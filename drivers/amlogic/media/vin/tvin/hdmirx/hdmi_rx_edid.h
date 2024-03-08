/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_RX_EDID_H_
#define _HDMI_RX_EDID_H_

#define EDID_EXT_BLK_OFF	128
#define EDID_SIZE			256
#define EDID_HDR_SIZE		7
#define EDID_HDR_HEAD_LEN	4
#define MAX_HDR_LUMI_LEN	3
#define MAX_EDID_BUF_SIZE	(EDID_SIZE * 6)
#define PORT_NUM		3
#define LATENCY_MAX		254
#define EDID_MAX_REFRESH_RATE 123 //No use for reference board

/* CTA-861G Table 54~57 CTA data block tag codes */
/* tag code 0x0: reserved */
#define AUDIO_TAG 0x1
#define VIDEO_TAG 0x2
#define VENDOR_TAG 0x3
/* tag of HF-VSDB is the same as VSDB
 * so add an offset(0xF0) for HF-VSDB
 */
#define HF_VSDB_OFFSET 0xF0
#define HF_VENDOR_DB_TAG (VENDOR_TAG + HF_VSDB_OFFSET)
#define SPEAKER_TAG 0x4
/* VESA Display Transfer Characteristic Data Block */
#define VDTCDB_TAG 0x5
/* tag code 0x6: reserved */
#define USE_EXTENDED_TAG 0x7

/* extended tag code */
#define VCDB_TAG 0 /* video capability data block */
#define VSVDB_TAG 1 /* Vendor-Specific Video Data Block */
#define VSVDB_OFFSET	0xF0
#define FREESYNC_OFFSET 0xE0
#define VSDB_FREESYNC_TAG (VENDOR_TAG + FREESYNC_OFFSET)
#define VSVDB_DV_TAG	((USE_EXTENDED_TAG << 8) + VSVDB_TAG)
#define VSVDB_HDR10P_TAG ((USE_EXTENDED_TAG << 8) + VSVDB_TAG + VSVDB_OFFSET)
#define VDDDB_TAG 2 /* VESA Display Device Data Block */
#define VVTBE_TAG 3 /* VESA Video Timing Block Extension */
/* extend tag code 0x4: Reserved for HDMI Video Data Block */
#define CDB_TAG 0x5 /* Colorimetry Data Block */
#define HDR_STATIC_TAG 6 /* HDR Static Metadata Data Block */
#define HDR_DYNAMIC_TAG 7 /* HDR Dynamic Metadata Data Block */
/* extend tag code 8-12: reserved */
#define VFPDB_TAG 13 /* Video Format Preference Data Block */
#define Y420VDB_TAG 14 /* YCBCR 4:2:0 Video Data Block */
#define Y420CMDB_TAG 15 /* YCBCR 4:2:0 Capability Map Data Block */
/* extend tag code 16: Reserved for CTA Miscellaneous Audio Fields */
#define VSADB_TAG 17 /* Vendor-Specific Audio Data Block */
/* extend tag code 18: Reserved for HDMI Audio Data Block */
#define RCDB_TAG 19 /* Room Configuration Data Block */
#define SLDB_TAG 20	/* Speaker Location Data Block */
/* extend tag code 21~31: Reserved */
#define IFDB_TAG 32 /* infoframe data block */
#define HF_EEODB 0x78 /* HDMI forum EDID extension override data block */
#define HDMI_VIC420_OFFSET 0x100
#define HDMI_3D_OFFSET 0x180
#define HDMI_VESA_OFFSET 0x200

/* eARC Rx Capabilities Data Structure version */
#define CAP_DS_VER 0x1
/* eARC Rx Capabilities Data Structure Maximum Length */
#define EARC_CAP_DS_MAX_LENGTH	256
/* eARC Rx Capability data structure End Marker */
#define EARC_CAP_DS_END_MARKER 0x00
#define EARC_CAP_BLOCK_MAX 3
#define DATA_BLK_MAX_NUM 32
#define TAG_MAX 0xFF
/* data block max length(include head): 0x1F+1 */
#define DB_LEN_MAX 32
/* short audio descriptor length */
#define SAD_LEN 3
#define BLK_LENGTH(a) ((a) & 0x1F)
/* maximum VSVDB length is VSVDB V0: 26bytes */
#define VSVDB_LEN	26
#define MAX_AUDIO_BLK_LEN 31
/* max pixel clk H or V active framerate */
#define MAX_PIXEL_CLK 600
#define MAX_H_ACTIVE 3840
#define MAX_V_ACTIVE 2160
#define MAX_FRAME_RATE 60
#define REFRESH_RATE 60

