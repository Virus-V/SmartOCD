/*
 * @Author: Virus.V
 * @Date: 2020-05-25 14:58:17
 * @LastEditors: Virus.V
 * @LastEditTime: 2020-05-25 18:35:15
 * @Description: file content
 * @Email: virusv@live.com
 */

#include "Adapter/adapter_private.h"
#include "smartocd.h"

struct transport *adapterCreateTransport(int count) {
  struct transport *head = NULL, *curr = NULL;
  int i;

  assert(count > 0);

  for (i = 0; i < count; i++) {
    curr = calloc(1, sizeof(struct transport));
    if (!curr) {
      goto _error_exit;
    }

    curr->mode = ADPT_MODE_MAX;
    INIT_LIST_HEAD(&curr->transports);

    if (head) {
      list_add_tail(&curr->transports, &head->transports);
    } else {
      head = curr;
    }
  }

  return head;

_error_exit:
  if (head) {
    list_for_each_entry_safe(head, curr, &head->transports, transports){
      free(head);
    }
  }
  return NULL;
}