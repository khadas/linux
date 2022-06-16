// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/drm_modeset_helper.h>
#include <drm/drmP.h>
#include <drm/drm_edid.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_hdcp.h>
#include <drm/drm_modeset_lock.h>

#include <linux/component.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_common.h>
#include <linux/miscdevice.h>
#include <drm/amlogic/meson_connector_dev.h>
#include <vout/vout_serve/vout_func.h>
#include <enhancement/amvecm/amcsc.h>

#include "meson_hdmi.h"
#include "meson_vpu.h"
#include "meson_crtc.h"

#define HDMITX_ATTR_LEN_MAX	16
#define HDMITX_MAX_BPC	12
#define MAX_VRR_MODE_GROUP 12

struct am_hdmi_tx am_hdmi_info;
bool attr_force_debugfs;
char attr_debugfs[16];

/*for hw limitiation, limit to 1080p/720p for recovery ui.*/
static bool hdmitx_set_smaller_pref = true;

/*TODO:will remove later.*/
static struct drm_display_mode dummy_mode = {
	.name = "dummy_l",
	.type = DRM_MODE_TYPE_USERDEF,
	.status = MODE_OK,
	.clock = 25000,
	.hdisplay = 720,
	.hsync_start = 736,
	.hsync_end = 798,
	.htotal = 858,
	.hskew = 0,
	.vdisplay = 480,
	.vsync_start = 489,
	.vsync_end = 495,
	.vtotal = 525,
	.vscan = 0,
	.vrefresh = 50,
	.flags =  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
};

struct hdmitx_color_attr dv_color_attr_list[] = {
	{COLORSPACE_YUV444, 8}, //"444,8bit"
	{COLORSPACE_RESERVED, COLORDEPTH_RESERVED}
};

struct hdmitx_color_attr dv_ll_color_attr_list[] = {
	{COLORSPACE_YUV422, 12}, //"422,12bit"
	{COLORSPACE_RESERVED, COLORDEPTH_RESERVED}
};

/* this is prior selected list of
 * 4k2k50hz, 4k2k60hz smpte50hz, smpte60hz
 */
struct hdmitx_color_attr color_attr_list[] = {
	{COLORSPACE_YUV420, 10}, //"420,10bit"
	{COLORSPACE_YUV422, 12}, //"422,12bit"
	{COLORSPACE_YUV420, 8}, //"420,8bit"
	{COLORSPACE_YUV444, 8}, //"444,8bit"
	{COLORSPACE_RGB444, 8}, //"rgb,8bit"
	{COLORSPACE_RESERVED, COLORDEPTH_RESERVED}
};

/* this is prior selected list of other display mode */
struct hdmitx_color_attr other_color_attr_list[] = {
	{COLORSPACE_YUV444, 10}, //"444,10bit"
	{COLORSPACE_YUV422, 12}, //"422,12bit"
	{COLORSPACE_RGB444, 10}, //"rgb,10bit"
	{COLORSPACE_YUV444, 8}, //"444,8bit"
	{COLORSPACE_RGB444, 8}, //"rgb,8bit"
	{COLORSPACE_RESERVED, COLORDEPTH_RESERVED}
};

#define MODE_4K2K24HZ                   "2160p24hz"
#define MODE_4K2K25HZ                   "2160p25hz"
#define MODE_4K2K30HZ                   "2160p30hz"
#define MODE_4K2K50HZ                   "2160p50hz"
#define MODE_4K2K60HZ                   "2160p60hz"
#define MODE_4K2KSMPTE                  "smpte24hz"
#define MODE_4K2KSMPTE30HZ              "smpte30hz"
#define MODE_4K2KSMPTE50HZ              "smpte50hz"
#define MODE_4K2KSMPTE60HZ              "smpte60hz"

void convert_attrstr(char *attr_str,
	struct hdmitx_color_attr *attr_param)
{
	attr_param->colorformat = COLORSPACE_RESERVED;
	attr_param->bitdepth = COLORDEPTH_RESERVED;

	if (strstr(attr_str, "420"))
		attr_param->colorformat = COLORSPACE_YUV420;
	else if (strstr(attr_str, "422"))
		attr_param->colorformat = COLORSPACE_YUV422;
	else if (strstr(attr_str, "444"))
		attr_param->colorformat = COLORSPACE_YUV444;
	else if (strstr(attr_str, "rgb"))
		attr_param->colorformat = COLORSPACE_RGB444;

	/*parse colorspace success*/
	if (attr_param->colorformat != COLORSPACE_RESERVED) {
		if (strstr(attr_str, "12bit"))
			attr_param->bitdepth = 12;
		else if (strstr(attr_str, "10bit"))
			attr_param->bitdepth = 10;
		else if (strstr(attr_str, "8bit"))
			attr_param->bitdepth = 8;
	}
}

static int convert_attrstr_to_colorspace(char *attr_str)
{
	if (strstr(attr_str, "420"))
		return COLORSPACE_YUV420;
	else if (strstr(attr_str, "422"))
		return COLORSPACE_YUV422;
	else if (strstr(attr_str, "444"))
		return COLORSPACE_YUV444;
	else if (strstr(attr_str, "rgb"))
		return COLORSPACE_RGB444;
	else
		return  COLORSPACE_RESERVED;
}

static int convert_attrstr_to_colordepth(char *attr_str)
{
	if (strstr(attr_str, "12bit"))
		return 12;
	else if (strstr(attr_str, "10bit"))
		return 10;
	else if (strstr(attr_str, "8bit"))
		return 8;
	else
		return COLORDEPTH_RESERVED;
}

static void build_hdmitx_attr_str(char *attr_str, u32 format, u32 bit_depth)
{
	const char *colorspace;

	switch (format) {
	case COLORSPACE_YUV420:
		colorspace = "420";
		break;
	case COLORSPACE_YUV422:
		colorspace = "422";
		break;
	case COLORSPACE_YUV444:
		colorspace = "444";
		break;
	case COLORSPACE_RGB444:
		colorspace = "rgb";
		break;
	default:
		colorspace = "rgb";
		DRM_ERROR("Unknown colospace valu %d\n", format);
		break;
	};

	sprintf(attr_str, "%s,%dbit", colorspace, bit_depth);
	DRM_INFO("%s:%s = %u+%u\n", __func__, attr_str, format, bit_depth);
}

static struct hdmitx_color_attr *meson_hdmitx_get_candidate_attr_list
	(struct am_meson_crtc_state *crtc_state)
{
	const char *outputmode = crtc_state->base.adjusted_mode.name;
	struct hdmitx_color_attr *attr_list = NULL;

	/* filter some color value options, aimed at some modes. */
	if (crtc_state->crtc_eotf_type ==
			HDMI_EOTF_MESON_DOLBYVISION) {
		attr_list = dv_color_attr_list;
	} else if (crtc_state->crtc_eotf_type ==
			HDMI_EOTF_MESON_DOLBYVISION_LL) {
		attr_list = dv_ll_color_attr_list;
	} else if (!strcmp(outputmode, MODE_4K2K60HZ) ||
	    !strcmp(outputmode, MODE_4K2K50HZ) ||
	    !strcmp(outputmode, MODE_4K2KSMPTE60HZ) ||
	    !strcmp(outputmode, MODE_4K2KSMPTE50HZ)) {
		attr_list = color_attr_list;
	} else {
		attr_list = other_color_attr_list;
	}

	return attr_list;
}

static bool meson_hdmitx_test_color_attr(struct am_meson_crtc_state *crtc_state,
	struct am_hdmitx_connector_state *conn_state,
	struct hdmitx_color_attr *test_attr)
{
	char *outputmode = crtc_state->base.adjusted_mode.name;
	struct hdmitx_color_attr *attr_list = NULL;
	char attr_str[HDMITX_ATTR_LEN_MAX];
	u8 max_bpc = conn_state->base.max_bpc;

	if (test_attr->colorformat == COLORSPACE_RESERVED ||
		test_attr->bitdepth > max_bpc)
		return false;

	attr_list = meson_hdmitx_get_candidate_attr_list(crtc_state);

	do {
		if (attr_list->colorformat == COLORSPACE_RESERVED)
			break;

		if (attr_list->colorformat == test_attr->colorformat &&
			attr_list->bitdepth == test_attr->bitdepth) {
			build_hdmitx_attr_str(attr_str,
				attr_list->colorformat, attr_list->bitdepth);
			if (am_hdmi_info.hdmitx_dev->test_attr(outputmode,
					attr_str)) {
				DRM_INFO("%s success [%d]+[%d]\n", __func__,
					attr_list->colorformat,
					attr_list->bitdepth);
				break;
			}
		}
	} while (attr_list++);

