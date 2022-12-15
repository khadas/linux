/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_mm.h
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2021/06/05	Initial release
 * </pre>
 *
 ******************************************************************************/

#ifndef __ADLAK_MM_H__
#define __ADLAK_MM_H__

/***************************** Include Files *********************************/
#include "adlak_api.h"
#include "adlak_common.h"
#include "adlak_context.h"
#include "adlak_device.h"
#include "adlak_mm_common.h"
#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

/************************** Function Prototypes ******************************/

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

int adlak_mem_alloc_request(struct adlak_context *context, struct adlak_buf_req *pbuf_req);
int adlak_mem_free_request(struct adlak_context *context, struct adlak_buf_desc *pbuf_desc);
int adlak_mem_free_all_context(struct adlak_context *context);
int adlak_ext_mem_attach_request(struct adlak_context *        context,
                                 struct adlak_extern_buf_info *pbuf_req);
int adlak_ext_mem_dettach_request(struct adlak_context *        context,
                                  struct adlak_extern_buf_info *pbuf_desc);
int adlak_mem_flush_request(struct adlak_context *context, struct adlak_buf_flush *pflush_desc);

int adlak_mem_init(struct adlak_device *padlak);

int adlak_mem_deinit(struct adlak_device *padlak);

int adlak_flush_cache(struct adlak_device *padlak, struct adlak_mem_handle *mm_info);

int adlak_invalid_cache(struct adlak_device *padlak, struct adlak_mem_handle *mm_info);

int adlak_mem_mmap(struct adlak_context *context, void *const vma, uint64_t iova);

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_MM_H__ end define*/
