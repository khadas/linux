/*
 * drivers/amlogic/memory_ext/page_trace.c
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

#include <linux/gfp.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#include <linux/mmzone.h>
#include <linux/memblock.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/sort.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/amlogic/page_trace.h>
#ifdef CONFIG_AMLOGIC_VMAP
#include <linux/amlogic/vmap_stack.h>
#endif
#ifdef CONFIG_AMLOGIC_CMA
#include <linux/amlogic/aml_cma.h>
#endif
#include <asm/stacktrace.h>
#include <asm/sections.h>

#ifndef CONFIG_64BIT
#define DEBUG_PAGE_TRACE	0
#else
#define DEBUG_PAGE_TRACE	0
#endif

#ifdef CONFIG_NUMA
#define COMMON_CALLER_SIZE	64	/* more function names if NUMA */
#else
#define COMMON_CALLER_SIZE	64
#endif

/*
 * this is a driver which will hook during page alloc/free and
 * record caller of each page to a buffer. Detailed information
 * of page allocate statistics can be find in /proc/pagetrace
 *
 */
static bool merge_function = 1;
static int page_trace_filter = 64; /* not print size < page_trace_filter */
unsigned int cma_alloc_trace;
static struct proc_dir_entry *d_pagetrace;
#ifndef CONFIG_64BIT
struct page_trace *trace_buffer;
static unsigned long ptrace_size;
static unsigned int trace_step = 1;
static bool page_trace_disable __initdata;
#endif

struct alloc_caller {
	unsigned long func_start_addr;
	unsigned long size;
};

struct fun_symbol {
	const char *name;
	int full_match;
	int matched;
};

static struct alloc_caller common_caller[COMMON_CALLER_SIZE];

/*
 * following functions are common API from page allocate, they should not
 * be record in page trace, parse for back trace should keep from these
 * functions
 */
static struct fun_symbol common_func[] __initdata = {
	{"__alloc_pages_nodemask",	1, 0},
	{"kmem_cache_alloc",		1, 0},
	{"__get_free_pages",		1, 0},
	{"__kmalloc",			1, 0},
	{"cma_alloc",			1, 0},
	{"dma_alloc_from_contiguous",	1, 0},
	{"aml_cma_alloc_post_hook",	1, 0},
	{"__dma_alloc",			1, 0},
	{"arm_dma_alloc",		1, 0},
	{"__kmalloc_track_caller",	1, 0},
	{"kmem_cache_alloc_trace",	1, 0},
	{"__alloc_from_contiguous",	1, 0},
	{"cma_allocator_alloc",		1, 0},
	{"cma_release",			1, 0},
	{"dma_release_from_contiguous", 1, 0},
	{"codec_mm_alloc_in",		0, 0},
	{"codec_mm_alloc",		0, 0},
	{"codec_mm_alloc_for_dma",	0, 0},
	{"codec_mm_extpool_pool_alloc",	1, 0},
	{"codec_mm_release",		1, 0},
	{"cma_allocator_free",		1, 0},
	{"alloc_pages_exact",		1, 0},
	{"get_zeroed_page",		1, 0},
	{"__vmalloc_node_range",	1, 0},
	{"vzalloc",			1, 0},
	{"vmalloc",			1, 0},
	{"__alloc_page_frag",		1, 0},
	{"kmalloc_order",		0, 0},
#ifdef CONFIG_NUMA
	{"alloc_pages_current",		1, 0},
	{"alloc_page_interleave",	1, 0},
	{"kmalloc_large_node",		1, 0},
	{"kmem_cache_alloc_node",	1, 0},
	{"__kmalloc_node",		1, 0},
	{"alloc_pages_vma",		1, 0},
#endif
#ifdef CONFIG_SLUB	/* for some static symbols not exported in headfile */
	{"new_slab",			0, 0},
	{"slab_alloc",			0, 0},
#endif
	{}		/* tail */
};

static inline bool page_trace_invalid(struct page_trace *trace)
{
	return trace->order == IP_INVALID;
}

#ifndef CONFIG_64BIT
static int __init early_page_trace_param(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (strcmp(buf, "off") == 0)
		page_trace_disable = true;
	else if (strcmp(buf, "on") == 0)
		page_trace_disable = false;

	pr_info("page_trace %sabled\n", page_trace_disable ? "dis" : "en");

	return 0;
}
early_param("page_trace", early_page_trace_param);

static int early_page_trace_step(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &trace_step))
		pr_info("page trace_step:%d\n", trace_step);

	return 0;
}
early_param("page_trace_step", early_page_trace_step);
#endif

#if DEBUG_PAGE_TRACE
static inline bool range_ok(struct page_trace *trace)
{
	unsigned long offset;

	offset = (trace->ret_ip << 2);
	if (trace->module_flag) {
		if (offset >= MODULES_END)
			return false;
	} else {
		if (offset >= ((unsigned long)_end - (unsigned long)_text))
			return false;
	}
	return true;
}

static bool check_trace_valid(struct page_trace *trace)
{
	unsigned long offset;

	if (trace->order == IP_INVALID)
		return true;

	if (trace->order >= 10 ||
	    trace->migrate_type >= MIGRATE_TYPES ||
	    !range_ok(trace)) {
		offset = (unsigned long)((trace - trace_buffer));
		pr_err("bad trace:%p, %x, pfn:%lx, ip:%pf\n", trace,
			*((unsigned int *)trace),
			offset / sizeof(struct page_trace),
			(void *)_RET_IP_);
		return false;
	}
	return true;
}
#endif /* DEBUG_PAGE_TRACE */

#ifdef CONFIG_64BIT
static void push_ip(struct page_trace *base, struct page_trace *ip)
{
	*base = *ip;
}
#else
static void push_ip(struct page_trace *base, struct page_trace *ip)
{
	int i;
	unsigned long end;

	for (i = trace_step - 1; i > 0; i--)
		base[i] = base[i - 1];

	/* debug check */
#if DEBUG_PAGE_TRACE
	check_trace_valid(base);
#endif
	end = (((unsigned long)trace_buffer) + ptrace_size);
	WARN_ON((unsigned long)(base + trace_step - 1) >= end);

	base[0] = *ip;
}
#endif /* CONFIG_64BIT */

