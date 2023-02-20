/**
 * Copyright (c) 2023, Virus.V <virusv@live.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of SmartOCD nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * Copyright 2023 Virus.V <virusv@live.com>
*/

#include "Adapter/cmsis-dap/cmsis-dap.h"

#include <stdio.h>
#include <stdlib.h>

#include "Component/component.h"
#include "Component/adapter/adapter_api.h"
#include "Library/log/log.h"
#include "Library/lua_api/api.h"
#include "smartocd.h"

// 注意!!!!所有Adapter对象的metatable都要以 "adapter." 开头!!!!
#define CMDAP_LUA_OBJECT_TYPE "adapter.CMSIS-DAP"

/**
 * 新建CMSIS-DAP对象
 */
static int luaApi_cmsis_dap_new(lua_State *L) {
  // 创建一个userdata类型的变量来保存Adapter对象指针
  Adapter *cmdapObj = CAST(Adapter *, lua_newuserdata(L, sizeof(Adapter))); // +1
  // 创建CMSIS-DAP对象
  *cmdapObj = CreateCmsisDap();
  if (!*cmdapObj) {
    return luaL_error(L, "Failed to create CMSIS-DAP Object.");
  }
  // 获得CMSIS-DAP对象的元表
  luaL_setmetatable(L, CMDAP_LUA_OBJECT_TYPE); // 将元表压栈 +1
  return 1;                                    // 返回压到栈中的返回值个数
}

/**
 * 连接CMSIS-DAP对象
 * 第一个参数是CMSIS-DAP对象
 * 第二个参数是数组，存放PID和VID {{vid,pid}...}
 * 第三个参数是serial number
 */
static int luaApi_cmsis_dap_connect(lua_State *L) {
  luaL_checktype(L, 2, LUA_TTABLE);
  const char *serial = NULL;
  if (!lua_isnone(L, 3)) {
    serial = lua_tostring(L, 3);
  }
  // 获得CMSIS-DAP接口对象
  Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
  // 获得vid和pid的表长度
  int len_VIDs_PIDs = lua_rawlen(L, 2);
  luaL_argcheck(L, len_VIDs_PIDs != 0, 2,
                "The length of the vid and pid parameter arrays is wrong.");
  // 分配空间
  uint16_t *vid_pid_buff = lua_newuserdata(L, (len_VIDs_PIDs << 1) * sizeof(uint16_t));
  uint16_t *vids = vid_pid_buff;
  uint16_t *pids = vid_pid_buff + len_VIDs_PIDs;
  // vid pid表在栈中的索引为2
  lua_pushnil(L);
  while (lua_next(L, 2) != 0) {
    /* 使用 '键' （在索引 -2 处） 和 '值' （在索引 -1 处）*/
    int vid_pid_arr = lua_gettop(L);                        // 返回栈顶元素的索引值
    int key_index = (int)lua_tointeger(L, vid_pid_arr - 1); // 获得当前key
    if (lua_rawgeti(L, vid_pid_arr, 1) != LUA_TNUMBER) {    // 检查数据类型
      return luaL_error(L, "VID is not a number.");
    }
    // lua的数组下标以1开始
    vids[key_index - 1] = (uint16_t)lua_tointeger(L, -1); // 获得当前vid
    if (lua_rawgeti(L, vid_pid_arr, 2) != LUA_TNUMBER) {  // 检查数据类型
      return luaL_error(L, "PID is not a number.");
    }
    pids[key_index - 1] = (uint16_t)lua_tointeger(L, -1); // 获得当前pid
    lua_pop(L, 3);                                        // 将3个值弹出，键保留在栈中以便下次迭代使用
  }
  // 连接CMSIS-DAP，返回结果
  if (ConnectCmsisDap(cmdapObj, vids, pids, serial) != ADPT_SUCCESS) {
    return luaL_error(L, "Couldn't connect CMSIS-DAP!");
  }
  return 0;
}

/**
 * 配置JTAG
 * 1#:Adapter对象
 * 2#:ir表
 */
