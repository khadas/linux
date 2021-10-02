// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "meson_crtc.h"
#include "meson_vpu_pipeline.h"
#include <linux/amlogic/media/vout/vout_notify.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif

#define OSD_DUMP_PATH		"/tmp/osd_dump/"

int crtc_force_hint;
MODULE_PARM_DESC(crtc_force_hint, "\n force modesetting hint\n");
module_param(crtc_force_hint, int, 0644);

static void meson_crtc_destroy_state(struct drm_crtc *crtc,
	struct drm_crtc_state *state)
{
	struct am_meson_crtc_state *meson_crtc_state;

	meson_crtc_state = to_am_meson_crtc_state(state);
	__drm_atomic_helper_crtc_destroy_state(&meson_crtc_state->base);
	kfree(meson_crtc_state);
}

static struct drm_crtc_state *meson_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct am_meson_crtc_state *new_state, *cur_state;

	cur_state = to_am_meson_crtc_state(crtc->state);

	new_state = kzalloc(sizeof(*new_state), GFP_KERNEL);
	if (!new_state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &new_state->base);

	new_state->crtc_hdr_process_policy =
		cur_state->crtc_hdr_process_policy;
	new_state->crtc_eotf_type = cur_state->crtc_eotf_type;
	new_state->crtc_dv_enable = cur_state->crtc_dv_enable;
	new_state->crtc_hdr_enable = cur_state->crtc_hdr_enable;

	/*reset dynamic info.*/
	new_state->uboot_mode_init = 0;

	return &new_state->base;
}

static void meson_crtc_init_hdr_preference
	(struct am_meson_crtc_state *crtc_state)
{
	crtc_state->crtc_hdr_process_policy = MESON_HDR_POLICY_FOLLOW_SINK;
	crtc_state->crtc_dv_enable = false;
	crtc_state->crtc_hdr_enable = false;
}

static void meson_crtc_reset(struct drm_crtc *crtc)
{
	struct am_meson_crtc_state *meson_crtc_state;

	if (crtc->state) {
		meson_crtc_destroy_state(crtc, crtc->state);
		crtc->state = NULL;
	}

	meson_crtc_state = kzalloc(sizeof(*meson_crtc_state), GFP_KERNEL);
	if (!meson_crtc_state)
		return;

	crtc->state = &meson_crtc_state->base;
	crtc->state->crtc = crtc;

	meson_crtc_init_hdr_preference(meson_crtc_state);
}

static int meson_crtc_atomic_get_property(struct drm_crtc *crtc,
	const struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t *val)
{
	DRM_INFO("unsupported crtc property\n");

	return -EINVAL;
}

static int meson_crtc_atomic_set_property(struct drm_crtc *crtc,
	struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t val)
{
	DRM_ERROR("unsupported crtc property\n");

	return -EINVAL;
}

static void meson_crtc_atomic_print_state(struct drm_printer *p,
		const struct drm_crtc_state *state)
{
	struct am_meson_crtc_state *cstate =
			container_of(state, struct am_meson_crtc_state, base);
	struct am_meson_crtc *meson_crtc = to_am_meson_crtc(cstate->base.crtc);
	struct meson_drm *priv = meson_crtc->priv;
	struct meson_vpu_pipeline_state *mvps;
	struct drm_private_state *obj_state;

	obj_state = priv->pipeline->obj.state;
	if (!obj_state) {
		DRM_ERROR("null pipeline obj state!\n");
		return;
	}

	mvps = container_of(obj_state, struct meson_vpu_pipeline_state, obj);
	if (!mvps) {
		DRM_INFO("%s mvps is NULL!\n", __func__);
		return;
	}

	drm_printf(p, "\t\tuboot_mode_init=%u\n", cstate->uboot_mode_init);
	drm_printf(p, "\t\tcrtc_hdr_policy:[%u,%u]\n",
		cstate->crtc_hdr_process_policy,
		cstate->crtc_eotf_type);

	drm_printf(p, "\t\tdv-hdr core state:[%d,%d]\n",
		cstate->crtc_dv_enable,
		cstate->crtc_hdr_enable);

	drm_printf(p, "\tmeson vpu pipeline state:\n");
	drm_printf(p, "\t\tenable_blocks=%llu\n", mvps->enable_blocks);
	drm_printf(p, "\t\tnum_plane=%u\n", mvps->num_plane);
	drm_printf(p, "\t\tnum_plane_video=%u\n", mvps->num_plane_video);
	drm_printf(p, "\t\tglobal_afbc=%u\n", mvps->global_afbc);
}

static const char * const pipe_crc_sources[] = {"vpp1", "NULL"};

static const char *const *meson_crtc_get_crc_sources(struct drm_crtc *crtc,
						     size_t *count)
{
	*count = ARRAY_SIZE(pipe_crc_sources);
	return pipe_crc_sources;
}

