/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_queue.h
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

#ifndef __ADLAK_QUEUE_H__
#define __ADLAK_QUEUE_H__

/***************************** Include Files *********************************/
#include "adlak_api.h"
#include "adlak_common.h"
#include "adlak_context.h"
#include "adlak_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/
struct adlak_global_id {
    int32_t  global_id_rs;
    int32_t  global_id_pwe;
    int32_t  global_id_pwx;
    uint32_t global_id_sw;  // global sw index
    uint32_t global_time_stamp;
    int32_t  start_id_rs;
    int32_t  start_id_pwe;
    int32_t  start_id_pwx;
};

struct adlak_dev_inference {
    adlak_os_spinlock_t spinlock;
    adlak_os_sema_t     sem_irq;
    adlak_os_sema_t     sem_dpm;
    int                 dmp_timeout;
    adlak_os_timer_t    emu_timer;
    adlak_os_timer_t    dpm_timer;
    adlak_os_thread_t   thrd_inference;
};
struct adlak_workqueue {
    adlak_os_mutex_t wq_mutex;
    adlak_os_sema_t  wk_update;
    struct list_head pending_list;
    struct list_head ready_list;
    struct list_head scheduled_list;
    struct list_head finished_list;

    int   sched_num;
    int   sched_num_max;
    int   ready_num;
    int   ready_num_max;
    int   pending_num;
    int   finished_num;
    void *ptask_sch_cur;

    struct adlak_global_id id_backup;  // for rollback
    struct adlak_global_id id_cur;     // for rollback

    struct adlak_dev_inference dev_inference;
};
/************************** Function Prototypes ******************************/

int adlak_queue_init(struct adlak_device *padlak);
int adlak_queue_deinit(struct adlak_device *padlak);
int adlak_queue_reset(struct adlak_device *padlak);

int adlak_debug_invoke_list_dump(struct adlak_device *padlak, uint32_t debug);

int adlak_test_irq_emu(struct adlak_device *padlak);

uint32_t adlak_cmd_get_sw_id(struct adlak_workqueue *pwq);
void     adlak_wq_globalid_backup(struct adlak_device *padlak);
void     adlak_wq_globalid_rollback(struct adlak_device *padlak);
int      adlak_dev_inference_init(struct adlak_device *padlak);
int      adlak_dev_inference_deinit(struct adlak_device *padlak);
#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_QUEUE_H__ end define*/
