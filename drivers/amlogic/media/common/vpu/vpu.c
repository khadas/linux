// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

 #define pr_fmt(fmt) "vpu: " fmt

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
#include "vpu_ctrl.h"
#include "vpu_module.h"

/* v20190305: initial version */
/* v20190605: add sm1, tm2 support */
#define VPU_VERION        "v20190605"

int vpu_debug_print_flag;
static spinlock_t vpu_mem_lock;
static spinlock_t vpu_clk_gate_lock;
static DEFINE_MUTEX(vpu_clk_mutex);
static DEFINE_MUTEX(vpu_dev_mutex);

#define VPU_DEV_MAX     255
static struct vpu_dev_s *vpu_dev_local[VPU_DEV_MAX];
static unsigned int vpu_dev_num;

struct vpu_conf_s vpu_conf = {
	.data = NULL,
	.clk_level = 0,

	/* clocktree */
	.gp_pll = NULL,
	.vpu_clk0 = NULL,
	.vpu_clk1 = NULL,
	.vpu_clk = NULL,

	.clk_vmod = NULL,
};

int vpu_chip_valid_check(void)
{
	int ret = 0;

	if (!vpu_conf.data) {
		VPUERR("invalid VPU in current chip\n");
		ret = -1;
	}
	return ret;
}

static unsigned int get_vpu_clk_level_max_vmod(void)
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
		if (video_clk <= vpu_clk_table[i].freq)
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

	mux = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 9, 3);
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
			clk_source = vpu_clk_table[8].freq;
			break;
		default:
			clk_source = 0;
			break;
		}

		div = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 0, 7) + 1;
		clk_freq = clk_source / div;

		return clk_freq;
	}

	hw = __clk_get_hw(vpu_conf.vpu_clk);
	clk_freq = clk_hw_get_rate(hw);
	return clk_freq;
}

static int switch_gp_pll(int flag)
{
	unsigned int clk;
	int ret = 0;

	if (IS_ERR_OR_NULL(vpu_conf.gp_pll)) {
		VPUERR("%s: gp_pll\n", __func__);
		return -1;
	}

	if (flag) { /* enable */
		clk = vpu_clk_table[8].freq;
		ret = clk_set_rate(vpu_conf.gp_pll, clk);
		if (ret)
			return ret;
		clk_prepare_enable(vpu_conf.gp_pll);
	} else { /* disable */
		clk_disable_unprepare(vpu_conf.gp_pll);
	}

	return 0;
}

static int adjust_vpu_clk(unsigned int clk_level)
{
	unsigned int clk;
	int ret = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;

	if (vpu_clk_table[vpu_conf.clk_level].mux == GPLL_CLK) {
		if (vpu_conf.data->gp_pll_valid == 0) {
			VPUERR("gp_pll is invalid\n");
			return -1;
		}

		ret = switch_gp_pll(1);
		if (ret) {
			VPUERR("%s: exit\n", __func__);
			return ret;
		}
	}

	if ((IS_ERR_OR_NULL(vpu_conf.vpu_clk0)) ||
	    (IS_ERR_OR_NULL(vpu_conf.vpu_clk1)) ||
	    (IS_ERR_OR_NULL(vpu_conf.vpu_clk))) {
		VPUERR("%s: vpu_clk\n", __func__);
		return -1;
	}

	vpu_conf.clk_level = clk_level;

	/* step 1:  switch to 2nd vpu clk patch */
	clk = vpu_clk_table[vpu_conf.data->clk_level_dft].freq;
	ret = clk_set_rate(vpu_conf.vpu_clk1, clk);
	if (ret)
		return ret;
	clk_set_parent(vpu_conf.vpu_clk, vpu_conf.vpu_clk1);
	usleep_range(10, 15);
	/* step 2:  adjust 1st vpu clk frequency */
	clk = vpu_clk_table[vpu_conf.clk_level].freq;
	ret = clk_set_rate(vpu_conf.vpu_clk0, clk);
	if (ret)
		return ret;
	usleep_range(20, 25);
	/* step 3:  switch back to 1st vpu clk patch */
	clk_set_parent(vpu_conf.vpu_clk, vpu_conf.vpu_clk0);

	clk = clk_get_rate(vpu_conf.vpu_clk);
	VPUPR("set vpu clk: %uHz(%d), readback: %uHz(0x%x)\n",
	      vpu_clk_table[vpu_conf.clk_level].freq, vpu_conf.clk_level,
	      clk, (vpu_hiu_read(HHI_VPU_CLK_CNTL)));

	return ret;
}

static int set_vpu_clk(unsigned int vclk)
{
	int ret = 0;
	unsigned int clk_level, clk;

	mutex_lock(&vpu_clk_mutex);

	if (vclk >= 100) { /* regard as vpu_clk */
		clk_level = get_vpu_clk_level(vclk);
	} else { /* regard as clk_level */
		clk_level = vclk;
	}

	if (clk_level >= vpu_conf.data->clk_level_max) {
		ret = 1;
		VPUERR("set vpu clk out of supported range\n");
		goto set_vpu_clk_limit;
	}
#ifdef LIMIT_VPU_CLK_LOW
	if (clk_level < vpu_conf.data->clk_level_dft) {
		ret = 3;
		VPUERR("set vpu clk less than system default\n");
		goto set_vpu_clk_limit;
	}
#endif
	if (clk_level >= sizeof(vpu_clk_table) / sizeof(struct vpu_clk_s)) {
		ret = 7;
		VPUERR("clk_level %d is invalid\n", clk_level);
		goto set_vpu_clk_limit;
	}

	clk = vpu_clk_get();
	if ((clk > (vpu_clk_table[clk_level].freq + VPU_CLK_TOLERANCE)) ||
	    (clk < (vpu_clk_table[clk_level].freq - VPU_CLK_TOLERANCE)))
		ret = adjust_vpu_clk(clk_level);

set_vpu_clk_limit:
	mutex_unlock(&vpu_clk_mutex);
	return ret;
}

static unsigned int vpu_vmod_clk_get(unsigned int vmod)
{
	unsigned int vpu_clk;
	int ret = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return 0;
	if (!vpu_conf.clk_vmod)
		return 0;

	mutex_lock(&vpu_clk_mutex);

	if (vmod < VPU_MOD_MAX) {
		vpu_clk = vpu_conf.clk_vmod[vmod];
		vpu_clk = vpu_clk_table[vpu_clk].freq;
	} else {
		vpu_clk = 0;
		VPUERR("%s: invalid vmod\n", __func__);
	}

	mutex_unlock(&vpu_clk_mutex);
	return vpu_clk;
}

