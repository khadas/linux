/*
 * drivers/amlogic/media/enhancement/amvecm/amcsc_pip.c
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

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/video_common.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include "amcsc.h"
#include "set_hdr2_v0.h"
#include "hdr/am_hdr10_plus.h"

static enum output_format_e target_format[VD_PATH_MAX];
static enum hdr_type_e cur_source_format[VD_PATH_MAX];
static enum output_format_e output_format;

static const char *module_str[7] = {
	"UNKNOWN",
	"VD1",
	"VD2",
	"OSD1",
	"VDIN0",
	"VDIN1",
	"DI"
};

static const char *process_str[16] = {
	"UNKNOWN",
	"HDR_BYPASS",
	"HDR_SDR",
	"SDR_HDR",
	"HLG_BYPASS",
	"HLG_SDR",
	"HLG_HDR",
	"SDR_HLG",
	"SDR_IPT",
	"HDR_IPT",
	"HLG_IPT",
	"HDR_HLG",
	"RGB_YUV",
	"RGB_HDR",
	"RGB_HLG",
	"HDR10P_SDR"
};

static const char *policy_str[3] = {
	"follow_sink",
	"follow_source",
	"force_output"
};

static const char *input_str[8] = {
	"NONE",
	"HDR",
	"HDR+",
	"DOVI",
	"PRIME",
	"HLG",
	"SDR",
	"MVC",
};

/* output_format_e */
static const char *output_str[7] = {
	"UNKNOWN",
	"709",
	"2020",
	"HDR",
	"HDR+",
	"HLG",
	"IPT"
};

static const char *dv_output_str[6] = {
	"IPT",
	"TUNNEL",
	"HDR10",
	"SDR10",
	"SDR8",
	"BYPASS"
};

void hdr_proc(
	enum hdr_module_sel module_sel,
	enum hdr_process_sel hdr_process_select,
	struct vinfo_s *vinfo)
{
	enum hdr_process_sel cur_hdr_process;

	cur_hdr_process = hdr_func(
		module_sel, hdr_process_select, vinfo);
	if (cur_hdr_process != hdr_process_select)
		pr_csc(8, "am_vecm: module=%s, process=%s(%s)\n",
			module_str[module_sel],
			process_str[hdr_process_select],
			process_str[cur_hdr_process]);
	else
		pr_csc(8, "am_vecm: hdr module=%s, process=%s\n",
			module_str[module_sel],
			process_str[hdr_process_select]);
}

int hdr_policy_process(
	struct vinfo_s *vinfo,
	enum hdr_type_e *source_format,
	enum vd_path_e vd_path)
{
	int change_flag = 0;
	enum vd_path_e oth_path =
		(vd_path == VD1_PATH) ? VD2_PATH : VD1_PATH;
	int cur_hdr_policy;
	int dv_policy = 0;
	int dv_mode = 0;
	int dv_format = 0;
	bool hdr10_plus_support	=
		sink_hdr_support(vinfo) & HDRP_SUPPORT;

	tx_hdr10_plus_support = hdr10_plus_support;

	cur_hdr_policy = get_hdr_policy();
	if (is_dolby_vision_enable()) {
		/* sync hdr_policy with dolby_vision_policy */
		/* get current dolby_vision_mode */
		dv_policy = get_dolby_vision_policy();
		dv_mode = get_dolby_vision_target_mode();
		dv_format = get_dolby_vision_src_format();
	}

