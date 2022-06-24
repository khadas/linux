// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/uaccess.h>
#include <linux/of_fdt.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/of_device.h>

#include <linux/amlogic/media/vfm/vframe.h>

/*dma_get_cma_size_int_byte*/
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/codec_mm_keeper.h>

#include "deinterlace_dbg.h"
#include "deinterlace.h"
#include "di_data_l.h"
#include "di_data.h"
#include "di_dbg.h"
#include "di_vframe.h"
#include "di_task.h"
#include "di_prc.h"
#include "di_sys.h"
#include "di_api.h"
#include "di_que.h"
//#include "sc2/di_hw_ex_v3.h"

#include "register.h"
#include "di_mem_sct_def.h"

/************************************
 * bit0: no tail
 * bit1: some information for each frame
 * bit2: clear for first
 * bit3: clear for each frame
 * bit4: used decoder buffer;
 * bit5: to sum;
 ************************************/

static u32 dbg_sct_cfg = DI_BIT5 | DI_BIT6;// =  BITS_EAFBC_CFG_4K;

module_param_named(dbg_sct_cfg, dbg_sct_cfg, uint, 0664);

bool dbg_sct_used_decoder_buffer(void)
{
	if (dbg_sct_cfg & DI_BIT4)
		return true;
	return false;
}

bool dbg_sct_printa(void)
{
	if (dbg_sct_cfg & DI_BIT1)
		return true;
	return false;
}

static bool dbg_sct_clear_first(void)
{
	if (dbg_sct_cfg & DI_BIT2)
		return true;
	return false;
}

bool dbg_sct_clear_by_frame(void)
{
	if (dbg_sct_cfg & DI_BIT3)
		return true;
	return false;
}

static bool dbg_sct_sum(void)
{
	if (dbg_sct_cfg & DI_BIT5)
		return true;
	return false;
}

static bool dbg_limit_ready(void)
{
	if (dbg_sct_cfg & DI_BIT6)
		return true;
	return false;
}

struct dim_msct_s {
	void *box;
	unsigned int flg_err;
	unsigned int max_nub;
	/*test*/
	unsigned int cnt_alloc;
	struct dim_pat_s *pat_buf;
	unsigned long tab_addr;
	unsigned int *tab_vaddr;
	union {
		unsigned int d32;
		struct {
		unsigned int box	: 1,
			pat	: 1,
			vbit	: 1, /*vmap flg*/
			tab	: 1,
			tab_resize	: 1,
			flg_rev	:27;
		} b;
	} flg_act;
};

static struct dim_msct_s dim_sct;

static struct dim_msct_s *get_msct(void)
{
	return &dim_sct;
}

static void sct_alloc(struct di_ch_s *pch,
		      unsigned int buffer_size,
		      unsigned int idx)
{
	int ret;
	struct dim_msct_s *psct = get_msct();
	int cur_mmu_4k_number;
	//unsigned int *vaddr;
	//struct di_mm_s *mm;
	//bool flg_vmap;
	u64 timer_st, timer_end, diff;

	timer_st = cur_to_usecs();

	cur_mmu_4k_number = ((buffer_size + (1 << 12) - 1) >> 12);

	ret = di_mmu_box_alloc_idx(psct->box,
				   idx,
				   cur_mmu_4k_number,
				   psct->tab_vaddr);
	if (ret == 0) {
		psct->flg_act.b.tab = 1;
		psct->flg_act.b.tab_resize = 0;
	} else {
		bset(&psct->flg_err, EDIM_SCT_ERR_ALLOC);
		PR_ERR("%s:\n", __func__);
	}

	timer_end = cur_to_usecs();
	diff = timer_end - timer_st;
	dbg_sct("%s:use %uus\n", __func__, (unsigned int)diff);
	dim_tr_ops.sct_alloc(idx, timer_st);
}

static void sct_free_tail(struct di_ch_s *pch,
		      unsigned int buffer_used,
		      unsigned int idx)

{
	struct dim_msct_s *psct = get_msct();
	int ret;
	u64 timer_st, timer_end, diff;

	timer_st = cur_to_usecs();
	ret = di_mmu_box_free_idx_tail(psct->box,
				       idx,
				       buffer_used);
	if (ret == 0) {
		psct->flg_act.b.tab_resize = 1;
	} else {
		bset(&psct->flg_err, EDIM_SCT_ERR_RESIZE);
		PR_ERR("%s:\n", __func__);
	}
	timer_end = cur_to_usecs();
	diff = timer_end - timer_st;
	dbg_sct("%s:use %uus\n", __func__, (unsigned int)diff);
}

static void sct_free(unsigned int idx)
{
	int ret;
	struct dim_msct_s *psct = get_msct();
	u64 timer_st, timer_end, diff;

	timer_st = cur_to_usecs();

	ret = di_mmu_box_free_idx(psct->box, idx);
	if (ret == 0) {
		psct->flg_act.b.tab_resize = 0;
		psct->flg_act.b.tab = 0;
	} else {
		bset(&psct->flg_err, EDIM_SCT_ERR_FREE);
		PR_ERR("%s:\n", __func__);
	}

	timer_end = cur_to_usecs();
	diff = timer_end - timer_st;
	dbg_sct("%s:use %uus\n", __func__, (unsigned int)diff);
}

