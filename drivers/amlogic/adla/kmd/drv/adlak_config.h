/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_config.h
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

#ifndef __ADLAK_CONFIG_H__
#define __ADLAK_CONFIG_H__

/***************************** Include Files *********************************/
#include "adlak_version.h"

#ifdef __cplusplus
extern "C" {
#endif

/************************** Constant Definitions *****************************/

#define DEVICE_NAME "adla"
#define CLASS_NAME "adla"
/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/
#if defined(CONFIG_ADLAK_DEBUG) && (CONFIG_ADLAK_DEBUG == 1)
#define ADLAK_DEBUG (1)
#ifndef ADLAK_LOG_LEVEL
#define ADLAK_LOG_LEVEL (4)
#endif
#else
#define ADLAK_DEBUG (0)
#ifndef ADLAK_LOG_LEVEL
#define ADLAK_LOG_LEVEL (2)
#endif
#endif

#ifndef CONFIG_ADLAK_SMMU_ENABLE
#define CONFIG_ADLAK_SMMU_ENABLE (1)
#endif

#ifndef CONFIG_ADLAK_EMU_EN
#define CONFIG_ADLAK_EMU_EN (0)  // this macro just for develop test.
#endif

#ifndef CONFIG_HAS_PM_DOMAIN
#define CONFIG_HAS_PM_DOMAIN (1)
#endif

#ifndef CONFIG_ADLAK_PROFILER_ENABLE
#define CONFIG_ADLAK_PROFILER_ENABLE (0)
#endif

#ifndef CONFIG_ADLAK_ENABLE_SYSFS
#define CONFIG_ADLAK_ENABLE_SYSFS (1)
#endif

#ifndef CONFIG_ADLAK_MEM_POOL_EN
#define CONFIG_ADLAK_MEM_POOL_EN (1)
#endif

#if (CONFIG_ADLAK_MEM_POOL_EN == 0) && defined(CONFIG_ADLA_FREERTOS)
#error "only support pool memory in freertos.Please set CONFIG_ADLAK_MEM_POOL_EN=1"
#endif

// adlak dynamic freq and voltage configure
#ifndef CONFIG_ADLAK_DPM_EN
#define CONFIG_ADLAK_DPM_EN (1)
#endif

/************************** Function Prototypes ******************************/

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_CONFIG_H__ end define*/
