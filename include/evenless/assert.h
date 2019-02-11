/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2006, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_ASSERT_H
#define _EVENLESS_ASSERT_H

#include <linux/kconfig.h>

#define EVL_INFO	KERN_INFO    "EVL: "
#define EVL_WARNING	KERN_WARNING "EVL: "
#define EVL_ERR		KERN_ERR     "EVL: "

#define EVL_DEBUG(__subsys)				\
	IS_ENABLED(CONFIG_EVENLESS_DEBUG_##__subsys)
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
#define EVL_WARN_ON_SMP(__subsys, __cond)		\
	do { } while (0)
#endif

#define oob_context_only()	EVL_WARN_ON_ONCE(CONTEXT, running_inband())
#define inband_context_only()	EVL_WARN_ON_ONCE(CONTEXT, !running_inband())

/* TEMP: needed until we have gotten rid of the infamous nklock. */
#define requires_ugly_lock()	WARN_ON_ONCE(!(xnlock_is_owner(&nklock) && hard_irqs_disabled()))
#define no_ugly_lock()		WARN_ON_ONCE(xnlock_is_owner(&nklock))

#endif /* !_EVENLESS_ASSERT_H */
