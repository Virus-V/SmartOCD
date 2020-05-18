/*
 * ADIv5.c
 *
 *  Created on: 2019-6-2
 *      Author: virusv
 */

#include "Component/ADI/include/ADIv5.h"

#include <stdio.h>
#include <stdlib.h>

#include "Component/lua_api/api.h"
#include "Library/log/log.h"
#include "smart_ocd.h"

#define ADIV5_LUA_OBJECT_TYPE "arch.ARM.ADIv5"
#define ADIV5_AP_MEM_LUA_OBJECT_TYPE "arch.ARM.ADIv5.AccessPort.Memory"
#define ADIV5_AP_JTAG_LUA_OBJECT_TYPE "arch.ARM.ADIv5.AccessPort.Jtag"

struct luaApi_dap {
  int adapterRef;  // adapter的Lua对象引用
  DAP dap;         // DAP 对象
};

struct luaApi_accessPort {
  int reference;  // lua_dap对象的reference
  AccessPort ap;  // AP对象
};

/**
 * 检查是否是Adapter对象
 */
static void *checkAdapter(lua_State *L, int ud) {
  void *p = lua_touserdata(L, ud);
  if (p != NULL) {                 /* value is a userdata? */
    if (lua_getmetatable(L, ud)) { /* does it have a metatable? */
      if (lua_getfield(L, -1, "__name") !=
          LUA_TSTRING) {  //获得metatable的__name字段
        lua_pop(L, 2);
        return NULL;
      }
      const char *typeName = lua_tostring(L, -1);  // 获得类型字符串
      if (strncmp(typeName, "adapter", 7) !=
          0) {  // 判断是否是Adapter类型的metatable
        lua_pop(L, 2);
        return NULL;
      }
      luaL_getmetatable(L, typeName); /* get correct metatable */
      if (!lua_rawequal(L, -1, -3))   /* not the same? */
        p = NULL;    /* value is a userdata with wrong metatable */
      lua_pop(L, 3); /* remove both metatables */
      return p;
    }
  }
  return NULL; /* value is not a userdata with a metatable */
}

/**
 * 创建DAP对象
 * 参数:
 * 1# Adapter对象
 * 返回值:
 * 1# DAP对象
 * 失败抛出错误
 */
static int luaApi_adiv5_create_dap(lua_State *L) {
  void *udata = checkAdapter(L, 1);
  if (udata == NULL) {
    return luaL_error(L, "Not a vailed Adapter object!");
  }
  Adapter adapterObj = *CAST(Adapter *, udata);

  struct luaApi_dap *luaDap =
      lua_newuserdata(L, sizeof(struct luaApi_dap));  // +1
  luaDap->dap = ADIv5_CreateDap(adapterObj);
  if (luaDap->dap == NULL) {
    return luaL_error(L, "Failed to create ADIv5 DAP object.");
  }
  luaL_setmetatable(L, ADIV5_LUA_OBJECT_TYPE);
  // adapter对象增加引用
  lua_pushvalue(L, -1);
  luaDap->adapterRef = luaL_ref(L, LUA_REGISTRYINDEX);
  return 1;  // 返回压到栈中的返回值个数
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
  struct luaApi_dap *dapObj =
      CAST(struct luaApi_dap *, luaL_checkudata(L, 1, ADIV5_LUA_OBJECT_TYPE));
  int type = luaL_checkinteger(L, 2);
  int bus = luaL_optinteger(L, 3, 0);
  // 创建AP对象
  struct luaApi_accessPort *luaAp =
      lua_newuserdata(L, sizeof(struct luaApi_accessPort));  // +1
  if (dapObj->dap->FindAccessPort(dapObj->dap, (enum AccessPortType)type,
                                  (enum busType)bus,
                                  &luaAp->ap) != ADI_SUCCESS) {
    return luaL_error(L, "Failed to find the AP.");
  }
  // 根据不同的类型,选择不同的元表
  if (type == AccessPort_Memory) {
    luaL_setmetatable(L, ADIV5_AP_MEM_LUA_OBJECT_TYPE);  // 添加元表
  } else if (type == AccessPort_JTAG) {
    // TODO ...
  }
  // 增加DAP的引用
  lua_pushvalue(L, 1);
  luaAp->reference = luaL_ref(L, LUA_REGISTRYINDEX);
  return 1;
}

/**
 * 返回当前AP的rom table
 * 1#：Adapter对象
 * 返回：
 * 1#: rom table
 */
static int luaApi_adiv5_ap_mem_rom_table(lua_State *L) {
  struct luaApi_accessPort *luaApObj =
      luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE);
  if (luaApObj->ap->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  lua_pushinteger(L, luaApObj->ap->Interface.Memory.RomTableBase);
  return 1;
}

