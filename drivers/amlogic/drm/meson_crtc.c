// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/sync_file.h>

#include "meson_crtc.h"
#include "meson_vpu_pipeline.h"

#include <linux/amlogic/media/vout/vout_notify.h>
#include <vout/vout_serve/vout_func.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#include <enhancement/amvecm/amcsc.h>

#define EOTF_RESERVED 23

#define OSD_DUMP_PATH		"/tmp/osd_dump/"

/*
 **********TV SUPPROT DV****************
 *HDR			2
 *DV			18
 *SDR			0
 *DV LL		    19
 */
static void set_eotf_by_property(struct am_meson_crtc_state *state)
{
	if (state->crtc_eotf_by_property_flag) {
		if (!dv_support() &&  get_amdv_mode() && is_amdv_enable()) {
			DRM_INFO("[%s] support DV\n", __func__);
			if (state->eotf_type_by_property == 2) {
				set_amdv_ll_policy(0);
				set_amdv_policy(2);
				set_amdv_enable(1);
				set_amdv_mode(2);
			} else if (state->eotf_type_by_property == 18) {
				set_amdv_ll_policy(0);
				set_amdv_policy(2);
				set_amdv_enable(1);
				set_amdv_mode(1);
			} else if (state->eotf_type_by_property == 0) {
				set_amdv_ll_policy(0);
				set_amdv_policy(2);
				set_amdv_enable(1);
				set_amdv_mode(4);
			} else if (state->eotf_type_by_property == 19) {
				set_amdv_ll_policy(1);
				set_amdv_policy(2);
				set_amdv_enable(1);
				set_amdv_mode(1);

			}
		} else {
			DRM_INFO("[%s] can not support DV\n", __func__);
			if (state->eotf_type_by_property == 0) {
				set_amdv_policy(2);
				set_amdv_mode(0);
				set_amdv_enable(0);
				set_hdr_policy(2);
				set_force_output(1);
			}
			if (state->eotf_type_by_property == 2) {
				set_amdv_policy(2);
				set_amdv_mode(0);
				set_amdv_enable(0);
				set_hdr_policy(2);
				set_force_output(3);
			}
		}
	}
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
	struct am_meson_crtc_state *new_state, *cur_state;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);

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
	new_state->crtc_eotf_by_property_flag = cur_state->crtc_eotf_by_property_flag;
	new_state->eotf_type_by_property = cur_state->eotf_type_by_property;
	new_state->dv_mode = cur_state->dv_mode;
	new_state->preset_vmode = VMODE_INVALID;

	/*reset dynamic info.*/
	if (amcrtc->priv->logo_show_done)
		new_state->uboot_mode_init = 0;
	else
		new_state->uboot_mode_init = cur_state->uboot_mode_init;

	return &new_state->base;
}

static void meson_crtc_init_hdr_preference
	(struct am_meson_crtc_state *crtc_state)
{
	crtc_state->crtc_hdr_process_policy = get_hdr_policy();
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	crtc_state->crtc_dv_enable = is_amdv_enable();
	crtc_state->dv_mode = get_amdv_ll_policy() ? 1 : 0;
#else
	crtc_state->crtc_dv_enable = false;
	crtc_state->dv_mode = false;
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
	crtc_state->crtc_hdr_enable = true;
#else
	crtc_state->crtc_hdr_enable = false;
#endif
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

static const char *meson_crtc_fence_get_driver_name(struct dma_fence *fence)
{
	return "meson";
}

static const char *meson_crtc_fence_get_timeline_name(struct dma_fence *fence)
{
	return "present_fence";
}

static const struct dma_fence_ops meson_crtc_fence_ops = {
	.get_driver_name = meson_crtc_fence_get_driver_name,
	.get_timeline_name = meson_crtc_fence_get_timeline_name,
};

struct dma_fence *meson_crtc_create_fence(spinlock_t *lock)
{
	struct dma_fence *fence;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return NULL;

	dma_fence_init(fence, &meson_crtc_fence_ops, lock,
		       0, 0);

	return fence;
}

int meson_crtc_creat_present_fence_ioctl(struct drm_device *dev,
			void *data, struct drm_file *file_priv)
{
	struct drm_meson_present_fence *arg = data;
	struct am_meson_crtc_present_fence *pre_fence;
	struct am_meson_crtc *amcrtc;
	struct drm_crtc *crtc;
	struct dma_fence *fence;
	struct sync_file *sync_file;
	int fd, ret;

	if (arg->crtc_idx >= dev->num_crtcs)
		return -EINVAL;

	crtc = drm_crtc_from_index(dev, arg->crtc_idx);
	if (!crtc)
		return -EINVAL;

	amcrtc = to_am_meson_crtc(crtc);
	pre_fence = &amcrtc->present_fence;

	if (pre_fence->fence) {
		DRM_DEBUG("fence already created, return!");
		return -EEXIST;
	}

	fence = meson_crtc_create_fence(&pre_fence->lock);
	if (!fence)
		return -ENOMEM;

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		ret = fd;
		goto err_put_fence;
	}

	sync_file = sync_file_create(fence);
	//dma_fence_put(fence);
	if (!sync_file) {
		ret = -ENOMEM;
		goto err_put_fd;
	}

	fd_install(fd, sync_file->file);
	arg->fd = fd;
	pre_fence->fd = fd;
	pre_fence->fence = fence;
	pre_fence->sync_file = sync_file;
	DRM_DEBUG("%s fd=%d, fence=%px\n", __func__,
		pre_fence->fd, pre_fence->fence);

	return 0;

err_put_fd:
	put_unused_fd(fd);
err_put_fence:
	dma_fence_put(fence);
	return ret;
}

