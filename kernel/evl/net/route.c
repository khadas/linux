/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/slab.h>
#include <net/route.h>
#include <evl/net/socket.h>
#include <evl/net/ipv4/route.h>

/*
 * Cache a new IP route.
 *
 * @rt route to cache. The caller got a reference on the inner dst
 * entry.
 */
int evl_net_cache_route(struct evl_cache *cache, /* in-band */
			struct rtable *rt,
			const void *key, size_t key_len)
{
	struct evl_net_route *e;
	int ret;

	e = kzalloc(sizeof(*e) + key_len, GFP_ATOMIC);
	if (!e)
		return -ENOMEM;

	e->rt = rt;
	memcpy(e->key, key, key_len);

	ret = evl_add_cache_entry(cache, &e->entry);
	if (ret)
		kfree(e);

	return ret;
}

/*
 * Release a route. This is a common helper which may be called by the
 * drop() handler from the generic cache layer for any cached IP route
 * entry.
 */
void evl_net_free_route(struct evl_cache_entry *entry) /* in-band */
{
	struct evl_net_route *e =
		container_of(entry, struct evl_net_route, entry);

	netdev_dbg(evl_net_route_dev(e), "dropping IPv4 route %pI4\n", e->key);

	/* Drop the ref. we received in evl_net_cache_route(). */
	ip_rt_put(e->rt);

	kfree(e);
}

/*
 * in-band hook which receives IPv4 routing decisions which go through
 * an oob-enabled device.
 */
void ip_learn_oob_route(struct net *net, struct flowi4 *fl4, struct rtable *rt)
{
	evl_net_learn_ipv4_route(net, fl4, rt);
}

/*
 * Flush the routes maintained in the out-of-band front cache. If @dev
 * is non-NULL, only the routes going via the device are
 * dropped. Otherwise, NULL is a wildcard for dropping all routes.
 */
void evl_net_flush_routes(struct net *net, struct net_device *dev)
{
	evl_net_flush_ipv4_routes(net, dev);
}
