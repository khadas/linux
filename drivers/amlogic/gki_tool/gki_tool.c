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

#include "gki_tool.h"

int gki_tool_debug;
module_param_named(debug, gki_tool_debug, int, 0644);

static int __init gki_tool_init(void)
{
	gki_module_init();
	return 0;
}

static void __exit gki_tool_exit(void)
{
}

module_init(gki_tool_init);
module_exit(gki_tool_exit);

MODULE_DESCRIPTION("Amlogic GKI tool");
MODULE_LICENSE("GPL v2");
