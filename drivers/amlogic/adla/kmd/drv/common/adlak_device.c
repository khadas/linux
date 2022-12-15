/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_device.c
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2022/04/11	Initial release
 * </pre>
 *
 ******************************************************************************/

/***************************** Include Files *********************************/

#include "adlak_common.h"
#include "adlak_dpm.h"
#include "adlak_hw.h"
#include "adlak_interrupt.h"
#include "adlak_mm.h"
#include "adlak_platform_config.h"
#include "adlak_profile.h"
#include "adlak_submit.h"
/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/

/************************** Function Prototypes ******************************/

#ifdef CONFIG_ADLAK_DEBUG_INNNER
#include "adlak_dbg.c"
#endif
/**
 * @brief init device in kernel mode
 *
 * @param padlak
 * @return int
 */
int adlak_device_init(struct adlak_device *padlak) {
    int ret = 0;
    adlak_os_printf("%s kmd version: %s\n", DEVICE_NAME, ADLAK_VERSION);
    adlak_os_printf("%s DDK version: 1.7.1\n",DEVICE_NAME);
    adlak_os_mutex_init(&padlak->dev_mutex);
    adlak_os_spinlock_init(&padlak->spinlock);
    INIT_LIST_HEAD(&padlak->context_list);

    ret = adlak_os_mutex_lock(&padlak->dev_mutex);
    if (ret) {
        goto err_lock;
    }

    ret = adlak_platform_pm_init(padlak);
    if (ret) {
        goto err_pm_init;
    }

    ret = adlak_mem_init(padlak);
    if (ret) {
        goto err_mem_init;
    }

    ret = adlak_queue_init(padlak);
    if (ret) {
        goto err_queue_init;
    }

    ret = adlak_dpm_init(padlak);
    if (ret) {
        goto err_dpm_init;
    }

    ret = adlak_hw_init(padlak);
    if (ret) {
        goto err_hw_init;
    }

    ret = adlak_dev_inference_init(padlak);
    if (ret) {
        AML_LOG_ERR("inference init fail!");
        goto err_inference_init;
    }

    ret = adlak_irq_init(padlak);
    if (ret) {
        AML_LOG_ERR("irq init fail!");
        ret = ERR(EINVAL);
        goto err_irq_init;
    }
    adlak_platform_suspend(padlak);

    adlak_os_mutex_unlock(&padlak->dev_mutex);

    return 0;

err_irq_init:

    adlak_hw_deinit(padlak);
err_hw_init:
    adlak_dpm_deinit(padlak);
err_inference_init:
    adlak_dev_inference_deinit(padlak);

err_dpm_init:
    adlak_queue_deinit(padlak);
err_queue_init:
    adlak_mem_deinit(padlak);

err_mem_init:
    adlak_platform_pm_deinit(padlak);
err_pm_init:
    adlak_os_mutex_unlock(&padlak->dev_mutex);
err_lock:
    return -1;
}

/**
 * @brief deinit device in kernel mode
 *
 * @param padlak
 * @return int
 */
int adlak_device_deinit(struct adlak_device *padlak) {
    int ret = 0;
    ret     = adlak_os_mutex_lock(&padlak->dev_mutex);
    if (ret) {
        goto err_lock;
    }
    adlak_platform_resume(padlak);
    adlak_os_mutex_unlock(&padlak->dev_mutex);
    adlak_dev_inference_deinit(padlak); /*the inference thread will internally call the dev_mutex*/
    adlak_os_mutex_lock(&padlak->dev_mutex);
    adlak_hw_deinit(padlak);
    adlak_dpm_deinit(padlak);
    adlak_irq_deinit(padlak);
    adlak_queue_deinit(padlak);
    adlak_os_mutex_unlock(&padlak->dev_mutex);
    adlak_destroy_all_context(padlak);
    adlak_os_mutex_lock(&padlak->dev_mutex);
    adlak_mem_deinit(padlak);
    adlak_platform_pm_deinit(padlak);
    adlak_os_mutex_unlock(&padlak->dev_mutex);

    return 0;
err_lock:
    return -1;
}

/**
 * @brief called in top-half IRQ handler
 *
 * @param padlak
 * @return int
 */
int adlak_irq_proc(struct adlak_device *const padlak) {
    struct adlak_irq_status *   irqstatus;
    struct adlak_task *         ptask      = NULL;
    struct adlak_hw_stat *      phw_stat   = NULL;
    struct adlak_dev_inference *pinference = NULL;
    // adlak_cant_sleep();
    ptask = padlak->queue.ptask_sch_cur;
    if (NULL == ptask) {
        return -1;
    }
    phw_stat   = &ptask->hw_stat;
    pinference = &padlak->queue.dev_inference;

    irqstatus                          = adlak_hal_get_irq_status(phw_stat);
    padlak->cmq_buf_info.cmq_rd_offset = phw_stat->ps_rbf_rpt;

    AML_LOG_INFO("IRQ status[0x%08X]", irqstatus->irq_masked);

    if (phw_stat->hw_info->irq_cfg.mask_normal & irqstatus->irq_masked) {
        if (0 == (irqstatus->irq_masked ^ ADLAK_IRQ_MASK_LAYER_END)) {
            AML_LOG_INFO("IRQ layer end only.");  // just for test
            adlak_hal_irq_clear(padlak, irqstatus->irq_masked);
            adlak_os_sema_give_from_isr(padlak->paser_refresh);
            return 0;
        }
    }

    if (unlikely(irqstatus->irq_masked == 0)) {
        AML_LOG_WARN("irq_masked[0x%08X] irq_raw[0x%08X]", irqstatus->irq_masked,
                     irqstatus->irq_raw);
        return -1;
    }

    /* save register status */
    adlak_get_hw_status(phw_stat);

    if (irqstatus->irq_masked & phw_stat->hw_info->irq_cfg.mask_err) {
        AML_LOG_ERR("interrupt disabled temporary!");
        adlak_hal_irq_enable(padlak, false);
    }
    adlak_hal_irq_clear(padlak, irqstatus->irq_masked);
    adlak_os_sema_give_from_isr(pinference->sem_irq);
    adlak_os_sema_give_from_isr(padlak->paser_refresh);

    return 0;
}
