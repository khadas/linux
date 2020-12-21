/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2020 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_SKB_H
#define _EVL_NET_SKB_H

#include <linux/types.h>
#include <linux/list.h>
#include <evl/lock.h>
#include <evl/timeout.h>

struct sk_buff;
struct net_device;
struct evl_net_handler;
struct evl_net_skb_queue;
struct evl_socket;

struct evl_net_cb {
	struct evl_net_handler *handler;
	struct sk_buff *origin;	/* of a clone. */
	dma_addr_t dma_addr;
	struct evl_socket *tracker;
	union {
		/* protocol-specific stuff should live here. */
	};
};
#define EVL_NET_CB(__skb)  ((struct evl_net_cb *)&((__skb)->cb[0]))

void evl_net_init_skb_queue(struct evl_net_skb_queue *txq);

void evl_net_destroy_skb_queue(struct evl_net_skb_queue *txq);

void evl_net_add_skb_queue(struct evl_net_skb_queue *skbq,
			struct sk_buff *skb);

struct sk_buff *
evl_net_get_skb_queue(struct evl_net_skb_queue *skbq);

bool evl_net_move_skb_queue(struct evl_net_skb_queue *skbq,
			struct list_head *list);

int evl_net_dev_build_pool(struct net_device *dev);

void evl_net_dev_purge_pool(struct net_device *dev);

struct sk_buff *evl_net_dev_alloc_skb(struct net_device *dev,
				      ktime_t timeout,
				      enum evl_tmode tmode);

void evl_net_free_skb(struct sk_buff *skb);

void evl_net_free_skb_list(struct list_head *list);

struct sk_buff *evl_net_clone_skb(struct sk_buff *skb);

int evl_net_init_pools(void);

ssize_t evl_net_show_clones(char *buf, size_t len);

#endif /* !_EVL_NET_SKB_H */
