// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_debug.c
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

/* Standard Linux Headers */
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/extcon.h>

#include <linux/amlogic/media/codec_mm/codec_mm.h>
#ifdef CONFIG_AMLOGIC_PIXEL_PROBE
#include <linux/amlogic/pixel_probe.h>
#endif
/* Local Headers */
#include "../tvin_format_table.h"
#include "vdin_drv.h"
#include "vdin_ctl.h"
#include "vdin_regs.h"
#include "vdin_afbce.h"
#include "vdin_canvas.h"
/*2018-07-18 add debugfs*/
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include "vdin_dv.h"

void vdin_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

ssize_t sig_det_show(struct device *dev,
		     struct device_attribute *attr,
		     char *buf)
{
	int callmaster_status = 0;

	return sprintf(buf, "%d\n", callmaster_status);
}

ssize_t sig_det_store(struct device *dev,
		      struct device_attribute *attr,
		      const char *buf, size_t len)
{
	enum tvin_port_e port = TVIN_PORT_NULL;
	struct tvin_frontend_s *frontend = NULL;
	int callmaster_status = 0;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	long val;

	if (!buf)
		return len;
	/* port = simple_strtol(buf, NULL, 10); */
	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	port = val;

	frontend = tvin_get_frontend(port, 0);
	if (frontend && frontend->dec_ops &&
	    frontend->dec_ops->callmaster_det) {
		/*call the frontend det function*/
		callmaster_status = frontend->dec_ops->callmaster_det(port,
								      frontend);
		/* pr_info("%d\n",callmaster_status); */
	}
	pr_info("[vdin.%d]:%s callmaster_status=%d,port=[%s]\n",
		devp->index, __func__,
		callmaster_status, tvin_port_str(port));
	return len;
}

static DEVICE_ATTR_RW(sig_det);

ssize_t attr_show(struct device *dev,
		  struct device_attribute *attr, char *buf)
{
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += sprintf(buf + len,
		       "\n0 HDMI0\t1 HDMI1\t2 HDMI2\t3 Component0\t4 Component1");
	len += sprintf(buf + len, "\n5 CVBS0\t6 CVBS1\t7 Vga0\t8 CVBS2\n");
	len += sprintf(buf + len,
		       "echo tvstart/v4l2start port fmt_id/resolution > ");
	len += sprintf(buf + len, "/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf + len,
		       "echo v4l2start bt656/viuin/video/isp h_actve v_active");
	len += sprintf(buf + len,
		       "viuin/viu_wb0_vd1/viu_wb0_vd1/viu_wb0_post_blend/viu_wb0_osd1/viu_wb0_osd2");
	len += sprintf(buf + len,
		       "viuin2/viu2_wb0_vd1/viu2_wb0_vd1/viu2_wb0_post_blend/viu2_wb0_osd1/viu2_wb0_osd2");
	len += sprintf(buf + len,
		       "frame_rate cfmt dfmt scan_fmt > /sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf + len,
		       "cfmt/dfmt:\t0 : RGB44\t1 YUV422\t2 YUV444\t7 NV12\t8 NV21\n");
	len += sprintf(buf + len, "scan_fmt:\t1 : PROGRESSIVE\t2 INTERLACE\n");
	len += sprintf(buf + len, "abnormal cnt %u\n", devp->wr_done_abnormal_cnt);
	len += sprintf(buf + len, "echo fps >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo conversion w h dest_cfmt >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "\ndest_cfmt: 0 TVIN_RGB444\t1 TVIN_YUV422\t2 TVIN_YUV444");
	len += sprintf(buf + len,
		       "\n3 TVIN_YUYV422\t4 TVIN_YVYU422\t5 TVIN_UYVY422");
	len += sprintf(buf + len,
		       "\n6 TVIN_VYUY422\t7 TVIN_NV12\t8 TVIN_NV21\t9 TVIN_BGGR");
	len += sprintf(buf + len,
		       "\n10 TVIN_RGGB\t11 TVIN_GBRG\t12 TVIN_GRBG");
	len += sprintf(buf + len,
		       "echo state >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo histgram hnum vnum >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo force_recycle >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo read_pic parm1 parm2 >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo read_bin parm1 parm2 parm3 >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo dump_reg >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo capture external_storage/xxx.bin >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo freeze >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo unfreeze >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo rgb_xy x y  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo rgb_info  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo mpeg2vdin h_active v_active >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo yuv_rgb_info >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo mat0_xy x y >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo mat0_set x >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo hdr  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo snowon  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo snowoff  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo vf_reg  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo vf_unreg  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo pause_dec  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo resume_dec  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo color_depth val  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo color_depth_support val  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo color_depth_mode val  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo auto_cutwindow_en 0(1)  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo auto_ratio_en 0(1)  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo dolby_config  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo metadata  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo dolby_config  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo clean_dv  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo dv_debug  >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo channel_order_config c0 c1 c2 >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo open_port portname >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo close_port >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo vshrk_en 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf + len,
		       "echo prehsc_en 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf + len,
		       "echo cma_mem_mode 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf + len,
		       "echo dolby_input 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf + len,
		       "echo hist_bar_enable 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf + len,
		       "echo black_bar_enable 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf + len,
		       "echo use_frame_rate 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf + len,
		       "echo rdma_enable 0(1) >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf + len,
		       "echo irq_cnt >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf + len,
		       "echo rdma_irq_cnt >/sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf + len,
		       "echo skip_vf_num 0/1/2 /sys/class/vdin/vdinx/attr.\n");
	len += sprintf(buf + len,
		       "echo dump_afbce storage/xxx.bin >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo wr_frame_en x >/sys/class/vdin/vdinx/attr\n");
	len += sprintf(buf + len,
		       "echo skip_frame_check x >/sys/class/vdin/vdinx/attr\n (1:not skip)");
	len += sprintf(buf + len,
		       "echo dv_crc x >/sys/class/vdin/vdinx/attr (0:force false 1:force true 2:auto)\n");
	return len;
}

static void vdin_dump_one_buf_mem(char *path, struct vdin_dev_s *devp,
				  unsigned int buf_num)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	unsigned int i, j;
	unsigned int span = 0, count = 0;
	int highmem_flag;
	unsigned long highaddr;
	unsigned long phys;
	mm_segment_t old_fs = get_fs();

	if (devp->mem_protected) {
		pr_err("can not capture picture in secure mode, return directly\n");
		return;
	}

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	if (IS_ERR_OR_NULL(filp)) {
		pr_info("create %s error or filp is NULL.\n", path);
		return;
	}
	if ((devp->cma_config_flag & 0x1) &&
	    devp->cma_mem_alloc == 0) {
		pr_info("%s:no cma alloc mem!!!\n", __func__);
		return;
	}
	pr_info("cma_config_flag:0x%x\n", devp->cma_config_flag);
	if (buf_num >= devp->canvas_max_num) {
		vfs_fsync(filp, 0);
		filp_close(filp, NULL);
		set_fs(old_fs);
		pr_info("buf_num > canvas_max_num, vdin exit dump\n");
		return;
	}

	if (devp->cma_config_flag & 0x100)
		highmem_flag = PageHighMem(phys_to_page(devp->vfmem_start[0]));
	else
		highmem_flag = PageHighMem(phys_to_page(devp->mem_start));

	if (vdin_is_convert_to_nv21(devp->format_convert))
		count = (devp->canvas_h * 3) / 2;
	else
		count = devp->canvas_h;

	if (highmem_flag == 0) {
		pr_info("low mem area: one line size (%d, active:%d),vfmem_start[%d]:%lx\n",
			devp->canvas_w, devp->canvas_active_w, buf_num,
			devp->vfmem_start[buf_num]);
		if (devp->cma_config_flag & 0x1)
			buf = codec_mm_phys_to_virt(devp->vfmem_start[buf_num]);
		else
			buf = phys_to_virt(devp->vfmem_start[buf_num]);
		/*only write active data*/
		for (i = 0; i < count; i++) {
			vdin_dma_flush(devp, buf, devp->canvas_w,
				       DMA_FROM_DEVICE);
			vfs_write(filp, buf, devp->canvas_active_w, &pos);
			buf += devp->canvas_w;
		}
		/*vfs_write(filp, buf, devp->canvas_max_size, &pos);*/
		pr_info("write buffer %2d of %2u  to %s.\n",
			buf_num, devp->canvas_max_num, path);
	} else {
		pr_info("high mem area: one line size (%d, active:%d)\n",
			devp->canvas_w, devp->canvas_active_w);
		span = devp->canvas_active_w;
		phys = devp->vfmem_start[buf_num];

		for (j = 0; j < count; j++) {
			highaddr = phys + j * devp->canvas_w;
			buf = vdin_vmap(highaddr, span);
			if (!buf) {
				pr_info("vdin_vmap error\n");
				return;
			}

			vdin_dma_flush(devp, buf, span, DMA_FROM_DEVICE);
			vfs_write(filp, buf, span, &pos);
			vdin_unmap_phyaddr(buf);
		}
		pr_info("high-mem write buffer %2d of %2u to %s.\n",
			buf_num, devp->canvas_max_num, path);
	}
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}

static void vdin_dump_mem(char *path, struct vdin_dev_s *devp)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	loff_t mem_size = 0;
	unsigned int i = 0, j = 0;
	unsigned int span = 0;
	unsigned int count = 0;
	unsigned long highaddr;
	unsigned long phys;
	int highmem_flag;
	void *buf = NULL;
	void *vfbuf[VDIN_CANVAS_MAX_CNT];
	mm_segment_t old_fs = get_fs();

	if (devp->mem_protected) {
		pr_err("can not capture picture in secure mode, return directly\n");
		return;
	}

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	if (vdin_is_convert_to_nv21(devp->format_convert))
		count = (devp->canvas_h * 3) / 2;
	else
		count = devp->canvas_h;

	mem_size = (loff_t)devp->canvas_active_w * count;

	for (i = 0; i < VDIN_CANVAS_MAX_CNT; i++)
		vfbuf[i] = NULL;
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("create %s error or filp is NULL.\n", path);
		return;
	}
	if (devp->cma_mem_alloc == 0) {
		pr_info("%s:no cma alloc mem!!!\n", __func__);
		return;
	}

	if (devp->cma_config_flag & 0x100)
		highmem_flag = PageHighMem(phys_to_page(devp->vfmem_start[0]));
	else
		highmem_flag = PageHighMem(phys_to_page(devp->mem_start));
	pr_info("cma_config_flag:0x%x\n", devp->cma_config_flag);
	if (highmem_flag == 0) {
		/*low mem area*/
		pr_info("low mem area: one line size (%d, active:%d)\n",
			devp->canvas_w, devp->canvas_active_w);
		for (i = 0; i < devp->canvas_max_num; i++) {
			pos = mem_size * i;
			if (devp->cma_config_flag == 0x1)
				buf = codec_mm_phys_to_virt(devp->vfmem_start[i]);
			else if (devp->cma_config_flag == 0x101)
				vfbuf[i] = codec_mm_phys_to_virt(devp->vfmem_start[i]);
			else if (devp->cma_config_flag == 0x100)
				vfbuf[i] = phys_to_virt(devp->vfmem_start[i]);
			else
				buf = phys_to_virt(devp->vfmem_start[i]);

			/*only write active data*/
			for (j = 0; j < count; j++) {
				vdin_dma_flush(devp, buf, devp->canvas_w,
					       DMA_FROM_DEVICE);
				if (devp->cma_config_flag & 0x100) {
					vfs_write(filp, vfbuf[i],
						  devp->canvas_active_w, &pos);
					vfbuf[i] += devp->canvas_w;
				} else {
					vfs_write(filp, buf,
						  devp->canvas_active_w, &pos);
					buf += devp->canvas_w;
				}
			}
			pr_info("write buffer %2d of %2u to %s.\n",
				i, devp->canvas_max_num, path);
		}
	} else {
		/*high mem area*/
		pr_info("high mem area: one line size (%d, active:%d)\n",
			devp->canvas_w, devp->canvas_active_w);
		span = devp->canvas_active_w;

		for (i = 0; i < devp->canvas_max_num; i++) {
			pos = mem_size * i;
			phys = devp->vfmem_start[i];
			for (j = 0; j < count; j++) {
				highaddr = phys + j * devp->canvas_w;
				buf = vdin_vmap(highaddr, span);
				if (!buf) {
					pr_info("vdin_vmap error\n");
					return;
				}

				vdin_dma_flush(devp, buf, span,
					       DMA_FROM_DEVICE);
				vfs_write(filp, buf, span, &pos);
				vdin_unmap_phyaddr(buf);
			}
			pr_info("high-mem write buffer %2d of %2u to %s.\n",
				i, devp->canvas_max_num, path);
		}
	}
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}

static void vdin_dump_one_afbce_mem(char *path, struct vdin_dev_s *devp,
				    unsigned int buf_num)
{
	#define K_PATH_BUFF_LENGTH	128
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf_head = NULL;
	void *buf_table = NULL;
	void *buf_body = NULL;
	unsigned long highaddr;
	unsigned long phys;
	int highmem_flag = 0;
	unsigned int span = 0;
	unsigned int remain = 0;
	unsigned int count = 0;
	unsigned int j = 0;
	void *vbuf = NULL;
	unsigned char buff[K_PATH_BUFF_LENGTH];
	mm_segment_t old_fs = get_fs();

	if (buf_num >= devp->canvas_max_num) {
		pr_info("%s: param error", __func__);
		return;
	}

	if ((devp->cma_config_flag & 0x1) &&
	    devp->cma_mem_alloc == 0) {
		pr_info("%s:no cma alloc mem!!!\n", __func__);
		return;
	}

	highmem_flag = PageHighMem(phys_to_page(devp->afbce_info->fm_body_paddr[0]));

	if (highmem_flag == 0) {
		/*low mem area*/
		pr_info("low mem area\n");
		if (devp->cma_config_flag == 0x101) {
			buf_head =
			codec_mm_phys_to_virt(devp->afbce_info->fm_head_paddr[buf_num]);
			buf_table =
			codec_mm_phys_to_virt(devp->afbce_info->fm_table_paddr[buf_num]);
			buf_body =
			codec_mm_phys_to_virt(devp->afbce_info->fm_body_paddr[buf_num]);

			pr_info(".head_paddr=0x%lx,table_paddr=0x%lx,body_paddr=0x%lx\n",
				devp->afbce_info->fm_head_paddr[buf_num],
				(devp->afbce_info->fm_table_paddr[buf_num]),
				devp->afbce_info->fm_body_paddr[buf_num]);
		} else if (devp->cma_config_flag == 0) {
			buf_head =
				phys_to_virt(devp->afbce_info->fm_head_paddr[buf_num]);
			buf_table =
				phys_to_virt(devp->afbce_info->fm_table_paddr[buf_num]);
			buf_body =
				phys_to_virt(devp->afbce_info->fm_body_paddr[buf_num]);

			pr_info("head_paddr=0x%lx,table_paddr=0x%lx,body_paddr=0x%lx\n",
				devp->afbce_info->fm_head_paddr[buf_num],
				(devp->afbce_info->fm_table_paddr[buf_num]),
				devp->afbce_info->fm_body_paddr[buf_num]);
		}
	} else {
		/*high mem area*/
		pr_info("high mem area\n");
		buf_head = vdin_vmap(devp->afbce_info->fm_head_paddr[buf_num],
				     devp->afbce_info->frame_head_size);

		buf_table = vdin_vmap(devp->afbce_info->fm_table_paddr[buf_num],
				      devp->afbce_info->frame_table_size);

		pr_info(".head_paddr=0x%lx,table_paddr=0x%lx,body_paddr=0x%lx\n",
			devp->afbce_info->fm_head_paddr[buf_num],
			(devp->afbce_info->fm_table_paddr[buf_num]),
			devp->afbce_info->fm_body_paddr[buf_num]);
	}

	set_fs(KERNEL_DS);
	/*write header bin*/

	if (strlen(path) < K_PATH_BUFF_LENGTH) {
		//strcpy(buff, path);
		snprintf(buff, sizeof(buff), "%s/img_%03d", path, buf_num);
	} else {
		pr_info("err path len\n");
		return;
	}
	strcat(buff, "header.bin");
	filp = filp_open(buff, O_RDWR | O_CREAT, 0666);
	if (IS_ERR_OR_NULL(filp)) {
		set_fs(old_fs);
		pr_info("create %s header error or filp is NULL.\n", buff);
		return;
	}

	vdin_dma_flush(devp, buf_head,
		       devp->afbce_info->frame_head_size,
		       DMA_FROM_DEVICE);
	vfs_write(filp, buf_head, devp->afbce_info->frame_head_size, &pos);
	if (highmem_flag)
		vdin_unmap_phyaddr(buf_head);
	pr_info("write buffer %2d of %2u head to %s.\n",
		buf_num, devp->canvas_max_num, buff);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);

	/*write table bin*/
	pos = 0;
	//strcpy(buff, path);
	snprintf(buff, sizeof(buff), "%s/img_%03d", path, buf_num);
	strcat(buff, "table.bin");
	filp = filp_open(buff, O_RDWR | O_CREAT, 0666);
	if (IS_ERR_OR_NULL(filp)) {
		set_fs(old_fs);
		pr_info("create %s table error or filp is NULL.\n", buff);
		return;
	}
	vdin_dma_flush(devp, buf_table,
		       devp->afbce_info->frame_table_size,
		       DMA_FROM_DEVICE);
	vfs_write(filp, buf_table, devp->afbce_info->frame_table_size, &pos);
	if (highmem_flag)
		vdin_unmap_phyaddr(buf_table);
	pr_info("write buffer %2d of %2u table to %s.\n",
		buf_num, devp->canvas_max_num, buff);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);

	/*write body bin*/
	pos = 0;
	//strcpy(buff, path);
	snprintf(buff, sizeof(buff), "%s/img_%03d", path, buf_num);
	strcat(buff, "body.bin");
	filp = filp_open(buff, O_RDWR | O_CREAT, 0666);
	if (IS_ERR_OR_NULL(filp)) {
		set_fs(old_fs);
		pr_info("create %s body error or filp is NULL.\n", buff);
		return;
	}
	if (highmem_flag == 0) {
		vfs_write(filp, buf_body,
			  devp->afbce_info->frame_body_size, &pos);
	} else {
		span = SZ_1M;
		count = devp->afbce_info->frame_body_size / PAGE_ALIGN(span);
		remain = devp->afbce_info->frame_body_size % PAGE_ALIGN(span);
		phys = devp->afbce_info->fm_body_paddr[buf_num];

		for (j = 0; j < count; j++) {
			highaddr = phys + j * span;
			vbuf = vdin_vmap(highaddr, span);
			if (!vbuf) {
				set_fs(old_fs);
				pr_info("vdin_vmap error\n");
				return;
			}

			vdin_dma_flush(devp, vbuf, span, DMA_FROM_DEVICE);
			vfs_write(filp, vbuf, span, &pos);
			vdin_unmap_phyaddr(vbuf);
		}

		if (remain) {
			span = devp->afbce_info->frame_body_size - remain;
			highaddr = phys + span;
			vbuf = vdin_vmap(highaddr, remain);
			if (!vbuf) {
				set_fs(old_fs);
				pr_info("vdin_vmap1 error\n");
				return;
			}

			vdin_dma_flush(devp, vbuf, remain, DMA_FROM_DEVICE);
			vfs_write(filp, vbuf, remain, &pos);
			vdin_unmap_phyaddr(vbuf);
		}
	}
	pr_info("write buffer %2d of %2u body to %s.\n",
		buf_num, devp->canvas_max_num, buff);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);

	set_fs(old_fs);
}

