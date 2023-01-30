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

#define     MAX_TRANS_BLK        (256)

#define stamp_after(a, b)	((int)(b) - (int)(a)  < 0)
#define KEY_BACKUP

/*
 * kernel head file
 *
 */
static struct mmc_card *mmc_card_key;
static struct aml_emmckey_info_t *emmckey_info;

#ifdef KEY_BACKUP
static struct aml_key_info key_infos[2] = { {0, 0, 0}, {0, 0, 0} };

int32_t emmc_read_valid_key(void *buffer, int valid_flag)
{
	int ret;
	u64  addr = 0;
	u32  size = 0;
	int blk, cnt;
	unsigned char *dst = NULL;
	struct mmc_card *card = mmc_card_key;
	int bit = card->csd.read_blkbits;

	size = EMMC_KEYAREA_SIZE;
	/*
	 * The lib functions don't need to be modified.
	 */
	/* coverity[overflow_before_widen:SUPPRESS] */
	addr = get_reserve_partition_off_from_tbl() + EMMCKEY_RESERVE_OFFSET
		+ (valid_flag - 1) * EMMC_KEYAREA_SIZE;
	blk = addr >> bit;
	cnt = size >> bit;
	dst = (unsigned char *)buffer;

	mmc_claim_host(card->host);
	aml_disable_mmc_cqe(card);
	do {
		ret = mmc_read_internal(card, blk, EMMC_BLOCK_SIZE, dst);
		if (ret) {
			pr_err("%s [%d] mmc_write_internal error\n",
				__func__, __LINE__);
			break;
		}
		blk += EMMC_BLOCK_SIZE;
		cnt -= EMMC_BLOCK_SIZE;
		dst = (unsigned char *)buffer + MAX_EMMC_BLOCK_SIZE;
	} while (cnt != 0);
	pr_info("%s:%d, read %s\n", __func__, __LINE__, (ret) ? "error" : "ok");
	aml_enable_mmc_cqe(card);
	mmc_release_host(card->host);

	return ret;
}

/* key read&write operation with backup updates */
static u64 _calc_key_checksum(void *addr, int size)
{
	int i = 0;
	u32 *buffer;
	u64 checksum = 0;

	buffer = (u32 *)addr;
	size = size >> 2;
	while (i < size)
		checksum += buffer[i++];

	return checksum;
}

int32_t emmc_write_one_key(void *buffer, int valid_flag)
{
	int ret;
	u64 blk, cnt, key_glb_offset;
	unsigned char *src = NULL;
	struct mmc_card *card = mmc_card_key;
	int bit = card->csd.read_blkbits;
	unsigned char *checksum_info;

	checksum_info = kmalloc(512, GFP_KERNEL);
	if (!checksum_info)
		return -1;

	memset(checksum_info, 0, 512);
	key_glb_offset = get_reserve_partition_off_from_tbl() + EMMCKEY_RESERVE_OFFSET;
	blk = (key_glb_offset + (valid_flag % 2) * EMMC_KEYAREA_SIZE) >> bit;
	cnt = EMMC_KEYAREA_SIZE >> bit;
	src = (unsigned char *)buffer;
	memcpy(checksum_info, &key_infos[valid_flag - 1], sizeof(struct aml_key_info));
	mmc_claim_host(card->host);
	aml_disable_mmc_cqe(card);
	do {
		ret = mmc_write_internal(card, blk, EMMC_BLOCK_SIZE, src);
		if (ret) {
			pr_err("%s [%d] mmc_write_internal error\n",
				__func__, __LINE__);
			goto exit_err;
		}
		blk += EMMC_BLOCK_SIZE;
		cnt -= EMMC_BLOCK_SIZE;
		src = (unsigned char *)buffer + MAX_EMMC_BLOCK_SIZE;
		pr_info("cnt %llu\n", cnt);
	} while (cnt != 0);

	blk = ((key_glb_offset + 2 * (EMMC_KEYAREA_SIZE)) >> bit) + (valid_flag % 2);
	ret = mmc_write_internal(card, blk, 1, checksum_info);
	if (ret)
		pr_err("%s: block # %#llx, ERROR!\n", __func__, blk);

	pr_info("%s:%d, write %s\n", __func__, __LINE__, (ret) ? "error" : "ok");
	aml_enable_mmc_cqe(card);
	mmc_release_host(card->host);

exit_err:
	kfree(checksum_info);
	return ret;
}

int update_old_key(struct mmc_card *mmc, void *addr)
{
	int ret = 0;
	int valid_flag;

	if (stamp_after(key_infos[1].stamp, key_infos[0].stamp)) {
		memcpy(&key_infos[1], &key_infos[0], sizeof(struct aml_key_info));
		valid_flag = 2;
	} else if (stamp_after(key_infos[0].stamp, key_infos[1].stamp)) {
		memcpy(&key_infos[0], &key_infos[1], sizeof(struct aml_key_info));
		valid_flag = 1;
	} else {
		pr_info("do nothing\n");
		return ret;
	}

	ret = emmc_write_one_key(addr, valid_flag);
	if (ret)
		return ret;
	mmc->key_stamp = key_infos[0].stamp;
	return ret;
}