	if (attr_list->colorformat == COLORSPACE_RESERVED)
		return false;
	else
		return true;
}

static int meson_hdmitx_decide_color_attr
	(struct am_meson_crtc_state *crtc_state,
	u8 max_bpc, struct hdmitx_color_attr *attr)
{
	char *outputmode = crtc_state->base.adjusted_mode.name;
	struct hdmitx_color_attr *attr_list = NULL;
	char attr_str[HDMITX_ATTR_LEN_MAX];

	if (!outputmode) {
		DRM_ERROR("%s current mode empty.\n", __func__);
		return -EINVAL;
	}

	attr_list = meson_hdmitx_get_candidate_attr_list(crtc_state);

	do {
		if (attr_list->colorformat == COLORSPACE_RESERVED)
			break;

		if (attr_list->bitdepth <= max_bpc) {
			build_hdmitx_attr_str(attr_str,
				attr_list->colorformat, attr_list->bitdepth);
			if (am_hdmi_info.hdmitx_dev->test_attr(outputmode,
					attr_str)) {
				attr->colorformat = attr_list->colorformat;
				attr->bitdepth = attr_list->bitdepth;
				DRM_INFO("%s get fmt attr [%d]+[%d]\n",
					__func__,
					attr->colorformat,
					attr->bitdepth);
				break;
			}
		}
	} while (attr_list++);
	if (attr_list->colorformat == COLORSPACE_RESERVED) {
		DRM_ERROR("%s no attr found, reset to 444,8bit.\n", __func__);
		attr->colorformat = COLORSPACE_RGB444;
		attr->bitdepth = 8;
	}

	DRM_DEBUG_KMS("[%s]:[%s,bpc:%d,eotf:%d]=>attr[%d,%d]\n", __func__,
		outputmode, max_bpc, crtc_state->crtc_eotf_type,
		attr->colorformat, attr->bitdepth);

	return 0;
}

static int meson_hdmitx_setup_color_attr(struct hdmitx_color_attr *attr)
{
	char hdmitx_attr_str[HDMITX_ATTR_LEN_MAX];

	build_hdmitx_attr_str(hdmitx_attr_str,
		attr->colorformat, attr->bitdepth);
	am_hdmi_info.hdmitx_dev->setup_attr(hdmitx_attr_str);

	DRM_DEBUG_KMS("%s:[%s]\n", __func__, hdmitx_attr_str);
	return 0;
}

int meson_hdmitx_get_modes(struct drm_connector *connector)
{
	bool vrr_cap;
	struct edid *edid;
	int *vics;
	int count = 0, i = 0, len = 0;
	struct drm_display_mode *mode, *pref_mode = NULL;
	struct hdmi_format_para *hdmi_para;
	struct hdmi_cea_timing *timing;
	struct am_hdmi_tx *am_hdmitx = connector_to_am_hdmi(connector);
	char *strp = NULL;
	u32 num, den;

	edid = (struct edid *)am_hdmitx->hdmitx_dev->get_raw_edid();
	drm_connector_update_edid_property(connector, edid);

	/* get vrr capability */
	if (am_hdmitx->hdmitx_dev->get_vrr_cap) {
		vrr_cap = am_hdmitx->hdmitx_dev->get_vrr_cap();
		drm_connector_set_vrr_capable_property(connector, vrr_cap);
	} else {
		drm_connector_set_vrr_capable_property(connector, false);
	}

	/*add modes from hdmitx instead of edid*/
	count = am_hdmitx->hdmitx_dev->get_vic_list(&vics);
	if (count) {
		for (i = 0; i < count; i++) {
			hdmi_para = hdmi_get_fmt_paras(vics[i]);
			timing = &hdmi_para->timing;
			if (hdmi_para->vic == HDMI_UNKNOWN) {
				DRM_ERROR("Get hdmi para by vic [%d] failed.\n", vics[i]);
				continue;
			}

			mode = drm_mode_create(connector->dev);
			if (!mode) {
				DRM_ERROR("drm mode create failed.\n");
				continue;
			}

			strncpy(mode->name, hdmi_para->hdmitx_vinfo.name, DRM_DISPLAY_MODE_LEN);
			/* remove _4x3 suffix, in case misunderstand */
			strp = strstr(mode->name, "_4x3");
			if (strp)
				*strp = '\0';
			/*
			 * filter 4k420 mode, 4k420 mode end with "420"
			 * 2160p60hz420 to 2160p60hz
			 */
			strp = strstr(mode->name, "420");
			if (strp) {
				len = strlen(mode->name) - strlen("420");
				if (!strcmp(mode->name + len, "420"))
					*strp = '\0';
			}

			mode->type = DRM_MODE_TYPE_DRIVER;
			num = hdmi_para->hdmitx_vinfo.sync_duration_num;
			den = hdmi_para->hdmitx_vinfo.sync_duration_den;
			mode->vrefresh = (int)DIV_ROUND_CLOSEST(num, den);
			mode->clock = timing->pixel_freq;

			mode->hdisplay = timing->h_active;
			mode->hsync_start = mode->hdisplay + timing->h_front;
			mode->hsync_end = timing->h_active + timing->h_front + timing->h_sync;

			mode->htotal = timing->h_total;
			/* for 480i/576i, horizontal timing is repeated */
			if (hdmi_para->pixel_repetition_factor == 1) {
				mode->hdisplay >>= 1;
				mode->hsync_start >>= 1;
				mode->hsync_end >>= 1;
				mode->htotal >>= 1;
			}

			mode->hskew = 0;
			mode->flags |= timing->hsync_polarity ?
				DRM_MODE_FLAG_PHSYNC : DRM_MODE_FLAG_NHSYNC;

			mode->vdisplay = timing->v_active;
			mode->vsync_start = mode->vdisplay + timing->v_front;
			mode->vsync_end = mode->vdisplay + timing->v_front + timing->v_sync;
			mode->vtotal = timing->v_total;
			mode->vscan = 0;
			mode->flags |= timing->vsync_polarity ?
				DRM_MODE_FLAG_PVSYNC : DRM_MODE_FLAG_NVSYNC;

			/* use logical display timing for drm. vidsplay
			 * while amlogic vout use half value.
			 */
			if (hdmi_para->hdmitx_vinfo.field_height !=
				hdmi_para->hdmitx_vinfo.height) {
				mode->flags |= DRM_MODE_FLAG_INTERLACE;
				mode->vdisplay = mode->vdisplay << 1;
				mode->vsync_start = mode->vsync_start << 1;
				mode->vsync_end = mode->vsync_end << 1;
			}

			/*for recovery ui*/
			if (hdmitx_set_smaller_pref) {
				/*
				 * select 1080P mode with hightest refresh rate first,
				 * if not find then select 720p mode as pref mode
				 */
				if (!(mode->flags & DRM_MODE_FLAG_INTERLACE) &&
					((mode->hdisplay == 1920 && mode->vdisplay == 1080) ||
					(mode->hdisplay == 1280 && mode->vdisplay == 720))) {
					if (!pref_mode)
						pref_mode = mode;
					else if (pref_mode->hdisplay < mode->hdisplay)
						pref_mode = mode;
					else if (pref_mode->hdisplay == mode->hdisplay &&
							pref_mode->vrefresh < mode->vrefresh)
						pref_mode = mode;
				}
			}

			drm_mode_probed_add(connector, mode);

			DRM_DEBUG("add mode [%s]\n", mode->name);
		}

		if (pref_mode)
			pref_mode->type |= DRM_MODE_TYPE_PREFERRED;

		kfree(vics);
	} else {
		DRM_ERROR("get vic_list from hdmitx dev return 0.\n");
	}

	/*TODO:add dummy mode temp.*/
	mode = drm_mode_duplicate(connector->dev, &dummy_mode);
	if (!mode) {
		DRM_INFO("[%s:%d]dup dummy mode failed.\n", __func__,
			 __LINE__);
	} else {
		drm_mode_probed_add(connector, mode);
		count++;
	}

	return count;
}

/*   drm_display_mode	     :		 hdmi_format_para
 *		hdisp     : h_active
 *		hsync_start(hss)    : h_active + h_front
 *		hsync_end(hse) : h_active + h_front + h_sync
 *		htotal : h_total
 */
enum drm_mode_status meson_hdmitx_check_mode(struct drm_connector *connector,
					   struct drm_display_mode *mode)
{
	return MODE_OK;
}

