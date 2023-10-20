// SPDX-License-Identifier: GPL-2.0+
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/clk.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include "../lcd_reg.h"
#include "../lcd_common.h"
#include "lcd_clk_config.h"
#include "lcd_clk_ctrl.h"
#include "lcd_clk_utils.h"

static struct mutex lcd_clk_mutex;

static char *lcd_ss_freq_table_dft[] = {
	"0, 29.5KHz",
	"1, 31.5KHz",
	"2, 50KHz",
	"3, 75KHz",
	"4, 100KHz",
	"5, 150KHz",
	"6, 200KHz",
};

static char *lcd_ss_mode_table_dft[] = {
	"0, center ss",
	"1, up ss",
	"2, down ss",
};

struct lcd_clk_config_s *get_lcd_clk_config(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (!pdrv->clk_conf) {
		LCDERR("[%d]: %s: clk_config is null\n", pdrv->index, __func__);
		return NULL;
	}
	cconf = (struct lcd_clk_config_s *)pdrv->clk_conf;
	if (!cconf->data) {
		LCDERR("[%d]: %s: clk config data is null\n",
		       pdrv->index, __func__);
		return NULL;
	}

	return cconf;
}

/* ****************************************************
 * lcd clk function api
 * ****************************************************
 */
void lcd_clk_frac_generate(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	/* update bit_rate by interface */
	switch (pdrv->config.basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_bit_rate_config(pdrv);
		break;
	case LCD_MLVDS:
		lcd_mlvds_bit_rate_config(pdrv);
		break;
	case LCD_P2P:
		lcd_p2p_bit_rate_config(pdrv);
		break;
	case LCD_MIPI:
		lcd_mipi_dsi_bit_rate_config(pdrv);
		break;
	case LCD_EDP:
		lcd_edp_bit_rate_config(pdrv);
	default:
		break;
	}
	if (cconf->data->pll_frac_generate)
		cconf->data->pll_frac_generate(pdrv);
}

void lcd_clk_generate_parameter(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	/* update bit_rate by interface */
	switch (pdrv->config.basic.lcd_type) {
	case LCD_VBYONE:
		lcd_vbyone_bit_rate_config(pdrv);
		break;
	case LCD_MLVDS:
		lcd_mlvds_bit_rate_config(pdrv);
		break;
	case LCD_P2P:
		lcd_p2p_bit_rate_config(pdrv);
		break;
	case LCD_MIPI:
		lcd_mipi_dsi_bit_rate_config(pdrv);
		break;
	case LCD_EDP:
		lcd_edp_bit_rate_config(pdrv);
	default:
		break;
	}
	if (cconf->data->clk_generate_parameter)
		cconf->data->clk_generate_parameter(pdrv);
}

int lcd_get_ss_num(struct aml_lcd_drv_s *pdrv,
	unsigned int *level, unsigned int *freq, unsigned int *mode)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data) {
		LCDERR("[%d] %s: clk_conf is null\n", pdrv->index, __func__);
		return -1;
	}

	if (cconf->data->ss_support == 0) {
		if (level)
			*level = 0;
		if (freq)
			*freq = 0;
		if (mode)
			*mode = 0;
	} else {
		if (level)
			*level = cconf->ss_level;
		if (freq)
			*freq = cconf->ss_freq;
		if (mode)
			*mode = cconf->ss_mode;
	}

	return 0;
}

int lcd_get_ss(struct aml_lcd_drv_s *pdrv, char *buf)
{
	struct lcd_clk_config_s *cconf;
	int len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data) {
		len += sprintf(buf + len, "[%d]: clk config data is null\n", pdrv->index);
		return len;
	}
	if (cconf->data->ss_support == 0) {
		len += sprintf(buf + len, "[%d]: spread spectrum is not support\n", pdrv->index);
		return len;
	}

	len += sprintf(buf + len, "ss_level: %d, %dppm, dep_sel=%d, str_m=%d\n",
		cconf->ss_level, cconf->ss_ppm,
		cconf->ss_dep_sel, cconf->ss_str_m);
	len += sprintf(buf + len, "ss_freq: %s\n",
		lcd_ss_freq_table_dft[cconf->ss_freq]);
	len += sprintf(buf + len, "ss_mode: %s\n",
		lcd_ss_mode_table_dft[cconf->ss_mode]);

	return len;
}

