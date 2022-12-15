/*******************************************************************************
 * Copyright (C) 2022 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_dpm.c
 * @brief    adlak dynamic power management
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

/***************************** Include Files *********************************/
#include "adlak_dpm.h"

#include "adlak_common.h"
#include "adlak_context.h"
#include "adlak_device.h"
#include "adlak_platform_config.h"
#include "adlak_submit.h"
/************************** Constant Definitions *****************************/
#define CONFIG_ADLAK_FREQ_ADJUST_NO (5)
/**************************** Type Definitions *******************************/

struct adlak_power_info {
    struct adlak_device *padlak;
    int32_t              invoke_task_cnt;
    int32_t              cnt_elapsed;
    int32_t              cnt_idel;
    int32_t              cnt_busy;
    int32_t              freq_cfg_idx;
    int32_t              freq_cfg_list[2][CONFIG_ADLAK_FREQ_ADJUST_NO];  // 0:core freq; 1:axi freq
    int32_t              core_freq_cur;
    int32_t              axi_freq_cur;
    int32_t              core_freq_expect;
    int32_t              axi_freq_expect;
};

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/

/************************** Function Prototypes ******************************/
void adlak_dpm_adjust(void *data) {
#if CONFIG_ADLAK_DPM_EN
    struct adlak_power_info *pdpm_info;
    struct adlak_device *    padlak = (struct adlak_device *)data;
    AML_LOG_DEBUG("%s", __func__);
    pdpm_info = (struct adlak_power_info *)padlak->pdpm;
    if (!pdpm_info) {
        return;
    }

    if (pdpm_info->core_freq_expect != pdpm_info->core_freq_cur) {
        // update the freq
        if (0 == pdpm_info->core_freq_expect) {
            AML_LOG_INFO("%s", "power off & update the freq");
            adlak_platform_suspend((void *)padlak);
        } else if (0 == pdpm_info->core_freq_cur) {
            AML_LOG_INFO("%s", "power on & update the freq");
            adlak_platform_resume((void *)padlak);
        } else {
            AML_LOG_INFO("%s,and set core_freq = %d,axi_freq = %d", "update the freq",
                         pdpm_info->core_freq_expect, pdpm_info->axi_freq_expect);
            adlak_platform_set_clock((void *)padlak, true, pdpm_info->core_freq_expect,
                                     pdpm_info->core_freq_expect);
        }
        pdpm_info->core_freq_cur = pdpm_info->core_freq_expect;
        pdpm_info->axi_freq_cur  = pdpm_info->axi_freq_expect;
    }
#endif
}

void adlak_dpm_stage_adjust(void *data, enum ADLAK_DPM_STRATEGY strategy) {
#if CONFIG_ADLAK_DPM_EN
    struct adlak_device *padlak = (struct adlak_device *)data;

    struct adlak_power_info *pdpm_info;
    struct adlak_workqueue * pwq;
    AML_LOG_DEBUG("%s", __func__);
    pdpm_info = (struct adlak_power_info *)padlak->pdpm;
    if (pdpm_info) {
        pwq = &padlak->queue;
        adlak_os_mutex_lock(&padlak->dev_mutex);
        switch (strategy) {
            case ADLAK_DPM_STRATEGY_UP:
                pdpm_info->cnt_busy++;
                // adlak_freq_increase
                pdpm_info->freq_cfg_idx--;
                if (pdpm_info->freq_cfg_idx < 0) {
                    pdpm_info->freq_cfg_idx = 0;
                }

                break;
            case ADLAK_DPM_STRATEGY_DOWN:
                pdpm_info->cnt_idel++;
                // adlak_freq_decrease
                pdpm_info->freq_cfg_idx++;
                if (pdpm_info->freq_cfg_idx >=
                    (ADLAK_ARRAY_SIZE(pdpm_info->freq_cfg_list[0]) - 1)) {
                    pdpm_info->freq_cfg_idx = (ADLAK_ARRAY_SIZE(pdpm_info->freq_cfg_list[0]) - 1);
                }
                break;
            case ADLAK_DPM_STRATEGY_MAX:
                pdpm_info->freq_cfg_idx = 0;
                pdpm_info->cnt_elapsed  = 0;
                pdpm_info->cnt_idel     = 0;
                pdpm_info->cnt_busy     = 0;
                break;
            case ADLAK_DPM_STRATEGY_MIN:
                pdpm_info->freq_cfg_idx = ADLAK_ARRAY_SIZE(pdpm_info->freq_cfg_list[0]) - 1;
                pdpm_info->cnt_elapsed  = 0;
                pdpm_info->cnt_idel     = 0;
                pdpm_info->cnt_busy     = 0;
                break;
            default:
                ASSERT(0);
                break;
        }
        pdpm_info->cnt_elapsed++;
        if (pdpm_info->cnt_elapsed < pdpm_info->cnt_idel ||
            pdpm_info->cnt_elapsed < pdpm_info->cnt_busy) {
            pdpm_info->cnt_elapsed = 0;
            pdpm_info->cnt_idel    = 0;
            pdpm_info->cnt_busy    = 0;
        }

        pdpm_info->core_freq_expect = pdpm_info->freq_cfg_list[0][pdpm_info->freq_cfg_idx];
        pdpm_info->axi_freq_expect  = pdpm_info->freq_cfg_list[1][pdpm_info->freq_cfg_idx];

        if (pdpm_info->core_freq_expect != pdpm_info->core_freq_cur) {
            if (!pdpm_info->invoke_task_cnt) {
                adlak_dpm_adjust(padlak);
            }
        }
        adlak_os_mutex_unlock(&padlak->dev_mutex);
    }
#endif
}

