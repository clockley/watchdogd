/*
 * Copyright © 2010-2012 Intel Corporation
 * Copyright © 2010 Francisco Jerez <currojerez@riseup.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "watchdogd.h"

#ifndef LIST_H_
#define LIST_H_

/**
 * The linkage struct for list nodes. This struct must be part of your
 * to-be-linked struct. struct list is required for both the head of the
 * list and for each list node.
 *
 * Position and name of the struct list field is irrelevant.
 * There are no requirements that elements of a list are of the same type.
 * There are no requirements for a list head, any struct list can be a list
 * head.
 */
struct list {
	struct list *next, *prev;
};

bool list_is_empty(const struct list *head);
void list_move_tail(struct list *list, struct list *head);
void list_move(struct list *list, struct list *head);
void list_del(struct list *entry);
void list_append(struct list *entry, struct list *head);
void list_replace(struct list *old, struct list *_new);
void list_add_tail(struct list *entry, struct list *head);
void list_init(struct list *list);
void list_add(struct list *entry, struct list *head);

/**
 * @file Classic doubly-link circular list implementation.
 * For real usage examples of the linked list, see the file test/list.c
 *
 * Example:
 * We need to keep a list of struct foo in the parent struct bar, i.e. what
 * we want is something like this.
 *
 *     struct bar {
 *          ...
 *          struct foo *list_of_foos; -----> struct foo {}, struct foo {}, struct foo{}
 *          ...
 *     }
 *
 * We need one list head in bar and a list element in all list_of_foos (both are of
 * data type 'struct list').
 *
 *     struct bar {
 *          ...
 *          struct list list_of_foos;
 *          ...
 *     }
 *
 *     struct foo {
 *          ...
 *          struct list entry;
 *          ...
 *     }
 *
 * Now we initialize the list head:
 *
 *     struct bar bar;
 *     ...
 *     list_init(&bar.list_of_foos);
 *
 * Then we create the first element and add it to this list:
 *
 *     struct foo *foo = malloc(...);
 *     ....
 *     list_add(&foo->entry, &bar.list_of_foos);
 *
 * Repeat the above for each element you want to add to the list. Deleting
 * works with the element itself.
 *      list_del(&foo->entry);
 *      free(foo);
 *
 * Note: calling list_del(&bar.list_of_foos) will set bar.list_of_foos to an empty
 * list again.
 *
 * Looping through the list requires a 'struct foo' as iterator and the
 * name of the field the subnodes use.
 *
 * struct foo *iterator;
 * list_for_each_entry(iterator, &bar.list_of_foos, entry) {
 *      if (iterator->something == ...)
 *             ...
 * }
 *
 * Note: You must not call list_del() on the iterator if you continue the
 * loop. You need to run the safe for-each loop instead:
 *
 * struct foo *iterator, *next;
 * list_for_each_entry_safe(iterator, next, &bar.list_of_foos, entry) {
 *      if (...)
 *              list_del(&iterator->entry);
 * }
 *
 */

/**
 * Alias of container_of
 */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

/**
 * Retrieve the first list entry for the given list pointer.
 *
 * Example:
 * struct foo *first;
 * first = list_first_entry(&bar->list_of_foos, struct foo, list_of_foos);
 *
 * @param ptr The list head
 * @param type Data type of the list element to retrieve
 * @param member Member name of the struct list field in the list element.
 * @return A pointer to the first list element.
 */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

/**
 * Retrieve the last list entry for the given listpointer.
 *
 * Example:
 * struct foo *first;
 * first = list_last_entry(&bar->list_of_foos, struct foo, list_of_foos);
 *
 * @param ptr The list head
 * @param type Data type of the list element to retrieve
 * @param member Member name of the struct list field in the list element.
 * @return A pointer to the last list element.
 */

#ifndef __cplusplus
#define decltype __typeof__
#endif

#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

#define __container_of(ptr, sample, member)				\
    (decltype(sample))(void*)((char *)(ptr)						\
	     - ((char *)&(sample)->member - (char *)(sample)))

/**
 * Loop through the list, keeping a backup pointer to the element. This
 * macro allows for the deletion of a list element while looping through the
 * list.
 *
 * See list_for_each_entry for more details.
 */

#define list_for_each_entry(pos, tmp, head, member)		\
    for (pos = __container_of((head)->next, pos, member),		\
	 tmp = __container_of(pos->member.next, pos, member);		\
	 &pos->member != (head);					\
	 pos = tmp, tmp = __container_of(pos->member.next, tmp, member))

#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

#define container_of(ptr, type, member) ({ \
                decltype( ((type *)0)->member ) *__mptr = (ptr); \
                (type *)( (char *)__mptr - offsetof(type,member));})
#endif				/* LIST_H_ */
