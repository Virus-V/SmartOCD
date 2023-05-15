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

struct request_write {
  uv_write_t req;
  lua_State* L;

  int write_done_cb_ref;
  /* lua 数据的引用 */
  int data_ref;
};

struct request_shutdown {
  uv_shutdown_t req;
  lua_State* L;

  int shutdown_cb_ref;
};

static struct handle_stream * luaApi_check_stream(lua_State* L, int index) {
  return (struct handle_stream *)LuaApi_must_object_type(L, index, STREAM_LUA_OBJECT_TYPE, "Must stream object");
}

/* 连接事件回调 */
static void connection_cb(uv_stream_t *server, int status) {
  struct handle_stream * handle = (struct handle_stream *)server;
  lua_State* L = handle->L;
  int nargs;

  if (status < 0) {
    lua_pushstring(L, uv_err_name(status));
    nargs = 1;
  } else {
    nargs = 0;
  }

  /* 压入stream userdata自身 */
  lua_rawgeti(L, LUA_REGISTRYINDEX, handle->ref_self);
  assert(lua_isuserdata(L, -1) && lua_touserdata(L, -1) == handle);

  /* 将栈顶的函数调整到函数参数前 */
  if (nargs) {
    lua_insert(L, -1 - nargs);
  }

  log_debug("connect cb ref: %d", handle->ref_cb.connect_cb);
  LuaApi_do_callback(L, handle->ref_cb.connect_cb, nargs + 1);
}