static int meson_crtc_atomic_get_property(struct drm_crtc *crtc,
	const struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t *val)
{
	struct am_meson_crtc_state *crtc_state =
		to_am_meson_crtc_state(state);
	struct am_meson_crtc *meson_crtc = to_am_meson_crtc(crtc);
	int ret = 0;

	if (!crtc_state->crtc_eotf_by_property_flag)
		crtc_state->eotf_type_by_property = EOTF_RESERVED;

	if (property == meson_crtc->hdr_policy) {
		*val = crtc_state->crtc_hdr_process_policy;
		return 0;
	} else if (property == meson_crtc->hdmi_eotf) {
		*val = crtc_state->eotf_type_by_property;
		return 0;
	} else if (property == meson_crtc->dv_enable_property) {
		*val = crtc_state->crtc_dv_enable;
		return 0;
	}  else if (property == meson_crtc->bgcolor_property) {
		*val = crtc_state->crtc_bgcolor;
		return 0;
	} else if (property == meson_crtc->dv_mode_property) {
		*val = crtc_state->dv_mode;
		return 0;
	}

	return ret;
}

static int meson_crtc_atomic_set_property(struct drm_crtc *crtc,
	struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t val)
{
	struct am_meson_crtc_state *crtc_state =
		to_am_meson_crtc_state(state);
	struct am_meson_crtc *meson_crtc = to_am_meson_crtc(crtc);
	int ret = 0;

	if (property == meson_crtc->hdr_policy) {
		crtc_state->crtc_hdr_process_policy = val;
		return 0;
	} else if (property == meson_crtc->hdmi_eotf) {
		crtc_state->eotf_type_by_property = val;
		crtc_state->crtc_eotf_by_property_flag = true;
		return 0;
	} else if (property == meson_crtc->dv_enable_property) {
		crtc_state->crtc_dv_enable = val;
		return 0;
	} else if (property == meson_crtc->bgcolor_property) {
		crtc_state->crtc_bgcolor = val;
		return 0;
	} else if (property == meson_crtc->dv_mode_property) {
		crtc_state->dv_mode = val;
		return 0;
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

	drm_printf(p, "\t\tvrr_enabled=%u\n", state->vrr_enabled);
	drm_printf(p, "\t\tbrr_mode=%s\n", cstate->brr_mode);
	drm_printf(p, "\t\tbrr=%u\n", cstate->brr);
	drm_printf(p, "\t\tuboot_mode_init=%u\n", cstate->uboot_mode_init);
	drm_printf(p, "\t\tcrtc_hdr_policy:[%u,%u]\n",
		cstate->crtc_hdr_process_policy,
		cstate->crtc_eotf_type);

	drm_printf(p, "\t\tdv-hdr core state:[%d,%d]\n",
		cstate->crtc_dv_enable,
		cstate->crtc_hdr_enable);

	drm_printf(p, "\tmeson vpu pipeline state:\n");
	drm_printf(p, "\t\tenable_blocks=%llu\n",
		mvps->sub_states[meson_crtc->crtc_index].enable_blocks);
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

static int meson_crtc_enable_vblank(struct drm_crtc *crtc)
{
	return 0;
}

static void meson_crtc_disable_vblank(struct drm_crtc *crtc)
{
}

static const struct drm_crtc_funcs am_meson_crtc_funcs = {
	.atomic_destroy_state	= meson_crtc_destroy_state,
	.atomic_duplicate_state = meson_crtc_duplicate_state,
	.destroy		= drm_crtc_cleanup,
	.page_flip		= drm_atomic_helper_page_flip,
	.reset			= meson_crtc_reset,
	.set_config		= drm_atomic_helper_set_config,
	.atomic_get_property = meson_crtc_atomic_get_property,
	.atomic_set_property = meson_crtc_atomic_set_property,
	.atomic_print_state = meson_crtc_atomic_print_state,
	.get_crc_sources	= meson_crtc_get_crc_sources,
	.set_crc_source		= meson_crtc_set_crc_source,
	.verify_crc_source	= meson_crtc_verify_crc_source,
	.enable_vblank = meson_crtc_enable_vblank,
	.disable_vblank = meson_crtc_disable_vblank,
};

static bool am_meson_crtc_mode_fixup(struct drm_crtc *crtc,
				     const struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	/* TODO: drm_calc_timestamping_constants() do framedur_ns /= 2,
	 * reset crtc info same as logical size, so we can get correct
	 * framedur_ns.
	 */
	drm_mode_set_crtcinfo(adj_mode, 0);
	//DRM_INFO("%s !!\n", __func__);

	return true;
}

static void am_meson_crtc_atomic_enable(struct drm_crtc *crtc,
					struct drm_crtc_state *old_state)
{
	int ret;
	char *name, *brr_name;
	enum vmode_e mode;
	struct drm_display_mode *adjusted_mode = &crtc->state->adjusted_mode;
	struct drm_display_mode *old_mode = &old_state->adjusted_mode;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);
	struct meson_vpu_pipeline *pipeline = amcrtc->pipeline;
	struct am_meson_crtc_state *meson_crtc_state =
					to_am_meson_crtc_state(crtc->state);
	struct meson_drm *priv = amcrtc->priv;
	int hdrpolicy = 0;

	if (!adjusted_mode) {
		DRM_ERROR("meson_crtc_enable NULL mode failed.\n");
		return;
	}

	DRM_INFO("%s-[%d] in: new[%s], old[%s], vmode[%d-%d], uboot[%d]\n",
		__func__, amcrtc->crtc_index,
		adjusted_mode->name, old_mode->name,
		meson_crtc_state->vmode, meson_crtc_state->preset_vmode,
		meson_crtc_state->uboot_mode_init);

	if (!priv->compat_mode) {
		/* update follow source/follow sink to hdr/dv core.
		 * drm didnot send hdmitx pkt, we just set policy to hdr core.
		 */
		if (meson_crtc_state->crtc_hdr_process_policy
				== MESON_HDR_POLICY_FOLLOW_SOURCE ||
			meson_crtc_state->crtc_hdr_process_policy
				== MESON_HDR_POLICY_FOLLOW_SINK) {
			#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			/*enable/disable dv*/
			if (meson_crtc_state->crtc_dv_enable) {
				if (meson_crtc_state->crtc_eotf_type
						== HDMI_EOTF_MESON_DOLBYVISION_LL) {
					set_amdv_ll_policy(1);
				} else {
					set_amdv_ll_policy(0);
				}
				set_amdv_enable(true);
			}
			#endif

			hdrpolicy = (meson_crtc_state->crtc_hdr_process_policy
				== MESON_HDR_POLICY_FOLLOW_SINK) ? 0 : 1;
			#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
			set_hdr_policy(hdrpolicy);
			#endif
		}
		/*force eotf by property*/
		set_eotf_by_property(meson_crtc_state);
	}

	/*update mode*/
	name = adjusted_mode->name;
	if (crtc->state->vrr_enabled) {
		DRM_INFO("%s-[%d], adjust raw %s to brr %s\n",
			__func__, amcrtc->crtc_index,
			name, meson_crtc_state->brr_mode);
		brr_name = meson_crtc_state->brr_mode;
		if (meson_crtc_state->valid_brr)
			name = brr_name;
	}

	if (meson_crtc_state->preset_vmode == VMODE_INVALID) {
		mode = vout_func_validate_vmode(amcrtc->vout_index, name, 0);
		if (mode == VMODE_MAX) {
			DRM_ERROR("crtc [%d]: no matched vout mode\n", amcrtc->crtc_index);
			return;
		}
	} else {
		mode = meson_crtc_state->preset_vmode;
		if (mode != VMODE_DUMMY_ENCL)
			DRM_ERROR("crtc [%d]: only support dummyl.\n", amcrtc->crtc_index);
	}

	DRM_INFO("%s-[%d]: enable mode %s final vmode %d\n",
		__func__, amcrtc->crtc_index, name, mode);

	if (mode == VMODE_DUMMY_ENCL ||
		mode == VMODE_DUMMY_ENCI ||
		mode == VMODE_DUMMY_ENCP) {
		ret = vout_func_set_current_vmode(amcrtc->vout_index, mode);
		if (ret)
			DRM_ERROR("crtc[%d]: new mode[%d] set error\n",
				amcrtc->crtc_index, mode);
		else
			meson_vout_update_mode_name(amcrtc->vout_index, name, "dummy");
	} else {
		if (meson_crtc_state->uboot_mode_init)
			mode |= VMODE_INIT_BIT_MASK;

		if (crtc->state->vrr_enabled &&
			adjusted_mode->hdisplay == old_mode->hdisplay &&
			adjusted_mode->vdisplay == old_mode->vdisplay) {
			set_vframe_rate_hint(adjusted_mode->vrefresh  * 100);
			DRM_INFO("%s-[%d], vrr set crtc enable, %d\n",
					__func__, amcrtc->crtc_index,
					adjusted_mode->vrefresh * 100);
			drm_crtc_vblank_on(crtc);
			return;
		}

		vout_func_set_state(amcrtc->vout_index, mode);
		vout_func_update_viu(amcrtc->vout_index);
	}

	meson_crtc_state->vmode = mode;

	memcpy(&pipeline->subs[amcrtc->crtc_index].mode, adjusted_mode,
	       sizeof(struct drm_display_mode));

	drm_crtc_vblank_on(crtc);
	enable_irq(amcrtc->irq);

	DRM_INFO("%s-[%d]: out\n", __func__, amcrtc->crtc_index);
}

static void am_meson_crtc_atomic_disable(struct drm_crtc *crtc,
	struct drm_crtc_state *old_state)
{
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);
	struct drm_display_mode *adjusted_mode = &crtc->state->adjusted_mode;
	struct drm_display_mode *old_mode = &old_state->adjusted_mode;
	struct am_meson_crtc_state *meson_crtc_state =
		to_am_meson_crtc_state(crtc->state);
	enum vmode_e mode;

	DRM_INFO("%s-[%d]:in\n", __func__, amcrtc->crtc_index);
	drm_crtc_vblank_off(crtc);

	if (crtc->state->vrr_enabled &&
		adjusted_mode->hdisplay == old_mode->hdisplay &&
		adjusted_mode->vdisplay == old_mode->vdisplay) {
		DRM_INFO("%s, vrr enable, skip crtc disable\n", __func__);
		return;
	}

	if (crtc->state->event && !crtc->state->active) {
		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irq(&crtc->dev->event_lock);
		crtc->state->event = NULL;
	}
	disable_irq(amcrtc->irq);

	meson_crtc_state->vmode = VMODE_INVALID;
	/* disable output by config null
	 * Todo: replace or delete it if have new method
	 */
	mode = vout_func_validate_vmode(amcrtc->vout_index, "null", 0);
	if (mode == VMODE_MAX) {
		DRM_ERROR("no matched vout mode\n");
		return;
	}
	vout_func_set_state(amcrtc->vout_index, mode);
	DRM_DEBUG("%s:out\n", __func__);
}

