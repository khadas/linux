// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifdef MODULE

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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "gki_tool.h"

/*
 * single_open() default buffer size is 4KB, but gki_config size is
 * probably bigger than 4KB, no need to try malloc buffers for times.
 */
#define GKI_CONFIG_BUFSIZE 8192

asm (
"       .pushsection .rodata, \"a\"             \n"
"       .global gki_config_data                 \n"
"gki_config_data:                               \n"
"       .incbin \"drivers/amlogic/gki_tool/.gki_config\" \n"
"	.word 0x00000000                        \n"
"       .global gki_config_data_end             \n"
"gki_config_data_end:                           \n"
"       .popsection                             \n"
);

extern char gki_config_data[];
extern char gki_config_data_end[];

static int gki_config_show(struct seq_file *m, void *arg)
{
	seq_printf(m, "%s\n", gki_config_data);

	return 0;
}

static int gki_config_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, gki_config_show, NULL, GKI_CONFIG_BUFSIZE);
}

static const struct file_operations gki_config_ops = {
	.open		= gki_config_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void gki_config_init(void)
{
	struct proc_dir_entry *dentry;
	//pr_info("gki_config_init: start=%px end=%px\n", gki_config_data, gki_config_data_end);

	dentry = proc_create("gki_config", 0644, NULL, &gki_config_ops);
	if (IS_ERR_OR_NULL(dentry)) {
		pr_err("%s, create sysfs failed\n", __func__);
		return;
	}
}
#endif
