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
#include "frc_drv.h"
#include "frc_film.h"
#include "frc_buf.h"
#include "frc_phase_table.h"

static const struct st_film_table_item g_stfilmtable[] =
{
	// 1.film_mdoe, 2.cadence_cnt, 3.enter_th, 4.match_data,
	// 5.cycle_num, 6.average_num, 7.quit_th, 8.mask_value
	// 9.step_ofset, 10.delay_num, 11.max_zeros_cnt, 12. jump_id_flag,
	// 13. simple_jump_add_buf, 14.mode_enter_level
	{1,       1,   1,    0x1,    1,   20,  1,  0x1,		4096, 0, 1 , 0, 0, 0},
	{22,      2,   16,   0x2,    5,   20,  4, 0x3,		2040, 1, 1 , 0, 0, 1},
	{32,      5,   15,   0x14,   2,   10,  5, 0x1f,	    810,  0, 2 , 0, 0, 1},
	{3223,    10,  20,   0x2A4,  2,   10,  8, 0x3ff,	810,  0, 2 , 0, 0, 0},
	{2224,    10,  20,   0x2A8,  1,   10,  8, 0x3ff,	810,  0, 3 , 0, 0, 2},
	{32322,   12,  12,   0xA94,  1,   12,  9, 0xfff,	810,  0, 2 , 0, 0, 0},
	{44,      4,   20,   0x8,    2,   12,  5, 0xf,		810,  0, 3 , 0, 0, 3},
	{21111,   6,   12,   0x3E,   1,   12,  3, 0x3f,	    810,  0, 1 , 0, 0, 0},
	{23322,   12,  24,   0xAA4,  1,   24,  7, 0xfff,	810,  0, 2 , 0, 0, 0},
	{2111,    5,   20,   0x1E,   1,   10,  3, 0x1f,	    810,  0, 1 , 0, 0, 0},
	{22224,   12,  24,   0xAA8,  1,   12,  9, 0xfff,	810,  0, 3 , 0, 0, 2},
	{33,      3,   12,   0x4,    2,   12,  4, 0x7,		810,  0, 2 , 0, 0, 1},
	{334,     10,  30,   0x248,  1,   10,  9, 0x3ff,	810,  0, 3 , 0, 0, 0},
	{55,      5,   15,   0x10,   2,   10,  7, 0x1f,	    810,  0, 4 , 0, 0, 1},
	{64,      10,  30,   0x220,  1,   10,  10, 0x3ff,	810,  0, 5 , 0, 0, 2},
	{66,      6,   18,   0x20,   2,   12,  7, 0x3f,	    810,  0, 5 , 1, 8, 1},
	{87,      15,  45,   0x4080, 1,   15,  17, 0x7fff,	810,  0, 7 , 1, 1, 1},
	{212,     5,   15,   0x1A,   1,   10,  4, 0x1f,	    810,  0, 1 , 0, 0, 0},
};

void frc_film_param_init(struct frc_dev_s *frc_devp)
{
	u8 nT0,fb_num;
	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	struct st_film_data_item *g_stfilmdata_item;
	struct st_film_ctrl_para *g_stfilm_ctrl_para;
	u32 input_n = 2, output_m = 5;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	g_stfilm_ctrl_para =  &frc_data->g_stfilmctrl_para;

	g_stfilm_ctrl_para->film_ctrl_en        = 1;
	g_stfilm_ctrl_para->film_check_mode_en  = 1;
	g_stfilm_ctrl_para->film_badedit_en     = 0;
	g_stfilm_ctrl_para->film_7_seg_en      = 0;
	g_stfilm_ctrl_para->film_mode_force_en         = 0;
	g_stfilm_ctrl_para->film_mode_force_value      = 0;
	g_stfilm_ctrl_para->film_mode_force_phs_en      = 0;
	g_stfilm_ctrl_para->film_cadence_switch = 0x6ff;

	g_stfilm_ctrl_para->glb_ratio        = 14;
	g_stfilm_ctrl_para->wind_ratio       = 10;
	g_stfilm_ctrl_para->glb_ofset        = 1024;
	g_stfilm_ctrl_para->wind_ofset       = 100;
	g_stfilm_ctrl_para->min_diff_th      = 2000;

	g_stfilm_detect_item->quit_reset_th    = 10;
	g_stfilm_detect_item->phase_error_flag = 0;
	g_stfilm_detect_item->force_phase      = 0;

	//badedit
	if (frc_devp->dbg_force_en && frc_devp->dbg_in_out_ratio) {
		input_n = (frc_devp->dbg_in_out_ratio >> 8) & 0xff;
		output_m = frc_devp->dbg_in_out_ratio & 0xff;
		WRITE_FRC_BITS(FRC_REG_PHS_TABLE, input_n, 24, 8);
		WRITE_FRC_BITS(FRC_REG_PHS_TABLE, output_m, 16, 8);
	}

	g_stfilm_detect_item->input_n    = (READ_FRC_REG(FRC_REG_PHS_TABLE) >> 24) & 0xFF;
	g_stfilm_detect_item->output_m   = (READ_FRC_REG(FRC_REG_PHS_TABLE) >> 16) & 0xFF;
	g_stfilm_detect_item->mode_shift = 0;
	g_stfilm_detect_item->step =
		(1 << PHS_FRAC_BIT) * g_stfilm_detect_item->input_n / g_stfilm_detect_item->output_m;
	g_stfilm_detect_item->pre_table_point = 0;
	g_stfilm_detect_item->cur_table_point = 1;
	g_stfilm_detect_item->nex_table_point = 2;
	g_stfilm_detect_item->pre_idx         = 0;
	g_stfilm_detect_item->cur_idx         = 0;
	g_stfilm_detect_item->nex_idx         = 0;
	g_stfilm_detect_item->otb_start       = 1;

	g_stfilm_detect_item->input_id_point  = 3;

	// mix mode
	g_stfilm_ctrl_para->mm_cown_thd       = 5;
	g_stfilm_ctrl_para->mm_cpre_thd       = 5;
	g_stfilm_ctrl_para->mm_cother_thd     = 5;
	g_stfilm_ctrl_para->mm_reset_thd      = 5;
	g_stfilm_ctrl_para->mm_difminthd      = 5;
	g_stfilm_ctrl_para->mm_chk_mmdifthd   = 5;

	for (nT0 = 0; nT0 < 6; nT0++) {
		g_stfilmdata_item->mm_num_cown[nT0]      = 0;
		g_stfilmdata_item->mm_num_cother[nT0]    = 0;
		g_stfilmdata_item->mm_num_cpre[nT0]      = 0;
		g_stfilmdata_item->mm_num_reset[nT0]     = 0;
		g_stfilmdata_item->mm_num_cpre_quit[nT0] = 0;
	}
	// mix_mode init end
	fb_num = READ_FRC_REG(FRC_FRAME_BUFFER_NUM) & 0x1F;
	g_stfilm_detect_item->input_id_length       = fb_num;
	g_stfilm_detect_item->frame_buf_num         = fb_num;
	g_stfilm_detect_item->mode_change_adjust_en = 1;
	g_stfilm_detect_item->jump_simple_mode      = 1;


	for (nT0 = 0; nT0 < FLMMODNUM; nT0++) {
		g_stfilmdata_item->ModCount[nT0]  = 0;
		g_stfilmdata_item->phase[nT0]     = 0;
		g_stfilmdata_item->phase_new[nT0] = 0;
	}

	for (nT0 = 0; nT0 < 2; nT0++)
		g_stfilmdata_item->quitCount[nT0] = 0;

	for (nT0 = 0; nT0 < 2; nT0++)
		g_stfilmdata_item->quit_reset_Cnt[nT0] = 0;

	//badedit
	for (nT0 = 0; nT0 < fb_num; nT0++) {
		g_stfilmdata_item->frame_buffer_flag[nT0] = 0;
		g_stfilmdata_item->input_id_table[nT0]    = nT0;
		g_stfilmdata_item->frame_buffer_id[nT0]   = nT0;
	}

	for(nT0 = 0; nT0 < fb_num; nT0++)
		g_stfilmdata_item->inout_id_table[nT0][0] = nT0 | (1 << 4);
}

void frc_in_out_ratio_init(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	struct st_film_data_item *g_stfilmdata_item;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	g_stfilmdata_item = &frc_data->g_stfilmdata_item;

	// set reg_inp_frm_vld_lut  accroding to different  input_n : ouput_m

	if (g_stfilm_detect_item->input_n == 1 && g_stfilm_detect_item->output_m == 1)
		UPDATE_FRC_REG_BITS(FRC_REG_TOP_CTRL18, 1, 0xFFFFFFFF );
	else if (g_stfilm_detect_item->input_n == 1 && g_stfilm_detect_item->output_m == 2)
		UPDATE_FRC_REG_BITS(FRC_REG_TOP_CTRL18, 2, 0xFFFFFFFF );
	else if (g_stfilm_detect_item->input_n == 2 && g_stfilm_detect_item->output_m == 3)
		UPDATE_FRC_REG_BITS(FRC_REG_TOP_CTRL18, 6, 0xFFFFFFFF );
	else if (g_stfilm_detect_item->input_n == 5 && g_stfilm_detect_item->output_m == 6)
		UPDATE_FRC_REG_BITS(FRC_REG_TOP_CTRL18, 0x37, 0xFFFFFFFF );
	else if (g_stfilm_detect_item->input_n == 2 && g_stfilm_detect_item->output_m == 5)
		UPDATE_FRC_REG_BITS(FRC_REG_TOP_CTRL18, 9, 0xFFFFFFFF );
}


void frc_glb_dif_init(struct frc_dev_s *frc_devp, u32 *glb_dif,
			u32 win6_glb_dif[][HISDIFNUM], u32 win12_glb_dif1[][HISDIFNUM],
			u32 win12_glb_dif2[][HISDIFNUM])
{
	u8 nT0,nT1;
	static u8 BN = 0;

	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	struct st_film_data_item *g_stfilmdata_item;
	struct st_film_detect_rd *g_stfilmdetect_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	g_stfilmdetect_rd = &frc_data->g_stfilmdetect_rd;

	if (BN == 0) {
		for (nT0 = 0; nT0 < HISDIFNUM; nT0++) {
			for(nT1 = 0;nT1 < 6; nT1++)
				win6_glb_dif[nT1][nT0] = 0;

			for(nT1 = 0; nT1 < 12; nT1++) {
				win12_glb_dif1[nT1][nT0] = 0;
				win12_glb_dif2[nT1][nT0] = 0;
			}
			glb_dif[nT0] = 0;
		}
		BN = 1;
	} else {
		for (nT0 = 1; nT0 < HISDIFNUM; nT0++) {
			for (nT1 = 0; nT1 < 6; nT1++)
				win6_glb_dif[nT1][nT0 - 1] = win6_glb_dif[nT1][nT0];

			for (nT1 = 0; nT1 < 12; nT1++) {
				win12_glb_dif1[nT1][nT0 - 1] = win12_glb_dif1[nT1][nT0];
				win12_glb_dif2[nT1][nT0 - 1] = win12_glb_dif2[nT1][nT0];
			}
			glb_dif[nT0 - 1] = glb_dif[nT0];
		}
	}

	//  flmGDif  read only
	glb_dif[HISDIFNUM - 1] = g_stfilmdetect_rd->glb_motion_all;

	// flmW6Dif 6/12 window read only
	for (nT1 = 0; nT1 < 6; nT1++)
		win6_glb_dif[nT1][HISDIFNUM - 1] = g_stfilmdetect_rd->region_motion_wind6[nT1];

	for (nT1 = 0; nT1 < 12; nT1++)
		win12_glb_dif1[nT1][HISDIFNUM - 1] = g_stfilmdetect_rd->region1_motion_wind12[nT1];

	for (nT1 = 0; nT1 < 12; nT1++)
		win12_glb_dif2[nT1][HISDIFNUM - 1] = g_stfilmdetect_rd->region2_motion_wind12[nT1];
}