static int meson_crtc_atomic_check(struct drm_crtc *crtc,
	struct drm_crtc_state *crtc_state)
{
	int ret;
	struct meson_vpu_pipeline_state *mvps;
	struct meson_vpu_sub_pipeline_state *mvsps;
	struct drm_display_mode *mode;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);
	struct am_meson_crtc_state *cur_state =
		to_am_meson_crtc_state(crtc->state);
	struct am_meson_crtc_state *new_state =
		to_am_meson_crtc_state(crtc_state);

	ret = 0;
	mvps = meson_vpu_pipeline_get_state(amcrtc->pipeline, crtc_state->state);
	mvsps = &mvps->sub_states[crtc->index];
	mode = &crtc_state->mode;
	if (mode->hdisplay > 4096 || mode->vdisplay > 2160)
		mvsps->more_4k = 1;
	else
		mvsps->more_4k = 0;

	if (mode->vrefresh > 60)
		mvsps->more_60 = 1;
	else
		mvsps->more_60 = 0;

	/*apply parameters need modeset.*/
	if (crtc_state->state->allow_modeset) {

		if (cur_state->crtc_dv_enable != new_state->crtc_dv_enable)
			crtc_state->mode_changed = true;

		if (cur_state->eotf_type_by_property != new_state->eotf_type_by_property)
			crtc_state->mode_changed = true;

		if (cur_state->dv_mode != new_state->dv_mode)
			crtc_state->mode_changed = true;
	}

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
	struct meson_vpu_sub_pipeline *sub_pipe;
	unsigned long flags;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);
	struct drm_atomic_state *old_atomic_state = old_state->state;
	struct meson_drm *priv = amcrtc->priv;
	struct meson_vpu_pipeline *pipeline = amcrtc->pipeline;
	struct am_meson_crtc_state *old_crtc_state =
		to_am_meson_crtc_state(old_state);
	struct am_meson_crtc_state *meson_crtc_state;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
	int gamma_lut_size = 0;
