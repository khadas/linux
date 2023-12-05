// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include "di_task.h"

#include "di_vframe.h"
#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_DIM
#include <trace/events/meson_atrace.h>

#ifdef MARK_HIS
const struct di_init_parm dim_cfg_parm_default = {
	.work_mode	= WORK_MODE_PRE_POST,
	.buffer_mode	= BUFFER_MODE_ALLOC_BUF,
	.ops = {
		.empty_input_done	= NULL,
		.fill_output_done	= NULL,
	},
};
#endif
#ifdef MARK_HIS //@ary_note to-do
/**********************************************************
 * ops
 **********************************************************/
static void *ins_peek(struct di_ch_s *pch)
{
	unsigned int index;

	if (!qued_ops.peek(pch, QUED_T_IS_IN, &index))
		return NULL;
	return (void *)pch->interf.u.dinst.in[index];
}

static void *ins_get(struct di_ch_s *pch)
{
	unsigned int index;
	struct di_buffer *dbuf;
	unsigned int dbg_cnt1, dbg_cnt2;

	if (!qued_ops.move(pch,
			   QUED_T_IS_IN,
			   QUED_T_IS_FREE, &index)) {
		dbg_cnt1 = qued_ops.list_count(pch, QUED_T_IS_IN);
		dbg_cnt2 = qued_ops.list_count(pch, QUED_T_IS_FREE);
		PR_ERR("%s:no space,%d,%d\n", __func__, dbg_cnt1, dbg_cnt2);
		return NULL;
	}
	sum_g_inc(pch->ch_id);
	dbuf = pch->interf.u.dinst.in[index];
	pch->interf.u.dinst.in[index] = NULL;
	return (void *)dbuf;
}

static void ins_put(void *data, struct di_ch_s *pch)
{
	struct dim_inter_s *pintf = &pch->interf;
	struct di_operations_s *ops;

	ops = &pintf->u.dinst.parm.ops;
	if (ops->empty_input_done) {
		sum_p_inc(pch->ch_id);
		ops->empty_input_done((struct di_buffer *)data);
		//debug only dump_stack();
	}
}

const struct dim_itf_ops_s inst_in_ops = {
	.peek	= ins_peek,
	.get	= ins_get,
	.put	= ins_put,
};

/* mode 2 for drop */
void ins_2_doing(struct di_ch_s *pch, bool bypass, struct di_buf_s *pstdi_buf)
{
	unsigned int ch = pch->ch_id;
	struct di_buf_s *di_buf = NULL; /*post nobuf buffer*/
	bool ret;
	struct di_buffer	*ins_buf, *ins_in;
	unsigned int idx;
	struct dim_inter_s	*pintf;
	struct vframe_s		*vfmin, *vfmout;
	struct canvas_config_s	cvs[2];

	if (di_que_is_empty(ch, QUE_POST_NOBUF))
		return;

	if (qued_ops.is_empty(pch, QUED_T_IS_PST_DOBEF))
		return;

	di_buf = di_que_peek(ch, QUE_POST_NOBUF);
	dim_print("%s:1:%p,buf[%d],t[%d]\n", __func__, di_buf, di_buf->index, di_buf->type);
	trace_buf(di_buf, DIM_QUE_PEEK, QUE_POST_NOBUF,
		(DIM_DBG_MARK | 0x00000001));
	ret = di_que_out(ch, QUE_POST_NOBUF, di_buf);
	if (!ret) {
		PR_ERR("%s:no buf?\n", __func__);
		return;
	}

	pintf = &pch->interf;
	qued_ops.out(pch, QUED_T_IS_PST_DOBEF, &idx);

	ins_buf = pintf->u.dinst.out[idx];
	if (!ins_buf) {
		PR_ERR("%s:out is null\n", __func__);
		return;
	}
	di_buf->c.pdvfm->vfm_out = pintf->u.dinst.out[idx];
	pintf->u.dinst.out[idx] = NULL;
	qued_ops.in(pch, QUED_T_IS_PST_FREE, idx);

	trace_buf(di_buf, DIM_QUE_OUT, QUE_POST_NOBUF,
			(DIM_DBG_MARK | 0x00000002));

	dbg_dbg("%s:ch[%d],type[%d], id[%d]\n", __func__, pch->ch_id,
		di_buf->type, di_buf->index);
	dbg_dbg("ins_buf,type[%d], id[%d]\n",
		ins_buf->mng.type,
		ins_buf->mng.index);
	if (!ins_buf)
		PR_ERR("%s:out2 is null\n", __func__);

	/*in*/
	ins_in = (struct di_buffer *)di_buf->c.pdvfm->vfm_in;
	vfmin = ins_in->vf;
	vfmout = ins_buf->vf;
	if (!di_buf->c.wmode.is_eos) {
		/*copy vfm */
		memcpy(&cvs[0], &vfmout->canvas0_config[0], sizeof(cvs));
		memcpy(vfmout, vfmin, sizeof(*vfmout));
		memcpy(&vfmout->canvas0_config[0],
			&cvs[0], sizeof(cvs));
	}

	if (!di_buf->c.wmode.is_bypass) {
		//di_buf->nr_adr	= ins_buf->phy_addr;
		//di_buf->nr_adr
		//= ins_buf->vf->canvas0_config[0].phy_addr;
		//ins_buf->phy_addr;
		dim_ins_cnt_post_cvs_size2(di_buf, ins_buf, ch);
	} else {
		/* bypass */
		if (di_buf->c.wmode.is_eos) {
			ins_buf->flag |= DI_FLAG_EOS;
			PR_INF("%s:eos\n", __func__);
		} else {
			ins_buf->flag |= DI_FLAG_BUF_BY_PASS;
		}
		dim_print("%s:%p bypass\n", __func__, ins_buf);
	}
	dbg_vfm(vfmout, 1);
	trace_buf(di_buf, DIM_QUE_IN, QUE_POST_DOING,
		(DIM_DBG_MARK | 0x00000003));
	dim_print("%s:%p,buf[%d],t[%d]\n", __func__,
		  di_buf, di_buf->index, di_buf->type);
	di_que_in(ch, QUE_POST_DOING, di_buf);
}
#endif //@ary_note todo

