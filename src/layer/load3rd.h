/*
 * load3rd.h
 *
 *  Created on: 2018-4-20
 *      Author: virusv
 */

#ifndef SRC_LAYER_LOAD3RD_H_
#define SRC_LAYER_LOAD3RD_H_

#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

typedef struct lua3rd_regConst{
	char *name;
	int value;
}lua3rd_regConst;

// strnicmp
#define STR_EQUAL(s,os) (strncasecmp((s), os, sizeof(os)) == 0)

void load3rd(lua_State *L);
void layer_regConstant(lua_State *L, lua3rd_regConst *c);
void layer_newTypeMetatable(lua_State *L, const char *tname, lua_CFunction gc, const luaL_Reg *oo);
#endif /* SRC_LAYER_LOAD3RD_H_ */
