// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/kfifo.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include "deinterlace.h"

#include "di_data_l.h"
#include "di_que.h"
#include "di_vframe.h"

#include "di_prc.h"

const char * const di_name_new_que[QUE_NUB] = {
	"QUE_IN_FREE",	/*0*/
	"QUE_PRE_READY",	/*1*/
	"QUE_POST_FREE",	/*2*/
	"QUE_POST_READY",	/*3*/
	"QUE_POST_BACK",	/*4*/
	"QUE_POST_DOING",
	"QUE_POST_KEEP",
	"QUE_POST_KEEP_BACK",
	"QUE_DBG",
/*	"QUE_NUB",*/

};

#define que_dbg		dim_print

void pw_queue_clear(unsigned int ch, enum QUE_TYPE qtype)
{
	struct di_ch_s *pch = get_chdata(ch);

#ifdef MARK_HIS
	if (qtype >= QUE_NUB)
		return;
#endif
	kfifo_reset(&pch->fifo[qtype]);
}

bool pw_queue_in(unsigned int ch, enum QUE_TYPE qtype, unsigned int buf_index)
{
	struct di_ch_s *pch = get_chdata(ch);

#ifdef MARK_HIS
	if (qtype >= QUE_NUB)
		return false;
#endif
	if (kfifo_in(&pch->fifo[qtype], &buf_index, sizeof(unsigned int))
		!= sizeof(unsigned int))
		return false;
#ifdef MARK_HIS

	/*below for debug: save in que*/
	if (qtype <= QUE_POST_RECYC) {
		if (buf_index >= MAX_POST_BUF_NUM) {
			pr_err("%s:err:overflow?[%d]\n", __func__, buf_index);
		} else {
			ppw = &pch->lpost_buf[buf_index];
			ppw->in_qtype = qtype;
		}
	}
#endif
	return true;
}

bool pw_queue_out(unsigned int ch, enum QUE_TYPE qtype,
		  unsigned int *buf_index)
{
	struct di_ch_s *pch = get_chdata(ch);
	unsigned int index;

#ifdef MARK_HIS
	if (qtype >= QUE_NUB)
		return false;
#endif
	if (kfifo_out(&pch->fifo[qtype], &index, sizeof(unsigned int))
		!= sizeof(unsigned int)) {
		PR_ERR("%s:ch[%d],qtye[%d],buf[%d]\n",
		       __func__, ch, qtype, *buf_index);
		return false;
	}

	*buf_index = index;

	return true;
}

static bool pw_queue_peek(unsigned int ch, enum QUE_TYPE qtype,
			  unsigned int *buf_index)
{
	struct di_ch_s *pch = get_chdata(ch);
	unsigned int index;

#ifdef MARK_HIS
	if (qtype >= QUE_NUB)
		return false;
#endif

	if (kfifo_out_peek(&pch->fifo[qtype], &index, sizeof(unsigned int))
		!= sizeof(unsigned int))
		return false;

	*buf_index = index;

	return true;
}

bool pw_queue_move(unsigned int ch, enum QUE_TYPE qtypef, enum QUE_TYPE qtypet,
		   unsigned int *oindex)
{
	struct di_ch_s *pch = get_chdata(ch);
	unsigned int index;

	/*struct di_post_buf_s *ppw;*/ /*debug only*/

#ifdef MARK_HIS
	if (qtypef >= QUE_NUB || qtypet >= QUE_NUB)
		return false;
#endif
	if (kfifo_out(&pch->fifo[qtypef], &index, sizeof(unsigned int))
		!= sizeof(unsigned int)) {
		PR_ERR("qtypef[%d] is empty\n", qtypef);
		return false;
	}
	if (kfifo_in(&pch->fifo[qtypet], &index, sizeof(unsigned int))
			!= sizeof(unsigned int)) {
		PR_ERR("qtypet[%d] is full\n", qtypet);
		return false;
	}

	*oindex = index;
#ifdef MARK_HIS
	if (qtypet <= QUE_POST_RECYC) {
		/*below for debug: save in que*/
		if (index >= MAX_POST_BUF_NUM) {
			pr_err("%s:err:overflow?[%d]\n", __func__, index);
		} else {
			ppw = &pch->lpost_buf[index];
			ppw->in_qtype = qtypet;
		}
	}
#endif
	return true;
}