enum edid_audio_format_e {
	AUDIO_FORMAT_HEADER,
	AUDIO_FORMAT_LPCM,
	AUDIO_FORMAT_AC3,
	AUDIO_FORMAT_MPEG1,
	AUDIO_FORMAT_MP3,
	AUDIO_FORMAT_MPEG2,
	AUDIO_FORMAT_AAC,
	AUDIO_FORMAT_DTS,
	AUDIO_FORMAT_ATRAC,
	AUDIO_FORMAT_OBA,
	AUDIO_FORMAT_DDP,
	AUDIO_FORMAT_DTSHD,
	AUDIO_FORMAT_MAT,
	AUDIO_FORMAT_DST,
	AUDIO_FORMAT_WMAPRO,
};

enum edid_tag_e {
	EDID_TAG_NONE,
	EDID_TAG_AUDIO = 1,
	EDID_TAG_VIDEO = 2,
	EDID_TAG_VSDB = 3,
	EDID_TAG_HF_VSDB = 0xf3,
	EDID_TAG_HDR = ((0x7 << 8) | 6),
};

enum edid_delivery_mothed_e {
	EDID_DELIVERY_NULL,
	EDID_DELIVERY_ALL_PORT,
	EDID_DELIVERY_ONE_PORT,
};

union bit_rate_u {
	struct pcm_t {
		unsigned char size_16bit:1;
		unsigned char size_20bit:1;
		unsigned char size_24bit:1;
		unsigned char size_reserved:5;
	} pcm;
	unsigned char others;
};

struct edid_audio_block_t {
	unsigned char max_channel:3;
	unsigned char format_code:4;
	unsigned char fmt_code_resvd:1;
	union u_sr {
		unsigned char freq_list;
		struct s_sr {
			unsigned char freq_32khz:1;
			unsigned char freq_44_1khz:1;
			unsigned char freq_48khz:1;
			unsigned char freq_88_2khz:1;
			unsigned char freq_96khz:1;
			unsigned char freq_176_4khz:1;
			unsigned char freq_192khz:1;
			unsigned char freq_reserved:1;
		} ssr;
	} usr;
	union bit_rate_u bit_rate;
};

struct edid_hdr_block_t {
	unsigned char ext_tag_code;
	unsigned char sdr:1;
	unsigned char hdr:1;
	unsigned char smtpe_2048:1;
	unsigned char future:5;
	unsigned char meta_des_type1:1;
	unsigned char reserved:7;
	unsigned char max_lumi;
	unsigned char avg_lumi;
	unsigned char min_lumi;
};

enum edid_list_e {
	EDID_LIST_TOP,
	EDID_LIST_14,
	EDID_LIST_14_AUD,
	EDID_LIST_14_420C,
	EDID_LIST_14_420VD,
	EDID_LIST_20,
	EDID_LIST_NUM,
	EDID_LIST_NULL
};

enum edid_ver_e {
	EDID_V14,
	EDID_V20,
	EDID_AUTO
};

struct detailed_timing_desc {
	unsigned int pixel_clk;
	unsigned int hactive;
	unsigned int vactive;
	unsigned int hblank;
	unsigned int vblank;
	unsigned int htotal;
	unsigned int vtotal;
	unsigned int framerate;
};

struct video_db_s {
	/* video data block: 31 bytes long maximum*/
	unsigned char svd_num;
	unsigned char hdmi_vic[31];
};

/* audio data block*/
struct audio_db_s {
	/* support for below audio format:
	 * 14 audio fmt + 1 header = 15
	 * uncompressed audio:lpcm
	 * compressed audio: others
	 */
	unsigned char aud_fmt_sup[15];
	/* short audio descriptor */
	struct edid_audio_block_t sad[15];
};

