/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_hw.c
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

#include "adlak_hw.h"

#include "adlak_common.h"
#include "adlak_device.h"
// #include "adlak_dma_smmu.h"
#include "adlak_io.h"
#include "adlak_mm_common.h"
#include "adlak_submit.h"
/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Variable Definitions *****************************/
struct __adlak_priv_caps {
    struct adlak_caps_desc uapi_caps_data;
    uint32_t               others;
};
static struct __adlak_priv_caps adlak_priv_caps;

/************************** Function Prototypes ******************************/
static void adlak_hal_get_revision(void *data);

static void adlak_parser_start(struct io_region *region) {
    HAL_ADLAK_PS_CTRL_S d;
    AML_LOG_DEBUG("%s", __func__);
    d.all = adlak_read32(region, REG_ADLAK_0X50);
    if (0 == d.bitc.ps_start) {
        d.bitc.ps_start = 1;
        adlak_write32(region, REG_ADLAK_0X50, d.all);
    }
}

static int adlak_parser_cmq_set(struct io_region *region, uint32_t wpt) {
    int ret = 0;
    AML_LOG_DEBUG("%s", __func__);
    if (wpt % 16 != 0) {
        AML_LOG_ERR("rbf_wpt must align with 16 bytes");
        ret = -1;
        goto end;
    }
    AML_LOG_DEBUG("Set CMQ WPT=0x%08X", wpt);
    adlak_write32(region, REG_ADLAK_0X6C, wpt & 0x0FFFFFFF);
end:
    return -1;
}

void adlak_pm_enable(void *padlak, uint32_t en) {
    struct io_region *region = NULL;

    HAL_ADLAK_PM_EN_S d;
    AML_LOG_DEBUG("%s", __func__);
    ASSERT(padlak);
    region = ((struct adlak_device *)padlak)->hw_res.preg;
    d.all  = 0;
    if (0 != en) {
        d.bitc.pm_en = 0x03;
    }
    adlak_write32(region, REG_ADLAK_0XF0, d.all);
}

void adlak_pm_reset(void *padlak) {
    struct io_region *region = NULL;

    HAL_ADLAK_PM_EN_S d;
    AML_LOG_DEBUG("%s", __func__);
    ASSERT(padlak);
    region          = ((struct adlak_device *)padlak)->hw_res.preg;
    d.all           = adlak_read32(region, REG_ADLAK_0XF0);
    d.bitc.pm_swrst = 1;
    adlak_write32(region, REG_ADLAK_0XF0, d.all);
    d.bitc.pm_swrst = 0;
    adlak_write32(region, REG_ADLAK_0XF0, d.all);
}

void adlak_pm_config(void *padlak, uint32_t addr, uint32_t buf_size, uint32_t wpt) {
    struct io_region *region = NULL;
    AML_LOG_DEBUG("%s", __func__);
    ASSERT(padlak);
    region = ((struct adlak_device *)padlak)->hw_res.preg;
    adlak_write32(region, REG_ADLAK_0XF4, addr);
    adlak_write32(region, REG_ADLAK_0XF8, buf_size);
}

uint32_t adlak_pm_get_stat(void *padlak) {
    struct io_region *region = NULL;
    uint32_t          val    = 0;
    AML_LOG_DEBUG("%s", __func__);
    ASSERT(padlak);
    region = ((struct adlak_device *)padlak)->hw_res.preg;
    val    = adlak_read32(region, REG_ADLAK_0XFC);

    return val;
}

int adlak_pm_fush_until_empty(void *padlak) {
    struct io_region * region = NULL;
    HAL_ADLAK_PM_STS_S d;
    uint32_t           cnt;
    AML_LOG_DEBUG("%s", __func__);
    ASSERT(padlak);
    region = ((struct adlak_device *)padlak)->hw_res.preg;

    // flush pm
    d.all           = 0;
    d.bitc.pm_flush = 1;
    adlak_write32(region, REG_ADLAK_0X104, d.all);

    /*2. Wait until pm empty*/
    cnt = 0;
    do {
        d.all = adlak_read32(region, REG_ADLAK_0X104);
        cnt++;
        if (cnt > 3000) {
            AML_LOG_WARN("wait pm empty timeout!");
            ASSERT(0);
            return -1;
        }
#if CONFIG_ADLAK_EMU_EN
        break;
#endif
    } while (d.bitc.pm_fifo_empty == 0);

    return 0;
}

