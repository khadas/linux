/*
 * drivers/amlogic/media/gdc/app/gdc_module.c
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

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/uio_driver.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <meson_ion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-buf.h>

#include <linux/of_address.h>
#include <api/gdc_api.h>
#include "system_log.h"

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/firmware.h>

//gdc configuration sequence
#include "gdc_config.h"
#include "gdc_dmabuf.h"
#include "gdc_wq.h"

int gdc_log_level;
struct gdc_manager_s gdc_manager;
unsigned int gdc_reg_store_mode;
int trace_mode_enable;
char *config_out_file;
int config_out_path_defined;

#define CORE_CLK_RATE 800000000
#define AXI_CLK_RATE  800000000

static const struct of_device_id gdc_dt_match[] = {
	{.compatible = "amlogic, g12b-gdc"},
	{} };

MODULE_DEVICE_TABLE(of, gdc_dt_match);
static void meson_gdc_cache_flush(struct device *dev,
					dma_addr_t addr,
					size_t size);

//////
static int meson_gdc_open(struct inode *inode, struct file *file)
{
	struct gdc_context_s *context = NULL;

	/* we create one gdc  workqueue for this file handler. */
	context = create_gdc_work_queue();
	if (!context) {
		gdc_log(LOG_ERR, "can't create work queue\n");
		return -1;
	}

	file->private_data = context;

	gdc_log(LOG_INFO, "Success open\n");

	return 0;
}

static int meson_gdc_release(struct inode *inode, struct file *file)
{
	struct gdc_context_s *context = NULL;
	struct page *cma_pages = NULL;
	bool rc = false;
	int ret = 0;
	struct device *dev = &gdc_manager.gdc_dev->pdev->dev;

	context = (struct gdc_context_s *)file->private_data;
	if (!context) {
		gdc_log(LOG_ERR, "context is null,release work queue error\n");
		return -1;
	}

	if (context->i_kaddr != 0 && context->i_len != 0) {
		cma_pages = virt_to_page(context->i_kaddr);
		rc = dma_release_from_contiguous(dev,
						cma_pages,
						context->i_len >> PAGE_SHIFT);
		if (rc == false) {
			ret = ret - 1;
			gdc_log(LOG_ERR, "Failed to release input buff\n");
		}
		mutex_lock(&context->d_mutext);
		context->i_kaddr = NULL;
		context->i_paddr = 0;
		context->i_len = 0;
		mutex_unlock(&context->d_mutext);
	}

	if (context->o_kaddr != 0 && context->o_len != 0) {
		cma_pages = virt_to_page(context->o_kaddr);
		rc = dma_release_from_contiguous(dev,
						cma_pages,
						context->o_len >> PAGE_SHIFT);
		if (rc == false) {
			ret = ret - 1;
			gdc_log(LOG_ERR, "Failed to release output buff\n");
		}
		mutex_lock(&context->d_mutext);
		context->o_kaddr = NULL;
		context->o_paddr = 0;
		context->o_len = 0;
		mutex_unlock(&context->d_mutext);
	}

	if (context->c_kaddr != 0 && context->c_len != 0) {
		cma_pages = virt_to_page(context->c_kaddr);
		rc = dma_release_from_contiguous(dev,
						cma_pages,
						context->c_len >> PAGE_SHIFT);
		if (rc == false) {
			ret = ret - 1;
			gdc_log(LOG_ERR, "Failed to release config buff\n");
		}
		mutex_lock(&context->d_mutext);
		context->c_kaddr = NULL;
		context->c_paddr = 0;
		context->c_len = 0;
		mutex_unlock(&context->d_mutext);
	}
	if (destroy_gdc_work_queue(context) == 0)
		gdc_log(LOG_INFO, "release one gdc device\n");

	return ret;
}

static int meson_gdc_set_buff(struct gdc_context_s *context,
			      struct page *cma_pages,
			      unsigned long len)
{
	int ret = 0;

	if (context == NULL || cma_pages == NULL || len == 0) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	switch (context->mmap_type) {
	case INPUT_BUFF_TYPE:
		if (context->i_paddr != 0 && context->i_kaddr != NULL)
			return -EAGAIN;
		mutex_lock(&context->d_mutext);
		context->i_paddr = page_to_phys(cma_pages);
		context->i_kaddr = phys_to_virt(context->i_paddr);
		context->i_len = len;
		mutex_unlock(&context->d_mutext);
	break;
	case OUTPUT_BUFF_TYPE:
		if (context->o_paddr != 0 && context->o_kaddr != NULL)
			return -EAGAIN;
		mutex_lock(&context->d_mutext);
		context->o_paddr = page_to_phys(cma_pages);
		context->o_kaddr = phys_to_virt(context->o_paddr);
		context->o_len = len;
		mutex_unlock(&context->d_mutext);
		meson_gdc_cache_flush(&gdc_manager.gdc_dev->pdev->dev,
			context->o_paddr, context->o_len);
	break;
	case CONFIG_BUFF_TYPE:
		if (context->c_paddr != 0 && context->c_kaddr != NULL)
			return -EAGAIN;
		mutex_lock(&context->d_mutext);
		context->c_paddr = page_to_phys(cma_pages);
		context->c_kaddr = phys_to_virt(context->c_paddr);
		context->c_len = len;
		mutex_unlock(&context->d_mutext);
	break;
	default:
		gdc_log(LOG_ERR, "Error mmap type:0x%x\n", context->mmap_type);
		ret = -EINVAL;
	break;
	}

	return ret;
}

static int meson_gdc_set_input_addr(u32 start_addr,
				    struct gdc_cmd_s *gdc_cmd)
{
	struct gdc_config_s *gc = NULL;

	if (gdc_cmd == NULL || start_addr == 0) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	gc = &gdc_cmd->gdc_config;

	switch (gc->format) {
	case NV12:
		gdc_cmd->y_base_addr = start_addr;
		gdc_cmd->uv_base_addr = start_addr +
			gc->input_y_stride * gc->input_height;
	break;
	case YV12:
		gdc_cmd->y_base_addr = start_addr;
		gdc_cmd->u_base_addr = start_addr +
			gc->input_y_stride * gc->input_height;
		gdc_cmd->v_base_addr = gdc_cmd->u_base_addr +
			gc->input_c_stride * gc->input_height / 2;
	break;
	case Y_GREY:
		gdc_cmd->y_base_addr = start_addr;
		gdc_cmd->u_base_addr = 0;
		gdc_cmd->v_base_addr = 0;
	break;
	case YUV444_P:
		gdc_cmd->y_base_addr = start_addr;
		gdc_cmd->u_base_addr = start_addr +
			gc->input_y_stride * gc->input_height;
		gdc_cmd->v_base_addr = gdc_cmd->u_base_addr +
			gc->input_c_stride * gc->input_height;
	break;
	case RGB444_P:
		gdc_cmd->y_base_addr = start_addr;
		gdc_cmd->u_base_addr = start_addr +
			gc->input_y_stride * gc->input_height;
		gdc_cmd->v_base_addr = gdc_cmd->u_base_addr +
			gc->input_c_stride * gc->input_height;
	break;
	default:
		gdc_log(LOG_ERR, "Error config format\n");
		return -EINVAL;
	break;
	}

	return 0;
}

static void meson_gdc_dma_flush(struct device *dev,
					dma_addr_t addr,
					size_t size)
{
	if (dev == NULL) {
		gdc_log(LOG_ERR, "Error input param");
		return;
	}

	dma_sync_single_for_device(dev, addr, size, DMA_TO_DEVICE);
}

static void meson_gdc_cache_flush(struct device *dev,
					dma_addr_t addr,
					size_t size)
{
	if (dev == NULL) {
		gdc_log(LOG_ERR, "Error input param");
		return;
	}

	dma_sync_single_for_cpu(dev, addr, size, DMA_FROM_DEVICE);
}

