/*
 * drivers/amlogic/amlnf/phy/chipenv.c
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

#include "../include/phynand.h"


unsigned int aml_info_checksum(unsigned char *data, int lenth)
{
	unsigned int checksum;
	unsigned char *pdata;
	int i;

	checksum = 0;
	pdata = (unsigned char *)data;

	for (i = 0; i < lenth; i++)
		checksum += pdata[i];

	return checksum;
}

static int aml_info_check_datasum(void *data, unsigned char *name)
{
	int ret = 0;
	unsigned int crc = 0;
	struct block_status *blk_status = NULL;
	struct shipped_bbt *bbt = NULL;
	struct nand_config *config = NULL;
	struct phy_partition_info *phy_part = NULL;

	if (!memcmp(name, BBT_HEAD_MAGIC, 4)) {
		blk_status = (struct block_status *)data;
		crc = blk_status->crc;
		if (aml_info_checksum((unsigned char *)(blk_status->blk_status),
			(MAX_CHIP_NUM*MAX_BLK_NUM)) != crc) {
			aml_nand_msg("%s :nand bbt bad crc error", __func__);
			ret = -NAND_READ_FAILED;
		}
	}

	if (!memcmp(name, SHIPPED_BBT_HEAD_MAGIC, 4)) {
		bbt = (struct shipped_bbt *)data;
		crc = bbt->crc;
		if (aml_info_checksum((unsigned char *)(bbt->shipped_bbt),
			(MAX_CHIP_NUM*MAX_BAD_BLK_NUM)) != crc) {
			aml_nand_msg("%s : nand shipped bbt  bad crc error",
				__func__);
			ret = -NAND_READ_FAILED;
		}
	}

	if (!memcmp(name, CONFIG_HEAD_MAGIC, 4)) {
		config = (struct nand_config *)data;
		crc = config->crc;
		if (aml_info_checksum((unsigned char *)(config->dev_para),
			(MAX_DEVICE_NUM*sizeof(struct dev_para))) != crc) {
			aml_nand_msg("%s : nand check config crc error",
				__func__);
			ret = -NAND_READ_FAILED;
		}
	}

	if (!memcmp(name, PHY_PARTITION_HEAD_MAGIC, 4)) {
		phy_part = (struct phy_partition_info *)data;
		crc = phy_part->crc;
		if (aml_info_checksum((unsigned char *)(phy_part->partition),
		(MAX_DEVICE_NUM*sizeof(struct _phy_partition))) != crc) {
			aml_nand_msg("%s : nand check phy partition crc error",
				__func__);
			ret = -NAND_READ_FAILED;
		}
	}

	return ret;
}

int amlnand_free_block_test(struct amlnand_chip *aml_chip, int start_blk)
{
	struct hw_controller *controller = &aml_chip->controller;
	struct chip_operation *operation = &aml_chip->operation;
	struct chip_ops_para  *ops_para = &aml_chip->ops_para;
	struct nand_flash *flash = &aml_chip->flash;
	struct en_slc_info *slc_info = &(controller->slc_info);
	char block_invalid = 0;

	unsigned char phys_erase_shift, phys_page_shift, nand_boot;
	unsigned int offset, pages_per_blk, pages_read;
	unsigned char  oob_buf[8];
	unsigned short  tmp_blk;
	int  ret = 0, t = 0;
	unsigned int tmp_value;

	unsigned char *dat_buf = NULL;

	dat_buf  = aml_nand_malloc(flash->pagesize);
	if (!dat_buf) {
		aml_nand_msg("amlnand_free_block_test : malloc failed");
		block_invalid = 1;
		ret =  -1;
		goto exit;
	}
	memset(dat_buf, 0xa5, flash->pagesize);

	nand_boot = 1;

	/*
	if(boot_device_flag == 0)
		nand_boot = 0;
	*/

	if (nand_boot)
		offset = (1024 * flash->pagesize);
	else
		offset = 0;

	phys_erase_shift = ffs(flash->blocksize) - 1;
	phys_page_shift =  ffs(flash->pagesize) - 1;
	pages_per_blk = (1 << (phys_erase_shift - phys_page_shift));

	tmp_blk = (offset >> phys_erase_shift);

	if ((flash->new_type) && ((flash->new_type < 10)
		|| (flash->new_type == SANDISK_19NM)))
		ops_para->option |= DEV_SLC_MODE;

	if (ops_para->option & DEV_SLC_MODE)
		pages_read = pages_per_blk >> 1;
	else
		pages_read = pages_per_blk;

	if (aml_chip->state == CHIP_READY)
		nand_get_chip(aml_chip);

	/* erase */
	memset((unsigned char *)ops_para, 0x0, sizeof(struct chip_ops_para));
	tmp_value = start_blk - start_blk % controller->chip_num;
	tmp_value /= controller->chip_num;
	tmp_value += tmp_blk - tmp_blk/controller->chip_num;
	ops_para->page_addr =  tmp_value * pages_per_blk;
	ops_para->chipnr = start_blk % controller->chip_num;
	controller->select_chip(controller, ops_para->chipnr);
	ret = operation->erase_block(aml_chip);
	if (ret < 0) {
		aml_nand_msg("nand blk %d check good but erase failed",
			start_blk);
		block_invalid = 1;
		ret =  -1;
		goto exit;
	}

	/* write */
	for (t = 0; t < pages_read; t++) {
		memset((unsigned char *)ops_para,
			0x0,
			sizeof(struct chip_ops_para));
		if ((flash->new_type) && ((flash->new_type < 10)
			|| (flash->new_type == SANDISK_19NM)))
			ops_para->option |= DEV_SLC_MODE;
		tmp_value = start_blk - start_blk % controller->chip_num;
		tmp_value /= controller->chip_num;
		tmp_value += tmp_blk - tmp_blk/controller->chip_num;
		ops_para->page_addr = (t + tmp_value * pages_per_blk);
		ops_para->chipnr = start_blk % controller->chip_num;
		controller->select_chip(controller, ops_para->chipnr);

		if ((ops_para->option & DEV_SLC_MODE)) {
			tmp_value = ~(pages_per_blk - 1);
			tmp_value &= ops_para->page_addr;
			if ((flash->new_type > 0) && (flash->new_type < 10))
				ops_para->page_addr = tmp_value |
				(slc_info->pagelist[ops_para->page_addr % 256]);
			if (flash->new_type == SANDISK_19NM)
				ops_para->page_addr = tmp_value |
				((ops_para->page_addr % pages_per_blk) << 1);
		}

		memset(aml_chip->user_page_buf, 0xa5, flash->pagesize);
		ops_para->data_buf = aml_chip->user_page_buf;
		ops_para->oob_buf = aml_chip->user_oob_buf;
		ops_para->ooblen = sizeof(oob_buf);

		ret = operation->write_page(aml_chip);
		if (ret < 0) {
			aml_nand_msg("nand write failed");
			block_invalid = 1;
			ret =  -1;
			goto exit;
		}
	}
	/* read */
	for (t = 0; t < pages_read; t++) {
		memset((unsigned char *)ops_para,
			0x0,
			sizeof(struct chip_ops_para));
		if ((flash->new_type) && ((flash->new_type < 10)
			|| (flash->new_type == SANDISK_19NM)))
			ops_para->option |= DEV_SLC_MODE;
		tmp_value = start_blk - start_blk % controller->chip_num;
		tmp_value /= controller->chip_num;
		tmp_value += tmp_blk - tmp_blk/controller->chip_num;
		ops_para->page_addr = (t + tmp_value * pages_per_blk);
		ops_para->chipnr = start_blk % controller->chip_num;
		controller->select_chip(controller, ops_para->chipnr);

		if ((ops_para->option & DEV_SLC_MODE)) {
			tmp_value = ~(pages_per_blk - 1);
			tmp_value &= ops_para->page_addr;
			if ((flash->new_type > 0) && (flash->new_type < 10))
				ops_para->page_addr = tmp_value |
				(slc_info->pagelist[ops_para->page_addr % 256]);
			if (flash->new_type == SANDISK_19NM)
				ops_para->page_addr = tmp_value |
				((ops_para->page_addr % pages_per_blk) << 1);
		}

		memset(aml_chip->user_page_buf, 0x0, flash->pagesize);
		ops_para->data_buf = aml_chip->user_page_buf;
		ops_para->oob_buf = aml_chip->user_oob_buf;
		ops_para->ooblen = sizeof(oob_buf);

		ret = operation->read_page(aml_chip);
		if (ret < 0) {
			aml_nand_msg("nand write failed");
			block_invalid = 1;
			ret =  -1;
			goto exit;
		}
		aml_nand_dbg("start_blk %d aml_chip->user_page_buf: ",
			start_blk);
		/* show_data_buf(aml_chip->user_page_buf); */
		aml_nand_dbg("start_blk %d dat_buf: ", start_blk);
		/* show_data_buf(dat_buf); */
		if (memcmp(aml_chip->user_page_buf,
			dat_buf,
			flash->pagesize)) {
			block_invalid = 1;
			ret =  -1;
			aml_nand_msg("free blk  %d,  page %d : test failed",
				start_blk,
				t);
			goto exit;
		}
	}

exit:
	if (aml_chip->state == CHIP_READY)
		nand_release_chip(aml_chip);

	if (dat_buf) {
		aml_nand_free(dat_buf);
		dat_buf = NULL;
	}

	if (!ret)
		aml_nand_msg("free blk start_blk %d test OK", start_blk);

	return ret;
}

int get_last_reserve_block(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &aml_chip->controller;
	struct chip_operation *operation = &aml_chip->operation;
	struct chip_ops_para  *ops_para = &aml_chip->ops_para;
	struct nand_flash *flash = &aml_chip->flash;

	u32 offset, start_blk, blk_addr, tmp_blk, pages_per_blk;
	unsigned char phys_erase_shift, phys_page_shift;
	int  ret = 0;
	unsigned int tmp_value;
	static u32 total_blk, scan_flag;/*add 150922*/

	if ((total_blk > RESERVED_BLOCK_CNT) &&	(scan_flag == 1))
		return total_blk;

	if (aml_chip->nand_bbtinfo.arg_valid)
		scan_flag = 1;

	offset = (1024 * flash->pagesize);

	phys_erase_shift = ffs(flash->blocksize) - 1;
	phys_page_shift =  ffs(flash->pagesize) - 1;
	pages_per_blk = (1 << (phys_erase_shift - phys_page_shift));
	memset((unsigned char *)ops_para, 0x0, sizeof(struct chip_ops_para));

	start_blk = (offset >> phys_erase_shift);
	tmp_blk = total_blk = start_blk;

	blk_addr = 0;
	/* decide the total block_addr */
	while (blk_addr < RESERVED_BLOCK_CNT) {
		memset((unsigned char *)ops_para,
			0x0,
			sizeof(struct chip_ops_para));
		tmp_value = total_blk - total_blk % controller->chip_num;
		tmp_value /= controller->chip_num;
		tmp_value += tmp_blk - tmp_blk/controller->chip_num;
		ops_para->page_addr = tmp_value * pages_per_blk;
		ops_para->chipnr = total_blk % controller->chip_num;
		controller->select_chip(controller, ops_para->chipnr);

		ret = operation->block_isbad(aml_chip);
		if (ret ==  NAND_BLOCK_FACTORY_BAD) {
			aml_nand_msg("nand blk %d is shipped bad block ",
				total_blk);
			total_blk++;
			continue;
		}
		total_blk++;
		blk_addr++;
	}

	return total_blk;
}

