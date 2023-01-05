// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *opyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/dma-mapping.h>
#include <linux/dma-direct.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/pfn.h>
#include <linux/types.h>
#include <linux/highmem.h>
#include <linux/gfp.h>
#include <asm/dma.h>
#include <linux/printk.h>

#include <linux/init.h>
#include <linux/iommu-helper.h>
#include <linux/amlogic/dma_pcie_mapping.h>
#include <linux/dma-noncoherent.h>
#include <linux/of.h>
#include <linux/dma-contiguous.h>
#include <linux/genalloc.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

/*
 * Enumeration for sync targets
 */
enum dma_sync_target {
	SYNC_FOR_CPU = 0,
	SYNC_FOR_DEVICE = 1,
};

#define OFFSET(val, align) ((unsigned long) \
			((val) & ((align) - 1)))

/* default to 32MB */
#define AML_IO_TLB_DEFAULT_SIZE (64UL << 20)

/*
 * Maximum allowable number of contiguous slabs to map,
 * must be a power of 2.  What is the appropriate value ?
 * The complexity of {map,unmap}_single is linearly dependent on this value.
 */
#define IO_TLB_SEGSIZE	2048

/*
 * log of the size of each IO TLB slab.  The number of slabs is command line
 * controllable.
 */
#define IO_TLB_SHIFT 11

/*
 * We need to save away the original address corresponding to a mapped entry
 * for the sync operations.
 */
#define INVALID_PHYS_ADDR (~(phys_addr_t)0)
static phys_addr_t *io_tlb_orig_addr;

/*
 * Protect the above data structures in the map and unmap calls
 */
static DEFINE_SPINLOCK(io_tlb_lock);

static bool no_iotlb_memory;

/*
 * Used to do a quick range check in swiotlb_tbl_unmap_single and
 * swiotlb_tbl_sync_single_*, to see if the memory was in fact allocated by this
 * API.
 */
static phys_addr_t io_tlb_start, io_tlb_end;

/*
 * The number of IO TLB blocks (in groups of 64) between io_tlb_start and
 * io_tlb_end.  This is command line adjustable via setup_io_tlb_npages.
 */
static unsigned long io_tlb_nslabs;

/*
 * The number of used IO TLB block
 */
static unsigned long io_tlb_used;

/*
 * This is a free list describing the number of free entries available from
 * each index
 */
static unsigned int *io_tlb_list;
static unsigned int io_tlb_index;

static inline bool is_swiotlb_buffer(phys_addr_t paddr)
{
	return paddr >= io_tlb_start && paddr < io_tlb_end;
}

/*
 * Bounce: copy the swiotlb buffer from or back to the original dma location
 */
static void swiotlb_bounce(phys_addr_t orig_addr, phys_addr_t tlb_addr,
			   size_t size, enum dma_data_direction dir)
{
	unsigned long pfn = PFN_DOWN(orig_addr);
	unsigned char *vaddr = phys_to_virt(tlb_addr);

	if (PageHighMem(pfn_to_page(pfn))) {
		/* The buffer does not have a mapping.  Map it in and copy */
		unsigned int offset = orig_addr & ~PAGE_MASK;
		char *buffer;
		unsigned int sz = 0;
		unsigned long flags;

		while (size) {
			sz = min_t(size_t, PAGE_SIZE - offset, size);

			local_irq_save(flags);
			buffer = kmap_atomic(pfn_to_page(pfn));
			if (dir == DMA_TO_DEVICE)
				memcpy(vaddr, buffer + offset, sz);
			else
				memcpy(buffer + offset, vaddr, sz);
			kunmap_atomic(buffer);
			local_irq_restore(flags);

			size -= sz;
			pfn++;
			vaddr += sz;
			offset = 0;
		}
	} else if (dir == DMA_TO_DEVICE) {
		memcpy(vaddr, phys_to_virt(orig_addr), size);
	} else {
		memcpy(phys_to_virt(orig_addr), vaddr, size);
	}
}