/**************************************
 * bit 0: force no release
 * bit 1: dbg for buffer
 **************************************/

static unsigned int new_int;
module_param_named(new_int, new_int, uint, 0664);

bool dim_dbg_new_int(unsigned int id)
{
	bool ret = false;
	/* id is 1 ~ 32 */

	if (id > 32 || id == 0)
		return false;

	if (bget(&new_int, id - 1))
		ret = true;

	return ret;
}

static void nins_m_unreg_new(struct di_ch_s *pch)
{
}

/**************************************
 * nins_m_recycle
 *	op_back_input
 *	_RECYCLE -> _IDLE
 *	back vfm to dec
 *	run in main
 **************************************/

static void nins_m_recycle_ins(struct di_ch_s *pch)
{
	struct buf_que_s *pbufq;
	int i;
	unsigned int cnt, ch;
	struct dim_nins_s *ins;
	//struct vframe_s *vfm;
	struct di_buffer *buffer;

	if (!pch || pch->itf.etype != EDIM_NIN_TYPE_INS) {
		PR_ERR("%s:\n", __func__);
		return;
	}
	ch = pch->ch_id;
	pbufq = &pch->nin_qb;

	cnt = qbufp_count(pbufq, QBF_NINS_Q_RECYCLE);

	if (!cnt)
		return;
	//qbuf_dbg_check_in_buffer_id(1);

	for (i = 0; i < cnt; i++) {
		ins = nins_move(pch, QBF_NINS_Q_RECYCLE, QBF_NINS_Q_IDLE);
	//	vfm = (struct vframe_s *)ins->c.ori;
		buffer = (struct di_buffer *)ins->c.ori;
		ins->c.ori = NULL;
		if (buffer) {
//			dim_dbg_buffer(buffer, 10);
			//pw_vf_put(vfm, ch);
			//pw_vf_notify_provider(ch,
			//		      VFRAME_EVENT_RECEIVER_PUT, NULL);
			pch->itf.u.dinst.parm.ops.empty_input_done(buffer);
			sum_p_inc(ch);
		}

		memset(&ins->c, 0, sizeof(ins->c));
	}
	//qbuf_dbg_check_in_buffer_id(2);
}

