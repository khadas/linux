/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/poll.h>
#include <linux/rbtree.h>
#include <evl/list.h>
#include <evl/sched.h>
#include <evl/thread.h>
#include <evl/clock.h>
#include <evl/observable.h>
#include <evl/factory.h>

/* Co-exists with EVL_NOTIFY_MASK bits. */
#define EVL_NOTIFY_INITIAL	(1 << 31)

#define EVL_OBSERVABLE_CLONE_FLAGS	\
	(EVL_CLONE_PUBLIC|EVL_CLONE_OBSERVABLE|EVL_CLONE_MASTER)

/*
 * We want any kind of threads to be able to subscribe to an
 * observable including non-EVL ones, evl_subscriber holds the
 * information we need for this. This descriptor is accessed directly
 * from the task_struct via the oob thread state.
 */
struct evl_subscriber {
  	struct rb_root subscriptions; /* struct evl_observer */
	evl_spinlock_t lock;
};

struct evl_notification_record {
	u32 tag;
	u32 serial;
	pid_t issuer;
	union evl_value event;
	ktime_t date;
	struct list_head next;	/* in evl_observers.(free|pending)_list */
};

struct evl_observer {
	size_t backlog_size;
	struct list_head free_list;	/* evl_notification_record */
	struct list_head pending_list;	/* evl_notification_record */
	struct list_head next;		/* in evl_observable.observers */
	fundle_t fundle;		/* fundle of observable */
	struct rb_node rb;		/* in evl_subscriber.subscriptions */
	int flags;			/* EVL_NOTIFY_xx */
	int refs;			/* notification vs unsubscription */
	struct evl_notice last_notice;
	struct evl_notification_record backlog[0];
};

static const struct file_operations observable_fops;

static inline
int index_observer(struct rb_root *root, struct evl_observer *observer)
{
	struct rb_node **rbp, *parent = NULL;
	struct evl_observer *tmp;

	rbp = &root->rb_node;
	while (*rbp) {
		tmp = rb_entry(*rbp, struct evl_observer, rb);
		parent = *rbp;
		if (observer->fundle < tmp->fundle)
			rbp = &(*rbp)->rb_left;
		else if (observer->fundle > tmp->fundle)
			rbp = &(*rbp)->rb_right;
		else
			return -EEXIST;
	}

	rb_link_node(&observer->rb, parent, rbp);
	rb_insert_color(&observer->rb, root);

	return 0;
}

static struct evl_observer *
find_observer(struct rb_root *root, fundle_t fundle)
{
	struct evl_observer *observer;
	struct rb_node *rb;

	rb = root->rb_node;
	while (rb) {
		observer = rb_entry(rb, struct evl_observer, rb);
		if (fundle < observer->fundle)
			rb = rb->rb_left;
		else if (fundle > observer->fundle)
			rb = rb->rb_right;
		else
			return observer;
	}

	return NULL;
}

/*
 * Threads can only subscribe/unsubscribe to/from observables for
 * themselves, which guarantees that no observer can go away while a
 * thread is accessing it from [oob_]read()/write() operations.  An
 * observer only knows about the observable's fundle, it does not
 * maintain any memory reference to the former: we want lose coupling
 * in this direction so that observables can go away while subscribers
 * still target them.
 */
