/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_platform_config.h
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

#ifndef __ADLAK_PLATFORM_CONFIG_H__
#define __ADLAK_PLATFORM_CONFIG_H__

/***************************** Include Files *********************************/
#include "adlak_typedef.h"
#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/
#ifndef CONFIG_OF
#define ADLAK_IRQ_LINE (0xA0)
#define ADLAK_REG_PHY_ADDR (0xe0004000UL)
#define ADLAK_REG_SIZE (0x00001000UL)
#define ADLAK_MEM_PHY_ADDR (0x80000000UL)
#define ADLAK_MEM_SIZE (60 * 1024 * 1024UL)  // 60MByte
#define ADLAK_SRAM_PHY_ADDR (0x80000000UL)
#define ADLAK_SRAM_SIZE (256 * 1024UL)
#define ADLAK_HAS_SMMU (0)
#define ADLAK_AXI_FREQ (800000000UL)   // unit Hz
#define ADLAK_CORE_FREQ (800000000UL)  // unit Hz
#define ADLAK_DPM_PERIOD (300)         // unit ms
#define ADLAK_TIMEOUT_MAX_MS (10)      // unit ms
#define ADLAK_CMD_QUEUE_SIZE (128 * 1024UL)

#endif
#define ADLAK_SMMU_MAX_ADDR_BITS 49

#define ADLAK_MAX_DEVICES (4U)

#define ADLAK_TASK_COUNT_MAX (100)  // TODO

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

/************************** Function Prototypes ******************************/
int adlak_platform_device_init(void);
int adlak_platform_device_uninit(void);
int adlak_platform_get_resource(void *data);
int adlak_platform_get_rsv_mem_size(void *dev, uint64_t *mem_size);

int adlak_platform_request_resource(void *data);

int adlak_platform_free_resource(void *data);

void adlak_platform_set_clock(void *data, bool enable, int core_freq, int axi_freq);
int  adlak_platform_pm_init(void *data);
void adlak_platform_pm_deinit(void *data);

void adlak_platform_set_power(void *data, bool enable);

void adlak_platform_resume(void *data);

void adlak_platform_suspend(void *data);
#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_PLATFORM_CONFIG_H__ end define*/
