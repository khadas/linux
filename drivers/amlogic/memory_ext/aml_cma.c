// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/memory_ext/aml_cma.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/stddef.h>
#include <linux/compiler.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/rmap.h>
#include <linux/kthread.h>
#include <linux/sched/rt.h>
#include <linux/completion.h>
#include <linux/module.h>
#include <linux/swap.h>
#include <linux/migrate.h>
#include <linux/cpu.h>
#include <linux/page-isolation.h>
#include <linux/spinlock_types.h>
#include <linux/amlogic/aml_cma.h>
#include <linux/sched/signal.h>
#include <linux/hugetlb.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/oom.h>
#include <linux/of.h>
#include <linux/shrinker.h>
#include <asm/system_misc.h>
#include <trace/events/page_isolation.h>
#ifdef CONFIG_AMLOGIC_PAGE_TRACE
#include <linux/amlogic/page_trace.h>
#endif /* CONFIG_AMLOGIC_PAGE_TRACE */

#define MAX_DEBUG_LEVEL		5

struct work_cma {
	struct list_head list;
	unsigned long pfn;
	unsigned long count;
	int ret;
};

struct cma_pcp {
	struct list_head list;
	struct completion start;
	struct completion end;
	spinlock_t  list_lock;		/* protect job list */
	int cpu;
};

static bool can_boost;
static DEFINE_PER_CPU(struct cma_pcp, cma_pcp_thread);
static struct proc_dir_entry *dentry;
int cma_debug_level;

DEFINE_SPINLOCK(cma_iso_lock);
static atomic_t cma_allocate;

int cma_alloc_ref(void)
{
	return atomic_read(&cma_allocate);
}
EXPORT_SYMBOL(cma_alloc_ref);

void get_cma_alloc_ref(void)
{
	atomic_inc(&cma_allocate);
}
EXPORT_SYMBOL(get_cma_alloc_ref);

void put_cma_alloc_ref(void)
{
	atomic_dec(&cma_allocate);
}
EXPORT_SYMBOL(put_cma_alloc_ref);

static __read_mostly unsigned long total_cma_pages;
static atomic_long_t nr_cma_allocated;
unsigned long get_cma_allocated(void)
{
	return atomic_long_read(&nr_cma_allocated);
}
EXPORT_SYMBOL(get_cma_allocated);

unsigned long get_total_cmapages(void)
{
	return total_cma_pages;
}
EXPORT_SYMBOL(get_total_cmapages);

void cma_page_count_update(long diff)
{
	total_cma_pages += diff / PAGE_SIZE;
}
EXPORT_SYMBOL(cma_page_count_update);

#define RESTRIC_ANON	0
#define ANON_RATIO	60
bool cma_first_wm_low __read_mostly;

bool can_use_cma(gfp_t gfp_flags)
{
#if RESTRIC_ANON
	unsigned long anon_cma;
#endif /* RESTRIC_ANON */

	if (unlikely(!cma_first_wm_low))
		return false;

	if (cma_forbidden_mask(gfp_flags))
		return false;

	if (task_nice(current) > 0)
		return false;

#if RESTRIC_ANON
	/*
	 * calculate if there are enough space for anon_cma
	 */
	if (!(gfp_flags & __GFP_COLD)) {
		anon_cma = global_zone_page_state(NR_INACTIVE_ANON_CMA) +
			   global_zone_page_state(NR_ACTIVE_ANON_CMA);
		if (anon_cma * 100 > total_cma_pages * ANON_RATIO)
			return false;
	}
#endif /* RESTRIC_ANON */

	return true;
}
EXPORT_SYMBOL(can_use_cma);

bool cma_page(struct page *page)
{
	int migrate_type = 0;

	if (!page)
		return false;
	migrate_type = get_pageblock_migratetype(page);
	if (is_migrate_cma(migrate_type) ||
	    is_migrate_isolate(migrate_type)) {
		return true;
	}
	return false;
}
EXPORT_SYMBOL(cma_page);

#ifdef CONFIG_AMLOGIC_PAGE_TRACE
static void update_cma_page_trace(struct page *page, unsigned long cnt)
{
	long i;
	unsigned long fun;

	if (!page)
		return;

	fun = find_back_trace();
	if (cma_alloc_trace)
		pr_info("%s alloc page:%lx, count:%ld, func:%ps\n", __func__,
			page_to_pfn(page), cnt, (void *)fun);
	for (i = 0; i < cnt; i++) {
		set_page_trace(page, 0, __GFP_NO_CMA, (void *)fun);
		page++;
	}
}
#endif /* CONFIG_AMLOGIC_PAGE_TRACE */

