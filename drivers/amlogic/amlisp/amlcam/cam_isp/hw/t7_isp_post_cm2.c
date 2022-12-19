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

static void post_cm2_cfg_param(struct isp_dev_t *isp_dev, void *param)
{
	u32 val = 0, i = 0, j = 0;
	aisp_cm2_cfg_t *cm2_cfg = param;
	uint32_t tmp[20];

	uint32_t *data_luma_via_hue_lut = (uint32_t *)(cm2_cfg->cm2_adj_luma_via_hue);
	uint32_t *data_sat_via_hs_lut = (uint32_t *)(cm2_cfg->cm2_adj_sat_via_hs);
	uint32_t *data_hue_via_h_lut = (uint32_t *)(cm2_cfg->cm2_adj_hue_via_h);
	uint32_t *data_hue_via_y_lut = (uint32_t *)(cm2_cfg->cm2_adj_hue_via_y);
	uint32_t *data_hue_via_s_lut = (uint32_t *)(cm2_cfg->cm2_adj_hue_via_s);
	uint32_t *data_satgain_via_y_lut = (uint32_t *)(cm2_cfg->cm2_adj_satgain_via_y);

	isp_hw_lut_wstart(isp_dev, PST_CM2_LUT_CFG);

	val = ((cm2_cfg->cm2_adj_satglbgain_via_y[0] << 0) & 0xFF) |
		((cm2_cfg->cm2_adj_satglbgain_via_y[1] << 8) & 0xFF00) |
		((cm2_cfg->cm2_adj_satglbgain_via_y[2] << 16) & 0xFF0000) |
		((cm2_cfg->cm2_adj_satglbgain_via_y[3] << 24) & 0xFF000000);
	isp_reg_write(isp_dev, ISP_CM2_SAT_GLB_GAIN_0, val);

	val = ((cm2_cfg->cm2_adj_satglbgain_via_y[4] << 0) & 0xFF) |
		((cm2_cfg->cm2_adj_satglbgain_via_y[5] << 8) & 0xFF00) |
		((cm2_cfg->cm2_adj_satglbgain_via_y[6] << 16) & 0xFF0000) |
		((cm2_cfg->cm2_adj_satglbgain_via_y[7] << 24) & 0xFF000000);
	isp_reg_write(isp_dev, ISP_CM2_SAT_GLB_GAIN_1, val);

	val = cm2_cfg->cm2_adj_satglbgain_via_y[8];
	isp_reg_write(isp_dev, ISP_CM2_SAT_GLB_GAIN_2, val);

	isp_reg_update_bits(isp_dev, ISP_CM2_EN, cm2_cfg->cm2_global_sat, 0, 12);
	isp_reg_update_bits(isp_dev, ISP_CM2_EN, cm2_cfg->cm2_global_hue, 12, 12);

	isp_reg_update_bits(isp_dev, ISP_CM2_LUMA_BC, cm2_cfg->cm2_luma_contrast, 0, 12);
	isp_reg_update_bits(isp_dev, ISP_CM2_LUMA_BC, cm2_cfg->cm2_luma_brightness, 12, 13);

	for (i = 0; i < 32; i++) {
		isp_reg_write(isp_dev, ISP_CM2_ADDR_PORT, 256+i*8);

		tmp[0] = data_luma_via_hue_lut[i];
		for (j = 0; j < 3; j++) {
			tmp[1+j] = data_sat_via_hs_lut[j*32+i];
		}
		tmp[4] = data_hue_via_h_lut[i];
		for (j = 0; j < 5; j++) {
			tmp[5+j] = data_hue_via_y_lut[j*32+i];
			tmp[10+j] = data_hue_via_s_lut[j*32+i];
			tmp[15+j] = data_satgain_via_y_lut[j*32+i];
		}

		for (j = 0; j < 5; j++) {
			val = (tmp[j*4+0]&0xff)|(tmp[j*4+1]<<8)|(tmp[j*4+2]<<16)|(tmp[j*4+3]<<24);
			isp_reg_write(isp_dev, ISP_CM2_DATA_PORT, val);
		}
	}

	isp_hw_lut_wend(isp_dev);
}