int vpu_vmod_clk_request(unsigned int vclk, unsigned int vmod)
{
	int ret = 0;
#ifdef CONFIG_VPU_DYNAMIC_ADJ
	unsigned int clk_level;

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;
	if (vmod >= VPU_MOD_MAX) {
		VPUERR("%s: invalid vmod: %d\n", __func__, vmod);
		return -1;
	}
	if (!vpu_conf.clk_vmod) {
		VPUERR("%s: clk_vmod is null\n", __func__);
		return -1;
	}

	mutex_lock(&vpu_clk_mutex);

	if (vclk >= 100) /* regard as vpu_clk */
		clk_level = get_vpu_clk_level(vclk);
	else /* regard as clk_level */
		clk_level = vclk;

	if (clk_level >= vpu_conf.data->clk_level_max) {
		VPUERR("%s: set clk is out of supported\n", __func__);
		mutex_unlock(&vpu_clk_mutex);
		return -1;
	}

	vpu_conf.clk_vmod[vmod] = clk_level;
	if (vpu_debug_print_flag) {
		VPUPR("%s: %s %uHz\n",
		      __func__,
		      vpu_mod_table[vmod],
		      vpu_clk_table[clk_level].freq);
		dump_stack();
	}

	clk_level = get_vpu_clk_level_max_vmod();
	if (clk_level != vpu_conf.clk_level)
		adjust_vpu_clk(clk_level);

	mutex_unlock(&vpu_clk_mutex);
#endif
	return ret;
}

int vpu_vmod_clk_release(unsigned int vmod)
{
	int ret = 0;
#ifdef CONFIG_VPU_DYNAMIC_ADJ
	unsigned int clk_level;

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;
	if (vmod >= VPU_MOD_MAX) {
		VPUERR("%s: invalid vmod: %d\n", __func__, vmod);
		return -1;
	}
	if (!vpu_conf.clk_vmod) {
		VPUERR("%s: clk_vmod is null\n", __func__);
		return -1;
	}

	mutex_lock(&vpu_clk_mutex);

	clk_level = 0;
	vpu_conf.clk_vmod[vmod] = clk_level;
	if (vpu_debug_print_flag) {
		VPUPR("%s: %s\n", __func__, vpu_mod_table[vmod]);
		dump_stack();
	}

	clk_level = get_vpu_clk_level_max_vmod();
	if (clk_level != vpu_conf.clk_level)
		adjust_vpu_clk(clk_level);

	mutex_unlock(&vpu_clk_mutex);
#endif
	return ret;
}

static int vpu_vmod_mem_pd_switch(unsigned int vmod, int flag)
{
	unsigned int _val, _reg, _bit, _len;
	struct vpu_ctrl_s *table;
	int i = 0, ret = 0, done = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;
	if (vmod >= VPU_MOD_MAX)
		return -1;

	table = vpu_conf.data->mem_pd_table;
	while (i < VPU_MEM_PD_CNT_MAX) {
		if (table[i].vmod == VPU_MOD_MAX)
			break;
		if (table[i].vmod == vmod) {
			_reg = table[i].reg;
			_bit = table[i].bit;
			_len = table[i].len;
			if (flag == VPU_MEM_POWER_ON) {
				_val = 0x0;
			} else {
				if (_len == 32)
					_val = 0xffffffff;
				else
					_val = (1 << _len) - 1;
			}
			vpu_hiu_setb(_reg, _val, _bit, _len);
			done++;
		}
		i++;
	}

	if (done == 0)
		return -1;
	return 0;
}

static int vpu_vmod_mem_pd_get(unsigned int vmod)
{
	unsigned int _reg, _bit, _len;
	struct vpu_ctrl_s *table;
	int i = 0, ret = 0, done = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;
	if (vmod >= VPU_MOD_MAX) {
		VPUERR("%s: invalid vpu mod: %d\n", __func__, vmod);
		return -1;
	}

	table = vpu_conf.data->mem_pd_table;
	ret = 0;
	while (i < VPU_MEM_PD_CNT_MAX) {
		if (table[i].vmod == VPU_MOD_MAX)
			break;
		if (table[i].vmod == vmod) {
			_reg = table[i].reg;
			_bit = table[i].bit;
			_len = table[i].len;
			ret += vpu_hiu_getb(_reg, _bit, _len);
			done++;
		}
		i++;
	}
	if (done == 0) {
		VPUPR("%s: unsupport vpu mod: %d\n", __func__, vmod);
		return -1;
	}

	if (ret == 0)
		return VPU_MEM_POWER_ON;
	else
		return VPU_MEM_POWER_DOWN;
}

static int vpu_vmod_clk_gate_switch(unsigned int vmod, int flag)
{
	unsigned int _val, _reg, _bit, _len;
	struct vpu_ctrl_s *table;
	int i = 0, ret = 0, done = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return -1;
	if (vmod >= VPU_MAX)
		return -1;

	table = vpu_conf.data->clk_gate_table;
	while (i < VPU_CLK_GATE_CNT_MAX) {
		if (table[i].vmod == VPU_MAX)
			break;
		if (table[i].vmod == vmod) {
			_reg = table[i].reg;
			_bit = table[i].bit;
			_len = table[i].len;
			if (flag == VPU_CLK_GATE_ON) {
				if (_len == 32)
					_val = 0xffffffff;
				else
					_val = (1 << _len) - 1;
			} else {
				_val = 0;
			}
			vpu_vcbus_setb(_reg, _val, _bit, _len);
			done++;
		}
		i++;
	}

	if (done == 0)
		return -1;
	return 0;
}

/* *********************************************** */
/* VPU API */
/* *********************************************** */
/*
 *  Function: vpu_dev_get
 *      Get vpu_dev struct by vmod and driver name
 *
 *      Parameters:
 *      vmod - unsigned int, must be the following constants:
 *                 VPU_MOD, supported by vpu_mod_e
 *      owner_name - which driver register the vmod
 *
 *  Returns:
 *      struct vpu_dev_s *
 *
 *      Example:
 *      lcd_vpu_dev = vpu_dev_get(VPU_VENCL, "lcd");
 *
 */
struct vpu_dev_s *vpu_dev_get(unsigned int vmod, char *owner_name)
{
	struct vpu_dev_s *temp_dev = NULL;
	int i;

	if (!owner_name) {
		VPUERR("%s: owner_name is null\n", __func__);
		return NULL;
	}

	if (vmod >= VPU_MAX) {
		VPUERR("%s: invalid vmod %d\n", __func__, vmod);
		return NULL;
	}

	mutex_lock(&vpu_dev_mutex);
	for (i = 0; i < vpu_dev_num; i++) {
		if (vpu_dev_local[i]->dev_id == vmod &&
		    (strcmp(vpu_dev_local[i]->owner_name, owner_name) == 0)) {
			temp_dev = vpu_dev_local[i];
			break;
		}
	}
	mutex_unlock(&vpu_dev_mutex);

	return temp_dev;
}

/*
 *  Function: vpu_dev_register
 *      Register vpu_dev struct by vmod and driver name
 *
 *      Parameters:
 *      vmod - unsigned int, must be the following constants:
 *                 VPU_MOD, supported by vpu_mod_e
 *      owner_name - which driver register the vmod
 *
 *  Returns:
 *      struct vpu_dev_s *
 *
 *      Example:
 *      lcd_vpu_dev = vpu_dev_register(VPU_VENCL, "lcd");
 *
 */
struct vpu_dev_s *vpu_dev_register(unsigned int vmod, char *owner_name)
{
	struct vpu_dev_s *temp_dev;
	int ret;

