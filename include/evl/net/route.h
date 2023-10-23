/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_ROUTE_H
#define _EVL_NET_ROUTE_H

#include <net/route.h>
#include <evl/cache.h>

struct net;
struct evl_work;

/* Cached route for IP datagrams. */
struct evl_net_route {
	/* Generic cache entry. */
	struct evl_cache_entry entry;
	/* Destination as resolved in-band (in-band refcounted). */
	struct rtable *rt;
	/* Index key (destination IP). */
	u8 key[0] __aligned(4);
};

int evl_net_cache_route(struct evl_cache *cache,
			struct rtable *rt,
			const void *key,
			size_t key_len);

void evl_net_free_route(struct evl_cache_entry *entry);

static inline void evl_net_put_route(struct evl_net_route *ert)
{
	evl_put_cache_entry(&ert->entry);
}

void evl_net_flush_routes(struct net *net, struct net_device *dev);

static inline struct dst_entry *evl_net_route_dst(const struct evl_net_route *ert)
{
	return &ert->rt->dst;
}

static inline struct net_device *evl_net_route_dev(const struct evl_net_route *ert)
{
	return evl_net_route_dst(ert)->dev;
}

#endif /* !_EVL_NET_ROUTE_H */
