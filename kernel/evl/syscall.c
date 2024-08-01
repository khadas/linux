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
#include <linux/prctl.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/signal.h>
#include <evl/thread.h>
#include <evl/sched.h>
#include <asm/syscall.h>
#include <uapi/evl/syscall-abi.h>
#include <asm/evl/syscall.h>
#include <trace/events/evl.h>

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

#define __EVL_CALL_NAME(__name)  \
	[sys_evl_ ## __name] = #__name

static const char *evl_sysnames[] = {
	   __EVL_CALL_NAME(read),
	   __EVL_CALL_NAME(write),
	   __EVL_CALL_NAME(ioctl),
};

#define SYSCALL_PROPAGATE   0
#define SYSCALL_STOP        1

static __always_inline
void invoke_syscall(unsigned int nr, struct pt_regs *regs,
		unsigned long *args)
{
	int error;
	long ret;

	/*
	 * We have only very few syscalls, prefer a plain switch to a
	 * pointer indirection which ends up being fairly costly due
	 * to exploit mitigations.
	 */
	switch (nr) {
	case sys_evl_read:
		ret = EVL_read((int)args[0],
			(char __user *)args[1],
			(size_t)args[2]);
		break;
	case sys_evl_write:
		ret = EVL_write((int)args[0],
				(const char __user *)args[1],
				(size_t)args[2]);
		break;
	case sys_evl_ioctl:
		ret = EVL_ioctl((int)args[0],
				(unsigned int)args[1],
				args[2]);
		break;
	}

	error = IS_ERR_VALUE(ret) ? ret : 0;
	syscall_set_return_value(current, regs, error, ret);
}

static void prepare_for_signal(struct task_struct *p,
			struct evl_thread *curr,
			struct pt_regs *regs)
{
	unsigned long flags;

	/*
	 * @curr == this_evl_rq()->curr over oob so no need to grab
	 * @curr->lock (i.e. @curr cannot go away under out feet).
	 */
	raw_spin_lock_irqsave(&curr->rq->lock, flags);

	/*
	 * We are called from out-of-band mode only to act upon a
	 * pending signal receipt. We may observe signal_pending(p)
	 * which implies that EVL_T_KICKED was set too
	 * (handle_sigwake_event()), or EVL_T_KICKED alone which means
	 * that we have been unblocked from a wait for some other
	 * reason.
	 */
	if (curr->info & EVL_T_KICKED) {
		if (signal_pending(p)) {
			int retval = -ERESTARTSYS;
			if (curr->local_info & EVL_T_NORST) {
				retval = -EINTR;
				curr->local_info &= ~EVL_T_NORST;
			}
			syscall_set_return_value(current, regs, retval, 0);
			curr->info &= ~EVL_T_BREAK;
		}
		curr->info &= ~EVL_T_KICKED;
	}

	raw_spin_unlock_irqrestore(&curr->rq->lock, flags);

	evl_test_cancel();

	evl_switch_inband(EVL_HMDIAG_SIGDEMOTE);
}

/*
 * Intercepting __NR_clock_gettime (or __NR_clock_gettime64 on 32bit
 * archs) here means that we are handling a fallback syscall for
 * clock_gettime*() from the vDSO, which failed performing a direct
 * access to the clocksource.  Such fallback would involve a switch to
 * in-band mode unless we provide the service directly from here,
 * which is not optimal but still correct.
 */
static bool handle_vdso_fallback(struct pt_regs *regs, unsigned int nr,
				unsigned long *args)
{
	struct __kernel_old_timespec __user *u_old_ts;
	struct __kernel_timespec uts, __user *u_uts;
	struct __kernel_old_timespec old_ts;
	int clock_id, ret = 0, error;
	struct evl_clock *clock;
	struct timespec64 ts64;

#define is_clock_gettime(__nr) ((__nr) == __NR_clock_gettime)
#ifndef __NR_clock_gettime64
#define is_clock_gettime64(__nr)  0
#else
#define is_clock_gettime64(__nr) ((__nr) == __NR_clock_gettime64)
#endif

	if (!is_clock_gettime(nr) && !is_clock_gettime64(nr))
		return false;

	clock_id = (int)args[0];
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
		u_old_ts = (struct __kernel_old_timespec __user *)args[1];
		if (raw_copy_to_user(u_old_ts, &old_ts, sizeof(old_ts)))
			ret = -EFAULT;
	} else if (is_clock_gettime64(nr)) {
		uts.tv_sec = ts64.tv_sec;
		uts.tv_nsec = ts64.tv_nsec;
		u_uts = (struct __kernel_timespec __user *)args[1];
		if (raw_copy_to_user(u_uts, &uts, sizeof(uts)))
			ret = -EFAULT;
	}

	error = IS_ERR_VALUE((long)ret) ? ret : 0;
	syscall_set_return_value(current, regs, error, ret);

#undef is_clock_gettime
#undef is_clock_gettime64

	return true;
}

