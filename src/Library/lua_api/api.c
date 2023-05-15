/**
 * Copyright (c) 2023, Virus.V <virusv@live.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of SmartOCD nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * Copyright 2023 Virus.V <virusv@live.com>
 */

#include <string.h>
#include <pthread.h>

#include "api.h"
#include "smartocd.h"

#include "Component/component.h"
#include "Library/log/log.h"

/**
 * 往栈顶的table中注册常量
 */
void LuaApi_reg_constant(lua_State *L, const luaApi_regConst *c) {
  assert(L != NULL);
  while (c->name != NULL) {
    // TODO distinguish constant type
    lua_pushinteger(L, c->value);
    // register constant to the table
    lua_setfield(L, -2, c->name);
    c++;
  }
}

/**
 * 创建类型元表
 * tname：类型名称
 * gc：该类型被垃圾回收时执行的自定义释放函数
 * oo：该类型对应的方法
 * tparent：该类型的父对象类型
 */
void LuaApi_create_new_type(lua_State *L, const char *tname, lua_CFunction gc, const luaL_Reg *oo, const char *tparent) {
  assert(L != NULL);
  assert(tname != NULL);
  assert(oo != NULL);

  int tparent_idx, stack_top;

  stack_top = lua_gettop(L);

  if (tparent != NULL){
    // 获得tparent对应的metatable
    if (LUA_TNIL == luaL_getmetatable(L, tparent)){ // +1
      luaL_error(L, "Unknow object type %s.\n", tparent);
      lua_settop(L, stack_top);
      return;
    }
    tparent_idx = lua_absindex(L, -1);
  }

  if (luaL_newmetatable(L, tname) == 0) { // +1
    log_warn("Type %s already registed.\n", tname);
    lua_settop(L, stack_top);
    return;
  }

  // 创建method表，并设置到元表的__index字段
  lua_newtable(L); // +1

  lua_pushvalue(L, -1); // +1
  lua_setfield(L, -3, "__index"); // -1

  if (gc != NULL){
    lua_pushcfunction(L, gc); // +1
    lua_setfield(L, -3, "__gc"); // -1
  }

  // 注册method到method表
  luaL_setfuncs(L, oo, 0);

  if (tparent != NULL){
    // 将tparent类型元表压栈
    lua_pushvalue(L, tparent_idx);  // +1
    lua_setmetatable(L, -2);  // -1
  }

  lua_settop(L, stack_top);
}

/**
 * 检查是否是指定类型的对象
 * ud: userdata index
 * type: 类型字符串
 */
void *LuaApi_check_object_type(lua_State *L, int ud, const char *type) {
  void *p = lua_touserdata(L, ud);
  if (p != NULL) {
    if (lua_getmetatable(L, ud)) {
      //获得metatable的__name字段
      if (lua_getfield(L, -1, "__name") != LUA_TSTRING) {
        lua_pop(L, 2);
        return NULL;
      }

      const char *typeName = lua_tostring(L, -1); // 获得类型字符串
      if (strncmp(typeName, type, strlen(type)) != 0) {
        lua_pop(L, 2);
        return NULL;
      }

      /* 获得typename对应的meta table */
      luaL_getmetatable(L, typeName);
      if (!lua_rawequal(L, -1, -3)) {
        p = NULL;
      }

      lua_pop(L, 3);
      return p;
    }
  }
  return NULL;
}

/**
 * 检查是否是指定类型的对象，否则抛出异常
 * ud: userdata index
 * type: 类型字符串
 * err_msg: 当类型不一致时，抛出的异常消息
 */
void *LuaApi_must_object_type(lua_State *L, int ud, const char *type, const char *err_msg) {
  void *udata = LuaApi_check_object_type(L, ud, type);
  if (udata == NULL) {
    /* luaL_error NEVER return */
    luaL_error(L, "Check type: %s", err_msg);
    return NULL;
  }

  return udata;
}

/**
 * 检查指定参数是否允许被调用
 * index: 参数索引
 */
int LuaApi_check_callable(lua_State* L, int index) {
  assert(L != NULL);

  if (luaL_getmetafield(L, index, "__call") != LUA_TNIL) {
    int callable = lua_isfunction(L, -1);
    lua_pop(L, 1);
    return callable;
  }

  return lua_isfunction(L, index);
}

