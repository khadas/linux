/*
 * drivers/amlogic/media/di_multi/di_prc.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/seq_file.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include "deinterlace.h"

#include "di_data_l.h"
#include "di_data.h"
#include "di_dbg.h"
#include "di_vframe.h"
#include "di_que.h"
#include "di_task.h"

#include "di_prc.h"
#include "di_pre.h"
#include "di_post.h"
#include "di_api.h"

/**************************************
 *
 * cfg ctr top
 *	bool
 **************************************/

const struct di_cfg_ctr_s di_cfg_top_ctr[K_DI_CFG_NUB] = {
	/*same order with enum eDI_DBG_CFG*/
	/* cfg for top */
	[eDI_CFG_BEGIN]  = {"cfg top begin ", eDI_CFG_BEGIN, 0},

	[eDI_CFG_first_bypass]  = {"first_bypass",
		eDI_CFG_first_bypass, 1},
	[eDI_CFG_ref_2]  = {"ref_2",
		eDI_CFG_ref_2, 0},
	[eDI_CFG_END]  = {"cfg top end ", eDI_CFG_END, 0},

};

char *di_cfg_top_get_name(enum eDI_CFG_TOP_IDX idx)
{
	return di_cfg_top_ctr[idx].name;
}

void di_cfg_top_get_info(unsigned int idx, char **name)
{
	if (di_cfg_top_ctr[idx].id != idx)
		PR_ERR("%s:err:idx not map [%d->%d]\n", __func__,
		       idx, di_cfg_top_ctr[idx].id);

	*name = di_cfg_top_ctr[idx].name;
}

bool di_cfg_top_check(unsigned int idx)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(di_cfg_top_ctr);
	if (idx >= tsize) {
		PR_ERR("%s:err:overflow:%d->%d\n",
		       __func__, idx, tsize);
		return false;
	}
	if (idx != di_cfg_top_ctr[idx].id) {
		PR_ERR("%s:err:not map:%d->%d\n",
		       __func__, idx, di_cfg_top_ctr[idx].id);
		return false;
	}
	pr_info("\t%-15s=%d\n", di_cfg_top_ctr[idx].name,
		di_cfg_top_ctr[idx].default_val);
	return true;
}

void di_cfg_top_init_val(void)
{
	int i;

	pr_info("%s:\n", __func__);
	for (i = eDI_CFG_BEGIN; i < eDI_CFG_END; i++) {
		if (!di_cfg_top_check(i))
			continue;
		di_cfg_top_set(i, di_cfg_top_ctr[i].default_val);
	}
	pr_info("%s:finish\n", __func__);
}

bool di_cfg_top_get(enum eDI_CFG_TOP_IDX id)
{
	return get_datal()->cfg_en[id];
}

void di_cfg_top_set(enum eDI_CFG_TOP_IDX id, bool en)
{
	get_datal()->cfg_en[id] = en;
}

/**************************************
 *
 * cfg ctr x
 *	bool
 **************************************/

const struct di_cfgx_ctr_s di_cfgx_ctr[K_DI_CFGX_NUB] = {
	/*same order with enum eDI_DBG_CFG*/

	/* cfg channel x*/
	[eDI_CFGX_BEGIN]  = {"cfg x begin ", eDI_CFGX_BEGIN, 0},
	/* bypass_all */
	[eDI_CFGX_BYPASS_ALL]  = {"bypass_all", eDI_CFGX_BYPASS_ALL, 0},
	[eDI_CFGX_END]  = {"cfg x end ", eDI_CFGX_END, 0},

	/* debug cfg x */
	[eDI_DBG_CFGX_BEGIN]  = {"cfg dbg begin ", eDI_DBG_CFGX_BEGIN, 0},
	[eDI_DBG_CFGX_IDX_VFM_IN] = {"vfm_in", eDI_DBG_CFGX_IDX_VFM_IN, 0},
	[eDI_DBG_CFGX_IDX_VFM_OT] = {"vfm_out", eDI_DBG_CFGX_IDX_VFM_OT, 1},
	[eDI_DBG_CFGX_END]    = {"cfg dbg end", eDI_DBG_CFGX_END, 0},
};

char *di_cfgx_get_name(enum eDI_CFGX_IDX idx)
{
	return di_cfgx_ctr[idx].name;
}

void di_cfgx_get_info(enum eDI_CFGX_IDX idx, char **name)
{
	if (di_cfgx_ctr[idx].id != idx)
		PR_ERR("%s:err:idx not map [%d->%d]\n", __func__,
		       idx, di_cfgx_ctr[idx].id);

	*name = di_cfgx_ctr[idx].name;
}

bool di_cfgx_check(unsigned int idx)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(di_cfgx_ctr);
	if (idx >= tsize) {
		PR_ERR("%s:err:overflow:%d->%d\n",
		       __func__, idx, tsize);
		return false;
	}
	if (idx != di_cfgx_ctr[idx].id) {
		PR_ERR("%s:err:not map:%d->%d\n",
		       __func__, idx, di_cfgx_ctr[idx].id);
		return false;
	}
	pr_info("\t%-15s=%d\n", di_cfgx_ctr[idx].name,
		di_cfgx_ctr[idx].default_val);
	return true;
}

void di_cfgx_init_val(void)
{
	int i, ch;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		for (i = eDI_CFGX_BEGIN; i < eDI_DBG_CFGX_END; i++)
			di_cfgx_set(ch, i, di_cfgx_ctr[i].default_val);
	}
}

bool di_cfgx_get(unsigned int ch, enum eDI_CFGX_IDX idx)
{
	return get_datal()->ch_data[ch].cfgx_en[idx];
}

void di_cfgx_set(unsigned int ch, enum eDI_CFGX_IDX idx, bool en)
{
	get_datal()->ch_data[ch].cfgx_en[idx] = en;
}

/**************************************
 *
 * module para top
 *	int
 **************************************/