static struct drm_encoder *meson_hdmitx_best_encoder
	(struct drm_connector *connector)
{
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);

	return &am_hdmi->encoder;
}

static enum drm_connector_status am_hdmitx_connector_detect
	(struct drm_connector *connector, bool force)
{
	struct am_hdmi_tx *am_hdmitx = connector_to_am_hdmi(connector);
	int hpdstat = am_hdmitx->hdmitx_dev->detect();

	DRM_DEBUG("am_hdmi_connector_detect [%d]\n", hpdstat);
	return hpdstat == 1 ?
		connector_status_connected : connector_status_disconnected;
}

static int get_hdr_info(void)
{
	static int hdr_cap_value;
	const struct hdr_info *hdr = am_hdmi_info.hdmitx_dev->get_hdr_info();
	const struct hdr10_plus_info *hdr10p = &hdr->hdr10plus_info;
	unsigned int hdr_priority =  am_hdmi_info.hdmitx_dev->get_hdr_priority();

	/*mask rx hdr capability*/
	if (hdr_priority == 2)
		return 0;

	/*hdr10plugsupported*/
	if (hdr10p->ieeeoui == HDR10_PLUS_IEEE_OUI &&
		hdr10p->application_version != 0xFF)
		hdr_cap_value |= 1 << 0;

	/*
	 *HDR Static Metadata:
	 *Supported EOTF:
	 */

	/*Traditional SDR*/
	hdr_cap_value |= (!!(hdr->hdr_support & 0x1)) << 1;

	/*Traditional HDR*/
	hdr_cap_value |= (!!(hdr->hdr_support & 0x2)) << 2;

	/*SMPTE ST 2084*/
	hdr_cap_value |= (!!(hdr->hdr_support & 0x4)) << 3;

	/*Hybrid Log-Gamma*/
	hdr_cap_value |= (!!(hdr->hdr_support & 0x8)) << 4;

	/*Supported SMD type1*/
	hdr_cap_value |= hdr->static_metadata_type1 << 5;

	return hdr_cap_value;
}

static int get_dv_info(void)
{
	unsigned int hdr_priority =  am_hdmi_info.hdmitx_dev->get_hdr_priority();
	const struct dv_info *dv = am_hdmi_info.hdmitx_dev->get_dv_info();
	int dv_flag = 0;

	if (dv->ieeeoui != DV_IEEE_OUI || hdr_priority) {
		/*The Rx don't support DolbyVision*/
		return 0;
	}

	/*The Rx don't support DolbyVision*/
	if (dv->ieeeoui != DV_IEEE_OUI || dv->block_flag != CORRECT)
		return 0;

	/*DolbyVision RX support list:*/

	if (dv->ver == 0) {
		/*VSVDB Version:  bit0,1*/
		dv_flag |= 0 << 0;
		/*2160p%shz: 1*/
		dv_flag |= (dv->sup_2160p60hz ? 1 : 0) << 2;
		/*Support mode:*/
		/*DV_RGB_444_8BIT*/
		dv_flag |= 1 << 3;
		/*DV_YCbCr_422_12BIT*/
		dv_flag |= (dv->sup_yuv422_12bit ? 1 : 0) << 4;
	}

	if (dv->ver == 1) {
		/*VSVDB Version: %d-byte*/
		dv_flag |= 1 << 0;

		if (dv->length == 0xB) {
			/*2160p%shz: 1*/
			dv_flag |= (dv->sup_2160p60hz ? 1 : 0) << 2;
			/*Support mode:*/
			/*DV_RGB_444_8BIT*/
			dv_flag |= 1 << 3;
			/*DV_YCbCr_422_12BIT*/
			dv_flag |= (dv->sup_yuv422_12bit ? 1 : 0) << 4;
			/*LL_YCbCr_422_12BIT*/
			if (dv->low_latency == 0x01)
				dv_flag |= 1 << 5;
		}

		if (dv->length == 0xE) {
			/*2160p%shz: 1*/
			dv_flag |= (dv->sup_2160p60hz ? 1 : 0) << 2;
			/*Support mode:*/
			/*DV_RGB_444_8BIT*/
			dv_flag |= 1 << 3;
			/*DV_YCbCr_422_12BIT*/
			dv_flag |= (dv->sup_yuv422_12bit ? 1 : 0) << 4;
		}
	}

	if (dv->ver == 2) {
		/*VSVDB Version:*/
		dv_flag |= 2 << 0;

		/*2160p%shz: 1*/
		dv_flag |= (dv->sup_2160p60hz ? 1 : 0) << 2;
		/*Support mode:*/

		if (dv->Interface != 0x00 && dv->Interface != 0x01) {
			/*DV_RGB_444_8BIT*/
			dv_flag |= 1 << 3;
			/*DV_YCbCr_422_12BIT*/
			if (dv->sup_yuv422_12bit)
				dv_flag |= (dv->sup_yuv422_12bit ? 1 : 0) << 4;
		}

		/*LL_YCbCr_422_12BIT*/
		dv_flag |= 1 << 5;

		if (dv->Interface == 0x01 || dv->Interface == 0x03) {
			if (dv->sup_10b_12b_444 == 0x1) {
				/*LL_RGB_444_10BITT*/
				dv_flag |= 1 << 6;
			}
			if (dv->sup_10b_12b_444 == 0x2) {
				/*LL_RGB_444_12BIT*/
				dv_flag |= 1 << 7;
			}
		}
	}

	return dv_flag;
}

static int hdcp_rx_ver(void)
{
	unsigned int hdcp_rx_type = am_hdmi_info.hdmitx_dev->get_rx_hdcp_cap();

	/* Detect RX support HDCP14
	 * Here, must assume RX support HDCP14, otherwise affect 1A-03
	 */

	if (hdcp_rx_type == 0x3)
		return 36;
	else
		return 14;

	return 0;
}

/*like hdmitx hdcp_mode node*/
static int get_hdcp_mode(void)
{
	int hdcp_mode_flag = 0;

	if (am_hdmi_info.hdcp_mode) {
		hdcp_mode_flag |= am_hdmi_info.hdcp_mode;
		if (am_hdmi_info.hdcp_state == HDCP_STATE_SUCCESS)
			hdcp_mode_flag |= 1 << 3;
		else if (am_hdmi_info.hdcp_state == HDCP_STATE_FAIL)
			hdcp_mode_flag |= 0 << 3;
	} else {
		return 0;
	}

	return hdcp_mode_flag;
}

static int am_hdmitx_connector_atomic_set_property
	(struct drm_connector *connector,
	struct drm_connector_state *state,
	struct drm_property *property, uint64_t val)
{
	struct am_hdmitx_connector_state *hdmitx_state =
		to_am_hdmitx_connector_state(state);
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);
	struct hdmitx_color_attr *attr = &hdmitx_state->color_attr_para;

	DRM_INFO("%s\n", __func__);
	if (property == am_hdmi->update_attr_prop) {
		hdmitx_state->update = true;
		return 0;
	} else if (property == am_hdmi->color_space_prop) {
		attr->colorformat = val;
		return 0;
	} else if (property == am_hdmi->color_depth_prop) {
		attr->bitdepth = val;
		hdmitx_state->color_force = true;
		return 0;
	} else if (property == am_hdmi->avmute_prop) {
		hdmitx_state->avmute = val;
		return 0;
	}

	return -EINVAL;
}

static int am_hdmitx_connector_atomic_get_property
	(struct drm_connector *connector,
	const struct drm_connector_state *state,
	struct drm_property *property, uint64_t *val)
{
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);
	struct am_hdmitx_connector_state *hdmitx_state =
		to_am_hdmitx_connector_state(state);
	char attr_force[16];

	am_hdmi_info.hdmitx_dev->get_attr(attr_force);

	if (property == am_hdmi->update_attr_prop) {
		*val = 0;
		return 0;
	} else if (property == am_hdmi->color_space_prop) {
		*val = convert_attrstr_to_colorspace(attr_force);
		return 0;
	} else if (property == am_hdmi->color_depth_prop) {
		*val = convert_attrstr_to_colordepth(attr_force);
		return 0;
	} else if (property == am_hdmi->avmute_prop) {
		*val = hdmitx_state->avmute;
		return 0;
	} else if (property == am_hdmi->hdmi_hdr_status_prop) {
		*val = am_hdmi_info.hdmitx_dev->get_hdmi_hdr_status();
		return 0;
	} else if (property == am_hdmi->hdr_cap_property) {
		*val = get_hdr_info();
		return 0;
	} else if (property == am_hdmi->dv_cap_property) {
		*val = get_dv_info();
		return 0;
	} else if (property == am_hdmi->hdcp_ver_prop) {
		*val = hdcp_rx_ver();
		return 0;
	} else if (property == am_hdmi->hdcp_mode_property) {
		*val = get_hdcp_mode();
		return 0;
	}
	return -EINVAL;
}

