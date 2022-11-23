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
int lcd_clk_msr_check(struct aml_lcd_drv_s *pdrv);
int lcd_pll_ss_level_generate(unsigned int *data, unsigned int level, unsigned int step);
int lcd_pll_wait_lock(unsigned int reg, unsigned int lock_bit);

/* ****************************************************
 * lcd clk parameters calculate
 * ****************************************************
 */
#define PLL_FVCO_ERR_MAX    2 /* kHz */
unsigned int clk_vid_pll_div_calc(unsigned int clk, unsigned int div_sel, int dir);
unsigned int clk_vid_pll_div_get(unsigned int clk_div);
int check_pll_3od(struct lcd_clk_config_s *cconf, unsigned int pll_fout);
int check_pll_1od(struct lcd_clk_config_s *cconf, unsigned int pll_fout);
int check_vco(struct lcd_clk_config_s *cconf, unsigned int pll_fvco);
int check_od(struct lcd_clk_config_s *cconf, unsigned int pll_fout);
int edp_div_check(struct lcd_clk_config_s *cconf, unsigned int bit_rate);

/* ****************************************************
 * lcd clk chip default func
 * ****************************************************
 */
int lcd_clk_config_print_dft(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
void lcd_pll_frac_generate_dft(struct aml_lcd_drv_s *pdrv);
void lcd_clk_disable_dft(struct aml_lcd_drv_s *pdrv);
void lcd_clk_gate_switch_dft(struct aml_lcd_drv_s *pdrv, int status);
void lcd_clk_config_init_print_dft(struct aml_lcd_drv_s *pdrv);
void lcd_clk_generate_dft(struct aml_lcd_drv_s *pdrv);
void lcd_clk_gate_optional_switch_fdt(struct aml_lcd_drv_s *pdrv, int status);
void lcd_set_vid_pll_div_dft(struct aml_lcd_drv_s *pdrv);

/* ****************************************************
 * lcd clk chip init help func
 * ****************************************************
 */
void lcd_clk_config_chip_init_axg(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_g12a(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_g12b(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_t7(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_t3(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
void lcd_clk_config_chip_init_tl1(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
#endif
void lcd_clk_config_chip_init_tm2(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_t5(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_t5d(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);
void lcd_clk_config_chip_init_t5w(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf);

/* ****************************************************
 * lcd clk prbs func
 * ****************************************************
 */
extern unsigned long lcd_encl_clk_check_std;
extern unsigned long lcd_fifo_clk_check_std;
extern unsigned int lcd_prbs_flag, lcd_prbs_performed, lcd_prbs_err;
int lcd_prbs_clk_check(unsigned long encl_clk, unsigned int encl_msr_id,
			    unsigned long fifo_clk, unsigned int fifo_msr_id, unsigned int cnt);

#endif
