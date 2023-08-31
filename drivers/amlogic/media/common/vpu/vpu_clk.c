// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "vpu_reg.h"
#include "vpu.h"

unsigned int get_vpu_clk_level_max_vmod(void)
{
	unsigned int max_level = 0;
	int i;

	if (!vpu_conf.clk_vmod)
		return max_level;

	max_level = 0;
	for (i = 0; i < VPU_MOD_MAX; i++) {
		if (vpu_conf.clk_vmod[i] > max_level)
			max_level = vpu_conf.clk_vmod[i];
	}

	return max_level;
}

static unsigned int get_vpu_clk_level(unsigned int video_clk)
{
	unsigned int clk_level;
	int i;

	for (i = 0; i < vpu_conf.data->clk_level_max; i++) {
		if (video_clk <= vpu_conf.data->clk_table[i].freq)
			break;
	}
	clk_level = i;

	return clk_level;
}

static unsigned int get_fclk_div_freq(unsigned int mux_id)
{
	struct fclk_div_s *fclk_div;
	unsigned int fclk, div, clk_source = 0;
	unsigned int i;

	fclk = CLK_FPLL_FREQ * 1000000;

	for (i = 0; i < FCLK_DIV_MAX; i++) {
		fclk_div = vpu_conf.data->fclk_div_table + i;
		if (fclk_div->fclk_id == mux_id) {
			div = fclk_div->fclk_div;
			clk_source = fclk / div;
			break;
		}
		if (fclk_div->fclk_id == FCLK_DIV_MAX)
			break;
	}

	return clk_source;
}

static unsigned int get_vpu_clk_mux_id(void)
{
	struct fclk_div_s *fclk_div;
	unsigned int i, mux, mux_id;

	mux = vpu_clk_getb(vpu_conf.data->vpu_clk_reg, 9, 3);
	mux_id = mux;
	for (i = 0; i < FCLK_DIV_MAX; i++) {
		fclk_div = vpu_conf.data->fclk_div_table + i;
		if (fclk_div->fclk_mux == mux) {
			mux_id = fclk_div->fclk_id;
			break;
		}
		if (fclk_div->fclk_id == FCLK_DIV_MAX)
			break;
	}

	return mux_id;
}

unsigned int vpu_clk_get(void)
{
	unsigned int clk_freq;
	unsigned int clk_source, div;
	unsigned int mux_id;
	struct clk_hw *hw;
	int ret;

	ret = vpu_chip_valid_check();
	if (ret)
		return 0;

	if (IS_ERR_OR_NULL(vpu_conf.vpu_clk)) {
		VPUERR("%s: vpu_clk\n", __func__);
		mux_id = get_vpu_clk_mux_id();
		switch (mux_id) {
		case FCLK_DIV4:
		case FCLK_DIV3:
		case FCLK_DIV5:
		case FCLK_DIV7:
			clk_source = get_fclk_div_freq(mux_id);
			break;
		case GPLL_CLK:
			clk_source = vpu_conf.data->clk_table[8].freq;
			break;
		default:
			clk_source = 0;
			break;
		}

		div = vpu_clk_getb(vpu_conf.data->vpu_clk_reg, 0, 7) + 1;
		clk_freq = clk_source / div;

		return clk_freq;
	}

	hw = __clk_get_hw(vpu_conf.vpu_clk);
	clk_freq = clk_hw_get_rate(hw);
	return clk_freq;
}
EXPORT_SYMBOL(vpu_clk_get);

static int switch_gp_pll(int flag)
{
	unsigned int clk;
	int ret = 0;

	if (IS_ERR_OR_NULL(vpu_conf.gp_pll)) {
		VPUERR("%s: gp_pll\n", __func__);
		return -1;
	}

	if (flag) { /* enable */
		clk = vpu_conf.data->clk_table[8].freq;
		ret = clk_set_rate(vpu_conf.gp_pll, clk);
		if (ret)
			return ret;
		clk_prepare_enable(vpu_conf.gp_pll);
	} else { /* disable */
		clk_disable_unprepare(vpu_conf.gp_pll);
	}

	return 0;
}