static void am_hdmitx_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

int meson_hdmitx_atomic_check(struct drm_connector *connector,
	struct drm_atomic_state *state)
{
	struct am_hdmitx_connector_state *new_hdmitx_state, *old_hdmitx_state;
	struct drm_crtc_state *new_crtc_state = NULL;
	struct am_hdmi_tx *am_hdmitx = connector_to_am_hdmi(connector);
	unsigned int hdmitx_content_type =
		am_hdmitx->hdmitx_dev->get_content_types();

	if (!state) {
		DRM_ERROR("state is NULL.\n");
		return -EINVAL;
	}

	old_hdmitx_state = to_am_hdmitx_connector_state
		(drm_atomic_get_old_connector_state(state, connector));
	new_hdmitx_state = to_am_hdmitx_connector_state
		(drm_atomic_get_new_connector_state(state, connector));

	if (!new_hdmitx_state || !old_hdmitx_state) {
		DRM_ERROR("hdmitx_state is NULL.\n");
		return -EINVAL;
	}

	/*check content type.*/
	if (((1 << new_hdmitx_state->base.content_type) &
		hdmitx_content_type) == 0) {
		DRM_ERROR("[%s] check contentype[%d-%u] fail\n",
			__func__,
			new_hdmitx_state->base.content_type,
			hdmitx_content_type);
		return -EINVAL;
	}

	if (new_hdmitx_state->base.crtc)
		new_crtc_state = drm_atomic_get_new_crtc_state(state,
			new_hdmitx_state->base.crtc);
	else
		return 0;

	if (state->allow_modeset && new_crtc_state) {
		if (!am_hdmi_info.hdmitx_on) {
			new_crtc_state->connectors_changed = true;
			DRM_ERROR("hdmitx_on changed, force modeset.\n");
		}
		/*force set mode.*/
		if (new_hdmitx_state->update)
			new_crtc_state->connectors_changed = true;

		if (new_hdmitx_state->color_force)
			new_crtc_state->mode_changed = true;
	}

	return 0;
}

struct drm_connector_state *meson_hdmitx_atomic_duplicate_state
	(struct drm_connector *connector)
{
	struct am_hdmitx_connector_state *new_state;
	struct am_hdmitx_connector_state *cur_state =
		to_am_hdmitx_connector_state(connector->state);

	new_state = kzalloc(sizeof(*new_state), GFP_KERNEL);
	if (!new_state)
		return NULL;

	__drm_atomic_helper_connector_duplicate_state(connector,
		&new_state->base);

	new_state->update = false;
	new_state->color_force = false;
	new_state->color_attr_para.colorformat = COLORSPACE_RESERVED;
	new_state->color_attr_para.bitdepth = COLORDEPTH_RESERVED;
	new_state->pref_hdr_policy = cur_state->pref_hdr_policy;

	return &new_state->base;
}

void meson_hdmitx_atomic_destroy_state(struct drm_connector *connector,
	 struct drm_connector_state *state)
{
	struct am_hdmitx_connector_state *hdmitx_state;

	hdmitx_state = to_am_hdmitx_connector_state(state);
	__drm_atomic_helper_connector_destroy_state(&hdmitx_state->base);
	kfree(hdmitx_state);
}

/*similar to drm_atomic_helper_connector_reset*/
void meson_hdmitx_reset(struct drm_connector *connector)
{
	struct am_hdmitx_connector_state *hdmitx_state;

	hdmitx_state = kzalloc(sizeof(*hdmitx_state), GFP_KERNEL);
	if (!hdmitx_state)
		return;

	if (connector->state)
		__drm_atomic_helper_connector_destroy_state(connector->state);
	kfree(connector->state);

	__drm_atomic_helper_connector_reset(connector, &hdmitx_state->base);

	hdmitx_state->base.hdcp_content_type = am_hdmi_info.hdcp_request_content_type;
	hdmitx_state->base.content_protection = am_hdmi_info.hdcp_request_content_protection;

	hdmitx_state->pref_hdr_policy = MESON_PREF_DV;

	/*drm api need update state, so need delay attch when create state.*/
	if (!connector->max_bpc_property)
		drm_connector_attach_max_bpc_property
				(connector, 8, HDMITX_MAX_BPC);
}

void meson_hdmitx_atomic_print_state(struct drm_printer *p,
	const struct drm_connector_state *state)
{
	struct am_hdmitx_connector_state *hdmitx_state =
		to_am_hdmitx_connector_state(state);

	drm_printf(p, "\tdrm hdmitx state:\n");
	drm_printf(p, "\t\t VRR_CAP:[%u]\n", am_hdmi_info.hdmitx_dev->get_vrr_cap ?
		am_hdmi_info.hdmitx_dev->get_vrr_cap() : 0);
	drm_printf(p, "\t\t android_path:[%d]\n", am_hdmi_info.android_path);
	drm_printf(p, "\t\t hdcp_state:[%d]\n", am_hdmi_info.hdcp_state);
	drm_printf(p, "\t\t color attr:[%d,%d], hdr_policy[%d]\n",
		hdmitx_state->color_attr_para.colorformat,
		hdmitx_state->color_attr_para.bitdepth,
		hdmitx_state->pref_hdr_policy);
}

static bool meson_hdmitx_is_hdcp_running(void)
{
	if (am_hdmi_info.hdcp_state == HDCP_STATE_DISCONNECT ||
		am_hdmi_info.hdcp_state == HDCP_STATE_STOP)
		return false;

	if (am_hdmi_info.hdcp_mode == HDCP_NULL)
		DRM_ERROR("hdcp mode should NOT null for state [%d]\n",
			am_hdmi_info.hdcp_state);

	return true;
}

static void meson_hdmitx_set_hdcp_result(int result)
{
	struct drm_connector *connector = &am_hdmi_info.base.connector;
	struct drm_modeset_lock *mode_lock =
		&connector->dev->mode_config.connection_mutex;
	bool locked_outer = drm_modeset_is_locked(mode_lock);

	if (result == HDCP_AUTH_OK) {
		am_hdmi_info.hdcp_state = HDCP_STATE_SUCCESS;
		if (!locked_outer)
			drm_modeset_lock(mode_lock, NULL);
		drm_hdcp_update_content_protection(connector, DRM_MODE_CONTENT_PROTECTION_ENABLED);
		if (!locked_outer)
			drm_modeset_unlock(mode_lock);
		DRM_DEBUG("hdcp [%d] set result ok.\n", am_hdmi_info.hdcp_mode);
	} else if (result == HDCP_AUTH_FAIL) {
		am_hdmi_info.hdcp_state = HDCP_STATE_FAIL;
		/*no event needed when fail.*/
		DRM_ERROR("hdcp [%d] set result fail.\n", am_hdmi_info.hdcp_mode);
	} else if (result == HDCP_AUTH_UNKNOWN) {
		/*reset property value to DESIRED.*/
		if (connector->state &&
			connector->state->content_protection ==
			DRM_MODE_CONTENT_PROTECTION_ENABLED) {
			if (!locked_outer)
				drm_modeset_lock(mode_lock, NULL);
			drm_hdcp_update_content_protection(connector,
				DRM_MODE_CONTENT_PROTECTION_DESIRED);
			if (!locked_outer)
				drm_modeset_unlock(mode_lock);
		}
	}
}

static void meson_hdmitx_start_hdcp(int hdcp_mode)
{
	if (hdcp_mode == HDCP_NULL)
		return;

	am_hdmi_info.hdcp_mode = hdcp_mode;
	am_hdmi_info.hdcp_state = HDCP_STATE_START;
	am_hdmi_info.hdmitx_dev->hdcp_enable(hdcp_mode);
	DRM_DEBUG("start hdcp [%d-%d]...\n",
		am_hdmi_info.hdcp_request_content_type, am_hdmi_info.hdcp_mode);
}

static void meson_hdmitx_stop_hdcp(void)
{
	if (meson_hdmitx_is_hdcp_running()) {
		am_hdmi_info.hdmitx_dev->hdcp_disable();
		am_hdmi_info.hdcp_state = HDCP_STATE_STOP;
		am_hdmi_info.hdcp_mode = HDCP_NULL;
		meson_hdmitx_set_hdcp_result(HDCP_AUTH_UNKNOWN);
	}
}