void isp_post_cm2_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt)
{
	u32 val = 0;
	int phidx, thidx, pvidx, tvidx;
	u32 xsize = fmt->width;
	u32 ysize = fmt->height;

	val = (ysize << 16) | (xsize << 0);
	isp_reg_write(isp_dev, FRAME_SIZE_REG, val);

	isp_reg_update_bits(isp_dev, ISP_LC_STA_HV_NUM, LTM_STA_LEN_H - 1, 0, 5);
	isp_reg_update_bits(isp_dev, ISP_LC_STA_HV_NUM, LTM_STA_LEN_V - 1, 8, 5);

	phidx = (0 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize - 1);
	phidx = MAX(phidx - 1, 0);
	thidx = (1 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize - 1);
	thidx = MAX(thidx - 1, 0);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LC_STA_HIDX_0, val);

	phidx = (2 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize - 1);
	phidx = MAX(phidx - 1, 0);
	thidx = (3 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize - 1);
	thidx = MAX(thidx - 1, 0);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LC_STA_HIDX_1, val);

	phidx = (4 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize - 1);
	phidx = MAX(phidx - 1, 0);
	thidx = (5 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize - 1);
	thidx = MAX(thidx - 1, 0);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LC_STA_HIDX_2, val);

	phidx = (6 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize - 1);
	phidx = MAX(phidx - 1, 0);
	thidx = (7 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize - 1);
	thidx = MAX(thidx - 1, 0);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LC_STA_HIDX_3, val);

	phidx = (8 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize - 1);
	phidx = MAX(phidx - 1, 0);
	thidx = (9 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize - 1);
	thidx = MAX(thidx - 1, 0);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LC_STA_HIDX_4, val);

	phidx = (10 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	phidx = MIN(phidx, xsize - 1);
	phidx = MAX(phidx - 1, 0);
	thidx = (11 * xsize / MAX(1, LTM_STA_LEN_H - 1));
	thidx = MIN(thidx, xsize - 1);
	thidx = MAX(thidx - 1, 0);
	val = (thidx << 16) | (phidx << 0);
	isp_reg_write(isp_dev, ISP_LC_STA_HIDX_5, val);

	phidx = xsize;
	phidx = MIN(phidx, xsize - 1);
	phidx = MAX(phidx - 1, 0);
	isp_reg_write(isp_dev, ISP_LC_STA_HIDX_12, phidx);


	pvidx = 0 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	pvidx = MIN(pvidx, ysize - 1);
	pvidx = MAX(pvidx - 1, 0);
	tvidx = 1 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	tvidx = MIN(tvidx, ysize - 1);
	tvidx = MAX(tvidx - 1, 0);
	val = (tvidx << 16) | (pvidx << 0);
	isp_reg_write(isp_dev, ISP_LC_STA_VIDX_0, val);

	pvidx = 2 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	pvidx = MIN(pvidx, ysize - 1);
	pvidx = MAX(pvidx - 1, 0);
	tvidx = 3 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	tvidx = MIN(tvidx, ysize - 1);
	tvidx = MAX(tvidx - 1, 0);
	val = (tvidx << 16) | (pvidx << 0);
	isp_reg_write(isp_dev, ISP_LC_STA_VIDX_1, val);

	pvidx = 4 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	pvidx = MIN(pvidx, ysize - 1);
	pvidx = MAX(pvidx - 1, 0);
	tvidx = 5 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	tvidx = MIN(tvidx, ysize - 1);
	tvidx = MAX(tvidx - 1, 0);
	val = (tvidx << 16) | (pvidx << 0);
	isp_reg_write(isp_dev, ISP_LC_STA_VIDX_2, val);

	pvidx = 6 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	pvidx = MIN(pvidx, ysize - 1);
	pvidx = MAX(pvidx - 1, 0);
	tvidx = 7 * ysize / MAX(1, LTM_STA_LEN_V - 1);
	tvidx = MIN(tvidx, ysize - 1);
	tvidx = MAX(tvidx - 1, 0);
	val = (tvidx << 16) | (pvidx << 0);
	isp_reg_write(isp_dev, ISP_LC_STA_VIDX_3, val);

	pvidx = ysize;
	pvidx = MIN(pvidx, ysize - 1);
	pvidx = MAX(pvidx - 1, 0);
	isp_reg_write(isp_dev, ISP_LC_STA_VIDX_8, pvidx);

	val = (0 << 0) | (xsize << 16);
	isp_reg_write(isp_dev, ISP_PST_GLBWIN_H, val);

	val = (0 << 0) | (ysize << 16);
	isp_reg_write(isp_dev, ISP_PST_GLBWIN_V, val);

	val = (0 << 0) | (xsize << 16);
	isp_reg_write(isp_dev, ISP_DNLP_GLBWIN_H, val);

	val = (0 << 0) | (ysize << 16);
	isp_reg_write(isp_dev, ISP_DNLP_GLBWIN_V, val);
}