/**
 * 读写CSW寄存器
 * 1#：Adapter对象
 * 2#：数据（optional）
 * 当只有1个参数时，执行读取动作，当有2个参数时，执行写入动作
 * 返回：空或者数据
 * 1#：数据
 */
static int luaApi_adiv5_ap_mem_csw(lua_State *L) {
  struct luaApi_accessPort *luaApObj =
      luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE);
  uint32_t data = 0;
  if (luaApObj->ap->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  if (lua_isnone(L, 2)) {  // 读CSW
    if (luaApObj->ap->Interface.Memory.ReadCSW(luaApObj->ap, &data) !=
        ADI_SUCCESS) {
      return luaL_error(L, "Read CSW register failed!");
    }
    lua_pushinteger(L, data);
    return 1;
  } else {  // 写CSW
    data = (uint32_t)luaL_checkinteger(L, 2);
    if (luaApObj->ap->Interface.Memory.WriteCSW(luaApObj->ap, data) !=
        ADI_SUCCESS) {
      return luaL_error(L, "Write CSW register failed!");
    }
    return 0;
  }
}

/**
 * 终止当前AP传输
 * 1#：Adapter对象
 * 返回：空
 */
static int luaApi_adiv5_ap_mem_abort(lua_State *L) {
  struct luaApi_accessPort *luaApObj =
      luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE);
  if (luaApObj->ap->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  if (luaApObj->ap->Interface.Memory.Abort(luaApObj->ap) != ADI_SUCCESS) {
    return luaL_error(L, "Abort failed!");
  }
  return 0;
}

/**
 * 读写8位数据
 * 1#：Adapter对象
 * 2#：addr：地址64位
 * 3#：数据（optional）
 * 当只有两个参数时，执行读取动作，当有三个参数时，执行写入动作
 * 返回：空或者数据
 * 1#：数据
 */
static int luaApi_adiv5_ap_mem_rw_8(lua_State *L) {
  struct luaApi_accessPort *luaApObj =
      luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE);
  uint64_t addr = luaL_checkinteger(L, 2);
  uint8_t data = 0;
  if (luaApObj->ap->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  if (lua_isnone(L, 3)) {  // 读内存
    if (luaApObj->ap->Interface.Memory.Read8(luaApObj->ap, addr, &data) !=
        ADI_SUCCESS) {
      return luaL_error(L, "Read byte memory %p failed!", addr);
    }
    lua_pushinteger(L, data);
    return 1;
  } else {  // 写内存
    data = (uint8_t)luaL_checkinteger(L, 3);
    if (luaApObj->ap->Interface.Memory.Write8(luaApObj->ap, addr, data) !=
        ADI_SUCCESS) {
      return luaL_error(L, "Write byte memory %p failed!", addr);
    }
    return 0;
  }
}

static int luaApi_adiv5_ap_mem_rw_16(lua_State *L) {
  struct luaApi_accessPort *luaApObj =
      luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE);
  uint64_t addr = luaL_checkinteger(L, 2);
  uint16_t data = 0;
  if (luaApObj->ap->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  if (lua_isnone(L, 3)) {  // 读内存
    if (luaApObj->ap->Interface.Memory.Read16(luaApObj->ap, addr, &data) !=
        ADI_SUCCESS) {
      return luaL_error(L, "Read halfword memory %p failed!", addr);
    }
    lua_pushinteger(L, data);
    return 1;
  } else {  // 写内存
    data = (uint16_t)luaL_checkinteger(L, 3);
    if (luaApObj->ap->Interface.Memory.Write16(luaApObj->ap, addr, data) !=
        ADI_SUCCESS) {
      return luaL_error(L, "Write halfword memory %p failed!", addr);
    }
    return 0;
  }
}

