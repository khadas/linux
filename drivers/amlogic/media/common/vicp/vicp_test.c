// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#endif
#include <linux/amlogic/media/utils/am_com.h>
#include <linux/amlogic/media/vicp/vicp.h>

#include "vicp_test.h"
#include "vicp_log.h"
#include "vicp_hardware.h"
#include "../../../video_processor/video_composer/video_composer.h"

int create_rgb24_colorbar(int width, int height, int bar_count)
{
	pr_info("%s-%d.\n", __func__, __LINE__);

	unsigned char *data = NULL;
	int barnum, barwidth;
	char filename[100] = {0};
	struct file *fp = NULL;
	mm_segment_t fs;
	loff_t pos;
	int i = 0, j = 0, offset = 0;
	int size = 0;

	if (width <= 0 || height <= 0 || bar_count <= 0) {
		pr_info("%s: invalid param!!!!!!", __func__);
		return -1;
	}

	if (width % bar_count != 0)
		pr_info("Warning: Width cannot be divided by Bar Number without remainder!\n");

	sprintf(filename, "/data/%d_%d_24.rgb", width, height);
	fp = filp_open(filename, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp)) {
		pr_info("%s: open file failed.\n", __func__);
		return -1;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = fp->f_pos;

	size = width * height * 3;
	data = vmalloc(size);
	memset(data, 0, size);
	barwidth = width / bar_count;

	for (j = 0; j < height; j++) {
		for (i = 0; i < width; i++) {
			barnum = i / barwidth;
			offset = (j * width + i) * 3;
			switch (barnum) {
			case 0:{
				data[offset + 0] = 255;
				data[offset + 1] = 255;
				data[offset + 2] = 255;
				break;
			}
			case 1:{
				data[offset + 0] = 255;
				data[offset + 1] = 255;
				data[offset + 2] = 0;
				break;
			}
			case 2:{
				data[offset + 0] = 0;
				data[offset + 1] = 255;
				data[offset + 2] = 255;
				break;
			}
			case 3:{
				data[offset + 0] = 0;
				data[offset + 1] = 255;
				data[offset + 2] = 0;
				break;
			}
			case 4:{
				data[offset + 0] = 255;
				data[offset + 1] = 0;
				data[offset + 2] = 255;
				break;
			}
			case 5:{
				data[offset + 0] = 255;
				data[offset + 1] = 0;
				data[offset + 2] = 0;
				break;
			}
			case 6:{
				data[offset + 0] = 0;
				data[offset + 1] = 0;
				data[offset + 2] = 255;
				break;
			}
			case 7:{
				data[offset + 0] = 0;
				data[offset + 1] = 0;
				data[offset + 2] = 0;
				break;
			}
			}
		}
	}

	vfs_write(fp, data, size, &pos);
	fp->f_pos = pos;
	vfs_fsync(fp, 0);
	set_fs(fs);
	filp_close(fp, NULL);
	vfree(data);

	return 0;
}

static unsigned char image_clip_value(unsigned char val, unsigned char min, unsigned char max)
{
	if (val > max)
		return max;
	else if (val < min)
		return min;
	else
		return val;
}

