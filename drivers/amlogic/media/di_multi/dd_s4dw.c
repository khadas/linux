// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/dd_s4dw.c
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
#include "register.h"
#include "register_nr4.h"
#include "deinterlace.h"
#include "deinterlace_dbg.h"

#include "di_data_l.h"
#include "deinterlace_hw.h"
#include "di_hw_v3.h"
#include "di_afbc_v3.h"

#include "di_dbg.h"
#include "di_pps.h"
#include "di_pre.h"
#include "di_prc.h"
#include "di_task.h"
#include "di_vframe.h"
#include "di_que.h"
#include "di_api.h"
#include "di_sys.h"
#include "di_reg_v3.h"
#include "dolby_sys.h"
/*2018-07-18 add debugfs*/
#include <linux/seq_file.h>
#include <linux/debugfs.h>
/*2018-07-18 -----------*/

static unsigned int dim_afbc_skip_en;
module_param_named(dim_afbc_skip_en, dim_afbc_skip_en, uint, 0664);

#ifdef DBG_BUFFER_FLOW
static unsigned int afbc_skip_pps_w, afbc_skip_pps_h;
module_param_named(afbc_skip_pps_w, afbc_skip_pps_w, uint, 0664);
module_param_named(afbc_skip_pps_h, afbc_skip_pps_h, uint, 0664);
#endif
bool dip_cfg_afbc_skip(void)
{
	return (dim_afbc_skip_en & DI_BIT0) ? true : false;
}

bool dip_cfg_afbc_pps(void)
{
	return (dim_afbc_skip_en & DI_BIT1) ? true : false;
}

static bool dbg_change_each_vs(void)
{
	return (dim_afbc_skip_en & DI_BIT3) ? true : false;
}

#ifdef DBG_BUFFER_FLOW
static bool dbg_flow(void)
{
	return (dim_afbc_skip_en & DI_BIT4) ? true : false;
}
#endif

static bool dbg_buffer_ext(void)
{
	return (dim_afbc_skip_en & DI_BIT5) ? true : false;
}

static bool dbg_vfm_cvs(void)
{
	return (dim_afbc_skip_en & DI_BIT6) ? true : false;
}

bool s4dw_test_ins(void)
{
	/* use internal buffer to test create instance */
	return (dim_afbc_skip_en & DI_BIT8) ? true : false;
}

static bool dip_cfg_afbc_dbg_display_buffer(void)
{
	return (dim_afbc_skip_en & DI_BIT9) ? true : false;
}

void dim_dbg_buffer_flow(struct di_ch_s *pch,
			 unsigned long addr,
			 unsigned long addr2,
			 unsigned int pos)
{
#ifdef DBG_BUFFER_FLOW
	unsigned int ch = 99;

	if (dbg_flow()) {
		if (pch)
			ch = pch->ch_id;
		PR_INF("dbg buffer:ch[%d],0x%lx,0x%lx,%d\n", ch, addr, addr2, pos);
	}
#endif /*DBG_BUFFER_FLOW*/
}

void dim_dbg_buffer_ext(struct di_ch_s *pch,
			struct di_buffer *buffer,
			unsigned int pos)
{
	#ifdef DBG_BUFFER_EXT
	unsigned int ch = 99;

	if (!dbg_buffer_ext())
		return;
	if (pch)
		ch = pch->ch_id;
	if (!buffer->vf) {
		PR_INF("dbg_buffer_ex:ch[%d],%d,0x%px,no vf\n",
			ch, pos, buffer);
		return;
	}
	PR_INF("dbg_buffer_ex:ch[%d],%d,0x%px,0x%x,[0x%px,0x%px]\n",
		ch, pos, buffer, buffer->flag,
		buffer->private_data, buffer->vf->private_data);
	PR_INF("\t0x%lx,0x%lx,0x%lx<%d %d>:0x%x:0x%x\n",
		buffer->phy_addr,
		buffer->vf->canvas0_config[0].phy_addr,
		buffer->vf->canvas0_config[1].phy_addr,
		buffer->vf->canvas0_config[0].width,
		buffer->vf->canvas0_config[0].height,
		buffer->vf->canvas0Addr,
		buffer->vf->type);

	#endif
}

void dim_dbg_vf_cvs(struct di_ch_s *pch,
			struct vframe_s *vfm,
			unsigned int pos)
{
	#ifdef DBG_VFM_CVS
	struct canvas_config_s *cvs;
	unsigned int ch = 99;

	if (!dbg_vfm_cvs() || !vfm)
		return;
	if (pch)
		ch = pch->ch_id;
	cvs = &vfm->canvas0_config[0];

	PR_INF("dbg_vf:ch[%d],%d,[%d],0x%px[0x%px]\n",
		ch, pos, vfm->plane_num, vfm, vfm->private_data);
	PR_INF("\t0x%lx,<%d %d>,0x%x",
		cvs->phy_addr,
		cvs->width,
		cvs->height,
		vfm->canvas0Addr);
	if (vfm->plane_num > 1) {
		cvs = &vfm->canvas0_config[1];
		PR_INF("dbg_vf:\t0x%lx,<%d %d>\n",
			cvs->phy_addr,
			cvs->width,
			cvs->height);
	}

	#endif
}

static int s4dw_init_buf_simple(struct di_ch_s *pch)
{
	int i, j;

	struct vframe_s **pvframe_in;
	struct vframe_s *pvframe_in_dup;
	struct vframe_s *pvframe_local;
	struct vframe_s *pvframe_post;
	struct di_buf_s *pbuf_local;
	struct di_buf_s *pbuf_in;
	struct di_buf_s *pbuf_post;
	struct di_buf_s *di_buf;

	struct div2_mm_s *mm; /*mm-0705*/
	unsigned int cnt;

	unsigned int ch;
	unsigned int length;

	/**********************************************/
	/* count buf info */
	/**********************************************/
	ch = pch->ch_id;
	pvframe_in	= get_vframe_in(ch);
	pvframe_in_dup	= get_vframe_in_dup(ch);
	pvframe_local	= get_vframe_local(ch);
	pvframe_post	= get_vframe_post(ch);
	pbuf_local	= get_buf_local(ch);
	pbuf_in		= get_buf_in(ch);
	pbuf_post	= get_buf_post(ch);
	mm		= dim_mm_get(ch);

	/* decoder'buffer had been releae no need put */
	for (i = 0; i < MAX_IN_BUF_NUM; i++)
		pvframe_in[i] = NULL;

	/**********************************************/
	/* que init */
	/**********************************************/

	queue_init(ch, MAX_LOCAL_BUF_NUM * 2);
	di_que_init(ch); /*new que*/

	/**********************************************/
	/* local buf init */
	/**********************************************/
	for (i = 0; i < (MAX_LOCAL_BUF_NUM * 2); i++) {
		di_buf = &pbuf_local[i];

		memset(di_buf, 0, sizeof(struct di_buf_s));
		di_buf->type = VFRAME_TYPE_LOCAL;
		di_buf->pre_ref_count = 0;
		di_buf->post_ref_count = 0;
		for (j = 0; j < 3; j++)
			di_buf->canvas_width[j] = mm->cfg.canvas_width[j];

		di_buf->index = i;
		di_buf->flg_null = 1;
		di_buf->vframe = &pvframe_local[i];
		di_buf->vframe->private_data = di_buf;
		di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
		di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
		di_buf->queue_index = -1;
		di_buf->invert_top_bot_flag = 0;
		di_buf->channel = ch;
		di_buf->canvas_config_flag = 2;

		di_que_in(ch, QUE_PRE_NO_BUF, di_buf);
		dbg_init("buf[%d], addr=0x%lx\n", di_buf->index,
			 di_buf->nr_adr);
	}

	/**********************************************/
	/* input buf init */
	/**********************************************/
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		di_buf = &pbuf_in[i];

		if (di_buf) {
			memset(di_buf, 0, sizeof(struct di_buf_s));
			di_buf->type = VFRAME_TYPE_IN;
			di_buf->pre_ref_count = 0;
			di_buf->post_ref_count = 0;
			di_buf->vframe = &pvframe_in_dup[i];
			di_buf->vframe->private_data = di_buf;
			di_buf->index = i;
			di_buf->queue_index = -1;
			di_buf->invert_top_bot_flag = 0;
			di_buf->channel = ch;
			di_que_in(ch, QUE_IN_FREE, di_buf);
		}
	}
	/**********************************************/
	/* post buf init */
	/**********************************************/
	cnt = 0;
	for (i = 0; i < MAX_POST_BUF_NUM; i++) {
		di_buf = &pbuf_post[i];

		if (di_buf) {
			memset(di_buf, 0, sizeof(struct di_buf_s));

			di_buf->type = VFRAME_TYPE_POST;
			di_buf->index = i;
			di_buf->vframe = &pvframe_post[i];
			di_buf->vframe->private_data = di_buf;
			di_buf->queue_index = -1;
			di_buf->invert_top_bot_flag = 0;
			di_buf->channel = ch;
			di_buf->flg_null = 1;

			if (dimp_get(edi_mp_post_wr_en) &&
			    dimp_get(edi_mp_post_wr_support)) {
				di_buf->canvas_width[NR_CANVAS] =
					mm->cfg.pst_cvs_w;
				//di_buf->canvas_rot_w = rot_width;
				di_buf->canvas_height = mm->cfg.pst_cvs_h;
				di_buf->canvas_config_flag = 1;

				dbg_init("[%d]post buf:%d: addr=0x%lx\n", i,
					 di_buf->index, di_buf->nr_adr);
			}
			cnt++;
			//di_que_in(channel, QUE_POST_FREE, di_buf);
			di_que_in(ch, QUE_PST_NO_BUF, di_buf);

		} else {
			PR_ERR("%s:%d:post buf is null\n", __func__, i);
		}
	}
	/* check */
	length = di_que_list_count(ch, QUE_PST_NO_BUF);

	di_buf = di_que_out_to_di_buf(ch, QUE_PST_NO_BUF);
	dbg_mem2("%s:ch[%d]:pst_no_buf:%d, indx[%d]\n", __func__,
		 ch, length, di_buf->index);
	return 0;
}

static int s4dw_cnt_post_buf(struct di_ch_s *pch)
{
	unsigned int ch;
	struct div2_mm_s *mm;
	unsigned int nr_width, /*nr_canvas_width,*/canvas_align_width = 32;
	unsigned int height, width;
	unsigned int tmpa, tmpb;
	unsigned int canvas_height;
	enum EDPST_MODE mode;

	ch = pch->ch_id;
	mm = dim_mm_get(ch);

	height	= mm->cfg.di_h;
	canvas_height = roundup(height, 32);
	width	= mm->cfg.di_w;

	mm->cfg.pbuf_hsize = width;
	nr_width = width;
	dbg_mem2("%s w[%d]h[%d]\n", __func__, width, height);

	/**********************************************/
	/* count buf info */
	/**********************************************/
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		canvas_align_width = 64;

	/***********************/
	mode = pch->mode; //EDPST_MODE_NV21_8BIT

	dbg_mem2("%s:mode:%d\n", __func__, mode);

	/* p */
	//tmp nr_width = roundup(nr_width, canvas_align_width);
	mm->cfg.pst_mode = EDPST_MODE_NV21_8BIT;
	if (mode == EDPST_MODE_NV21_8BIT) {
		nr_width = roundup(nr_width, canvas_align_width);
		tmpa = (nr_width * canvas_height) >> 1;/*uv*/
		tmpb = tmpa;
		tmpa = nr_width * canvas_height;

		mm->cfg.pst_buf_uv_size = roundup(tmpb, PAGE_SIZE);
		mm->cfg.pst_buf_y_size	= roundup(tmpa, PAGE_SIZE);
		mm->cfg.pst_buf_size	= mm->cfg.pst_buf_y_size +
				mm->cfg.pst_buf_uv_size;//tmpa + tmpb;
		mm->cfg.size_post	= mm->cfg.pst_buf_size;
		mm->cfg.pst_cvs_w	= nr_width;
		mm->cfg.pst_cvs_h	= canvas_height;
		mm->cfg.pst_afbci_size	= 0;
		mm->cfg.pst_afbct_size	= 0;
	} else {
		PR_ERR("%s:temp:not nv21\n", __func__);
		return -1;
	}

	mm->cfg.size_buf_hf = 0;

	mm->cfg.dw_size = 0;

	mm->cfg.size_post	= roundup(mm->cfg.size_post, PAGE_SIZE);
	mm->cfg.size_post_page	= mm->cfg.size_post >> PAGE_SHIFT;

	/* p */
	mm->cfg.pbuf_flg.b.page = mm->cfg.size_post_page;
	mm->cfg.pbuf_flg.b.is_i = 0;
	if (mm->cfg.pst_afbct_size)
		mm->cfg.pbuf_flg.b.afbc = 1;
	else
		mm->cfg.pbuf_flg.b.afbc = 0;
	if (mm->cfg.dw_size)
		mm->cfg.pbuf_flg.b.dw = 1;
	else
		mm->cfg.pbuf_flg.b.dw = 0;

	mm->cfg.pbuf_flg.b.typ = EDIM_BLK_TYP_OLDP;

	if (dip_itf_is_ins_exbuf(pch)) {
		mm->cfg.pbuf_flg.b.typ |= EDIM_BLK_TYP_POUT;
		mm->cfg.size_buf_hf = 0;
	}
	dbg_mem2("hf:size:%d\n", mm->cfg.size_buf_hf);

	dbg_mem2("%s:3 pst_cvs_w[%d]\n", __func__, mm->cfg.pst_cvs_w);
	#ifdef PRINT_BASIC
	dbg_mem2("%s:size:\n", __func__);
	dbg_mem2("\t%-15s:0x%x\n", "total_size", mm->cfg.size_post);
	dbg_mem2("\t%-15s:0x%x\n", "total_page", mm->cfg.size_post_page);
	dbg_mem2("\t%-15s:0x%x\n", "buf_size", mm->cfg.pst_buf_size);
	dbg_mem2("\t%-15s:0x%x\n", "y_size", mm->cfg.pst_buf_y_size);
	dbg_mem2("\t%-15s:0x%x\n", "uv_size", mm->cfg.pst_buf_uv_size);
	dbg_mem2("\t%-15s:0x%x\n", "afbci_size", mm->cfg.pst_afbci_size);
	dbg_mem2("\t%-15s:0x%x\n", "afbct_size", mm->cfg.pst_afbct_size);
	dbg_mem2("\t%-15s:0x%x\n", "flg", mm->cfg.pbuf_flg.d32);
	dbg_mem2("\t%-15s:0x%x\n", "dw_size", mm->cfg.dw_size);
	dbg_mem2("\t%-15s:0x%x\n", "size_hf", mm->cfg.size_buf_hf);
	PR_INF("\t%-15s:0x%x\n", "pst_cvs_h", mm->cfg.pst_cvs_h);
	#endif
	return 0;
}

