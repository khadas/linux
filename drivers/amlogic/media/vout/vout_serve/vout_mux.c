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
#include <linux/amlogic/clk_measure.h>

/* Local Headers */
#include "vout_func.h"
#include "vout_reg.h"

/* must be last includ file */
#include <linux/amlogic/gki_module.h>

struct vout_mux_data_s {
	int vdin_meas_id;
	void (*update_viu_mux)(int index, unsigned int mux_sel);
};

static struct vout_mux_data_s *vout_mux_data;

static inline unsigned int vout_do_div(unsigned long long num, unsigned int den)
{
	unsigned long long val = num;

	do_div(val, den);

	return (unsigned int)val;
}

unsigned int vout_frame_rate_measure(void)
{
	int clk_mux = 38;
	unsigned int val[2], fr;
	unsigned long long msr_clk;

	if (vout_mux_data)
		clk_mux = vout_mux_data->vdin_meas_id;

	if (clk_mux == -1)
		return 0;
	msr_clk = meson_clk_measure(clk_mux);

	val[0] = vout_vcbus_read(VPP_VDO_MEAS_CTRL);
	if (val[0]) {
		vout_vcbus_write(VPP_VDO_MEAS_CTRL, 0);
		msleep(200);
	}
	val[0] = vout_vcbus_read(VPP_VDO_MEAS_VS_COUNT_HI);
	val[1] = vout_vcbus_read(VPP_VDO_MEAS_VS_COUNT_LO);
	msr_clk *= 1000;
	if (val[0] & 0xffff)
		return 0;
	fr = vout_do_div(msr_clk, val[1]);

	return fr;
}

/* **********************************************************
 * vout venc mux
 * **********************************************************
 */
static void vout_viu_mux_update_default(int index, unsigned int mux_sel)
{
	unsigned int viu_sel, mux_bit = 0xff;
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
	viu_sel = mux_sel & 0xf;

	/*
	 *VOUTPR("%s: before: 0x%04x=0x%08x, 0x%04x=0x%08x\n",
	 *	__func__, VPU_VIU_VENC_MUX_CTRL,
	 *	vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL),
	 *	VPU_VENCX_CLK_CTRL,
	 *	vout_vcbus_read(VPU_VENCX_CLK_CTRL));
	 */

	switch (viu_sel) {
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
		vout_vcbus_setb(VPU_VIU_VENC_MUX_CTRL, viu_sel, mux_bit, 2);
	if (clk_bit < 0xff)
		vout_vcbus_setb(VPU_VENCX_CLK_CTRL, clk_sel, clk_bit, 1);
	/*
	 *VOUTPR("%s: %d, viu_sel=%d, mux_bit=%d, clk_sel=%d clk_bit=%d\n",
	 *	__func__, index, viu_sel, mux_bit, clk_sel, clk_bit);
	 *VOUTPR("%s: after: 0x%04x=0x%08x, 0x%04x=0x%08x\n",
	 *	__func__, VPU_VIU_VENC_MUX_CTRL,
	 *	vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL),
	 *	VPU_VENCX_CLK_CTRL,
	 *	vout_vcbus_read(VPU_VENCX_CLK_CTRL));
	 */
}

static void vout_viu_mux_update_t7(int index, unsigned int mux_sel)
{
	unsigned int viu_bit = 0xff, venc_idx;
	unsigned int viu_sel;
	unsigned int reg_value, venc_dmy;

	switch (index) {
	case 1:
		viu_bit = 0;
		break;
	case 2:
		viu_bit = 2;
		break;
	case 3:
		viu_bit = 4;
		break;
	default:
		VOUTERR("%s: invalid index %d\n", __func__, index);
		return;
	}
	viu_sel = mux_sel & 0xf;
	venc_idx = (mux_sel >> 4) & 0xf;

	/* viu_mux: viu0_sel: 0=venc0, 1=venc1, 2=venc2, 3=invalid */

	if (viu_sel == VIU_MUX_PROJECT) {
		reg_value = vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL);
		venc_idx = (reg_value >> 2) & 0x3;
		venc_dmy = reg_value & 0x3;
		reg_value &= ~(0xf);
		reg_value |= ((1 << 8) | (venc_dmy << 2) | venc_idx);
		vout_vcbus_write(VPU_VIU_VENC_MUX_CTRL, reg_value);
	} else {
		vout_vcbus_setb(VPU_VIU_VENC_MUX_CTRL, venc_idx, viu_bit, 2);
	}

	/*VOUTPR("%s: index=%d, mux_sel=0x%x, venc_idx=%d\n",
	 *	__func__, index, mux_sel, venc_idx);
	 *VOUTPR("%s: 0x%04x=0x%08x\n",
	 *	__func__, VPU_VIU_VENC_MUX_CTRL,
	 *	vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL));
	 */
}

