/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Inherited from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2001-2008, 2018 Philippe Gerum <rpm@xenomai.org>
 * Copyright (C) 2004,2005 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
 *
 * NOTE: this code is on its way out, the ugly superlock is the only
 * remaining lock of this type. Moving away from the inefficient
 * single-lock model is planned.
 */
#ifndef _EVENLESS_LOCK_H
#define _EVENLESS_LOCK_H

#include <linux/irq_pipeline.h>
#include <linux/percpu.h>
#include <linux/ktime.h>
#include <evenless/assert.h>

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

#endif /* !_EVENLESS_LOCK_H */