static int _verify_key_checksum(struct mmc_card *mmc, void *addr, int cpy)
{
	u64 checksum;
	int ret = 0;
	u64 blk;
	char *checksum_info;
	int bit = mmc->csd.read_blkbits;

	blk = get_reserve_partition_off_from_tbl() + EMMCKEY_RESERVE_OFFSET +
		EMMC_KEYAREA_SIZE * 2;
	blk = (blk >> bit) + cpy;
	checksum_info = kmalloc(512, GFP_KERNEL);
	if (!checksum_info)
		return -1;

	mmc_claim_host(mmc->host);
	aml_disable_mmc_cqe(mmc);
	ret =  mmc_read_internal(mmc, blk, 1, checksum_info);
	aml_enable_mmc_cqe(mmc);
	mmc_release_host(mmc->host);
	if (ret) {
		pr_err("%s, block # %#llx, ERROR!\n", __func__, blk);
		kfree(checksum_info);
		return ret;
	}

	memcpy(&key_infos[cpy], checksum_info, sizeof(struct aml_key_info));
	checksum = _calc_key_checksum(addr, EMMC_KEYAREA_SIZE);
	pr_info("calc %llx, store %llx\n", checksum, key_infos[cpy].checksum);

	kfree(checksum_info);
	return !(checksum == key_infos[cpy].checksum);
}

static int write_invalid_key(void *addr, int valid_flag)
{
	int ret;

	if (valid_flag > 2 || valid_flag < 1)
		return -1;

	ret = emmc_read_valid_key(addr, valid_flag);
	if (ret) {
		pr_err("read valid key failed\n");
		return ret;
	}
	ret = emmc_write_one_key(addr, valid_flag);
	if (ret) {
		pr_err("write invalid key failed\n");
		return ret;
	}
	return ret;
}

static int _amlmmc_read(struct mmc_card *mmc,
		     int blk, unsigned char *buf, int cnt)
{
	int ret = 0;
	unsigned char *dst = NULL;

	dst = (unsigned char *)buf;
	mmc_claim_host(mmc->host);
	aml_disable_mmc_cqe(mmc);
	do {
		ret = mmc_read_internal(mmc, blk, MAX_TRANS_BLK, dst);
		if (ret) {
			pr_err("%s: save key error", __func__);
			ret = -EFAULT;
			break;
		}
		blk += MAX_TRANS_BLK;
		cnt -= MAX_TRANS_BLK;
		dst = (unsigned char *)buf + MAX_EMMC_BLOCK_SIZE;
	} while (cnt != 0);
	aml_enable_mmc_cqe(mmc);
	mmc_release_host(mmc->host);
	return ret;
}

static int update_key_info(struct mmc_card *mmc, unsigned char *addr)
{
	int ret = 0;
	int cpy = 1;
	int bit = mmc->csd.read_blkbits;
	int blk;
	int cnt = EMMC_KEYAREA_SIZE >> bit;
	int valid_flag = 0;

	/* read key2 1st, for compatibility without checksum. */
	while (cpy >= 0) {
		blk = ((get_reserve_partition_off_from_tbl()
					+ EMMCKEY_RESERVE_OFFSET)
			+ cpy * EMMC_KEYAREA_SIZE) >> bit;
		if (_amlmmc_read(mmc, blk, addr, cnt)) {
			pr_err("%s: block # %#x ERROR!\n",
					__func__, blk);
		} else {
			ret = _verify_key_checksum(mmc, addr, cpy);
			if (!ret && key_infos[cpy].magic != 0)
				valid_flag += cpy + 1;
			else
				pr_err("cpy %d is not valid\n", cpy);
		}
		cpy--;
	}

	if (key_infos[0].stamp > key_infos[1].stamp)
		mmc->key_stamp = key_infos[0].stamp;
	else
		mmc->key_stamp = key_infos[1].stamp;

	return valid_flag;
}

static int _key_init(struct mmc_card *mmc)
{
	int ret = 0;
	void *key;
	int valid = 0;

	/*malloc 256k byte to key */
	key = kmalloc(EMMC_KEYAREA_SIZE, GFP_KERNEL);
	if (!key)
		return -ENOMEM;

	valid = update_key_info(mmc, key);

	switch (valid) {
	case 0:
	break;
	case 1:
		write_invalid_key(key, 1);
	break;
	case 2:
		write_invalid_key(key, 2);
	break;
	case 3:
		update_old_key(mmc, key);
	break;
	default:
		pr_err("impossible valid values.\n");
	break;
	}

	kfree(key);
	return ret;
}
#endif

