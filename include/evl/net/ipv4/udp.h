/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2021 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_IPV4_UDP_H
#define _EVL_NET_IPV4_UDP_H

#include <linux/refcount.h>
#include <evl/net/socket.h>
#include <evl/cache.h>
#include <evl/sem.h>
#include <evl/mutex.h>

/* Cached UDP receiver. */
struct evl_net_udp_receiver {
	/* Generic cache entry. */
	struct evl_cache_entry entry;
	/* Queue of pending datagrams. */
	struct list_head queue;
	/* Wait queue receivers sleep on. */
	struct evl_wait_queue wait;
	/* Users (SO_REUSEPORT) */
	refcount_t refs;
	/* The hash key must be aliasable to u32[]. */
	struct __evl_net_udp_key {
		u32 dport;
		u32 daddr;
	} key __packed;
};

int evl_net_deliver_udp(struct sk_buff *skb);

int evl_net_init_udp(struct net *net);

void evl_net_cleanup_udp(struct net *net);

extern struct evl_net_proto evl_net_udp_proto;

#endif /* !_EVL_NET_IPV4_UDP_H */
