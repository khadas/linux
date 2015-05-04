/*
 * drivers/amlogic/display/vout/lcd/edp_drv.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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


#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/amlogic/vout/lcdoutc.h>
#include "lcd_reg.h"
#include "edp_tx_reg.h"
#include "lcd_control.h"
#include "edp_drv.h"

/* *************************************
 * dptx for operation eDP Host (Tx)
 * trdp for operation eDP Sink (Rx)
 * dplpm for eDP link policy maker
 * ************************************* */

enum EDP_DPCD_Strings_s {
	dpcd_strings_0 = 0,
	dpcd_strings_1,
	dpcd_strings_2,
	dpcd_strings_3,
	dpcd_strings_4,
	dpcd_strings_5,
	dpcd_strings_6,
	dpcd_strings_7,
	dpcd_strings_8,
	dpcd_strings_9,
	dpcd_strings_10,
	dpcd_strings_11,
	dpcd_strings_12,
	dpcd_strings_13,
	dpcd_strings_14,
	dpcd_strings_15,
};

/* Strings used by printout functions */
static char *dpcd_strings[] = {
	"Unknown",         /* 0 */
	"Invalid",         /* 1 */
	"None",            /* 2 */
	"Yes",             /* 3 */
	"No",              /* 4 */
	"1.0",             /* 5 */
	"1.1",             /* 6 */
	"1.62 Gbps",       /* 7 */
	"2.7 Gbps",        /* 8 */
	"Displayport",     /* 9 */
	"Analog VGA",      /* 10 */
	"DVI",             /* 11 */
	"HDMI",            /* 12 */
	"Other (No EDID)", /* 13 */
	"1.2",             /* 14 */
	"5.4 Gbps"         /* 15 */
};

const char *edp_link_rate_string_table[] = {
	"1.62Gbps",
	"2.70Gbps",
	"5.40Gbps",
	"invalid",
};

static struct EDP_Link_Config_s link_conf = {
	.max_lane_count = 4,
	.max_link_rate = VAL_EDP_TX_LINK_BW_SET_270,
	.enhanced_framing_en = 0,
	.lane_count = 4,
	.link_rate = VAL_EDP_TX_LINK_BW_SET_270,
	.vswing = VAL_EDP_TX_PHY_VSWING_0,
	.preemphasis = VAL_EDP_TX_PHY_PREEMPHASIS_0,
	.ss_enable = 0,
	.link_update = 0,
	.training_settings = 0,
	.main_stream_enable = 0,
	.use_dpcd_caps = 0,
	.link_adaptive = 0,
	.link_rate_adjust_en = 1, /* enable adjust link rate */
	.bit_rate = 0,
};

static unsigned char preset_vswing_tx, preset_vswing_rx;
static unsigned char preset_preemp_tx, preset_preemp_rx;
static unsigned char adj_req_lane01, adj_req_lane23;

static inline void trdp_wait(unsigned n)
{
	mdelay(n);
}

static struct EDP_Link_Config_s *dptx_get_link_config(void)
{
	return &link_conf;
}

static unsigned int dptx_wait_phy_ready(void)
{
	unsigned int reg_val = 0;
	unsigned int done = 100;

	do {
		reg_val = dptx_reg_read(EDP_TX_PHY_STATUS);
		if (done < 20) {
			DPRINT("dptx wait phy ready: reg_val=0x%x, ", reg_val);
			DPRINT("wait_count=%u\n", (100-done));
		}
		done--;
		udelay(100);
	} while (((reg_val & 0x7f) != 0x7f) && (done > 0));

	if ((reg_val & 0x7f) == 0x7f)
		return RET_EDP_TX_OPERATION_SUCCESS;
	else
		return RET_EDP_TX_OPERATION_FAILED;
}

static void dptx_dump_link_config(void)
{
	struct EDP_Link_Config_s *link_config = dptx_get_link_config();

	DPRINT("********************************************\n");
	DPRINT(" Link Config:\n"
	   "    Link Rate               : 0x%02x\n"
	   "    Lane Count              : %u\n"
	   "    Vswing                  : 0x%02x\n"
	   "    Preemphasis             : 0x%02x\n"
	   "    Spread Spectrum level   : %u\n"
	   "    Use DPCD Caps           : %u\n"
	   "    Training Settings       : %u\n"
	   "    Link Rate Adjust        : %u\n"
	   "    Link Adaptive           : %u\n"
	   "    Main Stream Enable      : %u\n",
	   dptx_reg_read(EDP_TX_LINK_BW_SET),
	   dptx_reg_read(EDP_TX_LINK_COUNT_SET),
	   dptx_reg_read(EDP_TX_PHY_VOLTAGE_DIFF_LANE_0),
	   dptx_reg_read(EDP_TX_PHY_PRE_EMPHASIS_LANE_0),
	   dptx_reg_read(EDP_TX_DOWNSPREAD_CTRL),
	   link_config->use_dpcd_caps,
	   link_config->training_settings,
	   link_config->link_rate_adjust_en,
	   link_config->link_adaptive,
	   dptx_reg_read(EDP_TX_MAIN_STREAM_ENABLE));
	DPRINT("********************************************\n");
}

static void dptx_dump_MSA(void)
{
	DPRINT("********************************************\n");
	DPRINT(" Main Stream Attributes TX\n"
	   "    Clocks, H Total         : %u\n"
	   "    Clocks, V Total         : %u\n"
	   "    Polarity (V / H)        : %u\n"
	   "    HSync Width             : %u\n"
	   "    VSync Width             : %u\n"
	   "    Horz Resolution         : %u\n"
	   "    Vert Resolution         : %u\n"
	   "    Horz Start              : %u\n"
	   "    Vert Start              : %u\n"
	   "    Misc0                   : 0x%08x\n"
	   "    Misc1                   : 0x%08x\n"
	   "    User Pixel Width        : %u\n"
	   "    M Vid                   : %u\n"
	   "    N Vid                   : %u\n"
	   "    Transfer Unit Size      : %u\n"
	   "    User Data Count         : %u\n",
	   dptx_reg_read(EDP_TX_MAIN_STREAM_HTOTAL),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_VTOTAL),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_POLARITY),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_HSWIDTH),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_VSWIDTH),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_HRES),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_VRES),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_HSTART),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_VSTART),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_MISC0),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_MISC1),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_USER_PIXEL_WIDTH),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_M_VID),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_N_VID),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_TRANSFER_UNIT_SIZE),
	   dptx_reg_read(EDP_TX_MAIN_STREAM_DATA_COUNT_PER_LANE));
	DPRINT("********************************************\n");
}
#if 0
static unsigned int dptx_set_link_rate(unsigned char link_rate)
{
	unsigned int status = 0;

	switch (link_rate) {
	case VAL_EDP_TX_LINK_BW_SET_162:
	case VAL_EDP_TX_LINK_BW_SET_270:
		status = RET_EDP_TX_OPERATION_SUCCESS;
		break;
	default:
		status = RET_EDP_TX_AUX_INVALID_PARAMETER;
		break;
	}

	if (status == RET_EDP_TX_OPERATION_SUCCESS) {
		/* disable the transmitter */
		dptx_reg_write(EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 0);
		dptx_reg_write(EDP_TX_PHY_RESET, 0xf); /* reset the PHY */
		mdelay(10);

		dptx_reg_write(EDP_TX_LINK_BW_SET, link_rate);

		dptx_reg_write(EDP_TX_PHY_RESET, 0);
		status = dptx_wait_phy_ready();
		mdelay(10);
		dptx_reg_write(EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 1);
	}

	return status;
}

static unsigned int dptx_set_lane_count(unsigned char lane_count)
{
	unsigned int status = 0;

	/* disable the transmitter */
	dptx_reg_write(EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 0);
	dptx_reg_write(EDP_TX_PHY_RESET, 0xf); /* reset the PHY */
	mdelay(10);

	dptx_reg_write(EDP_TX_LINK_COUNT_SET, lane_count);

	dptx_reg_write(EDP_TX_PHY_RESET, 0);
	status = dptx_wait_phy_ready();
	mdelay(10);
	dptx_reg_write(EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 1);

	return status;
}
#endif

/* Main Stream Attributes */
static void dptx_set_MSA(struct EDP_MSA_s *vm)
{
	unsigned lane_count;
	unsigned data_per_lane;
	unsigned misc0_data;
	unsigned n_vid;
	struct EDP_Link_Config_s *link_config = dptx_get_link_config();

	switch (link_config->link_rate) {
	case VAL_EDP_TX_LINK_BW_SET_162:
		n_vid = 162000;
		break;
	case VAL_EDP_TX_LINK_BW_SET_270:
		n_vid = 270000;
		break;
	case VAL_EDP_TX_LINK_BW_SET_540:
		n_vid = 540000;
		break;
	default:
		n_vid = 270000;
		break;
	}

	lane_count = dptx_reg_read(EDP_TX_LINK_COUNT_SET);
	/* -1 or -lane_count? */
	data_per_lane = ((vm->h_active * vm->bpc * 3) + 15) / 16 - lane_count;

	/* bit[0] sync mode (1=sync 0=async) */
	misc0_data = ((vm->cformat << 1) | (vm->sync_clock_mode << 0));
	switch (vm->bpc) {
	case 6:
		misc0_data = (misc0_data & 0x1f) | (0x0 << 5);
		break;
	case 10:
		misc0_data = (misc0_data & 0x1f) | (0x2 << 5);
		break;
	case 12:
		misc0_data = (misc0_data & 0x1f) | (0x3 << 5);
		break;
	case 16:
		misc0_data = (misc0_data & 0x1f) | (0x4 << 5);
		break;
	case 8:
	default:
		misc0_data = (misc0_data & 0x1f) | (0x1 << 5);
		break;
	}

	dptx_reg_write(EDP_TX_MAIN_STREAM_HTOTAL, vm->h_period);
	dptx_reg_write(EDP_TX_MAIN_STREAM_VTOTAL, vm->v_period);
	dptx_reg_write(EDP_TX_MAIN_STREAM_POLARITY,
		((vm->vsync_pol & 0x1) << 1) | (vm->vsync_pol & 0x1));
	dptx_reg_write(EDP_TX_MAIN_STREAM_HSWIDTH, vm->hsync_width);
	dptx_reg_write(EDP_TX_MAIN_STREAM_VSWIDTH, vm->vsync_width);
	dptx_reg_write(EDP_TX_MAIN_STREAM_HRES, vm->h_active);
	dptx_reg_write(EDP_TX_MAIN_STREAM_VRES, vm->v_active);
	dptx_reg_write(EDP_TX_MAIN_STREAM_HSTART,
		(vm->hsync_bp + vm->hsync_width));
	dptx_reg_write(EDP_TX_MAIN_STREAM_VSTART,
		(vm->vsync_bp + vm->vsync_width));
	/* dptx_reg_write(EDP_TX_MAIN_STREAM_MISC0,
		((vm->cformat << 1) | (1 << 0))); //always sync mode */
	dptx_reg_write(EDP_TX_MAIN_STREAM_MISC0, misc0_data);
	dptx_reg_write(EDP_TX_MAIN_STREAM_MISC1, 0x00000000);
	/* unit: 1kHz */
	dptx_reg_write(EDP_TX_MAIN_STREAM_M_VID, (vm->clk / 1000));
	dptx_reg_write(EDP_TX_MAIN_STREAM_N_VID, n_vid); /* unit: 10kHz */
	dptx_reg_write(EDP_TX_MAIN_STREAM_TRANSFER_UNIT_SIZE, 32);
	/* bytes per lane */
	dptx_reg_write(EDP_TX_MAIN_STREAM_DATA_COUNT_PER_LANE, data_per_lane);
	dptx_reg_write(EDP_TX_MAIN_STREAM_USER_PIXEL_WIDTH, vm->ppc);
}

static unsigned int dptx_init_lane_config(unsigned char link_rate,
		unsigned char lane_count)
{
	unsigned enhance_framing_mode;
	unsigned int status = 0;

	DPRINT("set link_rate=0x%x, lane_count=%u\n", link_rate, lane_count);
	switch (link_rate) {
	case VAL_EDP_TX_LINK_BW_SET_162:
	case VAL_EDP_TX_LINK_BW_SET_270:
		status = RET_EDP_TX_OPERATION_SUCCESS;
		break;
	default:
		status = RET_EDP_TX_AUX_INVALID_PARAMETER;
		break;
	}
	if (status)
		DPRINT("error parameters\n");

	switch (lane_count) {
	case 1:
	case 2:
	case 4:
		enhance_framing_mode =
			(dptx_reg_read(EDP_TX_ENHANCED_FRAME_EN) & 0x01);
		if (enhance_framing_mode)
			lane_count |= (1 << 7);
		status = RET_EDP_TX_OPERATION_SUCCESS;
		break;
	default:
		status = RET_EDP_TX_AUX_INVALID_PARAMETER;
		break;
	}
	if (status == RET_EDP_TX_OPERATION_SUCCESS) {
		dptx_reg_write(EDP_TX_LINK_BW_SET, link_rate);
		dptx_reg_write(EDP_TX_LINK_COUNT_SET, lane_count);
		switch (lane_count) {
		case 1:
			/* power up lane 0 */
			dptx_reg_write(EDP_TX_PHY_POWER_DOWN, 0xe);
			break;
		case 2:
			/* power up lane 0,1 */
			dptx_reg_write(EDP_TX_PHY_POWER_DOWN, 0xc);
			break;
		case 4:
			/* power up lane 0,1,2,3 */
			dptx_reg_write(EDP_TX_PHY_POWER_DOWN, 0x0);
			break;
		default:
			/* power up phy */
			dptx_reg_write(EDP_TX_PHY_POWER_DOWN, 0x0);
			break;
		}
	} else {
		DPRINT("error parameters\n");
	}

	return status;
}