int repair_reserved_bad_block(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &aml_chip->controller;
	struct chip_operation *operation = &aml_chip->operation;
	struct chip_ops_para  *ops_para = &aml_chip->ops_para;
	struct nand_flash *flash = &aml_chip->flash;
	unsigned int offset, start_blk, total_blk, blk_addr, tmp_blk;
	unsigned int pages_per_blk, blk_used_bad_cnt = 0;
	unsigned char phys_erase_shift, phys_page_shift;
	int  ret = 0, i = 0, j = 0;
	uint bad_blk[128];
	unsigned char *dat_buf = NULL;
	unsigned char *oob_buf = NULL;
	unsigned int tmp_value;

	memset(bad_blk, 0, 128*sizeof(uint));
	offset = (1024 * flash->pagesize);

	phys_erase_shift = ffs(flash->blocksize) - 1;
	phys_page_shift =  ffs(flash->pagesize) - 1;
	pages_per_blk = (1 << (phys_erase_shift - phys_page_shift));
	memset((unsigned char *)ops_para, 0x0, sizeof(struct chip_ops_para));

	start_blk = (offset >> phys_erase_shift);
	tmp_blk = total_blk = start_blk;

	dat_buf  = aml_nand_malloc(flash->pagesize);
	if (!dat_buf) {
		aml_nand_msg("amlnand_free_block_test : malloc failed");
		ret = -1;
		return ret;
	}

	memset(dat_buf, 0, flash->pagesize);
	oob_buf  = aml_nand_malloc(flash->oobsize);
	if (!oob_buf) {
		aml_nand_msg("amlnand_free_block_test : malloc failed");
		ret = -1;
		kfree(dat_buf);

		return ret;
	}
	memset(oob_buf, 0, flash->oobsize);

	blk_addr = 0;
	/* decide the total block_addr */
	while (blk_addr < RESERVED_BLOCK_CNT) {
		memset((unsigned char *)ops_para,
			0x0,
			sizeof(struct chip_ops_para));
		tmp_value = total_blk - total_blk % controller->chip_num;
		tmp_value /= controller->chip_num;
		tmp_value += tmp_blk - tmp_blk/controller->chip_num;
		ops_para->page_addr = tmp_value * pages_per_blk;
		ops_para->chipnr = total_blk % controller->chip_num;
		controller->select_chip(controller, ops_para->chipnr);

		ret = operation->block_isbad(aml_chip);
		if (ret ==  NAND_BLOCK_FACTORY_BAD) {
			aml_nand_msg("nand blk %d is shipped bad block ",
				total_blk);
			total_blk++;
			continue;
		}
		if (ret == NAND_BLOCK_USED_BAD) {
			if (blk_used_bad_cnt < 128) {
				bad_blk[blk_used_bad_cnt] = total_blk;
				blk_used_bad_cnt++;
			}
		}
		total_blk++;
		blk_addr++;
	}

	if (blk_used_bad_cnt > 6) {
		nand_get_chip(aml_chip);
		aml_nand_msg("repair badblk of reserved,blk_used_bad_cnt=%d\n",
			blk_used_bad_cnt);
		for (i = 0; i < blk_used_bad_cnt; i++) {
			memset((unsigned char *)ops_para,
				0x0,
				sizeof(struct chip_ops_para));
			tmp_value = bad_blk[i]-bad_blk[i]%controller->chip_num;
			tmp_value /= controller->chip_num;
			tmp_value += tmp_blk - tmp_blk/controller->chip_num;
			ops_para->page_addr = tmp_value * pages_per_blk;
			ops_para->chipnr = bad_blk[i] % controller->chip_num;
			controller->select_chip(controller, ops_para->chipnr);
			ret = operation->blk_modify_bbt_chip_op(aml_chip, 0);
			/* erase */
			ret = operation->erase_block(aml_chip);
			if (ret) {
				ret = operation->blk_modify_bbt_chip_op(
					aml_chip,
					1);
				aml_nand_msg("test blk %d fail\n", bad_blk[i]);
				continue;
			}
			/* write */
			ops_para->page_addr = tmp_value * pages_per_blk;
			ops_para->data_buf = dat_buf;
			ops_para->oob_buf = oob_buf;
			for (j = 0; j < pages_per_blk; j++) {
				ops_para->page_addr += 1;
				memset(dat_buf, 0, flash->pagesize);
				memset(oob_buf, 0, flash->oobsize);
				ret = operation->write_page(aml_chip);
				if (ret) {
					ops_para->page_addr =
						tmp_value * pages_per_blk;
					ret =
					operation->blk_modify_bbt_chip_op(
						aml_chip, 1);
					aml_nand_msg("test blk %d fail\n",
						bad_blk[i]);
					goto write_read_fail;
				}
			}
			/* read */
			ops_para->page_addr = tmp_value * pages_per_blk;
			ops_para->data_buf = dat_buf;
			ops_para->oob_buf = oob_buf;
			for (j = 0; j < pages_per_blk; j++) {
				ops_para->page_addr += 1;
				memset(dat_buf, 0, flash->pagesize);
				memset(oob_buf, 0, flash->oobsize);
				ret = operation->read_page(aml_chip);
				if ((ops_para->ecc_err) || (ret < 0)) {
					ops_para->page_addr =
						tmp_value * pages_per_blk;
					ret =
					operation->blk_modify_bbt_chip_op(
						aml_chip, 1);
					aml_nand_msg("test blk %d fail\n",
						bad_blk[i]);
					goto write_read_fail;
				}
			}
			/* erase */
			ops_para->page_addr = tmp_value * pages_per_blk;
			ret = operation->erase_block(aml_chip);
			if (ret) {
				ret = operation->blk_modify_bbt_chip_op(
					aml_chip, 1);
				aml_nand_msg("test blk %d fail\n", bad_blk[i]);
				continue;
			}
			aml_nand_msg("test blk %d OK\n", bad_blk[i]);
write_read_fail:
			;
		}
		nand_release_chip(aml_chip);
		amlnand_update_bbt(aml_chip);
	}
	kfree(dat_buf);

	kfree(oob_buf);

	return total_blk;
}
/*****************************************************************************
*Name         :amlnand_get_free_block
*Description :search a good block by skip the shipped bad block
*Parameter  :
*Return       :
*Note          :
*****************************************************************************/
int amlnand_get_free_block(struct amlnand_chip *aml_chip, unsigned int block)
{
	struct hw_controller *controller = &aml_chip->controller;
	struct chip_operation *operation = &aml_chip->operation;
	struct chip_ops_para  *ops_para = &aml_chip->ops_para;
	struct nand_flash *flash = &aml_chip->flash;

	unsigned int offset, nand_boot, start_blk, total_blk;
	unsigned int blk_addr, tmp_blk, pages_per_blk;
	unsigned char phys_erase_shift, phys_page_shift;
	unsigned char  blk_used_flag = 0;
	int  ret = 0, i;
	unsigned int tmp_value;

	nand_boot = 1;

	/*if(boot_device_flag == 0) {
		nand_boot = 0;
	}*/
	if (nand_boot)
		offset = (1024 * flash->pagesize);
	else
		offset = 0;

	phys_erase_shift = ffs(flash->blocksize) - 1;
	phys_page_shift =  ffs(flash->pagesize) - 1;
	pages_per_blk = (1 << (phys_erase_shift - phys_page_shift));
	memset((unsigned char *)ops_para, 0x0, sizeof(struct chip_ops_para));

	start_blk = (offset >> phys_erase_shift);
	tmp_blk = total_blk = start_blk;

	total_blk = get_last_reserve_block(aml_chip);
	blk_addr = 0;

	while ((blk_addr < 1) && (start_blk < total_blk)) {
		for (i = 0; i < RESERVED_BLOCK_CNT;  i++) {
			if (aml_chip->reserved_blk[i] == start_blk) {
				/*
				aml_nand_msg("nand blk %d is used", start_blk);
				*/
				blk_used_flag = 1;
				break;
			} else
				blk_used_flag = 0;
		}

		if (blk_used_flag) {
			start_blk++;
			continue;
		}

		if (block == start_blk) {
			start_blk++;
			continue;
		}

		memset((unsigned char *)ops_para,
			0x0,
			sizeof(struct chip_ops_para));
		tmp_value = start_blk - start_blk % controller->chip_num;
		tmp_value /= controller->chip_num;
		tmp_value += tmp_blk - tmp_blk/controller->chip_num;
		ops_para->page_addr = tmp_value * pages_per_blk;
		ops_para->chipnr = start_blk % controller->chip_num;
		controller->select_chip(controller, ops_para->chipnr);

		ret = operation->block_isbad(aml_chip);
		if (ret == NAND_BLOCK_FACTORY_BAD) {
			aml_nand_msg("nand blk %d is shipped bad block ",
				start_blk);
			start_blk++;
			continue;
		}
		if (ret == NAND_BLOCK_USED_BAD) {
			aml_nand_msg("blk %d is used bad block ",
				start_blk);
			start_blk++;
			continue;
		}

		/*
		ret = amlnand_free_block_test(aml_chip, start_blk);
		if(ret){
			aml_nand_msg("nand get free block  %d invalid",
				start_blk);
			start_blk++;
			continue;
		}*/

		if (aml_chip->state == CHIP_READY)
			nand_get_chip(aml_chip);
		ret = operation->erase_block(aml_chip);
		if (aml_chip->state == CHIP_READY)
			nand_release_chip(aml_chip);
		if (ret < 0) {
			aml_nand_msg("nand blk %d check good but erase failed",
				start_blk);
			ret = operation->block_markbad(aml_chip);
			start_blk++;
			continue;
		} else
			aml_nand_dbg("nand get free block at %d", start_blk);

		blk_addr++;
	}

	if (start_blk >= total_blk) {
		ret = -NAND_BAD_BLCOK_FAILURE;
		aml_nand_msg("nand can not find free block");
	}

	return (ret == 0) ? start_blk : ret;
}

void amlnand_info_error_handle(struct amlnand_chip *aml_chip)
{
	struct nand_arg_info *nand_bbt = &aml_chip->nand_bbtinfo;
	struct nand_arg_info *nand_config = &aml_chip->config_msg;
	int ret = 0;

	if ((nand_bbt->arg_valid) && (nand_bbt->update_flag)) {
		/* aml_nand_msg("amlnand_info_error_handle : update bbt"); */
		ret = amlnand_update_bbt(aml_chip);
		nand_bbt->update_flag = 0;
		aml_nand_msg("NAND UPDATE CKECK : arg %s:", "bbt");
		aml_nand_msg("arg_valid=%d,valid_blk_addr=%d,valid_pg_addr=%d",
			nand_bbt->arg_valid,
			nand_bbt->valid_blk_addr,
			nand_bbt->valid_page_addr);
		if (ret) {
			aml_nand_msg("%s: nand update bbt failed", __func__);
			return;
		}
	}

	if ((nand_config->arg_valid) && (nand_config->update_flag)) {
		/*
		aml_nand_msg("amlnand_info_error_handle : update nand config");
		*/
		aml_chip->config_ptr->crc =
			aml_info_checksum(
			(unsigned char *)(aml_chip->config_ptr->dev_para),
			(MAX_DEVICE_NUM*sizeof(struct dev_para)));
		 ret = amlnand_save_info_by_name(aml_chip,
			(unsigned char *) &(aml_chip->config_msg),
			(unsigned char *)aml_chip->config_ptr,
			CONFIG_HEAD_MAGIC,
			sizeof(struct nand_config));
		nand_config->update_flag = 0;
		aml_nand_msg("NAND UPDATE CKECK: arg %s:", "config");
		aml_nand_msg("arg_valid=%d,valid_blk_addr=%d,valid_pg_addr=%d",
			nand_config->arg_valid,
			nand_config->valid_blk_addr,
			nand_config->valid_page_addr);
		if (ret < 0) {
			aml_nand_msg("save nand dev_configs failed and ret:%d",
				ret);
			return;
		}
	}

	return;
}

