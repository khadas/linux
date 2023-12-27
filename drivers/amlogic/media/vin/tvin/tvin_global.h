/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/vin/tvin/tvin_global.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __TVIN_GLOBAL_H
#define __TVIN_GLOBAL_H

/* #include <mach/io.h> */
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
/* #include <mach/am_regs.h> */
/*#include <linux/amlogic/iomap.h>*/
#include <linux/amlogic/media/registers/register_map.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/vfm/vframe.h>

#ifdef TVBUS_REG_ADDR
#define R_APB_REG(reg) aml_read_reg32(TVBUS_REG_ADDR(reg))
#define W_APB_REG(reg, val) aml_write_reg32(TVBUS_REG_ADDR(reg), val)
#define R_VBI_APB_REG(reg) aml_read_reg32(TVBUS_REG_ADDR(reg))
#define W_VBI_APB_REG(reg, val) aml_write_reg32(TVBUS_REG_ADDR(reg), val)
#define R_APB_BIT(reg, start, len) \
	aml_get_reg32_bits(TVBUS_REG_ADDR(reg), start, len)
#define W_APB_BIT(reg, val, start, len) \
	aml_set_reg32_bits(TVBUS_REG_ADDR(reg), val, start, len)
#define W_VBI_APB_BIT(reg, val, start, len) \
	aml_set_reg32_bits(TVBUS_REG_ADDR(reg), val, start, len)
#else

int tvafe_reg_read(unsigned int reg, unsigned int *val);
int tvafe_reg_write(unsigned int reg, unsigned int val);
int tvafe_vbi_reg_read(unsigned int reg, unsigned int *val);
int tvafe_vbi_reg_write(unsigned int reg, unsigned int val);
int tvafe_hiu_reg_read(unsigned int reg, unsigned int *val);
int tvafe_hiu_reg_write(unsigned int reg, unsigned int val);

/*
 * static void __iomem *tvafe_reg_base;
 *
 * static int tvafe_reg_read(unsigned int reg, unsigned int *val)
 * {
 * *val = readl(tvafe_reg_base + reg);
 * return 0;
 * }
 *
 * static int tvafe_reg_write(unsigned int reg, unsigned int val)
 * {
 * writel(val, (tvafe_reg_base + reg));
 * return 0;
 * }
 */

static inline u32 R_APB_REG(u32 reg)
{
	unsigned int val;

	tvafe_reg_read(reg, &val);
	return val;
}

static inline void W_APB_REG(u32 reg,
			     const u32 val)
{
	tvafe_reg_write(reg, val);
}

static inline u32 R_VBI_APB_REG(u32 reg)
{
	unsigned int val = 0;

	tvafe_vbi_reg_read(reg, &val);
	return val;
}

static inline void W_VBI_APB_REG(u32 reg,
				 const u32 val)
{
	tvafe_vbi_reg_write(reg, val);
}

