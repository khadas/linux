// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/dvb/ca.h>
#include <linux/module.h>
#include <linux/delay.h>

#include "sc2_control.h"
#include "dvb_reg.h"
#include "ts_output_reg.h"
#include "mem_desc_reg.h"
#include "demod_reg.h"
#include "dsc_reg.h"
#include "../dmx_log.h"

enum es_on_off {
	EST_OFF = 0,
	EST_SAVE_TS = 1,
	EST_SAVE_PES = 2,
	EST_SAVE_ES = 3
};

#define dprint_i(fmt, args...)   \
	dprintk(LOG_ERROR, debug_register, fmt, ## args)
#define pr_dbg(fmt, args...)     \
	dprintk(LOG_DBG, debug_register, "sc2 control:" fmt, ## args)

MODULE_PARM_DESC(debug_register,
		 "\n\t\t Enable sc2 register debug information");
static int debug_register;
module_param(debug_register, int, 0644);

unsigned int tsout_get_ready(void)
{
	return READ_CBUS_REG(PID_RDY);
}

void tsout_config_sid_table(u32 sid, u32 begin, u32 length)
{
	u32 sid_reg_idx = sid / 2;
	u32 sid_offset = sid % 2;
	u32 data = 0;
	u32 sid_addr;

	sid_addr = TS_OUT_SID_TAB_BASE + sid_reg_idx * 4;
	pr_dbg("%s read addr:0x%0x,sid:%d, pid begin:%d,len:%d\n",
	       __func__, sid_addr, sid, begin, length);

	data = READ_CBUS_REG(sid_addr);
	if (sid_offset == 0) {
		data = (data & ~0xFFFF)
		    | (begin & 0xFF)
		    | ((length & 0xFF) << 8);
	} else {
		data = (data & ~(0xFFFF << 16))
		    | ((begin & 0xFF) << 16)
		    | ((length & 0xFF) << 24);
	}
	pr_dbg("%s data:0x%0x\n", __func__, data);
	WRITE_CBUS_REG(sid_addr, data);
}

void tsout_config_ts_table(int pid, u32 pid_mask, u32 pid_entry, u32 buffer_id)
{
	union PID_CFG_FIELD cfg;
	union PID_DATA_FIELD data;

	pr_dbg("%s,pid:0x%0x, mask:0x%0x, pid_entry:0x%0x, buf_id:%d\n",
	       __func__, pid, pid_mask, pid_entry, buffer_id);
	cfg.data = 0;
	data.data = 0;
	if (pid == -1) {
		cfg.b.pid_entry = pid_entry;
		cfg.b.remap_flag = 0;
		cfg.b.buffer_id = buffer_id;
		cfg.b.on_off = 0;
		cfg.b.mode = MODE_WRITE;
		cfg.b.ap_pending = 1;
	} else {
		data.b.pid_mask = pid_mask;
		data.b.pid = pid & 0x1fff;

		cfg.b.pid_entry = pid_entry;
		cfg.b.remap_flag = 0;
		cfg.b.buffer_id = buffer_id;
		cfg.b.on_off = 1;
		cfg.b.mode = MODE_WRITE;
		cfg.b.ap_pending = 1;
	}
	WRITE_CBUS_REG(TS_OUTPUT_PID_DAT, data.data);
	WRITE_CBUS_REG(TS_OUTPUT_PID_CFG, cfg.data);

	pr_dbg("%s data.data:0x%0x\n", __func__, data.data);
	pr_dbg("%s cfg.data:0x%0x\n", __func__, cfg.data);

	do {
		cfg.data = READ_CBUS_REG(TS_OUTPUT_PID_CFG);
	} while (cfg.b.ap_pending);
}

void tsout_config_es_table(u32 es_entry, int pid,
			   u32 sid, u32 reset, u32 dup_ok, u8 fmt)
{
	union ES_TAB_FIELD data;

	data.data = 0;
	pr_dbg("%s es_entry:%d, pid:0x%0x, sid:0x%0x,",
	       __func__, es_entry, pid, sid);
	pr_dbg("reset:%d, dup_ok:%d, fmt:%d\n", reset, dup_ok, fmt);
	if (pid == -1) {
		data.b.on_off = EST_OFF;
	} else {
		data.b.pid = pid;
		data.b.sid = sid;
		data.b.reset = reset;
		data.b.dup_ok = dup_ok;
		if (fmt == ES_FORMAT)
			data.b.on_off = EST_SAVE_ES;
		else if (fmt == PES_FORMAT)
			data.b.on_off = EST_SAVE_PES;
		else
			data.b.on_off = EST_SAVE_TS;
	}
	pr_dbg("%s data.data:0x%0x\n", __func__, data.data);

	WRITE_CBUS_REG(TS_OUTPUT_ES_TAB(es_entry), data.data);
}

void tsout_config_pcr_table(u32 pcr_entry, u32 pcr_pid, u32 sid)
{
	union PCR_TAB_FIELD data;

	data.data = 0;

	if (pcr_pid != -1) {
		data.data = 0;
		data.b.pcr_pid = pcr_pid;
		data.b.sid = sid;
		data.b.valid = 1;
	}
	WRITE_CBUS_REG(TS_OUTPUT_PCR_TAB(pcr_entry), data.data);
	pr_dbg("%s data.data:0x%0x\n", __func__, data.data);
}

int tsout_config_get_pcr(u32 pcr_entry, u64 *pcr)
{
	union PCR_REG_LSB_FIELD lsb;
	union PCR_REG_MSB_FIELD msb;
	u64 data = 0;

	lsb.data = READ_CBUS_REG(TS_OUTPUT_PCR_REG_LSB(pcr_entry));
	msb.data = READ_CBUS_REG(TS_OUTPUT_PCR_REG_MSB(pcr_entry));

	data = msb.b.msb;
	data = data << 32;
	data |= lsb.data;

	*pcr = data;
	return 0;
}

unsigned int dsc_get_ready(int dsc_type)
{
	if (dsc_type == CA_DSC_COMMON_TYPE)
		return READ_CBUS_REG(TSN_PID_READY);
	else if (dsc_type == CA_DSC_TSD_TYPE)
		return READ_CBUS_REG(TSD_PID_READY);
	else
		return READ_CBUS_REG(TSE_PID_READY);
}

unsigned int dsc_get_status(int dsc_type)
{
	if (dsc_type == CA_DSC_COMMON_TYPE)
		return READ_CBUS_REG(TSN_PID_STATUS);
	else if (dsc_type == CA_DSC_TSD_TYPE)
		return READ_CBUS_REG(TSD_PID_STATUS);
	else
		return READ_CBUS_REG(TSE_PID_STATUS);
}

void dsc_config_ready(int dsc_type)
{
	if (dsc_type == CA_DSC_COMMON_TYPE)
		WRITE_CBUS_REG(TSN_PID_READY, 1);
	else if (dsc_type == CA_DSC_TSD_TYPE)
		WRITE_CBUS_REG(TSD_PID_READY, 1);
	else
		WRITE_CBUS_REG(TSE_PID_READY, 1);
}

//unsigned int dsc_get_pid_status(int dsc_type)
//{
//}

void dsc_config_pid_table(struct dsc_pid_table *pid_entry, int dsc_type)
{
	unsigned int lo_addr = 0;
	unsigned int hi_addr = 0;
	unsigned int lo_value = 0;
	unsigned int hi_value = 0;

	pr_dbg("%s dsc_type:%d, pid_entry:%d, sid:%d\n",
	       __func__, dsc_type, pid_entry->id, pid_entry->sid);
	dsc_get_ready(dsc_type);
	if (dsc_type == CA_DSC_COMMON_TYPE) {
		lo_addr = TSN_BASE_ADDR + pid_entry->id * 8;
		hi_addr = TSN_BASE_ADDR + pid_entry->id * 8 + 4;
	} else if (dsc_type == CA_DSC_TSD_TYPE) {
		lo_addr = TSD_BASE_ADDR + pid_entry->id * 8;
		hi_addr = TSD_BASE_ADDR + pid_entry->id * 8 + 4;
	} else {
		lo_addr = TSE_BASE_ADDR + pid_entry->id * 8;
		hi_addr = TSE_BASE_ADDR + pid_entry->id * 8 + 4;
	}

	lo_value = (pid_entry->kte_even_00 << LO_KTE_EVEN_00) |
	    (pid_entry->kte_odd << LO_KTE_ODD) |
	    (pid_entry->algo << LO_ALGO) |
	    ((pid_entry->pid & 0xFFF) << LO_PID_PART1);
	hi_value = ((pid_entry->pid >> 12) & 0x1) << HI_PID_PART2 |
	    (pid_entry->sid << HI_SID) |
	    (pid_entry->even_00_iv << HI_EVEN_00_IV) |
	    (pid_entry->odd_iv << HI_ODD_IV) |
	    (pid_entry->scb_as_is << HI_SCB_AS_IS) |
	    (pid_entry->scb_out << HI_SCB_OUT) |
	    (pid_entry->scb00 << HI_SCB00) | (pid_entry->valid << HI_VALID);
	WRITE_CBUS_REG(lo_addr, lo_value);
	WRITE_CBUS_REG(hi_addr, hi_value);

	pr_dbg("%s lo_value:0x%0x\n", __func__, lo_value);
	pr_dbg("%s hi_value:0x%0x\n", __func__, hi_value);
	dsc_config_ready(dsc_type);
}

void rdma_config_enable(u8 chan_id, int enable,
			unsigned int desc, unsigned int total_size,
			unsigned int len)
{
	u32 data = 0;

	if (enable) {
		WRITE_CBUS_REG(TS_DMA_RCH_ADDR(chan_id), desc);
		WRITE_CBUS_REG(TS_DMA_RCH_LEN(chan_id), len);
		pr_dbg("%s desc:0x%0x\n", __func__, desc);
		pr_dbg("%s total_size:0x%0x\n", __func__, len);

		data = 188 << RCH_CFG_READ_LEN;
		data |= 188 << RCH_CFG_PACKET_LEN;
		data |= 1 << RCH_CFG_ENABLE;
		WRITE_CBUS_REG(TS_DMA_RCH_EACH_CFG(chan_id), data);
		pr_dbg("%s addr:0x%0x data:0x%0x\n", __func__,
		       TS_DMA_RCH_EACH_CFG(chan_id), data);
	} else {
		data = READ_CBUS_REG(TS_DMA_RCH_EACH_CFG(chan_id));

		data &= ~(1 << RCH_CFG_ENABLE);
		WRITE_CBUS_REG(TS_DMA_RCH_EACH_CFG(chan_id), data);
		pr_dbg("%s data:0x%0x\n", __func__, data);
	}
}

unsigned int rdma_get_status(u8 chan_id)
{
	return READ_CBUS_REG(TS_DMA_RCH_STATUS(chan_id));
}

unsigned int rdma_get_len(u8 chan_id)
{
	return READ_CBUS_REG(TS_DMA_RCH_LEN(chan_id));
}

unsigned int rdma_get_rd_len(u8 chan_id)
{
	return READ_CBUS_REG(TS_DMA_RCH_RD_LEN(chan_id));
}

unsigned int rdma_get_ptr(u8 chan_id)
{
	return READ_CBUS_REG(TS_DMA_RCH_PTR(chan_id));
}

unsigned int rdma_get_pkt_sync_status(u8 chan_id)
{
	return READ_CBUS_REG(TS_DMA_RCH_PKT_SYNC_STATUS(chan_id));
}

unsigned int rdma_get_ready(u8 chan_id)
{
	return READ_CBUS_REG(TS_DMA_RCH_READY(chan_id));
}

unsigned int rdma_get_err(void)
{
	return READ_CBUS_REG(TS_DMA_RCH_RDES_ERR);
}

unsigned int rdma_get_len_err(void)
{
	return READ_CBUS_REG(TS_DMA_RCH_RDES_LEN_ERR);
}

unsigned int rdma_get_active(void)
{
	return READ_CBUS_REG(TS_DMA_RCH_ACTIVE);
}

void rdma_config_ready(u8 chan_id)
{
	WRITE_CBUS_REG(TS_DMA_RCH_READY(chan_id), 1);
}

unsigned int rdma_get_done(u8 chan_id)
{
	return (READ_CBUS_REG(TS_DMA_RCH_DONE) & (0x1 << chan_id));
}

unsigned int rdma_get_cfg_fifo(void)
{
	return READ_CBUS_REG(TS_DMA_RCH_CFG);
}

void rdma_config_sync_num(unsigned int sync_num)
{
	unsigned int data = READ_CBUS_REG(TS_DMA_RCH_CFG);

	pr_dbg("%s sync_num:%d\n", __func__, sync_num);
	data &= ~(0xF << 8);
	data |= ((sync_num & 0xF) << 8);
	WRITE_CBUS_REG(TS_DMA_RCH_CFG, data);
}

void rdma_clean(u8 chan_id)
{
	unsigned int data;

	data = READ_CBUS_REG(TS_DMA_RCH_CLEAN);
	data |= (1 << chan_id);

	WRITE_CBUS_REG(TS_DMA_RCH_CLEAN, data);

	pr_dbg("%s data:0x%0x\n", __func__, data);
}

void rdma_config_cfg_fifo(unsigned int value)
{
	unsigned int data = READ_CBUS_REG(TS_DMA_RCH_CFG);

	data &= ~(0x1F);
	data |= (value & 0x1F);
	WRITE_CBUS_REG(TS_DMA_RCH_CFG, value);
}

void wdma_clean(u8 chan_id)
{
	unsigned int data;

	unsigned int offset = chan_id / 32 * 4;

	data = READ_CBUS_REG(TS_DAM_WCH_CLEAN + offset);

	data |= (1 << (chan_id % 32));
	WRITE_CBUS_REG(TS_DAM_WCH_CLEAN + offset, data);
	pr_dbg("%s data:0x%0x\n", __func__, data);
}

void wdma_clean_batch(u8 chan_id)
{
	unsigned int data;

	unsigned int offset = chan_id / 32 * 4;

	data = READ_CBUS_REG(TS_DMA_WCH_CLEAN_BATCH + offset);

	data |= (1 << (chan_id % 32));
	WRITE_CBUS_REG(TS_DAM_WCH_CLEAN + offset, data);
	pr_dbg("%s data:0x%0x\n", __func__, data);
}

void wdma_irq(u8 chan_id, int enable)
{
	unsigned int data;
	unsigned int offset = chan_id / 32 * 4;

	data = READ_CBUS_REG(TS_DMA_WCH_INT_MASK + offset);

	if (enable)
		data |= (1 << (chan_id % 32));
	else
		data &= (~(1 << (chan_id % 32)));

	WRITE_CBUS_REG(TS_DAM_WCH_CLEAN + offset, data);
	pr_dbg("%s data:0x%0x\n", __func__, data);
}

unsigned int wdma_get_ready(u8 chan_id)
{
	return READ_CBUS_REG(TS_DMA_WCH_READY(chan_id));
}

void wdam_config_ready(u8 chan_id)
{
	WRITE_CBUS_REG(TS_DMA_WCH_READY(chan_id), 1);
}

void wdma_config_enable(u8 chan_id, int enable,
			unsigned int desc, unsigned int total_size)
{
	int times = 0;
	unsigned int cnt = 0;

	if (enable) {
		do {
		} while (!wdma_get_ready(chan_id) && times++ < 20);

		wdma_clean(chan_id);

		WRITE_CBUS_REG(TS_DMA_WCH_ADDR(chan_id), desc);
		WRITE_CBUS_REG(TS_DMA_WCH_LEN(chan_id), total_size);

		pr_dbg("%s desc:0x%0x\n", __func__, desc);
		pr_dbg("%s total_size:0x%0x\n", __func__, total_size);
	} else {
//              unsigned int data;

//              data = READ_CBUS_REG(TS_DMA_WCH_CFG(chan_id));
//              data |= (1 << WCH_CFG_CLEAR);
//              WRITE_CBUS_REG(TS_DMA_WCH_CFG(chan_id), data);
		WRITE_CBUS_REG(TS_DMA_WCH_ADDR(chan_id), 0);
		WRITE_CBUS_REG(TS_DMA_WCH_LEN(chan_id), 0);

		/*if dmx have cmd completed, need delay,
		 * or clean will cause
		 * demod enter overflow status,
		 * it can't resolve except reboot
		 */
		cnt = wdma_get_wcmdcnt(chan_id);
		if (cnt)
			msleep(20);
		wdma_clean(chan_id);
		//delay
//              while (times ++ < 500);
		pr_dbg("%s wptr:0x%0x\n", __func__,
		       READ_CBUS_REG(TS_DMA_WCH_WR_LEN(chan_id)));
		wdam_config_ready(chan_id);
	}
}

unsigned int wdma_get_wptr(u8 chan_id)
{
	return READ_CBUS_REG(TS_DMA_WCH_PTR(chan_id));
}

unsigned int wdma_get_wcmdcnt(u8 chan_id)
{
	return READ_CBUS_REG(TS_DMA_WCH_CMD_CNT(chan_id));
}

unsigned int wdma_get_batch_end(u8 chan_id)
{
	unsigned int addr = TS_DMA_WCH_BATCH_END + chan_id / 32 * 4;
	unsigned int bit = chan_id % 32;
	unsigned int data = 0;

	data = READ_CBUS_REG(addr);

	return (data & (0x1 << bit));
}

unsigned int wdma_get_active(u8 chan_id)
{
	unsigned int addr = TS_DMA_WCH_ACTIVE + chan_id / 32 * 4;
	unsigned int bit = chan_id % 32;
	unsigned int data = 0;

	data = READ_CBUS_REG(addr);

	return (data & (0x1 << bit));
}

unsigned int wdma_get_wr_len(u8 chan_id, int *overflow)
{
	unsigned int data;

	if (wdma_get_active(chan_id)) {
		data = READ_CBUS_REG(TS_DMA_WCH_WR_LEN(chan_id));
		if (data == 0)
			data = READ_CBUS_REG(TS_DMA_WCH_LEN(chan_id));

		if (wdma_get_batch_end(chan_id) && overflow)
			*overflow = 1;
		return data;
	} else {
		return 0;
	}
}

unsigned int wdma_get_done(u8 chan_id)
{
	unsigned int addr = TS_DMA_WCH_DONE + chan_id / 32 * 4;
	unsigned int bit = chan_id % 32;
	unsigned int data = 0;

	data = READ_CBUS_REG(addr);

	return (data & (0x1 << bit));
}

unsigned int wdma_get_err(u8 chan_id)
{
	unsigned int addr = TS_DMA_WCH_ERR + chan_id / 32 * 4;
	unsigned int bit = chan_id % 32;
	unsigned int data = 0;

	data = READ_CBUS_REG(addr);

	return (data & (0x1 << bit));
}

unsigned int wdma_get_eoc_done(u8 chan_id)
{
	unsigned int addr = TS_DMA_WCH_EOC_DONE + chan_id / 32 * 4;
	unsigned int bit = chan_id % 32;
	unsigned int data = 0;

	data = READ_CBUS_REG(addr);

	return (data & (0x1 << bit));
}

unsigned int wdma_get_resp_err(u8 chan_id)
{
	unsigned int addr = TS_DMA_WCH_RESP_ERR + chan_id / 32 * 4;
	unsigned int bit = chan_id % 32;
	unsigned int data = 0;

	data = READ_CBUS_REG(addr);

	return (data & (0x1 << bit));
}

unsigned int wdma_get_cfg_fast_mode(void)
{
	return READ_CBUS_REG(TS_DMA_WCH_CFG_FAST_MODE);
}

/*demod part*/
void demod_config_single(u8 port, u32 sid)
{
	unsigned int data = 0;

	data = sid << SIGNAL_SID;
	WRITE_CBUS_REG(DEMOD_PKT_CFG(port), data);
	pr_dbg("%s data:0x%0x\n", __func__, data);
}

void demod_config_multi(u8 port, u8 header_len,
			u32 custom_header, u32 sid_offset)
{
	unsigned int data = 0;

	data = (header_len << HEADER_LEN) |
	    (custom_header << CUSTOM_HEADER) | (sid_offset << SID_OFFSET);
	WRITE_CBUS_REG(DEMOD_PKT_CFG(port), data);

	pr_dbg("%s data:0x%0x\n", __func__, data);
}

void demod_config_tsin_invert(u8 port, u8 invert)
{
	unsigned int data = 0;

	data = READ_CBUS_REG(DEMOD_PATH_CTRL(port));

	data &= ~(0x1F << SERIAL_CONTROL);
	data |= (invert & 0x1F) << SERIAL_CONTROL;

	WRITE_CBUS_REG(DEMOD_PATH_CTRL(port), data);
	pr_dbg("%s data:0x%0x\n", __func__, data);
}

void demod_config_in(u8 port, u8 wire_type)
{
	unsigned int data = 0;

#define DEMOD_1_SERIAL      1
#define DEMOD_1_PARALLEL    0

	data = READ_CBUS_REG(DEMOD_PATH_CTRL(port));

	data &= ~(0x1 << FEC_S2P_SEL);
	data &= ~(0x1 << FEC_S2P_3WIRE);
	data &= ~(0x1 << TS_S_OR_P_SEL1);

	dprint_i("port:%d, wire_type:%d\n", port, wire_type);

	if (wire_type == DEMOD_3WIRE) {
		data |= 1 << FEC_S2P_SEL;
		data |= (0x1 << FEC_S2P_3WIRE);
		if (port == DEMOD_FEC_B)
			data |= DEMOD_1_SERIAL << TS_S_OR_P_SEL1;
		else if (port == DEMOD_FEC_A)
			data |= DEMOD_1_SERIAL << TS_S_OR_P_SEL1;

	} else if (wire_type == DEMOD_4WIRE) {
		data |= 1 << FEC_S2P_SEL;
		data |= (0 << FEC_S2P_3WIRE);
		if (port == DEMOD_FEC_B)
			data |= DEMOD_1_SERIAL << TS_S_OR_P_SEL1;
		else if (port == DEMOD_FEC_A)
			data |= DEMOD_1_SERIAL << TS_S_OR_P_SEL1;
	} else {
		if (port == DEMOD_FEC_B)
			data |= DEMOD_1_PARALLEL << TS_S_OR_P_SEL1;
		else
			return;
	}
	WRITE_CBUS_REG(DEMOD_PATH_CTRL(port), data);
	pr_dbg("%s data:0x%0x\n", __func__, data);
}

void demux_config_pipeline(int tsn_in, int tsn_out)
{
#define CFG_DEMUX_ADDR    (SECURE_BASE + 0x320)
	unsigned int data = 0;

	data = tsn_in & 0x1;
	data |= ((tsn_out & 0x1) << 1);
	WRITE_CBUS_REG(CFG_DEMUX_ADDR, data);
}

void sc2_dump_register(void)
{
	int i = 0;
	unsigned int data = 0;
	unsigned int lo_addr = 0;
	unsigned int hi_addr = 0;
	unsigned int lo_data = 0;
	unsigned int hi_data = 0;
	union PID_CFG_FIELD ts_cfg;
	union PID_DATA_FIELD ts_data;

	dprint_i("**************demod register************\n");
	for (i = 0; i < 4; i++) {
		data = READ_CBUS_REG(DEMOD_PATH_CTRL(i));
		dprint_i("path%d_ctrl:0x%0x ", i, data);

		data = READ_CBUS_REG(DEMOD_PKT_CFG(i));
		dprint_i("pkt_cfg%d:0x%0x\n", i, data);
	}

	data = READ_CBUS_REG(DEMOD_FIFO_CFG(0));
	dprint_i("demod_fifo_cfg0:0x%0x\n", data);

	data = READ_CBUS_REG(DEMOD_FIFO_CFG(2));
	dprint_i("demod_fifo_cfg1:0x%0x\n", data);

	data = READ_CBUS_REG(DEMOD_CLEAN_DEMOD_INT);
	dprint_i("clean_demod_int: 0x%0x\n", data);

	data = READ_CBUS_REG(DEMOD_INT_MASK);
	dprint_i("demod_int_mask: 0x%0x\n", data);

	data = READ_CBUS_REG(DEMOD_INT_STATUS);
	dprint_i("demod_int_status: 0x%0x\n", data);

	for (i = 0; i < 4; i++) {
		data = READ_CBUS_REG(DEMOD_TS_CH_ERR_STATUS(i));
		dprint_i("ts_chn%d_err_status: 0x%0x\n", i, data);
	}

	dprint_i("**************dsc tsn************\n");
	for (i = 0; i < 64; i++) {
		lo_addr = TSN_BASE_ADDR + i * 8;
		hi_addr = TSN_BASE_ADDR + i * 8 + 4;
		lo_data = READ_CBUS_REG(lo_addr);
		hi_data = READ_CBUS_REG(lo_addr);
		if (hi_data & (0x1 << HI_VALID))
			dprint_i("pid table:%d, 0x%0x %0x\n",
				 i, lo_data, hi_data);
	}
	dprint_i("**************dsc tsd************\n");
	for (i = 0; i < 64; i++) {
		lo_addr = TSD_BASE_ADDR + i * 8;
		hi_addr = TSD_BASE_ADDR + i * 8 + 4;
		lo_data = READ_CBUS_REG(lo_addr);
		hi_data = READ_CBUS_REG(lo_addr);
		if (hi_data & (0x1 << HI_VALID))
			dprint_i("pid table:%d, 0x%0x %0x\n",
				 i, lo_data, hi_data);
	}
	dprint_i("**************dsc tse************\n");
	for (i = 0; i < 64; i++) {
		lo_addr = TSE_BASE_ADDR + i * 8;
		hi_addr = TSE_BASE_ADDR + i * 8 + 4;
		lo_data = READ_CBUS_REG(lo_addr);
		hi_data = READ_CBUS_REG(lo_addr);
		if (hi_data & (0x1 << HI_VALID))
			dprint_i("pid table:%d, 0x%0x %0x\n",
				 i, lo_data, hi_data);
	}

	dprint_i("**************output************\n");

	for (i = 0; i < 1024; i++) {
		ts_cfg.b.pid_entry = i;
		ts_cfg.b.remap_flag = 0;
		ts_cfg.b.on_off = 0;
		ts_cfg.b.buffer_id = 0;
		ts_cfg.b.mode = MODE_READ;
		ts_cfg.b.ap_pending = 1;
		WRITE_CBUS_REG(TS_OUTPUT_PID_CFG, ts_cfg.data);
		do {
			ts_cfg.data = READ_CBUS_REG(TS_OUTPUT_PID_CFG);
		} while (ts_cfg.b.ap_pending);

		ts_data.data = READ_CBUS_REG(TS_OUTPUT_PID_DAT);
		if (ts_cfg.b.on_off)
			dprint_i("pid entry:%d, pid:0x%0x, mask:0x%0x\n",
				 i, ts_data.b.pid, ts_data.b.pid_mask);
	}

	dprint_i("**************rdma************\n");
	for (i = 0; i < 32; i++) {
		data = READ_CBUS_REG(TS_DMA_RCH_EACH_CFG(i));
		if (data & (0x1 << RCH_CFG_ENABLE))
			dprint_i("id:%d rch cfg: 0x%0x", i, data);
		else
			continue;

		data = READ_CBUS_REG(TS_DMA_RCH_ADDR(i));
		dprint_i("rch_adr:0x%0x ", data);

		data = READ_CBUS_REG(TS_DMA_RCH_LEN(i));
		dprint_i("rch_len:0x%0x ", data);

		data = READ_CBUS_REG(TS_DMA_RCH_RD_LEN(i));
		dprint_i("rch_rd_len:0x%0x ", data);

		data = READ_CBUS_REG(TS_DMA_RCH_PTR(i));
		dprint_i("rch_ptr:0x%0x ", data);
	}

	dprint_i("**************wdma************\n");
}
