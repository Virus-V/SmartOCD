/*
 * cmsis_dap.c
 *
 *  Created on: 2018-4-20
 *      Author: virusv
 */

#include <stdio.h>
#include <stdlib.h>
#include "smart_ocd.h"
#include "misc/log.h"
#include "adapter/cmsis-dap/cmsis-dap.h"

#include "api/api.h"

// 注意!!!!所有Adapter对象的metatable都要以 "adapter." 开头!!!!
#define CMDAP_LUA_OBJECT_TYPE "adapter.CMSIS-DAP"

/**
 * 设置状态指示灯 如果有的话
 * 第一个参数：AdapterObject指针
 * 第二个参数：字符串 状态：CONNECTED、DISCONNECT、RUNING、IDLE
 * 返回：无
 */
static int luaApi_adapter_set_status(lua_State *L){
	// 获得CMSIS-DAP接口对象
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	int type = (int)luaL_checkinteger(L, 2);	// 类型
	if(cmdapObj->SetStatus(cmdapObj, type) != ADPT_SUCCESS){
		return luaL_error(L, "Set CMSIS-DAP status failed!");
	}
	return 0;
}

/**
 * 设置仿真器时钟
 * 第一个参数：AdapterObject指针
 * 第二个参数：整数，频率 Hz
 */