static void dump_other_mem(struct vdin_dev_s *devp, char *path,
			   unsigned int start, unsigned int offset)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	mm_segment_t old_fs = get_fs();

	if (devp->mem_protected) {
		pr_err("can not capture picture in secure mode, return directly\n");
		return;
	}

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	if (IS_ERR_OR_NULL(filp)) {
		pr_info("create %s error or filp is NULL.\n", path);
		return;
	}
	buf = phys_to_virt(start);
	vfs_write(filp, buf, offset, &pos);
	pr_info("write from 0x%x to 0x%x to %s.\n",
		start, start + offset, path);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}

static void vdin_dv_debug(struct vdin_dev_s *devp)
{
	struct vframe_s *dv_vf;
	struct vf_entry *vfe;
	struct provider_aux_req_s req;

	vfe = devp->curr_wr_vfe;
	if (!vfe) {
		pr_info("current wr vfe is null\n");
		return;
	}
	dv_vf = &vfe->vf;
	req.vf = dv_vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;
	vf_notify_provider_by_name("vdin0",
				   VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
				   (void *)&req);
}

/*config vidn output channel order*/
static void vdin_channel_order_config(unsigned int offset,
				      unsigned int vdin_data_bus_0,
				      unsigned int vdin_data_bus_1,
				      unsigned int vdin_data_bus_2)
{
	wr_bits(offset, VDIN_COM_CTRL0, vdin_data_bus_0,
		COMP0_OUT_SWT_BIT, COMP0_OUT_SWT_WID);
	wr_bits(offset, VDIN_COM_CTRL0, vdin_data_bus_1,
		COMP1_OUT_SWT_BIT, COMP1_OUT_SWT_WID);
	wr_bits(offset, VDIN_COM_CTRL0, vdin_data_bus_2,
		COMP2_OUT_SWT_BIT, COMP2_OUT_SWT_WID);
}

static void vdin_channel_order_status(unsigned int offset)
{
	unsigned int c0, c1, c2;

	c0 = rd_bits(offset, VDIN_COM_CTRL0,
		     COMP0_OUT_SWT_BIT, COMP0_OUT_SWT_WID);
	c1 = rd_bits(offset, VDIN_COM_CTRL0,
		     COMP1_OUT_SWT_BIT, COMP1_OUT_SWT_WID);
	c2 = rd_bits(offset, VDIN_COM_CTRL0,
		     COMP2_OUT_SWT_BIT, COMP2_OUT_SWT_WID);
	pr_info("vdin(%x) current channel order is: %d %d %d\n",
		offset, c0, c1, c2);
}

const char *vdin_trans_matrix_str(enum vdin_matrix_csc_e csc_idx)
{
	switch (csc_idx) {
	case VDIN_MATRIX_XXX_YUV601_BLACK:
		return "VDIN_MATRIX_XXX_YUV601_BLACK";
	case VDIN_MATRIX_RGB_YUV601:
		return "VDIN_MATRIX_RGB_YUV601";
	case VDIN_MATRIX_GBR_YUV601:
		return "VDIN_MATRIX_GBR_YUV601";
	case VDIN_MATRIX_BRG_YUV601:
		return "VDIN_MATRIX_BRG_YUV601";
	case VDIN_MATRIX_YUV601_RGB:
		return "VDIN_MATRIX_YUV601_RGB";
	case VDIN_MATRIX_YUV601_GBR:
		return "VDIN_MATRIX_YUV601_GBR";
	case VDIN_MATRIX_YUV601_BRG:
		return "VDIN_MATRIX_YUV601_BRG";
	case VDIN_MATRIX_RGB_YUV601F:
		return "VDIN_MATRIX_RGB_YUV601F";
	case VDIN_MATRIX_YUV601F_RGB:
		return "VDIN_MATRIX_YUV601F_RGB";
	case VDIN_MATRIX_RGBS_YUV601:
		return "VDIN_MATRIX_RGBS_YUV601";
	case VDIN_MATRIX_YUV601_RGBS:
		return "VDIN_MATRIX_YUV601_RGBS";
	case VDIN_MATRIX_RGBS_YUV601F:
		return "VDIN_MATRIX_RGBS_YUV601F";
	case VDIN_MATRIX_YUV601F_RGBS:
		return "VDIN_MATRIX_YUV601F_RGBS";
	case VDIN_MATRIX_YUV601F_YUV601:
		return "VDIN_MATRIX_YUV601F_YUV601";
	case VDIN_MATRIX_YUV601_YUV601F:
		return "VDIN_MATRIX_YUV601_YUV601F";
	case VDIN_MATRIX_RGB_YUV709:
		return "VDIN_MATRIX_RGB_YUV709";
	case VDIN_MATRIX_YUV709_RGB:
		return "VDIN_MATRIX_YUV709_RGB";
	case VDIN_MATRIX_YUV709_GBR:
		return "VDIN_MATRIX_YUV709_GBR";
	case VDIN_MATRIX_YUV709_BRG:
		return "VDIN_MATRIX_YUV709_BRG";
	case VDIN_MATRIX_RGB_YUV709F:
		return "VDIN_MATRIX_RGB_YUV709F";
	case VDIN_MATRIX_YUV709F_RGB:
		return "VDIN_MATRIX_YUV709F_RGB";
	case VDIN_MATRIX_RGBS_YUV709:
		return "VDIN_MATRIX_RGBS_YUV709";
	case VDIN_MATRIX_YUV709_RGBS:
		return "VDIN_MATRIX_YUV709_RGBS";
	case VDIN_MATRIX_RGBS_YUV709F:
		return "VDIN_MATRIX_RGBS_YUV709F";
	case VDIN_MATRIX_YUV709F_RGBS:
		return "VDIN_MATRIX_YUV709F_RGBS";
	case VDIN_MATRIX_YUV709F_YUV709:
		return "VDIN_MATRIX_YUV709F_YUV709";
	case VDIN_MATRIX_YUV709_YUV709F:
		return "VDIN_MATRIX_YUV709_YUV709F";
	case VDIN_MATRIX_YUV601_YUV709:
		return "VDIN_MATRIX_YUV601_YUV709";
	case VDIN_MATRIX_YUV709_YUV601:
		return "VDIN_MATRIX_YUV709_YUV601";
	case VDIN_MATRIX_YUV601_YUV709F:
		return "VDIN_MATRIX_YUV601_YUV709F";
	case VDIN_MATRIX_YUV709F_YUV601:
		return "VDIN_MATRIX_YUV709F_YUV601";
	case VDIN_MATRIX_YUV601F_YUV709:
		return "VDIN_MATRIX_YUV601F_YUV709";
	case VDIN_MATRIX_YUV709_YUV601F:
		return "VDIN_MATRIX_YUV709_YUV601F";
	case VDIN_MATRIX_YUV601F_YUV709F:
		return "VDIN_MATRIX_YUV601F_YUV709F";
	case VDIN_MATRIX_YUV709F_YUV601F:
		return "VDIN_MATRIX_YUV709F_YUV601F";
	case VDIN_MATRIX_RGBS_RGB:
		return "VDIN_MATRIX_RGBS_RGB";
	case VDIN_MATRIX_RGB_RGBS:
		return "VDIN_MATRIX_RGB_RGBS";
	case VDIN_MATRIX_RGB2020_YUV2020:
		return "VDIN_MATRIX_RGB2020_YUV2020";
	case VDIN_MATRIX_YUV2020F_YUV2020:
		return "VDIN_MATRIX_YUV2020F_YUV2020";
	default:
		return "VDIN_MATRIX_NULL";
	}
};

const char *vdin_trans_irqflag_to_str(enum vdin_irq_flg_e flag)
{
	switch (flag) {
	case VDIN_IRQ_FLG_NO_END:
		return "VDIN_IRQ_FLG_NO_FRONT_END";
	case VDIN_IRQ_FLG_IRQ_STOP:
		return "VDIN_IRQ_FLG_IRQ_STOP";
	case VDIN_IRQ_FLG_FAKE_IRQ:
		return "VDIN_IRQ_FLG_FAKE_IRQ";
	case VDIN_IRQ_FLG_DROP_FRAME:
		return "VDIN_IRQ_FLG_DROP_FRAME";
	case VDIN_IRQ_FLG_DV_CHK_SUM_ERR:
		return "VDIN_IRQ_FLG_DV_CHK_SUM_ERR";
	case VDIN_IRQ_FLG_CYCLE_CHK:
		return "VDIN_IRQ_FLG_CYCLE_CHK";
	case VDIN_IRQ_FLG_SIG_NOT_STABLE:
		return "VDIN_IRQ_FLG_SIG_NOT_STABLE";
	case VDIN_IRQ_FLG_FMT_TRANS_CHG:
		return "VDIN_IRQ_FLG_FMT_TRANS_CHG";
	case VDIN_IRQ_FLG_CSC_CHG:
		return "VDIN_IRQ_FLG_CSC_CHG";
	case VDIN_IRQ_FLG_BUFF_SKIP:
		return "VDIN_IRQ_FLG_BUFF_SKIP";
	case VDIN_IRQ_FLG_IGNORE_FRAME:
		return "VDIN_IRQ_FLG_IGNORE_FRAME";
	case VDIN_IRQ_FLG_SKIP_FRAME:
		return "VDIN_IRQ_FLG_SKIP_FRAME";
	case VDIN_IRQ_FLG_GM_DV_CHK_SUM_ERR:
		return "VDIN_IRQ_FLG_GM_DV_CHK_SUM_ERR";
	case VDIN_IRQ_FLG_NO_WR_FE:
		return "VDIN_IRQ_FLG_NO_WR_FE";
	case VDIN_IRQ_FLG_NO_NEXT_FE:
		return "VDIN_IRQ_FLG_NO_NEXT_FE";
	default:
		return "VDIN_IRQ_FLAG_NULL";
	}
}

void vdin_dump_vs_info(struct vdin_dev_s *devp)
{
	unsigned int cnt = devp->unreliable_vs_cnt;

	if (devp->unreliable_vs_cnt > 10)
		cnt = 10;
	pr_info("unreliable_vs_cnt:%d\n", devp->unreliable_vs_cnt);
	if (devp->unreliable_vs_cnt > 0) {
		for (devp->unreliable_vs_idx = 0; devp->unreliable_vs_idx < cnt;
		     devp->unreliable_vs_idx++)
			pr_info("err t:%d\n",
				devp->unreliable_vs_time[devp->unreliable_vs_idx]);
	}
}