static int add_subscription(struct evl_observable *observable,
			unsigned int backlog_count, int op_flags) /* in-band */
{
	struct evl_subscriber *sbr = evl_get_subscriber();
	struct evl_notification_record *nf;
	struct evl_observer *observer;
	size_t backlog_size;
	unsigned long flags;
	unsigned int n;
	int ret;

	inband_context_only();

	if (backlog_count == 0)
		return -EINVAL;

	if (op_flags & ~EVL_NOTIFY_MASK)
		return -EINVAL;

	if (sbr == NULL) {
		sbr = kzalloc(sizeof(*sbr), GFP_KERNEL);
		if (sbr == NULL)
			return -ENOMEM;
		evl_spin_lock_init(&sbr->lock);
		sbr->subscriptions = RB_ROOT;
		evl_set_subscriber(sbr);
	}

	/*
	 * We don't need to guard against racing with in-band file
	 * release since the caller must be running in-band, so the
	 * VFS already ensures consistency between release vs other
	 * in-band I/O ops. We do need a reference on the underlying
	 * element though.
	 */
	evl_get_element(&observable->element);

	/*
	 * The observer descriptor and its backlog are adjacent in
	 * memory. The backlog contains notification records, align on
	 * this element size.
	 */
	backlog_size = backlog_count * sizeof(*nf);
	observer = kzalloc(sizeof(*observer) + backlog_size, GFP_KERNEL);
	if (observer == NULL) {
		ret = -ENOMEM;
		goto fail_alloc;
	}

	observer->fundle = fundle_of(observable);
	observer->backlog_size = backlog_size;
	observer->flags = op_flags;
	observer->refs = 1;
	INIT_LIST_HEAD(&observer->free_list);
	INIT_LIST_HEAD(&observer->pending_list);

	if (op_flags & EVL_NOTIFY_ONCHANGE)
		observer->flags |= EVL_NOTIFY_INITIAL;

	evl_spin_lock_irqsave(&sbr->lock, flags);
	ret = index_observer(&sbr->subscriptions, observer);
	evl_spin_unlock_irqrestore(&sbr->lock, flags);
	if (ret)
		goto dup_subscription;

	/* Build the free list of notification records. */
	for (nf = observer->backlog, n = 0; n < backlog_count; n++, nf++)
		list_add(&nf->next, &observer->free_list);

	/*
	 * Make the new observer visible to the observable. We need
	 * the observer and writability tracking to be atomically
	 * updated, so nested locking is required. Observers are
	 * linked to the observable's list by order of subscription
	 * (FIFO) in case users make such assumption for master mode
	 * (which undergoes round-robin).
	 */
	evl_spin_lock_irqsave(&observable->lock, flags);
	list_add_tail(&observer->next, &observable->observers);
	evl_spin_lock(&observable->oob_wait.lock);
	observable->writable_observers++;
	evl_spin_unlock(&observable->oob_wait.lock);
	evl_spin_unlock_irqrestore(&observable->lock, flags);
	evl_signal_poll_events(&observable->poll_head, POLLOUT|POLLWRNORM);

	evl_put_element(&observable->element);

	return 0;

dup_subscription:
	kfree(observer);
fail_alloc:
	evl_put_element(&observable->element);

	return ret;
}

/* observable->oob_wait.lock held, irqs off */
static void decrease_writability(struct evl_observable *observable)
{
	observable->writable_observers--;
	EVL_WARN_ON(CORE, observable->writable_observers < 0);
}

/* observable->lock held, irqs off */
static void detach_observer_locked(struct evl_observer *observer,
		struct evl_observable *observable) /* irqs off */
{
	list_del(&observer->next);

	evl_spin_lock(&observable->oob_wait.lock);

	if (!list_empty(&observer->free_list))
		decrease_writability(observable);

	evl_spin_unlock(&observable->oob_wait.lock);
}

static inline void get_observer(struct evl_observer *observer)
{
	observer->refs++;
}

static inline bool put_observer(struct evl_observer *observer)
{
	int new_refs = --observer->refs;

	EVL_WARN_ON(CORE, new_refs < 0);

	return new_refs == 0;
}

static bool detach_observer(struct evl_observer *observer,
			struct evl_observable *observable)
{
	bool dropped = false;
	unsigned long flags;

	evl_spin_lock_irqsave(&observable->lock, flags);

	/*
	 * If the refcount does not drop to zero, push_notification()
	 * will flush the observer later on when it is safe.
	 */
	if (likely(put_observer(observer))) {
		detach_observer_locked(observer, observable);
		dropped = true;
	}

	evl_spin_unlock_irqrestore(&observable->lock, flags);

	return dropped;
}

static int cancel_subscription(struct evl_observable *observable) /* in-band */
{
	struct evl_subscriber *sbr = evl_get_subscriber();
	struct evl_observer *observer;
	unsigned long flags;
	bool dropped;

	inband_context_only();

	if (sbr == NULL)
		return -ENOENT;

	evl_get_element(&observable->element);

	observer = find_observer(&sbr->subscriptions,
				fundle_of(observable));
	if (observer == NULL) {
		evl_put_element(&observable->element);
		return -ENOENT;
	}

	/*
	 * Remove observer from the subscription index. Once done,
	 * this observer escapes evl_drop_subscriptions() in case the
	 * caller exits afterwards.
	 */
	evl_spin_lock_irqsave(&sbr->lock, flags);
	rb_erase(&observer->rb, &sbr->subscriptions);
	evl_spin_unlock_irqrestore(&sbr->lock, flags);

	/*
	 * Remove observer from observable->observers. Subscription
	 * cannot be stale since observable is still around (meaning
	 * observable_release() did not run).
	 */
	dropped = detach_observer(observer, observable);

	evl_put_element(&observable->element);

	if (dropped)
		kfree(observer);

	return 0;
}

