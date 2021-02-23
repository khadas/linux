/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_COMMON_H__
#define __HDMI_COMMON_H__

#include <linux/amlogic/media/vout/vinfo.h>

/* HDMI VIC definitions */

/* HDMITX_VIC420_OFFSET and HDMITX_VIC_MASK are associated with
 * VIC_MAX_VALID_MODE and VIC_MAX_NUM in hdmi_tx_module.h
 */
#define HDMITX_VIC420_OFFSET	0x100
#define HDMITX_VIC420_FAKE_OFFSET 0x200
#define HDMITX_VESA_OFFSET	0x300

#define HDMITX_VIC_MASK			0xff

/* Refer to http://standards-oui.ieee.org/oui/oui.txt */
#define HDMI_IEEEOUI		0x000C03
#define HF_IEEEOUI		0xC45DD8
#define DOVI_IEEEOUI		0x00D046
#define HDR10PLUS_IEEEOUI	0x90848B

enum hdmi_tf_type {
	HDMI_NONE = 0,
	/* HDMI_HDR_TYPE, HDMI_DV_TYPE, and HDMI_HDR10P_TYPE
	 * should be mutexed with each other
	 */
	HDMI_HDR_TYPE = 0x10,
	HDMI_HDR_SMPTE_2084	= HDMI_HDR_TYPE | 1,
	HDMI_HDR_HLG		= HDMI_HDR_TYPE | 2,
	HDMI_HDR_HDR		= HDMI_HDR_TYPE | 3,
	HDMI_HDR_SDR		= HDMI_HDR_TYPE | 4,
	HDMI_DV_TYPE = 0x20,
	HDMI_DV_VSIF_STD	= HDMI_DV_TYPE | 1,
	HDMI_DV_VSIF_LL		= HDMI_DV_TYPE | 2,
	HDMI_HDR10P_TYPE = 0x30,
	HDMI_HDR10P_DV_VSIF	= HDMI_HDR10P_TYPE | 1,
};

#define GET_OUI_BYTE0(oui)	((oui) & 0xff) /* Little Endian */
#define GET_OUI_BYTE1(oui)	(((oui) >> 8) & 0xff)
#define GET_OUI_BYTE2(oui)	(((oui) >> 16) & 0xff)

