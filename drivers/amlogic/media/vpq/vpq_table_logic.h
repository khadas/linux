/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_TABLE_LOGIC_H__
#define __VPQ_TABLE_LOGIC_H__

#include <linux/types.h>
#include "vpq_common.h"

extern struct TABLE_PQ_MODULE_CFG pq_module_cfg;
extern struct PQ_TABLE_PARAM pq_table_param;
extern struct pq_tcon_gamma_table_s gamma_table;
extern struct pq_ctemp_gamma_table_s ctemp_table;

enum vpq_resolution_e {
	VPQ_SD_576_480 = 0,
	VPQ_SD_720_576,
	VPQ_HD_1280_720,
	VPQ_FHD_1920_1080,
	VPQ_4K_3840_2160,
	VPQ_4K_7680_4320,
	VPQ_RESO_MAX,
};

enum vpq_overscan_timing_e {
	VPQ_TIMING_SD_480 = 0,
	VPQ_TIMING_SD_576,
	VPQ_TIMING_HD,
	VPQ_TIMING_FHD,
	VPQ_TIMING_UHD,
	VPQ_TIMING_NTST_M,
	VPQ_TIMING_NTST_443,
	VPQ_TIMING_PAL_I,
	VPQ_TIMING_PAL_M,
	VPQ_TIMING_PAL_60,
	VPQ_TIMING_PAL_CN,
	VPQ_TIMING_SECAM,
	VPQ_TIMING_NTSC_50,
	VPQ_TIMING_MAX,
};

struct vpq_overscan_data_s {
	unsigned int src_timing;
	unsigned int value1;
	unsigned int value2;
	unsigned int reserved1;
	unsigned int reserved2;
};

enum vpq_module_e {
	VPQ_MODULE_VADJ1 = 0,
	VPQ_MODULE_VADJ2,
	VPQ_MODULE_PREGAMMA,
	VPQ_MODULE_GAMMA,
	VPQ_MODULE_WB,
	VPQ_MODULE_DNLP,
	VPQ_MODULE_CCORING,
	VPQ_MODULE_SR0,
	VPQ_MODULE_SR1,
	VPQ_MODULE_LC,
	VPQ_MODULE_CM,
	VPQ_MODULE_BLE,
	VPQ_MODULE_BLS,
	VPQ_MODULE_LUT3D,
	VPQ_MODULE_DEJAGGY_SR0,
	VPQ_MODULE_DEJAGGY_SR1,
	VPQ_MODULE_DERING_SR0,
	VPQ_MODULE_DERING_SR1,
	VPQ_MODULE_ALL,
};

void vpq_table_init(void);
void vpq_table_deinit(void);
int vpq_init_default_pqtable(struct vpq_pqtable_bin_param_s *pdata);
int vpq_set_pq_module_cfg(void);
void vpq_get_pq_module_status(struct vpq_pq_state_s *pdata);
int vpq_set_brightness(int value);
int vpq_set_contrast(int value);
int vpq_set_saturation(int value);
int vpq_set_hue(int value);
int vpq_set_sharpness(int value);
int vpq_set_brightness_post(int value);
int vpq_set_contrast_post(int value);
int vpq_set_saturation_post(int value);
int vpq_set_hue_post(int value);
int vpq_set_pq_module_status(enum vpq_module_e module, bool status);
int vpq_set_gamma_index(int value);
int vpq_set_gamma_table(struct vpq_gamma_table_s *pdata);
int vpq_set_blend_gamma(struct vpq_blend_gamma_s *pdata);
int vpq_set_pre_gamma_table(struct vpq_pre_gamma_table_s *pdata);
int vpq_set_rgb_ogo(struct vpq_rgb_ogo_s *pdata);
int vpq_set_color_customize(struct vpq_cms_s *pdata);
int vpq_set_dnlp_mode(int value);
int vpq_set_csc_type(int value);
int vpq_get_csc_type(void);
int vpq_get_hist_avg(struct vpq_histgm_ave_s *pdata);
int vpq_get_histogram(struct vpq_histgm_param_s *pdata);
int vpq_set_lc_mode(int value);
int vpq_set_hdr_tmo_mode(int value);
int vpq_set_hdr_tmo_data(struct vpq_hdr_lut_s *pdata);
int vpq_set_aipq_mode(int value);
int vpq_set_aisr_mode(int value);
enum vpq_color_primary_e vpq_get_color_primary(void);
int vpq_get_hdr_metadata(struct vpq_hdr_metadata_s *pdata);
int vpq_set_black_stretch(int value);
int vpq_set_blue_stretch(int value);
int vpq_set_chroma_coring(int value);
int vpq_set_xvycc_mode(int value);
int vpq_set_nr(int value);
int vpq_set_deblock(int value);
int vpq_set_demosquito(int value);
int vpq_set_mcdi_mode(int value);
int vpq_set_smoothplus_mode(int value);
int vpq_set_color_base(int value);
int vpq_set_cabc(void);
int vpq_set_add(void);
int vpq_set_overscan_data(unsigned int length, struct vpq_overscan_data_s *pdata);
int vpq_set_pll_value(void);
int vpq_set_pc_mode(int value);
void vpq_set_pq_effect_all(void);
int vpq_set_frame_status(enum vpq_frame_status_e status);
int vpq_get_event_info(void);
void vpq_get_signal_info(struct vpq_signal_info_s *pdata);
int vpq_dump_pq_table(enum vpq_dump_type_e value);
#endif //__VPQ_TABLE_LOGIC_H__
