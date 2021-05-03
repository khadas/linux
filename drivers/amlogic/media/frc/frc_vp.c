// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>

#include "frc_reg.h"
#include "frc_common.h"
#include "frc_vp.h"

void frc_vp_param_init(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *fw_data;
	struct st_vp_ctrl_para *g_stvpctrl_para;

	fw_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stvpctrl_para = &fw_data->g_stvpctrl_para;

	g_stvpctrl_para->vp_en                 = 0;
	g_stvpctrl_para->vp_ctrl_en            = 1;
	g_stvpctrl_para->global_oct_cnt_en     = 1;
	g_stvpctrl_para->global_oct_cnt_th     = 20;
	g_stvpctrl_para->region_oct_cnt_en     = 1;
	g_stvpctrl_para->region_oct_cnt_th     = 0;
	g_stvpctrl_para->global_dtl_cnt_en     = 1;
	g_stvpctrl_para->global_dtl_cnt_th     = 1000;
	g_stvpctrl_para->mv_activity_en        = 1;
	g_stvpctrl_para->mv_activity_th        = 2048;
	g_stvpctrl_para->global_t_consis_en    = 1;
	g_stvpctrl_para->global_t_consis_th    = 172032;//0x2a000
	g_stvpctrl_para->region_t_consis_en    = 1;
	g_stvpctrl_para->region_t_consis_th    = 28672; //3584*8
	g_stvpctrl_para->occl_mv_diff_en       = 1;
	g_stvpctrl_para->occl_mv_diff_th       = 172032;//0x2a000
	g_stvpctrl_para->occl_exist_most_en    = 1;
	g_stvpctrl_para->occl_exist_region_th  = 10;
	g_stvpctrl_para->occl_exist_most_th    = 8;

	g_stvpctrl_para->add_7_flag_en			= 0;
}

void frc_get_vp_gmv(struct frc_dev_s *frc_devp)
{
	u16 oct_total_cnt;//set u16 for divide
	u8  oct_cnt_th = 5;
	struct st_vp_rd *g_stvp_rd;
	struct st_me_rd *g_stme_rd;
	struct frc_fw_data_s *fw_data;

	fw_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stvp_rd = &fw_data->g_stvp_rd;
	g_stme_rd = &fw_data->g_stme_rd;

	oct_total_cnt = g_stvp_rd->glb_cover_cnt + g_stvp_rd->glb_uncov_cnt;

	UPDATE_FRC_REG_BITS(FRC_VP_GMV, (g_stme_rd->gmvx_mux) << 16, 0x1FFF0000);
	UPDATE_FRC_REG_BITS(FRC_VP_GMV, g_stme_rd->gmvy_mux, 0xFFF);

	if (oct_total_cnt <= oct_cnt_th) {
		UPDATE_FRC_REG_BITS(FRC_VP_OCT_GMV, 1 << 12, BIT_12);
		UPDATE_FRC_REG_BITS(FRC_VP_OCT_GMV, 0 << 16, 0x1FFF0000);
		UPDATE_FRC_REG_BITS(FRC_VP_OCT_GMV, 0, 0xFFF);
	} else {
		UPDATE_FRC_REG_BITS(FRC_VP_OCT_GMV, 0 << 12, BIT_12);
		UPDATE_FRC_REG_BITS(FRC_VP_OCT_GMV,
			(g_stvp_rd->oct_gmvx_sum / oct_total_cnt) << 16,
			0x1FFF0000);
		UPDATE_FRC_REG_BITS(FRC_VP_OCT_GMV,
			g_stvp_rd->oct_gmvy_sum / oct_total_cnt,
			0xFFF);
	}
	pr_frc(dbg_vp, "vp_gmv=[%d, %d]\n", (g_stme_rd->gmvx_mux), (g_stme_rd->gmvy_mux));
	pr_frc(dbg_vp, "oct_total_cnt=%d, oct_gmv=[%d, %d]\n",
		oct_total_cnt, g_stvp_rd->oct_gmvx_sum / oct_total_cnt,
		g_stvp_rd->oct_gmvy_sum / oct_total_cnt);
}

