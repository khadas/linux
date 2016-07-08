/*
 * drivers/amlogic/mmc/emmc_key.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#include <linux/types.h>
/* #include <linux/device.h> */
/* #include <linux/blkdev.h> */
#include <linux/err.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/slab.h>

#include <linux/mmc/card.h>
#include <linux/mmc/emmc_partitions.h>
#include <linux/amlogic/key_manage.h>
#include "emmc_key.h"


/*
 * kernel head file
 *
 ********************************** */
static struct mmc_card *mmc_card_key;
static struct aml_emmckey_info_t *emmckey_info;
/*
static u32 emmckey_calculate_checksum(u8 *buf, u32 lenth)
{
	u32 checksum = 0;
	u32 cnt;

	for (cnt = 0; cnt < lenth; cnt++)
		checksum += buf[cnt];

	return checksum;
}

static int emmc_key_transfer(u8 *buf, u32 *value, u32 len, u32 direct)
{
	u8 checksum = 0;
	u32 i;
	if (direct) {
		for (i = 0; i < len; i++)
			checksum += buf[i];

		for (i = 0; i < len; i++)
			buf[i] ^= checksum;

		*value = checksum;
		return 0;
	} else{
		checksum = *value;
		for (i = 0; i < len; i++)
			buf[i] ^= checksum;

		checksum = 0;

		for (i = 0; i < len; i++)
			checksum += buf[i];

		if (checksum == *value)
			return 0;

		return -1;
	}
}

static int emmc_key_kernel_rw(struct mmc_card *card,
	struct emmckey_valid_node_t *emmckey_valid_node, u8 *buf, u32 direct)
{
	int blk, cnt;
	int bit = card->csd.read_blkbits;
	int ret = -1;

	mmc_claim_host(card->host);
	ret = mmc_blk_main_md_part_switch(card);
	if (ret) {
		pr_err("%s: error %d mmc_blk_main_md_part_switch\n",
			__func__, ret);
		goto exit_err;
	}

	blk = emmckey_valid_node->phy_addr >> bit;
	cnt = emmckey_valid_node->phy_size >> bit;
	if (direct)
		ret = mmc_write_internal(card, blk, cnt, buf);
	else
		ret = mmc_read_internal(card, blk, cnt, buf);

exit_err:
	mmc_release_host(card->host);

	return ret;
}

static int emmc_key_rw(struct emmckey_valid_node_t *emmckey_valid_node,
						u8 *buf, u32 direct)
{
	struct mmc_card *card = mmc_card_key;
	return emmc_key_kernel_rw(card, emmckey_valid_node, buf, direct);
}
*/
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
		emmckey_valid_node = kmalloc(
			sizeof(*emmckey_valid_node), GFP_KERNEL);

		if (emmckey_valid_node == NULL) {
			pr_info("%s:%d,kmalloc memory fail\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		emmckey_valid_node->phy_addr = emmckey_info->keyarea_phy_addr
						+ part_size * keypart_cnt;
		emmckey_valid_node->phy_size = EMMC_KEYAREA_SIZE;
		emmckey_valid_node->next = NULL;
		emmckey_info->key_valid = 0;
		if (emmckey_info->key_valid_node == NULL) {

			emmckey_info->key_valid_node = emmckey_valid_node;

		} else{
			temp_valid_node = emmckey_info->key_valid_node;

			while (temp_valid_node->next != NULL)
				temp_valid_node = temp_valid_node->next;

			temp_valid_node->next = emmckey_valid_node;
		}
	} while (++keypart_cnt < emmckey_info->key_part_count);

#if 0
	struct emmckey_data_t *emmckey_data;
	u32 checksum;
	/*read key data from emmc key area*/
	temp_valid_node = emmckey_info->key_valid_node;
	if (temp_valid_node == NULL) {
		pr_info("%s:%d,don't find emmc key valid node\n",
			__func__, __LINE__);
		return -1;
	}
	emmckey_data = kmalloc(sizeof(*emmckey_data), GFP_KERNEL);
	if (emmckey_data == NULL) {
		pr_info("%s:%d,kmalloc memory fail\n", __func__, __LINE__);
		return -ENOMEM;
	}
	/*read key data */
	memset(emmckey_data, 0, sizeof(*emmckey_data));
	emmc_key_rw(provider, temp_valid_node, (u8 *)emmckey_data, 0);
	if (!memcmp(emmckey_data->keyarea_mark, EMMC_KEY_AREA_SIGNAL, 8)) {
		checksum = emmckey_calculate_checksum(emmckey_data->data,
							EMMCKEY_DATA_VALID_LEN);

		if (checksum == emmckey_data->checksum)
			emmckey_info->key_valid = 1;
	}
	/*write key data to emmc key area*/
	if (emmckey_info->key_valid == 0) {
		temp_valid_node = emmckey_info->key_valid_node;
		while (temp_valid_node) {
			memset(emmckey_data, 0, sizeof(*emmckey_data));
			memcpy(emmckey_data->keyarea_mark,
				EMMC_KEY_AREA_SIGNAL, 8);

			emmckey_data->checksum = emmckey_calculate_checksum(
				emmckey_data->data, EMMCKEY_DATA_VALID_LEN);
			emmc_key_rw(provider, temp_valid_node,
				(u8 *)emmckey_data, 1);

			temp_valid_node = temp_valid_node->next;
		}
		emmckey_info->key_valid = 1;
	}
	kfree(emmckey_data);
#else
	emmckey_info->key_valid = 1;
#endif
	return 0;
}
/*
static int32_t emmc_keybox_read(uint8_t *buf, int32_t size, int flags)
{
	int err = -1;
	u32 checksum;
	struct emmckey_valid_node_t *emmckey_valid_node;
	struct emmckey_data_t *emmckey_data;

	if (!emmckey_info->key_valid) {
		pr_info("%s:%d,can't read emmc key\n", __func__, __LINE__);
		return -1;
	}
	if (size > EMMCKEY_DATA_VALID_LEN) {
		pr_info("%s:%d,size is too big,fact:0x%x,need:0x%x\n",
		__func__, __LINE__, size, EMMCKEY_DATA_VALID_LEN);
		return -1;
	}
	emmckey_data = kmalloc(sizeof(*emmckey_data), GFP_KERNEL);
	if (emmckey_data == NULL) {
		pr_info("%s:%d,kmalloc memory fail\n", __func__, __LINE__);
		return -ENOMEM;
	}

	emmckey_valid_node = emmckey_info->key_valid_node;
	while (emmckey_valid_node) {
		memset(emmckey_data, 0, sizeof(*emmckey_data));
		err = emmc_key_rw(emmckey_valid_node, (u8 *)emmckey_data, 0);

		if (err == 0) {
			emmc_key_transfer(emmckey_data->keyarea_mark,
			&emmckey_data->keyarea_mark_checksum, 8, 0);

			if (!memcmp(emmckey_data->keyarea_mark,
					EMMC_KEY_AREA_SIGNAL, 8)) {

				checksum = emmckey_calculate_checksum(
					emmckey_data->data,
					EMMCKEY_DATA_VALID_LEN);

				if (checksum == emmckey_data->checksum) {
					memcpy(buf, emmckey_data->data, size);
					err = 0;
					break;
				}
			}
		}
		emmckey_valid_node = emmckey_valid_node->next;
	}
	kfree(emmckey_data);
	return err;
}


static int32_t emmc_keybox_write(uint8_t *buf, int32_t size)
{
	int err = 0;
	struct emmckey_valid_node_t *emmckey_valid_node;
	struct emmckey_data_t *emmckey_data;

	if (!emmckey_info->key_valid) {
		pr_info("%s:%d,can't write emmc key\n", __func__, __LINE__);
		return -1;
	}
	if (size > EMMCKEY_DATA_VALID_LEN) {
		pr_info("%s:%d,size is too big,fact:0x%x,need:0x%x\n",
			__func__, __LINE__, size, EMMCKEY_DATA_VALID_LEN);
		return -1;
	}
	emmckey_data = kmalloc(sizeof(*emmckey_data), GFP_KERNEL);
	if (emmckey_data == NULL) {
		pr_info("%s:%d,kmalloc memory fail\n", __func__, __LINE__);
		return -ENOMEM;
	}

	memset(emmckey_data, 0, sizeof(*emmckey_data));
	memcpy(emmckey_data->keyarea_mark, EMMC_KEY_AREA_SIGNAL, 8);
	emmc_key_transfer(emmckey_data->keyarea_mark,
				&emmckey_data->keyarea_mark_checksum, 8, 1);

	memcpy(emmckey_data->data, buf, size);
	emmckey_data->checksum = emmckey_calculate_checksum(
				emmckey_data->data, EMMCKEY_DATA_VALID_LEN);

	emmckey_valid_node = emmckey_info->key_valid_node;
	while (emmckey_valid_node) {
		err = emmc_key_rw(emmckey_valid_node, (u8 *)emmckey_data, 1);
		if (err != 0) {
			pr_info("%s:%d,write key data fail,err:%d\n",
					__func__, __LINE__, err);
		}
		emmckey_valid_node = emmckey_valid_node->next;
	}

	kfree(emmckey_data);
	return err;
}
*/

