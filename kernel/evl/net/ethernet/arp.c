/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2021 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/if_ether.h>
#include <net/arp.h>
#include <evl/net/neighbour.h>

#define EVL_NET_ARP_HASHBITS  6	/* 2^6 buckets in hash table */

static DEFINE_HASHTABLE(evl_net_arp_hash, EVL_NET_ARP_HASHBITS);

struct evl_net_arp_neigh {
	struct evl_net_neighbour base;
	u32 ipaddr;
	unsigned char macaddr[ETH_ALEN];
};

static struct evl_net_neighbour *alloc_arp_neighbour(struct neighbour *src)
{
	return NULL;		/* FIXME */
}

static void free_arp_neighbour(struct evl_net_neighbour *neigh)
{
	/* FIXME */
}

/*
 * CAUTION: lookup at resolution must account for caller's net namespace:
 *
 * net_eq(dev_net(n->dev), caller_net))
 */

static void hash_arp_neighbour(struct evl_net_neigh_cache *cache,
			struct evl_net_neighbour *neigh)
{
	struct evl_net_arp_neigh *entry, *na;
	struct evl_net_neighbour *n;

	/* FIXME: locking (oob-suitable) ? */

	entry = container_of(neigh, struct evl_net_arp_neigh, base);

	hash_for_each_possible(evl_net_arp_hash, n, hash, entry->ipaddr) {
		na = container_of(n, struct evl_net_arp_neigh, base);
		/* Match the device which covers the net namespace. */
		if (na->base.dev == entry->base.dev &&
			na->ipaddr == entry->ipaddr)
			return;
	}

	hash_add(evl_net_arp_hash, &entry->base.hash, entry->ipaddr);
}

static void unhash_arp_neighbour(struct evl_net_neigh_cache *cache,
				struct evl_net_neighbour *neigh)
{
	/* FIXME: locking (oob-suitable) ? */

	hash_del(&neigh->hash);
}

static struct evl_net_neigh_cache_ops evl_net_arp_cache_ops = {
	.alloc	= alloc_arp_neighbour,
	.free	= free_arp_neighbour,
	.hash	= hash_arp_neighbour,
	.unhash	= unhash_arp_neighbour,
};

struct evl_net_neigh_cache evl_net_arp_cache = {
	.source = &arp_tbl,
	.ops = &evl_net_arp_cache_ops,
};