static int meson_gdc_dma_map(struct gdc_dma_cfg *cfg)
{
	int ret = -1;
	int fd = -1;
	struct dma_buf *dbuf = NULL;
	struct dma_buf_attachment *d_att = NULL;
	struct sg_table *sg = NULL;
	void *vaddr = NULL;
	struct device *dev = NULL;
	enum dma_data_direction dir;

	if (cfg == NULL || (cfg->fd < 0) || cfg->dev == NULL) {
		gdc_log(LOG_ERR, "Error input param");
		return -EINVAL;
	}

	fd = cfg->fd;
	dev = cfg->dev;
	dir = cfg->dir;

	dbuf = dma_buf_get(fd);
	if (dbuf == NULL) {
		gdc_log(LOG_ERR, "Failed to get dma buffer");
		return -EINVAL;
	}

	d_att = dma_buf_attach(dbuf, dev);
	if (d_att == NULL) {
		gdc_log(LOG_ERR, "Failed to set dma attach");
		goto attach_err;
	}

	sg = dma_buf_map_attachment(d_att, dir);
	if (sg == NULL) {
		gdc_log(LOG_ERR, "Failed to get dma sg");
		goto map_attach_err;
	}

	ret = dma_buf_begin_cpu_access(dbuf, dir);
	if (ret != 0) {
		gdc_log(LOG_ERR, "Failed to access dma buff");
		goto access_err;
	}

	vaddr = dma_buf_vmap(dbuf);
	if (vaddr == NULL) {
		gdc_log(LOG_ERR, "Failed to vmap dma buf");
		goto vmap_err;
	}

	cfg->dbuf = dbuf;
	cfg->attach = d_att;
	cfg->vaddr = vaddr;
	cfg->sg = sg;

	return ret;

vmap_err:
	dma_buf_end_cpu_access(dbuf, dir);

access_err:
	dma_buf_unmap_attachment(d_att, sg, dir);

map_attach_err:
	dma_buf_detach(dbuf, d_att);

attach_err:
	dma_buf_put(dbuf);

	return ret;
}

static void meson_gdc_dma_unmap(struct gdc_dma_cfg *cfg)
{
	int fd = -1;
	struct dma_buf *dbuf = NULL;
	struct dma_buf_attachment *d_att = NULL;
	struct sg_table *sg = NULL;
	void *vaddr = NULL;
	struct device *dev = NULL;
	enum dma_data_direction dir;

	if (cfg == NULL || (cfg->fd < 0) || cfg->dev == NULL
			|| cfg->dbuf == NULL || cfg->vaddr == NULL
			|| cfg->attach == NULL || cfg->sg == NULL) {
		gdc_log(LOG_ERR, "Error input param");
		return;
	}

	fd = cfg->fd;
	dev = cfg->dev;
	dir = cfg->dir;
	dbuf = cfg->dbuf;
	vaddr = cfg->vaddr;
	d_att = cfg->attach;
	sg = cfg->sg;

	dma_buf_vunmap(dbuf, vaddr);

	dma_buf_end_cpu_access(dbuf, dir);

	dma_buf_unmap_attachment(d_att, sg, dir);

	dma_buf_detach(dbuf, d_att);

	dma_buf_put(dbuf);
}

static int meson_gdc_init_dma_addr(
	struct gdc_context_s *context,
	struct gdc_settings *gs)
{
	int ret = -1;
	struct gdc_dma_cfg *dma_cfg = NULL;
	struct gdc_cmd_s *gdc_cmd = &context->cmd;
	struct gdc_config_s *gc = &gdc_cmd->gdc_config;

	if (context == NULL || gs == NULL) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	switch (gc->format) {
	case NV12:
		dma_cfg = &context->y_dma_cfg;
		memset(dma_cfg, 0, sizeof(*dma_cfg));
		dma_cfg->dir = DMA_TO_DEVICE;
		dma_cfg->dev = &gdc_manager.gdc_dev->pdev->dev;
		dma_cfg->fd = gs->y_base_fd;

		ret = meson_gdc_dma_map(dma_cfg);
		if (ret != 0) {
			gdc_log(LOG_ERR, "Failed to get map dma buff");
			return ret;
		}

		gdc_cmd->y_base_addr = virt_to_phys(dma_cfg->vaddr);

		dma_cfg = &context->uv_dma_cfg;
		memset(dma_cfg, 0, sizeof(*dma_cfg));
		dma_cfg->dir = DMA_TO_DEVICE;
		dma_cfg->dev = &gdc_manager.gdc_dev->pdev->dev;
		dma_cfg->fd = gs->uv_base_fd;

		ret = meson_gdc_dma_map(dma_cfg);
		if (ret != 0) {
			gdc_log(LOG_ERR, "Failed to get map dma buff");
			return ret;
		}

		gdc_cmd->uv_base_addr = virt_to_phys(dma_cfg->vaddr);
	break;
	case Y_GREY:
		dma_cfg = &context->y_dma_cfg;
		memset(dma_cfg, 0, sizeof(*dma_cfg));
		dma_cfg->dir = DMA_TO_DEVICE;
		dma_cfg->dev = &gdc_manager.gdc_dev->pdev->dev;
		dma_cfg->fd = gs->y_base_fd;

		ret = meson_gdc_dma_map(dma_cfg);
		if (ret != 0) {
			gdc_log(LOG_ERR, "Failed to get map dma buff");
			return ret;
		}
		gdc_cmd->y_base_addr = virt_to_phys(dma_cfg->vaddr);
		gdc_cmd->uv_base_addr = 0;
	break;
	default:
		gdc_log(LOG_ERR, "Error image format");
	break;
	}

	return ret;
}

static void meson_gdc_deinit_dma_addr(struct gdc_context_s *context)
{
	struct gdc_dma_cfg *dma_cfg = NULL;
	struct gdc_cmd_s *gdc_cmd = &context->cmd;
	struct gdc_config_s *gc = &gdc_cmd->gdc_config;

	if (context == NULL) {
		gdc_log(LOG_ERR, "Error input param\n");
		return;
	}

	switch (gc->format) {
	case NV12:
		dma_cfg = &context->y_dma_cfg;
		meson_gdc_dma_unmap(dma_cfg);

		dma_cfg = &context->uv_dma_cfg;
		meson_gdc_dma_unmap(dma_cfg);
	break;
	case Y_GREY:
		dma_cfg = &context->y_dma_cfg;
		meson_gdc_dma_unmap(dma_cfg);
	break;
	default:
		gdc_log(LOG_ERR, "Error image format");
	break;
	}
}

static int gdc_buffer_alloc(struct gdc_dmabuf_req_s *gdc_req_buf)
{
	struct device *dev;

	dev = &(gdc_manager.gdc_dev->pdev->dev);
	return gdc_dma_buffer_alloc(gdc_manager.buffer,
		dev, gdc_req_buf);
}

static int gdc_buffer_export(struct gdc_dmabuf_exp_s *gdc_exp_buf)
{
	return gdc_dma_buffer_export(gdc_manager.buffer, gdc_exp_buf);
}

static int gdc_buffer_free(int index)
{
	return gdc_dma_buffer_free(gdc_manager.buffer, index);

}

static void gdc_buffer_dma_flush(int dma_fd)
{
	struct device *dev;

	dev = &(gdc_manager.gdc_dev->pdev->dev);
	gdc_dma_buffer_dma_flush(dev, dma_fd);
}

static void gdc_buffer_cache_flush(int dma_fd)
{
	struct device *dev;

	dev = &(gdc_manager.gdc_dev->pdev->dev);
	gdc_dma_buffer_cache_flush(dev, dma_fd);
}

static int gdc_buffer_get_phys(struct aml_dma_cfg *cfg, unsigned long *addr)
{
	return gdc_dma_buffer_get_phys(gdc_manager.buffer, cfg, addr);
}

static int gdc_get_buffer_fd(int plane_id, struct gdc_buffer_info *buf_info)
{
	int fd;

	switch (plane_id) {
	case 0:
		fd = buf_info->y_base_fd;
		break;
	case 1:
		fd = buf_info->uv_base_fd;
		break;
	case 2:
		fd = buf_info->v_base_fd;
		break;
	default:
		gdc_log(LOG_ERR, "Error plane id\n");
		return -EINVAL;
	}
	return fd;
}

static int gdc_set_output_addr(int plane_id,
	u32 addr,
	struct gdc_cmd_s *gdc_cmd)
{
	switch (plane_id) {
	case 0:
		gdc_cmd->buffer_addr = addr;
		gdc_cmd->current_addr = addr;
		break;
	case 1:
		gdc_cmd->uv_out_base_addr = addr;
		break;
	case 2:
		gdc_cmd->v_out_base_addr = addr;
		break;
	default:
		gdc_log(LOG_ERR, "Error plane id\n");
		return -EINVAL;
	}
	return 0;
}

