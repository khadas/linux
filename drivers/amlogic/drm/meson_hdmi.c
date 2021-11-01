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
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_common.h>
#include <linux/amlogic/media/vout/hdmi_tx/meson_drm_hdmitx.h>
#include <linux/miscdevice.h>
#include <linux/amlogic/media/vout/hdmi_tx/meson_drm_hdmitx.h>

#include "meson_hdmi.h"
#include "meson_hdcp.h"
#include "meson_vpu.h"
#include "meson_crtc.h"

#define HDMITX_ATTR_LEN_MAX	16
#define HDMITX_MAX_BPC	12

static struct am_hdmi_tx am_hdmi_info;

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

/* this is prior selected list of
 * 4k2k50hz, 4k2k60hz smpte50hz, smpte60hz
 */
struct hdmitx_color_attr color_attr_list1[] = {
	{COLORSPACE_YUV420, 10}, //"420,10bit"
	{COLORSPACE_YUV422, 12}, //"422,12bit"
	{COLORSPACE_YUV420, 8}, //"420,8bit"
	{COLORSPACE_YUV444, 8}, //"444,8bit"
	{COLORSPACE_RGB444, 8}, //"rgb,8bit"
};

/* this is prior selected list of other display mode */
struct hdmitx_color_attr color_attr_list2[] = {
	{COLORSPACE_YUV444, 10}, //"444,10bit"
	{COLORSPACE_YUV422, 12}, //"422,12bit"
	{COLORSPACE_RGB444, 10}, //"rgb,10bit"
	{COLORSPACE_YUV444, 8}, //"444,8bit"
	{COLORSPACE_RGB444, 8}, //"rgb,8bit"
};

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

#define MODE_4K2K24HZ                   "2160p24hz"
#define MODE_4K2K25HZ                   "2160p25hz"
#define MODE_4K2K30HZ                   "2160p30hz"
#define MODE_4K2K50HZ                   "2160p50hz"
#define MODE_4K2K60HZ                   "2160p60hz"
#define MODE_4K2KSMPTE                  "smpte24hz"
#define MODE_4K2KSMPTE30HZ              "smpte30hz"
#define MODE_4K2KSMPTE50HZ              "smpte50hz"
#define MODE_4K2KSMPTE60HZ              "smpte60hz"

static void choose_mode_attr(char *outputmode, u32 max_bpc, struct hdmitx_color_attr *attr)
{
	int length = 0;
	struct hdmitx_color_attr *attr_list = NULL;
	char attr_str[HDMITX_ATTR_LEN_MAX];
	int i;

	DRM_INFO("choose attr for [%s,%d]\n", outputmode, max_bpc);
	if (!outputmode)
		return;

	/* filter some color value options, aimed at some modes. */
	if (!strcmp(outputmode, MODE_4K2K60HZ) ||
	    !strcmp(outputmode, MODE_4K2K50HZ) ||
	    !strcmp(outputmode, MODE_4K2KSMPTE60HZ) ||
	    !strcmp(outputmode, MODE_4K2KSMPTE50HZ)) {
		attr_list = color_attr_list1;
		length = ARRAY_SIZE(color_attr_list1);
	} else {
		attr_list = color_attr_list2;
		length = ARRAY_SIZE(color_attr_list2);
	}

	for (i = 0; i < length; i++) {
		if (attr_list[i].bitdepth > max_bpc)
			continue;

		build_hdmitx_attr_str(attr_str, attr_list[i].colorformat, attr_list[i].bitdepth);
		if (drm_hdmitx_chk_mode_attr_sup(outputmode, attr_str)) {
			attr->colorformat = attr_list[i].colorformat;
			attr->bitdepth = attr_list[i].bitdepth;
			DRM_INFO("get dest attr [%d], [%d]+[%d]\n",
				i, attr->colorformat, attr->bitdepth);
			break;
		}
	}
	if (i == length) {
		attr->colorformat = COLORSPACE_RGB444;
		attr->bitdepth = 8;
	}
}