int vpu_clk_apply_dft(unsigned int clk_level)
{
	unsigned int clk;
	int ret = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;

	if (vpu_conf.data->clk_table[clk_level].mux == FCLK_DIV_MAX) {
		VPUERR("clk_mux is invalid\n");
		return -1;
	}

	if (vpu_conf.data->clk_table[vpu_conf.clk_level].mux == GPLL_CLK) {
		if (vpu_conf.data->gp_pll_valid == 0) {
			VPUERR("gp_pll is invalid\n");
			return -1;
		}

		ret = switch_gp_pll(1);
		if (ret)
			return ret;
	}

	if ((IS_ERR_OR_NULL(vpu_conf.vpu_clk0)) ||
	    (IS_ERR_OR_NULL(vpu_conf.vpu_clk1)) ||
	    (IS_ERR_OR_NULL(vpu_conf.vpu_clk))) {
		VPUERR("%s: vpu_clk\n", __func__);
		return -1;
	}

	/* step 1:  switch to 2nd vpu clk patch */
	clk = vpu_conf.data->clk_table[vpu_conf.data->clk_level_dft].freq;
	ret = clk_set_rate(vpu_conf.vpu_clk1, clk);
	if (ret)
		return ret;
	ret = clk_set_parent(vpu_conf.vpu_clk, vpu_conf.vpu_clk1);
	if (ret)
		VPUERR("%s: %d clk_set_parent error\n", __func__, __LINE__);
	usleep_range(10, 15);
	/* step 2:  adjust 1st vpu clk frequency */
	clk = vpu_conf.data->clk_table[clk_level].freq;
	ret = clk_set_rate(vpu_conf.vpu_clk0, clk);
	if (ret)
		return ret;
	usleep_range(20, 25);
	/* step 3:  switch back to 1st vpu clk patch */
	ret = clk_set_parent(vpu_conf.vpu_clk, vpu_conf.vpu_clk0);
	if (ret)
		VPUERR("%s: %d clk_set_parent error\n", __func__, __LINE__);

	clk = clk_get_rate(vpu_conf.vpu_clk);
	VPUPR("set vpu clk: %uHz(%d), readback: %uHz(0x%x)\n",
	      vpu_conf.data->clk_table[clk_level].freq, clk_level,
	      clk, (vpu_clk_read(vpu_conf.data->vpu_clk_reg)));

	return ret;
}

int vpu_clk_apply_c3(unsigned int clk_level)
{
	unsigned int clk;
	int ret = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;

	if (vpu_conf.data->clk_table[clk_level].mux == FCLK_DIV_MAX) {
		VPUERR("clk_mux is invalid\n");
		return -1;
	}

	if (IS_ERR_OR_NULL(vpu_conf.vpu_clk)) {
		VPUERR("%s: vpu_clk\n", __func__);
		return -1;
	}

	clk = vpu_conf.data->clk_table[clk_level].freq;
	ret = clk_set_rate(vpu_conf.vpu_clk, clk);
	if (ret)
		return ret;

	clk = clk_get_rate(vpu_conf.vpu_clk);
	VPUPR("set vpu clk: %uHz(%d), readback: %uHz(0x%x)\n",
	      vpu_conf.data->clk_table[clk_level].freq, clk_level,
	      clk, (vpu_clk_read(vpu_conf.data->vpu_clk_reg)));

	return ret;
}