/* When lc_sta_hidx are configured, the processing speed of isp hardware will slow down */
void isp_post_cm2_cfg_slice(struct isp_dev_t *isp_dev, struct aml_slice *param)
{
	int i = 0;
	u32 addr = 0;
	u32 val = 0;
	u32 start, end;
	s32 hidx = 0;
	s32 hsize = 0;
	s32 ovlp = 0;
	s32 slice_size = 0;

	hsize = param->whole_frame_hcenter * 2;

	if (param->pos == 0) {
		start = 0;
		end = param->pleft_hsize - param->pleft_ovlp;

		val = (start << 0) | (end << 16);
		isp_hwreg_write(isp_dev, ISP_PST_GLBWIN_H, val);
		isp_hwreg_write(isp_dev, ISP_DNLP_GLBWIN_H, val);

		for (i = 0; i < LTM_STA_LEN_H; i++) {
			hidx = (i == (LTM_STA_LEN_H - 1)) ? hsize : (i * hsize / MAX(1, LTM_STA_LEN_H - 1));
			slice_size = param->pleft_hsize - param->pleft_ovlp + (i - (LTM_STA_LEN_H - 1) / 2) * 4;
			hidx = MIN(MAX(hidx, 0), slice_size);

			hidx = MIN(MAX(hidx - 1, 0), param->pleft_hsize - (LTM_STA_LEN_H - i) * 4 - 1);

			addr = ISP_LC_STA_HIDX_0 + (i / 2 * 4);
			isp_hwreg_update_bits(isp_dev, addr, hidx, (i % 2) * 16, 16);
		}
	} else if (param->pos == 1) {
		start = param->pright_ovlp;
		end = param->pright_hsize;

		val = (start << 0) | (end << 16);
		isp_hwreg_write(isp_dev, ISP_PST_GLBWIN_H, val);
		isp_hwreg_write(isp_dev, ISP_DNLP_GLBWIN_H, val);

		slice_size = param->pright_hsize;
		ovlp = param->pright_ovlp;
		for (i = 0; i < LTM_STA_LEN_H; i++) {
			hidx = (i == (LTM_STA_LEN_H - 1)) ? hsize : (i * hsize / MAX(1, LTM_STA_LEN_H - 1));
			hidx = MIN(MAX(hidx + ovlp * 2 - slice_size, (ovlp -hsize /(LTM_STA_LEN_H - 1) + (i - (LTM_STA_LEN_H - 1) / 2 + 1) * 4)), slice_size);

			hidx = MIN(MAX(hidx -1, 0), slice_size - 1);
			addr = ISP_LC_STA_HIDX_0 + (i / 2 * 4);
			isp_hwreg_update_bits(isp_dev, addr, hidx, (i % 2) * 16, 16);
		}
	}
}

void isp_post_cm2_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff)
{
	aisp_param_t *param = buff->vaddr[AML_PLANE_A];

	if (param->pvalid.aisp_cm2)
		post_cm2_cfg_param(isp_dev, &param->cm2_cfg);
}

void isp_post_cm2_init(struct isp_dev_t *isp_dev)
{
	isp_reg_update_bits(isp_dev, ISP_CM2_EN, 512, 0, 12);
}