static void sct_reg(struct di_ch_s *pch)
{
	unsigned int ch;
	struct dim_msct_s *psct = get_msct();
	int buf_size = 64; /*?*/
	unsigned int max_num;
	bool tvp;
	struct dim_pat_s *pat_buf;
	struct div2_mm_s *mm;
	bool flg_vmap = false;

	ch = pch->ch_id;
	max_num = 2;
	psct->max_nub = max_num;
	tvp = 0;
	psct->box = di_mmu_box_alloc_box(DEVICE_NAME,
					      ch,
					      max_num,
					      buf_size * SZ_1M,
					      tvp);
	if (!psct->box) {
		bset(&psct->flg_err, EDIM_SCT_ERR_BOX);
		PR_ERR("%s\n", __func__);
		return;
	}
	pat_buf = qpat_out_ready(pch);
	if (!pat_buf) {
		bset(&psct->flg_err, EDIM_SCT_ERR_PAT);
		di_mmu_box_free(psct->box);
		psct->box = NULL;
		return;
	}
	/*vmap*/
	mm = dim_mm_get(ch);
	psct->tab_vaddr = (unsigned int *)dim_vmap(psct->tab_addr,
					mm->cfg.size_pafbct_one,
					&flg_vmap);
	if (!psct->tab_vaddr) {
		bset(&psct->flg_err, EDIM_SCT_ERR_VMAP);
		PR_ERR("%s:vmap:0x%lx\n", __func__, psct->tab_addr);
		return;
	}
	psct->flg_act.b.vbit = flg_vmap;

	psct->tab_addr	= pat_buf->mem_start;
	psct->pat_buf	= pat_buf;

	psct->flg_act.b.box = 1;
	psct->flg_act.b.pat = 1;
}

static void sct_unreg(struct di_ch_s *pch)
{
	struct dim_msct_s *psct = get_msct();

	/**/
	if (psct->flg_act.b.tab)
		sct_free(0);

	/* vmap */
	if (psct->flg_act.b.vbit) {
		dim_unmap_phyaddr((u8 *)psct->tab_vaddr);
		psct->tab_vaddr = NULL;
	}

	if (psct->flg_act.b.pat) {
		qpat_in_ready(pch, psct->pat_buf);
		psct->pat_buf = NULL;
		psct->flg_act.b.pat = 0;
	}

	if (psct->flg_act.b.box) {
		di_mmu_box_free(psct->box);
		psct->box = NULL;
		psct->flg_act.b.box = 0;
	}
	PR_INF("%s:end\n", __func__);
}

void tst_alloc(struct di_ch_s *pch)
{
	struct dim_msct_s *psct = get_msct();
	struct div2_mm_s *mm;
	unsigned int ch;

	ch = pch->ch_id;
	mm = dim_mm_get(ch);

	if (psct->flg_err) {
		dbg_sct("%s:0x%x\n", __func__, psct->flg_err);
		return;
	}
	if (psct->flg_act.b.tab)
		return;
	sct_alloc(pch, mm->cfg.pst_buf_size, 0);
}

void tst_resize(struct di_ch_s *pch, unsigned int used_size)
{
	struct dim_msct_s *psct = get_msct();
	struct div2_mm_s *mm;
	unsigned int ch;

	ch = pch->ch_id;
	mm = dim_mm_get(ch);

	if (psct->flg_err) {
		dbg_sct("%s:0x%x\n", __func__, psct->flg_err);
		return;
	}

	if (!psct->flg_act.b.tab || psct->flg_act.b.tab_resize)
		return;
	sct_free_tail(pch, used_size, 0);
}

void tst_release(struct di_ch_s *pch)
{
	struct dim_msct_s *psct = get_msct();

	if (psct->flg_err) {
		dbg_sct("%s:0x%x\n", __func__, psct->flg_err);
		return;
	}

	if (!psct->flg_act.b.tab)
		return;
	if (!psct->flg_act.b.tab_resize)
		return;

	sct_free(0);
}

void tst_unreg(struct di_ch_s *pch)
{
	sct_unreg(pch);
}

void tst_reg(struct di_ch_s *pch)
{
//	struct dim_msct_s *psct = get_msct();
	sct_reg(pch);
}

