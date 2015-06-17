/*
 * drivers/amlogic/amlnf/phy/chip.c
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

static int get_flash_type(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &(aml_chip->controller);
	struct chip_operation *operation = &(aml_chip->operation);
	unsigned char dev_id[MAX_ID_LEN] = {0};
	struct nand_flash *type = NULL;
	int ret = 0, i, extid;

	ret = operation->read_id(aml_chip,
		0,
		NAND_CMD_ID_ADDR_NORMAL,
		&dev_id[0]);
	if (ret < 0) {
		aml_nand_dbg("read id failed and ret:0x%x", ret);
		goto error_exit;
	}

	aml_nand_msg("NAND device id: %x %x %x %x %x %x %x %x",
			dev_id[0],
			dev_id[1],
			dev_id[2],
			dev_id[3],
			dev_id[4],
			dev_id[5],
			dev_id[6],
			dev_id[7]);

#ifdef AML_SLC_NAND_SUPPORT
	/* Lookup the slc flash id */
	for (i = 0; flash_ids_slc[i].name != NULL; i++) {
		if (dev_id[1] == flash_ids_slc[i].id[1]) {
			type = &flash_ids_slc[i];
			break;
		}
	}
#endif
	if (type) {
		aml_nand_msg("detect slc nand here");
		controller->flash_type = NAND_TYPE_SLC;
	} else {
#ifdef AML_MLC_NAND_SUPPORT
		/* Lookup the mlc flash id */
		for (i = 0; flash_ids_mlc[i].name != NULL; i++) {
			if (!strncmp((char *)flash_ids_mlc[i].id,
				(char *)dev_id,
				strlen((const char *)flash_ids_mlc[i].id))) {
				type = &flash_ids_mlc[i];
				break;
			}
		}
#endif
		if (!type) {
			aml_nand_msg("no matched id");
			ret = -NAND_ID_FAILURE;
			goto error_exit;
		}
		controller->flash_type = NAND_TYPE_MLC;
	}

	memcpy(&(aml_chip->flash), type, sizeof(struct nand_flash));
	controller->mfr_type = type->id[0];

	aml_nand_dbg("check type->T_REA:%d, type->T_RHOH:%d, flash:%d %d",
		type->T_REA,
		type->T_RHOH,
		aml_chip->flash.T_REA,
		aml_chip->flash.T_RHOH);

	aml_nand_msg("detect NAND device: %s", type->name);

#ifdef AML_SLC_NAND_SUPPORT
	type = &aml_chip->flash;

	/* Newer devices have all the information in additional id bytes */
	if (!type->pagesize) {
		/* The 3rd id byte holds MLC / multichip data */
		controller->readbyte(controller);
		/* The 4th id byte is the important one */
		extid = controller->readbyte(controller);
		/* Calc pagesize */
		type->pagesize = 1024 << (extid & 0x3);
		extid >>= 2;
		/* Calc oobsize */
		type->oobsize = (8 << (extid & 0x01)) * (type->pagesize>>9);
		extid >>= 2;
		/* Calc blocksize. Blocksize is multiples of 64KiB */
		type->blocksize = (64 * 1024) << (extid & 0x03);
		extid >>= 2;
		/* Get buswidth information */
		aml_nand_msg("detect nand buswidth:%s",
				(extid & 0x02) ? "16bit" : "8bit");
		if (extid & 0x02) {
			aml_nand_msg("do not support 16bit buswidth yet");
			ret = -NAND_ID_FAILURE;
			goto error_exit;
		}
	}
#endif

#ifdef AML_MLC_NAND_SUPPORT
	/* read onfi id */
	ret = operation->read_id(aml_chip,
		0,
		NAND_CMD_ID_ADDR_ONFI,
		&dev_id[0]);
	if (ret < 0) {
		aml_nand_msg("read id failed and ret:0x%x", ret);
		goto error_exit;
	}

	controller->page_shift =  ffs(aml_chip->flash.pagesize) - 1;
	controller->block_shift =  ffs(aml_chip->flash.blocksize) - 1;
	controller->internal_page_nums =
		((aml_chip->flash.chipsize<<(20 - controller->page_shift)) /
		aml_chip->flash.internal_chipnr);
	aml_nand_dbg("internal_page_nums =%d,internal_chipnr=%d",
			controller->internal_page_nums,
			aml_chip->flash.internal_chipnr);
	if (!memcmp((char *)dev_id, "ONFI", 4))
		controller->onfi_mode = type->onfi_mode;
#endif
error_exit:
	return ret;
}


