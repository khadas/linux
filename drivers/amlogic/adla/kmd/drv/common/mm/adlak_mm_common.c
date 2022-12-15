/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_mm_common.c
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2022/07/13	Initial release
 * </pre>
 *
 ******************************************************************************/

/***************************** Include Files *********************************/
#include "adlak_mm_common.h"

#include "adlak_mm_mbp.h"
#include "adlak_mm_os_common.h"
#include "adlak_mm_smmu.h"
#include "adlak_mm_smmu_tlb.h"
/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/

/************************** Function Prototypes ******************************/

static int adlak_malloc_contiguous_from_system(struct adlak_mem *       mm,
                                               struct adlak_mem_handle *mm_info);
static int adlak_malloc_discontiguous_from_system(struct adlak_mem *       mm,
                                                  struct adlak_mem_handle *mm_info);

static int adlak_malloc_from_mem_pool(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

static void adlak_free_to_mem_pool(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

static void adlak_free_uncacheable(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

static int  adlak_malloc_uncacheable(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);
static void adlak_free_cacheable(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);
static int  adlak_malloc_cacheable(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

#if CONFIG_ADLAK_MEM_POOL_EN
static int adlak_mem_pools_mgr_init(struct adlak_mem *mm);

static void adlak_mem_pools_mgr_deinit(struct adlak_mem *mm);

static int adlak_mem_pools_mgr_init(struct adlak_mem *mm) {
    size_t pool_size = mm->mem_pool->size;
    AML_LOG_DEBUG("%s", __func__);

    mm->usage.mem_pools_size += pool_size;

    return adlak_bitmap_pool_init(&mm->mem_pool->bitmap_area, pool_size);
}

static void adlak_mem_pools_mgr_deinit(struct adlak_mem *mm) {
    adlak_bitmap_pool_deinit(&mm->mem_pool->bitmap_area);
}

#endif

int adlak_bitmap_pool_init(struct adlak_mm_pool_priv *pool, size_t pool_size) {
    size_t bitmap_size;
    AML_LOG_DEBUG("%s", __func__);
    bitmap_size  = BITS_TO_LONGS(pool_size >> ADLAK_MM_POOL_PAGE_SHIFT) * sizeof(unsigned long);
    pool->bitmap = adlak_os_vzalloc(bitmap_size, ADLAK_GFP_KERNEL);
    if (!pool->bitmap) {
        return ERR(ENOMEM);
    }
    pool->addr_base  = 0;
    pool->used_count = 0;
    pool->bits       = (pool_size >> ADLAK_MM_POOL_PAGE_SHIFT);

    AML_LOG_INFO(
        "mm_pool size = %lu Mbytes, bitmap_size = %lu bytes, bitmap_maxno = %lu, "
        "pool_addr_base=0x%lX.",
        (uintptr_t)pool_size / (1024 * 1024), (uintptr_t)bitmap_size, (uintptr_t)pool->bits,
        (uintptr_t)pool->addr_base);
    adlak_os_mutex_init(&pool->lock);
    return 0;
}

void adlak_bitmap_pool_deinit(struct adlak_mm_pool_priv *pool) {
    adlak_os_mutex_lock(&pool->lock);
    adlak_os_mutex_unlock(&pool->lock);
    adlak_os_mutex_destroy(&pool->lock);
    if (pool->bitmap) {
        adlak_os_vfree(pool->bitmap);
        pool->bitmap = NULL;
    }
}

void adlak_bitmap_dump(struct adlak_mm_pool_priv *pool) {
#if ADLAK_DEBUG
    size_t bitmap_size = pool->bits / BITS_PER_BYTE;
    char * addr        = pool->bitmap;
    size_t i;

    AML_LOG_WARN("%s", __func__);
    if (bitmap_size > (1024 * 1024 * 1024 / 4096 / BITS_PER_BYTE))
        bitmap_size = (1024 * 1024 * 1024 / 4096 / BITS_PER_BYTE);
    for (i = 0; i < bitmap_size; i++) {
        AML_LOG_WARN(
            "%02X %02X %02X %02X %02X %02X %02X %02X "
            "%02X %02X %02X %02X %02X %02X %02X %02X "
            "%02X %02X %02X %02X %02X %02X %02X %02X "
            "%02X %02X %02X %02X %02X %02X %02X %02X ",
            addr[i + 0], addr[i + 1], addr[i + 2], addr[i + 3], addr[i + 4], addr[i + 5],
            addr[i + 6], addr[i + 7], addr[i + 8], addr[i + 9], addr[i + 10], addr[i + 11],
            addr[i + 12], addr[i + 13], addr[i + 14], addr[i + 15], addr[i + 16], addr[i + 17],
            addr[i + 18], addr[i + 19], addr[i + 20], addr[i + 21], addr[i + 22], addr[i + 23],
            addr[i + 24], addr[i + 25], addr[i + 26], addr[i + 27], addr[i + 28], addr[i + 29],
            addr[i + 30], addr[i + 31]);
        i += 32;
    }
#endif
}

unsigned long adlak_alloc_from_bitmap_pool(struct adlak_mm_pool_priv *pool, size_t size) {
    size_t        start = 0;
    size_t        bitmap_count;
    unsigned long addr_offset = ERR(ENOMEM);
    if (!pool || size == 0) {
        return ERR(ENOMEM);
    }
    size         = ADLAK_ALIGN(size, ADLAK_MM_POOL_PAGE_SIZE);
    bitmap_count = size >> ADLAK_MM_POOL_PAGE_SHIFT;
    adlak_os_mutex_lock(&pool->lock);
    if (bitmap_count > (pool->bits - pool->used_count)) {
        start = SMMU_IOVA_ADDR_SIZE;
        AML_LOG_WARN(
            "Didn't find zero area from mem-pool!\nbitmap_maxno = %lu,bitmap_usedno = "
            "%lu,bitmap_count = %lu",
            (uintptr_t)pool->bits, (uintptr_t)pool->used_count, (uintptr_t)bitmap_count);
        goto ret;
    }
    start = bitmap_find_next_zero_area(pool->bitmap, pool->bits, 0, bitmap_count, 0);
    if (start > pool->bits) {
        start = SMMU_IOVA_ADDR_SIZE;
        AML_LOG_WARN(
            "Didn't find zero area from mem-pool!\nbitmap_maxno = %lu,bitmap_usedno = "
            "%lu,bitmap_count = %lu",
            (uintptr_t)pool->bits, (uintptr_t)pool->used_count, (uintptr_t)bitmap_count);
        goto ret;
    }
    pool->used_count += bitmap_count;
    AML_LOG_DEBUG("start = %lu, bitmap_maxno = %lu,bitmap_usedno = %lu,bitmap_count = %lu",
                  (uintptr_t)start, (uintptr_t)pool->bits, (uintptr_t)pool->used_count,
                  (uintptr_t)bitmap_count);
    bitmap_set(pool->bitmap, start, bitmap_count);
    addr_offset = (unsigned long)(pool->addr_base + start * ADLAK_MM_POOL_PAGE_SIZE);
ret:
    if (SMMU_IOVA_ADDR_SIZE == start) {
        adlak_bitmap_dump(pool);
    }
    adlak_os_mutex_unlock(&pool->lock);

    return addr_offset;
}

void adlak_free_to_bitmap_pool(struct adlak_mm_pool_priv *pool, dma_addr_t start, size_t size) {
    int bitmap_count;

    if (!pool || ADLAK_IS_ERR((void *)start)) {
        return;
    }
    size         = ADLAK_ALIGN(size, ADLAK_MM_POOL_PAGE_SIZE);
    bitmap_count = size >> ADLAK_MM_POOL_PAGE_SHIFT;
    adlak_os_mutex_lock(&pool->lock);
    bitmap_clear(pool->bitmap, (start - pool->addr_base) / ADLAK_MM_POOL_PAGE_SIZE, bitmap_count);
    pool->used_count -= bitmap_count;
    adlak_os_mutex_unlock(&pool->lock);
}

int adlak_mm_init(struct adlak_device *padlak) {
    int               ret = 0;
    struct adlak_mem *mm  = NULL;

    AML_LOG_DEBUG("%s", __func__);
    mm = adlak_os_zalloc(sizeof(struct adlak_mem), ADLAK_GFP_KERNEL);
    if (ADLAK_IS_ERR_OR_NULL(mm)) {
        ret = -1;
        goto err_alloc;
    }
    padlak->mm       = mm;
    mm->padlak       = padlak;
    mm->use_smmu     = padlak->smmu_en;
    mm->has_mem_pool = false;

#ifndef CONFIG_ADLA_FREERTOS
    mm->dev = padlak->dev;
#endif
    mm->usage.mem_alloced_kmd = 0;
    mm->usage.mem_alloced_umd = 0;
    mm->usage.mem_pools_size  = -1;

    if (!mm->use_smmu) {
#ifndef CONFIG_ADLA_FREERTOS
        ret = adlak_cma_init(padlak->dev);
        if (ret) {
            ret = -1;
            goto err;
        }
#endif
#if CONFIG_ADLAK_MEM_POOL_EN
#if defined(CONFIG_ADLAK_USE_MBP)
        // Create unCacheable memory pool from MBP
        if (0 != adlak_create_mem_pool_from_mbp_uncache(mm)) {
            ret = -1;
            goto err;
        }
#elif defined(CONFIG_ADLAK_USE_RESERVED_MEMORY)
        // Create unCacheable memory pool from reserved memory
        if (0 != adlak_remap_region_nocache(mm)) {
            ret = -1;
            goto err;
        }
#else
#if defined(CONFIG_ADLA_FREERTOS)
#error "Not support CMA in freertos.Please change with reserved_memory or others"
#endif
        // Create unCacheable memory pool from CMA
        if (0 != adlak_alloc_cma_region_nocache(mm)) {
            ret = -1;
            goto err;
        }
#endif
        if (!ret) {
            mm->has_mem_pool = true;
        }
        if (mm->has_mem_pool) {
            mm->usage.mem_pools_size = 0;
            // init bitmap for memory pools
            if (0 != adlak_mem_pools_mgr_init(mm)) {
                goto err;
            }
        }
#endif
    } else {
        adlak_smmu_addr_pool_init(mm);
        adlak_smmu_init(mm);
    }
    return ERR(NONE);

err:

    adlak_mm_deinit(padlak);
err_alloc:
    return ret;
}

void adlak_mm_deinit(struct adlak_device *padlak) {
    struct adlak_mem *mm = padlak->mm;
    if (ADLAK_IS_ERR_OR_NULL(mm)) {
        return;
    }
    if (!mm->use_smmu) {
#if CONFIG_ADLAK_MEM_POOL_EN
        if (mm->has_mem_pool) {
            adlak_mem_pools_mgr_deinit(mm);
            mm->usage.mem_pools_size = 0;
        }

#if defined(CONFIG_ADLAK_USE_MBP)
        // destroy unCacheable memory pool from MBP
        adlak_destroy_mem_pool_from_mbp_uncache(mm);
#elif defined(CONFIG_ADLAK_USE_RESERVED_MEMORY)
        // destroy unCacheable memory pool from reserved memory
        adlak_unmap_region_nocache(mm);
#else
        // destroy unCacheable memory pool from CMA
        adlak_free_cma_region_nocache(mm);
#endif

#endif

#ifndef CONFIG_ADLA_FREERTOS
        adlak_cma_deinit(padlak->dev);
#endif
    } else {
        adlak_smmu_deinit(mm);
        adlak_smmu_addr_pool_deinit(mm);
    }

    adlak_os_free(mm);
    padlak->mm = NULL;
}

int adlak_flush_cache(struct adlak_device *padlak, struct adlak_mem_handle *mm_info) {
    struct adlak_mem *mm = padlak->mm;

    AML_LOG_DEBUG("%s", __func__);
    if (ADLAK_IS_ERR_OR_NULL(mm_info)) {
        return -1;
    }
    if (0 == (mm_info->mem_type & ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE)) {
        return 0;
    }

    AML_LOG_DEBUG("%s cpu_addr 0x%lX", __func__, (uintptr_t)mm_info->cpu_addr);
    if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_OS) {
        adlak_os_flush_cache(mm, mm_info);
    } else if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_RESERVED) {
        // TODO
    } else if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_MBP) {
        // TODO
    } else if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_CMA) {
        // nothing need to do.
    } else if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_EXT_PHYS) {
        // nothing need to do.
    }
    return 0;
}

