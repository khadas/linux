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
#include <linux/amlogic/cpu_version.h>

/* Local Headers */
#include "vout_func.h"
#include "vout_reg.h"

/* must be last include file */
#include <linux/amlogic/gki_module.h>

#define VDIN_MEAS_CLK_FREQ    50000000  //50MHz
struct vout_mux_data_s {
	struct clk *msr_clk;
	unsigned int (*vs_measure)(int index);
	unsigned int (*vs_measure_high_res)(int index);
	void (*msr_ctrl_init)(struct platform_device *pdev, struct vout_mux_data_s *vdata);
	void (*update_viu_mux)(int index, unsigned int mux_sel);
	void (*clear_viu_mux)(int index, unsigned int mux_sel);
};

static struct vout_mux_data_s *vout_mux_data;
static int vout_vdo_meas_init;

/* **********************************************************
 * vout vsync measure
 * **********************************************************
 */
static void vout_meas_ctrl_init_dft(struct platform_device *pdev, struct vout_mux_data_s *vdata)
{
	vout_vcbus_write(VPP_VDO_MEAS_CTRL, 0);
	vout_vdo_meas_init = 1;
}

static void vout_meas_ctrl_init_s5(struct platform_device *pdev, struct vout_mux_data_s *vdata)
{
	struct clk *fclk_div5;
	unsigned int clk_msr_val;

	if (!vdata)
		return;

	fclk_div5 = clk_get(&pdev->dev, "fclk_div5");
	if (IS_ERR_OR_NULL(fclk_div5)) {
		VOUTERR("%s: get fclk_div5 err!\n", __func__);
		return;
	}
	vdata->msr_clk = clk_get(&pdev->dev, "vdin_meas_clk");
	if (IS_ERR_OR_NULL(vdata->msr_clk)) {
		VOUTERR("%s: get vdin_meas_clk err!\n", __func__);
		vdata->msr_clk = NULL;
		return;
	}

	clk_set_parent(vdata->msr_clk, fclk_div5);
	clk_set_rate(vdata->msr_clk, VDIN_MEAS_CLK_FREQ);
	clk_prepare_enable(vdata->msr_clk);
	clk_msr_val = clk_get_rate(vdata->msr_clk);
	VOUTPR("%s: vdin_meas_clk %dHZ\n", __func__, clk_msr_val);

	vout_vcbus_setb(VPU_VENC_CTRL, 0, 8, 11);
	vout_vdo_meas_init = 1;
}

static inline unsigned int vout_do_div(unsigned long long num, unsigned int den)
{
	unsigned long long val = num;

	if (den == 0)
		return 0;

	do_div(val, den);

	return (unsigned int)val;
}

static unsigned int vout_vs_measure(int index)
{
	unsigned int val[2], fr;
	unsigned long long clk_msr;

	if (index > 1)
		return 0;
	if (vout_vdo_meas_init == 0)
		return 0;

	clk_msr = VDIN_MEAS_CLK_FREQ;

	val[0] = vout_vcbus_read(VPP_VDO_MEAS_VS_COUNT_HI);
	val[1] = vout_vcbus_read(VPP_VDO_MEAS_VS_COUNT_LO);
	clk_msr *= 1000;
	if (val[0] & 0xffff)
		return 0;
	fr = vout_do_div(clk_msr, val[1]);

	return fr;
}

static unsigned int vout_vs_measure_high_res(int index)
{
	unsigned int val[2], fr;
	unsigned long long clk_msr;

	if (index > 1)
		return 0;
	if (vout_vdo_meas_init == 0)
		return 0;

	clk_msr = VDIN_MEAS_CLK_FREQ;

	val[0] = vout_vcbus_read(VPP_VDO_MEAS_VS_COUNT_HI);
	val[1] = vout_vcbus_read(VPP_VDO_MEAS_VS_COUNT_LO);
	clk_msr *= 1000000;
	if (val[0] & 0xffff)
		return 0;
	fr = vout_do_div(clk_msr, val[1]);
	if (vout_debug_print) {
		VOUTPR("%s: 0x%04x=0x%08x, clk_msr=%lld, fr=%d\n",
			__func__, VPP_VDO_MEAS_VS_COUNT_LO,
			vout_vcbus_read(VPP_VDO_MEAS_VS_COUNT_LO),
			clk_msr, fr);
	}

	return fr;
}

