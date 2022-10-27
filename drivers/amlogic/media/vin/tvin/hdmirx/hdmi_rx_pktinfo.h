/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMIRX_PKT_INFO_H__
#define __HDMIRX_PKT_INFO_H__

#include <linux/workqueue.h>
#include <linux/amlogic/media/registers/cpu_version.h>

#define K_ONEPKT_BUFF_SIZE		8
#define K_PKT_REREAD_SIZE		2

#define K_FLAG_TAB_END			0xa0a05f5f

#define VSIF_PKT_READ_FROM_PD_FIFO

#define IEEE_VSI14		0x000c03
#define IEEE_DV15		0x00d046
#define IEEE_VSI21		0xc45dd8
#define IEEE_HDR10PLUS		0x90848b
#define IEEE_DV_PLUS_ALLM 0x1
#define IEEE_FREESYNC	0x00001a

#define VSIF_TYPE_DV15		1
#define VSIF_TYPE_HDR10P	2
#define VSIF_TYPE_HDMI21	4
#define VSIF_TYPE_HDMI14	8

#define EMP_TYPE_VSIF		1
#define EMP_TYPE_VTEM		2
#define EMP_TYPE_HDR		3

#define DV_NULL		0
#define DV_VSIF		1
#define DV_EMP		2

enum vsi_state_e {
	E_VSI_NULL,
	E_VSI_4K3D,
	E_VSI_VSI21,
	E_VSI_HDR10PLUS,
	E_VSI_DV10,
	E_VSI_DV15
};

enum pkt_length_e {
	E_PKT_LENGTH_4 = 0x04,
	E_PKT_LENGTH_5 = 0x05,
	E_PKT_LENGTH_24 = 0x18,
	E_PKT_LENGTH_27 = 0x1B
};

enum pkt_decode_type {
	PKT_BUFF_SET_FIFO = 0x01,
	PKT_BUFF_SET_GMD = 0x02,
	PKT_BUFF_SET_AIF = 0x04,
	PKT_BUFF_SET_AVI = 0x08,
	PKT_BUFF_SET_ACR = 0x10,
	PKT_BUFF_SET_GCP = 0x20,
	PKT_BUFF_SET_VSI = 0x40,
	PKT_BUFF_SET_AMP = 0x80,
	PKT_BUFF_SET_DRM   = 0x100,
	PKT_BUFF_SET_NVBI = 0x200,
	PKT_BUFF_SET_EMP = 0x400,
	PKT_BUFF_SET_SPD = 0x800,

	PKT_BUFF_SET_UNKNOWN = 0xffff,
};

/* data island packet type define */
enum pkt_type_e {
	PKT_TYPE_NULL = 0x0,
	PKT_TYPE_ACR = 0x1,
	/*PKT_TYPE_AUD_SAMPLE = 0x2,*/
	PKT_TYPE_GCP = 0x3,
	PKT_TYPE_ACP = 0x4,
	PKT_TYPE_ISRC1 = 0x5,
	PKT_TYPE_ISRC2 = 0x6,
	/*PKT_TYPE_1BIT_AUD = 0x7,*/
	/*PKT_TYPE_DST_AUD = 0x8,*/
	/*PKT_TYPE_HBIT_AUD = 0x9,*/
	PKT_TYPE_GAMUT_META = 0xa,
	/*PKT_TYPE_3DAUD = 0xb,*/
	/*PKT_TYPE_1BIT3D_AUD = 0xc,*/
	PKT_TYPE_AUD_META = 0xd,
	/*PKT_TYPE_MUL_AUD = 0xe,*/
	/*PKT_TYPE_1BITMUL_AUD = 0xf,*/

	PKT_TYPE_INFOFRAME_VSI = 0x81,
	PKT_TYPE_INFOFRAME_AVI = 0x82,
	PKT_TYPE_INFOFRAME_SPD = 0x83,
	PKT_TYPE_INFOFRAME_AUD = 0x84,
	PKT_TYPE_INFOFRAME_MPEGSRC = 0x85,
	PKT_TYPE_INFOFRAME_NVBI = 0x86,
	PKT_TYPE_INFOFRAME_DRM = 0x87,
	PKT_TYPE_EMP = 0x7f,

	PKT_TYPE_UNKNOWN,
};

enum pkt_op_flag {
	/*infoframe type*/
	PKT_OP_VSI = 0x01,
	PKT_OP_AVI = 0x02,
	PKT_OP_SPD = 0x04,
	PKT_OP_AIF = 0x08,

	PKT_OP_MPEGS = 0x10,
	PKT_OP_NVBI = 0x20,
	PKT_OP_DRM = 0x40,
	PKT_OP_EMP = 0x80,

	PKT_OP_ACR = 0x100,
	PKT_OP_GCP = 0x200,
	PKT_OP_ACP = 0x400,
	PKT_OP_ISRC1 = 0x800,

