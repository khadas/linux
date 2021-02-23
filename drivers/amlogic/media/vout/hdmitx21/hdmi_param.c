// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include <linux/string.h>
#include "hdmi_param.h"

static struct hdmi_format_para *all_fmt_paras[] = {
	NULL,
};

struct hdmi_format_para *hdmi21_get_fmt_paras(enum hdmi_vic vic)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	return hdev->para;
}

struct hdmi_format_para *hdmi21_match_dtd_paras(struct dtd *t)
{
	int i;

	if (!t)
		return NULL;
	for (i = 0; all_fmt_paras[i]; i++) {
		if ((abs(all_fmt_paras[i]->timing.pixel_freq / 10
		    - t->pixel_clock) <= (t->pixel_clock + 1000) / 1000) &&
		    t->h_active == all_fmt_paras[i]->timing.h_active &&
		    t->h_blank == all_fmt_paras[i]->timing.h_blank &&
		    t->v_active == all_fmt_paras[i]->timing.v_active &&
		    t->v_blank == all_fmt_paras[i]->timing.v_blank &&
		    t->h_sync_offset == all_fmt_paras[i]->timing.h_front &&
		    t->h_sync == all_fmt_paras[i]->timing.h_sync &&
		    t->v_sync_offset == all_fmt_paras[i]->timing.v_front &&
		    t->v_sync == all_fmt_paras[i]->timing.v_sync
		    )
			return all_fmt_paras[i];
	}

	return NULL;
}

struct hdmi_format_para *hdmi21_get_vesa_paras(struct vesa_standard_timing *t)
{
	return NULL;
}

/*
 * Parameter 'name' can be 1080p60hz, or 1920x1080p60hz
 * or 3840x2160p60hz, 2160p60hz
 * or 3840x2160p60hz420, 2160p60hz420 (Y420 mode)
 */
struct hdmi_format_para *hdmi21_get_fmt_name(char const *name, char const *attr)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();

	return hdev->para;
}

static inline void copy_para(struct hdmi_format_para *des,
			     struct hdmi_format_para *src)
{
	if (!des || !src)
		return;
	memcpy(des, src, sizeof(struct hdmi_format_para));
}

struct hdmi_format_para *hdmi21_tst_fmt_name(char const *name, char const *attr)
{
	static struct hdmi_format_para para;
	int i;

	memset(&para, 0, sizeof(para));
	/* check sname first */
	for (i = 0; i < ARRAY_SIZE(hdmi_timing_all); i++) {
		if (hdmi_timing_all[i].sname &&
		    (strstr(name, hdmi_timing_all[i].sname))) {
			para.timing = hdmi_timing_all[i];
			break;
		}
	}
	if (i == ARRAY_SIZE(hdmi_timing_all)) {
		/* check name */
		for (i = 0; i < ARRAY_SIZE(hdmi_timing_all); i++) {
			if (strstr(name, hdmi_timing_all[i].name)) {
				para.timing = hdmi_timing_all[i];
				break;
			}
		}
	}

	return &para;
}

static struct hdmi_timing *search_mode_in_allpara(char *mode)
{
	int i;

	/* check sname first */
	for (i = 0; i < ARRAY_SIZE(hdmi_timing_all); i++) {
		if (hdmi_timing_all[i].sname &&
		    (strcmp(hdmi_timing_all[i].sname, mode) == 0))
			break;
	}
	if (i < ARRAY_SIZE(hdmi_timing_all))
		return (struct hdmi_timing *)&hdmi_timing_all[i];

	/* check name */
	for (i = 0; i < ARRAY_SIZE(hdmi_timing_all); i++) {
		if (strcmp(hdmi_timing_all[i].name, mode) == 0)
			break;
	}
	if (i < ARRAY_SIZE(hdmi_timing_all))
		return (struct hdmi_timing *)&hdmi_timing_all[i];

	return NULL;
}

struct hdmi_timing *hdmitx21_gettiming(enum hdmi_vic vic)
{
	struct hdmi_timing *timing = NULL;
	int i;

