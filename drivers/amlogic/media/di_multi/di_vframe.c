// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/di_vframe.c
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

#include <linux/semaphore.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>

#include "deinterlace.h"
#include "di_data_l.h"
#include "di_pre.h"
#include "di_prc.h"
#include "di_dbg.h"
#include "di_que.h"

#include "di_vframe.h"
#include "di_task.h"
#include "di_sys.h"

struct dev_vfram_t *get_dev_vframe(unsigned int ch)
{
	if (ch < DI_CHANNEL_NUB)
		return &get_datal()->ch_data[ch].itf.dvfm;

	pr_info("err:%s ch overflow %d\n", __func__, ch);
	return &get_datal()->ch_data[0].itf.dvfm;
}

const char * const di_rev_name[4] = {
	"deinterlace",
	"dimulti.1",
	"dimulti.2",
	"dimulti.3",
};

/**************************************
 * nins_m_recycle
 *	op_back_input
 *	_RECYCLE -> _IDLE
 *	back vfm to dec
 *	run in main
 **************************************/
static void nins_m_recycle(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	int i;
	unsigned int cnt, ch;
	struct dim_nins_s *ins;
	struct vframe_s *vfm;

	if (!pch || pch->itf.etype != EDIM_NIN_TYPE_VFM) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	ch = pch->ch_id;
	pbufq = &pch->nin_qb;

	cnt = qbufp_count(pbufq, QBF_NINS_Q_RECYCLE);

	if (!cnt)
		return;

	for (i = 0; i < cnt; i++) {
		ins = nins_move(pch, QBF_NINS_Q_RECYCLE, QBF_NINS_Q_IDLE);
		vfm = (struct vframe_s *)ins->c.ori;
		if (vfm) {
			pw_vf_put(vfm, ch);
			pw_vf_notify_provider(ch,
					      VFRAME_EVENT_RECEIVER_PUT, NULL);
		}
		memset(&ins->c, 0, sizeof(ins->c));
	}
}

static void nins_m_unreg(struct di_ch_s *pch)
{
	//clear all input
	bufq_nin_reg(pch);
}

/************************************************
 * for clear q,
 * from used to idle,
 * not back to dec
 * ??
 ************************************************/

void nins_used2idle_one(struct di_ch_s *pch, struct dim_nins_s *ins)
{
	if (!pch || pch->itf.etype != EDIM_NIN_TYPE_VFM) {
		PR_ERR("%s:\n", __func__);
		return;
	}
}

/**************************************
 * bufq_ins_in_vf
 *	get vf from pre driver
 *	_IDLE -> _CHECK
 * set ins
 *	*pvf
 *	vf_copy
 **************************************/
#define DIM_K_VFM_IN_DCT_LIMIT	3
static bool nins_m_in_vf(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	unsigned int in_nub, free_nub, dct_nub, dct_nub_tmp;
	int i;
	unsigned int ch;
	struct vframe_s *vf;
	struct dim_nins_s	*pins;
	unsigned int index;
	bool flg_q;
	unsigned int err_cnt = 0;

	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return false;
	}
	ch = pch->ch_id;
	pbufq = &pch->nin_qb;

	in_nub		= qbufp_count(pbufq, QBF_NINS_Q_CHECK);
	free_nub	= qbufp_count(pbufq, QBF_NINS_Q_IDLE);
	dct_nub		= qbufp_count(pbufq, QBF_NINS_Q_DCT);
	dct_nub_tmp = dct_nub;

	if ((in_nub + dct_nub) >= DIM_K_VFM_IN_DCT_LIMIT	||
	    (free_nub < (DIM_K_VFM_IN_DCT_LIMIT - in_nub - dct_nub))) {
		return false;
	}

	for (i = 0; i < (DIM_K_VFM_IN_DCT_LIMIT - in_nub - dct_nub); i++) {
		if (dct_nub_tmp > 0) {
			pins = nins_move(pch, QBF_NINS_Q_DCT, QBF_NINS_Q_CHECK);
			if (pins) {
				dct_nub_tmp--;
				continue;
			} else {
				dct_nub_tmp = 0;
			}
		}
		vf = pw_vf_peek(ch);
		if (!vf)
			break;

		vf = pw_vf_get(ch);
		if (!vf)
			break;
#ifdef DBG_TEST_CREATE
		if (!(vf->type & VIDTYPE_V4L_EOS) && dbg_crt_r_is_sv_in()) {
			dbg_crt_set_vf(vf);
			dbg_crt_do_save_vf(vf);
		}
#endif

		/* get ins */
		flg_q = qbuf_out(pbufq, QBF_NINS_Q_IDLE, &index);
		if (!flg_q) {
			PR_ERR("%s:qout\n", __func__);
			err_cnt++;
			pw_vf_put(vf, ch);
			break;
		}
		pins = (struct dim_nins_s *)pbufq->pbuf[index].qbc;
		pins->c.ori = vf;
		pins->c.cnt = pch->in_cnt;
		pch->in_cnt++;
		//pins->c.etype = EDIM_NIN_TYPE_VFM;
		memcpy(&pins->c.vfm_cp, vf, sizeof(pins->c.vfm_cp));
		flg_q = qbuf_in(pbufq, QBF_NINS_Q_CHECK, index);
		if (!flg_q) {
			PR_ERR("%s:qin\n", __func__);
			err_cnt++;
			pw_vf_put(vf, ch);
			qbuf_in(pbufq, QBF_NINS_Q_IDLE, index);
			break;
		}
		if (pch->in_cnt < 4) {
			if (pch->in_cnt == 1)
				dbg_timer(ch, EDBG_TIMER_1_GET);
			else if (pch->in_cnt == 2)
				dbg_timer(ch, EDBG_TIMER_2_GET);
			else if (pch->in_cnt == 3)
				dbg_timer(ch, EDBG_TIMER_3_GET);
		}
	}

	if (err_cnt)
		return false;
	return true;
}