/*
 * Current is wrapping up, drop the associated subscriber information
 * if any, no need to lock subscriptions.
 */
void evl_drop_subscriptions(struct evl_subscriber *sbr)
{
	struct evl_observable *observable;
	struct evl_observer *observer;
	bool dropped = true;
	struct rb_node *rb;

	if (sbr == NULL)
		return;

	while ((rb = sbr->subscriptions.rb_node) != NULL) {
		observer = rb_entry(rb, struct evl_observer, rb);
		rb_erase(rb, &sbr->subscriptions);
		/* Some subscriptions might be stale, check this. */
		if (likely(!list_empty(&observer->next))) {
			observable = evl_get_factory_element_by_fundle(
				&evl_observable_factory,
				observer->fundle,
				struct evl_observable);
			if (!EVL_WARN_ON(CORE, observable == NULL)) {
				dropped = detach_observer(observer, observable);
				evl_put_element(&observable->element);
			} else {
				/* Something is going seriously wrong. */
				dropped = false;
			}
		}
		if (dropped)
			kfree(observer);
	}

	kfree(sbr);
}

static void wake_oob_threads(struct evl_observable *observable)
{
	unsigned long flags;

	evl_spin_lock_irqsave(&observable->oob_wait.lock, flags);

	if (evl_wait_active(&observable->oob_wait))
		evl_flush_wait_locked(&observable->oob_wait, 0);

	evl_spin_unlock_irqrestore(&observable->oob_wait.lock, flags);

	evl_schedule();
}

static void wake_inband_threads(struct evl_observable *observable) /* in-band */
{
	wake_up_all(&observable->inband_wait);
}

static void inband_wake_irqwork(struct irq_work *work)
{
	struct evl_observable *observable;

	observable = container_of(work, struct evl_observable, wake_irqwork);
	wake_inband_threads(observable);
	evl_put_element(&observable->element);
}

void evl_flush_observable(struct evl_observable *observable)
{
	struct evl_observer *observer, *tmp;
	unsigned long flags;

	/*
	 * We may be called from do_cleanup_current() in which case
	 * some events might still being pushed to the observable via
	 * the thread file descriptor, so locking is required.
	 */
	evl_spin_lock_irqsave(&observable->oob_wait.lock, flags);

	list_for_each_entry_safe(observer, tmp, &observable->observers, next) {
		list_del_init(&observer->next);
		if (!list_empty(&observer->free_list))
			decrease_writability(observable);
	}

	evl_spin_unlock_irqrestore(&observable->oob_wait.lock, flags);

	wake_oob_threads(observable);
}

static int observable_release(struct inode *inode, struct file *filp)
{
	struct evl_observable *observable = element_of(filp, struct evl_observable);

	/*
	 * Unbind observers, then wake up so that oob waiters can
	 * figure out that we forced unsubscribed them. Those
	 * subscriptions are now stale. There is no point in
	 * attempting to wake up in-band waiters, there cannot be any
	 * since no regular read()/write() request could be ongoing on
	 * this file since the VFS wants us to release it.
	 */
	evl_flush_observable(observable);

	/*
	 * Wait for any ongoing oob request to have completed before
	 * allowing the VFS to fully dismantle filp.
	 */
	return evl_release_element(inode, filp);
}

static void do_flush_work(struct evl_observable *observable)
{
	struct evl_observer *observer;
	unsigned long flags;

	/*
	 * Flush the observers pending deletion. This should happen
	 * only rarely, in case push_notification() raced with
	 * cancel_subscription() or evl_drop_subscriptions().
	 */
	evl_spin_lock_irqsave(&observable->lock, flags);

	while (!list_empty(&observable->flush_list)) {
		observer = list_get_entry(&observable->flush_list,
					struct evl_observer, next);
		evl_spin_unlock_irqrestore(&observable->lock, flags);
		kfree(observer);
		evl_spin_lock_irqsave(&observable->lock, flags);
	}

	evl_spin_unlock_irqrestore(&observable->lock, flags);
}

