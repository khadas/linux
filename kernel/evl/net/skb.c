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
#include <linux/log2.h>
#include <linux/err.h>
#include <linux/of_platform.h>
#include <linux/skbuff_ref.h>
#include <net/page_pool/helpers.h>
#include <evl/uio.h>
#include <evl/net.h>
#include <evl/lock.h>
#include <evl/list.h>
#include <evl/work.h>
#include <evl/wait.h>
#include <evl/net/device.h>
#include <evl/net/socket.h>

/*
 * skb lifecycle:
 *
 * [RX path]: netif_deliver_oob(skb)
 *             * false: pass down, freed through in-band stack
 *             * true: process -> receive -> evl_net_free_skb(skb)
 *                     skb_has_oob_storage(skb) ? immediately released to oob pool
 *                              : pushed to in-band recycling queue
 *
 * [TX path]: skb = evl_net_dev_alloc_skb()
 *           ...
 *           netdev_start_xmit(skb)
 *           ...
 *           [IRQ or NAPI context]
 *              napi_consume_skb(skb)
 *                    -> __napi_kfree_skb(skb) [3]
 *            |
 * [1]          consume_skb(skb)
 * [2]                 -> __kfree_skb(skb)
 *                           -> free_skb_oob(skb)
 *            |
 *              __dev_kfree_skb_any(skb)
 *                        -> __dev_kfree_skb_irq_reason(skb)
 *                             [SOFTIRQ NET_TX]
 *                                -> net_tx_action
 *                        |              -> __kfree_skb(skb) [2]
 *                        |              |
 * [3]                    |              -> __napi_kfree_skb(skb)
 *                                                -> free_skb_oob(skb)
 *                        -> dev_kfree_skb(skb)
 *                                -> consume_skb(skb) [1]
 */

#define SKB_RECYCLING_THRESHOLD 32

static LIST_HEAD(recycling_queue);

static int recycling_count;

static DEFINE_HARD_SPINLOCK(recycling_lock);

static void skb_recycler(struct evl_work *work)
{
	struct sk_buff *skb, *next;
	unsigned long flags;
	LIST_HEAD(list);

	raw_spin_lock_irqsave(&recycling_lock, flags);
	list_splice_init(&recycling_queue, &list);
	recycling_count = 0;
	raw_spin_unlock_irqrestore(&recycling_lock, flags);

	local_bh_disable();

	list_for_each_entry_safe(skb, next, &list, list) {
		skb_list_del_init(skb);
		finalize_skb_inband(skb);
	}

	local_bh_enable();
}

static EVL_DEFINE_WORK(recycler_work, skb_recycler);

static inline void maybe_kick_recycler(void)
{
	if (READ_ONCE(recycling_count) >= SKB_RECYCLING_THRESHOLD)
		evl_call_inband(&recycler_work);
}

static struct page *alloc_bufpage(struct net_device *dev,
				ktime_t timeout, enum evl_tmode tmode)
{
	struct evl_netdev_state *est = dev->oob_state.estate;
	unsigned long flags;
	struct page *page;
	int ret;

	for (;;) {
		raw_spin_lock_irqsave(&est->tx_wait.wchan.lock, flags);

		page = page_pool_dev_alloc_pages(est->tx_pages);
		if (likely(page))
			break;

		if (timeout == EVL_NONBLOCK) {
			page = ERR_PTR(-EWOULDBLOCK);
			break;
		}

		evl_add_wait_queue(&est->tx_wait, timeout, tmode);

		raw_spin_unlock_irqrestore(&est->tx_wait.wchan.lock, flags);

		ret = evl_wait_schedule(&est->tx_wait);
		if (ret)
			return ERR_PTR(ret);
	}

	raw_spin_unlock_irqrestore(&est->tx_wait.wchan.lock, flags);

	return page;
}

struct sk_buff *evl_net_dev_alloc_skb(struct net_device *dev,
				      ktime_t timeout, enum evl_tmode tmode)
{
	struct evl_netdev_state *est;
	struct net_device *real_dev;
	struct sk_buff *skb;
	struct page *page;

	/*
	 * Statically check the sanity of our basic assumptions:
	 *
	 * - the size of our control block fits into the space
	 * reserved for this purpose in the generic socket buffer.
	 *
	 * - the list head we use for queuing buffers does not overlap
	 * the device pointer in the unionized layout (this is
	 * definitely ugly, for sure).
	 */
	BUILD_BUG_ON(sizeof(struct evl_net_cb) > sizeof(skb->cb));
	BUILD_BUG_ON(sizeof(skb->list) > sizeof(struct sk_buff_list));

	/*
	 * Build a free skb (for TX) from a page pulled from a
	 * per-device pool, enforcing congestion control according to
	 * the specified timeout rule.
	 */
	real_dev = evl_net_real_dev(dev);
	page = alloc_bufpage(real_dev, timeout, tmode);
	if (IS_ERR(page))
		return ERR_PTR(PTR_ERR(page));

	est = real_dev->oob_state.estate;
	skb = build_skb(page_address(page), est->buf_size);
	if (!skb) {
		maybe_kick_recycler(); /* Hope for the best. */
		return ERR_PTR(-ENOMEM);
	}

	skb_mark_oob_storage(skb);
	skb_mark_for_recycle(skb);
	/*
	 * The current assumption is that we are going to deal with
	 * ethernet devices, for which we may need some extra header
	 * space for adding the 802.1q encapsulation. Reserve enough
	 * headroom, so that we won't have to reallocate for such
	 * purpose.
	 */
	skb_reserve(skb, VLAN_HLEN);
	skb->dev = real_dev;

	return skb;
}