bool pw_queue_empty(unsigned int ch, enum QUE_TYPE qtype)
{
	struct di_ch_s *pch = get_chdata(ch);

	if (kfifo_is_empty(&pch->fifo[qtype]))
		return true;

	return false;
}

/**********************************************************
 *
 **********************************************************/
int di_que_list_count(unsigned int ch, enum QUE_TYPE qtype)
{
	struct di_ch_s *pch = get_chdata(ch);
	unsigned int length;

#ifdef MARK_HIS
	if (qtype >= QUE_NUB)
		return -1;
#endif
	length = kfifo_len(&pch->fifo[qtype]);
	length = length / sizeof(unsigned int);

	return length;
}

/***************************************/
/*outbuf : array size MAX_FIFO_SIZE*/
/***************************************/
bool di_que_list(unsigned int ch, enum QUE_TYPE qtype, unsigned int *outbuf,
		 unsigned int *rsize)
{
	struct di_ch_s *pch = get_chdata(ch);
/*	unsigned int tmp[MAX_FIFO_SIZE + 1];*/
	int i;
	unsigned int index;
	bool ret = false;

	/*que_dbg("%s:begin\n", __func__);*/
	for (i = 0; i < MAX_FIFO_SIZE; i++)
		outbuf[i] = 0xff;

	if (kfifo_is_empty(&pch->fifo[qtype])) {
		que_dbg("\t%d:empty\n", qtype);
		*rsize = 0;
		return true;
	}

	ret = true;
	memcpy(&pch->fifo[QUE_DBG], &pch->fifo[qtype],
	       sizeof(pch->fifo[qtype]));

#ifdef MARK_HIS
	if (kfifo_is_empty(&pbm->fifo[QUE_DBG]))
		pr_err("%s:err, kfifo can not copy?\n", __func__);

#endif
	i = 0;
	*rsize = 0;

	while (kfifo_out(&pch->fifo[QUE_DBG], &index, sizeof(unsigned int))
		== sizeof(unsigned int)) {
		outbuf[i] = index;
		/*pr_info("%d->%d\n",i,index);*/
		i++;
	}
	*rsize = di_que_list_count(ch, qtype);
	#ifdef MARK_HIS	/*debug only*/
	que_dbg("%s: size[%d]\n", di_name_new_que[qtype], *rsize);
	for (i = 0; i < *rsize; i++)
		que_dbg("%d,", outbuf[i]);

	que_dbg("\n");
	#endif
	/*que_dbg("finish\n");*/

	return ret;
}

int di_que_is_empty(unsigned int ch, enum QUE_TYPE qtype)
{
	struct di_ch_s *pch = get_chdata(ch);

#ifdef MARK_HIS
	if (qtype >= QUE_NUB)
		return -1;
#endif
	return kfifo_is_empty(&pch->fifo[qtype]);
}

void di_que_init(unsigned int ch)
{
	int i;

	for (i = 0; i < QUE_NUB; i++) {
		if (i == QUE_POST_KEEP ||
		    i == QUE_POST_KEEP_BACK)
			continue;
		pw_queue_clear(ch, i);
	}
}

bool di_que_alloc(unsigned int ch)
{
	int i;
	int ret;
	bool flg_err;
	struct di_ch_s *pch = get_chdata(ch);

	/*kfifo----------------------------*/
	flg_err = 0;
	for (i = 0; i < QUE_NUB; i++) {
		ret = kfifo_alloc(&pch->fifo[i],
				  sizeof(unsigned int) * MAX_FIFO_SIZE,
				  GFP_KERNEL);
		if (ret < 0) {
			flg_err = 1;
			PR_ERR("%s:%d:can't get kfifo\n", __func__, i);
			break;
		}
		pch->flg_fifo[i] = 1;
	}
#ifdef MARK_HIS	/*canvas-----------------------------*/
	canvas_alloc();
#endif
/*	pdp_clear();*/

	if (!flg_err) {
		/*pbm->flg_fifo = 1;*/
		pr_info("%s:ok\n", __func__);
		ret = true;
	} else {
		di_que_release(ch);
		ret = false;
	}

	return ret;
}

