/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt, https://xenomai.org/
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#ifndef _EVENLESS_MEMORY_H
#define _EVENLESS_MEMORY_H

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/rbtree.h>
#include <evenless/list.h>
#include <evenless/factory.h>
#include <uapi/evenless/types.h>

#define EVL_HEAP_PAGE_SHIFT	9 /* 2^9 => 512 bytes */
#define EVL_HEAP_PAGE_SIZE	(1UL << EVL_HEAP_PAGE_SHIFT)
#define EVL_HEAP_PAGE_MASK	(~(EVL_HEAP_PAGE_SIZE - 1))
#define EVL_HEAP_MIN_LOG2	4 /* 16 bytes */
/*
 * Use bucketed memory for sizes between 2^EVL_HEAP_MIN_LOG2 and
 * 2^(EVL_HEAP_PAGE_SHIFT-1).
 */
#define EVL_HEAP_MAX_BUCKETS	(EVL_HEAP_PAGE_SHIFT - EVL_HEAP_MIN_LOG2)
#define EVL_HEAP_MIN_ALIGN	(1U << EVL_HEAP_MIN_LOG2)
/* Maximum size of a heap (4Gb - PAGE_SIZE). */
#define EVL_HEAP_MAX_HEAPSZ	(4294967295U - PAGE_SIZE + 1)
/* Bits we need for encoding a page # */
#define EVL_HEAP_PGENT_BITS      (32 - EVL_HEAP_PAGE_SHIFT)
/* Each page is represented by a page map entry. */
#define EVL_HEAP_PGMAP_BYTES	sizeof(struct evl_heap_pgentry)

struct evl_heap_pgentry {
	/* Linkage in bucket list. */
	unsigned int prev : EVL_HEAP_PGENT_BITS;
	unsigned int next : EVL_HEAP_PGENT_BITS;
	/*  page_list or log2. */
	unsigned int type : 6;
	/*
	 * We hold either a spatial map of busy blocks within the page
	 * for bucketed memory (up to 32 blocks per page), or the
	 * overall size of the multi-page block if entry.type ==
	 * page_list.
	 */
	union {
		u32 map;
		u32 bsize;
	};
};

/*
 * A range descriptor is stored at the beginning of the first page of
 * a range of free pages. evl_heap_range.size is nrpages *
 * EVL_HEAP_PAGE_SIZE. Ranges are indexed by address and size in
 * rbtrees.
 */
struct evl_heap_range {
	struct rb_node addr_node;
	struct rb_node size_node;
	size_t size;
};

struct evl_heap {
	void *membase;
	struct rb_root addr_tree;
	struct rb_root size_tree;
	struct evl_heap_pgentry *pagemap;
	size_t usable_size;
	size_t used_size;
	u32 buckets[EVL_HEAP_MAX_BUCKETS];
	hard_spinlock_t lock;
	struct list_head next;
};

extern struct evl_heap evl_system_heap;

extern struct evl_heap evl_shared_heap;

static inline size_t evl_get_heap_size(const struct evl_heap *heap)
{
	return heap->usable_size;
}

static inline size_t evl_get_heap_free(const struct evl_heap *heap)
{
	return heap->usable_size - heap->used_size;
}

static inline void *evl_get_heap_base(const struct evl_heap *heap)
{
	return heap->membase;
}

int evl_init_heap(struct evl_heap *heap, void *membase,
		size_t size);

void evl_destroy_heap(struct evl_heap *heap);

void *evl_alloc_chunk(struct evl_heap *heap, size_t size);

void evl_free_chunk(struct evl_heap *heap, void *block);

ssize_t evl_check_chunk(struct evl_heap *heap, void *block);

static inline void *evl_zalloc_chunk(struct evl_heap *heap, u32 size)
{
	void *p;

	p = evl_alloc_chunk(heap, size);
	if (p)
		memset(p, 0, size);

	return p;
}

static inline
int evl_shared_offset(void *p)
{
	return p - evl_get_heap_base(&evl_shared_heap);
}

static inline void *evl_alloc(size_t size)
{
	return evl_alloc_chunk(&evl_system_heap, size);
}

static inline void evl_free(void *ptr)
{
	evl_free_chunk(&evl_system_heap, ptr);
}

int evl_init_memory(void);

void evl_cleanup_memory(void);

extern size_t evl_shm_size;

#endif /* !_EVENLESS_MEMORY_H */