static int gdc_set_input_addr(int plane_id,
			      u32 addr,
			      struct gdc_cmd_s *gdc_cmd)
{
	struct gdc_config_s *gc = NULL;
	long size;

	if (gdc_cmd == NULL || addr == 0) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	gc = &gdc_cmd->gdc_config;

	switch (gc->format) {
	case NV12:
		if (plane_id == 0) {
			gdc_cmd->y_base_addr = addr;
			size = gc->input_y_stride * gc->input_height;
		} else if (plane_id == 1) {
			gdc_cmd->uv_base_addr = addr;
			size = gc->input_y_stride * gc->input_height / 2;
		} else
			gdc_log(LOG_ERR,
				"plane_id=%d is invalid for NV12\n",
				plane_id);
		break;
	case YV12:
		if (plane_id == 0) {
			gdc_cmd->y_base_addr = addr;
			size = gc->input_y_stride * gc->input_height;
		} else if (plane_id == 1) {
			gdc_cmd->u_base_addr = addr;
			size = gc->input_c_stride * gc->input_height / 2;
		} else if (plane_id == 2) {
			gdc_cmd->v_base_addr = addr;
			size = gc->input_c_stride * gc->input_height / 2;
		} else
			gdc_log(LOG_ERR,
				"plane_id=%d is invalid for YV12\n",
				plane_id);
		break;
	case Y_GREY:
		if (plane_id == 0) {
			gdc_cmd->y_base_addr = addr;
			gdc_cmd->u_base_addr = 0;
			gdc_cmd->v_base_addr = 0;
			size = gc->input_y_stride * gc->input_height;
		} else
			gdc_log(LOG_ERR,
				"plane_id=%d is invalid for Y_GREY\n",
				plane_id);
		break;
	case YUV444_P:
	case RGB444_P:
		size = gc->input_y_stride * gc->input_height;
		if (plane_id == 0)
			gdc_cmd->y_base_addr = addr;
		else if (plane_id == 1)
			gdc_cmd->u_base_addr = addr;
		else if (plane_id == 2)
			gdc_cmd->v_base_addr = addr;
		else
			gdc_log(LOG_ERR,
				"plane_id=%d is invalid for format=%d\n",
				plane_id, gc->format);
		break;
	default:
		gdc_log(LOG_ERR, "Error config format=%d\n", gc->format);
		return -EINVAL;
	}
	return 0;
}

static int gdc_get_input_size(struct gdc_cmd_s *gdc_cmd)
{
	struct gdc_config_s *gc = NULL;
	long size;

	if (gdc_cmd == NULL) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	gc = &gdc_cmd->gdc_config;

	switch (gc->format) {
	case NV12:
	case YV12:
		size = gc->input_y_stride * gc->input_height * 3 / 2;
		break;
	case Y_GREY:
		size = gc->input_y_stride * gc->input_height;
		break;
	case YUV444_P:
	case RGB444_P:
		size = gc->input_y_stride * gc->input_height * 3;
		break;
	default:
		gdc_log(LOG_ERR, "Error config format\n");
		return -EINVAL;
	}
	return 0;
}

static int gdc_process_input_dma_info(struct gdc_context_s *context,
				      struct gdc_settings_ex *gs_ex)
{
	int ret = -1;
	unsigned long addr;
	long size = 0;
	struct aml_dma_cfg *cfg = NULL;
	struct gdc_cmd_s *gdc_cmd = &context->cmd;
	int i, plane_number;

	if (context == NULL || gs_ex == NULL) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}
	if (gs_ex->input_buffer.plane_number < 1 ||
		gs_ex->input_buffer.plane_number > 3) {
		gdc_log(LOG_ERR, "%s, plane_number=%d invalid\n",
		__func__, gs_ex->input_buffer.plane_number);
		return -EINVAL;
	}

	plane_number = gs_ex->input_buffer.plane_number;
	for (i = 0; i < plane_number; i++) {
		context->dma_cfg.input_cfg[i].dma_used = 1;
		cfg = &context->dma_cfg.input_cfg[i].dma_cfg;
		cfg->fd = gdc_get_buffer_fd(i, &gs_ex->input_buffer);
		cfg->dev = &gdc_manager.gdc_dev->pdev->dev;
		cfg->dir = DMA_TO_DEVICE;
		ret = gdc_buffer_get_phys(cfg, &addr);
		if (ret < 0) {
			gdc_log(LOG_ERR,
				"dma import input fd %d err\n",
				cfg->fd);
			return -EINVAL;
		}
		if (plane_number == 1) {
			ret = meson_gdc_set_input_addr(addr, gdc_cmd);
			if (ret != 0) {
				gdc_log(LOG_ERR, "set input addr err\n");
				goto dma_buf_unmap;
			}
		} else {
			size = gdc_set_input_addr(i, addr, gdc_cmd);
			if (size < 0) {
				gdc_log(LOG_ERR, "set input addr err\n");
				goto dma_buf_unmap;
			}

		}
		gdc_log(LOG_DEBUG, "plane[%d] get input addr=%lx\n",
			i, addr);
		if (plane_number == 1) {
			size = gdc_get_input_size(gdc_cmd);
			if (size < 0) {
				gdc_log(LOG_ERR, "set input addr err\n");
				ret = -EINVAL;
				goto dma_buf_unmap;
			}
		}
		meson_gdc_dma_flush(&gdc_manager.gdc_dev->pdev->dev,
			addr, size);
	}
	return 0;

dma_buf_unmap:
	while (i >= 0) {
		cfg = &context->dma_cfg.input_cfg[i].dma_cfg;
		gdc_dma_buffer_unmap(cfg);
		context->dma_cfg.input_cfg[i].dma_used = 0;
		i--;
	}
	return ret;
}

static int gdc_process_output_dma_info(struct gdc_context_s *context,
				       struct gdc_settings_ex *gs_ex)
{
	int ret = -1;
	unsigned long addr;
	struct aml_dma_cfg *cfg = NULL;
	struct gdc_cmd_s *gdc_cmd = &context->cmd;
	int i, plane_number;

	if (context == NULL || gs_ex == NULL) {
		gdc_log(LOG_ERR, "Error output param\n");
		return -EINVAL;
	}
	if (gs_ex->output_buffer.plane_number < 1 ||
		gs_ex->output_buffer.plane_number > 3) {
		gs_ex->output_buffer.plane_number = 1;
		gdc_log(LOG_ERR, "%s, plane_number=%d invalid\n",
		__func__, gs_ex->output_buffer.plane_number);
	}

	plane_number = gs_ex->output_buffer.plane_number;
	for (i = 0; i < plane_number; i++) {
		context->dma_cfg.output_cfg[i].dma_used = 1;
		cfg = &context->dma_cfg.output_cfg[i].dma_cfg;
		cfg->fd = gdc_get_buffer_fd(i, &gs_ex->output_buffer);
		cfg->dev = &gdc_manager.gdc_dev->pdev->dev;
		cfg->dir = DMA_TO_DEVICE;
		ret = gdc_buffer_get_phys(cfg, &addr);
		if (ret < 0) {
			gdc_log(LOG_ERR,
				"dma import input fd %d err\n",
				cfg->fd);
			return -EINVAL;
		}
		if (plane_number == 1) {
			gdc_cmd->buffer_addr = addr;
			gdc_cmd->current_addr = gdc_cmd->buffer_addr;
		} else {
			ret = gdc_set_output_addr(i, addr, gdc_cmd);
			if (ret < 0) {
				gdc_log(LOG_ERR, "set input addr err\n");
				ret = -EINVAL;
				goto dma_buf_unmap;
			}
		}
		gdc_log(LOG_DEBUG, "plane[%d] get output addr=%lx\n",
			i, addr);
	}

	return 0;

dma_buf_unmap:
	while (i >= 0) {
		cfg = &context->dma_cfg.output_cfg[i].dma_cfg;
		gdc_dma_buffer_unmap(cfg);
		context->dma_cfg.output_cfg[i].dma_used = 0;
		i--;
	}