	PKT_OP_ISRC2 = 0x1000,
	PKT_OP_GMD = 0x2000,
	PKT_OP_AMP = 0x4000,
};

struct pkt_typeregmap_st {
	u32 pkt_type;
	u32 reg_bit;
};

/* audio clock regeneration pkt - 0x1 */
struct acr_pkt_st {
	/*packet header*/
	u8 pkttype;
	u8 zero0;
	u8 zero1;
	u8 rsvd;
	/*sub packet 1*/
	struct sbpkt1_st {
		/*subpacket*/
		u8 SB0;
		u8 SB1_CTS_H:4;
		u8 SB1_rev:4;
		u8 SB2_CTS_M;
		u8 SB3_CTS_L;

		u8 SB4_N_H:4;
		u8 SB4_rev:4;
		u8 SB5_N_M;
		u8 SB6_N_L;
	} sbpkt1;
	/*sub packet 2*/
	struct sbpkt2_st {
		/*subpacket*/
		u8 SB0;
		u8 SB1_CTS_H:4;
		u8 SB1_rev:4;
		u8 SB2_CTS_M;
		u8 SB3_CTS_L;

		u8 SB4_N_H:4;
		u8 SB4_rev:4;
		u8 SB5_N_M;
		u8 SB6_N_L;
	} sbpkt2;
	/*sub packet 3*/
	struct sbpkt3_st {
		/*subpacket*/
		u8 SB0;
		u8 SB1_CTS_H:4;
		u8 SB1_rev:4;
		u8 SB2_CTS_M;
		u8 SB3_CTS_L;

		u8 SB4_N_H:4;
		u8 SB4_rev:4;
		u8 SB5_N_M;
		u8 SB6_N_L;
	} sbpkt3;
	/*sub packet 4*/
	struct sbpkt4_st {
		/*subpacket*/
		u8 SB0;
		u8 SB1_CTS_H:4;
		u8 SB1_rev:4;
		u8 SB2_CTS_M;
		u8 SB3_CTS_L;

		u8 SB4_N_H:4;
		u8 SB4_rev:4;
		u8 SB5_N_M;
		u8 SB6_N_L;
	} sbpkt4;
};

/* audio sample pkt - 0x2 */
struct aud_sample_pkt_st {
	/*packet header*/
	u8 pkttype;
	/*HB1*/
	u8 sample_present:4;
	u8 layout:1;
	u8 hb1_rev:3;
	/*HB2*/
	u8 sample_flat:4;
	u8 b:4;
	u8 rsvd;
	struct aud_smpsbpkt_st {
		/*subpacket*/
		u32 left_27_4:24;
		u32 right_27_4:24;
		/*valid bit from first sub-frame*/
		u32 valid_l:1;
		/*user data bit from first sub-frame*/
		u32 user_l:1;
		/*channel status bit from first sub-frame*/
		u32 channel_l:1;
		/*parity bit from first sub-frame*/
		u32 parity_l:1;
		/*valid bit from second sub-frame*/
		u32 valid_r:1;
		/*user data bit from second sub-frame*/
		u32 user_r:1;
		/*channel status bit from second sub-frame*/
		u32 channel_r:1;
		/*parity bit from second sub-frame*/
		u32 parity_r:1;
	} sbpkt;
};

/* general control pkt - 0x3 */
struct gcp_pkt_st {
	/*packet header*/
	u8 pkttype;
	u8 hb1_zero;
	u8 hb2_zero;
	u8 rsvd;
	/*sub packet*/
	struct gcp_sbpkt_st {
		/*SB0*/
		u8 set_avmute:1;
		u8 sb0_zero0:3;
		u8 clr_avmute:1;
		u8 sb0_zero1:3;
		/*SB1*/
		u8 colordepth:4;
		u8 pixelpkgphase:4;
		/*SB2*/
		u8 def_phase:1;
		u8 sb2_zero:7;
		/*SB3*/
		u8 sb3_zero;
		/*SB4*/
		u8 sb4_zero;
		/*SB5*/
		u8 sb5_zero;
		/*SB6*/
		u8 sb6_zero;
	} sbpkt;
};

/* acp control pkt - 0x4 */
struct acp_pkt_st {
	/*packet header*/
	u8 pkttype;
	u8 acp_type;
	u8 rev;
	u8 rsvd;
	struct acp_sbpkt_st {
		/*depend on acp_type,section 9.3 for detail*/
		u8 pb[28];
	} sbpkt_st;
};

/* ISRC1 pkt - 0x50 and x06 */
struct isrc_pkt_st {
	/*packet header*/
	u8 pkttype;
	u8 isrc_sts:3;
	u8 hb1_rev:3;
	u8	isrc_valid:1;
	u8	isrc_cont:1;
	u8 hb2_rev;
	u8 rsvd;
	/*sub-pkt section 8.2 for detail*/
	struct isrc_sbpkt_st {
		/*UPC_EAN_ISRC 0-15*/
		/*UPC_EAN_ISRC 16-32*/
		u8 upc_ean_isrc[16];
		u8 rev[12];
	} sbpkt;
};

