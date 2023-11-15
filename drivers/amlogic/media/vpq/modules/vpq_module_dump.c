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
		VPQ_PR_DRV("dump vadj1\n");
		VPQ_PR_DRV("brig/cont/sat/hue:%d %d %d %d\n",
			pic_mode_item.data[VPQ_ITEM_BRIGHTNESS],
			pic_mode_item.data[VPQ_ITEM_CONTRAST],
			pic_mode_item.data[VPQ_ITEM_CONTRAST],
			pic_mode_item.data[VPQ_ITEM_HUE]);
	} else if (value == VPQ_DUMP_GAMMA) {
		VPQ_PR_DRV("dump gamma\n");
		for (i = 0; i < VPQ_GAMMA_TABLE_LEN; i++)
			VPQ_PR_DRV("gma_r/g/b[%d]:%d %d %d\n",
				i,
				vpp_gma_table.r_data[i],
				vpp_gma_table.g_data[i],
				vpp_gma_table.b_data[i]);
	} else if (value == VPQ_DUMP_WB) {
		VPQ_PR_DRV("dump wb\n");
		VPQ_PR_DRV("rgb_ogo:%d %d %d %d %d %d %d %d %d\n",
			rgb_ogo.r_pre_offset, rgb_ogo.g_pre_offset,
			rgb_ogo.b_pre_offset, rgb_ogo.r_gain,
			rgb_ogo.g_gain, rgb_ogo.b_gain,
			rgb_ogo.r_post_offset, rgb_ogo.g_post_offset,
			rgb_ogo.b_post_offset);
	} else if (value == VPQ_DUMP_DNLP) {
		VPQ_PR_DRV("dump dnlp\n");
		VPQ_PR_DRV("dnlp_enable: %d\n", dnlp_enable);
		for (i = 0; i < VPP_DNLP_SCURV_LEN; i++)
			VPQ_PR_DRV("dnlp_scurv_low/mid1/mid2/hgh1/hgh2[%d]:%d %d %d %d %d\n",
				i,
				pdnlp_data->dnlp_scurv_low[i],
				pdnlp_data->dnlp_scurv_mid1[i],
				pdnlp_data->dnlp_scurv_mid2[i],
				pdnlp_data->dnlp_scurv_hgh1[i],
				pdnlp_data->dnlp_scurv_hgh2[i]);
		for (i = 0; i < VPP_DNLP_GAIN_VAR_LUT_LEN; i++)
			VPQ_PR_DRV("gain_var_lut49[%d]:%d\n", i, pdnlp_data->gain_var_lut49[i]);
		for (i = 0; i < VPP_DNLP_WEXT_GAIN_LEN; i++)
			VPQ_PR_DRV("wext_gain[%d]:%d\n", i, pdnlp_data->wext_gain[i]);
		for (i = 0; i < VPP_DNLP_ADP_THRD_LEN; i++)
			VPQ_PR_DRV("adp_thrd[%d]:%d\n", i, pdnlp_data->adp_thrd[i]);
		for (i = 0; i < VPP_DNLP_REG_BLK_BOOST_LEN; i++)
			VPQ_PR_DRV("reg_blk_boost_12[%d]:%d\n", i, pdnlp_data->reg_blk_boost_12[i]);
		for (i = 0; i < VPP_DNLP_REG_ADP_OFSET_LEN; i++)
			VPQ_PR_DRV("reg_adp_ofset_20[%d]:%d\n", i, pdnlp_data->reg_adp_ofset_20[i]);
		for (i = 0; i < VPP_DNLP_REG_MONO_PROT_LEN; i++)
			VPQ_PR_DRV("reg_mono_protect[%d]:%d\n", i, pdnlp_data->reg_mono_protect[i]);
		for (i = 0; i < VPP_DNLP_TREND_WHT_EXP_LUT_LEN; i++)
			VPQ_PR_DRV("reg_trend_wht_expand_lut8[%d]:%d\n",
				i, pdnlp_data->reg_trend_wht_expand_lut8[i]);
		for (i = 0; i < VPP_DNLP_HIST_GAIN_LEN; i++)
			VPQ_PR_DRV("c/s_hist_gain[%d]:%d %d\n",
				i, pdnlp_data->c_hist_gain[i], pdnlp_data->s_hist_gain[i]);
		for (i = 0; i < EN_DNLP_PARAM_MAX; i++)
			VPQ_PR_DRV("param[%d]:%d\n", i, pdnlp_data->param[i]);
	} else if (value == VPQ_DUMP_LC) {
		VPQ_PR_DRV("dump lc\n");
		VPQ_PR_DRV("lc_enable: %d\n", lc_enable);
		for (i = 0; i < 63; i++)
			VPQ_PR_DRV("lc_saturation[%d]:%d\n", i, lc_curve.lc_saturation[i]);
		for (i = 0; i < 16; i++) {
			VPQ_PR_DRV("lc_yminval/ypkbv_ymaxval/ymaxval/ypkbv_lmt[%d]:%d %d %d %d\n",
				i,
				lc_curve.lc_yminval_lmt[i],
				lc_curve.lc_ypkbv_ymaxval_lmt[i],
				lc_curve.lc_ymaxval_lmt[i],
				lc_curve.lc_ypkbv_lmt[i]);
		}
		for (i = 0; i < 4; i++)
			VPQ_PR_DRV("lc_ypkbv_ratio[%d]:%d\n", i, lc_curve.lc_ypkbv_ratio[i]);
		for (i = 0; i < 18; i++)
			VPQ_PR_DRV("lc_param.param[%d]:%d\n", i, lc_param.param[i]);
	} else if (value == VPQ_DUMP_BLE) {
		VPQ_PR_DRV("dump ble\n");
		VPQ_PR_DRV("blackext_enable/start/slope1/mid_point/slope2: %d %d %d %d %d\n",
			blkext_en, pble_data[0], pble_data[1], pble_data[2], pble_data[3]);
	} else if (value == VPQ_DUMP_BLS) {
		VPQ_PR_DRV("dump bls\n");
		VPQ_PR_DRV("bluestretch_enable: %d\n", blue_str_en);
		for (i = 0; i < 18; i++)
			VPQ_PR_DRV("pbls_data[%d]:%d\n", i, pbls_data[i]);
	} else if (value == VPQ_DUMP_CM) {
		VPQ_PR_DRV("dump cm2 base curve\n");
		for (i = 0; i < SATBYY_CURVE_LENGTH * SATBYY_CURVE_NUM; i++)
			VPQ_PR_DRV("satbyy_base_curve[%d]:%d\n", i, satbyy_base_curve[i]);
		for (i = 0; i < LUMABYHUE_CURVE_LENGTH * LUMABYHUE_CURVE_NUM; i++)
			VPQ_PR_DRV("lumabyhue_base_curve[%d]:%d\n", i, lumabyhue_base_curve[i]);
		for (i = 0; i < SATBYHS_CURVE_LENGTH * SATBYHS_CURVE_NUM; i++)
			VPQ_PR_DRV("satbyhs_base_curve[%d]:%d\n", i, satbyhs_base_curve[i]);
		for (i = 0; i < HUEBYHUE_CURVE_LENGTH * HUEBYHUE_CURVE_NUM; i++)
			VPQ_PR_DRV("huebyhue_base_curve[%d]:%d\n", i, huebyhue_base_curve[i]);
		for (i = 0; i < HUEBYHY_CURVE_LENGTH * HUEBYHY_CURVE_NUM; i++)
			VPQ_PR_DRV("huebyhy_base_curve[%d]:%d\n", i, huebyhy_base_curve[i]);
		for (i = 0; i < HUEBYHS_CURVE_LENGTH * HUEBYHS_CURVE_NUM; i++)
			VPQ_PR_DRV("huebyhs_base_curve[%d]:%d\n", i, huebyhs_base_curve[i]);
		for (i = 0; i < SATBYHY_CURVE_LENGTH * SATBYHY_CURVE_NUM; i++)
			VPQ_PR_DRV("satbyhy_base_curve[%d]:%d\n", i, satbyhy_base_curve[i]);

		VPQ_PR_DRV("!!!!!!!!!!!!!!!!!!!!!!!!\n");
		VPQ_PR_DRV("dump cms customize curve\n");
		for (i = 0; i < SATBYHS_CURVE_LENGTH * SATBYHS_CURVE_NUM; i++)
			VPQ_PR_DRV("satbyhs_curve[%d]:%d\n", i, satbyhs_curve[i]);
		for (i = 0; i < HUEBYHUE_CURVE_LENGTH * HUEBYHUE_CURVE_NUM; i++)
			VPQ_PR_DRV("huebyhue_curve[%d]:%d\n", i, huebyhue_curve[i]);
		for (i = 0; i < LUMABYHUE_CURVE_LENGTH * LUMABYHUE_CURVE_NUM; i++)
			VPQ_PR_DRV("lumabyhue_curve[%d]:%d\n", i, lumabyhue_curve[i]);
	} else if (value == VPQ_DUMP_TMO) {
		VPQ_PR_DRV("dump tmo\n");
		VPQ_PR_DRV("%d %d %d %d %d %d %d %d %d %d\n",
			tmo_param.tmo_en, tmo_param.reg_highlight,
			tmo_param.reg_hist_th, tmo_param.reg_light_th,
			tmo_param.reg_highlight_th1, tmo_param.reg_highlight_th2,
			tmo_param.reg_display_e, tmo_param.reg_middle_a,
			tmo_param.reg_middle_a_adj, tmo_param.reg_middle_b);
		VPQ_PR_DRV("%d %d %d %d %d %d %d %d %d %d\n",
			tmo_param.reg_middle_s, tmo_param.reg_max_th1,
			tmo_param.reg_middle_th, tmo_param.reg_thold1,
			tmo_param.reg_thold2, tmo_param.reg_thold3,
			tmo_param.reg_thold4, tmo_param.reg_max_th2,
			tmo_param.reg_pnum_th, tmo_param.reg_hl0);
		VPQ_PR_DRV("%d %d %d %d %d %d %d %d %d %d\n",
			tmo_param.reg_hl1, tmo_param.reg_hl2,
			tmo_param.reg_hl3, tmo_param.reg_display_adj,
			tmo_param.reg_avg_th, tmo_param.reg_avg_adj,
			tmo_param.reg_low_adj, tmo_param.reg_high_en,
			tmo_param.reg_high_adj1, tmo_param.reg_high_adj2);
		VPQ_PR_DRV("%d %d %d\n",
			tmo_param.reg_high_maxdiff, tmo_param.reg_high_mindiff,
			tmo_param.alpha);
	} else if (value == VPQ_DUMP_AIPQ) {
		VPQ_PR_DRV("dump aipq\n");
		VPQ_PR_DRV("height:%d width:%d\n", aipq_table.height, aipq_table.width);
		for (i = 0; i < aipq_table.height * aipq_table.width; i++)
			VPQ_PR_DRV("table_ptr[%d]:0x%x\n",
				i,
				((unsigned int *)aipq_table.table_ptr)[i]);
	} else if (value == VPQ_DUMP_AISR) {
		VPQ_PR_DRV("dump aisr\n");
		for (i = 0; i < 37; i++)
			VPQ_PR_DRV("aisr_parm[%d]:%d\n", i, aisr_parm.param[i]);
		for (i = 0; i < 11; i++)
			VPQ_PR_DRV("aisr_nn_parm[%d]:%d\n", i, aisr_nn_parm.param[i]);
	} else if (value == VPQ_DUMP_CCORING) {
		VPQ_PR_DRV("dump chroma coring\n");
		VPQ_PR_DRV("enable/slope/threshold/y_threshold: %d %d %d %d\n",
			ccoring_en, pccoring_data[0], pccoring_data[1], pccoring_data[2]);
	}

	return 1;
}
