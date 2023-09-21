// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/scatterlist.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/mmc/emmc_partitions.h>
#include "emmc_key.h"
#include <linux/amlogic/aml_sd.h>
#include "../../mmc/core/mmc_ops.h"
#include "../../mmc/core/core.h"
#include <linux/dma-contiguous.h>
#include "../efuse_unifykey/efuse.h"

#define DTB_NAME		"dtb"
#define	SZ_1M			0x00100000
#define	MMC_DTB_PART_OFFSET	(40 * SZ_1M)
#define	EMMC_BLOCK_SIZE		(0x100)
#define	MAX_EMMC_BLOCK_SIZE	(128 * 1024)

#define DTB_RESERVE_OFFSET	(4 * SZ_1M)
#define	DTB_BLK_SIZE		(0x200)
#define	DTB_BLK_CNT		(512)
#define	DTB_SIZE		(DTB_BLK_CNT * DTB_BLK_SIZE)
#define DTB_COPIES		(2)
#define DTB_AREA_BLK_CNT	(DTB_BLK_CNT * DTB_COPIES)
/* pertransfer for internal operations. */
#define MAX_TRANS_BLK		(256)
#define	MAX_TRANS_SIZE		(MAX_TRANS_BLK * DTB_BLK_SIZE)
#define stamp_after(a, b)	((int)(b) - (int)(a)  < 0)

#define GPT_HEADER_SIGNATURE	0x5452415020494645ULL

struct aml_dtb_rsv {
	u8 data[DTB_BLK_SIZE * DTB_BLK_CNT - 4 * sizeof(unsigned int)];
	unsigned int magic;
	unsigned int version;
	unsigned int timestamp;
	unsigned int checksum;
};

struct aml_dtb_info {
	unsigned int stamp[2];
	u8 valid[2];
};

struct  efi_guid_t {
	u8 b[16];
};

struct gpt_header {
	__le64 signature;
	__le32 revision;
	__le32 header_size;
	__le32 header_crc32;
	__le32 reserved1;
	__le64 my_lba;
	__le64 alternate_lba;
	__le64 first_usable_lba;
	__le64 last_usable_lba;
	struct efi_guid_t disk_guid;
	__le64 partition_entry_lba;
	__le32 num_partition_entries;
	__le32 sizeof_partition_entry;
	__le32 partition_entry_array_crc32;
};

static dev_t amlmmc_dtb_no;
struct cdev amlmmc_dtb;
struct device *dtb_dev;
struct class *amlmmc_dtb_class;
static char *glb_dtb_buf;
struct mmc_card *card_dtb;
static struct aml_dtb_info dtb_infos = {{0, 0}, {0, 0} };
struct mmc_partitions_fmt *pt_fmt;

int aml_disable_mmc_cqe(struct mmc_card *card)
{
	int ret = 0;

	if (card->reenable_cmdq && card->ext_csd.cmdq_en) {
		pr_debug("[%s] [%d]\n", __func__, __LINE__);
		ret = mmc_cmdq_disable(card);
		if (ret)
			pr_err("[%s] disable cqe mode failed\n", __func__);
	}
	return ret;
}

int aml_enable_mmc_cqe(struct mmc_card *card)
{
	int ret = 0;

	if (card->reenable_cmdq && !card->ext_csd.cmdq_en) {
		pr_debug("[%s] [%d]\n", __func__, __LINE__);
		ret = mmc_cmdq_enable(card);
		if (ret)
			pr_err("[%s] reenable cqe mode failed\n", __func__);
	}
	return ret;
}

/* dtb read&write operation with backup updates */
static unsigned int _calc_dtb_checksum(struct aml_dtb_rsv *dtb)
{
	int i = 0;
	int size = sizeof(struct aml_dtb_rsv) - sizeof(unsigned int);
	unsigned int *buffer;
	unsigned int checksum = 0;

	size = size >> 2;
	buffer = (unsigned int *)dtb;
	while (i < size)
		checksum += buffer[i++];

	return checksum;
}

static int _verify_dtb_checksum(struct aml_dtb_rsv *dtb)
{
	unsigned int checksum;

	checksum = _calc_dtb_checksum(dtb);
	pr_info("calc %x, store %x\n", checksum, dtb->checksum);

	return !(checksum == dtb->checksum);
}

static int _dtb_write(struct mmc_card *mmc, int blk, unsigned char *buf)
{
	int ret = 0;
	unsigned char *src = NULL;
	int bit = mmc->csd.read_blkbits;
	int cnt = CONFIG_DTB_SIZE >> bit;

	src = (unsigned char *)buf;

	mmc_claim_host(mmc->host);
	aml_disable_mmc_cqe(mmc);
	do {
		ret = mmc_write_internal(mmc, blk, MAX_TRANS_BLK, src);
		if (ret) {
			pr_err("%s: save dtb error", __func__);
			ret = -EFAULT;
			break;
		}
		blk += MAX_TRANS_BLK;
		cnt -= MAX_TRANS_BLK;
		src = (unsigned char *)buf + MAX_TRANS_SIZE;
	} while (cnt != 0);
	aml_enable_mmc_cqe(mmc);
	mmc_release_host(mmc->host);

	return ret;
}

