/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_fops.c
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
#include "adlak_api.h"
#include "adlak_common.h"
#include "adlak_context.h"
#include "adlak_device.h"
#include "adlak_mm.h"
#include "adlak_queue.h"
#include "adlak_submit.h"
/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/

/************************** Function Prototypes ******************************/

static int drv_open(struct inode *inode, struct file *filp) {
    int                   ret     = 0;
    struct adlak_device * padlak  = NULL;
    struct adlak_context *context = NULL;
#if ADLAK_DEBUG
    g_adlak_log_level = g_adlak_log_level_pre;
#endif
    AML_LOG_DEBUG("%s", __func__);
    padlak = container_of(filp->f_op, struct adlak_device, fops);
    adlak_os_mutex_lock(&padlak->dev_mutex);
    ret = adlak_create_context(padlak, &context);
    adlak_os_mutex_unlock(&padlak->dev_mutex);
    if (ret) {
        return ret;
    } else {
        filp->private_data = context;
    }

    /* success */
    return ret;
}

static long drv_ioctl(struct file *filp, unsigned int ioctl_code, unsigned long arg) {
    void __user *const                   udata  = (void __user *)arg;
    int                                  ret    = 0;
    int                                  cp_ret = 0;
    struct adlak_buf_req                 buf_req;
    struct adlak_buf_desc                buf_desc;
    struct adlak_network_desc            net_reg_desc;
    struct adlak_context *               context = filp->private_data;
    struct adlak_device *                padlak  = context->padlak;
    struct adlak_network_del_desc        net_unreg_dec;
    struct adlak_network_invoke_desc     invoke_desc;
    struct adlak_network_invoke_del_desc uninvoke_desc;
    struct adlak_get_stat_desc           stat_desc;
    struct adlak_buf_flush               flush_desc;
    struct adlak_extern_buf_info         ext_buf_attach;
    struct adlak_extern_buf_info         ext_buf_dettach;
    struct adlak_profile_cfg_desc        profile_cfg;
    u_long                               size;
    AML_LOG_DEBUG("%s", __func__);

    size = (ioctl_code & IOCSIZE_MASK) >> IOCSIZE_SHIFT;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    if (!adlak_access_ok(udata, size)) return ERR(EFAULT);
#else
    if (ioctl_code & IOC_IN) {
        if (!adlak_access_ok(VERIFY_READ, udata, size)) return ERR(EFAULT);
    }
    if (ioctl_code & IOC_OUT) {
        if (!adlak_access_ok(VERIFY_WRITE, udata, size)) return ERR(EFAULT);
    }
#endif

    switch (ioctl_code) {
        case ADLAK_IOCTL_QUERYCAP:
            AML_LOG_DEBUG("ADLAK_IOCTL_QUERYCAP");
            /* If the user provided a NULL pointer then simply return the
             * size of the data.
             * If they provided a valid pointer then copy the data to them.
             **/
            if (!udata) {
                ret = padlak->dev_caps.size;
            } else {
                /* copy cap info/errcode to user for reference */
                cp_ret = copy_to_user(udata, padlak->dev_caps.data, padlak->dev_caps.size);
                if ((ERR(NONE) == ret) && (ERR(NONE) != cp_ret)) {
                    ret = cp_ret;
                }
            }
            break;

        case ADLAK_IOCTL_REQBUF:
            AML_LOG_DEBUG("ADLAK_IOCTL_REQBUF");
            ret = copy_from_user(&buf_req, udata, sizeof(buf_req));
            if (ret) {
                AML_LOG_ERR("buf_req desc copy from user failed!");
                ret = ERR(EFAULT);
                break;
            }
            ret = adlak_os_mutex_lock(&padlak->dev_mutex);
            if (ret) {
                break;
            }
            ret = adlak_mem_alloc_request(context, &buf_req);

            adlak_os_mutex_unlock(&padlak->dev_mutex);
            /* copy buf info/errcode to user for reference */
            cp_ret = copy_to_user(udata, &buf_req, sizeof(buf_req));
            if ((ERR(NONE) == ret) && (ERR(NONE) != cp_ret)) {
                ret = cp_ret;
            }
            break;

        case ADLAK_IOCTL_FREEBUF:
            AML_LOG_DEBUG("ADLAK_IOCTL_FREEBUF");
            ret = copy_from_user(&buf_desc, udata, sizeof(struct adlak_buf_desc));
            if (ret) {
                AML_LOG_ERR("buf_desc copy from user failed!");
                ret = ERR(EFAULT);
                break;
            }
            ret = adlak_os_mutex_lock(&padlak->dev_mutex);
            if (ret) {
                break;
            }
            ret = adlak_mem_free_request(context, &buf_desc);

            adlak_os_mutex_unlock(&padlak->dev_mutex);
            /* copy buf info/errcode to user for reference */
            cp_ret = copy_to_user(udata, &buf_desc, sizeof(struct adlak_buf_desc));
            if ((ERR(NONE) == ret) && (ERR(NONE) != cp_ret)) {
                ret = cp_ret;
            }

            break;

        case ADLAK_IOCTL_ATTACH_EXTERN_BUF:
            AML_LOG_DEBUG("ADLAK_IOCTL_ATTACH_EXTERN_BUF");
            ret = copy_from_user(&ext_buf_attach, udata, sizeof(struct adlak_extern_buf_info));
            if (ret) {
                AML_LOG_ERR("ext_buf_attach desc copy from user failed!");
                ret = ERR(EFAULT);
                break;
            }
            ret = adlak_os_mutex_lock(&padlak->dev_mutex);
            if (ret) {
                break;
            }
            ret = adlak_ext_mem_attach_request(context, &ext_buf_attach);

            adlak_os_mutex_unlock(&padlak->dev_mutex);
            /* copy buf info/errcode to user for reference */
            cp_ret = copy_to_user(udata, &ext_buf_attach, sizeof(struct adlak_extern_buf_info));
            if ((ERR(NONE) == ret) && (ERR(NONE) != cp_ret)) {
                ret = cp_ret;
            }
            break;

        case ADLAK_IOCTL_DETTACH_EXTERN_BUF:
            AML_LOG_DEBUG("ADLAK_IOCTL_DETTACH_EXTERN_BUF");
            ret = copy_from_user(&ext_buf_dettach, udata, sizeof(struct adlak_extern_buf_info));
            if (ret) {
                AML_LOG_ERR("ext_buf_dettach desc copy from user failed!");
                ret = ERR(EFAULT);
                break;
            }
            ret = adlak_os_mutex_lock(&padlak->dev_mutex);
            if (ret) {
                break;
            }
            ret = adlak_ext_mem_dettach_request(context, &ext_buf_dettach);

            adlak_os_mutex_unlock(&padlak->dev_mutex);
            /* copy buf info/errcode to user for reference */
            cp_ret = copy_to_user(udata, &ext_buf_dettach, sizeof(struct adlak_extern_buf_info));
            if ((ERR(NONE) == ret) && (ERR(NONE) != cp_ret)) {
                ret = cp_ret;
            }

            break;

        case ADLAK_IOCTL_FlUSH_CACHE:
            AML_LOG_DEBUG("ADLAK_IOCTL_FlUSH_CACHE");
            ret = copy_from_user(&flush_desc, udata, sizeof(struct adlak_buf_flush));
            if (ret) {
                AML_LOG_ERR("flush cache desc copy from user failed!");
                ret = ERR(EFAULT);
                break;
            }
            ret = adlak_os_mutex_lock(&padlak->dev_mutex);
            if (ret) {
                break;
            }
            ret = adlak_mem_flush_request(context, &flush_desc);

            adlak_os_mutex_unlock(&padlak->dev_mutex);
            /* copy buf info/errcode to user for reference */
            cp_ret = copy_to_user(udata, &flush_desc, sizeof(struct adlak_buf_flush));
            if ((ERR(NONE) == ret) && (ERR(NONE) != cp_ret)) {
                ret = cp_ret;
            }

            break;

        case ADLAK_IOCTL_REGISTER_NETWORK:

            AML_LOG_DEBUG("ADLAK_IOCTL_REGISTER_NETWORK");
            ret = copy_from_user(&net_reg_desc, udata, sizeof(struct adlak_network_desc));
            if (ret) {
                AML_LOG_ERR("net_reg_desc copy from user failed!");
                ret = ERR(EFAULT);
                break;
            }
            ret = adlak_os_mutex_lock(&padlak->dev_mutex);
            if (ret) {
                break;
            }
            ret = adlak_net_register_request(context, &net_reg_desc);

            adlak_os_mutex_unlock(&padlak->dev_mutex);
            /* copy buf info/errcode to user for reference */
            cp_ret = copy_to_user(udata, &net_reg_desc, sizeof(struct adlak_network_desc));
            if ((ERR(NONE) == ret) && (ERR(NONE) != cp_ret)) {
                ret = cp_ret;
            }

            break;
        case ADLAK_IOCTL_DESTROY_NETWORK:

            AML_LOG_DEBUG("ADLAK_IOCTL_DESTROY_NETWORK");
            ret = copy_from_user(&net_unreg_dec, udata, sizeof(struct adlak_network_del_desc));
            if (ret) {
                AML_LOG_ERR("net_unreg_dec copy from user failed!");
                ret = ERR(EFAULT);
                break;
            }
            ret = adlak_os_mutex_lock(&padlak->dev_mutex);
            if (ret) {
                break;
            }
            ret = adlak_net_unregister_request(context, &net_unreg_dec);

            adlak_os_mutex_unlock(&padlak->dev_mutex);
            /* copy buf info/errcode to user for reference */
            cp_ret = copy_to_user(udata, &net_reg_desc, sizeof(struct adlak_network_del_desc));
            if ((ERR(NONE) == ret) && (ERR(NONE) != cp_ret)) {
                ret = cp_ret;
            }

            break;
        case ADLAK_IOCTL_INVOKE:

            AML_LOG_DEBUG("ADLAK_IOCTL_INVOKE");
            ret = copy_from_user(&invoke_desc, udata, sizeof(struct adlak_network_invoke_desc));
            if (ret) {
                AML_LOG_ERR("invoke desc copy from user failed!");
                ret = ERR(EFAULT);
                break;
            }
            ret = adlak_os_mutex_lock(&padlak->dev_mutex);
            if (ret) {
                break;
            }
            ret = adlak_invoke_request(context, &invoke_desc);

            adlak_os_mutex_unlock(&padlak->dev_mutex);
            /* copy buf info/errcode to user for reference */
            cp_ret = copy_to_user(udata, &invoke_desc, sizeof(struct adlak_network_invoke_desc));
            if ((ERR(NONE) == ret) && (ERR(NONE) != cp_ret)) {
                ret = cp_ret;
            }

            break;
        case ADLAK_IOCTL_INVOKE_CANCLE:

            AML_LOG_DEBUG("ADLAK_IOCTL_INVOKE_CANCLE");
            ret =
                copy_from_user(&uninvoke_desc, udata, sizeof(struct adlak_network_invoke_del_desc));
            if (ret) {
                AML_LOG_ERR("uninvoke desc copy from user failed!");
                ret = ERR(EFAULT);
                break;
            }
            ret = adlak_os_mutex_lock(&padlak->dev_mutex);
            if (ret) {
                break;
            }
            ret = adlak_uninvoke_request(context, &uninvoke_desc);

            adlak_os_mutex_unlock(&padlak->dev_mutex);
            /* copy buf info/errcode to user for reference */
            cp_ret =
                copy_to_user(udata, &uninvoke_desc, sizeof(struct adlak_network_invoke_del_desc));
            if ((ERR(NONE) == ret) && (ERR(NONE) != cp_ret)) {
                ret = cp_ret;
            }

            break;
        case ADLAK_IOCTL_GET_STAT:

            AML_LOG_DEBUG("ADLAK_IOCTL_GET_STAT");
            ret = copy_from_user(&stat_desc, udata, sizeof(struct adlak_get_stat_desc));
            if (ret) {
                AML_LOG_ERR("stat desc copy from user failed!");
                ret = ERR(EFAULT);
                break;
            }
            ret = adlak_os_mutex_lock(&padlak->dev_mutex);
            if (ret) {
                break;
            }
            ret = adlak_get_status_request(context, &stat_desc);
            adlak_os_mutex_unlock(&padlak->dev_mutex);
            /* copy buf info/errcode to user for reference */
            cp_ret = copy_to_user(udata, &stat_desc, sizeof(struct adlak_get_stat_desc));
            if ((ERR(NONE) == ret) && (ERR(NONE) != cp_ret)) {
                ret = cp_ret;
            }

            break;

        case ADLAK_IOCTL_PRPOFILE_CFG:

            AML_LOG_DEBUG("ADLAK_IOCTL_PRPOFILE_CFG");
            ret = copy_from_user(&profile_cfg, udata, sizeof(struct adlak_profile_cfg_desc));
            if (ret) {
                AML_LOG_ERR("profile_cfg desc copy from user failed!");
                ret = ERR(EFAULT);
                break;
            }
            ret = adlak_os_mutex_lock(&padlak->dev_mutex);
            if (ret) {
                break;
            }
            ret = adlak_profile_config(context, &profile_cfg);
            adlak_os_mutex_unlock(&padlak->dev_mutex);
            /* copy buf info/errcode to user for reference */
            cp_ret = copy_to_user(udata, &profile_cfg, sizeof(struct adlak_profile_cfg_desc));
            if ((ERR(NONE) == ret) && (ERR(NONE) != cp_ret)) {
                ret = cp_ret;
            }

            break;
        case ADLAK_IOCTL_WAIT_UNTIL_FINISH:

            AML_LOG_DEBUG("ADLAK_IOCTL_WAIT_UNTIL_FINISH");
            ret = copy_from_user(&stat_desc, udata, sizeof(struct adlak_get_stat_desc));
            if (ret) {
                AML_LOG_ERR("stat desc copy from user failed!");
                ret = ERR(EFAULT);
                break;
            }
            ret = adlak_wait_until_finished(context, &stat_desc);
            /* copy buf info/errcode to user for reference */
            cp_ret = copy_to_user(udata, &stat_desc, sizeof(struct adlak_get_stat_desc));
            if ((ERR(NONE) == ret) && (ERR(NONE) != cp_ret)) {
                ret = cp_ret;
            }

            break;
        default:
            /*not support command*/
            ret = ERR(ENOTTY);
            break;
    }

    return ret;
}
#ifdef CONFIG_COMPAT
static long drv_ioctl_compat(struct file *filp, unsigned int ioctl_code, unsigned long arg) {
    AML_LOG_DEBUG("%s", __func__);
    return drv_ioctl(filp, ioctl_code, (unsigned long)compat_ptr(arg));
}