/*
  * fill amlnand_chip struct.
  * including chip partnum detect and multi-chip num detect function.
  *
  */
static int amlnand_chip_scan(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &(aml_chip->controller);
	struct chip_operation *operation = &(aml_chip->operation);
	unsigned char dev_id[MAX_ID_LEN] = {0};
	unsigned char onfi_features[4] = {0};
	int i, chip_num, ret = 0;

	/* should setting nand pinmux first */
	nand_get_chip(aml_chip);

	ret = get_flash_type(aml_chip);
	if (ret < 0) {
		aml_nand_msg("get_chip_type and ret:%x", ret);
		goto error_exit0;
	}

	controller->chip_num = MAX_CHIP_NUM;/* 1; */
	controller->option |= NAND_CTRL_NONE_RB;
	chip_num = 1;

	/*ce0 is always valid.*/
	aml_chip->ce_bit_mask |= 0x01;
		/* Check for a chip array */
	for (i = 1; i < MAX_CHIP_NUM; i++) {
		memset(&dev_id[0], 0, MAX_ID_LEN);
		ret = operation->read_id(aml_chip,
			i,
			NAND_CMD_ID_ADDR_NORMAL,
			&dev_id[0]);
		if (ret < 0) {
			aml_nand_dbg("read id failed and ret:%d", ret);
			continue;
		}
		/*
		memcmp((char*)&(aml_chip->flash.id[0]),
			(char*)&dev_id[0], MAX_ID_LEN)
		*/
		aml_nand_dbg("controller->flash_type =%d",
			controller->flash_type);
		if (((controller->flash_type == NAND_TYPE_SLC)
			|| (controller->flash_type == NAND_TYPE_MLC))
&& (aml_chip->flash.id[1] == dev_id[1])) {
			controller->ce_enable[chip_num] =
				(((CE_PAD_DEFAULT >> i*4) & 0xf) << 10);
			controller->rb_enable[chip_num] =
				(((RB_PAD_DEFAULT >> i*4) & 0xf) << 10);
			chip_num++;
			aml_chip->ce_bit_mask |= (1 << i);
		}
	}
	/* aml_nand_msg("nand chip ce mask %0x", aml_chip->ce_bit_mask); */

	controller->chip_num = chip_num;

	if (controller->chip_num > 1) {
		if (controller->chip_num == MAX_CHIP_NUM) {
			aml_chip->flash.option &= ~(NAND_MULTI_PLANE_MODE);
			aml_nand_msg("detect %d NF chips,disable twoplane mode",
				controller->chip_num);
		} else
			aml_nand_msg("detected %d NAND chips",
				controller->chip_num);
	}

	if (controller->onfi_mode) {
		operation->set_onfi_para(aml_chip,
			(unsigned char *)&(controller->onfi_mode),
			ONFI_TIMING_ADDR);
		operation->get_onfi_para(aml_chip,
			onfi_features,
			ONFI_TIMING_ADDR);
		if (onfi_features[0] != controller->onfi_mode) {
			aml_chip->flash.T_REA = DEFAULT_T_REA;
			aml_chip->flash.T_RHOH = DEFAULT_T_RHOH;
			aml_nand_msg("onfi timing mode set failed: %x",
				onfi_features[0]);
		}
	}

	ret = NAND_SUCCESS;
error_exit0:
	/* should clear nand pinmux here */
	nand_release_chip(aml_chip);
	return ret;
}

