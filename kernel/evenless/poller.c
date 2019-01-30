/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <evenless/file.h>
#include <evenless/thread.h>
#include <evenless/memory.h>
#include <evenless/poller.h>
#include <evenless/sched.h>
#include <asm/evenless/syscall.h>

struct event_poller {
	struct rb_root node_index;  /* struct poll_node */
	struct list_head node_list;  /* struct poll_node */
	struct evl_wait_queue wait_queue;
	struct evl_element element;
	hard_spinlock_t lock;
	int nodenr;
	unsigned int generation;
	bool readied;
	struct list_head next;	/* in private wake up list */
};

struct poll_node {
	unsigned int fd;
	struct evl_file *sfilp;
	int events_polled;
	struct event_poller *poller;
	struct rb_node rb;	/* in poller->node_index */
	struct list_head next;	/* in poller->node_list */
};

/*
 * The watchpoint struct linked to poll heads by drivers. This watches
 * files not elements, so that we can monitor any type of EVL files.
 */
struct evl_poll_watchpoint {
	struct poll_node node;
	int events_received;
	struct oob_poll_wait wait;
	struct evl_poll_head *head;
};

void evl_poll_watch(struct evl_poll_head *head,
		struct oob_poll_wait *wait)
{
	struct evl_poll_watchpoint *wpt;
	unsigned long flags;

	wpt = container_of(wait, struct evl_poll_watchpoint, wait);
	wpt->head = head;
	raw_spin_lock_irqsave(&head->lock, flags);
	wpt->events_received = 0;
	list_add(&wait->next, &head->watchpoints);
	raw_spin_unlock_irqrestore(&head->lock, flags);
}
EXPORT_SYMBOL_GPL(evl_poll_watch);

void __evl_signal_poll_events(struct evl_poll_head *head,
			int events)
{
	struct evl_poll_watchpoint *wpt, *n;
	struct event_poller *poller;
	LIST_HEAD(wakeup_list);
	unsigned long flags;
	int ready;

	raw_spin_lock_irqsave(&head->lock, flags);

	if (!list_empty(&head->watchpoints)) {
		list_for_each_entry_safe(wpt, n, &head->watchpoints, wait.next) {
			ready = events & wpt->node.events_polled;
			if (ready) {
				wpt->events_received |= ready;
				list_add(&wpt->node.poller->next, &wakeup_list);
			}
		}
	}

	raw_spin_unlock_irqrestore(&head->lock, flags);

	if (!list_empty(&wakeup_list)) {
		list_for_each_entry(poller, &wakeup_list, next) {
			xnlock_get_irqsave(&nklock, flags);
			poller->readied = true;
			evl_wake_up_head(&poller->wait_queue);
			xnlock_put_irqrestore(&nklock, flags);
		}
	}

	evl_schedule();
}
EXPORT_SYMBOL_GPL(__evl_signal_poll_events);

void __evl_clear_poll_events(struct evl_poll_head *head,
			int events)
{
	struct evl_poll_watchpoint *wpt;
	unsigned long flags;

	raw_spin_lock_irqsave(&head->lock, flags);

	if (!list_empty(&head->watchpoints))
		list_for_each_entry(wpt, &head->watchpoints, wait.next)
			wpt->events_received &= ~events;

	raw_spin_unlock_irqrestore(&head->lock, flags);
}
EXPORT_SYMBOL_GPL(__evl_clear_poll_events);

static inline
int __index_node(struct rb_root *root, struct poll_node *node)
{
	struct rb_node **rbp, *parent = NULL;
	struct poll_node *tmp;

	rbp = &root->rb_node;
	while (*rbp) {
		tmp = rb_entry(*rbp, struct poll_node, rb);
		parent = *rbp;
		if (node->fd < tmp->fd)
			rbp = &(*rbp)->rb_left;
		else if (node->fd > tmp->fd)
			rbp = &(*rbp)->rb_right;
		else
			return -EEXIST;
	}

	rb_link_node(&node->rb, parent, rbp);
	rb_insert_color(&node->rb, root);

	return 0;
}