void sct_max_check(struct di_ch_s *pch)
{
	unsigned int ch;
	struct dim_mscttop_s *psct;
	struct dim_msc_sum_s *psum;
	//unsigned int i;
	//struct qs_cls_s *p;
	//unsigned int psize;
	//unsigned int cnt_pst_ready, cnt_pst_dis, cnt_pst_back;
	//unsigned int cnt_sct_ready, cnt_sct_used,cnt_sct_re;
	struct buf_que_s *pbufq;
	//struct dim_sct_s *sct;
	//unsigned int tt_size;

	if (!dbg_sct_sum())
		return;

	ch = pch->ch_id;
	psct = &pch->msct_top;
	psum = &psct->sum;
	if (!psct->box)
		return;

	pbufq = &pch->sct_qb;
	psum->mts_sct_rcc	= qbufp_count(pbufq, QBF_SCT_Q_RECYCLE);
	psum->mts_sct_used	= qbufp_count(pbufq, QBF_SCT_Q_USED);
	psum->mts_sct_ready	= qbufp_count(pbufq, QBF_SCT_Q_READY);

	psum->mts_pst_free		= di_que_list_count(ch, QUE_POST_FREE);
	psum->mts_pst_ready		= ndrd_cnt(pch);
	//di_que_list_count(ch, QUE_POST_READY);
	psum->mts_pst_ready		= di_que_list_count(ch, QUE_POST_READY);
	psum->mts_pst_back		= di_que_list_count(ch, QUE_POST_BACK);
	psum->mts_pst_dispaly		= list_count(ch, QUEUE_DISPLAY);
}

/*because sct is not sync with reg/unreg */
void sct_sw_on(struct di_ch_s *pch,
		unsigned int max_num,
		bool tvp,
		unsigned int buffer_size)
{
	unsigned int ch;
	struct dim_mscttop_s *psct;
	int buf_size = 64; /*?*/

//	struct dim_pat_s *pat_buf;
//	struct di_mm_s *mm;
//	bool flg_vmap = false;

	ch = pch->ch_id;
	psct = &pch->msct_top;
	if (psct->box) {
		PR_WARN("%s:box is exist\n", __func__);
		return;
	}
	/*int*/
	memset(&psct->sum, 0, sizeof(psct->sum));

	psct->max_nub = max_num;

	psct->box = di_mmu_box_alloc_box(DEVICE_NAME,
					      ch,
					      max_num,
					      buf_size * SZ_1M,
					      tvp);
	if (!psct->box) {
		bset(&psct->flg_err, EDIM_SCT_ERR_BOX);
		PR_ERR("%s\n", __func__);
		return;
	}
	psct->buffer_size = buffer_size;
	psct->buffer_size_nub = ((psct->buffer_size + (1 << 12) - 1) >> 12);
	psct->flg_act_box = 1;

	dbg_sct("%s:ch[%d], nub[%d] tvp[%d], buf_size[0x%x]\n",
		__func__, ch, max_num, tvp, buffer_size);
}

void sct_sw_off(struct di_ch_s *pch)
{
	unsigned int ch;
	struct dim_mscttop_s *psct;
	struct buf_que_s *pbufq;
	//union q_buf_u q_buf;// = NULL;
	struct dim_sct_s *sct;
	int i;
	unsigned int len;
	bool ret;

	ch = pch->ch_id;
	psct = &pch->msct_top;
	if (!psct->box) {
		dbg_reg("%s:no box\n", __func__);
		return;
	}

	pbufq = &pch->sct_qb;
	/* ready clear */
	len = qbufp_count(pbufq, QBF_SCT_Q_READY);
	if (len) {
		for (i = 0; i < len; i++) {
			ret = qsct_any_to_recycle(pch, QBF_SCT_Q_READY, &sct);
			if (!ret) {
				PR_ERR("%s:used[%d][%d]\n", __func__, len, i);
				break;
			}
		}
	}
	/* used clear */
	len = qbufp_count(pbufq, QBF_SCT_Q_USED);
	if (len) {
		for (i = 0; i < len; i++) {
			ret = qsct_any_to_recycle(pch, QBF_SCT_Q_USED, &sct);
			if (!ret) {
				PR_ERR("%s:used[%d][%d]\n", __func__, len, i);
				break;
			}
		}
	}
	/* recycle clear */
	len = qbufp_count(pbufq, QBF_SCT_Q_RECYCLE);
	if (len) {
		for (i = 0; i < len; i++) {
			ret = qsct_recycle_to_idle(pch, &sct);
			if (!ret) {
				PR_ERR("%s:rcy[%d][%d]\n", __func__, len, i);
				break;
			}
//			if (!sct->flg_act.b.tab_keep)
			/* pat */
//			pat_release_vaddr(sct->pat_buf);
//			qpat_in_ready(pch,sct->pat_buf);
//			sct->pat_buf = NULL;
			sct_free_l(pch, sct);
		}
	}
	bufq_sct_rest(pch);

	di_mmu_box_free(psct->box);
		psct->box = NULL;
		psct->flg_act_box = 0;
		psct->max_nub = 0;
		PR_INF("%s:release\n", __func__);
}

//static
unsigned int sct_cnt_crc(struct device *dev,
				unsigned int *p,
				unsigned int buf_size)
{
	//bool flg = false; /*dbg*/

	int i, cnt;
//	unsigned int body;
	unsigned int crc = 0;
	unsigned long crc_tmp = 0;

	cnt = (buf_size + 0xfff) >> 12;

	for (i = 0; i < cnt; i++) {
		crc_tmp += *(p + i);
		/*debug*/
		if ((dbg_sct_cfg & DI_BIT8) && i < 5)
			PR_INF("\t:0x%x\n", *(p + i));
	}
	//PR_INF("%s:3\n", __func__);
	crc = (unsigned int)crc_tmp;
	if (dbg_sct_cfg & DI_BIT8)
		PR_INF("%s:0x%px:crc:0x%x, cnt[0x%x]\n", __func__, p, crc, cnt);

	return crc;
}