#endif

	int crtc_index = amcrtc->crtc_index;

	sub_pipe = &pipeline->subs[crtc_index];
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

	vpu_pipeline_prepare_update(amcrtc->pipeline,
			crtc->mode.vdisplay, crtc->mode.vrefresh, crtc_index);
	if (!meson_crtc_state->uboot_mode_init) {
		vpu_osd_pipeline_update(sub_pipe, old_atomic_state);
		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		vpu_pipeline_finish_update(pipeline, crtc_index);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
	}

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	if (crtc->state->event) {
		if (drm_crtc_vblank_get(crtc) == 0)
			drm_crtc_arm_vblank_event(crtc, crtc->state->event);
		else
			amcrtc->event = crtc->state->event;

		crtc->state->event = NULL;
	}
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
	if (meson_crtc_state->crtc_hdr_process_policy !=
		old_crtc_state->crtc_hdr_process_policy) {
		set_hdr_policy(meson_crtc_state->crtc_hdr_process_policy);
	}
#endif
}

static const struct drm_crtc_helper_funcs am_crtc_helper_funcs = {
	.atomic_check	= meson_crtc_atomic_check,
	.mode_fixup		= am_meson_crtc_mode_fixup,
	.atomic_begin	= am_meson_crtc_atomic_begin,
	.atomic_flush	= am_meson_crtc_atomic_flush,
	.atomic_enable	= am_meson_crtc_atomic_enable,
	.atomic_disable	= am_meson_crtc_atomic_disable,
};