void frc_film_detect_ctrl(struct frc_dev_s *frc_devp)
{
	static u32  flmGDif[HISDIFNUM];                       // global dif
	static u32  flmW6Dif[6][HISDIFNUM];                   //window6 dif
	static u32  flmW12Dif1[12][HISDIFNUM];                //window12 dif1
	static u32  flmW12Dif2[12][HISDIFNUM];                //window12 dif2
	static u8   mode_pre = 0;                             //initial video mode

	//u8 nT0, nT1, i;
	u8 mode;
	u32 mem;

	u8 fwhw_sel = READ_FRC_REG(FRC_REG_FILM_PHS_1) >> 16 & 0x1;
	u8 mode_hw  = READ_FRC_REG(FRC_REG_FILM_PHS_1) >> 8 & 0xff;
	u8 phase    = READ_FRC_REG(FRC_REG_FILM_PHS_2) >> 16 & 0xff;

	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	struct st_film_ctrl_para *g_stfilm_ctrl_para;
	// struct frc_dev_s *devp = (struct frc_dev_s *)dev_id;

	//struct st_film_data_item *g_stfilmdata_item;
	//struct st_film_detect_rd *g_stfilmdetect_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	g_stfilm_ctrl_para =  &frc_data->g_stfilmctrl_para;
	//g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	//g_stfilmdetect_rd = &frc_data->g_stfilmdetect_rd;

	g_stfilm_detect_item->phase = phase;
	g_stfilm_detect_item->input_n  = (READ_FRC_REG(FRC_REG_PHS_TABLE) >> 24) & 0xFF;
	g_stfilm_detect_item->output_m = (READ_FRC_REG(FRC_REG_PHS_TABLE) >> 16) & 0xFF;

	if (g_stfilm_ctrl_para->film_ctrl_en == 0)
		return;


	//Initialization
	frc_glb_dif_init(frc_devp, flmGDif, flmW6Dif, flmW12Dif1, flmW12Dif2);

	// set reg_inp_frm_vld_lut  accroding to different  input_n : ouput_m
	frc_in_out_ratio_init(frc_devp);
	// ------------------ init end----------

	if ( fwhw_sel ) {       // fw-film_detection
		UPDATE_FRC_REG_BITS(FRC_FD_ENABLE, 0 << 3, 1 << 3);//Bit  3   reg_film_motion_hwfw_sel;
		mode = g_stfilm_detect_item->filmMod;
	} else {                   // hw-film_detection
		UPDATE_FRC_REG_BITS(FRC_FD_ENABLE, 1 << 3, 1 << 3);//Bit  3   reg_film_motion_hwfw_sel;
		mode = mode_hw;
	}

	if (g_stfilm_ctrl_para->film_check_mode_en == 1)
		frc_film_check_mode(frc_devp, flmGDif, flmW6Dif, flmW12Dif1/*from yanling, flmW12Dif2*/);

	//u64 timestamp = sched_clock();
	if (g_stfilm_ctrl_para->film_badedit_en == 1)
		frc_film_badedit_ctrl(frc_devp, flmGDif, mode);
	//devp->out_sts.vs_duration = timestamp - devp->out_sts.vs_timestamp;

	if(mode != mode_pre) {
	    	//xil_printf("t:mod/pre=%d,%d\n\r",mode,mode_pre);
	        phase_table_core(g_stfilm_detect_item->input_n, g_stfilm_detect_item->output_m,
	        		   g_stfilm_detect_item->frame_buf_num,mode);
	}

	mode_pre = mode;

	pr_frc(dbg_film, "film_mode=%d,phase=%d\n", mode,phase);
	pr_frc(dbg_film, "glb=%d\n", flmGDif[HISDIFNUM - 1]);


	if (g_stfilm_ctrl_para->film_7_seg_en == 1)
		frc_film_add_7_seg(frc_devp);

	//xil_printf("mod=%d,phs=%d,g_dif=%d\n\r\n\r",mode,phase,g_stfilmdetect_rd.glb_motion_all);
	mem = READ_FRC_REG(FRC_IP_PAT_RECT_CYCLE);//Bit  7: 5        reg_ip_pat_mode
	//xil_printf("pat=%d\n\r", (mem >> 5) & 0x7);
}

void frc_film_check_mode(struct frc_dev_s *frc_devp, u32 *flmGDif, u32 flmW6Dif[][HISDIFNUM],
				u32 flmW12Dif[][HISDIFNUM]/* from yanling,u8 *film_mode_fw*/)
{
	static u8 mode_pre = 0;
	static u8 mix_mod_pre = 0;
	//static u8 all_film_cnt = 0;
	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	struct st_film_data_item *g_stfilmdata_item;
	struct st_film_ctrl_para *g_stfilm_ctrl_para;
	//struct st_film_detect_rd *g_stfilmdetect_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	g_stfilm_ctrl_para =  &frc_data->g_stfilmctrl_para;
	//g_stfilmdetect_rd = &frc_data->g_stfilmdetect_rd;

	u8  nT0, nT1, mix_mod = 0;
	//u8  cnt_tmp = 0;
	u8  AB_change[MAX_CYCLE];
	u32 sumGdif, max_dif,min_dif;                             //sum of global dif
	u32 match_value = 0;
	//u32 mem = 0;

	u8 *Mode_cnt    = g_stfilmdata_item->ModCount;             //mod count
	u8 *phase       = g_stfilmdata_item->phase;                //phase
	u8  Avnum       = 0;                                      //num of mod cycle
	u8  phase_next  = 0;                                      //next phase;
	u32 AveragG     = 0;                                      //average global dif
	u32 real_flag   = 0, expect_flag=0;
	u8 flag         = 0;

	//mode check
	for (nT0 = 1; nT0 < FLMMODNUM; nT0++) {
		//initial
		sumGdif = 0;
		max_dif = 0;
		min_dif = 0xFFFFFFFF;   // max of u32

		//min_dif = (1<<32)-1;
		g_stfilm_detect_item->cadence_cnt        = g_stfilmtable[nT0].cadence_cnt;
		g_stfilm_detect_item->mode_enter_th    = g_stfilmtable[nT0].enter_th;
		g_stfilm_detect_item->phase_match_data = g_stfilmtable[nT0].match_data;
		g_stfilm_detect_item->quit_mode_th     = g_stfilmtable[nT0].quit_th;
		g_stfilm_detect_item->phase_mask_value = g_stfilmtable[nT0].mask_value;
		Avnum                                = g_stfilmtable[nT0].average_num;

		// calculate averag
		for (nT1 = 0; nT1 < Avnum; nT1++) {
			sumGdif = sumGdif + flmGDif[HISDIFNUM - 1 - nT1];// sum
			max_dif = max_dif>flmGDif[HISDIFNUM - 1 - nT1] ?
					max_dif:flmGDif[HISDIFNUM - 1 - nT1];    // max
			min_dif = min_dif<flmGDif[HISDIFNUM - 1 - nT1] ?
					min_dif:flmGDif[HISDIFNUM - 1 - nT1];    // min
		}
		// delete max and min value(for scene change)
		sumGdif = sumGdif - max_dif - min_dif;
		AveragG = (sumGdif + Avnum / 2) / (Avnum - 2);
		AveragG =(AveragG * g_stfilm_ctrl_para->glb_ratio + 8) / 16; // average
		AveragG = AveragG +  g_stfilm_ctrl_para->glb_ofset;
		AveragG = MAX(AveragG, g_stfilm_ctrl_para->min_diff_th);// average as th
		frc_get_min_max_idx(frc_devp, flmGDif, AveragG, AB_change, &real_flag);// calculate "0","1"
		// least significant cadence_cnt  bits
		real_flag = real_flag % (1 << g_stfilm_detect_item->cadence_cnt);

		//Mode check
		frc_mode_check(frc_devp, real_flag, flmGDif, nT0);
	}
	pr_frc(dbg_film, "mode_pre=%d ,cnt=%d\n",mode_pre,Mode_cnt[nT1]);
	nT1 = mode_pre;
	if (nT1 == 0) {
		//video ->film mode begin phase = 0
		for (nT0 = 1; nT0 < FLMMODNUM; nT0++) {
			phase_next =
				(phase[nT0] == (g_stfilmtable[nT0].cadence_cnt -1)) ?
				0 : phase[nT0] + 1;//next phase
			if (Mode_cnt[nT0] > g_stfilmtable[nT0].enter_th && phase_next == 0)
				nT1=nT0;
		}
	} else {
		if (Mode_cnt[nT1] < g_stfilmtable[nT1].enter_th)
			nT1 = 0;

		for (nT0 = 1; nT0 < FLMMODNUM; nT0++) {
			if (nT0 == nT1)
				continue;

			if (Mode_cnt[nT0]> g_stfilmtable[nT0].enter_th) {//another mode count num > th
				if (g_stfilmtable[nT0].mode_enter_level >
				    g_stfilmtable[nT1].mode_enter_level) {   //compare level

					Mode_cnt[nT1] = 0;
					nT1 = 0;
				} else {
					if (g_stfilm_detect_item->phase_error_flag) {
						Mode_cnt[nT1] = 0;
						g_stfilm_detect_item->phase_error_flag = 0;
						nT1 = 0;
					} else {
						Mode_cnt[nT0] = 0;
					}
				}
			}
		}
	}
	match_value = frc_cycshift(g_stfilmtable[nT1].match_data, g_stfilmtable[nT1].cadence_cnt,
			       g_stfilmtable[nT1].mask_value, 1);
	expect_flag = frc_cycshift(match_value, g_stfilmtable[nT1].cadence_cnt,
			       g_stfilmtable[nT1].mask_value, phase[nT1]);

	if (nT1 > 0)
		mix_mod = 0;   // only for debug , bypass mix-mode
        //mix_mod = frc_mix_mod_check(flmGDif,flmW6Dif,expect_flag,nT1);

	if (mix_mod == 1)
		nT1 = 0;

	pr_frc(dbg_film, "nT1=%d,pre=%d\n",nT1,mode_pre);
	if (((g_stfilm_ctrl_para->film_cadence_switch >> nT1) & 0x1) == 0)
		nT1 = 0;
	pr_frc(dbg_film, "en=%d,nT1=%d\n",((g_stfilm_ctrl_para->film_cadence_switch >> nT1) & 0x1),nT1);

	//quit mix_mod mix_mod-->vedio->film mode
	if (mix_mod_pre == 1 && mix_mod == 0) {
		if(phase[nT1] > 0) {
			mix_mod_pre = 1;
			nT1 = 0;
		} else {
			mix_mod_pre = 0;
		}
	} else {
        	mix_mod_pre = mix_mod;
	}

	//xil_printf("phs_err=%d\n\r", g_stfilm_detect_item->phase_error_flag);
	//xil_printf("all_flm=%d\n\r", mem);
	//xil_printf("all_flm_cnt=%d\n\r", all_film_cnt);

	//fw delay 1 frame
	phase_next = phase[nT1] == (g_stfilmtable[nT1].cadence_cnt - 1) ? 0 : phase[nT1] + 1;
	//phase + 1  for (HW check table)
	phase_next = phase_next == (g_stfilmtable[nT1].cadence_cnt - 1) ? 0 : phase_next + 1;

	match_value = frc_cycshift(g_stfilmtable[nT1].match_data, g_stfilmtable[nT1].cadence_cnt,
			       g_stfilmtable[nT1].mask_value, 1);
	flag = frc_cycshift(g_stfilmtable[nT1].match_data, g_stfilmtable[nT1].cadence_cnt,
			g_stfilmtable[nT1].mask_value,phase_next) & 0x1;
	UPDATE_FRC_REG_BITS(FRC_FD_ENABLE, flag << 22, 1 << 22);


	if (g_stfilm_ctrl_para->film_mode_force_phs_en) {
		g_stfilm_detect_item->force_phase =
			g_stfilm_detect_item->force_phase == (g_stfilmtable[g_stfilm_ctrl_para->film_mode_force_value].cadence_cnt -1) ?
							0 : g_stfilm_detect_item->force_phase + 1 ;
		g_stfilm_detect_item->force_phase =
			g_stfilm_detect_item->force_phase == (g_stfilmtable[g_stfilm_ctrl_para->film_mode_force_value].cadence_cnt -1) ?
							0 : g_stfilm_detect_item->force_phase + 1 ;
		g_stfilm_ctrl_para->film_mode_force_phs_en = 0;
	} else {
		g_stfilm_detect_item->force_phase =
			g_stfilm_detect_item->force_phase == (g_stfilmtable[g_stfilm_ctrl_para->film_mode_force_value].cadence_cnt -1) ?
							0 : g_stfilm_detect_item->force_phase + 1 ;
	}

	if (g_stfilm_ctrl_para->film_mode_force_en) {
		UPDATE_FRC_REG_BITS(FRC_REG_PHS_TABLE,  g_stfilm_ctrl_para->film_mode_force_value, 0xFF);
		g_stfilm_detect_item->filmMod = g_stfilm_ctrl_para->film_mode_force_value;
		UPDATE_FRC_REG_BITS(FRC_REG_FILM_PHS_1, g_stfilm_detect_item->force_phase << 24,
				    0xFF000000);  //write film_mode and phase to reg
	} else {
		g_stfilm_detect_item->filmMod = nT1;
		UPDATE_FRC_REG_BITS(FRC_REG_PHS_TABLE,  nT1, 0xFF);
		 //write film_mode and phase to reg
		UPDATE_FRC_REG_BITS(FRC_REG_FILM_PHS_1, phase_next << 24, 0xFF000000);
	}

	//xil_printf("phsn=%d,f_phs=%d\n\r",phase_next,g_stfilm_detect_item->force_phase);
	//write film_mode and phase to reg
	//UPDATE_FRC_REG_BITS(FRC_REG_FILM_PHS_1, mix_mod<<17, 0x20000);

	//*film_mode_fw = nT1;
	mode_pre = nT1;
}