static int luaApi_adiv5_ap_mem_rw_32(lua_State *L) {
  struct luaApi_accessPort *luaApObj =
      luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE);
  uint64_t addr = luaL_checkinteger(L, 2);
  uint32_t data;
  if (luaApObj->ap->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  if (lua_isnone(L, 3)) {  // 读内存
    if (luaApObj->ap->Interface.Memory.Read32(luaApObj->ap, addr, &data) !=
        ADI_SUCCESS) {
      return luaL_error(L, "Read word memory %p failed!", addr);
    }
    lua_pushinteger(L, data);
    return 1;
  } else {  // 写内存
    data = (uint32_t)luaL_checkinteger(L, 3);
    if (luaApObj->ap->Interface.Memory.Write32(luaApObj->ap, addr, data) !=
        ADI_SUCCESS) {
      return luaL_error(L, "Write word memory %p failed!", addr);
    }
    return 0;
  }
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
static int luaApi_adiv5_ap_read_mem_block(lua_State *L) {
  struct luaApi_accessPort *luaApObj =
      luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE);
  uint64_t addr = luaL_checkinteger(L, 2);
  int addrIncMode = (int)luaL_checkinteger(L, 3);
  int dataSize = (int)luaL_checkinteger(L, 4);
  int transCnt = (int)luaL_checkinteger(L, 5);
  if (luaApObj->ap->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  uint8_t *buff = (uint8_t *)lua_newuserdata(L, transCnt * sizeof(uint32_t));
  if (luaApObj->ap->Interface.Memory.BlockRead(luaApObj->ap, addr, addrIncMode,
                                               dataSize, transCnt,
                                               buff) != ADI_SUCCESS) {
    return luaL_error(L, "Block read failed!");
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
static int luaApi_adiv5_ap_write_mem_block(lua_State *L) {
  struct luaApi_accessPort *luaApObj =
      luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE);
  uint64_t addr = luaL_checkinteger(L, 2);
  int addrIncMode = (int)luaL_checkinteger(L, 3);
  int dataSize = (int)luaL_checkinteger(L, 4);
  size_t transCnt;  // 注意size_t在在64位环境下是8字节，int在64位下是4字节
  uint8_t *buff = (uint8_t *)lua_tolstring(L, 5, &transCnt);
  if (luaApObj->ap->type != AccessPort_Memory) {
    return luaL_error(L, "Not a memory access port.");
  }
  if (transCnt & 0x3) {
    return luaL_error(
        L,
        "The length of the data to be written is not a multiple of the word.");
  }
  if (luaApObj->ap->Interface.Memory.BlockWrite(luaApObj->ap, addr, addrIncMode,
                                                dataSize, (int)transCnt >> 2,
                                                buff) != ADI_SUCCESS) {
    return luaL_error(L, "Block write failed!");
  }
  return 0;
}

/**
 * 读取Component ID 和 Peripheral ID
 * 1#：Adapter对象
 * 2#：base：Component地址 64位
 * 返回：
 * 1#：cid
 * 2#：pid 64位
 */
static int luaApi_adiv5_ap_get_pid_cid(lua_State *L) {
  struct luaApi_accessPort *luaApObj =
      luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE);
  uint64_t base = luaL_checkinteger(L, 2);
  uint32_t cid;
  uint64_t pid;
  if (ADIv5_ReadCidPid(luaApObj->ap, base, &cid, &pid) != ADI_SUCCESS) {
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
  struct luaApi_dap *luaDap =
      CAST(struct luaApi_dap *, luaL_checkudata(L, 1, ADIV5_LUA_OBJECT_TYPE));
  log_trace("[GC] ADIv5");
  // 销毁DAP对象
  ADIv5_DestoryDap(&luaDap->dap);
  // 取消引用Adapter对象
  luaL_unref(L, LUA_REGISTRYINDEX, luaDap->adapterRef);
  return 0;
}

/**
 * ADIv5 AccessPort 垃圾回收函数
 */
static int luaApi_adiv5_access_port_gc(lua_State *L) {
  struct luaApi_accessPort *luaAp =
      CAST(struct luaApi_accessPort *,
           luaL_checkudata(L, 1, ADIV5_AP_MEM_LUA_OBJECT_TYPE));
  log_trace("[GC] Access Port");
  // 取消引用DAP对象
  luaL_unref(L, LUA_REGISTRYINDEX, luaAp->reference);
  return 0;
}

// 模块静态函数
static const luaL_Reg lib_adiv5_f[] = {{"Create", luaApi_adiv5_create_dap},
                                       {NULL, NULL}};

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

// 初始化ADIv5库
int luaopen_adiv5(lua_State *L) {
  lua_createtable(L, 0,
                  sizeof(lib_adiv5_const) /
                      sizeof(lib_adiv5_const[0]));  // 预分配索引空间，提高效率
  // 注册常量到模块中
  LuaApiRegConstant(L, lib_adiv5_const);
  // 将函数注册进去
  luaL_setfuncs(L, lib_adiv5_f, 0);
  return 1;
}

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

// 注册接口调用
static int RegisterApi_ADIv5(lua_State *L, void *opaque) {
  // 创建
  LuaApiNewTypeMetatable(L, ADIV5_LUA_OBJECT_TYPE, luaApi_adiv5_gc,
                         lib_adiv5_oo);
  LuaApiNewTypeMetatable(L, ADIV5_AP_MEM_LUA_OBJECT_TYPE,
                         luaApi_adiv5_access_port_gc, lib_access_port_oo);
  luaL_requiref(L, "ADIv5", luaopen_adiv5, 0);
  lua_pop(L, 1);

  return 0;
}

LUA_API_ENTRY luaApi_entry api = {"ADIv5", RegisterApi_ADIv5, NULL};