/*
  * fill aml_nand_buf_init struct.
  * malloc tmp buf and dma buf here.
  *
*/
static int nand_buf_init(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &(aml_chip->controller);
	struct nand_flash *flash = &aml_chip->flash;
	unsigned int buf_size;
	int err = 0;

	controller->ecc_unit = NAND_ECC_UNIT_SIZE;

#if (AML_CFG_CONHERENT_BUFFER)
	controller->data_buf = dma_alloc_coherent(aml_chip->device,
		(flash->pagesize + flash->oobsize),
		&controller->data_dma_addr, GFP_KERNEL);
#else
		controller->data_buf =
			aml_nand_malloc(flash->pagesize + flash->oobsize);
#endif
	if (!controller->data_buf) {
		aml_nand_msg("no memory for data buf, and need %x",
			(flash->pagesize + flash->oobsize));
		err = -NAND_MALLOC_FAILURE;
		goto exit_error0;
	}

	buf_size = (flash->pagesize / controller->ecc_unit)*PER_INFO_BYTE;
	buf_size += 16;

	controller->user_buf = dma_alloc_coherent(aml_chip->device,
		buf_size,
		&(controller->info_dma_addr),
		GFP_KERNEL);/* amlnf_dma_malloc(buf_size, 1); */

	if (!controller->user_buf) {
		aml_nand_msg("no memory for usr info buf, and need %x",
			buf_size);
		err = -NAND_MALLOC_FAILURE;
		goto exit_error1;
	}

	buf_size = (flash->pagesize + flash->oobsize) * controller->chip_num;
	if (flash->option & NAND_MULTI_PLANE_MODE)
		buf_size <<= 1;

	controller->page_buf = aml_nand_malloc(buf_size);
	if (!controller->page_buf) {
		aml_nand_msg("no memory for data buf, and need %x", buf_size);
		err = -NAND_MALLOC_FAILURE;
		goto exit_error2;
	}

	buf_size = flash->oobsize * controller->chip_num;
	if (flash->option & NAND_MULTI_PLANE_MODE)
		buf_size <<= 1;

	controller->oob_buf = aml_nand_malloc(buf_size);
	if (!controller->oob_buf) {
		aml_nand_msg("no memory for data buf, and need %x", buf_size);
		err = -NAND_MALLOC_FAILURE;
		goto exit_error3;
	}

	/*
	if (request_irq(INT_NAND,
		(irq_handler_t)nand_interrupt_monitor,
		0,
		"anl_nand",
		aml_chip)) {
		printk("request SDIO irq error!!!\n");
		return -1;
	}
	*/
	return NAND_SUCCESS;
exit_error3:
	aml_nand_free(controller->page_buf);
exit_error2:
	amlnf_dma_free(controller->user_buf,
		(flash->pagesize / controller->ecc_bytes)*sizeof(int),
		1);
exit_error1:
#if (AML_CFG_CONHERENT_BUFFER)
	amlnf_dma_free(controller->data_buf,
		(flash->pagesize + flash->oobsize),
		0);
#else
	aml_nand_free(controller->data_buf);
#endif
exit_error0:
	return err;
}

/*
 * fill free malloc buf here.
 *
 *
*/
static void nand_buf_free(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &aml_chip->controller;
	struct nand_flash *flash = &aml_chip->flash;
#if (AML_CFG_CONHERENT_BUFFER)
	amlnf_dma_free(controller->data_buf,
		(flash->pagesize + flash->oobsize),
		0);
#else
	aml_nand_free(controller->data_buf);
#endif
	amlnf_dma_free(controller->user_buf,
		(flash->pagesize / controller->ecc_bytes)*sizeof(int),
		1);
	aml_nand_free(controller->page_buf);
	aml_nand_free(controller->oob_buf);
}

/*
  * check rb pin here.
  * if without rb pin, then setting NAND_CTRL_NONE_RB mode
  *
*/
#if 0
static void aml_chip_rb_mode_confirm(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &aml_chip->controller;
	unsigned int por_cfg = 0, rb_mode = 0;
	void __iomem *poc_cfg_reg = NULL;

	poc_cfg_reg = aml_nand_dev->platform_data->poc_cfg_reg;

	if (controller->chip_num > 2) {
		aml_nand_msg("force NO RB pin and chip_num:%d over 2",
			controller->chip_num);
		rb_mode = 1;
	} else {
		por_cfg = amlnf_read_reg32(poc_cfg_reg);
		aml_nand_msg("detect RB pin here and por_cfg:%x", por_cfg);
		if (por_cfg&POC_NAND_NO_RB) {
			aml_nand_msg("detect without RB pin here");
			rb_mode = 1;
		} else {
			aml_nand_msg("detect with RB pin here");
			controller->option &= ~NAND_CTRL_NONE_RB;
		}
	}

#ifdef AML_NAND_RB_IRQ
	rb_mode = 1;
	aml_nand_msg("force none rb mode for rb irq");
#endif

	if (rb_mode) {
		controller->rb_enable[0] = 0;
		controller->option |= NAND_CTRL_NONE_RB;
	}
}
#else
static void aml_chip_rb_mode_confirm(struct amlnand_chip *aml_chip)
{
	unsigned int rb_mode = 0;
	struct hw_controller *controller = &aml_chip->controller;

	if (rb_mode) {
		controller->rb_enable[0] = 0;
		controller->option |= NAND_CTRL_NONE_RB;
	}
}
#endif