void frc_get_min_max_idx(struct frc_dev_s *frc_devp, u32 *flmDif,
					u32 AveragG, u8 *AB_change, u32 *flag)
{
	u8 i;
	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	//struct st_film_data_item *g_stfilmdata_item;
	//struct st_film_detect_rd *g_stfilmdetect_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	//g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	//g_stfilmdetect_rd = &frc_data->g_stfilmdetect_rd;

	*flag = 0;

	for(i = 0; i < g_stfilm_detect_item->cadence_cnt; i++)
		AB_change[i] = 0;              //initial

	for (i = 0; i < g_stfilm_detect_item->cadence_cnt; i++)
	{
		if(flmDif[HISDIFNUM - 1 - i] > AveragG )    //---large "1"
			AB_change[i] = 1;
	}

	//0,1 check
	for (i = 0; i < g_stfilm_detect_item->cadence_cnt; i++) {
		if(AB_change[i] == 1)
			*flag = *flag | 1 << i;
	}
}

inline u32 frc_cycshift(u32 nflagtestraw, u8 ncycle, u32 mask_value, u8 n)
{
    u32 a,b,ret;

    a = nflagtestraw >> (ncycle - n);
    b = mask_value & (nflagtestraw << n);
    ret = a | b;

    return ret;
}

//phase change
void frc_phase_adjustment(u8 *new_phase, u8 phase, u8 cadence_cnt,
				u32 match_value, u8 *quitCunt)
{
	u8 phase1,nexpect_flag1;

	phase1 = phase == (cadence_cnt-1) ? 0 : phase + 1; //drop frame
	if(phase1 == 0)
		nexpect_flag1 = match_value;
	else
		nexpect_flag1 = match_value >> (cadence_cnt - phase1);

	nexpect_flag1 = nexpect_flag1 & 1;

	while(nexpect_flag1 == 0) {
		phase1 = phase1 == (cadence_cnt-1) ? 0 :  phase1 + 1;

		if(phase1 == 0)
			nexpect_flag1 = match_value;
		else
			nexpect_flag1 = match_value >> (cadence_cnt - phase1);

		nexpect_flag1 = nexpect_flag1 & 1;
	}
	*quitCunt = *quitCunt + 1;
	*new_phase = phase1;
}

//quit core
inline u8 quit_core(struct frc_dev_s *frc_devp, u8 *quitcnt, u8 *phase, u8 *quit_reset_cnt, u16 real_flag, u32 expect_flag,
   			u32 glb_mot, u8 quit_reset_th, u8 quitmodth, u32 min_diff_th, u8 cadence_cnt, u32 match_value, u32 mask_value)
{
	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	struct st_film_data_item *g_stfilmdata_item;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	g_stfilmdata_item = &frc_data->g_stfilmdata_item;

	u8 quit_flag = 0;
	u8 expect_zero = (expect_flag & 0x1) == 0;
	u8 real_zero = (real_flag & 0x1) == 0;
	u8 phase_new = 0;
	u8 i = 0;
	u8 phase_change_flag = 0;
	if (quitcnt[0] == 0) {
		if (expect_zero)
			if(real_zero == 0 && glb_mot > min_diff_th) {
				quitcnt[0] = 1;
				g_stfilm_detect_item->phase_error_flag = 1;
			}
	}

	else {
		if (expect_zero)
			if (real_zero == 0)
				quitcnt[0] = quitcnt[0] + 1;                       //quit + 1
		if (expect_flag == real_flag)
			quit_reset_cnt[0] = quit_reset_cnt[0] + 1;            //reset + 1
		//else
			//quit_reset_cnt[0] = quit_reset_cnt[0] > 0  quit_reset_cnt[0] -1 : 0;            //reset + 1


		if (real_flag != expect_flag) {   // phase wrong
			for (i = 0; i < cadence_cnt; i++)
			{
				//phase 0 ---cadence_cnt-1
				phase_new = phase_new == (cadence_cnt - 1) ? 0 :  phase_new + 1;
				 //expect mask value
				expect_flag = frc_cycshift(match_value, cadence_cnt, mask_value,phase_new);

				if (real_flag == expect_flag) { //expect = real enter mode
				    phase_change_flag = 1;
					break;
				}
			}
		}
		if (quitcnt[0] > quitmodth) {
		 	quit_flag = 1;
			quitcnt[0] = 0;
			quit_reset_cnt[0] = 0;
			g_stfilm_detect_item->phase_error_flag = 0;
		}
		if (quit_reset_cnt[0] > quit_reset_th) {
			if(quitcnt[0] > 0)
				quitcnt[0] = quitcnt[0] -1;
			else {
				quitcnt[0] = 0;
				g_stfilm_detect_item->phase_error_flag = 0;
			}
			quit_reset_cnt[0] = 0;
		}

		if (phase_change_flag)  //change phase
		 	*phase = phase_new;

		pr_frc(dbg_film, "p_flag=%d,phase_new=%d,quit_flag=%d\n",phase_change_flag,phase_new,quit_flag);
		pr_frc(dbg_film, "quitcnt=%d,quit_reset_cnt=%d,exp=%x,real=%x\n",quitcnt[0],quit_reset_cnt[0],expect_flag,real_flag);

	}

	return quit_flag;

}

void frc_mode_check(struct frc_dev_s *frc_devp, u16 real_flag, u32 *flmGDif, u8 mode)
{
	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	struct st_film_data_item *g_stfilmdata_item;
	struct st_film_ctrl_para *g_stfilm_ctrl_para;
	//struct st_film_detect_rd *g_stfilmdetect_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	g_stfilm_ctrl_para =  &frc_data->g_stfilmctrl_para;
	//g_stfilmdetect_rd = &frc_data->g_stfilmdetect_rd;

	u8  i;
	u8  mode_now;
	u32 expect_flag      = 0;
	u8  quit_flag        = 0;
	u8  quitmodth        = g_stfilm_detect_item->quit_mode_th;
	u8  quit_reset_th    = g_stfilmtable[mode].cadence_cnt;
	u32 difMinth         = g_stfilm_ctrl_para->min_diff_th;
	u8  *Mode_cnt        = g_stfilmdata_item->ModCount;
	u8  *phase           = g_stfilmdata_item->phase;
	u8  *quitcnt         = g_stfilmdata_item->quitCount;
	u8  *quit_reset_cnt  = g_stfilmdata_item->quit_reset_Cnt;
	u8  cadence_cnt      = g_stfilmtable[mode].cadence_cnt;
	u32 mask_value       = g_stfilmtable[mode].mask_value;
	u32 match_value      = frc_cycshift(g_stfilmtable[mode].match_data, cadence_cnt, mask_value, 1);
	u8  mode_hw          = READ_FRC_REG(FRC_REG_FILM_PHS_1) >> 8 & 0xff;
	u8  fwhw_sel         = READ_FRC_REG(FRC_REG_FILM_PHS_1) >> 16 & 0x1;

	mode_now             = fwhw_sel ? g_stfilm_detect_item->filmMod : mode_hw;

	//quit_reset_th = 3;
	//match_value = frc_cycshift(match_value,cadence_cnt,mask_value,1);

	// begin
	if (Mode_cnt[mode] == 0) {
		phase[mode] = 0;
		for (i = 0; i < cadence_cnt; i++)
		{
			//phase 0 ---cadence_cnt-1
			phase[mode] = phase[mode] == (cadence_cnt - 1) ? 0 :  phase[mode] + 1;
			 //expect mask value
			expect_flag = frc_cycshift(match_value, cadence_cnt, mask_value,phase[mode]);

			if (real_flag == expect_flag) {//expect = real enter mode
				Mode_cnt[mode] = Mode_cnt[mode]+1;
				break;
			}
		}
	} else if (Mode_cnt[mode] <= g_stfilmtable[mode].enter_th) { //check one cycle 0,1
		phase[mode] = phase[mode] == (cadence_cnt - 1) ? 0 :  phase[mode] + 1;      //phase 0---cadence_cnt-1
		expect_flag = frc_cycshift(match_value, cadence_cnt, mask_value, phase[mode]);

		if (real_flag == expect_flag) {
			Mode_cnt[mode] = Mode_cnt[mode]+1;
		} else {
			Mode_cnt[mode] = 0;   //reset
			phase[mode]    = 0;
		}
	} else {//only check "0"
		phase[mode] = phase[mode] == (cadence_cnt - 1) ? 0 :  phase[mode] + 1;
		if (phase[mode] == 0)
			expect_flag = match_value;
		else
			expect_flag = frc_cycshift(match_value, cadence_cnt, mask_value,phase[mode]); //match_value >> (cadence_cnt - phase[mode]);

		quit_flag = quit_core(frc_devp, quitcnt, &phase[mode], quit_reset_cnt, real_flag, expect_flag, flmGDif[HISDIFNUM -1], quit_reset_th,
			 quitmodth, difMinth, cadence_cnt, match_value, mask_value);

		if (quit_flag)
			Mode_cnt[mode] = 0;

	}

	if (Mode_cnt[mode] > 100)
		Mode_cnt[mode] = 100;
}

//average of each windows and global diff
void  frc_mixmod_diff_aveg(struct frc_dev_s *frc_devp, u32 *flmGDif,
				u32 flmW6Dif[][HISDIFNUM], u8 *aMnMx, u32 *aveg_min_dif,
				u32 *aveg_max_dif, u32 *aveg_min_difG, u8 wind_num, u8 mode)
{
	u8 nT0,nT1,nT2;
	u32 nMin,nMax_0,nMax_G;
	u8 cuntnum = 0;
	u8 num_cycle = g_stfilmtable[mode].cycle_num;
	u8 cadence = g_stfilmtable[mode].cadence_cnt;

	for (nT1 = 0; nT1 < cadence; nT1++)
		if (aMnMx[nT1] == 0)
			cuntnum = cuntnum + 1;

	for (nT1 = 0; nT1 < wind_num; nT1++) {
		aveg_min_dif[nT1] = 0;
		aveg_max_dif[nT1] = 0;
		nMin = flmW6Dif[nT1][HISDIFNUM - 1];
		nMax_0 = flmW6Dif[nT1][HISDIFNUM - 1];
		nMax_G = flmGDif[HISDIFNUM-1];
		for (nT2 = 0; nT2 < num_cycle; nT2++)
			for (nT0 = 0; nT0 < cadence; nT0++)
				if (aMnMx[nT0] == 0) {
					aveg_min_dif[nT1] = aveg_min_dif[nT1] +
						flmW6Dif[nT1][HISDIFNUM - 1 - nT0 - nT2 * cadence];
					if (nMin >
					    flmW6Dif[nT1][HISDIFNUM - 1 - nT0 - nT2 * cadence])
						nMin =
						flmW6Dif[nT1][HISDIFNUM - 1 - nT0 - nT2 * cadence];

					if (nMax_0 <
					    flmW6Dif[nT1][HISDIFNUM - 1 - nT0 - nT2 * cadence])
						nMax_0 =
						flmW6Dif[nT1][HISDIFNUM - 1 - nT0 - nT2 * cadence];

					if (nT1 == 0) {
						*aveg_min_difG = *aveg_min_difG +
						flmGDif[HISDIFNUM - 1 - nT0 - nT2 * cadence];
						nMax_G = nMax_G >
						flmGDif[HISDIFNUM - 1 - nT0 - nT2 * cadence] ?
						nMax_G : flmGDif[HISDIFNUM-1-nT0-nT2*cadence];
					}
				} else {
					aveg_max_dif[nT1] =
						aveg_max_dif[nT1] +
						flmW6Dif[nT1][HISDIFNUM - 1 - nT0 - nT2 * cadence];
					//nMax_1 = nMax_1>flmW6Dif[nT1][HISDIFNUM-1-nT0-nT2*cycle] ? nMax_1 :flmW6Dif[nT1][HISDIFNUM-1-nT0-nT2*cycle];
				}

		if (nMax_0 == flmW6Dif[nT1][HISDIFNUM - 1])
			aveg_min_dif[nT1] = (aveg_min_dif[nT1] + num_cycle) / (cuntnum * num_cycle);           //0 average
		else
			aveg_min_dif[nT1] =
			(aveg_min_dif[nT1] - nMax_0 + num_cycle) / (cuntnum * num_cycle - 1);           //0 average
		// if(nMax_1==flmW6Dif[nT1][HISDIFNUM-1-idx])
		aveg_max_dif[nT1] = (aveg_max_dif[nT1]) / ((cadence - cuntnum) * num_cycle);   //1 average
		// else
		//  aveg_max_dif[nT1] =
		//(aveg_max_dif[nT1] - nMax_1) / ((cycle - cuntnum) * num_cycle - 1);   //1 average
		if (nT1 == 0)	//0 global average
			*aveg_min_difG =
				(*aveg_min_difG - nMax_G + num_cycle) / (cuntnum * num_cycle - 1);
	}
}

