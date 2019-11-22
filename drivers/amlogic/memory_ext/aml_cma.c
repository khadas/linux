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
#include <linux/hugetlb.h>
#include <linux/proc_fs.h>
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
	spinlock_t  list_lock;
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
		anon_cma = global_page_state(NR_INACTIVE_ANON_CMA) +
			   global_page_state(NR_ACTIVE_ANON_CMA);
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

	if (page == NULL)
		return;

	fun = find_back_trace();
	if (cma_alloc_trace)
		pr_info("%s alloc page:%lx, count:%ld, func:%pf\n", __func__,
			page_to_pfn(page), cnt, (void *)fun);
	for (i = 0; i < cnt; i++) {
		set_page_trace(page, 0, __GFP_BDEV, (void *)fun);
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
		pr_info("%s free page:%lx, count:%d, func:%pf\n", __func__,
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

static struct page *get_migrate_page(struct page *page, unsigned long private,
				  int **resultp)
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
				    NULL, 0, cc->mode, MR_CMA);
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
		.page_type = COMPACT_CMA,
		.ignore_skip_hint = true,
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
					 bool skip_hwpoisoned_pages,
					 struct list_head *list)
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

	for (i = 0; i < nr_pages; i++)
		if (pfn_valid_within(pfn + i))
			break;
	if (unlikely(i == nr_pages))
		return NULL;
	return pfn_to_page(pfn + i);
}

int aml_check_pages_isolated(unsigned long start_pfn, unsigned long end_pfn,
			bool skip_hwpoisoned_pages)
{
	unsigned long pfn, flags;
	struct page *page;
	struct zone *zone;
	struct list_head free_list;

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
	if ((pfn < end_pfn) || !page)
		return -EBUSY;
	/* Check all pages are free or marked as ISOLATED */
	zone = page_zone(page);
	INIT_LIST_HEAD(&free_list);
	spin_lock_irqsave(&zone->lock, flags);
	pfn = __aml_check_pageblock_isolate(start_pfn, end_pfn,
					    skip_hwpoisoned_pages,
					    &free_list);
	spin_unlock_irqrestore(&zone->lock, flags);

	trace_test_pages_isolated(start_pfn, end_pfn, pfn);

	free_hot_cold_page_list(&free_list, 1);
	/* page may in kswap ? */
	if (pfn < end_pfn && zone->zone_pgdat)
		wake_up_interruptible(&zone->zone_pgdat->kswapd_wait);

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
		.page_type = COMPACT_CMA,
		.ignore_skip_hint = true,
		.contended = false,
	};
	INIT_LIST_HEAD(&cc.migratepages);

	cma_debug(0, NULL, " range [%lx-%lx]\n", start, end);
	ret = start_isolate_page_range(get_align_pfn_low(start),
				       get_align_pfn_high(end), MIGRATE_CMA,
				       false);
	if (ret) {
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
	} else
		ret = aml_alloc_contig_migrate_range(&cc, start, end, 0);

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
		if (cc.contended) {
			ret = -EINTR;
			pr_info("cma_alloc [%lx-%lx] aborted\n", start, end);
		} else
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

static int cma_vma_show(struct page *page, struct vm_area_struct *vma,
			unsigned long addr, void *arg)
{
#ifdef CONFIG_AMLOGIC_USER_FAULT
	struct mm_struct *mm = vma->vm_mm;

	show_vma(mm, addr);
#endif
	return SWAP_AGAIN;
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
	pr_info("page:%lx, map:%p, mf:%lx, pf:%lx, m:%d, c:%d, f:%pf\n",
		page_to_pfn(page), page->mapping, map_flag,
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
