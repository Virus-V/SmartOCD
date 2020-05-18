/*
 * @Author: Virus.V
 * @Date: 2020-05-18 21:24:15
 * @LastEditTime: 2020-05-18 22:34:17
 * @LastEditors: Virus.V
 * @Description:
 * @FilePath: /SmartOCD/src/Component/component.c
 * @Email: virusv@live.com
 */

#include "component.h"
#include "smartocd.h"

#include "Library/log/log.h"

// 所有component链表
struct componentEntry components = {
  NULL, NULL, NULL, {&components.entry, &components.entry}
};

// 注册Component
void ComponentRegist(struct componentEntry *c) {
  list_add(&c->entry, &components.entry);
}

// 初始化Components
void ComponentInit(lua_State *L) {
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