static phys_addr_t swiotlb_tbl_map_single(struct device *hwdev,
				   dma_addr_t tbl_dma_addr,
				   phys_addr_t orig_addr,
				   size_t mapping_size,
				   size_t alloc_size,
				   enum dma_data_direction dir,
				   unsigned long attrs)
{
	unsigned long flags;
	phys_addr_t tlb_addr;
	unsigned int nslots, stride, index, wrap;
	int i;
	unsigned long mask;
	unsigned long offset_slots;
	unsigned long max_slots;
	unsigned long tmp_io_tlb_used;

	if (no_iotlb_memory)
		panic("Can not allocate SWIOTLB buffer earlier and can't now provide you with the DMA bounce buffer");

	if (mapping_size > alloc_size) {
		dev_warn_once(hwdev, "Invalid sizes (mapping: %zd bytes, alloc: %zd bytes)",
			      mapping_size, alloc_size);
		return (phys_addr_t)DMA_MAPPING_ERROR;
	}

	mask = dma_get_seg_boundary(hwdev);

	tbl_dma_addr &= mask;

	offset_slots = ALIGN(tbl_dma_addr, 1 << IO_TLB_SHIFT) >> IO_TLB_SHIFT;

	/*
	 * Carefully handle integer overflow which can occur when mask == ~0UL.
	 */
	max_slots = mask + 1
		    ? ALIGN(mask + 1, 1 << IO_TLB_SHIFT) >> IO_TLB_SHIFT
		    : 1UL << (BITS_PER_LONG - IO_TLB_SHIFT);

	/*
	 * For mappings greater than or equal to a page, we limit the stride
	 * (and hence alignment) to a page size.
	 */
	nslots = ALIGN(alloc_size, 1 << IO_TLB_SHIFT) >> IO_TLB_SHIFT;
	if (alloc_size >= PAGE_SIZE)
		stride = (1 << (PAGE_SHIFT - IO_TLB_SHIFT));
	else
		stride = 1;

	WARN_ON(!nslots);

	/*
	 * Find suitable number of IO TLB entries size that will fit this
	 * request and allocate a buffer from that IO TLB pool.
	 */
	spin_lock_irqsave(&io_tlb_lock, flags);

	if (unlikely(nslots > io_tlb_nslabs - io_tlb_used))
		goto not_found;

	index = ALIGN(io_tlb_index, stride);
	if (index >= io_tlb_nslabs)
		index = 0;
	wrap = index;

	do {
		while (iommu_is_span_boundary(index, nslots, offset_slots,
					      max_slots)) {
			index += stride;
			if (index >= io_tlb_nslabs)
				index = 0;
			if (index == wrap)
				goto not_found;
		}

		/*
		 * If we find a slot that indicates we have 'nslots' number of
		 * contiguous buffers, we allocate the buffers from that slot
		 * and mark the entries as '0' indicating unavailable.
		 */
		if (io_tlb_list[index] >= nslots) {
			int count = 0;

			for (i = index; i < (int)(index + nslots); i++)
				io_tlb_list[i] = 0;
			for (i = index - 1; (OFFSET(i, IO_TLB_SEGSIZE) !=
				IO_TLB_SEGSIZE - 1) && io_tlb_list[i]; i--)
				io_tlb_list[i] = ++count;
			tlb_addr = io_tlb_start + (index << IO_TLB_SHIFT);

			/*
			 * Update the indices to avoid searching in the next
			 * round.
			 */
			io_tlb_index = ((index + nslots) < io_tlb_nslabs
					? (index + nslots) : 0);

			goto found;
		}
		index += stride;
		if (index >= io_tlb_nslabs)
			index = 0;
	} while (index != wrap);

not_found:
	tmp_io_tlb_used = io_tlb_used;

	spin_unlock_irqrestore(&io_tlb_lock, flags);
	if (!(attrs & DMA_ATTR_NO_WARN) && __printk_ratelimit(__func__))
		dev_warn(hwdev, "swiotlb buffer is full (sz: %zd bytes), total %lu (slots), used %lu (slots)\n",
			 alloc_size, io_tlb_nslabs, tmp_io_tlb_used);
	return (phys_addr_t)DMA_MAPPING_ERROR;
found:
	io_tlb_used += nslots;
	spin_unlock_irqrestore(&io_tlb_lock, flags);

	/*
	 * Save away the mapping from the original address to the DMA address.
	 * This is needed when we sync the memory.  Then we sync the buffer if
	 * needed.
	 */
	for (i = 0; i < nslots; i++)
		io_tlb_orig_addr[index + i] = orig_addr + (i << IO_TLB_SHIFT);
	if (!(attrs & DMA_ATTR_SKIP_CPU_SYNC) &&
	    (dir == DMA_TO_DEVICE || dir == DMA_BIDIRECTIONAL))
		swiotlb_bounce(orig_addr, tlb_addr, mapping_size, DMA_TO_DEVICE);

	return tlb_addr;
}

