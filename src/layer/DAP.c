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
#include "debugger/adapter.h"

#include "layer/load3rd.h"

/**
 * DAP初始化
 * 1#:Adapter对象
 */
static int dap_init(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	if(DAP_Init(adapterObj) == FALSE){
		return luaL_error(L, "DAP initialization failed!");
	}
	return 0;
}

/**
 * 在JTAG模式下设置DAP在扫描链的索引，SWD模式下忽略
 * 1#：Adapter对象
 * 2#：整数
 * 返回：无
 */
static int dap_set_tap(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint16_t index = (uint16_t)luaL_checkinteger(L, 2);
	adapter_DAP_Index(adapterObj, index);
	return 0;
}

/**
 * 设置WAIT的重试次数
 * 1#:Adapter对象
 * 2#：整数
 * 返回：无
 */
static int dap_set_retry(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	int retry = luaL_checkinteger(L, 2);
	adapterObj->dap.Retry = retry;
	return 0;
}

/**
 * 设置IdleCycle：在发起一个Memory Access的时候，可能需要等待其操作完成，IdleCycles就是等待的时间
 * 1#:Adapter对象
 * 2#：整数
 * 返回：无
 */
static int dap_set_idle_cycle(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	int idleCycle = luaL_checkinteger(L, 2);
	adapterObj->dap.IdleCycle = idleCycle;
	return 0;
}

/**
 * 选择AP
 * MEM-AP、JTAG-AP等
 * 1#：Adapter对象
 * 2#：AP编号
 * 返回：无
 */
static int dap_select_ap(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint8_t apSel = (uint8_t)luaL_checkinteger(L, 2);
	if(DAP_AP_Select(adapterObj, apSel) == FALSE){
		return luaL_error(L, "Select AP Failed!");
	}
	return 0;
}

/**
 * 写Abort寄存器或扫描链
 * 1#:Adapter对象
 * 2#:Abort值
 */
static int dap_write_abort(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint32_t abortData = (uint32_t)luaL_checkinteger(L, 2);
	if(adapter_DAP_WriteAbortReg(adapterObj, abortData) == FALSE){
		return luaL_error(L, "Write ABORT failed!");
	}
	return 0;
}


/**
 * 寻找特定类型的AP
 * 1#：Adapter对象
 * 2#：AP类型：dap.AP_TYPE_JTAG ...
 * 返回：
 * 1#：整数
 */
static int dap_find_ap(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	int apType = luaL_checkinteger(L, 2);
	uint8_t apIdx;
	if(DAP_Find_AP(adapterObj, apType, &apIdx) == FALSE){
		return luaL_error(L, "Find AP Failed!");
	}
	// 将结果压栈
	lua_pushinteger(L, apIdx);
	return 1;
}

/**
 * 获得当前AP的标志信息
 * 1#：Adapter对象
 * ...#：可能的参数："LD:Large Data","LA:Large Address","BE:Big-endian","PT:packed transfer","BT:byte Transfer"
 * 返回：按请求的个数和顺序返回布尔值
 */
static int dap_ap_cap(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	// 保证栈有足够的空间
	int stack_len = lua_gettop(L);
	luaL_checkstack(L, stack_len-1, "Too many args!");
	// 取出参数
	for(int n=2; n<=stack_len; n++){
		const char *ap_flag = luaL_checkstring(L, n);
		if(STR_EQUAL(ap_flag, "LD")){
			lua_pushboolean(L, adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.largeData);
		}else if(STR_EQUAL(ap_flag, "LA")){
			lua_pushboolean(L, adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.largeAddress);
		}else if(STR_EQUAL(ap_flag, "BE")){
			lua_pushboolean(L, adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.bigEndian);
		}else if(STR_EQUAL(ap_flag, "PT")){
			lua_pushboolean(L, adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.packedTransfers);
		}else if(STR_EQUAL(ap_flag, "BT")){
			lua_pushboolean(L, adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.lessWordTransfers);
		}else{
			lua_pushboolean(L, 0);
		}
	}
	return stack_len-1;
}

