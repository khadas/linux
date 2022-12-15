/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_mm_smmu.c
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
#include "adlak_mm_smmu.h"

#include "adlak_mm_smmu_tlb.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/

/************************** Function Prototypes ******************************/

void adlak_smmu_addr_pool_deinit(struct adlak_mem *mm) {
    if (ADLAK_IS_ERR_OR_NULL(mm->smmu)) {
        return;
    }
    adlak_bitmap_pool_deinit(&mm->smmu->bitmap_area);
    adlak_os_free(mm->smmu);
}

int adlak_smmu_addr_pool_init(struct adlak_mem *mm) {
    mm->smmu = adlak_os_zalloc(sizeof(struct adlak_mem), ADLAK_GFP_KERNEL);
    if (ADLAK_IS_ERR_OR_NULL(mm->smmu)) {
        goto err_alloc;
    }
    mm->smmu->size = (size_t)SMMU_IOVA_ADDR_SIZE;
    return adlak_bitmap_pool_init(&mm->smmu->bitmap_area, mm->smmu->size);
err_alloc:
    return ERR(ENOMEM);
}

static void adlak_smmu_unmap_iova_pages(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    int           smmu_nr_pages;
    int           i;
    unsigned long iova_addr;
    iova_addr = mm_info->iova_addr;
#if ADLAK_DEBUG_SMMU_EN
    AML_LOG_DEBUG("%s", __func__);
#endif
    smmu_nr_pages = mm_info->nr_pages * (ADLAK_PAGE_SIZE / SMMU_PAGESIZE);

    for (i = 0; i < smmu_nr_pages; ++i) {
        adlak_smmu_tlb_del((struct __adlak_smmu *)mm->padlak->psmmu, iova_addr);
        if (mm_info->req.bytes < (1 << SMMU_TLB2_VA_SHIFT)) {
            //   adlak_hal_smmu_cache_invalid((void *)mm->padlak, iova_addr);
        }
        iova_addr += SMMU_PAGESIZE;
    }
    if (mm_info->req.bytes >= (1 << SMMU_TLB2_VA_SHIFT)) {
        //  adlak_hal_smmu_cache_invalid((void *)mm->padlak, 0);
    }

    adlak_free_to_bitmap_pool(&mm->smmu->bitmap_area, mm_info->iova_addr,
                              (size_t)mm_info->req.bytes);
}

void adlak_smmu_iova_unmap(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
#if ADLAK_DEBUG_SMMU_EN
    AML_LOG_DEBUG("%s", __func__);
#endif

    if (SMMU_IOVA_ADDR_SIZE >= mm_info->iova_addr) {
        adlak_smmu_unmap_iova_pages(mm, mm_info);
    }
}

int adlak_smmu_iova_map(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    phys_addr_t   phys_addr;
    unsigned long iova_start_addr = 0;
    dma_addr_t    iova_addr;
    int           i, j;

    struct __adlak_smmu *psmmu = (struct __adlak_smmu *)mm->padlak->psmmu;

#if ADLAK_DEBUG_SMMU_EN
    AML_LOG_DEBUG("%s", __func__);
#endif
    mm_info->iova_addr = SMMU_IOVA_ADDR_SIZE;

    iova_start_addr = adlak_alloc_from_bitmap_pool(&mm->smmu->bitmap_area, mm_info->req.bytes);

    if (ADLAK_IS_ERR((void *)iova_start_addr)) {
        AML_LOG_ERR("failed to alloc iova!");
        goto early_exit;
    }
#if ADLAK_DEBUG_SMMU_EN
    AML_LOG_INFO("iova mapping %lu bytes starting at 0x%lX ", (uintptr_t)mm_info->req.bytes,
                 (uintptr_t)iova_start_addr);
#endif
    mm_info->iova_addr = iova_start_addr;
    iova_addr          = iova_start_addr;

    AML_LOG_DEBUG("iova_addr = 0x%llx, phys_addr[0] = 0x%llx ", (uint64_t)mm_info->iova_addr,
                  (uint64_t)mm_info->phys_addrs[0]);

    for (i = 0; i < mm_info->nr_pages; ++i) {
        for (j = 0; j < ADLAK_PAGE_SIZE / SMMU_PAGESIZE; ++j) {
            phys_addr = mm_info->phys_addrs[i] + j * SMMU_PAGESIZE;

            if (adlak_smmu_tlb_add(psmmu, iova_addr, phys_addr, SMMU_PAGESIZE)) {
                AML_LOG_ERR("failed to map iova 0x%lX pa 0x%lX size %lu\n", (uintptr_t)iova_addr,
                            (uintptr_t)phys_addr, ADLAK_PAGE_SIZE);
                goto unmap_pages;
            }
            if (mm_info->req.bytes < (1 << SMMU_TLB2_VA_SHIFT)) {
                //   adlak_hal_smmu_cache_invalid((void *)mm->padlak, iova_addr);
            }
            iova_addr += SMMU_PAGESIZE;
        }
    }
    if (mm_info->req.bytes >= (1 << SMMU_TLB2_VA_SHIFT)) {
        //  adlak_hal_smmu_cache_invalid((void *)mm->padlak, 0);
    }

    adlak_smmu_tlb_dump(psmmu);

    return 0;

unmap_pages:
    adlak_smmu_unmap_iova_pages(mm, mm_info);

early_exit:
    return ERR(ENOMEM);
}
