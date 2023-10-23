/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2021 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVL_NET_IPV4_ARP_H
#define _EVL_NET_IPV4_ARP_H

#include <net/neighbour.h>
#include <evl/cache.h>

/* ARP entry. */
struct evl_net_arp_entry {
	/* Generic cache entry. */
	struct evl_cache_entry entry;
	/* Device reference tracker. */
	netdevice_tracker dev_tracker;
	/* Cached hardware address. */
	unsigned char ha[ALIGN(MAX_ADDR_LEN, sizeof(unsigned long))] __aligned(8);
	/* Index key. */
	struct evl_net_arp_key {
		/* Associated netdev (in-band refcounted). */
		struct net_device *dev;
		/* IPv4 address of neighbour. */
		__be32 addr;
	} key;
};

int evl_net_init_arp(struct net *net);

void evl_net_cleanup_arp(struct net *net);

void evl_net_flush_arp(struct net *net);

struct evl_net_arp_entry *
evl_net_get_arp_entry(struct net_device *dev, __be32 addr);

static inline void evl_net_put_arp_entry(struct evl_net_arp_entry *earp)
{
	evl_put_cache_entry(&earp->entry);
}

#endif /* !_EVL_NET_IPV4_ARP_H */