/* one bit audio sample pkt - 0x7 */
struct obasmp_pkt_st {
	/*packet header*/
	u8 pkttype;
	/*HB1*/
	u8 samples_presents_sp_x:4;
	u8 layout:1;
	u8 hb1_rev:3;
	/*HB2*/
	u8 samples_invalid_sp_x:4;
	u8 hb2_rev:4;
	u8 rsvd;
	/*subpacket*/
	struct oba_sbpkt_st {
		u8 cha_part0_7;
		u8 cha_part8_15;
		u8 cha_part16_23;
		u8 chb_part0_7;
		u8 chb_part8_15;
		u8 chb_part16_23;
		u8 cha_part24_27:4;
		u8 chb_part24_27:4;
	} sbpkt;
};

/* DST audio pkt - 0x8 */
struct dstaud_pkt_st {
	/*packet header*/
	u8 pkttype;
	/*HB1*/
	u8 dst_normal_double:1;
	u8 hb1_rsvd:5;
	u8 sample_invalid:1;
	u8 frame_start:1;
	/*HB2*/
	u8 hb2_rsvd;
	u8 rsvd;
	struct dts_subpkt_st {
		u8 data[28];
	} sbpkt;
};

/* hbr audio pkt - 0x9 */
struct hbraud_pkt_st {
	/*packet header*/
	u8 pkttype;
	/*HB1*/
	u8 hb1_rsvd;
	/*HB2*/
	u8 hb2_rsvd:4;
	u8 bx:4;

	u8 rsvd;
	/*subpacket*/
	/*null*/
};

/* gamut metadata pkt - 0xa */
struct gamutmeta_pkt_st {
	/*packet header*/
	u8 pkttype;
	/*HB1*/
	u8 affect_seq_num:4;
	u8 gbd_profile:3;
	u8 next_feild:1;
	/*HB2*/
	u8 cur_seq_num:4;
	u8 pkt_seq:2;
	u8 hb2_rsvd:1;
	u8 no_cmt_gbd:1;
	u8 rsvd;
	/*subpacket*/
	union gamut_sbpkt_e {
		u8 p0_gbd_byte[28];
		struct p1_profile_st {
			u8 gbd_length_h;
			u8 gbd_length_l;
			u8 checksum;
			u8 gbd_byte_l[25];
		} p1_profile;
		u8 p1_gbd_byte_h[28];
	} sbpkt;
};

/* 3d audio sample pkt - 0xb */
struct a3dsmp_pkt_st {
	/*packet header*/
	u8 pkttype;
	/*hb1*/
	u8 sample_presents:4;
	u8 sample_start:1;
	u8 hb1_rsvd:3;
	/*hb2*/
	u8 sample_flat_sp:4;
	u8 b_x:4;

	u8 rsvd;
	/*audio sub-packet*/
	struct aud3d_sbpkt_st {
		u32 left_27_4:24;
		u32 right_27_4:24;
		/*valid bit from first sub-frame*/
		u8 valid_l:1;
		/*user data bit from first sub-frame*/
		u8 user_l:1;
		/*channel status bit from first sub-frame*/
		u8 channel_l:1;
		/*parity bit from first sub-frame*/
		u8 parity_l:1;
		/*valid bit from second sub-frame*/
		u8 valid_r:1;
		/*user data bit from second sub-frame*/
		u8 user_r:1;
		/*channel status bit from second sub-frame*/
		u8 channel_r:1;
		/*parity bit from second sub-frame*/
		u8 parity_r:1;
	} sbpkt;
};

/* one bit 3d audio sample pkt - 0xc */
struct ob3d_smppkt_st {
	/*packet header*/
	u8 pkttype;
	/*hb1*/
	u8 samples_present_sp_x:4;
	u8 sample_start:1;
	u8 hb1_rsvd:3;
	/*hb2*/
	u8 samples_invalid_sp_x:4;
	u8 hb2_rsvd:4;
	u8 rsvd;
	/*subpacket*/
	struct ob_sbpkt {
		u8 cha_part0_7;
		u8 cha_part8_15;
		u8 cha_part16_23;
		u8 chb_part0_7;
		u8 chb_part8_15;
		u8 chb_part16_23;
		u8 cha_part24_27:4;
		u8 chb_part24_27:4;
	} sbpkt;
};

/* audio metadata pkt - 0xd */
struct audmtdata_pkt_st {
	/*packet header*/
	u8 pkttype;
	/*hb1*/
	u8 audio_3d:1;
	u8 hb1_rsvd:7;
	/*hb2*/
	u8 num_view:2;
	u8 num_audio_str:2;
	u8 hb2_rsvd:4;

