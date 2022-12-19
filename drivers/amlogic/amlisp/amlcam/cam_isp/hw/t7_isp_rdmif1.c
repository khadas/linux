/* SPDX-License-Identifier: GPL-2.0
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

#include "aml_isp_reg.h"
#include "aml_isp_hw.h"

struct rdmif1_param_t {
	u32 xend0;
	u32 xend1;
	u32 yend0;
	u32 yend1;
	u32 stride0;
	u32 stride1;
	u32 bits_mode;
	u32 cb_en;
	u32 color_map;
	u32 demux_mode;
	u32 byte_per_pix;
	u32 fmt_hen;
	u32 fmt_ven;
	u32 fmt_y_hsize;
	u32 fmt_u_hsize;
	u32 chro_rpt_lastl;
	u32 fmt_yc_ratio;
};

static void rdmif1_cfg_reg_param(struct isp_dev_t *isp_dev, struct rdmif1_param_t *param)
{
	u32 val = 0;
	u32 xend0 = param->xend0;
	u32 xend1 = param->xend1;
	u32 yend0 = param->yend0;
	u32 yend1 = param->yend1;
	u32 stride0 = param->stride0;
	u32 stride1 = param->stride1;
	u32 bits_mode = param->bits_mode;
	u32 cb_en = param->cb_en;
	u32 color_map = param->color_map;
	u32 demux_mode = param->demux_mode;
	u32 byte_per_pix = param->byte_per_pix;
	u32 fmt_hen = param->fmt_hen;
	u32 fmt_ven = param->fmt_ven;
	u32 fmt_y_hsize = param->fmt_y_hsize;
	u32 fmt_u_hsize = param->fmt_u_hsize;
	u32 chro_rpt_lastl = param->chro_rpt_lastl;
	u32 fmt_yc_ratio = param->fmt_yc_ratio;

	val = (xend0 << 16) | (0 << 0);
	isp_reg_write(isp_dev, ISP_RDMIF1_LUMA_X0, val);

	val = (yend0 << 16) | (0 << 0);
	isp_reg_write(isp_dev, ISP_RDMIF1_LUMA_Y0, val);

	val = (xend1 << 16) | (0 << 0);
	isp_reg_write(isp_dev, ISP_RDMIF1_CHROMA_X0, val);

	val = (yend1 << 16) | (0 << 0);
	isp_reg_write(isp_dev, ISP_RDMIF1_CHROMA_Y0, val);

	val = (stride1 << 16) | (stride0 << 0);
	isp_reg_write(isp_dev, ISP_RDMIF1_STRIDE_0, val);

	val = (0 << 28) | (0 << 27) | (0 << 26) | (1 << 25) |
		(0 << 19) | (1 << 18) | (demux_mode << 16) | (byte_per_pix << 14) |
		(chro_rpt_lastl << 6) | (0 << 4) | (0 << 3) | (0 << 2) |
		(cb_en << 1) | (1 << 0);
	isp_reg_write(isp_dev, ISP_RDMIF1_GEN_REG, val);

	isp_reg_update_bits(isp_dev, ISP_RDMIF1_GEN_REG2, color_map, 0, 2);

	isp_reg_update_bits(isp_dev, ISP_RDMIF1_GEN_REG3, bits_mode, 8, 2);

	fmt_yc_ratio = (demux_mode == 0) ? 1 : 0;
	val = (0 << 31) | (0 << 30) | (1 << 28) | (0 << 24) | (0 << 23) |
		(fmt_yc_ratio << 21) | (fmt_hen << 20) | (1 << 19) | (0 << 18) |
		(1 << 17) | (0 << 16) | (0 << 12) | (0 << 8) | (8 << 1) | (fmt_ven << 0);
	isp_reg_write(isp_dev, ISP_RDMIF1_CFMT_CTRL, val);

	isp_reg_update_bits(isp_dev, ISP_RDMIF1_CFMT_W, fmt_u_hsize, 0, 13);
	isp_reg_update_bits(isp_dev, ISP_RDMIF1_CFMT_W, fmt_y_hsize, 16, 13);
}

static void rdmif1_cfg_yuv444_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 xsize = 0;
	u32 ysize = 0;
	struct rdmif1_param_t param;

	xsize = fmt->width;
	ysize = fmt->height;

	memset(&param, 0, sizeof(param));

	param.xend0 = xsize - 1;
	param.xend1 = xsize - 1;
	param.yend0 = ysize - 1;
	param.yend1 = ysize - 1;

	param.fmt_y_hsize = xsize;
	param.fmt_u_hsize = xsize;

	switch (fmt->bpp) {
	case 8:
		if (fmt->nplanes == 1) {
			param.stride0 = (xsize * 24 + 127) >> 7;
			param.stride1 = 0;
			param.bits_mode = 0;
			param.byte_per_pix = 2;
		} else {
			param.stride0 = (xsize * 8 + 127) >> 7;
			param.stride1 = (xsize * 16 + 127) >> 7;
			param.bits_mode = 0;
			param.byte_per_pix = 0;
			param.cb_en = 1;
			param.color_map = 1;
		}
	break;
	case 10:
		if (fmt->nplanes == 1) {
			param.stride0 = (xsize * 32 + 127) >> 7;
			param.stride1 = 0;
			param.bits_mode = 2;
			param.byte_per_pix = 2;
		} else {
			param.stride0 = (xsize * 12 + 127) >> 7;
			param.stride1 = (xsize * 20 + 127) >> 7;
			param.bits_mode = 3;
			param.byte_per_pix = 0;
			param.cb_en = 1;
			param.color_map = 1;
		}
	break;
	}

	param.demux_mode = 1;
	param.fmt_yc_ratio = (param.demux_mode == 0) ? 1 : 0;

	rdmif1_cfg_reg_param(isp_dev, &param);
}

static void rdmif1_cfg_yuv422_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 xsize = 0;
	u32 ysize = 0;
	struct rdmif1_param_t param;

	xsize = fmt->width;
	ysize = fmt->height;

	memset(&param, 0, sizeof(param));

	param.xend0 = xsize - 1;
	param.yend0 = ysize - 1;

	param.xend1 = xsize / 2- 1;
	param.yend1 = ysize - 1;

	param.fmt_y_hsize = xsize;
	param.fmt_u_hsize = xsize;

	switch (fmt->bpp) {
	case 8:
		if (fmt->nplanes == 1) {
			param.stride0 = (xsize * 16 + 127) >> 7;
			param.stride1 = 0;
			param.bits_mode = 0;
			param.fmt_hen = 1;
			param.byte_per_pix = 1;
		} else {
			param.stride0 = (xsize * 8 + 127) >> 7;
			param.stride1 = (xsize / 2 * 16 + 127) >> 7;
			param.bits_mode = 0;
			param.fmt_hen = 1;
			param.byte_per_pix = 0;
			param.cb_en = 1;
			param.color_map = 1;
		}
	break;
	case 10:
		if (fmt->nplanes == 1) {
			param.stride0 = (xsize * 20 + 127) >> 7;
			param.stride1 = 0;
			param.bits_mode = 3;
			param.fmt_hen = 1;
			param.byte_per_pix = 1;
		} else {
			param.stride0 = (xsize * 12 + 127) >> 7;
			param.stride1 = (xsize / 2 * 20 + 127) >> 7;
			param.bits_mode = 3;
			param.fmt_hen = 1;
			param.byte_per_pix = 0;
			param.cb_en = 1;
			param.color_map = 1;
		}
	break;
	}

	param.demux_mode = 0;
	param.fmt_u_hsize = xsize / 2;

	param.fmt_yc_ratio = (param.demux_mode == 0) ? 1 : 0;

	rdmif1_cfg_reg_param(isp_dev, &param);
}

static void rdmif1_cfg_yuv420_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 xsize = 0;
	u32 ysize = 0;
	struct rdmif1_param_t param;

	if (fmt->nplanes == 1) {
		pr_err("Error to support palnes\n");
		return;
	}

	xsize = fmt->width;
	ysize = fmt->height;

	memset(&param, 0, sizeof(param));

	param.xend0 = xsize - 1;
	param.yend0 = ysize - 1;

	param.xend1 = xsize / 2 - 1;
	param.yend1 = ysize / 2 - 1;

	param.fmt_y_hsize = xsize;
	param.fmt_u_hsize = xsize;

	switch (fmt->bpp) {
	case 8:
		param.stride0 = (xsize * 8 + 127) >> 7;
		param.stride1 = (xsize / 2 * 16 + 127) >> 7;
		param.bits_mode = 0;
		param.byte_per_pix = 0;
		param.fmt_hen = 1;
		param.fmt_ven = 1;
		param.cb_en = 1;
		param.color_map = 1;
	break;
	case 10:
		param.stride0 = (xsize * 12 + 127) >> 7;
		param.stride1 = (xsize / 2 * 20 + 127) >> 7;
		param.bits_mode = 3;
		param.byte_per_pix = 0;
		param.fmt_hen = 1;
		param.fmt_ven = 1;
		param.cb_en = 1;
		param.color_map = 1;
	break;
	}

	param.demux_mode = 0;
	param.fmt_u_hsize = xsize / 2;
	param.chro_rpt_lastl = 1;

	param.fmt_yc_ratio = (param.demux_mode == 0) ? 1 : 0;

	rdmif1_cfg_reg_param(isp_dev, &param);
}

static void rdmif1_cfg_yuv400_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 xsize = 0;
	u32 ysize = 0;
	struct rdmif1_param_t param;

	if (fmt->nplanes != 1) {
		pr_err("Error to support palnes\n");
		return;
	}

	xsize = fmt->width;
	ysize = fmt->height;

	memset(&param, 0, sizeof(param));

	param.xend0 = xsize - 1;
	param.yend0 = ysize - 1;

	param.xend1 = 0;
	param.yend1 = 0;

	param.fmt_y_hsize = xsize;
	param.fmt_u_hsize = xsize;

	switch (fmt->bpp) {
	case 8:
		param.stride0 = (xsize * 8 + 127) >> 7;
		param.stride1 = 0;
		param.bits_mode = 0;
		param.byte_per_pix = 0;
		param.fmt_hen = 0;
		param.fmt_ven = 0;
		param.cb_en = 0;
		param.color_map = 0;
	break;
	case 10:
		param.stride0 = (xsize * 12 + 127) >> 7;
		param.stride1 = 0 ;
		param.bits_mode = 3;
		param.byte_per_pix = 0;
		param.fmt_hen = 0;
		param.fmt_ven = 0;
		param.cb_en = 0;
		param.color_map = 0;
	break;
	}

	param.demux_mode = 1;
	param.fmt_yc_ratio = (param.demux_mode == 0) ? 1 : 0;

	rdmif1_cfg_reg_param(isp_dev, &param);
}

void isp_rdmif1_cfg_frm_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	switch (fmt->fourcc) {
	case 0:
		rdmif1_cfg_yuv444_size(isp_dev, fmt);
	break;
	case 1:
		rdmif1_cfg_yuv422_size(isp_dev, fmt);
	break;
	case 2:
		rdmif1_cfg_yuv420_size(isp_dev, fmt);
	break;
	case 3:
		rdmif1_cfg_yuv400_size(isp_dev, fmt);
	break;
	}
}

void isp_rdmif1_cfg_frm_buff(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	u32 paddr = 0;

	switch (buff->nplanes) {
	case 3:
		paddr = buff->addr[AML_PLANE_C];
		isp_reg_write(isp_dev, ISP_RDMIF1_BADDR_CR, paddr >> 4);
	case 2:
		paddr = buff->addr[AML_PLANE_B];
		isp_reg_write(isp_dev, ISP_RDMIF1_BADDR_CB, paddr >> 4);
	case 1:
		paddr = buff->addr[AML_PLANE_A];
		isp_reg_write(isp_dev, ISP_RDMIF1_BADDR_Y, paddr >> 4);
	break;
	default:
	break;
	}
}

void isp_rdmif1_module_enable(struct isp_dev_t *isp_dev, u32 enable)
{
	if (enable) {
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_EN, 1, 17, 2);
	} else {
		isp_reg_update_bits(isp_dev, ISP_TOP_PATH_EN, 0, 17, 2);
	}
}

void isp_rdmif1_init(struct isp_dev_t *isp_dev)
{
	return;
}
