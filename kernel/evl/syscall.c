/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2005-2020 Philippe Gerum  <rpm@xenomai.org>
 * Copyright (C) 2005 Gilles Chanteperdrix  <gilles.chanteperdrix@xenomai.org>
 */

#include <linux/types.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/dovetail.h>
#include <linux/kconfig.h>
#include <linux/nospec.h>
#include <linux/atomic.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/signal.h>
#include <evl/control.h>
#include <evl/thread.h>
#include <evl/timer.h>
#include <evl/monitor.h>
#include <evl/clock.h>
#include <evl/sched.h>
#include <evl/file.h>
#include <trace/events/evl.h>
#include <uapi/evl/syscall.h>
#include <asm/evl/syscall.h>

#define EVL_SYSCALL(__name, __args)		\
	long EVL_ ## __name __args

static EVL_SYSCALL(read, (int fd, char __user *u_buf, size_t size))
{
	struct evl_file *efilp = evl_get_file(fd);
	struct file *filp;
	ssize_t ret;

	if (efilp == NULL)
		return -EBADF;

	filp = efilp->filp;
	if (!(filp->f_mode & FMODE_READ)) {
		ret = -EBADF;
		goto out;
	}

	if (filp->f_op->oob_read == NULL) {
		ret = -EINVAL;
		goto out;
	}

	ret = filp->f_op->oob_read(filp, u_buf, size);
out:
	evl_put_file(efilp);

	return ret;
}

static EVL_SYSCALL(write, (int fd, const char __user *u_buf, size_t size))
{
	struct evl_file *efilp = evl_get_file(fd);
	struct file *filp;
	ssize_t ret;

	if (efilp == NULL)
		return -EBADF;

	filp = efilp->filp;
	if (!(filp->f_mode & FMODE_WRITE)) {
		ret = -EBADF;
		goto out;
	}

	if (filp->f_op->oob_write == NULL) {
		ret = -EINVAL;
		goto out;
	}

	ret = filp->f_op->oob_write(filp, u_buf, size);
out:
	evl_put_file(efilp);

	return ret;
}

static EVL_SYSCALL(ioctl, (int fd, unsigned int request, unsigned long arg))
{
	struct evl_file *efilp = evl_get_file(fd);
	long ret = -ENOTTY;
	struct file *filp;

	if (efilp == NULL)
		return -EBADF;

	filp = efilp->filp;

	if (unlikely(is_compat_oob_call())) {
		if (filp->f_op->compat_oob_ioctl)
			ret = filp->f_op->compat_oob_ioctl(filp, request, arg);
	} else  if (filp->f_op->oob_ioctl) {
		ret = filp->f_op->oob_ioctl(filp, request, arg);
	}

	if (ret == -ENOIOCTLCMD)
		ret = -ENOTTY;

	evl_put_file(efilp);

	return ret;
}

typedef long (*evl_syshand)(unsigned long arg1, unsigned long arg2,
			unsigned long arg3, unsigned long arg4,
			unsigned long arg5);