	return ret;

}

static int gdc_process_ex_info(struct gdc_context_s *context,
			       struct gdc_settings_ex *gs_ex)
{
	int ret;
	unsigned long addr = 0;
	struct aml_dma_cfg *cfg = NULL;
	struct gdc_cmd_s *gdc_cmd = &context->cmd;
	struct gdc_queue_item_s *pitem = NULL;
	int i;

	if (context == NULL || gs_ex == NULL) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}
	mutex_lock(&context->d_mutext);
	memcpy(&(gdc_cmd->gdc_config), &(gs_ex->gdc_config),
		sizeof(struct gdc_config_s));
	for (i = 0; i < GDC_MAX_PLANE; i++) {
		context->dma_cfg.input_cfg[i].dma_used = 0;
		context->dma_cfg.output_cfg[i].dma_used = 0;
	}
	context->dma_cfg.config_cfg.dma_used = 0;

	ret = gdc_process_output_dma_info(context, gs_ex);
	if (ret < 0) {
		ret = -EINVAL;
		goto unlock_return;
	}
	gdc_cmd->base_gdc = 0;

	context->dma_cfg.config_cfg.dma_used = 1;
	cfg = &context->dma_cfg.config_cfg.dma_cfg;
	cfg->fd = gs_ex->config_buffer.y_base_fd;
	cfg->dev = &gdc_manager.gdc_dev->pdev->dev;
	cfg->dir = DMA_TO_DEVICE;
	ret = gdc_buffer_get_phys(cfg, &addr);
	if (ret < 0) {
		gdc_log(LOG_ERR, "dma import config fd %d failed\n",
			gs_ex->config_buffer.shared_fd);
		ret = -EINVAL;
		goto unlock_return;
	}
	gdc_cmd->gdc_config.config_addr = addr;
	gdc_log(LOG_DEBUG, "%s, config addr=%lx\n", __func__, addr);

	ret = gdc_process_input_dma_info(context, gs_ex);
	if (ret < 0) {
		ret = -EINVAL;
		goto unlock_return;
	}
	gdc_cmd->outplane = gs_ex->output_buffer.plane_number;
	if (gdc_cmd->outplane < 1 || gdc_cmd->outplane > 3) {
		gdc_cmd->outplane = 1;
		gdc_log(LOG_ERR, "%s, plane_number=%d invalid\n",
		__func__, gs_ex->output_buffer.plane_number);
	}
	/* set block mode */
	context->cmd.wait_done_flag = 1;

	pitem = gdc_prepare_item(context);
	if (pitem == NULL) {
		gdc_log(LOG_ERR, "get item error\n");
		ret = -ENOMEM;
		goto unlock_return;
	}
	mutex_unlock(&context->d_mutext);
	if (gs_ex->config_buffer.mem_alloc_type == AML_GDC_MEM_DMABUF)
		gdc_buffer_dma_flush(gs_ex->config_buffer.shared_fd);

	gdc_wq_add_work(context, pitem);
	return 0;

unlock_return:
	mutex_unlock(&context->d_mutext);
	return ret;
}

static void release_config_firmware(struct fw_info_s *fw_info,
				    struct gdc_config_s *gdc_config)
{
	if (!fw_info || !gdc_config) {
		gdc_log(LOG_ERR, "NULL param, %s (%d)\n", __func__, __LINE__);
		return;
	}

	if (fw_info->virt_addr &&
	    gdc_config->config_size && gdc_config->config_addr) {
		dma_free_coherent(&gdc_manager.gdc_dev->pdev->dev,
				  gdc_config->config_size * 4,
				  fw_info->virt_addr,
				  gdc_config->config_addr);
	}
}

static int load_firmware_by_name(struct fw_info_s *fw_info,
				 struct gdc_config_s *gdc_config)
{
	int ret = -1;
	const struct firmware *fw = NULL;
	char *path = NULL;
	void __iomem *virt_addr = NULL;
	phys_addr_t phys_addr = 0;

	if (!fw_info || !fw_info->fw_name || !gdc_config) {
		gdc_log(LOG_ERR, "NULL param, %s (%d)\n", __func__, __LINE__);
		return -EINVAL;
	}

	gdc_log(LOG_DEBUG, "Try to load %s  ...\n", fw_info->fw_name);

	path = kzalloc(CONFIG_PATH_LENG, GFP_KERNEL);
	if (!path) {
		gdc_log(LOG_ERR, "cannot malloc fw_name!\n");
		return -ENOMEM;
	}
	snprintf(path, (CONFIG_PATH_LENG - 1), "%s/%s",
			FIRMWARE_DIR, fw_info->fw_name);

	ret = request_firmware(&fw, path, &gdc_manager.gdc_dev->pdev->dev);
	if (ret < 0) {
		gdc_log(LOG_DEBUG, "Error : %d can't load the %s.\n", ret, path);
		kfree(path);
		return -ENOENT;
	}

	if (fw->size <= 0) {
		gdc_log(LOG_ERR,
			"size error, wrong firmware or no enough mem.\n");
		ret = -ENOMEM;
		goto release;
	}

	virt_addr = dma_alloc_coherent(&gdc_manager.gdc_dev->pdev->dev, fw->size,
				       &phys_addr, GFP_DMA | GFP_KERNEL);
	if (!virt_addr) {
		gdc_log(LOG_ERR, "alloc config buffer failed\n");
		ret = -ENOMEM;
		goto release;
	}

	memcpy(virt_addr, (char *)fw->data, fw->size);

	gdc_log(LOG_DEBUG,
		"current firmware virt_addr: 0x%p, fw->data: 0x%p.\n",
		virt_addr, fw->data);

	gdc_config->config_addr = phys_addr;
	gdc_config->config_size = fw->size / 4;
	fw_info->virt_addr = virt_addr;

	gdc_log(LOG_DEBUG, "load firmware size : %zd, Name : %s.\n",
		fw->size, path);
	ret = fw->size;

release:
	release_firmware(fw);
	kfree(path);

	return ret;
}

static int gdc_process_with_fw(struct gdc_context_s *context,
			       struct gdc_settings_with_fw *gs_with_fw)
{
	int ret = -1;
	struct gdc_cmd_s *gdc_cmd = &context->cmd;
	char *fw_name = NULL;
	struct gdc_queue_item_s *pitem = NULL;

	if (context == NULL || gs_with_fw == NULL) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	gs_with_fw->fw_info.virt_addr = NULL;
	gs_with_fw->fw_info.cma_pages = NULL;

	fw_name = kzalloc(CONFIG_PATH_LENG, GFP_KERNEL);
	if (!fw_name) {
		gdc_log(LOG_ERR, "cannot malloc fw_name!\n");
		return -ENOMEM;
	}
	mutex_lock(&context->d_mutext);
	memcpy(&(gdc_cmd->gdc_config), &(gs_with_fw->gdc_config),
		sizeof(struct gdc_config_s));
	ret = gdc_process_output_dma_info(context,
					  (struct gdc_settings_ex *)gs_with_fw);
	if (ret < 0) {
		ret = -EINVAL;
		goto release_fw_name;
	}
	gdc_cmd->base_gdc = 0;

	ret =
	gdc_process_input_dma_info(context,
				   (struct gdc_settings_ex *)
				   gs_with_fw);
	if (ret < 0) {
		ret = -EINVAL;
		goto release_fw_name;
	}

	gdc_cmd->outplane = gs_with_fw->output_buffer.plane_number;
	if (gdc_cmd->outplane < 1 || gdc_cmd->outplane > 3) {
		gdc_cmd->outplane = 1;
		gdc_log(LOG_ERR, "%s, plane_number=%d invalid\n",
		__func__, gs_with_fw->output_buffer.plane_number);
	}

	/* load firmware */
	if (gs_with_fw->fw_info.fw_name != NULL) {
		ret = load_firmware_by_name(&gs_with_fw->fw_info,
					    &gs_with_fw->gdc_config);
		if (ret <= 0) {
			gdc_log(LOG_ERR, "line %d,load FW %s failed\n",
					__LINE__, gs_with_fw->fw_info.fw_name);
			ret = -EINVAL;
			goto release_fw;
		}
	}

