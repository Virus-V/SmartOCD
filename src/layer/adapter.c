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
 * 选择仿真器的传输模式
 * 第一个参数：AdapterObject指针
 * 第二个参数：传输方式类型adapter.SWD或adapter.JTAG。。
 * 如果不提供第二个参数，则表示读取当前活动的传输模式
 * 返回值：无返回值或者1返回值
 * 1#：当前活动的传输模式
 */
static int adapter_transmission_type(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	if(lua_isnone(L, 2)){	// 读取当前活动传输模式
		lua_pushinteger(L, adapterObj->currTrans);
		return 1;
	}else{	// 设置当前传输模式
		int type = (int)luaL_checkinteger(L, 2);	// 类型
		if(adapter_SelectTransmission(adapterObj, type) == FALSE){
			return luaL_error(L, "Set Adapter Transmission to %s Failed!", adapter_Transport2Str(type));
		}
		return 0;
	}

}

/**
 * 判断仿真器是否支持某个传输模式
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
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	lua_settop (L, 4);	// 将栈扩展到4个
	BOOL hard = (BOOL)lua_toboolean(L, 2);
	BOOL srst = (BOOL)lua_toboolean(L, 3);
	int pinWait = (int)luaL_optinteger(L, 4, 0);
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
		return luaL_error(L, "Inserting an state machine change instruction failed!");
	}
	return 0;
}

/**
 * jtag交换TDI TDO
 * 1#：adapter对象
 * 2#:字符串对象
 * 3#:二进制位个数
 * Note：该函数会刷新JTAG指令队列
 */
static int adapter_jtag_exchange_io(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	size_t str_len = 0;
	const char *tdi_data = lua_tolstring (L, 2, &str_len);
	int bitCnt = (int)luaL_checkinteger(L, 3);
	// 判断bit长度是否合法
	if((str_len << 3) < bitCnt ){	// 字符串长度小于要发送的字节数
		return luaL_error(L, "TDI data length is illegal!");
	}
	// 开辟缓冲内存空间
	uint8_t *data = malloc(str_len * sizeof(uint8_t));
	if(data == NULL){
		return luaL_error(L, "TDI data buff alloc Failed!");
	}
	// 拷贝数据
	memcpy(data, tdi_data, str_len * sizeof(uint8_t));
	// 插入执行队列
	if(adapter_JTAG_Exchange_IO(adapterObj, data, bitCnt) == FALSE){
		free(data);
		return luaL_error(L, "Insert to instruction queue failed!");
	}
	// 执行队列
	if(adapter_JTAG_Execute(adapterObj) == FALSE){
		free(data);
		// 清理指令队列
		adapter_JTAG_CleanCommandQueue(adapterObj);
		return luaL_error(L, "Execute the instruction queue failed!");
	}
	// 执行成功，构造lua字符串
	lua_pushlstring(L, data, str_len);
	free(data);	// 释放缓冲
	return 1;
}

/**
 * 在UPDATE之后转入idle状态等待几个时钟周期，以等待慢速的内存操作完成
 * 1#:adapter对象
 * 2#:cycles要进入Idle等待的周期
 */
static int adapter_jtag_idle_wait(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	int cycles = (int)luaL_checkinteger(L, 2);
	if(adapter_JTAG_IdleWait(adapterObj, cycles) == FALSE){
		return luaL_error(L, "Inserting an Idle instruction failed!");
	}
	return 0;
}

/**
 * 执行指令队列
 */
static int adapter_jtag_execute_cmd(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	if(adapter_JTAG_Execute(adapterObj) == FALSE){
		// 清理指令队列
		adapter_JTAG_CleanCommandQueue(adapterObj);
		return luaL_error(L, "Instruction execution failed!");
	}
	return 0;
}

/**
 * 清空指令队列
 * 1#:Adapter对象
 */
static int adapter_jtag_clean_cmd(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	adapter_JTAG_CleanCommandQueue(adapterObj);
	return 0;
}