int adlak_invalid_cache(struct adlak_device *padlak, struct adlak_mem_handle *mm_info) {
    struct adlak_mem *mm = padlak->mm;

    AML_LOG_DEBUG("%s", __func__);
    if (ADLAK_IS_ERR_OR_NULL(mm_info)) {
        return -1;
    }
    if (0 == (mm_info->mem_type & ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE)) {
        return 0;
    }
    AML_LOG_DEBUG("%s cpu_addr 0x%lX", __func__, (uintptr_t)mm_info->cpu_addr);
    if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_OS) {
        adlak_os_invalid_cache(mm, mm_info);
    } else if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_RESERVED) {
        // TODO
    } else if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_MBP) {
        // TODO
    } else if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_CMA) {
        // nothing need to do.
    } else if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_EXT_PHYS) {
        // nothing need to do.
    }
    return 0;
}

static void adlak_free_contiguous_to_system(struct adlak_mem *       mm,
                                            struct adlak_mem_handle *mm_info) {
    adlak_os_free_contiguous(mm, mm_info);
}

static int adlak_malloc_contiguous_from_system(struct adlak_mem *       mm,
                                               struct adlak_mem_handle *mm_info) {
    int ret;
    mm_info->mem_src  = ADLAK_ENUM_MEMSRC_OS;
    mm_info->mem_type = ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS;
    if (mm_info->req.mem_type & ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE) {
        // cacheable
        mm_info->mem_type = mm_info->mem_type | ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE;
    } else {
        // uncacheable
    }
    ret = adlak_os_alloc_contiguous(mm, mm_info);
    return ret;
}

