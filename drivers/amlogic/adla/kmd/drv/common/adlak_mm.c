/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_mm.c
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

/***************************** Include Files *********************************/
#include "adlak_mm.h"

#include "adlak_api.h"
#include "adlak_common.h"
#include "adlak_context.h"
#include "adlak_device.h"
#include "adlak_mm_common.h"
#include "adlak_mm_os_common.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/

/************************** Function Prototypes ******************************/

int adlak_mem_alloc_request(struct adlak_context *context, struct adlak_buf_req *pbuf_req) {
    int                      ret     = 0;
    struct adlak_mem_handle *mm_info = NULL;
    struct adlak_device *    padlak  = context->padlak;
    AML_LOG_DEBUG("%s", __func__);

    AML_LOG_DEBUG("mem_alloc_request size:0x%lX bytes", (uintptr_t)pbuf_req->bytes);
    mm_info = adlak_mm_alloc(padlak->mm, pbuf_req);

    if (ADLAK_IS_ERR_OR_NULL(mm_info)) {
        ret = ERR(ENOMEM);
        AML_LOG_ERR("adlak_dma_alloc_and_map failed.");
        goto err_malloc;
    }
    if (pbuf_req->mmap_en) {
        ret = adlak_os_mmap2userspace(padlak->mm, mm_info);
        if (ret) {
            AML_LOG_ERR("adlak_dma_mmap2userspace failed.");
            goto err_mmap;
        }
    }
    AML_LOG_DEBUG("cpu_addr=0x%lX, ", (uintptr_t)mm_info->cpu_addr);
    AML_LOG_DEBUG("cpu_addr_user=0x%lX, ", (uintptr_t)mm_info->cpu_addr_user);
    AML_LOG_DEBUG("iova_addr=0x%lX, ", (uintptr_t)mm_info->iova_addr);
    AML_LOG_DEBUG("size=%lu Kbytes\n", (uintptr_t)(mm_info->req.bytes / 1024));
    pbuf_req->ret_desc.bytes = mm_info->req.bytes;

    pbuf_req->ret_desc.va_kernel = (uint64_t)(uintptr_t)mm_info->cpu_addr;
    pbuf_req->ret_desc.va_user   = (uint64_t)(uintptr_t)mm_info->cpu_addr_user;
    pbuf_req->ret_desc.iova_addr = (uint64_t)(uintptr_t)mm_info->iova_addr;
    pbuf_req->ret_desc.phys_addr = (uint64_t)(uintptr_t)mm_info->phys_addr;

    if (ERR(NONE) == ret) {
        ret = adlak_context_attach_buf(context, (void *)mm_info);
        if (ERR(NONE) != ret) {
            AML_LOG_ERR("attach mm_info to context failed!");
            goto err_attach;
        }
    }
    context->mem_alloced += mm_info->req.bytes;

    padlak->mm->usage.mem_alloced_umd += mm_info->req.bytes;

    return ERR(NONE);
err_attach:

    adlak_os_unmmap_userspace(padlak->mm, mm_info);
err_mmap:
    adlak_mm_free(padlak->mm, mm_info);
err_malloc:
    return ret;
}

