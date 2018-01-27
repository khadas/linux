/*
 * drivers/amlogic/amlnf/block/aml_nftl_hw_interface.c
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


#include "aml_nftl_block.h"


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int nand_erase_superblk(struct aml_nftl_part_t *part, struct _physic_op_par *p)
{
	struct aml_nftl_dev *nftl_dev = NULL;
	struct ntd_info *ntd = NULL;

	nftl_dev = (struct aml_nftl_dev *)aml_nftl_get_part_priv(part);
	ntd = nftl_dev->ntd;

	/* PRINT("p->phy_page.blkNO_in_chip %d\n",p->phy_page.blkNO_in_chip); */
	return ntd->erase(ntd, p->phy_page.blkNO_in_chip);
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int nand_read_page(struct aml_nftl_part_t *part, struct _physic_op_par *p)
{
	struct aml_nftl_dev *nftl_dev = NULL;
	struct ntd_info *ntd = NULL;
	loff_t from;
	int ret;

	nftl_dev = (struct aml_nftl_dev *)aml_nftl_get_part_priv(part);
	ntd = nftl_dev->ntd;

	from = p->phy_page.blkNO_in_chip;
	from <<= ntd->blocksize_shift;
	from >>= ntd->pagesize_shift;
	from += p->phy_page.Page_NO;

	aml_nftl_add_part_total_read(part);

	ret = ntd->read_page_with_oob(ntd, from,
		p->spare_data_addr,
		p->main_data_addr);
	if (ret == -EUCLEAN) {
		/* if (mtd->ecc_stats.corrected >= 10) */
			/* do read err */
		/* ret = 0; */
		PRINT("read reclaim\n");
	}

	if ((ret != 0) && (ret != -EUCLEAN)) {
		PRINT("aml_ops_read_page failed: %llx %d %d\n",
			from,
			p->phy_page.blkNO_in_chip,
			p->phy_page.Page_NO);
		ret =  -EBADMSG;
	}

	return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int nand_write_page(struct aml_nftl_part_t *part, struct _physic_op_par *p)
{
	struct aml_nftl_dev *nftl_dev = NULL;
	struct ntd_info *ntd = NULL;
	loff_t from;
	int ret;

	nftl_dev = (struct aml_nftl_dev *)aml_nftl_get_part_priv(part);
	ntd = nftl_dev->ntd;

	from = p->phy_page.blkNO_in_chip;
	from <<= ntd->blocksize_shift;
	from >>= ntd->pagesize_shift;
	from += p->phy_page.Page_NO;

	aml_nftl_add_part_total_write(part);

	ret = ntd->write_page_with_oob(ntd, from,
		p->spare_data_addr,
		p->main_data_addr);

	return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int nand_is_blk_good(struct aml_nftl_part_t *part, struct _physic_op_par *p)
{
	int ret;
	struct aml_nftl_dev *nftl_dev = NULL;
	struct ntd_info *ntd = NULL;

	nftl_dev = (struct aml_nftl_dev *)aml_nftl_get_part_priv(part);
	ntd = nftl_dev->ntd;

	ret = ntd->block_isbad(ntd, p->phy_page.blkNO_in_chip);
	if (ret == 0)
		return RET_YES;
	else if (ret == 1)
		return 100;
	else
		return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int nand_mark_bad_blk(struct aml_nftl_part_t *part, struct _physic_op_par *p)
{
	unchar ret;
	struct aml_nftl_dev *nftl_dev = NULL;
	struct ntd_info *ntd = NULL;

	nftl_dev = (struct aml_nftl_dev *)aml_nftl_get_part_priv(part);
	ntd = nftl_dev->ntd;

	ret = ntd->block_markbad(ntd, p->phy_page.blkNO_in_chip);
	if (ret == 0)
		return 0;
	else
		return 1;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/

