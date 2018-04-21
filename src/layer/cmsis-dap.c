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

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

/**
 * CMSIS-DAP Lua 中间层
 * *新建CMSIS-DAP对象
 * *改变CMSIS-DAP状态
 */

/**
 * 释放CMSIS-DAP对象
 */
static int cmsis_dap_free(lua_State *L){
	// 获得对象
	struct cmsis_dap *cmsis_dapObj = lua_touserdata(L, 1);
	FreeCMSIS_DAP(cmsis_dapObj);
	return 0;
}

/**
 * 新建CMSIS-DAP对象
 */
static int cmsis_dap_new (lua_State *L) {
	struct cmsis_dap *cmsis_dapObj = lua_newuserdata(L, sizeof(struct cmsis_dap));
	// 清零空间
	memset(cmsis_dapObj, 0x0, sizeof(struct cmsis_dap));
	// 初始化CMSIS-DAP
	if( NewCMSIS_DAP(cmsis_dapObj) == FALSE){
		// 此函数永不返回，惯例用法前面加个return
		return luaL_error(L, "Failed to new CMSIS-DAP Object.");
	}
	// 设置metatable，同时指定__gc元素
	lua_createtable (L, 0, 1);	// 创建元表
	lua_pushcfunction(L, cmsis_dap_free);	// 创建元方法
	lua_setfield (L, -2, "__gc");	// 创建gc
	lua_setmetatable(L, -2);	// 设置成元表
	return 1;	// 返回压到栈中的返回值个数
}

/**
 * 连接CMSIS-DAP对象
 * 第一个参数是CMSIS-DAP对象
 * 第二个参数是数组，存放PID和VID {{vid,pid}...}
 * 第三个参数是serial number
 * 返回值：成功 TRUE，失败 FALSE
 */
static int cmsis_dap_connect(lua_State *L){
	luaL_checktype (L, 1, LUA_TUSERDATA);
	luaL_checktype (L, 2, LUA_TTABLE);
	luaL_checkany (L, 3);
	const char *serial = lua_tostring (L, 3);
	// 获得userdata数据
	struct cmsis_dap *cmsis_dapObj = lua_touserdata(L, 1);
	// 检查对象
	luaL_argcheck (L, cmsis_dapObj != NULL, 1, "The CMSIS-DAP object points to a null pointer.");
	// 获得vid和pid的表长度
	int len_VIDs_PIDs = lua_rawlen(L, 2);
	if(len_VIDs_PIDs == 0){
		return luaL_error(L, "The length of the vid and pid parameter arrays is wrong.");
	}
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
	lua_pushboolean (L, ConnectCMSIS_DAP(cmsis_dapObj, vids, pids, serial));
	return 1;	// 1个参数，布尔值
}

static const luaL_Reg cmsis_dap_lib[] = {
	{"new", cmsis_dap_new},
	{"connect", cmsis_dap_connect},
	{NULL, NULL}
};

// 初始化CMSIS-DAP库
int luaopen_cmsis_dap (lua_State *L) {
  luaL_newlib(L, cmsis_dap_lib);
  return 1;
}

// require时注册
void register2lua_cmsis_dap(lua_State *L){
	luaL_requiref(L, "cmsis_dap", luaopen_cmsis_dap, 0);
	// 将栈中model的副本弹出
	lua_pop(L, 1);
}
