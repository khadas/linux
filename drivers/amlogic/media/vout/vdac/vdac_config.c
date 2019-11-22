/*
 * drivers/amlogic/media/vout/vdac/vdac_config.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#include <linux/platform_device.h>
#include <linux/of_device.h>
#include "vdac_dev.h"

static struct meson_vdac_ctrl_s vdac_ctrl_enable_gxl[] = {
	{HHI_VDAC_CNTL0, 0, 9, 1},
	{HHI_VDAC_CNTL0, 1, 10, 1},
	{HHI_VDAC_CNTL0, 1, 0, 1},
	{HHI_VDAC_CNTL1, 0, 3, 1},
	{VDAC_REG_MAX, 0, 0, 0},
};

static struct meson_vdac_ctrl_s vdac_ctrl_enable_txl[] = {
	{HHI_VDAC_CNTL0, 1, 9, 1},
	{HHI_VDAC_CNTL0, 1, 10, 1},
	{HHI_VDAC_CNTL0, 1, 0, 1},
	{HHI_VDAC_CNTL1, 1, 3, 1},
	{VDAC_REG_MAX, 0, 0, 0},
};

static struct meson_vdac_ctrl_s vdac_ctrl_enable_txlx[] = {
	{HHI_VDAC_CNTL0, 1, 9, 1},
	{HHI_VDAC_CNTL0, 1, 10, 1},
	{HHI_VDAC_CNTL0, 1, 0, 1},
	{HHI_VDAC_CNTL0, 0, 13, 1}, /* bandgap */
	{HHI_VDAC_CNTL1, 1, 3, 1},
	{VDAC_REG_MAX, 0, 0, 0},
};

static struct meson_vdac_ctrl_s vdac_ctrl_enable_g12ab[] = {
	{HHI_VDAC_CNTL0_G12A, 0, 9, 1},
	{HHI_VDAC_CNTL0_G12A, 1, 0, 1},
	{HHI_VDAC_CNTL1_G12A, 0, 3, 1},
	{VDAC_REG_MAX, 0, 0, 0},
};

static struct meson_vdac_ctrl_s vdac_ctrl_enable_tl1[] = {
	{HHI_VDAC_CNTL0_G12A, 0, 9, 1},
	{HHI_VDAC_CNTL0_G12A, 1, 0, 1},
	{HHI_VDAC_CNTL1_G12A, 0, 3, 1},
	{HHI_VDAC_CNTL1_G12A, 0, 7, 1}, /* bandgap */
	{HHI_VDAC_CNTL1_G12A, 1, 6, 1}, /* bypass avout */
	{HHI_VDAC_CNTL1_G12A, 1, 8, 1}, /* bypass avout */
	{VDAC_REG_MAX, 0, 0, 0},
};

/* ********************************************************* */
static struct meson_vdac_data meson_gx_l_m_vdac_data = {
	.cpu_id = VDAC_CPU_GX_L_M,
	.name = "meson-gx_l_m-vdac",

	.reg_cntl0 = HHI_VDAC_CNTL0,
	.reg_cntl1 = HHI_VDAC_CNTL1,
	.ctrl_table = vdac_ctrl_enable_gxl,
};

static struct meson_vdac_data meson_txl_vdac_data = {
	.cpu_id = VDAC_CPU_TXL,
	.name = "meson-txl-vdac",

	.reg_cntl0 = HHI_VDAC_CNTL0,
	.reg_cntl1 = HHI_VDAC_CNTL1,
	.ctrl_table = vdac_ctrl_enable_txl,
};

static struct meson_vdac_data meson_txlx_vdac_data = {
	.cpu_id = VDAC_CPU_TXLX,
	.name = "meson-txlx-vdac",

	.reg_cntl0 = HHI_VDAC_CNTL0,
	.reg_cntl1 = HHI_VDAC_CNTL1,
	.ctrl_table = vdac_ctrl_enable_txlx,
};

static struct meson_vdac_data meson_gxlx_vdac_data = {
	.cpu_id = VDAC_CPU_GXLX,
	.name = "meson-gxlx-vdac",

	.reg_cntl0 = HHI_VDAC_CNTL0,
	.reg_cntl1 = HHI_VDAC_CNTL1,
	.ctrl_table = vdac_ctrl_enable_txl,
};

static struct meson_vdac_data meson_g12ab_vdac_data = {
	.cpu_id = VDAC_CPU_G12AB,
	.name = "meson-g12ab-vdac",

	.reg_cntl0 = HHI_VDAC_CNTL0_G12A,
	.reg_cntl1 = HHI_VDAC_CNTL1_G12A,
	.ctrl_table = vdac_ctrl_enable_g12ab,
};

static struct meson_vdac_data meson_tl1_vdac_data = {
	.cpu_id = VDAC_CPU_TL1,
	.name = "meson-tl1-vdac",

	.reg_cntl0 = HHI_VDAC_CNTL0_G12A,
	.reg_cntl1 = HHI_VDAC_CNTL1_G12A,
	.ctrl_table = vdac_ctrl_enable_tl1,
};

static struct meson_vdac_data meson_sm1_vdac_data = {
	.cpu_id = VDAC_CPU_SM1,
	.name = "meson-sm1-vdac",

	.reg_cntl0 = HHI_VDAC_CNTL0_G12A,
	.reg_cntl1 = HHI_VDAC_CNTL1_G12A,
	.ctrl_table = vdac_ctrl_enable_g12ab,
};

static struct meson_vdac_data meson_tm2_vdac_data = {
	.cpu_id = VDAC_CPU_TM2,
	.name = "meson-tm2-vdac",

	.reg_cntl0 = HHI_VDAC_CNTL0_G12A,
	.reg_cntl1 = HHI_VDAC_CNTL1_G12A,
	.ctrl_table = vdac_ctrl_enable_tl1,
};

const struct of_device_id meson_vdac_dt_match[] = {
	{
		.compatible = "amlogic, vdac-gxl",
		.data		= &meson_gx_l_m_vdac_data,
	}, {
		.compatible = "amlogic, vdac-gxm",
		.data		= &meson_gx_l_m_vdac_data,
	}, {
		.compatible = "amlogic, vdac-txl",
		.data		= &meson_txl_vdac_data,
	}, {
		.compatible = "amlogic, vdac-txlx",
		.data		= &meson_txlx_vdac_data,
	}, {
		.compatible = "amlogic, vdac-gxlx",
		.data		= &meson_gxlx_vdac_data,
	}, {
		.compatible = "amlogic, vdac-g12a",
		.data		= &meson_g12ab_vdac_data,
	}, {
		.compatible = "amlogic, vdac-g12b",
		.data		= &meson_g12ab_vdac_data,
	}, {
		.compatible = "amlogic, vdac-tl1",
		.data		= &meson_tl1_vdac_data,
	}, {
		.compatible = "amlogic, vdac-sm1",
		.data		= &meson_sm1_vdac_data,
	}, {
		.compatible = "amlogic, vdac-tm2",
		.data		= &meson_tm2_vdac_data,
	},
	{}
};

struct meson_vdac_data *aml_vdac_config_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct meson_vdac_data *data;

	match = of_match_device(meson_vdac_dt_match, &pdev->dev);
	if (match == NULL) {
		pr_err("%s, no matched table\n", __func__);
		return NULL;
	}
	data = (struct meson_vdac_data *)match->data;
	if (data) {
		pr_info("%s: cpu_id:%d, name:%s\n", __func__,
			data->cpu_id, data->name);
	}

	return data;
}