void di_que_release(unsigned int ch)
{
	struct di_ch_s *pch = get_chdata(ch);
	int i;

/*	canvas_release();*/
	for (i = 0; i < QUE_NUB; i++) {
		if (pch->flg_fifo[i]) {
			kfifo_free(&pch->fifo[i]);
			pch->flg_fifo[i] = 0;
		}
	}

	pr_info("%s:ok\n", __func__);
}

/********************************************
 *get di_buf from index that same in que
 *	(di_buf->type << 8) | (di_buf->index)
 ********************************************/
struct di_buf_s *pw_qindex_2_buf(unsigned int ch, unsigned int qindex)
{
	union UDI_QBUF_INDEX index;
	struct di_buf_s *di_buf;
	struct di_buf_pool_s *pbuf_pool = get_buf_pool(ch);

	index.d32 = qindex;
	di_buf = &pbuf_pool[index.b.type - 1].di_buf_ptr[index.b.index];

	return di_buf;
}

/********************************************/
/*get di_buf from index that same in que*/
/*(di_buf->type << 8) | (di_buf->index)*/
/********************************************/
static unsigned int pw_buf_2_qindex(unsigned int ch, struct di_buf_s *pdi_buf)
{
	union	UDI_QBUF_INDEX index;

	index.b.index = pdi_buf->index;
	index.b.type = pdi_buf->type;
	return index.d32;
}

/*di_buf is out*/
struct di_buf_s *di_que_out_to_di_buf(unsigned int ch, enum QUE_TYPE qtype)
{
	unsigned int q_index;
	struct di_buf_s *pdi_buf = NULL;

	if (!pw_queue_peek(ch, qtype, &q_index)) {
		PR_ERR("%s:no buf\n", __func__);
		return pdi_buf;
	}

	pdi_buf = pw_qindex_2_buf(ch, q_index);
	if (!pdi_buf) {
		PR_ERR("%s:buf is null[%d]\n", __func__, q_index);
		return NULL;
	}

	pw_queue_out(ch, qtype, &q_index);
	pdi_buf->queue_index = -1;

	return pdi_buf;
}

/*di_buf is input*/
bool di_que_out(unsigned int ch, enum QUE_TYPE qtype, struct di_buf_s *di_buf)
{
	unsigned int q_index;
	unsigned int q_index2;

	if (!pw_queue_peek(ch, qtype, &q_index)) {
		PR_ERR("%s:no buf ch[%d], qtype[%d], buf[%d,%d]\n", __func__,
		       ch, qtype, di_buf->type, di_buf->index);
		return false;
	}
	q_index2 = pw_buf_2_qindex(ch, di_buf);
	if (q_index2 != q_index) {
		PR_ERR("di:%s:%d not map[0x%x,0x%x]\n", __func__,
		       qtype, q_index2, q_index);
		return false;
	}

	pw_queue_out(ch, qtype, &q_index);
	di_buf->queue_index = -1;
	return true;
}

bool di_que_in(unsigned int ch, enum QUE_TYPE qtype, struct di_buf_s *di_buf)
{
	unsigned int q_index;

	if (!di_buf) {
		PR_ERR("di:%s:err:di_buf is NULL,ch[%d],qtype[%d]\n",
		       __func__, ch, qtype);
		return false;
	}
	if (di_buf->queue_index != -1) {
		PR_ERR("%s:buf in some que,ch[%d],qt[%d],qi[%d],bi[%d]\n",
		       __func__,
		       ch, qtype, di_buf->queue_index, di_buf->index);
		dump_stack();
		di_buf->queue_index = -1;
	}

	q_index = pw_buf_2_qindex(ch, di_buf);

	if (!pw_queue_in(ch, qtype, q_index)) {
		PR_ERR("%s:can't que in,ch[%d],qtype[%d],q_index[%d]\n",
		       __func__,
		       ch, qtype, q_index);
		return false;
	}
	di_buf->queue_index = qtype + QUEUE_NUM;

	if (qtype == QUE_PRE_READY)
		dim_print("di:pre_ready in %d\n", di_buf->index);

	return true;
}

bool di_que_is_in_que(unsigned int ch, enum QUE_TYPE qtype,
		      struct di_buf_s *di_buf)
{
	unsigned int q_index;
	unsigned int arr[MAX_FIFO_SIZE + 1];
	unsigned int asize = 0;
	bool ret = false;
	unsigned int i;

