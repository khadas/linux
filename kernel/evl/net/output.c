/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/if_vlan.h>
#include <linux/interrupt.h>
#include <linux/irq_work.h>
#include <evl/list.h>
#include <evl/lock.h>
#include <evl/flag.h>
#include <evl/net/socket.h>
#include <evl/net/output.h>
#include <evl/net/qdisc.h>

static void xmit_inband(struct irq_work *work);

DEFINE_IRQ_WORK(oob_xmit_work, xmit_inband);

static DEFINE_PER_CPU(struct evl_net_skb_queue, oob_tx_relay);

static inline netdev_tx_t
oob_start_xmit(struct net_device *dev, struct sk_buff *skb)
{
	/*
	 * If we got there, @dev is deemed oob-capable
	 * (IFF_OOB_CAPABLE, see evl_net_transmit()). The driver should
	 * check the current execution stage for handling the
	 * out-of-band packet properly.
	 */
	return dev->netdev_ops->ndo_start_xmit(skb, dev);
}

static inline void do_tx(struct evl_net_qdisc *qdisc,
			struct net_device *dev, struct sk_buff *skb)
{
	evl_uncharge_socket_wmem(skb);

	switch (oob_start_xmit(dev, skb)) {
	case NETDEV_TX_OK:
		break;
	default: /* busy, or whatever */
		qdisc->packet_dropped++;
		/* FIXME: we need to do better wrt error handling. */
		evl_net_free_skb(skb);
		break;
	}
}

void evl_net_do_tx(void *arg)
{
	struct net_device *dev = arg;
	struct evl_netdev_state *est;
	struct evl_net_qdisc *qdisc;
	struct sk_buff *skb, *n;
	LIST_HEAD(list);
	int ret;

	est = dev->oob_context.dev_state.estate;

	while (!evl_kthread_should_stop()) {
		ret = evl_wait_flag(&est->tx_flag);
		if (ret)
			break;

		/*
		 * Reread queueing discipline descriptor to allow
		 * dynamic updates. FIXME: protect this against
		 * swap/deletion while pulling packets (stax?).
		 */
		qdisc = est->qdisc;

		/*
		 * First we transmit the traffic as prioritized by the
		 * out-of-band queueing discipline attached to our
		 * device.
		 */
		for (;;) {
			skb = qdisc->oob_ops->dequeue(qdisc);
			if (skb == NULL)
				break;
			do_tx(qdisc, dev, skb);
		}

		/*
		 * Lastly, we send out any pending traffic we received from the
		 * in-band sch_oob Qdisc.
		 */
		if (evl_net_move_skb_queue(&qdisc->inband_q, &list)) {
			list_for_each_entry_safe(skb, n, &list, list) {
				list_del_init(&skb->list);
				do_tx(qdisc, dev, skb);
			}
		}
	}
}

/* in-band hook, called upon NET_TX_SOFTIRQ. */
void skb_inband_xmit_backlog(void)
{
	struct sk_buff *skb, *n;
	LIST_HEAD(list);

	if (evl_net_move_skb_queue(this_cpu_ptr(&oob_tx_relay), &list)) {
		list_for_each_entry_safe(skb, n, &list, list) {
			evl_uncharge_socket_wmem(skb);
			dev_queue_xmit(skb);
		}
	}
}

static void xmit_inband(struct irq_work *work) /* in-band, stalled */
{
	/*
	 * skb_inband_xmit_backlog() should run soon, kicked by tx_action.
	 */
	__raise_softirq_irqoff(NET_TX_SOFTIRQ);
}

/* oob or in-band */
static int xmit_oob(struct net_device *dev, struct sk_buff *skb)
{
	struct evl_netdev_state *est = dev->oob_context.dev_state.estate;
	int ret;

	ret = evl_net_sched_packet(dev, skb);
	if (ret)
		return ret;

	evl_raise_flag(&est->tx_flag);

	return 0;
}

/**
 *	evl_net_transmit - queue an egress packet for out-of-band
 *	transmission to the device.
 *
 *	Add an outgoing packet to the out-of-band transmit queue, so
 *	that it will be handed over to the device referred to by
 *	@skb->dev. The packet is complete (e.g. the VLAN tag is set if
 *	@skb->dev is an ethernet device).
 *
 *	@skb the packet to queue. Must not be linked to any upstream
 *	queue.
 *
 *	Prerequisites:
 *	- skb->dev is a valid (real) device. The caller must prevent from
 *        the interface going down.
 *	- skb->sk == NULL.
 *      - skb->oob == true.
 */
int evl_net_transmit(struct sk_buff *skb) /* oob or in-band */
{
	struct evl_net_skb_queue *rl = this_cpu_ptr(&oob_tx_relay);
	struct net_device *dev = skb->dev;
	unsigned long flags;
	bool kick;

	if (EVL_WARN_ON(NET, !dev))
		return -EINVAL;

	if (EVL_WARN_ON(NET, is_vlan_dev(dev)))
		return -EINVAL;

	if (EVL_WARN_ON(NET, skb->sk))
		return -EINVAL;

	/*
	 * Only packets obtained from the per-device oob pool are
	 * allowed to flow through this interface.
	 */
	if (EVL_WARN_ON(NET, !skb->oob))
		return -EINVAL;

	if (netdev_is_oob_capable(dev))
		return xmit_oob(dev, skb);

	/*
	 * If running in-band, just push the skb for transmission
	 * immediately to the in-band stack. Otherwise relay it via
	 * xmit_inband().
	 */
	if (running_inband()) {
		evl_uncharge_socket_wmem(skb);
		dev_queue_xmit(skb);
		return 0;
	}

	/*
	 * Running oob but net device is not oob-capable, resort to
	 * relaying the traffic to the in-band stage for enqueuing.
	 * Dovetail does ensure that __raise_softirq_irqoff() is safe
	 * to call from the oob stage, but we want the softirq to be
	 * raised as soon as in-band resumes with interrupts enabled,
	 * so we go through the irq_work indirection first.
	 */
	raw_spin_lock_irqsave(&rl->lock, flags);
	kick = list_empty(&rl->queue);
	list_add_tail(&skb->list, &rl->queue);
	raw_spin_unlock_irqrestore(&rl->lock, flags);

	if (kick)	/* Rare false positives are ok. */
		irq_work_queue(&oob_xmit_work);

	return 0;
}

/**
 *	netif_xmit_oob - send a network packet in out-of-band mode
 *
 *	Queue the packet for transmission from the out-of-band stage
 *	as low priority traffic. This hook is called by the enqueuing
 *	handler of the "sch_oob" in-band Qdisc for any packet sent to
 * 	an oob-capable interface, so that both in-band and out-of-band
 *	traffic is injected from our out-of-band Qdisc.
 *
 *	@skb the packet to transmit. Not linked to any upstream
 *	queue. We may take for granted that @skb->dev is valid and
 *	oob-capable. There will be no further routing of @skb, we only
 *	queue the packet to the oob queuing discipline we have for the
 *	oob-capable device.
 *
 *	Returns NET_XMIT_SUCCESS if the packet was successfully queued
 *	for transmission, NET_XMIT_DROP otherwise.
 */
int netif_xmit_oob(struct sk_buff *skb) /* in-band */
{
	return xmit_oob(skb->dev, skb) ? NET_XMIT_DROP : NET_XMIT_SUCCESS;
}

void __init evl_net_init_tx(void)
{
	struct evl_net_skb_queue *txq;
	int cpu;

	for_each_online_cpu(cpu) {
		txq = &per_cpu(oob_tx_relay, cpu);
		evl_net_init_skb_queue(txq);
	}
}