static bool nins_m_in_vf_dct(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	unsigned int in_nub, free_nub, dct_nub;
	int i;
	unsigned int ch;
	struct vframe_s *vf;
	struct dim_nins_s	*pins;
	unsigned int index;
	bool flg_q;
	unsigned int err_cnt = 0;

	if (!get_datal()->dct_op || !get_datal()->dct_op->is_en(pch))
		return nins_m_in_vf(pch);

	if (!pch) {
		PR_ERR("%s:\n", __func__);
		return false;
	}
	ch = pch->ch_id;
	pbufq = &pch->nin_qb;

	in_nub		= qbufp_count(pbufq, QBF_NINS_Q_CHECK);
	free_nub	= qbufp_count(pbufq, QBF_NINS_Q_IDLE);
	dct_nub		= qbufp_count(pbufq, QBF_NINS_Q_DCT);

	if ((in_nub + dct_nub) >= DIM_K_VFM_IN_DCT_LIMIT	||
	    (free_nub < (DIM_K_VFM_IN_DCT_LIMIT - in_nub - dct_nub))) {
		return false;
	}

	for (i = 0; i < (DIM_K_VFM_IN_DCT_LIMIT - in_nub - dct_nub); i++) {
		vf = pw_vf_peek(ch);
		if (!vf)
			break;

		vf = pw_vf_get(ch);
		if (!vf)
			break;

		/* get ins */
		flg_q = qbuf_out(pbufq, QBF_NINS_Q_IDLE, &index);
		if (!flg_q) {
			PR_ERR("%s:qout\n", __func__);
			err_cnt++;
			pw_vf_put(vf, ch);
			break;
		}
		pins = (struct dim_nins_s *)pbufq->pbuf[index].qbc;
		pins->c.ori = vf;
		pins->c.cnt = pch->in_cnt;
		pch->in_cnt++;
		//pins->c.etype = EDIM_NIN_TYPE_VFM;
		memcpy(&pins->c.vfm_cp, vf, sizeof(pins->c.vfm_cp));
		flg_q = qbuf_in(pbufq, QBF_NINS_Q_DCT, index);
		if (!flg_q) {
			PR_ERR("%s:qin\n", __func__);
			err_cnt++;
			pw_vf_put(vf, ch);
			qbuf_in(pbufq, QBF_NINS_Q_IDLE, index);
			break;
		}
		if (pch->in_cnt < 4) {
			if (pch->in_cnt == 1)
				dbg_timer(ch, EDBG_TIMER_1_GET);
			else if (pch->in_cnt == 2)
				dbg_timer(ch, EDBG_TIMER_2_GET);
			else if (pch->in_cnt == 3)
				dbg_timer(ch, EDBG_TIMER_3_GET);
		}
	}

	if (err_cnt)
		return false;
	return true;
}

