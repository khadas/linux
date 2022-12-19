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
#include "aml_isp_hw.h"
#include "aml_isp_reg.h"
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

void isp_hw_lut_wstart(struct isp_dev_t *isp_dev, u32 type)
{
	isp_dev->lutWr.lutSeq = LUT_WSTART;

	if (isp_dev->lutWr.lutRegW[type] == 0) {
		isp_dev->lutWr.lutRegW[type] = 1;
		isp_dev->lutWr.lutRegAddr[type] = isp_dev->wreg_cnt;
	} else {
		isp_dev->wreg_cnt = isp_dev->lutWr.lutRegAddr[type];
	}
}

void isp_hw_lut_wend(struct isp_dev_t *isp_dev)
{
	isp_dev->lutWr.lutSeq = LUT_WEND;
}

u32 isp_reg_read(struct isp_dev_t *isp_dev, u32 addr)
{
	u32 val = 0;
	struct aml_reg *rregs = isp_dev->rreg_buff.vaddr[AML_PLANE_A];

	if (isp_dev->apb_dma == 0)
		val = isp_hwreg_read(isp_dev, addr);
	else if (isp_dev->apb_dma == 1)
		val = rregs[(addr - ISP_BASE) >> 2].val;

	return val;
}

void isp_reg_write(struct isp_dev_t *isp_dev, u32 addr, u32 val)
{
	u32 i = 0;
	unsigned long flags = 0;
	struct aml_reg *wregs = isp_dev->wreg_buff.vaddr[AML_PLANE_A];
	struct aml_reg *rregs = isp_dev->rreg_buff.vaddr[AML_PLANE_A];
	struct isp_global_info *g_info = isp_global_get_info();

	if (isp_dev->apb_dma == 0) {
		isp_hwreg_write(isp_dev, addr, val);
	} else if (isp_dev->apb_dma == 1) {
		spin_lock_irqsave(&isp_dev->wreg_lock, flags);

		rregs[(addr - ISP_BASE) >> 2].val = val;

		if (g_info->mode == AML_ISP_SCAM) {
			wregs[isp_dev->twreg_cnt].addr = isp_dev->phy_base + addr;
			wregs[isp_dev->twreg_cnt].val = val;
			isp_dev->wreg_cnt ++;
			isp_dev->twreg_cnt ++;
		} else {
			if (isp_dev->isp_status == STATUS_STOP) {
				wregs[isp_dev->wreg_cnt].addr = isp_dev->phy_base + addr;
				wregs[isp_dev->wreg_cnt].val = val;
				isp_dev->wreg_cnt ++;
				isp_dev->fwreg_cnt ++;
				isp_dev->twreg_cnt ++;
			} else {
				if (isp_dev->lutWr.lutSeq == LUT_WSTART) {
					wregs[isp_dev->wreg_cnt].addr = isp_dev->phy_base + addr;
					wregs[isp_dev->wreg_cnt].val = val;
					isp_dev->wreg_cnt ++;

					if (isp_dev->wreg_cnt > isp_dev->twreg_cnt)
						isp_dev->twreg_cnt ++;

					spin_unlock_irqrestore(&isp_dev->wreg_lock, flags);
					return;
				}

				for (i = isp_dev->fwreg_cnt; i < isp_dev->twreg_cnt; i ++) {
					if (wregs[i].addr == (isp_dev->phy_base + addr)) {
						wregs[i].addr = isp_dev->phy_base + addr;
						wregs[i].val = val;
						break;
					}
				}

				if (i == isp_dev->twreg_cnt) {
					wregs[isp_dev->twreg_cnt].addr = isp_dev->phy_base + addr;
					wregs[isp_dev->twreg_cnt].val = val;
					isp_dev->twreg_cnt ++;
				}
			}
		}

		spin_unlock_irqrestore(&isp_dev->wreg_lock, flags);
	}
}

