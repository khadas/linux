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
#include "meson_mhu.h"
#include "meson_mhu_pl.h"
#include "meson_mhu_fifo.h"
#include "meson_mhu_sec.h"
#include "meson_mhu_user.h"

static int __init mailbox_init(void)
{
	aml_mhu_init();
	aml_mhu_pl_init();
	aml_mhu_fifo_init();
	aml_mhu_sec_init();
	aml_mhu_user_init();

	return 0;
}

static void __exit mailbox_exit(void)
{
	aml_mhu_exit();
	aml_mhu_pl_exit();
	aml_mhu_fifo_exit();
	aml_mhu_sec_exit();
	aml_mhu_user_exit();
}

module_init(mailbox_init);
module_exit(mailbox_exit);

MODULE_DESCRIPTION("Amlogic MHU driver");
MODULE_LICENSE("GPL v2");

