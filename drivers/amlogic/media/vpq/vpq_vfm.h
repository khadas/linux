/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_VFM_H__
#define __VPQ_VFM_H__

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/tvin.h>

#define vpq_vfm_source_type_e enum vframe_source_type_e
#define vpq_vfm_sig_status_e  enum tvin_sig_status_e
#define vpq_vfm_sig_fmt_e     enum tvin_sig_fmt_e
#define vpq_vfm_trans_fmt_e   enum tvin_trans_fmt
#define vpq_vfm_port_e        enum tvin_port_e
#define vpq_vfm_source_mode_e enum vframe_source_mode_e

enum vpq_vfm_hdr_type_e {
	VPQ_VFM_HDR_TYPE_NONE = 0,
	VPQ_VFM_HDR_TYPE_HDR10,
	VPQ_VFM_HDR_TYPE_HDR10PLUS,
	VPQ_VFM_HDR_TYPE_DOBVI,
	VPQ_VFM_HDR_TYPE_PRIMESL,
	VPQ_VFM_HDR_TYPE_HLG,
	VPQ_VFM_HDR_TYPE_SDR,
	VPQ_VFM_HDR_TYPE_MVC,
};

enum vpq_vfm_color_primary_e {
	VPQ_COLOR_PRIM_NULL = 0,
	VPQ_COLOR_PRIM_BT601,
	VPQ_COLOR_PRIM_BT709,
	VPQ_COLOR_PRIM_BT2020,
	VPQ_COLOR_PRIM_MAX,
};

enum vpq_vfm_scan_mode_e {
	VPQ_VFM_SCAN_MODE_NULL = 0,
	VPQ_VFM_SCAN_MODE_PROGRESSIVE,
	VPQ_VFM_SCAN_MODE_INTERLACED,
};

enum vpq_vfm_event_info_e {
	VPQ_VFM_EVENT_NONE = 0,
	VPQ_VFM_EVENT_SIG_INFO_CHANGE,
};

void vpq_vfm_init(void);
vpq_vfm_source_type_e vpq_vfm_get_source_type(void);
vpq_vfm_sig_status_e vpq_vfm_get_signal_status(void);
vpq_vfm_sig_fmt_e vpq_vfm_get_signal_format(void);
vpq_vfm_trans_fmt_e vpq_vfm_get_trans_format(void);
vpq_vfm_port_e vpq_vfm_get_source_port(void);
vpq_vfm_source_mode_e vpq_vfm_get_source_mode(void);
enum vpq_vfm_color_primary_e vpq_vfm_get_color_primaries(void);
enum vpq_vfm_hdr_type_e vpq_vfm_get_hdr_type(void);
bool vpq_vfm_get_is_dlby(void);
void vpq_frm_get_height_width(unsigned int *height, unsigned int *width);
enum vpq_vfm_scan_mode_e vpq_vfm_get_signal_scan_mode(void);
void vpq_frm_get_fps(unsigned int *fps);

void vpq_vfm_send_event(enum vpq_vfm_event_info_e event_info);
enum vpq_vfm_event_info_e vpq_vfm_get_event_info(void);

#endif //__VPQ_VFM_H__