enum hdmi_vic {
	/* Refer to CEA 861-D */
	HDMI_UNKNOWN = 0,
	HDMI_1_640x480p60_4x3		= 1,
	HDMI_2_720x480p60_4x3		= 2,
	HDMI_3_720x480p60_16x9		= 3,
	HDMI_4_1280x720p60_16x9		= 4,
	HDMI_5_1920x1080i60_16x9	= 5,
	HDMI_6_720x480i60_4x3		= 6,
	HDMI_7_720x480i60_16x9		= 7,
	HDMI_8_720x240p60_4x3		= 8,
	HDMI_9_720x240p60_16x9		= 9,
	HDMI_10_2880x480i60_4x3		= 10,
	HDMI_11_2880x480i60_16x9	= 11,
	HDMI_12_2880x240p60_4x3		= 12,
	HDMI_13_2880x240p60_16x9	= 13,
	HDMI_14_1440x480p60_4x3		= 14,
	HDMI_15_1440x480p60_16x9	= 15,
	HDMI_16_1920x1080p60_16x9	= 16,
	HDMI_17_720x576p50_4x3		= 17,
	HDMI_18_720x576p50_16x9		= 18,
	HDMI_19_1280x720p50_16x9	= 19,
	HDMI_20_1920x1080i50_16x9	= 20,
	HDMI_21_720x576i50_4x3		= 21,
	HDMI_22_720x576i50_16x9		= 22,
	HDMI_23_720x288p_4x3		= 23,
	HDMI_24_720x288p_16x9		= 24,
	HDMI_25_2880x576i50_4x3		= 25,
	HDMI_26_2880x576i50_16x9	= 26,
	HDMI_27_2880x288p50_4x3		= 27,
	HDMI_28_2880x288p50_16x9	= 28,
	HDMI_29_1440x576p_4x3		= 29,
	HDMI_30_1440x576p_16x9		= 30,
	HDMI_31_1920x1080p50_16x9	= 31,
	HDMI_32_1920x1080p24_16x9	= 32,
	HDMI_33_1920x1080p25_16x9	= 33,
	HDMI_34_1920x1080p30_16x9	= 34,
	HDMI_35_2880x480p60_4x3		= 35,
	HDMI_36_2880x480p60_16x9	= 36,
	HDMI_37_2880x576p50_4x3		= 37,
	HDMI_38_2880x576p50_16x9	= 38,
	HDMI_39_1920x1080i_t1250_50_16x9 = 39,
	HDMI_40_1920x1080i100_16x9	= 40,
	HDMI_41_1280x720p100_16x9	= 41,
	HDMI_42_720x576p100_4x3		= 42,
	HDMI_43_720x576p100_16x9	= 43,
	HDMI_44_720x576i100_4x3		= 44,
	HDMI_45_720x576i100_16x9	= 45,
	HDMI_46_1920x1080i120_16x9	= 46,
	HDMI_47_1280x720p120_16x9	= 47,
	HDMI_48_720x480p120_4x3		= 48,
	HDMI_49_720x480p120_16x9	= 49,
	HDMI_50_720x480i120_4x3		= 50,
	HDMI_51_720x480i120_16x9	= 51,
	HDMI_52_720x576p200_4x3		= 52,
	HDMI_53_720x576p200_16x9	= 53,
	HDMI_54_720x576i200_4x3		= 54,
	HDMI_55_720x576i200_16x9	= 55,
	HDMI_56_720x480p240_4x3		= 56,
	HDMI_57_720x480p240_16x9	= 57,
	HDMI_58_720x480i240_4x3		= 58,
	HDMI_59_720x480i240_16x9	= 59,
	HDMI_60_1280x720p24_16x9	= 60,
	HDMI_61_1280x720p25_16x9	= 61,
	HDMI_62_1280x720p30_16x9	= 62,
	HDMI_63_1920x1080p120_16x9	= 63,
	HDMI_64_1920x1080p100_16x9	= 64,
	HDMI_65_1280x720p24_64x27	= 65,
	HDMI_66_1280x720p25_64x27	= 66,
	HDMI_67_1280x720p30_64x27	= 67,
	HDMI_68_1280x720p50_64x27	= 68,
	HDMI_69_1280x720p60_64x27	= 69,
	HDMI_70_1280x720p100_64x27	= 70,
	HDMI_71_1280x720p120_64x27	= 71,
	HDMI_72_1920x1080p24_64x27	= 72,
	HDMI_73_1920x1080p25_64x27	= 73,
	HDMI_74_1920x1080p30_64x27	= 74,
	HDMI_75_1920x1080p50_64x27	= 75,
	HDMI_76_1920x1080p60_64x27	= 76,
	HDMI_77_1920x1080p100_64x27	= 77,
	HDMI_78_1920x1080p120_64x27	= 78,
	HDMI_79_1680x720p24_64x27	= 79,
	HDMI_80_1680x720p25_64x27	= 80,
	HDMI_81_1680x720p30_64x27	= 81,
	HDMI_82_1680x720p50_64x27	= 82,
	HDMI_83_1680x720p60_64x27	= 83,
	HDMI_84_1680x720p100_64x27	= 84,
	HDMI_85_1680x720p120_64x27	= 85,
	HDMI_86_2560x1080p24_64x27	= 86,
	HDMI_87_2560x1080p25_64x27	= 87,
	HDMI_88_2560x1080p30_64x27	= 88,
	HDMI_89_2560x1080p50_64x27	= 89,
	HDMI_90_2560x1080p60_64x27	= 90,
	HDMI_91_2560x1080p100_64x27	= 91,
	HDMI_92_2560x1080p120_64x27	= 92,
	HDMI_93_3840x2160p24_16x9	= 93,
	HDMI_94_3840x2160p25_16x9	= 94,
	HDMI_95_3840x2160p30_16x9	= 95,
	HDMI_96_3840x2160p50_16x9	= 96,
	HDMI_97_3840x2160p60_16x9	= 97,
	HDMI_98_4096x2160p24_256x135	= 98,
	HDMI_99_4096x2160p25_256x135	= 99,
	HDMI_100_4096x2160p30_256x135	= 100,
	HDMI_101_4096x2160p50_256x135	= 101,
	HDMI_102_4096x2160p60_256x135	= 102,
	HDMI_103_3840x2160p24_64x27	= 103,
	HDMI_104_3840x2160p25_64x27	= 104,
	HDMI_105_3840x2160p30_64x27	= 105,
	HDMI_106_3840x2160p50_64x27	= 106,
	HDMI_107_3840x2160p60_64x27	= 107,
	HDMI_108_1280x720p48_16x9	= 108,
	HDMI_109_1280x720p48_64x27	= 109,
	HDMI_110_1680x720p48_64x27	= 110,
	HDMI_111_1920x1080p48_16x9	= 111,
	HDMI_112_1920x1080p48_64x27	= 112,
	HDMI_113_2560x1080p48_64x27	= 113,
	HDMI_114_3840x2160p48_16x9	= 114,
	HDMI_115_4096x2160p48_256x135	= 115,
	HDMI_116_3840x2160p48_64x27	= 116,
	HDMI_117_3840x2160p100_16x9	= 117,
	HDMI_118_3840x2160p120_16x9	= 118,
	HDMI_119_3840x2160p100_64x27	= 119,
	HDMI_120_3840x2160p120_64x27	= 120,
	HDMI_121_5120x2160p24_64x27	= 121,
	HDMI_122_5120x2160p25_64x27	= 122,
	HDMI_123_5120x2160p30_64x27	= 123,
	HDMI_124_5120x2160p48_64x27	= 124,
	HDMI_125_5120x2160p50_64x27	= 125,
	HDMI_126_5120x2160p60_64x27	= 126,
	HDMI_127_5120x2160p100_64x27	= 127,
	/* 127 ~ 192 reserved */
	HDMI_193_5120x2160p120_64x27	= 193,
	HDMI_194_7680x4320p24_16x9	= 194,
	HDMI_195_7680x4320p25_16x9	= 195,
	HDMI_196_7680x4320p30_16x9	= 196,
	HDMI_197_7680x4320p48_16x9	= 197,
	HDMI_198_7680x4320p50_16x9	= 198,
	HDMI_199_7680x4320p60_16x9	= 199,
	HDMI_200_7680x4320p100_16x9	= 200,
	HDMI_201_7680x4320p120_16x9	= 201,
	HDMI_202_7680x4320p24_64x27	= 202,
	HDMI_203_7680x4320p25_64x27	= 203,
	HDMI_204_7680x4320p30_64x27	= 204,
	HDMI_205_7680x4320p48_64x27	= 205,
	HDMI_206_7680x4320p50_64x27	= 206,
	HDMI_207_7680x4320p60_64x27	= 207,
	HDMI_208_7680x4320p100_64x27	= 208,
	HDMI_209_7680x4320p120_64x27	= 209,
	HDMI_210_10240x4320p24_64x27	= 210,
	HDMI_211_10240x4320p25_64x27	= 211,
	HDMI_212_10240x4320p30_64x27	= 212,
	HDMI_213_10240x4320p48_64x27	= 213,
	HDMI_214_10240x4320p50_64x27	= 214,
	HDMI_215_10240x4320p60_64x27	= 215,
	HDMI_216_10240x4320p100_64x27	= 216,
	HDMI_217_10240x4320p120_64x27	= 217,
	HDMI_218_4096x2160p100_256x135	= 218,
	HDMI_219_4096x2160p120_256x135	= 219,
	HDMI_VIC_FAKE = HDMITX_VIC420_FAKE_OFFSET,
	HDMIV_640x480p60hz = HDMITX_VESA_OFFSET,
	HDMIV_800x480p60hz,
	HDMIV_800x600p60hz,
	HDMIV_852x480p60hz,
	HDMIV_854x480p60hz,
	HDMIV_1024x600p60hz,
	HDMIV_1024x768p60hz,
	HDMIV_1152x864p75hz,
	HDMIV_1280x600p60hz,
	HDMIV_1280x768p60hz,
	HDMIV_1280x800p60hz,
	HDMIV_1280x960p60hz,
	HDMIV_1280x1024p60hz,
	HDMIV_1360x768p60hz,
	HDMIV_1366x768p60hz,
	HDMIV_1400x1050p60hz,
	HDMIV_1440x900p60hz,
	HDMIV_1440x2560p60hz,
	HDMIV_1600x900p60hz,
	HDMIV_1600x1200p60hz,
	HDMIV_1680x1050p60hz,
	HDMIV_1920x1200p60hz,
	HDMIV_2160x1200p90hz,
	HDMIV_2560x1080p60hz,
	HDMIV_2560x1440p60hz,
	HDMIV_2560x1600p60hz,
	HDMIV_3440x1440p60hz,
	HDMIV_2400x1200p90hz,
	HDMI_VIC_END,
};

