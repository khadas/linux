// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/di_multi/di_vfm_test.c
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
#include <linux/seq_file.h>
#include <linux/types.h>

#include "deinterlace.h"
#include "di_data_l.h"
//#include "di_sys.h"
//#include "di_pre.h"
//#include "di_pres.h"
#include "di_prc.h"
#include "di_dbg.h"
#include "di_que.h"
//#include "di_task.h"

//#include "di_vframe.h"
//#include "di_interface.h"
//#include "di_vfm_test.h"

#define PR_ERR_Q(fmt, args ...)		pr_err("dim:err:" fmt, ## args)

//static DEFINE_SPINLOCK(dim_ready);
#ifdef MARK_HIS
unsigned int bypass_flg[] = {
	CODE_BYPASS,
};
#endif
/*keep same order with enum QS_FUNC_E*/
const char *qs_func_name[] = {
	"n_in",
	"n_out",
	"n_empty",
	"n_full",
	"n_list",
	"n_peek",
	"n_out_some",

	"f_in",// = 0x100,
	"f_out",
	"f_empty",
	"f_full",
	"f_list",
	"f_peek",
	"f_out_some"
};

const char *qs_err_name[] = {
	"index_overflow",
	"bit_check",
	"fifo_in",
	"fifo_o",
	"fifo_peek",
	"fifo_empty",
	"fifo_full",
	"fifo_alloc",
	"fifo_o_overflow"
};

const struct qs_err_msg_s qs_err_msg_tab[] = {
};

void qs_err_clear(struct qs_err_log_s *plog)
{
	if (!plog)
		return;
	memset(plog, 0, sizeof(*plog));
}

void qs_err_add(struct qs_err_log_s *plog, struct qs_err_msg_s *msg)
{
	if (!plog || !msg ||
	    plog->pos >= QS_ERR_LOG_SIZE)
		return;
	plog->msg[plog->pos] = *msg;
	plog->pos++;
}

bool qs_err_is_have(struct qs_err_log_s *plog)
{
	if (!plog || plog->pos > 0)
		return true;
	return false;
}

void qs_err_print(struct qs_err_log_s *plog)
{
	int i;
	unsigned int cnt_f, cnt_e;
	const char *fname, *errname;
	struct qs_err_msg_s *msg;
	unsigned int err_idx;

	if (!plog || !plog->pos)
		return;

	cnt_f = ARRAY_SIZE(qs_func_name);
	cnt_e = ARRAY_SIZE(qs_err_name);
	for (i = 0; i < plog->pos; i++) {
		msg = &plog->msg[i];
		if (msg->func_id < cnt_f)
			fname = qs_func_name[msg->func_id];
		else
			fname = "no";

		err_idx = msg->err_id - QS_ERR_INDEX_OVERFLOW;
		if (err_idx < cnt_e)
			errname = qs_err_name[err_idx];
		else
			errname = "no";

		PR_ERR("%s,err:%s:q[%s],p1[%d],p2[%d]\n",
		       fname, errname, msg->qname, msg->index1, msg->index2);
	}
}

static void n_lock(struct qs_cls_s *p)
{
	//mutex_lock(&p->n.mtex);
}

static void n_unlock(struct qs_cls_s *p)
{
	//mutex_unlock(&p->n.mtex);
}

static bool n_reset(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	ulong flags = 0;
	//int i;
	if (p->flg_lock)
		spin_lock_irqsave(&p->lock_rd, flags);

	p->n.marsk = 0;
	p->n.nub = 0;
	//memset(&p->n.lst[0], 0, sizeof(unsigned int) * MAX_FIFO_SIZE);
	if (p->flg_lock)
		spin_unlock_irqrestore(&p->lock_rd, flags);
	return true;
}

static bool is_eq_ubuf(union q_buf_u ubufa, union q_buf_u ubufb)
{
	if (ubufa.qbc == ubufb.qbc)
		return true;
	else
		return false;
}

static bool n_in(struct buf_que_s *pqb, struct qs_cls_s *p, union q_buf_u ubuf)
{
	struct qs_err_msg_s msg;
	//struct qs_cls_s *p;
	unsigned int buf_index;
	ulong flags = 0;

	buf_index = ubuf.qbc->index;
	if (buf_index > 31 ||
	    !is_eq_ubuf(pqb->pbuf[buf_index], ubuf)) {
		msg.func_id	= QS_FUNC_N_IN;
		msg.err_id	= QS_ERR_INDEX_OVERFLOW;
		msg.qname	= p->name;
		msg.index1	= buf_index;
		msg.index2	= 0;
		qs_err_add(p->plog, &msg);
		PR_ERR_Q("%s can't support %d\n", __func__, buf_index);

		return false;
	}

	if (p->flg_lock)
		spin_lock_irqsave(&p->lock_rd, flags);

	if (bget(&p->n.marsk, buf_index)) {
		msg.func_id	= QS_FUNC_N_IN;
		msg.err_id	= QS_ERR_BIT_CHECK;
		msg.qname	= p->name;
		msg.index1	= buf_index;
		msg.index2	= p->n.marsk;
		qs_err_add(p->plog, &msg);
		if (p->flg_lock)
			spin_unlock_irqrestore(&p->lock_rd, flags);
		//PR_ERR_Q("%s:\n", __func__);
		return false;
	}

	bset(&p->n.marsk, buf_index);
	p->n.nub++;
	if (p->flg_lock)
		spin_unlock_irqrestore(&p->lock_rd, flags);

	return true;
}

static bool n_out_some(struct buf_que_s *pqb, struct qs_cls_s *p,
		       union q_buf_u ubuf)
{
	bool ret = false;
	struct qs_err_msg_s msg;
	ulong flags = 0;

	unsigned int buf_index;

	buf_index = ubuf.qbc->index;

	if (buf_index > 31) {
		msg.func_id	= QS_FUNC_N_SOME;
		msg.err_id	= QS_ERR_INDEX_OVERFLOW;
		msg.qname	= p->name;
		msg.index1	= buf_index;
		msg.index2	= 0;
		qs_err_add(p->plog, &msg);
		PR_ERR_Q("%s can't support %d\n", __func__, buf_index);
		return false;
	}
	if (p->flg_lock)
		spin_lock_irqsave(&p->lock_rd, flags);
	if (bget(&p->n.marsk, buf_index)) {
		bclr(&p->n.marsk, buf_index);
		p->n.nub--;
		ret = true;
	}
	if (p->flg_lock)
		spin_unlock_irqrestore(&p->lock_rd, flags);
	if (!ret) {
		msg.func_id	= QS_FUNC_N_SOME;
		msg.err_id	= QS_ERR_BIT_CHECK;
		msg.qname	= p->name;
		msg.index1	= buf_index;
		msg.index2	= p->n.marsk;
		qs_err_add(p->plog, &msg);
		PR_ERR_Q("%s:%s:%d\n", __func__, p->name, buf_index);
	}

	return ret;
}

static bool n_empty(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	bool ret = false;
	bool err = false;
	struct qs_err_msg_s msg;
	ulong flags = 0;

	if (p->flg_lock)
		spin_lock_irqsave(&p->lock_rd, flags);
	if (!p->n.nub) {
		ret = true;
		if (p->n.marsk != 0) /*double check*/
			err = true;
	}
	if (p->flg_lock)
		spin_unlock_irqrestore(&p->lock_rd, flags);

	if (err) {
		msg.func_id	= QS_FUNC_N_EMPTY;
		msg.err_id	= QS_ERR_BIT_CHECK;
		msg.qname	= p->name;
		msg.index1	= p->n.nub;
		msg.index2	= p->n.marsk;
		qs_err_add(p->plog, &msg);

		PR_ERR_Q("%s:%s:nub[%d], mask[%d]\n", __func__,
			 p->name, p->n.nub, p->n.marsk);
	}
	return ret;
}