	if (ret <= 0 || gs_with_fw->fw_info.fw_name == NULL) {
		char in_info[64] = {};
		char out_info[64] = {};
		char *format = NULL;
		struct fw_input_info_s *in = &gs_with_fw->fw_info.fw_input_info;
		struct fw_output_info_s *out =
					&gs_with_fw->fw_info.fw_output_info;
		union transform_u *trans =
				&gs_with_fw->fw_info.fw_output_info.trans;

		switch (gdc_cmd->gdc_config.format) {
		case NV12:
			format = "nv12";
			break;
		case YV12:
			format = "yv12";
			break;
		case Y_GREY:
			format = "ygrey";
			break;
		case YUV444_P:
			format = "yuv444p";
			break;
		case RGB444_P:
			format = "rgb444p";
			break;
		default:
			gdc_log(LOG_ERR, "unsupported gdc format\n");
			ret = -EINVAL;
			goto release_fw;
		}
		snprintf(in_info, (64 - 1), "%d_%d_%d_%d_%d_%d",
				in->with, in->height,
				in->fov, in->diameter,
				in->offsetX, in->offsetY);

		snprintf(out_info, (64 - 1), "%d_%d_%d_%d-%d_%d_%s",
				out->offsetX, out->offsetY,
				out->width, out->height,
				out->pan, out->tilt, out->zoom);

		switch (gs_with_fw->fw_info.fw_type) {
		case EQUISOLID:
			snprintf(fw_name, (CONFIG_PATH_LENG - 1),
					"equisolid-%s-%s-%s_%s_%d-%s.bin",
					in_info, out_info,
					trans->fw_equisolid.strengthX,
					trans->fw_equisolid.strengthY,
					trans->fw_equisolid.rotation,
					format);
			break;
		case CYLINDER:
			snprintf(fw_name, (CONFIG_PATH_LENG - 1),
					"cylinder-%s-%s-%s_%d-%s.bin",
					in_info, out_info,
					trans->fw_cylinder.strength,
					trans->fw_cylinder.rotation,
					format);
			break;
		case EQUIDISTANT:
			snprintf(fw_name, (CONFIG_PATH_LENG - 1),
				"equidistant-%s-%s-%s_%d_%d_%d_%d_%d_%d_%d-%s.bin",
					in_info, out_info,
					trans->fw_equidistant.azimuth,
					trans->fw_equidistant.elevation,
					trans->fw_equidistant.rotation,
					trans->fw_equidistant.fov_width,
					trans->fw_equidistant.fov_height,
					trans->fw_equidistant.keep_ratio,
					trans->fw_equidistant.cylindricityX,
					trans->fw_equidistant.cylindricityY,
					format);
			break;
		case CUSTOM:
			if (trans->fw_custom.fw_name != NULL) {
			snprintf(fw_name, (CONFIG_PATH_LENG - 1),
					"custom-%s-%s-%s-%s.bin",
					in_info, out_info,
					trans->fw_custom.fw_name,
					format);
			} else {
				gdc_log(LOG_ERR, "custom fw_name is NULL\n");
				ret = -EINVAL;
				goto release_fw;
			}
			break;
		case AFFINE:
			snprintf(fw_name, (CONFIG_PATH_LENG - 1),
					"affine-%s-%s-%d-%s.bin",
					in_info, out_info,
					trans->fw_affine.rotation,
					format);
			break;
		default:
			gdc_log(LOG_ERR, "unsupported FW type\n");
			ret = -EINVAL;
			goto release_fw;
		}

		gs_with_fw->fw_info.fw_name = fw_name;
	}
	ret = load_firmware_by_name(&gs_with_fw->fw_info,
				    &gs_with_fw->gdc_config);
	if (ret <= 0) {
		gdc_log(LOG_ERR, "line %d,load FW %s failed\n",
				__LINE__, gs_with_fw->fw_info.fw_name);
		ret = -EINVAL;
		goto release_fw;
	}

	gdc_cmd->gdc_config.config_addr =
		gs_with_fw->gdc_config.config_addr;
	gdc_cmd->gdc_config.config_size =
		gs_with_fw->gdc_config.config_size;

	/* set block mode */
	context->cmd.wait_done_flag = 1;

	pitem = gdc_prepare_item(context);
	if (pitem == NULL) {
		gdc_log(LOG_ERR, "get item error\n");
		ret = -ENOMEM;
		goto release_fw;
	}
	mutex_unlock(&context->d_mutext);
	gdc_wq_add_work(context, pitem);
	release_config_firmware(&gs_with_fw->fw_info, &gs_with_fw->gdc_config);
	kfree(fw_name);
	return 0;

release_fw:
	release_config_firmware(&gs_with_fw->fw_info, &gs_with_fw->gdc_config);
release_fw_name:
	mutex_unlock(&context->d_mutext);
	kfree(fw_name);

	return ret;
}
EXPORT_SYMBOL(gdc_process_with_fw);

int gdc_process_phys(struct gdc_context_s *context,
		     struct gdc_phy_setting *gs)
{
	int ret = -1, i;
	struct gdc_cmd_s *gdc_cmd = NULL;
	struct gdc_queue_item_s *pitem = NULL;
	struct fw_info_s fw_info;
	u32 plane_number;
	u32 format = 0;
	u32 i_width = 0, i_height = 0;
	u32 o_width = 0, o_height = 0;
	u32 i_y_stride = 0, i_c_stride = 0;
	u32 o_y_stride = 0, o_c_stride = 0;

