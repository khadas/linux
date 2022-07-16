// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* This program is free software; you can redistribute it and/or modify
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

/* Standard Linux Headers */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/dma-buf.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "vdin_drv.h"
#include "vdin_screen.h"

static unsigned int debug_sleep_time = 50;
module_param(debug_sleep_time, uint, 0664);
MODULE_PARM_DESC(debug_sleep_time, "debug_sleep_time debug");

/* get support pixel format lists */

int vdin_get_support_pixel_format(struct support_pixel_format *pixel_format)
{
	if (!pixel_format) {
		pr_info("%s:pixel_format is null\n", __func__);
		return -1;
	}

	pixel_format->pixel_value[0] = TVIN_PIXEL_RGB444;
	pixel_format->pixel_value[1] = TVIN_PIXEL_YUV422;
	pixel_format->pixel_value[2] = TVIN_PIXEL_UYVY444;
	pixel_format->pixel_value[3] = TVIN_PIXEL_NV12;
	pixel_format->pixel_value[4] = TVIN_PIXEL_NV21;

	return 0;
}
EXPORT_SYMBOL(vdin_get_support_pixel_format);

int vdin_capture_picture(struct vdin_parm_s *vdin_cap_param,
			 struct dma_buf *cap_buf)
{
	struct page *page;
	struct vdin_dev_s *devp;

	devp = vdin_get_dev(1);
	if (!devp) {
		pr_info("devp is null\n");
		return -1;
	}

	vdin_cap_param->frame_rate = 60;
	devp->cfg_dma_buf = 1;
	devp->flags |= VDIN_FLAG_MANUAL_CONVERSION;
	devp->debug.dest_cfmt = vdin_cap_param->dfmt;
	devp->debug.scaling4w = vdin_cap_param->dest_h_active;
	devp->debug.scaling4h = vdin_cap_param->dest_v_active;
	vdin_set_canvas_addr[0].dma_buffer = cap_buf;
	vdin_set_canvas_addr[0].dmabuf_attach =
	dma_buf_attach(vdin_set_canvas_addr[0].dma_buffer, devp->dev);
	vdin_set_canvas_addr[0].sg_table =
	dma_buf_map_attachment(vdin_set_canvas_addr[0].dmabuf_attach,
				DMA_BIDIRECTIONAL);
	page = sg_page(vdin_set_canvas_addr[0].sg_table->sgl);
	vdin_set_canvas_addr[0].paddr = PFN_PHYS(page_to_pfn(page));
	vdin_set_canvas_addr[0].size =
		vdin_set_canvas_addr[0].dma_buffer->size;
	start_tvin_service(devp->index, vdin_cap_param);
	if (debug_sleep_time) {
		msleep(debug_sleep_time);
		stop_tvin_service(devp->index);
		dma_buf_unmap_attachment(vdin_set_canvas_addr[0].dmabuf_attach,
					 vdin_set_canvas_addr[0].sg_table,
					 DMA_BIDIRECTIONAL);
		dma_buf_detach(vdin_set_canvas_addr[0].dma_buffer,
			       vdin_set_canvas_addr[0].dmabuf_attach);
		dma_buf_put(vdin_set_canvas_addr[0].dma_buffer);
		memset(vdin_set_canvas_addr, 0,
			sizeof(struct vdin_set_canvas_addr_s) *
			VDIN_CANVAS_MAX_CNT);
		devp->cfg_dma_buf = 0;
		devp->flags &= (~VDIN_FLAG_MANUAL_CONVERSION);
	}

	return 0;
}
EXPORT_SYMBOL(vdin_capture_picture);

int vdin_screen_get_secure_flag(bool *secure_flag)
{
	struct vdin_dev_s *devp;

	devp = vdin_get_dev(1);
	if (!devp) {
		pr_info("devp is null\n");
		return -1;
	}

	*secure_flag = devp->secure_en;

	return 0;
}
EXPORT_SYMBOL(vdin_screen_get_secure_flag);

