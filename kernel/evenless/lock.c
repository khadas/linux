/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Inherited from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2004, 2005 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
 * Copyright (C) 2010, 2018 Philippe Gerum  <rpm@xenomai.org>
 *
 * NOTE: this code is on its way out, the ugly superlock is the only
 * remaining lock of this type. Moving away from the inefficient
 * single-lock model is planned.
 */

#include <linux/module.h>
#include <linux/sched/debug.h>
#include <evenless/lock.h>
#include <evenless/clock.h>

DEFINE_XNLOCK(nklock);
#if defined(CONFIG_SMP)
EXPORT_SYMBOL_GPL(nklock);

int ___xnlock_get(struct xnlock *lock)
{
	return ____xnlock_get(lock);
}
EXPORT_SYMBOL_GPL(___xnlock_get);

void ___xnlock_put(struct xnlock *lock)
{
	____xnlock_put(lock);
}
EXPORT_SYMBOL_GPL(___xnlock_put);

#endif /* CONFIG_SMP */