static void adlak_free_discontiguous_to_system(struct adlak_mem *       mm,
                                               struct adlak_mem_handle *mm_info) {
    adlak_os_free_discontiguous(mm, mm_info);
}

static int adlak_malloc_discontiguous_from_system(struct adlak_mem *       mm,
                                                  struct adlak_mem_handle *mm_info) {
    int ret;
    mm_info->mem_src  = ADLAK_ENUM_MEMSRC_OS;
    mm_info->mem_type = 0;
    if (mm_info->req.mem_type & ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE) {
        mm_info->mem_type = mm_info->mem_type | ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE;
        // cacheable
    } else {
        // uncacheable
    }
    ret = adlak_os_alloc_discontiguous(mm, mm_info);
    return ret;
}

static int adlak_malloc_from_mem_pool(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    unsigned long start;
    AML_LOG_DEBUG("%s", __func__);

    start = adlak_alloc_from_bitmap_pool(&mm->mem_pool->bitmap_area, (size_t)mm_info->req.bytes);
    if (ADLAK_IS_ERR((void *)start)) {
        mm_info->phys_addr = 0;
        return ERR(ENOMEM);
    }
    mm_info->mem_type = ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS;
    if (mm->mem_pool->cacheable) {
        mm_info->mem_type = mm_info->mem_type | ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE;
    }

    mm_info->mem_src   = mm->mem_pool->mem_src;
    mm_info->cpu_addr  = (void *)((dma_addr_t)mm->mem_pool->cpu_addr_base + (dma_addr_t)start);
    mm_info->dma_addr  = mm->mem_pool->dma_addr_base + (dma_addr_t)start;
    mm_info->phys_addr = mm->mem_pool->phys_addr_base + (phys_addr_t)start;

    mm_info->iova_addr = mm_info->phys_addr;

    return ERR(NONE);
}