static void vdin_dump_state(struct vdin_dev_s *devp)
{
	unsigned int i;
	struct vframe_s *vf = &devp->curr_wr_vfe->vf;
	struct tvin_parm_s *curparm = &devp->parm;
	struct vf_pool *vfp = devp->vfp;
	unsigned int vframe_size;
	unsigned int offset = devp->addr_offset;

	if (devp->vfmem_size_small)
		vframe_size = devp->vfmem_size_small;
	else
		vframe_size = devp->vfmem_size;

	pr_info("flags=0x%x,flags_isr=0x%x\n", devp->flags, devp->flags_isr);
	pr_info("h_active = %d, v_active = %d\n",
		devp->h_active, devp->v_active);
	pr_info("canvas_w = %d, canvas_h = %d\n",
		devp->canvas_w, devp->canvas_h);
	pr_info("canvas_alin_w = %d, canvas_active_w = %d\n",
		devp->canvas_alin_w, devp->canvas_active_w);
	pr_info("double write cfg:0x%x, cur:%d,10bit sup: %d\n", devp->double_wr_cfg,
		devp->double_wr,
		devp->double_wr_10bit_sup);
	pr_info("secure_en: %d, mem protected: %d\n", devp->secure_en,
		devp->mem_protected);
	if (devp->cma_config_en != 1 || !(devp->cma_config_flag & 0x100))
		pr_info("mem_start = 0x%lx, mem_size = 0x%x\n",
			devp->mem_start, devp->mem_size);
	else
		for (i = 0; i < devp->canvas_max_num; i++)
			pr_info("buf[%d]mem_start = 0x%lx, mem_size = 0x%x\n",
				i, devp->vfmem_start[i], vframe_size);
	pr_info("signal format	= %s(0x%x)\n",
		tvin_sig_fmt_str(devp->parm.info.fmt),
		devp->parm.info.fmt);
	pr_info("prop.trans_fmt	= %s(%d)\n",
		tvin_trans_fmt_str(devp->prop.trans_fmt),
		devp->prop.trans_fmt);
	pr_info("prop.color_format= %s(%d)\n",
		tvin_color_fmt_str(devp->prop.color_format),
		devp->prop.color_format);
	pr_info("prop.dest_cfmt	= %s(%d)\n",
		tvin_color_fmt_str(devp->prop.dest_cfmt),
		devp->prop.dest_cfmt);
	pr_info("prop.color_fmt_range	= (%s)%d\n",
		tvin_trans_color_range_str(devp->prop.color_fmt_range),
		devp->prop.color_fmt_range);
	pr_info("prop.cnt = %d\n", devp->prop.cnt);
	pr_info("format_convert	= %s(%d)\n",
		vdin_fmt_convert_str(devp->format_convert),
		devp->format_convert);
	pr_info("vdin csc_idx	= %s(%d)\n",
		vdin_trans_matrix_str(devp->csc_idx),
		devp->csc_idx);
	pr_info("signal_type	= 0x%x\n", devp->parm.info.signal_type);
	pr_info("color_range_force = %d\n", color_range_force);
	pr_info("aspect_ratio = %s(%d)\ndecimation_ratio/dvi	= %u / %u\n",
		tvin_aspect_ratio_str(devp->prop.aspect_ratio),
		devp->prop.aspect_ratio,
		devp->prop.decimation_ratio, devp->prop.dvi_info);
	pr_info("[pre->cur]:hs(%d->%d),he(%d->%d),vs(%d->%d),ve(%d->%d)\n",
		devp->prop.pre_hs, devp->prop.hs,
		devp->prop.pre_he, devp->prop.he,
		devp->prop.pre_vs, devp->prop.vs,
		devp->prop.pre_ve, devp->prop.ve);
	pr_info("scaling4w:%d,scaling4h:%u\n", devp->prop.scaling4w, devp->prop.scaling4h);
	pr_info("report: hactive %d, vactive:%d, vtotal:%d\n",
		vdin_get_active_h(offset), vdin_get_active_v(offset),
		vdin_get_total_v(offset));
	pr_info("frontend_fps:%d\n", devp->prop.fps);
	pr_info("frontend_colordepth:%d\n", devp->prop.colordepth);
	pr_info("source_bitdepth:%d\n", devp->source_bitdepth);
	pr_info("color_depth_config:0x%x\n", devp->color_depth_config);
	pr_info("matrix_pattern_mode:0x%x\n", devp->matrix_pattern_mode);
	pr_info("hdcp_sts:0x%x\n", devp->prop.hdcp_sts);
	pr_info("full_pack:%d\n", devp->full_pack);
	pr_info("force_malloc_yuv_422_to_444:%d\n", devp->force_malloc_yuv_422_to_444);
	pr_info("color_depth_support:0x%x\n", devp->color_depth_support);
	pr_info("cma_flag:0x%x\n", devp->cma_config_flag);
	pr_info("auto_cutwindow_en:%d\n", devp->auto_cutwindow_en);
	pr_info("cutwindow_cfg:%d\n", devp->cutwindow_cfg);
	pr_info("cma_mem_alloc:%d\n", devp->cma_mem_alloc);
	pr_info("cma_mem_size:0x%x\n", devp->cma_mem_size);
	pr_info("cma_mem_mode:%d\n", devp->cma_mem_mode);
	pr_info("frame_buff_num:%d, vfmem_max_cnt:%d\n",
		devp->frame_buff_num, devp->vfmem_max_cnt);
	pr_info("force_yuv444_malloc:%d\n", devp->force_yuv444_malloc);
	pr_info("hdr_Flag =0x%x\n", devp->prop.vdin_hdr_flag);
	pr_info("hdr10p_Flag =0x%x\n", devp->prop.hdr10p_info.hdr10p_on);
	vdin_check_hdmi_hdr(devp);
	pr_info("cycle:%d, msr_clk_val:%d\n", devp->cycle, devp->msr_clk_val);

	vdin_dump_vf_state(devp->vfp);
	if (vf) {
		pr_info("current vframe index(%u):\n", vf->index);
		pr_info("\t buf(w%u, h%u),type(0x%x),flag(0x%x), duration(%d),",
		vf->width, vf->height, vf->type, vf->flag, vf->duration);
		pr_info("\t ratio_control(0x%x), signal_type:0x%x\n",
			vf->ratio_control, vf->signal_type);
		pr_info("\t trans fmt %u, left_start_x %u,",
			vf->trans_fmt, vf->left_eye.start_x);
		pr_info("\t right_start_x %u, width_x %u\n",
			vf->right_eye.start_x, vf->left_eye.width);
		pr_info("\t left_start_y %u, right_start_y %u, height_y %u\n",
			vf->left_eye.start_y, vf->right_eye.start_y,
			vf->left_eye.height);
		pr_info("vf compwidth:%u,compheight:%u\n", vf->compWidth,
			vf->compHeight);
		pr_info("CRC: 0x%x\n", vf->crc);
	}
	if (vfp) {
		pr_info("skip_vf_num:%d\n", vfp->skip_vf_num);
		pr_info("**************disp_mode**************\n");
		for (i = 0; i < VFRAME_DISP_MAX_NUM; i++)
			pr_info("[%d]:%-5d", i, vfp->disp_mode[i]);
		pr_info("\n**************disp_index**************\n");
		for (i = 0; i < VFRAME_DISP_MAX_NUM; i++)
			pr_info("[%d]:%-5d", i, vfp->disp_index[i]);
	}
	pr_info("\n current parameters:\n");
	pr_info("\t frontend of vdin index :  %d, 3d flag : 0x%x\n",
		curparm->index,  curparm->flag);
	pr_info("\t reserved 0x%x, devp->flags:0x%x\n",
		curparm->reserved, devp->flags);
	pr_info("max buffer num %u, msr_clk_val:%d.\n",
		devp->canvas_max_num, devp->msr_clk_val);
	pr_info("canvas buffer size %u, rdma_enable: %d.\n",
		devp->canvas_max_size, devp->rdma_enable);
	pr_info("range(%d),csc_cfg:0x%x,urgent_en:%d\n",
		devp->prop.color_fmt_range,
		devp->csc_cfg, devp->urgent_en);
	pr_info("black_bar_enable: %d, hist_bar_enable: %d, use_frame_rate: %d\n ",
		devp->black_bar_enable,
		devp->hist_bar_enable, devp->use_frame_rate);
	pr_info("vdin_rest_flag: %d, irq_cnt: %d, rdma_irq_cnt: %d\n",
		devp->vdin_reset_flag, devp->irq_cnt, devp->rdma_irq_cnt);
	pr_info("vdin_irq_flag:%d %s\n", devp->vdin_irq_flag,
		vdin_trans_irqflag_to_str(devp->vdin_irq_flag));
	pr_info("vpu crash irq cnt: %d\n", devp->vpu_crash_cnt);
	pr_info("write done: %d\n", devp->wr_done_irq_cnt);
	pr_info("meta write done: %d\n", devp->meta_wr_done_irq_cnt);
	pr_info("vdin_drop_cnt: %d frame_cnt:%d ignore_frames:%d\n",
		vdin_drop_cnt, devp->frame_cnt, devp->ignore_frames);
	pr_info("game_mode cfg :  0x%x\n", game_mode);
	pr_info("game_mode cur:  0x%x\n", devp->game_mode);
	pr_info("vrr_mode:  0x%x,vdin_vrr_en_flag=%d\n", devp->vrr_mode,
		devp->vrr_data.vdin_vrr_en_flag);
	pr_info("vrr_en:  pre=%d,cur:%d\n",
		devp->pre_prop.vtem_data.vrr_en, devp->prop.vtem_data.vrr_en);
	pr_info("vdin_vrr_flag:  pre=%d,cur:%d\n", devp->pre_prop.vdin_vrr_flag,
		devp->prop.vdin_vrr_flag);

	pr_info("afbce_flag: 0x%x\n", devp->afbce_flag);
	pr_info("afbce_mode: %d, afbce_valid: %d\n", devp->afbce_mode,
		devp->afbce_valid);
	pr_info("write vframe en: %d, pre: %d\n", devp->vframe_wr_en,
		devp->vframe_wr_en_pre);
	if (devp->afbce_mode == 1 || devp->double_wr) {
		for (i = 0; i < devp->vfmem_max_cnt; i++) {
			pr_info("head(%d) addr:0x%lx, size:0x%x\n",
				i, devp->afbce_info->fm_head_paddr[i],
				devp->afbce_info->frame_head_size);
		}

		for (i = 0; i < devp->vfmem_max_cnt; i++) {
			pr_info("table(%d) addr:0x%lx, size:0x%x\n",
				i, devp->afbce_info->fm_table_paddr[i],
				devp->afbce_info->frame_table_size);
		}

		for (i = 0; i < devp->vfmem_max_cnt; i++) {
			pr_info("body(%d) addr:0x%lx, size:0x%x\n",
				i, devp->afbce_info->fm_body_paddr[i],
				devp->afbce_info->frame_body_size);
		}
	}
	pr_info("dolby_input :  %d\n", devp->dv.dolby_input);
	/*if (devp->cma_config_en != 1 || !(devp->cma_config_flag & 0x100))*/
	/*	pr_info("dolby_mem_start = %ld, dolby_mem_size = %d\n",*/
	/*		(devp->mem_start +*/
	/*		devp->mem_size - devp->canvas_max_num * dolby_size_byte),*/
	/*		dolby_size_byte);*/
	/*else*/
	/*	for (i = 0; i < devp->canvas_max_num; i++)*/
	/*		pr_info("dolby_mem_start[%d] = %ld, dolby_mem_size = %d\n",*/
	/*			i, (devp->vfmem_start[i] + devp->vfmem_size -*/
	/*			dolby_size_byte), dolby_size_byte);*/

	for (i = 0; i < devp->canvas_max_num; i++)
		pr_info("dv_mem(%d):0x%x\n",
			devp->vfp->dv_buf_size[i], devp->vfp->dv_buf_mem[i]);

	pr_info("dvEn:%d,dv_flag:%d;dv_config:%d,dolby_ver:%d,low_latency:(%d,%d,%d) allm:%d\n",
		is_dolby_vision_enable(),
		devp->dv.dv_flag, devp->dv.dv_config, devp->prop.dolby_vision,
		devp->dv.low_latency, devp->prop.low_latency,
		devp->vfp->low_latency, devp->pre_prop.latency.allm_mode);
	pr_info("dv emp size:%d crc_flag:%d\n", devp->prop.emp_data.size,
		devp->dv.dv_crc_check);
	pr_info("size of struct vdin_dev_s: %d\n", devp->vdin_dev_ssize);
	pr_info("devp->dv.dv_vsif:(%d,%d,%d,%d,%d,%d,%d,%d);\n",
		devp->dv.dv_vsif.dobly_vision_signal,
		devp->dv.dv_vsif.backlt_ctrl_MD_present,
		devp->dv.dv_vsif.auxiliary_MD_present,
		devp->dv.dv_vsif.eff_tmax_PQ_hi,
		devp->dv.dv_vsif.eff_tmax_PQ_low,
		devp->dv.dv_vsif.auxiliary_runmode,
		devp->dv.dv_vsif.auxiliary_runversion,
		devp->dv.dv_vsif.auxiliary_debug0);
	pr_info("devp->prop.dv_vsif:(%d,%d,%d,%d,%d,%d,%d,%d)\n",
		devp->prop.dv_vsif.dobly_vision_signal,
		devp->prop.dv_vsif.backlt_ctrl_MD_present,
		devp->prop.dv_vsif.auxiliary_MD_present,
		devp->prop.dv_vsif.eff_tmax_PQ_hi,
		devp->prop.dv_vsif.eff_tmax_PQ_low,
		devp->prop.dv_vsif.auxiliary_runmode,
		devp->prop.dv_vsif.auxiliary_runversion,
		devp->prop.dv_vsif.auxiliary_debug0);
	pr_info("devp->vfp->dv_vsif:(%d,%d,%d,%d,%d,%d,%d,%d)\n",
		devp->vfp->dv_vsif.dobly_vision_signal,
		devp->vfp->dv_vsif.backlt_ctrl_MD_present,
		devp->vfp->dv_vsif.auxiliary_MD_present,
		devp->vfp->dv_vsif.eff_tmax_PQ_hi,
		devp->vfp->dv_vsif.eff_tmax_PQ_low,
		devp->vfp->dv_vsif.auxiliary_runmode,
		devp->vfp->dv_vsif.auxiliary_runversion,
		devp->vfp->dv_vsif.auxiliary_debug0);
	pr_info("rdma handle : %d\n", devp->rdma_handle);
	pr_info("hv reverse enabled: %d\n", devp->hv_reverse_en);
	pr_info("dbg_dump_frames: %d,dbg_stop_dec_delay:%d\n",
		devp->dbg_dump_frames, devp->dbg_stop_dec_delay);
	pr_info("Vdin driver version :  %s\n", VDIN_VER);
	/*vdin_dump_vs_info(devp);*/
}

static void vdin_dump_count(struct vdin_dev_s *devp)
{
	pr_info("irq_cnt: %d\n", devp->irq_cnt);
	pr_info("vpu crash irq: %d\n", devp->vpu_crash_cnt);
	pr_info("write done irq: %d\n", devp->wr_done_irq_cnt);
	pr_info("wr done abnormal_cnt: %d\n", devp->wr_done_abnormal_cnt);
	pr_info("puted_frame_cnt:%d\n", devp->puted_frame_cnt);
	pr_info("frame_cnt:%d\n", devp->frame_cnt);
	pr_info("ignore_frames:%d\n", devp->ignore_frames);
	pr_info("frame_drop_num:%d\n", devp->frame_drop_num);
	pr_info("vdin_drop_cnt: %d\n", vdin_drop_cnt);
	vdin_dump_vs_info(devp);
}

/*same as vdin_dump_state*/
static int seq_file_vdin_state_show(struct seq_file *seq, void *v)
{
	struct vdin_dev_s *devp;
	unsigned int i;
	struct vframe_s *vf;
	struct tvin_parm_s *curparm;
	struct vf_pool *vfp;

	devp = vdin_get_dev(0);
	vf = &devp->curr_wr_vfe->vf;
	curparm = &devp->parm;
	vfp = devp->vfp;

	seq_printf(seq, "h_active = %d, v_active = %d\n",
		   devp->h_active, devp->v_active);
	seq_printf(seq, "canvas_w = %d, canvas_h = %d\n",
		   devp->canvas_w, devp->canvas_h);
	seq_printf(seq, "canvas_alin_w = %d, canvas_active_w = %d\n",
		   devp->canvas_alin_w, devp->canvas_active_w);
	if (devp->cma_config_en != 1 || !(devp->cma_config_flag & 0x1))
		seq_printf(seq, "mem_start = %ld, mem_size = %d\n",
			   devp->mem_start, devp->mem_size);
	else
		for (i = 0; i < devp->canvas_max_num; i++)
			seq_printf(seq, "buf[%d]mem_start = %ld, mem_size = %d\n",
				   i, devp->vfmem_start[i], devp->vfmem_size);
	seq_printf(seq, "signal format	= %s(0x%x)\n",
		   tvin_sig_fmt_str(devp->parm.info.fmt),
		   devp->parm.info.fmt);
	seq_printf(seq, "trans_fmt	= %s(%d)\n",
		   tvin_trans_fmt_str(devp->prop.trans_fmt),
		   devp->prop.trans_fmt);
	seq_printf(seq, "color_format	= %s(%d)\n",
		   tvin_color_fmt_str(devp->prop.color_format),
		   devp->prop.color_format);
	seq_printf(seq, "format_convert = %s(%d)\n",
		   vdin_fmt_convert_str(devp->format_convert),
		   devp->format_convert);
	seq_printf(seq, "aspect_ratio	= %s(%d)\ndecimation_ratio/dvi	= %u / %u\n",
		   tvin_aspect_ratio_str(devp->prop.aspect_ratio),
		   devp->prop.aspect_ratio,
		   devp->prop.decimation_ratio, devp->prop.dvi_info);
	seq_printf(seq, "[pre->cur]:hs(%d->%d),he(%d->%d),vs(%d->%d),ve(%d->%d)\n",
		   devp->prop.pre_hs, devp->prop.hs,
		   devp->prop.pre_he, devp->prop.he,
		   devp->prop.pre_vs, devp->prop.vs,
		   devp->prop.pre_ve, devp->prop.ve);
	seq_printf(seq, "frontend_fps:%d parm fps:%d\n", devp->prop.fps, devp->parm.info.fps);
	seq_printf(seq, "frontend_colordepth:%d\n", devp->prop.colordepth);
	seq_printf(seq, "source_bitdepth:%d\n", devp->source_bitdepth);
	seq_printf(seq, "color_depth_config:0x%x\n", devp->color_depth_config);
	seq_printf(seq, "full_pack:%d\n", devp->full_pack);
	seq_printf(seq, "force_malloc_yuv_422_to_444:%d\n", devp->force_malloc_yuv_422_to_444);
	seq_printf(seq, "color_depth_support:0x%x\n",
		   devp->color_depth_support);
	seq_printf(seq, "cma_flag:0x%x\n", devp->cma_config_flag);
	seq_printf(seq, "auto_cutwindow_en:%d\n", devp->auto_cutwindow_en);
	seq_printf(seq, "cma_mem_alloc:%d\n", devp->cma_mem_alloc);
	seq_printf(seq, "cma_mem_size:0x%x\n", devp->cma_mem_size);
	seq_printf(seq, "cma_mem_mode:%d\n", devp->cma_mem_mode);
	seq_printf(seq, "force_yuv444_malloc:%d\n", devp->force_yuv444_malloc);
	vdin_dump_vf_state_seq(devp->vfp, seq);
	if (vf) {
		seq_printf(seq, "current vframe index(%u):\n", vf->index);
		seq_printf(seq, "\t buf(w%u, h%u),type(0x%x),flag(0x%x), duration(%d),\n",
			    vf->width, vf->height, vf->type, vf->flag, vf->duration);
		seq_printf(seq, "\t ratio_control(0x%x).\n", vf->ratio_control);
		seq_printf(seq, "\t trans fmt %u, left_start_x %u,\n",
			   vf->trans_fmt, vf->left_eye.start_x);
		seq_printf(seq, "\t right_start_x %u, width_x %u\n",
			   vf->right_eye.start_x, vf->left_eye.width);
		seq_printf(seq, "\t left_start_y %u, right_start_y %u, height_y %u\n",
			   vf->left_eye.start_y, vf->right_eye.start_y,
			   vf->left_eye.height);
	}
	if (vfp) {
		seq_printf(seq, "skip_vf_num:%d\n", vfp->skip_vf_num);
		seq_puts(seq, "**************disp_mode**************\n");
		for (i = 0; i < VFRAME_DISP_MAX_NUM; i++)
			seq_printf(seq, "[%d]:%-5d\n", i, vfp->disp_mode[i]);
		seq_puts(seq, "\n**************disp_index**************\n");
		for (i = 0; i < VFRAME_DISP_MAX_NUM; i++)
			seq_printf(seq, "[%d]:%-5d\n", i, vfp->disp_index[i]);
	}
	seq_puts(seq, "\n current parameters:\n");
	seq_printf(seq, "\t frontend of vdin index :  %d, 3d flag : 0x%x\n",
		   curparm->index,  curparm->flag);
	seq_printf(seq, "\t reserved 0x%x, devp->flags:0x%x\n",
		   curparm->reserved, devp->flags);
	seq_printf(seq, "max buffer num %u, msr_clk_val:%d.\n",
		   devp->canvas_max_num, devp->msr_clk_val);
	seq_printf(seq, "canvas buffer size %u, rdma_enable: %d.\n",
		   devp->canvas_max_size, devp->rdma_enable);
	seq_printf(seq, "range(%d),csc_cfg:0x%x,urgent_en:%d\n",
		   devp->prop.color_fmt_range,
		   devp->csc_cfg, devp->urgent_en);
	seq_printf(seq, "black_bar_enable: %d, hist_bar_enable: %d, use_frame_rate: %d\n ",
		   devp->black_bar_enable,
		   devp->hist_bar_enable, devp->use_frame_rate);
	seq_printf(seq, "vdin_irq_flag: %d, vdin_rest_flag: %d, irq_cnt: %d, rdma_irq_cnt: %d\n",
		   devp->vdin_irq_flag, devp->vdin_reset_flag,
		   devp->irq_cnt, devp->rdma_irq_cnt);
	seq_printf(seq, "vpu crash irq cnt: %d\n", devp->vpu_crash_cnt);
	seq_printf(seq, "rdma_enable :  %d\n", devp->rdma_enable);
	seq_printf(seq, "dolby_input :  %d\n", devp->dv.dolby_input);
	if (devp->cma_config_en != 1 || !(devp->cma_config_flag & 0x100))
		seq_printf(seq, "dolby_mem_start = %ld, dolby_mem_size = %d\n",
			   (devp->mem_start +
			    devp->mem_size -
			    devp->canvas_max_num * dolby_size_byte),
			   dolby_size_byte);
	else
		for (i = 0; i < devp->canvas_max_num; i++)
			seq_printf(seq, "dolby_mem_start[%d] = %ld, dolby_mem_size = %d\n",
				   i, (devp->vfmem_start[i] + devp->vfmem_size -
				   dolby_size_byte), dolby_size_byte);
	for (i = 0; i < devp->canvas_max_num; i++) {
		seq_printf(seq, "dv_mem(%d):0x%x\n",
			   devp->vfp->dv_buf_size[i],
			   devp->vfp->dv_buf_mem[i]);
	}
	seq_printf(seq, "dv_flag:%d;dv_config:%d,dolby_vision:%d\n",
		   devp->dv.dv_flag, devp->dv.dv_config,
		   devp->prop.dolby_vision);
	seq_printf(seq, "size of struct vdin_dev_s: %d\n",
		   devp->vdin_dev_ssize);
	seq_printf(seq, "Vdin driver version :  %s\n", VDIN_VER);

	return 0;
}

