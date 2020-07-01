/**
 * src/smartocd.c
 * Copyright (c) 2020 Virus.V <virusv@live.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "smartocd.h"

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // usleep

#include "Component/component.h"
#include "Library/linenoise/linenoise.h"
#include "Library/log/log.h"
#include "Library/lua/src/lauxlib.h"
#include "Library/lua/src/lua.h"
#include "Library/lua/src/lualib.h"

static lua_State *globalL = NULL;
static FILE *logFd = NULL;

static const struct option long_option[] = {
    {"logfile", required_argument, NULL, 'l'}, {"help", no_argument, NULL, 'h'}, {"file", required_argument, NULL, 'f'}, {"debuglevel", required_argument, NULL, 'd'}, {"exit", no_argument, NULL, 'e'}, {NULL, 0, NULL, 0}};

// 打印帮助信息
static void printHelp(char *progName) {
  printf(
      "Usage: smartocd [options]\n\nOptions:\n"
      "\t-d level, --debuglevel level : Debug information output level; 0-6, 0 "
      "not output.\n"
      "\t-f script, --file script : Pre-executed script, this parameter can be "
      "more than one.\n"
      "\t-e, --exit : End of the script does not enter the interactive mode.\n"
      "\t-l, --logfile : Log file.\n"
      "\t-h, --help : Show this help message.\n\n"
      "For more information, visit: https://github.com/Virus-V/SmartOCD/\n");
}

// TODO 当用户按tab键时自动补全
static void completion(const char *buf, linenoiseCompletions *lc) {
  if (buf[0] == 'h') {
    linenoiseAddCompletion(lc, "hello");
    linenoiseAddCompletion(lc, "hello there");
  }
}

// TODO 代码提示
static char *hints(const char *buf, int *color, int *bold) {
  if (!strcasecmp(buf, "new_cmsis_dap")) {
    *color = 36;
    *bold = 1;
    return " -- Create New CMSIS-DAP Object";
  }
  return NULL;
}

static void lstop(lua_State *L, lua_Debug *ar) {
  (void)ar;                   /* unused arg. */
  lua_sethook(L, NULL, 0, 0); /* reset hook */
  luaL_error(L, "interrupted!");
}

