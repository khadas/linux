/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_HDR_H__
#define __VPP_MODULE_HDR_H__

#define HDR_EOTF_LUT_SIZE (143)
#define HDR_OETF_LUT_SIZE (149)
#define HDR_CGAIN_LUT_SIZE (65)
#define HDR_OGAIN_LUT_SIZE (149)
#define HDR_MTRX_COEF_SIZE (15)
#define HDR_MTRX_OFFSET_SIZE (3)
#define HDR_HIST_BACKUP_COUNT (16)
#define HDR_HIST_PERCENT_BACKUP_COUNT (9)

enum hdr_vpp_type_e {
	EN_TYPE_VPP0 = 0,
	EN_TYPE_VPP1,
	EN_TYPE_VPP2,
	EN_TYPE_VPP_MAX,
};

enum hdr_module_type_e {
	EN_MODULE_TYPE_VDIN0 = 0,
	EN_MODULE_TYPE_VDIN1,
	EN_MODULE_TYPE_VD1,
	EN_MODULE_TYPE_VD2,
	EN_MODULE_TYPE_VD3,
	EN_MODULE_TYPE_OSD1,
	EN_MODULE_TYPE_OSD2,
	EN_MODULE_TYPE_OSD3,
	EN_MODULE_TYPE_DI,
	EN_MODULE_TYPE_MAX,
};

enum hdr_sub_module_e {
	EN_SUB_MODULE_CGAIN = 0,
	EN_SUB_MODULE_OGAIN,
	EN_SUB_MODULE_OETF,
	EN_SUB_MODULE_EOTF,
	EN_SUB_MODULE_MTRXI,
	EN_SUB_MODULE_MTRXO,
	EN_SUB_MODULE_GAMUT,
	EN_SUB_MODULE_CGAIN_OFFSET,
	EN_SUB_MODULE_MAX,
};

enum hdr_hist_data_type_e {
	EN_HIST_DATA_RGB_MAX = 0,
	EN_HIST_DATA_LUMA = 1,
	EN_HIST_DATA_SAT = 2,
	EN_HIST_DATA_BEFORE_GM = 4,
	EN_HIST_DATA_AFTER_GM = 6,
};

struct hdr_matrix_data_s {
	int coef[HDR_MTRX_COEF_SIZE]; /*5x3*/
	int pre_offset[HDR_MTRX_OFFSET_SIZE];
	int offset[HDR_MTRX_OFFSET_SIZE];
};

struct hdr_gamut_data_s {
	int gamut_mode; /*1/2:gamut before/after ootf, others:disable*/
	int gamut_shift;
	int gamut_coef[9]; /*3x3*/
	int cgain_coef[3];
	int adpscl_mode;
	int adpscl_bypass[3];
	int adpscl_alpha[3];
	int adpscl_shift[3];
	int adpscl_beta[3];
	int adpscl_beta_s[3];
	int adpscl_ys_coef[3];
};

struct hdr_hist_report_s {
	unsigned int hist_buff[HDR_HIST_BACKUP_COUNT][VPP_HDR_HIST_BIN_COUNT];
	unsigned int percentile[HDR_HIST_PERCENT_BACKUP_COUNT];
};

int vpp_module_hdr_init(struct vpp_dev_s *pdev);
int vpp_module_hdr_en(enum hdr_module_type_e type, bool enable,
	enum hdr_vpp_type_e vpp_sel);
void vpp_module_hdr_sub_module_en(enum hdr_module_type_e type,
	enum hdr_sub_module_e sub_module, bool enable,
	enum hdr_vpp_type_e vpp_sel);
void vpp_module_hdr_set_lut(enum hdr_module_type_e type,
	enum hdr_sub_module_e sub_module, int *pdata,
	enum hdr_vpp_type_e vpp_sel);
void vpp_module_hdr_set_matrix(enum hdr_module_type_e type,
	enum hdr_sub_module_e sub_module, struct hdr_matrix_data_s *pdata,
	enum hdr_vpp_type_e vpp_sel);
void vpp_module_hdr_set_gamut(enum hdr_module_type_e type,
	struct hdr_gamut_data_s *pdata, enum hdr_vpp_type_e vpp_sel);
void vpp_module_hdr_hist_en(enum hdr_module_type_e type, bool enable);
void vpp_module_hdr_fetch_hist_report(enum hdr_module_type_e type,
	enum hdr_hist_data_type_e data_type, int hist_width, int hist_height);
struct hdr_hist_report_s *vpp_module_hdr_get_hist_report(void);
void vpp_module_hdr_get_lut(enum hdr_module_type_e type,
	enum hdr_sub_module_e sub_module, int *pdata,
	enum hdr_vpp_type_e vpp_sel);
void vpp_module_hdr_on_vs(void);

void vpp_module_hdr_dump_info(enum vpp_dump_module_info_e info_type);

#endif

