/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <linux/netdevice.h>
#include <linux/dma-mapping.h>
#include <linux/if_vlan.h>
#include <linux/err.h>
#include <evl/net.h>
#include <evl/lock.h>
#include <evl/list.h>
#include <evl/work.h>
#include <evl/wait.h>

static unsigned int net_clones = 1024; /* FIXME: Kconfig? */

static unsigned int net_clones_arg;
module_param_named(net_clones, net_clones_arg, uint, 0444);

/*
 * skb lifecycle:
 *
 * [RX path]: netif_oob_deliver(skb)
 *             * false: pass down, freed through in-band stack
 *             * true: process -> receive -> evl_net_free_skb(skb)
 *                     skb->oob ? added to skb->dev->free_skb_pool or clone_heads
 *                              : pushed to in-band recycling queue
 *
 * [TX path]: skb = evl_net_dev_alloc_skb() | netdev_alloc_skb*()
 *           ...
 *           netdev_start_xmit(skb)
 *           ...
 *           [IRQ or NAPI context]
 *              napi_consume_skb(skb)
 *                    -> recycle_oob_skb(skb)
 *            |
 * [1]          consume_skb(skb)
 * [2]                 -> __kfree_skb(skb)
 *                           -> recycle_oob_skb(skb)
 *            |
 *              __dev_kfree_skb_any(skb)
 *                        -> __dev_kfree_skb_irq(skb)
 *                             [SOFTIRQ NET_TX]
 *                                -> net_tx_action
 *                        |              -> __kfree_skb(skb) [2]
 *                        |              |
 *                        |              -> __kfree_skb_defer(skb)
 *                                                -> recycle_oob_skb(skb)
 *                        -> dev_kfree_skb(skb)
 *                                -> consume_skb(skb) [1]
 */

#define SKB_RECYCLING_THRESHOLD	64

static LIST_HEAD(recycling_queue);

static int recycling_count;

/*
 * CAUTION: Innermost lock, may be nested with
 * oob_netdev_state.pool_wait.lock.
 */
static DEFINE_HARD_SPINLOCK(recycling_lock);

static struct evl_work recycler_work;

static DEFINE_HARD_SPINLOCK(clone_lock);

static LIST_HEAD(clone_heads);

static unsigned int clone_count;

/**
 *	skb_oob_recycle - release a network packet to the out-of-band
 *	stack.
 *
 *	Called by the in-band stack in order to release a socket
 *	buffer (e.g. __kfree_skb[_defer]()). This routine might decide
 *	to leave it to the in-band caller for releasing the packet
 *	eventually, in which case it would return false.
 *
 *	@skb the packet to release.
 *
 *	Returns true if the oob stack consumed the packet.
 */
bool skb_oob_recycle(struct sk_buff *skb) /* in-band */
{
	if (EVL_WARN_ON(NET, !skb->oob || skb->dev == NULL))
		return false;

	evl_net_free_skb(skb);

	return true;
}

static void skb_recycler(struct evl_work *work)
{
	struct sk_buff *skb, *next;
	unsigned long flags;
	LIST_HEAD(list);

	raw_spin_lock_irqsave(&recycling_lock, flags);
	list_splice_init(&recycling_queue, &list);
	recycling_count = 0;
	raw_spin_unlock_irqrestore(&recycling_lock, flags);

	/*
	 * The recycler runs on a workqueue context with (in-band)
	 * irqs on, so calling dev_kfree_skb() is fine.
	 */
	list_for_each_entry_safe(skb, next, &list, list) {
		skb_list_del_init(skb);
		dev_kfree_skb(skb);
	}
}

struct sk_buff *evl_net_dev_alloc_skb(struct net_device *dev,
				      ktime_t timeout, enum evl_tmode tmode)
{
	struct evl_netdev_state *est = dev->oob_context.dev_state.estate;
	struct sk_buff *skb;
	unsigned long flags;
	int ret;

	if (EVL_WARN_ON(NET, is_vlan_dev(dev)))
		return NULL;

	for (;;) {
		raw_spin_lock_irqsave(&est->pool_wait.lock, flags);

		if (!list_empty(&est->free_skb_pool)) {
			skb = list_get_entry(&est->free_skb_pool,
					struct sk_buff, list);
			est->pool_free--;
			break;
		}

		if (timeout == EVL_NONBLOCK) {
			skb = ERR_PTR(-EWOULDBLOCK);
			break;
		}

		evl_add_wait_queue(&est->pool_wait, timeout, tmode);

		raw_spin_unlock_irqrestore(&est->pool_wait.lock, flags);

		ret = evl_wait_schedule(&est->pool_wait);
		if (ret)
			return ERR_PTR(ret);
	}

	raw_spin_unlock_irqrestore(&est->pool_wait.lock, flags);

	return skb;
}