static int meson_crc_parse_source(const char *source, bool *enabled)
{
	int i;
	int count = ARRAY_SIZE(pipe_crc_sources);

	if (!source) {
		*enabled = false;
		return 0;
	}

	for (i = 0; i < count; i++) {
		if (!strcmp(pipe_crc_sources[i], source))
			break;
	}

	if (i >= count) {
		*enabled = false;
		return -EINVAL;
	} else if (!strcmp(pipe_crc_sources[i], "NULL")) {
		*enabled  = false;
	} else {
		*enabled = true;
	}

	return 0;
}

static int meson_crtc_set_crc_source(struct drm_crtc *crtc, const char *source)
{
	bool enabled = false;
	int ret = 0;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);

	ret = meson_crc_parse_source(source, &enabled);
	amcrtc->vpp_crc_enable = enabled;

	return ret;
}

static int meson_crtc_verify_crc_source(struct drm_crtc *crtc,
					const char *source, size_t *values_cnt)
{
	bool enabled;

	if (meson_crc_parse_source(source, &enabled) < 0) {
		DRM_ERROR("unknown source %s\n", source);
		return -EINVAL;
	}

	*values_cnt = 1;

	return 0;
}

static const struct drm_crtc_funcs am_meson_crtc_funcs = {
	.atomic_destroy_state	= meson_crtc_destroy_state,
	.atomic_duplicate_state = meson_crtc_duplicate_state,
	.destroy		= drm_crtc_cleanup,
	.reset			= meson_crtc_reset,
	.atomic_get_property = meson_crtc_atomic_get_property,
	.atomic_set_property = meson_crtc_atomic_set_property,
	.atomic_print_state = meson_crtc_atomic_print_state,
	.get_crc_sources	= meson_crtc_get_crc_sources,
	.set_crc_source		= meson_crtc_set_crc_source,
	.verify_crc_source	= meson_crtc_verify_crc_source,
};

static void am_meson_crtc_atomic_enable(struct drm_crtc *crtc,
					struct drm_crtc_state *old_state)
{
	int ret;
	char *name;
	enum vmode_e mode;
	struct drm_display_mode *adjusted_mode = &crtc->state->adjusted_mode;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);
	struct meson_vpu_pipeline *pipeline = amcrtc->pipeline;
	struct am_meson_crtc_state *meson_crtc_state =
					to_am_meson_crtc_state(crtc->state);

	DRM_INFO("%s:in\n", __func__);
	if (!adjusted_mode) {
		DRM_ERROR("meson_crtc_enable fail, unsupport mode:%s\n",
			  adjusted_mode->name);
		return;
	}
	DRM_INFO("%s: %s, %d\n", __func__, adjusted_mode->name, meson_crtc_state->uboot_mode_init);

	/*update mode*/
	name = am_meson_crtc_get_voutmode(adjusted_mode);
	mode = validate_vmode(name, 0);
	if (mode == VMODE_MAX) {
		DRM_ERROR("no matched vout mode\n");
		return;
	}

	if (mode == VMODE_DUMMY_ENCL ||
		mode == VMODE_DUMMY_ENCI ||
		mode == VMODE_DUMMY_ENCP) {
		ret = set_current_vmode(mode);
		if (ret)
			DRM_ERROR("new mode set error\n");
	} else {
		if (meson_crtc_state->uboot_mode_init)
			mode |= VMODE_INIT_BIT_MASK;

		set_vout_init(mode);
		update_vout_viu();
	}
	set_vout_mode_name(name);

	memcpy(&pipeline->mode, adjusted_mode,
	       sizeof(struct drm_display_mode));
	drm_crtc_vblank_on(crtc);
	enable_irq(amcrtc->irq);

	DRM_INFO("%s-%d:out\n", __func__, meson_crtc_state->uboot_mode_init);
}

static void am_meson_crtc_atomic_disable(struct drm_crtc *crtc,
	struct drm_crtc_state *old_state)
{
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);
	enum vmode_e mode;

	DRM_INFO("%s:in\n", __func__);
	drm_crtc_vblank_off(crtc);
	if (crtc->state->event && !crtc->state->active) {
		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irq(&crtc->dev->event_lock);
		crtc->state->event = NULL;
	}
	disable_irq(amcrtc->irq);
	/* disable output by config null
	 * Todo: replace or delete it if have new method
	 */
	mode = validate_vmode("null", 0);
	if (mode == VMODE_MAX) {
		DRM_ERROR("no matched vout mode\n");
		return;
	}
	set_vout_init(mode);
	DRM_DEBUG("%s:out\n", __func__);
}

static int meson_crtc_atomic_check(struct drm_crtc *crtc,
	struct drm_crtc_state *crtc_state)
{
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);
	struct am_meson_crtc_state *cur_state =
		to_am_meson_crtc_state(crtc->state);
	struct am_meson_crtc_state *new_state =
		to_am_meson_crtc_state(crtc_state);
	int ret = 0;

	/*apply parameters need modeset.*/
	if (crtc_state->state->allow_modeset) {
		/*apply state value not set from property.*/
		DRM_DEBUG_KMS("%s force modeset.\n", __func__);
		if (crtc_force_hint > 0) {
			crtc_state->mode_changed = true;
			crtc_force_hint = 0;
		}
	}

	if (cur_state->crtc_dv_enable != new_state->crtc_dv_enable)
		crtc_state->mode_changed = true;

	/*check plane-update*/
	ret = vpu_pipeline_check(amcrtc->pipeline, crtc_state->state);

	return ret;
}