static unsigned int dptx_init_downspread(unsigned char ss_enable)
{
	unsigned int status = 0;

	lcd_print("set spread spectrum\n");
	ss_enable = (ss_enable > 0) ? 1 : 0;

	/* set in transmitter */
	dptx_reg_write(EDP_TX_DOWNSPREAD_CTRL, ss_enable);

	return status;
}

static unsigned int trdp_AUX_check_status(void)
{
	if (dptx_reg_read(EDP_TX_TRANSMITTER_OUTPUT_ENABLE) & 1)
		return 0;
	else
		return 1;
}

static unsigned int trdp_AUX_request(struct TRDP_AUXTrans_Req_s *req)
{
	unsigned int status = 0;
	int i = 0;

	status = trdp_AUX_check_status();
	if (status) {
		DPRINT("AUXRead error: edp transmitter disabled\n");
		return status;
	}

	/* check for transmitter ready state */
	do {
		status = dptx_reg_read(EDP_TX_AUX_STATE);
	} while (status & VAL_EDP_TX_AUX_STATE_REQUEST_IN_PROGRESS);

	dptx_reg_write(EDP_TX_AUX_ADDRESS, req->address); /* set address */
	/* submit data only for write commands */
	if (req->cmd_state == VAL_AUX_CMD_STATE_WRITE) {
		for (i = 0; i < req->byte_count; i++)
			dptx_reg_write(EDP_TX_AUX_WRITE_FIFO, req->wr_data[i]);
	}
	/* submit the command and the data size */
	dptx_reg_write(EDP_TX_AUX_COMMAND,
		(req->cmd_code | ((req->byte_count - 1) & 0x0f)));

	return status;
}

static unsigned int trdp_wait_reply(void)
{
	unsigned int status = 0;
	unsigned int state_value = 0;
	unsigned int done = 0;
	int timeout_count = 0;
	unsigned int failsafe_timeout = 100 * 1000 / VAL_EDP_TX_AUX_WAIT_TIME;

	while (!done) {
		state_value = dptx_reg_read(EDP_TX_AUX_STATUS);
		if (state_value & VAL_EDP_TX_AUX_STATUS_REPLY_RECEIVED) {
			status = RET_EDP_TX_AUX_OPERATION_SUCCESS;
			done = 1;
		} else if (state_value & VAL_EDP_TX_AUX_STATUS_REPLY_ERROR) {
			status = RET_EDP_TX_AUX_OPERATION_TIMEOUT;
			done = 1;
		} else {
			status = RET_EDP_TX_AUX_OPERATION_TIMEOUT;
			if (timeout_count++ == failsafe_timeout)
				done = 1;
			udelay(VAL_EDP_TX_AUX_WAIT_TIME);
		}
	}

	return status;
}

static unsigned int trdp_AUX_submit_cmd(struct TRDP_AUXTrans_Req_s *req)
{
	unsigned int status = RET_EDP_TX_AUX_OPERATION_TIMEOUT;
	unsigned int reply_code = 0;
	unsigned int defer_count = 0;
	unsigned int timeout_count = 0;
	unsigned int cmd_flag;

	/* flag: 0=read, 1=write */
	cmd_flag = (req->cmd_state == VAL_AUX_CMD_STATE_WRITE) ? 1 : 0;
	while ((status == RET_EDP_TX_AUX_OPERATION_TIMEOUT) &&
		(defer_count < VAL_EDP_TX_AUX_MAX_DEFER_COUNT) &&
		(timeout_count < VAL_EDP_TX_AUX_MAX_TIMEOUT_COUNT)) {
		trdp_AUX_request(req);
		status = trdp_wait_reply();
		if (status != RET_EDP_TX_AUX_OPERATION_TIMEOUT) {
			reply_code = dptx_reg_read(EDP_TX_AUX_REPLY_CODE);
			switch (reply_code) {
			case VAL_EDP_TX_AUX_REPLY_CODE_ACK:
			/* same as VAL_EDP_TX_AUX_REPLY_CODE_I2C_ACK */
				status = RET_EDP_TX_AUX_OPERATION_SUCCESS;
				/* DPRINT("edp AUX %s successful\n",
					((cmd_flag) ? "write" : "read")); */
				break;
			case VAL_EDP_TX_AUX_REPLY_CODE_I2C_DEFER:
				status = RET_EDP_TX_AUX_OPERATION_TIMEOUT;
				udelay(VAL_EDP_TX_AUX_WAIT_TIME);
				defer_count++;
				DPRINT("edp AUX i2c %s defered\n",
					((cmd_flag) ? "write" : "read"));
				break;
			case VAL_EDP_TX_AUX_REPLY_CODE_I2C_NACK:
				status = RET_EDP_TX_AUX_OPERATION_FAILED;
				DPRINT("edp AUX i2c %s NACK\n",
					((cmd_flag) ? "write" : "read"));
				break;
			case VAL_EDP_TX_AUX_REPLY_CODE_DEFER:
				status = RET_EDP_TX_AUX_OPERATION_TIMEOUT;
				udelay(VAL_EDP_TX_AUX_WAIT_TIME);
				defer_count++;
				DPRINT("edp AUX %s defered\n",
					((cmd_flag) ? "write" : "read"));
				break;
			case VAL_EDP_TX_AUX_REPLY_CODE_NACK:
				status = RET_EDP_TX_AUX_OPERATION_FAILED;
				DPRINT("edp AUX %s NACK\n",
					((cmd_flag) ? "write" : "read"));
				break;
			default: /* unknown or illegal response code */
				status = RET_EDP_TX_AUX_OPERATION_TIMEOUT;
				udelay(VAL_EDP_TX_AUX_WAIT_TIME);
				if (timeout_count++ > 2)
					status = RET_EDP_TX_AUX_OPERATION_ERROR;
				DPRINT("edp AUX %s error\n",
					((cmd_flag) ? "write" : "read"));
				break;
			}
		} else { /* timeout failsafe to avoid an infinite loop */
			timeout_count++;
			udelay(VAL_EDP_TX_AUX_WAIT_TIME);
			DPRINT("edp AUX %s timeout\n",
				((cmd_flag) ? "write" : "read"));
		}
	}

	return status;
}

#if 0
#define EDP_AUX_OPERATION_RETRY
/* Read EDPTX */
/* Read N-bytes from Aux-Channel -- upto 16bytes */
/* retry */
static unsigned int trdp_AUXRead(unsigned long address,
		unsigned long byte_count, unsigned char *data)
{
	int i;
	unsigned int status;
	int reply_state = RET_EDP_TX_AUX_OPERATION_TIMEOUT;
	unsigned defer_count = 0;
	unsigned timeout_count = 0;

	status = trdp_AUX_check_status();
	if (status) {
		DPRINT("AUXRead error: edp transmitter disabled\n");
		return status;
	}

	while ((reply_state == RET_EDP_TX_AUX_OPERATION_TIMEOUT) &&
		(defer_count < VAL_EDP_TX_AUX_MAX_DEFER_COUNT) &&
		(timeout_count < VAL_EDP_TX_AUX_MAX_TIMEOUT_COUNT)) {
		/* check for transmitter ready state */
		do {
			status = dptx_reg_read(EDP_TX_AUX_STATE);
		} while (status & VAL_EDP_TX_AUX_STATE_REQUEST_IN_PROGRESS);

		/* read AUX command */
		dptx_reg_write(EDP_TX_AUX_ADDRESS, address);
		dptx_reg_write(EDP_TX_AUX_COMMAND, (VAL_EDP_TX_AUX_CMD_READ |
			((byte_count-1) & 0xF)));

		/* wait reply */
		reply_state = RET_EDP_TX_AUX_OPERATION_SUCCESS;
		do {
			status = dptx_reg_read(EDP_TX_AUX_STATE);
			if (status & VAL_EDP_TX_AUX_STATE_TIMEOUT) {
				reply_state = RET_EDP_TX_AUX_OPERATION_TIMEOUT;
				timeout_count++;
				udelay(VAL_EDP_TX_AUX_WAIT_TIME);
				break;
			}
		} while ((status & VAL_EDP_TX_AUX_STATE_RECEIVED) == 0);

		if (reply_state != RET_EDP_TX_AUX_OPERATION_TIMEOUT) {
			status = dptx_reg_read(EDP_TX_AUX_REPLY_CODE);
			switch (status) {
			case VAL_EDP_TX_AUX_REPLY_CODE_ACK:
				reply_state = RET_EDP_TX_AUX_OPERATION_SUCCESS;
				break;
			case VAL_EDP_TX_AUX_REPLY_CODE_NACK:
				reply_state = RET_EDP_TX_AUX_OPERATION_FAILED;
				break;
			case VAL_EDP_TX_AUX_REPLY_CODE_DEFER:
				reply_state = RET_EDP_TX_AUX_OPERATION_TIMEOUT;
				udelay(VAL_EDP_TX_AUX_WAIT_TIME);
				defer_count++;
				break;
			default:
				reply_state = RET_EDP_TX_AUX_OPERATION_TIMEOUT;
				udelay(VAL_EDP_TX_AUX_WAIT_TIME);
				if (timeout_count++ > 2)
					reply_state =
						RET_EDP_TX_AUX_OPERATION_ERROR;
				break;
			}
		} else {
			DPRINT("AUXRead timeout\n");
		}
	}

	if (reply_state == RET_EDP_TX_AUX_OPERATION_SUCCESS) {
		for (i = 0; i < byte_count; i++)
			data[i] = dptx_reg_read(EDP_TX_AUX_REPLY_DATA);
	} else {
		DPRINT("AUXRead failed\n");
	}

	return reply_state;
}

/* Write N-bytes to Aux-Channel -- upto 16bytes */
/* retry */
static unsigned int trdp_AUXWrite(unsigned long address,
		unsigned long byte_count, unsigned char *data)
{
	int i;
	unsigned int status;
	unsigned int reply_state = RET_EDP_TX_AUX_OPERATION_TIMEOUT;
	unsigned defer_count = 0;
	unsigned timeout_count = 0;

	status = trdp_AUX_check_status();
	if (status) {
		DPRINT("AUXWrite error: edp transmitter disabled\n");
		return status;
	}

	while ((reply_state == RET_EDP_TX_AUX_OPERATION_TIMEOUT) &&
		(defer_count < VAL_EDP_TX_AUX_MAX_DEFER_COUNT) &&
		(timeout_count < VAL_EDP_TX_AUX_MAX_TIMEOUT_COUNT)) {
		/* check for transmitter ready state */
		do {
			status = dptx_reg_read(EDP_TX_AUX_STATE);
		} while (status & VAL_EDP_TX_AUX_STATE_REQUEST_IN_PROGRESS);

		/* write AUX command */
		dptx_reg_write(EDP_TX_AUX_ADDRESS, address);
		for (i = 0; i < byte_count; i++)
			dptx_reg_write(EDP_TX_AUX_WRITE_FIFO, data[i]);

		dptx_reg_write(EDP_TX_AUX_COMMAND,
			(VAL_EDP_TX_AUX_CMD_WRITE | ((byte_count-1) & 0xF)));

		/* wait reply */
		reply_state = RET_EDP_TX_AUX_OPERATION_SUCCESS;
		do {
			status = dptx_reg_read(EDP_TX_AUX_STATE);
			if (status & VAL_EDP_TX_AUX_STATE_TIMEOUT) {
				reply_state = RET_EDP_TX_AUX_OPERATION_TIMEOUT;
				timeout_count++;
				udelay(VAL_EDP_TX_AUX_WAIT_TIME);
				break;
			}
		} while ((status & VAL_EDP_TX_AUX_STATE_RECEIVED) == 0);

		if (reply_state != RET_EDP_TX_AUX_OPERATION_TIMEOUT) {
			status = dptx_reg_read(EDP_TX_AUX_REPLY_CODE);
			switch (status) {
			case VAL_EDP_TX_AUX_REPLY_CODE_ACK:
				reply_state = RET_EDP_TX_AUX_OPERATION_SUCCESS;
				break;
			case VAL_EDP_TX_AUX_REPLY_CODE_NACK:
				reply_state = RET_EDP_TX_AUX_OPERATION_FAILED;
				break;
			case VAL_EDP_TX_AUX_REPLY_CODE_DEFER:
				reply_state = RET_EDP_TX_AUX_OPERATION_TIMEOUT;
				udelay(VAL_EDP_TX_AUX_WAIT_TIME);
				defer_count++;
				break;
			default:
				reply_state = RET_EDP_TX_AUX_OPERATION_TIMEOUT;
				udelay(VAL_EDP_TX_AUX_WAIT_TIME);
				if (timeout_count++ > 2)
					reply_state =
						RET_EDP_TX_AUX_OPERATION_ERROR;
				break;
			}
		} else {
			DPRINT("AUXWrite time out\n");
		}
	}

	return reply_state;
}

