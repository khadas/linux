/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_platform_config.c
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
#include "adlak_platform_config.h"

#include "adlak_common.h"
#include "adlak_device.h"
#include "adlak_dpm.h"
#include "adlak_hw.h"
#include "adlak_interrupt.h"
#include "adlak_submit.h"

/************************** Constant Definitions *****************************/
#ifndef CONFIG_OF

static struct platform_device *pdev = NULL;
#endif

static uint irqline = 0;

static uint registerMemBase = 0;

static uint registerMemSize = 0;

static uint contiguousMemBase = 0;

static uint contiguousMemSize = 0;

static uint contiguousSramBase = 0;

static uint contiguousSramSize = 0;

static int adlak_has_smmu = -1;

static int adlak_dependency_mode = ADLAK_DEPENDENCY_MODE_MODULE_LAYER;

static int adlak_axi_freq = 800000000;

static int adlak_core_freq = 800000000;

static int adlak_cmd_queue_size = 512 * 1024;

static int adlak_sch_time_max_ms = 10;

static int adlak_dpm_period = 300;

static int adlak_log_level = -1;

#include "./adlak_platform_module_param.c"
/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/

/************************** Function Prototypes ******************************/

#ifndef CONFIG_OF

static struct resource adlak_resource[] = {

    [0] =
        {
            .name  = "adla_reg",
            .start = ADLAK_REG_PHY_ADDR,
            .end   = ADLAK_REG_PHY_ADDR + (ADLAK_REG_SIZE - 1),
            .flags = IORESOURCE_MEM,
        },
    [1] =
        {
            .name  = "adla_reserved_memory",
            .start = ADLAK_MEM_PHY_ADDR,
            .end   = ADLAK_MEM_PHY_ADDR + (ADLAK_MEM_SIZE - 1),
            .flags = IORESOURCE_MEM,
        },

    [2] =
        {
            .name  = "adla",
            .start = ADLAK_IRQ_LINE,
            .end   = ADLAK_IRQ_LINE,
            .flags = IORESOURCE_IRQ,
        },
    [3] =
        {
            .name  = "adla_sram",
            .start = ADLAK_SRAM_PHY_ADDR,
            .end   = ADLAK_SRAM_PHY_ADDR + (ADLAK_SRAM_SIZE - 1),
            .flags = IORESOURCE_MEM,
        },

};

static void platform_adlak_device_release(struct device *dev) { return; }

static uint64_t               adlak_dev_dmamask = DMA_BIT_MASK(34);
static struct platform_device adlak_pdev        = {
    .name          = DEVICE_NAME,
    .id            = 0,
    .resource      = adlak_resource,
    .num_resources = ADLAK_ARRAY_SIZE(adlak_resource),
    .dev =
        {
            .release           = platform_adlak_device_release,
            .dma_mask          = &adlak_dev_dmamask,
            .coherent_dma_mask = DMA_BIT_MASK(34),
        },
};

static void adlak_drv_show_param(void) {
    AML_LOG_DEBUG("%s", __func__);
    AML_LOG_DEFAULT("");
    AML_LOG_DEFAULT("registerMemBase         0x%08llX, ", (uint64_t)adlak_resource[0].start);
    AML_LOG_DEFAULT("registerMemSize         0x%08llX, ",
                    (uint64_t)(adlak_resource[0].end + 1 - adlak_resource[0].start));
    AML_LOG_DEFAULT("contiguousMemBase       0x%08llX, ", (uint64_t)adlak_resource[1].start);
    AML_LOG_DEFAULT("contiguousMemSize       0x%08llX, ",
                    (uint64_t)(adlak_resource[1].end + 1 - adlak_resource[1].start));
    AML_LOG_DEFAULT("irqline                 0x%08llX, ", (uint64_t)adlak_resource[2].start);
    AML_LOG_DEFAULT("contiguousSramBase      0x%08llX, ", (uint64_t)adlak_resource[3].start);
    AML_LOG_DEFAULT("contiguousSramSize      0x%08llX, ",
                    (uint64_t)(adlak_resource[3].end + 1 - adlak_resource[3].start));
    AML_LOG_DEFAULT("\n");
}

