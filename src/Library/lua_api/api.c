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

#include "api.h"
#include <string.h>
#include <pthread.h>

#include "Library/log/log.h"
#include "smartocd.h"

/**
 * 往栈顶的table中注册常量
 */
void LuaApi_reg_constant(lua_State *L, const luaApi_regConst *c) {
  assert(L != NULL);
  while (c->name != NULL) {
    // TODO distinguish constant type
    lua_pushinteger(L, c->value);
    // register constant to the table
    lua_setfield(L, -2, c->name);
    c++;
  }
}

/**
 * 创建类型元表
 * tname：类型名称
 * gc：该类型被垃圾回收时执行的自定义释放函数
 * oo：该类型对应的方法
 * tparent：该类型的父对象类型
 */
void LuaApi_create_new_type(lua_State *L, const char *tname, lua_CFunction gc, const luaL_Reg *oo, const char *tparent) {
  assert(L != NULL);
  assert(tname != NULL);
  assert(oo != NULL);

  int tparent_idx, stack_top;

  stack_top = lua_gettop(L);

  if (tparent != NULL){
    // 获得tparent对应的metatable
    if (LUA_TNIL == luaL_getmetatable(L, tparent)){ // +1
      luaL_error(L, "Unknow object type %s.\n", tparent);
      lua_settop(L, stack_top);
      return;
    }
    tparent_idx = lua_absindex(L, -1);
  }

  if (luaL_newmetatable(L, tname) == 0) { // +1
    log_warn("Type %s already registed.\n", tname);
    lua_settop(L, stack_top);
    return;
  }

  // 创建method表，并设置到元表的__index字段
  lua_newtable(L); // +1

  lua_pushvalue(L, -1); // +1
  lua_setfield(L, -3, "__index"); // -1

  if (gc != NULL){
    lua_pushcfunction(L, gc); // +1
    lua_setfield(L, -3, "__gc"); // -1
  }

  // 注册method到method表
  luaL_setfuncs(L, oo, 0);

  if (tparent != NULL){
    // 将tparent类型元表压栈
    lua_pushvalue(L, tparent_idx);  // +1
    lua_setmetatable(L, -2);  // -1
  }

  lua_settop(L, stack_top);
}

/**
 * 检查是否是Adapter对象
 * ud: userdata index
 */
void *LuaApi_check_object_type(lua_State *L, int ud, const char *type) {
  void *p = lua_touserdata(L, ud);
  if (p != NULL) {
    if (lua_getmetatable(L, ud)) {
      //获得metatable的__name字段
      if (lua_getfield(L, -1, "__name") != LUA_TSTRING) {
        lua_pop(L, 2);
        return NULL;
      }
      const char *typeName = lua_tostring(L, -1); // 获得类型字符串
      if (strncmp(typeName, type, strlen(type)) != 0) {
        lua_pop(L, 2);
        return NULL;
      }
      /* 获得typename对应的meta table */
      luaL_getmetatable(L, typeName);
      if (!lua_rawequal(L, -1, -3))
        p = NULL;
      lua_pop(L, 3);
      return p;
    }
  }
  return NULL;
}

// Lua 虚拟机锁
static pthread_mutex_t vmlock = PTHREAD_MUTEX_INITIALIZER;
void core_LockVM(lua_State *L) {
  (void)L;
  pthread_mutex_lock(&vmlock);
}

void core_UnlockVM(lua_State *L) {
  (void)L;
  pthread_mutex_unlock(&vmlock);
}