static inline int is_module_addr(unsigned long ip)
{
	if (ip >= MODULES_VADDR && ip < MODULES_END)
		return 1;
	return 0;
}

/*
 * set up information for common caller in memory allocate API
 */
static int __init setup_common_caller(unsigned long kaddr)
{
	unsigned long size, offset;
	int i = 0, ret;

	for (i = 0; i < COMMON_CALLER_SIZE; i++) {
		/* find a empty caller */
		if (!common_caller[i].func_start_addr)
			break;
	}
	if (i >= COMMON_CALLER_SIZE) {
		pr_err("%s, out of memory\n", __func__);
		return -1;
	}

	ret = kallsyms_lookup_size_offset(kaddr, &size, &offset);
	if (ret) {
		common_caller[i].func_start_addr = kaddr;
		common_caller[i].size = size;
		pr_debug("setup %d caller:%lx + %lx, %pf\n",
			i, kaddr, size, (void *)kaddr);
		return 0;
	}

	pr_err("can't find symbol %pf\n", (void *)kaddr);
	return -1;
}

static void __init dump_common_caller(void)
{
	int i;

	for (i = 0; i < COMMON_CALLER_SIZE; i++) {
		if (common_caller[i].func_start_addr)
			pr_debug("%2d, addr:%lx + %4lx, %pf\n", i,
				common_caller[i].func_start_addr,
				common_caller[i].size,
				(void *)common_caller[i].func_start_addr);
		else
			break;
	}
}

static int __init sym_cmp(const void *x1, const void *x2)
{
	struct alloc_caller *p1, *p2;

	p1 = (struct alloc_caller *)x1;
	p2 = (struct alloc_caller *)x2;

	/* desending order */
	return p1->func_start_addr < p2->func_start_addr ? 1 : -1;
}

static int __init match_common_caller(void *data, const char *name,
				       struct module *module,
				       unsigned long addr)
{
	int i, ret;
	struct fun_symbol *s;

	if (module)
		return -1;

	if (!strcmp(name, "_etext"))	/* end of text */
		return -1;

	for (i = 0; i < COMMON_CALLER_SIZE; i++) {
		s = &common_func[i];
		if (!s->name)
			break;		/* end */
		if (s->full_match && !s->matched) {
			if (!strcmp(name, s->name)) {	/* strict match */
				ret = setup_common_caller(addr);
				s->matched = 1;
			}
		} else if (!s->full_match) {
			if (strstr(name, s->name))	/* contians */
				setup_common_caller(addr);
		}
	}
	return 0;
}

static void __init find_static_common_symbol(void)
{

	memset(common_caller, 0, sizeof(common_caller));
	kallsyms_on_each_symbol(match_common_caller, NULL);
	sort(common_caller, COMMON_CALLER_SIZE, sizeof(struct alloc_caller),
		sym_cmp, NULL);
	dump_common_caller();
}

static int is_common_caller(struct alloc_caller *caller, unsigned long pc)
{
	int ret = 0;
	int low = 0, high = COMMON_CALLER_SIZE - 1, mid;
	unsigned long add_l, add_h;

	while (1) {
		mid = (high + low) / 2;
		add_l = caller[mid].func_start_addr;
		add_h = caller[mid].func_start_addr + caller[mid].size;
		if (pc >= add_l && pc < add_h) {
			ret = 1;
			break;
		}

		if (low >= high)	/* still not match */
			break;

		if (pc < add_l)		/* caller is desending order */
			low = mid + 1;
		else
			high = mid - 1;

		/* fix range */
		if (high < 0)
			high = 0;
		if (low > (COMMON_CALLER_SIZE - 1))
			low = COMMON_CALLER_SIZE - 1;
	}
	return ret;
}

unsigned long unpack_ip(struct page_trace *trace)
{
	unsigned long text;

	if (trace->order == IP_INVALID)
		return 0;

	if (trace->module_flag)
		text = MODULES_VADDR;
	else
		text = (unsigned long)_text;
	return text + ((trace->ret_ip) << 2);
}
EXPORT_SYMBOL(unpack_ip);

unsigned long find_back_trace(void)
{
	struct stackframe frame;
	int ret, step = 0;

#ifdef CONFIG_ARM64
	frame.fp = (unsigned long)__builtin_frame_address(0);
	frame.sp = current_stack_pointer;
	frame.pc = _RET_IP_;
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	frame.graph = current->curr_ret_stack;
#endif
#else
	frame.fp = (unsigned long)__builtin_frame_address(0);
	frame.sp = current_stack_pointer;
	frame.lr = (unsigned long)__builtin_return_address(0);
	frame.pc = (unsigned long)find_back_trace;
#endif
	while (1) {
	#ifdef CONFIG_ARM64
		ret = unwind_frame(current, &frame);
	#elif defined(CONFIG_ARM)
		ret = unwind_frame(&frame);
	#else	/* not supported */
		ret = -1;
	#endif
		if (ret < 0)
			return 0;
		step++;
		if (!is_common_caller(common_caller, frame.pc) && step > 1)
			return frame.pc;
	}
	pr_info("can't get pc\n");
	dump_stack();
	return 0;
}

int save_obj_stack(unsigned long *stack, int depth)
{
	struct stackframe frame;
	int ret, step = 0;

#ifdef CONFIG_ARM64
	frame.fp = (unsigned long)__builtin_frame_address(0);
	frame.sp = current_stack_pointer;
	frame.pc = _RET_IP_;
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	frame.graph = current->curr_ret_stack;
#endif
#else
	frame.fp = (unsigned long)__builtin_frame_address(0);
	frame.sp = current_stack_pointer;
	frame.lr = (unsigned long)__builtin_return_address(0);
	frame.pc = (unsigned long)save_obj_stack;
#endif
	while (step < depth) {
	#ifdef CONFIG_ARM64
		ret = unwind_frame(current, &frame);
	#elif defined(CONFIG_ARM)
		ret = unwind_frame(&frame);
	#else	/* not supported */
		ret = -1;
	#endif
		if (ret < 0)
			return 0;
		if (step >= 1)	/* ignore first function */
			stack[step - 1] = frame.pc;
		step++;
	}
	return 0;
}

#ifdef CONFIG_64BIT
struct page_trace *find_page_base(struct page *page)
{
	struct page_trace *trace;

