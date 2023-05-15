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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "uv.h"

#include "Component/component.h"
#include "Library/lua_api/loop.h"
#include "Library/lua_api/api.h"
#include "Library/log/log.h"
#include "smartocd.h"

static const char * const loop_key = "smartocd.loop";

struct loop * LuaApi_loop_get_context(lua_State* L) {
  struct loop * loop;

  lua_pushlightuserdata(L, (void *)loop_key);
  lua_rawget(L, LUA_REGISTRYINDEX);

  if (lua_isnil(L, -1)) {
    lua_pushlightuserdata(L, (void *)loop_key);
    loop = (struct loop *)lua_newuserdata(L, sizeof(struct loop));
    memset(loop, 0, sizeof(struct loop));
    uv_loop_init(&loop->loop);

    lua_rawset(L, LUA_REGISTRYINDEX);
  } else {
    loop = (struct loop *)lua_touserdata(L, -1);
  }

  lua_pop(L, 1);
  return loop;
}

/* 关闭loop */
static int loop_close(lua_State* L) {
#warning "TODO"
  assert(0);
}

// These are the same order as uv_run_mode which also starts at 0
static const char *const loop_runmodes[] = {
  "default", "once", "nowait", NULL
};

/* 运行Loop */
static int luaApi_loop_run(lua_State* L) {
  int mode = luaL_checkoption(L, 1, "default", loop_runmodes);
  struct loop *loop = LuaApi_loop_get_context(L);
  int ret = uv_run(&loop->loop, (uv_run_mode)mode);

  if (ret < 0) {
    return luaL_error(L, "%s: %s", uv_err_name(ret), uv_strerror(ret));
  }
  lua_pushboolean(L, ret);
  return 1;
}

static const char *const loop_configure_options[] = {
  "block_signal",
  "metrics_idle_time",
  NULL
};

static int luaApi_loop_configure(lua_State* L) {
  struct loop* loop = LuaApi_loop_get_context(L);
  uv_loop_option option = 0;
  int ret = 0;

  switch (luaL_checkoption(L, 1, NULL, loop_configure_options)) {
    case 0:
      option = UV_LOOP_BLOCK_SIGNAL;
      break;

    case 1:
      option = UV_METRICS_IDLE_TIME;
      break;

    default: break; /* unreachable */
  }

  if (option == UV_LOOP_BLOCK_SIGNAL) {
    int signal;

    luaL_checknumber(L, 2);
    signal = lua_tonumber(L, 2);;
    ret = uv_loop_configure(&loop->loop, UV_LOOP_BLOCK_SIGNAL, signal);
  } else {
    ret = uv_loop_configure(&loop->loop, option);
  }
  if (ret < 0) {
    return luaL_error(L, "%s: %s", uv_err_name(ret), uv_strerror(ret));
  }
  lua_pushinteger(L, ret);
  return 1;
}

// 模块静态函数
static const luaL_Reg lib_loop_f[] = {
  {"Run", luaApi_loop_run},
  {"Configure", luaApi_loop_configure},
  {NULL, NULL}
};

// 初始化loop
static int luaopen_event_loop(lua_State *L) {
  luaL_newlib(L, lib_loop_f);
  return 1;
}

// 注册接口调用
static int RegisterApi_EventLoop(lua_State *L, void *opaque) {
  luaL_requiref(L, "Loop", luaopen_event_loop, 0);
  lua_pop(L, 1);

  return 0;
}

COMPONENT_INIT(EVENT_LOOP, RegisterApi_EventLoop, NULL, COM_LOOP, 1);