static int luaApi_adapter_set_frequent(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	int clockHz = (int)luaL_checkinteger(L, 2);
	if(cmdapObj->SetFrequent(cmdapObj, clockHz) != ADPT_SUCCESS){
		return luaL_error(L, "Set CMSIS-DAP clock failed!");
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
static int luaApi_adapter_transmission_mode(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	if(lua_isnone(L, 2)){	// 读取当前活动传输模式
		lua_pushinteger(L, cmdapObj->currTransMode);
		return 1;
	}else{	// 设置当前传输模式
		int type = (int)luaL_checkinteger(L, 2);	// 类型
		if(cmdapObj->SetTransferMode(cmdapObj, type) != ADPT_SUCCESS){
			return luaL_error(L, "Set CMSIS-DAP transmission failed!");
		}
		return 0;
	}

}

/**
 * 复位Target
 * 1#:adapter对象
 * 2#：复位类型
 * 无返回值，失败会报错
 */
static int luaApi_adapter_reset(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	int type = (int)luaL_optinteger(L, 2, ADPT_RESET_SYSTEM_RESET);
	if(cmdapObj->Reset(cmdapObj, type) != ADPT_SUCCESS){
		return luaL_error(L, "Reset CMSIS-DAP failed!");
	}
	return 0;
}

/**
 * JTAG状态机切换
 * 1#：adapter对象
 * 2#：状态
 */
static int luaApi_adapter_jtag_status_change(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	int status = (int)luaL_checkinteger(L, 2);
	if(status < JTAG_TAP_RESET || status > JTAG_TAP_IRUPDATE){
		return luaL_error(L, "JTAG state machine new state is illegal!");
	}
	if(cmdapObj->JtagToState(cmdapObj, status) != ADPT_SUCCESS){
		return luaL_error(L, "Insert to instruction queue failed!");
	}
	// 执行队列
	if(cmdapObj->JtagCommit(cmdapObj) != ADPT_SUCCESS){
		// 清理指令队列
		cmdapObj->JtagCleanPending(cmdapObj);
		return luaL_error(L, "Execute the instruction queue failed!");
	}
	return 0;
}

/**
 * jtag交换TDI TDO
 * 1#：adapter对象
 * 2#:字符串对象
 * 3#:二进制位个数
 * Note：该函数会刷新JTAG指令队列,因为要同步获得数据
 * 返回：
 * 1#：捕获到的TDO数据
 */
static int luaApi_adapter_jtag_exchange_data(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	size_t str_len = 0;
	const char *tdi_data = lua_tolstring (L, 2, &str_len);
	unsigned int bitCnt = (unsigned int)luaL_checkinteger(L, 3);
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
	// 插入JTAG指令
	if(cmdapObj->JtagExchangeData(cmdapObj, data, bitCnt) != ADPT_SUCCESS){
		free(data);
		return luaL_error(L, "Insert to instruction queue failed!");
	}
	// 执行队列
	if(cmdapObj->JtagCommit(cmdapObj) != ADPT_SUCCESS){
		free(data);
		// 清理指令队列
		cmdapObj->JtagCleanPending(cmdapObj);
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
static int luaApi_adapter_jtag_idle_wait(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	unsigned int cycles = (unsigned int)luaL_checkinteger(L, 2);
	if(cmdapObj->JtagIdle(cmdapObj, cycles) != ADPT_SUCCESS){
		return luaL_error(L, "Insert to instruction queue failed!");
	}
	// 执行队列
	if(cmdapObj->JtagCommit(cmdapObj) != ADPT_SUCCESS){
		// 清理指令队列
		cmdapObj->JtagCleanPending(cmdapObj);
		return luaL_error(L, "Execute the instruction queue failed!");
	}
	return 0;
}

/**
 * 读取或写入JTAG引脚状态
 * 1#:Adapter对象
 * 2#:引脚掩码
 * 3#:要写入的数据
 * 4#:死区等待时间
 * 返回：
 * 1#:读取的引脚值
 */
static int luaApi_adapter_jtag_pins(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	uint8_t pin_mask = (uint8_t)luaL_checkinteger(L, 2);
	uint8_t pin_data = (uint8_t)luaL_checkinteger(L, 3);
	unsigned int pin_wait = (unsigned int)luaL_checkinteger(L, 4);
	if(cmdapObj->JtagPins(cmdapObj, pin_mask, pin_data, &pin_data, pin_wait) != ADPT_SUCCESS){
		return luaL_error(L, "Read/Write JTAG pins failed!");
	}
	lua_pushinteger(L, pin_data);
	return 1;
}

/**
 * DAP单次读寄存器
 * 1#:Adapter对象
 * 2#:type寄存器类型 AP还是DP
 * 3#:reg 寄存器号
 * 返回:
 * 1#:寄存器的值
 */
static int luaApi_adapter_dap_single_read(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	uint32_t data;
	int type = (int)luaL_checkinteger(L, 2);
	int reg = (int)luaL_checkinteger(L, 3);
	if(cmdapObj->DapSingleRead(cmdapObj, type, reg, &data) != ADPT_SUCCESS){
		return luaL_error(L, "Insert to instruction queue failed!");
	}
	// 执行队列
	if(cmdapObj->DapCommit(cmdapObj) != ADPT_SUCCESS){
		// 清理指令队列
		cmdapObj->DapCleanPending(cmdapObj);
		return luaL_error(L, "Execute the instruction queue failed!");
	}
	lua_pushinteger(L, data);
	return 1;
}

/**
 * DAP单次写寄存器
 * 1#:Adapter对象
 * 2#:type寄存器类型 AP还是DP
 * 3#:reg 寄存器号
 * 4#:data 写的值
 */
static int luaApi_adapter_dap_single_write(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	int type = (int)luaL_checkinteger(L, 2);
	int reg = (int)luaL_checkinteger(L, 3);
	uint32_t data = (uint32_t)luaL_checkinteger(L, 4);
	if(cmdapObj->DapSingleWrite(cmdapObj, type, reg, data) != ADPT_SUCCESS){
		return luaL_error(L, "Insert to instruction queue failed!");
	}
	// 执行队列
	if(cmdapObj->DapCommit(cmdapObj) != ADPT_SUCCESS){
		// 清理指令队列
		cmdapObj->DapCleanPending(cmdapObj);
		return luaL_error(L, "Execute the instruction queue failed!");
	}
	return 0;
}

/**
 * DAP多次读寄存器
 * 1#:Adapter对象
 * 2#:type寄存器类型 AP还是DP
 * 3#:reg 寄存器号
 * 4#:count 读取次数
 * 返回
 * 1#:读的数据
 */
static int luaApi_adapter_dap_multi_read(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	int type = (int)luaL_checkinteger(L, 2);
	int reg = (int)luaL_checkinteger(L, 3);
	int count = (int)luaL_checkinteger(L, 4);
	// 开辟缓冲内存空间
	uint32_t *buff = malloc(count * sizeof(uint32_t));
	if(buff == NULL){
		return luaL_error(L, "Multi-read buff alloc Failed!");
	}
	if(cmdapObj->DapMultiRead(cmdapObj, type, reg, count, buff) != ADPT_SUCCESS){
		return luaL_error(L, "Insert to instruction queue failed!");
	}
	// 执行队列
	if(cmdapObj->DapCommit(cmdapObj) != ADPT_SUCCESS){
		free(buff);
		// 清理指令队列
		cmdapObj->DapCleanPending(cmdapObj);
		return luaL_error(L, "Execute the instruction queue failed!");
	}
	lua_pushlstring(L, buff, count * sizeof(uint32_t));
	free(buff);
	return 1;
}

/**
 * DAP多次写寄存器
 * 1#:Adapter对象
 * 2#:type寄存器类型 AP还是DP
 * 3#:reg 寄存器号
 * 4#:data 写的数据(字符串)
 */
static int luaApi_adapter_dap_multi_write(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	int type = (int)luaL_checkinteger(L, 2);
	int reg = (int)luaL_checkinteger(L, 3);
	size_t transCnt;	// 注意size_t在在64位环境下是8字节，int在64位下是4字节
	uint32_t *buff = (uint32_t *)luaL_checklstring (L, 4, &transCnt);
	if(transCnt == 0 || (transCnt & 0x3)){
		return luaL_error(L, "The length of the data to be written is not a multiple of the word.");
	}
	if(cmdapObj->DapMultiWrite(cmdapObj, type, reg, transCnt >> 2, buff) != ADPT_SUCCESS){
		return luaL_error(L, "Insert to instruction queue failed!");
	}
	// 执行队列
	if(cmdapObj->DapCommit(cmdapObj) != ADPT_SUCCESS){
		// 清理指令队列
		cmdapObj->DapCleanPending(cmdapObj);
		return luaL_error(L, "Execute the instruction queue failed!");
	}
	return 0;
}

/**
 * 新建CMSIS-DAP对象
 */
static int luaApi_cmsis_dap_new (lua_State *L) {
	// 创建一个userdata类型的变量来保存Adapter对象指针
	Adapter *cmdapObj = CAST(Adapter *, lua_newuserdata(L, sizeof(Adapter)));	// +1
	// 创建CMSIS-DAP对象
	*cmdapObj = CreateCmsisDap();
	if(!*cmdapObj){
		// 此函数永不返回，惯例用法前面加个return
		return luaL_error(L, "Failed to create CMSIS-DAP Object.");
	}
	// 获得CMSIS-DAP对象的元表
	luaL_setmetatable(L, CMDAP_LUA_OBJECT_TYPE);	// 将元表压栈 +1
	return 1;	// 返回压到栈中的返回值个数
}

/**
 * 连接CMSIS-DAP对象
 * 第一个参数是CMSIS-DAP对象
 * 第二个参数是数组，存放PID和VID {{vid,pid}...}
 * 第三个参数是serial number
 */
static int luaApi_cmsis_dap_connect(lua_State *L){
	luaL_checktype (L, 2, LUA_TTABLE);
	const char *serial = NULL;
	if(!lua_isnone(L, 3)){
		serial = lua_tostring (L, 3);
	}
	// 获得CMSIS-DAP接口对象
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	// 获得vid和pid的表长度
	int len_VIDs_PIDs = lua_rawlen(L, 2);
	luaL_argcheck (L, len_VIDs_PIDs != 0, 2, "The length of the vid and pid parameter arrays is wrong.");
	// 分配空间
	uint16_t *vid_pid_buff = lua_newuserdata(L, (len_VIDs_PIDs << 1) * sizeof(uint16_t));
	uint16_t *vids = vid_pid_buff;
	uint16_t *pids = vid_pid_buff + len_VIDs_PIDs;
	// vid pid表在栈中的索引为2
	lua_pushnil(L);
	while (lua_next(L, 2) != 0){
		/* 使用 '键' （在索引 -2 处） 和 '值' （在索引 -1 处）*/
		int vid_pid_arr = lua_gettop(L);	// 返回栈顶元素的索引值
		int key_index = (int)lua_tointeger(L, vid_pid_arr-1);	// 获得当前key
		if(lua_rawgeti (L, vid_pid_arr, 1) != LUA_TNUMBER){	// 检查数据类型
			return luaL_error(L, "VID is not a number.");
		}
		// lua的数组下标以1开始
		vids[key_index-1] = (uint16_t)lua_tointeger(L, -1);	// 获得当前vid
		if(lua_rawgeti (L, vid_pid_arr, 2) != LUA_TNUMBER){	// 检查数据类型
			return luaL_error(L, "PID is not a number.");
		}
		pids[key_index-1] = (uint16_t)lua_tointeger(L, -1);	// 获得当前pid
		lua_pop(L, 3);	// 将3个值弹出，键保留在栈中以便下次迭代使用
	}
	// 连接CMSIS-DAP，返回结果
	if (ConnectCmsisDap(cmdapObj, vids, pids, serial) != ADPT_SUCCESS){
		return luaL_error(L, "Couldn't connect CMSIS-DAP!");
	}
	return 0;
}

/**
 * 配置JTAG
 * 1#:Adapter对象
 * 2#:ir表
 */
static int luaApi_cmsis_dap_jtag_configure(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	luaL_checktype(L, 2, LUA_TTABLE);
	// 获得JTAG扫描链中TAP个数
	uint8_t tapCount = (uint8_t)lua_rawlen(L, 2);
	// 开辟存放IR Len的空间，会把这个空间当做完全用户数据压栈
	uint8_t *irLens = lua_newuserdata(L, tapCount * sizeof(uint8_t));
	lua_pushnil(L);
	// XXX 防止表过大溢出
	while (lua_next(L, 2) != 0){
		/* 使用 '键' （在索引 -2 处） 和 '值' （在索引 -1 处）*/
		uint8_t key_index = (uint8_t)lua_tointeger(L, -2);	// 获得索引
		irLens[key_index-1] = (uint8_t)lua_tointeger(L, -1);	// 获得值
		lua_pop(L, 1);	// 将值弹出，键保留在栈中以便下次迭代使用
	}
	// 设置JTAG的TAP个数和IR长度
	if(CmdapJtagConfig(cmdapObj, tapCount, irLens) != ADPT_SUCCESS){
		return luaL_error(L, "CMSIS-DAP JTAG configure failed!");
	}
	return 0;
}

/**
 * transfer配置
 * 1#:Adapter对象
 * 2#:idleCycle
 * 3#:waitRetry
 * 4#:matchRetry
 */
static int luaApi_cmsis_dap_transfer_configure(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	uint8_t idleCycle = (uint8_t)luaL_checkinteger(L, 2);
	uint16_t waitRetry = (uint16_t)luaL_checkinteger(L, 3);
	uint16_t matchRetry = (uint16_t)luaL_checkinteger(L, 4);
	if(CmdapTransferConfigure(cmdapObj, idleCycle, waitRetry, matchRetry) != ADPT_SUCCESS){
		return luaL_error(L, "CMSIS-DAP DAP transfer configure failed!");
	}
	return 0;
}

/**
 * swd配置
 * 1#:adapter对象
 * 2#:cfg 具体看文档
 */
static int luaApi_cmsis_dap_swd_configure(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	uint8_t cfg = (uint8_t)luaL_checkinteger(L, 2);
	if(CmdapSwdConfig(cmdapObj, cfg) != ADPT_SUCCESS){
		return luaL_error(L, "CMSIS-DAP SWD configure failed!");
	}
	return 0;
}

/**
 * 写ABORT寄存器
 * 1#:adapter对象
 * 2#:abort的值
 */
static int luaApi_cmsis_dap_write_abort(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	uint32_t abort = (uint32_t)luaL_checkinteger(L, 2);
	if(CmdapWriteAbort(cmdapObj, abort) != ADPT_SUCCESS){
		return luaL_error(L, "Write Abort register failed!");
	}
	return 0;
}

/**
 * 写TAP索引
 * 1#:adapter对象
 * 2#:tap索引
 */
static int luaApi_cmsis_dap_set_tap_index(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	unsigned int index = (unsigned int)luaL_checkinteger(L, 2);
	if(CmdapSetTapIndex(cmdapObj, index) != ADPT_SUCCESS){
		return luaL_error(L, "Set TAP index failed!");
	}
	return 0;
}

/**
 * CMSIS-DAP垃圾回收函数
 */
static int luaApi_cmsis_dap_gc(lua_State *L){
	Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
	log_trace("[GC] CMSIS-DAP");
	// 销毁CMSIS-DAP对象
	DestroyCmsisDap(&cmdapObj);
	return 0;
}

// 模块静态函数
static const luaL_Reg lib_cmdap_f[] = {
	{"Create", luaApi_cmsis_dap_new},	// 创建CMSIS-DAP对象
	{NULL, NULL}
};

// 初始化Adapter库
int luaopen_cmsis_dap (lua_State *L) {
	lua_createtable(L, 0, 0);	// 预分配索引空间，提高效率
	// 注册常量到模块中
	//LuaApiRegConstant(L, lib_cmdap_const);
	// 将函数注册进去
	luaL_setfuncs(L, lib_cmdap_f, 0);
	return 1;
}

// 模块的面向对象方法
static const luaL_Reg lib_cmdap_oo[] = {
	// 基本函数
	{"SetStatus", luaApi_adapter_set_status},
	{"SetFrequent", luaApi_adapter_set_frequent},
	{"TransferMode", luaApi_adapter_transmission_mode},
	{"Reset", luaApi_adapter_reset},
	// JTAG
	{"JtagExchangeData", luaApi_adapter_jtag_exchange_data},
	{"JtagIdle", luaApi_adapter_jtag_idle_wait},
	{"JtagToState", luaApi_adapter_jtag_status_change},
	{"JtagPins", luaApi_adapter_jtag_pins},
	// DAP相关接口
	{"DapSingleRead", luaApi_adapter_dap_single_read},
	{"DapSingleWrite", luaApi_adapter_dap_single_write},
	{"DapMultiRead", luaApi_adapter_dap_multi_read},
	{"DapMultiWrite", luaApi_adapter_dap_multi_write},

	// CMSIS-DAP 特定接口
	{"Connect", luaApi_cmsis_dap_connect},	// 连接CMSIS-DAP
	//{"Disconnect", NULL},	// TODO 断开连接DAP
	{"TransferConfig", luaApi_cmsis_dap_transfer_configure},
	{"JtagConfig", luaApi_cmsis_dap_jtag_configure},
	{"SwdConfig", luaApi_cmsis_dap_swd_configure},
	{"WriteAbort", luaApi_cmsis_dap_write_abort},
	{"SetTapIndex", luaApi_cmsis_dap_set_tap_index},
	{NULL, NULL}
};

// 注册接口调用
void RegisterApi_CmsisDap(lua_State *L){
	// 创建CMSIS-DAP类型对应的元表
	LuaApiNewTypeMetatable(L, CMDAP_LUA_OBJECT_TYPE, luaApi_cmsis_dap_gc, lib_cmdap_oo);
	luaL_requiref(L, "CMSIS-DAP", luaopen_cmsis_dap, 0);
	lua_pop(L, 1);
}

