/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_hw.h
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

#ifndef __ADLAK_HW_H__
#define __ADLAK_HW_H__

/***************************** Include Files *********************************/
#include "adlak_config.h"
#include "adlak_platform_config.h"
#include "adlak_reg.h"
#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/
#ifndef ADLAK_HW_DEBUG_EN
#define ADLAK_HW_DEBUG_EN (0)
#endif
#if (!(ADLAK_DEBUG))
#undef ADLAK_HW_DEBUG_EN
#define ADLAK_HW_DEBUG_EN (0)
#endif

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

struct adlak_hw_info {
    union {
        struct _revision {
            uint8_t minor;
            uint8_t major;
        } bitc;
        uint32_t all;
    } rev;
    struct _irq_cfg {
        uint32_t mask;
        uint32_t mask_err;
        uint32_t mask_normal;
    } irq_cfg;

    uint64_t          smmu_entry;
    int               reg_lst[REG_ADLAK_NUM_MAX];
    struct io_region *region;
};

struct adlak_irq_status {
    uint32_t irq_masked;
    uint32_t irq_raw;
    uint32_t time_stamp;
    uint32_t timeout;
    uint32_t status_report;
};

struct adlak_hw_stat {
    HAL_ADLAK_CLK_IDLE_CNT_S clk_cnt;

    uint32_t                ps_finish_id;
    uint32_t                ps_status;
    uint32_t                ps_err_dat;
    uint32_t                ps_idle_status;
    uint32_t                ps_rbf_base;
    uint32_t                ps_rbf_size;
    uint32_t                ps_rbf_wpt;
    uint32_t                ps_rbf_rpt;
    uint32_t                ps_rbf_ppt;
    uint32_t                smmu_err_dft_pa;
    uint32_t                smmu_err_mdl_id;
    uint32_t                smmu_err_iova;
    uint32_t                regs_stat[REG_ADLAK_NUM_MAX];
    struct adlak_irq_status irq_status;
    struct adlak_hw_info *  hw_info;
};

/************************** Function Prototypes ******************************/

typedef volatile unsigned long __IO;

/**
 * struct io_region - a general struct describe IO region
 *
 * @phys: physical address base of an IO region
 * @kern: kernel virtual address base remapped from phys
 * @size: size of of an IO region in byte
 */
struct io_region {
    uint64_t pa_kernel;
    uint32_t size;
    void *   va_kernel;
};

#define ADLAK_BIT(data, n) (((data) >> (n)) & 0x1)

/**
 * @brief create ADLAK IO region using physical base address
 *
 * @param dev: device pointer
 * @param phys_base: base address
 * @param size: region size
 *
 * @return io_region pointer if successful; NULL if failed;
 */
struct io_region *adlak_create_ioregion(uintptr_t phys_base, uint32_t size);
/**
 * @brief destroy an ADLAK IO region
 *
 * @param region: region pointer created by adlak_create_ioregion
 */
void adlak_destroy_ioregion(struct io_region *region);

int      adlak_hw_init(void *data);
void     adlak_hw_deinit(void *data);
uint32_t adlak_get_hw_status(struct adlak_hw_stat *phw_stat);

struct adlak_irq_status *adlak_hal_get_irq_status(struct adlak_hw_stat *phw_stat);

int      adlak_save_cur_hw_info(void *data, void *phw_info_dest);
uint32_t adlak_hal_get_ps_rpt(void *data);

int adlak_hal_submit(void *data, uint32_t wpt);

void adlak_hal_smmu_cache_invalid(void *data, dma_addr_t iova);
int  adlak_hal_reset_and_start(void *data);
void adlak_hal_irq_clear(void *data, uint32_t clr_bits);
void adlak_status_dump(struct adlak_hw_stat *phw_stat);

int adlak_get_reg_name(int offset, char *buf, size_t buf_size);

void adlak_pm_reset(void *padlak);

void     adlak_pm_config(void *padlak, uint32_t addr, uint32_t buf_size, uint32_t wpt);
uint32_t adlak_pm_get_stat(void *padlak);
int      adlak_pm_fush_until_empty(void *padlak);
void     adlak_pm_enable(void *padlak, uint32_t en);
void     adlak_hal_irq_enable(void *padlak, uint32_t en);

void adlak_hw_dev_resume(void *data);
void adlak_hw_dev_suspend(void *data);

void adlak_check_dev_is_idle(void *data);
#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_HW_H__ end define*/
