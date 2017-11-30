#ifndef __CMA_H__
#define __CMA_H__

/*
 * There is always at least global CMA area and a few optional
 * areas configured in kernel .config.
 */
#ifdef CONFIG_CMA_AREAS
#define MAX_CMA_AREAS	(1 + CONFIG_CMA_AREAS)

#else
#define MAX_CMA_AREAS	(0)

#endif

struct cma {
	unsigned long	base_pfn;
	unsigned long	count;
	unsigned long	*bitmap;
	unsigned int order_per_bit; /* Order of pages represented by one bit */
	struct mutex	lock;
	struct hlist_head mem_head;
	spinlock_t mem_head_lock;
};

static inline unsigned long cma_bitmap_maxno(struct cma *cma)
{
	return cma->count >> cma->order_per_bit;
}

extern struct cma cma_areas[MAX_CMA_AREAS];
extern unsigned cma_area_count;

extern phys_addr_t cma_get_base(struct cma *cma);
extern unsigned long cma_get_size(struct cma *cma);

extern int __init cma_declare_contiguous(phys_addr_t base,
			phys_addr_t size, phys_addr_t limit,
			phys_addr_t alignment, unsigned int order_per_bit,
			bool fixed, struct cma **res_cma);
extern int cma_init_reserved_mem(phys_addr_t base,
					phys_addr_t size, int order_per_bit,
					struct cma **res_cma);
extern struct page *cma_alloc(struct cma *cma, int count, unsigned int align);
extern bool cma_release(struct cma *cma, struct page *pages, int count);
extern void update_cma_ip(struct page *page, int count, unsigned long ip);
#endif
