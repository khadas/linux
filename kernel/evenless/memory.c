/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/log2.h>
#include <linux/bitops.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <evenless/memory.h>
#include <evenless/factory.h>
#include <evenless/assert.h>
#include <evenless/init.h>
#include <uapi/evenless/thread.h>
#include <uapi/evenless/sem.h>
#include <uapi/evenless/monitor.h>

static unsigned long sysheap_size_arg;
module_param_named(sysheap_size, sysheap_size_arg, ulong, 0444);

struct evl_heap evl_system_heap;
EXPORT_SYMBOL_GPL(evl_system_heap);

struct evl_heap evl_shared_heap;

size_t evl_shm_size;

enum evl_heap_pgtype {
	page_free =0,
	page_cont =1,
	page_list =2
};

static inline u32 __always_inline
gen_block_mask(int log2size)
{
	return -1U >> (32 - (EVL_HEAP_PAGE_SIZE >> log2size));
}

static inline  __always_inline
int addr_to_pagenr(struct evl_heap *heap, void *p)
{
	return ((void *)p - heap->membase) >> EVL_HEAP_PAGE_SHIFT;
}

static inline  __always_inline
void *pagenr_to_addr(struct evl_heap *heap, int pg)
{
	return heap->membase + (pg << EVL_HEAP_PAGE_SHIFT);
}

#ifdef CONFIG_EVENLESS_DEBUG_MEMORY
/*
 * Setting page_cont/page_free in the page map is only required for
 * enabling full checking of the block address in free requests, which
 * may be extremely time-consuming when deallocating huge blocks
 * spanning thousands of pages. We only do such marking when running
 * in memory debug mode.
 */
static inline bool
page_is_valid(struct evl_heap *heap, int pg)
{
	switch (heap->pagemap[pg].type) {
	case page_free:
	case page_cont:
		return false;
	case page_list:
	default:
		return true;
	}
}

static void mark_pages(struct evl_heap *heap,
		int pg, int nrpages,
		enum evl_heap_pgtype type)
{
	while (nrpages-- > 0)
		heap->pagemap[pg].type = type;
}

#else

static inline bool
page_is_valid(struct evl_heap *heap, int pg)
{
	return true;
}

static void mark_pages(struct evl_heap *heap,
		int pg, int nrpages,
		enum evl_heap_pgtype type)
{ }

#endif

static struct evl_heap_range *
search_size_ge(struct rb_root *t, size_t size)
{
	struct rb_node *rb, *deepest = NULL;
	struct evl_heap_range *r;

	/*
	 * We first try to find an exact match. If that fails, we walk
	 * the tree in logical order by increasing size value from the
	 * deepest node traversed until we find the first successor to
	 * that node, or nothing beyond it, whichever comes first.
	 */
	rb = t->rb_node;
	while (rb) {
		deepest = rb;
		r = rb_entry(rb, struct evl_heap_range, size_node);
		if (size < r->size) {
			rb = rb->rb_left;
			continue;
		}
		if (size > r->size) {
			rb = rb->rb_right;
			continue;
		}
		return r;
	}

	rb = deepest;
	while (rb) {
		r = rb_entry(rb, struct evl_heap_range, size_node);
		if (size <= r->size)
			return r;
		rb = rb_next(rb);
	}

	return NULL;
}

static struct evl_heap_range *
search_left_mergeable(struct evl_heap *heap, struct evl_heap_range *r)
{
	struct rb_node *node = heap->addr_tree.rb_node;
	struct evl_heap_range *p;

	while (node) {
		p = rb_entry(node, struct evl_heap_range, addr_node);
		if ((void *)p + p->size == (void *)r)
			return p;
		if (&r->addr_node < node)
			node = node->rb_left;
		else
			node = node->rb_right;
	}

	return NULL;
}

static struct evl_heap_range *
search_right_mergeable(struct evl_heap *heap, struct evl_heap_range *r)
{
	struct rb_node *node = heap->addr_tree.rb_node;
	struct evl_heap_range *p;

	while (node) {
		p = rb_entry(node, struct evl_heap_range, addr_node);
		if ((void *)r + r->size == (void *)p)
			return p;
		if (&r->addr_node < node)
			node = node->rb_left;
		else
			node = node->rb_right;
	}

	return NULL;
}

