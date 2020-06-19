/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_EVL_POLL_H
#define _ASM_GENERIC_EVL_POLL_H

#ifdef CONFIG_EVL

#include <linux/list.h>

/*
 * Poll operation descriptor for f_op->oob_poll.  Can be attached
 * concurrently to at most EVL_POLL_NR_CONNECTORS poll heads.
 */
#define EVL_POLL_NR_CONNECTORS  4

struct evl_poll_head;

struct oob_poll_wait {
	struct evl_poll_connector {
		struct evl_poll_head *head;
		struct list_head next;
		void (*unwatch)(struct evl_poll_head *head);
		int events_received;
		int index;
	} connectors[EVL_POLL_NR_CONNECTORS];
};

#else

struct oob_poll_wait {
};

#endif	/* !CONFIG_EVL */

#endif /* !_ASM_GENERIC_EVL_POLL_H */
