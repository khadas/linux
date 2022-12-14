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

#include "vicp_test.h"
#include "vicp_log.h"
#include "vicp_hardware.h"

ulong input_buffer_addr;
ulong output_buffer_addr;

static void write_yuv_to_buf(void)
{
	struct file *fp = NULL;
	char name_buf[32];
	int data_size;
	u8 *data_addr;
	mm_segment_t fs;
	loff_t pos;

	if (input_color_format == 2) {
		snprintf(name_buf, sizeof(name_buf), "/mnt/%d_%d_nv12.yuv",
			input_width, input_height);
		data_size = input_width * input_height * 3 / 2;
	} else {
		snprintf(name_buf, sizeof(name_buf), "/mnt/%d_%d_yuv444.yuv",
			input_width, input_height);
		data_size = input_width * input_height * 3;
	}

	data_addr = codec_mm_vmap(input_buffer_addr, data_size);

	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp) || IS_ERR_OR_NULL(data_addr)) {
		pr_info("%s: vmap failed.\n", __func__);
		return;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = fp->f_pos;
	vfs_read(fp, data_addr, data_size, &pos);
	fp->f_pos = pos;
	vfs_fsync(fp, 0);
	pr_info("%s: read %u size.\n", __func__, data_size);

	codec_mm_dma_flush(data_addr, data_size, DMA_TO_DEVICE);
	codec_mm_unmap_phyaddr(data_addr);
	set_fs(fs);
	filp_close(fp, NULL);
}

int vicp_test_config(struct vid_cmpr_top_t *vid_cmpr_top)
{
	int buffer_size = 0;
	u8 *temp_addr;

	vicp_print(VICP_ERROR, "enter %s.\n", __func__);
	if (IS_ERR_OR_NULL(vid_cmpr_top)) {
		pr_info("%s: NULL param.\n", __func__);
		return -1;
	}

	//malloc buffer
	buffer_size = input_width * input_height * 3;
	input_buffer_addr = codec_mm_alloc_for_dma("vicp", buffer_size / PAGE_SIZE,
		0, CODEC_MM_FLAGS_DMA);
	write_yuv_to_buf();

	buffer_size = output_width * output_height * 3;
	output_buffer_addr = codec_mm_alloc_for_dma("vicp", buffer_size / PAGE_SIZE,
		0, CODEC_MM_FLAGS_DMA);
	temp_addr = codec_mm_vmap(output_buffer_addr, buffer_size);
	memset(temp_addr, 0x55, buffer_size);
	codec_mm_dma_flush(temp_addr, buffer_size, DMA_TO_DEVICE);
	codec_mm_unmap_phyaddr(temp_addr);

	vid_cmpr_top->src_compress = 0;
	vid_cmpr_top->src_hsize = input_width;
	vid_cmpr_top->src_vsize = input_height;
	vid_cmpr_top->src_head_baddr = 0;
	vid_cmpr_top->src_fmt_mode = input_color_format;
	vid_cmpr_top->src_compbits = input_color_dep;
	vid_cmpr_top->src_win_bgn_h = 0;
	vid_cmpr_top->src_win_end_h = input_width - 1;
	vid_cmpr_top->src_win_bgn_v = 0;
	vid_cmpr_top->src_win_end_v = input_height - 1;
	vid_cmpr_top->src_pip_src_mode = 0;
	vid_cmpr_top->rdmif_canvas0_addr0 = input_buffer_addr;
	if (vid_cmpr_top->src_fmt_mode == 2) {
		vid_cmpr_top->rdmif_canvas0_addr1 = vid_cmpr_top->rdmif_canvas0_addr0 +
			(input_width * input_height);
		vid_cmpr_top->rdmif_separate_en = 2;
	} else {
		vid_cmpr_top->rdmif_canvas0_addr1 = 0;
		vid_cmpr_top->rdmif_separate_en = 0;
	}
	vid_cmpr_top->rdmif_canvas0_addr2 = 0;

	vid_cmpr_top->hdr_en = 0;
	vid_cmpr_top->out_afbce_enable = 0;
	vid_cmpr_top->out_head_baddr = 0;
	vid_cmpr_top->out_mmu_info_baddr = 0;
	vid_cmpr_top->out_reg_init_ctrl = 1;
	vid_cmpr_top->out_reg_pip_mode = 1;
	vid_cmpr_top->out_reg_format_mode = 1;
	vid_cmpr_top->out_reg_compbits = 10;
	vid_cmpr_top->out_hsize_in = output_width;
	vid_cmpr_top->out_vsize_in = output_height;
	vid_cmpr_top->out_hsize_bgnd = output_width;
	vid_cmpr_top->out_vsize_bgnd = output_height;
	vid_cmpr_top->out_win_bgn_h = 0;
	vid_cmpr_top->out_win_end_h = output_width - 1;
	vid_cmpr_top->out_win_bgn_v = 0;
	vid_cmpr_top->out_win_end_v = output_height - 1;
	vid_cmpr_top->out_rot_en = 0;
	vid_cmpr_top->out_shrk_en = 0;
	vid_cmpr_top->out_shrk_mode = 0;
	vid_cmpr_top->wrmif_en = 1;
	vid_cmpr_top->wrmif_fmt_mode = output_color_format;
	vid_cmpr_top->wrmif_bits_mode = output_color_dep;
	vid_cmpr_top->wrmif_canvas0_addr0 = output_buffer_addr;
	if (vid_cmpr_top->wrmif_fmt_mode == 0) {
		vid_cmpr_top->wrmif_set_separate_en = 2;
		vid_cmpr_top->wrmif_canvas0_addr1 = vid_cmpr_top->wrmif_canvas0_addr0 +
			(output_width * output_height);
	} else {
		vid_cmpr_top->wrmif_set_separate_en = 0;
		vid_cmpr_top->wrmif_canvas0_addr1 = 0;
	}
	vid_cmpr_top->wrmif_canvas0_addr2 = 0;

	return 0;
}

