/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_mm_os_common.c
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2022/07/15	Initial release
 * </pre>
 *
 ******************************************************************************/

/***************************** Include Files *********************************/
#include "adlak_mm_os_common.h"

#include "adlak_mm_common.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/

/************************** Function Prototypes ******************************/

void adlak_cma_deinit(struct device *dev) {
    AML_LOG_DEBUG("%s", __func__);
    of_reserved_mem_device_release(dev);
}

int adlak_cma_init(struct device *dev) {
    int ret;
    AML_LOG_DEBUG("%s", __func__);

    /* Initialize CMA */
    ret = of_reserved_mem_device_init(dev);

    if (ret) {
        AML_LOG_WARN("cma mem not present or init failed\n");
        return -1;
    } else {
        AML_LOG_INFO("cma memory init ok\n");
    }
    return 0;
}

#if CONFIG_ADLAK_MEM_POOL_EN
void adlak_free_cma_region_nocache(struct adlak_mem *mm) {
    if (ADLAK_IS_ERR_OR_NULL(mm->mem_pool)) {
        return;
    }
    if (mm->mem_pool->cpu_addr_base) {
        dma_free_coherent(mm->dev, mm->mem_pool->size, mm->mem_pool->cpu_addr_base,
                          mm->mem_pool->dma_addr_base);
    }
    adlak_os_free(mm->mem_pool);
    mm->mem_pool = NULL;
}

int adlak_alloc_cma_region_nocache(struct adlak_mem *mm) {
    int ret;

    dma_addr_t dma_hd = 0;
    uint64_t   size;
    void *     vaddr = NULL;
    int        try;
    size_t     size_dec;
    AML_LOG_DEBUG("%s", __func__);
    //
    ret = adlak_platform_get_rsv_mem_size(mm->dev, &size);
    if (ret) {
        goto err;
    }
    try      = 10;
    size_dec = size / 16;
    while (try--) {
        vaddr = dma_alloc_coherent(mm->dev, (size_t)size, &dma_hd, ADLAK_GFP_KERNEL);
        if (!vaddr) {
            AML_LOG_ERR("DMA alloc coherent failed: pa 0x%lX, size = %lu\n", (uintptr_t)dma_hd,
                        (uintptr_t)size);
            size = size - size_dec;
        } else {
            break;
        }
    }
    if (!vaddr) {
        goto err;
    }

    mm->mem_pool = adlak_os_zalloc(sizeof(struct adlak_mem_pool_info), ADLAK_GFP_KERNEL);
    if (ADLAK_IS_ERR_OR_NULL(mm->mem_pool)) {
        goto err_alloc;
    }

    mm->mem_pool->cpu_addr_base  = vaddr;
    mm->mem_pool->phys_addr_base = dma_to_phys(mm->dev, dma_hd);
    mm->mem_pool->dma_addr_base  = dma_hd;
    mm->mem_pool->size           = (size_t)size;
    mm->mem_pool->mem_src        = ADLAK_ENUM_MEMSRC_CMA;
    mm->mem_pool->cacheable      = false;

    AML_LOG_INFO("cma memory info: dma_addr= 0x%lX,  phys_addr= 0x%lX,size=%lu MByte\n",
                 (uintptr_t)mm->mem_pool->dma_addr_base, (uintptr_t)mm->mem_pool->phys_addr_base,
                 (uintptr_t)(mm->mem_pool->size / (1024 * 1024)));

    return 0;
err_alloc:
    dma_free_coherent(mm->dev, size, vaddr, dma_hd);
err:
    adlak_free_cma_region_nocache(mm);
    return (ERR(ENOMEM));
}

#endif

#if (CONFIG_ADLAK_MEM_POOL_EN && defined(CONFIG_ADLAK_USE_RESERVED_MEMORY))

