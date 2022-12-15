/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_profile.c
 * @brief
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2021/08/26	Initial release
 * </pre>
 *
 ******************************************************************************/

/***************************** Include Files *********************************/
#include "adlak_profile.h"

#include "adlak_submit.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/

/************************** Function Prototypes ******************************/

int adlak_profile_start(struct adlak_device *padlak, struct adlak_profile *profile_data,
                        int32_t invoke_start_idx) {
    int32_t offset = 256 * invoke_start_idx;
    AML_LOG_DEBUG("%s", __func__);
    ASSERT(padlak);
    ASSERT(profile_data);
    if (1 == profile_data->profile_en) {
        profile_data->start = adlak_os_ktime_get();
        adlak_pm_enable(padlak, true);
        adlak_check_dev_is_idle(padlak);
        adlak_pm_reset(padlak);
        adlak_pm_config(padlak, profile_data->profile_iova + offset,
                        profile_data->profile_buf_size - offset, profile_data->profile_rpt);
    }
    return 0;
}

int adlak_profile_stop(struct adlak_device *padlak, struct adlak_profile *profile_data) {
    AML_LOG_DEBUG("%s", __func__);
    ASSERT(padlak);
    ASSERT(profile_data);
    if (1 == profile_data->profile_en) {
        adlak_pm_fush_until_empty(padlak);
        profile_data->profile_wpt = adlak_pm_get_stat(padlak);
        profile_data->profile_rpt = profile_data->profile_wpt;
#if CONFIG_ADLAK_EMU_EN
        profile_data->profile_rpt =
            ((struct adlak_task *)padlak->queue.ptask_sch_cur)->submit_tasks_num * 256;

#endif
        adlak_pm_enable(padlak, false);
        profile_data->finish = adlak_os_ktime_get();
        profile_data->time_elapsed_us =
            (uint32_t)adlak_os_ktime_us_delta(profile_data->finish, profile_data->start);
        AML_LOG_DEBUG("pm used %d ms.", profile_data->time_elapsed_us / 1000);
    }

    return 0;
}
