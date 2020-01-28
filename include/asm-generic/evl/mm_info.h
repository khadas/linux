/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_EVL_MM_INFO_H
#define _ASM_GENERIC_EVL_MM_INFO_H

#ifdef CONFIG_EVL

#include <linux/list.h>

#define EVL_MM_PTSYNC_BIT  0
#define EVL_MM_ACTIVE_BIT  30
#define EVL_MM_INIT_BIT    31

struct evl_wait_queue;

struct oob_mm_state {
	unsigned long flags;	/* Guaranteed zero initially. */
	struct list_head ptrace_sync;
	struct evl_wait_queue *ptsync_barrier;
};

#else

struct oob_mm_state { };

#endif	/* !CONFIG_EVL */

#endif /* !_ASM_GENERIC_EVL_MM_INFO_H */
