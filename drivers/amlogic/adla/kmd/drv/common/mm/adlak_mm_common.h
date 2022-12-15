/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_mm_common.h
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

#ifndef __ADLAK_MM_COMMON_H__
#define __ADLAK_MM_COMMON_H__

/***************************** Include Files *********************************/

#include "adlak_common.h"
#include "adlak_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

struct adlak_mm_pool_priv {
    void *           bitmap;
    dma_addr_t       addr_base;
    size_t           bits;  // bitmap_maxno
    size_t           used_count;
    adlak_os_mutex_t lock;
};

struct adlak_resv_mem {
    uint32_t                  mem_type;
    void *                    cpu_addr_base;
    phys_addr_t               phys_addr_base;
    dma_addr_t                dma_addr_base;
    size_t                    size;
    struct adlak_mm_pool_priv mm_pool;
};

enum adlak_mem_src {
    ADLAK_ENUM_MEMSRC_CMA = 0,
    ADLAK_ENUM_MEMSRC_RESERVED,
    ADLAK_ENUM_MEMSRC_OS,
    ADLAK_ENUM_MEMSRC_MBP,
    ADLAK_ENUM_MEMSRC_EXT_PHYS,
};

enum adlak_mem_type_inner {
    ADLAK_ENUM_MEMTYPE_INNER_CACHEABLE    = (1u << 0),
    ADLAK_ENUM_MEMTYPE_INNER_CONTIGUOUS   = (1u << 1),
    ADLAK_ENUM_MEMTYPE_INNER_PA_WITHIN_4G = (1u << 4)  // physical address less than 4Gbytes
};

struct adlak_mem_pool_info {
    uint32_t           cacheable;  //
    size_t             size;
    enum adlak_mem_src mem_src;  // RESERVED_MEM,CMA_MEM,MBP_MEM

    void *                    cpu_addr_base;
    phys_addr_t               phys_addr_base;
    dma_addr_t                dma_addr_base;
    struct adlak_mm_pool_priv bitmap_area;
};

struct adlak_mem_smmu {
    size_t                    size;
    struct adlak_mm_pool_priv bitmap_area;
};

/**
 *  statistics of memory usage
 */
struct adlak_mem_usage {
    size_t  used_umd;
    size_t  used_kmd;
    size_t  pool_size;
    size_t  mem_alloced_umd;
    size_t  mem_alloced_kmd;
    ssize_t mem_pools_size;
};

struct adlak_smmu_ops {
    int (*smmu_tlb_init)(struct adlak_mem *mm);
    int (*smmu_tlb_deinit)(struct adlak_mem *mm);
    int (*smmu_tlb_flush_cache)(struct adlak_mem *mm);
    int (*smmu_tlb_invalidate)(struct adlak_mem *mm);
};

struct adlak_mem_req_info {
    enum adlak_mem_type_inner mem_type;       // cacheable,contiguous
    enum adlak_mem_direction  mem_direction;  // rd&wr,rd,wr
    size_t                    bytes;          // align with 4096
};

struct adlak_mem_handle {
    struct adlak_mem_req_info req;
    int                       nr_pages;
    void *                    cpu_addr;
    void *                    cpu_addr_user;
    phys_addr_t               phys_addr;
    dma_addr_t                iova_addr;
    dma_addr_t                dma_addr;
    void *                    sgt;
    void *                    pages;
    phys_addr_t *             phys_addrs;
    enum dma_data_direction   direction;
    enum adlak_mem_src        mem_src;
    enum adlak_mem_type       mem_type;  // the mem_type may not same as request info
};

struct adlak_mem {
    int                         use_smmu;
    int                         use_mbp;
    int                         has_mem_pool;
    struct adlak_mem_usage      usage;
    struct adlak_device *       padlak;
    struct adlak_smmu_ops *     smmu_ops;
    struct adlak_mem_smmu *     smmu;
    struct adlak_mem_pool_info *mem_pool;
    struct device_node *        res_mem_dev;
    struct device *             dev;
};

#ifndef ADLAK_MM_POOL_PAGE_SHIFT
#define ADLAK_MM_POOL_PAGE_SHIFT (12)
#endif
#define ADLAK_MM_POOL_PAGE_SIZE (1UL << ADLAK_MM_POOL_PAGE_SHIFT)
#define ADLAK_MM_POOL_PAGE_MASK (~(ADLAK_MM_POOL_PAGE_SHIFT - 1))
/************************** Function Prototypes ******************************/

int adlak_mm_init(struct adlak_device *padlak);

void adlak_mm_deinit(struct adlak_device *padlak);

int  adlak_bitmap_pool_init(struct adlak_mm_pool_priv *pool, size_t pool_size);
void adlak_bitmap_pool_deinit(struct adlak_mm_pool_priv *pool);

unsigned long adlak_alloc_from_bitmap_pool(struct adlak_mm_pool_priv *pool, size_t size);

void adlak_free_to_bitmap_pool(struct adlak_mm_pool_priv *pool, dma_addr_t start, size_t size);

int adlak_flush_cache(struct adlak_device *padlak, struct adlak_mem_handle *mm_info);

int adlak_invalid_cache(struct adlak_device *padlak, struct adlak_mem_handle *mm_info);

void adlak_mm_free(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

struct adlak_mem_handle *adlak_mm_alloc(struct adlak_mem *mm, struct adlak_buf_req *pbuf_req);

void adlak_mm_dettach(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

struct adlak_mem_handle *adlak_mm_attach(struct adlak_mem *            mm,
                                         struct adlak_extern_buf_info *pbuf_req);

int adlak_mm_mmap(struct adlak_mem *mm, struct adlak_mem_handle *mm_info, void *const vma);

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_MM_COMMON_H__ end define*/
