// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_processor/v4lvideo/v4lvideo.c
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

#include <linux/amlogic/major.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>
//#include <linux/amlogic/media/utils/am_com.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/mutex.h>
#include <linux/dma-mapping.h>
#include <linux/of_fdt.h>
#include <linux/dma-contiguous.h>
#include <linux/of_reserved_mem.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/vfm/vframe.h>
//#include <linux/amlogic/media/registers/cpu_version.h>
#include "di_v4l.h"
#include <linux/platform_device.h>
#include <linux/amlogic/major.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/utils/amstream.h>

#define V4L_DI_DEVICE_NAME   "di_v4l"

static struct device *devp;

static u32 print_flag;
static u32 dump_out;
static int instance_no;
static u32 empty_done_count;

loff_t pos;
static struct di_buffer di_outputcrc[4];
static struct vframe_s vfcrc[4];

#define PRINT_ERROR		0X0
#define PRINT_INFO		0X1
#define PRINT_QUEUE_STATUS	0X0002
#define PRINT_OTHER		0X0004
#define DUMP_NAME_SIZE 64

struct crc_info_t crc_info[11] = {0};

int di_v4l_print(int index, int debug_flag, const char *fmt, ...)
{
	if ((print_flag & debug_flag) ||
	    debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "di_v4l:[%d]", index);
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static u8 __iomem *map_virt_from_phys(phys_addr_t phys,
				      unsigned long total_size)
{
	u32 offset, npages;
	struct page **pages = NULL;
	pgprot_t pgprot;
	u8 __iomem *vaddr;
	int i;

	npages = PAGE_ALIGN(total_size) / PAGE_SIZE;
	offset = phys & (~PAGE_MASK);
	if (offset)
		npages++;
	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return NULL;
	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}
	/*nocache*/
	pgprot = pgprot_noncached(PAGE_KERNEL);

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		pr_err("vmaped fail, size: %d\n",
			npages << PAGE_SHIFT);
		vfree(pages);
		return NULL;
	}
	vfree(pages);

	return vaddr;
}

static void unmap_virt_from_phys(u8 __iomem *vaddr)
{
	if (vaddr) {
		vunmap(vaddr);
		vaddr = NULL;
	}
}

static void codec_dma_flush(char *buf_start, u32 buf_size)
{
	codec_mm_dma_flush((void *)buf_start,
			   buf_size,
			   DMA_FROM_DEVICE);
}

uint32_t empty_input_done(struct di_buffer *buf)
{
	struct di_v4l_dev *dev = NULL;
	unsigned int tmp;

	if (!buf) {
		pr_err("di_v4l:empty di_buffer is NULL\n");
		return 0;
	}

	if (empty_done_count > 5) {
		//pr_err("vf is over\n");
		return 0;
	}

	dev = (struct di_v4l_dev *)(buf->caller_data);

	if (buf->flag & DI_FLAG_EOS)
		di_v4l_print(dev->index, PRINT_ERROR,
			"eos\n");

	if (!buf->vf) {
		di_v4l_print(dev->index, PRINT_ERROR,
			"vf is NULL\n");
		return 0;
	}

	dev->empty_done_count++;
	empty_done_count++;

	di_v4l_print(dev->index, PRINT_INFO,
		  "index=%d\n", dev->empty_done_count);

	tmp = kfifo_len(&dev->di_input_free_q) / sizeof(unsigned int);

	if ((buf->flag & DI_FLAG_BUF_BY_PASS) == 0)
		if (!kfifo_put(&dev->di_input_free_q, buf))
			di_v4l_print(dev->index, PRINT_INFO,
				"put di_input_free_q fail=%d,count=%d", tmp,
				dev->empty_done_count);

	return 0;
}

static void dump_yuv_data(struct di_v4l_dev *dev,
			  struct vframe_s *vf, u32 index)
{
	struct file *fp;
	mm_segment_t fs;
	char name_buf[DUMP_NAME_SIZE];
	u32 write_size;
	u32 phy_addr_y;
	u32 phy_addr_uv;
	u32 size_y;
	u32 size_uv;
	u32 size_total;
	void __iomem *buffer_start;
	char *p;
	u32 w;
	u32 h;

	w = vf->canvas0_config[0].width;
	h = vf->canvas0_config[0].height;
	di_v4l_print(dev->index, PRINT_INFO,
		"w=%d, h=%d, vf_w=%d, vf_h=%d, block_mode=%x, endian=%x, block_mode1=%x, endian1=%x, bitdepth=%d, type=%d\n",
		w, h,
		vf->width,
		vf->height,
		vf->canvas0_config[0].block_mode,
		vf->canvas0_config[0].endian,
		vf->canvas0_config[1].block_mode,
		vf->canvas0_config[1].endian,
		vf->bitdepth,
		vf->type);

