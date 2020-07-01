/**
 * src/Component/adapter/adapter_api_dap.c
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
#include "Library/lua_api/api.h"

/**
 * DAP单次读寄存器
 * 1#:Adapter对象
 * 2#:type寄存器类型 AP还是DP
 * 3#:reg 寄存器号
 * 返回:
 * 1#:寄存器的值
 */
static int luaApi_adapter_dap_single_read(lua_State *L) {
  struct luaApi_dapSkill * skillObj = CAST(struct luaApi_dapSkill *, luaL_checkudata(L, 1, SKILL_DAP_LUA_OBJECT_TYPE));
  uint32_t data;
  int type = (int)luaL_checkinteger(L, 2);
  int reg = (int)luaL_checkinteger(L, 3);

  if (skillObj->skill->DapSingleRead(skillObj->self, type, reg, &data) != ADPT_SUCCESS) {
    return luaL_error(L, "Insert to instruction queue failed!");
  }

  // 执行队列
  if (skillObj->skill->DapCommit(skillObj->self) != ADPT_SUCCESS) {
    // 清理指令队列
    skillObj->skill->DapCleanPending(skillObj->self);
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
static int luaApi_adapter_dap_single_write(lua_State *L) {
  struct luaApi_dapSkill * skillObj = CAST(struct luaApi_dapSkill *, luaL_checkudata(L, 1, SKILL_DAP_LUA_OBJECT_TYPE));
  int type = (int)luaL_checkinteger(L, 2);
  int reg = (int)luaL_checkinteger(L, 3);
  uint32_t data = (uint32_t)luaL_checkinteger(L, 4);
  
  if (skillObj->skill->DapSingleWrite(skillObj->self, type, reg, data) != ADPT_SUCCESS) {
    return luaL_error(L, "Insert to instruction queue failed!");
  }

  // 执行队列
  if (skillObj->skill->DapCommit(skillObj->self) != ADPT_SUCCESS) {
    // 清理指令队列
    skillObj->skill->DapCleanPending(skillObj->self);
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
static int luaApi_adapter_dap_multi_read(lua_State *L) {
  struct luaApi_dapSkill * skillObj = CAST(struct luaApi_dapSkill *, luaL_checkudata(L, 1, SKILL_DAP_LUA_OBJECT_TYPE));
  int type = (int)luaL_checkinteger(L, 2);
  int reg = (int)luaL_checkinteger(L, 3);
  int count = (int)luaL_checkinteger(L, 4);

  // 开辟缓冲内存空间
  uint32_t *buff = malloc(count * sizeof(uint32_t));
  if (buff == NULL) {
    return luaL_error(L, "Multi-read buff alloc Failed!");
  }
  
  if (skillObj->skill->DapMultiRead(skillObj->self, type, reg, count, buff) != ADPT_SUCCESS) {
    free(buff);
    return luaL_error(L, "Insert to instruction queue failed!");
  }
  
  // 执行队列
  if (skillObj->skill->DapCommit(skillObj->self) != ADPT_SUCCESS) {
    free(buff);
    // 清理指令队列
    skillObj->skill->DapCleanPending(skillObj->self);
    return luaL_error(L, "Execute the instruction queue failed!");
  }
  lua_pushlstring(L, (char *)buff, count * sizeof(uint32_t));
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
static int luaApi_adapter_dap_multi_write(lua_State *L) {
  struct luaApi_dapSkill * skillObj = CAST(struct luaApi_dapSkill *, luaL_checkudata(L, 1, SKILL_DAP_LUA_OBJECT_TYPE));
  int type = (int)luaL_checkinteger(L, 2);
  int reg = (int)luaL_checkinteger(L, 3);
  size_t transCnt; // 注意size_t在在64位环境下是8字节，int在64位下是4字节
  uint32_t *buff = (uint32_t *)luaL_checklstring(L, 4, &transCnt);

  if (transCnt == 0 || (transCnt & 0x3)) {
    return luaL_error(L, "The length of the data to be written is not a multiple of the word.");
  }

  if (skillObj->skill->DapMultiWrite(skillObj->self, type, reg, transCnt >> 2, buff) != ADPT_SUCCESS) {
    return luaL_error(L, "Insert to instruction queue failed!");
  }

  // 执行队列
  if (skillObj->skill->DapCommit(skillObj->self) != ADPT_SUCCESS) {
    // 清理指令队列
    skillObj->skill->DapCleanPending(skillObj->self);
    return luaL_error(L, "Execute the instruction queue failed!");
  }

  return 0;
}

static const luaL_Reg lib_dap_skill_oo[] = {
    // DAP相关接口
    {"DapSingleRead", luaApi_adapter_dap_single_read},
    {"DapSingleWrite", luaApi_adapter_dap_single_write},
    {"DapMultiRead", luaApi_adapter_dap_multi_read},
    {"DapMultiWrite", luaApi_adapter_dap_multi_write},
    {NULL, NULL}};

/* 注册DAP能力集对象元表 */
void LuaApi_dap_skill_type_register(lua_State *L) {
  LuaApi_create_new_type(L, SKILL_DAP_LUA_OBJECT_TYPE, NULL, lib_dap_skill_oo, NULL);
}

/* 创建DAP能力集对象 */
void LuaApi_create_dap_skill_object(lua_State *L, Adapter self, const struct skill *skill) {
  assert(L != NULL);
  assert(self != NULL && skill != NULL);

  struct luaApi_dapSkill *dapSkillObj;

  dapSkillObj = CAST(struct luaApi_dapSkill *, lua_newuserdata(L, sizeof(struct luaApi_dapSkill)));

  dapSkillObj->self = self;
  dapSkillObj->skill = CAST(DapSkill, skill);

  // 绑定metatable
  luaL_setmetatable(L, SKILL_DAP_LUA_OBJECT_TYPE);
}