int lcd_set_ss(struct aml_lcd_drv_s *pdrv, unsigned int level, unsigned int freq, unsigned int mode)
{
	struct lcd_clk_config_s *cconf;
	int ss_adv = 0, ret = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return -1;
	if (cconf->data->ss_support == 0) {
		LCDERR("[%d]: %s: not support\n", pdrv->index, __func__);
		return -1;
	}

	mutex_lock(&lcd_clk_mutex);

	if (level < 0xff) {
		if (level > cconf->data->ss_level_max) {
			LCDERR("[%d]: %s: ss_level %d is out of support (max %d)\n",
			       pdrv->index, __func__, level, cconf->data->ss_level_max);
			ret = -1;
			goto lcd_set_ss_end;
		}
		cconf->ss_level = level;
		if (cconf->data->set_ss_level)
			cconf->data->set_ss_level(pdrv);
	}
	if (freq < 0xff) {
		if (freq > cconf->data->ss_freq_max) {
			LCDERR("[%d]: %s: ss_freq %d is out of support (max %d)\n",
			       pdrv->index, __func__, freq, cconf->data->ss_freq_max);
			ret = -1;
			goto lcd_set_ss_end;
		}
		cconf->ss_freq = freq;
		ss_adv = 1;
	}
	if (mode < 0xff) {
		if (mode > cconf->data->ss_mode_max) {
			LCDERR("[%d]: %s: ss_mode %d is out of support (max %d)\n",
			       pdrv->index, __func__, mode, cconf->data->ss_mode_max);
			ret = -1;
			goto lcd_set_ss_end;
		}
		cconf->ss_mode = mode;
		ss_adv = 1;
	}
	if (ss_adv == 0)
		goto lcd_set_ss_end;
	if (cconf->data->set_ss_advance)
		cconf->data->set_ss_advance(pdrv);

lcd_set_ss_end:
	mutex_unlock(&lcd_clk_mutex);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	return ret;
}

/* design for vlock, don't save ss_level to clk_config */
//can't mutex_lock for atomic context
int lcd_ss_enable(int index, unsigned int flag)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", index, __func__);

	pdrv = aml_lcd_get_driver(index);
	if (!pdrv) {
		LCDERR("[%d]: %s: drv is null\n", index, __func__);
		return -1;
	}
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return -1;

	if (cconf->data->clk_ss_enable)
		cconf->data->clk_ss_enable(pdrv, flag);

	return 0;
}

int lcd_encl_clk_msr(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	int clk_mux;
	int encl_clk = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return 0;

	clk_mux = cconf->data->enc_clk_msr_id;
	if (clk_mux == -1)
		return 0;
	encl_clk = meson_clk_measure(clk_mux);

	return encl_clk;
}

void lcd_clk_pll_reset(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	mutex_lock(&lcd_clk_mutex);
	if (cconf->data->pll_reset)
		cconf->data->pll_reset(pdrv);
	mutex_unlock(&lcd_clk_mutex);
	LCDPR("[%d]: %s\n", pdrv->index, __func__);
}

//can't mutex_lock for atomic context
void lcd_vlock_m_update(int index, unsigned int vlock_m)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_clk_config_s *cconf;

	pdrv = aml_lcd_get_driver(index);
	if (!pdrv) {
		LCDERR("[%d]: %s: drv is null\n", index, __func__);
		return;
	}
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	vlock_m &= 0xff;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s, vlcok_m: 0x%x\n", index, __func__, vlock_m);

	if (cconf->data->pll_m_set)
		cconf->data->pll_m_set(pdrv, vlock_m);
}

//can't mutex_lock for atomic context
void lcd_vlock_frac_update(int index, unsigned int vlock_frac)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_clk_config_s *cconf;

	pdrv = aml_lcd_get_driver(index);
	if (!pdrv) {
		LCDERR("[%d]: %s: drv is null\n", index, __func__);
		return;
	}
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	vlock_frac &= 0x1ffff;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s, vlock_frac: 0x%x\n", index, __func__, vlock_frac);

	if (cconf->data->pll_frac_set)
		cconf->data->pll_frac_set(pdrv, vlock_frac);
}

/* for frame rate change */
void lcd_update_clk_frac(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return; // goto lcd_clk_update_end;

	mutex_lock(&lcd_clk_mutex);

	if (cconf->data->pll_frac_set)
		cconf->data->pll_frac_set(pdrv, cconf->pll_frac);

	mutex_unlock(&lcd_clk_mutex);
	LCDPR("[%d]: %s: pll_frac=0x%x\n", pdrv->index, __func__, cconf->pll_frac);
}