	if (vpu_dev_num >= VPU_DEV_MAX) {
		VPUERR("%s: no space for vpu_dev\n", __func__);
		return NULL;
	}

	if (!owner_name) {
		VPUERR("%s: owner_name is null\n", __func__);
		return NULL;
	}

	if (vmod >= VPU_MAX) {
		VPUERR("%s: invalid vmod %d\n", __func__, vmod);
		return NULL;
	}

	temp_dev = vpu_dev_get(vmod, owner_name);
	if (temp_dev) {
		VPUPR("%s: vpu_dev %d in %s is already registered\n",
		      __func__, vmod, owner_name);
		return temp_dev;
	}

	mutex_lock(&vpu_dev_mutex);

	temp_dev = kzalloc(sizeof(*temp_dev), GFP_KERNEL);
	if (!temp_dev) {
		VPUERR("%s: failed\n", __func__);
		return NULL;
	}

	ret = vpu_vmod_mem_pd_get(vmod);
	if (ret == 0)
		temp_dev->mem_pd_state = 0;
	else
		temp_dev->mem_pd_state = 1;
	temp_dev->dev_id = vmod;
	strncpy(temp_dev->owner_name, owner_name, 30 - 1);
	temp_dev->index = vpu_dev_num;
	vpu_dev_local[vpu_dev_num] = temp_dev;
	if (vpu_debug_print_flag) {
		VPUPR("%s: dev_id=%d, owner_name=%s OK\n",
		      __func__, vmod, owner_name);
	}
	vpu_dev_num++;

	mutex_unlock(&vpu_dev_mutex);

	return temp_dev;
}

/*
 *  Function: vpu_dev_unregister
 *      Unregister vpu_dev struct
 *
 *      Parameters:
 *      vpu_dev - struct vpu_dev_s *, must be registered
 *
 *  Returns:
 *      int, 0 for success, -1 for failed
 *
 *      Example:
 *      ret = vpu_dev_unregister(lcd_vpu_dev);
 *
 */
int vpu_dev_unregister(struct vpu_dev_s *vpu_dev)
{
	int i, n;

	if (!vpu_dev) {
		VPUERR("%s: vpu_dev is null\n", __func__);
		return -1;
	}

	mutex_lock(&vpu_dev_mutex);

	n = vpu_dev->index;
	if (n >= vpu_dev_num) {
		VPUERR("%s: vpu_dev is null\n", __func__);
		return -1;
	}
	if (vpu_dev_num == 0) {
		VPUERR("%s: vpu_dev_num is zero\n", __func__);
		return -1;
	}
	if (vpu_debug_print_flag) {
		VPUPR("%s: dev_id=%d, owner_name=%s\n",
		      __func__, vpu_dev->dev_id, vpu_dev->owner_name);
	}
	kfree(vpu_dev_local[n]);
	vpu_dev_local[n] = NULL;

	for (i = n; i < vpu_dev_num; i++) {
		if (i == (vpu_dev_num - 1))
			vpu_dev_local[i] = NULL;
		else
			vpu_dev_local[i] = vpu_dev_local[i + 1];
	}
	vpu_dev_num--;

	mutex_unlock(&vpu_dev_mutex);

	return 0;
}

/*
 *  Function: vpu_dev_clk_get
 *      Get vpu clk holding frequency with vpu_dev
 *
 *      Parameters:
 *      vpu_dev - struct vpu_dev_s *, must be registered
 *
 *  Returns:
 *      unsigned int, vpu clk frequency unit in Hz
 *
 *      Example:
 *      video_clk = vpu_dev_clk_get(lcd_vpu_dev);
 *
 */
unsigned int vpu_dev_clk_get(struct vpu_dev_s *vpu_dev)
{
	unsigned int vclk;

	if (!vpu_dev) {
		VPUERR("%s: vpu_dev is null\n", __func__);
		return 0;
	}

	mutex_lock(&vpu_dev_mutex);
	vclk = vpu_vmod_clk_get(vpu_dev->dev_id);
	mutex_unlock(&vpu_dev_mutex);

	return vclk;
}

/*
 *  Function: vpu_dev_clk_request
 *      Request a new vpu clk holding frequency with vpu_dev,
 *      Will change vpu clk if the max level in all vmod vpu clk holdings
 *      is unequal to current vpu clk level
 *
 *  Parameters:
 *      vpu_dev - struct vpu_dev_s *, must be registered
 *      vclk - unsigned int, vpu clk frequency unit in Hz
 *
 *  Returns:
 *      int, 0 for success, -1 for failed
 *
 *  Example:
 *      ret = vpu_dev_clk_request(lcd_vpu_dev, 148500000);
 *
 */
int vpu_dev_clk_request(struct vpu_dev_s *vpu_dev, unsigned int vclk)
{
	int ret = 0;

	if (!vpu_dev) {
		VPUERR("%s: vpu_dev is null\n", __func__);
		return 0;
	}

	mutex_lock(&vpu_dev_mutex);
	ret = vpu_vmod_clk_request(vclk, vpu_dev->dev_id);
	mutex_unlock(&vpu_dev_mutex);

	return ret;
}

/*
 *  Function: vpu_dev_clk_release
 *      Release vpu clk holding frequency to 0 with vpu_dev
 *      Will change vpu clk if the max level in all vmod vpu clk holdings
 *      is unequal to current vpu clk level
 *
 *  Parameters:
 *      vpu_dev - struct vpu_dev_s *, must be registered
 *
 *  Returns:
 *      int, 0 for success, -1 for failed
 *
 *  Example:
 *      ret = vpu_dev_clk_release(VPU_VENCP);
 *
 */
int vpu_dev_clk_release(struct vpu_dev_s *vpu_dev)
{
	int ret = 0;

	if (!vpu_dev) {
		VPUERR("%s: vpu_dev is null\n", __func__);
		return 0;
	}

	mutex_lock(&vpu_dev_mutex);
	ret = vpu_vmod_clk_release(vpu_dev->dev_id);
	mutex_unlock(&vpu_dev_mutex);

	return ret;
}

/*
 *  Function: vpu_dev_mem_power_on
 *      switch vpu memory power down by vpu_dev
 *
 *  Parameters:
 *      vpu_dev - struct vpu_dev_s *, must be registered
 *
 *  Example:
 *      vpu_dev_mem_power_on(lcd_vpu_dev);
 *
 */
void vpu_dev_mem_power_on(struct vpu_dev_s *vpu_dev)
{
	unsigned long flags = 0;
	int ret;

	if (!vpu_dev) {
		VPUERR("%s: vpu_dev is null\n", __func__);
		return;
	}

	if (vpu_dev->dev_id >= VPU_MOD_MAX) {
		VPUERR("%s: invalid vpu mod: %d\n", __func__, vpu_dev->dev_id);
		return;
	}

	spin_lock_irqsave(&vpu_mem_lock, flags);
	ret = vpu_vmod_mem_pd_switch(vpu_dev->dev_id, VPU_MEM_POWER_ON);
	spin_unlock_irqrestore(&vpu_mem_lock, flags);
	if (ret)
		VPUPR("%s: unsupport vpu mod: %d\n", __func__, vpu_dev->dev_id);

	if (vpu_debug_print_flag) {
		VPUPR("%s: %s in %s\n",
		      __func__,
		      vpu_mod_table[vpu_dev->dev_id],
		      vpu_dev->owner_name);
		dump_stack();
	}
}