void adlak_unmap_region_nocache(struct adlak_mem *mm) {
    if (ADLAK_IS_ERR_OR_NULL(mm->mem_pool)) {
        return;
    }
    if (mm->mem_pool->cpu_addr_base) {
        memunmap(mm->mem_pool->cpu_addr_base);
    }
    adlak_os_free(mm->mem_pool);
    mm->mem_pool = NULL;
}

inline int adlak_remap_region_nocache(struct adlak_mem *mm) {
    phys_addr_t physical;
    size_t      size;
    void *      vaddr = NULL;
#ifdef CONFIG_OF
#error "No support reserved-memory currently when the device-tree enabled."
#endif

    physical = mm->padlak->hw_res.adlak_resmem_pa;
    size     = mm->padlak->hw_res.adlak_resmem_size;
    if (0 == size) {
        goto err;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
    vaddr = memremap(physical, size, MEMREMAP_WC);  // write combine
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
    vaddr = memremap(physical, size, MEMREMAP_WT);
#else
    vaddr = ioremap_nocache(physical, size);
#endif

    if (!vaddr) {
        AML_LOG_ERR("fail to map physical to kernel space\n");
        vaddr = NULL;
        goto err;
    }

    mm->mem_pool = adlak_os_zalloc(sizeof(struct adlak_mem_pool_info), ADLAK_GFP_KERNEL);
    if (ADLAK_IS_ERR_OR_NULL(mm->mem_pool)) {
        goto err_alloc;
    }

    mm->mem_pool->cpu_addr_base  = vaddr;
    mm->mem_pool->phys_addr_base = physical;
    mm->mem_pool->dma_addr_base  = physical;
    mm->mem_pool->size           = size;
    mm->mem_pool->mem_src        = ADLAK_ENUM_MEMSRC_RESERVED;
    mm->mem_pool->cacheable      = false;

    AML_LOG_INFO("Reserved memory info: dma_addr= 0x%lX,  phys_addr= 0x%lX,size=%lu MByte\n",
                 (uintptr_t)mm->mem_pool->dma_addr_base, (uintptr_t)mm->mem_pool->phys_addr_base,
                 (uintptr_t)(mm->mem_pool->size / (1024 * 1024)));

    return 0;
err_alloc:

err:
    return -1;
}

#endif

static void adlak_os_free_pages(struct page *pages[], int nr_pages) {
    int i;
    AML_LOG_DEBUG("%s", __func__);
    for (i = 0; i < nr_pages; ++i) {
        if (pages[i]) __free_page(pages[i]);
    }
}
static void adlak_os_free_pages_contiguous(struct page *pages[], int nr_pages) {
    AML_LOG_DEBUG("%s", __func__);
    if (pages[0]) {
        __free_pages(pages[0], get_order(ADLAK_PAGE_ALIGN(nr_pages * ADLAK_PAGE_SIZE)));
    }
}

static int adlak_flush_cache_init(struct adlak_mem *mm, struct adlak_mem_handle *mm_info,
                                  uint32_t offset, uint32_t size) {
    int              ret        = ERR(NONE);
    struct sg_table *sgt        = NULL;
    int32_t          result     = 0;
    uint32_t         flush_size = ADLAK_ALIGN(size, cache_line_size());
    AML_LOG_DEBUG("%s", __func__);

    switch (mm_info->req.mem_direction) {
        case ADLAK_ENUM_MEM_DIR_WRITE_ONLY:
            mm_info->direction = DMA_TO_DEVICE;
            break;
        case ADLAK_ENUM_MEM_DIR_READ_ONLY:
            mm_info->direction = DMA_FROM_DEVICE;
            break;
        default:
            mm_info->direction = DMA_BIDIRECTIONAL;
            break;
    }

    if (size & (cache_line_size() - 1)) {
        AML_LOG_ERR("flush cache warning memory size 0x%x not align to cache line size 0x%x", size,
                    cache_line_size());
    }

    mm_info->sgt = kmalloc(sizeof(struct sg_table), ADLAK_GFP_KERNEL);
    if (!mm_info->sgt) {
        AML_LOG_ERR("failed to alloca memory for flush cache handle");
        ret = -1;
        goto err;
    }

    sgt = (struct sg_table *)mm_info->sgt;

    result = sg_alloc_table_from_pages(sgt, (struct page **)mm_info->pages, mm_info->nr_pages,
                                       offset, flush_size, ADLAK_GFP_KERNEL | __GFP_NOWARN);
    if (unlikely(result < 0)) {
        AML_LOG_ERR("sg_alloc_table_from_pages failed");
        ret = -1;
        goto err;
    }

    result = dma_map_sg(mm->dev, sgt->sgl, sgt->nents, mm_info->direction);
    if (unlikely(result != sgt->nents)) {
        AML_LOG_ERR("dma_map_sg failed");
        ret = -1;
        goto err;
    }

err:
    return ret;
}

static int adlak_flush_cache_destroy(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    struct sg_table *sgt = NULL;
    AML_LOG_DEBUG("%s", __func__);

    sgt = (struct sg_table *)mm_info->sgt;
    if (sgt != NULL) {
        dma_unmap_sg(mm->dev, sgt->sgl, sgt->nents, DMA_FROM_DEVICE);

        sg_free_table(sgt);

        adlak_os_free(sgt);
        mm_info->sgt = NULL;
    }

    return 0;
}

static int adlak_flush_cache_init2(struct adlak_mem *mm, struct adlak_mem_handle *mm_info,
                                   uint32_t offset, uint32_t size, struct page *page_continue) {
    switch (mm_info->req.mem_direction) {
        case ADLAK_ENUM_MEM_DIR_WRITE_ONLY:
            mm_info->direction = DMA_TO_DEVICE;
            break;
        case ADLAK_ENUM_MEM_DIR_READ_ONLY:
            mm_info->direction = DMA_FROM_DEVICE;
            break;
        default:
            mm_info->direction = DMA_BIDIRECTIONAL;
            break;
    }

    mm_info->dma_addr = dma_map_page(mm->dev, page_continue, offset, size, mm_info->direction);

    if (dma_mapping_error(mm->dev, mm_info->dma_addr)) {
        AML_LOG_ERR("failed to dma map pa 0x%lX\n", (uintptr_t)mm_info->phys_addrs[0]);
        return ERR(EFAULT);
    }
    return ERR(NONE);
}

static void adlak_flush_cache_destroy2(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    if (mm_info->dma_addr) {
        dma_unmap_page(mm->dev, mm_info->dma_addr, mm_info->req.bytes, mm_info->direction);
    }
}

void adlak_os_free_discontiguous(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    AML_LOG_DEBUG("%s", __func__);

    if (mm_info->cpu_addr) {
        /* ummap kernel space */
        vunmap(mm_info->cpu_addr);
        adlak_flush_cache_destroy(mm, mm_info);
        adlak_os_free_pages((struct page **)mm_info->pages, mm_info->nr_pages);
        adlak_os_free(mm_info->phys_addrs);
        adlak_os_free(mm_info->pages);
    }
}

int adlak_os_alloc_discontiguous(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    struct page **pages      = NULL;
    void *        cpu_addr   = NULL;
    phys_addr_t * phys_addrs = NULL;
    int           i;
    size_t        size = mm_info->req.bytes;
    gfp_t         gfp  = 0;
    AML_LOG_DEBUG("%s", __func__);

    pages = (struct page **)adlak_os_zalloc(sizeof(struct page *) * mm_info->nr_pages,
                                            ADLAK_GFP_KERNEL);
    if (!pages) {
        goto err_alloc_pages;
    }
    mm_info->pages = (void *)pages;

    phys_addrs =
        (phys_addr_t *)adlak_os_zalloc(sizeof(phys_addr_t) * mm_info->nr_pages, ADLAK_GFP_KERNEL);
    if (unlikely(!phys_addrs)) {
        goto err_alloc_phys_addrs;
    }
    mm_info->phys_addrs = phys_addrs;

    gfp |= (GFP_DMA | GFP_USER | __GFP_ZERO);
    if (ADLAK_ENUM_MEMTYPE_INNER_PA_WITHIN_4G & mm_info->req.mem_type) {
        gfp |= (__GFP_DMA32);
    }

    for (i = 0; i < mm_info->nr_pages; ++i) {
        pages[i] = alloc_page(gfp);
        if (unlikely(!pages[i])) {
            goto err_alloc_page;
        }
        phys_addrs[i] = page_to_phys(pages[i]);  // get physical addr
    }

    if (adlak_flush_cache_init(mm, mm_info, 0, size)) {
        goto err_flush_init;
    }

    /**make a long duration mapping of multiple physical pages into a contiguous virtual space**/
    if (mm_info->mem_type & ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE) {
        cpu_addr = vmap(pages, mm_info->nr_pages, VM_MAP, PAGE_KERNEL);

    } else {
        cpu_addr = vmap(pages, mm_info->nr_pages, VM_MAP, pgprot_writecombine(PAGE_KERNEL));
    }
    if (!cpu_addr) {
        goto err_vmap;
    }
    mm_info->cpu_addr = cpu_addr;

    AML_LOG_DEBUG("%s: PA=0x%llx,VA_kernel=0x%lX", __FUNCTION__, (uint64_t)phys_addrs[0],
                  (uintptr_t)cpu_addr);

    mm_info->phys_addr = -1;  // the phys is not contiguous
    // adlak_debug_mem_fill_as_address(mm_info);
    // adlak_debug_mem_dump(mm_info);

    return ERR(NONE);
err_vmap:
    adlak_flush_cache_destroy(mm, mm_info);
err_flush_init:
err_alloc_page:
    adlak_os_free(phys_addrs);
    adlak_os_free_pages(pages, i);
err_alloc_phys_addrs:
    adlak_os_free(pages);
err_alloc_pages:

    return ERR(ENOMEM);
}

void adlak_os_free_contiguous(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    AML_LOG_DEBUG("%s", __func__);

    if (mm_info->cpu_addr) {
        /* ummap kernel space */
        vunmap(mm_info->cpu_addr);
        adlak_flush_cache_destroy2(mm, mm_info);
        adlak_os_free_pages_contiguous((struct page **)mm_info->pages, mm_info->nr_pages);
        adlak_os_free(mm_info->phys_addrs);
        adlak_os_free(mm_info->pages);
    }
}

int adlak_os_alloc_contiguous(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    struct page **pages         = NULL;
    struct page * page_continue = NULL;
    void *        cpu_addr      = NULL;
    phys_addr_t * phys_addrs    = NULL;
    int           i, order;
    size_t        size = mm_info->req.bytes;
    gfp_t         gfp  = 0;

    AML_LOG_DEBUG("%s", __func__);

    pages = (struct page **)adlak_os_zalloc(sizeof(struct page *) * mm_info->nr_pages,
                                            ADLAK_GFP_KERNEL);
    if (!pages) {
        goto err_alloc_pages;
    }
    mm_info->pages = (void *)pages;

    phys_addrs =
        (phys_addr_t *)adlak_os_zalloc(sizeof(phys_addr_t) * mm_info->nr_pages, ADLAK_GFP_KERNEL);
    if (!phys_addrs) {
        goto err_alloc_phys_addrs;
    }
    mm_info->phys_addrs = phys_addrs;

    order = get_order(ADLAK_PAGE_ALIGN(size));
    if (order >= MAX_ORDER) {
        AML_LOG_WARN("contiguous alloc contiguous memory order is bigger than MAX, %d >= %d\n",
                     order, MAX_ORDER);
        goto err_order;
    }
    gfp |= (GFP_DMA | GFP_USER | __GFP_ZERO);
    if (ADLAK_ENUM_MEMTYPE_INNER_PA_WITHIN_4G & mm_info->req.mem_type) {
        gfp |= (__GFP_DMA32);
    }
    page_continue = alloc_pages(gfp, order);
    if (unlikely(!page_continue)) {
        AML_LOG_ERR("alloc_pages %d fail", order);
        goto err_alloc_page;
    }

    AML_LOG_DEBUG("page_continue=0x%lX", (uintptr_t)page_continue);
    phys_addrs[0] = page_to_phys(page_continue);
    for (i = 0; i < mm_info->nr_pages; ++i) {
        phys_addrs[i] = phys_addrs[0] + i * ADLAK_PAGE_SIZE;  // get physical addr
        pages[i]      = phys_to_page(phys_addrs[i]);
    }

    mm_info->phys_addr = phys_addrs[0];

    if (adlak_flush_cache_init2(mm, mm_info, 0, size, page_continue)) {
        goto err_flush_init;
    }

    if (mm_info->mem_type & ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE) {
        cpu_addr = vmap(pages, mm_info->nr_pages, VM_MAP, PAGE_KERNEL);
    } else {
        cpu_addr = vmap(pages, mm_info->nr_pages, VM_MAP, pgprot_writecombine(PAGE_KERNEL));
    }
    if (!cpu_addr) {
        goto err_vmap;
    }

    mm_info->cpu_addr = cpu_addr;

    AML_LOG_DEBUG("%s: PA=0x%lX,VA_kernel=0x%lX", __FUNCTION__, (uintptr_t)mm_info->phys_addr,
                  (uintptr_t)mm_info->cpu_addr);

    // adlak_debug_mem_fill_as_address(&mm_info);
    // adlak_debug_mem_dump(&mm_info);

    return ERR(NONE);
err_vmap:
    adlak_flush_cache_destroy2(mm, mm_info);
err_flush_init:
    adlak_os_free_pages_contiguous(pages, mm_info->nr_pages);

err_alloc_page:
err_order:
    adlak_os_free(phys_addrs);

err_alloc_phys_addrs:
    adlak_os_free(pages);
err_alloc_pages:

    return ERR(ENOMEM);
}

int adlak_os_attach_ext_mem_phys(struct adlak_mem *mm, struct adlak_mem_handle *mm_info,
                                 uint64_t phys_addr) {
    phys_addr_t *phys_addrs = NULL;
    void *       cpu_addr   = NULL;
    int          i;
    AML_LOG_DEBUG("%s", __func__);
    AML_LOG_DEBUG("phys_addr:0x%lX, size:0x%lX", (uintptr_t)phys_addr,
                  (uintptr_t)mm_info->req.bytes);

    mm_info->phys_addrs = NULL;
    phys_addrs =
        (phys_addr_t *)adlak_os_zalloc(sizeof(phys_addr_t) * mm_info->nr_pages, ADLAK_GFP_KERNEL);
    if (!phys_addrs) {
        goto err_alloc_phys_addrs;
    }

    mm_info->phys_addr = phys_addr;
    for (i = 0; i < mm_info->nr_pages; ++i) {
        phys_addrs[i] = phys_addr + (i * ADLAK_PAGE_SIZE);  // get physical addr
        AML_LOG_DEBUG("phys_addrs[%d]=0x%llx", i, (uint64_t)(uintptr_t)phys_addrs[i]);
    }
    mm_info->pages      = NULL;
    mm_info->phys_addrs = phys_addrs;
    mm_info->cpu_addr   = cpu_addr;
    mm_info->dma_addr   = (dma_addr_t)NULL;

    return ERR(NONE);
err_alloc_phys_addrs:

    return ERR(ENOMEM);
}

void adlak_os_dettach_ext_mem_phys(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    AML_LOG_DEBUG("%s", __func__);
    if (mm_info->phys_addrs) {
        adlak_os_free(mm_info->phys_addrs);
    }
}

int adlak_os_mmap(struct adlak_mem *mm, struct adlak_mem_handle *mm_info, void *const _vma) {
    unsigned long                addr;
    unsigned long                pfn;
    int                          i;
    struct vm_area_struct *const vma = (struct vm_area_struct *const)_vma;
    if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_OS) {
        if (mm_info->mem_type & ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE) {
            // vma->vm_page_prot = vma->vm_page_prot;
        } else {
            vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
        }
        if (mm_info->mem_type & ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS) {
            remap_pfn_range(vma, vma->vm_start, page_to_pfn(phys_to_page(mm_info->phys_addr)),
                            vma->vm_end - vma->vm_start, vma->vm_page_prot);
            AML_LOG_DEBUG("%s contiguous  phys_addr = 0x%lX", __func__,
                          (uintptr_t)mm_info->phys_addr);
        } else {
            AML_LOG_DEBUG("%s discontiguous phys_addr = 0x%lX", __func__,
                          (uintptr_t)mm_info->phys_addr);
            for (i = 0; i < mm_info->nr_pages; ++i) {
                addr = vma->vm_start + i * ADLAK_PAGE_SIZE;
                pfn  = page_to_pfn(((struct page **)(mm_info->pages))[i]);
                if (remap_pfn_range(vma, addr, pfn, ADLAK_PAGE_SIZE, vma->vm_page_prot)) {
                    return ERR(EAGAIN);
                }
            }
        }
    } else if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_RESERVED) {
        remap_pfn_range(vma, vma->vm_start, page_to_pfn(phys_to_page(mm_info->phys_addr)),
                        mm_info->req.bytes, vma->vm_page_prot);

    } else if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_CMA) {
        dma_mmap_coherent(mm->dev, vma, mm_info->cpu_addr, mm_info->dma_addr, mm_info->req.bytes);
    } else if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_EXT_PHYS) {
        vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);  // mmap as nocacheable
        remap_pfn_range(vma, vma->vm_start, page_to_pfn(phys_to_page(mm_info->phys_addr)),
                        vma->vm_end - vma->vm_start, vma->vm_page_prot);
        AML_LOG_DEBUG("%s ext phys buffer,  phys_addr = 0x%lX", __func__,
                      (uintptr_t)mm_info->phys_addr);
    }

    return 0;
}

