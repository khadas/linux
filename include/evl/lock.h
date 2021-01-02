/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Philippe Gerum <rpm@xenomai.org>
 */
#ifndef _EVL_LOCK_H
#define _EVL_LOCK_H

#include <evl/sched.h>

/*
 * A (hard) spinlock API which also deals with thread preemption
 * disabling in the EVL core. Such spinlock may be useful when only
 * EVL threads running out-of-band can contend for the lock, to the
 * exclusion of out-of-band IRQ handlers. In this case, disabling
 * preemption before attempting to grab the lock may be substituted to
 * disabling hard irqs.
 *
 * In other words, if you can guarantee that lock X can only be taken
 * from out-of-band EVL thread contexts, then the two locking forms
 * below would guard the section safely, without the extra cost of
 * masking out-of-band IRQs in the second form:
 *
 *     hard_spinlock_t X;
 *     ...
 *     unsigned long flags;
 *     raw_spin_lock_irqsave(&X, flags);
 *     (guarded section)
 *     raw_spin_unlock_irqrestore(&X, flags);
 *
 *     ------------------------------------------------
 *
 *     evl_spinlock_t X;
 *     ...
 *     evl_spin_lock(&X);     -- disables preemption
 *     (guarded section)
 *     evl_spin_unlock(&X);   -- enables preemption, may resched
 *
 * In this case, picking the right type of lock is a matter of
 * trade-off between interrupt latency and scheduling latency,
 * depending on how time-critical it is for some IRQ handler to
 * execute despite any request for rescheduling the latter might issue
 * would have to wait until the lock is dropped by the interrupted
 * thread eventually.
 *
 * Since an EVL spinlock is a hard lock at its core, you may also use
 * it to serialize access to data from the in-band context. However,
 * because such code would also be subject to preemption by the
 * in-band scheduler which might impose a severe priority inversion on
 * out-of-band threads spinning on the same lock from other CPU(s),
 * any attempt to grab an EVL lock from the in-band stage without
 * stalling such stage or disabling hard irqs is considered a bug.
 *
 * Just like the in-band preempt_count(), the EVL preemption count
 * which guards against unwanted rescheduling from the core allows
 * evl_spinlock_t locks to be nested safely.
 */

typedef struct evl_spinlock {
	hard_spinlock_t _lock;
} evl_spinlock_t;

#define __EVL_SPIN_LOCK_INITIALIZER(__lock)	{			\
		._lock = __HARD_SPIN_LOCK_INITIALIZER((__lock)._lock),	\
	}

#define DEFINE_EVL_SPINLOCK(__lock)	\
	evl_spinlock_t __lock = __EVL_SPIN_LOCK_INITIALIZER(__lock)

#define evl_spin_lock_init(__lock)	\
	raw_spin_lock_init(&(__lock)->_lock)

#define __evl_spin_lock(__lock)				\
	do {						\
		evl_disable_preempt();			\
		raw_spin_lock(&(__lock)->_lock);	\
	} while (0)

#define __evl_spin_lock_nested(__lock, __subclass)			\
	do {								\
		evl_disable_preempt();					\
		raw_spin_lock_nested(&(__lock)->_lock, __subclass);	\
	} while (0)

#define __evl_spin_trylock(__lock)			\
	({						\
		evl_disable_preempt();			\
		raw_spin_trylock(&(__lock)->_lock);	\
	})

#ifdef CONFIG_EVL_DEBUG_CORE
#define _evl_spin_lock_check()				\
	WARN_ON_ONCE(!(running_oob() ||			\
			irqs_disabled() ||		\
			hard_irqs_disabled()))
#define _evl_spin_lock(__lock)				\
	do {						\
		_evl_spin_lock_check();			\
		__evl_spin_lock(__lock);		\
	} while (0)

#define _evl_spin_lock_nested(__lock, __subclass)		\
	do {							\
		_evl_spin_lock_check();				\
		__evl_spin_lock_nested(__lock, __subclass);	\
	} while (0)

#define _evl_spin_trylock(__lock)			\
	({						\
		_evl_spin_lock_check();			\
		__evl_spin_trylock(__lock);		\
	})
#define evl_spin_lock(__lock)		_evl_spin_lock(__lock)
#define evl_spin_lock_nested(__lock)	_evl_spin_lock_nested(__lock)
#define evl_spin_trylock(__lock)	_evl_spin_trylock(__lock)
#else
#define evl_spin_lock(__lock)		__evl_spin_lock(__lock)
#define evl_spin_lock_nested(__lock)	__evl_spin_lock_nested(__lock)
#define evl_spin_trylock(__lock)	__evl_spin_trylock(__lock)
#endif	/* !CONFIG_EVL_DEBUG_CORE */

#define evl_spin_lock_irq(__lock)			\
	do {						\
		evl_disable_preempt();			\
		raw_spin_lock_irq(&(__lock)->_lock);	\
	} while (0)

#define evl_spin_unlock_irq(__lock)			\
	do {						\
		raw_spin_unlock_irq(&(__lock)->_lock);	\
		evl_enable_preempt();			\
	} while (0)

#define evl_spin_lock_irqsave(__lock, __flags)				\
	do {								\
		evl_disable_preempt();					\
		raw_spin_lock_irqsave(&(__lock)->_lock, __flags);	\
	} while (0)

#define evl_spin_unlock(__lock)				\
	do {						\
		raw_spin_unlock(&(__lock)->_lock);	\
		evl_enable_preempt();			\
	} while (0)

#define evl_spin_unlock_irqrestore(__lock, __flags)			\
	do {								\
		raw_spin_unlock_irqrestore(&(__lock)->_lock, __flags);	\
		evl_enable_preempt();					\
	} while (0)

#endif /* !_EVL_LOCK_H */
