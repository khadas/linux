// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vpq_vfm.h"
#include "vpq_printk.h"
#include "vpq_table_logic.h"
#include "vpq_drv.h"

static vpq_vfm_source_type_e cur_src_type;
static vpq_vfm_source_type_e pre_src_type;
static vpq_vfm_sig_status_e cur_sig_status;
static vpq_vfm_sig_status_e pre_sig_status;
static vpq_vfm_sig_fmt_e cur_sig_fmt;
static vpq_vfm_sig_fmt_e pre_sig_fmt;
static vpq_vfm_trans_fmt_e cur_trans_fmt;
static vpq_vfm_port_e cur_src_port;
static vpq_vfm_port_e pre_src_port;
static vpq_vfm_source_mode_e cur_src_mode;
static enum vpq_vfm_color_primary_e cur_color_primaries;
static enum vpq_vfm_hdr_type_e cur_hdr_type;
static enum vpq_vfm_hdr_type_e pre_hdr_type;
static enum vpq_vfm_scan_mode_e cur_scan_mode;
static bool cur_is_dlby;
static unsigned int src_height, src_width;
static unsigned int pre_height, pre_width;
static unsigned int cur_fps;

#define SIGNAL_TRANSFER_CHARACTERISTIC(type) ((type >> 8) & 0xff)
#define SIGNAL_COLOR_PRIMARIES(type) ((type >> 16) & 0xff)
#define SIGNAL_IS_DLBY(type) ((type >> 30) & 0x01)

void vpq_vfm_init(void)
{
	VPQ_PR_DRV("%s\n", __func__);

	pre_src_type = VFRAME_SOURCE_TYPE_HWC;
	pre_sig_status = TVIN_SIG_STATUS_NULL;
	pre_sig_fmt = TVIN_SIG_FMT_NULL;
	pre_src_port = TVIN_PORT_NULL;
	pre_hdr_type = VPQ_VFM_HDR_TYPE_NONE;
	pre_height = 0;
	pre_width = 0;
}

static void _vpq_vfm_update_source_type(enum vframe_source_type_e src_type)
{
	cur_src_type = src_type;
}

//static void _vpq_vfm_update_signal_status(enum tvin_sig_status_e sig_status)
//{
//	cur_sig_status = sig_status;
//}

static void _vpq_vfm_update_signal_format(enum tvin_sig_fmt_e sig_fmt)
{
	cur_sig_fmt = sig_fmt;
}

static void _vpq_vfm_update_trans_format(enum tvin_trans_fmt trans_fmt)
{
	cur_trans_fmt = trans_fmt;
}

static void _vpq_vfm_update_source_port(enum tvin_port_e src_port)
{
	cur_src_port = src_port;
}

static void _vpq_vfm_update_source_mode(enum vframe_source_mode_e src_mode)
{
	cur_src_mode = src_mode;
}

static void _vpq_vfm_update_height_width(unsigned int type,
	unsigned int compHeight, unsigned int compWidth,
	unsigned int height, unsigned int width)
{
	src_height = (type & VIDTYPE_COMPRESS) ? compHeight : height;
	src_width = (type & VIDTYPE_COMPRESS) ? compWidth : width;
	//VPQ_PR_INFO(PR_VFM, "%s src_height:%d src_width:%d\n",
	//	__func__, src_height, src_width);
}

static void _vpq_vfm_update_color_primaries(unsigned int signal_type)
{
	/* bit 23-16: color_primaries
	 * "unknown", "bt709", "undef", "bt601", "bt470m", "bt470bg",
	 * "smpte170m", "smpte240m", "film", "bt2020"
	 */

	unsigned int cp = SIGNAL_COLOR_PRIMARIES(signal_type);

	if (cp == 0x01)
		cur_color_primaries = VPQ_COLOR_PRIM_BT709;
	else if (cp == 0x03)
		cur_color_primaries = VPQ_COLOR_PRIM_BT601;
	else if (cp == 0x09)
		cur_color_primaries = VPQ_COLOR_PRIM_BT2020;
	else
		cur_color_primaries = VPQ_COLOR_PRIM_NULL;

	//VPQ_PR_INFO(PR_VFM, "%s cur_color_primaries:%d\n",
	//	__func__, cur_color_primaries);
}