void meson_hdmitx_setup_color_attr(struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state)
{
	bool reset_attr = true;
	char attr_str[HDMITX_ATTR_LEN_MAX];
	char *mode_str;

	/* android should not use this interface */
	if (am_hdmi_info.android_path)
		return;

	mode_str = crtc_state->mode.name;

	/*TODO: get attr from hdmitx drv, for legacy usage, remove later.*/
	if (true) {
		drm_hdmitx_get_attr(attr_str);
		DRM_INFO("new mode name :%s, current attr: %s\n",
		mode_str, attr_str);
		if (drm_hdmitx_chk_mode_attr_sup(mode_str, attr_str))
			reset_attr = false;
		else
			DRM_ERROR("mode-attr[%s-%s] NOT supported\n",
				mode_str, attr_str);
	}

	if (reset_attr) {
		choose_mode_attr(mode_str, conn_state->max_bpc, &am_hdmi_info.color_attr);
		build_hdmitx_attr_str(attr_str,
			am_hdmi_info.color_attr.colorformat, am_hdmi_info.color_attr.bitdepth);
		drm_hdmitx_setup_attr(attr_str);
		DRM_INFO("select default colorattr:[%s]\n", attr_str);
	}
}

char *am_meson_hdmi_get_voutmode(struct drm_display_mode *mode)
{
	return mode->name;
}

int meson_hdmitx_get_modes(struct drm_connector *connector)
{
	struct edid *edid;
	int *vics;
	int count = 0, i = 0, len = 0;
	struct drm_display_mode *mode, *pref_mode = NULL;
	struct hdmi_format_para *hdmi_para;
	struct hdmi_cea_timing *timing;
	char *strp = NULL;
	u32 num, den;

	edid = (struct edid *)drm_hdmitx_get_raw_edid();
	drm_connector_update_edid_property(connector, edid);

	/*add modes from hdmitx instead of edid*/
	count = drm_hdmitx_get_vic_list(&vics);
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
			mode->hsync_start = timing->h_active + timing->h_front;
			mode->hsync_end = timing->h_active + timing->h_front + timing->h_sync;
			mode->htotal = timing->h_total;
			mode->hskew = 0;
			mode->flags |= timing->hsync_polarity ?
				DRM_MODE_FLAG_PHSYNC : DRM_MODE_FLAG_NHSYNC;

			mode->vdisplay = timing->v_active;
			if (hdmi_para->hdmitx_vinfo.field_height !=
				hdmi_para->hdmitx_vinfo.height) {
				/* follow general rule       to use full vidsplay while
				 * amlogic vout use half value.
				 */
				mode->vdisplay = mode->vdisplay << 1;
				mode->flags |= DRM_MODE_FLAG_INTERLACE;
			}

			mode->vsync_start = mode->vdisplay + timing->v_front;
			mode->vsync_end = mode->vdisplay + timing->v_front + timing->v_sync;
			mode->vtotal = timing->v_total;
			mode->vscan = 0;
			mode->flags |= timing->vsync_polarity ?
				DRM_MODE_FLAG_PVSYNC : DRM_MODE_FLAG_NVSYNC;

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
		DRM_ERROR("drm_hdmitx_get_vic_list return 0.\n");
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
	int hpdstat = drm_hdmitx_detect_hpd();

	DRM_DEBUG("am_hdmi_connector_detect [%d]\n", hpdstat);
	return hpdstat == 1 ?
		connector_status_connected : connector_status_disconnected;
}