/*
 * 1  when key data is wrote to emmc,
 *add checksum to verify if the data is correct;
 *	when key data is read from emmc,
 *use checksum to verify if the data is correct.
 * 2  read/write size is constant
 * 3  setup link table to link different area same as nand.
 * 4  key area is split 2 or more area to save key data
 * */

#if 0
static char write_buf[50] = {"write 2 : 22222222222"};

static char read_buf[100] = {0};

#include <asm-generic/uaccess.h>
static int32_t emmc_keybox_read_file(uint8_t *buf, int32_t size, int flags)
{
	char filename[3][30] = {"/dev/cardblkinand2",
			"/dev/cardblkinand3", "/dev/cardblkinand4"};
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	int i;
	/* char *file = provider->priv; */
	char *file;
	memset(read_buf, 0, sizeof(read_buf));
	for (i = 0; i < 3; i++) {
		file = &filename[i][0];
		fp = filp_open(file, O_RDWR, 0644);
		if (IS_ERR(fp)) {
			pr_info("open file error:%s\n", file);
			return -1;
		}
		pr_info("open file ok:%s\n", file);
		fs = get_fs();
		set_fs(KERNEL_DS);
		pos = 0;
		/* buf = &read_buf[i][0]; */
		/* size = 55; */
		vfs_read(fp, buf, size, &pos);
		filp_close(fp, NULL);
		set_fs(fs);
		memcpy(&read_buf[0], buf, 55);
		pr_info("%s:%d,read data:%s\n", __func__,
				__LINE__, &read_buf[0]);
	}
	return 0;
}
static int32_t emmc_keybox_write_file(uint8_t *buf, int32_t size)
{
	char filename[2][30] = {"/dev/cardblkinand2", "/dev/cardblkinand3"};
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	int i;
	char *file;/* = provider->priv; */
	for (i = 0; i < 2; i++) {
		file = &filename[i][0];
		fp = filp_open(file, O_RDWR, 0644);
		if (IS_ERR(fp)) {
			pr_info("open file error\n");
			return -1;
		}
		pr_info("%s:%d,open file ok:%s\n", __func__, __LINE__, file);
		fs = get_fs();
		set_fs(KERNEL_DS);
		pos = 0;
		memcpy(buf, &write_buf[0], 55);
		/* size = 55; */
		vfs_write(fp, buf, size, &pos);
		filp_close(fp, NULL);
		set_fs(fs);
	}
	return 0;
}
#endif