#else
static unsigned int trdp_AUXRead(unsigned long address,
		unsigned long byte_count, unsigned char *data)
{
	int i;
	unsigned int status;
	struct TRDP_AUXTrans_Req_s request;

	status = trdp_AUX_check_status();
	if (status) {
		DPRINT("AUXRead error: edp transmitter disabled\n");
		return status;
	}

	request.cmd_code = VAL_EDP_TX_AUX_CMD_READ;
	request.cmd_state = VAL_AUX_CMD_STATE_READ;
	request.address = address;
	request.byte_count = byte_count;
	status = trdp_AUX_submit_cmd(&request);
	if (status == RET_EDP_TX_AUX_OPERATION_SUCCESS) {
		for (i = 0; i < byte_count; i++)
			data[i] = dptx_reg_read(EDP_TX_AUX_REPLY_DATA);
	} else {
		DPRINT("AUXRead failed\n");
		return RET_EDP_TX_AUX_OPERATION_FAILED;
	}

	return status;
}

static unsigned int trdp_AUXWrite(unsigned long address,
		unsigned long byte_count, unsigned char *data)
{
	unsigned int status;
	struct TRDP_AUXTrans_Req_s request;

	status = trdp_AUX_check_status();
	if (status) {
		DPRINT("AUXRead error: edp transmitter disabled\n");
		return status;
	}

	request.cmd_code = VAL_EDP_TX_AUX_CMD_WRITE;
	request.cmd_state = VAL_AUX_CMD_STATE_WRITE;
	request.address = address;
	request.byte_count = byte_count;
	request.wr_data = data;
	status = trdp_AUX_submit_cmd(&request);
	if (status) {
		DPRINT("AUXWrite failed\n");
		return RET_EDP_TX_AUX_OPERATION_FAILED;
	}

	return status;
}
#endif
static unsigned int trdp_AUX_I2C_Read(unsigned int dev_addr,
		unsigned int reg_addr, unsigned int byte_count,
		unsigned char *data_buffer)
{
	unsigned int status = 0;
	unsigned int data_count = 0, data_index = 0;
	int i;
	unsigned char aux_data[4];
	unsigned char done = 0;
	unsigned int reply_data_count = 0;
	struct TRDP_AUXTrans_Req_s request;

	status = trdp_AUX_check_status();
	if (status) {
		DPRINT("AUXRead error: edp transmitter disabled\n");
		return status;
	}

	/* cap the byte count */
	byte_count = (byte_count > 16) ? 16 : byte_count;

	aux_data[0] = reg_addr;
	aux_data[1] = 0x00;

	/* setup command */
	request.cmd_code = VAL_EDP_TX_AUX_CMD_I2C_WRITE_MOT;
	request.cmd_state = VAL_AUX_CMD_STATE_WRITE;
	request.address = dev_addr;
	request.byte_count = 1;
	request.wr_data = aux_data;

	/* send the address write */
	status = trdp_AUX_submit_cmd(&request);
	if (status != RET_EDP_TX_AUX_OPERATION_SUCCESS) {
		DPRINT("Error: I2C writing device address ");
		DPRINT("(0x%04x) failed (0x%02x)\n", dev_addr, status);
		return RET_EDP_TX_OPERATION_FAILED;
	}
	/* submit the read command to hardware */
	request.cmd_code = VAL_EDP_TX_AUX_CMD_I2C_READ;
	request.cmd_state = VAL_AUX_CMD_STATE_READ;
	request.address = dev_addr;
	request.byte_count = byte_count;

	data_count = 0;
	data_index = 0;
	while ((data_count < byte_count) && (!done)) {
		status = trdp_AUX_submit_cmd(&request);
		if (status == RET_EDP_TX_AUX_OPERATION_SUCCESS) {
			reply_data_count =
				dptx_reg_read(EDP_TX_AUX_REPLY_DATA_COUNT);
			for (i = 0; i < reply_data_count; i++) {
				data_buffer[data_index] =
					dptx_reg_read(EDP_TX_AUX_REPLY_DATA);
				data_index++;
				data_count++;
			}
			done = 1;
		} else {
			DPRINT("Error: I2C read device address ");
			DPRINT("(0x%04x) failed (0x%02x)\n", dev_addr, status);
			return RET_EDP_TX_OPERATION_FAILED;
		}
		request.byte_count -= reply_data_count;
		/* increment the address for the next transaction */
		aux_data[0] += reply_data_count;
	}

	return status;
}
#if 0
static unsigned int trdp_AUX_I2C_Write(unsigned int dev_addr,
		unsigned int reg_addr, unsigned int byte_count,
		unsigned char *data_buffer)
{
	unsigned int status = 0;
	unsigned char aux_data[4];
	struct TRDP_AUXTrans_Req_s request;

	status = trdp_AUX_check_status();
	if (status) {
		DPRINT("AUXRead error: edp transmitter disabled\n");
		return status;
	}

	byte_count = (byte_count > 16) ? 16 : byte_count;

	aux_data[0] = reg_addr;
	aux_data[1] = 0;

	/* setup command */
	request.cmd_code = VAL_EDP_TX_AUX_CMD_I2C_WRITE_MOT;
	request.cmd_state = VAL_AUX_CMD_STATE_WRITE;
	request.byte_count = 1;
	request.wr_data = aux_data;

	/* send the address write */
	status = trdp_AUX_submit_cmd(&request);
	if (status != VAL_EDP_TX_AUX_REPLY_CODE_I2C_ACK) {
		DPRINT("Error: I2C writing device address ");
		DPRINT("(0x%04x) failed (0x%02x)\n", dev_addr, status);
		return RET_EDP_TX_OPERATION_FAILED;
	}

	/* submit the write command to hardware */
	request.cmd_code = VAL_EDP_TX_AUX_CMD_I2C_WRITE;
	request.cmd_state = VAL_AUX_CMD_STATE_WRITE;
	request.address = dev_addr;
	request.byte_count = byte_count;
	request.wr_data = data_buffer;

	status = trdp_AUX_submit_cmd(&request);
	if (status != VAL_EDP_TX_AUX_REPLY_CODE_I2C_ACK) {
		DPRINT("Error: I2C writing device address ");
		DPRINT("(0x%04x) failed (0x%02x)\n", dev_addr, status);
		status = RET_EDP_TX_OPERATION_FAILED;
	}

	return status;
}
#endif

static void EDID_parse_print(struct EDP_EDID_Data_Type_s *edid_parse)
{
	DPRINT("Manufacturer ID:       %s\n"
		"Product ID:            0x%04x\n"
		"Product SN:            0x%08x\n"
		"Week:                  %d\n"
		"Year:                  %d\n"
		"EDID Version:          %04x\n",
		&edid_parse->mid[0], edid_parse->pid,
		edid_parse->psn, edid_parse->week,
		edid_parse->year, edid_parse->version);
	if ((edid_parse->string_flag >> 0) & 1) { /* monitor name */
		DPRINT("Monitor Name:          %s\n",
			&edid_parse->name[0]);
	}
	/* monitor ascii string */
	if ((edid_parse->string_flag >> 1) & 1) {
		DPRINT("Monitor AScii String:  %s\n",
			&edid_parse->asc_string[0]);
	}
	if ((edid_parse->string_flag >> 2) & 1) { /* monitor serial num */
		DPRINT("Monitor SN:            %s\n",
			&edid_parse->serial_num[0]);
	}
	DPRINT("Detail Timing:\n"
		"    Pixel Clock:   %d.%dMHz\n"
		"    H Active:      %d\n"
		"    H Blank:       %d\n"
		"    V Active:      %d\n"
		"    V Blank:       %d\n"
		"    H FP:          %d\n"
		"    H PW:          %d\n"
		"    V FP:          %d\n"
		"    V PW:          %d\n"
		"    H Size:        %dmm\n"
		"    V Size:        %dmm\n"
		"    H Border:      %d\n"
		"    V Border:      %d\n"
		"    Hsync Pol:     %d\n"
		"    Vsync Pol:     %d\n",
		(edid_parse->preferred_timing.pclk / 1000000),
		((edid_parse->preferred_timing.pclk % 1000000) / 1000),
		edid_parse->preferred_timing.h_active,
		edid_parse->preferred_timing.h_blank,
		edid_parse->preferred_timing.v_active,
		edid_parse->preferred_timing.v_blank,
		edid_parse->preferred_timing.h_fp,
		edid_parse->preferred_timing.h_pw,
		edid_parse->preferred_timing.v_fp,
		edid_parse->preferred_timing.v_pw,
		edid_parse->preferred_timing.h_size,
		edid_parse->preferred_timing.v_size,
		edid_parse->preferred_timing.h_border,
		edid_parse->preferred_timing.v_border,
		((edid_parse->preferred_timing.timing_ctrl >> 1) & 1),
		((edid_parse->preferred_timing.timing_ctrl >> 2) & 1));
}

static void EDID_get_preferred_timing(unsigned char *edid_data,
		struct EDP_EDID_Data_Type_s *edid_parse, int j)
{
	struct EDID_Timing_s *prt;
	unsigned short temp;

	prt = &(edid_parse->preferred_timing);
	prt->pclk = ((((unsigned int)edid_data[j+1]) << 8) |
		((unsigned int)edid_data[j])) * 10000;
	temp = (((((unsigned short)edid_data[j+4]) >> 4) & 0xf) << 8);
	prt->h_active = (temp | ((unsigned short)edid_data[j+2]));
	temp = (((((unsigned short)edid_data[j+4]) >> 0) & 0xf) << 8);
	prt->h_blank = (temp | ((unsigned short)edid_data[j+3]));
	temp = (((((unsigned short)edid_data[j+7]) >> 4) & 0xf) << 8);
	prt->v_active = (temp | ((unsigned short)edid_data[j+5]));
	temp = (((((unsigned short)edid_data[j+7]) >> 0) & 0xf) << 8);
	prt->v_blank = (temp | ((unsigned short)edid_data[j+6]));
	temp = (((((unsigned short)edid_data[j+11]) >> 6) & 0x3) << 8);
	prt->h_fp = (temp | ((unsigned short)edid_data[j+8]));
	temp = (((((unsigned short)edid_data[j+11]) >> 4) & 0x3) << 8);
	prt->h_pw = (temp | ((unsigned short)edid_data[j+9]));
	temp = (((((unsigned short)edid_data[j+11]) >> 2) & 0x3) << 4);
	prt->v_fp = (temp | ((((unsigned short)edid_data[j+10]) >> 4) & 0xf));
	temp = (((((unsigned short)edid_data[j+11]) >> 0) & 0x3) << 4);
	prt->v_pw = (temp | ((((unsigned short)edid_data[j+10]) >> 0) & 0xf));
	temp = (((((unsigned int)edid_data[j+14]) >> 4) & 0xf) << 8);
	prt->h_size = (temp | ((unsigned int)edid_data[j+12]));
	temp = (((((unsigned int)edid_data[j+14]) >> 0) & 0xf) << 8);
	prt->v_size = (temp | ((unsigned int)edid_data[j+13]));
	prt->h_border = (unsigned short)edid_data[j+15];
	prt->v_border = (unsigned short)edid_data[j+16];
	prt->timing_ctrl = (unsigned int)edid_data[j+17];
}

static void EDID_get_range_limit(unsigned char *edid_data,
		struct EDP_EDID_Data_Type_s *edid_parse, int j)
{
	struct EDID_Range_Limit_s *r_limit;

	r_limit = &(edid_parse->range_limit);
	r_limit->min_vfreq = (unsigned int)edid_data[j+5];
	r_limit->max_v_freq = (unsigned int)edid_data[j+6];
	r_limit->min_hfreq = (unsigned int)edid_data[j+7];
	r_limit->max_hfreq = (unsigned int)edid_data[j+8];
	r_limit->max_pclk = (unsigned int)edid_data[j+9];
	r_limit->GTF_ctrl = ((((unsigned int)edid_data[j+11]) << 8) |
			((unsigned int)edid_data[j+10]));
	r_limit->GTF_start_hfreq = (unsigned int)edid_data[j+12] * 2000;
	r_limit->GTF_C = (unsigned int)edid_data[j+13] / 2;
	r_limit->GTF_M = ((((unsigned int)edid_data[j+15]) << 8) |
			((unsigned int)edid_data[j+14]));
	r_limit->GTF_K = (unsigned int)edid_data[j+16];
	r_limit->GTF_J = (unsigned int)edid_data[j+17] / 2;
}

static unsigned int EDID_parse_data(unsigned char *edid_data,
		struct EDP_EDID_Data_Type_s *edid_parse)
{
	int i, j, k;
	unsigned int status = 0;
	unsigned int checksum;
	unsigned int temp;
	unsigned char edid_header[8] = {
			0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};

	for (i = 0; i < 8; i++) {
		if (edid_data[i] != edid_header[i]) {
			status = RET_EDP_TX_OPERATION_FAILED;
			DPRINT("Invalid EDID data\n");
			return status;
		}
	}

	temp = ((((unsigned short)edid_data[8]) << 8) |
		((unsigned short)edid_data[9]));
	for (i = 0; i < 3; i++) {
		edid_parse->mid[i] =
			(((temp >> ((2-i)*5)) & 0x1f) - 1) + 'A';
	}
	edid_parse->mid[3] = '\0';
	edid_parse->pid = ((((unsigned short)edid_data[11]) << 8) |
				((unsigned short)edid_data[10]));
	edid_parse->psn = ((((unsigned int)edid_data[12]) << 24) |
				(((unsigned int)edid_data[13]) << 16) |
				(((unsigned int)edid_data[14]) << 8) |
				((unsigned int)edid_data[15]));
	edid_parse->week = edid_data[16];
	edid_parse->year = 1990 + edid_data[17];
	edid_parse->version = ((((unsigned short)edid_data[18]) << 8) |
					((unsigned short)edid_data[19]));