static void sct_alloc_l(struct di_ch_s *pch,
		      struct dim_sct_s *sct)
{
	int ret;
	struct dim_mscttop_s *psct;
	int cur_mmu_4k_number;
	//unsigned int *vaddr;
	//struct di_mm_s *mm;
	//bool flg_vmap;
	u64 timer_st, timer_end, diff;
	struct dim_msc_sum_s *psum;

	psct = &pch->msct_top;
	psum = &psct->sum;
	timer_st = cur_to_usecs();

	cur_mmu_4k_number = psct->buffer_size_nub;
	//((psct->buffer_size + (1 << 12) - 1) >> 12);

	ret = di_mmu_box_alloc_idx
			(psct->box,
			 sct->header.index,
			 cur_mmu_4k_number,
			 sct->pat_buf->vaddr);
	if (ret == 0) {
		sct->flg_act.b.tab = 1;
		sct->tail_cnt = cur_mmu_4k_number;
		/* sum */
		psum->curr_nub++;
		if (psum->curr_nub > psum->max_nub)
			psum->max_nub = psum->curr_nub;
		psum->curr_tt_size += cur_mmu_4k_number;
		if (psum->max_tt_size2 < psum->curr_tt_size) {
			psum->max_tt_size2 = psum->curr_tt_size;
			sct_max_check(pch);
		}
	} else {
		bset(&psct->flg_err, EDIM_SCT_ERR_ALLOC);
		PR_ERR("%s:\n", __func__);
	}
	#ifdef HIS_CODE
	sct->pat_buf->crc = sct_cnt_crc(NULL,
					(unsigned int *)sct->pat_buf->vaddr,
					psct->buffer_size);
	#endif
	timer_end = cur_to_usecs();
	diff = timer_end - timer_st;
	#ifdef HIS_CODE
	dbg_sct("%s:pat:[idx=%d]:addr=0x%lx, vaddr=0x%px\n",
		__func__,
		sct->pat_buf->header.index,
		sct->pat_buf->mem_start,
		sct->pat_buf->vaddr);
	dbg_sct("%s:use %uus\n", __func__, (unsigned int)diff);
	#endif
	dim_tr_ops.sct_alloc(sct->header.index, timer_st);
}

void sct_free_tail_l(struct di_ch_s *pch,
		      unsigned int buffer_used,
		      struct dim_sct_s *sct)

{
	struct dim_mscttop_s *psct;
	int ret;
	u64 timer_st, timer_end, diff;
	struct dim_msc_sum_s *psum;

	if (!sct || !pch) {
		PR_ERR("%s:no sct\n", __func__);
		return;
	}

	if (dbg_sct_cfg & DI_BIT0)
		return;

	timer_st = cur_to_usecs();
	psct = &pch->msct_top;
	psum = &psct->sum;
	buffer_used += 3;
	if (buffer_used > psct->buffer_size_nub) {
		PR_ERR("%s:overflow:0x%x\n", __func__, buffer_used);
		return;
	}
	ret = di_mmu_box_free_idx_tail(psct->box,
				       sct->header.index,
				       buffer_used);
	if (ret == 0) {
		sct->flg_act.b.tab_resize = 1;
		/*sum*/
		//psum->max_nub--;
		psum->curr_tt_size -= (psct->buffer_size_nub - buffer_used);
		if (buffer_used > psum->max_size)
			psum->max_size = buffer_used;
	} else {
		bset(&psct->flg_err, EDIM_SCT_ERR_RESIZE);
		PR_ERR("%s:\n", __func__);
	}
	sct->tail_cnt = buffer_used;
	timer_end = cur_to_usecs();
	diff = timer_end - timer_st;
	dim_tr_ops.sct_tail(sct->header.index, buffer_used);
	//dbg_sct("%s:use %uus 0x%x\n", __func__, (unsigned int)diff, buffer_used);
}

void sct_free_l(struct di_ch_s *pch, struct dim_sct_s *sct)
{
	int ret;
	struct dim_mscttop_s *psct;
	//u64 timer_st, timer_end, diff;
	struct dim_msc_sum_s *psum;

	if (!pch || !sct)
		return;
	psct = &pch->msct_top;
	psum = &psct->sum;
	//timer_st = cur_to_usecs();
	if (sct->header.index > psct->max_nub) {
		PR_ERR("%s:sct[%d]\n", __func__, sct->header.index);
		return;
	}
	ret = di_mmu_box_free_idx(psct->box, sct->header.index);
	if (ret == 0) {
		sct->flg_act.b.tab_resize = 0;
		sct->flg_act.b.tab = 0;

		psum->curr_nub--;
		psum->curr_tt_size -= sct->tail_cnt;
	} else {
		bset(&psct->flg_err, EDIM_SCT_ERR_FREE);
		PR_ERR("%s:\n", __func__);
	}

	//timer_end = cur_to_usecs();
	//diff = timer_end - timer_st;
	#ifdef HIS_CODE
	dbg_sct("%s:ch[%d], index[%d]\n",
		__func__,
		pch->ch_id,
		sct->header.index);
	dbg_sct("%s:use %uus\n", __func__, (unsigned int)diff);
	#endif
}