static void vdin_dump_histgram(struct vdin_dev_s *devp)
{
	uint i;

	pr_info("%s:\n", __func__);
	for (i = 0; i < 64; i++) {
		pr_info("[%d]0x%-8x\t", i, devp->parm.histgram[i]);
		if ((i + 1) % 8 == 0)
			pr_info("\n");
	}
}

/*type: 1:nv21 2:yuv422 3:yuv444*/
static void vdin_write_afbce_mem(struct vdin_dev_s *devp, char *type,
				 char *path)
{
	char md_path_head[100], md_path_body[100];
	unsigned int i, j;
	int highmem_flag = 0;
	unsigned int size = 0;
	unsigned int span = 0;
	unsigned int remain = 0;
	unsigned int count = 0;
	unsigned long highaddr;
	unsigned long phys;
	long val;
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs;
	void *head_dts = NULL;
	void *body_dts = NULL;
	void *vbuf = NULL;

	if (kstrtol(type, 10, &val) < 0)
		return;
	if (!path)
		return;

	if (!devp->curr_wr_vfe) {
		devp->curr_wr_vfe = provider_vf_get(devp->vfp);
		if (!devp->curr_wr_vfe) {
			pr_info("no buffer to write.\n");
			return;
		}
	}

	sprintf(md_path_head, "%s_1header.bin", path);
	sprintf(md_path_body, "%s_1body.bin", path);

	i = devp->curr_wr_vfe->af_num;
	devp->curr_wr_vfe->vf.type = VIDTYPE_VIU_SINGLE_PLANE |
			VIDTYPE_VIU_FIELD | VIDTYPE_COMPRESS | VIDTYPE_SCATTER;
	switch (val) {
	case 1:
		devp->curr_wr_vfe->vf.type |= VIDTYPE_VIU_NV21;
		break;
	case 2:
		devp->curr_wr_vfe->vf.type |= VIDTYPE_VIU_422;
		break;
	case 3:
		devp->curr_wr_vfe->vf.type |= VIDTYPE_VIU_444;
		break;
	default:
		devp->curr_wr_vfe->vf.type |= VIDTYPE_VIU_422;
		break;
	}

	devp->curr_wr_vfe->vf.compHeadAddr = devp->afbce_info->fm_head_paddr[i];
	devp->curr_wr_vfe->vf.compBodyAddr = devp->afbce_info->fm_body_paddr[i];

	highmem_flag = PageHighMem(phys_to_page(devp->afbce_info->fm_body_paddr[0]));
	if (highmem_flag == 0) {
		pr_info("low mem area\n");
		head_dts =
			codec_mm_phys_to_virt(devp->afbce_info->fm_head_paddr[i]);
	} else {
		pr_info("high mem area\n");
		head_dts = vdin_vmap(devp->afbce_info->fm_head_paddr[i],
				     devp->afbce_info->frame_head_size);
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pr_info("head bin file path = %s\n", md_path_head);
	filp = filp_open(md_path_head, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("read %s error or filp is NULL.\n", md_path_head);
		return;
	}

	size = vfs_read(filp, head_dts,
			devp->afbce_info->frame_head_size, &pos);
	if (highmem_flag)
		vdin_unmap_phyaddr(head_dts);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);

	pos = 0;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pr_info("body bin file path = %s\n", md_path_body);
	filp = filp_open(md_path_body, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("read %s error or filp is NULL.\n", md_path_body);
		return;
	}

	if (highmem_flag == 0) {
		body_dts =
		codec_mm_phys_to_virt(devp->afbce_info->fm_body_paddr[i]);

		size = vfs_read(filp, body_dts,
				devp->afbce_info->frame_body_size, &pos);
	} else {
		span = SZ_1M;
		count = devp->afbce_info->frame_body_size / PAGE_ALIGN(span);
		remain = devp->afbce_info->frame_body_size % PAGE_ALIGN(span);
		phys = devp->afbce_info->fm_body_paddr[i];

		for (j = 0; j < count; j++) {
			highaddr = phys + j * span;
			vbuf = vdin_vmap(highaddr, span);
			if (!vbuf) {
				pr_info("vdin_vmap error\n");
				return;
			}
			vfs_read(filp, vbuf, span, &pos);
			vdin_unmap_phyaddr(vbuf);
		}
	}

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);

	provider_vf_put(devp->curr_wr_vfe, devp->vfp);
	devp->curr_wr_vfe = NULL;
	vf_notify_receiver(devp->name, VFRAME_EVENT_PROVIDER_VFRAME_READY,
			   NULL);
}

static void vdin_write_mem(struct vdin_dev_s *devp, char *type,
			   char *path, char *md_path)
{
	unsigned int size = 0, vtype = 0;
	struct file *filp = NULL,  *md_flip = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs;
	void *dts = NULL;
	long val;
	int index, j, span;
	int highmem_flag, count;
	unsigned long addr;
	unsigned long highaddr;
	struct vf_pool *p = devp->vfp;
	/* vtype = simple_strtol(type, NULL, 10); */

	if (kstrtol(type, 10, &val) < 0)
		return;

	vtype = val;
	if (!devp->curr_wr_vfe) {
		devp->curr_wr_vfe = provider_vf_get(devp->vfp);
		if (!devp->curr_wr_vfe) {
			pr_info("no buffer to write.\n");
			return;
		}
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pr_info("bin file path = %s\n", path);
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("read %s error or filp is NULL.\n", path);
		return;
	}
	devp->curr_wr_vfe->vf.type = VIDTYPE_VIU_SINGLE_PLANE |
			VIDTYPE_VIU_FIELD | VIDTYPE_VIU_422;
	if (vtype == 1) {
		devp->curr_wr_vfe->vf.type |= VIDTYPE_INTERLACE_TOP;
		pr_info("current vframe type is top.\n");
	} else if (vtype == 3) {
		devp->curr_wr_vfe->vf.type |= VIDTYPE_INTERLACE_BOTTOM;
		pr_info("current vframe type is bottom.\n");
	} else {
		devp->curr_wr_vfe->vf.type |= VIDTYPE_PROGRESSIVE;
	}
	if (vtype == 6) {
		devp->curr_wr_vfe->vf.bitdepth = BITDEPTH_Y10;
		devp->curr_wr_vfe->vf.bitdepth |= FULL_PACK_422_MODE;
		devp->curr_wr_vfe->vf.signal_type = 0x91000;
		devp->curr_wr_vfe->vf.source_type = VFRAME_SOURCE_TYPE_HDMI;
	}
	if (vtype == 7 || vtype == 8) {
		devp->curr_wr_vfe->vf.type = 0x7000;
		devp->curr_wr_vfe->vf.bitdepth = 0;
		devp->curr_wr_vfe->vf.signal_type = 0x10100;
		devp->curr_wr_vfe->vf.source_type = VFRAME_SOURCE_TYPE_HDMI;
	}
	addr = canvas_get_addr(devp->curr_wr_vfe->vf.canvas0Addr);
	/* dts = ioremap(canvas_get_addr(devp->curr_wr_vfe->vf.canvas0Addr), */
	/* real_size); */

	if (devp->cma_config_flag & 0x100)
		highmem_flag = PageHighMem(phys_to_page(devp->vfmem_start[0]));
	else
		highmem_flag = PageHighMem(phys_to_page(devp->mem_start));

	if (highmem_flag == 0) {
		pr_info("low mem area,addr:%lx\n", addr);
		dts = phys_to_virt(addr);
		for (j = 0; j < devp->canvas_h; j++) {
			vfs_read(filp, dts + (devp->canvas_w * j),
				 devp->canvas_active_w, &pos);
		}
		vfs_fsync(filp, 0);
		iounmap(dts);
		filp_close(filp, NULL);
		set_fs(old_fs);
	} else {
		pr_info("high mem area\n");
		count = devp->canvas_h;
		span = devp->canvas_active_w;
		for (j = 0; j < count; j++) {
			highaddr = addr + j * devp->canvas_w;
			dts = vdin_vmap(highaddr, span);
			if (!dts) {
				pr_info("vdin_vmap error\n");
				return;
			}
			vfs_read(filp, dts, span, &pos);
			vdin_dma_flush(devp, dts, span, DMA_TO_DEVICE);
			vdin_unmap_phyaddr(dts);
		}
		vfs_fsync(filp, 0);
		filp_close(filp, NULL);
		set_fs(old_fs);
	}

	if (vtype == 8) {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		pr_info("md file path = %s\n", md_path);
		md_flip = filp_open(md_path, O_RDONLY, 0);
		if (IS_ERR_OR_NULL(md_flip)) {
			pr_info("read %s error or md_flip = NULL.\n", md_path);
			return;
		}
		index = devp->curr_wr_vfe->vf.index & 0xff;
		if (index != 0xff && index >= 0 && index < p->size) {
			u8 *c = devp->vfp->dv_buf_vmem[index];

			pos = 0;
			size = (unsigned int)
			vfs_read(md_flip, devp->vfp->dv_buf_vmem[index],
				 4096, &pos);
			p->dv_buf_size[index] = size;
			devp->vfp->dv_buf[index] = &c[0];
		}
		vfs_fsync(md_flip, 0);
		filp_close(md_flip, NULL);
		set_fs(old_fs);
	}

	provider_vf_put(devp->curr_wr_vfe, devp->vfp);
	devp->curr_wr_vfe = NULL;
	vf_notify_receiver(devp->name, VFRAME_EVENT_PROVIDER_VFRAME_READY,
			   NULL);
}

