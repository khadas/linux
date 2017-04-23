/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_EVENLESS_THREAD_INFO_H
#define _ASM_GENERIC_EVENLESS_THREAD_INFO_H

struct evl_thread;

struct oob_thread_state {
	struct evl_thread *thread;
};

static inline
void evl_init_thread_state(struct oob_thread_state *p)
{
	p->thread = NULL;
}

#endif /* !_ASM_GENERIC_EVENLESS_THREAD_INFO_H */
