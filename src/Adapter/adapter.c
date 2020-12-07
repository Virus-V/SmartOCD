/**
 * src/Adapter/adapter.c
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

#include "Adapter/adapter.h"
#include "Library/misc/list.h"
#include "smartocd.h"

// 获取Adapter的能力集
const struct skill *Adapter_GetSkill(Adapter adapter, enum skillType type) {
  struct skill *skillObj;

  assert(adapter != NULL);

  list_for_each_entry(skillObj, &adapter->skills, skills) {
    if (skillObj->type == type) {
      return skillObj;
    }
  }

  return NULL;
}