static int _dtb_read(struct mmc_card *mmc, int blk, unsigned char *buf)
{
	int ret = 0;
	unsigned char *dst = NULL;
	int bit = mmc->csd.read_blkbits;
	int cnt = CONFIG_DTB_SIZE >> bit;

	dst = (unsigned char *)buf;
	mmc_claim_host(mmc->host);
	aml_disable_mmc_cqe(mmc);
	do {
		ret = mmc_read_internal(mmc, blk, MAX_TRANS_BLK, dst);
		if (ret) {
			pr_err("%s: save dtb error", __func__);
			ret = -EFAULT;
			break;
		}
		blk += MAX_TRANS_BLK;
		cnt -= MAX_TRANS_BLK;
		dst = (unsigned char *)buf + MAX_TRANS_SIZE;
	} while (cnt != 0);
	aml_enable_mmc_cqe(mmc);
	mmc_release_host(mmc->host);
	return ret;
}

static int _dtb_init(struct mmc_card *mmc)
{
	int ret = 0;
	struct aml_dtb_rsv *dtb;
	struct aml_dtb_info *info = &dtb_infos;
	int cpy = 1, valid = 0;
	int bit = mmc->csd.read_blkbits;
	int blk;

	if (!glb_dtb_buf) {
		glb_dtb_buf = kmalloc(CONFIG_DTB_SIZE, GFP_KERNEL);
		if (!glb_dtb_buf)
			return -ENOMEM;
	}
	dtb = (struct aml_dtb_rsv *)glb_dtb_buf;

	/* read dtb2 1st, for compatibility without checksum. */
	while (cpy >= 0) {
		blk = ((get_reserve_partition_off_from_tbl()
					+ DTB_RESERVE_OFFSET) >> bit)
			+ cpy * DTB_BLK_CNT;
		if (_dtb_read(mmc, blk, (unsigned char *)dtb)) {
			pr_err("%s: block # %#x ERROR!\n", __func__, blk);
		} else {
			ret = _verify_dtb_checksum(dtb);
			if (!ret) {
				info->stamp[cpy] = dtb->timestamp;
				info->valid[cpy] = 1;
			} else {
				pr_err("cpy %d is not valid\n", cpy);
			}
		}
		valid += info->valid[cpy];
		cpy--;
	}
	pr_info("total valid %d\n", valid);

	return ret;
}

int amlmmc_dtb_write(struct mmc_card *mmc, unsigned char *buf, int len)
{
	int ret = 0, blk;
	int bit = mmc->csd.read_blkbits;
	int cpy, valid;
	struct aml_dtb_rsv *dtb = (struct aml_dtb_rsv *)buf;
	struct aml_dtb_info *info = &dtb_infos;

	if (len > CONFIG_DTB_SIZE) {
		pr_err("%s dtb data len too much", __func__);
		return -EFAULT;
	}
	/* set info */
	valid = info->valid[0] + info->valid[1];
	if (valid == 0) {
		dtb->timestamp = 0;
	} else if (valid == 1) {
		dtb->timestamp = 1 + info->stamp[info->valid[0] ? 0 : 1];
	} else {
		/* both are valid */
		if (info->stamp[0] != info->stamp[1]) {
			pr_info("timestamp are not same %d:%d\n",
				info->stamp[0], info->stamp[1]);
			dtb->timestamp = 1 +
				(stamp_after(info->stamp[1], info->stamp[0]) ?
				info->stamp[1] : info->stamp[0]);
		} else {
			dtb->timestamp = 1 + info->stamp[0];
		}
	}
	/*setting version and magic*/
	dtb->version = 1; /* base version */
	dtb->magic = 0x00447e41; /*A~D\0*/
	dtb->checksum = _calc_dtb_checksum(dtb);
	pr_info("stamp %d, checksum 0x%x, version %d, magic %s\n",
		dtb->timestamp, dtb->checksum,
		dtb->version, (char *)&dtb->magic);
	/* write down... */
	for (cpy = 0; cpy < DTB_COPIES; cpy++) {
		blk = ((get_reserve_partition_off_from_tbl()
					+ DTB_RESERVE_OFFSET) >> bit)
			+ cpy * DTB_BLK_CNT;
		ret |= _dtb_write(mmc, blk, buf);
	}

	return ret;
}

int amlmmc_dtb_read(struct mmc_card *card, unsigned char *buf, int len)
{
	int ret = 0, start_blk, size, blk_cnt;
	int bit = card->csd.read_blkbits;
	unsigned char *dst = NULL;
	unsigned char *buffer = NULL;

	if (len > CONFIG_DTB_SIZE) {
		pr_err("%s dtb data len too much", __func__);
		return -EFAULT;
	}
	memset(buf, 0x0, len);

	start_blk = MMC_DTB_PART_OFFSET;
	buffer = kmalloc(DTB_CELL_SIZE, GFP_KERNEL | __GFP_RECLAIM);
	if (!buffer)
		return -ENOMEM;

	start_blk >>= bit;
	size = CONFIG_DTB_SIZE;
	blk_cnt = size >> bit;
	dst = (unsigned char *)buffer;
	while (blk_cnt != 0) {
		memset(buffer, 0x0, DTB_CELL_SIZE);
		ret = mmc_read_internal(card, start_blk,
					(DTB_CELL_SIZE >> bit), dst);
		if (ret) {
			pr_err("%s read dtb error", __func__);
			ret = -EFAULT;
			kfree(buffer);
			return ret;
		}
		start_blk += (DTB_CELL_SIZE >> bit);
		blk_cnt -= (DTB_CELL_SIZE >> bit);
		memcpy(buf, dst, DTB_CELL_SIZE);
		buf += DTB_CELL_SIZE;
	}
	kfree(buffer);
	return ret;
}
static CLASS_ATTR_STRING(emmcdtb, 0644, NULL);

int mmc_dtb_open(struct inode *node, struct file *file)
{
	return 0;
}

