// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/err.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/slab.h>
#include <linux/mmc/emmc_partitions.h>
#include <linux/amlogic/key_manage.h>
#include "emmc_key.h"
#include "../../mmc/core/core.h"

#define		EMMC_BLOCK_SIZE		(0x100)
#define		MAX_EMMC_BLOCK_SIZE	(128 * 1024)

/*
 * kernel head file
 *
 */
static struct mmc_card *mmc_card_key;
static struct aml_emmckey_info_t *emmckey_info;

//int32_t emmc_key_read(uint8_t *buffer,
s32 emmc_key_read(u8 *buffer, u32 length, u32 *actual_length)
{
	int ret;
	u64  addr = 0;
	u32  size = 0;
	int blk, cnt;
	unsigned char *dst = NULL;
	struct mmc_card *card = mmc_card_key;
	int bit = card->csd.read_blkbits;

	size = length;
	*actual_length = length;
	addr = get_reserve_partition_off_from_tbl() + EMMCKEY_RESERVE_OFFSET;
	blk = addr >> bit;
	cnt = size >> bit;
	dst = (unsigned char *)buffer;
	mmc_claim_host(card->host);
	do {
		ret = mmc_read_internal(card, blk,
					min(EMMC_BLOCK_SIZE, cnt), dst);
		if (ret) {
			pr_err("%s [%d] mmc_write_internal error\n",
			       __func__, __LINE__);
			return ret;
		}
		blk += EMMC_BLOCK_SIZE;
		cnt -= EMMC_BLOCK_SIZE;
		dst = (unsigned char *)buffer + MAX_EMMC_BLOCK_SIZE;
	} while (cnt > 0);

	pr_info("%s:%d, read %s\n", __func__, __LINE__, (ret) ? "error" : "ok");

	mmc_release_host(card->host);
	return ret;
}
EXPORT_SYMBOL(emmc_key_read);

//int32_t emmc_key_write(uint8_t *buffer,
s32 emmc_key_write(u8 *buffer, u32 length, u32 *actual_length)
{
	int ret;
	u64  addr = 0;
	u32  size = 0;
	int blk, cnt;
	unsigned char *src = NULL;
	struct mmc_card *card = mmc_card_key;
	int bit = card->csd.read_blkbits;

	size = length;
	addr = get_reserve_partition_off_from_tbl() + EMMCKEY_RESERVE_OFFSET;
	blk = addr >> bit;
	cnt = size >> bit;
	src = (unsigned char *)buffer;
	mmc_claim_host(card->host);
	do {
		ret = mmc_write_internal(card, blk,
					 min(EMMC_BLOCK_SIZE, cnt), src);
		if (ret) {
			pr_err("%s [%d] mmc_write_internal error\n",
			       __func__, __LINE__);
			return ret;
		}
		blk += EMMC_BLOCK_SIZE;
		cnt -= EMMC_BLOCK_SIZE;
		src = (unsigned char *)buffer + MAX_EMMC_BLOCK_SIZE;
	} while (cnt > 0);
	pr_info("%s:%d, write %s\n", __func__, __LINE__,
		(ret) ? "error" : "ok");
	mmc_release_host(card->host);
	return ret;
}
EXPORT_SYMBOL(emmc_key_write);

int emmc_key_init(struct mmc_card *card)
{
	u64  addr = 0;
	u32  size = 0;
	u64  lba_start = 0, lba_end = 0;
	int err = 0;
	int bit = card->csd.read_blkbits;
	struct unifykey_type *uk_type = NULL;
	struct unifykey_storage_ops ops;

	pr_info("card key: card_blk_probe.\n");
	emmckey_info = kmalloc(sizeof(*emmckey_info), GFP_KERNEL);
	if (!emmckey_info) {
		pr_info("%s:%d,kmalloc memory fail\n", __func__, __LINE__);
		return -ENOMEM;
	}
	uk_type = kmalloc(sizeof(*uk_type), GFP_KERNEL);
	if (!uk_type) {
		err = -ENOMEM;
		goto exit_err;
	}
	memset(emmckey_info, 0, sizeof(*emmckey_info));
	emmckey_info->key_init = 0;

	size = EMMCKEY_AREA_PHY_SIZE;
	if (get_reserve_partition_off_from_tbl() < 0) {
		err = -EINVAL;
		goto exit_err1;
	}
	addr = get_reserve_partition_off_from_tbl() + EMMCKEY_RESERVE_OFFSET;
	lba_start = addr >> bit;
	lba_end = (addr + size) >> bit;
	emmckey_info->key_init = 1;

	pr_info("%s:%d emmc key lba_start:0x%llx,lba_end:0x%llx\n",
		__func__, __LINE__, lba_start, lba_end);

	if (!emmckey_info->key_init) {
		err = -EINVAL;

		pr_info("%s:%d,emmc key init fail\n", __func__, __LINE__);
		goto exit_err1;
	}
	emmckey_info->keyarea_phy_addr = addr;
	emmckey_info->keyarea_phy_size = size;
	emmckey_info->lba_start = lba_start;
	emmckey_info->lba_end   = lba_end;
	mmc_card_key = card;

	uk_type->storage_type = UNIFYKEY_STORAGE_TYPE_EMMC;
	uk_type->ops = &ops;
	uk_type->ops->read = emmc_key_read;
	uk_type->ops->write = emmc_key_write;
/*
	if (register_unifykey_types(uk_type)) {
		err = -EINVAL;
		pr_info("%s:%d,emmc key check fail\n", __func__, __LINE__);
		goto exit_err1;
	}
	pr_info("emmc key: %s:%d ok.\n", __func__, __LINE__);
*/
exit_err1:
	kfree(uk_type);
exit_err:
	kfree(emmckey_info);
	return err;
}