static void vdin_write_cont_mem(struct vdin_dev_s *devp, char *type,
				char *path, char *num)
{
	unsigned int size = 0, i = 0;
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs;
	void *dts = NULL;
	long field_num;
	unsigned long addr;

	/* vtype = simple_strtol(type, NULL, 10); */

	if (kstrtol(num, 10, &field_num) < 0)
		return;

	if (!devp->curr_wr_vfe) {
		devp->curr_wr_vfe = provider_vf_get(devp->vfp);
		if (!devp->curr_wr_vfe) {
			pr_info("no buffer to write.\n");
			return;
		}
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pr_info("bin file path = %s\n", path);
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("read %s error or filp is NULL.\n", path);
		return;
	}
	for (i = 0; i < field_num; i++) {
		if (!devp->curr_wr_vfe) {
			devp->curr_wr_vfe = provider_vf_get(devp->vfp);
			if (!devp->curr_wr_vfe) {
				pr_info("no buffer to write.\n");
				return;
			}
		}
		if (!strcmp("top", type)) {
			if (i % 2 == 0) {
				devp->curr_wr_vfe->vf.type |=
					VIDTYPE_INTERLACE_TOP;
				pr_info("current vframe type is top.\n");
			} else {
				devp->curr_wr_vfe->vf.type |=
					VIDTYPE_INTERLACE_BOTTOM;
				pr_info("current vframe type is bottom.\n");
			}
		} else if (!strcmp("bot", type)) {
			if (i % 2 == 1) {
				devp->curr_wr_vfe->vf.type |=
					VIDTYPE_INTERLACE_TOP;
				pr_info("current vframe type is top.\n");
			} else {
				devp->curr_wr_vfe->vf.type |=
					VIDTYPE_INTERLACE_BOTTOM;
				pr_info("current vframe type is bottom.\n");
			}
		} else {
			devp->curr_wr_vfe->vf.type |= VIDTYPE_PROGRESSIVE;
		}
		addr = canvas_get_addr(devp->curr_wr_vfe->vf.canvas0Addr);
		/* real_size); */
		dts = phys_to_virt(addr);
		size = vfs_read(filp, dts, devp->canvas_max_size, &pos);
		if (size < devp->canvas_max_size) {
			pr_info("%s read %u < %u error.\n",
				__func__, size, devp->canvas_max_size);
			return;
		}
		vfs_fsync(filp, 0);
		provider_vf_put(devp->curr_wr_vfe, devp->vfp);
		devp->curr_wr_vfe = NULL;
		vf_notify_receiver(devp->name,
				   VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
		iounmap(dts);
	}
	filp_close(filp, NULL);
	set_fs(old_fs);
}

static void dump_dolby_metadata(struct vdin_dev_s *devp)
{
	unsigned int i, j;
	char *p;

	pr_info("*****dolby_metadata(%d byte):*****\n", dolby_size_byte);
	for (i = 0; i < devp->canvas_max_num; i++) {
		pr_info("*****dolby_metadata(%d)[0x%x](%d byte):*****\n",
			i, devp->vfp->dv_buf_mem[i], dolby_size_byte);
		for (j = 0; j < dolby_size_byte; j++) {
			if ((j % 16) == 0)
				pr_info("\n");
			p = devp->vfp->dv_buf_vmem[i];
			pr_info("0x%02x\t", *(p + j));
			/*pr_info("0x%02x\t", *(devp->vfp->dv_buf_ori[i] + j));*/

		}
	}
}

static void dump_dolby_buf_clean(struct vdin_dev_s *devp)
{
	/*unsigned int i;*/

	/*for (i = 0; i < devp->canvas_max_num; i++)*/
	/*	memset(devp->vfp->dv_buf_ori[i], 0, dolby_size_byte);*/
}

#ifdef CONFIG_AML_LOCAL_DIMMING

static void vdin_dump_histgram_ldim(struct vdin_dev_s *devp,
				    unsigned int hnum, unsigned int vnum)
{
	uint i, j;
	unsigned int local_ldim_max[100] = {0};

	/*memcpy(&local_ldim_max[0], &vdin_ldim_max_global[0],
	 *100*sizeof(unsigned int));
	 */
	pr_info("%s:\n", __func__);
	for (i = 0; i < hnum; i++) {
		for (j = 0; j < vnum; j++) {
			pr_info("(%d,%d,%d)\t",
				local_ldim_max[j + i * 10] & 0x3ff,
				(local_ldim_max[j + i * 10] >> 10) & 0x3ff,
				(local_ldim_max[j + i * 10] >> 20) & 0x3ff);
		}
		pr_info("\n");
	}
}
#endif

static void vdin_dump_regs(struct vdin_dev_s *devp, u32 size)
{
	unsigned int reg;
	unsigned int offset = devp->addr_offset;

	if (size) {
		pr_info("vdin dump reg value:%d\n", size);
		if (size > 256)
			size = 256;
		for (reg = VDIN_SCALE_COEF_IDX;
		     reg <= VDIN_SCALE_COEF_IDX + size; reg++)
			pr_info("0x%04x = 0x%08x\n",
				(reg + offset), rd(offset, reg));
		return;
	}

	pr_info("vdin%d regs start----\n", devp->index);
	for (reg = VDIN_SCALE_COEF_IDX; reg <= VDIN_COM_STATUS3; reg++)
		pr_info("0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
	pr_info("vdin%d regs end----\n\n", devp->index);

	if (is_meson_tm2_cpu()) {
		pr_info("vdin%d HDR2 regs start----\n", devp->index);
		for (reg = VDIN_HDR2_CTRL;
		     reg <= VDIN_HDR2_MATRIXO_EN_CTRL; reg++) {
			pr_info("0x%04x = 0x%08x\n",
				(reg + offset), rd(offset, reg));
		}
		pr_info("vdin%d HDR2 regs end----\n\n", devp->index);

		pr_info("vdin descramble scramble----start\n");
		for (reg = VDIN_DSC_CTRL; reg <= VDIN_DSC_TUNNEL_SEL; reg++) {
			pr_info("0x%04x = 0x%08x\n",
				(reg + offset), rd(offset, reg));
		}
		pr_info("vdin descramble scramble----end\n\n");

		pr_info("vdin%d DV regs start----\n", devp->index);
		for (reg = VDIN_DOLBY_DSC_CTRL0;
		     reg <= VDIN_DOLBY_DSC_STATUS2; reg++) {
			pr_info("0x%04x = 0x%08x\n",
				(reg + offset), rd(offset, reg));
		}

		pr_info("0x%04x = 0x%08x\n",
			(VDIN_DOLBY_DSC_STATUS3 + offset),
			rd(offset, VDIN_DOLBY_DSC_STATUS3));
		pr_info("vdin%d DV regs end----\n", devp->index);
	}

	if (devp->dtdata->hw_ver >= VDIN_HW_T7) {
		for (reg = VDIN_WR_BADDR_LUMA;
		     reg <= VDIN_WR_STRIDE_CHROMA; reg++) {
			pr_info("0x%04x = 0x%08x\n",
				(reg + offset), rd(offset, reg));
		}
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
		pr_info("vdin%d h/v shrk regs start----\n", devp->index);
		reg = VDIN_VSHRK_SIZE_M1;
		pr_info("0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
		for (reg = VDIN_HSK_CTRL; reg <= HSK_COEF_15; reg++) {
			pr_info("0x%04x = 0x%08x\n",
				(reg + offset), rd(offset, reg));
		}
		pr_info("vdin%d h/v sk regs end----\n\n", devp->index);

		reg = VDIN_TOP_DOUBLE_CTRL;
		pr_info("0x%04x = 0x%08x\n\n", (reg), R_VCBUS(reg));

		for (reg = VDIN2_WR_CTRL; reg <= VDIN_TOP_MEAS_RO_PIXB; reg++) {
			pr_info("0x%04x = 0x%08x\n",
				(reg), R_VCBUS(reg));
		}
	}

	if (devp->afbce_flag & VDIN_AFBCE_EN) {
		pr_info("vdin%d afbce regs start----\n", devp->index);
		for (reg = AFBCE_ENABLE; reg <= AFBCE_MMU_RMIF_RO_STAT;
			reg++) {
			pr_info("0x%04x = 0x%08x\n", (reg), R_VCBUS(reg));
		}
		pr_info("vdin%d afbce regs end----\n\n", devp->index);
	}
	reg = VDIN_MISC_CTRL;
	pr_info("0x%04x = 0x%08x\n", (reg), R_VCBUS(reg));
	pr_info("\nwrite back reg ---\n");
	pr_info("0x%04x = 0x%08x\n", 0x271a, R_VCBUS(0x271a));
	pr_info("0x%04x = 0x%08x\n", 0x1a0d, R_VCBUS(0x1a0d));
	pr_info("0x%04x = 0x%08x\n", 0x1a51, R_VCBUS(0x1a51));
	pr_info("0x%04x = 0x%08x\n", 0x1df9, R_VCBUS(0x1df9));
	pr_info("0x%04x = 0x%08x\n", 0x2783, R_VCBUS(0x2783));
}

void vdin_test_front_end(void)
{
	struct tvin_frontend_s *fe = tvin_get_frontend(TVIN_PORT_HDMI0,
		VDIN_FRONTEND_IDX);

	if (fe->sm_ops && fe->sm_ops->vdin_set_property)
		fe->sm_ops->vdin_set_property(fe);
}

/*
 * 1.show the current frame rate
 * echo fps >/sys/class/vdin/vdinx/attr
 * 2.dump the data from vdin memory
 * echo capture dir >/sys/class/vdin/vdinx/attr
 * 3.start the vdin hardware
 * echo tvstart/v4l2start port fmt_id/resolution(width height frame_rate) >dir
 * 4.freeze the vdin buffer
 * echo freeze/unfreeze >/sys/class/vdin/vdinx/attr
 * 5.enable vdin0-nr path or vdin0-mem
 * echo output2nr >/sys/class/vdin/vdin0/attr
 * echo output2mem >/sys/class/vdin/vdin0/attr
 * 6.modify for vdin fmt & color fmt conversion
 * echo conversion w h cfmt >/sys/class/vdin/vdin0/attr
 */
static ssize_t attr_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t len)
{
	unsigned int fps = 0;
	char ret = 0, *buf_orig, *parm[47] = {NULL};
	struct vdin_dev_s *devp;
	unsigned int time_start, time_end, time_delta;
	long val = 0;
	unsigned int temp;
	unsigned int mode = 0, flag = 0;
	unsigned int offset;

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	devp = dev_get_drvdata(dev);
	vdin_parse_param(buf_orig, (char **)&parm);
	offset = devp->addr_offset;

	if (!strncmp(parm[0], "fps", 3)) {
		if (devp->cycle)
			fps = (devp->msr_clk_val +
			       (devp->cycle >> 3)) / devp->cycle;
		pr_info("%u f/s\n", fps);
		pr_info("hdmirx: %u f/s\n", devp->parm.info.fps);
	} else if (!strcmp(parm[0], "capture")) {
		if (parm[3]) {
			unsigned int start = 0, offset = 0;

			if (kstrtol(parm[2], 16, &val) == 0)
				start = val;
			if (kstrtol(parm[3], 16, &val) == 0)
				offset = val;
			dump_other_mem(devp, parm[1], start, offset);
		} else if (parm[2]) {
			unsigned int buf_num = 0;

			if (kstrtol(parm[2], 10, &val) == 0)
				buf_num = val;
			vdin_dump_one_buf_mem(parm[1], devp, buf_num);
		} else if (parm[1]) {
			vdin_dump_mem(parm[1], devp);
		}
	} else if  (!strcmp(parm[0], "request_irq")) {
		snprintf(devp->irq_name, sizeof(devp->irq_name),
			 "vdin%d-irq", devp->index);
		if (!(devp->flags & VDIN_FLAG_ISR_REQ)) {
			ret = request_irq(devp->irq, vdin_isr, IRQF_SHARED,
					  devp->irq_name, (void *)devp);
			disable_irq(devp->irq);
			devp->flags |= VDIN_FLAG_ISR_REQ;
		}
		pr_info("req ISR flag:0x%x\n", devp->flags);
	} else if  (!strcmp(parm[0], "free_irq")) {
		free_irq(devp->irq, (void *)devp);
		pr_info("free irq\n");
	} else if (!strcmp(parm[0], "tvstart")) {
		unsigned int port = 0, fmt = 0;

		if (!parm[2]) {
			kfree(buf_orig);
			return len;
		}
		if (kstrtol(parm[1], 16, &val) == 0)
			port = val;
		switch (port) {
		case 0:/* HDMI0 */
			port = TVIN_PORT_HDMI0;
			break;
		case 1:/* HDMI1 */
			port = TVIN_PORT_HDMI1;
			break;
		case 2:/* HDMI2 */
			port = TVIN_PORT_HDMI2;
			break;
		case 5:/* CVBS0 */
			port = TVIN_PORT_CVBS0;
			break;
		case 6:/* CVBS1 */
			port = TVIN_PORT_CVBS1;
			break;
		case 8:/* CVBS2 */
			port = TVIN_PORT_CVBS2;
			break;
		case 9:/* CVBS3 as atvdemod to cvd */
			port = TVIN_PORT_CVBS3;
			break;
		default:
			port = TVIN_PORT_CVBS0;
			break;
		}
		if (kstrtol(parm[2], 16, &val) == 0)
			fmt = val;

		/* devp->flags |= VDIN_FLAG_FS_OPENED; */
		/* request irq */
		/*snprintf(devp->irq_name, sizeof(devp->irq_name),*/
		/*	 "vdin%d-irq", devp->index);*/
		/*pr_info("vdin work in normal mode\n");*/
		/*ret = request_irq(devp->irq, vdin_isr, IRQF_SHARED,*/
		/*		devp->irq_name, (void *)devp);*/

		/*if (vdin_dbg_en)*/
		/*	pr_info("%s vdin.%d request_irq\n", __func__,*/
		/*		devp->index);*/

		/*disable irq until vdin is configured completely*/
		/*disable_irq_nosync(devp->irq);*/

		if (vdin_dbg_en)
			pr_info("%s vdin.%d disable_irq_nosync\n", __func__,
				devp->index);
		/*init queue*/
		init_waitqueue_head(&devp->queue);
		/* remove the hardware limit to vertical [0-max]*/
		/* WRITE_VCBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000fff); */
		/* pr_info("open device %s ok\n", dev_name(devp->dev)); */
		/* vdin_open_fe(port, 0, devp); */
		devp->parm.port = port;
		devp->parm.info.fmt = fmt;
		devp->fmt_info_p  = (struct tvin_format_s *)
				tvin_get_fmt_info(fmt);

		devp->unstable_flag = false;
		ret = vdin_open_fe(devp->parm.port, 0, devp);
		if (ret) {
			pr_err("TVIN_IOC_OPEN(%d) failed to open port 0x%x\n",
			       devp->index, devp->parm.port);
		}
		devp->flags |= VDIN_FLAG_DEC_OPENED;
		pr_info("TVIN_IOC_OPEN() port %s opened ok\n\n",
			tvin_port_str(devp->parm.port));

		time_start = jiffies_to_msecs(jiffies);
start_chk:
		if (devp->flags & VDIN_FLAG_DEC_STARTED) {
			pr_info("TVIN_IOC_START_DEC() port 0x%x, started already\n",
				devp->parm.port);
		}
		time_end = jiffies_to_msecs(jiffies);
		time_delta = time_end - time_start;
		if ((devp->parm.info.status != TVIN_SIG_STATUS_STABLE ||
		     devp->parm.info.fmt == TVIN_SIG_FMT_NULL) &&
		    time_delta <= 5000)
			goto start_chk;

		pr_info("status: %s, fmt: %s\n",
			tvin_sig_status_str(devp->parm.info.status),
			tvin_sig_fmt_str(devp->parm.info.fmt));
		if (devp->parm.info.fmt != TVIN_SIG_FMT_NULL)
			fmt = devp->parm.info.fmt; /* = parm.info.fmt; */
		devp->fmt_info_p  =
			(struct tvin_format_s *)tvin_get_fmt_info(fmt);
		/* devp->fmt_info_p  =
		 * tvin_get_fmt_info(devp->parm.info.fmt);
		 */
		if (!devp->fmt_info_p) {
			pr_info("TVIN_IOC_START_DEC(%d) error, fmt is null\n",
				devp->index);
		}
		if (!(devp->flags & VDIN_FLAG_ISR_REQ)) {
			ret = request_irq(devp->irq, vdin_isr, IRQF_SHARED,
					  devp->irq_name, (void *)devp);
			disable_irq(devp->irq);
			/* for t7 meta data */
			if (devp->dtdata->hw_ver == VDIN_HW_T7) {
				ret = request_irq(devp->vdin2_meta_wr_done_irq,
						  vdin_wrmif2_dvmeta_wr_done_isr,
						  IRQF_SHARED,
						  devp->vdin2_meta_wr_done_irq_name,
						  (void *)devp);
				disable_irq(devp->vdin2_meta_wr_done_irq);
				pr_info("vdin%d req meta_wr_done_irq",
					devp->index);
			}
			devp->flags |= VDIN_FLAG_ISR_REQ;
		}
		vdin_start_dec(devp);
		if (!(devp->flags & VDIN_FLAG_ISR_EN)) {
			/*enable irq */
			enable_irq(devp->irq);

			/* for t7 dv meta data */
			if (devp->vdin2_meta_wr_done_irq > 0) {
				enable_irq(devp->vdin2_meta_wr_done_irq);
				pr_info("enable meta wt done irq %d\n",
					devp->vdin2_meta_wr_done_irq);
			}
			devp->flags |= VDIN_FLAG_ISR_EN;
		}
		pr_info("%s vdin.%d enable_irq\n",
			__func__, devp->index);
		devp->flags |= VDIN_FLAG_DEC_STARTED;
		pr_info("START_DEC port %s, decode started ok\n\n",
			tvin_port_str(devp->parm.port));
	} else if (!strcmp(parm[0], "startdec")) {
		temp = devp->parm.info.fmt;
		pr_info("cur timing info:0x%x %s\n", temp,
			tvin_sig_fmt_str(devp->parm.info.fmt));
		devp->fmt_info_p =
			(struct tvin_format_s *)tvin_get_fmt_info(temp);

		vdin_start_dec(devp);
		/*enable irq */
		enable_irq(devp->irq);
		pr_info("%s START_DEC vdin.%d enable_irq %d\n",
			__func__, devp->index, devp->irq);
		devp->flags |= VDIN_FLAG_DEC_STARTED;
	} else if (!strcmp(parm[0], "tvstop")) {
		vdin_stop_dec(devp);
		vdin_close_fe(devp);
		/* devp->flags &= (~VDIN_FLAG_FS_OPENED); */
		devp->flags &= (~VDIN_FLAG_DEC_STARTED);
		/* free irq, free when close device */
		/* free_irq(devp->irq, (void *)devp); */

		if (vdin_dbg_en)
			pr_info("%s vdin.%d free_irq\n", __func__,
				devp->index);
		/* reset the hardware limit to vertical [0-1079]  */
		/* WRITE_VCBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000437); */
	} else if (!strcmp(parm[0], "v4l2stop")) {
		stop_tvin_service(devp->index);
		devp->flags &= (~VDIN_FLAG_V4L2_DEBUG);
	} else if (!strcmp(parm[0], "v4l2start")) {
		struct vdin_parm_s param;

		if (!parm[4]) {
			pr_err("usage: echo v4l2start port width height");
			pr_err("fps cfmt > /sys/class/vdin/vdinx/attr.\n");
			pr_err("port mybe bt656 or viuin,");
			pr_err("fps the frame rate of input.\n");
			kfree(buf_orig);
			return len;
		}
		memset(&param, 0, sizeof(struct vdin_parm_s));
		/*parse the port*/
		if (!strcmp(parm[1], "bt656")) {
			param.port = TVIN_PORT_CAMERA;
			pr_info(" port is TVIN_PORT_CAMERA\n");
		} else if (!strcmp(parm[1], "viuin")) {
			param.port = TVIN_PORT_VIU1;
			pr_info(" port is TVIN_PORT_VIU\n");
		} else if (!strcmp(parm[1], "video")) {
			param.port = TVIN_PORT_VIU1_VIDEO;
			pr_info(" port is TVIN_PORT_VIU_VIDEO\n");
		} else if (!strcmp(parm[1], "viu_wb0_vpp")) {
			param.port = TVIN_PORT_VIU1_WB0_VPP;
			pr_info(" port is TVIN_PORT_VIU1_WB0_VPP\n");
		} else if (!strcmp(parm[1], "viu_wb0_vd1")) {
			param.port = TVIN_PORT_VIU1_WB0_VD1;
			pr_info(" port is TVIN_PORT_VIU_WB0_VD1\n");
		} else if (!strcmp(parm[1], "viu_wb0_vd2")) {
			param.port = TVIN_PORT_VIU1_WB0_VD2;
			pr_info(" port is TVIN_PORT_VIU_WB0_VD2\n");
		} else if (!strcmp(parm[1], "viu_wb0_osd1")) {
			param.port = TVIN_PORT_VIU1_WB0_OSD1;
			pr_info(" port is TVIN_PORT_VIU_WB0_OSD1\n");
		} else if (!strcmp(parm[1], "viu_wb0_osd2")) {
			param.port = TVIN_PORT_VIU1_WB0_OSD2;
			pr_info(" port is TVIN_PORT_VIU_WB0_OSD2\n");
		} else if (!strcmp(parm[1], "viu_wb0_post_blend")) {
			param.port = TVIN_PORT_VIU1_WB0_POST_BLEND;
			pr_info(" port is TVIN_PORT_VIU_WB0_POST_BLEND\n");
		} else if (!strcmp(parm[1], "viu_wb1_vpp")) {
			param.port = TVIN_PORT_VIU1_WB1_VPP;
			pr_info(" port is TVIN_PORT_VIU1_WB1_VPP\n");
		} else if (!strcmp(parm[1], "viu_wb1_vd1")) {
			param.port = TVIN_PORT_VIU1_WB1_VD1;
			pr_info(" port is TVIN_PORT_VIU_WB1_VD1\n");
		} else if (!strcmp(parm[1], "viu_wb1_vd2")) {
			param.port = TVIN_PORT_VIU1_WB1_VD2;
			pr_info(" port is TVIN_PORT_VIU_WB1_VD2\n");
		} else if (!strcmp(parm[1], "viu_wb1_osd1")) {
			param.port = TVIN_PORT_VIU1_WB1_OSD1;
			pr_info(" port is TVIN_PORT_VIU_WB1_OSD1\n");
		} else if (!strcmp(parm[1], "viu_wb0_osd2")) {
			param.port = TVIN_PORT_VIU1_WB1_OSD2;
			pr_info(" port is TVIN_PORT_VIU_WB1_OSD2\n");
		} else if (!strcmp(parm[1], "viu_wb1_post_blend")) {
			param.port = TVIN_PORT_VIU1_WB1_POST_BLEND;
			pr_info(" port is TVIN_PORT_VIU_WB1_POST_BLEND\n");
		} else if (!strcmp(parm[1], "viu_wb1_vdinbist")) {
			param.port = TVIN_PORT_VIU1_WB0_VDIN_BIST;
			pr_info(" port is viu_wb1_vdinbist\n");
		} else if (!strcmp(parm[1], "viuin2")) {
			param.port = TVIN_PORT_VIU2;
			pr_info(" port is TVIN_PORT_VIU\n");
		} else if (!strcmp(parm[1], "viu2_encl")) {
			param.port = TVIN_PORT_VIU2_ENCL;
			pr_info(" port is TVIN_PORT_VIU2_ENCL\n");
		} else if (!strcmp(parm[1], "viu2_enci")) {
			param.port = TVIN_PORT_VIU2_ENCI;
			pr_info(" port is TVIN_PORT_VIU2_ENCI\n");
		} else if (!strcmp(parm[1], "viu2_encp")) {
			param.port = TVIN_PORT_VIU2_ENCP;
			pr_info(" port is TVIN_PORT_VIU2_ENCP\n");
		} else if (!strcmp(parm[1], "isp")) {
			param.port = TVIN_PORT_ISP;
			pr_info(" port is TVIN_PORT_ISP\n");
		}

		/*parse the resolution*/
		if (kstrtol(parm[2], 10, &val) == 0)
			param.h_active = val;
		if (kstrtol(parm[3], 10, &val) == 0)
			param.v_active = val;
		if (kstrtol(parm[4], 10, &val) == 0)
			param.frame_rate = val;
		pr_info(" hactive:%d,vactive:%d, rate:%d\n",
			param.h_active,
			param.v_active,
			param.frame_rate);
		if (!parm[5])
			param.cfmt = TVIN_YUV422;
		else if (kstrtol(parm[5], 10, &val) == 0)
			param.cfmt = val;
		pr_info(" cfmt:%d\n", param.cfmt);
		if (!parm[6])
			param.dfmt = TVIN_YUV422;
		else if (kstrtol(parm[6], 10, &val) == 0)
			param.dfmt = val;
		pr_info(" dfmt:%d\n", param.dfmt);
		if (!parm[7])
			param.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
		else if (kstrtol(parm[7], 10, &val) == 0)
			param.scan_mode = val;
		pr_info(" scan_mode:%d\n", param.scan_mode);

		param.fmt = TVIN_SIG_FMT_MAX;
		devp->flags |= VDIN_FLAG_V4L2_DEBUG;
		param.reserved |= PARAM_STATE_HISTGRAM;
		/* param.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE; */
		/*start the vdin hardware*/
		start_tvin_service(devp->index, &param);
	} else if (!strcmp(parm[0], "disablesm")) {
		del_timer_sync(&devp->timer);
	} else if (!strcmp(parm[0], "freeze")) {
		if (!(devp->flags & VDIN_FLAG_DEC_STARTED))
			pr_info("vdin not started,can't do freeze!!!\n");
		else if (devp->fmt_info_p->scan_mode ==
			TVIN_SCAN_MODE_PROGRESSIVE)
			vdin_vf_freeze(devp->vfp, 1);
		else
			vdin_vf_freeze(devp->vfp, 2);

	} else if (!strcmp(parm[0], "unfreeze")) {
		if (!(devp->flags & VDIN_FLAG_DEC_STARTED))
			pr_info("vdin not started,can't do unfreeze!!!\n");
		else
			vdin_vf_unfreeze(devp->vfp);
	} else if (!strcmp(parm[0], "conversion")) {
		if (parm[1] && parm[2] && parm[3]) {
			if (kstrtoul(parm[1], 10, &val) == 0)
				devp->debug.scaler4w = val;
			if (kstrtoul(parm[2], 10, &val) == 0)
				devp->debug.scaler4h = val;
			if (kstrtoul(parm[3], 10, &val) == 0)
				devp->debug.dest_cfmt = val;

			devp->flags |= VDIN_FLAG_MANUAL_CONVERSION;
			pr_info("enable manual conversion w = %u h = %u ",
				devp->debug.scaler4w, devp->debug.scaler4h);
			pr_info("dest_cfmt = %s.\n",
				tvin_color_fmt_str(devp->debug.dest_cfmt));
		} else {
			devp->flags &= (~VDIN_FLAG_MANUAL_CONVERSION);
			pr_info("disable manual conversion w = %u h = %u ",
				devp->debug.scaler4w, devp->debug.scaler4h);
			pr_info("dest_cfmt = %s\n",
				tvin_color_fmt_str(devp->debug.dest_cfmt));
		}
	} else if (!strcmp(parm[0], "state")) {
		vdin_dump_state(devp);
	} else if (!strcmp(parm[0], "counter")) {
		vdin_dump_count(devp);
	} else if (!strcmp(parm[0], "histgram")) {
		vdin_dump_histgram(devp);
#ifdef CONFIG_AML_LOCAL_DIMMING
	} else if (!strcmp(parm[0], "histgram_ldim")) {
		unsigned int hnum, vnum;

		if (parm[1] && parm[2]) {
			hnum = kstrtoul(parm[1], 10, (unsigned long *)&hnum);
			vnum = kstrtoul(parm[2], 10, (unsigned long *)&vnum);
		} else {
			hnum = 8;
			vnum = 2;
		}
		vdin_dump_histgram_ldim(devp, hnum, vnum);
#endif
	} else if (!strcmp(parm[0], "force_recycle")) {
		devp->flags |= VDIN_FLAG_FORCE_RECYCLE;
	} else if (!strcmp(parm[0], "read_pic_afbce")) {
		if (parm[1] && parm[2])
			vdin_write_afbce_mem(devp, parm[1], parm[2]);
		else
			pr_err("miss parameters.\n");
	} else if (!strcmp(parm[0], "read_pic")) {
		if (parm[1] && parm[2])
			vdin_write_mem(devp, parm[1], parm[2], parm[3]);
		else
			pr_err("miss parameters .\n");
	} else if (!strcmp(parm[0], "read_bin")) {
		if (parm[1] && parm[2] && parm[3])
			vdin_write_cont_mem(devp, parm[1], parm[2], parm[3]);
		else
			pr_err("miss parameters .\n");
	} else if (!strcmp(parm[0], "dump_reg")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0))
			vdin_dump_regs(devp, temp);
		else
			vdin_dump_regs(devp, 0);
	} else if (!strcmp(parm[0], "prob_xy")) {
		unsigned int x = 0, y = 0;

#ifdef CONFIG_AMLOGIC_PIXEL_PROBE
		vdin_probe_enable();
#endif
		if (parm[1] && parm[2]) {
			if (kstrtoul(parm[1], 10, &val) == 0)
				x = val;
			if (kstrtoul(parm[2], 10, &val) == 0)
				y = val;
			vdin_prob_set_xy(devp->addr_offset, x, y, devp);
		} else {
			pr_err("miss parameters .\n");
		}
	} else if (!strcmp(parm[0], "prob_rgb")) {
		unsigned int r, g, b;

		vdin_prob_get_rgb(devp->addr_offset, &r, &g, &b);
		pr_info("rgb_info-->r:%x,g:%x,b:%x\n", r, g, b);
	} else if (!strcmp(parm[0], "prob_yuv")) {
		unsigned int r, g, b;

		vdin_prob_get_yuv(devp->addr_offset, &r, &g, &b);
		pr_info("yuv_info-->u:%x,v:%x,y:%x\n", r, g, b);
	} else if (!strcmp(parm[0], "prob_pre_post")) {
		unsigned int x = 0;

		if (!parm[1])
			pr_err("miss parameters .\n");
		if (kstrtoul(parm[1], 10, &val) == 0)
			x = val;
		pr_info("matrix post sel: %d\n", x);
		vdin_prob_set_before_or_after_mat(devp->addr_offset, x, devp);
	} else if (!strcmp(parm[0], "prob_mat_sel")) {
		unsigned int x = 0;

		if (!parm[1])
			pr_err("miss parameters .\n");
		if (kstrtoul(parm[1], 10, &val) == 0)
			x = val;
		pr_info("matrix sel : %d\n", x);
		vdin_prob_matrix_sel(devp->addr_offset, x, devp);
	} else if (!strcmp(parm[0], "mpeg2vdin")) {
		if (parm[1] && parm[2]) {
			if (kstrtoul(parm[1], 10, &val) == 0)
				devp->h_active = val;
			if (kstrtoul(parm[2], 10, &val) == 0)
				devp->v_active = val;
			vdin_set_mpegin(devp);
			pr_info("mpeg2vdin:h_active:%d,v_active:%d\n",
				devp->h_active, devp->v_active);
		} else {
			pr_err("miss parameters .\n");
		}
	} else if (!strcmp(parm[0], "hdr")) {
		int i;
		struct vframe_master_display_colour_s *prop;

		prop = &devp->curr_wr_vfe->vf.prop.master_display_colour;
		pr_info("present_flag: %d\n", prop->present_flag);
		for (i = 0; i < 6; i++)
			pr_info("primaries %d: %#x\n", i,
				*(((u32 *)(prop->primaries)) + i));

		pr_info("white point x: %#x, y: %#x\n",
			*((u32 *)(prop->white_point)),
			*((u32 *)(prop->white_point) + 1));
		pr_info("lumi max: %#x, min: %#x\n",
			*((u32 *)(prop->luminance)),
			*(((u32 *)(prop->luminance)) + 1));
	} else if (!strcmp(parm[0], "snowon")) {
		unsigned int fmt;

		fmt = TVIN_SIG_FMT_CVBS_NTSC_M;
		devp->flags |= VDIN_FLAG_SNOW_FLAG;
		devp->flags |= VDIN_FLAG_SM_DISABLE;
		if (devp->flags & VDIN_FLAG_DEC_STARTED) {
			pr_info("TVIN_IOC_START_DEC() TVIN_PORT_CVBS3, started already\n");
		} else {
			devp->parm.info.fmt = fmt;
			devp->fmt_info_p  =
				(struct tvin_format_s *)tvin_get_fmt_info(fmt);
			vdin_start_dec(devp);
			devp->flags |= VDIN_FLAG_DEC_STARTED;
			pr_info("TVIN_IOC_START_DEC port TVIN_PORT_CVBS3, decode started ok\n\n");
		}
		devp->flags &= (~VDIN_FLAG_SM_DISABLE);
		/*tvafe_snow_config(1);*/
		/*tvafe_snow_config_clamp(1);*/
		pr_info("snowon config done!!\n");
	} else if (!strcmp(parm[0], "snowoff")) {
		devp->flags &= (~VDIN_FLAG_SNOW_FLAG);
		devp->flags |= VDIN_FLAG_SM_DISABLE;
		/*tvafe_snow_config(0);*/
		/*tvafe_snow_config_clamp(0);*/
		/* if fmt change, need restart dec vdin */
		if (devp->parm.info.fmt != TVIN_SIG_FMT_CVBS_NTSC_M &&
		    devp->parm.info.fmt != TVIN_SIG_FMT_NULL) {
			if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
				pr_err("VDIN_FLAG_DEC_STARTED(%d) decode havn't started\n",
				       devp->index);
			} else {
				devp->flags |= VDIN_FLAG_DEC_STOP_ISR;
				vdin_stop_dec(devp);
				devp->flags &= ~VDIN_FLAG_DEC_STOP_ISR;
				devp->flags &= (~VDIN_FLAG_DEC_STARTED);
				pr_info("VDIN_FLAG_DEC_STARTED(%d) port %s, decode stop ok\n",
					devp->parm.index,
					tvin_port_str(devp->parm.port));
			}
			if (devp->flags & VDIN_FLAG_DEC_STARTED) {
				pr_info("VDIN_FLAG_DEC_STARTED() TVIN_PORT_CVBS3, started already\n");
			} else {
				devp->fmt_info_p  =
				(struct tvin_format_s *)
				tvin_get_fmt_info(devp->parm.info.fmt);
				vdin_start_dec(devp);
				devp->flags |= VDIN_FLAG_DEC_STARTED;
				pr_info("VDIN_FLAG_DEC_STARTED port TVIN_PORT_CVBS3, decode started ok\n");
			}
		}
		devp->flags &= (~VDIN_FLAG_SM_DISABLE);
		pr_info("snowoff config done!!\n");
	} else if (!strcmp(parm[0], "vf_reg")) {
		if ((devp->flags & VDIN_FLAG_DEC_REGED)
			== VDIN_FLAG_DEC_REGED) {
			pr_err("vf_reg(%d) decoder is registered already\n",
			       devp->index);
		} else {
			devp->flags |= VDIN_FLAG_DEC_REGED;
			vdin_vf_reg(devp);
			pr_info("vf_reg(%d) ok\n\n", devp->index);
		}
	} else if (!strcmp(parm[0], "vf_unreg")) {
		if ((devp->flags & VDIN_FLAG_DEC_REGED)
			!= VDIN_FLAG_DEC_REGED) {
			pr_err("vf_unreg(%d) decoder isn't registered\n",
			       devp->index);
		} else {
			devp->flags &= (~VDIN_FLAG_DEC_REGED);
			vdin_vf_unreg(devp);
			pr_info("vf_unreg(%d) ok\n\n", devp->index);
		}
	} else if (!strcmp(parm[0], "pause_dec")) {
		vdin_pause_dec(devp);
		pr_info("pause_dec(%d) ok\n\n", devp->index);
	} else if (!strcmp(parm[0], "resume_dec")) {
		vdin_resume_dec(devp);
		pr_info("resume_dec(%d) ok\n\n", devp->index);
	} else if (!strcmp(parm[0], "color_depth")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			if (val == 0)
				devp->color_depth_config = COLOR_DEEPS_AUTO;
			else
				devp->color_depth_config =
					val | COLOR_DEEPS_MANUAL;
			pr_info("color_depth(%d):0x%x\n\n", devp->index,
				devp->color_depth_config);
		}
	} else if (!strcmp(parm[0], "color_depth_support")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 16, &val) == 0) {
			devp->color_depth_support = val;
			pr_info("color_depth_support(%d):%d\n\n", devp->index,
				devp->color_depth_support);
		}
	} else if (!strcmp(parm[0], "force_stop_frame_num")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			devp->dbg_force_stop_frame_num = val;
			pr_info("force_stop_frame_num(%d):%d\n\n", devp->index,
				devp->dbg_force_stop_frame_num);
		}
	} else if (!strcmp(parm[0], "force_disp_skip_num")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			devp->force_disp_skip_num = val;
			pr_info("force_disp_skip_num(%d):%d\n\n", devp->index,
				devp->force_disp_skip_num);
		}
	} else if (!strcmp(parm[0], "full_pack")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->full_pack = val;
			pr_info("full_pack(%d):%d\n\n", devp->index,
				devp->full_pack);
		}
	} else if (!strcmp(parm[0], "flags_isr")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 16, &val) == 0) {
			devp->flags_isr = val;
			pr_info("vdin%d,flags_isr:%#x\n\n", devp->index,
				devp->flags_isr);
		}
	} else if (!strcmp(parm[0], "force_malloc_yuv_422_to_444")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			if (val)
				devp->force_malloc_yuv_422_to_444 = 1;
			else
				devp->force_malloc_yuv_422_to_444 = 0;
			pr_info("force_malloc_yuv_422_to_444(%d):%d\n\n", devp->index,
				devp->force_malloc_yuv_422_to_444);
		}
	} else if (!strcmp(parm[0], "auto_cutwindow_en")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->auto_cutwindow_en = val;
			pr_info("auto_cutwindow_en(%d):%d\n\n", devp->index,
				devp->auto_cutwindow_en);
		}
	} else if (!strcmp(parm[0], "dolby_config")) {
		vdin_dolby_config(devp);
		pr_info("dolby_config done\n");
	} else if (!strcmp(parm[0], "metadata")) {
		dump_dolby_metadata(devp);
		pr_info("dolby_config done\n");
	} else if (!strcmp(parm[0], "clean_dv")) {
		dump_dolby_buf_clean(devp);
		pr_info("clean dolby vision mem done\n");
	} else if (!strcmp(parm[0], "dv_debug")) {
		vdin_dv_debug(devp);
	} else if (!strcmp(parm[0], "channel_order_config")) {
		unsigned int c0, c1, c2;

		if (!parm[3]) {
			pr_info("miss parameters\n");
		} else {
			c0 = 0;
			c1 = 0;
			c2 = 0;
			if (kstrtoul(parm[1], 10, &val) == 0)
				c0 = val;
			if (kstrtoul(parm[2], 10, &val) == 0)
				c1 = val;
			if (kstrtoul(parm[3], 10, &val) == 0)
				c2 = val;
			vdin_channel_order_config(devp->addr_offset,
						  c0, c1, c2);
		}
	} else if (!strcmp(parm[0], "channel_order_status")) {
		vdin_channel_order_status(devp->addr_offset);
	} else if (!strcmp(parm[0], "open_port")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 16, &val) == 0) {
			devp->parm.index = 0;
			devp->parm.port  = val;
			devp->unstable_flag = false;
			ret = vdin_open_fe(devp->parm.port,
					   devp->parm.index, devp);
			if (ret) {
				pr_err("TVIN_IOC_OPEN(%d) failed to open port 0x%x\n",
				       devp->parm.index, devp->parm.port);
			} else {
				devp->flags |= VDIN_FLAG_DEC_OPENED;
				pr_info("TVIN_IOC_OPEN(%d) port %s opened ok\n\n",
					devp->parm.index,
				tvin_port_str(devp->parm.port));
			}
		}
	} else if (!strcmp(parm[0], "close_port")) {
		enum tvin_port_e port = devp->parm.port;

		if (!(devp->flags & VDIN_FLAG_DEC_OPENED)) {
			pr_err("TVIN_IOC_CLOSE(%d) you have not opened port\n",
			       devp->index);
		} else {
			vdin_close_fe(devp);
			devp->flags &= (~VDIN_FLAG_DEC_OPENED);
			pr_info("TVIN_IOC_CLOSE(%d) port %s closed ok\n\n",
				devp->parm.index, tvin_port_str(port));
		}
	} else if (!strcmp(parm[0], "prehsc_en")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->prehsc_en = val;
			pr_info("prehsc_en(%d):%d\n\n", devp->index,
				devp->prehsc_en);
		}
	} else if (!strcmp(parm[0], "vshrk_en")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->vshrk_en = val;
			pr_info("vshrk_en(%d):%d\n\n", devp->index,
				devp->vshrk_en);
		}
	} else if (!strcmp(parm[0], "cma_mem_mode")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->cma_mem_mode = val;
			pr_info("cma_mem_mode(%d):%d\n\n", devp->index,
				devp->cma_mem_mode);
		}
	} else if (!strcmp(parm[0], "black_bar_enable")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->black_bar_enable = val;
			pr_info("black_bar_enable(%d):%d\n\n", devp->index,
				devp->black_bar_enable);
		}
	} else if (!strcmp(parm[0], "hist_bar_enable")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->hist_bar_enable = val;
			pr_info("hist_bar_enable(%d):%d\n\n", devp->index,
				devp->hist_bar_enable);
		}
	} else if (!strcmp(parm[0], "use_frame_rate")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->use_frame_rate = val;
			pr_info("use_frame_rate(%d):%d\n\n", devp->index,
				devp->use_frame_rate);
		}
	} else if (!strcmp(parm[0], "dolby_input")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->dv.dolby_input = val;
			pr_info("dolby_input(%d):%d\n\n", devp->index,
				devp->dv.dolby_input);
		}
	} else if (!strcmp(parm[0], "rdma_enable")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->rdma_enable = val;
			pr_info("rdma_enable (%d):%d\n", devp->index,
				devp->rdma_enable);
		}
	} else if (!strcmp(parm[0], "urgent_en")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->urgent_en = val;
			pr_info("urgent_en (%d):%d\n", devp->index,
				devp->urgent_en);
		}
	} else if (!strcmp(parm[0], "irq_cnt")) {
		pr_info("vdin(%d) irq_cnt: %d\n", devp->index,	devp->irq_cnt);
	} else if (!strcmp(parm[0], "skip_vf_num")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if ((kstrtoul(parm[1], 10, &val) == 0) && (devp->vfp)) {
			devp->vfp->skip_vf_num = val;
			if (val == 0)
				memset(devp->vfp->disp_mode, 0,
				       (sizeof(enum vframe_disp_mode_e) *
					VFRAME_DISP_MAX_NUM));
			pr_info("vframe_skip(%d):%d\n\n", devp->index,
				devp->vfp->skip_vf_num);
		}
	} else if (!strcmp(parm[0], "dump_afbce")) {
		if (parm[2]) {
			unsigned int buf_num = 0;

			if (kstrtol(parm[2], 10, &val) == 0)
				buf_num = val;
			vdin_dump_one_afbce_mem(parm[1], devp, buf_num);
		} else if (parm[1]) {
			vdin_dump_one_afbce_mem(parm[1], devp, 0);
		}
	} else if (!strcmp(parm[0], "dbg_dump_frames")) {
		if (parm[1]) {
			if (kstrtol(parm[1], 10, &val) == 0)
				devp->dbg_dump_frames = val;
		}
		pr_info("vdin%d,dbg_dump_frames = %d\n",
			devp->index, devp->dbg_dump_frames);
	} else if (!strcmp(parm[0], "no_swap_en")) {
		if (parm[1]) {
			if (kstrtol(parm[1], 0, &val) == 0)
				devp->dbg_no_swap_en = val;
		}
		pr_info("vdin%d,dbg_no_swap_en = %d\n",
			devp->index, devp->dbg_no_swap_en);
	} else if (!strcmp(parm[0], "dbg_stop_dec_delay")) {
		if (parm[1]) {
			if (kstrtol(parm[1], 10, &val) == 0)
				devp->dbg_stop_dec_delay = val;
		}
		pr_info("vdin%d,dbg_stop_dec_delay = %u us\n",
			devp->index, devp->dbg_stop_dec_delay);
	} else if (!strcmp(parm[0], "skip_frame_debug")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &skip_frame_debug) == 0)
				pr_info("set skip_frame_debug: %d\n",
					skip_frame_debug);
		} else {
			pr_info("skip_frame_debug: %d\n", skip_frame_debug);
		}
	} else if (!strcmp(parm[0], "max_ignore_cnt")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &max_ignore_frame_cnt) == 0)
				pr_info("set max_ignore_frame_cnt: %d\n",
					max_ignore_frame_cnt);
		} else {
			pr_info("max_ignore_frame_cnt: %d\n",
				max_ignore_frame_cnt);
		}
	} else if (!strcmp(parm[0], "afbce_flag")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16, &devp->afbce_flag) == 0) {
				pr_info("set vdin_afbce_flag: 0x%x\n",
					devp->afbce_flag);
			}
		} else {
			pr_info("vdin_afbce_flag: 0x%x\n", devp->afbce_flag);
		}
	} else if (!strcmp(parm[0], "afbce_mode")) {
		if (parm[2]) {
			if ((kstrtouint(parm[1], 10, &mode) == 0) &&
			    (kstrtouint(parm[2], 10, &flag) == 0)) {
				vdin0_afbce_debug_force = flag;
				if (devp->afbce_flag & VDIN_AFBCE_EN)
					devp->afbce_mode = mode;
				else
					devp->afbce_mode = 0;
				if (vdin0_afbce_debug_force) {
					pr_info("set force vdin_afbce_mode: %d\n",
						devp->afbce_mode);
				} else {
					pr_info("set vdin_afbce_mode: %d\n",
						devp->afbce_mode);
				}
			}
		} else if (parm[1]) {
			if (kstrtouint(parm[1], 10, &mode) == 0) {
				if (devp->afbce_flag & VDIN_AFBCE_EN)
					devp->afbce_mode = mode;
				else
					devp->afbce_mode = 0;
				pr_info("set vdin_afbce_mode: %d\n",
					devp->afbce_mode);
			}
		} else {
			pr_info("vdin_afbce_mode: %d\n", devp->afbce_mode);
		}
	} else if (!strcmp(parm[0], "vdi6_afifo_overflow")) {
		pr_info("%d\n",
			vdin_check_vdi6_afifo_overflow(devp->addr_offset));
	} else if (!strcmp(parm[0], "vdi6_afifo_clear")) {
		vdin_clear_vdi6_afifo_overflow_flg(devp->addr_offset);
	} else if (!strcmp(parm[0], "skip_frame_check")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &devp->skip_disp_md_check)
				== 0)
				pr_info("skip frame check: %d\n",
					devp->skip_disp_md_check);
		} else {
			pr_info("skip frame check para err, ori: %d\n",
				devp->skip_disp_md_check);
		}
	} else if (!strcmp(parm[0], "vdinmtx")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &temp) == 0  &&
			    kstrtouint(parm[2], 10, &mode) == 0) {
				if (temp == VDIN_SEL_MATRIX0)
					vdin_change_matrix0(offset, mode);
				else if (temp == VDIN_SEL_MATRIX1)
					vdin_change_matrix1(offset, mode);
				else if (temp == VDIN_SEL_MATRIXHDR)
					vdin_change_matrixhdr(offset, mode);
			}
		}
	} else if (!strcmp(parm[0], "wr_frame_en")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &devp->vframe_wr_en) == 0)
				pr_info("vdin.%d vframe_wr_en %d\n",
					devp->index, devp->vframe_wr_en);
			else
				pr_err("parse para err\n");
		} else {
			pr_err("miss para, current vframe_wr_en:%d\n",
			       devp->vframe_wr_en);
		}
	} else if (!strcmp(parm[0], "dv_crc")) {
		/*
		 * 0:force false 1:force true 2:auto
		 */
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0)) {
			if (temp == 0) {
				dv_dbg_mask |= DV_CRC_FORCE_FALSE;
				dv_dbg_mask &= ~DV_CRC_FORCE_TRUE;
			} else if (temp == 1) {
				dv_dbg_mask &= ~DV_CRC_FORCE_FALSE;
				dv_dbg_mask |= DV_CRC_FORCE_TRUE;
			} else {
				dv_dbg_mask &= ~DV_CRC_FORCE_FALSE;
				dv_dbg_mask &= ~DV_CRC_FORCE_TRUE;
			}
			pr_info("dv_dbg_mask=0x%x\n", dv_dbg_mask);
		}
	} else if (!strcmp(parm[0], "gethist")) {
		pr_info("sum:0x%lx, width:%d, height:%d ave:0x%x\n",
			vdin1_hist.sum,
			vdin1_hist.width, vdin1_hist.height,
			vdin1_hist.ave);
	} else if (!strcmp(parm[0], "vfcrc")) {
		pr_info("vfcrc:0x%x\n", rd(devp->addr_offset, VDIN_RO_CRC));
	} else if (!strcmp(parm[0], "game_mode")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			devp->game_mode = temp;
			vdin_force_game_mode = temp;
			pr_info("set game mode: 0x%x\n", temp);
		}
	} else if (!strcmp(parm[0], "vrr_mode")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			devp->vrr_mode = temp;
			pr_info("set vrr_mode: 0x%x\n", temp);
		}
	} else if (!strcmp(parm[0], "matrix_pattern")) {
		/*
		 * 0:off 1:enable
		 */
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0)) {
			devp->matrix_pattern_mode = temp;
			vdin_set_matrix_color(devp);
			/* pr_info("matrix_pattern_mode:%d\n", devp->matrix_pattern_mode); */
		}
	} else if  (!strcmp(parm[0], "bist_set")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0))
			val = temp;

		if (!(parm[2] && (kstrtouint(parm[2], 10, &temp) == 0)))
			temp = 0;

		vdin_set_bist_pattern(devp, val, temp);
	} else if (!strcmp(parm[0], "pause_num")) {
		/*
		 * 0:off 1:enable
		 */
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0))
			devp->pause_num = temp;
	} else if (!strcmp(parm[0], "hv_reverse_en")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0))
			devp->hv_reverse_en = temp;
	} else if (!strcmp(parm[0], "doublewrite")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0))
			devp->double_wr_cfg = temp;
	} else if (!strcmp(parm[0], "secure_mem")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			if (temp) {
				devp->secure_en = 1;
				devp->cma_config_flag = 0x1;
			} else {
				devp->secure_en = 0;
				devp->cma_config_flag = 0x101;
			}
		}
		pr_info("secure:%d, cma flag:%d\n", devp->secure_en,
			devp->cma_config_flag);
	} else if (!strcmp(parm[0], "wv")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			if (parm[1] && (kstrtouint(parm[2], 16, &mode) == 0))
				W_VCBUS(temp, mode);
		}
	} else if (!strcmp(parm[0], "rv")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0))
			pr_info("addr:0x%x val:0x%x\n", temp, R_VCBUS(temp));
	} else if (!strcmp(parm[0], "game_mode_chg")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			pr_info("set new game mode to: 0x%x,pre:%#x\n", temp, game_mode);
			if (game_mode != temp)
				vdin_game_mode_chg(devp, game_mode, temp);
			game_mode = temp;
		}
	} else {
		pr_info("unknown command\n");
	}

	kfree(buf_orig);
	return len;
}