static inline void maybe_kick_recycler(void)
{
	if (READ_ONCE(recycling_count) >= SKB_RECYCLING_THRESHOLD)
		evl_call_inband(&recycler_work);
}

static void free_skb_inband(struct sk_buff *skb)
{
	raw_spin_lock(&recycling_lock);
	list_add(&skb->list, &recycling_queue);
	recycling_count++;
	raw_spin_unlock(&recycling_lock);
}

static void free_skb_to_dev(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct evl_netdev_state *est;
	unsigned long flags;

	est = dev->oob_context.dev_state.estate;
	raw_spin_lock_irqsave(&est->pool_wait.lock, flags);

	list_add(&skb->list, &est->free_skb_pool);
	est->pool_free++;

	if (evl_wait_active(&est->pool_wait))
		evl_wake_up_head(&est->pool_wait);

	raw_spin_unlock_irqrestore(&est->pool_wait.lock, flags);

	evl_signal_poll_events(&est->poll_head,	POLLOUT|POLLWRNORM);
}

static void free_skb_clone(struct sk_buff *skb)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&clone_lock, flags);
	list_add(&skb->list, &clone_heads);
	clone_count++;
	raw_spin_unlock_irqrestore(&clone_lock, flags);
}

static void free_skb(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct sk_buff *origin;
	int dref;

	if (EVL_WARN_ON(NET, dev == NULL))
		return;

	if (EVL_WARN_ON(NET, is_vlan_dev(dev)))
		return;

	/*
	 * We might receive requests to free regular skbs, or
	 * associated to devices for which diversion was just turned
	 * off: pass them on to the in-band stack if so.
	 * FIXME: should we receive that?
	 */
	if (unlikely(!netif_oob_diversion(skb->dev)))
		skb->oob = false;

	if (!skb_is_oob(skb)) {
		if (!skb_has_oob_clone(skb))
			free_skb_inband(skb);
		return;
	}

	if (!skb_release_oob_skb(skb, &dref))
		return;

	if (skb_is_oob_clone(skb)) {
		origin = EVL_NET_CB(skb)->origin;
		free_skb_clone(skb);
		if (!skb_is_oob(origin)) {
			if (dref == 1)
				free_skb_inband(origin);
			else
				EVL_WARN_ON(NET, dref < 1);
			return;
		}
		skb = origin;
	}

	if (!dref) {
		netdev_reset_oob_skb(dev, skb);
		free_skb_to_dev(skb);
	}
}

/**
 *	evl_net_clone_skb - clone a socket buffer.
 *
 *	Allocate and build a clone of @skb, referring to the same
 *	data. Unlike its in-band counterpart, a cloned skb lives
 *	(i.e. shell + data) until all its clones are freed.
 *
 *	@skb the packet to clone.
 */
struct sk_buff *evl_net_clone_skb(struct sk_buff *skb)
{
	struct sk_buff *clone = NULL;
	unsigned long flags;

	raw_spin_lock_irqsave(&clone_lock, flags);
	if (!list_empty(&clone_heads)) {
		clone = list_get_entry(&clone_heads, struct sk_buff, list);
		clone_count--;
	}
	raw_spin_unlock_irqrestore(&clone_lock, flags);

	if (unlikely(!clone))
		return NULL;

	skb_morph_oob_skb(clone, skb);
	/*
	 * Propagate the origin, this is a N(clones):1(origin)
	 * relationship.
	 */
	EVL_NET_CB(clone)->origin = EVL_NET_CB(skb)->origin ?: skb;

	return clone;
}

/**
 *	evl_net_free_skb - releases a socket buffer.
 *
 *	Packets which were conveying out-of-band data are moved back
 *	to the originating per-device pool (if that device is still
 *	active). Otherwise, the packet is scheduled for release to the
 *	in-band pool.
 *
 *	@skb the packet to release. Not linked to any upstream queue.
 */
void evl_net_free_skb(struct sk_buff *skb)
{
	EVL_WARN_ON(NET, hard_irqs_disabled());

	free_skb(skb);
	evl_schedule();
	maybe_kick_recycler();
}

/**
 *	evl_net_free_skb_list - releases a list of socket buffers.
 *
 *	Releases a list of buffers linked to a private list. Buffers
 *	may belong to different devices.

 *	@list the list head queuing packets to release.
 */
void evl_net_free_skb_list(struct list_head *list)
{
	struct sk_buff *skb, *n;

	EVL_WARN_ON(NET, hard_irqs_disabled());

	if (list_empty(list))
		return;

	list_for_each_entry_safe(skb, n, list, list)
		free_skb(skb);

	evl_schedule();
	maybe_kick_recycler();
}

