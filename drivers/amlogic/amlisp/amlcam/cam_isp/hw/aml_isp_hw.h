/*
*
* SPDX-License-Identifier: GPL-2.0
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

#ifndef __AML_ISP_HW_H__
#define __AML_ISP_HW_H__

#define pr_fmt(fmt)  "isp_hw:%s:%d: " fmt, __func__, __LINE__

#include <linux/io.h>
#include <linux/kernel.h>

#include "../aml_isp.h"
#include "aml_isp_cfg.h"

#define MAX(X, Y) ((X) >= (Y) ? (X) : (Y))
#define MIN(X, Y) ((X) <= (Y) ? (X) : (Y))
#define ISP_ALIGN(data, aln)   (((data) + (aln) -1) & (~((aln) - 1)))
#define ISP_ARRAY_SIZE(array)   (sizeof(array) / sizeof(array[0]))

#define MAX_CHANNEL  (2)
#define AE_STA_TOTL_MAX  (64 * 64)
#define AE_STA_HIST_NUM  (1 << 10)
#define AWB_STA_TOTL_MAX  (4 * 32 * 32)
#define AF_STA_TOTL_MAX  (32 * 32)
#define MAX_HSIZE 2896
#define LTM_STA_LEN_H  (13)
#define LTM_STA_LEN_V  (9)

enum {
	ISP_FMT_GRBG = 0,
	ISP_FMT_RGGB,
	ISP_FMT_BGGR,
	ISP_FMT_GBRG,
	ISP_FMT_RGBIR4X4,
	ISP_FMT_MAX
};

static inline u32 isp_hwreg_read(struct isp_dev_t *isp_dev, u32 addr)
{
	return readl(isp_dev->base + addr);
}

static inline void isp_hwreg_write(struct isp_dev_t *isp_dev, u32 addr, u32 val)
{
	writel(val, isp_dev->base + addr);
}

void isp_hw_lut_wstart(struct isp_dev_t *isp_dev, u32 type);
void isp_hw_lut_wend(struct isp_dev_t *isp_dev);
u32 isp_reg_read(struct isp_dev_t *isp_dev, u32 addr);
void isp_reg_write(struct isp_dev_t *isp_dev, u32 addr, u32 val);
int isp_reg_update_bits(struct isp_dev_t *isp_dev, u32 addr, u32 val, u32 start, u32 len);
int isp_hwreg_update_bits(struct isp_dev_t *isp_dev, u32 addr, u32 val, u32 start, u32 len);

int isp_reg_read_bits(struct isp_dev_t *isp_dev, u32 addr, u32 *val, u32 start, u32 len);
u32 isp_hw_float_convert(u32 data);
int isp_hw_convert_fmt(struct aml_format *fmt);

void isp_top_init(struct isp_dev_t *isp_dev);
void isp_top_enable(struct isp_dev_t *isp_dev);
void isp_top_disable(struct isp_dev_t *isp_dev);
u32 isp_top_irq_stat(struct isp_dev_t *isp_dev);
void isp_top_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_top_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_top_req_info(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_top_cfg_wdr(struct isp_dev_t *isp_dev, int wdr_en);
void isp_top_decmpr_disable(void *isp_dev);
void isp_top_reset(void *isp_dev);

void isp_ofe_init(struct isp_dev_t *isp_dev);
void isp_ofe_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_ofe_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_ofe_cfg_slice_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_ofe_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_ofe_req_info(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_timgen_enable(struct isp_dev_t *isp_dev);
void isp_timgen_disable(struct isp_dev_t *isp_dev);
void isp_timgen_cfg(struct isp_dev_t *isp_dev, struct aml_format *fmt);

void isp_patgen_init(struct isp_dev_t *isp_dev);
void isp_patgen_enable(struct isp_dev_t *isp_dev);
void isp_patgen_disable(struct isp_dev_t *isp_dev);
void isp_patgen_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_patgen_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_patgen_cfg_timgen(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_patgen_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_dpc_init(struct isp_dev_t *isp_dev);
void isp_dpc_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_dpc_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_fed_init(struct isp_dev_t *isp_dev);
void isp_fed_cfg_ofst(struct isp_dev_t *isp_dev);
void isp_fed_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_fed_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_nr_cac_init(struct isp_dev_t *isp_dev);
void isp_nr_cac_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_nr_cac_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_nr_cac_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_nr_cac_cfg_slice(struct isp_dev_t *isp_dev, struct aml_slice *param);

void isp_snr_init(struct isp_dev_t *isp_dev);
void isp_snr_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_snr_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_lens_init(struct isp_dev_t *isp_dev);
void isp_lens_cfg_ofst(struct isp_dev_t *isp_dev);
void isp_lens_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_lens_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_lens_cfg_slice(struct isp_dev_t *isp_dev, struct aml_slice *param);
void isp_lens_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_gtm_init(struct isp_dev_t *isp_dev);
void isp_gtm_req_info(struct isp_dev_t *isp_dev, u32 idx, struct aml_buffer *buff);
void isp_gtm_cfg_param(struct isp_dev_t *isp_dev, u32 idx, struct aml_buffer *buff);

void isp_dms_init(struct isp_dev_t *isp_dev);
void isp_dms_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_dms_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_dms_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_disp_init(struct isp_dev_t *isp_dev);
void isp_disp_set_input_size(struct isp_dev_t *isp_dev, u32 idx, struct aml_format *fmt);
void isp_disp_set_crop_size(struct isp_dev_t *isp_dev, u32 idx, struct aml_crop *crop);
void isp_disp_get_crop_size(struct isp_dev_t *isp_dev, u32 idx, struct aml_crop *crop);
void isp_disp_set_scaler_out_size(struct isp_dev_t *isp_dev, u32 idx, struct aml_format *fmt);
void isp_disp_get_scaler_out_size(struct isp_dev_t *isp_dev, u32 idx, struct aml_format *fmt);
void isp_disp_cfg_sel(struct isp_dev_t *isp_dev, u32 idx, u32 sel);
void isp_disp_enable(struct isp_dev_t *isp_dev, u32 idx);
void isp_disp_disable(struct isp_dev_t *isp_dev, u32 idx);
void isp_disp_pps_config(struct isp_dev_t *isp_dev, u32 idx, struct aml_crop *input, struct aml_format *output);
void isp_disp_cfg_param(struct isp_dev_t *isp_dev, u32 idx, struct aml_buffer *buff);
void isp_disp_set_overlap(struct isp_dev_t *isp_dev, int ovlp);
void isp_disp_get_overlap(struct isp_dev_t *isp_dev, int *ovlp);
void isp_disp_calc_slice(struct isp_dev_t *isp_dev, u32 idx, struct aml_slice *param);
void isp_disp_cfg_slice(struct isp_dev_t *isp_dev, u32 idx, struct aml_slice *param);
void isp_disp_set_csc2_fmt(struct isp_dev_t *isp_dev, u32 idx, struct aml_format *fmt);

void isp_3a_flkr_init(struct isp_dev_t *isp_dev);
void isp_3a_flkr_enable(struct isp_dev_t *isp_dev);
void isp_3a_flkr_disable(struct isp_dev_t *isp_dev);
void isp_3a_flkr_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_3a_flkr_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_3a_flkr_cfg_dmawr_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_3a_flkr_cfg_stat_buff(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_3a_flkr_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_3a_flkr_req_info(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_ptnr_mif_init(struct isp_dev_t *isp_dev);
void isp_ptnr_mif_enable(struct isp_dev_t *isp_dev, u32 enable);
void isp_ptnr_mif_cfg_buf(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_ptnr_mif_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);

void isp_wrmifx3_init(struct isp_dev_t *isp_dev);
void isp_wrmifx3_mirror_enable(struct isp_dev_t *isp_dev, u32 idx, u32 enable);
void isp_wrmifx3_flip_enable(struct isp_dev_t *isp_dev, u32 idx, u32 enable);
void isp_wrmifx3_cfg_frm_size(struct isp_dev_t *isp_dev, u32 idx, struct aml_format *fmt);
void isp_wrmifx3_cfg_frm_buff(struct isp_dev_t *isp_dev, u32 idx, struct aml_buffer *buff);
void isp_wrmifx3_autoacc_num(struct isp_dev_t *isp_dev, u32 idx, u32 num);
void isp_wrmifx3_autoacc_enable(struct isp_dev_t *isp_dev, u32 idx, u32 enable);
void isp_wrmifx3_module_enable(struct isp_dev_t *isp_dev, u32 idx, u32 enbale, u32 force);
void isp_wrmifx3_module_stat(struct isp_dev_t *isp_dev, u32 idx, u32* val);
void isp_wrmifx3_cfg_slice(struct isp_dev_t *isp_dev, u32 idx, struct aml_slice *param);

void isp_rdmif0_init(struct isp_dev_t *isp_dev);
void isp_rdmif0_mirror_enable(struct isp_dev_t *isp_dev, u32 enable);
void isp_rdmif0_flip_enable(struct isp_dev_t *isp_dev, u32 enable);
void isp_rdmif0_cfg_frm_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_rdmif0_cfg_frm_buff(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_rdmif0_module_enable(struct isp_dev_t *isp_dev, u32 enbale);

void isp_intf_top_init(struct isp_dev_t *isp_dev);
void isp_intf_top_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_intf_top_cfg_buf(struct isp_dev_t *isp_dev, struct aml_format *fmt, struct aml_buffer *buff);
void isp_intf_top_loss_index(struct isp_dev_t *isp_dev);

void isp_tnr_init(struct isp_dev_t *isp_dev);
void isp_tnr_cfg_slice(struct isp_dev_t *isp_dev, struct aml_slice *param);
void isp_tnr_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_cnr_init(struct isp_dev_t *isp_dev);
void isp_cnr_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_cnr_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_pk_init(struct isp_dev_t *isp_dev);
void isp_pk_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_ltm_init(struct isp_dev_t *isp_dev);
void isp_ltm_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_ltm_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_ltm_cfg_slice(struct isp_dev_t *isp_dev, struct aml_slice *param);
void isp_ltm_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_ltm_req_info(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_ltm_stats(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_post_pg2_ctrst_init(struct isp_dev_t *isp_dev);
void isp_post_pg2_ctrst_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_post_pg2_ctrst_cfg_slice(struct isp_dev_t *isp_dev, struct aml_slice *param);
void isp_post_pg2_ctrst_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_post_pg2_ctrst_stats(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_post_pg2_ctrst_req_info(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_post_cm2_init(struct isp_dev_t *isp_dev);
void isp_post_cm2_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_post_cm2_cfg_slice(struct isp_dev_t *isp_dev, struct aml_slice *param);
void isp_post_cm2_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_post_pg0_top_init(struct isp_dev_t *isp_dev);
void isp_post_pg0_top_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_cmpr_raw_init(struct isp_dev_t *isp_dev);
void isp_cmpr_raw_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_cmpr_raw_cfg_ratio(struct isp_dev_t *isp_dev);

void isp_post_tnr_init(struct isp_dev_t *isp_dev);
void isp_post_tnr_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_ofe_wdr_init(struct isp_dev_t *isp_dev);
void isp_ofe_wdr_cfg_param(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_ofe_wdr_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_ofe_wdr_stats(struct isp_dev_t *isp_dev, struct aml_buffer *buff);
void isp_ofe_wdr_req_info(struct isp_dev_t *isp_dev, struct aml_buffer *buff);

void isp_mcnr_mif_init(struct isp_dev_t *isp_dev);
void isp_mcnr_cfg_fmt(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_mcnr_mif_enable(struct isp_dev_t *isp_dev, u32 enable);
void isp_mcnr_mif_cfg_size(struct isp_dev_t *isp_dev, struct aml_format *fmt);
void isp_mcnr_mif_cfg_buf(struct isp_dev_t *isp_dev, struct aml_format *fmt, struct aml_buffer *buff);

void isp_apb_dma_init(struct isp_dev_t *isp_dev);
void isp_apb_dma_start(struct isp_dev_t *isp_dev);
void isp_apb_dma_stop(struct isp_dev_t *isp_dev);
void isp_apb_dma_check_done(struct isp_dev_t *isp_dev);
void isp_apb_dma_manual_trigger(struct isp_dev_t *isp_dev);
void isp_apb_dma_fill_rreg_buff(struct isp_dev_t *isp_dev);
void isp_apb_dma_auto_trigger(struct isp_dev_t *isp_dev);
void isp_apb_dma_fill_gisp_rreg_buff(struct isp_global_info *g_isp);
void isp_apb_dma_start_dwreg(struct isp_dev_t *isp_dev);

#endif
