/*
 * drivers/amlogic/drm/meson_vpu_pipeline_traverse.c
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

#include <dt-bindings/display/meson-drm-ids.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include "meson_vpu_pipeline.h"
#include "meson_drv.h"

static void stack_init(struct meson_vpu_stack *mvs)
{
	mvs->top = 0;
	memset(mvs->stack, 0, sizeof(struct meson_vpu_block *) *
						MESON_MAX_BLOCKS);
}

static void stack_push(struct meson_vpu_stack *mvs, struct meson_vpu_block *mvb)
{
	mvs->stack[mvs->top++] = mvb;
}

static struct meson_vpu_block *stack_pop(struct meson_vpu_stack *mvs)
{
	struct meson_vpu_block *mvb;

	mvb = mvs->stack[--mvs->top];
	mvs->stack[mvs->top] = NULL;
	return mvb;
}

static struct meson_vpu_block *neighbour(struct meson_vpu_block_state *mvbs,
					 int *index,
					 struct drm_atomic_state *state)
{
	int i;
	struct meson_vpu_block_link *mvbl;
	struct meson_vpu_block_state *next_state;

	for (i = 0; i < MESON_BLOCK_MAX_OUTPUTS; i++) {
		mvbl = &mvbs->outputs[i];

		if (!mvbl->link)
			continue;
		next_state = meson_vpu_block_get_state(mvbl->link, state);
		if (next_state->in_stack) {
			DRM_DEBUG("%s already in stack.\n", mvbl->link->name);
			continue;
		}
		if (!next_state->active) {
			DRM_DEBUG("%s is not active.\n", mvbl->link->name);
			continue;
		}
		if (!mvbl->edges_active) {
			DRM_DEBUG("edges is not active.\n");
			continue;
		}
		if (mvbl->edges_visited) {
			DRM_DEBUG("edges is already visited.\n");
			continue;
		}

		*index = i;
		return mvbl->link;
	}

	return NULL;
}

static void pipeline_visit_clean(struct meson_vpu_block_state *curr_state)
{
	int index;

	for (index = 0; index < MESON_BLOCK_MAX_OUTPUTS; index++)
		curr_state->outputs[index].edges_visited = 0;
}

/**
 * pipeline_dfs - dfs algorithm to search path
 * @osd_index: osd layer index
 * @start: the start block of the dfs
 * @end: the end block of the dfs
 *
 * use the non-recursive dfs algorithm to search all paths from the start block
 * to the end block, the result will be saved to the meson_vpu_traverse struct.
 *
 */
static void pipeline_dfs(int osd_index, struct meson_vpu_pipeline_state *mvps,
			 struct meson_vpu_block *start,
			 struct meson_vpu_block *end,
			 struct drm_atomic_state *state)
{
	struct meson_vpu_block *curr, *next, *prev;
	struct meson_vpu_block_state *curr_state, *next_state, *prev_state;
	int i, j, index;

	struct meson_vpu_stack *mvs = &mvps->osd_stack[osd_index];
	struct meson_vpu_traverse *mvt = &mvps->osd_traverse[osd_index];

	stack_init(mvs);
	stack_push(mvs, start);
	mvt->num_path = 0;
	j = 0;
	DRM_DEBUG("start->id=%d,name=%s\n", start->id, start->name);
	DRM_DEBUG("end->id=%d,name=%s\n", end->id, end->name);

	while (mvs->top) {
		if (mvs->stack[mvs->top - 1] == end) {
			for (i = 0; i < mvs->top; i++) {
				mvt->path[j][i] = mvs->stack[i];
				DRM_DEBUG("%s->\n", mvs->stack[i]->name);
			}
			j++;
			mvt->num_path++;
			DRM_DEBUG("\n");
			prev = stack_pop(mvs);
			prev_state = meson_vpu_block_get_state(prev, state);
			prev_state->in_stack = 0;
		} else {
			curr = mvs->stack[mvs->top - 1];
			curr_state = meson_vpu_block_get_state(curr, state);
			next = neighbour(curr_state, &index, state);

			if (next) {
				DRM_DEBUG("next->id=%d,name=%s\n",
					  next->id, next->name);
				curr_state->outputs[index].edges_visited = 1;
				next_state =
					meson_vpu_block_get_state(next, state);
				stack_push(mvs, next);
				next_state->in_stack = 1;
			} else {
				DRM_DEBUG("next is NULL!!\n");
				stack_pop(mvs);
				curr_state->in_stack = 0;
				pipeline_visit_clean(curr_state);
			}
		}
	}
}