const struct di_mp_uit_s di_mp_ui_top[] = {
	/*same order with enum eDI_MP_UI*/
	/* for top */
	[eDI_MP_UI_T_BEGIN]  = {"module para top begin ",
			eDI_MP_UI_T_BEGIN, 0},
	/**************************************/
	[eDI_MP_SUB_DI_B]  = {"di begin ",
			eDI_MP_SUB_DI_B, 0},
	[eDI_MP_force_prog]  = {"bool:force_prog:1",
			eDI_MP_force_prog, 1},
	[edi_mp_combing_fix_en]  = {"bool:combing_fix_en,def:1",
			edi_mp_combing_fix_en, 1},
	[eDI_MP_cur_lev]  = {"int cur_lev,def:2",
			eDI_MP_cur_lev, 2},
	[eDI_MP_pps_dstw]  = {"pps_dstw:int",
			eDI_MP_pps_dstw, 0},
	[eDI_MP_pps_dsth]  = {"pps_dsth:int",
			eDI_MP_pps_dsth, 0},
	[eDI_MP_pps_en]  = {"pps_en:bool",
			eDI_MP_pps_en, 0},
	[eDI_MP_pps_position]  = {"pps_position:uint:def:1",
			eDI_MP_pps_position, 1},
	[eDI_MP_pre_enable_mask]  = {"pre_enable_mask:bit0:ma;bit1:mc:def:3",
			eDI_MP_pre_enable_mask, 3},
	[eDI_MP_post_refresh]  = {"post_refresh:bool",
			eDI_MP_post_refresh, 0},
	[eDI_MP_nrds_en]  = {"nrds_en:bool",
			eDI_MP_nrds_en, 0},
	[eDI_MP_bypass_3d]  = {"bypass_3d:int:def:1",
			eDI_MP_bypass_3d, 1},
	[eDI_MP_bypass_trick_mode]  = {"bypass_trick_mode:int:def:1",
			eDI_MP_bypass_trick_mode, 1},
	[eDI_MP_invert_top_bot]  = {"invert_top_bot:int",
			eDI_MP_invert_top_bot, 0},
	[eDI_MP_skip_top_bot]  = {"skip_top_bot:int:",
			/*1or2: may affect atv when bypass di*/
			eDI_MP_skip_top_bot, 0},
	[eDI_MP_force_width]  = {"force_width:int",
			eDI_MP_force_width, 0},
	[eDI_MP_force_height]  = {"force_height:int",
			eDI_MP_force_height, 0},
	[eDI_MP_prog_proc_config]  = {"prog_proc_config:int:def:0x",
/* prog_proc_config,
 * bit[2:1]: when two field buffers are used,
 * 0 use vpp for blending ,
 * 1 use post_di module for blending
 * 2 debug mode, bob with top field
 * 3 debug mode, bot with bot field
 * bit[0]:
 * 0 "prog vdin" use two field buffers,
 * 1 "prog vdin" use single frame buffer
 * bit[4]:
 * 0 "prog frame from decoder/vdin" use two field buffers,
 * 1 use single frame buffer
 * bit[5]:
 * when two field buffers are used for decoder (bit[4] is 0):
 * 1,handle prog frame as two interlace frames
 * bit[6]:(bit[4] is 0,bit[5] is 0,use_2_interlace_buff is 0): 0,
 * process progress frame as field,blend by post;
 * 1, process progress frame as field,process by normal di
 */
			eDI_MP_prog_proc_config, ((1 << 5) | (1 << 1) | 1)},
	[eDI_MP_start_frame_drop_count]  = {"start_frame_drop_count:int:2",
			eDI_MP_start_frame_drop_count, 2},
	[eDI_MP_same_field_top_count]  = {"same_field_top_count:long?",
			eDI_MP_same_field_top_count, 0},
	[eDI_MP_same_field_bot_count]  = {"same_field_bot_count:long?",
			eDI_MP_same_field_bot_count, 0},
	[eDI_MP_vpp_3d_mode]  = {"vpp_3d_mode:int",
			eDI_MP_vpp_3d_mode, 0},
	[eDI_MP_force_recovery_count]  = {"force_recovery_count:uint",
			eDI_MP_force_recovery_count, 0},
	[eDI_MP_pre_process_time]  = {"pre_process_time:int",
			eDI_MP_pre_process_time, 0},
	[eDI_MP_bypass_post]  = {"bypass_post:int",
			eDI_MP_bypass_post, 0},
	[eDI_MP_post_wr_en]  = {"post_wr_en:bool:1",
			eDI_MP_post_wr_en, 1},
	[eDI_MP_post_wr_support]  = {"post_wr_support:uint",
			eDI_MP_post_wr_support, 0},
	[eDI_MP_bypass_post_state]  = {"bypass_post_state:int",
/* 0, use di_wr_buf still;
 * 1, call dim_pre_de_done_buf_clear to clear di_wr_buf;
 * 2, do nothing
 */
			eDI_MP_bypass_post_state, 0},
	[eDI_MP_use_2_interlace_buff]  = {"use_2_interlace_buff:int",
			eDI_MP_use_2_interlace_buff, 0},
	[eDI_MP_debug_blend_mode]  = {"debug_blend_mode:int:-1",
			eDI_MP_debug_blend_mode, -1},
	[eDI_MP_nr10bit_support]  = {"nr10bit_support:uint",
		/* 0: not support nr10bit, 1: support nr10bit */
			eDI_MP_nr10bit_support, 0},
	[eDI_MP_di_stop_reg_flag]  = {"di_stop_reg_flag:uint",
			eDI_MP_di_stop_reg_flag, 0},
	[eDI_MP_mcpre_en]  = {"mcpre_en:bool:true",
			eDI_MP_mcpre_en, 1},
	[eDI_MP_check_start_drop_prog]  = {"check_start_drop_prog:bool",
			eDI_MP_check_start_drop_prog, 0},
	[eDI_MP_overturn]  = {"overturn:bool:?",
			eDI_MP_overturn, 0},
	[eDI_MP_full_422_pack]  = {"full_422_pack:bool",
			eDI_MP_full_422_pack, 0},
	[eDI_MP_cma_print]  = {"cma_print:bool:1",
			eDI_MP_cma_print, 0},
	[eDI_MP_pulldown_enable]  = {"pulldown_enable:bool:1",
			eDI_MP_pulldown_enable, 1},
	[eDI_MP_di_force_bit_mode]  = {"di_force_bit_mode:uint:10",
			eDI_MP_di_force_bit_mode, 10},
	[eDI_MP_calc_mcinfo_en]  = {"calc_mcinfo_en:bool:1",
			eDI_MP_calc_mcinfo_en, 1},
	[eDI_MP_colcfd_thr]  = {"colcfd_thr:uint:128",
			eDI_MP_colcfd_thr, 128},
	[eDI_MP_post_blend]  = {"post_blend:uint",
			eDI_MP_post_blend, 0},
	[eDI_MP_post_ei]  = {"post_ei:uint",
			eDI_MP_post_ei, 0},
	[eDI_MP_post_cnt]  = {"post_cnt:uint",
			eDI_MP_post_cnt, 0},
	[eDI_MP_di_log_flag]  = {"di_log_flag:uint",
			eDI_MP_di_log_flag, 0},
	[eDI_MP_di_debug_flag]  = {"di_debug_flag:uint",
			eDI_MP_di_debug_flag, 0},
	[eDI_MP_buf_state_log_threshold]  = {"buf_state_log_threshold:unit:16",
			eDI_MP_buf_state_log_threshold, 16},
	[eDI_MP_di_vscale_skip_enable]  = {"di_vscale_skip_enable:int",
/*
 * bit[2]: enable bypass all when skip
 * bit[1:0]: enable bypass post when skip
 */
			eDI_MP_di_vscale_skip_enable, 0},
	[eDI_MP_di_vscale_skip_count]  = {"di_vscale_skip_count:int",
			eDI_MP_di_vscale_skip_count, 0},
	[eDI_MP_di_vscale_skip_count_real]  = {"di_vscale_skip_count_real:int",
			eDI_MP_di_vscale_skip_count_real, 0},
	[eDI_MP_det3d_en]  = {"det3d_en:bool",
			eDI_MP_det3d_en, 0},
	[eDI_MP_post_hold_line]  = {"post_hold_line:int:8",
			eDI_MP_post_hold_line, 8},
	[eDI_MP_post_urgent]  = {"post_urgent:int:1",
			eDI_MP_post_urgent, 1},
	[eDI_MP_di_printk_flag]  = {"di_printk_flag:uint",
			eDI_MP_di_printk_flag, 0},
	[eDI_MP_force_recovery]  = {"force_recovery:uint:1",
			eDI_MP_force_recovery, 1},
#if 0
	[eDI_MP_debug_blend_mode]  = {"debug_blend_mode:int:-1",
			eDI_MP_debug_blend_mode, -1},
#endif
	[eDI_MP_di_dbg_mask]  = {"di_dbg_mask:uint:0x02",
			eDI_MP_di_dbg_mask, 2},
	[eDI_MP_nr_done_check_cnt]  = {"nr_done_check_cnt:uint:5",
			eDI_MP_nr_done_check_cnt, 5},
	[eDI_MP_pre_hsc_down_en]  = {"pre_hsc_down_en:bool:0",
			eDI_MP_pre_hsc_down_en, 0},
	[eDI_MP_pre_hsc_down_width]  = {"pre_hsc_down_width:int:480",
				eDI_MP_pre_hsc_down_width, 480},
	[eDI_MP_show_nrwr]  = {"show_nrwr:bool:0",
				eDI_MP_show_nrwr, 0},

	/******deinterlace_hw.c**********/
	[eDI_MP_pq_load_dbg]  = {"pq_load_dbg:uint",
			eDI_MP_pq_load_dbg, 0},
	[eDI_MP_lmv_lock_win_en]  = {"lmv_lock_win_en:bool",
			eDI_MP_lmv_lock_win_en, 0},
	[eDI_MP_lmv_dist]  = {"lmv_dist:short:5",
			eDI_MP_lmv_dist, 5},
	[eDI_MP_pr_mcinfo_cnt]  = {"pr_mcinfo_cnt:ushort",
			eDI_MP_pr_mcinfo_cnt, 0},
	[eDI_MP_offset_lmv]  = {"offset_lmv:short:100",
			eDI_MP_offset_lmv, 100},
	[eDI_MP_post_ctrl]  = {"post_ctrl:uint",
			eDI_MP_post_ctrl, 0},
	[eDI_MP_if2_disable]  = {"if2_disable:bool",
			eDI_MP_if2_disable, 0},
	[eDI_MP_pre_flag]  = {"pre_flag:ushort:2",
			eDI_MP_pre_flag, 2},
	[eDI_MP_pre_mif_gate]  = {"pre_mif_gate:bool",
			eDI_MP_pre_mif_gate, 0},
	[eDI_MP_pre_urgent]  = {"pre_urgent:ushort",
			eDI_MP_pre_urgent, 0},
	[eDI_MP_pre_hold_line]  = {"pre_hold_line:ushort:10",
			eDI_MP_pre_hold_line, 10},
	[eDI_MP_pre_ctrl]  = {"pre_ctrl:uint",
			eDI_MP_pre_ctrl, 0},
	[eDI_MP_line_num_post_frst]  = {"line_num_post_frst:ushort:5",
			eDI_MP_line_num_post_frst, 5},
	[eDI_MP_line_num_pre_frst]  = {"line_num_pre_frst:ushort:5",
			eDI_MP_line_num_pre_frst, 5},
	[eDI_MP_pd22_flg_calc_en]  = {"pd22_flg_calc_en:bool:true",
			eDI_MP_pd22_flg_calc_en, 1},
	[eDI_MP_mcen_mode]  = {"mcen_mode:ushort:1",
			eDI_MP_mcen_mode, 1},
	[eDI_MP_mcuv_en]  = {"mcuv_en:ushort:1",
			eDI_MP_mcuv_en, 1},
	[eDI_MP_mcdebug_mode]  = {"mcdebug_mode:ushort",
			eDI_MP_mcdebug_mode, 0},
	[eDI_MP_pldn_ctrl_rflsh]  = {"pldn_ctrl_rflsh:uint:1",
			eDI_MP_pldn_ctrl_rflsh, 1},

	[eDI_MP_SUB_DI_E]  = {"di end-------",
				eDI_MP_SUB_DI_E, 0},
	/**************************************/
	[eDI_MP_SUB_NR_B]  = {"nr begin",
			eDI_MP_SUB_NR_B, 0},
	[eDI_MP_dnr_en]  = {"dnr_en:bool:true",
			eDI_MP_dnr_en, 1},
	[eDI_MP_nr2_en]  = {"nr2_en:uint:1",
			eDI_MP_nr2_en, 1},
	[eDI_MP_cue_en]  = {"cue_en:bool:true",
			eDI_MP_cue_en, 1},
	[eDI_MP_invert_cue_phase]  = {"invert_cue_phase:bool",
			eDI_MP_invert_cue_phase, 0},
	[eDI_MP_cue_pr_cnt]  = {"cue_pr_cnt:uint",
			eDI_MP_cue_pr_cnt, 0},
	[eDI_MP_cue_glb_mot_check_en]  = {"cue_glb_mot_check_en:bool:true",
			eDI_MP_cue_glb_mot_check_en, 1},
	[eDI_MP_glb_fieldck_en]  = {"glb_fieldck_en:bool:true",
			eDI_MP_glb_fieldck_en, 1},
	[eDI_MP_dnr_pr]  = {"dnr_pr:bool",
			eDI_MP_dnr_pr, 0},
	[eDI_MP_dnr_dm_en]  = {"dnr_dm_en:bool",
			eDI_MP_dnr_dm_en, 0},
	[eDI_MP_SUB_NR_E]  = {"nr end-------",
			eDI_MP_SUB_NR_E, 0},
	/**************************************/
	[eDI_MP_SUB_PD_B]  = {"pd begin",
			eDI_MP_SUB_PD_B, 0},
	[eDI_MP_flm22_ratio]  = {"flm22_ratio:uint:200",
			eDI_MP_flm22_ratio, 200},
	[eDI_MP_pldn_cmb0]  = {"pldn_cmb0:uint:1",
			eDI_MP_pldn_cmb0, 1},
	[eDI_MP_pldn_cmb1]  = {"pldn_cmb1:uint",
			eDI_MP_pldn_cmb1, 0},
	[eDI_MP_flm22_sure_num]  = {"flm22_sure_num:uint:100",
			eDI_MP_flm22_sure_num, 100},
	[eDI_MP_flm22_glbpxlnum_rat]  = {"flm22_glbpxlnum_rat:uint:4",
			eDI_MP_flm22_glbpxlnum_rat, 4},
	[eDI_MP_flag_di_weave]  = {"flag_di_weave:int:1",
			eDI_MP_flag_di_weave, 1},
	[eDI_MP_flm22_glbpxl_maxrow]  = {"flm22_glbpxl_maxrow:uint:16",
			eDI_MP_flm22_glbpxl_maxrow, 16},
	[eDI_MP_flm22_glbpxl_minrow]  = {"flm22_glbpxl_minrow:uint:3",
			eDI_MP_flm22_glbpxl_minrow, 3},
	[eDI_MP_cmb_3point_rnum]  = {"cmb_3point_rnum:uint",
			eDI_MP_cmb_3point_rnum, 0},
	[eDI_MP_cmb_3point_rrat]  = {"cmb_3point_rrat:unit:32",
			eDI_MP_cmb_3point_rrat, 32},
	/********************************/
	[eDI_MP_pr_pd]  = {"pr_pd:uint",
			eDI_MP_pr_pd, 0},
	[eDI_MP_prt_flg]  = {"prt_flg:bool",
			eDI_MP_prt_flg, 0},
	[eDI_MP_flmxx_maybe_num]  = {"flmxx_maybe_num:uint:15",
	/* if flmxx level > flmxx_maybe_num */
	/* mabye flmxx: when 2-2 3-2 not detected */
			eDI_MP_flmxx_maybe_num, 15},
	[eDI_MP_flm32_mim_frms]  = {"flm32_mim_frms:int:6",
			eDI_MP_flm32_mim_frms, 6},
	[eDI_MP_flm22_dif01a_flag]  = {"flm22_dif01a_flag:int:1",
			eDI_MP_flm22_dif01a_flag, 1},
	[eDI_MP_flm22_mim_frms]  = {"flm22_mim_frms:int:60",
			eDI_MP_flm22_mim_frms, 60},
	[eDI_MP_flm22_mim_smfrms]  = {"flm22_mim_smfrms:int:40",
			eDI_MP_flm22_mim_smfrms, 40},
	[eDI_MP_flm32_f2fdif_min0]  = {"flm32_f2fdif_min0:int:11",
			eDI_MP_flm32_f2fdif_min0, 11},
	[eDI_MP_flm32_f2fdif_min1]  = {"flm32_f2fdif_min1:int:11",
			eDI_MP_flm32_f2fdif_min1, 11},
	[eDI_MP_flm32_chk1_rtn]  = {"flm32_chk1_rtn:int:25",
			eDI_MP_flm32_chk1_rtn, 25},
	[eDI_MP_flm32_ck13_rtn]  = {"flm32_ck13_rtn:int:8",
			eDI_MP_flm32_ck13_rtn, 8},
	[eDI_MP_flm32_chk2_rtn]  = {"flm32_chk2_rtn:int:16",
			eDI_MP_flm32_chk2_rtn, 16},
	[eDI_MP_flm32_chk3_rtn]  = {"flm32_chk3_rtn:int:16",
			eDI_MP_flm32_chk3_rtn, 16},
	[eDI_MP_flm32_dif02_ratio]  = {"flm32_dif02_ratio:int:8",
			eDI_MP_flm32_dif02_ratio, 8},
	[eDI_MP_flm22_chk20_sml]  = {"flm22_chk20_sml:int:6",
			eDI_MP_flm22_chk20_sml, 6},
	[eDI_MP_flm22_chk21_sml]  = {"flm22_chk21_sml:int:6",
			eDI_MP_flm22_chk21_sml, 6},
	[eDI_MP_flm22_chk21_sm2]  = {"flm22_chk21_sm2:int:10",
			eDI_MP_flm22_chk21_sm2, 10},
	[eDI_MP_flm22_lavg_sft]  = {"flm22_lavg_sft:int:4",
			eDI_MP_flm22_lavg_sft, 4},
	[eDI_MP_flm22_lavg_lg]  = {"flm22_lavg_lg:int:24",
			eDI_MP_flm22_lavg_lg, 24},
	[eDI_MP_flm22_stl_sft]  = {"flm22_stl_sft:int:7",
			eDI_MP_flm22_stl_sft, 7},
	[eDI_MP_flm22_chk5_avg]  = {"flm22_chk5_avg:int:50",
			eDI_MP_flm22_chk5_avg, 50},
	[eDI_MP_flm22_chk6_max]  = {"flm22_chk6_max:int:20",
			eDI_MP_flm22_chk6_max, 20},
	[eDI_MP_flm22_anti_chk1]  = {"flm22_anti_chk1:int:61",
			eDI_MP_flm22_anti_chk1, 61},
	[eDI_MP_flm22_anti_chk3]  = {"flm22_anti_chk3:int:140",
			eDI_MP_flm22_anti_chk3, 140},
	[eDI_MP_flm22_anti_chk4]  = {"flm22_anti_chk4:int:128",
			eDI_MP_flm22_anti_chk4, 128},
	[eDI_MP_flm22_anti_ck140]  = {"flm22_anti_ck140:int:32",
			eDI_MP_flm22_anti_ck140, 32},
	[eDI_MP_flm22_anti_ck141]  = {"flm22_anti_ck141:int:80",
			eDI_MP_flm22_anti_ck141, 80},
	[eDI_MP_flm22_frmdif_max]  = {"flm22_frmdif_max:int:50",
			eDI_MP_flm22_frmdif_max, 50},
	[eDI_MP_flm22_flddif_max]  = {"flm22_flddif_max:int:100",
			eDI_MP_flm22_flddif_max, 100},
	[eDI_MP_flm22_minus_cntmax]  = {"flm22_minus_cntmax:int:2",
			eDI_MP_flm22_minus_cntmax, 2},
	[eDI_MP_flagdif01chk]  = {"flagdif01chk:int:1",
			eDI_MP_flagdif01chk, 1},
	[eDI_MP_dif01_ratio]  = {"dif01_ratio:int:10",
			eDI_MP_dif01_ratio, 10},
	/*************vof_soft_top**************/
	[eDI_MP_cmb32_blw_wnd]  = {"cmb32_blw_wnd:int:180",
			eDI_MP_cmb32_blw_wnd, 180},
	[eDI_MP_cmb32_wnd_ext]  = {"cmb32_wnd_ext:int:11",
			eDI_MP_cmb32_wnd_ext, 11},
	[eDI_MP_cmb32_wnd_tol]  = {"cmb32_wnd_tol:int:4",
			eDI_MP_cmb32_wnd_tol, 4},
	[eDI_MP_cmb32_frm_nocmb]  = {"cmb32_frm_nocmb:int:40",
			eDI_MP_cmb32_frm_nocmb, 40},
	[eDI_MP_cmb32_min02_sft]  = {"cmb32_min02_sft:int:7",
			eDI_MP_cmb32_min02_sft, 7},
	[eDI_MP_cmb32_cmb_tol]  = {"cmb32_cmb_tol:int:10",
			eDI_MP_cmb32_cmb_tol, 10},
	[eDI_MP_cmb32_avg_dff]  = {"cmb32_avg_dff:int:48",
			eDI_MP_cmb32_avg_dff, 48},
	[eDI_MP_cmb32_smfrm_num]  = {"cmb32_smfrm_num:int:4",
			eDI_MP_cmb32_smfrm_num, 4},
	[eDI_MP_cmb32_nocmb_num]  = {"cmb32_nocmb_num:int:20",
			eDI_MP_cmb32_nocmb_num, 20},
	[eDI_MP_cmb22_gcmb_rnum]  = {"cmb22_gcmb_rnum:int:8",
			eDI_MP_cmb22_gcmb_rnum, 8},
	[eDI_MP_flmxx_cal_lcmb]  = {"flmxx_cal_lcmb:int:1",
			eDI_MP_flmxx_cal_lcmb, 1},
	[eDI_MP_flm2224_stl_sft]  = {"flm2224_stl_sft:int:7",
			eDI_MP_flm2224_stl_sft, 7},
	[eDI_MP_SUB_PD_E]  = {"pd end------",
			eDI_MP_SUB_PD_E, 0},
	/**************************************/
	[eDI_MP_SUB_MTN_B]  = {"mtn begin",
			eDI_MP_SUB_MTN_B, 0},
	[eDI_MP_force_lev]  = {"force_lev:int:0xff",
			eDI_MP_force_lev, 0xff},
	[eDI_MP_dejaggy_flag]  = {"dejaggy_flag:int:-1",
			eDI_MP_dejaggy_flag, -1},
	[eDI_MP_dejaggy_enable]  = {"dejaggy_enable:int:1",
			eDI_MP_dejaggy_enable, 1},
	[eDI_MP_cmb_adpset_cnt]  = {"cmb_adpset_cnt:int",
			eDI_MP_cmb_adpset_cnt, 0},
	[eDI_MP_cmb_num_rat_ctl4]  = {"cmb_num_rat_ctl4:int:64:0~255",
			eDI_MP_cmb_num_rat_ctl4, 64},
	[eDI_MP_cmb_rat_ctl4_minthd]  = {"cmb_rat_ctl4_minthd:int:64",
			eDI_MP_cmb_rat_ctl4_minthd, 64},
	[eDI_MP_small_local_mtn]  = {"small_local_mtn:uint:70",
			eDI_MP_small_local_mtn, 70},
	[eDI_MP_di_debug_readreg]  = {"di_debug_readreg:int",
			eDI_MP_di_debug_readreg, 0},
	[eDI_MP_SUB_MTN_E]  = {"mtn end----",
			eDI_MP_SUB_MTN_E, 0},
	/**************************************/
	[eDI_MP_SUB_3D_B]  = {"3d begin",
			eDI_MP_SUB_3D_B, 0},
	[eDI_MP_chessbd_vrate]  = {"chessbd_vrate:int:29",
				eDI_MP_chessbd_vrate, 29},
	[eDI_MP_det3d_debug]  = {"det3d_debug:bool:0",
				eDI_MP_det3d_debug, 0},
	[eDI_MP_SUB_3D_E]  = {"3d begin",
				eDI_MP_SUB_3D_E, 0},

	/**************************************/
	[eDI_MP_UI_T_END]  = {"module para top end ", eDI_MP_UI_T_END, 0},
};

