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
 * 返回TRUE FALSE
 */
static int adapter_init(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	lua_pushboolean(L, adapterObj->Init(adapterObj));
	return 1;
}

/**
 * adapter反初始化
 * 第一个参数：AdapterObject指针
 * 返回TRUE FALSE
 */
static int adapter_deinit(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	lua_pushboolean(L, adapterObj->Deinit(adapterObj));
	return 1;
}

/**
 * 设置状态指示灯 如果有的话
 * 第一个参数：AdapterObject指针
 * 第二个参数：字符串 状态：CONNECTED、DISCONNECT、RUNING、IDLE
 * 返回：TRUE FALSE
 */
static int adapter_set_status(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	const char *status = luaL_checkstring(L, 2);
	if(strncasecmp(status, "CONNECTED", 9) == 0){
		lua_pushboolean(L, adapter_SetStatus(adapterObj, ADAPTER_STATUS_CONNECTED));
	}else if(strncasecmp(status, "DISCONNECT", 10) == 0){
		lua_pushboolean(L, adapter_SetStatus(adapterObj, ADAPTER_STATUS_DISCONNECT));
	}else if(strncasecmp(status, "RUNING", 6) == 0){
		lua_pushboolean(L, adapter_SetStatus(adapterObj, ADAPTER_STATUS_RUNING));
	}else if(strncasecmp(status, "IDLE", 4) == 0){
		lua_pushboolean(L, adapter_SetStatus(adapterObj, ADAPTER_STATUS_IDLE));
	}else{	// 其他状态
		lua_pushboolean(L, 0);
	}
	return 1;
}

/**
 * 设置仿真器时钟
 * 第一个参数：AdapterObject指针
 * 第二个参数：整数，频率 Hz
 * 返回：TRUE FALSE
 */
static int adapter_set_clock(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	int clockHz = (int)luaL_checkinteger(L, 2);
	lua_pushboolean(L, adapter_SetClock(adapterObj, clockHz));
	return 1;
}

/**
 * 选择仿真器的传输方式
 * 第一个参数：AdapterObject指针
 * 第二个参数：字符串JTAG、SWD。。
 * 返回值：TRUE FALSE
 */
static int adapter_select_transmission(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	const char *trans = luaL_checkstring(L, 2);
	// strnicmp
	if(strncasecmp(trans, "JTAG", 4) == 0){
		lua_pushboolean(L, adapter_SelectTransmission(adapterObj, JTAG));
	}else if(strncasecmp(trans, "SWD", 3) == 0){
		lua_pushboolean(L, adapter_SelectTransmission(adapterObj, SWD));
	}else{	// 不支持其他传输协议
		lua_pushboolean(L, 0);
	}
	return 1;
}

/**
 * 判断仿真器是否支持某个传输方式
 * 第一个参数：AdapterObject指针
 * 第二个参数：字符串JTAG、SWD。。
 */
static int adapter_have_transmission(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	const char *trans = luaL_checkstring(L, 2);
	// strnicmp
	if(strncasecmp(trans, "JTAG", 4) == 0){
		lua_pushboolean(L, adapter_HaveTransmission(adapterObj, JTAG));
	}else if(strncasecmp(trans, "SWD", 3) == 0){
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
