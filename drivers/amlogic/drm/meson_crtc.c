// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "meson_crtc.h"
#include "meson_vpu_pipeline.h"

#define OSD_DUMP_PATH		"/tmp/osd_dump/"

int crtc_force_hint;
MODULE_PARM_DESC(crtc_force_hint, "\n force modesetting hint\n");
module_param(crtc_force_hint, int, 0644);

static int meson_crtc_set_mode(struct drm_mode_set *set,
			       struct drm_modeset_acquire_ctx *ctx)
{
	struct am_meson_crtc *amcrtc;
	int ret;

	DRM_DEBUG_DRIVER("%s\n", __func__);
	amcrtc = to_am_meson_crtc(set->crtc);
	ret = drm_atomic_helper_set_config(set, ctx);

	return ret;
}

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
	struct am_meson_crtc_state *meson_crtc_state, *old_crtc_state;

	old_crtc_state = to_am_meson_crtc_state(crtc->state);

	meson_crtc_state = kzalloc(sizeof(*meson_crtc_state), GFP_KERNEL);
	if (!meson_crtc_state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &meson_crtc_state->base);

	meson_crtc_state->uboot_mode_init = 0;
	return &meson_crtc_state->base;
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
}

static int meson_crtc_atomic_get_property(struct drm_crtc *crtc,
					  const struct drm_crtc_state *state,
					struct drm_property *property,
					uint64_t *val)
{
	struct am_meson_crtc *amcrtc;
	struct am_meson_crtc_state *meson_crtc_state;
	int ret = -EINVAL;

	amcrtc = to_am_meson_crtc(crtc);
	meson_crtc_state = to_am_meson_crtc_state(state);
	if (property == amcrtc->prop_hdr_policy) {
		#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
		meson_crtc_state->hdr_policy = get_hdr_policy();
		#endif
		*val = meson_crtc_state->hdr_policy;
		ret = 0;
	} else if (property == amcrtc->prop_dv_policy) {
		#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		meson_crtc_state->dv_policy = get_dolby_vision_policy();
		#endif
		*val = meson_crtc_state->dv_policy;
		ret = 0;
	} else if (property == amcrtc->prop_video_out_fence_ptr) {
		*val = 0;
		ret = 0;
	} else {
		DRM_INFO("unsupported crtc property\n");
	}
	return ret;
}

static int meson_crtc_atomic_set_property(struct drm_crtc *crtc,
					  struct drm_crtc_state *state,
					struct drm_property *property,
					uint64_t val)
{
	struct am_meson_crtc *amcrtc;
	struct am_meson_crtc_state *meson_crtc_state;
	s32 __user *fence_ptr;
	int ret = -EINVAL;

	amcrtc = to_am_meson_crtc(crtc);
	meson_crtc_state = to_am_meson_crtc_state(state);
	if (property == amcrtc->prop_hdr_policy) {
		meson_crtc_state->hdr_policy = val;
		#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
		set_hdr_policy(val);
		#endif
		ret = 0;
	} else if (property == amcrtc->prop_dv_policy) {
		meson_crtc_state->dv_policy = val;
		#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		set_dolby_vision_policy(val);
		#endif
		ret = 0;
	} else if (property == amcrtc->prop_video_out_fence_ptr) {
		fence_ptr = u64_to_user_ptr(val);
		if (!fence_ptr)
			return -EFAULT;
		if (put_user(-1, fence_ptr))
			return -EFAULT;
		ret = 0;
		meson_crtc_state->fence_state.video_out_fence_ptr = fence_ptr;
		DRM_DEBUG("set video out fence ptr done\n");
	} else {
		DRM_INFO("unsupported crtc property\n");
	}
	return ret;
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

	drm_printf(p, "\tmeson crtc state:\n");
	drm_printf(p, "\t\thdr_policy=%u\n", cstate->hdr_policy);
	drm_printf(p, "\t\tdv_policy=%u\n", cstate->dv_policy);
	drm_printf(p, "\t\tuboot_mode_init=%u\n", cstate->uboot_mode_init);

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
	.page_flip		= drm_atomic_helper_page_flip,
	.reset			= meson_crtc_reset,
	.set_config             = meson_crtc_set_mode,
	.atomic_get_property = meson_crtc_atomic_get_property,
	.atomic_set_property = meson_crtc_atomic_set_property,
	.atomic_print_state = meson_crtc_atomic_print_state,
	.get_crc_sources	= meson_crtc_get_crc_sources,
	.set_crc_source		= meson_crtc_set_crc_source,
	.verify_crc_source	= meson_crtc_verify_crc_source,
};

static bool am_meson_crtc_mode_fixup(struct drm_crtc *crtc,
				     const struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	//DRM_INFO("%s !!\n", __func__);

	return true;
}

