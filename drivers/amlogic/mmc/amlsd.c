/*
 * drivers/amlogic/mmc/amlsd.c
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

#include <linux/stddef.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
/* #include <linux/earlysuspend.h> */
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/clk.h>
#include <linux/mmc/host.h>
#include <linux/mmc/core.h>
#include <linux/io.h>
#include <linux/semaphore.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/card.h>
#include <linux/genhd.h>
#include <linux/slab.h>
#include <linux/amlogic/sd.h>
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>
/* #include <asm/outercache.h> */

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/jtag.h>

#include <linux/clk.h>
#include <linux/highmem.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include "amlsd.h"

#ifndef CONFIG_ARM64
#include <asm/opcodes-sec.h>
#else
#include <asm/psci.h>
#endif

/*====================================================================*/
/* Support for /proc/mtd */

/* static int wifi_setup_dt(void); */

static struct proc_dir_entry *proc_card;
static struct mtd_partition *card_table[16];
const u8 tuning_blk_pattern_4bit[64] = {
	0xff, 0x0f, 0xff, 0x00, 0xff, 0xcc, 0xc3, 0xcc,
	0xc3, 0x3c, 0xcc, 0xff, 0xfe, 0xff, 0xfe, 0xef,
	0xff, 0xdf, 0xff, 0xdd, 0xff, 0xfb, 0xff, 0xfb,
	0xbf, 0xff, 0x7f, 0xff, 0x77, 0xf7, 0xbd, 0xef,
	0xff, 0xf0, 0xff, 0xf0, 0x0f, 0xfc, 0xcc, 0x3c,
	0xcc, 0x33, 0xcc, 0xcf, 0xff, 0xef, 0xff, 0xee,
	0xff, 0xfd, 0xff, 0xfd, 0xdf, 0xff, 0xbf, 0xff,
	0xbb, 0xff, 0xf7, 0xff, 0xf7, 0x7f, 0x7b, 0xde,
};
const u8 tuning_blk_pattern_8bit[128] = {
	0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00,
	0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc, 0xcc,
	0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xff,
	0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee, 0xff,
	0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd, 0xdd,
	0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff, 0xbb,
	0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff, 0xff,
	0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee, 0xff,
	0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00,
	0x00, 0xff, 0xff, 0xcc, 0xcc, 0xcc, 0x33, 0xcc,
	0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc, 0xcc, 0xff,
	0xff, 0xff, 0xee, 0xff, 0xff, 0xff, 0xee, 0xee,
	0xff, 0xff, 0xff, 0xdd, 0xff, 0xff, 0xff, 0xdd,
	0xdd, 0xff, 0xff, 0xff, 0xbb, 0xff, 0xff, 0xff,
	0xbb, 0xbb, 0xff, 0xff, 0xff, 0x77, 0xff, 0xff,
	0xff, 0x77, 0x77, 0xff, 0x77, 0xbb, 0xdd, 0xee,
};

void aml_mmc_ver_msg_show(void)
{
	static bool one_time_flag;

	if (!one_time_flag) {
		pr_info("mmc driver version: %d.%02d, %s\n",
			AML_MMC_MAJOR_VERSION, AML_MMC_MINOR_VERSION,
				AML_MMC_VER_MESSAGE);

	one_time_flag = true;
	}
}

static inline int card_proc_info(struct seq_file *m, char *dev_name, int i)
{
	struct mtd_partition *this = card_table[i];

	if (!this)
		return 0;

	return seq_printf(m, "%s%d: %8.8llx %8.8x \"%s\"\n", dev_name,
			i+1, (unsigned long long)this->size,
		       512*1024, this->name);
}

static int card_proc_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "dev:    size   erasesize  name\n");
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
	.release = seq_release,
};

/**
 * add_card_partition : add card partition , refer to
 * board-****.c  inand_partition_info[]
 * @disk: add partitions in which disk
 * @part: partition table
 * @nr_part: partition numbers
 */
int add_part_table(struct mtd_partition *part, unsigned int nr_part)
{
	unsigned int i;
	uint64_t cur_offset = 0;
	uint64_t offset, size;

	if (!part)
		return 0;

	for (i = 0; i < nr_part; i++) {
		offset = part[i].offset>>9;
		size = part[i].size>>9;
		if (part[i].offset == MTDPART_OFS_APPEND)
			offset = cur_offset;
		cur_offset = offset + size;

		card_table[i] = &part[i];
		card_table[i]->offset = offset<<9;
		card_table[i]->size = size<<9;
		card_table[i]->name = part[i].name;
	}

	if (!proc_card)
		proc_card = proc_create("inand", 0, NULL, &card_proc_fops);

	return 0;
}

struct hd_struct *add_emmc_each_part(struct gendisk *disk, int partno,
				sector_t start, sector_t len, int flags,
				char *pname)
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
	pdev = part_to_dev(p);

	p->start_sect = start;
	p->alignment_offset =
		queue_limit_alignment_offset(&disk->queue->limits, start);
	p->discard_alignment =
		queue_limit_discard_alignment(&disk->queue->limits, start);
	p->nr_sects = len;
	p->partno = partno;
	p->policy = get_disk_ro(disk);

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

/**
 * add_emmc_partition : add emmc partition , refer to
 * board-****.c  inand_partition_info[]
 * @disk: add partitions in which disk
 * @part: partition table
 * @nr_part: partition numbers
 */
EXPORT_SYMBOL(add_emmc_partition);
int add_emmc_partition(struct gendisk *disk)
{
	unsigned int i;
	struct hd_struct *ret = NULL;
	uint64_t offset, size;

	pr_info(KERN_INFO "add_emmc_partition\n");
	if (!proc_card) {
		pr_info(KERN_INFO "proc_nand NULL\n");
		return 0;
	}

	for (i = 0; i < CONFIG_MMC_BLOCK_MINORS; i++) {
		if (!card_table[i])
			break;
		offset = card_table[i]->offset>>9;
		if (card_table[i]->size == MTDPART_SIZ_FULL) {
			size = get_capacity(disk) - offset;
			card_table[i]->size = size<<9;
		} else{
			size = card_table[i]->size>>9;
		}

		ret = add_emmc_each_part(disk, 1+i, offset, size, 0,
			(char *) card_table[i]->name);
		pr_info("[%sp%d] %20s  offset 0x%012llx, len 0x%012llx %s\n",
				disk->disk_name, 1+i, card_table[i]->name,
				offset<<9, size<<9, IS_ERR(ret) ?
				 "add fail":"");
	}
	return 0;
}


/*-----------sg copy buffer------------*/

/**
 * aml_sg_miter_stop - stop mapping iteration for amlogic,
 * We don't disable irq in this function
 */