static int reg_idle_ch(void)
{
	unsigned int ch;
	struct dim_itf_s *pintf;
	int ret = -1;
	//int i;
	struct di_ch_s *pch;
	struct di_mng_s *pbm = get_bufmng();

	for (ch = 0; ch < DI_CHANNEL_NUB; ch++) {
		pch = get_chdata(ch);

		pintf = &pch->itf;//get_dev_intf(ch);
		mutex_lock(&pch->itf.lock_reg);

		if ((pbm->tmode_pre[ch] == EDIM_TMODE_2_PW_OUT ||
		     pbm->tmode_pre[ch] == EDIM_TMODE_3_PW_LOCAL) &&
		    !pintf->reg) {
			/*set reg flg*/
			pintf->reg = true;
			ret = (int)ch;
			mutex_unlock(&pch->itf.lock_reg);
			dbg_dbg("%s:ch[%d], ret[%d]\n", __func__, ch, ret);
			break;
		}
		dbg_dbg("%s:ch[%d], tmode[%d], reg[%d]\n", __func__,
			ch, pbm->tmode_pre[ch], pintf->reg);

		mutex_unlock(&pch->itf.lock_reg);
	}
	dbg_dbg("%s:%d\n", __func__, ret);

	return ret;
}

#define ERR_INDEX	(0xffff)
static unsigned int index_2_ch(int index)
{
	unsigned int ret;

	if (index >= DI_CHANNEL_NUB || index < 0) {
		PR_ERR("instance index is overflow:%d\n", index);
		ret = ERR_INDEX;
	} else {
		ret = (unsigned int)index;
	}

	return ret;
}

void dim_dbg_buffer(struct di_buffer *buffer, unsigned int id)
{
	PR_INF("%s:%d:%px\n", __func__, id, buffer);
	PR_INF("\t:code:0x%x,ch[%d],index[%d], type[%d]\n",
	       buffer->mng.code, buffer->mng.ch,
	       buffer->mng.index, buffer->mng.type);
	if (buffer->vf)
		PR_INF("\t:vf:0x%px, 0x%x\n", buffer->vf, buffer->vf->index);
	else
		PR_INF("\t:%s\n", "no vf");
}

void dim_dbg_buffer2(struct di_buffer *buffer, unsigned int id)
{
	if (!buffer) {
		dbg_nq("%s:%d:no buffer\n", __func__, id);
		return;
	}
	dbg_nq("%s:%d:%px\n", __func__, id, buffer);
	if (buffer->vf) {
		dbg_nq("\t:0x%px, 0x%lx,0x%lx\n", buffer,
			buffer->vf->canvas0_config[0].phy_addr,
			buffer->vf->canvas0_config[1].phy_addr);
	} else {
		dbg_nq("\t:%s\n", "no vf");
	}
}

static void cfg_ch_set_for_s4_cp(struct di_ch_s *pch)
{
	unsigned int out_format;

	if (pch->itf.u.dinst.parm.work_mode != WORK_MODE_S4_DCOPY)
		return;
	cfgsch(pch, KEEP_DEC_VF, 0); // for all new_interface
	out_format = pch->itf.u.dinst.parm.output_format &
		DIM_OUT_FORMAT_FIX_MASK;
	/* out: nv21 */
	if (out_format == DI_OUTPUT_NV21)
		cfgsch(pch, POUT_FMT, 1);
	else
		cfgsch(pch, POUT_FMT, 2);
	PR_INF("%s:output_format:0x%x\n", __func__, out_format);
	cfgsch(pch, IOUT_FMT, 1);
	cfgsch(pch, ALLOC_SCT, 0);
	cfgsch(pch, DAT, 0);
	cfgsch(pch, 4K, 1);
	cfgsch(pch, HF, 0);
	cfgsch(pch, DCT, 0);
	cfgsch(pch, DW_EN, 0);
}

