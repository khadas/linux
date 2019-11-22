/*
 * drivers/amlogic/drm/vpu-hw/meson_vpu_hdr_dv.c
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

#include "meson_vpu_pipeline.h"

static int hdr_check_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state,
		struct meson_vpu_pipeline_state *mvps)
{
	struct meson_vpu_hdr *hdr = to_hdr_block(vblk);

	if (state->checked)
		return 0;

	state->checked = true;

	//vpu_block_check_input(vblk, state, mvps);

	DRM_DEBUG("%s set_state called.\n", hdr->base.name);
	return 0;
}

static void hdr_set_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state)
{
	struct meson_vpu_hdr *hdr = to_hdr_block(vblk);
	//struct meson_vpu_hdr_state *hdr_state = to_hdr_state(state);

	DRM_DEBUG("%s set_state called.\n", hdr->base.name);
}

static void hdr_enable(struct meson_vpu_block *vblk)
{
	struct meson_vpu_hdr *hdr = to_hdr_block(vblk);

	DRM_DEBUG("%s enable called.\n", hdr->base.name);
}

static void hdr_disable(struct meson_vpu_block *vblk)
{
	struct meson_vpu_hdr *hdr = to_hdr_block(vblk);

	DRM_DEBUG("%s disable called.\n", hdr->base.name);
}

struct meson_vpu_block_ops hdr_ops = {
	.check_state = hdr_check_state,
	.update_state = hdr_set_state,
	.enable = hdr_enable,
	.disable = hdr_disable,
};

static int dolby_check_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state,
		struct meson_vpu_pipeline_state *mvps)
{
	struct meson_vpu_dolby *dolby = to_dolby_block(vblk);

	if (state->checked)
		return 0;

	state->checked = true;

	//vpu_block_check_input(vblk, state, mvps);

	DRM_DEBUG("%s check_state called.\n", dolby->base.name);
	return 0;
}
static void dolby_set_state(struct meson_vpu_block *vblk,
		struct meson_vpu_block_state *state)
{
	struct meson_vpu_dolby *dolby = to_dolby_block(vblk);

	DRM_DEBUG("%s set_state called.\n", dolby->base.name);
}

static void dolby_enable(struct meson_vpu_block *vblk)
{
	struct meson_vpu_dolby *dolby = to_dolby_block(vblk);

	DRM_DEBUG("%s enable called.\n", dolby->base.name);
}

static void dolby_disable(struct meson_vpu_block *vblk)
{
	struct meson_vpu_dolby *dolby = to_dolby_block(vblk);

	DRM_DEBUG("%s disable called.\n", dolby->base.name);
}

struct meson_vpu_block_ops dolby_ops = {
	.check_state = dolby_check_state,
	.update_state = dolby_set_state,
	.enable = dolby_enable,
	.disable = dolby_disable,
};