	if (!di_buf)
		return false;

	q_index = pw_buf_2_qindex(ch, di_buf);

	di_que_list(ch, qtype, &arr[0], &asize);

	if (asize == 0)
		return ret;

	for (i = 0; i < asize; i++) {
		if (arr[i] == q_index) {
			ret = true;
			break;
		}
	}
	return ret;
}

/* clear and rebuild que*/
bool di_que_out_not_fifo(unsigned int ch, enum QUE_TYPE qtype,
			 struct di_buf_s *di_buf)
{
	unsigned int q_index;
	unsigned int arr[MAX_FIFO_SIZE + 1];
	unsigned int asize = 0;
	unsigned int i;
	bool ret = false;

	if (!pw_queue_peek(ch, qtype, &q_index))
		return false;

	q_index = pw_buf_2_qindex(ch, di_buf);

	di_que_list(ch, qtype, &arr[0], &asize);

	pw_queue_clear(ch, qtype);

	if (asize == 0) {
		PR_ERR("%s:size 0\n", __func__);
		return ret;
	}

	for (i = 0; i < asize; i++) {
		if (arr[i] == q_index) {
			ret = true;
			di_buf->queue_index = -1;
			continue;
		}
		pw_queue_in(ch, qtype, arr[i]);
	}
	return ret;
}

/*same as get_di_buf_head*/
struct di_buf_s *di_que_peek(unsigned int ch, enum QUE_TYPE qtype)
{
	struct di_buf_s *di_buf = NULL;
	unsigned int q_index;

	if (!pw_queue_peek(ch, qtype, &q_index))
		return di_buf;
	di_buf = pw_qindex_2_buf(ch, q_index);

	return di_buf;
}

bool di_que_type_2_new(unsigned int q_type, enum QUE_TYPE *nqtype)
{
	if (!F_IN(q_type, QUEUE_NEW_THD_MIN, QUEUE_NEW_THD_MAX))
		return false;
	*nqtype = (enum QUE_TYPE)(q_type - QUEUE_NUM);

	return true;
}

/**********************************************************/
/**********************************************************/
/*ary add this function for reg ini value, no need wait peek*/
void queue_init2(unsigned int channel)
{
	int i, j;
	struct queue_s *pqueue = get_queue(channel);

	for (i = 0; i < QUEUE_NUM; i++) {
		queue_t *q = &pqueue[i];

		for (j = 0; j < MAX_QUEUE_POOL_SIZE; j++)
			q->pool[j] = 0;

		q->in_idx = 0;
		q->out_idx = 0;
		q->num = 0;
		q->type = 0;
		if (i == QUEUE_RECYCLE ||
		    i == QUEUE_DISPLAY ||
		    i == QUEUE_TMP     ||
		    i == QUEUE_POST_DOING)
			q->type = 1;

#ifdef MARK_HIS
		if (i == QUEUE_LOCAL_FREE && dim_get_use_2_int_buf())
			q->type = 2;
#endif
	}
}

void queue_init(unsigned int channel, int local_buffer_num)
{
	int i, j;
	struct di_buf_s *pbuf_local = get_buf_local(channel);
	struct di_buf_s *pbuf_in = get_buf_in(channel);
	struct di_buf_s *pbuf_post = get_buf_post(channel);
	struct queue_s *pqueue = get_queue(channel);
	struct di_buf_pool_s *pbuf_pool = get_buf_pool(channel);

	for (i = 0; i < QUEUE_NUM; i++) {
		queue_t *q = &pqueue[i];

		for (j = 0; j < MAX_QUEUE_POOL_SIZE; j++)
			q->pool[j] = 0;

		q->in_idx = 0;
		q->out_idx = 0;
		q->num = 0;
		q->type = 0;
		if (i == QUEUE_RECYCLE ||
		    i == QUEUE_DISPLAY ||
		    i == QUEUE_TMP
		    /*||(i == QUEUE_POST_DOING)*/
		    )
			q->type = 1;

		if (i == QUEUE_LOCAL_FREE &&
		    dimp_get(edi_mp_use_2_interlace_buff))
			q->type = 2;
	}
	if (local_buffer_num > 0) {
		pbuf_pool[VFRAME_TYPE_IN - 1].di_buf_ptr = &pbuf_in[0];
		pbuf_pool[VFRAME_TYPE_IN - 1].size = MAX_IN_BUF_NUM;

		pbuf_pool[VFRAME_TYPE_LOCAL - 1].di_buf_ptr = &pbuf_local[0];
		pbuf_pool[VFRAME_TYPE_LOCAL - 1].size = local_buffer_num;

		pbuf_pool[VFRAME_TYPE_POST - 1].di_buf_ptr = &pbuf_post[0];
		pbuf_pool[VFRAME_TYPE_POST - 1].size = MAX_POST_BUF_NUM;
	}
}