static void cfg_ch_set(struct di_ch_s *pch)
{
	struct di_init_parm *parm;
	unsigned int out_format;
	bool used_di_define = false;
	parm = &pch->itf.u.dinst.parm;
	out_format = parm->output_format & DIM_OUT_FORMAT_FIX_MASK;

	if (parm->output_format & DI_OUTPUT_BY_DI_DEFINE &&
	    pch->itf.tmode == EDIM_TMODE_3_PW_LOCAL)
		used_di_define = true;

	if (!used_di_define) {
		switch (out_format) {
		case DI_OUTPUT_422:
			cfgsch(pch, POUT_FMT, 0);
			cfgsch(pch, IOUT_FMT, 0);
			break;
		case DI_OUTPUT_NV12:
			cfgsch(pch, POUT_FMT, 2);
			cfgsch(pch, IOUT_FMT, 2);
			break;
		case DI_OUTPUT_NV21:
			cfgsch(pch, POUT_FMT, 1);
			cfgsch(pch, IOUT_FMT, 1);
			break;
		default:
			PR_INF("%s:format not set\n", __func__);
			break;
		}
		cfgsch(pch, DW_EN, 0);
	}
	cfgsch(pch, KEEP_DEC_VF, 0); // for all new_interface
	if (dip_itf_is_ins_exbuf(pch)) {
		//cfgsch(pch, KEEP_DEC_VF, 0);
		cfgsch(pch, ALLOC_SCT, 0);
		cfgsch(pch, 4K, 0);
		cfgsch(pch, DW_EN, 0);
	}

	if (!(di_dbg & DBG_M_REG))
		return;
	/* debug only */
	PR_INF("%s:ch[%d]\n",
	       __func__, pch->ch_id);
	PR_INF("\tkeep_dec_vf[%d]\n", cfggch(pch, KEEP_DEC_VF));
	PR_INF("\tout_fmt[%d][%d]\n", cfggch(pch, POUT_FMT),
		cfggch(pch, IOUT_FMT));
	PR_INF("\talloc_sct[%d],4K[%d],dw[%d]\n", cfggch(pch, ALLOC_SCT),
		cfggch(pch, 4K),
		cfggch(pch, DW_EN));
	PR_INF("\ttvp:%d\n",
	       parm->output_format & DI_OUTPUT_TVP ? 1 : 0);
	PR_INF("\tPOST_NUB[%d]\n", cfggch(pch, POST_NUB));
}

/**********************************************************
 * @brief  di_create_instance  creat di instance
 * @param[in]  parm    Pointer of parm structure
 * @return      di index for success, or fail type if < 0
 **********************************************************/
int new_create_instance(struct di_init_parm parm)
{
	int ret;
	unsigned int ch;
	struct di_ch_s *pch;
	struct dim_itf_s *itf;

	dbg_reg("%s:", __func__);
	ret = reg_idle_ch();
	if (ret < 0) {
		PR_ERR("%s:no idle ch\n", __func__);
		return DI_ERR_REG_NO_IDLE_CH;
	}

	if ((parm.output_format & 0xffff) == DI_OUTPUT_422 ||
	    parm.output_format & DI_OUTPUT_BY_DI_DEFINE)
		parm.output_format &= ~DI_OUTPUT_LINEAR;

	ch = (unsigned int)ret;
	pch = get_chdata(ch);
	itf = &pch->itf;
	itf->etype = EDIM_NIN_TYPE_INS;
	/*parm*/
	memcpy(&itf->u.dinst.parm, &parm, sizeof(struct di_init_parm));

	itf->tmode = EDIM_TMODE_2_PW_OUT;
	if (s4dw_test_ins()) {
		/*used local buffer to check s4 copy */
		itf->tmode = EDIM_TMODE_3_PW_LOCAL;
		itf->u.dinst.parm.work_mode = WORK_MODE_S4_DCOPY;
	}
	//check ic:
	if (itf->u.dinst.parm.work_mode == WORK_MODE_S4_DCOPY) {
		if (!DIM_IS_IC(S4) && !DIM_IS_IC(T3)) {
			PR_ERR("%s:copy only support for s4\n", __func__);
			return DI_ERR_UNSUPPORT;
		}
		PR_INF("S4 DW\n");
	}

	if (itf->u.dinst.parm.work_mode == WORK_MODE_PRE_POST) {
		switch (itf->u.dinst.parm.buffer_mode) {
		case BUFFER_MODE_ALLOC_BUF:
			itf->tmode = EDIM_TMODE_3_PW_LOCAL;
			break;
		case BUFFER_MODE_USE_BUF:
			itf->tmode = EDIM_TMODE_2_PW_OUT;
			break;
		default:
			PR_ERR("%s:bmode[%d]\n", __func__,
			       itf->u.dinst.parm.buffer_mode);
			break;
		}
	} else if (itf->u.dinst.parm.work_mode == WORK_MODE_S4_DCOPY) {
		#ifndef TMP_FOR_S4DW
		if (itf->u.dinst.parm.buffer_mode != BUFFER_MODE_USE_BUF)
			PR_INF("%s:S4_DCOPY: need use exit buffer %d\n",
			       itf->u.dinst.parm.buffer_mode);
		/* fix setting */
		itf->tmode = EDIM_TMODE_2_PW_OUT;
		#else
		switch (itf->u.dinst.parm.buffer_mode) {
		case BUFFER_MODE_ALLOC_BUF:
			itf->tmode = EDIM_TMODE_3_PW_LOCAL;
			break;
		case BUFFER_MODE_USE_BUF:
			itf->tmode = EDIM_TMODE_2_PW_OUT;
			break;
		default:
			PR_ERR("%s:bmode[%d]\n", __func__,
			       itf->u.dinst.parm.buffer_mode);
			break;
		}
		#endif
	} else {
		PR_ERR("%s:wmode[%d]\n", __func__,
		       itf->u.dinst.parm.work_mode);
	}

	//reg:
	mutex_lock(&pch->itf.lock_reg);
	if (parm.output_format & DI_OUTPUT_BYPASS)
		itf->bypass_ext = true;
	pch->sum_reg_cnt++;
	dim_api_reg(DIME_REG_MODE_NEW, pch);
	npst_reset(pch);
	pch->itf.opins_m_back_in	= nins_m_recycle_ins;
	pch->itf.op_m_unreg		= nins_m_unreg_new;
	pch->ponly_set = false;
	pch->plink_dct = false;
	if (itf->tmode == EDIM_TMODE_3_PW_LOCAL) {
		itf->op_fill_ready	= ndis_fill_ready;
		itf->op_ready_out	= dip_itf_ndrd_ins_m2_out;
	} else {
		itf->op_fill_ready	= ndrd_m1_fill_ready;
		itf->op_ready_out	= dip_itf_ndrd_ins_m1_out;
	}

	if (itf->u.dinst.parm.work_mode == WORK_MODE_S4_DCOPY) {
		itf->op_cfg_ch_set	= cfg_ch_set_for_s4_cp;
		itf->flg_s4dw	= true;
		pch->s4dw	= &dim_s4dw_def;
	} else {
		itf->op_cfg_ch_set	= cfg_ch_set;
		itf->flg_s4dw	= false;
		pch->s4dw	= NULL;
	}
	//cfg_ch_set(pch);
	mutex_unlock(&pch->itf.lock_reg);
	PR_INF("%s:ch[%d],tmode[%d]\n", "ins:create", ch, itf->tmode);
	PR_INF("\tout:0x%x\n", itf->u.dinst.parm.output_format);
	return ch;
}

