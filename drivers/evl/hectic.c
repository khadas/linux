/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt's switchtest driver, https://xenomai.org/
 * Copyright (C) 2010 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>.
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/semaphore.h>
#include <linux/irq_work.h>
#include <evl/thread.h>
#include <evl/flag.h>
#include <evl/file.h>
#include <asm/evl/fptest.h>
#include <uapi/evl/devices/hectic.h>

#define HECTIC_KTHREAD      0x20000

struct rtswitch_context;

struct rtswitch_task {
	struct hectic_task_index base;
	struct evl_flag rt_synch;
	struct semaphore nrt_synch;
	struct evl_kthread kthread; /* For kernel-space real-time tasks. */
	unsigned int last_switch;
	struct rtswitch_context *ctx;
};

struct rtswitch_context {
	struct rtswitch_task *tasks;
	unsigned int tasks_count;
	unsigned int next_index;
	struct semaphore lock;
	unsigned int cpu;
	unsigned int switches_count;

	unsigned long pause_us;
	unsigned int next_task;
	struct evl_timer wake_up_delay;

	bool failed;
	struct hectic_error error;

	struct rtswitch_task *utask;
	struct irq_work wake_utask;
	struct evl_file efile;
};

static u32 fp_features;

#define trace_fpu_breakage(__ctx)				\
	do {							\
		trace_evl_fpu_corrupt((__ctx)->error.fp_val);	\
		trace_evl_trigger("hectic");			\
	} while (0)

static void handle_fpu_error(struct rtswitch_context *ctx,
			     unsigned int fp_in, unsigned int fp_out,
			     int bad_reg)
{
	struct rtswitch_task *cur = &ctx->tasks[ctx->error.last_switch.to];
	unsigned int i;

	printk(EVL_ERR "fpreg%d trashed: in=%u, out=%u\n",
	       bad_reg, fp_in, fp_out);

	ctx->failed = true;
	ctx->error.fp_val = fp_out;
	trace_fpu_breakage(ctx);

	if ((cur->base.flags & HECTIC_OOB_WAIT) == HECTIC_OOB_WAIT)
		for (i = 0; i < ctx->tasks_count; i++) {
			struct rtswitch_task *task = &ctx->tasks[i];

			/* Find the first non kernel-space task. */
			if ((task->base.flags & HECTIC_KTHREAD))
				continue;

			/* Unblock it. */
			switch(task->base.flags & HECTIC_OOB_WAIT) {
			case HECTIC_INBAND_WAIT:
				ctx->utask = task;
				irq_work_queue(&ctx->wake_utask);
				break;

			case HECTIC_OOB_WAIT:
				evl_raise_flag(&task->rt_synch);
				break;
			}

			evl_hold_thread(&cur->kthread.thread, T_SUSP);
		}
}

static int rtswitch_pend_rt(struct rtswitch_context *ctx,
			    unsigned int idx)
{
	struct rtswitch_task *task;
	int rc;

	if (idx > ctx->tasks_count)
		return -EINVAL;

	task = &ctx->tasks[idx];
	task->base.flags |= HECTIC_OOB_WAIT;

	rc = evl_wait_flag(&task->rt_synch);
	if (rc < 0)
		return rc;

	if (ctx->failed)
		return 1;

	return 0;
}

static void timed_wake_up(struct evl_timer *timer) /* hard irqs off */
{
	struct rtswitch_context *ctx;
	struct rtswitch_task *task;

	ctx = container_of(timer, struct rtswitch_context, wake_up_delay);
	task = &ctx->tasks[ctx->next_task];

	switch (task->base.flags & HECTIC_OOB_WAIT) {
	case HECTIC_INBAND_WAIT:
		ctx->utask = task;
		irq_work_queue(&ctx->wake_utask);
		break;

	case HECTIC_OOB_WAIT:
		evl_raise_flag(&task->rt_synch);
	}
}