struct di_buf_s *get_di_buf_head(unsigned int channel, int queue_idx)
{
	struct queue_s *pqueue = get_queue(channel);
	queue_t *q = &pqueue[queue_idx];
	int idx;
	unsigned int pool_idx, di_buf_idx;
	struct di_buf_s *di_buf = NULL;
	struct di_buf_pool_s *pbuf_pool = get_buf_pool(channel);
	enum QUE_TYPE nqtype;/*new que*/

	if (dimp_get(edi_mp_di_log_flag) & DI_LOG_QUEUE)
		dim_print("%s:<%d:%d,%d,%d>\n", __func__, queue_idx,
			  q->num, q->in_idx, q->out_idx);
	/* ****new que***** */
	if (di_que_type_2_new(queue_idx, &nqtype))
		return di_que_peek(channel, nqtype);

	/* **************** */

	if (q->num > 0) {
		if (q->type == 0) {
			idx = q->out_idx;
		} else {
			for (idx = 0; idx < MAX_QUEUE_POOL_SIZE; idx++)
				if (q->pool[idx] != 0)
					break;
		}
		if (idx < MAX_QUEUE_POOL_SIZE) {
			pool_idx = ((q->pool[idx] >> 8) & 0xff) - 1;
			di_buf_idx = q->pool[idx] & 0xff;

	if (pool_idx < VFRAME_TYPE_NUM) {
		if (di_buf_idx < pbuf_pool[pool_idx].size)
			di_buf = &pbuf_pool[pool_idx].di_buf_ptr[di_buf_idx];
	}
		}
	}

	if ((di_buf) && ((((pool_idx + 1) << 8) | di_buf_idx) !=
			 ((di_buf->type << 8) | di_buf->index))) {
		pr_dbg("%s: Error (%x,%x)\n", __func__,
		       (((pool_idx + 1) << 8) | di_buf_idx),
		       ((di_buf->type << 8) | (di_buf->index)));

		if (dim_vcry_get_flg() == 0) {
			dim_vcry_set_log_reason(2);
			dim_vcry_set_log_q_idx(queue_idx);
			dim_vcry_set_log_di_buf(di_buf);
		}
		dim_vcry_flg_inc();
		di_buf = NULL;
	}

	if (dimp_get(edi_mp_di_log_flag) & DI_LOG_QUEUE) {
		if (di_buf)
			dim_print("%s: 0x%p(%d,%d)\n", __func__, di_buf,
				  pool_idx, di_buf_idx);
		else
			dim_print("%s: 0x%p\n", __func__, di_buf);
	}

	return di_buf;
}