static int am_hdmitx_connector_atomic_set_property
	(struct drm_connector *connector,
	struct drm_connector_state *state,
	struct drm_property *property, uint64_t val)
{
	struct am_hdmitx_connector_state *hdmitx_state =
		to_am_hdmitx_connector_state(state);
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);

	if (property == am_hdmi->update_attr_prop) {
		hdmitx_state->update = true;
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

	if (property == am_hdmi->update_attr_prop) {
		*val = 0;
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
	unsigned int hdmitx_content_type = drm_hdmitx_get_contenttypes();
	old_hdmitx_state = to_am_hdmitx_connector_state
		(drm_atomic_get_old_connector_state(state, connector));
	new_hdmitx_state = to_am_hdmitx_connector_state
		(drm_atomic_get_new_connector_state(state, connector));

	if (new_hdmitx_state->base.crtc)
		new_crtc_state = drm_atomic_get_new_crtc_state(state,
			new_hdmitx_state->base.crtc);

	/*check content type.*/
	if (((1 << new_hdmitx_state->base.content_type) &
		hdmitx_content_type) == 0) {
		DRM_ERROR("[%s] check contentype[%d-%u] fail\n",
			__func__,
			new_hdmitx_state->base.content_type,
			hdmitx_content_type);
		return -EINVAL;
	}

	/*force set mode.*/
	if (new_crtc_state && new_hdmitx_state->update)
		new_crtc_state->connectors_changed = true;

	return 0;
}

struct drm_connector_state *am_hdmitx_atomic_duplicate_state
	(struct drm_connector *connector)
{
	struct am_hdmitx_connector_state *hdmitx_state;

	hdmitx_state = kzalloc(sizeof(*hdmitx_state), GFP_KERNEL);
	if (!hdmitx_state)
		return NULL;

	__drm_atomic_helper_connector_duplicate_state(connector, &hdmitx_state->base);
	hdmitx_state->update = false;

	return &hdmitx_state->base;
}

void am_hdmitx_atomic_destroy_state(struct drm_connector *connector,
	 struct drm_connector_state *state)
{
	struct am_hdmitx_connector_state *hdmitx_state;

	hdmitx_state = to_am_hdmitx_connector_state(state);
	__drm_atomic_helper_connector_destroy_state(&hdmitx_state->base);
	kfree(hdmitx_state);
}

/*similar to drm_atomic_helper_connector_reset*/
void am_hdmitx_reset(struct drm_connector *connector)
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

	/*drm api need update state, so need delay attch when create state.*/
	if (!connector->max_bpc_property)
		drm_connector_attach_max_bpc_property
				(connector, 8, HDMITX_MAX_BPC);
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
	meson_hdcp_enable(hdcp_mode);
	DRM_DEBUG("start hdcp [%d-%d]...\n",
		am_hdmi_info.hdcp_request_content_type, am_hdmi_info.hdcp_mode);
}

static void meson_hdmitx_stop_hdcp(void)
{
	if (meson_hdmitx_is_hdcp_running()) {
		meson_hdcp_disable();
		am_hdmi_info.hdcp_state = HDCP_STATE_STOP;
		am_hdmi_info.hdcp_mode = HDCP_NULL;
		meson_hdmitx_set_hdcp_result(HDCP_AUTH_UNKNOWN);
	}
}

static void meson_hdmitx_disconnect_hdcp(void)
{
	if (meson_hdmitx_is_hdcp_running()) {
		meson_hdcp_disconnect();
		am_hdmi_info.hdcp_state = HDCP_STATE_DISCONNECT;
		am_hdmi_info.hdcp_mode = HDCP_NULL;
		meson_hdmitx_set_hdcp_result(HDCP_AUTH_UNKNOWN);
	}
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

	hdcp_request_mode = meson_hdcp_get_valid_type(hdcp_request_mask);

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
	if (new_state->content_type != old_state->content_type)
		drm_hdmitx_set_contenttype(new_state->content_type);
	if (!am_hdmi_info.android_path) {
		/*nothing changed, exit.*/
		if (old_state->hdcp_content_type == new_state->hdcp_content_type) {
			if (old_state->content_protection == new_state->content_protection)
				return;
			/*omit desired->enable which switch by driver.*/
			if (new_state->content_protection == DRM_MODE_CONTENT_PROTECTION_ENABLED)
				return;
		}

		/*check hdcp property update*/
		am_hdmi_info.hdcp_request_content_type = new_state->hdcp_content_type;
		am_hdmi_info.hdcp_request_content_protection = new_state->content_protection;

		if (!drm_atomic_crtc_needs_modeset(new_state->crtc->state))
			meson_hdmitx_update_hdcp();
	}

}

