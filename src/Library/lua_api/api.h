/**
 * src/Library/lua_api/api.h
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

#ifndef SRC_API_API_H_
#define SRC_API_API_H_

#include <string.h>

#include "Library/lua/src/lauxlib.h"
#include "Library/lua/src/lua.h"
#include "Library/lua/src/lualib.h"

typedef struct {
  char *name;
  int value;
} luaApi_regConst;

// strnicmp
#define STR_EQUAL(s, os) (strncasecmp((s), os, sizeof(os)) == 0)

/**
 * 初始化Lua接口
 */
void LuaApiInit(lua_State *L);
void LuaApi_reg_constant(lua_State *L, const luaApi_regConst *c);
void LuaApi_create_new_type(lua_State *L, const char *tname, lua_CFunction gc, const luaL_Reg *oo, const char *tparent);
void *LuaApi_check_object_type(lua_State *L, int ud, const char *type); 

#endif /* SRC_API_API_H_ */