void amlchip_dumpinfo(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &(aml_chip->controller);
	struct nand_flash *flash = &(aml_chip->flash);

	/* flash info */
	aml_nand_msg("flash  info");
	aml_nand_msg("name:%s, id:%2x %2x %2x %2x %2x %2x %2x %2x",
		flash->name,
		flash->id[0],
		flash->id[1],
		flash->id[2],
		flash->id[3],
		flash->id[4],
		flash->id[5],
		flash->id[6],
		flash->id[7]);
	aml_nand_msg("pgs:%x,blks:%x,oobs:%d,chips:%d,opt:0x%x,T_R:%d,T_RH:%d",
		flash->pagesize,
		flash->blocksize,
		flash->oobsize,
		flash->chipsize,
		flash->option,
		flash->T_REA,
		flash->T_RHOH);
	aml_nand_msg("hw controller info\n");
	aml_nand_msg("chipn:%d,onfi:%d,pg_shift:%d,blk_shift:%d,option:0x%x",
		controller->chip_num,
		controller->onfi_mode,
		controller->page_shift,
		controller->block_shift,
		controller->option);
	aml_nand_msg("ecc_unit:%d, ecc_bytes:%d, ecc_steps:%d, ecc_max:%d",
		controller->ecc_unit,
		controller->ecc_bytes,
		controller->ecc_steps,
		controller->ecc_max);
	aml_nand_msg("bch_mode:%d, user_mode:%d, oobavail:%d, oobtail:%d",
		controller->bch_mode,
		controller->user_mode,
		controller->oobavail,
		controller->oobtail);
}

int amlchip_opstest(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &(aml_chip->controller);
	struct chip_operation *operation = &(aml_chip->operation);
	struct nand_flash *flash = &(aml_chip->flash);
	struct chip_ops_para *ops_para = &(aml_chip->ops_para);
	uint64_t addr, opslen = 0, len = 0;
	unsigned int erase_shift, write_shift, writesize, erasesize;
	int i, ret = 0;

	/* should setting nand pinmux first */
	nand_get_chip(aml_chip);

	/* clear ops_para here */
	memset(ops_para, 0, sizeof(struct chip_ops_para));

	writesize = flash->pagesize;
	erasesize = flash->blocksize;

	/* ops_para->option = (DEV_ECC_HW_MODE |DEV_SERIAL_CHIP_MODE ); */
	ops_para->option = (DEV_ECC_HW_MODE |
		NAND_MULTI_PLANE_MODE |
		DEV_SERIAL_CHIP_MODE);
	ops_para->data_buf = controller->page_buf;
	/* ops_para->oob_buf = controller->oobbuf; */

	if (ops_para->option & DEV_MULTI_PLANE_MODE) {
		writesize *= 2;
		erasesize *= 2;
	}

	if (ops_para->option & DEV_MULTI_CHIP_MODE) {
		writesize *= controller->chip_num;
		erasesize *= controller->chip_num;
	}

	erase_shift  = ffs(erasesize) - 1;
	write_shift  = ffs(writesize) - 1;

#if 0
	/* read */
	/* start addr 0, read whole chip size */
	opslen = addr = 0;
	len = ((uint64_t)(flash->chipsize*controller->chip_num))<<20;
	aml_nand_dbg("TEST step1 read whole chip,len:%llx, total_page:%d",
		len,
		len>>write_shift);

	while (1) {
		memset(ops_para->data_buf, 0, writesize);
		if (ops_para->option & DEV_SERIAL_CHIP_MODE) {
			ops_para->chipnr =
				(addr>>erase_shift)%controller->chip_num;
		}

		ops_para->page_addr =
			(int)(addr >> write_shift)/controller->chip_num;
		ret = operation->read_page(aml_chip);
		if (ret < 0) {
			aml_nand_msg("fail page_addr:%d", ops_para->page_addr);
			break;
		}
		aml_nand_dbg("page:%d data: %x %x %x %x", ops_para->page_addr,
			ops_para->data_buf[0],
			ops_para->data_buf[1],
			ops_para->data_buf[2],
			ops_para->data_buf[3]);

		addr += writesize;
		opslen += writesize;
		if (opslen >= len)
			break;

		if (ops_para->ecc_err)
			aml_nand_msg("ecc failed at page_addr:%d",
				ops_para->page_addr);
		else if (ops_para->bit_flip)
			aml_nand_msg("bit_flip at page_addr:%d",
				ops_para->page_addr);
	}
#endif
/* BUG(); */
#if 1
	/* erase */
	/* start addr 0, read whole chip size */
	aml_nand_dbg("TEST step2 erase whole chip");
	opslen = addr = 0;
	/* len = ((uint64_t)(flash->chipsize*controller->chip_num))<<20; */
	len = 1<<erase_shift;
	aml_nand_dbg("TEST step2 erase whole chip, len:%llx, total_page:%d",
		len,
		len>>write_shift);
	while (1) {
		if (ops_para->option & DEV_SERIAL_CHIP_MODE)
			ops_para->chipnr =
				(addr>>erase_shift)%controller->chip_num;

		ops_para->page_addr =
			(int)(addr >> write_shift)/controller->chip_num;
		ret = operation->erase_block(aml_chip);
		if (ret < 0)
			aml_nand_msg("fail page_addr:%d", ops_para->page_addr);
			break;

		addr += erasesize;
		opslen += erasesize;
		if (opslen >= len)
			break;
	}
#endif
	/* BUG(); */
	/* write */
	opslen = addr = 0;
	/* len = ((uint64_t)(flash->chipsize*controller->chip_num))<<20; */
	aml_nand_dbg("TEST step3 write whole chip, len:%llx, total_page:%d",
				len, len>>write_shift);
	for (i = 0; i < writesize; i++)
		ops_para->data_buf[i] = i;

	while (1) {
		if (ops_para->option & DEV_SERIAL_CHIP_MODE)
			ops_para->chipnr =
				(addr>>erase_shift)%controller->chip_num;

		ops_para->page_addr =
			(int)(addr >> write_shift)/controller->chip_num;
		ret = operation->write_page(aml_chip);
		if (ret < 0) {
			aml_nand_msg("fail page_addr:%d", ops_para->page_addr);
			break;
		}

		addr += writesize;
		/* ops_para->data_buf += writesize; */
		opslen += writesize;
		if (opslen >= len)
			break;
	}

	/* read */
	/* start addr 0, read whole chip size */
	opslen = addr = 0;
	/* len = ((uint64_t)(flash->chipsize*controller->chip_num))<<20; */
	aml_nand_dbg("TEST step4 read whole chip, len:%llx, total_page:%d",
		len,
		len>>write_shift);
	while (1) {
		memset(ops_para->data_buf, 0, writesize);
		if (ops_para->option & DEV_SERIAL_CHIP_MODE)
			ops_para->chipnr =
				(addr>>erase_shift)%controller->chip_num;

		ops_para->page_addr =
			(int)(addr >> write_shift)/controller->chip_num;
		ret = operation->read_page(aml_chip);
		if (ret < 0) {
			aml_nand_msg("fail page_addr:%d", ops_para->page_addr);
			break;
		}
		aml_nand_dbg("page:%d data: %x %x %x %x",
			ops_para->page_addr,
			ops_para->data_buf[0],
			ops_para->data_buf[1],
			ops_para->data_buf[2],
			ops_para->data_buf[3]);
		addr += writesize;
		/* ops_para->data_buf += writesize; */
		opslen += writesize;
		if (opslen >= len)
			break;

		if (ops_para->ecc_err)
			aml_nand_msg("ecc failed at page_addr:%d",
				ops_para->page_addr);
		else if (ops_para->bit_flip)
			aml_nand_msg("bit_flip at page_addr:%d",
				ops_para->page_addr);
	}

	/* should clear nand pinmux here */
	nand_release_chip(aml_chip);
	return 0;
}

