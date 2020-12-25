/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2006, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_ASSERT_H
#define _EVL_ASSERT_H

#include <linux/kconfig.h>
#include <linux/irqstage.h>

#define EVL_INFO	KERN_INFO    "EVL: "
#define EVL_WARNING	KERN_WARNING "EVL: "
#define EVL_ERR		KERN_ERR     "EVL: "

#define EVL_DEBUG(__subsys)				\
	IS_ENABLED(CONFIG_EVL_DEBUG_##__subsys)
#define EVL_ASSERT(__subsys, __cond)			\
	(!WARN_ON(EVL_DEBUG(__subsys) && !(__cond)))
#define EVL_WARN(__subsys, __cond, __fmt...)		\
	WARN(EVL_DEBUG(__subsys) && (__cond), __fmt)
#define EVL_WARN_ON(__subsys, __cond)			\
	WARN_ON(EVL_DEBUG(__subsys) && (__cond))
#define EVL_WARN_ON_ONCE(__subsys, __cond)		\
	WARN_ON_ONCE(EVL_DEBUG(__subsys) && (__cond))
#ifdef CONFIG_SMP
#define EVL_WARN_ON_SMP(__subsys, __cond)		\
	EVL_WARN_ON(__subsys, __cond)
#else
#define EVL_WARN_ON_SMP(__subsys, __cond)  0
#endif

#define oob_context_only()       EVL_WARN_ON_ONCE(CORE, running_inband())
#define inband_context_only()    EVL_WARN_ON_ONCE(CORE, !running_inband())
#ifdef CONFIG_SMP
#define assert_hard_lock(__lock) EVL_WARN_ON_ONCE(CORE, \
				!(raw_spin_is_locked(__lock) && hard_irqs_disabled()))
#define assert_evl_lock(__lock) assert_hard_lock(&(__lock)->_lock)
#else
#define assert_hard_lock(__lock) EVL_WARN_ON_ONCE(CORE, !hard_irqs_disabled())
#define assert_evl_lock(__lock) assert_hard_lock(&(__lock)->_lock)
#endif

#define assert_thread_pinned(__thread)			\
	do {						\
		assert_hard_lock(&(__thread)->lock);	\
		assert_hard_lock(&(__thread)->rq->lock);\
	} while (0)

#endif /* !_EVL_ASSERT_H */