enum hdmi_phy_para {
	HDMI_PHYPARA_6G = 1, /* 2160p60hz 444 8bit */
	HDMI_PHYPARA_4p5G, /* 2160p50hz 420 12bit */
	HDMI_PHYPARA_3p7G, /* 2160p30hz 444 10bit */
	HDMI_PHYPARA_3G, /* 2160p24hz 444 8bit */
	HDMI_PHYPARA_LT3G, /* 1080p60hz 444 12bit */
	HDMI_PHYPARA_DEF = HDMI_PHYPARA_LT3G,
	HDMI_PHYPARA_270M, /* 480p60hz 444 8bit */
};

enum hdmi_audio_fs;
struct dtd;

/* CEA TIMING STRUCT DEFINITION */
struct hdmi_timing {
	unsigned int vic;
	unsigned char *name;
	unsigned char *sname;
	unsigned short pi_mode; /* 1: progressive  0: interlaced */
	unsigned int h_freq; /* in Hz */
	unsigned int v_freq; /* in 0.001 Hz */
	unsigned int pixel_freq; /* Unit: 1000 */
	unsigned short h_total;
	unsigned short h_blank;
	unsigned short h_front;
	unsigned short h_sync;
	unsigned short h_back;
	unsigned short h_active;
	unsigned short v_total;
	unsigned short v_blank;
	unsigned short v_front;
	unsigned short v_sync;
	unsigned short v_back;
	unsigned short v_active;
	unsigned short v_sync_ln;