static bool n_is_full(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	bool ret = false;
	bool err = false;
	struct qs_err_msg_s msg;
	ulong flags = 0;

	if (p->flg_lock)
		spin_lock_irqsave(&p->lock_rd, flags);
	if (p->n.nub == MAX_FIFO_SIZE) {
		ret = true;
		if (p->n.marsk != 0xffffffff) /*double check*/
			err = true;
	}
	if (p->flg_lock)
		spin_unlock_irqrestore(&p->lock_rd, flags);

	if (err) {
		msg.func_id	= QS_FUNC_N_FULL;
		msg.err_id	= QS_ERR_BIT_CHECK;
		msg.qname	= p->name;
		msg.index1	= p->n.nub;
		msg.index2	= p->n.marsk;
		qs_err_add(p->plog, &msg);
		PR_ERR_Q("%s:%s:nub[%d], mask[%d]\n", __func__,
			 p->name, p->n.nub, p->n.marsk);
	}
	return ret;
}

static unsigned int n_count(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	unsigned int length;
	ulong flags = 0;

	if (p->flg_lock)
		spin_lock_irqsave(&p->lock_rd, flags);
	length = p->n.nub;

	if (p->flg_lock)
		spin_unlock_irqrestore(&p->lock_rd, flags);
	return length;
}

static bool n_list(struct buf_que_s *pqb, struct qs_cls_s *p,
		   unsigned int *rsize)
{
	int i;
	//unsigned int index;
	unsigned int mask;
	unsigned int cnt;
	ulong flags = 0;

	if (p->flg_lock)
		spin_lock_irqsave(&p->lock_rd, flags);

	/*que_dbg("%s:begin\n", __func__);*/
	for (i = 0; i < MAX_FIFO_SIZE; i++)
		pqb->list_id[i] = 0xff;
	/*QUED_K_N*/

	mask = p->n.marsk;
	*rsize = p->n.nub;

	cnt = 0;
	for (i = 0; i < MAX_FIFO_SIZE; i++) {
		if (mask & 0x01) {
			pqb->list_id[cnt] = i;
			cnt++;
		}
		mask >>= 1;
	}

#ifdef MARK_SC2	/*debug only*/
	que_dbg("%s: size[%d]\n", di_name_new_que[qtype], *rsize);
	for (i = 0; i < *rsize; i++)
		que_dbg("%d,", outbuf[i]);

	que_dbg("\n");
#endif
	if (p->flg_lock)
		spin_unlock_irqrestore(&p->lock_rd, flags);

	return true;
}

static bool n_out(struct buf_que_s *pqb, struct qs_cls_s *p,
		  union q_buf_u *pbuf)
{
	int i;
	//unsigned int index;
	unsigned int mask;
	//unsigned int cnt;
	ulong flags = 0;
	bool ret = false;

	if (p->flg_lock)
		spin_lock_irqsave(&p->lock_rd, flags);

	if (!p->n.nub) {
		/* *pbuf = NULL;*/
		if (p->flg_lock)
			spin_unlock_irqrestore(&p->lock_rd, flags);
		return false;
	}

	mask = p->n.marsk;

	for (i = 0; i < MAX_FIFO_SIZE; i++) {
		if (mask & 0x01) {
			//pqb->list_id[cnt] = i;
			*pbuf = pqb->pbuf[i];
			bclr(&p->n.marsk, i);
			p->n.nub--;
			ret = true;
			break;
		}
		mask >>= 1;
	}

	if (p->flg_lock)
		spin_unlock_irqrestore(&p->lock_rd, flags);

	return ret;
}

/* 2020-12-07 */
static bool n_is_in(struct buf_que_s *pqb, struct qs_cls_s *p, union q_buf_u ubuf)
{
	struct qs_err_msg_s msg;
	//struct qs_cls_s *p;
	unsigned int buf_index;
	ulong flags = 0;
	bool ret;

	buf_index = ubuf.qbc->index;
	if (buf_index > 31 ||
	    !is_eq_ubuf(pqb->pbuf[buf_index], ubuf)) {
		msg.func_id	= QS_FUNC_N_IS_IN;
		msg.err_id	= QS_ERR_INDEX_OVERFLOW;
		msg.qname	= p->name;
		msg.index1	= buf_index;
		msg.index2	= 0;
		qs_err_add(p->plog, &msg);
		PR_ERR_Q("%s can't support %d\n", __func__, buf_index);

		return false;
	}

	if (p->flg_lock)
		spin_lock_irqsave(&p->lock_rd, flags);

	if (bget(&p->n.marsk, buf_index))
		ret = true;
	else
		ret = false;

	if (p->flg_lock)
		spin_unlock_irqrestore(&p->lock_rd, flags);

	return ret;
}

static bool n_get_marsk(struct buf_que_s *pqb, struct qs_cls_s *p,
		  unsigned int *marsk)
{
//	int i;
	//unsigned int index;
//	unsigned int mask;
//	unsigned int cnt;
	ulong flags = 0;
//	bool ret = false;

	if (p->flg_lock)
		spin_lock_irqsave(&p->lock_rd, flags);

	*marsk = p->n.marsk;
	if (p->flg_lock)
		spin_unlock_irqrestore(&p->lock_rd, flags);
	return true;
}

/******************************************************************/
static bool np_reset(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	n_lock(p);
	p->n.marsk = 0;
	p->n.nub = 0;

	memset(&p->n.lst[0], 0, sizeof(ud) * MAX_FIFO_SIZE);
	n_unlock(p);
	return true;
}

static unsigned int np_check_empty_index(struct qs_cls_s *p)
{
	unsigned int ret = 0xff;
	int i;
	unsigned int mask;

	if (p->n.marsk == 0xffffffff)
		return ret;

	mask = p->n.marsk;
	for (i = 0; i < MAX_FIFO_SIZE; i++) {
		if ((mask & 0x01) == 0) {
			ret = i;
			break;
		}
		mask >>= 1;
	}

	return ret;
}

static bool np_in(struct buf_que_s *pqb, struct qs_cls_s *p, union q_buf_u ubuf)
{
	struct qs_err_msg_s msg;
	unsigned int buf_index;

	buf_index = np_check_empty_index(p);

	if (buf_index > 31) {
		msg.func_id	= QS_FUNC_N_IN;
		msg.err_id	= QS_ERR_INDEX_OVERFLOW;
		msg.qname	= p->name;
		msg.index1	= buf_index;
		msg.index2	= 0;
		qs_err_add(p->plog, &msg);
		PR_ERR_Q("%s can't support %d\n", __func__, buf_index);

		return false;
	}

	n_lock(p);
	if (bget(&p->n.marsk, buf_index)) {
		msg.func_id	= QS_FUNC_N_IN;
		msg.err_id	= QS_ERR_BIT_CHECK;
		msg.qname	= p->name;
		msg.index1	= buf_index;
		msg.index2	= p->n.marsk;
		qs_err_add(p->plog, &msg);
		n_unlock(p);
		//PR_ERR_Q("%s:\n", __func__);
		return false;
	}

	bset(&p->n.marsk, buf_index);
	p->n.lst[buf_index] = ubuf;
	p->n.nub++;
	n_unlock(p);

	return true;
}