static void aml_sg_miter_stop(struct sg_mapping_iter *miter)
{

	WARN_ON(miter->consumed > miter->length);

	/* drop resources from the last iteration */
	if (miter->addr) {
		miter->__offset += miter->consumed;
		miter->__remaining -= miter->consumed;

		if (miter->__flags & SG_MITER_TO_SG)
			flush_kernel_dcache_page(miter->page);

		if (miter->__flags & SG_MITER_ATOMIC) {
			WARN_ON_ONCE(preemptible());
			kunmap_atomic(miter->addr);
		} else
			kunmap(miter->page);


		miter->page = NULL;
		miter->addr = NULL;
		miter->length = 0;
		miter->consumed = 0;
	}
}

/**
 * aml_sg_miter_next - proceed mapping iterator to the next mapping for amlogic,
 * We don't disable irq in this function
 */
static bool aml_sg_miter_next(struct sg_mapping_iter *miter)
{
	unsigned long flags;

	sg_miter_stop(miter);

	/*
	 * Get to the next page if necessary.
	 * __remaining, __offset is adjusted by sg_miter_stop
	 */
	if (!miter->__remaining) {
		struct scatterlist *sg;
		unsigned long pgoffset;

		if (!__sg_page_iter_next(&miter->piter))
			return false;

		sg = miter->piter.sg;
		pgoffset = miter->piter.sg_pgoffset;

		miter->__offset = pgoffset ? 0 : sg->offset;
		miter->__remaining = sg->offset + sg->length -
				(pgoffset << PAGE_SHIFT) - miter->__offset;
		miter->__remaining = min_t(unsigned long, miter->__remaining,
					   PAGE_SIZE - miter->__offset);
	}
	miter->page = sg_page_iter_page(&miter->piter);
	miter->consumed = miter->length = miter->__remaining;

	if (miter->__flags & SG_MITER_ATOMIC) {
		/*pr_info(KERN_DEBUG "AML_SDHC miter_next highmem\n"); */
		local_irq_save(flags);
		miter->addr = kmap_atomic(miter->page) + miter->__offset;
		local_irq_restore(flags);
	} else
		miter->addr = kmap(miter->page) + miter->__offset;
	return true;
}

/**
 * aml_sg_copy_buffer - Copy data between a linear buffer and an SG list  for amlogic,
 * We don't disable irq in this function
 **/
EXPORT_SYMBOL(aml_sg_copy_buffer);
size_t aml_sg_copy_buffer(struct scatterlist *sgl, unsigned int nents,
			     void *buf, size_t buflen, int to_buffer)
{
	unsigned int offset = 0;
	struct sg_mapping_iter miter;
	unsigned int sg_flags = SG_MITER_ATOMIC;
	unsigned long flags;

	if (to_buffer)
		sg_flags |= SG_MITER_FROM_SG;
	else
		sg_flags |= SG_MITER_TO_SG;

	sg_miter_start(&miter, sgl, nents, sg_flags);
	local_irq_save(flags);

	while (aml_sg_miter_next(&miter) && offset < buflen) {
		unsigned int len;

		len = min(miter.length, buflen - offset);

		if (to_buffer)
			memcpy(buf + offset, miter.addr, len);
		else
			memcpy(miter.addr, buf + offset, len);

		offset += len;
	}

	aml_sg_miter_stop(&miter);
	local_irq_restore(flags);

	return offset;

}


/*-------------------eMMC/tSD-------------------*/

int storage_flag = 0;
int  __init get_storage_device(char *str)
{
	int ret;
	ret = kstrtoul(str, 16, (long *)&storage_flag);

	pr_info("[%s] storage_flag=%d\n", __func__, storage_flag);
	if ((storage_flag != EMMC_BOOT_FLAG)
		&& (storage_flag != SPI_EMMC_FLAG)) {
		pr_info(
		"[%s] storage NOT relate to eMMC,"" storage_flag=%d\n",
		__func__, storage_flag);
	}

	return ret;
}
early_param("storage", get_storage_device);

bool is_emmc_exist(struct amlsd_host *host) /* is eMMC/tSD exist */
{
	print_tmp("host->storage_flag=%d, POR_BOOT_VALUE=%d\n",
	host->storage_flag, POR_BOOT_VALUE);

	if ((host->storage_flag == EMMC_BOOT_FLAG)
		|| (host->storage_flag == SPI_EMMC_FLAG)
		|| (((host->storage_flag == 0)  || (host->storage_flag == -1))
					&& (POR_EMMC_BOOT()
					|| POR_SPI_BOOT()))) {
		return true;
	}

	return false;
}

void aml_sd_emmc_print_reg(struct amlsd_host *host)
{
	return;
}
static int aml_sd_emmc_regs_show(struct seq_file *s, void *v)
{
	struct mmc_host *mmc = (struct mmc_host *)s->private;
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	aml_sd_emmc_print_reg(host);
	return 0;
}
static int aml_sd_emmc_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, aml_sd_emmc_regs_show, inode->i_private);
}
static const struct file_operations aml_sd_emmc_regs_fops = {
	.owner		= THIS_MODULE,
	.open		= aml_sd_emmc_regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
/*----sdhc----*/

void aml_sdhc_print_reg_(u32 *buf)
{
	pr_info("***********SDHC_REGS***********\n");
	pr_info("SDHC_ARGU: 0x%08x\n", buf[SDHC_ARGU/4]);
	pr_info("SDHC_SEND: 0x%08x\n", buf[SDHC_SEND/4]);
	pr_info("SDHC_CTRL: 0x%08x\n", buf[SDHC_CTRL/4]);
	pr_info("SDHC_STAT: 0x%08x\n", buf[SDHC_STAT/4]);
	pr_info("SDHC_CLKC: 0x%08x\n", buf[SDHC_CLKC/4]);
	pr_info("SDHC_ADDR: 0x%08x\n", buf[SDHC_ADDR/4]);
	pr_info("SDHC_PDMA: 0x%08x\n", buf[SDHC_PDMA/4]);
	pr_info("SDHC_MISC: 0x%08x\n", buf[SDHC_MISC/4]);
	pr_info("SDHC_DATA: 0x%08x\n", buf[SDHC_DATA/4]);
	pr_info("SDHC_ICTL: 0x%08x\n", buf[SDHC_ICTL/4]);
	pr_info("SDHC_ISTA: 0x%08x\n", buf[SDHC_ISTA/4]);
	pr_info("SDHC_SRST: 0x%08x\n", buf[SDHC_SRST/4]);
	pr_info("SDHC_ESTA: 0x%08x\n", buf[SDHC_ESTA/4]);
	pr_info("SDHC_ENHC: 0x%08x\n", buf[SDHC_ENHC/4]);
	pr_info("SDHC_CLK2: 0x%08x\n", buf[SDHC_CLK2/4]);
}

void aml_sdhc_print_reg(struct amlsd_host *host)
{
	u32 buf[16];
	memcpy_fromio(buf, host->base, 0x3C);
	aml_sdhc_print_reg_(buf);
}

static int aml_sdhc_regs_show(struct seq_file *s, void *v)
{
	struct mmc_host *mmc = (struct mmc_host *)s->private;
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;

	u32 buf[16];
	memcpy_fromio(buf, host->base, 0x30);

	seq_puts(s, "***********SDHC_REGS***********\n");
	seq_printf(s, "SDHC_ARGU: 0x%x\n", buf[SDHC_ARGU/4]);
	seq_printf(s, "SDHC_SEND: 0x%x\n", buf[SDHC_SEND/4]);
	seq_printf(s, "SDHC_CTRL: 0x%x\n", buf[SDHC_CTRL/4]);
	seq_printf(s, "SDHC_STAT: 0x%x\n", buf[SDHC_STAT/4]);
	seq_printf(s, "SDHC_CLKC: 0x%x\n", buf[SDHC_CLKC/4]);
	seq_printf(s, "SDHC_ADDR: 0x%x\n", buf[SDHC_ADDR/4]);
	seq_printf(s, "SDHC_PDMA: 0x%x\n", buf[SDHC_PDMA/4]);
	seq_printf(s, "SDHC_MISC: 0x%x\n", buf[SDHC_MISC/4]);
	seq_printf(s, "SDHC_DATA: 0x%x\n", buf[SDHC_DATA/4]);
	seq_printf(s, "SDHC_ICTL: 0x%x\n", buf[SDHC_ICTL/4]);
	seq_printf(s, "SDHC_ISTA: 0x%x\n", buf[SDHC_ISTA/4]);
	seq_printf(s, "SDHC_SRST: 0x%x\n", buf[SDHC_SRST/4]);

	return 0;
}

static int aml_sdhc_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, aml_sdhc_regs_show, inode->i_private);
}

