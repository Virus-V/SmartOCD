/**
 * src/Component/adapter/adapter_api.c
 * Copyright (c) 2020 Virus.V <virusv@live.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Adapter/adapter.h"

#include "Component/adapter/adapter_api.h"
#include "Component/component.h"

// 模块常量
// XXX 在metatable里防止常量被改写
static const luaApi_regConst lib_adapter_const[] = {
    // 传输方式
    {"MODE_JTAG", ADPT_MODE_JTAG},
    {"MODE_SWD", ADPT_MODE_SWD},

    // Skill类型
    {"SKILL_DAP", ADPT_SKILL_DAP},
    {"SKILL_JTAG", ADPT_SKILL_JTAG},

    // JTAG引脚bit mask
    {"PIN_SWCLK_TCK", JTAG_PIN_SWCLK_TCK},
    {"PIN_SWDIO_TMS", JTAG_PIN_SWDIO_TMS},
    {"PIN_TDI", JTAG_PIN_TDI},
    {"PIN_TDO", JTAG_PIN_TDO},
    {"PIN_nTRST", JTAG_PIN_nTRST},
    {"PIN_nRESET", JTAG_PIN_nRESET},

    // 复位类型
    {"RESET_SYSTEM", ADPT_RESET_SYSTEM},
    {"RESET_DEBUG", ADPT_RESET_DEBUG},

    // DAP寄存器类型
    {"REG_DP", SKILL_DAP_DP_REG},
    {"REG_AP", SKILL_DAP_AP_REG},

    // TAP 状态
    {"TAP_RESET", JTAG_TAP_RESET},
    {"TAP_IDLE", JTAG_TAP_IDLE},
    {"TAP_DR_SELECT", JTAG_TAP_DRSELECT},
    {"TAP_DR_CAPTURE", JTAG_TAP_DRCAPTURE},
    {"TAP_DR_SHIFT", JTAG_TAP_DRSHIFT},
    {"TAP_DR_EXIT1", JTAG_TAP_DREXIT1},
    {"TAP_DR_PAUSE", JTAG_TAP_DRPAUSE},
    {"TAP_DR_EXIT2", JTAG_TAP_DREXIT2},
    {"TAP_DR_UPDATE", JTAG_TAP_DRUPDATE},
    {"TAP_IR_SELECT", JTAG_TAP_IRSELECT},
    {"TAP_IR_CAPTURE", JTAG_TAP_IRCAPTURE},
    {"TAP_IR_SHIFT", JTAG_TAP_IRSHIFT},
    {"TAP_IR_EXIT1", JTAG_TAP_IREXIT1},
    {"TAP_IR_PAUSE", JTAG_TAP_IRPAUSE},
    {"TAP_IR_EXIT2", JTAG_TAP_IREXIT2},
    {"TAP_IR_UPDATE", JTAG_TAP_IRUPDATE},

    // 仿真器的状态
    {"STATUS_CONNECTED", ADPT_STATUS_CONNECTED},
    {"STATUS_DISCONNECT", ADPT_STATUS_DISCONNECT},
    {"STATUS_RUNING", ADPT_STATUS_RUNING},
    {"STATUS_IDLE", ADPT_STATUS_IDLE},
    {NULL, 0}};

/**
 * 设置/读取Adapter状态
 * 第一个参数：AdapterObject指针
 * 第二个参数：状态：CONNECTED、DISCONNECT、RUNING、IDLE
 * 返回：无
 */
static int luaApi_adapter_status(lua_State *L) {
  // 获得adapter接口对象
  void *udata = LuaApi_check_object_type(L, 1, ADAPTER_LUA_OBJECT_TYPE);
  if (udata == NULL) {
    return luaL_error(L, "Not a vailed Adapter object!");
  }
  Adapter adapterObj = *CAST(Adapter *, udata);

  if (lua_gettop(L) == 1) {
    lua_pushinteger(L, adapterObj->currStatus);
    return 1;
  } else {
    int type = (int)luaL_checkinteger(L, 2); // 类型
    if (adapterObj->SetStatus(adapterObj, type) != ADPT_SUCCESS) {
      return luaL_error(L, "Set adapter status failed!");
    }
    return 0;
  }
}

/**
 * 设置仿真器时钟
 * 第一个参数：AdapterObject指针
 * 第二个参数：整数，频率 Hz
 */
static int luaApi_adapter_frequency(lua_State *L) {
  void *udata = LuaApi_check_object_type(L, 1, ADAPTER_LUA_OBJECT_TYPE);
  if (udata == NULL) {
    return luaL_error(L, "Not a vailed Adapter object!");
  }
  Adapter adapterObj = *CAST(Adapter *, udata);

  if (lua_gettop(L) == 1) {
    lua_pushinteger(L, adapterObj->currFrequency);
    return 1;
  } else {
    int clockHz = (int)luaL_checkinteger(L, 2);
    if (adapterObj->SetFrequency(adapterObj, clockHz) != ADPT_SUCCESS) {
      return luaL_error(L, "Set adapter clock failed!");
    }
    return 0;
  }
}