	phy_addr_y = vf->canvas0_config[0].phy_addr;
	phy_addr_uv = vf->canvas0_config[1].phy_addr;
	size_y = w  * h;
	size_uv = size_y / 2;
	size_total = size_y + size_uv;

	snprintf(name_buf, DUMP_NAME_SIZE, "/data/tmp/diout%d-%d-%d.raw",
		 vf->width, vf->height, index);

	fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(name_buf, O_WRONLY | O_CREAT, 0666);
	if (IS_ERR(fp)) {
		di_v4l_print(dev->index, PRINT_ERROR,
			"create %s fail.\n", name_buf);
	} else {
		write_size = size_total;
		buffer_start = map_virt_from_phys(phy_addr_y, size_total);
		p = (char *)buffer_start;
		vfs_write(fp, p, size_total, &pos);
		unmap_virt_from_phys(buffer_start);
		filp_close(fp, NULL);
	}
	set_fs(fs);
}

uint32_t fill_output_done(struct di_buffer *buf)
{
	struct di_v4l_dev *dev = NULL;

	if (!buf) {
		pr_err("di_v4l: di_buffer is NULL\n");
		return 0;
	}
	dev = (struct di_v4l_dev *)(buf->caller_data);

	if (buf->flag & DI_FLAG_EOS) {
		di_v4l_print(dev->index, PRINT_ERROR,
			"%s: eos\n", __func__);
	}

	if (!buf->vf) {
		di_v4l_print(dev->index, PRINT_ERROR,
			"%s: vf is NULL\n", __func__);
		return 0;
	}
	if (dev->fill_done_count > 5)
		crc_info->crcdata[dev->fill_done_count] = buf->nrcrcout;
	else
		crc_info->crcdata[dev->fill_done_count] = buf->crcout;

	crc_info->done_flag[dev->fill_done_count] = true;

	di_v4l_print(dev->index, PRINT_OTHER,
		"%s count=%d, crc =%x\n",
		__func__,
		dev->fill_done_count,
		crc_info->crcdata[dev->fill_done_count]);

	dev->fill_done_count++;

	di_v4l_print(dev->index, PRINT_OTHER,
		  "%s: index=%d\n", __func__, buf->vf->omx_index);

	if (dump_out)
		dump_yuv_data(dev, buf->vf, dev->fill_done_count);

	if (!kfifo_put(&dev->di_output_di_q, buf))
		di_v4l_print(dev->index, PRINT_ERROR, "%s:putfail\n", __func__);
	return 0;
}