/**********************************************************
 **
 * @brief  di_set_parameter  set parameter to di for init
 *
 * @param[in]  index   instance index
 *
 * @return      0 for success, or fail type if < 0
 *
 **********************************************************/
int new_destroy_instance(int index)
{
	struct dim_itf_s *pintf;
	unsigned int ch;
	struct di_ch_s *pch;

	dbg_reg("%s:\n", __func__);
	ch = index_2_ch(index);
	if (ch == ERR_INDEX) {
		PR_ERR("%s:index overflow\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}

	pch = get_chdata(ch);

	pintf = &pch->itf;
	mutex_lock(&pch->itf.lock_reg);
	if (!pintf->reg) {
		PR_WARN("%s:double unreg\n", __func__);
		mutex_unlock(&pch->itf.lock_reg);
		return 0;
	}
	pintf->bypass_ext = false;
	pintf->reg = 0;
	//dip_event_unreg_chst(ch);
	dim_trig_unreg(ch);
	dim_api_unreg(DIME_REG_MODE_NEW, pch);
	mutex_unlock(&pch->itf.lock_reg);
	PR_INF("%s:ch[%d]:end\n", "ins:destroy", ch);
	return 0;
}

/**********************************************************
 **
 * @brief  di_empty_input_buffer  send input date to di
 *
 * @param[in]  index   instance index
 * @param[in]  buffer  Pointer of buffer structure
 *
 * @return      Success or fail type
 **********************************************************/
enum DI_ERRORTYPE new_empty_input_buffer(int index, struct di_buffer *buffer)
{
	struct dim_itf_s *pintf;
	unsigned int ch;
	struct di_ch_s *pch;
//	unsigned int buf_index = 0xff;
	//2020-12-10:
	unsigned int free_nub;
	struct buf_que_s *pbufq;
	struct dim_nins_s	*pins;
	bool flg_q, plink_dct = false;
	unsigned int bindex = 0;
//	unsigned int err;

	ch = index_2_ch(index);
	if (ch == ERR_INDEX) {
		PR_ERR("%s:index overflow\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}
	pch = get_chdata(ch);
#ifdef __HIS_CODE__
	if (pch->itf.pre_vpp_link && dpvpp_vf_ops()) {
		dpvpp_patch_first_buffer(pch->itf.p_itf);
		return dpvpp_empty_input_buffer(pch->itf.p_itf, buffer);
	}
#endif
	pintf = &pch->itf;
	if (!pintf->reg) {
		PR_WARN("%s:ch[%d] not reg\n", __func__, ch);
		return DI_ERR_INDEX_NOT_ACTIVE;
	}

	if (pintf->u.dinst.parm.work_mode == WORK_MODE_S4_DCOPY)
		return s4dw_empty_input(pch, buffer);

	#ifdef MARK_HIS
	qued_ops.peek(pch, QUED_T_IS_FREE, &buf_index);
	if (buf_index == 0xff) {
		PR_ERR("%s:no free\n", __func__);
		return DI_ERR_IN_NO_SPACE;
	}
	qued_ops.out(pch, QUED_T_IS_FREE, &buf_index);
	pintf->u.dinst.in[buf_index] = buffer;
	qued_ops.in(pch, QUED_T_IS_IN, buf_index);
	#endif

	//2020-12-10:
	pbufq = &pch->nin_qb;
	free_nub	= qbufp_count(pbufq, QBF_NINS_Q_IDLE);
	if (!free_nub) {
		PR_WARN("%s:no nins idle\n", __func__);
		return DI_ERR_IN_NO_SPACE;
	}

	/* get ins */
	flg_q = qbuf_out(pbufq, QBF_NINS_Q_IDLE, &bindex);
	if (!flg_q) {
		PR_ERR("%s:qout\n", __func__);
		return DI_ERR_IN_NO_SPACE;
	}
	if (buffer->vf)
		dbg_poll("ins:ch[%d] buffer[0x%px:0x%x]:vf[0x%px:0x%x],bindex=%d,free_nub=%d\n",
			ch, buffer, buffer->flag,
			buffer->vf, buffer->vf->type, bindex, free_nub);
	else
		dbg_poll("ins:ch[%d] buffer[0x%px:0x%x]:no vf\n",
			ch, buffer, buffer->flag);
	pins = (struct dim_nins_s *)pbufq->pbuf[bindex].qbc;
	pins->c.ori = buffer;
	//pins->c.etype = EDIM_NIN_TYPE_VFM;
	if (dip_itf_is_ins(pch) && dim_dbg_new_int(2))
		dim_dbg_buffer2(buffer, 0);
	pins->c.cnt = pch->in_cnt;
	pch->in_cnt++;

	if (buffer->flag & DI_FLAG_EOS) {
		if (buffer->vf) {
			memcpy(&pins->c.vfm_cp,
			       buffer->vf, sizeof(pins->c.vfm_cp));

			if (IS_I_SRC(pins->c.vfm_cp.type)) {
				PR_INF("ch[%d] in eos I\n", ch);
				pins->c.vfm_cp.type |= 0x3800;
			} else if (IS_COMP_MODE(pins->c.vfm_cp.type)) {
				PR_INF("ch[%d] in eos COMP\n", ch);
				pins->c.vfm_cp.type |= 0x9000;
			} else {
				pins->c.vfm_cp.type |= 0x9000;
				PR_INF("ch[%d] in eos p-other\n", ch);
			}
		} else {
			pins->c.vfm_cp.type |= VIDTYPE_INTERLACE_TOP;
						//test only
			pins->c.vfm_cp.type |= 0x3800;
			PR_INF("ch[%d] in eos vf null:0x%px\n", ch, buffer);
		}
		pins->c.vfm_cp.type |= VIDTYPE_V4L_EOS;
		pins->c.vfm_cp.width = 1920;
		pins->c.vfm_cp.height = 1080;
	} else if (!buffer->vf) {
		PR_ERR("%s:no vf\n", __func__);
	} else {
		memcpy(&pins->c.vfm_cp, buffer->vf, sizeof(pins->c.vfm_cp));
	}
	if (pins->c.vfm_cp.type & VIDTYPE_COMPRESS) {
		if (buffer->vf && is_src_crop_valid(buffer->vf->src_crop)) {
			buffer->vf->compHeight =
				buffer->vf->compHeight - buffer->vf->src_crop.bottom;
			pins->c.vfm_cp.compHeight =
				pins->c.vfm_cp.compHeight - pins->c.vfm_cp.src_crop.bottom;
		}
	}
	plink_dct = dip_plink_check_ponly_dct(pch, &pins->c.vfm_cp);
	dbg_poll("ins:plink_dct=%d,flg_q=%d\n", plink_dct, flg_q);

	if (plink_dct) /* for plink */
		flg_q = qbuf_in(pbufq, QBF_NINS_Q_DCT, bindex);
	else if (get_datal()->dct_op && get_datal()->dct_op->is_en(pch))
		flg_q = qbuf_in(pbufq, QBF_NINS_Q_DCT, bindex);
	else
		flg_q = qbuf_in(pbufq, QBF_NINS_Q_CHECK, bindex);
	sum_g_inc(ch);
	ATRACE_COUNTER("dim_in", pins->c.vfm_cp.index_disp);
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
	task_send_ready(20);
	return DI_ERR_NONE;
}

//@ary_note mem alloc by outside
static enum DI_ERRORTYPE di_fill_output_buffer_mode2(struct di_ch_s *pch,
						     struct di_buffer *buffer)
{
	//bool ret;

	#ifdef MARK_HIS //@ary_note todo
	/*from QUED_T_IS_PST_FREE to QUED_T_IS_PST_DOBEF */
	struct di_ch_s *pch;
	unsigned int buf_index = 0xff;
	struct dim_itf_s *pintf;

	pch = get_chdata(pintf->ch);
	#ifdef MARK_HIS
	/*que IS_PST*/
	if (!qued_ops.move(pch,
			   QUED_T_IS_PST_FREE,
			   QUED_T_IS_PST_DOBEF, &buf_index)) {
		PR_WARN("%s:ch[%d]:no space for 0x%p\n", __func__, ch, buffer);
		return DI_ERR_IN_NO_SPACE;
	}

	pintf->u.dinst.out[buf_index] = buffer;
	#else
	qued_ops.peek(pch, QUED_T_IS_PST_FREE, &buf_index);
	if (buf_index == 0xff) {
		PR_ERR("%s: NO PST_FREE\n", __func__);
		return DI_ERR_IN_NO_SPACE;
	}
	qued_ops.out(pch, QUED_T_IS_PST_FREE, &buf_index);
	pintf->u.dinst.out[buf_index] = buffer;
	qued_ops.in(pch, QUED_T_IS_PST_DOBEF, buf_index);

	#endif
	#endif //@ary_note todo
	if (dip_itf_is_ins(pch) && dim_dbg_new_int(2))
		dim_dbg_buffer2(buffer, 1);
	npst_qin(pch, buffer);

	pch->sum_ext_buf_in2++;
	return DI_ERR_NONE;
}

/* @ary_note: buffer alloc by di			*/
/* @ary_note: use this api to put back display buffer	*/
/* @ary_note: same as vfm put */
static enum DI_ERRORTYPE di_fill_output_buffer_mode3(struct di_ch_s *pch,
						     struct di_buffer *buffer)
{
	/*back buf to di */
	struct dim_itf_s *pintf;
//	unsigned int buf_index = 0xff;
	//struct di_buf_s *di_buf = NULL;
//tmp	ulong irq_flag2 = 0;
	unsigned int ch;
	struct dim_ndis_s *ndis1;

	//pch = get_chdata(ch);
	ch = pch->ch_id;
	pintf = &pch->itf;

	sum_pst_p_inc(ch);
	ndis1 = (struct dim_ndis_s *)buffer->private_data;
	if (IS_ERR_OR_NULL(pintf)) {
		/* bypass */
		PR_WARN("%s:bypass?\n", __func__);
		pintf->u.dinst.parm.ops.empty_input_done(buffer);
		sum_p_inc(ch);//
		return DI_ERR_NONE;
	}

	task_send_cmd2(ch,
			LCMD2(ECMD_RL_KEEP,
			     ch,
			     ndis1->header.index));
	//task_send_ready();
	return DI_ERR_NONE;
}

/**********************************************************
 * @brief  di_fill_output_buffer  send output buffer to di
 *
 * @param[in]  index   instance index
 * @param[in]  buffer  Pointer of buffer structure
 *
 * @return      Success or fail type
 *********************************************************/
enum DI_ERRORTYPE new_fill_output_buffer(int index, struct di_buffer *buffer)
{
	struct dim_itf_s *pintf;
	unsigned int ch;
	struct di_ch_s *pch;
	enum DI_ERRORTYPE ret = DI_ERR_NONE;

	/*check channel*/
	ch = index_2_ch(index);
	if (ch == ERR_INDEX) {
		PR_ERR("%s:index overflow\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}
	pch = get_chdata(ch);
	if (pch->itf.pre_vpp_link/* && dpvpp_vf_ops()*/)
		return dpvpp_fill_output_buffer(pch->itf.p_itf, buffer);
	pintf = &pch->itf;
	dim_print("%s:ch[%d],ptf ch[%d]\n", __func__, ch, pintf->ch);
	if (!pintf->reg) {
		PR_WARN("%s:ch[%d] not reg\n", __func__, ch);
		return DI_ERR_INDEX_NOT_ACTIVE;
	}

	if (pintf->tmode == EDIM_TMODE_2_PW_OUT) {
		//dbg:
		dim_dbg_buffer_ext(pch, buffer, 1);
		ret = di_fill_output_buffer_mode2(pch, buffer);
	} else if (pintf->tmode == EDIM_TMODE_3_PW_LOCAL) {
		ret = di_fill_output_buffer_mode3(pch, buffer);
	}
	task_send_ready(21);
	return ret;
}

int new_release_keep_buf(struct di_buffer *buffer)
{
	/*back buf to di */
//	struct dim_itf_s *pintf;
	struct di_ch_s *pch;
	unsigned int ch;
	struct dim_ndis_s *ndis1;

	if (!buffer) {
		PR_INF("%s:buffer\n", __func__);
		return -1;
	}

	if (buffer->mng.ch == DIM_PRE_VPP_NUB) {
		return dpvpp_fill_output_buffer2(buffer);
	}

	if (!buffer->private_data) {
		PR_INF("%s:no di data:0x%px\n", __func__, buffer);
		return -1;
	}

	ndis1 = (struct dim_ndis_s *)buffer->private_data;

	ch = buffer->mng.ch;
	pch = get_chdata(ch);

	sum_pst_p_inc(ch);

	task_send_cmd2(ch,
			LCMD2(ECMD_RL_KEEP,
			     ch,
			     ndis1->header.index));
	return 0;
}

/**********************************************************
 * @brief  di_get_output_buffer_num  get output buffer num
 *
 * @param[in]  index   instance index
 * @param[in]  buffer  Pointer of buffer structure
 *
 * @return      number or fail type
 *********************************************************/
int new_get_output_buffer_num(int index)
{
	struct dim_itf_s *pintf;
	unsigned int ch = 0;
	struct di_ch_s *pch;
	int ret = -1;

	dbg_reg("%s:\n", __func__);
	ch = index_2_ch(index);
	if (ch == ERR_INDEX) {
		PR_ERR("%s:index overflow\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}

	pch = get_chdata(ch);
	pintf = &pch->itf;
	if (pintf->tmode == EDIM_TMODE_3_PW_LOCAL)
		ret = DIM_NDIS_NUB;
	dbg_reg("%s:end\n", __func__);
	return ret;
}

/**********************************************************
 * @brief  di_get_input_buffer_num  get input buffer num
 *
 * @param[in]  index   instance index
 * @param[in]  buffer  Pointer of buffer structure
 *
 * @return      number or fail type
 *********************************************************/
int new_get_input_buffer_num(int index)
{
	struct dim_itf_s *pintf;
	unsigned int ch = 0;
	struct di_ch_s *pch;
	int ret = -1;

	dbg_reg("%s:\n", __func__);
	ch = index_2_ch(index);
	if (ch == ERR_INDEX) {
		PR_ERR("%s:index overflow\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}

	pch = get_chdata(ch);
	pintf = &pch->itf;
	if (pintf->tmode == EDIM_TMODE_3_PW_LOCAL)
		ret = DIM_NINS_NUB;
	dbg_reg("%s:end\n", __func__);
	return ret;
}

int di_ls_bypass_ch(int index, bool on)
{
	struct dim_itf_s *pintf;
	unsigned int ch = 0;
	struct di_ch_s *pch;

	dbg_reg("%s:\n", __func__);
	ch = index_2_ch(index);
	if (ch == ERR_INDEX) {
		PR_ERR("%s:index overflow\n", __func__);
		return DI_ERR_INDEX_OVERFLOW;
	}
	pch = get_chdata(ch);
	mutex_lock(&pch->itf.lock_reg);
	pintf = &pch->itf;
	pintf->bypass_ext = on;
	mutex_unlock(&pch->itf.lock_reg);
	PR_INF("%s:ch[%d]:%d\n", __func__, pch->ch_id, on);
	return DI_ERR_NONE;
}

