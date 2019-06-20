/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <evl/file.h>
#include <evl/thread.h>
#include <evl/memory.h>
#include <evl/poll.h>
#include <evl/sched.h>
#include <evl/flag.h>
#include <evl/mutex.h>
#include <asm/evl/syscall.h>

struct poll_group {
	struct rb_root item_index;  /* struct poll_item */
	struct list_head item_list; /* struct poll_item */
	struct list_head waiter_list; /* struct poll_waiter */
	evl_spinlock_t wait_lock;
	struct evl_file efile;
	struct evl_kmutex item_lock;
	int nr_items;
	unsigned int generation;
};

struct poll_item {
	unsigned int fd;
	int events_polled;
	struct rb_node rb;	    /* in group->item_index */
	struct list_head next;	    /* in group->item_list */
};

struct poll_waiter {
	struct evl_flag flag;
	struct list_head next;
};

/* Maximum nesting depth (poll group watching other group(s)) */
#define POLLER_NEST_MAX  4

static const struct file_operations poll_fops;

void evl_poll_watch(struct evl_poll_head *head,
		struct oob_poll_wait *wait,
		void (*unwatch)(struct file *filp))
{
	struct evl_poll_watchpoint *wpt;
	unsigned long flags;

	wpt = container_of(wait, struct evl_poll_watchpoint, wait);
	/* Add to driver's poll head. */
	evl_spin_lock_irqsave(&head->lock, flags);
	wpt->head = head;
	wpt->events_received = 0;
	wpt->unwatch = unwatch;
	list_add(&wait->next, &head->watchpoints);
	evl_spin_unlock_irqrestore(&head->lock, flags);
}
EXPORT_SYMBOL_GPL(evl_poll_watch);

void __evl_signal_poll_events(struct evl_poll_head *head,
			int events)
{
	struct evl_poll_watchpoint *wpt;
	unsigned long flags;
	int ready;

	evl_spin_lock_irqsave(&head->lock, flags);

	list_for_each_entry(wpt, &head->watchpoints, wait.next) {
		ready = events & wpt->events_polled;
		if (ready) {
			wpt->events_received |= ready;
			evl_raise_flag_nosched(wpt->flag);
		}
	}

	evl_spin_unlock_irqrestore(&head->lock, flags);
}
EXPORT_SYMBOL_GPL(__evl_signal_poll_events);

void evl_drop_poll_table(struct evl_thread *thread)
{
	struct evl_poll_watchpoint *table;

	table = thread->poll_context.table;
	if (table)
		evl_free(table);
}

static inline
int index_item(struct rb_root *root, struct poll_item *item)
{
	struct rb_node **rbp, *parent = NULL;
	struct poll_item *tmp;

	rbp = &root->rb_node;
	while (*rbp) {
		tmp = rb_entry(*rbp, struct poll_item, rb);
		parent = *rbp;
		if (item->fd < tmp->fd)
			rbp = &(*rbp)->rb_left;
		else if (item->fd > tmp->fd)
			rbp = &(*rbp)->rb_right;
		else
			return -EEXIST;
	}

	rb_link_node(&item->rb, parent, rbp);
	rb_insert_color(&item->rb, root);

	return 0;
}

static inline void new_generation(struct poll_group *group)
{
	if (++group->generation == 0) /* Keep zero for init state. */
		group->generation = 1;
}

static int check_no_loop_deeper(struct poll_group *origin,
				struct poll_item *item,
				int depth)
{
	struct poll_group *group;
	struct poll_item *_item;
	struct evl_file *efilp;
	struct file *filp;
	int ret = 0;

	if (depth >= POLLER_NEST_MAX)
		return -EINVAL;

	efilp = evl_get_file(item->fd);
	if (efilp == NULL)
		return 0;

	filp = efilp->filp;
	if (filp->f_op != &poll_fops)
		goto out;

	group = filp->private_data;
	if (group == origin) {
		ret = -ELOOP;
		goto out;
	}

	evl_lock_kmutex(&group->item_lock);

	list_for_each_entry(_item, &group->item_list, next) {
		ret = check_no_loop_deeper(origin, _item, depth + 1);
		if (ret)
			break;
	}

	evl_unlock_kmutex(&group->item_lock);
out:
	evl_put_file(efilp);

	return ret;
}

static int check_no_loop(struct poll_group *group,
			struct poll_item *item)
{
	return check_no_loop_deeper(group, item, 0);
}

