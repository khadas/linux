/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_mm_mbp.c
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

#include "adlak_mm_mbp.h"
/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/

/************************** Function Prototypes ******************************/
int adlak_malloc_through_mbp(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    // uncacheable
    // TODO

    return ERR(NONE);
}

void adlak_free_through_mbp(struct adlak_mem *mm, struct adlak_mem_handle *mm_info) {
    // uncacheable
    // TODO
}

int adlak_create_mem_pool_from_mbp_uncache(struct adlak_mem *mm) { return ERR(NONE); }

int adlak_destroy_mem_pool_from_mbp_uncache(struct adlak_mem *mm) { return ERR(NONE); }