static u8 find_out_port(struct meson_vpu_block *in,
				struct meson_vpu_block *out)
{
	int i;
	struct meson_vpu_block_link *mvbl;

	for (i = 0; i < out->avail_inputs; i++) {
		mvbl = &out->inputs[i];
		if (mvbl->link == in)
			return mvbl->port;
	}
	return 0;
}

void vpu_pipeline_scaler_scope_size_calc(u8 index, u8 osd_index,
			struct meson_vpu_pipeline_state *mvps)
{
	u8 m, i;
	u32 ratio_x[MESON_MAX_SCALERS], ratio_y[MESON_MAX_SCALERS];
	struct meson_vpu_scaler_param *scaler_param, *scaler_param_1;

	i = index;
	if (mvps->scaler_cnt[i] == 0) {
		/*scope size calc*/
		mvps->osd_scope_pre[osd_index].h_start =
			mvps->plane_info[osd_index].src_x;
		mvps->osd_scope_pre[osd_index].v_start =
			mvps->plane_info[osd_index].src_y;
		mvps->osd_scope_pre[osd_index].h_end =
			mvps->osd_scope_pre[osd_index].h_start
			+ mvps->plane_info[osd_index].src_w - 1;
		mvps->osd_scope_pre[osd_index].v_end =
			mvps->osd_scope_pre[osd_index].v_start
			+ mvps->plane_info[osd_index].src_h - 1;
	} else if (mvps->scaler_cnt[i] == 1) {
		m = mvps->scale_blk[i][0]->index;
		scaler_param = &mvps->scaler_param[m];
		scaler_param->ratio_x =
			(mvps->plane_info[osd_index].src_w *
			RATIO_BASE) /
			mvps->plane_info[osd_index].dst_w;
		scaler_param->ratio_y =
			(mvps->plane_info[osd_index].src_h *
			RATIO_BASE) /
			mvps->plane_info[osd_index].dst_h;
		scaler_param->calc_done_mask |=
			SCALER_RATIO_X_CALC_DONE |
			SCALER_RATIO_Y_CALC_DONE;
		if (scaler_param->before_osdblend) {
			/*scale size calc firstly*/
			scaler_param->input_width =
				mvps->plane_info[osd_index].src_w;
			scaler_param->input_height =
				mvps->plane_info[osd_index].src_h;
			scaler_param->output_width =
				mvps->plane_info[osd_index].dst_w;
			scaler_param->output_height =
				mvps->plane_info[osd_index].dst_h;
			scaler_param->calc_done_mask |=
				SCALER_IN_W_CALC_DONE |
				SCALER_IN_H_CALC_DONE |
				SCALER_OUT_W_CALC_DONE |
				SCALER_OUT_H_CALC_DONE;
			/*scope size calc*/
			mvps->osd_scope_pre[osd_index].h_start =
				mvps->plane_info[osd_index].dst_x;
			mvps->osd_scope_pre[osd_index].v_start =
				mvps->plane_info[osd_index].dst_y;
			mvps->osd_scope_pre[osd_index].h_end =
				mvps->osd_scope_pre[osd_index].h_start
				+ scaler_param->output_width - 1;
			mvps->osd_scope_pre[osd_index].v_end =
				mvps->osd_scope_pre[osd_index].v_start
				+ scaler_param->output_height - 1;
		} else {/*scaler position is after osdlend*/
			/*scope size calc firstly*/
			mvps->osd_scope_pre[osd_index].h_start =
				mvps->plane_info[osd_index].src_x;
			mvps->osd_scope_pre[osd_index].v_start =
				mvps->plane_info[osd_index].src_y;
			mvps->osd_scope_pre[osd_index].h_end =
				mvps->osd_scope_pre[osd_index].h_start
				+ mvps->plane_info[osd_index].src_w - 1;
			mvps->osd_scope_pre[osd_index].v_end =
				mvps->osd_scope_pre[osd_index].v_start
				+ mvps->plane_info[osd_index].src_h - 1;
			/*scaler size calc*/
			scaler_param->input_width =
				mvps->osd_scope_pre[osd_index].h_end + 1;
			scaler_param->input_height =
				mvps->osd_scope_pre[osd_index].v_end + 1;
			scaler_param->output_width =
				mvps->plane_info[osd_index].dst_x +
				mvps->plane_info[osd_index].dst_w;
			scaler_param->output_height =
				mvps->plane_info[osd_index].dst_y +
				mvps->plane_info[osd_index].dst_h;
			scaler_param->calc_done_mask |=
				SCALER_IN_W_CALC_DONE |
				SCALER_IN_H_CALC_DONE |
				SCALER_OUT_W_CALC_DONE |
				SCALER_OUT_H_CALC_DONE;
		}
	} else if (mvps->scaler_cnt[i] == 2) {
		m = mvps->scale_blk[i][0]->index;
		scaler_param = &mvps->scaler_param[m];
		m = mvps->scale_blk[i][1]->index;
		scaler_param_1 = &mvps->scaler_param[m];
		if (scaler_param_1->calc_done_mask &
			SCALER_RATIO_X_CALC_DONE) {/*TODO*/
			ratio_x[1] = scaler_param_1->ratio_x;
			ratio_y[1] = scaler_param_1->ratio_y;
			/*recheck scaler size*/
		} else {/*TODO*/
			ratio_x[1] = RATIO_BASE;
			ratio_y[1] = RATIO_BASE;
			scaler_param_1->calc_done_mask |=
				SCALER_RATIO_X_CALC_DONE |
				SCALER_RATIO_Y_CALC_DONE;
		}
		/*calculate scaler input/output size and scope*/
		if (scaler_param->before_osdblend) {
			scaler_param->input_width =
				mvps->plane_info[osd_index].src_w;
			scaler_param->input_height =
				mvps->plane_info[osd_index].src_h;
			scaler_param->output_width =
				mvps->plane_info[osd_index].dst_w *
				ratio_x[1] / RATIO_BASE;
			scaler_param->output_height =
				mvps->plane_info[osd_index].dst_h *
				ratio_y[1] / RATIO_BASE;
			scaler_param->calc_done_mask |=
				SCALER_IN_W_CALC_DONE |
				SCALER_IN_H_CALC_DONE |
				SCALER_OUT_W_CALC_DONE |
				SCALER_OUT_H_CALC_DONE;
			/*scope size calc*/
			mvps->osd_scope_pre[osd_index].h_start =
				mvps->plane_info[osd_index].dst_x *
				ratio_x[1] / RATIO_BASE;
			mvps->osd_scope_pre[osd_index].v_start =
				mvps->plane_info[osd_index].dst_y *
				ratio_y[1] / RATIO_BASE;
			mvps->osd_scope_pre[osd_index].h_end =
				mvps->osd_scope_pre[osd_index].h_start
				+ scaler_param->output_width - 1;
			mvps->osd_scope_pre[osd_index].v_end =
				mvps->osd_scope_pre[osd_index].v_start
				+ scaler_param->output_height - 1;
		} else {
			/*TODO*/
		}
		/*reclac second scaler size*/
		/*scaler_param_1->before_osdblend == 0*/
		if (scaler_param_1->input_width <
			mvps->osd_scope_pre[osd_index].h_end + 1) {
			scaler_param_1->input_width =
				mvps->osd_scope_pre[osd_index].h_end + 1;
			scaler_param_1->output_width =
				mvps->plane_info[osd_index].dst_x +
				mvps->plane_info[osd_index].dst_w;
		}
		if (scaler_param_1->input_height <
			mvps->osd_scope_pre[osd_index].v_end + 1) {
			scaler_param_1->input_height =
				mvps->osd_scope_pre[osd_index].v_end + 1;
			scaler_param_1->output_height =
				mvps->plane_info[osd_index].dst_y +
				mvps->plane_info[osd_index].dst_h;
		}
	}
}
static void vpu_osd_shift_recalc(struct meson_vpu_pipeline_state *state)
{
	u8 i;

	for (i = 0; i < MESON_MAX_OSDS; i++) {
		state->osd_scope_pre[i].v_start += 1;
		state->osd_scope_pre[i].v_end += 1;
	}
	state->scaler_param[0].input_height += 1;
}
int vpu_pipeline_scaler_check(int *combination, int num_planes,
				struct meson_vpu_pipeline_state *mvps)
{
	int i, j, osd_index, ret, m;
	struct meson_vpu_traverse *mvt;
	struct meson_vpu_block **mvb;
	struct meson_vpu_block *block;
	struct meson_vpu_scaler_param *scaler_param, *scaler_param_1;
	u32 ratio_x[MESON_MAX_SCALERS], ratio_y[MESON_MAX_SCALERS];
	bool have_blend;

