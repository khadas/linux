/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2004, 2005 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
 * Copyright (C) 2010, 2018 Philippe Gerum  <rpm@xenomai.org>
 *
 * NOTE: this code is on its way out, the ugly superlock is the only
 * remaining lock of this type. We are moving away from the
 * inefficient single-lock model.
 */

#include <linux/module.h>
#include <evl/lock.h>

DEFINE_XNLOCK(nklock);
#if defined(CONFIG_SMP)
EXPORT_SYMBOL_GPL(nklock);
#endif /* CONFIG_SMP */