static void adlak_update_commandline_parameters(void) {
    AML_LOG_DEBUG("%s", __func__);
    if (0 != registerMemBase) {
        adlak_resource[0].start = registerMemBase;
        if (0 == registerMemSize) {
            registerMemSize = ADLAK_REG_SIZE;
        }
        adlak_resource[0].end = registerMemBase + registerMemSize - 1;
    }
    if (0 != contiguousMemBase) {
        adlak_resource[1].start = contiguousMemBase;
        if (0 == contiguousMemSize) {
            contiguousMemSize = ADLAK_MEM_SIZE;
        }
        adlak_resource[1].end = contiguousMemBase + contiguousMemSize - 1;
    }
    if (0 != irqline) {
        adlak_resource[2].start = irqline;
        adlak_resource[2].end   = adlak_resource[2].start;
    }
    if (0 != contiguousSramBase) {
        adlak_resource[3].start = contiguousSramBase;
        if (0 == contiguousSramSize) {
            contiguousSramSize = ADLAK_SRAM_SIZE;
        }
        adlak_resource[3].end = contiguousSramBase + contiguousSramSize - 1;
    }
    adlak_drv_show_param();
}
#endif
int adlak_platform_device_init(void) {
    int ret = 0;
    AML_LOG_DEBUG("%s", __func__);
#ifndef CONFIG_OF
    pdev = &adlak_pdev;
    adlak_update_commandline_parameters();
    platform_device_register(pdev);
#endif
    return ret;
}