static int s4dw_buf_init(struct di_ch_s *pch)
{
	struct mtsk_cmd_s blk_cmd;
	//struct di_dev_s *de_devp = get_dim_de_devp();
	struct div2_mm_s *mm;
	unsigned int ch;
//	unsigned int tmp_nub;

	ch = pch->ch_id;
	mm = dim_mm_get(ch);

	check_tvp_state(pch);
	//di_cnt_i_buf(pch, 1920, 1088);
	//di_cnt_i_buf(pch, 1920, 2160);
	//di_cnt_pst_afbct(pch);
	mm->cfg.size_pafbct_all	= 0;
	mm->cfg.size_pafbct_one	= 0;
	mm->cfg.nub_pafbct	= 0;
	mm->cfg.dat_pafbct_flg.d32 = 0;

	s4dw_cnt_post_buf(pch);
	s4dw_init_buf_simple(pch);

	//di_que_list_count(ch, QUE_POST_KEEP);
	PR_INF("%s:\n", __func__);
	if (cfgeq(MEM_FLAG, EDI_MEM_M_CMA)	||
	    cfgeq(MEM_FLAG, EDI_MEM_M_CODEC_A)	||
	    cfgeq(MEM_FLAG, EDI_MEM_M_CODEC_B)) {	/*trig cma alloc*/
		dbg_timer(ch, EDBG_TIMER_ALLOC);

		if ((mm->cfg.pbuf_flg.b.typ & 0x8) ==
			   EDIM_BLK_TYP_POUT) {
			//move all to wait:
			di_buf_no2wait(pch, mm->cfg.num_post);
		} else if (mm->cfg.pbuf_flg.b.page) {//@ary_note: ??
			/* post */
			blk_cmd.cmd = ECMD_BLK_ALLOC;

			blk_cmd.nub = mm->cfg.num_post;
			blk_cmd.flg.d32 = mm->cfg.pbuf_flg.d32;
			if (mm->cfg.size_buf_hf)
				blk_cmd.hf_need = 1;
			else
				blk_cmd.hf_need = 0;

			mtask_send_cmd(ch, &blk_cmd);
		}

		mm->sts.flg_alloced = true;
		mm->cfg.num_rebuild_keep = 0;
	}
	return 0;
}

#ifdef DBG_BUFFER_FLOW
static unsigned int s4dw_buf_h;
module_param_named(s4dw_buf_h, s4dw_buf_h, uint, 0664);
#endif /* DBG_BUFFER_FLOW */

const struct di_mm_cfg_s c_mm_cfg_s4_cp = {
	.di_h	=	544, //540,
	.di_w	=	960,//960,
	.num_local	=	0,
	.num_post	=	9,
	.num_step1_post = 0,
};

static void s4dw_reg_variable(struct di_ch_s *pch, struct vframe_s *vframe)
{
	unsigned int ch;
	struct di_pre_stru_s *ppre;
	struct di_post_stru_s *ppost;
	struct di_dev_s *de_devp = get_dim_de_devp();
	struct div2_mm_s *mm;

	if (!pch)
		return;
	ch = pch->ch_id;

	PR_INF("%s:=%d\n", __func__, ch);
	//tmp dim_slt_init();

	ppre = get_pre_stru(ch);

	dim_print("%s:0x%p\n", __func__, vframe);
	mm = dim_mm_get(ch);
	/**/
	if (vframe) {
		/* ext dip_init_value_reg(channel, vframe);*/
		/* struct di_post_stru_s */
		ppost = get_post_stru(ch);
		ppost = get_post_stru(ch);
		memset(ppost, 0, sizeof(struct di_post_stru_s));
		ppost->next_canvas_id = 1; //??
		/*pre: struct di_pre_stru_s */
		memset(ppre, 0, sizeof(struct di_pre_stru_s));

		dim_bypass_st_clear(pch);
		if (pch->itf.op_cfg_ch_set)
			pch->itf.op_cfg_ch_set(pch);

		pch->en_hf	= false;
		pch->en_hf_buf	= false;

		pch->en_dw_mem = false;
		pch->en_dw = false;
		/* normal */
		dimp_set(edi_mp_prog_proc_config, 0x23);
		dimp_set(edi_mp_use_2_interlace_buff, 0);//1? 0?
		pch->src_type = vframe->source_type; //?? no used
		pch->ponly = false;
		//?? di_set_default(ch);
		memcpy(&mm->cfg, &c_mm_cfg_s4_cp, sizeof(mm->cfg));
#ifdef DBG_BUFFER_FLOW
		if (s4dw_buf_h)
			mm->cfg.di_h = s4dw_buf_h;
#endif
		PR_INF("%s:di_h:%d\n", __func__, mm->cfg.di_h);
		mm->cfg.dis_afbce = 0;
		mm->cfg.fix_buf = 1;
		mm->cfg.num_local = 0;
		mm->cfg.num_post = 5; //tmp;
		pch->mode = EDPST_MODE_NV21_8BIT;
		/* dip_init_value_reg end*/
		/*******************************************/
		dim_ddbg_mod_save(EDI_DBG_MOD_RVB, ch, 0);

		ppre->bypass_flag = false;

		de_devp->nrds_enable = 0; //??
		//de_devp->pps_enable = dimp_get(edi_mp_pps_en); // ??
		/*di pre h scaling down: sm1 tm2*/
		de_devp->h_sc_down_en = 0; //??

		if (dimp_get(edi_mp_di_printk_flag) & 2) //??
			dimp_set(edi_mp_di_printk_flag, 1);

//		dim_print("%s: vframe come => di_init_buf\n", __func__);

	//no use	if (cfgeq(MEM_FLAG, EDI_MEM_M_REV) && !de_devp->mem_flg)
	//no use		dim_rev_mem_check();

		/*need before set buffer*/
		if (dim_afds())
			dim_afds()->reg_val(pch);

		s4dw_buf_init(pch);

		//no use pre_sec_alloc(pch, mm->cfg.dat_idat_flg.d32);
		//no use pst_sec_alloc(pch, mm->cfg.dat_pafbct_flg.d32);
		ppre->mtn_status =	//?? need check
			get_ops_mtn()->adpative_combing_config
				(vframe->width,
				 (vframe->height >> 1),
				 (vframe->source_type),
				 VFMT_IS_P(vframe->type),/*is_progressive(vframe)*/
				 vframe->sig_fmt);

		dimh_patch_post_update_mc_sw(DI_MC_SW_REG, true);
		di_sum_reg_init(ch); //check

		//?? dcntr_reg(1);

		set_init_flag(ch, true);/*init_flag = 1;*/

		dim_ddbg_mod_save(EDI_DBG_MOD_RVE, ch, 0);
	}
}

enum ES4DW_BYPASS_ID {
	ES4DW_BYPASS_ALL_BYPASS = 1,
	ES4DW_BYPASS_CH,
	ES4DW_BYPASS_NOT_AFBCD,
	ES4DW_BYPASS_I,
	ES4DW_BYPASS_TYP,
	ES4DW_BYPASS_SRC_PPMGR,
	ES4DW_BYPASS_SIZE_UP_4K,
	ES4DW_BYPASS_SIZE_DOWN,
	ES4DW_BYPASS_EOS,
};

static unsigned int s4dw_bypasse_checkvf(struct vframe_s *vf)
{
	unsigned int x, y;

	if (!vf)
		return 0;
	if (!IS_COMP_MODE(vf->type))
		return ES4DW_BYPASS_NOT_AFBCD;
	if (VFMT_IS_I(vf->type))
		return ES4DW_BYPASS_I;
	if (vf->type & DIM_BYPASS_VF_TYPE)
		return ES4DW_BYPASS_TYP;
	if (vf->type & VIDTYPE_V4L_EOS)
		return ES4DW_BYPASS_EOS;
	if (vf->source_type == VFRAME_SOURCE_TYPE_PPMGR)
		return ES4DW_BYPASS_SRC_PPMGR;

	dim_vf_x_y(vf, &x, &y);
	if (x > 3840 || y > 2160)
		return ES4DW_BYPASS_SIZE_UP_4K;

	if (x < 128 || y < 16)
		return ES4DW_BYPASS_SIZE_DOWN;

	return 0;
}

static u32 s4dw_bypass_checkother(struct di_ch_s *pch)
{
	if (dimp_get(edi_mp_di_debug_flag) & 0x100000)
		return ES4DW_BYPASS_ALL_BYPASS;
	/* EDI_CFGX_BYPASS_ALL */
	di_cfgx_set(pch->ch_id,
		    EDI_CFGX_BYPASS_ALL,
		    pch->itf.bypass_ext, DI_BIT4);
	if (di_cfgx_get(pch->ch_id, EDI_CFGX_BYPASS_ALL))
		return ES4DW_BYPASS_CH;
	//other
	return 0;
}

static u32 s4dw_is_bypass(struct di_ch_s *pch, struct vframe_s *vf)
{
	u32 ret = 0;
	//static u32 last_ret;//dbg only

	ret = s4dw_bypass_checkother(pch);
	if (!ret && vf)
		ret = s4dw_bypasse_checkvf(vf);
	//debug only
	if (pch->last_bypass != ret) {
		PR_INF("%s:ch[%d]:%d->%d\n", __func__,
		       pch->ch_id, pch->last_bypass, ret);
		pch->last_bypass = ret;
	}
	return ret;
}

/**/
static bool s4dw_bypass_2_ready_bynins(struct di_ch_s *pch,
				       struct dim_nins_s *nins)
{
	void *in_ori;
//	struct dim_nins_s	*nins =NULL;
	struct di_buffer *buffer, *buffer_o;
	struct di_buf_s *buf_pst;

	if (!nins) {
		PR_ERR("%s:no in ori ?\n", __func__);
		return true;
	}
	in_ori = nins->c.ori;

	/* recycle */
	nins->c.ori = NULL;
	nins_used_some_to_recycle(pch, nins);