/*
 * Create a swiotlb mapping for the buffer at @phys, and in case of DMAing
 * to the device copy the data into it as well.
 */
static bool swiotlb_map(struct device *dev, phys_addr_t *phys, dma_addr_t *dma_addr,
		size_t size, enum dma_data_direction dir, unsigned long attrs)
{
	/* Oh well, have to allocate and map a bounce buffer. */
	*phys = swiotlb_tbl_map_single(dev, __phys_to_dma(dev, io_tlb_start),
			*phys, size, size, dir, attrs);
	if (*phys == (phys_addr_t)DMA_MAPPING_ERROR)
		return false;

	/* Ensure that the address returned is DMA'ble */
	*dma_addr = __phys_to_dma(dev, *phys);

	return true;
}

static void swiotlb_cleanup(void)
{
	io_tlb_end = 0;
	io_tlb_start = 0;
	io_tlb_nslabs = 0;
}

static void swiotlb_print_info(void)
{
	unsigned long bytes = io_tlb_nslabs << IO_TLB_SHIFT;

	if (no_iotlb_memory) {
		pr_warn("No low mem\n");
		return;
	}

	pr_info("mapped [mem %#010llx-%#010llx] (%luMB)\n",
		(unsigned long long)io_tlb_start,
		(unsigned long long)io_tlb_end, bytes >> 20);
}

static int swiotlb_init_with_tbl(phys_addr_t tlb, unsigned long nslabs)
{
	unsigned long i, bytes;

	bytes = nslabs << IO_TLB_SHIFT;

	io_tlb_nslabs = nslabs;
	io_tlb_start = tlb;
	io_tlb_end = io_tlb_start + bytes;

	/*
	 * Allocate and initialize the free list array.  This array is used
	 * to find contiguous free memory regions of size up to IO_TLB_SEGSIZE
	 * between io_tlb_start and io_tlb_end.
	 */
	io_tlb_list = (unsigned int *)__get_free_pages(GFP_KERNEL,
			get_order(io_tlb_nslabs * sizeof(int)));
	if (!io_tlb_list)
		goto cleanup3;

	io_tlb_orig_addr = (phys_addr_t *)
		__get_free_pages(GFP_KERNEL,
				get_order(io_tlb_nslabs * sizeof(phys_addr_t)));
	if (!io_tlb_orig_addr)
		goto cleanup4;

	for (i = 0; i < io_tlb_nslabs; i++) {
		io_tlb_list[i] = IO_TLB_SEGSIZE - OFFSET(i, IO_TLB_SEGSIZE);
		io_tlb_orig_addr[i] = INVALID_PHYS_ADDR;
	}
	io_tlb_index = 0;
	no_iotlb_memory = false;

	swiotlb_print_info();

	return 0;

cleanup4:
	free_pages((unsigned long)io_tlb_list,
		get_order(io_tlb_nslabs * sizeof(int)));
	io_tlb_list = NULL;
cleanup3:
	swiotlb_cleanup();
	return -ENOMEM;
}

/*
 * tlb_addr is the physical address of the bounce buffer to unmap.
 */