/*
 *  Function: vpu_dev_mem_power_down
 *      switch vpu memory power down by vpu_dev
 *
 *  Parameters:
 *      vpu_dev - struct vpu_dev_s *, must be registered
 *
 *  Example:
 *      vpu_dev_mem_power_down(lcd_vpu_dev);
 *
 */
void vpu_dev_mem_power_down(struct vpu_dev_s *vpu_dev)
{
	unsigned long flags = 0;
	int ret;

	if (!vpu_dev) {
		VPUERR("%s: vpu_dev is null\n", __func__);
		return;
	}

	if (vpu_dev->dev_id >= VPU_MOD_MAX) {
		VPUERR("%s: invalid vpu mod: %d\n", __func__, vpu_dev->dev_id);
		return;
	}

	spin_lock_irqsave(&vpu_mem_lock, flags);
	ret = vpu_vmod_mem_pd_switch(vpu_dev->dev_id, VPU_MEM_POWER_DOWN);
	spin_unlock_irqrestore(&vpu_mem_lock, flags);
	if (ret)
		VPUPR("%s: unsupport vpu mod: %d\n", __func__, vpu_dev->dev_id);

	if (vpu_debug_print_flag) {
		VPUPR("%s: %s in %s\n",
		      __func__,
		      vpu_mod_table[vpu_dev->dev_id],
		      vpu_dev->owner_name);
		dump_stack();
	}
}

/*
 *  Function: vpu_dev_mem_pd_get
 *      switch vpu memory power down by vpu_dev
 *
 *  Parameters:
 *      vpu_dev - struct vpu_dev_s *, must be registered
 *
 *  Returns:
 *      int, 0 for power on, 1 for power down, -1 for error
 *
 *  Example:
 *      ret = vpu_dev_mem_pd_get(lcd_vpu_dev);
 *
 */
int vpu_dev_mem_pd_get(struct vpu_dev_s *vpu_dev)
{
	int ret;

	if (!vpu_dev) {
		VPUERR("%s: vpu_dev is null\n", __func__);
		return 0;
	}

	mutex_lock(&vpu_dev_mutex);
	ret = vpu_vmod_mem_pd_get(vpu_dev->dev_id);
	mutex_unlock(&vpu_dev_mutex);

	return ret;
}

/*
 *  Function: vpu_dev_clk_gate_on
 *      switch vpu clk gate by vpu_dev
 *
 *  Parameters:
 *      vpu_dev - struct vpu_dev_s *, must be registered
 *
 *  Example:
 *      vpu_dev_clk_gate_on(lcd_vpu_dev);
 *
 */
void vpu_dev_clk_gate_on(struct vpu_dev_s *vpu_dev)
{
	unsigned long flags = 0;
	int ret;

	if (!vpu_dev) {
		VPUERR("%s: vpu_dev is null\n", __func__);
		return;
	}

	if (vpu_dev->dev_id >= VPU_MAX) {
		VPUERR("%s: invalid vpu mod: %d\n", __func__, vpu_dev->dev_id);
		return;
	}

	spin_lock_irqsave(&vpu_clk_gate_lock, flags);
	ret = vpu_vmod_clk_gate_switch(vpu_dev->dev_id, VPU_CLK_GATE_ON);
	spin_unlock_irqrestore(&vpu_clk_gate_lock, flags);
	if (ret)
		VPUPR("%s: unsupport vpu mod: %d\n", __func__, vpu_dev->dev_id);

	if (vpu_debug_print_flag) {
		VPUPR("%s: %s in %s\n",
		      __func__,
		      vpu_mod_table[vpu_dev->dev_id],
		      vpu_dev->owner_name);
		dump_stack();
	}
}

/*
 *  Function: vpu_dev_clk_gate_off
 *      switch vpu clk gate by vpu_dev
 *
 *  Parameters:
 *      vpu_dev - struct vpu_dev_s *, must be registered
 *
 *  Example:
 *      vpu_dev_clk_gate_off(lcd_vpu_dev);
 *
 */
void vpu_dev_clk_gate_off(struct vpu_dev_s *vpu_dev)
{
	unsigned long flags = 0;
	int ret;

	if (!vpu_dev) {
		VPUERR("%s: vpu_dev is null\n", __func__);
		return;
	}

	if (vpu_dev->dev_id >= VPU_MAX) {
		VPUERR("%s: invalid vpu mod: %d\n", __func__, vpu_dev->dev_id);
		return;
	}

	spin_lock_irqsave(&vpu_clk_gate_lock, flags);
	ret = vpu_vmod_clk_gate_switch(vpu_dev->dev_id, VPU_CLK_GATE_OFF);
	spin_unlock_irqrestore(&vpu_clk_gate_lock, flags);
	if (ret)
		VPUPR("%s: unsupport vpu mod: %d\n", __func__, vpu_dev->dev_id);

	if (vpu_debug_print_flag) {
		VPUPR("%s: %s in %s\n",
		      __func__,
		      vpu_mod_table[vpu_dev->dev_id],
		      vpu_dev->owner_name);
		dump_stack();
	}
}

/* *********************************************** */

/* *********************************************** */
/* VPU sysfs function */
/* *********************************************** */
static const char *vpu_usage_str = {
"Usage:\n"
"	echo w <reg> <data> > /sys/class/vpu/reg ; write data to vcbus reg\n"
"	echo r <reg> > /sys/class/vpu/reg ; read vcbus reg\n"
"	echo d <reg> <num> > /sys/class/vpu/reg ; dump vcbus regs\n"
"\n"
"	echo r > /sys/class/vpu/mem ; read vpu memory power down status\n"
"	echo w <vmod> <flag> > /sys/class/vpu/mem ; write vpu memory power down\n"
"	<flag>: 0=power up, 1=power down\n"
"\n"
"	echo r > /sys/class/vpu/gate ; read vpu clk gate status\n"
"	echo w <vmod> <flag> > /sys/class/vpu/gate ; write vpu clk gate\n"
"	<flag>: 0=gate off, 1=gate on\n"
"\n"
"	echo dump > /sys/class/vpu/dev ; dump vpu_dev information\n"
"\n"
"	echo 1 > /sys/class/vpu/test ; run vcbus access test\n"
"\n"
"	echo get > /sys/class/vpu/clk ; print current vpu clk\n"
"	echo set <vclk> > /sys/class/vpu/clk ; force to set vpu clk\n"
"	echo dump [vmod] > /sys/class/vpu/clk ; dump vpu clk by vmod, [vmod] is unnecessary\n"
"	echo request <vclk> <vmod> > /sys/class/vpu/clk ; request vpu clk holding by vmod\n"
"	echo release <vmod> > /sys/class/vpu/clk ; release vpu clk holding by vmod\n"
"\n"
"	request & release will change vpu clk if the max level in all vmod vpu clk holdings is unequal to current vpu clk level.\n"
"	vclk both support level(1~10) and frequency value (unit in Hz).\n"
"	vclk level & frequency:\n"
"		0: 100M    1: 167M    2: 200M    3: 250M\n"
"		4: 333M    5: 400M    6: 500M    7: 667M\n"
"\n"
"	echo <0|1> > /sys/class/vpu/print ; set debug print flag\n"
};

