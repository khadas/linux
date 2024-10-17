/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/slab.h>
#include <net/route.h>
#include <net/ip.h>
#include <evl/net/ipv4/route.h>

/* Start an IPv4 route cache with 256 entries. */
#define EVL_NET_IPV4_ROUTE_SHIFT  8

static u32 hash_ipv4_route(const void *key)
{
	return ipv4_addr_hash(*(const u32 *)key);
}

static bool eq_ipv4_route(const struct evl_cache_entry *entry,
			const void *key)
{
	const struct evl_net_route *e =
		container_of(entry, struct evl_net_route, entry);

	return *(const __be32 *)e->key == *(const __be32 *)key;
}

static char *format_ipv4_key(const struct evl_cache_entry *entry)
{
	const struct evl_net_route *e =
		container_of(entry, struct evl_net_route, entry);

	return kasprintf(GFP_ATOMIC, "%pI4", e->key);
}

static const void *get_ipv4_key(const struct evl_cache_entry *entry)
{
	const struct evl_net_route *e =
		container_of(entry, struct evl_net_route, entry);

	return e->key;
}

static struct evl_cache_ops ipv4_route_cache_ops = {
	.hash		= hash_ipv4_route,
	.eq		= eq_ipv4_route,
	.get_key	= get_ipv4_key,
	.format_key	= format_ipv4_key,
	.drop		= evl_net_free_route,
};

int evl_net_init_ipv4_routing(struct net *net)
{
	struct oob_net_state *nets = &net->oob;
	struct evl_cache *cache;

	/* Route cache for IPv4 destinations. */
	cache = &nets->ipv4.routes;
	cache->ops = &ipv4_route_cache_ops;
	cache->init_shift = EVL_NET_IPV4_ROUTE_SHIFT;
	cache->name = "ipv4_routes";

	return evl_init_cache(cache);
}

static bool compare_route_dev(struct evl_cache_entry *entry, void *arg)
{
	const struct evl_net_route *ert =
		container_of(entry, struct evl_net_route, entry);
	struct net_device *dev = arg;

	return dev == evl_net_route_dev(ert);
}

static inline void flush_route_cache(struct net *net, struct net_device *dev)
{
	if (dev)
		evl_clean_cache(&net->oob.ipv4.routes, compare_route_dev, dev);
	else
		evl_flush_cache(&net->oob.ipv4.routes);
}

void evl_net_cleanup_ipv4_routing(struct net *net)
{
	flush_route_cache(net, NULL);
}

/*
 *  Update the out-of-band front-cache on the fly with the routing
 *  information we received for the outgoing IPv4 traffic on an
 *  oob-enabled device. We may be running in softirq context, don't
 *  wait.
 */
void evl_net_learn_ipv4_route(struct net *net,
			struct flowi4 *fl4, struct rtable *rt) /* in-band */
{
	struct oob_net_state *nets = &net->oob;
	struct net_device *dev = rt->dst.dev;
	struct evl_net_route *e;
	int ret;

	netdev_dbg(dev, "learning ipv4 route: %pI4 -> %pI4 via %s\n",
		   &fl4->saddr, &fl4->daddr, netdev_name(dev));

	e = evl_net_get_ipv4_route(net, fl4->daddr);
	if (e && e->rt->dst.dev == dev) {
		evl_net_put_route(e);
		return;
	}

	e = kzalloc(sizeof(*e) + sizeof(fl4->daddr), GFP_ATOMIC);
	if (!e)
		goto warn;

	e->rt = rt_dst_clone(dev, rt);
	*(__be32 *)e->key = fl4->daddr;
	netdev_dbg(dev, "caching route to %pI4\n", &fl4->daddr);
	ret = evl_add_cache_entry(&nets->ipv4.routes, &e->entry);
	if (ret) {
		ip_rt_put(e->rt);
		kfree(e);
		goto warn;
	}

	return;
warn:
	printk(EVL_WARNING "out of memory for IPv4 route cache\n");
}

/*
 * Find a route to a destination IPv4 peer in the front cache.
 *
 * On success, the caller needs to release the route by a call to
 * evl_net_put_route().
 */
struct evl_net_route *
evl_net_get_ipv4_route(struct net *net, __be32 daddr)
{
	struct oob_net_state *nets = &net->oob;
	struct evl_cache_entry *entry;

	entry = evl_lookup_cache(&nets->ipv4.routes, &daddr);
	if (likely(entry))
		return container_of(entry, struct evl_net_route, entry);

	return NULL;
}

void evl_net_flush_ipv4_routes(struct net *net, struct net_device *dev)
{
	flush_route_cache(net, dev);
}