static DEVICE_ATTR_WO(attr);

#ifdef VF_LOG_EN
static ssize_t vf_log_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int len = 0;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	struct vf_log_s *log = &devp->vfp->log;

	len += sprintf(buf + len, "%d of %d\n", log->log_cur, VF_LOG_LEN);
	return len;
}

static ssize_t vf_log_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct vdin_dev_s *devp = dev_get_drvdata(dev);

	if (!strncmp(buf, "start", 5)) {
		vf_log_init(devp->vfp);
	} else if (!strncmp(buf, "print", 5)) {
		vf_log_print(devp->vfp);
	} else {
		pr_info("unknown command :  %s\n"
			"Usage:\n"
			"a. show log message:\n"
			"echo print > / sys/class/vdin/vdin0/vf_log\n"
			"b. restart log message:\n"
			"echo start > / sys/class/vdin/vdin0/vf_log\n"
			"c. show log records\n"
			"cat > / sys/class/vdin/vdin0/vf_log\n", buf);
	}
	return count;
}

/*
 * 1. show log length.
 * cat /sys/class/vdin/vdin0/vf_log
 * cat /sys/class/vdin/vdin1/vf_log
 * 2. clear log buffer and start log.
 * echo start > /sys/class/vdin/vdin0/vf_log
 * echo start > /sys/class/vdin/vdin1/vf_log
 * 3. print log
 * echo print > /sys/class/vdin/vdin0/vf_log
 * echo print > /sys/class/vdin/vdin1/vf_log
 */