#ifdef CONFIG_AMLOGIC_VOUT
static void am_meson_crtc_atomic_enable(struct drm_crtc *crtc,
					struct drm_crtc_state *old_state)
{
	int ret;
	char *name;
	enum vmode_e mode;
	struct drm_display_mode *adjusted_mode = &crtc->state->adjusted_mode;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);
	struct am_meson_crtc_state *meson_crtc_state = to_am_meson_crtc_state(crtc->state);
	struct meson_vpu_pipeline *pipeline = amcrtc->pipeline;
	const struct vinfo_s *info = NULL;
	unsigned int fr;

	DRM_INFO("%s:in\n", __func__);
	if (!adjusted_mode) {
		DRM_ERROR("meson_crtc_enable fail, unsupport mode:%s\n",
			  adjusted_mode->name);
		return;
	}
	DRM_INFO("%s: %s, %d\n", __func__, adjusted_mode->name, meson_crtc_state->uboot_mode_init);

	name = am_meson_crtc_get_voutmode(adjusted_mode);
	mode = validate_vmode(name, 0);
	if (mode == VMODE_MAX) {
		DRM_ERROR("no matched vout mode\n");
		return;
	}
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &mode);
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
	info = get_current_vinfo();
	fr = info->sync_duration_num * 100 / info->sync_duration_den;
	DRM_INFO("%s: vinfo: mode:%s(%d), frame_rate:%d.%02dHz\n",
		__func__, info->name, info->mode, (fr / 100), (fr % 100));

	memcpy(&pipeline->mode, adjusted_mode,
	       sizeof(struct drm_display_mode));
	drm_crtc_vblank_on(crtc);
	enable_irq(amcrtc->irq);
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);
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
	/*disable output by config null
	 *Todo: replace or delete it if have new method
	 */
	mode = validate_vmode("null", 0);
	if (mode == VMODE_MAX) {
		DRM_ERROR("no matched vout mode\n");
		return;
	}
	set_vout_init(mode);
	DRM_DEBUG("%s:out\n", __func__);
}
#else
static void am_meson_crtc_atomic_enable(struct drm_crtc *crtc,
					struct drm_crtc_state *old_state)
{
}

static void am_meson_crtc_atomic_disable(struct drm_crtc *crtc,
					 struct drm_crtc_state *old_state)
{
}
#endif

static void am_meson_crtc_commit(struct drm_crtc *crtc)
{
	//DRM_INFO("%s\n", __func__);
}

static int am_meson_atomic_check(struct drm_crtc *crtc,
				 struct drm_crtc_state *crtc_state)
{
	struct am_meson_crtc *amcrtc;
	struct meson_vpu_pipeline *pipeline;
	struct drm_atomic_state *state = crtc_state->state;

	amcrtc = to_am_meson_crtc(crtc);
	pipeline = amcrtc->pipeline;

	/*apply parameters need modeset.*/
	if (crtc_state->state &&
		crtc_state->state->allow_modeset) {
		/*apply state value not set from property.*/
		DRM_DEBUG_KMS("%s force modeset.\n", __func__);
		if (crtc_force_hint > 0) {
			crtc_state->mode_changed = true;
			crtc_force_hint = 0;
		}
	}

	return vpu_pipeline_check(pipeline, state);
}

static int am_meson_video_fence_setup(struct video_out_fence_state *fence_state,
				      struct dma_fence *fence)
{
	fence_state->fd = get_unused_fd_flags(O_CLOEXEC);
	if (fence_state->fd < 0)
		return fence_state->fd;

	if (put_user(fence_state->fd, fence_state->video_out_fence_ptr))
		return -EFAULT;

	fence_state->sync_file = sync_file_create(fence);
	if (!fence_state->sync_file)
		return -ENOMEM;

	return 0;
}

static const struct dma_fence_ops am_meson_crtc_fence_ops;

static struct drm_crtc *am_meson_fence_to_crtc(struct dma_fence *fence)
{
	WARN_ON(fence->ops != &am_meson_crtc_fence_ops);
	return container_of(fence->lock, struct drm_crtc, fence_lock);
}

static const char *am_meson_crtc_fence_get_driver_name(struct dma_fence *fence)
{
	struct drm_crtc *crtc = am_meson_fence_to_crtc(fence);

	return crtc->dev->driver->name;
}

static const char *
am_meson_crtc_fence_get_timeline_name(struct dma_fence *fence)
{
	struct drm_crtc *crtc = am_meson_fence_to_crtc(fence);

	return crtc->timeline_name;
}

static const struct dma_fence_ops am_meson_crtc_fence_ops = {
	.get_driver_name = am_meson_crtc_fence_get_driver_name,
	.get_timeline_name = am_meson_crtc_fence_get_timeline_name,
};

static struct dma_fence *am_meson_crtc_create_fence(struct drm_crtc *crtc)
{
	struct dma_fence *fence;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return NULL;

	dma_fence_init(fence, &am_meson_crtc_fence_ops, &crtc->fence_lock,
		       crtc->fence_context, ++crtc->fence_seqno);

	return fence;
}

