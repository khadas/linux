// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vpq_module_dump.h"
#include "../vpq_printk.h"

int vpq_module_dump_pq_table(enum vpq_dump_type_e value)
{
	int i = 0;

	if (value == VPQ_DUMP_VADJ1) {
		VPQ_PR_DRV("dump vadj1");
		VPQ_PR_DRV("brightness:%d", pic_mode_item.data[VPQ_ITEM_BRIGHTNESS]);
		VPQ_PR_DRV("contrast:%d", pic_mode_item.data[VPQ_ITEM_CONTRAST]);
		VPQ_PR_DRV("saturation:%d", pic_mode_item.data[VPQ_ITEM_SATURATION]);
		VPQ_PR_DRV("hue:%d", pic_mode_item.data[VPQ_ITEM_HUE]);
	} else if (value == VPQ_DUMP_GAMMA) {
		VPQ_PR_DRV("dump gamma");
		for (i = 0; i < VPQ_GAMMA_TABLE_LEN; i++)
			VPQ_PR_DRV("gma_r/g/b[%d]:%d %d %d",
				i,
				vpp_gma_table.r_data[i],
				vpp_gma_table.g_data[i],
				vpp_gma_table.b_data[i]);
	} else if (value == VPQ_DUMP_WB) {
		VPQ_PR_DRV("dump wb");
		VPQ_PR_DRV("rgb_ogo:%d %d %d %d %d %d %d %d %d %d",
			rgb_ogo.en,
			rgb_ogo.r_pre_offset, rgb_ogo.g_pre_offset, rgb_ogo.b_pre_offset,
			rgb_ogo.r_gain, rgb_ogo.g_gain, rgb_ogo.b_gain,
			rgb_ogo.r_post_offset, rgb_ogo.g_post_offset, rgb_ogo.b_post_offset);
	} else if (value == VPQ_DUMP_DNLP) {
		VPQ_PR_DRV("dump dnlp");
		VPQ_PR_DRV("dnlp_enable: %d\n", dnlp_enable);
		for (i = 0; i < VPP_DNLP_SCURV_LEN; i++)
			VPQ_PR_DRV("dnlp_scurv_low/mid1/mid2/hgh1/hgh2[%d]:%d %d %d %d %d",
				i,
				pdnlp_data->dnlp_scurv_low[i],
				pdnlp_data->dnlp_scurv_mid1[i],
				pdnlp_data->dnlp_scurv_mid2[i],
				pdnlp_data->dnlp_scurv_hgh1[i],
				pdnlp_data->dnlp_scurv_hgh2[i]);
		for (i = 0; i < VPP_DNLP_GAIN_VAR_LUT_LEN; i++)
			VPQ_PR_DRV("gain_var_lut49[%d]:%d", i, pdnlp_data->gain_var_lut49[i]);
		for (i = 0; i < VPP_DNLP_WEXT_GAIN_LEN; i++)
			VPQ_PR_DRV("wext_gain[%d]:%d", i, pdnlp_data->wext_gain[i]);
		for (i = 0; i < VPP_DNLP_ADP_THRD_LEN; i++)
			VPQ_PR_DRV("adp_thrd[%d]:%d", i, pdnlp_data->adp_thrd[i]);
		for (i = 0; i < VPP_DNLP_REG_BLK_BOOST_LEN; i++)
			VPQ_PR_DRV("reg_blk_boost_12[%d]:%d", i, pdnlp_data->reg_blk_boost_12[i]);
		for (i = 0; i < VPP_DNLP_REG_ADP_OFSET_LEN; i++)
			VPQ_PR_DRV("reg_adp_ofset_20[%d]:%d", i, pdnlp_data->reg_adp_ofset_20[i]);
		for (i = 0; i < VPP_DNLP_REG_MONO_PROT_LEN; i++)
			VPQ_PR_DRV("reg_mono_protect[%d]:%d", i, pdnlp_data->reg_mono_protect[i]);
		for (i = 0; i < VPP_DNLP_TREND_WHT_EXP_LUT_LEN; i++)
			VPQ_PR_DRV("reg_trend_wht_expand_lut8[%d]:%d",
				i, pdnlp_data->reg_trend_wht_expand_lut8[i]);
		for (i = 0; i < VPP_DNLP_HIST_GAIN_LEN; i++)
			VPQ_PR_DRV("c/s_hist_gain[%d]:%d %d",
				i, pdnlp_data->c_hist_gain[i], pdnlp_data->s_hist_gain[i]);
		for (i = 0; i < EN_DNLP_PARAM_MAX; i++)
			VPQ_PR_DRV("param[%d]:%d", i, pdnlp_data->param[i]);
	} else if (value == VPQ_DUMP_LC) {
		VPQ_PR_DRV("dump lc");
		VPQ_PR_DRV("lc_enable: %d\n", lc_enable);
		for (i = 0; i < 63; i++)
			VPQ_PR_DRV("lc_saturation[%d]:%d", i, lc_curve.lc_saturation[i]);
		for (i = 0; i < 16; i++) {
			VPQ_PR_DRV("lc_yminval/ypkbv_ymaxval/ymaxval/ypkbv_lmt[%d]:%d %d %d %d",
				i,
				lc_curve.lc_yminval_lmt[i],
				lc_curve.lc_ypkbv_ymaxval_lmt[i],
				lc_curve.lc_ymaxval_lmt[i],
				lc_curve.lc_ypkbv_lmt[i]);
		}
		for (i = 0; i < 4; i++)
			VPQ_PR_DRV("lc_ypkbv_ratio[%d]:%d", i, lc_curve.lc_ypkbv_ratio[i]);
		for (i = 0; i < 18; i++)
			VPQ_PR_DRV("lc_param.param[%d]:%d", i, lc_param.param[i]);
	} else if (value == VPQ_DUMP_BLE) {
		VPQ_PR_DRV("dump ble");
		VPQ_PR_DRV("blackext_enable: %d\n", blkext_en);
		VPQ_PR_DRV("blackext_start: %d\n", pble_data[0]);
		VPQ_PR_DRV("blackext_slope1: %d\n", pble_data[1]);
		VPQ_PR_DRV("blackext_mid_point: %d\n", pble_data[2]);
		VPQ_PR_DRV("blackext_slope2: %d\n", pble_data[3]);
	} else if (value == VPQ_DUMP_BLS) {
		VPQ_PR_DRV("dump bls");
		VPQ_PR_DRV("bluestretch_enable: %d\n", blue_str_en);
		for (i = 0; i < 18; i++)
			VPQ_PR_DRV("pbls_data[%d]:%d\n", i, pbls_data[i]);
	} else if (value == VPQ_DUMP_CM) {
		VPQ_PR_DRV("dump cm");
		for (i = 0; i < 9; i++)
			VPQ_PR_DRV("SatByY_0[%d]:%d\n", i, pcm_base_data->satbyy_0[i]);
		for (i = 0; i < 32; i++)
			VPQ_PR_DRV("LumaByHue_0[%d]:%d\n", i, pcm_base_data->lumabyhue_0[i]);
		for (i = 0; i < 32; i++)
			VPQ_PR_DRV("SatByHS_0/1/2[%d]:%d %d %d\n",
				i,
				pcm_base_data->satbyhs_0[i],
				pcm_base_data->satbyhs_1[i],
				pcm_base_data->satbyhs_2[i]);
		for (i = 0; i < 32; i++)
			VPQ_PR_DRV("HueByHue_0[%d]:%d\n", i, pcm_base_data->huebyhue_0[i]);
		for (i = 0; i < 32; i++)
			VPQ_PR_DRV("HueByHY_0/1/2/3/4[%d]:%d %d %d %d %d\n",
				i,
				pcm_base_data->huebyhy_0[i], pcm_base_data->huebyhy_1[i],
				pcm_base_data->huebyhy_2[i], pcm_base_data->huebyhy_3[i],
				pcm_base_data->huebyhy_4[i]);
		for (i = 0; i < 32; i++)
			VPQ_PR_DRV("HueByHS_0/1/2/3/4[%d]:%d %d %d %d %d\n",
				i,
				pcm_base_data->huebyhs_0[i], pcm_base_data->huebyhs_1[i],
				pcm_base_data->huebyhs_2[i], pcm_base_data->huebyhs_3[i],
				pcm_base_data->huebyhs_4[i]);
		for (i = 0; i < 32; i++)
			VPQ_PR_DRV("SatByHY_0/1/2/3/4[%d]:%d %d %d %d %d\n",
				i,
				pcm_base_data->satbyhy_0[i], pcm_base_data->satbyhy_1[i],
				pcm_base_data->satbyhy_2[i], pcm_base_data->satbyhy_3[i],
				pcm_base_data->satbyhy_4[i]);
	} else if (value == VPQ_DUMP_AIPQ) {
		VPQ_PR_DRV("dump aipq");
		VPQ_PR_DRV("height:%d width:%d\n", aipq_table.height, aipq_table.width);
		for (i = 0; i < aipq_table.height * aipq_table.width; i++)
			VPQ_PR_DRV("table_ptr[%d]:0x%x",
				i,
				((unsigned int *)aipq_table.table_ptr)[i]);
	} else if (value == VPQ_DUMP_AISR) {
		VPQ_PR_DRV("dump aisr");
		for (i = 0; i < 37; i++)
			VPQ_PR_DRV("aisr_parm[%d]:%d", i, aisr_parm.param[i]);
		for (i = 0; i < 11; i++)
			VPQ_PR_DRV("aisr_nn_parm[%d]:%d", i, aisr_nn_parm.param[i]);
	}

	return 1;
}