static ssize_t vpu_debug_help(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", vpu_usage_str);
}

static ssize_t vpu_clk_debug(struct class *class, struct class_attribute *attr,
			     const char *buf, size_t count)
{
	unsigned int ret = 0;
	unsigned int tmp[2], n;

	switch (buf[0]) {
	case 'g': /* get */
		VPUPR("get current clk: %uHz(0x%08x)\n",
		      vpu_clk_get(), (vpu_hiu_read(HHI_VPU_CLK_CNTL)));
		break;
	case 's': /* set */
		tmp[0] = 4;
		ret = sscanf(buf, "set %u", &tmp[0]);
		if (ret == 1) {
			if (tmp[0] > 100)
				VPUPR("set clk frequency: %uHz\n", tmp[0]);
			else
				VPUPR("set clk level: %u\n", tmp[0]);
			set_vpu_clk(tmp[0]);
		} else {
			VPUERR("invalid parameters\n");
		}
		break;
	case 'r':
		if (buf[2] == 'q') { /* request */
			tmp[0] = 0;
			tmp[1] = VPU_MOD_MAX;
			ret = sscanf(buf, "request %u %u", &tmp[0], &tmp[1]);
			if (ret == 2)
				vpu_vmod_clk_request(tmp[0], tmp[1]);
			else
				VPUERR("invalid parameters\n");
		} else if (buf[2] == 'l') { /* release */
			tmp[0] = VPU_MOD_MAX;
			ret = sscanf(buf, "release %u", &tmp[0]);
			if (ret == 1)
				vpu_vmod_clk_release(tmp[0]);
			else
				VPUERR("invalid parameters\n");
		}
		break;
	case 'd':
		if (!vpu_conf.clk_vmod) {
			VPUERR("clk_vmod is null\n");
			return count;
		}
		tmp[0] = VPU_MOD_MAX;
		ret = sscanf(buf, "dump %u", &tmp[0]);
		if (ret == 1) {
			tmp[0] = (tmp[0] >= VPU_MOD_MAX) ? VPU_MOD_MAX : tmp[0];
			VPUPR("clk holdings:\n");
			pr_info("%s:  %uHz(%u)\n", vpu_mod_table[tmp[0]],
				vpu_clk_table[vpu_conf.clk_vmod[tmp[0]]].freq,
				vpu_conf.clk_vmod[tmp[0]]);
		} else {
			n = get_vpu_clk_level_max_vmod();
			VPUPR("clk max holdings: %uHz(%u)\n",
			      vpu_clk_table[n].freq, n);
		}
		break;
	default:
		VPUERR("invalid debug command\n");
		break;
	}

	return count;
}

static ssize_t vpu_mem_debug(struct class *class, struct class_attribute *attr,
			     const char *buf, size_t count)
{
	unsigned long flags = 0;
	unsigned int tmp[2], val;
	unsigned int _reg0, _reg1, _reg2, _reg3, _reg4;
	int ret = 0, i;

	_reg0 = vpu_conf.data->mem_pd_reg0;
	_reg1 = vpu_conf.data->mem_pd_reg1;
	_reg2 = vpu_conf.data->mem_pd_reg2;
	_reg3 = vpu_conf.data->mem_pd_reg3;
	_reg4 = vpu_conf.data->mem_pd_reg4;
	switch (buf[0]) {
	case 'r':
		if (_reg0 < VPU_REG_END)
			VPUPR("mem_pd0: 0x%08x\n", vpu_hiu_read(_reg0));
		if (_reg1 < VPU_REG_END)
			VPUPR("mem_pd1: 0x%08x\n", vpu_hiu_read(_reg1));
		if (_reg2 < VPU_REG_END)
			VPUPR("mem_pd2: 0x%08x\n", vpu_hiu_read(_reg2));
		if (_reg3 < VPU_REG_END)
			VPUPR("mem_pd3: 0x%08x\n", vpu_hiu_read(_reg3));
		if (_reg4 < VPU_REG_END)
			VPUPR("mem_pd4: 0x%08x\n", vpu_hiu_read(_reg4));
		break;
	case 'w':
		ret = sscanf(buf, "w %u %u", &tmp[0], &tmp[1]);
		if (ret == 2) {
			tmp[0] = (tmp[0] >= VPU_MOD_MAX) ? VPU_MOD_MAX : tmp[0];
			tmp[1] = (tmp[1] == VPU_MEM_POWER_ON) ? 0 : 1;
			VPUPR("switch_vpu_mem_pd: %s %s\n",
			      vpu_mod_table[tmp[0]],
			      (tmp[1] ? "DOWN" : "ON"));
			if (tmp[0] >= VPU_MOD_MAX) {
				VPUERR("invalid vpu mod: %d\n", tmp[0]);
				return count;
			}
			spin_lock_irqsave(&vpu_mem_lock, flags);
			ret = vpu_vmod_mem_pd_switch(tmp[0], tmp[1]);
			spin_unlock_irqrestore(&vpu_mem_lock, flags);
			if (ret)
				VPUPR("unsupport vpu mod: %d\n", tmp[0]);
		} else {
			VPUERR("invalid parameters\n");
		}
		break;
	case 'i':
		VPUPR("vpu modules:\n");
		for (i = 0; i < VPU_MOD_MAX; i++) {
			val = vpu_vmod_mem_pd_get(i);
			pr_info("  [%02d] %s    %s(%d)\n",
				i, vpu_mod_table[i],
				(val == VPU_MEM_POWER_ON) ? "ON" : "OFF",
				val);
		}
		break;
	default:
		VPUERR("invalid mem_pd command\n");
		break;
	}

	return count;
}

static ssize_t vpu_clk_gate_debug(struct class *class,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long flags = 0;
	unsigned int tmp[2];
	unsigned int _reg = 0;
	struct vpu_ctrl_s *table;
	int i = 0, ret = 0;

	switch (buf[0]) {
	case 'r':
		table = vpu_conf.data->clk_gate_table;
		while (1) {
			if (table[i].vmod >= VPU_MAX)
				break;
			if (table[i].reg == _reg) {
				i++;
				continue;
			}
			_reg = table[i].reg;
			pr_info("VPU_CLK_GATE: 0x%04x = 0x%08x\n",
				_reg, vpu_vcbus_read(_reg));
			i++;
		}
		break;
	case 'w':
		ret = sscanf(buf, "w %u %u", &tmp[0], &tmp[1]);
		if (ret == 2) {
			tmp[0] = (tmp[0] >= VPU_MAX) ? VPU_MAX : tmp[0];
			tmp[1] = (tmp[1] == VPU_CLK_GATE_ON) ? 1 : 0;
			VPUPR("switch_vpu_clk_gate: %s %s\n",
			      vpu_mod_table[tmp[0]], (tmp[1] ? "ON" : "OFF"));
			if (tmp[0] >= VPU_MAX) {
				VPUERR("invalid vpu mod: %d\n", tmp[0]);
				return count;
			}
			spin_lock_irqsave(&vpu_clk_gate_lock, flags);
			ret = vpu_vmod_clk_gate_switch(tmp[0], tmp[1]);
			spin_unlock_irqrestore(&vpu_clk_gate_lock, flags);
			if (ret)
				VPUPR("unsupport vpu mod: %d\n", tmp[0]);
		} else {
			VPUERR("invalid parameters\n");
		}
		break;
	default:
		VPUERR("invalid clk_gate command\n");
		break;
	}

	return count;
}