static unsigned int vout_vs_measure_s5(int index)
{
	struct vinfo_s *vinfo = NULL;
	unsigned int venc_index, val, fr;
	unsigned long long clk_msr = VDIN_MEAS_CLK_FREQ;

	if (vout_vdo_meas_init == 0)
		return 0;
	if (index == 1)
		vinfo = get_current_vinfo();
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		vinfo = get_current_vinfo2();
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		vinfo = get_current_vinfo3();
#endif
	if (!vinfo)
		return 0;
	venc_index = (vinfo->viu_mux >> 4) & 0xf;
	if (venc_index >= 1)
		return 0;

	clk_msr *= 1000;
	val = vout_vcbus_read(VPU_VENC_RO_MEAS0);
	fr = vout_do_div(clk_msr, val);

	return fr;
}

static unsigned int vout_vs_measure_high_res_s5(int index)
{
	struct vinfo_s *vinfo = NULL;
	unsigned int venc_index, val, fr;
	unsigned long long clk_msr = VDIN_MEAS_CLK_FREQ;

	if (vout_vdo_meas_init == 0)
		return 0;
	if (index == 1)
		vinfo = get_current_vinfo();
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	else if (index == 2)
		vinfo = get_current_vinfo2();
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (index == 3)
		vinfo = get_current_vinfo3();
#endif
	if (!vinfo)
		return 0;
	venc_index = (vinfo->viu_mux >> 4) & 0xf;
	if (venc_index >= 1)
		return 0;

	clk_msr *= 1000000;
	val = vout_vcbus_read(VPU_VENC_RO_MEAS0);
	fr = vout_do_div(clk_msr, val);

	return fr;
}

unsigned int vout_frame_rate_measure(int index)
{
	unsigned int fr;

	if (!vout_mux_data || !vout_mux_data->vs_measure)
		return 0;

	fr = vout_mux_data->vs_measure(index);
	return fr;
}

unsigned int vout_frame_rate_msr_high_res(int index)
{
	unsigned int fr;

	if (!vout_mux_data || !vout_mux_data->vs_measure_high_res)
		return 0;

	fr = vout_mux_data->vs_measure_high_res(index);
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

	if (vout_debug_print) {
		VOUTPR("%s: before: 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			__func__, VPU_VIU_VENC_MUX_CTRL,
			vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL),
			VPU_VENCX_CLK_CTRL,
			vout_vcbus_read(VPU_VENCX_CLK_CTRL));
	}

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
	if (vout_debug_print) {
		VOUTPR("%s: %d, viu_sel=%d, mux_bit=%d, clk_sel=%d clk_bit=%d\n",
			__func__, index, viu_sel, mux_bit, clk_sel, clk_bit);
		VOUTPR("%s: after: 0x%04x=0x%08x, 0x%04x=0x%08x\n",
			__func__, VPU_VIU_VENC_MUX_CTRL,
			vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL),
			VPU_VENCX_CLK_CTRL,
			vout_vcbus_read(VPU_VENCX_CLK_CTRL));
	}
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

	if (vout_debug_print) {
		VOUTPR("%s: index=%d, mux_sel=0x%x, venc_idx=%d\n",
			__func__, index, mux_sel, venc_idx);
		VOUTPR("%s: 0x%04x=0x%08x\n",
			__func__, VPU_VIU_VENC_MUX_CTRL,
			vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL));
	}
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

	if (vout_debug_print) {
		VOUTPR("%s: index=%d, mux_sel=0x%x, venc_idx=%d\n",
		__func__, index, mux_sel, venc_idx);
		VOUTPR("%s: 0x%04x=0x%08x\n",
			__func__, VPU_VIU_VENC_MUX_CTRL,
			vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL));
	}
}