//compare with own history information
void frc_mixmod_compare_to_history(struct frc_dev_s *frc_devp, u32 *flmGDif,
					u32 flmW6Dif[][HISDIFNUM], u8 *aMnMx,
					u32 aveg_min_difG, u32 aveg_min_dif, u32 mean_other_min,
					u32 mm_chk_mmdifthd, u8 cadence, u8 *ratio_difmin,
					u8 *dif_aMnMx0, u8 *mix_modnum_cpre, u8 *mix_modnum_reset,
					u8 *mix_modnum_cpre_quit, u8 mix_modnum_cown,
					u16 nsize, u8 *flag, u8 nT2, u8 nT0)
{
	u32 difthinc,DifGG,difthincg,difthles,difthlesg,weight;
	s32 DifWW;
	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	struct st_film_ctrl_para *g_stfilm_ctrl_para;
	//struct st_film_data_item *g_stfilmdata_item;
	//struct st_film_detect_rd *g_stfilmdetect_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	//g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	//g_stfilmdetect_rd = &frc_data->g_stfilmdetect_rd;
	g_stfilm_ctrl_para =  &frc_data->g_stfilmctrl_para;

	DifWW = flmW6Dif[nT0][HISDIFNUM - 1] - flmW6Dif[nT0][HISDIFNUM - 1 - nT2];

	//horizontal direction
	if (nT0 < 3) {
		if (DifWW <= -1024)
			DifWW = DifWW - 2048;

		DifGG = flmGDif[HISDIFNUM - 1] - flmGDif[HISDIFNUM - 1 - nT2];
		difthinc = (aveg_min_dif * 10 + 1024) / 8;  //increse thd
		if (*mix_modnum_cpre >= 2) {
			difthinc = MAX(mean_other_min * 4,MAX(difthinc, 1024));
			mm_chk_mmdifthd = mm_chk_mmdifthd - 120;          //KIisarazu 378
		}

		difthincg = (aveg_min_difG * 8 + 1024) / 8;  //global thd
		difthles = (aveg_min_dif * 8) / 8;   //decrese thd
		difthlesg = (aveg_min_difG * 8 + 1024) / 8;  //global  flmGDif[29]
		*ratio_difmin  = (flmW6Dif[nT0][HISDIFNUM - 1] + 1024) / (DifWW + 1024);

		//enter mix_mode //dif gradually increase
		if (flmW6Dif[nT0][HISDIFNUM - 1] > difthinc &&
		    DifWW > mm_chk_mmdifthd && flmGDif[HISDIFNUM - 1] > difthincg)
			*mix_modnum_cpre = *mix_modnum_cpre + 1;
		else
			if (*mix_modnum_cpre > 5)
				if (flmW6Dif[nT0][HISDIFNUM - 1] < difthinc &&
				    flmW6Dif[nT0][HISDIFNUM - 1] > difthles)
					*mix_modnum_cpre = *mix_modnum_cpre + 1;
				else
					*mix_modnum_cpre = MAX(0, *mix_modnum_cpre - 1);
			else
				*mix_modnum_cpre = 0;

		//quit mix_mode
		if (flmW6Dif[nT0][HISDIFNUM - 1] < difthles &&
		    DifWW < -mm_chk_mmdifthd && flmGDif[HISDIFNUM - 1] < difthlesg)//dif gradually decrease
			*mix_modnum_cpre_quit = *mix_modnum_cpre_quit + 1;
		else
			if (mix_modnum_cpre_quit[nT0] > 5)
				if (flmW6Dif[nT0][HISDIFNUM - 1] < difthinc &&
				    flmW6Dif[nT0][HISDIFNUM - 1] > difthles)
					*mix_modnum_cpre_quit = *mix_modnum_cpre_quit + 1;
				else
					*mix_modnum_cpre_quit = MAX(0, *mix_modnum_cpre_quit - 1);
			else
				*mix_modnum_cpre_quit = 0;

		if(*mix_modnum_cpre_quit > cadence &&
		   flmW6Dif[nT0][HISDIFNUM - 1] < difthles &&
		   flmW6Dif[nT0][HISDIFNUM - 1] < mean_other_min &&
		   *mix_modnum_cpre > 5)
			mix_modnum_reset[nT0] = mix_modnum_reset[nT0] + 1;

		//KIisarazu
		if (*mix_modnum_cpre == 1) {
			if(flmW6Dif[nT0][HISDIFNUM - 1] /
			   (flmW6Dif[nT0][HISDIFNUM - 1 - nT2] + 1) >= 2)
				*dif_aMnMx0 = flmW6Dif[nT0][HISDIFNUM - 1 - nT2] / 2;
			else
				*dif_aMnMx0 = flmW6Dif[nT0][HISDIFNUM - 1 - nT2];

			if(flmW6Dif[nT0][HISDIFNUM - 1] < 1024)
				*mix_modnum_cpre = 0;
		}

		if (*mix_modnum_cpre == g_stfilm_ctrl_para->mm_cpre_thd &&
		    (((flmW6Dif[nT0][HISDIFNUM - 1] + *dif_aMnMx0 / 2) / (*dif_aMnMx0 + 1)) < 9))
			*mix_modnum_cpre = 0;

		if (*mix_modnum_cpre > 4 || mix_modnum_cown > cadence + 1)
			*flag = 1;
	} else {//vertical direction
		if (*mix_modnum_cpre > 0) {
			weight = MIN(500, MIN(flmW6Dif[nT0][HISDIFNUM - 1],
				     flmW6Dif[nT0][HISDIFNUM - 1 - nT2]) / 2);
			//weight = (flmW6Dif[nT0][HISDIFNUM-1] + flmW6Dif[nT0][HISDIFNUM-1-nT2])/2;
			DifWW = flmW6Dif[nT0][HISDIFNUM - 1] - flmW6Dif[nT0][HISDIFNUM - 1 - nT2];
		}

		//enter mix_mode
		if ( *mix_modnum_cpre > 0 && (DifWW > 0 || ABS(DifWW) < weight))
			*mix_modnum_cpre = *mix_modnum_cpre + 1;
		else if (*mix_modnum_cpre < cadence + 1)
			*mix_modnum_cpre = 0;

		if (DifWW>(flmW6Dif[nT0][HISDIFNUM-1]*3)/4&&flmW6Dif[nT0][HISDIFNUM-1]>(nsize<<5)&&*mix_modnum_cpre==0&&*flag==0) {
			*dif_aMnMx0 = flmW6Dif[nT0][HISDIFNUM-1];
			*mix_modnum_cpre = 1;
		}

		//quit mix_mode
		if (*mix_modnum_cpre > 5 && DifWW < 0 &&
		    ABS(DifWW) > (flmW6Dif[nT0][HISDIFNUM - 1 - nT2] * 2) / 4 &&
		    g_stfilm_detect_item->phase_error_flag == 0) {
			mix_modnum_reset[nT0] = 10;
			mix_modnum_reset[0] = 10;
			mix_modnum_reset[1] = 10;
			mix_modnum_reset[2] = 10;
		}
	}
}

// compare with global diff
void frc_mixmod_compare_to_other(struct frc_dev_s *frc_devp, u32 *flmGDif,
				u32 flmW6Dif[][HISDIFNUM], u8 *mix_modnum_cother,
				u8 *mix_modnum_reset, int nT2, int nT0)
{
	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	struct st_film_ctrl_para *g_stfilm_ctrl_para;
	//struct st_film_data_item *g_stfilmdata_item;
	//struct st_film_detect_rd *g_stfilmdetect_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	//g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	//g_stfilmdetect_rd = &frc_data->g_stfilmdetect_rd;
	g_stfilm_ctrl_para =  &frc_data->g_stfilmctrl_para;

	if (g_stfilm_detect_item->phase_error_flag == 0 && nT0 < 5) {
		if ((flmGDif[HISDIFNUM - 1] - 500) <= flmW6Dif[nT0][HISDIFNUM - 1] &&
		    flmW6Dif[nT0][HISDIFNUM - 1] > 1024)
			*mix_modnum_cother = *mix_modnum_cother + 1;
		else
			*mix_modnum_cother = MAX(0, *mix_modnum_cother - 1);
	}

	if(flmW6Dif[nT0][HISDIFNUM - 1] < 500 && mix_modnum_cother[nT0] >= 4)
		*mix_modnum_reset = *mix_modnum_reset + 1;

	//if(mix_modnum_cother[nT0]<4&&flmWDif[nT0][HISDIFNUM-1]<(10000))   //ex44 735
	if(*mix_modnum_cother <= g_stfilm_ctrl_para->mm_cother_thd &&
	   (flmW6Dif[nT0][HISDIFNUM - 1] - flmW6Dif[nT0][HISDIFNUM - 1 - nT2]) < 1024)   //ex44 735
		*mix_modnum_cother = 0;
}