static void vfm_m_fill_ready(struct di_ch_s *pch)
{
	pw_vf_notify_receiver(pch->ch_id,
			      VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
}

/*for first frame no need to ready buf*/
/* @ary_note: this only used for vfm */
static bool dim_bypass_first_frame(struct di_ch_s *pch)
{
	struct vframe_s *vframe;
	struct dim_nins_s *nins;
	unsigned int ch;
	//struct di_ch_s *pch;

	ch = pch->ch_id;
	nins = nins_peek(pch);
	if (!nins)
		return false;

	nins = nins_get(pch);
	vframe = nins->c.ori;
	nins->c.ori = NULL;
	nins_used_some_to_recycle(pch, nins);

	ndrd_qin(pch, vframe);

	pw_vf_notify_receiver(ch,
			      VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);

	PR_INF("%s:ok\n", __func__);
	return true;
}

static struct vframe_s *dim_nbypass_get(struct di_ch_s *pch)
{
	struct vframe_s *vframe;
	struct dim_nins_s *nins;

	nins = nins_peek(pch);
	if (!nins)
		return NULL;

	nins = nins_get(pch);
	vframe = nins->c.ori;
	nins->c.ori = NULL;
	nins_used_some_to_recycle(pch, nins);

	pw_vf_notify_receiver(pch->ch_id,
			      VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);

	//PR_INF("%s:ok\n", __func__);
	return vframe;
}

static struct vframe_s *dim_nbypass_peek(struct di_ch_s *pch)
{
	struct vframe_s *vframe;

	vframe = nins_peekvfm_ori(pch);
	return vframe;
}

/* @ary_note: api for release */
void dim_post_keep_cmd_release2_local(struct vframe_s *vframe)
{
	//struct di_buf_s *di_buf;
	struct dim_ndis_s *ndis;

	if (!dil_get_diff_ver_flag())
		return;
	if (!vframe) {
		PR_ERR("%s:no vfm\n", __func__);
		return;
	}

//#ifdef TST_NEW_INS_INTERFACE
	//@ary_note:temp:
	if (dim_dbg_new_int(1)) {
		PR_ERR("%s:extbuff\n", __func__);
		return;
	}
//#endif
	ndis = (struct dim_ndis_s *)vframe->private_data;
	//di_buf = (struct di_buf_s *)vframe->private_data;

	if (!ndis) {
		/* for bypass mode, di no need back vf buffer */
		//PR_WARN("%s:no di_buf\n", __func__);
		/* bypass mode */
		return;
	}

	sum_release_inc(ndis->header.ch);
	dbg_keep("release keep ch[%d],index[%d]\n",
		 ndis->header.ch,
		 ndis->header.index);
	//dbg_wq("k:c[%d]\n", di_buf->index);
	task_send_cmd2(ndis->header.ch,
		       LCMD2(ECMD_RL_KEEP,
			     ndis->header.ch,
			     ndis->header.index));
}

static void dev_vframe_reg_first(struct dim_itf_s *itf)
{
	//struct dev_vfram_t *pvfm;
	struct dev_vfm_s *pvfmc;

	itf->etype = EDIM_NIN_TYPE_VFM;
	//pvfm = &itf->dvfm;
	pvfmc = &itf->u.dvfmc;

	/* clear */
	memset(pvfmc, 0, sizeof(*pvfmc));
	if (IS_IC_SUPPORT(DECONTOUR))
		pvfmc->vf_m_fill_polling	= nins_m_in_vf_dct;
	else
		pvfmc->vf_m_fill_polling	= nins_m_in_vf;
	pvfmc->vf_m_fill_ready		= vfm_m_fill_ready;
	pvfmc->vf_m_bypass_first_frame	= dim_bypass_first_frame;
	itf->opins_m_back_in	= nins_m_recycle;
	itf->op_fill_ready	= ndis_fill_ready;
	itf->op_m_unreg		= nins_m_unreg;
	itf->op_ready_out	= NULL;
	itf->op_cfg_ch_set	= NULL;
}

static void dev_vframe_reg(struct dim_itf_s *itf)
{
	struct dev_vfram_t *pvfm;

	pvfm = &itf->dvfm;
	vf_reg_provider(&pvfm->di_vf_prov);
	vf_notify_receiver(pvfm->name, VFRAME_EVENT_PROVIDER_START, NULL);
}

#ifdef MARK_HIS
static void dev_vframe_unreg(struct dim_itf_s *itf)
{
	struct dev_vfram_t *pvfm;

	pvfm = &itf->dvfm;
	if (itf->reg) {
		pr_debug("%s:ch[%s]:begin\n", __func__, pvfm->name);
		vf_unreg_provider(&pvfm->di_vf_prov);
		pr_debug("%s:ch[%s]:end\n", __func__, pvfm->name);
		itf->reg = 0;
	}
}
#endif

static int di_ori_event_qurey_vdin2nr(unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	return ppre->vdin2nr;
}

static int di_ori_event_reset(unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	int i;
//ary 2020-12-09	ulong flags;

	/*block*/
	di_block_set(1);//di_blocking = 1;

	/*dbg_ev("%s: VFRAME_EVENT_PROVIDER_RESET\n", __func__);*/
	if (dim_is_bypass(NULL, channel)	||
	    di_bypass_state_get(channel)	||
	    ppre->bypass_flag) {
		pw_vf_notify_receiver(channel,
				      VFRAME_EVENT_PROVIDER_RESET,
			NULL);
	}

//ary 2020-12-09	spin_lock_irqsave(&plist_lock, flags);
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		if (pvframe_in[i])
			pr_dbg("DI:clear vframe_in[%d]\n", i);

		pvframe_in[i] = NULL;
	}
//ary 2020-12-09	spin_unlock_irqrestore(&plist_lock, flags);
	di_block_set(0);//di_blocking = 0;

	return 0;
}

static int di_ori_event_light_unreg(unsigned int channel)
{
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	int i;
//ary 2020-12-09	ulong flags;

	di_block_set(1);//di_blocking = 1;

	pr_dbg("%s: vf_notify_receiver light unreg\n", __func__);

//ary 2020-12-09	spin_lock_irqsave(&plist_lock, flags);
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		if (pvframe_in[i])
			pr_dbg("DI:clear vframe_in[%d]\n", i);

		pvframe_in[i] = NULL;
	}
//ary 2020-12-09	spin_unlock_irqrestore(&plist_lock, flags);
	di_block_set(0);//di_blocking = 0;

	return 0;
}

static int di_ori_event_light_unreg_revframe(unsigned int channel)
{
	struct vframe_s **pvframe_in = get_vframe_in(channel);
	int i;
//ary 2020-12-09	ulong flags;

	unsigned char vf_put_flag = 0;

	pr_info("%s:VFRAME_EVENT_PROVIDER_LIGHT_UNREG_RETURN_VFRAME\n",
		__func__);
/*
 * do not display garbage when 2d->3d or 3d->2d
 */
//ary 2020-12-09	spin_lock_irqsave(&plist_lock, flags);
	for (i = 0; i < MAX_IN_BUF_NUM; i++) {
		if (pvframe_in[i]) {
			pw_vf_put(pvframe_in[i], channel);
			pr_dbg("DI:clear vframe_in[%d]\n", i);
			vf_put_flag = 1;
		}
		pvframe_in[i] = NULL;
	}
	if (vf_put_flag)
		pw_vf_notify_provider(channel,
				      VFRAME_EVENT_RECEIVER_PUT, NULL);

//ary 2020-12-09	spin_unlock_irqrestore(&plist_lock, flags);

	return 0;
}

static int di_irq_ori_event_ready(unsigned int channel)
{
	return 0;
}

static int di_ori_event_ready(unsigned int channel)
{
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (ppre->bypass_flag)
		pw_vf_notify_receiver(channel,
				      VFRAME_EVENT_PROVIDER_VFRAME_READY,
				      NULL);

	if (dip_chst_get(channel) == EDI_TOP_STATE_REG_STEP1)
		task_send_cmd(LCMD1(ECMD_READY, channel));
	else
		task_send_ready(9);

	di_irq_ori_event_ready(channel);
	return 0;
}