	/* check sname first */
	for (i = 0; i < ARRAY_SIZE(hdmi_timing_all); i++) {
		if (hdmi_timing_all[i].vic == vic) {
			timing = (struct hdmi_timing *)&hdmi_timing_all[i];
			break;
		}
	}
	return timing;
}

struct hdmi_format_para *hdmitx21_get_fmtpara(char *mode)
{
	struct hdmitx_dev *hdev = get_hdmitx21_device();
	struct hdmi_timing *timing;
	struct vinfo_s *tx_vinfo = &hdev->para->hdmitx_vinfo;

	timing = search_mode_in_allpara(mode);
	if (timing) {
		hdev->para->timing = *timing;
		hdev->para->cs = COLORSPACE_YUV444;
		hdev->para->cd = COLORDEPTH_24B;
		/* manually assign hdmitx_vinfo from timing */
		tx_vinfo->name = timing->sname ? timing->sname : timing->name;
		tx_vinfo->mode = VMODE_HDMI;
		tx_vinfo->frac = 0; /* TODO */
		tx_vinfo->width = timing->h_active;
		tx_vinfo->height = timing->v_active;
		tx_vinfo->field_height = timing->pi_mode ? timing->v_active : timing->v_active / 2;
		tx_vinfo->aspect_ratio_num = timing->h_pict;
		tx_vinfo->aspect_ratio_den = timing->v_pict;
		if (timing->v_freq % 1000 == 0) {
			tx_vinfo->sync_duration_num = timing->v_freq / 1000;
			tx_vinfo->sync_duration_den = 1;
		} else {
			tx_vinfo->sync_duration_num = timing->v_freq;
			tx_vinfo->sync_duration_den = 1000;
		}
		tx_vinfo->video_clk = timing->pixel_freq;
		tx_vinfo->htotal = timing->h_total;
		tx_vinfo->vtotal = timing->v_total;
		tx_vinfo->fr_adj_type = VOUT_FR_ADJ_HDMI;
		tx_vinfo->viu_color_fmt = COLOR_FMT_YUV444;
		tx_vinfo->viu_mux = timing->pi_mode ? VIU_MUX_ENCP : VIU_MUX_ENCI;
		/* 1080i use the ENCP, not ENCI */
		if (strncmp(timing->name, "1080i", 5) == 0)
			tx_vinfo->viu_mux = VIU_MUX_ENCP;

		return hdev->para;
	} else {
		return NULL;
	}
}

/* For check all format parameters only */
void check21_detail_fmt(void)
{
}