void aml_cma_alloc_pre_hook(int *dummy, int count)
{
	get_cma_alloc_ref();

	/* temperary increase task priority if allocate many pages */
	*dummy = task_nice(current);
	if (count >= (pageblock_nr_pages / 2))
		set_user_nice(current, -18);
}
EXPORT_SYMBOL(aml_cma_alloc_pre_hook);

void aml_cma_alloc_post_hook(int *dummy, int count, struct page *page)
{
	put_cma_alloc_ref();
	if (page)
		atomic_long_add(count, &nr_cma_allocated);
	if (count >= (pageblock_nr_pages / 2))
		set_user_nice(current, *dummy);
#ifdef CONFIG_AMLOGIC_PAGE_TRACE
	update_cma_page_trace(page, count);
#endif /* CONFIG_AMLOGIC_PAGE_TRACE */
}
EXPORT_SYMBOL(aml_cma_alloc_post_hook);

void aml_cma_release_hook(int count, struct page *pages)
{
#ifdef CONFIG_AMLOGIC_PAGE_TRACE
	if (cma_alloc_trace)
		pr_info("%s free page:%lx, count:%d, func:%ps\n", __func__,
			page_to_pfn(pages), count, (void *)find_back_trace());
#endif /* CONFIG_AMLOGIC_PAGE_TRACE */
	atomic_long_sub(count, &nr_cma_allocated);
}
EXPORT_SYMBOL(aml_cma_release_hook);

static unsigned long get_align_pfn_low(unsigned long pfn)
{
	return pfn & ~(max_t(unsigned long, MAX_ORDER_NR_PAGES,
			     pageblock_nr_pages) - 1);
}

static unsigned long get_align_pfn_high(unsigned long pfn)
{
	return ALIGN(pfn, max_t(unsigned long, MAX_ORDER_NR_PAGES,
				pageblock_nr_pages));
}

static struct page *get_migrate_page(struct page *page, unsigned long private)
{
	gfp_t gfp_mask = GFP_USER | __GFP_MOVABLE;
	struct page *new = NULL;
#ifdef CONFIG_AMLOGIC_PAGE_TRACE
	struct page_trace *old_trace, *new_trace;
#endif

	/*
	 * TODO: allocate a destination hugepage from a nearest neighbor node,
	 * accordance with memory policy of the user process if possible. For
	 * now as a simple work-around, we use the next node for destination.
	 */
	if (PageHuge(page)) {
		new = alloc_huge_page_node(page_hstate(compound_head(page)),
					   next_node_in(page_to_nid(page),
							node_online_map));
	#ifdef CONFIG_AMLOGIC_PAGE_TRACE
	#ifdef CONFIG_HUGETLB_PAGE
		if (new) {
			old_trace = find_page_base(page);
			new_trace = find_page_base(new);
			*new_trace = *old_trace;
		}
	#endif
	#endif
		return new;
	}

	if (PageHighMem(page))
		gfp_mask |= __GFP_HIGHMEM;

	new = alloc_page(gfp_mask);
#ifdef CONFIG_AMLOGIC_PAGE_TRACE
	if (new) {
		old_trace = find_page_base(page);
		new_trace = find_page_base(new);
		*new_trace = *old_trace;
	}
#endif
	return new;
}

