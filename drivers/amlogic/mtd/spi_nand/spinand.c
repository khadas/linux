// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <asm/div64.h>
#include <linux/sizes.h>
#include <linux/of.h>
#include <linux/version.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/spinand.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_rsv.h>
#include <linux/amlogic/aml_spi_nand.h>
#include <linux/spi/spi-mem.h>

#define NAND_BLOCK_GOOD	0
#define NAND_BLOCK_BAD	1
#define NAND_FACTORY_BAD	2

#define NAND_FIPMODE_DISCRETE   (1)
//#define CONFIG_NOT_SKIP_BAD_BLOCK

struct meson_spinand {
	struct mtd_info *mtd;
	struct spinand_device *spinand;
	struct meson_rsv_handler_t *rsv;
	s8 *block_status;
	unsigned int erasesize_shift;
};

struct meson_spinand *meson_spinand_global;

bool meson_spinand_isbad(struct nand_device *nand, const struct nand_pos *pos)
{
	struct meson_spinand *meson_spinand = meson_spinand_global;
	u8 block_status;

	BUG_ON(!meson_spinand->block_status);

	block_status = meson_spinand->block_status[pos->eraseblock];
	if (block_status == NAND_BLOCK_BAD ||
	    block_status == NAND_FACTORY_BAD) {
		pr_err("NAND bbt detect%sBad block at %llx\n",
		       (block_status == NAND_FACTORY_BAD) ? " factory " : " ",
		       (u64)nanddev_pos_to_offs(nand, pos));
		return true;
	}

	return false;
}
EXPORT_SYMBOL_GPL(meson_spinand_isbad);

static int spinand_mtd_block_isbad(struct mtd_info *mtd, loff_t offs)
{
	struct meson_spinand *meson_spinand = meson_spinand_global;
	struct spinand_device *spinand = meson_spinand->spinand;
	struct nand_device *nand = mtd_to_nanddev(mtd);
	struct nand_pos pos;
	u8 block_status;

	nanddev_offs_to_pos(nand, offs, &pos);
	mutex_lock(&spinand->lock);
	block_status = meson_spinand_isbad(nand, &pos);
	mutex_unlock(&spinand->lock);

	return block_status;
}

static int spinand_mtd_block_markbad(struct mtd_info *mtd, loff_t offs)
{
	struct meson_spinand *meson_spinand = meson_spinand_global;
	struct spinand_device *spinand = meson_spinand->spinand;
	struct nand_device *nand = mtd_to_nanddev(mtd);
	struct nand_pos pos;
	u8 bad_block;
	s8 *buf = NULL;

	nanddev_offs_to_pos(nand, offs, &pos);
	mutex_lock(&spinand->lock);
	if (meson_spinand->block_status) {
		/* TODO: Keep one plane */
		bad_block = meson_spinand->block_status[pos.eraseblock];
		if (bad_block != NAND_BLOCK_BAD &&
		    bad_block != NAND_FACTORY_BAD &&
		    bad_block != NAND_BLOCK_GOOD) {
			pr_err("bad block table is mixed\n");
			mutex_unlock(&spinand->lock);
			return -EINVAL;
		}
		if (bad_block == NAND_BLOCK_GOOD) {
			buf = meson_spinand->block_status;
			buf[pos.eraseblock] = NAND_BLOCK_BAD;
			meson_rsv_bbt_write((u_char *)buf,
					    meson_spinand->rsv->bbt->size);
		}
		mutex_unlock(&spinand->lock);
		return 0;
	}
	pr_info("bbt table is not initial");
	mutex_unlock(&spinand->lock);
	return -EINVAL;
}

int meson_spinand_bbt_check(struct mtd_info *mtd)
{
	struct meson_spinand *meson_spinand = meson_spinand_global;
	int ret;

	ret = meson_rsv_scan(meson_spinand->rsv->bbt);
	if (ret != 0 && (ret != (-1)))
		return ret;

	if (meson_spinand->rsv->bbt->valid == 1) {
		pr_info("%s %d bbt is valid, reading.\n", __func__, __LINE__);
		meson_rsv_read(meson_spinand->rsv->bbt,
			       (u_char *)meson_spinand->block_status);
	}
	return 0;
}