static void _vpq_vfm_update_transfer_characteristic(unsigned int signal_type)
{
	/* bit 15-8: transfer_characteristic
	 * "unknown", "bt709", "undef", "bt601", "bt470m", "bt470bg",
	 * "smpte170m", "smpte240m", "linear", "log100", "log316",
	 * "iec61966-2-4", "bt1361e", "iec61966-2-1", "bt2020-10",
	 * "bt2020-12", "smpte-st-2084", "smpte-st-428"
	 */

	unsigned int tc = SIGNAL_TRANSFER_CHARACTERISTIC(signal_type);

	if ((tc == 0x0e || tc == 0x12) && cur_color_primaries == VPQ_COLOR_PRIM_BT2020)
		cur_hdr_type = VPQ_VFM_HDR_TYPE_HLG;
	else if (tc == 0x30 && cur_color_primaries == VPQ_COLOR_PRIM_BT2020)
		cur_hdr_type = VPQ_VFM_HDR_TYPE_HDR10PLUS;
	else if (tc == 0x10)
		cur_hdr_type = VPQ_VFM_HDR_TYPE_HDR10;
	else
		cur_hdr_type = VPQ_VFM_HDR_TYPE_SDR;

	//VPQ_PR_INFO(PR_VFM, "%s cur_hdr_type:%d\n", __func__, cur_hdr_type);
}

static void _vpq_vfm_update_is_dlby(unsigned int signal_type, bool discard_dv_data)
{
	/* judge dlby
	 * 1) vdin path; signal_type; bit 30; 0:not dv 1:dv
	 * 2) decoder path; discard_dv_data; 0:dv 1:not dv
	 */
	if (cur_src_type == VFRAME_SOURCE_TYPE_HDMI)
		cur_is_dlby = SIGNAL_IS_DLBY(signal_type) ? true : false;
	else if (cur_src_type == VFRAME_SOURCE_TYPE_OTHERS)
		cur_is_dlby = discard_dv_data ? false : true;
	else
		; //no dlby source

	//VPQ_PR_INFO(PR_VFM, "%s cur_is_dlby:%d\n", __func__, cur_is_dlby);
}

