/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_mm_os_common.h
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

#ifndef __ADLAK_MM_OS_COMMON_H__
#define __ADLAK_MM_OS_COMMON_H__

/***************************** Include Files *********************************/

#include "adlak_mm_common.h"
#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

/************************** Function Prototypes ******************************/

void adlak_cma_deinit(struct device *dev);

int adlak_cma_init(struct device *dev);

int adlak_remap_region_nocache(struct adlak_mem *mm);

void adlak_unmap_region_nocache(struct adlak_mem *mm);

int adlak_os_alloc_discontiguous(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

int adlak_os_alloc_contiguous(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

void adlak_os_free_discontiguous(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

void adlak_os_free_contiguous(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

int adlak_os_attach_ext_mem_phys(struct adlak_mem *mm, struct adlak_mem_handle *mm_info,
                                 uint64_t phys_addr);

void adlak_os_dettach_ext_mem_phys(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

int adlak_os_mmap(struct adlak_mem *mm, struct adlak_mem_handle *mm_info, void *const vma);

void adlak_os_flush_cache(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

void adlak_os_invalid_cache(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

void adlak_free_through_dma(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

int adlak_malloc_through_dma(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

int adlak_os_mmap2userspace(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

void adlak_os_unmmap_userspace(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

void adlak_free_cma_region_nocache(struct adlak_mem *mm);

int adlak_alloc_cma_region_nocache(struct adlak_mem *mm);
#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_MM_OS_COMMON_H__ end define*/