	/* to ready buffer */
	if (dip_itf_is_ins(pch)) {
		/* need mark bypass flg*/
		buffer = (struct di_buffer *)in_ori;
		buffer->flag |= DI_FLAG_BUF_BY_PASS;
	}
	if (dip_itf_is_ins_exbuf(pch)) {
		/* get out buffer */
		buf_pst = di_que_out_to_di_buf(pch->ch_id, QUE_POST_FREE);
		if (!buf_pst) {
			PR_ERR("%s:no post free\n", __func__);
			return true;
		}

		buffer_o = buf_pst->c.buffer;
		if (!buffer_o) {
			PR_ERR("%s:no buffer_o\n", __func__);
			return true;
		}
		di_buf_clear(pch, buf_pst);
		di_que_in(pch->ch_id, QUE_PST_NO_BUF_WAIT, buf_pst);
		if (buffer->flag & DI_FLAG_EOS ||
		    (buffer->vf && buffer->vf->type & VIDTYPE_V4L_EOS))
			buffer_o->flag |= DI_FLAG_EOS;
		if (buffer_o->vf)
			buffer_o->vf->vf_ext = buffer->vf;
		buffer_o->flag |= DI_FLAG_BUF_BY_PASS;
		ndrd_qin(pch, buffer_o);
	} else {
		ndrd_qin(pch, in_ori);
	}

	return true;
}

const struct dim_s4dw_data_s dim_s4dw_def = {
	.reg_variable	= s4dw_reg_variable,
	.is_bypass	= s4dw_is_bypass,
	.is_bypass_sys	= s4dw_bypass_checkother,
	.fill_2_ready_bynins = s4dw_bypass_2_ready_bynins,
};

static enum DI_ERRORTYPE s4dw_empty_inputl(struct di_ch_s *pch,
					   struct di_buffer *buffer)
{
//	struct dim_itf_s *pintf;
	unsigned int ch;
//	struct di_ch_s *pch;
	unsigned int index;
	unsigned int free_nub;
	struct buf_que_s *pbufq;
	struct dim_nins_s	*pins;
	bool flg_q;

	ch = pch->ch_id;
	pbufq = &pch->nin_qb;
	free_nub	= qbufp_count(pbufq, QBF_NINS_Q_IDLE);
	if (!free_nub) {
		PR_WARN("%s:no nins idle\n", __func__);
		return DI_ERR_IN_NO_SPACE;
	}

	/* get ins */
	flg_q = qbuf_out(pbufq, QBF_NINS_Q_IDLE, &index);
	if (!flg_q) {
		PR_ERR("%s:qout\n", __func__);
		return DI_ERR_IN_NO_SPACE;
	}
	pins = (struct dim_nins_s *)pbufq->pbuf[index].qbc;
	pins->c.ori = buffer;
	dim_dbg_buffer_ext(pch, buffer, 2);

	pins->c.cnt = pch->in_cnt;
	pch->in_cnt++;

	/* @ary_note: eos may be no vf */
	//move to parser memcpy(&pins->c.vfm_cp, buffer->vf, sizeof(pins->c.vfm_cp));

	flg_q = qbuf_in(pbufq, QBF_NINS_Q_DCT, index);
	dbg_copy("%s:%d\n", __func__, pins->c.cnt);
	sum_g_inc(ch);
	if (!flg_q) {
		PR_ERR("%s:qin check\n", __func__);
		qbuf_in(pbufq, QBF_NINS_Q_IDLE, index);
	}
	//dbg_itf_tmode(pch,1);
	if (pch->in_cnt < 4) {
		if (pch->in_cnt == 1)
			dbg_timer(ch, EDBG_TIMER_1_GET);
		else if (pch->in_cnt == 2)
			dbg_timer(ch, EDBG_TIMER_2_GET);
		else if (pch->in_cnt == 3)
			dbg_timer(ch, EDBG_TIMER_3_GET);
	}
	return DI_ERR_NONE;
}

enum DI_ERRORTYPE s4dw_empty_input(struct di_ch_s *pch,
				   struct di_buffer *buffer)
{
	enum DI_ERRORTYPE ret;

	mutex_lock(&pch->itf.lock_in);
	ret = s4dw_empty_inputl(pch, buffer);
	mutex_unlock(&pch->itf.lock_in);

	if (ret == DI_ERR_NONE)
		task_send_ready(20);
	return ret;
}

/* in -> check */
void s4dw_parser_infor(struct di_ch_s *pch)
{
	unsigned int index;
	unsigned int in_nub;
	struct buf_que_s *pbufq;
	struct dim_nins_s	*pins;
	bool flg_q;
	int i;
	struct vframe_s *vfm = NULL;
	struct di_buffer *buffer = NULL;
	u32 bypass_ret;

	if (!pch || !pch->itf.flg_s4dw)
		return;
	pbufq = &pch->nin_qb;
	in_nub	= qbufp_count(pbufq, QBF_NINS_Q_DCT);
	if (!in_nub) {
		//PR_WARN("%s:no nins in\n", __func__);
		return;
	}
	for (i = 0; i < in_nub; i++) {
		/* get ins */
		flg_q = qbuf_out(pbufq, QBF_NINS_Q_DCT, &index);
		if (!flg_q) {
			PR_ERR("%s:qout\n", __func__);
			return;
		}
		pins = (struct dim_nins_s *)pbufq->pbuf[index].qbc;
		buffer = (struct di_buffer *)pins->c.ori;
		if (!buffer->vf) {
			pins->c.wmode.is_bypass = 1;
			dbg_copy("set1:b:%d\n", pins->c.cnt);
		} else {
			vfm = buffer->vf;
			bypass_ret = s4dw_is_bypass(pch, vfm);
			if (bypass_ret) {
				pins->c.wmode.is_bypass = 1;
				dbg_copy("set2:b:%d\n", pins->c.cnt);
			} else {
				dbg_copy("set3:no bypass:%d\n", pins->c.cnt);
				memcpy(&pins->c.vfm_cp, vfm, sizeof(pins->c.vfm_cp));
			}
		}

		flg_q = qbuf_in(pbufq, QBF_NINS_Q_CHECK, index);
		if (!flg_q) {
			PR_ERR("%s:qin check:%d:%d\n", __func__, in_nub, i);
			return;
		}
	}
}

static bool s4dw_pre_sw2bypass(struct di_ch_s *pch, unsigned int bypassid)
{
	struct di_pre_stru_s *ppre;
	unsigned int ch;
	bool ret = false;

	ch = pch->ch_id;
	ppre = get_pre_stru(ch);

	dim_bypass_set(pch, 1, bypassid);
	if (di_bypass_state_get(ch) == 0) {
		//cnt_rebuild = 0; /*from no bypass to bypass*/
		ppre->is_bypass_all = true;
		bset(&pch->self_trig_mask, 29);
		/*********************************/
		di_bypass_state_set(ch, true);/*bypass_state:1;*/
		pch->sumx.need_local = false;
		PR_INF("%s:to  bypass\n", __func__);
		ret = true;
	}
	return ret;
}

/*************************************************
 * return
 *	< 0: do not need do next;
 *	> 0: need di process;
 *	-1: no input;
 *	-2: have done;
 *	1 : not in bypass mode;
 *	2 : have input but not bypass.
 **************************************************/
int s4dw_pre_bypass(struct di_ch_s *pch)
{
	struct dim_nins_s *nins = NULL;
	unsigned int ch, nub, bypass_nub;
	int i, flg = -1, err = 0;

	ch = pch->ch_id;
//	check state
	if (di_bypass_state_get(ch) == 0) {
		nins = nins_peek_pre(pch);
		if (!nins) {
			dbg_copy("%s:no:input\n", __func__);
			return -1; // no input
		}
		if (nins->c.wmode.is_bypass) {
			//switch state:
			dbg_copy("%s:in:bypass\n", __func__);
			s4dw_pre_sw2bypass(pch, nins->c.wmode.bypass_reason);
		} else {
			dbg_copy("%s:not bypass:%d\n", __func__, nins->c.cnt);
		}
	}
//
	if (di_bypass_state_get(ch) == 0)
		return 1;

	nub = nins_cnt(pch, QBF_NINS_Q_CHECK);

	if (nub <= 0)
		return -1;
	bypass_nub = 0;
	for (i = 0; i < nub; i++) {
		nins = nins_peek_pre(pch);
		if (!nins) {
			err = 2;
			break;
		}
		if (!nins->c.wmode.is_bypass) {
			flg = 2; //have input but not bypass.
			break;
		}
		//bypass:
		nins = nins_get(pch);
		if (!nins) {
			err = 1;
			PR_ERR("%s:peek but can't get\n", __func__);
			break;
		}

		if (pch->rsc_bypass.b.ponly_fst_cnt) {
			pch->rsc_bypass.b.ponly_fst_cnt--;
			//PR_INF("%s:%d\n", __func__, pch->rsc_bypass.b.ponly_fst_cnt);
		}
		dim_s4dw_def.fill_2_ready_bynins(pch, nins);
		bypass_nub++;
	}
	if (err) {
		PR_INF("%s:err:%d;%d:%d\n", __func__, err, nub, i);
		return -1;//no input;
	}
	if (nub)
		dbg_copy("%s:%d,%d\n", __func__, nub, bypass_nub);
	return flg;
}

/********************************************
 * s4dw_pre_cfg_bypass
 *	part of pre config;
 * when return > 0 date to ready, and state to bypass
 * when return 0, state to normal.
 ********************************************/
unsigned char s4dw_pre_cfg_bypass(struct di_ch_s *pch, struct dim_nins_s *nins)
{
	unsigned int ch, bypassr;
	struct di_pre_stru_s *ppre;

	ch = pch->ch_id;
	ppre = get_pre_stru(ch);

	bypassr = dim_s4dw_def.is_bypass_sys(pch);
	if (!bypassr) {
		if (nins->c.wmode.is_bypass || nins->c.wmode.need_bypass) {
			bypassr = nins->c.wmode.bypass_reason;
		} else {
			//not bypass:
			return 0;
		}
	}
	/* from normal to bypass ?*/
	//dim_bypass_set(pch, 1, bypassr);

	s4dw_pre_sw2bypass(pch, bypassr);

	if (pch->rsc_bypass.b.ponly_fst_cnt) {
		pch->rsc_bypass.b.ponly_fst_cnt--;
		//PR_INF("%s:%d\n", __func__, pch->rsc_bypass.b.ponly_fst_cnt);
	}
	dim_s4dw_def.fill_2_ready_bynins(pch, nins);

	return (unsigned char)bypassr;
}

/*copy from dimpst_fill_outvf */
static void s4dw_fill_pstvf_cvs(struct di_ch_s *pch,
				struct di_buf_s *di_buf,
				enum EDPST_OUT_MODE mode)
{
	struct canvas_config_s *cvsp, *cvsf;
	struct vframe_s *vfm;
	struct di_buffer *buffer; //for ext buff
	struct div2_mm_s *mm;

	if (!di_buf || !di_buf->vframe)
		return;

	vfm = di_buf->vframe;

	if (dip_itf_is_ins_exbuf(pch)) {
		if (!di_buf->c.buffer) {
			PR_ERR("%s:ext_buf,no buffer\n", __func__);
			return;
		}
		buffer = (struct di_buffer *)di_buf->c.buffer;
		if (!buffer->vf) {
			PR_ERR("%s:ext_buf,no vf\n", __func__);
			return;
		}
		dim_print("%s:buffer %px\n", __func__, buffer);
		cvsf = &buffer->vf->canvas0_config[0];
		cvsp = &vfm->canvas0_config[0];
		cvsp->phy_addr	= cvsf->phy_addr;
		cvsp->width	= cvsf->width;
		cvsp->height	= cvsf->height;
		//if (buffer->vf->plane_num == 1)
		//	return;
		cvsf = &buffer->vf->canvas0_config[1];
		cvsp = &vfm->canvas0_config[1];

		cvsp->phy_addr	= cvsf->phy_addr;
		cvsp->width	= cvsf->width;
		cvsp->height	= cvsf->height;
#ifdef DBG_EXTBUFFER_ONLY_ADDR
		cvsp = &vfm->canvas0_config[0];
		if (cvsp->width)
			return;
		vfm->plane_num = 2;
		mm = dim_mm_get(pch->ch_id);

		cvsp->width = mm->cfg.pst_cvs_w;
		cvsp->height = mm->cfg.pst_cvs_h;

		cvsp = &vfm->canvas0_config[1];
		cvsp->width = mm->cfg.pst_cvs_w;
		cvsp->height = mm->cfg.pst_cvs_h;
#endif
		return;
	}
	/*local*/
	vfm->canvas0Addr = (u32)-1;
	vfm->canvas1Addr = (u32)-1;
	mm = dim_mm_get(pch->ch_id);
	if (mode == EDPST_OUT_MODE_DEF) {
		vfm->plane_num = 1;
		cvsp = &vfm->canvas0_config[0];
		cvsp->phy_addr = di_buf->nr_adr;
		cvsp->block_mode = 0;
		cvsp->endian = 0;
		//di_buf->canvas_width[NR_CANVAS];
		//di_buf->canvas_height;
		cvsp->width = mm->cfg.pst_cvs_w;
		cvsp->height = mm->cfg.pst_cvs_h;
	} else {
		vfm->plane_num = 2;
		cvsp = &vfm->canvas0_config[0];
		cvsp->phy_addr = di_buf->nr_adr;
		cvsp->width = mm->cfg.pst_cvs_w;
		cvsp->height = mm->cfg.pst_cvs_h;
		cvsp->block_mode = 0;
		cvsp->endian = 0;

		cvsp = &vfm->canvas0_config[1];
		cvsp->phy_addr = di_buf->nr_adr + mm->cfg.pst_buf_y_size;
		cvsp->width = mm->cfg.pst_cvs_w;
		cvsp->height = mm->cfg.pst_cvs_h;
		cvsp->block_mode = 0;
		cvsp->endian = 0;
	}
}

