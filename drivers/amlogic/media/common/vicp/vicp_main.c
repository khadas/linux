// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/common/vicp/vicp_main.c
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
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
/* Amlogic Headers */
#include <linux/amlogic/major.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif

/* Local Headers */
#include "vicp_log.h"
#include "vicp_reg.h"
#include "vicp_process.h"
#include "vicp_test.h"
#include "vicp_hardware.h"

#define VICP_DEVICE_NAME "vicp"
void __iomem *vicp_reg_map;

u32 print_flag;
u32 demo_enable;
static int demo_result;
u32 dump_yuv_flag;
u32 scaler_en = 1;
u32 hdr_en = 1;
u32 crop_en = 1;
u32 shrink_en = 1;
u32 fgrain_en = 1;
u32 debug_axis_en;
struct output_axis_s axis;
u32 rdma_en;
u32 debug_rdma_en;
u32 debug_reg_en;

struct mutex vicp_mutex; /*used to avoid user space call at the same time*/
struct vicp_hdr_data_s *vicp_hdr;

struct vicp_device_s {
	char name[20];
	atomic_t open_count;
	int major;
	unsigned int dbg_enable;
	struct class *cla;
	struct device *dev;
};

static struct vicp_device_s vicp_device;

static ssize_t print_flag_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "current print_flag is %d.\n", print_flag);
}

static ssize_t print_flag_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int val;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return -EINVAL;

	if (val > 0)
		print_flag = val;
	else
		print_flag = 0;

	pr_info("set print_flag to %d.\n", print_flag);
	return count;
}

static int parse_param(char *buf, char **parm)
{
	char *position, *token;
	u32 count = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	position = buf;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&position, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[count++] = token;
	}
	return count;
}

static ssize_t reg_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	pr_info("usage: this node used to read or write vicp reg.\n");
	pr_info("usage: vicp reg range from 0x0000 to 0x038d.\n");
	pr_info("read one reg by [echo rv reg_addr > /sys/class/vicp/reg].\n");
	pr_info("read some reg by [echo rv reg_addr reg_count > /sys/class/vicp/reg].\n");
	pr_info("write one reg by [echo wv reg_addr reg_val > /sys/class/vicp/reg].\n");

	return 0;
}

static ssize_t reg_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	u32 val = 0, param_count;
	u32 reg_addr, reg_val, reg_count;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	param_count = parse_param(buf_orig, (char **)&parm);
	if (!strcmp(parm[0], "rv")) {
		if (param_count < 2) {
			pr_info("%s: invalid read param.\n", __func__);
			kfree(buf_orig);
			buf_orig = NULL;
			return count;
		}

		if (kstrtouint(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		reg_addr = val;
		if (param_count > 2) {
			if (kstrtouint(parm[2], 16, &val) < 0) {
				kfree(buf_orig);
				buf_orig =  NULL;
				return -EINVAL;
			}
			reg_count = val;
			vicp_dump_reg(reg_addr, reg_count);
		} else {
			vicp_dump_reg(reg_addr, 1);
		}
	} else if (!strcmp(parm[0], "wv")) {
		if (param_count < 2) {
			pr_info("%s: invalid write param.\n", __func__);
			kfree(buf_orig);
			buf_orig = NULL;
			return count;
		}

		if (kstrtouint(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		reg_addr = val;
		if (kstrtouint(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			buf_orig =  NULL;
			return -EINVAL;
		}
		reg_val = val;
		vicp_reg_write(reg_addr, reg_val);
	}
	kfree(buf_orig);
	buf_orig = NULL;
	return count;
}

static ssize_t demo_enable_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count = sprintf(buf, "current demo_enable is %d.\n", demo_enable);

	if (demo_result == -1)
		count += sprintf(buf + count, "vicp test result: failed.\n");
	else if (demo_result == 0)
		count += sprintf(buf + count, "vicp test not complete.\n");
	else
		count += sprintf(buf + count, "vicp test result: success.\n");

	return count;
}

static ssize_t demo_enable_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int val;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return -EINVAL;

	pr_info("set demo_enable from %d to %d.\n", demo_enable, val);
	if (val > 0)
		demo_enable = val;
	else
		demo_enable = 0;

	demo_result = 0;
	if (demo_enable)
		demo_result = vicp_test();

	return count;
}

static ssize_t dump_yuv_flag_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "current dump_yuv_flag is %d.\n", dump_yuv_flag);
}