u8 frc_mix_mod_check(struct frc_dev_s *frc_devp, u32 *flmGDif, u32 flmW6Dif[][HISDIFNUM],
			u32 expect_flag, u8 mode)
{
	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	struct st_film_data_item *g_stfilmdata_item;
	struct st_film_ctrl_para  *g_stfilm_ctrl_para;
	//struct st_film_detect_rd *g_stfilmdetect_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	g_stfilm_ctrl_para = &frc_data->g_stfilmctrl_para;
	//g_stfilmdetect_rd = &frc_data->g_stfilmdetect_rd;

	u8 nT0, nT1, nT2, nT3;
	u32 DifWW, wind_flag;
	u32 MeanWW = 0, mean_other_min = 0, mean_other_max = 0, nMean = 0;
	u8 testflag = 0;
	u8 cuntnum = 0;
	u32 thd_max;
	u8 *mix_modnum_cown   =  &g_stfilmdata_item->mm_num_cown[0];//mix mode num compare with own window
	u8 *mix_modnum_cother =  &g_stfilmdata_item->mm_num_cother[0];//mix mode num compare with other window
	u8 *mix_modnum_cpre   =  &g_stfilmdata_item->mm_num_cpre[0];//mix mode num compare with history (diff gradually increase,may enter mix mode)
	u8 *mix_modnum_reset  =  &g_stfilmdata_item->mm_num_reset[0];
	u8 *mix_modnum_cpre_quit = &g_stfilmdata_item->mm_num_cpre_quit[0];//mix mode num compare with history(diff gradually reduced,may quit mix mode)
	u8 mix_mod = 0;
	u8 mm_chk_mmdifthd = g_stfilm_ctrl_para->mm_chk_mmdifthd;
	u8 cadence = g_stfilmtable[mode].cadence_cnt;
	u8 idx = cadence + 1;
	//int phasecycle = g_stfilm_detect_item->phasecycle;
	//int size;
	u8 aMnMxW[20];//[cadence];
	u32 aveg_min_dif[6];
	u32 aveg_max_dif[6];
	u32 aveg_min_difG = 0;
	u8 aMnMx[20];//[cadence];
	u8 ratio = g_stfilm_ctrl_para->wind_ratio;

	//int Dif[cycle];
	u32 nMin = 0;
	u32 nMax;//nMax_1 = 0;
	u16 width = READ_FRC_REG(FRC_ME_RO_FRM_SIZE) >> 12 & 0x3FF;
	u16 hight = READ_FRC_REG(FRC_ME_RO_FRM_SIZE) & 0x3FF;
	u32 nsize = (width * hight) >> 8;
	u8 flag = 0;
	static u8 mix_modpre = 0;
	static u8 ratio_difmax[WINNUM]={0, 0, 0, 0, 0, 0};
	static u8 ratio_difmin[WINNUM]={0, 0, 0, 0, 0, 0};
	static u8 dif_aMnMx0[WINNUM]={0, 0, 0, 0, 0, 0};
	static u8 mix_modnum_quit_reset[WINNUM]={0, 0, 0, 0, 0, 0};
	//u8  g_stfilm_detect_item->mm_cown_thd = cadence + 1;
	//the num of window
	u8 wind_num = 6;

	//initial  get idx
	for (nT2 = 0; nT2 < cadence; nT2++)
		if((expect_flag >> nT2) & 1) {
			aMnMx[nT2] = 1;
			if(nT2<idx)
				idx=nT2;
		} else
			aMnMx[nT2] = 0;

	//get the aveg_min/aveg_max of num_cycle*cycle frames
	frc_mixmod_diff_aveg(frc_devp, flmGDif, flmW6Dif, aMnMx, aveg_min_dif, aveg_max_dif, &aveg_min_difG, wind_num, mode);

	for (nT0 = 0; nT0 < wind_num; nT0++) {
		testflag = 0;	     //initial
		nMax = 0;
		//get other window average
		//0-2 MeanWW(thd)=average(avegWdif[0-2]);
		if (nT0 < 3) {
			nT1 = nT0 < 3 ? nT0 + 1 : 0;
			nT2 = nT1 < 3 ? nT1 + 1 : 0;
			MeanWW = (aveg_min_dif[0] + aveg_min_dif[1] + aveg_min_dif[2] + 1) / 3;
			thd_max = (aveg_max_dif[0] + aveg_max_dif[1] + aveg_max_dif[2] + 1024) / 3;

			//mean_other_min = (aveg_min_dif[nT1] +aveg_min_dif[nT2]+1)/2;	 //other window aver_min
			mean_other_max = (aveg_max_dif[nT1] + aveg_max_dif[nT2] + 1024) / 2;    //other window aver_max
			mean_other_min = MAX(aveg_min_dif[nT1],aveg_min_dif[nT2]);

			if(mix_modnum_cpre[nT1] > 1||mix_modnum_cown[nT1] > 1)
				mean_other_min = aveg_min_dif[nT2];

			if(mix_modnum_cpre[nT2] > 1||mix_modnum_cown[nT2] > 1)
				mean_other_min = aveg_min_dif[nT1];

			if((mix_modnum_cpre[nT1] > 1 && mix_modnum_cpre[nT2] > 1)
			    || (mix_modnum_cown[nT1] > 1 && mix_modnum_cown[nT2] > 1))
				mean_other_min = 1024;

		} else if (nT0 < 5) {//3-4 MeanWW(thd)=average(avegWdif[3-4]);
			MeanWW = (aveg_min_dif[3] + aveg_min_dif[4] +1)/2;
			thd_max= (aveg_max_dif[3] + aveg_max_dif[4]+1024)/2;
			mean_other_min = nT0==3 ? aveg_min_dif[4] : aveg_min_dif[3];     //other window aver_min
			mean_other_max = nT0==3 ? aveg_max_dif[4] : aveg_max_dif[3];     //other window aver_max
		} else if (nT0 < 6) {
			MeanWW = aveg_min_dif[nT0] ;
			thd_max= aveg_max_dif[nT0] ;
			mean_other_min = MeanWW;	  //other window aver_max
			mean_other_max = thd_max;	  //other window aver_max
		} else {//12 window
			for (nT1 = 0; nT1 < WINNUM - 6; nT1++) {
				MeanWW = MeanWW + aveg_min_dif[nT1 + 6];
				thd_max = thd_max + aveg_max_dif[nT1 + 6];
				if(nT1 + 6 == nT0)
					continue;
				mean_other_min = mean_other_min + aveg_min_dif[nT1 + 6];
				mean_other_max = mean_other_max + aveg_max_dif[nT1 + 6];
			}
			MeanWW = (MeanWW + 1024) / (WINNUM - 6);
			thd_max = (thd_max + 1024) / (WINNUM - 6);
			mean_other_min = (mean_other_min + 1) / (WINNUM - 7);
			mean_other_max = (mean_other_max + 1024) / (WINNUM - 7);
		}

		mean_other_min = mean_other_min + g_stfilm_ctrl_para->wind_ofset;
		mean_other_min = MIN(mean_other_min,4096);
		//compare to history
		if (aMnMx[0] == 0) {
			nT2 = 0;
			for (nT1 = 1; nT1 < cadence; nT1++)
				if(aMnMx[nT1] == 0) {
					nT2 = nT1;
					break;
				}
			nT2 = nT2==0 ? cadence : nT2;	    //pre aMnMx = 0
			frc_mixmod_compare_to_history(frc_devp, flmGDif, flmW6Dif, aMnMx,
						  aveg_min_difG, aveg_min_dif[nT0], mean_other_min,
						  mm_chk_mmdifthd, cadence, &ratio_difmin[nT0],
						  &dif_aMnMx0[nT0], &mix_modnum_cpre[nT0],
						  mix_modnum_reset, &mix_modnum_cpre_quit[nT0],
						  mix_modnum_cown[nT0], nsize, &flag, nT2, nT0);
			//compare with global dif
			frc_mixmod_compare_to_other(frc_devp, flmGDif, flmW6Dif,
						&mix_modnum_cother[nT0],
						&mix_modnum_reset[nT0], nT2, nT0);
		} else if (nT0 < 3) {
			nT2 = 0;
			for (nT1 = 1; nT1 < cadence; nT1++)
				if(aMnMx[nT1] == 1) {
					nT2 = nT1;
					break;
				}

			nT2 = nT2 == 0 ? cadence : nT2;
			DifWW = flmW6Dif[nT0][HISDIFNUM - 1] - flmW6Dif[nT0][HISDIFNUM - 1 - nT2];
			if (DifWW <= -1024)
				DifWW = DifWW - 2048;

			if (mix_modnum_cpre[nT0]>0)
				ratio_difmax[nT0] =
					(flmW6Dif[nT0][HISDIFNUM-1] + 1024) / (DifWW + 1024);
			else
				ratio_difmax[nT0]  = 0;

			if (ratio_difmax[nT0] > 0 && (ratio_difmax[nT0] < ratio_difmin[nT0]) &&
			    mix_modnum_cpre[nT0] < 5 &&
			    DifWW < (nsize << 6) && flmW6Dif[nT0][HISDIFNUM - 1] > (nsize << 5))
				mix_modnum_cpre[nT0] = 0;
		}

		//compare with own window
		MeanWW = (aveg_min_dif[nT0] + aveg_max_dif[nT0]) / 2;
		MeanWW = (MeanWW * ratio + 8) / 16;

		MeanWW = MAX(MeanWW, (nsize * 6));  //44 5310
		nT2 = 0;

		frc_get_min_max_idx(frc_devp, flmW6Dif[nT0], MeanWW, aMnMxW, &wind_flag);
		nMin = flmW6Dif[nT0][HISDIFNUM - 1];
		nMax = flmW6Dif[nT0][HISDIFNUM - 1];
		for(nT1 = 0; nT1 < cadence; nT1++) {
			if (aMnMx[nT1] == 0 && aMnMxW[nT1] == 1)
				testflag = 1;	  //only compare 0

			if (aMnMxW[nT1] == 0)
				nT2 = nT2 + 1;

			nT3 = flmW6Dif[nT0][HISDIFNUM-1-nT1];
			//min/max
			if (nT3<nMin)
				nMin = nT3;

			if (nT3>nMax)
				nMax = nT3;
		}

		//phase is wrong in the case of drop frame
		if (testflag == 1 && g_stfilm_detect_item->phase_error_flag == 1
		    && nT2 >= (cuntnum - 1) && mix_modpre == 0) {
			testflag = 0;
			mix_modnum_cown[nT0] = 0;
		}

		if (nT0 < 3) {//11-01-18
			nMean = (nMax+nMin)/10;
			//the dif of max and min is small may be mix_mode
			if ((nMax - nMin) < nMean && nMin > 1024)
				testflag = 1;
		}

		//testflag = frc_get_min_max_idx(frc_devp, &(prm_fd_mod),flmW6Dif[nT0],MeanWW,aMnMxW);
		if(testflag == 0) {
			//if(mix_modnum_cown[nT0]>0&&
			//(flmW6Dif[nT0][HISDIFNUM-1]<(mean_other_min*5/2)||
			//(aveg_max_dif[nT0]/aveg_min_dif[nT0]>30&&
			//flmW6Dif[nT0][HISDIFNUM-1-idx]>(nsize<<9)))&&aMnMx[0]==0)
			if (mix_modnum_cown[nT0] > 0 &&
			    (flmW6Dif[nT0][HISDIFNUM - 1] < (mean_other_min * 5 / 2)) &&
			    aMnMx[0] == 0) {
				if (mix_modnum_cown[nT0]>(cadence+1)) {
					//mix_modnum_cown[nT0] = mix_modnum_cown[nT0] -1;
					mix_modnum_reset[nT0] = mix_modnum_reset[nT0] + 1;
					mix_modnum_quit_reset[nT0] = 0;
				}

			} else if (mix_modnum_quit_reset[nT0] > 30) {
				mix_modnum_reset[nT0]  = MAX(0,mix_modnum_reset[nT0] -1);
				mix_modnum_quit_reset[nT0] = mix_modnum_quit_reset[nT0] +1;

			} else {
				mix_modnum_quit_reset[nT0] = mix_modnum_quit_reset[nT0] + 1;
			}
		} else {
			mix_modnum_quit_reset[nT0] = 0;
			mix_modnum_cown[nT0] = mix_modnum_cown[nT0] + 1;
			mix_modnum_reset[nT0] = MAX(mix_modnum_reset[nT0] - 1,0);
			if(flmW6Dif[nT0][HISDIFNUM - 1] > (aveg_min_dif[nT0] * 2) && aMnMx[0] == 0)
				mix_modnum_reset[nT0] = 0;
		}

		if (aMnMx[0] == 0 && flmW6Dif[nT0][HISDIFNUM - 1] < 100 &&
		    g_stfilm_detect_item->phase_error_flag == 0 &&
		    (mix_modnum_cother[nT0] > 0 || mix_modnum_cpre[nT0] > 0)) {
			mix_modnum_reset[nT0] = mix_modnum_reset[nT0] + 1;
		}

		if (mix_modnum_cown[nT0] > 255) {
			mix_modnum_reset[nT0] = 0;
			mix_modnum_cown[nT0] = 255;
		}

		if (mix_modnum_reset[nT0] > g_stfilm_ctrl_para->mm_reset_thd) {
			mix_modnum_cown[nT0] = 0;
			mix_modnum_cother[nT0] = 0;
			mix_modnum_cpre[nT0] = 0;
			mix_modnum_reset[nT0] = 0;
		}
	}

	for (nT0 = 0; nT0 < wind_num; nT0++) {
		if (mix_modnum_cpre[nT0] > g_stfilm_ctrl_para->mm_cpre_thd ||
		    mix_modnum_cown[nT0] > g_stfilm_ctrl_para->mm_cown_thd||
		    mix_modnum_cother[nT0] >= g_stfilm_ctrl_para->mm_cother_thd) {
			mix_mod = 1;
			break;
		}
	}

	return mix_mod;
}

void  frc_update_film_a1a2_flag(struct frc_dev_s *frc_devp, u8 input_n, u8 output_m,
					u8 table_cnt, u8 mode, u8 cadence_cnt)
{
	int A1A2_change_flag_org[65];
	int film_phase_org[65];
	int tb_idx = 0, j = 0, i = 0, A1A2_change_temp = 0, idx = 0;
	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	struct st_film_data_item *g_stfilmdata_item;
	//struct st_film_detect_rd *g_stfilmdetect_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	//g_stfilmdetect_rd = &frc_data->g_stfilmdetect_rd;

	A1A2_change_flag_org[0] = 1;

	//get film phase ,frc phase ,A1A2 change
	while (tb_idx < table_cnt) {
		if (((j * input_n + g_stfilm_detect_item->mode_shift) >= (i * output_m)) &&
		    ((j * input_n + g_stfilm_detect_item->mode_shift) < ((i + 1) * output_m))) {
		    film_phase_org[tb_idx] = i % cadence_cnt;
			tb_idx++;
			j++;
			A1A2_change_temp = 0;
		} else if ((j * input_n + g_stfilm_detect_item->mode_shift) >= ((i + 1) * output_m)) {
			A1A2_change_temp = 1;
			i++;
		}
		A1A2_change_flag_org[tb_idx] = A1A2_change_temp ;
	}

	if (mode > 0) {//for film
		for (tb_idx=0; tb_idx<table_cnt; tb_idx++) {
			if (film_phase_org[tb_idx] == 1) {
				break;
			}
		}
	}

	for(i = 0; i < table_cnt; i++) {
		idx = (i + tb_idx >= table_cnt) ? (i + tb_idx - table_cnt) : (i + tb_idx);
		g_stfilmdata_item->A1A2_change_flag[i] = A1A2_change_flag_org[idx];
	}
}

void frc_initial_point(struct frc_dev_s *frc_devp, int frame_buf_num)
{
	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	//struct st_film_data_item *g_stfilmdata_item;
	//struct st_film_detect_rd *g_stfilmdetect_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	//g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	//g_stfilmdetect_rd = &frc_data->g_stfilmdetect_rd;

	//(g_stfilm_detect_item->input_fid_fw + 10 + 1)%10;
	g_stfilm_detect_item->nex_table_point = g_stfilm_detect_item->inout_table_point;
	//(g_stfilm_detect_item.input_fid_fw + 10 )%10;
	g_stfilm_detect_item->cur_table_point  =
		(g_stfilm_detect_item->inout_table_point -1 + frame_buf_num) % frame_buf_num;
	//(g_stfilm_detect_item.input_fid_fw + 10 -1)%10;
	g_stfilm_detect_item->pre_table_point  =
		(g_stfilm_detect_item->inout_table_point - 2 + frame_buf_num) % frame_buf_num;
	g_stfilm_detect_item->pre_idx = 0;
	g_stfilm_detect_item->cur_idx = 0;
	g_stfilm_detect_item->nex_idx = 0;
}