	ret = 0;
	/*clean up scaler and scope size before check & calc*/
	memset(mvps->scaler_param, 0,
		MESON_MAX_SCALERS * sizeof(struct meson_vpu_scaler_param));
	memset(mvps->osd_scope_pre, 0,
		MESON_MAX_OSDS * sizeof(struct osd_scope_s));
	for (i = 0; i < MESON_MAX_OSDS && !ret; i++) {
		if (!mvps->plane_info[i].enable)
			continue;
		osd_index = mvps->plane_index[i];
		mvt = &mvps->osd_traverse[osd_index];
		mvb = mvt->path[combination[i]];
		mvps->scaler_cnt[i] = 0;
		have_blend = 0;

		for (j = 0; j < MESON_MAX_BLOCKS; j++) {
			block = mvb[j];
			if (!block)
				continue;
			if (block->type == MESON_BLK_OSDBLEND)
				have_blend = 1;
			if (block->type == MESON_BLK_SCALER) {
				m = mvps->scaler_cnt[i];
				mvps->scale_blk[i][m] = block;
				mvps->scaler_cnt[i]++;
				m = block->index;
				mvps->scaler_param[m].plane_mask |=
					BIT(osd_index);
				mvps->scaler_param[m].before_osdblend =
					have_blend ? 0:1;
			}
		}

		if (mvps->scaler_cnt[i] == 0) {
			ratio_x[0] = (mvps->plane_info[osd_index].src_w *
				RATIO_BASE) /
				mvps->plane_info[osd_index].dst_w;
			ratio_y[0] = (mvps->plane_info[osd_index].src_h *
				RATIO_BASE) /
				mvps->plane_info[osd_index].dst_h;
			if (ratio_x[0] != RATIO_BASE ||
				ratio_y[0] != RATIO_BASE) {
				ret = -1;
				break;
			}
			vpu_pipeline_scaler_scope_size_calc(i,
				osd_index, mvps);
		} else if (mvps->scaler_cnt[i] == 1) {
			vpu_pipeline_scaler_scope_size_calc(i,
				osd_index, mvps);
		} else if (mvps->scaler_cnt[i] == 2) {
			/*
			 *check second scaler firstly,
			 *if second scaler have not check,
			 *disable the second scaler.only use first scale.
			 */
			m = mvps->scale_blk[i][1]->index;
			scaler_param_1 = &mvps->scaler_param[m];
			if (scaler_param_1->calc_done_mask &
				SCALER_RATIO_X_CALC_DONE) {/*TODO*/
				ratio_x[1] = scaler_param_1->ratio_x;
				ratio_y[1] = scaler_param_1->ratio_y;
				/*recheck scaler size*/
			} else {/*TODO*/
				ratio_x[1] = RATIO_BASE;
				ratio_y[1] = RATIO_BASE;
				scaler_param_1->ratio_x = RATIO_BASE;
				scaler_param_1->ratio_y = RATIO_BASE;
				scaler_param_1->calc_done_mask |=
					SCALER_RATIO_X_CALC_DONE |
					SCALER_RATIO_Y_CALC_DONE;
			}
			ratio_x[0] = (mvps->plane_info[osd_index].src_w *
				RATIO_BASE) /
				mvps->plane_info[osd_index].dst_w;
			ratio_y[0] = (mvps->plane_info[osd_index].src_h *
				RATIO_BASE) /
				mvps->plane_info[osd_index].dst_h;
			m = mvps->scale_blk[i][0]->index;
			scaler_param = &mvps->scaler_param[m];
			scaler_param->ratio_x =
				(ratio_x[0] * RATIO_BASE) / ratio_x[1];
			scaler_param->ratio_y =
				(ratio_y[0] * RATIO_BASE) / ratio_y[1];
			scaler_param->calc_done_mask |=
				SCALER_RATIO_X_CALC_DONE |
				SCALER_RATIO_Y_CALC_DONE;
			ratio_x[0] = scaler_param->ratio_x;
			ratio_y[0] = scaler_param->ratio_y;

			if ((ratio_x[0] > RATIO_BASE &&
			     ratio_x[1] < RATIO_BASE) ||
				(ratio_y[0] > RATIO_BASE &&
				ratio_y[1] < RATIO_BASE)) {
				ret = -1;
				break;
			}
			vpu_pipeline_scaler_scope_size_calc(i,
				osd_index, mvps);
		}
	}
	if (ret == 0 && mvps->num_plane > 0 &&
		mvps->pipeline->osd_version <= OSD_V2)
		vpu_osd_shift_recalc(mvps);
	return ret;
}