unsigned int sct_keep(struct di_ch_s *pch, struct dim_sct_s *sct)
{
	struct dim_mscttop_s *psct;
	void *mem_handle;
	int ret;

	if (!pch || !sct)
		return DIM_KEEP_NONE;
	psct = &pch->msct_top;

	mem_handle =
		di_mmu_box_get_mem_handle
			(psct->box, sct->header.index);
	dbg_mem2("scta:keep:0x%px\n", mem_handle);
	ret = codec_mm_keeper_mask_keep_mem(mem_handle,
		MEM_TYPE_CODEC_MM_SCATTER);
	dbg_mem2("scta:keep:nub:%d\n", ret);
	#ifdef HIS_CODE
	vf->mem_head_handle =
		decoder_bmmu_box_get_mem_handle
			(hevc->bmmu_box, VF_BUFFER_IDX(pic->BUF_index));
	#endif
	if (ret >= 0)
		return ret;
	PR_WARN("%s:overflow:%d\n", __func__, ret);
	return DIM_KEEP_NONE;
}

static void sct_alloc_in_poling(unsigned int ch)
{
	struct di_ch_s		*pch;
	struct dim_mscttop_s	*psct;
	unsigned int		cnt_sct_req;
	struct buf_que_s	*pbufq;
	int i;
	struct dim_sct_s *sct, *sct2;
	//struct dim_pat_s *pat_buf = NULL;

	pch = get_chdata(ch);
	if (!pch)
		return;
	psct = &pch->msct_top;
	if (!psct->box)
		return;
	pbufq = &pch->sct_qb;
	cnt_sct_req	= qbufp_count(pbufq, QBF_SCT_Q_REQ);

	if (!cnt_sct_req)
		return;
	psct->flg_allocing = 1;
	for (i = 0; i < cnt_sct_req; i++) {
		/* peek */

		mutex_lock(&psct->lock_ready);
		sct = qsct_req_peek(pch);
		mutex_unlock(&psct->lock_ready);
		if (!sct)
			break;

		/* alloc */
		sct_alloc_l(pch, sct);

		/* req to ready */
		mutex_lock(&psct->lock_ready);
		if (sct->flg_act.b.tab) {
			qsct_req_to_ready(pch, &sct2);
			if (sct != sct2)
				PR_ERR("%s:not same\n", __func__);
		}
		mutex_unlock(&psct->lock_ready);

		/* to-do list */
		/* if unreg or change mode */
		if (psct->flg_trig_dis) {
			psct->flg_trig_dis = 0;
			break;
		}
	}
	psct->flg_allocing = 0;
}

static void pat_set_vaddr(struct dim_pat_s *pat, unsigned int buf_size)
{
	bool flg_vmap = false;

	if (!pat)
		return;

	if (pat->vaddr) {
		PR_WARN("%s:vaddr exist[%d]\n", __func__, pat->header.index);
		return;
	}

	pat->vaddr = (unsigned int *)dim_vmap(pat->mem_start,
					buf_size,
					&flg_vmap);
	if (!pat->vaddr) {
		//bset(&psct->flg_err, EDIM_SCT_ERR_VMAP);
		PR_ERR("%s:vmap:0x%lx\n", __func__, pat->mem_start);
		return;
	}
	pat->flg_vmap = flg_vmap;
}

void pat_release_vaddr(struct dim_pat_s *pat)
{
	if (!pat)
		return;
	if (pat->vaddr && pat->flg_vmap)
		dim_unmap_phyaddr((u8 *)pat->vaddr);
	pat->vaddr = NULL;
}

/* when from 4k to other size use this to recycle 4k buffer */
void sct_mng_working_recycle(struct di_ch_s *pch)
{
	struct div2_mm_s *mm;
	unsigned int cnt_recycle, cnt_idle;
	struct buf_que_s *pbufq;
	struct dim_sct_s *sct;
	struct dim_mscttop_s	*pmsct;
	int i;

	pmsct = &pch->msct_top;

	if (!pmsct->box)
		return;

	mm = dim_mm_get(pch->ch_id);
	if (mm->cfg.pbuf_flg.b.typ == EDIM_BLK_TYP_PSCT)
		return;

	pbufq = &pch->sct_qb;
	/* recycle */
	cnt_recycle	= qbufp_count(pbufq, QBF_SCT_Q_RECYCLE);
	if (!cnt_recycle)
		return;

	for (i = 0; i < cnt_recycle; i++) {
		qsct_recycle_to_idle(pch, &sct);

		/* pat */
		pat_release_vaddr(sct->pat_buf);
		qpat_in_ready(pch, sct->pat_buf);
		sct->pat_buf = NULL;
		sct_free_l(pch, sct);
	}

	cnt_idle	= qbufp_count(pbufq, QBF_SCT_Q_IDLE);

	if (cnt_idle == DIM_SCT_NUB &&
	    pmsct->box) {
		di_mmu_box_free(pmsct->box);
		pmsct->box = NULL;
		pmsct->flg_act_box = 0;
		pmsct->max_nub = 0;
		dbg_sct("%s:release\n", __func__);
	} else {
		dbg_sct("%s:cnt_idle=%d\n", __func__, cnt_idle);
	}
}

