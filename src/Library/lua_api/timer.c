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

#include "Library/lua_api/loop.h"

#include "smartocd.h"

struct handle_timer {
  uv_timer_t timer;
  lua_State *L;
  int cb_ref;
  int self_ref;
};

static struct handle_timer * luaApi_check_timer(lua_State* L, int index) {
  return (struct handle_timer *)LuaApi_must_object_type(L, index, TIMER_LUA_OBJECT_TYPE, "Must timer object");
}

static int luaApi_timer_create(lua_State* L) {
  struct loop *loop = LuaApi_loop_get_context(L);
  struct handle_timer *handle = (struct handle_timer *)lua_newuserdata(L, sizeof(struct handle_timer));
  int ret = uv_timer_init(&loop->loop, &handle->timer);
  if (ret < 0) {
    lua_pop(L, 1);
    return luaL_error(L, "uv_timer_init: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }

  handle->cb_ref = LUA_NOREF;
  handle->self_ref = LUA_NOREF;
  handle->L = L;

  /*为userdata 绑定相应的metatable类型 */
  luaL_setmetatable(L, TIMER_LUA_OBJECT_TYPE);
  return 1;
}

static void timer_cb(uv_timer_t *handle) {
  struct handle_timer *data = (struct handle_timer *)handle;
  lua_State* L = data->L;

  if (data->cb_ref == LUA_NOREF) {
    return;
  }

  log_trace("timer, call cb.");
  LuaApi_do_callback(L, data->cb_ref, 0);

  // FIXME callback执行异常的时候，释放资源
  if(uv_timer_get_repeat(handle) == 0) {
    luaL_unref(L, LUA_REGISTRYINDEX, data->cb_ref);
    luaL_unref(L, LUA_REGISTRYINDEX, data->self_ref);
    data->cb_ref = LUA_NOREF;
    data->self_ref = LUA_NOREF;
  }
}

static int luaApi_timer_start(lua_State* L) {
  struct handle_timer *handle = luaApi_check_timer(L, 1);
  uint64_t timeout;
  uint64_t repeat;
  int ret;

  timeout = luaL_checkinteger(L, 2);
  repeat = luaL_checkinteger(L, 3);
  luaL_argcheck(L, LuaApi_check_callable(L, 4), 4, "Must be an callable object");

  lua_pushvalue(L, 4);
  handle->cb_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushvalue(L, 1);
  handle->self_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  ret = uv_timer_start(&handle->timer, timer_cb, timeout, repeat);
  if (ret < 0) {
    return luaL_error(L, "uv_timer_start: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }

  return 0;
}

static int luaApi_timer_stop(lua_State* L) {
  struct handle_timer *handle = luaApi_check_timer(L, 1);
  int ret = uv_timer_stop(&handle->timer);
  if (ret < 0) {
    return luaL_error(L, "uv_timer_stop: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }
  return 0;
}

static int luaApi_timer_again(lua_State* L) {
  struct handle_timer *handle = luaApi_check_timer(L, 1);
  int ret = uv_timer_again(&handle->timer);
  if (ret < 0) {
    return luaL_error(L, "uv_timer_stop: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }
  return 0;
}

static int luaApi_timer_repeat(lua_State* L) {
  struct handle_timer *handle = luaApi_check_timer(L, 1);

  if (lua_isnoneornil(L, 2)) {
    uint64_t repeat = uv_timer_get_repeat(&handle->timer);
    lua_pushinteger(L, repeat);
    return 1;
  } else {
    uint64_t repeat = luaL_checkinteger(L, 2);
    uv_timer_set_repeat(&handle->timer, repeat);
    return 0;
  }
}

static int luaApi_timer_gc(lua_State *L) {
  log_trace("[GC] Timer");
  return 0;
}

// 模块静态函数
static const luaL_Reg lib_timer_f[] = {
  {"Create", luaApi_timer_create},
  {NULL, NULL}
};

// 模块的面向对象方法
static const luaL_Reg lib_timer_oo[] = {
    {"Start", luaApi_timer_start},
    {"Stop", luaApi_timer_stop},
    {"Again", luaApi_timer_again},
    {"Repeat", luaApi_timer_repeat},
    {NULL, NULL}};

static int luaopen_timer(lua_State *L) {
  LuaApi_create_new_type(L, TIMER_LUA_OBJECT_TYPE, luaApi_timer_gc, lib_timer_oo, NULL);
  luaL_newlib(L, lib_timer_f);
  return 1;
}

// 注册接口调用
static int RegisterApi_LoopTimer(lua_State *L, void *opaque) {
  luaL_requiref(L, "Loop.timer", luaopen_timer, 0);
  lua_pop(L, 1);

  return 0;
}
COMPONENT_INIT(EVENT_LOOP_TIMER, RegisterApi_LoopTimer, NULL, COM_LOOP, 4);