/**
 * 读取当前选择的AP的ROM TABLE的地址
 * 1#：Adapter对象
 * 返回：
 * 1#：rom table 地址 inteager
 */
static int dap_rom_table(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint64_t rom_addr = 0;
	// 如果当前AP支持大内存扩展
	// XXX ROM Table只支持32位有符号偏移
	// 见ID080813 第7-178页
	if(adapterObj->dap.AP[DAP_CURR_AP(adapterObj)].ctrl_state.largeAddress){
		if(adapter_DAP_Read_AP_Single(adapterObj, ROM_MSB, CAST(uint32_t *, &rom_addr) + 1, TRUE) == FALSE){
			return luaL_error(L, "Read BASE_MSB Register Failed!");
		}
	}
	// 读取rom TABLE的低32位地址
	if(adapter_DAP_Read_AP_Single(adapterObj, ROM_LSB, CAST(uint32_t *, &rom_addr), TRUE) == FALSE){
		return luaL_error(L, "Read BASE_LSB Register Failed!");
	}
	// 执行指令
	if(adapter_DAP_Execute(adapterObj) == FALSE){
		return luaL_error(L, "Read BASE Register Failed!");
	}
	lua_pushinteger(L, rom_addr);
	return 1;
}

/**
 * 获得CTRL/STAT寄存器
 * 1#:Adapter对象
 * 返回：
 * 1#：CTRL/STAT
 */
static int dap_get_ctrl_stat(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	lua_pushinteger(L, adapterObj->dap.CTRL_STAT_Reg.regData);
	return 1;
}

/**
 * 写入CTRL/STAT寄存器
 * 1#:Adapter对象
 * 2#：数据
 * 返回：无
 */
static int dap_set_ctrl_stat(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint32_t ctrl_stat_data = luaL_checkinteger(L, 2);
	if(adapter_DAP_Write_DP_Single(adapterObj, CTRL_STAT, ctrl_stat_data, TRUE) == FALSE){
		return luaL_error(L, "Write CTRL/STAT Register Failed!");
	}
	if(adapter_DAP_Execute(adapterObj) == FALSE){	// 自动更新CTRL/STAT
		return luaL_error(L, "Write CTRL/STAT Register Failed!");
	}
	return 0;
}

/**
 * 写入CSW寄存器数据
 * 1#:Adapter对象
 * 2#：数据
 * 返回：无
 */
static int dap_set_csw(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint32_t csw_data = luaL_checkinteger(L, 2);
	if(DAP_WriteCSW(adapterObj, csw_data) == FALSE){
		return luaL_error(L, "Write CSW Register Failed!");
	}
	return 0;
}

/**
 * 读取CSW寄存器数据
 * 1#:Adapter对象
 * 返回：
 * 1#：CSW数据
 */
static int dap_get_csw(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint32_t csw_data = 0;
	if(DAP_ReadCSW(adapterObj, &csw_data) == FALSE){
		return luaL_error(L, "Read CSW Register Failed!");
	}
	lua_pushinteger(L, csw_data);
	return 1;
}

/**
 * 读取8位数据
 * 1#：Adapter对象
 * 2#：addr：地址64位
 * 返回：
 * 1#：数据
 */
static int dap_mem_read_8(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint64_t addr = luaL_checkinteger(L, 2);
	uint8_t data;
	if(DAP_ReadMem8(adapterObj, addr, &data) == FALSE){
		return luaL_error(L, "Read Byte Memory %p Failed!", addr);
	}
	lua_pushinteger(L, data);
	return 1;
}

/**
 * 读取16位数据
 * 1#：Adapter对象
 * 2#：addr：地址64位
 * 返回：
 * 1#：数据
 */
static int dap_mem_read_16(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint64_t addr = luaL_checkinteger(L, 2);
	uint16_t data;
	if(DAP_ReadMem16(adapterObj, addr, &data) == FALSE){
		return luaL_error(L, "Read Halfword Memory %p Failed!", addr);
	}
	lua_pushinteger(L, data);
	return 1;
}

