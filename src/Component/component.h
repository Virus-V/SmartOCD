/**
 * src/Component/component.h
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

#ifndef SRC_COMPONENT_COMPONENT_H_
#define SRC_COMPONENT_COMPONENT_H_

#include "smartocd.h"

#include "Library/lua/src/lauxlib.h"
#include "Library/lua/src/lua.h"
#include "Library/lua/src/lualib.h"
#include "Library/misc/list.h"
//
typedef int (*ComponentInitFunc)(lua_State *L, void *opaque);

struct componentEntry {
  const char *name;
  ComponentInitFunc init;
  void *opaque;
  unsigned int priority;
  struct list_head entry;
};

enum componentPriority {
  COM_ADAPTER = 0x0 << 16,
};

void component_regist(struct componentEntry *c);
void component_init(lua_State *L);

/*
 * 注册组件
 * name：组件名
 * init：组件初始化函数
 * opaque：函数参数
 * com：组件类型
 * pri：组件初始化优先级
 */
#define COMPONENT_INIT(name, init, opaque, com, pri)                                                       \
  static struct componentEntry _component_item_##name = {                                                  \
      #name, init, opaque, (com) + (pri), {&_component_item_##name.entry, &_component_item_##name.entry}}; \
  void __attribute__((used, constructor)) _component_item_##name##_register(void) {                        \
    component_regist(&_component_item_##name);                                                             \
  }

#endif