static void adlak_free_to_mem_pool(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    unsigned long start;
    AML_LOG_DEBUG("%s", __func__);
    if (mm_info->phys_addr) {
        start = mm_info->phys_addr - mm->mem_pool->phys_addr_base;
        adlak_free_to_bitmap_pool(&mm->mem_pool->bitmap_area, start, (size_t)mm_info->req.bytes);
    }
    mm_info->phys_addr = 0;
}

void adlak_free_uncacheable(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    AML_LOG_DEBUG("%s", __func__);

    if (mm->use_smmu) {
        if (mm_info->mem_type & ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS) {
            // free uncacheable contiguous memory
            return adlak_free_contiguous_to_system(mm, mm_info);
        } else {
            // free uncacheable discontiguous memory
            return adlak_free_discontiguous_to_system(mm, mm_info);
        }
    } else {
        if (mm->has_mem_pool) {
            // free uncacheable memory from memory pool
            return adlak_free_to_mem_pool(mm, mm_info);
        } else {
            // free uncacheable memory through dma api
            return adlak_free_through_dma(mm, mm_info);
        }
    }
}

static int adlak_malloc_uncacheable(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    int ret;
    AML_LOG_DEBUG("%s", __func__);

    if (mm->use_smmu) {
        if (mm_info->req.mem_type & ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS) {
            // alloc uncacheable contiguous memory
            return adlak_malloc_contiguous_from_system(mm, mm_info);
        } else {
            // alloc uncacheable discontiguous memory
            return adlak_malloc_discontiguous_from_system(mm, mm_info);
        }
    } else {
        if (mm->has_mem_pool) {
            // alloc uncacheable memory from memory pool
            ret = adlak_malloc_from_mem_pool(mm, mm_info);
            if (ERR(NONE) != ret) {
                // alloc uncacheable memory through dma api
                return adlak_malloc_through_dma(mm, mm_info);
            } else {
                return ret;
            }
        } else {
            // alloc uncacheable memory through dma api
            return adlak_malloc_through_dma(mm, mm_info);
        }
    }
}

