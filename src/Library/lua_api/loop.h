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

#ifndef SRC_API_LOOP_H_
#define SRC_API_LOOP_H_

#include "lua.h"
#include "uv.h"

#define STREAM_LUA_OBJECT_TYPE "stream"
#define TCP_STREAM_LUA_OBJECT_TYPE "stream.tcp"
#define TIMER_LUA_OBJECT_TYPE "timer"

/* Loop 对象，事件驱动 */
struct loop {
  uv_loop_t loop; /* main loop */
};

/* libuv Stream 对象 */
struct handle_stream {
  /* uv_stream_t is an abstract type, libuv provides 3
   * stream implementations in the form of uv_tcp_t,
   * uv_pipe_t and uv_tty_t.
   */
  union {
    uv_stream_t base;
    uv_tcp_t tcp;
    uv_pipe_t pipe;
    uv_tty_t tty;
  } stream;

  lua_State* L;
  union {
    int read_cb;
    int connect_cb;
  } ref_cb;

  int ref_self;
};

/* 获得当前Lua状态机绑定的loop上下文 */
struct loop * LuaApi_loop_get_context(lua_State* L);

#endif