static void s4_dw_dimpst_fill_outvf(struct vframe_s *vfm,
				    struct di_buf_s *di_buf,
				    enum EDPST_OUT_MODE mode)
{
	struct di_dev_s *de_devp = get_dim_de_devp();
	unsigned int ch;
	struct di_ch_s *pch;
	//bool ext_buf = false;

	//check ext buffer:
	ch = di_buf->channel;
	pch = get_chdata(ch);

	/* canvas */
	s4dw_fill_pstvf_cvs(pch, di_buf, mode);

	dim_print("%s:addr0[0x%lx], 1[0x%lx],w[%d,%d]\n",
		__func__,
		(unsigned long)vfm->canvas0_config[0].phy_addr,
		(unsigned long)vfm->canvas0_config[1].phy_addr,
		vfm->canvas0_config[0].width,
		vfm->canvas0_config[0].height);

	/* type */
	if (mode == EDPST_OUT_MODE_NV21 ||
	    mode == EDPST_OUT_MODE_NV12) {
		/*clear*/
		vfm->type &= ~(VIDTYPE_VIU_NV12	|
			       VIDTYPE_VIU_444	|
			       VIDTYPE_VIU_NV21	|
			       VIDTYPE_VIU_422	|
			       VIDTYPE_VIU_SINGLE_PLANE	|
			       VIDTYPE_COMPRESS	|
			       VIDTYPE_PRE_INTERLACE);
		vfm->type |= VIDTYPE_VIU_FIELD;
		vfm->type |= VIDTYPE_DI_PW;
		if (mode == EDPST_OUT_MODE_NV21)
			vfm->type |= VIDTYPE_VIU_NV21;
		else
			vfm->type |= VIDTYPE_VIU_NV12;

		/* bit */
		vfm->bitdepth &= ~(BITDEPTH_MASK);
		vfm->bitdepth &= ~(FULL_PACK_422_MODE);
		vfm->bitdepth |= (BITDEPTH_Y8	|
				  BITDEPTH_U8	|
				  BITDEPTH_V8);
	}

	if (de_devp->pps_enable &&
	    dimp_get(edi_mp_pps_position) == 0) {
		if (dimp_get(edi_mp_pps_dstw))
			vfm->width = dimp_get(edi_mp_pps_dstw);

		if (dimp_get(edi_mp_pps_dsth))
			vfm->height = dimp_get(edi_mp_pps_dsth);
	}

	if (di_buf->afbc_info & DI_BIT0)
		vfm->height	= vfm->height / 2;

	//dim_print("%s:h[%d]\n", __func__, vfm->height);
#ifdef NV21_DBG
	if (cfg_vf)
		vfm->type = cfg_vf;
#endif
}

/* copy from mem_resize_buf*/
static void s4dw_mem_resize_buf(struct di_ch_s *pch, struct di_buf_s *di_buf)
{
	struct div2_mm_s *mm;
	struct di_buffer *buf;
	struct canvas_config_s *cvs;

	//struct vframe_s *vfm;
	mm = dim_mm_get(pch->ch_id);

	if (dip_itf_is_ins_exbuf(pch)) {
		buf = (struct di_buffer *)di_buf->c.buffer;
		if (buf && buf->vf) {
			di_buf->canvas_width[NR_CANVAS] = buf->vf->canvas0_config[0].width;
			return;
		}
		PR_ERR("%s:\n", __func__);
	} else {
		//for local buffer:
		cvs = &di_buf->vframe->canvas0_config[0];
		cvs->width = mm->cfg.pst_cvs_w;
		cvs->height = mm->cfg.pst_cvs_h;
		cvs = &di_buf->vframe->canvas0_config[1];
		cvs->width = mm->cfg.pst_cvs_w;
		cvs->height = mm->cfg.pst_cvs_h;
	}

	di_buf->canvas_width[NR_CANVAS]	= mm->cfg.pst_cvs_w;
}