/* may need extend spker alloc from CTA-SPEC */
/* speaker allocation data block: 3 bytes*/
struct speaker_alloc_db_s {
	unsigned char flw_frw:1;
	unsigned char rlc_rrc:1;
	unsigned char flc_frc:1;
	unsigned char rc:1;
	unsigned char rl_rr:1;
	unsigned char fc:1;
	unsigned char lfe:1;
	unsigned char fl_fr:1;
	unsigned char resrvd1:5;
	unsigned char fch:1;
	unsigned char tc:1;
	unsigned char flh_frh:1;
	unsigned char resrvd2;
};

struct specific_vic_3d {
	unsigned char _2d_vic_order:4;
	unsigned char _3d_struct:4;
	unsigned char _3d_detail:4;
	unsigned char resvd:4;
};

struct vsdb_s {
	u8 len:5;
	u8 tag:3;
	u8 ieee_third;
	u8 ieee_second;
	u8 ieee_first;
	//unsigned int ieee_oui;
	/* phy addr 2 bytes */
	unsigned char b:4;
	unsigned char a:4;
	unsigned char d:4;
	unsigned char c:4;

	/* Set if Sink supports DVI dual-link operation */
	unsigned char dvi_dual:1;
	unsigned char rsv_1:2;
	/* DC_Y444: Set if supports YCbCb4:4:4 Deep Color modes */
	unsigned char dc_y444:1;
	/* the three DC_XXbit bits above only indicate support
	 * for RGB4:4:4 at that pixel size.Support for YCbCb4:4:4
	 * in Deep Color modes is indicated with the DC_Y444 bit.
	 * If DC_Y444 is set, then YCbCr4:4:4 is supported for
	 * all modes indicated by the DC_XXbit flags.
	 */
	unsigned char dc_30bit:1;
	unsigned char dc_36bit:1;
	unsigned char dc_48bit:1;
	/* support_AI: Set to 1 if the Sink supports at least
	 * one function that uses information carried by the
	 * ACP, ISRC1, or ISRC2 packets.
	 */
	unsigned char support_ai:1;
	//pb4
	unsigned char max_tmds_clk;
	//pb5
	/* each bit indicates support for particular Content Type */
	unsigned char cnc0:1;/* graphics */
	unsigned char cnc1:1;/* photo*/
	unsigned char cnc2:1;/* cinema */
	unsigned char cnc3:1;/* game */
	unsigned char rsv_2:1;
	unsigned char hdmi_video_present:1;
	unsigned char i_latency_fields_present:1;
	unsigned char latency_fields_present:1;
	//pb6
	unsigned char video_latency;
	unsigned char audio_latency;
	unsigned char interlaced_video_latency;
	unsigned char interlaced_audio_latency;
	//pb10
	unsigned char resrvd3:3;
	unsigned char image_size:2;
	unsigned char _3d_multi_present:2;
	unsigned char _3d_present:1;
	//pb11
	unsigned char hdmi_3d_len:5;
	unsigned char hdmi_vic_len:3;
	unsigned char hdmi_4k2k_30hz_sup;
	unsigned char hdmi_4k2k_25hz_sup;
	unsigned char hdmi_4k2k_24hz_sup;
	unsigned char hdmi_smpte_sup;
	/*3D*/
	u16 _3d_struct_all;
	u16 _3d_mask_15_0;
	struct specific_vic_3d _2d_vic[16];
	unsigned char _2d_vic_num;
};