static int di_ori_event_qurey_state(unsigned int channel)
{
	/*int in_buf_num = 0;*/
	struct vframe_states states;

	if (dim_vcry_get_flg())
		return RECEIVER_INACTIVE;

	/*fix for ucode reset method be break by di.20151230*/
	di_vf_l_states(&states, channel);
	if (states.buf_avail_num > 0)
		return RECEIVER_ACTIVE;

	if (pw_vf_notify_receiver(channel,
				  VFRAME_EVENT_PROVIDER_QUREY_STATE,
				  NULL) == RECEIVER_ACTIVE)
		return RECEIVER_ACTIVE;

	return RECEIVER_INACTIVE;
}

static void  di_ori_event_set_3D(int type, void *data, unsigned int channel)
{
#ifdef DET3D

	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	if (type == VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE) {
		int flag = (long)data;

		ppre->vframe_interleave_flag = flag;
	}

#endif
}

static void  di_ori_event_set_fcc(unsigned int channel)
{
	struct div2_mm_s *mm = dim_mm_get(channel);

	mm->fcc_value = 2;
	pr_info("%s: ch[%d] %d\n", __func__, channel, mm->fcc_value);
}

/*************************/
/************************************/
/************************************/
struct vframe_s *di_vf_l_get(unsigned int channel)
{
	vframe_t *vframe_ret = NULL;
//	struct di_buf_s *di_buf = NULL;
	struct di_ch_s *pch;

//	struct vframe_s *vfm_dbg;
//	ulong irq_flag2 = 0;
	struct dim_ndis_s *ndis1, *ndis2;

	dim_print("%s:ch[%d]\n", __func__, channel);

	pch = get_chdata(channel);

	/* special case */
	if (!get_init_flag(channel)	||
	    dim_vcry_get_flg()		||
	    di_block_get()			||
	    !get_reg_flag(channel)	||
	    dump_state_flag_get()) {
		dim_tr_ops.post_get2(1);
		return NULL;
	}

	/**************************/
	if (ndis_cnt(pch, QBF_NDIS_Q_DISPLAY) > DI_POST_GET_LIMIT) {
		dim_tr_ops.post_get2(2);
		return NULL;
	}
	/**************************/
	if (!atomic_dec_and_test(&pch->vf_get_idle)) { /* check */
		PR_WARN("get busy\n");
		return NULL;
	}
	/**************************/
	vframe_ret = ndrd_qout(pch);
	if (!vframe_ret || !vframe_ret->private_data) {
		dbg_nq("%s:bypass?\n", __func__);
		didbg_vframe_out_save(channel, vframe_ret, 3);
		atomic_set(&pch->vf_get_idle, 1); /* check set */
		return vframe_ret;
	}
	ndis1 = (struct dim_ndis_s *)vframe_ret->private_data;
	if (!ndis1) {/*is bypass*/
		atomic_set(&pch->vf_get_idle, 1); /* check set */
		return vframe_ret;
	}
	ndis2 = ndis_move(pch, QBF_NDIS_Q_USED, QBF_NDIS_Q_DISPLAY);
	if (ndis1 != ndis2) {
		PR_ERR("%s:ndis1[%d]\n", __func__, ndis1->header.index);
		PR_ERR("ndis2[%d]\n", ndis2->header.index);
	}
	didbg_vframe_out_save(channel, vframe_ret, 4);
	dim_tr_ops.post_get(vframe_ret->index_disp);
	atomic_set(&pch->vf_get_idle, 1); /* check set */
	return vframe_ret;
}

#ifdef MARK_HIS
void di_vf_l_put(struct vframe_s *vf, unsigned char channel)
{
	struct di_buf_s *di_buf = NULL;
	ulong irq_flag2 = 0;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);
//	struct di_post_stru_s *ppost = get_post_stru(channel);

	dim_print("%s:ch[%d]\n", __func__, channel);

	if (ppre->bypass_flag) {
		pw_vf_put(vf, channel);
		pw_vf_notify_provider(channel,
				      VFRAME_EVENT_RECEIVER_PUT, NULL);

		//if (!IS_ERR_OR_NULL(ppost->keep_buf))
		//	recycle_keep_buffer(channel);
		return;
	}