void frc_film_add_7_seg(struct frc_dev_s *frc_devp)
{
	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	//struct st_film_data_item *g_stfilmdata_item;
	//struct st_film_detect_rd *g_stfilmdetect_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	//g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	//g_stfilmdetect_rd = &frc_data->g_stfilmdetect_rd;

	u8 film_mode;
	u8 color1     = 1;    // red
	u8 color2     = 0;    // white
	u8 fwhw_sel   = READ_FRC_REG(FRC_REG_FILM_PHS_1) >> 16 & 0x1;//Bit 16     reg_film_hwfw_sel
	u8 mode_hw    = READ_FRC_REG(FRC_REG_FILM_PHS_1) >> 8 & 0xff;//Bit 15: 8  ro_frc_film_mode_hw

	film_mode     = fwhw_sel ? g_stfilm_detect_item->filmMod : mode_hw;

	// mode
	color1 = film_mode >= 0xA ? 3 : 1;      //  blue : red
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, 1 << 15, BIT_15);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, color1 << 12, 0x7000);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, film_mode << 8, 0xF00);

	// fwhw_sel
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, 1 << 7, BIT_7);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, color2 << 4, 0x70);
	UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_POSI_AND_NUM11_NUM12, fwhw_sel, 0xF);
}

u8 frc_frame_buf_id_reset(struct frc_dev_s *frc_devp, int frame_buf_num, int latch_err)
{
	u8 nT0,nT1,nT2;
	u8 input_fid;
	u8 reset_flag = 0;
	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	struct st_film_data_item *g_stfilmdata_item;
	//struct st_film_detect_rd *g_stfilmdetect_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	//g_stfilmdetect_rd = &frc_data->g_stfilmdetect_rd;

	if (g_stfilm_detect_item->jump_simple_mode) {
		//xil_printf("rp=%x,in=%x,pre=%x\n\r",g_stfilm_detect_item->replace_id_point,
		//g_stfilm_detect_item->input_id_point,g_stfilm_detect_item->pre_table_point);
		input_fid = g_stfilmdata_item->input_id_table[g_stfilm_detect_item->input_id_point];
		if (g_stfilm_detect_item->replace_id_point > g_stfilm_detect_item->input_id_point) {
			for (nT0 = 0; nT0 <
			     g_stfilm_detect_item->replace_id_point -
			     g_stfilm_detect_item->input_id_point - 1; nT0++)
				g_stfilmdata_item->input_id_table[nT0] =
					g_stfilmdata_item->input_id_table[nT0 +
					g_stfilm_detect_item->input_id_point + 1];
		} else {

			nT1 = g_stfilm_detect_item->replace_id_point +
				g_stfilm_detect_item->input_id_length -
				g_stfilm_detect_item->input_id_point - 1;
			//xil_printf("nT1=%x,f=%d\n\r",nT1,
			//g_stfilmdata_item.input_id_table[g_stfilm_detect_item->input_id_point+1]);
			for (nT0 = g_stfilm_detect_item->replace_id_point; nT0 < nT1; nT0++) {
				g_stfilmdata_item->input_id_table[nT0] =
				g_stfilmdata_item->input_id_table[nT0 +
					g_stfilm_detect_item->input_id_point + 1 -
					g_stfilm_detect_item->replace_id_point];
				//g_stfilmdata_item.input_id_table[nT0] =
				//g_stfilmdata_item.input_id_table[nT0 +
				//g_stfilm_detect_item->input_id_point + 1];

			}
		}
		g_stfilmdata_item->input_id_table[frame_buf_num-1] = input_fid;

		nT1 = nT0 - 1 ;

		//g_stfilmdata_item.input_id_table[frame_buf_num-1] =
		//g_stfilmdata_item.input_id_table[g_stfilm_detect_item->input_id_point];
		g_stfilm_detect_item->input_id_length = frame_buf_num;

	        //xil_printf("pre=%x,in=%x,nT1=%d\n\r", g_stfilm_detect_item->pre_table_point,
	        //g_stfilm_detect_item->inout_table_point, nT1);

	        if (g_stfilm_detect_item->pre_table_point > g_stfilm_detect_item->inout_table_point) {
	        	for (nT0 = g_stfilm_detect_item->pre_table_point;
	        	    nT0 < frame_buf_num; nT0++) {
				if (nT0 == g_stfilm_detect_item->pre_table_point) {
					nT1 = nT1 + 1;
					g_stfilmdata_item->input_id_table[nT1] =
					g_stfilmdata_item->inout_id_table[
					g_stfilm_detect_item->pre_table_point][
					g_stfilm_detect_item->pre_idx] & 0xF;
				} else {
					for (nT2 = 0; nT2 < 8; nT2++) {
						nT1 = nT1 + 1;
						if (g_stfilmdata_item->inout_id_table[nT0][nT2] <
						    frame_buf_num) {
							g_stfilmdata_item->input_id_table[nT1] =
							g_stfilmdata_item->inout_id_table[nT0][nT2];
						} else {
							g_stfilmdata_item->input_id_table[nT1] =
							g_stfilmdata_item->inout_id_table[nT0][nT2] &
							0xF;
							break;
						}

					}

				}
			}

	        	for (nT0 = 0; nT0 <= g_stfilm_detect_item->inout_table_point; nT0++) {
	        		if (nT0 == g_stfilm_detect_item->inout_table_point) {
					for (nT2 = 0; nT2 <= g_stfilm_detect_item->inout_idx; nT2++) {
						nT1 = nT1 + 1;
						g_stfilmdata_item->input_id_table[nT1] =
						g_stfilmdata_item->inout_id_table[nT0][nT2];
						//if(g_stfilmdata_item->inout_id_table[nT0][nT2]<frame_buf_num)
						//	g_stfilmdata_item->input_id_table[nT1] =
						//	g_stfilmdata_item->inout_id_table[nT0][nT2];
						//else
						//{
						//	g_stfilmdata_item->input_id_table[nT1] =
						//g_stfilmdata_item->inout_id_table[nT0][nT2]&0xf;
						//	break;
						//}
					}
				} else {
	        			for (nT2 = 0; nT2 < 8; nT2++) {
						nT1 = nT1 + 1;
						if (g_stfilmdata_item->inout_id_table[nT0][nT2] <
						    frame_buf_num) {
							g_stfilmdata_item->input_id_table[nT1] =
							g_stfilmdata_item->inout_id_table[nT0][nT2];
						} else {
							g_stfilmdata_item->input_id_table[nT1] =
							g_stfilmdata_item->inout_id_table[nT0][nT2] &
							0xf;
							break;
						}
					}
	        		}
	        	}
		}  else {
	        	for (nT0 = g_stfilm_detect_item->pre_table_point;
	        	     nT0 <= g_stfilm_detect_item->inout_table_point; nT0++) {
				if (nT0 == g_stfilm_detect_item->pre_table_point) {
					nT1 = nT1 + 1;
					g_stfilmdata_item->input_id_table[nT1] =
					g_stfilmdata_item->inout_id_table[
					g_stfilm_detect_item->pre_table_point][
					g_stfilm_detect_item->pre_idx] & 0xF;
					//xil_printf("pre_fid=%x\n\r",
					//g_stfilmdata_item->inout_id_table[
					//g_stfilm_detect_item->pre_table_point][
					//g_stfilm_detect_item->pre_idx]);
				} else if (nT0 == g_stfilm_detect_item->inout_table_point) {
					for (nT2 = 0; nT2 <= g_stfilm_detect_item->inout_idx; nT2++) {
						nT1 = nT1 + 1;
						g_stfilmdata_item->input_id_table[nT1] =
						g_stfilmdata_item->inout_id_table[nT0][nT2];
						//if(g_stfilmdata_item->inout_id_table[nT0][nT2]<
						//frame_buf_num)
						//	g_stfilmdata_item->input_id_table[nT1] =
						//g_stfilmdata_item->inout_id_table[nT0][nT2];
						//else
						//{
						//	g_stfilmdata_item->input_id_table[nT1] =
						//g_stfilmdata_item->inout_id_table[nT0][nT2]&0xf;
						//	break;
						//}
					}
				} else {
					for (nT2 = 0; nT2 < 8; nT2++) {
						nT1 = nT1 + 1;
						if (g_stfilmdata_item->inout_id_table[nT0][nT2] <
						    frame_buf_num) {
							g_stfilmdata_item->input_id_table[nT1] =
							g_stfilmdata_item->inout_id_table[nT0][nT2];
						} else {
							g_stfilmdata_item->input_id_table[nT1] =
							g_stfilmdata_item->inout_id_table[nT0][nT2] &
							0xf;
							break;
						}
					}
				}
			}
		}
	}
	// reset input_id_table from 0 to 15

	for (nT0 = 0; nT0 < frame_buf_num; nT0++) {
		for (nT1 = 0; nT1 < frame_buf_num; nT1++) {
			if (g_stfilmdata_item->input_id_table[nT0] ==
			    g_stfilmdata_item->input_id_table[nT1]) {
				reset_flag = 1;
				break;
			}
		}

		if (reset_flag == 1) {
			break;
		}
	}

	if (latch_err || reset_flag) {// reset input_id_table from 0 to 15
		for(nT0=0;nT0<frame_buf_num;nT0++)
			g_stfilmdata_item->input_id_table[nT0] = nT0;
	}

	//xil_printf("pre_tbl_pnt=%x,io_tbl_pnt=%x\n\r", g_stfilm_detect_item->pre_table_point,g_stfilm_detect_item->inout_table_point);

	g_stfilm_detect_item->input_id_point = g_stfilm_detect_item->input_id_length - 1;
	// xil_printf("id_tbl:");
	for (nT0=0; nT0<g_stfilm_detect_item->input_id_length; nT0++)
	{
		//xil_printf("%x,",g_stfilmdata_item->input_id_table[nT0]);
	}
	// xil_printf("\n\r");
	return 0;
}