static unsigned char s4dw_pre_buf_config(struct di_ch_s *pch)
{
	unsigned int channel;
	struct di_buf_s *di_buf = NULL;
	struct vframe_s *vframe;
	unsigned char change_type = 0;
	unsigned char change_type2 = 0;
	bool bit10_pack_patch = false;
	unsigned int width_roundup = 2;
	struct di_pre_stru_s *ppre;

	struct di_dev_s *de_devp = get_dim_de_devp();
	int cfg_prog_proc = dimp_get(edi_mp_prog_proc_config);
	unsigned int nv21_flg = 0;
	enum EDI_SGN sgn;
	struct div2_mm_s *mm;
	u32 cur_dw_width = 0xffff;
	u32 cur_dw_height = 0xffff;
	struct dim_nins_s *nins = NULL;
	enum EDPST_OUT_MODE tmpmode;
	unsigned int pps_w, pps_h;
	u32 typetmp;
	unsigned char chg_hdr = 0;

	if (!pch) {
		PR_ERR("%s:no pch\n", __func__);
		return 100;
	}
	channel =  pch->ch_id;
	ppre = get_pre_stru(channel);

//tmp	if (di_blocking) /* pause */
//		return 1;

	if (di_que_list_count(channel, QUE_IN_FREE) < 1)
		return 2;

	if (di_que_is_empty(channel, QUE_POST_FREE))
		return 5;

	/* check post buffer */
	di_buf = di_que_peek(channel, QUE_POST_FREE);
	mm = dim_mm_get(channel);
	if (!dip_itf_is_ins_exbuf(pch) &&
	    (!di_buf->blk_buf ||
	    di_buf->blk_buf->flg.d32 != mm->cfg.pbuf_flg.d32)) {
		if (!di_buf->blk_buf)
			PR_ERR("%s:pst no blk:idx[%d]\n",
		       __func__,
		       di_buf->index);
		else
			PR_ERR("%s:pst flgis err:buf:idx[%d] 0x%x->0x%x\n",
		       __func__,
		       di_buf->index,
		       mm->cfg.pbuf_flg.d32,
		       di_buf->blk_buf->flg.d32);

		return 6;
	}

	/* input get */
	nins = nins_peek_pre(pch);
	if (!nins)
		return 10;
	nins = nins_get(pch);
	if (!nins) {
		PR_ERR("%s:peek but can't get\n", __func__);
		return 14;
	}
	/* bypass control */
	if (s4dw_pre_cfg_bypass(pch, nins))
		return 101;
	/**************************************************/
	vframe = &nins->c.vfm_cp;

	/*mem check*/
	memcpy(&ppre->vfm_cpy, vframe, sizeof(ppre->vfm_cpy));

	sgn = di_vframe_2_sgn(vframe);

	if (nins->c.cnt < 3) {
		if (nins->c.cnt == 0)
			dbg_timer(channel, EDBG_TIMER_1_PRE_CFG);
		else if (nins->c.cnt == 1)
			dbg_timer(channel, EDBG_TIMER_2_PRE_CFG);
		else if (nins->c.cnt == 2)
			dbg_timer(channel, EDBG_TIMER_3_PRE_CFG);
	}

	if (vframe->type & VIDTYPE_COMPRESS) {
		/* backup the original vf->width/height for bypass case */
		cur_dw_width = vframe->width;
		cur_dw_height = vframe->height;
		vframe->width = vframe->compWidth;
		vframe->height = vframe->compHeight;
	}
	dim_tr_ops.pre_get(vframe->index_disp);
	if (dip_itf_is_vfm(pch))
		didbg_vframe_in_copy(channel, nins->c.ori);
	else
		didbg_vframe_in_copy(channel, vframe);

	dim_print("DI:ch[%d]get %dth vf[0x%p][%d] from front %u ms\n",
		  channel,
		  ppre->in_seq, vframe,
		  vframe->index_disp,
		  jiffies_to_msecs(jiffies_64 -
		  vframe->ready_jiffies64));
	vframe->prog_proc_config = (cfg_prog_proc & 0x20) >> 5;

	bit10_pack_patch =  (is_meson_gxtvbb_cpu() ||
						is_meson_gxl_cpu() ||
						is_meson_gxm_cpu());
	width_roundup = bit10_pack_patch ? 16 : width_roundup;

	dimp_set(edi_mp_force_width, 0);

	ppre->source_trans_fmt = vframe->trans_fmt;

	ppre->invert_flag = false;
	vframe->type &= ~TB_DETECT_MASK;

	ppre->width_bk = vframe->width;
	if (dimp_get(edi_mp_force_width))
		vframe->width = dimp_get(edi_mp_force_width);
	if (dimp_get(edi_mp_force_height))
		vframe->height = dimp_get(edi_mp_force_height);
	vframe->width = roundup(vframe->width, width_roundup);

	/* backup frame motion info */
	vframe->combing_cur_lev = dimp_get(edi_mp_cur_lev);/*cur_lev;*/

	dim_print("%s: vf_get => 0x%p\n", __func__, nins->c.ori);

	di_buf = di_que_out_to_di_buf(channel, QUE_IN_FREE);
	di_buf->dec_vf_state = 0;	/*dec vf keep*/
	di_buf->is_eos = 0;
	if (dim_check_di_buf(di_buf, 10, channel))
		return 16;

	if (dimp_get(edi_mp_di_log_flag) & DI_LOG_VFRAME)
		dim_dump_vframe(vframe);

	memcpy(di_buf->vframe, vframe, sizeof(vframe_t));
	dim_dbg_pre_cnt(channel, "cf1");
	di_buf->width_bk = ppre->width_bk;	/*ary.sui 2019-04-23*/
	di_buf->dw_width_bk = cur_dw_width;
	di_buf->dw_height_bk = cur_dw_height;
	di_buf->vframe->private_data = di_buf;
	//10-09	di_buf->vframe->vf_ext = NULL; /*09-25*/

	di_buf->c.in = nins;
	//dbg_nins_log_buf(di_buf, 1);

	di_buf->seq = ppre->in_seq;
	ppre->in_seq++;

	/* source change*/
	pre_vinfo_set(channel, vframe);
	change_type = is_source_change(vframe, channel);

	if (change_type) {
		sgn = di_vframe_2_sgn(vframe);
		ppre->sgn_lv = sgn;
		PR_INF("%s:ch[%d]:%ums %dth source change:\n",
		       "pre cfg",
		       channel,
		       jiffies_to_msecs(jiffies_64),
		       ppre->in_seq);
		PR_INF("source change:cp:0x%x/%d/%d/%d=>0x%x/%d/%d/%d:%d\n",
		       ppre->cur_inp_type,
		       ppre->cur_width,
		       ppre->cur_height,
		       ppre->cur_source_type,
		       di_buf->vframe->type,
		       di_buf->vframe->width,
		       di_buf->vframe->height,
		       di_buf->vframe->source_type, ppre->sgn_lv);
		if (di_buf->vframe->type & VIDTYPE_COMPRESS) {
			ppre->cur_width =
				di_buf->vframe->compWidth;
			ppre->cur_height =
				di_buf->vframe->compHeight;
		} else {
			ppre->cur_width = di_buf->vframe->width;
			ppre->cur_height = di_buf->vframe->height;
		}
		ppre->pps_width = ppre->cur_width;
		ppre->pps_height = ppre->cur_height;

		if (IS_COMP_MODE(vframe->type)) {
			PR_INF("enable afbc skip\n");
			pps_w = ppre->cur_width >> 1;
			pps_h = ppre->cur_height >> 1;
			//dip_cfg_afbcskip_s(pps_w, pps_h);
			ppre->afbc_skip_w = pps_w;
			ppre->afbc_skip_h = pps_h;
			//dip_cfg_afbcskip_ori_s(ppre->cur_width, ppre->cur_height);
			ppre->pps_width = pps_w;
			ppre->pps_height = pps_h;

			PR_INF("en pps\n");
			//de_devp->pps_enable = true;
			pps_w = pps_w >> 1;
			pps_h = pps_h >> 1;
			//dip_cfg_afbcskip_pps_s(pps_w, pps_h);
			ppre->pps_dstw = pps_w;
			ppre->pps_dsth = pps_h;
#ifdef DBG_BUFFER_FLOW
			/* by hand */
			if (afbc_skip_pps_w)
				ppre->pps_dstw = afbc_skip_pps_w;
			if (afbc_skip_pps_h)
				ppre->pps_dsth = afbc_skip_pps_h;

#endif
		}
		/* dw */
		memset(&ppre->shrk_cfg,  0,
		       sizeof(ppre->shrk_cfg));

		ppre->cur_prog_flag = VFMT_IS_P(di_buf->vframe->type);
//			is_progressive(di_buf->vframe);
		if (ppre->cur_prog_flag) {
			ppre->prog_proc_type = 0x10;
			pch->sumx.need_local = 0;
		} else {
			ppre->prog_proc_type = 0;
			pch->sumx.need_local = true;
		}
		ppre->cur_inp_type = di_buf->vframe->type;
		ppre->cur_source_type =
			di_buf->vframe->source_type;
		ppre->cur_sig_fmt = di_buf->vframe->sig_fmt;
		ppre->orientation = di_buf->vframe->video_angle;
		ppre->source_change_flag = 1;
		ppre->input_size_change_flag = true;
		ppre->is_bypass_mem = 0;

		ppre->field_count_for_cont = 0;
		ppre->used_pps = true;
		chg_hdr++;
	} else { /* not change */
		ppre->cur_inp_type = di_buf->vframe->type;
	}
	dimp_set(edi_mp_pps_position, 1);
	dimp_set(edi_mp_pps_dstw, ppre->pps_dstw);
	dimp_set(edi_mp_pps_dsth, ppre->pps_dsth);
	/*--------------------------*/
	if (!change_type) {
		change_type2 = is_vinfo_change(channel);
		if (change_type2) {
			/*ppre->source_change_flag = 1;*/
			ppre->input_size_change_flag = true;
		}
	}
	/*--------------------------*/
	if (VFMT_IS_P(di_buf->vframe->type)) {
		if (ppre->is_bypass_all) { /* ??need check: from bypass to normal*/
			ppre->input_size_change_flag = true;
			ppre->source_change_flag = 1;
			bclr(&pch->self_trig_mask, 29);
			//ppre->field_count_for_cont = 0;
		}
		ppre->is_bypass_all = false;
		di_buf->post_proc_flag = 0;
		ppre->di_inp_buf = di_buf;

		if (!ppre->di_mem_buf_dup_p) {
			/* use n */
			ppre->di_mem_buf_dup_p = di_buf;
		}
	} else {
		PR_ERR("%s:i?\n", __func__);
	}

	/* peek input end */
	/******************************************/
	/*dim_dbg_pre_cnt(channel, "cfg");*/
	/* di_wr_buf */

	if (ppre->prog_proc_type == 0x10) {
/********************************************************/
/* di_buf is local buf */
		//di_que_out_to_di_buf(channel, QUE_POST_FREE);
		//di_buf = pp_pst_2_local(pch);
		di_buf = di_que_out_to_di_buf(channel, QUE_POST_FREE);
		if (dim_check_di_buf(di_buf, 17, channel)) {
			//di_unlock_irqfiq_restore(irq_flag2);
			return 22;
		}
		//move to below s4dw_mem_resize_buf(pch, di_buf);//need check
		/* hdr */
		di_buf->c.en_hdr = false;

		di_buf->post_proc_flag = 0;
		di_buf->canvas_config_flag = 1;
		di_buf->di_wr_linked_buf = NULL;
		di_buf->c.src_is_i = false;

		di_buf->en_hf = 0;

		di_buf->hf_done = 0;

		//if (dim_cfg_pre_nv21(0)) {
		if (pch->ponly && dip_is_ponly_sct_mem(pch)) {
			nv21_flg = 0;
			di_buf->flg_nv21 = 0;
		} else if ((cfggch(pch, POUT_FMT) == 1) || (cfggch(pch, POUT_FMT) == 2)) {
			nv21_flg = 1; /*nv21*/
			di_buf->flg_nv21 = cfggch(pch, POUT_FMT);
		} else if ((cfggch(pch, POUT_FMT) == 5) &&
			   (ppre->sgn_lv == EDI_SGN_4K)) {
			nv21_flg = 1; /*nv21*/
			di_buf->flg_nv21 = 1;
		}
		/*keep dec vf*/
		//di_buf->dec_vf_state = DI_BIT0;
		if (pch->ponly && dip_is_ponly_sct_mem(pch))
			ppre->di_inp_buf->dec_vf_state = DI_BIT0;
		else if (cfggch(pch, KEEP_DEC_VF) == 1)
			ppre->di_inp_buf->dec_vf_state = DI_BIT0;
		else if ((cfggch(pch, KEEP_DEC_VF) == 2) &&
			 (ppre->sgn_lv == EDI_SGN_4K))
			ppre->di_inp_buf->dec_vf_state = DI_BIT0;

		if ((dip_is_support_nv2110(channel)) &&
		    ppre->sgn_lv == EDI_SGN_4K)
			di_buf->afbce_out_yuv420_10 = 1;
		else
			di_buf->afbce_out_yuv420_10 = 0;

		if (ppre->shrk_cfg.shrk_en) {
			dw_pre_sync_addr(&ppre->dw_wr_dvfm, di_buf);
			di_buf->dw_have = true;
			ppre->dw_wr_dvfm.is_dw = true;
		} else {
			di_buf->dw_have = false;
			ppre->dw_wr_dvfm.is_dw = false;
		}

		if (ppre->input_size_change_flag)
			di_buf->trig_post_update = 1;
		else
			di_buf->trig_post_update = 0;

	} else {
		PR_ERR("%s:3 check\n", __func__);
	}

	di_buf->afbc_sgn_cfg	= 0;
	di_buf->sgn_lv		= ppre->sgn_lv;
	ppre->di_wr_buf		= di_buf;
	ppre->di_wr_buf->pre_ref_count = 1;

	if (ppre->cur_inp_type & VIDTYPE_COMPRESS) {
		ppre->di_inp_buf->vframe->width =
			ppre->di_inp_buf->vframe->compWidth;
		ppre->di_inp_buf->vframe->height =
			ppre->di_inp_buf->vframe->compHeight;
	}

	memcpy(di_buf->vframe,
	       ppre->di_inp_buf->vframe, sizeof(vframe_t));
	di_buf->dw_width_bk = cur_dw_width;
	di_buf->dw_height_bk = cur_dw_height;
	di_buf->vframe->private_data = di_buf;
	di_buf->vframe->canvas0Addr = di_buf->nr_canvas_idx;
	di_buf->vframe->canvas1Addr = di_buf->nr_canvas_idx;
	s4dw_mem_resize_buf(pch, di_buf); //move from before
	//if (di_buf->vframe->width == 3840 && di_buf->vframe->height == 2160)
	if (ppre->sgn_lv == EDI_SGN_4K)
		di_buf->is_4k = 1;
	else
		di_buf->is_4k = 0;

	ppre->is_bypass_mem = 1; //for copy mode
	/* set vframe bit info */
	di_buf->vframe->bitdepth &= ~(BITDEPTH_YMASK);
	di_buf->vframe->bitdepth &= ~(FULL_PACK_422_MODE);
	/* no need pps auto */
	if (ppre->used_pps) {
		di_buf->vframe->width = ppre->afbc_skip_w;
		di_buf->vframe->height = ppre->afbc_skip_h;
		ppre->width_bk = ppre->afbc_skip_w;
		{
			di_buf->vframe->width = ppre->pps_dstw;
			di_buf->vframe->height = ppre->pps_dsth;
			ppre->width_bk = ppre->pps_dstw;
		}
	} else if (de_devp->pps_enable && dimp_get(edi_mp_pps_position)) {
		if (dimp_get(edi_mp_pps_dstw) != di_buf->vframe->width) {
			di_buf->vframe->width = dimp_get(edi_mp_pps_dstw);
			ppre->width_bk = dimp_get(edi_mp_pps_dstw);
		}
		if (dimp_get(edi_mp_pps_dsth) != di_buf->vframe->height)
			di_buf->vframe->height = dimp_get(edi_mp_pps_dsth);
	} else if (de_devp->h_sc_down_en) {
		if (di_mp_uit_get(edi_mp_pre_hsc_down_width)
			!= di_buf->vframe->width) {
			pr_info("di: hscd %d to %d\n", di_buf->vframe->width,
				di_mp_uit_get(edi_mp_pre_hsc_down_width));
			di_buf->vframe->width =
				di_mp_uit_get(edi_mp_pre_hsc_down_width);
			/*di_pre_stru.width_bk = pre_hsc_down_width;*/
			ppre->width_bk =
				di_mp_uit_get(edi_mp_pre_hsc_down_width);
		}
	}
	if (dimp_get(edi_mp_di_force_bit_mode) == 10) {
		di_buf->vframe->bitdepth |= (BITDEPTH_Y10);
		if (dimp_get(edi_mp_full_422_pack))
			di_buf->vframe->bitdepth |= (FULL_PACK_422_MODE);
	} else {
		di_buf->vframe->bitdepth |= (BITDEPTH_Y8);
	}
	di_buf->width_bk = ppre->width_bk;	/*ary:2019-04-23*/
	di_buf->field_count = ppre->field_count_for_cont;
	if (ppre->prog_proc_type) {
		if (ppre->prog_proc_type != 0x10 ||
		    (ppre->prog_proc_type == 0x10 && !nv21_flg))
			di_buf->vframe->type = VIDTYPE_PROGRESSIVE |
				       VIDTYPE_VIU_422 |
				       VIDTYPE_VIU_SINGLE_PLANE |
				       VIDTYPE_VIU_FIELD;
		if (ppre->cur_inp_type & VIDTYPE_PRE_INTERLACE)
			di_buf->vframe->type |= VIDTYPE_PRE_INTERLACE;

		if (pch->ponly && dip_is_ponly_sct_mem(pch)) {
			if (dim_afds() && dim_afds()->cnt_sgn_mode) {
				if (IS_COMP_MODE(ppre->di_inp_buf->vframe->type)) {
					di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode
						(AFBC_SGN_P_H265);
				} else {
					di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode(AFBC_SGN_P);
				}
			}
		} else if (ppre->prog_proc_type == 0x10 &&
		    (nv21_flg				||
		     (cfggch(pch, POUT_FMT) == 0)	||
		     (((cfggch(pch, POUT_FMT) == 4)	||
		      (cfggch(pch, POUT_FMT) == 5)	||
		      (cfggch(pch, POUT_FMT) == 6)	||
		      (cfggch(pch, POUT_FMT) == 7)) &&
		     (ppre->sgn_lv <= EDI_SGN_HD)))) {
			if (dim_afds() && dim_afds()->cnt_sgn_mode) {
				typetmp = ppre->di_inp_buf->vframe->type;
				if (IS_COMP_MODE(typetmp)) {
					di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode
						(AFBC_SGN_P_H265_AS_P);
					//for copy not need afbcd mem
					//di_buf->afbc_sgn_cfg =
					//	dim_afds()->cnt_sgn_mode
					//	(AFBC_SGN_H265_SINP);
				} else {
					di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode
						(AFBC_SGN_BYPSS);
				}
			}
		} else {
			if (dim_afds() && dim_afds()->cnt_sgn_mode) {
				typetmp = ppre->di_inp_buf->vframe->type;
				if (IS_COMP_MODE(typetmp)) {
					di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode
						(AFBC_SGN_P_H265);
				} else {
					di_buf->afbc_sgn_cfg =
					dim_afds()->cnt_sgn_mode(AFBC_SGN_P);
				}
			}
		}
		if (nv21_flg && ppre->prog_proc_type == 0x10) {
			dim_print("cfg wr buf as nv21\n");
				if (cfggch(pch, POUT_FMT) == 1)
					tmpmode = EDPST_OUT_MODE_NV21;
				else
					tmpmode = EDPST_OUT_MODE_NV12;
			s4_dw_dimpst_fill_outvf(di_buf->vframe,
				  di_buf, tmpmode);
		}
	} else {
		//for interlace
	}

	//need check: dimp_set(edi_mp_bypass_post_state, 0);

	ppre->madi_enable = 0;
	ppre->mcdi_enable = 0;
	di_buf->post_proc_flag = 0;
	dimh_patch_post_update_mc_sw(DI_MC_SW_OTHER, false);

	if (ppre->di_mem_buf_dup_p == ppre->di_wr_buf ||
	    ppre->di_chan2_buf_dup_p == ppre->di_wr_buf) {
		PR_ERR("+++++++++++++++++++++++\n");

		return 24;
	}
	dim_dbg_vf_cvs(pch, ppre->di_wr_buf->vframe, 1);

	//need check di_load_pq_table();
	//need check:
	ppre->combing_fix_en = false;//di_mpr(combing_fix_en);

	return 0; /*ok*/
}