/**
 * vpu_pipeline_check_block: check pipeline block
 * @combination: index array of every layer path
 * @num_planes: the number of layer
 *
 * For some blocks that have multiple output port,
 * call the ops->check interface to determain a valid path.
 *
 * RETURNS:
 * 0 for the valid path or -1 for the invalid path
 */
int vpu_pipeline_check_block(int *combination, int num_planes,
					struct meson_vpu_pipeline_state *mvps,
					struct drm_atomic_state *state)
{
	int i, j, osd_index, ret;
	struct meson_vpu_traverse *mvt;
	struct meson_vpu_block **mvb;
	struct meson_vpu_block *block;
	struct meson_vpu_block *osdblend;
	struct meson_vpu_block_state *mvbs;

	osdblend = &mvps->pipeline->osdblend->base;
	ret = vpu_pipeline_scaler_check(combination, num_planes, mvps);
	if (ret)
		return -1;

	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (!mvps->plane_info[i].enable)
			continue;
		osd_index = mvps->plane_index[i];
		mvt = &mvps->osd_traverse[osd_index];
		mvb = mvt->path[combination[i]];
		mvps->scaler_cnt[i] = 0;

		for (j = 0; j < MESON_MAX_BLOCKS; j++) {
			block = mvb[j];
			if (!block)
				break;

			if (block == osdblend) {
				mvps->dout_index[i] =
					find_out_port(block, mvb[j+1]);
				DRM_DEBUG("osd-%d blend out port: %d.\n",
						i, mvps->dout_index[i]);
				break;
			}
		}
	}

	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (!mvps->plane_info[i].enable)
			continue;
		osd_index = mvps->plane_index[i];
		mvt = &mvps->osd_traverse[osd_index];
		mvb = mvt->path[combination[i]];

		for (j = 0; j < MESON_MAX_BLOCKS; j++) {
			block = mvb[j];
			if (!block)
				break;

			if (block->ops && block->ops->check_state) {
				mvbs = meson_vpu_block_get_state(block, state);
				ret = block->ops->check_state(block,
						mvbs, mvps);

				if (ret) {
					DRM_ERROR("%s block check error.\n",
						block->name);
					return ret;
				}
			}
		}
	}

	return ret;
}

