// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#ifdef CONFIG_AMLOGIC_POWER
#include <linux/amlogic/power_domain.h>
#endif
#include <linux/amlogic/media/vpu/vpu.h>
#include "vpu_reg.h"
#include "vpu.h"

int vpu_power_init_check_dft(void)
{
	unsigned int val;
	int ret = 0;

	val = vpu_clk_getb(vpu_conf.data->vpu_clk_reg, 31, 1);
	if (val)
		val = vpu_clk_getb(vpu_conf.data->vpu_clk_reg, 24, 1);
	else
		val = vpu_clk_getb(vpu_conf.data->vpu_clk_reg, 8, 1);
	ret = (val == 0) ? 1 : 0;
	if (vpu_debug_print_flag) {
		VPUPR("%s: vpu_clk_ctrl: 0x%08x, ret=%d\n",
		      __func__, vpu_clk_read(vpu_conf.data->vpu_clk_reg), ret);
	}

	return ret;
}

void vpu_mem_pd_init_off(void)
{
}

void vpu_module_init_config(void)
{
	struct vpu_ctrl_s *ctrl_table;
	unsigned int _reg, _val, _bit, _len;
	int i = 0;

	VPUPR("%s\n", __func__);

	/* vpu clk gate init off */
	//ctrl_table = vpu_conf.data->module_init_table;
	if (vpu_conf.data->module_init_table) {
		ctrl_table = vpu_conf.data->module_init_table;
		i = 0;
		while (i < VPU_MOD_INIT_CNT_MAX) {
			if (ctrl_table[i].reg == VPU_REG_END)
				break;
			_reg = ctrl_table[i].reg;
			_val = ctrl_table[i].val;
			_bit = ctrl_table[i].bit;
			_len = ctrl_table[i].len;
			vpu_vcbus_setb(_reg, _val, _bit, _len);
			i++;
		}
	}

	/*don't set dmc_arb config in kernel*/

	if (vpu_debug_print_flag)
		VPUPR("%s finish\n", __func__);
}

void vpu_power_on(void)
{
	struct vpu_ctrl_s *ctrl_table;
	struct vpu_reset_s *reset_table;
	unsigned int _reg, _val, _bit, _len, mask;
	int i = 0;

	VPUPR("%s\n", __func__);

	/* power on VPU_HDMI power */
	ctrl_table = vpu_conf.data->power_table;
	if (ctrl_table) {
		i = 0;
		while (i < VPU_HDMI_ISO_CNT_MAX) {
			if (ctrl_table[i].reg == VPU_REG_END)
				break;
			_reg = ctrl_table[i].reg;
			_val = 0;
			_bit = ctrl_table[i].bit;
			_len = ctrl_table[i].len;
			vpu_ao_setb(_reg, _val, _bit, _len);
			i++;
		}
	}
	usleep_range(20, 25);

	/* power up memories */
	ctrl_table = vpu_conf.data->mem_pd_table;
	if (ctrl_table) {
		i = 0;
		while (i < VPU_MEM_PD_CNT_MAX) {
			if (ctrl_table[i].vmod == VPU_MOD_MAX)
				break;
			_reg = ctrl_table[i].reg;
			_bit = ctrl_table[i].bit;
			_len = ctrl_table[i].len;
			vpu_clk_setb(_reg, 0x0, _bit, _len);
			usleep_range(5, 10);
			i++;
		}
		for (i = 8; i < 16; i++) {
			vpu_clk_setb(HHI_MEM_PD_REG0, 0, i, 1);
			usleep_range(5, 10);
		}
		usleep_range(20, 25);
	}

	/* Reset VIU + VENC */
	/* Reset VENCI + VENCP + VADC + VENCL */
	/* Reset HDMI-APB + HDMI-SYS + HDMI-TX + HDMI-CEC */
	reset_table = vpu_conf.data->reset_table;
	if (reset_table) {
		i = 0;
		while (i < VPU_RESET_CNT_MAX) {
			if (reset_table[i].reg == VPU_REG_END)
				break;
			_reg = reset_table[i].reg;
			mask = reset_table[i].mask;
			vpu_cbus_clr_mask(_reg, mask);
			i++;
		}
		usleep_range(5, 10);
		/* release Reset */
		i = 0;
		while (i < VPU_RESET_CNT_MAX) {
			if (reset_table[i].reg == VPU_REG_END)
				break;
			_reg = reset_table[i].reg;
			mask = reset_table[i].mask;
			vpu_cbus_set_mask(_reg, mask);
			i++;
		}
	}

	/* Remove VPU_HDMI ISO */
	ctrl_table = vpu_conf.data->iso_table;
	if (ctrl_table) {
		i = 0;
		while (i < VPU_HDMI_ISO_CNT_MAX) {
			if (ctrl_table[i].reg == VPU_REG_END)
				break;
			_reg = ctrl_table[i].reg;
			_val = 0;
			_bit = ctrl_table[i].bit;
			_len = ctrl_table[i].len;
			vpu_ao_setb(_reg, _val, _bit, _len);
			i++;
		}
	}

	if (vpu_debug_print_flag)
		VPUPR("%s finish\n", __func__);
}