static void dump_test_yuv(int flag)
{
	struct file *fp = NULL;
	char name_buf[32];
	int data_size;
	u8 *data_addr;
	mm_segment_t fs;
	loff_t pos;

	//use flag to distinguish src and dst vframe
	if (flag == 0) {
		if (input_color_format == 2) {
			snprintf(name_buf, sizeof(name_buf), "/mnt/in_%d_%d_nv12.yuv",
				input_width, input_height);
			data_size = input_width * input_height * 3 / 2;
		} else {
			snprintf(name_buf, sizeof(name_buf), "/mnt/in_%d_%d_yuv444.yuv",
				input_width, input_height);
			data_size = input_width * input_height * 3;
		}

		data_addr = codec_mm_vmap(input_buffer_addr, data_size);
	} else {
		if (output_color_format == 0) {
			snprintf(name_buf, sizeof(name_buf), "/mnt/out_%d_%d_nv12.yuv",
				output_width, output_height);
			data_size = output_width * output_height * 3 / 2;
		} else {
			snprintf(name_buf, sizeof(name_buf), "/mnt/out_%d_%d_yuv444.yuv",
				output_width, output_height);
			data_size = output_width * output_height * 3;
		}

		data_addr = codec_mm_vmap(output_buffer_addr, data_size);
	}

	codec_mm_dma_flush(data_addr, data_size, DMA_FROM_DEVICE);

	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp) || IS_ERR_OR_NULL(data_addr)) {
		pr_info("%s: vmap failed.\n", __func__);
		return;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = fp->f_pos;
	vfs_write(fp, data_addr, data_size, &pos);
	fp->f_pos = pos;
	vfs_fsync(fp, 0);
	pr_info("%s: write %u size.\n", __func__, data_size);
	codec_mm_unmap_phyaddr(data_addr);
	set_fs(fs);
	filp_close(fp, NULL);
}

int vicp_test(void)
{
	int ret = 0;
	struct vid_cmpr_top_t vid_cmpr_top;

	memset(&vid_cmpr_top, 0, sizeof(struct vid_cmpr_top_t));
	ret = vicp_test_config(&vid_cmpr_top);
	if (ret < 0) {
		vicp_print(VICP_ERROR, "test config failed.\n");
		return ret;
	}

	ret = vicp_process_task(&vid_cmpr_top);
	if (ret < 0) {
		vicp_print(VICP_ERROR, "vicp task failed.\n");
		return ret;
	}

	if (input_buffer_addr != 0) {
		dump_test_yuv(0);
		codec_mm_free_for_dma("vicp", input_buffer_addr);
		input_buffer_addr = 0;
	}

	if (output_buffer_addr != 0) {
		dump_test_yuv(1);
		codec_mm_free_for_dma("vicp", output_buffer_addr);
		output_buffer_addr = 0;
	}

	vicp_process_reset();

	return 0;
}
