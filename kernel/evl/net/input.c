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
#include <evl/thread.h>
#include <evl/lock.h>
#include <evl/list.h>
#include <evl/flag.h>
#include <evl/net.h>
#include <evl/net/device.h>
#include <evl/net/ipv4.h>

static void napi_poll_oob(struct evl_netdev_state *est) /* oob */
{
	struct napi_struct *napi, *tmp;
	LIST_HEAD(requeuing);
	unsigned long flags;

	/*
	 * We cannot conflict with the in-band stack on queuing via
	 * napi->poll_list by design, since we own the NAPI instances
	 * queued to est->rx_poll until we release them in this
	 * routine by a call to napi_schedule_unprep().
	 */
	raw_spin_lock_irqsave(&est->rx_lock, flags);

	/*
	 * We are about to drop the RX lock, clear this flag early to
	 * close a race. We might compete with __set_rx_filter(), so
	 * use atomic bitops.
	 */
	clear_bit(EVL_NETDEV_POLL_SCHED, &est->flags);

	list_for_each_entry_safe(napi, tmp, &est->rx_poll, poll_list) {
		int budget = napi->weight;
		list_del_init(&napi->poll_list);
		raw_spin_unlock_irqrestore(&est->rx_lock, flags);
		budget -= napi->poll(napi, budget);
		/*
		 * If the budget was not fully consumed (> 0), then we
		 * have no more work for this instance and we may
		 * release it, unless a scheduling request was missed,
		 * in which case napi_schedule_unprep() would take
		 * care of calling napi_schedule_oob() for
		 * it. Otherwise, we need to requeue the instance for
		 * another polling round.
		 */
		if (budget > 0)
			napi_schedule_unprep(napi);
		else
			list_add(&napi->poll_list, &requeuing);
		raw_spin_lock_irqsave(&est->rx_lock, flags);
	}

	if (!list_empty(&requeuing)) {
		list_splice(&requeuing, &est->rx_poll);
		set_bit(EVL_NETDEV_POLL_SCHED, &est->flags);
	}

	raw_spin_unlock_irqrestore(&est->rx_lock, flags);
}

/*
 * RX thread dealing with ingress traffic and garbage collection for
 * stale input fragments. Specifically, this thread handles:
 *
 * - the outstanding requests for polling the device for new packets
 * (napi_schedule_oob())
 *
 * - the polled ingress packets queued by netif_deliver_oob(), passing
 * them over to the proper protocol layer.
 *
 * - the garbage collection to flush the IP fragments which have not
 * been collected in time.
 *
 * Each net device is served by a dedicated RX thread.
 */
void evl_net_do_rx(void *arg)
{
	struct net_device *dev = arg;
	struct evl_netdev_state *est;
	struct sk_buff *skb, *next;
	LIST_HEAD(list);
	int ret;

	est = dev->oob_state.estate;

	while (!evl_kthread_should_stop()) {
		ret = evl_wait_flag(&est->rx_flag);
		if (ret)
			break;

		if (test_bit(EVL_NETDEV_POLL_SCHED, &est->flags))
			napi_poll_oob(est);

		if (evl_net_move_skb_queue(&est->rx_packets, &list)) {
			list_for_each_entry_safe(skb, next, &list, list) {
				list_del(&skb->list);
				EVL_NET_CB(skb)->handler->ingress(skb);
			}
		}

		evl_net_ipv4_gc(dev_net(dev));
	}
}

void evl_net_wake_rx(struct net_device *dev)
{
	struct evl_netdev_state *est = dev->oob_state.estate;

	evl_raise_flag(&est->rx_flag);
}

/**
 * evl_net_receive - schedule an ingress packet for oob handling
 *
 * Schedule an incoming packet for delivery to a listening EVL socket
 * This call is either invoked:
 *
 * - in-band by a protocol-specific out-of-band packet filter
 *   (e.g. evl_net_ether_accept(), evl_net_ether_accept_vlan())
 *   diverting packets from the regular networking stack, in order to
 *   queue work for its .ingress() handler.
 *
 * - out-of-band on behalf of a fully oob capable NIC driver,
 *   typically from an out-of-band (RX) IRQ context.
 *
 * @skb the packet to queue. May be linked to some upstream
 * queue. skb->dev must be valid.
 *
 * @handler the network protocol descriptor which should eventually
 * handle the packet.
 */
void evl_net_receive(struct sk_buff *skb,
		struct evl_net_handler *handler) /* in-band or oob */
{
	struct evl_netdev_state *est = skb->dev->oob_state.estate;

	if (skb->next)
		skb_list_del_init(skb);

