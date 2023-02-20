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

#include "Component/ADI/ADIv5.h"

#include <stdio.h>
#include <stdlib.h>

#include "Component/adapter/adapter_api.h"
#include "Component/component.h"
#include "Library/log/log.h"
#include "Library/lua_api/api.h"
#include "smartocd.h"

#define ADIV5_LUA_OBJECT_TYPE "arch.ARM.ADIv5"
#define ADIV5_AP_MEM_LUA_OBJECT_TYPE "arch.ARM.ADIv5.AccessPort.Memory"
#define ADIV5_AP_JTAG_LUA_OBJECT_TYPE "arch.ARM.ADIv5.AccessPort.Jtag"

/**
 * 创建DAP对象
 * 参数:
 * 1# skill对象
 * 返回值:
 * 1# DAP对象
 * 失败抛出错误
 */
static int luaApi_adiv5_create_dap(lua_State *L) {
  void *udata = LuaApi_check_object_type(L, 1, SKILL_DAP_LUA_OBJECT_TYPE);
  if (udata == NULL) {
    return luaL_error(L, "Not a vailed skill object!");
  }
  DapSkill skillObj = *CAST(DapSkill *, udata);

  DAP *dapObj = lua_newuserdatauv(L, sizeof(DAP), 1); // +1
  *dapObj = ADIv5_CreateDap(skillObj);
  if (*dapObj == NULL) {
    return luaL_error(L, "Failed to create ADIv5 DAP object.");
  }

  luaL_setmetatable(L, ADIV5_LUA_OBJECT_TYPE);

  // 引用DapSkill对象
  lua_pushvalue(L, 1);         // +1
  lua_setiuservalue(L, -2, 1); // -1

  return 1; // 返回压到栈中的返回值个数
}

/**
 * 搜素AP,并返回AP对象
 * 参数:
 * 	1# DAP对象
 * 	2# AP类型
 * 	3# 总线类型(Optional)
 * 返回:
 * 	1# AP对象
 */
static int luaApi_adiv5_find_access_port(lua_State *L) {
  DAP dapObj = *CAST(DAP *, luaL_checkudata(L, 1, ADIV5_LUA_OBJECT_TYPE));
  enum AccessPortType type = CAST(enum AccessPortType, luaL_checkinteger(L, 2));
  enum busType bus = CAST(enum busType, luaL_optinteger(L, 3, 0));

  // 创建AP对象
  AccessPort *apObj = lua_newuserdatauv(L, sizeof(AccessPort), 1); // +1
  if (dapObj->FindAccessPort(dapObj, type, bus, apObj) != ADI_SUCCESS) {
    return luaL_error(L, "Failed to find the AP.");
  }

  // 根据不同的类型,选择不同的元表
  if (type == AccessPort_Memory) {
    luaL_setmetatable(L, ADIV5_AP_MEM_LUA_OBJECT_TYPE); // 0
  } else if (type == AccessPort_JTAG) {
    // TODO ...
  }

  // 增加DAP的引用
  lua_pushvalue(L, 1);
  lua_setiuservalue(L, -2, 1);

  return 1;
}

/**
 * 返回当前AP的rom table
 * 1#：skill对象
 * 返回：
 * 1#: rom table
 */
static int luaApi_adiv5_ap_mem_rom_table(lua_State *L) {
  AccessPort apObj = *CAST(AccessPort *, luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE));
  if (apObj->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  lua_pushinteger(L, apObj->Interface.Memory.RomTableBase);
  return 1;
}

/**
 * 读写CSW寄存器
 * 1#：skill对象
 * 2#：数据（optional）
 * 当只有1个参数时，执行读取动作，当有2个参数时，执行写入动作
 * 返回：空或者数据
 * 1#：数据
 */
static int luaApi_adiv5_ap_mem_csw(lua_State *L) {
  AccessPort apObj = *CAST(AccessPort *, luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE));
  uint32_t data = 0;
  if (apObj->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  if (lua_isnone(L, 2)) { // 读CSW
    if (apObj->Interface.Memory.ReadCSW(apObj, &data) != ADI_SUCCESS) {
      return luaL_error(L, "Read CSW register failed!");
    }
    lua_pushinteger(L, data);
    return 1;
  } else { // 写CSW
    data = (uint32_t)luaL_checkinteger(L, 2);
    if (apObj->Interface.Memory.WriteCSW(apObj, data) != ADI_SUCCESS) {
      return luaL_error(L, "Write CSW register failed!");
    }
    return 0;
  }
}

/**
 * 终止当前AP传输
 * 1#：skill对象
 * 返回：空
 */
static int luaApi_adiv5_ap_mem_abort(lua_State *L) {
  AccessPort apObj = *CAST(AccessPort *, luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE));
  if (apObj->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  if (apObj->Interface.Memory.Abort(apObj) != ADI_SUCCESS) {
    return luaL_error(L, "Abort failed!");
  }
  return 0;
}