/*
#define TEST_STRING_1  "kernel test 11113451111111111"
#define TEST_STRING_2  "kernel 2222222789222"
#define TEST_STRING_3  "kernel test 3333333333333333 3333333333333333"
static void fill_data(uint8_t *buffer)
{
	memset(buffer, 0, 2048);
	memcpy(&buffer[0],
			TEST_STRING_1, sizeof(TEST_STRING_1));
	memcpy(&buffer[512],
			TEST_STRING_2, sizeof(TEST_STRING_2));
	memcpy(&buffer[1024],
			TEST_STRING_3, sizeof(TEST_STRING_3));

}
*/
#define		EMMC_BLOCK_SIZE		(0x100)
#define		MAX_EMMC_BLOCK_SIZE	(128*1024)

int32_t emmc_key_read(uint8_t *buffer,
	uint32_t length, uint32_t *actual_lenth)
{
	int ret;
	u64  addr = 0;
	u32  size = 0;
	int blk, cnt;
	unsigned char *dst = NULL;
	struct mmc_card *card = mmc_card_key;
	int bit = card->csd.read_blkbits;
	size = length;
	*actual_lenth = length;
	addr = get_reserve_partition_off_from_tbl() + EMMCKEY_RESERVE_OFFSET;
	blk = addr >> bit;
	cnt = size >> bit;
	dst = (unsigned char *)buffer;
	mmc_claim_host(card->host);
	do {
		ret = mmc_read_internal(card, blk, EMMC_BLOCK_SIZE, dst);
		if (ret) {
			pr_err("%s [%d] mmc_write_internal error\n",
				__func__, __LINE__);
			return ret;
		}
		blk += EMMC_BLOCK_SIZE;
		cnt -= EMMC_BLOCK_SIZE;
		dst = (unsigned char *)buffer + MAX_EMMC_BLOCK_SIZE;
	} while (cnt != 0);
	pr_info("%s:%d, read %s\n", __func__, __LINE__, (ret) ? "error":"ok");
	/*
	pr_info("%s:%d,%s\n", __func__, __LINE__, buffer);
	pr_info("%s:%d,%s\n", __func__, __LINE__, &buffer[512]);
	pr_info("%s:%d,%s\n", __func__, __LINE__, &buffer[1024]);
	*/
	mmc_release_host(card->host);
	return ret;
}
EXPORT_SYMBOL(emmc_key_read);