static ssize_t vpu_dev_debug(struct class *class, struct class_attribute *attr,
			     const char *buf, size_t count)
{
	int i;

	switch (buf[0]) {
	case 'd':
		if (vpu_dev_num == 0) {
			VPUERR("no vpu_dev\n");
			return count;
		}
		VPUPR("vpu_dev information:\n");
		for (i = 0; i < vpu_dev_num; i++) {
			if (!vpu_dev_local[i])
				continue;
			pr_info("index %d:\n"
				"    owner_name        %s\n"
				"    dev_id            %d\n"
				"    mem_pd_state      %d\n"
				"    clk_gate_state    %d\n",
				vpu_dev_local[i]->index,
				vpu_dev_local[i]->owner_name,
				vpu_dev_local[i]->dev_id,
				vpu_dev_local[i]->mem_pd_state,
				vpu_dev_local[i]->clk_gate_state);
		}
		break;
	default:
		VPUERR("invalid command\n");
		break;
	}

	return count;
}

static unsigned int vcbus_reg[] = {
	0x1b7f, /* VENC_VDAC_TST_VAL */
	0x1c30, /* ENCP_DVI_HSO_BEGIN */
	0x1d00, /* VPP_DUMMY_DATA */
	0x2730, /* VPU_VPU_PWM_V0 */
};

static void vcbus_test(void)
{
	unsigned int val;
	unsigned int temp;
	int i, j;

	VPUPR("vcbus test:\n");
	for (i = 0; i < ARRAY_SIZE(vcbus_reg); i++) {
		for (j = 0; j < 24; j++) {
			val = vpu_vcbus_read(vcbus_reg[i]);
			pr_info("%02d read 0x%04x=0x%08x\n",
				j, vcbus_reg[i], val);
		}
		pr_info("\n");
	}
	temp = 0x5a5a5a5a;
	for (i = 0; i < ARRAY_SIZE(vcbus_reg); i++) {
		vpu_vcbus_write(vcbus_reg[i], temp);
		val = vpu_vcbus_read(vcbus_reg[i]);
		pr_info("write 0x%04x=0x%08x, readback: 0x%08x\n",
			vcbus_reg[i], temp, val);
	}
	for (i = 0; i < ARRAY_SIZE(vcbus_reg); i++) {
		for (j = 0; j < 24; j++) {
			val = vpu_vcbus_read(vcbus_reg[i]);
			pr_info("%02d read 0x%04x=0x%08x\n",
				j, vcbus_reg[i], val);
		}
		pr_info("\n");
	}
}

static ssize_t vpu_test_debug(struct class *class, struct class_attribute *attr,
			      const char *buf, size_t count)
{
	vcbus_test();

	return count;
}

static ssize_t vpu_debug_print_show(struct class *class,
				    struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "vpu debug print: %d\n",
		vpu_debug_print_flag);
}

static ssize_t vpu_debug_print_store(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned int ret;

	ret = kstrtoint(buf, 10, &vpu_debug_print_flag);
	pr_info("set vpu debug_print_flag: %d\n", vpu_debug_print_flag);

	return count;
}

static ssize_t vpu_debug_info(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	unsigned int level_max, clk;
	ssize_t len = 0;
	int ret;
#ifdef CONFIG_VPU_DYNAMIC_ADJ
	int i;
#endif

	ret = vpu_chip_valid_check();
	if (ret) {
		len = sprintf(buf, "vpu invalid\n");
		return len;
	}

	clk = vpu_clk_get();
	level_max = vpu_conf.data->clk_level_max - 1;

	len = sprintf(buf, "driver version: %s(%d-%s)\n",
		      VPU_VERION,
		      vpu_conf.data->chip_type,
		      vpu_conf.data->chip_name);
	len += sprintf(buf + len, "actual clk:     %dHz\n"
		       "clk_level:      %d(%dHz)\n"
		       "clk_level dft:  %d(%dHz)\n"
		       "clk_level max:  %d(%dHz)\n\n",
		       clk,
		       vpu_conf.clk_level,
		       vpu_clk_table[vpu_conf.clk_level].freq,
		       vpu_conf.data->clk_level_dft,
		       vpu_clk_table[vpu_conf.data->clk_level_dft].freq,
		       level_max, vpu_clk_table[level_max].freq);

	len += sprintf(buf + len, "mem_pd:\n");
	if (vpu_conf.data->mem_pd_reg0 < VPU_REG_END) {
		len += sprintf(buf + len, "  mem_pd0:      0x%08x\n",
			       vpu_hiu_read(vpu_conf.data->mem_pd_reg0));
	}
	if (vpu_conf.data->mem_pd_reg1 < VPU_REG_END) {
		len += sprintf(buf + len, "  mem_pd1:      0x%08x\n",
			       vpu_hiu_read(vpu_conf.data->mem_pd_reg1));
	}
	if (vpu_conf.data->mem_pd_reg2 < VPU_REG_END) {
		len += sprintf(buf + len, "  mem_pd2:      0x%08x\n",
			       vpu_hiu_read(vpu_conf.data->mem_pd_reg2));
	}
	if (vpu_conf.data->mem_pd_reg3 < VPU_REG_END) {
		len += sprintf(buf + len, "  mem_pd3:      0x%08x\n",
			       vpu_hiu_read(vpu_conf.data->mem_pd_reg3));
	}
	if (vpu_conf.data->mem_pd_reg4 < VPU_REG_END) {
		len += sprintf(buf + len, "  mem_pd4:      0x%08x\n",
			       vpu_hiu_read(vpu_conf.data->mem_pd_reg4));
	}

#ifdef CONFIG_VPU_DYNAMIC_ADJ
	if (vpu_conf.clk_vmod) {
		len += sprintf(buf + len, "\nclk_vmod:\n");
		for (i = 0; i < VPU_MOD_MAX; i++) {
			len += sprintf(buf + len, "  %02d:    %dHz\n",
				       i, vpu_conf.clk_vmod[i]);
		}
	}
#endif
	len += sprintf(buf + len, "\n");

	return len;
}

