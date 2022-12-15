/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_context.c
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

/***************************** Include Files *********************************/
#include "adlak_context.h"

#include "adlak_api.h"
#include "adlak_common.h"
#include "adlak_device.h"
#include "adlak_mm.h"
#include "adlak_submit.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/

/************************** Function Prototypes ******************************/

int adlak_create_context(void *adlak_device, struct adlak_context **p_context) {
    int                   ret     = ERR(NONE);
    struct adlak_context *context = NULL;
    struct adlak_device * padlak  = (struct adlak_device *)adlak_device;
    AML_LOG_DEBUG("%s", __func__);
    if ((!adlak_device) || (!p_context)) {
        AML_LOG_ERR("invalid input context or common args to be null!");
        goto end;
    }
    context = adlak_os_zalloc(sizeof(struct adlak_context), ADLAK_GFP_KERNEL);
    if (!context) {
        return ERR(ENOMEM);
    }

    INIT_LIST_HEAD(&context->sbuf_list);
    INIT_LIST_HEAD(&context->net_list);
    adlak_os_mutex_init(&context->context_mutex);

    adlak_os_mutex_lock(&context->context_mutex);
    context->padlak     = adlak_device;
    context->state      = CONTEXT_STATE_INITED;
    context->invoke_cnt = 0;
    ++padlak->net_id;
    if (padlak->net_id < 0) {
        padlak->net_id = 0;
    }
    context->net_id      = padlak->net_id;
    context->mem_alloced = 0;
    AML_LOG_DEBUG("new context created, net_id[%d]", context->net_id);
    adlak_to_umd_sinal_init(&context->wait);
    /*Add to context queue*/
    INIT_LIST_HEAD(&context->head);
    list_add_tail(&context->head, &padlak->context_list);
#ifdef CONFIG_ADLAK_DEBUG_INNNER
    adlak_dbg_inner_init(context);
#endif
    *p_context = context;

    adlak_os_sema_init(&context->ctx_idle, 1, 0);
    adlak_os_mutex_unlock(&context->context_mutex);
end:
    return ret;
}

int adlak_net_dettach_by_id(struct adlak_context *context, int net_id) {
    int                ret   = 0;
    struct adlak_task *ptask = NULL, *ptask_tmp = NULL;
    struct list_head * hd = &context->net_list;
    AML_LOG_DEBUG("%s", __func__);
    if (!list_empty(hd)) {
        list_for_each_entry_safe(ptask, ptask_tmp, hd, head) {
            {
                if ((net_id != -1) && (net_id != ptask->net_id)) {
                    continue;
                }
                ret++;
                list_del(&ptask->head);
                adlak_task_destroy(ptask);
            }
        }
    }
    return ret;
}

int adlak_destroy_context(struct adlak_device *padlak, struct adlak_context *context) {
    int     ret = ERR(NONE);
    int32_t net_id;
    AML_LOG_DEBUG("%s", __func__);
    if (!context) {
        AML_LOG_WARN("invalid input context args to be null or invalid operation!");
        ret = -1;
        goto end;
    }

#ifdef CONFIG_ADLAK_DEBUG_INNNER
    adlak_dbg_inner_update(context, "context destroy");
    adlak_dbg_inner_dump_info(context);
    adlak_dbg_inner_deinit(context);
#endif

    net_id = context->net_id;
    adlak_os_mutex_lock(&context->context_mutex);
    context->state = CONTEXT_STATE_CLOSED;
    adlak_os_mutex_unlock(&context->context_mutex);
    ret = adlak_invoke_del_all(padlak, net_id);
    if (0 != ret) {
        AML_LOG_WARN("net [%d] is busy,so destroy delay!", net_id);
        if (ERR(EINTR) == adlak_os_sema_take(context->ctx_idle)) {
            AML_LOG_ERR("sema[stat_idle] take err!");
            ASSERT(0);
        }
    }
    {
        AML_LOG_INFO("net [%d] is idle!", net_id);
        adlak_os_mutex_lock(&context->context_mutex);
        adlak_mem_free_all_context(context);
        adlak_net_dettach_by_id(context, net_id);

        adlak_to_umd_sinal_deinit(&context->wait);

        adlak_os_sema_destroy(&context->ctx_idle);
        adlak_os_mutex_unlock(&context->context_mutex);
        adlak_os_mutex_destroy(&context->context_mutex);

        adlak_os_mutex_lock(&padlak->dev_mutex);
        list_del(&context->head); /*del from context list*/
        adlak_os_free(context);   // destroy context
        AML_LOG_DEBUG("net_id [%d] destroyed", net_id);
        adlak_os_mutex_unlock(&padlak->dev_mutex);
    }

end:
    return 0;
}

