/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_EVL_POLL_H
#define _ASM_GENERIC_EVL_POLL_H

#ifdef CONFIG_EVL

#include <evl/poll_head.h>

/*
 * Poll operation descriptor for f_op->oob_poll.  Can be attached
 * concurrently to at most EVL_POLL_NR_CONNECTORS poll heads.
 */
#define EVL_POLL_NR_CONNECTORS  4

struct oob_poll_wait {
	struct evl_poll_connector {
		struct evl_poll_head *head;
		struct list_head next;
		void (*unwatch)(struct evl_poll_head *head);
		int events_received;
		int index;
	} connectors[EVL_POLL_NR_CONNECTORS];
};

struct oob_poll_queue {
	struct evl_poll_head head;
};

static inline
void init_oob_poll_queue(struct oob_poll_queue *pwq)
{
	evl_init_poll_head(&pwq->head);
}

#define poll_signal_oob(__pwq, __mask)	\
	evl_signal_poll_events(&(__pwq)->head, __mask)

#define poll_wait_oob(__pwq, __wait)	\
	evl_poll_watch(&(__pwq)->head, __wait, NULL)

#else

struct oob_poll_wait {
};

struct oob_poll_queue {
};

static inline void init_oob_poll_queue(struct oob_poll_queue *pwq) { }

#define poll_signal_oob(__pwq, __mask)	\
	do { (void)(__pwq), (void)(__mask); } while (0)

#define poll_wait_oob(__pwq, __wait)	\
	do { (void)(__pwq), (void)(__wait); } while (0)

#endif	/* !CONFIG_EVL */

#endif /* !_ASM_GENERIC_EVL_POLL_H */