static void s4dw_dbg_reg(void)
{
#ifdef DBG_BUFFER_FLOW
	int i;
	struct reg_t *preg;

	if (!dbg_flow())
		return;
	preg = &get_datal()->s4dw_reg[0];
	for (i = 0; i < DIM_S4DW_REG_BACK_NUB; i++) {
		if (!(preg + i)->add)
			break;
		PR_INF("special:0x%x=0x%x\n",
			(preg + i)->add, (preg + i)->df_val);
	}
#endif /*DBG_BUFFER_FLOW*/
}

static void s4dw_reg_special_set(bool set)
{
	int i;
	struct reg_t *preg;

	preg = &get_datal()->s4dw_reg[0];

	if (set) {
		//save old data and set new value;
		i = 0;
		(preg + i)->add = DNR_CTRL;
		(preg + i)->df_val = RD(DNR_CTRL);
		DIM_DI_WR(DNR_CTRL, 0x0);
		i++;
		(preg + i)->add = NR4_TOP_CTRL;
		(preg + i)->df_val = RD(NR4_TOP_CTRL);
		DIM_DI_WR(NR4_TOP_CTRL, 0x3e03c);
		i++;
		for (; i < DIM_S4DW_REG_BACK_NUB; i++)
			(preg + i)->add = 0;

		s4dw_dbg_reg();
		return;
	}

	/* reset: set old data */
	for (i = 0; i < DIM_S4DW_REG_BACK_NUB; i++) {
		if (!(preg + i)->add)
			break;
		DIM_DI_WR((preg + i)->add, (preg + i)->df_val);
	}
}

void s4dw_hpre_check_pps(void)
{
	struct di_hpre_s  *pre = get_hw_pre();
	struct di_pre_stru_s *ppre;

	ppre = pre->pres;
	if (pre->used_pps && !ppre->used_pps) {
		dim_pps_disable();
		dim_print("pps->no pps\n");
		pre->used_pps = false;
	} else if (!pre->used_pps && ppre->used_pps) {
		ppre->input_size_change_flag = true;
		pre->used_pps = true;
		dim_print("no pps->pps\n");
	}
}