struct hf_vsdb_s {
	u8 len:5;
	u8 tag:3;
	u8 ieee_third;
	u8 ieee_second;
	u8 ieee_first;
	//unsigned int ieee_oui;
	//pb1
	unsigned char version;
	//pb2
	unsigned char max_tmds_rate;
	//pb3
	/* if set, the sink is capable of initiating an SCDC read request */
	unsigned char _3d_osd_disparity:1;
	unsigned char dual_view:1;
	unsigned char independ_view:1;
	unsigned char lte_340m_scramble:1;
	unsigned char ccbpci:1;
	unsigned char cable_status:1;
	unsigned char rr_cap:1;
	unsigned char scdc_present:1;
	//pb4
	unsigned char dc_30bit_420:1;
	unsigned char dc_36bit_420:1;
	unsigned char dc_48bit_420:1;
	unsigned char uhd_vic:1;
	unsigned char max_frl_rate:4;
	//pb5
	unsigned char fapa_start_location:1;
	unsigned char allm:1;
	unsigned char fva:1;
	unsigned char neg_mvrr:1;
	unsigned char qms_tfr_min:1;
	unsigned char m_delta:1;
	unsigned char qms:1;
	unsigned char qms_tfr_max:1;
	//pb6
	unsigned char vrr_min:6;
	unsigned char vrr_max_hi:2;//bit[9:8]
	//pb7
	unsigned char vrr_max_lo;
	//pb8
	unsigned char dsc_10bpc:1;
	unsigned char dsc_12bpc:1;
	unsigned char dsc_16bpc:1;//=0
	unsigned char dsc_all_bpp:1;
	unsigned char rsv_0:2;
	unsigned char dsc_native_420:1;
	unsigned char dsc_1p2:1;
	//pb9
	unsigned char dsc_max_slices:4;
	unsigned char dsc_max_frl_rate:4;
	//pb10
	unsigned char dsc_total_chunk_kbytes:6;
	unsigned char rsv_1:2;
	unsigned char fapa_end_extended:1;
	//pb11-28
	unsigned char rsv[18];
};

struct colorimetry_db_s {
	unsigned char BT2020_RGB:1;
	unsigned char BT2020_YCC:1;
	unsigned char bt2020_cycc:1;
	unsigned char adobe_rgb:1;
	unsigned char adobe_ycc601:1;
	unsigned char sycc601:1;
	unsigned char xvycc709:1;
	unsigned char xvycc601:1;

	unsigned char resrvd:4;
	/* MDX: designated for future gamut-related metadata. As yet undefined,
	 * this metadata is carried in an interface-specific way.
	 */
	unsigned char MD3:1;
	unsigned char MD2:1;
	unsigned char MD1:1;
	unsigned char MD0:1;
};

struct hdr_db_s {
	/* CEA861.3 table7-9 */
	/* ET_4 to ET_5 shall be set to 0. Future
	 * Specifications may define other EOTFs
	 */
	unsigned char resrvd1: 4;
	unsigned char eotf_hlg:1;
	/* SMPTE ST 2084[2] */
	unsigned char eotf_smpte_st_2084:1;
	/* Traditional gamma - HDR Luminance Range */
	unsigned char eotf_hdr:1;
	/* Traditional gamma - SDR Luminance Range */
	unsigned char eotf_sdr:1;

	/* Static Metadata Descriptors:
	 * SM_1 to SM_7  Reserved for future use
	 */
	unsigned char hdr_SMD_other_type:7;
	unsigned char hdr_SMD_type1:1;

	unsigned char hdr_lum_max;
	unsigned char hdr_lum_avg;
	unsigned char hdr_lum_min;
};

struct video_cap_db_s {
	/* quantization range:
	 * 0: no data
	 * 1: selectable via AVI YQ/Q
	 */
	unsigned char quanti_range_ycc:1;
	unsigned char quanti_range_rgb:1;
	/* scan behaviour */
	/* s_PT
	 * 0: No Data (refer to S_CE or S_IT fields)
	 * 1: always overscanned
	 * 2: always underscanned
	 * 3: support both
	 */
	unsigned char s_PT:2;
	/* s_IT
	 * 0: IT Video Formats not supported
	 * 1: always overscanned
	 * 2: always underscanned
	 * 3: support both
	 */
	unsigned char s_IT:2;
	unsigned char s_CE:2;
};

struct dv_vsvdb_s {
	unsigned int ieee_oui;
	unsigned char version:3;
	unsigned char DM_version:3;
	unsigned char sup_2160p60hz:1;
	unsigned char sup_yuv422_12bit:1;

	unsigned char target_max_lum:7;
	unsigned char sup_global_dimming:1;

	unsigned char target_min_lum:7;
	unsigned char colorimetry:1;

	unsigned char resvd;
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
};

struct cta_data_blk_info {
	unsigned char cta_blk_index;
	u16 tag_code;
	unsigned char offset;
	unsigned char blk_len;
};

