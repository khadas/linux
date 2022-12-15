/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_profile.h
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

#ifndef __ADLAK_PROFILE_H__
#define __ADLAK_PROFILE_H__

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
struct adlak_profiler {
    struct adlak_mem_handle *profiler_mm_info;
    //
    uint32_t iova;
    uint32_t bufer_size;
    int      wpt;
    int      rpt;
};
struct adlak_profile {
    int32_t          profile_en;  // profilling enable
    uint64_t         profile_iova;
    uint32_t         profile_buf_size;
    uint32_t         profile_wpt;  // profilling write point
    uint32_t         profile_rpt;  // profilling read point
    uint32_t         time_elapsed_us;
    adlak_os_ktime_t start;
    adlak_os_ktime_t finish;
};

/************************** Function Prototypes ******************************/

int adlak_profile_start(struct adlak_device *padlak, struct adlak_profile *profile_data,
                        int32_t invoke_start_idx);

int adlak_profile_stop(struct adlak_device *padlak, struct adlak_profile *profile_data);

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_PROFILE_H__ end define*/
