// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vpp_common.h"
#include "vpp_data.h"
#include "vpp_vf_proc.h"
#include "vpp_modules_inc.h"
#include "vpp_csc.h"
#include "vpp_data_hdr.h"
#include "alg/hdr/aml_hdr10_plus.h"
#include "alg/hdr/aml_hdr10_plus_ootf.h"
#include "alg/hdr/aml_gamut_convert.h"
#include "alg/hdr/aml_hdr10_tmo.h"

#define SIG_CHG_CS                  0x01
#define SIG_CHG_SRC                 0x02
#define SIG_CHG_PRI_INFO            0x04
#define SIG_CHG_KNEE_FACTOR         0x08
#define SIG_CHG_HDR_MODE            0x10
#define SIG_CHG_HDR_SUPPORT         0x20
#define SIG_CHG_WB                  0x40
#define SIG_CHG_HLG_MODE            0x80
#define SIG_CHG_HLG_SUPPORT         0x100
#define SIG_CHG_OP                  0x200
#define SIG_CHG_SRC_OUTPUT          0x400  /*for box*/
#define SIG_CHG_HDR10_PLUS_MODE     0x800
#define SIG_CHG_COLORIMETRY_SUPPORT 0x1000
#define SIG_CHG_OUTPUT_MODE         0x2000
#define SIG_CHG_HDR_OOTF            0x4000
#define SIG_CHG_FORCE               0x8000

#define SIGNAL_FORMAT(type) ((type >> 26) & 0x7)
#define SIGNAL_RANGE(type) ((type >> 25) & 0x1)
#define SIGNAL_COLOR_PRIMARIES(type) ((type >> 16) & 0xff)
#define SIGNAL_TRANSFER_CHARACTERISTIC(type) ((type >> 8) & 0xff)

struct _overscan_data_s {
	unsigned int load_flag;
	unsigned int afd_enable;
	unsigned int screen_mode;
	enum vpp_overscan_input_e source;
	enum vpp_overscan_timing_e timing;
	unsigned int hs;
	unsigned int he;
	unsigned int vs;
	unsigned int ve;
};

struct _signal_info_s {
	unsigned int hdr_support;
	unsigned int hlg_support;
	unsigned int colorimetry_support;
	unsigned int viu_color_fmt[EN_VF_TOP_MAX];
};

static unsigned int cur_signal_type[EN_VD_PATH_MAX] = {
	0xffffffff,
	0xffffffff,
	0xffffffff
};

static unsigned int adp_scal_x_shift;
static unsigned int adp_scal_y_shift;
static unsigned int disable_flush_flag;

static struct _signal_info_s cur_signal_info;
static struct vpp_hdr_metadata_s hdr_metadata;
static struct vframe_master_display_colour_s cur_master_display_color[EN_VD_PATH_MAX];
static struct hdr10pgen_param_s hdr10p_gen_param;
static struct hdr_gamut_data_s hdr_gamut_data;
static struct hdr_proc_param_lut_s hdr_lut_param;
static struct hdr_proc_param_mtrx_s hdr_mtrx_param;
static struct hdr10plus_para tx_hdr10p_params[EN_VD_PATH_MAX];

static bool pc_mode_change;
static bool hist_sel;
static bool video_status[EN_VD_PATH_MAX] = {0};
static bool video_layer_wait_on[EN_VD_PATH_MAX] = {0};
static bool hdr_module_support[EN_MODULE_TYPE_MAX] = {0};
static int pc_mode_status;
static int fix_range_mode;
static int skip_hdr_mode;
static enum vpp_pw_state_e pre_pw_state;
static enum vpp_hdr_type_e cur_hdr_type;
static enum vpp_color_primary_e cur_color_primary;
static enum vframe_source_type_e cur_vf_src[EN_VD_PATH_MAX] = {
	VFRAME_SOURCE_TYPE_COMP,
	VFRAME_SOURCE_TYPE_COMP,
	VFRAME_SOURCE_TYPE_COMP,
};

static int overscan_status;
static int overscan_reset;
static int overscan_update_flag;
static int overscan_atv_source;
static struct _overscan_data_s overscan_data[EN_TIMING_MAX];

/*Internal functions*/
static unsigned int _log2(unsigned int value)
{
	unsigned int ret;

	for (ret = 0; value > 1; ret++)
		value >>= 1;

	return ret;
}

static struct vinfo_s *_get_vinfo(enum vpp_vf_top_e vpp_top)
{
	struct vinfo_s *pvinfo = NULL;

	if (vpp_top == EN_VF_TOP1)
		pvinfo = get_current_vinfo2();
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	else if (vpp_top == EN_VF_TOP2)
		pvinfo = get_current_vinfo3();
#endif
	else
		pvinfo = get_current_vinfo();

	return pvinfo;
}

static int _get_vinfo_lcd_support(void)
{
	struct vinfo_s *pvinfo = get_current_vinfo();

	if (pvinfo->mode == VMODE_LCD ||
		pvinfo->mode == VMODE_DUMMY_ENCP)
		return 1;
	else
		return 0;
}

static int _is_vinfo_available(const struct vinfo_s *pvinfo)
{
	int ret;

	if (!pvinfo || !pvinfo->name)
		ret = 0;
	else
		ret = strcmp(pvinfo->name, "invalid") &&
			strcmp(pvinfo->name, "null") &&
			strcmp(pvinfo->name, "576cvbs") &&
			strcmp(pvinfo->name, "470cvbs");

	return ret;
}

static bool _is_video_layer_on(enum vpp_vd_path_e vd_path)
{
	bool video_on = false;

	/*
	 *if (vd_path == EN_VD1_PATH)
	 *	video_on = get_video_enabled();
	 *else if (vd_path == EN_VD2_PATH)
	 *	video_on = get_videopip_enabled();
	 *else if (vd_path == EN_VD3_PATH)
	 *	video_on = get_videopip2_enabled();
	 *else
	 *	video_on = false;

	 *if (video_on)
	 *	video_layer_wait_on[vd_path] = false;
	 */

	return video_on || video_layer_wait_on[vd_path];
}

static int _is_video_turn_on(bool *pvd_on, enum vpp_vd_path_e vd_path)
{
	int ret = 0;
	bool tmp = _is_video_layer_on(vd_path);

	if (!pvd_on)
		return ret;

	if (pvd_on[vd_path] != tmp) {
		pvd_on[vd_path] = tmp;
		ret = pvd_on[vd_path] ? 1 : -1;
	}

	return ret;
}

static unsigned int _update_signal_type(struct vframe_s *pvf)
{
	unsigned int signal_type = 0;
	unsigned int present_flag = 1;                  /*video available*/
	unsigned int video_format = 5;                  /*unspecified*/
	unsigned int range = 0;                         /*limited*/
	unsigned int color_description_present_flag = 1;/*color available*/
	unsigned int color_primaries = 1;               /*bt709*/
	unsigned int transfer_characteristic = 1;       /*bt709*/
	unsigned int matrix_coefficient = 1;            /*bt709*/
	unsigned int tmp = 0;

	if (!pvf)
		return 0;

	if (pvf->signal_type & (1 << 29)) {
		signal_type = pvf->signal_type;
	} else {
		if (pvf->source_type == VFRAME_SOURCE_TYPE_TUNER ||
			pvf->source_type == VFRAME_SOURCE_TYPE_CVBS ||
			pvf->source_type == VFRAME_SOURCE_TYPE_COMP ||
			pvf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
			if (fix_range_mode == 0)
				range = pvf->signal_type & (1 << 25);
			else
				range = 1;/*full*/
		} else {/*for multimedia*/
			if (pvf->height < 720) {
				/*SD default 601 limited*/
				color_primaries = 3;
				transfer_characteristic = 3;
				matrix_coefficient = 3;
			}
		}

		signal_type = (present_flag << 29) |
			(video_format << 26) |
			(range << 25) |
			(color_description_present_flag << 24) |
			(color_primaries << 16) |
			(transfer_characteristic << 8) |
			matrix_coefficient;
	}

	/*customer overwrite*/
	tmp = pvf->prop.master_display_colour.present_flag & 0x80000000;
	if (vpp_csc_get_customer_master_display_en() && tmp == 0)
		signal_type = (1 << 29) |
			(5 << 26) |
			(0 << 25) |
			(1 << 24) |
			(9 << 16) |
			(16 << 8) |
			(9 << 0);

	if (skip_hdr_mode > 0) {
		if ((signal_type & 0xff) != 1 &&
			(signal_type & 0xff) != 3) {
			signal_type &= 0xffffff00;
			signal_type |= 0x00000001;
		}

		if ((signal_type & 0xff00) >> 8 != 1 &&
			(signal_type & 0xff00) >> 8 != 3) {
			signal_type &= 0xffff00ff;
			signal_type |= 0x00000100;
		}

		if ((signal_type & 0xff0000) >> 16 != 1 &&
			(signal_type & 0xff0000) >> 16 != 3) {
			signal_type &= 0xff00ffff;
			signal_type |= 0x00010000;
		}
	}

	return signal_type;
}