/**
 * 读取32位数据
 * 1#：Adapter对象
 * 2#：addr：地址64位
 * 返回：
 * 1#：数据
 */
static int dap_mem_read_32(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint64_t addr = luaL_checkinteger(L, 2);
	uint32_t data;
	if(DAP_ReadMem32(adapterObj, addr, &data) == FALSE){
		return luaL_error(L, "Read Word Memory %p Failed!", addr);
	}
	lua_pushinteger(L, data);
	return 1;
}

/**
 * 写入8位数据
 * 1#：Adapter对象
 * 2#：addr：地址64位
 * 3#：data：数据
 * 返回：无
 */
static int dap_mem_write_8(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint64_t addr = luaL_checkinteger(L, 2);
	uint8_t data = (uint8_t)luaL_checkinteger(L, 3);
	if(DAP_WriteMem8(adapterObj, addr, data) == FALSE){
		return luaL_error(L, "Write Byte Memory %p Failed!", addr);
	}
	return 0;
}

/**
 * 写入16位数据
 * 1#：Adapter对象
 * 2#：addr：地址64位
 * 3#：data：数据
 * 返回：无
 */
static int dap_mem_write_16(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint64_t addr = luaL_checkinteger(L, 2);
	uint16_t data = (uint16_t)luaL_checkinteger(L, 3);
	if(DAP_WriteMem16(adapterObj, addr, data) == FALSE){
		return luaL_error(L, "Write Halfword Memory %p Failed!", addr);
	}
	return 0;
}


/**
 * 写入32位数据
 * 1#：Adapter对象
 * 2#：addr：地址64位
 * 3#：data：数据
 * 返回：无
 */
static int dap_mem_write_32(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint64_t addr = luaL_checkinteger(L, 2);
	uint32_t data = (uint32_t)luaL_checkinteger(L, 3);
	if(DAP_WriteMem32(adapterObj, addr, data) == FALSE){
		return luaL_error(L, "Write Word Memory %p Failed!", addr);
	}
	return 0;
}

/**
 * 读取内存块
 * 1#:Adapter对象
 * 2#:要读取的地址
 * 3#:地址自增模式
 * 4#:单次传输数据大小
 * 5#:读取多少次
 * 返回：
 * 1#:读取的数据 字符串形式
 */
static int dap_read_mem_block(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint64_t addr = luaL_checkinteger(L, 2);
	int addrIncMode = (int)luaL_checkinteger(L, 3);
	int transSize = (int)luaL_checkinteger(L, 4);
	int transCnt = (int)luaL_checkinteger(L, 5);
	uint32_t *buff = (uint32_t *)lua_newuserdata(L, transCnt * sizeof(uint32_t));
	if(DAP_ReadMemBlock(adapterObj, addr, addrIncMode, transSize, transCnt, buff) == FALSE){
		return luaL_error(L, "Read Block Failed!");
	}
	lua_pushlstring(L, CAST(const char *, buff), transCnt * sizeof(uint32_t));
	return 1;
}

/**
 * 写入内存块
 * 1#:Adapter对象
 * 2#:要读取的地址
 * 3#:地址自增模式
 * 4#:单次传输数据大小
 * 5#:要写的数据（字符串）
 */
static int dap_write_mem_block(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint64_t addr = luaL_checkinteger(L, 2);
	int addrIncMode = (int)luaL_checkinteger(L, 3);
	int transSize = (int)luaL_checkinteger(L, 4);
	size_t transCnt;	// 注意size_t在在64位环境下是8字节，int在64位下是4字节
	uint32_t *buff = (uint32_t *)lua_tolstring (L, 5, &transCnt);
	if(DAP_WriteMemBlock(adapterObj, addr, addrIncMode, transSize, (int)transCnt, buff) == FALSE){
		return luaL_error(L, "Write Block Failed!");
	}
	return 0;
}

/**
 * 读取Component ID 和 Peripheral ID
 * 1#：Adapter对象
 * 2#：Addr：地址64位
 * 返回：
 * 1#：cid
 * 2#：pid 64位
 */
