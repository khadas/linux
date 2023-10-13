// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <dt-bindings/display/meson-drm-ids.h>

#include "vpu-hw/meson_osd_afbc.h"
#include "meson_vpu_pipeline.h"
#include "meson_drv.h"
#include "meson_vpu.h"

static int flush_time = 3;
module_param(flush_time, int, 0664);
MODULE_PARM_DESC(flush_time, "flush time");

static int osd_slice_mode;
module_param(osd_slice_mode, int, 0664);
MODULE_PARM_DESC(osd_slice_mode, "osd_slice_mode");

#define MAX_LINKS 5
#define MAX_PORTS 6
#define MAX_PORT_ID 32

static struct meson_rdma_done_detect drm_rdma_cnt[MESON_MAX_CRTC] = {
	{VIU_OSD2_TCOLOR_AG1, 0},
	{VIU_OSD2_TCOLOR_AG2, 0},
	{VIU_OSD2_TCOLOR_AG3, 0},
};
static struct meson_vpu_block **vpu_blocks;

struct meson_vpu_link_para {
	u8 id;
	u8 port;
};

struct meson_vpu_block_para {
	char *name;
	u8 id;
	u8 index;
	u8 type;
	u8 num_inputs;
	u8 num_outputs;
	struct meson_vpu_link_para inputs[MESON_BLOCK_MAX_INPUTS];
	struct meson_vpu_link_para outputs[MESON_BLOCK_MAX_OUTPUTS];
	u64 inputs_mask;
	u64 outputs_mask;
};

static struct meson_vpu_block *create_block(size_t blk_sz,
					    struct meson_vpu_block_para *para,
					    struct meson_vpu_block_ops *ops,
	struct meson_vpu_pipeline *pipeline)
{
	struct meson_vpu_block *mvb;
	int i;

	mvb = kzalloc(blk_sz, GFP_KERNEL);
	if (!mvb)
		return NULL;

	snprintf(mvb->name, MESON_BLOCK_MAX_NAME_LEN, "%s", para->name);
	mvb->id = para->id;
	mvb->type = para->type;
	mvb->index = para->index;
	mvb->avail_inputs = para->num_inputs;
	mvb->avail_outputs = para->num_outputs;
	mvb->max_inputs = para->num_inputs * 2;
	mvb->max_outputs = para->num_outputs * 2;
	mvb->inputs_mask = para->inputs_mask;
	mvb->outputs_mask = para->outputs_mask;

	for (i = 0; i < mvb->avail_inputs; i++) {
		mvb->inputs[i].port = para->inputs[i].port;
		mvb->inputs[i].id = para->inputs[i].id;
		mvb->inputs[i].edges_active = 1;
		mvb->inputs[i].edges_visited = 0;
	}

	for (i = 0; i < mvb->avail_outputs; i++) {
		mvb->outputs[i].port = para->outputs[i].port;
		mvb->outputs[i].id = para->outputs[i].id;
		mvb->outputs[i].edges_active = 1;
		mvb->outputs[i].edges_visited = 0;
	}

	mvb->ops = ops;
	mvb->pipeline = pipeline;

	return mvb;
}

static void parse_vpu_node(struct device_node *child_node,
			   struct meson_vpu_block_para *para)
{
	int i, j, ret, size;
	u8 id;
	struct device_node *link;
	const __be32 *list, *phandle;
	u64 in_mask, out_mask;

	in_mask = 0;
	out_mask = 0;
	ret = of_property_read_u8(child_node, "id", &para->id);
	if (ret)
		para->id = 0;
	ret = of_property_read_u8(child_node, "index", &para->index);
	if (ret)
		para->index = 0;
	ret = of_property_read_u8(child_node, "type", &para->type);
	if (ret)
		para->type = 0;
	ret = of_property_read_u8(child_node, "num_in_links",
				  &para->num_inputs);
	if (ret)
		para->num_inputs = 0;
	ret = of_property_read_u8(child_node, "num_out_links",
				  &para->num_outputs);
	if (ret)
		para->num_outputs = 0;
	ret = of_property_read_string(child_node, "block_name",
				      (const char **)&para->name);
	if (ret)
		para->name = NULL;

	list = of_get_property(child_node, "in_links", &size);
	if (list) {
		size /= sizeof(*list);
		if (!size || size % 2) {
			DRM_ERROR("wrong in_links config\n");
			//return -EINVAL;
		}
		for (i = 0, j = 0; i < size; i += 2, j++) {
			para->inputs[j].port = be32_to_cpu(*list++);
			phandle = list++;
			link = of_find_node_by_phandle(be32_to_cpup(phandle));
			of_property_read_u8(link, "id", &id);
			if (id >= MESON_MAX_BLOCKS)
				continue;
			para->inputs[j].id = id;
			in_mask |= (u64)1 << id;
		}
		para->inputs_mask = in_mask;
	}