int adlak_mem_free_request(struct adlak_context *context, struct adlak_buf_desc *pbuf_desc) {
    int                      ret = 0;
    struct adlak_mem_handle  mm_info;
    struct adlak_mem_handle *pmm_info_hd;
    struct adlak_device *    padlak = context->padlak;
    AML_LOG_DEBUG("%s", __func__);
    if (0 != context->invoke_cnt) {
        ret = -1;
        goto err;
    }
    mm_info.iova_addr = (dma_addr_t)pbuf_desc->iova_addr;

    pmm_info_hd = (struct adlak_mem_handle *)adlak_context_dettach_buf(context, (void *)&mm_info);
    if (NULL == pmm_info_hd) {
        ret = -1;
        AML_LOG_ERR("dettach mm_info to context failed!");
        goto err;
    }
    AML_LOG_DEFAULT("iova_addr=0x%lX, ", (uintptr_t)mm_info.iova_addr);
    AML_LOG_DEFAULT("size=%lu KByte \n", (uintptr_t)(pmm_info_hd->req.bytes / 1024));
    if (pmm_info_hd->cpu_addr_user) {
        adlak_os_unmmap_userspace(padlak->mm, pmm_info_hd);
    }

    context->mem_alloced -= pmm_info_hd->req.bytes;

    padlak->mm->usage.mem_alloced_umd -= pmm_info_hd->req.bytes;

    adlak_mm_free(padlak->mm, pmm_info_hd);
    return 0;
err:
    return ret;
}
int adlak_ext_mem_attach_request(struct adlak_context *        context,
                                 struct adlak_extern_buf_info *pbuf_req) {
    int                      ret     = 0;
    struct adlak_mem_handle *mm_info = NULL;
    struct adlak_device *    padlak  = context->padlak;
    AML_LOG_DEBUG("%s", __func__);
    if (ADLAK_PAGE_ALIGN(pbuf_req->bytes) != pbuf_req->bytes) {
        ret = -1;
        AML_LOG_ERR("mem size is not align with page_size.");
        goto err;
    }
    if (0 == pbuf_req->buf_type)  // physical addr type
    {
        AML_LOG_DEBUG("this extern memory type is physical.");

        mm_info = adlak_mm_attach(padlak->mm, pbuf_req);
    } else if (1 == pbuf_req->buf_type)  //  dma handle addr type
    {
        //  AML_LOG_DEBUG("this extern memory type is dma handle.");
        ret = -1;
        AML_LOG_ERR("This memory type[%u] is unsupported Temporarily.", pbuf_req->buf_type);
        goto err;

    } else {
        ret = -1;
        AML_LOG_ERR("This memory type[%u] is not support.", pbuf_req->buf_type);
        goto err;
    }

    if (ADLAK_IS_ERR_OR_NULL(mm_info)) {
        ret = -1;
        AML_LOG_ERR("adlak_dma_alloc_and_map failed.");
        goto err;
    }
    mm_info->cpu_addr_user   = NULL;
    pbuf_req->ret_desc.bytes = mm_info->req.bytes;

    pbuf_req->ret_desc.va_kernel = (uint64_t)(uintptr_t)mm_info->cpu_addr;
    pbuf_req->ret_desc.va_user   = (uint64_t)(uintptr_t)mm_info->cpu_addr_user;
    pbuf_req->ret_desc.iova_addr = (uint64_t)(uintptr_t)mm_info->iova_addr;

    if (pbuf_req->mmap_en) {
        ret = adlak_os_mmap2userspace(padlak->mm, mm_info);
        if (ret) {
            AML_LOG_ERR("adlak_dma_mmap2userspace failed.");
            goto err_mmap;
        }
    }
    AML_LOG_DEBUG("cpu_addr=0x%lX, ", (uintptr_t)mm_info->cpu_addr);
    AML_LOG_DEBUG("cpu_addr_user=0x%lX, ", (uintptr_t)mm_info->cpu_addr_user);
    AML_LOG_DEBUG("iova_addr=0x%lX, ", (uintptr_t)mm_info->iova_addr);
    AML_LOG_DEBUG("size=%lu Kbytes\n", (uintptr_t)(mm_info->req.bytes / 1024));
    pbuf_req->ret_desc.bytes = mm_info->req.bytes;

    pbuf_req->ret_desc.va_kernel = (uint64_t)(uintptr_t)mm_info->cpu_addr;
    pbuf_req->ret_desc.va_user   = (uint64_t)(uintptr_t)mm_info->cpu_addr_user;
    pbuf_req->ret_desc.iova_addr = (uint64_t)(uintptr_t)mm_info->iova_addr;
    pbuf_req->ret_desc.phys_addr = (uint64_t)(uintptr_t)mm_info->phys_addr;

    pbuf_req->errcode = 0;

    if (ERR(NONE) == ret) {
        ret = adlak_context_attach_buf(context, (void *)mm_info);
        if (ERR(NONE) != ret) {
            AML_LOG_ERR("attach mm_info to context failed!");
        }
    }
    return 0;
err_mmap:
    adlak_mm_dettach(padlak->mm, mm_info);
err:

    return ret;
}