/*ary: note:*/
/*a. di_buf->queue_index = -1*/
/*b. */
void queue_out(unsigned int channel, struct di_buf_s *di_buf)
{
	int i;
	queue_t *q;
	struct queue_s *pqueue = get_queue(channel);
	enum QUE_TYPE nqtype;/*new que*/

	if (!di_buf) {
		PR_ERR("%s:Error\n", __func__);

		if (dim_vcry_get_flg() == 0)
			dim_vcry_set_log_reason(3);

		dim_vcry_flg_inc();
		return;
	}
	/* ****new que***** */
	if (di_que_type_2_new(di_buf->queue_index, &nqtype)) {
		di_que_out(channel, nqtype, di_buf);	/*?*/
		return;
	}
	/* **************** */

	if (di_buf->queue_index >= 0 && di_buf->queue_index < QUEUE_NUM) {
		q = &pqueue[di_buf->queue_index];

		if (dimp_get(edi_mp_di_log_flag) & DI_LOG_QUEUE)
			dim_print("%s:<%d:%d,%d,%d> 0x%p\n", __func__,
				  di_buf->queue_index, q->num, q->in_idx,
				  q->out_idx, di_buf);

		if (q->num > 0) {
			if (q->type == 0) {
				if (q->pool[q->out_idx] ==
				    ((di_buf->type << 8) | di_buf->index)) {
					q->num--;
					q->pool[q->out_idx] = 0;
					q->out_idx++;
					if (q->out_idx >= MAX_QUEUE_POOL_SIZE)
						q->out_idx = 0;
					di_buf->queue_index = -1;
				} else {
					PR_ERR("%s: Error (%d, %x,%x)\n",
					       __func__,
					       di_buf->queue_index,
					       q->pool[q->out_idx],
					       ((di_buf->type << 8) |
					       (di_buf->index)));

			if (dim_vcry_get_flg() == 0) {
				dim_vcry_set_log_reason(4);
				dim_vcry_set_log_q_idx(di_buf->queue_index);
				dim_vcry_set_log_di_buf(di_buf);
			}
					dim_vcry_flg_inc();
				}
			} else if (q->type == 1) {
				int pool_val =
					(di_buf->type << 8) | (di_buf->index);
				for (i = 0; i < MAX_QUEUE_POOL_SIZE; i++) {
					if (q->pool[i] == pool_val) {
						q->num--;
						q->pool[i] = 0;
						di_buf->queue_index = -1;
						break;
					}
				}
				if (i == MAX_QUEUE_POOL_SIZE) {
					PR_ERR("%s: Error\n", __func__);

			if (dim_vcry_get_flg() == 0) {
				dim_vcry_set_log_reason(5);
				dim_vcry_set_log_q_idx(di_buf->queue_index);
				dim_vcry_set_log_di_buf(di_buf);
			}
					dim_vcry_flg_inc();
				}
			} else if (q->type == 2) {
				int pool_val =
					(di_buf->type << 8) | (di_buf->index);
				if (di_buf->index < MAX_QUEUE_POOL_SIZE &&
				    q->pool[di_buf->index] == pool_val) {
					q->num--;
					q->pool[di_buf->index] = 0;
					di_buf->queue_index = -1;
				} else {
					PR_ERR("%s: Error\n", __func__);

		if (dim_vcry_get_flg() == 0) {
			dim_vcry_set_log_reason(5);
			dim_vcry_set_log_q_idx(di_buf->queue_index);
			dim_vcry_set_log_di_buf(di_buf);
		}
					dim_vcry_flg_inc();
				}
			}
		}
	} else {
		PR_ERR("%s: queue_index %d is not right\n",
		       __func__, di_buf->queue_index);

		if (dim_vcry_get_flg() == 0) {
			dim_vcry_set_log_reason(6);
			dim_vcry_set_log_q_idx(0);
			dim_vcry_set_log_di_buf(di_buf);
		}
		dim_vcry_flg_inc();
	}

	if (dimp_get(edi_mp_di_log_flag) & DI_LOG_QUEUE)
		dim_print("%s done\n", __func__);
}