/* [start, end) must belong to a single zone. */
static int aml_alloc_contig_migrate_range(struct compact_control *cc,
					  unsigned long start,
					  unsigned long end, bool boost)
{
	/* This function is based on compact_zone() from compaction.c. */
	unsigned long nr_reclaimed;
	unsigned long pfn = start;
	unsigned int tries = 0;
	int ret = 0;

	if (!boost)
		migrate_prep();

	while (pfn < end || !list_empty(&cc->migratepages)) {
		if (fatal_signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		if (list_empty(&cc->migratepages)) {
			cc->nr_migratepages = 0;
			pfn = isolate_migratepages_range(cc, pfn, end);
			if (!pfn) {
				ret = -EINTR;
				cma_debug(1, NULL, " iso migrate page fail\n");
				break;
			}
			tries = 0;
		} else if (++tries == 5) {
			ret = ret < 0 ? ret : -EBUSY;
			break;
		}

		nr_reclaimed = reclaim_clean_pages_from_list(cc->zone,
							     &cc->migratepages);
		cc->nr_migratepages -= nr_reclaimed;

		ret = migrate_pages(&cc->migratepages, get_migrate_page,
				    NULL, 0, cc->mode, MR_CONTIG_RANGE);
	}
	if (ret < 0) {
		putback_movable_pages(&cc->migratepages);
		return ret;
	}
	return 0;
}

static int cma_boost_work_func(void *cma_data)
{
	struct cma_pcp *c_work;
	struct work_cma *job;
	unsigned long pfn, end;
	int ret = -1;
	int this_cpu;
	struct compact_control cc = {
		.nr_migratepages = 0,
		.order = -1,
		.mode = MIGRATE_SYNC,
		.ignore_skip_hint = true,
		.no_set_skip_hint = true,
		.gfp_mask = GFP_KERNEL,
		.page_type = COMPACT_CMA,
	};

	c_work  = (struct cma_pcp *)cma_data;
	for (;;) {
		ret = wait_for_completion_interruptible(&c_work->start);
		if (ret < 0) {
			pr_err("%s wait for task %d is %d\n",
			       __func__, c_work->cpu, ret);
			continue;
		}
		this_cpu = get_cpu();
		put_cpu();
		if (this_cpu != c_work->cpu) {
			pr_err("%s, cpu %d is not work cpu:%d\n",
			       __func__, this_cpu, c_work->cpu);
		}
		spin_lock(&c_work->list_lock);
		if (list_empty(&c_work->list)) {
			/* NO job todo ? */
			pr_err("%s,%d, list empty\n", __func__, __LINE__);
			spin_unlock(&c_work->list_lock);
			goto next;
		}
		job = list_first_entry(&c_work->list, struct work_cma, list);
		list_del(&job->list);
		spin_unlock(&c_work->list_lock);

		INIT_LIST_HEAD(&cc.migratepages);
		lru_add_drain();
		pfn      = job->pfn;
		cc.zone  = page_zone(pfn_to_page(pfn));
		end      = pfn + job->count;
		ret      = aml_alloc_contig_migrate_range(&cc, pfn, end, 1);
		job->ret = ret;
		if (!ret) {
			lru_add_drain();
			drain_local_pages(NULL);
		}
		if (ret)
			cma_debug(1, NULL, "failed, ret:%d\n", ret);
next:
		complete(&c_work->end);
		if (kthread_should_stop()) {
			pr_err("%s task exit\n", __func__);
			break;
		}
	}
	return 0;
}

static int __init init_cma_boost_task(void)
{
	int cpu;
	struct task_struct *task;
	struct cma_pcp *work;
	char task_name[20] = {};

	for_each_possible_cpu(cpu) {
		memset(task_name, 0, sizeof(task_name));
		sprintf(task_name, "cma_task%d", cpu);
		work = &per_cpu(cma_pcp_thread, cpu);
		init_completion(&work->start);
		init_completion(&work->end);
		INIT_LIST_HEAD(&work->list);
		spin_lock_init(&work->list_lock);
		work->cpu = cpu;
		task = kthread_create(cma_boost_work_func, work, task_name);
		if (!IS_ERR(task)) {
			kthread_bind(task, cpu);
			set_user_nice(task, -17);
			pr_debug("create cma task%p, for cpu %d\n", task, cpu);
			wake_up_process(task);
		} else {
			can_boost = 0;
			pr_err("create task for cpu %d fail:%p\n", cpu, task);
			return -1;
		}
	}
	can_boost = 1;
	return 0;
}
module_init(init_cma_boost_task);

int cma_alloc_contig_boost(unsigned long start_pfn, unsigned long count)
{
	struct cpumask has_work;
	int cpu, cpus, i = 0, ret = 0, ebusy = 0, einv = 0;
	atomic_t ok;
	unsigned long delta;
	unsigned long cnt;
	unsigned long flags;
	struct cma_pcp *work;
	struct work_cma job[NR_CPUS] = {};

	cpumask_clear(&has_work);

	cpus  = num_online_cpus();
	cnt   = count;
	delta = count / cpus;
	atomic_set(&ok, 0);
	local_irq_save(flags);
	for_each_online_cpu(cpu) {
		work = &per_cpu(cma_pcp_thread, cpu);
		spin_lock(&work->list_lock);
		INIT_LIST_HEAD(&job[cpu].list);
		job[cpu].pfn   = start_pfn + i * delta;
		job[cpu].count = delta;
		job[cpu].ret   = -1;
		if (i == cpus - 1)
			job[cpu].count = count - i * delta;
		cpumask_set_cpu(cpu, &has_work);
		list_add(&job[cpu].list, &work->list);
		spin_unlock(&work->list_lock);
		complete(&work->start);
		i++;
	}
	local_irq_restore(flags);

	for_each_cpu(cpu, &has_work) {
		work = &per_cpu(cma_pcp_thread, cpu);
		wait_for_completion(&work->end);
		if (job[cpu].ret) {
			if (job[cpu].ret != -EBUSY)
				einv++;
			else
				ebusy++;
		}
	}

	if (einv)
		ret = -EINVAL;
	else if (ebusy)
		ret = -EBUSY;
	else
		ret = 0;

	if (ret < 0 && ret != -EBUSY) {
		pr_err("%s, failed, ret:%d, ok:%d\n",
		       __func__, ret, atomic_read(&ok));
	}

	return ret;
}

static int __aml_check_pageblock_isolate(unsigned long pfn,
					 unsigned long end_pfn,
					 bool skip_hwpoisoned_pages)
{
	struct page *page;

	while (pfn < end_pfn) {
		if (!pfn_valid_within(pfn)) {
			pfn++;
			continue;
		}
		page = pfn_to_page(pfn);
		if (PageBuddy(page)) {
			/*
			 * If the page is on a free list, it has to be on
			 * the correct MIGRATE_ISOLATE freelist. There is no
			 * simple way to verify that as VM_BUG_ON(), though.
			 */
			pfn += 1 << page_private(page);
		} else if (skip_hwpoisoned_pages && PageHWPoison(page)) {
			/*
			 * The HWPoisoned page may be not in buddy
			 * system, and page_count() is not 0.
			 */
			pfn++;
		} else {
			cma_debug(1, page, " isolate failed\n");
			break;
		}
	}
	return pfn;
}

static inline struct page *
check_page_valid(unsigned long pfn, unsigned long nr_pages)
{
	int i;

	for (i = 0; i < nr_pages; i++) {
		struct page *page;

		page = pfn_to_online_page(pfn + i);
		if (!page)
			continue;
		return page;
	}
	return NULL;
}

int aml_check_pages_isolated(unsigned long start_pfn, unsigned long end_pfn,
			     bool skip_hwpoisoned_pages)
{
	unsigned long pfn, flags;
	struct page *page;
	struct zone *zone;

	/*
	 * Note: pageblock_nr_pages != MAX_ORDER. Then, chunks of free pages
	 * are not aligned to pageblock_nr_pages.
	 * Then we just check migratetype first.
	 */
	for (pfn = start_pfn; pfn < end_pfn; pfn += pageblock_nr_pages) {
		page = check_page_valid(pfn, pageblock_nr_pages);
		if (page && get_pageblock_migratetype(page) != MIGRATE_ISOLATE)
			break;
	}
	page = check_page_valid(start_pfn, end_pfn - start_pfn);
	if (pfn < end_pfn || !page)
		return -EBUSY;
	/* Check all pages are free or marked as ISOLATED */
	zone = page_zone(page);
	spin_lock_irqsave(&zone->lock, flags);
	pfn = __aml_check_pageblock_isolate(start_pfn, end_pfn,
					    skip_hwpoisoned_pages);
	spin_unlock_irqrestore(&zone->lock, flags);

	trace_test_pages_isolated(start_pfn, end_pfn, pfn);

	return pfn < end_pfn ? -EBUSY : 0;
}

int aml_cma_alloc_range(unsigned long start, unsigned long end)
{
	unsigned long outer_start, outer_end;
	int ret = 0, order;
	int try_times = 0;
	int boost_ok = 0;

	struct compact_control cc = {
		.nr_migratepages = 0,
		.order = -1,
		.zone = page_zone(pfn_to_page(start)),
		.mode = MIGRATE_SYNC,
		.ignore_skip_hint = true,
		.no_set_skip_hint = true,
		.gfp_mask = GFP_KERNEL,
		.page_type = COMPACT_CMA,
	};
	INIT_LIST_HEAD(&cc.migratepages);

	cma_debug(0, NULL, " range [%lx-%lx]\n", start, end);
	ret = start_isolate_page_range(get_align_pfn_low(start),
				       get_align_pfn_high(end), MIGRATE_CMA,
				       false);
	if (ret < 0) {
		cma_debug(1, NULL, "ret:%d\n", ret);
		return ret;
	}

try_again:
	/*
	 * try to use more cpu to do this job when alloc count is large
	 */
	if ((num_online_cpus() > 1) && can_boost &&
	    ((end - start) >= pageblock_nr_pages / 2)) {
		get_online_cpus();
		ret = cma_alloc_contig_boost(start, end - start);
		put_online_cpus();
		boost_ok = !ret ? 1 : 0;
	} else {
		ret = aml_alloc_contig_migrate_range(&cc, start, end, 0);
	}

	if (ret && ret != -EBUSY) {
		cma_debug(1, NULL, "ret:%d\n", ret);
		goto done;
	}

	ret = 0;
	if (!boost_ok) {
		lru_add_drain_all();
		drain_all_pages(cc.zone);
	}
	order = 0;
	outer_start = start;
	while (!PageBuddy(pfn_to_page(outer_start))) {
		if (++order >= MAX_ORDER) {
			outer_start = start;
			break;
		}
		outer_start &= ~0UL << order;
	}

	if (outer_start != start) {
		order = page_private(pfn_to_page(outer_start)); /* page order */

		/*
		 * outer_start page could be small order buddy page and
		 * it doesn't include start page. Adjust outer_start
		 * in this case to report failed page properly
		 * on tracepoint in test_pages_isolated()
		 */
		if (outer_start + (1UL << order) <= start)
			outer_start = start;
	}

	/* Make sure the range is really isolated. */
	if (aml_check_pages_isolated(outer_start, end, false)) {
		cma_debug(1, NULL, "check page isolate(%lx, %lx) failed\n",
			  outer_start, end);
		try_times++;
		if (try_times < 10)
			goto try_again;
		ret = -EBUSY;
		goto done;
	}

	/* Grab isolated pages from freelists. */
	outer_end = isolate_freepages_range(&cc, outer_start, end);
	if (!outer_end) {
		ret = -EBUSY;
		cma_debug(1, NULL, "iso free range(%lx, %lx) failed\n",
			  outer_start, end);
		goto done;
	}

	/* Free head and tail (if any) */
	if (start != outer_start)
		aml_cma_free(outer_start, start - outer_start);
	if (end != outer_end)
		aml_cma_free(end, outer_end - end);

done:
	undo_isolate_page_range(get_align_pfn_low(start),
				get_align_pfn_high(end), MIGRATE_CMA);
	return ret;
}
EXPORT_SYMBOL(aml_cma_alloc_range);

static int __aml_cma_free_check(struct page *page, int order, unsigned int *cnt)
{
	int i;
	int ref = 0;

	/*
	 * clear ref count, head page should avoid this operation.
	 * ref count of head page will be cleared when __free_pages
	 * is called.
	 */
	for (i = 1; i < (1 << order); i++) {
		if (!put_page_testzero(page + i))
			ref++;
	}
	if (ref) {
		pr_info("%s, %d pages are still in use\n", __func__, ref);
		*cnt += ref;
		return -1;
	}
	return 0;
}

static int aml_cma_get_page_order(unsigned long pfn)
{
	int i, mask = 1;

	for (i = 0; i < (MAX_ORDER - 1); i++) {
		if (pfn & (mask << i))
			break;
	}

	return i;
}

void aml_cma_free(unsigned long pfn, unsigned int nr_pages)
{
	unsigned int count = 0;
	struct page *page;
	int free_order, start_order = 0;
	int batch;

	while (nr_pages) {
		page = pfn_to_page(pfn);
		free_order = aml_cma_get_page_order(pfn);
		if (nr_pages >= (1 << free_order)) {
			start_order = free_order;
		} else {
			/* remain pages is not enough */
			start_order = 0;
			while (nr_pages >= (1 << start_order))
				start_order++;
			start_order--;
		}
		batch = (1 << start_order);
		if (__aml_cma_free_check(page, start_order, &count))
			break;
		__free_pages(page, start_order);
		pr_debug("pages:%4d, free:%2d, start:%2d, batch:%4d, pfn:%lx\n",
			 nr_pages, free_order,
			 start_order, batch, pfn);
		nr_pages -= batch;
		pfn += batch;
	}
	WARN(count != 0, "%d pages are still in use!\n", count);
}
EXPORT_SYMBOL(aml_cma_free);

static bool cma_vma_show(struct page *page, struct vm_area_struct *vma,
			 unsigned long addr, void *arg)
{
#ifdef CONFIG_AMLOGIC_USER_FAULT
	struct mm_struct *mm = vma->vm_mm;

	show_vma(mm, addr);
#endif
	return true; /* keep loop */
}

void rmap_walk_vma(struct page *page)
{
	struct rmap_walk_control rwc = {
		.rmap_one = cma_vma_show,
	};

	pr_info("%s, show map for page:%lx,f:%lx, m:%p, p:%d\n",
		__func__, page_to_pfn(page), page->flags,
		page->mapping, page_count(page));
	if (!page_mapping(page))
		return;
	rmap_walk(page, &rwc);
}

void show_page(struct page *page)
{
	unsigned long trace = 0;
	unsigned long map_flag = -1UL;

	if (!page)
		return;
#ifdef CONFIG_AMLOGIC_PAGE_TRACE
	trace = get_page_trace(page);
#endif
	if (page->mapping && !((unsigned long)page->mapping & 0x3))
		map_flag = page->mapping->flags;
	pr_info("page:%lx, map:%lx, mf:%lx, pf:%lx, m:%d, c:%d, f:%ps\n",
		page_to_pfn(page), (unsigned long)page->mapping, map_flag,
		page->flags & 0xffffffff,
		page_mapcount(page), page_count(page),
		(void *)trace);
	if (cma_debug_level > 4)
		rmap_walk_vma(page);
}

static int cma_debug_show(struct seq_file *m, void *arg)
{
	seq_printf(m, "level=%d\n", cma_debug_level);
	return 0;
}

static ssize_t cma_debug_write(struct file *file, const char __user *buffer,
			       size_t count, loff_t *ppos)
{
	int arg = 0;

	if (kstrtoint_from_user(buffer, count, 10, &arg))
		return -EINVAL;

	if (arg > MAX_DEBUG_LEVEL)
		return -EINVAL;

	cma_debug_level = arg;
	return count;
}

static int cma_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, cma_debug_show, NULL);
}