int adlak_platform_device_uninit(void) {
    int ret = 0;
    AML_LOG_DEBUG("%s", __func__);

#ifndef CONFIG_OF
    if (pdev) {
        platform_device_unregister(pdev);
        pdev = NULL;
    }
    AML_LOG_DEBUG("%s End", __func__);
#else

#endif
    return ret;
}
static bool adlak_smmu_available(struct device *dev) {
    bool has_smmu = false;
#ifdef CONFIG_OF
    if (of_property_read_bool(dev->of_node, "smmu")) {
        has_smmu = true;
    }
#else
    if (1 == adlak_has_smmu) {
        has_smmu = true;
    }
#endif
    return has_smmu;
}
static bool adlak_regulator_nn_available(struct device *dev) {
    bool regulator_nna = false;
#ifdef CONFIG_OF
    if (of_property_read_bool(dev->of_node, "regulator_nn")) {
        regulator_nna = true;
    }
#endif
    return regulator_nna;
}
int adlak_platform_get_resource(void *data) {
    int                  ret    = 0;
    struct resource *    res    = NULL;
    struct adlak_device *padlak = (struct adlak_device *)data;

    AML_LOG_DEBUG("%s", __func__);

    padlak->smmu_en = adlak_smmu_available(padlak->dev);

    if (padlak->smmu_en) {
        AML_LOG_INFO("smmu available.\n");
    } else {
        AML_LOG_INFO("smmu not available.\n");
    }

    padlak->regulator_nn_en = adlak_regulator_nn_available(padlak->dev);
    if (padlak->regulator_nn_en) {
        AML_LOG_INFO("regulator nna available.\n");
    } else {
        AML_LOG_INFO("regulator nna not available.\n");
    }

    /* get ADLAK IO */

    res = platform_get_resource_byname(padlak->pdev, IORESOURCE_MEM, "adla_reg");
    if (!res) {
        AML_LOG_ERR("get platform io region failed");
        ret = ERR(EINVAL);
        goto err;
    }
    AML_LOG_DEBUG("get ADLAK IO region: [0x%lX, 0x%lX]", (uintptr_t)res->start,
                  (uintptr_t)res->end);

    padlak->hw_res.adlak_reg_pa   = res->start;
    padlak->hw_res.adlak_reg_size = res->end - res->start + 1;

    res = platform_get_resource_byname(padlak->pdev, IORESOURCE_MEM, "adla_sram");
    if (!res) {
        AML_LOG_INFO("get platform sram region failed");
        padlak->hw_res.adlak_sram_pa   = 0;
        padlak->hw_res.adlak_sram_size = 0;
    } else {
        AML_LOG_DEBUG("get ADLAK SRAM region: [0x%lX, 0x%lX]", (uintptr_t)res->start,
                      (uintptr_t)res->end);
        padlak->hw_res.adlak_sram_pa   = res->start;
        padlak->hw_res.adlak_sram_size = res->end - res->start + 1;
    }
    padlak->hw_res.sram_wrap = 1;  // this configure must sync with adla-compiler,the default is
                                   // wrap enable in adla-compiler.

    /* get reserve-memory */

    res = platform_get_resource_byname(padlak->pdev, IORESOURCE_MEM, "adla_reserved_memory");
    if (!res) {
        AML_LOG_INFO("get platform reserved_memory region failed");
        padlak->hw_res.adlak_resmem_pa   = 0;
        padlak->hw_res.adlak_resmem_size = 0;
    } else {
        AML_LOG_DEBUG("get ADLA reserved_memory region: [0x%lX, 0x%lX]", (uintptr_t)res->start,
                      (uintptr_t)res->end);
        padlak->hw_res.adlak_resmem_pa   = res->start;
        padlak->hw_res.adlak_resmem_size = res->end - res->start + 1;
    }

    /* get interrupt number */
    res = platform_get_resource_byname(padlak->pdev, IORESOURCE_IRQ, "adla");
    if (!res) {
        AML_LOG_ERR("get irqnum failed");
        ret = ERR(EINVAL);
        goto err;
    }
    padlak->hw_res.irqline = res->start;
    AML_LOG_DEBUG("get IRQ number: %d", padlak->hw_res.irqline);

    padlak->hw_timeout_ms = (adlak_sch_time_max_ms);
    AML_LOG_DEBUG("padlak->hw_timeout_ms =  %d ms", adlak_sch_time_max_ms);

    padlak->cmq_buf_info.size       = ADLAK_ALIGN(adlak_cmd_queue_size, 256);
    padlak->cmq_buf_info.total_size = padlak->cmq_buf_info.size;
    AML_LOG_DEBUG("cmq_size=%d byte,total size=%d", padlak->cmq_buf_info.size,
                  padlak->cmq_buf_info.total_size);

    if (adlak_dependency_mode >= ADLAK_DEPENDENCY_MODE_COUNT) {
        adlak_dependency_mode = ADLAK_DEPENDENCY_MODE_MODULE_H_COUNT;
    }
    padlak->dependency_mode = adlak_dependency_mode;
    AML_LOG_DEBUG("padlak->dependency_mode =  %d", padlak->dependency_mode);

    padlak->clk_axi = devm_clk_get(padlak->dev, "adla_axi_clk");
    if (IS_ERR(padlak->clk_axi)) {
        AML_LOG_WARN("Failed to get adla_axi_clk\n");
    }
    padlak->clk_core = devm_clk_get(padlak->dev, "adla_core_clk");
    if (IS_ERR(padlak->clk_core)) {
        AML_LOG_ERR("Failed to get adla_core_clk\n");
    }
    padlak->clk_axi_freq_set  = adlak_axi_freq;
    padlak->clk_core_freq_set = adlak_core_freq;
    padlak->dpm_period_set    = adlak_dpm_period;
    if (adlak_log_level != -1) {
        g_adlak_log_level = adlak_log_level;
#if ADLAK_DEBUG
        g_adlak_log_level_pre = g_adlak_log_level;
#endif
    }
    return 0;
err:
    return ret;
}