	list = of_get_property(child_node, "out_links", &size);
	if (list) {
		size /= sizeof(*list);
		if (!size || size % 2) {
			DRM_ERROR("wrong out_links config\n");
			//return -EINVAL;
		}
		for (i = 0, j = 0; i < size; i += 2, j++) {
			para->outputs[j].port = be32_to_cpu(*list++);
			phandle = list++;
			link = of_find_node_by_phandle(be32_to_cpup(phandle));
			of_property_read_u8(link, "id", &id);
			if (id >= MESON_MAX_BLOCKS)
				continue;
			para->outputs[j].id = id;
			out_mask |= (u64)1 << id;
		}
		para->outputs_mask = out_mask;
	}
	DRM_DEBUG("id=%d,index=%d,num_in_links=%d,num_out_links=%d\n",
		 para->id, para->index,
		para->num_inputs, para->num_outputs);
	DRM_DEBUG("in_mask=0x%llx,out_mask=0x%llx\n", in_mask, out_mask);
}

static struct meson_vpu_block *
meson_vpu_create_block(struct meson_vpu_block_para *para,
		       struct meson_vpu_pipeline *pipeline)
{
	struct meson_vpu_block *mvb;
	struct meson_vpu_block_ops *ops;
	size_t blk_size;

	switch (para->type) {
	case MESON_BLK_OSD:
		blk_size = sizeof(struct meson_vpu_osd);
		if (pipeline->priv && pipeline->priv->vpu_data)
			ops = pipeline->priv->vpu_data->osd_ops;
		else
			ops = &osd_ops;

		mvb = create_block(blk_size, para, ops, pipeline);

		pipeline->osds[mvb->index] = to_osd_block(mvb);
		pipeline->num_osds++;
		break;
	case MESON_BLK_AFBC:
		blk_size = sizeof(struct meson_vpu_afbc);
		if (pipeline->priv && pipeline->priv->vpu_data)
			ops = pipeline->priv->vpu_data->afbc_ops;
		else
			ops = &afbc_ops;

		mvb = create_block(blk_size, para, ops, pipeline);

		pipeline->afbc_osds[mvb->index] = to_afbc_block(mvb);
		pipeline->num_afbc_osds++;
		break;
	case MESON_BLK_SCALER:
		blk_size = sizeof(struct meson_vpu_scaler);
		if (pipeline->priv && pipeline->priv->vpu_data)
			ops = pipeline->priv->vpu_data->scaler_ops;
		else
			ops = &scaler_ops;

		mvb = create_block(blk_size, para, ops, pipeline);

		pipeline->scalers[mvb->index] = to_scaler_block(mvb);
		pipeline->num_scalers++;
		break;
	case MESON_BLK_OSDBLEND:
		blk_size = sizeof(struct meson_vpu_osdblend);
		if (pipeline->priv && pipeline->priv->vpu_data)
			ops = pipeline->priv->vpu_data->osdblend_ops;
		else
			ops = &osdblend_ops;

		mvb = create_block(blk_size, para, ops, pipeline);

		pipeline->osdblend = to_osdblend_block(mvb);
		break;
	case MESON_BLK_HDR:
		blk_size = sizeof(struct meson_vpu_hdr);
		if (pipeline->priv && pipeline->priv->vpu_data)
			ops = pipeline->priv->vpu_data->hdr_ops;
		else
			ops = &hdr_ops;

		mvb = create_block(blk_size, para, ops, pipeline);

		pipeline->hdrs[mvb->index] = to_hdr_block(mvb);
		pipeline->num_hdrs++;
		break;
	case MESON_BLK_DOVI:
		blk_size = sizeof(struct meson_vpu_db);
		if (pipeline->priv && pipeline->priv->vpu_data)
			ops = pipeline->priv->vpu_data->dv_ops;
		else
			ops = &db_ops;

		mvb = create_block(blk_size, para, ops, pipeline);

		pipeline->dbs[mvb->index] = to_db_block(mvb);
		pipeline->num_dbs++;
		break;
	case MESON_BLK_VPPBLEND:
		blk_size = sizeof(struct meson_vpu_postblend);
		if (pipeline->priv && pipeline->priv->vpu_data)
			ops = pipeline->priv->vpu_data->postblend_ops;
		else
			ops = &postblend_ops;

		mvb = create_block(blk_size, para, ops, pipeline);

		pipeline->postblends[mvb->index] = to_postblend_block(mvb);
		pipeline->num_postblend++;
		break;
	case MESON_BLK_SLICE2PPC:
		blk_size = sizeof(struct meson_vpu_slice2ppc);
		if (pipeline->priv && pipeline->priv->vpu_data &&
		    pipeline->priv->vpu_data->slice2ppc_ops)
			ops = pipeline->priv->vpu_data->slice2ppc_ops;
		else
			ops = &slice2ppc_ops;

		mvb = create_block(blk_size, para, ops, pipeline);
		pipeline->slice2ppc = to_slice2ppc_block(mvb);
		break;
	case MESON_BLK_VIDEO:
		blk_size = sizeof(struct meson_vpu_video);
		if (pipeline->priv && pipeline->priv->vpu_data)
			ops = pipeline->priv->vpu_data->video_ops;
		else
			ops = &video_ops;

		mvb = create_block(blk_size, para, ops, pipeline);

		pipeline->video[mvb->index] = to_video_block(mvb);
		pipeline->num_video++;
		break;
	default:
		return NULL;
	}

