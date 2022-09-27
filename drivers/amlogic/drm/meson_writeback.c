// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/drm_modeset_helper.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_modeset_lock.h>
#include <drm/drm_probe_helper.h>

#include <linux/component.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>

#include "meson_writeback.h"
#include "meson_vpu.h"
#include "meson_crtc.h"

u32 writeback_fmts[TVIN_COLOR_FMT_MAX];

static int meson_writeback_connector_get_modes(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;

	return drm_add_modes_noedid(connector, dev->mode_config.max_width,
				    dev->mode_config.max_height);
}

enum drm_mode_status
meson_writeback_connector_check_mode(struct drm_connector *connector,
					   struct drm_display_mode *mode)
{
	struct drm_device *dev = connector->dev;
	struct drm_mode_config *mode_config = &dev->mode_config;
	int w = mode->hdisplay, h = mode->vdisplay;

	if (w < mode_config->min_width || w > mode_config->max_width)
		return MODE_BAD_HVALUE;

	if (h < mode_config->min_height || h > mode_config->max_height)
		return MODE_BAD_VVALUE;

	return MODE_OK;
}

static int meson_writeback_connector_atomic_check(struct drm_connector *conn,
					  struct drm_atomic_state *state)
{
	struct drm_connector_state *conn_state;
	struct drm_crtc_state *crtc_state;
	struct drm_framebuffer *fb;
	int i;

	DRM_DEBUG("am_drm_writeback: %s %d\n", __func__, __LINE__);

	conn_state = drm_atomic_get_new_connector_state(state, conn);
	if (!conn_state->writeback_job)
		return 0;

	crtc_state = drm_atomic_get_new_crtc_state(state, conn_state->crtc);
	fb = conn_state->writeback_job->fb;

	if (fb->width != crtc_state->mode.hdisplay ||
	    fb->height != crtc_state->mode.vdisplay) {
		DRM_ERROR("Invalid framebuffer size %ux%u\n",
			      fb->width, fb->height);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(writeback_fmts); i++) {
		if (fb->format->format == writeback_fmts[i])
			break;
	}

	if (WARN_ON(i == ARRAY_SIZE(writeback_fmts))) {
		DRM_ERROR("%s pixel format not support!\n", __func__);
		return -EINVAL;
	}

	DRM_DEBUG("am_drm_writeback: %s %d\n", __func__, __LINE__);
	return 0;
}

u32 meson_writeback_format_covert2drm(enum tvin_pixel_format_e pixel_value)
{
	u32 writeback_fmt = DRM_FORMAT_RGB888;

	switch (pixel_value) {
	case TVIN_PIXEL_RGB444:
		writeback_fmt = DRM_FORMAT_RGB888;
		break;
	case TVIN_PIXEL_YUV422:
		writeback_fmt = DRM_FORMAT_YUV422;
		break;
	case TVIN_PIXEL_UYVY444:
		writeback_fmt = DRM_FORMAT_UYVY;
		break;
	case TVIN_PIXEL_NV12:
		writeback_fmt = DRM_FORMAT_NV12;
		break;
	case TVIN_PIXEL_NV21:
		writeback_fmt = DRM_FORMAT_NV21;
		break;
	default:
		break;
	}
	return writeback_fmt;
}

enum tvin_pixel_format_e meson_writeback_format_covert2vdin(u32 writeback_fmt)
{
	enum tvin_pixel_format_e pixel_value = TVIN_PIXEL_RGB444;

	switch (writeback_fmt) {
	case DRM_FORMAT_RGB888:
		pixel_value = TVIN_PIXEL_RGB444;
		break;
	case DRM_FORMAT_YUV422:
		pixel_value = TVIN_PIXEL_YUV422;
		break;
	case DRM_FORMAT_UYVY:
		pixel_value = TVIN_PIXEL_UYVY444;
		break;
	case DRM_FORMAT_NV12:
		pixel_value = TVIN_PIXEL_NV12;
		break;
	case DRM_FORMAT_NV21:
		pixel_value = TVIN_PIXEL_NV21;
		break;
	default:
		break;
	}

	return pixel_value;
}

