/**
 * src/Component/adapter/adapter_api.h
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

#ifndef __ADAPTER_API_H__
#define __ADAPTER_API_H__ 

#include "Adapter/adapter.h"
#include "Adapter/adapter_dap.h"
#include "Adapter/adapter_jtag.h"
#include "Library/lua_api/api.h"

#define ADAPTER_LUA_OBJECT_TYPE "adapter"
#define SKILL_DAP_LUA_OBJECT_TYPE "skill.dap"
#define SKILL_JTAG_LUA_OBJECT_TYPE "skill.jtag"

struct luaApi_jtagSkill {
  Adapter self;
  JtagSkill skill;
};

struct luaApi_dapSkill {
  Adapter self;
  DapSkill skill;  
};

void LuaApi_jtag_skill_type_register(lua_State *L);
void LuaApi_dap_skill_type_register(lua_State *L);

void LuaApi_create_jtag_skill_object(lua_State *L, Adapter self, const struct skill *skill);
void LuaApi_create_dap_skill_object(lua_State *L, Adapter self, const struct skill *skill);

#endif