void adlak_dpm_clk_update(void *data, int core_freq, int axi_freq) {
#if CONFIG_ADLAK_DPM_EN
    struct adlak_device *    padlak = (struct adlak_device *)data;
    struct adlak_power_info *pdpm_info;
    int                      i;
    AML_LOG_DEBUG("%s axi_freq=%d, core_freq=%d", __func__, axi_freq, core_freq);

    pdpm_info = (struct adlak_power_info *)padlak->pdpm;
    if (ADLAK_IS_ERR_OR_NULL(pdpm_info)) {
        return;
    }
    pdpm_info->core_freq_cur = core_freq;
    pdpm_info->axi_freq_cur  = axi_freq;

    pdpm_info->core_freq_cur = core_freq;
    pdpm_info->axi_freq_cur  = axi_freq;
    /*update the index of freq_cfg*/
    for (i = 0; i < ADLAK_ARRAY_SIZE(pdpm_info->freq_cfg_list[0]); i++) {
        if (pdpm_info->freq_cfg_list[0][i] == core_freq) {
            pdpm_info->freq_cfg_idx = i;
            break;
        }
    }
    if (i >= ADLAK_ARRAY_SIZE(pdpm_info->freq_cfg_list[0])) {
        if (0 == core_freq) {
            pdpm_info->freq_cfg_idx = (ADLAK_ARRAY_SIZE(pdpm_info->freq_cfg_list[0]) - 1);
        } else {
            pdpm_info->freq_cfg_idx = 0;
        }
    }

    AML_LOG_DEBUG("%s pdpm_info->freq_cfg_idx=%d", __func__, pdpm_info->freq_cfg_idx);
#endif
}
void adlak_dpm_deinit(void *data) {
#if CONFIG_ADLAK_DPM_EN
    struct adlak_device *    padlak = (struct adlak_device *)data;
    struct adlak_power_info *pdpm_info;
    AML_LOG_DEBUG("%s", __func__);
    pdpm_info = (struct adlak_power_info *)padlak->pdpm;
    if (!pdpm_info) {
        return;
    }
    adlak_os_free(pdpm_info);
#endif
}

int adlak_dpm_init(void *data) {
#if CONFIG_ADLAK_DPM_EN
    int                      ret = 0;
    int                      i;
    struct adlak_power_info *pdpm_info;
    struct adlak_device *    padlak = (struct adlak_device *)data;
    AML_LOG_DEBUG("%s", __func__);
    pdpm_info = adlak_os_zalloc(sizeof(struct adlak_power_info), ADLAK_GFP_KERNEL);
    if (!pdpm_info) {
        ret = ERR(ENOMEM);
        goto err;
    }
    padlak->pdpm      = (void *)pdpm_info;
    pdpm_info->padlak = padlak;

    AML_LOG_DEBUG("dpm_period_set =  %d ms", padlak->dpm_period_set);
    // init freq list

    for (i = 0; i < ADLAK_ARRAY_SIZE(pdpm_info->freq_cfg_list[0]); i++) {
        switch (i) {
            case 0:
                pdpm_info->freq_cfg_list[0][i] = padlak->clk_core_freq_set;
                pdpm_info->freq_cfg_list[1][i] = padlak->clk_axi_freq_set;
                break;
            case 1:
                pdpm_info->freq_cfg_list[0][i] = padlak->clk_core_freq_set / 2;
                pdpm_info->freq_cfg_list[1][i] = padlak->clk_axi_freq_set / 2;
                break;
            case 2:
                pdpm_info->freq_cfg_list[0][i] = padlak->clk_core_freq_set / 3;
                pdpm_info->freq_cfg_list[1][i] = padlak->clk_axi_freq_set / 3;
                break;
            case 3:
                pdpm_info->freq_cfg_list[0][i] = padlak->clk_core_freq_set / 4;
                pdpm_info->freq_cfg_list[1][i] = padlak->clk_axi_freq_set / 4;
                break;
            default:
                pdpm_info->freq_cfg_list[0][i] = 0;
                pdpm_info->freq_cfg_list[1][i] = 0;
                break;
        }
    }

    pdpm_info->freq_cfg_idx     = 0;
    pdpm_info->core_freq_expect = pdpm_info->freq_cfg_list[0][pdpm_info->freq_cfg_idx];
    pdpm_info->axi_freq_expect  = pdpm_info->freq_cfg_list[1][pdpm_info->freq_cfg_idx];
    pdpm_info->core_freq_cur =
        pdpm_info->freq_cfg_list[0][(ADLAK_ARRAY_SIZE(pdpm_info->freq_cfg_list[0]) - 1)];
    pdpm_info->axi_freq_cur =
        pdpm_info->freq_cfg_list[1][(ADLAK_ARRAY_SIZE(pdpm_info->freq_cfg_list[0]) - 1)];

    AML_LOG_DEBUG("%s axi_freq=%d, core_freq=%d", __func__, pdpm_info->axi_freq_cur,
                  pdpm_info->core_freq_cur);
    return 0;
err:
    adlak_dpm_deinit(padlak);
    return ret;
#else
    return 0;
#endif
}
int adlak_dmp_get_efficiency(void *data) {
#if CONFIG_ADLAK_DPM_EN
    struct adlak_device *    padlak = (struct adlak_device *)data;
    struct adlak_power_info *pdpm_info;
    int32_t                  efficiency;
    AML_LOG_DEBUG("%s", __func__);
    pdpm_info = (struct adlak_power_info *)padlak->pdpm;
    if (!pdpm_info) {
        return 0;
    }
    efficiency = (pdpm_info->cnt_busy * 100) / (pdpm_info->cnt_elapsed);
    AML_LOG_INFO("adlak efficiency %d/%d = %d%%", pdpm_info->cnt_busy, pdpm_info->cnt_elapsed,
                 efficiency);
    return efficiency;
#else
    return 0;
#endif
}