static void vout_viu_mux_update_t3(int index, unsigned int mux_sel)
{
	unsigned int viu_bit = 0xff, venc_idx;
	unsigned int viu_sel;
	unsigned int reg_value, venc_dmy;

	switch (index) {
	case 1:
		viu_bit = 0;
		break;
	case 2:
		viu_bit = 2;
		break;
	default:
		VOUTERR("%s: invalid index %d\n", __func__, index);
		return;
	}
	viu_sel = mux_sel & 0xf;
	venc_idx = (mux_sel >> 4) & 0xf;

	/* viu_mux: viu0_sel: 0=venc0, 1=venc1, 2=venc2, 3=invalid */

	if (viu_sel == VIU_MUX_PROJECT) {
		reg_value = vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL);
		venc_idx = (reg_value >> 2) & 0x3;
		venc_dmy = reg_value & 0x3;
		reg_value &= ~(0xf);
		reg_value |= ((1 << 8) | (venc_dmy << 2) | venc_idx);
		vout_vcbus_write(VPU_VIU_VENC_MUX_CTRL, reg_value);
	} else {
		vout_vcbus_setb(VPU_VIU_VENC_MUX_CTRL, venc_idx, viu_bit, 2);
	}

	/* VOUTPR("%s: index=%d, mux_sel=0x%x, venc_idx=%d\n",
	 *       __func__, index, mux_sel, venc_idx);
	 * VOUTPR("%s: 0x%04x=0x%08x\n",
	 *	__func__, VPU_VIU_VENC_MUX_CTRL,
	 *	vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL));
	 */
}

void vout_viu_mux_update(int index, unsigned int mux_sel)
{
	/* for default case */
	if (!vout_mux_data) {
		vout_viu_mux_update_default(index, mux_sel);
		return;
	}

	/* for new case */
	if (vout_mux_data->update_viu_mux)
		vout_mux_data->update_viu_mux(index, mux_sel);
}

/* ********************************************************* */
static struct vout_mux_data_s vout_mux_match_data = {
	.vdin_meas_id = 38,
	.update_viu_mux = vout_viu_mux_update_default,
};

static struct vout_mux_data_s vout_mux_match_data_t7 = {
	.vdin_meas_id = 60,
	.update_viu_mux = vout_viu_mux_update_t7,
};

static struct vout_mux_data_s vout_mux_match_data_t3 = {
	.vdin_meas_id = 60,
	.update_viu_mux = vout_viu_mux_update_t3,
};

static const struct of_device_id vout_mux_dt_match_table[] = {
	{
		.compatible = "amlogic, vout_mux",
		.data = &vout_mux_match_data,
	},
	{
		.compatible = "amlogic, vout_mux-t7",
		.data = &vout_mux_match_data_t7,
	},
	{
		.compatible = "amlogic, vout_mux-t3",
		.data = &vout_mux_match_data_t3,
	},
	{}
};

static int vout_mux_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;

	match = of_match_device(vout_mux_dt_match_table, &pdev->dev);
	if (!match) {
		VOUTERR("%s: no match table\n", __func__);
		return -1;
	}
	vout_mux_data = (struct vout_mux_data_s *)match->data;

	VOUTPR("%s OK\n", __func__);

	return 0;
}

static int vout_mux_remove(struct platform_device *pdev)
{
	VOUTPR("%s\n", __func__);
	return 0;
}

static struct platform_driver vout_mux_platform_driver = {
	.probe = vout_mux_probe,
	.remove = vout_mux_remove,
	.driver = {
		.name = "vout_mux",
		.owner = THIS_MODULE,
		.of_match_table = vout_mux_dt_match_table,
	},
};

int __init vout_mux_init(void)
{
	if (platform_driver_register(&vout_mux_platform_driver)) {
		VOUTERR("failed to register vout_mux driver module\n");
		return -ENODEV;
	}

	return 0;
}

void __exit vout_mux_exit(void)
{
	platform_driver_unregister(&vout_mux_platform_driver);
}