ssize_t mmc_dtb_read(struct file *file,
		char __user *buf,
		size_t count,
		loff_t *ppos)
{
	unsigned char *dtb_ptr = NULL;
	unsigned char *dst = NULL;
	ssize_t read_size = 0;
	int bit = card_dtb->csd.read_blkbits;
	int blk = (MMC_DTB_PART_OFFSET + *ppos) >> bit;
	int read_count = 0;
	int cnt = 0;
	int ret = 0;

	if (*ppos == CONFIG_DTB_SIZE)
		return 0;

	if (*ppos >= CONFIG_DTB_SIZE) {
		pr_err("%s: out of space!", __func__);
		return -EFAULT;
	}

	dtb_ptr = glb_dtb_buf;
	if (!dtb_ptr)
		return -ENOMEM;

	if ((*ppos + count) > CONFIG_DTB_SIZE)
		read_size = CONFIG_DTB_SIZE - *ppos;
	else
		read_size = count;

	cnt = read_size >> bit;
	dst = dtb_ptr;
	mmc_claim_host(card_dtb->host);
	aml_disable_mmc_cqe(card_dtb);
	do {
		if (cnt > MAX_TRANS_BLK)
			read_count = MAX_TRANS_BLK;
		else
			read_count = cnt;
		ret = mmc_read_internal(card_dtb, blk, read_count, dst);
		if (ret) {
			pr_err("%s: read failed:%d", __func__, ret);
			ret = -EFAULT;
			goto exit;
		}
		blk += read_count;
		cnt -= read_count;
		dst += read_count << bit;
	} while (cnt != 0);

	ret = copy_to_user(buf, dtb_ptr, read_size);
	*ppos += read_size;

exit:
	aml_enable_mmc_cqe(card_dtb);
	mmc_release_host(card_dtb->host);
	return read_size;
}

ssize_t mmc_dtb_write(struct file *file,
		const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned char *dtb_ptr = NULL;
	unsigned char *dst = NULL;
	ssize_t write_size = 0;
	int bit = card_dtb->csd.read_blkbits;
	int ret = 0;
	int cnt = 0;
	int write_count = 0;
	int blk = (MMC_DTB_PART_OFFSET + *ppos) >> bit;

	if (*ppos == CONFIG_DTB_SIZE)
		return 0;
	if (*ppos >= CONFIG_DTB_SIZE) {
		pr_err("%s: out of space!", __func__);
		return -EFAULT;
	}

	dtb_ptr = glb_dtb_buf;
	if (!dtb_ptr)
		return -ENOMEM;

	if ((*ppos + count) > CONFIG_DTB_SIZE)
		write_size = CONFIG_DTB_SIZE - *ppos;
	else
		write_size = count;

	ret = copy_from_user(dtb_ptr, buf, write_size);

	cnt = round_up(write_size, DTB_BLK_SIZE) >> bit;
	dst = dtb_ptr;
	mmc_claim_host(card_dtb->host);
	aml_disable_mmc_cqe(card_dtb);
	do {
		if (cnt > MAX_TRANS_BLK)
			write_count = MAX_TRANS_BLK;
		else
			write_count = cnt;
		ret = mmc_write_internal(card_dtb, blk, write_count, dst);
		if (ret) {
			pr_err("%s: write failed:%d", __func__, ret);
			ret = -EFAULT;
			goto exit;
		}
		blk += write_count;
		cnt -= write_count;
		dst += write_count << bit;
	} while (cnt != 0);

	ret = amlmmc_dtb_read(card_dtb, dtb_ptr, CONFIG_DTB_SIZE);
	if (ret) {
		pr_err("%s: read dtb failed", __func__);
		ret = -EFAULT;
		goto exit;
	}
	ret = amlmmc_dtb_write(card_dtb, dtb_ptr, CONFIG_DTB_SIZE);
	if (ret) {
		pr_err("%s: write dtb failed", __func__);
		ret = -EFAULT;
		goto exit;
	}

	*ppos += write_size;
exit:
	aml_enable_mmc_cqe(card_dtb);
	mmc_release_host(card_dtb->host);
	return write_size;
}

long mmc_dtb_ioctl(struct file *file, unsigned int cmd, unsigned long args)
{
	return 0;
}

static const struct file_operations dtb_ops = {
	.open = mmc_dtb_open,
	.read = mmc_dtb_read,
	.write = mmc_dtb_write,
	.unlocked_ioctl = mmc_dtb_ioctl,
};

int amlmmc_dtb_init(struct mmc_card *card)
{
	int ret = 0;

	card_dtb = card;
	pr_info("%s: register dtb chardev", __func__);

	_dtb_init(card);

	ret = alloc_chrdev_region(&amlmmc_dtb_no, 0, 1, DTB_NAME);
	if (ret < 0) {
		pr_err("alloc dtb dev_t no failed");
		ret = -1;
		goto exit_err;
	}

	cdev_init(&amlmmc_dtb, &dtb_ops);
	amlmmc_dtb.owner = THIS_MODULE;
	ret = cdev_add(&amlmmc_dtb, amlmmc_dtb_no, 1);
	if (ret) {
		pr_err("dtb dev add failed");
		ret = -1;
		goto exit_err1;
	}

	amlmmc_dtb_class = class_create(THIS_MODULE, DTB_NAME);
	if (IS_ERR(amlmmc_dtb_class)) {
		pr_err("dtb dev add failed");
		ret = -1;
		goto exit_err2;
	}

	ret = class_create_file(amlmmc_dtb_class, &class_attr_emmcdtb.attr);
	if (ret) {
		pr_err("dtb dev add failed");
		ret = -1;
		goto exit_err2;
	}

	dtb_dev = device_create(amlmmc_dtb_class, NULL,
				amlmmc_dtb_no, NULL, DTB_NAME);
	if (IS_ERR(dtb_dev)) {
		pr_err("dtb dev add failed");
		ret = -1;
		goto exit_err3;
	}

	pr_info("%s: register dtb chardev OK", __func__);

	return ret;

exit_err3:
	class_remove_file(amlmmc_dtb_class, &class_attr_emmcdtb.attr);
	class_destroy(amlmmc_dtb_class);
exit_err2:
	cdev_del(&amlmmc_dtb);
exit_err1:
	unregister_chrdev_region(amlmmc_dtb_no, 1);
exit_err:
	return ret;
}