static void inband_flush_irqwork(struct irq_work *work)
{
	struct evl_observable *observable;

	observable = container_of(work, struct evl_observable, flush_irqwork);
	do_flush_work(observable);
	evl_put_element(&observable->element);
}

static bool notify_one_observer(struct evl_observable *observable,
			struct evl_observer *observer,
			const struct evl_notification_record *tmpl_nfr)
{
	struct evl_notification_record *nfr;
	struct evl_notice last_notice;
	unsigned long flags;

	evl_spin_lock_irqsave(&observable->oob_wait.lock, flags);

	if (observer->flags & EVL_NOTIFY_ONCHANGE) {
		last_notice = observer->last_notice;
		observer->last_notice.tag = tmpl_nfr->tag;
		observer->last_notice.event = tmpl_nfr->event;
		if (unlikely(observer->flags & EVL_NOTIFY_INITIAL))
			observer->flags &= ~EVL_NOTIFY_INITIAL;
		else if (tmpl_nfr->tag == last_notice.tag &&
			tmpl_nfr->event.lval == last_notice.event.lval)
			goto done;
	}

	if (list_empty(&observer->free_list)) {
		evl_spin_unlock_irqrestore(&observable->oob_wait.lock, flags);
		return false;
	}

	nfr = list_get_entry(&observer->free_list,
			struct evl_notification_record, next);

	*nfr = *tmpl_nfr;
	list_add_tail(&nfr->next, &observer->pending_list);

	if (list_empty(&observer->free_list))
		decrease_writability(observable);
done:
	evl_spin_unlock_irqrestore(&observable->oob_wait.lock, flags);

	return true;
}

static bool notify_single(struct evl_observable *observable,
			struct evl_notification_record *nfr,
			ssize_t *len_r)
{
	struct evl_observer *observer;
	bool do_flush = false;
	unsigned long flags;

	evl_spin_lock_irqsave(&observable->lock, flags);

	if (list_empty(&observable->observers))
		goto out;

	nfr->serial = observable->serial_counter++;

	observer = list_first_entry(&observable->observers,
				struct evl_observer, next);
	get_observer(observer);

	/*
	 * Use round-robin as a naive load-balancing strategy among
	 * multiple observers forming a pool of worker threads.
	 */
	if (!list_is_singular(&observable->observers)) {
		list_del(&observer->next);
		list_add_tail(&observer->next, &observable->observers);
	}

	evl_spin_unlock_irqrestore(&observable->lock, flags);

	if (notify_one_observer(observable, observer, nfr))
		*len_r += sizeof(struct evl_notice);

	evl_spin_lock_irqsave(&observable->lock, flags);

	if (unlikely(put_observer(observer))) {
		detach_observer_locked(observer, observable);
		list_add(&observer->next, &observable->flush_list);
		do_flush = true;
	}
out:
	evl_spin_unlock_irqrestore(&observable->lock, flags);

	return do_flush;
}

static bool notify_all(struct evl_observable *observable,
			struct evl_notification_record *nfr,
			ssize_t *len_r)
{
	struct list_head *pos, *nextpos;
	struct evl_observer *observer;
	unsigned long flags;
	bool do_flush;

	evl_spin_lock_irqsave(&observable->lock, flags);

	if (list_empty(&observable->observers)) {
		do_flush = false;
		goto out;
	}

	nfr->serial = observable->serial_counter++;

	observer = list_first_entry(&observable->observers,
				struct evl_observer, next);
	get_observer(observer);

	/*
	 * Prevent cancel_subscription() and evl_drop_subscriptions()
	 * from dropping the observers we are about to push the
	 * notification to, without locking the observable all over
	 * the list walk. The trick is to hold a reference both on the
	 * current and next items during the traversal. Observers
	 * which might be added to the list concurrently by
	 * add_subscription() would not be picked during the
	 * traversal, which is still logically correct.
	 */
	list_for_each_safe(pos, nextpos, &observable->observers) {

		if (likely(nextpos != &observable->observers)) {
			observer = list_entry(nextpos, struct evl_observer, next);
			get_observer(observer);
		}

		observer = list_entry(pos, struct evl_observer, next);

		evl_spin_unlock_irqrestore(&observable->lock, flags);

		if (notify_one_observer(observable, observer, nfr))
			*len_r += sizeof(struct evl_notice);

		evl_spin_lock_irqsave(&observable->lock, flags);

		if (unlikely(put_observer(observer))) {
			detach_observer_locked(observer, observable);
			list_add(&observer->next, &observable->flush_list);
		}
	}

	do_flush = !list_empty(&observable->flush_list);
out:
	evl_spin_unlock_irqrestore(&observable->lock, flags);

	return do_flush;
}