int isp_reg_update_bits(struct isp_dev_t *isp_dev, u32 addr, u32 val, u32 start, u32 len)
{
	int rtn = -1;
	u32 mask = 0;
	u32 orig = 0;
	u32 temp = 0;

	if (start + len > 32) {
		dump_stack();
		pr_err("ISP: Error input start and len\n");
		return rtn;
	} else if (start == 0 && len == 32) {
		isp_reg_write(isp_dev, addr, val);
		return 0;
	}

	orig = isp_reg_read(isp_dev, addr);

	mask = ((1 << len) - 1) << start;
	val = (val << start);

	temp = orig & (~mask);
	temp |= val & mask;

	isp_reg_write(isp_dev, addr, temp);

	return 0;
}

int isp_hwreg_update_bits(struct isp_dev_t *isp_dev, u32 addr, u32 val, u32 start, u32 len)
{
	int rtn = -1;
	u32 mask = 0;
	u32 orig = 0;
	u32 temp = 0;

	if (start + len > 32) {
		dump_stack();
		pr_err("ISP: Error input start and len\n");
		return rtn;
	} else if (start == 0 && len == 32) {
		isp_hwreg_write(isp_dev, addr, val);
		return 0;
	}

	orig = isp_hwreg_read(isp_dev, addr);

	mask = ((1 << len) - 1) << start;
	val = (val << start);

	temp = orig & (~mask);
	temp |= val & mask;

	isp_hwreg_write(isp_dev, addr, temp);

	return 0;
}

int isp_reg_read_bits(struct isp_dev_t *isp_dev, u32 addr, u32 *val, u32 start, u32 len)
{
	int rtn = -1;
	u32 mask = 0;
	u32 orig = 0;

	if (start + len > 32 || !val) {
		dump_stack();
		pr_err("ISP: Error input start and len\n");
		return rtn;
	} else if (start == 0 && len == 32) {
		*val = isp_reg_read(isp_dev, addr);
		return 0;
	}

	mask = (1 << len) -1;
	orig = isp_reg_read(isp_dev, addr);

	*val = (orig >> start) & mask;

	return 0;
}

u32 isp_hw_float_convert(u32 data)
{
	u32 shift = (1<<31);
	int exp = 31 -12 + 1;

	if (data < (1 << 12))
		return data;

	for (exp = 31 -12 + 1; exp > 0; exp--) {
		if (data & shift)
			break;

		shift = (shift>>1);
	}

	return ((exp << 12) | (data >> exp));
}

int isp_hw_convert_fmt(struct aml_format *fmt)
{
	int isp_fmt = 0;

	if ((fmt->code == MEDIA_BUS_FMT_YVYU8_2X8) || (fmt->code == MEDIA_BUS_FMT_YUYV8_2X8))
		return isp_fmt;

	switch (fmt->code) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SBGGR14_1X14:
		isp_fmt = ISP_FMT_BGGR;
	break;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		isp_fmt = ISP_FMT_RGGB;
	break;
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
		isp_fmt = ISP_FMT_GRBG;
	break;
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
		isp_fmt = ISP_FMT_GBRG;
	break;
	default:
		pr_err("ISP: Error input code\n");
		isp_fmt = ISP_FMT_RGGB;
	}

	return isp_fmt;
}

