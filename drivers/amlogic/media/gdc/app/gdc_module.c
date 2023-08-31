// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include <dev_ion.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-buf.h>
#include <linux/pm_runtime.h>
#include <linux/of_address.h>
#include <linux/timer.h>
#include <linux/ctype.h>

#ifdef CONFIG_AMLOGIC_FREERTOS
#include <linux/amlogic/freertos.h>
#endif

#include <api/gdc_api.h>
#include "system_log.h"

#include <linux/types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/firmware.h>
#include <linux/amlogic/power_domain.h>

//gdc configuration sequence
#include "gdc_config.h"
#include "gdc_dmabuf.h"
#include "gdc_wq.h"

#ifdef CONFIG_AMLOGIC_FREERTOS
struct timer_data_s {
	u32 dev_type;
	struct meson_gdc_dev_t *gdc_dev;
	struct timer_list timer;
	struct work_struct work;
};

static struct timer_data_s timer_data[HW_TYPE];
#define TIMER_MS (2000)
#endif

int gdc_log_level;
int gdc_smmu_enable;

int gdc_debug_enable;
int gdc_in_swap_endian;
int gdc_out_swap_endian;
int gdc_in_swap_64bit;
int gdc_out_swap_64bit;

struct gdc_manager_s gdc_manager;
static int kthread_created;
static struct gdc_irq_handle_wq irq_handle_wq[CORE_NUM];
static struct gdc_fw_func_ptr *g_fw_func_ptr;
static int fw_size_alloc = 100 * 1024; /* 100KB */

#define DEF_CLK_RATE 800000000

static struct gdc_device_data_s arm_gdc_clk2 = {
	.dev_type = ARM_GDC,
	.clk_type = CORE_AXI,
	.core_cnt = 1,
	.smmu_support = 0,
	.fw_version = ARMGDC_FW_V1,
	.endian_config = NO_ENDIAN_CONFIG
};

static struct gdc_device_data_s arm_gdc = {
	.dev_type = ARM_GDC,
	.clk_type = MUXGATE_MUXSEL_GATE,
	.ext_msb_8g = 1,
	.core_cnt = 1,
	.smmu_support = 0,
	.fw_version = ARMGDC_FW_V1,
	.endian_config = NO_ENDIAN_CONFIG
};

static struct gdc_device_data_s aml_gdc = {
	.dev_type = AML_GDC,
	.clk_type = MUXGATE_MUXSEL_GATE,
	.core_cnt = 1,
	.smmu_support = 0,
	.fw_version = AMLGDC_FW_V1,
	.endian_config = OLD_ENDIAN_CONFIG
};

static struct gdc_device_data_s aml_gdc_v2 = {
	.dev_type = AML_GDC,
	.clk_type = GATE,
	.bit_width_ext = 1,
	.gamma_support = 1,
	.core_cnt = 3,
	.smmu_support = 1,
	.fw_version = AMLGDC_FW_V1,
	.endian_config = NEW_ENDIAN_CONFIG
};

static struct gdc_device_data_s aml_gdc_v3 = {
	.dev_type = AML_GDC,
	.clk_type = MUXGATE_MUXSEL_GATE,
	.bit_width_ext = 1,
	.gamma_support = 0,
	.core_cnt = 1,
	.smmu_support = 0,
	.fw_version = AMLGDC_FW_V2,
	.endian_config = NEW_ENDIAN_CONFIG
};

static const struct of_device_id gdc_dt_match[] = {
	{.compatible = "amlogic, g12b-gdc", .data = &arm_gdc_clk2},
	{.compatible = "amlogic, arm-gdc",  .data = &arm_gdc},
	{} };

MODULE_DEVICE_TABLE(of, gdc_dt_match);

static const struct of_device_id amlgdc_dt_match[] = {
	{.compatible = "amlogic, aml-gdc",  .data = &aml_gdc},
	{.compatible = "amlogic, aml-gdc-v2",  .data = &aml_gdc_v2},
	{.compatible = "amlogic, aml-gdc-v3",  .data = &aml_gdc_v3},
	{} };

MODULE_DEVICE_TABLE(of, amlgdc_dt_match);

static char *clk_name[CLK_NAME_NUM][CORE_NUM] = {
	{"core", "core1", "core2"},
	{"axi",  "axi1",  "axi2"},
	{"mux_gate", "mux_gate1", "mux_gate2"},
	{"mux_sel",  "mux_sel1",  "mux_sel2"},
	{"clk_gate", "clk_gate1", "clk_gate2"}
};

static char *irq_name[HW_TYPE][CORE_NUM] = {
	{"gdc",     "gdc1",     "gdc2"},
	{"amlgdc",  "amlgdc1",  "amlgdc2"},
};

static struct gdc_irq_data_s irq_data[HW_TYPE][CORE_NUM];

static void meson_gdc_cache_flush(struct device *dev,
				  dma_addr_t addr,
				  size_t size);

static int meson_gdc_open(struct inode *inode, struct file *file)
{
	struct gdc_context_s *context = NULL;
	u32 minor = iminor(inode);
	u32 dev_type;

	if (gdc_manager.gdc_dev &&
	    minor == gdc_manager.gdc_dev->misc_dev.minor)
		dev_type = ARM_GDC;
	else
		dev_type = AML_GDC;

	/* we create one gdc  workqueue for this file handler. */
	context = create_gdc_work_queue(dev_type);
	if (!context) {
		gdc_log(LOG_ERR, "can't create work queue\n");
		return -1;
	}

	file->private_data = context;

	gdc_log(LOG_DEBUG, "Success open %s\n",
		dev_type == ARM_GDC ? "arm-gdc" : "aml-gdc");

	return 0;
}