/*
 * Checks that a normal transfer didn't have any errors
 */
static int mmc_check_result(struct mmc_request *mrq)
{
	int ret;

	WARN_ON(!mrq || !mrq->cmd || !mrq->data);

	ret = 0;

	if (!ret && mrq->cmd->error)
		ret = mrq->cmd->error;
	if (!ret && mrq->data->error)
		ret = mrq->data->error;
	if (!ret && mrq->stop && mrq->stop->error)
		ret = mrq->stop->error;
	if (!ret && mrq->data->bytes_xfered !=
			mrq->data->blocks * mrq->data->blksz)
		ret = RESULT_FAIL;

	if (ret == -EINVAL)
		ret = RESULT_UNSUP_HOST;

	return ret;
}

static void mmc_prepare_mrq(struct mmc_card *card,
			    struct mmc_request *mrq, struct scatterlist *sg,
			    unsigned int sg_len, unsigned int dev_addr,
			    unsigned int blocks,
			    unsigned int blksz, int write)
{
	WARN_ON(!mrq || !mrq->cmd || !mrq->data || !mrq->stop);

	if (blocks > 1) {
		mrq->cmd->opcode = write ?
			MMC_WRITE_MULTIPLE_BLOCK : MMC_READ_MULTIPLE_BLOCK;
	} else {
		mrq->cmd->opcode = write ?
			MMC_WRITE_BLOCK : MMC_READ_SINGLE_BLOCK;
	}

	mrq->cmd->arg = dev_addr;
	if (!mmc_card_is_blockaddr(card))
		mrq->cmd->arg <<= 9;

	mrq->cmd->flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	if (blocks == 1) {
		mrq->stop = NULL;
	} else {
		mrq->stop->opcode = MMC_STOP_TRANSMISSION;
		mrq->stop->arg = 0;
		mrq->stop->flags = MMC_RSP_R1B | MMC_CMD_AC;
	}

	mrq->data->blksz = blksz;
	mrq->data->blocks = blocks;
	mrq->data->flags = write ? MMC_DATA_WRITE : MMC_DATA_READ;
	mrq->data->sg = sg;
	mrq->data->sg_len = sg_len;

	mmc_set_data_timeout(mrq->data, card);
}

unsigned int mmc_capacity(struct mmc_card *card)
{
	if (!mmc_card_sd(card) && mmc_card_is_blockaddr(card))
		return card->ext_csd.sectors;
	else
		return card->csd.capacity << (card->csd.read_blkbits - 9);
}

static int mmc_transfer(struct mmc_card *card, unsigned int dev_addr,
			unsigned int blocks, void *buf, int write)
{
	u8 original_part_config;
	u8 user_part_number = 0;
	u8 cur_part_number;
	bool switch_partition = false;
	unsigned int size;
	struct scatterlist sg;
	struct mmc_request mrq = {0};
	struct mmc_command cmd = {0};
	struct mmc_command stop = {0};
	struct mmc_data data = {0};
	int ret;

	cur_part_number = card->ext_csd.part_config
		& EXT_CSD_PART_CONFIG_ACC_MASK;
	if (cur_part_number != user_part_number) {
		switch_partition = true;
		original_part_config = card->ext_csd.part_config;
		cur_part_number = original_part_config
			& (~EXT_CSD_PART_CONFIG_ACC_MASK);
		ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_PART_CONFIG, cur_part_number,
				 card->ext_csd.part_time);
		if (ret)
			return ret;

		card->ext_csd.part_config = cur_part_number;
	}
	if ((dev_addr + blocks) >= mmc_capacity(card)) {
		pr_info("[%s] %s range exceeds device capacity!\n",
			__func__, write ? "write" : "read");
		ret = -1;
		return ret;
	}

	size = blocks << card->csd.read_blkbits;
	sg_init_one(&sg, buf, size);

	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = &stop;

	mmc_prepare_mrq(card, &mrq, &sg, 1, dev_addr,
			blocks, 1 << card->csd.read_blkbits, write);

	mmc_wait_for_req(card->host, &mrq);

	ret = mmc_check_result(&mrq);
	if (switch_partition) {
		ret = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_PART_CONFIG, original_part_config,
				 card->ext_csd.part_time);
		if (ret)
			return ret;
		card->ext_csd.part_config = original_part_config;
	}

	return ret;
}

/*write tuning para on emmc, the offset is 0x14400*/
static int amlmmc_write_tuning_para(struct mmc_card *card,
				    unsigned int dev_addr)
{
	unsigned int size;
	struct mmc_host *mmc = card->host;
	struct meson_host *host = mmc_priv(mmc);
	struct aml_tuning_para *parameter = &host->para;
	unsigned int *buf;
	int para_size;
	int blocks;

	if (host->save_para == 0)
		return 0;

	if (parameter->update == 0)
		return 0;
	parameter->update = 0;

	para_size = sizeof(struct aml_tuning_para);
	blocks = (para_size - 1)  / 512 + 1;
	size = blocks << card->csd.read_blkbits;

	buf = kmalloc(size, GFP_KERNEL);
	memset(buf, 0, size);

	memcpy(buf, parameter, sizeof(struct aml_tuning_para));

	mmc_claim_host(card->host);
	aml_disable_mmc_cqe(card);
	mmc_transfer(card, dev_addr, blocks, buf, 1);
	aml_enable_mmc_cqe(card);
	mmc_release_host(card->host);

	kfree(buf);
	return 0;
}