static int adlak_hal_soft_reset(void *data) {
    struct adlak_device *padlak = (struct adlak_device *)data;
    struct io_region *   region = NULL;
    HAL_ADLAK_AB_CTL_S   d_ab;
    HAL_ADLAK_SWRST_S    d;
    uint32_t             cnt;
    AML_LOG_DEBUG("%s", __func__);

    region = padlak->hw_res.preg;

    /*1. Stop the memory access*/
    d_ab.all = adlak_read32(region, REG_ADLAK_0XA0);

    d_ab.bitc.ab_force_stop_en = 1;
    adlak_write32(region, REG_ADLAK_0XA0, d_ab.all);

    /*2. Wait until memory access complete*/
    cnt = 0;
    do {
        adlak_os_msleep(1);
        d_ab.all = adlak_read32(region, REG_ADLAK_0XA0);
        cnt++;
        if (cnt > 3000) {
            AML_LOG_ERR("wait ab_force_stop timeout!");
            ASSERT(0);
            return -1;
        }

#if CONFIG_ADLAK_EMU_EN
        break;
#endif
    } while (d_ab.bitc.ab_force_stop_idle == 0);

    /*3. Release the memory access*/
    d_ab.all                     = adlak_read32(region, REG_ADLAK_0XA0);
    d_ab.bitc.ab_force_stop_en   = 0;
    d_ab.bitc.ab_force_stop_idle = 0;
    adlak_write32(region, REG_ADLAK_0XA0, d_ab.all);

    /*4. Reset adlak*/
    d.bitc.adlak_swrst = 1;
    adlak_write32(region, REG_ADLAK_0X20, d.all);

    adlak_os_msleep(1);

    d.bitc.adlak_swrst = 0;
    adlak_write32(region, REG_ADLAK_0X20, d.all);
    return 0;
}

/**
 * @brief hardware reset if soc support this.
 *
 * @param data
 * @return int
 */
static int adlak_hal_hard_reset(void *data) {
    // TODO(shiwei.sun)The function is empty because the hardware does not support it
    AML_LOG_DEBUG("%s", __func__);

    return 0;
}

static uint32_t adlak_hal_get_reg(void *data, uint32_t offset) {
    struct adlak_device *padlak = (struct adlak_device *)data;
    struct io_region *   region = padlak->hw_res.preg;
    uint32_t             val;
    val = adlak_read32(region, offset);
    AML_LOG_DEBUG("read reg [0x%X] = 0x%X", offset, val);
    return val;
}

int adlak_hal_set_mmu(void *data, bool enable) {
    struct adlak_device * padlak   = (struct adlak_device *)data;
    struct adlak_hw_info *phw_info = (struct adlak_hw_info *)padlak->hw_info;

    // struct adlak_workqueue *pwq    = &padlak->queue;
    struct io_region *  region = padlak->hw_res.preg;
    HAL_ADLAK_SMMU_EN_S d;
    uint64_t            smmu_entry;
    AML_LOG_DEBUG("%s", __func__);

    if (enable) {
        d.bitc.smmu_en = true;
    } else {
        d.bitc.smmu_en = false;
    }
    adlak_write32(region, REG_ADLAK_0XC0, d.all);

    if (enable) {
        smmu_entry = phw_info->smmu_entry;
        smmu_entry = ((smmu_entry >> 5) << 12);
        AML_LOG_INFO("SMMU Enable, and set smmu_entry=0x%lX", (uintptr_t)smmu_entry);

        adlak_write32(region, REG_ADLAK_0XC4, (uint32_t)(smmu_entry & (0xFFFFFFFF)));
        adlak_write32(region, REG_ADLAK_0XC8, (uint32_t)((smmu_entry >> 32) & (0xFFFFFFFF)));
        adlak_hal_smmu_cache_invalid(data, 0);
    } else {
        AML_LOG_INFO("SMMU Disable");
    }

    return 0;
}
void adlak_hal_smmu_cache_invalid(void *data, dma_addr_t iova) {
    struct adlak_device *padlak = (struct adlak_device *)data;
    // struct adlak_hw_info *   phw_info = (struct adlak_hw_info *)padlak->hw_info;
    struct io_region *       region = padlak->hw_res.preg;
    HAL_ADLAK_SMMU_INV_CTL_S d;
    uint32_t                 cnt;
    //  AML_LOG_DEBUG( "%s", __func__);

    d.all = 0;
    if (likely(0 == iova)) {
        // invalid all
        d.bitc.smmu_invalid_rdy = 1;
        d.bitc.smmu_invalid_all = 0x0F;
    } else {
        // invalid one
        d.bitc.smmu_invalid_rdy = 1;
        d.bitc.smmu_invalid_one = 0x0B;
    }
    if (ADLAK_HW_STAT_UNKNOWN != padlak->hw_stat.stat) {
        adlak_write32(region, REG_ADLAK_0XD8, (uint32_t)iova);
        adlak_write32(region, REG_ADLAK_0XD4, (uint32_t)d.all);
    }

    /*2. Wait until smmu_invalid complete*/
    cnt = 0;
    do {
        d.all = adlak_read32(region, REG_ADLAK_0XD4);
        cnt++;
        if (cnt > 3000) {
            AML_LOG_ERR("wait smmu_invalid_rdy timeout!");
            ASSERT(0);
        }
#if CONFIG_ADLAK_EMU_EN
        break;
#endif
    } while (d.bitc.smmu_invalid_rdy == 1);
}

void adlak_parser_set_pend_timer(struct io_region *region, uint32_t time) {
    HAL_ADLAK_PS_PEND_EN_S d;
    AML_LOG_DEBUG("%s", __func__);
    d.bitc.ps_pend_timer_en = 0;
    if (time) {
        d.bitc.ps_pend_timer_en = 1;
    }
    adlak_write32(region, REG_ADLAK_0X84, d.all);
    adlak_write32(region, REG_ADLAK_0X88, time);
}
void adlak_parser_set_apb_timeout(struct io_region *region, uint32_t time) {
    // HAL_ADLAK_PS_PEND_EN_S d;
    AML_LOG_DEBUG("%s", __func__);
    adlak_write32(region, REG_ADLAK_0X4, time);
}

