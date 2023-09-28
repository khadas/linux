// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Jeff Kletsky
 *
 * Author: Jeff Kletsky <git-commits@allycomm.com>
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_DOSILICON			0xe5
#define DOSILICON_STATUS_ECC_MASK		GENMASK(6, 4)
#define DOSILICON_STATUS_ECC_NO_BITFLIPS	(0 << 4)
#define DOSILICON_STATUS_ECC_1_3_BITFLIPS	BIT(4)
#define DOSILICON_STATUS_ECC_UNCOR_ERROR	(2 << 4)
#define DOSILICON_STATUS_ECC_4_6_BITFLIPS	(3 << 4)
#define DOSILICON_STATUS_ECC_7_8_BITFLIPS	(5 << 4)

static SPINAND_OP_VARIANTS(read_cache_variants,
		//SPINAND_PAGE_READ_FROM_CACHE_QUADIO_OP(0, 2, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		//SPINAND_PAGE_READ_FROM_CACHE_DUALIO_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
		SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
		SPINAND_PROG_LOAD_X4(false, 0, NULL, 0),
		SPINAND_PROG_LOAD(false, 0, NULL, 0));

static int ds35q2gb_ooblayout_ecc(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	/* Unable to know the layout of ECC */
	return -ERANGE;
}

static int ds35q2gb_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	if (section)
		return -ERANGE;

	/* Reserve 2 bytes for the BBM. */
	region->offset = 2;
	region->length = 62;

	return 0;
}

static const struct mtd_ooblayout_ops ds35q2gb_ooblayout = {
	.ecc = ds35q2gb_ooblayout_ecc,
	.free = ds35q2gb_ooblayout_free,
};

static int ds35q2gb_ecc_get_status(struct spinand_device *spinand,
				   u8 status)
{
	switch (status & DOSILICON_STATUS_ECC_MASK) {
	case DOSILICON_STATUS_ECC_NO_BITFLIPS:
		return 0;
	/*
	 * We have no way to know exactly how many bitflips have been
	 * fixed, so let's return the maximum possible value so that
	 * wear-leveling layers move the data immediately.
	 */
	case DOSILICON_STATUS_ECC_1_3_BITFLIPS:
		return 3;
	case DOSILICON_STATUS_ECC_4_6_BITFLIPS:
		return 6;
	case DOSILICON_STATUS_ECC_7_8_BITFLIPS:
		return 8;
	case DOSILICON_STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;

	default:
		break;
	}

	return -EINVAL;
}

static const struct spinand_info dosilicon_spinand_table[] = {
	SPINAND_INFO("DS35Q2GB-IBR 3.3v", 0xf2,
		     NAND_MEMORG(1, 2048, 128, 64, 2048, 40, 2, 1, 1),
		     NAND_ECCREQ(8, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     SPINAND_HAS_QE_BIT,
		     SPINAND_ECCINFO(&ds35q2gb_ooblayout,
				     ds35q2gb_ecc_get_status)),
};

static int dosilicon_spinand_detect(struct spinand_device *spinand)
{
	u8 *id = spinand->id.data;
	int ret;

	if (id[0] != SPINAND_MFR_DOSILICON)
		return 0;

	ret = spinand_match_and_init(spinand, dosilicon_spinand_table,
				     ARRAY_SIZE(dosilicon_spinand_table),
				     id[1]);
	if (ret)
		return ret;

	return 1;
}

static const struct spinand_manufacturer_ops dosilicon_spinand_manuf_ops = {
	.detect = dosilicon_spinand_detect,
};

const struct spinand_manufacturer dosilicon_spinand_manufacturer = {
	.id = SPINAND_MFR_DOSILICON,
	.name = "dosilicon",
	.ops = &dosilicon_spinand_manuf_ops,
};