static void _update_hdr_type(unsigned int signal_type)
{
	unsigned int cp = SIGNAL_COLOR_PRIMARIES(signal_type);
	unsigned int tc = SIGNAL_TRANSFER_CHARACTERISTIC(signal_type);

	/*hdr type*/
	if ((tc == 14 || tc == 18) && cp == 9)
		cur_hdr_type = EN_TYPE_HLG;
	else if (tc == 0x30 && cp == 9)
		cur_hdr_type = EN_TYPE_HDR10PLUS;
	else if (tc == 16)
		cur_hdr_type = EN_TYPE_HDR10;
	else
		cur_hdr_type = EN_TYPE_SDR;

	/*color primary*/
	if (cp == 1)
		cur_color_primary = EN_COLOR_PRI_BT709;
	else if (cp == 3)
		cur_color_primary = EN_COLOR_PRI_BT601;
	else if (cp == 9)
		cur_color_primary = EN_COLOR_PRI_BT2020;
	else
		cur_color_primary = EN_COLOR_PRI_NULL;
}

static void _update_hdr_metadata(struct vframe_s *pvf)
{
	unsigned int cp = SIGNAL_COLOR_PRIMARIES(pvf->signal_type);
	struct vframe_master_display_colour_s *ptmp;

	if (cp == 9) {
		ptmp = &pvf->prop.master_display_colour;
		if (ptmp->present_flag) {
			memcpy(hdr_metadata.primaries,
				ptmp->primaries, sizeof(u32) * 6);
			memcpy(hdr_metadata.white_point,
				ptmp->white_point, sizeof(u32) * 2);
			hdr_metadata.luminance[0] = ptmp->luminance[0];
			hdr_metadata.luminance[1] = ptmp->luminance[1];
		} else {
			memset(hdr_metadata.primaries, 0, sizeof(hdr_metadata.primaries));
		}
	} else {
		memset(hdr_metadata.primaries, 0, sizeof(hdr_metadata.primaries));
	}
}

int _get_primaries_type(struct vframe_master_display_colour_s *p_mdc)
{
	if (!p_mdc->present_flag)
		return 0;

	if (p_mdc->primaries[0][1] > p_mdc->primaries[1][1] &&
		p_mdc->primaries[0][1] > p_mdc->primaries[2][1] &&
		p_mdc->primaries[2][0] > p_mdc->primaries[0][0] &&
		p_mdc->primaries[2][0] > p_mdc->primaries[1][0]) {
		return 2; /*reasonable g,b,r*/
	} else if (p_mdc->primaries[0][0] > p_mdc->primaries[1][0] &&
		p_mdc->primaries[0][0] > p_mdc->primaries[2][0] &&
		p_mdc->primaries[1][1] > p_mdc->primaries[0][1] &&
		p_mdc->primaries[1][1] > p_mdc->primaries[2][1]) {
		return 1; /*reasonable r,g,b*/
	}

	return 0; /*source not usable, use standard bt2020*/
}

enum vpp_csc_type_e _get_csc_type(enum vpp_vd_path_e vd_path)
{
	unsigned int type = 0;
	unsigned int format = 0;
	unsigned int range = 0;
	unsigned int color_primaries = 0;
	unsigned int transfer_characteristic = 0;
	enum vpp_csc_type_e csc_type = EN_CSC_MATRIX_NULL;

	if (vd_path == EN_VD_PATH_MAX)
		return csc_type;

	type = cur_signal_type[vd_path];
	format = SIGNAL_FORMAT(type);
	range = SIGNAL_RANGE(type);
	color_primaries = SIGNAL_COLOR_PRIMARIES(type);
	transfer_characteristic = SIGNAL_TRANSFER_CHARACTERISTIC(type);

	if (color_primaries == 1 && transfer_characteristic < 14) {
		if (range == 0)
			csc_type = EN_CSC_MATRIX_YUV709_RGB;
		else
			csc_type = EN_CSC_MATRIX_YUV709F_RGB;
	} else if ((color_primaries == 3) && (transfer_characteristic < 14)) {
		if (range == 0)
			csc_type = EN_CSC_MATRIX_YUV601_RGB;
		else
			csc_type = EN_CSC_MATRIX_YUV601F_RGB;
	} else if ((color_primaries == 9) || (transfer_characteristic >= 14)) {
		if (transfer_characteristic == 16) { /*smpte st-2084*/
			csc_type = EN_CSC_MATRIX_BT2020YUV_BT2020RGB;
			if (color_primaries != 9)
				pr_vpp(PR_DEBUG,
					"[%s] \tWARNING: non-standard HDR!!!\n", __func__);
		} else if (transfer_characteristic == 14 ||
			transfer_characteristic == 18) { /*14: bt2020-10, 18: bt2020-12*/
			csc_type = EN_CSC_MATRIX_BT2020YUV_BT2020RGB;
		} else if (transfer_characteristic == 15) { /*bt2020-12*/
			if (range == 0)
				csc_type = EN_CSC_MATRIX_YUV709_RGB;
			else
				csc_type = EN_CSC_MATRIX_YUV709F_RGB;
			pr_vpp(PR_DEBUG,
				"[%s] \tWARNING: bt2020-12 HDR!!!\n", __func__);
		} else if (transfer_characteristic == 48) {
			if (color_primaries == 9) {
				csc_type = EN_CSC_MATRIX_BT2020YUV_BT2020RGB_DYNAMIC;
			} else {
				csc_type = EN_CSC_MATRIX_BT2020YUV_BT2020RGB;
				pr_vpp(PR_DEBUG,
					"[%s] \tWARNING: non-standard HDR10+!!!\n", __func__);
			}
		} else { /*unknown transfer characteristic*/
			if (range == 0)
				csc_type = EN_CSC_MATRIX_YUV709_RGB;
			else
				csc_type = EN_CSC_MATRIX_YUV709F_RGB;
			pr_vpp(PR_DEBUG, "[%s] \tWARNING: unknown HDR!!!\n", __func__);
		}
	} else {
		if (format == 0)
			pr_vpp(PR_DEBUG,
				"[%s] \tWARNING: unknown colour space!!!\n", __func__);

		if (range == 0)
			csc_type = EN_CSC_MATRIX_YUV601_RGB;
		else
			csc_type = EN_CSC_MATRIX_YUV601F_RGB;
	}

	return csc_type;
}

enum vpp_csc_type_e _get_csc_type_by_timing(struct vframe_s *pvf,
	struct vinfo_s *pvinfo)
{
	enum vpp_csc_type_e csc_type = EN_CSC_MATRIX_NULL;
	unsigned int width, height;

	if (!_get_vinfo_lcd_support()) {
		width = (pvf->type & VIDTYPE_COMPRESS) ?
			pvf->compWidth : pvf->width;
		height = (pvf->type & VIDTYPE_COMPRESS) ?
			pvf->compHeight : pvf->height;

		if (height < 720 && pvinfo->height >= 720)
			csc_type = EN_CSC_MATRIX_YUV601L_YUV709L;
		else if (height >= 720 && pvinfo->height < 720)
			csc_type = EN_CSC_MATRIX_YUV709L_YUV601L;
		else
			csc_type = EN_CSC_MATRIX_NULL;
	}

	return csc_type;
}

/*t3 for power consumption, disable clock
 *modules only before post blend can be disable
 *because after postblend it used for color temperature
 *or color correction
 */
static void _enhance_clock_ctrl(int val)
{
	vpp_module_ve_set_clock_ctrl(EN_CLK_BLUE_STRETCH, val);
	vpp_module_ve_set_clock_ctrl(EN_CLK_CM, (val | (val << 2)));
	vpp_module_ve_set_clock_ctrl(EN_CLK_CCORING, val);
	vpp_module_ve_set_clock_ctrl(EN_CLK_BLKEXT, val);
}

static void _set_enhance_clock(struct vframe_s *pvf,
	struct vframe_s *prpt_vf)
{
	/*cm/blue stretch/black extension/chroma coring*/
	/*other modules(sr0/sr1/dnlp/lc disable in amvideo)*/
	/*en = 1: disable clock, 0: enable clock*/
	if (pvf || prpt_vf) {
		if (pre_pw_state != EN_PW_ON) {
			_enhance_clock_ctrl(EN_PW_ON);
			pr_vpp(PR_DEBUG, "[%s] PW_ON: pre_state: %d\n", __func__, pre_pw_state);
			pre_pw_state = EN_PW_ON;
		}
	} else {
		if (pre_pw_state != EN_PW_OFF) {
			_enhance_clock_ctrl(EN_PW_ON);
			pr_vpp(PR_DEBUG, "[%s] PW_OFF: pre_state: %d\n", __func__, pre_pw_state);
			pre_pw_state = EN_PW_OFF;
		}
	}
}

