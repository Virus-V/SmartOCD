/*
 * DAP.c
 *
 *  Created on: 2018-4-20
 *      Author: virusv
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smart_ocd.h"
#include "misc/log.h"
#include "arch/ARM/ADI/DAP.h"

#include "layer/load3rd.h"

/**
 * 释放DAP对象
 */
static int dap_gc(lua_State *L){
	// 获得对象
	DAPObject *dapObj = lua_touserdata(L, 1);
	__DESTORY(DAP)(dapObj);
	return 0;
}

/**
 * 新建DAP对象
 */
static int dap_new(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	// 开辟TAP对象的空间
	DAPObject *dapObj = lua_newuserdata(L, sizeof(DAPObject));
	// 清零空间
	memset(dapObj, 0x0, sizeof(DAPObject));
	// 初始化CMSIS-DAP
	if(__CONSTRUCT(DAP)(dapObj, adapterObj) == FALSE){
		// 此函数永不返回，惯例用法前面加个return
		return luaL_error(L, "Failed to new DAP Object.");
	}

	luaL_getmetatable(L, "obj.DAP");	// 将元表压栈 +1
	lua_setmetatable(L, -2);	// -1
	return 1;	// 返回压到栈中的返回值个数
}

/**
 * 选择TAP
 * 1#：DAP对象
 * 2#：整数
 * 返回：无
 */
static int dap_set_tap(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint16_t index = (uint16_t)luaL_checkinteger(L, 2);
	DAP_Set_TAP_Index(dapObj, index);
	return 0;
}

/**
 * 设置WAIT的重试次数
 * 1#：DAP对象
 * 2#：整数
 * 返回：无
 */
static int dap_set_retry(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint16_t index = (uint16_t)luaL_checkinteger(L, 2);
	DAP_SetRetry(dapObj, index);
	return 0;
}

/**
 * 读取xP寄存器
 * #1：DAPObject对象引用
 * #2：寄存器的地址
 * 返回：
 * #1：整数 读取的寄存器值
 * #2：BOOL 读取是否成功
 */
static int dap_read_dp(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint8_t reg = (uint8_t)luaL_checkinteger(L, 2);
	uint32_t reg_data;
	lua_pushboolean(L, DAP_DP_Read(dapObj, reg, &reg_data)); // 将结果压栈
	lua_pushinteger(L, reg_data);
	// 将值插入到布尔值之前
	lua_insert(L, -2);
	return 2;
}

static int dap_read_ap(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint8_t reg = (uint8_t)luaL_checkinteger(L, 2);
	uint32_t reg_data;
	lua_pushboolean(L, DAP_AP_Read(dapObj, reg, &reg_data)); // 将结果压栈
	lua_pushinteger(L, reg_data);
	// 将值插入到布尔值之前
	lua_insert(L, -2);
	return 2;
}

/**
 * 写入xP寄存器
 * #1：DAPObject对象引用
 * #2：寄存器的地址
 * #3：要写入的内容
 * 返回：
 * #1：BOOL 读取是否成功
 */
static int dap_write_dp(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint8_t reg = (uint8_t)luaL_checkinteger(L, 2);
	uint32_t reg_data = (uint32_t)luaL_checkinteger(L, 3);
	lua_pushboolean(L, DAP_DP_Write(dapObj, reg, reg_data)); // 将结果压栈
	return 1;
}

static int dap_write_ap(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint8_t reg = (uint8_t)luaL_checkinteger(L, 2);
	uint32_t reg_data = (uint32_t)luaL_checkinteger(L, 3);
	lua_pushboolean(L, DAP_AP_Write(dapObj, reg, reg_data)); // 将结果压栈
	return 1;
}

/**
 * 选择AP
 * MEM-AP、JTAG-AP等
 * 1#：DAPObject对象
 * 2#：AP编号
 * 返回：TRUE FALSE
 */
static int dap_select_ap(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint8_t apSel = (uint8_t)luaL_checkinteger(L, 2);
	lua_pushboolean(L, DAP_AP_Select(dapObj, apSel)); // 将结果压栈
	return 1;
}


/**
 * 写入Abort寄存器
 * 1#：DAPObject对象
 * 2#：要写入Abort寄存器的内容
 * 返回：TRUE FALSE
 */
static int dap_write_abort(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint32_t abort = (uint32_t)luaL_checkinteger(L, 2);
	lua_pushboolean(L, DAP_WriteAbort(dapObj, abort)); // 将结果压栈
	return 1;
}

/**
 * 由AP IDR解析可读信息
 * 1#：uint32 整数
 * 返回：字符串
 */
static int dap_parse_ap_info(lua_State *L){
	luaL_Buffer buff;
	AP_IDRParse parse;
	uint32_t ap_idr = (uint32_t)luaL_checkinteger(L, 1);
	luaL_buffinit(L, &buff);
	parse.regData = ap_idr;
	if(parse.regInfo.Class == 0x0){	// JTAG AP
		luaL_addstring(&buff, "JTAG-AP:JTAG Connection to this AP\n");
	}else if(parse.regInfo.Class == 0x8){	// MEM-AP
		luaL_addstring(&buff, "MEM-AP:");
		switch(parse.regInfo.Type){
		case 0x1:
			luaL_addstring(&buff, "AMBA AHB bus");
			break;
		case 0x2:
			luaL_addstring(&buff, "AMBA APB2 or APB3 bus");
			break;
		case 0x4:
			luaL_addstring(&buff, "AMBA AXI3 or AXI4 bus, with optional ACT-Lite support");
		}
		luaL_addstring(&buff, " connection to this AP.\n");
	}
	// %p 代替 %x，详情见手册
	lua_pushfstring(L, "Revision:%p, Manufacturer:%p, Variant:%p.",
			parse.regInfo.Revision,
			parse.regInfo.JEP106Code,
			parse.regInfo.Variant);
	luaL_addvalue(&buff);
	// 生成字符串
	luaL_pushresult(&buff);
	return 1;
}

static const luaL_Reg lib_dap_f[] = {
	{"new", dap_new},
	{"parse_APIDR", dap_parse_ap_info},
	{NULL, NULL}
};

// 继承TAP的方法
extern const luaL_Reg lib_tap_oo[];
// 继承DAP的方法
static const luaL_Reg lib_dap_oo[] = {
	{"set_TAP", dap_set_tap},
	{"setRetry", dap_set_retry},
	{"read_DP", dap_read_dp},
	{"write_DP", dap_write_dp},
	{"read_AP", dap_read_ap},
	{"write_AP", dap_write_ap},
	{"select_AP", dap_select_ap},
	{"writeAbort", dap_write_abort},
	//{"clearStickyError", tap_get_idcode},
	{NULL, NULL}
};

int luaopen_DAP (lua_State *L) {
  luaL_newlib(L, lib_dap_f);
  return 1;
}

// 加载器
void register2lua_DAP(lua_State *L){
	// 创建类型元表
	layer_newTypeMetatable(L, "obj.DAP", dap_gc, lib_tap_oo);	// 注册TAP的方法 +1
	luaL_setfuncs (L, lib_dap_oo, 0);	// 注册DAP的方法 -0
	// 加载器
	luaL_requiref(L, "DAP", luaopen_DAP, 0);
	lua_pop(L, 2);	// 清空栈
}

/* SRC_LAYER_DAP_C_ */