/* struct di_buf_s *p = NULL; */
/* int itmp = 0; */
	if (!get_init_flag(channel)	||
	    dim_vcry_get_flg()		||
	    IS_ERR_OR_NULL(vf)) {
		PR_ERR("%s: 0x%p\n", __func__, vf);
		return;
	}
	if (di_block_get())
		return;
	dim_log_buffer_state("pu_", channel);
	di_buf = (struct di_buf_s *)vf->private_data;
	if (IS_ERR_OR_NULL(di_buf)) {
		pw_vf_put(vf, channel);
		pw_vf_notify_provider(channel,
				      VFRAME_EVENT_RECEIVER_PUT, NULL);
		PR_WARN("%s: get vframe %p without di buf\n",
			__func__, vf);
		return;
	}

	if (di_buf->type == VFRAME_TYPE_POST) {
		#ifdef MARK_HIS
		if (!di_buf->blk_buf) {
			PR_ERR("%s:no blk_buf ind[%d]\n",
				__func__, di_buf->index);
		}
		#endif
		#ifdef MARK_HIS
		if (di_buf->in_buf)
			dbg_nins_log_buf(di_buf->in_buf, 3);
		#endif
		di_lock_irqfiq_save(irq_flag2);

		if (is_in_queue(channel, di_buf, QUEUE_DISPLAY)) {
			di_buf->queue_index = -1;
			di_que_in(channel, QUE_POST_BACK, di_buf);
			di_unlock_irqfiq_restore(irq_flag2);

		} else {
			task_send_cmd2(channel,
				       LCMD2(ECMD_RL_KEEP,
				       channel,
				       di_buf->index));
			di_unlock_irqfiq_restore(irq_flag2);
			PR_WARN("%s:ch[%d]not in display %d\n",
				__func__, channel, di_buf->index);
		}
	} else {
		PR_ERR("%s:t[%d][%d]\n", __func__, di_buf->type, di_buf->index);

		di_lock_irqfiq_save(irq_flag2);
		queue_in(channel, di_buf, QUEUE_RECYCLE);
		di_unlock_irqfiq_restore(irq_flag2);

		dim_print("%s: %s[%d] =>recycle_list\n", __func__,
			  dim_get_vfm_type_name(di_buf->type), di_buf->index);
	}

	task_send_ready();
}
#else
void di_vf_l_put(struct vframe_s *vf, unsigned char channel)
{
//	struct di_buf_s *di_buf = NULL;
//	ulong irq_flag2 = 0;
	//struct di_pre_stru_s *ppre = get_pre_stru(channel);
//	struct di_post_stru_s *ppost = get_post_stru(channel);
	struct di_ch_s *pch;
	struct dim_ndis_s *ndis1;

	dim_print("%s:ch[%d]\n", __func__, channel);

	pch = get_chdata(channel);

	/* special case */
	if (!get_init_flag(channel)	||
	    dim_vcry_get_flg()		||
	    IS_ERR_OR_NULL(vf)) {
		PR_ERR("%s: 0x%p\n", __func__, vf);
		return;
	}
	if (di_block_get())
		return;

	ndis1 = (struct dim_ndis_s *)vf->private_data;
	if (IS_ERR_OR_NULL(ndis1)) {
		pw_vf_put(vf, channel);
		pw_vf_notify_provider(channel,
				      VFRAME_EVENT_RECEIVER_PUT, NULL);
		//PR_WARN("%s: get vframe %p without di buf\n",
		//	__func__, vf);
		return;
	}
	task_send_cmd2(channel,
			LCMD2(ECMD_RL_KEEP,
			     channel,
			     ndis1->header.index));
	//task_send_ready();
}

#endif

#ifdef MARK_HIS
struct vframe_s *di_vf_l_peek(unsigned int channel)
{
	struct vframe_s *vframe_ret = NULL;
	struct di_buf_s *di_buf = NULL;
	struct di_pre_stru_s *ppre = get_pre_stru(channel);

	/*dim_print("%s:ch[%d]\n",__func__, channel);*/

	di_sum_inc(channel, EDI_SUM_O_PEEK_CNT);
	if (ppre->bypass_flag) {
		dim_tr_ops.post_peek(0);
		return pw_vf_peek(channel);
	}
	if (!get_init_flag(channel)	||
	    dim_vcry_get_flg()		||
	    di_block_get()			||
	    !get_reg_flag(channel)	||
	    dump_state_flag_get()) {
		dim_tr_ops.post_peek(1);
		return NULL;
	}

	/**************************/
	if (list_count(channel, QUEUE_DISPLAY) > DI_POST_GET_LIMIT) {
		dim_tr_ops.post_peek(2);
		return NULL;
	}
	/**************************/
	dim_log_buffer_state("pek", channel);

	if (!di_que_is_empty(channel, QUE_POST_READY)) {
		di_buf = di_que_peek(channel, QUE_POST_READY);
		if (di_buf)
			vframe_ret = di_buf->vframe;
	}
#ifdef DI_BUFFER_DEBUG
	if (vframe_ret)
		dim_print("%s: %s[%d]:%x\n", __func__,
			  dim_get_vfm_type_name(di_buf->type),
			  di_buf->index, vframe_ret);
#endif
	if (vframe_ret) {
		dim_tr_ops.post_peek(9);
	} else {
		task_send_ready();
		dim_tr_ops.post_peek(4);
	}
	return vframe_ret;
}
#else
struct vframe_s *di_vf_l_peek(unsigned int channel)
{
	struct vframe_s *vframe_ret = NULL;
//	struct di_buf_s *di_buf = NULL;
	struct di_ch_s *pch;
//	struct dim_ndis_s *ndis1;

	//struct di_pre_stru_s *ppre = get_pre_stru(channel);

	/*dim_print("%s:ch[%d]\n",__func__, channel);*/
	pch = get_chdata(channel);
	di_sum_inc(channel, EDI_SUM_O_PEEK_CNT);

	/* special case */
	if (!get_init_flag(channel)	||
	    dim_vcry_get_flg()		||
	    di_block_get()			||
	    !get_reg_flag(channel)	||
	    dump_state_flag_get()) {
		dim_tr_ops.post_peek(1);
		return NULL;
	}

	/**************************/
	if (ndis_cnt(pch, QBF_NDIS_Q_DISPLAY) > DI_POST_GET_LIMIT) {
		dim_tr_ops.post_peek(2);
		return NULL;
	}
	/**************************/
	vframe_ret = ndrd_qpeekvfm(pch);

	if (vframe_ret) {
		dim_tr_ops.post_peek(9);
	} else {
//		task_send_ready(22);
		dim_tr_ops.post_peek(4);
	}
	return vframe_ret;
}

