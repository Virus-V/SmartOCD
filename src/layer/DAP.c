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
 * 寻找特定类型的AP
 * 1#：DAPObject对象
 * 2#：AP类型：JTAG、AXI、AHB、APB
 * 返回：
 * 1#：整数
 * 2#：bool OK
 */
static int dap_find_ap(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	const char *apType = luaL_checkstring(L, 2);
	uint8_t apIdx;
	// strnicmp
	if(STR_EQUAL(apType, "JTAG")){
		lua_pushboolean(L, DAP_Find_AP(dapObj, AP_TYPE_JTAG, &apIdx));
	}else if(STR_EQUAL(apType, "AXI")){
		lua_pushboolean(L, DAP_Find_AP(dapObj, AP_TYPE_AMBA_AXI, &apIdx));
	}else if(STR_EQUAL(apType, "AHB")){
		lua_pushboolean(L, DAP_Find_AP(dapObj, AP_TYPE_AMBA_AHB, &apIdx));
	}else if(STR_EQUAL(apType, "APB")){
		lua_pushboolean(L, DAP_Find_AP(dapObj, AP_TYPE_AMBA_APB, &apIdx));
	}else{
		lua_pushboolean(L, 0);
	}
	lua_pushinteger(L, apIdx);
	lua_insert(L, -2);	// 调整位置，将apidx设置为第一个返回值
	return 2;
}

/**
 * 获得AP的标志信息
 * 1#：DAPObject对象
 * ...#：可能的参数："LD:Large Data","LA:Large Address","BE:Big-endian","PT:packed transfer","BT:byte Transfer"
 * 返回：按请求的个数和顺序返回布尔值
 */
static int dap_ap_cap(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	// 保证栈有足够的空间
	int stack_len = lua_gettop(L);
	luaL_checkstack(L, stack_len-1, "Too many args!");
	// 取出参数
	for(int n=2; n<=stack_len; n++){
		const char *ap_flag = luaL_checkstring(L, n);
		if(STR_EQUAL(ap_flag, "LD")){
			lua_pushboolean(L, dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.largeData);
		}else if(STR_EQUAL(ap_flag, "LA")){
			lua_pushboolean(L, dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.largeAddress);
		}else if(STR_EQUAL(ap_flag, "BE")){
			lua_pushboolean(L, dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.bigEndian);
		}else if(STR_EQUAL(ap_flag, "PT")){
			lua_pushboolean(L, dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.packedTransfers);
		}else if(STR_EQUAL(ap_flag, "BT")){
			lua_pushboolean(L, dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.lessWordTransfers);
		}else{
			lua_pushboolean(L, 0);
		}
	}
	return stack_len-1;
}

/**
 * 读取当前选择的AP的ROM TABLE的地址
 * 1#：DAPObject对象
 * 返回：
 * 1#：rom table 地址 inteager
 * 2#：是否成功
 */
static int dap_rom_table(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint64_t rom_addr = 0; uint32_t tmp;
	// 如果当前AP支持大内存扩展
	// XXX ROM Table只支持32位有符号偏移
	// 见ID080813 第7-178页
	if(dapObj->AP[DAP_CURR_AP(dapObj)].ctrl_state.largeAddress){
		if(DAP_AP_Read(dapObj, AP_REG_ROM_MSB, &tmp) == FALSE){
			lua_pushboolean(L, 0);
			goto EXIT;
		}
		rom_addr = tmp;
		rom_addr <<= 32;	// 将ROM MSB寄存器的值放到64位地址的高32位
	}
	// 读取rom TABLE的低32位地址
	if(DAP_AP_Read(dapObj, AP_REG_ROM_LSB, &tmp) == FALSE){
		lua_pushboolean(L, 0);
		rom_addr = 0;
		goto EXIT;
	}
	rom_addr += tmp;
	lua_pushboolean(L, 1);
EXIT:
	lua_pushinteger(L, rom_addr);
	lua_insert(L, -2);
	return 2;
}

/**
 * 读取TAR寄存器数据
 * 1#:DAPObject对象
 * 返回：
 * 1#：TAR内容
 * 2#：OK
 */
static int dap_get_tar(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint64_t addr;
	lua_pushboolean(L, DAP_Read_TAR(dapObj, &addr));
	lua_pushinteger(L, addr);
	lua_insert(L, -2);
	return 2;
}