int adlak_platform_get_rsv_mem_size(void *dev, uint64_t *mem_size) {
    int                 ret = 0;
    struct resource     res;
    uint64_t            size;
    const __be32 *      ranges = NULL;
    int                 nsize;
    struct device_node *res_mem_dev;
    /* find a memory-region phandle */
    res_mem_dev = of_parse_phandle(((struct device *)dev)->of_node, "memory-region", 0);
    if (!res_mem_dev) {
        goto err;
    }
    ret = of_address_to_resource(res_mem_dev, 0, &res);
    if (!ret) {
        AML_LOG_DEBUG("get cma memory region: [0x%lX, 0x%lX]", (uintptr_t)res.start,
                      (uintptr_t)res.end);
        size = res.end - res.start + 1;
    } else {
        nsize  = of_n_size_cells(res_mem_dev);
        ranges = of_get_property(res_mem_dev, "size", NULL);
        if (!ranges) {
            AML_LOG_ERR("get cma size failed!\n");
            goto err;
        }
        size = of_read_number(ranges, nsize);
    }
    AML_LOG_DEBUG("get cma size=0x%lX", (uintptr_t)size);
    *mem_size = size;
    return 0;
err:
    return -1;
}

int adlak_platform_request_resource(void *data) {
    int                  ret    = 0;
    struct adlak_device *padlak = (struct adlak_device *)data;

    AML_LOG_DEBUG("%s", __func__);
    padlak->hw_res.preg = adlak_create_ioregion((uintptr_t)padlak->hw_res.adlak_reg_pa,
                                                padlak->hw_res.adlak_reg_size);
    if (NULL == padlak->hw_res.preg) {
        AML_LOG_ERR("create ioregion failed");
        ret = ERR(EINVAL);
        goto err;
    }

    padlak->all_task_num     = 0;
    padlak->all_task_num_max = ADLAK_TASK_COUNT_MAX;

    return 0;
err:
    return ret;
}

int adlak_platform_free_resource(void *data) {
    int                  ret    = 0;
    struct adlak_device *padlak = (struct adlak_device *)data;

    if (padlak->hw_res.preg) {
        adlak_destroy_ioregion(padlak->hw_res.preg);
    }
    return ret;
}

void adlak_platform_set_clock(void *data, bool enable, int core_freq, int axi_freq) {
    int                  ret    = 0;
    struct adlak_device *padlak = (struct adlak_device *)data;
    AML_LOG_DEBUG("%s", __func__);

    if (false == enable) {
        if (true == padlak->is_clk_axi_enabled) {
            if (!IS_ERR_OR_NULL(padlak->clk_axi)) {
                clk_disable_unprepare(padlak->clk_axi);
                padlak->is_clk_axi_enabled  = false;
            }
        }
        if (true == padlak->is_clk_core_enabled) {
            if (!IS_ERR_OR_NULL(padlak->clk_core)) {
                clk_disable_unprepare(padlak->clk_core);
                padlak->is_clk_core_enabled = false;
            }
        }
    } else {
        // clk enable
        if (false == padlak->is_clk_axi_enabled) {
            if (!ADLAK_IS_ERR_OR_NULL(padlak->clk_axi)) {
                ret = clk_prepare_enable(padlak->clk_axi);
                if (ret) {
                    AML_LOG_ERR("Failed to enable adla_axi_clk\n");
                }
            }
            padlak->is_clk_axi_enabled = true;
        }
        if (false == padlak->is_clk_core_enabled) {
            if (!ADLAK_IS_ERR_OR_NULL(padlak->clk_core)) {
                ret = clk_prepare_enable(padlak->clk_core);
                if (ret) {
                    AML_LOG_ERR("Failed to enable adla_core_clk\n");
                }
                padlak->is_clk_core_enabled = true;
            }
            if (!ADLAK_IS_ERR_OR_NULL(padlak->clk_axi)) {
                clk_set_rate(padlak->clk_axi, axi_freq);
                if (ret) {
                    AML_LOG_ERR("Failed to set adla_axi_clk\n");
                }
                padlak->clk_axi_freq_real = (int)clk_get_rate(padlak->clk_axi);
                AML_LOG_DEBUG("adlak_axi clk requirement of %d Hz,and real val is %d Hz.", axi_freq,
                              padlak->clk_axi_freq_real);
            }
            if (!ADLAK_IS_ERR_OR_NULL(padlak->clk_core)) {
                ret = clk_set_rate(padlak->clk_core, core_freq);
                if (ret) {
                    AML_LOG_ERR("Failed to set adla_core_clk\n");
                }
                padlak->clk_core_freq_real = (int)clk_get_rate(padlak->clk_core);

                AML_LOG_DEBUG("adlak_core clk requirement of %d Hz,and real val is %d Hz.",
                              core_freq, padlak->clk_core_freq_real);
            }
        }
    }
    adlak_dpm_clk_update(padlak, core_freq, axi_freq);
}

