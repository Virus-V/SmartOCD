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

#define MAX_LIST_LENGTH_BITS 20
typedef int (*cmp_cb)(void *priv, struct list_head *a, struct list_head *b);

// 所有component链表
LIST_HEAD(components);

// 注册Component
void component_regist(struct componentEntry *c) {
  list_add(&c->entry, &components);
}
/*
 * Returns a list organized in an intermediate format suited
 * to chaining of merge() calls: null-terminated, no reserved or
 * sentinel head node, "prev" links not maintained.
 */
static struct list_head *merge(void *priv, cmp_cb cmp, struct list_head *a, struct list_head *b) {
  struct list_head head, *tail = &head;

  while (a && b) {
    /* if equal, take 'a' -- important for sort stability */
    if ((*cmp)(priv, a, b) <= 0) {
      tail->next = a;
      a = a->next;
    } else {
      tail->next = b;
      b = b->next;
    }
    tail = tail->next;
  }
  tail->next = a ?: b;
  return head.next;
}

/*
 * Combine final list merge with restoration of standard doubly-linked
 * list structure.  This approach duplicates code from merge(), but
 * runs faster than the tidier alternatives of either a separate final
 * prev-link restoration pass, or maintaining the prev links
 * throughout.
 */
static void merge_and_restore_back_links(void *priv, cmp_cb cmp, struct list_head *head, struct list_head *a, struct list_head *b) {
  struct list_head *tail = head;
  int count = 0;

  while (a && b) {
    /* if equal, take 'a' -- important for sort stability */
    if ((*cmp)(priv, a, b) <= 0) {
      tail->next = a;
      a->prev = tail;
      a = a->next;
    } else {
      tail->next = b;
      b->prev = tail;
      b = b->next;
    }
    tail = tail->next;
  }
  tail->next = a ?: b;

  do {
    /*
     * In worst cases this loop may run many iterations.
     * Continue callbacks to the client even though no
     * element comparison is needed, so the client's cmp()
     * routine can invoke cond_resched() periodically.
     */
    if (!(++count))
      (*cmp)(priv, tail->next, tail->next);

    tail->next->prev = tail;
    tail = tail->next;
  } while (tail->next);

  tail->next = head;
  head->prev = tail;
}

/**
 * list_sort - sort a list
 * @priv: private data, opaque to list_sort(), passed to @cmp
 * @head: the list to sort
 * @cmp: the elements comparison function
 *
 * This function implements "merge sort", which has O(nlog(n))
 * complexity.
 *
 * The comparison function @cmp must return a negative value if @a
 * should sort before @b, and a positive value if @a should sort after
 * @b. If @a and @b are equivalent, and their original relative
 * ordering is to be preserved, @cmp must return 0.
 */
void list_sort(void *priv, struct list_head *head, cmp_cb cmp) {
  /* sorted partial lists last slot is a sentinel index into part[] */
  struct list_head *part[MAX_LIST_LENGTH_BITS + 1];

  int lev;
  int max_lev = 0;
  struct list_head *list;

  if (list_empty(head))
    return;

  memset(part, 0, sizeof(part));

  head->prev->next = NULL;
  list = head->next;

  while (list) {
    struct list_head *cur = list;
    list = list->next;
    cur->next = NULL;

    for (lev = 0; part[lev]; lev++) {
      cur = merge(priv, cmp, part[lev], cur);
      part[lev] = NULL;
    }
    if (lev > max_lev) {
      if (lev >= MAX_LIST_LENGTH_BITS) {
        log_warn("Component list too long for efficiency\n");
        lev--;
      }
      max_lev = lev;
    }
    part[lev] = cur;
  }

  for (lev = 0; lev < max_lev; lev++)
    if (part[lev])
      list = merge(priv, cmp, part[lev], list);

  merge_and_restore_back_links(priv, cmp, head, part[max_lev], list);
}

static int priority_cmp(void *priv, struct list_head *a, struct list_head *b) {
  struct componentEntry *compa, *compb;
  (void)priv;

  compa = list_entry(a, struct componentEntry, entry);
  compb = list_entry(b, struct componentEntry, entry);

  return compa->priority - compb->priority;
}

// 初始化Components
void component_init(lua_State *L) {
  int ret;
  struct componentEntry *curr;

  assert(L != NULL);

  list_sort(NULL, &components, priority_cmp);

  list_for_each_entry(curr, &components, entry) {
    log_info("Regist Lua API: %s.", curr->name);
    ret = (*curr->init)(L, curr->opaque);
    if (ret != 0) {
      log_fatal("Regist %s failed!", curr->name);
      FATAL_ABORT(1);
    }
  }
}