static void s4dw_pre_set(unsigned int channel)
{
	//ulong irq_flag2 = 0;
	unsigned short pre_width = 0, pre_height = 0, t5d_cnt;
	unsigned char chan2_field_num = 1;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	int canvases_idex = ppre->field_count_for_cont % 2;
	unsigned short cur_inp_field_type = VIDTYPE_TYPEMASK;
	struct di_cvs_s *cvss;
	struct vframe_s *vf_i, *vf_mem, *vf_chan2;
	union hw_sc2_ctr_pre_s *sc2_pre_cfg;
	struct di_ch_s *pch;
	u32	cvs_nv21[2];
	bool  mc_en = false;//dimp_get(edi_mp_mcpre_en)

	vf_i	= NULL;
	vf_mem	= NULL;
	vf_chan2 = NULL;
#ifdef TMP_S4DW_MC_EN
	mc_en = dimp_get(edi_mp_mcpre_en) ? true : false;
#endif
	pch = get_chdata(channel);
	ppre->pre_de_process_flag = 1;
	dim_ddbg_mod_save(EDI_DBG_MOD_PRE_SETB, channel, ppre->in_seq);/*dbg*/
	cvss = &get_datal()->cvs;

	if (IS_ERR_OR_NULL(ppre->di_inp_buf))
		return;

	#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	pre_inp_canvas_config(ppre->di_inp_buf->vframe);
	#endif

	pre_inp_mif_w(&ppre->di_inp_mif, ppre->di_inp_buf->vframe);
	if (DIM_IS_IC_EF(SC2))
		opl1()->pre_cfg_mif(&ppre->di_inp_mif,
				    DI_MIF0_ID_INP,
				    ppre->di_inp_buf,
				    channel);
	else
		config_di_mif(&ppre->di_inp_mif, ppre->di_inp_buf, channel);
	/* pr_dbg("set_separate_en=%d vframe->type %d\n",
	 * di_pre_stru.di_inp_mif.set_separate_en,
	 * di_pre_stru.di_inp_buf->vframe->type);
	 */

	if (IS_ERR_OR_NULL(ppre->di_mem_buf_dup_p))
		return;

	dim_secure_sw_pre(channel);
#ifdef S4D_OLD_SETTING_KEEP
	if (ppre->di_mem_buf_dup_p	&&
	    ppre->di_mem_buf_dup_p != ppre->di_inp_buf) {
		if (ppre->di_mem_buf_dup_p->flg_nv21) {
			cvs_nv21[0] = cvss->post_idx[1][3];
			cvs_nv21[1] = cvss->post_idx[1][4];
			ppre->di_mem_buf_dup_p->vframe->canvas0Addr = ((u32)-1);
			dim_canvas_set2(ppre->di_mem_buf_dup_p->vframe,
					&cvs_nv21[0]);
			dim_print("mem:vfm:pnub[%d]\n",
				  ppre->di_mem_buf_dup_p->vframe->plane_num);
			//dim_print("mem:vfm:w[%d]\n",
				    //ppre->di_mem_buf_dup_p->vframe->
				    //canvas0_config[0].width);
			//ppre->di_mem_mif.canvas_num =
				//re->di_mem_buf_dup_p->vframe->canvas0Addr;
		} else {
			config_canvas_idx(ppre->di_mem_buf_dup_p,
					  cvss->pre_idx[canvases_idex][0], -1);
		}
		dim_print("mem:flg_nv21[%d]:flg_[%d] %px\n",
			  ppre->di_mem_buf_dup_p->flg_nv21,
			  ppre->di_mem_buf_dup_p->flg_nr,
			  ppre->di_mem_buf_dup_p);
		config_cnt_canvas_idx(ppre->di_mem_buf_dup_p,
				      cvss->pre_idx[canvases_idex][1]);
		if (DIM_IS_IC_EF(T7)) {
			ppre->di_contp2rd_mif.addr =
				ppre->di_mem_buf_dup_p->cnt_adr;
			dbg_ic("contp2rd:0x%lx\n", ppre->di_contp2rd_mif.addr);
		}
	} else {
		config_cnt_canvas_idx(ppre->di_wr_buf,
				      cvss->pre_idx[canvases_idex][1]);
		config_di_cnt_mif(&ppre->di_contp2rd_mif,
			  ppre->di_wr_buf);
	}
	if (ppre->di_chan2_buf_dup_p) {
		config_canvas_idx(ppre->di_chan2_buf_dup_p,
				  cvss->pre_idx[canvases_idex][2], -1);
		config_cnt_canvas_idx(ppre->di_chan2_buf_dup_p,
				      cvss->pre_idx[canvases_idex][3]);
	} else {
		config_cnt_canvas_idx(ppre->di_wr_buf,
				      cvss->pre_idx[canvases_idex][3]);
	}
#endif /* S4D_OLD_SETTING_KEEP */
	ppre->di_nrwr_mif.is_dw = 0;
	if (ppre->di_wr_buf->flg_nv21) {
		//cvss = &get_datal()->cvs;
		//0925	cvs_nv21[0] = cvss->post_idx[1][1];
		cvs_nv21[0] = cvss->pre_idx[canvases_idex][4];//0925
		cvs_nv21[1] = cvss->post_idx[1][2];
		ppre->di_wr_buf->vframe->canvas0Addr = ((u32)-1);
		dim_dbg_vf_cvs(pch, ppre->di_wr_buf->vframe, 5);
		dim_canvas_set2(ppre->di_wr_buf->vframe, &cvs_nv21[0]);
#ifdef S4D_OLD_SETTING_KEEP
		config_canvas_idx_mtn(ppre->di_wr_buf,
				      cvss->pre_idx[canvases_idex][5]);
#endif /* S4D_OLD_SETTING_KEEP */
		//ppost->di_diwr_mif.canvas_num = pst->vf_post.canvas0Addr;
		ppre->di_nrwr_mif.canvas_num =
			ppre->di_wr_buf->vframe->canvas0Addr;
		ppre->di_wr_buf->nr_canvas_idx =
			ppre->di_wr_buf->vframe->canvas0Addr;
		if (ppre->di_wr_buf->flg_nv21 == 1)
			ppre->di_nrwr_mif.cbcr_swap = 0;
		else
			ppre->di_nrwr_mif.cbcr_swap = 1;
		if (dip_itf_is_o_linear(pch)) {
			ppre->di_nrwr_mif.reg_swap = 0;
			ppre->di_nrwr_mif.l_endian = 1;

		} else {
			ppre->di_nrwr_mif.reg_swap = 1;
			ppre->di_nrwr_mif.l_endian = 0;
		}
		if (cfgg(LINEAR)) {
			ppre->di_nrwr_mif.linear = 1;
			ppre->di_nrwr_mif.addr =
				ppre->di_wr_buf->vframe->canvas0_config[0].phy_addr;
			ppre->di_nrwr_mif.addr1 =
				ppre->di_wr_buf->vframe->canvas0_config[1].phy_addr;
		}
	} else  if (ppre->shrk_cfg.shrk_en) {
		ppre->di_nrwr_mif.reg_swap = 1;
		ppre->di_nrwr_mif.cbcr_swap = 0;
		ppre->di_nrwr_mif.l_endian = 0;
		ppre->di_nrwr_mif.is_dw = 1;
		if (cfgg(LINEAR)) {
			ppre->di_nrwr_mif.linear = 1;
			ppre->di_nrwr_mif.addr =
				ppre->dw_wr_dvfm.vfs.canvas0_config[0].phy_addr;
			ppre->di_nrwr_mif.addr1 =
				ppre->dw_wr_dvfm.vfs.canvas0_config[1].phy_addr;
		}

		dim_dvf_config_canvas(&ppre->dw_wr_dvfm);
		//ppre->di_wr_buf->vframe->canvas0Addr = cvs_nv21[0];
		//ppre->di_wr_buf->vframe->canvas1Addr = cvs_nv21[0]; /*?*/
		//dim_print("wr:%px\n", ppre->di_wr_buf);
	} else {
		ppre->di_nrwr_mif.reg_swap = 1;
		ppre->di_nrwr_mif.cbcr_swap = 0;
		ppre->di_nrwr_mif.l_endian = 0;
		if (cfgg(LINEAR)) {
			ppre->di_nrwr_mif.linear = 1;
			ppre->di_nrwr_mif.addr = ppre->di_wr_buf->nr_adr;
		}
		config_canvas_idx(ppre->di_wr_buf,
				  cvss->pre_idx[canvases_idex][4],
				  cvss->pre_idx[canvases_idex][5]);
	}
#ifdef S4D_OLD_SETTING_KEEP
	config_cnt_canvas_idx(ppre->di_wr_buf,
			      cvss->pre_idx[canvases_idex][6]);
#endif /* S4D_OLD_SETTING_KEEP */
#ifdef TMP_S4DW_MC_EN
	if (dimp_get(edi_mp_mcpre_en)) {
		if (ppre->di_chan2_buf_dup_p)
			config_mcinfo_canvas_idx
				(ppre->di_chan2_buf_dup_p,
				 cvss->pre_idx[canvases_idex][7]);
		else
			config_mcinfo_canvas_idx
				(ppre->di_wr_buf,
				 cvss->pre_idx[canvases_idex][7]);

		config_mcinfo_canvas_idx(ppre->di_wr_buf,
					 cvss->pre_idx[canvases_idex][8]);
		config_mcvec_canvas_idx(ppre->di_wr_buf,
					cvss->pre_idx[canvases_idex][9]);
	}
#endif /* TMP_S4DW_MC_EN */
	if (DIM_IS_IC_EF(SC2))
		opl1()->pre_cfg_mif(&ppre->di_mem_mif,
				    DI_MIF0_ID_MEM,
				    ppre->di_mem_buf_dup_p,
				    channel);
	else
		config_di_mif(&ppre->di_mem_mif,
			      ppre->di_mem_buf_dup_p, channel);
	/* patch */
	if (ppre->di_wr_buf->flg_nv21	&&
	    ppre->di_mem_buf_dup_p	&&
	    ppre->di_mem_buf_dup_p != ppre->di_inp_buf) {
		ppre->di_mem_mif.l_endian = ppre->di_nrwr_mif.l_endian;
		ppre->di_mem_mif.cbcr_swap = ppre->di_nrwr_mif.cbcr_swap;
		ppre->di_mem_mif.reg_swap = ppre->di_nrwr_mif.reg_swap;
	}
	if (!ppre->di_chan2_buf_dup_p) {
		if (DIM_IS_IC_EF(SC2))
			opl1()->pre_cfg_mif(&ppre->di_chan2_mif,
					    DI_MIF0_ID_CHAN2,
					    ppre->di_inp_buf,
					    channel);
		else
			config_di_mif(&ppre->di_chan2_mif,
				      ppre->di_inp_buf, channel);

	} else {
		if (DIM_IS_IC_EF(SC2))
			opl1()->pre_cfg_mif(&ppre->di_chan2_mif,
					    DI_MIF0_ID_CHAN2,
					    ppre->di_chan2_buf_dup_p,
					    channel);
		else
			config_di_mif(&ppre->di_chan2_mif,
				      ppre->di_chan2_buf_dup_p, channel);
	}
	if (ppre->prog_proc_type == 0) {
		ppre->di_nrwr_mif.src_i = 1;
		ppre->di_mtnwr_mif.src_i = 1;
	} else {
		ppre->di_nrwr_mif.src_i = 0;
		ppre->di_mtnwr_mif.src_i = 0;
	}
	if (DIM_IS_IC_EF(SC2)) {
		if (ppre->shrk_cfg.shrk_en) {
			//dw_fill_outvf_pre(&ppre->vf_copy, ppre->di_wr_buf);
			//ppre->vfm_cpy.private_data = ppre->di_wr_buf;
			opl1()->wr_cfg_mif_dvfm(&ppre->di_nrwr_mif,
					   EDI_MIFSM_NR,
					   &ppre->dw_wr_dvfm, NULL);
		} else {
			opl1()->wr_cfg_mif(&ppre->di_nrwr_mif,
					   EDI_MIFSM_NR,
					   ppre->di_wr_buf, NULL);
		}
	} else {
		config_di_wr_mif(&ppre->di_nrwr_mif, &ppre->di_mtnwr_mif,
				 ppre->di_wr_buf, channel);
	}

	if (DIM_IS_IC_EF(SC2))
		config_di_mtnwr_mif(&ppre->di_mtnwr_mif, ppre->di_wr_buf);

	if (ppre->di_chan2_buf_dup_p)
		config_di_cnt_mif(&ppre->di_contprd_mif,
				  ppre->di_chan2_buf_dup_p);
	else
		config_di_cnt_mif(&ppre->di_contprd_mif,
				  ppre->di_wr_buf);

	config_di_cnt_mif(&ppre->di_contwr_mif, ppre->di_wr_buf);

#ifdef TMP_S4DW_MC_EN
	if (dimp_get(edi_mp_mcpre_en)) {
		if (ppre->di_chan2_buf_dup_p)
			config_di_mcinford_mif(&ppre->di_mcinford_mif,
					       ppre->di_chan2_buf_dup_p);
		else
			config_di_mcinford_mif(&ppre->di_mcinford_mif,
					       ppre->di_wr_buf);

		config_di_pre_mc_mif(&ppre->di_mcinfowr_mif,
				     &ppre->di_mcvecwr_mif, ppre->di_wr_buf);
	}
#endif
	if (ppre->di_chan2_buf_dup_p &&
	    ((ppre->di_chan2_buf_dup_p->vframe->type & VIDTYPE_TYPEMASK)
	     == VIDTYPE_INTERLACE_TOP))
		chan2_field_num = 0;

	if (ppre->shrk_cfg.shrk_en) {
		pre_width = ppre->di_wr_buf->vframe->width;
		pre_height = ppre->di_wr_buf->vframe->height;
	} else {
		pre_width = ppre->di_nrwr_mif.end_x + 1;
		pre_height = ppre->di_nrwr_mif.end_y + 1;
	}
	if (IS_ERR_OR_NULL(ppre->di_inp_buf))
		return;
	s4dw_hpre_check_pps(); // for add s4cp

	if (/*ppre->di_inp_buf && */ppre->di_inp_buf->vframe)
		vf_i = ppre->di_inp_buf->vframe;

	if (dim_afds() && dim_afds()->pre_check)
		dim_afds()->pre_check(ppre->di_wr_buf->vframe, pch);
	if (ppre->di_wr_buf->is_4k) {
		ppre->pre_top_cfg.b.is_inp_4k = 1;
		ppre->pre_top_cfg.b.is_mem_4k = 0;
	}
	if (ppre->input_size_change_flag || dbg_change_each_vs()) {
		if (dim_afds() && dim_afds()->rest_val)
			dim_afds()->rest_val();
		cur_inp_field_type =
		(ppre->di_inp_buf->vframe->type & VIDTYPE_TYPEMASK);
		cur_inp_field_type =
	ppre->cur_prog_flag ? VIDTYPE_PROGRESSIVE : cur_inp_field_type;
		/*di_async_reset2();*/
		di_pre_size_change(pre_width, pre_height,
				   cur_inp_field_type, channel);
		ppre->input_size_change_flag = false;
	}

	if (DIM_IS_IC_EF(SC2)) {
		sc2_pre_cfg = &get_hw_pre()->pre_top_cfg;
		if (sc2_pre_cfg->d32 != ppre->pre_top_cfg.d32) {
			sc2_pre_cfg->d32 = ppre->pre_top_cfg.d32;
			dim_sc2_contr_pre(sc2_pre_cfg);
			dim_sc2_4k_set(sc2_pre_cfg->b.mode_4k);
		}
	}
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
		dim_nr_ds_hw_ctrl(false);

	#ifndef TMP_MASK_FOR_T7
	/*patch for SECAM signal format from vlsi-feijun for all IC*/
	get_ops_nr()->secam_cfr_adjust(ppre->di_inp_buf->vframe->sig_fmt,
				       ppre->di_inp_buf->vframe->type);
	#endif
	/* set interrupt mask for pre module.
	 * we need to only leave one mask open
	 * to prevent multiple entry for dim_irq
	 */

	if (ppre->is_bypass_mem)
		ppre->di_wr_buf->is_bypass_mem = 1;
	else
		ppre->di_wr_buf->is_bypass_mem = 0;

	/* for t5d vb netflix change afbc input timeout */
	if (DIM_IS_IC(T5DB)) {
		t5d_cnt = cfgg(T5DB_P_NOTNR_THD);
		if (t5d_cnt < 2)
			t5d_cnt = 2;
		if (ppre->field_count_for_cont < t5d_cnt &&
		    ppre->cur_prog_flag)
			ppre->is_bypass_mem |= 0x02;
		else
			ppre->is_bypass_mem &= ~0x02;

		if (ppre->is_bypass_mem)
			ppre->di_wr_buf->is_bypass_mem = 1;
		else
			ppre->di_wr_buf->is_bypass_mem = 0;
		dim_print("%s:is_bypass_mem[%d]\n", __func__,
			  ppre->di_wr_buf->is_bypass_mem);
		if (ppre->field_count_for_cont < 1 &&
		    IS_FIELD_I_SRC(ppre->cur_inp_type))
			ppre->is_disable_chan2 = 1;
		else
			ppre->is_disable_chan2 = 0;
		//when p mode/frist frame ,set 0 to reset the mc vec wr,
		//second frame write back to 1,from vlsi feijun.fan for DMC bug

		if (ppre->field_count_for_cont < 1 &&
		    IS_PROG(ppre->cur_inp_type)) {
			dimh_set_slv_mcvec(0);
		} else {
			if (!dimh_get_slv_mcvec())
				dimh_set_slv_mcvec(1);
		}
	}
	if (dim_config_crc_ic()) //add for crc @2k22-0102
		dimh_set_crc_init(ppre->field_count_for_cont);

	if (IS_ERR_OR_NULL(ppre->di_wr_buf))
		return;
	if (dim_hdr_ops() && ppre->di_wr_buf->c.en_hdr)
		dim_hdr_ops()->set(1);
	//dbg:
	if (cfgg(LINEAR))
		dim_dbg_buffer_flow(pch,
				    ppre->di_nrwr_mif.addr,
				    ppre->di_nrwr_mif.addr1, 10);
	dim_dbg_vf_cvs(pch, ppre->di_wr_buf->vframe, 6);

	di_lock_irq();//di_lock_irqfiq_save(irq_flag2);
	if (IS_IC_SUPPORT(DW)) { /* dw */
		opl1()->shrk_set(&ppre->shrk_cfg, &di_pre_regset);
	}
	dimh_enable_di_pre_aml(&ppre->di_inp_mif,
			       &ppre->di_mem_mif,
			       &ppre->di_chan2_mif,
			       &ppre->di_nrwr_mif,
			       &ppre->di_mtnwr_mif,
			       &ppre->di_contp2rd_mif,
			       &ppre->di_contprd_mif,
			       &ppre->di_contwr_mif,
			       ppre->madi_enable,
			       chan2_field_num,
			       ppre->vdin2nr |
			       (ppre->is_bypass_mem << 4),
			       ppre);

	//no need for s4dw dcntr_set();

	if (dim_afds()) {
		if (ppre->di_mem_buf_dup_p && ppre->di_mem_buf_dup_p->vframe)
			vf_mem = ppre->di_mem_buf_dup_p->vframe;
		if (ppre->di_chan2_buf_dup_p &&
		    ppre->di_chan2_buf_dup_p->vframe)
			vf_chan2 = ppre->di_chan2_buf_dup_p->vframe;
		//use di_pulldown transfer s4cp infor
		vf_i->di_pulldown = 1;
		dbg_copy("set:0x%px, pd %d\n", vf_i, vf_i->di_pulldown);
		if (vf_mem && vf_mem != vf_i)
			vf_mem->di_pulldown = 0;
		if (vf_chan2 && vf_chan2 != vf_i)
			vf_chan2->di_pulldown = 0;

		if (/*ppre->di_wr_buf && */ppre->di_wr_buf->vframe)
			dim_afds()->en_pre_set(vf_i,
					       vf_mem,
					       vf_chan2,
					       ppre->di_wr_buf->vframe);
		vf_i->di_pulldown = 0;
	}
#ifdef TMP_S4DW_MC_EN
	if (dimp_get(edi_mp_mcpre_en)) {
		if (DIM_IS_IC_EF(T7))
			opl1()->pre_enable_mc(&ppre->di_mcinford_mif,
						  &ppre->di_mcinfowr_mif,
						  &ppre->di_mcvecwr_mif,
						  ppre->mcdi_enable);
		else if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
			dimh_enable_mc_di_pre_g12(&ppre->di_mcinford_mif,
						  &ppre->di_mcinfowr_mif,
						  &ppre->di_mcvecwr_mif,
						  ppre->mcdi_enable);
		else
			dimh_enable_mc_di_pre(&ppre->di_mcinford_mif,
					      &ppre->di_mcinfowr_mif,
					      &ppre->di_mcvecwr_mif,
					      ppre->mcdi_enable);
	}
#endif
	s4dw_reg_special_set(true);
	ppre->field_count_for_cont++;

	if (ppre->field_count_for_cont >= 5)
		DIM_DI_WR_REG_BITS(DI_MTN_CTRL, 0, 30, 1);

	dimh_txl_patch_prog(ppre->cur_prog_flag,
			    ppre->field_count_for_cont,
			    /*dimp_get(edi_mp_mcpre_en)*/mc_en);

	/* must make sure follow part issue without interrupts,
	 * otherwise may cause watch dog reboot
	 */
	//di_lock_irqfiq_save(irq_flag2);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		/* enable mc pre mif*/
		dimh_enable_di_pre_mif(true, /*dimp_get(edi_mp_mcpre_en)*/mc_en);
		dim_pre_frame_reset_g12(ppre->madi_enable,
					ppre->mcdi_enable);
	} else {
		dim_pre_frame_reset();
		/* enable mc pre mif*/
		dimh_enable_di_pre_mif(true, /*dimp_get(edi_mp_mcpre_en)*/mc_en);
	}
	/*dbg_set_DI_PRE_CTRL();*/
	atomic_set(&get_hw_pre()->flg_wait_int, 1);
	ppre->pre_de_busy = 1;
	ppre->irq_time[0] = cur_to_usecs();
	ppre->irq_time[1] = ppre->irq_time[0];
	di_unlock_irq();//di_unlock_irqfiq_restore(irq_flag2);
	/*reinit pre busy flag*/
	pch->sum_pre++;
	dim_dbg_pre_cnt(channel, "s3");