struct context_buf *find_buffer_by_desc(struct adlak_context *context, void *pmm_info) {
    struct context_buf *     target_buf   = NULL;
    struct context_buf *     context_buf  = NULL;
    struct list_head *       node         = NULL;
    struct list_head *       node_tmp     = NULL;
    struct adlak_mem_handle *pmm_info_tmp = NULL;
    AML_LOG_DEBUG("%s", __func__);
    if ((!context) || (!pmm_info)) {
        AML_LOG_ERR("invalid input context or buf_desc args to be null!");
        goto end;
    }

    list_for_each_prev_safe(node, node_tmp, &context->sbuf_list) {
        /*Due to the special application scenario, the target can be found faster by looking from
         * the back to the front*/
        context_buf = list_entry(node, struct context_buf, head);
        if (context_buf) {
            pmm_info_tmp = (struct adlak_mem_handle *)((context_buf->mm_info));
            if (pmm_info_tmp->iova_addr == ((struct adlak_mem_handle *)pmm_info)->iova_addr) {
                target_buf = context_buf;
                AML_LOG_DEBUG("found matched buffer.");
                break;
            }
        }
    }

end:
    return target_buf;
}

int adlak_context_attach_buf(struct adlak_context *context, void *mm_info) {
    int                 ret      = ERR(NONE);
    struct context_buf *new_sbuf = NULL;
    AML_LOG_DEBUG("%s", __func__);
    if ((!context) || (!mm_info)) {
        AML_LOG_ERR("invalid input context or buf_req or buf args to be null!");
        ret = -1;
        goto end;
    }

    new_sbuf = adlak_os_zalloc(sizeof(struct context_buf), ADLAK_GFP_KERNEL);
    if (!new_sbuf) {
        AML_LOG_ERR("alloc context buf failed!");

        ret = -1;
    } else {
        new_sbuf->mm_info = mm_info;
        INIT_LIST_HEAD(&new_sbuf->head);

        adlak_os_mutex_lock(&context->context_mutex);
        list_add(&new_sbuf->head, &context->sbuf_list);
        adlak_os_mutex_unlock(&context->context_mutex);
    }

end:
    return ret;
}

void *adlak_context_dettach_buf(struct adlak_context *context, void *pmm_info) {
    struct context_buf *target_buf      = NULL;
    void *              pmm_info_finded = NULL;
    if ((!context) || (!pmm_info)) {
        AML_LOG_ERR("invalid input context or buf args to be null!");

        goto err;
    }

    adlak_os_mutex_lock(&context->context_mutex);
    target_buf = find_buffer_by_desc(context, pmm_info);
    if (!target_buf) {
        AML_LOG_ERR("no corresponding buffer found in this context!");
        goto end;
    } else {
        pmm_info_finded = target_buf->mm_info;
        list_del(&target_buf->head);
        adlak_os_free(target_buf);
        target_buf = NULL;
    }

end:
    adlak_os_mutex_unlock(&context->context_mutex);
err:
    return pmm_info_finded;
}

int adlak_destroy_all_context(struct adlak_device *padlak) {
    struct adlak_context *context, *context_tmp;

    AML_LOG_DEBUG("%s", __func__);
    list_for_each_entry_safe(context, context_tmp, &padlak->context_list, head) {
        if (context) {
            adlak_destroy_context(padlak, context);
        }
    }

    return 0;
}

int adlak_context_flush_cache(struct adlak_context *context) {
    struct context_buf *sbuf = NULL, *sbuf_tmp = NULL;

    AML_LOG_DEBUG("%s", __func__);
    list_for_each_entry_safe(sbuf, sbuf_tmp, &context->sbuf_list, head) {
        if (sbuf) {
            adlak_flush_cache(context->padlak, sbuf->mm_info);
        }
    }
    return 0;
}

int adlak_context_invalid_cache(struct adlak_context *context) {
    struct context_buf *sbuf = NULL, *sbuf_tmp = NULL;

    AML_LOG_DEBUG("%s", __func__);
    list_for_each_entry_safe(sbuf, sbuf_tmp, &context->sbuf_list, head) {
        if (sbuf) {
            adlak_invalid_cache(context->padlak, sbuf->mm_info);
        }
    }
    return 0;
}