int mmc_read_internal(struct mmc_card *card, unsigned int dev_addr,
		      unsigned int blocks, void *buf)
{
	return mmc_transfer(card, dev_addr, blocks, buf, 0);
}

int mmc_write_internal(struct mmc_card *card, unsigned int dev_addr,
		       unsigned int blocks, void *buf)
{
	return mmc_transfer(card, dev_addr, blocks, buf, 1);
}

static int mmc_partition_tbl_checksum_calc(struct partitions *part,
					   int part_num)
{
	int i, j;
	u32 checksum = 0, *p;

	for (i = 0; i < part_num; i++) {
		p = (u32 *)part;

		for (j = sizeof(struct partitions) / sizeof(checksum);
				j > 0; j--) {
			checksum += *p;
			p++;
		}
	}

	return checksum;
}

static int get_reserve_partition_off(struct mmc_card *card) /* byte unit */
{
	int off = -1;

	off = MMC_BOOT_PARTITION_SIZE + MMC_BOOT_PARTITION_RESERVED;

	return off;
}

int get_reserve_partition_off_from_tbl(void)
{
	int i;

	for (i = 0; i < pt_fmt->part_num; i++) {
		if (!strcmp(pt_fmt->partitions[i].name, MMC_RESERVED_NAME))
			return pt_fmt->partitions[i].offset;
	}
	return -1;
}

/* static void show_mmc_partition (struct partitions *part, int part_num)
 * {
 * int i, cnt_stuff;

 * pr_info("	name	offset	size\n");
 * pr_info("===========================\n");
 * for (i=0; i < part_num ; i++) {
 * pr_info("%4d: %s", i, part[i].name);
 * cnt_stuff = sizeof(part[i].name) - strlen(part[i].name);
 * // something is wrong
 * if (cnt_stuff < 0)
 * cnt_stuff = 0;
 * cnt_stuff += 2;
 * while (cnt_stuff--) {
 * pr_info(" ");
 * }
 * pr_info("%18llx%18llx\n", part[i].offset, part[i].size);
 * }
 * }
 */

static int mmc_read_partition_tbl(struct mmc_card *card,
				  struct mmc_partitions_fmt *pt_fmt)
{
	int ret = 0, start_blk, size, blk_cnt;
	int bit = card->csd.read_blkbits;
	int blk_size = 1 << bit; /* size of a block */
	char *buf, *dst;

	buf = kmalloc(blk_size, GFP_KERNEL);
	if (!buf) {
		/*	pr_info("malloc failed for buffer!\n");*/
		ret = -ENOMEM;
		goto exit_err;
	}
	memset(pt_fmt, 0, sizeof(struct mmc_partitions_fmt));
	memset(buf, 0, blk_size);
	start_blk = get_reserve_partition_off(card);
	if (start_blk < 0) {
		ret = -EINVAL;
		goto exit_err;
	}
	start_blk >>= bit;
	size = sizeof(struct mmc_partitions_fmt);
	dst = (char *)pt_fmt;
	if (size >= blk_size) {
		blk_cnt = size >> bit;
		ret = mmc_read_internal(card, start_blk, blk_cnt, dst);
		if (ret) { /* error */
			goto exit_err;
		}
		start_blk += blk_cnt;
		dst += blk_cnt << bit;
		size -= blk_cnt << bit;
	}
	if (size > 0) { /* the last block */
		ret = mmc_read_internal(card, start_blk, 1, buf);
		if (ret)
			goto exit_err;
		memcpy(dst, buf, size);
	}
	/* pr_info("Partition table stored in eMMC/TSD:\n"); */
	/* pr_info("magic: %s, version: %s, checksum=%#x\n", */
	/* pt_fmt->magic, pt_fmt->version, pt_fmt->checksum); */
	/* show_mmc_partition(pt_fmt->partitions, pt_fmt->part_num); */

	if ((strncmp(pt_fmt->magic, MMC_PARTITIONS_MAGIC,
		     sizeof(pt_fmt->magic)) == 0) && pt_fmt->part_num > 0 &&
			pt_fmt->part_num <= MAX_MMC_PART_NUM &&
			pt_fmt->checksum == mmc_partition_tbl_checksum_calc
			(pt_fmt->partitions, pt_fmt->part_num)) {
		ret = 0; /* everything is OK now */

	} else {
		if (strncmp(pt_fmt->magic, MMC_PARTITIONS_MAGIC,
			    sizeof(pt_fmt->magic)) != 0) {
			pr_info("magic error: %s\n", pt_fmt->magic);
		} else if ((pt_fmt->part_num < 0) ||
			    (pt_fmt->part_num > MAX_MMC_PART_NUM)) {
			pr_info("partition number error: %d\n",
				pt_fmt->part_num);
		} else {
			pr_info("checksum error: pt_fmt->checksum=%d,calc_result=%d\n",
				pt_fmt->checksum,
				mmc_partition_tbl_checksum_calc
				(pt_fmt->partitions, pt_fmt->part_num));
		}

		pr_info("[%s]: partition verified error\n", __func__);
		ret = -1; /* the partition information is invalid */
	}

exit_err:
	kfree(buf);

	pr_info("[%s] mmc read partition %s!\n",
		__func__, (ret == 0) ? "OK" : "ERROR");

