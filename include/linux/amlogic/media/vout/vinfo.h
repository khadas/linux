/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _VINFO_H_
#define _VINFO_H_
#include <linux/types.h>
#include <uapi/linux/amlogic/vout_ioc.h>

/* the MSB is represent vmode set by vmode_init */
#define	VMODE_INIT_BIT_MASK	0x8000
#define	VMODE_MODE_BIT_MASK	0xff
#define VMODE_NULL_DISP_MAX	2

enum viu_mux_e {
	VIU_MUX_ENCL = 0,
	VIU_MUX_ENCI,
	VIU_MUX_ENCP,
	VIU_MUX_MAX,
	VIU_MUX_PROJECT,
};

enum vout_fr_adj_type_e {
	VOUT_FR_ADJ_CLK = 0,
	VOUT_FR_ADJ_HTOTAL,
	VOUT_FR_ADJ_VTOTAL,
	VOUT_FR_ADJ_COMBO, /* vtotal + htotal + clk */
	VOUT_FR_ADJ_HDMI,  /* 50<->60: htotal; 60<->59.94: clk */
	VOUT_FR_ADJ_FREERUN,  /* freerun, panel fix frame rate*/
	VOUT_FR_ADJ_NONE,  /* disable fr_adj */
	VOUT_FR_ADJ_MAX,
};

enum vinfo_3d_e {
	NON_3D = 0,
	SS_3D,
	FP_3D,
	TB_3D,
};

/*emp : extended metadata type*/
#define VENDOR_SPECIFIC_EM_DATA 0x0
#define COMPRESS_VIDEO_TRAMSPORT 0x1
#define HDR_DYNAMIC_METADATA 0x2
#define VIDEO_TIMING_EXTENDED 0x3

#define SUPPORT_2020	0x01

/* master_display_info for display device */
struct master_display_info_s {
	u32 present_flag;
	u32 features;			/* feature bits bt2020/2084 */
	u32 primaries[3][2];		/* normalized 50000 in G,B,R order */
	u32 white_point[2];		/* normalized 50000 */
	u32 luminance[2];		/* max/min lumin, normalized 10000 */
	u32 max_content;		/* Maximum Content Light Level */
	u32 max_frame_average;	/* Maximum Frame-average Light Level */
};

/*
 *hdr_dynamic_type
 * 0x0001: type_1_hdr_metadata_version
 * 0x0002: ts_103_433_spec_version
 * 0x0003: itu_t_h265_spec_version
 * 0x0004: type_4_hdr_metadata_version
 */
struct hdr_dynamic {
	unsigned int type;
	unsigned char support_flags;
	unsigned int of_len;   /*optional_fields length*/
	unsigned char optional_fields[28];
};

struct hdr10_plus_info {
	u32 ieeeoui;
	u8 length;
	u8 application_version;
};

struct cuva_info {
	u32 cuva_support;
	u8 rawdata[15];
	u8 length;
	u32 ieeeoui;
	u8 system_start_code;
	u8 version_code;
	u32 display_max_lum;
	u16 display_min_lum;
	u8 monitor_mode_sup;
	u8 rx_mode_sup;
};

struct sbtm_info {
	unsigned char sbtm_support: 1;
	unsigned char max_sbtm_ver: 4;
	unsigned char grdm_support: 2;
	unsigned char drdm_ind: 1;
	unsigned char hgig_cat_drdm_sel: 3;
	unsigned char: 1;
	unsigned char use_hgig_drdm: 1;
	unsigned char maxrgb: 1;
	unsigned char gamut: 2;
	unsigned short red_x;
	unsigned short red_y;
	unsigned short green_x;
	unsigned short green_y;
	unsigned short blue_x;
	unsigned short blue_y;
	unsigned short white_x;
	unsigned short white_y;
	unsigned char min_bright_10;
	unsigned char peak_bright_100;
	unsigned char p0_exp: 2;
	unsigned char p0_mant: 6;
	unsigned char peak_bright_p0;
	unsigned char p1_exp: 2;
	unsigned char p1_mant: 6;
	unsigned char peak_bright_p1;
	unsigned char p2_exp: 2;
	unsigned char p2_mant: 6;
	unsigned char peak_bright_p2;
	unsigned char p3_exp: 2;
	unsigned char p3_mant: 6;
	unsigned char peak_bright_p3;
};