	trace = (struct page_trace *)&page->trace;
	return trace;
}
#else
struct page_trace *find_page_base(struct page *page)
{
	unsigned long pfn, zone_offset = 0, offset;
	struct zone *zone;
	struct page_trace *p;

	if (!trace_buffer)
		return NULL;

	pfn = page_to_pfn(page);
	for_each_populated_zone(zone) {
		/* pfn is in this zone */
		if (pfn <= zone_end_pfn(zone) &&
		    pfn >= zone->zone_start_pfn) {
			offset = pfn - zone->zone_start_pfn;
			p = trace_buffer;
			return p + ((offset + zone_offset) * trace_step);
		}
		/* next zone */
		zone_offset += zone->spanned_pages;
	}
	return NULL;
}
#endif

unsigned long get_page_trace(struct page *page)
{
	struct page_trace *trace;

	trace = find_page_base(page);
	if (trace)
		return unpack_ip(trace);

	return 0;
}

#ifndef CONFIG_64BIT
static void __init set_init_page_trace(struct page *page, int order, gfp_t flag)
{
	unsigned long text, ip;
	struct page_trace trace = {}, *base;

	if (page && trace_buffer) {
		ip = (unsigned long)set_page_trace;
		text = (unsigned long)_text;

		trace.ret_ip = (ip - text) >> 2;
		trace.migrate_type = gfpflags_to_migratetype(flag);
		trace.order = order;
		base = find_page_base(page);
		push_ip(base, &trace);
	}

}
#endif

unsigned int pack_ip(unsigned long ip, int order, gfp_t flag)
{
	unsigned long text;
	struct page_trace trace = {};

	text = (unsigned long)_text;
	if (ip >= (unsigned long)_text)
		text = (unsigned long)_text;
	else if (is_module_addr(ip)) {
		text = MODULES_VADDR;
		trace.module_flag = 1;
	}

	trace.ret_ip = (ip - text) >> 2;
#ifdef CONFIG_AMLOGIC_CMA
	if (flag == __GFP_BDEV)
		trace.migrate_type = MIGRATE_CMA;
	else
		trace.migrate_type = gfpflags_to_migratetype(flag);
#else
	trace.migrate_type = gfpflags_to_migratetype(flag);
#endif /* CONFIG_AMLOGIC_CMA */
	trace.order = order;
#if DEBUG_PAGE_TRACE
	pr_debug("%s, base:%p, page:%lx, _ip:%x, o:%d, f:%x, ip:%lx\n",
		 __func__, base, page_to_pfn(page),
		 (*((unsigned int *)&trace)), order,
		 flag, ip);
#endif
	return *((unsigned int *)&trace);
}
EXPORT_SYMBOL(pack_ip);