static void meson_hdmitx_disconnect_hdcp(void)
{
	if (meson_hdmitx_is_hdcp_running()) {
		am_hdmi_info.hdmitx_dev->hdcp_disconnect();
		am_hdmi_info.hdcp_state = HDCP_STATE_DISCONNECT;
		am_hdmi_info.hdcp_mode = HDCP_NULL;
		meson_hdmitx_set_hdcp_result(HDCP_AUTH_UNKNOWN);
	}
}

static int meson_hdmitx_get_hdcp_request(struct am_hdmi_tx *tx,
	int request_type_mask)
{
	struct meson_hdmitx_dev *tx_dev = tx->hdmitx_dev;
	int type;
	unsigned int hdcp_tx_type = tx_dev->get_tx_hdcp_cap();
	unsigned int hdcp_rx_type = tx_dev->get_rx_hdcp_cap();

	DRM_INFO("%s usr_type: %d, hdcp cap: %d,%d\n",
			__func__, request_type_mask,
			hdcp_tx_type, hdcp_rx_type);

	switch (hdcp_tx_type & 0x3) {
	case 0x3:
		if ((hdcp_rx_type & 0x2) && (request_type_mask & 0x2))
			type = HDCP_MODE22;
		else if ((hdcp_rx_type & 0x1) && (request_type_mask & 0x1))
			type = HDCP_MODE14;
		else
			type = HDCP_NULL;
		break;
	case 0x2:
		if ((hdcp_rx_type & 0x2) && (request_type_mask & 0x2))
			type = HDCP_MODE22;
		else
			type = HDCP_NULL;
		break;
	case 0x1:
		if ((hdcp_rx_type & 0x1) && (request_type_mask & 0x1))
			type = HDCP_MODE14;
		else
			type = HDCP_NULL;
		break;
	default:
		type = HDCP_NULL;
		DRM_INFO("[%s]: TX no hdcp key\n", __func__);
		break;
	}
	return type;
}

void meson_hdmitx_update_hdcp(void)
{
	int hdcp_request_mode = HDCP_NULL;
	int hdcp_request_mask = HDCP_NULL;

	DRM_DEBUG("%s\n", __func__);

	/*Undesired, disable hdcp.*/
	if (am_hdmi_info.hdcp_request_content_protection == DRM_MODE_CONTENT_PROTECTION_UNDESIRED) {
		meson_hdmitx_stop_hdcp();
		return;
	}

	if (am_hdmi_info.hdcp_request_content_type == DRM_MODE_HDCP_CONTENT_TYPE0)
		hdcp_request_mask = HDCP_MODE14 | HDCP_MODE22;
	else
		hdcp_request_mask = HDCP_MODE22;

	hdcp_request_mode = meson_hdmitx_get_hdcp_request(&am_hdmi_info, hdcp_request_mask);

	/*mode is same, try to re-use last state*/
	if (hdcp_request_mode == am_hdmi_info.hdcp_mode) {
		switch (am_hdmi_info.hdcp_state) {
		case HDCP_STATE_START:
			DRM_INFO("waiting hdcp result.\n");
			return;
		case HDCP_STATE_SUCCESS:
			meson_hdmitx_set_hdcp_result(HDCP_AUTH_OK);
			return;
		case HDCP_STATE_FAIL:
		default:
			DRM_ERROR("meet stopped hdcp stat\n");
			break;
		};
	}

	meson_hdmitx_stop_hdcp();

	if (hdcp_request_mode != HDCP_NULL)
		meson_hdmitx_start_hdcp(hdcp_request_mode);
	else
		DRM_ERROR("No valid hdcp mode exit, maybe hdcp havenot init.\n");
}

void meson_hdmitx_update_hdcp_locked(void)
{
	struct drm_connector *connector = &am_hdmi_info.base.connector;
	struct drm_modeset_lock *mode_lock =
		&connector->dev->mode_config.connection_mutex;

	drm_modeset_lock(mode_lock, NULL);
	meson_hdmitx_update_hdcp();
	drm_modeset_unlock(mode_lock);
}

void meson_hdmitx_update(struct drm_connector_state *new_state,
	struct drm_connector_state *old_state)
{
	struct am_hdmitx_connector_state *old_hdmitx_state =
		to_am_hdmitx_connector_state(old_state);
	struct am_hdmitx_connector_state *new_hdmitx_state =
		to_am_hdmitx_connector_state(new_state);

	if (new_state->content_type != old_state->content_type)
		am_hdmi_info.hdmitx_dev->set_content_type(new_state->content_type);

	if (new_hdmitx_state->avmute != old_hdmitx_state->avmute) {
		if (new_hdmitx_state->avmute) {
			am_hdmi_info.hdmitx_dev->avmute(new_hdmitx_state->avmute);
		} else {
			am_hdmi_info.hdmitx_dev->set_phy(0);
			am_hdmi_info.hdmitx_dev->set_phy(1);
			am_hdmi_info.hdmitx_dev->avmute(new_hdmitx_state->avmute);
		}
	}

	if (am_hdmi_info.android_path)
		return;

	/*Linux only implement*/
	if (old_state->hdcp_content_type
			!= new_state->hdcp_content_type ||
		(old_state->content_protection !=
			new_state->content_protection &&
		new_state->content_protection !=
			DRM_MODE_CONTENT_PROTECTION_ENABLED)) {
		/*check hdcp property update*/
		am_hdmi_info.hdcp_request_content_type =
			new_state->hdcp_content_type;
		am_hdmi_info.hdcp_request_content_protection =
			new_state->content_protection;

		if (new_state->crtc && !drm_atomic_crtc_needs_modeset(new_state->crtc->state))
			meson_hdmitx_update_hdcp();
	}
}

static void meson_hdmitx_hdcp_notify(void *data, int type, int result)
{
	if (type == HDCP_KEY_UPDATE && result == HDCP_AUTH_UNKNOWN) {
		DRM_ERROR("HDCP statue changed, need re-run hdcp\n");
		meson_hdmitx_update_hdcp_locked();
		return;
	}

	if (type != am_hdmi_info.hdcp_mode) {
		DRM_DEBUG("notify type is mismatch[%d]-[%d]\n",
			type, am_hdmi_info.hdcp_mode);
		return;
	}

	if (result == HDCP_AUTH_OK) {
		meson_hdmitx_set_hdcp_result(HDCP_AUTH_OK);
	} else if (result == HDCP_AUTH_FAIL) {
		if (type == HDCP_MODE14) {
			meson_hdmitx_set_hdcp_result(HDCP_AUTH_FAIL);
		} else if (type == HDCP_MODE22) {
			if (am_hdmi_info.hdcp_request_content_type == DRM_MODE_HDCP_CONTENT_TYPE0) {
				DRM_ERROR("ContentType0 hdcp 22 -> hdcp14.\n");
				meson_hdmitx_stop_hdcp();
				meson_hdmitx_start_hdcp(HDCP_MODE14);
			} else {
				meson_hdmitx_set_hdcp_result(HDCP_AUTH_FAIL);
			}
		}
	} else {
		DRM_ERROR("HDCP report unknown result [%d-%d]\n", type, result);
	}
}

static const struct drm_connector_helper_funcs am_hdmi_connector_helper_funcs = {
	.get_modes = meson_hdmitx_get_modes,
	.mode_valid = meson_hdmitx_check_mode,
	.atomic_check	= meson_hdmitx_atomic_check,
	.best_encoder = meson_hdmitx_best_encoder,
};

static const struct drm_connector_funcs am_hdmi_connector_funcs = {
	.detect			= am_hdmitx_connector_detect,
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.atomic_set_property	= am_hdmitx_connector_atomic_set_property,
	.atomic_get_property	= am_hdmitx_connector_atomic_get_property,
	.destroy		= am_hdmitx_connector_destroy,
	.reset			= meson_hdmitx_reset,
	.atomic_duplicate_state	= meson_hdmitx_atomic_duplicate_state,
	.atomic_destroy_state	= meson_hdmitx_atomic_destroy_state,
	.atomic_print_state = meson_hdmitx_atomic_print_state,
};

