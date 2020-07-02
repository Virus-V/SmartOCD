/**
 * src/Component/adapter/adapter_api_jtag.c
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

#include "Component/adapter/adapter_api.h"

#include "Component/component.h"
#include "Library/jtag/jtag.h"
#include "Library/lua_api/api.h"

/**
 * JTAG状态机切换
 * 1#：adapter对象
 * 2#：状态
 */
static int luaApi_adapter_jtag_status_change(lua_State *L) {
  JtagSkill skillObj = *CAST(JtagSkill *, luaL_checkudata(L, 1, SKILL_JTAG_LUA_OBJECT_TYPE));
  enum JTAG_TAP_State state = (enum JTAG_TAP_State)luaL_checkinteger(L, 2);

  if (state < JTAG_TAP_RESET || state > JTAG_TAP_IRUPDATE) {
    return luaL_error(L, "JTAG state machine new state is illegal!");
  }

  if (skillObj->JtagToState(skillObj, state) != ADPT_SUCCESS) {
    return luaL_error(L, "Insert to instruction queue failed!");
  }

  // 执行队列
  if (skillObj->JtagCommit(skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    skillObj->JtagCleanPending(skillObj);
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
static int luaApi_adapter_jtag_exchange_data(lua_State *L) {
  JtagSkill skillObj = *CAST(JtagSkill *, luaL_checkudata(L, 1, SKILL_JTAG_LUA_OBJECT_TYPE));
  size_t str_len = 0;
  const char *tdi_data = lua_tolstring(L, 2, &str_len);
  unsigned int bitCnt = (unsigned int)luaL_checkinteger(L, 3);

  // 判断bit长度是否合法
  if ((str_len << 3) < bitCnt) { // 字符串长度小于要发送的字节数
    return luaL_error(L, "TDI data length is illegal!");
  }

  // 开辟缓冲内存空间
  uint8_t *data = malloc(str_len * sizeof(uint8_t));
  if (data == NULL) {
    return luaL_error(L, "TDI data buff alloc Failed!");
  }

  // 拷贝数据
  memcpy(data, tdi_data, str_len * sizeof(uint8_t));
  // 插入JTAG指令
  if (skillObj->JtagExchangeData(skillObj, data, bitCnt) != ADPT_SUCCESS) {
    free(data);
    return luaL_error(L, "Insert to instruction queue failed!");
  }

  // 执行队列
  if (skillObj->JtagCommit(skillObj) != ADPT_SUCCESS) {
    free(data);
    // 清理指令队列
    skillObj->JtagCleanPending(skillObj);
    return luaL_error(L, "Execute the instruction queue failed!");
  }

  // 执行成功，构造lua字符串
  lua_pushlstring(L, data, str_len);
  free(data); // 释放缓冲

  return 1;
}

/**
 * 在UPDATE之后转入idle状态等待几个时钟周期，以等待慢速的内存操作完成
 * 1#:adapter对象
 * 2#:cycles要进入Idle等待的周期
 */
static int luaApi_adapter_jtag_idle_wait(lua_State *L) {
  JtagSkill skillObj = *CAST(JtagSkill *, luaL_checkudata(L, 1, SKILL_JTAG_LUA_OBJECT_TYPE));
  unsigned int cycles = (unsigned int)luaL_checkinteger(L, 2);

  if (skillObj->JtagIdle(skillObj, cycles) != ADPT_SUCCESS) {
    return luaL_error(L, "Insert to instruction queue failed!");
  }

  // 执行队列
  if (skillObj->JtagCommit(skillObj) != ADPT_SUCCESS) {
    // 清理指令队列
    skillObj->JtagCleanPending(skillObj);
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
static int luaApi_adapter_jtag_pins(lua_State *L) {
  JtagSkill skillObj = *CAST(JtagSkill *, luaL_checkudata(L, 1, SKILL_JTAG_LUA_OBJECT_TYPE));
  uint8_t pin_mask = (uint8_t)luaL_checkinteger(L, 2);
  uint8_t pin_data = (uint8_t)luaL_checkinteger(L, 3);
  unsigned int pin_wait = (unsigned int)luaL_checkinteger(L, 4);

  if (skillObj->JtagPins(skillObj, pin_mask, pin_data, &pin_data, pin_wait) != ADPT_SUCCESS) {
    return luaL_error(L, "Read/Write JTAG pins failed!");
  }
  lua_pushinteger(L, pin_data);
  return 1;
}

static const luaL_Reg lib_jtag_skill_oo[] = {
    // JTAG相关接口
    {"JtagExchangeData", luaApi_adapter_jtag_exchange_data},
    {"JtagIdle", luaApi_adapter_jtag_idle_wait},
    {"JtagToState", luaApi_adapter_jtag_status_change},
    {"JtagPins", luaApi_adapter_jtag_pins},

    {NULL, NULL}};

/* 注册JTAG能力集对象元表 */
void LuaApi_jtag_skill_type_register(lua_State *L) {
  LuaApi_create_new_type(L, SKILL_JTAG_LUA_OBJECT_TYPE, NULL, lib_jtag_skill_oo, NULL);
}

/* 创建JTAG能力集对象 */
void LuaApi_create_jtag_skill_object(lua_State *L, const struct skill *skill) {
  assert(L != NULL);
  assert(skill != NULL);

  JtagSkill *jtagSkillObj;

  jtagSkillObj = CAST(JtagSkill *, lua_newuserdatauv(L, sizeof(JtagSkill), 1));

  *jtagSkillObj = CAST(JtagSkill, skill);

  // 绑定metatable
  luaL_setmetatable(L, SKILL_JTAG_LUA_OBJECT_TYPE);
}