int amlnand_read_info_by_name(struct amlnand_chip *aml_chip,
	unsigned char *info,
	unsigned char *buf,
	unsigned char *name,
	unsigned int size)
{
	struct hw_controller *controller = &aml_chip->controller;
	struct nand_flash *flash = &aml_chip->flash;
	struct chip_operation *operation = &aml_chip->operation;
	struct chip_ops_para  *ops_para = &aml_chip->ops_para;
	struct en_slc_info *slc_info = &(controller->slc_info);
	struct nand_arg_info *arg_info = (struct nand_arg_info *)info;
	struct nand_arg_oobinfo *arg_oob_info;

	unsigned char phys_erase_shift, phys_page_shift, nand_boot;
	unsigned int offset, offset_tmp;
	unsigned int pages_per_blk, pages_read, amount_loaded = 0;
	unsigned char oob_buf[sizeof(struct nand_arg_oobinfo)];
	unsigned short start_blk, total_blk, tmp_blk;
	int  ret = 0, len;
	unsigned int tmp_value, tmp_index;

	nand_boot = 1;

	/*if(boot_device_flag == 0){
		nand_boot = 0;
	}*/

	if (nand_boot)
		offset = (1024 * flash->pagesize);
	else
		offset = 0;

	arg_oob_info = (struct nand_arg_oobinfo *)oob_buf;
	memset((unsigned char *)ops_para, 0x0, sizeof(struct chip_ops_para));

	phys_erase_shift = ffs(flash->blocksize) - 1;
	phys_page_shift =  ffs(flash->pagesize) - 1;
	pages_per_blk = (1 << (phys_erase_shift - phys_page_shift));

	if ((flash->new_type)
		&& ((flash->new_type < 10)
		|| (flash->new_type == SANDISK_19NM)))
		ops_para->option |= DEV_SLC_MODE;

	if (ops_para->option & DEV_SLC_MODE)
		pages_read = pages_per_blk >> 1;
	else
		pages_read = pages_per_blk;

	start_blk = (offset >> phys_erase_shift);
	tmp_blk = start_blk;
	total_blk = (offset >> phys_erase_shift) + RESERVED_BLOCK_CNT;

	if (arg_info->arg_valid == 1) {
		/* load bbt */
		offset_tmp = 0;
		while (amount_loaded < size) {
			memset((unsigned char *)ops_para,
				0x0,
				sizeof(struct chip_ops_para));
			if ((flash->new_type)
				&& ((flash->new_type < 10)
				|| (flash->new_type == SANDISK_19NM)))
				ops_para->option |= DEV_SLC_MODE;

			ops_para->data_buf = aml_chip->user_page_buf;
			ops_para->oob_buf = aml_chip->user_oob_buf;
			ops_para->ooblen = sizeof(oob_buf);
			memset((unsigned char *)ops_para->data_buf,
				0x0, flash->pagesize);
			memset((unsigned char *)ops_para->oob_buf,
				0x0, sizeof(oob_buf));

			tmp_value = arg_info->valid_blk_addr;
			tmp_value = tmp_value-tmp_value % controller->chip_num;
			tmp_value /= controller->chip_num;
			tmp_value += tmp_blk - tmp_blk/controller->chip_num;
			ops_para->page_addr = (arg_info->valid_page_addr +
				tmp_value * pages_per_blk) + offset_tmp;

			if ((ops_para->option & DEV_SLC_MODE)) {
				tmp_value = ~(pages_per_blk - 1);
				tmp_value &= ops_para->page_addr;
				if ((flash->new_type > 0)
					&& (flash->new_type < 10)) {
					tmp_index = ops_para->page_addr % 256;
					ops_para->page_addr = tmp_value |
						slc_info->pagelist[tmp_index];
				}
				if (flash->new_type == SANDISK_19NM)
					ops_para->page_addr = tmp_value |
						((ops_para->page_addr %
						pages_per_blk) << 1);
			}

			ops_para->chipnr =
				arg_info->valid_blk_addr % controller->chip_num;
			controller->select_chip(controller, ops_para->chipnr);

			if (aml_chip->state == CHIP_READY)
				nand_get_chip(aml_chip);

			ret = operation->read_page(aml_chip);

			if (aml_chip->state == CHIP_READY)
				nand_release_chip(aml_chip);

			if ((ops_para->ecc_err) || (ret < 0)) {
				aml_nand_msg("read arg %s fail,chip%d page=%d",
					name,
					ops_para->chipnr,
					ops_para->page_addr);
				goto exit_error0;
			}

			memcpy((unsigned char *)arg_oob_info,
				aml_chip->user_oob_buf,
				sizeof(oob_buf));
			if (!memcmp(arg_oob_info->name, name, 4)) {
				len = min(flash->pagesize, size-amount_loaded);
				memcpy(((u8 *)(buf)+amount_loaded),
					(u8 *)aml_chip->user_page_buf,
					len);
			}
			offset_tmp += 1;
			amount_loaded += flash->pagesize;
		}
	}

	return ret;
exit_error0:
	return ret;
}

