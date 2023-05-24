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

struct handle_tcp {
  struct handle_stream s;
  int close_reset_cb_ref;
};

struct request_connect {
  uv_connect_t req;
  lua_State* L;

  int connect_cb_ref;
};

static struct handle_tcp * luaApi_check_tcp(lua_State* L, int index) {
  return (struct handle_tcp *)LuaApi_must_object_type(L, index, TCP_STREAM_LUA_OBJECT_TYPE, "Must tcp stream object");
}

/* Address families map */
static const struct {
  const char *afStr;
  int afVal;
} tcpAfMap[] = {
#ifdef AF_UNIX
  {"unix", AF_UNIX},
#endif
#ifdef AF_INET
  {"inet", AF_INET},
#endif
#ifdef AF_INET6
  {"inet6", AF_INET6},
#endif
#ifdef AF_IPX
  {"ipx", AF_IPX},
#endif
#ifdef AF_NETLINK
  {"netlink", AF_NETLINK},
#endif
  {NULL, 0}
};

static int getTcpAddressFamilyByStr(const char *afStr) {
  int i;
  for (i = 0; i < sizeof(tcpAfMap) / sizeof(tcpAfMap[0]); i++) {
    if (strcmp(tcpAfMap[i].afStr, afStr) == 0) {
      return tcpAfMap[i].afVal;
    }
  }
  return 0;
}

static const char * getTcpAddressFamilyByVal(int afVal) {
  int i;
  for (i = 0; i < sizeof(tcpAfMap) / sizeof(tcpAfMap[0]); i++) {
    if (tcpAfMap[i].afVal == afVal) {
      return tcpAfMap[i].afStr;
    }
  }
  return NULL;
}