//	ppre->irq_time[0] = cur_to_msecs();
//	ppre->irq_time[1] = cur_to_msecs();
	dim_ddbg_mod_save(EDI_DBG_MOD_PRE_SETE, channel, ppre->in_seq);/*dbg*/
	dim_tr_ops.pre_set(ppre->di_wr_buf->vframe->index_disp);

	ppre->pre_de_process_flag = 0;
}

static void dbg_display_buffersize(struct di_ch_s *pch, struct vframe_s *vfm)
{
	struct div2_mm_s *mm;

	if (!dip_cfg_afbc_dbg_display_buffer())
		return;
	mm = dim_mm_get(pch->ch_id);
	vfm->width = mm->cfg.di_w;
	vfm->height = mm->cfg.di_h;
}

static void s4dw_done_config(unsigned int channel, bool flg_timeout)
{
	struct di_buf_s *post_wr_buf = NULL;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct di_ch_s *pch;
	struct vframe_s *vfmt;

	dim_dbg_pre_cnt(channel, "d1");
	dim_ddbg_mod_save(EDI_DBG_MOD_PRE_DONEB, channel, ppre->in_seq);/*dbg*/
	pch = get_chdata(channel);
	s4dw_reg_special_set(false);
	if (!ppre->di_wr_buf || !ppre->di_inp_buf) {
		PR_ERR("%s:no key buffer? 0x%px,0x%px\n",
		       __func__, ppre->di_wr_buf, ppre->di_inp_buf);
		return;
	}

	if (flg_timeout) {
		if (DIM_IS_IC_EF(SC2))
			opl1()->pre_gl_sw(false);
		else
			hpre_gl_sw(false);
		//for cp disable ppre->timeout_check = true;
	}
	if (ppre->di_wr_buf->field_count < 3) {
		if (!ppre->di_wr_buf->field_count)
			dbg_timer(channel, EDBG_TIMER_1_PREADY);
		else if (ppre->di_wr_buf->field_count == 1)
			dbg_timer(channel, EDBG_TIMER_2_PREADY);
		else if (ppre->di_wr_buf->field_count == 2)
			dbg_timer(channel, EDBG_TIMER_3_PREADY);
	}

	dim_tr_ops.pre_ready(ppre->di_wr_buf->vframe->index_disp);
	ppre->di_wr_buf->flg_nr = 1;
	if (ppre->pre_throw_flag > 0) {
		ppre->di_wr_buf->throw_flag = 1;
		ppre->pre_throw_flag--;
	} else {
		ppre->di_wr_buf->throw_flag = 0;
	}

	if (ppre->di_wr_buf->flg_afbce_set) {
		ppre->di_wr_buf->flg_afbce_set = 0;
		PR_ERR("%s:afbce?\n", __func__);
	}

	/*dec vf keep*/
	if (ppre->di_inp_buf &&
	    ppre->di_inp_buf->dec_vf_state & DI_BIT0) {
		ppre->di_wr_buf->in_buf = ppre->di_inp_buf;
		dim_print("dim:dec vf:l[%d]\n",
			  ppre->di_wr_buf->in_buf->vframe->index_disp);
	}
	//ppre->di_post_wr_buf not use

	post_wr_buf = ppre->di_wr_buf;

	post_wr_buf->vframe->di_pulldown = 0;
	post_wr_buf->vframe->di_gmv = 0;
	post_wr_buf->vframe->di_cm_cnt = 0;

	if (ppre->prog_proc_type == 0x10) {
		ppre->di_mem_buf_dup_p->pre_ref_count = 0;
		/*recycle the progress throw buffer*/
		ppre->di_wr_buf->seq = ppre->pre_ready_seq++;
		ppre->di_wr_buf->post_ref_count = 0;

		if (ppre->di_wr_buf->throw_flag) {
			ppre->di_wr_buf->pre_ref_count = 0;
			ppre->di_mem_buf_dup_p = NULL;
			ppre->di_post_wr_buf = NULL;
			ppre->di_wr_buf = NULL;

		} else if (ppre->di_wr_buf->is_bypass_mem) {
			//di_que_in(channel, QUE_PRE_READY,
			//	  ppre->di_wr_buf);
			/* set flag */
			vfmt = ppre->di_wr_buf->vframe;
			/* flag clear */
			vfmt->flag &= VFMT_FLG_CHG_MASK;

			if (!dip_itf_is_o_linear(pch))
				vfmt->flag &= (~VFRAME_FLAG_VIDEO_LINEAR);
			vfmt->canvas0Addr = (u32)-1;
			vfmt->canvas1Addr = (u32)-1;
#ifdef TMP_FOR_S4DW	/*temp*/
			if (s4dw_test_ins()) {
				vfmt->early_process_fun = dim_do_post_wr_fun;
				vfmt->process_fun = NULL;
				vfmt->mem_handle = NULL;
				vfmt->type |= VIDTYPE_DI_PW;

				if (dip_itf_is_vfm(pch))
					vfmt->flag |= VFRAME_FLAG_DI_PW_VFM;
				else if (dip_itf_is_ins_lbuf(pch))
					vfmt->flag |= VFRAME_FLAG_DI_PW_N_LOCAL;
				else
					vfmt->flag |= VFRAME_FLAG_DI_PW_N_EXT;
			}
#endif
			dbg_display_buffersize(pch, vfmt);
			pch->itf.op_fill_ready(pch, ppre->di_wr_buf);
			ppre->di_mem_buf_dup_p = NULL;
			ppre->di_post_wr_buf = NULL;
			ppre->di_wr_buf = NULL;
			dim_print("mem bypass\n");
		} else {
			/* clear mem ref */
			PR_ERR("not bypass mem?\n");
		}

		/******************************************/
		/* need check */
		if (ppre->source_change_flag) {
			//ppre->di_wr_buf->new_format_flag = 1;
			ppre->source_change_flag = 0;
		} else {
			//ppre->di_wr_buf->new_format_flag = 0;
		}
	}  else {
		PR_INF("%s: err2\n", __func__);
	}

	if (!(ppre->di_inp_buf->dec_vf_state & DI_BIT0)) {
		/*dec vf keep*/
		queue_in(channel, ppre->di_inp_buf,
			 QUEUE_RECYCLE);
		ppre->di_inp_buf = NULL;
	}
	dim_ddbg_mod_save(EDI_DBG_MOD_PRE_DONEE, channel, ppre->in_seq);/*dbg*/
	dim_dbg_pre_cnt(channel, "d2");
}

/**************************************************************/
//s4dw pre setting table :
/* use do_table: */
/* bypass or setting */
static unsigned int s4dw_t_check(void *data)
{
	struct di_hpre_s  *pre = get_hw_pre();
	unsigned int ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
	struct di_ch_s *pch;
	int ret_bypass;

	pch = get_chdata(pre->curr_ch);
	/* @ary bypass flow*/
	ret_bypass = s4dw_pre_bypass(pch);
	dbg_copy("%s:1:%d\n", "s4dw check", ret_bypass);
	if (ret_bypass < 0) { /**/
		/* @ary no nin, to start */
		ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
	} else {
		/* @ary pre config */
		ret_bypass = s4dw_pre_buf_config(pch);
		dbg_copy("%s:2:%d\n", "s4dw check", ret_bypass);
		if (!ret_bypass) {
			/* pre set */
			s4dw_pre_set(pre->curr_ch);
			pre->irq_nr = false;
			di_tout_contr(EDI_TOUT_CONTR_EN, &pre->tout);
			ret = K_DO_R_FINISH;
		} else {
			/* @ary bypass to start */
			ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);
		}
	}

	return ret;
}

/*************************************
 * return:
 *	false: don't wait. finish seting and go to start;
 *	true: wait
 *************************************/
static bool s4dw_wait_int_process(void)
{
	struct di_hpre_s  *pre = get_hw_pre();
	//tmp ulong flags = 0;
	struct di_pre_stru_s *ppre;
	bool ret = true; //wait

	if (pre->flg_int_done) {
		/*have INT done flg*/

		/*di_pre_wait_irq_set(false);*/
		/*finish to count timer*/
		di_tout_contr(EDI_TOUT_CONTR_FINISH, &pre->tout);
		//tmp spin_lock_irqsave(&plist_lock, flags);

		s4dw_done_config(pre->curr_ch, false);

		pre->flg_int_done = 0;

		dpre_recyc(pre->curr_ch);
		dpre_vdoing(pre->curr_ch);

		//tmp spin_unlock_irqrestore(&plist_lock, flags);

		ppre = get_pre_stru(pre->curr_ch);

		ret = false;

	} else {
		/*check if timeout:*/
		if (di_tout_contr(EDI_TOUT_CONTR_CHECK, &pre->tout)) {
			/* @ary timeout */
			if (!atomic_dec_and_test(&get_hw_pre()->flg_wait_int)) {
				PR_WARN("%s:ch[%d]timeout\n", __func__,
					pre->curr_ch);
				di_tout_contr(EDI_TOUT_CONTR_EN, &pre->tout);
				ret = true;
			} else {
				/*return K_DO_R_FINISH;*/
				/* real timeout */
				hpre_timeout_read();

				if (DIM_IS_IC_EF(SC2))
					opl1()->pre_gl_sw(false);
#ifdef TMP_S4DW_MC_EN
				dimh_enable_di_pre_mif(false, dimp_get(edi_mp_mcpre_en));
#else
				dimh_enable_di_pre_mif(false, false);
#endif
				pre->pres->pre_de_irq_timeout_count++;

				pre->pres->pre_de_busy = 0;
				pre->pres->pre_de_clear_flag = 2;

				PR_WARN("ch[%d]cp %d timeout 0x%x(%d ms)%d\n",
					pre->curr_ch,
					pre->pres->field_count_for_cont,
					RD(DI_INTR_CTRL),
					(unsigned int)((cur_to_usecs() -
					pre->pres->irq_time[0]) >> 10),
					pre->irq_nr);

				/*******************************/
				s4dw_done_config(pre->curr_ch, true);

				dpre_recyc(pre->curr_ch);
				dpre_vdoing(pre->curr_ch);

				ret = false;
			}
		} else {	//wait
			ret = true;
		}
	}
	return  ret;
}

/* wait int or done or reset ? */
static unsigned int s4dw_t_wait_int(void *data)
{
	bool wait;
	unsigned int ret = K_DO_R_NOT_FINISH;

	wait = s4dw_wait_int_process();
	if (wait)
		ret = K_DO_R_NOT_FINISH;
	else
		ret = K_DO_R_JUMP(K_DO_TABLE_ID_STOP);

	return ret;
}

enum EDI_S4DW_MT {
	EDI_S4DW_MT_CHECK = K_DO_TABLE_ID_START,
	//EDI_DCT_MT_SET,
	EDI_S4DW_MT_WAIT_INT,
};

//static
const struct do_table_ops_s s4dw_hw_processt[4] = {
	/*fix*/
	[K_DO_TABLE_ID_PAUSE] = {
	.id = K_DO_TABLE_ID_PAUSE,
	.mark = 0,
	.con = NULL,
	.do_op = NULL,
	.do_stop_op = NULL,
	.name = "pause",
	},
	[K_DO_TABLE_ID_STOP] = {
	.id = K_DO_TABLE_ID_STOP,
	.mark = 0,
	.con = NULL,
	.do_op = NULL,
	.do_stop_op = NULL,
	.name = "stop",
	},
	/******************/
	[K_DO_TABLE_ID_START] = {	/*EDI_PRE_MT_CHECK*/
	.id = K_DO_TABLE_ID_START,
	.mark = 0,
	.con = NULL,
	.do_op = s4dw_t_check,
	.do_stop_op = NULL,
	.name = "s4dw_check",
	},
	[EDI_S4DW_MT_WAIT_INT] = {
	.id = EDI_S4DW_MT_WAIT_INT,
	.mark = 0,
	.con = NULL,
	.do_op = s4dw_t_wait_int,
	.do_stop_op = NULL,
	.name = "s4dw_wait_int",
	}
};

/*s4dw pre setting table end*/
/**************************************************************/