static ssize_t vpu_debug_reg_store(struct class *class,
				   struct class_attribute *attr,
				   const char *buf, size_t count)
{
	int ret = 0;
	unsigned int reg32 = 0, data32 = 0;
	int i;

	switch (buf[0]) {
	case 'w':
		ret = sscanf(buf, "w %x %x", &reg32, &data32);
		if (ret == 2) {
			vpu_vcbus_write(reg32, data32);
			pr_info("write vcbus[0x%04x]=0x%08x, readback 0x%08x\n",
				reg32, data32, vpu_vcbus_read(reg32));
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'r':
		ret = sscanf(buf, "r %x", &reg32);
		if (ret == 1) {
			pr_info("read vcbus[0x%04x] = 0x%08x\n",
				reg32, vpu_vcbus_read(reg32));
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	case 'd':
		ret = sscanf(buf, "d %x %d", &reg32, &data32);
		if (ret == 2) {
			pr_info("dump vcbus regs:\n");
			for (i = 0; i < data32; i++) {
				pr_info("[0x%04x] = 0x%08x\n",
					(reg32 + i), vpu_vcbus_read(reg32 + i));
			}
		} else {
			pr_info("invalid data\n");
			return -EINVAL;
		}
		break;
	default:
		pr_info("wrong command\n");
		break;
	}

	return count;
}

static struct class_attribute vpu_debug_class_attrs[] = {
	__ATTR(clk,   0644, NULL, vpu_clk_debug),
	__ATTR(mem,   0644, NULL, vpu_mem_debug),
	__ATTR(gate,  0644, NULL, vpu_clk_gate_debug),
	__ATTR(dev,   0644, NULL, vpu_dev_debug),
	__ATTR(test,  0644, NULL, vpu_test_debug),
	__ATTR(print, 0644, vpu_debug_print_show, vpu_debug_print_store),
	__ATTR(reg,   0644, NULL, vpu_debug_reg_store),
	__ATTR(info,  0444, vpu_debug_info, NULL),
	__ATTR(help,  0444, vpu_debug_help, NULL),
};

static struct class *vpu_debug_class;
static int creat_vpu_debug_class(void)
{
	int i;

	vpu_debug_class = class_create(THIS_MODULE, "vpu");
	if (IS_ERR_OR_NULL(vpu_debug_class)) {
		VPUERR("create vpu_debug_class failed\n");
		return -1;
	}
	for (i = 0; i < ARRAY_SIZE(vpu_debug_class_attrs); i++) {
		if (class_create_file(vpu_debug_class,
				      &vpu_debug_class_attrs[i])) {
			VPUERR("create vpu debug attribute %s failed\n",
			       vpu_debug_class_attrs[i].attr.name);
		}
	}
	return 0;
}

static int remove_vpu_debug_class(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vpu_debug_class_attrs); i++)
		class_remove_file(vpu_debug_class, &vpu_debug_class_attrs[i]);

	class_destroy(vpu_debug_class);
	vpu_debug_class = NULL;
	return 0;
}

/* ********************************************************* */

static int get_vpu_config(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int val;
	struct device_node *vpu_np;

	vpu_np = pdev->dev.of_node;
	if (!vpu_np) {
		VPUERR("don't find vpu node\n");
		return -1;
	}

	ret = of_property_read_u32(vpu_np, "clk_level", &val);
	if (ret) {
		VPUPR("don't find clk_level, use default setting\n");
	} else {
		if (val >= vpu_conf.data->clk_level_max) {
			VPUERR("clk_level is out of support, set to default\n");
			val = vpu_conf.data->clk_level_dft;
		}

		vpu_conf.clk_level = val;
	}
	VPUPR("load vpu_clk: %uHz(%u)\n",
	      vpu_clk_table[val].freq, vpu_conf.clk_level);

	ret = of_property_read_u32(vpu_np, "debug_print", &val);
	if (ret) {
		vpu_debug_print_flag = 0;
	} else {
		vpu_debug_print_flag = val;
		if (vpu_debug_print_flag)
			VPUPR("find debug_print: %d\n", vpu_debug_print_flag);
	}

	return ret;
}

static void vpu_clktree_init(struct device *dev)
{
	struct clk *clk_vapb, *clk_vpu_intr;

	/* init & enable vapb_clk */
	clk_vapb = devm_clk_get(dev, "vapb_clk");
	if (IS_ERR_OR_NULL(clk_vapb))
		VPUERR("%s: vapb_clk\n", __func__);
	else
		clk_prepare_enable(clk_vapb);

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
		clk_set_parent(vpu_conf.vpu_clk, vpu_conf.vpu_clk0);
		clk_prepare_enable(vpu_conf.vpu_clk);
	}

	if (vpu_debug_print_flag)
		VPUPR("clktree_init\n");
}

static int vpu_power_init_check(void)
{
	unsigned int val;
	int ret = 0;

	val = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 31, 1);
	if (val)
		val = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 24, 1);
	else
		val = vpu_hiu_getb(HHI_VPU_CLK_CNTL, 8, 1);
	ret = (val == 0) ? 1 : 0;

	return ret;
}

static void vpu_power_init(void)
{
	int ret = 0;

	ret = vpu_chip_valid_check();
	if (ret)
		return;

	vpu_power_on();
	vpu_mem_pd_init_off();
	vpu_module_init_config();
}

static struct vpu_data_s vpu_data_g12a = {
	.chip_type = VPU_CHIP_G12A,
	.chip_name = "g12a",
	.clk_level_dft = CLK_LEVEL_DFT_G12A,
	.clk_level_max = CLK_LEVEL_MAX_G12A,
	.fclk_div_table = fclk_div_table_g12a,

	.gp_pll_valid = 0,
	.mem_pd_reg0 = HHI_VPU_MEM_PD_REG0,
	.mem_pd_reg1 = HHI_VPU_MEM_PD_REG1,
	.mem_pd_reg2 = HHI_VPU_MEM_PD_REG2,
	.mem_pd_reg3 = VPU_REG_END,
	.mem_pd_reg4 = VPU_REG_END,

	.reset_table = vpu_reset_g12a,
	.hdmi_iso_pre_table = vpu_hdmi_iso_pre_g12a,
	.hdmi_iso_table = vpu_hdmi_iso_g12a,
	.module_init_table = NULL,

	.mem_pd_table = vpu_mem_pd_g12a,
	.clk_gate_table = vpu_clk_gate_g12a,
};

static struct vpu_data_s vpu_data_g12b = {
	.chip_type = VPU_CHIP_G12B,
	.chip_name = "g12b",
	.clk_level_dft = CLK_LEVEL_DFT_G12A,
	.clk_level_max = CLK_LEVEL_MAX_G12A,
	.fclk_div_table = fclk_div_table_g12a,

	.gp_pll_valid = 0,
	.mem_pd_reg0 = HHI_VPU_MEM_PD_REG0,
	.mem_pd_reg1 = HHI_VPU_MEM_PD_REG1,
	.mem_pd_reg2 = HHI_VPU_MEM_PD_REG2,
	.mem_pd_reg3 = VPU_REG_END,
	.mem_pd_reg4 = VPU_REG_END,

	.reset_table = vpu_reset_g12a,
	.hdmi_iso_pre_table = vpu_hdmi_iso_pre_g12a,
	.hdmi_iso_table = vpu_hdmi_iso_g12a,
	.module_init_table = NULL,

	.mem_pd_table = vpu_mem_pd_g12b,
	.clk_gate_table = vpu_clk_gate_g12a,
};