static int luaApi_stream_tcp_create(lua_State* L) {
  struct handle_tcp *handle;
  struct loop *loop = LuaApi_loop_get_context(L);
  int ret;

  lua_settop(L, 1);
  handle = (struct handle_tcp *)lua_newuserdata(L, sizeof(struct handle_tcp));
  memset(handle, 0, sizeof(struct handle_tcp));
  handle->s.L = L;

  /* 记录自身的索引，在回调函数中使用 */
  lua_pushvalue(L, -1);
  handle->s.ref_self = luaL_ref(L, LUA_REGISTRYINDEX);

  if (lua_isnoneornil(L, 1)) {
    log_trace("Create TCP[%p].", handle);
    ret = uv_tcp_init(&loop->loop, &handle->s.stream.tcp);
  } else {
    unsigned int flags = AF_UNSPEC;
    if (lua_isnumber(L, 1)) {
      flags = lua_tointeger(L, 1);
    } else if (lua_isstring(L, 1)) {
      flags = getTcpAddressFamilyByStr(lua_tostring(L, 1));
    } else {
      return luaL_argerror(L, 1, "Need string or number");
    }

    log_trace("Create TCP[%p] with flags %d.", handle, flags);
    ret = uv_tcp_init_ex(&loop->loop, &handle->s.stream.tcp, flags);
  }

  if (ret < 0) {
    return luaL_error(L, "uv_tcp_init: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }

  /*为userdata 绑定相应的metatable类型 */
  luaL_setmetatable(L, TCP_STREAM_LUA_OBJECT_TYPE); // 将元表压栈 +1
  return 1;
}

static int luaApi_stream_tcp_open(lua_State* L) {
  struct handle_tcp *handle = luaApi_check_tcp(L, 1);
  uv_os_sock_t sock = luaL_checkinteger(L, 2);

  log_trace("TCP[%p] open with sock %d.", handle, sock);
  int ret = uv_tcp_open(&handle->s.stream.tcp, sock);
  if (ret < 0) {
    return luaL_error(L, "uv_tcp_open: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }
  return 0;
}

static int luaApi_stream_tcp_nodelay(lua_State* L) {
  struct handle_tcp *handle = luaApi_check_tcp(L, 1);
  int ret, enable;
  luaL_checktype(L, 2, LUA_TBOOLEAN);
  enable = lua_toboolean(L, 2);
  ret = uv_tcp_nodelay(&handle->s.stream.tcp, enable);

  if (ret < 0) {
    return luaL_error(L, "uv_tcp_nodelay: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }
  return 0;

}

static int luaApi_stream_tcp_keepalive(lua_State* L) {
  struct handle_tcp *handle = luaApi_check_tcp(L, 1);
  int ret, enable;
  unsigned int delay = 0;
  luaL_checktype(L, 2, LUA_TBOOLEAN);
  enable = lua_toboolean(L, 2);
  if (enable) {
    delay = luaL_checkinteger(L, 3);
  }
  ret = uv_tcp_keepalive(&handle->s.stream.tcp, enable, delay);
  if (ret < 0) {
    return luaL_error(L, "uv_tcp_keepalive: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }
  return 0;
}

static int luaApi_stream_tcp_simultaneous_accepts(lua_State* L) {
  struct handle_tcp *handle = luaApi_check_tcp(L, 1);
  int ret, enable;
  luaL_checktype(L, 2, LUA_TBOOLEAN);
  enable = lua_toboolean(L, 2);
  ret = uv_tcp_simultaneous_accepts(&handle->s.stream.tcp, enable);
  if (ret < 0) {
    return luaL_error(L, "uv_tcp_simultaneous_accepts: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }
  return 0;
}

static int luaApi_stream_tcp_bind(lua_State* L) {
  struct handle_tcp *handle = luaApi_check_tcp(L, 1);
  const char* host = luaL_checkstring(L, 2);
  int port = luaL_checkinteger(L, 3);
  unsigned int flags = 0;
  struct sockaddr_storage addr;
  int ret;

  log_trace("TCP[%p] bind to %s:%d.", handle, host, port);
  if (uv_ip4_addr(host, port, (struct sockaddr_in*)&addr) &&
      uv_ip6_addr(host, port, (struct sockaddr_in6*)&addr)) {
    return luaL_error(L, "Invalid IP address or port [%s:%d]", host, port);
  }

  if (lua_type(L, 4) == LUA_TTABLE) {
    lua_getfield(L, 4, "ipv6only");
    if (lua_toboolean(L, -1)) flags |= UV_TCP_IPV6ONLY;
    lua_pop(L, 1);
  }

  ret = uv_tcp_bind(&handle->s.stream.tcp, (struct sockaddr*)&addr, flags);
  if (ret < 0) {
    return luaL_error(L, "uv_tcp_bind: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }
  return 0;
}

/* 连接事件回调 */
static void connection_cb(uv_connect_t *req, int status) {
  struct request_connect *reqc = (struct request_connect *)req;
  lua_State* L = reqc->L;
  int nargs;

  if (status < 0) {
    lua_pushstring(L, uv_err_name(status));
    nargs = 1;
  } else {
    lua_pushnil(L);
    nargs = 0;
  }

  if (reqc->connect_cb_ref == LUA_NOREF){
    goto _exit;
  }

  log_trace("connection, call cb.");
  LuaApi_do_callback(L, reqc->connect_cb_ref, nargs);

_exit:
  /* 释放资源 */
  luaL_unref(L, LUA_REGISTRYINDEX, reqc->connect_cb_ref);
  free(reqc);
}

static int luaApi_stream_tcp_connect(lua_State* L) {
  struct handle_tcp *handle = luaApi_check_tcp(L, 1);
  const char* host = luaL_checkstring(L, 2);
  int port = luaL_checkinteger(L, 3);
  struct sockaddr_storage addr;

  struct request_connect *reqc;
  int ret, ref;

  if (uv_ip4_addr(host, port, (struct sockaddr_in*)&addr) &&
      uv_ip6_addr(host, port, (struct sockaddr_in6*)&addr)) {
    return luaL_error(L, "Invalid IP address or port [%s:%d]", host, port);
  }

  /* 处理connect callback */
  if (lua_isnoneornil(L, 4)) {
    ref = LUA_NOREF;
  } else {
    luaL_argcheck(L, LuaApi_check_callable(L, 4), 4, "Must be an callable object");
    lua_pushvalue(L, 4);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }

  reqc = (struct request_connect *)malloc(sizeof(struct request_connect));
  if (reqc == NULL) {
    return luaL_error(L, "Create connect request failed!");
  }

  memset(reqc, 0, sizeof(struct request_connect));
  reqc->L = L;

  ret = uv_tcp_connect(&reqc->req, &handle->s.stream.tcp, (struct sockaddr*)&addr, connection_cb);
  if (ret < 0) {
    return luaL_error(L, "uv_tcp_connect: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }
  return 0;
}

static void close_reset_cb(uv_handle_t* handle) {
  assert(handle != NULL);
  struct handle_tcp * data = (struct handle_tcp *)handle;
  lua_State* L = data->s.L;
  assert(L != NULL);

  if (data->close_reset_cb_ref == LUA_NOREF){
    goto _exit;
  }

  log_trace("close reset, call cb.");
  LuaApi_do_callback(L, data->close_reset_cb_ref, 0);

_exit:
  /* unref callback */
  luaL_unref(L, LUA_REGISTRYINDEX, data->close_reset_cb_ref);
  /* unref stream self */
  luaL_unref(L, LUA_REGISTRYINDEX, data->s.ref_self);
}

static int luaApi_stream_tcp_close_reset(lua_State* L) {
  int ret, ref;
  struct handle_tcp *handle = luaApi_check_tcp(L, 1);

  /* 处理callback */
  if (lua_isnoneornil(L, 2)) {
    ref = LUA_NOREF;
  } else {
    luaL_argcheck(L, LuaApi_check_callable(L, 2), 2, "Must be an callable object");
    lua_pushvalue(L, 2);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  handle->close_reset_cb_ref = ref;

  ret = uv_tcp_close_reset(&handle->s.stream.tcp, close_reset_cb);

  if (ret < 0) {
    return luaL_error(L, "uv_tcp_close_reset: %s: %s", uv_err_name(ret), uv_strerror(ret));
  }
  return 0;
}

static int luaApi_stream_tcp_gc(lua_State *L) {
  log_trace("[GC] Stream TCP");
  return 0;
}

// 模块静态函数
static const luaL_Reg lib_stream_tcp_f[] = {
  {"Create", luaApi_stream_tcp_create},
  {NULL, NULL}
};

// 模块的面向对象方法
static const luaL_Reg lib_stream_tcp_oo[] = {
    {"Open", luaApi_stream_tcp_open},
    {"Bind", luaApi_stream_tcp_bind},
    {"NoDelay", luaApi_stream_tcp_nodelay},
    {"KeepAlive", luaApi_stream_tcp_keepalive},
    {"SimultaneousAccepts", luaApi_stream_tcp_open},
    {"Connect", luaApi_stream_tcp_connect},
    {"Close", luaApi_stream_tcp_close_reset},
    {NULL, NULL}};

static int luaopen_stream_tcp(lua_State *L) {
  LuaApi_create_new_type(L, TCP_STREAM_LUA_OBJECT_TYPE, luaApi_stream_tcp_gc, lib_stream_tcp_oo, STREAM_LUA_OBJECT_TYPE);
  luaL_newlib(L, lib_stream_tcp_f);
  return 1;
}

// 注册接口调用
static int RegisterApi_StreamTcp(lua_State *L, void *opaque) {
  luaL_requiref(L, "Loop.tcp", luaopen_stream_tcp, 0);
  lua_pop(L, 1);

  return 0;
}
COMPONENT_INIT(EVENT_LOOP_STREAM_TCP, RegisterApi_StreamTcp, NULL, COM_LOOP, 3);