#endif
static int drv_mmap(struct file *filp, struct vm_area_struct *vma) {
    int                   ret;
    struct adlak_context *context = filp->private_data;
    uint64_t              iova    = vma->vm_pgoff * ADLAK_PAGE_SIZE;

    AML_LOG_DEBUG("%s iova=0x%lX", __func__, (uintptr_t)iova);

    vma->vm_pgoff = 0;
    ret           = adlak_mem_mmap(context, vma, iova);
    vma->vm_pgoff = iova;
    return ret;
}
unsigned int drv_poll(struct file *filp, struct poll_table_struct *wait) {
    unsigned int            mask      = 0;
    struct adlak_context *  context   = filp->private_data;
    struct adlak_device *   padlak    = context->padlak;
    struct adlak_task *     ptask     = NULL;
    struct adlak_task *     ptask_tmp = NULL;
    struct adlak_workqueue *pwq       = &padlak->queue;

    AML_LOG_DEBUG("%s", __func__);

    if (!context) {
        goto end;
    }
    /*Add to wait queue*/
    poll_wait(filp, (wait_queue_head_t *)context->wait, wait);
    /*check whether the current net_id task is complete*/
    adlak_os_mutex_lock(&context->context_mutex);

    adlak_os_mutex_lock(&pwq->wq_mutex);
    list_for_each_entry_safe(ptask, ptask_tmp, &pwq->finished_list, head) {
        if (ptask && ptask->net_id == context->net_id) {
            {
                mask = POLLPRI;
#ifdef CONFIG_ADLAK_DEBUG_INNNER
                adlak_dbg_inner_update(context, "poll to umd");
#endif
                break;
            }
        }
    }
    adlak_os_mutex_unlock(&pwq->wq_mutex);
    adlak_os_mutex_unlock(&context->context_mutex);
end:
    return mask;
}

static int drv_release(struct inode *inode, struct file *filp) {
    int                   ret     = ERR(NONE);
    struct adlak_device * padlak  = NULL;
    struct adlak_context *context = filp->private_data;
    padlak                        = context->padlak;

    AML_LOG_DEBUG("%s", __func__);
    ret = adlak_destroy_context(padlak, context);
    if (ERR(NONE) != ret) {
        goto err_handle;
    }
    filp->private_data = NULL;

err_handle:
    /* no handler */
    return ret;
}

static int adlak_register_fops(struct adlak_device *padlak) {
    AML_LOG_DEBUG("%s", __func__);
    padlak->fops.owner          = THIS_MODULE;
    padlak->fops.llseek         = no_llseek;
    padlak->fops.open           = &drv_open;
    padlak->fops.release        = &drv_release;
    padlak->fops.poll           = &drv_poll;
    padlak->fops.unlocked_ioctl = &drv_ioctl;
#ifdef CONFIG_COMPAT
    padlak->fops.compat_ioctl = &drv_ioctl_compat;
#endif

    padlak->fops.mmap = &drv_mmap;

    return 0;
}