static ssize_t dump_yuv_flag_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int val;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return -EINVAL;

	if (val > 0)
		dump_yuv_flag = val;
	else
		dump_yuv_flag = 0;

	pr_info("set dump_yuv_flag to %d.\n", dump_yuv_flag);
	return count;
}

static ssize_t scaler_en_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "current scaler_en is %d.\n", scaler_en);
}

static ssize_t scaler_en_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int val;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return -EINVAL;

	if (val > 0)
		scaler_en = val;
	else
		scaler_en = 0;

	pr_info("set scaler_en to %d.\n", scaler_en);
	return count;
}

static ssize_t hdr_en_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "current hdr_en is %d.\n", hdr_en);
}

static ssize_t hdr_en_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int val;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return -EINVAL;

	if (val > 0)
		hdr_en = val;
	else
		hdr_en = 0;

	pr_info("set hdr_en to %d.\n", hdr_en);
	return count;
}

static ssize_t crop_en_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "current crop_en is %d.\n", crop_en);
}

static ssize_t crop_en_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int val;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return -EINVAL;

	if (val > 0)
		crop_en = val;
	else
		crop_en = 0;

	pr_info("set crop_en to %d.\n", crop_en);
	return count;
}

static ssize_t shrink_en_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "current shrink_en is %d.\n", shrink_en);
}

static ssize_t shrink_en_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int val;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return -EINVAL;

	if (val > 0)
		shrink_en = val;
	else
		shrink_en = 0;

	pr_info("set shrink_en to %d.\n", shrink_en);
	return count;
}

static ssize_t fgrain_en_show(struct class *class, struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "current fgrain_en is %d.\n", fgrain_en);
}

static ssize_t fgrain_en_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int val;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return -EINVAL;

	if (val > 0)
		fgrain_en = val;
	else
		fgrain_en = 0;

	pr_info("set fgrain_en to %d.\n", fgrain_en);
	return count;
}

static ssize_t debug_axis_en_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "current debug_axis_en is %d.\n", debug_axis_en);
}

static ssize_t debug_axis_en_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	int val;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return -EINVAL;

	if (val > 0)
		debug_axis_en = val;
	else
		debug_axis_en = 0;

	pr_info("set debug_axis_en to %d.\n", debug_axis_en);
	return count;
}

static ssize_t axis_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "left: %d, top: %d, width: %d, height: %d.\n",
		axis.left, axis.top, axis.width, axis.height);
}

static ssize_t axis_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	char *token = NULL;
	char *params, *params_base;
	int value[4] = {0, 0, 0, 0};
	int len = 0, number = 0;
	int res = 0;
	int ret = 0;

	if (!buf)
		return 0;

	params = kstrdup(buf, GFP_KERNEL);
	params_base = params;
	token = params;
	if (token) {
		len = strlen(token);
		do {
			token = strsep(&params, " ");
			if (!token)
				break;
			while (token && (isspace(*token) || !isgraph(*token)) && len) {
				token++;
				len--;
			}
			if (len == 0)
				break;
			ret = kstrtoint(token, 0, &res);
			if (ret < 0)
				break;
			len = strlen(token);
			value[number] = res;
			number++;
		} while ((number < 4) && (len > 0));
	}
	kfree(params_base);

	axis.left = value[0];
	axis.top = value[1];
	axis.width = value[2];
	axis.height = value[3];

	return count;
}

static ssize_t rdma_en_show(struct class *cla, struct class_attribute *attr,
	char *buf)
{
	return snprintf(buf, 80, "current rdma_enable is %d.\n", rdma_en);
}

static ssize_t rdma_en_store(struct class *cla, struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	rdma_en = tmp;

	pr_info("set rdma_en to %d.\n", rdma_en);
	return count;
}

static ssize_t debug_rdma_en_show(struct class *cla, struct class_attribute *attr,
	char *buf)
{
	return snprintf(buf, 80, "current debug_rdma_enable is %d.\n", debug_rdma_en);
}

static ssize_t debug_rdma_en_store(struct class *cla, struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	debug_rdma_en = tmp;

	pr_info("set debug_rdma_en to %d.\n", debug_rdma_en);
	return count;
}

