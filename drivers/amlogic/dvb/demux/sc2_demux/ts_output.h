/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _TS_OUTPUT_H_
#define _TS_OUTPUT_H_

#include <linux/types.h>
#include "sc2_control.h"
#include <linux/dvb/dmx.h>

struct pts_dts {
	int pid;
	char pts_dts_flag;
	int es_bytes_from_beginning;
	u64 pts;
	u64 dts;
};

struct out_elem;

typedef int (*ts_output_cb) (struct out_elem *pout,
			     char *buf, int count, void *udata,
				 int req_len, int *req_ret);
enum content_type {
	NONE_TYPE,
	VIDEO_TYPE,
	AUDIO_TYPE,
	SUB_TYPE,
	TTX_TYPE,
	SEC_TYPE,
	OTHER_TYPE
};

#define PCR_TYPE	1
#define TEMI_TYPE	2
#define PCR_TEMI_TYPE	3

/**
 * ts_output_init
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_init(void);

/**
 * ts_output_destroy
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_destroy(void);

/**
 * remap pid
 * \param sid: stream id
 * \param pid: original pid
 * \param new_pid: replace pid
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_remap_pid(int sid, int pid, int new_pid);

/**
 * set pid pcr
 * \param sid: stream id
 * \param pcrpid
 * \param pcr_num
 * \retval 0:success.
 * \retval -1:fail.
 * \note:pcrpid == -1, it will close
 */
int ts_output_set_pcr(int sid, int pcrpid, int pcr_num);

/**
 * get pcr value
 * \param pcrpid
 * \param pcr_num
 * \param pcr:pcr value
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_get_pcr(int pcr_num, uint64_t *pcr);

struct out_elem *ts_output_find_same_section_pid(int sid, int pid);

/**
 * open one output pipeline
 * \param dmx_id:demux id.
 * \param sid:stream id.
 * \param format:output format.
 * \param type:input content type.
 * \param media_type:aucpu support format
 * \param output_mode:1 will output raw mode,just for ES.
 * \retval return out_elem.
 * \retval NULL:fail.
 */
struct out_elem *ts_output_open(int sid, u8 dmx_id, u8 format,
				enum content_type type, int media_type,
				int output_mode);

/**
 * close openned index
 * \param pout
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_close(struct out_elem *pout);

/**
 * add pid in stream
 * \param pout
 * \param pid:
 * \param pid_mask:0,matched all bits; 0x1FFF matched any PID
 * \param dmx_id: dmx_id
 * \param cb_id:same pid ref
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_add_pid(struct out_elem *pout, int pid,
		int pid_mask, int dmx_id, int *cb_id);

/**
 * remove pid in stream
 * \param pout
 * \param pid
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_remove_pid(struct out_elem *pout, int pid);

/**
 * set out elem mem
 * \param memsize
 * \param sec_level
 * \param pts_memsize
 * \param pts_level
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_set_mem(struct out_elem *pout, int memsize,
	int sec_level, int pts_memsize, int pts_level);

int ts_output_set_sec_mem(struct out_elem *pout,
	unsigned int buf, unsigned int size);

int ts_output_get_mem_info(struct out_elem *pout,
			   unsigned int *total_size,
			   unsigned int *buf_phy_start,
			   unsigned int *free_size, unsigned int *wp_offset,
			   __u64 *newest_pts);

/**
 * reset index pipeline, clear the buf
 * \param pout
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_reset(struct out_elem *pout);

/**
 * set callback for getting data
 * \param pout
 * \param cb
 * \param udata:private data
 * \param dmx_id:dmx_id
 * \param format:format
 * \param is_sec: is section callback
 * \param demux_id:dmx id
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_add_cb(struct out_elem *pout, ts_output_cb cb, void *udata,
		     u8 dmx_id, u8 format, bool is_sec, u8 demux_id);

/**
 * remove callback for getting data
 * \param pout
 * \param cb
 * \param udata:private data
 * \param cb_id:cb_id
 * \param is_sec: is section callback
 * \retval 0:success.
 * \retval -1:fail.
 */
int ts_output_remove_cb(struct out_elem *pout, ts_output_cb cb, void *udata,
			u8 cb_id, bool is_sec);

struct out_elem *ts_output_find_dvr(int sid);

int ts_output_sid_debug(void);
int ts_output_dump_info(char *buf);
int ts_output_update_filter(int dmx_no, int sid);
int ts_output_set_dump_timer(int flag);

int ts_output_set_decode_info(int sid, struct decoder_mem_info *info);
int ts_output_check_flow_control(int sid, int percentage);
int ts_output_add_temi_pid(struct out_elem *pout, int pid, int dmx_id,
						int *cb_id, int *index, int type);
int ts_output_add_remove_temi_pid(struct out_elem *pout, int index);
int ts_output_alloc_pcr_temi_entry(int pid, int sid, int type);
int ts_output_free_pcr_temi_entry(int index, int type);

void *ts_output_find_temi_pcr(int sid, int pid, int *pcr_index, int *temi_index);
int ts_output_get_wp(struct out_elem *pout, unsigned int *wp);
int ts_output_get_meminfo(struct out_elem *pout, unsigned int *size,
	unsigned long *mem, unsigned long *mem_phy);
int ts_output_dump_clone_info(char *buf);
#endif
