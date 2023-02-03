/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_GAMMA_H__
#define __VPP_MODULE_GAMMA_H__

int vpp_module_gamma_init(struct vpp_dev_s *pdev);
void vpp_module_gamma_set_viu_sel(int val);
int vpp_module_pre_gamma_en(bool enable);
int vpp_module_pre_gamma_write_single(enum vpp_rgb_mode_e mode,
	unsigned int *pdata);
int vpp_module_pre_gamma_write(unsigned int *pr_data,
	unsigned int *pg_data, unsigned int *pb_data);
int vpp_module_pre_gamma_read(unsigned int *pr_data,
	unsigned int *pg_data, unsigned int *pb_data);
int vpp_module_pre_gamma_pattern(bool enable,
	unsigned int r_val, unsigned int g_val, unsigned int b_val);
unsigned int vpp_module_pre_gamma_get_table_len(void);
void vpp_module_pre_gamma_on_vs(void);
int vpp_module_lcd_gamma_en(bool enable);
int vpp_module_lcd_gamma_write_single(enum vpp_rgb_mode_e mode,
	unsigned int *pdata);
int vpp_module_lcd_gamma_write(unsigned int *pr_data,
	unsigned int *pg_data, unsigned int *pb_data);
int vpp_module_lcd_gamma_read(unsigned int *pr_data,
	unsigned int *pg_data, unsigned int *pb_data);
int vpp_module_lcd_gamma_pattern(bool enable,
	unsigned int r_val, unsigned int g_val, unsigned int b_val);
unsigned int vpp_module_lcd_gamma_get_table_len(void);
void vpp_module_lcd_gamma_notify(void);
void vpp_module_lcd_gamma_on_vs(void);

void vpp_module_gamma_dump_info(enum vpp_dump_module_info_e info_type);

#endif