static int do_oob_syscall(struct irq_stage *stage, struct pt_regs *regs,
			unsigned int scno, unsigned long *args, bool is_evlsc)
{
	struct task_struct *tsk = current;
	struct evl_thread *curr;

	if (!is_evlsc)
		goto do_inband;

	if (scno >= NR_EVL_SYSCALLS) {
		printk(EVL_WARNING "invalid out-of-band syscall <%#x>\n", scno);
		goto bad_syscall;
	}

	curr = evl_current();
	if (curr == NULL || !cap_raised(current_cap(), CAP_SYS_NICE)) {
		if (EVL_DEBUG(CORE))
			printk(EVL_WARNING
				"syscall <oob_%s> denied to %s[%d]\n",
				evl_sysnames[scno], tsk->comm, task_pid_nr(tsk));
		syscall_set_return_value(tsk, regs, -EPERM, 0);
		return SYSCALL_STOP;
	}

	/*
	 * If the syscall originates from in-band context, hand it
	 * over to handle_inband_syscall() where the caller would be
	 * switched to OOB context prior to handling the request.
	 */
	if (stage != &oob_stage)
		return SYSCALL_PROPAGATE;

	trace_evl_oob_sysentry(scno);

	invoke_syscall(scno, regs, args);

	/* Syscall might have switched in-band, recheck. */
	if (!evl_is_inband()) {
		if (signal_pending(tsk) || (curr->info & EVL_T_KICKED))
			prepare_for_signal(tsk, curr, regs);
		else if ((curr->state & EVL_T_WEAK) &&
			!atomic_read(&curr->held_mutex_count))
			evl_switch_inband(EVL_HMDIAG_NONE);
	}

	/* Update the stats and user visible info. */
	evl_inc_counter(&curr->stat.sc);
	evl_sync_uwindow(curr);

	trace_evl_oob_sysexit(syscall_get_return_value(tsk, regs));

	return SYSCALL_STOP;

do_inband:
	if (evl_is_inband())
		return SYSCALL_PROPAGATE;

	/*
	 * We don't want to trigger a stage switch whenever the
	 * current request issued from the out-of-band stage is not a
	 * valid in-band syscall, but rather deliver -ENOSYS directly
	 * instead.  Otherwise, switch to in-band mode before
	 * propagating the syscall down the pipeline.
	 */
	if (is_valid_inband_syscall(scno)) {
		if (handle_vdso_fallback(regs, scno, args))
			return SYSCALL_STOP;
		evl_switch_inband(EVL_HMDIAG_SYSDEMOTE);
		return SYSCALL_PROPAGATE;
	}

	printk(EVL_WARNING "invalid in-band syscall <%u>\n", scno);

bad_syscall:
	syscall_set_return_value(tsk, regs, -ENOSYS, 0);

	return SYSCALL_STOP;
}

static int do_inband_syscall(struct pt_regs *regs, unsigned int scno,
			unsigned long *args,
			bool is_evlsc)
{
	struct evl_thread *curr = evl_current(); /* Always valid. */
	struct task_struct *tsk = current;
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
	if (!is_evlsc)
		return SYSCALL_PROPAGATE;

	/*
	 * Process an OOB syscall after switching current to the
	 * out-of-band stage.  do_oob_syscall() already checked the
	 * syscall number.
	 */
	trace_evl_inband_sysentry(scno);

	ret = evl_switch_oob();
	/*
	 * -ERESTARTSYS might be received if switching oob was blocked
	 * by a pending signal, otherwise -EINTR might be received
	 * upon signal detection after the transition to oob context,
	 * in which case the common logic applies (i.e. based on
	 * EVL_T_KICKED and/or signal_pending()).
	 */
	if (ret == -ERESTARTSYS) {
		syscall_set_return_value(tsk, regs, ret, 0);
		goto done;
	}

	invoke_syscall(scno, regs, args);

	if (!evl_is_inband()) {
		if (signal_pending(tsk) || (curr->info & EVL_T_KICKED))
			prepare_for_signal(tsk, curr, regs);
		else if ((curr->state & EVL_T_WEAK) &&
			!atomic_read(&curr->held_mutex_count))
			evl_switch_inband(EVL_HMDIAG_NONE);
	}
done:
	if (curr->local_info & EVL_T_IGNOVR)
		curr->local_info &= ~EVL_T_IGNOVR;

	evl_inc_counter(&curr->stat.sc);
	evl_sync_uwindow(curr);

	trace_evl_inband_sysexit(syscall_get_return_value(tsk, regs));

	return SYSCALL_STOP;
}

static bool collect_syscall_args(struct pt_regs *regs,
				unsigned long *args,
				unsigned int *scno)
{
	struct task_struct *tsk = current;

	/*
	 * We'll need the arguments later on for handling either of
	 * inband or evl syscalls.
	 */
	syscall_get_arguments(tsk, regs, args);

	if (!in_oob_syscall(regs)) {
		*scno = syscall_get_nr(tsk, regs);
		return false;
	}

	/*
	 * Since ABI 36, we recognize EVL requests only when folded
	 * into a prctl() call, such as prctl(PR_OOB_SYSCALL, @nr,
	 * args...). If so, fetch the EVL syscall number then shift
	 * the arguments left to skip it (3 arguments max).
	 * Otherwise, assume this is an in-band syscall, so leave the
	 * argument vector unchanged.
	 */
	*scno = args[1];
	args[0] = args[2];
	args[1] = args[3];
	args[2] = args[4];

	return true;
}

int handle_pipelined_syscall(struct irq_stage *stage, struct pt_regs *regs)
{
	unsigned long args[6] = { 0 };
	unsigned int scno;
	bool is_evlsc;

	is_evlsc = collect_syscall_args(regs, args, &scno);

	if (unlikely(running_inband()))
		return do_inband_syscall(regs, scno, args, is_evlsc);

	return do_oob_syscall(stage, regs, scno, args, is_evlsc);
}

int handle_oob_syscall(struct pt_regs *regs)
{
	unsigned long args[6] = { 0 };
	unsigned int scno;
	bool is_evlsc;
	int ret;

	is_evlsc = collect_syscall_args(regs, args, &scno);
	ret = do_oob_syscall(&oob_stage, regs, scno, args, is_evlsc);
	EVL_WARN_ON(CORE, ret == SYSCALL_PROPAGATE); /* Keep me there! */

	return ret;
}