static int meson_hdmitx_update_dv_eotf(struct drm_display_mode *mode,
	u8 *crtc_eotf_type)
{
	const struct dv_info *dvcap = am_hdmi_info.hdmitx_dev->get_dv_info();
	u8 dv_types = 0;
	u8 eotf_type = *crtc_eotf_type;

	if (dvcap->ieeeoui != DV_IEEE_OUI || dvcap->block_flag != CORRECT)
		return -ENODEV;

	DRM_ERROR("Mode %s,%d\n", mode->name, mode->flags);
	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		return -EINVAL;

	if (strstr(mode->name, "2160p60hz") ||
	    strstr(mode->name, "2160p50hz")) {
		if (!dvcap->sup_2160p60hz)
			return -ERANGE;
	}

	/*refer to sink_dv_support()*/
	if (dvcap->ver == 2) {
		if (dvcap->Interface <= 1)
			dv_types = 2; /* LL only */
		else
			dv_types = 3; /* STD & LL */
	} else if (dvcap->low_latency) {
		dv_types = 3; /* STD & LL */
	} else {
		dv_types = 1; /* STD only */
	}

	/*When pref mode not supported, update to other mode.*/
	if (eotf_type == HDMI_EOTF_MESON_DOLBYVISION_LL &&
		!(dv_types & 2)) {
		eotf_type = HDMI_EOTF_MESON_DOLBYVISION;
	}
	if (eotf_type == HDMI_EOTF_MESON_DOLBYVISION &&
		!(dv_types & 1)) {
		eotf_type = HDMI_EOTF_MESON_DOLBYVISION_LL;
	}

	if (*crtc_eotf_type != eotf_type) {
		DRM_INFO("[%s] change eotf [%u]->[%u]\n",
			__func__, *crtc_eotf_type, eotf_type);
		*crtc_eotf_type = eotf_type;
	}

	return 0;
}

/*check hdr10&hlg cap*/
static int meson_hdmitx_update_hdr_eotf(struct drm_display_mode *mode,
	u8 *crtc_eotf_type)
{
	/*refer to sink_hdr_support()*/
	const u8 hdr10_bit = BIT(2);
	const u8 hlg_bit = BIT(3);
	const struct hdr_info *hdrcap = am_hdmi_info.hdmitx_dev->get_hdr_info();

	/*hdr core can support all the hdr types.*/
	if ((hdrcap->hdr_support & hdr10_bit) ||
		(hdrcap->hdr_support & hlg_bit) ||
		(hdrcap->hdr10plus_info.ieeeoui == HDR10_PLUS_IEEE_OUI &&
		hdrcap->hdr10plus_info.application_version == 1)) {
		return 0;
	}

	return -ENODEV;
}

static int meson_hdmitx_decide_eotf_type
	(struct am_meson_crtc_state *meson_crtc_state,
	struct am_hdmitx_connector_state *hdmitx_state)
{
	u8 crtc_eotf_type = HDMI_EOTF_TRADITIONAL_GAMMA_SDR;
	int ret = 0;
	struct drm_display_mode *mode = &meson_crtc_state->base.adjusted_mode;

	/*If the HDMI cable is not plugin before starting the board,pref_hdr_policy
	 *may not be available in am_meson_update_output_state() function
	 */
	hdmitx_state->pref_hdr_policy = am_hdmi_info.hdmitx_dev->get_hdr_priority();

	/* TODO:hdr priority handled by hdmitx, and dont support dynamic set.
	 * Currently checking is to confirm crtc_eotf_type == dv/dv_ll mode,
	 * we need special setting for dv.
	 */
	if (hdmitx_state->pref_hdr_policy == MESON_PREF_DV)
		crtc_eotf_type = HDMI_EOTF_MESON_DOLBYVISION;
	else if (hdmitx_state->pref_hdr_policy == MESON_PREF_HDR)
		crtc_eotf_type = HDMI_EOTF_SMPTE_ST2084;
	else
		crtc_eotf_type = HDMI_EOTF_TRADITIONAL_GAMMA_SDR;

	DRM_DEBUG_KMS("%s: default eotf [%u]\n", __func__, crtc_eotf_type);

	if (crtc_eotf_type == HDMI_EOTF_MESON_DOLBYVISION) {
		/*check if dv core valid*/
		if (meson_crtc_state->crtc_dv_enable)
			ret = 0;
		else
			ret = -ENODEV;

		/*check dv cap & mode */
		if (ret == 0)
			ret = meson_hdmitx_update_dv_eotf(mode,
				&crtc_eotf_type);
		if (ret < 0) {
			crtc_eotf_type = HDMI_EOTF_SMPTE_ST2084;/*try hdr10*/
			DRM_DEBUG_KMS("hdmitx dv eotf check fail [%d]\n", ret);
		}

		DRM_DEBUG_KMS("%s: dv check dv eotf finish => [%u]\n",
			__func__, crtc_eotf_type);
	}

	if (crtc_eotf_type == HDMI_EOTF_SMPTE_ST2084) {
		/*check if hdr core valid*/
		if (meson_crtc_state->crtc_hdr_enable)
			ret = 0;
		else
			ret = -ENODEV;

		if (ret == 0)
			ret = meson_hdmitx_update_hdr_eotf(mode,
				&crtc_eotf_type);
		if (ret < 0) {
			crtc_eotf_type = HDMI_EOTF_TRADITIONAL_GAMMA_SDR;
			DRM_INFO("hdmitx hdr eotf check fail [%d]\n", ret);
		}

		DRM_DEBUG_KMS("%s: HDR10 check eotf => [%u]\n",
			__func__, crtc_eotf_type);
	}

	if (!meson_crtc_state->crtc_eotf_by_property_flag)
		meson_crtc_state->crtc_eotf_type = crtc_eotf_type;
	else
		meson_crtc_state->crtc_eotf_type = meson_crtc_state->eotf_type_by_property;

	DRM_DEBUG_KMS("%s: [%u->%u]\n", __func__,
		hdmitx_state->pref_hdr_policy,
		meson_crtc_state->crtc_eotf_type);

	return 0;
}

static void meson_hdmitx_cal_brr(struct am_hdmi_tx *hdmitx,
				 struct am_meson_crtc_state *crtc_state,
				 struct drm_display_mode *adj_mode)
{
	int i, vic, brr = 60;
	int num_group;
	struct drm_vrr_mode_group *group;
	struct hdmi_format_para *hdmi_para;
	struct drm_vrr_mode_group *groups;

	groups = kcalloc(MAX_VRR_MODE_GROUP, sizeof(*groups), GFP_KERNEL);
	if (!groups) {
		DRM_ERROR("%s alloc fail\n", __func__);
		return;
	}

	num_group = hdmitx->hdmitx_dev->get_vrr_mode_group(groups,
							   MAX_VRR_MODE_GROUP);

	vic = HDMI_UNKNOWN;

	for (i = 0; i < num_group; i++) {
		group = &groups[i];
		if (group->width == adj_mode->hdisplay &&
		    group->height == adj_mode->vdisplay) {
			if (group->vrr_max >= brr) {
				brr = group->vrr_max;
				vic = group->brr_vic;
			}
		}
	}

	hdmi_para = hdmi_get_fmt_paras(vic);
	if (hdmi_para->vic == HDMI_UNKNOWN) {
		DRM_ERROR("%s, Get hdmi para by vic [%d] failed.\n",
			  __func__, vic);
	} else {
		strncpy(crtc_state->brr_mode, hdmi_para->hdmitx_vinfo.name,
			DRM_DISPLAY_MODE_LEN);
		crtc_state->brr_mode[DRM_DISPLAY_MODE_LEN - 1] = '\0';
		crtc_state->valid_brr = 1;
	}

	DRM_INFO("%s, %d, %d, %s\n", __func__, vic, brr, crtc_state->brr_mode);
	crtc_state->brr = brr;
	kfree(groups);
}

