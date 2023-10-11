/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_SPI_NAND_H_
#define __AML_SPI_NAND_H_

/* Max total is 1024 as romboot says so... */
#define SPI_NAND_BOOT_TOTAL_PAGES	(1024)
/* This depends on uboot size */
#define SPI_NAND_BOOT_PAGES_PER_COPY (1024)
#define SPI_NAND_BOOT_COPY_NUM (SPI_NAND_BOOT_TOTAL_PAGES / SPI_NAND_BOOT_PAGES_PER_COPY)

#define SPI_NAND_BL2_SIZE			(64 * 1024)
#define SPI_NAND_BL2_OCCUPY_PER_PAGE		2048
#define SPI_NAND_BL2_PAGES			(SPI_NAND_BL2_SIZE / SPI_NAND_BL2_OCCUPY_PER_PAGE)
#define SPI_NAND_BL2_COPY_NUM		8
#define SPI_NAND_TPL_SIZE_PER_COPY	0x200000
#define SPI_NAND_TPL_COPY_NUM		4
#define SPI_NAND_NBITS		2

struct spinand_info_page {
	char magic[8];	/* magic header of info page */
	/* info page version, +1 when you update this struct */
	u8 version;	/* 1 for now */
	u8 mode;	/* 1 discrete, 0 compact */
	u8 bl2_num;	/* bl2 copy number */
	u8 fip_num;	/* fip copy number */
	union {
		struct {
#define SPINAND_MAGIC       "AMLIFPG"
#define SPINAND_INFO_VER    1
			u8 rd_max; /* spi nand max read io */
			u8 oob_offset; /* user bytes offset */
			u8 reserved[2];
			u32 fip_start; /* start pages */
			u32 fip_pages; /* pages per fip */
			u32 page_size; /* spi nand page size (bytes) */
			u32 page_per_blk;	/* page number per block */
			u32 oob_size;	/* valid oob size (bytes) */
			u32 bbt_start; /* bbt start pages */
			u32 bbt_valid; /* bbt valid offset pages */
			u32 bbt_size;	/* bbt occupied bytes */
		} s;/* spi nand */
		struct {
			u32 reserved;
		} e;/* emmc */
	} dev;

};
#endif/* __AML_SPI_NAND_H_ */