#endif
int di_vf_l_states(struct vframe_states *states, unsigned int channel)
{
	struct div2_mm_s *mm = dim_mm_get(channel);
	struct dim_sum_s *psumx = get_sumx(channel);

	/*pr_info("%s: ch[%d]\n", __func__, channel);*/
	if (!states)
		return -1;
	states->vf_pool_size = mm->sts.num_local;
	states->buf_free_num = psumx->b_pre_free;

	states->buf_avail_num = psumx->b_pst_ready;
	states->buf_recycle_num = psumx->b_recyc;
	if (dimp_get(edi_mp_di_dbg_mask) & 0x1) {
		di_pr_info("di-pre-ready-num:%d\n", psumx->b_pre_ready);
		di_pr_info("di-display-num:%d\n", psumx->b_display);
	}
	return 0;
}

/*--------------------------*/

const char * const di_receiver_event_cmd[] = {
	"",
	"_UNREG",
	"_LIGHT_UNREG",
	"_START",
	NULL,	/* "_VFRAME_READY", */
	NULL,	/* "_QUREY_STATE", */
	"_RESET",
	NULL,	/* "_FORCE_BLACKOUT", */
	"_REG",
	"_LIGHT_UNREG_RETURN_VFRAME",
	NULL,	/* "_DPBUF_CONFIG", */
	NULL,	/* "_QUREY_VDIN2NR", */
	NULL,	/* "_SET_3D_VFRAME_INTERLEAVE", */
	NULL,	/* "_FR_HINT", */
	NULL,	/* "_FR_END_HINT", */
	NULL,	/* "_QUREY_DISPLAY_INFO", */
	NULL,	/* "_PROPERTY_CHANGED", */
};

#define VFRAME_EVENT_PROVIDER_CMD_MAX	16

static int di_receiver_event_fun(int type, void *data, void *arg)
{
	struct dev_vfram_t *pvfm;
	unsigned int ch;
	int ret = 0;
	struct di_ch_s *pch;
	char *provider_name = (char *)data;
	struct div2_mm_s *mm = NULL;

	ch = *(int *)arg;
	pch = get_chdata(ch);
	pvfm = get_dev_vframe(ch);
	mm = dim_mm_get(ch);

	if (type <= VFRAME_EVENT_PROVIDER_CMD_MAX	&&
	    di_receiver_event_cmd[type]) {
		dbg_ev("ch[%d]:%s:%d:%s\n", ch, "event",
		       type,
		       di_receiver_event_cmd[type]);
	}

	switch (type) {
	case VFRAME_EVENT_PROVIDER_UNREG:
		mutex_lock(&pch->itf.lock_reg);
		if (!pch->itf.reg) {
			mutex_unlock(&pch->itf.lock_reg);
			PR_WARN("duplicate ureg\n");
			break;
		}
		mutex_unlock(&pch->itf.lock_reg);
		vf_unreg_provider(&pch->itf.dvfm.di_vf_prov);
		mutex_lock(&pch->itf.lock_reg);
		pch->itf.reg = 0;
		//dev_vframe_unreg(&pch->itf);
		dim_trig_unreg(ch);
		dim_api_unreg(DIME_REG_MODE_VFM, pch);
		mutex_unlock(&pch->itf.lock_reg);
		if (mm->fcc_value != 0)
			mm->fcc_value--;
		break;
	case VFRAME_EVENT_PROVIDER_REG:

		mutex_lock(&pch->itf.lock_reg);
		if (pch->itf.reg) {
			PR_WARN("duplicate reg\n");
			mutex_unlock(&pch->itf.lock_reg);
			break;
		}
		dev_vframe_reg_first(&pch->itf);
		pch->sum_reg_cnt++;
		dbg_reg("reg:%s[%d]\n", provider_name, pch->sum_reg_cnt);
		atomic_set(&pch->vf_get_idle, 1);
		pch->ponly_set = false;
		pch->plink_dct = false;
		dim_api_reg(DIME_REG_MODE_VFM, pch);

		dev_vframe_reg(&pch->itf);
		pch->itf.reg = 1;
		mutex_unlock(&pch->itf.lock_reg);

		break;
	case VFRAME_EVENT_PROVIDER_START:
		break;

	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG:
		ret = di_ori_event_light_unreg(ch);
		break;
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
		ret = di_ori_event_ready(ch);
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_STATE:
		ret = di_ori_event_qurey_state(ch);
		break;
	case VFRAME_EVENT_PROVIDER_RESET:
		ret = di_ori_event_reset(ch);
		break;
	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG_RETURN_VFRAME:
		ret = di_ori_event_light_unreg_revframe(ch);
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_VDIN2NR:
		ret = di_ori_event_qurey_vdin2nr(ch);
		break;
	case VFRAME_EVENT_PROVIDER_SET_3D_VFRAME_INTERLEAVE:
		di_ori_event_set_3D(type, data, ch);
		break;
	case VFRAME_EVENT_PROVIDER_FR_HINT:
	case VFRAME_EVENT_PROVIDER_FR_END_HINT:
		vf_notify_receiver(pvfm->name, type, data);
		break;
	case VFRAME_EVENT_PROVIDER_FCC:
		di_ori_event_set_fcc(ch);
		break;

	default:
		break;
	}

	return ret;
}

static const struct vframe_receiver_op_s di_vf_receiver = {
	.event_cb	= di_receiver_event_fun
};

#ifdef MARK_HIS
bool vf_type_is_prog(unsigned int type)
{
	bool ret = (type & VIDTYPE_TYPEMASK) == 0 ? true : false;

	return ret;
}

bool vf_type_is_interlace(unsigned int type)
{
	bool ret = (type & VIDTYPE_INTERLACE) ? true : false;

	return ret;
}