	u8 rsvd;
	/*sub-packet*/
	union aud_mdata_subpkt_u {
		struct aud_mtsbpkt_3d_1_st {
			u8 threeD_cc:5;
			u8 rsvd2:3;

			u8 acat:4;
			u8 rsvd3:4;

			u8 threeD_ca;
			u8 rsvd4[25];
		} subpkt_3d_1;

		struct aud_mtsbpkt_3d_0_st {
			u8 descriptor0[5];
			u8 descriptor1[5];
			u8 descriptor2[5];
			u8 descriptor3[5];
			u8 rsvd4[8];
		} subpkt_3d_0;
	} sbpkt;
};

/* multi-stream audio sample pkt - 0xe */
struct msaudsmp_pkt_st {
	/*packet header*/
	u8 pkttype;
	/*hb1*/
	u8 stream_present_sp_x:4;
	u8 hb1_rsvd:4;
	/*hb2*/
	u8 stream_flat_sp:4;
	u8 b_x:4;

	u8 rsvd1;
	/*audio sub-packet*/
	struct audmul_sbpkt_st {
		/*subpacket*/
		u32 left_27_4:24;
		u32 right_27_4:24;
		/*valid bit from first sub-frame*/
		u32 valid_l:1;
		/*user data bit from first sub-frame*/
		u32 user_l:1;
		/*channel status bit from first sub-frame*/
		u32 channel_l:1;
		/*parity bit from first sub-frame*/
		u32 parity_l:1;
		/*valid bit from second sub-frame*/
		u32 valid_r:1;
		/*user data bit from second sub-frame*/
		u32 user_r:1;
		/*channel status bit from second sub-frame*/
		u32 channel_r:1;
		/*parity bit from second sub-frame*/
		u32 parity_r:1;
	} sbpkt;
};

/* one bit multi-stream audio sample pkt - 0xf */
struct obmaudsmp_pkt_st {
	/*packet header*/
	u8 pkttype;
	/*hb1*/
	u8 stream_present_sp_x:4;
	u8 hb1_rsvd:4;
	/*hb2*/
	u8 stream_invalid_sp_x:4;
	u8 hb2_rsvd:4;

	u8 rsvd;
	/*audio sub-packet*/
	struct onebmtstr_smaud_sbpkt_st {
		u8 cha_part0_7;
		u8 cha_part8_15;
		u8 cha_part16_23;
		u8 chb_part0_7;
		u8 chb_part8_15;
		u8 chb_part16_23;
		u8 cha_part24_27:4;
		u8 chb_part24_27:4;
	} __packed sbpkt;
} __packed;

/* EMP pkt - 0x7f */
struct emp_pkt_st {
	/*packet header*/
	u8 pkttype;
	/*hb1*/
	u8 first:1;
	u8 last:1;
	u8 hb1_rsvd:6;
	/*hb2*/
	u8 sequence_idx;

	u8 rsvd;
	/*content*/
	struct content_st {
		u8 new:1;
		u8 end:1;
		u8 ds_type:2;
		u8 afr:1;
		u8 vfr:1;
		u8 sync:1;
		u8 rev_0:1;
		u8 rev_1;
		u8 organization_id;
		u8 data_set_tag_hi;
		u8 data_set_tag_lo;
		u8 data_set_length_hi;
		u8 data_set_length_lo;
		u8 md[21];
	} __packed cnt;
} __packed;

/* fifo raw data type - 0x8x */
struct fifo_rawdata_st {
	/*packet header*/
	u8 pkttype;
	u8 version;
	u8 length;
	u8 rsd;
	/*packet body*/
	u8 PB[28];
} __packed;

/* vendor specific infoFrame packet - 0x81 */
struct vsi_infoframe_st {
	u8 pkttype:8;
	struct vsi_ver_st {
		u8 version:7;
		u8 chgbit:1;
	} __packed ver_st;
	u8 length:5;
	u8 rsd:3;
	u8 rsd_hdmi; /* T7 has no this byte */
	/*PB0*/
	u32 checksum:8;
	/*PB1-3*/
	u32 ieee:24;/* first two hex digits*/

	/*body by different format*/
	union vsi_sbpkt_u {
		struct payload_st {
			u32 data[6];
		} __packed payload;

		/* video format 0x01*/
		struct vsi_st {
			u8 data[24];
		} __packed vsi_st;

		/* 3D: video format(0x2) */
		struct vsi_3dext_st {
			/*pb4*/
			u8 rsvd0:5;
			u8 vdfmt:3;
			/*pb5*/
			u8 rsvd2:3;
			u8 threed_meta_pre:1;
			u8 threed_st:4;
			/*pb6*/
			u8 rsvd3:4;
			u8 threed_ex:4;
			/*pb7*/
			u8 threed_meta_type:3;
			u8 threed_meta_length:5;
			u8 threed_meta_data[20];
		} __packed vsi_3dext;