	return ret;
}

/* This function is copy and modified from kernel function add_partition() */
static struct hd_struct *add_emmc_each_part(struct gendisk *disk, int partno,
					    sector_t start, sector_t len,
					    int flags, char *pname)
{
	struct hd_struct *p;
	dev_t devt = MKDEV(0, 0);
	struct device *ddev = disk_to_dev(disk);
	struct device *pdev;
	struct disk_part_tbl *ptbl;
	const char *dname;
	int err;

	err = disk_expand_part_tbl(disk, partno);
	if (err)
		return ERR_PTR(err);
	ptbl = disk->part_tbl;

	if (ptbl->part[partno])
		return ERR_PTR(-EBUSY);

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return ERR_PTR(-EBUSY);

	if (!init_part_stats(p)) {
		err = -ENOMEM;
		goto out_free;
	}
	seqcount_init(&p->nr_sects_seq);
	pdev = part_to_dev(p);

	p->start_sect = start;
	p->alignment_offset =
		queue_limit_alignment_offset(&disk->queue->limits, start);
	p->discard_alignment =
		queue_limit_discard_alignment(&disk->queue->limits, start);
	p->nr_sects = len;
	p->partno = partno;
	p->policy = get_disk_ro(disk);
	p->info = alloc_part_info(disk);
	sprintf(p->info->volname, "%s", pname);

	dname = dev_name(ddev);
	dev_set_name(pdev, "%s", pname);

	device_initialize(pdev);
	pdev->class = &block_class;
	pdev->type = &part_type;
	pdev->parent = ddev;

	err = blk_alloc_devt(p, &devt);
	if (err)
		goto out_free_info;
	pdev->devt = devt;

	/* delay uevent until 'holders' subdir is created */
	dev_set_uevent_suppress(pdev, 1);
	err = device_add(pdev);
	if (err)
		goto out_put;

	err = -ENOMEM;
	p->holder_dir = kobject_create_and_add("holders", &pdev->kobj);
	if (!p->holder_dir)
		goto out_del;

	dev_set_uevent_suppress(pdev, 0);

	/* everything is up and running, commence */
	rcu_assign_pointer(ptbl->part[partno], p);

	/* suppress uevent if the disk suppresses it */
	if (!dev_get_uevent_suppress(ddev))
		kobject_uevent(&pdev->kobj, KOBJ_ADD);

	hd_ref_init(p);
	return p;

out_free_info:
	free_part_info(p);
out_free:
	kfree(p);
	return ERR_PTR(err);
out_del:
	kobject_put(p->holder_dir);
	device_del(pdev);
out_put:
	put_device(pdev);
	blk_free_devt(devt);
	return ERR_PTR(err);
}

static inline int card_proc_info(struct seq_file *m, char *dev_name, int i)
{
	struct partitions *this = &pt_fmt->partitions[i];

	if (i >= pt_fmt->part_num)
		return 0;

	seq_printf(m, "%s%02d: %9llx %9x \"%s\"\n", dev_name,
		   i + 1, (unsigned long long)this->size,
		   512 * 1024, this->name);
	return 0;
}

static int card_proc_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "dev:	size   erasesize  name\n");
	for (i = 0; i < 16; i++)
		card_proc_info(m, "inand", i);

	return 0;
}

static int card_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, card_proc_show, NULL);
}