static int meson_crtc_get_scanout_position(struct am_meson_crtc *crtc,
	bool in_vblank_irq, int *vpos, int *hpos,
	ktime_t *stime, ktime_t *etime,
	const struct drm_display_mode *mode)
{
	int ret = 0;
	/*adjust mode crtc_vtotal is same as logical vtotal*/
	int real_vtotal = 0;

	if (stime)
		*stime = ktime_get();

	ret = vpu_pipeline_read_scanout_pos(crtc->pipeline, vpos, hpos, crtc->crtc_index);

	if (mode->flags & DRM_MODE_FLAG_INTERLACE) {
		/* for interlace mode, enc 0 ~ vtotal*2
		 * include two interlace image.
		 */
		real_vtotal = mode->crtc_vtotal >> 1;
		if (*vpos >= real_vtotal)
			*vpos -= real_vtotal;
	}

	if (etime)
		*etime = ktime_get();

	return ret;
}

/* Optional eotf properties. */
static const struct drm_prop_enum_list hdmi_eotf_enum_list[] = {
	{ HDMI_EOTF_TRADITIONAL_GAMMA_SDR, "SDR" },
	{ HDMI_EOTF_SMPTE_ST2084, "HDR" },
	{ HDMI_EOTF_MESON_DOLBYVISION, "HDMI_EOTF_MESON_DOLBYVISION" },
	{ HDMI_EOTF_MESON_DOLBYVISION_LL, "HDMI_EOTF_MESON_DOLBYVISION_LL" },
	{ EOTF_RESERVED, "EOTF_RESERVED" }
};