static void isp_hw_init(struct isp_dev_t *isp_dev)
{
	isp_top_init(isp_dev);
	isp_intf_top_init(isp_dev);
	isp_disp_init(isp_dev);
	isp_ofe_init(isp_dev);
	isp_patgen_init(isp_dev);
	isp_dpc_init(isp_dev);
	isp_fed_init(isp_dev);
	isp_3a_flkr_init(isp_dev);
	isp_nr_cac_init(isp_dev);
	isp_lens_init(isp_dev);
	isp_gtm_init(isp_dev);
	isp_dms_init(isp_dev);
	isp_tnr_init(isp_dev);
	isp_snr_init(isp_dev);
	isp_cnr_init(isp_dev);
	isp_pk_init(isp_dev);
	isp_ltm_init(isp_dev);
	isp_post_pg2_ctrst_init(isp_dev);
	isp_post_cm2_init(isp_dev);
	isp_cmpr_raw_init(isp_dev);
	isp_post_pg0_top_init(isp_dev);

	isp_mcnr_mif_init(isp_dev);
	isp_ptnr_mif_init(isp_dev);
	isp_wrmifx3_init(isp_dev);
	isp_rdmif0_init(isp_dev);
	isp_ofe_wdr_init(isp_dev);

	isp_apb_dma_init(isp_dev);

	pr_info("ISP%u: hw init\n", isp_dev->index);
}

static void isp_hw_reset(struct isp_dev_t *isp_dev)
{
	isp_top_reset(isp_dev);
	isp_top_decmpr_disable(isp_dev);

	pr_info("ISP%u: hw reset\n", isp_dev->index);

	return;
}

static u32 isp_hw_version(struct isp_dev_t *isp_dev)
{
	u32 version = 0xcafebeef;

	return version;
}

static u32 isp_hw_interrupt_status(struct isp_dev_t *isp_dev)
{
	u32 status = 0;

	status = isp_top_irq_stat(isp_dev);

	pr_debug("ISP%u: irq status 0x%x\n", isp_dev->index, status);

	return status;
}

static int isp_hw_set_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 i = 0;

	isp_ofe_cfg_fmt(isp_dev, fmt);
	isp_dpc_cfg_fmt(isp_dev, fmt);
	isp_lens_cfg_fmt(isp_dev, fmt);
	isp_fed_cfg_fmt(isp_dev, fmt);
	isp_3a_flkr_cfg_fmt(isp_dev, fmt);
	isp_nr_cac_cfg_fmt(isp_dev, fmt);
	isp_cnr_cfg_fmt(isp_dev, fmt);
	isp_dms_cfg_fmt(isp_dev, fmt);
	isp_snr_cfg_fmt(isp_dev, fmt);
	isp_ltm_cfg_fmt(isp_dev, fmt);
	isp_ofe_wdr_cfg_fmt(isp_dev, fmt);
	isp_mcnr_cfg_fmt(isp_dev, fmt);

	isp_3a_flkr_cfg_size(isp_dev, fmt);
	isp_nr_cac_cfg_size(isp_dev, fmt);
	isp_ofe_cfg_size(isp_dev, fmt);
	isp_lens_cfg_size(isp_dev, fmt);

	isp_post_cm2_cfg_size(isp_dev, fmt);
	isp_ltm_cfg_size(isp_dev, fmt);
	isp_post_pg2_ctrst_cfg_size(isp_dev, fmt);
	isp_intf_top_cfg_size(isp_dev, fmt);
	isp_ptnr_mif_cfg_size(isp_dev, fmt);
	isp_mcnr_mif_cfg_size(isp_dev, fmt);

	isp_dms_cfg_size(isp_dev, fmt);

	isp_lens_cfg_ofst(isp_dev);
	isp_fed_cfg_ofst(isp_dev);

	isp_patgen_disable(isp_dev);
	for (i = AML_ISP_STREAM_0; i < AML_ISP_STREAM_RAW; i++) {
		isp_dev->video[i].acrop.hstart = 0;
		isp_dev->video[i].acrop.vstart = 0;
		isp_dev->video[i].acrop.hsize = fmt->width;
		isp_dev->video[i].acrop.vsize = fmt->height;

		isp_disp_set_input_size(isp_dev, i - 3, fmt);
		isp_disp_set_crop_size(isp_dev, i - 3, &isp_dev->video[i].acrop);
	}

	return 0;
}