	unsigned short h_pol;
	unsigned short v_pol;
	unsigned short h_pict;
	unsigned short v_pict;
	unsigned short h_pixel;
	unsigned short v_pixel;
};

enum hdmi_color_depth {
	COLORDEPTH_24B = 4,
	COLORDEPTH_30B = 5,
	COLORDEPTH_36B = 6,
	COLORDEPTH_48B = 7,
	COLORDEPTH_RESERVED,
};

enum hdmi_color_space {
	COLORSPACE_RGB444 = 0,
	COLORSPACE_YUV422 = 1,
	COLORSPACE_YUV444 = 2,
	COLORSPACE_YUV420 = 3,
	COLORSPACE_RESERVED,
};

enum hdmi_color_range {
	COLORRANGE_LIM,
	COLORRANGE_FUL,
};

enum hdmi_3d_type {
	T3D_FRAME_PACKING = 0,
	T3D_FIELD_ALTER = 1,
	T3D_LINE_ALTER = 2,
	T3D_SBS_FULL = 3,
	T3D_L_DEPTH = 4,
	T3D_L_DEPTH_GRAPHICS = 5,
	T3D_TAB = 6, /* Top and Buttom */
	T3D_RSVD = 7,
	T3D_SBS_HALF = 8,
	T3D_DISABLE,
};

/* get hdmi cea timing */
/* t: struct hdmi_cea_timing * */
#define GET_TIMING(name)      (t->(name))

struct hdmi_format_para {
	enum hdmi_color_depth cd; /* cd8, cd10 or cd12 */
	enum hdmi_color_space cs; /* rgb, y444, y422, y420 */
	enum hdmi_color_range cr; /* limit, full */
	u32 scrambler_en:1;
	u32 tmds_clk_div40:1;
	u32 tmds_clk; /* Unit: 1000 */
	struct hdmi_timing timing;
	struct vinfo_s hdmitx_vinfo;
};

/* Refer to HDMI Spec */
/* HDMI Packet Type Definitions */
#define PKT_NULL		0x00
#define PKT_ACR			0x01
#define PKT_AUDSAMP		0x02
#define PKT_GCP			0x03
#define PKT_ACP			0x04
#define PKT_ISRC1		0x05
#define PKT_ISRC2		0x06
#define PKT_ONEBITAUDSAMP	0x07
#define PKT_DSTAUD		0x08
#define PKT_HBRAUD		0x09
#define PKT_GAMUTMETADATA	0x0A
#define PKT_3DAUDSAMP		0x0B
#define PKT_ONEBIT3DAUDSAMP	0x0C
#define PKT_AUDMETADATA		0x0D
#define PKT_MULTISTREAMAUDSAMP	0x0E
#define PKT_ONEBITMULTISTREAM	0x0F
#define PKT_EMP			0x7f
/* Infoframe Packet */
#define IF_VSIF			0x81
#define IF_AVI			0x82
#define IF_SPD			0x83
#define IF_AUD			0x84
//#define IF_MPEGSRC		0x85
//#define IF_NTSCVBI		0x86
#define IF_DRM			0x87