/*Calculate parameters before enable crtc&encoder.*/
void meson_hdmitx_encoder_atomic_mode_set(struct drm_encoder *encoder,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state)
{
	struct am_meson_crtc_state *meson_crtc_state =
		to_am_meson_crtc_state(crtc_state);
	struct am_hdmitx_connector_state *hdmitx_state =
		to_am_hdmitx_connector_state(conn_state);
	struct hdmitx_color_attr *attr = &hdmitx_state->color_attr_para;
	struct drm_display_mode *adj_mode = &crtc_state->adjusted_mode;
	struct am_hdmi_tx *hdmitx = encoder_to_am_hdmi(encoder);
	bool update_attr = false;

	DRM_INFO("%s: enter\n", __func__);
	if (crtc_state->vrr_enabled)
		meson_hdmitx_cal_brr(hdmitx, meson_crtc_state, adj_mode);

	if (am_hdmi_info.android_path)
		return;

	DRM_INFO("%s enter:attr[%d-%d]\n", __func__,
		attr->colorformat, attr->bitdepth);
	meson_hdmitx_decide_eotf_type(meson_crtc_state, hdmitx_state);

	/*check force attr info: from uboot set or debugfs;
	 *for uboot: it may not support dv, but kernel support dv, the attr
	 *from uboot is not valid.
	 */
	if (attr_force_debugfs) {
		attr_force_debugfs = false;
		am_hdmi_info.hdmitx_dev->setup_attr(attr_debugfs);
		DRM_INFO("debugfs attr\n");
		return;
	}

	if (hdmitx_state->color_force) {
		char attr_char[16];
		char *name = meson_crtc_state->base.adjusted_mode.name;

		build_hdmitx_attr_str(attr_char, attr->colorformat, attr->bitdepth);
		if (am_hdmi_info.hdmitx_dev->test_attr(name, attr_char)) {
			DRM_INFO("color property setting successfully\n");
		} else {
			hdmitx_state->color_force = false;
			DRM_INFO("color property setting failed\n");
		}
	}

	if (!hdmitx_state->color_force) {
		if (attr->colorformat != COLORSPACE_RESERVED) {
			if (meson_hdmitx_test_color_attr(meson_crtc_state,
				hdmitx_state, attr)) {
				update_attr = false;
			} else {
				update_attr = true;
				DRM_INFO("%s: force attr fail[%d-%d]\n",
					__func__, attr->colorformat, attr->bitdepth);
			}
		} else {
			update_attr = true;
		}
		if (update_attr) {
			meson_hdmitx_decide_color_attr(meson_crtc_state,
				hdmitx_state->base.max_bpc, attr);
			hdmitx_state->update = true;
		}
	}

	meson_hdmitx_setup_color_attr(attr);
}

static
int meson_encoder_vrr_change(struct drm_encoder *encoder,
			     struct drm_atomic_state *state, int status)
{
	struct drm_connector *connector;
	struct drm_connector_state *conn_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_state, *old_state;
	struct drm_display_mode *new_mode, *old_mode;

	connector = drm_atomic_get_new_connector_for_encoder(state, encoder);
	if (!connector)
		return 0;

	conn_state = drm_atomic_get_new_connector_state(state, connector);
	if (!conn_state)
		return 0;

	crtc = conn_state->crtc;
	new_state = drm_atomic_get_new_crtc_state(state, crtc);
	old_state = drm_atomic_get_old_crtc_state(state, crtc);
	new_mode = &new_state->adjusted_mode;
	old_mode = &old_state->adjusted_mode;

	if (new_state->vrr_enabled &&
		new_mode->hdisplay == old_mode->hdisplay &&
		new_mode->vdisplay == old_mode->vdisplay) {
		DRM_INFO("[%s], vrr, skip encoder %s\n", __func__,
			 status == 1 ? "enable" : "disable");
		return 1;
	}

	return 0;
}

void meson_hdmitx_encoder_atomic_enable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	struct am_meson_crtc_state *meson_crtc_state =
		to_am_meson_crtc_state(encoder->crtc->state);
	struct drm_connector_state *conn_state =
		drm_atomic_get_new_connector_state(state,
		&am_hdmi_info.base.connector);
	struct am_hdmitx_connector_state *meson_conn_state =
		to_am_hdmitx_connector_state(conn_state);
	struct drm_display_mode *mode = &encoder->crtc->state->adjusted_mode;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(encoder->crtc);
	enum vmode_e vmode = meson_crtc_state->vmode;

	if (vmode == VMODE_HDMI) {
		DRM_INFO("[%s]\n", __func__);
	} else {
		if (vmode == (VMODE_HDMI | VMODE_INIT_BIT_MASK))
			am_hdmi_info.hdmitx_on = 1;

		DRM_INFO("[%s] fail! vmode:%d\n", __func__, vmode);
		return;
	}

	if (meson_encoder_vrr_change(encoder, state, 1))
		return;

	if (meson_crtc_state->uboot_mode_init == 1 &&
		meson_conn_state->update != 1)
		vmode |= VMODE_INIT_BIT_MASK;

	meson_vout_notify_mode_change(amcrtc->vout_index,
		vmode, EVENT_MODE_SET_START);
	vout_func_set_vmode(amcrtc->vout_index, vmode);
	meson_vout_notify_mode_change(amcrtc->vout_index,
		vmode, EVENT_MODE_SET_FINISH);
	meson_vout_update_mode_name(amcrtc->vout_index, mode->name, "hdmitx");

	if (!am_hdmi_info.android_path) {
		am_hdmi_info.hdmitx_dev->avmute(0);
		meson_hdmitx_update_hdcp();
	}

	if (mode->vrefresh != meson_crtc_state->brr) {
		set_vframe_rate_hint(mode->vrefresh  * 100);
		DRM_INFO("%s, vrr set rate hint, %d\n", __func__,
			 mode->vrefresh  * 100);
	}

	am_hdmi_info.hdmitx_on = 1;
}

void meson_hdmitx_encoder_atomic_disable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	struct am_meson_crtc_state *meson_crtc_state =
		to_am_meson_crtc_state(encoder->crtc->state);

	if (meson_encoder_vrr_change(encoder, state, 0))
		return;

	DRM_INFO("[%s]\n", __func__);

	if (am_hdmi_info.android_path ||
		meson_crtc_state->uboot_mode_init == 1)
		return;

	am_hdmi_info.hdmitx_on = 0;
	am_hdmi_info.hdmitx_dev->avmute(1);
	meson_hdmitx_stop_hdcp();
	msleep(100);
}

static const struct drm_encoder_helper_funcs meson_hdmitx_encoder_helper_funcs = {
	.atomic_mode_set	= meson_hdmitx_encoder_atomic_mode_set,
	.atomic_enable		= meson_hdmitx_encoder_atomic_enable,
	.atomic_disable		= meson_hdmitx_encoder_atomic_disable,
};

static const struct drm_encoder_funcs meson_hdmitx_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct of_device_id am_meson_hdmi_dt_ids[] = {
	{ .compatible = "amlogic, drm-amhdmitx", },
	{},
};

MODULE_DEVICE_TABLE(of, am_meson_hdmi_dt_ids);

static const struct drm_prop_enum_list hdmi_hdr_status_enum_list[] = {
	{ HDR10PLUS_VSIF, "HDR10Plus-VSIF" },
	{ dolbyvision_std, "DolbyVision-Std" },
	{ dolbyvision_lowlatency, "DolbyVision-Lowlatency" },
	{ HDR10_GAMMA_ST2084, "HDR10-GAMMA_ST2084" },
	{ HDR10_others, "HDR10-others" },
	{ HDR10_GAMMA_HLG, "HDR10-GAMMA_HLG" },
	{ SDR, "SDR" }
};

static void meson_hdmitx_init_hdmi_hdr_status_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	prop = drm_property_create_enum(drm_dev, 0, "hdmi_hdr_status",
					hdmi_hdr_status_enum_list,
					ARRAY_SIZE(hdmi_hdr_status_enum_list));
	if (prop) {
		am_hdmi->hdmi_hdr_status_prop = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to hdmi_hdr_status property\n");
	}
}

static void meson_hdmitx_init_hdcp_ver_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0,
			"hdcp_ver", 0, 36);

	if (prop) {
		am_hdmi->hdcp_ver_prop = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to hdcp_ver property\n");
	}
}

static void meson_hdmitx_init_hdcp_mode_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0,
			"hdcp_mode", 0, 36);

	if (prop) {
		am_hdmi->hdcp_mode_property = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to hdcp_mode property\n");
	}
}

/* Optional colorspace properties. */
static const struct drm_prop_enum_list hdmi_color_space_enum_list[] = {
	{ COLORSPACE_RGB444, "RGB" },
	{ COLORSPACE_YUV422, "422" },
	{ COLORSPACE_YUV444, "444" },
	{ COLORSPACE_YUV420, "420" },
	{ COLORSPACE_RESERVED, "COLORSPACE_RESERVED" }
};

static void meson_hdmitx_init_colorspace_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	prop = drm_property_create_enum(drm_dev, 0, "color_space",
					hdmi_color_space_enum_list,
					ARRAY_SIZE(hdmi_color_space_enum_list));
	if (prop) {
		am_hdmi->color_space_prop = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to color_space property\n");
	}
}

static void meson_hdmitx_init_colordepth_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0,
			"color_depth", 0, 16);

	if (prop) {
		am_hdmi->color_depth_prop = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to color_depth property\n");
	}
}

static void meson_hdmitx_init_hdr_cap_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0,
			"hdr_cap", 0, 1023);

	if (prop) {
		am_hdmi->hdr_cap_property = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to hdr_cap property\n");
	}
}