int amlnand_save_info_by_name(struct amlnand_chip *aml_chip,
	unsigned char *info,
	unsigned char *buf,
	unsigned char *name,
	unsigned int size)
{
	struct hw_controller *controller = &aml_chip->controller;
	struct nand_flash *flash = &aml_chip->flash;
	struct chip_operation *operation = &aml_chip->operation;
	struct chip_ops_para  *ops_para = &aml_chip->ops_para;
	struct en_slc_info *slc_info = &(controller->slc_info);
	struct nand_arg_info *arg_info = (struct nand_arg_info *)info;
	struct nand_arg_oobinfo *arg_oob_info;
	unsigned int len, offset, offset_tmp, nand_boot, blk_addr = 0;
	unsigned int tmp_blk_addr, amount_saved = 0;
	unsigned int pages_per_blk, arg_pages, pages_read;
	unsigned char phys_erase_shift, phys_page_shift;
	unsigned char oob_buf[sizeof(struct nand_arg_oobinfo)];
	unsigned short tmp_blk;
	unsigned int tmp_addr, temp_option;
	unsigned char temp_ran_mode;
	int full_page_flag = 0, ret = 0, i, test_cnt = 0;
	int extra_page = 0, write_page_cnt = 0, temp_page_num = 0;
	unsigned int tmp_value, index;

	nand_boot = 1;

	/*if(boot_device_flag == 0){
		nand_boot = 0;
	}	*/

	if (nand_boot)
		offset = (1024 * flash->pagesize);
	else
		offset = 0;

	phys_erase_shift = ffs(flash->blocksize) - 1;
	phys_page_shift =  ffs(flash->pagesize) - 1;
	pages_per_blk = (1 << (phys_erase_shift - phys_page_shift));

	arg_pages = ((size>>phys_page_shift) + 1);
	if ((size%flash->pagesize) == 0)
		extra_page = 1;
	else
		extra_page = 0;

	aml_nand_msg("size = %d arg_pages = %d extra_page = %d",
		size,
		arg_pages,
		extra_page);

	tmp_blk = (offset >> phys_erase_shift);

	if ((flash->new_type) && ((flash->new_type < 10)
		|| (flash->new_type == SANDISK_19NM)))
		ops_para->option |= DEV_SLC_MODE;

	if (ops_para->option & DEV_SLC_MODE)
		pages_read = pages_per_blk >> 1;
	else
		pages_read = pages_per_blk;

write_again:
	arg_oob_info = (struct nand_arg_oobinfo *) oob_buf;
	arg_info->timestamp += 1;
	arg_oob_info->timestamp = arg_info->timestamp;

	memcpy(arg_oob_info->name, name, 4);
	memset((unsigned char *)ops_para, 0x0, sizeof(struct chip_ops_para));

#if 0
	aml_nand_dbg(" buf: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
		bbt_ptr[0], bbt_ptr[1], bbt_ptr[2], bbt_ptr[3], bbt_ptr[4],
		bbt_ptr[5], bbt_ptr[6], bbt_ptr[7], bbt_ptr[8], bbt_ptr[9],
		bbt_ptr[10], bbt_ptr[11], bbt_ptr[12], bbt_ptr[13],
		bbt_ptr[14], bbt_ptr[15]);

	aml_nand_dbg(" oob_buf: %x %x %x %x %x %x %x %x",
		oob_buf[0], oob_buf[1], oob_buf[2], oob_buf[3],
		oob_buf[4], oob_buf[5], oob_buf[6], oob_buf[7]);

	aml_nand_dbg(" bbt_oob_info: %x %x %x %x %x %x %x %x",
		bbt_oob_info[0], bbt_oob_info[1], bbt_oob_info[2],
		bbt_oob_info[3], bbt_oob_info[4], bbt_oob_info[5],
		bbt_oob_info[6], bbt_oob_info[7]);
#endif

get_free_blk:
	/* get new block according to arg_type or update_flag */
	if ((arg_info->arg_valid)
		&& (!arg_info->update_flag)
		&& (arg_info->arg_type == FULL_PAGE)) {
		if ((arg_info->valid_page_addr + 2 * arg_pages) > pages_read) {
			ret = amlnand_get_free_block(aml_chip, blk_addr);
			blk_addr = ret;
			if (ret < 0) {
				aml_nand_msg("nand get free blcok failed");
				ret = -NAND_BAD_BLCOK_FAILURE;
				goto exit_error0;
			}
			aml_nand_dbg("nand get free blcok  at %d", blk_addr);
			full_page_flag = 1;
		} else
			blk_addr = arg_info->valid_blk_addr;
	} else {
		ret = amlnand_get_free_block(aml_chip, blk_addr);
		blk_addr = ret;
		if (ret < 0) {
			aml_nand_msg("nand get free blcok failed");
			ret = -NAND_BAD_BLCOK_FAILURE;
			goto exit_error0;
		}
		aml_nand_dbg("nand get free blcok  at %d", blk_addr);
	}

	/* show_data_buf(buf); */
	if (arg_info->arg_type == FULL_BLK) {
		for (i = 0; i < pages_read;) {
			if ((pages_read - i) < arg_pages) {
				if (flash->new_type == HYNIX_1YNM) {
					/*
					for slc mode, if not full block write,
					need write dummy random data to lock
					data
					*/
					/*
					write dummy page
					*/
					memset((u8 *)ops_para,
						0x0,
						sizeof(struct chip_ops_para));
					ops_para->option |= DEV_SLC_MODE;

					tmp_value = blk_addr;
					tmp_value /= controller->chip_num;
					tmp_value += tmp_blk -
						tmp_blk/controller->chip_num;
					tmp_value *= pages_per_blk;
					ops_para->page_addr = tmp_value + i;
					tmp_value = ~(pages_per_blk - 1);
					tmp_value &= ops_para->page_addr;
					index = ops_para->page_addr % 256;
					ops_para->page_addr = tmp_value |
						(slc_info->pagelist[index]);
					ops_para->chipnr =
						blk_addr % controller->chip_num;
					controller->select_chip(controller,
						ops_para->chipnr);
					ops_para->data_buf =
						aml_chip->user_page_buf;
					ops_para->oob_buf =
						aml_chip->user_oob_buf;
					ops_para->ooblen = sizeof(oob_buf);
					memset(aml_chip->user_oob_buf, 0x5a,
						sizeof(oob_buf));
					memset(aml_chip->user_page_buf, 0x5a,
						flash->pagesize);

					if (aml_chip->state == CHIP_READY)
						nand_get_chip(aml_chip);

			aml_nand_msg("dummy random data,i=%d pg_addr=%x",
						i,
						ops_para->page_addr);
					ret = operation->write_page(aml_chip);
					if (aml_chip->state == CHIP_READY)
						nand_release_chip(aml_chip);

					/* write dummy page paried */
					tmp_addr = ops_para->page_addr;
					temp_ran_mode = controller->ran_mode;
					temp_option = ops_para->option;
					controller->ran_mode = 0;
					ops_para->page_addr += 1;
			aml_nand_msg("dummy random paired data,i=%d pg_addr=%x",
						i,
						ops_para->page_addr);
					ops_para->option |= DEV_ECC_SOFT_MODE;
					ops_para->option &=
						DEV_SERIAL_CHIP_MODE;
					memset(aml_chip->user_page_buf, 0xff,
						flash->pagesize);
					memset(aml_chip->user_oob_buf, 0xff,
						sizeof(oob_buf));

					if (aml_chip->state == CHIP_READY)
						nand_get_chip(aml_chip);

					ret = operation->write_page(aml_chip);

					if (aml_chip->state == CHIP_READY)
						nand_release_chip(aml_chip);

					controller->ran_mode = temp_ran_mode;
					ops_para->page_addr = tmp_addr;
					ops_para->option = temp_option;
				}
				break;
			}
			offset_tmp = 0;
			amount_saved = 0;
			while (amount_saved <
				(size+extra_page*flash->pagesize)) {
				memset((unsigned char *)ops_para, 0x0,
					sizeof(struct chip_ops_para));
				if ((flash->new_type)
					&& ((flash->new_type < 10)
					|| (flash->new_type == SANDISK_19NM)))
					ops_para->option |= DEV_SLC_MODE;

				tmp_value = blk_addr;
				tmp_value /= controller->chip_num;
				tmp_value += tmp_blk -
					tmp_blk/controller->chip_num;
				tmp_value *= pages_per_blk;
				tmp_value += offset_tmp + i;
				ops_para->page_addr = tmp_value;

				if ((ops_para->option & DEV_SLC_MODE)) {
					tmp_value = ops_para->page_addr;
					tmp_value &= (~(pages_per_blk - 1));
					if ((flash->new_type > 0)
						&& (flash->new_type < 10)) {
						index =
						ops_para->page_addr % 256;
						ops_para->page_addr =
						tmp_value |
						(slc_info->pagelist[index]);
					}
					if (flash->new_type == SANDISK_19NM) {
						index = ops_para->page_addr;
						index %= pages_per_blk;
						index <<= 0x01;
						ops_para->page_addr =
							tmp_value | index;
					}
				}

				ops_para->chipnr =
					blk_addr % controller->chip_num;
				controller->select_chip(controller,
					ops_para->chipnr);
				ops_para->data_buf = aml_chip->user_page_buf;
				ops_para->oob_buf = aml_chip->user_oob_buf;
				ops_para->ooblen = sizeof(oob_buf);

				len = min(flash->pagesize,
					size +
					extra_page*flash->pagesize -
					amount_saved);
				memset(aml_chip->user_page_buf,
					0x0,
					flash->pagesize);
				memset(aml_chip->user_oob_buf,
					0x0,
					sizeof(oob_buf));
				memcpy((u8 *)aml_chip->user_page_buf,
					((u8 *)(buf) + amount_saved),
					len);
				memcpy(aml_chip->user_oob_buf,
					(u8 *)arg_oob_info,
					sizeof(oob_buf));
#if 0
			aml_nand_dbg("oob_buf:%x %x %x %x %x %x %x %x",
				ops_para->oob_buf[0], ops_para->oob_buf[1],
				ops_para->oob_buf[2], ops_para->oob_buf[3],
				ops_para->oob_buf[4], ops_para->oob_buf[5],
				ops_para->oob_buf[6], ops_para->oob_buf[7]);
#endif
				if (aml_chip->state == CHIP_READY)
					nand_get_chip(aml_chip);

				ret = operation->write_page(aml_chip);
				if (aml_chip->state == CHIP_READY)
					nand_release_chip(aml_chip);

				if (ret < 0) {
					aml_nand_msg("nand write failed");
					if (test_cnt >= 3) {
						aml_nand_msg("test 3 times");
						break;
					}
				ret = operation->test_block_reserved(aml_chip,
					blk_addr);
					test_cnt++;
			if (ret) {
				ret = operation->block_markbad(aml_chip);
				if (ret < 0)
					aml_nand_msg("nand mark bad blk=%d",
						blk_addr);
			}
					aml_nand_msg("rewrite!");
					goto get_free_blk;
				}
				/*for slc mode*/
				if (flash->new_type == HYNIX_1YNM) {
					temp_page_num = offset_tmp + i;
				if (temp_page_num >= 1) {
					ops_para->chipnr =
						blk_addr % controller->chip_num;
					controller->select_chip(controller,
						ops_para->chipnr);

					tmp_addr = ops_para->page_addr;
					temp_ran_mode = controller->ran_mode;
					temp_option = ops_para->option;
					controller->ran_mode = 0;
					ops_para->page_addr += 1;
					ops_para->option |= DEV_ECC_SOFT_MODE;
					ops_para->option &=
						DEV_SERIAL_CHIP_MODE;
					memset(aml_chip->user_page_buf, 0xff,
						flash->pagesize);
					memset(aml_chip->user_oob_buf, 0xff,
						sizeof(oob_buf));

					if (aml_chip->state == CHIP_READY)
						nand_get_chip(aml_chip);
			/*
			aml_nand_msg("normal blk write,pgnum=%d pgaddr=%x",
						temp_page_num,
						ops_para->page_addr);
			*/
						ret =
						operation->write_page(aml_chip);

					if (aml_chip->state == CHIP_READY)
						nand_release_chip(aml_chip);

					controller->ran_mode = temp_ran_mode;
					ops_para->page_addr = tmp_addr;
					ops_para->option = temp_option;
					/*
					ops_para->option &= DEV_ECC_HW_MODE;
					*/
					/* ops_para->option |=DEV_SLC_MODE; */
				}
				}

				offset_tmp += 1;
				amount_saved += flash->pagesize;
			}
			i += arg_pages;
			if (ret < 0)
				break;
		}
	} else if (arg_info->arg_type == FULL_PAGE) {
		offset_tmp = 0;
		amount_saved = 0;
		while (amount_saved < size+extra_page*flash->pagesize) {
			memset((unsigned char *)ops_para, 0x0,
				sizeof(struct chip_ops_para));
			if ((flash->new_type)
				&& ((flash->new_type < 10)
				|| (flash->new_type == SANDISK_19NM)))
				ops_para->option |= DEV_SLC_MODE;
			tmp_value = blk_addr;
			tmp_value /= controller->chip_num;
			tmp_value += tmp_blk - tmp_blk/controller->chip_num;
			tmp_value *= pages_per_blk;
			ops_para->page_addr = tmp_value + offset_tmp;
			if (arg_info->arg_valid
				&& (!full_page_flag)
				&& (!arg_info->update_flag))
				ops_para->page_addr +=
					(arg_info->valid_page_addr + arg_pages);

			/*for slc mode*/
			if (flash->new_type == HYNIX_1YNM) {
				if (arg_info->arg_valid
					&& (!full_page_flag)
					&& (!arg_info->update_flag))
					ops_para->page_addr += 1;
					temp_page_num =
					ops_para->page_addr % 256;
			}

			if ((ops_para->option & DEV_SLC_MODE)) {
				tmp_value = ops_para->page_addr;
				tmp_value &= (~(pages_per_blk - 1));
				if ((flash->new_type > 0)
					&& (flash->new_type < 10)) {
					index = ops_para->page_addr % 256;
					ops_para->page_addr = tmp_value |
						(slc_info->pagelist[index]);
				}
				if (flash->new_type == SANDISK_19NM) {
					index = ops_para->page_addr;
					index %= pages_per_blk;
					index <<= 0x01;
					ops_para->page_addr = tmp_value | index;
				}
			}

			ops_para->chipnr = blk_addr % controller->chip_num;
			controller->select_chip(controller, ops_para->chipnr);

			ops_para->data_buf = aml_chip->user_page_buf;
			ops_para->oob_buf = aml_chip->user_oob_buf;
			ops_para->ooblen = sizeof(oob_buf);

			len = min(flash->pagesize,
			size + extra_page*flash->pagesize - amount_saved);
			memset(aml_chip->user_page_buf, 0x0, flash->pagesize);
			memset(aml_chip->user_oob_buf, 0x0, sizeof(oob_buf));
			memcpy((unsigned char *)aml_chip->user_page_buf,
				((unsigned char *)(buf) + amount_saved), len);
			memcpy(aml_chip->user_oob_buf,
				(unsigned char *)arg_oob_info, sizeof(oob_buf));

			if (aml_chip->state == CHIP_READY)
				nand_get_chip(aml_chip);

			ret = operation->write_page(aml_chip);

			if (aml_chip->state == CHIP_READY)
				nand_release_chip(aml_chip);

			if (ret < 0) {
				aml_nand_msg("nand write failed");
				if (test_cnt >= 3) {
					aml_nand_msg("test blk 3times");
					break;
				}
				ret = operation->test_block_reserved(aml_chip,
					blk_addr);
				test_cnt++;
				if (ret) {
					ret =
					operation->block_markbad(aml_chip);
					if (ret < 0)
						aml_nand_msg("mark bad blk %d",
							blk_addr);
				}
				goto get_free_blk;
			}

			/*for slc mode*/
			if (flash->new_type == HYNIX_1YNM) {
				if (temp_page_num >= 1) {
					ops_para->chipnr =
						blk_addr % controller->chip_num;
					controller->select_chip(controller,
						ops_para->chipnr);
					tmp_addr = ops_para->page_addr;
					temp_ran_mode = controller->ran_mode;
					temp_option = ops_para->option;
					controller->ran_mode = 0;
					ops_para->page_addr += 1;
					ops_para->option |= DEV_ECC_SOFT_MODE;
					ops_para->option &=
						DEV_SERIAL_CHIP_MODE;
					memset(aml_chip->user_page_buf, 0xff,
						flash->pagesize);
					memset(aml_chip->user_oob_buf, 0xff,
						sizeof(oob_buf));

					if (aml_chip->state == CHIP_READY)
						nand_get_chip(aml_chip);
					/*
					aml_nand_msg("pgnum=%d pgaddr=%x",
						temp_page_num,
						ops_para->page_addr);
					*/
					ret = operation->write_page(aml_chip);

					if (aml_chip->state == CHIP_READY)
						nand_release_chip(aml_chip);

					controller->ran_mode = temp_ran_mode;
					ops_para->page_addr = tmp_addr;
					ops_para->option = temp_option;
					/*
					ops_para->option &= DEV_ECC_HW_MODE;
					*/
					/*
					ops_para->option |=DEV_SLC_MODE;
					*/
				}
			}

			offset_tmp += 1;
			amount_saved += flash->pagesize;

			if (flash->new_type == HYNIX_1YNM) {
				if (amount_saved >= size+
					extra_page*flash->pagesize) {
					/*
					for slc mode, if not full block write,
					need write dummy random data to lock
					data
					*/
					/*
					write dummy page
					*/
					memset((unsigned char *)ops_para, 0x0,
						sizeof(struct chip_ops_para));
					ops_para->option |= DEV_SLC_MODE;

					tmp_value = blk_addr;
					tmp_value /= controller->chip_num;
					tmp_value += tmp_blk -
						tmp_blk/controller->chip_num;
					tmp_value *= pages_per_blk;
					ops_para->page_addr =
						tmp_value + temp_page_num + 1;

					tmp_value = ops_para->page_addr;
					tmp_value &= (~(pages_per_blk - 1));
					index = ops_para->page_addr % 256;
					ops_para->page_addr = tmp_value |
						(slc_info->pagelist[index]);
					ops_para->chipnr =
						blk_addr % controller->chip_num;
					controller->select_chip(controller,
						ops_para->chipnr);
					ops_para->data_buf =
						aml_chip->user_page_buf;
					ops_para->oob_buf =
						aml_chip->user_oob_buf;
					ops_para->ooblen = sizeof(oob_buf);

					memset(aml_chip->user_page_buf, 0x5a,
						flash->pagesize);
					memset(aml_chip->user_oob_buf, 0x5a,
						sizeof(oob_buf));

					if (aml_chip->state == CHIP_READY)
						nand_get_chip(aml_chip);

			aml_nand_msg("dummy random data:pgnum=%d pgaddr=%x",
						temp_page_num,
						ops_para->page_addr);
					ret = operation->write_page(aml_chip);

					if (aml_chip->state == CHIP_READY)
						nand_release_chip(aml_chip);

					/* write dummy page paried */
					tmp_addr = ops_para->page_addr;
					temp_ran_mode = controller->ran_mode;
					temp_option = ops_para->option;
					controller->ran_mode = 0;
					ops_para->page_addr += 1;
					ops_para->option |= DEV_ECC_SOFT_MODE;
					ops_para->option &=
						DEV_SERIAL_CHIP_MODE;
					memset(aml_chip->user_page_buf, 0xff,
						flash->pagesize);
					memset(aml_chip->user_oob_buf, 0xff,
						sizeof(oob_buf));

					if (aml_chip->state == CHIP_READY)
						nand_get_chip(aml_chip);

		aml_nand_msg("dummy random paired data:pgnum=%d pgaddr=%x",
						temp_page_num,
						ops_para->page_addr);
					ret = operation->write_page(aml_chip);

					if (aml_chip->state == CHIP_READY)
						nand_release_chip(aml_chip);

					controller->ran_mode = temp_ran_mode;
					ops_para->page_addr = tmp_addr;
					ops_para->option = temp_option;

				}
			}
		}
	}

	if (ret < 0) { /* write failed */
		aml_nand_msg(" NAND SAVE :arg %s faild at blk:%d",
			name,
			blk_addr);
		ret = -NAND_WRITE_FAILED;
		goto exit_error0;
	} else {
		/* write success and update reserved_blk table */
		if ((arg_info->arg_valid == 0)
			|| (arg_info->arg_type == FULL_BLK)
			|| ((arg_info->arg_type == FULL_PAGE)
			&& (full_page_flag || arg_info->update_flag))) {
			for (i = 0; i < RESERVED_BLOCK_CNT; i++) {
				if (aml_chip->reserved_blk[i] == 0xff) {
					aml_nand_dbg("update blk %s blk %d",
						name,
						blk_addr);
					aml_chip->reserved_blk[i] = blk_addr;
					break;
				}
			}
		}
#if 0
		for (i = 0; i < RESERVED_BLOCK_CNT; i++)
			aml_nand_dbg("aml_chip->reserved_blk[%d]=%d ",
			i,
			aml_chip->reserved_blk[i]);
#endif
		/* write success and update arg_info */
		tmp_blk_addr = arg_info->valid_blk_addr;
		arg_info->valid_blk_addr = blk_addr;

		if ((arg_info->arg_type == FULL_PAGE)
			&& (arg_info->arg_valid)) {
			if (full_page_flag || arg_info->update_flag)
				arg_info->valid_page_addr  = 0;
			else {
				arg_info->valid_page_addr += arg_pages;
				if (flash->new_type == HYNIX_1YNM)
					arg_info->valid_page_addr += 1;
			}
		} else if ((arg_info->arg_type == FULL_BLK)
			&& (arg_info->arg_valid))
			arg_info->valid_page_addr  = 0;

		aml_nand_dbg("NAND  SAVE :  arg  %s success at blk:%d",
			name,
			blk_addr);

		/* erase old info block */
		if ((arg_info->arg_type == FULL_BLK)
			|| ((arg_info->arg_type == FULL_PAGE)
			&& (full_page_flag || arg_info->update_flag))) {
			if ((arg_info->arg_valid) && (tmp_blk_addr != 0)) {
				aml_nand_dbg("nand erase old arg %s blk at %d",
					name,
					tmp_blk_addr);
				tmp_value = tmp_blk_addr;
				tmp_value /= controller->chip_num;
				tmp_value += tmp_blk -
					tmp_blk/controller->chip_num;
				ops_para->page_addr = tmp_value * pages_per_blk;
				ops_para->chipnr =
					tmp_blk_addr % controller->chip_num;
				controller->select_chip(controller,
					ops_para->chipnr);

				if (aml_chip->state == CHIP_READY)
					nand_get_chip(aml_chip);

				ret = operation->erase_block(aml_chip);

				if (aml_chip->state == CHIP_READY)
					nand_release_chip(aml_chip);

				if (ret < 0) {
					aml_nand_msg("blk %d erase failed",
						tmp_blk_addr);
					ret =
					operation->test_block_reserved(aml_chip,
						tmp_blk_addr);
				if (ret) {
					ret =
					operation->block_markbad(aml_chip);
				if (ret < 0) {
					aml_nand_msg("mark failed,blk=%d",
						tmp_blk_addr);
					goto exit_error0;
				}
				}
				}

				for (i = 0; i < RESERVED_BLOCK_CNT; i++) {
					if (aml_chip->reserved_blk[i]
						== tmp_blk_addr) {
						aml_chip->reserved_blk[i] =
							0xff;
						break;
					}
				}
#if 0
				for (i = 0; i < RESERVED_BLOCK_CNT; i++)
					aml_nand_dbg("reserved_blk[%d]=%d ",
						i,
						aml_chip->reserved_blk[i]);
#endif
			}
		}

		if ((arg_info->arg_type == FULL_PAGE)
			&& (flash->blocksize > CONFIG_KEY_MAX_SIZE)) {
			if (write_page_cnt == 0) {
				arg_info->arg_valid = 1;
				full_page_flag = 0;
				arg_info->update_flag = 0;
				write_page_cnt = 1;
				goto write_again;
			}
		}
		arg_info->arg_valid = 1;   /* SAVE SET VALID */
		full_page_flag = 0;
	}

exit_error0:
	return ret;
}