static void am_meson_crtc_atomic_begin(struct drm_crtc *crtc,
	struct drm_crtc_state *old_crtc_state)
{

}

static void am_meson_crtc_atomic_flush(struct drm_crtc *crtc,
	struct drm_crtc_state *old_state)
{
	struct drm_color_ctm *ctm;
	struct drm_color_lut *lut;
	unsigned long flags;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);
	struct drm_atomic_state *old_atomic_state = old_state->state;
	struct meson_drm *priv = amcrtc->priv;
	struct meson_vpu_pipeline *pipeline = amcrtc->pipeline;
	struct am_meson_crtc_state *meson_crtc_state;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
	int gamma_lut_size = 0;
#endif

	meson_crtc_state = to_am_meson_crtc_state(crtc->state);
	if (crtc->state->color_mgmt_changed) {
		DRM_INFO("%s color_mgmt_changed!\n", __func__);
		if (crtc->state->ctm) {
			DRM_INFO("%s color_mgmt_changed 1!\n", __func__);
			ctm = (struct drm_color_ctm *)
				crtc->state->ctm->data;
			#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
			am_meson_ctm_set(0, ctm);
			#endif
		} else {
			DRM_DEBUG("%s Disable CTM!\n", __func__);
			#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
			am_meson_ctm_disable();
			#endif
		}
	}
	if (crtc->state->gamma_lut != priv->gamma_lut_blob) {
		DRM_DEBUG("%s GAMMA LUT blob changed!\n", __func__);
		drm_property_blob_put(priv->gamma_lut_blob);
		priv->gamma_lut_blob = NULL;
		if (crtc->state->gamma_lut) {
			DRM_INFO("%s Set GAMMA\n", __func__);
			priv->gamma_lut_blob =
				drm_property_blob_get(crtc->state->gamma_lut);
			lut = (struct drm_color_lut *)
				crtc->state->gamma_lut->data;
			#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
			gamma_lut_size = amvecm_drm_get_gamma_size(0);
			amvecm_drm_gamma_set(0, lut, gamma_lut_size);
			#endif
		} else {
			DRM_DEBUG("%s Disable GAMMA!\n", __func__);
			#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
			amvecm_drm_gamma_disable(0);
			#endif
		}
	}
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	meson_vpu_line_check(crtc->index, crtc->mode.vdisplay, crtc->mode.vrefresh);
#endif
	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	if (!meson_crtc_state->uboot_mode_init) {
		vpu_osd_pipeline_update(pipeline, old_atomic_state);
		#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
		meson_vpu_reg_vsync_config();
		#endif
	}

	if (crtc->state->event) {
		amcrtc->event = crtc->state->event;
		crtc->state->event = NULL;
	}
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
}

static const struct drm_crtc_helper_funcs am_crtc_helper_funcs = {
	.atomic_check	= meson_crtc_atomic_check,
	.atomic_begin	= am_meson_crtc_atomic_begin,
	.atomic_flush	= am_meson_crtc_atomic_flush,
	.atomic_enable	= am_meson_crtc_atomic_enable,
	.atomic_disable	= am_meson_crtc_atomic_disable,
};

int am_meson_crtc_create(struct am_meson_crtc *amcrtc)
{
	struct meson_drm *priv = amcrtc->priv;
	struct drm_crtc *crtc = &amcrtc->base;
	struct meson_vpu_pipeline *pipeline = priv->pipeline;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
	int gamma_lut_size = 0;
#endif
	int ret;

	DRM_INFO("%s\n", __func__);
	ret = drm_crtc_init_with_planes(priv->drm, crtc,
					priv->primary_plane, priv->cursor_plane,
					&am_meson_crtc_funcs, "amlogic vpu");
	if (ret) {
		dev_err(amcrtc->drm_dev->dev, "Failed to init CRTC\n");
		return ret;
	}

	drm_crtc_helper_add(crtc, &am_crtc_helper_funcs);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	meson_vpu_reg_handle_register();
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
	amvecm_drm_init(0);
	gamma_lut_size = amvecm_drm_get_gamma_size(0);
	drm_mode_crtc_set_gamma_size(crtc, gamma_lut_size);
	drm_crtc_enable_color_mgmt(crtc, 0, true, gamma_lut_size);
#endif

	amcrtc->pipeline = pipeline;
	pipeline->crtc = crtc;
	strcpy(amcrtc->osddump_path, OSD_DUMP_PATH);
	priv->crtc = crtc;
	priv->crtcs[priv->num_crtcs++] = amcrtc;

	return 0;
}