void adlak_hal_enable(void *data, uint32_t en) {
    struct adlak_device *padlak = (struct adlak_device *)data;
    HAL_ADLAK_ADLAK_EN_S d;
    AML_LOG_DEBUG("%s", __func__);

    d.bitc.adlak_en = 0;
    if (en) {
        d.bitc.adlak_en = 1;
    }
    adlak_write32(padlak->hw_res.preg, REG_ADLAK_0X24, d.all);
}

void adlak_hal_irq_enable(void *data, uint32_t en) {
    struct adlak_device * padlak   = (struct adlak_device *)data;
    struct adlak_hw_info *phw_info = (struct adlak_hw_info *)padlak->hw_info;
    AML_LOG_DEBUG("%s", __func__);

    if (!en) {
        adlak_write32(padlak->hw_res.preg, REG_ADLAK_0X18, phw_info->irq_cfg.mask);
        adlak_write32(padlak->hw_res.preg, REG_ADLAK_0X14, 0);
    } else {
        adlak_write32(padlak->hw_res.preg, REG_ADLAK_0X18, 0xFFFF);  // irq clear
        adlak_write32(padlak->hw_res.preg, REG_ADLAK_0X14, phw_info->irq_cfg.mask);
    }
}

void adlak_hal_irq_clear(void *data, uint32_t clr_bits) {
    struct adlak_device *padlak = (struct adlak_device *)data;
    // struct adlak_hw_info *phw_info = (struct adlak_hw_info *)padlak->hw_info;
    AML_LOG_DEBUG("%s", __func__);

    adlak_write32(padlak->hw_res.preg, REG_ADLAK_0X18,
                  clr_bits & ((ADLAK_IRQ_MASK_INVALID_IOVA << 1) - 1));
}

static void adlak_hal_get_revision(void *data) {
    struct adlak_device * padlak   = (struct adlak_device *)data;
    struct adlak_hw_info *phw_info = (struct adlak_hw_info *)padlak->hw_info;
    struct io_region *    region   = padlak->hw_res.preg;
    HAL_ADLAK_REV_S       d;
    AML_LOG_DEBUG("%s", __func__);
    d.all                    = adlak_read32(region, REG_ADLAK_0X0);
    phw_info->rev.bitc.major = d.bitc.major_rev;
    phw_info->rev.bitc.minor = d.bitc.minor_rev;
    AML_LOG_INFO("ADLAK HW Revesion:%d.%d,REG_ADLAK_0X2C=0x%08X", phw_info->rev.bitc.major,
                 phw_info->rev.bitc.minor, adlak_read32(region, REG_ADLAK_0X2C));
}

void adlak_hal_get_all_regs(struct adlak_hw_stat *phw_stat) {
    struct io_region *region = NULL;
    int               idx;
    AML_LOG_DEBUG("%s", __func__);
    ASSERT(phw_stat);
    region = phw_stat->hw_info->region;

    AML_LOG_DEBUG("%s", __func__);

    for (idx = 0; idx < sizeof(phw_stat->hw_info->reg_lst) / sizeof(phw_stat->hw_info->reg_lst[0]);
         idx++) {
        phw_stat->regs_stat[idx] = adlak_read32(region, phw_stat->hw_info->reg_lst[idx]);
    }
}
static void adlak_all_regs_dump(struct adlak_hw_stat *phw_stat) {
    int  idx;
    char reg_name[64];
    AML_LOG_ERR("%s", __func__);
    for (idx = 0; idx < sizeof(phw_stat->hw_info->reg_lst) / sizeof(phw_stat->hw_info->reg_lst[0]);
         idx++) {
        adlak_get_reg_name(phw_stat->hw_info->reg_lst[idx], reg_name, sizeof(reg_name));
        AML_LOG_ERR("0x%-*x%-*s0x%08x", 6, phw_stat->hw_info->reg_lst[idx], 22, reg_name,
                    phw_stat->regs_stat[idx]);
    }
}

