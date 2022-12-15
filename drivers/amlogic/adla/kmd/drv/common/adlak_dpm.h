/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_dpm.h
 * @brief     adlak dynamic power management
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   	Who				Date				Changes
 * ----------------------------------------------------------------------------
 * 1.00a shiwei.sun@amlogic.com	2022/03/09	Initial release
 * </pre>
 *
 ******************************************************************************/

#ifndef __ADLAK_DPM_H__
#define __ADLAK_DPM_H__

/***************************** Include Files *********************************/

#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/
enum ADLAK_DPM_STRATEGY {
    ADLAK_DPM_STRATEGY_MIN = 0,
    ADLAK_DPM_STRATEGY_UP,
    ADLAK_DPM_STRATEGY_DOWN,
    ADLAK_DPM_STRATEGY_MAX,
};

/************************** Function Prototypes ******************************/
void adlak_dpm_deinit(void *data);

int  adlak_dpm_init(void *data);
void adlak_dpm_adjust(void *data);
void adlak_dpm_clk_update(void *data, int core_freq, int axi_freq);
int  adlak_dmp_get_efficiency(void *data);
void adlak_dpm_stage_adjust(void *data, enum ADLAK_DPM_STRATEGY strategy);

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_DPM_H__ end define*/