static int isp_hw_set_input_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 i = 0;

	isp_top_cfg_fmt(isp_dev, fmt);

	isp_hw_set_fmt(isp_dev, fmt);

	pr_info("ISP%u: set input fmt\n", isp_dev->index);

	return 0;
}

static int isp_hw_set_slice_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 i = 0;
	int temp = 0;
	int pixel_bit = 0;
	int multiple = 0;
	int hsize_mipi = 0;
	int hsize_org = 0;
	int hsize_blk = 0;
	int hsize_isp = 0;
	int ovlp = 0;

	switch (fmt->bpp) {
	case 6:
		multiple = 128;
	break;
	case 8:
		multiple = 128;
	break;
	case 10:
		multiple = 640;
	break;
	case 12:
		multiple = 384;
	break;
	case 14:
		multiple = 128;
	break;
	}

	pixel_bit = fmt->bpp;
	hsize_org = fmt->width;
	if ((hsize_org / 2) % 6 == 0)
		hsize_blk = (hsize_org / 2) / 6;
	else
		hsize_blk = (hsize_org / 2) / 6 + 1;

	ovlp = hsize_blk + 20;
	hsize_isp = (hsize_org / 2) + ovlp;
	if ((hsize_isp * pixel_bit) % multiple == 0)
		hsize_mipi = hsize_isp;
	else {
		temp = hsize_isp * pixel_bit / multiple + 1;
		hsize_mipi = temp * multiple /pixel_bit;
	}

	isp_dev->lfmt.xstart = 0;
	isp_dev->lfmt.ystart = 0;
	isp_dev->lfmt.width = hsize_isp;
	isp_dev->lfmt.height = fmt->height;

	isp_dev->rfmt.xstart = hsize_mipi - hsize_isp;
	isp_dev->rfmt.ystart = 0;
	isp_dev->rfmt.width = hsize_isp;
	isp_dev->rfmt.height = fmt->height;

	fmt->xstart = 0;
	fmt->ystart = 0;
	fmt->width = hsize_mipi;

	isp_top_cfg_fmt(isp_dev, fmt);

	fmt->width = hsize_isp;

	isp_hw_set_fmt(isp_dev, fmt);

	isp_disp_set_overlap(isp_dev, ovlp);

	pr_info("ISP%u: set slice input fmt: %u-%u\n", isp_dev->index, fmt->width, fmt->height);

	return 0;
}

static void isp_hw_calc_slice(struct isp_dev_t *isp_dev, u32 idx)
{
	int ovlp;
	u32 hsize_org;
	struct aml_crop crop;
	struct aml_format fmt;

	isp_disp_get_overlap(isp_dev, &ovlp);
	isp_disp_get_crop_size(isp_dev, idx, &crop);
	isp_disp_get_scaler_out_size(isp_dev, idx, &fmt);

	hsize_org = (crop.hsize - ovlp) * 2;
	crop.hsize = hsize_org;

	isp_disp_pps_config(isp_dev, idx, &crop, &fmt);

	isp_disp_calc_slice(isp_dev, idx, &isp_dev->aslice[idx]);

	pr_info("ISP%u: calc slice param\n", isp_dev->index);
}

static int isp_hw_cfg_slice(struct isp_dev_t *isp_dev, int pos)
{
	int i = 0;

	switch (pos) {
	case 0:
		isp_ofe_cfg_slice_size(isp_dev, &isp_dev->lfmt);
	break;
	case 1:
		isp_ofe_cfg_slice_size(isp_dev, &isp_dev->rfmt);
	break;
	case 2:
		for (i = 0; i < 1; i++)
			isp_hw_calc_slice(isp_dev, i);
	break;
	}

	for (i = 0; i < 1; i++) {
		isp_dev->aslice[i].pos = pos;
		isp_disp_cfg_slice(isp_dev, i, &isp_dev->aslice[i]);
		isp_wrmifx3_cfg_slice(isp_dev, i, &isp_dev->aslice[i]);
	}

	isp_lens_cfg_slice(isp_dev, &isp_dev->aslice[0]);

	isp_tnr_cfg_slice(isp_dev, &isp_dev->aslice[0]);

	isp_nr_cac_cfg_slice(isp_dev, &isp_dev->aslice[0]);

	isp_post_pg2_ctrst_cfg_slice(isp_dev, &isp_dev->aslice[0]);

	isp_post_cm2_cfg_slice(isp_dev, &isp_dev->aslice[0]);

	isp_ltm_cfg_slice(isp_dev, &isp_dev->aslice[0]);

	return 0;
}