static const struct file_operations aml_sdhc_regs_fops = {
	.owner		= THIS_MODULE,
	.open		= aml_sdhc_regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*----sdio----*/

void aml_sdio_print_reg(struct amlsd_host *host)
{
	u32 buf[16];
	memcpy_fromio(buf, host->base, 0x20);

	pr_info("***********SDIO_REGS***********\n");
	pr_info("SDIO_ARGU: 0x%x\n", buf[SDIO_ARGU/4]);
	pr_info("SDIO_SEND: 0x%x\n", buf[SDIO_SEND/4]);
	pr_info("SDIO_CONF: 0x%x\n", buf[SDIO_CONF/4]);
	pr_info("SDIO_IRQS: 0x%x\n", buf[SDIO_IRQS/4]);
	pr_info("SDIO_IRQC: 0x%x\n", buf[SDIO_IRQC/4]);
	pr_info("SDIO_MULT: 0x%x\n", buf[SDIO_MULT/4]);
	pr_info("SDIO_ADDR: 0x%x\n", buf[SDIO_ADDR/4]);
	pr_info("SDIO_EXT: 0x%x\n", buf[SDIO_EXT/4]);
}

static int aml_sdio_regs_show(struct seq_file *s, void *v)
{
	struct mmc_host *mmc = (struct mmc_host *)s->private;
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	u32 buf[16];
	memcpy_fromio(buf, host->base, 0x20);

	seq_puts(s, "***********SDIO_REGS***********\n");
	seq_printf(s, "SDIO_ARGU: 0x%x\n", buf[SDIO_ARGU/4]);
	seq_printf(s, "SDIO_SEND: 0x%x\n", buf[SDIO_SEND/4]);
	seq_printf(s, "SDIO_CONF: 0x%x\n", buf[SDIO_CONF/4]);
	seq_printf(s, "SDIO_IRQS: 0x%x\n", buf[SDIO_IRQS/4]);
	seq_printf(s, "SDIO_IRQC: 0x%x\n", buf[SDIO_IRQC/4]);
	seq_printf(s, "SDIO_MULT: 0x%x\n", buf[SDIO_MULT/4]);
	seq_printf(s, "SDIO_ADDR: 0x%x\n", buf[SDIO_ADDR/4]);
	seq_printf(s, "SDIO_EXT: 0x%x\n", buf[SDIO_EXT/4]);
	return 0;
}

static int aml_sdio_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, aml_sdio_regs_show, inode->i_private);
}