	edid_parse->string_flag = 0;
	for (i = 0; i < 4; i++) {
		j = 54 + i * 18;
		if ((edid_data[j] + edid_data[j+1]) == 0) {
			if ((edid_data[j+2] + edid_data[j+4]) == 0) {
				switch (edid_data[j+3]) {
				case 0xfc: /* monitor name */
					for (k = 0; k < 13; k++)
						edid_parse->name[k] =
							edid_data[j+5+k];
					edid_parse->name[13] = '\0';
					edid_parse->string_flag |=
						(1 << 0);
					break;
				case 0xfd: /* monitor range limits */
					EDID_get_range_limit(edid_data,
							edid_parse,
							j);
					break;
				case 0xfe: /* ascii string */
					for (k = 0; k < 13; k++) {
						edid_parse->asc_string[k] =
							edid_data[j+5+k];
					}
					edid_parse->asc_string[13] = '\0';
					edid_parse->string_flag |= (1 << 1);
					break;
				case 0xff: /* monitor serial num */
					for (k = 0; k < 13; k++) {
						edid_parse->serial_num[k] =
							edid_data[j+5+k];
					}
					edid_parse->serial_num[13] = '\0';
					edid_parse->string_flag |= (1 << 2);
					break;
				default:
					break;
				}
			}
		} else {/* detail timing */
			EDID_get_preferred_timing(edid_data, edid_parse, j);
		}
	}

	edid_parse->ext_flag = edid_data[126];
	edid_parse->checksum = edid_data[127];
	checksum = 0;
	for (i = 0; i < 128; i++)
		checksum += edid_data[i];
	if ((checksum & 0xff) == 0) {
		/* status = RET_EDP_TX_OPERATION_SUCCESS; */
		DPRINT("EDID checksum: Right\n");
	} else {
		/* status = RET_EDP_TX_OPERATION_FAILED; */
		DPRINT("EDID checksum: Wrong\n");
	}

	return status;
}

static unsigned int trdp_read_EDID(unsigned char *edid_data)
{
	unsigned int status = 0;
	int i;

	if (edid_data == NULL) {
		DPRINT("Invalid pointer for EDID data\n");
		return RET_EDP_TX_OPERATION_FAILED;
	}

	for (i = 0; i < 128; i += 16) {
		status = trdp_AUX_I2C_Read(0x50, i, 16, &edid_data[i]);
		if (status)
			break;
	}

	return status;
}

static unsigned int trdp_dump_EDID(void)
{
	unsigned char edid_data[128];
	unsigned int status = 0;
	struct EDP_EDID_Data_Type_s edid_data_parse;
	int i = 0;

	status = trdp_read_EDID(&edid_data[0]);
	if (status) {
		DPRINT("Failed to get EDID from sink device\n");
	} else {
		DPRINT("********************************************\n");
		DPRINT("dump EDID Raw data:");
		for (i = 0; i < 128; i++) {
			if (i % 16 == 0)
				DPRINT("\n");
			DPRINT("0x%02x ", edid_data[i]);
		}
		DPRINT("\n");

		status = EDID_parse_data(&edid_data[0], &edid_data_parse);
		if (status == RET_EDP_TX_OPERATION_SUCCESS)
			EDID_parse_print(&edid_data_parse);
		DPRINT("********************************************\n");
	}

	return status;
}

static unsigned int trdp_get_sink_caps(struct TRDP_DPCDData_s *dpcd_data)
{
	unsigned int status = 0, xx = 0;
	unsigned char aux_data[16];

	if (dpcd_data == NULL) {
		DPRINT("Invalid pointer for DPCD data\n");
		return RET_EDP_TX_AUX_OPERATION_FAILED;
	}

	status = trdp_AUXRead(EDP_DPCD_REVISION, 12, aux_data);
	if (status)
		return status;

	/* Clear data structures */
	memset(dpcd_data->downstream_port_types, 0, 10);
	memset(dpcd_data->downstream_port_caps, 0, 10);

	switch (aux_data[0]) {
	case 0x10:
		dpcd_data->dpcd_rev = 0x10;
		dpcd_data->rev_string = dpcd_strings_5;
		break;
	case 0x11:
		dpcd_data->dpcd_rev = 0x11;
		dpcd_data->rev_string = dpcd_strings_6;
		break;
	case 0x12:
		dpcd_data->dpcd_rev = 0x12;
		dpcd_data->rev_string = dpcd_strings_14;
		break;
	default:
		dpcd_data->dpcd_rev = 0x00;
		dpcd_data->rev_string = dpcd_strings_0;
		break;
	}

	switch (aux_data[1]) {
	case VAL_EDP_TX_LINK_BW_SET_162:
		dpcd_data->max_link_rate = VAL_EDP_TX_LINK_BW_SET_162;
		dpcd_data->link_rate_string = dpcd_strings_7;
		break;
	case VAL_EDP_TX_LINK_BW_SET_270:
		dpcd_data->max_link_rate = VAL_EDP_TX_LINK_BW_SET_270;
		dpcd_data->link_rate_string = dpcd_strings_8;
		break;
	case VAL_EDP_TX_LINK_BW_SET_540:
		dpcd_data->max_link_rate = VAL_EDP_TX_LINK_BW_SET_540;
		dpcd_data->link_rate_string = dpcd_strings_15;
		break;
	default:
		dpcd_data->max_link_rate = 0;
		dpcd_data->link_rate_string = dpcd_strings_1;
		break;
	}

	/* Lane count */
	dpcd_data->max_lane_count = aux_data[2] & 0x07;
	/* Enhanced framing support */
	dpcd_data->enhanced_framing_support = (aux_data[2] >> 7) & 0x01;
	/* Max downspread support */
	dpcd_data->downspread_support = (aux_data[3] & 0x01);
	/* AUX handshake required */
	dpcd_data->require_aux_handshake = (aux_data[3] >> 6) & 0x01;
	/* Number of receive ports */
	dpcd_data->num_rcv_ports = (aux_data[4] & 0x01);
	/* ANSI 8B/10B coding support */
	dpcd_data->ansi_8B10_support = (aux_data[6] & 0x01);
	/* Number of downstream ports */
	dpcd_data->num_downstream_ports = (aux_data[7] & 0x0F);
	dpcd_data->oui_support = (aux_data[7] >> 7) & 0x01;
	/* Receiver Port 0 capabilities */
	dpcd_data->rx0_has_edid = (aux_data[8] & 0x01);
	dpcd_data->rx0_use_prev = (aux_data[8] & 0x02);
	dpcd_data->rx0_buffer_size = aux_data[9];

	if (dpcd_data->num_downstream_ports) {
		/* Check for format conversion support */
		dpcd_data->format_conversion_support =
			(aux_data[5] >> 4) & 0x01;
		dpcd_data->oui_support = (aux_data[7] >> 8) & 0x01;
		/* Read downstream port capabilities */
		trdp_AUXRead(EDP_DPCD_DOWNSTREAM_PORT_CAPS,
			dpcd_data->num_downstream_ports, aux_data);
		/* Copy to local array */
		memcpy(dpcd_data->downstream_port_caps, aux_data,
			dpcd_data->num_downstream_ports);
		/* Get the port type */
		for (xx = 0; xx < dpcd_data->num_downstream_ports; xx++) {
			/* Downstream port type */
			switch ((dpcd_data->downstream_port_caps[xx] & 0x07)) {
			case 0x01:
				dpcd_data->downstream_port_types[xx] =
					dpcd_strings_9;
				break;
			case 0x02:
				dpcd_data->downstream_port_types[xx] =
					dpcd_strings_10;
				break;
			case 0x03:
				dpcd_data->downstream_port_types[xx] =
					dpcd_strings_11;
				break;
			case 0x04:
				dpcd_data->downstream_port_types[xx] =
					dpcd_strings_12;
				break;
			case 0x05:
				dpcd_data->downstream_port_types[xx] =
					dpcd_strings_13;
				break;
			default:
				dpcd_data->downstream_port_types[xx] =
					dpcd_strings_1;
				break;
			}
		}
	} else {/* No downstream ports */
		dpcd_data->port_type_string = "None";
	}

	return RET_EDP_TX_AUX_OPERATION_SUCCESS;
}

static unsigned int trdp_dump_DPCD(void)
{
	unsigned int status = 0;
	int i;
	struct TRDP_DPCDData_s dpcdinfo;

	DPRINT("********************************************\n");
	status = trdp_get_sink_caps(&dpcdinfo);
	if (status != RET_EDP_TX_AUX_OPERATION_SUCCESS) {
		DPRINT("Failed to get DPCD from sink device\n");
		return status;
	}

	DPRINT("Displayport Configuration Data:\n"
	   "   DPCD Revision                 : %s\n"
	   "   Max Link Rate                 : %s\n"
	   "   Max Lane Count                : %u\n"
	   "      Enhanced Framing           : %s\n"
	   "   Max Downspread                : %s\n"
	   "      Require AUX Handshake      : %s\n"
	   "   Number of RX Ports            : %u\n"
	   "   Main Link ANSI 8B/10B         : %s\n"
	   "   Downstream Port Count         : %u\n"
	   "      Format Conversion Support  : %s\n"
	   "      OUI Support                : %s\n"
	   "   Receiver Port 0:\n"
	   "     Has EDID                    : %s\n"
	   "     Uses Previous Port          : %s\n"
	   "     Buffer Size                 : %u\n",
	    dpcd_strings[dpcdinfo.rev_string],
	    dpcd_strings[dpcdinfo.link_rate_string],
	    dpcdinfo.max_lane_count,
	   (dpcdinfo.enhanced_framing_support == 1) ?
		dpcd_strings[3] : dpcd_strings[4],
	   (dpcdinfo.downspread_support == 1) ? "0.5%" : "None",
	   (dpcdinfo.require_aux_handshake == 1) ?
		dpcd_strings[4] : dpcd_strings[3],
	    dpcdinfo.num_rcv_ports,
	   (dpcdinfo.ansi_8B10_support == 1) ?
		dpcd_strings[3] : dpcd_strings[4],
	    dpcdinfo.num_downstream_ports,
	   (dpcdinfo.format_conversion_support == 1) ?
		dpcd_strings[3] : dpcd_strings[4],
	   (dpcdinfo.oui_support == 1)  ? dpcd_strings[3] : dpcd_strings[4],
	   (dpcdinfo.rx0_has_edid == 1) ? dpcd_strings[3] : dpcd_strings[4],
	   (dpcdinfo.rx0_use_prev == 1) ? dpcd_strings[3] : dpcd_strings[4],
	    dpcdinfo.rx0_buffer_size);

	for (i = 0; i < dpcdinfo.num_downstream_ports; i++) {
		DPRINT("   Downstream Port %u:\n"
			"      Port Type         : %s\n"
			"      HPD Aware         : %s\n",
			i, dpcd_strings[dpcdinfo.downstream_port_types[i]],
			((dpcdinfo.downstream_port_caps[i] & 0x08) == 0x08) ?
				dpcd_strings[3] : dpcd_strings[4]);
	}
	DPRINT("********************************************\n");

	return RET_EDP_TX_AUX_OPERATION_SUCCESS;
}

static unsigned int trdp_dump_DPCD_training_status(void)
{
	unsigned int status = 0;
	unsigned char aux_data[8];

	DPRINT("********************************************\n");
	status = trdp_AUXRead(EDP_DPCD_SINK_COUNT, 8, aux_data);
	if (status != RET_EDP_TX_AUX_OPERATION_SUCCESS) {
		DPRINT("Failed to get DPCD status from sink device\n");
		return status;
	}
	DPRINT("Displayport DPCD training status:\n"
	   "   Lane 0/1 Status          : 0x%02x\n"
	   "   Lane 2/3 Status          : 0x%02x\n"
	   "   Lane Align Status        : 0x%02x\n"
	   "   Sink Status              : 0x%02x\n"
	   "   Adjustment Request 0/1   : 0x%02x\n"
	   "   Adjustment Request 2/3   : 0x%02x\n\n",
	    aux_data[2],
	    aux_data[3],
	    aux_data[4],
	    aux_data[5],
	    aux_data[6],
	    aux_data[7]);

	status = trdp_AUXRead(EDP_DPCD_LINK_BANDWIDTH_SET, 8, aux_data);
	if (status != RET_EDP_TX_AUX_OPERATION_SUCCESS) {
		DPRINT("Failed to get DPCD settings from sink device\n");
		return status;
	}
	DPRINT("  Training Config:\n"
	   "      Link Bandwidth Setup  : 0x%02x\n"
	   "      Lane Count Set        : 0x%02x\n"
	   "      Training Pattern Set  : 0x%02x\n"
	   "      Training Lane 0 Set   : 0x%02x\n"
	   "      Training Lane 1 Set   : 0x%02x\n"
	   "      Training Lane 2 Set   : 0x%02x\n"
	   "      Training Lane 3 Set   : 0x%02x\n"
	   "      Downspread Ctrl       : 0x%02x\n",
	    aux_data[0],
	    aux_data[1],
	    aux_data[2],
	    aux_data[3],
	    aux_data[4],
	    aux_data[5],
	    aux_data[6],
	    aux_data[7]);
	DPRINT("********************************************\n");

	return RET_EDP_TX_AUX_OPERATION_SUCCESS;
}