static void meson_hdmitx_init_dv_cap_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0,
			"dv_cap", 0, 1023);

	if (prop) {
		am_hdmi->dv_cap_property = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to dv_cap property\n");
	}
}

static void meson_hdmitx_init_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	prop = drm_property_create_bool(drm_dev, 0, "UPDATE");
	if (prop) {
		am_hdmi->update_attr_prop = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to UPDATE property\n");
	}
}

static void meson_hdmitx_init_avmute_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	prop = drm_property_create_bool(drm_dev, 0, "MESON_DRM_HDMITX_PROP_AVMUTE");
	if (prop) {
		am_hdmi->avmute_prop = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to AVMUTE property\n");
	}
}

static void meson_hdmitx_hpd_cb(void *data)
{
	struct am_hdmi_tx *am_hdmi = (struct am_hdmi_tx *)data;
#ifdef CONFIG_CEC_NOTIFIER
	struct edid *pedid;
#endif

	DRM_INFO("drm hdmitx hpd notify\n");
	if (am_hdmi->hdmitx_dev->detect() == 0 && !am_hdmi->android_path) {
		meson_hdmitx_disconnect_hdcp();
		am_hdmi_info.hdmitx_on = 0;
	}
#ifdef CONFIG_CEC_NOTIFIER
	if (am_hdmi->hdmitx_dev->detect()) {
		DRM_INFO("%s[%d]\n", __func__, __LINE__);
		pedid = (struct edid *)am_hdmi->hdmitx_dev->get_raw_edid();
		cec_notifier_set_phys_addr_from_edid(am_hdmi->cec_notifier,
						     pedid);
	} else {
		DRM_INFO("%s[%d]\n", __func__, __LINE__);
		cec_notifier_set_phys_addr(am_hdmi->cec_notifier,
					   CEC_PHYS_ADDR_INVALID);
	}
#endif
	drm_helper_hpd_irq_event(am_hdmi->base.connector.dev);
}

int meson_hdmitx_dev_bind(struct drm_device *drm,
	int type, struct meson_connector_dev *intf)
{
	struct meson_drm *priv = drm->dev_private;
	struct am_hdmi_tx *am_hdmi;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct meson_connector *mesonconn;
	struct connector_hpd_cb hpd_cb;
	int ret;
#ifdef CONFIG_CEC_NOTIFIER
	struct edid *pedid;
#endif
	struct connector_hdcp_cb hdcp_cb;
	int hdcp_ctl_lvl;

	DRM_INFO("[%s] in\n", __func__);
	memset(&am_hdmi_info, 0, sizeof(am_hdmi_info));
	am_hdmi_info.hdmitx_dev = to_meson_hdmitx_dev(intf);
	hdcp_ctl_lvl = am_hdmi_info.hdmitx_dev->get_hdcp_ctl_lvl();
	DRM_INFO("hdcp_ctl_lvl=%d\n", hdcp_ctl_lvl);
	if (hdcp_ctl_lvl == 0) {
		am_hdmi_info.android_path = true;
	} else if (am_hdmi_info.hdmitx_dev->hdcp_init) {
		am_hdmi_info.hdmitx_dev->hdcp_init();
		if (hdcp_ctl_lvl == 1) {
			/*TODO: for westeros start hdcp by driver, will move to userspace.*/
			am_hdmi_info.hdcp_request_content_type =
				DRM_MODE_HDCP_CONTENT_TYPE0;
			am_hdmi_info.hdcp_request_content_protection =
				DRM_MODE_CONTENT_PROTECTION_DESIRED;
		} else {
			am_hdmi_info.hdcp_request_content_type =
				DRM_MODE_HDCP_CONTENT_TYPE0;
			am_hdmi_info.hdcp_request_content_protection =
				DRM_MODE_CONTENT_PROTECTION_UNDESIRED;
		}
	} else {
		DRM_ERROR("%s no HDCP func regsitered.\n", __func__);
		am_hdmi_info.android_path = true;
	}

#ifdef CONFIG_CEC_NOTIFIER
	am_hdmi_info.cec_notifier = cec_notifier_get(drm->dev);
#endif

	am_hdmi = &am_hdmi_info;
	mesonconn = &am_hdmi->base;
	mesonconn->drm_priv = priv;
	mesonconn->update = meson_hdmitx_update;
	encoder = &am_hdmi->encoder;
	connector = &am_hdmi->base.connector;

	/* Connector */
	connector->polled = DRM_CONNECTOR_POLL_HPD;
	drm_connector_helper_add(connector,
				 &am_hdmi_connector_helper_funcs);

	ret = drm_connector_init(drm, connector, &am_hdmi_connector_funcs,
				 DRM_MODE_CONNECTOR_HDMIA);
	if (ret) {
		dev_err(priv->dev, "Failed to init hdmi tx connector\n");
		return ret;
	}
	connector->interlace_allowed = 1;

	/* Encoder */
	encoder->possible_crtcs = priv->crtc_masks[ENCODER_HDMI];
	drm_encoder_helper_add(encoder, &meson_hdmitx_encoder_helper_funcs);
	ret = drm_encoder_init(drm, encoder, &meson_hdmitx_encoder_funcs,
			       DRM_MODE_ENCODER_TMDS, "am_hdmi_encoder");
	if (ret) {
		dev_err(priv->dev, "Failed to init hdmi encoder\n");
		return ret;
	}

	drm_connector_attach_encoder(connector, encoder);

	/*hpd irq moved to amhdmitx, registe call back */
	hpd_cb.callback = meson_hdmitx_hpd_cb;
	hpd_cb.data = &am_hdmi_info;
	am_hdmi->hdmitx_dev->register_hpd_cb(&hpd_cb);

	/*hdcp init, default is disable state*/
	if (!am_hdmi_info.android_path) {
		drm_connector_attach_content_protection_property(connector, true);
		hdcp_cb.hdcp_notify = meson_hdmitx_hdcp_notify;
		hdcp_cb.data = &am_hdmi_info;
		am_hdmi_info.hdmitx_dev->register_hdcp_notify(&hdcp_cb);
		am_hdmi_info.hdcp_mode = HDCP_NULL;
		am_hdmi_info.hdcp_state = HDCP_STATE_DISCONNECT;
	}

	drm_connector_attach_content_type_property(connector);
	drm_connector_attach_vrr_capable_property(connector);

	/*amlogic prop*/
	meson_hdmitx_init_property(drm, am_hdmi);
	meson_hdmitx_init_colordepth_property(drm, am_hdmi);
	meson_hdmitx_init_colorspace_property(drm, am_hdmi);
	meson_hdmitx_init_avmute_property(drm, am_hdmi);
	meson_hdmitx_init_hdmi_hdr_status_property(drm, am_hdmi);
	meson_hdmitx_init_hdr_cap_property(drm, am_hdmi);
	meson_hdmitx_init_dv_cap_property(drm, am_hdmi);
	meson_hdmitx_init_hdcp_ver_property(drm, am_hdmi);
	meson_hdmitx_init_hdcp_mode_property(drm, am_hdmi);

	/*TODO:update compat_mode for drm driver, remove later.*/
	priv->compat_mode = am_hdmi_info.android_path;
	/* notifier phy addr to cec when boot
	 * so that to not miss any hpd event
	 */
#ifdef CONFIG_CEC_NOTIFIER
	if (am_hdmi->hdmitx_dev->detect()) {
		DRM_INFO("%s[%d]\n", __func__, __LINE__);
		pedid = (struct edid *)am_hdmi->hdmitx_dev->get_raw_edid();
		cec_notifier_set_phys_addr_from_edid(am_hdmi->cec_notifier,
						     pedid);
	} else {
		cec_notifier_set_phys_addr(am_hdmi->cec_notifier,
					   CEC_PHYS_ADDR_INVALID);
	}
#endif
	DRM_INFO("[%s] out\n", __func__);
	return 0;
}

int meson_hdmitx_dev_unbind(struct drm_device *drm,
	int type, int connector_id)
{
	if (am_hdmi_info.hdmitx_dev->hdcp_exit)
		am_hdmi_info.hdmitx_dev->hdcp_exit();

#ifdef CONFIG_CEC_NOTIFIER
	if (am_hdmi_info.cec_notifier) {
		cec_notifier_set_phys_addr(am_hdmi_info.cec_notifier,
					   CEC_PHYS_ADDR_INVALID);
		cec_notifier_put(am_hdmi_info.cec_notifier);
	}
#endif

	am_hdmi_info.base.connector.funcs->destroy(&am_hdmi_info.base.connector);
	am_hdmi_info.encoder.funcs->destroy(&am_hdmi_info.encoder);
	return 0;
}