static int isp_hw_set_wdr_mode(struct isp_dev_t *isp_dev, int wdr_en)
{
	isp_top_cfg_wdr(isp_dev, wdr_en);

	return 0;
}

static int isp_hw_cfg_pattern(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	int rtn = -EINVAL;
	struct aml_format timgen;

	switch (isp_dev->index) {
	case 0:
	case 1:
		fmt->width = 1920;
		fmt->height = 1080;
		timgen.width = 8800;
		timgen.height = 5120;
	break;
	case 2:
	case 3:
		fmt->width = 1920;
		fmt->height = 1080;
		timgen.width = 8800;
		timgen.height = 5120;
	break;
		pr_err("ISP%u: Error isp index\n", isp_dev->index);
		return rtn;
	}

	isp_patgen_cfg_fmt(isp_dev, fmt);
	isp_patgen_cfg_size(isp_dev, fmt);
	isp_timgen_cfg(isp_dev, &timgen);

	isp_patgen_enable(isp_dev);
	isp_timgen_enable(isp_dev);

	return 0;
}

static int isp_hw_cfg_timgen(struct isp_dev_t *isp_dev)
{
	int rtn = -EINVAL;
	struct aml_format timgen;

	switch (isp_dev->index) {
	case 0:
	case 1:
		timgen.width = 2592 + 1726;
		timgen.height = 1944 + 1944;
	break;
	case 2:
	case 3:
		timgen.width = 1328 + 1665;
		timgen.height = 1120 + 1120;
	break;
		pr_err("ISP%u: Error isp index\n", isp_dev->index);
		return rtn;
	}

	isp_timgen_cfg(isp_dev, &timgen);

	return 0;
}

static int isp_stream_set_fmt(struct aml_video *video, struct aml_format *fmt)
{
	switch (video->id) {
	case AML_ISP_STREAM_DDR:
		isp_rdmif0_cfg_frm_size(video->priv, fmt);
	break;
	case AML_ISP_STREAM_STATS:
		isp_3a_flkr_cfg_dmawr_size(video->priv, fmt);
	break;
	case AML_ISP_STREAM_0:
	case AML_ISP_STREAM_1:
	case AML_ISP_STREAM_2:
	case AML_ISP_STREAM_3:
		isp_disp_set_scaler_out_size(video->priv, video->id - 3, fmt);
		isp_disp_set_csc2_fmt(video->priv, video->id -3, fmt);
	case AML_ISP_STREAM_RAW:
		isp_wrmifx3_cfg_frm_size(video->priv, video->id - 3, fmt);
	break;
	}

	return 0;
}

static int isp_stream_cfg_buf(struct aml_video *video, struct aml_buffer *buff)
{
	switch (video->id) {
	case AML_ISP_STREAM_DDR:
		isp_rdmif0_cfg_frm_buff(video->priv, buff);
	break;
	case AML_ISP_STREAM_STATS:
		isp_3a_flkr_cfg_stat_buff(video->priv, buff);
	break;
	case AML_ISP_STREAM_0:
	case AML_ISP_STREAM_1:
	case AML_ISP_STREAM_2:
	case AML_ISP_STREAM_3:
	case AML_ISP_STREAM_RAW:
		isp_wrmifx3_cfg_frm_buff(video->priv, video->id - 3, buff);
	break;
	}

	return 0;
}