void adlak_os_flush_cache(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    struct sg_table *sgt = NULL;
    AML_LOG_DEBUG("%s", __func__);

    if (ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE & mm_info->mem_type) {
        if (ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS & mm_info->mem_type) {
            dma_sync_single_for_device(mm->dev, mm_info->dma_addr, mm_info->req.bytes,
                                       DMA_TO_DEVICE);
        } else {
            sgt = (struct sg_table *)mm_info->sgt;
            dma_sync_sg_for_device(mm->dev, sgt->sgl, sgt->nents, DMA_TO_DEVICE);
        }
    }
}

void adlak_os_invalid_cache(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    struct sg_table *sgt = NULL;
    AML_LOG_DEBUG("%s", __func__);
    if (ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE & mm_info->mem_type) {
        if (ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS & mm_info->mem_type) {
            dma_sync_single_for_cpu(mm->dev, mm_info->dma_addr, mm_info->req.bytes,
                                    DMA_FROM_DEVICE);
        } else {
            sgt = (struct sg_table *)mm_info->sgt;
            dma_sync_sg_for_cpu(mm->dev, sgt->sgl, sgt->nents, DMA_FROM_DEVICE);
        }
    }
}

void adlak_free_through_dma(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    if (mm_info->cpu_addr) {
        dma_free_coherent(mm->dev, mm_info->req.bytes, mm_info->cpu_addr, mm_info->dma_addr);
    }
}

