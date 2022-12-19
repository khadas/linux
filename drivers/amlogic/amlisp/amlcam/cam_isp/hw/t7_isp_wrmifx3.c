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

static u32 isp_wrmifx3_bits_mode(struct aml_format *fmt, int pidx)
{
	u32 bits_mode = 1;

	switch (fmt->bpp * pidx) {
	case 8:
		bits_mode = 1;
	break;
	case 10:
		bits_mode = 7;
	break;
	case 12:
		bits_mode = 6;
	break;
	case 14:
		bits_mode = 8;
	break;
	case 16:
		bits_mode = 2;
	break;
	case 20:
		bits_mode = 10;
	break;
	case 24:
		bits_mode = 9;
	break;
	case 30:
		bits_mode = 11;
	break;
	case 32:
		bits_mode = 3;
	break;
	case 64:
		bits_mode = 4;
	break;
	case 128:
		bits_mode = 5;
	break;
	default:
		bits_mode = 1;
	break;
	}

	return bits_mode;
}

static u32 isp_wrmifx3_mtx_ibits(struct aml_format *fmt)
{
	u32 mtx_ibits = 0;

	switch (fmt->bpp) {
	case 8:
		mtx_ibits = 0;
	break;
	case 10:
		mtx_ibits = 1;
	break;
	case 12:
		mtx_ibits = 2;
	break;
	case 16:
		mtx_ibits = 3;
	break;
	}

	return mtx_ibits;
}

void isp_wrmifx3_mirror_enable(struct isp_dev_t *isp_dev, u32 idx, u32 enable)
{
	u32 raddr = 0;

	if (idx > 3) {
		pr_err("Error input wmif index\n");
		return;
	}

	raddr = ISP_WRMIFX3_0_CH0_CTRL0 + ((idx * 0x40) << 2);
	isp_reg_update_bits(isp_dev, raddr, enable, 4, 1);

	raddr = ISP_WRMIFX3_0_CH1_CTRL0 + ((idx * 0x40) << 2);
	isp_reg_update_bits(isp_dev, raddr, enable, 4, 1);

	raddr = ISP_WRMIFX3_0_CH2_CTRL0 + ((idx * 0x40) << 2);
	isp_reg_update_bits(isp_dev, raddr, enable, 4, 1);
}

void isp_wrmifx3_flip_enable(struct isp_dev_t *isp_dev, u32 idx, u32 enable)
{
	u32 raddr = 0;

	if (idx > 3) {
		pr_err("Error input wmif index\n");
		return;
	}

	raddr = ISP_WRMIFX3_0_CH0_CTRL0 + ((idx * 0x40) << 2);
	isp_reg_update_bits(isp_dev, raddr, enable, 5, 1);

	raddr = ISP_WRMIFX3_0_CH1_CTRL0 + ((idx * 0x40) << 2);
	isp_reg_update_bits(isp_dev, raddr, enable, 5, 1);

	raddr = ISP_WRMIFX3_0_CH2_CTRL0 + ((idx * 0x40) << 2);
	isp_reg_update_bits(isp_dev, raddr, enable, 5, 1);
}

void isp_wrmifx3_cfg_frm_size(struct isp_dev_t *isp_dev, u32 idx,
			struct aml_format *fmt)
{
	u32 val = 0;
	u32 raddr = 0;
	u32 bpp, stride;
	u32 width, height;
	u32 bits_node = 0;

	if (idx > 3) {
		pr_err("Error input wmif index\n");
		return;
	}

	bpp = fmt->bpp;
	width = fmt->width;
	height = fmt->height;

	raddr = ISP_WRMIFX3_0_FMT_SIZE + ((idx * 0x40) << 2);
	val = (height << 16) | (width << 0);
	isp_reg_write(isp_dev, raddr, val);

	raddr = ISP_WRMIFX3_0_FMT_CTRL + ((idx * 0x40) << 2);
	isp_reg_update_bits(isp_dev, raddr, fmt->fourcc, 16, 3);
	if (fmt->fourcc == AML_FMT_YUV422)
		isp_reg_update_bits(isp_dev, raddr, 1, 3, 1);
	else
		isp_reg_update_bits(isp_dev, raddr, 0, 3, 1);