static int isp_stream_bilateral_cfg(struct aml_video *video, struct aml_buffer *buff)
{
	int rtn = 0;

	switch (video->id) {
	case AML_ISP_STREAM_PARAM:
		isp_fed_cfg_param(video->priv, buff);
		isp_lens_cfg_param(video->priv, buff);

		isp_snr_cfg_param(video->priv, buff);
		isp_cnr_cfg_param(video->priv, buff);
		isp_tnr_cfg_param(video->priv, buff);
		isp_ofe_cfg_param(video->priv, buff);
		isp_ofe_wdr_cfg_param(video->priv, buff);
		isp_gtm_cfg_param(video->priv, 0, buff);

		isp_disp_cfg_param(video->priv, 0, buff);
		isp_disp_cfg_param(video->priv, 1, buff);
		isp_disp_cfg_param(video->priv, 2, buff);

		isp_3a_flkr_cfg_param(video->priv, buff);
		isp_dpc_cfg_param(video->priv, buff);
		isp_nr_cac_cfg_param(video->priv, buff);
		isp_top_cfg_param(video->priv, buff);
		isp_post_pg2_ctrst_cfg_param(video->priv, buff);
		isp_patgen_cfg_param(video->priv, buff);
		isp_ltm_cfg_param(video->priv, buff);
		isp_post_pg0_top_cfg_param(video->priv, buff);

		isp_post_cm2_cfg_param(video->priv, buff);
		isp_post_tnr_cfg_param(video->priv, buff);
		isp_pk_cfg_param(video->priv, buff);
		isp_dms_cfg_param(video->priv, buff);
	break;
	case AML_ISP_STREAM_STATS:
		isp_ofe_wdr_stats(video->priv, buff);
		isp_ltm_stats(video->priv, buff);
		isp_post_pg2_ctrst_stats(video->priv, buff);

		isp_top_req_info(video->priv, buff);
		isp_3a_flkr_req_info(video->priv, buff);
		isp_ofe_wdr_req_info(video->priv, buff);
		isp_ltm_req_info(video->priv, buff);
		isp_post_pg2_ctrst_req_info(video->priv, buff);
		isp_ofe_req_info(video->priv, buff);
		isp_intf_top_loss_index(video->priv);
	break;
	}

	return rtn;
}

static void isp_hw_stream_crop(struct aml_video *video)
{
	switch (video->id) {
	case AML_ISP_STREAM_0:
	case AML_ISP_STREAM_1:
	case AML_ISP_STREAM_2:
	case AML_ISP_STREAM_3:
		isp_disp_set_crop_size(video->priv, video->id - 3, &video->acrop);
		isp_disp_set_scaler_out_size(video->priv, video->id - 3, &video->afmt);
	break;
	}
}

static void isp_hw_stream_on(struct aml_video *video)
{
	struct isp_dev_t *isp_dev = video->priv;

	switch (video->id) {
	case AML_ISP_STREAM_DDR:
		isp_rdmif0_module_enable(video->priv, 1);
		isp_hw_cfg_timgen(isp_dev);
		isp_timgen_enable(isp_dev);
	break;
	case AML_ISP_STREAM_STATS:
		isp_3a_flkr_enable(video->priv);
	break;
	case AML_ISP_STREAM_0:
	case AML_ISP_STREAM_1:
	case AML_ISP_STREAM_2:
		isp_disp_pps_config(video->priv, video->id - 3,
				&video->acrop, &video->afmt);
	case AML_ISP_STREAM_3:
		if (isp_dev->slice == 0)
			isp_disp_set_crop_size(video->priv, video->id - 3, &video->acrop);

		isp_disp_cfg_sel(video->priv, video->id -3, video->disp_sel);
		isp_disp_enable(video->priv, video->id - 3);

	case AML_ISP_STREAM_RAW:
		isp_wrmifx3_module_enable(video->priv, video->id - 3, 1, 0);
	break;
	}

	pr_info("ISP%u: hw stream %d on\n", isp_dev->index, video->id);
}

