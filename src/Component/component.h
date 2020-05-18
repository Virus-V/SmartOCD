/*
 * @Author: Virus.V
 * @Date: 2020-05-18 21:06:50
 * @LastEditTime: 2020-05-18 22:20:18
 * @LastEditors: Virus.V
 * @Description:
 * @FilePath: /SmartOCD/src/Component/component.h
 * @Email: virusv@live.com
 */

#ifndef SRC_COMPONENT_COMPONENT_H_
#define SRC_COMPONENT_COMPONENT_H_

#include "smartocd.h"

#include "Library/lua/src/lauxlib.h"
#include "Library/lua/src/lua.h"
#include "Library/lua/src/lualib.h"
#include "Library/misc/list.h"
//
typedef int (*ComponentInitFunc)(lua_State *L, void *opaque);

// XXX 增加优先级
struct componentEntry {
  const char *name;
  ComponentInitFunc init;
  void *opaque;
  struct list_head entry;
};

extern struct componentEntry components;

void ComponentRegist(struct componentEntry *c);
void ComponentInit(lua_State *L);

#define COMPONENT_INIT(name, init, opaque)                             \
  static struct componentEntry _component_item_##name = {              \
      #name,                                                           \
      init,                                                            \
      opaque,                                                          \
      {&_component_item_##name.entry, &_component_item_##name.entry}}; \
  void __attribute__((used, constructor))                              \
      _component_item_##name##_register(void) {                        \
    ComponentRegist(&_component_item_##name);                          \
  }

#endif