#ifdef KEY_BACKUP
int32_t emmc_key_write(u8 *buffer,
	u32 length, u32 *actual_length)
{
	int ret;
	int cpy = 1, index;
	struct mmc_card *card = mmc_card_key;

	do {
		index = cpy - 1;
		key_infos[index].stamp = card->key_stamp + 1;
		key_infos[index].checksum = _calc_key_checksum(buffer, length);
		key_infos[index].magic = 9;
		pr_info("new stamp %d, checksum 0x%llx, magic %d\n",
				key_infos[index].stamp,
				key_infos[index].checksum,
				key_infos[index].magic);

		ret = emmc_write_one_key(buffer, cpy);
		if (ret) {
			pr_info("write %d failed\n", cpy);
			return ret;
		}
		cpy++;
	} while (cpy < 3);
	return ret;
}
EXPORT_SYMBOL(emmc_key_write);
#else
int32_t emmc_key_write(u8 *buffer,
		u32 length, u32 *actual_length)
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
	aml_disable_mmc_cqe(card);
	do {
		ret = mmc_write_internal(card, blk, EMMC_BLOCK_SIZE, src);
		if (ret) {
			pr_err("%s [%d] mmc_write_internal error\n",
					__func__, __LINE__);
			return ret;
		}
		blk += EMMC_BLOCK_SIZE;
		cnt -= EMMC_BLOCK_SIZE;
		src = (unsigned char *)buffer + MAX_EMMC_BLOCK_SIZE;
	} while (cnt != 0);
	pr_info("%s:%d, write %s\n", __func__, __LINE__, (ret) ? "error" : "ok");
	aml_enable_mmc_cqe(card);
	mmc_release_host(card->host);
	return ret;
}
EXPORT_SYMBOL(emmc_key_write);
#endif

static int aml_emmc_key_check(void)
{
	u8 keypart_cnt;
	u64 part_size;
	struct emmckey_valid_node_t *emmckey_valid_node, *temp_valid_node;

	emmckey_info->key_part_count =
		emmckey_info->keyarea_phy_size / EMMC_KEYAREA_SIZE;

	if (emmckey_info->key_part_count
			> EMMC_KEYAREA_COUNT) {
		emmckey_info->key_part_count = EMMC_KEYAREA_COUNT;
	}
	keypart_cnt = 0;
	part_size = EMMC_KEYAREA_SIZE;
	do {
		emmckey_valid_node = kmalloc(sizeof(*emmckey_valid_node),
				GFP_KERNEL);

		if (!emmckey_valid_node) {
			pr_info("%s:%d,kmalloc memory fail\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		emmckey_valid_node->phy_addr = emmckey_info->keyarea_phy_addr
						+ part_size * keypart_cnt;
		emmckey_valid_node->phy_size = EMMC_KEYAREA_SIZE;
		emmckey_valid_node->next = NULL;
		emmckey_info->key_valid = 0;
		if (!emmckey_info->key_valid_node) {
			emmckey_info->key_valid_node = emmckey_valid_node;

		} else {
			temp_valid_node = emmckey_info->key_valid_node;

			while (temp_valid_node->next)
				temp_valid_node = temp_valid_node->next;

			temp_valid_node->next = emmckey_valid_node;
		}
	} while (++keypart_cnt < emmckey_info->key_part_count);

	emmckey_info->key_valid = 1;
	return 0;
}

int32_t emmc_key_read(u8 *buffer,
	u32 length, u32 *actual_length)
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
	aml_disable_mmc_cqe(card);
	do {
		ret = mmc_read_internal(card, blk,
					min(EMMC_BLOCK_SIZE, cnt), dst);
		if (ret) {
			pr_err("%s [%d] mmc_write_internal error\n",
			       __func__, __LINE__);
			break;
		}
		blk += EMMC_BLOCK_SIZE;
		cnt -= EMMC_BLOCK_SIZE;
		dst = (unsigned char *)buffer + MAX_EMMC_BLOCK_SIZE;
	} while (cnt > 0);

	pr_info("%s:%d, read %s\n", __func__, __LINE__, (ret) ? "error" : "ok");

	aml_enable_mmc_cqe(card);
	mmc_release_host(card->host);
	return ret;
}
EXPORT_SYMBOL(emmc_key_read);

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
#ifdef KEY_BACKUP
	size = EMMCKEY_AREA_PHY_SIZE + (512 * 2);
#else
	size = EMMCKEY_AREA_PHY_SIZE;
#endif
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
#ifdef KEY_BACKUP
	_key_init(card);
#endif
	err = aml_emmc_key_check();
	if (err) {
		pr_info("%s:%d,emmc key check fail\n", __func__, __LINE__);
		goto exit_err1;
	}

	uk_type->storage_type = UNIFYKEY_STORAGE_TYPE_EMMC;
	uk_type->ops = &ops;
	uk_type->ops->read = emmc_key_read;
	uk_type->ops->write = emmc_key_write;

	if (register_unifykey_types(uk_type)) {
		err = -EINVAL;
		pr_info("%s:%d,emmc key check fail\n", __func__, __LINE__);
		goto exit_err1;
	}
	pr_info("emmc key: %s:%d ok.\n", __func__, __LINE__);
exit_err1:
	kfree(uk_type);
exit_err:
	kfree(emmckey_info);
	return err;
}