/**
 * 选择仿真器的传输模式
 * 第一个参数：AdapterObject指针
 * 第二个参数：传输方式类型adapter.SWD或adapter.JTAG。。
 * 如果不提供第二个参数，则表示读取当前活动的传输模式
 * 返回值：无返回值或者1返回值
 * 1#：当前活动的传输模式
 */
static int luaApi_adapter_transmission_mode(lua_State *L) {
  void *udata = LuaApi_check_object_type(L, 1, ADAPTER_LUA_OBJECT_TYPE);
  if (udata == NULL) {
    return luaL_error(L, "Not a vailed Adapter object!");
  }
  Adapter adapterObj = *CAST(Adapter *, udata);

  if (lua_gettop(L) == 1) { // 读取当前活动传输模式
    lua_pushinteger(L, adapterObj->currTransMode);
    return 1;
  } else {                                                                     // 设置当前传输模式
    enum transferMode type = CAST(enum transferMode, luaL_checkinteger(L, 2)); // 类型
    if (adapterObj->SetTransferMode(adapterObj, type) != ADPT_SUCCESS) {
      return luaL_error(L, "Set adapter transmission failed!");
    }
    return 0;
  }
}

/**
 * 获取Adapter的能力集
 * 1#: adapter对象
 * 2#: 能力集类型
 * 返回: 能力集对象，
 *      如果没有指定的能力集，返回nil
 *      其他错误抛出异常
 */
static int luaApi_adapter_get_skill(lua_State *L) {
  void *udata = LuaApi_check_object_type(L, 1, ADAPTER_LUA_OBJECT_TYPE);
  if (udata == NULL) {
    return luaL_error(L, "Not a vailed Adapter object!");
  }
  Adapter adapterObj = *CAST(Adapter *, udata);

  enum skillType skill_type = (enum skillType)luaL_checkinteger(L, 2);
  if (skill_type < 0 || skill_type >= ADPT_SKILL_MAX) {
    return luaL_argerror(L, 2, "Invaild Skill Type!");
  }

  const struct skill *skill = Adapter_GetSkill(adapterObj, skill_type);
  if (skill == NULL) {
    lua_pushnil(L);
    return 1;
  }

  switch (skill_type) {
  case ADPT_SKILL_DAP:
    LuaApi_create_dap_skill_object(L, skill);
    break;
  case ADPT_SKILL_JTAG:
    LuaApi_create_jtag_skill_object(L, skill);
    break;

  default:
    return luaL_error(L, "Unknow skill type");
  }

  // 绑定Adapter对象，防止过早被GC释放
  lua_pushvalue(L, -3); // 复制Adapter对象
  lua_setiuservalue(L, -2, 1);

  return 1;
}

/**
 * 复位Target
 * 1#:adapter对象
 * 2#：复位类型
 * 无返回值，失败会报错
 */
static int luaApi_adapter_reset(lua_State *L) {
  void *udata = LuaApi_check_object_type(L, 1, ADAPTER_LUA_OBJECT_TYPE);
  if (udata == NULL) {
    return luaL_error(L, "Not a vailed Adapter object!");
  }
  Adapter adapterObj = *CAST(Adapter *, udata);

  int type = (int)luaL_optinteger(L, 2, ADPT_RESET_SYSTEM);
  if (adapterObj->Reset(adapterObj, type) != ADPT_SUCCESS) {
    return luaL_error(L, "Reset adapter failed!");
  }
  return 0;
}

static const luaL_Reg lib_adapter_oo[] = {
    // 基本函数
    {"Status", luaApi_adapter_status},
    {"Frequency", luaApi_adapter_frequency},
    {"TransferMode", luaApi_adapter_transmission_mode},
    {"GetSkill", luaApi_adapter_get_skill},
    {"Reset", luaApi_adapter_reset},

    {NULL, NULL}};

// 初始化Adapter库
int luaopen_adapter(lua_State *L) {
  // create module table
  lua_createtable(L, 0, sizeof(lib_adapter_const) / sizeof(lib_adapter_const[0]));

  // 注册Adapter常量到模块中
  LuaApi_reg_constant(L, lib_adapter_const);

  // 注册skill类型、Adapter类型
  LuaApi_create_new_type(L, ADAPTER_LUA_OBJECT_TYPE, NULL, lib_adapter_oo, NULL);
  LuaApi_dap_skill_type_register(L);
  LuaApi_jtag_skill_type_register(L);

  return 1;
}

// 注册接口调用
static int RegisterApi_Adapter(lua_State *L, void *opaque) {
  // 在栈中存在luaopen_adapter()的返回值副本
  luaL_requiref(L, "Adapter", luaopen_adapter, 0);
  lua_pop(L, 1);

  return 0;
}

COMPONENT_INIT(Adapter, RegisterApi_Adapter, NULL, COM_ADAPTER, 0);