static void di_buf_init(struct di_v4l_dev *dev, struct yuv_info_t *yuv_info)
{
	int i;
	int buf_size;
	int flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;
	struct vframe_s *vf = NULL;
	struct di_buffer *di_buf;
	int w;
	int h;
	int buf_w;
	int buf_h;

	dev->yuv_info = *yuv_info;
	w = yuv_info->width;
	h = yuv_info->height;
	buf_w = yuv_info->buffer_w;
	buf_h = yuv_info->buffer_h;

	di_v4l_print(dev->index, PRINT_INFO,
		  "w = %d, h = %d\n", w, h);
	buf_size = buf_w * buf_h * 3 / 2;
	dev->in_buf_ready = true;

	INIT_KFIFO(dev->di_input_free_q);
	kfifo_reset(&dev->di_input_free_q);

	INIT_KFIFO(dev->di_output_di_q);
	kfifo_reset(&dev->di_output_di_q);

	for (i = 0; i < DI_INPUT_SIZE; i++) {
		if (dev->in_buf[i].phy_addr == 0)
			dev->in_buf[i].phy_addr =
				codec_mm_alloc_for_dma("di_v4l",
						       buf_size / PAGE_SIZE,
						       0, flags);
		di_v4l_print(dev->index, PRINT_OTHER,
			 "cma memory is %x , size is  %x\n",
			 (unsigned int)dev->in_buf[i].phy_addr,
			 (unsigned int)buf_size);

		if (dev->in_buf[i].phy_addr == 0) {
			dev->in_buf_ready = false;
			di_v4l_print(dev->index, PRINT_ERROR,
				 "cma memory config fail\n");
			return;
		}
		dev->in_buf[i].index = i;
		dev->in_buf[i].buf_w = buf_w;
		dev->in_buf[i].buf_h = buf_h;
		dev->in_buf[i].buf_size = buf_size;
		vf = &dev->in_buf[i].frame;
		memset(vf, 0, sizeof(struct vframe_s));
		vf->canvas0Addr = -1;
		vf->canvas0_config[0].phy_addr = dev->in_buf[i].phy_addr;
		vf->canvas0_config[0].width = buf_w;
		vf->canvas0_config[0].height = buf_h;
		vf->canvas0_config[0].block_mode = 1;
		vf->canvas0_config[0].endian = 0;

		vf->canvas1Addr = -1;
		vf->canvas0_config[1].phy_addr = dev->in_buf[i].phy_addr
			+ vf->canvas0_config[0].width
			* vf->canvas0_config[0].height;
		vf->canvas0_config[1].width = buf_w;
		vf->canvas0_config[1].height = buf_h;
		vf->canvas0_config[1].block_mode = 1;
		vf->canvas0_config[1].endian = 0;

		vf->width = w;
		vf->height = h;
		vf->plane_num = 2;
		vf->type = VIDTYPE_VIU_NV21
				| VIDTYPE_INTERLACE_FIRST
				| VIDTYPE_INTERLACE_TOP;
		vf->bitdepth = 0;
			//BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
		vf->flag = 0x10;

		di_buf = &dev->in_buf[i].di_buf;
		di_buf->vf = vf;
		di_buf->caller_data = (void *)dev;
		di_buf->flag = 0;
		di_buf->phy_addr = dev->in_buf[i].phy_addr;
		if (!kfifo_put(&dev->di_input_free_q, di_buf))
			di_v4l_print(dev->index, PRINT_ERROR,
				 "init buffer free_q is full\n");
	}
}

static void di_buf_uninit(struct di_v4l_dev *dev)
{
	int i;

	for (i = 0; i < DI_INPUT_SIZE; i++) {
		if (dev->in_buf[i].phy_addr != 0) {
			di_v4l_print(dev->index, PRINT_INFO,
				 "cma free addr is %x\n",
				 (unsigned int)dev->in_buf[i].phy_addr);
			codec_mm_free_for_dma("di_v4l",
					      dev->in_buf[i].phy_addr);
			dev->in_buf[i].phy_addr = 0;
		}
	}
}

static void di_init(struct di_v4l_dev *dev)
{
	di_v4l_print(dev->index, PRINT_ERROR,
		  "begin %s\n", __func__);

	dev->di_index = -1;

	dev->di_parm.work_mode = WORK_MODE_PRE_POST;
	dev->di_parm.buffer_mode = BUFFER_MODE_ALLOC_BUF;
	dev->di_parm.output_format = 0;
	dev->di_parm.ops.empty_input_done = empty_input_done;
	dev->di_parm.ops.fill_output_done = fill_output_done;
	dev->di_parm.caller_data = (void *)dev;
	dev->di_index = di_create_instance(dev->di_parm);
	if (dev->di_index < 0) {
		di_v4l_print(dev->index, PRINT_ERROR,
			  "creat di fail, di_index=%d\n",
			  dev->di_index);
		return;
	}
	di_v4l_print(dev->index, PRINT_OTHER,
		  "di_index = %d\n",  dev->di_index);

	di_v4l_print(dev->index, PRINT_ERROR,
		  "end %s\n",  __func__);
	dev->empty_done_count = 0;
	dev->fill_done_count = 0;
	dev->fill_frame_count = 0;
	dev->fill_crc_count = 0;
	empty_done_count = 0;
}

static void di_uninit(struct di_v4l_dev *dev)
{
	int ret;

	di_v4l_print(dev->index, PRINT_ERROR,
		  "begin %s\n",  __func__);

	if (dev->di_index >= 0) {
		ret = di_destroy_instance(dev->di_index);
		if (ret != 0)
			di_v4l_print(dev->index, PRINT_ERROR,
				  "destroy di fail, di_index=%d\n",
				  dev->di_index);
		dev->di_index = -1;
	}

	di_v4l_print(dev->index, PRINT_ERROR,
		  "end %s\n", __func__);
}

static int di_v4l_open(struct inode *inode, struct file *file)
{
	struct di_v4l_dev *dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	file->private_data = dev;
	di_init(dev);
	instance_no++;
	pos = 0;

	return 0;
}