void set_page_trace(struct page *page, int order, gfp_t flag, void *func)
{
	unsigned long ip;
	struct page_trace *base;
	unsigned int val, i;

#ifdef CONFIG_64BIT
	if (page) {
#else
	if (page && trace_buffer) {
#endif
		if (!func)
			ip = find_back_trace();
		else
			ip = (unsigned long)func;
		if (!ip) {
			pr_debug("can't find backtrace for page:%lx\n",
				page_to_pfn(page));
			return;
		}
		val = pack_ip(ip, order, flag);
		base = find_page_base(page);
		push_ip(base, (struct page_trace *)&val);
		if (order) {
			/* in order to easy get trace for high order alloc */
			val = pack_ip(ip, 0, flag);
			for (i = 1; i < (1 << order); i++) {
			#ifdef CONFIG_64BIT
				base = find_page_base(++page);
			#else
				base += (trace_step);
			#endif
				push_ip(base, (struct page_trace *)&val);
			}
		}
	}
}
EXPORT_SYMBOL(set_page_trace);

#ifdef CONFIG_64BIT
void reset_page_trace(struct page *page, int order)
{
	struct page_trace *base;
	struct page *p;
	int i, cnt;

	if (page) {
		cnt = 1 << order;
		p = page;
		for (i = 0; i < cnt; i++) {
			base = find_page_base(p);
			base->order = IP_INVALID;
			p++;
		}
	}
}
#else
void reset_page_trace(struct page *page, int order)
{
	struct page_trace *base;
	int i, cnt;
#if DEBUG_PAGE_TRACE
	unsigned long end;
#endif

	if (page && trace_buffer) {
		base = find_page_base(page);
		cnt = 1 << order;
	#if DEBUG_PAGE_TRACE
		check_trace_valid(base);
		end = ((unsigned long)trace_buffer + ptrace_size);
		WARN_ON((unsigned long)(base + cnt * trace_step - 1) >= end);
	#endif
		for (i = 0; i < cnt; i++) {
			base->order = IP_INVALID;
			base += (trace_step);
		}
	}
}
#endif
EXPORT_SYMBOL(reset_page_trace);

#ifndef CONFIG_64BIT
/*
 * move page out of buddy and make sure they are not malloced by
 * other module
 *
 * Note:
 * before call this functions, memory for page trace buffer are
 * freed to buddy.
 */
static int __init page_trace_pre_work(unsigned long size)
{
	unsigned int order = get_order(size);
	unsigned long addr;
	unsigned long used, end;
	struct page *page;

	if (order >= MAX_ORDER) {
		pr_err("trace buffer size %lx is too large\n", size);
		return -1;
	}
	addr = __get_free_pages(GFP_KERNEL, order);
	if (!addr)
		return -ENOMEM;

	trace_buffer = (struct page_trace *)addr;
	end  = addr + (PAGE_SIZE << order);
	used = addr + size;
	pr_info("%s, trace buffer:%p, size:%lx, used:%lx, end:%lx\n",
		__func__, trace_buffer, size, used, end);
	memset((void *)addr, 0, size);

	for (; addr < end; addr += PAGE_SIZE) {
		if (addr < used) {
			page = virt_to_page((void *)addr);
			set_init_page_trace(page, 0, GFP_KERNEL);
		#if DEBUG_PAGE_TRACE
			pr_info("%s, trace page:%p, %lx\n",
				__func__, page, page_to_pfn(page));
		#endif
		} else {
			free_page(addr);
		#if DEBUG_PAGE_TRACE
			pr_info("%s, free page:%lx, %lx\n", __func__,
				addr, page_to_pfn(virt_to_page(addr)));
		#endif
		}
	}
	return 0;
}
#endif

/*--------------------------sysfs node -------------------------------*/
#define LARGE	512
#define SMALL	128

/* caller for unmovalbe are max */
#define MT_UNMOVABLE_IDX	0                            /* 0,UNMOVABLE   */
#define MT_MOVABLE_IDX		(MT_UNMOVABLE_IDX   + LARGE) /* 1,MOVABLE     */
#define MT_RECLAIMABLE_IDX	(MT_MOVABLE_IDX     + SMALL) /* 2,RECLAIMABLE */
#define MT_HIGHATOMIC_IDX	(MT_RECLAIMABLE_IDX + SMALL) /* 3,HIGHATOMIC  */
#define MT_CMA_IDX		(MT_HIGHATOMIC_IDX  + SMALL) /* 4,CMA         */
#define MT_ISOLATE_IDX		(MT_CMA_IDX         + SMALL) /* 5,ISOLATE     */

#define SHOW_CNT		(MT_ISOLATE_IDX)

static int mt_offset[] = {
	MT_UNMOVABLE_IDX,
	MT_MOVABLE_IDX,
	MT_RECLAIMABLE_IDX,
	MT_HIGHATOMIC_IDX,
	MT_CMA_IDX,
	MT_ISOLATE_IDX,
	MT_ISOLATE_IDX + SMALL
};

struct page_summary {
	struct rb_node entry;
	unsigned long ip;
	unsigned long cnt;
};

struct pagetrace_summary {
	struct page_summary *sum;
	unsigned long ticks;
	int mt_cnt[MIGRATE_TYPES];
};

static unsigned long find_ip_base(unsigned long ip)
{
	unsigned long size, offset;
	int ret;

	ret = kallsyms_lookup_size_offset(ip, &size, &offset);
	if (ret)	/* find */
		return ip - offset;
	else		/* not find */
		return ip;
}

static int find_page_ip(struct page_trace *trace,
			struct page_summary *sum, int *o,
			int range, int *mt_cnt, int size,
			struct rb_root *root)
{
	int order;
	unsigned long ip;
	struct rb_node **link, *parent = NULL;
	struct page_summary *tmp;

	*o = 0;
	ip = unpack_ip(trace);
	if (!ip || !trace->ip_data)	/* invalid ip */
		return 0;

	order = trace->order;
	link  = &root->rb_node;
	while (*link) {
		parent = *link;
		tmp = rb_entry(parent, struct page_summary, entry);
		if (ip == tmp->ip) { /* match */
			tmp->cnt += ((1 << order) * size);
			*o = order;
			return 0;
		} else if (ip < tmp->ip)
			link = &tmp->entry.rb_left;
		else
			link = &tmp->entry.rb_right;
	}
	/* not found, get a new page summary */
	if (mt_cnt[trace->migrate_type] >= range)
		return -ERANGE;
	tmp       = &sum[mt_cnt[trace->migrate_type]];
	tmp->ip   = ip;
	tmp->cnt += ((1 << order) * size);
	*o        = order;
	mt_cnt[trace->migrate_type]++;
	rb_link_node(&tmp->entry, parent, link);
	rb_insert_color(&tmp->entry, root);
	return 0;
}

#define K(x)		((x) << (PAGE_SHIFT - 10))
static int trace_cmp(const void *x1, const void *x2)
{
	struct page_summary *s1, *s2;

	s1 = (struct page_summary *)x1;
	s2 = (struct page_summary *)x2;
	return s2->cnt - s1->cnt;
}

static void show_page_trace(struct seq_file *m, struct zone *zone,
			    struct page_summary *sum, int *mt_cnt)
{
	int i, j;
	struct page_summary *p;
	unsigned long total_mt, total_used = 0;

	seq_printf(m, "%s            %s, %s\n",
		  "count(KB)", "kaddr", "function");
	seq_puts(m, "------------------------------\n");
	for (j = 0; j < MIGRATE_TYPES; j++) {

		if (!mt_cnt[j])	/* this migrate type is empty */
			continue;

		p = sum + mt_offset[j];
		sort(p, mt_cnt[j], sizeof(*p), trace_cmp, NULL);

		total_mt = 0;
		for (i = 0; i < mt_cnt[j]; i++) {
			if (!p[i].cnt)	/* may be empty after merge */
				continue;

			if (K(p[i].cnt) >= page_trace_filter) {
				seq_printf(m, "%8ld, %16lx, %pf\n",
					   K(p[i].cnt), p[i].ip,
					   (void *)p[i].ip);
			}
			total_mt += p[i].cnt;
		}
		if (!total_mt)
			continue;
		seq_puts(m, "------------------------------\n");
		seq_printf(m, "total pages:%6ld, %9ld kB, type:%s\n",
			   total_mt, K(total_mt), migratetype_names[j]);
		seq_puts(m, "------------------------------\n");
		total_used += total_mt;
	}
	seq_printf(m, "Zone %8s, managed:%6ld KB, free:%6ld kB, used:%6ld KB\n",
		   zone->name, K(zone->managed_pages),
		   K(zone_page_state(zone, NR_FREE_PAGES)), K(total_used));
	seq_puts(m, "------------------------------\n");
}

static int _merge_same_function(struct page_summary *p, int range)
{
	int j, k, real_used;

	/* first, replace all ip to entry of each function */
	for (j = 0; j < range; j++) {
		if (!p[j].ip)	/* Not used */
			break;
		p[j].ip = find_ip_base(p[j].ip);
	}

	real_used = j;
	/* second, loop and merge same ip */
	for (j = 0; j < real_used; j++) {
		for (k = j + 1; k < real_used; k++) {
			if (p[k].ip != (-1ul) &&
			    p[k].ip == p[j].ip) {
				p[j].cnt += p[k].cnt;
				p[k].ip  = (-1ul);
				p[k].cnt = 0;
			}
		}
	}
	return real_used;
}

static void merge_same_function(struct page_summary *sum, int *mt_cnt)
{
	int i, range;
	struct page_summary *p;

	for (i = 0; i < MIGRATE_TYPES; i++) {
		range = mt_cnt[i];
		p = sum + mt_offset[i];
		_merge_same_function(p, range);
	}
}

static int update_page_trace(struct seq_file *m, struct zone *zone,
			     struct page_summary *sum, int *mt_cnt)
{
	unsigned long pfn;
	unsigned long start_pfn = zone->zone_start_pfn;
	unsigned long end_pfn = zone_end_pfn(zone);
	int    ret = 0, mt;
	int    order;
	struct page_trace *trace;
	struct page_summary *p;
	struct rb_root root[MIGRATE_TYPES];

	memset(root, 0, sizeof(root));
	/* loop whole zone */
	for (pfn = start_pfn; pfn < end_pfn; pfn++) {
		struct page *page;

		order = 0;
		if (!pfn_valid(pfn))
			continue;

		page = pfn_to_page(pfn);

		/* Watch for unexpected holes punched in the memmap */
		if (!memmap_valid_within(pfn, page, zone))
			continue;

		trace = find_page_base(page);
	#if DEBUG_PAGE_TRACE
		check_trace_valid(trace);
	#endif
		if (page_trace_invalid(trace)) /* free pages */
			continue;

		if (!(*(unsigned int *)trace)) /* empty */
			continue;

		mt = trace->migrate_type;
		p  = sum + mt_offset[mt];
		ret = find_page_ip(trace, p, &order,
				   mt_offset[mt + 1] - mt_offset[mt],
				   mt_cnt, 1, &root[mt]);
		if (ret) {
			pr_err("mt type:%d, out of range:%d\n",
			       mt, mt_offset[mt + 1] - mt_offset[mt]);
			break;
		}
		if (order)
			pfn += ((1 << order) - 1);
	}
	if (merge_function)
		merge_same_function(sum, mt_cnt);
	return ret;
}

static int pagetrace_show(struct seq_file *m, void *arg)
{
	struct zone *zone;
	int ret, size = sizeof(struct page_summary) * SHOW_CNT;
	struct pagetrace_summary *sum;

#ifndef CONFIG_64BIT
	if (!trace_buffer) {
		seq_puts(m, "page trace not enabled\n");
		return 0;
	}
#endif
	sum = kzalloc(sizeof(*sum), GFP_KERNEL);
	if (!sum)
		return -ENOMEM;

	m->private = sum;
	sum->sum = vzalloc(size);
	if (!sum->sum) {
		kfree(sum);
		m->private = NULL;
		return -ENOMEM;
	}

	/* update only once */
	seq_puts(m, "==============================\n");
	sum->ticks = sched_clock();
	for_each_populated_zone(zone) {
		memset(sum->sum, 0, size);
		ret = update_page_trace(m, zone, sum->sum, sum->mt_cnt);
		if (ret) {
			seq_printf(m, "Error %d in zone %8s\n",
				   ret, zone->name);
			continue;
		}
		show_page_trace(m, zone, sum->sum, sum->mt_cnt);
	}
	sum->ticks = sched_clock() - sum->ticks;

	seq_printf(m, "SHOW_CNT:%d, buffer size:%d, tick:%ld ns\n",
		   SHOW_CNT, size, sum->ticks);
	seq_puts(m, "==============================\n");

	vfree(sum->sum);
	kfree(sum);

	return 0;
}

static int pagetrace_open(struct inode *inode, struct file *file)
{
	return single_open(file, pagetrace_show, NULL);
}

static ssize_t pagetrace_write(struct file *file, const char __user *buffer,
			      size_t count, loff_t *ppos)
{
	char *buf;
	unsigned long arg = 0;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EINVAL;
	}

	if (!strncmp(buf, "merge=", 6)) {	/* option for 'merge=' */
		if (sscanf(buf, "merge=%ld", &arg) < 0) {
			kfree(buf);
			return -EINVAL;
		}
		merge_function = arg ? 1 : 0;
		pr_info("set merge_function to %d\n", merge_function);
	}

	if (!strncmp(buf, "cma_trace=", 10)) {	/* option for 'cma_trace=' */
		if (sscanf(buf, "cma_trace=%ld", &arg) < 0) {
			kfree(buf);
			return -EINVAL;
		}
		cma_alloc_trace = arg ? 1 : 0;
		pr_info("set cma_trace to %d\n", cma_alloc_trace);
	}
	if (!strncmp(buf, "filter=", 7)) {	/* option for 'filter=' */
		if (sscanf(buf, "filter=%ld", &arg) < 0) {
			kfree(buf);
			return -EINVAL;
		}
		page_trace_filter = arg;
		pr_info("set filter to %d KB\n", page_trace_filter);
	}

	kfree(buf);

	return count;
}