uint32_t adlak_get_hw_status(struct adlak_hw_stat *phw_stat) {
    struct io_region *region = NULL;
    AML_LOG_DEBUG("%s", __func__);
    ASSERT(phw_stat);
    region = phw_stat->hw_info->region;
#if ADLAK_DEBUG
    phw_stat->clk_cnt.all    = adlak_read32(region, REG_ADLAK_0X2C);
    phw_stat->ps_err_dat     = adlak_read32(region, REG_ADLAK_0X58);
    phw_stat->ps_finish_id   = adlak_read32(region, REG_ADLAK_0X78);
    phw_stat->ps_status      = adlak_read32(region, REG_ADLAK_0X54);
    phw_stat->ps_idle_status = adlak_read32(region, REG_ADLAK_0X5C);

    phw_stat->ps_rbf_base = adlak_read32(region, REG_ADLAK_0X64);
    phw_stat->ps_rbf_size = adlak_read32(region, REG_ADLAK_0X68);
#endif
    phw_stat->ps_rbf_wpt = adlak_read32(region, REG_ADLAK_0X6C);
    phw_stat->ps_rbf_rpt = adlak_read32(region, REG_ADLAK_0X70);
    phw_stat->ps_rbf_ppt = adlak_read32(region, REG_ADLAK_0X74);
    if (phw_stat->ps_status & ADLAK_IRQ_MASK_INVALID_IOVA) {
        phw_stat->smmu_err_dft_pa = adlak_read32(region, REG_ADLAK_0XDC);
        phw_stat->smmu_err_mdl_id = adlak_read32(region, REG_ADLAK_0XE0);
        phw_stat->smmu_err_iova   = adlak_read32(region, REG_ADLAK_0XE4);
    }

    if ((phw_stat->hw_info->irq_cfg.mask_err | ADLAK_IRQ_MASK_SW_TIMEOUT) &
        phw_stat->irq_status.irq_masked) {
        adlak_hal_get_all_regs(phw_stat);
    }

    return phw_stat->irq_status.irq_masked;
}

struct adlak_irq_status *adlak_hal_get_irq_status(struct adlak_hw_stat *phw_stat) {
    struct io_region *region = NULL;
    AML_LOG_DEBUG("%s", __func__);
    ASSERT(phw_stat);
    region                          = phw_stat->hw_info->region;
    phw_stat->irq_status.irq_masked = adlak_read32(region, REG_ADLAK_0X10);
    phw_stat->irq_status.irq_raw    = adlak_read32(region, REG_ADLAK_0X18);
    if (true == phw_stat->irq_status.timeout) {
        phw_stat->irq_status.irq_masked |= ADLAK_IRQ_MASK_SW_TIMEOUT;
    }
    phw_stat->irq_status.time_stamp    = adlak_read32(region, REG_ADLAK_0X60);
    phw_stat->irq_status.status_report = adlak_read32(region, REG_ADLAK_0X1C);

    phw_stat->ps_rbf_rpt = adlak_read32(region, REG_ADLAK_0X70);

    return &phw_stat->irq_status;
}

void adlak_status_dump(struct adlak_hw_stat *phw_stat) {
    AML_LOG_DEBUG("%s", __func__);
    AML_LOG_DEBUG("REG_ADLAK_0X2C : 0x%08X", phw_stat->clk_cnt.all);
    AML_LOG_DEBUG("REG_ADLAK_0X10   : 0x%08X", phw_stat->irq_status.irq_masked);
    AML_LOG_DEBUG("REG_ADLAK_0X18      : 0x%08X", phw_stat->irq_status.irq_raw);
    AML_LOG_DEBUG("REG_ADLAK_0X58   : 0x%08X", phw_stat->ps_err_dat);
    AML_LOG_DEBUG("REG_ADLAK_0X78 : 0x%08X", phw_stat->ps_finish_id);
    AML_LOG_DEBUG("REG_ADLAK_0X54       : 0x%08X", phw_stat->ps_status);
    AML_LOG_DEBUG("REG_ADLAK_0X60: 0x%08X", phw_stat->irq_status.time_stamp);
    AML_LOG_DEBUG("REG_ADLAK_0X5C  : 0x%08X", phw_stat->ps_idle_status);
    AML_LOG_DEBUG("REG_ADLAK_0XDC     : 0x%08X", phw_stat->smmu_err_dft_pa);
    AML_LOG_DEBUG("REG_ADLAK_0XE0 : 0x%08X", phw_stat->smmu_err_mdl_id);
    AML_LOG_DEBUG("REG_ADLAK_0XE4  : 0x%08X", phw_stat->smmu_err_iova);
    AML_LOG_DEBUG("REG_ADLAK_0X64  : 0x%08X", phw_stat->ps_rbf_base);
    AML_LOG_DEBUG("REG_ADLAK_0X68  : 0x%08X", phw_stat->ps_rbf_size);
    AML_LOG_DEBUG("REG_ADLAK_0X6C   : 0x%08X", phw_stat->ps_rbf_wpt);
    AML_LOG_DEBUG("REG_ADLAK_0X70   : 0x%08X", phw_stat->ps_rbf_rpt);
    AML_LOG_DEBUG("REG_ADLAK_0X74   : 0x%08X", phw_stat->ps_rbf_ppt);

    if ((phw_stat->hw_info->irq_cfg.mask_err | ADLAK_IRQ_MASK_SW_TIMEOUT) &
        phw_stat->irq_status.irq_masked) {
        adlak_all_regs_dump(phw_stat);
    }
}

