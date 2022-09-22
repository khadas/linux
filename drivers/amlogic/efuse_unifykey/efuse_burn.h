/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EFUSE_BURN_H
#define __EFUSE_BURN_H

#define EFUSE_BURN_BURN_KEY		    _IO('f', 0x60)
#define EFUSE_BURN_CHECK_KEY		_IO('f', 0x62)

#define IOCTL_TYPE_UNKNOWN       0
#define IOCTL_TYPE_BURN          1
#define IOCTL_TYPE_CHECK         2

struct efuse_burn_info {
	char itemname[32];
	int  status;  //1:written, 0:not write, -1:fail.
};

struct aml_efuse_burn_dev {
	struct platform_device *pdev;
	struct class           cls;
	struct cdev            cdev;
	dev_t                  devno;
	int efuse_pattern_size;
};

#ifdef CONFIG_AMLOGIC_EFUSE_BURN
int __init aml_efuse_burn_init(void);
void aml_efuse_burn_exit(void);
#else
int __init aml_efuse_burn_init(void)
{
	return 0;
}

void aml_efuse_burn_exit(void)
{
}
#endif
#endif