static void trdp_edp_link_rate_update(unsigned char link_rate)
{
	struct EDP_Link_Config_s *link_config = dptx_get_link_config();

	switch (link_rate) {
	case VAL_EDP_TX_LINK_BW_SET_162:
	case VAL_EDP_TX_LINK_BW_SET_270:
		link_config->link_rate = link_rate;
		break;
	default:
		break;
	}
}
#if 0
static unsigned int trdp_set_link_rate(unsigned char link_rate)
{
	unsigned int status = RET_EDP_TX_AUX_OPERATION_SUCCESS;

	lcd_print("set link rate\n");
	if (link_rate != dptx_reg_read(EDP_TX_LINK_BW_SET)) {
		dptx_reg_write(EDP_TX_LINK_BW_SET, link_rate);
		if (status)
			return status;

		switch (link_rate) {
		case VAL_EDP_TX_LINK_BW_SET_162:
		case VAL_EDP_TX_LINK_BW_SET_270:
			status = trdp_AUXWrite(EDP_DPCD_LINK_BANDWIDTH_SET,
				1, &link_rate);
			break;
		default:
			status = RET_EDP_TX_AUX_INVALID_PARAMETER;
			break;
		}
	}

	return status;
}

static unsigned int trdp_set_lane_count(unsigned char lane_count)
{
	unsigned int status = 0;
	unsigned enhance_framing_mode;

	lcd_print("set lane count\n");
	switch (lane_count) {
	case 1:
	case 2:
	case 4:
		enhance_framing_mode =
			(dptx_reg_read(EDP_TX_ENHANCED_FRAME_EN) & 0x01);
		if (enhance_framing_mode)
			lane_count |= (1 << 7);
		dptx_reg_write(EDP_TX_LINK_COUNT_SET, lane_count);
		status = trdp_AUXWrite(EDP_DPCD_LANE_COUNT_SET, 1, &lane_count);
		break;
	default:
		status = RET_EDP_TX_AUX_INVALID_PARAMETER;
		break;
	}

	return status;
}
#endif
static unsigned int trdp_set_downspread(unsigned char ss_enable)
{
	unsigned int status = 0;

	/* lcd_print("set spread spectrum\n"); */
	ss_enable = (ss_enable > 0) ? 1 : 0;

	/* set in sink device */
	ss_enable <<= 4;
	status = trdp_AUXWrite(EDP_DPCD_DOWNSPREAD_CONTROL, 1, &ss_enable);

	return status;
}

static unsigned int trdp_verify_training_config(void)
{
	unsigned int status = RET_EDP_CONFIG_VALID;
	unsigned link_rate = 0, lane_count = 0;

	link_rate = dptx_reg_read(EDP_TX_LINK_BW_SET);
	lane_count = dptx_reg_read(EDP_TX_LINK_COUNT_SET);

	switch (link_rate) {
	case VAL_EDP_TX_LINK_BW_SET_162:
	case VAL_EDP_TX_LINK_BW_SET_270:
	case VAL_EDP_TX_LINK_BW_SET_540:
		break;
	default:
		status = RET_EDP_CONFIG_INVALID_LINK_RATE;
		break;
	}

	switch (lane_count & 0x07) {
	case 1:
	case 2:
	case 4:
		break;
	default:
		status = RET_EDP_CONFIG_INVALID_LANE_COUNT;
		break;
	}

	return status;
}

static unsigned char trdp_DPCD_vswing_for_value(unsigned tx_vswing)
{
	unsigned char dpcd_value = 0xff;

	switch (tx_vswing) {
	case VAL_EDP_TX_PHY_VSWING_0:
		dpcd_value = 0;
		break;
	case VAL_EDP_TX_PHY_VSWING_1:
		dpcd_value = 1;
		break;
	case VAL_EDP_TX_PHY_VSWING_2:
		dpcd_value = 2;
		break;
	case VAL_EDP_TX_PHY_VSWING_3:
		dpcd_value = 3;
		break;
	default:
		dpcd_value = 0;
		break;
	}

	if (dpcd_value == 3) /* reach the max level flag */
		dpcd_value |= (1 << 2);

	return dpcd_value;
}

static unsigned char trdp_DPCD_preemphasis_for_value(unsigned tx_preemp,
		unsigned current_vswing)
{
	unsigned max_preemp, dpcd_value;

	switch (tx_preemp) {
	case VAL_EDP_TX_PHY_PREEMPHASIS_0:
		dpcd_value = 0;
		break;
	case VAL_EDP_TX_PHY_PREEMPHASIS_1:
		dpcd_value = 1;
		break;
	case VAL_EDP_TX_PHY_PREEMPHASIS_2:
		dpcd_value = 2;
		break;
	case VAL_EDP_TX_PHY_PREEMPHASIS_3:
		dpcd_value = 3;
		break;
	default:
		dpcd_value = 0;
		break;
	}

	switch (current_vswing) {
	case VAL_EDP_TX_PHY_VSWING_0:
		max_preemp = 3;
		break;
	case VAL_EDP_TX_PHY_VSWING_1:
		max_preemp = 2;
		break;
	case VAL_EDP_TX_PHY_VSWING_2:
		max_preemp = 1;
		break;
	case VAL_EDP_TX_PHY_VSWING_3:
		max_preemp = 0;
		break;
	default:
		max_preemp = 0;
		break;
	}

	dpcd_value = (dpcd_value > max_preemp) ? max_preemp : dpcd_value;
	if (dpcd_value == 3) /* reach the max level flag */
		dpcd_value |= (1 << 2);

	return dpcd_value;
}

static void trdp_preset_vswing(unsigned vswing)
{
	preset_vswing_tx = vswing;
	preset_vswing_rx = trdp_DPCD_vswing_for_value(preset_vswing_tx);
}

static void trdp_preset_preemphasis(unsigned preemp)
{
	preset_preemp_tx = preemp;
	preset_preemp_rx = trdp_DPCD_preemphasis_for_value(preset_preemp_tx,
				preset_vswing_tx);
}

static unsigned int trdp_select_training_settings(unsigned training_settings)
{
	unsigned int status = 0;

	switch (training_settings) {
	case 0: /* vswing 0, preemphasis 0 */
		trdp_preset_vswing(VAL_EDP_TX_PHY_VSWING_0);
		trdp_preset_preemphasis(VAL_EDP_TX_PHY_PREEMPHASIS_0);
		break;
	case 1: /* vswing 0, preemphasis 1 */
		trdp_preset_vswing(VAL_EDP_TX_PHY_VSWING_0);
		trdp_preset_preemphasis(VAL_EDP_TX_PHY_PREEMPHASIS_1);
		break;
	case 2: /* vswing 0, preemphasis 2 */
		trdp_preset_vswing(VAL_EDP_TX_PHY_VSWING_0);
		trdp_preset_preemphasis(VAL_EDP_TX_PHY_PREEMPHASIS_2);
		break;
	case 3: /* vswing 0, preemphasis 3 */
		trdp_preset_vswing(VAL_EDP_TX_PHY_VSWING_0);
		trdp_preset_preemphasis(VAL_EDP_TX_PHY_PREEMPHASIS_3);
		break;
	case 4: /* vswing 1, preemphasis 0 */
		trdp_preset_vswing(VAL_EDP_TX_PHY_VSWING_1);
		trdp_preset_preemphasis(VAL_EDP_TX_PHY_PREEMPHASIS_0);
		break;
	case 5: /* vswing 1, preemphasis 1 */
		trdp_preset_vswing(VAL_EDP_TX_PHY_VSWING_1);
		trdp_preset_preemphasis(VAL_EDP_TX_PHY_PREEMPHASIS_1);
		break;
	case 6: /* vswing 1, preemphasis 2 */
		trdp_preset_vswing(VAL_EDP_TX_PHY_VSWING_1);
		trdp_preset_preemphasis(VAL_EDP_TX_PHY_PREEMPHASIS_2);
		break;
	case 7: /* vswing 2, preemphasis 0 */
		trdp_preset_vswing(VAL_EDP_TX_PHY_VSWING_2);
		trdp_preset_preemphasis(VAL_EDP_TX_PHY_PREEMPHASIS_0);
		break;
	case 8: /* vswing 2, preemphasis 1 */
		trdp_preset_vswing(VAL_EDP_TX_PHY_VSWING_2);
		trdp_preset_preemphasis(VAL_EDP_TX_PHY_PREEMPHASIS_1);
		break;
	case 9: /* vswing 3, preemphasis 0 */
		trdp_preset_vswing(VAL_EDP_TX_PHY_VSWING_3);
		trdp_preset_preemphasis(VAL_EDP_TX_PHY_PREEMPHASIS_0);
		break;
	default:
		status = RET_EDP_TX_AUX_INVALID_PARAMETER;
		break;
	}

	return status;
}

static unsigned int trdp_set_training_values(void)
{
	unsigned int status = 0;
	struct EDP_Link_Config_s *link_config = dptx_get_link_config();
	unsigned char aux_data[4];

	if (link_config->link_update)
		return status;

	link_config->vswing = preset_vswing_tx;
	link_config->preemphasis = preset_preemp_tx;

	/* set edp phy config */
	edp_phy_config_update(preset_vswing_tx, preset_preemp_tx);
	link_config->link_update = 1;

	dptx_reg_write(EDP_TX_PHY_VOLTAGE_DIFF_LANE_0, preset_vswing_tx);
	dptx_reg_write(EDP_TX_PHY_VOLTAGE_DIFF_LANE_1, preset_vswing_tx);
	dptx_reg_write(EDP_TX_PHY_VOLTAGE_DIFF_LANE_2, preset_vswing_tx);
	dptx_reg_write(EDP_TX_PHY_VOLTAGE_DIFF_LANE_3, preset_vswing_tx);

	dptx_reg_write(EDP_TX_PHY_PRE_EMPHASIS_LANE_0, preset_preemp_tx);
	dptx_reg_write(EDP_TX_PHY_PRE_EMPHASIS_LANE_1, preset_preemp_tx);
	dptx_reg_write(EDP_TX_PHY_PRE_EMPHASIS_LANE_2, preset_preemp_tx);
	dptx_reg_write(EDP_TX_PHY_PRE_EMPHASIS_LANE_3, preset_preemp_tx);

	aux_data[0] = ((preset_preemp_rx << 3) | preset_vswing_rx);
	aux_data[1] = ((preset_preemp_rx << 3) | preset_vswing_rx);
	aux_data[2] = ((preset_preemp_rx << 3) | preset_vswing_rx);
	aux_data[3] = ((preset_preemp_rx << 3) | preset_vswing_rx);

	status = trdp_AUXWrite(EDP_DPCD_TRAINING_LANE0_SET, 4, aux_data);

	return status;
}

static unsigned int trdp_set_training_pattern(unsigned char pattern)
{
	unsigned int status = 0;

	lcd_print("%s: pattern %u\n", __func__, pattern);
	/* disable scrambling for any active test pattern */
	if (pattern)
		pattern |= (1 << 5);

	dptx_reg_write(EDP_TX_TRAINING_PATTERN_SET,
		(((unsigned)pattern) & 0x03));
	status = trdp_AUXWrite(EDP_DPCD_TRAINING_PATTERN_SET, 1, &pattern);

	return status;
}

static unsigned int trdp_run_training_adjustment(void)
{
	unsigned int status = 0;
	unsigned training_set = 0;
	unsigned char vswing_level[4], preemp_level[4];
	struct EDP_Link_Config_s *link_config = dptx_get_link_config();

	vswing_level[0] = (adj_req_lane01 & 0x03);
	preemp_level[0] = ((adj_req_lane01 >> 2) & 0x03);

	switch (vswing_level[0]) {
	case 0:
		training_set = 0;
		break;
	case 1:
		training_set = 4;
		break;
	case 2:
		training_set = 7;
		break;
	case 3:
		training_set = 9;
		break;
	default:
		break;
	}
	/* get (vswing+preemphasis) training level */
	training_set += preemp_level[0];

	status = trdp_select_training_settings(training_set);

	link_config->link_update = 0; /* need to update */
	return status;
}

static unsigned int trdp_run_clock_recovery(void)
{
	unsigned int status = 0;
	unsigned lane_count;
	unsigned char cr_done[4];
	unsigned char aux_data[6];
	unsigned char clock_recovery_done = 0;

	lane_count = dptx_reg_read(EDP_TX_LINK_COUNT_SET);
	trdp_wait(VAL_EDP_CLOCK_REC_TIMEOUT); /* wait for clock recovery */

	status = trdp_AUXRead(EDP_DPCD_STATUS_LANE_0_1, 6, aux_data);
	if (status) { /* clear cr_done flags on failure of AUX transaction */
		cr_done[0] = 0;
		cr_done[1] = 0;
		cr_done[2] = 0;
		cr_done[3] = 0;
		status = RET_EDP_TX_AUX_OPERATION_FAILED;
	} else {
		adj_req_lane01 = aux_data[4];
		adj_req_lane23 = aux_data[5];

		cr_done[0] = ((aux_data[0] >>
				BIT_EDP_LANE_0_STATUS_CLK_REC_DONE) & 0x01);
		cr_done[1] = ((aux_data[0] >>
				BIT_EDP_LANE_1_STATUS_CLK_REC_DONE) & 0x01);
		cr_done[2] = ((aux_data[1] >>
				BIT_EDP_LANE_2_STATUS_CLK_REC_DONE) & 0x01);
		cr_done[3] = ((aux_data[1] >>
				BIT_EDP_LANE_3_STATUS_CLK_REC_DONE) & 0x01);

		clock_recovery_done = cr_done[0] + cr_done[1] +
					cr_done[2] + cr_done[3];
		if (clock_recovery_done == lane_count)
			status = RET_EDP_TX_OPERATION_SUCCESS;
		else
			status = RET_EDP_TX_OPERATION_FAILED;
	}

	return status;
}