static const struct file_operations aml_sdio_regs_fops = {
	.owner		= THIS_MODULE,
	.open		= aml_sdio_regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

/*----host param----*/

static int amlsd_param_show(struct seq_file *s, void *v)
{
	struct mmc_host *mmc = (struct mmc_host *)s->private;
	struct amlsd_platform *pdata = mmc_priv(mmc);

	seq_printf(s, "f_max : %d\n", pdata->f_max);
	seq_printf(s, "f_max_w : %d\n", pdata->f_max_w);
	seq_printf(s, "f_min : %d\n", pdata->f_min);
	seq_printf(s, "port : %d\n", pdata->port);
	seq_printf(s, "caps : 0x%x\n", pdata->caps);
	seq_printf(s, "ocr_avail : 0x%x\n", pdata->ocr_avail);
	seq_printf(s, "max_req_size : %d\n", pdata->max_req_size);
	return 0;
}

static int amlsd_param_open(struct inode *inode, struct file *file)
{
	return single_open(file, amlsd_param_show, inode->i_private);
}

static const struct file_operations amlsd_param_fops = {
	.owner		= THIS_MODULE,
	.open		= amlsd_param_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
void aml_sd_emmc_init_debugfs(struct mmc_host *mmc)
{
	struct dentry		*root;
	struct dentry		*node;
	if (!mmc->debugfs_root)
		return;
	root = debugfs_create_dir(dev_name(&mmc->class_dev), mmc->debugfs_root);
	if (IS_ERR(root)) {
		dev_err(&mmc->class_dev, "NEWSD debugfs just isn't enabled\n");
		return;
	}
	if (!root) {
		dev_err(&mmc->class_dev, "NEWSD it failed to create the directory\n");
		goto err;
	}
	debugfs_create_x32("sd_emmc_dbg", S_IRUGO | S_IWUSR, root,
		(u32 *)&sd_emmc_debug);
	sd_emmc_debug = 1;
	node = debugfs_create_file("sd_emmc_regs", S_IRUGO, root, mmc,
		&aml_sd_emmc_regs_fops);
	if (IS_ERR(node))
		return;
	node = debugfs_create_file("params", S_IRUGO, root, mmc,
		&amlsd_param_fops);
	if (IS_ERR(node))
		return;
	return;
err:
	dev_err(&mmc->class_dev, "failed to initialize debugfs for slot\n");
}

void aml_sdhc_init_debugfs(struct mmc_host *mmc)
{
	struct dentry		*root;
	struct dentry		*node;

	if (!mmc->debugfs_root)
		return;

	root = debugfs_create_dir(dev_name(&mmc->class_dev), mmc->debugfs_root);
	if (IS_ERR(root)) {
		/* Don't complain -- debugfs just isn't enabled */
		dev_err(&mmc->class_dev, "SDHC debugfs just isn't enabled\n");
		return;
	}
	if (!root) {
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		dev_err(&mmc->class_dev, "SDHC it failed to create the directory\n");
		goto err;
	}

	debugfs_create_x32("sdhc_dbg", S_IRWXUGO, root, (u32 *)&sdhc_debug);

	node = debugfs_create_file("sdhc_regs", S_IRWXUGO, root, mmc,
		&aml_sdhc_regs_fops);
	if (IS_ERR(node))
		return;

	node = debugfs_create_file("params", S_IRWXUGO, root, mmc,
		&amlsd_param_fops);
	if (IS_ERR(node))
		return;

	return;

err:
	dev_err(&mmc->class_dev, "failed to initialize debugfs for slot\n");
}

void aml_sdio_init_debugfs(struct mmc_host *mmc)
{
	struct dentry		*root;
	struct dentry		*node;

	if (!mmc->debugfs_root)
		return;

	root = debugfs_create_dir(dev_name(&mmc->class_dev), mmc->debugfs_root);
	if (IS_ERR(root)) {
		/* Don't complain -- debugfs just isn't enabled */
		dev_err(&mmc->class_dev, "SDIO debugfs just isn't enabled\n");
		return;
	}
	if (!root) {
		/* Complain -- debugfs is enabled, but it failed to
		 * create the directory. */
		dev_err(&mmc->class_dev, "SDIO it failed to create the directory\n");
		goto err;
	}

	debugfs_create_x32("sdio_dbg", S_IRWXUGO, root, (u32 *)&sdio_debug);

	node = debugfs_create_file("sdio_regs", S_IRWXUGO, root, mmc,
		&aml_sdio_regs_fops);
	if (IS_ERR(node))
		return;

	node = debugfs_create_file("params", S_IRWXUGO, root, mmc,
		&amlsd_param_fops);
	if (IS_ERR(node))
		return;

	return;

err:
	dev_err(&mmc->class_dev, "failed to initialize debugfs for SDIO\n");
}



void of_amlsd_irq_init(struct amlsd_platform *pdata)
{
	if (pdata->irq_in && pdata->irq_out) {
		gpio_for_irq(pdata->gpio_cd,
				AML_GPIO_IRQ(pdata->irq_in, FILTER_NUM7,
				GPIO_IRQ_FALLING));
		gpio_for_irq(pdata->gpio_cd,
				AML_GPIO_IRQ(pdata->irq_out, FILTER_NUM7,
				GPIO_IRQ_RISING));
	}
}

/*set pin input*/
void of_amlsd_pwr_prepare(struct amlsd_platform *pdata)
{
}


void of_amlsd_pwr_on(struct amlsd_platform *pdata)
{
	if (pdata->gpio_power)
		gpio_set_value(pdata->gpio_power, pdata->power_level);
	/* if(pdata->port == MESON_SDIO_PORT_A) { */
	/* extern_wifi_set_enable(1); //power on wifi */
    /* } */
}

void of_amlsd_pwr_off(struct amlsd_platform *pdata)
{
	if (pdata->gpio_power)
		gpio_set_value(pdata->gpio_power, !pdata->power_level);
	/* if(pdata->port == MESON_SDIO_PORT_A){ */
	/* extern_wifi_set_enable(0); //power off wifi */
    /* } */
}
int of_amlsd_init(struct amlsd_platform *pdata)
{
	int ret;

	BUG_ON(!pdata);

	if (pdata->gpio_cd) {
		ret = gpio_request_one(pdata->gpio_cd,
			GPIOF_IN, MODULE_NAME);
		CHECK_RET(ret);
	}

	if (pdata->gpio_ro) {
		ret = gpio_request_one(pdata->gpio_ro,
			GPIOF_IN, MODULE_NAME);
		if (!ret) { /* ok */
			ret = gpio_set_pullup(pdata->gpio_ro, 1);
			CHECK_RET(ret);
		} else {
			sdio_err("request gpio_ro pin fail!\n");
		}
	}

	if (pdata->gpio_power) {
		if (pdata->power_level) {
			ret = gpio_request_one(pdata->gpio_power,
					GPIOF_OUT_INIT_LOW, MODULE_NAME);
			CHECK_RET(ret);
		} else {
			ret = gpio_request_one(pdata->gpio_power,
					GPIOF_OUT_INIT_HIGH, MODULE_NAME);
		CHECK_RET(ret);
		}
	}

	/* if(pdata->port == MESON_SDIO_PORT_A) */
		/* wifi_setup_dt(); */
	return 0;
}

void aml_devm_pinctrl_put(struct amlsd_host *host)
{
	if (host->pinctrl) {
		devm_pinctrl_put(host->pinctrl);
		host->pinctrl = NULL;

		host->pinctrl_name[0] = '\0';
		/* sdio_err("Put Pinctrl\n"); */
	}
}

static struct pinctrl * __must_check aml_devm_pinctrl_get_select(
				struct amlsd_host *host, const char *name)
{
	struct pinctrl *p = host->pinctrl;
	struct pinctrl_state *s;
	int ret;

	if (!p) {
		p = devm_pinctrl_get(&host->pdev->dev);

		if (IS_ERR(p))
			return p;

		host->pinctrl = p;
		/* sdio_err("switch %s\n", name); */
	}

	s = pinctrl_lookup_state(p, name);
	if (IS_ERR(s)) {
		sdio_err("lookup %s fail\n", name);
		devm_pinctrl_put(p);
		return ERR_CAST(s);
	}

	ret = pinctrl_select_state(p, s);
	if (ret < 0) {
		sdio_err("select %s fail\n", name);
		devm_pinctrl_put(p);
		return ERR_PTR(ret);
	}
	return p;
}

void of_amlsd_xfer_pre(struct amlsd_platform *pdata)
{
	char pinctrl[30];
	char *p = pinctrl;
	int i, size = 0;
	struct pinctrl *ppin;
	/* to avoid emmc platform autoreboot issue */
	if (pdata->port == PORT_SDIO_C)
		udelay(65);

	size = sizeof(pinctrl);
	if (pdata->port > PORT_SDIO_C)
		aml_snprint(&p, &size, "sdhc_");

	if (pdata->mmc->ios.chip_select == MMC_CS_DONTCARE) {
		if ((pdata->mmc->caps & MMC_CAP_4_BIT_DATA)
		|| (strcmp(pdata->pinname, "sd"))
		|| (pdata->mmc->caps & MMC_CAP_8_BIT_DATA)) {
			aml_snprint(&p, &size, "%s_all_pins", pdata->pinname);
		} else{
			if (pdata->is_sduart && (!strcmp(pdata->pinname, "sd")))
				aml_snprint(&p, &size,
					"%s_1bit_uart_pins", pdata->pinname);
			else
				aml_snprint(&p, &size,
					"%s_1bit_pins", pdata->pinname);
		}
	} else { /* MMC_CS_HIGH */
		if (pdata->is_sduart && (!strcmp(pdata->pinname, "sd")))
			aml_snprint(&p, &size,
				"%s_clk_cmd_uart_pins", pdata->pinname);
		else
			aml_snprint(&p, &size,
				"%s_clk_cmd_pins", pdata->pinname);
	}

	/* if pinmux setting is changed (pinctrl_name is different) */
	if (strncmp(pdata->host->pinctrl_name, pinctrl,
		sizeof(pdata->host->pinctrl_name))) {

		if (strlcpy(pdata->host->pinctrl_name, pinctrl,
			sizeof(pdata->host->pinctrl_name))
			>= sizeof(pdata->host->pinctrl_name)) {

			sdio_err("Pinctrl name is too long!\n");
			return;
		}

	for (i = 0; i < 100; i++) {
		mutex_lock(&pdata->host->pinmux_lock);
		ppin = aml_devm_pinctrl_get_select(pdata->host, pinctrl);
		mutex_unlock(&pdata->host->pinmux_lock);
		if (!IS_ERR(ppin)) {
			/* pdata->host->pinctrl = ppin; */
			break;
		}
		/* else -> aml_irq_cdin_thread()
		*should be using one of the GPIO of card,
		* then we should wait here until the GPIO is free,
		* otherwise something must be wrong.
		*/
		mdelay(1);
	}
	if (i == 100)
		sdhc_err("CMD%d: get pinctrl %s fail.\n",
					pdata->host->opcode, pinctrl);

	/* pr_info("pre pinctrl %x, %s, ppin %x\n",
				pdata->host->pinctrl, p, ppin); */
	}
}

void of_amlsd_xfer_post(struct amlsd_platform *pdata)
{
	/* if (pdata->host->pinctrl) {
		devm_pinctrl_put(pdata->host->pinctrl);
		pdata->host->pinctrl = NULL;
	} else {
		sdhc_err("CMD%d: pdata->host->pinctrl = NULL\n",
					pdata->host->opcode);
	}
	pr_info(KERN_ERR "CMD%d: put pinctrl\n", pdata->host->opcode);
	aml_dbg_print_pinmux();  */
}

int of_amlsd_ro(struct amlsd_platform *pdata)
{
	int ret = 0; /* 0--read&write, 1--read only */

	if (pdata->gpio_ro)
		ret = gpio_get_value(pdata->gpio_ro);
	/* sdio_err("read-only?--%s\n", ret?"YES":"NO"); */
	return ret;
}



void aml_cs_high(struct amlsd_platform *pdata) /* chip select high */
{
	int ret;

	/*
	* Non-SPI hosts need to prevent chipselect going active during
	* GO_IDLE; that would put chips into SPI mode.  Remind them of
	* that in case of hardware that won't pull up DAT3/nCS otherwise.
	*
	* Now the way to accomplish this is:
	* 1) set DAT3-pin as a GPIO pin(by pinmux), and pulls up;
	* 2) send CMD0;
	* 3) set DAT3-pin as a card-dat3-pin(by pinmux);
	*/
	if ((pdata->mmc->ios.chip_select == MMC_CS_HIGH)
	&& (pdata->gpio_dat3 != 0)
	&& (pdata->jtag_pin == 0)) { /* is NOT sd card */
		aml_devm_pinctrl_put(pdata->host);
		ret = gpio_request_one(pdata->gpio_dat3,
				GPIOF_OUT_INIT_HIGH, MODULE_NAME);
		CHECK_RET(ret);
		if (ret == 0) {
			ret = gpio_direction_output(pdata->gpio_dat3, 1);
			CHECK_RET(ret);
		}
	}
}

void aml_cs_dont_care(struct amlsd_platform *pdata) /* chip select don't care */
{

	if ((pdata->mmc->ios.chip_select == MMC_CS_DONTCARE)
	&& (pdata->gpio_dat3 != 0)
	&& (pdata->jtag_pin == 0)
	&& (gpio_get_value(pdata->gpio_dat3) >= 0))
		gpio_free(pdata->gpio_dat3);
}

static int aml_is_card_insert(struct amlsd_platform *pdata)
{
	int ret = 0, in_count = 0, out_count = 0, i;
	if (pdata->gpio_cd) {
		mdelay(pdata->card_in_delay);
		for (i = 0; i < 200; i++) {
			ret = gpio_get_value(pdata->gpio_cd);
			if (ret)
				out_count++;
			in_count++;
			if ((out_count > 100) || (in_count > 100))
				break;
		}
		if (out_count > 100)
			ret = 1;
		else if (in_count > 100)
			ret = 0;
	}
	sdio_err("card %s\n", ret?"OUT":"IN");
	if (!pdata->gpio_cd_level)
		ret = !ret; /* reverse, so ---- 0: no inserted  1: inserted */

	return ret;
}

/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
static int aml_is_sdjtag(struct amlsd_platform *pdata)
{
	int in = 0, i;
	int high_cnt = 0, low_cnt = 0;
	if (gpio_request_one(pdata->jtag_pin, GPIOF_IN, MODULE_NAME)) {
		pr_info("jtag_pin request failed\n");
		return 0;

	}
	for (i = 0; ; i++) {
		mdelay(1);
		if (gpio_get_value(pdata->jtag_pin)) {
			high_cnt++;
			low_cnt = 0;
		} else {
			low_cnt++;
			high_cnt = 0;
		}
		if ((high_cnt > 50) || (low_cnt > 50))
			break;
	}

	if (low_cnt > 50)
		in = 1;
	gpio_free(pdata->jtag_pin);
	return !in;
}

static int aml_is_sduart(struct amlsd_platform *pdata)
{
#ifdef CONFIG_MESON_CPU_EMULATOR
	return 0;
#else
	int in = 0, i;
	int high_cnt = 0, low_cnt = 0;
	struct pinctrl *pc;

	mutex_lock(&pdata->host->pinmux_lock);
	pc = aml_devm_pinctrl_get_select(pdata->host, "sd_to_ao_uart_pins");

	if (gpio_request_one(pdata->gpio_dat3,
		GPIOF_IN, MODULE_NAME))
		return 0;
	for (i = 0; ; i++) {
		mdelay(1);
		if (gpio_get_value(pdata->gpio_dat3)) {
			high_cnt++;
			low_cnt = 0;
		} else {
			low_cnt++;
			high_cnt = 0;
		}
		if ((high_cnt > 100) || (low_cnt > 100))
			break;
	}
	if (low_cnt > 100)
		in = 1;
	gpio_free(pdata->gpio_dat3);
	mutex_unlock(&pdata->host->pinmux_lock);
	return in;
#endif
}

/* int n=0; */
static int aml_uart_switch(struct amlsd_platform *pdata, bool on)
{
	struct pinctrl *pc;
	char *name[2] = {
		"sd_to_ao_uart_pins",
		"ao_to_sd_uart_pins",
	};
	/* if (on == pdata->is_sduart)
		 return 0; */

	pdata->is_sduart = on;
	mutex_lock(&pdata->host->pinmux_lock);
	pc = aml_devm_pinctrl_get_select(pdata->host, name[on]);
	mutex_unlock(&pdata->host->pinmux_lock);
	return on;
}
/* #endif */

/* clear detect information */
void aml_sd_uart_detect_clr(struct amlsd_platform *pdata)
{
	pdata->is_sduart = 0;
	pdata->is_in = 0;
}


/*
 * setup jtag on/off, and setup ao/ee jtag
 *
 * @state: must be JTAG_STATE_ON/JTAG_STATE_OFF
 * @select: mest be JTAG_DISABLE/JTAG_A53_AO/JTAG_A53_EE
 */
#ifdef CONFIG_ARM64
void jtag_set_state(unsigned state, unsigned select)
{
	uint64_t command;
	if (state == JTAG_STATE_ON)
		command = JTAG_ON;
	else
		command = JTAG_OFF;
	asm __volatile__("" : : : "memory");

	__invoke_psci_fn_smc(command, select, 0, 0);
}
#else
void jtag_set_state(unsigned state, unsigned select)
{
	unsigned command;
	register unsigned r0 asm("r0");
	register unsigned r1 asm("r1");

	if (state == JTAG_STATE_ON)
		command = JTAG_ON;
	else
		command = JTAG_OFF;

	asm __volatile__("" : : : "memory");

	r0 = select;
	r1 = command;

	asm volatile(
		__asmeq("%0", "r1")
		__asmeq("%1", "r0")
		__SMC(0)
		: : "r" (r1), "r"(r0));
}
#endif

void jtag_select_ao(void)
{
	set_cpus_allowed_ptr(current, cpumask_of(0));
	jtag_set_state(JTAG_STATE_ON, JTAG_A53_AO);
	set_cpus_allowed_ptr(current, cpu_all_mask);
}

void jtag_select_sd(void)
{
	set_cpus_allowed_ptr(current, cpumask_of(0));
	jtag_set_state(JTAG_STATE_ON, JTAG_A53_EE);
	set_cpus_allowed_ptr(current, cpu_all_mask);
}


static void aml_jtag_switch_sd(struct amlsd_platform *pdata)
{
	struct pinctrl *pc;
	int i;
	for (i = 0; i < 100; i++) {
		mutex_lock(&pdata->host->pinmux_lock);
		pc = aml_devm_pinctrl_get_select(pdata->host,
			"ao_to_sd_jtag_pins");
		mutex_unlock(&pdata->host->pinmux_lock);
		if (!IS_ERR(pc))
			break;
		mdelay(1);
	}
	if (is_jtag_apee()) {
		jtag_select_sd();
		pr_info("setup apee\n");
	}
	return;
}

static void aml_jtag_switch_ao(struct amlsd_platform *pdata)
{
	struct pinctrl *pc;
	int i;
	for (i = 0; i < 100; i++) {
		mutex_lock(&pdata->host->pinmux_lock);
		pc = aml_devm_pinctrl_get_select(pdata->host,
			"sd_to_ao_jtag_pins");
		mutex_unlock(&pdata->host->pinmux_lock);
		if (!IS_ERR(pc))
			break;
		mdelay(1);
	}
	return;
}


int aml_sd_uart_detect(struct amlsd_platform *pdata)
{
	static bool is_jtag;
	if (aml_is_card_insert(pdata)) {
		if (pdata->is_in)
			return 1;
		else
			pdata->is_in = true;
		if (aml_is_sduart(pdata)) {
			aml_uart_switch(pdata, 1);
			pr_info("Uart in\n");
			pdata->mmc->caps &= ~MMC_CAP_4_BIT_DATA;
			if (aml_is_sdjtag(pdata)) {
				is_jtag = true;
				/* aml_jtag_sd(); */
				aml_jtag_switch_sd(pdata);
				pdata->is_in = false;
				pr_info("JTAG in\n");
				return 0;
			}
		} else {
			pr_info("normal card in\n");
			aml_uart_switch(pdata, 0);
			/* aml_jtag_gpioao(); */
			aml_jtag_switch_ao(pdata);
			if (pdata->caps & MMC_CAP_4_BIT_DATA)
				pdata->mmc->caps |= MMC_CAP_4_BIT_DATA;
		}
	} else {
		if ((!pdata->is_in) && (pdata->is_sduart == false))
			return 1;
		else
			pdata->is_in = false;
		if (is_jtag) {
			is_jtag = false;
			pr_info("JTAG OUT\n");
		} else
			pr_info("card out\n");

		pdata->is_tuned = false;
		if (pdata->mmc && pdata->mmc->card)
			mmc_card_set_removed(pdata->mmc->card);
		aml_uart_switch(pdata, 0);
		/* aml_jtag_gpioao(); */
		aml_jtag_switch_ao(pdata);
		 /* switch to 3.3V */
		aml_sd_voltage_switch(pdata,
		MMC_SIGNAL_VOLTAGE_330);

		if (pdata->caps & MMC_CAP_4_BIT_DATA)
			pdata->mmc->caps |= MMC_CAP_4_BIT_DATA;
	}
	return 0;
}

/*u32 cd_irq_cnt[2] = {0, 0}; //debug*/
static int card_dealed;
irqreturn_t aml_irq_cd_thread(int irq, void *data)
{
	struct amlsd_platform *pdata = (struct amlsd_platform *)data;
	int ret = 0;
	/* cd_irq_cnt[(irq == 99)] ++; //debug */
	mutex_lock(&pdata->in_out_lock);
	if (card_dealed == 1) {
		card_dealed = 0;
		mutex_unlock(&pdata->in_out_lock);
		return IRQ_HANDLED;
	}
	ret = aml_sd_uart_detect(pdata);
	if (ret == 1) {/* the same as the last*/
		mutex_unlock(&pdata->in_out_lock);
		return IRQ_HANDLED;
	}
	card_dealed = 1;
	if ((pdata->is_in == 0) && aml_card_type_non_sdio(pdata))
		pdata->host->init_flag = 0;
	mutex_unlock(&pdata->in_out_lock);
	/* mdelay(500); */
	if (pdata->is_in)
		mmc_detect_change(pdata->mmc, msecs_to_jiffies(100));
	else
		mmc_detect_change(pdata->mmc, msecs_to_jiffies(0));
	card_dealed = 0;
	return IRQ_HANDLED;
}

irqreturn_t aml_sd_irq_cd(int irq, void *dev_id)
{
	/* pr_info("cd dev_id %x\n", (unsigned)dev_id); */
	return IRQ_WAKE_THREAD;
}

void aml_sduart_pre(struct amlsd_platform *pdata)
{
	if (((pdata->port == MESON_SDIO_PORT_B) ||
	(pdata->port == MESON_SDIO_PORT_XC_B))) {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8) {
			/* clear pinmux of CARD_0 and CARD_4 to
				make them used as gpio */
			/* CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_2, 0x5040); */
			/* CLEAR_CBUS_REG_MASK(PERIPHS_PIN_MUX_8, 0x400); */
			aml_jtag_gpioao();
			aml_devm_pinctrl_put(pdata->host);
		}
		aml_sd_uart_detect(pdata);
	}
}

static int aml_cmd_invalid(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	unsigned long flags;

	spin_lock_irqsave(&pdata->host->mrq_lock, flags);
	mrq->cmd->error = -EINVAL;
	spin_unlock_irqrestore(&pdata->host->mrq_lock, flags);
	mmc_request_done(mmc, mrq);

	return -EINVAL;
}
static int aml_rpmb_cmd_invalid(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	struct amlsd_host *host = pdata->host;
	unsigned long flags;

	spin_lock_irqsave(&host->mrq_lock, flags);
	host->xfer_step = XFER_FINISHED;
	host->mrq = NULL;
	host->status = HOST_INVALID;
	spin_unlock_irqrestore(&host->mrq_lock, flags);
	mrq->data->bytes_xfered = mrq->data->blksz*mrq->data->blocks;
	mmc_request_done(mmc, mrq);
	return -EINVAL;
}

int aml_check_unsupport_cmd(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);

	/* if ((pdata->port != PORT_SDIO) &&
		(mrq->cmd->opcode == SD_IO_SEND_OP_COND || */
	/* mrq->cmd->opcode == SD_IO_RW_DIRECT || */
	/* mrq->cmd->opcode == SD_IO_RW_EXTENDED)) { */
	/* mrq->cmd->error = -EINVAL; */
	/* mmc_request_done(mmc, mrq); */
	/* return -EINVAL; */
	/* } */

	/* CMD3 means the first time initialized flow is running */
	if (mrq->cmd->opcode == 3)
		mmc->first_init_flag = false;

	if (aml_card_type_mmc(pdata)) {
		if (mrq->cmd->opcode == 6) {
			if (mrq->cmd->arg == 0x3B30301)
				pdata->rmpb_cmd_flag = 1;
			else
				pdata->rmpb_cmd_flag = 0;
		}
		if (pdata->rmpb_cmd_flag && (!pdata->rpmb_valid_command)) {
			if ((mrq->cmd->opcode == 18)
				|| (mrq->cmd->opcode == 25))
				return aml_rpmb_cmd_invalid(mmc, mrq);
		}
		if (pdata->rmpb_cmd_flag && (mrq->cmd->opcode == 23))
			pdata->rpmb_valid_command = 1;
		else
			pdata->rpmb_valid_command = 0;
	}

	if (mmc->caps & MMC_CAP_NONREMOVABLE) { /* nonremovable device */
	if (mmc->first_init_flag) { /* init for the first time */
		/* for 8189ETV needs ssdio reset when starts */
		if (aml_card_type_sdio(pdata)) {
			/* if (mrq->cmd->opcode == SD_IO_RW_DIRECT */
			/* || mrq->cmd->opcode == SD_IO_RW_EXTENDED */
			/* || mrq->cmd->opcode == SD_SEND_IF_COND) {*/
			/* return aml_cmd_invalid(mmc, mrq); */
			/* } */
			return 0;
		} else if (aml_card_type_mmc(pdata)) {
			if (mrq->cmd->opcode == SD_IO_SEND_OP_COND
			|| mrq->cmd->opcode == SD_IO_RW_DIRECT
			|| mrq->cmd->opcode == SD_IO_RW_EXTENDED
			|| mrq->cmd->opcode == SD_SEND_IF_COND
			|| mrq->cmd->opcode == MMC_APP_CMD) {
				return aml_cmd_invalid(mmc, mrq);
		}
		} else if (aml_card_type_sd(pdata)
		|| aml_card_type_non_sdio(pdata)) {
			if (mrq->cmd->opcode == SD_IO_SEND_OP_COND
			|| mrq->cmd->opcode == SD_IO_RW_DIRECT
			|| mrq->cmd->opcode == SD_IO_RW_EXTENDED) {
				return aml_cmd_invalid(mmc, mrq);
			}
		}
	}
	} else { /* removable device */
	/* filter cmd 5/52/53 for a non-sdio device */
	if (!aml_card_type_sdio(pdata) && !aml_card_type_unknown(pdata)) {
		if (mrq->cmd->opcode == SD_IO_SEND_OP_COND
		|| mrq->cmd->opcode == SD_IO_RW_DIRECT
		|| mrq->cmd->opcode == SD_IO_RW_EXTENDED) {
			return aml_cmd_invalid(mmc, mrq);
		}
	}
	}
	return 0;
}