	return mvb;
}

static void populate_block_link(void)
{
	int i, j, id;
	struct meson_vpu_block *mvb;

	for (i = 0; i < BLOCK_ID_MAX; i++) {
		mvb = vpu_blocks[i];

		if (!mvb)
			continue;

		for (j = 0; j < mvb->avail_inputs; j++) {
			id = mvb->inputs[j].id;
			mvb->inputs[j].link = vpu_blocks[id];
		}

		for (j = 0; j < mvb->avail_outputs; j++) {
			id = mvb->outputs[j].id;
			mvb->outputs[j].link = vpu_blocks[id];
		}
	}
}

static int populate_vpu_pipeline(struct device_node *vpu_block_node,
				 struct meson_vpu_pipeline *pipeline)
{
	int i;
	struct device_node *child_node;
	struct meson_vpu_block *mvb;
	struct meson_vpu_block_para para;
	u32 num_blocks;

	num_blocks = of_get_child_count(vpu_block_node);
	if (num_blocks <= 0)
		return -ENODEV;

	vpu_blocks = kcalloc(BLOCK_ID_MAX, sizeof(*vpu_blocks), GFP_KERNEL);
	if (!vpu_blocks)
		return -ENOMEM;

	for_each_child_of_node(vpu_block_node, child_node) {
		parse_vpu_node(child_node, &para);

		mvb = meson_vpu_create_block(&para, pipeline);

		if (!mvb)
			return -ENOMEM;
		vpu_blocks[mvb->id] = mvb;
	}
	pipeline->mvbs = vpu_blocks;
	pipeline->num_blocks = num_blocks;

	populate_block_link();

	for (i = 0; i < MESON_MAX_CRTC; i++) {
		if (i < pipeline->num_postblend) {
			pipeline->subs[i].index = i;
			pipeline->subs[i].pipeline = pipeline;
		} else {
			pipeline->subs[i].index = -1;
			pipeline->subs[i].pipeline = NULL;
		}
	}

	return 0;
}

void VPU_PIPELINE_HW_INIT(struct meson_vpu_block *mvb)
{
	if (mvb->ops->init)
		mvb->ops->init(mvb);
}

void VPU_PIPELINE_HW_FINI(struct meson_vpu_block *mvb)
{
	if (mvb->ops->fini)
		mvb->ops->fini(mvb);
}

static void vpu_pipeline_planes_calc(struct meson_vpu_pipeline *pipeline,
				     struct meson_vpu_pipeline_state *mvps)
{
	u8 i;
	int crtc_index;
	struct meson_vpu_sub_pipeline_state *mvsps;

	mvps->num_plane = 0;
	mvps->num_plane_video = 0;

	for (i = 0; i < MESON_MAX_CRTC; i++) {
		mvsps = &mvps->sub_states[i];
		mvsps->enable_blocks = 0;
		if (osd_slice_mode)
			mvsps->more_60 = 1;
	}