int adlak_ext_mem_dettach_request(struct adlak_context *        context,
                                  struct adlak_extern_buf_info *pbuf_desc) {
    int                      ret = 0;
    struct adlak_mem_handle  mm_info;
    struct adlak_mem_handle *pmm_info_hd;
    struct adlak_device *    padlak = context->padlak;
    AML_LOG_DEBUG("%s", __func__);
    mm_info.iova_addr = (dma_addr_t)pbuf_desc->ret_desc.iova_addr;

    pmm_info_hd = (struct adlak_mem_handle *)adlak_context_dettach_buf(context, (void *)&mm_info);
    if (NULL == pmm_info_hd) {
        AML_LOG_ERR("dettach mm_info to context failed!");
        goto err;
        ret = -1;
    }
    AML_LOG_DEFAULT("iova_addr=0x%lX, ", (uintptr_t)mm_info.iova_addr);
    AML_LOG_DEFAULT("size=%lu KByte \n", (uintptr_t)(pmm_info_hd->req.bytes / 1024));
    if (pmm_info_hd->cpu_addr_user) {
        adlak_os_unmmap_userspace(padlak->mm, pmm_info_hd);
    }

    adlak_mm_dettach(padlak->mm, pmm_info_hd);
    return 0;
err:
    return ret;
}

int adlak_mem_free_all_context(struct adlak_context *context) {
    struct adlak_mem_handle *pmm_info_hd;
    struct adlak_device *    padlak   = context->padlak;
    struct context_buf *     pcontext = NULL, *pcontext_tmp = NULL;
    AML_LOG_DEBUG("%s", __func__);
    if (!list_empty(&context->sbuf_list)) {
        list_for_each_entry_safe(pcontext, pcontext_tmp, &context->sbuf_list, head) {
            if (pcontext) {
                list_del(&pcontext->head);
                pmm_info_hd = pcontext->mm_info;
                if (CONTEXT_STATE_CLOSED != context->state) {
                    // not need this.  adlak_dma_unmmap_userspace
                }
                if (pmm_info_hd->mem_src != ADLAK_ENUM_MEMSRC_EXT_PHYS) {
                    context->mem_alloced -= pmm_info_hd->req.bytes;
                    padlak->mm->usage.mem_alloced_umd -= pmm_info_hd->req.bytes;
                    adlak_mm_free(padlak->mm, pmm_info_hd);
                } else {
                    adlak_mm_dettach(padlak->mm, pmm_info_hd);
                }
                adlak_os_free(pcontext);
            }
        }
    }
    return 0;
}

int adlak_mem_flush_request(struct adlak_context *context, struct adlak_buf_flush *pflush_desc) {
    int                      ret = 0;
    struct adlak_mem_handle  mm_info;
    struct adlak_mem_handle *pmm_info_hd;
    struct adlak_device *    padlak     = context->padlak;
    struct adlak_buf_desc *  pbuf_desc  = NULL;
    struct context_buf *     target_buf = NULL;

    AML_LOG_DEBUG("%s", __func__);
    pbuf_desc         = &pflush_desc->buf_desc;
    mm_info.iova_addr = (dma_addr_t)pbuf_desc->iova_addr;
    /* LOCK */
    adlak_os_mutex_lock(&context->context_mutex);
    target_buf = find_buffer_by_desc(context, (void *)&mm_info);
    if (!target_buf) {
        AML_LOG_ERR("no corresponding buffer found in this context!");
        ret = -1;
    } else {
        pmm_info_hd = target_buf->mm_info;
        switch (pflush_desc->direction) {
            case FLUSH_TO_DEVICE:
                // flush cache
                adlak_flush_cache(padlak, pmm_info_hd);
                break;
            case FLUSH_FROM_DEVICE:
                // invalid cache
                adlak_invalid_cache(padlak, pmm_info_hd);
                break;
            default:
                break;
        }
        target_buf = NULL;
        ret        = 0;
    }
    /* UNLOCK */
    adlak_os_mutex_unlock(&context->context_mutex);

    return ret;
}