static void swiotlb_tbl_unmap_single(struct device *hwdev, phys_addr_t tlb_addr,
			      size_t mapping_size, size_t alloc_size,
			      enum dma_data_direction dir, unsigned long attrs)
{
	unsigned long flags;
	int i, count, nslots = ALIGN(alloc_size, 1 << IO_TLB_SHIFT) >> IO_TLB_SHIFT;
	int index = (tlb_addr - io_tlb_start) >> IO_TLB_SHIFT;
	phys_addr_t orig_addr = io_tlb_orig_addr[index];

	/*
	 * First, sync the memory before unmapping the entry
	 */
	if (orig_addr != INVALID_PHYS_ADDR &&
		!(attrs & DMA_ATTR_SKIP_CPU_SYNC) &&
		(dir == DMA_FROM_DEVICE || dir == DMA_BIDIRECTIONAL))
		swiotlb_bounce(orig_addr, tlb_addr, mapping_size, DMA_FROM_DEVICE);

	/*
	 * Return the buffer to the free list by setting the corresponding
	 * entries to indicate the number of contiguous entries available.
	 * While returning the entries to the free list, we merge the entries
	 * with slots below and above the pool being returned.
	 */
	spin_lock_irqsave(&io_tlb_lock, flags);
	{
		count = ((index + nslots) < ALIGN(index + 1, IO_TLB_SEGSIZE) ?
			 io_tlb_list[index + nslots] : 0);
		/*
		 * Step 1: return the slots to the free list, merging the
		 * slots with superceeding slots
		 */
		for (i = index + nslots - 1; i >= index; i--) {
			io_tlb_list[i] = ++count;
			io_tlb_orig_addr[i] = INVALID_PHYS_ADDR;
		}
		/*
		 * Step 2: merge the returned slots with the preceding slots,
		 * if available (non zero)
		 */
		for (i = index - 1; (OFFSET(i, IO_TLB_SEGSIZE) !=
			IO_TLB_SEGSIZE - 1) && io_tlb_list[i]; i--)
			io_tlb_list[i] = ++count;

		io_tlb_used -= nslots;
	}
	spin_unlock_irqrestore(&io_tlb_lock, flags);
}

static void swiotlb_tbl_sync_single(struct device *hwdev, phys_addr_t tlb_addr,
			     size_t size, enum dma_data_direction dir,
			     enum dma_sync_target target)
{
	int index = (tlb_addr - io_tlb_start) >> IO_TLB_SHIFT;
	phys_addr_t orig_addr = io_tlb_orig_addr[index];

	if (orig_addr == INVALID_PHYS_ADDR)
		return;
	orig_addr += (unsigned long)tlb_addr & ((1 << IO_TLB_SHIFT) - 1);

	switch (target) {
	case SYNC_FOR_CPU:
		if (likely(dir == DMA_FROM_DEVICE || dir == DMA_BIDIRECTIONAL))
			swiotlb_bounce(orig_addr, tlb_addr,
				       size, DMA_FROM_DEVICE);
		else
			WARN_ON(dir != DMA_TO_DEVICE);
		break;
	case SYNC_FOR_DEVICE:
		if (likely(dir == DMA_TO_DEVICE || dir == DMA_BIDIRECTIONAL))
			swiotlb_bounce(orig_addr, tlb_addr,
				       size, DMA_TO_DEVICE);
		else
			WARN_ON(dir != DMA_FROM_DEVICE);
		break;
	default:
		BUG();
	}
}

static struct device *aml_dma_dev;
/*
 * Statically reserve bounce buffer space and initialize bounce buffer data
 * structures for the software IO TLB used to implement the DMA API.
 */
void  pcie_swiotlb_init(struct device *dma_dev)
{
	size_t default_size = AML_IO_TLB_DEFAULT_SIZE;
	unsigned char *vstart;
	unsigned long bytes;
	dma_addr_t paddr = 0;

	if (!io_tlb_nslabs) {
		io_tlb_nslabs = (default_size >> IO_TLB_SHIFT);
		io_tlb_nslabs = ALIGN(io_tlb_nslabs, IO_TLB_SEGSIZE);
	}

	bytes = io_tlb_nslabs << IO_TLB_SHIFT;

	aml_dma_dev = dma_dev;
	/* Get IO TLB memory from the low pages */
	vstart = dma_alloc_coherent(dma_dev, PAGE_ALIGN(bytes), &paddr, GFP_KERNEL);
	if (vstart && !swiotlb_init_with_tbl(paddr, io_tlb_nslabs))
		return;

	pr_warn("Cannot allocate buffer");
	no_iotlb_memory = true;
}

