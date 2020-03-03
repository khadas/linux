/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __UNIFYKEY_MANAGE_H__
#define __UNIFYKEY_MANAGE_H__

#include <linux/cdev.h>
#include <linux/list.h>

#define KEYUNIFY_ATTACH		_IO('f', 0x60)
#define KEYUNIFY_GET_INFO	_IO('f', 0x62)

#define KEY_UNIFY_NAME_LEN	48

enum key_manager_dev_e {
	KEY_UNKNOWN_DEV = 0,
	KEY_EFUSE,
	KEY_NORMAL,/*nand/emmc key*/
	KEY_SECURE,
	KEY_MAX_DEV,
};

enum key_manager_permit_e {
	KEY_PERM_READ = BIT(0),
	KEY_PERM_WRITE = BIT(1),
};

/* for ioctrl transfer parameters. */
struct key_item_info_t {
	unsigned int id;
	char	     name[KEY_UNIFY_NAME_LEN];
	unsigned int size;
	unsigned int perm;
	unsigned int flag;	/*bit 0: 1 exsit, 0-none;*/
	unsigned int reserve;
};

#define KEY_ATTR_ENCRYPT		BIT(8)
#define KEY_ATTR_SECURE		BIT(0)

struct key_item_t {
	struct list_head node;
	char name[KEY_UNIFY_NAME_LEN];
	int id;
	unsigned int dev;
	unsigned int perm;
	int attr;
	int reserve;
};

struct key_info_t {
	int key_num;
	int key_flag;
	int encrypt_type;
};

struct aml_uk_dev {
	struct platform_device *pdev;
	struct class           cls;
	struct cdev            cdev;
	dev_t                  uk_devno;
	int		       init_flag;
	int                    lock_flag;
	struct key_item_t      *curkey;
	struct list_head       uk_hdr;
	struct key_info_t      uk_info;
};

int uk_dt_create(struct platform_device *pdev);
int uk_dt_release(struct platform_device *pdev);

#ifdef CONFIG_AMLOGIC_UNIFYKEY
int __init aml_unifykeys_init(void);
void __exit aml_unifykeys_exit(void);
#else
static int __init aml_unifykeys_init(void)
{
	return 0;
}

static void aml_unifykeys_exit(void)
{
}
#endif
#endif