//input: AB_chang_flag
//       change_flag  nex_point change flag
//       frame_buf_num  fb_num
//       change_num   from input long cadence nex point change num
//output: replace_id_point
//        input_id_table
//        inout_id_table
void frc_jump_frame_simple_mode(struct frc_dev_s *frc_devp, u8 *AB_change_flag,
					u8 change_flag, u8 mode_pre, u8 frame_buf_num,
					u8 change_num, u8 pre_idx_pre)
{
	u8 nT0 = 0, nT1 = 1/*, nT2 = 2*/;
	u8 pre_id_point;

	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	struct st_film_data_item *g_stfilmdata_item;
	//struct st_film_detect_rd *g_stfilmdetect_rd;

	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	//g_stfilmdetect_rd = &frc_data->g_stfilmdetect_rd;

	if (mode_pre == 0) {    //video->long cadence
		//xil_printf("pre_fid=%x,point=%x,idx=%x\n\r",
		//g_stfilmdata_item->inout_id_table[g_stfilm_detect_item->pre_table_point][g_stfilm_detect_item->pre_idx],
		//g_stfilm_detect_item->pre_table_point,g_stfilm_detect_item->pre_idx);
		for (pre_id_point = 0; pre_id_point < frame_buf_num; pre_id_point++) {
			if (g_stfilmdata_item->input_id_table[pre_id_point] ==
			    (g_stfilmdata_item->inout_id_table[g_stfilm_detect_item->pre_table_point][g_stfilm_detect_item->pre_idx] & 0xF))
				break;
		}

		if (pre_id_point > g_stfilm_detect_item->input_id_point) {
			for (nT0=0;nT0<frame_buf_num-g_stfilm_detect_item->input_id_point;nT0++) {
				g_stfilmdata_item->input_id_table[nT0] =
				g_stfilmdata_item->input_id_table[nT0+g_stfilm_detect_item->input_id_point + 1];
			}
			g_stfilm_detect_item->replace_id_point = frame_buf_num - 4;
			//g_stfilm_detect_item->replace_id_point =
			//frame_buf_num -  (frame_buf_num+g_stfilm_detect_item->input_id_point + 1-pre_id_point);
		} else {
			nT1 = frame_buf_num - (g_stfilm_detect_item->input_id_point + 1) + pre_id_point;
			for (nT0 = pre_id_point; nT0 < nT1; nT0++) {
				g_stfilmdata_item->input_id_table[nT0] =
				g_stfilmdata_item->input_id_table[nT0+g_stfilm_detect_item->input_id_point-pre_id_point + 1];
				//xil_printf("nT0=%x,id=%x\n\r",nT0,g_stfilmdata_item->input_id_table[nT0]);
			}
			g_stfilm_detect_item->replace_id_point = frame_buf_num - 4;
			//g_stfilm_detect_item->replace_id_point = frame_buf_num - (g_stfilm_detect_item->input_id_point + 1-pre_id_point);

		}
		g_stfilm_detect_item->input_id_point = g_stfilm_detect_item->input_id_length - 1;
	}

	if (change_flag) {
		if (g_stfilm_detect_item->cadence_chang_mode == 0 && change_num < 3) {
			g_stfilmdata_item->input_id_table[g_stfilm_detect_item->replace_id_point] =
				g_stfilmdata_item->inout_id_table[(g_stfilm_detect_item->pre_table_point - 1 + frame_buf_num) % frame_buf_num][0] & 0xF;
			g_stfilm_detect_item->replace_id_point = (g_stfilm_detect_item->replace_id_point + 1) % g_stfilm_detect_item->input_id_length;

			if (change_num == 2) {
				g_stfilmdata_item->input_id_table[g_stfilm_detect_item->replace_id_point] =
				g_stfilmdata_item->inout_id_table[g_stfilm_detect_item->pre_table_point][0]&0xF;
				g_stfilm_detect_item->replace_id_point =
					(g_stfilm_detect_item->replace_id_point + 1) %
					g_stfilm_detect_item->input_id_length;
			}

		} else {
			g_stfilmdata_item->input_id_table[g_stfilm_detect_item->replace_id_point] =
				g_stfilmdata_item->inout_id_table[(g_stfilm_detect_item->pre_table_point - 1 + frame_buf_num) % frame_buf_num][pre_idx_pre] & 0xF;
			g_stfilm_detect_item->replace_id_point = (g_stfilm_detect_item->replace_id_point + 1) % g_stfilm_detect_item->input_id_length;
			if (g_stfilm_detect_item->pre_idx == 1)
			{
				g_stfilmdata_item->input_id_table[g_stfilm_detect_item->replace_id_point] =
				g_stfilmdata_item->inout_id_table[g_stfilm_detect_item->pre_table_point][0] & 0xF;
				g_stfilm_detect_item->replace_id_point = (g_stfilm_detect_item->replace_id_point + 1) % g_stfilm_detect_item->input_id_length;
			}
		}
	}

	if (AB_change_flag[0] == 0 && AB_change_flag[1] == 0) {
		g_stfilmdata_item->input_id_table[g_stfilm_detect_item->replace_id_point] =
			g_stfilmdata_item->inout_id_table[g_stfilm_detect_item->inout_table_point][g_stfilm_detect_item->inout_idx-1] & 0xF;
		g_stfilmdata_item->inout_id_table[g_stfilm_detect_item->inout_table_point][g_stfilm_detect_item->inout_idx-1] =
			g_stfilmdata_item->inout_id_table[g_stfilm_detect_item->inout_table_point][g_stfilm_detect_item->inout_idx] | 0x10;
		g_stfilm_detect_item->inout_idx = g_stfilm_detect_item->inout_idx -1 ;
		g_stfilm_detect_item->replace_id_point =
			(g_stfilm_detect_item->replace_id_point + 1) % g_stfilm_detect_item->input_id_length;
	}

	//debug
	//xil_printf("rp_id=%x\n\r",g_stfilmdata_item->input_id_table[g_stfilm_detect_item.replace_id_point]);
	//xil_printf("pp_idx=%x\n\r",pre_idx_pre);
	//xil_printf("r_p=%x,c_flg/num=%x,%d\n\r"  ,g_stfilm_detect_item.replace_id_point,change_flag,change_num);
	//xil_printf("r_p=%x\n\r", g_stfilm_detect_item.replace_id_point);
	//xil_printf("ip_id_p=%x\n\r",g_stfilm_detect_item.input_id_point);
	//xil_printf("pre_tbl_pnt=%x\n\r",g_stfilm_detect_item.pre_table_point);
	//xil_printf("ip_id_tbl:");

	for (nT0 = 0; nT0 < g_stfilm_detect_item->input_id_length; nT0++) {
		//xil_printf("%x,",g_stfilmdata_item->input_id_table[nT0]);
	}
}

