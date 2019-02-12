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
#include <linux/pid.h>
#include <linux/module.h>
#include <evenless/file.h>
#include <evenless/thread.h>
#include <evenless/memory.h>
#include <evenless/poller.h>
#include <evenless/sched.h>
#include <evenless/flag.h>
#include <evenless/mutex.h>
#include <asm/evenless/syscall.h>

struct event_poller {
	struct rb_root node_index;  /* struct poll_node */
	struct list_head node_list;  /* struct poll_node */
	struct evl_wait_queue wait_queue;
	struct evl_element element;
	struct evl_kmutex lock;
	int nodenr;
	unsigned int generation;
	struct pid *owner;
};

struct poll_node {
	unsigned int fd;
	struct evl_file *efilp;
	int events_polled;
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
	struct evl_flag *flag;
	struct evl_poll_head *head;
};

/* Maximum nesting depth (poller watching poller(s) */
#define POLLER_NEST_MAX  4

static const struct file_operations poller_fops;

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
	struct evl_poll_watchpoint *wpt;
	unsigned long flags;
	int ready;

	raw_spin_lock_irqsave(&head->lock, flags);

	list_for_each_entry(wpt, &head->watchpoints, wait.next) {
		ready = events & wpt->node.events_polled;
		if (ready) {
			wpt->events_received |= ready;
			evl_raise_flag_nosched(wpt->flag);
		}
	}

	raw_spin_unlock_irqrestore(&head->lock, flags);

	evl_schedule();
}
EXPORT_SYMBOL_GPL(__evl_signal_poll_events);

void __evl_clear_poll_events(struct evl_poll_head *head,
			int events)
{
	struct evl_poll_watchpoint *wpt;
	unsigned long flags;

	raw_spin_lock_irqsave(&head->lock, flags);

	list_for_each_entry(wpt, &head->watchpoints, wait.next)
		wpt->events_received &= ~events;

	raw_spin_unlock_irqrestore(&head->lock, flags);
}
EXPORT_SYMBOL_GPL(__evl_clear_poll_events);

void evl_drop_poll_table(struct evl_thread *thread)
{
	struct evl_poll_watchpoint *table;

	table = thread->poll_context.table;
	if (table)
		evl_free(table);
}

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

static inline void new_generation(struct event_poller *poller)
{
	if (++poller->generation == 0) /* Keep zero for init state. */
		poller->generation = 1;
}

static int check_no_loop(struct file *origin, struct file *child)
{
	if (origin == child)
		return -EINVAL;

	return 0;
}

static int add_node(struct file *filp, struct event_poller *poller,
		struct evl_poller_ctlreq *creq)
{
	struct poll_node *node;
	int ret;

	node = evl_alloc(sizeof(*node));
	if (node == NULL)
		return -ENOMEM;

	node->fd = creq->fd;
	node->events_polled = creq->events;

	node->efilp = evl_get_file(creq->fd);
	if (node->efilp == NULL) {
		ret = -EBADF;
		goto fail_get;
	}

	/* Make sure not to have a poller watch itself. */
	ret = check_no_loop(filp, node->efilp->filp);
	if (ret)
		goto fail_check;

	evl_lock_kmutex(&poller->lock);

	ret = __index_node(&poller->node_index, node);
	if (ret)
		goto fail_add;

	list_add(&node->next, &poller->node_list);
	poller->nodenr++;
	new_generation(poller);

	evl_unlock_kmutex(&poller->lock);

	return 0;

fail_add:
	evl_unlock_kmutex(&poller->lock);
fail_check:
	evl_put_file(node->efilp);
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
	evl_put_file(node->efilp);
	evl_free(node);
}

static int del_node(struct event_poller *poller,
		struct evl_poller_ctlreq *creq)
{
	struct poll_node *node;

	evl_lock_kmutex(&poller->lock);

	node = lookup_node(&poller->node_index, creq->fd);
	if (node == NULL) {
		evl_unlock_kmutex(&poller->lock);
		return -EBADF;
	}

	rb_erase(&node->rb, &poller->node_index);
	list_del(&node->next);
	poller->nodenr--;
	new_generation(poller);

	evl_unlock_kmutex(&poller->lock);

	__del_node(node);

	return 0;
}

static inline
int mod_node(struct event_poller *poller,
	struct evl_poller_ctlreq *creq)
{
	struct poll_node *node;

	evl_lock_kmutex(&poller->lock);

	node = lookup_node(&poller->node_index, creq->fd);
	if (node == NULL) {
		evl_unlock_kmutex(&poller->lock);
		return -EBADF;
	}

	node->events_polled = creq->events;
	new_generation(poller);

	evl_unlock_kmutex(&poller->lock);

	return 0;
}

static inline
int setup_node(struct file *filp, struct event_poller *poller,
	struct evl_poller_ctlreq *creq)
{
	int ret;