static const struct file_operations cma_dbg_file_ops = {
	.open		= cma_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.write		= cma_debug_write,
	.release	= single_release,
};

static int __init aml_cma_init(void)
{
	atomic_set(&cma_allocate, 0);
	atomic_long_set(&nr_cma_allocated, 0);

	dentry = proc_create("cma_debug", 0644, NULL, &cma_dbg_file_ops);
	if (IS_ERR_OR_NULL(dentry)) {
		pr_err("%s, create sysfs failed\n", __func__);
		return -1;
	}

	return 0;
}
arch_initcall(aml_cma_init);

/*----------------- for cma shrinker -----------------------*/
#define PARA_COUNT		6
struct cma_shrinker {
	unsigned int adj[PARA_COUNT];
	unsigned int free[PARA_COUNT];
	unsigned long shrink_timeout;
	unsigned long foreground_timeout;
};

struct cma_shrinker *cs;

static unsigned long cma_shrinker_count(struct shrinker *s,
					struct shrink_control *sc)
{
	return  global_node_page_state(NR_ACTIVE_ANON) +
		global_node_page_state(NR_ACTIVE_FILE) +
		global_node_page_state(NR_INACTIVE_ANON) +
		global_node_page_state(NR_INACTIVE_FILE);
}

static void show_task_adj(void)
{
#define SHOW_PRIFIX	"score_adj:%5d, rss:%5lu"
	struct task_struct *tsk;
	int tasksize;

	/* avoid print too many */
	if (time_after(cs->foreground_timeout, jiffies))
		return;

	cs->foreground_timeout = jiffies + HZ * 5;
	show_mem(0, NULL);
	pr_emerg("Foreground task killed, show all Candidates\n");
	for_each_process(tsk) {
		struct task_struct *p;
		short oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;
		p = find_lock_task_mm(tsk);
		if (!p)
			continue;
		oom_score_adj = p->signal->oom_score_adj;
		tasksize = get_mm_rss(p->mm);
		task_unlock(p);
	#ifdef CONFIG_ZRAM
		pr_emerg(SHOW_PRIFIX ", rswap:%5lu, task:%5d, %s\n",
			 oom_score_adj, get_mm_rss(p->mm),
			 get_mm_counter(p->mm, MM_SWAPENTS),
			 p->pid, p->comm);
	#else
		pr_emerg(SHOW_PRIFIX ", task:%5d, %s\n",
			 oom_score_adj, get_mm_rss(p->mm),
			 p->pid, p->comm);
	#endif /* CONFIG_ZRAM */
	}
}

