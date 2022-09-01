/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_CSC_H__
#define __VPP_CSC_H__

#define s64 int

#define SUPPORT_SDR    (BIT(1))
#define SUPPORT_HDR    (BIT(2))
#define SUPPORT_HLG    (BIT(3))
#define SUPPORT_HDRP   (BIT(4))
#define SUPPORT_BT2020 (BIT(5))
#define SUPPORT_DV_SHF (6)
#define SUPPORT_DV     (3 << SUPPORT_DV_SHF)

#define CSC_MTRX_5x3_COEF_SIZE 24
#define INORM  50000
#define BL  16

enum csc_output_format_e {
	EN_UNKNOWN = 0,
	EN_BT709,
	EN_BT2020,
	EN_BT2020_PQ,
	EN_BT2020_PQ_DYNAMIC,
	EN_BT2020_HLG,
	EN_BT2100_IPT,
	EN_BT_BYPASS, /*force bypass all process*/
};

struct csc_mtrx_param_s {
	unsigned int pre_offset[3];
	unsigned int coef[3][3];
	unsigned int offset[3];
	unsigned int right_shift;
};

int vpp_csc_init(struct vpp_dev_s *pdev);
void vpp_csc_set_force_output(enum csc_output_format_e format);
enum csc_output_format_e vpp_csc_get_force_output(void);
void vpp_csc_set_hdr_policy(int val);
int vpp_csc_get_hdr_policy(void);
/*
 *void vpp_csc_calculate_gamut_mtrx(s64 (*src_prmy)[2], s64 (*dst_prmy)[2],
 *	s64 (*tout)[3], int norm, int obl);
 *void vpp_csc_calculate_mtrx_setting(s64 (*in)[3],
 *	int ibl, int obl, struct csc_mtrx_param_s *m);
 */
void vpp_csc_set_customer_mtrx_en(bool val);
bool vpp_csc_get_customer_mtrx_en(void);
bool vpp_csc_get_customer_mtrx_used(void);
void vpp_csc_set_customer_hdmi_display_en(bool val);
bool vpp_csc_get_customer_hdmi_display_en(void);
void vpp_csc_set_customer_master_display_en(bool val);
bool vpp_csc_get_customer_master_display_en(void);
void vpp_csc_set_customer_panel_lumin(int val);
int vpp_csc_get_customer_panel_lumin(void);
void vpp_csc_set_hdr_process_status(int vd_path, int val);
int vpp_csc_get_hdr_process_status(int vd_path);
void vpp_csc_eo_clip_proc(unsigned int max_luma,
	unsigned int present_flag, int *ptbl_prmy_maxl, int *ptbl_margin_maxl,
	int *peo_y_lut_hdr_10000, int *peo_y_lut_src, int *peo_y_lut_dest);

#endif

