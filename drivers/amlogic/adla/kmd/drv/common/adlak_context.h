/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_context.h
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2021/06/06	Initial release
 * </pre>
 *
 ******************************************************************************/

#ifndef __ADLAK_CONTEXT_H__
#define __ADLAK_CONTEXT_H__

/***************************** Include Files *********************************/
#include "adlak_api.h"
#include "adlak_common.h"
#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

/**
 * @brief head of context buffer
 *
 */
struct context_buf {
    void *           mm_info;
    struct list_head head;
};

/**
 * @brief private data struct for every file open operation
 *
 */
struct adlak_context {
    struct list_head     head;
    struct adlak_device *padlak;
    struct list_head     sbuf_list;
    struct list_head     net_list;
    adlak_os_mutex_t     context_mutex;
    uintptr_t            wait;  // wait_queue_head_t *
    uint32_t             state;
    uint32_t             invoke_cnt;

#define CONTEXT_STATE_USED (1 << 2)
#define CONTEXT_STATE_INITED (1 << 1)
#define CONTEXT_STATE_CLOSED (1 << 0)
    int32_t net_id;
    size_t  mem_alloced;
#ifdef CONFIG_ADLAK_DEBUG_INNNER
    struct adlak_dbg_info *dbg_info;
#endif

    adlak_os_sema_t ctx_idle;
};

/************************** Function Prototypes ******************************/

/**
 * @brief create a context
 *
 * @param adlak_device
 * @param p_context
 * @return int
 */
int adlak_create_context(void *adlak_device, struct adlak_context **p_context);

/**
 * @brief destroy a context
 *
 * @param padlak
 * @param context
 * @return int
 */
int adlak_destroy_context(struct adlak_device *padlak, struct adlak_context *context);

int adlak_get_context_pid(struct adlak_context *context);

/**
 * @brief attach mm_info to context
 *
 * @param context
 * @param mm_info
 * @return int
 */
int adlak_context_attach_buf(struct adlak_context *context, void *mm_info);

/**
 * @brief dettach mm_info from context
 *
 * @param context
 * @param pmm_info
 * @return void*
 */
void *adlak_context_dettach_buf(struct adlak_context *context, void *mm_info);

/**
 * @brief destroy all context
 *
 * @param padlak
 * @return int
 */
int adlak_destroy_all_context(struct adlak_device *padlak);

/**
 * @brief destroy task from context by net_id
 *
 * @param context
 * @param net_id
 * @return int
 */
int adlak_net_dettach_by_id(struct adlak_context *context, int net_id);

/**
 * @brief flush all memory in the context
 *
 * @param context
 * @return int
 */
int adlak_context_flush_cache(struct adlak_context *context);

/**
 * @brief invalid all memory in the context
 *
 * @param context
 * @return int
 */
int adlak_context_invalid_cache(struct adlak_context *context);

/**
 * @brief find context_buf by mm_info
 *
 * @param context
 * @param pmm_info
 * @return struct context_buf*
 */
struct context_buf *find_buffer_by_desc(struct adlak_context *context, void *pmm_info);

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_CONTEXT_H__ end define*/