		/* dolby vision10, length 0x18 */
		/* ieee 0x000c03 */
		struct vsi_dobv10 {
			/*pb4*/
			/* 0x00: Video formats not defined in Table 8-14
			 *	of the HDMI specification v1.4b
			 * 0x20: Video formats defined in Table 8-14
			 *	of the HDMI specification v1.4b
			 */
			u8 vdfmt;
			/*pb5*/
			/* 0x0: Video formats not defined in Table 8-14
			 *	of the HDMI specification, v1.4b
			 * 0x1: 4K x 2K at 29.97 Hz or 30Hz
			 *	as defined in Table 8-14
			 *	of the HDMI specification, v1.4b
			 * 0x2: 4K x 2K at 25 Hz as defined in Table 8-14
			 *	of the HDMI specification, v1.4b
			 * 0x3: 4K x 2K at 23.98 Hz or 24Hz
			 *	as defined in Table 8-14
			 *	of the HDMI specification, v1.4b
			 */
			u8 hdmi_vic;
			/*pb6*/
			u8 data[19]; /* val=0 */
		} __packed vsi_dobv10;

		/* dolby vision15, length 0x1b*/
		/* ieee 0x00d046 */
		struct vsi_dobv15 {
			/*pb4*/
			u8 ll:1;
			u8 dv_vs10_sig_type:4;
			u8 source_dm_ver:3;
			/*pb5*/
			u8 tmax_pq_hi:4;
			u8 rsvd:2;
			u8 aux_md:1;
			u8 bklt_md:1;
			/*pb6*/
			u8 tmax_pq_lo;
			/*pb7*/
			u8 aux_run_mode;
			/*pb8*/
			u8 aux_run_ver;
			/*pb9*/
			u8 aux_debug;
			/* pb10 */
			u8 content_type;
			/*pb11~27*/
			u8 data[17]; /* val=0 */
		} __packed vsi_dobv15;

		/* HDR10+, length 27*/
		/* ieee 0x90848b */
		struct vsi_hdr10p {
			/*pb4*/
			u8 rsvd:1;
			u8 max_lum:5;
			u8 app_ver:2;
			u8 average_maxrgb;
			u8 distr_val_0;
			u8 distr_val_1;
			u8 distr_val_2;
			u8 distr_val_3;
			u8 distr_val_4;
			u8 distr_val_5;
			u8 distr_val_6;
			u8 distr_val_7;
			u8 distr_val_8;
			u8 knee_point_x_hi:4;
			u8 num_bezier:4;
			u8 knee_point_y_hi:2;
			u8 knee_point_x_lo:6;
			u8 knee_point_y_lo;
			/*pb17~26*/
			u8 data[10]; /* val=0 */
			u8 rsvd1:6;
			u8 vsif_timing_mode:1;
			u8 graphics_overlay_flag:1;
		} __packed vsi_hdr10p;

		/*TODO:hdmi2.1 spec vsi packet*/
		struct vsi_st_21 {
			/*pb4*/
			uint8_t ver:8;
			/*pb5*/
			u8 valid_3d:1;
			u8 allm_mode:1;
			u8 rsvd1:2;
			u8 ccbpc:4;
			/*pb6*/
			u8 data[22];
			/*todo*/
		} __packed vsi_st_21;
	} __packed sbpkt;
} __packed;

/* AVI infoFrame packet - 0x82 */
struct avi_infoframe_st {
	u8 pkttype;
	u8 version;
	u8 length;
	/*PB0*/
	u8 checksum;
	union cont_u {
		struct v1_st {
			/*byte 1*/
			u8 scaninfo:2;			/* S1,S0 */
			u8 barinfo:2;			/* B1,B0 */
			u8 activeinfo:1;		/* A0 */
			u8 colorindicator:2;	/* Y1,Y0 */
			u8 rev0:1;
			/*byte 2*/
			u8 fmt_ration:4;		/* R3-R0 */
			u8 pic_ration:2;		/* M1-M0 */
			u8 colorimetry:2;		/* C1-C0 */
			/*byte 3*/
			u8 pic_scaling:2;		/* SC1-SC0 */
			u8 rev1:6;
			/*byte 4*/
			u8 rev2:8;
			/*byte 5*/
			u8 rev3:8;
		} __packed v1;
		struct v4_st { /* v2=v3=v4 */
			/*byte 1*/
			u8 scaninfo:2;			/* S1,S0 */
			u8 barinfo:2;			/* B1,B0 */
			u8 activeinfo:1;		/* A0 1 */
			u8 colorindicator:3;		/* Y2-Y0 */
			/*byte 2*/
			u8 fmt_ration:4;		/* R3-R0 */
			u8 pic_ration:2;		/* M1-M0 */
			u8 colorimetry:2;		/* C1-C0 */
			/*byte 3*/
			u8 pic_scaling:2;		/* SC1-SC0 */
			u8 qt_range:2;			/* Q1-Q0 */
			u8 ext_color:3;			/* EC2-EC0 */
			u8 it_content:1;		/* ITC */
			/*byte 4*/
			u8 vic:8;				/* VIC7-VIC0 */
			/*byte 5*/
			u8 pix_repeat:4;		/* PR3-PR0 */
			u8 content_type:2;		/* CN1-CN0 */
			u8 ycc_range:2;			/* YQ1-YQ0 */
		} __packed v4;
	} cont;
	/*byte 6,7*/
	u16 line_num_end_topbar:16;	/*littel endian can use*/
	/*byte 8,9*/
	u16 line_num_start_btmbar:16;
	/*byte 10,11*/
	u16 pix_num_left_bar:16;
	/*byte 12,13*/
	u16 pix_num_right_bar:16;
	/* byte 14 */
	u8 additional_colorimetry;
} __packed;