int aml_sd_voltage_switch(struct amlsd_platform *pdata, char signal_voltage)
{
	struct amlsd_host *host = pdata->host;
	int ret = 0;

	/* voltage is the same, return directly */
	if (!aml_card_type_non_sdio(pdata)
		|| (pdata->signal_voltage == signal_voltage)) {
		if (aml_card_type_sdio(pdata))
			host->sd_sdio_switch_volat_done = 1;
			return 0;
	}
	if (pdata->vol_switch) {
		if (pdata->signal_voltage == 0xff) {
			gpio_free(pdata->vol_switch);
			ret = gpio_request_one(pdata->vol_switch,
					GPIOF_OUT_INIT_HIGH, MODULE_NAME);
			if (ret) {
				pr_err("%s [%d] request error\n",
						__func__, __LINE__);
				return -EINVAL;
			}
		}
		if (signal_voltage == MMC_SIGNAL_VOLTAGE_180)
			ret = gpio_direction_output(pdata->vol_switch,
						pdata->vol_switch_18);
		else
			ret = gpio_direction_output(pdata->vol_switch,
					(!pdata->vol_switch_18));
		CHECK_RET(ret);
		if (!ret)
			pdata->signal_voltage = signal_voltage;
	} else
		return -EINVAL;

	host->sd_sdio_switch_volat_done = 1;
	return 0;
}

