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
//nclude <linux/ratelimit.h>
//nclude <linux/oom.h>
//nclude <linux/topology.h>
//nclude <linux/sysctl.h>
//nclude <linux/cpu.h>
//nclude <linux/cpuset.h>
//nclude <linux/memory_hotplug.h>
//nclude <linux/nodemask.h>
//nclude <linux/vmalloc.h>
//nclude <linux/vmstat.h>
//nclude <linux/mempolicy.h>
//nclude <linux/memremap.h>
//nclude <linux/stop_machine.h>
//nclude <linux/random.h>
//nclude <linux/sort.h>
//nclude <linux/pfn.h>
//nclude <linux/backing-dev.h>
//nclude <linux/fault-inject.h>
//nclude <linux/page-isolation.h>
//nclude <linux/debugobjects.h>
//nclude <linux/kmemleak.h>
//nclude <linux/compaction.h>
//nclude <trace/events/kmem.h>
//nclude <trace/events/oom.h>
//nclude <linux/prefetch.h>
//nclude <linux/mm_inline.h>
//nclude <linux/migrate.h>
//nclude <linux/hugetlb.h>
//nclude <linux/sched/rt.h>
//nclude <linux/sched/mm.h>
//nclude <linux/page_owner.h>
//nclude <linux/kthread.h>
//nclude <linux/memcontrol.h>
//nclude <linux/ftrace.h>
//nclude <linux/lockdep.h>
//nclude <linux/nmi.h>
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