static int luaApi_cmsis_dap_jtag_configure(lua_State *L) {
  Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
  luaL_checktype(L, 2, LUA_TTABLE);
  // 获得JTAG扫描链中TAP个数
  uint8_t tapCount = (uint8_t)lua_rawlen(L, 2);
  // 开辟存放IR Len的空间，会把这个空间当做完全用户数据压栈
  uint8_t *irLens = lua_newuserdata(L, tapCount * sizeof(uint8_t));
  lua_pushnil(L);
  // XXX 防止表过大溢出
  while (lua_next(L, 2) != 0) {
    /* 使用 '键' （在索引 -2 处） 和 '值' （在索引 -1 处）*/
    uint8_t key_index = (uint8_t)lua_tointeger(L, -2);     // 获得索引
    irLens[key_index - 1] = (uint8_t)lua_tointeger(L, -1); // 获得值
    lua_pop(L, 1);                                         // 将值弹出，键保留在栈中以便下次迭代使用
  }
  // 设置JTAG的TAP个数和IR长度
  if (CmdapJtagConfig(cmdapObj, tapCount, irLens) != ADPT_SUCCESS) {
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
static int luaApi_cmsis_dap_transfer_configure(lua_State *L) {
  Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
  uint8_t idleCycle = (uint8_t)luaL_checkinteger(L, 2);
  uint16_t waitRetry = (uint16_t)luaL_checkinteger(L, 3);
  uint16_t matchRetry = (uint16_t)luaL_checkinteger(L, 4);
  if (CmdapTransferConfigure(cmdapObj, idleCycle, waitRetry, matchRetry) != ADPT_SUCCESS) {
    return luaL_error(L, "CMSIS-DAP DAP transfer configure failed!");
  }
  return 0;
}

/**
 * swd配置
 * 1#:adapter对象
 * 2#:cfg 具体看文档
 */
static int luaApi_cmsis_dap_swd_configure(lua_State *L) {
  Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
  uint8_t cfg = (uint8_t)luaL_checkinteger(L, 2);
  if (CmdapSwdConfig(cmdapObj, cfg) != ADPT_SUCCESS) {
    return luaL_error(L, "CMSIS-DAP SWD configure failed!");
  }
  return 0;
}

/**
 * 写ABORT寄存器
 * 1#:adapter对象
 * 2#:abort的值
 */
static int luaApi_cmsis_dap_write_abort(lua_State *L) {
  Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
  uint32_t abort = (uint32_t)luaL_checkinteger(L, 2);
  if (CmdapWriteAbort(cmdapObj, abort) != ADPT_SUCCESS) {
    return luaL_error(L, "Write Abort register failed!");
  }
  return 0;
}

/**
 * 写TAP索引
 * 1#:adapter对象
 * 2#:tap索引
 */
static int luaApi_cmsis_dap_set_tap_index(lua_State *L) {
  Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
  unsigned int index = (unsigned int)luaL_checkinteger(L, 2);
  if (CmdapSetTapIndex(cmdapObj, index) != ADPT_SUCCESS) {
    return luaL_error(L, "Set TAP index failed!");
  }
  return 0;
}

/**
 * CMSIS-DAP垃圾回收函数
 */
static int luaApi_cmsis_dap_gc(lua_State *L) {
  Adapter cmdapObj = *CAST(Adapter *, luaL_checkudata(L, 1, CMDAP_LUA_OBJECT_TYPE));
  log_trace("[GC] CMSIS-DAP");
  // 销毁CMSIS-DAP对象
  DestroyCmsisDap(&cmdapObj);
  return 0;
}

// 模块静态函数
static const luaL_Reg lib_cmdap_f[] = {{"Create", luaApi_cmsis_dap_new}, // 创建CMSIS-DAP对象
                                       {NULL, NULL}};

// 模块的面向对象方法
static const luaL_Reg lib_cmdap_oo[] = {
    // CMSIS-DAP 特定接口
    {"Connect", luaApi_cmsis_dap_connect}, // 连接CMSIS-DAP
    //{"Disconnect", NULL},	// TODO 断开连接DAP
    {"TransferConfig", luaApi_cmsis_dap_transfer_configure},
    {"JtagConfig", luaApi_cmsis_dap_jtag_configure},
    {"SwdConfig", luaApi_cmsis_dap_swd_configure},
    {"WriteAbort", luaApi_cmsis_dap_write_abort},
    {"SetTapIndex", luaApi_cmsis_dap_set_tap_index},
    {NULL, NULL}};

// 初始化Adapter库
static int luaopen_cmsis_dap(lua_State *L) {
  // 创建CMSIS-DAP类型对应的元表
  LuaApi_create_new_type(L, CMDAP_LUA_OBJECT_TYPE, luaApi_cmsis_dap_gc, lib_cmdap_oo, ADAPTER_LUA_OBJECT_TYPE);
  luaL_newlib(L, lib_cmdap_f);
  return 1;
}

// 注册接口调用
static int RegisterApi_CmsisDap(lua_State *L, void *opaque) {
  luaL_requiref(L, "CMSIS-DAP", luaopen_cmsis_dap, 0);
  lua_pop(L, 1);

  return 0;
}

COMPONENT_INIT(CMSIS_DAP, RegisterApi_CmsisDap, NULL, COM_ADAPTER, 1);