/* for timing init */
void lcd_set_clk(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	int cnt = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	if (pdrv->lcd_pxp) {
		if (cconf->data->vclk_crt_set)
			cconf->data->vclk_crt_set(pdrv);
		return;
	}

	mutex_lock(&lcd_clk_mutex);
lcd_set_clk_retry:
	if (cconf->data->clk_set)
		cconf->data->clk_set(pdrv);
	if (cconf->data->vclk_crt_set)
		cconf->data->vclk_crt_set(pdrv);
	usleep_range(10000, 10001);

	while (lcd_clk_msr_check(pdrv)) {
		if (cnt++ >= 5) {
			LCDERR("[%d]: %s timeout\n", pdrv->index, __func__);
			break;
		}
		goto lcd_set_clk_retry;
	}

	if (cconf->data->clktree_set)
		cconf->data->clktree_set(pdrv);
	mutex_unlock(&lcd_clk_mutex);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
}

void lcd_disable_clk(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	mutex_lock(&lcd_clk_mutex);
	if (cconf->data->clk_disable)
		cconf->data->clk_disable(pdrv);
	mutex_unlock(&lcd_clk_mutex);

	LCDPR("[%d]: %s\n", pdrv->index, __func__);
}

static void lcd_clk_gate_optional_switch(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	if (!cconf->data->clk_gate_optional_switch)
		return;

	if (status) {
		if (cconf->clktree.clk_gate_optional_state) {
			LCDPR("[%d]: clk_gate_optional is already on\n",
			      pdrv->index);
		} else {
			cconf->data->clk_gate_optional_switch(pdrv, 1);
		}
	} else {
		if (cconf->clktree.clk_gate_optional_state == 0) {
			LCDPR("[%d]: clk_gate_optional is already off\n",
			      pdrv->index);
		} else {
			cconf->data->clk_gate_optional_switch(pdrv, 0);
		}
	}
}

void lcd_clk_gate_switch(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	if (status) {
		if (cconf->clktree.clk_gate_state) {
			LCDPR("[%d]: clk_gate is already on\n", pdrv->index);
		} else {
#ifdef CONFIG_AMLOGIC_VPU
			vpu_dev_clk_gate_on(pdrv->lcd_vpu_dev);
#endif
			if (cconf->data->clk_gate_switch)
				cconf->data->clk_gate_switch(pdrv, 1);
			cconf->clktree.clk_gate_state = 1;
		}
		lcd_clk_gate_optional_switch(pdrv, 1);
	} else {
		lcd_clk_gate_optional_switch(pdrv, 0);
		if (cconf->clktree.clk_gate_state == 0) {
			LCDPR("[%d]: clk_gate is already off\n",  pdrv->index);
		} else {
			if (cconf->data->clk_gate_switch)
				cconf->data->clk_gate_switch(pdrv, 0);
#ifdef CONFIG_AMLOGIC_VPU
			vpu_dev_clk_gate_off(pdrv->lcd_vpu_dev);
#endif
			cconf->clktree.clk_gate_state = 0;
		}
	}
}

int lcd_clk_clkmsr_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int clk;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "[%d]: %s: clk config is null\n",
				pdrv->index, __func__);
		return len;
	}

	if (cconf->data->enc_clk_msr_id == -1)
		goto lcd_clk_clkmsr_print_step_1;
	clk = meson_clk_measure(cconf->data->enc_clk_msr_id);
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"encl_clk:    %u\n", clk);

lcd_clk_clkmsr_print_step_1:
	if (cconf->data->fifo_clk_msr_id == -1)
		goto lcd_clk_clkmsr_print_step_2;
	clk = meson_clk_measure(cconf->data->fifo_clk_msr_id);
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"fifo_clk:    %u\n", clk);

lcd_clk_clkmsr_print_step_2:
	switch (pdrv->config.basic.lcd_type) {
	case LCD_MLVDS:
	case LCD_P2P:
		if (cconf->data->tcon_clk_msr_id == -1)
			break;
		clk = meson_clk_measure(cconf->data->tcon_clk_msr_id);
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"tcon_clk:    %u\n", clk);
	default:
		break;
	}

	return len;
}

int lcd_clk_config_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "[%d]: %s: clk config is null\n",
				pdrv->index, __func__);
		return len;
	}

	if (cconf->data->clk_config_print)
		len = cconf->data->clk_config_print(pdrv, buf, offset);

	return len;
}