/*
 * Plan for a skb to be released by the in-band stack.
 *
 * CAUTION: the caller must call evl_schedule() and call the in-band
 * recycler.
 */
static void free_inband_skb(struct sk_buff *skb)
{
	unsigned long flags;

	if (running_inband()) {
		finalize_skb_inband(skb);
	} else {
		raw_spin_lock_irqsave(&recycling_lock, flags);
		list_add(&skb->list, &recycling_queue);
		recycling_count++;
		raw_spin_unlock_irqrestore(&recycling_lock, flags);
	}
}

static void __free_evl_skb(struct sk_buff *skb)
{
	struct skb_shared_info *shinfo = skb_shinfo(skb);
	struct net_device *dev = skb->dev;
	struct evl_netdev_state *est = dev->oob_state.estate;
	unsigned long flags;

	/* If the data storage is still shared, don't release it. */
	if (skb->cloned &&
	    atomic_sub_return(skb->nohdr ? (1 << SKB_DATAREF_SHIFT) + 1 : 1,
			      &shinfo->dataref))
		goto release_head;

	/*
	 * Release the data. This is the gist of skb_pp_recycle(),
	 * since we already know for sure that an oob-managed skb is
	 * built around a page from a per-device pool (in
	 * evl_netdev_state).
	 */
	napi_pp_put_page(page_to_netmem(virt_to_page(skb->head)));

	/*
	 * Wake up any thread waiting for buffer space to send to the
	 * device we are releasing the page to.
	 */
	raw_spin_lock_irqsave(&est->tx_wait.wchan.lock, flags);

	if (evl_wait_active(&est->tx_wait))
		evl_wake_up_head(&est->tx_wait);

	raw_spin_unlock_irqrestore(&est->tx_wait.wchan.lock, flags);

	evl_signal_poll_events(&est->poll_head,	POLLOUT|POLLWRNORM);

release_head:
	EVL_WARN_ON(NET, atomic_read(&shinfo->dataref) < 0);
	/* Now release the buffer head. */
	put_oob_skb(skb);
}

/*
 * Free an skb we originally allocated from our pool. The caller has
 * exclusive ownership on this (i.e. no other reference is pending).
 */
static void free_evl_skb(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct sk_buff *fskb, *nskb;

	if (EVL_WARN_ON(NET, dev == NULL))
		return;

	/*
	 * All skbs on a given fragment list are guaranteed to belong
	 * to the same device.
	 */
	for (fskb = skb_shinfo(skb)->frag_list; fskb; fskb = nskb) {
		netdev_dbg(dev, "releasing frag %px from %px\n", fskb, skb);
		nskb = fskb->next;
		__free_evl_skb(fskb);
	}

	netdev_dbg(dev, "releasing skb %px (has_frags=%d)\n",
		skb, skb_has_frag_list(skb));

	__free_evl_skb(skb);
}

static void __free_skb(struct sk_buff *skb)
{
	/*
	 * If the skb data does not live in an oob pool, hand over the
	 * release to the in-band stack. Otherwise we may immediately
	 * attempt to free the data if no other skb refers to it, and
	 * the buffer head too.
	 */
	if (!skb_has_oob_storage(skb))
		free_inband_skb(skb);
	else
		free_evl_skb(skb);
}

static void free_skb(struct sk_buff *skb)
{
	if (skb_unref(skb))
		__free_skb(skb);
}

/**
 *	evl_net_free_skb - releases a socket buffer.
 *
 *	Packets which were conveying out-of-band data are moved back
 *	to the originating per-device pool (if that device is still
 *	active). Otherwise, the packet is scheduled for release to the
 *	in-band pool.
 *
 *	@skb the packet to release. Not linked to any upstream
 *	queue. The routine also accepts regular in-band buffers.
 */
void evl_net_free_skb(struct sk_buff *skb) /* in-band/oob */
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