static bool is_pool_master(struct evl_observable *observable)
{
	return !!(observable->element.clone_flags & EVL_CLONE_MASTER);
}

static bool push_notification(struct evl_observable *observable,
			const struct evl_notice *ntc,
			ktime_t date)
{
	struct evl_notification_record nfr;
	ssize_t len = 0;
	bool do_flush;

	/*
	 * Prep a template notification record so that multiple
	 * observers receive exactly the same notification data for
	 * any single change, timestamp included.
	 */
	nfr.tag = ntc->tag;
	nfr.event = ntc->event;
	nfr.issuer = ntc->tag >= EVL_NOTICE_USER ? task_pid_nr(current) : 0;
	nfr.date = date;

	/*
	 * Feed one observer/worker if master, or broadcast to all
	 * observers.
	 */
	if (is_pool_master(observable))
		do_flush = notify_single(observable, &nfr, &len);
	else
		do_flush = notify_all(observable, &nfr, &len);

	if (unlikely(do_flush)) {
		if (running_inband()) {
			do_flush_work(observable);
		} else {
			evl_get_element(&observable->element);
			if (!irq_work_queue(&observable->flush_irqwork))
				evl_put_element(&observable->element);
		}
	}

	return len > 0;  /* True if some subscribers could receive. */
}

static void wake_up_observers(struct evl_observable *observable)
{
	wake_oob_threads(observable);
	evl_signal_poll_events(&observable->poll_head, POLLIN|POLLRDNORM);

	/*
	 * If running oob, we need to go through the wake_irqwork
	 * trampoline for waking up in-band waiters.
	 */
	if (running_inband()) {
		wake_inband_threads(observable);
	} else {
		evl_get_element(&observable->element);
		if (!irq_work_queue(&observable->wake_irqwork))
			evl_put_element(&observable->element);
	}
}

bool evl_send_observable(struct evl_observable *observable, int tag,
			union evl_value details)
{
	struct evl_notice ntc;
	ktime_t now;

	ntc.tag = tag;
	ntc.event = details;
	now = evl_ktime_monotonic();

	if (push_notification(observable, &ntc, now)) {
		wake_up_observers(observable);
		return true;
	}

	return false;
}
EXPORT_SYMBOL_GPL(evl_send_observable);

ssize_t evl_write_observable(struct evl_observable *observable,
			const char __user *u_buf, size_t count)
{
	ssize_t len = 0, xlen = 0;
	struct evl_notice ntc;
	ktime_t now;
	int ret = 0;

	if (!evl_element_is_observable(&observable->element))
		return -ENXIO;

	if (count == 0)
		return 0;

	if (count % sizeof(struct evl_notice))
		return -EINVAL;	/* No partial write. */

	now = evl_ktime_monotonic();

	while (xlen < count) {
		if (raw_copy_from_user(&ntc, u_buf, sizeof(ntc))) {
			ret = -EFAULT;
			break;
		}

		if (ntc.tag < EVL_NOTICE_USER) {
			ret = -EINVAL;
			break;
		}

		if (push_notification(observable, &ntc, now))
			len += sizeof(ntc);

		xlen += sizeof(ntc);
		u_buf += sizeof(ntc);
	}

	/* If some notification was delivered, wake observers up. */
	if (len > 0)
		wake_up_observers(observable);

	return ret ?: len;
}

