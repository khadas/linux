/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2021 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/notifier.h>
#include <linux/mutex.h>
#include <net/netevent.h>
#include <evl/net/neighbour.h>

static DEFINE_SPINLOCK(cache_lock);

static LIST_HEAD(cache_list);

static void update_neigh_cache(struct evl_net_neigh_cache *cache,
			struct neighbour *neigh)
{
	if (neigh->nud_state & NUD_CONNECTED)
		; /* (re-)cache, complete */
	else if (neigh->dead)
		; /* dropped from source table, destroy */

#if 0
	printk("# NEIGH: proto=%#x, state=%#x, iface=%s\n", ntohs(neigh->tbl->protocol),
		neigh->nud_state, neigh->dev ? netdev_name(neigh->dev) : "??");
#endif
}

static int netevent_handler(struct notifier_block *nb,
			unsigned long event, void *arg)
{
	struct evl_net_neigh_cache *cache;
 	struct neighbour *neigh = arg;

	spin_lock(&cache_lock);

	list_for_each_entry(cache, &cache_list, next) {
		if (event != NETEVENT_NEIGH_UPDATE)
			continue;
		if (neigh->tbl == cache->source)
			update_neigh_cache(cache, neigh);
	}

	spin_unlock(&cache_lock);

	return NOTIFY_DONE;
}

static void register_neigh_cache(struct evl_net_neigh_cache *cache)
{
	spin_lock(&cache_lock);
	list_add(&cache->next, &cache_list);
	spin_unlock(&cache_lock);
}

static void unregister_neigh_cache(struct evl_net_neigh_cache *cache)
{
	spin_lock(&cache_lock);
	list_del(&cache->next);
	spin_unlock(&cache_lock);
}

static struct notifier_block evl_netevent_notifier __read_mostly = {
	.notifier_call = netevent_handler,
};

int evl_net_init_neighbour(void)
{
	register_neigh_cache(&evl_net_arp_cache);
	register_netevent_notifier(&evl_netevent_notifier);

	return 0;
}

void evl_net_cleanup_neighbour(void)
{
	unregister_netevent_notifier(&evl_netevent_notifier);
	unregister_neigh_cache(&evl_net_arp_cache);
}
