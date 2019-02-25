/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Inherited from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2001-2008, 2018 Philippe Gerum <rpm@xenomai.org>
 * Copyright (C) 2004,2005 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
 *
 * NOTE: this code is on its way out, the ugly superlock is the only
 * remaining lock of this type. There is an ongoing effort for moving
 * away from the inefficient single-lock model. Do NOT add more
 * instances of such lock.
 */
#ifndef _EVENLESS_LOCK_H
#define _EVENLESS_LOCK_H

#include <linux/irq_pipeline.h>

#define splhigh(x)  ((x) = oob_irq_save() & 1)
#ifdef CONFIG_SMP
#define splexit(x)  oob_irq_restore(x & 1)
#else /* !CONFIG_SMP */
#define splexit(x)  oob_irq_restore(x)
#endif /* !CONFIG_SMP */

struct xnlock {
	unsigned owner;
	arch_spinlock_t alock;
};

#define XNARCH_LOCK_UNLOCKED			\
	(struct xnlock) {			\
		~0,				\
		__ARCH_SPIN_LOCK_UNLOCKED,	\
	}

#if defined(CONFIG_SMP)

#define xnlock_get(lock)		__xnlock_get(lock)
#define xnlock_put(lock)		__xnlock_put(lock)
#define xnlock_get_irqsave(lock,x) \
	((x) = __xnlock_get_irqsave(lock))
#define xnlock_put_irqrestore(lock,x) \
	__xnlock_put_irqrestore(lock,x)
#define xnlock_clear_irqoff(lock)	xnlock_put_irqrestore(lock, 1)
#define xnlock_clear_irqon(lock)	xnlock_put_irqrestore(lock, 0)

static inline void xnlock_init (struct xnlock *lock)
{
	*lock = XNARCH_LOCK_UNLOCKED;
}

#define DECLARE_XNLOCK(lock)		struct xnlock lock
#define DECLARE_EXTERN_XNLOCK(lock)	extern struct xnlock lock
#define DEFINE_XNLOCK(lock)		struct xnlock lock = XNARCH_LOCK_UNLOCKED
#define DEFINE_PRIVATE_XNLOCK(lock)	static DEFINE_XNLOCK(lock)

static inline int ____xnlock_get(struct xnlock *lock)
{
	int cpu = raw_smp_processor_id();

	if (lock->owner == cpu)
		return 2;

	arch_spin_lock(&lock->alock);
	lock->owner = cpu;

	return 0;
}

static inline void ____xnlock_put(struct xnlock *lock)
{
	lock->owner = ~0U;
	arch_spin_unlock(&lock->alock);
}

int ___xnlock_get(struct xnlock *lock);

void ___xnlock_put(struct xnlock *lock);

static inline unsigned long
__xnlock_get_irqsave(struct xnlock *lock)
{
	unsigned long flags;

	splhigh(flags);

	flags |= ___xnlock_get(lock);

	return flags;
}

static inline void __xnlock_put_irqrestore(struct xnlock *lock, unsigned long flags)
{
	/* Only release the lock if we didn't take it recursively. */
	if (!(flags & 2))
		___xnlock_put(lock);

	splexit(flags & 1);
}

static inline int xnlock_is_owner(struct xnlock *lock)
{
	return lock->owner == raw_smp_processor_id();
}

static inline int __xnlock_get(struct xnlock *lock)
{
	return ___xnlock_get(lock);
}

static inline void __xnlock_put(struct xnlock *lock)
{
	___xnlock_put(lock);
}

#else /* !CONFIG_SMP */

#define xnlock_init(lock)		do { } while(0)
#define xnlock_get(lock)		do { } while(0)
#define xnlock_put(lock)		do { } while(0)
#define xnlock_get_irqsave(lock,x)	splhigh(x)
#define xnlock_put_irqrestore(lock,x)	splexit(x)
#define xnlock_clear_irqoff(lock)	oob_irq_disable()
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
 * Conversely, when the protected section is guaranteed not to call
 * evl_schedule() either directly or indirectly, and the ugly nklock
 * is not nested, Dovetail's hard spinlock API may be used instead.
 *
 * Once the ugly lock is entirely dropped from the code, we may
 * manipulate pure hardware flags for interrupt masking instead of the
 * OOB stall bit, as we won't have to store any lock recursion marker
 * there anymore. IOW, splhigh/exit can be replaced by regular
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

#define evl_spin_unlock_irqrestore(__lock, __flags)	\
	do {						\
		raw_spin_unlock(&(__lock)->_lock);	\
		splexit(__flags);			\
		evl_enable_preempt();			\
	} while (0)

#endif /* !_EVENLESS_LOCK_H */