static struct vm_struct *__dma_common_pages_remap(struct page **pages,
			size_t size, pgprot_t prot, const void *caller)
{
	struct vm_struct *area;

	area = get_vm_area_caller(size, VM_DMA_COHERENT, caller);
	if (!area)
		return NULL;

	if (map_vm_area(area, prot, pages)) {
		vunmap(area->addr);
		return NULL;
	}

	return area;
}

/*
 * Remaps an allocated contiguous region into another vm_area.
 * Cannot be used in non-sleeping contexts
 */
static void *aml_dma_common_contiguous_remap(struct page *page, size_t size,
			pgprot_t prot, const void *caller)
{
	int i;
	struct page **pages;
	struct vm_struct *area;

	pages = kmalloc(sizeof(struct page *) << get_order(size), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < (size >> PAGE_SHIFT); i++)
		pages[i] = nth_page(page, i);

	area = __dma_common_pages_remap(pages, size, prot, caller);

	kfree(pages);

	if (!area)
		return NULL;
	return area->addr;
}

/*
 * Unmaps a range previously mapped by dma_common_*_remap
 */
static void aml_dma_common_free_remap(void *cpu_addr, size_t size)
{
	struct vm_struct *area = find_vm_area(cpu_addr);

	if (!area || area->flags != VM_DMA_COHERENT) {
		WARN(1, "trying to free invalid coherent area: %p\n", cpu_addr);
		return;
	}

	unmap_kernel_range((unsigned long)cpu_addr, PAGE_ALIGN(size));
	vunmap(cpu_addr);
}

static struct gen_pool *aml_atomic_pool __ro_after_init;

static size_t atomic_pool_size __initdata = SZ_256K;

int aml_dma_atomic_pool_init(struct device *dev)
{
	unsigned int pool_size_order = get_order(atomic_pool_size);
	unsigned long nr_pages = atomic_pool_size >> PAGE_SHIFT;
	struct page *page;
	void *addr;
	int ret;

	page = dma_alloc_from_contiguous(dev, nr_pages,
						 pool_size_order, false);
	if (!page)
		goto out;

	arch_dma_prep_coherent(page, atomic_pool_size);

	aml_atomic_pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!aml_atomic_pool)
		goto free_page;

	addr = aml_dma_common_contiguous_remap(page, atomic_pool_size,
					   pgprot_dmacoherent(PAGE_KERNEL),
					   __builtin_return_address(0));
	if (!addr)
		goto destroy_genpool;

	ret = gen_pool_add_virt(aml_atomic_pool, (unsigned long)addr,
				page_to_phys(page), atomic_pool_size, -1);
	if (ret)
		goto remove_mapping;
	gen_pool_set_algo(aml_atomic_pool, gen_pool_first_fit_order_align, NULL);

	pr_info("aml DMA: preallocated %zu KiB pool for atomic allocations\n",
		atomic_pool_size / 1024);
	return 0;

remove_mapping:
	aml_dma_common_free_remap(addr, atomic_pool_size);
destroy_genpool:
	gen_pool_destroy(aml_atomic_pool);
	aml_atomic_pool = NULL;
free_page:
	dma_release_from_contiguous(dev, page, nr_pages);
out:
	pr_err("aml DMA: failed to allocate %zu KiB pool for atomic coherent allocation\n",
		atomic_pool_size / 1024);
	return -ENOMEM;
}

static bool aml_dma_in_atomic_pool(void *start, size_t size)
{
	if (unlikely(!aml_atomic_pool))
		return false;

	return addr_in_gen_pool(aml_atomic_pool, (unsigned long)start, size);
}