static void _pc_mode_proc(int mode)
{
}

static int _hdr10_primaries_changed(struct vframe_master_display_colour_s *pnew,
	struct vframe_master_display_colour_s *pcur)
{
	struct vframe_content_light_level_s *p_cll =
		&pnew->content_light_level;
	struct vframe_master_display_colour_s *p = pcur;
	unsigned int (*prim_mdc)[2] = pnew->primaries;
	unsigned int (*prim_p)[2] = p->primaries;
	unsigned int max_lum = 1000 * 10000;
	unsigned int min_lum = 50;
	int primaries_type = 0;
	int i, j;
	int flag = 0;

	primaries_type = _get_primaries_type(pnew);
	if (primaries_type == 0) {
		if ((p->luminance[0] != max_lum &&
			p->luminance[0] != max_lum / 10000) ||
			p->luminance[1] != min_lum) {
			p->luminance[0] = max_lum;
			p->luminance[1] = min_lum;
			flag |= 1;
		}

		for (i = 0; i < 2; i++) {
			if (p->white_point[i] != white_point_bt2020[i]) {
				p->white_point[i] = white_point_bt2020[i];
				flag |= 4;
			}
		}

		for (i = 0; i < 3; i++) { /*GBR -> GBR*/
			for (j = 0; j < 2; j++) {
				if (prim_p[i][j] != primaries_bt2020[i][j]) {
					prim_p[i][j] = primaries_bt2020[i][j];
					flag |= 2;
				}
			}
		}

		p->present_flag = 1;
	} else {
		for (i = 0; i < 2; i++) {
			if (p->luminance[i] != pnew->luminance[i]) {
				if (i != 0 ||
					p->luminance[0] != pnew->luminance[0] / 10000)
					flag |= 1;
				p->luminance[i] = pnew->luminance[i];
			}

			if (p->white_point[i] != pnew->white_point[i]) {
				p->white_point[i] = pnew->white_point[i];
				flag |= 4;
			}
		}

		if (primaries_type == 1) { /*RGB -> GBR*/
			for (i = 0; i < 3; i++) {
				for (j = 0; j < 2; j++) {
					if (prim_p[i][j] != prim_mdc[(i + 2) % 3][j]) {
						prim_p[i][j] = prim_mdc[(i + 2) % 3][j];
						flag |= 2;
					}
				}
			}

			p->present_flag = 0x21;
		} else if (primaries_type == 2) { /*GBR -> GBR*/
			for (i = 0; i < 3; i++) {
				for (j = 0; j < 2; j++) {
					if (prim_p[i][j] != prim_mdc[i][j]) {
						prim_p[i][j] = prim_mdc[i][j];
						flag |= 2;
					}
				}
			}

			p->present_flag = 0x11;
		}
	}

	if (p_cll->present_flag) {
		if (p->content_light_level.max_content != p_cll->max_content ||
			p->content_light_level.max_pic_average != p_cll->max_pic_average)
			flag |= 8;

		if (flag & 8) {
			p->content_light_level.max_content = p_cll->max_content;
			p->content_light_level.max_pic_average = p_cll->max_pic_average;
		}

		p->content_light_level.present_flag = 1;
	} else {
		if (p->content_light_level.max_content != 0 ||
			p->content_light_level.max_pic_average != 0) {
			p->content_light_level.max_content = 0;
			p->content_light_level.max_pic_average = 0;
		}

		if (p->content_light_level.present_flag)
			flag |= 8;

		p->content_light_level.present_flag = 0;
	}

	return flag;
}

static unsigned int _sink_dv_support(const struct vinfo_s *pvinfo)
{
	unsigned int ret = 0;

	if (!pvinfo || !pvinfo->name ||
		!pvinfo->vout_device ||
		!pvinfo->vout_device->dv_info ||
		pvinfo->vout_device->dv_info->ieeeoui != 0x00d046 ||
		pvinfo->vout_device->dv_info->block_flag != CORRECT ||
		pvinfo->height != pvinfo->field_height)
		return ret;

	/* for sink not support 60 dovi */
	if (strstr(pvinfo->name, "2160p60hz") ||
		strstr(pvinfo->name, "2160p50hz")) {
		if (!pvinfo->vout_device->dv_info->sup_2160p60hz)
			return ret;
	}
	/* for sink not support 120hz*/
	if (strstr(pvinfo->name, "120hz") ||
	    strstr(pvinfo->name, "100hz")) {
		if (!pvinfo->vout_device->dv_info->sup_1080p120hz)
			return 0;
	}
	/* currently all sink not support 4k100/120 and 8k dv */
	if (strstr(pvinfo->name, "2160p100hz") ||
	    strstr(pvinfo->name, "2160p120hz") ||
	    strstr(pvinfo->name, "3840x2160p100hz") ||
	    strstr(pvinfo->name, "3840x2160p120hz") ||
	    strstr(pvinfo->name, "7680x4320p")) {
	    /*in the future, some new flag in vsvdb will be used to judge dv cap*/
		return 0;
	}
	/* for interlace output */
	if (pvinfo->height != pvinfo->field_height)
		return 0;
	if (pvinfo->vout_device->dv_info->ver == 2) {
		if (pvinfo->vout_device->dv_info->Interface <= 1)
			ret = 2; /*LL only*/
		else
			ret = 3; /*STD & LL*/
	} else if (pvinfo->vout_device->dv_info->low_latency) {
		ret = 3; /*STD & LL*/
	} else {
		ret = 1; /*STD only*/
	}

	return ret;
}

static unsigned int _sink_hdr_support(const struct vinfo_s *pvinfo)
{
	unsigned int hdr_cap = 0;
	unsigned int dv_cap = 0;

	/*when policy == follow sink(0) or force output(2)*/
	/*use force_output*/
	if (vpp_csc_get_hdr_policy() != 1) {
		switch (vpp_csc_get_force_output()) {
		case EN_BT2020:
			hdr_cap |= SUPPORT_BT2020;
			break;
		case EN_BT2020_PQ:
			hdr_cap |= SUPPORT_HDR;
			break;
		case EN_BT2020_PQ_DYNAMIC:
			hdr_cap |= SUPPORT_HDRP;
			break;
		case EN_BT2020_HLG:
			hdr_cap |= SUPPORT_HLG;
			break;
		case EN_BT2100_IPT:
			hdr_cap |= SUPPORT_DV;
		default:
			break;
		}
	} else if (pvinfo) {
		if (pvinfo->hdr_info.hdr_support & SUPPORT_HDR)
			hdr_cap |= SUPPORT_HDR;

		if (pvinfo->hdr_info.hdr_support & SUPPORT_HLG)
			hdr_cap |= SUPPORT_HLG;

		if (pvinfo->hdr_info.hdr10plus_info.ieeeoui == 0x90848b &&
			pvinfo->hdr_info.hdr10plus_info.application_version == 1)
			hdr_cap |= SUPPORT_HDRP;

		if (pvinfo->hdr_info.colorimetry_support & 0xe0)
			hdr_cap |= SUPPORT_BT2020;

		dv_cap = _sink_dv_support(pvinfo);
		if (dv_cap)
			hdr_cap |= (dv_cap << SUPPORT_DV_SHF) & SUPPORT_DV;
	}

	return hdr_cap;
}

