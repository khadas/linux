// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <dt-bindings/display/meson-drm-ids.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include "meson_vpu_pipeline.h"
#include "meson_drv.h"
#include "meson_crtc.h"
#include "meson_plane.h"

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
			//DRM_DEBUG("edges is not active.\n");
			continue;
		}
		if (mvbl->edges_visited) {
			//DRM_DEBUG("edges is already visited.\n");
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
	MESON_DRM_TRAVERSE("start->id=%d,name=%s\n", start->id, start->name);
	MESON_DRM_TRAVERSE("end->id=%d,name=%s\n", end->id, end->name);

	while (mvs->top) {
		if (mvs->stack[mvs->top - 1] == end) {
			MESON_DRM_TRAVERSE("######path%d######\n", mvt->num_path);
			for (i = 0; i < mvs->top; i++) {
				mvt->path[j][i] = mvs->stack[i];
				MESON_DRM_TRAVERSE("%s->\n", mvs->stack[i]->name);
			}
			j++;
			mvt->num_path++;
			MESON_DRM_TRAVERSE("\n");
			prev = stack_pop(mvs);
			prev_state = meson_vpu_block_get_state(prev, state);
			prev_state->in_stack = 0;
		} else {
			curr = mvs->stack[mvs->top - 1];
			curr_state = meson_vpu_block_get_state(curr, state);
			next = neighbour(curr_state, &index, state);

			if (next) {
				MESON_DRM_TRAVERSE("next->id=%d,name=%s\n",
					  next->id, next->name);
				curr_state->outputs[index].edges_visited = 1;
				next_state =
					meson_vpu_block_get_state(next, state);
				stack_push(mvs, next);
				next_state->in_stack = 1;
			} else {
			//	DRM_DEBUG("next is NULL!!\n");
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

static u8 find_in_port(struct meson_vpu_block *in, struct meson_vpu_block *out)
{
	int i;
	struct meson_vpu_block_link *mvbl;

	for (i = 0; i < in->avail_outputs; i++) {
		mvbl = &in->outputs[i];
		if (mvbl->link == out)
			return mvbl->port;
	}
	return 0;
}

void vpu_pipeline_scaler_scope_size_calc(u8 index, u8 osd_index,
					 struct meson_vpu_pipeline_state *mvps)
{
	u8 m, i;
	u32 ratio_w_num, ratio_w_den;
	u32 ratio_h_num, ratio_h_den;
	struct meson_vpu_scaler_param *scaler_param, *scaler_param_1;

	i = index;
	if (mvps->scaler_cnt[i] == 0) {
		/*scope size calc*/
		mvps->osd_scope_pre[osd_index].h_start =
			mvps->plane_info[osd_index].dst_x;
		mvps->osd_scope_pre[osd_index].v_start =
			mvps->plane_info[osd_index].dst_y;
		mvps->osd_scope_pre[osd_index].h_end =
			mvps->osd_scope_pre[osd_index].h_start
			+ mvps->plane_info[osd_index].dst_w - 1;
		mvps->osd_scope_pre[osd_index].v_end =
			mvps->osd_scope_pre[osd_index].v_start
			+ mvps->plane_info[osd_index].dst_h - 1;
	} else if (mvps->scaler_cnt[i] == 1) {
		m = mvps->scale_blk[i][0]->index;
		scaler_param = &mvps->scaler_param[m];

		/*save scaler ratio*/
		scaler_param->ratio_w_num = mvps->plane_info[osd_index].src_w;
		scaler_param->ratio_w_den = mvps->plane_info[osd_index].dst_w;
		scaler_param->ratio_h_num = mvps->plane_info[osd_index].src_h;
		scaler_param->ratio_h_den = mvps->plane_info[osd_index].dst_h;
		scaler_param->calc_done_mask |=
			SCALER_RATIO_X_CALC_DONE |
			SCALER_RATIO_Y_CALC_DONE;

		if (scaler_param->before_osdblend) {
			/*scale size follow buffer size*/
			scaler_param->input_width =
				mvps->plane_info[osd_index].src_w;
			scaler_param->input_height =
				mvps->plane_info[osd_index].src_h;
			scaler_param->output_width =
				mvps->plane_info[osd_index].dst_w;
			scaler_param->output_height =
				mvps->plane_info[osd_index].dst_h;
			/*osdblend scope size calc*/
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
		} else {/*scaler position is after osd land*/
			mvps->osd_scope_pre[osd_index].h_start =
				mvps->plane_info[osd_index].dst_x *
				scaler_param->ratio_w_num /
				scaler_param->ratio_w_den;
			mvps->osd_scope_pre[osd_index].v_start =
				mvps->plane_info[osd_index].dst_y *
				scaler_param->ratio_h_num /
				scaler_param->ratio_h_den;
			mvps->osd_scope_pre[osd_index].h_end =
				mvps->osd_scope_pre[osd_index].h_start +
				mvps->plane_info[osd_index].src_w - 1;
			mvps->osd_scope_pre[osd_index].v_end =
				mvps->osd_scope_pre[osd_index].v_start +
				mvps->plane_info[osd_index].src_h - 1;

			/*scale size follow blend scope.*/
			scaler_param->input_width =
				mvps->osd_scope_pre[osd_index].h_end + 1;
			scaler_param->input_height =
				mvps->osd_scope_pre[osd_index].v_end + 1;
			scaler_param->output_width =
				mvps->plane_info[osd_index].dst_w +
				mvps->plane_info[osd_index].dst_x;
			scaler_param->output_height =
				mvps->plane_info[osd_index].dst_h +
				mvps->plane_info[osd_index].dst_y;
		}

		scaler_param->calc_done_mask |=
			SCALER_IN_W_CALC_DONE |
			SCALER_IN_H_CALC_DONE |
			SCALER_OUT_W_CALC_DONE |
			SCALER_OUT_H_CALC_DONE;

		DRM_DEBUG("Scaler [%d-%d] of osd [%d]  = [%d/%d,%d/%d], scope[%d,%d-%d,%d]\n",
			m, mvps->scaler_cnt[i], osd_index,
			scaler_param->ratio_w_num, scaler_param->ratio_w_den,
			scaler_param->ratio_h_num, scaler_param->ratio_h_den,
			mvps->osd_scope_pre[osd_index].h_start,
			mvps->osd_scope_pre[osd_index].h_end,
			mvps->osd_scope_pre[osd_index].v_start,
			mvps->osd_scope_pre[osd_index].v_end);
	} else if (mvps->scaler_cnt[i] == 2) {
		m = mvps->scale_blk[i][1]->index;
		scaler_param_1 = &mvps->scaler_param[m];
		m = mvps->scale_blk[i][0]->index;
		scaler_param = &mvps->scaler_param[m];

		if (scaler_param_1->calc_done_mask &
			SCALER_RATIO_X_CALC_DONE) {/*TODO*/
			ratio_w_num = scaler_param_1->ratio_w_num;
			ratio_w_den = scaler_param_1->ratio_w_den;
			ratio_h_num = scaler_param_1->ratio_h_num;
			ratio_h_den = scaler_param_1->ratio_h_den;
			/*recheck scaler size*/
		} else {/*TODO*/
			DRM_DEBUG("Global scaler not set, use default 1:1 config.\n");
			ratio_w_num = RATIO_BASE;
			ratio_w_den = RATIO_BASE;
			ratio_h_num = RATIO_BASE;
			ratio_h_den = RATIO_BASE;
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
				ratio_w_num / ratio_w_den;
			scaler_param->output_height =
				mvps->plane_info[osd_index].dst_h *
				ratio_h_num / ratio_h_den;
			scaler_param->calc_done_mask |=
				SCALER_IN_W_CALC_DONE |
				SCALER_IN_H_CALC_DONE |
				SCALER_OUT_W_CALC_DONE |
				SCALER_OUT_H_CALC_DONE;

			/*scope size calc*/
			mvps->osd_scope_pre[osd_index].h_start =
				mvps->plane_info[osd_index].dst_x *
				ratio_w_num / ratio_w_den;
			mvps->osd_scope_pre[osd_index].v_start =
				mvps->plane_info[osd_index].dst_y *
				ratio_h_num / ratio_h_den;
			mvps->osd_scope_pre[osd_index].h_end =
				mvps->osd_scope_pre[osd_index].h_start
				+ scaler_param->output_width - 1;
			mvps->osd_scope_pre[osd_index].v_end =
				mvps->osd_scope_pre[osd_index].v_start
				+ scaler_param->output_height - 1;
		} else {
			/*TODO*/
			DRM_ERROR("two scaler after blend?!\n");
		}
		/*reclac second scaler size, it may changed according to blend output.*/
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

		DRM_DEBUG("Scaler [%d-%d] of osd [%d]  = [%d/%d,%d/%d], scope[%d,%d-%d,%d]\n",
			m, mvps->scaler_cnt[i], osd_index,
			scaler_param->input_width, scaler_param->output_width,
			scaler_param->input_height, scaler_param->output_height,
			mvps->osd_scope_pre[osd_index].h_start,
			mvps->osd_scope_pre[osd_index].h_end,
			mvps->osd_scope_pre[osd_index].v_start,
			mvps->osd_scope_pre[osd_index].v_end);

		DRM_DEBUG("Scaler Global ratio[%d/%d-%d/%d], input [%d/%d,%d/%d],\n",
			scaler_param_1->ratio_w_num, scaler_param_1->ratio_w_den,
			scaler_param_1->ratio_h_num, scaler_param_1->ratio_h_den,
			scaler_param_1->input_width, scaler_param_1->output_width,
			scaler_param_1->input_height, scaler_param_1->output_height);
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
					have_blend ? 0 : 1;
			}
		}

		if (mvps->scaler_cnt[i] == 0) {
			vpu_pipeline_scaler_scope_size_calc(i,
							    osd_index, mvps);
		} else if (mvps->scaler_cnt[i] == 1) {
			vpu_pipeline_scaler_scope_size_calc(i,
							    osd_index, mvps);
		} else if (mvps->scaler_cnt[i] == 2) {
			vpu_pipeline_scaler_scope_size_calc(i,
							    osd_index, mvps);
			/*
			 *filter unsuitable scaler setting:
			 *1. NO: scale down -> scale up
			 *
			 */
			m = mvps->scale_blk[i][1]->index;
			scaler_param_1 = &mvps->scaler_param[m];
			m = mvps->scale_blk[i][0]->index;
			scaler_param = &mvps->scaler_param[m];

			/* TODO
			if ((scaler_param->input_width > scaler_param->output_width &&
				scaler_param_1->ratio_w_num < scaler_param_1->ratio_w_den) ||
				(scaler_param->input_height > scaler_param->output_height &&
				scaler_param_1->ratio_h_num < scaler_param_1->ratio_h_den)) {
				DRM_ERROR("Bad scaler setting:scale down -> up\n");
				ret = -1;
				break;
			}
			*/
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
	u8 in_port, out_port;
	int i, j, osd_index, ret;
	struct meson_vpu_traverse *mvt;
	struct meson_vpu_block **mvb;
	struct meson_vpu_block *block;
	struct meson_vpu_block *osdblend;
	struct meson_vpu_block_state *mvbs;

	osdblend = &mvps->pipeline->osdblend->base;
	ret = vpu_pipeline_scaler_check(combination, num_planes, mvps);
	if (ret) {
		DRM_DEBUG("%s check scaler failed\n", __func__);
		return -1;
	}

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
					find_out_port(block, mvb[j + 1]);
				MESON_DRM_TRAVERSE("osd-%d blend out port: %d.\n",
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

			if (block->type == MESON_BLK_AFBC) {
				in_port = find_in_port(mvb[j - 1], block);
				out_port = find_out_port(block, mvb[j + 1]);
				if (in_port != out_port) {
					MESON_DRM_TRAVERSE("afbc prev next not match.\n");
					return -1;
				}
			}

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

int s5_check_pipeline_path(int *combination, int num_planes,
			   struct meson_vpu_pipeline_state *mvps,
			   struct drm_atomic_state *state)
{
	int more_4k;
	int more_60;
	int i, j, osd_index, ret;
	struct meson_vpu_block_state *mvbs;
	struct meson_vpu_traverse *mvt;
	struct meson_vpu_block **mvb;
	struct meson_vpu_block *block;
	bool have_blend, have_slice2ppc = 0;

	more_4k = mvps->sub_states[0].more_4k;
	more_60 = mvps->sub_states[0].more_60;

	for (i = 0; i < MESON_MAX_OSDS; i++) {
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
			if (block->type == MESON_BLK_SLICE2PPC)
				have_slice2ppc = 1;

			if (block->ops && block->ops->check_state) {
				mvbs = meson_vpu_block_get_state(block, state);
				ret = block->ops->check_state(block, mvbs,
							      mvps);

				if (ret) {
					DRM_ERROR("%s block check error.\n",
						  block->name);
					return ret;
				}
			}
		}

		if (more_4k && !have_blend)
			return -1;
		if (!more_60 && have_slice2ppc)
			return -1;
		if (mvps->plane_info[i].blend_bypass && have_blend)
			return -1;
	}

	return 0;
}

int t7_check_pipeline_path(int *combination, int num_planes,
			     struct meson_vpu_pipeline_state *mvps,
					struct drm_atomic_state *state)
{
	return vpu_pipeline_check_block(combination, num_planes, mvps, state);
}

int g12a_check_pipeline_path(int *combination, int num_planes,
			     struct meson_vpu_pipeline_state *mvps,
					struct drm_atomic_state *state)
{
	return vpu_pipeline_check_block(combination, num_planes, mvps, state);
}

int vpu_video_pipeline_check_block(struct meson_vpu_pipeline_state *mvps,
				   struct drm_atomic_state *state)
{
	int i, ret = 0;
	struct meson_vpu_block *block;
	struct meson_vpu_block_state *mvbs;

	DRM_DEBUG("mvps (%p), atomic-state(%p)\n", mvps, state);

	for (i = 0; i < MESON_MAX_VIDEO; i++) {
		if (!mvps->video_plane_info[i].enable)
			continue;
		block = &mvps->pipeline->video[i]->base;
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
	return ret;
}

void vpu_pipeline_enable_block(int *combination, int num_planes,
			       struct meson_vpu_pipeline_state *mvps)
{
	int i, j, osd_index, crtc_index;
	struct meson_vpu_traverse *mvt;
	struct meson_vpu_block **mvb;
	struct meson_vpu_block *block;
	struct meson_vpu_block_state *mvbs;
	struct meson_vpu_sub_pipeline_state *sub_state;
	struct meson_vpu_pipeline *mvp = mvps->pipeline;

	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (!mvps->plane_info[i].enable)
			continue;
		osd_index = mvps->plane_index[i];
		crtc_index = mvps->plane_info[i].crtc_index;
		if (crtc_index == -EINVAL) {
			DRM_ERROR("%s, overbound crtc index\n", __func__);
			crtc_index = 0;
		}
		sub_state = &mvps->sub_states[crtc_index];
		mvt = &mvps->osd_traverse[osd_index];
		mvb = mvt->path[combination[i]];
		MESON_DRM_TRAVERSE("OSD%d select path%d\n",
			  (osd_index + 1), combination[i]);

		for (j = 0; j < MESON_MAX_BLOCKS; j++) {
			block = mvb[j];
			if (!block)
				break;

			mvbs = meson_vpu_block_get_state(block, mvps->obj.state);
			mvbs->sub = &mvp->subs[crtc_index];
			sub_state->enable_blocks |= BIT(block->id);
		}
	}
	/*TODO*/
	//for (i = 0; i < MESON_MAX_VIDEO; i++)
}

int s5_set_pipeline_para(int *combination, int num_planes,
			 struct meson_vpu_pipeline_state *mvps,
			 struct drm_atomic_state *state)
{
	int more_4k;
	int more_60;
	int i;
	u64 tmp;
	u32 osd_in_hsize_real, osd_out_hsize_real, osd_out_hsize_raw;
	u32 osd_pps_din_hsize[MESON_MAX_OSDS] = {0};
	u32 osd_pps_dout_hsize[MESON_MAX_OSDS] = {0};
	u32 osd_pps_din_vsize[MESON_MAX_OSDS] = {0};
	u32 osd_pps_dout_vsize[MESON_MAX_OSDS] = {0};
	u32 osd_slice_dout_hsize[MESON_MAX_OSDS] = {0};
	u32 hwincut_bgn[MESON_MAX_OSDS] = {0};
	u32 hwincut_end[MESON_MAX_OSDS] = {0};
	u32 init_phase[MESON_MAX_OSDS] = {0};
	u32 slice_st[MESON_MAX_OSDS] = {0};
	u32 horz_phase_step, slice1_init_phase_out;
	struct meson_vpu_sub_pipeline_state *mvsps;

	mvsps = &mvps->sub_states[0];
	more_4k = mvsps->more_4k;
	more_60 = mvsps->more_60;

	osd_in_hsize_real = 0;
	osd_out_hsize_real = 0;
	osd_out_hsize_raw = 0;

	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (!mvps->plane_info[i].enable)
			continue;

		if (more_60) {
			osd_in_hsize_real += mvps->plane_info[i].src_w;
			osd_out_hsize_raw += mvps->plane_info[i].dst_w;
		}

		osd_pps_din_hsize[i] = mvps->plane_info[i].src_w;
		osd_pps_dout_vsize[i] = mvps->plane_info[i].dst_h;
		osd_pps_din_vsize[i] = mvps->plane_info[i].src_h;
	}

	osd_out_hsize_real = ALIGN(osd_out_hsize_raw, 8);
	mvsps->slice2ppc_hsize = osd_out_hsize_real / 2;
	mvsps->slice2ppc_vsize = mvps->plane_info[OSD1_SLICE0].dst_h;
	mvsps->osd_out_hsize_raw = osd_out_hsize_raw;
	mvsps->osd_out_hsize_real = osd_out_hsize_real;

	for (i = 0; i < MESON_MAX_OSDS; i++) {
		memset(&mvps->osd_scope_pre[i], 0, sizeof(mvps->osd_scope_pre[i]));

		if (!mvps->plane_info[i].enable)
			continue;

		if (more_60) {
			if (!osd_out_hsize_raw) {
				DRM_ERROR("invalid osd_out_hsize_raw parameter\n");
				return -EINVAL;
			}
			horz_phase_step =
				(osd_in_hsize_real << 18) / osd_out_hsize_raw;
			horz_phase_step = (horz_phase_step << 6);
			osd_slice_dout_hsize[i] = osd_out_hsize_real / 2;

			if (osd_in_hsize_real == osd_out_hsize_raw) {
				if (i == OSD1_SLICE0) {
					hwincut_bgn[i] = 0;
					hwincut_end[i] =
						osd_slice_dout_hsize[i] - 1;
					osd_pps_dout_hsize[i] = osd_slice_dout_hsize[i] +
						SCALER_OVERLAP;
				} else if (i == OSD3_SLICE1) {
					hwincut_bgn[i] = SCALER_OVERLAP;
					hwincut_end[i] =
						osd_slice_dout_hsize[i] +
						SCALER_OVERLAP - 1;
					osd_pps_dout_hsize[i] = osd_out_hsize_raw -
						osd_slice_dout_hsize[i] + SCALER_OVERLAP;
				}
				osd_pps_din_hsize[i] = osd_pps_dout_hsize[i];
			} else {
				if (i == OSD1_SLICE0) {
					slice_st[i] = 0;
					osd_pps_din_hsize[i] =
						(((long long)(osd_slice_dout_hsize
								      [i]) *
						  (horz_phase_step >> 4) /
						  (1 << 20)) +
						 1) / 2 *
							2 +
						SCALER_OVERLAP;
					tmp = (u64)osd_pps_din_hsize[i] << 24;
					do_div(tmp, horz_phase_step);
					osd_pps_dout_hsize[i] = (tmp + 1) / 2 * 2;
					hwincut_bgn[i] = 0;
					hwincut_end[i] =
						osd_slice_dout_hsize[i] - 1;
				} else if (i == OSD3_SLICE1) {
					slice_st[i] =
						osd_pps_din_hsize[OSD1_SLICE0] -
						2 * SCALER_OVERLAP;
					tmp = (u64)(slice_st[i]) * (1 << 24);
					do_div(tmp, horz_phase_step);
					slice1_init_phase_out = tmp;

					osd_slice_dout_hsize[i] = osd_out_hsize_raw -
						osd_out_hsize_real / 2;
					osd_pps_din_hsize[i] =
						osd_in_hsize_real -
						osd_pps_din_hsize[0] +
						SCALER_OVERLAP * 2;
					osd_pps_dout_hsize[i] =
						osd_out_hsize_raw -
						slice1_init_phase_out;
					osd_pps_dout_hsize[i] = ALIGN(osd_pps_dout_hsize[i], 2);

					hwincut_bgn[i] =
						osd_pps_dout_hsize[i] -
						osd_slice_dout_hsize[i];
					hwincut_end[i] =
						osd_pps_dout_hsize[i] - 1;
				}
			}
		} else {
			osd_pps_din_hsize[i] = mvps->plane_info[i].src_w;
			osd_pps_din_vsize[i] = mvps->plane_info[i].src_h;
			slice_st[i] = 0;
			init_phase[i] = 0;
			if (more_4k) {
				osd_pps_dout_hsize[i] =
					mvps->plane_info[i].dst_w / 2;
				osd_pps_dout_vsize[i] =
					mvps->plane_info[i].dst_h / 2;
			} else {
				osd_pps_dout_hsize[i] =
					mvps->plane_info[i].dst_w;
				osd_pps_dout_vsize[i] =
					mvps->plane_info[i].dst_h;
			}
			//horz_phase_step = (osd_pps_din_hsize[i] << 18) /
			//		  osd_pps_dout_hsize[i];
		}

		mvsps->scaler_din_hsize[i] = osd_pps_din_hsize[i];
		mvsps->scaler_dout_hsize[i] = osd_pps_dout_hsize[i];
		mvsps->scaler_din_vsize[i] = osd_pps_din_vsize[i];
		mvsps->scaler_dout_vsize[i] = osd_pps_dout_vsize[i];
		mvps->scaler_param[i].input_width = osd_pps_din_hsize[i];
		mvps->scaler_param[i].input_height = osd_pps_din_vsize[i];
		mvps->scaler_param[i].output_width = osd_pps_dout_hsize[i];
		mvps->scaler_param[i].output_height = osd_pps_dout_vsize[i];

		if (more_60) {
			mvps->osd_scope_pre[i].h_start = mvps->plane_info[0].dst_x;
			mvps->osd_scope_pre[i].h_end = mvps->plane_info[0].dst_x +
						mvps->plane_info[0].dst_w * 2 - 1;
			mvps->osd_scope_pre[i].v_start = mvps->plane_info[0].dst_y;
			mvps->osd_scope_pre[i].v_end = mvps->plane_info[0].dst_y +
						mvps->plane_info[0].dst_h - 1;
		} else if (more_4k) {
			mvps->osd_scope_pre[i].h_start = mvps->plane_info[i].dst_x / 2;
			mvps->osd_scope_pre[i].h_end = mvps->plane_info[i].dst_x / 2 +
						osd_pps_dout_hsize[i] - 1;
			mvps->osd_scope_pre[i].v_start = mvps->plane_info[i].dst_y / 2;
			mvps->osd_scope_pre[i].v_end = mvps->plane_info[i].dst_y / 2 +
						osd_pps_dout_vsize[i] - 1;
		} else {
			mvps->osd_scope_pre[i].h_start = mvps->plane_info[i].dst_x;
			mvps->osd_scope_pre[i].h_end = mvps->plane_info[i].dst_x +
						osd_pps_dout_hsize[i] - 1;
			mvps->osd_scope_pre[i].v_start = mvps->plane_info[i].dst_y;
			mvps->osd_scope_pre[i].v_end = mvps->plane_info[i].dst_y +
						osd_pps_dout_vsize[i] - 1;
		}
		mvsps->slice_dout_hsize[i] = osd_slice_dout_hsize[i];
		mvsps->hwincut_bgn[i] = hwincut_bgn[i];
		mvsps->hwincut_end[i] = hwincut_end[i];
		mvsps->slice_x_st[i] = slice_st[i];
		mvsps->slice_x_end[i] = 0;
		mvsps->init_phase[i] = init_phase[0];
		mvsps->slice2ppc_hsize = osd_out_hsize_real / 2;
		DRM_DEBUG("%s, scaler_d:%u, %u, %u, %u, %d\n", __func__, mvsps->scaler_din_hsize[i],
			  mvsps->scaler_dout_hsize[i], mvsps->scaler_din_vsize[i],
			  mvsps->scaler_dout_vsize[i], i);
		DRM_DEBUG("%s, scaler_p:%u, %u, %u, %u\n", __func__,
			  mvps->scaler_param[i].input_width,
			  mvps->scaler_param[i].input_height, mvps->scaler_param[i].output_width,
			  mvps->scaler_param[i].output_height);
		DRM_DEBUG("%s, osd_scope_pre:%u, %u\n", __func__, mvps->osd_scope_pre[i].h_end,
			  mvps->osd_scope_pre[i].v_end);
	}

	vpu_pipeline_enable_block(combination, num_planes, mvps);
	return 0;
}

int t7_set_pipeline_para(int *combination, int num_planes,
			     struct meson_vpu_pipeline_state *mvps,
					struct drm_atomic_state *state)
{
	vpu_pipeline_enable_block(combination, num_planes, mvps);
	return 0;
}

int g12a_set_pipeline_para(int *combination, int num_planes,
			     struct meson_vpu_pipeline_state *mvps,
					struct drm_atomic_state *state)
{
	vpu_pipeline_enable_block(combination, num_planes, mvps);
	return 0;
}

void vpu_pipeline_clean_block(int *combination, int num_planes,
			      struct meson_vpu_pipeline_state *mvps,
			      struct drm_atomic_state *state)
{
	int i, j, osd_index;
	struct meson_vpu_traverse *mvt;
	struct meson_vpu_block **mvb;
	struct meson_vpu_block *block;
	struct meson_vpu_block_state *mvbs;

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
				mvbs->checked = 0;
			}
		}
	}
	/*clean video wrapper block*/
	for (i = 0; i < MESON_MAX_VIDEO; i++) {
		if (!mvps->video_plane_info[i].enable)
			continue;
		block = &mvps->pipeline->video[i]->base;
		if (block->ops && block->ops->check_state) {
			mvbs = meson_vpu_block_get_state(block, state);
			mvbs->checked = 0;
		}
	}
}

/**
 * combine_layer_path - combine every found layer path
 * @path_num_array: the number of every layer's found path
 * @num_planes: the number of layer
 *
 * use combination algorithm to check whether the path is valid
 *
 * RETURNS:
 * 0 for the valid path or -1 for the invalid path
 */
int combine_layer_path(int *path_num_array, int num_planes,
			 struct meson_vpu_pipeline_state *mvps,
					struct drm_atomic_state *state)
{
	int i, j, k, ret, index;
	bool is_continue = false;
	/*comb of osd path index or each osd.*/
	int combination[MESON_MAX_OSDS] = {0};
	struct meson_vpu_pipeline_ops *ops = mvps->pipeline->ops;

	i = 0;
	ret = -1;

	for (index = MESON_MAX_OSDS - 1; index >= 0; index--) {
		if (mvps->plane_info[index].enable)
			break;
	}

	do {
		MESON_DRM_TRAVERSE("Comb check [%d-%d-%d-%d]\n",
			combination[0], combination[1], combination[2], combination[3]);
		// sum the combination result to check osd blend block
		ret = ops->check_pipeline_path(combination, num_planes, mvps,
					       state);
		if (!ret || ret == -EINVAL)
			break;
		vpu_pipeline_clean_block(combination, num_planes, mvps, state);
		i++;
		combination[index] = i;

		for (j = index; j >= 0; j--) {
			if (combination[j] >= path_num_array[j]) {
				combination[j] = 0;
				i = 0;

				for (k = j - 1; k >= 0; k--) {
					if (mvps->plane_info[k].enable)
						break;
				}

				if (k >= 0)
					combination[k] =
						combination[k] + 1;
			}
		}

		is_continue = false;

		for (j = 0; j < MESON_MAX_OSDS; j++) {
			if (combination[j] != 0 &&
			    mvps->plane_info[j].enable) {
				is_continue = true;
				break;
			}
		}
	} while (is_continue);
	if (!ret)
		ops->set_pipeline_para(combination, num_planes, mvps, state);

	return ret;
}

/**
 * find every layer's path(from start block to the end block) through
 * pipeline_dfs, combine every found path of every layer
 * and check whether the combination is a valid path
 * that can meet the requirement of hardware limites.
 *
 * RETURNS:
 * 0 for the valid path or -1 for the invalid path
 */
int vpu_pipeline_traverse(struct meson_vpu_pipeline_state *mvps,
			  struct drm_atomic_state *state)
{
	int i, osd_index, crtc_index, ret;
	int num_planes;
	struct meson_vpu_block *start, *end;
	int path[MESON_MAX_OSDS] = {0};
	struct meson_vpu_pipeline *mvp = mvps->pipeline;
	struct meson_vpu_block *mvb;
	struct meson_vpu_block_state *mvbs;

	/* to dup new state for all blocks.The duped state is empty/disable state.
	 */
	for (i = 0; i < BLOCK_ID_MAX; i++) {
		mvb = mvp->mvbs[i];

		if (!mvb)
			continue;

		mvbs = meson_vpu_block_get_state(mvb, state);
	}

	num_planes = mvps->num_plane;
	if (!num_planes)
		return 0;

	MESON_DRM_TRAVERSE("====> traverse start num: %d  %p.\n", num_planes, mvps);
	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (!mvps->plane_info[i].enable)
			continue;
		osd_index = mvps->plane_index[i];
		crtc_index = mvps->plane_info[i].crtc_index;
		if (crtc_index == -EINVAL) {
			DRM_ERROR("%s, overbound crtc index\n", __func__);
			crtc_index = 0;
		}

		start = &mvp->osds[osd_index]->base;
		end = &mvp->postblends[crtc_index]->base;
		MESON_DRM_TRAVERSE("do pipeline_dfs: OSD%d.\n", (osd_index + 1));
		pipeline_dfs(osd_index, mvps, start, end, state);
	}

	MESON_DRM_TRAVERSE("==>pipeline_dfs end, start combine.\n");
	// start to combination every layer case
	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (!mvps->plane_info[i].enable)
			continue;
		osd_index = mvps->plane_index[i];
		path[i] = mvps->osd_traverse[osd_index].num_path;
		MESON_DRM_TRAVERSE("osd%d traverse path num: %d\n",
			  (osd_index + 1), path[i]);
	}

	ret = combine_layer_path(path, num_planes, mvps, state);
	if (ret)
		DRM_ERROR("can't find a valid path.\n");

	MESON_DRM_TRAVERSE("====> traverse end\n");
	return ret;
}

struct meson_vpu_pipeline_ops g12a_vpu_pipeline_ops = {
	.check_pipeline_path = g12a_check_pipeline_path,
	.set_pipeline_para = g12a_set_pipeline_para,
};

struct meson_vpu_pipeline_ops t7_vpu_pipeline_ops = {
	.check_pipeline_path = t7_check_pipeline_path,
	.set_pipeline_para = t7_set_pipeline_para,
};

struct meson_vpu_pipeline_ops s5_vpu_pipeline_ops = {
	.check_pipeline_path = s5_check_pipeline_path,
	.set_pipeline_para = s5_set_pipeline_para,
};