struct cta_blk_parse_info {
	/* audio data block */
	struct video_db_s video_db;
	/* audio data block */
	struct audio_db_s audio_db;
	/* speaker allocation data block */
	struct speaker_alloc_db_s speaker_alloc;
	/* vendor specific data block */
	struct vsdb_s vsdb;
	/* hdmi forum vendor specific data block */
	bool contain_hf_vsdb;
	struct hf_vsdb_s hf_vsdb;
	/* video capability data block */
	bool contain_vcdb;
	struct video_cap_db_s vcdb;
	/* vendor specific video data block - dolby vision */
	bool contain_vsvdb;
	struct dv_vsvdb_s dv_vsvdb;
	/* colorimetry data block */
	bool contain_cdb;
	struct colorimetry_db_s color_db;
	/* HDR Static Metadata Data Block */
	bool contain_hdr_db;
	struct hdr_db_s hdr_db;
	/* Y420 video data block: 6 SVD maximum:
	 * y420vdb support only 4K50/60hz, smpte50/60hz
	 * 4K50/60hz format aspect ratio: 16:9, 64:27
	 * smpte50/60hz 256:135
	 */
	bool contain_y420_vdb;
	unsigned char y420_vic_len;
	unsigned char y420_vdb_vic[6];
	/* Y420 Capability Map Data Block: 31 SVD maximum */
	bool contain_y420_cmdb;
	unsigned char y420_all_vic;
	unsigned char y420_cmdb_vic[31];

	bool contain_vsadb;
	unsigned int vsadb_ieee;
	/* CTA-861-G 7.5.8 27-2-3 */
	unsigned char vsadb_payload[22];

	unsigned char data_blk_num;
	struct cta_data_blk_info db_info[DATA_BLK_MAX_NUM];
};

/* CEA extension */
struct cea_ext_parse_info {
	/* CEA header */
	unsigned char cea_tag;
	unsigned char cea_revision;
	unsigned char dtd_offset;
	unsigned char underscan_sup:1;
	unsigned char basic_aud_sup:1;
	unsigned char ycc444_sup:1;
	unsigned char ycc422_sup:1;
	unsigned char native_dtd_num:4;

	struct cta_blk_parse_info blk_parse_info;
};

struct edid_info_s {
	/* 8 */
	unsigned char manufacturer_name[3];
	/* 10 */
	unsigned int product_code;
	/* 12 */
	unsigned int serial_number;
	/* 16 */
	unsigned char product_week;
	unsigned int product_year;
	/* unsigned char edid_version; */
	/* unsigned char edid_revision; */
	/* 54 + 18 * x */
	struct detailed_timing_desc descriptor1;
	struct detailed_timing_desc descriptor2;
	/* 54 + 18 * x */
	unsigned char monitor_name[13];
	unsigned int max_sup_pixel_clk;
	u8 extension_flag;
	/* 127 */
	unsigned char block0_chk_sum;

	struct cea_ext_parse_info cea_ext_info;

	int free_size;
	unsigned char total_free_size;
	unsigned char dtd_size;
};

struct edid_standard_timing {
	u8 h_active[8];
	u8 aspect_ratio[8];
	u8 refresh_rate[8];
};

struct cta_blk_pair {
	u16 tag_code;
	char *blk_name;
};

struct edid_data_s {
	unsigned char edid[256];
	unsigned int physical[4];
	unsigned char phy_offset;
	unsigned int checksum;
};

enum tx_hpd_event_e {
	E_IDLE = 0,
	E_EXE = 1,
	E_RCV = 2,
};

struct cap_block_s {
	unsigned char block_id;
	unsigned char payload_len;
	unsigned char offset;
};

struct earc_cap_ds {
	unsigned char cap_ds_len;
	unsigned char cap_ds_ver;
	struct cap_block_s cap_block[EARC_CAP_BLOCK_MAX];
	struct cta_blk_parse_info blk_parse_info;
};