bool di_mp_uit_check(unsigned int idx)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(di_mp_ui_top);
	if (idx >= tsize) {
		PR_ERR("%s:err:overflow:%d->%d\n",
		       __func__, idx, tsize);
		return false;
	}
	if (idx != di_mp_ui_top[idx].id) {
		PR_ERR("%s:err:not map:%d->%d\n",
		       __func__, idx, di_mp_ui_top[idx].id);
		return false;
	}
	dbg_init("\t%-15s=%d\n", di_mp_ui_top[idx].name,
		 di_mp_ui_top[idx].default_val);
	return true;
}

char *di_mp_uit_get_name(enum eDI_MP_UI_T idx)
{
	return di_mp_ui_top[idx].name;
}

void di_mp_uit_init_val(void)
{
	int i;

	for (i = eDI_MP_UI_T_BEGIN; i < eDI_MP_UI_T_END; i++) {
		if (!di_mp_uit_check(i))
			continue;
		di_mp_uit_set(i, di_mp_ui_top[i].default_val);
	}
}

int di_mp_uit_get(enum eDI_MP_UI_T idx)
{
	return get_datal()->mp_uit[idx];
}

void di_mp_uit_set(enum eDI_MP_UI_T idx, int val)
{
	get_datal()->mp_uit[idx] = val;
}

