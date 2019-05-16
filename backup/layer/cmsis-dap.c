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
#include "debugger/cmsis-dap.h"

#include "layer/load3rd.h"
/**
 * CMSIS-DAP Lua 中间层
 * *新建CMSIS-DAP对象
 * *改变CMSIS-DAP状态
 */

/**
 * 新建CMSIS-DAP对象
 */
static int cmsis_dap_new (lua_State *L) {
	struct cmsis_dap *cmsis_dapObj = lua_newuserdata(L, sizeof(struct cmsis_dap));	// +1
	// 初始化CMSIS-DAP
	if(NewCMSIS_DAP(cmsis_dapObj) == FALSE){
		// 此函数永不返回，惯例用法前面加个return
		return luaL_error(L, "Failed to init CMSIS-DAP Object.");
	}
	// 获得CMSIS-DAP对象的元表
	luaL_getmetatable(L, "obj.Adapter");	// 将元表压栈 +1
	lua_setmetatable(L, -2);	// -1
	return 1;	// 返回压到栈中的返回值个数
}

/**
 * 连接CMSIS-DAP对象
 * 第一个参数是CMSIS-DAP对象
 * 第二个参数是数组，存放PID和VID {{vid,pid}...}
 * 第三个参数是serial number
 */
static int cmsis_dap_connect(lua_State *L){
	luaL_checktype (L, 2, LUA_TTABLE);
	luaL_checkany (L, 3);
	const char *serial = lua_tostring (L, 3);
	// 获得userdata数据
	struct cmsis_dap *cmsis_dapObj = luaL_checkudata(L, 1, "obj.Adapter");
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
	if (Connect_CMSIS_DAP(cmsis_dapObj, vids, pids, serial) == FALSE){
		return luaL_error(L, "Couldn't connect CMSIS-DAP!");
	}
	return 0;
}

/**
 * 配置JTAG
 * 1#:Adapter对象
 * 2#:ir表
 */
static int cmsis_dap_jtag_configure(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
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
	// 设置信息
	if(CMSIS_DAP_JTAG_Configure(adapterObj, tapCount, irLens) == FALSE){
		return luaL_error(L, "CMSIS-DAP JTAG Configure Failed!");
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
static int cmsis_dap_transfer_configure(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint8_t idleCycle = (uint8_t)luaL_checkinteger(L, 2);
	uint16_t waitRetry = (uint16_t)luaL_checkinteger(L, 3);
	uint16_t matchRetry = (uint16_t)luaL_checkinteger(L, 4);
	if(CMSIS_DAP_TransferConfigure(adapterObj, idleCycle, waitRetry, matchRetry) == FALSE){
		return luaL_error(L, "CMSIS-DAP DAP Transfer Configure Failed!");
	}
	return 0;
}

/**
 * swd配置
 * 1#:adapter对象
 * 2#:cfg 具体看文档
 */
static int cmsis_dap_swd_configure(lua_State *L){
	AdapterObject *adapterObj = luaL_checkudata(L, 1, "obj.Adapter");
	uint8_t cfg = (uint8_t)luaL_checkinteger(L, 2);
	if(CMSIS_DAP_SWD_Configure(adapterObj, cfg) == FALSE){
		return luaL_error(L, "CMSIS-DAP SWD Configure Failed!");
	}
	return 0;
}


static const luaL_Reg lib_cmsis_dap_f[] = {
	{"New", cmsis_dap_new},
	{"Connect", cmsis_dap_connect},
	{"jtagConfigure", cmsis_dap_jtag_configure},
	{"TransferConfigure", cmsis_dap_transfer_configure},
	{"swdConfigure", cmsis_dap_swd_configure},
	{NULL, NULL}
};

// 初始化CMSIS-DAP库
int luaopen_cmsis_dap (lua_State *L) {
	luaL_newlib(L, lib_cmsis_dap_f);
	return 1;
}

// require时注册
void register2lua_cmsis_dap(lua_State *L){
	luaL_requiref(L, "CMSIS-DAP", luaopen_cmsis_dap, 0);
	lua_pop(L, 1);	// 清空栈
}