static int luaApi_stream_listen(lua_State* L) {
  struct handle_stream *handle = luaApi_check_stream(L, 1);
  int backlog = luaL_checkinteger(L, 2);
  int ret;

  /* 检查第三个参数是否可以被调用 */
  luaL_argcheck(L, LuaApi_check_callable(L, 3), 3, "Must be an callable object");
  handle->L = L;

  lua_pushvalue(L, 3);
  handle->ref_cb.connect_cb = luaL_ref(L, LUA_REGISTRYINDEX);
  log_debug("connect cb ref: %d", handle->ref_cb.connect_cb);

  ret = uv_listen(&handle->stream.base, backlog, connection_cb);

  if (ret < 0) {
    return luaL_error(L, "uv_listen: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }

  lua_pushboolean(L, ret);
  return 1;
}

static int luaApi_stream_accept(lua_State* L) {
  struct handle_stream *server = luaApi_check_stream(L, 1);
  struct handle_stream *client = luaApi_check_stream(L, 2);

  int ret = uv_accept(&server->stream.base, &client->stream.base);

  if (ret < 0) {
    return luaL_error(L, "uv_accept: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }

  log_trace("accept %d", ret);
  lua_pushboolean(L, ret);
  return 1;
}

static void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  (void)handle;
  buf->base = (char*)malloc(suggested_size);
  assert(buf->base);
  buf->len = suggested_size;
}

static void read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  struct handle_stream * handle = (struct handle_stream *)stream;
  lua_State* L = handle->L;
  int nargs;

  if (nread > 0) {
    lua_pushnil(L);
    lua_pushlstring(L, buf->base, nread);
    nargs = 2;
  }

  free(buf->base);
  if (nread == 0){
    return;
  }

  if (nread == UV_EOF) {
    nargs = 0;
  } else if (nread < 0) {
    lua_pushstring(L, uv_err_name(nread));
    nargs = 1;
  }

  log_trace("read, call cb.");
  LuaApi_do_callback(L, handle->ref_cb.read_cb, nargs);
}

static int luaApi_stream_read_start(lua_State* L) {
  int ret;
  struct handle_stream *handle = luaApi_check_stream(L, 1);

  luaL_argcheck(L, LuaApi_check_callable(L, 2), 2, "Must be an callable object");

  /* 记录read 回调函数 */
  luaL_unref(L, LUA_REGISTRYINDEX, handle->ref_cb.read_cb);
  lua_pushvalue(L, 2);
  handle->ref_cb.read_cb = luaL_ref(L, LUA_REGISTRYINDEX);
  handle->L = L;

  ret = uv_read_start(&handle->stream.base, alloc_cb, read_cb);
  if (ret < 0) {
    return luaL_error(L, "uv_read_start: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }

  return 0;
}

static int luaApi_stream_read_stop(lua_State* L) {
  struct handle_stream *handle = luaApi_check_stream(L, 1);
  int ret = uv_read_stop(&handle->stream.base);

  if (ret < 0) {
    return luaL_error(L, "uv_read_stop: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }

  return 0;
}

static void write_done_cb(uv_write_t* req, int status) {
  struct request_write * reqw = (struct request_write *)req;
  lua_State* L = reqw->L;
  int nargs;

  if (status < 0) {
    lua_pushstring(L, uv_err_name(status));
    nargs = 1;
  } else {
    lua_pushnil(L);
    nargs = 0;
  }

  if (reqw->write_done_cb_ref == LUA_NOREF) {
    lua_pop(L, 1);
    goto _exit;
  } else {
    log_trace("write done, call cb.");
    LuaApi_do_callback(L, reqw->write_done_cb_ref, nargs);
  }

_exit:
  luaL_unref(L, LUA_REGISTRYINDEX, reqw->write_done_cb_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, reqw->data_ref);

  free(reqw);
}

static int luaApi_stream_write(lua_State* L) {
  struct handle_stream *handle = luaApi_check_stream(L, 1);
  struct request_write * reqw;
  int ret, ref;

  /* 准备buffer */
  if (!lua_isstring(L, 2)) {
    return luaL_argerror(L, 2, lua_pushfstring(L, "data must be string, got %s", luaL_typename(L, 2)));
  }

  /* 处理write done callback */
  if (lua_isnoneornil(L, 3)) {
    ref = LUA_NOREF;
  } else {
    luaL_argcheck(L, LuaApi_check_callable(L, 3), 3, "Must be an callable object");
    lua_pushvalue(L, 3);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  reqw = (struct request_write *)malloc(sizeof(struct request_write));
  if (reqw == NULL) {
    return luaL_error(L, "Create write request failed!");
  }

  memset(reqw, 0, sizeof(struct request_write));
  reqw->L = L;
  reqw->write_done_cb_ref = ref;

  uv_buf_t *buf = (uv_buf_t*)malloc(sizeof(uv_buf_t));
  buf->base = (char*)lua_tolstring(L, 2, &buf->len);
  lua_pushvalue(L, 2);
  reqw->data_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  ret = uv_write(&reqw->req, &handle->stream.base, buf, 1, write_done_cb);

  free(buf);

  if (ret < 0) {
    return luaL_error(L, "uv_write: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }
  return 0;
}

static void shutdown_cb(uv_shutdown_t* req, int status) {
  struct request_shutdown * reqs = (struct request_shutdown *)req;
  lua_State* L = reqs->L;
  int nargs;

  assert(L != NULL);

  log_debug("shutdown cb called!!");
  if (status < 0) {
    lua_pushstring(L, uv_err_name(status));
    nargs = 1;
  } else {
    nargs = 0;
  }

  if (reqs->shutdown_cb_ref == LUA_NOREF) {
    lua_pop(L, 1);
    goto _exit;
  } else {
    log_trace("shutdown, call cb.");
    LuaApi_do_callback(L, reqs->shutdown_cb_ref, nargs);
  }

_exit:
  luaL_unref(L, LUA_REGISTRYINDEX, reqs->shutdown_cb_ref);
  free(reqs);
}

static int luaApi_stream_shutdown(lua_State* L) {
  struct handle_stream *handle = luaApi_check_stream(L, 1);
  struct request_shutdown *reqs;
  int ret, ref;

  log_trace("Stream[%p] shutdown.", handle);
  /* 处理shutdown callback */
  if (lua_isnoneornil(L, 2)) {
    ref = LUA_NOREF;
  } else {
    luaL_argcheck(L, LuaApi_check_callable(L, 2), 2, "Must be an callable object");
    lua_pushvalue(L, 2);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  reqs = (struct request_shutdown *)malloc(sizeof(struct request_shutdown));
  if (reqs == NULL) {
    return luaL_error(L, "Create shutdown request failed!");
  }

  memset(reqs, 0, sizeof(struct request_shutdown));
  reqs->L = L;
  reqs->shutdown_cb_ref = ref;

  ret = uv_shutdown(&reqs->req, &handle->stream.base, shutdown_cb);

  if (ret < 0) {
    return luaL_error(L, "uv_shutdown: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }

  return 0;
}

static int luaApi_stream_gc(lua_State *L) {
  log_trace("[GC] Stream");
  return 0;
}

// 模块的面向对象方法
static const luaL_Reg lib_stream_oo[] = {
    {"Listen", luaApi_stream_listen},
    {"Accept", luaApi_stream_accept},
    {"ReadStart", luaApi_stream_read_start},
    {"ReadStop", luaApi_stream_read_stop},
    {"Write", luaApi_stream_write},
    {"Shutdown", luaApi_stream_shutdown},
    {NULL, NULL}};

// 注册接口调用
static int RegisterApi_LoopStream(lua_State *L, void *opaque) {
  /* 注册stream类型，它只被继承，如TCP、UDP、PIPE等 */
  LuaApi_create_new_type(L, STREAM_LUA_OBJECT_TYPE, luaApi_stream_gc, lib_stream_oo, NULL);
  return 0;
}

COMPONENT_INIT(EVENT_LOOP_STREAM, RegisterApi_LoopStream, NULL, COM_LOOP, 2);