/**
 * 读取或写入JTAG引脚状态
 * 1#:Adapter对象
 * 2#:要写入的数据
 * 3#:要写入的数据掩码
 * 4#:死区等待时间
 * 返回：
 * 1#:读取的引脚值
 */
static int adapter_jtag_pins(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint8_t pin_data = (uint8_t)luaL_checkinteger(L, 2);
	uint8_t pin_mask = (uint8_t)luaL_checkinteger(L, 3);
	int pin_wait = (int)luaL_checkinteger(L, 4);
	if(adapter_JTAG_RW_Pins(adapterObj, pin_mask, &pin_data, pin_wait) == FALSE){
		return luaL_error(L, "Read/Write JTAG Pins failed!");
	}
	lua_pushinteger(L, pin_data);
	return 1;
}

/**
 * 设置JTAG扫描链上的TAP状态机信息
 * 1#：TAPObject对象
 * 2#：数组，{4,5,8...} 用来表示每个TAP的IR寄存器长度。
 * 返回：无
 */
static int adapter_jtag_set_tap_info(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	luaL_checktype(L, 2, LUA_TTABLE);
	// 获得JTAG扫描链中TAP个数
	int tapCount = lua_rawlen(L, 2);
	// 开辟存放IR Len的空间，会把这个空间当做完全用户数据压栈
	uint16_t *irLens = lua_newuserdata(L, tapCount * sizeof(uint16_t));
	lua_pushnil(L);
	while (lua_next(L, 2) != 0){
		/* 使用 '键' （在索引 -2 处） 和 '值' （在索引 -1 处）*/
		int key_index = (int)lua_tointeger(L, -2);	// 获得索引
		irLens[key_index-1] = (uint16_t)lua_tointeger(L, -1);	// 获得值
		lua_pop(L, 1);	// 将值弹出，键保留在栈中以便下次迭代使用
	}
	// 设置信息
	if(adapter_JTAG_Set_TAP_Info(adapterObj, tapCount, irLens) == FALSE){
		return luaL_error(L, "Set TAP Info Failed!");
	}
	return 0;
}

/**
 * 写某个tap的IR，在调用该函数之前要先调用adapter_jtag_set_tap_info配置TAP信息
 * 1#:adapter对象
 * 2#:TAP 在扫描链的索引
 * 3#:要写入的数据
 */
static int adapter_write_tap_ir(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint16_t tap_index = (uint16_t)luaL_checkinteger(L, 2);
	uint32_t ir_data = (uint32_t)luaL_checkinteger(L, 3);

	if(adapter_JTAG_Wirte_TAP_IR(adapterObj, tap_index, ir_data) == FALSE){
		// 清理指令队列
		adapter_JTAG_CleanCommandQueue(adapterObj);
		return luaL_error(L, "Write %d IR failed!", tap_index);
	}
	return 0;
}

/**
 * 交换某个TAP的DR值
 * 1#:Adapter
 * 2#:TAP 在扫描链的索引
 * 3#:数据字符串
 * 4#:交换数据位长度
 * 返回：
 * 1#:获得数据
 */