/* Recommended N and Expected CTS for 32kHz */
static const struct hdmi_audio_fs_ncts aud_32k_para = {
	.array[0] = {
		.tmds_clk = 25175,
		.n = 4576,
		.cts = 28125,
		.n_36bit = 9152,
		.cts_36bit = 84375,
		.n_48bit = 4576,
		.cts_48bit = 56250,
	},
	.array[1] = {
		.tmds_clk = 25200,
		.n = 4096,
		.cts = 25200,
		.n_36bit = 4096,
		.cts_36bit = 37800,
		.n_48bit = 4096,
		.cts_48bit = 50400,
	},
	.array[2] = {
		.tmds_clk = 27000,
		.n = 4096,
		.cts = 27000,
		.n_36bit = 4096,
		.cts_36bit = 40500,
		.n_48bit = 4096,
		.cts_48bit = 54000,
	},
	.array[3] = {
		.tmds_clk = 27027,
		.n = 4096,
		.cts = 27027,
		.n_36bit = 8192,
		.cts_36bit = 81081,
		.n_48bit = 4096,
		.cts_48bit = 54054,
	},
	.array[4] = {
		.tmds_clk = 54000,
		.n = 4096,
		.cts = 54000,
		.n_36bit = 4096,
		.cts_36bit = 81000,
		.n_48bit = 4096,
		.cts_48bit = 108000,
	},
	.array[5] = {
		.tmds_clk = 54054,
		.n = 4096,
		.cts = 54054,
		.n_36bit = 4096,
		.cts_36bit = 81081,
		.n_48bit = 4096,
		.cts_48bit = 108108,
	},
	.array[6] = {
		.tmds_clk = 74176,
		.n = 11648,
		.cts = 210937,
		.n_36bit = 11648,
		.cts_36bit = 316406,
		.n_48bit = 11648,
		.cts_48bit = 421875,
	},
	.array[7] = {
		.tmds_clk = 74250,
		.n = 4096,
		.cts = 74250,
		.n_36bit = 4096,
		.cts_36bit = 111375,
		.n_48bit = 4096,
		.cts_48bit = 148500,
	},
	.array[8] = {
		.tmds_clk = 148352,
		.n = 11648,
		.cts = 421875,
		.n_36bit = 11648,
		.cts_36bit = 632812,
		.n_48bit = 11648,
		.cts_48bit = 843750,
	},
	.array[9] = {
		.tmds_clk = 148500,
		.n = 4096,
		.cts = 148500,
		.n_36bit = 4096,
		.cts_36bit = 222750,
		.n_48bit = 4096,
		.cts_48bit = 297000,
	},
	.array[10] = {
		.tmds_clk = 296703,
		.n = 5824,
		.cts = 421875,
	},
	.array[11] = {
		.tmds_clk = 297000,
		.n = 3072,
		.cts = 222750,
	},
	.array[12] = {
		.tmds_clk = 593407,
		.n = 5824,
		.cts = 843750,
	},
	.array[13] = {
		.tmds_clk = 594000,
		.n = 3072,
		.cts = 445500,
	},
	.def_n = 4096,
};

/* Recommended N and Expected CTS for 44.1kHz and Multiples */
static const struct hdmi_audio_fs_ncts aud_44k1_para = {
	.array[0] = {
		.tmds_clk = 25175,
		.n = 7007,
		.cts = 31250,
		.n_36bit = 7007,
		.cts_36bit = 46875,
		.n_48bit = 7007,
		.cts_48bit = 62500,
	},
	.array[1] = {
		.tmds_clk = 25200,
		.n = 6272,
		.cts = 28000,
		.n_36bit = 6272,
		.cts_36bit = 42000,
		.n_48bit = 6272,
		.cts_48bit = 56000,
	},
	.array[2] = {
		.tmds_clk = 27000,
		.n = 6272,
		.cts = 30000,
		.n_36bit = 6272,
		.cts_36bit = 45000,
		.n_48bit = 6272,
		.cts_48bit = 60000,
	},
	.array[3] = {
		.tmds_clk = 27027,
		.n = 6272,
		.cts = 30030,
		.n_36bit = 6272,
		.cts_36bit = 45045,
		.n_48bit = 6272,
		.cts_48bit = 60060,
	},
	.array[4] = {
		.tmds_clk = 54000,
		.n = 6272,
		.cts = 60000,
		.n_36bit = 6272,
		.cts_36bit = 90000,
		.n_48bit = 6272,
		.cts_48bit = 120000,
	},
	.array[5] = {
		.tmds_clk = 54054,
		.n = 6272,
		.cts = 60060,
		.n_36bit = 6272,
		.cts_36bit = 90090,
		.n_48bit = 6272,
		.cts_48bit = 120120,
	},
	.array[6] = {
		.tmds_clk = 74176,
		.n = 17836,
		.cts = 234375,
		.n_36bit = 17836,
		.cts_36bit = 351562,
		.n_48bit = 17836,
		.cts_48bit = 468750,
	},
	.array[7] = {
		.tmds_clk = 74250,
		.n = 6272,
		.cts = 82500,
		.n_36bit = 6272,
		.cts_36bit = 123750,
		.n_48bit = 6272,
		.cts_48bit = 165000,
	},
	.array[8] = {
		.tmds_clk = 148352,
		.n = 8918,
		.cts = 234375,
		.n_36bit = 17836,
		.cts_36bit = 703125,
		.n_48bit = 8918,
		.cts_48bit = 468750,
	},
	.array[9] = {
		.tmds_clk = 148500,
		.n = 6272,
		.cts = 165000,
		.n_36bit = 6272,
		.cts_36bit = 247500,
		.n_48bit = 6272,
		.cts_48bit = 330000,
	},
	.array[10] = {
		.tmds_clk = 296703,
		.n = 4459,
		.cts = 234375,
	},
	.array[11] = {
		.tmds_clk = 297000,
		.n = 4707,
		.cts = 247500,
	},
	.array[12] = {
		.tmds_clk = 593407,
		.n = 8918,
		.cts = 937500,
	},
	.array[13] = {
		.tmds_clk = 594000,
		.n = 9408,
		.cts = 990000,
	},
	.def_n = 6272,
};

