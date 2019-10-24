/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2001-2008, 2018 Philippe Gerum <rpm@xenomai.org>
 * Copyright (C) 2004,2005 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
 *
 * NOTE: this code is on its way out, the ugly superlock is the only
 * remaining lock of this type. There is an ongoing effort for moving
 * away from the inefficient single-lock model. Do NOT add more
 * instances of such lock.
 */
#ifndef _EVL_LOCK_H
#define _EVL_LOCK_H

#include <linux/irq_pipeline.h>

#define splhigh(x)  ((x) = oob_irq_save())
#ifdef CONFIG_SMP
#define splexit(x)  oob_irq_restore(x)
#else /* !CONFIG_SMP */
#define splexit(x)  oob_irq_restore(x)
#endif /* !CONFIG_SMP */

struct xnlock {
	unsigned owner;
	unsigned int nesting;
	arch_spinlock_t alock;
};

#define XNARCH_LOCK_UNLOCKED			\
	(struct xnlock) {			\
		~0,				\
		0,				\
		__ARCH_SPIN_LOCK_UNLOCKED,	\
	}

#if defined(CONFIG_SMP)

#define xnlock_get(lock)		__xnlock_get(lock)
#define xnlock_put(lock)		__xnlock_put(lock)
#define xnlock_get_irqsave(lock,x) \
	((x) = __xnlock_get_irqsave(lock))
#define xnlock_put_irqrestore(lock,x) \
	__xnlock_put_irqrestore(lock,x)

static inline void xnlock_init (struct xnlock *lock)
{
	*lock = XNARCH_LOCK_UNLOCKED;
}

#define DECLARE_XNLOCK(lock)		struct xnlock lock
#define DECLARE_EXTERN_XNLOCK(lock)	extern struct xnlock lock
#define DEFINE_XNLOCK(lock)		struct xnlock lock = XNARCH_LOCK_UNLOCKED
#define DEFINE_PRIVATE_XNLOCK(lock)	static DEFINE_XNLOCK(lock)

static inline void __xnlock_get(struct xnlock *lock)
{
	int cpu = raw_smp_processor_id();

	if (lock->owner == cpu) {
		lock->nesting++;
		return;
	}

	arch_spin_lock(&lock->alock);
	lock->owner = cpu;
	lock->nesting = 1;
}

static inline void __xnlock_put(struct xnlock *lock)
{
	WARN_ON_ONCE(lock->nesting <= 0);

	if (--lock->nesting == 0) {
		lock->owner = ~0U;
		arch_spin_unlock(&lock->alock);
	}
}

static inline unsigned long
__xnlock_get_irqsave(struct xnlock *lock)
{
	unsigned long flags;

	splhigh(flags);
	__xnlock_get(lock);
	return flags;
}

static inline
void __xnlock_put_irqrestore(struct xnlock *lock, unsigned long flags)
{
	__xnlock_put(lock);
	splexit(flags);
}

static inline void xnlock_clear_irqon(struct xnlock *lock)
{
	lock->nesting = 1;
	__xnlock_put(lock);
	splexit(0);
}

static inline int xnlock_is_owner(struct xnlock *lock)
{
	return lock->owner == raw_smp_processor_id();
}

#else /* !CONFIG_SMP */

#define xnlock_init(lock)		do { } while(0)
#define xnlock_get(lock)		do { } while(0)
#define xnlock_put(lock)		do { } while(0)
#define xnlock_get_irqsave(lock,x)	splhigh(x)
#define xnlock_put_irqrestore(lock,x)	splexit(x)
#define xnlock_clear_irqon(lock)	oob_irq_enable()
#define xnlock_is_owner(lock)		1

#define DECLARE_XNLOCK(lock)
#define DECLARE_EXTERN_XNLOCK(lock)
#define DEFINE_XNLOCK(lock)
#define DEFINE_PRIVATE_XNLOCK(lock)

#endif /* !CONFIG_SMP */

DECLARE_EXTERN_XNLOCK(nklock);

/*
 * This new spinlock API enforces EVL preemption disabling on lock,
 * and may be nested with the obsolete ugly lock API. Specifically,
 * this lock _must_ be used in patterns like follows:
 *
 * evl_spin_lock--(&lock[, flags_outer]);
 *    xnlock_get--(&nklock[, flags_inner]);
 *    xnlock_put--(&nklock[, flags_inner]);
 * evl_spin_unlock--(&lock[, flags_outer]);
 *
 * In addition to preventing unwanted rescheduling, this construct
 * makes sure xnlock_put_irq* will not re-enable irqs unexpectedly, by
 * having evl_spin_lock_irq* update the OOB stall bit, which the
 * xnlock* API tests and saves.
 *
 * Conversely, when the protected section is guaranteed not to call
 * evl_schedule() either directly or indirectly, does not traverse
 * code altering the OOB stall bit (including as a consequence of
 * nesting the ugly nklock), Dovetail's hard spinlock API may be used
 * instead.
 *
 * Once the ugly lock is dropped, we may manipulate hardware flags for
 * interrupt masking directly instead of maintaining the consistency
 * of the OOB stall bit, replacing splhigh/exit by regular
 * hard_local_irqsave/restore pairs.
 */

typedef struct evl_spinlock {
	hard_spinlock_t _lock;
} evl_spinlock_t;

#define __EVL_SPIN_LOCK_INITIALIZER(__lock)	{			\
		._lock = __HARD_SPIN_LOCK_INITIALIZER((__lock)._lock),	\
	}

#define DEFINE_EVL_SPINLOCK(__lock)					\
	evl_spinlock_t __lock = {					\
		._lock = __HARD_SPIN_LOCK_INITIALIZER((__lock)._lock),	\
	}

#define evl_spin_lock_init(__lock)	raw_spin_lock_init(&(__lock)->_lock)

#define evl_spin_lock(__lock)				\
	do {						\
		evl_disable_preempt();			\
		raw_spin_lock(&(__lock)->_lock);	\
	} while (0)

#define evl_spin_lock_nested(__lock, __subclass)			\
	do {								\
		evl_disable_preempt();					\
		raw_spin_lock_nested(&(__lock)->_lock, __subclass);	\
	} while (0)

#define evl_spin_trylock(__lock)				\
	({							\
		int __ret;					\
		evl_disable_preempt();				\
		__ret = raw_spin_trylock(&(__lock)->_lock);	\
		if (!__ret)					\
			evl_enable_preempt();			\
		__ret;						\
	})

#define evl_spin_lock_irq(__lock)			\
	do {						\
		evl_disable_preempt();			\
		oob_irq_disable();			\
		raw_spin_lock(&(__lock)->_lock);	\
	} while (0)

#define evl_spin_lock_irqsave(__lock, __flags)		\
	do {						\
		splhigh(__flags);			\
		evl_spin_lock(__lock);			\
	} while (0)

#define evl_spin_unlock(__lock)				\
	do {						\
		raw_spin_unlock(&(__lock)->_lock);	\
		evl_enable_preempt();			\
	} while (0)

#define evl_spin_unlock_irq(__lock)			\
	do {						\
		raw_spin_unlock(&(__lock)->_lock);	\
		oob_irq_enable();			\
		evl_enable_preempt();			\
	} while (0)

#define evl_spin_unlock_irqrestore(__lock, __flags)	\
	do {						\
		raw_spin_unlock(&(__lock)->_lock);	\
		splexit(__flags);			\
		evl_enable_preempt();			\
	} while (0)

#endif /* !_EVL_LOCK_H */