void show_data_buf(unsigned char *buf)
{
	int i = 0;

	for (i = 0; i < 10; i++)
		aml_nand_dbg("buf[%d]= %d", i, buf[i]);

	return;
}

int amlnand_check_info_by_name(struct amlnand_chip *aml_chip,
	unsigned char *info,
	unsigned char *name,
	unsigned int size)
{
	struct hw_controller *controller = &aml_chip->controller;
	struct nand_flash *flash = &aml_chip->flash;
	struct chip_operation *operation = &aml_chip->operation;
	struct chip_ops_para  *ops_para = &aml_chip->ops_para;
	struct en_slc_info *slc_info = &(controller->slc_info);
	struct nand_arg_info *arg_info = (struct nand_arg_info *)info;
	struct nand_arg_oobinfo *arg_oob_info;

	unsigned char phys_erase_shift, phys_page_shift, nand_boot;
	unsigned int i, offset, pages_per_blk, pages_read;
	unsigned char oob_buf[sizeof(struct nand_arg_oobinfo)];
	unsigned short start_blk, total_blk, tmp_blk;
	int ret = 0, read_failed_page = 0, read_middle_page_failed = 0;
	unsigned int tmp_value, index;

	nand_boot = 1;
	/*if(boot_device_flag == 0){
		nand_boot = 0;
	}	*/

	if (nand_boot)
		offset = (1024 * flash->pagesize);
	else
		offset = 0;

	arg_oob_info = (struct nand_arg_oobinfo *)oob_buf;
	memset((unsigned char *)ops_para, 0x0, sizeof(struct chip_ops_para));

	phys_erase_shift = ffs(flash->blocksize) - 1;
	phys_page_shift =  ffs(flash->pagesize) - 1;
	pages_per_blk = (1 << (phys_erase_shift - phys_page_shift));

	if ((flash->new_type)
		&& ((flash->new_type < 10)
		|| (flash->new_type == SANDISK_19NM)))
		ops_para->option |= DEV_SLC_MODE;

	if (ops_para->option & DEV_SLC_MODE)
		pages_read = pages_per_blk >> 1;
	else
		pages_read = pages_per_blk;

	start_blk = (offset >> phys_erase_shift);
	tmp_blk = start_blk;
#if 0
	total_blk = (offset >> phys_erase_shift) + RESERVED_BLOCK_CNT;
#else
	total_blk = get_last_reserve_block(aml_chip);
#endif
#if 1
	for (; start_blk < total_blk; start_blk++) {
		read_failed_page = 0;
		read_middle_page_failed = 0;
		memset((unsigned char *)ops_para, 0x0,
			sizeof(struct chip_ops_para));

		tmp_value = start_blk;
		tmp_value /= controller->chip_num;
		tmp_value += tmp_blk - tmp_blk/controller->chip_num;
		ops_para->page_addr = tmp_value * pages_per_blk;
		ops_para->chipnr = start_blk % controller->chip_num;
		controller->select_chip(controller, ops_para->chipnr);
		/* yyh0704, avoid bbt mismatch block status! */
		ret = operation->block_isbad(aml_chip);
		if (ret) {
			/*if (memcmp(name, DTD_INFO_HEAD_MAGIC, 4)) {*/
				aml_nand_msg("nand block at blk %d is bad ",
					start_blk);
				continue;
			/*}*/
		}
		for (i = 0; i < pages_read;) {
			memset((unsigned char *)ops_para, 0x0,
				sizeof(struct chip_ops_para));
			if ((flash->new_type)
				&& ((flash->new_type < 10)
				|| (flash->new_type == SANDISK_19NM)))
				ops_para->option |= DEV_SLC_MODE;

			ops_para->page_addr = (i + tmp_value * pages_per_blk);
			ops_para->chipnr = start_blk % controller->chip_num;

			controller->select_chip(controller, ops_para->chipnr);

			if ((ops_para->option & DEV_SLC_MODE)) {
				index = ops_para->page_addr;
				index &= (~(pages_per_blk - 1));
				if ((flash->new_type > 0)
					&& (flash->new_type < 10))
					ops_para->page_addr = index |
				(slc_info->pagelist[ops_para->page_addr % 256]);
				if (flash->new_type == SANDISK_19NM)
					ops_para->page_addr = index |
				((ops_para->page_addr % pages_per_blk) << 1);
			}
			ops_para->data_buf = aml_chip->user_page_buf;
			ops_para->oob_buf = aml_chip->user_oob_buf;
			ops_para->ooblen = sizeof(oob_buf);

			memset(ops_para->data_buf, 0x0, flash->pagesize);
			memset(ops_para->oob_buf, 0x0, sizeof(oob_buf));
			if (aml_chip->state == CHIP_READY)
				nand_get_chip(aml_chip);

			ret = operation->read_page(aml_chip);
			if (aml_chip->state == CHIP_READY)
				nand_release_chip(aml_chip);

			if ((ops_para->ecc_err) || (ret < 0)) {
				aml_nand_msg("blk check good but read failed");
				aml_nand_msg("chip%d page=%d",
					ops_para->chipnr,
					ops_para->page_addr);
				read_failed_page++;
				i += (size >> phys_page_shift) + 1;
				if ((read_failed_page > 8)
					&& (read_middle_page_failed == 0)) {
					aml_nand_msg("failed_pg=%d",
						read_failed_page);
					i = ((size >> phys_page_shift) + 1) *
			((pages_per_blk/((size >> phys_page_shift) + 1))>>1);
					read_middle_page_failed = -1;
				} else if (read_middle_page_failed == -1) {
					aml_nand_msg("failed_page=%d",
						read_failed_page);
					i = ((size >> phys_page_shift) + 1) *
			((pages_per_blk/((size >> phys_page_shift) + 1)));
					read_middle_page_failed = -2;
				}
				continue;
			}

			memcpy((unsigned char *)arg_oob_info,
				aml_chip->user_oob_buf,
				sizeof(oob_buf));
			if ((!memcmp(arg_oob_info->name, name, 4))) {
				if (arg_info->arg_valid == 1) {
					if (arg_oob_info->timestamp >
						arg_info->timestamp) {
						arg_info->free_blk_addr =
						arg_info->valid_blk_addr;
						arg_info->valid_blk_addr =
							start_blk;
						arg_info->timestamp =
							arg_oob_info->timestamp;
					} else
						arg_info->free_blk_addr =
							start_blk;
					break;
				} else {
					arg_info->arg_valid = 1;
					arg_info->valid_blk_addr = start_blk;
					arg_info->timestamp =
						arg_oob_info->timestamp;
				}
			}
			break;
		}

		if ((arg_info->arg_type == FULL_BLK)
			&& (arg_info->arg_valid == 1)
			&& (arg_info->valid_blk_addr != 0))
			break;
	}
#else
	do {
		memset((unsigned char *)ops_para, 0x0,
			sizeof(struct chip_ops_para));
		ops_para->page_addr =
		(((start_blk - start_blk % controller->chip_num) /
		controller->chip_num) + tmp_blk -
		tmp_blk/controller->chip_num) * pages_per_blk;
		ops_para->chipnr = start_blk % controller->chip_num;
		controller->select_chip(controller, ops_para->chipnr);
		ret = operation->block_isbad(aml_chip);
		if (ret < 0) {
			aml_nand_msg("nand block at blk %d is bad ",
				start_blk);
			continue;
		}

		memset((unsigned char *)ops_para, 0x0,
			sizeof(struct chip_ops_para));
		if ((flash->new_type) && ((flash->new_type < 10)
			|| (flash->new_type == SANDISK_19NM)))
			ops_para->option |= DEV_SLC_MODE
		ops_para->page_addr =
			(((start_blk - start_blk % controller->chip_num) /
			controller->chip_num) + tmp_blk -
			tmp_blk/controller->chip_num) * pages_per_blk;
		ops_para->chipnr = start_blk % controller->chip_num;
		controller->select_chip(controller, ops_para->chipnr);

		if ((ops_para->option & DEV_SLC_MODE)) {
			if ((flash->new_type > 0) && (flash->new_type < 10))
				ops_para->page_addr = (ops_para->page_addr &
					(~(pages_per_blk - 1))) |
				(slc_info->pagelist[ops_para->page_addr % 256]);
			if (flash->new_type == SANDISK_19NM)
				ops_para->page_addr =
			(ops_para->page_addr & (~(pages_per_blk - 1))) |
				((ops_para->page_addr % pages_per_blk) << 1);
		}

		ops_para->data_buf = aml_chip->user_page_buf;
		ops_para->oob_buf = aml_chip->user_oob_buf;
		ops_para->ooblen = sizeof(oob_buf);

		memset(ops_para->data_buf, 0x0, flash->pagesize);
		memset(ops_para->oob_buf, 0x0, sizeof(oob_buf));

		if (aml_chip->state == CHIP_READY)
			nand_get_chip(aml_chip);

		ret = operation->read_page(aml_chip);

		if (aml_chip->state == CHIP_READY)
			nand_release_chip(aml_chip);

		if ((ops_para->ecc_err) || (ret < 0)) {
			aml_nand_msg("blk check good but read failed");
			aml_nand_msg("chip%d page=%d",
				ops_para->chipnr,
				ops_para->page_addr);
			continue;
		}

#if 0
	aml_nand_dbg(" ops_para->oob_buf : %x %x %x %x %x %x %x %x",
		ops_para->oob_buf[0], ops_para->oob_buf[1],
		ops_para->oob_buf[2], ops_para->oob_buf[3],
		ops_para->oob_buf[4], ops_para->oob_buf[5],
		ops_para->oob_buf[6], ops_para->oob_buf[7]);
#endif
		memcpy((unsigned char *)arg_oob_info,  aml_chip->user_oob_buf,
			sizeof(oob_buf));
		if ((!memcmp(arg_oob_info->name, name, 4))) {
			if (arg_info->arg_valid == 1) {
				if (arg_oob_info->timestamp >
					arg_info->timestamp) {
					arg_info->free_blk_addr =
						arg_info->valid_blk_addr;
					arg_info->valid_blk_addr = start_blk;
					arg_info->timestamp =
						arg_oob_info->timestamp;
				} else
					arg_info->free_blk_addr = start_blk;
				break;
			} else {
				arg_info->arg_valid = 1;
				arg_info->valid_blk_addr = start_blk;
				arg_info->timestamp = arg_oob_info->timestamp;
			}
		}

		if ((arg_info->arg_type == FULL_BLK)
			&& (arg_info->arg_valid == 1)
			&& (arg_info->valid_blk_addr != 0))
			break;
	} while ((++start_blk) < total_blk);
#endif

	if (arg_info->arg_valid == 1) {
		if (arg_info->arg_type == FULL_BLK) {
			for (i = 0; i < pages_read;) {
				memset((unsigned char *)ops_para, 0x0,
					sizeof(struct chip_ops_para));
				ops_para->data_buf = NULL;
				ops_para->oob_buf = aml_chip->user_oob_buf;
				ops_para->ooblen = sizeof(oob_buf);
				memset((unsigned char *)ops_para->oob_buf, 0x0,
					sizeof(oob_buf));

				if ((flash->new_type)
					&& ((flash->new_type < 10)
					|| (flash->new_type == SANDISK_19NM)))
					ops_para->option |= DEV_SLC_MODE;

				tmp_value = arg_info->valid_blk_addr;
				tmp_value /= controller->chip_num;
				tmp_value += tmp_blk -
					tmp_blk/controller->chip_num;
				tmp_value *= pages_per_blk;
				ops_para->page_addr = i + tmp_value;

				if ((ops_para->option & DEV_SLC_MODE)) {
					index = ops_para->page_addr;
					index &= (~(pages_per_blk - 1));
					if ((flash->new_type > 0)
						&& (flash->new_type < 10))
						ops_para->page_addr = index |
				(slc_info->pagelist[ops_para->page_addr % 256]);
					if (flash->new_type == SANDISK_19NM)
						ops_para->page_addr = index |
				((ops_para->page_addr % pages_per_blk) << 1);
				}

				ops_para->chipnr =
				arg_info->valid_blk_addr % controller->chip_num;
				controller->select_chip(controller,
					ops_para->chipnr);

				if (aml_chip->state == CHIP_READY)
					nand_get_chip(aml_chip);

				ret = operation->read_page(aml_chip);

				if (aml_chip->state == CHIP_READY)
					nand_release_chip(aml_chip);

			if ((ops_para->ecc_err) || (ret < 0)) {
				aml_nand_msg("read %s failed chip%d page=%d",
					name,
					ops_para->chipnr,
					ops_para->page_addr);
				i += (size >> phys_page_shift) + 1;
				arg_info->update_flag = 1;
				continue;
			}

				memcpy((unsigned char *)arg_oob_info,
					aml_chip->user_oob_buf,
					sizeof(oob_buf));
				if (!memcmp(arg_oob_info->name, name, 4)) {
					arg_info->valid_page_addr = i;
					arg_info->timestamp =
						arg_oob_info->timestamp;
					break;
				}
				i += (size >> phys_page_shift) + 1;
			}
		} else if (arg_info->arg_type == FULL_PAGE) {
			for (i = 0; i < pages_read;) {
				memset((unsigned char *)ops_para, 0x0,
					sizeof(struct chip_ops_para));
				if ((flash->new_type)
					&& ((flash->new_type < 10)
					|| (flash->new_type == SANDISK_19NM)))
					ops_para->option |= DEV_SLC_MODE;

				ops_para->data_buf = aml_chip->user_page_buf;
				ops_para->oob_buf = aml_chip->user_oob_buf;
				ops_para->ooblen = sizeof(oob_buf);
				memset((unsigned char *)ops_para->data_buf,
					0x0, flash->pagesize);
				memset((unsigned char *)ops_para->oob_buf,
					0x0, sizeof(oob_buf));

				tmp_value = arg_info->valid_blk_addr;
				tmp_value /= controller->chip_num;
				tmp_value += tmp_blk -
					tmp_blk/controller->chip_num;
				tmp_value *= pages_per_blk;
				ops_para->page_addr = i + tmp_value;

				if ((ops_para->option & DEV_SLC_MODE)) {
					index = ops_para->page_addr;
					index &= (~(pages_per_blk - 1));
					if ((flash->new_type > 0)
						&& (flash->new_type < 10))
						ops_para->page_addr = index |
				(slc_info->pagelist[ops_para->page_addr % 256]);
					if (flash->new_type == SANDISK_19NM)
						ops_para->page_addr = index |
				((ops_para->page_addr % pages_per_blk) << 1);
				}

				ops_para->chipnr = arg_info->valid_blk_addr %
					controller->chip_num;
				controller->select_chip(controller,
					ops_para->chipnr);

				if (aml_chip->state == CHIP_READY)
					nand_get_chip(aml_chip);

				ret = operation->read_page(aml_chip);

				if (aml_chip->state == CHIP_READY)
					nand_release_chip(aml_chip);

			if ((ops_para->ecc_err) || (ret < 0)) {
				aml_nand_msg("read %s failed at chip%d page %d",
					name,
					ops_para->chipnr,
					ops_para->page_addr);
				i += (size >> phys_page_shift) + 1;
				arg_info->update_flag = 1;
				continue;
			}

				memcpy((unsigned char *)arg_oob_info,
					aml_chip->user_oob_buf,
					sizeof(oob_buf));
				if (!memcmp(arg_oob_info->name, name, 4)) {
					arg_info->valid_page_addr = i;
					arg_info->timestamp =
						arg_oob_info->timestamp;
				} else
					break;

				i += (size >> phys_page_shift) + 1;
				/*for hynix slc mode*/
				if (flash->new_type == HYNIX_1YNM)
					i += 1;
			}
		}

		for (i = 0; i < RESERVED_BLOCK_CNT; i++) {
			if (aml_chip->reserved_blk[i] == 0xff) {
				aml_chip->reserved_blk[i] =
					arg_info->valid_blk_addr;
				break;
			}
		}
		aml_nand_dbg("NAND CKECK: %s success at blk:%d page %d",
			name,
			arg_info->valid_blk_addr,
			arg_info->valid_page_addr);
	}
	aml_nand_msg("NAND CKECK:arg %s:arg_valid=%d,blk_addr=%d,page_addr=%d",
		name,
		arg_info->arg_valid,
		arg_info->valid_blk_addr,
		arg_info->valid_page_addr);
	aml_nand_dbg(" complete ");

	return ret;
}