static const struct file_operations pagetrace_file_ops = {
	.open		= pagetrace_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.write		= pagetrace_write,
	.release	= single_release,
};

#ifdef CONFIG_AMLOGIC_SLAB_TRACE
/*---------------- part 2 for slab trace ---------------------*/
#include <linux/jhash.h>

#define SLAB_TRACE_FLAG		(__GFP_ZERO | __GFP_REPEAT | GFP_ATOMIC)

static LIST_HEAD(st_root);
static int slab_trace_en __read_mostly;
static struct kmem_cache *slab_trace_cache;
static struct slab_stack_master *stm;
static struct proc_dir_entry *d_slabtrace;

static int __init early_slab_trace_param(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (strcmp(buf, "off") == 0)
		slab_trace_en = false;
	else if (strcmp(buf, "on") == 0)
		slab_trace_en = true;

	pr_info("slab_trace %sabled\n", slab_trace_en ? "dis" : "en");

	return 0;
}
early_param("slab_trace", early_slab_trace_param);

int __init slab_trace_init(void)
{
	struct slab_trace_group *group = NULL;
	struct kmem_cache *cache;
	int cache_size;
	char buf[64] = {0};
	int i;

	if (!slab_trace_en)
		return -EINVAL;

	for (i = 0; i <= KMALLOC_SHIFT_HIGH; i++) {
		cache = kmalloc_caches[i];
		if (!cache || cache->size >= PAGE_SIZE)
			continue;

		sprintf(buf, "trace_%s", cache->name);
		group = kzalloc(sizeof(*group), GFP_KERNEL);
		if (!group)
			goto nomem;

		cache_size = PAGE_SIZE * (1 << get_cache_max_order(cache));
		cache_size = (cache_size / cache->size) * sizeof(int);
		group->ip_cache = kmem_cache_create(buf, cache_size, cache_size,
						    SLAB_NOLEAKTRACE, NULL);
		if (!group->ip_cache)
			goto nomem;

		spin_lock_init(&group->lock);
		list_add(&group->list, &st_root);
		group->object_size = cache->size;
		cache->trace_group = group;
		pr_debug("%s, trace group %p for %s, %d:%d, cache_size:%d:%d\n",
			 __func__, group, cache->name,
			 cache->size, cache->object_size,
			 cache_size, get_cache_max_order(cache));
	}
	stm = kzalloc(sizeof(*stm), GFP_KERNEL);
	stm->slab_stack_cache = KMEM_CACHE(slab_stack, SLAB_NOLEAKTRACE);
	spin_lock_init(&stm->stack_lock);

	slab_trace_cache = KMEM_CACHE(slab_trace, SLAB_NOLEAKTRACE);
	WARN_ON(!slab_trace_cache);
	pr_info("%s, create slab trace cache:%p\n",
		__func__, slab_trace_cache);

	return 0;
nomem:
	pr_err("%s, failed to create trace group for %s\n",
		__func__, buf);
	kfree(group);
	return -ENOMEM;
}