/* Recommended N and Expected CTS for 48kHz and Multiples */
static const struct hdmi_audio_fs_ncts aud_48k_para = {
	.array[0] = {
		.tmds_clk = 25175,
		.n = 6864,
		.cts = 28125,
		.n_36bit = 9152,
		.cts_36bit = 58250,
		.n_48bit = 6864,
		.cts_48bit = 56250,
	},
	.array[1] = {
		.tmds_clk = 25200,
		.n = 6144,
		.cts = 25200,
		.n_36bit = 6144,
		.cts_36bit = 37800,
		.n_48bit = 6144,
		.cts_48bit = 50400,
	},
	.array[2] = {
		.tmds_clk = 27000,
		.n = 6144,
		.cts = 27000,
		.n_36bit = 6144,
		.cts_36bit = 40500,
		.n_48bit = 6144,
		.cts_48bit = 54000,
	},
	.array[3] = {
		.tmds_clk = 27027,
		.n = 6144,
		.cts = 27027,
		.n_36bit = 8192,
		.cts_36bit = 54054,
		.n_48bit = 6144,
		.cts_48bit = 54054,
	},
	.array[4] = {
		.tmds_clk = 54000,
		.n = 6144,
		.cts = 54000,
		.n_36bit = 6144,
		.cts_36bit = 81000,
		.n_48bit = 6144,
		.cts_48bit = 108000,
	},
	.array[5] = {
		.tmds_clk = 54054,
		.n = 6144,
		.cts = 54054,
		.n_36bit = 6144,
		.cts_36bit = 81081,
		.n_48bit = 6144,
		.cts_48bit = 108108,
	},
	.array[6] = {
		.tmds_clk = 74176,
		.n = 11648,
		.cts = 140625,
		.n_36bit = 11648,
		.cts_36bit = 210937,
		.n_48bit = 11648,
		.cts_48bit = 281250,
	},
	.array[7] = {
		.tmds_clk = 74250,
		.n = 6144,
		.cts = 74250,
		.n_36bit = 6144,
		.cts_36bit = 111375,
		.n_48bit = 6144,
		.cts_48bit = 148500,
	},
	.array[8] = {
		.tmds_clk = 148352,
		.n = 5824,
		.cts = 140625,
		.n_36bit = 11648,
		.cts_36bit = 421875,
		.n_48bit = 5824,
		.cts_48bit = 281250,
	},
	.array[9] = {
		.tmds_clk = 148500,
		.n = 6144,
		.cts = 148500,
		.n_36bit = 6144,
		.cts_36bit = 222750,
		.n_48bit = 6144,
		.cts_48bit = 297000,
	},
	.array[10] = {
		.tmds_clk = 296703,
		.n = 5824,
		.cts = 281250,
	},
	.array[11] = {
		.tmds_clk = 297000,
		.n = 5120,
		.cts = 247500,
	},
	.array[12] = {
		.tmds_clk = 593407,
		.n = 5824,
		.cts = 562500,
	},
	.array[13] = {
		.tmds_clk = 594000,
		.n = 6144,
		.cts = 594000,
	},
	.def_n = 6144,
};

static const struct hdmi_audio_fs_ncts *all_aud_paras[] = {
	&aud_32k_para,
	&aud_44k1_para,
	&aud_48k_para,
	NULL,
};

