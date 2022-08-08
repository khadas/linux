/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AMLOGIC_CMA_H__
#define __AMLOGIC_CMA_H__

#include <linux/migrate_mode.h>
#include <linux/pagemap.h>

enum migrate_type {
	COMPACT_NORMAL,
	COMPACT_CMA,
	COMPACT_TO_CMA,
};

#define GFP_NO_CMA    (__GFP_NO_CMA | __GFP_WRITE)

/*
 * Copy from mm/internal.h, must keep same as it
 * And we only add 2 new members for cma control
 */
struct compact_control {
	struct list_head freepages;	/* List of free pages to migrate to */
	struct list_head migratepages;	/* List of pages being migrated */
	unsigned int nr_freepages;	/* Number of isolated free pages */
	unsigned int nr_migratepages;	/* Number of pages to migrate */
	unsigned long free_pfn;		/* isolate_freepages search base */
	unsigned long migrate_pfn;	/* isolate_migratepages search base */
	unsigned long fast_start_pfn;	/* a pfn to start linear scan from */
	struct zone *zone;
	unsigned long total_migrate_scanned;
	unsigned long total_free_scanned;
	unsigned short fast_search_fail;/* failures to use free list searches */
	short search_order;		/* order to start a fast search at */
	const gfp_t gfp_mask;		/* gfp mask of a direct compactor */
	int order;			/* order a direct compactor needs */
	int migratetype;		/* migratetype of direct compactor */
	const unsigned int alloc_flags;	/* alloc flags of a direct compactor */
	const int classzone_idx;	/* zone index of a direct compactor */
	enum migrate_mode mode;		/* Async or sync migration mode */
	bool ignore_skip_hint;		/* Scan blocks even if marked skip */
	bool no_set_skip_hint;		/* Don't mark blocks for skipping */
	bool ignore_block_suitable;	/* Scan blocks considered unsuitable */
	bool direct_compaction;		/* False from kcompactd or /proc/... */
	bool whole_zone;		/* Whole zone should/has been scanned */
	bool contended;			/* Signal lock or sched contention */
	bool rescan;			/* Rescanning the same pageblock */

	/* add members */
	enum migrate_type page_type;
	bool forbid_to_cma;             /* Forbit to migrate to cma */
};

/*
 * Used in direct compaction when a page should be taken from the freelists
 * immediately when one is created during the free path.
 */
struct capture_control {
	struct compact_control *cc;
	struct page *page;
};

static inline bool cma_forbidden_mask(gfp_t gfp_flags)
{
	if ((gfp_flags & GFP_NO_CMA) || !(gfp_flags & __GFP_MOVABLE))
		return true;
	return false;
}

extern spinlock_t cma_iso_lock;
void cma_page_count_update(long size);
void aml_cma_alloc_pre_hook(int *a, int b);
void aml_cma_alloc_post_hook(int *a, int b, struct page *p);
void aml_cma_release_hook(int a, struct page *p);
struct page *get_cma_page(struct zone *zone, unsigned int order);
unsigned long compact_to_free_cma(struct zone *zone);
int cma_alloc_ref(void);
bool can_use_cma(gfp_t gfp_flags);
void get_cma_alloc_ref(void);
void put_cma_alloc_ref(void);
bool cma_page(struct page *page);
unsigned long get_cma_allocated(void);
unsigned long get_total_cmapages(void);
extern unsigned long ion_cma_allocated;
extern bool cma_first_wm_low;
extern bool no_filecache_in_cma;
extern int cma_debug_level;
int aml_cma_alloc_range(unsigned long start, unsigned long end);

void aml_cma_free(unsigned long pfn, unsigned int nr_pages);

unsigned long reclaim_clean_pages_from_list(struct zone *zone,
					    struct list_head *page_list);

void show_page(struct page *page);
unsigned long
isolate_freepages_range(struct compact_control *cc,
			unsigned long start_pfn, unsigned long end_pfn);
unsigned long
isolate_migratepages_range(struct compact_control *cc,
			   unsigned long low_pfn, unsigned long end_pfn);

struct page *compaction_cma_alloc(struct page *migratepage,
				  unsigned long data,
				  int **result);

#define cma_debug(l, p, format, args...)	\
	{								\
		if ((l) < cma_debug_level) {				\
			show_page(p);					\
			pr_info("%s,%d " format, __func__, __LINE__, ##args); \
		}							\
	}

#endif /* __AMLOGIC_CMA_H__ */
