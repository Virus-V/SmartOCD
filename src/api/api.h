/*
 * api.h
 *
 *  Created on: 2018-4-20
 *      Author: virusv
 */

#ifndef SRC_API_API_H_
#define SRC_API_API_H_

#include <string.h>

#include "lua/src/lua.h"
#include "lua/src/lauxlib.h"
#include "lua/src/lualib.h"

typedef struct {
	char *name;
	int value;
}luaApi_regConst;

// strnicmp
#define STR_EQUAL(s,os) (strncasecmp((s), os, sizeof(os)) == 0)

/**
 * 初始化Lua接口
 */
void LuaApiInit(lua_State *L);

void LuaApiRegConstant(lua_State *L, const luaApi_regConst *c);

void LuaApiNewTypeMetatable(lua_State *L, const char *tname, lua_CFunction gc, const luaL_Reg *oo);
#endif /* SRC_API_API_H_ */
