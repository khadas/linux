/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _SC2_CONTROL_H_
#define _SC2_CONTROL_H_
#include "mem_desc.h"

struct dsc_pid_table {
	int id;
	int used;
	int valid;
	char scb00;
	char scb_user;
	char scb_out;
	/*if 1, keep out scb same,
	 * if 0, out scb will scb_out
	 */
	char scb_as_is;
	char odd_iv;
	char even_00_iv;
	char sid;
	int pid;
	char algo;
	char kte_odd;
	char kte_even_00;
};

enum output_format {
	TS_FORMAT,
	ES_FORMAT,
	PES_FORMAT,
	SECTION_FORMAT,
	DVR_FORMAT,
	TEMI_FORMAT,
	CLONE_FORMAT,
};

enum pid_cfg_mode {
	MODE_READ,
	MODE_WRITE,
	MODE_TURN_OFF,
	MODE_TURN_ON
};

enum FEC_PORT_E {
	DEMOD_FEC_A = 0,
	DEMOD_FEC_B = 1,
	DEMOD_FEC_C = 2,
	DEMOD_FEC_D = 3
};

enum DEMOD_HEADER_LEN_E {
	DEMOD_NO_HEADER = 0,
	DEMOD_HEADER_4B = 1,
	DEMOD_HEADER_8B = 2,
	DEMOD_HEADER_12B = 3
};

enum DEMOD_WIRE_TYPE_E {
	DEMOD_3WIRE = 0,
	DEMOD_4WIRE = 1,
	DEMOD_PARALLEL = 2
};

enum DEMOD_TSIN_SEL_E {
	TSIN_0 = 0,
	TSIN_1 = 1
};

/*****************************************************/
/*****************************************************/
/*****************************************************/
unsigned int tsout_get_ready(void);
void tsout_config_sid_table(u32 sid, u32 begin, u32 length);
void tsout_config_ts_table(int pid, u32 pid_mask, u32 pid_entry,
			u32 buffer_id, int sid, int sec_level);
void tsout_config_es_table(u32 es_entry, int pid,
			   u32 sid, u32 reset, u32 dup_ok, u8 fmt);
void tsout_config_pcr_table(u32 pcr_entry, u32 pcr_pid, u32 sid);
void tsout_config_temi_table(u32 temi_entry, u32 pcr_pid, u32 sid, u32 buffer_id, u32 status);
int tsout_config_get_pcr(u32 pcr_entry, u64 *pcr);
void tsout_config_remap_table(u32 pid_entry, u32 sid, int pid, int pid_new);

/*****************************************************/
/*****************************************************/
/*****************************************************/
unsigned int dsc_get_status(int dsc_type);
unsigned int dsc_get_ready(int dsc_type);
void dsc_config_ready(int dsc_type);
void dsc_config_pid_table(struct dsc_pid_table *pid_entry, int dsc_type);

/*****************************************************/
/*****************************************************/
/*****************************************************/
//void rdma_config_enable(u8 chan_id, int enable,
void rdma_config_enable(struct chan_id *pchan, int enable,
			unsigned int desc, unsigned int total_size,
			unsigned int len, unsigned int pack_len);
void rdma_config_ready(u8 chan_id);
void rdma_clean(u8 chan_id);
void rdma_config_cfg_fifo(unsigned int value);
void rdma_config_sync_num(unsigned int sync_num);

unsigned int rdma_get_status(u8 chan_id);
unsigned int rdma_get_len(u8 chan_id);
unsigned int rdma_get_rd_len(u8 chan_id);
unsigned int rdma_get_ptr(u8 chan_id);
unsigned int rdma_get_pkt_sync_status(u8 chan_id);
unsigned int rdma_get_ready(u8 chan_id);
unsigned int rdma_get_err(void);
unsigned int rdma_get_len_err(void);
unsigned int rdma_get_active(void);
unsigned int rdma_get_done(u8 chan_id);
unsigned int rdma_get_cfg_fifo(void);

/*****************************************************/
/*****************************************************/
/*****************************************************/
void wdma_clean(u8 chan_id);
//void wdma_config_enable(u8 chan_id, int enable,
void wdma_config_enable(struct chan_id *pchan, int enable,
			unsigned int desc, unsigned int total_size,
			int sid, int pid, int sec_level);

void wdam_config_ready(u8 chan_id);
unsigned int wdma_get_wptr(u8 chan_id);
unsigned int wdma_get_wcmdcnt(u8 chan_id);
unsigned int wdma_get_active(u8 chan_id);
unsigned int wdma_get_done(u8 chan_id);
unsigned int wdma_get_err(u8 chan_id);
unsigned int wdma_get_eoc_done(u8 chan_id);
unsigned int wdma_get_resp_err(u8 chan_id);
unsigned int wdma_get_cfg_fast_mode(void);
void wdma_clean_batch(u8 chan_id);
void wdma_irq(u8 chan_id, int enable);
unsigned int wdma_get_wr_len(u8 chan_id, int *overflow);
void tso_set(int path);
/*****************************************************/
/*****************************************************/
/*****************************************************/
void demod_config_single(u8 port, u32 sid);
void demod_config_multi(u8 port,
			u8 header_len, u32 custom_header, u32 sid_offset);
/*bit4: set 1 to invert input error signal*/
/*Bit3: set 1 to invert input data signal*/
/*bit2: set 1 to invert input sync signal*/
/*bit1: set 1 to invert input valid signal*/
/*bit0: set 1 to invert input clk signal*/
void demod_config_tsin_invert(u8 port, u8 invert);
void demod_config_in(u8 port, u8 wire_type);
void demod_config_fifo(u8 port, u16 fifo_th);
//for s4d clear clk mux, using tsinA clk
void demod_config_tsind_clk(u8 b);

/*****************************************************/
/*****************************************************/
/*****************************************************/
void sc2_dump_register(void);
#endif