static void insert_range_bysize(struct evl_heap *heap, struct evl_heap_range *r)
{
	struct rb_node **new = &heap->size_tree.rb_node, *parent = NULL;
	struct evl_heap_range *p;

	while (*new) {
		p = container_of(*new, struct evl_heap_range, size_node);
		parent = *new;
		if (r->size <= p->size)
			new = &((*new)->rb_left);
		else
			new = &((*new)->rb_right);
	}

	rb_link_node(&r->size_node, parent, new);
	rb_insert_color(&r->size_node, &heap->size_tree);
}

static void insert_range_byaddr(struct evl_heap *heap, struct evl_heap_range *r)
{
	struct rb_node **new = &heap->addr_tree.rb_node, *parent = NULL;
	struct evl_heap_range *p;

	while (*new) {
		p = container_of(*new, struct evl_heap_range, addr_node);
		parent = *new;
		if (r < p)
			new = &((*new)->rb_left);
		else
			new = &((*new)->rb_right);
	}

	rb_link_node(&r->addr_node, parent, new);
	rb_insert_color(&r->addr_node, &heap->addr_tree);
}

static int reserve_page_range(struct evl_heap *heap, size_t size)
{
	struct evl_heap_range *new, *splitr;

	/* Find a suitable range of pages covering 'size'. */
	new = search_size_ge(&heap->size_tree, size);
	if (new == NULL)
		return -1;

	rb_erase(&new->size_node, &heap->size_tree);
	if (new->size == size) {
		rb_erase(&new->addr_node, &heap->addr_tree);
		return addr_to_pagenr(heap, new);
	}

	/*
	 * The free range fetched is larger than what we need: split
	 * it in two, the upper part is returned to the caller, the
	 * lower part is sent back to the free list, which makes
	 * reindexing by address pointless.
	 */
	splitr = new;
	splitr->size -= size;
	new = (struct evl_heap_range *)((void *)new + splitr->size);
	insert_range_bysize(heap, splitr);

	return addr_to_pagenr(heap, new);
}

static void release_page_range(struct evl_heap *heap,
			void *page, size_t size)
{
	struct evl_heap_range *freed = page, *left, *right;
	bool addr_linked = false;

	freed->size = size;

	left = search_left_mergeable(heap, freed);
	if (left) {
		rb_erase(&left->size_node, &heap->size_tree);
		left->size += freed->size;
		freed = left;
		addr_linked = true;
	}

	right = search_right_mergeable(heap, freed);
	if (right) {
		rb_erase(&right->size_node, &heap->size_tree);
		freed->size += right->size;
		if (addr_linked)
			rb_erase(&right->addr_node, &heap->addr_tree);
		else
			rb_replace_node(&right->addr_node, &freed->addr_node,
					&heap->addr_tree);
	} else if (!addr_linked)
		insert_range_byaddr(heap, freed);

	insert_range_bysize(heap, freed);
	mark_pages(heap, addr_to_pagenr(heap, page),
		size >> EVL_HEAP_PAGE_SHIFT, page_free);
}

static void add_page_front(struct evl_heap *heap,
			int pg, int log2size)
{
	struct evl_heap_pgentry *new, *head, *next;
	int ilog;

	/* Insert page at front of the per-bucket page list. */

	ilog = log2size - EVL_HEAP_MIN_LOG2;
	new = &heap->pagemap[pg];
	if (heap->buckets[ilog] == -1U) {
		heap->buckets[ilog] = pg;
		new->prev = new->next = pg;
	} else {
		head = &heap->pagemap[heap->buckets[ilog]];
		new->prev = heap->buckets[ilog];
		new->next = head->next;
		next = &heap->pagemap[new->next];
		next->prev = pg;
		head->next = pg;
		heap->buckets[ilog] = pg;
	}
}

static void remove_page(struct evl_heap *heap,
			int pg, int log2size)
{
	struct evl_heap_pgentry *old, *prev, *next;
	int ilog = log2size - EVL_HEAP_MIN_LOG2;

	/* Remove page from the per-bucket page list. */

	old = &heap->pagemap[pg];
	if (pg == old->next)
		heap->buckets[ilog] = -1U;
	else {
		if (pg == heap->buckets[ilog])
			heap->buckets[ilog] = old->next;
		prev = &heap->pagemap[old->prev];
		prev->next = old->next;
		next = &heap->pagemap[old->next];
		next->prev = old->prev;
	}
}