bool vf_type_is_top(unsigned int type)
{
	bool ret = ((type & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP)
		? true : false;
	return ret;
}

bool vf_type_is_bottom(unsigned int type)
{
	bool ret = ((type & VIDTYPE_INTERLACE_BOTTOM)
		== VIDTYPE_INTERLACE_BOTTOM)
		? true : false;

	return ret;
}

bool vf_type_is_inter_first(unsigned int type)
{
	bool ret = (type & VIDTYPE_INTERLACE_TOP) ? true : false;

	return ret;
}

bool vf_type_is_mvc(unsigned int type)
{
	bool ret = (type & VIDTYPE_MVC) ? true : false;

	return ret;
}

bool vf_type_is_no_video_en(unsigned int type)
{
	bool ret = (type & VIDTYPE_NO_VIDEO_ENABLE) ? true : false;

	return ret;
}

bool vf_type_is_VIU422(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_422) ? true : false;

	return ret;
}

bool vf_type_is_VIU_FIELD(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_FIELD) ? true : false;

	return ret;
}

bool vf_type_is_VIU_SINGLE(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_SINGLE_PLANE) ? true : false;

	return ret;
}

bool vf_type_is_VIU444(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_444) ? true : false;

	return ret;
}

bool vf_type_is_VIUNV21(unsigned int type)
{
	bool ret = (type & VIDTYPE_VIU_NV21) ? true : false;

	return ret;
}

bool vf_type_is_vscale_dis(unsigned int type)
{
	bool ret = (type & VIDTYPE_VSCALE_DISABLE) ? true : false;

	return ret;
}

bool vf_type_is_canvas_toggle(unsigned int type)
{
	bool ret = (type & VIDTYPE_CANVAS_TOGGLE) ? true : false;

	return ret;
}

bool vf_type_is_pre_interlace(unsigned int type)
{
	bool ret = (type & VIDTYPE_PRE_INTERLACE) ? true : false;

	return ret;
}

bool vf_type_is_highrun(unsigned int type)
{
	bool ret = (type & VIDTYPE_HIGHRUN) ? true : false;

	return ret;
}

bool vf_type_is_compress(unsigned int type)
{
	bool ret = (type & VIDTYPE_COMPRESS) ? true : false;

	return ret;
}

bool vf_type_is_pic(unsigned int type)
{
	bool ret = (type & VIDTYPE_PIC) ? true : false;

	return ret;
}

bool vf_type_is_scatter(unsigned int type)
{
	bool ret = (type & VIDTYPE_SCATTER) ? true : false;

	return ret;
}

bool vf_type_is_vd2(unsigned int type)
{
	bool ret = (type & VIDTYPE_VD2) ? true : false;

	return ret;
}
#endif

bool is_bypss_complete(struct dev_vfram_t *pvfm)
{
	return pvfm->bypass_complete;
}

#ifdef MARK_HIS
bool is_reg(unsigned int ch)
{
	struct dev_vfram_t *pvfm;

	pvfm = get_dev_vframe(ch);

	return pvfm->reg;
}
#endif

#ifdef MARK_HIS
void set_bypass_complete(struct dev_vfram_t *pvfm, bool on)
{
	if (on)
		pvfm->bypass_complete = true;
	else
		pvfm->bypass_complete = false;
}
#endif

#ifdef MARK_HIS //move to di_prc for interface
void set_bypass2_complete(unsigned int ch, bool on)
{
	struct dev_vfram_t *pvfm;

	pvfm = get_dev_vframe(ch);
	set_bypass_complete(pvfm, on);
}

bool is_bypss2_complete(unsigned int ch)
{
	struct dev_vfram_t *pvfm;

	pvfm = get_dev_vframe(ch);

	return is_bypss_complete(pvfm);
}
#endif

static struct vframe_s *di_vf_peek(void *arg)
{
	unsigned int ch = *(int *)arg;
	struct di_ch_s *pch;
	struct vframe_s *vfm;

	/*dim_print("%s:ch[%d]\n",__func__,ch);*/
	pch = get_chdata(ch);

	if (!pch->itf.reg)
		return NULL;

	if (di_is_pause(ch))
		return NULL;
	if (pch->itf.pre_vpp_link && dpvpp_vf_ops())
		return dpvpp_vf_ops()->peek(pch->itf.p_itf);
	if (is_bypss2_complete(ch)) {
		vfm = dim_nbypass_peek(pch);
		if (vfm)
			return vfm;
		return pw_vf_peek(ch);
	}
	return di_vf_l_peek(ch);
}

static struct vframe_s *di_vf_get(void *arg)
{
	unsigned int ch = *(int *)arg;
	struct di_ch_s *pch;
	struct vframe_s *vfm;

	/*struct vframe_s *vfm;*/
	pch = get_chdata(ch);

	if (!pch->itf.reg)
		return NULL;

	dim_tr_ops.post_get2(5);
	if (di_is_pause(ch))
		return NULL;

	di_pause_step_done(ch);
	if (pch->itf.pre_vpp_link && dpvpp_vf_ops())
		return dpvpp_vf_ops()->get(pch->itf.p_itf);

	/*pvfm = get_dev_vframe(ch);*/

	if (is_bypss2_complete(ch)) {
		vfm = dim_nbypass_get(pch);
		if (vfm) {
			dbg_ic("%s vf:%p, index:%d, pts_us64:0x%llx, video_id:%d\n",
				__func__, vfm, vfm->index, vfm->pts_us64,
				vfm->vf_ud_param.ud_param.instance_id);
			return vfm;
		}
		vfm = pw_vf_get(ch);
		if (vfm)
			dbg_ic("%s vf:%p, index:%d, pts_us64:0x%llx, video_id:%d\n",
				__func__, vfm, vfm->index, vfm->pts_us64,
				vfm->vf_ud_param.ud_param.instance_id);
		return vfm;
	}
	sum_pst_g_inc(ch);