static int swapcache_low(void)
{
	unsigned long free_swap;
	unsigned long total_swap;

	free_swap  = get_nr_swap_pages();
	total_swap = total_swap_pages;
	if ((free_swap <= (total_swap / 8)) && total_swap) {
		/* free swap < 1/8 total */
		pr_debug("swap free is low, free:%ld, total:%ld\n",
			 free_swap, total_swap);
		return 1;
	}
	return 0;
}

static unsigned long cma_shrinker_scan(struct shrinker *s,
				       struct shrink_control *sc)
{
	struct task_struct *tsk;
	struct task_struct *selected = NULL;
	unsigned long rem = 0;
	int tasksize;
	int taskswap = 0;
	int i;
	short min_score_adj = OOM_SCORE_ADJ_MAX + 1;
	int minfree = 0;
	int selected_tasksize = 0;
	short selected_oom_score_adj;
	int other_free = global_zone_page_state(NR_FREE_PAGES) - totalreserve_pages;
	int other_file = global_node_page_state(NR_FILE_PAGES) -
			 global_node_page_state(NR_SHMEM) -
			 global_node_page_state(NR_UNEVICTABLE) -
			 total_swapcache_pages();
	int free_cma   = 0;
	int file_cma   = 0;
	int swap_low   = 0;
	int cache_low  = 0;
	int last_idx   = PARA_COUNT - 1;
	int selected_taskswap = 0;

