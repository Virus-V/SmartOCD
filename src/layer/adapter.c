/*
 * adapter.c
 *
 *  Created on: 2018-4-20
 *      Author: virusv
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smart_ocd.h"
#include "misc/log.h"
#include "debugger/adapter.h"

#include "layer/load3rd.h"

/**
 * Adapter仿真器通用接口
 * 设置仿真器JTAG的CLK频率
 * 设置仿真器的传输方式
 * 判断是否支持某一个传输方式
 * 返回传输方式的字符串形式
 */

/**
 * adapter初始化
 * 第一个参数：AdapterObject指针
 * 返回 无
 */
static int adapter_init(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	if(adapterObj->Init(adapterObj) == FALSE){
		return luaL_error(L, "Adapter Init Failed!");
	}
	return 0;
}

/**
 * adapter反初始化
 * 第一个参数：AdapterObject指针
 * 返回TRUE FALSE
 */
static int adapter_deinit(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	if(adapterObj->Deinit(adapterObj) == FALSE){
		return luaL_error(L, "Adapter Deinit Failed!");
	}
	return 0;
}

/**
 * 设置状态指示灯 如果有的话
 * 第一个参数：AdapterObject指针
 * 第二个参数：字符串 状态：CONNECTED、DISCONNECT、RUNING、IDLE
 * 返回：无
 */
static int adapter_set_status(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	const char *status = luaL_checkstring(L, 2);
	BOOL result = FALSE;
	if(STR_EQUAL(status, "CONNECTED")){
		result = adapter_SetStatus(adapterObj, ADAPTER_STATUS_CONNECTED);
	}else if(STR_EQUAL(status, "DISCONNECT")){
		result = adapter_SetStatus(adapterObj, ADAPTER_STATUS_DISCONNECT);
	}else if(STR_EQUAL(status, "RUNING")){
		result = adapter_SetStatus(adapterObj, ADAPTER_STATUS_RUNING);
	}else if(STR_EQUAL(status, "IDLE")){
		result = adapter_SetStatus(adapterObj, ADAPTER_STATUS_IDLE);
	}
	if(result == FALSE){
		return luaL_error(L, "Set Adapter Status %s Failed!", status);
	}
	return 0;
}

/**
 * 设置仿真器时钟
 * 第一个参数：AdapterObject指针
 * 第二个参数：整数，频率 Hz
 */
static int adapter_set_clock(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	int clockHz = (int)luaL_checkinteger(L, 2);
	if(adapter_SetClock(adapterObj, clockHz) == FALSE){
		return luaL_error(L, "Set Adapter Clock Failed!");
	}
	return 0;
}

/**
 * 选择仿真器的传输方式
 * 第一个参数：AdapterObject指针
 * 第二个参数：字符串JTAG、SWD。。
 * 返回值：无
 */
static int adapter_select_transmission(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	const char *trans = luaL_checkstring(L, 2);
	BOOL result = FALSE;
	// strnicmp
	if(STR_EQUAL(trans, "JTAG")){
		result = adapter_SelectTransmission(adapterObj, JTAG);
	}else if(STR_EQUAL(trans, "SWD")){
		result = adapter_SelectTransmission(adapterObj, SWD);
	}
	if(result == FALSE){
		return luaL_error(L, "Set Adapter Transmission to %s Failed!", trans);
	}
	return 0;
}

/**
 * 判断仿真器是否支持某个传输方式
 * 第一个参数：AdapterObject指针
 * 第二个参数：字符串JTAG、SWD。。
 * 返回：TRUE FALSE
 */
static int adapter_have_transmission(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	const char *trans = luaL_checkstring(L, 2);
	// strnicmp
	if(STR_EQUAL(trans, "JTAG")){
		lua_pushboolean(L, adapter_HaveTransmission(adapterObj, JTAG));
	}else if(STR_EQUAL(trans, "SWD")){
		lua_pushboolean(L, adapter_HaveTransmission(adapterObj, SWD));
	}else{	// 不支持其他传输协议
		lua_pushboolean(L, 0);
	}
	return 1;
}

/**
 * Adapter垃圾回收函数
 */
static int adapter_gc(lua_State *L){
	AdapterObject *adapterObj = lua_touserdata(L, 1);
	// 销毁Adapter对象
	adapterObj->Destroy(adapterObj);
	return 0;
}

static const luaL_Reg lib_adapter_oo[] = {
	{"init", adapter_init},
	{"deinit", adapter_deinit},
	{"setStatus", adapter_set_status},
	{"setClock", adapter_set_clock},
	{"selectTransmission", adapter_select_transmission},
	{"haveTransmission", adapter_have_transmission},
	{NULL, NULL}
};

// 初始化Adapter库
//int luaopen_adapter (lua_State *L) {
//	luaL_newlib(L, adapter_lib);
//	return 1;
//}

// 加载器
void register2lua_adapter(lua_State *L){
	// 创建类型元表
	layer_newTypeMetatable(L, "obj.Adapter", adapter_gc, lib_adapter_oo);
	//luaL_requiref(L, "adapter", luaopen_adapter, 0);
	lua_pop(L, 1);
}
