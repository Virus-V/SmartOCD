/**
 * src/Component/component.c
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

#include "component.h"
#include "smartocd.h"

#include "Library/log/log.h"

// 所有component链表
struct componentEntry components = {NULL, NULL, NULL, {&components.entry, &components.entry}};

// 注册Component
void component_regist(struct componentEntry *c) { list_add(&c->entry, &components.entry); }

// 初始化Components
void component_init(lua_State *L) {
  int ret;
  struct componentEntry *curr;

  assert(L != NULL);
  list_for_each_entry(curr, &components.entry, entry) {
    log_info("Regist Lua API: %s.", curr->name);
    ret = (*curr->init)(L, curr->opaque);
    if (ret != 0) {
      log_fatal("Regist %s failed!", curr->name);
      FATAL_ABORT(1);
    }
  }
}