	if (!context || !gs) {
		gdc_log(LOG_ERR, "NULL param, %s (%d)\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&context->d_mutext);
	gdc_cmd = &context->cmd;
	memset(gdc_cmd, 0, sizeof(struct gdc_cmd_s));
	memset(&fw_info, 0, sizeof(struct fw_info_s));

	/* set gdc_config */
	format = gs->format;
	i_width = gs->in_width;
	i_height = gs->in_height;
	o_width = gs->out_width;
	o_height = gs->out_height;
	i_y_stride = gs->in_y_stride;
	i_c_stride = gs->in_c_stride;
	o_y_stride = gs->out_y_stride;
	o_c_stride = gs->out_c_stride;

	/* if the input and output strides are all not set,
	 * calculations will be made based on the format and input/output size.
	 */
	if (!i_y_stride && !i_c_stride) {
		if (format == NV12 || format == YUV444_P ||
		    format == RGB444_P) {
			i_y_stride = AXI_WORD_ALIGN(i_width);
			i_c_stride = AXI_WORD_ALIGN(i_width);
		} else if (format == YV12) {
			i_c_stride = AXI_WORD_ALIGN(i_width / 2);
			i_y_stride = i_c_stride * 2;
		} else if (format == Y_GREY) {
			i_y_stride = AXI_WORD_ALIGN(i_width);
			i_c_stride = 0;
		} else {
			gdc_log(LOG_ERR, "Error unknown format\n");
			mutex_unlock(&context->d_mutext);
			return -EINVAL;
		}
	}

	if (!o_y_stride && !o_c_stride) {
		if (format == NV12 || format == YUV444_P ||
		    format == RGB444_P) {
			o_y_stride = AXI_WORD_ALIGN(o_width);
			o_c_stride = AXI_WORD_ALIGN(o_width);
		} else if (format == YV12) {
			o_c_stride = AXI_WORD_ALIGN(o_width / 2);
			o_y_stride = o_c_stride * 2;
		} else if (format == Y_GREY) {
			o_y_stride = AXI_WORD_ALIGN(o_width);
			o_c_stride = 0;
		} else {
			gdc_log(LOG_ERR, "Error unknown format\n");
			mutex_unlock(&context->d_mutext);
			return -EINVAL;
		}
	}

	gdc_log(LOG_DEBUG, "input, format:%d, width:%d, height:%d y_stride:%d c_stride:%d\n",
		format, i_width, i_height, i_y_stride, i_c_stride);
	gdc_log(LOG_DEBUG, "output, format:%d, width:%d, height:%d y_stride:%d c_stride:%d\n",
		format, o_width, o_height, o_y_stride, o_c_stride);

	gdc_cmd->gdc_config.format = format;
	gdc_cmd->gdc_config.input_width = i_width;
	gdc_cmd->gdc_config.input_height = i_height;
	gdc_cmd->gdc_config.input_y_stride = i_y_stride;
	gdc_cmd->gdc_config.input_c_stride = i_c_stride;
	gdc_cmd->gdc_config.output_width = o_width;
	gdc_cmd->gdc_config.output_height = o_height;
	gdc_cmd->gdc_config.output_y_stride = o_y_stride;
	gdc_cmd->gdc_config.output_c_stride = o_c_stride;
	if (!gs->use_builtin_fw) {
		gdc_cmd->gdc_config.config_addr = gs->config_paddr;
		gdc_cmd->gdc_config.config_size = gs->config_size;
	}
	gdc_cmd->outplane = gs->out_plane_num;

	/* output_addr */
	plane_number = gs->out_plane_num;
	if (plane_number < 1 || plane_number > 3) {
		plane_number = 1;
		gdc_log(LOG_ERR, "%s, input plane_number=%d invalid\n",
			__func__, plane_number);
	}

	for (i = 0; i < plane_number; i++) {
		if (plane_number == 1) {
			gdc_cmd->buffer_addr = gs->out_paddr[0];
			gdc_cmd->current_addr = gdc_cmd->buffer_addr;
		} else {
			ret = gdc_set_output_addr(i, gs->out_paddr[i], gdc_cmd);
			if (ret < 0) {
				gdc_log(LOG_ERR, "set input addr err\n");
				mutex_unlock(&context->d_mutext);
				return -EINVAL;
			}
		}
		gdc_log(LOG_DEBUG, "plane[%d] get output paddr=0x%x\n",
			i, gs->out_paddr[i]);
	}

	/* input_addr */
	plane_number = gs->in_plane_num;
	if (plane_number < 1 || plane_number > 3) {
		plane_number = 1;
		gdc_log(LOG_ERR, "%s, output plane_number=%d invalid\n",
			__func__, plane_number);
	}
	for (i = 0; i < plane_number; i++) {
		if (plane_number == 1) {
			ret = meson_gdc_set_input_addr(gs->in_paddr[0],
						       gdc_cmd);
			if (ret != 0) {
				gdc_log(LOG_ERR, "set input addr err\n");
				mutex_unlock(&context->d_mutext);
				return -EINVAL;
			}
		} else {
			ret = gdc_set_input_addr(i, gs->in_paddr[i], gdc_cmd);
			if (ret < 0) {
				gdc_log(LOG_ERR, "set input addr err\n");
				mutex_unlock(&context->d_mutext);
				return -EINVAL;
			}
		}
		gdc_log(LOG_DEBUG, "plane[%d] get input addr=0x%x\n",
			i, gs->in_paddr[i]);
	}

	/* config_addr */
	if (gs->use_builtin_fw) {
		fw_info.fw_name = gs->config_name;
		ret = load_firmware_by_name(&fw_info, &gdc_cmd->gdc_config);
		if (ret <= 0) {
			gdc_log(LOG_DEBUG, "line %d,load FW %s failed\n",
				__LINE__, fw_info.fw_name);
			ret = -EINVAL;
			mutex_unlock(&context->d_mutext);
			goto release_fw;
		}
	}

	/* set block mode */
	context->cmd.wait_done_flag = 1;
	pitem = gdc_prepare_item(context);
	if (!pitem) {
		gdc_log(LOG_ERR, "get item error\n");
		ret = -ENOMEM;
		mutex_unlock(&context->d_mutext);
		goto release_fw;
	}
	mutex_unlock(&context->d_mutext);
	gdc_wq_add_work(context, pitem);
release_fw:
	release_config_firmware(&fw_info, &gdc_cmd->gdc_config);

	return ret;
}
EXPORT_SYMBOL(gdc_process_phys);

static long meson_gdc_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int ret = -1;
	size_t len;
	struct gdc_context_s *context = NULL;
	struct gdc_settings gs;
	struct gdc_cmd_s *gdc_cmd = NULL;
	struct gdc_config_s *gc = NULL;
	struct gdc_buf_cfg buf_cfg;
	struct page *cma_pages = NULL;
	struct gdc_settings_ex gs_ex;
	struct gdc_settings_with_fw gs_with_fw;
	struct gdc_dmabuf_req_s gdc_req_buf;
	struct gdc_dmabuf_exp_s gdc_exp_buf;
	ion_phys_addr_t addr;
	int index, dma_fd;
	void __user *argp = (void __user *)arg;
	struct gdc_queue_item_s *pitem = NULL;
	struct device *dev = NULL;

	context = (struct gdc_context_s *)file->private_data;
	gdc_cmd = &context->cmd;
	gc = &gdc_cmd->gdc_config;
	switch (cmd) {
	case GDC_PROCESS:
		ret = copy_from_user(&gs, argp, sizeof(gs));
		if (ret < 0) {
			gdc_log(LOG_ERR, "copy from user failed\n");
			return -EINVAL;
		}

		gdc_log(LOG_DEBUG, "sizeof(gs)=%zu, magic=%d\n",
				sizeof(gs), gs.magic);

		//configure gdc config, buffer address and resolution
		ret = meson_ion_share_fd_to_phys(gdc_manager.ion_client,
				gs.out_fd, &addr, &len);
		if (ret < 0) {
			gdc_log(LOG_ERR,
				"import out fd %d failed\n", gs.out_fd);
			return -EINVAL;
		}
		mutex_lock(&context->d_mutext);
		memcpy(&gdc_cmd->gdc_config, &gs.gdc_config,
			sizeof(struct gdc_config_s));
		gdc_cmd->buffer_addr = addr;
		gdc_cmd->buffer_size = len;

		gdc_cmd->base_gdc = 0;
		gdc_cmd->current_addr = gdc_cmd->buffer_addr;

		ret = meson_ion_share_fd_to_phys(gdc_manager.ion_client,
				gc->config_addr, &addr, &len);
		if (ret < 0) {
			gdc_log(LOG_ERR, "import config fd failed\n");
			mutex_unlock(&context->d_mutext);
			return -EINVAL;
		}

		gc->config_addr = addr;

		ret = meson_ion_share_fd_to_phys(gdc_manager.ion_client,
				gs.in_fd, &addr, &len);
		if (ret < 0) {
			gdc_log(LOG_ERR, "import in fd %d failed\n", gs.in_fd);
			mutex_unlock(&context->d_mutext);
			return -EINVAL;
		}

		ret = meson_gdc_set_input_addr(addr, gdc_cmd);
		if (ret != 0) {
			gdc_log(LOG_ERR, "set input addr failed\n");
			mutex_unlock(&context->d_mutext);
			return -EINVAL;
		}

		gdc_cmd->outplane = 1;
		/* set block mode */
		context->cmd.wait_done_flag = 1;
		pitem = gdc_prepare_item(context);
		if (pitem == NULL) {
			gdc_log(LOG_ERR, "get item error\n");
			mutex_unlock(&context->d_mutext);
			return -ENOMEM;
		}
		mutex_unlock(&context->d_mutext);
		gdc_wq_add_work(context, pitem);
	break;
	case GDC_RUN:
		ret = copy_from_user(&gs, argp, sizeof(gs));
		if (ret < 0)
			gdc_log(LOG_ERR, "copy from user failed\n");
		mutex_lock(&context->d_mutext);
		memcpy(&gdc_cmd->gdc_config, &gs.gdc_config,
			sizeof(struct gdc_config_s));
		gdc_cmd->buffer_addr = context->o_paddr;
		gdc_cmd->buffer_size = context->o_len;

		gdc_cmd->base_gdc = 0;
		gdc_cmd->current_addr = gdc_cmd->buffer_addr;

		gc->config_addr = context->c_paddr;

		ret = meson_gdc_set_input_addr(context->i_paddr, gdc_cmd);
		if (ret != 0) {
			gdc_log(LOG_ERR, "set input addr failed\n");
			mutex_unlock(&context->d_mutext);
			return -EINVAL;
		}
		gdc_cmd->outplane = 1;
		/* set block mode */
		context->cmd.wait_done_flag = 1;
		pitem = gdc_prepare_item(context);
		if (pitem == NULL) {
			gdc_log(LOG_ERR, "get item error\n");
			mutex_unlock(&context->d_mutext);
			return -ENOMEM;
		}
		mutex_unlock(&context->d_mutext);
		dev = &gdc_manager.gdc_dev->pdev->dev;
		meson_gdc_dma_flush(dev,
					context->i_paddr, context->i_len);
		meson_gdc_dma_flush(dev,
					context->c_paddr, context->c_len);
		gdc_wq_add_work(context, pitem);
		meson_gdc_cache_flush(dev,
					context->o_paddr, context->o_len);
	break;
	case GDC_HANDLE:
		ret = copy_from_user(&gs, argp, sizeof(gs));
		if (ret < 0)
			gdc_log(LOG_ERR, "copy from user failed\n");
		mutex_lock(&context->d_mutext);
		memcpy(&gdc_cmd->gdc_config, &gs.gdc_config,
			sizeof(struct gdc_config_s));
		gdc_cmd->buffer_addr = context->o_paddr;
		gdc_cmd->buffer_size = context->o_len;

		gdc_cmd->base_gdc = 0;
		gdc_cmd->current_addr = gdc_cmd->buffer_addr;

		gc->config_addr = context->c_paddr;

		gdc_cmd->outplane = 1;
		/* set block mode */
		context->cmd.wait_done_flag = 1;
		ret = meson_gdc_init_dma_addr(context, &gs);
		if (ret != 0) {
			mutex_unlock(&context->d_mutext);
			gdc_log(LOG_ERR, "Failed to init dma addr");
			return ret;
		}
		pitem = gdc_prepare_item(context);
		if (pitem == NULL) {
			gdc_log(LOG_ERR, "get item error\n");
			mutex_unlock(&context->d_mutext);
			return -ENOMEM;
		}
		mutex_unlock(&context->d_mutext);

		dev = &gdc_manager.gdc_dev->pdev->dev;
		meson_gdc_dma_flush(dev,
					context->c_paddr, context->c_len);
		gdc_wq_add_work(context, pitem);
		meson_gdc_cache_flush(dev,
					context->o_paddr, context->o_len);
		meson_gdc_deinit_dma_addr(context);
	break;
	case GDC_REQUEST_BUFF:
		ret = copy_from_user(&buf_cfg, argp, sizeof(buf_cfg));
		if (ret < 0 || buf_cfg.type >= GDC_BUFF_TYPE_MAX) {
			gdc_log(LOG_ERR, "Error user param\n");
			return ret;
		}

		buf_cfg.len = PAGE_ALIGN(buf_cfg.len);
		dev = &gdc_manager.gdc_dev->pdev->dev;
		cma_pages = dma_alloc_from_contiguous
						(dev,
						buf_cfg.len >> PAGE_SHIFT, 0);
		if (cma_pages != NULL) {
			context->mmap_type = buf_cfg.type;
			ret = meson_gdc_set_buff(context,
				cma_pages, buf_cfg.len);
			if (ret != 0) {
				dma_release_from_contiguous(
						dev,
						cma_pages,
						buf_cfg.len >> PAGE_SHIFT);
				gdc_log(LOG_ERR, "Failed to set buff\n");
				return ret;
			}
		} else {
			gdc_log(LOG_ERR, "Failed to alloc dma buff\n");
			return -ENOMEM;
		}

	break;
	case GDC_PROCESS_WITH_FW:
		ret = copy_from_user(&gs_with_fw, argp, sizeof(gs_with_fw));
		if (ret < 0)
			gdc_log(LOG_ERR, "copy from user failed\n");
		memcpy(&gdc_cmd->gdc_config, &gs_with_fw.gdc_config,
			sizeof(struct gdc_config_s));
		ret = gdc_process_with_fw(context, &gs_with_fw);
		break;
	case GDC_PROCESS_EX:
		ret = copy_from_user(&gs_ex, argp, sizeof(gs_ex));
		if (ret < 0)
			gdc_log(LOG_ERR, "copy from user failed\n");
		ret = gdc_process_ex_info(context, &gs_ex);
		break;
	case GDC_REQUEST_DMA_BUFF:
		ret = copy_from_user(&gdc_req_buf, argp,
			sizeof(struct gdc_dmabuf_req_s));
		if (ret < 0) {
			pr_err("Error user param\n");
			return -EINVAL;
		}
		ret = gdc_buffer_alloc(&gdc_req_buf);
		if (ret == 0)
			ret = copy_to_user(argp, &gdc_req_buf,
				sizeof(struct gdc_dmabuf_req_s));
		break;
	case GDC_EXP_DMA_BUFF:
		ret = copy_from_user(&gdc_exp_buf, argp,
			sizeof(struct gdc_dmabuf_exp_s));
		if (ret < 0) {
			pr_err("Error user param\n");
			return -EINVAL;
		}
		ret = gdc_buffer_export(&gdc_exp_buf);
		if (ret == 0)
			ret = copy_to_user(argp, &gdc_exp_buf,
				sizeof(struct gdc_dmabuf_exp_s));
		break;
	case GDC_FREE_DMA_BUFF:
		ret = copy_from_user(&index, argp,
			sizeof(int));
		if (ret < 0) {
			pr_err("Error user param\n");
			return -EINVAL;
		}
		ret = gdc_buffer_free(index);
		break;
	case GDC_SYNC_DEVICE:
		ret = copy_from_user(&dma_fd, argp,
			sizeof(int));
		if (ret < 0) {
			pr_err("Error user param\n");
			return -EINVAL;
		}
		gdc_buffer_dma_flush(dma_fd);
		break;
	case GDC_SYNC_CPU:
		ret = copy_from_user(&dma_fd, argp,
			sizeof(int));
		if (ret < 0) {
			pr_err("Error user param\n");
			return -EINVAL;
		}
		gdc_buffer_cache_flush(dma_fd);
		break;
	default:
		gdc_log(LOG_ERR, "unsupported cmd 0x%x\n", cmd);
		return -EINVAL;
	break;
	}

	return ret;
}

static int meson_gdc_mmap(struct file *file_p,
				struct vm_area_struct *vma)
{
	int ret = -1;
	unsigned long buf_len = 0;
	struct gdc_context_s *context = NULL;