/* 打印调用栈 */
static int luaApi_traceback (lua_State *L) {
  log_debug("stack backtrace");
#if 0
  if (!lua_isstring(L, 1)) {  /* 'message' not a string? */
    return 1;  /* keep it intact */
  }
#endif

  lua_pushglobaltable(L);
  lua_getfield(L, -1, "debug");
  lua_remove(L, -2);

  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return 1;
  }

  lua_getfield(L, -1, "traceback");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    return 1;
  }

  lua_pushvalue(L, 1);  /* pass error message */
  lua_pushinteger(L, 2);  /* skip this function and traceback */
  lua_call(L, 2, 1);  /* call debug.traceback */
  return 1;
}

/* 调用Lua函数 */
int LuaApi_cfpcall(lua_State* L, int nargs) {
  int ret, errfunc, stack_size;
  int nresult;

  assert(L != NULL);
  assert(nargs >= 0);

  /* 检查函数位置是否可以被调用 */
  if (!LuaApi_check_callable(L, -1 - nargs)) {
    return luaL_error(L, "Try call a not callable object.");
  }

  /* 压栈错误处理函数 */
  lua_pushcfunction(L, luaApi_traceback);

  errfunc = lua_gettop(L);
  /* 获得Lua栈在调用函数前，栈上已有的个数，计算返回值的时候使用 */
  stack_size = errfunc - nargs - 2;

  /* 把错误处理函数放在被调函数和参数前 */
  lua_insert(L, -2 - nargs);

  /* 错误处理函数在栈上的索引 */
  errfunc -= nargs + 1;

  lua_call(L, nargs, LUA_MULTRET);
#if 0
  ret = lua_pcall(L, nargs, LUA_MULTRET, errfunc);
  log_trace("lua_pcall ret: %d", ret);
  switch (ret) {
  case LUA_OK:
    break;

  case LUA_ERRMEM:
    log_fatal("System Error: %s\n", lua_tostring(L, -1));
    abort();
    break;

  case LUA_ERRRUN:
  case LUA_ERRERR:
  default:
    lua_pop(L, 1);
    ret = -ret;
    break;
  }
#endif

  /* 平衡栈 */
  lua_remove(L, errfunc);

  if (ret == LUA_OK) {
    nresult = lua_gettop(L) - stack_size;
    log_info("Lua function call success, nresult = %d", nresult);
    return nresult;
  }

  log_warn("Lua function call failed, ret = %d", ret);
  return ret;
}

/*
 * 执行Lua函数，返回值为Lua函数返回值个数，调用该函数前，应先将Lua函数的参数
 * 压栈。
 * 参数:
 *  ref: Lua函数对象在注册表上的reference，或者LUA_NOREF
 */
int LuaApi_do_callback(lua_State* L, int ref, int nargs) {
  assert(L != NULL);
  int ret;

  log_trace("Call lua function %d with %d args, stack size: %d", ref, nargs, lua_gettop(L));

  if (ref == LUA_NOREF) {
    log_trace("Not vaild function ref.");
    lua_pop(L, nargs);
    return 0;
  }

  luaL_checkstack(L, nargs + 1, "Could not expand stack.");

  /* 获得函数对象，可能是nil，需要处理此种情况 */
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

  /* 将栈顶的函数调整到函数参数前 */
  if (nargs) {
    lua_insert(L, -1 - nargs);
  }

  ret = LuaApi_cfpcall(L, nargs);
  /* FIXME */
  if (ret < 0) {
    log_fatal("ret < 0!");
  }
}

// Lua 虚拟机锁
static pthread_mutex_t vmlock = PTHREAD_MUTEX_INITIALIZER;
void core_LockVM(lua_State *L) {
  (void)L;
  pthread_mutex_lock(&vmlock);
}

void core_UnlockVM(lua_State *L) {
  (void)L;
  pthread_mutex_unlock(&vmlock);
}

//COMPONENT_INIT(FTDI, RegisterApi_Ftdi, NULL, COM_ADAPTER, 2);