static bool np_out_some(struct buf_que_s *pqb, struct qs_cls_s *p,
			union q_buf_u ubuf)
{
	bool ret = false;
	struct qs_err_msg_s msg;
	int i;
	unsigned int buf_index;
	unsigned int mask;
	//ud	addr;

	n_lock(p);
	mask = p->n.marsk;
	//addr = (ud)pbuf;

	buf_index = 0xff;
	for (i = 0; i < MAX_FIFO_SIZE; i++) {
		if (mask & 0x01 &&
		    is_eq_ubuf(ubuf, p->n.lst[i])) {
			buf_index = i;
			break;
		}
		mask >>= 1;
	}
	if (buf_index >= MAX_FIFO_SIZE) {
		PR_ERR("%s:q[%s]:%p, is not in q\n", __func__, p->name,
		       ubuf.qbc);
		return false;
	}
	if (bget(&p->n.marsk, buf_index)) {
		p->n.lst[buf_index].qbc = NULL;
		bclr(&p->n.marsk, buf_index);
		p->n.nub--;
		ret = true;
	}
	n_unlock(p);
	if (!ret) {
		msg.func_id	= QS_FUNC_N_SOME;
		msg.err_id	= QS_ERR_BIT_CHECK;
		msg.qname	= p->name;
		msg.index1	= buf_index;
		msg.index2	= p->n.marsk;
		qs_err_add(p->plog, &msg);
		//PR_ERR_Q("%s:%s:%d\n", __func__, p->name, buf_index);
	}

	return ret;
}

static bool np_empty(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	bool ret = false;
	bool err = false;
	struct qs_err_msg_s msg;

	n_lock(p);
	if (!p->n.nub) {
		ret = true;
		if (p->n.marsk != 0) /*double check*/
			err = true;
	}
	n_unlock(p);

	if (err) {
		msg.func_id	= QS_FUNC_N_EMPTY;
		msg.err_id	= QS_ERR_BIT_CHECK;
		msg.qname	= p->name;
		msg.index1	= p->n.nub;
		msg.index2	= p->n.marsk;
		qs_err_add(p->plog, &msg);

		PR_ERR_Q("%s:%s:nub[%d], mask[%d]\n", __func__,
			 p->name, p->n.nub, p->n.marsk);
	}
	return ret;
}

static bool np_is_full(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	bool ret = false;
	bool err = false;
	struct qs_err_msg_s msg;

	n_lock(p);
	if (p->n.nub == MAX_FIFO_SIZE) {
		ret = true;
		if (p->n.marsk != 0xffffffff) /*double check*/
			err = true;
	}
	n_unlock(p);

	if (err) {
		msg.func_id	= QS_FUNC_N_FULL;
		msg.err_id	= QS_ERR_BIT_CHECK;
		msg.qname	= p->name;
		msg.index1	= p->n.nub;
		msg.index2	= p->n.marsk;
		qs_err_add(p->plog, &msg);
		PR_ERR_Q("%s:%s:nub[%d], mask[%d]\n", __func__,
			 p->name, p->n.nub, p->n.marsk);
	}
	return ret;
}

static unsigned int np_count(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	unsigned int length;

	n_lock(p);
	length = p->n.nub;
	n_unlock(p);
	return length;
}

static bool np_list(struct buf_que_s *pqb, struct qs_cls_s *p,
		    unsigned int *rsize)
{
	int i;
	//unsigned int index;
	unsigned int mask;
	unsigned int cnt;

	/*que_dbg("%s:begin\n", __func__);*/
	for (i = 0; i < MAX_FIFO_SIZE; i++)
		pqb->list_ud[i] = 0x0;
	/*QUED_K_N*/

	mask = p->n.marsk;
	*rsize = p->n.nub;

	cnt = 0;
	for (i = 0; i < MAX_FIFO_SIZE; i++) {
		if (mask & 0x01) {
			pqb->list_ud[cnt] = (ud)p->n.lst[i].qbc;
			cnt++;
		}
		mask >>= 1;
	}

#ifdef MARK_SC2	/*debug only*/
	que_dbg("%s: size[%d]\n", di_name_new_que[qtype], *rsize);
	for (i = 0; i < *rsize; i++)
		que_dbg("%d,", outbuf[i]);

	que_dbg("\n");
#endif
	return true;
}

/******************************************************************/
static bool f_reset(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	//PR_INF("%s:que reset t[%s]\n", __func__, p->name);
	kfifo_reset(&p->f.fifo);
	return true;
}

static bool f_in(struct buf_que_s *pqb, struct qs_cls_s *p, union q_buf_u ubuf)
{
	struct qs_err_msg_s msg;
	unsigned int buf_index;
	unsigned int ret;

	buf_index = ubuf.qbc->index;
	if (p->flg_lock & DIM_QUE_LOCK_WR)
		ret = kfifo_in_spinlocked(&p->f.fifo,
					  &buf_index,
					  tst_que_ele,
					  &p->lock_wr);
	else
		ret = kfifo_in(&p->f.fifo, &buf_index, tst_que_ele);

	if (ret	!= tst_que_ele) {
		msg.func_id	= QS_FUNC_F_IN;
		msg.err_id	= QS_ERR_FIFO_IN;
		msg.qname	= p->name;
		msg.index1	= buf_index;
		msg.index2	= 0;
		qs_err_add(p->plog, &msg);

		PR_ERR_Q("%s:%s:%d\n", __func__, p->name, buf_index);
		return false;
	}

	return true;
}

static bool f_out(struct buf_que_s *pqb, struct qs_cls_s *p,
		  union q_buf_u *pbuf)
{
	unsigned int index;
	struct qs_err_msg_s msg;
	unsigned int ret;

	if (p->flg_lock & DIM_QUE_LOCK_RD) {
		ret = kfifo_out_spinlocked(&p->f.fifo,
					   &index,
					   tst_que_ele,
					   &p->lock_rd);
	} else {
		ret = kfifo_out(&p->f.fifo, &index, tst_que_ele);
	}
	if (ret != tst_que_ele) {
		msg.func_id	= QS_FUNC_F_O;
		msg.err_id	= QS_ERR_FIFO_O;
		msg.qname	= p->name;
		msg.index1	= 0;
		msg.index2	= 0;
		qs_err_add(p->plog, &msg);

		PR_ERR_Q("%s:%s\n", __func__, p->name);
		return false;
	}

	*pbuf = pqb->pbuf[index];

	return true;
}

static bool f_peek(struct buf_que_s *pqb, struct qs_cls_s *p,
		   union q_buf_u *pbuf)
{
	unsigned int index;
	struct qs_err_msg_s msg;

	if (kfifo_is_empty(&p->f.fifo)) {
		pbuf = NULL;
		return false;
	}

	if (kfifo_out_peek(&p->f.fifo, &index, tst_que_ele) !=
	    tst_que_ele) {
		msg.func_id	= QS_FUNC_F_PEEK;
		msg.err_id	= QS_ERR_FIFO_PEEK;
		msg.qname	= p->name;
		msg.index1	= 0;
		msg.index2	= 0;
		qs_err_add(p->plog, &msg);
		pbuf = NULL;
		return false;
	}
	*pbuf = pqb->pbuf[index];

	return true;
}

#ifdef MARK_SC2
static bool q_move(enum TST_QUED_TYPE qtypef, enum TST_QUED_TYPE qtypet,
		   unsigned int *oindex)
{
	unsigned int index;

	if (kfifo_out(&fifo[qtypef], &index, tst_que_ele)
		!= tst_que_ele) {
		//PR_ERR("qtypef[%d] is empty\n", qtypef);
		return false;
	}
	if (kfifo_in(&fifo[qtypet], &index, tst_que_ele)
			!= tst_que_ele) {
		//PR_ERR("qtypet[%d] is full\n", qtypet);
		return false;
	}

	*oindex = index;

	return true;
}
#endif

