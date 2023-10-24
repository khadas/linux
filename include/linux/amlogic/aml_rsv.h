/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_RSV_H_
#define __MESON_RSV_H_

#define NAND_RSV_BLOCK_NUM 48
#define CONFIG_ENV_SIZE  (64 * 1024U)

#define NAND_GAP_BLOCK_NUM 4
#define NAND_BBT_BLOCK_NUM 4
#define NAND_ENV_BLOCK_NUM 8
#define NAND_KEY_BLOCK_NUM 8
#define NAND_DTB_BLOCK_NUM 4

#define BBT_NAND_MAGIC	"nbbt"
#define ENV_NAND_MAGIC	"nenv"
#define KEY_NAND_MAGIC	"nkey"
#define SEC_NAND_MAGIC	"nsec"
#define DTB_NAND_MAGIC	"ndtb"

#define NAND_BOOT_NAME	"bootloader"
#define NAND_NORMAL_NAME "nandnormal"
/*define abnormal state for reserved area*/
#define POWER_ABNORMAL_FLAG	0x01
#define ECC_ABNORMAL_FLAG	0x02

struct meson_rsv_info_t {
	struct mtd_info *mtd;
	struct valid_node_t *valid_node;
	struct free_node_t *free_node;
	unsigned int start_block;
	unsigned int end_block;
	unsigned int size;
	char name[8];
	u_char valid;
	u_char init;
	void *handler;
	int (*read)(u_char *dest, size_t size);
	int (*write)(u_char *src, size_t size);
};

struct valid_node_t {
	s16 ec;
	s16	phy_blk_addr;
	s16	phy_page_addr;
	int timestamp;
	s16 status;
};

struct free_node_t {
	unsigned int index;
	s16 ec;
	s16	phy_blk_addr;
	int dirty_flag;
	struct free_node_t *next;
};

struct oobinfo_t {
	char name[4];
	s16 ec;
	unsigned timestamp:15;
	unsigned status_page:1;
};

struct meson_rsv_ops {
	int (*_erase)(struct mtd_info *mtd, struct erase_info *einfo);
	int (*_write_oob)(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops);
	int (*_read_oob)(struct mtd_info *mtd, loff_t from, struct mtd_oob_ops *ops);
	int (*_block_markbad)(struct mtd_info *mtd, loff_t off);
	int (*_block_isbad)(struct mtd_info *mtd, loff_t off);
};

struct meson_rsv_handler_t {
	struct mtd_info *mtd;
	unsigned long long freeNodeBitmask;
	struct free_node_t *free_node[NAND_RSV_BLOCK_NUM];
	struct meson_rsv_info_t *bbt;
	struct meson_rsv_info_t *env;
	struct meson_rsv_info_t *key;
	struct meson_rsv_info_t	*dtb;
	struct meson_rsv_ops rsv_ops;
	void *priv;
};

int meson_rsv_key_read(u_char *dest, size_t size);
int meson_rsv_key_write(u_char *source, size_t size);
int meson_rsv_erase_protect(struct meson_rsv_handler_t *handler,
			    uint32_t block_addr);

#include<linux/cdev.h>
#define DTB_CDEV_NAME "dtb"
#define ENV_CDEV_NAME "nand_env"

struct meson_rsv_user_t {
	struct meson_rsv_info_t *info;
	dev_t devt;
	struct cdev cdev;
	struct device *dev;
	struct class *cls;
	/* in case crash */
	struct mutex lock;
};

int meson_rsv_register_cdev(struct meson_rsv_info_t *info, char *name);
int meson_rsv_register_unifykey(struct meson_rsv_info_t *key);
int meson_rsv_bbt_write(u_char *source, size_t size);
int meson_rsv_init(struct mtd_info *mtd, struct meson_rsv_handler_t *handler);
int meson_rsv_check(struct meson_rsv_info_t *rsv_info);
int meson_rsv_scan(struct meson_rsv_info_t *rsv_info);
int meson_rsv_read(struct meson_rsv_info_t *rsv_info, u_char *buf);
#endif/* __MESON_RSV_H_ */