/*
int32_t emmc_key_read(uint8_t *buffer, uint32_t length)
{
	int ret;
	memset(buffer, 0, 1024*3);
	length = EMMCKEY_DATA_VALID_LEN/2;
	ret = emmc_keybox_read(buffer, length, 0);

	pr_info("%s:%d, read %s\n", __func__, __LINE__, (ret) ? "error":"ok");
	pr_info("%s:%d,%s\n", __func__, __LINE__, buffer);
	pr_info("%s:%d,%s\n", __func__, __LINE__, &buffer[512]);
	pr_info("%s:%d,%s\n", __func__, __LINE__, &buffer[1024]);
	return ret;
}*/

/*
int32_t emmc_key_write(uint8_t *buffer, uint32_t length)
{
	int ret;
	fill_data(buffer);
	length = EMMCKEY_DATA_VALID_LEN;
	ret = emmc_keybox_write(buffer, length);
	pr_info("%s:%d, write %s\n", __func__, __LINE__, (ret) ? "error":"ok");

	return ret;
}
*/
int32_t emmc_key_write(uint8_t *buffer,
	uint32_t length, uint32_t *actual_lenth)
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
	pr_info("%s:%d, write %s\n", __func__, __LINE__, (ret) ? "error":"ok");
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

	pr_info("card key: card_blk_probe.\n");
	emmckey_info = kmalloc(sizeof(*emmckey_info), GFP_KERNEL);
	if (emmckey_info == NULL) {
		pr_info("%s:%d,kmalloc memory fail\n", __func__, __LINE__);
		return -ENOMEM;
	}
	memset(emmckey_info, 0, sizeof(*emmckey_info));
	emmckey_info->key_init = 0;

	size = EMMCKEY_AREA_PHY_SIZE;
	addr = get_reserve_partition_off_from_tbl() + EMMCKEY_RESERVE_OFFSET;
	if (addr < 0) {
		err = -EINVAL;
		goto exit_err;
	}
	lba_start = addr >> bit;
	lba_end = (addr + size) >> bit;
	emmckey_info->key_init = 1;

	pr_info("%s:%d emmc key lba_start:0x%llx,lba_end:0x%llx\n",
	 __func__, __LINE__, lba_start, lba_end);

	if (!emmckey_info->key_init) {
		err = -EINVAL;

		pr_info("%s:%d,emmc key init fail\n", __func__, __LINE__);
		goto exit_err;
	}
	emmckey_info->keyarea_phy_addr = addr;
	emmckey_info->keyarea_phy_size = size;
	emmckey_info->lba_start = lba_start;
	emmckey_info->lba_end   = lba_end;
	mmc_card_key = card;
	err = aml_emmc_key_check();
	if (err) {
		pr_info("%s:%d,emmc key check fail\n", __func__, __LINE__);
	goto exit_err;
	}

	storage_ops_read(emmc_key_read);
	storage_ops_write(emmc_key_write);
	/* err = aml_keybox_provider_register(&emmc_provider);
	if (err)
		pr_info("emmc key register fail.\n"); */

	pr_info("emmc key: %s:%d ok.\n", __func__, __LINE__);
	return err;

exit_err:
	kfree(emmckey_info);
	return err;
}