/*
 * This function must under protect of lock
 */
static struct slab_trace *find_slab_trace(struct slab_trace_group *group,
					  unsigned long addr)
{
	struct rb_node *rb;
	struct slab_trace *trace;

	rb = group->root.rb_node;
	while (rb) {
		trace = rb_entry(rb, struct slab_trace, entry);
		if (addr >= trace->s_addr && addr < trace->e_addr)
			return trace;
		if (addr < trace->s_addr)
			rb = trace->entry.rb_left;
		if (addr >= trace->e_addr)
			rb = trace->entry.rb_right;
	}
	return NULL;
}

int slab_trace_add_page(struct page *page, int order,
			struct kmem_cache *s, gfp_t flag)
{
	struct rb_node **link, *parent = NULL;
	struct slab_trace *trace = NULL, *tmp;
	struct slab_trace_group *group;
	void *buf = NULL;
	unsigned long addr, flags;
	int obj_cnt;

	if (!slab_trace_en || !page || !s || !s->trace_group)
		return -EINVAL;

	trace = kmem_cache_alloc(slab_trace_cache, SLAB_TRACE_FLAG);
	if (!trace)
		goto nomem;

	obj_cnt = PAGE_SIZE * (1 << order) / s->size;
	group = s->trace_group;
	buf = kmem_cache_alloc(group->ip_cache, SLAB_TRACE_FLAG);
	if (!buf)
		goto nomem;

	addr = (unsigned long)page_address(page);
	trace->s_addr = addr;
	trace->e_addr = addr + PAGE_SIZE * (1 << order);
	trace->object_count = obj_cnt;
	trace->object_ip = buf;
	/*
	 * insert it to rb_tree;
	 */
	spin_lock_irqsave(&group->lock, flags);
	link = &group->root.rb_node;
	while (*link) {
		parent = *link;
		tmp = rb_entry(parent, struct slab_trace, entry);
		if (addr < tmp->s_addr)
			link = &tmp->entry.rb_left;
		else if (addr >= tmp->e_addr)
			link = &tmp->entry.rb_right;
	}
	rb_link_node(&trace->entry, parent, link);
	rb_insert_color(&trace->entry, &group->root);
	group->trace_count++;
	spin_unlock_irqrestore(&group->lock, flags);
	pr_debug("%s, add:%lx-%lx, buf:%p, trace:%p to group:%p, %ld, obj:%d\n",
		s->name, addr, trace->e_addr,
		buf, trace, group, group->trace_count, obj_cnt);
	return 0;

nomem:
	pr_err("%s, failed to trace obj %p for %s, trace:%p\n", __func__,
		page_address(page), s->name, trace);
	kfree(trace);
	return -ENOMEM;
}

int slab_trace_remove_page(struct page *page, int order, struct kmem_cache *s)
{
	struct slab_trace *trace = NULL;
	struct slab_trace_group *group;
	unsigned long addr, flags;

	if (!slab_trace_en || !page || !s || !s->trace_group)
		return -EINVAL;

	addr  = (unsigned long)page_address(page);
	group = s->trace_group;
	spin_lock_irqsave(&group->lock, flags);
	trace = find_slab_trace(group, addr);
	if (!trace) {
		spin_unlock_irqrestore(&group->lock, flags);
		return 0;
	}
	rb_erase(&trace->entry, &group->root);
	group->trace_count--;
	spin_unlock_irqrestore(&group->lock, flags);
	WARN_ON((addr + PAGE_SIZE * (1 << order)) != trace->e_addr);
	pr_debug("%s, rm: %lx-%lx, buf:%p, trace:%p to group:%p, %ld\n",
		s->name, addr, trace->e_addr,
		trace->object_ip, trace, group, group->trace_count);
	kfree(trace->object_ip);
	kfree(trace);
	return 0;
}

static int record_stack(unsigned int hash, unsigned long *stack)
{
	struct rb_node **link, *parent = NULL;
	struct slab_stack *tmp, *new;
	unsigned long flags;

	/* No matched hash, we need create another one */
	new = kmem_cache_alloc(stm->slab_stack_cache, SLAB_TRACE_FLAG);
	if (!new)
		return -ENOMEM;

	spin_lock_irqsave(&stm->stack_lock, flags);
	link = &stm->stack_root.rb_node;
	while (*link) {
		parent = *link;
		tmp = rb_entry(parent, struct slab_stack, entry);
		if (hash == tmp->hash) {
			tmp->use_cnt++;
			/* hash match, but we need check stack same? */
			if (memcmp(stack, tmp->stack, sizeof(tmp->stack))) {
				int i;

				pr_err("%s stack hash confilct:%x\n",
				       __func__, hash);
				for (i = 0; i < SLAB_STACK_DEP; i++) {
					pr_err("%16lx %16lx, %pf, %pf\n",
					       tmp->stack[i], stack[i],
					       (void *)tmp->stack[i],
					       (void *)stack[i]);
				}
			}
			spin_unlock_irqrestore(&stm->stack_lock, flags);
			kfree(new);
			return 0;
		} else if (hash < tmp->hash)
			link = &tmp->entry.rb_left;
		else
			link = &tmp->entry.rb_right;
	}
	/* add to stack tree */
	new->hash    = hash;
	new->use_cnt = 1;
	memcpy(new->stack, stack, sizeof(new->stack));
	rb_link_node(&new->entry, parent, link);
	rb_insert_color(&new->entry, &stm->stack_root);
	stm->stack_cnt++;
	spin_unlock_irqrestore(&stm->stack_lock, flags);

	return 0;
}