/* boot9 here */
void aml_emmc_hw_reset(struct mmc_host *mmc)
{
	struct amlsd_platform *pdata = mmc_priv(mmc);
	u32 ret;
	if (!aml_card_type_mmc(pdata) || !pdata->hw_reset)
		return;

	/* boot_9 used as eMMC hw_rst pin here. */
	gpio_free(pdata->hw_reset);
	ret = gpio_request_one(pdata->hw_reset,
			GPIOF_OUT_INIT_HIGH, MODULE_NAME);
	CHECK_RET(ret);
	if (ret) {
		pr_err("%s [%d] request error\n",
				__func__, __LINE__);
		return;
	}
	ret = gpio_direction_output(pdata->hw_reset, 0);
	CHECK_RET(ret);
	if (ret) {
		pr_err("%s [%d] output high error\n",
			__func__, __LINE__);
		return;
	}
	mdelay(2);
	ret = gpio_direction_output(pdata->hw_reset, 1);
	CHECK_RET(ret);
	if (ret) {
		pr_err("%s [%d] output high error\n",
			__func__, __LINE__);
		return;
	}
	mdelay(2);
	return;
}

static void sdio_rescan(struct mmc_host *host)
{
	int ret;

	host->rescan_entered = 0;
	host->host_rescan_disable = false;
	mmc_detect_change(host, 0);
	/* start the delayed_work */
	ret = flush_work(&(host->detect.work));
	/* wait for the delayed_work to finish */
	if (!ret)
		pr_info("Error: delayed_work mmc_rescan() already idle!\n");
}