enum hdmi_vic_e {
	/* Refer to CEA 861-D */
	HDMI_UNKNOWN = 0,
	HDMI_640x480p60 = 1,
	/* for video format which have two different
	 * aspect ratios, VICs list below that don't
	 * indicate aspect ratio, its aspect ratio
	 * is default. e.g:
	 * HDMI_480p60, means 480p60_4x3
	 * HDMI_720p60, means 720p60_16x9
	 * HDMI_1080p50, means 1080p50_16x9
	 */
	HDMI_480p60 = 2,
	HDMI_480p60_16x9 = 3,
	HDMI_720p60 = 4,
	HDMI_1080i60 = 5,
	HDMI_480i60 = 6,
	HDMI_480i60_16x9 = 7,
	HDMI_1440x240p60 = 8,
	HDMI_1440x240p60_16x9 = 9,
	HDMI_2880x480i60 = 10,
	HDMI_2880x480i60_16x9 = 11,
	HDMI_2880x240p60 = 12,
	HDMI_2880x240p60_16x9 = 13,
	HDMI_1440x480p60 = 14,
	HDMI_1440x480p60_16x9 = 15,
	HDMI_1080p60 = 16,
	HDMI_576p50 = 17,
	HDMI_576p50_16x9 = 18,
	HDMI_720p50 = 19,
	HDMI_1080i50 = 20,
	HDMI_576i50 = 21,
	HDMI_576i50_16x9 = 22,
	HDMI_1440x288p50 = 23,
	HDMI_1440x288p50_16x9 = 24,
	HDMI_2880x576i50 = 25,
	HDMI_2880x576i50_16x9 = 26,
	HDMI_2880x288p50 = 27,
	HDMI_2880x288p50_16x9 = 28,
	HDMI_1440x576p50 = 29,
	HDMI_1440x576p50_16x9 = 30,
	HDMI_1080p50 = 31,
	HDMI_1080p24 = 32,
	HDMI_1080p25 = 33,
	HDMI_1080p30 = 34,
	HDMI_2880x480p60 = 35,
	HDMI_2880x480p60_16x9 = 36,
	HDMI_2880x576p50 = 37,
	HDMI_2880x576p50_16x9 = 38,
	HDMI_1080i50_1250 = 39,
	HDMI_1080i100 = 40,
	HDMI_720p100 = 41,
	HDMI_576p100 = 42,
	HDMI_576p100_16x9 = 43,
	HDMI_576i100 = 44,
	HDMI_576i100_16x9 = 45,
	HDMI_1080i120 = 46,
	HDMI_720p120 = 47,
	HDMI_480p120 = 48,
	HDMI_480p120_16x9 = 49,
	HDMI_480i120 = 50,
	HDMI_480i120_16x9 = 51,
	HDMI_576p200 = 52,
	HDMI_576p200_16x9 = 53,
	HDMI_576i200 = 54,
	HDMI_576i200_16x9 = 55,
	HDMI_480p240 = 56,
	HDMI_480p240_16x9 = 57,
	HDMI_480i240 = 58,
	HDMI_480i240_16x9 = 59,
	/* Refer to CEA 861-F */
	HDMI_720p24 = 60,
	HDMI_720p25 = 61,
	HDMI_720p30 = 62,
	HDMI_1080p120 = 63,
	HDMI_1080p100 = 64,
	HDMI_720p24_64x27 = 65,
	HDMI_720p25_64x27 = 66,
	HDMI_720p30_64x27 = 67,
	HDMI_720p50_64x27 = 68,
	HDMI_720p60_64x27 = 69,
	HDMI_720p100_64x27 = 70,
	HDMI_720p120_64x27 = 71,
	HDMI_1080p24_64x27 = 72,
	HDMI_1080p25_64x27 = 73,
	HDMI_1080p30_64x27 = 74,
	HDMI_1080p50_64x27 = 75,
	HDMI_1080p60_64x27 = 76,
	HDMI_1080p100_64x27 = 77,
	HDMI_1080p120_64x27 = 78,
	HDMI_1680x720p24_64x27 = 79,
	HDMI_1680x720p25_64x27 = 80,
	HDMI_1680x720p30_64x27 = 81,
	HDMI_1680x720p50_64x27 = 82,
	HDMI_1680x720p60_64x27 = 83,
	HDMI_1680x720p100_64x27 = 84,
	HDMI_1680x720p120_64x27 = 85,
	HDMI_2560x1080p24_64x27 = 86,
	HDMI_2560x1080p25_64x27 = 87,
	HDMI_2560x1080p30_64x27 = 88,
	HDMI_2560x1080p50_64x27 = 89,
	HDMI_2560x1080p60_64x27 = 90,
	HDMI_2560x1080p100_64x27 = 91,
	HDMI_2560x1080p120_64x27 = 92,
	/* 3840*2160 */
	HDMI_2160p24_16x9 = 93,
	HDMI_2160p25_16x9 = 94,
	HDMI_2160p30_16x9 = 95,
	HDMI_2160p50_16x9 = 96,
	HDMI_2160p60_16x9 = 97,
	/* 4096*2160 */
	HDMI_4096p24_256x135 = 98,
	HDMI_4096p25_256x135 = 99,
	HDMI_4096p30_256x135 = 100,
	HDMI_4096p50_256x135 = 101,
	HDMI_4096p60_256x135 = 102,
	/* 3840*2160 */
	HDMI_2160p24_64x27 = 103,
	HDMI_2160p25_64x27 = 104,
	HDMI_2160p30_64x27 = 105,
	HDMI_2160p50_64x27 = 106,
	HDMI_2160p60_64x27 = 107,
	HDMI_720x480i = 108,
	HDMI_1920x2160p60_16x9 = 109,
	HDMI_960x540 = 110,
	HDMI_1440x480i60 = 111,
	HDMI_1440x576i50 = 112,
	HDMI_RESERVED = 113,
	/* VIC 111~255: Reserved for the Future */