static void *aml_dma_alloc_from_pool(size_t size, struct page **ret_page, gfp_t flags)
{
	unsigned long val;
	void *ptr = NULL;

	if (!aml_atomic_pool) {
		WARN(1, "coherent pool not initialised!\n");
		return NULL;
	}

	val = gen_pool_alloc(aml_atomic_pool, size);
	if (val) {
		phys_addr_t phys = gen_pool_virt_to_phys(aml_atomic_pool, val);

		*ret_page = pfn_to_page(__phys_to_pfn(phys));
		ptr = (void *)val;
		memset(ptr, 0, size);
	}

	return ptr;
}

static bool aml_dma_free_from_pool(void *start, size_t size)
{
	if (!aml_dma_in_atomic_pool(start, size))
		return false;
	gen_pool_free(aml_atomic_pool, (unsigned long)start, size);
	return true;
}

static void *aml_dma_direct_alloc(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t gfp, unsigned long attrs)
{
	struct device_node *of_node = dev->of_node;
	int count;
	void *ret;
	struct page *page = NULL;

	count = of_property_count_elems_of_size(of_node, "memory-region", sizeof(u32));
	if (count <= 0 && aml_dma_dev)
		dev = aml_dma_dev;

	if (!gfpflags_allow_blocking(gfp)) {
		size = PAGE_ALIGN(size);
		ret = aml_dma_alloc_from_pool(size, &page, gfp);
		if (!ret)
			return NULL;

		*dma_handle = phys_to_dma(dev, page_to_phys(page));
		return ret;
	}

	return dma_direct_alloc(dev, size, dma_handle, gfp, attrs);
}

static void aml_dma_direct_free(struct device *dev, size_t size,
		void *cpu_addr, dma_addr_t dma_addr, unsigned long attrs)
{
	struct device_node *of_node = dev->of_node;
	int count;

	count = of_property_count_elems_of_size(of_node, "memory-region", sizeof(u32));
	if (count <= 0 && aml_dma_dev)
		dev = aml_dma_dev;

	if (!aml_dma_free_from_pool(cpu_addr, PAGE_ALIGN(size)))
		return dma_direct_free(dev, size, cpu_addr, dma_addr, attrs);
}

static void report_addr(struct device *dev, dma_addr_t dma_addr, size_t size)
{
	if (!dev->dma_mask) {
		dev_err_once(dev, "DMA map on device without dma_mask\n");
	} else if (*dev->dma_mask >= DMA_BIT_MASK(32) || dev->bus_dma_mask) {
		dev_err_once(dev,
			"overflow %pad+%zu of DMA mask %llx bus mask %llx\n",
			&dma_addr, size, *dev->dma_mask, dev->bus_dma_mask);
	}
	WARN_ON_ONCE(1);
}

static dma_addr_t aml_dma_map_page(struct device *dev, struct page *page,
		unsigned long offset, size_t size, enum dma_data_direction dir,
		unsigned long attrs)
{
	phys_addr_t phys = page_to_phys(page) + offset;
	dma_addr_t dma_addr = phys_to_dma(dev, phys);

	if (!swiotlb_map(dev, &phys, &dma_addr, size, dir, attrs)) {
		report_addr(dev, dma_addr, size);
		return DMA_MAPPING_ERROR;
	}

	if (!dev_is_dma_coherent(dev) && !(attrs & DMA_ATTR_SKIP_CPU_SYNC))
		arch_sync_dma_for_device(dev, phys, size, dir);
	return dma_addr;
}

static void aml_dma_sync_single_for_cpu(struct device *dev,
		dma_addr_t addr, size_t size, enum dma_data_direction dir)
{
	phys_addr_t paddr = dma_to_phys(dev, addr);

	if (!dev_is_dma_coherent(dev)) {
		arch_sync_dma_for_cpu(dev, paddr, size, dir);
		arch_sync_dma_for_cpu_all(dev);
	}

	if (unlikely(is_swiotlb_buffer(paddr)))
		swiotlb_tbl_sync_single(dev, paddr, size, dir, SYNC_FOR_CPU);
}