	for (i = 0; i < MESON_MAX_OSDS; i++) {
		crtc_index = mvps->plane_info[i].crtc_index;
		if (!mvps->sub_states[crtc_index].more_60) {
			if (mvps->plane_info[i].enable) {
				if (mvps->plane_info[i].src_w >
					MESON_OSD_INPUT_W_LIMIT ||
					mvps->plane_info[i].dst_w == 0) {
					mvps->plane_info[i].enable = 0;
					continue;
				}
				DRM_DEBUG("osdplane [%d] enable:(%d-%llx, %d-%d)\n",
						mvps->plane_info[i].plane_index,
						mvps->plane_info[i].zorder,
						mvps->plane_info[i].phy_addr,
						mvps->plane_info[i].dst_w,
						mvps->plane_info[i].dst_h);
				mvps->num_plane++;
			} else {
				DRM_DEBUG("osdplane index [%d] disable.\n", i);
			}
		} else {
			if (i == OSD1_SLICE0 && mvps->plane_info[i].enable) {
				mvps->plane_info[i].src_w = mvps->plane_info[i].src_w / 2;
				mvps->plane_info[i].dst_w = mvps->plane_info[i].dst_w / 2;
				mvps->num_plane++;
			} else if (i == OSD3_SLICE1) {
				memcpy(&mvps->plane_info[i], &mvps->plane_info[OSD1_SLICE0],
						sizeof(struct meson_vpu_osd_layer_info));
				mvps->plane_info[i].src_w = mvps->plane_info[OSD1_SLICE0].src_w;
				mvps->plane_info[i].dst_w = mvps->plane_info[OSD1_SLICE0].dst_w;

				mvps->plane_info[i].src_x = mvps->plane_info[OSD1_SLICE0].src_x +
							mvps->plane_info[OSD1_SLICE0].src_w;
				mvps->plane_info[i].dst_x = mvps->plane_info[OSD1_SLICE0].dst_x +
							mvps->plane_info[OSD1_SLICE0].dst_w;
				mvps->plane_info[i].plane_index = i;
				mvps->plane_index[i] = i;
				mvps->num_plane++;
			} else {
				DRM_DEBUG("slice mode osdplane [%d] disable.\n", i);
			}
		}
	}

	for (i = 0; i < pipeline->num_video; i++) {
		if (mvps->video_plane_info[i].enable) {
			crtc_index = mvps->video_plane_info[i].crtc_index;
			mvsps = &mvps->sub_states[crtc_index];
			mvsps->enable_blocks |=
				BIT(pipeline->video[i]->base.id);
			mvps->num_plane_video++;
			DRM_DEBUG("video[%d]-id=%d\n", i,
				  pipeline->video[i]->base.id);
		}
	}
	DRM_DEBUG("num_plane=%d, video_plane_num=%d.\n",
		  mvps->num_plane, mvps->num_plane_video);
}

int vpu_pipeline_osd_check(struct meson_vpu_pipeline *pipeline,
		       struct drm_atomic_state *state)
{
	struct meson_vpu_pipeline_state *mvps;

	mvps = meson_vpu_pipeline_get_state(pipeline, state);
	vpu_pipeline_planes_calc(pipeline, mvps);

	DRM_DEBUG("check done--num_plane=%d.\n", mvps->num_plane);

	return 0;
}

int vpu_pipeline_video_check(struct meson_vpu_pipeline *pipeline,
		       struct drm_atomic_state *state)
{
	struct meson_vpu_pipeline_state *mvps;

	mvps = meson_vpu_pipeline_get_state(pipeline, state);

	vpu_pipeline_planes_calc(pipeline, mvps);
	vpu_video_pipeline_check_block(mvps, state);
	DRM_DEBUG("check done--num_video=%d.\n", mvps->num_plane_video);

	return 0;
}

/*for VIU_OSD2_TCOLOR_AGx, alpha channel [7:0] need keep 0xff in case color key is enabled*/
void vpu_pipeline_append_finish_reg(int crtc_index, struct rdma_reg_ops *reg_ops)
{
	drm_rdma_cnt[crtc_index].val += 0xff;
	reg_ops->rdma_write_reg(drm_rdma_cnt[crtc_index].reg, drm_rdma_cnt[crtc_index].val);
}

void vpu_pipeline_check_finish_reg(int crtc_index)
{
	u32  val;

	val = meson_drm_read_reg(drm_rdma_cnt[crtc_index].reg);
	if (val != drm_rdma_cnt[crtc_index].val)
		DRM_DEBUG("request crtc%d, drm_rdma_dt_cnt [%d] current [%d]\n",
			  crtc_index, drm_rdma_cnt[crtc_index].val >> 8, val >> 8);
}

int vpu_pipeline_check(struct meson_vpu_pipeline *pipeline,
		       struct drm_atomic_state *state)
{
	int ret;
	struct meson_vpu_pipeline_state *mvps;

	mvps = meson_vpu_pipeline_get_state(pipeline, state);
	vpu_pipeline_planes_calc(pipeline, mvps);