/**************************************
 *
 * module para x
 *	unsigned int
 **************************************/

const struct di_mp_uix_s di_mpx[] = {
	/*same order with enum eDI_MP_UI*/

	/* module para for channel x*/
	[eDI_MP_UIX_BEGIN]  = {"module para x begin ", eDI_MP_UIX_BEGIN, 0},
	/*debug:	run_flag*/
	[eDI_MP_UIX_RUN_FLG]  = {"run_flag(0:run;1:pause;2:step)",
				eDI_MP_UIX_RUN_FLG, DI_RUN_FLAG_RUN},
	[eDI_MP_UIX_END]  = {"module para x end ", eDI_MP_UIX_END, 0},

};

bool di_mp_uix_check(unsigned int idx)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(di_mpx);
	if (idx >= tsize) {
		PR_ERR("%s:err:overflow:%d->%d\n",
		       __func__, idx, tsize);
		return false;
	}
	if (idx != di_mpx[idx].id) {
		PR_ERR("%s:err:not map:%d->%d\n",
		       __func__, idx, di_mpx[idx].id);
		return false;
	}
	dbg_init("\t%-15s=%d\n", di_mpx[idx].name, di_mpx[idx].default_val);

	return true;
}

char *di_mp_uix_get_name(enum eDI_MP_UIX_T idx)
{
	return di_mpx[idx].name;
}

void di_mp_uix_init_val(void)
{
	int i, ch;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		dbg_init("%s:ch[%d]\n", __func__, ch);
		for (i = eDI_MP_UIX_BEGIN; i < eDI_MP_UIX_END; i++) {
			if (ch == 0) {
				if (!di_mp_uix_check(i))
					continue;
			}
			di_mp_uix_set(ch, i, di_mpx[i].default_val);
		}
	}
}

unsigned int di_mp_uix_get(unsigned int ch, enum eDI_MP_UIX_T idx)
{
	return get_datal()->ch_data[ch].mp_uix[idx];
}

void di_mp_uix_set(unsigned int ch, enum eDI_MP_UIX_T idx, unsigned int val)
{
	get_datal()->ch_data[ch].mp_uix[idx] = val;
}

bool di_is_pause(unsigned int ch)
{
	unsigned int run_flag;

	run_flag = di_mp_uix_get(ch, eDI_MP_UIX_RUN_FLG);

	if (run_flag == DI_RUN_FLAG_PAUSE	||
	    run_flag == DI_RUN_FLAG_STEP_DONE)
		return true;

	return false;
}

void di_pause_step_done(unsigned int ch)
{
	unsigned int run_flag;

	run_flag = di_mp_uix_get(ch, eDI_MP_UIX_RUN_FLG);
	if (run_flag == DI_RUN_FLAG_STEP) {
		di_mp_uix_set(ch, eDI_MP_UIX_RUN_FLG,
			      DI_RUN_FLAG_STEP_DONE);
	}
}

void di_pause(unsigned int ch, bool on)
{
	pr_info("%s:%d\n", __func__, on);
	if (on)
		di_mp_uix_set(ch, eDI_MP_UIX_RUN_FLG,
			      DI_RUN_FLAG_PAUSE);
	else
		di_mp_uix_set(ch, eDI_MP_UIX_RUN_FLG,
			      DI_RUN_FLAG_RUN);
}

/**************************************
 *
 * summmary variable
 *
 **************************************/
const struct di_sum_s di_sum_tab[] = {
	/*video_peek_cnt*/
	[eDI_SUM_O_PEEK_CNT] = {"o_peek_cnt", eDI_SUM_O_PEEK_CNT, 0},
	/*di_reg_unreg_cnt*/
	[eDI_SUM_REG_UNREG_CNT] = {
			"di_reg_unreg_cnt", eDI_SUM_REG_UNREG_CNT, 100},

	[eDI_SUM_NUB] = {"end", eDI_SUM_NUB, 0},
};