void frc_set_global_dehalo_en(u32 set_value)
{
	u8 k;

	for (k = 0; k < 12; k++)
		UPDATE_FRC_REG_BITS(FRC_VP_REGION_WINDOW_4,
			set_value << (k + 1),
			1 << (k + 1));
}

void frc_vp_ctrl(struct frc_dev_s *frc_devp)
{
	u32 k, occl_region_cnt = 0;
	struct st_vp_rd *g_stvp_rd;
	struct st_me_rd *g_stme_rd;
	struct st_vp_ctrl_para *g_stvpctrl_para;
	struct frc_fw_data_s *fw_data;

	fw_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stvp_rd = &fw_data->g_stvp_rd;
	g_stme_rd = &fw_data->g_stme_rd;
	g_stvpctrl_para = &fw_data->g_stvpctrl_para;

	frc_get_vp_gmv(frc_devp);
	frc_set_global_dehalo_en(1);

	if (g_stvpctrl_para->vp_ctrl_en == 1) {
		//rule1-- if global occl_cnt very small, then dehalo off
		if (((g_stvp_rd->glb_cover_cnt + g_stvp_rd->glb_uncov_cnt) <
			g_stvpctrl_para->global_oct_cnt_th) &&
			g_stvpctrl_para->global_oct_cnt_en == 1) {
			frc_set_global_dehalo_en(0);
			pr_frc(dbg_vp, "rule1 global_oct_cnt=%d\n",
				(g_stvp_rd->glb_cover_cnt + g_stvp_rd->glb_uncov_cnt));
			pr_frc(dbg_vp, "rule1 global dehalo off\n");
		}

		//rule1-- if 12 regions occl_cnt very small, then dehalo off
		if (g_stvpctrl_para->region_oct_cnt_en == 1) {
			for (k = 0; k < 12; k++) {
				if ((g_stvp_rd->region_cover_cnt[k] +
					g_stvp_rd->region_uncov_cnt[k]) <
					g_stvpctrl_para->region_oct_cnt_th) {
					UPDATE_FRC_REG_BITS(FRC_VP_REGION_WINDOW_4,
						0, 1 << (k + 1));
					pr_frc(dbg_vp, "rule1 region[%d]_oct_cnt=%d\n",
						k, (g_stvp_rd->region_cover_cnt[k] +
						g_stvp_rd->region_uncov_cnt[k]));
					pr_frc(dbg_vp, "rule1 region[%d] dehalo off\n", k);
				}
			}
		}

		//rule2-- if global dtl_cnt very few, then dehalo off
		if (g_stme_rd->gmv_dtl_cnt < g_stvpctrl_para->global_dtl_cnt_th &&
			g_stvpctrl_para->global_dtl_cnt_en == 1) {
			frc_set_global_dehalo_en(0);
			pr_frc(dbg_vp, "rule2 gmv_dtl_cnt=%d\n", g_stme_rd->gmv_dtl_cnt);
			pr_frc(dbg_vp, "rule2 global dehalo off\n");
		}

		//rule3-- if mv activity very high, then dehalo off
		if (g_stme_rd->glb_act_hi_x_cnt + g_stme_rd->glb_act_hi_y_cnt >
			g_stvpctrl_para->mv_activity_th &&
			g_stvpctrl_para->mv_activity_en == 1) {
			frc_set_global_dehalo_en(0);
			pr_frc(dbg_vp, "rule3 glb_act_hi_cnt=%d\n",
				g_stme_rd->glb_act_hi_x_cnt + g_stme_rd->glb_act_hi_y_cnt);
			pr_frc(dbg_vp, "rule3 global dehalo off\n");
		}

		//rule4-- if global temporal consistence very high, then dehalo off
		if (g_stme_rd->glb_t_consis[0] >
			g_stvpctrl_para->global_t_consis_th &&
			g_stvpctrl_para->global_t_consis_en == 1) {
			frc_set_global_dehalo_en(0);
			pr_frc(dbg_vp, "rule4 glb_t_consis=%d\n", g_stme_rd->glb_t_consis[0]);
			pr_frc(dbg_vp, "rule4 global dehalo off\n");
		}

		//rule5-- if 12 regions temporal consistence very high, then dehalo off
		if (g_stvpctrl_para->region_t_consis_en == 1) {
			for (k = 0; k < 12; k++) {
				if (g_stme_rd->region_t_consis[k] >
					g_stvpctrl_para->region_t_consis_th) {
					UPDATE_FRC_REG_BITS(FRC_VP_REGION_WINDOW_4,
						0, 1 << (k + 1));
					pr_frc(dbg_vp, "rule5 region[%d]_t_consis=%d\n",
						k, g_stme_rd->region_t_consis[k]);
					pr_frc(dbg_vp, "rule5 region[%d] dehalo off\n", k);
				}
			}
		}

		//rule6-- if occl mv difference very high, then dehalo off
		if (g_stvp_rd->oct_gmv_diff > g_stvpctrl_para->occl_mv_diff_th &&
			g_stvpctrl_para->occl_mv_diff_en == 1) {
			frc_set_global_dehalo_en(0);
			pr_frc(dbg_vp, "rule6 oct_gmv_diff=%d\n", g_stvp_rd->oct_gmv_diff);
			pr_frc(dbg_vp, "rule6 global dehalo off\n");
		}

		//rule7-- if occl exist in most of regions, then dehalo off
		if (g_stvpctrl_para->occl_exist_most_en == 1) {
			for (k = 0; k < 12; k++) {
				occl_region_cnt += ((g_stvp_rd->region_cover_cnt[k] +
							g_stvp_rd->region_uncov_cnt[k]) >
							g_stvpctrl_para->occl_exist_region_th);
			}
			if (occl_region_cnt > g_stvpctrl_para->occl_exist_most_th) {
				frc_set_global_dehalo_en(0);
				pr_frc(dbg_vp, "rule7 global dehalo off\n");
			}
		}
	}

	if (g_stvpctrl_para->add_7_flag_en == 1)
		frc_vp_add_7_seg();
}