static void move_page_front(struct evl_heap *heap,
			int pg, int log2size)
{
	int ilog = log2size - EVL_HEAP_MIN_LOG2;

	/* Move page at front of the per-bucket page list. */

	if (heap->buckets[ilog] == pg)
		return;	 /* Already at front, no move. */

	remove_page(heap, pg, log2size);
	add_page_front(heap, pg, log2size);
}

static void move_page_back(struct evl_heap *heap,
			int pg, int log2size)
{
	struct evl_heap_pgentry *old, *last, *head, *next;
	int ilog;

	/* Move page at end of the per-bucket page list. */

	old = &heap->pagemap[pg];
	if (pg == old->next) /* Singleton, no move. */
		return;

	remove_page(heap, pg, log2size);

	ilog = log2size - EVL_HEAP_MIN_LOG2;
	head = &heap->pagemap[heap->buckets[ilog]];
	last = &heap->pagemap[head->prev];
	old->prev = head->prev;
	old->next = last->next;
	next = &heap->pagemap[old->next];
	next->prev = pg;
	last->next = pg;
}

static void *add_free_range(struct evl_heap *heap,
			size_t bsize, int log2size)
{
	int pg;

	pg = reserve_page_range(heap, ALIGN(bsize, EVL_HEAP_PAGE_SIZE));
	if (pg < 0)
		return NULL;

	/*
	 * Update the page entry.  If @log2size is non-zero
	 * (i.e. bsize < EVL_HEAP_PAGE_SIZE), bsize is (1 << log2Size)
	 * between 2^EVL_HEAP_MIN_LOG2 and 2^(EVL_HEAP_PAGE_SHIFT -
	 * 1).  Save the log2 power into entry.type, then update the
	 * per-page allocation bitmap to reserve the first block.
	 *
	 * Otherwise, we have a larger block which may span multiple
	 * pages: set entry.type to page_list, indicating the start of
	 * the page range, and entry.bsize to the overall block size.
	 */
	if (log2size) {
		heap->pagemap[pg].type = log2size;
		/*
		 * Mark the first object slot (#0) as busy, along with
		 * the leftmost bits we won't use for this log2 size.
		 */
		heap->pagemap[pg].map = ~gen_block_mask(log2size) | 1;
		/*
		 * Insert the new page at front of the per-bucket page
		 * list, enforcing the assumption that pages with free
		 * space live close to the head of this list.
		 */
		add_page_front(heap, pg, log2size);
	} else {
		heap->pagemap[pg].type = page_list;
		heap->pagemap[pg].bsize = (u32)bsize;
		mark_pages(heap, pg + 1,
			(bsize >> EVL_HEAP_PAGE_SHIFT) - 1, page_cont);
	}

	heap->used_size += bsize;

	return pagenr_to_addr(heap, pg);
}

int evl_init_heap(struct evl_heap *heap, void *membase, size_t size)
{
	int n, nrpages;

	inband_context_only();

	if (size > EVL_HEAP_MAX_HEAPSZ || !PAGE_ALIGNED(size))
		return -EINVAL;

	/* Reset bucket page lists, all empty. */
	for (n = 0; n < EVL_HEAP_MAX_BUCKETS; n++)
		heap->buckets[n] = -1U;

	raw_spin_lock_init(&heap->lock);

	nrpages = size >> EVL_HEAP_PAGE_SHIFT;
	heap->pagemap = kzalloc(sizeof(struct evl_heap_pgentry) * nrpages,
				GFP_KERNEL);
	if (heap->pagemap == NULL)
		return -ENOMEM;

	heap->membase = membase;
	heap->usable_size = size;
	heap->used_size = 0;

	/*
	 * The free page pool is maintained as a set of ranges of
	 * contiguous pages indexed by address and size in rbtrees.
	 * Initially, we have a single range in those trees covering
	 * the whole memory we have been given for the heap. Over
	 * time, that range will be split then possibly re-merged back
	 * as allocations and deallocations take place.
	 */
	heap->size_tree = RB_ROOT;
	heap->addr_tree = RB_ROOT;
	release_page_range(heap, membase, size);

	return 0;
}
EXPORT_SYMBOL_GPL(evl_init_heap);

