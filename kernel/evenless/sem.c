/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <evenless/synch.h>
#include <evenless/thread.h>
#include <evenless/clock.h>
#include <evenless/sem.h>
#include <evenless/memory.h>
#include <evenless/lock.h>
#include <evenless/factory.h>
#include <evenless/sched.h>
#include <asm/evenless/syscall.h>
#include <trace/events/evenless.h>

struct evl_sem {
	struct evl_element element;
	struct evl_sem_state *state;
	struct evl_syn wait_queue;
};

struct sem_wait_data {
	int count;
};

static int acquire_sem(struct evl_sem *sem,
		       struct evl_sem_waitreq *req)
{
	struct evl_thread *curr = evl_current_thread();
	struct evl_sem_state *state = sem->state;
	struct sem_wait_data wda;
	enum evl_tmode tmode;
	int info, ret = 0;
	ktime_t timeout;

	if (req->count <= 0)
		return -EINVAL;

	if ((unsigned long)req->timeout.tv_nsec >= ONE_BILLION)
		return -EINVAL;

	if (state->flags & EVL_SEM_PULSE)
		req->count = 1;

	if (atomic_sub_return(req->count, &state->value) >= 0)
		return 0;

	wda.count = req->count;
	curr->wait_data = &wda;

	timeout = timespec_to_ktime(req->timeout);
	tmode = timeout ? EVL_ABS : EVL_REL;
	info = evl_sleep_on_syn(&sem->wait_queue, timeout, tmode);
	if (info & (T_BREAK|T_BCAST|T_TIMEO)) {
		atomic_add(req->count, &state->value);
		ret = -ETIMEDOUT;
		if (info & T_BREAK)
			ret = -EINTR;
		else if (info & T_BCAST)
			ret = -EAGAIN;
	} /* No way we could receive T_RMID */

	return ret;
}

static int release_sem(struct evl_sem *sem, int count)
{
	struct evl_sem_state *state = sem->state;
	struct evl_thread *waiter, *n;
	struct sem_wait_data *wda;
	unsigned long flags;
	int oldval;

	if (count <= 0)
		return -EINVAL;

	oldval = atomic_read(&state->value);
	if (oldval + count < 0)
		return -EINVAL;

	if (state->flags & EVL_SEM_PULSE)
		count = 1;

	if (atomic_add_return(count, &state->value) >= count) {
		/* Old value >= 0, nobody is waiting. */
		if (state->flags & EVL_SEM_PULSE)
			atomic_set(&state->value, 0);
		return 0;
	}

	xnlock_get_irqsave(&nklock, flags);

	if (!evl_syn_has_waiter(&sem->wait_queue))
		goto out;

	/*
	 * Try waking up waiters. The top waiter must progress, do not
	 * serve other waiters down the queue until this one is
	 * satisfied.
	 */
	evl_for_each_syn_waiter_safe(waiter, n, &sem->wait_queue) {
		wda = waiter->wait_data;
		if (atomic_sub_return(wda->count, &state->value) < 0) {
			atomic_add(wda->count, &state->value); /* Nope, undo. */
			break;
		}
		evl_wake_up_targeted_syn(&sem->wait_queue, waiter);
	}

	evl_schedule();
out:
	xnlock_put_irqrestore(&nklock, flags);

	return 0;
}

static int broadcast_sem(struct evl_sem *sem)
{
	if (evl_flush_syn(&sem->wait_queue, T_BCAST))
		evl_schedule();

	return 0;
}