	ret = vpu_pipeline_traverse(mvps, state);
	vpu_video_pipeline_check_block(mvps, state);
	DRM_DEBUG("check done--num_plane=%d.\n", mvps->num_plane);

	return ret;
}

void vpu_pipeline_init(struct meson_vpu_pipeline *pipeline)
{
	int i;

	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (pipeline->osds[i])
			VPU_PIPELINE_HW_INIT(&pipeline->osds[i]->base);
	}

	for (i = 0; i < pipeline->num_video; i++)
		VPU_PIPELINE_HW_INIT(&pipeline->video[i]->base);

	for (i = 0; i < pipeline->num_afbc_osds; i++)
		VPU_PIPELINE_HW_INIT(&pipeline->afbc_osds[i]->base);

	for (i = 0; i < MESON_MAX_SCALERS; i++) {
		if (pipeline->scalers[i])
			VPU_PIPELINE_HW_INIT(&pipeline->scalers[i]->base);
	}

	VPU_PIPELINE_HW_INIT(&pipeline->osdblend->base);

	for (i = 0; i < MESON_MAX_HDRS; i++)
		if (pipeline->hdrs[i])
			VPU_PIPELINE_HW_INIT(&pipeline->hdrs[i]->base);

	for (i = 0; i < pipeline->num_postblend; i++)
		VPU_PIPELINE_HW_INIT(&pipeline->postblends[i]->base);

	if (pipeline->slice2ppc)
		VPU_PIPELINE_HW_INIT(&pipeline->slice2ppc->base);
}

void vpu_pipeline_fini(struct meson_vpu_pipeline *pipeline)
{
	int i;

	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (pipeline->osds[i])
			VPU_PIPELINE_HW_FINI(&pipeline->osds[i]->base);
	}

	for (i = 0; i < pipeline->num_video; i++)
		VPU_PIPELINE_HW_FINI(&pipeline->video[i]->base);

	for (i = 0; i < pipeline->num_afbc_osds; i++)
		VPU_PIPELINE_HW_FINI(&pipeline->afbc_osds[i]->base);

	for (i = 0; i < pipeline->num_scalers; i++)
		VPU_PIPELINE_HW_FINI(&pipeline->scalers[i]->base);

	VPU_PIPELINE_HW_FINI(&pipeline->osdblend->base);

	for (i = 0; i < pipeline->num_hdrs; i++)
		VPU_PIPELINE_HW_FINI(&pipeline->hdrs[i]->base);

	for (i = 0; i < pipeline->num_postblend; i++)
		VPU_PIPELINE_HW_FINI(&pipeline->postblends[i]->base);
}
/*
 * Start of Roku async update func implement
 */
int vpu_pipeline_video_update(struct meson_vpu_sub_pipeline *sub_pipeline,
			struct drm_atomic_state *old_state)
{
	int crtc_index;
	unsigned long id;
	struct meson_vpu_block *mvb;
	struct meson_vpu_block_state *mvbs, *old_mvbs;
	struct meson_vpu_pipeline_state *new_mvps;
	struct meson_vpu_sub_pipeline_state *new_mvsps;
	unsigned long affected_blocks = 0;
	struct meson_vpu_pipeline *pipeline = sub_pipeline->pipeline;

	crtc_index = sub_pipeline->index;
	new_mvps = priv_to_pipeline_state(pipeline->obj.state);
	new_mvsps = &new_mvps->sub_states[crtc_index];

	affected_blocks = new_mvsps->enable_blocks;
	for_each_set_bit(id, &affected_blocks, 32) {
		mvb = vpu_blocks[id];
		if (mvb->type != MESON_BLK_VIDEO)
			continue;

		mvbs = priv_to_block_state(mvb->obj.state);
		old_mvbs = meson_vpu_block_get_old_state(mvb, old_state);
		if (new_mvsps->enable_blocks & BIT(id)) {
			mvb->ops->update_state(mvb, mvbs, old_mvbs);
			mvb->ops->enable(mvb, mvbs);
		} else {
			mvb->ops->disable(mvb, old_mvbs);
		}
	}

	return 0;
}

int vpu_pipeline_osd_update(struct meson_vpu_sub_pipeline *sub_pipeline,
			struct drm_atomic_state *old_state)
{
#ifdef CONFIG_DEBUG_FS
	int i;
#endif
	int crtc_index;
	unsigned long id;
	struct meson_vpu_block *mvb;
	struct meson_vpu_block_state *mvbs, *old_mvbs;
	struct meson_vpu_pipeline_state *new_mvps;
	struct meson_vpu_sub_pipeline_state *new_mvsps;
	struct meson_vpu_pipeline *pipeline = sub_pipeline->pipeline;
	unsigned long affected_blocks = 0;