static int add_node(struct event_poller *poller,
		struct evl_poller_ctlreq *creq)
{
	struct poll_node *node;
	unsigned long flags;
	int ret;

	node = evl_alloc(sizeof(*node));
	if (node == NULL)
		return -ENOMEM;

	node->fd = creq->fd;
	node->events_polled = creq->events;

	node->sfilp = evl_get_file(creq->fd);
	if (node->sfilp == NULL) {
		ret = -EBADF;
		goto fail_get;
	}

	raw_spin_lock_irqsave(&poller->lock, flags);

	ret = __index_node(&poller->node_index, node);
	if (ret)
		goto fail_add;

	list_add(&node->next, &poller->node_list);
	poller->nodenr++;
	if (++poller->generation == 0) /* Keep zero for init state. */
		poller->generation = 1;

	raw_spin_unlock_irqrestore(&poller->lock, flags);

	return 0;

fail_add:
	raw_spin_unlock_irqrestore(&poller->lock, flags);
	evl_put_file(node->sfilp);
fail_get:
	evl_free(node);

	return ret;
}

static struct poll_node *
lookup_node(struct rb_root *root, unsigned int fd)
{
	struct poll_node *node;
	struct rb_node *rb;

	rb = root->rb_node;
	while (rb) {
		node = rb_entry(rb, struct poll_node, rb);
		if (fd < node->fd)
			rb = rb->rb_left;
		else if (fd > node->fd)
			rb = rb->rb_right;
		else
			return node;
	}

	return NULL;
}

static void __del_node(struct poll_node *node)
{
	evl_put_file(node->sfilp);
	evl_free(node);
}

static int del_node(struct event_poller *poller,
		struct evl_poller_ctlreq *creq)
{
	struct poll_node *node;
	unsigned long flags;

	raw_spin_lock_irqsave(&poller->lock, flags);

	node = lookup_node(&poller->node_index, creq->fd);
	if (node == NULL) {
		raw_spin_unlock_irqrestore(&poller->lock, flags);
		return -EBADF;
	}

	rb_erase(&node->rb, &poller->node_index);
	list_del(&node->next);
	poller->nodenr--;
	poller->generation++;

	raw_spin_unlock_irqrestore(&poller->lock, flags);

	__del_node(node);

	return 0;
}

static inline
int mod_node(struct event_poller *poller,
	struct evl_poller_ctlreq *creq)
{
	struct poll_node *node;
	unsigned long flags;

	raw_spin_lock_irqsave(&poller->lock, flags);

	node = lookup_node(&poller->node_index, creq->fd);
	if (node == NULL) {
		raw_spin_unlock_irqrestore(&poller->lock, flags);
		return -EBADF;
	}

	node->events_polled = creq->events;
	poller->generation++;

	raw_spin_unlock_irqrestore(&poller->lock, flags);

	return 0;
}

static inline
int setup_node(struct event_poller *poller,
	struct evl_poller_ctlreq *creq)
{
	int ret;

	switch (creq->action) {
	case EVL_POLLER_CTLADD:
		ret = add_node(poller, creq);
		break;
	case EVL_POLLER_CTLDEL:
		ret = del_node(poller, creq);
		break;
	case EVL_POLLER_CTLMOD:
		ret = mod_node(poller, creq);
		break;
	}

	return -EINVAL;
}