int amlnand_info_init(struct amlnand_chip *aml_chip,
	unsigned char *info,
	unsigned char *buf,
	unsigned char *name,
	unsigned int size)
{
	struct nand_arg_info *arg_info = (struct nand_arg_info *)info;
	int ret = 0;

	aml_nand_dbg("NAME :  %s", name);

	ret = amlnand_check_info_by_name(aml_chip,
		(unsigned char *) arg_info ,
		name,
		size);
	if (ret < 0) {
		aml_nand_msg("nand check info failed");
		goto exit_error;
	}

	if (arg_info->arg_valid == 1) {
		ret = amlnand_read_info_by_name(aml_chip,
			(unsigned char *)arg_info,
			buf,
			name,
			size);
		if (ret < 0) {
			aml_nand_msg("nand check info success but read failed");
			goto exit_error;
		}
		ret = aml_info_check_datasum(buf, name);
		if (ret < 0) {
			aml_nand_msg("amlnand_info_init:check read %s failed",
				name);
			/* ret = 0; */
			goto exit_error;
		}
	} else
		aml_nand_msg("found NO arg : %s info", name);

exit_error:
	return ret;
}


/*****************************************************************************
*Name         :amlnand_update_bbt
*Description :update bbt by block status
*Parameter  :
*Return       :
*Note          :
*****************************************************************************/
int amlnand_update_bbt(struct amlnand_chip *aml_chip)
{
	struct nand_flash *flash = &aml_chip->flash;
	unsigned int total_blk, pages_per_blk;
	unsigned char phys_erase_shift, phys_page_shift;
	int ret = 0;
	uint64_t tmp_size;

	aml_nand_dbg("amlnand_update_bbt  :here!!");
	phys_erase_shift = ffs(flash->blocksize) - 1;
	phys_page_shift =  ffs(flash->pagesize) - 1;
	pages_per_blk = (1 << (phys_erase_shift - phys_page_shift));

	tmp_size = flash->chipsize;
	total_blk = (int) ((tmp_size<<20) >> phys_erase_shift);

#if 0
	/* show the bbt */
	aml_nand_dbg("show the bbt");
	for (chipnr = 0; chipnr < controller->chip_num; chipnr++) {
		tmp_arr = &aml_chip->bbt_ptr->nand_bbt[chipnr][0];
		for (start_block = 0; start_block < 200; start_block++)
			aml_nand_msg(" tmp_arr[%d][%d]=%d",
				chipnr,
				start_block,
				tmp_arr[start_block]);
	}
#endif

	aml_chip->block_status->crc =
		aml_info_checksum((u8 *)aml_chip->block_status->blk_status,
			(MAX_CHIP_NUM*MAX_BLK_NUM));
	ret = amlnand_save_info_by_name(aml_chip,
		(u8 *)&(aml_chip->nand_bbtinfo),
		(u8 *)aml_chip->block_status,
		BBT_HEAD_MAGIC,
		sizeof(struct block_status));
	if (ret < 0) {
		aml_nand_msg("nand update bbt failed");
		goto exit_error0;
	}

	return ret;
exit_error0:
	return ret;
}


/*****************************************************************************
*Name         :amlnand_init_block_status
*Description :init the block_status table by bbt;
*Parameter  :
*Return       :
*Note          :
*****************************************************************************/
int amlnand_init_block_status(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &aml_chip->controller;
	/* struct chip_operation *operation = &aml_chip->operation; */
	struct nand_flash *flash = &aml_chip->flash;
	/* struct shipped_bbt * shipped_bbt_ptr = aml_chip->shipped_bbt_ptr; */
	u32 start_blk, total_blk, chipnr, pages_per_blk, offset, nand_boot;
	unsigned short *tmp_bbt, *tmp_status;
	unsigned char phys_erase_shift, phys_page_shift;
	uint64_t tmp_size;
	int ret = 0, i, j;

	aml_nand_dbg("amlnand_init_block_status : start");

	nand_boot = 1;

	if (boot_device_flag == 0)
		nand_boot = 0;

	if (nand_boot)
		offset = (1024 * flash->pagesize);
	else
		offset = 0;

	phys_erase_shift = ffs(flash->blocksize) - 1;
	phys_page_shift =  ffs(flash->pagesize) - 1;
	pages_per_blk = (1 << (phys_erase_shift - phys_page_shift));

	tmp_size = flash->chipsize;
	start_blk = offset >> phys_erase_shift;
	total_blk = (int) ((tmp_size<<20) >> phys_erase_shift);

#if 0
	aml_nand_dbg("show the bbt");
	for (chipnr = 0; chipnr < controller->chip_num; chipnr++) {
		tmp_arr = &aml_chip->bbt_ptr->nand_bbt[chipnr][0];
		for (start_block = 0; start_block < 200; start_block++)
			aml_nand_msg(" tmp_arr[%d][%d]=%d",
				chipnr,
				start_block,
				tmp_arr[start_block]);
	}
#endif

	for (chipnr = 0; chipnr < controller->chip_num;  chipnr++) {
		tmp_bbt = &aml_chip->shipped_bbt_ptr->shipped_bbt[chipnr][0];
		tmp_status = &aml_chip->block_status->blk_status[chipnr][0];
		for (i = 0; i < total_blk; i++) {
			tmp_status[i] = NAND_BLOCK_GOOD;
			for (j = 0; j < MAX_BAD_BLK_NUM; j++) {
				if ((tmp_bbt[j] & 0x7fff) == i) {
					if ((tmp_bbt[j] & 0x8000)) {
						tmp_status[i] =
							NAND_BLOCK_FACTORY_BAD;
						aml_nand_dbg("s[%d][%d]=%d",
							chipnr,
							i,
							tmp_status[i]);
					} else {
				if (i == 0)
					tmp_status[i] = NAND_BLOCK_GOOD;
				else{
					tmp_status[i] = NAND_BLOCK_USED_BAD;
					aml_nand_dbg("s[%d][%d]=%d",
						chipnr,
						i,
						tmp_status[i]);
				}
					}
					break;
				}
			}
		}
	}

#if 0
	aml_nand_dbg("show the block status ");
	unsigned short *tmp_arr;
	int start_block;
	for (chipnr = 0; chipnr < controller->chip_num; chipnr++) {
		tmp_arr = &aml_chip->block_status->blk_status[chipnr][0];
		for (start_block = 0; start_block < 50; start_block++)
			aml_nand_msg(" tmp_arr[%d][%d]=%d",
				chipnr,
				start_block,
				tmp_arr[start_block]);
	}
#endif

	return ret;
}


