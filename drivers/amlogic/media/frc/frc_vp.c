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
	g_stvpctrl_para->global_oct_cnt_th     = 100;
	g_stvpctrl_para->region_oct_cnt_en     = 1;
	g_stvpctrl_para->region_oct_cnt_th     = 1000;
	g_stvpctrl_para->global_dtl_cnt_en     = 1;
	g_stvpctrl_para->global_dtl_cnt_th     = 1000;
	g_stvpctrl_para->mv_activity_en        = 1;
	g_stvpctrl_para->mv_activity_th        = 1000;
	g_stvpctrl_para->global_t_consis_en    = 1;
	g_stvpctrl_para->global_t_consis_th    = 43008;//0x2a000>>2
	g_stvpctrl_para->region_t_consis_en    = 1;
	g_stvpctrl_para->region_t_consis_th    = 3584;
	g_stvpctrl_para->occl_mv_diff_en       = 1;
	g_stvpctrl_para->occl_mv_diff_th       = 43008;//0x2a000>>2
	g_stvpctrl_para->occl_exist_most_en    = 1;
	g_stvpctrl_para->occl_exist_region_th  = 10;
	g_stvpctrl_para->occl_exist_most_th    = 8;
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

	if (g_stvpctrl_para->vp_ctrl_en == 1) {
		//rule1-- if global occl_cnt very small, then dehalo off
		if (((g_stvp_rd->glb_cover_cnt + g_stvp_rd->glb_uncov_cnt) <
			g_stvpctrl_para->global_oct_cnt_th) &&
			g_stvpctrl_para->global_oct_cnt_en == 1)
			frc_set_global_dehalo_en(0);

		//rule1-- if 12 regions occl_cnt very small, then dehalo off
		if (g_stvpctrl_para->region_oct_cnt_en == 1) {
			for (k = 0; k < 12; k++) {
				if ((g_stvp_rd->region_cover_cnt[k] +
					g_stvp_rd->region_uncov_cnt[k]) <
					g_stvpctrl_para->region_oct_cnt_th)
					UPDATE_FRC_REG_BITS(FRC_VP_REGION_WINDOW_4,
						0, 1 << (k + 1));
			}
		}

		//rule2-- if global dtl_cnt very few, then dehalo off
		if (g_stme_rd->gmv_dtl_cnt < g_stvpctrl_para->global_dtl_cnt_th &&
			g_stvpctrl_para->global_dtl_cnt_en == 1)
			frc_set_global_dehalo_en(0);

		//rule3-- if mv activity very high, then dehalo off
		if (g_stme_rd->glb_act_hi_x_cnt + g_stme_rd->glb_act_hi_y_cnt >
			g_stvpctrl_para->mv_activity_th &&
			g_stvpctrl_para->mv_activity_en == 1)
			frc_set_global_dehalo_en(0);

		//rule4-- if global temporal consistence very high, then dehalo off
		if (g_stme_rd->glb_t_consis[0] >
			g_stvpctrl_para->global_t_consis_th &&
			g_stvpctrl_para->global_t_consis_en == 1)
			frc_set_global_dehalo_en(0);

		//rule5-- if 12 regions temporal consistence very high, then dehalo off
		if (g_stvpctrl_para->region_t_consis_en == 1) {
			for (k = 0; k < 12; k++) {
				if (g_stme_rd->region_t_consis[k] >
					g_stvpctrl_para->region_t_consis_th)
					UPDATE_FRC_REG_BITS(FRC_VP_REGION_WINDOW_4,
						0, 1 << (k + 1));
			}
		}

		//rule6-- if occl mv difference very high, then dehalo off
		if (g_stvp_rd->oct_gmv_diff > g_stvpctrl_para->occl_mv_diff_th &&
			g_stvpctrl_para->occl_mv_diff_en == 1)
			frc_set_global_dehalo_en(0);

		//rule7-- if occl exist in most of regions, then dehalo off
		if (g_stvpctrl_para->occl_exist_most_en == 1) {
			for (k = 0; k < 12; k++) {
				occl_region_cnt +=
					((g_stvp_rd->region_cover_cnt[k] +
					g_stvp_rd->region_uncov_cnt[k]) >
					g_stvpctrl_para->occl_exist_region_th);
			}
			if (occl_region_cnt > g_stvpctrl_para->occl_exist_most_th)
				frc_set_global_dehalo_en(0);
		}
	}
}