	switch (creq->action) {
	case EVL_POLLER_CTLADD:
		ret = add_node(filp, poller, creq);
		break;
	case EVL_POLLER_CTLDEL:
		ret = del_node(poller, creq);
		break;
	case EVL_POLLER_CTLMOD:
		ret = mod_node(poller, creq);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int collect_events(struct event_poller *poller,
			struct evl_poll_event __user *u_set,
			int maxevents, struct evl_flag *flag)
{
	struct evl_thread *curr = evl_current();
	struct evl_poll_watchpoint *wpt, *table;
	int ret, n, nr, count = 0, ready;
	struct evl_poll_event ev;
	unsigned int generation;
	struct poll_node *node;
	struct file *filp;

	evl_lock_kmutex(&poller->lock);

	nr = poller->nodenr;
	if (nr == 0) {
		evl_unlock_kmutex(&poller->lock);
		return -EINVAL;
	}

	/*
	 * Check whether the registered nodes are in sync with the
	 * caller's registered watchpoints (if any). Go polling
	 * directly using those watchpoints if so, otherwise resync.
	 */
	table = curr->poll_context.table;
	if (flag == NULL)
		goto collect;

	generation = poller->generation;
	if (likely(generation == curr->poll_context.generation)) {
		list_for_each_entry(node, &poller->node_list, next)
			evl_get_fileref(node->efilp);
		goto collect;
	}

	/* Need to resync. */
	do {
		generation = poller->generation;
		evl_unlock_kmutex(&poller->lock);
		evl_drop_poll_table(curr);
		table = evl_alloc(sizeof(*wpt) * nr);
		if (table == NULL) {
			curr->poll_context.nr = 0;
			curr->poll_context.table = NULL;
			curr->poll_context.generation = 0;
			return -ENOMEM;
		}
		evl_lock_kmutex(&poller->lock);
	} while (generation != poller->generation);

	curr->poll_context.table = table;
	curr->poll_context.nr = nr;
	curr->poll_context.generation = generation;

	/* Build the poll table. */
	wpt = table;
	list_for_each_entry(node, &poller->node_list, next) {
		evl_get_fileref(node->efilp);
		wpt->node = *node;
		wpt++;
	}

collect:
	evl_unlock_kmutex(&poller->lock);

	/*
	 * Provided that each f_op->release of the OOB drivers maintaining
	 * wpt->node.efilp is properly calling evl_release_file()
	 * before it dismantles the file, having a reference on
	 * wpt->efilp guarantees us that wpt->efilp->filp is stable
	 * until the last ref. is dropped by evl_put_file().
	 */
	for (n = 0, wpt = table; n < nr; n++, wpt++) {
		if (flag) {
			wpt->flag = flag;
			/* If oob_poll() is absent, default to all events ready. */
			ready = POLLIN|POLLOUT|POLLRDNORM|POLLWRNORM;
			filp = wpt->node.efilp->filp;
			if (filp->f_op->oob_poll)
				ready = filp->f_op->oob_poll(filp, &wpt->wait);
		} else
			ready = wpt->events_received;

		ready &= wpt->node.events_polled;
		if (ready) {
			ev.fd = wpt->node.fd;
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
	 */
	for (n = 0, wpt = curr->poll_context.table;
	     n < curr->poll_context.nr; n++, wpt++) {
		raw_spin_lock_irqsave(&wpt->head->lock, flags);
		list_del(&wpt->wait.next);
		raw_spin_unlock_irqrestore(&wpt->head->lock, flags);
		evl_put_file(wpt->node.efilp);
	}
}

static inline
int wait_events(struct file *filp,
		struct event_poller *poller,
		struct evl_poller_waitreq *wreq)
{
	DEFINE_EVL_FLAG(flag);
	enum evl_tmode tmode;
	ktime_t timeout;
	int ret, count;

	if (wreq->nrset < 0 || wreq->nrset > poller->nodenr)
		return -EINVAL;

	if ((unsigned long)wreq->timeout.tv_nsec >= ONE_BILLION)
		return -EINVAL;

	if (wreq->nrset == 0)
		return 0;

	count = collect_events(poller, wreq->pollset, wreq->nrset, &flag);
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
	ret = evl_wait_flag_timeout(&flag, timeout, tmode);
	if (ret == 0)
		count = collect_events(poller, wreq->pollset,
				wreq->nrset, NULL);
	else
		count = ret;
unwait:
	clear_wait();
	evl_destroy_flag(&flag);

	return count;
}

static int poller_open(struct inode *inode, struct file *filp)
{
	struct task_struct *group_leader;
	struct event_poller *poller;
	int ret;

	ret = evl_open_element(inode, filp);
	if (ret)
		return ret;

	/*
	 * A poller is privately owned by the process which
	 * instantiates it, as it implicitly refers to its file
	 * table. Deny threads from other processes from opening it.
	 */
	poller = element_of(filp, struct event_poller);
	rcu_read_lock();
	group_leader = pid_task(poller->owner, PIDTYPE_PID);
	if (group_leader != current->group_leader)
		ret = -EPERM;
	rcu_read_unlock();

	return ret;
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
		ret = setup_node(filp, poller, &creq);
		break;
	case EVL_POLIOC_WAIT:
		u_wreq = (typeof(u_wreq))arg;
		ret = raw_copy_from_user(&wreq, u_wreq, sizeof(wreq));
		if (ret)
			return -EFAULT;
		ret = wait_events(filp, poller, &wreq);
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

static const struct file_operations poller_fops = {
	.open		= poller_open,
	.release	= evl_release_element,
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
	evl_init_kmutex(&poller->lock);
	poller->owner = get_task_pid(current->group_leader, PIDTYPE_PID);

	return &poller->element;

fail_element:
	kfree(poller);
fail_alloc:
	evl_put_clock(clock);

	return ERR_PTR(ret);
}

static inline void flush_nodes(struct event_poller *poller)
{
	struct poll_node *node, *n;

	list_for_each_entry_safe(node, n, &poller->node_list, next)
		__del_node(node);
}

static void poller_factory_dispose(struct evl_element *e)
{
	struct event_poller *poller;

	poller = container_of(e, struct event_poller, element);

	put_pid(poller->owner);
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