unsigned int di_sum_get_tab_size(void)
{
	return ARRAY_SIZE(di_sum_tab);
}

bool di_sum_check(unsigned int ch, enum eDI_SUM id)
{
	unsigned int tsize;

	tsize = ARRAY_SIZE(di_sum_tab);

	if (id >= tsize) {
		PR_ERR("%s:err:overflow:tsize[%d],id[%d]\n",
		       __func__, tsize, id);
		return false;
	}
	if (di_sum_tab[id].index != id) {
		PR_ERR("%s:err:table:id[%d],tab_id[%d]\n",
		       __func__, id, di_sum_tab[id].index);
		return false;
	}
	return true;
}

void di_sum_reg_init(unsigned int ch)
{
	unsigned int tsize;
	int i;

	tsize = ARRAY_SIZE(di_sum_tab);

	dbg_init("%s:ch[%d]\n", __func__, ch);
	for (i = 0; i < tsize; i++) {
		if (!di_sum_check(ch, i))
			continue;
		dbg_init("\t:%d:name:%s,%d\n", i, di_sum_tab[i].name,
			 di_sum_tab[i].default_val);
		di_sum_set_l(ch, i, di_sum_tab[i].default_val);
	}
}

void di_sum_get_info(unsigned int ch,  enum eDI_SUM id, char **name,
		     unsigned int *pval)
{
	*name = di_sum_tab[id].name;
	*pval = di_sum_get(ch, id);
}

void di_sum_set(unsigned int ch, enum eDI_SUM id, unsigned int val)
{
	if (!di_sum_check(ch, id))
		return;

	di_sum_set_l(ch, id, val);
}

unsigned int di_sum_inc(unsigned int ch, enum eDI_SUM id)
{
	if (!di_sum_check(ch, id))
		return 0;
	return di_sum_inc_l(ch, id);
}

unsigned int di_sum_get(unsigned int ch, enum eDI_SUM id)
{
	if (!di_sum_check(ch, id))
		return 0;
	return di_sum_get_l(ch, id);
}

/****************************/
/*call by event*/
/****************************/
void dip_even_reg_init_val(unsigned int ch)
{
}

void dip_even_unreg_val(unsigned int ch)
{
}

/****************************/
static void dip_cma_init_val(void)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		/* CMA state */
		atomic_set(&pbm->cma_mem_state[ch], eDI_CMA_ST_IDL);

		/* CMA reg/unreg cmd */
		pbm->cma_reg_cmd[ch] = 0;
	}
}

void dip_cma_close(void)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();

	if (dip_cma_st_is_idl_all())
		return;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		if (!dip_cma_st_is_idle(ch)) {
			dim_cma_top_release(ch);
			pr_info("%s:force release ch[%d]", __func__, ch);
			atomic_set(&pbm->cma_mem_state[ch], eDI_CMA_ST_IDL);

			pbm->cma_reg_cmd[ch] = 0;
		}
	}
}

static void dip_wq_cma_handler(struct work_struct *work)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();
	enum eDI_CMA_ST cma_st;
	bool do_flg;

	pr_info("%s:start\n", __func__);
	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		do_flg = false;
		cma_st = dip_cma_get_st(ch);
		switch (cma_st) {
		case eDI_CMA_ST_IDL:
			if (pbm->cma_reg_cmd[ch]) {
				do_flg = true;
				/*set:alloc:*/
				atomic_set(&pbm->cma_mem_state[ch],
					   eDI_CMA_ST_ALLOC);
				if (dim_cma_top_alloc(ch)) {
					atomic_set(&pbm->cma_mem_state[ch],
						   eDI_CMA_ST_READY);
				}
			}
			break;
		case eDI_CMA_ST_READY:
			if (!pbm->cma_reg_cmd[ch]) {
				do_flg = true;
				atomic_set(&pbm->cma_mem_state[ch],
					   eDI_CMA_ST_RELEASE);
				dim_cma_top_release(ch);
				atomic_set(&pbm->cma_mem_state[ch],
					   eDI_CMA_ST_IDL);
			}
			break;
		case eDI_CMA_ST_ALLOC:	/*do*/
		case eDI_CMA_ST_RELEASE:/*do*/
		default:
			break;
		}
		if (!do_flg)
			pr_info("\tch[%d],do nothing[%d]\n", ch, cma_st);
		else
			task_send_ready();
	}
	pr_info("%s:end\n", __func__);
}

static void dip_wq_prob(void)
{
	struct di_mng_s *pbm = get_bufmng();

	pbm->wq_cma = create_singlethread_workqueue("deinterlace");
	INIT_WORK(&pbm->wq_work, dip_wq_cma_handler);
}

static void dip_wq_ext(void)
{
	struct di_mng_s *pbm = get_bufmng();

	cancel_work_sync(&pbm->wq_work);
	destroy_workqueue(pbm->wq_cma);
	pr_info("%s:finish\n", __func__);
}

void dip_wq_cma_run(unsigned char ch, bool reg_cmd)
{
	struct di_mng_s *pbm = get_bufmng();

	if (reg_cmd)
		pbm->cma_reg_cmd[ch] = 1;
	else
		pbm->cma_reg_cmd[ch] = 0;

	queue_work(pbm->wq_cma, &pbm->wq_work);
}

bool dip_cma_st_is_ready(unsigned int ch)
{
	struct di_mng_s *pbm = get_bufmng();
	bool ret = false;

	if (atomic_read(&pbm->cma_mem_state[ch]) == eDI_CMA_ST_READY)
		ret = true;

	return ret;
}

bool dip_cma_st_is_idle(unsigned int ch)
{
	struct di_mng_s *pbm = get_bufmng();
	bool ret = false;

	if (atomic_read(&pbm->cma_mem_state[ch]) == eDI_CMA_ST_IDL)
		ret = true;

	return ret;
}

bool dip_cma_st_is_idl_all(void)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();
	bool ret = true;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		if (atomic_read(&pbm->cma_mem_state[ch]) != eDI_CMA_ST_IDL) {
			ret = true;
			break;
		}
	}
	return ret;
}

enum eDI_CMA_ST dip_cma_get_st(unsigned int ch)
{
	struct di_mng_s *pbm = get_bufmng();

	return atomic_read(&pbm->cma_mem_state[ch]);
}

const char * const di_cma_state_name[] = {
	"IDLE",
	"do_alloc",
	"READY",
	"do_release",
};

const char *di_cma_dbg_get_st_name(unsigned int ch)
{
	enum eDI_CMA_ST st = dip_cma_get_st(ch);
	const char *p = "";

	if (st <= eDI_CMA_ST_RELEASE)
		p = di_cma_state_name[st];
	return p;
}

void dip_cma_st_set_ready_all(void)
{
	unsigned int ch;
	struct di_mng_s *pbm = get_bufmng();

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++)
		atomic_set(&pbm->cma_mem_state[ch], eDI_CMA_ST_READY);
}

/****************************/
/*channel STATE*/
/****************************/
void dip_chst_set(unsigned int ch, enum eDI_TOP_STATE chSt)
{
	struct di_mng_s *pbm = get_bufmng();

	atomic_set(&pbm->ch_state[ch], chSt);
}

enum eDI_TOP_STATE dip_chst_get(unsigned int ch)
{
	struct di_mng_s *pbm = get_bufmng();

	return atomic_read(&pbm->ch_state[ch]);
}

void dip_chst_init(void)
{
	unsigned int ch;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++)
		dip_chst_set(ch, eDI_TOP_STATE_IDLE);
}

bool dip_event_reg_chst(unsigned int ch)
{
	enum eDI_TOP_STATE chst;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	bool err_flg = false;
	bool ret = true;

	dbg_dbg("%s:ch[%d]\n", __func__, ch);

	chst = dip_chst_get(ch);
#if 0	/*move*/
	if (chst > eDI_TOP_STATE_NOPROB)
		set_flag_trig_unreg(ch, false);
#endif
	switch (chst) {
	case eDI_TOP_STATE_IDLE:

		queue_init2(ch);
		di_que_init(ch);
		di_vframe_reg(ch);

		dip_chst_set(ch, eDI_TOP_STATE_REG_STEP1);
		task_send_cmd(LCMD1(eCMD_REG, ch));
		dbg_dbg("reg ok\n");
		break;
	case eDI_TOP_STATE_REG_STEP1:
	case eDI_TOP_STATE_REG_STEP1_P1:
	case eDI_TOP_STATE_REG_STEP2:
	case eDI_TOP_STATE_READY:
	case eDI_TOP_STATE_BYPASS:
		PR_WARN("have reg\n");
		ret = false;
		break;
	case eDI_TOP_STATE_UNREG_STEP1:
	case eDI_TOP_STATE_UNREG_STEP2:
		/*wait*/
		ppre->reg_req_flag_cnt = 0;
		while (dip_chst_get(ch) != eDI_TOP_STATE_IDLE) {
			usleep_range(10000, 10001);
			if (ppre->reg_req_flag_cnt++ >
				dim_get_reg_unreg_cnt()) {
				dim_reg_timeout_inc();
				PR_ERR("%s,ch[%d] reg timeout!!!\n",
				       __func__, ch);
				err_flg = true;
				ret = false;
				break;
			}
		}
		if (!err_flg) {
		/*same as IDLE*/
			queue_init2(ch);
			di_que_init(ch);
			di_vframe_reg(ch);

			dip_chst_set(ch, eDI_TOP_STATE_REG_STEP1);
			task_send_cmd(LCMD1(eCMD_REG, ch));
			dbg_dbg("reg retry ok\n");
		}
		break;
	case eDI_TOP_STATE_NOPROB:
	default:
		ret = false;
		PR_ERR("err: not prob[%d]\n", chst);

		break;
	}

	return ret;
}