static bool f_empty(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	return kfifo_is_empty(&p->f.fifo);
}

static bool f_is_full(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	return kfifo_is_full(&p->f.fifo);
}

static unsigned int f_count(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	unsigned int length;

	length = kfifo_len(&p->f.fifo);
	length = length / tst_que_ele;
	return length;
}

bool f_list(struct buf_que_s *pqb, struct qs_cls_s *p,
	    unsigned int *rsize)
{
	int i;
	unsigned int index;

	for (i = 0; i < MAX_FIFO_SIZE; i++)
		pqb->list_id[i] = 0xff;

	if (kfifo_is_empty(&p->f.fifo)) {
		PR_INF("\t%s:empty\n", p->name);
		*rsize = 0;
		return true;
	}

	memcpy(&pqb->tmp_kfifo.fifo, &p->f.fifo, sizeof(pqb->tmp_kfifo.fifo));

	i = 0;
	*rsize = 0;

	while (kfifo_out(&pqb->tmp_kfifo.fifo, &index, tst_que_ele) ==
	       tst_que_ele) {
		pqb->list_id[i] = index;
		i++;
	}
	*rsize = i;

	#ifdef MARK_SC2
	/*dbg*/
	PR_INF("%s:%d\n", __func__, *rsize);
	for (i = 0; i < *rsize; i++)
		PR_INF("\t%d,%d\n", i, p->list[i]);

	#endif

	return true;
}

#ifdef MARK_SC2
const struct qsp_ops_s qfifo_ops = {
	.in	= f_in,
	.out	= f_out,
	.peek	= f_peek,
	.is_empty = f_empty,
	.is_full	= f_is_full,
	.count	= f_count,
	.reset	= f_reset,
	.list	= f_list,
};
#endif
/******************************************************************/
static bool fp_reset(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	//PR_INF("%s:que reset t[%s]\n", __func__, p->name);
	kfifo_reset(&p->f.fifo);
	return true;
}

static bool fp_in(struct buf_que_s *pqb, struct qs_cls_s *p, union q_buf_u ubuf)
{
	struct qs_err_msg_s msg;
//	struct qs_cls_s *p;
	ud buf_add;
	unsigned int ret;

	buf_add = (ud)ubuf.qbc;
	if (p->flg_lock & DIM_QUE_LOCK_WR) {
		ret = kfifo_in_spinlocked(&p->f.fifo,
					  &buf_add,
					  tst_que_ele,
					  &p->lock_wr);
	} else {
		ret = kfifo_in(&p->f.fifo, &buf_add, tst_quep_ele);
	}

	if (ret	!= tst_quep_ele) {
		msg.func_id	= QS_FUNC_F_IN;
		msg.err_id	= QS_ERR_FIFO_IN;
		msg.qname	= p->name;
		msg.index1	= buf_add;
		msg.index2	= 0;
		qs_err_add(p->plog, &msg);

		PR_ERR_Q("%s:%s:%p\n", __func__, p->name, ubuf.qbc);
		return false;
	}
	return true;
}

static bool fp_out(struct buf_que_s *pqb, struct qs_cls_s *p,
		   union q_buf_u *pbuf)
{
	//unsigned int index;
	struct qs_err_msg_s msg;
	ud buf_add;
	union q_buf_u u_buf;
	unsigned int ret;

	if (p->flg_lock & DIM_QUE_LOCK_RD) {
		ret = kfifo_out_spinlocked(&p->f.fifo,
					   &buf_add,
					   tst_quep_ele,
					   &p->lock_rd);
	} else {
		ret = kfifo_out(&p->f.fifo, &buf_add, tst_quep_ele);
	}

	if (ret != tst_quep_ele) {
		msg.func_id	= QS_FUNC_F_O;
		msg.err_id	= QS_ERR_FIFO_O;
		msg.qname	= p->name;
		msg.index1	= 0;
		msg.index2	= 0;
		qs_err_add(p->plog, &msg);

		PR_ERR_Q("%s:%s\n",
			 __func__, p->name);
		return false;
	}

	u_buf.qbc = (struct qs_buf_s *)buf_add;
	*pbuf = u_buf;

	return true;
}

static bool fp_peek(struct buf_que_s *pqb, struct qs_cls_s *p,
		    union q_buf_u *pbuf)
{
	//unsigned int index;
	ud index;
	struct qs_err_msg_s msg;
	union q_buf_u u_buf;

	if (kfifo_is_empty(&p->f.fifo))
		return false;

	if (kfifo_out_peek(&p->f.fifo, &index, tst_quep_ele) !=
	    tst_quep_ele) {
		msg.func_id	= QS_FUNC_F_PEEK;
		msg.err_id	= QS_ERR_FIFO_PEEK;
		msg.qname	= p->name;
		msg.index1	= 0;
		msg.index2	= 0;
		qs_err_add(p->plog, &msg);
		return false;
	}
	u_buf.qbc = (struct qs_buf_s *)index;
	*pbuf = u_buf;

	return true;
}

static bool fp_empty(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	return kfifo_is_empty(&p->f.fifo);
}

static bool fp_is_full(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	return kfifo_is_full(&p->f.fifo);
}

static unsigned int fp_count(struct buf_que_s *pqb, struct qs_cls_s *p)
{
	unsigned int length;

	length = kfifo_len(&p->f.fifo);
	length = length / tst_quep_ele;
	return length;
}

bool fp_list(struct buf_que_s *pqb, struct qs_cls_s *p,
	     unsigned int *rsize)
{
	int i;
	ud index;

	for (i = 0; i < MAX_FIFO_SIZE; i++)
		pqb->list_id[i] = 0xff;

	if (kfifo_is_empty(&p->f.fifo)) {
		PR_INF("\t%s:empty\n", p->name);
		*rsize = 0;
		return true;
	}

	memcpy(&pqb->tmp_kfifo.fifo, &p->f.fifo, sizeof(pqb->tmp_kfifo.fifo));

	i = 0;
	*rsize = 0;

	while (kfifo_out(&pqb->tmp_kfifo.fifo, &index, tst_quep_ele) ==
	       tst_quep_ele) {
		pqb->list_ud[i] = index;
		i++;
	}
	*rsize = i;

	#ifdef MARK_SC2
	/*dbg*/
	PR_INF("%s:%d\n", __func__, *rsize);
	for (i = 0; i < *rsize; i++)
		PR_INF("\t%d,%d\n", i, p->list[i]);

	#endif

	return true;
}

const struct qsp_ops_s ques_ops[] = {
	[Q_T_FIFO] = {
		.in	= f_in,
		.out	= f_out,
		.out_some	= NULL,
		.peek	= f_peek,
		.is_empty = f_empty,
		.is_full	= f_is_full,
		.count	= f_count,
		.reset	= f_reset,
		.list	= f_list,
		.is_in  = NULL,
		.n_get_marsk = NULL,
	},
	[Q_T_N] = {
		.in		= n_in,
		.out		= n_out,
		.out_some	= n_out_some,
		.peek		= NULL,
		.is_empty	= n_empty,
		.is_full	= n_is_full,
		.count		= n_count,
		.reset		= n_reset,
		.list		= n_list,
		.is_in		= n_is_in,
		.n_get_marsk = n_get_marsk,
	},
	[Q_T_FIFO_2] = {
		.in	= fp_in,
		.out	= fp_out,
		.out_some	= NULL,
		.peek	= fp_peek,
		.is_empty = fp_empty,
		.is_full	= fp_is_full,
		.count	= fp_count,
		.reset	= fp_reset,
		.list	= fp_list,
		.is_in  = NULL,
		.n_get_marsk = NULL,
	},
	[Q_T_N_2] = {
		.in		= np_in,
		.out		= NULL,
		.out_some	= np_out_some,
		.peek		= NULL,
		.is_empty	= np_empty,
		.is_full	= np_is_full,
		.count		= np_count,
		.reset		= np_reset,
		.list		= np_list,
		.is_in  = NULL,
		.n_get_marsk = NULL,
	}
};