void queue_out_dbg(unsigned int channel, struct di_buf_s *di_buf)
{
	int i;
	queue_t *q;
	struct queue_s *pqueue = get_queue(channel);
	enum QUE_TYPE nqtype;/*new que*/

	if (!di_buf) {
		PR_ERR("%s:Error\n", __func__);

		if (dim_vcry_get_flg() == 0)
			dim_vcry_set_log_reason(3);

		dim_vcry_flg_inc();
		return;
	}
	/* ****new que***** */
	if (di_que_type_2_new(di_buf->queue_index, &nqtype)) {
		di_que_out(channel, nqtype, di_buf);	/*?*/
		pr_info("dbg1:nqtype=%d\n", nqtype);
		return;
	}
	/* **************** */

	if (di_buf->queue_index >= 0 && di_buf->queue_index < QUEUE_NUM) {
		q = &pqueue[di_buf->queue_index];

		if (dimp_get(edi_mp_di_log_flag) & DI_LOG_QUEUE)
			dim_print("%s:<%d:%d,%d,%d> 0x%p\n", __func__,
				  di_buf->queue_index, q->num, q->in_idx,
				  q->out_idx, di_buf);

		if (q->num > 0) {
			if (q->type == 0) {
				pr_info("dbg3\n");
				if (q->pool[q->out_idx] ==
				    ((di_buf->type << 8) | di_buf->index)) {
					q->num--;
					q->pool[q->out_idx] = 0;
					q->out_idx++;
					if (q->out_idx >= MAX_QUEUE_POOL_SIZE)
						q->out_idx = 0;
					di_buf->queue_index = -1;
				} else {
					PR_ERR("%s: Error (%d, %x,%x)\n",
					       __func__,
					       di_buf->queue_index,
					       q->pool[q->out_idx],
					       ((di_buf->type << 8) |
					       (di_buf->index)));

			if (dim_vcry_get_flg() == 0) {
				dim_vcry_set_log_reason(4);
				dim_vcry_set_log_q_idx(di_buf->queue_index);
				dim_vcry_set_log_di_buf(di_buf);
			}
					dim_vcry_flg_inc();
				}
			} else if (q->type == 1) {
				int pool_val =
					(di_buf->type << 8) | (di_buf->index);
				for (i = 0; i < MAX_QUEUE_POOL_SIZE; i++) {
					if (q->pool[i] == pool_val) {
						q->num--;
						q->pool[i] = 0;
						di_buf->queue_index = -1;
						break;
					}
				}
				pr_info("dbg2:i=%d,qindex=%d\n", i,
					di_buf->queue_index);
				if (i == MAX_QUEUE_POOL_SIZE) {
					PR_ERR("%s: Error\n", __func__);

			if (dim_vcry_get_flg() == 0) {
				dim_vcry_set_log_reason(5);
				dim_vcry_set_log_q_idx(di_buf->queue_index);
				dim_vcry_set_log_di_buf(di_buf);
			}
					dim_vcry_flg_inc();
				}
			} else if (q->type == 2) {
				int pool_val =
					(di_buf->type << 8) | (di_buf->index);

				pr_info("dbg4\n");
				if (di_buf->index < MAX_QUEUE_POOL_SIZE &&
				    q->pool[di_buf->index] == pool_val) {
					q->num--;
					q->pool[di_buf->index] = 0;
					di_buf->queue_index = -1;
				} else {
					PR_ERR("%s: Error\n", __func__);

					if (dim_vcry_get_flg() == 0) {
						dim_vcry_set
							(5,
							 di_buf->queue_index,
							 di_buf);
					}
					dim_vcry_flg_inc();
				}
			}
		}
	} else {
		PR_ERR("%s: Error, queue_index %d is not right\n",
		       __func__, di_buf->queue_index);

		if (dim_vcry_get_flg() == 0) {
			dim_vcry_set_log_reason(6);
			dim_vcry_set_log_q_idx(0);
			dim_vcry_set_log_di_buf(di_buf);
		}
		dim_vcry_flg_inc();
	}

	if (dimp_get(edi_mp_di_log_flag) & DI_LOG_QUEUE)
		dim_print("%s done\n", __func__);
}