static int _check_primaries(unsigned int hdr_flag_val,
	unsigned int (*p)[3][2],/*src primaries*/
	unsigned int (*w)[2],/*src white point*/
	const struct vinfo_s *v,/*dst display info from vinfo*/
	s64 (*si)[4][2], s64 (*di)[4][2])/*prepare src and dst color info array*/
{
	int i, j;
	int need_calculate_mtrx = 1; /*always calculate to apply scale factor*/
	const struct master_display_info_s *d;

	if (!p || !w || !v || !si || !di)
		return 0;

	/*check and copy primaries*/
	if (hdr_flag_val & 1) {
		if (((*p)[0][1] > (*p)[1][1]) &&
			((*p)[0][1] > (*p)[2][1]) &&
			((*p)[2][0] > (*p)[0][0]) &&
			((*p)[2][0] > (*p)[1][0])) {/*reasonable g,b,r*/
			for (i = 0; i < 3; i++) {
				for (j = 0; j < 2; j++) {
					(*si)[i][j] = (*p)[(i + 2) % 3][j];
					if ((*si)[i][j] != primaries_bt2020[(i + 2) % 3][j])
						need_calculate_mtrx = 1;
				}
			}
		} else if (((*p)[0][0] > (*p)[1][0]) &&
			((*p)[0][0] > (*p)[2][0]) &&
			((*p)[1][1] > (*p)[0][1]) &&
			((*p)[1][1] > (*p)[2][1])) {/*reasonable r,g,b*/
			for (i = 0; i < 3; i++) {
				for (j = 0; j < 2; j++) {
					(*si)[i][j] = (*p)[i][j];
					if ((*si)[i][j] != primaries_bt2020[(i + 2) % 3][j])
						need_calculate_mtrx = 1;
				}
			}
		} else {/*source not usable, use standard bt2020*/
			for (i = 0; i < 3; i++)
				for (j = 0; j < 2; j++)
					(*si)[i][j] = primaries_bt2020[(i + 2) % 3][j];
		}

		/*check white point*/
		if (need_calculate_mtrx == 1) {
			if (((*w)[0] > (*si)[2][0]) &&
				((*w)[0] < (*si)[0][0]) &&
				((*w)[1] > (*si)[2][1]) &&
				((*w)[1] < (*si)[1][1])) {
				for (i = 0; i < 2; i++)
					(*si)[3][i] = (*w)[i];
			} else {
				for (i = 0; i < 3; i++)
					for (j = 0; j < 2; j++)
						(*si)[i][j] = primaries_bt2020[(i + 2) % 3][j];

				for (i = 0; i < 2; i++)
					(*si)[3][i] = white_point_bt2020[i];
				/*need_calculate_mtx = 0;*/
			}
		} else {
			if (((*w)[0] > (*si)[2][0]) &&
				((*w)[0] < (*si)[0][0]) &&
				((*w)[1] > (*si)[2][1]) &&
				((*w)[1] < (*si)[1][1])) {
				for (i = 0; i < 2; i++) {
					(*si)[3][i] = (*w)[i];
					if ((*si)[3][i] != white_point_bt2020[i])
						need_calculate_mtrx = 1;
				}
			} else {
				for (i = 0; i < 2; i++)
					(*si)[3][i] = white_point_bt2020[i];
			}
		}
	} else {/*use standard bt2020*/
		for (i = 0; i < 3; i++)
			for (j = 0; j < 2; j++)
				(*si)[i][j] = primaries_bt2020[(i + 2) % 3][j];

		for (i = 0; i < 2; i++)
			(*si)[3][i] = white_point_bt2020[i];
	}

	/*check display*/
	if (v->master_display_info.present_flag && (hdr_flag_val & 2)) {
		d = &v->master_display_info;

		for (i = 0; i < 3; i++) {
			for (j = 0; j < 2; j++) {
				(*di)[i][j] = d->primaries[(i + 2) % 3][j];
				if ((*di)[i][j] != primaries_bt709[(i + 2) % 3][j])
					need_calculate_mtrx = 1;
			}
		}

		for (i = 0; i < 2; i++) {
			(*di)[3][i] = d->white_point[i];
			if ((*di)[3][i] != white_point_bt709[i])
				need_calculate_mtrx = 1;
		}
	} else {
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 2; j++)
				(*di)[i][j] = primaries_bt709[(i + 2) % 3][j];
		}

		for (i = 0; i < 2; i++)
			(*di)[3][i] = white_point_bt709[i];
	}

	return need_calculate_mtrx;
}

static enum vpp_csc_type_e _prepare_customer_mtrx(unsigned int hdr_flag_val,
	unsigned int (*s)[3][2],/*src primaries*/
	unsigned int (*w)[2],/*src white point*/
	const struct vinfo_s *v,/*vinfo carry display primaries*/
	struct csc_mtrx_param_s *m, bool inverse_flag)
{
	s64 prmy_src[4][2];
	s64 prmy_dst[4][2];
	/*s64 out[3][3];*/
	int i, j;
	enum vpp_csc_type_e ret = EN_CSC_MATRIX_BT2020YUV_BT2020RGB;

	if (!s || !w || !v || !m)
		return ret;

	if (vpp_csc_get_customer_mtrx_en() &&
		vpp_csc_get_customer_mtrx_used()) {
		for (i = 0; i < 3; i++) {
			m->pre_offset[i] = customer_mtrx_param[i];
			m->offset[i] = customer_mtrx_param[12 + i];

			for (j = 0; j < 3; j++)
				m->coef[i][j] = customer_mtrx_param[3 + i * 3 + j];
		}

		m->right_shift = customer_mtrx_param[15];
		ret = EN_CSC_MATRIX_BT2020RGB_CUSRGB;
	} else {
		_check_primaries(hdr_flag_val, s, w, v, &prmy_src, &prmy_dst);
		/*
		 *if (_check_primaries(hdr_flag_val, s, w, v, &prmy_src, &prmy_dst)) {
		 *	if (inverse_flag)
		 *		vpp_csc_calculate_gamut_mtrx(prmy_dst, prmy_src, out, INORM, BL);
		 *	else
		 *		vpp_csc_calculate_gamut_mtrx(prmy_src, prmy_dst, out, INORM, BL);

		 *	vpp_csc_calculate_mtrx_setting(out, BL, 13, m);
		 *}
		 */

		ret = EN_CSC_MATRIX_BT2020RGB_CUSRGB;
	}

	return ret;
}

static void _cp_hdr_info(struct master_display_info_s *hdr_data,
	struct vframe_master_display_colour_s *p)
{
	int i, j;

	if (!hdr_data || !p)
		return;

	if (vpp_csc_get_customer_hdmi_display_en()) {
		hdr_data->features = (1 << 29) | /*video available*/
			(5 << 26) | /*unspecified*/
			(0 << 25) | /*limit*/
			(1 << 24) | /*color available*/
			(customer_hdmi_display_param[0] << 16) | /*bt2020*/
			(customer_hdmi_display_param[1] << 8) | /*2084*/
			(10 << 0); /*bt2020c*/

		memcpy(hdr_data->primaries, &customer_hdmi_display_param[2],
			sizeof(unsigned int) * 6);
		memcpy(hdr_data->white_point, &customer_hdmi_display_param[8],
			sizeof(unsigned int) * 2);

		hdr_data->luminance[0] = customer_hdmi_display_param[10];
		hdr_data->luminance[1] = customer_hdmi_display_param[11];
		hdr_data->max_content = customer_hdmi_display_param[12];
		hdr_data->max_frame_average = customer_hdmi_display_param[13];
	} else if ((((hdr_data->features >> 16) & 0xff) == 9) ||
		((((hdr_data->features >> 8) & 0xff) >= 14) &&
		(((hdr_data->features >> 8) & 0xff) <= 18))) {
		if (p->present_flag & 1) {
			memcpy(hdr_data->primaries, p->primaries,
				sizeof(unsigned int) * 6);
			memcpy(hdr_data->white_point, p->white_point,
				sizeof(unsigned int) * 2);

			hdr_data->luminance[0] = p->luminance[0];
			hdr_data->luminance[1] = p->luminance[1];

			if (p->content_light_level.present_flag == 1) {
				hdr_data->max_content =
					p->content_light_level.max_content;
				hdr_data->max_frame_average =
					p->content_light_level.max_pic_average;
			} else {
				hdr_data->max_content = 0;
				hdr_data->max_frame_average = 0;
			}
		} else {
			for (i = 0; i < 3; i++)
				for (j = 0; j < 2; j++)
					hdr_data->primaries[i][j] = primaries_bt2020[i][j];

			hdr_data->white_point[0] = white_point_bt709[0];
			hdr_data->white_point[1] = white_point_bt709[1];

			/*default luminance*/
			hdr_data->luminance[0] = 1000 * 10000;
			hdr_data->luminance[1] = 50;

			/*content_light_level*/
			hdr_data->max_content = 0;
			hdr_data->max_frame_average = 0;
		}

		hdr_data->luminance[0] = hdr_data->luminance[0] / 10000;
		hdr_data->present_flag = 1;
	} else {
		memset(hdr_data->primaries, 0, sizeof(hdr_data->primaries));
	}
}

static int _detect_signal_type(struct vframe_s *pvf,
	struct vinfo_s *pvinfo, enum vpp_vd_path_e vd_path,
	enum vpp_vf_top_e vpp_top)
{
	int i = 0;
	int signal_type_change = 0;
	unsigned int signal_type = 0;
	unsigned int tmp = 0;
	struct vframe_master_display_colour_s *p_cur = NULL;
	struct vframe_master_display_colour_s *p_new = NULL;
	struct vframe_master_display_colour_s cus;

	if (!pvf || !pvinfo)
		return 0;

	p_new = &pvf->prop.master_display_colour;
	p_cur = &cur_master_display_color[vd_path];

