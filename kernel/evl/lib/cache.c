/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2023 Philippe Gerum  <rpm@xenomai.org>
 *
 * This file implements a generic dynamically-growable cache with
 * support for in-band update and out-of-band lookup.
 */

#include <linux/kmemleak.h>
#include <evl/cache.h>

static int realloc_hash_table(struct evl_cache *cache);

static void hash_free_rcu(struct rcu_head *rcu);

static void entry_free_rcu(struct rcu_head *rcu);

static void entry_free_work(struct evl_work *work);

int evl_init_cache(struct evl_cache *cache) /* in-band */
{
	spin_lock_init(&cache->lock);
	cache->hash_table = NULL;

	return realloc_hash_table(cache);
}
EXPORT_SYMBOL_GPL(evl_init_cache);

void evl_cleanup_cache(struct evl_cache *cache) /* in-band */
{
	evl_flush_cache(cache);
}
EXPORT_SYMBOL_GPL(evl_cleanup_cache);

/*
 * Cache a new entry. The cache must have been locked prior to calling
 * this routine. This call must be issued from the in-band stage.
 *
 * @entry entry to add to the cache.
 */
int evl_add_cache_entry_locked(struct evl_cache *cache, /* in-band */
			struct evl_cache_entry *entry)
{
	const void *key = cache->ops->get_key(entry);
	struct evl_cache_entry *e, **ep;
	struct evl_hash_table *ht;
	u32 hashval;
	int ret;

	hashval = cache->ops->hash(key);
retry:
	ht = rcu_dereference_protected(cache->hash_table,
				lockdep_is_held(&cache->lock));
	if (!ht || ht->nr_entries >= (1 << ht->shift)) {
		spin_unlock_bh(&cache->lock);
		ret = realloc_hash_table(cache);
		spin_lock_bh(&cache->lock);
		if (ret)
			return ret;
		goto retry;
	}

	/*
	 * Find the place to attach the new entry, dropping a previous
	 * match on the fly if any.
	 */
	hashval >>= (32 - ht->shift); /* Modulo table size. */
	for (ep = &ht->buckets[hashval],
		     e = rcu_dereference_protected(ht->buckets[hashval],
			     lockdep_is_held(&cache->lock));
	     e; e = rcu_dereference_protected(e->next,
		     lockdep_is_held(&cache->lock))) {
		if (cache->ops->eq(e, key)) {
			rcu_assign_pointer(*ep, e->next);
			if (refcount_dec_and_test(&e->refcnt))
				call_rcu(&e->rcu, entry_free_rcu);
		} else {
			ep = &e->next;
		}
	}

	ht->nr_entries++;
	entry->cache = cache;
	refcount_set(&entry->refcnt, 1);
	evl_init_work(&entry->work, entry_free_work);
	rcu_assign_pointer(*ep, entry);

	return 0;
}
EXPORT_SYMBOL_GPL(evl_add_cache_entry_locked);

/*
 * Cache a new entry. This call must be issued from the in-band
 * stage.
 *
 * @entry entry to add to the cache.
 */
int evl_add_cache_entry(struct evl_cache *cache, /* in-band */
			struct evl_cache_entry *entry)
{
	int ret;