void evl_net_init_skb_queue(struct evl_net_skb_queue *skbq)
{
	INIT_LIST_HEAD(&skbq->queue);
	raw_spin_lock_init(&skbq->lock);
}

void evl_net_destroy_skb_queue(struct evl_net_skb_queue *skbq)
{
	evl_net_free_skb_list(&skbq->queue);
}

void evl_net_add_skb_queue(struct evl_net_skb_queue *skbq,
			struct sk_buff *skb)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&skbq->lock, flags);
	list_add_tail(&skb->list, &skbq->queue);
	raw_spin_unlock_irqrestore(&skbq->lock, flags);
}

struct sk_buff *evl_net_get_skb_queue(struct evl_net_skb_queue *skbq)
{
	struct sk_buff *skb = NULL;
	unsigned long flags;

	raw_spin_lock_irqsave(&skbq->lock, flags);

	if (!list_empty(&skbq->queue))
		skb = list_get_entry(&skbq->queue, struct sk_buff, list);

	raw_spin_unlock_irqrestore(&skbq->lock, flags);

	return skb;
}

bool evl_net_move_skb_queue(struct evl_net_skb_queue *skbq,
			struct list_head *list)
{
	unsigned long flags;
	bool ret;

	raw_spin_lock_irqsave(&skbq->lock, flags);
	list_splice_init(&skbq->queue, list);
	ret = !list_empty(list);
	raw_spin_unlock_irqrestore(&skbq->lock, flags);

	return ret;
}

static struct sk_buff *alloc_one_skb(struct net_device *dev)
{
	struct evl_netdev_state *est;
	struct sk_buff *skb;
	dma_addr_t dma_addr;

	if (!netdev_is_oob_capable(dev)) {
		est = dev->oob_context.dev_state.estate;
		return __netdev_alloc_oob_skb(dev, est->buf_size, GFP_KERNEL);
	}

	skb = netdev_alloc_oob_skb(dev, &dma_addr);
	if (skb)
		EVL_NET_CB(skb)->dma_addr = dma_addr;

	return skb;
}

/* in-band */
int evl_net_dev_build_pool(struct net_device *dev)
{
	struct evl_netdev_state *est;
	struct sk_buff *skb;
	int n;

	if (EVL_WARN_ON(NET, is_vlan_dev(dev)))
		return -EINVAL;

	if (EVL_WARN_ON(NET, netif_oob_diversion(dev)))
		return -EBUSY;

	est = dev->oob_context.dev_state.estate;

	INIT_LIST_HEAD(&est->free_skb_pool);

	for (n = 0; n < est->pool_max; n++) {
		skb = alloc_one_skb(dev);
		if (skb == NULL) {
			evl_net_dev_purge_pool(dev);
			return -ENOMEM;
		}
		list_add(&skb->list, &est->free_skb_pool);
	}

	est->pool_free = est->pool_max;
	evl_init_wait(&est->pool_wait, &evl_mono_clock, EVL_WAIT_PRIO);
	evl_init_poll_head(&est->poll_head);

	return 0;
}

/* in-band, only when diversion is disabled! */
void evl_net_dev_purge_pool(struct net_device *dev)
{
	struct evl_netdev_state *est;
	struct sk_buff *skb, *next;

	if (EVL_WARN_ON(NET, netif_oob_diversion(dev)))
		return;

	est = dev->oob_context.dev_state.estate;

	list_for_each_entry_safe(skb, next, &est->free_skb_pool, list) {
		if (netdev_is_oob_capable(dev))
			netdev_free_oob_skb(dev, skb,
					EVL_NET_CB(skb)->dma_addr);
		else
			__netdev_free_oob_skb(dev, skb);
	}

	evl_destroy_wait(&est->pool_wait);
}

ssize_t evl_net_show_clones(char *buf, size_t len)
{
	return scnprintf(buf, len, "%u\n", clone_count);
}

int __init evl_net_init_pools(void)
{
	struct sk_buff *clone, *tmp;
	unsigned int n;

	clone_count = net_clones;

	for (n = 0; n < clone_count; n++) {
		clone = skb_alloc_oob_head(GFP_KERNEL);
		if (clone == NULL)
			goto fail;
		list_add(&clone->list, &clone_heads);
	}

	evl_init_work(&recycler_work, skb_recycler);

	return 0;

fail:
	list_for_each_entry_safe(clone, tmp, &clone_heads, list) {
		list_del(&clone->list);
		kfree_skb(clone);
	}

	clone_count = 0;

	return -ENOMEM;
}