u32 hdmi21_get_aud_n_paras(enum hdmi_audio_fs fs,
				  enum hdmi_color_depth cd,
				  u32 tmds_clk)
{
	const struct hdmi_audio_fs_ncts *p = NULL;
	u32 i, n;
	u32 N_multiples = 1;

	pr_info("hdmitx: fs = %d, cd = %d, tmds_clk = %d\n", fs, cd, tmds_clk);
	switch (fs) {
	case FS_32K:
		p = all_aud_paras[0];
		N_multiples = 1;
		break;
	case FS_44K1:
		p = all_aud_paras[1];
		N_multiples = 1;
		break;
	case FS_88K2:
		p = all_aud_paras[1];
		N_multiples = 2;
		break;
	case FS_176K4:
		p = all_aud_paras[1];
		N_multiples = 4;
		break;
	case FS_48K:
		p = all_aud_paras[2];
		N_multiples = 1;
		break;
	case FS_96K:
		p = all_aud_paras[2];
		N_multiples = 2;
		break;
	case FS_192K:
		p = all_aud_paras[2];
		N_multiples = 4;
		break;
	default: /* Default as FS_48K */
		p = all_aud_paras[2];
		N_multiples = 1;
		break;
	}
	for (i = 0; i < AUDIO_PARA_MAX_NUM; i++) {
		if (tmds_clk == p->array[i].tmds_clk)
			break;
	}

	if (i < AUDIO_PARA_MAX_NUM)
		if (cd == COLORDEPTH_24B || cd == COLORDEPTH_30B)
			n = p->array[i].n ? p->array[i].n : p->def_n;
		else if (cd == COLORDEPTH_36B)
			n = p->array[i].n_36bit ?
				p->array[i].n_36bit : p->def_n;
		else if (cd == COLORDEPTH_48B)
			n = p->array[i].n_48bit ?
				p->array[i].n_48bit : p->def_n;
		else
			n = p->array[i].n ? p->array[i].n : p->def_n;
	else
		n = p->def_n;
	return n * N_multiples;
}

/* for csc coef */

static const u8 coef_yc444_rgb_24bit_601[] = {
	0x20, 0x00, 0x69, 0x26, 0x74, 0xfd, 0x01, 0x0e,
	0x20, 0x00, 0x2c, 0xdd, 0x00, 0x00, 0x7e, 0x9a,
	0x20, 0x00, 0x00, 0x00, 0x38, 0xb4, 0x7e, 0x3b
};

static const u8 coef_yc444_rgb_24bit_709[] = {
	0x20, 0x00, 0x71, 0x06, 0x7a, 0x02, 0x00, 0xa7,
	0x20, 0x00, 0x32, 0x64, 0x00, 0x00, 0x7e, 0x6d,
	0x20, 0x00, 0x00, 0x00, 0x3b, 0x61, 0x7e, 0x25
};

static const struct hdmi_csc_coef_table hdmi_csc_coef[] = {
	{COLORSPACE_YUV444, COLORSPACE_RGB444, COLORDEPTH_24B, 0,
		sizeof(coef_yc444_rgb_24bit_601), coef_yc444_rgb_24bit_601},
	{COLORSPACE_YUV444, COLORSPACE_RGB444, COLORDEPTH_24B, 1,
		sizeof(coef_yc444_rgb_24bit_709), coef_yc444_rgb_24bit_709},
};

bool _is_hdmi14_4k(enum hdmi_vic vic)
{
	bool ret = 0;

	switch (vic) {
	case HDMI_93_3840x2160p24_16x9:
	case HDMI_94_3840x2160p25_16x9:
	case HDMI_95_3840x2160p30_16x9:
	case HDMI_98_4096x2160p24_256x135:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

bool _is_hdmi4k_420(enum hdmi_vic vic)
{
	if ((vic & HDMITX_VIC420_OFFSET) == HDMITX_VIC420_OFFSET)
		return 1;
	return 0;
}