void frc_vp_add_7_seg(void)
{
	s32 vp_gmvx, vp_gmvy;
	u16 cnt, cnt1k, cnt100, cnt10, sign_flag;
	u8  region_en[12], k;

	vp_gmvx = READ_FRC_REG(FRC_VP_GMV) >> 16 & 0x1FFF;
	vp_gmvy = READ_FRC_REG(FRC_VP_GMV) & 0xFFF;
	vp_gmvx = negative_convert(vp_gmvx, 13);
	vp_gmvy	= negative_convert(vp_gmvy, 12);

	for (k = 0; k < 12; k++)
		region_en[k] = READ_FRC_REG(FRC_VP_REGION_WINDOW_4) >> (k + 1) & 0x1;

	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, 1 << 15, BIT_15);//flag1_1_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, 1 << 7, BIT_7);//flag1_2_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, 1 << 31, BIT_31);//flag1_3_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, 1 << 23, BIT_23);//flag1_4_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, 1 << 15, BIT_15);//flag1_5_en

	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, 1 << 15, BIT_15);//flag2_1_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, 1 << 7, BIT_7);//flag2_2_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, 1 << 31, BIT_31);//flag2_3_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, 1 << 23, BIT_23);//flag2_4_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, 1 << 15, BIT_15);//flag2_5_en

	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM31_NUM32, 1 << 15, BIT_15);//flag3_1_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM31_NUM32, 1 << 7, BIT_7);//flag3_2_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM33_NUM34_NUM35_NUM36, 1 << 31, BIT_31);//flag3_3_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM33_NUM34_NUM35_NUM36, 1 << 23, BIT_23);//flag3_4_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM33_NUM34_NUM35_NUM36, 1 << 15, BIT_15);//flag3_5_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM33_NUM34_NUM35_NUM36, 1 << 7,  BIT_7);//flag3_6_en

	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM41_NUM42, 1 << 15, BIT_15);//flag4_1_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM41_NUM42, 1 << 7, BIT_7);//flag4_2_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 1 << 31, BIT_31);//flag4_3_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 1 << 23, BIT_23);//flag4_4_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 1 << 15, BIT_15);//flag4_5_en
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 1 << 7,  BIT_7);//flag4_6_en

	sign_flag = vp_gmvx >= 0 ? 0 : 1;
	vp_gmvx = ABS(vp_gmvx);
	cnt	 = vp_gmvx;
	cnt1k	 = cnt / 1000;
	cnt	 = (cnt - cnt1k * 1000);
	cnt100	 = cnt / 100;
	cnt	 = (cnt - cnt100 * 100);
	cnt10	 = cnt / 10;
	cnt	 = (cnt - cnt10 * 10);

	//flag1 num1-num5
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, sign_flag << 8, 0xF00);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, cnt1k, 0xF);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, cnt100 << 24, 0xF000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, cnt10 << 16, 0xF0000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, cnt << 8, 0xF00);
	//  color
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, 7 << 12, 0x7000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, 7 << 4, 0x70);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, 7 << 28, 0x70000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, 7 << 20, 0x700000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, 7 << 12, 0x7000);

	sign_flag = vp_gmvy >= 0 ? 0 : 1;
	vp_gmvy = ABS(vp_gmvy);
	cnt	 = vp_gmvy;
	cnt1k	 = cnt / 1000;
	cnt	 = (cnt - cnt1k * 1000);
	cnt100	 = cnt / 100;
	cnt	 = (cnt - cnt100 * 100);
	cnt10	 = cnt / 10;
	cnt	 = (cnt - cnt10 * 10);

	// flag2 num1-num5
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, sign_flag << 8, 0xF00);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, cnt1k, 0xF);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, cnt100 << 24, 0xF000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, cnt10 << 16, 0xF0000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, cnt << 8, 0xF00);
	//  color
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, 7 << 12, 0x7000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22, 7 << 4, 0x70);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, 7 << 28, 0x70000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, 7 << 20, 0x700000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26, 7 << 12, 0x7000);

	//region dehalo enable, flag3 num1-num6
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM31_NUM32, region_en[0] << 8, 0xF00);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM31_NUM32, region_en[1], 0xF);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM33_NUM34_NUM35_NUM36,
				region_en[2] << 24, 0xF000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM33_NUM34_NUM35_NUM36, region_en[3] << 16, 0xF0000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM33_NUM34_NUM35_NUM36, region_en[4] << 8, 0xF00);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM33_NUM34_NUM35_NUM36, region_en[5], 0xF);
	//color
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM31_NUM32, 7 << 12, 0x7000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM31_NUM32, 7 << 4, 0x70);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM33_NUM34_NUM35_NUM36, 7 << 28, 0x70000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM33_NUM34_NUM35_NUM36, 7 << 20, 0x700000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM33_NUM34_NUM35_NUM36, 7 << 12, 0x7000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM33_NUM34_NUM35_NUM36, 7 << 4,  0x70);

	//region dehalo enable, flag4 num1-num6
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM41_NUM42, region_en[6] << 8, 0xF00);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM41_NUM42, region_en[7], 0xF);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46,
				region_en[8] << 24, 0xF000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, region_en[9] << 16, 0xF0000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, region_en[10] << 8, 0xF00);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, region_en[11], 0xF);
	//color
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM41_NUM42, 7 << 12, 0x7000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM41_NUM42, 7 << 4, 0x70);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 7 << 28, 0x70000000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 7 << 20, 0x700000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 7 << 12, 0x7000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM43_NUM44_NUM45_NUM46, 7 << 4,  0x70);
}