#define DIM_SCT_KEEP_READY	(1)
static void sct_mng_working(struct di_ch_s *pch)
{
	unsigned int ch;
	unsigned int cnt_pst_free, cnt_sct_ready, cnt_sct_req;
	unsigned int cnt_idle, cnt_wait, need_req, req_new = 0;
	unsigned int cnt_recycle, ready_set, cnt_pst_ready;
	struct buf_que_s *pbufq, *pbufq_dis;
	struct dim_mscttop_s	*pmsct;
	struct dim_sct_s *sct;
	//bool f_no_res = false;
	bool f_req = false;
	bool f_no_wbuf = false;
	bool ret;
	int i;
	unsigned int err = 0;
	struct di_mng_s *pbm = get_bufmng();
	struct dim_pat_s *pat_buf = NULL;
	struct div2_mm_s *mm;
	struct di_buf_s *di_buf = NULL;
	struct di_dev_s *devp = get_dim_de_devp();
	struct di_pre_stru_s *ppre;
	unsigned int frame_nub;
	unsigned int cnt_display = 0;

	if (!pch)
		return;

	pmsct = &pch->msct_top;
	if (!pmsct->box)
		return;

	ch = pch->ch_id;

	if (atomic_read(&pbm->trig_unreg[ch]))
		return;

	mm = dim_mm_get(pch->ch_id);
	if (mm->cfg.pbuf_flg.b.typ != EDIM_BLK_TYP_PSCT)
		return;

	pbufq = &pch->sct_qb;

	ppre = get_pre_stru(ch);
	if (!ppre) {
		PR_ERR("%s:no ppre\n", __func__);
		return;
	}

	frame_nub = ppre->field_count_for_cont;

	/* recycle */
	cnt_recycle	= qbufp_count(pbufq, QBF_SCT_Q_RECYCLE);
	for (i = 0; i < cnt_recycle; i++) {
		qsct_recycle_to_idle(pch, &sct);

		/* pat */
		pat_release_vaddr(sct->pat_buf);
		qpat_in_ready(pch, sct->pat_buf);
		sct->pat_buf = NULL;
		sct_free_l(pch, sct);
	}

	/*summary */
	cnt_pst_free	= di_que_list_count(ch, QUE_POST_FREE);
	cnt_pst_ready	= ndrd_cnt(pch);//di_que_list_count(ch, QUE_POST_READY);
	cnt_idle	= qbufp_count(pbufq, QBF_SCT_Q_IDLE);
	cnt_wait	= di_que_list_count(ch, QUE_PST_NO_BUF_WAIT);
	mutex_lock(&pmsct->lock_ready);
	cnt_sct_ready	= qbufp_count(pbufq, QBF_SCT_Q_READY);
	cnt_sct_req	= qbufp_count(pbufq, QBF_SCT_Q_REQ);
	mutex_unlock(&pmsct->lock_ready);

	pbufq_dis = &pch->ndis_qb;
	cnt_display = qbufp_count(pbufq_dis, QBF_NDIS_Q_DISPLAY);

	ready_set = cnt_sct_ready;
	if (cnt_sct_ready && cnt_wait < cnt_sct_ready) {
		//PR_INF("cnt_wait:%d->%d\n", cnt_wait, cnt_sct_ready);
		//cnt_sct_ready = cnt_wait;
		ready_set = cnt_wait;
	}
	if (!cnt_wait)
		f_no_wbuf = true;

	if (ready_set) {
		/* ready to used */
		for (i = 0; i < ready_set; i++) {
			ret = qsct_ready_to_used(pch, &sct);
			if (!ret) {
				bset(&pmsct->flg_err, EDIM_SCT_ERR_QUE_2USED);
				err++;
				PR_ERR("%s:ready 2 used no sct\n", __func__);
				break;
			}
			/*flash*/
			pat_frash(&devp->pdev->dev,
						pch,
						sct->pat_buf);
			/* set free buffer*/
			di_buf = di_que_out_to_di_buf(ch, QUE_PST_NO_BUF_WAIT);
			if (!di_buf) {
				di_que_in(ch, QUE_PST_NO_BUF_WAIT, di_buf);
				break;
			}
			di_buf->blk_buf->pat_buf = sct->pat_buf;
			di_buf->blk_buf->sct	= sct;

			di_buf->afbct_adr	= sct->pat_buf->mem_start;
			di_que_in(ch, QUE_POST_FREE, di_buf);
			cnt_pst_free++;
			cnt_sct_ready--;
		}
		//cnt_sct_ready = 0;
	}

	if (cnt_pst_free >= DIM_SCT_KEEP_READY ||
	    (dbg_limit_ready() && cnt_pst_ready >= 3)) {
		if (pmsct->flg_no_buf) {
			pmsct->flg_no_buf = 0;
			pch->rsc_bypass.b.no_buf = 0;/*tmp*/
			PR_WARN("no buffer:recover[%d]\n", frame_nub);
		}
		return;
	}
	if ((cnt_pst_free + cnt_sct_req + cnt_sct_ready) >= DIM_SCT_KEEP_READY)
		return;

	need_req = DIM_SCT_KEEP_READY - cnt_pst_free - cnt_sct_req - cnt_sct_ready;

	if (cnt_idle >= need_req) {
		f_req = true;
	} else if (cnt_idle > 0) {
		need_req = cnt_idle;
		f_req = true;
	}

	if (f_req /*&& pat_buf && pat_buf->vaddr*/) {
		/* required */

		for (i = 0; i < need_req; i++) {
			pat_buf = qpat_out_ready(pch);
			//mm = dim_mm_get(pch->ch_id);
			if (pat_buf) {
				pat_set_vaddr(pat_buf,
					      mm->cfg.pst_afbct_size);
				//#if 1
				/* cash */
				if ((dbg_sct_clear_first() && !pat_buf->flg_mode) ||
				    dbg_sct_clear_by_frame()) {
					pat_clear_mem(&devp->pdev->dev,
						pch,
						pat_buf);
					pat_buf->flg_mode = 1;
				}
				//#endif
			} else {
				PR_ERR("%s:no pat\n", __func__);
				err++;
				break;
			}

			mutex_lock(&pmsct->lock_ready);
			ret = qsct_idle_to_req(pch, &sct);
			sct->pat_buf = pat_buf;
			sct->flg_act.b.pat = 1;

			mutex_unlock(&pmsct->lock_ready);
			if (!ret) {
				bset(&pmsct->flg_err, EDIM_SCT_ERR_QUE_2REQ);
				err++;
				break;
			}
			cnt_sct_req++;
			req_new++;
		}
		//if (req_new)
		//	mtask_wake_for_sct();
	} else if (cnt_pst_ready >= 1) {
	} else if (dip_itf_is_ins_lbuf(pch) &&
		   cnt_display >= (pmsct->max_nub - 3)) {
	} else {
		/* no resource */
		if (f_no_wbuf)
			PR_WARN("no_buf:0:cnt_wait:%d[%d]\n", cnt_sct_ready, frame_nub);

		if (pch->rsc_bypass.b.no_buf) {
			if (time_after_eq(jiffies,
			   pmsct->jiff_no_buf + HZ * 3)) {
			   //print:
				PR_WARN("no buffer:3:[%d]\n", frame_nub);
				pmsct->jiff_no_buf = jiffies;
			}
		} else if (pmsct->flg_no_buf) {
			if (time_after_eq(jiffies,
			   pmsct->jiff_no_buf + HZ)) {
				pch->rsc_bypass.b.no_buf = 1;
				PR_WARN("no_buf:2:[%d]\n", frame_nub);
			}
		} else {
			pmsct->flg_no_buf = 1;
			pmsct->jiff_no_buf = jiffies;
			PR_WARN("no_buf:1:[%d:%d:%d:%d]\n",
				frame_nub,
				cnt_pst_free,
				cnt_idle,
				cnt_sct_req);
		}
	}
	if (err)
		pch->rsc_bypass.b.scr_err = 1;
}