int meson_spinand_init(struct spinand_device *spinand, struct mtd_info *mtd)
{
	struct meson_spinand *meson_spinand = NULL;
	int err = 0;

	meson_spinand = kzalloc(sizeof(*meson_spinand), GFP_KERNEL);
	if (!meson_spinand)
		return -ENOMEM;

	meson_spinand_global = meson_spinand;
	meson_spinand->erasesize_shift = ffs(mtd->erasesize) - 1;
	meson_spinand->block_status =
		kzalloc((mtd->size >> meson_spinand->erasesize_shift),
			GFP_KERNEL);
	if (!meson_spinand->block_status) {
		err = -ENOMEM;
		goto exit_error2;
	}

	meson_spinand->mtd = mtd;
	meson_spinand->spinand = spinand;

	meson_spinand->rsv = kzalloc(sizeof(*meson_spinand->rsv),
				     GFP_KERNEL);
	if (!meson_spinand->rsv) {
		err = -ENOMEM;
		goto exit_error1;
	}

	spi_mem_set_mtd(mtd);

	mtd->_block_isbad = spinand_mtd_block_isbad;
	mtd->_block_markbad = spinand_mtd_block_markbad;
	mtd->_block_isreserved = NULL;
	mtd->erasesize_shift = meson_spinand->erasesize_shift;
	mtd->writesize_shift = ffs(mtd->writesize) - 1;

	meson_rsv_init(mtd, meson_spinand->rsv);
	meson_spinand->rsv->rsv_ops._erase = mtd->_erase;
	meson_spinand->rsv->rsv_ops._write_oob = mtd->_write_oob;
	meson_spinand->rsv->rsv_ops._read_oob = mtd->_read_oob;
	meson_spinand->rsv->rsv_ops._block_markbad = NULL;
	meson_spinand->rsv->rsv_ops._block_isbad = mtd->_block_isbad;
	err = meson_spinand_bbt_check(mtd);
	if (err) {
		pr_err("Couldn't search or uncorrected bad block table\n");
		err = -ENODEV;
		goto exit_error;
	}
#ifndef CONFIG_MTD_ENV_IN_NAND
	meson_rsv_check(meson_spinand->rsv->env);
#endif
	meson_rsv_check(meson_spinand->rsv->key);
	meson_rsv_check(meson_spinand->rsv->dtb);

	return 0;
exit_error:
	kfree(meson_spinand->rsv);
exit_error1:
	kfree(meson_spinand->block_status);
exit_error2:
	kfree(meson_spinand);
	return err;
}
EXPORT_SYMBOL_GPL(meson_spinand_init);

bool spinand_is_info_page(struct nand_device *nand, int page)
{
	return unlikely((page % 128) == (SPI_NAND_BL2_PAGES - 1) &&
			page < SPI_NAND_BOOT_TOTAL_PAGES);
}
EXPORT_SYMBOL_GPL(spinand_is_info_page);

int spinand_set_info_page(struct mtd_info *mtd, void *buf)
{
	struct meson_spinand *spinand = meson_spinand_global;
	u32 page_per_blk;
	struct mtd_oob_region region;
	struct spinand_info_page *info_page = (struct spinand_info_page *)buf;

	page_per_blk = mtd->erasesize / mtd->writesize;
	memcpy(info_page->magic, SPINAND_MAGIC, strlen(SPINAND_MAGIC));
	info_page->version = SPINAND_INFO_VER;
	/* DISCRETE only */
	info_page->mode = 1;
	info_page->bl2_num = SPI_NAND_BL2_COPY_NUM;
	info_page->fip_num = SPI_NAND_TPL_COPY_NUM;
	info_page->dev.s.rd_max = SPI_NAND_NBITS;
	info_page->dev.s.fip_start =
		SPI_NAND_BOOT_TOTAL_PAGES + NAND_RSV_BLOCK_NUM * page_per_blk;
	info_page->dev.s.fip_pages = SPI_NAND_TPL_SIZE_PER_COPY / mtd->writesize;
	info_page->dev.s.page_size = mtd->writesize;
	info_page->dev.s.page_per_blk = page_per_blk;
	info_page->dev.s.oob_size = mtd->oobsize;
	mtd->ooblayout->free(mtd, 0, &region);
	info_page->dev.s.oob_offset = region.offset;
	info_page->dev.s.bbt_start = 0;
	info_page->dev.s.bbt_valid = 0;
	info_page->dev.s.bbt_size = spinand->rsv->bbt->size;

	return 0;
}
EXPORT_SYMBOL_GPL(spinand_set_info_page);

struct meson_partition_platform_data {
	u32 reserved_part_blk_num;
	u32 bl_mode;
	u32 fip_copies;
	u32 fip_size;
	u32 part_num;
	struct mtd_partition part[0];
};

