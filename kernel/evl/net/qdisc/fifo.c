/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <evl/list.h>
#include <evl/net/qdisc.h>

struct qdisc_fifo_priv {
	struct evl_net_skb_queue q;
};

static int init_qdisc_fifo(struct evl_net_qdisc *qdisc)
{
	struct qdisc_fifo_priv *p = evl_qdisc_priv(qdisc);

	evl_net_init_skb_queue(&p->q);

	return 0;
}

static void destroy_qdisc_fifo(struct evl_net_qdisc *qdisc)
{
	struct qdisc_fifo_priv *p = evl_qdisc_priv(qdisc);

	evl_net_destroy_skb_queue(&p->q);
}

static int enqueue_qdisc_fifo(struct evl_net_qdisc *qdisc,
			struct sk_buff *skb)
{
	struct qdisc_fifo_priv *p = evl_qdisc_priv(qdisc);

	evl_net_add_skb_queue(&p->q, skb);

	return 0;
}

static struct sk_buff *dequeue_qdisc_fifo(struct evl_net_qdisc *qdisc)
{
	struct qdisc_fifo_priv *p = evl_qdisc_priv(qdisc);

	return evl_net_get_skb_queue(&p->q);
}

struct evl_net_qdisc_ops evl_net_qdisc_fifo = {
	.name	        = "oob_fifo",
	.priv_size      = sizeof(struct qdisc_fifo_priv),
	.init		= init_qdisc_fifo,
	.destroy	= destroy_qdisc_fifo,
	.enqueue	= enqueue_qdisc_fifo,
	.dequeue	= dequeue_qdisc_fifo,
};
