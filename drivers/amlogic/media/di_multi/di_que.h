/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#endif	/*__DI_QUE_H__*/