static int rtswitch_to_rt(struct rtswitch_context *ctx,
			  unsigned int from_idx,
			  unsigned int to_idx)
{
	struct rtswitch_task *from, *to;
	int rc;

	if (from_idx > ctx->tasks_count || to_idx > ctx->tasks_count)
		return -EINVAL;

	/* to == from is a special case which means
	   "return to the previous task". */
	if (to_idx == from_idx)
		to_idx = ctx->error.last_switch.from;

	from = &ctx->tasks[from_idx];
	to = &ctx->tasks[to_idx];

	from->base.flags |= HECTIC_OOB_WAIT;
	from->last_switch = ++ctx->switches_count;
	ctx->error.last_switch.from = from_idx;
	ctx->error.last_switch.to = to_idx;
	barrier();

	if (ctx->pause_us) {
		ctx->next_task = to_idx;
		barrier();
		evl_start_timer(&ctx->wake_up_delay,
				evl_abs_timeout(&ctx->wake_up_delay,
						ctx->pause_us * 1000),
				EVL_INFINITE);
		evl_disable_preempt();
	} else
		switch (to->base.flags & HECTIC_OOB_WAIT) {
		case HECTIC_INBAND_WAIT:
			ctx->utask = to;
			barrier();
			irq_work_queue(&ctx->wake_utask);
			evl_disable_preempt();
			break;

		case HECTIC_OOB_WAIT:
			evl_disable_preempt();
			evl_raise_flag(&to->rt_synch);
			break;

		default:
			return -EINVAL;
		}

	rc = evl_wait_flag(&from->rt_synch);
	evl_enable_preempt();

	if (rc < 0)
		return rc;

	if (ctx->failed)
		return 1;

	return 0;
}

static int rtswitch_pend_nrt(struct rtswitch_context *ctx,
			     unsigned int idx)
{
	struct rtswitch_task *task;

	if (idx > ctx->tasks_count)
		return -EINVAL;

	task = &ctx->tasks[idx];

	task->base.flags &= ~HECTIC_OOB_WAIT;

	if (down_interruptible(&task->nrt_synch))
		return -EINTR;

	if (ctx->failed)
		return 1;

	return 0;
}

static int rtswitch_to_nrt(struct rtswitch_context *ctx,
			   unsigned int from_idx,
			   unsigned int to_idx)
{
	struct rtswitch_task *from, *to;
	unsigned int expected, fp_val;
	bool fp_check;
	int bad_reg;

	if (from_idx > ctx->tasks_count || to_idx > ctx->tasks_count)
		return -EINVAL;

	/* to == from is a special case which means
	   "return to the previous task". */
	if (to_idx == from_idx)
		to_idx = ctx->error.last_switch.from;

	from = &ctx->tasks[from_idx];
	to = &ctx->tasks[to_idx];

	fp_check = ctx->switches_count == from->last_switch + 1
		&& ctx->error.last_switch.from == to_idx
		&& ctx->error.last_switch.to == from_idx;

	from->base.flags &= ~HECTIC_OOB_WAIT;
	from->last_switch = ++ctx->switches_count;
	ctx->error.last_switch.from = from_idx;
	ctx->error.last_switch.to = to_idx;
	barrier();

	if (ctx->pause_us) {
		ctx->next_task = to_idx;
		barrier();
		evl_start_timer(&ctx->wake_up_delay,
				evl_abs_timeout(&ctx->wake_up_delay,
						ctx->pause_us * 1000),
				EVL_INFINITE);
	} else
		switch (to->base.flags & HECTIC_OOB_WAIT) {
		case HECTIC_INBAND_WAIT:
		switch_to_nrt:
			up(&to->nrt_synch);
			break;

		case HECTIC_OOB_WAIT:

			if (!fp_check || !evl_begin_fpu())
				goto signal_nofp;

			expected = from_idx + 500 +
				(ctx->switches_count % 4000000) * 1000;

			evl_set_fpregs(fp_features, expected);
			evl_raise_flag(&to->rt_synch);
			fp_val = evl_check_fpregs(fp_features, expected, bad_reg);
			evl_end_fpu();

			if (down_interruptible(&from->nrt_synch))
				return -EINTR;
			if (ctx->failed)
				return 1;
			if (fp_val != expected) {
				handle_fpu_error(ctx, expected, fp_val, bad_reg);
				return 1;
			}

			from->base.flags &= ~HECTIC_OOB_WAIT;
			from->last_switch = ++ctx->switches_count;
			ctx->error.last_switch.from = from_idx;
			ctx->error.last_switch.to = to_idx;
			if ((to->base.flags & HECTIC_OOB_WAIT) == HECTIC_INBAND_WAIT)
				goto switch_to_nrt;
			expected = from_idx + 500 +
				(ctx->switches_count % 4000000) * 1000;
			barrier();

			evl_begin_fpu();
			evl_set_fpregs(fp_features, expected);
			evl_raise_flag(&to->rt_synch);
			fp_val = evl_check_fpregs(fp_features, expected, bad_reg);
			evl_end_fpu();

			if (down_interruptible(&from->nrt_synch))
				return -EINTR;
			if (ctx->failed)
				return 1;
			if (fp_val != expected) {
				handle_fpu_error(ctx, expected, fp_val, bad_reg);
				return 1;
			}

			from->base.flags &= ~HECTIC_OOB_WAIT;
			from->last_switch = ++ctx->switches_count;
			ctx->error.last_switch.from = from_idx;
			ctx->error.last_switch.to = to_idx;
			barrier();
			if ((to->base.flags & HECTIC_OOB_WAIT) == HECTIC_INBAND_WAIT)
				goto switch_to_nrt;

		signal_nofp:
			evl_raise_flag(&to->rt_synch);
			break;

		default:
			return -EINVAL;
		}

	if (down_interruptible(&from->nrt_synch))
		return -EINTR;

	if (ctx->failed)
		return 1;

	return 0;
}