bool dip_event_unreg_chst(unsigned int ch)
{
	enum eDI_TOP_STATE chst, chst2;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	bool ret = false;
	bool err_flg = false;
	unsigned int cnt;

	chst = dip_chst_get(ch);
	dbg_reg("%s:ch[%d]:%s\n", __func__, ch, dip_chst_get_name(chst));
	#if 0
	if (chst > eDI_TOP_STATE_IDLE)
		set_reg_flag(ch, false);/*set_flag_trig_unreg(ch, true);*/
	#endif
	if (chst > eDI_TOP_STATE_NOPROB)
		set_flag_trig_unreg(ch, true);

	switch (chst) {
	case eDI_TOP_STATE_READY:

		di_vframe_unreg(ch);
		/*trig unreg*/
		dip_chst_set(ch, eDI_TOP_STATE_UNREG_STEP1);
		task_send_cmd(LCMD1(eCMD_UNREG, ch));
		/*debug only di_dbg = di_dbg|DBG_M_TSK;*/

		/*wait*/
		ppre->unreg_req_flag_cnt = 0;
		chst2 = dip_chst_get(ch);

		while (chst2 != eDI_TOP_STATE_IDLE) {
			task_send_ready();
			usleep_range(10000, 10001);
			/*msleep(5);*/
			if (ppre->unreg_req_flag_cnt++
				> dim_get_reg_unreg_cnt()) {
				dim_reg_timeout_inc();
				PR_ERR("%s:ch[%d] unreg timeout!!!\n",
				       __func__, ch);
				/*dim_unreg_process();*/
				err_flg = true;
				break;
			}
			#if 0	/*debug only*/
			dbg_reg("\tch[%d]:s[%s],ecnt[%d]\n",
				ch,
				dip_chst_get_name(chst2),
				ppre->unreg_req_flag_cnt);
			#endif
			chst2 = dip_chst_get(ch);
		}

		/*debug only di_dbg = di_dbg & (~DBG_M_TSK);*/
		dbg_reg("%s:ch[%d] ready end\n", __func__, ch);
		#if 0
		if (!err_flg)
			set_reg_flag(ch, false);
		#endif
		break;
	case eDI_TOP_STATE_BYPASS:
		/*from bypass complet to unreg*/
		di_vframe_unreg(ch);
		di_unreg_variable(ch);

		set_reg_flag(ch, false);
		if (!get_reg_flag_all()) {
			di_unreg_setting();
			dpre_init();
			dpost_init();
		}
		dip_chst_set(ch, eDI_TOP_STATE_IDLE);
		ret = true;

		break;
	case eDI_TOP_STATE_IDLE:
		PR_WARN("have unreg\n");
		break;
	case eDI_TOP_STATE_REG_STEP1:
		dbg_dbg("%s:in reg step1\n", __func__);
		di_vframe_unreg(ch);
		set_reg_flag(ch, false);
		dip_chst_set(ch, eDI_TOP_STATE_IDLE);

		ret = true;
		break;
	case eDI_TOP_STATE_REG_STEP1_P1:
		/*wait:*/
		cnt = 0;
		chst2 = dip_chst_get(ch);
		while (chst2 == eDI_TOP_STATE_REG_STEP1_P1 || cnt < 5) {
			task_send_ready();
			usleep_range(3000, 3001);
			cnt++;
		}
		if (cnt >= 5)
			PR_ERR("%s:ch[%d] in p1 timeout\n", __func__, ch);

		set_reg_flag(ch, false);

		di_vframe_unreg(ch);
		/*trig unreg*/
		dip_chst_set(ch, eDI_TOP_STATE_UNREG_STEP1);
		task_send_cmd(LCMD1(eCMD_UNREG, ch));
		/*debug only di_dbg = di_dbg|DBG_M_TSK;*/

		/*wait*/
		ppre->unreg_req_flag_cnt = 0;
		chst2 = dip_chst_get(ch);

		while (chst2 != eDI_TOP_STATE_IDLE) {
			task_send_ready();
			usleep_range(10000, 10001);
			/*msleep(5);*/
			if (ppre->unreg_req_flag_cnt++
				> dim_get_reg_unreg_cnt()) {
				dim_reg_timeout_inc();
				PR_ERR("%s:ch[%d] unreg timeout!!!\n",
				       __func__,
				       ch);
				/*di_unreg_process();*/
				err_flg = true;
				break;
			}

			chst2 = dip_chst_get(ch);
		}

		/*debug only di_dbg = di_dbg & (~DBG_M_TSK);*/
		dbg_reg("%s:ch[%d] ready end\n", __func__, ch);
		ret = true;

		break;
	case eDI_TOP_STATE_REG_STEP2:
		di_vframe_unreg(ch);
		di_unreg_variable(ch);
		set_reg_flag(ch, false);
		if (!get_reg_flag_all()) {
			di_unreg_setting();
			dpre_init();
			dpost_init();
		}

		dip_chst_set(ch, eDI_TOP_STATE_IDLE);

		ret = true;
		break;
	case eDI_TOP_STATE_UNREG_STEP1:
	case eDI_TOP_STATE_UNREG_STEP2:
		task_send_cmd(LCMD1(eCMD_UNREG, ch));

		/*wait*/
		ppre->unreg_req_flag_cnt = 0;
		while (dip_chst_get(ch) != eDI_TOP_STATE_IDLE) {
			usleep_range(10000, 10001);
			if (ppre->unreg_req_flag_cnt++ >
				dim_get_reg_unreg_cnt()) {
				dim_reg_timeout_inc();
				PR_ERR("%s:unreg_reg_flag timeout!!!\n",
				       __func__);
				di_vframe_unreg(ch);
				err_flg = true;
				break;
			}
		}
		break;
	case eDI_TOP_STATE_NOPROB:
	default:
		PR_ERR("err: not prob[%d]\n", chst);
		break;
	}
	if (err_flg)
		ret = false;

	return ret;
}