static void meson_crtc_init_hdmi_eotf_property(struct drm_device *drm_dev,
						   struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_enum(drm_dev, 0, "EOTF",
					hdmi_eotf_enum_list,
					ARRAY_SIZE(hdmi_eotf_enum_list));
	if (prop) {
		amcrtc->hdmi_eotf = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, EOTF_RESERVED);
	} else {
		DRM_ERROR("Failed to EOTF property\n");
	}
}

static void meson_crtc_init_property(struct drm_device *drm_dev,
						  struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_bool(drm_dev, 0, "meson.crtc.hdr_policy");
	if (prop) {
		amcrtc->hdr_policy = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to UPDATE property\n");
	}
}

static void meson_crtc_init_dv_enable_property(struct drm_device *drm_dev,
						  struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_bool(drm_dev, 0, "dv_enable");
	if (prop) {
		amcrtc->dv_enable_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to dv_enable property\n");
	}
}

static void meson_crtc_init_dv_mode_property(struct drm_device *drm_dev,
						  struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_bool(drm_dev, 0, "dv_mode");
	if (prop) {
		amcrtc->dv_mode_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to dv_mode property\n");
	}
}

static void meson_crtc_add_bgcolor_property(struct drm_device *drm_dev,
						  struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0, "BACKGROUND_COLOR",
					0, GENMASK_ULL(63, 0));
	if (prop) {
		amcrtc->bgcolor_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to background color property\n");
	}
}

struct am_meson_crtc *meson_crtc_bind(struct meson_drm *priv, int idx)
{
	struct am_meson_crtc *amcrtc;
	struct drm_crtc *crtc;
	struct meson_vpu_pipeline *pipeline = priv->pipeline;
	struct drm_plane *primary_plane;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
	int gamma_lut_size = 0;
#endif
	int ret, plane_index;
	char crtc_name[64];

	DRM_INFO("%s\n", __func__);

	amcrtc = devm_kzalloc(priv->dev, sizeof(*amcrtc), GFP_KERNEL);
	if (!amcrtc)
		return NULL;

	amcrtc->priv = priv;
	amcrtc->dev = priv->dev;
	amcrtc->drm_dev = priv->drm;
	amcrtc->crtc_index = idx;
	amcrtc->vout_index = idx + 1;/*vout index start from 1.*/
	crtc = &amcrtc->base;
	plane_index = priv->primary_plane_index[idx];
	primary_plane = &priv->osd_planes[plane_index]->base;

	snprintf(crtc_name, 64, "%s-%d", "VPP", amcrtc->crtc_index);

	ret = drm_crtc_init_with_planes(priv->drm, crtc,
					primary_plane, priv->cursor_plane,
					&am_meson_crtc_funcs, crtc_name);
	if (ret) {
		dev_err(amcrtc->drm_dev->dev, "Failed to init CRTC\n");
		return NULL;
	}

	drm_crtc_helper_add(crtc, &am_crtc_helper_funcs);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	meson_vpu_reg_handle_register(amcrtc->crtc_index);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
	amvecm_drm_init(0);
	gamma_lut_size = amvecm_drm_get_gamma_size(0);
	drm_mode_crtc_set_gamma_size(crtc, gamma_lut_size);
	drm_crtc_enable_color_mgmt(crtc, 0, true, gamma_lut_size);
#endif

	amcrtc->get_scanout_position = meson_crtc_get_scanout_position;
	amcrtc->force_crc_chk = 8;
	atomic_set(&amcrtc->commit_num, 0);
	mutex_init(&amcrtc->commit_mutex);
	spin_lock_init(&amcrtc->present_fence.lock);
	meson_crtc_init_property(priv->drm, amcrtc);
	meson_crtc_init_hdmi_eotf_property(priv->drm, amcrtc);
	meson_crtc_init_dv_enable_property(priv->drm, amcrtc);
	meson_crtc_init_dv_mode_property(priv->drm, amcrtc);
	meson_crtc_add_bgcolor_property(priv->drm, amcrtc);
	amcrtc->pipeline = pipeline;
	strcpy(amcrtc->osddump_path, OSD_DUMP_PATH);
	priv->crtcs[priv->num_crtcs++] = amcrtc;

	return amcrtc;
}

