/*******************************************************************************
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 ******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file adlak_debugfs.h
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

#ifndef __ADLAK_DEBUGFS_H__
#define __ADLAK_DEBUGFS_H__

/***************************** Include Files *********************************/
#include "adlak_common.h"

#ifdef __cplusplus
extern "C" {
#endif
struct class;
/************************** Constant Definitions *****************************/
#define MAX_CHAR_SYSFS 4096

/**************************Global Variable************************************/

/**************************Type Definition and Structure**********************/

/************************** Function Prototypes ******************************/

/*
 * @brief initialize sysfs debug interfaces in probe
 *
 * @param adlak_device: adlak_device struct pointer
 *
 * @return 0 if successful; others if failed.
 */
int adlak_create_sysfs(void *adlak_device);
/*
 * @brief de-initialize sysfs debug interfaces in remove
 *
 * @param adlak_device: adlak_device struct pointer
 */
void adlak_destroy_sysfs(void *adlak_device);

int adlak_create_class_file(struct class *);

void adlak_destroy_class_file(struct class *);

#ifdef __cplusplus
}
#endif

#endif /* __ADLAK_DEBUGFS_H__ end define*/