/*process for reg and unreg cmd*/
void dip_chst_process_reg(unsigned int ch)
{
	enum eDI_TOP_STATE chst;
	struct vframe_s *vframe;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);
	bool reflesh = true;

	while (reflesh) {
		reflesh = false;

	chst = dip_chst_get(ch);

	/*dbg_reg("%s:ch[%d]%s\n", __func__, ch, dip_chst_get_name(chst));*/

	switch (chst) {
	case eDI_TOP_STATE_NOPROB:
	case eDI_TOP_STATE_IDLE:
		break;
	case eDI_TOP_STATE_REG_STEP1:/*wait peek*/
		vframe = pw_vf_peek(ch);

		if (vframe) {
			dim_tr_ops.pre_get(vframe->omx_index);
			set_flag_trig_unreg(ch, false);
			#if 0
			di_reg_variable(ch);

			/*?how  about bypass ?*/
			if (ppre->bypass_flag) {
				/* complete bypass */
				set_bypass2_complete(ch, true);
				if (!get_reg_flag_all()) {
					/*first channel reg*/
					dpre_init();
					dpost_init();
					di_reg_setting(ch, vframe);
				}
				dip_chst_set(ch, eDI_TOP_STATE_BYPASS);
				set_reg_flag(ch, true);
			} else {
				set_bypass2_complete(ch, false);
				if (!get_reg_flag_all()) {
					/*first channel reg*/
					dpre_init();
					dpost_init();
					di_reg_setting(ch, vframe);
				}
				dip_chst_set(ch, eDI_TOP_STATE_REG_STEP2);
			}
			#else
			dip_chst_set(ch, eDI_TOP_STATE_REG_STEP1_P1);
			#endif

			reflesh = true;
		}
		break;
	case eDI_TOP_STATE_REG_STEP1_P1:
		vframe = pw_vf_peek(ch);
		if (!vframe) {
			PR_ERR("%s:p1 vfm nop\n", __func__);
			dip_chst_set(ch, eDI_TOP_STATE_REG_STEP1);

			break;
		}
		di_reg_variable(ch, vframe);
		/*di_reg_process_irq(ch);*/ /*check if bypass*/

		/*?how  about bypass ?*/
		if (ppre->bypass_flag) {
			/* complete bypass */
			set_bypass2_complete(ch, true);
			if (!get_reg_flag_all()) {
				/*first channel reg*/
				dpre_init();
				dpost_init();
				di_reg_setting(ch, vframe);
			}
			dip_chst_set(ch, eDI_TOP_STATE_BYPASS);
			set_reg_flag(ch, true);
		} else {
			set_bypass2_complete(ch, false);
			if (!get_reg_flag_all()) {
				/*first channel reg*/
				dpre_init();
				dpost_init();
				di_reg_setting(ch, vframe);
			}
			/*this will cause first local buf not alloc*/
			/*dim_bypass_first_frame(ch);*/
			dip_chst_set(ch, eDI_TOP_STATE_REG_STEP2);
			/*set_reg_flag(ch, true);*/
		}

		reflesh = true;
		break;
	case eDI_TOP_STATE_REG_STEP2:/*now no change to do*/
		if (dip_cma_get_st(ch) == eDI_CMA_ST_READY) {
			if (di_cfg_top_get(eDI_CFG_first_bypass)) {
				if (get_sum_g(ch) == 0)
					dim_bypass_first_frame(ch);
				else
					PR_INF("ch[%d],g[%d]\n",
					       ch, get_sum_g(ch));
			}
			dip_chst_set(ch, eDI_TOP_STATE_READY);
			set_reg_flag(ch, true);
			/*move to step1 dim_bypass_first_frame(ch);*/
		}
		break;
	case eDI_TOP_STATE_READY:

		break;
	case eDI_TOP_STATE_BYPASS:
		/*do nothing;*/
		break;
	case eDI_TOP_STATE_UNREG_STEP1:

#if 0
		if (!get_reg_flag(ch)) {	/*need wait pre/post done*/
			dip_chst_set(ch, eDI_TOP_STATE_UNREG_STEP2);
			reflesh = true;
		}
#else
		/*debug only dbg_reg("%s:UNREG_STEP1\n", __func__);*/

		if (dpre_can_exit(ch) && dpst_can_exit(ch)) {
			dip_chst_set(ch, eDI_TOP_STATE_UNREG_STEP2);
			set_reg_flag(ch, false);
			reflesh = true;
		}
#endif
		break;
	case eDI_TOP_STATE_UNREG_STEP2:
		/*debug only dbg_reg("%s:ch[%d]:UNREG_STEP2\n",__func__, ch);*/
		di_unreg_variable(ch);
		if (!get_reg_flag_all()) {
			di_unreg_setting();
			dpre_init();
			dpost_init();
		}

		dip_chst_set(ch, eDI_TOP_STATE_IDLE);
		/*debug only dbg_reg("ch[%d]UNREG_STEP2 end\n",ch);*/
		break;
	}
	}
}

void dip_chst_process_ch(void)
{
	unsigned int ch;
	unsigned int chst;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		chst = dip_chst_get(ch);
		switch (chst) {
		case eDI_TOP_STATE_REG_STEP2:
			if (dip_cma_get_st(ch) == eDI_CMA_ST_READY) {
				if (di_cfg_top_get(eDI_CFG_first_bypass)) {
					if (get_sum_g(ch) == 0)
						dim_bypass_first_frame(ch);
					else
						PR_INF("ch[%d],g[%d]\n",
						       ch, get_sum_g(ch));
				}
				dip_chst_set(ch, eDI_TOP_STATE_READY);
				set_reg_flag(ch, true);
			}
			break;
#if 1
		case eDI_TOP_STATE_UNREG_STEP1:
			if (dpre_can_exit(ch) && dpst_can_exit(ch)) {
				dip_chst_set(ch, eDI_TOP_STATE_UNREG_STEP2);
				set_reg_flag(ch, false);
			}

			break;
#endif
		case eDI_TOP_STATE_UNREG_STEP2:
			dbg_reg("%s:ch[%d]:at UNREG_STEP2\n", __func__, ch);
			di_unreg_variable(ch);
			if (!get_reg_flag_all()) {
				di_unreg_setting();
				dpre_init();
				dpost_init();
			}

			dip_chst_set(ch, eDI_TOP_STATE_IDLE);
			dbg_reg("ch[%d]STEP2 end\n", ch);
			break;
		}
	}
}

bool dip_chst_change_2unreg(void)
{
	unsigned int ch;
	unsigned int chst;
	bool ret = false;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		chst = dip_chst_get(ch);
		dbg_poll("[%d]%d\n", ch, chst);
		if (chst == eDI_TOP_STATE_UNREG_STEP1) {
			dbg_reg("%s:ch[%d]to UNREG_STEP2\n", __func__, ch);
			set_reg_flag(ch, false);
			dip_chst_set(ch, eDI_TOP_STATE_UNREG_STEP2);
			ret = true;
		}
	}
	return ret;
}

#if 0
void di_reg_flg_check(void)
{
	int ch;
	unsigned int chst;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++)
		chst = dip_chst_get(ch);
}
#endif

void dip_hw_process(void)
{
	di_dbg_task_flg = 5;
	dpre_process();
	di_dbg_task_flg = 6;
	pre_mode_setting();
	di_dbg_task_flg = 7;
	dpst_process();
	di_dbg_task_flg = 8;
}

const char * const di_top_state_name[] = {
	"NOPROB",
	"IDLE",
	"REG_STEP1",
	"REG_P1",
	"REG_STEP2",
	"READY",
	"BYPASS",
	"UNREG_STEP1",
	"UNREG_STEP2",
};

const char *dip_chst_get_name_curr(unsigned int ch)
{
	const char *p = "";
	enum eDI_TOP_STATE chst;

	chst = dip_chst_get(ch);

	if (chst < ARRAY_SIZE(di_top_state_name))
		p = di_top_state_name[chst];

	return p;
}

const char *dip_chst_get_name(enum eDI_TOP_STATE chst)
{
	const char *p = "";

	if (chst < ARRAY_SIZE(di_top_state_name))
		p = di_top_state_name[chst];

	return p;
}

/**********************************/
/* TIME OUT CHEKC api*/
/**********************************/

void di_tout_int(struct di_time_out_s *tout, unsigned int thd)
{
	tout->en = false;
	tout->timer_start = 0;
	tout->timer_thd = thd;
}

bool di_tout_contr(enum eDI_TOUT_CONTR cmd, struct di_time_out_s *tout)
{
	unsigned long ctimer;
	unsigned long diff;
	bool ret = false;

	ctimer = cur_to_msecs();

	switch (cmd) {
	case eDI_TOUT_CONTR_EN:
		tout->en = true;
		tout->timer_start = ctimer;
		break;
	case eDI_TOUT_CONTR_FINISH:
		if (tout->en) {
			diff = ctimer - tout->timer_start;

			if (diff > tout->timer_thd) {
				tout->over_flow_cnt++;

				if (tout->over_flow_cnt > 0xfffffff0) {
					tout->over_flow_cnt = 0;
					tout->flg_over = 1;
				}
		#if 0
				if (tout->do_func)
					tout->do_func();

		#endif
				ret = true;
			}
			tout->en = false;
		}
		break;

	case eDI_TOUT_CONTR_CHECK:	/*if time is overflow, disable timer*/
		if (tout->en) {
			diff = ctimer - tout->timer_start;

			if (diff > tout->timer_thd) {
				tout->over_flow_cnt++;

				if (tout->over_flow_cnt > 0xfffffff0) {
					tout->over_flow_cnt = 0;
					tout->flg_over = 1;
				}
				#if 0
				if (tout->do_func)
					tout->do_func();

				#endif
				ret = true;
				tout->en = false;
			}
		}
		break;
	case eDI_TOUT_CONTR_CLEAR:
		tout->en = false;
		tout->timer_start = ctimer;
		break;
	case eDI_TOUT_CONTR_RESET:
		tout->en = true;
		tout->timer_start = ctimer;
		break;
	}

	return ret;
}

const unsigned int di_ch2mask_table[DI_CHANNEL_MAX] = {
	DI_BIT0,
	DI_BIT1,
	DI_BIT2,
	DI_BIT3,
};

/****************************************/
/* do_table				*/
/****************************************/

/*for do_table_working*/
#define K_DO_TABLE_LOOP_MAX	15

const struct do_table_s do_table_def = {
	.ptab = NULL,
	.data = NULL,
	.size = 0,
	.op_lst = K_DO_TABLE_ID_STOP,
	.op_crr = K_DO_TABLE_ID_STOP,
	.do_stop = 0,
	.flg_stop = 0,
	.do_pause = 0,
	.do_step = 0,
	.flg_repeat = 0,

};

void do_table_init(struct do_table_s *pdo,
		   const struct do_table_ops_s *ptable,
		   unsigned int size_tab)
{
	memcpy(pdo, &do_table_def, sizeof(struct do_table_s));

	if (ptable) {
		pdo->ptab = ptable;
		pdo->size = size_tab;
	}
}

