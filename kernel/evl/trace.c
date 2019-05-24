/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/uaccess.h>
#include <evl/factory.h>
#include <uapi/evl/trace.h>
#include <trace/events/evl.h>

static long trace_common_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case EVL_TRCIOC_SNAPSHOT:
#ifdef CONFIG_TRACER_SNAPSHOT
		tracing_snapshot();
#endif
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static long trace_oob_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	return trace_common_ioctl(filp, cmd, arg);
}

static notrace ssize_t
trace_oob_write(struct file *filp,
		const char __user *u_buf, size_t count)
{
	char buf[128];
	int ret;

	if (count >= sizeof(buf))
		count = sizeof(buf) - 1;

	/* May be called from in-band context too. */
	ret = raw_copy_from_user(buf, u_buf, count);
	if (ret)
		return -EFAULT;

	buf[count] = '\0';

	/*
	 * trace_printk() is slow and triggers a scary and noisy
	 * warning at boot. Prefer a common tracepoint for issuing the
	 * message to the log.
	 */
	trace_evl_trace(buf);

	return count;
}

static long trace_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	return trace_common_ioctl(filp, cmd, arg);
}

static notrace
ssize_t trace_write(struct file *filp,
		const char __user *u_buf, size_t count,
		loff_t *ppos)
{
	return trace_oob_write(filp, u_buf, count);
}

static const struct file_operations trace_fops = {
	.unlocked_ioctl	=	trace_ioctl,
	.write		=	trace_write,
	.oob_ioctl	=	trace_oob_ioctl,
	.oob_write	=	trace_oob_write,
};

struct evl_factory evl_trace_factory = {
	.name	=	"trace",
	.fops	=	&trace_fops,
	.flags	=	EVL_FACTORY_SINGLE,
};
