/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/di_que.h
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

#ifndef __DI_QUE_H__
#define __DI_QUE_H__

void queue_init(unsigned int channel, int local_buffer_num);
void queue_out(unsigned int channel, struct di_buf_s *di_buf);
void queue_in(unsigned int channel, struct di_buf_s *di_buf,
	      int queue_idx);
int list_count(unsigned int channel, int queue_idx);
bool queue_empty(unsigned int channel, int queue_idx);
bool is_in_queue(unsigned int channel, struct di_buf_s *di_buf,
		 int queue_idx);
struct di_buf_s *get_di_buf_head(unsigned int channel,
				 int queue_idx);

void queue_init2(unsigned int channel);

/*new buf:*/
bool pw_queue_in(unsigned int ch, enum QUE_TYPE qtype,
		 unsigned int buf_index);
bool pw_queue_out(unsigned int ch, enum QUE_TYPE qtype,
		  unsigned int *buf_index);
bool pw_queue_empty(unsigned int ch, enum QUE_TYPE qtype);
void pw_queue_clear(unsigned int ch, enum QUE_TYPE qtype);

/******************************************/
/*new api*/
/******************************************/
union   UDI_QBUF_INDEX {
	unsigned int d32;
	struct {
		unsigned int index:8,	/*low*/
			     type:8,
			     reserved0:16;
	} b;
};

void di_que_init(unsigned int ch);
bool di_que_alloc(unsigned int ch);
void di_que_release(unsigned int ch);

int di_que_is_empty(unsigned int ch, enum QUE_TYPE qtype);
bool di_que_out(unsigned int ch, enum QUE_TYPE qtype,
		struct di_buf_s *di_buf);

struct di_buf_s *di_que_out_to_di_buf(unsigned int ch,
				      enum QUE_TYPE qtype);
bool di_que_out_not_fifo(unsigned int ch, enum QUE_TYPE qtype,
			 struct di_buf_s *di_buf);

bool di_que_in(unsigned int ch, enum QUE_TYPE qtype,
	       struct di_buf_s *di_buf);
bool di_que_is_in_que(unsigned int ch, enum QUE_TYPE qtype,
		      struct di_buf_s *di_buf);
struct di_buf_s *di_que_peek(unsigned int ch, enum QUE_TYPE qtype);
bool di_que_type_2_new(unsigned int q_type, enum QUE_TYPE *nqtype);
int di_que_list_count(unsigned int ch, enum QUE_TYPE qtype);
bool di_que_list(unsigned int ch, enum QUE_TYPE qtype,
		 unsigned int *outbuf, unsigned int *rsize);

struct di_buf_s *pw_qindex_2_buf(unsigned int ch, unsigned int qindex);

void queue_out_dbg(unsigned int channel, struct di_buf_s *di_buf);

/* di_que_buf */
void qbuf_int(struct buf_que_s *pbufq, const struct que_creat_s *cfg,
	      const struct qbuf_creat_s *cfg_qbuf);
bool qbuf_release_que(struct buf_que_s *pqbuf);
bool qbuf_is_empty(struct buf_que_s *pqbuf, unsigned int qindex);
bool qbuf_is_full(struct buf_que_s *pqbuf, unsigned int qindex);
bool qbuf_move_some(struct buf_que_s *pqbuf, unsigned int qf,
		    unsigned int qt, unsigned int index);
bool qbuf_move(struct buf_que_s *pqbuf, unsigned int qf,
	       unsigned int qt, unsigned int *oindex);
bool qbuf_in(struct buf_que_s *pqbuf, unsigned int qt, unsigned int data);
bool qbuf_out(struct buf_que_s *pqbuf, unsigned int qt, unsigned int *pindex);
bool qbuf_reset(struct buf_que_s *pqbuf);
bool qbuf_peek(struct buf_que_s *pqbuf, unsigned int qt,
	       unsigned int *buf_index);
//bool qbuf_peek_s(struct buf_que_s *pqbuf, unsigned int qt,
//		 union q_buf_u *pbuf);

bool qbuf_in_all(struct buf_que_s *pqbuf, unsigned int qt);
bool qbuf_out_some(struct buf_que_s *pqbuf,
		   unsigned int qt, union q_buf_u q_buf);
void qbuf_dbg_checkid(struct buf_que_s *pqbuf, unsigned int dbgid);

bool qbufp_move_some(struct buf_que_s *pqbuf, unsigned int qf,
		     unsigned int qt, union q_buf_u q_buf);
bool qbufp_move(struct buf_que_s *pqbuf, unsigned int qf,
		unsigned int qt, union q_buf_u *pbuf);
bool qbufp_in(struct buf_que_s *pqbuf, unsigned int qt, union q_buf_u q_buf);
bool qbufp_out(struct buf_que_s *pqbuf, unsigned int qt, union q_buf_u *pbuf);
bool qbufp_peek(struct buf_que_s *pqbuf, unsigned int qt,
		union q_buf_u *pbuf);
unsigned int qbufp_count(struct buf_que_s *pqbuf, unsigned int qt);
bool qbufp_restq(struct buf_que_s *pqbuf, unsigned int qt);
bool qbufp_out_some(struct buf_que_s *pqbuf,
		    unsigned int qt, union q_buf_u q_buf);
bool qbufp_list(struct buf_que_s *pqbuf,
		unsigned int qt);

void qfp_int(struct qs_cls_s	*pq,
	      unsigned char *qname,
	      unsigned int lock);
bool qfp_release(struct qs_cls_s *pq);
/* dbg only*/
unsigned int qfp_list(struct qs_cls_s *p,
	     unsigned int size,
	     void **list);

/* only for n type have this function */
bool qbuf_n_is_in(struct buf_que_s *pqbuf,
		   unsigned int qt,
		   union q_buf_u q_buf);

/*************************************************/
#endif	/*__DI_QUE_H__*/