static void vout_viu_mux_clear_t7(int index, unsigned int mux_sel)
{
	unsigned int viu_sel, venc_idx;
	unsigned int reg_value, venc_dmy;

	reg_value = vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL);
	if (vout_debug_print) {
		VOUTPR("%s: index=%d, mux_sel=0x%x\n", __func__, index, mux_sel);
		VOUTPR("%s: viu_mux reg=0x%x\n", __func__, reg_value);
	}

	viu_sel = mux_sel & 0xf;
	if (viu_sel == VIU_MUX_PROJECT) {
		venc_dmy = (reg_value >> 2) & 0x3;
		venc_idx = reg_value & 0x3;
		reg_value &= ~(0x10f);
		reg_value |= ((venc_idx << 2) | venc_dmy);
		vout_vcbus_write(VPU_VIU_VENC_MUX_CTRL, reg_value);
		VOUTPR("%s: update viu_mux reg=0x%x\n", __func__, reg_value);
	}
}

void vout_viu_mux_update(int index, unsigned int mux_sel)
{
	/* for c3/a4 no vout data */
	if ((is_meson_c3_cpu() || is_meson_a4_cpu()) &&
		!vout_mux_data)
		return;
	/* for default case */
	if (!vout_mux_data) {
		vout_viu_mux_update_default(index, mux_sel);
		return;
	}

	/* for new case */
	if (vout_mux_data->update_viu_mux)
		vout_mux_data->update_viu_mux(index, mux_sel);
}

void vout_viu_mux_clear(int index, unsigned int mux_sel)
{
	/* for default case */
	if (!vout_mux_data)
		return;

	/* for new case */
	if (vout_mux_data->clear_viu_mux)
		vout_mux_data->clear_viu_mux(index, mux_sel);
}

/* ********************************************************* */
static struct vout_mux_data_s vout_mux_match_data = {
	.msr_clk = NULL,
	.vs_measure = vout_vs_measure,
	.vs_measure_high_res = vout_vs_measure_high_res,
	.msr_ctrl_init = vout_meas_ctrl_init_dft,
	.update_viu_mux = vout_viu_mux_update_default,
	.clear_viu_mux = NULL,
};

static struct vout_mux_data_s vout_mux_match_data_t7 = {
	.msr_clk = NULL,
	.vs_measure = vout_vs_measure,
	.vs_measure_high_res = vout_vs_measure_high_res,
	.msr_ctrl_init = vout_meas_ctrl_init_dft,
	.update_viu_mux = vout_viu_mux_update_t7,
	.clear_viu_mux = vout_viu_mux_clear_t7,
};

static struct vout_mux_data_s vout_mux_match_data_t3 = {
	.msr_clk = NULL,
	.vs_measure = vout_vs_measure,
	.vs_measure_high_res = vout_vs_measure_high_res,
	.msr_ctrl_init = vout_meas_ctrl_init_dft,
	.update_viu_mux = vout_viu_mux_update_t3,
	.clear_viu_mux = vout_viu_mux_clear_t7,
};

static struct vout_mux_data_s vout_mux_match_data_t5w = {
	.msr_clk = NULL,
	.vs_measure = vout_vs_measure,
	.vs_measure_high_res = vout_vs_measure_high_res,
	.msr_ctrl_init = vout_meas_ctrl_init_dft,
	.update_viu_mux = vout_viu_mux_update_t3,
	.clear_viu_mux = vout_viu_mux_clear_t7,
};

static struct vout_mux_data_s vout_mux_match_data_s5 = {
	.msr_clk = NULL,
	.vs_measure = vout_vs_measure_s5,
	.vs_measure_high_res = vout_vs_measure_high_res_s5,
	.msr_ctrl_init = vout_meas_ctrl_init_s5,
	.update_viu_mux = vout_viu_mux_update_t3,
	.clear_viu_mux = vout_viu_mux_clear_t7,
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
	{
		.compatible = "amlogic, vout_mux-t5w",
		.data = &vout_mux_match_data_t5w,
	},
	{
		.compatible = "amlogic, vout_mux-s5",
		.data = &vout_mux_match_data_s5,
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

	if (vout_mux_data->msr_ctrl_init)
		vout_mux_data->msr_ctrl_init(pdev, vout_mux_data);

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

