// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include "meson_vpu_pipeline.h"
#include "meson_drv.h"

static inline void
vpu_pipeline_state_set(struct meson_vpu_block *pblk,
		       struct meson_vpu_block_state *state)
{
	u8 i;

	for (i = 0; i < pblk->avail_inputs; i++) {
		memcpy(&state->inputs[i], &pblk->inputs[i],
		       sizeof(struct meson_vpu_block_link));
	}
	for (i = 0; i < pblk->avail_outputs; i++) {
		memcpy(&state->outputs[i], &pblk->outputs[i],
		       sizeof(struct meson_vpu_block_link));
	}
	state->in_stack = 0;
	state->active = 1;
}

static struct drm_private_state *
meson_vpu_osd_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct meson_vpu_block *mvb;
	struct meson_vpu_osd_state *state;

	mvb = priv_to_block(obj);

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	state->base.pblk = mvb;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &state->base.obj);
	vpu_pipeline_state_set(mvb, &state->base);

	return &state->base.obj;
}

static void meson_vpu_osd_atomic_destroy_state(struct drm_private_obj *obj,
					       struct drm_private_state *state)
{
	struct meson_vpu_block_state *mvbs = priv_to_block_state(state);
	struct meson_vpu_osd_state *mvos = to_osd_state(mvbs);

	DRM_DEBUG("%s id=%d,index=%d\n",
		  mvbs->pblk->name, mvbs->pblk->id, mvbs->pblk->index);
	kfree(mvos);
}

static const struct drm_private_state_funcs meson_vpu_osd_obj_funcs = {
	.atomic_duplicate_state = meson_vpu_osd_atomic_duplicate_state,
	.atomic_destroy_state = meson_vpu_osd_atomic_destroy_state,
};

static int meson_vpu_osd_state_init(struct meson_drm *private,
				    struct meson_vpu_osd *osd)
{
	struct meson_vpu_osd_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->base.pblk = &osd->base;
	drm_atomic_private_obj_init(private->drm, &osd->base.obj,
				    &state->base.obj,
				    &meson_vpu_osd_obj_funcs);

	return 0;
}

static struct drm_private_state *
meson_vpu_video_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct meson_vpu_block *mvb;
	struct meson_vpu_video_state *state;

	mvb = priv_to_block(obj);

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	state->base.pblk = mvb;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &state->base.obj);
	vpu_pipeline_state_set(mvb, &state->base);

	return &state->base.obj;
}

static void
meson_vpu_video_atomic_destroy_state(struct drm_private_obj *obj,
				     struct drm_private_state *state)
{
	struct meson_vpu_block_state *mvbs = priv_to_block_state(state);
	struct meson_vpu_video_state *mvvs = to_video_state(mvbs);

	DRM_DEBUG("%s id=%d,index=%d\n",
		  mvbs->pblk->name, mvbs->pblk->id, mvbs->pblk->index);
	kfree(mvvs);
}

static const struct drm_private_state_funcs meson_vpu_video_obj_funcs = {
	.atomic_duplicate_state = meson_vpu_video_atomic_duplicate_state,
	.atomic_destroy_state = meson_vpu_video_atomic_destroy_state,
};

static int meson_vpu_video_state_init(struct meson_drm *private,
				      struct meson_vpu_video *video)
{
	struct meson_vpu_video_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->base.pblk = &video->base;
	drm_atomic_private_obj_init(private->drm, &video->base.obj,
				    &state->base.obj,
				    &meson_vpu_video_obj_funcs);

	return 0;
}

static struct drm_private_state *
meson_vpu_afbc_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct meson_vpu_block *mvb;
	struct meson_vpu_afbc_state *state;

	mvb = priv_to_block(obj);

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	state->base.pblk = mvb;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &state->base.obj);
	vpu_pipeline_state_set(mvb, &state->base);

	return &state->base.obj;
}

static void
meson_vpu_afbc_atomic_destroy_state(struct drm_private_obj *obj,
				    struct drm_private_state *state)
{
	struct meson_vpu_block_state *mvbs = priv_to_block_state(state);
	struct meson_vpu_afbc_state *mvas = to_afbc_state(mvbs);

	kfree(mvas);
}

static const struct drm_private_state_funcs meson_vpu_afbc_obj_funcs = {
	.atomic_duplicate_state = meson_vpu_afbc_atomic_duplicate_state,
	.atomic_destroy_state = meson_vpu_afbc_atomic_destroy_state,
};

static int meson_vpu_afbc_state_init(struct meson_drm *private,
				     struct meson_vpu_afbc *afbc)
{
	struct meson_vpu_afbc_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->base.pblk = &afbc->base;
	drm_atomic_private_obj_init(private->drm, &afbc->base.obj,
				    &state->base.obj,
				    &meson_vpu_afbc_obj_funcs);

	return 0;
}

/*afbc block state ops end
 */

/*scaler block state ops start
 */