static struct evl_notification_record *
pull_from_oob(struct evl_observable *observable,
	struct evl_observer *observer,
	bool wait)
{
	struct evl_notification_record *nfr;
	unsigned long flags;
	int ret;

	evl_spin_lock_irqsave(&observable->oob_wait.lock, flags);

	/*
	 * observable->wait.lock guards the pending and free
	 * notification lists of all observers subscribed to it.
	 */
	for (;;) {
		if (list_empty(&observer->next)) {
			/* Unsubscribed by observable_release(). */
			nfr = ERR_PTR(-EBADF);
			goto out;
		}
		if (!list_empty(&observer->pending_list))
			break;
		if (!wait) {
			nfr = ERR_PTR(-EWOULDBLOCK);
			goto out;
		}
		evl_add_wait_queue(&observable->oob_wait, EVL_INFINITE, EVL_REL);
		evl_spin_unlock_irqrestore(&observable->oob_wait.lock, flags);
		ret = evl_wait_schedule(&observable->oob_wait);
		if (ret)
			return ERR_PTR(ret);
		evl_spin_lock_irqsave(&observable->oob_wait.lock, flags);
	}

	nfr = list_get_entry(&observer->pending_list,
			struct evl_notification_record, next);
out:
	evl_spin_unlock_irqrestore(&observable->oob_wait.lock, flags);

	return nfr;
}

static struct evl_notification_record *
pull_from_inband(struct evl_observable *observable,
		struct evl_observer *observer,
		bool wait)
{
	struct evl_notification_record *nfr = NULL;
	unsigned long ib_flags, oob_flags;
	struct wait_queue_entry wq_entry;
	int ret = 0;

	init_wait_entry(&wq_entry, 0);

	/*
	 * In-band lock first, oob next. See stax implementation for
	 * an explanation.
	 */
	spin_lock_irqsave(&observable->inband_wait.lock, ib_flags);
	evl_spin_lock_irqsave(&observable->oob_wait.lock, oob_flags);

	for (;;) {
		/*
		 * No need to check for observer->next linkage when
		 * handling read(): observable_release() did not run
		 * for the underlying file, obviously.
		 */
		if (list_empty(&wq_entry.entry))
			__add_wait_queue(&observable->inband_wait, &wq_entry);

		if (!list_empty(&observer->pending_list)) {
			nfr = list_get_entry(&observer->pending_list,
					struct evl_notification_record, next);
			break;
		}
		if (!wait) {
			ret = -EWOULDBLOCK;
			break;
		}
		set_current_state(TASK_INTERRUPTIBLE);
		evl_spin_unlock_irqrestore(&observable->oob_wait.lock, oob_flags);
		spin_unlock_irqrestore(&observable->inband_wait.lock, ib_flags);
		schedule();
		spin_lock_irqsave(&observable->inband_wait.lock, ib_flags);
		evl_spin_lock_irqsave(&observable->oob_wait.lock, oob_flags);

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}
	}

	list_del(&wq_entry.entry);

	evl_spin_unlock_irqrestore(&observable->oob_wait.lock, oob_flags);
	spin_unlock_irqrestore(&observable->inband_wait.lock, ib_flags);

	return ret ? ERR_PTR(ret) : nfr;
}

static int pull_notification(struct evl_observable *observable,
			struct evl_observer *observer,
			char __user *u_buf,
			bool wait)
{
	struct evl_notification_record *nfr;
	struct __evl_notification nf;
	bool sigpoll = false;
	unsigned long flags;
	int ret;

	if (running_inband())
		nfr = pull_from_inband(observable, observer, wait);
	else
		nfr = pull_from_oob(observable, observer, wait);

	if (IS_ERR(nfr))
		return PTR_ERR(nfr);

	nf.tag = nfr->tag;
	nf.event = nfr->event;
	nf.issuer = nfr->issuer;
	nf.serial = nfr->serial;
	nf.date = ktime_to_u_timespec(nfr->date);

	/*
	 * Do not risk losing notifications because of a broken
	 * userland; re-post them at front in case we fail copying the
	 * information back. A more recent record might have slipped
	 * in since we dropped the lock, but monotonic timestamps can
	 * still be used to sort out order of arrival.
	 */
	ret = raw_copy_to_user(u_buf, &nf, sizeof(nf));

	evl_spin_lock_irqsave(&observable->oob_wait.lock, flags);

	if (ret) {
		list_add(&nfr->next, &observer->pending_list);
		ret = -EFAULT;
	} else {
		if (list_empty(&observer->free_list)) {
			observable->writable_observers++;
			sigpoll = true;
		}
		list_add(&nfr->next, &observer->free_list);
	}

	evl_spin_unlock_irqrestore(&observable->oob_wait.lock, flags);

	if (unlikely(sigpoll))
		evl_signal_poll_events(&observable->poll_head,
				POLLOUT|POLLWRNORM);

	return ret;
}