static long sem_common_ioctl(struct evl_sem *sem,
			     unsigned int cmd, unsigned long arg)
{
	__s32 count;
	long ret;

	switch (cmd) {
	case EVL_SEMIOC_PUT:
		ret = raw_get_user(count, (__s32 *)arg);
		if (ret)
			return -EFAULT;
		ret = release_sem(sem, count);
		break;
	case EVL_SEMIOC_BCAST:
		ret = broadcast_sem(sem);
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static long sem_oob_ioctl(struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	struct evl_sem *sem = element_of(filp, struct evl_sem);
	struct evl_sem_waitreq wreq, __user *u_wreq;
	long ret;

	switch (cmd) {
	case EVL_SEMIOC_GET:
		u_wreq = (typeof(u_wreq))arg;
		ret = raw_copy_from_user(&wreq, u_wreq, sizeof(wreq));
		if (ret)
			return -EFAULT;
		ret = acquire_sem(sem, &wreq);
		break;
	default:
		ret = sem_common_ioctl(sem, cmd, arg);
	}

	return ret;
}

static long sem_ioctl(struct file *filp, unsigned int cmd,
		      unsigned long arg)
{
	struct evl_sem *sem = element_of(filp, struct evl_sem);
	struct evl_element_ids eids, __user *u_eids;

	if (cmd != EVL_SEMIOC_BIND)
		return sem_common_ioctl(sem, cmd, arg);

	eids.minor = sem->element.minor;
	eids.state_offset = evl_shared_offset(sem->state);
	eids.fundle = fundle_of(sem);
	u_eids = (typeof(u_eids))arg;

	return copy_to_user(u_eids, &eids, sizeof(eids)) ? -EFAULT : 0;
}

static const struct file_operations sem_fops = {
	.open		= evl_open_element,
	.release	= evl_close_element,
	.unlocked_ioctl	= sem_ioctl,
	.oob_ioctl	= sem_oob_ioctl,
};

static struct evl_element *
sem_factory_build(struct evl_factory *fac, const char *name,
		  void __user *u_attrs, u32 *state_offp)
{
	struct evl_sem_state *state;
	struct evl_sem_attrs attrs;
	struct evl_clock *clock;
	int synflags = 0, ret;
	struct evl_sem *sem;

	ret = copy_from_user(&attrs, u_attrs, sizeof(attrs));
	if (ret)
		return ERR_PTR(-EFAULT);

	if (attrs.flags & ~(EVL_SEM_PRIO|EVL_SEM_PULSE))
		return ERR_PTR(-EINVAL);

	if (attrs.initval < 0)
		return ERR_PTR(-EINVAL);

	if ((attrs.flags & EVL_SEM_PULSE) && attrs.initval > 0)
		return ERR_PTR(-EINVAL);

	clock = evl_get_clock_by_fd(attrs.clockfd);
	if (clock == NULL)
		return ERR_PTR(-EINVAL);

	sem = kzalloc(sizeof(*sem), GFP_KERNEL);
	if (sem == NULL) {
		ret = -ENOMEM;
		goto fail_alloc;
	}

	ret = evl_init_element(&sem->element, &evl_sem_factory);
	if (ret)
		goto fail_element;

	state = evl_alloc_chunk(&evl_shared_heap, sizeof(*state));
	if (state == NULL) {
		ret = -ENOMEM;
		goto fail_heap;
	}

	if (attrs.flags & EVL_SEM_PRIO)
		synflags |= EVL_SYN_PRIO;

	evl_init_syn(&sem->wait_queue, synflags, clock, NULL);
	atomic_set(&state->value, attrs.initval);
	state->flags = attrs.flags;
	sem->state = state;

	*state_offp = evl_shared_offset(state);
	evl_index_element(&sem->element);

	return &sem->element;

fail_heap:
	evl_destroy_element(&sem->element);
fail_element:
	kfree(sem);
fail_alloc:
	evl_put_clock(clock);

	return ERR_PTR(ret);
}

static void sem_factory_dispose(struct evl_element *e)
{
	struct evl_sem *sem;

	sem = container_of(e, struct evl_sem, element);

	evl_unindex_element(&sem->element);
	evl_put_clock(sem->wait_queue.clock);
	evl_destroy_syn(&sem->wait_queue);
	evl_free_chunk(&evl_shared_heap, sem->state);
	evl_destroy_element(&sem->element);
	kfree_rcu(sem, element.rcu);
}

static ssize_t value_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct evl_sem *sem;
	ssize_t ret;

	sem = evl_get_element_by_dev(dev, struct evl_sem);
	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		       atomic_read(&sem->state->value));
	evl_put_element(&sem->element);

	return ret;
}
static DEVICE_ATTR_RO(value);

static struct attribute *sem_attrs[] = {
	&dev_attr_value.attr,
	NULL,
};
ATTRIBUTE_GROUPS(sem);

struct evl_factory evl_sem_factory = {
	.name	=	"semaphore",
	.fops	=	&sem_fops,
	.build =	sem_factory_build,
	.dispose =	sem_factory_dispose,
	.nrdev	=	CONFIG_EVENLESS_NR_SEMAPHORES,
	.attrs	=	sem_groups,
	.flags	=	EVL_FACTORY_CLONE,
};