static int collect_events(struct event_poller *poller,
			struct evl_poll_event __user *u_ev,
			int maxevents, bool do_poll)
{
	struct evl_thread *curr = evl_current_thread();
	struct evl_poll_watchpoint *wpt, *table;
	int ret, n, nr, count = 0, ready;
	struct evl_poll_event ev;
	struct poll_node *node;
	unsigned int generation;
	unsigned long flags;
	struct file *filp;

	raw_spin_lock_irqsave(&poller->lock, flags);

	nr = poller->nodenr;
	if (nr == 0) {
		raw_spin_unlock_irqrestore(&poller->lock, flags);
		return -EINVAL;
	}

	/*
	 * Check whether the registered nodes are in sync with the
	 * caller's registered watchpoints (if any). Go polling
	 * directly using those watchpoints if so, otherwise resync.
	 */
	table = curr->poll_context.table;
	generation = poller->generation;
	if (likely(generation == curr->poll_context.generation || !do_poll))
		goto collect;

	/* Need to resync. */
	do {
		generation = poller->generation;
		raw_spin_unlock_irqrestore(&poller->lock, flags);
		if (table)
			evl_free(table);
		table = evl_alloc(sizeof(*wpt) * nr);
		if (table == NULL) {
			curr->poll_context.nr = 0;
			curr->poll_context.table = NULL;
			curr->poll_context.generation = 0;
			return -ENOMEM;
		}
		raw_spin_lock_irqsave(&poller->lock, flags);
	} while (generation != poller->generation);

	curr->poll_context.table = table;
	curr->poll_context.nr = nr;
	curr->poll_context.generation = generation;

	wpt = table;
	list_for_each_entry(node, &poller->node_list, next) {
		evl_get_fileref(node->sfilp);
		wpt->node = *node;
		wpt++;
	}

collect:
	raw_spin_unlock_irqrestore(&poller->lock, flags);

	/*
	 * Provided that each f_op->release of the OOB drivers maintaining
	 * wpt->node.sfilp is properly calling evl_release_file()
	 * before it dismantles the file, having a reference on
	 * wpt->sfilp guarantees us that wpt->sfilp->filp is stable
	 * until the last ref. is dropped via evl_put_file().
	 */
	for (n = 0, wpt = table; n < nr; n++, wpt++) {
		if (do_poll) {
			ready = POLLIN|POLLOUT|POLLRDNORM|POLLWRNORM; /* Default. */
			filp = wpt->node.sfilp->filp;
			if (filp->f_op->oob_poll)
				ready = filp->f_op->oob_poll(filp, &wpt->wait);
		} else
			ready = wpt->events_received & wpt->node.events_polled;

		if (ready) {
			ev.fd = wpt->node.fd;
			ev.events = ready;
			ret = raw_copy_to_user(u_ev, &ev, sizeof(ev));
			if (ret)
				return -EFAULT;
			u_ev++;
			if (++count >= maxevents)
				break;
		}
	}

	return count;
}

static inline void clear_wait(void)
{
	struct evl_thread *curr = evl_current_thread();
	struct evl_poll_watchpoint *wpt;
	unsigned long flags;
	int n;

	/*
	 * Current stopped waiting for events, remove the watchpoints
	 * we have been monitoring so far from their poll heads.
	 * wpt->head->lock serializes with __evl_signal_poll_events().
	 */
	for (n = 0, wpt = curr->poll_context.table;
	     n < curr->poll_context.nr; n++, wpt++) {
		raw_spin_lock_irqsave(&wpt->head->lock, flags);
		list_del(&wpt->wait.next);
		raw_spin_unlock_irqrestore(&wpt->head->lock, flags);
		evl_put_file(wpt->node.sfilp);
	}
}

static inline
int wait_events(struct file *filp,
		struct event_poller *poller,
		struct evl_poller_waitreq *wreq)
{
	enum evl_tmode tmode;
	unsigned long flags;
	ktime_t timeout;
	int info, count;

	if (wreq->nrevents < 0 || wreq->nrevents > poller->nodenr)
		return -EINVAL;

	if ((unsigned long)wreq->timeout.tv_nsec >= ONE_BILLION)
		return -EINVAL;

	if (wreq->nrevents == 0)
		return 0;

	count = collect_events(poller, wreq->events,
			wreq->nrevents, true);
	if (count > 0 || count == -EFAULT)
		goto unwait;
	if (count < 0)
		return count;