	if (isp_dev->fmt.code == MEDIA_BUS_FMT_YUYV8_2X8)
		isp_reg_update_bits(isp_dev, raddr, 1, 2, 1);
	else
		isp_reg_update_bits(isp_dev, raddr, 0, 2, 1);

	isp_reg_update_bits(isp_dev, raddr, fmt->nplanes - 1, 4, 2);

	val = isp_wrmifx3_mtx_ibits(fmt);
	isp_reg_update_bits(isp_dev, raddr, val, 0, 2);

	raddr = ISP_WRMIFX3_0_WIN_LUMA_H + ((idx * 0x40) << 2);
	val = (0 << 0) | ((width - 1) << 16);
	isp_reg_write(isp_dev, raddr, val);

	raddr = ISP_WRMIFX3_0_WIN_LUMA_V + ((idx * 0x40) << 2);
	val = (0 << 0) | ((height - 1) << 16);
	isp_reg_write(isp_dev, raddr, val);

	raddr = ISP_WRMIFX3_0_WIN_CHROM_H + ((idx * 0x40) << 2);
	val = (0 << 0) | ((width - 1) << 16);
	isp_reg_write(isp_dev, raddr, val);

	raddr = ISP_WRMIFX3_0_WIN_CHROM_V + ((idx * 0x40) << 2);
	val = (0 << 0) | ((height - 1) << 16);
	isp_reg_write(isp_dev, raddr, val);

	raddr = ISP_WRMIFX3_0_CRP_HSIZE + ((idx * 0x40) << 2);
	val = (0 << 0) | ((width - 1) << 16);
	isp_reg_write(isp_dev, raddr, val);

	raddr = ISP_WRMIFX3_0_CRP_VSIZE + ((idx * 0x40) << 2);
	val = (0 << 0) | ((height - 1) << 16);
	isp_reg_write(isp_dev, raddr, val);

	raddr = ISP_WRMIFX3_0_CH0_BADDR_OFST + ((idx * 0x40) << 2);
	val = fmt->size >> 4;
	isp_reg_write(isp_dev, raddr, val);

	raddr = ISP_WRMIFX3_0_CH1_BADDR_OFST + ((idx * 0x40) << 2);
	val = fmt->size >> 4;
	isp_reg_write(isp_dev, raddr, val);

	stride = (width * bpp + 127) >> 7;

	switch (fmt->nplanes) {
	case 2:
		raddr = ISP_WRMIFX3_0_CH1_CTRL0 + ((idx * 0x40) << 2);
		isp_reg_update_bits(isp_dev, raddr, stride, 16, 13);

		if (fmt->fourcc == AML_FMT_HDR_RAW)
			bits_node = isp_wrmifx3_bits_mode(fmt, 1);
		else
			bits_node = isp_wrmifx3_bits_mode(fmt, 2);
		raddr = ISP_WRMIFX3_0_CH1_CTRL1 + ((idx * 0x40) << 2);
		isp_reg_update_bits(isp_dev, raddr, bits_node, 27, 4);
		isp_reg_update_bits(isp_dev, raddr, 0, 24, 3);

		if (fmt->fourcc == AML_FMT_HDR_RAW) {
			raddr = ISP_WRMIFX3_0_WIN_CHROM_H + ((idx * 0x40) << 2);
			val = (0 << 0) | ((width - 1) << 16);
			isp_reg_write(isp_dev, raddr, val);

			raddr = ISP_WRMIFX3_0_WIN_CHROM_V + ((idx * 0x40) << 2);
			val = (0 << 0) | ((height - 1) << 16);
			isp_reg_write(isp_dev, raddr, val);
		} else {
			raddr = ISP_WRMIFX3_0_WIN_CHROM_H + ((idx * 0x40) << 2);
			val = (0 << 0) | (((width >> 1) - 1) << 16);
			isp_reg_write(isp_dev, raddr, val);

			raddr = ISP_WRMIFX3_0_WIN_CHROM_V + ((idx * 0x40) << 2);
			val = (0 << 0) | (((height >> 1) - 1) << 16);

			isp_reg_write(isp_dev, raddr, val);
		}
	case 1:
		raddr = ISP_WRMIFX3_0_CH0_CTRL0 + ((idx * 0x40) << 2);
		isp_reg_update_bits(isp_dev, raddr, stride, 16, 13);

		bits_node = isp_wrmifx3_bits_mode(fmt, 1);
		raddr = ISP_WRMIFX3_0_CH0_CTRL1 + ((idx * 0x40) << 2);
		isp_reg_update_bits(isp_dev, raddr, bits_node, 27, 4);
		isp_reg_update_bits(isp_dev, raddr, 0, 24, 3);
	break;
	default:
		pr_err("Error input stream index\n");
	break;
	}
}