/***************************************/
/* set di_buf->queue_index*/
/***************************************/
void queue_in(unsigned int channel, struct di_buf_s *di_buf, int queue_idx)
{
	queue_t *q = NULL;
	struct queue_s *pqueue = get_queue(channel);
	enum QUE_TYPE nqtype;/*new que*/

	if (!di_buf) {
		PR_ERR("%s:Error\n", __func__);
		if (dim_vcry_get_flg() == 0) {
			dim_vcry_set_log_reason(7);
			dim_vcry_set_log_q_idx(queue_idx);
			dim_vcry_set_log_di_buf(di_buf);
		}
		dim_vcry_flg_inc();
		return;
	}
	/* ****new que***** */
	if (di_que_type_2_new(queue_idx, &nqtype)) {
		di_que_in(channel, nqtype, di_buf);
		return;
	}
	/* **************** */
	if (di_buf->queue_index != -1) {
		PR_ERR("%s:%s[%d] queue_index(%d) is not -1, to que[%d]\n",
		       __func__, dim_get_vfm_type_name(di_buf->type),
		       di_buf->index, di_buf->queue_index, queue_idx);
		if (dim_vcry_get_flg() == 0) {
			dim_vcry_set_log_reason(8);
			dim_vcry_set_log_q_idx(queue_idx);
			dim_vcry_set_log_di_buf(di_buf);
		}
		dim_vcry_flg_inc();
		return;
	}
	q = &pqueue[queue_idx];
	if (dimp_get(edi_mp_di_log_flag) & DI_LOG_QUEUE)
		dim_print("%s:<%d:%d,%d,%d> 0x%p\n", __func__, queue_idx,
			  q->num, q->in_idx, q->out_idx, di_buf);

	if (q->type == 0) {
		q->pool[q->in_idx] = (di_buf->type << 8) | (di_buf->index);
		di_buf->queue_index = queue_idx;
		q->in_idx++;
		if (q->in_idx >= MAX_QUEUE_POOL_SIZE)
			q->in_idx = 0;

		q->num++;
	} else if (q->type == 1) {
		int i;

		for (i = 0; i < MAX_QUEUE_POOL_SIZE; i++) {
			if (q->pool[i] == 0) {
				q->pool[i] =
					(di_buf->type << 8) | (di_buf->index);
				di_buf->queue_index = queue_idx;
				q->num++;
				break;
			}
		}
		if (i == MAX_QUEUE_POOL_SIZE) {
			pr_dbg("%s: Error\n", __func__);
			if (dim_vcry_get_flg() == 0) {
				dim_vcry_set_log_reason(9);
				dim_vcry_set_log_q_idx(queue_idx);
			}
			dim_vcry_flg_inc();
		}
	} else if (q->type == 2) {
		if (di_buf->index < MAX_QUEUE_POOL_SIZE &&
		    q->pool[di_buf->index] == 0) {
			q->pool[di_buf->index] =
				(di_buf->type << 8) | (di_buf->index);
			di_buf->queue_index = queue_idx;
			q->num++;
		} else {
			pr_dbg("%s: Error\n", __func__);
			if (dim_vcry_get_flg() == 0) {
				dim_vcry_set_log_reason(9);
				dim_vcry_set_log_q_idx(queue_idx);
			}
			dim_vcry_flg_inc();
		}
	}

	if (dimp_get(edi_mp_di_log_flag) & DI_LOG_QUEUE)
		dim_print("%s done\n", __func__);
}

int list_count(unsigned int channel, int queue_idx)
{
	struct queue_s *pqueue;
	enum QUE_TYPE nqtype;/*new que*/

	/* ****new que***** */
	if (di_que_type_2_new(queue_idx, &nqtype)) {
		PR_ERR("%s:err: over flow\n", __func__);
		return di_que_list_count(channel, nqtype);
	}
	/* **************** */

	pqueue = get_queue(channel);
	return pqueue[queue_idx].num;
}

bool queue_empty(unsigned int channel, int queue_idx)
{
	struct queue_s *pqueue;
	bool ret;
	enum QUE_TYPE nqtype;/*new que*/

	/* ****new que***** */
	if (di_que_type_2_new(queue_idx, &nqtype)) {
		PR_ERR("%s:err: over flow\n", __func__);
		return di_que_is_empty(channel, nqtype);
	}
	/* **************** */

	pqueue = get_queue(channel);
	ret = (pqueue[queue_idx].num == 0);

	return ret;
}

bool is_in_queue(unsigned int channel, struct di_buf_s *di_buf, int queue_idx)
{
	bool ret = 0;
	struct di_buf_s *p = NULL;
	int itmp;
	unsigned int overflow_cnt;
	enum QUE_TYPE nqtype;/*new que*/

	/* ****new que***** */
	if (di_que_type_2_new(queue_idx, &nqtype))
		return di_que_is_in_que(channel, nqtype, di_buf);

	/* **************** */

	overflow_cnt = 0;
	if (!di_buf || queue_idx < 0 || queue_idx >= QUEUE_NUM) {
		ret = 0;
		dim_print("%s: not in queue:%d!!!\n", __func__, queue_idx);
		return ret;
	}
	queue_for_each_entry(p, channel, queue_idx, list) {
		if (p == di_buf) {
			ret = 1;
			break;
		}
		if (overflow_cnt++ > MAX_QUEUE_POOL_SIZE) {
			ret = 0;
			dim_print("%s: overflow_cnt!!!\n", __func__);
			break;
		}
	}
	return ret;
}