static void meson_hdmitx_hdcp_notify(int type, int result)
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
	.reset			= am_hdmitx_reset,
	.atomic_duplicate_state	= am_hdmitx_atomic_duplicate_state,
	.atomic_destroy_state	= am_hdmitx_atomic_destroy_state,
};

void meson_hdmitx_encoder_atomic_mode_set(struct drm_encoder *encoder,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state)
{
}

void meson_hdmitx_encoder_atomic_enable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	enum vmode_e vmode = get_current_vmode();
	struct am_meson_crtc_state *meson_crtc_state = to_am_meson_crtc_state(encoder->crtc->state);
	struct drm_connector_state *conn_state =
		drm_atomic_get_new_connector_state(state, &am_hdmi_info.base.connector);

	if (vmode == VMODE_HDMI) {
		DRM_INFO("[%s]\n", __func__);
	} else {
		DRM_INFO("[%s] fail! vmode:%d\n", __func__, vmode);
		return;
	}

	if (meson_crtc_state->uboot_mode_init == 1)
		vmode |= VMODE_INIT_BIT_MASK;

	/* todo: need set avmute before switch mode
	 * (no need when boot up && hdmi output
	 * enabled under uboot already)
	 */
	if (!am_hdmi_info.android_path) {
		if ((vmode & VMODE_INIT_BIT_MASK) !=
			VMODE_INIT_BIT_MASK) {
			drm_hdmitx_avmute(1);
			msleep(100);
			/*todo:android setup by hdmitx sysfs*/
			meson_hdmitx_setup_color_attr(encoder->crtc->state, conn_state);
		}
		meson_hdmitx_stop_hdcp();
	}
	set_vout_mode_pre_process(vmode);
	set_vout_vmode(vmode);
	set_vout_mode_post_process(vmode);
	/* msleep(1000); */
	/* todo: clear avmute */
	if (!am_hdmi_info.android_path) {
		drm_hdmitx_avmute(0);
		meson_hdmitx_update_hdcp();
	}
}

void meson_hdmitx_encoder_atomic_disable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	DRM_INFO("[%s]\n", __func__);
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