/* now no use */
void sct_mng_idle(struct di_ch_s *pch)
{
	struct dim_mscttop_s	*pmsct;
	unsigned int cnt_recycle, cnt_idle;
	struct buf_que_s *pbufq;
	struct dim_sct_s *sct;
	int i;

	pmsct = &pch->msct_top;

	if (!pmsct->box)
		return;

	/* recycle */
	pbufq = &pch->sct_qb;
	cnt_recycle	= qbufp_count(pbufq, QBF_SCT_Q_RECYCLE);
	if (!cnt_recycle)
		return;
	for (i = 0; i < cnt_recycle; i++) {
		qsct_recycle_to_idle(pch, &sct);

		/* pat */
		pat_release_vaddr(sct->pat_buf);
		qpat_in_ready(pch, sct->pat_buf);
		sct->pat_buf = NULL;
		sct_free_l(pch, sct);
	}

	cnt_idle	= qbufp_count(pbufq, QBF_SCT_Q_IDLE);

	if (cnt_idle == DIM_SCT_NUB &&
	    pmsct->box) {
		di_mmu_box_free(pmsct->box);
		pmsct->box = NULL;
		pmsct->flg_act_box = 0;
		pmsct->max_nub = 0;
		PR_INF("%s:release\n", __func__);
	} else {
		PR_INF("%s:cnt_idle=%d\n", __func__, cnt_idle);
	}
}