static struct vpu_data_s vpu_data_tl1 = {
	.chip_type = VPU_CHIP_TL1,
	.chip_name = "tl1",
	.clk_level_dft = CLK_LEVEL_DFT_G12A,
	.clk_level_max = CLK_LEVEL_MAX_G12A,
	.fclk_div_table = fclk_div_table_g12a,

	.gp_pll_valid = 0,
	.mem_pd_reg0 = HHI_VPU_MEM_PD_REG0,
	.mem_pd_reg1 = HHI_VPU_MEM_PD_REG1,
	.mem_pd_reg2 = HHI_VPU_MEM_PD_REG2,
	.mem_pd_reg3 = HHI_VPU_MEM_PD_REG3,
	.mem_pd_reg4 = HHI_VPU_MEM_PD_REG4,

	.reset_table = vpu_reset_tl1,
	.hdmi_iso_pre_table = vpu_hdmi_iso_pre_g12a,
	.hdmi_iso_table = vpu_hdmi_iso_g12a,
	.module_init_table = NULL,

	.mem_pd_table = vpu_mem_pd_tl1,
	.clk_gate_table = vpu_clk_gate_g12a,
};

static struct vpu_data_s vpu_data_sm1 = {
	.chip_type = VPU_CHIP_SM1,
	.chip_name = "sm1",
	.clk_level_dft = CLK_LEVEL_DFT_G12A,
	.clk_level_max = CLK_LEVEL_MAX_G12A,
	.fclk_div_table = fclk_div_table_g12a,

	.gp_pll_valid = 0,
	.mem_pd_reg0 = HHI_VPU_MEM_PD_REG0,
	.mem_pd_reg1 = HHI_VPU_MEM_PD_REG1,
	.mem_pd_reg2 = HHI_VPU_MEM_PD_REG2,
	.mem_pd_reg3 = HHI_VPU_MEM_PD_REG3_SM1,
	.mem_pd_reg4 = HHI_VPU_MEM_PD_REG4_SM1,

	.reset_table = vpu_reset_g12a,
	.hdmi_iso_pre_table = vpu_hdmi_iso_pre_g12a,
	.hdmi_iso_table = vpu_hdmi_iso_sm1,
	.module_init_table = NULL,

	.mem_pd_table = vpu_mem_pd_sm1,
	.clk_gate_table = vpu_clk_gate_g12a,
};

static struct vpu_data_s vpu_data_tm2 = {
	.chip_type = VPU_CHIP_TM2,
	.chip_name = "tm2",
	.clk_level_dft = CLK_LEVEL_DFT_G12A,
	.clk_level_max = CLK_LEVEL_MAX_G12A,
	.fclk_div_table = fclk_div_table_g12a,

	.gp_pll_valid = 0,
	.mem_pd_reg0 = HHI_VPU_MEM_PD_REG0,
	.mem_pd_reg1 = HHI_VPU_MEM_PD_REG1,
	.mem_pd_reg2 = HHI_VPU_MEM_PD_REG2,
	.mem_pd_reg3 = HHI_VPU_MEM_PD_REG3,
	.mem_pd_reg4 = HHI_VPU_MEM_PD_REG4,

	.reset_table = vpu_reset_tl1,
	.hdmi_iso_pre_table = vpu_hdmi_iso_pre_g12a,
	.hdmi_iso_table = vpu_hdmi_iso_sm1,
	.module_init_table = NULL,

	.mem_pd_table = vpu_mem_pd_tm2,
	.clk_gate_table = vpu_clk_gate_g12a,
};

static const struct of_device_id vpu_of_table[] = {
	{
		.compatible = "amlogic, vpu-g12a",
		.data = &vpu_data_g12a,
	},
	{
		.compatible = "amlogic, vpu-g12b",
		.data = &vpu_data_g12b,
	},
	{
		.compatible = "amlogic, vpu-tl1",
		.data = &vpu_data_tl1,
	},
	{
		.compatible = "amlogic, vpu-sm1",
		.data = &vpu_data_sm1,
	},
	{
		.compatible = "amlogic, vpu-tm2",
		.data = &vpu_data_tm2,
	},
	{},
};

static int vpu_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	int ret = 0;

	vpu_debug_print_flag = 0;

	match = of_match_device(vpu_of_table, &pdev->dev);
	if (!match) {
		VPUERR("%s: no match table\n", __func__);
		return -EINVAL;
	}
	vpu_conf.data = (struct vpu_data_s *)match->data;
	ret = vpu_chip_valid_check();
	if (ret)
		return -EINVAL;

	vpu_dev_num = 0;
	VPUPR("driver version: %s(%d-%s)\n",
	      VPU_VERION,
	      vpu_conf.data->chip_type,
	      vpu_conf.data->chip_name);

	vpu_conf.clk_vmod = devm_kcalloc(&pdev->dev, VPU_MOD_MAX,
					 sizeof(unsigned int),
			GFP_KERNEL);
	if (!vpu_conf.clk_vmod)
		VPUERR("%s: Not enough memory\n", __func__);

	spin_lock_init(&vpu_mem_lock);
	spin_lock_init(&vpu_clk_gate_lock);

	get_vpu_config(pdev);
	ret = vpu_ioremap(pdev);
	if (ret < 0)
		return -ENOMEM;

	ret = vpu_power_init_check();
	vpu_clktree_init(&pdev->dev);
	set_vpu_clk(vpu_conf.clk_level);
	if (ret)
		vpu_power_init();
	creat_vpu_debug_class();

	VPUPR("%s OK\n", __func__);
	return 0;
}

static int vpu_remove(struct platform_device *pdev)
{
	remove_vpu_debug_class();
	kfree(vpu_conf.clk_vmod);
	vpu_conf.clk_vmod = NULL;

	return 0;
}

#ifdef CONFIG_PM
static int vpu_suspend(struct platform_device *pdev, pm_message_t state)
{
	VPUPR("suspend clk: %uHz(0x%x)\n",
	      vpu_clk_get(), (vpu_hiu_read(HHI_VPU_CLK_CNTL)));
	return 0;
}

static int vpu_resume(struct platform_device *pdev)
{
	set_vpu_clk(vpu_conf.clk_level);
	VPUPR("resume clk: %uHz(0x%x)\n",
	      vpu_clk_get(), (vpu_hiu_read(HHI_VPU_CLK_CNTL)));
	return 0;
}
#endif

static struct platform_driver vpu_driver = {
	.driver = {
		.name = "vpu",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(vpu_of_table),
	},
	.probe = vpu_probe,
	.remove = vpu_remove,
#ifdef CONFIG_PM
	.suspend    = vpu_suspend,
	.resume     = vpu_resume,
#endif
};

int __init vpu_init(void)
{
	return platform_driver_register(&vpu_driver);
}

void __exit vpu_exit(void)
{
	platform_driver_unregister(&vpu_driver);
}

#ifndef MODULE
arch_initcall_sync(vpu_init);
module_exit(vpu_exit);
#endif

//MODULE_DESCRIPTION("meson vpu driver");
//MODULE_LICENSE("GPL v2");
