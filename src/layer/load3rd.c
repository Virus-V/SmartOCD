/*
 * load3rd.c
 *
 *  Created on: 2018-4-20
 *      Author: virusv
 */

#include "smart_ocd.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

extern void register2lua_cmsis_dap(lua_State *L);
extern void register2lua_adapter(lua_State *L);
/**
 * 初始化第三方库
 */
void load3rd(lua_State *L){
	// 注册cmsis-dap仿真器库函数
	register2lua_cmsis_dap(L);
	register2lua_adapter(L);
}