static int rtswitch_set_tasks_count(struct rtswitch_context *ctx, unsigned int count)
{
	struct rtswitch_task *tasks;

	if (ctx->tasks_count == count)
		return 0;

	tasks = vmalloc(count * sizeof(*tasks));

	if (!tasks)
		return -ENOMEM;

	down(&ctx->lock);

	if (ctx->tasks)
		vfree(ctx->tasks);

	ctx->tasks = tasks;
	ctx->tasks_count = count;
	ctx->next_index = 0;

	up(&ctx->lock);

	return 0;
}

static int rtswitch_register_task(struct rtswitch_context *ctx,
				struct hectic_task_index *arg,
				int flags)
{
	struct rtswitch_task *t;

	down(&ctx->lock);

	if (ctx->next_index == ctx->tasks_count) {
		up(&ctx->lock);
		return -EBUSY;
	}

	arg->index = ctx->next_index;
	t = &ctx->tasks[arg->index];
	ctx->next_index++;
	t->base.index = arg->index;
	t->base.flags = (arg->flags & HECTIC_OOB_WAIT)|flags;
	t->last_switch = 0;
	sema_init(&t->nrt_synch, 0);
	evl_init_flag(&t->rt_synch);

	up(&ctx->lock);

	return 0;
}

static void rtswitch_kthread(struct evl_kthread *kthread)
{
	struct rtswitch_context *ctx;
	struct rtswitch_task *task;
	unsigned int to, i = 0;

	task = container_of(kthread, struct rtswitch_task, kthread);
	ctx = task->ctx;

	to = task->base.index;

	rtswitch_pend_rt(ctx, task->base.index);

	while (!evl_kthread_should_stop()) {
		switch(i % 3) {
		case 0:
			/* to == from means "return to last task" */
			rtswitch_to_rt(ctx, task->base.index, task->base.index);
			break;
		case 1:
			if (++to == task->base.index)
				++to;
			if (to > ctx->tasks_count - 1)
				to = 0;
			if (to == task->base.index)
				++to;

			/* Fall through. */
		case 2:
			rtswitch_to_rt(ctx, task->base.index, to);
		}
		if (++i == 4000000)
			i = 0;
	}
}