struct hdmi_csc_coef_table {
	u8 input_format;
	u8 output_format;
	u8 color_depth;
	u8 color_format; /* 0 for ITU601, 1 for ITU709 */
	u8 coef_length;
	const u8 *coef;
};

enum hdmi_audio_packet {
	hdmi_audio_packet_SMP = 0x02,
	hdmi_audio_packet_1BT = 0x07,
	hdmi_audio_packet_DST = 0x08,
	hdmi_audio_packet_HBR = 0x09,
};

enum hdmi_aspect_ratio {
	ASPECT_RATIO_SAME_AS_SOURCE = 0x8,
	TV_ASPECT_RATIO_4_3 = 0x9,
	TV_ASPECT_RATIO_16_9 = 0xA,
	TV_ASPECT_RATIO_14_9 = 0xB,
	TV_ASPECT_RATIO_MAX
};

struct vesa_standard_timing;

struct hdmi_format_para *hdmi21_get_fmt_paras(enum hdmi_vic vic);
struct hdmi_format_para *hdmi21_match_dtd_paras(struct dtd *t);
void check21_detail_fmt(void);
struct hdmi_format_para *hdmi21_get_fmt_name(char const *name, char const *attr);
struct hdmi_format_para *hdmi21_tst_fmt_name(char const *name, char const *attr);
struct hdmi_format_para *hdmitx21_get_fmtpara(char *mode);
u32 hdmi21_get_aud_n_paras(enum hdmi_audio_fs fs,
				  enum hdmi_color_depth cd,
				  u32 tmds_clk);
struct hdmi_format_para *hdmi21_get_vesa_paras(struct vesa_standard_timing *t);

/* HDMI Audio Parmeters */
/* Refer to CEA-861-D Page 88 */
#define DTS_HD_TYPE_MASK 0xff00
#define DTS_HD_MA  (0X1 << 8)
enum hdmi_audio_type {
	CT_REFER_TO_STREAM = 0,
	CT_PCM,
	CT_AC_3, /* DD */
	CT_MPEG1,
	CT_MP3,
	CT_MPEG2,
	CT_AAC,
	CT_DTS,
	CT_ATRAC,
	CT_ONE_BIT_AUDIO,
	CT_DOLBY_D, /* DDP or DD+ */
	CT_DTS_HD,
	CT_MAT, /* TrueHD */
	CT_DST,
	CT_WMA,
	CT_DTS_HD_MA = CT_DTS_HD + (DTS_HD_MA),
	CT_MAX,
};

enum hdmi_audio_chnnum {
	CC_REFER_TO_STREAM = 0,
	CC_2CH,
	CC_3CH,
	CC_4CH,
	CC_5CH,
	CC_6CH,
	CC_7CH,
	CC_8CH,
	CC_MAX_CH
};

enum hdmi_audio_format {
	AF_SPDIF = 0, AF_I2S, AF_DSD, AF_HBR, AT_MAX
};

enum hdmi_audio_sampsize {
	SS_REFER_TO_STREAM = 0, SS_16BITS, SS_20BITS, SS_24BITS, SS_MAX
};

struct size_map {
	u32 sample_bits;
	enum hdmi_audio_sampsize ss;
};

/* FL-- Front Left */
/* FC --Front Center */
/* FR --Front Right */
/* FLC-- Front Left Center */
/* FRC-- Front RiQhtCenter */
/* RL-- Rear Left */
/* RC --Rear Center */
/* RR-- Rear Right */
/* RLC-- Rear Left Center */
/* RRC --Rear RiQhtCenter */
/* LFE-- Low Frequency Effect */
enum hdmi_speak_location {
	CA_FR_FL = 0,
	CA_LFE_FR_FL,
	CA_FC_FR_FL,
	CA_FC_LFE_FR_FL,

	CA_RC_FR_FL,
	CA_RC_LFE_FR_FL,
	CA_RC_FC_FR_FL,
	CA_RC_FC_LFE_FR_FL,

	CA_RR_RL_FR_FL,
	CA_RR_RL_LFE_FR_FL,
	CA_RR_RL_FC_FR_FL,
	CA_RR_RL_FC_LFE_FR_FL,

	CA_RC_RR_RL_FR_FL,
	CA_RC_RR_RL_LFE_FR_FL,
	CA_RC_RR_RL_FC_FR_FL,
	CA_RC_RR_RL_FC_LFE_FR_FL,