void evl_destroy_heap(struct evl_heap *heap)
{
	inband_context_only();

	kfree(heap->pagemap);
}
EXPORT_SYMBOL_GPL(evl_destroy_heap);

void *evl_alloc_chunk(struct evl_heap *heap, size_t size)
{
	int log2size, ilog, pg, b = -1;
	unsigned long flags;
	size_t bsize;
	void *block;

	if (size == 0)
		return NULL;

	if (size < EVL_HEAP_MIN_ALIGN) {
		bsize = size = EVL_HEAP_MIN_ALIGN;
		log2size = EVL_HEAP_MIN_LOG2;
	} else {
		log2size = ilog2(size);
		if (log2size < EVL_HEAP_PAGE_SHIFT) {
			if (size & (size - 1))
				log2size++;
			bsize = 1 << log2size;
		} else
			bsize = ALIGN(size, EVL_HEAP_PAGE_SIZE);
	}

	/*
	 * Allocate entire pages directly from the pool whenever the
	 * block is larger or equal to EVL_HEAP_PAGE_SIZE.  Otherwise,
	 * use bucketed memory.
	 *
	 * NOTE: Fully busy pages from bucketed memory are moved back
	 * at the end of the per-bucket page list, so that we may
	 * always assume that either the heading page has some room
	 * available, or no room is available from any page linked to
	 * this list, in which case we should immediately add a fresh
	 * page.
	 */
	raw_spin_lock_irqsave(&heap->lock, flags);

	if (bsize >= EVL_HEAP_PAGE_SIZE)
		/* Add a range of contiguous free pages. */
		block = add_free_range(heap, bsize, 0);
	else {
		ilog = log2size - EVL_HEAP_MIN_LOG2;
		EVL_WARN_ON(MEMORY, ilog < 0 || ilog >= EVL_HEAP_MAX_BUCKETS);
		pg = heap->buckets[ilog];
		/*
		 * Find a block in the heading page if any. If there
		 * is none, there won't be any down the list: add a
		 * new page right away.
		 */
		if (pg < 0 || heap->pagemap[pg].map == -1U)
			block = add_free_range(heap, bsize, log2size);
		else {
			b = ffs(~heap->pagemap[pg].map) - 1;
			/*
			 * Got one block from the heading per-bucket
			 * page, tag it as busy in the per-page
			 * allocation map.
			 */
			heap->pagemap[pg].map |= (1U << b);
			heap->used_size += bsize;
			block = heap->membase +
				(pg << EVL_HEAP_PAGE_SHIFT) +
				(b << log2size);
			if (heap->pagemap[pg].map == -1U)
				move_page_back(heap, pg, log2size);
		}
	}

	raw_spin_unlock_irqrestore(&heap->lock, flags);

	return block;
}
EXPORT_SYMBOL_GPL(evl_alloc_chunk);

void evl_free_chunk(struct evl_heap *heap, void *block)
{
	unsigned long pgoff, boff;
	int log2size, pg, n;
	unsigned long flags;
	size_t bsize;
	u32 oldmap;

	raw_spin_lock_irqsave(&heap->lock, flags);

	/* Compute the heading page number in the page map. */
	pgoff = block - heap->membase;
	pg = pgoff >> EVL_HEAP_PAGE_SHIFT;

	if (!page_is_valid(heap, pg))
		goto bad;

	switch (heap->pagemap[pg].type) {
	case page_list:
		bsize = heap->pagemap[pg].bsize;
		EVL_WARN_ON(MEMORY, (bsize & (EVL_HEAP_PAGE_SIZE - 1)) != 0);
		release_page_range(heap, pagenr_to_addr(heap, pg), bsize);
		break;

	default:
		log2size = heap->pagemap[pg].type;
		bsize = (1 << log2size);
		EVL_WARN_ON(MEMORY, bsize >= EVL_HEAP_PAGE_SIZE);
		boff = pgoff & ~EVL_HEAP_PAGE_MASK;
		if ((boff & (bsize - 1)) != 0) /* Not at block start? */
			goto bad;

		n = boff >> log2size; /* Block position in page. */
		oldmap = heap->pagemap[pg].map;
		heap->pagemap[pg].map &= ~(1U << n);

		/*
		 * If the page the block was sitting on is fully idle,
		 * return it to the pool. Otherwise, check whether
		 * that page is transitioning from fully busy to
		 * partially busy state, in which case it should move
		 * toward the front of the per-bucket page list.
		 */
		if (heap->pagemap[pg].map == ~gen_block_mask(log2size)) {
			remove_page(heap, pg, log2size);
			release_page_range(heap, pagenr_to_addr(heap, pg),
					EVL_HEAP_PAGE_SIZE);
		} else if (oldmap == -1U)
			move_page_front(heap, pg, log2size);
	}

	heap->used_size -= bsize;

	raw_spin_unlock_irqrestore(&heap->lock, flags);

	return;
bad:
	raw_spin_unlock_irqrestore(&heap->lock, flags);

	EVL_WARN(MEMORY, 1, "invalid block %p in heap %s",
		block, heap == &evl_shared_heap ?
		"shared" : "system");
}
EXPORT_SYMBOL_GPL(evl_free_chunk);

