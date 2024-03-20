/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_QDISC_H
#define _EVL_NET_QDISC_H

#include <linux/types.h>
#include <linux/list.h>
#include <evl/net/skb.h>

struct evl_net_qdisc;

struct evl_net_qdisc_ops {
	const char *name;
	size_t priv_size;
	int (*init)(struct evl_net_qdisc *qdisc);
	void (*destroy)(struct evl_net_qdisc *qdisc);
	int (*enqueue)(struct evl_net_qdisc *qdisc, struct sk_buff *skb);
	struct sk_buff *(*dequeue)(struct evl_net_qdisc *qdisc);
	struct list_head next;
};

struct evl_net_qdisc {
	const struct evl_net_qdisc_ops *oob_ops;
	unsigned long packet_dropped;
};

static inline void *evl_qdisc_priv(struct evl_net_qdisc *qdisc)
{
	return qdisc + 1;
}

void evl_net_register_qdisc(struct evl_net_qdisc_ops *ops);

void evl_net_unregister_qdisc(struct evl_net_qdisc_ops *ops);

struct evl_net_qdisc *
evl_net_alloc_qdisc(struct evl_net_qdisc_ops *ops);

void evl_net_free_qdisc(struct evl_net_qdisc *qdisc);

int evl_net_sched_packet(struct net_device *dev,
			 struct sk_buff *skb);

void evl_net_init_qdisc(void);

void evl_net_cleanup_qdisc(void);

extern struct evl_net_qdisc_ops evl_net_qdisc_fifo;

#endif /* !_EVL_NET_QDISC_H */