int set_vpu_clk(unsigned int vclk)
{
	int ret = -1;
	unsigned int clk_level, clk;

	if (vclk >= 100) /* regard as vpu_clk */
		clk_level = get_vpu_clk_level(vclk);
	else /* regard as clk_level */
		clk_level = vclk;

	if (clk_level >= vpu_conf.data->clk_level_max) {
		ret = 1;
		VPUERR("set vpu clk out of supported range\n");
		return ret;
	}
#ifdef LIMIT_VPU_CLK_LOW
	if (clk_level < vpu_conf.data->clk_level_dft) {
		ret = 3;
		VPUERR("set vpu clk less than system default\n");
		return ret;
	}
#endif
	if (clk_level >= 9) {
		ret = 7;
		VPUERR("clk_level %d is invalid\n", clk_level);
		return ret;
	}

	clk = vpu_clk_get();
	if ((clk > (vpu_conf.data->clk_table[clk_level].freq + VPU_CLK_TOLERANCE)) ||
	    (clk < (vpu_conf.data->clk_table[clk_level].freq - VPU_CLK_TOLERANCE))) {
		vpu_conf.clk_level = clk_level;
		if (vpu_conf.data->clk_apply)
			ret = vpu_conf.data->clk_apply(clk_level);
	}

	return ret;
}

void vpu_clktree_init_dft(struct device *dev)
{
	struct clk *clk_vapb, *clk_vpu_intr;
	int ret = 0;

	/* init & enable vapb_clk */
	vpu_conf.vapb_clk0 = devm_clk_get(dev, "vapb_clk0");
	vpu_conf.vapb_clk1 = devm_clk_get(dev, "vapb_clk1");
	vpu_conf.vapb_clk = devm_clk_get(dev, "vapb_clk");
	if ((IS_ERR_OR_NULL(vpu_conf.vapb_clk0)) ||
		(IS_ERR_OR_NULL(vpu_conf.vapb_clk1)) ||
		(IS_ERR_OR_NULL(vpu_conf.vapb_clk))) {
		clk_vapb = devm_clk_get(dev, "vapb_clk");
		if (IS_ERR_OR_NULL(clk_vapb))
			VPUERR("%s: vapb_clk\n", __func__);
		else
			clk_prepare_enable(clk_vapb);
	} else {
		ret = clk_set_parent(vpu_conf.vapb_clk, vpu_conf.vapb_clk0);
		if (ret)
			VPUERR("%s: %d clk_set_parent error\n", __func__, __LINE__);
		clk_prepare_enable(vpu_conf.vapb_clk);
		ret = clk_set_rate(vpu_conf.vapb_clk1, 50000000);
		if (ret)
			VPUERR("%s: clk_set_rate error\n", __func__);
	}


	clk_vpu_intr = devm_clk_get(dev, "vpu_intr_gate");
	if (IS_ERR_OR_NULL(clk_vpu_intr))
		VPUERR("%s: vpu_intr_gate\n", __func__);
	else
		clk_prepare_enable(clk_vpu_intr);

	if (vpu_conf.data->gp_pll_valid) {
		vpu_conf.gp_pll = devm_clk_get(dev, "gp_pll");
		if (IS_ERR_OR_NULL(vpu_conf.gp_pll))
			VPUERR("%s: gp_pll\n", __func__);
	}

	/* init & enable vpu_clk */
	vpu_conf.vpu_clk0 = devm_clk_get(dev, "vpu_clk0");
	vpu_conf.vpu_clk1 = devm_clk_get(dev, "vpu_clk1");
	vpu_conf.vpu_clk = devm_clk_get(dev, "vpu_clk");
	if ((IS_ERR_OR_NULL(vpu_conf.vpu_clk0)) ||
	    (IS_ERR_OR_NULL(vpu_conf.vpu_clk1)) ||
	    (IS_ERR_OR_NULL(vpu_conf.vpu_clk))) {
		VPUERR("%s: vpu_clk\n", __func__);
	} else {
		ret = clk_set_parent(vpu_conf.vpu_clk, vpu_conf.vpu_clk0);
		if (ret)
			VPUERR("%s: %d clk_set_parent error\n", __func__, __LINE__);
		clk_prepare_enable(vpu_conf.vpu_clk);
	}
}

void vpu_clktree_init_c3(struct device *dev)
{
	/* init & enable vpu_clk */
	vpu_conf.vpu_clk = devm_clk_get(dev, "vpu_clk");
	if (IS_ERR_OR_NULL(vpu_conf.vpu_clk))
		VPUERR("%s: vpu_clk\n", __func__);
	else
		clk_prepare_enable(vpu_conf.vpu_clk);
}