ssize_t evl_read_observable(struct evl_observable *observable,
			char __user *u_buf, size_t count, bool wait)
{
	struct evl_subscriber *sbr = evl_get_subscriber();
	struct evl_observer *observer;
	ssize_t len = 0;
	int ret = 0;

	/*
	 * The caller must have a valid subscription for reading an
	 * observable.
	 */
	if (sbr == NULL)
		return -ENXIO;

	if (!evl_element_is_observable(&observable->element))
		return -ENXIO;

	/*
	 * Invariant: the caller owns observer (if found), nobody else
	 * may free it. This is _not_ true for push_notification()
	 * walking the list of observers which may belong to different
	 * threads.
	 */
	observer = find_observer(&sbr->subscriptions, fundle_of(observable));
	if (observer == NULL)
		return -ENXIO;	/* Not subscribed. */

	if (count == 0)
		return 0;

	if (count % sizeof(struct __evl_notification))
		return -EINVAL;	/* No partial read. */
	/*
	 * Return as many available notifications we can, waiting for
	 * the first one if necessary.
	 */
	while (len < count) {
		ret = pull_notification(observable, observer, u_buf, wait);
		if (ret)
			break;
		wait = false;
		u_buf += sizeof(struct __evl_notification);
		len += sizeof(struct __evl_notification);
	}

	return len ?: ret;
}

static __poll_t poll_observable(struct evl_observable *observable)
{
	struct evl_subscriber *sbr = evl_get_subscriber();
	struct evl_observer *observer;
	unsigned long flags;
	__poll_t ret = 0;

	if (sbr == NULL)
		return POLLERR;

	if (!evl_element_is_observable(&observable->element))
		return POLLERR;

	observer = find_observer(&sbr->subscriptions, fundle_of(observable));

	evl_spin_lock_irqsave(&observable->oob_wait.lock, flags);

	/* Only subscribers can inquire about readability. */
	if (observer && !list_empty(&observer->pending_list))
		ret = POLLIN|POLLRDNORM;

	if (observable->writable_observers > 0)
		ret |= POLLOUT|POLLWRNORM;

	evl_spin_unlock_irqrestore(&observable->oob_wait.lock, flags);

	return ret;
}

__poll_t evl_oob_poll_observable(struct evl_observable *observable,
				struct oob_poll_wait *wait)
{
	evl_poll_watch(&observable->poll_head, wait, NULL);

	return poll_observable(observable);
}

__poll_t evl_poll_observable(struct evl_observable *observable,
			struct file *filp, poll_table *pt)
{
	poll_wait(filp, &observable->inband_wait, pt);

	return poll_observable(observable);
}

long evl_ioctl_observable(struct evl_observable *observable, unsigned int cmd,
			unsigned long arg)
{
	struct evl_subscription sub, *u_sub;
	int ret;

	switch (cmd) {
	case EVL_OBSIOC_SUBSCRIBE:
		/*
		 * Add a subscription to @observable for the current
		 * thread, which may not be attached.
		 */
		u_sub = (typeof(u_sub))arg;
		ret = raw_copy_from_user(&sub, u_sub, sizeof(sub));
		if (ret)
			return -EFAULT;
		ret = add_subscription(observable,
				sub.backlog_count, sub.flags);
		break;
	case EVL_OBSIOC_UNSUBSCRIBE:
		/* Likewise for unsubscribing from @observable. */
		ret = cancel_subscription(observable);
		break;
	default:
		ret = -ENOTTY;
	}

	evl_schedule();

	return ret;
}

static ssize_t observable_oob_write(struct file *filp,
				const char __user *u_buf, size_t count)
{
	struct evl_observable *observable = element_of(filp, struct evl_observable);

	return evl_write_observable(observable, u_buf, count);
}