struct hdr_info {
/* RX EDID hdr support types */
	/* hdr_support: bit0/SDR bit1/HDR bit2/SMPTE2084 bit3/HLG */
	u32 hdr_support;
	unsigned char static_metadata_type1;
	unsigned char rawdata[7];
/*
 *dynamic_info[0] expresses type1's parameters certainly
 *dynamic_info[1] expresses type2's parameters certainly
 *dynamic_info[2] expresses type3's parameters certainly
 *dynamic_info[3] expresses type4's parameters certainly
 *if some types don't exist, the corresponding dynamic_info
 *is zero instead of inexistence
 */
	struct hdr_dynamic dynamic_info[4];
	struct hdr10_plus_info hdr10plus_info;
/*bit7:BT2020RGB    bit6:BT2020YCC bit5:BT2020cYCC bit4:adobeRGB*/
/*bit3:adobeYCC601 bit2:sYCC601     bit1:xvYCC709    bit0:xvYCC601*/
	u8 colorimetry_support; /* RX EDID colorimetry support types */
	u32 lumi_max; /* RX EDID Lumi Max value */
	u32 lumi_avg; /* RX EDID Lumi Avg value */
	u32 lumi_min; /* RX EDID Lumi Min value */
	struct cuva_info cuva_info;
	struct sbtm_info sbtm_info;

	u8 ldim_support;
	u32 lumi_peak;
};

struct hdr10plus_para {
	u8 application_version;
	u8 targeted_max_lum;
	u8 average_maxrgb;
	u8 distribution_values[9];
	u8 num_bezier_curve_anchors;
	u32 knee_point_x;
	u32 knee_point_y;
	u8 bezier_curve_anchors[9];
	u8 graphics_overlay_flag;
	u8 no_delay_flag;
};

enum eotf_type {
	EOTF_T_NULL = 0,
	EOTF_T_DOLBYVISION,
	EOTF_T_HDR10,
	EOTF_T_SDR,
	EOTF_T_LL_MODE,
	EOTF_T_DV_AHEAD,
	EOTF_T_MAX,
};

enum mode_type {
	YUV422_BIT12 = 0,
	RGB_8BIT,
	RGB_10_12BIT,
	YUV444_10_12BIT,
};

#define DV_IEEE_OUI	0x00D046
#define HDR10_PLUS_IEEE_OUI	0x90848B
#define HDR10_PLUS_DISABLE_VSIF 0
#define HDR10_PLUS_ENABLE_VSIF  1
#define HDR10_PLUS_ZERO_VSIF    2

/* Dolby Version VSIF  parameter*/
struct dv_vsif_para {
	u8 ver; /* 0 or 1 or 2*/
	u8 length;/*ver1: 15 or 12*/
	u8 ver2_l11_flag;
	union {
		struct {
			u8 low_latency:1;
			u8 dobly_vision_signal:4;
			u8 backlt_ctrl_MD_present:1;
			u8 auxiliary_MD_present:1;
			u8 eff_tmax_PQ_hi;
			u8 eff_tmax_PQ_low;
			u8 auxiliary_runmode;
			u8 auxiliary_runversion;
			u8 auxiliary_debug0;
			u8 src_dm_version:3;
		} ver2;
		struct {
			u8 low_latency:1;
			u8 dobly_vision_signal:4;
			u8 backlt_ctrl_MD_present:1;
			u8 auxiliary_MD_present:1;
			u8 eff_tmax_PQ_hi;
			u8 eff_tmax_PQ_low;
			u8 auxiliary_runmode;
			u8 auxiliary_runversion;
			u8 auxiliary_debug0;
			u8 src_dm_version:3;
			u8 content_type;
			u8 content_sub_type;
			u8 crf;
			u8 intended_white_point;
			u8 l11_byte2;
			u8 l11_byte3;
		} ver2_l11;
	} vers;
};

struct cuva_hdr_vsif_para {
	u8 system_start_code;
	u8 version_code;
	u8 monitor_mode_en;
	u8 transfer_character;
};

struct cuva_hdr_vs_emds_para {
	u8 system_start_code;
	u8 version_code;
	u16 min_maxrgb_pq;
	u16 avg_maxrgb_pq;
	u16 var_maxrgb_pq;
	u16 max_maxrgb_pq;
	u16 targeted_max_lum_pq;
	u8 transfer_character;
	u8 base_enable_flag;
	u16 base_param_m_p;
	u16 base_param_m_m;
	u16 base_param_m_a;
	u16 base_param_m_b;
	u16 base_param_m_n;
	u8 base_param_k[3];
	u8 base_param_delta_enable_mode;
	u8 base_param_enable_delta;
	u8 _3spline_enable_num;
	u8 _3spline_enable_flag;
	struct {
		u8 th_enable_mode;
		u8 th_enable_mb;
		u16 th_enable;
		u16 th_enable_delta[2];
		u8 enable_strength;
	} _3spline_data[2];
	u8 color_saturation_num;
	u8 color_saturation_gain[8];
	u8 graphic_src_display_value;
	u16 max_display_mastering_lum;
};

struct vsif_debug_save {
	enum eotf_type type;
	enum mode_type tunnel_mode;
	struct dv_vsif_para data;
	bool signal_sdr;
};

