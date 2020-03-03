/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EFUSE_H
#define __EFUSE_H

/* #define EFUSE_READ_ONLY */

#ifndef EFUSE_READ_ONLY
#define EFUSE_CLASS_ATTR CLASS_ATTR_RW
#else
#define EFUSE_CLASS_ATTR CLASS_ATTR_RO
#endif

struct aml_efuse_dev {
	struct platform_device *pdev;
	struct class           cls;
	struct cdev            cdev;
	dev_t                  devno;
};

struct aml_efuse_key {
	int                   num;
	struct efusekey_info *infos;
};

#define EFUSE_INFO_GET		_IO('f', 0x40)

#define EFUSE_HAL_API_READ	0
#define EFUSE_HAL_API_WRITE 1
#define EFUSE_HAL_API_USER_MAX 3

#define AML_DATA_PROCESS            (0x820000FF)
#define AML_D_P_W_EFUSE_AMLOGIC     (0x20)
#define EFUSE_PATTERN_SIZE      (0x400)

/* efuse HAL_API arg */
struct efuse_hal_api_arg {
	unsigned int cmd;
	unsigned int offset;
	unsigned int size;
	unsigned long buffer;
	unsigned long retcnt;
};

struct aml_efuse_cmd {
	unsigned int read_cmd;
	unsigned int write_cmd;
	unsigned int get_max_cmd;
	unsigned int mem_in_base_cmd;
	unsigned int mem_out_base_cmd;
};

extern struct aml_efuse_cmd efuse_cmd;

ssize_t efuse_get_max(void);
ssize_t efuse_read_usr(char *buf, size_t count, loff_t *ppos);
ssize_t efuse_write_usr(char *buf, size_t count, loff_t *ppos);
unsigned long efuse_amlogic_set(char *buf, size_t count);

#ifdef CONFIG_AMLOGIC_EFUSE
int __init aml_efuse_init(void);
void aml_efuse_exit(void);
#else
static int __init aml_efuse_init(void)
{
	return 0;
}

static void aml_efuse_exit(void)
{
}
#endif
#endif