static int adapter_exchange_tap_dr(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint16_t tap_index = (uint16_t)luaL_checkinteger(L, 2);
	// 判断tap_index是否合法
	luaL_argcheck(L, tap_index < adapterObj->tap.TAP_Count, 2, "TAP index value is too large!");
	size_t str_len = 0;
	const char *tdi_data = lua_tolstring (L, 3, &str_len);
	int bitCnt = (int)luaL_checkinteger(L, 4);
	// 判断bit长度是否合法
	if((str_len << 3) < bitCnt ){	// 字符串长度小于要发送的字节数
		return luaL_error(L, "TDI data length is illegal!");
	}
	// 开辟缓冲内存空间
	uint8_t *data = malloc(str_len * sizeof(uint8_t));
	if(data == NULL){
		return luaL_error(L, "TDI data buff alloc Failed!");
	}
	// 拷贝数据
	memcpy(data, tdi_data, str_len * sizeof(uint8_t));
	// 插入执行队列
	if(adapter_JTAG_Exchange_TAP_DR(adapterObj, tap_index, data, bitCnt) == FALSE){
		free(data);
		// 清理指令队列
		adapter_JTAG_CleanCommandQueue(adapterObj);
		return luaL_error(L, "Exchange TAP DR failed!");
	}
	// 执行成功，构造lua字符串
	lua_pushlstring(L, data, str_len);
	free(data);	// 释放缓冲
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
	// TAP 状态
	{"TAP_RESET", JTAG_TAP_RESET},
	{"TAP_IDLE", JTAG_TAP_IDLE},
	{"TAP_DR_SELECT", JTAG_TAP_DRSELECT},
	{"TAP_DR_CAPTURE", JTAG_TAP_DRCAPTURE},
	{"TAP_DR_SHIFT", JTAG_TAP_DRSHIFT},
	{"TAP_DR_EXIT1", JTAG_TAP_DREXIT1},
	{"TAP_DR_PAUSE", JTAG_TAP_DRPAUSE},
	{"TAP_DR_EXIT2", JTAG_TAP_DREXIT2},
	{"TAP_DR_UPDATE", JTAG_TAP_DRUPDATE},
	{"TAP_IR_SELECT", JTAG_TAP_IRSELECT},
	{"TAP_IR_CAPTURE", JTAG_TAP_IRCAPTURE},
	{"TAP_IR_SHIFT", JTAG_TAP_IRSHIFT},
	{"TAP_IR_EXIT1", JTAG_TAP_IREXIT1},
	{"TAP_IR_PAUSE", JTAG_TAP_IRPAUSE},
	{"TAP_IR_EXIT2", JTAG_TAP_IREXIT2},
	{"TAP_IR_UPDATE", JTAG_TAP_IRUPDATE},
	// Adapter 状态
	{"STATUS_CONNECTED", ADAPTER_STATUS_CONNECTED},
	{"STATUS_DISCONNECT", ADAPTER_STATUS_DISCONNECT},
	{"STATUS_RUNING", ADAPTER_STATUS_RUNING},
	{"STATUS_IDLE", ADAPTER_STATUS_IDLE},
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
	{"Init", adapter_init},
	{"Deinit", adapter_deinit},
	{"SetStatus", adapter_set_status},
	{"SetClock", adapter_set_clock},
	{"TransType", adapter_transmission_type},
	{"HaveTransType", adapter_have_transmission},
	{"Reset", adapter_reset},
	// JTAG
	{"jtagStatusChange", adapter_jtag_status_change},	// JTAG状态机状态切换
	{"jtagExchangeIO", adapter_jtag_exchange_io},
	{"jtagIdleWait", adapter_jtag_idle_wait},
	{"jtagExecuteCmd", adapter_jtag_execute_cmd},
	{"jtagCleanCmd", adapter_jtag_clean_cmd},
	//
	{"jtagPins", adapter_jtag_pins},
	//
	{"jtagSetTAPInfo", adapter_jtag_set_tap_info},
	{"jtagWriteTAPIR", adapter_write_tap_ir},
	{"jtagExchangeTAPDR", adapter_exchange_tap_dr},
	{NULL, NULL}
};

// 加载器
void register2lua_adapter(lua_State *L){
	// 创建类型元表
	layer_newTypeMetatable(L, "obj.Adapter", adapter_gc, lib_adapter_oo);
	// _LOADED["adapter"] = luaopen_adapter(); // 不设置_G[modname] = luaopen_adapter();
	// 在栈中存在luaopen_adapter()的返回值副本
	luaL_requiref(L, "Adapter", luaopen_adapter, 0);
	lua_pop(L, 1);
}