int rgb24_to_yuv420p(char *yuv_file, char *rgb_file, int width, int height)
{
	int i = 0, j = 0, count = 0;
	unsigned char y, u, v, r, g, b;
	unsigned char *ptr_y, *ptr_u, *ptr_v, *ptr_rgb;
	unsigned char *rgb_buf = NULL;
	unsigned char *yuv_buf = NULL;
	int rgb_size = 0, yuv_size = 0;
	struct file *rgb_fp = NULL;
	struct file *yuv_fp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs;

	if (!yuv_file || !rgb_file) {
		pr_info("%s: param is NULL !!!!!!", __func__);
		return -1;
	}

	rgb_fp = filp_open(rgb_file, O_RDONLY, 0444);
	yuv_fp = filp_open(yuv_file, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(rgb_fp) || IS_ERR(yuv_fp)) {
		pr_info("%s: open file failed.\n", __func__);
		return -1;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	rgb_size = width * height * 3;
	yuv_size = width * height * 3 / 2;
	rgb_buf = vmalloc(rgb_size);
	yuv_buf = vmalloc(yuv_size);

	count = vfs_read(rgb_fp, rgb_buf, rgb_size, &pos);
	pr_info("read count = %u\n", count);

	ptr_y = yuv_buf;
	ptr_u = yuv_buf + width * height;
	ptr_v = ptr_u + (width * height * 1 / 4);

	for (j = 0; j < height; j++) {
		ptr_rgb = rgb_buf + width * j * 3;
		for (i = 0; i < width; i++) {
			r = *(ptr_rgb++);
			g = *(ptr_rgb++);
			b = *(ptr_rgb++);
			y = (unsigned char)((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
			u = (unsigned char)((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
			v = (unsigned char)((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;

			*(ptr_y) = image_clip_value(y, 0, 255);
			ptr_y++;

			if (j % 2 == 0 && i % 2 == 0) {
				*(ptr_u) = image_clip_value(u, 0, 255);
				ptr_u++;
			} else {
				if (i % 2 == 0) {
					*(ptr_v) = image_clip_value(v, 0, 255);
					ptr_v++;
				}
			}
		}
	}

	pos = 0;
	count = vfs_write(yuv_fp, yuv_buf, yuv_size, &pos);
	pr_info("write count = %u\n", count);

	filp_close(rgb_fp, NULL);
	filp_close(yuv_fp, NULL);

	vfree(rgb_buf);
	vfree(yuv_buf);

	set_fs(old_fs);
	return 0;
}

int rgb24_to_nv12(char *nv12_file, char *rgb_file, int width, int height)
{
	int i = 0, j = 0, count = 0;
	unsigned char y, u, v, r, g, b;
	unsigned char *ptr_y, *ptr_u, *ptr_rgb;
	unsigned char *rgb_buf = NULL;
	unsigned char *yuv_buf = NULL;
	int rgb_size = 0, yuv_size = 0;
	struct file *rgb_fp = NULL;
	struct file *yuv_fp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs;

	if (!nv12_file || !rgb_file) {
		pr_info("%s: param is NULL !!!!!!", __func__);
		return -1;
	}

	rgb_fp = filp_open(rgb_file, O_RDONLY, 0444);
	yuv_fp = filp_open(nv12_file, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(rgb_fp) || IS_ERR(yuv_fp)) {
		pr_info("%s: open file failed.\n", __func__);
		return -1;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	rgb_size = width * height * 3;
	yuv_size = width * height * 3 / 2;
	rgb_buf = vmalloc(rgb_size);
	yuv_buf = vmalloc(yuv_size);

	count = vfs_read(rgb_fp, rgb_buf, rgb_size, &pos);
	pr_info("read count = %u\n", count);

	ptr_y = yuv_buf;
	ptr_u = yuv_buf + width * height;

	for (j = 0; j < height; j++) {
		ptr_rgb = rgb_buf + width * j * 3;
		for (i = 0; i < width; i++) {
			r = *(ptr_rgb++);
			g = *(ptr_rgb++);
			b = *(ptr_rgb++);
			y = (unsigned char)((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
			u = (unsigned char)((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
			v = (unsigned char)((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;

			*(ptr_y++) = image_clip_value(y, 0, 255);

			if (j % 2 == 0 && i % 2 == 0) {
				*(ptr_u++) = image_clip_value(u, 0, 255);
				*(ptr_u++) = image_clip_value(v, 0, 255);
			}
		}
	}

	pos = 0;
	count = vfs_write(yuv_fp, yuv_buf, yuv_size, &pos);
	pr_info("write count = %u\n", count);

	filp_close(rgb_fp, NULL);
	filp_close(yuv_fp, NULL);

	vfree(rgb_buf);
	vfree(yuv_buf);

	set_fs(old_fs);
	return 0;
}

int create_nv12_colorbar_file(int width, int height, int bar_count)
{
	struct file *fp = NULL;
	mm_segment_t fs;
	loff_t pos;
	unsigned char *data = NULL;
	unsigned char *data_y = NULL;
	unsigned char *data_uv = NULL;

	int i = 0, j = 0;
	char filename[100] = {0};
	int barwidth = 0, barnum = 0, size = 0;

	if (width <= 0 || height <= 0 || bar_count <= 0) {
		vicp_print(VICP_ERROR, "%s: invalid param!!!!!!\n", __func__);
		return -1;
	}

	if (width % bar_count != 0)
		vicp_print(VICP_INFO,
			"Warning: Width cannot be divided by Bar Number without remainder!\n");

	size = width * height * 3 / 2;
	data = vmalloc(size);
	memset(data, 0, size);
	data_y = data;
	data_uv = data + width * height;
	barwidth = width / bar_count;

	sprintf(filename, "/data/%d_%d_nv12.yuv", width, height);
	fp = filp_open(filename, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp)) {
		vicp_print(VICP_ERROR, "%s: open file failed.\n", __func__);
		vfree(data);
		data = NULL;
		data_y = NULL;
		data_uv = NULL;
		return -1;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = fp->f_pos;

	for (j = 0; j < height; j++) {
		for (i = 0; i < width; i++) {
			barnum = i / barwidth;
			switch (barnum) {
			case 0:{
				*(data_y++) = 235;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 128;
					*(data_uv++) = 128;
				}
				break;
			}
			case 1:{
				*(data_y++) = 210;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 16;
					*(data_uv++) = 146;
				}
				break;
			}
			case 2:{
				*(data_y++) = 169;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 166;
					*(data_uv++) = 16;
				}
				break;
			}
			case 3:{
				*(data_y++) = 144;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 54;
					*(data_uv++) = 34;
				}
				break;
			}
			case 4:{
				*(data_y++) = 107;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 202;
					*(data_uv++) = 222;
				}
				break;
			}
			case 5:{
				*(data_y++) = 82;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 90;
					*(data_uv++) = 240;
				}
				break;
			}
			case 6:{
				*(data_y++) = 41;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 240;
					*(data_uv++) = 110;
				}
				break;
			}
			case 7:{
				*(data_y++) = 16;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 128;
					*(data_uv++) = 128;
				}
				break;
			}
			}
		}
	}

	vfs_write(fp, data, size, &pos);
	fp->f_pos = pos;
	vfs_fsync(fp, 0);
	set_fs(fs);
	filp_close(fp, NULL);
	vfree(data);

	return 0;
}

int create_nv12_colorbar_buf(u8 *addr, int width, int height, int bar_count)
{
	int i = 0, j = 0;
	int barwidth = 0, barnum = 0, size = 0;
	unsigned char *data_y = NULL;
	unsigned char *data_uv = NULL;

	if (!addr) {
		vicp_print(VICP_ERROR, "%s: NULL param.\n", __func__);
		return -1;
	}

	if (width <= 0 || height <= 0 || bar_count <= 0) {
		vicp_print(VICP_ERROR, "%s: invalid param!!!!!!\n", __func__);
		return -1;
	}

	if (width % bar_count != 0)
		vicp_print(VICP_INFO,
			"Warning: Width cannot be divided by Bar Number without remainder!\n");

	size = width * height * 3 / 2;
	memset(addr, 0, size);
	data_y = addr;
	data_uv = addr + width * height;
	barwidth = width / bar_count;

	for (j = 0; j < height; j++) {
		for (i = 0; i < width; i++) {
			barnum = i / barwidth;
			switch (barnum) {
			case 0:{
				*(data_y++) = 235;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 128;
					*(data_uv++) = 128;
				}
				break;
			}
			case 1:{
				*(data_y++) = 210;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 16;
					*(data_uv++) = 146;
				}
				break;
			}
			case 2:{
				*(data_y++) = 169;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 166;
					*(data_uv++) = 16;
				}
				break;
			}
			case 3:{
				*(data_y++) = 144;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 54;
					*(data_uv++) = 34;
				}
				break;
			}
			case 4:{
				*(data_y++) = 107;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 202;
					*(data_uv++) = 222;
				}
				break;
			}
			case 5:{
				*(data_y++) = 82;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 90;
					*(data_uv++) = 240;
				}
				break;
			}
			case 6:{
				*(data_y++) = 41;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 240;
					*(data_uv++) = 110;
				}
				break;
			}
			case 7:{
				*(data_y++) = 16;
				if (j % 2 == 0 && i % 2 == 0) {
					*(data_uv++) = 128;
					*(data_uv++) = 128;
				}
				break;
			}
			}
		}
	}
	codec_mm_dma_flush(addr, size, DMA_TO_DEVICE);

	return 0;
}