static struct meson_partition_platform_data *
	meson_partition_parse_platform_data(struct device_node *np)
{
	struct meson_partition_platform_data *pdata = NULL;
	struct device_node *part_np, *child;
	struct mtd_partition *part;
	phandle phandles;
	int part_num, ret;

	if (!np)
		return NULL;

	ret = of_property_read_u32(np, "partition", (u32 *)&phandles);
	if (ret) {
		pr_info("%s: no partition in dts\n", __func__);
		return NULL;
	}

	part_np = of_find_node_by_phandle(phandles);
	if (!part_np) {
		pr_info("%s: partition handle error\n", __func__);
		return NULL;
	}

	child = of_get_next_child(part_np, NULL);
	part_num = of_get_child_count(part_np);
	if (!child || !part_num) {
		pr_info("%s: no partition table in dts\n", __func__);
		return NULL;
	}

	pdata = kzalloc(sizeof(*pdata) + sizeof(*part) * part_num,
			GFP_KERNEL);
	pdata->part_num = part_num;

	ret = of_property_read_u32(np, "bl_mode", &pdata->bl_mode);
	pr_info("bl_mode %s\n", pdata->bl_mode ? "discrete" : "compact");

	ret = of_property_read_u32(np, "fip_copies", &pdata->fip_copies);
	pr_info("fip_copies %d\n", pdata->fip_copies);

	ret = of_property_read_u32(np, "fip_size", &pdata->fip_size);
	pr_info("fip_size 0x%x\n", pdata->fip_size);

	part = pdata->part;
	for_each_child_of_node(part_np, child) {
		part->name = (char *)child->name;
		if (of_property_read_u64(child, "offset", &part->offset))
			goto parse_err;
		if (of_property_read_u64(child, "size", &part->size))
			goto parse_err;
		pr_info("part->offset 0x%llx pdata->fip_size = 0x%llx\n",
			part->offset, part->size);
		part++;
	}

	return pdata;

parse_err:
	kfree(pdata);
	return NULL;
}

static void meson_partition_relocate(struct mtd_info *mtd,
				     struct mtd_partition *part)
{
#ifndef CONFIG_NOT_SKIP_BAD_BLOCK
	struct meson_spinand *meson_spinand = meson_spinand_global;
	struct nand_device *nand = mtd_to_nanddev(mtd);
	struct nand_pos pos;
	loff_t offset = part->offset;
	loff_t end = offset + part->size;

	BUG_ON(!meson_spinand->block_status);
	while (offset < end) {
		nanddev_offs_to_pos(nand, offset, &pos);
		if (meson_spinand->block_status[pos.eraseblock] == NAND_FACTORY_BAD) {
			pr_err("add partition detect FBB at %llx\n", (u64)offset);
			part->size += mtd->erasesize;
			end += mtd->erasesize;
			if (end > mtd->size)
				break;
		}
		offset += mtd->erasesize;
	}
#endif
}

int meson_add_mtd_partitions(struct mtd_info *mtd)
{
	struct meson_partition_platform_data *pdata;
	struct mtd_partition *part;
	loff_t offset;
	int i;

	pdata = meson_partition_parse_platform_data(mtd_get_of_node(mtd));
	if (!pdata) {
		pr_err("%s: parse platform data failed\n", __func__);
		return -ENODEV;
	}

	/* bootloader */
	part = pdata->part;
	offset = 0;
	part->offset = offset;
	part->size = SPI_NAND_BOOT_TOTAL_PAGES * mtd->writesize;
	offset += part->size;

	/* skip rsv */
	offset += NAND_RSV_BLOCK_NUM * (loff_t)mtd->erasesize;

	/* tpl, support NAND_FIPMODE_DISCRETE only */
	//part++;
	//part->offset = offset;
	//part->size = pdata->fip_copies * pdata->fip_size;
	//offset += part->size;

	i = pdata->part_num - 2;
	while (i--) {
		part++;
		part->offset = offset;
		meson_partition_relocate(mtd, part);
		offset += part->size;
		if (offset > mtd->size)
			goto meson_add_mtd_partitions_err;
	}

	/* data */
	part++;
	part->offset = offset;
	part->size = mtd->size - offset;

	return mtd_device_register(mtd, pdata->part, pdata->part_num);

meson_add_mtd_partitions_err:
	pr_err("%s: add partition failed\n", __func__);
	kfree(pdata);
	return mtd_device_register(mtd, NULL, 0);
}
EXPORT_SYMBOL_GPL(meson_add_mtd_partitions);

MODULE_DESCRIPTION("MESON SPI NAND INTERFACE");
MODULE_AUTHOR("sunny luo<sunny.luo@amlogic.com>");
MODULE_LICENSE("GPL v2");