int adlak_malloc_through_dma(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    // uncacheable

    mm_info->cpu_addr = dma_alloc_coherent(mm->dev, (size_t)mm_info->req.bytes, &mm_info->dma_addr,
                                           GFP_USER | GFP_DMA | GFP_DMA32);
    if (!mm_info->cpu_addr) {
        AML_LOG_ERR("failed to dma_alloc %lu bytes\n", (uintptr_t)mm_info->req.bytes);
        return ERR(ENOMEM);
    }
    mm_info->phys_addr = dma_to_phys(mm->dev, mm_info->dma_addr);
    mm_info->mem_src   = ADLAK_ENUM_MEMSRC_CMA;
    mm_info->mem_type  = ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS;  // uncacheable|contiguous
    mm_info->iova_addr = mm_info->phys_addr;
    AML_LOG_DEBUG(
        "%s: dma_addr= 0x%lX,  phys_addr= "
        "0x%lX,size=%lu KByte\n",
        __FUNCTION__, (uintptr_t)mm_info->dma_addr, (uintptr_t)mm_info->phys_addr,
        (uintptr_t)(mm_info->req.bytes / 1024));
    return ERR(NONE);
}

int adlak_os_mmap2userspace(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    int                    ret            = ERR(EINVAL);
    unsigned long          addr_userspace = 0;
    struct vm_area_struct *vma            = NULL;
    AML_LOG_DEBUG("%s", __func__);

    if (ADLAK_IS_ERR_OR_NULL(mm_info)) goto exit;
    addr_userspace = vm_mmap(NULL, 0L, mm_info->req.bytes, PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_NORESERVE, 0);

    AML_LOG_DEBUG("vm_mmap addr = 0x%lX", addr_userspace);
    if (ADLAK_IS_ERR_OR_NULL((void *)addr_userspace)) {
        ret = ERR(ENOMEM);
        AML_LOG_ERR("Unable to mmap process text, errno %d\n", ret);
        addr_userspace = 0;
        goto exit;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    mmap_write_lock(current->mm);
#else
    down_write(&current->mm->mmap_sem);
#endif

    vma = find_vma(current->mm, (unsigned long)addr_userspace);
    if (NULL == vma) {
        AML_LOG_ERR("failed to find vma\n");
        vm_munmap(addr_userspace, mm_info->req.bytes);
        addr_userspace = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
        mmap_write_unlock(current->mm);
#else
        up_write(&current->mm->mmap_sem);
#endif
        goto exit;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
    vma->vm_flags |= (VM_IO | VM_DONTCOPY | VM_DONTEXPAND | VM_DONTDUMP);
#else
    vma->vm_flags |= (VM_IO | VM_DONTCOPY | VM_DONTEXPAND | VM_RESERVED);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    mmap_write_unlock(current->mm);
#else
    up_write(&current->mm->mmap_sem);
#endif

    ret = adlak_os_mmap(mm, mm_info, (void *)vma);

exit:
    mm_info->cpu_addr_user = (void *)addr_userspace;

    AML_LOG_DEBUG("cpu_addr_user = 0x%lX", addr_userspace);

    return ret;
}

void adlak_os_unmmap_userspace(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    unsigned long addr_userspace = 0;
    AML_LOG_DEBUG("%s", __func__);

    if (unlikely(!current->mm)) {
        /* the task is exiting. */
        return;
    }

    AML_LOG_DEBUG("%s cpu_addr_user = 0x%lX", __func__, (uintptr_t)mm_info->cpu_addr_user);
    addr_userspace = (unsigned long)mm_info->cpu_addr_user;
    if (addr_userspace) {
        AML_LOG_DEBUG("unmap user space addr=0x%lX", addr_userspace);
        vm_munmap(addr_userspace, mm_info->req.bytes);
        mm_info->cpu_addr_user = 0;
    }
}