static void _vpq_vfm_update_signal_scan_mode(unsigned int type_original)
{
	if ((type_original & VIDTYPE_TYPEMASK) == VIDTYPE_PROGRESSIVE) {
		cur_scan_mode = VPQ_VFM_SCAN_MODE_PROGRESSIVE;
	} else if ((type_original & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP ||
			   (type_original & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_BOTTOM) {
		cur_scan_mode = VPQ_VFM_SCAN_MODE_INTERLACED;
	} else {
		cur_scan_mode = VPQ_VFM_SCAN_MODE_NULL;
	}

	//VPQ_PR_INFO(PR_VFM, "%s cur_scan_mode:0x%x\n", __func__, cur_scan_mode);
}

static void _vpq_vfm_update_fps(unsigned int duration)
{
	/*duration:
	 * 800(120fps) 801(119.88fps) 960(100fps) 1600(60fps) 1920(50fps)
	 * 3200(30fps) 3203(29.97) 3840(25fps) 4000(24fps) 4004(23.976fps)
	 */

	if (duration == 800 || duration == 801)
		cur_fps = 120;
	else if (duration == 960)
		cur_fps = 100;
	else if (duration == 1600)
		cur_fps = 60;
	else if (duration == 1920)
		cur_fps = 50;
	else if (duration == 3200 || duration == 3203)
		cur_fps = 30;
	else if (duration == 3840)
		cur_fps = 25;
	else if (duration == 4000 || duration == 4004)
		cur_fps = 24;

	//VPQ_PR_INFO(PR_VFM, "%s cur_fps:%d\n", __func__, cur_fps);
}

static void _vpq_vfm_update_pq_effect(void)
{
	/* during hdmi src */
	if (cur_src_type == VFRAME_SOURCE_TYPE_HDMI) {
		/* switch src to hdmi case
		 * switch sig fmt case
		 * switch hdr type case
		 * switch port case
		 */
		if (pre_src_type != VFRAME_SOURCE_TYPE_HDMI ||
			cur_sig_fmt != pre_sig_fmt ||
			cur_hdr_type != pre_hdr_type ||
			cur_src_port != pre_src_port) {
			VPQ_PR_DRV("%s trigger pq update for hdmi\n", __func__);
			VPQ_PR_DRV("%s current status %d 0x%x %d 0x%x\n",
				__func__, cur_src_type, cur_sig_fmt, cur_hdr_type, cur_src_port);
			VPQ_PR_DRV("%s previes status %d 0x%x %d 0x%x\n",
				__func__, pre_src_type, pre_sig_fmt, pre_hdr_type, pre_src_port);

			vpq_set_pq_effect();

			pre_src_type = cur_src_type;
			pre_sig_fmt = cur_sig_fmt;
			pre_hdr_type = cur_hdr_type;
			pre_src_port = cur_src_port;

			vpq_vfm_send_event(VPQ_VFM_EVENT_SIG_INFO_CHANGE);
		}
	}
	/* during av src */
	else if (cur_src_type == VFRAME_SOURCE_TYPE_CVBS) {
		/* switch src to av case
		 * switch sig fmt case
		 */
		if (pre_src_type != VFRAME_SOURCE_TYPE_CVBS ||
			cur_sig_fmt != pre_sig_fmt) {
			VPQ_PR_DRV("%s trigger pq update for av\n", __func__);
			VPQ_PR_DRV("%s current status %d 0x%x\n",
				__func__, cur_src_type, cur_sig_fmt);
			VPQ_PR_DRV("%s previes status %d 0x%x\n",
				__func__, pre_src_type, pre_sig_fmt);

			vpq_set_pq_effect();

			pre_src_type = cur_src_type;
			pre_sig_fmt = cur_sig_fmt;

			vpq_vfm_send_event(VPQ_VFM_EVENT_SIG_INFO_CHANGE);
		}
	}
	/* during mpeg src */
	else if (cur_src_type == VFRAME_SOURCE_TYPE_OTHERS) {
		/* switch src to mpeg case
		 * switch sig resolution case
		 * switch hdr type case
		 */
		if (pre_src_type != VFRAME_SOURCE_TYPE_OTHERS ||
			src_height != pre_height ||
			src_width != pre_width ||
			cur_hdr_type != pre_hdr_type) {
			VPQ_PR_DRV("%s trigger pq update for mpeg\n", __func__);
			VPQ_PR_DRV("%s current status %d %d %d %d\n",
				__func__, cur_src_type, src_height, src_width, cur_hdr_type);
			VPQ_PR_DRV("%s previes status %d %d %d %d\n",
				__func__, pre_src_type, pre_height, pre_width, pre_hdr_type);

			vpq_set_pq_effect();

			pre_src_type = cur_src_type;
			pre_height = src_height;
			pre_width = src_width;
			pre_hdr_type = cur_hdr_type;

			vpq_vfm_send_event(VPQ_VFM_EVENT_SIG_INFO_CHANGE);
		}
	}
}

void vpq_vfm_process(struct vframe_s *pvf)
{
	if (!pvf) {
		VPQ_PR_INFO(PR_VFM, "%s pvf is null\n", __func__);
		return;
	}

	//update local signal information
	_vpq_vfm_update_source_type(pvf->source_type);
	//_vpq_vfm_update_signal_status(pvf->status);
	_vpq_vfm_update_signal_format(pvf->sig_fmt);
	_vpq_vfm_update_trans_format(pvf->trans_fmt);
	_vpq_vfm_update_source_port(pvf->port);
	_vpq_vfm_update_source_mode(pvf->source_mode);
	_vpq_vfm_update_color_primaries(pvf->signal_type);
	_vpq_vfm_update_transfer_characteristic(pvf->signal_type);
	_vpq_vfm_update_is_dlby(pvf->signal_type, pvf->discard_dv_data);
	_vpq_vfm_update_signal_scan_mode(pvf->type_original);
	_vpq_vfm_update_height_width(pvf->type, pvf->compHeight, pvf->compWidth,
								 pvf->height, pvf->width);
	_vpq_vfm_update_fps(pvf->duration);

	VPQ_PR_INFO(PR_VFM, "%s\n"
		"source_type:%d sig_fmt:0x%x     trans_fmt:%d\n"
		"port:0x%x      source_mode:%d   type:%d\n"
		"compHeight:%d  compWidth:%d     height:%d\n"
		"width:%d       signal_type:0x%x discard_dv_data:%d\n"
		"type_original:%d duration:%d\n",
		__func__,
		pvf->source_type, pvf->sig_fmt, pvf->trans_fmt,
		pvf->port, pvf->source_mode, pvf->type,
		pvf->compHeight, pvf->compWidth, pvf->height,
		pvf->width, pvf->signal_type, pvf->discard_dv_data,
		pvf->type_original, pvf->duration);

	//trigger pq update when vf changed
	_vpq_vfm_update_pq_effect();
}

void vpq_vfm_video_enable(int enable)
{
	VPQ_PR_INFO(PR_VFM, "%s\n", __func__);

	pre_src_type = VFRAME_SOURCE_TYPE_HWC;
	pre_sig_status = TVIN_SIG_STATUS_NULL;
	pre_sig_fmt = TVIN_SIG_FMT_NULL;
	pre_src_port = TVIN_PORT_NULL;
	pre_hdr_type = VPQ_VFM_HDR_TYPE_NONE;
	pre_height = 0;
	pre_width = 0;

	cur_src_type = VFRAME_SOURCE_TYPE_HWC;
	cur_sig_status = TVIN_SIG_STATUS_NULL;
	cur_sig_fmt = TVIN_SIG_FMT_NULL;
	cur_src_port = TVIN_PORT_NULL;
	cur_hdr_type = VPQ_VFM_HDR_TYPE_NONE;
	cur_scan_mode = VPQ_VFM_SCAN_MODE_NULL;
	cur_is_dlby = false;
	src_height = 0;
	src_width = 0;
}

/* private api for other files to get current signal information */
vpq_vfm_source_type_e vpq_vfm_get_source_type(void)
{
	return cur_src_type;
}

vpq_vfm_sig_status_e vpq_vfm_get_signal_status(void)
{
	return cur_sig_status;
}

vpq_vfm_sig_fmt_e vpq_vfm_get_signal_format(void)
{
	return cur_sig_fmt;
}

vpq_vfm_trans_fmt_e vpq_vfm_get_trans_format(void)
{
	return cur_trans_fmt;
}

vpq_vfm_port_e vpq_vfm_get_source_port(void)
{
	return cur_src_port;
}

vpq_vfm_source_mode_e vpq_vfm_get_source_mode(void)
{
	return cur_src_mode;
}

enum vpq_vfm_color_primary_e vpq_vfm_get_color_primaries(void)
{
	return cur_color_primaries;
}

enum vpq_vfm_hdr_type_e vpq_vfm_get_hdr_type(void)
{
	return cur_hdr_type;

	/* Note:
	 * or whether can directly use vframe.h bellow api to replace cur_hdr_type?
	 * enum vframe_signal_fmt_e signal_fmt = get_vframe_src_fmt(pvf);
	 */
}

bool vpq_vfm_get_is_dlby(void)
{
	return cur_is_dlby;
}

void vpq_frm_get_height_width(unsigned int *height, unsigned int *width)
{
	*height = src_height;
	*width = src_width;
}

enum vpq_vfm_scan_mode_e vpq_vfm_get_signal_scan_mode(void)
{
	return cur_scan_mode;
}

void vpq_frm_get_fps(unsigned int *fps)
{
	*fps = cur_fps;
}

void vpq_vfm_send_event(enum vpq_vfm_event_info_e event_info)
{
	VPQ_PR_DRV("%s start\n", __func__);

	struct vpq_dev_s *vpq_devp = get_vpq_dev();

	vpq_devp->event_info = (unsigned int)VPQ_VFM_EVENT_SIG_INFO_CHANGE;

	schedule_delayed_work(&vpq_devp->event_dwork, 0);

	wake_up(&vpq_devp->queue);
}

enum vpq_vfm_event_info_e vpq_vfm_get_event_info(void)
{
	struct vpq_dev_s *vpq_devp = get_vpq_dev();

	VPQ_PR_DRV("%s vpq_devp->event_info:%d\n", __func__, vpq_devp->event_info);

	return (enum vpq_vfm_event_info_e)vpq_devp->event_info;
}