	crtc_index = sub_pipeline->index;
	new_mvps = priv_to_pipeline_state(pipeline->obj.state);
	new_mvsps = &new_mvps->sub_states[crtc_index];

	affected_blocks = new_mvsps->enable_blocks;
	for_each_set_bit(id, &affected_blocks, 32) {
		mvb = vpu_blocks[id];
		/*TODO: we may need also update other blocks on newer soc.*/
		if (mvb->type != MESON_BLK_OSD &&
			mvb->type != MESON_BLK_AFBC)
			continue;

		mvbs = priv_to_block_state(mvb->obj.state);
		old_mvbs = meson_vpu_block_get_old_state(mvb, old_state);
		if (new_mvsps->enable_blocks & BIT(id)) {
			mvb->ops->update_state(mvb, mvbs, old_mvbs);
			mvb->ops->enable(mvb, mvbs);
		} else {
			mvb->ops->disable(mvb, mvbs);
		}
	}

#ifdef CONFIG_DEBUG_FS
	if (overwrite_enable) {
		for (i = 0; i < reg_num; i++)
			meson_vpu_write_reg(overwrite_reg[i], overwrite_val[i]);
	}
#endif

	vpu_pipeline_append_finish_reg(crtc_index, sub_pipeline->reg_ops);

	return 0;
}

//end of Roku async update func implement

int vpu_video_plane_update(struct meson_vpu_sub_pipeline *sub_pipeline,
			struct drm_atomic_state *old_state, int plane_index)
{
	int crtc_index;
	struct meson_vpu_block *mvb;
	struct meson_vpu_block_state *mvbs, *old_mvbs;
	struct meson_vpu_pipeline_state *old_mvps, *new_mvps;
	struct meson_vpu_sub_pipeline_state *old_mvsps, *new_mvsps;
	struct meson_vpu_pipeline *pipeline = sub_pipeline->pipeline;
	struct meson_vpu_video *mvv = pipeline->video[plane_index];
	unsigned long affected_blocks = 0;

	crtc_index = sub_pipeline->index;
	mvb = &mvv->base;
	mvbs = priv_to_block_state(mvb->obj.state);
	old_mvbs = meson_vpu_block_get_old_state(mvb, old_state);
	old_mvps = meson_vpu_pipeline_get_state(pipeline, old_state);
	new_mvps = priv_to_pipeline_state(pipeline->obj.state);
	old_mvsps = &old_mvps->sub_states[crtc_index];
	new_mvsps = &new_mvps->sub_states[crtc_index];

	affected_blocks = old_mvsps->enable_blocks | new_mvsps->enable_blocks;

	if (affected_blocks & BIT(mvb->id)) {
		if (new_mvsps->enable_blocks & BIT(mvb->id)) {
			mvb->ops->update_state(mvb, mvbs, old_mvbs);
			mvb->ops->enable(mvb, mvbs);
		} else {
			mvb->ops->disable(mvb, mvbs);
		}
	}

	return 0;
}

/* maybe use graph traverse is a good choice */
int vpu_osd_pipeline_update(struct meson_vpu_sub_pipeline *sub_pipeline,
			struct drm_atomic_state *old_state)
{
#ifdef CONFIG_DEBUG_FS
	int i;
#endif

	int crtc_index;
	unsigned long id;
	struct meson_vpu_block *mvb;
	struct meson_vpu_block_state *mvbs, *old_mvbs;
	struct meson_vpu_pipeline_state *old_mvps, *new_mvps;
	struct meson_vpu_sub_pipeline_state *old_mvsps, *new_mvsps;
	struct meson_vpu_pipeline *pipeline = sub_pipeline->pipeline;
	unsigned long affected_blocks = 0;

	crtc_index = sub_pipeline->index;
	old_mvps = meson_vpu_pipeline_get_state(pipeline, old_state);
	new_mvps = priv_to_pipeline_state(pipeline->obj.state);
	old_mvsps = &old_mvps->sub_states[crtc_index];
	new_mvsps = &new_mvps->sub_states[crtc_index];
	new_mvps->global_afbc = 0;

	DRM_DEBUG("old_enable_blocks: 0x%llx - %p, new_enable_blocks: 0x%llx - %p.\n",
		  old_mvsps->enable_blocks, old_mvsps,
		  new_mvsps->enable_blocks, new_mvsps);

	arm_fbc_check_error();

