/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __PAGE_TRACE_H__
#define __PAGE_TRACE_H__

#include <asm/memory.h>
#include <asm/stacktrace.h>
#include <asm/sections.h>
#include <linux/page-flags.h>
//#include <linux/slub_def.h>

/*
 * bit map lay out for _ret_ip table
 *
 *  31   28 27 26  24 23             0
 * +------+---+----------------------+
 * |      |   |      |               |
 * +------+---+----------------------+
 *     |    |     |         |
 *     |    |     |         +-------- offset of ip in base address
 *     |    |     +------------------ MIGRATE_TYPE
 *     |    +------------------------ base address select
 *     |                              0: ip base is in kernel address
 *     |                              1: ip base is in module address
 *     +----------------------------- allocate order
 *
 * Note:
 *  offset in ip address is Logical shift right by 2 bits,
 *  because kernel code are always compiled by ARM instruction
 *  set, so pc is aligned by 2. There are 24 bytes used for store
 *  offset in kernel/module, plus these 2 shift bits, You must
 *  make sure your kernel image size is not larger than 2^26 = 64MB
 */
#define IP_ORDER_MASK		(0xf0000000)
#define IP_MODULE_BIT		BIT(27)
#define IP_MIGRATE_MASK		(0x07000000)
#define IP_RANGE_MASK		(0x00ffffff)
 /* max order usually should not be 15 */
#define IP_INVALID		(0xf)

struct page;

/* this struct should not larger than 32 bit */
struct page_trace {
	union {
		struct {
			unsigned int ret_ip       :24;
			unsigned int migrate_type : 3;
			unsigned int module_flag  : 1;
			unsigned int order        : 4;
		};
		unsigned int ip_data;
	};
};

#ifdef CONFIG_AMLOGIC_SLAB_TRACE
/*
 * @entry:         rb tree for quick search/insert/delete
 * @s_addr:        start address for this slab object
 * @e_addr:        end address for this slab object
 * @object_count:  how many objects in this slab obj
 * @object_ip: a   array stores ip for each slab object
 */
struct slab_trace {
	struct rb_node entry;
	unsigned long s_addr;
	unsigned long e_addr;
	unsigned int object_count;
	unsigned int *object_ip;
};

/*
 * @trace_count:    how many slab_trace object we have used
 * @total_obj_size: total object size according obj size
 * @lock:           protection for rb tree update
 * @list:           link to root list
 * @root:           root for rb tree
 */
struct slab_trace_group {
	unsigned long trace_count;
	unsigned long total_obj_size;
	unsigned int  object_size;
	spinlock_t   lock;		/* protection for rb tree update */
	struct list_head list;
	struct kmem_cache *ip_cache;
	struct rb_root root;
};

#define SLAB_STACK_DEP		7
/*
 * @hash: hash value for stack
 * @entry: rb tree for quick search
 * @stack: stack for object
 */
struct slab_stack {
	unsigned int hash;
	unsigned int use_cnt;
	struct rb_node entry;
	unsigned long stack[SLAB_STACK_DEP];
};

struct slab_stack_master {
	int stack_cnt;
	spinlock_t stack_lock;		/* protection for rb tree update */
	struct kmem_cache *slab_stack_cache;
	struct rb_root stack_root;
};
#endif

#ifdef CONFIG_AMLOGIC_PAGE_TRACE
extern unsigned int cma_alloc_trace;
unsigned long unpack_ip(struct page_trace *trace);
unsigned int pack_ip(unsigned long ip, int order, gfp_t flag);
void set_page_trace(struct page *page, int order,
		    gfp_t gfp_flags, void *func);
void reset_page_trace(struct page *page, int order);
void page_trace_mem_init(void);
struct page_trace *find_page_base(struct page *page);
unsigned long find_back_trace(void);
unsigned long get_page_trace(struct page *page);
void show_data(unsigned long addr, int nbytes, const char *name);
int save_obj_stack(unsigned long *stack, int depth);
#ifdef CONFIG_AMLOGIC_SLAB_TRACE
int slab_trace_init(void);
int slab_trace_add_page(struct page *page, int order,
			struct kmem_cache *s, gfp_t flags);
int slab_trace_remove_page(struct page *page, int order, struct kmem_cache *s);
int slab_trace_mark_object(void *object, unsigned long ip,
			   struct kmem_cache *s);
int slab_trace_remove_object(void *object, struct kmem_cache *s);
int get_cache_max_order(struct kmem_cache *s);
#endif
#else
static inline unsigned long unpack_ip(struct page_trace *trace)
{
	return 0;
}

static inline void set_page_trace(struct page *page, int order, gfp_t gfp_flags)
{
}

static inline void reset_page_trace(struct page *page, int order)
{
}

static inline void page_trace_mem_init(void)
{
}

static inline struct page_trace *find_page_base(struct page *page)
{
	return NULL;
}

static inline unsigned long find_back_trace(void)
{
	return 0;
}

static inline unsigned long get_page_trace(struct page *page)
{
	return 0;
}

static inline int slab_trace_init(void)
{
	return 0;
}

static inline int slab_trace_add_page(struct page *page, int order,
				      struct kmem_cache *s, gfp_t flags)
{
	return 0;
}

static inline int slab_trace_remove_page(struct page *page, int order,
					 struct kmem_cache *s)
{
	return 0;
}

static inline int slab_trace_mark_object(void *object, unsigned long ip,
					 struct kmem_cache *s)
{
	return 0;
}

static inline int slab_trace_remove_object(void *object, struct kmem_cache *s)
{
	return 0;
}

static inline int get_cache_max_order(struct kmem_cache *s)
{
	return 0;
}

static inline int save_obj_stack(unsigned long *stack, int depth)
{
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_SLUB_DEBUG
int aml_slub_check_object(struct kmem_cache *s, void *p, void *q);
void aml_get_slub_trace(struct kmem_cache *s, struct page *page,
			gfp_t flags, int order);
void aml_put_slub_trace(struct page *page, struct kmem_cache *s);
int aml_check_kmemcache(struct kmem_cache_cpu *c, struct kmem_cache *s,
			void *object);
void aml_slub_set_trace(struct kmem_cache *s, void *object);
#endif /* CONFIG_AMLOGIC_SLUB_DEBUG */

#ifdef CONFIG_KALLSYMS
extern const unsigned long kallsyms_addresses[] __weak;
extern const int kallsyms_offsets[] __weak;
extern const u8 kallsyms_names[] __weak;
extern const unsigned long kallsyms_num_syms
	__attribute__((weak)) __section(.rodata);
extern const unsigned long kallsyms_relative_base
	__attribute__((weak)) __section(.rodata);
extern const u8 kallsyms_token_table[] __weak;
extern const u16 kallsyms_token_index[] __weak;
#endif /* CONFIG_KALLSYMS */

#endif /* __PAGE_TRACE_H__ */