static ssize_t observable_oob_read(struct file *filp,
				char __user *u_buf, size_t count)
{
	struct evl_observable *observable = element_of(filp, struct evl_observable);

	return evl_read_observable(observable, u_buf, count,
				!(filp->f_flags & O_NONBLOCK));
}

static __poll_t observable_oob_poll(struct file *filp,
			struct oob_poll_wait *wait)
{
	struct evl_observable *observable = element_of(filp, struct evl_observable);

	return evl_oob_poll_observable(observable, wait);
}

static ssize_t observable_write(struct file *filp, const char __user *u_buf,
				size_t count, loff_t *ppos)
{
	struct evl_observable *observable = element_of(filp, struct evl_observable);

	return evl_write_observable(observable, u_buf, count);
}

static ssize_t observable_read(struct file *filp, char __user *u_buf,
			size_t count, loff_t *ppos)
{
	struct evl_observable *observable = element_of(filp, struct evl_observable);

	return evl_read_observable(observable, u_buf, count,
				!(filp->f_flags & O_NONBLOCK));
}

static __poll_t observable_poll(struct file *filp, poll_table *pt)
{
	struct evl_observable *observable = element_of(filp, struct evl_observable);

	return evl_poll_observable(observable, filp, pt);
}

static long observable_ioctl(struct file *filp, unsigned int cmd,
			unsigned long arg)
{
	struct evl_observable *observable = element_of(filp, struct evl_observable);

	return evl_ioctl_observable(observable, cmd, arg);
}

static const struct file_operations observable_fops = {
	.open		= evl_open_element,
	.release	= observable_release,
	.oob_write	= observable_oob_write,
	.oob_read	= observable_oob_read,
	.oob_poll	= observable_oob_poll,
	.write		= observable_write,
	.read		= observable_read,
	.poll		= observable_poll,
	.unlocked_ioctl	= observable_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= compat_ptr_ioctl,
#endif
};

struct evl_observable *evl_alloc_observable(int clone_flags)
{
	struct evl_observable *observable;
	int ret;

	observable = kzalloc(sizeof(*observable), GFP_KERNEL);
	if (observable == NULL)
		return ERR_PTR(-ENOMEM);

	ret = evl_init_element(&observable->element,
			&evl_observable_factory,
			clone_flags | EVL_CLONE_OBSERVABLE);
	if (ret) {
		kfree(observable);
		return ERR_PTR(ret);
	}

	INIT_LIST_HEAD(&observable->observers);
	INIT_LIST_HEAD(&observable->flush_list);
	evl_init_wait(&observable->oob_wait, &evl_mono_clock, EVL_WAIT_PRIO);
	init_waitqueue_head(&observable->inband_wait);
	init_irq_work(&observable->wake_irqwork, inband_wake_irqwork);
	init_irq_work(&observable->flush_irqwork, inband_flush_irqwork);
	evl_init_poll_head(&observable->poll_head);
	evl_spin_lock_init(&observable->lock);
	evl_index_factory_element(&observable->element);

	return observable;
}

static struct evl_element *
observable_factory_build(struct evl_factory *fac, const char *name,
		void __user *u_attrs, int clone_flags, u32 *state_offp)
{
	struct evl_observable *observable;

	if (clone_flags & ~EVL_OBSERVABLE_CLONE_FLAGS)
		return ERR_PTR(-EINVAL);

	observable = evl_alloc_observable(clone_flags);
	if (IS_ERR(observable))
		return ERR_PTR(PTR_ERR(observable));

	return &observable->element;
}

static void observable_factory_dispose(struct evl_element *e)
{
	struct evl_observable *observable;

	observable = container_of(e, struct evl_observable, element);
	evl_destroy_wait(&observable->oob_wait);
	evl_unindex_factory_element(&observable->element);
	evl_destroy_element(&observable->element);
	kfree_rcu(observable, element.rcu);
}

struct evl_factory evl_observable_factory = {
	.name	=	EVL_OBSERVABLE_DEV,
	.fops	=	&observable_fops,
	.build =	observable_factory_build,
	.dispose =	observable_factory_dispose,
	/* Threads are observables in essence. */
	.nrdev	=	CONFIG_EVL_NR_OBSERVABLES + CONFIG_EVL_NR_THREADS,
	.flags	=	EVL_FACTORY_CLONE,
};