	tmp = pvf->prop.master_display_colour.present_flag & 0x80000000;
	if (vpp_csc_get_customer_master_display_en() && tmp == 0) {
		for (i = 0; i < 3; i++) {
			cus.primaries[i][0] = customer_master_display_param[i * 2];
			cus.primaries[i][1] = customer_master_display_param[i * 2 + 1];
		}

		for (i = 0; i < 2; i++)
			cus.white_point[i] = customer_master_display_param[6 + i];

		for (i = 0; i < 2; i++)
			cus.luminance[i] = customer_master_display_param[8 + i];

		cus.present_flag = 1;
		p_new = &cus;
	}

	signal_type = _update_signal_type(pvf);
	tmp = (signal_type >> 16) & 0xff;

	/*only check primary for new = bt2020 or cur = bt2020*/
	if (p_new && p_cur && (tmp == 9 || p_cur->present_flag)) {
		tmp = _hdr10_primaries_changed(p_new, p_cur);
		if (tmp)
			signal_type_change |= SIG_CHG_PRI_INFO;
	}

	if (cur_signal_type[vd_path] != signal_type) {
		cur_signal_type[vd_path] = signal_type;
		signal_type_change |= SIG_CHG_CS;
	}

	if (cur_vf_src[vd_path] != pvf->source_type) {
		cur_vf_src[vd_path] = pvf->source_type;
		signal_type_change |= SIG_CHG_SRC;
	}

	tmp = pvinfo->hdr_info.hdr_support & 0x4;
	if (cur_signal_info.hdr_support != tmp) {
		cur_signal_info.hdr_support = tmp;
		signal_type_change |= SIG_CHG_HDR_SUPPORT;
	}

	tmp = pvinfo->hdr_info.hdr_support & 0x8;
	if (cur_signal_info.hlg_support != tmp) {
		cur_signal_info.hlg_support = tmp;
		signal_type_change |= SIG_CHG_HLG_SUPPORT;
	}

	tmp = pvinfo->hdr_info.colorimetry_support;
	if (cur_signal_info.colorimetry_support != tmp) {
		cur_signal_info.colorimetry_support = tmp;
		signal_type_change |= SIG_CHG_COLORIMETRY_SUPPORT;
	}

	tmp = pvinfo->viu_color_fmt;
	if (cur_signal_info.viu_color_fmt[vpp_top] != tmp) {
		cur_signal_info.viu_color_fmt[vpp_top] = tmp;
		signal_type_change |= SIG_CHG_OP;
	}

	return signal_type_change;
}

static int _update_hdr10_tmo(enum hdr_module_type_e module,
	enum vpp_vf_top_e vpp_top)
{
	enum hdr_vpp_type_e vpp_sel = EN_TYPE_VPP0;

	if (disable_flush_flag)
		return 0;

	switch (vpp_top) {
	case EN_VF_TOP0:
	default:
		vpp_sel = EN_TYPE_VPP0;
		break;
	case EN_VF_TOP1:
		vpp_sel = EN_TYPE_VPP1;
		break;
	case EN_VF_TOP2:
		vpp_sel = EN_TYPE_VPP2;
		break;
	}

	vpp_module_hdr_set_lut(module, EN_SUB_MODULE_OGAIN,
		&data_oo_y_lut_hdr_sdr[0], vpp_sel);

	return 0;
}

static void _hdr10_tmo_update_proc(unsigned int *pluminance,
	enum vpp_vd_path_e vd_path, enum vpp_vf_top_e vpp_top)
{
	int i = 0;
	int tmp = 0;
	int index = 0;
	int latest_hist_diff = 0;
	int latest_hist_sum = 0;
	enum hdr_module_type_e module;
	struct hdr_hist_report_s *phdr_hist;

	if (!pluminance)
		return;

	phdr_hist = vpp_module_hdr_get_hist_report();
	index = HDR_HIST_BACKUP_COUNT - 1;

	for (i = 0; i < VPP_HDR_HIST_BIN_COUNT; i++) {
		tmp = ABS(phdr_hist->hist_buff[index][i] -
			phdr_hist->hist_buff[index - 1][i]);
		latest_hist_diff += tmp;
		latest_hist_sum += phdr_hist->hist_buff[index][i];
	}

	hdr10_tmo_dynamic_proc(&data_oo_y_lut_hdr_sdr[0],
		&data_oo_y_lut_hdr_sdr_def[0],
		&phdr_hist->percentile[0],
		&phdr_hist->hist_buff[index][0],
		latest_hist_diff,
		latest_hist_sum,
		pluminance);

	if (vd_path == EN_VD1_PATH)
		module = EN_MODULE_TYPE_VD1;
	else if (vd_path == EN_VD2_PATH)
		module = EN_MODULE_TYPE_VD2;
	else if (vd_path == EN_VD3_PATH)
		module = EN_MODULE_TYPE_VD3;
	else
		return;

	_update_hdr10_tmo(module, vpp_top);
}

static bool _update_hdr10p_metadata(struct vframe_s *pvf,
	enum vpp_csc_type_e csc_type, int tx_support_hdr10p,
	struct hdr10plus_para *pdata)
{
	if (!pvf || csc_type != EN_CSC_MATRIX_BT2020YUV_BT2020RGB_DYNAMIC)
		return false;

	hdr10p_parser_metadata(pvf);

	if (tx_support_hdr10p) {
		if (!pdata)
			return false;

		hdr10p_hdmitx_vsif_parser(pvf, pdata);
	}

	return true;
}

static void _set_hdr10p_gamut_param(enum hdr_module_type_e module,
	struct hdr_proc_param_mtrx_s *phdr_mtrx_param,
	struct hdr_proc_param_lut_s *phdr_lut_param,
	struct hdr10pgen_param_s *p_hdr10pgen_param,
	enum vpp_vf_top_e vpp_top)
{
	int i = 0;
	int tmp = 0;
	int shift_x = 0;
	int shift_y = 0;
	int scale_shift = 0;
	int ogain_lut_148;
	bool eo_gmt_bit_mode;
	enum hdr_vpp_type_e vpp_sel = EN_TYPE_VPP0;

	if (!phdr_mtrx_param || !phdr_lut_param)
		return;

	switch (vpp_top) {
	case EN_VF_TOP0:
	default:
		vpp_sel = EN_TYPE_VPP0;
		break;
	case EN_VF_TOP1:
		vpp_sel = EN_TYPE_VPP1;
		break;
	case EN_VF_TOP2:
		vpp_sel = EN_TYPE_VPP2;
		break;
	}

	ogain_lut_148 = phdr_lut_param->ogain_lut[SIZE_OOTF_LUT - 1];
	eo_gmt_bit_mode = phdr_mtrx_param->gmt_bit_mode;

	memset(&hdr_gamut_data, 0, sizeof(struct hdr_gamut_data_s));

	for (i = 0; i < SIZE_COEF_3X3; i++)
		hdr_gamut_data.gamut_coef[i] = phdr_mtrx_param->mtrx_gamut[i];

	if (eo_gmt_bit_mode) {
		hdr_gamut_data.gamut_shift = 8;
		/*gamut shift bit for used for enable oo 33bit*/
		/*after tm2 revb fix 32bit bug*/
		hdr_gamut_data.gamut_shift |= 1 << 4;
	} else {/*use integer mode for gamut coeff*/
		hdr_gamut_data.gamut_shift = 0;
	}

	for (i = 0; i < SIZE_OFFSET; i++)
		hdr_gamut_data.cgain_coef[i] =
			phdr_mtrx_param->mtrx_cgain[i] << 2;

	/*0: adaptive scaler mode(Ys); 1: max linear(RGB max)*/
	/*2: none linear Ys -- Do NOT use it*/
	hdr_gamut_data.adpscl_mode = 1;

	shift_x = phdr_lut_param->adp_scal_x_shift;
	shift_y = 1 << phdr_lut_param->adp_scal_y_shift;
	for (i = 0; i < SIZE_OFFSET; i++) {
		if (phdr_mtrx_param->mtrx_only == 1)
			hdr_gamut_data.adpscl_bypass[i] = 1;
		else
			hdr_gamut_data.adpscl_bypass[i] = 0;

		hdr_gamut_data.adpscl_alpha[i] = shift_y;

		if (hdr_gamut_data.adpscl_mode == 1)
			hdr_gamut_data.adpscl_ys_coef[i] = 1 << shift_x;
		else
			hdr_gamut_data.adpscl_ys_coef[i] =
				phdr_lut_param->ys_coef[i] >> (10 - shift_x);

		hdr_gamut_data.adpscl_beta_s[i] = 0;
		hdr_gamut_data.adpscl_beta[i] = 0;
	}

	shift_x = phdr_lut_param->adp_scal_x_shift;
	shift_y = OO_NOR;
	tmp = 1 << OO_NOR;
	/*shift0 is for x coordinate*/
	/*shift1 is for scale multiple*/
	if (p_hdr10pgen_param)
		scale_shift = _log2(tmp /
			p_hdr10pgen_param->gain[SIZE_OOTF_LUT - 1]);
	else
		scale_shift = _log2(tmp / ogain_lut_148);

