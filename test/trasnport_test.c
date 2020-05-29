/*
 * @Author: Virus.V
 * @Date: 2020-05-25 18:52:18
 * @LastEditors: Virus.V
 * @LastEditTime: 2020-05-29 10:30:34
 * @Description: file content
 * @Email: virusv@live.com
 */

#include <stdio.h>

#define CTEST_MAIN

// uncomment lines below to enable/disable features. See README.md for details
#define CTEST_SEGFAULT
//#define CTEST_NO_COLORS
//#define CTEST_COLOR_OK

#include "ctest.h"
#include "Library/misc/list.h"

int main(int argc, const char *argv[]) { return ctest_main(argc, argv); }

struct transport {
  struct list_head transports; // 传输模式链表
};

struct transport *adapterCreateTransport(int count) {
  struct transport *head = NULL, *curr = NULL;
  int i;

  assert(count > 0);

  for (i = 0; i < count; i++) {
    curr = calloc(1, sizeof(struct transport));
    if (!curr) {
      goto _error_exit;
    }

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


CTEST(object, test_transport_object) {

}