struct emp_debug_save {
	unsigned char data[128];
	unsigned int type;
	unsigned int size;
};

/* Dolby Version support information from EDID*/
/* Refer to DV Spec version2.9 page26 to page39*/
enum block_type {
	ERROR_NULL = 0,
	ERROR_LENGTH,
	ERROR_OUI,
	ERROR_VER,
	CORRECT,
};

struct dv_info {
	unsigned char rawdata[27];
	enum block_type block_flag;
	u32 ieeeoui;
	u8 ver; /* 0 or 1 or 2*/
	u8 length;/*ver1: 15 or 12*/

	u8 sup_yuv422_12bit:1;
	/* if as 0, then support RGB tunnel mode */
	u8 sup_2160p60hz:1;
	/* if equals 0, then don't support 1080p100/120hz */
	u8 sup_1080p120hz:1;
	/* if as 0, then support 2160p30hz */
	u8 sup_global_dimming:1;
	u8 dv_emp_cap:1;
	u16 Rx;
	u16 Ry;
	u16 Gx;
	u16 Gy;
	u16 Bx;
	u16 By;
	u16 Wx;
	u16 Wy;
	u16 tminPQ;
	u16 tmaxPQ;
	u8 dm_major_ver;
	u8 dm_minor_ver;
	u8 dm_version;
	u8 tmax_lum;
	u8 colorimetry:1;/* ver1*/
	u8 tmin_lum;
	u8 low_latency;/* ver1_12 and 2*/
	u8 sup_backlight_control:1;/*only ver2*/
	u8 backlt_min_luma;/*only ver2*/
	u8 Interface;/*only ver2*/
	u8 parity:1;/*only ver2*/
	u8 sup_10b_12b_444;/*only ver2*/
};

struct vout_device_s {
	const struct dv_info *dv_info;
	void (*fresh_tx_hdr_pkt)(struct master_display_info_s *data);
	void (*fresh_tx_vsif_pkt)(enum eotf_type type,
				  enum mode_type tunnel_mode,
				  struct dv_vsif_para *data,
				  bool signal_sdr);
	void (*fresh_tx_hdr10plus_pkt)(unsigned int flag,
				       struct hdr10plus_para *data);
	void (*fresh_tx_cuva_hdr_vsif)(struct cuva_hdr_vsif_para *data);
	void (*fresh_tx_cuva_hdr_vs_emds)(struct cuva_hdr_vs_emds_para *data);
	void (*fresh_tx_emp_pkt)(unsigned char *data, unsigned int type,
				 unsigned int size);
	void (*get_attr)(char attr[16]);
	void (*setup_attr)(const char *buf);
	void (*video_mute)(unsigned int flag);
};

#define LATENCY_INVALID_UNKNOWN	0
#define LATENCY_NOT_SUPPORT		0xffff
struct rx_av_latency {
	unsigned int vLatency;
	unsigned int aLatency;
	unsigned int i_vLatency;
	unsigned int i_aLatency;
};

struct vinfo_s {
	char *name;
	enum vmode_e mode;
	char ext_name[32];
	u32 frac;
	u32 width;
	u32 height;
	u32 field_height;
	u32 aspect_ratio_num;
	u32 aspect_ratio_den;
	u32 screen_real_width;
	u32 screen_real_height;
	u32 sync_duration_num;
	u32 sync_duration_den;
	u32 std_duration;
	u32 vfreq_max;
	u32 vfreq_min;
	u32 video_clk;
	u32 htotal;
	u32 vtotal;
	u32 hsw;
	u32 hbp;
	u32 hfp;
	u32 vsw;
	u32 vbp;
	u32 vfp;
	unsigned char hdmichecksum[10];
	enum vinfo_3d_e info_3d;
	enum vout_fr_adj_type_e fr_adj_type;
	enum color_fmt_e viu_color_fmt;
	/* viu_mux
	 * bit[3:0]: venc_sel, such as encp, enci, encl;
	 * bit[7:4]: venc_index, such as 0, 1, 2
	 */
	unsigned int viu_mux;
	struct master_display_info_s master_display_info;
	struct hdr_info hdr_info;
	struct rx_av_latency rx_latency;
	struct vout_device_s *vout_device;
	/* new parameters for s5 or later
	 * if current output is FRL or DSC mode,
	 * then there may use 2 or 4 slices pixel per clock.
	 * the default value is 0 or 1.
	 */
	u8 cur_enc_ppc;
	/* 0: yuv, 1: rgb */
	u8 vpp_post_out_color_fmt;
};

#ifdef CONFIG_AMLOGIC_MEDIA_FB
struct disp_rect_s {
	int x;
	int y;
	int w;
	int h;
};
#endif
#endif /* _VINFO_H_ */