static void dump_test_yuv(int flag, int width, int height, ulong addr, int num)
{
	struct file *fp = NULL;
	char name_buf[32];
	int data_size;
	u8 *data_addr;
	mm_segment_t fs;
	loff_t pos;

	//use flag to distinguish src and dst vframe
	if (flag == 0) {
		snprintf(name_buf, sizeof(name_buf), "/data/in_%d_%d_nv12-%d.yuv",
			width, height, num);
		data_size = width * height * 3 / 2;
		data_addr = codec_mm_vmap(addr, data_size);
	} else {
		snprintf(name_buf, sizeof(name_buf), "/data/out_%d_%d_nv12-%d.yuv",
			width, height, num);
		data_size = width * height * 3 / 2;
		data_addr = codec_mm_vmap(addr, data_size);
	}

	if (IS_ERR_OR_NULL(data_addr)) {
		vicp_print(VICP_ERROR, "%s: vmap failed.\n", __func__);
		return;
	}

	codec_mm_dma_flush(data_addr, data_size, DMA_FROM_DEVICE);

	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp)) {
		vicp_print(VICP_ERROR, "%s: open file failed.\n", __func__);
		return;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = fp->f_pos;
	vfs_write(fp, data_addr, data_size, &pos);
	fp->f_pos = pos;
	vfs_fsync(fp, 0);
	vicp_print(VICP_INFO, "%s: write %u size.\n", __func__, data_size);

	codec_mm_dma_flush(data_addr, data_size, DMA_TO_DEVICE);
	codec_mm_unmap_phyaddr(data_addr);
	set_fs(fs);
	filp_close(fp, NULL);
}