	if (get_hdr_module_status(vd_path) != HDR_MODULE_ON) {
		/* hdr module off or bypass */
		sdr_process_mode[vd_path] = PROC_BYPASS;
		hdr_process_mode[vd_path] = PROC_BYPASS;
		hlg_process_mode[vd_path] = PROC_BYPASS;
		hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
		target_format[vd_path] = BT709;
	} else if (cur_hdr_policy == 0) {
		if (source_format[vd_path] == HDRTYPE_MVC) {
			/* hdr bypass output need sdr */
			sdr_process_mode[vd_path] = PROC_BYPASS;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			sdr_process_mode[oth_path] = PROC_BYPASS;
			hdr_process_mode[oth_path] = PROC_BYPASS;
			hlg_process_mode[oth_path] = PROC_BYPASS;
			hdr10_plus_process_mode[oth_path] = PROC_BYPASS;
			target_format[vd_path] = BT709;
			target_format[oth_path] = BT709;
		} else if (vd_path == VD1_PATH &&
		    is_dolby_vision_enable() &&
		    !is_dolby_vision_on() &&
		    (source_format[vd_path]
		     == HDRTYPE_DOVI ||
		     source_format[vd_path]
		     == HDRTYPE_HDR10 ||
		     source_format[vd_path]
		     == HDRTYPE_SDR)) {
			/* vd1 follow sink: dv handle sdr/hdr/dovi */
			sdr_process_mode[vd_path] = PROC_BYPASS;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT709;
			set_hdr_module_status(vd_path, HDR_MODULE_OFF);
			dolby_vision_set_toggle_flag(1);
		} else if ((vd_path == VD1_PATH) &&
			(source_format[vd_path]
			== HDRTYPE_HLG) &&
			(sink_hdr_support(vinfo)
			& HLG_SUPPORT)) {
			/* vd1 bypass hlg */
			hlg_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT2020_HLG;
		} else if ((vd_path == VD1_PATH) &&
			(!is_video_layer_on(VD2_PATH)) &&
			(source_format[vd_path]
			== HDRTYPE_HDR10PLUS) &&
			hdr10_plus_support) {
			/* vd1 bypass hdr+ when vd2 off */
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT2020_PQ_DYNAMIC;
		} else if (is_dolby_vision_on() && (vd_path == VD2_PATH)) {
			/* vd2 *->ipt when vd1 dolby on */
			hdr_process_mode[vd_path] = PROC_MATCH;
			hlg_process_mode[vd_path] = PROC_MATCH;
			sdr_process_mode[vd_path] = PROC_MATCH;
			hdr10_plus_process_mode[vd_path] = PROC_MATCH;
			target_format[vd_path] = BT2100_IPT;
		} else if ((vd_path == VD2_PATH) &&
			is_video_layer_on(VD1_PATH)) {
			/* vd1 on and vd2 follow vd1 output */
			if (target_format[VD1_PATH] == BT2020_HLG) {
				/* vd2 *->hlg when vd1 output hlg */
				sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
				hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
				hlg_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] =
					PROC_HDRP_TO_HLG;
				target_format[vd_path] = BT2020_HLG;
			} else if ((target_format[VD1_PATH] == BT2020_PQ) ||
				(target_format[VD1_PATH]
				== BT2020_PQ_DYNAMIC)) {
				/* vd2 *->hdr when vd1 output hdr/hdr+ */
				sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
				hdr10_plus_process_mode[vd_path] =
					PROC_HDRP_TO_HDR;
				target_format[vd_path] = BT2020_PQ;
			} else {
				/* vd2 *->sdr when vd1 output sdr */
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
				hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
				hdr10_plus_process_mode[vd_path] =
					PROC_HDRP_TO_SDR;
				if ((target_format[VD1_PATH] == BT2020) &&
				(source_format[vd_path] == HDRTYPE_HLG))
					target_format[vd_path] = BT2020;
				else
					target_format[vd_path] = BT709;
			}
		} else if ((sink_hdr_support(vinfo) & HDR_SUPPORT) &&
			((source_format[vd_path] != HDRTYPE_HLG) ||
			((source_format[vd_path] == HDRTYPE_HLG) &&
			(hdr_flag & 0x10)))) {
			/* *->hdr */
			sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
			hdr10_plus_process_mode[vd_path] =
				PROC_HDRP_TO_HDR;
			target_format[vd_path] = BT2020_PQ;
		} else {
			/* *->sdr */
			sdr_process_mode[vd_path] = PROC_BYPASS;
			hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
			hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
			hdr10_plus_process_mode[vd_path] =
				PROC_HDRP_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
			if ((sink_hdr_support(vinfo)
			& BT2020_SUPPORT) &&
			((source_format[vd_path] == HDRTYPE_HLG) ||
			 (source_format[vd_path] == HDRTYPE_HDR10)) &&
			!is_video_layer_on(oth_path))
				target_format[vd_path] = BT2020;
			else
				target_format[vd_path] = BT709;
#else
			target_format[vd_path] = BT709;
#endif
		}
	} else if (cur_hdr_policy == 1) {
		if (source_format[vd_path] == HDRTYPE_MVC) {
			/* hdr bypass output need sdr */
			sdr_process_mode[vd_path] = PROC_BYPASS;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			sdr_process_mode[oth_path] = PROC_BYPASS;
			hdr_process_mode[oth_path] = PROC_BYPASS;
			hlg_process_mode[oth_path] = PROC_BYPASS;
			hdr10_plus_process_mode[oth_path] = PROC_BYPASS;
			target_format[vd_path] = BT709;
			target_format[oth_path] = BT709;
		} else if (vd_path == VD1_PATH &&
		    is_dolby_vision_enable() &&
		    !is_dolby_vision_on() &&
		    source_format[vd_path]
		    == HDRTYPE_DOVI) {
			/* vd1 follow source: dv handle dovi */
			sdr_process_mode[vd_path] = PROC_BYPASS;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT709;
			set_hdr_module_status(vd_path, HDR_MODULE_OFF);
			dolby_vision_set_toggle_flag(1);
		} else if ((vd_path == VD2_PATH) && is_dolby_vision_on()) {
			/* VD2 with VD1 in DV mode */
			hdr_process_mode[vd_path] = PROC_MATCH;
			hlg_process_mode[vd_path] = PROC_MATCH;
			sdr_process_mode[vd_path] = PROC_MATCH; /* *->ipt */
			target_format[vd_path] = BT2100_IPT;
		} else if ((vd_path == VD1_PATH) ||
			((vd_path == VD2_PATH) &&
			!is_video_layer_on(VD1_PATH))) {
			/* VD1(with/without VD2) */
			/* or VD2(without VD1) <= should switch to VD1 */
			switch (source_format[vd_path]) {
			case HDRTYPE_SDR:
				if (is_video_layer_on(oth_path)) {
					if ((target_format[oth_path] ==
					BT2020_PQ) ||
					(target_format[oth_path] ==
					BT2020_PQ_DYNAMIC)) {
						/* other layer output HDR */
						/* sdr *->hdr */
						sdr_process_mode[vd_path] =
							PROC_SDR_TO_HDR;
						target_format[vd_path] =
							BT2020_PQ;
					} else if (target_format[oth_path] ==
					BT2020_HLG) {
						/* other layer output hlg */
						/* sdr *->hlg */
						sdr_process_mode[vd_path] =
							PROC_SDR_TO_HLG;
						target_format[vd_path] =
							BT2020_HLG;
					} else {
						/* sdr->sdr */
						sdr_process_mode[vd_path] =
							PROC_BYPASS;
						target_format[vd_path] = BT709;
					}
				} else {
					/* sdr->sdr */
					sdr_process_mode[vd_path] =
						PROC_BYPASS;
					target_format[vd_path] = BT709;
				}
				break;
			case HDRTYPE_HLG:
				/* source HLG */
				if (is_video_layer_on(oth_path)
				&& ((target_format[oth_path] ==
				BT2020_PQ) ||
				(target_format[oth_path] ==
				BT2020_PQ_DYNAMIC))) {
					/* hlg->hdr */
					hlg_process_mode[vd_path] =
						PROC_HLG_TO_HDR;
					target_format[vd_path] =
						BT2020_PQ;
				} else if (sink_hdr_support(vinfo)
				& HLG_SUPPORT) {
					/* hlg->hlg */
					hlg_process_mode[vd_path] =
						PROC_BYPASS;
					target_format[vd_path] =
						BT2020_HLG;
				} else if ((sink_hdr_support(vinfo)
				& HDR_SUPPORT) && (hdr_flag & 0x10)) {
					/* hlg->hdr */
					hlg_process_mode[vd_path] =
						PROC_HLG_TO_HDR;
					target_format[vd_path] =
						BT2020_PQ;
				} else {
					/* hlg->sdr */
					hlg_process_mode[vd_path] =
						PROC_HLG_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
					if ((sink_hdr_support(vinfo)
					& BT2020_SUPPORT) &&
					!is_video_layer_on(oth_path))
						target_format[vd_path] =
							BT2020;
					else
						target_format[vd_path] =
							BT709;
#else
					target_format[vd_path] = BT709;
#endif
				}
				break;
			case HDRTYPE_HDR10:
				/* source HDR10 */
				if (sink_hdr_support(vinfo)
				& HDR_SUPPORT) {
					/* hdr bypass */
					hdr_process_mode[vd_path] =
						PROC_BYPASS;
					target_format[vd_path] =
						BT2020_PQ;
				} else if (sink_hdr_support(vinfo)
				& HLG_SUPPORT) {
					/* hdr->hlg */
					hdr_process_mode[vd_path] =
						PROC_HDR_TO_HLG;
					target_format[vd_path] =
						BT2020_HLG;
				} else {
					/* hdr ->sdr */
					hdr_process_mode[vd_path] =
						PROC_HDR_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
					if ((sink_hdr_support(vinfo)
					& BT2020_SUPPORT) &&
					!is_video_layer_on(oth_path))
						target_format[vd_path] =
							BT2020;
					else
#else
					target_format[vd_path] =
						BT709;
#endif
				}
				break;
			case HDRTYPE_HDR10PLUS:
				/* source HDR10+ */
				if (hdr10_plus_support
				&& !is_video_layer_on(oth_path)) {
					/* hdr+ bypass */
					hdr10_plus_process_mode[vd_path] =
						PROC_BYPASS;
					target_format[vd_path] =
						BT2020_PQ_DYNAMIC;
				} else if (sink_hdr_support(vinfo)
				& HDR_SUPPORT) {
					/* hdr+->hdr */
					hdr10_plus_process_mode[vd_path] =
						PROC_HDRP_TO_HDR;
					target_format[vd_path] = BT2020_PQ;
				} else if (sink_hdr_support(vinfo)
				& HLG_SUPPORT) {
					/* hdr+->hlg */
					hdr10_plus_process_mode[vd_path] =
						PROC_HDRP_TO_HLG;
					target_format[vd_path] = BT2020_HLG;
				} else {
					/* hdr+ *->sdr */
					hdr10_plus_process_mode[vd_path] =
						PROC_HDRP_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
					if ((sink_hdr_support(vinfo)
					& BT2020_SUPPORT) &&
					!is_video_layer_on(oth_path))
						target_format[vd_path] =
							BT2020;
					else
#else
					target_format[vd_path] =
						BT709;
#endif
				}
				break;
			default:
				break;
			}
		} else {
			/* VD2 with VD1 */
			if (is_dolby_vision_on()) {
				/* VD1 is dolby vision */
				hdr_process_mode[vd_path] = PROC_MATCH;
				hlg_process_mode[vd_path] = PROC_MATCH;
				sdr_process_mode[vd_path] = PROC_MATCH;
				target_format[vd_path] = BT2100_IPT;
			} else {
				oth_path = VD1_PATH;
				switch (source_format[vd_path]) {
				case HDRTYPE_SDR:
					/* VD2 source SDR */
					if ((target_format[oth_path] ==
					BT2020_PQ)
					|| (target_format[oth_path] ==
					BT2020_PQ_DYNAMIC)) {
						/* other layer output HDR */
						/* sdr *->hdr */
						sdr_process_mode[vd_path] =
							PROC_SDR_TO_HDR;
						target_format[vd_path] =
							BT2020_PQ;
					} else if (target_format[oth_path] ==
					BT2020_HLG) {
						/* other layer on and not sdr */
						/* sdr *->hlg */
						sdr_process_mode[vd_path] =
							PROC_SDR_TO_HLG;
						target_format[vd_path] =
							BT2020_HLG;
					} else {
						/* sdr->sdr */
						sdr_process_mode[vd_path] =
							PROC_BYPASS;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HLG:
					/* VD2 source HLG */
					if (target_format[oth_path]
					== BT2020_HLG) {
						/* hlg->hlg */
						hlg_process_mode[vd_path] =
							PROC_BYPASS;
						target_format[vd_path] =
							BT2020_HLG;
					} else if ((target_format[oth_path] ==
					BT2020_PQ)
					|| (target_format[oth_path] ==
					BT2020_PQ_DYNAMIC)) {
						/* hlg->hdr */
						hlg_process_mode[vd_path] =
							PROC_HLG_TO_HDR;
						target_format[vd_path] =
							BT2020_PQ;
					} else if (target_format[oth_path] ==
					BT709) {
						/* hlg->sdr */
						hlg_process_mode[vd_path] =
							PROC_HLG_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HDR10:
					/* VD2 source HDR10 */
					if ((target_format[oth_path] ==
					BT2020_PQ)
					|| (target_format[oth_path] ==
					BT2020_PQ_DYNAMIC)) {
						/* hdr->hdr */
						hdr_process_mode[vd_path] =
							PROC_BYPASS;
						target_format[vd_path] =
							BT2020_PQ;
					} else if (target_format[oth_path]
					== BT2020_HLG) {
						/* hdr->hlg */
						hdr_process_mode[vd_path] =
							PROC_HDR_TO_HLG;
						target_format[vd_path] =
							BT2020_HLG;
					} else {
						/* hdr->sdr */
						hdr_process_mode[vd_path] =
							PROC_HDR_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HDR10PLUS:
					/* VD2 source HDR10+ */
					if ((target_format[oth_path] ==
					BT2020_PQ)
					|| (target_format[oth_path] ==
					BT2020_PQ_DYNAMIC)) {
						/* hdr->hdr */
						hdr10_plus_process_mode[vd_path]
							= PROC_HDRP_TO_HDR;
						target_format[vd_path] =
							BT2020_PQ;
					} else if (target_format[oth_path]
					== BT2020_HLG) {
						/* hdr->hlg */
						hdr10_plus_process_mode[vd_path]
							= PROC_HDR_TO_HLG;
						target_format[vd_path] =
							BT2020_HLG;
					} else {
						/* hdr->sdr */
						hdr10_plus_process_mode[vd_path]
							= PROC_HDRP_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				default:
					break;
				}
			}
		}
	} else if (cur_hdr_policy == 2 &&
	!is_dolby_vision_enable()) {
		/* dv off, and policy == debug */
		/* *->force_output */
		if ((vd_path == VD1_PATH) ||
		    ((vd_path == VD2_PATH) &&
		     !is_video_layer_on(VD1_PATH))) {
			/* VD1 or VD2 without VD1 */
			target_format[vd_path] = get_force_output();
			switch (target_format[vd_path]) {
			case BT709:
				sdr_process_mode[vd_path] =
					PROC_BYPASS;
				hlg_process_mode[vd_path] =
					PROC_HLG_TO_SDR;
				hdr_process_mode[vd_path] =
					PROC_HDR_TO_SDR;
				hdr10_plus_process_mode[vd_path]
					= PROC_HDRP_TO_SDR;
				break;
			case BT2020:
				break;
			case BT2020_PQ:
				sdr_process_mode[vd_path] =
					PROC_SDR_TO_HDR;
				hlg_process_mode[vd_path] =
					PROC_HLG_TO_HDR;
				hdr_process_mode[vd_path] =
					PROC_BYPASS;
				hdr10_plus_process_mode[vd_path]
					= PROC_HDRP_TO_HDR;
				break;
			case BT2020_PQ_DYNAMIC:
				sdr_process_mode[vd_path] =
					PROC_SDR_TO_HDR;
				hlg_process_mode[vd_path] =
					PROC_HLG_TO_HDR;
				hdr_process_mode[vd_path] =
					PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] =
					PROC_BYPASS;
				break;
			case BT2020_HLG:
				sdr_process_mode[vd_path] =
					PROC_SDR_TO_HLG;
				hlg_process_mode[vd_path] =
					PROC_BYPASS;
				hdr_process_mode[vd_path] =
					PROC_HDR_TO_HLG;
				hdr10_plus_process_mode[vd_path]
					= PROC_HDRP_TO_HLG;
				break;
			case BT2100_IPT:
				/* hdr module not handle dv output */
				break;
			default:
				sdr_process_mode[vd_path] =
					PROC_BYPASS;
				hlg_process_mode[vd_path] =
					PROC_HLG_TO_SDR;
				hdr_process_mode[vd_path] =
					PROC_HDR_TO_SDR;
				hdr10_plus_process_mode[vd_path]
					= PROC_HDRP_TO_SDR;
				break;
			}
		} else {
			/* VD2 with VD1 on */
			if (is_dolby_vision_on()) {
				/* VD1 is dolby vision */
				hdr_process_mode[vd_path] = PROC_MATCH;
				hlg_process_mode[vd_path] = PROC_MATCH;
				sdr_process_mode[vd_path] = PROC_MATCH;
				target_format[vd_path] = BT2100_IPT;
			} else {
				oth_path = VD1_PATH;
				switch (source_format[vd_path]) {
				case HDRTYPE_SDR:
					/* VD2 source SDR */
					if ((target_format[oth_path] ==
					BT2020_PQ) ||
					(target_format[oth_path] ==
					BT2020_PQ_DYNAMIC)) {
						/* other layer output HDR */
						/* sdr *->hdr */
						sdr_process_mode[vd_path] =
							PROC_SDR_TO_HDR;
						target_format[vd_path] =
							BT2020_PQ;
					} else if (target_format[oth_path] ==
					BT2020_HLG) {
						/* other layer on and not sdr */
						/* sdr *->hlg */
						sdr_process_mode[vd_path] =
							PROC_SDR_TO_HLG;
						target_format[vd_path] =
							BT2020_HLG;
					} else {
						/* sdr->sdr */
						sdr_process_mode[vd_path] =
							PROC_BYPASS;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HLG:
					/* VD2 source HLG */
					if (target_format[oth_path]
					== BT2020_HLG) {
						/* hlg->hlg */
						hlg_process_mode[vd_path] =
							PROC_BYPASS;
						target_format[vd_path] =
							BT2020_HLG;
					} else if ((target_format[oth_path] ==
					BT2020_PQ) ||
					(target_format[oth_path] ==
					BT2020_PQ_DYNAMIC)) {
						/* hlg->hdr */
						hlg_process_mode[vd_path] =
							PROC_HLG_TO_HDR;
						target_format[vd_path] =
							BT2020_PQ;
					} else if (target_format[oth_path] ==
					BT709) {
						/* hlg->sdr */
						hlg_process_mode[vd_path] =
							PROC_HLG_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HDR10:
					/* VD2 source HDR10 */
					if ((target_format[oth_path] ==
					BT2020_PQ) ||
					(target_format[oth_path] ==
					BT2020_PQ_DYNAMIC)) {
						/* hdr->hdr */
						hdr_process_mode[vd_path] =
							PROC_BYPASS;
						target_format[vd_path] =
							BT2020_PQ;
					} else if (target_format[oth_path]
					== BT2020_HLG) {
						/* hdr->hlg */
						hdr_process_mode[vd_path] =
							PROC_HDR_TO_HLG;
						target_format[vd_path] =
							BT2020_HLG;
					} else {
						/* hdr->sdr */
						hdr_process_mode[vd_path] =
							PROC_HDR_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HDR10PLUS:
					/* VD2 source HDR10+ */
					if ((target_format[oth_path] ==
					BT2020_PQ) ||
					(target_format[oth_path] ==
					BT2020_PQ_DYNAMIC)) {
						/* hdr->hdr */
						hdr10_plus_process_mode[vd_path]
							= PROC_HDRP_TO_HDR;
						target_format[vd_path] =
							BT2020_PQ;
					} else if (target_format[oth_path]
					== BT2020_HLG) {
						/* hdr->hlg */
						hdr10_plus_process_mode[vd_path]
							= PROC_HDR_TO_HLG;
						target_format[vd_path] =
							BT2020_HLG;
					} else {
						/* hdr->sdr */
						hdr10_plus_process_mode[vd_path]
							= PROC_HDRP_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				default:
					break;
				}
			}
		}
	}

	/* update change flags */
	if (is_dolby_vision_on()
	&& (vd_path == VD1_PATH)) {
		pr_csc(4, "am_vecm: vd%d: (%s) %s->%s.\n",
			vd_path + 1,
			policy_str[dv_policy],
			input_str[dv_format],
			dv_output_str[dv_mode]);
	} else {
		if (cur_hdr10_plus_process_mode[vd_path]
		!= hdr10_plus_process_mode[vd_path])
			change_flag |= SIG_HDR_MODE;
		if (cur_hdr_process_mode[vd_path]
		!= hdr_process_mode[vd_path])
			change_flag |= SIG_HDR_MODE;
		if (cur_hlg_process_mode[vd_path]
		!= hlg_process_mode[vd_path])
			change_flag |= SIG_HLG_MODE;
		if (cur_sdr_process_mode[vd_path]
		!= sdr_process_mode[vd_path])
			change_flag |= SIG_HDR_MODE;
		if (cur_source_format[vd_path]
		!= source_format[vd_path])
			change_flag |= SIG_SRC_CHG;
		if (change_flag)
			pr_csc(4, "am_vecm: vd%d: (%s) %s->%s (%s).\n",
				vd_path + 1,
				policy_str[cur_hdr_policy],
				input_str[source_format[vd_path]],
				output_str[target_format[vd_path]],
				is_dolby_vision_on() ?
				dv_output_str[dv_mode] :
				output_str[output_format]);
	}
	cur_source_format[vd_path] = source_format[vd_path];

	if (is_dolby_vision_on() &&
	    (vd_path == VD2_PATH) &&
	    is_video_layer_on(VD2_PATH) &&
	    (target_format[vd_path] != BT2100_IPT)) {
		pr_csc(4, "am_vecm: vd%d output mode not match to dolby %s.\n",
			vd_path + 1,
			output_str[target_format[vd_path]]);
		change_flag |= SIG_OUTPUT_MODE_CHG;
	} else if (!is_dolby_vision_on() &&
		is_video_layer_on(VD1_PATH) &&
		is_video_layer_on(VD2_PATH) &&
		(target_format[vd_path]
		!= target_format[oth_path])) {
		pr_csc(4, "am_vecm: vd%d output mode not match %s %s.\n",
			vd_path + 1,
			output_str[target_format[vd_path]],
			output_str[target_format[oth_path]]);
		change_flag |= SIG_OUTPUT_MODE_CHG;
	}
	if (change_flag & SIG_OUTPUT_MODE_CHG) {
		/* need change video process for another path */
		switch (cur_source_format[oth_path]) {
		case HDRTYPE_HDR10PLUS:
			cur_hdr10_plus_process_mode[oth_path] =
				PROC_OFF;
			break;
		case HDRTYPE_HDR10:
			cur_hdr_process_mode[oth_path] = PROC_OFF;
			break;
		case HDRTYPE_HLG:
			cur_hlg_process_mode[oth_path] = PROC_OFF;
			break;
		case HDRTYPE_SDR:
			cur_sdr_process_mode[oth_path] = PROC_OFF;
			break;
		case HDRTYPE_MVC:
			cur_sdr_process_mode[oth_path] = PROC_BYPASS;
			cur_hdr_process_mode[oth_path] = PROC_BYPASS;
			cur_hlg_process_mode[oth_path] = PROC_BYPASS;
			cur_hdr10_plus_process_mode[oth_path] = PROC_BYPASS;
			break;
		default:
			break;
		}
	}
	return change_flag;
}

static void prepare_hdr_info(
	struct master_display_info_s *hdr_data,
	struct vframe_master_display_colour_s *p)
{
	memset(hdr_data->primaries, 0, sizeof(hdr_data->primaries));
	if ((p->present_flag & 1)
	&& (((hdr_data->features >> 16) & 0xff) == 9)) {
		memcpy(hdr_data->primaries,
			p->primaries,
			sizeof(u32)*6);
		memcpy(hdr_data->white_point,
			p->white_point,
			sizeof(u32)*2);
		hdr_data->luminance[0] =
			p->luminance[0];
		hdr_data->luminance[1] =
			p->luminance[1];
		if (p->content_light_level.present_flag == 1) {
			hdr_data->max_content =
				p->content_light_level.max_content;
			hdr_data->max_frame_average =
				p->content_light_level.max_pic_average;
		} else {
			hdr_data->max_content = 0;
			hdr_data->max_frame_average = 0;
		}
		hdr_data->luminance[0] = hdr_data->luminance[0] / 10000;
		hdr_data->present_flag = 1;
	}
}

static unsigned int content_max_lumin[VD_PATH_MAX];

void hdmi_packet_process(
	int signal_change_flag,
	struct vinfo_s *vinfo,
	struct vframe_master_display_colour_s *p,
	struct hdr10plus_para *hdmitx_hdr10plus_param,
	enum vd_path_e vd_path)
{
	struct vout_device_s *vdev = NULL;
	struct master_display_info_s send_info;
	enum output_format_e cur_output_format = output_format;

	if (customer_hdr_clipping)
		content_max_lumin[vd_path] =
			customer_hdr_clipping;
	else if (p->luminance[0])
		content_max_lumin[vd_path] =
			p->luminance[0] / 10000;
	else
		content_max_lumin[vd_path] = 1250;

	if (!vinfo)
		return;
	if (!vinfo->vout_device) {
		/* pr_info("vinfo->vout_device is null, return\n"); */
		return;
	}
	vdev = vinfo->vout_device;
	if (!vdev->fresh_tx_hdr_pkt) {
		pr_info("vdev->fresh_tx_hdr_pkt is null, return\n");
		/* continue */
	}

	if ((target_format[vd_path] == cur_output_format)
	&& (cur_output_format != BT2020_PQ_DYNAMIC)
	&& ((cur_output_format == BT2020_PQ) &&
	!(signal_change_flag & SIG_PRI_INFO)))
		return;

	/* clean hdr10plus packet when switch to others */
	if ((target_format[vd_path] != BT2020_PQ_DYNAMIC)
	&& (cur_output_format == BT2020_PQ_DYNAMIC)) {
		if (get_hdr10_plus_pkt_delay()) {
			update_hdr10_plus_pkt(false,
				(void *)NULL,
				(void *)NULL);
		} else if (vdev->fresh_tx_hdr10plus_pkt)
			vdev->fresh_tx_hdr10plus_pkt(0,
				hdmitx_hdr10plus_param);
		pr_csc(4, "am_vecm: vd%d hdmi clean hdr10+ pkt\n",
			vd_path + 1);
	}

	if (output_format != target_format[vd_path]) {
		pr_csc(4,
			"am_vecm: vd%d %s %s, vd2 %s %s, output_format %s => %s\n",
			vd_path + 1,
			is_video_layer_on(VD1_PATH) ? "on" : "off",
			output_str[target_format[VD1_PATH]],
			is_video_layer_on(VD2_PATH) ? "on" : "off",
			output_str[target_format[VD2_PATH]],
			output_str[cur_output_format],
			output_str[target_format[vd_path]]);
		output_format = target_format[vd_path];
	}

	switch (output_format) {
	case BT709:
		send_info.features =
			/* default 709 limit */
			(0 << 30) /*sdr output 709*/
			| (1 << 29)	/*video available*/
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/*color available*/
			| (1 << 16)	/* bt709 */
			| (1 << 8)	/* bt709 */
			| (1 << 0);	/* bt709 */
		break;
	case BT2020:
		send_info.features =
			/* default 709 full */
			(1 << 30) /*sdr output 709*/
			| (1 << 29) /*video available*/
			| (5 << 26) /* unspecified */
			| (0 << 25) /* limit */
			| (1 << 24) /*color available*/
			| (9 << 16) /* 2020 */
			| (1 << 8)	/* bt709 */
			| (10 << 0);
		break;
	case BT2020_PQ:
		send_info.features =
			(0 << 30) /*sdr output 709*/
			| (1 << 29)	/*video available*/
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/*color available*/
			| (9 << 16)
			| (16 << 8)
			| (10 << 0);	/* bt2020c */
		break;
	case BT2020_HLG:
		send_info.features =
			(0 << 30) /*sdr output 709*/
			| (1 << 29)	/*video available*/
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/*color available*/
			| (9 << 16)
			| (18 << 8)
			| (10 << 0);
		break;
	case BT2020_PQ_DYNAMIC:
		send_info.features =
			(0 << 30) /*sdr output 709*/
			| (1 << 29)	/*video available*/
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/*color available*/
			| (9 << 16)
			| (16 << 8)  /* Always HDR10 */
			| (10 << 0); /* bt2020c */
		break;
	case UNKNOWN_FMT:
	case BT2100_IPT:
		/* handle by dolby vision */
		return;
	}

	/* drm */
	prepare_hdr_info(&send_info, p);

	/* hdr10+ */
	if ((output_format == BT2020_PQ_DYNAMIC)
	&& hdmitx_hdr10plus_param) {
		if (get_hdr10_plus_pkt_delay()) {
			update_hdr10_plus_pkt(true,
				(void *)hdmitx_hdr10plus_param,
				(void *)&send_info);
		} else {
			if (vdev->fresh_tx_hdr_pkt)
				vdev->fresh_tx_hdr_pkt(&send_info);
			if (vdev->fresh_tx_hdr10plus_pkt)
				vdev->fresh_tx_hdr10plus_pkt(
					1, hdmitx_hdr10plus_param);
		}
		return;
	}
	/* none hdr+ */
	if (vdev->fresh_tx_hdr_pkt)
		vdev->fresh_tx_hdr_pkt(&send_info);
}

void video_post_process(
	enum vpp_matrix_csc_e csc_type,
	struct vinfo_s *vinfo,
	enum vd_path_e vd_path)
{
	enum hdr_type_e src_format = cur_source_format[vd_path];

	if (get_hdr_module_status(vd_path) == HDR_MODULE_OFF) {
		if (vd_path == VD1_PATH)
			hdr_proc(VD1_HDR, HDR_BYPASS, vinfo);
		else
			hdr_proc(VD2_HDR, HDR_BYPASS, vinfo);
		if (((vd_path == VD1_PATH) &&
		     !is_video_layer_on(VD2_PATH)) ||
		    ((vd_path == VD2_PATH) &&
		     !is_video_layer_on(VD1_PATH)))
			hdr_proc(OSD1_HDR, HDR_BYPASS, vinfo);
		src_format = HDRTYPE_NONE;
	}

	switch (src_format) {
	case HDRTYPE_SDR:
		if (vd_path == VD2_PATH && is_dolby_vision_on()) {
			hdr_proc(VD2_HDR, SDR_IPT, vinfo);
		} else if (sdr_process_mode[vd_path] == PROC_BYPASS) {
			if (vd_path == VD1_PATH)
				hdr_proc(VD1_HDR, HDR_BYPASS, vinfo);
			else
				hdr_proc(VD2_HDR, HDR_BYPASS, vinfo);
			if (((vd_path == VD1_PATH) &&
			!is_video_layer_on(VD2_PATH))
			|| ((vd_path == VD2_PATH) &&
			!is_video_layer_on(VD1_PATH)))
				hdr_proc(OSD1_HDR, HDR_BYPASS, vinfo);
		} else if (sdr_process_mode[vd_path] == PROC_SDR_TO_HDR) {
			if (vd_path == VD1_PATH)
				hdr_proc(VD1_HDR, SDR_HDR, vinfo);
			else
				hdr_proc(VD2_HDR, SDR_HDR, vinfo);
			hdr_proc(OSD1_HDR, SDR_HDR, vinfo);
		} else if (sdr_process_mode[vd_path] == PROC_SDR_TO_HLG) {
			if (vd_path == VD1_PATH)
				hdr_proc(VD1_HDR, SDR_HLG, vinfo);
			else if (vd_path == VD2_PATH)
				hdr_proc(VD2_HDR, SDR_HLG, vinfo);
			hdr_proc(OSD1_HDR, SDR_HLG, vinfo);
		}
		break;
	case HDRTYPE_HDR10:
		if (vd_path == VD2_PATH && is_dolby_vision_on()) {
			hdr_proc(VD2_HDR, HDR_IPT, vinfo);
		} else if (hdr_process_mode[vd_path] == PROC_BYPASS) {
			if (vd_path == VD1_PATH)
				hdr_proc(VD1_HDR, HDR_BYPASS, vinfo);
			else
				hdr_proc(VD2_HDR, HDR_BYPASS, vinfo);
			hdr_proc(OSD1_HDR, SDR_HDR, vinfo);
		} else if (hdr_process_mode[vd_path] == PROC_HDR_TO_SDR) {
			if (vd_path == VD1_PATH) {
				hdr_proc(VD1_HDR, HDR_SDR, vinfo);
				hdr10_plus_process_update(
					content_max_lumin[vd_path]);
			} else {
				hdr_proc(VD2_HDR, HDR_SDR, vinfo);
			}
			hdr_proc(OSD1_HDR, HDR_BYPASS, vinfo);
		} else if (hdr_process_mode[vd_path] == PROC_HDR_TO_HLG) {
			if (vd_path == VD1_PATH)
				hdr_proc(VD1_HDR, HDR_HLG, vinfo);
			else
				hdr_proc(VD2_HDR, HDR_HLG, vinfo);
			hdr_proc(OSD1_HDR, SDR_HLG, vinfo);
		}
		break;
	case HDRTYPE_HLG:
		if (vd_path == VD2_PATH && is_dolby_vision_on()) {
			hdr_proc(VD2_HDR, HLG_IPT, vinfo);
		} else if (hlg_process_mode[vd_path] == PROC_BYPASS) {
			if (vd_path == VD1_PATH)
				hdr_proc(VD1_HDR, HDR_BYPASS, vinfo);
			else
				hdr_proc(VD2_HDR, HDR_BYPASS, vinfo);
			hdr_proc(OSD1_HDR, SDR_HLG, vinfo);
		} else if (hlg_process_mode[vd_path] == PROC_HLG_TO_SDR) {
			if (vd_path == VD1_PATH)
				hdr_proc(VD1_HDR, HLG_SDR, vinfo);
			else
				hdr_proc(VD2_HDR, HLG_SDR, vinfo);
			hdr_proc(OSD1_HDR, HDR_BYPASS, vinfo);
		} else if (hlg_process_mode[vd_path] == PROC_HLG_TO_HDR) {
			if (vd_path == VD1_PATH)
				hdr_proc(VD1_HDR, HLG_HDR, vinfo);
			else
				hdr_proc(VD2_HDR, HLG_HDR, vinfo);
			hdr_proc(OSD1_HDR, SDR_HDR, vinfo);
		}

		break;
	case HDRTYPE_HDR10PLUS:
		if ((vd_path == VD2_PATH) && is_dolby_vision_on()) {
			hdr_proc(VD2_HDR, HDR_IPT, vinfo);
		} else if (hdr10_plus_process_mode[vd_path] ==
		PROC_BYPASS) {
			if (vd_path == VD1_PATH)
				hdr_proc(VD1_HDR, HDR_BYPASS, vinfo);
			else
				hdr_proc(VD2_HDR, HDR_BYPASS, vinfo);
			hdr_proc(OSD1_HDR, SDR_HDR, vinfo);
		} else if (hdr10_plus_process_mode[vd_path] ==
		PROC_HDRP_TO_HDR) {
			if (vd_path == VD1_PATH)
				hdr_proc(VD1_HDR, HDR_BYPASS, vinfo);
			else
				hdr_proc(VD2_HDR, HDR_BYPASS, vinfo);
			hdr_proc(OSD1_HDR, SDR_HDR, vinfo);
		} else if (hdr10_plus_process_mode[vd_path] ==
		PROC_HDRP_TO_SDR) {
			if (vd_path == VD1_PATH)
				hdr_proc(VD1_HDR, HDR10P_SDR, vinfo);
			else
				hdr_proc(VD2_HDR, HDR10P_SDR, vinfo);
			hdr_proc(OSD1_HDR, HDR_BYPASS, vinfo);
		} else if (hdr10_plus_process_mode[vd_path] ==
		PROC_HDRP_TO_HLG) {
			if (vd_path == VD1_PATH)
				hdr_proc(VD1_HDR, HDR_HLG, vinfo);
			else
				hdr_proc(VD2_HDR, HDR_HLG, vinfo);
			hdr_proc(OSD1_HDR, SDR_HLG, vinfo);
		}
		break;
	case HDRTYPE_MVC:
		hdr_osd_off();
		hdr_vd1_off();
		hdr_vd2_off();
		break;
	default:
		break;
	}

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		if (vinfo->viu_color_fmt != COLOR_FMT_RGB444)
			mtx_setting(POST2_MTX, MATRIX_NULL, MTX_OFF);
		else
			mtx_setting(POST2_MTX,
				MATRIX_YUV709_RGB, MTX_ON);
	}

	if (cur_sdr_process_mode[vd_path] !=
		sdr_process_mode[vd_path]) {
		if (cur_source_format[vd_path] == HDRTYPE_SDR)
			pr_csc(1,
				"am_vecm: vd%d: sdr_process_mode %d to %d\n",
				vd_path + 1,
				cur_sdr_process_mode[vd_path],
				sdr_process_mode[vd_path]);
		if (cur_source_format[vd_path] == HDRTYPE_MVC)
			pr_csc(1,
			       "am_vecm: vd%d: mvc_process_mode %d to %d\n",
			       vd_path + 1,
			       cur_sdr_process_mode[vd_path],
			       sdr_process_mode[vd_path]);
		cur_sdr_process_mode[vd_path] =
			sdr_process_mode[vd_path];
	}
	if (cur_hdr_process_mode[vd_path] !=
		hdr_process_mode[vd_path]) {
		if (cur_source_format[vd_path] == HDRTYPE_HDR10)
			pr_csc(1,
				"am_vecm: vd%d: hdr_process_mode %d to %d\n",
				vd_path + 1,
				cur_hdr_process_mode[vd_path],
				hdr_process_mode[vd_path]);
		cur_hdr_process_mode[vd_path] =
			hdr_process_mode[vd_path];
	}
	if (cur_hlg_process_mode[vd_path] !=
		hlg_process_mode[vd_path]) {
		if (cur_source_format[vd_path] == HDRTYPE_HLG)
			pr_csc(1,
				"am_vecm: vd%d: hlg_process_mode %d to %d\n",
				vd_path + 1,
				cur_hlg_process_mode[vd_path],
				hlg_process_mode[vd_path]);
		cur_hlg_process_mode[vd_path] =
			hlg_process_mode[vd_path];
	}
	if (cur_hdr10_plus_process_mode[vd_path] !=
		hdr10_plus_process_mode[vd_path]) {
		if (cur_source_format[vd_path] == HDRTYPE_HDR10PLUS)
			pr_csc(1,
				"am_vecm: vd%d: hdr10_plus_process_mode %d to %d\n",
				vd_path + 1,
				cur_hdr10_plus_process_mode[vd_path],
				hdr10_plus_process_mode[vd_path]);
		cur_hdr10_plus_process_mode[vd_path] =
			hdr10_plus_process_mode[vd_path];
	}
	if (cur_csc_type[vd_path] != csc_type) {
		pr_csc(1, "am_vecm: vd%d: csc from 0x%x to 0x%x.\n",
			vd_path + 1, cur_csc_type[vd_path], csc_type);
		cur_csc_type[vd_path] = csc_type;
	}
}