static ssize_t debug_reg_en_show(struct class *cla, struct class_attribute *attr,
	char *buf)
{
	return snprintf(buf, 80, "current debug_reg_enable is %d.\n", debug_reg_en);
}

static ssize_t debug_reg_en_store(struct class *cla, struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	debug_reg_en = tmp;

	pr_info("set debug_reg_en to %d.\n", debug_reg_en);
	return count;
}

static CLASS_ATTR_RW(print_flag);
static CLASS_ATTR_RW(reg);
static CLASS_ATTR_RW(demo_enable);
static CLASS_ATTR_RW(dump_yuv_flag);
static CLASS_ATTR_RW(scaler_en);
static CLASS_ATTR_RW(hdr_en);
static CLASS_ATTR_RW(crop_en);
static CLASS_ATTR_RW(shrink_en);
static CLASS_ATTR_RW(fgrain_en);
static CLASS_ATTR_RW(debug_axis_en);
static CLASS_ATTR_RW(axis);
static CLASS_ATTR_RW(rdma_en);
static CLASS_ATTR_RW(debug_rdma_en);
static CLASS_ATTR_RW(debug_reg_en);

static struct attribute *vicp_class_attrs[] = {
	&class_attr_print_flag.attr,
	&class_attr_reg.attr,
	&class_attr_demo_enable.attr,
	&class_attr_dump_yuv_flag.attr,
	&class_attr_scaler_en.attr,
	&class_attr_hdr_en.attr,
	&class_attr_crop_en.attr,
	&class_attr_shrink_en.attr,
	&class_attr_fgrain_en.attr,
	&class_attr_debug_axis_en.attr,
	&class_attr_axis.attr,
	&class_attr_rdma_en.attr,
	&class_attr_debug_rdma_en.attr,
	&class_attr_debug_reg_en.attr,
	NULL
};
ATTRIBUTE_GROUPS(vicp_class);

static struct class vicp_class = {
	.name = VICP_DEVICE_NAME,
	.class_groups = vicp_class_groups,
};

static unsigned long get_buf_phy_addr(u32 buf_fd)
{
	struct dma_buf *dbuf = NULL;
	unsigned long phy_addr = 0;
	struct sg_table *table = NULL;
	struct page *page = NULL;
	struct dma_buf_attachment *attach = NULL;

	dbuf = dma_buf_get(buf_fd);
	if (IS_ERR_OR_NULL(dbuf)) {
		pr_err("%s: get phyaddr failed: fd is %d.\n", __func__, buf_fd);
		return -EINVAL;
	}

	attach = dma_buf_attach(dbuf, vicp_device.dev);
	if (IS_ERR_OR_NULL(attach)) {
		dma_buf_put(dbuf);
		pr_err("%s: attach err\n", __func__);
		return -EINVAL;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(table)) {
		pr_err("%s: get table failed.\n", __func__);
		dma_buf_detach(dbuf, attach);
		return -EINVAL;
	}

	page = sg_page(table->sgl);
	phy_addr = PFN_PHYS(page_to_pfn(page));
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
	dma_buf_detach(dbuf, attach);
	dma_buf_put(dbuf);

	return phy_addr;
}

static int config_vicp_param(struct vicp_data_info_s *vicp_data_info,
	struct vicp_data_config_s *data_config)
{
	struct dma_data_config_s data_dma;

	if (IS_ERR_OR_NULL(vicp_data_info) || IS_ERR_OR_NULL(data_config)) {
		pr_err("%s: NULL param, please check.\n", __func__);
		return -1;
	}

	data_config->input_data.is_vframe = false;
	memset(&data_dma, 0, sizeof(struct dma_data_config_s));
	data_dma.buf_addr = get_buf_phy_addr(vicp_data_info->src_buf_fd);
	data_dma.buf_stride_w = vicp_data_info->src_buf_alisg_w;
	data_dma.buf_stride_h = vicp_data_info->src_buf_alisg_h;
	data_dma.color_format = vicp_data_info->src_color_fmt;
	data_dma.color_depth = vicp_data_info->src_color_depth;
	data_dma.data_width = vicp_data_info->src_data_w;
	data_dma.data_height = vicp_data_info->src_data_h;
	data_dma.plane_count = 2;
	data_dma.endian = vicp_data_info->src_endian;
	data_dma.need_swap_cbcr = vicp_data_info->src_swap_cbcr;
	data_config->input_data.data_dma = &data_dma;