static int am_meson_video_fence_create(struct drm_crtc *crtc)
{
	struct am_meson_crtc *amcrtc;
	struct am_meson_crtc_state *meson_crtc_state;
	struct dma_fence *fence;
	struct video_out_fence_state *fence_state;
	int ret, i;

	amcrtc = to_am_meson_crtc(crtc);
	meson_crtc_state = to_am_meson_crtc_state(crtc->state);
	fence_state = &meson_crtc_state->fence_state;
	fence = am_meson_crtc_create_fence(crtc);
	if (!fence)
		return -ENOMEM;
	/*setup out fence*/
	ret = am_meson_video_fence_setup(fence_state, fence);
	if (ret) {
		dma_fence_put(fence);
		return ret;
	}
	fd_install(fence_state->fd, fence_state->sync_file->file);
	for (i = 0; i < VIDEO_LATENCY_VSYNC; i++) {
		if (amcrtc->video_fence[i].fence)
			continue;
		amcrtc->video_fence[i].fence = fence;
		atomic_set(&amcrtc->video_fence[i].refcount,
			   VIDEO_LATENCY_VSYNC);
		DRM_DEBUG("video fence create done index:%d\n", i);
		break;
	}
	return 0;
}

static void am_meson_crtc_atomic_begin(struct drm_crtc *crtc,
				       struct drm_crtc_state *old_crtc_state)
{
	struct am_meson_crtc_state *meson_crtc_state;
	int ret = 0;

	meson_crtc_state = to_am_meson_crtc_state(crtc->state);
	if (meson_crtc_state->fence_state.video_out_fence_ptr) {
		ret = am_meson_video_fence_create(crtc);
		meson_crtc_state->fence_state.video_out_fence_ptr = NULL;
	}
	if (ret)
		DRM_INFO("video fence create fail\n");
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
	.commit		= am_meson_crtc_commit,
	.mode_fixup	= am_meson_crtc_mode_fixup,
	.atomic_check	= am_meson_atomic_check,
	.atomic_begin	= am_meson_crtc_atomic_begin,
	.atomic_flush	= am_meson_crtc_atomic_flush,
	.atomic_enable	= am_meson_crtc_atomic_enable,
	.atomic_disable	= am_meson_crtc_atomic_disable,
};

/* Optional hdr_policy properties. */
static const struct drm_prop_enum_list drm_hdr_policy_enum_list[] = {
	{ DRM_MODE_HDR_FOLLOW_SINK, "HDR_follow_sink" },
	{ DRM_MODE_HDR_FOLLOW_SOURCE, "HDR_follow_source" },
};

/* Optional dv_policy properties. */
static const struct drm_prop_enum_list drm_dv_policy_enum_list[] = {
	{ DRM_MODE_DV_FOLLOW_SINK, "DV_follow_sink" },
	{ DRM_MODE_DV_FOLLOW_SOURCE, "DV_follow_source" },
};

int drm_plane_create_hdr_policy_property(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct drm_property *prop;
	struct am_meson_crtc *amcrtc;
	int hdr_policy;

	#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
	hdr_policy = get_hdr_policy();
	#else
	hdr_policy = DRM_MODE_HDR_FOLLOW_SINK
	#endif

	amcrtc = to_am_meson_crtc(crtc);
	prop = drm_property_create_enum(dev, 0, "hdr_policy",
					drm_hdr_policy_enum_list,
					ARRAY_SIZE(drm_hdr_policy_enum_list));
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&crtc->base, prop, hdr_policy);
	amcrtc->prop_hdr_policy = prop;

	return 0;
}

int drm_plane_create_dv_policy_property(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct drm_property *prop;
	struct am_meson_crtc *amcrtc;
	int dv_policy;

	#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	dv_policy = get_dolby_vision_policy();
	#else
	dv_policy = DRM_MODE_DV_FOLLOW_SINK
	#endif

	amcrtc = to_am_meson_crtc(crtc);
	prop = drm_property_create_enum(dev, 0, "dv_policy",
					drm_dv_policy_enum_list,
					ARRAY_SIZE(drm_dv_policy_enum_list));
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&crtc->base, prop, dv_policy);
	amcrtc->prop_dv_policy = prop;

	return 0;
}

int am_meson_crtc_create_video_out_fence_property(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct drm_property *prop;
	struct am_meson_crtc *amcrtc;

	amcrtc = to_am_meson_crtc(crtc);
	prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC,
					 "VIDEO_OUT_FENCE_PTR", 0, U64_MAX);
	if (!prop)
		return -ENOMEM;

	drm_object_attach_property(&crtc->base, prop, 0);
	amcrtc->prop_video_out_fence_ptr = prop;

	return 0;
}

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
		dev_err(amcrtc->dev, "Failed to init CRTC\n");
		return ret;
	}

	drm_plane_create_hdr_policy_property(crtc);
	drm_plane_create_dv_policy_property(crtc);
	am_meson_crtc_create_video_out_fence_property(crtc);
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

