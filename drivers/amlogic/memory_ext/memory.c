// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/stddef.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/swap.h>
#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <linux/jiffies.h>
#include <linux/memblock.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/kasan.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/pagevec.h>
#include <linux/blkdev.h>
#include <linux/slab.h>
//include <linux/ratelimit.h>
//include <linux/oom.h>
//include <linux/topology.h>
//include <linux/sysctl.h>
//include <linux/cpu.h>
//include <linux/cpuset.h>
//include <linux/memory_hotplug.h>
//include <linux/nodemask.h>
//include <linux/vmalloc.h>
//include <linux/vmstat.h>
//include <linux/mempolicy.h>
//include <linux/memremap.h>
//include <linux/stop_machine.h>
//include <linux/random.h>
//include <linux/sort.h>
//include <linux/pfn.h>
//include <linux/backing-dev.h>
//include <linux/fault-inject.h>
//include <linux/page-isolation.h>
//include <linux/debugobjects.h>
//include <linux/kmemleak.h>
//include <linux/compaction.h>
//include <trace/events/kmem.h>
//include <trace/events/oom.h>
//include <linux/prefetch.h>
//include <linux/mm_inline.h>
//include <linux/migrate.h>
//include <linux/hugetlb.h>
//include <linux/sched/rt.h>
//include <linux/sched/mm.h>
//include <linux/page_owner.h>
//include <linux/kthread.h>
//include <linux/memcontrol.h>
//include <linux/ftrace.h>
//include <linux/lockdep.h>
//include <linux/nmi.h>
#include <../../mm/internal.h>

void should_wakeup_kswap(gfp_t gfp_mask, int order,
				       struct alloc_context *ac)
{
	unsigned long free_pages;
	struct zoneref *z = ac->preferred_zoneref;
	struct zone *zone;
	unsigned long high_wm;

	/*
	 * 1, if flag not allow reclaim
	 * 2, if with aotimic, we still need enable pre-wake up of
	 *    kswap to avoid large amount memory request fail in very
	 *    short time
	 */
	if (!(gfp_mask & __GFP_RECLAIM) && !(gfp_mask & __GFP_ATOMIC))
		return;

	for_next_zone_zonelist_nodemask(zone, z, ac->zonelist, ac->high_zoneidx,
								ac->nodemask) {
		free_pages = zone_page_state(zone, NR_FREE_PAGES);
	#ifdef CONFIG_AMLOGIC_CMA
		if (!can_use_cma(gfp_mask))
			free_pages -= zone_page_state(zone, NR_FREE_CMA_PAGES);
	#endif /* CONFIG_AMLOGIC_CMA */
		/*
		 * wake up kswapd before get pages from buddy, this help to
		 * fast reclaim process and can avoid memory become too low
		 * some times
		 */
		high_wm = high_wmark_pages(zone);
		if (gfp_mask & __GFP_HIGH) /* 1.5x if __GFP_HIGH */
			high_wm = ((high_wm * 3) / 2);
		if (free_pages <= high_wm)
			wakeup_kswapd(zone, gfp_mask, order, ac->high_zoneidx);
	}
}
EXPORT_SYMBOL(should_wakeup_kswap);

unsigned long aml_free_reserved_area(void *start, void *end, int poison, const char *s)
{
	return free_reserved_area(start, end, poison, s);
}
EXPORT_SYMBOL(aml_free_reserved_area);
