// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Linux Headers */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of_device.h>

/* Local Headers */
#include "vout_func.h"
#include "vout_reg.h"

/* must be last includ file */
#include <linux/amlogic/gki_module.h>

struct vout_venc_data_s {
	unsigned int offset[3];
	void (*update_venc_mux)(int index, unsigned int mux_sel);
};

static struct vout_venc_data_s *vout_venc_data;

/* **********************************************************
 * vout venc mux
 * **********************************************************
 */
static void vout_venc_mux_update_default(int index, unsigned int mux_sel)
{
	unsigned int mux_bit = 0xff;
	unsigned int clk_bit = 0xff, clk_sel = 0;

	if (index == 1) {
		mux_bit = 0;
		clk_sel = 0;
	} else if (index == 2) {
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
		mux_bit = 2;
		clk_sel = 1;
#endif
	}

	/*
	 *VOUTPR("%s: before: 0x%04x=0x%08x, 0x%04x=0x%08x\n",
	 *	__func__, VPU_VIU_VENC_MUX_CTRL,
	 *	vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL),
	 *	VPU_VENCX_CLK_CTRL,
	 *	vout_vcbus_read(VPU_VENCX_CLK_CTRL));
	 */

	switch (mux_sel) {
	case VIU_MUX_ENCL:
		clk_bit = 1;
		break;
	case VIU_MUX_ENCI:
		clk_bit = 2;
		break;
	case VIU_MUX_ENCP:
		clk_bit = 0;
		break;
	default:
		break;
	}

	if (mux_bit < 0xff)
		vout_vcbus_setb(VPU_VIU_VENC_MUX_CTRL, mux_sel, mux_bit, 2);
	if (clk_bit < 0xff)
		vout_vcbus_setb(VPU_VENCX_CLK_CTRL, clk_sel, clk_bit, 1);
	/*
	 *VOUTPR("%s: %d, mux_sel=%d, mux_bit=%d, clk_sel=%d clk_bit=%d\n",
	 *	__func__, index, mux_sel, mux_bit, clk_sel, clk_bit);
	 *VOUTPR("%s: after: 0x%04x=0x%08x, 0x%04x=0x%08x\n",
	 *	__func__, VPU_VIU_VENC_MUX_CTRL,
	 *	vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL),
	 *	VPU_VENCX_CLK_CTRL,
	 *	vout_vcbus_read(VPU_VENCX_CLK_CTRL));
	 */
}

static void vout_venc_mux_update_t7(int index, unsigned int mux_sel)
{
	unsigned int mux_idx = 0, mux_val = 3;
	unsigned int reg;

	mux_idx = index - 1;
	if (mux_idx > 2) {
		VOUTERR("%s: invalid index %d\n", __func__, index);
		return;
	}

	switch (mux_sel) {
	case VIU_MUX_ENCL:
		mux_val = 2;
		break;
	case VIU_MUX_ENCI:
		mux_val = 0;
		break;
	case VIU_MUX_ENCP:
		mux_val = 1;
		break;
	default:
		mux_val = 3;
		break;
	}

	reg = VPU_VENC_CTRL + vout_venc_data->offset[mux_idx];
	vout_vcbus_write(reg, mux_val);
}

void vout_venc_mux_update(int index, unsigned int mux_sel)
{
	/* for default case */
	if (!vout_venc_data) {
		vout_venc_mux_update_default(index, mux_sel);
		return;
	}

	/* for new case */
	if (vout_venc_data->update_venc_mux)
		vout_venc_data->update_venc_mux(index, mux_sel);
}

/* ********************************************************* */
static struct vout_venc_data_s vout_venc_match_data = {
	.offset[0] = 0x0,
	.offset[1] = 0x0,
	.offset[2] = 0x0,
	.update_venc_mux = vout_venc_mux_update_default,
};

static struct vout_venc_data_s vout_venc_match_data_t7 = {
	.offset[0] = 0x0,
	.offset[1] = 0x600,
	.offset[2] = 0x800,
	.update_venc_mux = vout_venc_mux_update_t7,
};

static const struct of_device_id vout_venc_dt_match_table[] = {
	{
		.compatible = "amlogic, vout_venc",
		.data = &vout_venc_match_data,
	},
	{
		.compatible = "amlogic, vout_venc-t7",
		.data = &vout_venc_match_data_t7,
	},
	{}
};

static int vout_venc_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;

	match = of_match_device(vout_venc_dt_match_table, &pdev->dev);
	if (!match) {
		VOUTERR("%s: no match table\n", __func__);
		return -1;
	}
	vout_venc_data = (struct vout_venc_data_s *)match->data;

	VOUTPR("%s OK\n", __func__);

	return 0;
}

static int vout_venc_remove(struct platform_device *pdev)
{
	VOUTPR("%s\n", __func__);
	return 0;
}

static struct platform_driver vout_venc_platform_driver = {
	.probe = vout_venc_probe,
	.remove = vout_venc_remove,
	.driver = {
		.name = "vout_venc",
		.owner = THIS_MODULE,
		.of_match_table = vout_venc_dt_match_table,
	},
};

int __init vout_venc_init(void)
{
	if (platform_driver_register(&vout_venc_platform_driver)) {
		VOUTERR("failed to register vout_venc driver module\n");
		return -ENODEV;
	}

	return 0;
}

void __exit vout_venc_exit(void)
{
	platform_driver_unregister(&vout_venc_platform_driver);
}

