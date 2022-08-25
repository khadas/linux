// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
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
#include "hwspinlock_coherent_new.h"
#include "hwspinlock.h"

static int __init hwspinlock_init(void)
{
	aml_hwspinlock_init();
	aml_bak_hwspinlock_init();
	return 0;
}

static void __exit hwspinlock_exit(void)
{
	aml_hwspinlock_exit();
	aml_bak_hwspinlock_exit();
}

module_init(hwspinlock_init);
module_exit(hwspinlock_exit);

MODULE_DESCRIPTION("Amlogic hwspinlock driver");
MODULE_LICENSE("GPL v2");