/**
 * 读写8位数据
 * 1#：skill对象
 * 2#：addr：地址64位
 * 3#：数据（optional）
 * 当只有两个参数时，执行读取动作，当有三个参数时，执行写入动作
 * 返回：空或者数据
 * 1#：数据
 */
static int luaApi_adiv5_ap_mem_rw_8(lua_State *L) {
  AccessPort apObj = *CAST(AccessPort *, luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE));
  uint64_t addr = luaL_checkinteger(L, 2);
  uint8_t data = 0;
  if (apObj->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  if (lua_isnone(L, 3)) { // 读内存
    if (apObj->Interface.Memory.Read8(apObj, addr, &data) != ADI_SUCCESS) {
      return luaL_error(L, "Read byte memory %p failed!", addr);
    }
    lua_pushinteger(L, data);
    return 1;
  } else { // 写内存
    data = (uint8_t)luaL_checkinteger(L, 3);
    if (apObj->Interface.Memory.Write8(apObj, addr, data) != ADI_SUCCESS) {
      return luaL_error(L, "Write byte memory %p failed!", addr);
    }
    return 0;
  }
}

static int luaApi_adiv5_ap_mem_rw_16(lua_State *L) {
  AccessPort apObj = *CAST(AccessPort *, luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE));
  uint64_t addr = luaL_checkinteger(L, 2);
  uint16_t data = 0;
  if (apObj->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  if (lua_isnone(L, 3)) { // 读内存
    if (apObj->Interface.Memory.Read16(apObj, addr, &data) != ADI_SUCCESS) {
      return luaL_error(L, "Read halfword memory %p failed!", addr);
    }
    lua_pushinteger(L, data);
    return 1;
  } else { // 写内存
    data = (uint16_t)luaL_checkinteger(L, 3);
    if (apObj->Interface.Memory.Write16(apObj, addr, data) != ADI_SUCCESS) {
      return luaL_error(L, "Write halfword memory %p failed!", addr);
    }
    return 0;
  }
}

static int luaApi_adiv5_ap_mem_rw_32(lua_State *L) {
  AccessPort apObj = *CAST(AccessPort *, luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE));
  uint64_t addr = luaL_checkinteger(L, 2);
  uint32_t data;
  if (apObj->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  if (lua_isnone(L, 3)) { // 读内存
    if (apObj->Interface.Memory.Read32(apObj, addr, &data) != ADI_SUCCESS) {
      return luaL_error(L, "Read word memory %p failed!", addr);
    }
    lua_pushinteger(L, data);
    return 1;
  } else { // 写内存
    data = (uint32_t)luaL_checkinteger(L, 3);
    if (apObj->Interface.Memory.Write32(apObj, addr, data) != ADI_SUCCESS) {
      return luaL_error(L, "Write word memory %p failed!", addr);
    }
    return 0;
  }
}

/**
 * 读取内存块
 * 1#:skill对象
 * 2#:要读取的地址
 * 3#:地址自增模式
 * 4#:单次传输数据大小
 * 5#:读取多少次
 * 返回：
 * 1#:读取的数据 字符串形式
 */
static int luaApi_adiv5_ap_read_mem_block(lua_State *L) {
  AccessPort apObj = *CAST(AccessPort *, luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE));
  uint64_t addr = luaL_checkinteger(L, 2);
  int addrIncMode = (int)luaL_checkinteger(L, 3);
  int dataSize = (int)luaL_checkinteger(L, 4);
  int transCnt = (int)luaL_checkinteger(L, 5);
  if (apObj->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  uint8_t *buff = (uint8_t *)lua_newuserdata(L, transCnt * sizeof(uint32_t));
  if (apObj->Interface.Memory.BlockRead(apObj, addr, addrIncMode, dataSize, transCnt,
                                        buff) != ADI_SUCCESS) {
    return luaL_error(L, "Block read failed!");
  }
  lua_pushlstring(L, CAST(const char *, buff), transCnt * sizeof(uint32_t));
  return 1;
}

/**
 * 写入内存块
 * 1#:skill对象
 * 2#:要读取的地址
 * 3#:地址自增模式
 * 4#:单次传输数据大小
 * 5#:要写的数据（字符串）
 */
static int luaApi_adiv5_ap_write_mem_block(lua_State *L) {
  AccessPort apObj = *CAST(AccessPort *, luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE));
  uint64_t addr = luaL_checkinteger(L, 2);
  int addrIncMode = (int)luaL_checkinteger(L, 3);
  int dataSize = (int)luaL_checkinteger(L, 4);
  size_t transCnt; // 注意size_t在在64位环境下是8字节，int在64位下是4字节
  uint8_t *buff = (uint8_t *)lua_tolstring(L, 5, &transCnt);
  if (apObj->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  if (transCnt & 0x3) {
    return luaL_error(L, "The length of the data to be written is not a multiple of the word.");
  }
  if (apObj->Interface.Memory.BlockWrite(apObj, addr, addrIncMode, dataSize,
                                         (int)transCnt >> 2, buff) != ADI_SUCCESS) {
    return luaL_error(L, "Block write failed!");
  }
  return 0;
}

