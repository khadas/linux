// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/utils/amlog.h>
#include <linux/amlogic/media/ge2d/ge2d_func.h>
#include "vframe_ge2d_composer.h"
#include "vfq.h"
#include <linux/amlogic/cpu_version.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include "vc_util.h"
#include "vframe_dewarp_composer.h"

#define GDC_FIRMWARE_PATH    "/vendor/lib/firmware/gdc/"

static unsigned int dewarp_com_dump;
MODULE_PARM_DESC(dewarp_com_dump, "\n dewarp_com_dump\n");
module_param(dewarp_com_dump, uint, 0664);

int get_dewarp_format(struct vframe_s *vf)
{
	int format = NV12;

	if (IS_ERR_OR_NULL(vf)) {
		pr_info("%s: vf is NULL.\n", __func__);
		return -1;
	}

	if ((vf->type & VIDTYPE_VIU_NV21) ||
	    (vf->type & VIDTYPE_VIU_NV12))
		format = NV12;
	else if (vf->type & VIDTYPE_VIU_422)
		format = 0;
	else if (vf->type & VIDTYPE_VIU_444)
		format = YUV444_P;
	else if (vf->type & VIDTYPE_RGB_444)
		format = RGB444_P;

	return format;
}

static int dump_vframe(char *path, u32 phy_adr, int size)
{
	int ret = 0;
	struct file *fp = NULL;
	mm_segment_t fs;
	loff_t position = 0;
	u8 *data;

	if (IS_ERR_OR_NULL(path)) {
		pr_info("%s: path is NULL.\n", __func__);
		return -1;
	}

	/* open file to write */
	fp = filp_open(path, O_WRONLY | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		pr_info("%s: open file error\n", __func__);
		return -1;
	}

	/* Write buf to file */
	data = codec_mm_vmap(phy_adr, size);
	if (!data) {
		pr_info("%s: vmap failed\n", __func__);
		return -1;
	}
	/* change to KERNEL_DS address limit */
	fs = get_fs();
	set_fs(KERNEL_DS);
	ret = vfs_write(fp, data, size, &position);
	codec_mm_unmap_phyaddr(data);

	vfs_fsync(fp, 0);
	filp_close(fp, NULL);
	set_fs(fs);

	pr_info("%s: want write size: %d, real write size: %d.\n",
			__func__, size, ret);

	return ret;
}

bool is_dewarp_supported(struct dewarp_composer_para *param)
{
	int ret;
	char file_name[64];
	struct kstat stat;
	int rotate_value = 0;
	struct composer_vf_para *composer_vf_param = NULL;

	if (!is_aml_gdc_supported()) {
		vc_print(param->vc_index, PRINT_DEWARP,
			"%s: hardware not support.\n", __func__);
		return false;
	}

	if (IS_ERR_OR_NULL(param)) {
		vc_print(param->vc_index, PRINT_DEWARP,
			"%s: NULL param, please check.\n", __func__);
		return false;
	}

	composer_vf_param = param->vf_para;
	if (IS_ERR_OR_NULL(param)) {
		vc_print(param->vc_index, PRINT_DEWARP,
			"%s: composer_vf_param is NULL.\n", __func__);
		return false;
	}

	if (composer_vf_param->src_vf_format != NV12) {
		vc_print(param->vc_index, PRINT_DEWARP,
			"%s: not support format: %d.\n", __func__,
			composer_vf_param->src_vf_format);
		return false;
	}

	if (composer_vf_param->src_vf_angle == VC_TRANSFORM_ROT_90)
		rotate_value = 90;
	else if (composer_vf_param->src_vf_angle == VC_TRANSFORM_ROT_180)
		rotate_value = 180;
	else if (composer_vf_param->src_vf_angle == VC_TRANSFORM_ROT_270)
		rotate_value = 270;

	memset(file_name, 0, 64);
	sprintf(file_name, "%s%dx%d-%dx%d-%d_nv12.bin",
		GDC_FIRMWARE_PATH,
		composer_vf_param->src_vf_width,
		composer_vf_param->src_vf_height,
		composer_vf_param->dst_vf_width,
		composer_vf_param->dst_vf_height,
		rotate_value);

	ret = vfs_stat(file_name, &stat);
	if (ret < 0) {
		vc_print(param->vc_index, PRINT_DEWARP,
			"%s: %s don't exist.\n", __func__, file_name);
		return false;
	}

	return true;
}

