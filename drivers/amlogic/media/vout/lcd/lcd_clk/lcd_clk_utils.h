/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _LCD_CLK_UTILS_H
#define _LCD_CLK_UTILS_H

#include "lcd_clk_config.h"
// #include <linux/amlogic/media/vout/lcd/lcd_vout.h>

/* ****************************************************
 * lcd pll & clk operation
 * ****************************************************
 */
#define PLL_CLK_CHECK_MAX    2000000 /* Hz */
inline unsigned long long lcd_abs(unsigned long long a, unsigned long long b);
int lcd_clk_msr_check(struct aml_lcd_drv_s *pdrv);
int lcd_pll_ss_level_generate(struct lcd_clk_config_s *cconf);
int lcd_pll_wait_lock(unsigned int reg, unsigned int lock_bit);
int lcd_pll_wait_lock_hiu(unsigned int reg, unsigned int lock_bit);

/* ****************************************************
 * lcd clk parameters calculate
 * ****************************************************
 */
#define PLL_FVCO_ERR_MAX    2000 /* Hz */
unsigned long long clk_vid_pll_div_calc(unsigned long long clk, unsigned int div_sel, int dir);
int lcd_pll_get_frac(struct lcd_clk_config_s *cconf, unsigned long long pll_fvco);
int check_pll_3od(struct lcd_clk_config_s *cconf, unsigned long long pll_fout);
int check_pll_1od(struct lcd_clk_config_s *cconf, unsigned long long pll_fout);
int check_vco(struct lcd_clk_config_s *cconf, unsigned long long pll_fvco);
int check_od(struct lcd_clk_config_s *cconf, unsigned long long pll_fout);

/* ****************************************************
 * lcd clk chip default func
 * ****************************************************
 */
int lcd_clk_config_print_dft(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
void lcd_pll_frac_generate_dft(struct aml_lcd_drv_s *pdrv);
void lcd_clk_config_init_print_dft(struct aml_lcd_drv_s *pdrv);
void lcd_clk_generate_dft(struct aml_lcd_drv_s *pdrv);
void lcd_set_vid_pll_div_dft(struct aml_lcd_drv_s *pdrv);

#define MAX_CLKTREE_GATE 6
enum clktree_type {
	CLKTREE_GP0_PLL = 1,
	CLKTREE_ENCL_TOP_GATE,
	CLKTREE_ENCL_INT_GATE,
	CLKTREE_DSI_HOST_GATE,
	CLKTREE_DSI_PHY_GATE,
	CLKTREE_DSI_MEAS,
	CLKTREE_MIPI_ENABLE_GATE,
	CLKTREE_MIPI_BANDGAP_GATE,
	CLKTREE_TCON_GATE,
	CLKTREE_TCON,
};

void lcd_clktree_bind(struct aml_lcd_drv_s *pdrv, unsigned char status);
void lcd_clktree_gate_switch(struct aml_lcd_drv_s *pdrv, unsigned char status);

/* ****************************************************
 * lcd clk chip init help func
 * ****************************************************
 */
void lcd_clk_config_chip_init_axg(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_g12a(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_g12b(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_t7(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_t3(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_tl1(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_tm2(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_t5(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_t5d(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_t5w(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_a4(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);

/* ****************************************************
 * lcd clk prbs func
 * ****************************************************
 */
extern unsigned int lcd_prbs_flag, lcd_prbs_performed, lcd_prbs_err;
int lcd_prbs_clk_check(unsigned long encl_clk, unsigned int encl_msr_id,
			    unsigned long fifo_clk, unsigned int fifo_msr_id, unsigned int cnt);

#endif