static struct slab_stack *get_hash_stack(unsigned int hash)
{
	struct rb_node *rb;
	struct slab_stack *stack;

	rb = stm->stack_root.rb_node;
	while (rb) {
		stack = rb_entry(rb, struct slab_stack, entry);
		if (hash == stack->hash)
			return stack;

		if (hash < stack->hash)
			rb = stack->entry.rb_left;
		if (hash > stack->hash)
			rb = stack->entry.rb_right;
	}
	return NULL;
}

int slab_trace_mark_object(void *object, unsigned long ip,
			   struct kmem_cache *s)
{
	struct slab_trace *trace = NULL;
	struct slab_trace_group *group;
	unsigned long addr, flags, index;
	unsigned long stack[SLAB_STACK_DEP] = {0};
	unsigned int hash;

	if (!slab_trace_en || !object || !s || !s->trace_group)
		return -EINVAL;

	addr  = (unsigned long)object;
	group = s->trace_group;
	spin_lock_irqsave(&group->lock, flags);
	trace = find_slab_trace(group, addr);
	spin_unlock_irqrestore(&group->lock, flags);
	if (!trace)
		return -ENODEV;

	group->total_obj_size += s->size;
	index = (addr - trace->s_addr) / s->size;
	WARN_ON(index >= trace->object_count);
	if (save_obj_stack(stack, SLAB_STACK_DEP))
		return -EINVAL;
	hash = jhash2((unsigned int *)stack,
		      sizeof(stack) / sizeof(int), 0x9747b28c);
	record_stack(hash, stack);
	trace->object_ip[index] = hash;
	pr_debug("%s, mk object:%p,%lx, idx:%ld, trace:%p, group:%p,%ld, %pf\n",
		s->name, object, trace->s_addr, index,
		trace, group, group->total_obj_size, (void *)ip);
	return 0;
}

int slab_trace_remove_object(void *object, struct kmem_cache *s)
{
	struct slab_trace *trace = NULL;
	struct slab_trace_group *group;
	unsigned long addr, flags, index;
	unsigned int hash, need_free = 0;
	struct slab_stack *ss;

	if (!slab_trace_en || !object || !s || !s->trace_group)
		return -EINVAL;

	addr  = (unsigned long)object;
	group = s->trace_group;
	spin_lock_irqsave(&group->lock, flags);
	trace = find_slab_trace(group, addr);
	spin_unlock_irqrestore(&group->lock, flags);
	if (!trace)
		return -EINVAL;

	group->total_obj_size -= s->size;
	index = (addr - trace->s_addr) / s->size;
	WARN_ON(index >= trace->object_count);

	/* remove hashed stack */
	hash = trace->object_ip[index];
	spin_lock_irqsave(&stm->stack_lock, flags);
	ss = get_hash_stack(hash);
	if (ss) {
		ss->use_cnt--;
		if (!ss->use_cnt) {
			rb_erase(&ss->entry, &stm->stack_root);
			stm->stack_cnt--;
			need_free = 1;
		}
	}
	spin_unlock_irqrestore(&stm->stack_lock, flags);
	trace->object_ip[index] = 0;
	if (need_free)
		kfree(ss);
	pr_debug("%s, rm object: %p, %lx, idx:%ld, trace:%p, group:%p, %ld\n",
		s->name, object, trace->s_addr, index,
		trace, group, group->total_obj_size);
	return 0;
}

/*
 * functions to summary slab trace
 */
#define SLAB_TRACE_SHOW_CNT		1024

static int find_slab_hash(unsigned int hash, struct page_summary *sum,
			  int range, int *funcs, int size, struct rb_root *root)
{
	struct rb_node **link, *parent = NULL;
	struct page_summary *tmp;

	link  = &root->rb_node;
	while (*link) {
		parent = *link;
		tmp = rb_entry(parent, struct page_summary, entry);
		if (hash == tmp->ip) { /* match */
			tmp->cnt += size;
			return 0;
		} else if (hash < tmp->ip)
			link = &tmp->entry.rb_left;
		else
			link = &tmp->entry.rb_right;
	}
	/* not found, get a new page summary */
	if (*funcs >= range)
		return -ERANGE;
	tmp       = &sum[*funcs];
	tmp->ip   = hash;
	tmp->cnt += size;
	*funcs    = *funcs + 1;
	rb_link_node(&tmp->entry, parent, link);
	rb_insert_color(&tmp->entry, root);
	return 0;
}

static int update_slab_trace(struct seq_file *m, struct slab_trace_group *group,
			     struct page_summary *sum, unsigned long *tick,
			     int remain)
{
	struct rb_node *rb;
	struct slab_trace *trace;
	unsigned long flags, time;
	int i, r, funcs = 0;
	struct rb_root root = RB_ROOT;

	/* This may lock long time */
	time = sched_clock();
	spin_lock_irqsave(&group->lock, flags);
	for (rb = rb_first(&group->root); rb; rb = rb_next(rb)) {
		trace = rb_entry(rb, struct slab_trace, entry);
		for (i = 0; i < trace->object_count; i++) {
			if (!trace->object_ip[i])
				continue;

			r = find_slab_hash(trace->object_ip[i], sum, remain,
					   &funcs, group->object_size, &root);
			if (r) {
				pr_err("slab trace cout is not enough\n");
				spin_unlock_irqrestore(&group->lock, flags);
				return -ERANGE;
			}
		}
	}
	spin_unlock_irqrestore(&group->lock, flags);
	*tick = sched_clock() - time;
	return funcs;
}