void isp_wrmifx3_cfg_frm_buff(struct isp_dev_t *isp_dev, u32 idx,
			struct aml_buffer *buff)
{
	u32 paddr = 0;
	u32 raddr = 0;
	struct isp_global_info *g_info = isp_global_get_info();

	if (idx > 3) {
		pr_err("Error input wmif index\n");
		return;
	}

	switch(buff->nplanes) {
	case 2:
		paddr = buff->addr[AML_PLANE_B] >> 4;
		raddr = ISP_WRMIFX3_0_CH1_BADDR + ((idx * 0x40) << 2);
		if (g_info->mode == AML_ISP_SCAM)
			isp_hwreg_write(isp_dev, raddr, paddr);
		else
			isp_reg_write(isp_dev, raddr, paddr);
	case 1:
		paddr = buff->addr[AML_PLANE_A] >> 4;
		raddr = ISP_WRMIFX3_0_CH0_BADDR + ((idx * 0x40) << 2);
		if (g_info->mode == AML_ISP_SCAM)
			isp_hwreg_write(isp_dev, raddr, paddr);
		else
			isp_reg_write(isp_dev, raddr, paddr);
	break;
	default:
		pr_err("Error input stream index\n");
	break;
	}
}

void isp_wrmifx3_cfg_slice(struct isp_dev_t *isp_dev, u32 idx, struct aml_slice *param)
{
	u32 fourcc = 2;
	u32 addr, val;
	u32 start, end;
	int left_hsize, right_hsize;
	int left_ovlp, right_ovlp;

	left_hsize = param->left_hsize;
	left_ovlp = param->left_ovlp;
	right_hsize = param->right_hsize;
	right_ovlp = param->right_ovlp;

	isp_reg_read_bits(isp_dev, ISP_WRMIFX3_0_FMT_CTRL, &fourcc, 16, 3);

	if (param->pos == 0) {
		left_hsize = param->left_hsize;
		left_ovlp = param->left_ovlp;
		addr = ISP_WRMIFX3_0_CRP_HSIZE + ((idx * 0x40) << 2);
		start = 0;
		end = left_hsize - left_ovlp -1;
		val = (start << 0) | (end << 16);
		isp_hwreg_write(isp_dev, addr, val);

		addr = ISP_WRMIFX3_0_WIN_LUMA_H + ((idx * 0x40) << 2);
		start = 0;
		end = left_hsize - left_ovlp -1;
		val = (start << 0) | (end << 16);
		isp_hwreg_write(isp_dev, addr, val);

		addr = ISP_WRMIFX3_0_WIN_CHROM_H + ((idx * 0x40) << 2);
		if (fourcc == AML_FMT_YUV420) {
			start = 0;
			end = (left_hsize - left_ovlp) / 2 -1;
		} else {
			start = 0;
			end = left_hsize - left_ovlp -1;
		}
		val = (start << 0) | (end << 16);
		isp_hwreg_write(isp_dev, addr, val);

		addr = ISP_WRMIFX3_0_FMT_SIZE + ((idx * 0x40) << 2);
		if (fourcc == AML_FMT_HDR_RAW || fourcc == AML_FMT_RAW) {
			isp_reg_read_bits(isp_dev, ISP_TOP_INPUT_SIZE, &val, 16, 16);
		} else {
			val = left_hsize;
		}
		isp_hwreg_update_bits(isp_dev, addr, val, 0, 16);

		addr = ISP_WRMIFX3_0_CRP_CTR + ((idx * 0x40) << 2);
		isp_hwreg_update_bits(isp_dev, addr, 1, 0, 1);
	} else if (param->pos == 1) {
		addr = ISP_WRMIFX3_0_CRP_HSIZE + ((idx * 0x40) << 2);
		if (fourcc == AML_FMT_HDR_RAW || fourcc == AML_FMT_RAW) {
			isp_reg_read_bits(isp_dev, ISP_TOP_INPUT_SIZE, &val, 16, 16);
			start = val - param->whole_frame_hcenter;
			end = val - 1;
		} else {
			start = right_ovlp;
			end = right_hsize -1;
		}
		val = (start << 0) | (end << 16);
		isp_hwreg_write(isp_dev, addr, val);

		addr = ISP_WRMIFX3_0_WIN_LUMA_H + ((idx * 0x40) << 2);
		start = left_hsize - left_ovlp;
		end = (right_hsize - right_ovlp) * 2 -1;
		val = (start << 0) | (end << 16);
		isp_hwreg_write(isp_dev, addr, val);

		addr = ISP_WRMIFX3_0_WIN_CHROM_H + ((idx * 0x40) << 2);
		if (fourcc == AML_FMT_YUV420) {
			start = (left_hsize - left_ovlp) >> 1;
			end = right_hsize - right_ovlp -1;
		} else {
			start = left_hsize - left_ovlp;
			end = (right_hsize - right_ovlp) * 2 -1;
		}
		val = (start << 0) | (end << 16);
		isp_hwreg_write(isp_dev, addr, val);

		addr = ISP_WRMIFX3_0_FMT_SIZE + ((idx * 0x40) << 2);
		if (fourcc == AML_FMT_HDR_RAW) {
			isp_reg_read_bits(isp_dev, ISP_TOP_INPUT_SIZE, &val, 16, 16);
		} else {
			val = right_hsize;
		}
		isp_hwreg_update_bits(isp_dev, addr, val, 0, 16);

		addr = ISP_WRMIFX3_0_CRP_CTR + ((idx * 0x40) << 2);
		isp_hwreg_update_bits(isp_dev, addr, 1, 0, 1);
	}
}