	CA_RRC_RC_RR_RL_FR_FL,
	CA_RRC_RC_RR_RL_LFE_FR_FL,
	CA_RRC_RC_RR_RL_FC_FR_FL,
	CA_RRC_RC_RR_RL_FC_LFE_FR_FL,

	CA_FRC_RLC_FR_FL,
	CA_FRC_RLC_LFE_FR_FL,
	CA_FRC_RLC_FC_FR_FL,
	CA_FRC_RLC_FC_LFE_FR_FL,

	CA_FRC_RLC_RC_FR_FL,
	CA_FRC_RLC_RC_LFE_FR_FL,
	CA_FRC_RLC_RC_FC_FR_FL,
	CA_FRC_RLC_RC_FC_LFE_FR_FL,

	CA_FRC_RLC_RR_RL_FR_FL,
	CA_FRC_RLC_RR_RL_LFE_FR_FL,
	CA_FRC_RLC_RR_RL_FC_FR_FL,
	CA_FRC_RLC_RR_RL_FC_LFE_FR_FL,
};

enum hdmi_audio_downmix {
	LSV_0DB = 0,
	LSV_1DB,
	LSV_2DB,
	LSV_3DB,
	LSV_4DB,
	LSV_5DB,
	LSV_6DB,
	LSV_7DB,
	LSV_8DB,
	LSV_9DB,
	LSV_10DB,
	LSV_11DB,
	LSV_12DB,
	LSV_13DB,
	LSV_14DB,
	LSV_15DB,
};

enum hdmi_rx_audio_state {
	STATE_AUDIO__MUTED = 0,
	STATE_AUDIO__REQUEST_AUDIO = 1,
	STATE_AUDIO__AUDIO_READY = 2,
	STATE_AUDIO__ON = 3,
};

/* Sampling Freq Fs:
 * 0 - Refer to Stream Header;
 * 1 - 32KHz;
 * 2 - 44.1KHz;
 * 3 - 48KHz;
 * 4 - 88.2KHz...
 */
enum hdmi_audio_fs {
	FS_REFER_TO_STREAM = 0,
	FS_32K = 1,
	FS_44K1 = 2,
	FS_48K = 3,
	FS_88K2 = 4,
	FS_96K = 5,
	FS_176K4 = 6,
	FS_192K = 7,
	FS_768K = 8,
	FS_MAX,
};

struct rate_map_fs {
	u32 rate;
	enum hdmi_audio_fs fs;
};

struct hdmi_rx_audioinfo {
	/* !< Signal decoding type -- TvAudioType */
	enum hdmi_audio_type type;
	enum hdmi_audio_format format;
	/* !< active audio channels bit mask. */
	enum hdmi_audio_chnnum channels;
	enum hdmi_audio_fs fs; /* !< Signal sample rate in Hz */
	enum hdmi_audio_sampsize ss;
	enum hdmi_speak_location speak_loc;
	enum hdmi_audio_downmix lsv;
	u32 N_value;
	u32 CTS;
};

#define AUDIO_PARA_MAX_NUM       14
struct hdmi_audio_fs_ncts {
	struct {
		u32 tmds_clk;
		u32 n; /* 24 or 30 bit */
		u32 cts; /* 24 or 30 bit */
		u32 n_36bit;
		u32 cts_36bit;
		u32 n_48bit;
		u32 cts_48bit;
	} array[AUDIO_PARA_MAX_NUM];
	u32 def_n;
};

struct parse_cd {
	enum hdmi_color_depth cd;
	const char *name;
};

struct parse_cs {
	enum hdmi_color_space cs;
	const char *name;
};

struct parse_cr {
	enum hdmi_color_range cr;
	const char *name;
};

/* Refer CEA861-D Page 116 Table 55 */
struct dtd {
	unsigned short pixel_clock;
	unsigned short h_active;
	unsigned short h_blank;
	unsigned short v_active;
	unsigned short v_blank;
	unsigned short h_sync_offset;
	unsigned short h_sync;
	unsigned short v_sync_offset;
	unsigned short v_sync;
	u8 h_image_size;
	u8 v_image_size;
	u8 h_border;
	u8 v_border;
	u8 flags;
	enum hdmi_vic vic;
};

struct vesa_standard_timing {
	unsigned short hactive;
	unsigned short vactive;
	unsigned short hblank;
	unsigned short vblank;
	unsigned short hsync;
	unsigned short tmds_clk; /* Value = Pixel clock ?? 10,000 */
	enum hdmi_vic vesa_timing;
};

#endif
