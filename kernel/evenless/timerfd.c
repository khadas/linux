/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2013 Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <evenless/thread.h>
#include <evenless/clock.h>
#include <evenless/timer.h>
#include <evenless/lock.h>
#include <evenless/poller.h>
#include <evenless/sched.h>
#include <evenless/factory.h>
#include <asm/evenless/syscall.h>
#include <uapi/evenless/timerfd.h>
#include <trace/events/evenless.h>

struct evl_timerfd {
	struct evl_timer timer;
	struct evl_wait_queue readers;
	bool ticked;
	struct evl_poll_head poll_head;
	struct evl_element element;
};

static void get_timer_value(struct evl_timer *__restrict__ timer,
			struct itimerspec *__restrict__ value)
{
	value->it_interval = ktime_to_timespec(timer->interval);

	if (!evl_timer_is_running(timer)) {
		value->it_value.tv_sec = 0;
		value->it_value.tv_nsec = 0;
	} else
		value->it_value =
			ktime_to_timespec(evl_get_timer_delta(timer));
}

static int set_timer_value(struct evl_timer *__restrict__ timer,
			const struct itimerspec *__restrict__ value)
{
	ktime_t start, period;

	if (value->it_value.tv_nsec == 0 && value->it_value.tv_sec == 0) {
		evl_stop_timer(timer);
		return 0;
	}

	if ((unsigned long)value->it_value.tv_nsec >= ONE_BILLION ||
		((unsigned long)value->it_interval.tv_nsec >= ONE_BILLION &&
			(value->it_value.tv_sec != 0 || value->it_value.tv_nsec != 0)))
		return -EINVAL;

	period = timespec_to_ktime(value->it_interval);
	start = timespec_to_ktime(value->it_value);
	evl_start_timer(timer, start, period);

	return 0;
}

static int set_timerfd(struct evl_timerfd *timerfd,
		struct evl_timerfd_setreq *sreq)
{
	unsigned long flags;

	get_timer_value(&timerfd->timer, &sreq->ovalue);
	xnlock_get_irqsave(&nklock, flags);
	evl_set_timer_rq(&timerfd->timer, evl_current_rq());
	xnlock_put_irqrestore(&nklock, flags);

	return set_timer_value(&timerfd->timer, &sreq->value);
}

static long timerfd_oob_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	struct evl_timerfd *timerfd = element_of(filp, struct evl_timerfd);
	struct evl_timerfd_setreq sreq, __user *u_sreq;
	struct evl_timerfd_getreq greq, __user *u_greq;
	long ret = 0;

	switch (cmd) {
	case EVL_TFDIOC_SET:
		u_sreq = (typeof(u_sreq))arg;
		ret = raw_copy_from_user(&sreq, u_sreq, sizeof(sreq));
		if (ret)
			return -EFAULT;
		ret = set_timerfd(timerfd, &sreq);
		if (ret)
			return ret;
		if (raw_copy_to_user(&u_sreq->ovalue, &sreq.ovalue,
					sizeof(sreq.ovalue)))
			return -EFAULT;
		break;
	case EVL_TFDIOC_GET:
		get_timer_value(&timerfd->timer, &greq.value);
		u_greq = (typeof(u_greq))arg;
		if (raw_copy_to_user(&u_greq->value, &greq.value,
					sizeof(greq.value)))
			return -EFAULT;
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static void timerfd_handler(struct evl_timer *timer) /* hard IRQs off */
{
	struct evl_timerfd *timerfd;

	timerfd = container_of(timer, struct evl_timerfd, timer);
	timerfd->ticked = true;
	evl_signal_poll_events(&timerfd->poll_head, POLLIN);
	evl_flush_wait(&timerfd->readers, 0);
}

static bool read_timerfd_event(struct evl_timerfd *timerfd)
{
	if (timerfd->ticked) {
		timerfd->ticked = false;
		return true;
	}

	return false;
}

static ssize_t timerfd_oob_read(struct file *filp,
				char __user *u_buf, size_t count)
{
	struct evl_timerfd *timerfd = element_of(filp, struct evl_timerfd);
	__u64 __user *u_ticks = (__u64 __user *)u_buf, ticks = 0;
	ktime_t timeout = EVL_INFINITE;
	int ret;

	if (count < sizeof(ticks))
		return -EINVAL;

	if (filp->f_flags & O_NONBLOCK)
		timeout = EVL_NONBLOCK;

	ret = evl_wait_event_timeout(&timerfd->readers, timeout,
			EVL_REL, read_timerfd_event(timerfd));
	if (ret)
		return ret;

	ticks = 1;
	if (evl_timer_is_periodic(&timerfd->timer))
		ticks += evl_get_timer_overruns(&timerfd->timer);

	evl_clear_poll_events(&timerfd->poll_head, POLLIN);

	if (raw_put_user(ticks, u_ticks))
		return -EFAULT;

	return sizeof(ticks);
}

static __poll_t timerfd_oob_poll(struct file *filp,
				struct oob_poll_wait *wait)
{
	struct evl_timerfd *timerfd = element_of(filp, struct evl_timerfd);

	evl_poll_watch(&timerfd->poll_head, wait);

	return timerfd->ticked ? POLLIN|POLLRDNORM : 0;
}

static const struct file_operations timerfd_fops = {
	.open		= evl_open_element,
	.release	= evl_release_element,
	.oob_ioctl	= timerfd_oob_ioctl,
	.oob_read	= timerfd_oob_read,
	.oob_poll	= timerfd_oob_poll,
};

static struct evl_element *
timerfd_factory_build(struct evl_factory *fac, const char *name,
		void __user *u_attrs, u32 *state_offp)
{
	struct evl_timerfd_attrs attrs;
	struct evl_timerfd *timerfd;
	struct evl_clock *clock;
	int ret;

	ret = copy_from_user(&attrs, u_attrs, sizeof(attrs));
	if (ret)
		return ERR_PTR(-EFAULT);

	clock = evl_get_clock_by_fd(attrs.clockfd);
	if (clock == NULL)
		return ERR_PTR(-EINVAL);

	timerfd = kzalloc(sizeof(*timerfd), GFP_KERNEL);
	if (timerfd == NULL) {
		ret = -ENOMEM;
		goto fail_alloc;
	}

	ret = evl_init_element(&timerfd->element,
			&evl_timerfd_factory);
	if (ret)
		goto fail_element;

	evl_init_timer(&timerfd->timer, clock, timerfd_handler,
		NULL, EVL_TIMER_UGRAVITY);
	evl_init_wait(&timerfd->readers, clock, EVL_WAIT_PRIO);
	evl_init_poll_head(&timerfd->poll_head);

	return &timerfd->element;

fail_element:
	kfree(timerfd);
fail_alloc:
	evl_put_clock(clock);

	return ERR_PTR(ret);
}

static void timerfd_factory_dispose(struct evl_element *e)
{
	struct evl_timerfd *timerfd;

	timerfd = container_of(e, struct evl_timerfd, element);

	evl_destroy_timer(&timerfd->timer);
	evl_put_clock(timerfd->readers.clock);
	evl_destroy_wait(&timerfd->readers);
	evl_destroy_element(&timerfd->element);

	kfree_rcu(timerfd, element.rcu);
}

struct evl_factory evl_timerfd_factory = {
	.name	=	"timerfd",
	.fops	=	&timerfd_fops,
	.build =	timerfd_factory_build,
	.dispose =	timerfd_factory_dispose,
	.nrdev	=	CONFIG_EVENLESS_NR_TIMERFDS,
	.flags	=	EVL_FACTORY_CLONE,
};