void sdio_reinit(void)
{
	if (sdio_host) {
		if (sdio_host->card)
			sdio_reset_comm(sdio_host->card);
		else
		sdio_rescan(sdio_host);
	} else {
	    pr_info("Error: sdio_host is NULL\n");
	}

	pr_info("[%s] finish\n", __func__);
}
EXPORT_SYMBOL(sdio_reinit);

/*-------------------debug---------------------*/

unsigned int sdhc_debug = 0x0;

static int __init sdhc_debug_setup(char *str)
{
	int ret;
	ret = kstrtol(str, 0, (long *)&sdhc_debug);
	return ret;
}
__setup("sdhc_debug=", sdhc_debug_setup);


unsigned int sdio_debug = 0x000000;

static int __init sdio_debug_setup(char *str)
{
	int ret;
	ret = kstrtol(str, 0, (long *)&sdio_debug);
	return ret;
	return 1;
}
__setup("sdio_debug=", sdio_debug_setup);

unsigned int sd_emmc_debug = 0x0;
static int __init sd_emmc_debug_setup(char *str)
{
	int ret;
	ret = kstrtol(str, 0, (long *)&sd_emmc_debug);
	return ret;
}
__setup("sd_emmc_debug=", sd_emmc_debug_setup);
void aml_dbg_print_pinmux(void)
{
	pr_info(
	"REG2=0x%x,REG3=0x%x, REG4=0x%x, REG5=0x%x, REG6=0x%x, REG8=0x%x\n",
		aml_read_cbus(PERIPHS_PIN_MUX_2),
		aml_read_cbus(PERIPHS_PIN_MUX_3),
		aml_read_cbus(PERIPHS_PIN_MUX_4),
		aml_read_cbus(PERIPHS_PIN_MUX_5),
		aml_read_cbus(PERIPHS_PIN_MUX_6),
		aml_read_cbus(PERIPHS_PIN_MUX_8));
}

