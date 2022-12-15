/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_mm_smmu_tlb.h
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

#ifndef __ADLAK_MM_SMMU_TLB_H__
#define __ADLAK_MM_SMMU_TLB_H__

/***************************** Include Files *********************************/
#include "adlak_mm_smmu.h"

#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

struct __adlak_smmu_tlb_info {
    struct adlak_mem_handle *mm_info;
    size_t                   size;
};

struct __adlak_smmu {
    uint64_t                     smmu_entry;
    struct __adlak_smmu_tlb_info tlb_l1;
    struct __adlak_smmu_tlb_info tlb_l2[SMMU_TLB1_ENTRY_COUNT];
    struct __adlak_smmu_tlb_info tlb_l3[SMMU_TLB1_ENTRY_COUNT][SMMU_TLB2_ENTRY_COUNT_2M];
    ;
};

struct adlak_smmu_private {
    struct iommu_domain *domain;

    void *           bitmap;
    dma_addr_t       addr_base;
    size_t           bits;
    struct page *    page;
    adlak_os_mutex_t lock;
};

/************************** Function Prototypes ******************************/

int  adlak_smmu_tlb_add(struct __adlak_smmu *psmmu, dma_addr_t iova_addr, phys_addr_t phys_addr,
                        size_t size);
int  adlak_smmu_tlb_del(struct __adlak_smmu *psmmu, dma_addr_t iova_addr);
int  adlak_smmu_tlb_dump(struct __adlak_smmu *psmmu);
void adlak_debug_mem_fill_as_address(struct adlak_mem_handle *mm_info);
void adlak_debug_mem_dump(struct adlak_mem_handle *mm_info);

int adlak_smmu_init(struct adlak_mem *mm);

void adlak_smmu_deinit(struct adlak_mem *mm);

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_MM_SMMU_TLB_H__ end define*/