static struct drm_private_state *
meson_vpu_scaler_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct meson_vpu_block *mvb;
	struct meson_vpu_scaler_state *state;

	mvb = priv_to_block(obj);

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	state->base.pblk = mvb;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &state->base.obj);
	vpu_pipeline_state_set(mvb, &state->base);

	return &state->base.obj;
}

static void
meson_vpu_scaler_atomic_destroy_state(struct drm_private_obj *obj,
				      struct drm_private_state *state)
{
	struct meson_vpu_block_state *mvbs = priv_to_block_state(state);
	struct meson_vpu_scaler_state *mvss = to_scaler_state(mvbs);

	kfree(mvss);
}

static const struct drm_private_state_funcs meson_vpu_scaler_obj_funcs = {
	.atomic_duplicate_state = meson_vpu_scaler_atomic_duplicate_state,
	.atomic_destroy_state = meson_vpu_scaler_atomic_destroy_state,
};

static int meson_vpu_scaler_state_init(struct meson_drm *private,
				       struct meson_vpu_scaler *scaler)
{
	struct meson_vpu_scaler_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->base.pblk = &scaler->base;
	drm_atomic_private_obj_init(private->drm, &scaler->base.obj,
				    &state->base.obj,
				    &meson_vpu_scaler_obj_funcs);

	return 0;
}

static struct drm_private_state *
meson_vpu_osdblend_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct meson_vpu_block *mvb;
	struct meson_vpu_osdblend_state *state;

	mvb = priv_to_block(obj);

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	state->base.pblk = mvb;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &state->base.obj);
	vpu_pipeline_state_set(mvb, &state->base);

	return &state->base.obj;
}

static void
meson_vpu_osdblend_atomic_destroy_state(struct drm_private_obj *obj,
					struct drm_private_state *state)
{
	struct meson_vpu_block_state *mvbs = priv_to_block_state(state);
	struct meson_vpu_osdblend_state *mvos = to_osdblend_state(mvbs);

	kfree(mvos);
}

static const struct drm_private_state_funcs meson_vpu_osdblend_obj_funcs = {
	.atomic_duplicate_state = meson_vpu_osdblend_atomic_duplicate_state,
	.atomic_destroy_state = meson_vpu_osdblend_atomic_destroy_state,
};

static int meson_vpu_osdblend_state_init(struct meson_drm *private,
					 struct meson_vpu_osdblend *osdblend)
{
	struct meson_vpu_osdblend_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->base.pblk = &osdblend->base;
	drm_atomic_private_obj_init(private->drm, &osdblend->base.obj,
				    &state->base.obj,
				    &meson_vpu_osdblend_obj_funcs);

	return 0;
}

static struct drm_private_state *
meson_vpu_hdr_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct meson_vpu_block *mvb;
	struct meson_vpu_hdr_state *state;

	mvb = priv_to_block(obj);

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	state->base.pblk = mvb;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &state->base.obj);
	vpu_pipeline_state_set(mvb, &state->base);

	return &state->base.obj;
}

static void meson_vpu_hdr_atomic_destroy_state(struct drm_private_obj *obj,
					       struct drm_private_state *state)
{
	struct meson_vpu_block_state *mvbs = priv_to_block_state(state);
	struct meson_vpu_hdr_state *mvhs = to_hdr_state(mvbs);

	kfree(mvhs);
}

static const struct drm_private_state_funcs meson_vpu_hdr_obj_funcs = {
	.atomic_duplicate_state = meson_vpu_hdr_atomic_duplicate_state,
	.atomic_destroy_state = meson_vpu_hdr_atomic_destroy_state,
};

static int meson_vpu_hdr_state_init(struct meson_drm *private,
				    struct meson_vpu_hdr *hdr)
{
	struct meson_vpu_hdr_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->base.pblk = &hdr->base;
	drm_atomic_private_obj_init(private->drm, &hdr->base.obj,
				    &state->base.obj,
				    &meson_vpu_hdr_obj_funcs);

	return 0;
}

/*hdr block state ops end
 */

/*postblend block state ops start
 */

static struct drm_private_state *
meson_vpu_postblend_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct meson_vpu_block *mvb;
	struct meson_vpu_postblend_state *state;

	mvb = priv_to_block(obj);

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	state->base.pblk = mvb;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &state->base.obj);
	vpu_pipeline_state_set(mvb, &state->base);

	return &state->base.obj;
}

static void
meson_vpu_postblend_atomic_destroy_state(struct drm_private_obj *obj,
					 struct drm_private_state *state)
{
	struct meson_vpu_block_state *mvbs = priv_to_block_state(state);
	struct meson_vpu_postblend_state *mvas = to_postblend_state(mvbs);

	kfree(mvas);
}

static const struct drm_private_state_funcs meson_vpu_postblend_obj_funcs = {
	.atomic_duplicate_state = meson_vpu_postblend_atomic_duplicate_state,
	.atomic_destroy_state = meson_vpu_postblend_atomic_destroy_state,
};

