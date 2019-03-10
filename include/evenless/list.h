/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2013, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_LIST_H
#define _EVENLESS_LIST_H

#include <linux/list.h>

#define __list_add_pri(__new, __head, __member_pri, __member_next, __relop)	\
do {										\
	typeof(*__new) *__pos;							\
	if (list_empty(__head))							\
		list_add(&(__new)->__member_next, __head);		 	\
	else {									\
		list_for_each_entry_reverse(__pos, __head, __member_next) {	\
			if ((__new)->__member_pri __relop __pos->__member_pri)	\
				break;						\
		}								\
		list_add(&(__new)->__member_next, &__pos->__member_next); 	\
	}									\
} while (0)

#define list_add_priff(__new, __head, __member_pri, __member_next)		\
	__list_add_pri(__new, __head, __member_pri, __member_next, <=)

#define list_add_prilf(__new, __head, __member_pri, __member_next)		\
	__list_add_pri(__new, __head, __member_pri, __member_next, <)

#define list_get_entry(__head, __type, __member)		\
  ({								\
	  __type *__item;					\
	  __item = list_first_entry(__head, __type, __member);	\
	  list_del(&__item->__member);				\
	  __item;						\
  })

#ifndef list_next_entry
#define list_next_entry(__item, __member)			\
	list_entry((__item)->__member.next, typeof(*(__item)), __member)
#endif

#endif /* !_EVENLESS_LIST_H_ */
