/*
 * drivers/amlogic/amlnf/phy/amlnand_init.c
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

struct amlnand_chip *aml_nand_chip = NULL;

static void show_nand_driver_version(void)
{
	aml_nand_msg("Nand PHY Ver:%d.%02d.%03d.%04d (c) 2013 Amlogic Inc.",
		(DRV_PHY_VERSION >> 24)&0xff,
		(DRV_PHY_VERSION >> 16)&0xff,
		(DRV_PHY_VERSION >> 8)&0xff,
		(DRV_PHY_VERSION)&0xff);
}

int amlnf_phy_init(unsigned char flag, struct platform_device *pdev)
{
	struct amlnand_chip *aml_chip = NULL;
	int ret = 0;

	/*show nand phy version here.*/
	show_nand_driver_version();

	aml_chip = aml_nand_malloc(sizeof(struct amlnand_chip));
	if (aml_chip == NULL) {
		aml_nand_msg("malloc failed for aml_chip:%lx",
			sizeof(struct amlnand_chip));
		ret = -NAND_MALLOC_FAILURE;
		goto exit_error1;
	}
	memset(aml_chip , 0, sizeof(struct amlnand_chip));
	memset(aml_chip->reserved_blk, 0xff, RESERVED_BLOCK_CNT);
	aml_chip->init_flag = flag;
	aml_chip->nand_status = NAND_STATUS_NORMAL;
	aml_nand_chip = aml_chip;
	aml_chip->device = &pdev->dev;

#ifdef CONFIG_OF
	aml_chip->nand_pinctrl = devm_pinctrl_get(aml_chip->device);
	if (IS_ERR(aml_chip->nand_pinctrl)) {
		aml_nand_msg("get pinctrl error");
		return PTR_ERR(aml_chip->nand_pinctrl);
	}
	aml_chip->nand_rbstate = pinctrl_lookup_state(aml_chip->nand_pinctrl,
		"nand_rb_mod");
	if (IS_ERR(aml_chip->nand_rbstate)) {
		devm_pinctrl_put(aml_chip->nand_pinctrl);
		return PTR_ERR(aml_chip->nand_rbstate);
	}
	aml_chip->nand_norbstate = pinctrl_lookup_state(aml_chip->nand_pinctrl,
		"nand_norb_mod");
	if (IS_ERR(aml_chip->nand_norbstate)) {
		devm_pinctrl_put(aml_chip->nand_pinctrl);
		return PTR_ERR(aml_chip->nand_norbstate);
	}
#if (AML_CFG_PINMUX_ONCE_FOR_ALL)
	ret = pinctrl_select_state(aml_chip->nand_pinctrl ,
			aml_chip->nand_norbstate);
	if (ret < 0)
		aml_nand_msg("%s:%d %s can't get pinctrl",
			__func__,
			__LINE__,
			dev_name(aml_chip->device));
#endif
	aml_chip->nand_idlestate = pinctrl_lookup_state(aml_chip->nand_pinctrl,
		"nand_cs_pins_only");
	if (IS_ERR(aml_chip->nand_idlestate)) {
		devm_pinctrl_put(aml_chip->nand_pinctrl);
		return PTR_ERR(aml_chip->nand_idlestate);
	}
#endif
	/* Step 1: init hw controller */
	ret = amlnand_hwcontroller_init(aml_chip);
	if (ret < 0) {
		aml_nand_msg("aml_hw_controller_init failed");
		ret = -NAND_FAILED;
		goto exit_error1;
	}

	/* Step 2: init aml_chip operation */
	ret = amlnand_init_operation(aml_chip);
	if (ret < 0) {
		aml_nand_msg("chip detect failed and ret:%x", ret);
		ret = -NAND_FAILED;
		goto exit_error1;
	}

	/* Step 3: get nand flash and get hw flash information */
	ret = amlnand_chip_init(aml_chip);
	if (ret < 0) {
		aml_nand_msg("chip detect failed and ret:%x", ret);
		ret = -NAND_FAILED;
		goto exit_error1;
	}

	/* Step 4: get device configs */
	ret = amlnand_get_dev_configs(aml_chip);
	if (ret < 0) {
		aml_nand_msg("get device configs failed and ret:%x", ret);
		ret = -NAND_READ_FAILED;
		goto exit_error0;
	}

	/* Step 5: register nand device, and config device information */
	ret = amlnand_phydev_init(aml_chip);
	if (ret < 0) {
		aml_nand_msg("register nand device failed and ret:%x", ret);
		ret = -NAND_READ_FAILED;
		goto exit_error0;
	}
	return ret;
exit_error1:
	aml_nand_free(aml_chip);
exit_error0:

	return ret;
}