int aml_nand_save_hynix_info(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &aml_chip->controller;
	struct nand_flash *flash = &aml_chip->flash;
	struct chip_operation *operation = &aml_chip->operation;
	struct chip_ops_para  *ops_para = &aml_chip->ops_para;
	struct read_retry_info *retry_info = &(controller->retry_info);
	struct en_slc_info *slc_info = &(controller->slc_info);

	unsigned char phys_erase_shift, phys_page_shift;
	unsigned short  blk_addr = 0,  tmp_blk,  nand_boot;
	unsigned int i, j, offset,  pages_per_blk, pages_read;
	unsigned char oob_buf[8];
	int ret = 0;
	unsigned int tmp_addr;
	int save_cnt = 20, test_cnt = 0;
	unsigned char tmp_rand;
	unsigned int tmp_value;

	if (retry_info->retry_cnt_lp > 20)
		save_cnt = retry_info->retry_cnt_lp;
	if ((flash->new_type == 0) || (flash->new_type > 10))
		return NAND_SUCCESS;

	aml_nand_dbg("aml_nand_save_hynix_info : here!! ");

	nand_boot = 1;

	/*if(boot_device_flag == 0){
		nand_boot = 0;
	}*/

	if (nand_boot)
		offset = (1024 * flash->pagesize);
	else
		offset = 0;

	phys_erase_shift = ffs(flash->blocksize) - 1;
	phys_page_shift =  ffs(flash->pagesize) - 1;
	pages_per_blk = (1 << (phys_erase_shift - phys_page_shift));
	tmp_blk = (offset >> phys_erase_shift);

	if ((flash->new_type) && (flash->new_type < 10))
		ops_para->option |= DEV_SLC_MODE;

	if (ops_para->option & DEV_SLC_MODE)
		pages_read = pages_per_blk >> 1;
	else
		pages_read = pages_per_blk;

	memset((unsigned char *)ops_para, 0x0, sizeof(struct chip_ops_para));
	memcpy(oob_buf, HYNIX_DEV_HEAD_MAGIC, strlen(HYNIX_DEV_HEAD_MAGIC));

get_free_blk:
	ret = amlnand_get_free_block(aml_chip, blk_addr);
	blk_addr = ret;
	if (ret < 0) {
		aml_nand_msg("nand get free block failed");
		ret = -NAND_BAD_BLCOK_FAILURE;
		goto exit_error0;
	}
	aml_nand_dbg("nand get free block for hynix readretry info at %d",
		blk_addr);

	for (i = 0; i < pages_read; i++) {
		memset((unsigned char *)ops_para, 0x0,
			sizeof(struct chip_ops_para));
		if ((flash->new_type) && (flash->new_type < 10))
			ops_para->option |= DEV_SLC_MODE;
		tmp_value = blk_addr;
		tmp_value /= controller->chip_num;
		tmp_value += tmp_blk - tmp_blk/controller->chip_num;
		ops_para->page_addr = i + (tmp_value * pages_per_blk);
		ops_para->chipnr = blk_addr % controller->chip_num;
		controller->select_chip(controller, ops_para->chipnr);

		if ((ops_para->option & DEV_SLC_MODE)
			&& ((flash->new_type > 0)
			&& (flash->new_type < 10))) {
			tmp_value = ops_para->page_addr;
			tmp_value &= (~(pages_per_blk - 1));
			ops_para->page_addr = tmp_value |
				(slc_info->pagelist[ops_para->page_addr % 256]);
		}

		ops_para->data_buf = aml_chip->user_page_buf;
		ops_para->oob_buf = aml_chip->user_oob_buf;
		ops_para->ooblen = sizeof(oob_buf);

		if (flash->new_type == HYNIX_1YNM) {
			if ((i > 1) && ((i%2) == 0)) {
				tmp_addr =	ops_para->page_addr;
				tmp_rand = controller->ran_mode;
				ops_para->page_addr -= 1;
				controller->ran_mode = 0;
				ops_para->option |= DEV_ECC_SOFT_MODE;
				ops_para->option &= DEV_SERIAL_CHIP_MODE;
				memset(aml_chip->user_page_buf, 0xff,
					flash->pagesize);

				if (aml_chip->state == CHIP_READY)
					nand_get_chip(aml_chip);

				ret = operation->write_page(aml_chip);

				if (aml_chip->state == CHIP_READY)
					nand_release_chip(aml_chip);

				ops_para->page_addr = tmp_addr;
				controller->ran_mode = tmp_rand;
				ops_para->option &= DEV_ECC_HW_MODE;
				ops_para->option |= DEV_SLC_MODE;
			}
		}
		memset(aml_chip->user_page_buf, 0x0, flash->pagesize);
		memset(aml_chip->user_oob_buf, 0x0, sizeof(oob_buf));
		memcpy((unsigned char *)aml_chip->user_page_buf,
			&retry_info->reg_def_val[0][0],
			MAX_CHIP_NUM*READ_RETRY_REG_NUM);
		memcpy(aml_chip->user_oob_buf, (unsigned char *)oob_buf, 4);

#ifdef DEBUG_HYINX_DEF
		for (k = 0; k < controller->chip_num; k++)
			for (j = 0; j < save_cnt; j++)
				memcpy((u8 *)(aml_chip->user_page_buf +
					MAX_CHIP_NUM*READ_RETRY_REG_NUM +
					j*READ_RETRY_REG_NUM+k*READ_RETRY_CNT),
					&retry_info->reg_offs_val_lp[k][j][0],
					READ_RETRY_REG_NUM);
#else
		memcpy((u8 *)(aml_chip->user_page_buf +
			MAX_CHIP_NUM*READ_RETRY_REG_NUM),
			&retry_info->reg_offs_val_lp[0][0][0],
			MAX_CHIP_NUM*READ_RETRY_CNT*READ_RETRY_REG_NUM);
#endif

		if (aml_chip->state == CHIP_READY)
			nand_get_chip(aml_chip);

		ret = operation->write_page(aml_chip);

		if (aml_chip->state == CHIP_READY)
			nand_release_chip(aml_chip);

		if (ret < 0) {
			aml_nand_msg("%s:nand write failed", __func__);
			if (test_cnt >= 3) {
				aml_nand_msg("test blk 3times,");
				break;
			}
			ret = operation->test_block_reserved(aml_chip,
				blk_addr);
			test_cnt++;
			if (ret) {
				ret = operation->block_markbad(aml_chip);
				if (ret < 0)
					aml_nand_dbg("mark badblk failed %d",
						blk_addr);
			}
			goto get_free_blk;
		}
	}

	if (ret < 0) {
		aml_nand_msg("hynix nand save readretry info failed");
		goto exit_error0;
	} else {
		for (j = 0; j < RESERVED_BLOCK_CNT; j++) {
			if (aml_chip->reserved_blk[j] == 0xff) {
				aml_chip->reserved_blk[j] = blk_addr;
				break;
			}
		}
		retry_info->info_save_blk = blk_addr;
		aml_nand_dbg("save hynix readretry info success at blk %d",
			blk_addr);
	}

exit_error0:
	return ret;
}

int aml_nand_scan_hynix_info(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &aml_chip->controller;
	struct nand_flash *flash = &aml_chip->flash;
	struct chip_operation *operation = &aml_chip->operation;
	struct chip_ops_para  *ops_para = &aml_chip->ops_para;
	struct read_retry_info *retry_info = &(controller->retry_info);
	struct en_slc_info *slc_info = &(controller->slc_info);

	unsigned char phys_erase_shift, phys_page_shift;
	unsigned short start_blk, tmp_blk, total_blk, nand_boot;
	unsigned int i, j, k, n, offset,  pages_per_blk, pages_read;
	unsigned char oob_buf[8];
	int nand_type, ret = 0;
	int save_cnt = 20;
	unsigned int tmp_value, index;

	if (retry_info->retry_cnt_lp > 20)
		save_cnt = retry_info->retry_cnt_lp;

	if ((flash->new_type == 0) || (flash->new_type > 10))
		return NAND_SUCCESS;

	nand_boot = 1;

	/*if(boot_device_flag == 0){
		nand_boot = 0;
	}*/

	if (nand_boot)
		offset = (1024 * flash->pagesize);
	else
		offset = 0;

	memset((unsigned char *)ops_para, 0x0, sizeof(struct chip_ops_para));

	phys_erase_shift = ffs(flash->blocksize) - 1;
	phys_page_shift =  ffs(flash->pagesize) - 1;
	if ((flash->new_type) && (flash->new_type < 10))
		ops_para->option |= DEV_SLC_MODE;
	pages_per_blk = (1 << (phys_erase_shift - phys_page_shift));

	if (ops_para->option & DEV_SLC_MODE)
		pages_read = pages_per_blk >> 1;
	else
		pages_read = pages_per_blk;

	retry_info->default_flag = 0;
	retry_info->flag = 0;

	start_blk = (int)(offset >> phys_erase_shift);
	tmp_blk = start_blk;
	total_blk = (offset >> phys_erase_shift)+RESERVED_BLOCK_CNT;

	do {
		memset((unsigned char *)ops_para, 0x0,
			sizeof(struct chip_ops_para));
		tmp_value = start_blk;
		tmp_value /= controller->chip_num;
		tmp_value += tmp_blk - tmp_blk/controller->chip_num;
		ops_para->page_addr = tmp_value * pages_per_blk;
		ops_para->chipnr = start_blk % controller->chip_num;
		controller->select_chip(controller, ops_para->chipnr);

		nand_type = flash->new_type;
		flash->new_type = 0;
		ret = operation->block_isbad(aml_chip);
		flash->new_type = nand_type;
		if (ret == NAND_BLOCK_FACTORY_BAD) {
			aml_nand_msg("nand block at blk %d is bad ", start_blk);
			continue;
		}

		for (i = 0; i < pages_read; i++) {
			memset((unsigned char *)ops_para, 0x0,
				sizeof(struct chip_ops_para));

			if ((flash->new_type) && (flash->new_type < 10))
				ops_para->option |= DEV_SLC_MODE;

			ops_para->data_buf = aml_chip->user_page_buf;
			ops_para->oob_buf = aml_chip->user_oob_buf;
			ops_para->ooblen = sizeof(oob_buf);

			ops_para->page_addr = (i + tmp_value * pages_per_blk);
			if ((ops_para->option & DEV_SLC_MODE)
				&& ((flash->new_type > 0)
				&& (flash->new_type < 10))) {
				index = ops_para->page_addr;
				index &= (~(pages_per_blk - 1));
				ops_para->page_addr = index |
				(slc_info->pagelist[ops_para->page_addr % 256]);
			}
			ops_para->chipnr = start_blk % controller->chip_num;
			controller->select_chip(controller, ops_para->chipnr);

			memset((unsigned char *)ops_para->data_buf, 0x0,
				flash->pagesize);
			memset((unsigned char *)ops_para->oob_buf, 0x0,
				sizeof(oob_buf));

			nand_type = flash->new_type;
			flash->new_type = 0;

			if (aml_chip->state == CHIP_READY)
				nand_get_chip(aml_chip);

			ret = operation->read_page(aml_chip);

			if (aml_chip->state == CHIP_READY)
				nand_release_chip(aml_chip);

			flash->new_type = nand_type;
			if ((ops_para->ecc_err) || (ret < 0)) {
				aml_nand_msg("blk check good but read failed ");
				aml_nand_msg("chip %d page %d",
					ops_para->chipnr,
					ops_para->page_addr);
				continue;
			}

			memcpy(oob_buf, aml_chip->user_oob_buf,
				sizeof(oob_buf));
			if (!memcmp(oob_buf,
				HYNIX_DEV_HEAD_MAGIC,
				strlen(HYNIX_DEV_HEAD_MAGIC))) {
				memcpy(&retry_info->reg_def_val[0][0],
					(u8 *)aml_chip->user_page_buf,
					MAX_CHIP_NUM*READ_RETRY_REG_NUM);
				aml_nand_msg("get def value at blk:%d,page:%d",
					start_blk,
					ops_para->page_addr);

		for (k = 0; k < controller->chip_num; k++)
			for (j = 0; j < retry_info->reg_cnt_lp;
				j++)
				aml_nand_dbg("REG(0x%x):val:0x%x,for chip%d",
				retry_info->reg_addr_lp[j],
			retry_info->reg_def_val[k][j], k);

	if ((flash->new_type == HYNIX_20NM_8GB)
		|| (flash->new_type == HYNIX_20NM_4GB)
		|| (flash->new_type == HYNIX_1YNM)) {
#ifdef DEBUG_HYINX_DEF
		for (n = 0; n < controller->chip_num; n++)
			for (j = 0; j < save_cnt; j++)
				memcpy(&retry_info->reg_offs_val_lp[n][j][0],
					(u8 *)(aml_chip->user_page_buf +
					MAX_CHIP_NUM*READ_RETRY_REG_NUM +
					j*READ_RETRY_REG_NUM+n*save_cnt),
					READ_RETRY_REG_NUM);
#else
		memcpy(&retry_info->reg_offs_val_lp[0][0][0],
			(u8 *)(aml_chip->user_page_buf +
			MAX_CHIP_NUM*READ_RETRY_REG_NUM),
			MAX_CHIP_NUM*READ_RETRY_CNT*READ_RETRY_REG_NUM);
#endif
				}
for (n = 0; n < controller->chip_num; n++)
	for (j = 0; j < retry_info->retry_cnt_lp; j++)
		for (k = 0; k < retry_info->reg_cnt_lp; k++)
			aml_nand_dbg("Retry%dst,REG(0x%x):val:0x%2x,for chip%d",
				k,
				retry_info->reg_addr_lp[k],
			retry_info->reg_offs_val_lp[n][j][k],
			n);

				retry_info->default_flag = 1;
				retry_info->flag = 1;
				break;
			} else {
				aml_nand_dbg("read OK but magic is not hynix");
				break;
			}
		}

		if (retry_info->default_flag && flash->new_type)
			break;
	} while ((++start_blk) < total_blk);

	if (retry_info->default_flag && flash->new_type) {
		retry_info->info_save_blk = start_blk;
		for (i = 0; i < RESERVED_BLOCK_CNT; i++) {
			if (aml_chip->reserved_blk[i] == 0xff) {
				aml_chip->reserved_blk[i] = start_blk;
				break;
			}
		}
	}

	return ret;
}

