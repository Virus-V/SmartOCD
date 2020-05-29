/*
 * @Author: Virus.V
 * @Date: 2020-05-25 14:58:17
 * @LastEditors: Virus.V
 * @LastEditTime: 2020-05-29 17:29:37
 * @Description: file content
 * @Email: virusv@live.com
 */

#include "Adapter/adapter_private.h"
#include "smartocd.h"
#include "Library/misc/list.h"

// 获取Adapter的能力集
struct capability *Adapter_GetCapability(Adapter adapter, enum capabilityType type) {
  struct capability *capObj;

  assert(adapter != NULL);

  list_for_each_entry(capObj, &adapter->capabilities, capabilities) {
    if (capObj->type == type) {
      return capObj;
    }
  }

  return NULL;
}