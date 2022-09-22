/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMIRX_REPEATER__
#define __HDMIRX_REPEATER__

/* EDID */
#define MAX_RECEIVE_EDID	40/*33*/
#define MAX_HDR_LUMI		3
#define MAX_KSV_SIZE		5
#define MAX_REPEAT_DEPTH	7
#define MAX_KSV_LIST_SIZE	sizeof(struct hdcp14_topo_s)
#define HDCP14_KSV_MAX_COUNT	127
#define DETAILED_TIMING_LEN	18
/*size of one format in edid*/
#define FORMAT_SIZE			sizeof(struct edid_audio_block_t)
#define EDID_DEFAULT_START		132
#define EDID_DESCRIP_OFFSET		2
#define EDID_BLOCK1_OFFSET		128
#define KSV_LIST_WR_TH			100
#define KSV_V_WR_TH			500
#define KSV_LIST_WR_MAX			5
#define KSV_LIST_WAIT_DELAY		500/*according to the timer,5s*/

enum repeater_state_e {
	REPEATER_STATE_WAIT_KSV,
	REPEATER_STATE_WAIT_ACK,
	REPEATER_STATE_IDLE,
	REPEATER_STATE_START,
};

struct hdcp_topo_s {
	unsigned char hdcp_ver;
	unsigned char depth;
	unsigned char dev_cnt;
	unsigned char max_cascade_exceeded;
	unsigned char max_devs_exceeded;
	unsigned char hdcp1_dev_ds;
	unsigned char hdcp2_dev_ds;
	unsigned char ksv_list[HDCP14_KSV_MAX_COUNT * 5];
};

struct hdcp_hw_info_s {
	unsigned int cur_5v:4;
	unsigned int open:4;
	unsigned int frame_rate:8;
	unsigned int signal_stable:1;
	unsigned int reserved:15;
};

extern int receive_edid_len;
extern int tx_hpd_event;
extern bool new_edid;
//extern int hdcp_array_len;
extern int hdcp_len;
extern int hdcp_repeat_depth;
extern bool new_hdcp;
extern bool repeat_plug;
extern int up_phy_addr;/*d c b a 4bit*/
//extern unsigned char receive_hdcp[MAX_KSV_LIST_SIZE];
extern u8 ksvlist[10];

int rx_hdmi_tx_notify_handler(struct notifier_block *nb,
				     unsigned long value, void *p);
u8 hdmitx_reauth_request(u8 hdcp_version);
u8 __attribute__((weak))hdmitx_reauth_request(u8 hdcp_version);
void rx_set_repeater_support(bool enable);
bool get_rx_active_sts(void);
//int rx_set_receiver_edid(const char *data, int len);
void rx_start_repeater_auth(void);
void rx_set_repeat_signal(bool repeat);
bool rx_set_repeat_aksv(unsigned char *data, int len, int depth,
			bool dev_exceed, bool cascade_exceed);
unsigned char *rx_get_dw_edid_addr(void);
void repeater_dwork_handle(struct work_struct *work);
//bool rx_set_receive_hdcp(unsigned char *data,
			 //int len, int depth,
			 //bool cas_exceed, bool devs_exceed);
void rx_repeat_hpd_state(bool plug);
void rx_repeat_hdcp_ver(int version);
void rx_check_repeat(void);
bool hdmirx_is_key_write(void);
unsigned char *rx_get_dw_hdcp_addr(void);
unsigned char *rx_get_dw_edid_addr(void);

#endif
