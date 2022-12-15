/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_mm_smmu.h
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

#ifndef __ADLAK_MM_SMMU_H__
#define __ADLAK_MM_SMMU_H__

/***************************** Include Files *********************************/
#include "adlak_mm_common.h"
#ifdef __cplusplus
extern "C" {
#endif

#ifndef ADLAK_DEBUG_SMMU_EN
#define ADLAK_DEBUG_SMMU_EN (0)
#endif

// #define SMMU_SIZE_1G (1U << 30)
#define SMMU_SIZE_1G (1 << 30)
#define SMMU_MASK_1G (~(SMMU_SIZE_1G - 1))
#define SMMU_SIZE_2M (2U << 20)
#define SMMU_MASK_2M (~(SMMU_SIZE_2M - 1))
#define SMMU_SIZE_4K (4U << 10)
#define SMMU_MASK_4K (~(SMMU_SIZE_4K - 1))

#define SMMU_VA_SPACE_MAX (2)  // unit 1GB, The max value is 3
#define SMMU_ALIGN_SIZE (64)

#define SMMU_TLB_ENTRY_SIZE (8U)

#define SMMU_TLB1_ENTRY_COUNT (SMMU_VA_SPACE_MAX)
#define SMMU_TLB1_SIZE (SMMU_TLB1_ENTRY_COUNT * SMMU_TLB_ENTRY_SIZE)
#define SMMU_TLB1_VA_SHIFT (30)
#define SMMU_TLB1_VA_MASK ((SMMU_TLB1_ENTRY_COUNT - 1) << SMMU_TLB1_VA_SHIFT)
#define GET_SMMU_TLB1_ENTRY_OFFSEST(iova) (((iova)&SMMU_TLB1_VA_MASK) >> SMMU_TLB1_VA_SHIFT)

#define SMMU_TLB2_ENTRY_COUNT_2M (512U)
#define SMMU_TLB2_SIZE (SMMU_TLB2_ENTRY_COUNT_2M * SMMU_TLB_ENTRY_SIZE)
#define SMMU_TLB2_ENTRY_COUNT_TOTAL (SMMU_TLB2_ENTRY_COUNT_2M * SMMU_TLB1_ENTRY_COUNT)
#define SMMU_TLB2_VA_SHIFT (21)
#define SMMU_TLB2_VA_MASK ((SMMU_TLB2_ENTRY_COUNT_2M - 1) << SMMU_TLB2_VA_SHIFT)
#define GET_SMMU_TLB2_ENTRY_OFFSEST(iova) (((iova)&SMMU_TLB2_VA_MASK) >> SMMU_TLB2_VA_SHIFT)

#define SMMU_TLB3_ENTRY_COUNT_4K (512U)
#define SMMU_TLB3_SIZE (SMMU_TLB3_ENTRY_COUNT_4K * SMMU_TLB_ENTRY_SIZE)
#define SMMU_TLB3_ENTRY_COUNT_TOTAL (SMMU_TLB3_ENTRY_COUNT_4K * SMMU_TLB2_ENTRY_COUNT_TOTAL)
#define SMMU_TLB3_VA_SHIFT (12)
#define SMMU_TLB3_VA_MASK ((SMMU_TLB3_ENTRY_COUNT_4K - 1) << SMMU_TLB3_VA_SHIFT)
#define GET_SMMU_TLB3_ENTRY_OFFSEST(iova) (((iova)&SMMU_TLB3_VA_MASK) >> SMMU_TLB3_VA_SHIFT)

#define SMMU_PAGESIZE (1 << SMMU_TLB3_VA_SHIFT)

#define SMMU_ENTRY_FLAG_MASK (0x03)
#define SMMU_ENTRY_FLAG_L1_VALID (0x03)
#define SMMU_ENTRY_FLAG_L1_INVALID (0x00)
#define SMMU_ENTRY_FLAG_L2_VALID_4K (0x03)
#define SMMU_ENTRY_FLAG_L2_VALID_2M (0x01)
#define SMMU_ENTRY_FLAG_L2_INVALID (0x00)
#define SMMU_ENTRY_FLAG_L3_VALID (0x03)
#define SMMU_ENTRY_FLAG_L3_INVALID (0x00)

/* SMMU IOVA address space size,   */
#define SMMU_IOVA_ADDR_SIZE (0x40000000UL * SMMU_TLB1_ENTRY_COUNT)

/************************** Function Prototypes ******************************/

void adlak_smmu_addr_pool_deinit(struct adlak_mem *mm);

int  adlak_smmu_addr_pool_init(struct adlak_mem *mm);
void adlak_smmu_iova_unmap(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);
int  adlak_smmu_iova_map(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_MM_SMMU_H__ end define*/
