/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2021-2023 Philippe Gerum  <rpm@xenomai.org>
 *
 * This file implements a simple out-of-band front cache to the ARP
 * table maintained by the in-band network stack. Basically, we listen
 * to update events from the later, caching new complete entries
 * observed on oob-enabled devices, uncaching invalidated/dead
 * entries. The front cache may be used safely from oob context for
 * address lookup.
 */

#include <linux/if_ether.h>
#include <linux/hash.h>
#include <linux/notifier.h>
#include <net/netevent.h>
#include <net/arp.h>
#include <evl/net/ipv4/arp.h>

#define EVL_NET_ARP_CACHE_SHIFT  8

static u32 hash_arp_entry(const void *key)
{
	const struct evl_net_arp_key *arp_k = key;
	return arp_k->addr ^ hash32_ptr(arp_k->dev);
}

static bool eq_arp_entry(const struct evl_cache_entry *entry,
			const void *key)
{
	const struct evl_net_arp_entry *e =
		container_of(entry, struct evl_net_arp_entry, entry);
	const struct evl_net_arp_key *arp_k = key;

	return e->key.addr == arp_k->addr && e->key.dev == arp_k->dev;
}

static char *format_arp_key(const struct evl_cache_entry *entry)
{
	const struct evl_net_arp_entry *e =
		container_of(entry, struct evl_net_arp_entry, entry);

	return kasprintf(GFP_ATOMIC, "%pI4", &e->key.addr);
}

static const void *get_arp_key(const struct evl_cache_entry *entry)
{
	const struct evl_net_arp_entry *e =
		container_of(entry, struct evl_net_arp_entry, entry);

	return &e->key;
}

static void free_arp_entry(struct evl_cache_entry *entry) /* in-band */
{
	struct evl_net_arp_entry *e =
		container_of(entry, struct evl_net_arp_entry, entry);

	netdev_put(e->key.dev, &e->dev_tracker);
	kfree(e);
}

static struct evl_cache_ops arp_cache_ops = {
	.hash		= hash_arp_entry,
	.eq		= eq_arp_entry,
	.get_key	= get_arp_key,
	.format_key	= format_arp_key,
	.drop		= free_arp_entry,
};

/*
 * Cache a new ARP entry.
 */
static int cache_arp_entry(struct evl_cache *cache, struct neighbour *neigh) /* in-band */
{
	struct net_device *dev = neigh->dev;
	struct evl_net_arp_entry *e;
	int ret;

	e = kzalloc(sizeof(*e), GFP_ATOMIC);
	if (!e)
		return -ENOMEM;

	e->key.dev = dev;
	e->key.addr = *(const __be32 *)neigh->primary_key;
	memcpy(e->ha, neigh->ha, sizeof(e->ha));
	netdev_hold(dev, &e->dev_tracker, GFP_ATOMIC);

	ret = evl_add_cache_entry(cache, &e->entry);
	if (ret) {
		netdev_put(dev, &e->dev_tracker);
		kfree(e);
	}

	return ret;
}

/*
 * Uncache an ARP entry.
 */
static void uncache_arp_entry(struct evl_cache *cache, struct neighbour *neigh) /* in-band */
{
	const struct evl_net_arp_key key = {
		.addr = *(const __be32 *)neigh->primary_key,
		.dev = neigh->dev,
	};

	evl_del_cache_entry(cache, &key);
}

/*
 * Handle an update notification from the in-band ARP cache.
 */
static void update_arp_cache(struct neighbour *neigh) /* in-band */
{
	struct net_device *dev = neigh->dev;
	struct oob_net_state *nets = &dev_net(dev)->oob;
	struct evl_cache *cache = &nets->ipv4.arp;
	int ret;

	read_lock_bh(&neigh->lock); /* Protect against races on nud_state */

	if (netif_oob_port(dev))
		netdev_dbg(dev, "proto=%#x, state=%#x, dead=%d, "
			"iface=%s, ip=%pI4, mac=%pM\n",
			ntohs(neigh->tbl->protocol), neigh->nud_state,
			neigh->dead, netdev_name(neigh->dev),
			neigh->primary_key, neigh->ha);

	if (neigh->nud_state & NUD_REACHABLE && netif_oob_port(dev)) {
		/* Cache complete entries from oob-enabled devices. */
		ret = cache_arp_entry(cache, neigh);
		if (ret) {
			/*
			 * Yeah, well. Nothing useful we can do. Now
			 * the oob cache is out of sync and since the
			 * in-band one is up to date, nothing would
			 * justify to redo the resolution so that we
			 * might catch its output - except flushing
			 * the in-band cache entirely. This said, this
			 * would likely be the gentlest effect of
			 * receiving OOM here anyway.
			 */
			printk(EVL_WARNING "out of memory for ARP cache\n");
		}
	} else {
		/*
		 * Try uncaching any invalidated, stale or dead entry.
		 * Out-of-band caps might have just been turned off
		 * for the device although we still keep entries
		 * referring to it, so do not filter out on
		 * netif_oob_port().
		 */
		if (neigh->dead || neigh->nud_state & (NUD_FAILED|NUD_STALE))
			uncache_arp_entry(cache, neigh);
	}

	read_unlock_bh(&neigh->lock);
}

static int netevent_handler(struct notifier_block *nb,
			unsigned long event, void *arg)
{
 	struct neighbour *neigh = arg;

	if (event == NETEVENT_NEIGH_UPDATE && neigh->tbl == &arp_tbl)
		update_arp_cache(neigh);

	return NOTIFY_DONE;
}

struct evl_net_arp_entry *evl_net_get_arp_entry(struct net_device *dev, __be32 addr)
{
	const struct evl_net_arp_key key = {
		.addr = addr,
		.dev = dev,
	};
	struct oob_net_state *nets = &dev_net(dev)->oob;
	struct evl_cache_entry *entry;

	entry = evl_lookup_cache(&nets->ipv4.arp, &key);
	if (likely(entry))
		return container_of(entry, struct evl_net_arp_entry, entry);

	return NULL;
}

static struct notifier_block netevent_notifier __read_mostly = {
	.notifier_call = netevent_handler,
};

void evl_net_flush_arp(struct net *net)
{
	struct oob_net_state *nets = &net->oob;

	evl_flush_cache(&nets->ipv4.arp);
}

int evl_net_init_arp(struct net *net)
{
	struct oob_net_state *nets = &net->oob;
	struct evl_cache *cache;
	int ret;

	/* ARP resolution cache. */
	cache = &nets->ipv4.arp;
	cache->ops = &arp_cache_ops;
	cache->init_shift = EVL_NET_ARP_CACHE_SHIFT;
	cache->name = "ARP";

	ret = evl_init_cache(cache);
	if (ret)
		return ret;

	register_netevent_notifier(&netevent_notifier);

	return 0;
}

void evl_net_cleanup_arp(struct net *net)
{
	unregister_netevent_notifier(&netevent_notifier);
	evl_net_flush_arp(net);
}