ssize_t evl_check_chunk(struct evl_heap *heap, void *block)
{
	unsigned long pg, pgoff, boff;
	ssize_t ret = -EINVAL;
	unsigned long flags;
	size_t bsize;

	raw_spin_lock_irqsave(&heap->lock, flags);

	/* Calculate the page number from the block address. */
	pgoff = block - heap->membase;
	pg = pgoff >> EVL_HEAP_PAGE_SHIFT;
	if (page_is_valid(heap, pg)) {
		if (heap->pagemap[pg].type == page_list)
			bsize = heap->pagemap[pg].bsize;
		else {
			bsize = (1 << heap->pagemap[pg].type);
			boff = pgoff & ~EVL_HEAP_PAGE_MASK;
			if ((boff & (bsize - 1)) != 0) /* Not at block start? */
				goto out;
		}
		ret = (ssize_t)bsize;
	}
out:
	raw_spin_unlock_irqrestore(&heap->lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(evl_check_chunk);

static int init_shared_heap(void)
{
	size_t size;
	void *mem;
	int ret;

	size = CONFIG_EVENLESS_NR_THREADS *
		sizeof(struct evl_user_window) +
		CONFIG_EVENLESS_NR_MONITORS *
		sizeof(struct evl_monitor_state) +
		CONFIG_EVENLESS_NR_SEMAPHORES *
		sizeof(struct evl_sem_state);
	size = PAGE_ALIGN(size);
	mem = kzalloc(size, GFP_KERNEL);
	if (mem == NULL)
		return -ENOMEM;

	ret = evl_init_heap(&evl_shared_heap, mem, size);
	if (ret) {
		kfree(mem);
		return ret;
	}

	evl_shm_size = size;

	return 0;
}

static void cleanup_shared_heap(void)
{
	void *membase = evl_get_heap_base(&evl_shared_heap);

	evl_destroy_heap(&evl_shared_heap);
	kfree(membase);
}

static int init_system_heap(void)
{
	size_t size = sysheap_size_arg;
	void *sysmem;
	int ret;

	if (size == 0)
		size = CONFIG_EVENLESS_SYS_HEAPSZ * 1024;

	sysmem = vmalloc(size);
	if (sysmem == NULL)
		return -ENOMEM;

	ret = evl_init_heap(&evl_system_heap, sysmem, size);
	if (ret) {
		vfree(sysmem);
		return -ENOMEM;
	}

	return 0;
}

static void cleanup_system_heap(void)
{
	void *membase = evl_get_heap_base(&evl_system_heap);

	evl_destroy_heap(&evl_system_heap);
	vfree(membase);
}

int __init evl_init_memory(void)
{
	int ret;

	ret = EVL_INIT_CALL(1, init_system_heap());
	if (ret)
		return ret;

	ret = EVL_INIT_CALL(1, init_shared_heap());
	if (ret) {
		cleanup_system_heap();
		return ret;
	}

	return 0;
}

void evl_cleanup_memory(void)
{
	cleanup_shared_heap();
	cleanup_system_heap();
}
