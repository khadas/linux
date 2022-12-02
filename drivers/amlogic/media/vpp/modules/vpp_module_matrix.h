/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_MATRIX_H__
#define __VPP_MODULE_MATRIX_H__

enum matrix_mode_e {
	EN_MTRX_MODE_CMPT = 0,
	EN_MTRX_MODE_VD1,
	EN_MTRX_MODE_POST,
	EN_MTRX_MODE_POST2,
	EN_MTRX_MODE_OSD2,
	EN_MTRX_MODE_WRAP_OSD1,
	EN_MTRX_MODE_WRAP_OSD2,
	EN_MTRX_MODE_WRAP_OSD3,
	EN_MTRX_MODE_MAX,
};

int vpp_module_matrix_init(struct vpp_dev_s *pdev);
int vpp_module_matrix_en(enum matrix_mode_e mode, bool enable);
int vpp_module_matrix_sel(enum matrix_mode_e mode, int val);
int vpp_module_matrix_clmod(enum matrix_mode_e mode, int val);
int vpp_module_matrix_rs(enum matrix_mode_e mode, int val);
int vpp_module_matrix_set_offset(enum matrix_mode_e mode,
	int *pdata);
int vpp_module_matrix_set_pre_offset(enum matrix_mode_e mode,
	int *pdata);
int vpp_module_matrix_set_coef_3x3(enum matrix_mode_e mode, int *pdata);
int vpp_module_matrix_set_coef_3x5(enum matrix_mode_e mode, int *pdata);
int vpp_module_matrix_set_contrast_uv(int val_u, int val_v);
void vpp_module_matrix_on_vs(void);

void vpp_module_matrix_dump_info(enum vpp_dump_module_info_e info_type);

#endif

