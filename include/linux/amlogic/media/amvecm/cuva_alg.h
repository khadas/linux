/* SPDX-License-Identifier: (GPL-2.0+ OR MIT)*/
/*
 * include/linux/amlogic/media/amvecm/cuva_alg.h
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef CUVA_ALG_H
#define CUVA_ALG_H

/*cuva hdr metadata(dynamic) parse, default windows num = 1*/
struct cuva_hdr_dynamic_metadata_s {
	int system_start_code;
	int min_maxrgb_pq;
	int avg_maxrgb_pq;
	int var_maxrgb_pq;
	int max_maxrgb_pq;
	int tm_enable_mode_flag;
	int tm_param_en_num;
	int tgt_system_dsp_max_luma_pq[2];
	int base_en_flag[2];
	/*if (base_en_flag)*/
	int base_param_m_p[2];
	int base_param_m_m[2];
	int base_param_m_a[2];
	int base_param_m_b[2];
	int base_param_m_n[2];
	int base_param_m_k1[2];
	int base_param_m_k2[2];
	int base_param_m_k3[2];
	int base_param_delta_en_mode[2];
	int base_param_en_delta[2];
	int spline_en_flag[2];
	int spline_en_num[2];
	int spline_th_en_mode[2][2];
	int spline_th_en_mb[2][2];
	int spline_th_enable[2][2];
	int spline_th_en_delta1[2][2];
	int spline_th_en_delta2[2][2];
	int spline_en_strength[2][2];
	int color_sat_mapping_flag;
	int color_sat_num;
	int clor_sat_gain[8];
};

struct aml_gain_reg {
	s64 *ogain_lut;
	s64 *cgain_lut;
};

struct aml_dm_cuva_reg_sw {
	int minimum_maxrgb; //12bit >> 12bit
	int average_maxrgb; //12bit >> 12bit
	int variance_maxrgb; //12bit >> 12bit
	int maximum_maxrgb; //12bit >> 12bit
	int tone_mapping_mode_flag; //2bit
	int tone_mapping_param_num; //2bit
	int targeted_system_display_maximum_luminance; //12bit >> 12bi
	int base_flag; //1bit 0-1
	int m_p; // 10bit 7bit = 1 0-10
	int m_m; // 6bit 0-6.3
	int m_a; // 10bit
	int m_b; // 14bit = 1 0-0.25
	int m_n; // 6bit 0-6.3
	int k1; // 2bit
	int k2; // 2bit
	int k3; // 16bit  12bit = 1 0-31
	int base_param_delta_mode; // 3bit
	int base_param_delta; // 8bit 7bit = 1 -1~1
	int spline3_flag; // 1bit 0-1
	int spline3_num; // 3bit 0-1
	int spline3_th_mode[2]; // 4bit
	int spline3_th_mb[2]; // 13bit 12bit = 1  0-1.1
	int spline3_th_base_offset[2]; //12bit = 1
	int spline3_th[2][3]; // 12bit = 1
	int spline3_strength[2]; // 7bit = 1
	int color_saturation_mapping_flag; // 1bit 0-1
	int color_saturation_num; // 3bit 0-7
	int color_saturation_gain[8]; // 7bit = 1
};

struct aml_static_reg_sw {
	s64 max_display_mastering_luminance; //16bit
	s64 min_display_mastering_luminance; //16bit
	s64 max_content_light_level;
	s64 max_picture_average_light_level;
};