/**********************************************************************/
static bool qs_creat(enum Q_TYPE t, struct qs_cls_s *p, const char *name)
{
	int ret;

	if (!p) {
		//PR_ERR("%s:null\n", __func__);
		return false;
	}

	memset(p, 0, sizeof(*p));
	p->name = name;
	p->flg	= true;

	if (t == Q_T_FIFO) {
		p->type = Q_T_FIFO;

		ret = kfifo_alloc(&p->f.fifo,
				  tst_que_ele * MAX_FIFO_SIZE,
				  GFP_KERNEL);
		if (ret < 0) {
			PR_ERR_Q("%s:alloc kfifo err:%s\n", __func__, p->name);
			p->flg = false;
			return false;
		}

		memcpy(&p->ops, &ques_ops[Q_T_FIFO], sizeof(p->ops));
		return true;
	}

	if (t == Q_T_N) {
		p->type = Q_T_N;
		//mutex_init(&p->n.mtex);
		memcpy(&p->ops, &ques_ops[Q_T_N], sizeof(p->ops));
		return true;
	}

	if (t == Q_T_FIFO_2) {
		p->type = Q_T_FIFO_2;

		ret = kfifo_alloc(&p->f.fifo,
				  tst_quep_ele * MAX_FIFO_SIZE,
				  GFP_KERNEL);
		if (ret < 0) {
			PR_ERR_Q("%s:alloc kfifo err:%s\n", __func__, p->name);
			p->flg = false;
			return false;
		}

		memcpy(&p->ops, &ques_ops[Q_T_FIFO_2], sizeof(p->ops));
		return true;
	}

	if (t == Q_T_N_2) {
		p->type = Q_T_N_2;
		//mutex_init(&p->n.mtex);
		memcpy(&p->ops, &ques_ops[Q_T_N_2], sizeof(p->ops));
		return true;
	}
	return true;
}

static bool qs_release(struct qs_cls_s *p)
{
	if ((p->type == Q_T_FIFO	||
	     p->type == Q_T_FIFO_2)		&&
	     p->flg)
		kfifo_free(&p->f.fifo);

	memset(p, 0, sizeof(*p));

	return true;
}

#ifdef MARK_SC2
static bool qb_list(struct buf_que_s *pqb, unsigned int qindex,
		    unsigned int *rsize)
{
	struct qs_cls_s *p;
	bool ret;

	if (pqb && pqb->pque[qindex]) {
		p = pqb->pque[qindex];
	} else {
		PR_ERR("%s:no que %d\n", __func__, qindex);
		return false;
	}

	if (p->type == Q_T_FIFO)
		ret = f_list(pqb, qindex, rsize);
	if (p->type == Q_T_N)
		ret = n_list(pqb, qindex, rsize);
	if (p->type == Q_T_FIFO_2)
		return fp_list(pqb, qindex, rsize);

		return false;
}
#endif
/* fp que don't need pbufq */
void qfp_int(struct qs_cls_s	*pq,
	      unsigned char *qname,
	      unsigned int lock)
{
//	int i;
//	const struct que_creat_s *pcfg;

//	bool rsc_flg = true;
//	int ret;

	if (!pq)
		return;

	/*creat que*/
	qs_creat(Q_T_FIFO_2, pq, qname);
	if (!pq->flg) {
		//rsc_flg = false;
		//no resource
		return;
	}
	/*reset*/
	pq->ops.reset(NULL, pq);
	/*lock ?*/
	if (lock & DIM_QUE_LOCK_RD) {
		spin_lock_init(&pq->lock_rd);
		pq->flg_lock |= DIM_QUE_LOCK_RD;
	}
	if (lock & DIM_QUE_LOCK_WR) {
		spin_lock_init(&pq->lock_wr);
		pq->flg_lock |= DIM_QUE_LOCK_WR;
	}

	dbg_reg("%s:%s:end\n", __func__, qname);
}

bool qfp_release(struct qs_cls_s	*pq)
{
//	int i;

	if (!pq)
		return true;
	qs_release(pq);

	return true;
}

/* @ary_note: this need protect */
unsigned int qfp_list(struct qs_cls_s *p,
	     unsigned int size,
	     void **list)
{
	int i;
	ud index;
	struct kfifo	tmp_kfifo;
	int ret;
	unsigned int cnt = 0;

	for (i = 0; i < size; i++)
		list[i] = NULL;

	if (kfifo_is_empty(&p->f.fifo)) {
		PR_INF("\t%s:empty\n", p->name);
		return 0;
	}

	ret = kfifo_alloc(&tmp_kfifo,
			  tst_quep_ele * MAX_FIFO_SIZE,
			  GFP_KERNEL);
	if (ret < 0) {
		PR_ERR_Q("%s:alloc kfifo err:tmp\n", __func__);
		return false;
	}

	memcpy(&tmp_kfifo, &p->f.fifo, sizeof(tmp_kfifo));

	while (kfifo_out(&tmp_kfifo, &index, tst_quep_ele) ==
	       tst_quep_ele && (cnt < size)) {
		list[i] = (void *)index;
		cnt++;
	}
	kfifo_free(&tmp_kfifo);
	return cnt;
}

void qbuf_int(struct buf_que_s *pbufq, const struct que_creat_s *cfg,
	      const struct qbuf_creat_s *cfg_qbuf)
{
	int i;
	const struct que_creat_s *pcfg;
	struct qs_cls_s	*pq;
	bool rsc_flg = true;
	int ret;

	if (!pbufq || !cfg || !cfg_qbuf) {
		PR_ERR_Q("%s: no input\n", __func__);
		return;
	}
	//pbufq->ppb = pbufq;
	/*ops*/
	#ifdef MARK_SC2
	memcpy(&pbufq->ops[Q_T_FIFO], &qfifo_ops,
	       sizeof(struct qs_ops_s));
	memcpy(&pbufq->ops[Q_T_N], &qn_ops,
	       sizeof(struct qs_ops_s));
	#else

	//memcpy(&pbufq->ops[Q_T_FIFO], &ques_ops,
	//       (sizeof(struct qsp_ops_s) * Q_T_NUB));

	#endif
	/* que */
	for (i = 0; i < cfg_qbuf->nub_que; i++) {
		//PR_INF("%s:que[%d]\n", __func__, i);
		pcfg = cfg + i;
		/*creat que*/
		pq = pbufq->pque[i];
		qs_creat(pcfg->type, pq, pcfg->name);
		if (!pq->flg) {
			rsc_flg = false;
			break;
		}
		/*reset*/
		pq->ops.reset(pbufq, pq);
		/*lock ?*/
		if (pcfg->lock & DIM_QUE_LOCK_RD) {
			spin_lock_init(&pq->lock_rd);
			pq->flg_lock |= DIM_QUE_LOCK_RD;
		}
		if (pcfg->lock & DIM_QUE_LOCK_WR) {
			spin_lock_init(&pq->lock_wr);
			pq->flg_lock |= DIM_QUE_LOCK_WR;
		}
		/*log*/
		pq->plog = &pbufq->log;
	}

	//pbufq->opsb.list = qb_list;

	//PR_INF("%s:5\n", __func__);
	/* que tmp*/
	pbufq->tmp_kfifo.flg = true;
	ret = kfifo_alloc(&pbufq->tmp_kfifo.fifo,
			  tst_que_ele * MAX_FIFO_SIZE,
			  GFP_KERNEL);
	if (ret < 0) {
		PR_ERR("%s:alloc kfifo err:tmp\n", __func__);
		pbufq->tmp_kfifo.flg = false;
		rsc_flg = false;
	}
	/*buf*/
	for (i = 0; i < cfg_qbuf->nub_buf; i++) {
		//PR_INF("%s:buf[%d]\n", __func__, i);
		if (pbufq->pbuf[i].qbc)
			pbufq->pbuf[i].qbc->code = cfg_qbuf->code;
	}

	pbufq->name = cfg_qbuf->name;
	pbufq->nub_buf = cfg_qbuf->nub_buf;
	pbufq->nub_que = cfg_qbuf->nub_que;
	pbufq->bque_id	= cfg_qbuf->que_id;
	if (rsc_flg)
		pbufq->rflg = true;
	else
		pbufq->rflg = false;

	dbg_reg("%s:%s:end\n", __func__, pbufq->name);
}

