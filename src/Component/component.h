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

// XXX 增加优先级
struct componentEntry {
  const char *name;
  ComponentInitFunc init;
  void *opaque;
  struct list_head entry;
};

extern struct componentEntry components;

void component_regist(struct componentEntry *c);
void component_init(lua_State *L);

#define COMPONENT_INIT(name, init, opaque)                                                  \
  static struct componentEntry _component_item_##name = {                                   \
      #name, init, opaque, {&_component_item_##name.entry, &_component_item_##name.entry}}; \
  void __attribute__((used, constructor)) _component_item_##name##_register(void) {         \
    component_regist(&_component_item_##name);                                               \
  }

#endif
