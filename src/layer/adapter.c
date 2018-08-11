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
	int type = (int)luaL_checkinteger(L, 2);	// 类型
	if(adapter_SetStatus(adapterObj, type) == FALSE){
		return luaL_error(L, "Set Adapter Status %s Failed!", adapter_Status2Str(type));
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
 * 第二个参数：传输方式类型adapter.SWD或adapter.JTAG。。
 * 返回值：无
 */
static int adapter_select_transmission(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	int type = (int)luaL_checkinteger(L, 2);	// 类型
	if(adapter_SelectTransmission(adapterObj, type) == FALSE){
		return luaL_error(L, "Set Adapter Transmission to %s Failed!", adapter_Transport2Str(type));
	}
	return 0;
}

/**
 * 判断仿真器是否支持某个传输方式
 * 第一个参数：AdapterObject指针
 * 第二个参数：传输方式类型adapter.SWD或adapter.JTAG。。
 * 返回：TRUE FALSE
 */
static int adapter_have_transmission(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	int type = (int)luaL_checkinteger(L, 2);	// 类型
	lua_pushboolean(L, adapter_HaveTransmission(adapterObj, type));
	return 1;
}

/**
 * 复位Target
 * 1#:adapter对象
 * 2#：bool hard 是否硬复位（可选）
 * 3#: bool srst 是否系统复位（可选）
 * 4#：int pinWait（可选）
 * 无返回值，失败会报错
 */
static int adapter_reset(lua_State *L){
	lua_settop (L, 4);	// 将栈扩展到4个
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	BOOL hard = (BOOL)lua_toboolean(L, 2);
	BOOL srst = (BOOL)lua_toboolean(L, 3);
	int pinWait = (int)luaL_optinteger(L, 4, 0);
	//lua_pushboolean(L, adapter_Reset(adapterObj, hard, srst, pinWait));
	//return 1;
	if(adapter_Reset(adapterObj, hard, srst, pinWait) == FALSE){
		return luaL_error(L, "Reset Adapter Failed!");
	}
	return 0;
}

/**
 * JTAG状态机切换
 * 1#：adapter对象
 * 2#：状态
 */
static int adapter_jtag_status_change(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	int status = (int)luaL_checkinteger(L, 2);
	if(status < JTAG_TAP_RESET || status > JTAG_TAP_IRUPDATE){
		return luaL_error(L, "JTAG state machine new state is illegal!");
	}
	if(adapter_JTAG_StatusChange(adapterObj, status) == FALSE){
		return luaL_error(L, "JTAG state machine change state failed!");
	}
	return 0;
}

/**
 * jtag交换TDI TDO
 * 1#：adapter对象
 * 2#:字符串对象 00101010 从字符串开始依次发出
 */
static int adapter_jtag_exchange_io(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	int tdi_len = 0;
	const char *tdi_data = lua_tolstring (L, 2, &tdi_len);

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

// 模块函数以及常量
//static const luaL_Reg lib_adapter_f[] = {
//	{"new", dap_new},
//	{"parse_APIDR", dap_parse_APIDR},
//	{NULL, NULL}
//};

// 模块常量
static const lua3rd_regConst lib_adapter_const[] = {
	// 传输方式
	{"JTAG", JTAG},
	{"SWD", SWD},
	// JTAG引脚bit mask
	{"PIN_SWCLK_TCK", SWJ_PIN_SWCLK_TCK},
	{"PIN_SWDIO_TMS", SWJ_PIN_SWDIO_TMS},
	{"PIN_TDI", SWJ_PIN_TDI},
	{"PIN_TDO", SWJ_PIN_TDO},
	{"PIN_nTRST", SWJ_PIN_nTRST},
	{"PIN_nRESET", SWJ_PIN_nRESET},
	// Adapter 状态
	{"STATUS_CONNECTED", ADAPTER_STATUS_CONNECTED},
	{"STATUS_DISCONNECT", ADAPTER_STATUS_DISCONNECT},
	{"STATUS_RUNING", ADAPTER_STATUS_RUNING},
	{"STATUS_IDLE", ADAPTER_STATUS_IDLE},
	// DP寄存器
	{"DPREG_CTRL_STAT", CTRL_STAT},
	{"DPREG_SELECT", SELECT},
	{"DPREG_RDBUFF", RDBUFF},
	{"DPREG_DPIDR", DPIDR},
	{"DPREG_ABORT", ABORT},
	{"DPREG_DLCR", DLCR},
	{"DPREG_RESEND", RESEND},
	{"DPREG_TARGETID", TARGETID},
	{"DPREG_DLPIDR", DLPIDR},
	{"DPREG_EVENTSTAT", EVENTSTAT},
	{"DPREG_TARGETSEL", TARGETSEL},
	// AP寄存器
	{"APREG_CSW", CSW},
	{"APREG_TAR_LSB", TAR_LSB},
	{"APREG_TAR_MSB", TAR_MSB},
	{"APREG_DRW", DRW},
	{"APREG_BD0", BD0},
	{"APREG_BD1", BD1},
	{"APREG_BD2", BD2},
	{"APREG_BD3", BD3},
	{"APREG_CFG", CFG},
	{"APREG_ROM_LSB", ROM_LSB},
	{"APREG_ROM_MSB", ROM_MSB},
	{"APREG_IDR", IDR},
	// JTAG 扫描链
	{"JTAG_ABORT", JTAG_ABORT},
	{"JTAG_DPACC", JTAG_DPACC},
	{"JTAG_APACC", JTAG_APACC},
	{"JTAG_IDCODE", JTAG_IDCODE},
	{"JTAG_BYPASS", JTAG_BYPASS},
	{"JTAG_RESP_OK_FAULT", JTAG_RESP_OK_FAULT},
	{"JTAG_RESP_WAIT", JTAG_RESP_WAIT},
	{NULL, 0}
};

// 初始化Adapter库
int luaopen_adapter (lua_State *L) {
	// 新建一张表，将函数注册到表中：创建c闭包（adapter_init）直接加入到表中，键为函数名字符串（init）
	// 将这个表留在栈中
	//luaL_newlib(L, lib_adapter_f);
	// create module table
	lua_createtable(L, 0, sizeof(lib_adapter_const)/sizeof(lib_adapter_const[0]));	// 预分配索引空间，提高效率
	// 注册常量到模块中
	layer_regConstant(L, lib_adapter_const);
	// 将函数注册进去
	//luaL_setfuncs(L, lib_adapter_f, 0);
	return 1;
}

// 模块的面向对象方法
static const luaL_Reg lib_adapter_oo[] = {
	{"init", adapter_init},
	{"deinit", adapter_deinit},
	{"setStatus", adapter_set_status},
	{"setClock", adapter_set_clock},
	{"selectTransmission", adapter_select_transmission},
	{"haveTransmission", adapter_have_transmission},
	{"reset", adapter_reset},
	{"jtagStatusChange", adapter_jtag_status_change},	// JTAG状态机状态切换
	{"jtagExchangeIO", adapter_jtag_exchange_io},	// 用字符串实现，luaL_Buffer
	{"jtagIdleWait"},
	{"jtagExecuteCmd"},
	{"jtagCleanCmd"},
	//
	{"jtagPins"},
	//
	{"jtagSetTAPInfo"},
	{"jtagWriteTAPIR"},
	{"jtagExchangeTAPDR"},
	// dap实现？
//	{},
//	{},
//	{},
//	{},
	{NULL, NULL}
};

// 加载器
void register2lua_adapter(lua_State *L){
	// 创建类型元表
	layer_newTypeMetatable(L, "obj.Adapter", adapter_gc, lib_adapter_oo);
	// _LOADED["adapter"] = luaopen_adapter(); // 不设置_G[modname] = luaopen_adapter();
	// 在栈中存在luaopen_adapter()的返回值副本
	luaL_requiref(L, "adapter", luaopen_adapter, 0);
	lua_pop(L, 1);
}
