// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vpp_common.h"
#include "vpp_data.h"
#include "vpp_vf_proc.h"
#include "vpp_modules_inc.h"

#define SIGNAL_FORMAT(type) ((type >> 26) & 0x7)
#define SIGNAL_RANGE(type) ((type >> 25) & 0x1)
#define SIGNAL_COLOR_PRIMARIES(type) ((type >> 16) & 0xff)
#define SIGNAL_TRANSFER_CHARACTERISTIC(type) ((type >> 8) & 0xff)

static unsigned int cur_signal_type[EN_VD_PATH_MAX] = {
	0xffffffff,
	0xffffffff,
	0xffffffff
};

static struct vpp_hdr_metadata_s hdr_metadata;
static enum vpp_hdr_type_e cur_hdr_type;
static enum vpp_color_primary_e cur_color_primary;
static int fix_range_mode;
static int skip_hdr_mode;
static bool hist_sel;
static enum vpp_data_src_e cur_data_src[EN_VD_PATH_MAX] = {
	EN_SRC_VGA,
	EN_SRC_VGA,
	EN_SRC_VGA
};

/*Internal functions*/
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

		signal_type =
			(present_flag << 29) |
			(video_format << 26) |
			(range << 25) |
			(color_description_present_flag << 24) |
			(color_primaries << 16) |
			(transfer_characteristic << 8) |
			matrix_coefficient;
	}

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

static int _detect_signal_type(struct vframe_s *pvf,
	struct vinfo_s *pvinfo, enum vpp_vd_path_e vd_path,
	enum vpp_vf_top_e vpp_top)
{
	/*enum vpp_data_src_e data_src = EN_SRC_VGA;*/
	/*int signal_type_change = 0;*/
	return 0;
}

/*External functions*/
int vpp_vf_init(struct vpp_dev_s *pdev)
{
	enum vpp_chip_type_e chip_id;

	chip_id = pdev->pm_data->chip_id;
	if (chip_id == CHIP_TXHD)
		skip_hdr_mode = 1;
	else
		skip_hdr_mode = 0;

	cur_hdr_type = EN_TYPE_SDR;
	fix_range_mode = 0;
	hist_sel = 1; /*1: vpp, 0: vdin*/

	return 0;
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
	if (vd_path == EN_VD_PATH_MAX)
		return EN_SRC_VGA;

	return cur_data_src[vd_path];
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

	vpp_module_dnlp_update_data(hist_luma_sum, phist_data);
	vpp_module_dnlp_get_sat_compensation(&do_sat_comp, &sat_comp_val);

	if (do_sat_comp) /*!!!maybe conflict with pq table tuning params*/
		vpp_module_cm_set_tuning_param(EN_PARAM_GLB_SAT, &sat_comp_val);
}

void vpp_vf_proc(struct vframe_s *pvf,
	struct vframe_s *ptoggle_vf,
	struct vpp_vf_param_s *pvf_param,
	int flags,
	enum vpp_vd_path_e vd_path,
	enum vpp_vf_top_e vpp_top)
{
	unsigned int signal_type = 0;
	int *phist_data = NULL;
	struct lc_vs_param_s vs_param;

	if (!pvf || !ptoggle_vf)
		return;

	signal_type = _update_signal_type(pvf);
	_update_hdr_type(signal_type);
	_update_hdr_metadata(pvf);

	_detect_signal_type(pvf, NULL, vd_path, vpp_top);

	vpp_module_pre_gamma_on_vs();
	vpp_module_lcd_gamma_on_vs();

	vpp_module_lc_on_vs(phist_data, &vs_param);
}

