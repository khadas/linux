/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <linux/netdevice.h>
#include <linux/irq_work.h>
#include <linux/if_vlan.h>
#include <linux/skbuff.h>
#include <evl/net.h>
#include <evl/thread.h>
#include <evl/lock.h>
#include <evl/list.h>
#include <evl/flag.h>

void evl_net_do_rx(void *arg)
{
	struct net_device *dev = arg;
	struct evl_netdev_state *est;
	struct sk_buff *skb, *next;
	LIST_HEAD(list);
	int ret;

	est = dev->oob_context.dev_state.estate;

	while (!evl_kthread_should_stop()) {
		ret = evl_wait_flag(&est->rx_flag);
		if (ret)
			break;

		if (!evl_net_move_skb_queue(&est->rx_queue, &list))
			continue;

		list_for_each_entry_safe(skb, next, &list, list) {
			list_del(&skb->list);
			EVL_NET_CB(skb)->handler->ingress(skb);
		}

		/* Do NOT wait for evl_wait_flag() for this. */
		evl_schedule();
	}
}

/**
 *	evl_net_receive - schedule an ingress packet for oob handling
 *
 *	Add an incoming packet to the out-of-band receive queue, so
 *	that it will be delivered to a listening EVL socket (or
 *	dropped). This call is either invoked:
 *
 *	- in-band by a protocol-specific stage dispatcher
 *	(e.g. evl_net_ether_accept()) diverting packets from the
 *	regular networking stack, in order to queue work for its
 *	.ingress() handler.
 *
 *	- out-of-band on behalf of a fully oob capable NIC driver,
 *	typically from an out-of-band (RX) IRQ context.
 *
 *	@skb the packet to queue. May be linked to some upstream
 *	queue. skb->dev must be valid.
 *
 *	@handler the network protocol descriptor which should eventually
 *	handle the packet.
 */
void evl_net_receive(struct sk_buff *skb,
		struct evl_net_handler *handler) /* in-band or oob */
{
	struct evl_netdev_state *est = skb->dev->oob_context.dev_state.estate;

	if (skb->next)
		skb_list_del_init(skb);

	EVL_NET_CB(skb)->handler = handler;

	/*
	 * Enqueue then kick our kthread handling the ingress path
	 * immediately if called from oob context. Otherwise, wait for
	 * the NIC driver to invoke napi_complete_done() when the RX
	 * side goes quiescent.
	 */
	evl_net_add_skb_queue(&est->rx_queue, skb);

	if (running_oob())
		evl_raise_flag(&est->rx_flag);
}

struct evl_net_rxqueue *evl_net_alloc_rxqueue(u32 hkey) /* in-band */
{
	struct evl_net_rxqueue *rxq;

	rxq = kzalloc(sizeof(*rxq), GFP_KERNEL);
	if (rxq == NULL)
		return NULL;

	rxq->hkey = hkey;
	INIT_LIST_HEAD(&rxq->subscribers);
	evl_spin_lock_init(&rxq->lock);

	return rxq;
}

/* in-band */
void evl_net_free_rxqueue(struct evl_net_rxqueue *rxq)
{
	EVL_WARN_ON(NET, !list_empty(&rxq->subscribers));

	kfree(rxq);
}

/**
 *	netif_oob_deliver - receive a network packet
 *
 *	Decide whether we should channel a freshly incoming packet to
 *	our out-of-band stack. May be called from any stage.
 *
 *	@skb the packet to inspect for oob delivery. May be linked to
 *	some upstream queue.
 *
 *	Returns true if the oob stack wants to handle @skb, in which
 *	case the caller must assume that it does not own the packet
 *	anymore.
 */
bool netif_oob_deliver(struct sk_buff *skb) /* oob or in-band */
{
	/*
	 * Trivially simple at the moment: the set of protocols we
	 * handle is statically defined. The point is to provide an
	 * expedited data path via the oob stage for the protocols
	 * which most users need, without reinventing the whole
	 * networking infrastructure. XDP would not help here for
	 * several reasons.
	 */
	switch (skb->protocol) {
	case htons(ETH_P_IP):	/* Try IP over ethernet. */
		return evl_net_ether_accept(skb);
	default:
		/*
		 * For those adapters without hw-accelerated VLAN
		 * capabilities, check the ethertype directly.
		 */
		if (eth_type_vlan(skb->protocol))
			return evl_net_ether_accept(skb);

		return false;
	}
}

/**
 *	netif_oob_run - run the oob packet delivery
 *
 *	This call is invoked from napi_complete_done() in order to
 *	kick our RX kthread, which will forward the oob packets to the
 *	proper protocol handlers. This is an in-band call.
 *
 *	@dev the device receiving packets. netif_oob_diversion(dev) is
 *	     true.
 */
void netif_oob_run(struct net_device *dev) /* in-band */
{
	struct evl_netdev_state *est = dev->oob_context.dev_state.estate;

	evl_raise_flag(&est->rx_flag);
}