static void meson_hdmitx_hpd_cb(void *data)
{
	struct am_hdmi_tx *am_hdmi = (struct am_hdmi_tx *)data;
#ifdef CONFIG_CEC_NOTIFIER
	struct edid *pedid;
#endif

	DRM_INFO("drm hdmitx hpd notify\n");
	if (drm_hdmitx_detect_hpd() == 0 && !am_hdmi->android_path)
		meson_hdmitx_disconnect_hdcp();
#ifdef CONFIG_CEC_NOTIFIER
	if (drm_hdmitx_detect_hpd()) {
		DRM_INFO("%s[%d]\n", __func__, __LINE__);
		pedid = (struct edid *)drm_hdmitx_get_raw_edid();
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

static int am_meson_hdmi_bind(struct device *dev,
			      struct device *master, void *data)
{
	struct drm_device *drm = data;
	struct meson_drm *priv = drm->dev_private;
	struct am_hdmi_tx *am_hdmi;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct meson_connector *mesonconn;
	struct drm_hdmitx_hpd_cb hpd_cb;
	int ret;

	DRM_INFO("[%s] in\n", __func__);
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
	drm_encoder_helper_add(encoder, &meson_hdmitx_encoder_helper_funcs);
	ret = drm_encoder_init(drm, encoder, &meson_hdmitx_encoder_funcs,
			       DRM_MODE_ENCODER_TMDS, "am_hdmi_encoder");
	if (ret) {
		dev_err(priv->dev, "Failed to init hdmi encoder\n");
		return ret;
	}
	encoder->possible_crtcs = BIT(0);
	drm_connector_attach_encoder(connector, encoder);

	/*hpd irq moved to amhdmitx, registe call back */
	hpd_cb.callback = meson_hdmitx_hpd_cb;
	hpd_cb.data = &am_hdmi_info;
	drm_hdmitx_register_hpd_cb(&hpd_cb);

	/*hdcp init, default is disable state*/
	if (!am_hdmi_info.android_path) {
		drm_connector_attach_content_protection_property(connector, true);
		meson_hdcp_reg_result_notify(meson_hdmitx_hdcp_notify);
		am_hdmi_info.hdcp_mode = HDCP_NULL;
		am_hdmi_info.hdcp_state = HDCP_STATE_DISCONNECT;
	}

	drm_connector_attach_content_type_property(connector);

	/*amlogic prop*/
	meson_hdmitx_init_property(drm, am_hdmi);

	DRM_INFO("[%s] out\n", __func__);
	return 0;
}

static void am_meson_hdmi_unbind(struct device *dev,
				 struct device *master, void *data)
{
	am_hdmi_info.base.connector.funcs->destroy(&am_hdmi_info.base.connector);
	am_hdmi_info.encoder.funcs->destroy(&am_hdmi_info.encoder);
}

static const struct component_ops am_meson_hdmi_ops = {
	.bind	= am_meson_hdmi_bind,
	.unbind	= am_meson_hdmi_unbind,
};

static int am_meson_hdmi_probe(struct platform_device *pdev)
{
	struct hdmitx_dev *hdmitx_dev = get_hdmitx_device();

	DRM_INFO("[%s] in\n", __func__);
	memset(&am_hdmi_info, 0, sizeof(am_hdmi_info));

	if (hdmitx_dev->hdcp_ctl_lvl == 0) {
		am_hdmi_info.android_path = true;
	} else {
		meson_hdcp_init();
		if (hdmitx_dev->hdcp_ctl_lvl == 1) {
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
	}
#ifdef CONFIG_CEC_NOTIFIER
	am_hdmi_info.cec_notifier = cec_notifier_get(&pdev->dev);
#endif
	return component_add(&pdev->dev, &am_meson_hdmi_ops);
}

static int am_meson_hdmi_remove(struct platform_device *pdev)
{
	meson_hdcp_exit();
#ifdef CONFIG_CEC_NOTIFIER
	if (am_hdmi_info.cec_notifier) {
		cec_notifier_set_phys_addr(am_hdmi_info.cec_notifier,
					   CEC_PHYS_ADDR_INVALID);
		cec_notifier_put(am_hdmi_info.cec_notifier);
	}
#endif
	component_del(&pdev->dev, &am_meson_hdmi_ops);
	return 0;
}

static struct platform_driver am_meson_hdmi_pltfm_driver = {
	.probe  = am_meson_hdmi_probe,
	.remove = am_meson_hdmi_remove,
	.driver = {
		.name = "meson-amhdmitx",
		.of_match_table = am_meson_hdmi_dt_ids,
	},
};

int __init am_meson_hdmi_init(void)
{
	return platform_driver_register(&am_meson_hdmi_pltfm_driver);
}

void __exit am_meson_hdmi_exit(void)
{
	platform_driver_unregister(&am_meson_hdmi_pltfm_driver);
}

#ifndef MODULE
module_init(am_meson_hdmi_init);
module_exit(am_meson_hdmi_exit);
#endif

MODULE_AUTHOR("MultiMedia Amlogic <multimedia-sh@amlogic.com>");
MODULE_DESCRIPTION("Amlogic Meson Drm HDMI driver");
MODULE_LICENSE("GPL");