static DEVICE_ATTR_RW(vf_log);

#endif /* VF_LOG_EN */

#ifdef ISR_LOG_EN
static ssize_t isr_log_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	u32 len = 0;
	struct vdin_dev_s *vdevp;

	vdevp = dev_get_drvdata(dev);
		len += sprintf(buf + len, "%d of %d\n",
			       vdevp->vfp->isr_log.log_cur, ISR_LOG_LEN);
	return len;
}

/*
 * 1. show isr log length.
 * cat /sys/class/vdin/vdin0/vf_log
 * 2. clear isr log buffer and start log.
 * echo start > /sys/class/vdin/vdinx/isr_log
 * 3. print isr log
 * echo print > /sys/class/vdin/vdinx/isr_log
 */
static ssize_t isr_log_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct vdin_dev_s *vdevp;

	vdevp = dev_get_drvdata(dev);
	if (!strncmp(buf, "start", 5))
		isr_log_init(vdevp->vfp);
	else if (!strncmp(buf, "print", 5))
		isr_log_print(vdevp->vfp);
	return count;
}

static DEVICE_ATTR_RW(isr_log);
#endif

static ssize_t crop_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	int len = 0;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	struct tvin_cutwin_s *crop = &devp->debug.cutwin;

	len += sprintf(buf + len,
		       "hs_offset %u, he_offset %u, vs_offset %u, ve_offset %u\n",
		       crop->hs, crop->he, crop->vs, crop->ve);
	return len;
}

static ssize_t crop_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	char *parm[4] = {NULL}, *buf_orig;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	struct tvin_cutwin_s *crop = &devp->debug.cutwin;
	long val;
	ssize_t ret_ext = count;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	vdin_parse_param(buf_orig, parm);

	if (!parm[3]) {
		ret_ext = -EINVAL;
		pr_info("miss param!!\n");
	} else {
		if (kstrtol(parm[0], 10, &val) == 0)
			crop->hs = val;
		if (kstrtol(parm[1], 10, &val) == 0)
			crop->he = val;
		if (kstrtol(parm[2], 10, &val) == 0)
			crop->vs = val;
		if (kstrtol(parm[3], 10, &val) == 0)
			crop->ve = val;
	}

	kfree(buf_orig);

	pr_info("hs_offset %u, he_offset %u, vs_offset %u, ve_offset %u.\n",
		crop->hs, crop->he, crop->vs, crop->ve);
	return ret_ext;
}

static DEVICE_ATTR_RW(crop);