	EVL_NET_CB(skb)->handler = handler;

	/*
	 * Enqueue then kick our kthread handling the ingress path
	 * immediately if called from oob context. Otherwise, wait for
	 * the NIC driver to invoke napi_complete_done() when the RX
	 * side goes quiescent.
	 */
	evl_net_add_skb_queue(&est->rx_packets, skb);

	if (running_oob())
		evl_net_wake_rx(skb->dev);
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
 * napi_schedule_oob - plan for polling a NAPI instance.
 *
 * The RX kthread is resumed so that it polls the associated device
 * for ingress packets directly from the oob stage.
 *
 * @n is the NAPI instance associated to a device for which oob packet
 * diversion is enabled. An earlier call to napi_schedule_prep() is
 * expected to have been issued for @n.
 */
void napi_schedule_oob(struct napi_struct *n) /* oob */
{
	struct net_device *dev = n->dev;
	struct evl_netdev_state *est = dev->oob_state.estate;
	unsigned long flags;

	if (EVL_WARN_ON(NET, !(n->state & NAPIF_STATE_SCHED)))
		return;

	/*
	 * We might have multiple NAPI instances per device, so
	 * serialization is required despite a single NAPI instance
	 * may be active at any point in time. Oh, well. See
	 * napi_poll_oob() for an explanation about the requirement
	 * for atomic bitops (EVL_NETDEV_POLL_SCHED).
	 */
	raw_spin_lock_irqsave(&est->rx_lock, flags);
	list_add(&n->poll_list, &est->rx_poll);
	set_bit(EVL_NETDEV_POLL_SCHED, &est->flags);
	raw_spin_unlock_irqrestore(&est->rx_lock, flags);
	evl_net_wake_rx(dev);
}

/**
 * napi_complete_oob - release a NAPI instance.
 *
 * May be called in-band or out-of-band indifferently. Eventually, the
 * RX kthread is resumed so that it passes the pending ingress packets
 * to the proper protocol handlers.
 *
 * @n is the NAPI instance associated to a device for which oob packet
 * diversion is enabled.
 */
void napi_complete_oob(struct napi_struct *n) /* inband / oob */
{
	evl_net_wake_rx(n->dev);
}

/**
 * netif_deliver_oob - receive a network packet from the hardware.
 *
 * Decide whether we should channel a freshly incoming packet to our
 * out-of-band stack. May be called from any stage.
 *
 * Delivery is trivially simple at the moment: the set of protocols we
 * handle is statically defined, currently ETH_P_IP. The point is to
 * provide an expedited data path via the oob stage for the protocols
 * which most users need, without reinventing the whole networking
 * infrastructure.
 *
 * @skb the packet to inspect for oob delivery. May be linked to some
 * upstream queue.
 *
 * Returns true if the oob stack wants to handle @skb, in which case
 * the caller must assume that it does not own the packet anymore.
 */
bool netif_deliver_oob(struct sk_buff *skb) /* oob or in-band */
{
	skb_reset_network_header(skb);
	if (!skb_transport_header_was_set(skb))
		skb_reset_transport_header(skb);
	skb_reset_mac_len(skb);

	/*
	 * Filter the incoming packet through the eBPF RX program
	 * attached to the input device (if any), passing it down to
	 * the regular in-band stack if the filter code says that we
	 * are not interested in it.
	 */
	switch (evl_net_filter_rx(skb->dev, skb)) {
	case EVL_RX_VLAN:
		/*
		 * Apply our VLAN rules to decide whether this is an
		 * oob packet.
		 */
		break;
	case EVL_RX_ACCEPT:
		/* Direct the packet to the oob stack unconditionally. */
		switch (skb->protocol) {
		case htons(ETH_P_IP):
			return evl_net_ether_accept(skb);
		default:
			/*
			 * We don't deal with non-IP protocols, and
			 * the filter mistakenly told us to handle the
			 * packet. Leave it to inband.
			 */
			return false;
		}
	case EVL_RX_SKIP:
		/* Leave the packet to inband. */
		return false;
	case EVL_RX_DROP:
		/* Blackhole. */
		evl_net_free_skb(skb);
		return true;
	}

	/*
	 * Fallback to VLAN-based filtering to figure out whether the
	 * packet should be handled by the oob stack.
	 */
	switch (skb->protocol) {
	case htons(ETH_P_IP):
		return evl_net_ether_accept_vlan(skb);
	default:
		/*
		 * For those adapters without hw-accelerated VLAN
		 * capabilities, check the ethertype directly.
		 */
		if (eth_type_vlan(skb->protocol))
			return evl_net_ether_accept_vlan(skb);

		return false;
	}
}