struct aml_cuva_reg_sw {
	int itp;
	int cuva_en;
	int maxlum_adj_min;   //12bit >> 12bit
	int maxlum_adj_a;     //10bit >> 10bit
	int maxlum_adj_b;     //10bit >> 10bit
	int base_curve_generate_0_pvh0;  //16bit >> 12bit
	int base_curve_generate_0_pvl0;  //16bit >> 12bit
	int base_curve_generate_0_tph0;  //12bit >> 12bit
	int base_curve_generate_0_tpl0;  //12bit >> 12bit
	int base_curve_generate_0_pdh1;  //12bit >> 12bit
	int base_curve_generate_0_pdl1;  //12bit >> 12bit
	int base_curve_generate_0_tph1;  //12bit >> 12bit
	int base_curve_generate_0_tpl1;  //12bit >> 12bit
	int base_curve_generate_0_n;  //8bit
	int base_curve_generate_0_m;  //8bit
	int base_curve_adj_1_n;  //8bit
	int base_curve_adj_2_n;  //8bit
	int spline1_generate_0_hlmaxh2;  //12bit >> 12bit
	int spline1_generate_0_hlmaxl2;  //12bit >> 12bit
	int spline1_generate_0_tdmaxh2;  //12bit >> 12bit
	int spline1_generate_0_tdmaxl2;  //12bit >> 12bit
	int spline1_generate_0_avmaxh3;  //12bit >> 12bit
	int spline1_generate_0_avmaxl3;  //12bit >> 12bit
	int spline1_generate_0_sdmaxh3;  //12bit >> 12bit
	int spline1_generate_0_sdmaxl3;  //12bit >> 12bit
	int spline1_generate_0_n;  //8bit
	int spline1_generate_0_m;  //8bit
	int spline1_adj_0_n1;  //8bit
	int spline1_adj_0_n2;  //8bit
	int spline3_generate_0_b;  //12bit >> 12bit
	int spline3_generate_0_c;  //12bit >> 12bit
	int spline3_generate_0_d;  //12bit >> 12bit
	int spline1_generate_sdr_0_avmaxh6;  //12bit >> 12bit
	int spline1_generate_sdr_0_avmaxl6;  //12bit >> 12bit
	int spline1_generate_sdr_0_sdmaxh6;  //12bit >> 12bit
	int spline1_generate_sdr_0_sdmaxl6;  //12bit >> 12bit
	int spline1_generate_sdr_0_n;  //8bit
	int spline3_generate_sdr_0_b;  //12bit >> 12bit
	int spline3_generate_sdr_0_c;  //12bit >> 12bit
	int spline3_generate_sdr_0_d;  //12bit >> 12bit
	int sca_adj_a; //12bit >> 12bit
	int sca_adj_b; //12bit >> 12bit
	int sca_satr; //12bit >> 12bit
	int hdrsdrth; //12bit
};

struct aml_cuva_curve_reg_sw {
	int m_p;
	int m_m;
	int m_a;
	int m_b;
	int m_n;
	int k1;
	int k2;
	int k3;
	s64 mb00;
	s64 th30;

	s64 mb01;
	s64 mb11;
	s64 ma01;
	s64 ma11;
	s64 mc01;
	s64 mc11;
	s64 md01;
	s64 md11;
	s64 th11;
	s64 th21;
	s64 th31;

	s64 mb02;
	s64 mb12;
	s64 ma02;
	s64 ma12;
	s64 mc02;
	s64 mc12;
	s64 md02;
	s64 md12;
	s64 th12;
	s64 th22;
	s64 th32;
};

enum ic_type_e {
	IC_OTHER = 0,
	IC_G12A = 1
};

struct aml_cuva_data_s {
	enum ic_type_e ic_type;
	int max_panel_e;
	int min_panel_e;
	struct aml_cuva_reg_sw *cuva_reg;
	struct aml_static_reg_sw *static_reg;
	struct aml_gain_reg *aml_vm_regs;
	struct cuva_hdr_dynamic_metadata_s *cuva_md;
	void (*cuva_hdr_alg)(struct aml_cuva_data_s *aml_cuva_data);
};

enum cuva_func_e {
	CUVA_HDR2SDR = 0,
	CUVA_HDR2HDR10,
	CUVA_HLG2SDR,
	CUVA_HLG2HLG,
	CUVA_HLG2HDR10
};

struct aml_cuva_data_s *get_cuva_data(void);
#endif
