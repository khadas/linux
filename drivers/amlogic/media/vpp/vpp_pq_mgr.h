/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_PQ_PROC_H__
#define __VPP_PQ_PROC_H__

struct vpp_pq_mgr_settings {
	struct vpp_pq_state_s pq_status;
	int pc_mode;
	int brightness;      /*-1024~1023*/
	int contrast;        /*-1024~1023*/
	int hue;             /*-25~25*/
	int saturation;      /*-128~127*/
	int brightness_post; /*-1024~1023*/
	int contrast_post;   /*-1024~1023*/
	int hue_post;        /*-25~25*/
	int saturation_post; /*-128~127*/
	int sharpness;       /*0~255*/
	struct vpp_white_balance_s video_rgb_ogo;
	struct vpp_gamma_table_s cur_pre_gamma_tbl;
	struct vpp_gamma_table_s cur_gamma_tbl;
	struct vpp_mtrx_param_s matrix_param[EN_MTRX_MAX];
	struct vpp_dnlp_curve_param_s dnlp_param;
};

int vpp_pq_mgr_init(struct vpp_dev_s *pdev);
void vpp_pq_mgr_deinit(void);
int vpp_pq_mgr_set_status(struct vpp_pq_state_s *pstatus);
int vpp_pq_mgr_set_brightness(int val);
int vpp_pq_mgr_set_brightness_post(int val);
int vpp_pq_mgr_set_contrast(int val);
int vpp_pq_mgr_set_contrast_post(int val);
int vpp_pq_mgr_set_saturation(int sat_val);
int vpp_pq_mgr_set_saturation_post(int sat_val);
int vpp_pq_mgr_set_hue(int hue_val);
int vpp_pq_mgr_set_hue_post(int hue_val);
int vpp_pq_mgr_set_sharpness(int val);
int vpp_pq_mgr_set_whitebalance(struct vpp_white_balance_s *pdata);
int vpp_pq_mgr_set_pre_gamma_table(struct vpp_gamma_table_s *pdata);
int vpp_pq_mgr_set_gamma_table(struct vpp_gamma_table_s *pdata);
int vpp_pq_mgr_set_cm_curve(enum vpp_cm_curve_type_e type, int *pdata);
int vpp_pq_mgr_set_cm_offset_curve(enum vpp_cm_curve_type_e type, char *pdata);
int vpp_pq_mgr_set_matrix_param(struct vpp_mtrx_info_s *pdata);
int vpp_pq_mgr_set_dnlp_param(struct vpp_dnlp_curve_param_s *pdata);
int vpp_pq_mgr_set_lc_curve(struct vpp_lc_curve_s *pdata);
int vpp_pq_mgr_set_lc_param(struct vpp_lc_param_s *pdata);
int vpp_pq_mgr_set_module_status(enum vpp_module_e module, bool enable);
int vpp_pq_mgr_set_pc_mode(int val);
int vpp_pq_mgr_set_color_primary_status(int val);
int vpp_pq_mgr_set_color_primary(struct vpp_color_primary_s *pdata);
int vpp_pq_mgr_set_csc_type(int val);
int vpp_pq_mgr_load_3dlut_data(struct vpp_lut3d_path_s *pdata);
int vpp_pq_mgr_set_3dlut_data(struct vpp_lut3d_table_s *ptable);
int vpp_pq_mgr_set_hdr_cgain_curve(struct vpp_hdr_lut_s *pdata);
int vpp_pq_mgr_set_hdr_oetf_curve(struct vpp_hdr_lut_s *pdata);
int vpp_pq_mgr_set_hdr_eotf_curve(struct vpp_hdr_lut_s *pdata);
int vpp_pq_mgr_set_hdr_tmo_curve(struct vpp_hdr_lut_s *pdata);
int vpp_pq_mgr_set_hdr_tmo_param(struct vpp_tmo_param_s *pdata);
int vpp_pq_mgr_set_cabc_param(struct vpp_cabc_param_s *pdata);
int vpp_pq_mgr_set_aad_param(struct vpp_aad_param_s *pdata);
int vpp_pq_mgr_set_eye_protect(struct vpp_eye_protect_s *pdata);
int vpp_pq_mgr_set_aipq_offset_table(char *pdata_str,
	unsigned int height, unsigned int width);
int vpp_pq_mgr_set_overscan_table(unsigned int length,
	struct vpp_overscan_table_s *load_table);
void vpp_pq_mgr_set_lc_isr(void);

void vpp_pq_mgr_get_status(struct vpp_pq_state_s *pstatus);
void vpp_pq_mgr_get_brightness(int *pval);
void vpp_pq_mgr_get_brightness_post(int *pval);
void vpp_pq_mgr_get_contrast(int *pval);
void vpp_pq_mgr_get_contrast_post(int *pval);
void vpp_pq_mgr_get_saturation(int *pval);
void vpp_pq_mgr_get_saturation_post(int *pval);
void vpp_pq_mgr_get_hue(int *pval);
void vpp_pq_mgr_get_hue_post(int *pval);
void vpp_pq_mgr_get_sharpness(int *pval);
void vpp_pq_mgr_get_whitebalance(struct vpp_white_balance_s *pdata);
struct vpp_gamma_table_s *vpp_pq_mgr_get_pre_gamma_table(void);
struct vpp_gamma_table_s *vpp_pq_mgr_get_gamma_table(void);
void vpp_pq_mgr_get_matrix_param(struct vpp_mtrx_info_s *pdata);
void vpp_pq_mgr_get_dnlp_param(struct vpp_dnlp_curve_param_s *pdata);
void vpp_pq_mgr_get_module_status(enum vpp_module_e module, bool *penable);
void vpp_pq_mgr_get_hdr_tmo_param(struct vpp_tmo_param_s *pdata);
void vpp_pq_mgr_get_hdr_metadata(struct vpp_hdr_metadata_s *pdata);
void vpp_pq_mgr_get_hdr_histogram(struct vpp_hdr_histgm_param_s *pdata);
void vpp_pq_mgr_get_histogram_ave(struct vpp_histgm_ave_s *pdata);
void vpp_pq_mgr_get_histogram(struct vpp_histgm_param_s *pdata);
int vpp_pq_mgr_get_pc_mode(void);
enum vpp_csc_type_e vpp_pq_mgr_get_csc_type(void);
enum vpp_hdr_type_e vpp_pq_mgr_get_hdr_type(void);
enum vpp_color_primary_e vpp_pq_mgr_get_color_primary(void);
int vpp_pq_mgr_get_pre_gamma_table_len(void);
int vpp_pq_mgr_get_gamma_table_len(void);

struct vpp_pq_mgr_settings *vpp_pq_mgr_get_settings(void);

#endif