static inline void W_VBI_APB_BIT(u32 reg,
				 const u32 value,
				 const u32 start,
				 const u32 len)
{
	W_VBI_APB_REG(reg, ((R_VBI_APB_REG(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline void W_APB_BIT(u32 reg,
			     const u32 value,
			     const u32 start,
			     const u32 len)
{
	W_APB_REG(reg, ((R_APB_REG(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline u32 R_APB_BIT(u32 reg,
			    const u32 start,
			    const u32 len)
{
	u32 val;

	val = ((R_APB_REG(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static inline void W_VCBUS(u32 reg, const u32 value)
{
	aml_write_vcbus(reg, value);
}

static inline u32 R_VCBUS(u32 reg)
{
	return aml_read_vcbus(reg);
}

static inline void W_VCBUS_BIT(u32 reg, const u32 value, const u32 start,
			       const u32 len)
{
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline u32 R_VCBUS_BIT(u32 reg, const u32 start, const u32 len)
{
	u32 val;

	val = ((aml_read_vcbus(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static inline u32 R_HIU_REG(u32 reg)
{
	unsigned int val;

	tvafe_hiu_reg_read(reg, &val);
	return val;
}

static inline void W_HIU_REG(u32 reg,
			     const u32 val)
{
	tvafe_hiu_reg_write(reg, val);
}

static inline void W_HIU_BIT(u32 reg,
			     const u32 value,
			     const u32 start,
			     const u32 len)
{
	W_HIU_REG(reg, ((R_HIU_REG(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline u32 R_HIU_BIT(u32 reg, const u32 start, const u32 len)
{
	u32 val;

	val = ((R_HIU_REG(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

/*
 *#define R_APB_REG(reg) READ_APB_REG(reg)
 *#define W_APB_REG(reg, val) WRITE_APB_REG(reg, val)
 *#define R_APB_BIT(reg, start, len) \
 *	READ_APB_REG_BITS(reg, start, len)
 *#define W_APB_BIT(reg, val, start, len) \
 *	WRITE_APB_REG_BITS(reg, val, start, len)
 */
#endif

static inline u32 rd(u32 offset, u32 reg)
{
	return (u32)aml_read_vcbus(reg + offset);
}

static inline void wr(u32 offset, u32 reg, const u32 val)
{
	aml_write_vcbus(reg + offset, val);
}

static inline void wr_bits(u32 offset,
			   u32 reg,
			   const u32 value,
			   const u32 start,
			   const u32 len)
{
	aml_write_vcbus(reg + offset, ((aml_read_vcbus(reg + offset) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline u32 rd_bits(u32 offset, u32 reg, const u32 start, const u32 len)
{
	u32 val;

	val = ((aml_read_vcbus(reg + offset) >> (start)) & ((1L << (len)) - 1));

	return val;
}

/* ************************************************************************* */
/* *** enum definitions ********************************************* */
/* ************************************************************************* */

#define STATUS_ANTI_SHOCKING    3
#define MINIMUM_H_CNT           1400

#define ADC_REG_NUM             112
#define CVD_PART1_REG_NUM       64
#define CVD_PART1_REG_MIN       0x00
#define CVD_PART2_REG_NUM       144
#define CVD_PART2_REG_MIN       0x70
#define CVD_PART3_REG_NUM       7 /* 0x87, 0x93, 0x94, 0x95, 0x96, 0xe6, 0xfa */
#define CVD_PART3_REG_0         0x87
#define CVD_PART3_REG_1         0x93
#define CVD_PART3_REG_2         0x94
#define CVD_PART3_REG_3         0x95
#define CVD_PART3_REG_4         0x96
#define CVD_PART3_REG_5         0xe6
#define CVD_PART3_REG_6         0xfa

/* #define ACD_REG_NUM1            0x32  //0x00-0x32 except 0x1E&0x31 */
/* #define ACD_REG_NUM2            0x39  //the sum of the part2 acd register */
#define ACD_REG_NUM            0xff/* the sum all of the acd register */
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
#define CRYSTAL_24M
/* #endif */
#ifndef CRYSTAL_24M
#define CRYSTAL_25M
#endif

#ifdef CRYSTAL_24M
#define CVD2_CHROMA_DTO_NTSC_M   0x262e8ba2
#define CVD2_CHROMA_DTO_NTSC_443 0x2f4abc24
#define CVD2_CHROMA_DTO_PAL_I    0x2f4abc24
#define CVD2_CHROMA_DTO_PAL_M    0x2623cd98
#define CVD2_CHROMA_DTO_PAL_CN   0x263566cf
#define CVD2_CHROMA_DTO_PAL_60   0x2f4abc24
#define CVD2_CHROMA_DTO_SECAM    0x2dcba328
#define CVD2_HSYNC_DTO_NTSC_M    0x24000000
#define CVD2_HSYNC_DTO_NTSC_443  0x24000000
#define CVD2_HSYNC_DTO_PAL_I     0x24000000
#define CVD2_HSYNC_DTO_PAL_M     0x24000000
#define CVD2_HSYNC_DTO_PAL_CN    0x24000000
#define CVD2_HSYNC_DTO_PAL_60    0x24000000
#define CVD2_HSYNC_DTO_SECAM     0x24000000
#define CVD2_DCRESTORE_ACCUM     0x98       /* [5:0] = 24(MHz) */
#endif

#ifdef CRYSTAL_25M
#define CVD2_CHROMA_DTO_NTSC_M   0x24a7904a
#define CVD2_CHROMA_DTO_NTSC_443 0x2d66772d
#define CVD2_CHROMA_DTO_PAL_I    0x2d66772d
#define CVD2_CHROMA_DTO_PAL_M    0x249d4040
#define CVD2_CHROMA_DTO_PAL_CN   0x24ae2541
#define CVD2_CHROMA_DTO_PAL_60   0x2d66772d
#define CVD2_CHROMA_DTO_SECAM    0x2be37de9
#define CVD2_HSYNC_DTO_NTSC_M    0x228f5c28
#define CVD2_HSYNC_DTO_NTSC_443  0x228f5c28
#define CVD2_HSYNC_DTO_PAL_I     0x228f5c28
#define CVD2_HSYNC_DTO_PAL_M     0x228f5c28
#define CVD2_HSYNC_DTO_PAL_CN    0x228f5c28
#define CVD2_HSYNC_DTO_PAL_60    0x228f5c28
#define CVD2_HSYNC_DTO_SECAM     0x228f5c28
#define CVD2_DCRESTORE_ACCUM     0x99       /* [5:0] = 25(MHz) */
#endif

#define TVAFE_SET_CVBS_PGA_EN
#ifdef TVAFE_SET_CVBS_PGA_EN
#define TVAFE_SET_CVBS_PGA_START    5
#define TVAFE_SET_CVBS_PGA_STEP     1
#define CVD2_DGAIN_MIDDLE           0x0200
#define CVD2_DGAIN_WINDOW           0x000F
#define CVD2_DGAIN_LIMITH (CVD2_DGAIN_MIDDLE + CVD2_DGAIN_WINDOW)
#define CVD2_DGAIN_LIMITL (CVD2_DGAIN_MIDDLE - CVD2_DGAIN_WINDOW)
#define CVD2_DGAIN_MAX		    0x0600
#define CVD2_DGAIN_MIN		    0x0100
#define PGA_DELTA_VAL			0x10
#endif

#define TVAFE_SET_CVBS_CDTO_EN
#ifdef TVAFE_SET_CVBS_CDTO_EN
#define TVAFE_SET_CVBS_CDTO_START   300
#define TVAFE_SET_CVBS_CDTO_STEP    0
#define HS_CNT_STANDARD             0x31380	/*0x17a00*/
#define CDTO_FILTER_FACTOR			1
#endif

enum tvin_color_space_e {
	TVIN_CS_RGB444 = 0,
	TVIN_CS_YUV444,
	TVIN_CS_YUV422_16BITS,
	TVIN_CS_YCBCR422_8BITS,
	TVIN_CS_MAX
};

/*vdin buffer control for tvin frontend*/
enum tvin_buffer_ctl_e {
	TVIN_BUF_NULL,
	TVIN_BUF_SKIP,
	TVIN_BUF_TMP,
	TVIN_BUF_RECYCLE_TMP,
};

/* ************************************************************************* */
/* *** structure definitions ********************************************* */
/* ************************************************************************* */
/* Hs_cnt        Pixel_Clk(Khz/10) */

enum tvin_ar_b3_b0_val_e {
	TVIN_AR_14x9_LB_CENTER_VAL = 0x1,
	TVIN_AR_14x9_LB_TOP_VAL = 0x2,
	TVIN_AR_16x9_LB_TOP_VAL = 0x4,
	TVIN_AR_16x9_FULL_VAL = 0x7,
	TVIN_AR_4x3_FULL_VAL = 0x8,
	TVIN_AR_16x9_LB_CENTER_VAL = 0xb,
	TVIN_AR_16x9_LB_CENTER1_VAL = 0xd,
	TVIN_AR_14x9_FULL_VAL = 0xe,
	TVIN_AR_NOT_VALUE = 0xf,
};
const char *tvin_aspect_ratio_str(enum tvin_aspect_ratio_e aspect_ratio);

enum tvin_hdr_eotf_e {
	EOTF_SDR,
	EOTF_HDR,
	EOTF_SMPTE_ST_2048,
	EOTF_HLG,
	EOTF_MAX,
};

enum tvin_hdr_state_e {
	HDR_STATE_NULL,
	HDR_STATE_GET,
	HDR_STATE_SET,
};

struct tvin_hdr_property_s {
	unsigned int x;/* max */
	unsigned int y;/* min */
};

struct tvin_hdr_data_s {
	enum tvin_hdr_eotf_e eotf:8;
	unsigned char metadata_id;
	unsigned char length;
	unsigned char reserved;
	struct tvin_hdr_property_s primaries[3];
	struct tvin_hdr_property_s white_points;
	struct tvin_hdr_property_s master_lum;/* max min lum */
	unsigned int mcll;
	unsigned int mfall;
	u8 rawdata[32];
};

struct tvin_hdr_info_s {
	struct tvin_hdr_data_s hdr_data;
	enum tvin_hdr_state_e hdr_state;
	unsigned int hdr_check_cnt;
};

struct tvin_dv_vsif_raw_s {
	u8 pkttype;
	u8 version;
	u8 length;
	u8 PB[29];
};

struct tvin_emp_data_s {
	u8 size; //dv is pkt_cnt
	u8 empbuf[1024];
	u8 tag_id;
};

/* refer to hdmi_rx_drv.h */
struct tvin_vtem_data_s {
	u8 vrr_en;
	u8 m_const;
	u8 qms_en;
	u8 fva_factor_m1;
	u8 base_v_front;
	u8 rb;
	u16 base_framerate;
};

struct tvin_cuva_emds_data_s {
	/* Sequence_Index:0 */
	u8 pkt_type_0;
	u8 hb1_0;
	u8 sequence_index_0;
	u8 pb0;
	u16 organization_id;
	u16 data_tag;
	u16 data_length;
	u32 ieee:24;
	u32 system_start_code:8;
	u16 min_maxrgb_pq;
	u16 avg_maxrgb_pq;
	u16 var_maxrgb_pq;
	u16 max_maxrgb_pq;
	u16 targeted_max_lum_pq;
	u16 base_param_m_p;
	u8 base_param_m_m;
	u16 base_param_m_a;
	u16 base_param_m_b;
	/* Sequence_Index:1 */
	u8 pkt_type_1;
	u8 hb1_1;
	u8 sequence_index_1;
	u8 base_param_m_n;
	u8 base_param_k;
	u8 base_param_delta_enable_mode;
	u8 base_param_enable_delta;
	struct {
		u8 th_enable_mode;
		u8 th_enable_mb;
		u16 th_enable;
		u16 th_enable_delta[2];
		u8 enable_strength;
	} _3spline_data[2];
	u8 color_saturation_num;
	u8 color_saturation_gain0[5];
	/* Sequence_Index:2 */
	u8 pkt_type_2;
	u8 hb1_2;
	u8 sequence_index_2;
	u8 color_saturation_gain1[3];
	u8 graphic_src_display_value;
	u8 rvd;
	u16 max_display_mastering_lum;
	u8 rvd1[21];
};

struct tvin_sbtm_data_s {
	u8 sbtm_ver;
	u8 sbtm_mode;
	u8 sbtm_type;
	u8 grdm_min;
	u8 grdm_lum;
	u16 frm_pb_limit_int;
};

struct tvin_spd_data_s {
	u8 pkttype;
	u8 version;
	u8 length;
	u8 checksum;
	//u8 ieee_oui[3]; //data[0:2]
	//data[5]:bit2 bit3 is freesync type,1:VDIN_VRR_FREESYNC
	//2:VDIN_VRR_FREESYNC_PREMIUM 3:VDIN_VRR_FREESYNC_PREMIUM_PRO
	u8 data[28];
};

struct tvin_hdr10plus_info_s {
	bool hdr10p_on;
	struct tvin_hdr10p_data_s hdr10p_data;
};

struct tvin_3d_meta_data_s {
	bool meta_data_flag;
	u8 meta_data_type;
	u8 meta_data_length;
	u8 meta_data[20];
};

struct tvin_cuva_data_s {
	u8 vsif_type;		//hb0
	u8 visf_version;	//hb1
	u8 payload_length;	//hb2
	u8 check_sum;		//pb0
	u32 ieee:24;		//pb1-3
	u8 sys_start_code;	//pb4
	/* pb5 */
	u8 rsvd:2;
	u8 transfer_char:1;
	u8 monitor_mode_enable:1;
	u8 version_code:4;
	/* pb6-pb27 */
	u8 reserved[22];
};

struct tvin_cuva_vsif_s {
	bool cuva_on;
	struct tvin_cuva_data_s cuva_data;
};

struct tvin_fmm_s {
	bool fmm_flag;
	bool fmm_vsif_flag;
	bool it_content;
	enum tvin_cn_type_e cn_type;
	struct tvin_fmm_data_s fmm_data;
};

struct tvin_sig_property_s {
	enum tvin_trans_fmt	trans_fmt;
	enum tvin_color_fmt_e	color_format;
	/* for vdin matrix destination color fmt */
	enum tvin_color_fmt_e	dest_cfmt;
	enum tvin_aspect_ratio_e	aspect_ratio;
	unsigned int		dvi_info;
	unsigned short		scaling4h;	/* for vscaler */
	unsigned short		scaling4w;	/* for hscaler */
	unsigned int		hs;	/* for horizontal start cut window */
	unsigned int		he;	/* for horizontal end cut window */
	unsigned int		vs;	/* for vertical start cut window */
	unsigned int		ve;	/* for vertical end cut window */
	unsigned int		pre_vs;	/* for vertical start cut window */
	unsigned int		pre_ve;	/* for vertical end cut window */
	unsigned int		pre_hs;	/* for horizontal start cut window */
	unsigned int		pre_he;	/* for horizontal end cut window */
	unsigned int		decimation_ratio;	/* for decimation */
	unsigned int		colordepth; /* for color bit depth */
	unsigned int		vdin_hdr_flag;
	unsigned int            vdin_vrr_flag;
	enum tvin_color_fmt_range_e color_fmt_range;
	struct tvin_hdr_info_s hdr_info;
	struct tvin_dv_vsif_s dv_vsif;/*dolby vsi info*/
	struct tvin_cuva_vsif_s cuva_info; /* cuva hdr info */
	struct tvin_dv_vsif_raw_s dv_vsif_raw;
	u8 dolby_vision;/*is signal dolby version 1:vsif 2:emp */
	bool low_latency;/*is low latency dolby mode*/
	u8 fps;
	unsigned int skip_vf_num;/*skip pre vframe num*/
	struct tvin_latency_s latency;
	struct tvin_fmm_s filmmaker;
	struct tvin_hdr10plus_info_s hdr10p_info;
	struct tvin_3d_meta_data_s threed_info;
	struct tvin_emp_data_s emp_data;
	struct tvin_vtem_data_s vtem_data;
	struct tvin_sbtm_data_s sbtm_data;
	struct tvin_cuva_emds_data_s cuva_emds_data;
	struct tvin_spd_data_s spd_data;
	unsigned int cnt;
	unsigned int hw_vic;
	unsigned int avi_ec;//hdmi avi ext_colorimetry
	/* only use for loopback, 0=positvie, 1=negative */
	unsigned int polarity_vs;
	unsigned int hdcp_sts;	/* protected content src. 1:protected 0:not*/
};

#define TVAFE_VF_POOL_SIZE		6 /* 8 */
#define VDIN_VF_POOL_MAX_SIZE		6 /* 8 */
#define TVHDMI_VF_POOL_SIZE		6 /* 8 */

#define BT656IN_ANCI_DATA_SIZE		0x4000 /* save anci data from bt656in */
#define CAMERA_IN_ANCI_DATA_SIZE	0x4000 /* save anci data from bt656in */

#endif /* __TVIN_GLOBAL_H */