void aml_lcd_prbs_test(struct aml_lcd_drv_s *pdrv, unsigned int ms, unsigned int mode_flag)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return;

	if (cconf->data->prbs_test)
		cconf->data->prbs_test(pdrv, ms, mode_flag);
}

int lcd_clk_path_change(struct aml_lcd_drv_s *pdrv, int sel)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return -1;

	if (cconf->clk_path_change)
		cconf->clk_path_change(pdrv, sel);

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		if (cconf->data->clk_config_init_print)
			cconf->data->clk_config_init_print(pdrv);
	}

	return 0;
}

void lcd_clk_config_parameter_init(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned int ss_level, ss_freq, ss_mode;

	if (!cconf || !cconf->data)
		return;

	if (cconf->data->clk_parameter_init)
		cconf->data->clk_parameter_init(pdrv);

	ss_level = pdrv->config.timing.ss_level;
	cconf->ss_level = (ss_level > cconf->data->ss_level_max) ?
				cconf->data->ss_level_max : ss_level;
	if (cconf->ss_level == 0)
		cconf->ss_en = 0;
	else
		cconf->ss_en = 1;

	ss_freq = pdrv->config.timing.ss_freq;
	cconf->ss_freq = (ss_freq > cconf->data->ss_freq_max) ?
				cconf->data->ss_freq_max : ss_freq;

	ss_mode = pdrv->config.timing.ss_mode;
	cconf->ss_mode = (ss_mode > cconf->data->ss_mode_max) ? 0 : ss_mode;

	if (cconf->data->clk_ss_init)
		cconf->data->clk_ss_init(cconf);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: %s: ss_level=%d, ss_freq=%d, ss_mode=%d\n",
		      pdrv->index, __func__,
		      cconf->ss_level, cconf->ss_freq, cconf->ss_mode);
	}
}

static void lcd_clk_config_chip_init(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf)
{
	cconf->pll_id = 0;
	cconf->pll_offset = 0;
	cconf->fin = FIN_FREQ;
	switch (pdrv->data->chip_type) {
	case LCD_CHIP_AXG:
		lcd_clk_config_chip_init_axg(pdrv, cconf);
		break;
	case LCD_CHIP_G12A:
	case LCD_CHIP_SM1:
		lcd_clk_config_chip_init_g12a(pdrv, cconf);
		break;
	case LCD_CHIP_G12B:
		lcd_clk_config_chip_init_g12b(pdrv, cconf);
		break;
	case LCD_CHIP_TL1:
		lcd_clk_config_chip_init_tl1(pdrv, cconf);
		break;
	case LCD_CHIP_TM2:
		lcd_clk_config_chip_init_tm2(pdrv, cconf);
		break;
	case LCD_CHIP_T5:
		lcd_clk_config_chip_init_t5(pdrv, cconf);
		break;
	case LCD_CHIP_T5D:
		lcd_clk_config_chip_init_t5d(pdrv, cconf);
		break;
	case LCD_CHIP_T7:
		lcd_clk_config_chip_init_t7(pdrv, cconf);
		break;
	case LCD_CHIP_T3: /* only one pll */
		lcd_clk_config_chip_init_t3(pdrv, cconf);
		break;
	case LCD_CHIP_T5W:
		lcd_clk_config_chip_init_t5w(pdrv, cconf);
		break;
	case LCD_CHIP_A4:
		lcd_clk_config_chip_init_a4(pdrv, cconf);
		break;
	default:
		LCDPR("[%d]: %s: invalid chip type\n", pdrv->index, __func__);
		return;
	}

	cconf->pll_od_fb = cconf->data->pll_od_fb;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		if (cconf->data->clk_config_init_print)
			cconf->data->clk_config_init_print(pdrv);
	}
}

void lcd_clk_config_probe(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = kzalloc(sizeof(*cconf), GFP_KERNEL);
	if (!cconf)
		return;
	pdrv->clk_conf = (void *)cconf;

	lcd_clk_config_chip_init(pdrv, cconf);
	if (!cconf->data)
		return;

	if (cconf->data->clktree_probe)
		cconf->data->clktree_probe(pdrv);
}

void lcd_clk_config_remove(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (!pdrv->clk_conf)
		return;

	cconf = (struct lcd_clk_config_s *)pdrv->clk_conf;
	if (cconf->data && cconf->data->clktree_remove)
		cconf->data->clktree_remove(pdrv);

	kfree(cconf);
	pdrv->clk_conf = NULL;
}

void lcd_clk_init(void)
{
	mutex_init(&lcd_clk_mutex);
}