	data_config->output_data.fbc_out_en = false;
	data_config->output_data.mif_out_en = true;
	data_config->output_data.mif_color_fmt = vicp_data_info->dst_color_fmt;
	data_config->output_data.mif_color_dep = vicp_data_info->dst_color_depth;
	data_config->output_data.phy_addr[0] = get_buf_phy_addr(vicp_data_info->dst_buf_fd);
	data_config->output_data.stride[0] = vicp_data_info->dst_buf_w;
	data_config->output_data.width = vicp_data_info->dst_buf_w;
	data_config->output_data.height = vicp_data_info->dst_buf_h;
	data_config->output_data.endian = vicp_data_info->dst_endian;
	data_config->output_data.need_swap_cbcr = vicp_data_info->dst_swap_cbcr;
	data_config->output_data.out_sig_fmt = VFRAME_SIGNAL_FMT_SDR;

	data_config->data_option.crop_info.left = vicp_data_info->crop_x;
	data_config->data_option.crop_info.top = vicp_data_info->crop_y;
	data_config->data_option.crop_info.width = vicp_data_info->crop_w;
	data_config->data_option.crop_info.height = vicp_data_info->crop_h;
	data_config->data_option.output_axis.left = vicp_data_info->output_x;
	data_config->data_option.output_axis.top = vicp_data_info->output_y;
	data_config->data_option.output_axis.width = vicp_data_info->output_w;
	data_config->data_option.output_axis.height = vicp_data_info->output_h;
	data_config->data_option.rotation_mode = vicp_data_info->rotation_mode;
	data_config->data_option.rdma_enable = vicp_data_info->rdma_enable;
	data_config->data_option.security_enable = vicp_data_info->security_enable;
	data_config->data_option.shrink_mode = vicp_data_info->shrink_mode;
	data_config->data_option.skip_mode = vicp_data_info->skip_mode;
	data_config->data_option.input_source_count = vicp_data_info->input_source_count;
	data_config->data_option.input_source_number = vicp_data_info->input_source_number;

	return 0;
}

static int vicp_open(struct inode *inode, struct file *file)
{
	pr_info("%s\n", __func__);

	vicp_process_enable(1);
	return 0;
}