uint32_t adlak_hal_get_ps_rpt(void *data) {
#if CONFIG_ADLAK_EMU_EN
    struct adlak_device *padlak = (struct adlak_device *)data;
    return padlak->cmq_buf_info.cmq_rd_offset;

#endif
    return adlak_hal_get_reg(data, REG_ADLAK_0X70);
}
int adlak_hal_set_cmq(void *data) {
    struct adlak_device *padlak = (struct adlak_device *)data;

    struct io_region *region = padlak->hw_res.preg;
    AML_LOG_DEBUG("%s", __func__);

    AML_LOG_DEBUG("cmq_addr=0x%lX,size=0x%lX",
                  (uintptr_t)padlak->cmq_buf_info.cmq_mm_info->iova_addr,
                  (uintptr_t)padlak->cmq_buf_info.cmq_mm_info->req.bytes);
    if (padlak->cmq_buf_info.cmq_mm_info->iova_addr > 0xFFFFFFFF) {
        AML_LOG_ERR("The address space of command queue must be in (0 - 4G)! ");
        ASSERT(0);
    }
    adlak_write32(region, REG_ADLAK_0X64, padlak->cmq_buf_info.cmq_mm_info->iova_addr);
    adlak_write32(region, REG_ADLAK_0X68, padlak->cmq_buf_info.cmq_mm_info->req.bytes);
    return 0;
}

int adlak_hal_set_axisram(void *data) {
    struct adlak_device *padlak = (struct adlak_device *)data;

    struct io_region * region = padlak->hw_res.preg;
    uint32_t           pa_start, va_start, va_end, wrap_en;
    HAL_ADLAK_AB_CTL_S d_ab;
    AML_LOG_DEBUG("%s", __func__);
    if (0 != padlak->hw_res.adlak_sram_size) {
        AML_LOG_INFO("AXI_SRAM_BASE = 0x%lx,size = 0x%lX", (uintptr_t)padlak->hw_res.adlak_sram_pa,
                     (uintptr_t)padlak->hw_res.adlak_sram_size);
        wrap_en  = padlak->hw_res.sram_wrap;
        pa_start = (uint32_t)((uintptr_t)padlak->hw_res.adlak_sram_pa);
        va_start = (uint32_t)((uintptr_t)padlak->hw_res.adlak_sram_pa);
        va_end   = (uint32_t)((uintptr_t)padlak->hw_res.adlak_sram_pa) +
                 (uint32_t)((uintptr_t)padlak->hw_res.adlak_sram_size);
        if (wrap_en) {
            va_end += va_end - va_start;
        }
        AML_LOG_INFO("va_start = 0x%lx,va_end = 0x%lX", (uintptr_t)va_start, (uintptr_t)va_end);
        adlak_write32(region, REG_ADLAK_0XA4, va_start / 4096);
        adlak_write32(region, REG_ADLAK_0XA8, va_end / 4096);
        adlak_write32(region, REG_ADLAK_0X9C, pa_start / 4096);
        d_ab.all                      = adlak_read32(region, REG_ADLAK_0XA0);
        d_ab.bitc.ab_axi_addr_wrap_en = wrap_en;
        adlak_write32(region, REG_ADLAK_0XA0, d_ab.all);
    }
    return 0;
}

static int adlak_hal_set_autoclock(void *data, uint32_t en) {
    struct adlak_device *padlak = (struct adlak_device *)data;
    struct io_region *   region = padlak->hw_res.preg;

    AML_LOG_DEBUG("%s", __func__);
    if (0 == en) {
        adlak_write32(region, REG_ADLAK_0X28, 0x0);
        adlak_write32(region, REG_ADLAK_0X120, 0x0);
    } else {
        adlak_write32(region, REG_ADLAK_0X28, 0x3FFF);
        // adlak_write32(region, REG_ADLAK_0X120, 0xF1);
        adlak_write32(region, REG_ADLAK_0X120, 0xFF);
        adlak_write32(region, REG_ADLAK_0X2C, 0x0608);
    }
    return 0;
}

int adlak_hal_reset(void *data) {
    int                  ret    = ERR(EINVAL);
    struct adlak_device *padlak = (struct adlak_device *)data;
    struct io_region *   region = NULL;
    AML_LOG_DEBUG("%s", __func__);

    region = padlak->hw_res.preg;

    ret = adlak_hal_soft_reset(data);
    if (ret) ret = adlak_hal_hard_reset(data);
    padlak->hw_stat.stat = ADLAK_HW_STAT_INITED;
    // adlak_smmu_reset(region);
    // adlak_parser_reset(region);
    padlak->cmq_buf_info.cmq_wr_offset = 0;
    padlak->cmq_buf_info.cmq_rd_offset = 0;
    //  adlak_pm_reset(padlak);
    adlak_hal_irq_clear(padlak, 0xFFFFFFFF);
    padlak->hw_stat.paser_stat = ADLAK_PS_STAT_RESETED;

    return ret;
}

int adlak_hal_start(void *data) {
    struct adlak_device *padlak = (struct adlak_device *)data;
    AML_LOG_DEBUG("%s", __func__);
    adlak_check_dev_is_idle(padlak);
    adlak_hal_set_autoclock(padlak, true);
    /*2.2. Set MMU enable */
    adlak_hal_set_mmu(padlak, padlak->smmu_en);
    adlak_hal_set_cmq(padlak);
    adlak_hal_set_axisram(padlak);
    adlak_parser_set_apb_timeout(padlak->hw_res.preg, 0xFF);

    adlak_parser_set_pend_timer(padlak->hw_res.preg, 0x1000000);

    adlak_hal_irq_enable(padlak, true);
    return 0;
}
int adlak_hal_stop(void *data) {
    struct adlak_device *padlak = (struct adlak_device *)data;
    AML_LOG_DEBUG("%s", __func__);

    /*2.2. Set MMU enable */
    adlak_hal_set_mmu(padlak, false);
    // adlak_hal_irq_clear(padlak,0xFF);
    adlak_hal_irq_enable(padlak, false);
    return 0;
}