static int amlnand_config_buf_malloc(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &aml_chip->controller;
	struct nand_flash *flash = &aml_chip->flash;
	/* struct chip_operation *operation = & aml_chip->operation; */
	/* struct chip_ops_para  *ops_para = & aml_chip->ops_para; */
	u32 ret = 0, buf_size;

	buf_size = flash->oobsize * controller->chip_num;
	if (flash->option & NAND_MULTI_PLANE_MODE)
		buf_size <<= 1;

	aml_chip->user_oob_buf = aml_nand_malloc(buf_size);
	if (aml_chip->user_oob_buf == NULL) {
		aml_nand_msg("malloc failed for user_oob_buf ");
		ret = -NAND_MALLOC_FAILURE;
		goto exit_error0;
	}
	memset(aml_chip->user_oob_buf, 0x0, buf_size);
	buf_size = (flash->pagesize + flash->oobsize) * controller->chip_num;
	if (flash->option & NAND_MULTI_PLANE_MODE)
		buf_size <<= 1;

	aml_chip->user_page_buf = aml_nand_malloc(buf_size);
	if (aml_chip->user_page_buf == NULL) {
		aml_nand_msg("malloc failed for user_page_buf ");
		ret = -NAND_MALLOC_FAILURE;
		goto exit_error0;
	}
	memset(aml_chip->user_page_buf, 0x0, buf_size);

	aml_chip->block_status =
	(struct block_status *)aml_nand_malloc(sizeof(struct block_status));
	if (aml_chip->block_status == NULL) {
		aml_nand_msg("malloc failed for block_status and size:%zx",
			sizeof(struct block_status));
		ret = -NAND_MALLOC_FAILURE;
		goto exit_error0;
	}
	memset(aml_chip->block_status, 0x0, (sizeof(struct block_status)));
	aml_chip->shipped_bbt_ptr = aml_nand_malloc(sizeof(struct shipped_bbt));
	if (aml_chip->shipped_bbt_ptr == NULL) {
		aml_nand_msg("malloc failed for shipped_bbt_ptr ");
		ret = -NAND_MALLOC_FAILURE;
		goto exit_error0;
	}
	memset(aml_chip->shipped_bbt_ptr, 0x0, (sizeof(struct shipped_bbt)));

	aml_chip->config_ptr = aml_nand_malloc(sizeof(struct nand_config));
	if (aml_chip->config_ptr == NULL) {
		aml_nand_msg("malloc failed for config_ptr ");
		ret = -NAND_MALLOC_FAILURE;
		goto exit_error0;
	}
	memset(aml_chip->config_ptr, 0x0, (sizeof(struct nand_config)));

	aml_chip->phy_part_ptr =
		aml_nand_malloc(sizeof(struct phy_partition_info));
	if (aml_chip->phy_part_ptr == NULL) {
		aml_nand_msg("malloc failed for phy_part_ptr ");
		ret = -NAND_MALLOC_FAILURE;
		goto exit_error0;
	}
	memset(aml_chip->phy_part_ptr,
		0x0,
		(sizeof(struct phy_partition_info)));

	return ret;
exit_error0:

	kfree(aml_chip->block_status);
	aml_chip->block_status = NULL;

	kfree(aml_chip->shipped_bbt_ptr);
	aml_chip->shipped_bbt_ptr = NULL;

	kfree(aml_chip->config_ptr);
	aml_chip->config_ptr = NULL;

	kfree(aml_chip->phy_part_ptr);
	aml_chip->phy_part_ptr = NULL;

	kfree(aml_chip->user_oob_buf);
	aml_chip->user_oob_buf = NULL;


	kfree(aml_chip->user_page_buf);
	aml_chip->user_page_buf = NULL;

	return ret;
}

void amlnand_set_config_attribute(struct amlnand_chip *aml_chip)
{
	aml_chip->nand_bbtinfo.arg_type = FULL_BLK;
	aml_chip->shipped_bbtinfo.arg_type = FULL_BLK;
	aml_chip->config_msg.arg_type = FULL_BLK;
	aml_chip->nand_secure.arg_type = FULL_PAGE;
	aml_chip->nand_key.arg_type = FULL_PAGE;
	aml_chip->uboot_env.arg_type = FULL_PAGE;
	aml_chip->nand_phy_partition.arg_type = FULL_PAGE;
#if (AML_CFG_DTB_RSV_EN)
	aml_chip->amlnf_dtb.arg_type = FULL_PAGE;
#endif
	return;
}

/*****************************************************************************
*Name         :amlnand_get_dev_configs
*Description :search bbt /fbbt /config /key;
*Parameter  :
*Return       :
*Note          :
*****************************************************************************/
int amlnand_get_dev_configs(struct amlnand_chip *aml_chip)
{
	int  ret = 0, i;
	unsigned int use_min_size;
	struct nand_flash *flash = &aml_chip->flash;

	use_min_size = min_t(u32, CONFIG_KEY_MAX_SIZE, flash->blocksize);
	aml_chip->keysize = use_min_size;
	aml_chip->dtbsize = use_min_size;

	aml_nand_dbg("key size is : 0x%0x", use_min_size);

	ret = amlnand_config_buf_malloc(aml_chip);
	if (ret < 0) {
		aml_nand_msg("nand malloc buf failed");
		goto exit_error0;
	}

	if (aml_chip->flash.new_type) {
		aml_nand_dbg("detect new nand here and new_type:%d",
			aml_chip->flash.new_type);
		ret = amlnand_set_readretry_slc_para(aml_chip);
		if (ret < 0) {
			aml_nand_msg("setting new nand para failed, ret:0x%x",
				ret);
			goto exit_error0;
		}
	}

	amlnand_set_config_attribute(aml_chip);

	/* search  bbt */
	ret = amlnand_info_init(aml_chip,
		(u8 *)&(aml_chip->nand_bbtinfo),
		(u8 *)aml_chip->block_status,
		BBT_HEAD_MAGIC,
		sizeof(struct block_status));
	if (ret < 0)
		aml_nand_msg("nand scan bbt info failed :%d", ret);

	if (aml_chip->nand_bbtinfo.arg_valid == 0) { /* bbt invalid */
		aml_nand_msg("nand scan bbt failed");
		ret = -NAND_READ_FAILED;
		goto exit_error0;
	} else {	/* bbt valid */
#if 0
		aml_nand_msg("show the block status !!!!");
		unsigned short *tmp_arr;
		int start_block, chipnr;
		for (chipnr = 0; chipnr < controller->chip_num; chipnr++) {
			tmp_arr =
				&aml_chip->block_status->blk_status[chipnr][0];
			for (start_block = 0; start_block < 50; start_block++)
				aml_nand_msg(" tmp_arr[%d][%d]=%d",
					chipnr,
					start_block,
					tmp_arr[start_block]);
		}
#endif

		ret = amlnand_info_init(aml_chip,
			(u8 *)&(aml_chip->config_msg),
			(u8 *)aml_chip->config_ptr,
			CONFIG_HEAD_MAGIC,
			sizeof(struct nand_config));
		if (ret < 0) {
			aml_nand_msg("nand scan config failed and ret:%d", ret);
			/* goto exit_error0; */
			goto exit_error0;
		}

		if (aml_chip->config_msg.arg_valid == 1) {
			aml_chip->shipped_bbtinfo.valid_blk_addr =
				aml_chip->config_ptr->fbbt_blk_addr;
			for (i = 0; i < RESERVED_BLOCK_CNT; i++) {
				if (aml_chip->reserved_blk[i] == 0xff) {
					aml_chip->reserved_blk[i] =
					aml_chip->config_ptr->fbbt_blk_addr;
					break;
				}
			}
		}
	}

	/*
	scan phy partition info here,
	if we can't find phy partition,
	we will calc and save it in phydev init stage.
	*/
	ret = amlnand_info_init(aml_chip,
		(unsigned char *)&(aml_chip->nand_phy_partition),
		(unsigned char *)aml_chip->phy_part_ptr,
		PHY_PARTITION_HEAD_MAGIC,
		sizeof(struct phy_partition_info));
	if (ret < 0)
		aml_nand_msg("nand scan phy partition failed and ret:%d", ret);

	ret = aml_sys_info_init(aml_chip); /* key  and stoarge */
	if (ret < 0) {
		aml_nand_msg("nand init sys_info failed and ret:%d", ret);
		goto exit_error0;
	}

	 amlnand_info_error_handle(aml_chip);
#if 0
	aml_nand_msg("nand debug: save info bbt again!!!!");
	ret = amlnand_save_info_by_name(aml_chip,
		(u8 *)&(aml_chip->nand_bbtinfo),
		aml_chip->block_status,
		BBT_HEAD_MAGIC,
		sizeof(struct block_status));
	if (ret < 0) {
		aml_nand_msg("nand save bbt failed and ret:%d", ret);
		goto exit_error0;
	}
#endif
	repair_reserved_bad_block(aml_chip);
	return ret;

exit_error0:

	kfree(aml_chip->block_status);
	aml_chip->block_status = NULL;

	kfree(aml_chip->shipped_bbt_ptr);
	aml_chip->shipped_bbt_ptr = NULL;

	kfree(aml_chip->config_ptr);
	aml_chip->config_ptr = NULL;

	kfree(aml_chip->phy_part_ptr);
	aml_chip->phy_part_ptr = NULL;

	kfree(aml_chip->user_oob_buf);
	aml_chip->user_oob_buf = NULL;

	kfree(aml_chip->user_page_buf);
	aml_chip->user_page_buf = NULL;
	return ret;
}