static int add_item(struct file *filp, struct poll_group *group,
		struct evl_poll_ctlreq *creq)
{
	struct poll_item *item;
	struct evl_file *efilp;
	int ret, events;

	item = evl_alloc(sizeof(*item));
	if (item == NULL)
		return -ENOMEM;

	item->fd = creq->fd;
	events = creq->events & ~POLLNVAL;
	item->events_polled = events | POLLERR | POLLHUP;

	efilp = evl_get_file(creq->fd);
	if (efilp == NULL) {
		ret = -EBADF;
		goto fail_get;
	}

	evl_lock_kmutex(&group->item_lock);

	/* Check for cyclic deps. */
	ret = check_no_loop(group, item);
	if (ret)
		goto fail_add;

	ret = index_item(&group->item_index, item);
	if (ret)
		goto fail_add;

	list_add(&item->next, &group->item_list);
	group->nr_items++;
	new_generation(group);

	evl_unlock_kmutex(&group->item_lock);
	evl_put_file(efilp);

	return 0;

fail_add:
	evl_unlock_kmutex(&group->item_lock);
	evl_put_file(efilp);
fail_get:
	evl_free(item);

	return ret;
}

static struct poll_item *
lookup_item(struct rb_root *root, unsigned int fd)
{
	struct poll_item *item;
	struct rb_node *rb;

	rb = root->rb_node;
	while (rb) {
		item = rb_entry(rb, struct poll_item, rb);
		if (fd < item->fd)
			rb = rb->rb_left;
		else if (fd > item->fd)
			rb = rb->rb_right;
		else
			return item;
	}

	return NULL;
}

static int del_item(struct poll_group *group,
		struct evl_poll_ctlreq *creq)
{
	struct poll_item *item;

	evl_lock_kmutex(&group->item_lock);

	item = lookup_item(&group->item_index, creq->fd);
	if (item == NULL) {
		evl_unlock_kmutex(&group->item_lock);
		return -ENOENT;
	}

	rb_erase(&item->rb, &group->item_index);
	list_del(&item->next);
	group->nr_items--;
	new_generation(group);

	evl_unlock_kmutex(&group->item_lock);

	evl_free(item);

	return 0;
}

/* fdt_lock held, irqs off. */
void evl_drop_watchpoints(struct list_head *drop_list)
{
	struct evl_poll_watchpoint *wpt;
	struct evl_poll_node *node;

	/*
	 * Drop the watchpoints attached to a closed file descriptor
	 * upon release from inband. A watchpoint found in @drop_list
	 * was registered via a call to evl_watch_fd() from
	 * wait_events() but not unregistered by calling
	 * evl_ignore_fd() from clear_wait() yet, so we know it is
	 * still valid. Since a polled EVL fd has to be passed to this
	 * routine before the file it references can be dismantled, we
	 * may keep and use a direct pointer to this file in the
	 * watchpoint struct until we return.
	 */
	list_for_each_entry(node, drop_list, next) {
		wpt = container_of(node, struct evl_poll_watchpoint, node);
		evl_spin_lock(&wpt->head->lock);
		wpt->events_received |= POLLNVAL;
		if (wpt->unwatch)
			wpt->unwatch(wpt->filp);
		evl_raise_flag_nosched(wpt->flag);
		evl_spin_unlock(&wpt->head->lock);
		wpt->filp = NULL;
	}
}

static inline
int mod_item(struct poll_group *group,
	struct evl_poll_ctlreq *creq)
{
	struct poll_item *item;
	int events;

	events = creq->events & ~POLLNVAL;

	evl_lock_kmutex(&group->item_lock);

	item = lookup_item(&group->item_index, creq->fd);
	if (item == NULL) {
		evl_unlock_kmutex(&group->item_lock);
		return -ENOENT;
	}

	item->events_polled = events | POLLERR | POLLHUP;
	new_generation(group);

	evl_unlock_kmutex(&group->item_lock);

	return 0;
}

static inline
int setup_item(struct file *filp, struct poll_group *group,
	struct evl_poll_ctlreq *creq)
{
	int ret;

