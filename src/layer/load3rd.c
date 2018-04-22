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
extern void register2lua_TAP(lua_State *L);
extern void register2lua_DAP(lua_State *L);
/**
 * 初始化第三方库
 */
void load3rd(lua_State *L){
	// 注册cmsis-dap仿真器库函数
	register2lua_cmsis_dap(L);
	register2lua_adapter(L);
	register2lua_TAP(L);
	register2lua_DAP(L);
}

/**
 * 新建类型元表
 * gc：该类型的垃圾回收回调
 * oo：面向对象的方法集合
 * 将元表的副本留在栈中
 */
void layer_newTypeMetatable(lua_State *L, const char *tname, lua_CFunction gc, const luaL_Reg *oo){
	// 创建元表
	luaL_newmetatable(L, tname); // +1
	lua_pushvalue(L, -1);	// 复制索引 +1
	// 实现面向对象方式，则将__index指向该元表
	lua_setfield(L, -2, "__index");	// -1
	// 将gc函数压栈
	lua_pushcfunction(L, gc);	// 垃圾回收函数 +1
	lua_setfield (L, -2, "__gc");	// 创建gc -1
	// 注册oo函数，注册到栈顶的表中
	luaL_setfuncs (L, oo, 0);	// -0
	//lua_pop(L, 1);	// 将栈顶的元表弹出
}