void vpu_power_off(void)
{
	struct vpu_ctrl_s *ctrl_table;
	unsigned int _val, _reg, _bit, _len;
	int i = 0;

	VPUPR("%s\n", __func__);

	/* Power down VPU_HDMI */
	/* Enable Isolation */
	ctrl_table = vpu_conf.data->iso_table;
	if (ctrl_table) {
		i = 0;
		while (i < VPU_HDMI_ISO_CNT_MAX) {
			if (ctrl_table[i].reg == VPU_REG_END)
				break;
			_reg = ctrl_table[i].reg;
			_val = ctrl_table[i].val;
			_bit = ctrl_table[i].bit;
			_len = ctrl_table[i].len;
			vpu_ao_setb(_reg, _val, _bit, _len);
			i++;
		}
	}
	usleep_range(20, 25);

	/* power down memories */
	ctrl_table = vpu_conf.data->mem_pd_table;
	if (ctrl_table) {
		i = 0;
		while (i < VPU_MEM_PD_CNT_MAX) {
			if (ctrl_table[i].vmod == VPU_MOD_MAX)
				break;
			_reg = ctrl_table[i].reg;
			_bit = ctrl_table[i].bit;
			_len = ctrl_table[i].len;
			if (_len == 32)
				_val = 0xffffffff;
			else
				_val = (1 << _len) - 1;
			vpu_clk_setb(_reg, _val, _bit, _len);
			usleep_range(5, 10);
			i++;
		}
		for (i = 8; i < 16; i++) {
			vpu_clk_setb(HHI_MEM_PD_REG0, 0x1, i, 1);
			usleep_range(5, 10);
		}
		usleep_range(20, 25);
	}

	/* Power down VPU domain */
	ctrl_table = vpu_conf.data->power_table;
	if (ctrl_table) {
		i = 0;
		while (i < VPU_HDMI_ISO_CNT_MAX) {
			if (ctrl_table[i].reg == VPU_REG_END)
				break;
			_reg = ctrl_table[i].reg;
			_val = ctrl_table[i].val;
			_bit = ctrl_table[i].bit;
			_len = ctrl_table[i].len;
			vpu_ao_setb(_reg, _val, _bit, _len);
			i++;
		}
	}

	if (vpu_debug_print_flag)
		VPUPR("%s finish\n", __func__);
}

void vpu_power_on_new(void)
{
#ifdef CONFIG_AMLOGIC_POWER
	unsigned int pwr_id;
	int i = 0;

	if (!vpu_conf.data->pwrctrl_id_table)
		return;

	while (i < VPU_PWR_ID_MAX) {
		pwr_id = vpu_conf.data->pwrctrl_id_table[i];
		if (pwr_id == VPU_PWR_ID_END)
			break;
		if (vpu_debug_print_flag)
			VPUPR("%s: pwr_id=%d\n", __func__, pwr_id);
		pwr_ctrl_psci_smc(pwr_id, 1);
		i++;
	}
	VPUPR("%s\n", __func__);
#else
	VPUERR("%s: no CONFIG_AMLOGIC_POWER\n", __func__);
#endif

	if (vpu_debug_print_flag)
		VPUPR("%s finish\n", __func__);
}

void vpu_power_off_new(void)
{
#ifdef CONFIG_AMLOGIC_POWER
	unsigned int pwr_id;
	int i = 0;

	VPUPR("%s\n", __func__);
	if (!vpu_conf.data->pwrctrl_id_table)
		return;

	while (i < VPU_PWR_ID_MAX) {
		pwr_id = vpu_conf.data->pwrctrl_id_table[i];
		if (pwr_id == VPU_PWR_ID_END)
			break;
		if (vpu_debug_print_flag)
			VPUPR("%s: pwr_id=%d\n", __func__, pwr_id);
		pwr_ctrl_psci_smc(pwr_id, 0);
		i++;
	}
#else
	VPUERR("%s: no CONFIG_AMLOGIC_POWER\n", __func__);
#endif

	if (vpu_debug_print_flag)
		VPUPR("%s finish\n", __func__);
}
