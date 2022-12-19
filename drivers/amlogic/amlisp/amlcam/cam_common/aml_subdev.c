/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2020 Amlogic or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#define pr_fmt(fmt)  "aml-subdev:%s:%d: " fmt, __func__, __LINE__
#include <linux/version.h>
#include <linux/io.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <linux/of_platform.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
#include <linux/dma-map-ops.h>
#else
#include <linux/dma-contiguous.h>
#endif
#include <linux/cma.h>

#include "aml_common.h"

static int subdev_set_stream(struct v4l2_subdev *sd, int enable)
{
	int rtn = 0;

	struct aml_subdev *subdev = v4l2_get_subdevdata(sd);

	if (enable)
		rtn = subdev->ops->stream_on(subdev->priv);
	else
		subdev->ops->stream_off(subdev->priv);

	return rtn;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int subdev_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)

#else
static int subdev_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
#endif
{
	struct aml_subdev *subdev = v4l2_get_subdevdata(sd);

	if (code->pad == 0) {
		if (code->index >= subdev->fmt_cnt)
			return -EINVAL;

		code->code = subdev->formats[code->index].code;
	} else {
		if (code->index > 0)
			return -EINVAL;

		code->code = subdev->formats[0].code;
	}

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int subdev_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *cfg,
				struct v4l2_subdev_selection *sel)
#else
static int subdev_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel)
#endif
{
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int subdev_set_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *cfg,
				struct v4l2_subdev_selection *sel)