static int rtswitch_create_kthread(struct rtswitch_context *ctx,
				   struct hectic_task_index *ptask)
{
	struct rtswitch_task *task;
	int err;

	err = rtswitch_register_task(ctx, ptask, HECTIC_KTHREAD);
	if (err)
		return err;

	task = &ctx->tasks[ptask->index];
	task->ctx = ctx;
	err = evl_run_kthread_on_cpu(&task->kthread, ctx->cpu,
				     rtswitch_kthread, 1,
				     "rtk%d@%u:%d",
				     ptask->index, ctx->cpu,
				     task_pid_nr(current));
	/*
	 * On error, clear the flag bits in order to avoid calling
	 * evl_cancel_kthread() for an invalid thread in
	 * hectic_release().
	 */
	if (err)
		task->base.flags = 0;

	return err;
}

static void rtswitch_utask_waker(struct irq_work *work)
{
	struct rtswitch_context *ctx;
	ctx = container_of(work, struct rtswitch_context, wake_utask);
	up(&ctx->utask->nrt_synch);
}

static long hectic_ioctl(struct file *filp, unsigned int cmd,
			 unsigned long arg)
{
	struct rtswitch_context *ctx = filp->private_data;
	struct hectic_switch_req fromto, __user *u_fromto;
	struct hectic_task_index task, __user *u_task;
	struct hectic_error __user *u_lerr;
	__u32 count;
	int err;

	switch (cmd) {
	case EVL_HECIOC_SET_TASKS_COUNT:
		return rtswitch_set_tasks_count(ctx, arg);

	case EVL_HECIOC_SET_CPU:
		if (arg > num_online_cpus() - 1)
			return -EINVAL;

		ctx->cpu = arg;
		return 0;

	case EVL_HECIOC_SET_PAUSE:
		ctx->pause_us = arg;
		return 0;

	case EVL_HECIOC_REGISTER_UTASK:
		u_task = (typeof(u_task))arg;
		err = copy_from_user(&task, u_task, sizeof(task));
		if (err)
			return -EFAULT;

		err = rtswitch_register_task(ctx, &task, 0);
		if (!err && copy_to_user(u_task, &task, sizeof(task)))
			err = -EFAULT;

		return err;

	case EVL_HECIOC_CREATE_KTASK:
		u_task = (typeof(u_task))arg;
		err = copy_from_user(&task, u_task, sizeof(task));
		if (err)
			return -EFAULT;

		err = rtswitch_create_kthread(ctx, &task);
		if (!err && copy_to_user(u_task, &task, sizeof(task)))
			err = -EFAULT;

		return err;

	case EVL_HECIOC_PEND:
		u_task = (typeof(u_task))arg;
		err = copy_from_user(&task, u_task, sizeof(task));
		if (err)
			return -EFAULT;

		return rtswitch_pend_nrt(ctx, task.index);

	case EVL_HECIOC_SWITCH_TO:
		u_fromto = (typeof(u_fromto))arg;
		err = copy_from_user(&fromto, u_fromto, sizeof(fromto));
		if (err)
			return -EFAULT;

		return rtswitch_to_nrt(ctx, fromto.from, fromto.to);

	case EVL_HECIOC_GET_SWITCHES_COUNT:
		count = ctx->switches_count;
		return copy_to_user((__u32 *)arg, &count, sizeof(count)) ?
			-EFAULT : 0;

	case EVL_HECIOC_GET_LAST_ERROR:
		trace_fpu_breakage(ctx);
		u_lerr = (typeof(u_lerr))arg;
		return copy_to_user(u_lerr, &ctx->error, sizeof(ctx->error)) ?
			-EFAULT : 0;
	default:
		return -ENOTTY;
	}
}

