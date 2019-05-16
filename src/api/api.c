/*
 * api.c
 *
 *  Created on: 2018-4-20
 *      Author: virusv
 */

#include "smart_ocd.h"
#include "api/api.h"

extern void RegisterApi_CmsisDap(lua_State *L);
/**
 * 初始化Lua接口
 */
void LuaApiInit(lua_State *L){
	// 注册cmsis-dap仿真器库函数
	RegisterApi_CmsisDap(L);
}

/**
 * 往栈顶的table中注册常量
 * The table must exist at the top of the stack before calling this function.
 */
void LuaApiRegConstant(lua_State *L, const luaApi_regConst *c){
	assert(L != NULL);
	while(c->name != NULL){
		// TODO distinguish constant type
		lua_pushinteger(L, c->value);
		// register constant to the table
		lua_setfield(L, -2, c->name);
		c++;
	}
}

/**
 * 新建类型元表
 * gc：该类型的垃圾回收回调
 * oo：面向对象的方法集合
 * 将元表的副本留在栈中
 */
void LuaApiNewTypeMetatable(lua_State *L, const char *tname, lua_CFunction gc, const luaL_Reg *oo){
	assert(L != NULL);
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