/*
  * fill amlnand_chip struct.
  * including hw init, chip detect, option setting and operation function.
  *
  */
unsigned int amlnand_chip_init(struct amlnand_chip *aml_chip)
{
	struct hw_controller *controller = &(aml_chip->controller);
	/* struct chip_operation *operation = &aml_chip->operation; */
	int ret = 0;

	ret = amlnand_chip_scan(aml_chip);
	if (ret) {
		aml_nand_msg("get_chip_type and ret:%x", ret);
		goto error_exit0;
	}

	/* amlchip_dumpinfo(aml_chip); */

	ret = nand_buf_init(aml_chip);
	if (ret) {
		aml_nand_msg("buf init failed and ret:%x", ret);
		goto error_exit0;
	}

	ret = controller->adjust_timing(controller);
	if (ret) {
		aml_nand_msg("adjust_timing failed and ret:%x", ret);
		goto error_exit1;
	}

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_M8)
		aml_chip_rb_mode_confirm(aml_chip);

	ret = controller->ecc_confirm(controller);
	if (ret) {
		aml_nand_msg("buf init failed and ret:%x", ret);
		goto error_exit1;
	}

#ifdef AML_NAND_DBG
	amlchip_dumpinfo(aml_chip);
#endif

/* basic operation test here, read/write/erase */
#if 0
	amlchip_opstest(aml_chip);
#endif

	return NAND_SUCCESS;
error_exit1:
	nand_buf_free(aml_chip);
error_exit0:
	return ret;

}

