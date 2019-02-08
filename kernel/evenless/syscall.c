/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Context handling logic derived from Xenomai Cobalt
 * (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2005, 2018 Philippe Gerum  <rpm@xenomai.org>
 * Copyright (C) 2005 Gilles Chanteperdrix  <gilles.chanteperdrix@xenomai.org>
 */

#include <linux/types.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/dovetail.h>
#include <linux/kconfig.h>
#include <linux/atomic.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/signal.h>
#include <evenless/control.h>
#include <evenless/thread.h>
#include <evenless/timer.h>
#include <evenless/monitor.h>
#include <evenless/clock.h>
#include <evenless/sched.h>
#include <evenless/file.h>
#include <trace/events/evenless.h>
#include <uapi/evenless/syscall.h>
#include <asm/evenless/syscall.h>
#ifdef CONFIG_FTRACE
#include <trace/trace.h>
#endif

#define EVENLESS_SYSCALL(__name, __args)	\
	long EvEnLeSs_ ## __name __args

#define SYSCALL_PROPAGATE   0
#define SYSCALL_STOP        1

typedef long (*evl_syshand)(unsigned long arg1, unsigned long arg2,
			unsigned long arg3, unsigned long arg4,
			unsigned long arg5);

static const evl_syshand evl_syscalls[__NR_EVENLESS_SYSCALLS];

static inline void do_oob_request(int nr, struct pt_regs *regs)
{
	evl_syshand handler;
	long ret;

	handler = evl_syscalls[nr];
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
	int cause = SIGDEBUG_UNDEFINED;
	unsigned long flags;

	/*
	 * FIXME: no restart mode flag for setting -EINTR instead of
	 * -ERESTARTSYS should be obtained from curr->local_info on a
	 * per-invocation basis, not on a per-call one (since we have
	 * 3 generic calls only).
	 */

	xnlock_get_irqsave(&nklock, flags);

	if (curr->info & T_KICKED) {
		if (signal_pending(p)) {
			set_oob_error(regs, -ERESTARTSYS);
			if (!(curr->state & T_SSTEP))
				cause = SIGDEBUG_MIGRATE_SIGNAL;
			curr->info &= ~T_BREAK;
		}
		curr->info &= ~T_KICKED;
	}

	xnlock_put_irqrestore(&nklock, flags);

	evl_test_cancel();

	evl_switch_inband(cause);
}

static int do_oob_syscall(struct irq_stage *stage, struct pt_regs *regs)
{
	struct evl_thread *curr;
	struct task_struct *p;
	unsigned int nr;

	if (!is_oob_syscall(regs))
		goto do_inband;

	nr = oob_syscall_nr(regs);
	curr = evl_current();
	if (curr == NULL || !cap_raised(current_cap(), CAP_SYS_NICE)) {
		if (EVL_DEBUG(CORE))
			printk(EVL_WARNING
				"OOB syscall <%d> denied to %s[%d]\n",
				nr, current->comm, task_pid_nr(current));
		set_oob_error(regs, -EPERM);
		return SYSCALL_STOP;
	}

	if (nr >= ARRAY_SIZE(evl_syscalls))
		goto bad_syscall;

	/*
	 * If the syscall originates from in-band context, hand it
	 * over to handle_inband_syscall() where the caller would be
	 * switched to OOB context prior to handling the request.
	 */
	if (stage != &oob_stage)
		return SYSCALL_PROPAGATE;

	trace_evl_oob_sysentry(nr);

	do_oob_request(nr, regs);

	/* Syscall might have switched in-band, recheck. */
	if (!evl_is_inband()) {
		p = current;
		if (signal_pending(p) || (curr->info & T_KICKED))
			prepare_for_signal(p, curr, regs);
		else if ((curr->state & T_WEAK) &&
			!atomic_read(&curr->inband_disable_count))
			evl_switch_inband(SIGDEBUG_UNDEFINED);
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
	 * If this is a legit in-band syscall issued from OOB context,
	 * switch to in-band mode before propagating the syscall down
	 * the pipeline.
	 */
	if (inband_syscall_nr(regs, &nr)) {
		evl_switch_inband(SIGDEBUG_MIGRATE_SYSCALL);
		return SYSCALL_PROPAGATE;
	}

bad_syscall:
	printk(EVL_WARNING "bad OOB syscall <%#x>\n", nr);

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
	if (ret) {
		set_oob_error(regs, ret);
		goto done;
	}

	do_oob_request(nr, regs);

	if (!evl_is_inband()) {
		p = current;
		if (signal_pending(p))
			prepare_for_signal(p, curr, regs);
		else if ((curr->state & T_WEAK) &&
			!atomic_read(&curr->inband_disable_count))
			evl_switch_inband(SIGDEBUG_UNDEFINED);
	}
done:
	curr->local_info &= ~T_HICCUP;
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

int handle_oob_syscall(struct pt_regs *regs)
{
	int ret;

	ret = do_oob_syscall(&oob_stage, regs);
	if (EVL_WARN_ON(CORE, ret == SYSCALL_PROPAGATE))
		ret = SYSCALL_STOP;

	return ret;
}

static EVENLESS_SYSCALL(read, (int fd, char __user *u_buf, size_t size))
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

static EVENLESS_SYSCALL(write, (int fd, const char __user *u_buf, size_t size))
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

static EVENLESS_SYSCALL(ioctl, (int fd, unsigned int request,
				unsigned long arg))
{
	struct evl_file *efilp = evl_get_file(fd);
	struct file *filp;
	long ret;

	if (efilp == NULL)
		return -EBADF;

	filp = efilp->filp;
	if (filp->f_op->oob_ioctl) {
		ret = filp->f_op->oob_ioctl(filp, request, arg);
		if (ret == -ENOIOCTLCMD)
			ret = -ENOTTY;
	} else
		ret = -ENOTTY;

	evl_put_file(efilp);

	return ret;
}

static int EvEnLeSs_ni(void)
{
	return -ENOSYS;
}

#define __syshand__(__name)	((evl_syshand)(EvEnLeSs_ ## __name))

#define __EVENLESS_CALL_ENTRIES			\
	__EVENLESS_CALL_ENTRY(read)		\
		__EVENLESS_CALL_ENTRY(write)	\
		__EVENLESS_CALL_ENTRY(ioctl)

#define __EVENLESS_NI	__syshand__(ni)

#define __EVENLESS_CALL_NI					\
	[0 ... __NR_EVENLESS_SYSCALLS-1] = __EVENLESS_NI,

#define __EVENLESS_CALL_ENTRY(__name)				\
	[sys_evenless_ ## __name] = __syshand__(__name),

static const evl_syshand evl_syscalls[] = {
	__EVENLESS_CALL_NI
	__EVENLESS_CALL_ENTRIES
};