static int di_v4l_release(struct inode *inode, struct file *file)
{
	struct di_v4l_dev *dev = file->private_data;
	struct di_buffer *di_buf = NULL;

	while (kfifo_get(&dev->di_output_di_q, &di_buf)) {
		di_fill_output_buffer(dev->di_index, di_buf);
		dev->fill_count++;
	}

	di_uninit(dev);
	di_buf_uninit(dev);
	while (kfifo_get(&dev->di_output_di_q, &di_buf)) {
		di_release_keep_buf(di_buf);
		dev->fill_count++;
	}
	di_v4l_print(dev->index, PRINT_ERROR,
		  "fill_done_count %d, fill_count=%d\n",
		  dev->fill_done_count,
		  dev->fill_count);

	kfree(dev);
	return 0;
}

static ssize_t di_v4l_write(struct file *file, const char *buf,
					size_t count, loff_t *ppos)
{
	struct di_v4l_dev *dev = file->private_data;
	char *dst;
	unsigned long phy_addr;
	int ret;
	struct di_buffer *di_buf = NULL;
	struct di_buffer *di_bufcrc = &di_outputcrc[0];
	struct vframe_s *pvfcrc = &vfcrc[0];
	int i;

	di_v4l_print(dev->index, PRINT_INFO,
		"di_v4l_write__1\n");
	if (dev->fill_frame_count < 3) {
		for (i = 0; i < 2; i++) {
			if (kfifo_get(&dev->di_input_free_q, &di_buf)) {
				phy_addr = di_buf->phy_addr;
				dst = map_virt_from_phys(phy_addr, count);
				if (copy_from_user(dst, buf, count)) {
					di_v4l_print(dev->index, PRINT_ERROR,
						"copy_from_user err\n");
					unmap_virt_from_phys(dst);
					return -EFAULT;
				}
				//dma_flush(phy_addr, count);
				codec_dma_flush(dst, count);
				unmap_virt_from_phys(dst);
				if (i == 0)
					di_buf->vf->type = VIDTYPE_VIU_NV21
						| VIDTYPE_INTERLACE_FIRST
						| VIDTYPE_INTERLACE_TOP;
				else
					di_buf->vf->type = VIDTYPE_VIU_NV21
						| VIDTYPE_INTERLACE_FIRST
						| VIDTYPE_INTERLACE_BOTTOM;
				ret = di_empty_input_buffer(dev->di_index,
							    di_buf);
				di_v4l_print(dev->index, PRINT_INFO,
					"canvaswidth=%d,height=%d\n",
					 di_buf->vf->canvas0_config[0].width,
					 di_buf->vf->canvas0_config[0].height);
				di_v4l_print(dev->index, PRINT_INFO,
					"bufWidth=%d, width=%d,height=%d\n",
					di_buf->vf->bufWidth, di_buf->vf->width,
					di_buf->vf->height);
				di_v4l_print(dev->index, PRINT_INFO,
					"type=%d, plane_num=%d\n",
					di_buf->vf->type,
					di_buf->vf->plane_num);
				if (ret != 0)
					di_v4l_print(dev->index, PRINT_ERROR,
					"empty_input err:ret=%d, di_index=%d\n",
					ret, dev->di_index);
			} else {
				return -ENOMEM;
			}
		}
		dev->fill_frame_count++;
		di_v4l_print(dev->index, PRINT_INFO,
		"di_v4l_write__2\n");
	}

	if (dev->fill_frame_count == 3) {
		while (1) {
			if (dev->fill_frame_count > 7)
				break;
			di_v4l_print(dev->index, PRINT_INFO,
				"di_v4l_writecrc__5 start\n");
			if (kfifo_get(&dev->di_output_di_q, &di_buf)) {
				memcpy(pvfcrc, di_buf->vf,
				       sizeof(struct vframe_s));
				pvfcrc->private_data = NULL;
				//di_bufcrc->private_data = NULL;
				di_bufcrc->flag = di_buf->flag;
				di_bufcrc->vf = pvfcrc;
				if (dump_out)
					dump_yuv_data(dev, di_bufcrc->vf,
						      dev->fill_frame_count);
				ret = di_empty_input_buffer
					(dev->di_index, di_bufcrc);
				if (ret != 0)
					di_v4l_print(dev->index, PRINT_ERROR,
					"empty_input err:ret=%d, di_index=%d\n",
					ret, dev->di_index);
				di_fill_output_buffer(dev->di_index,
							      di_buf);
				dev->fill_frame_count++;
				di_bufcrc++;
				pvfcrc++;
				di_v4l_print(dev->index, PRINT_INFO,
					"di_v4l_writecrc__6 end\n");
			} else {
				di_v4l_print(dev->index, PRINT_INFO,
					"output_di_q\n");
				//usleep(1000 * 5);
			}
		}
	}
	return count;
}