bool qbuf_release_que(struct buf_que_s *pqbuf)
{
	int i;

	if (!pqbuf)
		return true;

	for (i = 0; i < pqbuf->nub_que; i++) {
		if (pqbuf->pque[i])
			qs_release(pqbuf->pque[i]);
		else
			PR_ERR_Q("%s:qb[%s]q[%i] is nothing\n",
				 __func__, pqbuf->name, i);
	}

	if (pqbuf->tmp_kfifo.flg) {
		kfifo_free(&pqbuf->tmp_kfifo.fifo);
		pqbuf->tmp_kfifo.flg = 0;
	}
	return true;
}

bool qbuf_is_empty(struct buf_que_s *pqbuf, unsigned int qindex)
{
	//enum Q_TYPE etype;
	struct qs_cls_s *pq;

	/*check que*/
	if (!pqbuf->pque[qindex]) {
		PR_ERR_Q("%s:pq[%s][%d] is null\n", __func__,
			 pqbuf->name, qindex);
		dump_stack();
		return true;
	}
	pq = pqbuf->pque[qindex];

	return pq->ops.is_empty(pqbuf, pq);
}

bool qbuf_is_full(struct buf_que_s *pqbuf, unsigned int qindex)
{
//	bool ret;
	struct qs_cls_s *pq;

	/*check que*/
	if (!pqbuf->pque[qindex]) {
		PR_ERR_Q("%s:pq[%s][%d] is null\n", __func__,
			 pqbuf->name, qindex);
		return true;
	}
	pq = pqbuf->pque[qindex];

	return pq->ops.is_full(pqbuf, pq);
}

/**/
bool qbuf_move_some(struct buf_que_s *pqbuf, unsigned int qf,
		    unsigned int qt, unsigned int index)
{
	struct qs_cls_s *pqf, *pqt;
	//enum Q_TYPE etypef, etypet;
	union q_buf_u q_buf;

	/*check que*/
	if (!pqbuf->pque[qf]) {
		PR_ERR_Q("%s:pq[%s]f[%d] is null\n", __func__,
			 pqbuf->name, qf);
		return false;
	}

	if (!pqbuf->pque[qt]) {
		PR_ERR_Q("%s:pq[%s]t[%d] is null\n", __func__,
			 pqbuf->name, qt);
		return false;
	}

	pqf = pqbuf->pque[qf];
	pqt = pqbuf->pque[qt];
	//etypef = pqf->type;
	//etypet = pqt->type;

	/*check out_some*/
	if (!pqf->ops.out_some) {
		PR_ERR_Q("%s:qbuf[%s]:que[%s] no out_some\n", __func__,
			 pqbuf->name, pqf->name);
		return false;
	}
	/*check qt is full*/
	if (pqt->ops.is_full(pqbuf, pqt)) {
		PR_ERR_Q("%s:qbuf[%s]:q[%s] is full\n", __func__,
			 pqbuf->name, pqt->name);
		return false;
	}
	q_buf = pqbuf->pbuf[index];
	if (pqf->ops.out_some(pqbuf, pqf, q_buf))
		pqt->ops.in(pqbuf, pqt, q_buf);

	return true;
}

bool qbuf_move(struct buf_que_s *pqbuf, unsigned int qf,
	       unsigned int qt, unsigned int *oindex)
{
//	unsigned int buf_index;
	struct qs_cls_s *pqf, *pqt;
	bool ret = false;
	//enum Q_TYPE etypef, etypet;
	union q_buf_u q_buf;

	/*check que*/
	if (!pqbuf->pque[qf]) {
		PR_ERR_Q("%s:pq[%s]f[%d] is null\n", __func__,
			 pqbuf->name, qf);
		return false;
	}

	if (!pqbuf->pque[qt]) {
		PR_ERR_Q("%s:pq[%s]t[%d] is null\n", __func__,
			 pqbuf->name, qt);
		return false;
	}

	pqf = pqbuf->pque[qf];
	pqt = pqbuf->pque[qt];
	//etypef = pqf->type;
	//etypet = pqt->type;

	/*check out_some*/
	if (!pqf->ops.out) {
		PR_ERR_Q("%s:qbuf[%s]:que[%s] no out_some\n", __func__,
			 pqbuf->name, pqf->name);
		return false;
	}
	/*check qt is full*/
	if (pqt->ops.is_full(pqbuf, pqt)) {
		PR_ERR_Q("%s:qbuf[%s]:q[%s] is full\n", __func__,
			 pqbuf->name, pqt->name);
		return false;
	}

	if (pqf->ops.out(pqbuf, pqf, &q_buf)) {
		pqt->ops.in(pqbuf, pqt, q_buf);
		*oindex = q_buf.qbc->index;
		ret = true;
	} else {
		PR_ERR_Q("%s:qb[%s],qf[%s], qt[%s]\n", __func__,
			 pqbuf->name, pqf->name, pqf->name);
	}

	return ret;
}

bool qbuf_in(struct buf_que_s *pqbuf, unsigned int qt, unsigned int data)
{
	struct qs_cls_s *pqt;
	union q_buf_u q_buf;

	/*check que*/
	if (!pqbuf->pque[qt]) {
		PR_ERR_Q("%s:pq[%s]f[%d] is null\n", __func__,
			 pqbuf->name, qt);
		return false;
	}

	pqt = pqbuf->pque[qt];
	q_buf = pqbuf->pbuf[data];

	return pqt->ops.in(pqbuf, pqt, q_buf);//pqt->ops.in(pqt, data);
}

bool qbuf_out(struct buf_que_s *pqbuf, unsigned int qt, unsigned int *pindex)
{
	struct qs_cls_s *pqt;
	bool ret;
	union q_buf_u q_buf;

	/*check que*/
	if (!pqbuf->pque[qt]) {
		PR_ERR_Q("%s:pq[%s]f[%d] is null\n", __func__,
			 pqbuf->name, qt);
		return false;
	}
	pqt = pqbuf->pque[qt];
	if (!pqt->ops.out) {
		PR_ERR_Q("pq[%s]:q[%s] no out\n", pqbuf->name,
			 pqt->name);
		return false;
	}

	ret = pqt->ops.out(pqbuf, pqt, &q_buf);
	if (ret)
		*pindex = q_buf.qbc->index;
	return ret;
}