static long hectic_oob_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	struct rtswitch_context *ctx = filp->private_data;
	struct hectic_switch_req fromto, __user *u_fromto;
	struct hectic_task_index task, __user *u_task;
	struct hectic_error __user *u_lerr;
	int err;

	switch (cmd) {
	case EVL_HECIOC_PEND:
		u_task = (typeof(u_task))arg;
		err = raw_copy_from_user(&task, u_task, sizeof(task));
		return err ? -EFAULT :
			rtswitch_pend_rt(ctx, task.index);

	case EVL_HECIOC_SWITCH_TO:
		u_fromto = (typeof(u_fromto))arg;
		err = raw_copy_from_user(&fromto, u_fromto, sizeof(fromto));
		return err ? -EFAULT :
			rtswitch_to_rt(ctx, fromto.from, fromto.to);

	case EVL_HECIOC_GET_LAST_ERROR:
		trace_fpu_breakage(ctx);
		u_lerr = (typeof(u_lerr))arg;
		return raw_copy_to_user(u_lerr, &ctx->error, sizeof(ctx->error)) ?
			-EFAULT : 0;

	default:
		return -ENOTTY;
	}
}

static int hectic_open(struct inode *inode, struct file *filp)
{
	struct rtswitch_context *ctx;
	int ret;

	ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
	if (ctx == NULL)
		return -ENOMEM;

	ret = evl_open_file(&ctx->efile, filp);
	if (ret) {
		kfree(ctx);
		return ret;
	}

	ctx->tasks = NULL;
	ctx->tasks_count = ctx->next_index = ctx->cpu = ctx->switches_count = 0;
	sema_init(&ctx->lock, 1);
	ctx->failed = false;
	ctx->error.last_switch.from = ctx->error.last_switch.to = -1;
	ctx->pause_us = 0;

	init_irq_work(&ctx->wake_utask, rtswitch_utask_waker);
	evl_init_core_timer(&ctx->wake_up_delay, timed_wake_up);

	filp->private_data = ctx;

	return 0;
}

static int hectic_release(struct inode *inode, struct file *filp)
{
	struct rtswitch_context *ctx = filp->private_data;
	unsigned int i;

	evl_destroy_timer(&ctx->wake_up_delay);

	if (ctx->tasks) {
		set_cpus_allowed_ptr(current, cpumask_of(ctx->cpu));

		for (i = 0; i < ctx->next_index; i++) {
			struct rtswitch_task *task = &ctx->tasks[i];

			if (task->base.flags & HECTIC_KTHREAD)
				evl_cancel_kthread(&task->kthread);

			evl_destroy_flag(&task->rt_synch);
		}
		vfree(ctx->tasks);
	}

	evl_release_file(&ctx->efile);
	kfree(ctx);

	return 0;
}

static struct class hectic_class = {
	.name = "hectic",
	.owner = THIS_MODULE,
};

static const struct file_operations hectic_fops = {
	.open		= hectic_open,
	.release	= hectic_release,
	.unlocked_ioctl	= hectic_ioctl,
	.oob_ioctl	= hectic_oob_ioctl,
};

static dev_t hectic_devt;

static struct cdev hectic_cdev;

static int __init hectic_init(void)
{
	struct device *dev;
	int ret;

	fp_features = evl_detect_fpu();

	ret = class_register(&hectic_class);
	if (ret)
		return ret;

	ret = alloc_chrdev_region(&hectic_devt, 0, 1, "hectic");
	if (ret)
		goto fail_region;

	cdev_init(&hectic_cdev, &hectic_fops);
	ret = cdev_add(&hectic_cdev, hectic_devt, 1);
	if (ret)
		goto fail_add;

	dev = device_create(&hectic_class, NULL, hectic_devt, NULL, "hectic");
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		goto fail_dev;
	}

	return 0;

fail_dev:
	cdev_del(&hectic_cdev);
fail_add:
	unregister_chrdev_region(hectic_devt, 1);
fail_region:
	class_unregister(&hectic_class);

	return ret;
}
module_init(hectic_init);

static void __exit hectic_exit(void)
{
	device_destroy(&hectic_class, MKDEV(MAJOR(hectic_devt), 0));
	cdev_del(&hectic_cdev);
	class_unregister(&hectic_class);
}
module_exit(hectic_exit);

MODULE_LICENSE("GPL");
