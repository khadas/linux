/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2019 Philippe Gerum <rpm@xenomai.org>
 */
#ifndef _EVL_LOCK_H
#define _EVL_LOCK_H

#include <linux/irq_pipeline.h>

/*
 * The spinlock API used in the EVL core, which preserves Dovetail's
 * stall bit for the out-of-band stage.
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

#define evl_spin_lock(__lock)		\
	raw_spin_lock(&(__lock)->_lock)

#define evl_spin_lock_nested(__lock, __subclass)	\
	raw_spin_lock_nested(&(__lock)->_lock, __subclass)

#define evl_spin_trylock(__lock)			\
	raw_spin_trylock(&(__lock)->_lock)

#define evl_spin_lock_irq(__lock)			\
	do {						\
		oob_irq_disable();			\
		raw_spin_lock(&(__lock)->_lock);	\
	} while (0)

#define evl_spin_unlock_irq(__lock)			\
	do {						\
		raw_spin_unlock(&(__lock)->_lock);	\
		oob_irq_enable();			\
	} while (0)

#define evl_spin_lock_irqsave(__lock, __flags)		\
	do {						\
		(__flags) = oob_irq_save();		\
		evl_spin_lock(__lock);			\
	} while (0)

#define evl_spin_unlock(__lock)				\
	raw_spin_unlock(&(__lock)->_lock)

#define evl_spin_unlock_irqrestore(__lock, __flags)	\
	do {						\
		raw_spin_unlock(&(__lock)->_lock);	\
		oob_irq_restore(__flags);		\
	} while (0)

#endif /* !_EVL_LOCK_H */