	if (eo_gmt_bit_mode) {
		shift_y -= scale_shift;
	} else {/*because input 1/2, shift0/shift1 need change*/
		shift_x -= 1;
		shift_y -= 1;
		shift_y -= scale_shift;
	}

	if (p_hdr10pgen_param) {
		shift_x -= p_hdr10pgen_param->shift;
		shift_y -= p_hdr10pgen_param->shift;
	}

	hdr_gamut_data.adpscl_shift[0] = shift_x;
	hdr_gamut_data.adpscl_shift[1] = shift_y;

	/*shift2 is not used, set default*/
	hdr_gamut_data.adpscl_shift[2] = phdr_lut_param->adp_scal_y_shift;

	vpp_module_hdr_set_gamut(module, &hdr_gamut_data, vpp_sel);
}

static int _update_hdr10p_ebzcurve(enum hdr_module_type_e module,
	enum vpp_vf_top_e vpp_top, struct hdr10pgen_param_s *pdata)
{
	int i = 0;
	bool eo_gmt_bit_mode = true;
	enum hdr_vpp_type_e vpp_sel = EN_TYPE_VPP0;

	if (disable_flush_flag)
		return 0;

	switch (vpp_top) {
	case EN_VF_TOP0:
	default:
		vpp_sel = EN_TYPE_VPP0;
		break;
	case EN_VF_TOP1:
		vpp_sel = EN_TYPE_VPP1;
		break;
	case EN_VF_TOP2:
		vpp_sel = EN_TYPE_VPP2;
		break;
	}

	memset(&hdr_lut_param, 0, sizeof(struct hdr_proc_param_lut_s));
	memset(&hdr_gamut_data, 0, sizeof(struct hdr_gamut_data_s));

	/*lut parameters*/
	for (i = 0; i < SIZE_OFFSET; i++)
		hdr_lut_param.ys_coef[i] = data_ys_coef[i];

	hdr_lut_param.adp_scal_x_shift = adp_scal_x_shift;
	hdr_lut_param.adp_scal_y_shift = adp_scal_y_shift;

	for (i = 0; i < SIZE_OOTF_LUT; i++) {
		if (!pdata)
			hdr_lut_param.ogain_lut[i] = data_oo_y_lut_hdr_sdr[i];
		else
			hdr_lut_param.ogain_lut[i] = pdata->gain[i];
	}

	hdr_lut_param.lut_on = 1;

	vpp_module_hdr_set_lut(module, EN_SUB_MODULE_OGAIN,
		&hdr_lut_param.ogain_lut[0], vpp_sel);

	/*matrix parameters*/
	hdr_mtrx_param.mtrx_only = 0;
	hdr_mtrx_param.mtrx_gamut_mode = 1;

	for (i = 0; i < SIZE_COEF_3X3; i++) {
		if (!pdata)
			hdr_mtrx_param.mtrx_gamut[i] = data_ncl_2020_709_8bit[i];
		else
			hdr_mtrx_param.mtrx_gamut[i] =
				data_ncl_prmy_panel[i] * pdata->scale_gmt / 1024;
	}

	if (eo_gmt_bit_mode)
		hdr_mtrx_param.gmt_bit_mode = 1;

	hdr_mtrx_param.mtrx_on = 1;

	_set_hdr10p_gamut_param(module, &hdr_mtrx_param,
		&hdr_lut_param, pdata, vpp_top);

	return 0;
}

static void _hdr10p_update_proc(int force_src_lumin,
	enum vpp_vd_path_e vd_path, enum vpp_vf_top_e vpp_top)
{
	int panel_lumin;
	struct vinfo_s *pvinfo = NULL;
	enum hdr_module_type_e module;
	struct hdr_hist_report_s *phdr_hist;

	panel_lumin = vpp_csc_get_customer_panel_lumin();

	pvinfo = _get_vinfo(vpp_top);
	if (pvinfo) {
		if (pvinfo->hdr_info.lumi_max > 0 &&
			pvinfo->hdr_info.lumi_max <= 1000)
			panel_lumin = pvinfo->hdr_info.lumi_max;
	}

	phdr_hist = vpp_module_hdr_get_hist_report();

	hdr10p_ootf_gen(panel_lumin, force_src_lumin,
		&hdr10p_gen_param, phdr_hist->percentile);

	if (vd_path == EN_VD1_PATH)
		module = EN_MODULE_TYPE_VD1;
	else if (vd_path == EN_VD2_PATH)
		module = EN_MODULE_TYPE_VD2;
	else if (vd_path == EN_VD3_PATH)
		module = EN_MODULE_TYPE_VD3;
	else
		return;

	_update_hdr10p_ebzcurve(module, vpp_top, &hdr10p_gen_param);
}

static void _set_vpp_matrix_by_csc(enum matrix_mode_e mode,
	enum vpp_csc_type_e csc_type, bool enable)
{
	unsigned int pre_offset[VPP_MTRX_OFFSET_LEN] = {0};
	unsigned int matrix_coef[VPP_MTRX_COEF_LEN] = {0};
	unsigned int post_offset[VPP_MTRX_OFFSET_LEN] = {0};

	vpp_module_matrix_en(mode, enable);

	if (!enable)
		return;

	switch (csc_type) {
	case EN_CSC_MATRIX_RGB_YUV709:
		post_offset[0] = 0x40;
		post_offset[1] = 0x200;
		post_offset[2] = 0x200;
		matrix_coef[0] = 0xbb;
		matrix_coef[1] = 0x275;
		matrix_coef[2] = 0x3f;
		matrix_coef[3] = 0x1f99;
		matrix_coef[4] = 0x1ea6;
		matrix_coef[5] = 0x1c2;
		matrix_coef[6] = 0x1c2;
		matrix_coef[7] = 0x1e67;
		matrix_coef[8] = 0x1fd7;
		break;
	case EN_CSC_MATRIX_YUV709_RGB:
		pre_offset[0] = 0x7c0;
		pre_offset[1] = 0x600;
		pre_offset[2] = 0x600;
		matrix_coef[0] = 0x4a8;
		matrix_coef[1] = 0;
		matrix_coef[2] = 0x72c;
		matrix_coef[3] = 0x4a8;
		matrix_coef[4] = 0x1f26;
		matrix_coef[5] = 0x1ddd;
		matrix_coef[6] = 0x4a8;
		matrix_coef[7] = 0x876;
		matrix_coef[8] = 0;
		break;
	case EN_CSC_MATRIX_YUV709F_RGB:
		pre_offset[0] = 0;
		pre_offset[1] = 0x600;
		pre_offset[2] = 0x600;
		matrix_coef[0] = 0x400;
		matrix_coef[1] = 0;
		matrix_coef[2] = 0x64d;
		matrix_coef[3] = 0x400;
		matrix_coef[4] = 0x1f41;
		matrix_coef[5] = 0x1e21;
		matrix_coef[6] = 0x400;
		matrix_coef[7] = 0x76d;
		matrix_coef[8] = 0;
		break;
	default:
		return;
	}

	vpp_module_matrix_set_pre_offset(mode, &pre_offset[0]);
	vpp_module_matrix_set_offset(mode, &post_offset[0]);
	vpp_module_matrix_set_coef_3x3(mode, &matrix_coef[0]);
}

static int _vpp_matrix_update_proc(struct vframe_s *pvf,
	struct vinfo_s *pvinfo, int flags,
	enum vpp_vd_path_e vd_path, enum vpp_vf_top_e vpp_top)
{
	enum vpp_csc_type_e csc_type = EN_CSC_MATRIX_NULL;

	if (!pvf || !pvinfo)
		return 0;

	csc_type = _get_csc_type(vd_path);

	_update_hdr10p_metadata(pvf, csc_type, 0,
		&tx_hdr10p_params[vd_path]);

	_hdr10_tmo_update_proc(NULL, vd_path, vpp_top);
	_hdr10p_update_proc(0, vd_path, vpp_top);

	_set_vpp_matrix_by_csc(EN_MTRX_MODE_MAX, csc_type, false);

	return 0;
}