void vpu_pipeline_enable_block(int *combination, int num_planes,
				struct meson_vpu_pipeline_state *mvps)
{
	int i, j, osd_index;
	struct meson_vpu_traverse *mvt;
	struct meson_vpu_block **mvb;
	struct meson_vpu_block *block;

	mvps->enable_blocks = 0;

	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (!mvps->plane_info[i].enable)
			continue;
		osd_index = mvps->plane_index[i];
		mvt = &mvps->osd_traverse[osd_index];
		mvb = mvt->path[combination[i]];

		for (j = 0; j < MESON_MAX_BLOCKS; j++) {
			block = mvb[j];
			if (!block)
				break;
			mvps->enable_blocks |= BIT(block->id);
		}
	}
}

/**
 * combinate_layer_path - combinate every found layer path
 * @path_num_array: the number of every layer's found path
 * @num_planes: the number of layer
 *
 * use combination algorithm to check whether the path is valid
 *
 * RETURNS:
 * 0 for the valid path or -1 for the invalid path
 */
int combinate_layer_path(int *path_num_array, int num_planes,
				struct meson_vpu_pipeline_state *mvps,
					struct drm_atomic_state *state)
{
	int i, j, ret;
	bool is_continue = false;
	int combination[MESON_MAX_OSDS] = {0};