static unsigned int trdp_run_channel_equalization(void)
{
	unsigned int status = 0;
	unsigned lane_count, cr_valid;
	unsigned channel_equalization_done, symbol_lock_done, lane_align_done;
	unsigned char chan_eq_done[4], sym_lock_done[4], aux_data[6];

	lane_count = dptx_reg_read(EDP_TX_LINK_COUNT_SET);
	trdp_wait(VAL_EDP_CHAN_EQ_TIMEOUT);

	status = trdp_AUXRead(EDP_DPCD_STATUS_LANE_0_1, 6, aux_data);
	if (status) {
		status = RET_EDP_TX_AUX_OPERATION_FAILED;
	} else {
		adj_req_lane01 = aux_data[4];
		adj_req_lane23 = aux_data[5];

		cr_valid = ((aux_data[0] >>
				BIT_EDP_LANE_0_STATUS_CLK_REC_DONE) & 0x01) +
			((aux_data[0] >>
				BIT_EDP_LANE_1_STATUS_CLK_REC_DONE) & 0x01) +
			((aux_data[1] >>
				BIT_EDP_LANE_2_STATUS_CLK_REC_DONE) & 0x01) +
			((aux_data[1] >>
				BIT_EDP_LANE_3_STATUS_CLK_REC_DONE) & 0x01);

		if (cr_valid != lane_count) {
			status = RET_EDP_TRAINING_CR_FAILED;
			DPRINT("%s: training CR failed\n", __func__);
		} else {
			chan_eq_done[0] = ((aux_data[0] >>
				BIT_EDP_LANE_0_STATUS_CHAN_EQ_DONE) & 0x01);
			chan_eq_done[1] = ((aux_data[0] >>
				BIT_EDP_LANE_1_STATUS_CHAN_EQ_DONE) & 0x01);
			chan_eq_done[2] = ((aux_data[1] >>
				BIT_EDP_LANE_2_STATUS_CHAN_EQ_DONE) & 0x01);
			chan_eq_done[3] = ((aux_data[1] >>
				BIT_EDP_LANE_3_STATUS_CHAN_EQ_DONE) & 0x01);

			sym_lock_done[0] = ((aux_data[0] >>
				BIT_EDP_LANE_0_STATUS_SYM_LOCK_DONE) & 0x01);
			sym_lock_done[1] = ((aux_data[0] >>
				BIT_EDP_LANE_1_STATUS_SYM_LOCK_DONE) & 0x01);
			sym_lock_done[2] = ((aux_data[1] >>
				BIT_EDP_LANE_2_STATUS_SYM_LOCK_DONE) & 0x01);
			sym_lock_done[3] = ((aux_data[1] >>
				BIT_EDP_LANE_3_STATUS_SYM_LOCK_DONE) & 0x01);

			channel_equalization_done = chan_eq_done[0] +
							chan_eq_done[1] +
							chan_eq_done[2] +
							chan_eq_done[3];
			symbol_lock_done = sym_lock_done[0] + sym_lock_done[1] +
					sym_lock_done[2] + sym_lock_done[3];

			lane_align_done = ((aux_data[2] >>
				BIT_EDP_LANE_ALIGNMENT_DONE) & 0x01);

			if ((channel_equalization_done == lane_count) &&
				(symbol_lock_done == lane_count) &&
				(lane_align_done == 1)) {
				status = RET_EDP_TX_OPERATION_SUCCESS;
			} else {
				status = RET_EDP_TX_OPERATION_FAILED;
			}
		}
	}

	return status;
}

static unsigned int trdp_run_clock_recovery_loop(unsigned max_iterations,
		unsigned adaptive)
{
	unsigned int status = 0;
	unsigned xx = 0;
	unsigned done = 0;

	/* set training pattern */
	trdp_set_training_pattern(VAL_EDP_TRAINING_PATTERN_1);

	while (!done) {
		if (adaptive == 1) {
			/* set lane vswing & preemphasis */
			trdp_set_training_values();
		}

		status = trdp_run_clock_recovery();
		switch (status) {
		case RET_EDP_TX_OPERATION_SUCCESS:
			done = 1;
			break;
		case RET_EDP_TX_OPERATION_FAILED:
			if (adaptive == 1) {
				/* check for max vswing level */
				if ((preset_vswing_rx & 0x07) != 0x07)
					trdp_run_training_adjustment();
				else
					done = 1;
			}
			break;
		case RET_EDP_TX_AUX_OPERATION_FAILED:
			done = 1;
			break;
		default:
			break;
		}
		if (++xx == max_iterations)
			done = 1;
	}
	return status;
}

static unsigned int trdp_run_channel_equalization_loop(unsigned max_iterations,
		unsigned adaptive)
{
	unsigned int status = 0;
	unsigned xx = 0;
	unsigned done = 0;

	trdp_set_training_pattern(VAL_EDP_TRAINING_PATTERN_2);

	/* channel equalization & symbol lock loop */
	while (!done) {
		if (adaptive == 1)
			trdp_set_training_values();

		status = trdp_run_channel_equalization();
		switch (status) {
		case RET_EDP_TRAINING_CR_FAILED:
			done = 1;
			break;
		case RET_EDP_TX_OPERATION_SUCCESS:
			done = 1;
			break;
		case RET_EDP_TX_AUX_OPERATION_FAILED:
			done = 1;
			break;
		case RET_EDP_TX_OPERATION_FAILED:
			if (adaptive == 1)
				trdp_run_training_adjustment();
			break;
		default:
			break;
		}
		if (xx++ == max_iterations) /* execute 6 times */
			done = 1;
	}

	return status;
}

static unsigned int trdp_update_status(void)
{
	unsigned int status = 0;
	unsigned char aux_data[3];

	status = trdp_AUXRead(EDP_DPCD_STATUS_LANE_0_1, 3, aux_data);
	lcd_print("%s: aux_data0=0x%x, aux_data1=0x%x, aux_data2=0x%x,\n",
		__func__, aux_data[0], aux_data[1], aux_data[2]);
	if (status == RET_EDP_TX_OPERATION_SUCCESS) {
		status = (aux_data[2] << 16) | (aux_data[1] << 8) |
			(aux_data[0] << 0);
	}

	return status;
}

static unsigned int trdp_run_training_loop(unsigned training_settings,
		unsigned link_rate_adjust_en, unsigned adaptive,
		unsigned retry_num)
{
	unsigned int status = 0;
	unsigned lanes = 0;
	unsigned char aux_data[4];
	unsigned done = 0, link_rate_adjust = 0;
	unsigned training_state = STA_EDP_TRAINING_CLOCK_REC;
	unsigned link_speed;

	memset(aux_data, 0, 4);
	status = trdp_verify_training_config();
	if (status)
		return status;

	adj_req_lane01 = 0;
	adj_req_lane23 = 0;
	trdp_select_training_settings(training_settings);

	adaptive = (adaptive > 1) ? 1 : adaptive;

	/* trun off scrambling for training */
	dptx_reg_write(EDP_TX_SCRAMBLING_DISABLE, 0x01);

	/* enter training loop */
	while (!done) {
		switch (training_state) {
			/* ************************************** */
			/* the initial state preforms clock recovery. */
			/* if successful, the training sequence moves on to
			     symbol lock. */
			/* if clock recovery fails, the training loop exits. */
			/* ************************************** */
		case STA_EDP_TRAINING_CLOCK_REC: /* clock recovery loop */
			status = trdp_run_clock_recovery_loop(
				VAL_EDP_MAX_TRAINING_ATTEMPTS, adaptive);
			if (status == RET_EDP_TX_OPERATION_SUCCESS) {
				training_state = STA_EDP_TRAINING_CHANNEL_EQ;
			} else if (status == RET_EDP_TX_AUX_OPERATION_FAILED) {
				DPRINT("trdp_run_clock_recovery_loop");
				DPRINT("AUX failed\n");
				done = 1;
			} else {
				DPRINT("trdp_run_clock_recovery_loop");
				DPRINT("CR failed\n");
				status = RET_EDP_TRAINING_CR_FAILED;
				if (link_rate_adjust_en == 1) {
					training_state =
						STA_EDP_TRAINING_ADJUST_SPD;
				} else {
					done = 1;
				}
			}
			break;

		/* ************************************** */
		/* once clock recovery is complete, perform symbol lock and
		       channel equalization. */
		/* if this state fails, then we can adjust the link rate. */
		/* ************************************** */
		case STA_EDP_TRAINING_CHANNEL_EQ:
			status = trdp_run_channel_equalization_loop(
				VAL_EDP_MAX_TRAINING_ATTEMPTS, adaptive);
			if (status == RET_EDP_TX_OPERATION_SUCCESS) {
				done = 1;
			} else if (status == RET_EDP_TX_AUX_OPERATION_FAILED) {
				DPRINT("trdp_run_channel_equalization_loop");
				DPRINT("AUX failed\n");
				done = 1;
			} else {
				DPRINT("trdp_run_channel_equalization_loop");
				DPRINT("EQ failed\n");
				status = RET_EDP_TRAINING_CHAN_EQ_FAILED;
				if (link_rate_adjust_en == 1) {
					training_state =
						STA_EDP_TRAINING_ADJUST_SPD;
				} else {
					done = 1;
				}
			}
			break;

		/* ************************************** */
		/* when allowed by the function parameter, adjust the
		     link speed and attempt to retrain the link starting
		     with clock recovery. */
		/* the state of the status variable should not be changed
		     in this state allowing a failure condition to report
		     the proper status. */
		/* ************************************** */
		case STA_EDP_TRAINING_ADJUST_SPD:
			if (retry_num == 1) {
				link_speed = dptx_reg_read(EDP_TX_LINK_BW_SET);
				lanes = dptx_reg_read(EDP_TX_LINK_COUNT_SET);
				if (link_speed != VAL_EDP_TX_LINK_BW_SET_162) {
					if (link_speed ==
						VAL_EDP_TX_LINK_BW_SET_270) {
						link_speed =
						VAL_EDP_TX_LINK_BW_SET_162;
					} else {
						link_speed =
						VAL_EDP_TX_LINK_BW_SET_270;
					}
					DPRINT("[warning]:");
					DPRINT("reduce edp link rate\n");
					trdp_edp_link_rate_update(link_speed);
					link_rate_adjust = 1;
				}
			}
			done = 1;
			break;
		}
	}

	trdp_set_training_pattern(VAL_EDP_TRAINING_PATTERN_OFF);
	/* turn on scrambling after training */
	dptx_reg_write(EDP_TX_SCRAMBLING_DISABLE, 0x00);

	/* if (status == RET_EDP_TX_OPERATION_SUCCESS) */
	if (link_rate_adjust)
		status = RET_EDP_LPM_STATUS_LINK_RATE_ADJUST;
	else
		status = trdp_update_status();

	return status;
}

static unsigned int trdp_set_link_config(unsigned char link_rate,
		unsigned char lane_count)
{
	unsigned char aux_data[2];
	unsigned enhance_framing_mode;
	unsigned int status = 0;

	switch (link_rate) {
	case VAL_EDP_TX_LINK_BW_SET_162:
	case VAL_EDP_TX_LINK_BW_SET_270:
		aux_data[0] = link_rate;
		break;
	default:
		status = RET_EDP_TX_AUX_INVALID_PARAMETER;
		break;
	}
	switch (lane_count) {
	case 1:
	case 2:
	case 4:
		enhance_framing_mode =
			(dptx_reg_read(EDP_TX_ENHANCED_FRAME_EN) & 0x01);
		if (enhance_framing_mode)
			lane_count |= (1 << 7);
		aux_data[1] = lane_count;
		break;
	default:
		status = RET_EDP_TX_AUX_INVALID_PARAMETER;
		break;
	}
	if (status == 0) {
		status = trdp_AUXWrite(EDP_DPCD_LINK_BANDWIDTH_SET,
				2, aux_data);
		if (status)
			DPRINT("set lane config failed\n");
	} else {
		DPRINT("error parameters\n");
	}
	return status;
}

static unsigned int trdp_set_data_lane_config(
		struct EDP_Link_Config_s *link_config)
{
	unsigned int status = 0;
	unsigned char aux_data[4];
	unsigned vswing = link_config->vswing & 0xff;
	unsigned preemphasis = link_config->preemphasis & 0xff;
	unsigned char vswing_rx, preemp_rx;

	/* preset voltage swing levels */
	switch (vswing) {
	case VAL_EDP_TX_PHY_VSWING_0:
	case VAL_EDP_TX_PHY_VSWING_1:
	case VAL_EDP_TX_PHY_VSWING_2:
	case VAL_EDP_TX_PHY_VSWING_3:
		dptx_reg_write(EDP_TX_PHY_VOLTAGE_DIFF_LANE_0, vswing);
		dptx_reg_write(EDP_TX_PHY_VOLTAGE_DIFF_LANE_1, vswing);
		dptx_reg_write(EDP_TX_PHY_VOLTAGE_DIFF_LANE_2, vswing);
		dptx_reg_write(EDP_TX_PHY_VOLTAGE_DIFF_LANE_3, vswing);
		break;
	default:
		status = RET_EDP_TX_OPERATION_FAILED;
		break;
	}