static const struct file_operations card_proc_fops = {
	.open = card_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int add_emmc_partition(struct gendisk *disk,
			      struct mmc_partitions_fmt *pt_fmt)
{
	unsigned int i;
	struct hd_struct *ret = NULL;
	u64 offset, size, cap;
	struct partitions *pp;
	struct proc_dir_entry *proc_card;

	pr_info("%s\n", __func__);

	cap = get_capacity(disk); /* unit:512 bytes */
	for (i = 0; i < pt_fmt->part_num; i++) {
		pp = &pt_fmt->partitions[i];
		offset = pp->offset >> 9; /* unit:512 bytes */
		size = pp->size >> 9; /* unit:512 bytes */
		if ((offset + size) <= cap) {
			ret = add_emmc_each_part(disk, 1 + i, offset,
						 size, 0, pp->name);

			pr_info("[%sp%02d] %20s  offset 0x%012llx, size 0x%012llx %s\n",
				disk->disk_name, 1 + i,
				pp->name, offset << 9,
				size << 9, IS_ERR(ret) ? "add fail" : "");
		} else {
			pr_info("[%s] %s: partition exceeds device capacity:\n",
				__func__, disk->disk_name);

			pr_info("%20s	offset 0x%012llx, size 0x%012llx\n",
				pp->name, offset << 9, size << 9);

			break;
		}
	}
	/* create /proc/inand */

	proc_card = proc_create("inand", 0444, NULL, &card_proc_fops);
	if (!proc_card)
		pr_info("[%s] create /proc/inand fail.\n", __func__);

	/* create /proc/ntd */
	if (!proc_create("ntd", 0444, NULL, &card_proc_fops))
		pr_info("[%s] create /proc/ntd fail.\n", __func__);

	return 0;
}

static int is_card_emmc(struct mmc_card *card)
{
	struct mmc_host *mmc = card->host;
	struct meson_host *host = mmc_priv(mmc);

	/* emmc port, so it must be an eMMC or TSD */
	if (aml_card_type_mmc(host))
		return 0;
	else
		return 1;
	/*return mmc->is_emmc_port;*/
}

static ssize_t emmc_version_get(struct class *class,
				struct class_attribute *attr, char *buf)
{
	int num = 0;

	return sprintf(buf, "%d", num);
}

static void show_partition_table(struct partitions *table)
{
	int i = 0;
	struct partitions *par_table = NULL;

	pr_info("show partition table:\n");
	for (i = 0; i < MAX_MMC_PART_NUM; i++) {
		par_table = &table[i];
		if (par_table->size == -1)
			pr_info("part: %d, name : %10s, size : %-4s mask_flag %d\n",
				i, par_table->name, "end",
				par_table->mask_flags);
		else
			pr_info("part: %d, name : %10s, size : %-4llx  mask_flag %d\n",
				i, par_table->name, par_table->size,
				par_table->mask_flags);
	}
}

static ssize_t emmc_part_table_get(struct class *class,
				   struct class_attribute *attr, char *buf)
{
	struct partitions *part_table = NULL;
	struct partitions *tmp_table = NULL;
	int i = 0, part_num = 0;

	tmp_table = pt_fmt->partitions;
	part_table = kmalloc_array(MAX_MMC_PART_NUM,
				   sizeof(struct partitions), GFP_KERNEL);

	if (!part_table) {
		pr_info("[%s] malloc failed for  part_table!\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < MAX_MMC_PART_NUM; i++) {
		if (tmp_table[i].mask_flags == STORE_CODE) {
			strncpy(part_table[part_num].name,
				tmp_table[i].name,
				MAX_MMC_PART_NAME_LEN);

			part_table[part_num].size = tmp_table[i].size;
			part_table[part_num].offset = tmp_table[i].offset;

			part_table[part_num].mask_flags =
				tmp_table[i].mask_flags;
			part_num++;
		}
	}
	for (i = 0; i < MAX_MMC_PART_NUM; i++) {
		if (tmp_table[i].mask_flags == STORE_CACHE) {
			strncpy(part_table[part_num].name,
				tmp_table[i].name,
				MAX_MMC_PART_NAME_LEN);

			part_table[part_num].size = tmp_table[i].size;
			part_table[part_num].offset = tmp_table[i].offset;

			part_table[part_num].mask_flags =
				tmp_table[i].mask_flags;

			part_num++;
		}
	}
	for (i = 0; i < MAX_MMC_PART_NUM; i++) {
		if (tmp_table[i].mask_flags == STORE_DATA) {
			strncpy(part_table[part_num].name,
				tmp_table[i].name,
				MAX_MMC_PART_NAME_LEN);

			part_table[part_num].size = tmp_table[i].size;
			part_table[part_num].offset = tmp_table[i].offset;
			part_table[part_num].mask_flags =
				tmp_table[i].mask_flags;

			if (!strncmp(part_table[part_num].name, "data",
				     MAX_MMC_PART_NAME_LEN))
				/* last part size is FULL */
				part_table[part_num].size = -1;
			part_num++;
		}
	}

	show_partition_table(part_table);
	memcpy(buf, part_table, MAX_MMC_PART_NUM * sizeof(struct partitions));

	kfree(part_table);
	part_table = NULL;

	return MAX_MMC_PART_NUM * sizeof(struct partitions);
}

static int store_device = -1;
static ssize_t store_device_flag_get(struct class *class,
				     struct class_attribute *attr, char *buf)
{
	if (store_device == -1) {
		pr_info("[%s]  get store device flag something wrong !\n",
			__func__);
	}

	return sprintf(buf, "%d", store_device);
}

static ssize_t get_bootloader_offset(struct class *class,
				     struct class_attribute *attr, char *buf)
{
	int offset = 0;

	offset = 512;
	return sprintf(buf, "%d", offset);
}

/*
 * extern u32 cd_irq_cnt[2];
 *
 *static ssize_t get_cdirq_cnt(struct class *class,
 *	struct class_attribute *attr, char *buf)
 *{
 *	return sprintf(buf, "in:%d, out:%d\n", cd_irq_cnt[1], cd_irq_cnt[0]);
 *}
 */

static struct class_attribute aml_version =
__ATTR(version, 0444, emmc_version_get, NULL);
static struct class_attribute aml_part_table =
__ATTR(part_table, 0444, emmc_part_table_get, NULL);
static struct class_attribute aml_store_device =
__ATTR(store_device, 0444, store_device_flag_get, NULL);
static struct class_attribute bootloader_offset =
__ATTR(bl_off_bytes, 0444, get_bootloader_offset, NULL);

static char slot_str[8] = "0";
static int active_slot_num(char *s)
{
	if (s)
		sprintf(slot_str, "%s", s);
	pr_info("**********slot_str: %s\n", slot_str);
	return 0;
}
__setup("androidboot.slot_suffix=", active_slot_num);

/* for irq cd dbg */
/* static struct class_attribute cd_irq_cnt_ =
 *	__ATTR(cdirq_cnt, S_IRUGO, get_cdirq_cnt, NULL);
 */

int add_fake_boot_partition(struct gendisk *disk, char *name, int idx)
{
	u64 boot_size = (u64)get_capacity(disk) - 1;
	char fake_name[80];
	int offset = 1;
	struct hd_struct *ret = NULL;
	struct efuse_obj_field_t efuse_field;
	u8 buff[32];
	u32 bufflen = sizeof(buff);
	char *efuse_field_name = "FEAT_DISABLE_EMMC_USER";
	u32 rc = 0;

	idx ^= 1;
	sprintf(fake_name, name, idx);

	memset(&buff[0], 0, sizeof(buff));
	memset(&efuse_field, 0, sizeof(efuse_field));

	rc = efuse_obj_read(EFUSE_OBJ_EFUSE_DATA, efuse_field_name, buff, &bufflen);

	if (rc == EFUSE_OBJ_SUCCESS) {
		memset(&efuse_field, 0, sizeof(efuse_field));
		strncpy(efuse_field.name, efuse_field_name, sizeof(efuse_field.name));
		memcpy(efuse_field.data, buff, bufflen);
		efuse_field.size = bufflen;
	} else {
		pr_err("Error getting eFUSE DATA: %d\n", rc);
	}

	if (*efuse_field.data == 0) {
		ret = add_emmc_each_part(disk, 1, offset, boot_size, 0, fake_name);
		if (IS_ERR(ret))
			pr_info("%s added failed\n", fake_name);
	} else {
		if (strcmp(slot_str, "_a") == 0) {
			if (idx == 1) {
				ret = add_emmc_each_part(disk, 1, offset,
				boot_size, 0, "bootloader0");
				if (IS_ERR(ret))
					pr_info("bootloader_nocs_a added failed\n");
			} else if (idx == 0) {
				ret = add_emmc_each_part(disk, 1, offset,
				boot_size, 0, "bootloader1");
				if (IS_ERR(ret))
					pr_info("bootloader_nocs_a added failed\n");
			}
		} else {
			if (idx == 0) {
				ret = add_emmc_each_part(disk, 1, offset,
				boot_size, 0, "bootloader0");
				if (IS_ERR(ret))
					pr_info("bootloader_nocs_a added failed\n");
			} else if (idx == 1) {
				ret = add_emmc_each_part(disk, 1, offset,
				boot_size, 0, "bootloader1");
				if (IS_ERR(ret))
					pr_info("bootloader_nocs_a added failed\n");
			}
		}
	}

	return 0;
}

int aml_emmc_partition_ops(struct mmc_card *card, struct gendisk *disk)
{
	int ret = 0;
	struct disk_part_iter piter;
	struct hd_struct *part;
	struct class *aml_store_class = NULL;
	struct gpt_header *gpt_h = NULL;
	unsigned char *buffer = NULL;

	pr_info("Enter %s\n", __func__);
	if (is_card_emmc(card)) /* not emmc, nothing to do */
		return 0;

	buffer = kmalloc(512, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	mmc_claim_host(card->host);
	aml_disable_mmc_cqe(card);

	/*self adapting*/
	ret = mmc_read_internal(card, 1, 1, buffer);
	if (ret) {
		pr_err("%s: save dtb error", __func__);
		kfree(buffer);
		goto out;
	}

	gpt_h = (struct gpt_header *)buffer;

	pt_fmt = kmalloc(sizeof(*pt_fmt), GFP_KERNEL);
	if (!pt_fmt) {
		/*	pr_info(
		 *	"[%s] malloc failed for struct mmc_partitions_fmt!\n",
		 *	__func__);
		 */
		aml_enable_mmc_cqe(card);
		mmc_release_host(card->host);
		return -ENOMEM;
	}

	if (le64_to_cpu(gpt_h->signature) == GPT_HEADER_SIGNATURE) {
		kfree(buffer);
		/**/
		ret = mmc_read_partition_tbl(card, pt_fmt);
		aml_enable_mmc_cqe(card);
		mmc_release_host(card->host);
		if (ret == 0)
			ret = emmc_key_init(card);
		if (ret)
			goto out;
		return 0;
	}

	kfree(buffer);

	disk_part_iter_init(&piter, disk, DISK_PITER_INCL_EMPTY);

	while ((part = disk_part_iter_next(&piter))) {
		pr_info("Delete invalid mbr partition part %p, part->partno %d\n",
			part, part->partno);
		delete_partition(disk, part->partno);
	}
	disk_part_iter_exit(&piter);

	ret = mmc_read_partition_tbl(card, pt_fmt);
	if (ret == 0) { /* ok */
		ret = add_emmc_partition(disk, pt_fmt);
	}
	aml_enable_mmc_cqe(card);
	mmc_release_host(card->host);

	if (ret == 0) /* ok */
		ret = emmc_key_init(card);
	if (ret)
		goto out;

	amlmmc_dtb_init(card);
	amlmmc_write_tuning_para(card, MMC_TUNING_OFFSET);

	aml_store_class = class_create(THIS_MODULE, "aml_store");
	if (IS_ERR(aml_store_class)) {
		pr_info("[%s] create aml_store_class class fail.\n", __func__);
		ret = -1;
		goto out;
	}

	ret = class_create_file(aml_store_class, &aml_version);
	if (ret) {
		pr_info("[%s] can't create aml_store_class file .\n", __func__);
		goto out_class1;
	}
	ret = class_create_file(aml_store_class, &aml_part_table);
	if (ret) {
		pr_info("[%s] can't create aml_store_class file .\n", __func__);
		goto out_class2;
	}
	ret = class_create_file(aml_store_class, &aml_store_device);
	if (ret) {
		pr_info("[%s] can't create aml_store_class file .\n", __func__);
		goto out_class3;
	}

	ret = class_create_file(aml_store_class, &bootloader_offset);
	if (ret) {
		pr_info("[%s] can't create aml_store_class file .\n", __func__);
		goto out_class3;
	}

	/*ret = class_create_file(aml_store_class, &cd_irq_cnt_);
	 *if (ret) {
	 *	pr_info("[%s] can't create aml_store_class file .\n", __func__);
	 *	goto out_class3;
	 *}
	 */
	pr_info("Exit %s %s.\n", __func__, (ret == 0) ? "OK" : "ERROR");
	return ret;

out_class3:
	class_remove_file(aml_store_class, &aml_part_table);
out_class2:
	class_remove_file(aml_store_class, &aml_version);
out_class1:
	class_destroy(aml_store_class);
out:
	kfree(pt_fmt);
	return ret;
}
