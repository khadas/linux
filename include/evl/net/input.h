/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_INPUT_H
#define _EVL_NET_INPUT_H

#include <linux/list.h>
#include <evl/lock.h>

struct sk_buff;

struct evl_net_handler {
	void (*ingress)(struct sk_buff *skb);
};

struct evl_net_rxqueue {
	u32 hkey;
	struct hlist_node hash;
	struct list_head subscribers;
	evl_spinlock_t lock;
	struct list_head next;
};

void evl_net_do_rx(void *arg);

void evl_net_receive(struct sk_buff *skb,
		struct evl_net_handler *handler);

struct evl_net_rxqueue *evl_net_alloc_rxqueue(u32 hkey);

void evl_net_free_rxqueue(struct evl_net_rxqueue *rxq);

bool evl_net_ether_accept(struct sk_buff *skb);

static inline bool evl_net_rxqueue_active(struct evl_net_rxqueue *rxq)
{
	return list_empty(&rxq->subscribers);
}

ssize_t evl_net_store_vlans(const char *buf, size_t len);

ssize_t evl_net_show_vlans(char *buf, size_t len);

#endif /* !_EVL_NET_INPUT_H */