/*if change to async?*/
/* now only call in same thread */
void do_talbe_cmd(struct do_table_s *pdo, enum eDO_TABLE_CMD cmd)
{
	switch (cmd) {
	case eDO_TABLE_CMD_NONE:
		pr_info("test:%s\n", __func__);
		break;
	case eDO_TABLE_CMD_STOP:
		pdo->do_stop = true;
		break;
	case eDO_TABLE_CMD_START:
		if (pdo->op_crr == K_DO_TABLE_ID_STOP) {
			pdo->op_lst = pdo->op_crr;
			pdo->op_crr = K_DO_TABLE_ID_START;
			pdo->do_stop = false;
			pdo->flg_stop = false;
		} else if (pdo->op_crr == K_DO_TABLE_ID_PAUSE) {
			pdo->op_crr = pdo->op_lst;
			pdo->op_lst = K_DO_TABLE_ID_PAUSE;
			pdo->do_pause = false;
		} else {
			pr_info("crr is [%d], not start\n", pdo->op_crr);
		}
		break;
	case eDO_TABLE_CMD_PAUSE:
		if (pdo->op_crr <= K_DO_TABLE_ID_STOP) {
			/*do nothing*/
		} else {
			pdo->op_lst = pdo->op_crr;
			pdo->op_crr = K_DO_TABLE_ID_PAUSE;
			pdo->do_pause = true;
		}
		break;
	case eDO_TABLE_CMD_STEP:
		pdo->do_step = true;
		break;
	case eDO_TABLE_CMD_STEP_BACK:
		pdo->do_step = false;
		break;
	default:
		break;
	}
}

bool do_table_is_crr(struct do_table_s *pdo, unsigned int state)
{
	if (pdo->op_crr == state)
		return true;
	return false;
}

void do_table_working(struct do_table_s *pdo)
{
	const struct do_table_ops_s *pcrr;
	unsigned int ret = 0;
	unsigned int next;
	bool flash = false;
	unsigned int cnt = 0;	/*proction*/
	unsigned int lst_id;	/*dbg only*/
	char *name = "";	/*dbg only*/
	bool need_pr = false;	/*dbg only*/

	if (!pdo)
		return;

	if (!pdo->ptab		||
	    pdo->op_crr >= pdo->size) {
		PR_ERR("di:err:%s:ovflow:0x%p,0x%p,crr=%d,size=%d\n",
		       __func__,
		       pdo, pdo->ptab,
		       pdo->op_crr,
		       pdo->size);
		return;
	}

	pcrr = pdo->ptab + pdo->op_crr;

	if (pdo->name)
		name = pdo->name;
	/*stop ?*/
	if (pdo->do_stop &&
	    (pcrr->mark & K_DO_TABLE_CAN_STOP)) {
		dbg_dt("%s:do stop\n", name);

		/*do stop*/
		if (pcrr->do_stop_op)
			pcrr->do_stop_op(pdo->data);
		/*set status*/
		pdo->op_lst = pdo->op_crr;
		pdo->op_crr = K_DO_TABLE_ID_STOP;
		pdo->flg_stop = true;
		pdo->do_stop = false;

		return;
	}

	/*pause?*/
	if (pdo->op_crr == K_DO_TABLE_ID_STOP	||
	    pdo->op_crr == K_DO_TABLE_ID_PAUSE)
		return;

	do {
		flash = false;
		cnt++;
		if (cnt > K_DO_TABLE_LOOP_MAX) {
			PR_ERR("di:err:%s:loop more %d\n", name, cnt);
			break;
		}

		/*stop again? */
		if (pdo->do_stop &&
		    (pcrr->mark & K_DO_TABLE_CAN_STOP)) {
			/*do stop*/
			dbg_dt("%s: do stop in loop\n", name);
			if (pcrr->do_stop_op)
				pcrr->do_stop_op(pdo->data);
			/*set status*/
			pdo->op_lst = pdo->op_crr;
			pdo->op_crr = K_DO_TABLE_ID_STOP;
			pdo->flg_stop = true;
			pdo->do_stop = false;

			break;
		}

		/*debug:*/
		lst_id = pdo->op_crr;
		need_pr = true;

		if (pcrr->con) {
			if (pcrr->con(pdo->data))
				ret = pcrr->do_op(pdo->data);
			else
				break;

		} else {
			ret = pcrr->do_op(pdo->data);
			dbg_dt("do_table:do:%d:ret=0x%x\n", pcrr->id, ret);
		}

		/*not finish, keep current status*/
		if ((ret & K_DO_TABLE_R_B_FINISH) == 0) {
			dbg_dt("%s:not finish,wait\n", __func__);
			break;
		}

		/*fix to next */
		if (ret & K_DO_TABLE_R_B_NEXT) {
			pdo->op_lst = pdo->op_crr;
			pdo->op_crr++;
			if (pdo->op_crr >= pdo->size) {
				pdo->op_crr = pdo->flg_repeat ?
					K_DO_TABLE_ID_START
					: K_DO_TABLE_ID_STOP;
				dbg_dt("%s:to end,%d\n", __func__,
				       pdo->op_crr);
				break;
			}
			/*return;*/
			flash = true;
		} else {
			next = ((ret & K_DO_TABLE_R_B_OTHER) >>
					K_DO_TABLE_R_B_OTHER_SHIFT);
			if (next < pdo->size) {
				pdo->op_lst = pdo->op_crr;
				pdo->op_crr = next;
				if (next > K_DO_TABLE_ID_STOP)
					flash = true;
				else
					flash = false;
			} else {
				PR_ERR("%s: next[%d] err:\n",
				       __func__, next);
			}
		}
		/*debug 1:*/
		need_pr = false;
		if (lst_id != pdo->op_crr) {
			dbg_dt("do_table:%s:%s->%s\n", pdo->name,
			       pdo->ptab[lst_id].name,
			       pdo->ptab[pdo->op_crr].name);
		}

		pcrr = pdo->ptab + pdo->op_crr;
	} while (flash && !pdo->do_step);

	/*debug 2:*/
	if (need_pr) {
		if (lst_id != pdo->op_crr) {
			dbg_dt("do_table2:%s:%s->%s\n", pdo->name,
			       pdo->ptab[lst_id].name,
			       pdo->ptab[pdo->op_crr].name);
		}
	}
}

/**********************************/

/****************************/
void dip_init_value_reg(unsigned int ch)
{
	struct di_post_stru_s *ppost;
	struct di_buf_s *keep_post_buf;
	struct di_pre_stru_s *ppre = get_pre_stru(ch);

	pr_info("%s:\n", __func__);

	/*post*/
	ppost = get_post_stru(ch);
	/*keep buf:*/
	keep_post_buf = ppost->keep_buf_post;

	memset(ppost, 0, sizeof(struct di_post_stru_s));
	ppost->next_canvas_id = 1;
	ppost->keep_buf_post = keep_post_buf;

	/*pre*/
	memset(ppre, 0, sizeof(struct di_pre_stru_s));
}

static bool dip_init_value(void)
{
	unsigned int ch;
	struct di_post_stru_s *ppost;
	struct di_mm_s *mm = dim_mm_get();
	bool ret = false;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		ppost = get_post_stru(ch);
		memset(ppost, 0, sizeof(struct di_post_stru_s));
		ppost->next_canvas_id = 1;

		/*que*/
		ret = di_que_alloc(ch);
	}
	set_current_channel(0);

	/*mm cfg*/
	mm->cfg.di_h = 1080;
	mm->cfg.di_w = 1920;
	mm->cfg.num_local = MAX_LOCAL_BUF_NUM;
	mm->cfg.num_post = MAX_POST_BUF_NUM;
	/*mm sts*/
	mm->sts.mem_start = 0;
	mm->sts.mem_size = 0;
	mm->sts.total_pages = NULL;
	mm->sts.flag_cma = 0;

	return ret;
}

/******************************************
 *	pq ops
 *****************************************/
void dip_init_pq_ops(void)
{
	di_attach_ops_pulldown(&get_datal()->ops_pd);
	di_attach_ops_3d(&get_datal()->ops_3d);
	di_attach_ops_nr(&get_datal()->ops_nr);
	di_attach_ops_mtn(&get_datal()->ops_mtn);

	dim_attach_to_local();
}

/**********************************/
void dip_clean_value(void)
{
	unsigned int ch;

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		/*que*/
		di_que_release(ch);
	}
}

bool dip_prob(void)
{
	bool ret = true;

	ret = dip_init_value();

	di_cfgx_init_val();
	di_cfg_top_init_val();
	di_mp_uit_init_val();
	di_mp_uix_init_val();

	dev_vframe_init();
	didbg_fs_init();
	dip_wq_prob();
	dip_cma_init_val();
	dip_chst_init();

	dpre_init();
	dpost_init();

	dip_init_pq_ops();

	return ret;
}

void dip_exit(void)
{
	dip_wq_ext();
	dev_vframe_exit();
	dip_clean_value();
	didbg_fs_exit();
}