void frc_film_badedit_ctrl(struct frc_dev_s *frc_devp, u32 *flmGDif,u8 mode)
{
	s8 nT0,nT1;
	u32 step;
	u8 table_cnt,comb_AB_change_cnt,expect_AB_change_cnt,flag,zeros_cnt;
	u8 change_flag, idx, ro_input_fid, ro_input_fid_p = 0;
	u8 AB_change_flag_org[HISDIFNUM];                 //real AB change flag
	u8 expect_AB_change_flag_org[HISDIFNUM];          //theoretically AB change flag
	u8 comb_AB_change[HISDIFNUM];                     //combined theoretical and practical results
	static u8 comb_AB_change_flag_org[HISDIFNUM];     //combined theoretical and practical results
	static u8 mode_pre = 0xFF;
	struct frc_fw_data_s *frc_data;
	struct st_film_detect_item *g_stfilm_detect_item;
	struct st_film_data_item *g_stfilmdata_item;
	struct st_film_ctrl_para *g_stfilmctrl_para;


	frc_data = (struct frc_fw_data_s *)frc_devp->fw_data;
	g_stfilm_detect_item = &frc_data->g_stfilm_detect_item;
	g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	g_stfilmdata_item = &frc_data->g_stfilmdata_item;
	g_stfilmctrl_para = &frc_data->g_stfilmctrl_para;

	u32  sumGdif = 0;                                   //sum of global dif
	u32  AveragG = 0;                                   //average global dif
	u32  point;
	u8  phase           = g_stfilm_detect_item->phase;
	u8  input_n         = g_stfilm_detect_item->input_n;
	u8  output_m        = g_stfilm_detect_item->output_m;
	u8  frame_buf_num   = READ_FRC_REG(FRC_FRAME_BUFFER_NUM) & 0x1F;
	u8  cadence_cnt     = g_stfilmtable[mode].cadence_cnt;
	u8  frame_num       = g_stfilmtable[mode].average_num ;
	u8  max_zeros_cnt   = g_stfilmtable[mode].max_zeros_cnt + 1;
	u32 mask_value      = g_stfilmtable[mode].mask_value;
	u32 match_data      = frc_cycshift(g_stfilmtable[mode].match_data, cadence_cnt, mask_value, 1);
	u8  update_HW_flag  = 0;

	u8 pre_idx_pre      = 0;
	u8 skip_flag        = 0;
	u8 latch_err        = 0;
	u8 raw_fid          = 0;

	static u8  input_fid      = 2;
	static u8  BN             = 0;
	static u8  change_num     = 0;
	static u8  start_change   = 0;

	static u8  step_ofset_cnt = 0;



	u32 mem  = 0 ;

	// after reading ro_frc_hw_latch_error (bit30), set reg_clr_fw_proc_err_flag=1  immediately
	mem    = READ_FRC_REG(FRC_REG_FWD_SIGN_RO);       //ro_frc_opre/ocur/onex_point
	//Bit 0 , default = 0,this bit use as pulse reg_clr_fw_proc_err_flag
	UPDATE_FRC_REG_BITS(FRC_BE_CLR_FLAG, 1, 0x1);
	latch_err = (mem >> 30) & 0x1;     //Bit 30     ro_frc_hw_latch_error

	g_stfilm_detect_item->input_fid_fw = input_fid;               // last input_fid
	input_fid = READ_FRC_REG(FRC_REG_FWD_FID) >> 24 & 0xF;      //Bit 27:24  reg_frc_input_fid

	raw_fid = READ_FRC_REG(FRC_REG_PAT_POINTER) >> 4 & 0xF;      //Bit 27:24  reg_frc_input_fid

	pr_frc(dbg_film + 1, "input_fid=%d,raw_fid=%d\n", input_fid,raw_fid);

	//table count update
	if((output_m * cadence_cnt % input_n) != 0) {
		table_cnt = output_m * cadence_cnt;
	} else {
		table_cnt = output_m * cadence_cnt / input_n;
	}

	//Initialization
	if (mode_pre != mode) {
		frc_update_film_a1a2_flag(frc_devp, input_n, output_m, table_cnt, mode, cadence_cnt);
		for (nT0 = 0; nT0 < table_cnt; nT0++) {
			if (nT0 < 32)
				UPDATE_FRC_REG_BITS(FRC_REG_LOAD_FRAME_FLAG_0,
					g_stfilmdata_item->A1A2_change_flag[nT0], 1 << nT0);
			else
				UPDATE_FRC_REG_BITS(FRC_REG_LOAD_FRAME_FLAG_1,
					g_stfilmdata_item->A1A2_change_flag[nT0], 1 << (nT0 - 32));
		}
	}

	if (BN == 0) {
		for(nT0 = 0; nT0 < HISDIFNUM; nT0++)
			comb_AB_change_flag_org[nT0] = 1;
		BN = 1;
	} else {
		for (nT0 = frame_buf_num - 2; nT0 >= 0; nT0--)
			g_stfilmdata_item->frame_buffer_id[nT0+1] =
				g_stfilmdata_item->frame_buffer_id[nT0];  //update fid
		//update AB_change_flag
		for (nT0 = HISDIFNUM - 2; nT0 >= 0; nT0--)
			comb_AB_change_flag_org[nT0 + 1] = comb_AB_change_flag_org[nT0];
	}
	g_stfilmdata_item->frame_buffer_id[0] = g_stfilm_detect_item->input_fid_fw;

	pre_idx_pre = g_stfilm_detect_item->pre_idx;

	// jump_id_flag --> for long cadence use
	if (g_stfilmtable[mode].jump_id_flag||g_stfilmtable[mode_pre].jump_id_flag) {
		if ((g_stfilm_detect_item->cur_table_point == ((mem >> 24) & 0xf)) &&
		    g_stfilmtable[mode_pre].jump_id_flag) {
			skip_flag = 1;
			change_num = change_num<255 ? change_num + 1 : change_num;
		}
		g_stfilm_detect_item->pre_table_point = (mem >> 24) & 0xF;
		g_stfilm_detect_item->cur_table_point = (mem >> 20) & 0xF;
		g_stfilm_detect_item->nex_table_point = (mem >> 16) & 0xF;
		g_stfilm_detect_item->pre_idx 		= (mem >> 8)  & 0x7;
		g_stfilm_detect_item->cur_idx		= (mem >> 4)  & 0x7;
		g_stfilm_detect_item->nex_idx 		= (mem) & 0x7;
	}

	//calculate "0" ,"1" dif th
	for (nT0 = 0; nT0 < frame_num; nT0++)
		sumGdif = sumGdif + flmGDif[HISDIFNUM - 1 - nT0];

	AveragG = (sumGdif + frame_num / 2) / (frame_num);
	AveragG = (AveragG * g_stfilmctrl_para->glb_ratio + 8) / 16;         //average
	AveragG = AveragG + g_stfilmctrl_para->glb_ofset;                  // mode 1/0 th

	AveragG = MAX(AveragG, g_stfilmctrl_para->min_diff_th);

	phase = phase == 0 ? cadence_cnt -1 : phase - 1;
	//flmGdif[0] phase
	phase = (phase - (HISDIFNUM - 1) + (HISDIFNUM) /
		 cadence_cnt * cadence_cnt + cadence_cnt) % cadence_cnt;

	//calculate AB_change_flag
	for (nT0 = HISDIFNUM - 1; nT0 >= 0; nT0--) {
		if (flmGDif[HISDIFNUM - nT0 - 1] > AveragG || (mode == 0)) {
			zeros_cnt = 0;
			AB_change_flag_org[nT0] = 1;
			change_flag = 1;
		} else {
			zeros_cnt = zeros_cnt + 1;
			AB_change_flag_org[nT0] = 0;
		}

		if (mode > 0) {
			//check if it is a difference frame
			flag = frc_cycshift(match_data,cadence_cnt, mask_value, phase) & 1;
			phase = phase ==cadence_cnt-1 ? 0 : phase+1;
		} else
			flag = 1;
		if (flag==1)
			expect_AB_change_flag_org[nT0] = 1;
		else
			expect_AB_change_flag_org[nT0] = 0;

		comb_AB_change[nT0] = AB_change_flag_org[nT0];
		if (zeros_cnt > max_zeros_cnt) {
			comb_AB_change[nT0] = expect_AB_change_flag_org[nT0];
			if (change_flag)
				comb_AB_change[nT0] = 1;
			change_flag = 0;
		}
	}
	comb_AB_change_flag_org[0] = comb_AB_change[0];

	if (comb_AB_change_flag_org[0]) {
		//new frame "1"
		g_stfilmdata_item->inout_id_table[
			g_stfilm_detect_item->inout_table_point][g_stfilm_detect_item->inout_idx] =
			g_stfilmdata_item->inout_id_table[
			g_stfilm_detect_item->inout_table_point][
			g_stfilm_detect_item->inout_idx] | 0x10;
		g_stfilm_detect_item->inout_idx  = 0;
		g_stfilm_detect_item->inout_table_point =
			(g_stfilm_detect_item->inout_table_point + 1) % frame_buf_num;

		if (mode == 0 && mode_pre == 0) {
			//g_stfilmdata_item->input_id_table[(g_stfilm_detect_item->input_id_point-
			//1+frame_buf_num)%frame_buf_num]|0x10;
			g_stfilmdata_item->inout_id_table[
				g_stfilm_detect_item->inout_table_point][0] = input_fid;
		} else if (mode != 0 && mode_pre == 0) {
			g_stfilm_detect_item->inout_table_point =
				(g_stfilm_detect_item->inout_table_point -1 +frame_buf_num)%frame_buf_num;
			g_stfilmdata_item->inout_id_table[
				g_stfilm_detect_item->inout_table_point][0] =
					g_stfilmdata_item->frame_buffer_id[0];
		} else {
			g_stfilmdata_item->inout_id_table[
				g_stfilm_detect_item->inout_table_point][0] =
				g_stfilmdata_item->frame_buffer_id[0];
		}
		g_stfilmdata_item->input_cover_flag_table[g_stfilmdata_item->frame_buffer_id[0]] = 0;
	} else {
		 //old frame "0"
		g_stfilm_detect_item->inout_idx  = g_stfilm_detect_item->inout_idx  + 1;
		g_stfilmdata_item->inout_id_table[
			g_stfilm_detect_item->inout_table_point][g_stfilm_detect_item->inout_idx] =
					g_stfilmdata_item->frame_buffer_id[0];
		g_stfilmdata_item->input_cover_flag_table[g_stfilmdata_item->frame_buffer_id[0]] = 1;
		g_stfilmdata_item->input_cover_point_table[g_stfilmdata_item->frame_buffer_id[0]] =
			g_stfilm_detect_item->inout_table_point;
		g_stfilmdata_item->input_cover_idx_table[g_stfilmdata_item->frame_buffer_id[0]] =
			g_stfilm_detect_item->inout_idx ;
	}

	if (mode_pre != mode) {   //mode change

		change_num     = 0;
		step_ofset_cnt = 0;
		if (g_stfilmtable[mode].jump_id_flag == 1 &&
		    g_stfilmtable[mode_pre].jump_id_flag == 1)
			g_stfilm_detect_item->cadence_chang_mode = 2;
		else if (g_stfilmtable[mode].jump_id_flag==1 && mode_pre == 0)
			g_stfilm_detect_item->cadence_chang_mode = 0;
		else
			g_stfilm_detect_item->cadence_chang_mode = 1;

		frc_update_film_a1a2_flag(frc_devp, input_n,output_m,table_cnt,mode,cadence_cnt);

		if (g_stfilm_detect_item->mode_change_adjust_en) {
			//update AB_change_flag
			for (nT0 = 0; nT0 < HISDIFNUM; nT0++)
				comb_AB_change_flag_org[nT0] = expect_AB_change_flag_org[nT0];

			g_stfilm_detect_item->otb_start = 1;
			//(g_stfilm_detect_item.input_fid_fw + 10)%10;
			g_stfilm_detect_item->nex_table_point =
				g_stfilm_detect_item->inout_table_point;
			start_change = 0;
			update_HW_flag = 1;
		} else {
			update_HW_flag = 0;
		}
	}

	if (latch_err == 1) {
		update_HW_flag = 1;
		frc_initial_point(frc_devp, frame_buf_num);
		//xil_printf("latch_err!\n\r");
	}

	// long cadence  --in
	//if(g_stfilmtable[mode].jump_id_flag)
	if (g_stfilmtable[mode].jump_id_flag || g_stfilmtable[mode_pre].jump_id_flag) {
		if (g_stfilmtable[mode].jump_id_flag) {
			//g_stfilm_detect_item->replace_id_point =
			//g_stfilm_detect_item->input_id_length;
			g_stfilm_detect_item->input_id_length =
				frame_buf_num + g_stfilmtable[mode].simple_jump_add_buf;
		}

		if( g_stfilm_detect_item->jump_simple_mode)
			frc_jump_frame_simple_mode(frc_devp, comb_AB_change_flag_org, skip_flag,
						   mode_pre,frame_buf_num,change_num,pre_idx_pre);
	}

	// long cadence  --out
	if (g_stfilmtable[mode_pre].jump_id_flag && g_stfilmtable[mode].jump_id_flag == 0) {
		// fid reset within input_id_talbe
		frc_frame_buf_id_reset(frc_devp, frame_buf_num,latch_err);
		//for(j=0;j<frame_buf_num;j++)
		//	g_stfilmdata_item->input_id_table[j] = j;
	}

	if (mode_pre != mode && (mode_pre == 0)) { // video->mode
		idx = 0;
		start_change = 0;
		//update AB_change_flag
		for (nT0 = 0; nT0 < HISDIFNUM; nT0++)
			comb_AB_change_flag_org[nT0] = expect_AB_change_flag_org[nT0];
		frc_initial_point(frc_devp, frame_buf_num);

		//for(nT0=0;nT0<frame_buf_num;nT0++)
			// g_stfilmdata_item->inout_id_table[nT0][0] =
			//g_stfilmdata_item->frame_buffer_id[(g_stfilm_detect_item->
			//inout_table_point-nT0+frame_buf_num)%frame_buf_num]|0x10;
	}

	if (mode_pre != mode && mode == 0) {  //mode->video
		g_stfilm_detect_item->inout_table_point =
			(g_stfilm_detect_item->inout_table_point + 1) % frame_buf_num;
		//g_stfilmdata_item->input_id_table[(g_stfilm_detect_item->input_id_point-1+
		//frame_buf_num)%frame_buf_num];
		g_stfilmdata_item->inout_id_table[g_stfilm_detect_item->inout_table_point][0] =
										input_fid;
		g_stfilm_detect_item->inout_idx = 0;
		if (g_stfilm_detect_item->mode_change_adjust_en) {
			//update AB_change_flag
			for (nT0 = 0; nT0 < HISDIFNUM; nT0++)
				comb_AB_change_flag_org[nT0] = expect_AB_change_flag_org[nT0];
			start_change = 0;
			//(g_stfilm_detect_item->input_fid_fw + 1 + 10)%10)
			if (g_stfilm_detect_item->nex_table_point !=
			    g_stfilm_detect_item->inout_table_point)
				g_stfilm_detect_item->otb_start = 1;
			frc_initial_point(frc_devp, frame_buf_num);
		}

		for (nT0 = 0; nT0 < frame_buf_num; nT0++)
			g_stfilmdata_item->inout_id_table[nT0][0] =
			g_stfilmdata_item->inout_id_table[nT0][0] | 0x10;
	}

	comb_AB_change_cnt = 0;
	expect_AB_change_cnt = 0;

	for (nT0 = 0; nT0 < frame_num; nT0++) {
		if (comb_AB_change_flag_org[nT0]==1)
			comb_AB_change_cnt = comb_AB_change_cnt + 1;
		if (expect_AB_change_flag_org[nT0]==1)
			expect_AB_change_cnt = expect_AB_change_cnt + 1;
	}

	step = (1 << 20) * comb_AB_change_cnt * input_n / (frame_num * output_m);

	g_stfilm_detect_item->input_id_point =
		g_stfilm_detect_item->input_id_point >= (g_stfilm_detect_item->input_id_length-1) ?
		0 : g_stfilm_detect_item->input_id_point + 1;

	//new input_fid which is going to be write to 16*8 table
	ro_input_fid = g_stfilmdata_item->input_id_table[g_stfilm_detect_item->input_id_point];
	//ro_input_fid_p->= g_stfilmdata_item->input_id_table[
	//(g_stfilm_detect_item->input_id_point-1+frame_buf_num)%frame_buf_num];   //pre fid

	//Bit 27:24   reg_frc_input_fid
	UPDATE_FRC_REG_BITS(FRC_REG_FWD_FID, (ro_input_fid << 24) | (ro_input_fid_p << 28),
			    0xFF000000);

	//Bit 21: 0    reg_frc_phase_delta
	UPDATE_FRC_REG_BITS(FRC_REG_FWD_PHS_ADJ,step,0x3FFFFF);
	// reg_frc_opre/ocur/onex_point
	UPDATE_FRC_REG_BITS(FRC_REG_FWD_FID_POSI,
			    (g_stfilm_detect_item->inout_table_point<<16),0xF0000);
	// reg_frc_opre/ocur/onex_idx
	UPDATE_FRC_REG_BITS(FRC_REG_FWD_FID_POSI,(g_stfilm_detect_item->inout_idx),0x7);

	for (nT0 = 0; nT0 < frame_buf_num; nT0++) {
		for (nT1 = 0; nT1 < 8; nT1++) {
			//if(nT1>0&&g_stfilmdata_item->inout_id_table[nT0][nT1-1]>16)
				//break;
			if (nT1 < 4)
				UPDATE_FRC_REG_BITS(FRC_REG_FWD_FID_LUT_2_0 +
					(FRC_REG_FWD_FID_LUT_2_1 - FRC_REG_FWD_FID_LUT_2_0) * nT0,
					g_stfilmdata_item->inout_id_table[nT0][nT1] <<
					(nT1 * 8),
					(1 << (nT1 * 8 + 5)) - (1 << (nT1 * 8)));
			else
				UPDATE_FRC_REG_BITS(FRC_REG_FWD_FID_LUT_1_0 +
					(FRC_REG_FWD_FID_LUT_1_1 - FRC_REG_FWD_FID_LUT_1_0) * nT0,
					g_stfilmdata_item->inout_id_table[nT0][nT1] <<
					((nT1 % 4) * 8),
					(1 << ((nT1 % 4) * 8 + 5)) - (1 << ((nT1 % 8) * 8)));

			}
			pr_frc(dbg_film + 1, "id:idx_%d:%d,%d,%d\n",nT0,g_stfilmdata_item->inout_id_table[nT0][0],g_stfilmdata_item->inout_id_table[nT0][1],g_stfilmdata_item->inout_id_table[nT0][2]);
		}
	if (update_HW_flag) {
		point = g_stfilm_detect_item->pre_table_point << 28 |
			g_stfilm_detect_item->cur_table_point << 24 |
			g_stfilm_detect_item->nex_table_point << 20;
		idx = g_stfilm_detect_item->pre_idx << 12 | g_stfilm_detect_item->cur_idx << 8 |
			g_stfilm_detect_item->nex_idx << 4;
		// Bit 29   reg_frc_force_point_idx_en
		UPDATE_FRC_REG_BITS(FRC_REG_FWD_SIGN_RO,1<<29,1<<29);
		// write    reg_frc_opre/ocur/onex_point
		UPDATE_FRC_REG_BITS(FRC_REG_FWD_FID_POSI,point,0xFFF00000);
		// write    reg_frc_opre/ocur/onex_idx
		UPDATE_FRC_REG_BITS(FRC_REG_FWD_FID_POSI,idx,0xFFF0);
		// Bit 3    reg_frc_otb_up_flag
		UPDATE_FRC_REG_BITS(FRC_REG_FWD_TABLE_CNT_PHAOFS,1<<3,0x8);
		// Bit 31   reg_frc_ophase_reset
		UPDATE_FRC_REG_BITS(FRC_REG_FWD_PHS_ADJ,1<<31,1<<31);
		//xil_printf("f:p=%x,idx=%x\n\r",point,idx);
	} else {
		UPDATE_FRC_REG_BITS(FRC_REG_FWD_TABLE_CNT_PHAOFS,0,0x8);//Bit  3    reg_frc_otb_up_flag
		// else, reset bit29 and bit31
		UPDATE_FRC_REG_BITS(FRC_REG_FWD_SIGN_RO,0,1<<29); //Bit 29    reg_frc_force_point_idx_en
		UPDATE_FRC_REG_BITS(FRC_REG_FWD_PHS_ADJ,0,1<<31); //Bit 31    reg_frc_ophase_reset
	}
	//Bit 0 , default = 0,this bit use as pulse reg_clr_fw_proc_err_flag
	//UPDATE_FRC_REG_BITS(FRC_BE_CLR_FLAG, 1, 0x1);
	UPDATE_FRC_REG_BITS(FRC_REG_FWD_SIGN_RO,1<<31,1<<31);//Bit 31    reg_frc_tbl_wr_down_en
	UPDATE_FRC_REG_BITS(FRC_REG_FWD_SIGN_RO,0,1<<31);    //Bit 31    reg_frc_tbl_wr_down_en

	mode_pre = mode;
	//u64 timestamp_out = sched_clock();

	mem    = READ_FRC_REG(FRC_REG_FWD_FID);       //ro_frc_opre/ocur/onex_point

	pr_frc(dbg_film + 1, "fid:pre=%d,cur=%d,nex=%d\n", (mem>>8)&0xF,(mem>>4)&0xF,(mem)&0xF);

	mem    = READ_FRC_REG(FRC_REG_FWD_SIGN_RO);       //ro_frc_opre/ocur/onex_point

	pr_frc(dbg_film + 1, "point:pre=%d,cur=%d,nex=%d,out=%d\n", (mem>>24)&0xF,(mem>>20)&0xF,(mem>>16)&0xF,g_stfilm_detect_item->inout_table_point);

	pr_frc(dbg_film + 1, "idx:pre=%d,cur=%d,nex=%d,out=%d\n", (mem>>8)&0x7,(mem>>4)&0x7,(mem>>2)&0x7,g_stfilm_detect_item->inout_idx);
}

