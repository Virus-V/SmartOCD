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

#include "Adapter/ftdi/ftdi.h"

#include <stdio.h>
#include <stdlib.h>

#include "Component/component.h"
#include "Component/adapter/adapter_api.h"
#include "Library/log/log.h"
#include "Library/lua_api/api.h"
#include "smartocd.h"

#define FTDI_LUA_OBJECT_TYPE "adapter.FTDI"

/**
 * 新建FTDI对象
 */
static int luaApi_ftdi_new(lua_State *L) {
  // 创建一个userdata类型的变量来保存Adapter对象指针
  Adapter *ftdiObj = CAST(Adapter *, lua_newuserdata(L, sizeof(Adapter))); // +1
  // 创建FTDI对象
  *ftdiObj = CreateFtdi();
  if (!*ftdiObj) {
    return luaL_error(L, "Failed to create FTDI Object.");
  }
  // 获得FTDI对象的元表
  luaL_setmetatable(L, FTDI_LUA_OBJECT_TYPE); // 将元表压栈 +1
  return 1;                                   // 返回压到栈中的返回值个数
}

/**
 * 连接FTDI对象
 * 第一个参数是FTDI对象
 * 第二个参数是数组，存放PID和VID {{vid,pid}...}
 * 第三个参数是serial number
 * 第四个参数是Channel
 */
static int luaApi_ftdi_connect(lua_State *L) {
  luaL_checktype(L, 2, LUA_TTABLE);
  const char *serial = NULL;
  if (!lua_isnone(L, 3)) {
    serial = lua_tostring(L, 3);
  }
  // 获得FTDI接口对象
  Adapter ftdiObj = *CAST(Adapter *, luaL_checkudata(L, 1, FTDI_LUA_OBJECT_TYPE));
  // 获得vid和pid的表长度
  int len_VIDs_PIDs = lua_rawlen(L, 2);
  luaL_argcheck(L, len_VIDs_PIDs != 0, 2, "The length of the vid and pid parameter arrays is wrong.");
  int channel = luaL_checkinteger(L, 4);

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
  // 连接FTDI，返回结果
  if (ConnectFtdi(ftdiObj, vids, pids, serial, channel) != ADPT_SUCCESS) {
    return luaL_error(L, "Couldn't connect FTDI!");
  }
  return 0;
}

/**
 * FTDI垃圾回收函数
 */
static int luaApi_ftdi_gc(lua_State *L) {
  Adapter ftdiObj = *CAST(Adapter *, luaL_checkudata(L, 1, FTDI_LUA_OBJECT_TYPE));
  log_trace("[GC] FTDI");
  // 销毁FTDI对象
  DestroyFtdi(&ftdiObj);
  return 0;
}

// 模块静态函数
static const luaL_Reg lib_ftdi_f[] = {{"Create", luaApi_ftdi_new}, // 创建FTDI对象
                                      {NULL, NULL}};

// 模块的面向对象方法
static const luaL_Reg lib_ftdi_oo[] = {
    // FTDI 特定接口
    {"Connect", luaApi_ftdi_connect}, // 连接FTDI
    //{"Disconnect", NULL},	// TODO 断开连接DAP
    {NULL, NULL}};

// 初始化Adapter库
static int luaopen_ftdi(lua_State *L) {
  // 创建FTDI类型对应的元表
  LuaApi_create_new_type(L, FTDI_LUA_OBJECT_TYPE, luaApi_ftdi_gc, lib_ftdi_oo, ADAPTER_LUA_OBJECT_TYPE);
  luaL_newlib(L, lib_ftdi_f);
  return 1;
}

// 注册接口调用
static int RegisterApi_Ftdi(lua_State *L, void *opaque) {
  luaL_requiref(L, "FTDI", luaopen_ftdi, 0);
  lua_pop(L, 1);

  return 0;
}

COMPONENT_INIT(FTDI, RegisterApi_Ftdi, NULL, COM_ADAPTER, 2);