int adlak_hal_reset_and_start(void *data) {
    int                   ret    = ERR(NONE);
    struct adlak_device * padlak = (struct adlak_device *)data;
    struct adlak_hw_info *phw_info;

    AML_LOG_DEBUG("%s", __func__);
    phw_info = padlak->hw_info;
    /*1. hardware reset */
    adlak_hal_reset(padlak);

    /*2. hardware start*/
    adlak_hal_start(padlak);
#if ADLAK_DEBUG
    g_adlak_log_level = g_adlak_log_level_pre;
#endif
    return ret;
}
int adlak_get_reg_name(int offset, char *buf, size_t buf_size) {
    switch (offset) {
        case REG_ADLAK_0X0:
            return adlak_os_snprintf(buf, buf_size, "%s", "REV");

        case REG_ADLAK_0X4:
            return adlak_os_snprintf(buf, buf_size, "%s", "WAIT_TIMER");
        // irq
        case REG_ADLAK_0X10:
            return adlak_os_snprintf(buf, buf_size, "%s", "IRQ_MASKED");
        case REG_ADLAK_0X14:
            return adlak_os_snprintf(buf, buf_size, "%s", "IRQ_MASK");
        case REG_ADLAK_0X18:
            return adlak_os_snprintf(buf, buf_size, "%s", "IRQ_RAW");
        case REG_ADLAK_0X1C:
            return adlak_os_snprintf(buf, buf_size, "%s", "STS_REPORT");
        // power&clock
        case REG_ADLAK_0X20:
            return adlak_os_snprintf(buf, buf_size, "%s", "SWRST");
        case REG_ADLAK_0X24:
            return adlak_os_snprintf(buf, buf_size, "%s", "ADLAK_EN");
        case REG_ADLAK_0X28:
            return adlak_os_snprintf(buf, buf_size, "%s", "CLK_AUTOCLK");
        case REG_ADLAK_0X2C:
            return adlak_os_snprintf(buf, buf_size, "%s", "CLK_IDLE_CNT");
        // debug
        case REG_ADLAK_0X30:
            return adlak_os_snprintf(buf, buf_size, "%s", "DBG_EN");
        case REG_ADLAK_0X34:
            return adlak_os_snprintf(buf, buf_size, "%s", "DBG_SEL");
        case REG_ADLAK_0X38:
            return adlak_os_snprintf(buf, buf_size, "%s", "DBG_SUB_SEL");
        case REG_ADLAK_0X3C:
            return adlak_os_snprintf(buf, buf_size, "%s", "DBG_DAT");
        case REG_ADLAK_0X40:
            return adlak_os_snprintf(buf, buf_size, "%s", "DBG_SRAM_CTRL");
        case REG_ADLAK_0X44:
            return adlak_os_snprintf(buf, buf_size, "%s", "DBG_SRAM_ADDR");
        case REG_ADLAK_0X48:
            return adlak_os_snprintf(buf, buf_size, "%s", "DBG_SRAM_WDAT");
        case REG_ADLAK_0X4C:
            return adlak_os_snprintf(buf, buf_size, "%s", "DBG_SRAM_RDAT");

        // parser
        case REG_ADLAK_0X50:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_CTRL");
        case REG_ADLAK_0X54:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_STS");
        case REG_ADLAK_0X58:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_ERR_DAT");
        case REG_ADLAK_0X5C:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_IDLE_STS");
        case REG_ADLAK_0X60:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_TIME_STAMP");
        case REG_ADLAK_0X64:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_RBF_BASE");
        case REG_ADLAK_0X68:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_RBF_SIZE");
        case REG_ADLAK_0X6C:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_RBF_WPT");
        case REG_ADLAK_0X70:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_RBF_RPT");
        case REG_ADLAK_0X74:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_RBF_PPT");
        case REG_ADLAK_0X78:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_FINISH_ID");
        case REG_ADLAK_0X7C:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_HCNT");
        case REG_ADLAK_0X80:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_OST");
        case REG_ADLAK_0X84:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_PEND_EN");
        case REG_ADLAK_0X88:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_PEND_VAL");
        case REG_ADLAK_0X8C:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_MODULE_IDLE_STS");
        case REG_ADLAK_0X90:
            return adlak_os_snprintf(buf, buf_size, "%s", "PS_DBG_SW_ID");
        case REG_ADLAK_0X9C:
            return adlak_os_snprintf(buf, buf_size, "%s", "AB_AXI_PADDR");
        case REG_ADLAK_0XA0:
            return adlak_os_snprintf(buf, buf_size, "%s", "AB_CTL");
        case REG_ADLAK_0XA4:
            return adlak_os_snprintf(buf, buf_size, "%s", "AB_AXI_SADDR");
        case REG_ADLAK_0XA8:
            return adlak_os_snprintf(buf, buf_size, "%s", "AB_AXI_EADDR");
        case REG_ADLAK_0XAC:
            return adlak_os_snprintf(buf, buf_size, "%s", "AB_R_CS_PRIO");
        case REG_ADLAK_0XB0:
            return adlak_os_snprintf(buf, buf_size, "%s", "AB_R_LS_PRIO");
        case REG_ADLAK_0XB4:
            return adlak_os_snprintf(buf, buf_size, "%s", "AB_R_L2_PRIO");
        case REG_ADLAK_0XB8:
            return adlak_os_snprintf(buf, buf_size, "%s", "AB_W_PRIO");
        case REG_ADLAK_0XBC:
            return adlak_os_snprintf(buf, buf_size, "%s", "AB_AXI_USER");
        // smmu
        case REG_ADLAK_0XC0:
            return adlak_os_snprintf(buf, buf_size, "%s", "SMMU_EN");
        case REG_ADLAK_0XC4:
            return adlak_os_snprintf(buf, buf_size, "%s", "SMMU_TTBR_L");
        case REG_ADLAK_0XC8:
            return adlak_os_snprintf(buf, buf_size, "%s", "SMMU_TTBR_H");
        case REG_ADLAK_0XCC:
            return adlak_os_snprintf(buf, buf_size, "%s", "SMMU_PRIO_POW2_0");
        case REG_ADLAK_0XD0:
            return adlak_os_snprintf(buf, buf_size, "%s", "SMMU_PRIO_POW2_1");
        case REG_ADLAK_0XD4:
            return adlak_os_snprintf(buf, buf_size, "%s", "SMMU_INV_CTL");
        case REG_ADLAK_0XD8:
            return adlak_os_snprintf(buf, buf_size, "%s", "SMMU_INV_VA");
        case REG_ADLAK_0XDC:
            return adlak_os_snprintf(buf, buf_size, "%s", "SMMU_DFT");
        case REG_ADLAK_0XE0:
            return adlak_os_snprintf(buf, buf_size, "%s", "SMMU_IVD_MDL");
        case REG_ADLAK_0XE4:
            return adlak_os_snprintf(buf, buf_size, "%s", "SMMU_IVD_VA");
        // pm
        case REG_ADLAK_0XF0:
            return adlak_os_snprintf(buf, buf_size, "%s", "PM_EN");
        case REG_ADLAK_0XF4:
            return adlak_os_snprintf(buf, buf_size, "%s", "PM_RBF_BASE");
        case REG_ADLAK_0XF8:
            return adlak_os_snprintf(buf, buf_size, "%s", "PM_RBF_SIZE");
        case REG_ADLAK_0XFC:
            return adlak_os_snprintf(buf, buf_size, "%s", "PM_RBF_WPT");
        case REG_ADLAK_0X100:
            return adlak_os_snprintf(buf, buf_size, "%s", "PM_RBF_RPT");
        case REG_ADLAK_0X104:
            return adlak_os_snprintf(buf, buf_size, "%s", "PM_STS");
        // AXI DRAM
        case REG_ADLAK_0X110:
            return adlak_os_snprintf(buf, buf_size, "%s", "AXIBRG_DX_CTL");
        case REG_ADLAK_0X114:
            return adlak_os_snprintf(buf, buf_size, "%s", "AXIBRG_DX_HOLD");
        // AXI SRAM
        case REG_ADLAK_0X118:
            return adlak_os_snprintf(buf, buf_size, "%s", "AXIBRG_SX_CTL");
        case REG_ADLAK_0X11C:
            return adlak_os_snprintf(buf, buf_size, "%s", "AXIBRG_SX_HOLD");

        case REG_ADLAK_0X120:
            return adlak_os_snprintf(buf, buf_size, "%s", "MC_CTL");
        default:
            return 0;
    }
}

