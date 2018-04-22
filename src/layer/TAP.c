/*
 * TAP.c
 *
 *  Created on: 2018-4-20
 *      Author: virusv
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smart_ocd.h"
#include "misc/log.h"
#include "lib/TAP.h"

#include "layer/load3rd.h"

/**
 * TAP相关操作
 */

/**
 * 释放TAP状态机对象
 */
static int tap_gc(lua_State *L){
	// 获得对象
	TAPObject *tapObj = lua_touserdata(L, 1);
	__DESTORY(TAP)(tapObj);
	return 0;
}

/**
 * 新建TAP状态机对象
 * 第一个参数：AdapterObject对象的引用
 * 返回值：TAP对象
 */
static int tap_new(lua_State *L){
	luaL_checktype(L, 1, LUA_TUSERDATA);
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	// 开辟TAP对象的空间
	TAPObject *tapObj = lua_newuserdata(L, sizeof(TAPObject));
	// 清零空间
	memset(tapObj, 0x0, sizeof(TAPObject));
	if(__CONSTRUCT(TAP)(tapObj, adapterObj) == FALSE){
		// 此函数永不返回，惯例用法前面加个return
		return luaL_error(L, "Failed to new TAP Object.");
	}
	luaL_getmetatable(L, "obj.TAP");	// 将元表压栈 +1
	lua_setmetatable(L, -2);	// -1
	return 1;	// 返回压到栈中的返回值个数
}

/**
 * 复位TAP状态机
 * 第一个参数：TAPObject对象
 * 第二个参数：布尔值，是否硬复位
 * 第三个参数：数字，表示硬复位时引脚延时，默认100
 * 返回：TRUE FALSE
 */
static int tap_reset(lua_State *L){
	luaL_checktype(L, 1, LUA_TUSERDATA);
	luaL_checktype(L, 2, LUA_TBOOLEAN);
	TAPObject *tapObj = luaL_testudata(L, 1, "obj.TAP");
	if(tapObj == NULL){	// 如果是DAP的类型
		tapObj = luaL_testudata(L, 1, "obj.DAP");
	}
	luaL_argcheck(L, tapObj != NULL, 1, "Not a TAP or DAP object.");
	BOOL hard = (BOOL)lua_toboolean(L, 2);
	int pinWait = (int)luaL_optinteger(L, 3, 100);
	// 复位状态机
	lua_pushboolean(L, TAP_Reset(tapObj, hard, pinWait));
	return 1;
}

/**
 * 设置JTAG扫描链上的TAP状态机信息
 * 第一个参数：TAPObject对象
 * 第二个参数：数组，{4,5,8...} 用来表示每个TAP的IR寄存器长度。
 * 返回：TRUE FALSE
 */
static int tap_set_info(lua_State *L){
	luaL_checktype(L, 1, LUA_TUSERDATA);
	luaL_checktype(L, 2, LUA_TTABLE);
	TAPObject *tapObj = luaL_testudata(L, 1, "obj.TAP");
	if(tapObj == NULL){	// 如果是DAP的类型
		tapObj = luaL_testudata(L, 1, "obj.DAP");
	}
	luaL_argcheck(L, tapObj != NULL, 1, "Not a TAP or DAP object.");
	// 获得JTAG扫描链中TAP个数
	int tapCount = lua_rawlen(L, 2);
	// 开辟存放IR Len的空间
	uint16_t *irLens = lua_newuserdata(L, tapCount * sizeof(uint16_t));
	lua_pushnil(L);
	while (lua_next(L, 2) != 0){
		/* 使用 '键' （在索引 -2 处） 和 '值' （在索引 -1 处）*/
		int key_index = (int)lua_tointeger(L, -2);	// 获得索引
		irLens[key_index-1] = (uint16_t)lua_tointeger(L, -1);	// 获得值
		lua_pop(L, 1);	// 将值弹出，键保留在栈中以便下次迭代使用
	}
	// 设置信息
	lua_pushboolean(L, TAP_SetInfo(tapObj, tapCount, irLens));
	return 1;
}

/**
 * 获得idcode
 * 第一个参数：TAPObject对象引用
 * 返回：表
 */
static int tap_get_idcode(lua_State *L){
	luaL_checktype(L, 1, LUA_TUSERDATA);
	TAPObject *tapObj = luaL_testudata(L, 1, "obj.TAP");
	if(tapObj == NULL){	// 如果是DAP的类型
		tapObj = luaL_testudata(L, 1, "obj.DAP");
	}
	luaL_argcheck(L, tapObj != NULL, 1, "Not a TAP or DAP object.");
	// IDCODE的缓存空间
	uint32_t *idCodes = lua_newuserdata(L, tapObj->TAP_Count * sizeof(uint32_t));
	if(TAP_Get_IDCODE(tapObj, idCodes) == FALSE){
		lua_pushnil(L);
		return 1;
	}
	// 将IDCODE组装成一个表
	lua_createtable(L, tapObj->TAP_Count, 0); // +1
	for(int idx=0; idx < tapObj->TAP_Count; idx++){
		lua_pushinteger(L, idCodes[idx]); // +1
		lua_seti(L, -2, idx+1); // -1
	}
	return 1;
}

static const luaL_Reg lib_tap_f[] = {
	{"new", tap_new},	// 新建TAP对象
	{NULL, NULL}
};

// XXX 这个没有声明为static 因为DAP对象是TAP的子类，所以DAP需要继承TAP对象的方法
const luaL_Reg lib_tap_oo[] = {
	{"reset", tap_reset},
	{"setInfo", tap_set_info},
	{"get_IDCODE", tap_get_idcode},
	// TODO 实现低级操作TAP的方法
	//{"write_IR", adapter_set_clock},
	//{"exchange_DR", adapter_select_transmission},
	//{"execute", adapter_have_transmission},
	{NULL, NULL}
};

int luaopen_TAP (lua_State *L) {
  luaL_newlib(L, lib_tap_f);
  return 1;
}

// 加载器
void register2lua_TAP(lua_State *L){
	// 创建类型元表
	layer_newTypeMetatable(L, "obj.TAP", tap_gc, lib_tap_oo);
	// 加载器
	luaL_requiref(L, "TAP", luaopen_TAP, 0);
	lua_pop(L, 2);	// 清空栈
}
/* SRC_LAYER_TAP_C_ */