static int dap_get_pid_cid(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint64_t addr = luaL_checkinteger(L, 2);
	uint32_t cid; uint64_t pid;
	if(DAP_Read_CID_PID(adapterObj, addr, &cid, &pid) == FALSE)
		return luaL_error(L, "Read CID PID Failed!");
	lua_pushinteger(L, cid);
	lua_pushinteger(L, pid);
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

// dap的常量
static const lua3rd_regConst lib_dap_const[] = {
	// AP类型
	{"AP_TYPE_JTAG", AP_TYPE_JTAG},
	{"AP_TYPE_AMBA_AHB", AP_TYPE_AMBA_AHB},
	{"AP_TYPE_AMBA_APB", AP_TYPE_AMBA_APB},
	{"AP_TYPE_AMBA_AXI", AP_TYPE_AMBA_AXI},
	//
	{"ADDRINC_OFF", DAP_ADDRINC_OFF},
	{"ADDRINC_SINGLE", DAP_ADDRINC_SINGLE},
	{"ADDRINC_PACKED", DAP_ADDRINC_PACKED},
	// DataSize
	{"DATA_SIZE_8", DAP_DATA_SIZE_8},
	{"DATA_SIZE_16", DAP_DATA_SIZE_16},
	{"DATA_SIZE_32", DAP_DATA_SIZE_32},
	{"DATA_SIZE_64", DAP_DATA_SIZE_64},
	{"DATA_SIZE_128", DAP_DATA_SIZE_128},
	{"DATA_SIZE_256", DAP_DATA_SIZE_256},
//	{"", },
	{NULL, 0},
};

static const luaL_Reg lib_dap_f[] = {
	{"dapParseAPIDR", dap_parse_APIDR},
	{NULL, NULL}
};

// DAP的方法
static const luaL_Reg lib_dap_oo[] = {
	{"Init", dap_init},
	{"SetIndex", dap_set_tap},	// 在JTAG模式下设置DAP在扫描链的索引，SWD模式下忽略
	{"SetRetry", dap_set_retry},
	{"SetIdleCycle", dap_set_idle_cycle},

	{"SelectAP", dap_select_ap},
	{"WriteAbort", dap_write_abort},
	{"FindAP", dap_find_ap},
	{"GetAPCapacity", dap_ap_cap},
	{"GetROMTable", dap_rom_table},
	// TODO 读写AP DP REG
//	{"GetTAR", dap_get_tar},
//	{"SetTAR", dap_set_tar},

	{"GetCTRL_STATReg", dap_get_ctrl_stat},
	{"SetCTRL_STATReg", dap_set_ctrl_stat},
	{"GetCSWReg", dap_get_csw},
	{"SetCSWReg", dap_set_csw},

	{"ReadMem8", dap_mem_read_8},
	{"ReadMem16", dap_mem_read_16},
	{"ReadMem32", dap_mem_read_32},
	//{"dapReadMem64", dap_mem_read_64},
	{"WriteMem8", dap_mem_write_8},
	{"WriteMem16", dap_mem_write_16},
	{"WriteMem32", dap_mem_write_32},
	//{"dapWriteMem64", dap_mem_write_64},
	{"ReadMemBlock", dap_read_mem_block},
	{"WriteMemBlock", dap_write_mem_block},
	{"Get_CID_PID", dap_get_pid_cid},
	{NULL, NULL}
};

int luaopen_DAP (lua_State *L) {
	//luaL_newlib(L, lib_dap_f);
	lua_createtable(L, 0, sizeof(lib_dap_const)/sizeof(lib_dap_const[0]));
	// 注册常量到模块中
	layer_regConstant(L, lib_dap_const);
	luaL_setfuncs (L, lib_dap_f, 0);	// -0 0upvalue
	luaL_setfuncs (L, lib_dap_oo, 0);	// -0
	return 1;
}

// 加载器
void register2lua_DAP(lua_State *L){
	// 加载器
	luaL_requiref(L, "DAP", luaopen_DAP, 0);
	lua_pop(L, 1);	// 清空栈
}

/* SRC_LAYER_DAP_C_ */