static ssize_t cm2_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int len = 0;
	struct vdin_dev_s *devp;
	unsigned int addr_port = VDIN_CHROMA_ADDR_PORT;
	unsigned int data_port = VDIN_CHROMA_DATA_PORT;

	devp = dev_get_drvdata(dev);
	if (devp->addr_offset != 0) {
		addr_port = VDIN_CHROMA_ADDR_PORT + devp->addr_offset;
		data_port = VDIN_CHROMA_DATA_PORT + devp->addr_offset;
	}

	len += sprintf(buf + len, "addr_port[0x%x] data_port[0x%x]\n",
		       addr_port, data_port);
	len += sprintf(buf + len, "Usage:");
	len += sprintf(buf + len,
		       "echo wm addr data0 data1 data2 data3 data4 >");
	len += sprintf(buf + len, "/sys/class/vdin/vdin0/cm2\n");
	len += sprintf(buf + len,
		       "echo rm addr > / sys/class/vdin/vdin0/cm2\n");
	len += sprintf(buf + len,
		       "echo wm addr data0 data1 data2 data3 data4 >");
	len += sprintf(buf + len, "/sys/class/vdin/vdin1/cm2\n");
	len += sprintf(buf + len,
		       "echo rm addr > / sys/class/vdin/vdin1/cm2\n");
	return len;
}

static ssize_t cm2_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buffer, size_t count)
{
	struct vdin_dev_s *devp;
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[7];
	u32 addr = 0;
	int data[5] = {0};
	unsigned int addr_port = VDIN_CHROMA_ADDR_PORT;
	unsigned int data_port = VDIN_CHROMA_DATA_PORT;
	long val;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	strcat(delim1, delim2);

	devp = dev_get_drvdata(dev);
	if (devp->addr_offset != 0) {
		addr_port = VDIN_CHROMA_ADDR_PORT + devp->addr_offset;
		data_port = VDIN_CHROMA_DATA_PORT + devp->addr_offset;
	}
	buf_orig = kstrdup(buffer, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (n == 0) {
		pr_info("parm not initialized.\n");
		kfree(buf_orig);
		return count;
	}
	if ((parm[0][0] == 'w') && parm[0][1] == 'm') {
		if (n != 7) {
			pr_info("read : invalid parameter\n");
			pr_info("please : cat / sys/class/vdin/vdin0/cm2\n");
			kfree(buf_orig);
			return count;
		}
		/* addr = simple_strtol(parm[1], NULL, 16); */
		if (kstrtol(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		addr = val;
		addr = addr - addr % 8;
		if (kstrtol(parm[2], 16, &val) == 0)
			data[0] = val;
		if (kstrtol(parm[3], 16, &val) == 0)
			data[1] = val;
		if (kstrtol(parm[4], 16, &val) == 0)
			data[2] = val;
		if (kstrtol(parm[5], 16, &val) == 0)
			data[3] = val;
		if (kstrtol(parm[6], 16, &val) == 0)
			data[4] = val;
		aml_write_vcbus(addr_port, addr);
		aml_write_vcbus(data_port, data[0]);
		aml_write_vcbus(addr_port, addr + 1);
		aml_write_vcbus(data_port, data[1]);
		aml_write_vcbus(addr_port, addr + 2);
		aml_write_vcbus(data_port, data[2]);
		aml_write_vcbus(addr_port, addr + 3);
		aml_write_vcbus(data_port, data[3]);
		aml_write_vcbus(addr_port, addr + 4);
		aml_write_vcbus(data_port, data[4]);

		pr_info("wm[0x%x]  0x0\n", addr);
	} else if ((parm[0][0] == 'r') && parm[0][1] == 'm') {
		if (n != 2) {
			pr_info("read : invalid parameter\n");
			pr_info("please : cat / sys/class/vdin/vdin0/cm2\n");
			kfree(buf_orig);
			return count;
		}
		if (kstrtol(parm[1], 16, &val) == 0)
			addr = val;
		addr = addr - addr % 8;
		aml_write_vcbus(addr_port, addr);
		data[0] = aml_read_vcbus(data_port);
		data[0] = aml_read_vcbus(data_port);
		data[0] = aml_read_vcbus(data_port);
		aml_write_vcbus(addr_port, addr + 1);
		data[1] = aml_read_vcbus(data_port);
		data[1] = aml_read_vcbus(data_port);
		data[1] = aml_read_vcbus(data_port);
		aml_write_vcbus(addr_port, addr + 2);
		data[2] = aml_read_vcbus(data_port);
		data[2] = aml_read_vcbus(data_port);
		data[2] = aml_read_vcbus(data_port);
		aml_write_vcbus(addr_port, addr + 3);
		data[3] = aml_read_vcbus(data_port);
		data[3] = aml_read_vcbus(data_port);
		data[3] = aml_read_vcbus(data_port);
		aml_write_vcbus(addr_port, addr + 4);
		data[4] = aml_read_vcbus(data_port);
		data[4] = aml_read_vcbus(data_port);
		data[4] = aml_read_vcbus(data_port);

		pr_info("rm:[0x%x] : data[0x%x][0x%x][0x%x][0x%x][0x%x]\n",
			addr, data[0], data[1], data[2], data[3], data[4]);
	} else if (!strcmp(parm[0], "enable")) {
		wr_bits(devp->addr_offset, VDIN_CM_BRI_CON_CTRL, 1,
			CM_TOP_EN_BIT, CM_TOP_EN_WID);
	} else if (!strcmp(parm[0], "disable")) {
		wr_bits(devp->addr_offset, VDIN_CM_BRI_CON_CTRL, 0,
			CM_TOP_EN_BIT, CM_TOP_EN_WID);
	} else {
		pr_info("invalid command\n");
		pr_info("please : cat / sys/class/vdin/vdin0/bit");
	}

	kfree(buf_orig);
	return count;
}

static DEVICE_ATTR_RW(cm2);

static ssize_t snow_flag_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	int snow_flag = 0;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);

	snow_flag = (devp->flags & VDIN_FLAG_SNOW_FLAG) >> 14;
	return snprintf(buf, sizeof(unsigned int), "%d\n", snow_flag);
}

static DEVICE_ATTR_RO(snow_flag);

int vdin_create_debug_files(struct device *dev)
{
	int ret = 0;

	ret = device_create_file(dev, &dev_attr_sig_det);
	/* create sysfs attribute files */
	#ifdef VF_LOG_EN
	ret = device_create_file(dev, &dev_attr_vf_log);
	#endif
	#ifdef ISR_LOG_EN
	ret = device_create_file(dev, &dev_attr_isr_log);
	#endif
	ret = device_create_file(dev, &dev_attr_attr);
	ret = device_create_file(dev, &dev_attr_cm2);
	/*ret = device_create_file(dev, &dev_attr_debug_for_isp);*/
	ret = device_create_file(dev, &dev_attr_crop);
	ret = device_create_file(dev, &dev_attr_snow_flag);
	return ret;
}

void vdin_remove_debug_files(struct device *dev)
{
	#ifdef VF_LOG_EN
	device_remove_file(dev, &dev_attr_vf_log);
	#endif
	#ifdef ISR_LOG_EN
	device_remove_file(dev, &dev_attr_isr_log);
	#endif
	device_remove_file(dev, &dev_attr_attr);
	device_remove_file(dev, &dev_attr_cm2);
	/*device_remove_file(dev, &dev_attr_debug_for_isp);*/
	device_remove_file(dev, &dev_attr_crop);
	device_remove_file(dev, &dev_attr_sig_det);
	device_remove_file(dev, &dev_attr_snow_flag);
}

#ifdef DEBUG_SUPPORT
static int memp = MEMP_DCDR_WITHOUT_3D;
static char *memp_str(int profile)
{
	switch (profile) {
	case MEMP_VDIN_WITHOUT_3D:
		return "vdin without 3d";
	case MEMP_VDIN_WITH_3D:
		return "vdin with 3d";
	case MEMP_DCDR_WITHOUT_3D:
		return "decoder without 3d";
	case MEMP_DCDR_WITH_3D:
		return "decoder with 3d";
	case MEMP_ATV_WITHOUT_3D:
		return "atv without 3d";
	case MEMP_ATV_WITH_3D:
		return "atv with 3d";
	default:
		return "unknown";
	}
}

/*
 * cat /sys/class/vdin/memp
 */
static ssize_t memp_show(struct class *class,
			 struct class_attribute *attr, char *buf)
{
	int len = 0;

	len += sprintf(buf + len, "%d %s\n", memp, memp_str(memp));
	return len;
}

/*
 * echo 0|1|2|3|4|5 > /sys/class/vdin/memp
 */
static void memp_set(int type)
{
	switch (type) {
	case MEMP_VDIN_WITHOUT_3D:
	case MEMP_VDIN_WITH_3D:
		aml_write_vcbus(VPU_VDIN_ASYNC_HOLD_CTRL, 0x80408040);
		aml_write_vcbus(VPU_VDISP_ASYNC_HOLD_CTRL, 0x80408040);
		aml_write_vcbus(VPU_VPUARB2_ASYNC_HOLD_CTRL, 0x80408040);
		aml_write_vcbus(VPU_VD1_MMC_CTRL,
				aml_read_vcbus(VPU_VD1_MMC_CTRL) | (1 << 12));
		 /* arb0 */
		aml_write_vcbus(VPU_VD2_MMC_CTRL,
				aml_read_vcbus(VPU_VD2_MMC_CTRL) |
				(1 << 12));
		  /* arb0 */
		aml_write_vcbus(VPU_DI_IF1_MMC_CTRL,
				aml_read_vcbus(VPU_DI_IF1_MMC_CTRL) |
				(1 << 12));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_MEM_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MEM_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_INP_MMC_CTRL,
				aml_read_vcbus(VPU_DI_INP_MMC_CTRL) &
				(~(1 << 12)));
		 /* arb0 */
		aml_write_vcbus(VPU_DI_MTNRD_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MTNRD_MMC_CTRL) |
				(1 << 12));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_CHAN2_MMC_CTRL,
				aml_read_vcbus(VPU_DI_CHAN2_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_MTNWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MTNWR_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_NRWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_NRWR_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_DIWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_DIWR_MMC_CTRL) &
				(~(1 << 12)));
		memp = type;
		break;
	case MEMP_DCDR_WITHOUT_3D:
	case MEMP_DCDR_WITH_3D:
		aml_write_vcbus(VPU_VDIN_ASYNC_HOLD_CTRL, 0x80408040);
		 /* arb0 */
		aml_write_vcbus(VPU_VD1_MMC_CTRL,
				aml_read_vcbus(VPU_VD1_MMC_CTRL) | (1 << 12));
		/* arb0 */
		aml_write_vcbus(VPU_VD2_MMC_CTRL,
				aml_read_vcbus(VPU_VD2_MMC_CTRL) | (1 << 12));
		/* arb0 */
		aml_write_vcbus(VPU_DI_IF1_MMC_CTRL,
				aml_read_vcbus(VPU_DI_IF1_MMC_CTRL) |
				(1 << 12));
		  /* arb1 */
		aml_write_vcbus(VPU_DI_MEM_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MEM_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_INP_MMC_CTRL,
				aml_read_vcbus(VPU_DI_INP_MMC_CTRL) &
				(~(1 << 12)));
		/* arb0 */
		aml_write_vcbus(VPU_DI_MTNRD_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MTNRD_MMC_CTRL) |
				(1 << 12));
		/* arb1 */
		aml_write_vcbus(VPU_DI_CHAN2_MMC_CTRL,
				aml_read_vcbus(VPU_DI_CHAN2_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_MTNWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MTNWR_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_NRWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_NRWR_MMC_CTRL) &
				(~(1 << 12)));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_DIWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_DIWR_MMC_CTRL) &
				(~(1 << 12)));
		memp = type;
		break;
	case MEMP_ATV_WITHOUT_3D:
	case MEMP_ATV_WITH_3D:
		/* arb0 */
		aml_write_vcbus(VPU_VD1_MMC_CTRL,
				aml_read_vcbus(VPU_VD1_MMC_CTRL) | (1 << 12));
		/* arb0 */
		aml_write_vcbus(VPU_VD2_MMC_CTRL,
				aml_read_vcbus(VPU_VD2_MMC_CTRL) | (1 << 12));
		 /* arb0 */
		aml_write_vcbus(VPU_DI_IF1_MMC_CTRL,
				aml_read_vcbus(VPU_DI_IF1_MMC_CTRL) |
				(1 << 12));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_MEM_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MEM_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_INP_MMC_CTRL,
				aml_read_vcbus(VPU_DI_INP_MMC_CTRL) &
				(~(1 << 12)));
		/* arb0 */
		aml_write_vcbus(VPU_DI_MTNRD_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MTNRD_MMC_CTRL) |
				(1 << 12));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_CHAN2_MMC_CTRL,
				aml_read_vcbus(VPU_DI_CHAN2_MMC_CTRL) &
				(~(1 << 12)));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_MTNWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MTNWR_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_NRWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_NRWR_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_DIWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_DIWR_MMC_CTRL) &
				(~(1 << 12)));
		/* arb2 */
		aml_write_vcbus(VPU_TVD3D_MMC_CTRL,
				aml_read_vcbus(VPU_TVD3D_MMC_CTRL) |
				(1 << 14));
		/* urgent */
		aml_write_vcbus(VPU_TVD3D_MMC_CTRL,
				aml_read_vcbus(VPU_TVD3D_MMC_CTRL) |
				(1 << 15));
		/* arb2 */
		aml_write_vcbus(VPU_TVDVBI_MMC_CTRL,
				aml_read_vcbus(VPU_TVDVBI_MMC_CTRL) |
				(1 << 14));
		 /* urgent */
		aml_write_vcbus(VPU_TVD3D_MMC_CTRL,
				aml_read_vcbus(VPU_TVD3D_MMC_CTRL) |
				(1 << 15));
		memp = type;
		break;
	default:
		/* @todo */
		break;
	}
}

static ssize_t memp_store(struct class *class,
			  struct class_attribute *attr,
			  const char *buf, size_t count)
{
	/* int type = simple_strtol(buf, NULL, 10); */
	long type;

	if (kstrtol(buf, 10, &type) == 0)
		memp_set(type);
	else
		return -EINVAL;

	return count;
}

static CLASS_ATTR_RW(memp);

int vdin_create_class_files(struct class *vdin_clsp)
{
	int ret = 0;

	ret = class_create_file(vdin_clsp, &class_attr_memp);
	return ret;
}

void vdin_remove_class_files(struct class *vdin_clsp)
{
	class_remove_file(vdin_clsp, &class_attr_memp);
}

#endif

/*2018-07-18 add debugfs*/
#define DEFINE_SHOW_VDIN(__name) \
static int __name ## _open(struct inode *inode, struct file *file)	\
{ \
	return single_open(file, __name ## _show, inode->i_private);	\
} \
									\
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = __name ## _open,	\
	.read = seq_read,		\
	.llseek = seq_lseek,		\
	.release = single_release,	\
}

DEFINE_SHOW_VDIN(seq_file_vdin_state);

struct vdin_debugfs_files_t {
	const char *name;
	const umode_t mode;
	const struct file_operations *fops;
};

static struct vdin_debugfs_files_t vdin_debugfs_files[] = {
	{"state", S_IFREG | 0644, &seq_file_vdin_state_fops},

};

void vdin_debugfs_init(struct vdin_dev_s *vdevp)
{
	int i;
	struct dentry *ent;
	unsigned int nub;

	nub = vdevp->index;

	if (nub > 0) {
		pr_info("%s only support debug vdin0 %d\n", __func__, nub);
		return;
	}

	if (vdevp->dbg_root)
		return;

	vdevp->dbg_root = debugfs_create_dir("vdin0", NULL);

	if (!vdevp->dbg_root) {
		pr_err("can't create debugfs dir di\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(vdin_debugfs_files); i++) {
		ent = debugfs_create_file(vdin_debugfs_files[i].name,
					  vdin_debugfs_files[i].mode,
					  vdevp->dbg_root, NULL,
					  vdin_debugfs_files[i].fops);
		if (!ent)
			pr_err("debugfs create failed\n");
	}
}

void vdin_debugfs_exit(struct vdin_dev_s *vdevp)
{
	unsigned int nub;

	nub = vdevp->index;
	if (nub > 0) {
		pr_info("%s only support debug vdin0 %d\n", __func__, nub);
		return;
	}

	debugfs_remove(vdevp->dbg_root);
}

void vdin_dump_frames(struct vdin_dev_s *devp)
{
	char file_path[32];
	int  i;

	if (devp->dbg_dump_frames) {
		/* todo:check path available */
		snprintf(file_path, sizeof(file_path), "%s", "/mnt/img");
		pr_info("dir:%s\n", file_path);
		if (devp->afbce_valid == 1) {
			vdin_pause_dec(devp);
			for (i = 0; i < devp->canvas_max_num; i++)
				vdin_dump_one_afbce_mem(file_path, devp, i);

			vdin_resume_dec(devp);
		} else {
			/* mif */
		}
	}
}

/*------------------------------------------*/