int vicp_test(void)
{
	int ret = 0;
	int buf_size = 0, buf_width = 0, buf_height = 0;
	ulong mif_in_addr = 0, mif_out_addr = 0;
	u8 *vitr_addr;
	int input_width = 0, input_height = 0, output_width = 0, output_height = 0;
	ulong fbc_head_addr = 0, fbc_body_addr = 0, fbc_table_addr = 0;
	int fbc_body_size = 0, fbc_head_size = 0, fbc_table_size = 0;
	int i = 0;
	u32 *virt_addr = NULL;
	u32 temp_body_addr;
	struct dma_data_config_s data_dma;
	struct vicp_data_config_s data_config;
	struct vframe_s *test_vframe = NULL;
	struct timeval time1, time2, time3;
	int cost_time = 0;
	int result1 = -1, result2 = -1, result3 = -1;

	//mif->mif
	input_width = 1920;
	input_height = 1080;
	output_width = 1920;
	output_height = 1080;
	buf_size = input_width * input_height * 3 / 2;
	mif_in_addr = codec_mm_alloc_for_dma("vicp", buf_size / PAGE_SIZE, 0, CODEC_MM_FLAGS_DMA);
	if (mif_in_addr == 0) {
		vicp_print(VICP_ERROR, "%s: alloc input buf failed.\n", __func__);
		return -1;
	}
	vitr_addr = codec_mm_vmap(mif_in_addr, buf_size);
	memset(vitr_addr, 0, buf_size);
	do_gettimeofday(&time1);
	ret = create_nv12_colorbar_buf(vitr_addr, input_width, input_height, 8);
	if (ret != 0) {
		vicp_print(VICP_ERROR, "%s: create nv12 colorbar failed.\n", __func__);
		goto exit;
	}
	do_gettimeofday(&time2);
	cost_time = (1000000 * (time2.tv_sec - time1.tv_sec)
		+ (time2.tv_usec - time1.tv_usec));
	vicp_print(VICP_INFO, "create yuv file cost: %d us\n", cost_time);

	memset(&data_dma, 0, sizeof(struct dma_data_config_s));
	data_dma.buf_addr = mif_in_addr;
	data_dma.buf_stride_w = input_width;
	data_dma.buf_stride_h = input_height;
	data_dma.color_format = VICP_COLOR_FORMAT_YUV420;
	data_dma.color_depth = 8;
	data_dma.data_width = input_width;
	data_dma.data_height = input_height;
	data_dma.plane_count = 2;
	data_dma.endian = 0;
	data_dma.need_swap_cbcr = 0;
	memset(&data_config, 0, sizeof(struct vicp_data_config_s));
	data_config.input_data.is_vframe = false;
	data_config.input_data.data_dma = &data_dma;

	buf_size = output_width * output_height * 3 / 2;
	buf_size = PAGE_ALIGN(buf_size);
	mif_out_addr = codec_mm_alloc_for_dma("vicp", buf_size / PAGE_SIZE, 0,
		CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR);
	data_config.output_data.fbc_out_en = false;
	data_config.output_data.mif_out_en = true;
	data_config.output_data.mif_color_fmt = VICP_COLOR_FORMAT_YUV420;
	data_config.output_data.mif_color_dep = 8;
	data_config.output_data.phy_addr[0] = mif_out_addr;
	data_config.output_data.stride[0] = output_width;
	data_config.output_data.width = output_width;
	data_config.output_data.height = output_height;
	data_config.output_data.endian = 0;
	data_config.output_data.need_swap_cbcr = 0;

	data_config.data_option.crop_info.left = 0;
	data_config.data_option.crop_info.top = 0;
	data_config.data_option.crop_info.width = output_width;
	data_config.data_option.crop_info.height = output_height;
	data_config.data_option.output_axis.left = 0;
	data_config.data_option.output_axis.top = 0;
	data_config.data_option.output_axis.width = output_width;
	data_config.data_option.output_axis.height = output_height;
	data_config.data_option.rotation_mode = 0;
	data_config.data_option.rdma_enable = 0;
	data_config.data_option.security_enable = 0;
	data_config.data_option.shrink_mode = 0;
	data_config.data_option.skip_mode = 0;
	data_config.data_option.input_source_count = 0;
	data_config.data_option.input_source_number = 0;
	ret = vicp_process(&data_config);
	if (ret != 0) {
		vicp_print(VICP_ERROR, "%s: dma input, mif output failed.\n", __func__);
		goto exit;
	}

	result1 = vicp_crc0_check(VICP_CRC0_CHECK_FLAG0);
	do_gettimeofday(&time3);
	cost_time = (1000000 * (time3.tv_sec - time1.tv_sec)
		+ (time3.tv_usec - time1.tv_usec));
	vicp_print(VICP_INFO, "test cost: %d us\n", cost_time);
	if (dump_yuv_flag)
		dump_test_yuv(1,
			output_width,
			output_height,
			data_config.output_data.phy_addr[0],
			1);
	codec_mm_free_for_dma("vicp", mif_in_addr);
	mif_in_addr = 0;

	//mif->fbc
	input_width = 1920;
	input_height = 1080;
	output_width = 3840;
	output_height = 2160;
	memset(&data_dma, 0, sizeof(struct dma_data_config_s));
	data_dma.buf_addr = mif_out_addr;
	data_dma.buf_stride_w = input_width;
	data_dma.buf_stride_h = input_height;
	data_dma.color_format = VICP_COLOR_FORMAT_YUV420;
	data_dma.color_depth = 8;
	data_dma.data_width = input_width;
	data_dma.data_height = input_height;
	data_dma.plane_count = 2;
	data_dma.endian = 0;
	data_dma.need_swap_cbcr = 0;
	memset(&data_config, 0, sizeof(struct vicp_data_config_s));
	data_config.input_data.is_vframe = false;
	data_config.input_data.data_dma = &data_dma;

	buf_width = (output_width + 0x1f) & ~0x1f;
	buf_height = output_height;
	fbc_body_size = (buf_width * buf_height + (1024 * 1658)) * 3 / 2;
	fbc_body_size = roundup(PAGE_ALIGN(fbc_body_size), PAGE_SIZE);
	fbc_head_size = (roundup(buf_width, 64) * roundup(buf_height, 64)) / 32;
	fbc_head_size = PAGE_ALIGN(fbc_head_size);
	fbc_table_size = PAGE_ALIGN((fbc_body_size * 4) / PAGE_SIZE);
	buf_size = fbc_body_size + fbc_head_size;
	buf_size = PAGE_ALIGN(buf_size);

	fbc_body_addr = codec_mm_alloc_for_dma("vicp", buf_size / PAGE_SIZE, 0,
		CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR);
	if (fbc_body_addr == 0) {
		vicp_print(VICP_ERROR, "cma memory config fail\n");
		return -1;
	}
	codec_mm_memset(fbc_body_addr, 0, buf_size);

	fbc_head_addr = fbc_body_addr + fbc_body_size;
	fbc_table_addr = codec_mm_alloc_for_dma("vicp",
				fbc_table_size / PAGE_SIZE,
				0,
				CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR);
	if (fbc_table_addr == 0) {
		vicp_print(VICP_ERROR, "alloc table buf fail.\n");
		return -1;
	}
	codec_mm_memset(fbc_table_addr, 0, fbc_table_size);

	temp_body_addr = fbc_body_addr & 0xffffffff;
	virt_addr = codec_mm_phys_to_virt(fbc_table_addr);
	memset(virt_addr, 0, fbc_table_size);
	for (i = 0; i < fbc_body_size; i += 4096) {
		*virt_addr = ((i + temp_body_addr) >> 12) & 0x000fffff;
		virt_addr++;
	}

	codec_mm_dma_flush(codec_mm_phys_to_virt(fbc_table_addr), fbc_table_size, DMA_TO_DEVICE);

	vicp_print(VICP_INFO, "HeadAddr = 0x%lx, BodyAddr = 0x%lx, tableAddr = 0x%lx.\n",
		fbc_head_addr, fbc_body_addr, fbc_table_addr);
	vicp_print(VICP_INFO, "headsize = 0x%x, bodysize = 0x%x, tablesize = 0x%x.\n",
		fbc_head_size, fbc_body_size, fbc_table_size);

	data_config.output_data.fbc_out_en = true;
	data_config.output_data.mif_out_en = false;
	data_config.output_data.fbc_color_fmt = VICP_COLOR_FORMAT_YUV420;
	data_config.output_data.fbc_color_dep = 8;
	data_config.output_data.phy_addr[0] = fbc_body_addr;
	data_config.output_data.stride[0] = output_width;
	data_config.output_data.width = output_width;
	data_config.output_data.height = output_height;
	data_config.output_data.endian = 0;
	data_config.output_data.need_swap_cbcr = 0;
	data_config.output_data.phy_addr[1] = fbc_head_addr;
	data_config.output_data.stride[1] = output_width;
	data_config.output_data.phy_addr[2] = fbc_table_addr;
	data_config.output_data.stride[2] = output_width;
	data_config.output_data.fbc_init_ctrl = 1;
	data_config.output_data.fbc_pip_mode = 1;

	data_config.data_option.crop_info.left = 0;
	data_config.data_option.crop_info.top = 0;
	data_config.data_option.crop_info.width = output_width;
	data_config.data_option.crop_info.height = output_height;
	data_config.data_option.output_axis.left = 0;
	data_config.data_option.output_axis.top = 0;
	data_config.data_option.output_axis.width = output_width;
	data_config.data_option.output_axis.height = output_height;
	data_config.data_option.rotation_mode = 0;
	data_config.data_option.rdma_enable = 0;
	data_config.data_option.security_enable = 0;
	data_config.data_option.shrink_mode = 0;
	data_config.data_option.skip_mode = 0;
	data_config.data_option.input_source_count = 0;
	data_config.data_option.input_source_number = 0;

	ret = vicp_process(&data_config);
	if (ret != 0) {
		vicp_print(VICP_ERROR, "%s: mif input,fbc output failed.\n", __func__);
		goto exit;
	}

	result2 = vicp_crc0_check(VICP_CRC0_CHECK_FLAG1);
	codec_mm_free_for_dma("vicp", mif_out_addr);
	mif_out_addr = 0;

	//fbc->mif
	input_width = 3840;
	input_height = 2160;
	output_width = 1920;
	output_height = 1080;
	data_config.input_data.is_vframe = true;
	test_vframe = vmalloc(sizeof(*test_vframe));
	memset(test_vframe, 0, sizeof(struct vframe_s));
	test_vframe->compWidth = input_width;
	test_vframe->compHeight = input_height;
	test_vframe->compHeadAddr = fbc_head_addr;
	test_vframe->compBodyAddr = fbc_body_addr;
	test_vframe->canvas0Addr = -1;
	test_vframe->canvas1Addr = -1;
	test_vframe->plane_num = 2;
	test_vframe->bitdepth = (BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8);
	test_vframe->flag |= VFRAME_FLAG_VIDEO_LINEAR;
	test_vframe->type |= (VIDTYPE_COMPRESS | VIDTYPE_SCATTER | VIDTYPE_PROGRESSIVE);
	test_vframe->type |= (VIDTYPE_VIU_FIELD | VIDTYPE_VIU_NV21);
	data_config.input_data.data_vf = test_vframe;

	if (dump_yuv_flag)
		vd_vframe_afbc_soft_decode(test_vframe, 1);

	data_config.output_data.fbc_out_en = false;
	data_config.output_data.mif_out_en = true;
	buf_size = buf_width * buf_height * 3 / 2;
	buf_size = PAGE_ALIGN(buf_size);
	mif_out_addr = codec_mm_alloc_for_dma("vicp", buf_size / PAGE_SIZE, 0,
		CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR);
	codec_mm_memset(mif_out_addr, 0, buf_size);
	data_config.output_data.mif_color_fmt = VICP_COLOR_FORMAT_YUV420;
	data_config.output_data.mif_color_dep = 8;
	data_config.output_data.phy_addr[0] = mif_out_addr;
	data_config.output_data.stride[0] = output_width;
	data_config.output_data.width = output_width;
	data_config.output_data.height = output_height;
	data_config.output_data.endian = 0;
	data_config.output_data.need_swap_cbcr = 0;

	data_config.data_option.crop_info.left = 0;
	data_config.data_option.crop_info.top = 0;
	data_config.data_option.crop_info.width = output_width;
	data_config.data_option.crop_info.height = output_height;
	data_config.data_option.output_axis.left = 0;
	data_config.data_option.output_axis.top = 0;
	data_config.data_option.output_axis.width = output_width;
	data_config.data_option.output_axis.height = output_height;
	data_config.data_option.rotation_mode = 0;
	data_config.data_option.rdma_enable = 0;
	data_config.data_option.security_enable = 0;
	data_config.data_option.shrink_mode = 0;
	data_config.data_option.skip_mode = 0;
	data_config.data_option.input_source_count = 0;
	data_config.data_option.input_source_number = 0;
	ret = vicp_process(&data_config);
	if (ret != 0) {
		vicp_print(VICP_ERROR, "%s: fbc input, mif output failed.\n", __func__);
		goto exit;
	}

	result3 = vicp_crc0_check(VICP_CRC0_CHECK_FLAG2);
	if (dump_yuv_flag)
		dump_test_yuv(1,
			output_width,
			output_height,
			data_config.output_data.phy_addr[0],
			3);

	vfree(test_vframe);
exit:
	if (mif_in_addr != 0) {
		codec_mm_free_for_dma("vicp", mif_in_addr);
		mif_in_addr = 0;
	}

	if (mif_out_addr != 0) {
		codec_mm_free_for_dma("vicp", mif_out_addr);
		mif_out_addr = 0;
	}

	if (fbc_body_addr != 0) {
		codec_mm_free_for_dma("vicp", fbc_body_addr);
		fbc_body_addr = 0;
	}

	if (fbc_table_addr != 0) {
		codec_mm_free_for_dma("vicp", fbc_table_addr);
		fbc_table_addr = 0;
	}

	do_gettimeofday(&time3);
	cost_time = (1000000 * (time3.tv_sec - time1.tv_sec)
		+ (time3.tv_usec - time1.tv_usec));
	vicp_print(VICP_INFO, "all test cost: %d us\n", cost_time);
	if (result1 != 0 || result2 != 0 || result3 != 0) {
		pr_info("%s: result: failed.\n", __func__);
		ret = -1;
	} else {
		pr_info("%s: result: success.\n", __func__);
		ret = 1;
	}

	return ret;
}