	spin_lock_bh(&cache->lock);
	ret = evl_add_cache_entry_locked(cache, entry);
	spin_unlock_bh(&cache->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(evl_add_cache_entry);

/*
 * Remove an entry from the cache.  The cache must have been locked
 * prior to calling this routine. This call must be issued from the
 * in-band stage.
 *
 * @entry entry to remove from the cache.
 */
void evl_del_cache_entry_locked(struct evl_cache *cache, const void *key) /* in-band */
{
	struct evl_cache_entry *e, **ep;
	struct evl_hash_table *ht;
	u32 hashval;

	ht = rcu_dereference_protected(cache->hash_table,
				lockdep_is_held(&cache->lock));
	if (!ht)
		return;

	hashval = cache->ops->hash(key);
	hashval >>= (32 - ht->shift); /* Modulo table size. */

	/* Don't bark on invalid removal request, just ignore it. */
	for (ep = &ht->buckets[hashval],
		     e = rcu_dereference_protected(
			     ht->buckets[hashval],
			     lockdep_is_held(&cache->lock));
	     e; e = rcu_dereference_protected(
		     e->next,
		     lockdep_is_held(&cache->lock))) {
		if (cache->ops->eq(e, key)) {
			rcu_assign_pointer(*ep, e->next);
			if (refcount_dec_and_test(&e->refcnt))
				call_rcu(&e->rcu, entry_free_rcu);
			break;
		}
		ep = &e->next;
	}
}
EXPORT_SYMBOL_GPL(evl_del_cache_entry_locked);

/*
 * Remove an entry from the cache. This call must be issued from the
 * in-band stage.
 *
 * @entry entry to remove from the cache.
 */
void evl_del_cache_entry(struct evl_cache *cache, const void *key) /* in-band */
{
	spin_lock_bh(&cache->lock);
	evl_del_cache_entry_locked(cache, key);
	spin_unlock_bh(&cache->lock);
}
EXPORT_SYMBOL_GPL(evl_del_cache_entry);

void evl_flush_cache(struct evl_cache *cache) /* in-band */
{
	struct evl_cache_entry __rcu **buckets, *e, *next;
	struct evl_hash_table *ht;
	size_t size;
	int n;

	spin_lock_bh(&cache->lock);

	ht = rcu_dereference_protected(cache->hash_table,
				lockdep_is_held(&cache->lock));
	if (likely(ht)) {
		rcu_assign_pointer(cache->hash_table, NULL);
		size = (1 << ht->shift) * sizeof(struct evl_cache_entry *);
		buckets = rcu_dereference_protected(ht->buckets,
					lockdep_is_held(&cache->lock));

		for (n = 0; n < (1 << ht->shift); n++) {
			for (e = rcu_dereference_protected(buckets[n],
					lockdep_is_held(&cache->lock)); e; e = next) {
				next = rcu_dereference_protected(e->next,
						lockdep_is_held(&cache->lock));
				if (refcount_dec_and_test(&e->refcnt))
					call_rcu(&e->rcu, entry_free_rcu);
			}
		}

		call_rcu(&ht->rcu, hash_free_rcu);
	}

	spin_unlock_bh(&cache->lock);
}
EXPORT_SYMBOL_GPL(evl_flush_cache);

void evl_clean_cache(struct evl_cache *cache,
		bool (*testfn)(struct evl_cache_entry *e, void *arg),
		void *arg) /* in-band */
{
	struct evl_cache_entry __rcu **buckets, *e, **ep, *next;
	struct evl_hash_table *ht;
	size_t size;
	int n;

	spin_lock_bh(&cache->lock);

	ht = rcu_dereference_protected(cache->hash_table,
				lockdep_is_held(&cache->lock));
	if (likely(ht)) {
		size = (1 << ht->shift) * sizeof(struct evl_cache_entry *);
		buckets = rcu_dereference_protected(ht->buckets,
					lockdep_is_held(&cache->lock));

		for (n = 0; n < (1 << ht->shift); n++) {
			for (ep = &ht->buckets[n],
				     e = rcu_dereference_protected(buckets[n],
					lockdep_is_held(&cache->lock)); e; e = next) {
				next = rcu_dereference_protected(e->next,
						lockdep_is_held(&cache->lock));
				if (testfn(e, arg)) {
					rcu_assign_pointer(*ep, e->next);
					if (refcount_dec_and_test(&e->refcnt))
						call_rcu(&e->rcu, entry_free_rcu);
				} else {
					ep = &e->next;
				}
			}
		}
	}

	spin_unlock_bh(&cache->lock);
}
EXPORT_SYMBOL_GPL(evl_clean_cache);

/* in-band / oob */
struct evl_cache_entry *evl_lookup_cache(struct evl_cache *cache,
					const void *key)
{
	struct evl_cache_entry *e = NULL;
	struct evl_hash_table *ht;
	u32 hashval;

	rcu_read_lock();

	ht = rcu_dereference(cache->hash_table);
	if (likely(ht)) {
		hashval = cache->ops->hash(key);
		hashval >>= (32 - ht->shift); /* Modulo table size. */
		for (e = rcu_dereference(ht->buckets[hashval]);
		     e; e = rcu_dereference(e->next)) {
			if (cache->ops->eq(e, key)) {
				refcount_inc(&e->refcnt);
				break;
			}
		}
	}

	rcu_read_unlock();

	return e;
}
EXPORT_SYMBOL_GPL(evl_lookup_cache);

/* in-band / oob */
void evl_put_cache_entry(struct evl_cache_entry *entry)
{
	if (refcount_dec_and_test(&entry->refcnt)) {
		if (running_inband())
			call_rcu(&entry->rcu, entry_free_rcu);
		else
			evl_call_inband(&entry->work);
	}
}
EXPORT_SYMBOL_GPL(evl_put_cache_entry);

static void entry_free_rcu(struct rcu_head *rcu) /* in-band */
{
	struct evl_cache_entry *e = container_of(rcu, struct evl_cache_entry, rcu);
	e->cache->ops->drop(e);
}

/*
 * Trampoline to schedule our RCU callback. We could not do this
 * directly from oob, since call_rcu() would not support this, _and_
 * RCU does neither watch nor even know about the oob context in the
 * first place.
 */
static void entry_free_work(struct evl_work *work) /* in-band */
{
	struct evl_cache_entry *e = container_of(work, struct evl_cache_entry, work);
	call_rcu(&e->rcu, entry_free_rcu);
}

static void hash_free_rcu(struct rcu_head *head)
{
	struct evl_hash_table *ht =
		container_of(head, struct evl_hash_table, rcu);
	size_t size = (1 << ht->shift) * sizeof(struct evl_cache_entry *);
	struct evl_cache_entry __rcu **buckets = ht->buckets;

	if (size <= PAGE_SIZE) {
		kfree(buckets);
	} else {
		kmemleak_free(buckets);
		free_pages((unsigned long)buckets, get_order(size));
	}

	kfree(ht);
}

/*
 * Grow the hash table of a cache.  We may look up into a table from
 * oob inside a read-side RCU section (Dovetail may emulate an NMI
 * entry in this case).
 */
static int realloc_hash_table(struct evl_cache *cache)
{
	struct evl_hash_table *old_ht, *new_ht;
	struct evl_cache_entry __rcu **buckets;
	unsigned int shift;
	size_t size;
	u32 hash;
	int n;

	rcu_read_lock();
	old_ht = rcu_dereference(cache->hash_table);
	shift = old_ht ? old_ht->shift + 1 : cache->init_shift;
	rcu_read_unlock();
	size = (1 << shift) * sizeof(struct evl_cache_entry *);

	new_ht = kmalloc(sizeof(*new_ht), GFP_ATOMIC);
	if (!new_ht)
		return -ENOMEM;

	if (size <= PAGE_SIZE) {
		buckets = kzalloc(size, GFP_ATOMIC);
	} else {
		buckets = (struct evl_cache_entry __rcu **)
			  __get_free_pages(GFP_ATOMIC | __GFP_ZERO,
					   get_order(size));
		kmemleak_alloc(buckets, size, 1, GFP_ATOMIC);
	}

	if (!buckets) {
		kfree(new_ht);
		return -ENOMEM;
	}

	/* The table must be valid before aborting. */
	new_ht->buckets = buckets;
	new_ht->shift = shift;
	new_ht->nr_entries = 0;

	spin_lock_bh(&cache->lock);

	old_ht = rcu_dereference_protected(cache->hash_table, lockdep_is_held(&cache->lock));
	if (!old_ht)
		goto finish;

	/* Somebody slipped in and grew the table already, abort. */
	if (old_ht->shift >= shift) {
		spin_unlock_bh(&cache->lock);
		hash_free_rcu(&new_ht->rcu);
		return 0;
	}

	/* Copy the old table content to the new (larger) one. */
	for (n = 0; n < (1 << old_ht->shift); n++) {
		struct evl_cache_entry *e, *next;

		for (e = rcu_dereference_protected(old_ht->buckets[n],
						   lockdep_is_held(&cache->lock));
		     e; e = next) {
			hash = cache->ops->hash(e);
			hash >>= (32 - shift);
			next = rcu_dereference_protected(e->next,
						lockdep_is_held(&cache->lock));
			rcu_assign_pointer(e->next,
					   rcu_dereference_protected(
						new_ht->buckets[hash],
						lockdep_is_held(&cache->lock)));
			rcu_assign_pointer(new_ht->buckets[hash], e);
		}
	}

	new_ht->nr_entries = old_ht->nr_entries;

finish:
	rcu_assign_pointer(cache->hash_table, new_ht);
	spin_unlock_bh(&cache->lock);

	if (old_ht)
		call_rcu(&old_ht->rcu, hash_free_rcu);

	return 0;
}