static void _overscan_fresh(struct vframe_s *vf)
{
	unsigned int height = 0;
	unsigned int cur_overscan_timing = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	unsigned int cur_fmt;
	unsigned int offset = EN_TIMING_UHD + 1;/*av&atv*/
#endif

	if (!overscan_status || !vf)
		return;

	if (overscan_data[0].load_flag) {
		height = (vf->type & VIDTYPE_COMPRESS) ?
			vf->compHeight : vf->height;

		if (height <= 480)
			cur_overscan_timing = EN_TIMING_SD_480;
		else if (height <= 576)
			cur_overscan_timing = EN_TIMING_SD_576;
		else if (height <= 720)
			cur_overscan_timing = EN_TIMING_HD;
		else if (height <= 1088)
			cur_overscan_timing = EN_TIMING_FHD;
		else
			cur_overscan_timing = EN_TIMING_UHD;

		vf->pic_mode.AFD_enable =
			overscan_data[cur_overscan_timing].afd_enable;

		/*local play screen mode set by decoder*/
		if (!(vf->pic_mode.screen_mode == VIDEO_WIDEOPTION_CUSTOM &&
			vf->pic_mode.AFD_enable)) {
			if (overscan_data[0].source == EN_INPUT_MPEG)
				vf->pic_mode.screen_mode = 0xff;
			else
				vf->pic_mode.screen_mode =
					overscan_data[cur_overscan_timing].screen_mode;
		}

		if (vf->pic_mode.provider == PIC_MODE_PROVIDER_WSS &&
			vf->pic_mode.AFD_enable) {
			vf->pic_mode.hs += overscan_data[cur_overscan_timing].hs;
			vf->pic_mode.he += overscan_data[cur_overscan_timing].he;
			vf->pic_mode.vs += overscan_data[cur_overscan_timing].vs;
			vf->pic_mode.ve += overscan_data[cur_overscan_timing].ve;
		} else {
			vf->pic_mode.hs = overscan_data[cur_overscan_timing].hs;
			vf->pic_mode.he = overscan_data[cur_overscan_timing].he;
			vf->pic_mode.vs = overscan_data[cur_overscan_timing].vs;
			vf->pic_mode.ve = overscan_data[cur_overscan_timing].ve;
		}

		vf->ratio_control |= DISP_RATIO_ADAPTED_PICMODE;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	if (overscan_data[offset].load_flag) {
		cur_fmt = vf->sig_fmt;
		if (cur_fmt == TVIN_SIG_FMT_CVBS_NTSC_M)
			cur_overscan_timing = EN_TIMING_NTST_M;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_NTSC_443)
			cur_overscan_timing = EN_TIMING_NTST_443;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_PAL_I)
			cur_overscan_timing = EN_TIMING_PAL_I;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_PAL_M)
			cur_overscan_timing = EN_TIMING_PAL_M;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_PAL_60)
			cur_overscan_timing = EN_TIMING_PAL_60;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_PAL_CN)
			cur_overscan_timing = EN_TIMING_PAL_CN;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_SECAM)
			cur_overscan_timing = EN_TIMING_SECAM;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_NTSC_50)
			cur_overscan_timing = EN_TIMING_NTSC_50;
		else
			return;

		vf->pic_mode.AFD_enable =
			overscan_data[cur_overscan_timing].afd_enable;

		if (!(vf->pic_mode.screen_mode == VIDEO_WIDEOPTION_CUSTOM &&
			vf->pic_mode.AFD_enable))
			vf->pic_mode.screen_mode =
				overscan_data[cur_overscan_timing].screen_mode;

		if (vf->pic_mode.provider == PIC_MODE_PROVIDER_WSS &&
			vf->pic_mode.AFD_enable) {
			vf->pic_mode.hs += overscan_data[cur_overscan_timing].hs;
			vf->pic_mode.he += overscan_data[cur_overscan_timing].he;
			vf->pic_mode.vs += overscan_data[cur_overscan_timing].vs;
			vf->pic_mode.ve += overscan_data[cur_overscan_timing].ve;
		} else {
			vf->pic_mode.hs = overscan_data[cur_overscan_timing].hs;
			vf->pic_mode.he = overscan_data[cur_overscan_timing].he;
			vf->pic_mode.vs = overscan_data[cur_overscan_timing].vs;
			vf->pic_mode.ve = overscan_data[cur_overscan_timing].ve;
		}

		vf->ratio_control |= DISP_RATIO_ADAPTED_PICMODE;
	}
#endif

	if (overscan_reset) {
		vf->pic_mode.AFD_enable = 0;
		vf->pic_mode.screen_mode = 0;
		vf->pic_mode.hs = 0;
		vf->pic_mode.he = 0;
		vf->pic_mode.vs = 0;
		vf->pic_mode.ve = 0;
		vf->ratio_control &= ~DISP_RATIO_ADAPTED_PICMODE;
		overscan_reset = 0;
	}
}

static void _overscan_reset(void)
{
	unsigned int offset = EN_TIMING_UHD + 1; /*av&atv*/
	enum vpp_overscan_input_e src;

	src = overscan_data[0].source;

	if (!overscan_status)
		return;

	if (src != EN_INPUT_DTV && src != EN_INPUT_MPEG) {
		overscan_data[0].load_flag = 0;
	} else {
		if (!overscan_atv_source)
			overscan_data[offset].load_flag = 0;
	}
}

static void _overscan_proc(struct vframe_s *vf,
	struct vframe_s *toggle_vf, int flags,
	enum vpp_vd_path_e vd_path)
{
	if (vd_path != EN_VD1_PATH)
		return;

	if (flags & 0x2) {
		if (toggle_vf)
			_overscan_fresh(toggle_vf);
		else if (vf)
			_overscan_fresh(vf);
	} else {
		if (!toggle_vf && !vf) {
			_overscan_reset();
		} else {
			if (vf)
				_overscan_fresh(vf);
			else
				_overscan_reset();
		}
	}
}

static void _overscan_dump_data(void)
{
	int i = 0;

	pr_info("\n*****dump overscan_data after parsing*****\n");
	pr_info("i, hs, he, vs, ve, screen_mode, afd, flag\n");
	for (i = 0; i < EN_TIMING_MAX; i++)
		pr_info("%d, %d, %d, %d, %d, %d, %d, %d\n",
			i, overscan_data[i].hs, overscan_data[i].he,
			overscan_data[i].vs, overscan_data[i].ve,
			overscan_data[i].screen_mode,
			overscan_data[i].afd_enable,
			overscan_data[i].load_flag);
}

/*External functions*/
int vpp_vf_init(struct vpp_dev_s *pdev)
{
	enum vpp_chip_type_e chip_id;
	int i = 0;

	chip_id = pdev->pm_data->chip_id;
	if (chip_id == CHIP_TXHD)
		skip_hdr_mode = 1;
	else
		skip_hdr_mode = 0;

	cur_hdr_type = EN_TYPE_SDR;
	fix_range_mode = 0;
	hist_sel = 1; /*1: vpp, 0: vdin*/
	pc_mode_change = false;
	pc_mode_status = 0xff;
	pre_pw_state = EN_PW_MAX;

	memset(&cur_signal_info, 0, sizeof(struct _signal_info_s));

	for (i = 0; i < EN_VD_PATH_MAX; i++)
		memset(&cur_master_display_color[i], 0,
			sizeof(struct vframe_master_display_colour_s));

	for (i = 0; i < EN_MODULE_TYPE_MAX; i++)
		hdr_module_support[i] = true;

	switch (chip_id) {
	case CHIP_T7:
		hdr_module_support[EN_MODULE_TYPE_OSD2] = false;
		break;
	case CHIP_T3:
		hdr_module_support[EN_MODULE_TYPE_VD3] = false;
		break;
	default:
		break;
	}

	memset(&hdr_lut_param, 0, sizeof(struct hdr_proc_param_lut_s));
	memset(&hdr_mtrx_param, 0, sizeof(struct hdr_proc_param_mtrx_s));
	memset(&hdr10p_gen_param, 0, sizeof(struct hdr10pgen_param_s));
	memset(&hdr_gamut_data, 0, sizeof(struct hdr_gamut_data_s));

	adp_scal_x_shift = 10; /*1.0 = 1024*/
	adp_scal_y_shift = 10; /*1.0 = 1024*/
	disable_flush_flag = 0xffffffff;

	return 0;
}

void vpp_vf_set_pc_mode(int val)
{
	if (val != pc_mode_status) {
		pc_mode_status = val;
		pc_mode_change = true;
	}
}

void vpp_vf_set_overscan_mode(int val)
{
	/*0: disable, 1: enable*/
	overscan_status = val;
}

void vpp_vf_set_overscan_reset(int val)
{
	overscan_reset = val;
}

void vpp_vf_set_overscan_table(unsigned int length,
	struct vpp_overscan_table_s *load_table)
{
	unsigned int i;
	unsigned int offset = EN_TIMING_UHD + 1;

	if (!load_table)
		return;

	memset(overscan_data, 0, sizeof(overscan_data));

