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
#if defined(CONFIG_SMP) || defined(CONFIG_EVENLESS_DEBUG_LOCKING)
EXPORT_SYMBOL_GPL(nklock);

int ___xnlock_get(struct xnlock *lock /*, */ T_LOCK_DBG_CONTEXT_ARGS)
{
	return ____xnlock_get(lock /* , */ T_LOCK_DBG_PASS_CONTEXT);
}
EXPORT_SYMBOL_GPL(___xnlock_get);

void ___xnlock_put(struct xnlock *lock /*, */ T_LOCK_DBG_CONTEXT_ARGS)
{
	____xnlock_put(lock /* , */ T_LOCK_DBG_PASS_CONTEXT);
}
EXPORT_SYMBOL_GPL(___xnlock_put);
#endif /* CONFIG_SMP || CONFIG_EVENLESS_DEBUG_LOCKING */

#ifdef CONFIG_EVENLESS_DEBUG_LOCKING

DEFINE_PER_CPU(struct xnlockinfo, xnlock_stats);
EXPORT_PER_CPU_SYMBOL_GPL(xnlock_stats);

void xnlock_dbg_prepare_acquire(ktime_t *start)
{
	*start = evl_read_clock(&evl_mono_clock);
}
EXPORT_SYMBOL_GPL(xnlock_dbg_prepare_acquire);

void xnlock_dbg_acquired(struct xnlock *lock, int cpu, ktime_t *start,
			const char *file, int line, const char *function)
{
	lock->lock_date = *start;
	lock->spin_time = ktime_sub(evl_read_clock(&evl_mono_clock), *start);
	lock->file = file;
	lock->function = function;
	lock->line = line;
	lock->cpu = cpu;
}
EXPORT_SYMBOL_GPL(xnlock_dbg_acquired);

int xnlock_dbg_release(struct xnlock *lock,
		const char *file, int line, const char *function)
{
	struct xnlockinfo *stats;
	ktime_t lock_time;
	int cpu;

	lock_time = ktime_sub(evl_read_clock(&evl_mono_clock), lock->lock_date);
	cpu = raw_smp_processor_id();
	stats = &per_cpu(xnlock_stats, cpu);

	if (lock->file == NULL) {
		lock->file = "??";
		lock->line = 0;
		lock->function = "invalid";
	}

	if (unlikely(lock->owner != cpu)) {
		printk(EVL_ERR "lock %p already unlocked on CPU #%d\n"
			"          last owner = %s:%u (%s(), CPU #%d)\n",
			lock, cpu, lock->file, lock->line, lock->function,
			lock->cpu);
		show_stack(NULL,NULL);
		return 1;
	}

	/* File that we released it. */
	lock->cpu = -lock->cpu;
	lock->file = file;
	lock->line = line;
	lock->function = function;

	if (lock_time > stats->lock_time) {
		stats->lock_time = lock_time;
		stats->spin_time = lock->spin_time;
		stats->file = lock->file;
		stats->function = lock->function;
		stats->line = lock->line;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(xnlock_dbg_release);

#endif