int meson_writeback_capture_picture(struct drm_framebuffer *fb, u32 port)
{
	struct am_meson_fb *meson_fb;
	struct dma_buf *dmabuf;
	struct vdin_parm_s vdin_pram;
	u32 format;
	int ret = 0;

	meson_fb = container_of(fb, struct am_meson_fb, base);
	if (!meson_fb)
		return -EINVAL;

	vdin_pram.h_active = fb->width;
	vdin_pram.v_active = fb->height;
	vdin_pram.dest_h_active = fb->width;
	vdin_pram.dest_v_active = fb->height;
	vdin_pram.scan_mode = 1;
	vdin_pram.frame_rate = 60;
	format = fb->format->format;
	vdin_pram.port = (enum tvin_port_e)port;
	vdin_pram.fmt = TVIN_SIG_FMT_NULL;
	vdin_pram.cfmt =
	(enum tvin_color_fmt_e)meson_writeback_format_covert2vdin(format);
	vdin_pram.dfmt =
	(enum tvin_color_fmt_e)meson_writeback_format_covert2vdin(format);
	dmabuf = meson_fb->bufp[0]->dmabuf;
	get_dma_buf(dmabuf);

	DRM_DEBUG("%s dmabuf-%px, h&v(%d-%d), cfmt-%d, port-%d\n",
		__func__, dmabuf, vdin_pram.h_active, vdin_pram.v_active,
		vdin_pram.cfmt, vdin_pram.port);

	ret = vdin_capture_picture(&vdin_pram, dmabuf);
	if (ret) {
		DRM_ERROR("writeback capture picture fail!\n");
		return ret;
	}

	return ret;
}

static void meson_writeback_capture_work(struct work_struct *work)
{
	struct am_drm_writeback *drm_writeback = container_of(work,
			struct am_drm_writeback, writeback_work);

	meson_writeback_capture_picture(drm_writeback->fb,
		drm_writeback->capture_port);

	drm_writeback_signal_completion(&drm_writeback->wb_connector, 0);
	DRM_DEBUG("%s %d capture done!\n", __func__, __LINE__);
}

static void meson_writeback_connector_atomic_commit(struct drm_connector *conn,
					struct drm_connector_state *conn_state)
{
	struct drm_framebuffer *fb;
	struct am_drm_writeback *drm_writeback;
	int i;

	DRM_DEBUG("am_drm_writeback: %s %d\n", __func__, __LINE__);

	if (WARN_ON(!conn_state->writeback_job))
		return;

	drm_writeback = connector_to_am_writeback(conn);
	fb = conn_state->writeback_job->fb;
	drm_writeback->fb = fb;

	for (i = 0; i < ARRAY_SIZE(writeback_fmts); i++) {
		if (fb->format->format == writeback_fmts[i])
			break;
	}

	if (WARN_ON(i == ARRAY_SIZE(writeback_fmts))) {
		DRM_ERROR("%s pixel format not support!\n", __func__);
		return;
	}

	drm_writeback_queue_job(&drm_writeback->wb_connector, conn_state);
	queue_work(drm_writeback->writeback_wq, &drm_writeback->writeback_work);
	DRM_DEBUG("am_drm_writeback: %s %d\n", __func__, __LINE__);
}

static const struct drm_connector_helper_funcs
am_writeback_connector_helper_funcs = {
	.get_modes = meson_writeback_connector_get_modes,
	.mode_valid = meson_writeback_connector_check_mode,
	.atomic_check	= meson_writeback_connector_atomic_check,
	.atomic_commit = meson_writeback_connector_atomic_commit,
};

static enum drm_connector_status
am_writeback_connector_detect(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static void am_writeback_connector_destroy(struct drm_connector *connector)
{
	drm_connector_cleanup(connector);
}

static struct drm_connector_state *
am_writeback_connector_duplicate_state(struct drm_connector *connector)
{
	struct am_drm_writeback_state *writeback_state;

	DRM_DEBUG("am_drm_writeback: %s %d\n", __func__, __LINE__);

	writeback_state = kzalloc(sizeof(*writeback_state), GFP_KERNEL);
	if (!writeback_state)
		return NULL;

	__drm_atomic_helper_connector_duplicate_state(connector,
		&writeback_state->base);

	return &writeback_state->base;
}

static void
am_writeback_connector_destroy_state(struct drm_connector *connector,
					   struct drm_connector_state *state)
{
	struct am_drm_writeback_state *writeback_state;

	DRM_DEBUG("am_drm_writeback: %s %d\n", __func__, __LINE__);

	writeback_state = to_am_writeback_state(state);
	__drm_atomic_helper_connector_destroy_state(&writeback_state->base);
	kfree(writeback_state);
}

static int am_writeback_connector_atomic_set_property
	(struct drm_connector *connector,
	struct drm_connector_state *state,
	struct drm_property *property, uint64_t val)
{
	struct am_drm_writeback *drm_writeback;

	drm_writeback = connector_to_am_writeback(connector);

	if (property == drm_writeback->capture_port_prop) {
		drm_writeback->capture_port = val;
		return 0;
	}

	return -EINVAL;
}

static int am_writeback_connector_atomic_get_property
	(struct drm_connector *connector,
	const struct drm_connector_state *state,
	struct drm_property *property, uint64_t *val)
{
	struct am_drm_writeback *drm_writeback;

	drm_writeback = connector_to_am_writeback(connector);

	if (property == drm_writeback->capture_port_prop) {
		*val = drm_writeback->capture_port;
		return 0;
	}

	return -EINVAL;
}

static const struct drm_connector_funcs am_writeback_connector_funcs = {
	.detect			= am_writeback_connector_detect,
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.destroy		= am_writeback_connector_destroy,
	.reset			= drm_atomic_helper_connector_reset,
	.atomic_duplicate_state	= am_writeback_connector_duplicate_state,
	.atomic_destroy_state	= am_writeback_connector_destroy_state,
	.atomic_set_property	= am_writeback_connector_atomic_set_property,
	.atomic_get_property	= am_writeback_connector_atomic_get_property,
};

void meson_writeback_encoder_atomic_mode_set(struct drm_encoder *encoder,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state)
{
	/*TODO*/
}

void meson_writeback_encoder_atomic_enable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	/*TODO*/
}