	buf_len = vma->vm_end - vma->vm_start;
	context = (struct gdc_context_s *)file_p->private_data;

	switch (context->mmap_type) {
	case INPUT_BUFF_TYPE:
		ret = remap_pfn_range(vma, vma->vm_start,
			context->i_paddr >> PAGE_SHIFT,
			buf_len, vma->vm_page_prot);
		if (ret != 0)
			gdc_log(LOG_ERR, "Failed to mmap input buffer\n");
	break;
	case OUTPUT_BUFF_TYPE:
		ret = remap_pfn_range(vma, vma->vm_start,
			context->o_paddr >> PAGE_SHIFT,
			buf_len, vma->vm_page_prot);
		if (ret != 0)
			gdc_log(LOG_ERR, "Failed to mmap input buffer\n");

	break;
	case CONFIG_BUFF_TYPE:
		ret = remap_pfn_range(vma, vma->vm_start,
			context->c_paddr >> PAGE_SHIFT,
			buf_len, vma->vm_page_prot);
		if (ret != 0)
			gdc_log(LOG_ERR, "Failed to mmap input buffer\n");
	break;
	default:
		gdc_log(LOG_ERR, "Error mmap type:0x%x\n", context->mmap_type);
	break;
	}

	return ret;
}

static const struct file_operations meson_gdc_fops = {
	.owner = THIS_MODULE,
	.open = meson_gdc_open,
	.release = meson_gdc_release,
	.unlocked_ioctl = meson_gdc_ioctl,
	.compat_ioctl = meson_gdc_ioctl,
	.mmap = meson_gdc_mmap,
};

static struct miscdevice meson_gdc_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "gdc",
	.fops	= &meson_gdc_fops,
};

static ssize_t gdc_dump_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int i;

	if (gdc_reg_store_mode) {
		len += sprintf(buf+len, "gdc adapter register below\n");
		for (i = 0; i <= 0xff; i += 4) {
			len += sprintf(buf+len,
					"\t[0xff950000 + 0x%08x, 0x%-8x\n",
						i, system_gdc_read_32(i));
		}
	} else {
		len += sprintf(buf+len,
				"err: please flow blow steps\n");
		len += sprintf(buf+len,
				"1. turn on dump mode, \"echo 1 > dump_reg\"\n");
		len += sprintf(buf+len,
				"2. run gdc to process\n");
		len += sprintf(buf+len,
				"3. show reg value, \"cat dump_reg\"\n");
	}

	return len;
}

