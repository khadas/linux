// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/ctype.h>
#include <linux/kallsyms.h>
#include "efuse.h"
#include "unifykey.h"
#include "efuse_burn.h"

static int __init efuse_unifykey_init(void)
{
	int ret;

	ret = aml_efuse_init();
	if (ret)
		return ret;

	ret = aml_unifykeys_init();
	if (ret) {
		aml_efuse_exit();
		return ret;
	}

	ret = aml_efuse_burn_init();
	if (ret) {
		aml_efuse_burn_exit();
		return ret;
	}

	return 0;
}

static void __exit efuse_unifykey_exit(void)
{
	aml_efuse_exit();
	aml_unifykeys_exit();
	aml_efuse_burn_exit();
}

module_init(efuse_unifykey_init);
module_exit(efuse_unifykey_exit);

MODULE_DESCRIPTION("Amlogic efuse/unifykey management driver");
MODULE_LICENSE("GPL v2");