	for (i = 0; i < length; i++) {
		overscan_data[i].load_flag =
			(load_table[i].src_timing >> 31) & 0x1;
		overscan_data[i].afd_enable =
			(load_table[i].src_timing >> 30) & 0x1;
		overscan_data[i].screen_mode =
			(load_table[i].src_timing >> 24) & 0x3f;
		overscan_data[i].source =
			(load_table[i].src_timing >> 16) & 0xff;
		overscan_data[i].timing =
			load_table[i].src_timing & 0xffff;
		overscan_data[i].hs =
			load_table[i].value1 & 0xffff;
		overscan_data[i].he =
			(load_table[i].value1 >> 16) & 0xffff;
		overscan_data[i].vs =
			load_table[i].value2 & 0xffff;
		overscan_data[i].ve =
			(load_table[i].value2 >> 16) & 0xffff;
	}

	/*overscan reset for dtv auto afd set.*/
	/*if auto set load_flag = 0 by user, overscan set by dtv afd*/
	if (!overscan_data[0].load_flag &&
		!overscan_data[offset].load_flag)
		overscan_update_flag = 1;

	/*because EN_INPUT_TV is 0, need to add flag to check ATV*/
	if (overscan_data[offset].load_flag == 1 &&
		overscan_data[offset].source == EN_INPUT_TV)
		overscan_atv_source = 1;
	else
		overscan_atv_source = 0;
}

void vpp_vf_get_signal_info(enum vpp_vd_path_e vd_path,
	struct vpp_vf_signal_info_s *pinfo)
{
	unsigned int type = 0;

	if (!pinfo || vd_path == EN_VD_PATH_MAX)
		return;

	type = cur_signal_type[vd_path];
	pinfo->format = SIGNAL_FORMAT(type);
	pinfo->range = SIGNAL_RANGE(type);
	pinfo->color_primaries = SIGNAL_COLOR_PRIMARIES(type);
	pinfo->transfer_characteristic = SIGNAL_TRANSFER_CHARACTERISTIC(type);
}

unsigned int vpp_vf_get_signal_type(enum vpp_vd_path_e vd_path)
{
	if (vd_path == EN_VD_PATH_MAX)
		return 0;

	return cur_signal_type[vd_path];
}

enum vpp_data_src_e vpp_vf_get_src_type(enum vpp_vd_path_e vd_path)
{
	return EN_SRC_VGA;
}

enum vpp_hdr_type_e vpp_vf_get_hdr_type(void)
{
	return cur_hdr_type;
}

enum vpp_color_primary_e vpp_vf_get_color_primary(void)
{
	return cur_color_primary;
}

struct vpp_hdr_metadata_s *vpp_vf_get_hdr_metadata(void)
{
	return &hdr_metadata;
}

enum vpp_csc_type_e vpp_vf_get_csc_type(enum vpp_vd_path_e vd_path)
{
	return _get_csc_type(vd_path);
}

int vpp_vf_get_vinfo_lcd_support(void)
{
	return _get_vinfo_lcd_support();
}

void vpp_vf_dump_data(enum vpp_dump_data_type_e type)
{
	switch (type) {
	case EN_DUMP_DATA_OVERSCAN:
		_overscan_dump_data();
		break;
	default:
		break;
	}
}

void vpp_vf_refresh(struct vframe_s *pvf, struct vframe_s *prpt_vf)
{
	int i = 0;
	struct vframe_s *ptmp;
	struct vpp_hist_report_s *phist_report;
	struct sr_fmeter_report_s *pfm_report;
	int hist_luma_sum = 0;
	unsigned short *phist_data;
	bool do_sat_comp = false;
	int sat_comp_val = 0;

	if (!pvf && !prpt_vf)
		return;

	ptmp = pvf ? pvf : prpt_vf;

	/*histogram*/
	vpp_module_meter_fetch_hist_report();
	phist_report = vpp_module_meter_get_hist_report();

	ptmp->prop.hist.hist_pow = phist_report->hist_pow;
	ptmp->prop.hist.vpp_luma_sum = phist_report->luma_sum;
	ptmp->prop.hist.vpp_chroma_sum = phist_report->chroma_sum;
	ptmp->prop.hist.vpp_pixel_sum = phist_report->pixel_sum;
	ptmp->prop.hist.vpp_height = phist_report->height;
	ptmp->prop.hist.vpp_width = phist_report->width;
	ptmp->prop.hist.vpp_luma_max = phist_report->luma_max;
	ptmp->prop.hist.vpp_luma_min = phist_report->luma_min;

	for (i = 0; i < HIST_GM_BIN_CNT; i++)
		ptmp->prop.hist.vpp_gamma[i] = phist_report->gamma[i];

	for (i = 0; i < HIST_HUE_GM_BIN_CNT; i++)
		ptmp->prop.hist.vpp_hue_gamma[i] = phist_report->hue_gamma[i];

	for (i = 0; i < HIST_SAT_GM_BIN_CNT; i++)
		ptmp->prop.hist.vpp_sat_gamma[i] = phist_report->sat_gamma[i];

	/*fmeter*/
	if (vpp_module_sr_get_fmeter_support()) {
		vpp_module_sr_fetch_fmeter_report();
		pfm_report = vpp_module_sr_get_fmeter_report();

		for (i = 0; i < FMETER_HCNT_MAX; i++) {
			ptmp->fmeter0_hcnt[i] = pfm_report->hcnt[0][i];
			ptmp->fmeter1_hcnt[i] = pfm_report->hcnt[1][i];
		}

		ptmp->fmeter0_score = pfm_report->score[0];
		ptmp->fmeter1_score = pfm_report->score[1];
	}

	/*dnlp*/
	if (hist_sel) {
		hist_luma_sum = ptmp->prop.hist.vpp_luma_sum;
		phist_data = &ptmp->prop.hist.vpp_gamma[0];
	} else {
		hist_luma_sum = ptmp->prop.hist.luma_sum;
		phist_data = &ptmp->prop.hist.gamma[0];
	}

	/*vpp_module_dnlp_on_vs(hist_luma_sum, phist_data);*/
	vpp_module_dnlp_get_sat_compensation(&do_sat_comp, &sat_comp_val);

	/*if (do_sat_comp) !!!maybe conflict with pq table tuning params*/
		/*vpp_module_cm_set_tuning_param(EN_PARAM_GLB_SAT, &sat_comp_val);*/
}

void vpp_vf_proc(struct vframe_s *pvf,
	struct vframe_s *ptoggle_vf,
	struct vpp_vf_param_s *pvf_param,
	int flags,
	enum vpp_vd_path_e vd_path,
	enum vpp_vf_top_e vpp_top)
{
	unsigned int signal_type = 0;
	struct vpp_hist_report_s *phist_report = NULL;
	struct lc_vs_param_s lc_vs_param;
	struct data_vs_param_s data_vs_param;
	struct vinfo_s *pvinfo = NULL;
	int tmp = 0;

	if (!pvf || !ptoggle_vf || !pvf_param)
		return;

	signal_type = _update_signal_type(pvf);
	_update_hdr_type(signal_type);
	_update_hdr_metadata(pvf);

	_detect_signal_type(pvf, NULL, vd_path, vpp_top);

	vpp_module_pre_gamma_on_vs();
	vpp_module_lcd_gamma_on_vs();

	vpp_module_matrix_on_vs();

	phist_report = vpp_module_meter_get_hist_report();

	lc_vs_param.vf_type = pvf->type;
	lc_vs_param.vf_signal_type = signal_type;
	lc_vs_param.vf_height = pvf->height;
	lc_vs_param.vf_width = pvf->width;
	lc_vs_param.sps_h_en = pvf_param->sps_h_en;
	lc_vs_param.sps_v_en = pvf_param->sps_v_en;
	lc_vs_param.sps_w_in = pvf_param->sps_w_in;
	lc_vs_param.sps_h_in = pvf_param->sps_h_in;

	vpp_module_lc_on_vs(&phist_report->gamma[0], &lc_vs_param);

	data_vs_param.src_type = EN_SRC_VGA;
	data_vs_param.vf_signal_change = signal_type;
	data_vs_param.vf_width = pvf->width;
	data_vs_param.vf_height = pvf->height;

	vpp_data_on_vs(&data_vs_param);

	if (pc_mode_change)
		_pc_mode_proc(pc_mode_status);

	if (vd_path == EN_VD1_PATH)
		_set_enhance_clock(ptoggle_vf, pvf);

	/*On-going*/
	pvinfo = _get_vinfo(EN_VF_TOP0);
	tmp = _sink_hdr_support(pvinfo);
	tmp = _is_vinfo_available(pvinfo);
	tmp = _is_video_turn_on(&video_status[0], EN_VD1_PATH);
	_prepare_customer_mtrx(0, NULL, NULL, NULL, NULL, false);
	_cp_hdr_info(NULL, NULL);
	_vpp_matrix_update_proc(NULL, NULL, 0, EN_VD1_PATH, EN_VF_TOP0);
	_overscan_proc(pvf, ptoggle_vf, flags, vd_path);
}