	/* the following VICs are for y420 mode,
	 * they are fake VICs that used to diff
	 * from non-y420 mode, and have same VIC
	 * with normal VIC in the lower bytes.
	 */
	HDMI_VIC_Y420 = HDMI_VIC420_OFFSET,
	HDMI_2160p50_16x9_Y420	=
		HDMI_VIC420_OFFSET + HDMI_2160p50_16x9,
	HDMI_2160p60_16x9_Y420	=
		HDMI_VIC420_OFFSET + HDMI_2160p60_16x9,
	HDMI_4096p50_256x135_Y420 =
		HDMI_VIC420_OFFSET + HDMI_4096p50_256x135,
	HDMI_4096p60_256x135_Y420 =
		HDMI_VIC420_OFFSET + HDMI_4096p60_256x135,
	HDMI_2160p50_64x27_Y420 =
		HDMI_VIC420_OFFSET + HDMI_2160p50_64x27,
	HDMI_2160p60_64x27_Y420 =
		HDMI_VIC420_OFFSET + HDMI_2160p60_64x27,
	HDMI_1080p_420,
	HDMI_VIC_Y420_MAX,

	HDMI_VIC_3D = HDMI_3D_OFFSET,
	HDMI_480p_FRAMEPACKING,
	HDMI_576p_FRAMEPACKING,
	HDMI_720p_FRAMEPACKING,
	HDMI_1080i_ALTERNATIVE,
	HDMI_1080i_FRAMEPACKING,
	HDMI_1080p_ALTERNATIVE,
	HDMI_1080p_FRAMEPACKING,

	HDMI_640_350 = HDMI_VESA_OFFSET,
	HDMI_640_400,
	HDMI_800_600,
	HDMI_848_480,
	HDMI_1024_768,
	HDMI_720_400,
	HDMI_720_350,
	HDMI_1280_768,
	HDMI_1280_800,
	HDMI_1280_960,
	HDMI_1280_1024,
	HDMI_1360_768,
	HDMI_1366_768,
	HDMI_1600_900,
	HDMI_1600_1200,
	HDMI_1792_1344,
	HDMI_1856_1392,
	HDMI_1920_1200,
	HDMI_1920_1440,
	HDMI_1440_900,
	HDMI_1400_1050,
	HDMI_1680_1050,
	HDMI_2048_1152,
	HDMI_1152_864,
	HDMI_3840_600,
	HDMI_2560_1440,
	HDMI_2560_1600,
	HDMI_2688_1520,
	HDMI_1920_1080_INTERLACED,//1920*1080*interlace
	HDMI_UNSUPPORT,
};

enum earc_cap_block_id {
	EARC_CAP_BLOCK_ID_0 = 0,
	EARC_CAP_BLOCK_ID_1 = 1,
	EARC_CAP_BLOCK_ID_2 = 2,
	EARC_CAP_BLOCK_ID_3 = 3
};