	/* preset preemphasis */
	switch (preemphasis) {
	case VAL_EDP_TX_PHY_PREEMPHASIS_0:
	case VAL_EDP_TX_PHY_PREEMPHASIS_1:
	case VAL_EDP_TX_PHY_PREEMPHASIS_2:
	case VAL_EDP_TX_PHY_PREEMPHASIS_3:
		dptx_reg_write(EDP_TX_PHY_PRE_EMPHASIS_LANE_0, preemphasis);
		dptx_reg_write(EDP_TX_PHY_PRE_EMPHASIS_LANE_1, preemphasis);
		dptx_reg_write(EDP_TX_PHY_PRE_EMPHASIS_LANE_2, preemphasis);
		dptx_reg_write(EDP_TX_PHY_PRE_EMPHASIS_LANE_3, preemphasis);
		break;
	default:
		status = RET_EDP_TX_OPERATION_FAILED;
		break;
	}

	/* write the preset values to the sink device */
	vswing_rx = trdp_DPCD_vswing_for_value(vswing);
	preemp_rx = trdp_DPCD_preemphasis_for_value(preemphasis, vswing);
	if (status == RET_EDP_TX_OPERATION_SUCCESS) {
		aux_data[0] = (preemp_rx << 3) | vswing_rx;
		aux_data[1] = (preemp_rx << 3) | vswing_rx;
		aux_data[2] = (preemp_rx << 3) | vswing_rx;
		aux_data[3] = (preemp_rx << 3) | vswing_rx;
		status = trdp_AUXWrite(EDP_DPCD_TRAINING_LANE0_SET,
				4, aux_data);
	}

	link_config->link_update = 1;

	return status;
}

static void dplpm_print_training_status(int status)
{
	char status_string[80];

	switch (status) {
	case RET_EDP_TRAINING_CR_FAILED:
		sprintf(status_string, "Clock Recovery failed");
		break;
	case RET_EDP_TRAINING_CHAN_EQ_FAILED:
		sprintf(status_string, "Symbol Lock failed");
		break;
	case RET_EDP_TX_AUX_OPERATION_FAILED:
		sprintf(status_string, "AUX operation failure during training");
		break;
	case RET_EDP_CONFIG_INVALID_LINK_RATE:
		sprintf(status_string, "Invalid link rate config");
		break;
	case RET_EDP_CONFIG_INVALID_LANE_COUNT:
		sprintf(status_string, "Invalid lane count config");
		break;
	case RET_EDP_CONFIG_HPD_DEASSERTED:
		sprintf(status_string, "HPD deasserted");
		break;
	case RET_EDP_TX_OPERATION_SUCCESS:
		sprintf(status_string, "Success");
		break;
	case RET_EDP_LPM_STATUS_RETRAIN:
		sprintf(status_string, "Retrain");
		break;
	default:
		sprintf(status_string, "Error 0x%08x", status);
		break;
	}
	DPRINT("displayport training: %s\n", status_string);
}

static void dplpm_main_stream_enable(unsigned enable)
{
	struct EDP_Link_Config_s *link_config = dptx_get_link_config();

	enable = (enable > 0) ? 1 : 0;
	link_config->main_stream_enable = enable;

	if (enable) {
		dptx_reg_write(EDP_TX_FORCE_SCRAMBLER_RESET, 1);
		dptx_reg_write(EDP_TX_MAIN_STREAM_ENABLE, 1);
	} else
		dptx_reg_write(EDP_TX_MAIN_STREAM_ENABLE, 0);
	lcd_print("displayport main stream %s\n", enable ?
		"enable" : "disable");
}

static unsigned int dplpm_verify_link_status(void)
{
	unsigned int status = 0;
	unsigned link_ok = 0;
	unsigned char aux_data[8];

	status = trdp_AUXRead(EDP_DPCD_SINK_COUNT, 8, aux_data);
	if (!status) {
		status = (((aux_data[4] & 0x01) << 16) | (aux_data[3] << 8) |
			aux_data[2]);
	}

	switch (dptx_reg_read(EDP_TX_LINK_COUNT_SET)) {
	case 0: /* Lane count not set */
		link_ok = VAL_EDP_TX_LANE_STATUS_OK_NONE;
		break;
	case 1:
		link_ok = VAL_EDP_TX_LANE_STATUS_OK_1;
		break;
	case 2:
		link_ok = VAL_EDP_TX_LANE_STATUS_OK_2;
		break;
	case 4:
		link_ok = VAL_EDP_TX_LANE_STATUS_OK_4;
		break;
	}

	if ((status & link_ok) == link_ok)
		status = RET_EDP_LPM_STATUS_TRAINING_SUCCESS;
	else
		status = RET_EDP_LPM_STATUS_RETRAIN;

	lcd_print("%s: %s\n", __func__,
		(status == RET_EDP_LPM_STATUS_TRAINING_SUCCESS) ?
		"training success" : "retrain");
	return status;
}

static unsigned int dplpm_maintain_link(void)
{
	unsigned training_attempts = VAL_EDP_MAX_TRAINING_ATTEMPTS;
	unsigned training_successful = 0, retrain = 0;
	unsigned int status = RET_EDP_LPM_STATUS_RETRAIN;
	struct EDP_Link_Config_s *link_config = dptx_get_link_config();

	/* status = dplpm_verify_link_status(); */
	if (status == RET_EDP_LPM_STATUS_RETRAIN) {
		dplpm_main_stream_enable(0);
		retrain = 1;
	}

	if (retrain == 1) {
		while ((training_attempts > 0) && (training_successful != 1)) {
			status = trdp_run_training_loop(
					link_config->training_settings,
					link_config->link_rate_adjust_en,
					link_config->link_adaptive,
					training_attempts);
			status = dplpm_verify_link_status();
			if (status == RET_EDP_TX_OPERATION_SUCCESS) {
				training_successful = 1;
				/* dplpm_main_stream_enable(1); */
			} else {
				training_attempts--;
				link_config->link_update = 0;
			}
		}
	} else {
		dplpm_main_stream_enable(1);
	}

	dplpm_print_training_status(status);
	return status;
}

static unsigned int dplpm_link_init(struct EDP_Link_Config_s *link_config)
{
	unsigned int status = 0;
	unsigned char link_rate = link_config->link_rate & 0xff;
	unsigned char lane_count = link_config->lane_count & 0x7;
	unsigned char ss_enable = link_config->ss_enable;
	unsigned char power_state;
	unsigned core_id;

	core_id = (dptx_reg_read(EDP_TX_CORE_ID) >> 16);
	if (core_id == VAL_EDP_TX_CORE_ID) {
		lcd_reg_write(ENCL_VIDEO_EN, 0);
		mdelay(10);
		/* disable the transmitter */
		dptx_reg_write(EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 0);
		/* reset the PHY */
		dptx_reg_write(EDP_TX_PHY_RESET, 0xf);

		/* reset edp tx fifo */
		lcd_reg_setb(RESET4_MASK, 0, 11, 1);
		lcd_reg_setb(RESET4_REGISTER, 1, 11, 1);
		lcd_reg_setb(RESET4_MASK, 1, 11, 1);
		lcd_print("reset edp tx\n");
		mdelay(10);
		lcd_reg_setb(RESET4_MASK, 0, 11, 1);
		lcd_reg_setb(RESET4_REGISTER, 0, 11, 1);
		lcd_reg_setb(RESET4_MASK, 1, 11, 1);
		lcd_print("release reset edp tx\n");

		/* Set Aux clk-div by APB clk */
		dptx_reg_write(EDP_TX_AUX_CLOCK_DIVIDER, 170);

		status = dptx_init_lane_config(link_rate, lane_count);
		status = dptx_init_downspread(ss_enable);

		mdelay(10);
		dptx_reg_write(EDP_TX_PHY_RESET, 0);
		dptx_wait_phy_ready();
		mdelay(10);
		dptx_reg_write(EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 1);
		/* mask the interrupt, use polling mode */
		dptx_reg_write(EDP_TX_AUX_INTERRUPT_MASK, 0xf);

		status = trdp_set_link_config(link_rate, lane_count);
		if (status)
			return RET_EDP_TX_OPERATION_FAILED;

		status = trdp_set_downspread(ss_enable);
		if (status)
			return RET_EDP_TX_OPERATION_FAILED;

		/* set vswing & preemphasis */
		status = trdp_set_data_lane_config(link_config);
		if (status)
			return RET_EDP_TX_OPERATION_FAILED;

		lcd_print("..... Power up sink link .....\n");
		power_state = 1; /* normal operation */
		status = trdp_AUXWrite(EDP_DPCD_SET_POWER, 1, &power_state);
		mdelay(30);
		if (status)
			return RET_EDP_TX_OPERATION_FAILED;
	} else {
		DPRINT("Can't find eDP Core ID\n");
		return RET_EDP_TX_OPERATION_FAILED;
	}
	return status;
}

static unsigned int edp_link_on(struct EDP_MSA_s *vm)
{
	unsigned int status = 0;
	struct EDP_Link_Config_s *link_config = dptx_get_link_config();

	status = dplpm_link_init(link_config);

	if (status == RET_EDP_TX_OPERATION_SUCCESS) {
		status = dplpm_maintain_link(); /* link_policy_maker */
		if (lcd_print_flag > 0) {
			trdp_dump_EDID();
			trdp_dump_DPCD();
			trdp_dump_DPCD_training_status();
		}
		dptx_set_MSA(vm);
		lcd_reg_write(ENCL_VIDEO_EN, 1);
		dplpm_main_stream_enable(1);
	} else {
		DPRINT("displayport initial failed\n");
		status = RET_EDP_TX_OPERATION_FAILED;
	}
	if (lcd_print_flag > 0) {
		dptx_dump_link_config();
		dptx_dump_MSA();
	}

	return status;
}

unsigned int edp_link_off(void)
{
	unsigned int status = 0;
	unsigned char aux_data;

	lcd_print("..... Power down edp sink link .....\n");
	aux_data = 2; /* power down mode */
	status = trdp_AUXWrite(EDP_DPCD_SET_POWER, 1, &aux_data);

	return status;
}

unsigned char get_edp_config_index(const unsigned char *config_table,
		unsigned char config_value)
{
	unsigned char index = 0;

	while (index < 5) {
		if ((config_value == config_table[index]) ||
			(config_table[index] == VAL_EDP_TX_INVALID_VALUE)) {
			break;
		}
		index++;
	}
	return index;
}

unsigned int edp_host_on(struct Lcd_Config_s *pConf)
{
	unsigned int status = 0;
	struct EDP_MSA_s  vm;
	struct EDP_Link_Config_s *lconf = dptx_get_link_config();
	struct EDP_Config_s *econf;
	unsigned char temp;

	econf = pConf->lcd_control.edp_config;

	lconf->max_lane_count = 4;
	lconf->max_link_rate = VAL_EDP_TX_LINK_BW_SET_270;
	lconf->lane_count = econf->lane_count;
	temp = (unsigned char)(pConf->lcd_timing.clk_ctrl >> CLK_CTRL_SS) & 0xf;
	lconf->ss_enable = (temp > 0) ? 1 : 0;
	lconf->link_adaptive = econf->link_adaptive;
	lconf->bit_rate = econf->bit_rate;
	lconf->link_rate = edp_link_rate_table[econf->link_rate];
	lconf->vswing = edp_vswing_table[econf->vswing];
	lconf->preemphasis = edp_preemphasis_table[econf->preemphasis];

	/* edp main stream attribute */
	vm.h_active = pConf->lcd_basic.h_active;
	vm.v_active = pConf->lcd_basic.v_active;
	vm.h_period = pConf->lcd_basic.h_period;
	vm.v_period = pConf->lcd_basic.v_period;
	vm.clk = pConf->lcd_timing.lcd_clk;
	vm.hsync_pol = (pConf->lcd_timing.pol_ctrl >> POL_CTRL_HS) & 1;
	vm.hsync_width = pConf->lcd_timing.hsync_width;
	vm.hsync_bp = pConf->lcd_timing.hsync_bp;
	vm.vsync_pol = (pConf->lcd_timing.pol_ctrl >> POL_CTRL_VS) & 1;
	vm.vsync_width = pConf->lcd_timing.vsync_width;
	vm.vsync_bp = pConf->lcd_timing.vsync_bp;
	vm.ppc = 1;     /* pixels per clock cycle */
	vm.cformat = 0; /* color format(0=RGB, 1=4:2:2, 2=Y only) */
	vm.bpc = pConf->lcd_basic.lcd_bits; /* bits per color */
	vm.sync_clock_mode = econf->sync_clock_mode & 1;

	status = edp_link_on(&vm);

	/* feedback link config to lcd driver */
	econf->lane_count = lconf->lane_count;
	econf->bit_rate = lconf->bit_rate;
	econf->link_rate = get_edp_config_index(&edp_link_rate_table[0],
				lconf->link_rate);
	econf->vswing = get_edp_config_index(&edp_vswing_table[0],
				lconf->vswing);
	econf->preemphasis = get_edp_config_index(&edp_preemphasis_table[0],
				lconf->preemphasis);

	lcd_print("%s finish\n", __func__);
	return status;
}

void edp_host_off(void)
{
	lcd_reg_write(ENCL_VIDEO_EN, 0);
	mdelay(10);
	dplpm_main_stream_enable(0);

	/* disable the transmitter */
	dptx_reg_write(EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 0);
	dptx_reg_write(EDP_TX_PHY_RESET, 0xf); /* reset the PHY */
	dptx_reg_write(EDP_TX_PHY_POWER_DOWN, 0xf); /* need to set */

	/* mdelay(100); */
	lcd_reg_setb(RESET4_MASK, 0, 11, 1);
	lcd_reg_setb(RESET4_REGISTER, 1, 11, 1);
	lcd_reg_setb(RESET4_MASK, 1, 11, 1);
	lcd_print("%s finish\n", __func__);
}

