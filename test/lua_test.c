/*
 * lua_test.c
 *
 *  Created on: 2018-4-20
 *      Author: virusv
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "smart_ocd.h"
#include "misc/log.h"
#include "misc/linenoise.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


static int my_math_sin (lua_State *L) {
	lua_pushnumber(L, sin(luaL_checknumber(L, 1)));
	return 1;	// 返回压到栈中的返回值个数
}

static int my_math_cos (lua_State *L) {
	lua_pushnumber(L, cos(luaL_checknumber(L, 1)));
	return 1;
}


static const luaL_Reg mathlib[] = {
	{"my_cos",   my_math_cos},
	{"my_sin",   my_math_sin},
	{NULL, NULL}
};

/*
** Open my_math library
*/
int luaopen_my_math (lua_State *L) {
  luaL_newlib(L, mathlib);
  return 1;
}


int main (){
	lua_State *L = luaL_newstate();
	if (L == NULL) {
		log_fatal("cannot create state: not enough memory.");
		return 1;
	}
	// 打开标准库
	luaL_openlibs(L);

	luaL_requiref(L,"my_math",luaopen_my_math,0);

	return luaL_dostring (L, "my_ = require(\"my_math\")\n"
			"print(my_.my_sin(3.1415926/2))\n"
			"print(my_.my_cos(3.1415926))");
}
















