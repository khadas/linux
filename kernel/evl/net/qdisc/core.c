/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/netdevice.h>
#include <evl/net/qdisc.h>

static LIST_HEAD(all_net_qdisc);

static DEFINE_MUTEX(qdisc_list_lock);

void evl_net_register_qdisc(struct evl_net_qdisc_ops *ops)
{
	mutex_lock(&qdisc_list_lock);
	list_add(&ops->next, &all_net_qdisc);
	mutex_unlock(&qdisc_list_lock);
}
EXPORT_SYMBOL_GPL(evl_net_register_qdisc);

void evl_net_unregister_qdisc(struct evl_net_qdisc_ops *ops)
{
	mutex_lock(&qdisc_list_lock);
	list_del(&ops->next);
	mutex_unlock(&qdisc_list_lock);
}
EXPORT_SYMBOL_GPL(evl_net_unregister_qdisc);

struct evl_net_qdisc *evl_net_alloc_qdisc(struct evl_net_qdisc_ops *ops)
{
	struct evl_net_qdisc *qdisc;
	int ret;

	qdisc = kzalloc(sizeof(*qdisc) + ops->priv_size, GFP_KERNEL);
	if (qdisc == NULL)
		return ERR_PTR(-ENOMEM);

	qdisc->oob_ops = ops;
	evl_net_init_skb_queue(&qdisc->inband_q);

	ret = ops->init(qdisc);
	if (ret) {
		kfree(qdisc);
		return ERR_PTR(ret);
	}

	return qdisc;
}
EXPORT_SYMBOL_GPL(evl_net_alloc_qdisc);

void evl_net_free_qdisc(struct evl_net_qdisc *qdisc)
{
	evl_net_destroy_skb_queue(&qdisc->inband_q);
	qdisc->oob_ops->destroy(qdisc);
	kfree(qdisc);
}
EXPORT_SYMBOL_GPL(evl_net_free_qdisc);

/**
 *	evl_net_sched_packet - pass an outgoing buffer to the packet
 *	scheduler.
 *
 *	Any high priority packet originating from the EVL stack
 *	(i.e. skb->oob is true) is given to the out-of-band qdisc
 *	handler attached to the device for scheduling. Otherwise, the
 *	packet is linked to the qdisc's low priority in-band queue.
 *
 *	@skb the packet to schedule for transmission. Must not be
 *	linked to any upstream queue.
 *
 *	@dev the device to pass the packet to.
 */
int evl_net_sched_packet(struct net_device *dev, struct sk_buff *skb) /* oob or in-band */
{
	struct evl_net_qdisc *qdisc = dev->oob_context.dev_state.estate->qdisc;

	/*
	 * Low-priority traffic sent by the sched_oob Qdisc goes to
	 * the internal in-band queue.
	 */
	if (!skb->oob) {
		evl_net_add_skb_queue(&qdisc->inband_q, skb);
		return 0;
	}

	return qdisc->oob_ops->enqueue(qdisc, skb);
}

void __init evl_net_init_qdisc(void)
{
	evl_net_register_qdisc(&evl_net_qdisc_fifo);
}

void __init evl_net_cleanup_qdisc(void)
{
	evl_net_unregister_qdisc(&evl_net_qdisc_fifo);
}