int adlak_cmq_buf_init(struct adlak_device *padlak) {
    struct adlak_buf_req buf_req;

    AML_LOG_DEBUG("%s", __func__);
    AML_LOG_INFO("alloc buffer for cmq,size=0x%08x", padlak->cmq_buf_info.total_size);
    buf_req.bytes             = padlak->cmq_buf_info.total_size;
    buf_req.ret_desc.mem_type = ADLAK_ENUM_MEMTYPE_CONTIGUOUS | ADLAK_ENUM_MEMTYPE_INNER;
    /*In order to avoid constantly sync the cache of cmq, it is recommended that cmq use
     * uncache buffer*/
    //  buf_req.ret_desc.mem_type      = buf_req.ret_desc.mem_type | ADLAK_ENUM_MEMTYPE_CACHEABLE;
    buf_req.ret_desc.mem_direction = ADLAK_ENUM_MEM_DIR_WRITE_ONLY;

    padlak->cmq_buf_info.cmq_mm_info = adlak_mm_alloc(padlak->mm, &buf_req);
    if (NULL == padlak->cmq_buf_info.cmq_mm_info) {
        AML_LOG_ERR("alloc buffer for cmq failed!");
        goto err;
    }

    padlak->cmq_buf_info.cmq_wr_offset = 0;
    padlak->cmq_buf_info.cmq_rd_offset = 0;

    padlak->mm->usage.mem_alloced_kmd += padlak->cmq_buf_info.cmq_mm_info->req.bytes;
    return 0;
err:
    return -1;
}

void adlak_cmq_buf_deinit(struct adlak_device *padlak) {
    AML_LOG_DEBUG("%s", __func__);
    padlak->cmq_buf_info.cmq_wr_offset = 0;
    padlak->cmq_buf_info.cmq_rd_offset = 0;

    if (NULL != padlak->cmq_buf_info.cmq_mm_info) {
        padlak->mm->usage.mem_alloced_kmd -= padlak->cmq_buf_info.cmq_mm_info->req.bytes;
        adlak_mm_free(padlak->mm, padlak->cmq_buf_info.cmq_mm_info);
    }
}

int adlak_mem_init(struct adlak_device *padlak) {
    int ret = 0;

    AML_LOG_DEBUG("%s", __func__);
    ret = adlak_mm_init(padlak);
    if (ret) {
        goto end;
    }
    ret = adlak_cmq_buf_init(padlak);
    if (ret) {
        goto end;
    }

    return 0;

end:
    return ret;
}

int adlak_mem_deinit(struct adlak_device *padlak) {
    AML_LOG_DEBUG("%s", __func__);
    if (padlak->mm) {
        adlak_cmq_buf_deinit(padlak);
        adlak_mm_deinit(padlak);
    }
    return 0;
}

int adlak_mem_mmap(struct adlak_context *context, void *const vma, uint64_t iova_addr) {
    int                  ret    = 0;
    struct adlak_device *padlak = context->padlak;

    struct adlak_mem_handle  mm_info;
    struct adlak_mem_handle *pmm_info_hd;
    struct context_buf *     target_buf = NULL;
    AML_LOG_DEBUG("%s", __func__);
    mm_info.iova_addr = (dma_addr_t)iova_addr;

    /* LOCK */
    adlak_os_mutex_lock(&context->context_mutex);
    target_buf = find_buffer_by_desc(context, &mm_info);
    if (!target_buf) {
        AML_LOG_ERR("no corresponding buffer found in this context!");
        ret = -1;
        goto err;
    } else {
        pmm_info_hd = target_buf->mm_info;
    }

    ret = adlak_mm_mmap(padlak->mm, pmm_info_hd, vma);
    if (ret) {
        AML_LOG_ERR("adlak_dma_map failed.");
        goto err;
    }
err:
    adlak_os_mutex_unlock(&context->context_mutex);

    return ret;
}