#else
static int subdev_set_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_selection *sel)
#endif
{
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static struct v4l2_mbus_framefmt *subdev_get_padfmt(struct aml_subdev *subdev,
				struct v4l2_subdev_state *cfg,
				struct v4l2_subdev_format *fmt)
#else
static struct v4l2_mbus_framefmt *subdev_get_padfmt(struct aml_subdev *subdev,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
#endif
{
	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(subdev->sd, cfg, fmt->pad);

	return &subdev->pfmt[fmt->pad];
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int subdev_set_format(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *cfg,
			     struct v4l2_subdev_format *fmt)
#else
static int subdev_set_format(struct v4l2_subdev *sd,
			     struct v4l2_subdev_pad_config *cfg,
			     struct v4l2_subdev_format *fmt)
#endif
{
	struct v4l2_mbus_framefmt *format;
	struct aml_subdev *subdev = v4l2_get_subdevdata(sd);

	format = subdev_get_padfmt(subdev, cfg, fmt);
	if (format == NULL)
		return -EINVAL;

	*format = fmt->format;
	format->field = V4L2_FIELD_NONE;

	if (subdev->ops->set_format)
		subdev->ops->set_format(subdev->priv, fmt, format);

	return 0;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int subdev_get_format(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *cfg,
			     struct v4l2_subdev_format *fmt)
#else
static int subdev_get_format(struct v4l2_subdev *sd,
			     struct v4l2_subdev_pad_config *cfg,
			     struct v4l2_subdev_format *fmt)
#endif
{
	struct v4l2_mbus_framefmt *format;
	struct aml_subdev *subdev = v4l2_get_subdevdata(sd);

	format = subdev_get_padfmt(subdev, cfg, fmt);
	if (format == NULL)
		return -EINVAL;

	fmt->format = *format;

	return 0;
}

static int subdev_log_status(struct v4l2_subdev *sd)
{
	struct aml_subdev *subdev = v4l2_get_subdevdata(sd);

	if (subdev->ops->log_status)
		subdev->ops->log_status(subdev->priv);

	return 0;
}

static int subdev_event_subscribe(struct v4l2_subdev *sd, struct v4l2_fh *fh,
			       struct v4l2_event_subscription *sub)
{
	if (sub->type != V4L2_EVENT_FRAME_SYNC || sub->id != 0)
		return -EINVAL;

	return v4l2_event_subscribe(fh, sub, 0, NULL);
}

const struct v4l2_subdev_core_ops subdev_core_ops = {
	.log_status = subdev_log_status,
	.subscribe_event = subdev_event_subscribe,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
};

static const struct v4l2_subdev_video_ops subdev_video_ops = {
	.s_stream = subdev_set_stream,
};

static const struct v4l2_subdev_pad_ops subdev_pad_ops = {
	.enum_mbus_code = subdev_enum_mbus_code,
	.get_selection = subdev_get_selection,
	.set_selection = subdev_set_selection,
	.get_fmt = subdev_get_format,
	.set_fmt = subdev_set_format,
};

static const struct v4l2_subdev_ops subdev_v4l2_ops = {
	.core = &subdev_core_ops,
	.video = &subdev_video_ops,
	.pad = &subdev_pad_ops,
};

static int subdev_link_validate(struct media_link *link)
{
	int rtn = 0;
	struct media_entity *src = link->source->entity;
	struct media_entity *sink = link->sink->entity;

	rtn = v4l2_subdev_link_validate(link);
	if (rtn)
		pr_err("Error: src->sink: %s-->%s, rtn %d\n",
			src->name, sink->name, rtn);

	return rtn;
}

static const struct media_entity_operations subdev_media_ops = {
	.link_validate = subdev_link_validate,
};

int aml_subdev_register(struct aml_subdev *subdev)
{
	int rtn = -1;
	u32 pad_max = subdev->pad_max;
	struct v4l2_subdev *sd = subdev->sd;
	struct media_pad *pads = subdev->pads;
	struct v4l2_device *v4l2_dev = subdev->v4l2_dev;

	v4l2_subdev_init(sd, &subdev_v4l2_ops);
	sd->owner = THIS_MODULE;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	memcpy(sd->name, subdev->name, sizeof(sd->name));

	sd->entity.function = subdev->function;

	v4l2_set_subdevdata(sd, subdev);

	sd->entity.ops = &subdev_media_ops;

	rtn = media_entity_pads_init(&sd->entity, pad_max, pads);
	if (rtn) {
		pr_err("Error init entity pads: %d\n", rtn);
		return rtn;
	}

	rtn = v4l2_device_register_subdev(v4l2_dev, sd);
	if (rtn < 0) {
		pr_err("Error to register subdev: %d\n", rtn);
		media_entity_cleanup(&sd->entity);
	}

	return rtn;
}

void aml_subdev_unregister(struct aml_subdev *subdev)
{
	struct v4l2_subdev *sd = subdev->sd;

	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
}

int aml_subdev_cma_alloc(struct platform_device *pdev, u32 *paddr, void *addr, unsigned long size)
{
	struct page *cma_pages = NULL;

	size = ISP_SIZE_ALIGN(size, 1 << 12);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	struct device *dev = &(pdev->dev);
	struct cma *cma_area;
	if (dev && dev->cma_area)
		cma_area = dev->cma_area;
	else
		cma_area = dma_contiguous_default_area;
	cma_pages = cma_alloc(cma_area, size >> PAGE_SHIFT, 0, 0);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
	cma_pages = dma_alloc_from_contiguous(
		&(pdev->dev), size >> PAGE_SHIFT, 0, false);
#else
	cma_pages = dma_alloc_from_contiguous(
		&(pdev->dev), size >> PAGE_SHIFT, 0);
#endif
	if (cma_pages) {
		*paddr = page_to_phys(cma_pages);
	} else {
		pr_debug("Failed alloc cma pages.\n");
		return -1;
	}

	addr = (void *)cma_pages;

	return 0;
}

void aml_subdev_cma_free(struct platform_device *pdev, void *paddr, unsigned long size)
{
	struct page *cma_pages = NULL;
	bool rc = false;

	size = ISP_SIZE_ALIGN(size, 1 << 12);

	cma_pages = paddr;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	struct cma *cma_area;
	struct device *dev = &(pdev->dev);
	if (dev && dev->cma_area)
		cma_area = dev->cma_area;
	else
		cma_area = dma_contiguous_default_area;
	rc = cma_release(cma_area, cma_pages, size >> PAGE_SHIFT);
#else
	rc = dma_release_from_contiguous(&(pdev->dev), cma_pages, size >> PAGE_SHIFT);
#endif
	if (rc == false) {
		pr_debug("Failed to release cma buffer\n");
		return;
	}
}

void * aml_subdev_map_vaddr(u32 phys_addr, u32 length)
{
	pgprot_t prot;
	struct page *page = phys_to_page(phys_addr);
	struct page **pages = NULL;
	unsigned int pagesnr;
	struct page *tmp = NULL;
	void *vaddr = NULL;
	int array_size;
	int i;

	prot = pgprot_writecombine(PAGE_KERNEL);
	pagesnr = length / PAGE_SIZE;
	tmp = page;
	array_size = sizeof(struct page *) * pagesnr;

	pages = vmalloc(array_size);
	if (pages == NULL) {
		pr_info("0x%x vmalloc failed.\n", array_size);
		return NULL;
	}

	for (i = 0; i < pagesnr; i++) {
		*(pages + i) = tmp;
		tmp++;
	}

	vaddr = vmap(pages, pagesnr, VM_MAP, prot);
	vfree(pages);

	return vaddr;
}

void aml_subdev_unmap_vaddr(void *vaddr)
{
	vunmap(vaddr);
}