	swap_low = swapcache_low();
	if ((!cma_forbidden_mask(sc->gfp_mask) || current_is_kswapd()) &&
	    !swap_low)
		return 0;

	free_cma    = global_zone_page_state(NR_FREE_CMA_PAGES);
	file_cma    = global_zone_page_state(NR_INACTIVE_FILE_CMA) +
		      global_zone_page_state(NR_ACTIVE_FILE_CMA);
	other_free -= free_cma;
	other_file -= file_cma;

	for (i = 0; i < PARA_COUNT; i++) {
		minfree = cs->free[i];
		if (other_free < minfree && other_file < minfree) {
			min_score_adj = cs->adj[i];
			cache_low = 1;
			break;
		}
	}

	pr_debug("%s %lu, %x, ofree %d %d, ma %hd\n", __func__,
		 sc->nr_to_scan, sc->gfp_mask, other_free,
		 other_file, min_score_adj);

	if (!cache_low) {
		if (swap_low) {
			/* kill from last prio task */
			min_score_adj = cs->adj[last_idx];
		} else {
			/* nothing to do */
			return 0;
		}
	}

retry:
	selected_oom_score_adj = min_score_adj;
	pr_debug("%s, cachelow:%d, swaplow:%d, adj:%d\n",
		 __func__, cache_low, swap_low, min_score_adj);
	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		short oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		if (task_lmk_waiting(p) &&
		    time_before_eq(jiffies, cs->shrink_timeout)) {
			task_unlock(p);
			rcu_read_unlock();
			return 0;
		}
		oom_score_adj = p->signal->oom_score_adj;
		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}
		tasksize = get_mm_rss(p->mm);
		if (swap_low && !cache_low)
			taskswap = get_mm_counter(p->mm, MM_SWAPENTS);

		task_unlock(p);
		if (tasksize <= 0)
			continue;
		if (swap_low && !cache_low && !taskswap) {
			/* we need free swap but this task don't have swap */
			continue;
		}
		if (selected) {
			if (oom_score_adj < selected_oom_score_adj)
				continue;

			if (swap_low && !cache_low &&
			    taskswap < selected_taskswap)
				continue;

			if (oom_score_adj == selected_oom_score_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		if (!strcmp(p->comm, "k.glbenchmark27"))
			continue;

		selected = p;
		selected_taskswap = taskswap;
		selected_tasksize = tasksize;
		selected_oom_score_adj = oom_score_adj;
		pr_debug("select '%s' (%d), adj %hd, size %d, to kill\n",
			 p->comm, p->pid, oom_score_adj, tasksize);
	}
	/* we try to kill somebody if swap is near full
	 * but too many filecache, this is a coner case
	 */
	if (!selected && !cache_low && swap_low) {
		last_idx--;
		if (last_idx > 0) {	/* don't kill forground */
			min_score_adj = cs->adj[last_idx];
			rcu_read_unlock();
			goto retry;
		} else {
			/* swap full but no one killed */
			show_task_adj();
		}
	}
	if (selected) {
		long cache_size = other_file * (long)(PAGE_SIZE / 1024);
		long cache_limit = minfree * (long)(PAGE_SIZE / 1024);
		long free = other_free * (long)(PAGE_SIZE / 1024);

		if (swap_low && !cache_low) {
			pr_emerg("  Free Swap:%ld kB, Total Swap:%ld kB\n",
				 get_nr_swap_pages() << (PAGE_SHIFT - 10),
				 total_swap_pages << (PAGE_SHIFT - 10));
			pr_emerg("  Task swap:%d, idx:%d\n",
				 selected_taskswap << (PAGE_SHIFT - 10),
				 last_idx);
		}
		task_lock(selected);
		send_sig(SIGKILL, selected, 0);
		if (selected->mm)
			task_set_lmk_waiting(selected);
		task_unlock(selected);
		cs->shrink_timeout = jiffies + HZ / 2;
		pr_emerg("Killing '%s' (%d) (tgid %d), adj %hd,\n"
			 "   to free %ldkB on behalf of '%s' (%d) because\n"
			 "   cache %ldkB is below %ldkB for oom_score_adj %hd\n"
			 "   Free memory is %ldkB above reserved\n",
			 selected->comm, selected->pid, selected->tgid,
			 selected_oom_score_adj,
			 selected_tasksize * (long)(PAGE_SIZE / 1024),
			 current->comm, current->pid,
			 cache_size, cache_limit,
			 min_score_adj,
			 free);
		/* kill quickly if can't use cma */
		pr_info("   Free cma:%ldkB, file cma:%ldkB, Global free:%ldkB\n",
			free_cma * (long)(PAGE_SIZE / 1024),
			file_cma * (long)(PAGE_SIZE / 1024),
			global_zone_page_state(NR_FREE_PAGES));
		rem += selected_tasksize;
		if (!selected_oom_score_adj) /* forgeround task killed */
			show_task_adj();
	}

	pr_debug("%s %lu, %x, return %lu\n", __func__,
		 sc->nr_to_scan, sc->gfp_mask, rem);
	rcu_read_unlock();
	return rem;
}