	i = 0;
	ret = -1;

	do {
		// sum the combination result to check osd blend block
		ret = vpu_pipeline_check_block(combination,
				num_planes, mvps, state);
		if (!ret)
			break;

		i++;
		combination[num_planes-1] = i;

		for (j = num_planes - 1; j >= 0; j--) {
			if (combination[j] >= path_num_array[j]) {
				combination[j] = 0;
				i = 0;
				if ((j - 1) >= 0)
					combination[j - 1] =
						combination[j - 1] + 1;
			}
		}

		is_continue = false;

		for (j = 0; j < num_planes; j++) {
			if (combination[j] != 0)
				is_continue = true;
		}
	} while (is_continue);
	if (!ret)
		vpu_pipeline_enable_block(combination, num_planes, mvps);
	return ret;
}

/**
 * find every layer's path(from start block to the end block) through
 * pipeline_dfs, combinate every found path of every layer
 * and check whether the combination is a valid path
 * that can meet the requirement of hardware limites.
 *
 * RETURNS:
 * 0 for the valid path or -1 for the invalid path
 */
int vpu_pipeline_traverse(struct meson_vpu_pipeline_state *mvps,
					struct drm_atomic_state *state)
{
	int i, osd_index, ret;
	int num_planes;
	struct meson_vpu_block *start, *end;
	int path[MESON_MAX_OSDS] = {0};
	struct meson_vpu_pipeline *mvp = mvps->pipeline;

	end = &mvp->postblend->base;
	num_planes = mvps->num_plane;

	if (!num_planes)
		return 0;

	DRM_DEBUG("traverse num: %d  %p.\n", num_planes, mvps);
	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (!mvps->plane_info[i].enable)
			continue;
		osd_index = mvps->plane_index[i];
		start = &mvp->osds[osd_index]->base;
		DRM_DEBUG("do pipeline_dfs: OSD%d.\n", (osd_index + 1));
		pipeline_dfs(osd_index, mvps, start, end, state);
	}

	// start to combination every layer case
	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (!mvps->plane_info[i].enable)
			continue;
		osd_index = mvps->plane_index[i];
		path[i] = mvps->osd_traverse[osd_index].num_path;
		DRM_DEBUG("osd%d traverse path num: %d\n",
			(osd_index + 1), path[i]);
	}

	ret = combinate_layer_path(path, num_planes, mvps, state);
	if (ret)
		DRM_ERROR("can't find a valid path.\n");

	return ret;
}