/**
 *	free_skb_oob - attempt to free a buffer head along with its
 *	data storage.
 *
 *      Called from the in-band net core right after the last
 *      reference to the buffer was dropped. We get a chance to
 *      release the buffer immediately to our bufheads pool. However,
 *      if the buffer data was allocated in-band, send the skb back to
 *      the in-band core for disposal from there.
 */
void free_skb_oob(struct sk_buff *skb) /* inband/oob */
{
	EVL_WARN_ON(NET, hard_irqs_disabled());

	/*
	 * The in-band stack should give us only fully released
	 * buffers via this hook. skb_unref() might have skipped
	 * decrementation down to zero if skb->users == 1 on entry
	 * (i.e. exclusive ownership), account for this.
	 */
	if (EVL_WARN_ON(NET, refcount_read(&skb->users) > 1))
		return;

	__free_skb(skb);
	evl_schedule();
	maybe_kick_recycler();
}

/**
 *	evl_net_clone_skb - clone a socket buffer.
 *
 *	Allocate and build a clone of @skb, referring to the same
 *	data.
 *
 *	@skb the packet to clone.
 *
 *      CAUTION: fragments are not cloned.
 */
struct sk_buff *evl_net_clone_skb(struct sk_buff *skb)
{
	struct sk_buff *clone;

	clone = get_oob_skb();
	if (!clone)
		return NULL;

	clone->head = NULL;	/* So we can morph safely. */
	skb_morph(clone, skb);

	return clone;
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

bool evl_net_charge_skb_rmem(struct evl_socket *esk, struct sk_buff *skb)
{
	bool ret;

	EVL_NET_CB(skb)->tracker = NULL;
	ret = evl_charge_socket_rmem(esk, skb->truesize);
	if (likely(ret))
		EVL_NET_CB(skb)->tracker = esk;

	return ret;
}

void evl_net_uncharge_skb_rmem(struct sk_buff *skb)
{
	struct evl_socket *esk = EVL_NET_CB(skb)->tracker;

	if (esk) {
		EVL_NET_CB(skb)->tracker = NULL;
		evl_uncharge_socket_rmem(esk, skb->truesize);
	}
}

int evl_net_charge_skb_wmem(struct evl_socket *esk,
			struct sk_buff *skb,
			ktime_t timeout, enum evl_tmode tmode)
{
	int ret;

	EVL_NET_CB(skb)->tracker = NULL;
	ret = evl_charge_socket_wmem(esk, skb->truesize, timeout, tmode);
	if (likely(!ret))
		EVL_NET_CB(skb)->tracker = esk;

	return ret;
}

void evl_net_uncharge_skb_wmem(struct sk_buff *skb)
{
	struct evl_socket *esk = EVL_NET_CB(skb)->tracker;

	/*
	 * If set, the tracking socket cannot be stale as it has to
	 * pass the wmem_crossing first before unwinding in
	 * sock_oob_destroy().
	 */
	if (esk) {
		EVL_NET_CB(skb)->tracker = NULL;
		evl_uncharge_socket_wmem(esk, skb->truesize);
	}
}

/* in-band */
int evl_net_dev_build_pool(struct net_device *dev)
{
	struct page_pool_params pp_params;
	struct evl_netdev_state *est;

	if (EVL_WARN_ON(NET, is_vlan_dev(dev)))
		return -EINVAL;

	if (EVL_WARN_ON(NET, netif_oob_diversion(dev)))
		return -EBUSY;

	est = dev->oob_state.estate;

	/*
	 * Set up a page pool for TX from the EVL netstack through the
	 * device.
	 */
	est->buf_size = ALIGN(est->buf_size, PAGE_SIZE);
	pp_params = (struct page_pool_params){
		.order = ilog2(est->buf_size / PAGE_SIZE),
		.flags = PP_FLAG_PAGE_OOB,
		.pool_size = est->pool_max,
		.nid = dev_to_node(dev->dev.parent),
		.dev = dev->dev.parent,
		.dma_dir = DMA_NONE,
		.offset = 0,
		.max_len = est->buf_size,
	};

	/*
	 * If the device is oob-capable, the page pool must perform
	 * DMA pre-mapping so that the NIC driver only has to deal
	 * with cache synchronization on the out-of-band TX path.  We
	 * are piggybacked by the XDP/TX support which enables
	 * DMA_BIDIRECTIONAL (DMA_TO_DEVICE is not supported).
	 */
	if (netdev_is_oob_capable(dev)) {
		pp_params.flags |= PP_FLAG_DMA_MAP | PP_FLAG_DMA_SYNC_DEV;
		pp_params.dma_dir = DMA_BIDIRECTIONAL;
	}

	est->tx_pages = page_pool_create(&pp_params);
	if (IS_ERR(est->tx_pages))
		return PTR_ERR(est->tx_pages);

	evl_init_wait(&est->tx_wait, &evl_mono_clock, EVL_WAIT_PRIO);
	evl_init_poll_head(&est->poll_head);

	return 0;
}

/* in-band, only when diversion is disabled! */
void evl_net_dev_purge_pool(struct net_device *dev)
{
	struct evl_netdev_state *est;

	if (EVL_WARN_ON(NET, netif_oob_diversion(dev)))
		return;

	est = dev->oob_state.estate;
	evl_destroy_wait(&est->tx_wait);
	page_pool_destroy(est->tx_pages);
}

/*
 * evl_net_wget_skb - allocate a buffer with contention management for
 * output.
 *
 * If the allocation causes the per-socket write contention threshold
 * to be crossed, the caller may sleep according to the timeout
 * specification.
 */
struct sk_buff *evl_net_wget_skb(struct evl_socket *esk,
				struct net_device *dev, ktime_t timeout)
{
	enum evl_tmode tmode = timeout ? EVL_ABS : EVL_REL;
	struct sk_buff *skb;
	int ret;