static int meson_gdc_release(struct inode *inode, struct file *file)
{
	struct gdc_context_s *context = NULL;
	struct page *cma_pages = NULL;
	bool rc = false;
	int ret = 0;
	struct device *dev = NULL;

	context = (struct gdc_context_s *)file->private_data;
	if (!context) {
		gdc_log(LOG_ERR, "context is null,release work queue error\n");
		return -1;
	}

	dev = GDC_DEVICE(context->cmd.dev_type);

	if (context->i_kaddr != 0 && context->i_len != 0) {
		cma_pages = virt_to_page(context->i_kaddr);
		rc = dma_release_from_contiguous(dev,
						 cma_pages,
						 context->i_len >> PAGE_SHIFT);
		if (!rc) {
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
		if (!rc) {
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
		if (!rc) {
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
		gdc_log(LOG_DEBUG, "release one gdc device\n");

	return ret;
}

static int meson_gdc_set_buff(struct gdc_context_s *context,
			      struct page *cma_pages,
			      unsigned long len)
{
	int ret = 0;
	struct device *dev = NULL;

	if (!context || !cma_pages || len == 0) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	switch (context->mmap_type) {
	case INPUT_BUFF_TYPE:
		if (context->i_paddr != 0 && context->i_kaddr)
			return -EAGAIN;
		mutex_lock(&context->d_mutext);
		context->i_paddr = page_to_phys(cma_pages);
		context->i_kaddr = phys_to_virt(context->i_paddr);
		context->i_len = len;
		mutex_unlock(&context->d_mutext);
	break;
	case OUTPUT_BUFF_TYPE:
		dev = GDC_DEVICE(context->cmd.dev_type);

		if (context->o_paddr != 0 && context->o_kaddr)
			return -EAGAIN;
		mutex_lock(&context->d_mutext);
		context->o_paddr = page_to_phys(cma_pages);
		context->o_kaddr = phys_to_virt(context->o_paddr);
		context->o_len = len;
		mutex_unlock(&context->d_mutext);
		meson_gdc_cache_flush(dev,
				      context->o_paddr, context->o_len);
	break;
	case CONFIG_BUFF_TYPE:
		if (context->c_paddr != 0 && context->c_kaddr)
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

	if (!gdc_cmd || start_addr == 0) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	gc = &gdc_cmd->gdc_config;

	switch (gc->format & FORMAT_TYPE_MASK) {
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
	if (!dev) {
		gdc_log(LOG_ERR, "Error input param");
		return;
	}

	dma_sync_single_for_device(dev, addr, size, DMA_TO_DEVICE);
}

static void meson_gdc_cache_flush(struct device *dev,
				  dma_addr_t addr,
				  size_t size)
{
	if (!dev) {
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

	if (!cfg || cfg->fd < 0 || !cfg->dev) {
		gdc_log(LOG_ERR, "Error input param");
		return -EINVAL;
	}

	fd = cfg->fd;
	dev = cfg->dev;
	dir = cfg->dir;

	dbuf = dma_buf_get(fd);
	if (!dbuf) {
		gdc_log(LOG_ERR, "Failed to get dma buffer");
		return -EINVAL;
	}

	d_att = dma_buf_attach(dbuf, dev);
	if (!d_att) {
		gdc_log(LOG_ERR, "Failed to set dma attach");
		goto attach_err;
	}

	sg = dma_buf_map_attachment(d_att, dir);
	if (!sg) {
		gdc_log(LOG_ERR, "Failed to get dma sg");
		goto map_attach_err;
	}

	ret = dma_buf_begin_cpu_access(dbuf, dir);
	if (ret != 0) {
		gdc_log(LOG_ERR, "Failed to access dma buff");
		goto access_err;
	}

	vaddr = dma_buf_vmap(dbuf);
	if (!vaddr) {
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

	if (!cfg || cfg->fd < 0 || !cfg->dev ||
	    !cfg->dbuf || !cfg->vaddr ||
	    !cfg->attach || !cfg->sg) {
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

static int meson_gdc_init_dma_addr(struct gdc_context_s *context,
				   struct gdc_settings *gs)
{
	int ret = -1;
	struct gdc_dma_cfg *dma_cfg = NULL;
	struct gdc_cmd_s *gdc_cmd = &context->cmd;
	struct gdc_config_s *gc = &gdc_cmd->gdc_config;
	struct device *dev = NULL;

	if (!context || !gs) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	dev = GDC_DEVICE(context->cmd.dev_type);

	switch (gc->format) {
	case NV12:
		dma_cfg = &context->y_dma_cfg;
		memset(dma_cfg, 0, sizeof(*dma_cfg));
		dma_cfg->dir = DMA_TO_DEVICE;
		dma_cfg->dev = dev;
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
		dma_cfg->dev = dev;
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
		dma_cfg->dev = dev;
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

	if (!context) {
		gdc_log(LOG_ERR, "Error input param\n");
		return;
	}

	switch (gc->format & FORMAT_TYPE_MASK) {
	case NV12:
		/*coverity[Double free] free different dma_cfg*/
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

static int gdc_buffer_alloc(struct gdc_dmabuf_req_s *gdc_req_buf, u32 dev_type)
{
	struct device *dev;

	dev = GDC_DEVICE(dev_type);

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

static void gdc_buffer_dma_flush(int dma_fd, u32 dev_type)
{
	struct device *dev;

	dev = GDC_DEVICE(dev_type);

	gdc_dma_buffer_dma_flush(dev, dma_fd);
}

static void gdc_buffer_cache_flush(int dma_fd, u32 dev_type)
{
	struct device *dev;

	dev = GDC_DEVICE(dev_type);

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

	if (!gdc_cmd || addr == 0) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	gc = &gdc_cmd->gdc_config;

	switch (gc->format & FORMAT_TYPE_MASK) {
	case NV12:
		if (plane_id == 0) {
			gdc_cmd->y_base_addr = addr;
			size = gc->input_y_stride * gc->input_height;
		} else if (plane_id == 1) {
			gdc_cmd->uv_base_addr = addr;
			size = gc->input_y_stride * gc->input_height / 2;
		} else {
			gdc_log(LOG_ERR,
				"plane_id=%d is invalid for NV12\n",
				plane_id);
		}
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
		} else {
			gdc_log(LOG_ERR,
				"plane_id=%d is invalid for YV12\n",
				plane_id);
		}
		break;
	case Y_GREY:
		if (plane_id == 0) {
			gdc_cmd->y_base_addr = addr;
			gdc_cmd->u_base_addr = 0;
			gdc_cmd->v_base_addr = 0;
			size = gc->input_y_stride * gc->input_height;
		} else {
			gdc_log(LOG_ERR,
				"plane_id=%d is invalid for Y_GREY\n",
				plane_id);
		}
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

	if (!gdc_cmd) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	gc = &gdc_cmd->gdc_config;

	switch (gc->format & FORMAT_TYPE_MASK) {
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
	struct device *dev = NULL;

	if (!context || !gs_ex) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	dev = GDC_DEVICE(context->cmd.dev_type);

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
		cfg->dev = dev;
		cfg->dir = DMA_TO_DEVICE;
		cfg->is_config_buf = 0;
		ret = gdc_buffer_get_phys(cfg, &addr);
		if (ret < 0) {
			gdc_log(LOG_ERR,
				"dma import input fd %d err\n",
				cfg->fd);
			return -EINVAL;
		}

		if (plane_number == 1) {
			int j;

			/* set MSB val */
			for (j = 0; j < GDC_MAX_PLANE; j++)
				context->dma_cfg.input_cfg[j].paddr_8g_msb =
							(u64)addr >> 32;

			ret = meson_gdc_set_input_addr(addr, gdc_cmd);
			if (ret != 0) {
				gdc_log(LOG_ERR, "set input addr err\n");
				return ret;
			}
		} else {
			/* set MSB val */
			context->dma_cfg.input_cfg[i].paddr_8g_msb =
							(u64)addr >> 32;

			size = gdc_set_input_addr(i, addr, gdc_cmd);
			if (size < 0) {
				gdc_log(LOG_ERR, "set input addr err\n");
				return ret;
			}
		}
		gdc_log(LOG_DEBUG, "plane[%d] get input addr=%lx\n",
			i, addr);
		if (plane_number == 1) {
			size = gdc_get_input_size(gdc_cmd);
			if (size < 0) {
				gdc_log(LOG_ERR, "set input addr err\n");
				return ret;
			}
		}
		meson_gdc_dma_flush(dev, addr, size);
	}

	return 0;
}

static int gdc_process_output_dma_info(struct gdc_context_s *context,
				       struct gdc_settings_ex *gs_ex)
{
	int ret = -1;
	unsigned long addr;
	struct aml_dma_cfg *cfg = NULL;
	struct gdc_cmd_s *gdc_cmd = &context->cmd;
	int i, plane_number;
	struct device *dev = NULL;

	if (!context || !gs_ex) {
		gdc_log(LOG_ERR, "Error output param\n");
		return -EINVAL;
	}

	dev = GDC_DEVICE(context->cmd.dev_type);

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
		cfg->dev = dev;
		cfg->dir = DMA_FROM_DEVICE;
		cfg->is_config_buf = 0;
		ret = gdc_buffer_get_phys(cfg, &addr);
		if (ret < 0) {
			gdc_log(LOG_ERR,
				"dma import input fd %d err\n",
				cfg->fd);
			return -EINVAL;
		}

		if (plane_number == 1) {
			int j;

			/* set MSB val */
			for (j = 0; j < GDC_MAX_PLANE; j++)
				context->dma_cfg.output_cfg[j].paddr_8g_msb =
							(u64)addr >> 32;

			gdc_cmd->buffer_addr = addr;
			gdc_cmd->current_addr = gdc_cmd->buffer_addr;
		} else {
			/* set MSB val */
			context->dma_cfg.output_cfg[i].paddr_8g_msb =
							(u64)addr >> 32;

			ret = gdc_set_output_addr(i, addr, gdc_cmd);
			if (ret < 0) {
				gdc_log(LOG_ERR, "set input addr err\n");
				return ret;
			}
		}
		gdc_log(LOG_DEBUG, "plane[%d] get output addr=%lx\n",
			i, addr);
	}
	return 0;
}

static int gdc_process_ex_info(struct gdc_context_s *context,
			       struct gdc_settings_ex *gs_ex)
{
	int ret;
	unsigned long addr = 0;
	struct aml_dma_cfg *cfg = NULL;
	struct gdc_cmd_s *gdc_cmd = &context->cmd;
	struct gdc_queue_item_s *pitem = NULL;
	struct device *dev = NULL;
	int i;

	if (!context || !gs_ex) {
		gdc_log(LOG_ERR, "Error input param\n");
		return -EINVAL;
	}

	dev = GDC_DEVICE(context->cmd.dev_type);

	mutex_lock(&context->d_mutext);
	memcpy(&gdc_cmd->gdc_config, &gs_ex->gdc_config,
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
	cfg->dev = dev;
	cfg->dir = DMA_TO_DEVICE;
	cfg->is_config_buf = 1;
	ret = gdc_buffer_get_phys(cfg, &addr);
	if (ret < 0) {
		gdc_log(LOG_ERR, "dma import config fd %d failed\n",
			gs_ex->config_buffer.shared_fd);
		ret = -EINVAL;
		goto unlock_return;
	}
	/* set MSB val */
	context->dma_cfg.config_cfg.paddr_8g_msb = (u64)addr >> 32;

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
	if (!pitem) {
		gdc_log(LOG_ERR, "get item error\n");
		ret = -ENOMEM;
		goto unlock_return;
	}
	mutex_unlock(&context->d_mutext);
	if (gs_ex->config_buffer.mem_alloc_type == AML_GDC_MEM_DMABUF)
		gdc_buffer_dma_flush(gs_ex->config_buffer.shared_fd,
				     context->cmd.dev_type);

	gdc_wq_add_work(context, pitem);
	return 0;

unlock_return:
	mutex_unlock(&context->d_mutext);
	return ret;
}

void release_config_firmware(struct firmware_load_s *fw_load)
{
	struct device *dev = NULL;

	if (!fw_load) {
		gdc_log(LOG_ERR, "NULL param, %s (%d)\n", __func__, __LINE__);
		return;
	}

	if (is_aml_gdc_supported()) {
		dev = GDC_DEVICE(AML_GDC);
	} else if (is_gdc_supported()) {
		dev = GDC_DEVICE(ARM_GDC);
	} else {
		gdc_log(LOG_ERR, "HW is not supported %s (%d)\n",
			__func__, __LINE__);
		return;
	}

	if (fw_load->virt_addr &&
	    fw_load->size_32bit && fw_load->phys_addr) {
		dma_free_coherent(dev,
				  fw_load->size_32bit * 4,
				  fw_load->virt_addr,
				  fw_load->phys_addr);
	}
}
EXPORT_SYMBOL(release_config_firmware);

int load_firmware_by_name(char *fw_name, struct firmware_load_s *fw_load)
{
	int ret = -1;
	const struct firmware *fw = NULL;
	char *path = NULL;
	void __iomem *virt_addr = NULL;
	phys_addr_t phys_addr = 0;
	struct device *dev = NULL;

	if (!fw_name || !fw_load) {
		gdc_log(LOG_ERR, "NULL param, %s (%d)\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (is_aml_gdc_supported()) {
		dev = GDC_DEVICE(AML_GDC);
	} else if (is_gdc_supported()) {
		dev = GDC_DEVICE(ARM_GDC);
	} else {
		gdc_log(LOG_ERR, "HW is not supported %s (%d)\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	gdc_log(LOG_DEBUG, "Try to load %s  ...\n", fw_name);

	path = kzalloc(CONFIG_PATH_LENG, GFP_KERNEL);
	if (!path) {
		gdc_log(LOG_ERR, "cannot malloc fw_name!\n");
		return -ENOMEM;
	}
	snprintf(path, (CONFIG_PATH_LENG - 1), "%s/%s",
		 FIRMWARE_DIR, fw_name);

	ret = request_firmware(&fw, path, dev);
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

	virt_addr = dma_alloc_coherent(dev, fw->size,
				       &phys_addr, GFP_DMA | GFP_KERNEL);
	if (!virt_addr) {
		gdc_log(LOG_ERR, "alloc config buffer failed\n");
		ret = -ENOMEM;
		goto release;
	}

	memcpy(virt_addr, (char *)fw->data, fw->size);

	fw_load->size_32bit = fw->size / 4;
	fw_load->phys_addr = phys_addr;
	fw_load->virt_addr = virt_addr;

	gdc_log(LOG_DEBUG,
		"current firmware virt_addr: 0x%p, fw->data: 0x%p.\n",
		virt_addr, fw->data);

	gdc_log(LOG_DEBUG, "load firmware size : %zd, Name : %s.\n",
		fw->size, path);
	ret = fw->size;
release:
	release_firmware(fw);
	kfree(path);

	return ret;
}
EXPORT_SYMBOL(load_firmware_by_name);

int rotation_calc_and_load_firmware(struct firmware_rotate_s *fw_param,
				    struct firmware_load_s *fw_load)
{
	int ret = -1, fw_size = -1;
	void __iomem *virt_addr = NULL;
	phys_addr_t phys_addr = 0;
	struct device *dev = NULL;

	if (!fw_param || !fw_load) {
		gdc_log(LOG_ERR, "NULL param, %s (%d)\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (is_aml_gdc_supported()) {
		dev = GDC_DEVICE(AML_GDC);
	} else if (is_gdc_supported()) {
		dev = GDC_DEVICE(ARM_GDC);
	} else {
		gdc_log(LOG_ERR, "HW is not supported %s (%d)\n",
			__func__, __LINE__);
		return -ENODEV;
	}

	gdc_log(LOG_DEBUG, "Try to calculate and load firmware\n");
	gdc_log(LOG_DEBUG, "format:%d in w:%d h:%d, out w:%d h:%d, rotate:%d\n",
		fw_param->format, fw_param->in_width, fw_param->in_height,
		fw_param->out_width, fw_param->out_height,
		fw_param->degree);

	virt_addr = dma_alloc_coherent(dev, fw_size_alloc,
				       &phys_addr, GFP_DMA | GFP_KERNEL);
	if (!virt_addr) {
		gdc_log(LOG_ERR, "alloc config buffer failed\n");
		return -ENOMEM;
	}
	if (!g_fw_func_ptr) {
		gdc_log(LOG_ERR, "g_fw_func_ptr is NULL, gdc_fw.ko is not installed\n");
		ret = -EPERM;
		goto release;
	}

	fw_size = g_fw_func_ptr->calculate_rotate_fw(fw_param, virt_addr);
	if (fw_size <= 0) {
		gdc_log(LOG_ERR, "calculate config buffer failed\n");
		ret = -EINVAL;
		goto release;
	}

	fw_load->size_32bit = fw_size_alloc / 4;
	fw_load->phys_addr = phys_addr;
	fw_load->virt_addr = virt_addr;

	gdc_log(LOG_DEBUG, "current firmware virt_addr: 0x%p.\n", virt_addr);
	gdc_log(LOG_DEBUG, "load firmware size : %d.\n", fw_size);

	return fw_size;
release:
	dma_free_coherent(dev, fw_size_alloc, virt_addr, phys_addr);
	fw_load->size_32bit = 0;
	fw_load->phys_addr = 0;
	fw_load->virt_addr = NULL;

	return ret;
}
EXPORT_SYMBOL(rotation_calc_and_load_firmware);

int gdc_process_with_fw(struct gdc_context_s *context,
			struct gdc_settings_with_fw *gs_with_fw)
{
	int ret = -1;
	struct gdc_cmd_s *gdc_cmd = &context->cmd;
	char *fw_name = NULL;
	struct gdc_queue_item_s *pitem = NULL;
	struct firmware_load_s  fw_load;

	if (!context || !gs_with_fw) {
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
	memcpy(&gdc_cmd->gdc_config, &gs_with_fw->gdc_config,
	       sizeof(struct gdc_config_s));
	ret = gdc_process_output_dma_info
		(context,
		 (struct gdc_settings_ex *)gs_with_fw);
	if (ret < 0) {
		ret = -EINVAL;
		goto release_fw_name;
	}
	gdc_cmd->base_gdc = 0;

	ret = gdc_process_input_dma_info(context,
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

	/* load firmware:
	 * if fw_name is null, assign a name first according to params
	 * else load FW directly with given name
	 */
	if (!gs_with_fw->fw_info.fw_name) {
		char in_info[64] = {};
		char out_info[64] = {};
		char *format = NULL;
		struct fw_input_info_s *in = &gs_with_fw->fw_info.fw_input_info;
		struct fw_output_info_s *out =
					&gs_with_fw->fw_info.fw_output_info;
		union transform_u *trans =
				&gs_with_fw->fw_info.fw_output_info.trans;

		switch (gdc_cmd->gdc_config.format & FORMAT_TYPE_MASK) {
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
			goto release_fw_name;
		}
		snprintf(in_info, (64 - 1), "%d_%d_%d_%d_%d_%d",
			 in->with, in->height,
			 in->fov, in->diameter,
			 in->offset_x, in->offset_y);

		snprintf(out_info, (64 - 1), "%d_%d_%d_%d-%d_%d_%s",
			 out->offset_x, out->offset_y,
			 out->width, out->height,
			 out->pan, out->tilt, out->zoom);

		switch (gs_with_fw->fw_info.fw_type) {
		case EQUISOLID:
			snprintf(fw_name, (CONFIG_PATH_LENG - 1),
				 "equisolid-%s-%s-%s_%s_%d-%s.bin",
				 in_info, out_info,
				 trans->fw_equisolid.strength_x,
				 trans->fw_equisolid.strength_y,
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
				 trans->fw_equidistant.cylindricity_x,
				 trans->fw_equidistant.cylindricity_y,
				 format);
			break;
		case CUSTOM:
			if (trans->fw_custom.fw_name) {
				snprintf(fw_name, (CONFIG_PATH_LENG - 1),
					 "custom-%s-%s-%s-%s.bin",
					 in_info, out_info,
					 trans->fw_custom.fw_name,
					 format);
			} else {
				gdc_log(LOG_ERR, "custom fw_name is NULL\n");
				ret = -EINVAL;
				goto release_fw_name;
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
			goto release_fw_name;
		}

		gs_with_fw->fw_info.fw_name = fw_name;
	}

	ret = load_firmware_by_name(gs_with_fw->fw_info.fw_name,
				    &fw_load);
	if (ret <= 0) {
		gdc_log(LOG_ERR, "line %d,load FW %s failed\n",
			__LINE__, gs_with_fw->fw_info.fw_name);
		ret = -EINVAL;
		goto release_fw_name;
	}

	gdc_cmd->gdc_config.config_addr = fw_load.phys_addr;
	gdc_cmd->gdc_config.config_size = fw_load.size_32bit;

	/* set block mode */
	context->cmd.wait_done_flag = 1;

	pitem = gdc_prepare_item(context);
	if (!pitem) {
		gdc_log(LOG_ERR, "get item error\n");
		ret = -ENOMEM;
		goto release_fw;
	}
	mutex_unlock(&context->d_mutext);
	gdc_wq_add_work(context, pitem);
	release_config_firmware(&fw_load);
	kfree(fw_name);
	return 0;
release_fw:
	release_config_firmware(&fw_load);

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
	u32 dev_type = 0;

	if (!context || !gs) {
		gdc_log(LOG_ERR, "NULL param, %s (%d)\n", __func__, __LINE__);
		return -EINVAL;
	}

	dev_type = context->cmd.dev_type;

	mutex_lock(&context->d_mutext);
	gdc_cmd = &context->cmd;
	memset(gdc_cmd, 0, sizeof(struct gdc_cmd_s));
	memset(&fw_info, 0, sizeof(struct fw_info_s));

	/* set dev type, ARM_GDC or AML_GDC */
	context->cmd.dev_type = dev_type;

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

	gdc_log(LOG_DEBUG,
		"input, format:%d, width:%d, height:%d y_stride:%d c_stride:%d endian:0x%x\n",
		format, i_width, i_height, i_y_stride, i_c_stride, gs->in_endian);
	gdc_log(LOG_DEBUG,
		"output, format:%d, width:%d, height:%d y_stride:%d c_stride:%d endian:0x%x\n",
		format, o_width, o_height, o_y_stride, o_c_stride, gs->out_endian);

	gdc_cmd->gdc_config.format = format;
	gdc_cmd->gdc_config.input_width = i_width;
	gdc_cmd->gdc_config.input_height = i_height;
	gdc_cmd->gdc_config.input_y_stride = i_y_stride;
	gdc_cmd->gdc_config.input_c_stride = i_c_stride;
	gdc_cmd->gdc_config.output_width = o_width;
	gdc_cmd->gdc_config.output_height = o_height;
	gdc_cmd->gdc_config.output_y_stride = o_y_stride;
	gdc_cmd->gdc_config.output_c_stride = o_c_stride;
	gdc_cmd->gdc_config.config_addr = (u32)gs->config_paddr;
	gdc_cmd->gdc_config.config_size = gs->config_size;
	gdc_cmd->outplane = gs->out_plane_num;
	gdc_cmd->use_sec_mem = gs->use_sec_mem;
	gdc_cmd->in_endian = gs->in_endian;
	gdc_cmd->out_endian = gs->out_endian;

	/* set config_paddr MSB val */
	context->dma_cfg.config_cfg.paddr_8g_msb = (u64)gs->config_paddr >> 32;

	/* output_addr */
	plane_number = gs->out_plane_num;
	if (plane_number < 1 || plane_number > 3) {
		plane_number = 1;
		gdc_log(LOG_ERR, "%s, input plane_number=%d invalid\n",
			__func__, plane_number);
	}

	for (i = 0; i < plane_number; i++) {
		if (plane_number == 1) {
			int j;

			/* set MSB val */
			for (j = 0; j < GDC_MAX_PLANE; j++)
				context->dma_cfg.output_cfg[j].paddr_8g_msb =
						(u64)gs->out_paddr[0] >> 32;

			gdc_cmd->buffer_addr = gs->out_paddr[0];
			gdc_cmd->current_addr = gdc_cmd->buffer_addr;
		} else {
			/* set MSB val */
			context->dma_cfg.output_cfg[i].paddr_8g_msb =
						(u64)gs->out_paddr[i] >> 32;

			ret = gdc_set_output_addr(i, gs->out_paddr[i], gdc_cmd);
			if (ret < 0) {
				gdc_log(LOG_ERR, "set input addr err\n");
				mutex_unlock(&context->d_mutext);
				return -EINVAL;
			}
		}
		gdc_log(LOG_DEBUG, "plane[%d] get output paddr=0x%lx\n",
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
			int j;

			/* set MSB val */
			for (j = 0; j < GDC_MAX_PLANE; j++)
				context->dma_cfg.input_cfg[j].paddr_8g_msb =
						(u64)gs->in_paddr[0] >> 32;

			ret = meson_gdc_set_input_addr(gs->in_paddr[0],
						       gdc_cmd);
			if (ret != 0) {
				gdc_log(LOG_ERR, "set input addr err\n");
				mutex_unlock(&context->d_mutext);
				return -EINVAL;
			}
		} else {
			/* set MSB val */
			context->dma_cfg.input_cfg[i].paddr_8g_msb =
						(u64)gs->in_paddr[i] >> 32;

			ret = gdc_set_input_addr(i, gs->in_paddr[i], gdc_cmd);
			if (ret < 0) {
				gdc_log(LOG_ERR, "set input addr err\n");
				mutex_unlock(&context->d_mutext);
				return -EINVAL;
			}
		}
		gdc_log(LOG_DEBUG, "plane[%d] get input addr=0x%lx\n",
			i, gs->in_paddr[i]);
	}

	/* set block mode */
	context->cmd.wait_done_flag = 1;
	pitem = gdc_prepare_item(context);
	if (!pitem) {
		gdc_log(LOG_ERR, "get item error\n");
		mutex_unlock(&context->d_mutext);
		return -ENOMEM;
	}
	mutex_unlock(&context->d_mutext);
	gdc_wq_add_work(context, pitem);

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
	phys_addr_t addr;
	int index, dma_fd;
	void __user *argp = (void __user *)arg;
	struct gdc_queue_item_s *pitem = NULL;
	struct device *dev = NULL;
	int fw_version = -1;

	context = (struct gdc_context_s *)file->private_data;
	gdc_cmd = &context->cmd;
	gc = &gdc_cmd->gdc_config;

	dev = GDC_DEVICE(context->cmd.dev_type);
	fw_version = GDC_DEV_T(context->cmd.dev_type)->fw_version;

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
		ret = meson_ion_share_fd_to_phys(gs.out_fd,
						 &addr,
						 &len);
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

		ret = meson_ion_share_fd_to_phys(gc->config_addr,
						 &addr,
						 &len);
		if (ret < 0) {
			gdc_log(LOG_ERR, "import config fd failed\n");
			mutex_unlock(&context->d_mutext);
			return -EINVAL;
		}

		gc->config_addr = addr;

		ret = meson_ion_share_fd_to_phys(gs.in_fd,
						 &addr,
						 &len);
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
		if (!pitem) {
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
		if (!pitem) {
			gdc_log(LOG_ERR, "get item error\n");
			mutex_unlock(&context->d_mutext);
			return -ENOMEM;
		}
		mutex_unlock(&context->d_mutext);
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
		if (!pitem) {
			gdc_log(LOG_ERR, "get item error\n");
			mutex_unlock(&context->d_mutext);
			return -ENOMEM;
		}
		mutex_unlock(&context->d_mutext);

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
		cma_pages = dma_alloc_from_contiguous
						(dev,
						buf_cfg.len >> PAGE_SHIFT,
						0, 0);
		if (!cma_pages) {
			context->mmap_type = buf_cfg.type;
			ret = meson_gdc_set_buff(context,
						 cma_pages, buf_cfg.len);
			if (ret != 0) {
				dma_release_from_contiguous
					(dev,
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
		ret = gdc_buffer_alloc(&gdc_req_buf, context->cmd.dev_type);
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
		gdc_buffer_dma_flush(dma_fd, context->cmd.dev_type);
		break;
	case GDC_SYNC_CPU:
		ret = copy_from_user(&dma_fd, argp,
				     sizeof(int));
		if (ret < 0) {
			pr_err("Error user param\n");
			return -EINVAL;
		}
		gdc_buffer_cache_flush(dma_fd, context->cmd.dev_type);
		break;
	case GDC_GET_VERSION:
		if (fw_version < 0)
			gdc_log(LOG_ERR, "Invalid amlgdc_compatible\n");

		ret = copy_to_user(argp, &fw_version,
					sizeof(int));
		if (ret < 0) {
			pr_err("Error user param\n");
			return -EINVAL;
		}
		gdc_log(LOG_DEBUG, "fw_version = %d", fw_version);
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

static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	if (!token)
		return 0;
	len = strlen(token);
	do {
		token = strsep(&params, " ");
		while (token && (isspace(*token) ||
				 !isgraph(*token)) && len) {
			token++;
			len--;
		}
		if (len == 0 || !token)
			break;
		ret = kstrtoint(token, 0, &res);
		if (ret < 0)
			break;
		len = strlen(token);
		*out++ = res;
		count++;
	} while ((token) && (count < para_num) && (len > 0));

	kfree(params_base);
	return count;
}

static ssize_t dump_reg_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int core_id;
	struct meson_gdc_dev_t *gdc_dev =
				(struct meson_gdc_dev_t *)dev_get_drvdata(dev);
	u32 dev_type = 0;

	if (gdc_dev == gdc_manager.aml_gdc_dev)
		dev_type = AML_GDC;
	else
		dev_type = ARM_GDC;

	if (gdc_dev->reg_store_mode_enable) {
		for (core_id = 0; core_id < gdc_dev->core_cnt; core_id++) {
			if (gdc_dev->pd[core_id].status == 1) {
				dump_gdc_regs(dev_type, core_id);
			}
		}
	} else {
		len += sprintf(buf + len,
				"err: please flow blow steps\n");
		len += sprintf(buf + len,
				"1. turn on dump mode, \"echo 1 > dump_reg\"\n");
		len += sprintf(buf + len,
				"2. run gdc to process\n");
		len += sprintf(buf + len,
				"3. show reg value, \"cat dump_reg\"\n");
	}

	return len;
}

static ssize_t dump_reg_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t len)
{
	int res = 0;
	int ret = 0;
	struct meson_gdc_dev_t *gdc_dev =
				(struct meson_gdc_dev_t *)dev_get_drvdata(dev);

	ret = kstrtoint(buf, 0, &res);

	pr_info("dump mode: %d->%d\n", gdc_dev->reg_store_mode_enable, res);
	gdc_dev->reg_store_mode_enable = res;

	return len;
}
static DEVICE_ATTR_RW(dump_reg);

static ssize_t loglevel_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len, "%d\n", gdc_log_level);
	return len;
}

static ssize_t loglevel_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t len)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("log_level: %d->%d\n", gdc_log_level, res);
	gdc_log_level = res;

	return len;
}
static DEVICE_ATTR_RW(loglevel);

static ssize_t trace_mode_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct meson_gdc_dev_t *gdc_dev =
				(struct meson_gdc_dev_t *)dev_get_drvdata(dev);

	len += sprintf(buf + len, "trace_mode_enable: %d\n",
		       gdc_dev->trace_mode_enable);
	return len;
}

static ssize_t trace_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	int res = 0;
	int ret = 0;
	struct meson_gdc_dev_t *gdc_dev =
				(struct meson_gdc_dev_t *)dev_get_drvdata(dev);

	ret = kstrtoint(buf, 0, &res);
	pr_info("trace_mode: %d->%d\n", gdc_dev->trace_mode_enable, res);
	gdc_dev->trace_mode_enable = res;

	return len;
}
static DEVICE_ATTR_RW(trace_mode);

static ssize_t config_out_path_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct meson_gdc_dev_t *gdc_dev =
			(struct meson_gdc_dev_t *)dev_get_drvdata(dev);

	if (gdc_dev->config_out_path_defined)
		len += sprintf(buf + len, "config out path: %s\n",
			       gdc_dev->config_out_file);
	else
		len += sprintf(buf + len, "config out path is not set\n");

	return len;
}

static ssize_t config_out_path_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t len)
{
	struct meson_gdc_dev_t *gdc_dev =
			(struct meson_gdc_dev_t *)dev_get_drvdata(dev);

	if (strlen(buf) >= CONFIG_PATH_LENG) {
		pr_info("err: path too long\n");
	} else {
		strncpy(gdc_dev->config_out_file, buf, CONFIG_PATH_LENG - 1);
		gdc_dev->config_out_path_defined = 1;
		pr_info("set config out path: %s\n", gdc_dev->config_out_file);
	}

	return len;
}

static DEVICE_ATTR_RW(config_out_path);

static ssize_t debug_endian_show(struct device *device,
					struct device_attribute *attr,
					char *buf)
{
	return snprintf(buf, 80, "%d %d %d %d %d\n",
			gdc_debug_enable,
			gdc_in_swap_endian,
			gdc_out_swap_endian,
			gdc_in_swap_64bit,
			gdc_out_swap_64bit);
}

static ssize_t debug_endian_store(struct device *device,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int parsed[5];

	if (likely(parse_para(buf, 5, parsed) == 5)) {
		gdc_debug_enable    = parsed[0];
		gdc_in_swap_endian  = parsed[1];
		gdc_out_swap_endian = parsed[2];
		gdc_in_swap_64bit   = parsed[3];
		gdc_out_swap_64bit  = parsed[4];
	} else {
		pr_err("wrong params\n");
	}

	return count;
}

static DEVICE_ATTR_RW(debug_endian);

void irq_handle_func(struct work_struct *work)
{
	struct gdc_irq_handle_wq *irq_handle_wq =
		container_of(work, struct gdc_irq_handle_wq, work);
	struct gdc_queue_item_s *current_item = irq_handle_wq->current_item;
	u32 dev_type = current_item->cmd.dev_type;
	u32 core_id = current_item->core_id;
	u32 time_cost;
	struct meson_gdc_dev_t *gdc_dev = GDC_DEV_T(dev_type);
	u32 trace_mode_enable = gdc_dev->trace_mode_enable;

	current_item = gdc_dev->current_item[core_id];

	time_cost = gdc_time_cost(current_item);
	if (time_cost > GDC_WAIT_THRESHOLD)
		gdc_timeout_dump(current_item);
	else if (trace_mode_enable == 1)
		gdc_log(LOG_ERR, "core%d gdc process time = %d ms\n",
			core_id, time_cost);

	gdc_finish_item(current_item);
}

irqreturn_t gdc_interrupt_handler(int irq, void *param)
{
	u32 dev_type = ((struct gdc_irq_data_s *)param)->dev_type;
	u32 core_id = ((struct gdc_irq_data_s *)param)->core_id;
	struct gdc_queue_item_s *current_item = NULL;

	current_item = GDC_DEV_T(dev_type)->current_item[core_id];

	irq_handle_wq[core_id].current_item = current_item;
	schedule_work(&irq_handle_wq[core_id].work);

	return IRQ_HANDLED;
}

static void gdc_irq_init(struct meson_gdc_dev_t *gdc_dev, u32 dev_type)
{
	int i, rc = 0, irq;

	if (!gdc_dev || !gdc_dev->pdev) {
		gdc_log(LOG_ERR, "%s, wrong param\n", __func__);
		return;
	}
	/* irq init */
	for (i = 0; i < gdc_dev->core_cnt; i++) {
		irq = platform_get_irq(gdc_dev->pdev, i);
		if (irq < 0) {
			gdc_log(LOG_ERR, "cannot find %d irq for gdc\n", i);
			return;
		}

		gdc_log(LOG_DEBUG, "request irq:%s\n",
			irq_name[dev_type][i]);
		irq_data[dev_type][i].dev_type = dev_type;
		irq_data[dev_type][i].core_id = i;
		rc = devm_request_irq(&gdc_dev->pdev->dev, irq,
				      gdc_interrupt_handler,
				      IRQF_SHARED,
				      irq_name[dev_type][i],
				      &irq_data[dev_type][i]);
		if (rc != 0) {
			gdc_log(LOG_ERR, "cannot create %d irq func gdc\n", i);
			return;
		}
	}
}

#ifdef CONFIG_AMLOGIC_FREERTOS
static void work_func(struct work_struct *w)
{
	struct timer_data_s *timer_data =
		container_of(w, struct timer_data_s, work);
	u32 dev_type = timer_data->dev_type;

	if (dev_type == ARM_GDC)
		gdc_log(LOG_DEBUG, "gdc %s enter\n", __func__);
	else
		gdc_log(LOG_DEBUG, "amlgdc %s enter\n", __func__);

	gdc_runtime_pwr_all(dev_type, 0);
	gdc_clk_config_all(dev_type, 0);
	gdc_irq_init(timer_data->gdc_dev, dev_type);
}

static void timer_expire(struct timer_list *t)
{
	struct timer_data_s *timer_data = from_timer(timer_data, t, timer);
	u32 dev_type = timer_data->dev_type;

	if (dev_type == ARM_GDC)
		gdc_log(LOG_DEBUG, "gdc %s enter\n", __func__);
	else
		gdc_log(LOG_DEBUG, "amlgdc %s enter\n", __func__);

	if (freertos_is_finished()) {
		del_timer(&timer_data->timer);
		/* might sleep, use work to process */
		schedule_work(&timer_data->work);
	} else {
		mod_timer(&timer_data->timer,
			  jiffies + msecs_to_jiffies(TIMER_MS));
	}
}
#endif

static int gdc_platform_probe(struct platform_device *pdev)
{
	int rc = -1, clk_rate = 0, i = 0;
	struct resource *gdc_res;
	struct meson_gdc_dev_t *gdc_dev = NULL;
	const struct of_device_id *match;
	struct gdc_device_data_s *gdc_data;
	const char *drv_name = pdev->dev.driver->name;
	char *config_out_file;
	u32 dev_type;

	match = of_match_node(gdc_dt_match, pdev->dev.of_node);
	if (!match) {
		match = of_match_node(amlgdc_dt_match, pdev->dev.of_node);
		if (!match) {
			pr_err("%s NOT match\n", __func__);
			return -ENODEV;
		}
	}

	gdc_data = (struct gdc_device_data_s *)match->data;
	dev_type = gdc_data->dev_type;
	rc = of_reserved_mem_device_init(&pdev->dev);
	if (rc != 0)
		gdc_log(LOG_INFO, "reserve_mem is not used\n");

	/* alloc mem to store config out path*/
	config_out_file = kzalloc(CONFIG_PATH_LENG, GFP_KERNEL);
	if (!config_out_file) {
		gdc_log(LOG_ERR, "config out alloc failed\n");
		return -ENOMEM;
	}

	gdc_dev = devm_kzalloc(&pdev->dev, sizeof(*gdc_dev),
			       GFP_KERNEL);

	if (!gdc_dev) {
		gdc_log(LOG_DEBUG, "devm alloc gdc dev failed\n");
		rc = -ENOMEM;
		goto free_config;
	}

	if (dev_type == ARM_GDC)
		gdc_manager.gdc_dev = gdc_dev;
	else
		gdc_manager.aml_gdc_dev = gdc_dev;

	gdc_dev->config_out_file = config_out_file;
	gdc_dev->clk_type = gdc_data->clk_type;
	gdc_dev->bit_width_ext = gdc_data->bit_width_ext;
	gdc_dev->gamma_support = gdc_data->gamma_support;
	gdc_dev->pdev = pdev;
	gdc_dev->ext_msb_8g = gdc_data->ext_msb_8g;
	gdc_dev->core_cnt = gdc_data->core_cnt;
	gdc_dev->fw_version = gdc_data->fw_version;
	gdc_dev->endian_config = gdc_data->endian_config;

	gdc_dev->misc_dev.minor = MISC_DYNAMIC_MINOR;
	gdc_dev->misc_dev.name = drv_name;
	gdc_dev->misc_dev.fops = &meson_gdc_fops;

	for (i = 0; i < gdc_dev->core_cnt; i++) {
		gdc_res = platform_get_resource(pdev,
						IORESOURCE_MEM, i);
		if (!gdc_res) {
			gdc_log(LOG_ERR, "Error, no IORESOURCE_MEM DT!\n");
			goto free_config;
		}
	}

	if (init_gdc_io(pdev->dev.of_node, dev_type) != 0) {
		gdc_log(LOG_ERR, "Error on mapping gdc memory!\n");
		goto free_config;
	}

#ifndef CONFIG_AMLOGIC_FREERTOS
	gdc_irq_init(gdc_dev, dev_type);
#endif

	if (gdc_data->smmu_support &&
	    of_find_property(pdev->dev.of_node, "iommus", NULL)) {
		gdc_smmu_enable = 1;
		gdc_log(LOG_DEBUG, "enable smmu_support\n");
	}

	rc = of_property_read_u32(pdev->dev.of_node, "clk-rate", &clk_rate);
	if (rc < 0)
		clk_rate = DEF_CLK_RATE;

	for (i = 0; i < gdc_dev->core_cnt; i++) {
		if (gdc_data->clk_type == CORE_AXI) {
			/* core clk */
			gdc_dev->clk_core[i] = devm_clk_get(&pdev->dev,
							    clk_name[CORE][i]);
			if (IS_ERR(gdc_dev->clk_core[i])) {
				gdc_log(LOG_ERR, "cannot get gdc core clk\n");
			} else {
				clk_set_rate(gdc_dev->clk_core[i], clk_rate);
				clk_prepare_enable(gdc_dev->clk_core[i]);
				rc = clk_get_rate(gdc_dev->clk_core[i]);
				gdc_log(LOG_INFO, "%s core clk is %d MHZ\n",
					clk_name[CORE][i], rc / 1000000);
			}

			/* axi clk */
			gdc_dev->clk_axi[i] = devm_clk_get(&pdev->dev,
							   clk_name[AXI][i]);
			if (IS_ERR(gdc_dev->clk_axi[i])) {
				gdc_log(LOG_ERR, "cannot get gdc axi clk\n");
			} else {
				clk_set_rate(gdc_dev->clk_axi[i], clk_rate);
				clk_prepare_enable(gdc_dev->clk_axi[i]);
				rc = clk_get_rate(gdc_dev->clk_axi[i]);
				gdc_log(LOG_INFO, "%s axi clk is %d MHZ\n",
					clk_name[AXI][i], rc / 1000000);
			}
		} else if (gdc_data->clk_type == MUXGATE_MUXSEL_GATE) {
			struct clk *mux_gate = NULL;
			struct clk *mux_sel = NULL;

			/* mux_gate */
			mux_gate = devm_clk_get(&pdev->dev,
						clk_name[MUX_GATE][i]);
			if (IS_ERR(mux_gate))
				gdc_log(LOG_ERR, "cannot get gdc mux_gate\n");

			/* mux_sel */
			mux_sel = devm_clk_get(&pdev->dev,
					       clk_name[MUX_SEL][i]);
			if (IS_ERR(mux_gate))
				gdc_log(LOG_ERR, "cannot get gdc mux_sel\n");

			clk_set_parent(mux_sel, mux_gate);

			/* clk_gate */
			gdc_dev->clk_gate[i] =
				devm_clk_get(&pdev->dev, clk_name[CLK_GATE][i]);
			if (IS_ERR(gdc_dev->clk_gate[i])) {
				gdc_log(LOG_ERR, "cannot get gdc clk_gate\n");
			} else {
				clk_set_rate(gdc_dev->clk_gate[i], clk_rate);
				clk_prepare_enable(gdc_dev->clk_gate[i]);
				rc = clk_get_rate(gdc_dev->clk_gate[i]);
				gdc_log(LOG_INFO, "%s clk_gate is %d MHZ\n",
					clk_name[CLK_GATE][i], rc / 1000000);
			}
		} else if (gdc_data->clk_type == GATE) {
			/* clk_gate */
			gdc_dev->clk_gate[i] =
				devm_clk_get(&pdev->dev, clk_name[CLK_GATE][i]);
			if (IS_ERR(gdc_dev->clk_gate[i])) {
				gdc_log(LOG_ERR, "cannot get gdc clk_gate\n");
			} else {
				clk_set_rate(gdc_dev->clk_gate[i], clk_rate);
				clk_prepare_enable(gdc_dev->clk_gate[i]);
				rc = clk_get_rate(gdc_dev->clk_gate[i]);
				gdc_log(LOG_INFO, "%s clk_gate is %d MHZ\n",
					clk_name[CLK_GATE][i], rc / 1000000);
			}
		}
	}

	/* for smmu: default dma_mask has been set in of_dma_configure */
	if (!gdc_smmu_enable) {
		/* 8g memory support */
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
	}
	rc = misc_register(&gdc_dev->misc_dev);
	if (rc < 0) {
		gdc_log(LOG_ERR, "misc_register() for minor %d failed\n",
			gdc_dev->misc_dev.minor);
		goto free_config;
	}
	device_create_file(gdc_dev->misc_dev.this_device,
			   &dev_attr_dump_reg);
	device_create_file(gdc_dev->misc_dev.this_device,
			   &dev_attr_loglevel);
	device_create_file(gdc_dev->misc_dev.this_device,
			   &dev_attr_trace_mode);
	device_create_file(gdc_dev->misc_dev.this_device,
			   &dev_attr_config_out_path);
	device_create_file(gdc_dev->misc_dev.this_device,
			   &dev_attr_debug_endian);

	platform_set_drvdata(pdev, gdc_dev);
	dev_set_drvdata(gdc_dev->misc_dev.this_device, gdc_dev);

	for (i = 0; i < gdc_dev->core_cnt; i++)
		GDC_DEV_T(dev_type)->is_idle[i] = 1;

	/* power domain init */
	rc = gdc_pwr_init(&pdev->dev, gdc_dev->pd, dev_type);
	if (rc) {
		gdc_pwr_remove(gdc_dev->pd);
		gdc_log(LOG_ERR,
			"power domain init failed %d\n",
			rc);
		goto free_config;
	}

#ifdef CONFIG_AMLOGIC_FREERTOS
	/* To avoid interfering with RTOS clock/power/interrupt resources,
	 * turn on and keep power/clk first.
	 * Poll the status of rtos,
	 * then turn off power/clk and register interrupt.
	 */
	if (freertos_is_run() && !freertos_is_finished()) {
		INIT_WORK(&timer_data[dev_type].work, work_func);
		timer_setup(&timer_data[dev_type].timer, timer_expire, 0);
		timer_data[dev_type].timer.expires =
					jiffies + msecs_to_jiffies(TIMER_MS);
		timer_data[dev_type].gdc_dev = gdc_dev;
		timer_data[dev_type].dev_type = dev_type;
		add_timer(&timer_data[dev_type].timer);

		gdc_runtime_pwr_all(dev_type, 1);
	} else {
		gdc_irq_init(gdc_dev, dev_type);
		gdc_clk_config_all(dev_type, 0);
	}
#else
	gdc_clk_config_all(dev_type, 0);
#endif

	if (!kthread_created) {
		gdc_wq_init();

		for (i = 0; i < gdc_dev->core_cnt; i++)
			INIT_WORK(&irq_handle_wq[i].work, irq_handle_func);
		kthread_created = 1;
	}

	if (dev_type == ARM_GDC)
		gdc_manager.gdc_dev->probed = 1;
	else
		gdc_manager.aml_gdc_dev->probed = 1;

	return 0;

free_config:
	kfree(config_out_file);

	return rc;
}

static int gdc_platform_remove(struct platform_device *pdev)
{
	struct meson_gdc_dev_t *gdc_dev =
			(struct meson_gdc_dev_t *)platform_get_drvdata(pdev);
	struct miscdevice *misc_dev = &gdc_dev->misc_dev;

	device_remove_file(misc_dev->this_device,
			   &dev_attr_dump_reg);
	device_remove_file(misc_dev->this_device,
			   &dev_attr_loglevel);
	device_remove_file(misc_dev->this_device,
			   &dev_attr_trace_mode);
	device_remove_file(misc_dev->this_device,
			   &dev_attr_config_out_path);

	kfree(gdc_dev->config_out_file);
	gdc_dev->config_out_file = NULL;
	gdc_wq_deinit();
	gdc_pwr_remove(gdc_dev->pd);
	misc_deregister(misc_dev);
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

static struct platform_driver amlgdc_platform_driver = {
	.driver = {
		.name = "amlgdc",
		.owner = THIS_MODULE,
		.of_match_table = amlgdc_dt_match,
	},
	.probe	= gdc_platform_probe,
	.remove	= gdc_platform_remove,
};

int register_gdc_fw_funcs(struct gdc_fw_func_ptr *func_ptr, char *version)
{
	if (!func_ptr || !version) {
		gdc_log(LOG_ERR, "func_ptr:%p version:%p\n",
			 func_ptr, version);
		return -1;
	}

	gdc_log(LOG_INFO, "%s, func_ptr:%p version:%s\n", __func__,
		func_ptr, version);
	g_fw_func_ptr = func_ptr;

	return 0;
}
EXPORT_SYMBOL(register_gdc_fw_funcs);

void unregister_gdc_fw_funcs(void)
{
	gdc_log(LOG_INFO, "%s\n", __func__);
	g_fw_func_ptr = NULL;
}
EXPORT_SYMBOL(unregister_gdc_fw_funcs);

int __init gdc_driver_init(void)
{
	int ret = -1;

	ret = platform_driver_register(&gdc_platform_driver);
	if (ret) {
		gdc_log(LOG_ERR, "gdc driver register error\n");
		return -ENODEV;
	}

	ret = platform_driver_register(&amlgdc_platform_driver);
	if (ret) {
		gdc_log(LOG_ERR, "aml gdc driver register error\n");
		return -ENODEV;
	}

	return 0;
}

void gdc_driver_exit(void)
{
	platform_driver_unregister(&gdc_platform_driver);
	platform_driver_unregister(&amlgdc_platform_driver);
}

//MODULE_LICENSE("GPL v2");
//MODULE_AUTHOR("Amlogic Multimedia");
