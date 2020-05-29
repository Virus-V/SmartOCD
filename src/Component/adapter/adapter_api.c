/*
 * @Author: Virus.V
 * @Date: 2020-05-18 21:51:33
 * @LastEditTime: 2020-05-29 16:42:12
 * @LastEditors: Virus.V
 * @Description:
 * @FilePath: /SmartOCD/src/Component/adapter/adapter_api.c
 * @Email: virusv@live.com
 */
/*
 * adapter.c
 *
 *  Created on: 2019-5-16
 *      Author: virusv
 */

#include "Adapter/adapter_jtag.h"
#include "Adapter/adapter_dap.h"

#include "Component/component.h"
#include "Library/lua_api/api.h"

// 模块常量
static const luaApi_regConst lib_adapter_const[] = {
    // 传输方式
    {"JTAG", ADPT_MODE_JTAG},
    {"SWD", ADPT_MODE_SWD},
    // JTAG引脚bit mask
    {"PIN_SWCLK_TCK", SWJ_PIN_SWCLK_TCK},
    {"PIN_SWDIO_TMS", SWJ_PIN_SWDIO_TMS},
    {"PIN_TDI", SWJ_PIN_TDI},
    {"PIN_TDO", SWJ_PIN_TDO},
    {"PIN_nTRST", SWJ_PIN_nTRST},
    {"PIN_nRESET", SWJ_PIN_nRESET},
    // 复位类型
    {"RESET_SYSTEM", ADPT_RESET_SYSTEM_RESET},
    {"RESET_DEBUG", ADPT_RESET_DEBUG_RESET},
    // DAP寄存器类型
    {"REG_DP", ADPT_DAP_DP_REG},
    {"REG_AP", ADPT_DAP_AP_REG},
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

// 初始化Adapter库
int luaopen_adapter(lua_State *L) {
  // create module table
  lua_createtable(
      L, 0,
      sizeof(lib_adapter_const) /
          sizeof(lib_adapter_const[0]));  // 预分配索引空间，提高效率
  // 注册常量到模块中
  LuaApiRegConstant(L, lib_adapter_const);
  return 1;
}

// 注册接口调用
static int RegisterApi_Adapter(lua_State *L, void *opaque) {
  // 在栈中存在luaopen_adapter()的返回值副本
  luaL_requiref(L, "Adapter", luaopen_adapter, 0);
  lua_pop(L, 1);

  return 0;
}

COMPONENT_INIT(Adapter, RegisterApi_Adapter, NULL);