static long di_v4l_ioctl(struct file *file,
			   unsigned int cmd,
			   ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	struct di_v4l_dev *dev = file->private_data;

	switch (cmd) {
	case DI_V4L_IOCTL_GET_CRC:{
			struct di_buffer *di_buf = NULL;

			if (crc_info->done_flag[dev->fill_crc_count] &&
			    dev->fill_frame_count > 7) {
				di_v4l_print(dev->index, PRINT_INFO,
					"get crc ok\n");
				put_user(crc_info->crcdata[dev->fill_crc_count],
					 (u32 __user *)argp);
				if (kfifo_get(&dev->di_output_di_q, &di_buf))
					di_fill_output_buffer(dev->di_index,
							      di_buf);
				dev->fill_count++;
				dev->fill_crc_count++;
			} else {
				di_v4l_print(dev->index, PRINT_INFO,
					"get crc ng\n");
				return -EAGAIN;
			}
		}
		break;
	case DI_V4L_IOCTL_INIT:{
			struct yuv_info_t yuv_info;

			di_v4l_print(dev->index, PRINT_ERROR,
				"DI_V4L_IOCTL_INIT\n");
			if (copy_from_user((void *)&yuv_info, (void *)arg,
				sizeof(struct yuv_info_t)))
				return -EFAULT;
			di_buf_init(dev, &yuv_info);
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long di_v4l_compat_ioctl(struct file *file,
				  unsigned int cmd,
				  ulong arg)
{
	long ret = 0;

	ret = di_v4l_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

static const struct file_operations di_v4l_fops = {
	.owner = THIS_MODULE,
	.open = di_v4l_open,
	.release = di_v4l_release,
	.write = di_v4l_write,
	.unlocked_ioctl = di_v4l_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = di_v4l_compat_ioctl,
#endif
	.poll = NULL,
};

static ssize_t print_flag_show(struct class *class,
				     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", print_flag);
}

static ssize_t print_flag_store(struct class *class,
				      struct class_attribute *attr,
				      const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	print_flag = val;
	pr_info("set print_flag:%d\n", print_flag);
	return count;
}

static ssize_t dump_out_show(struct class *class,
				     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", dump_out);
}

static ssize_t dump_out_store(struct class *class,
				      struct class_attribute *attr,
				      const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	dump_out = val;
	pr_info("set dump_out:%d\n", dump_out);
	return count;
}

static CLASS_ATTR_RW(print_flag);
static CLASS_ATTR_RW(dump_out);

static struct attribute *di_v4l_class_attrs[] = {
	&class_attr_print_flag.attr,
	&class_attr_dump_out.attr,
	NULL
};

ATTRIBUTE_GROUPS(di_v4l_class);

static struct class di_v4l_class = {
	.name = "di_v4l",
	.class_groups = di_v4l_class_groups,
};

int __init di_v4l_init(void)
{
	int ret = -1;
	int r;

	pr_info("%s init\n", __func__);
	ret = class_register(&di_v4l_class);
	if (ret < 0)
		return ret;

	ret = register_chrdev(DI_V4L_MAJOR, "di_v4l", &di_v4l_fops);
	if (ret < 0) {
		pr_err("Can't allocate major for di_v4l device\n");
		goto error1;
	}

	devp = device_create(&di_v4l_class,
			     NULL,
			     MKDEV(DI_V4L_MAJOR, 0),
			     NULL,
			     V4L_DI_DEVICE_NAME);
	if (IS_ERR(devp)) {
		pr_err("failed to create di_v4l device node\n");
		ret = PTR_ERR(devp);
		return ret;
	}

	r = of_reserved_mem_device_init(devp);
	if (r < 0)
		pr_err("%s reserved mem is not used.\n", __func__);
	pr_info("%s out\n", __func__);

	return 0;
error1:
	pr_info("%s err\n", __func__);
	unregister_chrdev(DI_V4L_MAJOR, "di_v4l");
	class_unregister(&di_v4l_class);
	return ret;
}

void __exit di_v4l_exit(void)
{
	device_destroy(&di_v4l_class, MKDEV(DI_V4L_MAJOR, 0));
	unregister_chrdev(DI_V4L_MAJOR, V4L_DI_DEVICE_NAME);
	class_unregister(&di_v4l_class);
}

//#ifndef MODULE
//module_init(di_v4l_init);
//module_exit(di_v4l_exit);
//#endif

