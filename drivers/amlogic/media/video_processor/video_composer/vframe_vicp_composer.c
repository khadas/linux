// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/utils/amlog.h>
#include <linux/amlogic/media/vicp/vicp.h>
#include "vfq.h"
#include <linux/amlogic/cpu_version.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include "vc_util.h"
#include "vframe_vicp_composer.h"

bool is_vicp_supported(void)
{
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_S5)
		return true;
	else
		return false;
}

enum vicp_rotation_mode_e map_rotationmode_from_vc_to_vicp(int rotation_vc)
{
	enum vicp_rotation_mode_e rotation = VICP_ROTATION_MAX;

	if (rotation_vc == 1)
		rotation = VICP_ROTATION_MIRROR_H;
	else if (rotation_vc == 2)
		rotation = VICP_ROTATION_MIRROR_V;
	else if (rotation_vc == 3)
		rotation = VICP_ROTATION_180;
	else if (rotation_vc == 4)
		rotation = VICP_ROTATION_90;
	else if (rotation_vc == 7)
		rotation = VICP_ROTATION_270;
	else
		rotation = VICP_ROTATION_0;

	return rotation;
}

int config_vicp_input_data(struct vframe_s *vf, ulong addr, int stride, int width, int height,
	int endian, int color_fmt, int color_depth, struct input_data_param_t *input_data)
{
	struct dma_data_config_t dma_data;

	if (IS_ERR_OR_NULL(input_data)) {
		pr_info("%s: NULL param, please check.\n", __func__);
		return -1;
	}

	memset(input_data, 0, sizeof(struct input_data_param_t));
	if (vf) {
		input_data->is_vframe = true;
		if (vf->canvas0_config[0].phy_addr == 0) {
			if ((vf->flag &  VFRAME_FLAG_DOUBLE_FRAM) && vf->vf_ext) {
				input_data->data_vf = vf->vf_ext;
			} else {
				pr_info("%s: vf no yuv data, composer fail\n", __func__);
				return -1;
			}
		} else {
			input_data->data_vf = vf;
		}
	} else {
		input_data->is_vframe = false;

		memset(&dma_data, 0, sizeof(struct dma_data_config_t));
		dma_data.buf_addr = addr;
		dma_data.buf_stride_w = stride;
		dma_data.buf_stride_h = stride;
		dma_data.data_width = width;
		dma_data.data_height = height;
		dma_data.color_format = color_fmt;
		dma_data.color_depth = color_depth;
		dma_data.plane_count = 2;
		dma_data.endian = endian;
		dma_data.need_swap_cbcr = 0;

		input_data->data_dma = &dma_data;
	}

	return 0;
}

int config_vicp_output_data(int fbc_out_en, int mif_out_en, ulong *phy_addr, int stride,
	int width, int height, int endian, enum vicp_color_format_e cfmt_mif, int cdep_mif,
	enum vicp_color_format_e cfmt_fbc, int cdep_fbc, int init_ctrl, int pip_mode,
	struct output_data_param_t *output_data)
{
	if (IS_ERR_OR_NULL(output_data)) {
		pr_info("%s: NULL param, please check.\n", __func__);
		return -1;
	}
	memset(output_data, 0, sizeof(struct output_data_param_t));
	output_data->width = width;
	output_data->height = height;
	output_data->phy_addr[0] = phy_addr[0];
	output_data->stride[0] = stride;
	output_data->mif_out_en = mif_out_en;
	output_data->endian = endian;
	output_data->need_swap_cbcr = 1;
	if (mif_out_en) {
		output_data->mif_color_fmt = cfmt_mif;
		output_data->mif_color_dep = cdep_mif;
	}

	output_data->fbc_out_en = fbc_out_en;
	if (fbc_out_en) {
		output_data->fbc_color_fmt = cfmt_fbc;
		output_data->fbc_color_dep = cdep_fbc;
		output_data->phy_addr[1] = phy_addr[1];
		output_data->phy_addr[2] = phy_addr[2];
		output_data->fbc_init_ctrl = init_ctrl;
		output_data->fbc_pip_mode = pip_mode;
	}

	return 0;
}

int vicp_data_composer(struct vicp_data_config_t *data_config)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(data_config)) {
		pr_info("%s: NULL param, please check.\n", __func__);
		return -1;
	}

	ret = vicp_process(data_config);
	if (ret < 0)
		pr_info("%s: vicp_process failed.\n", __func__);

	return ret;
}