static ssize_t gdc_dump_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{

	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);

	pr_info("dump mode: %d->%d\n", gdc_reg_store_mode, res);
	gdc_reg_store_mode = res;

	return len;
}
static DEVICE_ATTR(dump_reg, 0664, gdc_dump_reg_show, gdc_dump_reg_store);

static ssize_t firmware1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	gdc_log(LOG_DEBUG, "%s, %d\n", __func__, __LINE__);
	return 1;
}

static ssize_t firmware1_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	gdc_log(LOG_DEBUG, "%s, %d\n", __func__, __LINE__);
	//gdc_fw_init();
	return 1;
}
static DEVICE_ATTR(firmware1, 0664, firmware1_show, firmware1_store);

static ssize_t loglevel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf+len, "%d\n", gdc_log_level);
	return len;
}

static ssize_t loglevel_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("log_level: %d->%d\n", gdc_log_level, res);
	gdc_log_level = res;

	return len;
}

static DEVICE_ATTR(loglevel, 0664, loglevel_show, loglevel_store);

static ssize_t trace_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf+len, "trace_mode_enable: %d\n",
			trace_mode_enable);
	return len;
}

static ssize_t trace_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("trace_mode: %d->%d\n", trace_mode_enable, res);
	trace_mode_enable = res;

	return len;
}
static DEVICE_ATTR(trace_mode, 0664, trace_mode_show, trace_mode_store);

static ssize_t config_out_path_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	if (config_out_path_defined)
		len += sprintf(buf+len, "config out path: %s\n",
				config_out_file);
	else
		len += sprintf(buf+len, "config out path is not set\n");

	return len;
}

static ssize_t config_out_path_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	if (strlen(buf) >= CONFIG_PATH_LENG) {
		pr_info("err: path too long\n");
	} else {
		strncpy(config_out_file, buf, CONFIG_PATH_LENG - 1);
		config_out_path_defined = 1;
		pr_info("set config out path: %s\n", config_out_file);
	}

	return len;
}
static DEVICE_ATTR(config_out_path, 0664, config_out_path_show,
					config_out_path_store);

irqreturn_t gdc_interrupt_handler(int irq, void *param)
{
	complete(&gdc_manager.event.d_com);
	return IRQ_HANDLED;
}

static int gdc_platform_probe(struct platform_device *pdev)
{
	int rc = -1;
	struct resource *gdc_res;
	struct meson_gdc_dev_t *gdc_dev = NULL;
	void *pd_cntl = NULL;
	u32 reg_value = 0;

	// Initialize irq
	gdc_res = platform_get_resource(pdev,
			IORESOURCE_MEM, 0);
	if (!gdc_res) {
		gdc_log(LOG_ERR, "Error, no IORESOURCE_MEM DT!\n");
		return -ENOMEM;
	}

	if (init_gdc_io(pdev->dev.of_node) != 0) {
		gdc_log(LOG_ERR, "Error on mapping gdc memory!\n");
		return -ENOMEM;
	}

	rc = of_reserved_mem_device_init(&pdev->dev);
	if (rc != 0)
		gdc_log(LOG_INFO, "reserve_mem is not used\n");

	/* alloc mem to store config out path*/
	config_out_file = kzalloc(CONFIG_PATH_LENG, GFP_KERNEL);
	if (config_out_file == NULL) {
		gdc_log(LOG_ERR, "config out alloc failed\n");
		return -ENOMEM;
	}

	gdc_dev = devm_kzalloc(&pdev->dev, sizeof(*gdc_dev),
			GFP_KERNEL);

	if (gdc_dev == NULL) {
		gdc_log(LOG_DEBUG, "devm alloc gdc dev failed\n");
		return -ENOMEM;
	}

	gdc_dev->pdev = pdev;
	gdc_dev->misc_dev.minor = meson_gdc_dev.minor;
	gdc_dev->misc_dev.name = meson_gdc_dev.name;
	gdc_dev->misc_dev.fops = meson_gdc_dev.fops;

	gdc_dev->irq = platform_get_irq(pdev, 0);
	if (gdc_dev->irq < 0) {
		gdc_log(LOG_DEBUG, "cannot find irq for gdc\n");
		return -EINVAL;
	}

	/* mem_pd */
	pd_cntl = of_iomap(pdev->dev.of_node, 2);
	reg_value = ioread32(pd_cntl);
	gdc_log(LOG_DEBUG, "pd_cntl=%x\n", reg_value);
	reg_value = reg_value & (~(3<<18));
	gdc_log(LOG_DEBUG, "pd_cntl=%x\n", reg_value);
	iowrite32(reg_value, pd_cntl);

	/* core/axi clk */
	gdc_dev->clk_core =
		devm_clk_get(&pdev->dev, "core");
	if (IS_ERR(gdc_dev->clk_core)) {
		gdc_log(LOG_ERR, "cannot get gdc core clk\n");
	} else {
		clk_set_rate(gdc_dev->clk_core, CORE_CLK_RATE);
		clk_prepare_enable(gdc_dev->clk_core);
		rc =  clk_get_rate(gdc_dev->clk_core);
		gdc_log(LOG_INFO, "gdc core clk is %d MHZ\n", rc/1000000);
	}

	gdc_dev->clk_axi =
		devm_clk_get(&pdev->dev, "axi");
	if (IS_ERR(gdc_dev->clk_axi)) {
		gdc_log(LOG_ERR, "cannot get gdc axi clk\n");
	} else {
		clk_set_rate(gdc_dev->clk_axi, AXI_CLK_RATE);
		clk_prepare_enable(gdc_dev->clk_axi);
		rc =  clk_get_rate(gdc_dev->clk_axi);
		gdc_log(LOG_INFO, "gdc axi clk is %d MHZ\n", rc/1000000);
	}

	rc = devm_request_irq(&pdev->dev, gdc_dev->irq,
						gdc_interrupt_handler,
						IRQF_SHARED, "gdc", gdc_dev);
	if (rc != 0)
		gdc_log(LOG_ERR, "cannot create irq func gdc\n");

	gdc_wq_init(gdc_dev);

	rc = misc_register(&gdc_dev->misc_dev);
	if (rc < 0) {
		dev_err(&pdev->dev,
			"misc_register() for minor %d failed\n",
			gdc_dev->misc_dev.minor);
	}
	device_create_file(gdc_dev->misc_dev.this_device,
		&dev_attr_dump_reg);
	device_create_file(gdc_dev->misc_dev.this_device,
		&dev_attr_firmware1);
	device_create_file(gdc_dev->misc_dev.this_device,
		&dev_attr_loglevel);
	device_create_file(gdc_dev->misc_dev.this_device,
		&dev_attr_trace_mode);
	device_create_file(gdc_dev->misc_dev.this_device,
		&dev_attr_config_out_path);

	platform_set_drvdata(pdev, gdc_dev);
	gdc_pwr_config(false);

	gdc_manager.probed = 1;

	return rc;
}

static int gdc_platform_remove(struct platform_device *pdev)
{

	device_remove_file(meson_gdc_dev.this_device,
		&dev_attr_dump_reg);
	device_remove_file(meson_gdc_dev.this_device,
		&dev_attr_firmware1);
	device_remove_file(meson_gdc_dev.this_device,
		&dev_attr_loglevel);
	device_remove_file(meson_gdc_dev.this_device,
		&dev_attr_trace_mode);
	device_remove_file(meson_gdc_dev.this_device,
		&dev_attr_config_out_path);

	kfree(config_out_file);
	config_out_file = NULL;
	gdc_wq_deinit();

	misc_deregister(&meson_gdc_dev);
	return 0;
}

static struct platform_driver gdc_platform_driver = {
	.driver = {
		.name = "gdc",
		.owner = THIS_MODULE,
		.of_match_table = gdc_dt_match,
	},
	.probe	= gdc_platform_probe,
	.remove	= gdc_platform_remove,
};

module_platform_driver(gdc_platform_driver);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Amlogic Multimedia");
