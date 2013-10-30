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

void _list_del(void *arg_one);
void __list_del(void *arg_one, void *arg_two);
void __list_add(void *arg_one, void *arg_two, void *arg_three);

#include "watchdogd.h"

/**
 * Initialize the list as an empty list.
 *
 * Example:
 * list_init(&bar->list_of_foos);
 *
 * @param The list to initialized.
 */
void list_init(void *arg)
{
	struct list *list = arg;
	list->next = list->prev = list;
}

void __list_add(void *arg_one, void *arg_two, void *arg_three)
{
	struct list *entry = arg_one;
	struct list *prev = arg_two;
	struct list *next = arg_three;

	next->prev = entry;
	entry->next = next;
	entry->prev = prev;
	prev->next = entry;
}

/**
 * Insert a new element after the given list head. The new element does not
 * need to be initialised as empty list.
 * The list changes from:
 *      head → some element → ...
 * to
 *      head → new element → older element → ...
 *
 * Example:
 * struct foo *newfoo = malloc(...);
 * list_add(&newfoo->entry, &bar->list_of_foos);
 *
 * @param entry The new element to prepend to the list.
 * @param head The existing list.
 */
void list_add(void *arg_one, void *arg_two)
{
	struct list *entry = arg_one;
	struct list *head = arg_two;

	__list_add(entry, head, head->next);
}

void list_add_tail(void *arg_one, void *arg_two)
{
	struct list *entry = arg_one;
	struct list *head = arg_two;

	__list_add(entry, head->prev, head);
}

void list_replace(void *arg_one, void *arg_two)
{
	struct list *old = arg_one;
	struct list *new = arg_two;

	new->next = old->next;
	new->next->prev = new;
	new->prev = old->prev;
	new->prev->next = new;
}

#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

#define list_for_each(pos, head)				\
    for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * Append a new element to the end of the list given with this list head.
 *
 * The list changes from:
 *      head → some element → ... → lastelement
 * to
 *      head → some element → ... → lastelement → new element
 *
 * Example:
 * struct foo *newfoo = malloc(...);
 * list_append(&newfoo->entry, &bar->list_of_foos);
 *
 * @param entry The new element to prepend to the list.
 * @param head The existing list.
 */
void list_append(void *arg_one, void *arg_two)
{
	struct list *entry = arg_one;
	struct list *head = arg_two;
	__list_add(entry, head->prev, head);
}

void __list_del(void *arg_one, void *arg_two)
{
	struct list *prev = arg_one;
	struct list *next = arg_two;

	assert(next->prev == prev->next);
	next->prev = prev;
	prev->next = next;
}

void _list_del(void *arg_one)
{
	struct list *entry = arg_one;

	assert(entry->prev->next == entry);
	assert(entry->next->prev == entry);
	__list_del(entry->prev, entry->next);
}

/**
 * Remove the element from the list it is in. Using this function will reset
 * the pointers to/from this element so it is removed from the list. It does
 * NOT free the element itself or manipulate it otherwise.
 *
 * Using list_del on a pure list head (like in the example at the top of
 * this file) will NOT remove the first element from
 * the list but rather reset the list as empty list.
 *
 * Example:
 * list_del(&foo->entry);
 *
 * @param entry The element to remove.
 */
void list_del(void *arg_one)
{
	struct list *entry = arg_one;
	_list_del(entry);
	list_init(entry);
}

void list_move(void *arg_one, void *arg_two)
{
	struct list *list = arg_one;
	struct list *head = arg_two;

	if (list->prev != head) {
		_list_del(list);
		list_add(list, head);
	}
}

void list_move_tail(void *arg_one, void *arg_two)
{
	struct list *list = arg_one;
	struct list *head = arg_two;

	_list_del(list);
	list_add_tail(list, head);
}

/**
 * Check if the list is empty.
 *
 * Example:
 * list_is_empty(&bar->list_of_foos);
 *
 * @return True if the list contains one or more elements or False otherwise.
 */
bool list_is_empty(void *arg_one)
{
	const struct list *head = arg_one;

	return head->next == head;
}