void meson_writeback_encoder_atomic_disable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	/*TODO*/
	DRM_INFO("[%s]\n", __func__);
}

static const struct drm_encoder_helper_funcs
am_writeback_encoder_helper_funcs = {
	.atomic_mode_set = meson_writeback_encoder_atomic_mode_set,
	.atomic_enable = meson_writeback_encoder_atomic_enable,
	.atomic_disable = meson_writeback_encoder_atomic_disable,
};

/* Optional capture port properties. */
static const struct drm_prop_enum_list writeback_capture_port_enum_list[] = {
	{ TVIN_PORT_VIU1_WB0_VD1, "video" },
	{ TVIN_PORT_VIU1_WB0_OSD1, "osd" },
	{ TVIN_PORT_VIU1_WB0_VPP, "video&osd" },
};

static int meson_writeback_port_property(struct drm_device *drm_dev,
	struct am_drm_writeback *drm_writeback)
{
	struct drm_property *prop;
	struct drm_mode_object mode_obj;
	int capture_port = TVIN_PORT_VIU1_WB0_VPP;

	prop = drm_property_create_enum(drm_dev, 0, "CAPTURE_PORT",
				writeback_capture_port_enum_list,
				ARRAY_SIZE(writeback_capture_port_enum_list));
	if (prop) {
		drm_writeback->capture_port_prop = prop;
		mode_obj = drm_writeback->wb_connector.base.base;
		drm_object_attach_property(&mode_obj, prop, capture_port);
	} else {
		DRM_ERROR("Failed to UPDATE property\n");
		return -ENOMEM;
	}

	return 0;
}

int meson_writeback_get_format(u32 *writeback_fmts)
{
	struct support_pixel_format pixel_format;
	int i, ret;

	ret = vdin_get_support_pixel_format(&pixel_format);
	if (ret)
		return ret;

	for (i = 0; i < TVIN_PIXEL_FORMAT_NUM; i++)
		writeback_fmts[i] =
		meson_writeback_format_covert2drm(pixel_format.pixel_value[i]);

	return 0;
}

int am_meson_writeback_create(struct drm_device *drm)
{
	struct drm_writeback_connector *wb_connector;
	struct am_drm_writeback *drm_writeback;
	struct meson_connector *mesonconn;
	int ret = 0;

	drm_writeback = kzalloc(sizeof(*drm_writeback), GFP_KERNEL);
	if (!drm_writeback)
		return -ENOMEM;

	mesonconn = &drm_writeback->base;
	mesonconn->drm_priv = drm->dev_private;
	mesonconn->update = NULL;
	wb_connector = &drm_writeback->wb_connector;
	wb_connector->encoder.possible_crtcs = BIT(0);
	drm_writeback->drm_dev = *drm;
	drm_writeback->capture_port = TVIN_PORT_VIU1_WB0_VPP;

	/*get pixel format*/
	ret = meson_writeback_get_format(writeback_fmts);
	if (ret) {
		DRM_ERROR("Failed to get pixel format\n");
		return ret;
	}

	/* register Connector */
	drm_connector_helper_add(&wb_connector->base,
			&am_writeback_connector_helper_funcs);
	ret = drm_writeback_connector_init(drm, wb_connector,
			&am_writeback_connector_funcs,
			&am_writeback_encoder_helper_funcs,
			writeback_fmts, ARRAY_SIZE(writeback_fmts));
	if (ret) {
		DRM_ERROR("Failed to init writeback connector\n");
		return ret;
	}

	/*amlogic prop*/
	ret = meson_writeback_port_property(drm, drm_writeback);
	if (ret) {
		DRM_ERROR("Failed to attach capture port prop\n");
		return ret;
	}

	/*create writeback capture workqueue*/
	drm_writeback->writeback_wq = alloc_workqueue("writeback_capture",
				WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);

	INIT_WORK(&drm_writeback->writeback_work, meson_writeback_capture_work);

	DRM_INFO("am_drm_writeback creat ok\n");
	return ret;
}