extern u8 port_hpd_rst_flag;
extern int edid_mode;
extern int port_map;
extern bool new_hdr_lum;
extern u32 atmos_edid_update_hpd_en;
extern u32 en_take_dtd_space;
extern u32 earc_cap_ds_update_hpd_en;
extern unsigned char edid_temp[MAX_EDID_BUF_SIZE];
extern unsigned int edid_select;
extern u32 vsvdb_update_hpd_en;
extern enum edid_delivery_mothed_e edid_delivery_mothed;
#ifdef CONFIG_AMLOGIC_HDMITX
extern u32 tx_hdr_priority;
#endif

int rx_set_hdr_lumi(unsigned char *data, int len);
void rx_edid_physical_addr(int a, int b, int c, int d);
unsigned char rx_parse_arc_aud_type(const unsigned char *buff);
bool hdmi_rx_top_edid_update(void);
unsigned char rx_get_edid_index(void);
unsigned char *rx_get_edid(int edid_index);
void edid_parse_block0(u8 *p_edid, struct edid_info_s *edid_info);
void edid_parse_cea_ext_block(u8 *p_edid,
			      struct cea_ext_parse_info *blk_parse_info);
void rx_edid_parse(u8 *p_edid, struct edid_info_s *edid_info);
void rx_edid_parse_print(struct edid_info_s *edid_info);
void rx_blk_index_print(struct cta_blk_parse_info *blk_info);
int rx_edid_free_size(u8 *cur_edid, int size);
unsigned char rx_edid_total_free_size(unsigned char *cur_edid,
				      unsigned int size);
unsigned char rx_get_cea_dtd_size(unsigned char *cur_edid, unsigned int size);
bool rx_set_earc_cap_ds(unsigned char *data, unsigned int len);
void rx_prase_earc_capds_dbg(void);
void edid_splice_earc_capds(unsigned char *p_edid,
			    unsigned char *earc_cap_ds,
			    unsigned int len);
void edid_splice_earc_capds_dbg(u8 *p_edid);
void edid_splice_data_blk_dbg(u8 *p_edid, u8 idx);
void edid_rm_db_by_tag(u8 *p_edid, u16 tagid);
void edid_rm_db_by_idx(u8 *p_edid, u8 blk_idx);
void splice_tag_db_to_edid(u8 *p_edid, u8 *add_buf,
			   u8 buf_len, u16 tagid);
u8 *edid_tag_extract(u8 *p_edid, u16 tagid);
void splice_data_blk_to_edid(u_char *p_edid, u_char *add_buf,
			     u_char blk_idx);
void rx_modify_edid(unsigned char *buffer,
		    int len, unsigned char *addition);
void rx_edid_update_audio_info(unsigned char *p_edid,
			       unsigned int len);
bool is_ddc_idle(unsigned char port_id);
bool is_edid_buff_normal(unsigned char port_id);
bool need_update_edid(void);
enum edid_ver_e get_edid_selection(u8 port);
enum edid_ver_e rx_parse_edid_ver(u8 *p_edid);
u_char *rx_get_cur_edid(u_char port);
bool rx_set_vsvdb(unsigned char *data, unsigned int len);
u_char rx_edid_get_aud_sad(u_char *sad_data);
bool rx_edid_set_aud_sad(u_char *sad, u_char len);
u_int rx_get_cea_tag_offset(u8 *cur_edid, u16 tag_code);
void get_edid_standard_timing_info(u8 *p_edid, struct edid_standard_timing *edid_st_info);
void rm_unsupported_st(u8 *p_edid,
	struct edid_standard_timing *edid_st_info, unsigned int refresh_rate);

#ifdef CONFIG_AMLOGIC_HDMITX
bool rx_update_tx_edid_with_audio_block(unsigned char *edid_data,
					unsigned char *audio_block);
void rpt_edid_hf_vs_db_extraction(unsigned char *p_edid);
void rpt_edid_14_vs_db_extraction(unsigned char *p_edid);
void rpt_edid_video_db_extraction(unsigned char *p_edid);
void rpt_edid_audio_db_extraction(unsigned char *p_edid);
void rpt_edid_colorimetry_db_extraction(unsigned char *p_edid);
void rpt_edid_420_vdb_extraction(unsigned char *p_edid);
void rpt_edid_hdr_static_db_extraction(unsigned char *p_edid);
void rpt_edid_extraction(unsigned char *p_edid);
bool is_valid_edid_data(unsigned char *p_edid);
#endif
#endif