static void laction(int i) {
  signal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
  lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

// 检查lua脚本执行结果
static int report(lua_State *L, int status) {
  if (status != LUA_OK) {
    const char *msg = lua_tostring(L, -1);
    //log_error("%s", msg);
    fprintf(stderr, "Error: %s\n", msg);
    lua_pop(L, 1); /* remove message */
  }
  return status;
}

/*
** Message handler used to run all chunks
*/
static int msghandler(lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  if (msg == NULL) {                         /* is error object not a string? */
    if (luaL_callmeta(L, 1, "__tostring") && /* does it have a metamethod */
        lua_type(L, -1) == LUA_TSTRING)      /* that produces a string? */
      return 1;                              /* that is the message */
    else
      msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
  }
  luaL_traceback(L, L, msg, 1); /* append a standard traceback */
  return 1;                     /* return the traceback */
}

/**
 * 执行脚本
 * script：脚本路径
 */
static int handleScript(lua_State *L, char *script) {
  int status;
  // XXX luaL_loadfile这个函数使用script时是否会其进行拷贝？
  status = luaL_loadfile(L, script);
  if (status == LUA_OK) {
    // 错误处理handle
    lua_pushcfunction(L, msghandler);
    lua_insert(L, -2);
    globalL = L;                               /* to be available to 'laction' */
    signal(SIGINT, laction);                   /* set C-signal handler */
    status = lua_pcall(L, 0, LUA_MULTRET, -2); // -2 错误处理函数
    signal(SIGINT, SIG_DFL);                   /* reset C-signal handler */
    lua_remove(L, -2);                         /* 移除错误处理函数 */
  }
  return report(L, status);
}

static int docall(lua_State *L, int narg, int nres) {
  int status;
  int base = lua_gettop(L) - narg;         /* function index */
  lua_pushcfunction(L, msghandler);        /* push message handler */
  lua_insert(L, base);                     /* put it under function and args */
  globalL = L;                             /* to be available to 'laction' */
  signal(SIGINT, laction);                 /* set C-signal handler */
  status = lua_pcall(L, narg, nres, base); // base is error handle function
  signal(SIGINT, SIG_DFL);                 /* reset C-signal handler */
  lua_remove(L, base);                     /* remove message handler from the stack */
  return status;
}

/* mark in error messages for incomplete statements */
#define EOFMARK "<eof>"
#define MARKLEN (sizeof(EOFMARK) / sizeof(char) - 1)

/**
 * 判断用户输入语法是否完整
 */
static int incomplete(lua_State *L, int status) {
  if (status == LUA_ERRSYNTAX) {
    size_t lmsg;
    const char *msg = lua_tolstring(L, -1, &lmsg);
    if (lmsg >= MARKLEN && strcmp(msg + lmsg - MARKLEN, EOFMARK) == 0) {
      lua_pop(L, 1);
      return 1;
    }
  }
  return 0; /* else... */
}

/*
** Prompt the user, read a line, and push it into the Lua stack.
*/
static int pushline(lua_State *L, int firstline) {
  char *line;
  size_t l;
  //获得行首提示符
  const char *prmt = firstline ? "SmartOCD> " : "SmartOCD>> ";
  int readstatus = (line = linenoise(prmt), line != NULL);
  // 没有输入，Ctrl-d结束
  if (readstatus == 0)
    return 0;

  l = strlen(line);
  if (l > 0 && line[l - 1] == '\n') /* line ends with newline? */
    line[--l] = '\0';               /* remove it */

  if (firstline && line[0] == '=') {           /* for compatibility with 5.2, ... */
    lua_pushfstring(L, "return %s", line + 1); /* change '=' to 'return' */
  } else
    lua_pushlstring(L, line, l);
  free(line);
  return 1;
}

/*
** Try to compile line on the stack as 'return <line>;'; on return, stack
** has either compiled chunk or original line (if compilation failed).
*/
static int addreturn(lua_State *L) {
  const char *line = lua_tostring(L, -1); /* original line */
  const char *retline = lua_pushfstring(L, "return %s;", line);
  // 编译一段代码，但不运行
  int status = luaL_loadbuffer(L, retline, strlen(retline), "=stdin");
  if (status == LUA_OK) {  // 如果编译成功
    lua_remove(L, -2);     /* remove modified line */
    if (line[0] != '\0') { /* non empty? */
      // 加入历史记录并保存
      linenoiseHistoryAdd(line);
      linenoiseHistorySave(COMMAND_HISTORY);
    }
  } else
    lua_pop(L, 2); /* pop result from 'luaL_loadbuffer' and modified line */
  return status;
}

/*
** Read multiple lines until a complete Lua statement
*/
static int multiline(lua_State *L) {
  while (1) { /* repeat until gets a complete statement */
    size_t len;
    const char *line = lua_tolstring(L, 1, &len);         /* get what it has */
    int status = luaL_loadbuffer(L, line, len, "=stdin"); /* try it */
    // 如果输入完成了或者没有输入，则结束，并返回编译状态
    if (!incomplete(L, status) || !pushline(L, 0)) {
      // 加入历史记录并保存
      linenoiseHistoryAdd(line);
      linenoiseHistorySave(COMMAND_HISTORY);
      return status; /* cannot or should not try to add continuation line */
    }
    lua_pushliteral(L, " "); /* add newline...压栈 */
    lua_insert(L, -2);       /* ...between the two lines */
    lua_concat(L, 3);        /* join them */
  }
}

/*
** Read a line and try to load (compile) it first as an expression (by
** adding "return " in front of it) and second as a statement. Return
** the final status of load/call with the resulting function (if any)
** in the top of the stack.
*/
static int loadline(lua_State *L) {
  int status;
  lua_settop(L, 0);                      // 清空栈中的所有元素
  if (!pushline(L, 1))                   // 读取一行压入栈中
    return -1;                           /* no input */
  if ((status = addreturn(L)) != LUA_OK) /* 'return ...' did not work? */
    status = multiline(L);               /* try as command, maybe with continuation lines */
  lua_remove(L, 1);                      /* remove line from the stack */
  // 栈中只有一个元素是代码块
  lua_assert(lua_gettop(L) == 1);
  return status;
}

/*
** Prints (calling the Lua 'print' function) any values on the stack
*/
static void l_print(lua_State *L) {
  int n = lua_gettop(L);
  if (n > 0) { /* any result to be printed? */
    luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
    lua_getglobal(L, "print"); //
    lua_insert(L, 1);
    if (lua_pcall(L, n, 0, 0) != LUA_OK)
      log_error("%s", lua_pushfstring(L, "error calling 'print' (%s)", lua_tostring(L, -1)));
  }
}

/**
 * 注册全局函数
 */
// delay毫秒
static int global_fun_sleep(lua_State *L) {
  lua_Number delay = luaL_checknumber(L, 1); // ms
  // 转换成微秒
  delay *= 1000;
  lua_Integer delay_us = 0;
  if (!lua_numbertointeger(delay, &delay_us)) {
    return luaL_error(L, "Unable to convert milliseconds to microseconds, data is not legal?");
  }
  if (delay_us == 0) {
    return 0;
  }
  usleep(delay_us);
  return 0;
}

// 设置一些全局变量
static int setGlobal(lua_State *L) {
  lua_pushfstring(L, "%s", VERSION);
  lua_setglobal(L, "_SMARTOCD_VERSION"); // 版本信息
  lua_pushcfunction(L, global_fun_sleep);
  lua_setglobal(L, "sleep"); // 延时函数
  return LUA_OK;
}

/*
** Do the REPL: repeatedly read (load) a line, evaluate (call) it, and
** print any results.
*/
static void doREPL(lua_State *L) {
  int status;
  while ((status = loadline(L)) != -1) { // -1 是没有输入的时候
    if (status == LUA_OK)
      status = docall(L, 0, LUA_MULTRET); // 0个参数，多个返回值
    if (status == LUA_OK)
      l_print(L); // 打印返回值
    else
      report(L, status);
  }
  lua_settop(L, 0); /* clear stack */
  lua_writeline();
}

/**
 * 日志输出重定向
 * fileName：重定向文件路径
 */
static int setLogfile(const char *fileName) {
  logFd = fopen(fileName, "wb+");
  if (logFd == NULL) {
    log_error("Unable to create log file.");
    return 1;
  }
  log_set_fp(logFd);
  return 0;
}

/**
 * SmartOCD环境初始化
 * 预加载文件 -f --file
 * 执行完脚本文件之后不进入交互模式 -e --exit
 * 调试输出级别 -d --debuglevel
 * 日志重定向 -l --logfile
 * 帮助：-h --help
 */
static int smartocd_init(lua_State *L) {
  // 取回参数个数和参数数据
  int argc = (int)lua_tointeger(L, -2);
  char **argv = lua_touserdata(L, -1);
  int opt, logLevel = LOG_INFO;
  int exitFlag = 0; // 执行脚本后结束运行

  // 打开标准库
  luaL_openlibs(L);
  // 设置全局变量，SmartOCD版本信息
  if (setGlobal(L) != LUA_OK) {
    lua_pushboolean(L, 0);
    return 1;
  }

  // 默认日志等级
  log_set_level(logLevel);
  // 注册SmartOCD API接口
  component_init(L);
  
  // 解析参数
  while ((opt = getopt_long(argc, argv, "f:d:ehl:", long_option, NULL)) != -1) {
    switch (opt) {
    case 'f': // 执行脚本
      // log_debug("script name:%s", optarg);
      handleScript(L, optarg);
      break;
    case 'd': // 日志输出等级 -1 不输出任何日志， 转换失败则会返回0
      logLevel = atoi(optarg);
      if (logLevel > 0) {
        log_set_level(logLevel - 1);
      }
      break;
    case 'e': // 执行脚本后不进入交互模式，直接退出
      exitFlag = 1;
      break;
    case 'h': // help
      printHelp(argv[0]);
      lua_pushboolean(L, 1);
      return 1;
      break;
    case 'l': // logfile 日志重定向到文件
      // FIXME 这个日志文件无法及时关闭
      if (setLogfile(optarg)) {
        lua_pushboolean(L, 0);
        return 1;
      }
      break;
    default:; // 0 error: label at end of compound statement
    }
  }
  if (exitFlag)
    goto EXIT;
  // line noise初始化
  linenoiseSetMultiLine(1); // 多行编辑
  linenoiseSetCompletionCallback(completion);
  linenoiseSetHintsCallback(hints);
  // 加载历史输入
  linenoiseHistoryLoad(COMMAND_HISTORY);
  // 开始用户交互模式
  doREPL(L);
EXIT:
  lua_pushboolean(L, 1);
  return 1;
}

// 致命错误恢复点
jmp_buf fatalException;

// 打印logo和版本
static void printVersion() {
  char *logo[] = {"   _____                      __  ____  __________ ",
                  "  / ___/____ ___  ____ ______/ /_/ __ \\/ ____/ __ \\",
                  "  \\__ \\/ __ `__ \\/ __ `/ ___/ __/ / / / /   / / / /",
                  " ___/ / / / / / / /_/ / /  / /_/ /_/ / /___/ /_/ / ",
                  "/____/_/ /_/ /_/\\__,_/_/   \\__/\\____/\\____/_____/  ",
                  NULL};
  int i;
  for (i = 0; logo[i]; i++)
    printf("%s\n", logo[i]);

  printf(
      " * SmartOCD v%s By: Virus.V <virusv@live.com>\n"
      " * Complile time: %s\n"
      " * Github: https://github.com/Virus-V/SmartOCD\n\n",
      VERSION, COMPILE_TIME);
}

/**
 * SmartOCD Entry Point
 */
int main(int argc, char **argv) {
  int status, result;
  int opt, logLevel = LOG_INFO;

  // 设置初始日志级别
  log_set_level(LOG_INFO);
  switch (setjmp(fatalException)) {
  default:
    log_fatal("Fatal Error! Abort!");
    return 1;
  case 0:
    break;
  }
  // 打印logo和版本
  printVersion();

  // 创建lua状态机
  lua_State *L = luaL_newstate();
  if (L == NULL) {
    log_fatal("cannot create state: not enough memory.");
    return 1;
  }
  // 以保护模式运行初始化函数，这些函数不会抛出异常。
  lua_pushcfunction(L, &smartocd_init);
  // 将参数个数argc压栈
  lua_pushinteger(L, argc);
  // 参数指针argv压栈
  lua_pushlightuserdata(L, argv);
  // 以保护模式调用函数
  status = lua_pcall(L, 2, 1, 0);
  // 打如果错误则打印
  report(L, status);
  result = lua_toboolean(L, -1);
  lua_close(L);
  return (result && status == LUA_OK) ? 0 : 1;
}