/* source product descriptor infoFrame  - 0x83 */
//0x00 "unknown",
//0x01 "Digital STB",
//0x02 "DVD player",
//0x03 "D-VHS",
//0x04 "HDD Videorecorder",
//0x05 "DVC",
//0x06 "DSC",
//0x07 "Video CD",
//0x08 "Game",
//0x09 "PC general",
//0x0A "Blu-Ray Disc (BD)",
//0x0B "Super Audio CD",
//0x0C "HD DVD",
//0x0D "PMP",
//0x1A "FREESYNC"
struct spd_infoframe_st {
	u8 pkttype;
	u8 version;
	u8 length;
	//u8 rsd;  //note: T3 has not this byte. T5 has it.
	u8 checksum;
	//u8 ieee_oui[3]; //data[1:3]
	union meta_u {
		struct freesync_st {
			/*PB1-3*/
			u32 ieee:24;
			u32 rsvd:8;
			u8 rsvd1;
			/*PB6*/
			u8 supported:1;
			u8 enabled:1;
			u8 active:1;
			u8 cs_active:1;
			u8 rsvd2:2;
			u8 ld_disable:1;
			u8 rsvd3:3;
			u8 min_frame_rate;
			u8 max_frame_rate;
			/*pb9-pb27*/
			u8 data[19];
		} __packed freesync;
		u8 data[28];
	} __packed des_u;
} __packed;

/* audio infoFrame packet - 0x84 */
struct aud_infoframe_st {
	u8 pkttype;
	u8 version;
	u8 length;
	u8 rsd;
	u8 checksum;
	/*byte 1*/
	u8 ch_count:3;		/*CC2-CC0*/
	u8 rev0:1;
	u8 coding_type:4;	/*CT3-CT0*/
	/*byte 2*/
	u8 sample_size:2;	/*SS1-SS0*/
	u8 sample_frq:3;	/*SF2-SF0*/
	u8 rev1:3;
	/*byte 3*/
	u8 fromat;		/*fmt according to CT3-CT0*/
	/*byte 4*/
	u8 ca;		/*CA7-CA0*/
	/*byte 5*/
	u8 lfep:2; /*BL1-BL0*/
	u8 rev2:1;
	u8 level_shift_value:4;/*LSV3-LSV0*/
	u8 down_mix:1;/*DM_INH*/
	/*byte 6-10*/
	u8 rev[5];
} __packed;

/* mpeg source infoframe packet - 0x85 */
struct ms_infoframe_st {
	u8 pkttype;
	u8 version;
	u8 length;
	u8 rsd;
	u8 checksum;
	/*byte 1-4*/
	/*little endian mode*/
	u32 bitrate;	/*byte MB0(low)-MB3(upper)*/

	/*byte 5*/
	struct ms_byte5_st {
		u8 mpeg_frame:2;/*MF1-MF0*/
		u8 rev0:2;
		u8 field_rpt:1;/*FR0*/
		u8 rev1:3;
	} __packed b5_st;
	/*byte 6-10*/
	u8 rev[5];
} __packed;

/* ntsc vbi infoframe packet - 0x86 */
struct vbi_infoframe_st {
	u8 pkttype;
	u8 version;
	u8 length;
	u8 rsd;
	u8 checksum;
	/*packet content*/
	u8 data_identifier;
	u8 data_unit_id;
	u8 data_unit_length;
	u8 data_field[24];
} __packed;

/* dynamic range and mastering infoframe packet - 0x87 */
struct drm_infoframe_st {
	u8 pkttype;
	u8 version;
	u8 length;
	u8 rsd;

	/*static metadata descriptor*/
	union meta_des_u {
		struct des_type1_st {
			/*PB0*/
			u8 checksum;
			/*PB1*/
			/*electrico-optinal transfer function*/
			u8 eotf:3;
			u8 rev0:5;
			/*PB2*/
			/*static metadata descriptor id*/
			u8 meta_des_id:3;
			u8 rev1:5;

