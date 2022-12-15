/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_mm_mbp.h
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

#ifndef __ADLAK_MM_MBP_H__
#define __ADLAK_MM_MBP_H__

/***************************** Include Files *********************************/
#include "adlak_mm_common.h"
#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

/************************** Function Prototypes ******************************/

int  adlak_malloc_through_mbp(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);
void adlak_free_through_mbp(struct adlak_mem *mm, struct adlak_mem_handle *mm_info);

int adlak_create_mem_pool_from_mbp_uncache(struct adlak_mem *mm);

int adlak_destroy_mem_pool_from_mbp_uncache(struct adlak_mem *mm);

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_MM_MBP_H__ end define*/