bool qbuf_reset(struct buf_que_s *pqbuf)
{
	int i;
//	enum Q_TYPE etype;
	struct qs_cls_s *pqt;

	for (i = 0; i < pqbuf->nub_que; i++) {
		pqt = pqbuf->pque[i];
		if (!pqt) {
			PR_ERR("%s:qb[%s]:%d is null\n", __func__,
			       pqbuf->name, i);
			continue;
		}
		pqt->ops.reset(pqbuf, pqt);
	}

	return true;
}

bool qbuf_peek(struct buf_que_s *pqbuf, unsigned int qt,
	       unsigned int *buf_index)
{
//	enum Q_TYPE etype;
	struct qs_cls_s *pqt;
	union q_buf_u q_buf;
	bool ret;

	if (!pqbuf->pque[qt]) {
		//PR_ERR("%s:%s: no que %d\n", __func__, pqbuf->name, qt);
		return false;
	}

	//etype = pqbuf->pque[qt]->type;
	pqt = pqbuf->pque[qt];
	ret = pqt->ops.peek(pqbuf, pqt, &q_buf);
	if (ret)
		*buf_index = q_buf.qbc->index;
	return ret;
}

#ifdef HIS_CODE
/* pbuf is point */
bool qbuf_peek_s(struct buf_que_s *pqbuf, unsigned int qt,
		 union q_buf_u *pbuf)
{
	struct qs_cls_s *pqt;
	bool ret;

	pbuf = NULL;

	if (!pqbuf->pque[qt]) {
		PR_ERR("%s:%s: no que %d\n", __func__, pqbuf->name, qt);
		return false;
	}

	//etype = pqbuf->pque[qt]->type;
	pqt = pqbuf->pque[qt];
	ret = pqt->ops.peek(pqbuf, pqt, pbuf);

	return ret;
}
#endif

bool qbuf_in_all(struct buf_que_s *pqbuf, unsigned int qt)
{
	struct qs_cls_s *pqt;
	int i;

	/*check que*/
	if (!pqbuf->pque[qt]) {
		PR_ERR_Q("%s:pq[%s]f[%d] is null\n", __func__,
			 pqbuf->name, qt);
		return false;
	}

	pqt = pqbuf->pque[qt];
	for (i = 0; i < pqbuf->nub_buf; i++)
		pqt->ops.in(pqbuf, pqt, pqbuf->pbuf[i]);
#ifdef _HIS_CODE_
	if (pqt->ops.is_full(pqbuf, pqt)) {
		PR_ERR_Q("%s:pq[%s]:q[%s] is full %d:%d\n", __func__,
			 pqbuf->name, pqt->name, i,
			 pqt->ops.count(pqbuf, pqt));
		return false;
	}
#endif
	return true;
}

bool qbuf_n_is_in(struct buf_que_s *pqbuf,
		   unsigned int qt,
		   union q_buf_u q_buf)
{
	struct qs_cls_s *pqt;

	if (!pqbuf->pque[qt]) {
		PR_ERR("%s:%s: no que %d\n", __func__, pqbuf->name, qt);
		return false;
	}

	pqt = pqbuf->pque[qt];

	if (pqt->ops.is_in)
		return pqt->ops.is_in(pqbuf, pqt, q_buf);
	PR_ERR("%s:%s:no is_in function:\n", __func__, pqt->name);
	return false;
}

bool qbuf_out_some(struct buf_que_s *pqbuf,
		   unsigned int qt,
		   union q_buf_u q_buf)
{
	struct qs_cls_s *pqt;
	int i;
	unsigned int length;
	bool ret = false;
	bool reta;
	union q_buf_u bufq_c;

	if (!pqbuf->pque[qt]) {
		PR_ERR("%s:%s: no que %d\n", __func__, pqbuf->name, qt);
		return false;
	}

	pqt = pqbuf->pque[qt];

	if (pqt->ops.out_some)
		return pqt->ops.out_some(pqbuf, pqt, q_buf);

	length = pqt->ops.count(pqbuf, pqt);

	for (i = 0; i < length; i++) {
		reta = pqt->ops.out(pqbuf, pqt, &bufq_c);
		if (!reta)
			break;
		if (bufq_c.qbc->index == q_buf.qbc->index) {
			ret = true;
			break;
		}
		pqt->ops.in(pqbuf,  pqt, bufq_c);
	}

	return ret;
}

void qbuf_dbg_checkid(struct buf_que_s *pqbuf, unsigned int dbgid)
{
	int i;

	//PR_INF("%s\n", pqbuf->name);
	for (i = 0; i < pqbuf->nub_buf; i++) {
		if (!pqbuf->pbuf[i].qbc) {
			PR_ERR("%s:[%d]:[%d]no\n", pqbuf->name, dbgid, i);
			break;
		}

		if (i != pqbuf->pbuf[i].qbc->index) {
			PR_ERR("%s:[%d]id change [%d][%d]\n",
			       pqbuf->name,
			       dbgid, i, pqbuf->pbuf[i].qbc->index);
			break;
		}
		//PR_INF("\t:%d:%d\n", i, pqbuf->pbuf[i].qbc->index);
	}
}
/***********************************************************/
bool qbufp_move_some(struct buf_que_s *pqbuf, unsigned int qf,
		     unsigned int qt, union q_buf_u q_buf)
{
	struct qs_cls_s *pqf, *pqt;
	//enum Q_TYPE etypef, etypet;
	//union q_buf_u q_buf;

	/*check que*/
	if (!pqbuf->pque[qf]) {
		PR_ERR_Q("%s:pq[%s]f[%d] is null\n", __func__,
			 pqbuf->name, qf);
		return false;
	}

	if (!pqbuf->pque[qt]) {
		PR_ERR_Q("%s:pq[%s]t[%d] is null\n", __func__,
			 pqbuf->name, qt);
		return false;
	}

	pqf = pqbuf->pque[qf];
	pqt = pqbuf->pque[qt];
	//etypef = pqf->type;
	//etypet = pqt->type;

	/*check out_some*/
	if (!pqf->ops.out_some) {
		PR_ERR_Q("%s:qbuf[%s]:que[%s] no out_some\n", __func__,
			 pqbuf->name, pqf->name);
		return false;
	}
	/*check qt is full*/
	if (pqt->ops.is_full(pqbuf, pqt)) {
		PR_ERR_Q("%s:qbuf[%s]:q[%s] is full\n", __func__,
			 pqbuf->name, pqt->name);
		return false;
	}

	if (pqf->ops.out_some(pqbuf, pqf, q_buf))
		pqt->ops.in(pqbuf, pqt, q_buf);

	return true;
}