			/*little endian use*/
			/*display primaries*/
			u16 dis_pri_x0;
			u16 dis_pri_y0;
			u16 dis_pri_x1;
			u16 dis_pri_y1;
			u16 dis_pri_x2;
			u16 dis_pri_y2;
			u16 white_points_x;
			u16 white_points_y;
			/*max display mastering luminance*/
			u16 max_dislum;
			/*min display mastering luminance*/
			u16 min_dislum;
			/*maximum content light level*/
			u16 max_light_lvl;
			/*maximum frame-average light level*/
			u16 max_fa_light_lvl;
		} __packed tp1;
		u32 payload[7];
	} __packed des_u;
} __packed;

union pktinfo {
	/*normal packet 0x0-0xf*/
	struct acr_pkt_st audclkgen_ptk;
	struct aud_sample_pkt_st audsmp_pkt;
	struct gcp_pkt_st gcp_pkt;
	struct acp_pkt_st acp_pkt;
	struct isrc_pkt_st isrc_pkt;
	struct obasmp_pkt_st onebitaud_pkt;
	struct dstaud_pkt_st dstaud_pkt;
	struct hbraud_pkt_st hbraud_pkt;
	struct gamutmeta_pkt_st gamutmeta_pkt;
	struct a3dsmp_pkt_st aud3dsmp_pkt;
	struct ob3d_smppkt_st oneb3dsmp_pkt;
	struct audmtdata_pkt_st audmeta_pkt;
	struct msaudsmp_pkt_st mulstraudsamp_pkt;
	struct obmaudsmp_pkt_st obmasmpaud_pkt;
	struct emp_pkt_st emp_pkt;
};

union infoframe_u {
	/*info frame 0x81 - 0x87*/
	/* struct pd_infoframe_s word_md_infoframe; */
	struct fifo_rawdata_st raw_infoframe;
	struct vsi_infoframe_st vsi_infoframe;
	struct avi_infoframe_st avi_infoframe;
	struct spd_infoframe_st spd_infoframe;
	struct aud_infoframe_st aud_infoframe;
	struct ms_infoframe_st ms_infoframe;
	struct vbi_infoframe_st vbi_infoframe;
	struct drm_infoframe_st drm_infoframe;
};

enum vsi_vid_format_e {
	VSI_FORMAT_NO_DATA,
	VSI_FORMAT_EXT_RESOLUTION,
	VSI_FORMAT_3D_FORMAT,
	VSI_FORMAT_FUTURE,
};

struct rxpkt_st {
	u32 pkt_cnt_avi;
	u32 pkt_cnt_vsi;
	u32 pkt_cnt_drm;
	u32 pkt_cnt_spd;
	u32 pkt_cnt_audif;
	u32 pkt_cnt_mpeg;
	u32 pkt_cnt_nvbi;
	u32 pkt_cnt_acr;
	u32 pkt_cnt_gcp;
	u32 pkt_cnt_acp;
	u32 pkt_cnt_isrc1;
	u32 pkt_cnt_isrc2;
	u32 pkt_cnt_gameta;
	u32 pkt_cnt_amp;
	u32 pkt_cnt_emp;
	//modify to rcvd
	u32 pkt_cnt_vsi_ex;
	u32 pkt_cnt_drm_ex;
	u32 pkt_cnt_gmd_ex;
	u32 pkt_cnt_aif_ex;
	u32 pkt_cnt_avi_ex;
	u32 pkt_cnt_acr_ex;
	u32 pkt_cnt_gcp_ex;
	u32 pkt_cnt_amp_ex;
	u32 pkt_cnt_nvbi_ex;
	u32 pkt_cnt_emp_ex;
	u32 pkt_spd_updated;
	u32 pkt_op_flag;

	u32 fifo_int_cnt;
	u32 fifo_pkt_num;
	u8 dv_pkt_num;

	u32 pkt_chk_flg;

	u32 pkt_attach_vsi;
	u32 pkt_attach_drm;
};

struct pd_infoframe_s {
	u32 HB;
	u32 PB0;
	u32 PB1;
	u32 PB2;
	u32 PB3;
	u32 PB4;
	u32 PB5;
	u32 PB6;
};