#define __EVL_CALL_ENTRY(__name)  \
	[sys_evl_ ## __name] = ((evl_syshand)(EVL_ ## __name))

static const evl_syshand evl_systbl[] = {
	   __EVL_CALL_ENTRY(read),
	   __EVL_CALL_ENTRY(write),
	   __EVL_CALL_ENTRY(ioctl),
};

#define __EVL_CALL_NAME(__name)  \
	[sys_evl_ ## __name] = #__name

static const char *evl_sysnames[] = {
	   __EVL_CALL_NAME(read),
	   __EVL_CALL_NAME(write),
	   __EVL_CALL_NAME(ioctl),
};

#define SYSCALL_PROPAGATE   0
#define SYSCALL_STOP        1

static inline void invoke_syscall(unsigned int nr, struct pt_regs *regs)
{
	evl_syshand handler;
	long ret;

	handler = evl_systbl[array_index_nospec(nr, ARRAY_SIZE(evl_systbl))];
	ret = handler(oob_arg1(regs),
		oob_arg2(regs),
		oob_arg3(regs),
		oob_arg4(regs),
		oob_arg5(regs));

	set_oob_retval(regs, ret);
}

static void prepare_for_signal(struct task_struct *p,
			struct evl_thread *curr,
			struct pt_regs *regs)
{
	unsigned long flags;

	/*
	 * FIXME: no restart mode flag for setting -EINTR instead of
	 * -ERESTARTSYS should be obtained from curr->local_info on a
	 * per-invocation basis, not on a per-call one (since we have
	 * 3 generic calls only).
	 */

	/*
	 * @curr == this_evl_rq()->curr over oob so no need to grab
	 * @curr->lock (i.e. @curr cannot go away under out feet).
	 */
	evl_spin_lock_irqsave(&curr->rq->lock, flags);

	/*
	 * We are called from out-of-band mode only to act upon a
	 * pending signal receipt. We may observe signal_pending(p)
	 * which implies that T_KICKED was set too
	 * (handle_sigwake_event()), or T_KICKED alone which means
	 * that we have been unblocked from a wait for some other
	 * reason.
	 */
	if (curr->info & T_KICKED) {
		if (signal_pending(p)) {
			set_oob_error(regs, -ERESTARTSYS);
			curr->info &= ~T_BREAK;
		}
		curr->info &= ~T_KICKED;
	}

	evl_spin_unlock_irqrestore(&curr->rq->lock, flags);

	evl_test_cancel();

	evl_switch_inband(SIGDEBUG_MIGRATE_SIGNAL);
}

/*
 * Intercepting __NR_clock_gettime (or __NR_clock_gettime64 on 32bit
 * archs) here means that we are handling a fallback syscall for
 * clock_gettime*() from the vDSO, which failed performing a direct
 * access to the clocksource.  Such fallback would involve a switch to
 * in-band mode unless we provide the service directly from here,
 * which is not optimal but still correct.
 */
static __always_inline
bool handle_vdso_fallback(int nr, struct pt_regs *regs)
{
	struct __kernel_old_timespec __user *u_old_ts;
	struct __kernel_timespec uts, __user *u_uts;
	struct __kernel_old_timespec old_ts;
	struct evl_clock *clock;
	struct timespec64 ts64;
	int clock_id, ret = 0;

#define is_clock_gettime(__nr) ((__nr) == __NR_clock_gettime)
#ifndef __NR_clock_gettime64
#define is_clock_gettime64(__nr)  0
#else
#define is_clock_gettime64(__nr) ((__nr) == __NR_clock_gettime64)
#endif

	if (!is_clock_gettime(nr) && !is_clock_gettime64(nr))
		return false;

	clock_id = (int)oob_arg1(regs);
	switch (clock_id) {
	case CLOCK_MONOTONIC:
		clock = &evl_mono_clock;
		break;
	case CLOCK_REALTIME:
		clock = &evl_realtime_clock;
		break;
	default:
		return false;
	}

	ts64 = ktime_to_timespec64(evl_read_clock(clock));

	if (is_clock_gettime(nr)) {
		old_ts.tv_sec = (__kernel_old_time_t)ts64.tv_sec;
		old_ts.tv_nsec = ts64.tv_nsec;
		u_old_ts = (struct __kernel_old_timespec __user *)oob_arg2(regs);
		if (raw_copy_to_user(u_old_ts, &old_ts, sizeof(old_ts)))
			ret = -EFAULT;
	} else if (is_clock_gettime64(nr)) {
		uts.tv_sec = ts64.tv_sec;
		uts.tv_nsec = ts64.tv_nsec;
		u_uts = (struct __kernel_timespec __user *)oob_arg2(regs);
		if (raw_copy_to_user(u_uts, &uts, sizeof(uts)))
			ret = -EFAULT;
	}

	set_oob_retval(regs, ret);

#undef is_clock_gettime
#undef is_clock_gettime64

	return true;
}

static int do_oob_syscall(struct irq_stage *stage, struct pt_regs *regs)
{
	struct evl_thread *curr;
	struct task_struct *p;
	unsigned int nr;

	if (!is_oob_syscall(regs))
		goto do_inband;

	nr = oob_syscall_nr(regs);
	if (nr >= ARRAY_SIZE(evl_systbl))
		goto bad_syscall;

	curr = evl_current();
	if (curr == NULL || !cap_raised(current_cap(), CAP_SYS_NICE)) {
		if (EVL_DEBUG(CORE))
			printk(EVL_WARNING
				"syscall <oob_%s> denied to %s[%d]\n",
				evl_sysnames[nr], current->comm, task_pid_nr(current));
		set_oob_error(regs, -EPERM);
		return SYSCALL_STOP;
	}

	/*
	 * If the syscall originates from in-band context, hand it
	 * over to handle_inband_syscall() where the caller would be
	 * switched to OOB context prior to handling the request.
	 */
	if (stage != &oob_stage)
		return SYSCALL_PROPAGATE;

	trace_evl_oob_sysentry(nr);

	invoke_syscall(nr, regs);

	/* Syscall might have switched in-band, recheck. */
	if (!evl_is_inband()) {
		p = current;
		if (signal_pending(p) || (curr->info & T_KICKED))
			prepare_for_signal(p, curr, regs);
		else if ((curr->state & T_WEAK) &&
			!atomic_read(&curr->inband_disable_count))
			evl_switch_inband(SIGDEBUG_NONE);
	}

	/* Update the stats and user visible info. */
	evl_inc_counter(&curr->stat.sc);
	evl_sync_uwindow(curr);

	trace_evl_oob_sysexit(oob_retval(regs));

	return SYSCALL_STOP;

do_inband:
	if (evl_is_inband())
		return SYSCALL_PROPAGATE;

	/*
	 * We don't want to trigger a stage switch whenever the
	 * current request issued from the out-of-band stage is not a
	 * valid in-band syscall, but rather deliver -ENOSYS directly
	 * instead.  Otherwise, switch to in-band mode before
	 * propagating the syscall down the pipeline. CAUTION:
	 * inband_syscall_nr(regs, &nr) is valid only if
	 * !is_oob_syscall(regs), which we checked earlier in
	 * do_oob_syscall().
	 */
	if (inband_syscall_nr(regs, &nr)) {
		if (handle_vdso_fallback(nr, regs))
			return SYSCALL_STOP;
		evl_switch_inband(SIGDEBUG_MIGRATE_SYSCALL);
		return SYSCALL_PROPAGATE;
	}

bad_syscall:
	printk(EVL_WARNING "invalid out-of-band syscall <%#x>\n", nr);

	set_oob_error(regs, -ENOSYS);

	return SYSCALL_STOP;
}

static int do_inband_syscall(struct irq_stage *stage, struct pt_regs *regs)
{
	struct evl_thread *curr = evl_current(); /* Always valid. */
	struct task_struct *p;
	unsigned int nr;
	int ret;

	/*
	 * Some architectures may use special out-of-bound syscall
	 * numbers which escape Dovetail's range check, e.g. when
	 * handling aarch32 syscalls over an aarch64 kernel. When so,
	 * assume this is an in-band syscall which we need to
	 * propagate downstream to the common handler.
	 */
	if (curr == NULL)
		return SYSCALL_PROPAGATE;

	/*
	 * Catch cancellation requests pending for threads undergoing
	 * the weak scheduling policy, which won't cross
	 * prepare_for_signal() frequently as they run mostly in-band.
	 */
	evl_test_cancel();

	/* Handle lazy schedparam updates before switching. */
	evl_propagate_schedparam_change(curr);

	/* Propagate in-band syscalls. */
	if (!is_oob_syscall(regs))
		return SYSCALL_PROPAGATE;

	/*
	 * Process an OOB syscall after switching current to OOB
	 * context.  do_oob_syscall() already checked the syscall
	 * number.
	 */
	nr = oob_syscall_nr(regs);

	trace_evl_inband_sysentry(nr);

	ret = evl_switch_oob();
	/*
	 * -ERESTARTSYS might be received if switching oob was blocked
	 * by a pending signal, otherwise -EINTR might be received
	 * upon signal detection after the transition to oob context,
	 * in which case the common logic applies (i.e. based on
	 * T_KICKED and/or signal_pending()).
	 */
	if (ret == -ERESTARTSYS) {
		set_oob_error(regs, ret);
		goto done;
	}

	invoke_syscall(nr, regs);

	if (!evl_is_inband()) {
		p = current;
		if (signal_pending(p) || (curr->info & T_KICKED))
			prepare_for_signal(p, curr, regs);
		else if ((curr->state & T_WEAK) &&
			!atomic_read(&curr->inband_disable_count))
			evl_switch_inband(SIGDEBUG_NONE);
	}
done:
	if (curr->local_info & T_IGNOVR)
		curr->local_info &= ~T_IGNOVR;

	evl_inc_counter(&curr->stat.sc);
	evl_sync_uwindow(curr);

	trace_evl_inband_sysexit(oob_retval(regs));

	return SYSCALL_STOP;
}

int handle_pipelined_syscall(struct irq_stage *stage, struct pt_regs *regs)
{
	if (unlikely(running_inband()))
		return do_inband_syscall(stage, regs);

	return do_oob_syscall(stage, regs);
}

void handle_oob_syscall(struct pt_regs *regs)
{
	int ret;

	ret = do_oob_syscall(&oob_stage, regs);
	EVL_WARN_ON(CORE, ret == SYSCALL_PROPAGATE);
}