static void adlak_reg_lst_init(struct adlak_hw_info *phw_info) {
    int idx = 0;

    phw_info->reg_lst[idx++] = REG_ADLAK_0X0;

    phw_info->reg_lst[idx++] = REG_ADLAK_0X4;
    // irq
    phw_info->reg_lst[idx++] = REG_ADLAK_0X10;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X14;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X18;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X1C;
    // power&clock
    phw_info->reg_lst[idx++] = REG_ADLAK_0X20;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X24;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X28;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X2C;
    // debug
    phw_info->reg_lst[idx++] = REG_ADLAK_0X30;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X34;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X38;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X3C;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X40;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X44;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X48;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X4C;

    // parser
    phw_info->reg_lst[idx++] = REG_ADLAK_0X50;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X54;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X58;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X5C;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X60;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X64;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X68;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X6C;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X70;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X74;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X78;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X7C;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X80;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X84;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X88;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X8C;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X90;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X9C;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XA0;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XA4;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XA8;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XAC;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XB0;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XB4;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XB8;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XBC;
    // smmu
    phw_info->reg_lst[idx++] = REG_ADLAK_0XC0;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XC4;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XC8;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XCC;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XD0;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XD4;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XD8;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XDC;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XE0;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XE4;
    // pm
    phw_info->reg_lst[idx++] = REG_ADLAK_0XF0;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XF4;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XF8;
    phw_info->reg_lst[idx++] = REG_ADLAK_0XFC;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X100;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X104;
    // AXI DRAM
    phw_info->reg_lst[idx++] = REG_ADLAK_0X110;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X114;
    // AXI SRAM
    phw_info->reg_lst[idx++] = REG_ADLAK_0X118;
    phw_info->reg_lst[idx++] = REG_ADLAK_0X11C;

    phw_info->reg_lst[idx++] = REG_ADLAK_0X120;
}
static void adlak_hw_caps_update(struct adlak_device *padlak, struct adlak_caps_desc *uapi_caps) {
    struct adlak_hw_info *phw_info = (struct adlak_hw_info *)padlak->hw_info;
    uapi_caps->hw_ver              = phw_info->rev.all;
    uapi_caps->core_freq_max       = padlak->clk_core_freq_set;
    uapi_caps->axi_freq_max        = padlak->clk_axi_freq_set;
    uapi_caps->cmq_size            = padlak->cmq_buf_info.size;
    uapi_caps->sram_base           = padlak->hw_res.adlak_sram_pa;
    uapi_caps->sram_size           = padlak->hw_res.adlak_sram_size;
}
int adlak_hw_init(void *data) {
    struct adlak_device * padlak = (struct adlak_device *)data;
    struct adlak_hw_info *phw_info;

    AML_LOG_DEBUG("%s", __func__);
    phw_info        = adlak_os_zalloc(sizeof(struct adlak_hw_info), ADLAK_GFP_KERNEL);
    padlak->hw_info = (void *)phw_info;
    adlak_reg_lst_init(padlak->hw_info);

    adlak_hal_enable(data, true);  // alda enable

    adlak_hal_get_revision((void *)padlak);

    phw_info->irq_cfg.mask_err = ADLAK_IRQ_MASK_PEND_TIMOUT | ADLAK_IRQ_MASK_APB_WAIT_TIMOUT |
                                 ADLAK_IRQ_MASK_PARSER_STOP_ERR | ADLAK_IRQ_MASK_INVALID_IOVA |
                                 ADLAK_IRQ_MASK_SW_TIMEOUT;

    phw_info->irq_cfg.mask_err =
        phw_info->irq_cfg.mask_err |
        (ADLAK_IRQ_MASK_PM_DRAM_OVF | ADLAK_IRQ_MASK_PM_FIFO_OVF | ADLAK_IRQ_MASK_PM_ARBITER_OVF);

    phw_info->irq_cfg.mask_normal = ADLAK_IRQ_MASK_TIM_STAMP;
#if ADLAK_DEBUG
    // phw_info->irq_cfg.mask_normal |= ADLAK_IRQ_MASK_LAYER_END;  // debug only
#endif
    phw_info->irq_cfg.mask =
        ((phw_info->irq_cfg.mask_err ^ ADLAK_IRQ_MASK_SW_TIMEOUT) | phw_info->irq_cfg.mask_normal);
    phw_info->smmu_entry = padlak->smmu_entry;
    phw_info->region     = padlak->hw_res.preg;

    padlak->dev_caps.data = &adlak_priv_caps.uapi_caps_data;
    padlak->dev_caps.size = sizeof(adlak_priv_caps.uapi_caps_data);
    adlak_hw_caps_update(padlak, &adlak_priv_caps.uapi_caps_data);
    return adlak_hal_reset_and_start(data);
}