static int
meson_vpu_postblend_state_init(struct meson_drm *private,
			       struct meson_vpu_postblend *postblend)
{
	struct meson_vpu_postblend_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->base.pblk = &postblend->base;
	drm_atomic_private_obj_init(private->drm, &postblend->base.obj,
				    &state->base.obj,
				    &meson_vpu_postblend_obj_funcs);
	return 0;
}

/*postblend block state ops end
 */
static struct drm_private_state *
meson_vpu_pipeline_atomic_duplicate_state(struct drm_private_obj *obj)
{
	struct meson_vpu_pipeline_state *state;
	struct meson_vpu_pipeline *pipeline = priv_to_pipeline(obj);
	struct meson_vpu_pipeline_state *cur_state = priv_to_pipeline_state(obj->state);

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	memcpy(state, cur_state, sizeof(struct meson_vpu_pipeline_state));

	state->pipeline = pipeline;
	state->global_afbc = 0;
	return &state->obj;
}

static void
meson_vpu_pipeline_atomic_destroy_state(struct drm_private_obj *obj,
					struct drm_private_state *state)
{
	struct meson_vpu_pipeline_state *mvps = priv_to_pipeline_state(state);

	kfree(mvps);
}

static const struct drm_private_state_funcs meson_vpu_pipeline_obj_funcs = {
	.atomic_duplicate_state = meson_vpu_pipeline_atomic_duplicate_state,
	.atomic_destroy_state = meson_vpu_pipeline_atomic_destroy_state,
};

static int meson_vpu_pipeline_state_init(struct meson_drm *private,
					 struct meson_vpu_pipeline *pipeline)
{
	struct meson_vpu_pipeline_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	state->pipeline = pipeline;
	drm_atomic_private_obj_init(private->drm, &pipeline->obj, &state->obj,
				    &meson_vpu_pipeline_obj_funcs);

	return 0;
}

struct meson_vpu_block_state *
meson_vpu_block_get_state(struct meson_vpu_block *block,
			  struct drm_atomic_state *state)
{
	struct drm_private_state *dps;
	struct meson_vpu_block_state *mvbs;

	dps = drm_atomic_get_private_obj_state(state, &block->obj);
	if (dps) {
		mvbs = priv_to_block_state(dps);
		return mvbs;
	}

	return NULL;
}

struct meson_vpu_block_state *
meson_vpu_block_get_old_state(struct meson_vpu_block *mvb,
			struct drm_atomic_state *state)
{
	struct drm_private_obj *obj;
	struct drm_private_state *old_obj_state;
	struct meson_vpu_block_state *mvbs = NULL;
	int i;

	for_each_old_private_obj_in_state(state, obj, old_obj_state, i) {
		mvbs = priv_to_block_state(old_obj_state);
		if (mvb == mvbs->pblk)
			return mvbs;
	}

	return NULL;
}

struct meson_vpu_pipeline_state *
meson_vpu_pipeline_get_state(struct meson_vpu_pipeline *pipeline,
			     struct drm_atomic_state *state)
{
	struct drm_private_state *dps;

	dps = drm_atomic_get_private_obj_state(state, &pipeline->obj);
	if (dps) {
		dps->state = state;
		return priv_to_pipeline_state(dps);
	}

	return NULL;
}

int meson_vpu_block_state_init(struct meson_drm *private,
			       struct meson_vpu_pipeline *pipeline)
{
	int i, ret;

	ret = meson_vpu_pipeline_state_init(private, pipeline);
	if (ret)
		return ret;

	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (pipeline->osds[i]) {
			ret = meson_vpu_osd_state_init(private,
						       pipeline->osds[i]);
			if (ret)
				return ret;
		}
	}

	for (i = 0; i < pipeline->num_video; i++) {
		ret = meson_vpu_video_state_init(private, pipeline->video[i]);
		if (ret)
			return ret;
	}

	for (i = 0; i < pipeline->num_afbc_osds; i++) {
		ret = meson_vpu_afbc_state_init(private,
						pipeline->afbc_osds[i]);
		if (ret)
			return ret;
	}

	for (i = 0; i < pipeline->num_scalers; i++) {
		ret = meson_vpu_scaler_state_init(private,
						  pipeline->scalers[i]);
		if (ret)
			return ret;
	}

	ret = meson_vpu_osdblend_state_init(private, pipeline->osdblend);
	if (ret)
		return ret;

	for (i = 0; i < pipeline->num_hdrs; i++) {
		ret = meson_vpu_hdr_state_init(private, pipeline->hdrs[i]);
		if (ret)
			return ret;
	}

	for (i = 0; i < pipeline->num_postblend; i++) {
		ret = meson_vpu_postblend_state_init(private,
				pipeline->postblends[i]);
		if (ret)
			return ret;
	}

	return 0;
}
