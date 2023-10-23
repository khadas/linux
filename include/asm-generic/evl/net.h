/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_GENERIC_EVL_NET_H
#define _ASM_GENERIC_EVL_NET_H

#ifdef CONFIG_EVL_NET

#include <linux/hashtable.h>
#include <linux/rbtree.h>
#include <net/inet_frag.h>
#include <evl/lock.h>
#include <evl/mutex.h>
#include <evl/timer.h>
#include <evl/cache.h>

#define EVL_NET_FRAGS_HASHBITS 7

struct evl_net_frag_tdir;
struct net_device;

struct evl_net_frag_tree {
	/* End offset (bytes). */
	size_t end;
	/* Cumulated length (bytes). */
	size_t len;
	/* Status flags. */
	int flags;
	/* Root of fragment tree. */
	struct rb_root frags;
	/* Hash collision link. */
	struct hlist_node hash;
	/* Backlink to parent tree directory. */
	struct evl_net_frag_tdir *tdir;
	/* The device performing garbage collection. */
	struct net_device *gc_dev;
	/* Garbage collection link (pending). */
	struct hlist_node gc;
	/* Expiration timer (set to IP_FRAG_TIME). */
	struct evl_timer timer;
	/*
	 * Serializes access to the frags tree. This is lightweight,
	 * and may be initialized from the out-of-band stage.
	 */
	evl_spinlock_t lock;
	/* Hash key (ipv4 so far). */
	union {
		struct frag_v4_compare_key ipv4;
	} key;
};

struct evl_net_frag_gc {
	/* Garbage collection queue for fragment trees. */
	struct hlist_head queue;
	/*
	 * Serializes access to the garbage collection queue between
	 * an oob interrupt handler and a kthread.
	 */
	hard_spinlock_t lock;
};

struct evl_net_frag_tdir {
	/* Hash map of fragment trees. */
	DECLARE_HASHTABLE(ht, EVL_NET_FRAGS_HASHBITS);
	/* Serializes updates to the hash table. */
	struct evl_kmutex lock;
	/* Garbage collector. */
	struct evl_net_frag_gc gc;
	/* Fragment lifetime (ns). */
	ktime_t timeout;
};

struct oob_net_state {
	struct {
		/* Fragment tree directory. */
		struct evl_net_frag_tdir ftdir;
		/* ARP resolution cache. */
		struct evl_cache arp;
		/* Route cache of IPv4 destinations. */
		struct evl_cache routes;
		/* Cache of active UDP4 receivers. */
		struct evl_cache udp;
	} ipv4;
};

void net_init_oob_state(struct net *net);
void net_cleanup_oob_state(struct net *net);

#else

struct oob_net_state {
};

struct net;

static inline void net_init_oob_state(struct net *net) { }
static inline void net_cleanup_oob_state(struct net *net) { }

#endif

#endif /* !_ASM_GENERIC_EVL_NET_H */