void adlak_hw_deinit(void *data) {
    struct adlak_device *padlak = (struct adlak_device *)data;

    AML_LOG_DEBUG("%s", __func__);
    /*
    - hardware reset
    - deinit smmu tbl buffer
    */

    /*1. hardware stop*/
    adlak_hal_stop(padlak);
    /*2. hardware reset */
    adlak_hal_reset(padlak);

    adlak_hal_enable(data, false);  // alda disable

    if (padlak->hw_info) {
        adlak_os_free(padlak->hw_info);
    }
}
int adlak_hal_submit(void *data, uint32_t wpt) {
    int                  ret    = ERR(NONE);
    struct adlak_device *padlak = (struct adlak_device *)data;
    adlak_parser_cmq_set(padlak->hw_res.preg, wpt);
    adlak_parser_start(padlak->hw_res.preg);
    return ret;
}

void adlak_hw_dev_resume(void *data) {
    AML_LOG_DEBUG("%s", __func__);
    adlak_hal_enable(data, true);  // alda enable
    adlak_hal_reset_and_start(data);
}

void adlak_hw_dev_suspend(void *data) {
    AML_LOG_DEBUG("%s", __func__);
    // adlak_hal_reset(data);
}

void adlak_check_dev_is_idle(void *data) {
#if !(CONFIG_ADLAK_EMU_EN)
    struct adlak_device * padlak   = (struct adlak_device *)data;
    struct adlak_hw_info *phw_info = (struct adlak_hw_info *)padlak->hw_info;
    uint32_t              idel_sts;
    AML_LOG_DEBUG("%s", __func__);

    idel_sts = adlak_read32(phw_info->region, REG_ADLAK_0X8C);
    if (idel_sts != 0xFFFFFFFF) {
        AML_LOG_WARN("REG_ADLAK_0X8C   : 0x%08X", idel_sts);
    }
#endif
}