static void show_slab_trace(struct seq_file *m, struct page_summary *p,
			    int count)
{
	int i, j;
	unsigned long total = 0, flags;
	struct slab_stack *stack;

	seq_printf(m, "%s          %s, %s\n",
		  "size(bytes)", "kaddr", "function");
	seq_puts(m, "------------------------------\n");

	sort(p, count, sizeof(*p), trace_cmp, NULL);

	for (i = 0; i < count; i++) {
		if (!p[i].cnt)	/* may be empty after merge */
			continue;

		total += p[i].cnt;
		if (p[i].cnt >= page_trace_filter) {
			spin_lock_irqsave(&stm->stack_lock, flags);
			stack = get_hash_stack(p[i].ip);
			spin_unlock_irqrestore(&stm->stack_lock, flags);
			if (!stack)
				continue;

			seq_printf(m, "%8ld, %16lx, %pf\n",
				   p[i].cnt, stack->stack[0],
				   (void *)stack->stack[0]);
			for (j = 1; j < SLAB_STACK_DEP; j++) {
				seq_printf(m, "%8s  %16lx, %pf\n",
					   " ", stack->stack[j],
					   (void *)stack->stack[j]);
			}
		}
	}
	seq_printf(m, "total kmalloc slabs:%6ld, %9ld kB\n",
		   total, total >> 10);
	seq_puts(m, "------------------------------\n");
}

static int slabtrace_show(struct seq_file *m, void *arg)
{
	struct page_summary *sum, *p;
	int ret = 0, remain, alloc;
	struct slab_trace_group *group;
	unsigned long ticks, group_time = 0, funcs = 0;

	alloc = stm->stack_cnt + 200;
	sum   = vzalloc(sizeof(struct page_summary) * alloc);
	if (!sum)
		return -ENOMEM;
	m->private = sum;

	/* update only once */
	seq_puts(m, "==============================\n");
	p      = sum;
	remain = alloc;
	ticks  = sched_clock();
	list_for_each_entry(group, &st_root, list) {
		ret = update_slab_trace(m, group, p, &group_time, remain);
		seq_printf(m, "%s-%4d, trace:%8ld, total:%10ld, time:%12ld, f:%d\n",
			   "slab kmalloc", group->object_size,
			   group->trace_count, group->total_obj_size,
			   group_time, ret);
		if (ret < 0) {
			seq_printf(m, "Error %d in slab %d\n",
				   ret, group->object_size);
			return -ERANGE;
		}
		funcs  += ret;
		p      += ret;
		remain -= ret;
	}
	seq_puts(m, "------------------------------\n");
	show_slab_trace(m, sum, funcs);
	ticks = sched_clock() - ticks;

	seq_printf(m, "SHOW_CNT:%d, tick:%ld ns, funs:%ld\n",
		   stm->stack_cnt, ticks, funcs);
	seq_puts(m, "==============================\n");

	vfree(sum);

	return 0;
}

static int slabtrace_open(struct inode *inode, struct file *file)
{
	return single_open(file, slabtrace_show, NULL);
}
static const struct file_operations slabtrace_file_ops = {
	.open		= slabtrace_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

/*---------------- end for slab trace -----------------*/

static int __init page_trace_module_init(void)
{
#ifdef CONFIG_64BIT
	d_pagetrace = proc_create("pagetrace", 0444,
				  NULL, &pagetrace_file_ops);
#else
	if (!page_trace_disable)
		d_pagetrace = proc_create("pagetrace", 0444,
					  NULL, &pagetrace_file_ops);
#endif
	if (IS_ERR_OR_NULL(d_pagetrace)) {
		pr_err("%s, create sysfs failed\n", __func__);
		return -1;
	}

#ifdef CONFIG_AMLOGIC_SLAB_TRACE
	if (slab_trace_en)
		d_slabtrace = proc_create("slabtrace", 0444,
					  NULL, &slabtrace_file_ops);
	if (IS_ERR_OR_NULL(d_slabtrace)) {
		pr_err("%s, create sysfs failed\n", __func__);
		return -1;
	}
#endif

#ifndef CONFIG_64BIT
	if (!trace_buffer)
		return -ENOMEM;
#endif

	return 0;
}

static void __exit page_trace_module_exit(void)
{
	if (d_pagetrace)
		proc_remove(d_pagetrace);
#ifdef CONFIG_AMLOGIC_SLAB_TRACE
	if (d_slabtrace)
		proc_remove(d_slabtrace);
#endif
}
module_init(page_trace_module_init);
module_exit(page_trace_module_exit);

void __init page_trace_mem_init(void)
{
#ifndef CONFIG_64BIT
	struct zone *zone;
	unsigned long total_page = 0;
#endif

	find_static_common_symbol();
#ifdef CONFIG_KASAN	/* open multi_shot for kasan */
	kasan_save_enable_multi_shot();
#endif
#ifdef CONFIG_64BIT
	/*
	 * if this compiler error occurs, that means there are over 32 page
	 * flags, you should disable AMLOGIC_PAGE_TRACE or reduce some page
	 * flags.
	 */
	BUILD_BUG_ON((__NR_PAGEFLAGS + ZONES_WIDTH +
		      NODES_WIDTH + SECTIONS_WIDTH +
		      LAST_CPUPID_SHIFT) > 32);
#else
	if (page_trace_disable)
		return;

	for_each_populated_zone(zone) {
		total_page += zone->spanned_pages;
		pr_info("zone:%s, spaned pages:%ld, total:%ld\n",
			zone->name, zone->spanned_pages, total_page);
	}
	ptrace_size = total_page * sizeof(struct page_trace) * trace_step;
	ptrace_size = PAGE_ALIGN(ptrace_size);
	if (page_trace_pre_work(ptrace_size)) {
		trace_buffer = NULL;
		ptrace_size = 0;
		pr_err("%s reserve memory failed\n", __func__);
		return;
	}
#endif
}

void arch_report_meminfo(struct seq_file *m)
{
#ifdef CONFIG_AMLOGIC_CMA
	seq_printf(m, "DriverCma:      %8ld kB\n",
		   get_cma_allocated() * (1 << (PAGE_SHIFT - 10)));
#endif
#ifdef CONFIG_AMLOGIC_VMAP
	vmap_report_meminfo(m);
#endif
}