/* sw off when rebuild */
void sct_sw_off_rebuild(struct di_ch_s *pch)
{
	unsigned int ch;
	struct dim_mscttop_s *psct;
	struct buf_que_s *pbufq;
	//union q_buf_u q_buf;// = NULL;
	struct dim_sct_s *sct;
//	struct di_buf_s *di_buf;
	int i;
	unsigned int len;
	bool ret;

	psct = &pch->msct_top;
	if (!psct->box) {
		PR_WARN("%s:box is not exist\n", __func__);
		return;
	}
	ch = pch->ch_id;

	pbufq = &pch->sct_qb;
	/* clear req */
	len = qbufp_count(pbufq, QBF_SCT_Q_REQ);
	if (len) {
		for (i = 0; i < len; i++) {
			ret = qsct_req_to_idle(pch, &sct);
			if (!ret) {
				PR_ERR("%s:clear req[%d][%d]\n", __func__, len, i);
				break;
			}
			/*pat*/
			if (sct->pat_buf) {
				pat_release_vaddr(sct->pat_buf);
				qpat_in_ready(pch, sct->pat_buf);
				sct->pat_buf = NULL;
			}
		}
	}

	/* ready clear */
	len = qbufp_count(pbufq, QBF_SCT_Q_READY);
	if (len) {
		for (i = 0; i < len; i++) {
			ret = qsct_any_to_recycle(pch, QBF_SCT_Q_READY, &sct);
			if (!ret) {
				PR_ERR("%s:used[%d][%d]\n", __func__, len, i);
				break;
			}
		}
	}
}

int dim_dbg_sct_top_show(struct seq_file *s, void *what)
{
	int *chp;
	struct di_ch_s *pch;
	struct dim_mscttop_s *psct;
	struct dim_msc_sum_s *psum;

	chp = (int *)s->private;

	seq_printf(s, "%s:ch[%d]\n", __func__, *chp);
	pch = get_chdata(*chp);

	psct = &pch->msct_top;
	psum = &psct->sum;
	seq_printf(s, "\t%s:0x%x\n", "err", psct->flg_err);
	seq_printf(s, "\t%s:%d\n", "max_nub", psct->max_nub);

	seq_printf(s, "\t%s:%d\n", "flg_support", psct->flg_support);
	seq_printf(s, "\t%s:%d\n", "flg_no_buf", psct->flg_no_buf);
	seq_printf(s, "\t%s:%d\n", "flg_act_box", psct->flg_act_box);
	seq_printf(s, "\t%s:%d\n", "flg_trig_dis", psct->flg_trig_dis);
	seq_printf(s, "\t%s:%d\n", "flg_allocing", psct->flg_allocing);
	seq_printf(s, "%s:\n", "sum");
	seq_printf(s, "\t%s:%d\n", "max_nub", psum->max_nub);
	seq_printf(s, "\t%s:%d\n", "max_size", psum->max_size);
	//seq_printf(s, "\t%s:%d\n", "sum_max_tt_size", psct->sum_max_tt_size);
	seq_printf(s, "\t%s:%d\n", "max_tt_size2", psum->max_tt_size2);
	seq_printf(s, "\t%s:%d\n", "curr_tt_size", psum->curr_tt_size);
	seq_printf(s, "\t%s:%d\n", "curr_nub", psum->curr_nub);
	seq_printf(s, "\t%s:\n", "pst");
	seq_printf(s, "\t\t%s:%d\n", "free", psum->mts_pst_free);
	seq_printf(s, "\t\t%s:%d\n", "back", psum->mts_pst_back);
	seq_printf(s, "\t\t%s:%d\n", "display", psum->mts_pst_dispaly);
	seq_printf(s, "\t\t%s:%d\n", "ready", psum->mts_pst_ready);
	seq_printf(s, "\t%s:\n", "sct");
	seq_printf(s, "\t\t%s:%d\n", "rcc", psum->mts_sct_rcc);
	seq_printf(s, "\t\t%s:%d\n", "ready", psum->mts_sct_ready);
	seq_printf(s, "\t\t%s:%d\n", "used", psum->mts_sct_used);
	return 0;
}

void sct_prob(struct di_ch_s *pch)
{
	struct dim_mscttop_s *psct;

	psct = &pch->msct_top;

	psct->flg_support = 1;
	mutex_init(&psct->lock_ready);
	di_mmu_box_init();
	dbg_sct("%s:ch[%d]\n", __func__, pch->ch_id);
}

void sct_polling(struct di_ch_s *pch, unsigned int pos)
{
	struct dim_mscttop_s	*pmsct;
	struct di_mng_s *pbm = get_bufmng();
	unsigned int cnt_pst_free;

	if (!pch)
		return;

	pmsct = &pch->msct_top;
	if (!pmsct->box)
		return;
	if (atomic_read(&pbm->trig_unreg[pch->ch_id]))
		return;

	cnt_pst_free	= di_que_list_count(pch->ch_id, QUE_POST_FREE);
	if (cnt_pst_free >= DIM_SCT_KEEP_READY)
		return;

//	dbg_poll("b:%d:\n", pos);
	sct_mng_working(pch);
	sct_alloc_in_poling(pch->ch_id);
	sct_mng_working(pch);
//	dbg_poll("b:end:\n");
	//check:
}
