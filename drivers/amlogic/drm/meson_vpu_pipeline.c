/*
 * drivers/amlogic/drm/meson_vpu_pipeline.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <dt-bindings/display/meson-drm-ids.h>

#include "meson_vpu_pipeline.h"
#include "meson_drv.h"

#define MAX_LINKS 5
#define MAX_PORTS 6
#define MAX_PORT_ID 32

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
			para->inputs[j].id = id;
			in_mask |= 1 << id;
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
			para->outputs[j].id = id;
			out_mask |= 1 << id;
		}
		para->outputs_mask = out_mask;
	}
	DRM_INFO("id=%d,index=%d,num_in_links=%d,num_out_links=%d\n",
		para->id, para->index,
		para->num_inputs, para->num_outputs);
	DRM_INFO("in_mask=0x%llx,out_mask=0x%llx\n", in_mask, out_mask);
}

static struct meson_vpu_block *
meson_vpu_create_block(struct meson_vpu_block_para *para,
		       struct meson_vpu_pipeline *pipeline)
{
	struct meson_vpu_block *mvb;
	size_t blk_size;

	switch (para->type) {
	case MESON_BLK_OSD:
		blk_size = sizeof(struct meson_vpu_osd);
		mvb = create_block(blk_size, para, &osd_ops, pipeline);

		pipeline->osds[mvb->index] = to_osd_block(mvb);
		pipeline->num_osds++;
		break;
	case MESON_BLK_AFBC:
		blk_size = sizeof(struct meson_vpu_afbc);
		mvb = create_block(blk_size, para, &afbc_ops, pipeline);

		pipeline->afbc_osds[mvb->index] = to_afbc_block(mvb);
		pipeline->num_afbc_osds++;
		break;
	case MESON_BLK_SCALER:
		blk_size = sizeof(struct meson_vpu_scaler);
		mvb = create_block(blk_size, para, &scaler_ops, pipeline);

		pipeline->scalers[mvb->index] = to_scaler_block(mvb);
		pipeline->num_scalers++;
		break;
	case MESON_BLK_OSDBLEND:
		blk_size = sizeof(struct meson_vpu_osdblend);
		mvb = create_block(blk_size, para, &osdblend_ops, pipeline);

		pipeline->osdblend = to_osdblend_block(mvb);
		break;
	case MESON_BLK_HDR:
		blk_size = sizeof(struct meson_vpu_hdr);
		mvb = create_block(blk_size, para, &hdr_ops, pipeline);

		pipeline->hdr = to_hdr_block(mvb);
		break;
	case MESON_BLK_DOVI:
		blk_size = sizeof(struct meson_vpu_dolby);
		mvb = create_block(blk_size, para, &dolby_ops, pipeline);

		pipeline->dolby = to_dolby_block(mvb);
		break;
	case MESON_BLK_VPPBLEND:
		blk_size = sizeof(struct meson_vpu_postblend);
		mvb = create_block(blk_size, para, &postblend_ops, pipeline);

		pipeline->postblend = to_postblend_block(mvb);
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

	return 0;
}

void VPU_PIPELINE_HW_INIT(struct meson_vpu_block *mvb)
{
	if (mvb->ops->init)
		mvb->ops->init(mvb);
}

static void vpu_pipeline_planes_calc(struct meson_vpu_pipeline *pipeline,
		struct meson_vpu_pipeline_state *mvps)
{
	u8 i;

	mvps->num_plane = 0;
	mvps->enable_blocks = 0;
	for (i = 0; i < pipeline->num_osds; i++) {
		if (mvps->plane_info[i].enable) {
			if (mvps->plane_info[i].src_w >
				MESON_OSD_INPUT_W_LIMIT ||
				mvps->plane_info[i].dst_w == 0) {
				mvps->plane_info[i].enable = 0;
				continue;
			}
			mvps->num_plane++;
		}
	}
	DRM_DEBUG("num_plane=%d.\n", mvps->num_plane);
}

int vpu_pipeline_check(struct meson_vpu_pipeline *pipeline,
		struct drm_atomic_state *state)
{
	int ret;
	struct meson_vpu_pipeline_state *mvps;

	mvps = meson_vpu_pipeline_get_state(pipeline, state);

	vpu_pipeline_planes_calc(pipeline, mvps);

	ret = vpu_pipeline_traverse(mvps, state);
	DRM_DEBUG("check done--num_plane=%d.\n", mvps->num_plane);

	return ret;
}

void vpu_pipeline_init(struct meson_vpu_pipeline *pipeline)
{
	int i;

	for (i = 0; i < pipeline->num_osds; i++)
		VPU_PIPELINE_HW_INIT(&pipeline->osds[i]->base);

	for (i = 0; i < pipeline->num_afbc_osds; i++)
		VPU_PIPELINE_HW_INIT(&pipeline->afbc_osds[i]->base);

	for (i = 0; i < pipeline->num_scalers; i++)
		VPU_PIPELINE_HW_INIT(&pipeline->scalers[i]->base);

	VPU_PIPELINE_HW_INIT(&pipeline->osdblend->base);

	VPU_PIPELINE_HW_INIT(&pipeline->hdr->base);

	VPU_PIPELINE_HW_INIT(&pipeline->postblend->base);
}

/* maybe use graph traverse is a good choice */
int vpu_pipeline_update(struct meson_vpu_pipeline *pipeline,
		struct drm_atomic_state *old_state)
{
	unsigned long id;
	struct meson_vpu_block *mvb;
	struct meson_vpu_block_state *mvbs;
	struct meson_vpu_pipeline_state *old_mvps, *new_mvps;
	unsigned long affected_blocks = 0;

	old_mvps = meson_vpu_pipeline_get_state(pipeline, old_state);
	new_mvps = priv_to_pipeline_state(pipeline->obj.state);

	DRM_DEBUG("old_enable_blocks: 0x%llx, new_enable_blocks: 0x%llx.\n",
		old_mvps->enable_blocks, new_mvps->enable_blocks);

	#ifdef MESON_DRM_VERSION_V0
	meson_vpu_pipeline_atomic_backup_state(new_mvps);
	#endif
	affected_blocks = old_mvps->enable_blocks | new_mvps->enable_blocks;
	for_each_set_bit(id, &affected_blocks, 32) {
		mvb = vpu_blocks[id];
		mvbs = priv_to_block_state(mvb->obj.state);

		if (new_mvps->enable_blocks & BIT(id)) {
			mvb->ops->update_state(mvb, mvbs);
			mvb->ops->enable(mvb);
		} else {
			mvb->ops->disable(mvb);
		}
	}

	return 0;
}

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

	populate_vpu_pipeline(vpu_block_node, pipeline);
	priv->pipeline = pipeline;
	of_node_put(vpu_block_node);
	of_node_put(child);

	return 0;
}
EXPORT_SYMBOL(vpu_topology_init);
