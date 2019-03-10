/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_EVL_THREAD_INFO_H
#define _ASM_GENERIC_EVL_THREAD_INFO_H

struct evl_thread;

struct oob_thread_state {
	struct evl_thread *thread;
	int preempt_count;
};

static inline
void evl_init_thread_state(struct oob_thread_state *p)
{
	p->thread = NULL;
	p->preempt_count = 0;
}

#endif /* !_ASM_GENERIC_EVL_THREAD_INFO_H */