	affected_blocks = old_mvsps->enable_blocks | new_mvsps->enable_blocks;
	for_each_set_bit(id, &affected_blocks, 32) {
		mvb = vpu_blocks[id];
		if (mvb->type == MESON_BLK_VIDEO)
			continue;

		mvbs = priv_to_block_state(mvb->obj.state);
		old_mvbs = meson_vpu_block_get_old_state(mvb, old_state);

		if (new_mvsps->enable_blocks & BIT(id)) {
			DRM_DEBUG("Enable block %s: mvbs new-%p, old-%p\n",
				mvb->name, mvbs, old_mvbs);

			mvb->ops->update_state(mvb, mvbs, old_mvbs);
			mvb->ops->enable(mvb, mvbs);
		} else {
			DRM_DEBUG("Disable block %s: mvbs new-%p, old-%p\n",
				mvb->name, mvbs, old_mvbs);

			if (!old_mvbs || !old_mvbs->sub) {
				DRM_ERROR("old_mvbs or sub is invalid.\n");
				continue;
			}

			mvb->ops->disable(mvb, old_mvbs);
		}
	}

#ifdef CONFIG_DEBUG_FS
	if (overwrite_enable) {
		for (i = 0; i < reg_num; i++)
			meson_vpu_write_reg(overwrite_reg[i], overwrite_val[i]);
	}
#endif

	vpu_pipeline_append_finish_reg(crtc_index, sub_pipeline->reg_ops);

	return 0;
}

int vpu_topology_populate(struct meson_vpu_pipeline *pipeline)
{
	struct meson_drm *priv = pipeline->priv;

	populate_vpu_pipeline(priv->blocks_node, pipeline);
	of_node_put(priv->blocks_node);
	of_node_put(priv->topo_node);

	return 0;
}
EXPORT_SYMBOL(vpu_topology_populate);

int vpu_topology_init(struct platform_device *pdev, struct meson_drm *priv)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child, *vpu_block_node;
	struct meson_vpu_pipeline *pipeline;

	child = of_get_child_by_name(np, "vpu_topology");
	if (!child)
		return -ENODEV;

	vpu_block_node = of_get_child_by_name(child, "vpu_blocks");
	if (!vpu_block_node) {
		of_node_put(child);
		return -ENODEV;
	}

	pipeline = kzalloc(sizeof(*pipeline), GFP_KERNEL);
	if (!pipeline)
		return -ENOMEM;

	priv->pipeline = pipeline;
	pipeline->priv = priv;
	priv->topo_node = child;
	priv->blocks_node = vpu_block_node;

	return 0;
}
EXPORT_SYMBOL(vpu_topology_init);

static int get_venc_type(struct meson_vpu_pipeline *pipeline, u32 viu_type)
{
	u32 venc_type = 0;

	if (pipeline->priv->vpu_data->enc_method == 1) {
		u32 venc_mux = 3;
		u32 venc_addr = VPU_VENC_CTRL;

		venc_mux = aml_read_vcbus(VPU_VIU_VENC_MUX_CTRL) & 0x3f;
		venc_mux >>= (viu_type * 2);
		venc_mux &= 0x3;

		switch (venc_mux) {
		case 0:
			venc_addr = VPU_VENC_CTRL;
			break;
		case 1:
			venc_addr = VPU1_VENC_CTRL;
			break;
		case 2:
			venc_addr = VPU2_VENC_CTRL;
			break;
		}
		venc_type = aml_read_vcbus(venc_addr);
	} else {
		if (viu_type == 0)
			venc_type = aml_read_vcbus(VPU_VIU_VENC_MUX_CTRL);
		else if (viu_type == 1)
			venc_type = aml_read_vcbus(VPU_VIU_VENC_MUX_CTRL) >> 2;
	}

	venc_type &= 0x3;

	return venc_type;
}