void isp_wrmifx3_autoacc_num(struct isp_dev_t *isp_dev, u32 idx, u32 num)
{
	if (idx > 2) {
		pr_err("Error input wrmif index\n");
		return;
	}

	isp_reg_update_bits(isp_dev,
			ISP_WRMIFX3_0_BADDR_OFSTEN + ((idx * 0x40) << 2),
			num & 0xff,
			0, 8);
}

void isp_wrmifx3_autoacc_enable(struct isp_dev_t *isp_dev, u32 idx, u32 enable)
{
	if (idx > 2) {
		pr_err("Error input wrmif index\n");
		return;
	}

	isp_reg_update_bits(isp_dev,
			ISP_WRMIFX3_0_BADDR_OFSTEN + ((idx * 0x40) << 2),
			enable & 0x1,
			8, 1);
}

void isp_wrmifx3_module_enable(struct isp_dev_t *isp_dev, u32 idx, u32 enable, u32 force)
{
	if (idx > 3) {
		pr_err("Error input wmif index\n");
		return;
	}

	if (force)
		isp_hwreg_update_bits(isp_dev, ISP_TOP_PATH_EN, enable & 0x1, idx + 8, 1);

	isp_reg_update_bits(isp_dev, ISP_TOP_PATH_EN, enable & 0x1, idx + 8, 1);
}

void isp_wrmifx3_module_stat(struct isp_dev_t *isp_dev, u32 idx, u32* val)
{
	if (idx > 3) {
		pr_err("Error input wmif index\n");
		return;
	}

	isp_reg_read_bits(isp_dev, ISP_TOP_PATH_EN, val, idx + 8, 1);
}

void isp_wrmifx3_init(struct isp_dev_t *isp_dev)
{

}