static long vicp_ioctl(struct file *file, unsigned int cmd, unsigned long args)
{
	long ret = 0;
	void __user *argp = (void __user *)args;
	struct vicp_data_info_s vicp_data_info;
	struct vicp_data_config_s vicp_data_config;

	switch (cmd) {
	case VICP_PROCESS:
		memset(&vicp_data_info, 0, sizeof(struct vicp_data_info_s));
		memset(&vicp_data_config, 0, sizeof(struct vicp_data_config_s));
		if (copy_from_user(&vicp_data_info, argp, sizeof(struct vicp_data_info_s)) == 0) {
			config_vicp_param(&vicp_data_info, &vicp_data_config);
			ret = vicp_process(&vicp_data_config);
		} else {
			ret = -EFAULT;
		}
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long vicp_compat_ioctl(struct file *file, unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = vicp_ioctl(file, cmd, args);

	return ret;
}
#endif

static int vicp_release(struct inode *inode, struct file *file)
{
	pr_info("%s\n", __func__);

	vicp_process_enable(0);
	return 0;
}

static const struct file_operations vicp_fops = {
	.owner = THIS_MODULE,
	.open = vicp_open,
	.unlocked_ioctl = vicp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vicp_compat_ioctl,
#endif
	.release = vicp_release,
};

static struct vicp_device_data_s vicp_s5 = {
	.rate = 667000000,
	.cpu_type = MESON_CPU_MAJOR_ID_S5,
	.film_grain_support = false,
	.cr_lossy_support = false,
	.ddr16_support = false,
};

static struct vicp_device_data_s vicp_t3x = {
	.rate = 800000000,
	.cpu_type = MESON_CPU_MAJOR_ID_T3X,
	.film_grain_support = true,
	.cr_lossy_support = true,
	.ddr16_support = true,
};

static const struct of_device_id vicp_dt_match[] = {
	{
		.compatible = "amlogic, vicp",
		.data = &vicp_s5,
	},
	{
		.compatible = "amlogic, vicp-t3x",
		.data = &vicp_t3x,
	},
	{},
};

static int init_vicp_device(void)
{
	int  ret = 0;

	strcpy(vicp_device.name, VICP_DEVICE_NAME);
	ret = register_chrdev(0, vicp_device.name, &vicp_fops);
	if (ret <= 0) {
		pr_err("register vicp device error\n");
		return  ret;
	}
	vicp_device.major = ret;
	vicp_device.dbg_enable = 0;
	ret = class_register(&vicp_class);
	if (ret < 0) {
		pr_err("error create vicp class\n");
		return ret;
	}
	vicp_device.cla = &vicp_class;
	vicp_device.dev = device_create(vicp_device.cla,
					NULL,
					MKDEV(vicp_device.major, 0),
					NULL,
					vicp_device.name);
	if (IS_ERR_OR_NULL(vicp_device.dev)) {
		pr_err("create vicp device error\n");
		class_unregister(vicp_device.cla);
		return -1;
	}

	vicp_device.dev->coherent_dma_mask = DMA_BIT_MASK(64);
	vicp_device.dev->dma_mask = &vicp_device.dev->coherent_dma_mask;

	return ret;
}

static int uninit_vicp_device(void)
{
	if (!vicp_device.cla)
		return 0;

	if (vicp_device.dev)
		device_destroy(vicp_device.cla, MKDEV(vicp_device.major, 0));
	class_unregister(vicp_device.cla);
	unregister_chrdev(vicp_device.major, vicp_device.name);

	return  0;
}

static void vicp_param_init(struct vicp_device_data_s device_data)
{
	mutex_init(&vicp_mutex);
	vicp_device_init(device_data);
	init_vicp_module_reg(device_data.cpu_type);
	memset(&axis, 0, sizeof(struct output_axis_s));

	vicp_hdr = vicp_hdr_prob();
	if (IS_ERR_OR_NULL(vicp_hdr))
		pr_info("vicp hdr init failed.\n");
}

static void vicp_param_uninit(void)
{
	vicp_hdr_remove(vicp_hdr);
}

static int vicp_probe(struct platform_device *pdev)
{
	int ret = 0;
	int irq = 0;
	struct resource res;
	int clk_cnt = 0;
	struct clk *clk_gate;
	struct clk *clk_vapb0;
	struct clk *clk;
	struct vicp_device_data_s *vicp_meson;

	if (pdev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_node(vicp_dt_match, pdev->dev.of_node);
		if (match) {
			vicp_meson = (struct vicp_device_data_s *)match->data;
			if (!vicp_meson) {
				pr_err("%s data NOT match\n", __func__);
				return -ENODEV;
			}
		} else {
			pr_err("%s NOT match\n", __func__);
			return -ENODEV;
		}
	}

	init_vicp_device();

	/* get interrupt resource */
	irq = platform_get_irq_byname(pdev, "vicp_proc");
	if (irq  == -ENXIO) {
		pr_err("cannot get vicp irq resource.\n");
		ret = -ENXIO;
		goto error;
	}

	pr_info("vicp driver probe: irq-1:%d.\n", irq);
	ret = request_irq(irq, &vicp_isr_handle, IRQF_SHARED, "vicp_proc", (void *)"vicp-dev");
	if (ret < 0) {
		pr_err("cannot get vicp irq resource.\n");
		ret = -ENXIO;
		goto error;
	}

	irq = platform_get_irq_byname(pdev, "vicp_rdma");
	if (irq  == -ENXIO) {
		pr_err("cannot get vicp rdma resource.\n");
		ret = -ENXIO;
		goto error;
	}

	pr_info("vicp driver probe: irq-2:%d.\n", irq);
	ret = request_irq(irq, &vicp_rdma_handle, IRQF_SHARED, "vicp_rdma", (void *)"vicp-dev");
	if (ret < 0) {
		pr_err("cannot get vicp rdma resource.\n");
		ret = -ENXIO;
		goto error;
	}

	clk_cnt = of_property_count_strings(pdev->dev.of_node, "clock-names");
	if (clk_cnt < 0) {
		pr_info("count clock-names err.\n");
		ret = -ENOENT;
		goto error;
	}

	if (clk_cnt == 3 || clk_cnt == 2) {
		clk_gate = devm_clk_get(&pdev->dev, "clk_vicp_gate");
		if (IS_ERR_OR_NULL(clk_gate)) {
			pr_err("cannot get clock.\n");
			clk_gate = NULL;
			ret = -ENOENT;
			goto error;
		}
		pr_info("clock source clk_vicp_gate %p.\n", clk_gate);
		if (clk_cnt == 2) {
			clk_set_rate(clk_gate, vicp_meson->rate);
			pr_info("vicp gate clock is %lu MHZ.\n", clk_get_rate(clk_gate) / 1000000);
		}
		clk_prepare_enable(clk_gate);

		if (clk_cnt == 3) {
			clk = devm_clk_get(&pdev->dev, "clk_vicp");
			if (IS_ERR_OR_NULL(clk)) {
				pr_err("cannot get clock.\n");
				clk = NULL;
				ret = -ENOENT;
				goto error;
			}
			pr_info("clock clk_vicp source %p.\n", clk);
			clk_prepare_enable(clk);
		}

		clk_vapb0 = devm_clk_get(&pdev->dev, "clk_vapb_0");
		if (PTR_ERR(clk_vapb0) != -ENOENT) {
			int vapb_rate, vpu_rate;

			if (!IS_ERR_OR_NULL(clk_vapb0)) {
				pr_info("clock source clk_vapb_0 %p.\n", clk_vapb0);
				vpu_rate = vicp_meson->rate;
				vapb_rate = vicp_meson->rate;

				pr_info("vicp init clock is %d HZ, VPU clock is %d HZ.\n",
					vapb_rate, vpu_rate);
				clk_set_rate(clk_vapb0, vapb_rate);
				clk_prepare_enable(clk_vapb0);
				vapb_rate = clk_get_rate(clk_vapb0);
				pr_info("vicp clock is %d MHZ.\n", vapb_rate / 1000000);
			}
		}
	} else if (clk_cnt == 1) {
		pr_info("vicp only one clock.\n");
		clk_gate = devm_clk_get(&pdev->dev, "clk_vicp");
			if (!IS_ERR_OR_NULL(clk_gate)) {
				int clk_rate = vicp_meson->rate;

				clk_set_rate(clk_gate, clk_rate);
				clk_prepare_enable(clk_gate);
				clk_rate = clk_get_rate(clk_gate);
				pr_info("vicp clock is %d MHZ.\n", clk_rate / 1000000);
			} else {
				pr_err("cannot get clock.\n");
				clk_gate = NULL;
				ret = -ENOENT;
				goto error;
			}
	} else {
		pr_err("unsupported clk cnt.\n");
		ret = -EINVAL;
		goto error;
	}

	ret = of_address_to_resource(pdev->dev.of_node, 0, &res);
	if (ret == 0) {
		if (res.start != 0) {
			vicp_reg_map = ioremap(res.start, resource_size(&res));
			if (vicp_reg_map) {
				pr_info("map io source 0x%p,size=%d to 0x%p.\n",
						(void *)res.start,
						(int)resource_size(&res),
						vicp_reg_map);
			}
		} else {
			vicp_reg_map = 0;
			pr_info("ignore io source start %p, size=%d.\n",
					  (void *)res.start,
					  (int)resource_size(&res));
		}
	}
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret < 0)
		pr_info("reserved mem is not used.\n");

	pm_runtime_enable(&pdev->dev);
	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret < 0)
		pr_err("runtime get power error.\n");

	//clk_disable_unprepare(clk_gate);
	vicp_param_init(*vicp_meson);
	return 0;
error:
	unregister_chrdev(VICP_MAJOR, VICP_DEVICE_NAME);
	class_unregister(&vicp_class);
	return ret;
}

static int vicp_remove(struct platform_device *pdev)
{
	vicp_param_uninit();
	pm_runtime_put_sync(&pdev->dev);
	uninit_vicp_device();

	return 0;
}

static struct platform_driver vicp_driver = {
	.probe = vicp_probe,
	.remove = vicp_remove,
	.driver = {
		.name = "amlogic-vicp",
		.of_match_table = vicp_dt_match,
	}
};

int __init vicp_init_module(void)
{
	return platform_driver_register(&vicp_driver);
}

void __exit vicp_remove_module(void)
{
	platform_driver_unregister(&vicp_driver);
}