void edp_edid_pre_enable(void)
{
	/* disable the transmitter */
	dptx_reg_write(EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 0);
	dptx_reg_write(EDP_TX_PHY_RESET, 0xf); /* reset the PHY */

	/* reset edp tx fifo */
	lcd_reg_setb(RESET4_MASK, 0, 11, 1);
	lcd_reg_setb(RESET4_REGISTER, 1, 11, 1);
	lcd_reg_setb(RESET4_MASK, 1, 11, 1);
	mdelay(10);
	lcd_reg_setb(RESET4_MASK, 0, 11, 1);
	lcd_reg_setb(RESET4_REGISTER, 0, 11, 1);
	lcd_reg_setb(RESET4_MASK, 1, 11, 1);

	/* Set Aux clk-div by APB clk */
	dptx_reg_write(EDP_TX_AUX_CLOCK_DIVIDER, 170);

	mdelay(10);
	dptx_reg_write(EDP_TX_PHY_RESET, 0);
	dptx_wait_phy_ready();
	mdelay(10);
	dptx_reg_write(EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 1);
	/* mask the interrupt, use polling mode */
	dptx_reg_write(EDP_TX_AUX_INTERRUPT_MASK, 0xf);
}

void edp_edid_pre_disable(void)
{
	/* disable the transmitter */
	dptx_reg_write(EDP_TX_TRANSMITTER_OUTPUT_ENABLE, 0);
	dptx_reg_write(EDP_TX_PHY_RESET, 0xf); /* reset the PHY */
	dptx_reg_write(EDP_TX_PHY_POWER_DOWN, 0xf); /* need to set */

	lcd_reg_setb(RESET4_MASK, 0, 11, 1);
	lcd_reg_setb(RESET4_REGISTER, 1, 11, 1);
	lcd_reg_setb(RESET4_MASK, 1, 11, 1);
}

void edp_link_config_select(struct Lcd_Config_s *pConf)
{
	unsigned bit_rate, band_width;
	unsigned i, j, done = 0;
	struct EDP_Config_s *econf;

	econf = pConf->lcd_control.edp_config;
	bit_rate = (pConf->lcd_timing.lcd_clk / 1000) *
		pConf->lcd_basic.lcd_bits * 3 / 1000;  /* Mbps */
	econf->bit_rate = bit_rate;

	if (econf->link_user == 0) {/* auto calculate */
		i = 0;
		while ((edp_lane_count_table[i] <= econf->max_lane_count) &&
			(done == 0)) {
			for (j = 0; j <= 1; j++) {
				band_width = edp_link_capacity_table[j] *
					edp_lane_count_table[i];
				if (band_width > bit_rate) {
					econf->link_rate = j;
					econf->lane_count =
						edp_lane_count_table[i];
					done = 1;
					break;
				}
			}
			if (done == 0)
				i++;
		}
		if (done == 0) {
			econf->link_rate = 1;
			econf->lane_count = econf->max_lane_count;
			DPRINT("Error: bit_rate is out of support, ");
			DPRINT("should reduce frame rate(pixel clock)\n");
		} else {
			DPRINT("select edp link_rate=%s, lane_count=%u\n",
				edp_link_rate_string_table[econf->link_rate],
				econf->lane_count);
		}
	} else { /* verify user define */
		i = get_edp_config_index(&edp_lane_count_table[0],
			econf->lane_count);
		while ((edp_lane_count_table[i] <= econf->max_lane_count) &&
			(done == 0)) {
			band_width = edp_link_capacity_table[econf->link_rate] *
				edp_lane_count_table[i];
			if (band_width > bit_rate) {
				econf->lane_count = edp_lane_count_table[i];
				done = 1;
			} else {
				i++;
			}
		}
		if (done == 0) {
			econf->lane_count = econf->max_lane_count;
			DPRINT("Error: bandwidth is not enough at ");
			DPRINT("link_rate=%s, lane_count=%d\n",
				edp_link_rate_string_table[econf->link_rate],
				econf->lane_count);
		} else {
			DPRINT("set edp link_rate=%s, lane_count=%u\n",
				edp_link_rate_string_table[econf->link_rate],
				econf->lane_count);
		}
	}
}

/* *********************************************** */

static const char *edp_usage_str = {
"Usage:\n"
"    echo read tx <addr> <reg_count> > edp ; read edp tx reg value\n"
"    echo write <value> tx <addr> > edp ; write edp tx reg with value\n"
"    echo read rx <addr> <reg_count> > edp ; read edp DPCD reg value\n"
"    echo write <value> rx <addr> > edp ; write edp DPCD reg with value\n"
"    echo link > edp ; print edp link config information\n"
"    echo msa > edp ; print edp main stream attributes information\n"
"    echo dpcd > edp ; print edp DPCD information\n"
"    echo edid > edp ; print edp EDID information\n"
"    echo status > edp ; print edp link training status information\n"
};

static ssize_t edp_debug_help(struct class *class,
		struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", edp_usage_str);
}

static ssize_t edp_debug(struct class *class,
		struct class_attribute *attr, const char *buf, size_t count)
{
	unsigned int ret;
	unsigned t[3];
	unsigned char s[8];
	unsigned status = 0;
	unsigned char aux_data[16];
	unsigned num = 0;
	int i;

	if (lcd_chip_valid_check() == FALSE)
		return -EINVAL;
	switch (buf[0]) {
	case 'r': /* read */
		num = 1;
		ret = sscanf(buf, "read %s %x %u", s, &t[0], &num);
		if (s[0] == 't') { /* tx */
			DPRINT("read edp tx reg:\n");
			for (i = 0; i < num; i++) {
				DPRINT("  0x%04x = 0x%08x\n",
					t[0]+i, dptx_reg_read(t[0]+i*4));
			}
		} else if (s[0] == 'r') { /* rx -- aux --dpcd */
			num = (num < 16) ? num : 16;
			status = trdp_AUXRead(t[0], num, aux_data);
			if (status == RET_EDP_TX_OPERATION_SUCCESS) {
				DPRINT("read edp DPCD reg:\n");
				for (i = 0; i < num; i++) {
					DPRINT("  0x%04x = 0x%02x\n",
						t[0]+i, aux_data[i]);
				}
			} else {
				DPRINT("Failed to read DPCD regs\n");
			}
		}
		break;
	case 'w': /* write */
		ret = sscanf(buf, "write %x %s %x", &t[0], s, &t[1]);
		if (s[0] == 't') { /* tx */
			dptx_reg_write(t[1], t[0]);
			DPRINT("write tx reg 0x%04x = 0x%08x, ", t[1], t[0]);
			DPRINT("readback 0x%08x\n", dptx_reg_read(t[1]));
		} else if (s[0] == 'r') { /* rx -- aux --dpcd */
			aux_data[0] = t[0];
			status = trdp_AUXWrite(t[1], 1, aux_data);
			if (status == RET_EDP_TX_OPERATION_SUCCESS) {
				status = trdp_AUXRead(t[1], 1, aux_data);
				if (status != RET_EDP_TX_OPERATION_SUCCESS) {
					DPRINT("Failed to readback DPCD reg\n");
					aux_data[0] = 0;
				}
				DPRINT("write DPCD reg 0x%04x = 0x%02x, ",
					t[1], t[0]);
				DPRINT("readback 0x%02x\n", aux_data[0]);
			} else {
				DPRINT("Failed to write DPCD reg\n");
			}
		}
		break;
	case 't':
		if (buf[1] == 'c') { /* tc */
			DPRINT("Training clock recovery\n");
			dptx_reg_write(EDP_TX_SCRAMBLING_DISABLE, 0x01);
			trdp_run_clock_recovery_loop(5, 0);
			trdp_set_training_pattern(VAL_EDP_TRAINING_PATTERN_OFF);
			dptx_reg_write(EDP_TX_SCRAMBLING_DISABLE, 0x00);
		} else { /* te */
			DPRINT("Training channel equalization\n");
			dptx_reg_write(EDP_TX_SCRAMBLING_DISABLE, 0x01);
			trdp_run_channel_equalization_loop(5, 0);
			trdp_set_training_pattern(VAL_EDP_TRAINING_PATTERN_OFF);
			dptx_reg_write(EDP_TX_SCRAMBLING_DISABLE, 0x00);
		}
		break;
	case 'l':
		dptx_dump_link_config();
		break;
	case 'm':
		dptx_dump_MSA();
		break;
	case 'd':
		trdp_dump_DPCD();
		break;
	case 'e':
		trdp_dump_EDID();
		break;
	case 's':
		trdp_dump_DPCD_training_status();
		break;
	default:
		DPRINT("wrong format of edp debug command\n");
		break;
	}

	if (ret != 1 || ret != 2)
		return -EINVAL;

	return count;
	/* return 0; */
}

static struct class_attribute edp_debug_class_attrs[] = {
	__ATTR(edp, S_IRUGO | S_IWUSR, edp_debug_help, edp_debug),
};

int creat_edp_attr(struct class *debug_class)
{
	int i;

	/* create class attr */
	for (i = 0; i < ARRAY_SIZE(edp_debug_class_attrs); i++) {
		if (class_create_file(debug_class, &edp_debug_class_attrs[i])) {
			DPRINT("create edp debug attribute %s fail\n",
				edp_debug_class_attrs[i].attr.name);
		}
	}

	return 0;
}
int remove_edp_attr(struct class *debug_class)
{
	int i;

	if (debug_class == NULL)
		return -1;

	for (i = 0; i < ARRAY_SIZE(edp_debug_class_attrs); i++)
		class_remove_file(debug_class, &edp_debug_class_attrs[i]);

	return 0;
}
/* ********************************************************* */
static void edp_edid_timing_config(struct Lcd_Config_s *pConf,
		struct EDP_EDID_Data_Type_s *edid_parse)
{
	unsigned int temp;

	pConf->lcd_basic.h_active = edid_parse->preferred_timing.h_active;
	pConf->lcd_basic.v_active = edid_parse->preferred_timing.v_active;
	if (edid_parse->preferred_timing.h_blank > 120) {
		pConf->lcd_basic.h_period =
			edid_parse->preferred_timing.h_active +
			edid_parse->preferred_timing.h_blank;
	} else {
		pConf->lcd_basic.h_period =
			edid_parse->preferred_timing.h_active + 121;
	}
	if (edid_parse->preferred_timing.v_blank >= 22) {
		pConf->lcd_basic.v_period =
			edid_parse->preferred_timing.v_active +
			edid_parse->preferred_timing.v_blank;
	} else {
		pConf->lcd_basic.v_period =
			edid_parse->preferred_timing.v_active + 22;
	}

	temp = edid_parse->preferred_timing.pclk /
		(edid_parse->preferred_timing.h_active +
		edid_parse->preferred_timing.h_blank);
	temp = temp * 100 / (edid_parse->preferred_timing.v_active +
		edid_parse->preferred_timing.v_blank);
	temp = (temp + 5) / 10;
	pConf->lcd_timing.lcd_clk = (temp * pConf->lcd_basic.v_period / 10) *
					pConf->lcd_basic.h_period;

	pConf->lcd_timing.hsync_width = edid_parse->preferred_timing.h_pw;
	pConf->lcd_timing.hsync_bp =
		edid_parse->preferred_timing.h_blank -
		edid_parse->preferred_timing.h_fp -
		edid_parse->preferred_timing.h_pw; /* without pw */
	pConf->lcd_timing.vsync_width = edid_parse->preferred_timing.v_pw;
	pConf->lcd_timing.vsync_bp =
		edid_parse->preferred_timing.v_blank -
		edid_parse->preferred_timing.v_fp -
		edid_parse->preferred_timing.v_pw; /* without pw */

	pConf->lcd_basic.h_active_area =
		edid_parse->preferred_timing.h_size;
	pConf->lcd_basic.v_active_area =
		edid_parse->preferred_timing.v_size;
	pConf->lcd_basic.screen_ratio_width =
		edid_parse->preferred_timing.h_size;
	pConf->lcd_basic.screen_ratio_height =
		edid_parse->preferred_timing.v_size;

	pConf->lcd_timing.pol_ctrl =
		((pConf->lcd_timing.pol_ctrl & ~((1 << POL_CTRL_HS) |
		(1 << POL_CTRL_VS))) |
		(((edid_parse->preferred_timing.timing_ctrl >> 1) & 1) <<
			POL_CTRL_HS) |
		(((edid_parse->preferred_timing.timing_ctrl >> 2) & 1) <<
			POL_CTRL_VS));
}

int edp_edid_timing_probe(struct Lcd_Config_s *pConf)
{
	unsigned int status;
	unsigned char edid_data[128];
	struct EDP_EDID_Data_Type_s edid_data_parse;

	status = trdp_read_EDID(&edid_data[0]);
	if (status) {
		DPRINT("Failed to get EDID from sink device\n");
		return -1;
	} else {
		status = EDID_parse_data(&edid_data[0], &edid_data_parse);
		if (status == RET_EDP_TX_OPERATION_SUCCESS) {
			edp_edid_timing_config(pConf, &edid_data_parse);
			DPRINT("Load edp EDID timing\n");
		} else {
			DPRINT("Failed to load EDID timing\n");
			return -1;
		}
	}

	return 0;
}