static void isp_hw_stream_off(struct aml_video *video)
{
	struct isp_dev_t *isp_dev = video->priv;

	switch (video->id) {
	case AML_ISP_STREAM_DDR:
		isp_rdmif0_module_enable(video->priv, 0);
		isp_timgen_disable(isp_dev);
	break;
	case AML_ISP_STREAM_STATS:
		isp_3a_flkr_disable(video->priv);
	break;
	case AML_ISP_STREAM_0:
	case AML_ISP_STREAM_1:
	case AML_ISP_STREAM_2:
	case AML_ISP_STREAM_3:
		isp_disp_disable(video->priv, video->id - 3);
	case AML_ISP_STREAM_RAW:
		isp_wrmifx3_module_enable(video->priv, video->id - 3, 0, 0);
	break;
	}

	pr_info("ISP%u: hw stream %d off\n", isp_dev->index, video->id);
}

static void isp_hw_start(struct isp_dev_t *isp_dev)
{
	isp_top_enable(isp_dev);

	pr_info("ISP%u: hw start\n", isp_dev->index);
}

static void isp_hw_stop(struct isp_dev_t *isp_dev)
{
	isp_top_disable(isp_dev);
	isp_patgen_disable(isp_dev);
	isp_timgen_disable(isp_dev);

	pr_info("ISP%u: hw stop\n", isp_dev->index);
}

static int isp_hw_enable_wrmifx3(struct aml_video *video, int enable, int force)
{
	struct isp_global_info *g_info = isp_global_get_info();

	if (g_info->mode != AML_ISP_SCAM)
		force = 0;

	isp_wrmifx3_module_enable(video->priv, video->id - 3, enable, force);

	return 0;
}

static int isp_hw_get_wrmifx3_stat(struct aml_video *video)
{
	u32 val = 0;

	isp_wrmifx3_module_stat(video->priv, video->id - 3, &val);

	return val;
}

static int isp_hw_cfg_ptnr_mif_buf(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	isp_ptnr_mif_cfg_buf(isp_dev, buff);

	return 0;
}

static int isp_hw_enable_ptnr_mif(struct isp_dev_t *isp_dev, u32 enable)
{
	isp_ptnr_mif_enable(isp_dev, enable);

	return 0;
}

static int isp_hw_cfg_mcnr_mif_buf(struct isp_dev_t *isp_dev, struct aml_format *fmt, struct aml_buffer *buff)
{
	isp_mcnr_mif_cfg_buf(isp_dev, fmt, buff);
	isp_intf_top_cfg_buf(isp_dev, fmt, buff);

	return 0;
}

static int isp_hw_enable_mcnr_mif(struct isp_dev_t *isp_dev, int enable)
{
	isp_mcnr_mif_enable(isp_dev, enable);

	return 0;
}

static int isp_hw_enable_rot(struct aml_video *video, int enable)
{

	switch(video->rot_type){
		case AML_MIRROR_NONE:
			isp_wrmifx3_mirror_enable(video->priv, video->id - 3, 0);
			isp_wrmifx3_flip_enable(video->priv, video->id - 3, 0);
		break;
		case AML_MIRROR_HORIZONTAL:
			isp_wrmifx3_mirror_enable(video->priv, video->id - 3, enable);
			isp_wrmifx3_flip_enable(video->priv, video->id - 3, 0);
		break;
		case AML_MIRROR_VERTICAL:
			isp_wrmifx3_mirror_enable(video->priv, video->id - 3, 0);
			isp_wrmifx3_flip_enable(video->priv, video->id - 3, enable);
		break;
		case AML_MIRROR_BOTH:
			isp_wrmifx3_mirror_enable(video->priv, video->id - 3, enable);
			isp_wrmifx3_flip_enable(video->priv, video->id - 3, enable);
		break;
		default:
			pr_err("Failed to support rot type %d\n", video->rot_type);
			return -1;
	}

	return 0;
}

