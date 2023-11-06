/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 *
 * A dynamically-growable cache with support for in-band update and
 * out-of-band lookup.
 */

#ifndef _EVL_CACHE_H
#define _EVL_CACHE_H

#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/refcount.h>
#include <evl/work.h>

struct evl_cache;

/* Cacheable entry. */
struct evl_cache_entry {
	/* Out-of-band reference count. */
	refcount_t refcnt;
	/* Link in hash table. */
	struct evl_cache_entry __rcu *next;
	/* Holder for work trampoline to RCU. */
	struct evl_work work;
	/* Owner cache. */
	struct evl_cache *cache;
	/* RCU holder for release. */
	struct rcu_head	rcu;
};

/* Client-specific operations. */
struct evl_cache_ops {
	u32 (*hash)(const void *key);
	bool (*eq)(const struct evl_cache_entry *entry, const void *key);
	void (*drop)(struct evl_cache_entry *entry);
	const void *(*get_key)(const struct evl_cache_entry *entry);
	char *(*format_key)(const struct evl_cache_entry *entry);
};

/* Dynamic hash table indexing entries. */
struct evl_hash_table {
	/* RCU-managed bucket array. */
	struct evl_cache_entry __rcu **buckets;
	/* Size order for bucket array. */
	unsigned int shift;
	/* Number of busy entries. */
	int nr_entries;
	/* RCU holder for release. */
	struct rcu_head	rcu;
};

/* Generic cache. */
struct evl_cache {
	/* Client-specific operation descriptor. */
	struct evl_cache_ops *ops;
	/* For serializing updates (in-band only). */
	spinlock_t lock;
	/* Hash table of entries. */
	struct evl_hash_table __rcu *hash_table;
	/* Initial cache shift. */
	size_t init_shift;
	/* Name of cache. */
	const char *name;
};

int evl_init_cache(struct evl_cache *cache);

void evl_cleanup_cache(struct evl_cache *cache);

int evl_add_cache_entry(struct evl_cache *cache,
			struct evl_cache_entry *entry);

int evl_add_cache_entry_locked(struct evl_cache *cache,
			struct evl_cache_entry *entry);

void evl_del_cache_entry(struct evl_cache *cache,
			const void *key);

void evl_del_cache_entry_locked(struct evl_cache *cache,
			const void *key);

struct evl_cache_entry *evl_lookup_cache(struct evl_cache *cache,
					const void *key);

static inline void evl_get_cache_entry(struct evl_cache_entry *entry)
{
	refcount_inc(&entry->refcnt);
}

void evl_put_cache_entry(struct evl_cache_entry *entry);

void evl_clean_cache(struct evl_cache *cache,
		bool (*testfn)(struct evl_cache_entry *e, void *arg),
		void *arg);

void evl_flush_cache(struct evl_cache *cache);

static inline void evl_lock_cache(struct evl_cache *cache)
{
	spin_lock_bh(&cache->lock);
}

static inline void evl_unlock_cache(struct evl_cache *cache)
{
	spin_unlock_bh(&cache->lock);
}

#endif /* !_EVL_CACHE_H */