int vpu_pipeline_read_scanout_pos(struct meson_vpu_pipeline *pipeline,
	int *vpos, int *hpos, int crtc_index)
{
	int viu_type = crtc_index;
	unsigned int reg = VPU_VENCI_STAT;
	unsigned int reg_val = 0;
	u32 offset = 0;
	u32 venc_type = get_venc_type(pipeline, viu_type);

	if (pipeline->priv->vpu_data->enc_method == 1) {
		u32 venc_mux = 3;

		venc_mux = aml_read_vcbus(VPU_VIU_VENC_MUX_CTRL) & 0x3f;
		venc_mux >>= (viu_type * 2);
		venc_mux &= 0x3;
		switch (venc_mux) {
		case 0:
			offset = 0;
			break;
		case 1:
			offset = 0x600;
			break;
		case 2:
			offset = 0x800;
			break;
		}
		switch (venc_type) {
		case 0:
			reg = VPU_VENCI_STAT;
			break;
		case 1:
			reg = VPU_VENCP_STAT;
			break;
		case 2:
			reg = VPU_VENCL_STAT;
			break;
		}

		reg_val = aml_read_vcbus(reg + offset);
		if (!venc_type)
			*vpos = (reg_val >> 16) & 0xfff;
		else
			*vpos = (reg_val >> 16) & 0x1fff;

		*hpos = (reg_val & 0x1fff);
	} else {
		switch (venc_type) {
		case 0:
			reg = ENCL_INFO_READ;
			break;
		case 1:
			reg = ENCI_INFO_READ;
			break;
		case 2:
			reg = ENCP_INFO_READ;
			break;
		case 3:
			reg = ENCT_INFO_READ;
			break;
		}

		reg_val = aml_read_vcbus(reg + offset);
		*vpos = (reg_val >> 16) & 0x1fff;
		*hpos = (reg_val & 0x1fff);
	}

	return 0;
}
EXPORT_SYMBOL(vpu_pipeline_read_scanout_pos);

static int vpu_pipeline_get_active_begin_line(struct meson_vpu_pipeline *pipeline, u32 viu_type)
{
	int active_line_begin = 0;
	u32 offset = 0;
	u32 reg = ENCL_VIDEO_VAVON_BLINE;

	if (pipeline->priv->vpu_data->enc_method == 1) {
		u32 venc_mux = 3;

		venc_mux = aml_read_vcbus(VPU_VIU_VENC_MUX_CTRL) & 0x3f;
		venc_mux >>= (viu_type * 2);
		venc_mux &= 0x3;

		switch (venc_mux) {
		case 0:
			offset = 0;
			break;
		case 1:
			offset = 0x600;
			break;
		case 2:
			offset = 0x800;
			break;
		}
		switch (get_venc_type(pipeline, viu_type)) {
		case 0:
			reg = ENCI_VFIFO2VD_LINE_TOP_START;
			break;
		case 1:
			reg = ENCP_VIDEO_VAVON_BLINE;
			break;
		case 2:
			reg = ENCL_VIDEO_VAVON_BLINE;
			break;
		}

	} else {
		switch (get_venc_type(pipeline, viu_type)) {
		case 0:
			reg = ENCL_VIDEO_VAVON_BLINE;
			break;
		case 1:
			reg = ENCI_VFIFO2VD_LINE_TOP_START;
			break;
		case 2:
			reg = ENCP_VIDEO_VAVON_BLINE;
			break;
		case 3:
			reg = ENCT_VIDEO_VAVON_BLINE;
			break;
		}
	}

	active_line_begin = aml_read_vcbus(reg + offset);

	return active_line_begin;
}

void vpu_pipeline_prepare_update(struct meson_vpu_pipeline *pipeline,
	int vdisplay, int vrefresh, int crtc_index)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	int vsync_active_begin, wait_cnt, cur_line, cur_col, line_threshold;

	/*for rdma, we need
	 * 1. finish rdma table write before VACTIVE(last_VFP~VBP).
	 * 2. wait rdma hw flush finish (flush time depends on aps clock.)
	 * | VSYNC| VBackP | VACTIVE | VFrontP |...
	 */
	vsync_active_begin = vpu_pipeline_get_active_begin_line(pipeline, crtc_index);
	vpu_pipeline_read_scanout_pos(pipeline, &cur_line, &cur_col, crtc_index);
	line_threshold = vdisplay * flush_time * vrefresh / 1000;
	wait_cnt = 0;
	while (cur_line >= vdisplay + vsync_active_begin - line_threshold ||
			cur_line <= vsync_active_begin) {
		DRM_DEBUG_VBL("enc line=%d, vdisplay %d, BP = %d\n",
			cur_line, vdisplay, vsync_active_begin);
		/* 0.5ms */
		usleep_range(500, 600);
		wait_cnt++;
		if (wait_cnt >= WAIT_CNT_MAX) {
			DRM_DEBUG_VBL("%s time out\n", __func__);
			break;
		}
		vpu_pipeline_read_scanout_pos(pipeline, &cur_line, &cur_col, crtc_index);
	}
#endif
}
EXPORT_SYMBOL(vpu_pipeline_prepare_update);

void vpu_pipeline_finish_update(struct meson_vpu_pipeline *pipeline, int crtc_index)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	meson_vpu_reg_vsync_config(crtc_index);
#endif
}
EXPORT_SYMBOL(vpu_pipeline_finish_update);