int init_dewarp_composer(struct dewarp_composer_para *param)
{
	int ret = 0;
	char file_name[64];
	int rotate_value = 0;

	if (IS_ERR_OR_NULL(param)) {
		vc_print(param->vc_index, PRINT_ERROR,
			"%s: NULL param, please check.\n", __func__);
		return -1;
	}

	if (param->fw_load.phys_addr == 0) {
		if (param->vf_para->src_vf_angle == VC_TRANSFORM_ROT_90)
			rotate_value = 90;
		else if (param->vf_para->src_vf_angle == VC_TRANSFORM_ROT_180)
			rotate_value = 180;
		else if (param->vf_para->src_vf_angle == VC_TRANSFORM_ROT_270)
			rotate_value = 270;

		memset(file_name, 0, 64);
		sprintf(file_name, "%dx%d-%dx%d-%d_nv12.bin",
			param->vf_para->src_vf_width,
			param->vf_para->src_vf_height,
			param->vf_para->dst_vf_width,
			param->vf_para->dst_vf_height,
			rotate_value);

		ret = load_firmware_by_name(file_name, &param->fw_load);
		if (ret <= 0) {
			vc_print(param->vc_index, PRINT_ERROR,
				"%s: load firmware failed.\n", __func__);
			return -1;
		}
	} else {
		vc_print(param->vc_index, PRINT_DEWARP,
			"%s: firmware already load.\n", __func__);
	}

	if (IS_ERR_OR_NULL(param->context)) {
		param->context = create_gdc_work_queue(AML_GDC);
		if (IS_ERR_OR_NULL(param->context)) {
			vc_print(param->vc_index, PRINT_DEWARP,
				"%s: create dewrap work_queue failed.\n",
				__func__);
			ret = -1;
		} else {
			vc_print(param->vc_index, PRINT_DEWARP,
				"%s: create dewrap work_queue success.\n",
				__func__);
		}
	} else {
		vc_print(param->vc_index, PRINT_DEWARP,
			"%s: dewrap work queue exist.\n", __func__);
	}

	return ret;
}

int uninit_dewarp_composer(struct dewarp_composer_para *param)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(param)) {
		vc_print(param->vc_index, PRINT_ERROR,
			"%s: NULL param, please check.\n", __func__);
		return -1;
	}

	if (param->fw_load.phys_addr != 0) {
		vc_print(param->vc_index, PRINT_DEWARP,
			"%s: release firmware.\n", __func__);
		release_config_firmware(&param->fw_load);
	} else {
		vc_print(param->vc_index, PRINT_DEWARP,
			"%s: firmware don't load.\n", __func__);
	}

	if (!IS_ERR_OR_NULL(param->context)) {
		ret = destroy_gdc_work_queue(param->context);
		if (ret != 0) {
			vc_print(param->vc_index, PRINT_ERROR,
				"%s: destroy dewarp work queue failed.\n",
				__func__);
			ret = -1;
		} else {
			vc_print(param->vc_index, PRINT_DEWARP,
				"%s: destroy dewarp work queue success.\n",
				__func__);
		}
	} else {
		vc_print(param->vc_index, PRINT_DEWARP,
			"%s: dewarp work queue not create.\n",
			__func__);
	}

	return ret;
}

int config_dewarp_vframe(int vc_index, int rotation,
				struct vframe_s *src_vf,
				struct dst_buf_t *dst_buf,
				struct composer_vf_para *vframe_para)
{
	struct vframe_s *vf = NULL;

	if (IS_ERR_OR_NULL(src_vf) ||
		IS_ERR_OR_NULL(dst_buf) ||
		IS_ERR_OR_NULL(vframe_para)) {
		vc_print(vc_index, PRINT_ERROR,
			"%s: NULL param, please check.\n", __func__);
		return -1;
	}

	if (src_vf->canvas0_config[0].phy_addr == 0) {
		if ((src_vf->flag &  VFRAME_FLAG_DOUBLE_FRAM) &&
			src_vf->vf_ext) {
			vf = src_vf->vf_ext;
		} else {
			vc_print(vc_index, PRINT_ERROR,
				"%s: vf no yuv data.\n", __func__);
			return -1;
		}
	} else {
		vf = src_vf;
	}

	vframe_para->src_vf_width = vf->width;
	vframe_para->src_vf_height = vf->height;
	vframe_para->src_vf_format = get_dewarp_format(vf);
	vframe_para->src_vf_plane_count = 2;
	vframe_para->src_buf_addr = vf->canvas0_config[0].phy_addr;
	vframe_para->src_buf_stride = vf->canvas0_config[0].width;

