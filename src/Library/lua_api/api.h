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

#ifndef SRC_API_API_H_
#define SRC_API_API_H_

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "Library/lua/src/lauxlib.h"
#include "Library/lua/src/lua.h"
#include "Library/lua/src/lualib.h"

typedef struct {
  char *name;
  int value;
} luaApi_regConst;

/**
 * 初始化Lua接口
 */
void LuaApiInit(lua_State *L);
void LuaApi_reg_constant(lua_State *L, const luaApi_regConst *c);
void LuaApi_create_new_type(lua_State *L, const char *tname, lua_CFunction gc, const luaL_Reg *oo, const char *tparent);
void *LuaApi_check_object_type(lua_State *L, int ud, const char *type);
void *LuaApi_must_object_type(lua_State *L, int ud, const char *type, const char *err_msg);
int LuaApi_check_callable(lua_State* L, int index);

int LuaApi_do_callback(lua_State* L, int ref, int nargs);

#endif /* SRC_API_API_H_ */