void aml_snprint (char **pp, int *left_size,  const char *fmt, ...)
{
	va_list args;
	char *p = *pp;
	int size;

	if (*left_size <= 1) {
		sdhc_err("buf is full\n");
		return;
	}

	va_start(args, fmt);
	size = vsnprintf(p, *left_size, fmt, args);
	va_end(args);
	*pp += size;
	*left_size -= size;
}


#ifdef CONFIG_MMC_AML_DEBUG

#define     CARD_PULL_UP_REG            PAD_PULL_UP_REG2
#define     EMMC_PULL_UP_REG            PAD_PULL_UP_REG2
#define     SDIO_PULL_UP_REG            PAD_PULL_UP_REG4

#define     CARD_PULL_UP_REG_EN         PAD_PULL_UP_EN_REG2
#define     EMMC_PULL_UP_REG_EN         PAD_PULL_UP_EN_REG2
#define     SDIO_PULL_UP_REG_EN         PAD_PULL_UP_EN_REG4

#define     CARD_PULL_UP_REG_MASK       0x03f00000 /* card[5:0], REG2 */
#define     EMMC_PULL_UP_REG_MASK       0x0003000f /* boot[17:16, 3:0], REG2 */
#define     SDIO_PULL_UP_REG_MASK       0x0000030f /* gpioX[9:8, 3:0], REG4 */
void aml_dbg_verify_pull_up(struct amlsd_platform *pdata)
{
	int reg;
	int reg_en;
	int reg_mask;

	if (pdata->port == PORT_SDIO_A) {
		reg = aml_read_cbus(SDIO_PULL_UP_REG);
		reg_en = aml_read_cbus(SDIO_PULL_UP_REG_EN);
		reg_mask = SDIO_PULL_UP_REG_MASK;
	} else if (pdata->port == PORT_SDIO_B) {
		reg = aml_read_cbus(CARD_PULL_UP_REG);
		reg_en = aml_read_cbus(CARD_PULL_UP_REG_EN);
		reg_mask = CARD_PULL_UP_REG_MASK;
	} else if (pdata->port == PORT_SDIO_C) {
		reg = aml_read_cbus(EMMC_PULL_UP_REG);
		reg_en = aml_read_cbus(EMMC_PULL_UP_REG_EN);
		reg_mask = EMMC_PULL_UP_REG_MASK;
	}

	if ((reg&reg_mask) != reg_mask) {
		sdio_err(" %s pull-up error: CMD%d, reg=%#08x, reg_mask=%#x\n",
			mmc_hostname(pdata->mmc), pdata->host->opcode,
				reg, reg_mask);
	}
	if ((reg_en&reg_mask) != reg_mask) {
		sdio_err(" %s pull-up error: CMD%d, reg_en=%#08x, reg_mask=%#x\n",
			mmc_hostname(pdata->mmc), pdata->host->opcode,
				reg_en, reg_mask);
	}
}

#define     CARD_PINMUX_REG             PERIPHS_PIN_MUX_2
#define     EMMC_PINMUX_REG             PERIPHS_PIN_MUX_6
#define     SDIO_PINMUX_REG             PERIPHS_PIN_MUX_8
int aml_dbg_verify_pinmux(struct amlsd_platform *pdata)
{
	int reg;
	int reg_mask;

	if (pdata->port == PORT_SDIO_A) {
		reg = SDIO_PINMUX_REG;
		if (pdata->mmc->ios.chip_select == MMC_CS_DONTCARE) {
			reg_mask = 0x0000003f; /* all pin */
		} else { /* MMC_CS_HIGH */
			reg_mask = 0x00000003; /* clk & cmd */
		}
	} else if (pdata->port == PORT_SDIO_B) {
		reg = CARD_PINMUX_REG;
		if (pdata->mmc->ios.chip_select == MMC_CS_DONTCARE) {
			if (pdata->mmc->caps & MMC_CAP_4_BIT_DATA)
				reg_mask = 0x0000fc00; /* all pin */
			else
				reg_mask = 0x00008c00; /* 1 bit */
		} else { /* MMC_CS_HIGH */
			reg_mask = 0x00000c00; /* clk & cmd */
		}
	} else if (pdata->port == PORT_SDIO_C) {
		reg = EMMC_PINMUX_REG;
		if (pdata->mmc->ios.chip_select == MMC_CS_DONTCARE) {
			reg_mask = 0x3f000000; /* all pin */
		} else { /* MMC_CS_HIGH */
			reg_mask = 0x03000000; /* clk & cmd */
		}
	}

	reg = aml_read_cbus(reg);
	if ((reg&reg_mask) != reg_mask) {
		sdio_err(" %s pinmux error: CMD%d, reg=%#08x, reg_mask=%#x\n",
		mmc_hostname(pdata->mmc), pdata->host->opcode,
		reg, reg_mask);
	}

	return 0;
}
#endif /* #ifdef      CONFIG_MMC_AML_DEBUG */

/* #ifndef CONFIG_AM_WIFI_SD_MMC */
/* int wifi_setup_dt() */
/* { */
	/* return 0; */
/* } */
/* void extern_wifi_set_enable(int is_on) */
/* { */
    /* return; */
/* } */
/* #endif */