bool qbufp_move(struct buf_que_s *pqbuf, unsigned int qf,
		unsigned int qt, union q_buf_u *pbuf)
{
//	unsigned int buf_index;
	struct qs_cls_s *pqf, *pqt;
	bool ret = false;
	//enum Q_TYPE etypef, etypet;
	union q_buf_u q_buf;

	/*check que*/
	if (!pqbuf->pque[qf]) {
		PR_ERR_Q("%s:pq[%s]f[%d] is null\n", __func__,
			 pqbuf->name, qf);
		return false;
	}

	if (!pqbuf->pque[qt]) {
		PR_ERR_Q("%s:pq[%s]t[%d] is null\n", __func__,
			 pqbuf->name, qt);
		return false;
	}

	pqf = pqbuf->pque[qf];
	pqt = pqbuf->pque[qt];
	//etypef = pqf->type;
	//etypet = pqt->type;

	/*check out_some*/
	if (!pqf->ops.out) {
		PR_ERR_Q("%s:qbuf[%s]:que[%s] no out_some\n", __func__,
			 pqbuf->name, pqf->name);
		return false;
	}
	/*check qt is full*/
	if (pqt->ops.is_full(pqbuf, pqt)) {
		PR_ERR_Q("%s:qbuf[%s]:q[%s] is full\n", __func__,
			 pqbuf->name, pqt->name);
		return false;
	}

	if (pqf->ops.out(pqbuf, pqf, &q_buf)) {
		pqt->ops.in(pqbuf, pqt, q_buf);
		*pbuf = q_buf;
		ret = true;
	} else {
		PR_ERR_Q("%s:qb[%s],qf[%s], qt[%s]\n", __func__,
			 pqbuf->name, pqf->name, pqf->name);
	}

	return ret;
}

bool qbufp_in(struct buf_que_s *pqbuf, unsigned int qt, union q_buf_u q_buf)
{
	struct qs_cls_s *pqt;
	//union q_buf_u q_buf;

	/*check que*/
	if (!pqbuf->pque[qt]) {
		PR_ERR_Q("%s:pq[%s]f[%d] is null\n", __func__,
			 pqbuf->name, qt);
		return false;
	}

	pqt = pqbuf->pque[qt];
	//q_buf = pqbuf->pbuf[data];

	return pqt->ops.in(pqbuf, pqt, q_buf);//pqt->ops.in(pqt, data);
}

bool qbufp_out(struct buf_que_s *pqbuf, unsigned int qt, union q_buf_u *pbuf)
{
	struct qs_cls_s *pqt;
	bool ret;
	union q_buf_u q_buf;

	/*check que*/
	if (!pqbuf->pque[qt]) {
		PR_ERR_Q("%s:pq[%s]f[%d] is null\n", __func__,
			 pqbuf->name, qt);
		return false;
	}
	pqt = pqbuf->pque[qt];
	if (!pqt->ops.out) {
		PR_ERR_Q("pq[%s]:q[%s] no out\n", pqbuf->name,
			 pqt->name);
		return false;
	}

	ret = pqt->ops.out(pqbuf, pqt, &q_buf);
	if (ret)
		*pbuf = q_buf;
	return ret;
}

bool qbufp_peek(struct buf_que_s *pqbuf, unsigned int qt,
		union q_buf_u *pbuf)
{
//	enum Q_TYPE etype;
	struct qs_cls_s *pqt;
	union q_buf_u q_buf;
	bool ret;

	if (!pqbuf->pque[qt]) {
		PR_ERR("%s:%s: no que %d\n", __func__, pqbuf->name, qt);
		return false;
	}

	//etype = pqbuf->pque[qt]->type;
	pqt = pqbuf->pque[qt];
	ret = pqt->ops.peek(pqbuf, pqt, &q_buf);
	if (ret)
		*pbuf = q_buf;
	return ret;
}

bool qbufp_restq(struct buf_que_s *pqbuf, unsigned int qt)
{
	struct qs_cls_s *pqt;
//	union q_buf_u q_buf;
	bool ret;

	if (!pqbuf->pque[qt]) {
		PR_ERR("%s:%s: no que %d\n", __func__, pqbuf->name, qt);
		return false;
	}

	pqt = pqbuf->pque[qt];
	ret = pqt->ops.reset(pqbuf, pqt);
	return ret;
}

/* for qbuf_count too */
unsigned int qbufp_count(struct buf_que_s *pqbuf, unsigned int qt)
{
//	enum Q_TYPE etype;
	struct qs_cls_s *pqt;
//	union q_buf_u q_buf;
	unsigned int cnt;

	if (!pqbuf->pque[qt]) {
		PR_ERR("%s:%s: no que %d\n", __func__, pqbuf->name, qt);
		return false;
	}

	//etype = pqbuf->pque[qt]->type;
	pqt = pqbuf->pque[qt];
	cnt = pqt->ops.count(pqbuf, pqt);

	return cnt;
}

/* for qbuf_count too */
bool qbufp_out_some(struct buf_que_s *pqbuf,
		    unsigned int qt, union q_buf_u q_buf)
{
	struct qs_cls_s *pqt;
	int i;
	unsigned int length;
	bool ret = false;
	bool reta;
	union q_buf_u bufq_c;

	if (!pqbuf->pque[qt]) {
		PR_ERR("%s:%s: no que %d\n", __func__, pqbuf->name, qt);
		return false;
	}
	pqt = pqbuf->pque[qt];

	if (pqt->ops.out_some)
		return pqt->ops.out_some(pqbuf, pqt, q_buf);

	length = pqt->ops.count(pqbuf, pqt);

	for (i = 0; i < length; i++) {
		reta = pqt->ops.out(pqbuf, pqt, &bufq_c);
		if (!reta)
			break;
		if (bufq_c.qbc == q_buf.qbc) {
			ret = true;
			break;
		}
		pqt->ops.in(pqbuf,  pqt, bufq_c);
	}

	return ret;
}

bool qbufp_list(struct buf_que_s *pqbuf, unsigned int qt)
{
	struct qs_cls_s *pqt;
	unsigned int siz;
//	int i;
//	ud index;
//	struct qs_buf_s *qbc;

	if (!pqbuf->pque[qt]) {
		PR_ERR("%s:%s: no que %d\n", __func__, pqbuf->name, qt);
		return false;
	}
	pqt = pqbuf->pque[qt];
	if (pqt->ops.list)
		pqt->ops.list(pqbuf, pqt, &siz);

	#ifdef MARK_HIS
	PR_INF("%s:%s:\n", __func__, pqt->name);
	for (i = 0; i < MAX_FIFO_SIZE; i++) {
		if (pqbuf->list_ud[i]) {
			qbc = (struct qs_buf_s *)pqbuf->list_ud[i];
			PR_INF("\t%d:%d\n", i, qbc->index);
		} else {
			break;
		}
	}
	#endif
	return true;
}

#ifdef MARK_SC2 /* ary tmp */
int vfmtst_que_show(struct seq_file *s, void *what)
{
	char *splt = "---------------------------";
	unsigned int psize;
	int i, j;
	struct qs_cls_s *p;
	unsigned long flags;
	struct buf_que_s *pqbuf;

	pqbuf = &bufq_in;
	for (j = 0; j < pqbuf->nub_que; j++) {
		p = pqbuf->pque[j];
		p->ops.list(pqbuf, p, &psize);
		//pqbuf->opsb.list(pqbuf, j, &psize);
		seq_printf(s, "%s (crr %d):\n", p->name, psize);
		for (i = 0; i < psize; i++)
			seq_printf(s, "\t%2d,\n", pqbuf->list_id[i]);

		seq_printf(s, "%s\n", splt);
	}

	pqbuf = &bufq_o;
	for (j = 0; j < pqbuf->nub_que; j++) {
		spin_lock_irqsave(&dim_ready, flags);
		p = pqbuf->pque[j];
		//pqbuf->opsb.list(pqbuf, j, &psize);
		p->ops.list(pqbuf, p, &psize);
		spin_unlock_irqrestore(&dim_ready, flags);
		seq_printf(s, "%s (crr %d):\n", p->name, psize);
		for (i = 0; i < psize; i++)
			seq_printf(s, "\t%2d,\n", pqbuf->list_id[i]);

		seq_printf(s, "%s\n", splt);
	}
	return 0;
}
#endif
/**********************************************/