	skb = evl_net_dev_alloc_skb(dev, timeout, tmode);
	if (IS_ERR(skb))
		return skb;

	ret = evl_net_charge_skb_wmem(esk, skb, timeout, tmode);

	return ret ? ERR_PTR(ret) : skb;
}

/*
 * evl_net_wput_skb - deallocate a buffer obtained from
 * evl_net_wget_skb() for output.
 *
 * Fragments are deallocated if present.
 */
void evl_net_wput_skb(struct sk_buff *skb)
{
	struct sk_buff *fskb;

	skb_walk_frags(skb, fskb) {
		evl_net_uncharge_skb_wmem(fskb);
	}
	evl_net_uncharge_skb_wmem(skb);
	evl_net_free_skb(skb);
}

/*
 * evl_net_rput_skb - deallocate a buffer obtained from the ingress
 * path.
 *
 * Fragments are deallocated if present.
 */
void evl_net_rput_skb(struct sk_buff *skb)
{
	struct sk_buff *fskb;

	skb_walk_frags(skb, fskb) {
		evl_net_uncharge_skb_rmem(fskb);
	}
	evl_net_uncharge_skb_rmem(skb);
	evl_net_free_skb(skb);
}

static ssize_t __skb_to_uio(const struct iovec *iov, size_t iovlen,
			size_t *vpos, size_t *bpos,
			const void *data, size_t len)
{
	size_t rem = len;
	ssize_t ret = 0;

	while (rem > 0) {
		size_t avail = iov[*vpos].iov_len - *bpos, copy = rem;
		if (avail == 0) {
			if (++(*vpos) >= iovlen)
				break;
			*bpos = 0;
			continue;
		}
		if (copy > avail)
			copy = avail;
		if (raw_copy_to_user(iov[*vpos].iov_base + *bpos, data + ret, copy))
			return -EFAULT;
		*bpos += copy;
		ret += copy;
		rem -= copy;
	}

	return ret;
}

static ssize_t skb_to_uio(const struct iovec *iov, size_t iovlen,
			size_t *vpos, size_t *bpos,
			const void *data, size_t len)
{
	size_t rem = len;
	ssize_t ret = 0;

	while (rem > 0) {
		ssize_t partial = __skb_to_uio(iov, iovlen, vpos, bpos,
					data + ret, rem);
		if (partial <= 0)
			return partial;
		ret += partial;
		rem -= partial;
	}

	return ret;
}

/*
 * evl_net_skb_to_uio - copy the content of a socket buffer to a user
 * I/O vector. @skb may contain fragments.
 *
 * Returns the count of bytes written to @iov, or -EFAULT on uaccess
 * error. @short_write is set on return if not enough space is
 * available from @iov for storing the entire content.
 */
ssize_t evl_net_skb_to_uio(const struct iovec *iov, size_t iovlen,
			struct sk_buff *skb,
			size_t skip,
			bool *short_write)
{
	size_t vpos = 0, bpos = 0;
	struct sk_buff *fskb;
	ssize_t ret, out;

	if (skip)
		skb_pull_inline(skb, skip);

	ret = skb_to_uio(iov, iovlen, &vpos, &bpos, skb->data, skb->len);
	if (ret < skb->len) {
		*short_write = true;
		return ret;
	}

	skb_walk_frags(skb, fskb) {
		if (skip)
			skb_pull_inline(fskb, skip);
		out = skb_to_uio(iov, iovlen, &vpos, &bpos, fskb->data, fskb->len);
		if (out < 0)
			return out;
		ret += out;
		if (out < fskb->len) {
			*short_write = true;
			break;
		}
	}

	*short_write = false;

	return ret;
}