static struct shrinker cma_shrinkers = {
	.scan_objects  = cma_shrinker_scan,
	.count_objects = cma_shrinker_count,
	.seeks         = DEFAULT_SEEKS * 16
};

static ssize_t adj_store(struct class *cla,
			 struct class_attribute *attr,
			 const char *buf, size_t count)
{
	int i;
	int adj[6];

	i = sscanf(buf, "%d,%d,%d,%d,%d,%d",
		   &adj[0], &adj[1], &adj[2],
		   &adj[3], &adj[4], &adj[5]);
	if (i != PARA_COUNT) {
		pr_err("invalid input:%s\n", buf);
		return count;
	}

	memcpy(cs->adj, adj, sizeof(adj));
	return count;
}

static ssize_t adj_show(struct class *cla,
			struct class_attribute *attr, char *buf)
{
	int i, sz = 0;

	for (i = 0; i < PARA_COUNT; i++)
		sz += sprintf(buf + sz, "%d ", cs->adj[i]);
	sz += sprintf(buf + sz, "\n");
	return sz;
}

static ssize_t free_store(struct class *cla,
			  struct class_attribute *attr,
			  const char *buf, size_t count)
{
	int i;
	int free[6];

	i = sscanf(buf, "%d,%d,%d,%d,%d,%d",
		   &free[0], &free[1], &free[2],
		   &free[3], &free[4], &free[5]);
	if (i != PARA_COUNT) {
		pr_err("invalid input:%s\n", buf);
		return count;
	}

	memcpy(cs->free, free, sizeof(free));
	return count;
}