void adlak_free_cacheable(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    AML_LOG_DEBUG("%s", __func__);
    if (mm->use_mbp) {
        // free cacheable memory through MBP api
        return adlak_free_through_mbp(mm, mm_info);
    } else {
        // free cacheable memory from system by myself
        if (mm_info->mem_type & ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS) {
            // free cacheable contiguous memory
            return adlak_free_contiguous_to_system(mm, mm_info);
        } else {
            // free cacheable discontiguous memory
            return adlak_free_discontiguous_to_system(mm, mm_info);
        }
    }
}

static int adlak_malloc_cacheable(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    int ret;
    AML_LOG_DEBUG("%s", __func__);
    if (mm->use_mbp) {
        // alloc cacheable memory through MBP api
        ret = adlak_malloc_through_mbp(mm, mm_info);
    } else {
        // alloc cacheable memory from system by myself
        if (mm_info->req.mem_type & ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS) {
            // alloc cacheable contiguous memory
            ret = adlak_malloc_contiguous_from_system(mm, mm_info);
        } else {
            // alloc cacheable discontiguous memory
            ret = adlak_malloc_discontiguous_from_system(mm, mm_info);
        }
    }
    AML_LOG_DEBUG("%s cpu_addr 0x%lX", __func__, (uintptr_t)mm_info->cpu_addr);
    return ret;
}

void adlak_mm_free(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    ASSERT(mm_info);
    if (mm->use_smmu) {
        adlak_smmu_iova_unmap(mm, mm_info);
    }

    if (mm_info->mem_type & ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE) {
        adlak_free_cacheable(mm, mm_info);
    } else {
        adlak_free_uncacheable(mm, mm_info);
    }
    adlak_os_free(mm_info);
}

static void adlak_mm_rewrite_memtype(struct adlak_mem *mm, struct adlak_buf_req *pbuf_req,
                                     struct adlak_mem_handle *mm_info) {
    mm_info->req.mem_type = 0;

    if (pbuf_req->ret_desc.mem_type & ADLAK_ENUM_MEMTYPE_CACHEABLE) {
        mm_info->req.mem_type = mm_info->req.mem_type | ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE;
    }
    if (!(pbuf_req->ret_desc.mem_type & ADLAK_ENUM_MEMTYPE_INNER)) {
        mm_info->req.mem_type = mm_info->req.mem_type | ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS;
    } else {
        if (pbuf_req->ret_desc.mem_type & ADLAK_ENUM_MEMTYPE_CONTIGUOUS) {
            mm_info->req.mem_type = mm_info->req.mem_type | ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS;
        }
    }
    if (!mm->use_smmu) {
        mm_info->req.mem_type = mm_info->req.mem_type | ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS;
    }
    if (pbuf_req->ret_desc.mem_type & ADLAK_ENUM_MEMTYPE_PA_WITHIN_4G) {
        mm_info->req.mem_type = mm_info->req.mem_type | ADLAK_ENUM_MEMTYPE_INNER_PA_WITHIN_4G;
    }
}

static void adlak_mm_update_memtype(struct adlak_mem *mm, struct adlak_buf_req *pbuf_req,
                                    struct adlak_mem_handle *mm_info) {
    pbuf_req->ret_desc.mem_type = 0;

    if (mm_info->mem_type | ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE) {
        pbuf_req->ret_desc.mem_type = pbuf_req->ret_desc.mem_type | ADLAK_ENUM_MEMTYPE_CACHEABLE;
    }
    if (mm_info->mem_type | ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS) {
        pbuf_req->ret_desc.mem_type = pbuf_req->ret_desc.mem_type | ADLAK_ENUM_MEMTYPE_CONTIGUOUS;
    }
}

