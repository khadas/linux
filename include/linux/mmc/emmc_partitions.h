/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _EMMC_PARTITIONS_H
#define _EMMC_PARTITIONS_H

#include<linux/genhd.h>

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/core.h>

/* #include <mach/register.h> */
/* #include <mach/am_regs.h> */
#define CONFIG_DTB_SIZE  (256 * 1024U)
#define CONFIG_DDR_PARAMETER_SIZE  SZ_2M
#define DTB_CELL_SIZE	(16 * 1024U)
#define	STORE_CODE				1
#define	STORE_CACHE				BIT(1)
#define	STORE_DATA				BIT(2)

#define     MAX_PART_NAME_LEN               16
#define     MAX_MMC_PART_NUM                32

/* MMC Partition Table */
#define     MMC_PARTITIONS_MAGIC            "MPT"
#define     MMC_RESERVED_NAME               "reserved"

#define     SZ_1M                           0x00100000

/* the size of bootloader partition */
#define     MMC_BOOT_PARTITION_SIZE         (4 * SZ_1M)
#define		MMC_TUNING_OFFSET               0X14400

/* the size of reserve space behind bootloader partition */
#define     MMC_BOOT_PARTITION_RESERVED     (32 * SZ_1M)

#define     RESULT_OK                       0
#define     RESULT_FAIL                     1
#define     RESULT_UNSUP_HOST               2
#define     RESULT_UNSUP_CARD               3

struct partitions {
	/* identifier string */
	char name[MAX_PART_NAME_LEN];
	/* partition size, byte unit */
	u64 size;
	/* offset within the master space, byte unit */
	u64 offset;
	/* master flags to mask out for this partition */
	unsigned int mask_flags;
};

struct mmc_partitions_fmt {
	char magic[4];
	unsigned char version[12];
	int part_num;
	int checksum;
	struct partitions partitions[MAX_MMC_PART_NUM];
};

/*#ifdef CONFIG_MMC_AML*/
int aml_emmc_partition_ops(struct mmc_card *card, struct gendisk *disk);
int add_fake_boot_partition(struct gendisk *disk, char *name, int idx);
/*
 *#else
 *static inline int aml_emmc_partition_ops(struct mmc_card *card,
 *					 struct gendisk *disk)
 *{
 *	return -1;
 *}
 *#endif
 */
unsigned int mmc_capacity(struct mmc_card *card);
int mmc_read_internal(struct mmc_card *card,
		      unsigned int dev_addr, unsigned int blocks, void *buf);
int mmc_write_internal(struct mmc_card *card,
		       unsigned int dev_addr, unsigned int blocks, void *buf);
int get_reserve_partition_off_from_tbl(void);
#endif

extern struct mmc_partitions_fmt *pt_fmt;

int aml_disable_mmc_cqe(struct mmc_card *card);

int aml_enable_mmc_cqe(struct mmc_card *card);