static ssize_t free_show(struct class *cla,
			 struct class_attribute *attr, char *buf)
{
	int i, sz = 0;

	for (i = 0; i < PARA_COUNT; i++)
		sz += sprintf(buf + sz, "%d ", cs->free[i]);
	sz += sprintf(buf + sz, "\n");
	return sz;
}

static CLASS_ATTR_RW(adj);
static CLASS_ATTR_RW(free);

static struct attribute *cma_shrinker_attrs[] = {
	&class_attr_adj.attr,
	&class_attr_free.attr,
	NULL
};

ATTRIBUTE_GROUPS(cma_shrinker);

static struct class cma_shrinker_class = {
		.name = "cma_shrinker",
		.class_groups = cma_shrinker_groups,
};

static int cma_shrinker_probe(struct platform_device *pdev)
{
	struct cma_shrinker *p;
	struct device_node *np;
	int ret, i;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	cs = p;
	np = pdev->dev.of_node;
	ret = of_property_read_u32_array(np, "adj", cs->adj, PARA_COUNT);
	if (ret < 0)
		goto err;

	ret = of_property_read_u32_array(np, "free", cs->free, PARA_COUNT);
	if (ret < 0)
		goto err;

	ret = class_register(&cma_shrinker_class);
	if (ret)
		goto err;

	if (register_shrinker(&cma_shrinkers))
		goto err;

	for (i = 0; i < PARA_COUNT; i++) {
		pr_debug("cma shrinker, adj:%3d, free:%d\n",
			cs->adj[i], cs->free[i]);
	}
	return 0;

err:
	kfree(p);
	cs = NULL;
	return -EINVAL;
}

static int cma_shrinker_remove(struct platform_device *pdev)
{
	if (cs) {
		class_unregister(&cma_shrinker_class);
		kfree(cs);
		cs = NULL;
	}
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id cma_shrinker_dt_match[] = {
	{
		.compatible = "amlogic, cma-shrinker",
	},
	{}
};
#endif

static struct platform_driver cma_shrinker_driver = {
	.driver = {
		.name  = "cma_shrinker",
		.owner = THIS_MODULE,
	#ifdef CONFIG_OF
		.of_match_table = cma_shrinker_dt_match,
	#endif
	},
	.probe = cma_shrinker_probe,
	.remove = cma_shrinker_remove,
};

static int __init cma_shrinker_init(void)
{
	return platform_driver_register(&cma_shrinker_driver);
}

static void __exit cma_shrinker_uninit(void)
{
	platform_driver_unregister(&cma_shrinker_driver);
}

device_initcall(cma_shrinker_init);
module_exit(cma_shrinker_uninit);