struct adlak_mem_handle *adlak_mm_alloc(struct adlak_mem *mm, struct adlak_buf_req *pbuf_req) {
    int                      ret;
    struct adlak_mem_handle *mm_info;
    if (!pbuf_req->bytes) {
        return (void *)NULL;
    }
    mm_info = adlak_os_zalloc(sizeof(struct adlak_mem_handle), ADLAK_GFP_KERNEL);
    if (unlikely(!mm_info)) {
        goto end;
    }
    mm_info->req.bytes         = ADLAK_PAGE_ALIGN(pbuf_req->bytes);
    mm_info->req.mem_direction = pbuf_req->ret_desc.mem_direction;

    adlak_mm_rewrite_memtype(mm, pbuf_req, mm_info);

    mm_info->nr_pages = ADLAK_DIV_ROUND_UP(mm_info->req.bytes, ADLAK_PAGE_SIZE);
    mm_info->cpu_addr = NULL;
    if (mm_info->req.mem_type & ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE) {
        ret = adlak_malloc_cacheable(mm, mm_info);
        if (ret) {
            // If malloc fails, then retry malloc as uncacheable
            mm_info->req.mem_type = (mm_info->req.mem_type & (~ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE));
            ret                   = adlak_malloc_uncacheable(mm, mm_info);
        }
    } else {
        ret = adlak_malloc_uncacheable(mm, mm_info);
    }
    if (ret) {
        AML_LOG_ERR("%s fail!", __FUNCTION__);
        goto err;
    }
    if (mm->use_smmu) {
        // update the tlb of smmu
        if (mm->padlak->smmu_entry) {
            ret = adlak_smmu_iova_map(mm, mm_info);
        }
    } else {
        mm_info->iova_addr = mm_info->phys_addr;
    }
    adlak_mm_update_memtype(mm, pbuf_req, mm_info);

err:
    if (!ret) {
        pbuf_req->errcode = 0;
    } else {
        pbuf_req->errcode = ret;
        adlak_mm_free(mm, mm_info);
        mm_info = NULL;
    }

end:
    return mm_info;
}

void adlak_mm_dettach(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    if (!mm_info) {
        return;
    }
    if (mm->use_smmu) {
        adlak_smmu_iova_unmap(mm, mm_info);
    }
    if (mm_info->phys_addrs) {
        adlak_os_free(mm_info->phys_addrs);
    }
    adlak_os_free(mm_info);
}

struct adlak_mem_handle *adlak_mm_attach(struct adlak_mem *            mm,
                                         struct adlak_extern_buf_info *pbuf_req) {
    int                      ret;
    struct adlak_mem_handle *mm_info   = NULL;
    uint64_t                 phys_addr = pbuf_req->buf_handle;
    size_t                   size      = pbuf_req->bytes;
    if (!size || !ADLAK_IS_ALIGNED((unsigned long)(size), ADLAK_PAGE_SIZE)) {
        goto early_exit;
    }
    mm_info = adlak_os_zalloc(sizeof(struct adlak_mem_handle), ADLAK_GFP_KERNEL);
    if (unlikely(!mm_info)) {
        goto end;
    }
    mm_info->req.bytes    = size;
    mm_info->req.mem_type = ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS;
    // set as uncacheable for exttern memory
    mm_info->req.mem_direction = pbuf_req->ret_desc.mem_direction;
    mm_info->req.mem_direction = pbuf_req->ret_desc.mem_direction;

    mm_info->nr_pages = ADLAK_DIV_ROUND_UP(mm_info->req.bytes, ADLAK_PAGE_SIZE);
    mm_info->cpu_addr = NULL;

    mm_info->mem_src = ADLAK_ENUM_MEMSRC_EXT_PHYS;
    ret              = adlak_os_attach_ext_mem_phys(mm, mm_info, phys_addr);

    if (!ret) {
        pbuf_req->errcode = 0;
    } else {
        pbuf_req->errcode = ret;
        adlak_os_free(mm_info);
        mm_info = (void *)NULL;
    }
    if (mm->use_smmu) {
        // update the tlb of smmu
        ret = adlak_smmu_iova_map(mm, mm_info);
    } else {
        mm_info->iova_addr = mm_info->phys_addr;
    }
end:
early_exit:

    return mm_info;
}

int adlak_mm_mmap(struct adlak_mem *mm, struct adlak_mem_handle *mm_info, void *const vma) {
    AML_LOG_DEBUG("%s", __func__);
    if (mm_info->mem_src != ADLAK_ENUM_MEMSRC_MBP) {
        adlak_os_mmap(mm, mm_info, vma);
    } else if (mm_info->mem_src == ADLAK_ENUM_MEMSRC_MBP) {
        // TODO
    }
    return 0;
}