/**
 * 写入TAR寄存器数据
 * 1#:DAPObject对象
 * 2#：数据
 * 返回：
 * 1#：OK
 */
static int dap_set_tar(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint64_t addr = luaL_checkinteger(L, 2);
	lua_pushboolean(L, DAP_Write_TAR(dapObj, addr));
	return 1;
}

/**
 * 读取8位数据
 * 1#：DAPObject对象
 * 2#：addr：地址64位
 * 返回：
 * 1#：数据
 * 2#：OK
 */
static int dap_mem_read_8(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint64_t addr = luaL_checkinteger(L, 2);
	uint8_t data;
	lua_pushboolean(L, DAP_ReadMem8(dapObj, addr, &data));
	lua_pushinteger(L, data);
	lua_insert(L, -2);
	return 2;
}

/**
 * 读取16位数据
 * 1#：DAPObject对象
 * 2#：addr：地址64位
 * 返回：
 * 1#：数据
 * 2#：OK
 */
static int dap_mem_read_16(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint64_t addr = luaL_checkinteger(L, 2);
	uint16_t data;
	lua_pushboolean(L, DAP_ReadMem16(dapObj, addr, &data));
	lua_pushinteger(L, data);
	lua_insert(L, -2);
	return 2;
}

/**
 * 读取32位数据
 * 1#：DAPObject对象
 * 2#：addr：地址64位
 * 返回：
 * 1#：数据
 * 2#：OK
 */
static int dap_mem_read_32(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint64_t addr = luaL_checkinteger(L, 2);
	uint32_t data;
	lua_pushboolean(L, DAP_ReadMem32(dapObj, addr, &data));
	lua_pushinteger(L, data);
	lua_insert(L, -2);
	return 2;
}

/**
 * 写入32位数据
 * 1#：DAPObject对象
 * 2#：addr：地址64位
 * 3#：data：数据
 * 返回：
 * 1#：OK
 */
static int dap_mem_write_32(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint64_t addr = luaL_checkinteger(L, 2);
	uint32_t data = (uint32_t)luaL_checkinteger(L, 3);
	lua_pushboolean(L, DAP_WriteMem32(dapObj, addr, data));
	return 1;
}

/**
 * 读取Component ID 和 Peripheral ID
 * 1#：DAPObject对象
 * 2#：Addr：地址64位
 * 返回：
 * 1#：cid
 * 2#：pid 64位
 * 3#：OK
 */
static int dap_get_pid_cid(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	uint64_t addr = luaL_checkinteger(L, 2);
	uint32_t cid; uint64_t pid;
	BOOL result = DAP_Read_CID_PID(dapObj, addr, &cid, &pid);
	lua_pushinteger(L, cid);
	lua_pushinteger(L, pid);
	lua_pushboolean(L, result);
	return 3;
}

/**
 * 检查前面操作是否出现错误
 * 1#：DAPObject
 * 返回：
 * 1#：TRUE 没有错误 FALSE 有错误
 * 2#：CTRL/STATE寄存器
 */
static int dap_check_error(lua_State *L){
	DAPObject *dapObj = luaL_checkudata(L, 1, "obj.DAP");
	lua_pushboolean(L, DAP_CheckError(dapObj));
	lua_pushinteger(L, dapObj->CTRL_STAT_Reg.regData);
	return 2;
}

/**
 * 由AP IDR解析可读信息
 * 1#：uint32 整数
 * 返回：字符串
 */
static int dap_parse_APIDR(lua_State *L){
	luaL_Buffer buff;
	AP_IDR_Parse parse;
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
	{"parse_APIDR", dap_parse_APIDR},
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
	{"find_AP", dap_find_ap},
	{"get_AP_Capacity", dap_ap_cap},
	{"get_ROM_Table", dap_rom_table},
	{"get_TAR", dap_get_tar},
	{"set_TAR", dap_set_tar},
	{"readMem8", dap_mem_read_8},
	{"readMem16", dap_mem_read_16},
	{"readMem32", dap_mem_read_32},
	/*
	{"writeMem8"},
	{"writeMem16"},
	*/
	{"writeMem32", dap_mem_write_32},
	{"get_CID_PID", dap_get_pid_cid},
	{"checkError", dap_check_error},
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