void adlak_platform_set_power(void *data, bool enable) {
#if CONFIG_HAS_PM_DOMAIN
    int                  ret    = 0;
    struct adlak_device *padlak = (struct adlak_device *)data;
#endif
    AML_LOG_DEBUG("%s", __func__);
    if (false == enable) {
#if CONFIG_HAS_PM_DOMAIN
        pm_runtime_put_sync(padlak->dev);
        if (pm_runtime_enabled(padlak->dev)) {
            pm_runtime_disable(padlak->dev);
        }
#endif

    } else {
#if CONFIG_HAS_PM_DOMAIN
        pm_runtime_enable(padlak->dev);
        if (pm_runtime_enabled(padlak->dev)) {
            ret = pm_runtime_get_sync(padlak->dev);
            if (ret < 0) {
                AML_LOG_ERR("Getpower failed\n");
            }
        }
#endif
    }
}
int adlak_platform_pm_init(void *data) {
    int                  ret    = 0;
    struct adlak_device *padlak = (struct adlak_device *)data;
    AML_LOG_DEBUG("%s", __func__);
#if CONFIG_HAS_PM_DOMAIN
    ret = pm_runtime_set_active(padlak->dev);
#endif
    if (ret < 0) {
        AML_LOG_ERR("Get power failed\n");
        goto end;
    }
    // power on
    adlak_platform_set_power(padlak, true);
    // clk enable
    adlak_platform_set_clock(padlak, true, padlak->clk_core_freq_set, padlak->clk_axi_freq_set);
    padlak->is_suspend       = false;
    padlak->need_reset_queue = true;
end:
    return ret;
}
void adlak_platform_pm_deinit(void *data) {
    struct adlak_device *padlak = (struct adlak_device *)data;
    AML_LOG_DEBUG("%s", __func__);
    // clk disable
    adlak_platform_set_clock(padlak, false, 0, 0);
    // power off
    adlak_platform_set_power(padlak, false);
    padlak->is_suspend = true;
}

void adlak_platform_resume(void *data) {
#if CONFIG_ADLAK_DPM_EN
    struct adlak_device *padlak = (struct adlak_device *)data;
    AML_LOG_INFO("%s", __func__);
    if (false != padlak->is_suspend) {
        // power on
        adlak_platform_set_power(padlak, true);
        // clk enable
        adlak_platform_set_clock(padlak, true, padlak->clk_core_freq_set, padlak->clk_axi_freq_set);
        padlak->is_suspend = false;
        adlak_hw_dev_resume(padlak);
    }
#endif
}

void adlak_platform_suspend(void *data) {
#if CONFIG_ADLAK_DPM_EN
    struct adlak_device *padlak = (struct adlak_device *)data;
    AML_LOG_INFO("%s", __func__);
    if (false == padlak->is_suspend) {
        adlak_hw_dev_suspend(padlak);
        padlak->is_suspend       = true;
        padlak->need_reset_queue = true;
        // clk disable
        adlak_platform_set_clock(padlak, false, 0, 0);
        // power off
        adlak_platform_set_power(padlak, false);
    }
#endif
}