	vfm = di_vf_l_get(ch);
	if (vfm)
		dbg_ic("%s vf:%p, index:%d, pts_us64:0x%llx, video_id:%d, sum_pst:%d\n",
			__func__, vfm, vfm->index, vfm->pts_us64,
			vfm->vf_ud_param.ud_param.instance_id, get_sum_pst_g(ch));
	return vfm;
}

static void di_vf_put(struct vframe_s *vf, void *arg)
{
	unsigned int ch = *(int *)arg;
	struct di_ch_s *pch;

	pch = get_chdata(ch);

	if (!pch->itf.reg) {
		PR_ERR("%s:ch[%d],unreg?\n", __func__, ch);
		return;
	}

	if (pch->itf.pre_vpp_link && dpvpp_vf_ops()) {
		dpvpp_vf_ops()->put(vf, pch->itf.p_itf);
		return;
	}
	if (is_bypss2_complete(ch)) {
		pw_vf_put(vf, ch);
		pw_vf_notify_provider(ch,
				      VFRAME_EVENT_RECEIVER_PUT, NULL);
		return;
	}

	sum_pst_p_inc(ch);

	di_vf_l_put(vf, ch);
}

static int di_event_cb(int type, void *data, void *private_data)
{
	if (type == VFRAME_EVENT_RECEIVER_FORCE_UNREG) {
		pr_info("%s: RECEIVER_FORCE_UNREG return\n",
			__func__);
		return 0;
	}
	return 0;
}

static int di_vf_states(struct vframe_states *states, void *arg)
{
	unsigned int ch = *(int *)arg;
	struct di_ch_s *pch;

	if (!states)
		return -1;
	pch = get_chdata(ch);
	if (pch->itf.pre_vpp_link && dpvpp_vf_ops()) {
		dpvpp_vf_ops()->vf_states(states, pch->itf.p_itf);
		return 0;
	}

	dim_print("%s:ch[%d]\n", __func__, ch);

	di_vf_l_states(states, ch);
	return 0;
}

static const struct vframe_operations_s deinterlace_vf_provider = {
	.peek		= di_vf_peek,
	.get		= di_vf_get,
	.put		= di_vf_put,
	.event_cb	= di_event_cb,
	.vf_states	= di_vf_states,
};

struct vframe_s *pw_vf_get(unsigned int ch)
{
	sum_g_inc(ch);
	struct vframe_s *vf = vf_get(di_rev_name[ch]);

	if (vf)
		dbg_ic("%s bypass:%d, vf:%p, index:%d, pts_us64:0x%llx, video_id:%d\n",
			__func__, is_bypss2_complete(ch), vf,
			vf->index, vf->pts_us64, vf->vf_ud_param.ud_param.instance_id);
	return vf;
}

struct vframe_s *pw_vf_peek(unsigned int ch)
{
	#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	return vf_peek(di_rev_name[ch]);
	#else
	return NULL;
	#endif
}

void pw_vf_put(struct vframe_s *vf, unsigned int ch)
{
	sum_p_inc(ch);
	#ifdef CONFIG_AMLOGIC_MEDIA_VFM
	vf_put(vf, di_rev_name[ch]);
	#endif
}

int pw_vf_notify_provider(unsigned int channel, int event_type, void *data)
{
	return vf_notify_provider(di_rev_name[channel], event_type, data);
}

int pw_vf_notify_receiver(unsigned int channel, int event_type, void *data)
{
	return vf_notify_receiver(di_rev_name[channel], event_type, data);
}

void pw_vf_light_unreg_provider(unsigned int ch)
{
	struct dev_vfram_t *pvfm;
	struct vframe_provider_s *prov;

	pvfm = get_dev_vframe(ch);

	prov = &pvfm->di_vf_prov;
	vf_light_unreg_provider(prov);
}

void dev_vframe_exit(void)
{
	struct dev_vfram_t *pvfm;
	int ch;
	struct di_ch_s *pch;

#ifdef TST_NEW_INS_INTERFACE
		vfmtst_exit();
		PR_INF("new ins interface test end\n");
		return;
#endif

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		//pvfm = get_dev_vframe(ch);
		pch = get_chdata(ch);
		pvfm = &pch->itf.dvfm;
		vf_unreg_provider(&pvfm->di_vf_prov);
		vf_unreg_receiver(&pvfm->di_vf_recv);
	}
	pr_info("%s finish\n", __func__);
}

void dev_vframe_init(void)
{
	struct dev_vfram_t *pvfm;
	int ch;
	struct di_ch_s *pch;

#ifdef TST_NEW_INS_INTERFACE
	struct dim_itf_s *pintf;

	vfmtst_init();
	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pch = get_chdata(ch);
		pintf = &pch->itf;
		pintf->ch = ch;
	}
	PR_INF("new ins interface test enable\n");
	return;
#endif

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pch = get_chdata(ch);
		pch->itf.ch = ch; //2020-12-21
		pvfm = &pch->itf.dvfm;
		pvfm->name = di_rev_name[ch];
		pvfm->index = ch;
		/*set_bypass_complete(pvfm, true);*/ /*test only*/

		/*receiver:*/
		vf_receiver_init(&pvfm->di_vf_recv, pvfm->name,
				 &di_vf_receiver, &pvfm->index);
		vf_reg_receiver(&pvfm->di_vf_recv);

		/*provider:*/
		vf_provider_init(&pvfm->di_vf_prov, pvfm->name,
				 &deinterlace_vf_provider, &pvfm->index);
	}
	dbg_mem("%s finish\n", __func__);
}