	switch (creq->action) {
	case EVL_POLL_CTLADD:
		ret = add_item(filp, group, creq);
		break;
	case EVL_POLL_CTLDEL:
		ret = del_item(group, creq);
		break;
	case EVL_POLL_CTLMOD:
		ret = mod_item(group, creq);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int collect_events(struct poll_group *group,
			struct evl_poll_event __user *u_set,
			int maxevents, struct evl_flag *flag)
{
	struct evl_thread *curr = evl_current();
	struct evl_poll_watchpoint *wpt, *table;
	int ret, n, nr, count = 0, ready;
	struct evl_poll_event ev;
	unsigned int generation;
	struct poll_item *item;
	struct evl_file *efilp;
	struct file *filp;

	evl_lock_kmutex(&group->item_lock);

	nr = group->nr_items;
	if (nr == 0) {
		evl_unlock_kmutex(&group->item_lock);
		return -EINVAL;
	}

	/*
	 * Check whether the registered items are in sync with the
	 * caller's registered watchpoints (if any). Go polling
	 * directly using those watchpoints if so, otherwise resync.
	 */
	table = curr->poll_context.table;
	if (flag == NULL)
		goto collect;

	generation = group->generation;
	if (likely(generation == curr->poll_context.generation))
		goto collect;

	/* Need to resync. */
	do {
		generation = group->generation;
		evl_unlock_kmutex(&group->item_lock);
		evl_drop_poll_table(curr);
		table = evl_alloc(sizeof(*wpt) * nr);
		if (table == NULL) {
			curr->poll_context.nr = 0;
			curr->poll_context.table = NULL;
			curr->poll_context.generation = 0;
			return -ENOMEM;
		}
		evl_lock_kmutex(&group->item_lock);
	} while (generation != group->generation);

	curr->poll_context.table = table;
	curr->poll_context.nr = nr;
	curr->poll_context.generation = generation;

	/* Build the poll table. */
	wpt = table;
	list_for_each_entry(item, &group->item_list, next) {
		wpt->fd = item->fd;
		wpt->events_polled = item->events_polled;
		wpt++;
	}

collect:
	evl_unlock_kmutex(&group->item_lock);

	for (n = 0, wpt = table; n < nr; n++, wpt++) {
		if (flag) {
			wpt->flag = flag;
			INIT_LIST_HEAD(&wpt->wait.next);
			/* If oob_poll() is absent, default to all events ready. */
			ready = POLLIN|POLLOUT|POLLRDNORM|POLLWRNORM;
			efilp = evl_watch_fd(wpt->fd, &wpt->node);
			if (efilp == NULL)
				goto stale;
			filp = efilp->filp;
			wpt->filp = filp;
			if (filp->f_op->oob_poll)
				ready = filp->f_op->oob_poll(filp, &wpt->wait);
			evl_put_file(efilp);
		} else {
			ready = wpt->events_received;
			if (ready & POLLNVAL)
				goto stale;
		}

		ready &= wpt->events_polled;
		if (ready) {
			ev.fd = wpt->fd;
			ev.events = ready;
			ret = raw_copy_to_user(u_set, &ev, sizeof(ev));
			if (ret)
				return -EFAULT;
			u_set++;
			if (++count >= maxevents)
				break;
		}
	}

	return count;
stale:
	/*
	 * We have a stale fd in the table, force regeneration next
	 * time we collect events then bail out on error.
	 */
	evl_lock_kmutex(&group->item_lock);
	new_generation(group);
	evl_unlock_kmutex(&group->item_lock);

	return -EBADF;
}

static inline void clear_wait(void)
{
	struct evl_thread *curr = evl_current();
	struct evl_poll_watchpoint *wpt;
	unsigned long flags;
	int n;

	/*
	 * Current stopped waiting for events, remove the watchpoints
	 * we have been monitoring so far from their poll heads.
	 * wpt->head->lock serializes with __evl_signal_poll_events().
	 * Any watchpoint which does not bear the POLLNVAL bit is
	 * monitoring a still valid file by construction.
	 */
	for (n = 0, wpt = curr->poll_context.table;
	     n < curr->poll_context.nr; n++, wpt++) {
		evl_ignore_fd(&wpt->node);
		/* Remove from driver's poll head. */
		if (!list_empty(&wpt->wait.next)) {
			evl_spin_lock_irqsave(&wpt->head->lock, flags);
			list_del(&wpt->wait.next);
			if (!(wpt->events_received & POLLNVAL) && wpt->unwatch)
				wpt->unwatch(wpt->filp);
			evl_spin_unlock_irqrestore(&wpt->head->lock, flags);
		}
	}
}

static inline
int wait_events(struct file *filp,
		struct poll_group *group,
		struct evl_poll_waitreq *wreq)
{
	struct poll_waiter waiter;
	enum evl_tmode tmode;
	unsigned long flags;
	ktime_t timeout;
	int ret, count;

	if (wreq->nrset < 0)
		return -EINVAL;

	if ((unsigned long)wreq->timeout.tv_nsec >= ONE_BILLION)
		return -EINVAL;

	if (wreq->nrset == 0)
		return 0;

	evl_init_flag(&waiter.flag);

	count = collect_events(group, wreq->pollset, wreq->nrset, &waiter.flag);
	if (count > 0 || (count == -EFAULT || count == -EBADF))
		goto unwait;
	if (count < 0)
		goto out;

	if (filp->f_flags & O_NONBLOCK) {
		count = -EAGAIN;
		goto unwait;
	}

	timeout = timespec_to_ktime(wreq->timeout);
	tmode = timeout ? EVL_ABS : EVL_REL;

	evl_spin_lock_irqsave(&group->wait_lock, flags);
	list_add(&waiter.next, &group->waiter_list);
	evl_spin_unlock_irqrestore(&group->wait_lock, flags);
	ret = evl_wait_flag_timeout(&waiter.flag, timeout, tmode);
	evl_spin_lock_irqsave(&group->wait_lock, flags);
	list_del(&waiter.next);
	evl_spin_unlock_irqrestore(&group->wait_lock, flags);

	count = ret;
	if (count == 0)	/* Re-collect events after successful wait. */
		count = collect_events(group, wreq->pollset,
				wreq->nrset, NULL);
unwait:
	clear_wait();
out:
	evl_destroy_flag(&waiter.flag);

	return count;
}

static int poll_open(struct inode *inode, struct file *filp)
{
	struct poll_group *group;
	int ret;

	group = kzalloc(sizeof(*group), GFP_KERNEL);
	if (group == NULL)
		return -ENOMEM;

	ret = evl_open_file(&group->efile, filp);
	if (ret) {
		kfree(group);
		return ret;
	}

	group->item_index = RB_ROOT;
	INIT_LIST_HEAD(&group->item_list);
	INIT_LIST_HEAD(&group->waiter_list);
	evl_init_kmutex(&group->item_lock);
	evl_spin_lock_init(&group->wait_lock);
	filp->private_data = group;

	return ret;
}

static inline void flush_items(struct poll_group *group)
{
	struct poll_item *item, *n;

	list_for_each_entry_safe(item, n, &group->item_list, next)
		evl_free(item);
}

static int poll_release(struct inode *inode, struct file *filp)
{
	struct poll_group *group = filp->private_data;
	struct poll_waiter *waiter;
	unsigned long flags;

	evl_spin_lock_irqsave(&group->wait_lock, flags);
	list_for_each_entry(waiter, &group->waiter_list, next)
		evl_flush_flag_nosched(&waiter->flag, T_RMID);
	evl_spin_unlock_irqrestore(&group->wait_lock, flags);
	evl_schedule();

	flush_items(group);
	evl_release_file(&group->efile);
	kfree(group);

	return 0;
}

static long poll_oob_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct poll_group *group = filp->private_data;
	struct evl_poll_waitreq wreq, __user *u_wreq;
	struct evl_poll_ctlreq creq, __user *u_creq;
	int ret;

	switch (cmd) {
	case EVL_POLIOC_CTL:
		u_creq = (typeof(u_creq))arg;
		ret = raw_copy_from_user(&creq, u_creq, sizeof(creq));
		if (ret)
			return -EFAULT;
		ret = setup_item(filp, group, &creq);
		break;
	case EVL_POLIOC_WAIT:
		u_wreq = (typeof(u_wreq))arg;
		ret = raw_copy_from_user(&wreq, u_wreq, sizeof(wreq));
		if (ret)
			return -EFAULT;
		ret = wait_events(filp, group, &wreq);
		if (ret < 0)
			return ret;
		if (raw_put_user(ret, &u_wreq->nrset))
			return -EFAULT;
		ret = 0;
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static const struct file_operations poll_fops = {
	.open		= poll_open,
	.release	= poll_release,
	.oob_ioctl	= poll_oob_ioctl,
};

struct evl_factory evl_poll_factory = {
	.name	=	EVL_POLL_DEV,
	.fops	=	&poll_fops,
	.flags	=	EVL_FACTORY_SINGLE,
};