/**
 * 读取Component ID 和 Peripheral ID
 * 1#：skill对象
 * 2#：base：Component地址 64位
 * 返回：
 * 1#：cid
 * 2#：pid 64位
 */
static int luaApi_adiv5_ap_get_pid_cid(lua_State *L) {
  AccessPort apObj = *CAST(AccessPort *, luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE));
  uint64_t base = luaL_checkinteger(L, 2);
  uint32_t cid;
  uint64_t pid;
  if (ADIv5_ReadCidPid(apObj, base, &cid, &pid) != ADI_SUCCESS) {
    return luaL_error(L, "Read Component ID and Peripheral ID Failed!");
  }
  lua_pushinteger(L, cid);
  lua_pushinteger(L, pid);
  return 2;
}

/**
 * ADIv5垃圾回收函数
 */
static int luaApi_adiv5_gc(lua_State *L) {
  DAP *dapObj = CAST(DAP *, luaL_checkudata(L, 1, ADIV5_LUA_OBJECT_TYPE));
  log_trace("[GC] ADIv5");
  // 销毁DAP对象
  ADIv5_DestoryDap(dapObj);

  return 0;
}

// 模块静态函数
static const luaL_Reg lib_adiv5_f[] = {{"Create", luaApi_adiv5_create_dap}, {NULL, NULL}};

// 模块常量
static const luaApi_regConst lib_adiv5_const[] = {
    // Access Port 类型
    {"AP_Memory", AccessPort_Memory},
    {"AP_JTAG", AccessPort_JTAG},
    // 总线类型
    {"Bus_AMBA_AHB", Bus_AMBA_AHB},
    {"Bus_AMBA_APB", Bus_AMBA_APB},
    {"Bus_AMBA_AXI", Bus_AMBA_AXI},
    // 地址自增模式
    {"AddrInc_Off", AddrInc_Off},
    {"AddrInc_Single", AddrInc_Single},
    {"AddrInc_Packed", AddrInc_Packed},
    // 每次总线请求的数据长度
    {"DataSize_8", DataSize_8},
    {"DataSize_16", DataSize_16},
    {"DataSize_32", DataSize_32},
    {"DataSize_64", DataSize_64},
    {"DataSize_128", DataSize_128},
    {"DataSize_256", DataSize_256},
    {NULL, 0}};

// 模块的面向对象方法
static const luaL_Reg lib_adiv5_oo[] = {
    // 基本函数
    {"FindAccessPort", luaApi_adiv5_find_access_port},
    {NULL, NULL}};

// 模块的面向对象方法
static const luaL_Reg lib_access_port_oo[] = {
    // 基本函数
    {"RomTable", luaApi_adiv5_ap_mem_rom_table},
    {"CSW", luaApi_adiv5_ap_mem_csw},
    {"Abort", luaApi_adiv5_ap_mem_abort},
    {"GetCidPid", luaApi_adiv5_ap_get_pid_cid},

    {"Memory8", luaApi_adiv5_ap_mem_rw_8},
    {"Memory16", luaApi_adiv5_ap_mem_rw_16},
    {"Memory32", luaApi_adiv5_ap_mem_rw_32},
    // TODO {"Memory64", luaApi_adiv5_find_access_port},

    {"BlockRead", luaApi_adiv5_ap_read_mem_block},
    {"BlockWrite", luaApi_adiv5_ap_write_mem_block},
    {NULL, NULL}};

// 初始化ADIv5库
int luaopen_adiv5(lua_State *L) {
  LuaApi_create_new_type(L, ADIV5_LUA_OBJECT_TYPE, luaApi_adiv5_gc, lib_adiv5_oo, NULL);
  LuaApi_create_new_type(L, ADIV5_AP_MEM_LUA_OBJECT_TYPE, NULL, lib_access_port_oo, NULL);

  lua_createtable(L, 0, sizeof(lib_adiv5_const) / sizeof(lib_adiv5_const[0]));
  // 注册常量到模块中
  LuaApi_reg_constant(L, lib_adiv5_const);
  // 将函数注册进去
  luaL_setfuncs(L, lib_adiv5_f, 0);
  return 1;
}

// 注册接口调用
static int RegisterApi_ADIv5(lua_State *L, void *opaque) {
  luaL_requiref(L, "ADIv5", luaopen_adiv5, 0);
  lua_pop(L, 1);

  return 0;
}

COMPONENT_INIT(ADIv5, RegisterApi_ADIv5, NULL, 1 << 16, 0);