struct packet_info_s {
	/* packet type 0x81 vendor-specific */
	struct pd_infoframe_s vs_info;
	/* packet type 0x82 AVI */
	struct pd_infoframe_s avi_info;
	/* packet type 0x83 source product description */
	struct pd_infoframe_s spd_info;
	/* packet type 0x84 Audio */
	struct pd_infoframe_s aud_pktinfo;
	/* packet type 0x85 Mpeg source */
	struct pd_infoframe_s mpegs_info;
	/* packet type 0x86 NTSCVBI */
	struct pd_infoframe_s ntscvbi_info;
	/* packet type 0x87 DRM */
	struct pd_infoframe_s drm_info;
	/* packet type 0x01 info */
	struct pd_infoframe_s acr_info;
	/* packet type 0x03 info */
	struct pd_infoframe_s gcp_info;
	/* packet type 0x04 info */
	struct pd_infoframe_s acp_info;
	/* packet type 0x05 info */
	struct pd_infoframe_s isrc1_info;
	/* packet type 0x06 info */
	struct pd_infoframe_s isrc2_info;
	/* packet type 0x0a info */
	struct pd_infoframe_s gameta_info;
	/* packet type 0x0d audio metadata data */
	struct pd_infoframe_s amp_info;
	/* packet type 0x7f emp */
	struct pd_infoframe_s emp_info;
};

struct st_pkt_test_buff {
	/* packet type 0x81 vendor-specific */
	struct pd_infoframe_s vs_info;
	/* packet type 0x82 AVI */
	struct pd_infoframe_s avi_info;
	/* packet type 0x83 source product description */
	struct pd_infoframe_s spd_info;
	/* packet type 0x84 Audio */
	struct pd_infoframe_s aud_pktinfo;
	/* packet type 0x85 Mpeg source */
	struct pd_infoframe_s mpegs_info;
	/* packet type 0x86 NTSCVBI */
	struct pd_infoframe_s ntscvbi_info;
	/* packet type 0x87 DRM */
	struct pd_infoframe_s drm_info;

	/* packet type 0x01 info */
	struct pd_infoframe_s acr_info;
	/* packet type 0x03 info */
	struct pd_infoframe_s gcp_info;
	/* packet type 0x04 info */
	struct pd_infoframe_s acp_info;
	/* packet type 0x05 info */
	struct pd_infoframe_s isrc1_info;
	/* packet type 0x06 info */
	struct pd_infoframe_s isrc2_info;
	/* packet type 0x0a info */
	struct pd_infoframe_s gameta_info;
	/* packet type 0x0d audio metadata data */
	struct pd_infoframe_s amp_info;

	/* packet type 0x7f EMP */
	struct pd_infoframe_s emp_info;

	/*externl set*/
	struct pd_infoframe_s ex_vsi;
	struct pd_infoframe_s ex_avi;
	struct pd_infoframe_s ex_audif;
	struct pd_infoframe_s ex_drm;
	struct pd_infoframe_s ex_nvbi;
	struct pd_infoframe_s ex_acr;
	struct pd_infoframe_s ex_gcp;
	struct pd_infoframe_s ex_gmd;
	struct pd_infoframe_s ex_amp;
};

extern struct packet_info_s rx_pkt;
extern u32 rx_vsif_type;
extern u32 rx_emp_type;
extern u32 rx_spd_type;
/*extern bool hdr_enable;*/
void rx_pkt_status(void);
void rx_pkt_debug(void);
void rx_debug_pktinfo(char input[][20]);
void rx_pkt_dump(enum pkt_type_e typeid);
void rx_pkt_initial(void);
int rx_pkt_handler(enum pkt_decode_type pkt_int_src);
u32 rx_pkt_type_mapping(enum pkt_type_e pkt_type);
void rx_pkt_buffclear(enum pkt_type_e pkt_type);
void rx_pkt_content_chk_en(u32 enable);
void rx_pkt_check_content(void);
void rx_pkt_set_fifo_pri(u32 pri);
u32 rx_pkt_get_fifo_pri(void);
void rx_get_vsi_info(void);
/*please ignore checksum byte*/
void rx_pkt_get_audif_ex(void *pktinfo);
/*please ignore checksum byte*/
void rx_pkt_get_avi_ex(void *pktinfo);
void rx_pkt_get_drm_ex(void *pktinfo);
void rx_pkt_get_acr_ex(void *pktinfo);
void rx_pkt_get_gmd_ex(void *pktinfo);
void rx_pkt_get_ntscvbi_ex(void *pktinfo);
void rx_pkt_get_amp_ex(void *pktinfo);
void rx_pkt_get_vsi_ex(void *pktinfo);
void rx_pkt_get_gcp_ex(void *pktinfo);
u32 rx_pkt_chk_attach_vsi(void);
void rx_pkt_clr_attach_vsi(void);
u32 rx_pkt_chk_attach_drm(void);
//SPD
u32 rx_pkt_chk_updated_spd(void);
void rx_pkt_clr_updated_spd(void);
void rx_pkt_clr_attach_drm(void);
u32 rx_pkt_chk_busy_vsi(void);
u32 rx_pkt_chk_busy_drm(void);
void rx_get_pd_fifo_param(enum pkt_type_e pkt_type,
			  struct pd_infoframe_s *pkt_info);
void rx_get_avi_info(struct avi_infoframe_st *st_pkt);
void rx_get_vtem_info(void);
void rx_get_aif_info(void);
void dump_pktinfo_status(void);
#endif
