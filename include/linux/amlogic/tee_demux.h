/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __TEE_DEMUX_H__
#define __TEE_DEMUX_H__

#define TEE_DMX_ENABLE 1

enum tee_dmx_cmd {
	TEE_DMX_SET_SID_TABLE,
	TEE_DMX_SET_TS_TABLE,
	TEE_DMX_SET_ES_TABLE,
	TEE_DMX_SET_PCR_TABLE,
	TEE_DMX_SET_PID_TABLE,
	TEE_DMX_SET_DMA_DESC,
	TEE_DMX_GET_SECURITY_ENABLE = 0x1000,
	TEE_DMX_GET_PCR,
	TEE_DMX_GET_TSX_STATUS,
};

struct tee_dmx_ts_table_param {
	int pid;
	u32 pid_mask;
	u32 pid_entry;
	u32 buffer_id;
};

struct tee_dmx_sid_table_param {
	u32 sid;
	u32 begin;
	u32 length;
};

union tee_dsc_pid_table {
	u64 data;
	struct {
		u64 valid:1;
		u64 resv0:7;
		u64 resv1:1;
		u64 scb00:1;
		u64 scb_out:2;
		u64 scb_as_is:1;
		u64 odd_iv:6;
		u64 even_00_iv:6;
		u64 sid:6;
		u64 pid:13;
		u64 algo:4;
		u64 kte_odd:8;
		u64 kte_even_00:8;
	} bits;
};

struct tee_dmx_pid_table_param {
	u32 type;
	u32 id;
	union tee_dsc_pid_table table;
};

struct tee_dmx_es_table_param {
	u32 es_entry;
	int pid;
	u32 sid;
	u32 reset;
	u32 dup_ok;
	u8 fmt;
};

struct tee_dmx_pcr_table_param {
	u32 pcr_entry;
	u32 pcr_pid;
	u32 sid;
};

struct tee_dmx_dma_desc_param {
	u32 len;
	u32 address;
	u32 buffer_id;
};

int tee_demux_set(enum tee_dmx_cmd cmd, void *data, u32 len);

int tee_demux_get(enum tee_dmx_cmd cmd,
		void *in, u32 in_len, void *out, u32 out_len);

#endif /* __TEE_DEMUX_H__ */

