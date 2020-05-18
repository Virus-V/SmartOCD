/***
 * @Author: Virus.V
 * @Date: 2020-05-15 11:30:22
 * @LastEditors: Virus.V
 * @LastEditTime: 2020-05-18 19:45:53
 * @Description: file content
 * @Email: virusv@live.com
 */
/*
 * api.h
 *
 *  Created on: 2018-4-20
 *      Author: virusv
 */

#ifndef SRC_API_API_H_
#define SRC_API_API_H_

#include <string.h>

#include "Library/lua/src/lauxlib.h"
#include "Library/lua/src/lua.h"
#include "Library/lua/src/lualib.h"

// lua api 注册
#define LUA_API_ENTRY __attribute__((used, section(".data.lua_api")))

// Lua接口注册
typedef int (*luaApi_reg_fun)(lua_State *L, void *opaque);

typedef struct {
  // 接口名
  const char *name;
  // 注册函数
  luaApi_reg_fun cb;
  // 注册函数的自定义参数
  void *opaque;
} luaApi_entry;

// link set
extern luaApi_entry __start_lua_api;
extern luaApi_entry __stop_lua_api;

typedef struct {
  char *name;
  int value;
} luaApi_regConst;

// strnicmp
#define STR_EQUAL(s, os) (strncasecmp((s), os, sizeof(os)) == 0)

/**
 * 初始化Lua接口
 */
void LuaApiInit(lua_State *L);

void LuaApiRegConstant(lua_State *L, const luaApi_regConst *c);

void LuaApiNewTypeMetatable(lua_State *L, const char *tname, lua_CFunction gc,
                            const luaL_Reg *oo);
#endif /* SRC_API_API_H_ */
