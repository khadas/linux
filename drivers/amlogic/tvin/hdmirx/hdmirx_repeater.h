/*
 * hdmirx_repeater.h for HDMI device driver, and declare IO function,
 * structure, enum, used in hdmirx sub-module processing
 *
 * Copyright (C) 2014 AMLOGIC, INC. All Rights Reserved.
 * Author: hongmin hua <hongmin.hua@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#ifndef __HDMIRX_REPEATER__
#define __HDMIRX_REPEATER__

/* EDID */
#define MAX_RECEIVE_EDID	33
#define MAX_HDR_LUMI		3
#define MAX_KSV_SIZE		5
#define MAX_REPEAT_COUNT	127
#define MAX_REPEAT_DEPTH	7
#define MAX_KSV_LIST_SIZE	(MAX_KSV_SIZE*MAX_REPEAT_COUNT)
/*size of one format in edid*/
#define FORMAT_SIZE			sizeof(struct edid_audio_block_t)
#define EDID_DEFAULT_START		132
#define EDID_DESCRIP_OFFSET		2
#define EDID_BLOCK1_OFFSET		128
#define KSV_LIST_WR_TH			100
#define KSV_V_WR_TH			500
#define KSV_LIST_WR_MAX			5
#define KSV_LIST_WAIT_DELAY		500/*according to the timer,5s*/

#define is_audio_support(x) (((x) == AUDIO_FORMAT_LPCM) || \
		((x) == AUDIO_FORMAT_DTS) || ((x) == AUDIO_FORMAT_DDP))

enum repeater_state_e {
	REPEATER_STATE_WAIT_KSV,
	REPEATER_STATE_WAIT_ACK,
	REPEATER_STATE_IDLE,
	REPEATER_STATE_START,
};

extern int receive_edid_len;
extern bool new_edid;
extern int hdcp_array_len;
extern int hdcp_len;
extern int hdcp_repeat_depth;
extern bool new_hdcp;
extern bool repeat_plug;
extern int up_phy_addr;/*d c b a 4bit*/

extern void rx_modify_edid(unsigned char *buffer,
				int len, unsigned char *addition);
extern unsigned int rx_exchange_bits(unsigned int value);
extern void rx_start_repeater_auth(void);
extern void rx_edid_update_audio_info(unsigned char *p_edid,
						unsigned int len);
extern void rx_set_repeat_signal(bool repeat);
extern bool rx_set_repeat_aksv(unsigned char *data, int len, int depth,
		bool dev_exceed, bool cascade_exceed);

#endif