static int isp_hw_fill_rreg_buff(struct isp_dev_t *isp_dev)
{
	if (isp_dev->apb_dma == 0)
		return 0;

	isp_apb_dma_fill_rreg_buff(isp_dev);

	return 0;
}

static int isp_hw_fill_gisp_rreg_buff(struct isp_global_info *g_isp_info)
{
	if (g_isp_info->isp_dev->apb_dma == 0)
		return 0;

	isp_apb_dma_fill_gisp_rreg_buff(g_isp_info);

	return 0;
}

static int isp_hw_start_apb_dma(struct isp_dev_t *isp_dev)
{
	isp_apb_dma_start(isp_dev);

	return 0;
}

static int isp_hw_stop_apb_dma(struct isp_dev_t *isp_dev)
{
	isp_apb_dma_stop(isp_dev);

	return 0;
}

static int isp_hw_check_done_apb_dma(struct isp_dev_t *isp_dev)
{
	isp_apb_dma_check_done(isp_dev);

	return 0;
}

static int isp_hw_manual_trigger_apb_dma(struct isp_dev_t *isp_dev)
{
	isp_apb_dma_manual_trigger(isp_dev);

	return 0;
}

static int isp_hw_auto_trigger_apb_dma(struct isp_dev_t *isp_dev)
{
	isp_apb_dma_auto_trigger(isp_dev);

	return 0;
}

const struct isp_dev_ops isp_hw_ops = {
	.hw_init = isp_hw_init,
	.hw_reset = isp_hw_reset,
	.hw_version = isp_hw_version,
	.hw_interrupt_status = isp_hw_interrupt_status,
	.hw_set_input_fmt = isp_hw_set_input_fmt,
	.hw_set_slice_fmt = isp_hw_set_slice_fmt,
	.hw_cfg_slice = isp_hw_cfg_slice,
	.hw_cfg_pattern = isp_hw_cfg_pattern,
	.hw_stream_set_fmt = isp_stream_set_fmt,
	.hw_stream_cfg_buf = isp_stream_cfg_buf,
	.hw_stream_bilateral_cfg = isp_stream_bilateral_cfg,
	.hw_stream_crop = isp_hw_stream_crop,
	.hw_stream_on = isp_hw_stream_on,
	.hw_stream_off = isp_hw_stream_off,
	.hw_start = isp_hw_start,
	.hw_stop = isp_hw_stop,
	.hw_enable_wrmif = isp_hw_enable_wrmifx3,
	.hw_get_wrmif_stat = isp_hw_get_wrmifx3_stat,
	.hw_set_wdr_mode = isp_hw_set_wdr_mode,
	.hw_set_rot = isp_hw_enable_rot,
	.hw_cfg_ptnr_mif_buf = isp_hw_cfg_ptnr_mif_buf,
	.hw_enable_ptnr_mif = isp_hw_enable_ptnr_mif,
	.hw_cfg_mcnr_mif_buf = isp_hw_cfg_mcnr_mif_buf,
	.hw_enable_mcnr_mif = isp_hw_enable_mcnr_mif,
	.hw_start_apb_dma = isp_hw_start_apb_dma,
	.hw_stop_apb_dma = isp_hw_stop_apb_dma,
	.hw_check_done_apb_dma = isp_hw_check_done_apb_dma,
	.hw_manual_trigger_apb_dma = isp_hw_manual_trigger_apb_dma,
	.hw_auto_trigger_apb_dma = isp_hw_auto_trigger_apb_dma,
	.hw_fill_rreg_buff = isp_hw_fill_rreg_buff,
	.hw_fill_gisp_rreg_buff = isp_hw_fill_gisp_rreg_buff,
};
EXPORT_SYMBOL(isp_hw_ops);