	vframe_para->dst_vf_width = dst_buf->buf_w;
	vframe_para->dst_vf_height = dst_buf->buf_h;
	vframe_para->dst_vf_plane_count = 2;
	vframe_para->dst_buf_addr = dst_buf->phy_addr;
	vframe_para->dst_buf_stride = dst_buf->buf_w;
	vframe_para->src_vf_angle = rotation;

	vc_print(vc_index, PRINT_DEWARP,
		"src_vf, w:%d, h:%d, fromat:%d.\n",
			vframe_para->src_vf_width,
			vframe_para->src_vf_height,
			vframe_para->src_vf_format);
	vc_print(vc_index, PRINT_DEWARP,
		"src_buf, w:%d.\n", vframe_para->src_buf_stride);
	vc_print(vc_index, PRINT_DEWARP,
		"dst_vf, w:%d, h:%d.\n",
			vframe_para->dst_vf_width, vframe_para->dst_vf_height);
	vc_print(vc_index, PRINT_DEWARP,
		"dst_buf, w:%d.\n", vframe_para->dst_buf_stride);
	return 0;
}

int dewarp_data_composer(struct dewarp_composer_para *param)
{
	int ret, dump_num = 0;
	struct gdc_phy_setting gdc_config;
	int src_vf_size, dst_vf_size;
	char dump_name[32];

	if (IS_ERR_OR_NULL(param)) {
		vc_print(param->vc_index, PRINT_ERROR,
			"%s: NULL param, please check.\n", __func__);
		return -1;
	}

	memset(&gdc_config, 0, sizeof(struct gdc_phy_setting));
	gdc_config.format = param->vf_para->src_vf_format;
	gdc_config.in_width = param->vf_para->src_vf_width;
	gdc_config.in_height = param->vf_para->src_vf_height;
	/*16-byte alignment*/
	gdc_config.in_y_stride = AXI_WORD_ALIGN(param->vf_para->src_buf_stride);
	/*16-byte alignment*/
	gdc_config.in_c_stride = AXI_WORD_ALIGN(param->vf_para->src_buf_stride);
	gdc_config.in_plane_num = param->vf_para->src_vf_plane_count;
	gdc_config.out_width = param->vf_para->dst_vf_width;
	gdc_config.out_height = param->vf_para->dst_vf_height;
	/*16-byte alignment*/
	gdc_config.out_y_stride = AXI_WORD_ALIGN(param->vf_para->dst_buf_stride);
	/*16-byte alignment*/
	gdc_config.out_c_stride = AXI_WORD_ALIGN(param->vf_para->dst_buf_stride);
	gdc_config.out_plane_num = param->vf_para->dst_vf_plane_count;
	gdc_config.in_paddr[0] = param->vf_para->src_buf_addr;
	gdc_config.in_paddr[1] = param->vf_para->src_buf_addr
				+ AXI_WORD_ALIGN(gdc_config.in_width)
				* AXI_WORD_ALIGN(gdc_config.in_height);
	gdc_config.out_paddr[0] = param->vf_para->dst_buf_addr;
	gdc_config.out_paddr[1] = param->vf_para->dst_buf_addr
				+ AXI_WORD_ALIGN(gdc_config.out_width)
				* AXI_WORD_ALIGN(gdc_config.out_height);
	gdc_config.config_paddr = param->fw_load.phys_addr;
	gdc_config.config_size = param->fw_load.size_32bit; /* in 32bit */
	gdc_config.use_sec_mem = 0; /* secure mem access */

	ret = gdc_process_phys(param->context, &gdc_config);
	if (ret < 0) {
		vc_print(param->vc_index, PRINT_ERROR,
			"%s: dewrap process failed.\n", __func__);
	} else {
		if (dewarp_com_dump != dump_num) {
			sprintf(dump_name, "/data/src_%d.yuv", dewarp_com_dump);
			src_vf_size = param->vf_para->src_vf_width *
				param->vf_para->src_vf_height * 3 / 2;
			dump_vframe(dump_name, param->vf_para->src_buf_addr, src_vf_size);

			sprintf(dump_name, "/data/dst_%d.yuv", dewarp_com_dump);
			dst_vf_size = param->vf_para->dst_vf_width *
				param->vf_para->dst_vf_height * 3 / 2;
			dump_vframe(dump_name, param->vf_para->dst_buf_addr, dst_vf_size);
			dump_num = dewarp_com_dump;
		}
	}

	return ret;
}