static void aml_dma_unmap_page(struct device *dev, dma_addr_t addr,
		size_t size, enum dma_data_direction dir, unsigned long attrs)
{
	phys_addr_t phys = dma_to_phys(dev, addr);

	if (!(attrs & DMA_ATTR_SKIP_CPU_SYNC))
		aml_dma_sync_single_for_cpu(dev, addr, size, dir);

	if (unlikely(is_swiotlb_buffer(phys)))
		swiotlb_tbl_unmap_single(dev, phys, size, size, dir, attrs);
}

void aml_dma_sync_single_for_device(struct device *dev,
		dma_addr_t addr, size_t size, enum dma_data_direction dir)
{
	phys_addr_t paddr = dma_to_phys(dev, addr);

	if (unlikely(is_swiotlb_buffer(paddr)))
		swiotlb_tbl_sync_single(dev, paddr, size, dir, SYNC_FOR_DEVICE);

	if (!dev_is_dma_coherent(dev))
		arch_sync_dma_for_device(dev, paddr, size, dir);
}

void aml_dma_unmap_sg(struct device *dev, struct scatterlist *sgl,
		int nents, enum dma_data_direction dir, unsigned long attrs)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl, sg, nents, i)
		aml_dma_unmap_page(dev, sg->dma_address, sg_dma_len(sg), dir,
			     attrs);
}

int aml_dma_map_sg(struct device *dev, struct scatterlist *sgl, int nents,
		enum dma_data_direction dir, unsigned long attrs)
{
	int i;
	struct scatterlist *sg;

	for_each_sg(sgl, sg, nents, i) {
		sg->dma_address = aml_dma_map_page(dev, sg_page(sg),
				sg->offset, sg->length, dir, attrs);
		if (sg->dma_address == DMA_MAPPING_ERROR)
			goto out_unmap;
		sg_dma_len(sg) = sg->length;
	}

	return nents;

out_unmap:
	aml_dma_unmap_sg(dev, sgl, i, dir, attrs | DMA_ATTR_SKIP_CPU_SYNC);
	return 0;
}

void aml_dma_sync_sg_for_cpu(struct device *dev,
		struct scatterlist *sgl, int nents, enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl, sg, nents, i) {
		phys_addr_t paddr = dma_to_phys(dev, sg_dma_address(sg));

		if (!dev_is_dma_coherent(dev))
			arch_sync_dma_for_cpu(dev, paddr, sg->length, dir);

		if (unlikely(is_swiotlb_buffer(paddr)))
			swiotlb_tbl_sync_single(dev, paddr, sg->length, dir,
					SYNC_FOR_CPU);
	}
}

void aml_dma_sync_sg_for_device(struct device *dev,
		struct scatterlist *sgl, int nents, enum dma_data_direction dir)
{
	struct scatterlist *sg;
	int i;

	for_each_sg(sgl, sg, nents, i) {
		phys_addr_t paddr = dma_to_phys(dev, sg_dma_address(sg));

		if (unlikely(is_swiotlb_buffer(paddr)))
			swiotlb_tbl_sync_single(dev, paddr, sg->length,
					dir, SYNC_FOR_DEVICE);
		if (!dev_is_dma_coherent(dev))
			arch_sync_dma_for_device(dev, paddr, sg->length, dir);
	}
}

const struct dma_map_ops aml_pcie_dma_ops = {
	.alloc			= aml_dma_direct_alloc,
	.free			= aml_dma_direct_free,
	.mmap			= dma_common_mmap,
	.get_sgtable		= dma_common_get_sgtable,
	.map_page		= aml_dma_map_page,
	.unmap_page		= aml_dma_unmap_page,
	.map_sg			= aml_dma_map_sg,
	.unmap_sg		= aml_dma_unmap_sg,
	.map_resource		= dma_direct_map_resource,
	.sync_single_for_cpu	= aml_dma_sync_single_for_cpu,
	.sync_single_for_device	= aml_dma_sync_single_for_device,
	.sync_sg_for_cpu	= aml_dma_sync_sg_for_cpu,
	.sync_sg_for_device	= aml_dma_sync_sg_for_device,
	.dma_supported		= dma_direct_supported,
	.get_required_mask	= dma_direct_get_required_mask,
};

