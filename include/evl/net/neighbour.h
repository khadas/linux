/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2021 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_NEIGHBOUR_H
#define _EVL_NET_NEIGHBOUR_H

#include <linux/refcount.h>
#include <net/neighbour.h>

struct evl_net_neigh_cache;
struct evl_net_neighbour;

struct evl_net_neigh_cache_ops {
	struct evl_net_neighbour *(*alloc)(struct neighbour *src);
	void (*free)(struct evl_net_neighbour *neigh);
	void (*hash)(struct evl_net_neigh_cache *cache,
		struct evl_net_neighbour *neigh);
	void (*unhash)(struct evl_net_neigh_cache *cache,
		struct evl_net_neighbour *neigh);
};

struct evl_net_neigh_cache {
	struct evl_net_neigh_cache_ops *ops;
	struct neigh_table *source;
	struct list_head next;	/* in cache_list */
};

struct evl_net_neighbour {
	struct evl_net_neigh_cache *cache;
	refcount_t refcnt;
	struct net_device *dev;
	struct hlist_node hash;
};

int evl_net_init_neighbour(void);

void evl_net_cleanup_neighbour(void);

extern struct evl_net_neigh_cache evl_net_arp_cache;

#endif /* !_EVL_NET_NEIGHBOUR_H */