	if (filp->f_flags & O_NONBLOCK) {
		count = -EAGAIN;
		goto unwait;
	}

	timeout = timespec_to_ktime(wreq->timeout);
	tmode = timeout ? EVL_ABS : EVL_REL;

	xnlock_get_irqsave(&nklock, flags);

	info = 0;
	if (!poller->readied)
		info = evl_wait_timeout(&poller->wait_queue, timeout, tmode);

	xnlock_put_irqrestore(&nklock, flags);
	if (info)
		/* No way we could have received T_RMID. */
		count = info & T_BREAK ? -EINTR : -ETIMEDOUT;
	else
		count = collect_events(poller, wreq->events,
				wreq->nrevents, false);
unwait:
	clear_wait();

	return count;
}

static long poller_oob_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct event_poller *poller = element_of(filp, struct event_poller);
	struct evl_poller_waitreq wreq, __user *u_wreq;
	struct evl_poller_ctlreq creq, __user *u_creq;
	int ret;

	switch (cmd) {
	case EVL_POLIOC_CTL:
		u_creq = (typeof(u_creq))arg;
		ret = raw_copy_from_user(&creq, u_creq, sizeof(creq));
		if (ret)
			return -EFAULT;
		ret = setup_node(poller, &creq);
		break;
	case EVL_POLIOC_WAIT:
		u_wreq = (typeof(u_wreq))arg;
		ret = raw_copy_from_user(&wreq, u_wreq, sizeof(wreq));
		if (ret)
			return -EFAULT;
		ret = wait_events(filp, poller, &wreq);
		if (ret >= 0 && raw_put_user(ret, &u_wreq->nrevents))
			return -EFAULT;
		ret = 0;
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static const struct file_operations poller_fops = {
	.open		= evl_open_element,
	.release	= evl_close_element,
	.oob_ioctl	= poller_oob_ioctl,
};

static struct evl_element *
poller_factory_build(struct evl_factory *fac, const char *name,
		void __user *u_attrs, u32 *state_offp)
{
	struct evl_poller_attrs attrs;
	struct event_poller *poller;
	struct evl_clock *clock;
	int ret;

	ret = copy_from_user(&attrs, u_attrs, sizeof(attrs));
	if (ret)
		return ERR_PTR(-EFAULT);

	clock = evl_get_clock_by_fd(attrs.clockfd);
	if (clock == NULL)
		return ERR_PTR(-EINVAL);

	poller = kzalloc(sizeof(*poller), GFP_KERNEL);
	if (poller == NULL) {
		ret = -ENOMEM;
		goto fail_alloc;
	}

	ret = evl_init_element(&poller->element, &evl_poller_factory);
	if (ret)
		goto fail_element;

	poller->node_index = RB_ROOT;
	INIT_LIST_HEAD(&poller->node_list);
	evl_init_wait(&poller->wait_queue, clock, EVL_WAIT_PRIO);
	raw_spin_lock_init(&poller->lock);

	return &poller->element;

fail_element:
	kfree(poller);
fail_alloc:
	evl_put_clock(clock);

	return ERR_PTR(ret);
}

static inline void flush_nodes(struct event_poller *poller)
{
	struct poll_node *node;

	if (!list_empty(&poller->node_list))
		list_for_each_entry(node, &poller->node_list, next)
			__del_node(node);
}

static void poller_factory_dispose(struct evl_element *e)
{
	struct event_poller *poller;

	poller = container_of(e, struct event_poller, element);

	flush_nodes(poller);
	evl_put_clock(poller->wait_queue.clock);
	evl_destroy_element(&poller->element);
	kfree_rcu(poller, element.rcu);
}

struct evl_factory evl_poller_factory = {
	.name	=	"poller",
	.fops	=	&poller_fops,
	.build =	poller_factory_build,
	.dispose =	poller_factory_dispose,
	.nrdev	=	CONFIG_EVENLESS_NR_POLLERS,
	.flags	=	EVL_FACTORY_CLONE,
};
