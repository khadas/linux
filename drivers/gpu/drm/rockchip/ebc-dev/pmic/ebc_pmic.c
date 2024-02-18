// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Rockchip Electronics Co. Ltd.
 *
 * Author: Zorro Liu <zorro.liu@rock-chips.com>
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/soc/rockchip/rk_vendor_storage.h>
#include "ebc_pmic.h"

#define EINK_VCOM_MAX 64
static int vcom = 0;

int ebc_pmic_set_vcom(struct ebc_pmic *pmic, int value)
{
	int ret;
	char data[EINK_VCOM_MAX] = { 0 };

	/* check vcom value */
	if (value <= VCOM_MIN_MV || value > VCOM_MAX_MV) {
		dev_err(pmic->dev, "vcom value should be %d~%d\n", VCOM_MIN_MV, VCOM_MAX_MV);
		return -1;
	}
	dev_info(pmic->dev, "set chip vcom to: %dmV\n", value);

	/* set pmic vcom */
	pmic->pmic_set_vcom(pmic, value);

	/* store vendor storage */
	snprintf(data, sizeof(data), "%d", value);
	dev_info(pmic->dev, "store vcom %d to vendor storage\n", value);

	ret = rk_vendor_write(EINK_VCOM_ID, (void *)data, EINK_VCOM_MAX);
	if (ret < 0) {
		dev_err(pmic->dev, "%s failed to write vendor storage\n", __func__);
		return ret;
	}

	return 0;
}

void ebc_pmic_verity_vcom(struct ebc_pmic *pmic)
{
	int ret;
	int value_chip;
	int value_vendor;

	//check vcom value
	value_vendor = vcom;
	if (value_vendor <= VCOM_MIN_MV || value_vendor > VCOM_MAX_MV) {
		dev_err(pmic->dev, "invaild vcom value %d from vendor storage\n", value_vendor);
		return;
	}
	value_chip = pmic->pmic_get_vcom(pmic);
	if (value_chip != value_vendor) {
		dev_info(pmic->dev, "chip_vcom %d != vendor_vcom %d, set vcom from vendor\n", value_chip, value_vendor);
		ret = pmic->pmic_set_vcom(pmic, value_vendor);
		if (ret) {
			dev_err(pmic->dev, "set vcom value failed\n");
		}
	}

	return;
}

int ebc_regulator_set_vcom(struct regulator *r, int value)
{
	int ret;
	char data[EINK_VCOM_MAX] = { 0 };

	/* check vcom value */
	if (value <= VCOM_MIN_MV || value > VCOM_MAX_MV) {
		pr_err("vcom value should be %d~%d\n", VCOM_MIN_MV, VCOM_MAX_MV);
		return -1;
	}

	pr_info("set chip vcom to: %dmV\n", value);

	ret = regulator_set_voltage(r, value * 1000, value * 1000 + 1);
	if (ret) {
		pr_err("Failed to set vcom:%d\n", ret);
		return ret;
	}

	/* store vendor storage */
	snprintf(data, sizeof(data), "%d", value);
	pr_info("store vcom %d to vendor storage\n", value);

	ret = rk_vendor_write(EINK_VCOM_ID, (void *)data, EINK_VCOM_MAX);
	if (ret < 0) {
		pr_err("%s failed to write vendor storage\n", __func__);
		return ret;
	}

	return 0;
}

void ebc_regulator_verity_vcom(struct regulator *r)
{
	int ret;
	int value_chip;
	int value_vendor;

	value_vendor = vcom;
	if (value_vendor <= VCOM_MIN_MV || value_vendor > VCOM_MAX_MV) {
		pr_err("invaild vcom value %d from vendor storage\n", value_vendor);
		return;
	}

	value_chip = regulator_get_voltage(r) / 1000;
	if (value_chip != value_vendor) {
		pr_info("chip_vcom %d != vendor_vcom %d, set vcom from vendor\n", value_chip,
			value_vendor);
		ret = regulator_set_voltage(r, value_vendor * 1000, value_vendor * 1000 + 1);
		if (ret)
			pr_err("set vcom value failed\n");
	}
}

module_param(vcom, int, 0644);
