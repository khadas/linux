// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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

	MESON_DRM_BLOCK("%s check_state called.\n", hdr->base.name);
	return 0;
}

static void hdr_set_state(struct meson_vpu_block *vblk,
			  struct meson_vpu_block_state *state,
			  struct meson_vpu_block_state *old_state)
{
	struct meson_vpu_hdr *hdr = to_hdr_block(vblk);
	//struct meson_vpu_hdr_state *hdr_state = to_hdr_state(state);

	MESON_DRM_BLOCK("%s set_state called.\n", hdr->base.name);
}

static void hdr_enable(struct meson_vpu_block *vblk,
		       struct meson_vpu_block_state *state)
{
	struct meson_vpu_hdr *hdr = to_hdr_block(vblk);

	MESON_DRM_BLOCK("%s enable called.\n", hdr->base.name);
}

static void hdr_disable(struct meson_vpu_block *vblk,
			struct meson_vpu_block_state *state)
{
	struct meson_vpu_hdr *hdr = to_hdr_block(vblk);

	MESON_DRM_BLOCK("%s disable called.\n", hdr->base.name);
}

struct meson_vpu_block_ops hdr_ops = {
	.check_state = hdr_check_state,
	.update_state = hdr_set_state,
	.enable = hdr_enable,
	.disable = hdr_disable,
};

static int db_check_state(struct meson_vpu_block *vblk,
			     struct meson_vpu_block_state *state,
		struct meson_vpu_pipeline_state *mvps)
{
	struct meson_vpu_db *mvd = to_db_block(vblk);

	if (state->checked)
		return 0;

	state->checked = true;

	//vpu_block_check_input(vblk, state, mvps);

	MESON_DRM_BLOCK("%s check_state called.\n", mvd->base.name);
	return 0;
}

static void db_set_state(struct meson_vpu_block *vblk,
			    struct meson_vpu_block_state *state,
			    struct meson_vpu_block_state *old_state)
{
	struct meson_vpu_db *mvd = to_db_block(vblk);

	MESON_DRM_BLOCK("%s set_state called.\n", mvd->base.name);
}

static void db_enable(struct meson_vpu_block *vblk,
			 struct meson_vpu_block_state *state)
{
	struct meson_vpu_db *mvd = to_db_block(vblk);

	MESON_DRM_BLOCK("%s enable called.\n", mvd->base.name);
}

static void db_disable(struct meson_vpu_block *vblk,
			  struct meson_vpu_block_state *state)
{
	struct meson_vpu_db *mvd = to_db_block(vblk);

	MESON_DRM_BLOCK("%s disable called.\n", mvd->base.name);
}

struct meson_vpu_block_ops db_ops = {
	.check_state = db_check_state,
	.update_state = db_set_state,
	.enable = db_enable,
	.disable = db_disable,
};
