/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _SCPI_PROTOCOL_H_
#define _SCPI_PROTOCOL_H_
#include <linux/types.h>

enum scpi_client_id {
	SCPI_CL_NONE,
	SCPI_CL_CLOCKS,
	SCPI_CL_DVFS,
	SCPI_CL_POWER,
	SCPI_CL_THERMAL,
	SCPI_CL_REMOTE,
	SCPI_CL_LED_TIMER,
	SCPI_CL_SET_CEC_DATA,
	SCPI_MAX,
};

enum scpi_std_cmd {
	SCPI_CMD_INVALID		= 0x00,
	SCPI_CMD_SCPI_READY		= 0x01,
	SCPI_CMD_SCPI_CAPABILITIES	= 0x02,
	SCPI_CMD_EVENT			= 0x03,
	SCPI_CMD_SET_CSS_PWR_STATE	= 0x04,
	SCPI_CMD_GET_CSS_PWR_STATE	= 0x05,
	SCPI_CMD_CFG_PWR_STATE_STAT	= 0x06,
	SCPI_CMD_GET_PWR_STATE_STAT	= 0x07,
	SCPI_CMD_SYS_PWR_STATE		= 0x08,
	SCPI_CMD_L2_READY		= 0x09,
	SCPI_CMD_SET_AP_TIMER		= 0x0a,
	SCPI_CMD_CANCEL_AP_TIME		= 0x0b,
	SCPI_CMD_DVFS_CAPABILITIES	= 0x0c,
	SCPI_CMD_GET_DVFS_INFO		= 0x0d,
	SCPI_CMD_SET_DVFS		= 0x0e,
	SCPI_CMD_GET_DVFS		= 0x0f,
	SCPI_CMD_GET_DVFS_STAT		= 0x10,
	SCPI_CMD_SET_RTC		= 0x11,
	SCPI_CMD_GET_RTC		= 0x12,
	SCPI_CMD_CLOCK_CAPABILITIES	= 0x13,
	SCPI_CMD_SET_CLOCK_INDEX	= 0x14,
	SCPI_CMD_SET_CLOCK_VALUE	= 0x15,
	SCPI_CMD_GET_CLOCK_VALUE	= 0x16,
	SCPI_CMD_PSU_CAPABILITIES	= 0x17,
	SCPI_CMD_SET_PSU		= 0x18,
	SCPI_CMD_GET_PSU		= 0x19,
	SCPI_CMD_SENSOR_CAPABILITIES	= 0x1a,
	SCPI_CMD_SENSOR_INFO		= 0x1b,
	SCPI_CMD_SENSOR_VALUE		= 0x1c,
	SCPI_CMD_SENSOR_CFG_PERIODIC	= 0x1d,
	SCPI_CMD_SENSOR_CFG_BOUNDS	= 0x1e,
	SCPI_CMD_SENSOR_ASYNC_VALUE	= 0x1f,
	SCPI_CMD_SET_USR_DATA = 0x20,
	SCPI_CMD_OSCRING_VALUE = 0x43,
	SCPI_CMD_WAKEUP_REASON_GET = 0x30,
	SCPI_CMD_WAKEUP_REASON_CLR = 0X31,
	SCPI_CMD_GET_ETHERNET_CALC = 0x32,
	SCPI_CMD_GET_CPUINFO = 0x33,
	SCPI_CMD_INIT_DSP = 0x34,
	SCPI_CMD_SET_ETHERNET_WOL = 0x35,
	SCPI_CMD_HIFI4STOP	= 0x4d,
	SCPI_CMD_HIFI4SUSPEND		= 0x4e,
	SCPI_CMD_HIFI4RESUME		= 0x4f,

	SCPI_CMD_HIFI4LOGCTL		= 0x50,
	SCPI_CMD_HIFI4SYSTLOG		= 0x51,
	SCPI_CMD_HIFI5_SYSLOG_START = 0x63,
	SCPI_CMD_HIFI5_FREQ_SET_END	= 0x67,

	SCPI_CMD_GET_CEC1		= 0xB4,
	SCPI_CMD_GET_CEC2		= 0xB5,
	SCPI_CMD_SET_CEC_DATA		= 0xB6,/*to aocpu cec module*/
	SCPI_CMD_SEND_DSP_DATA		= 0xB6,
	SCPI_CMD_SET_CEC1		= 0xB7,
	SCPI_CMD_SET_CEC2		= 0xB8,
	SCPI_CMD_GET_CEC_REASON		= 0xB9,
	SCPI_CMD_SET_CEC_REASON		= 0xBA,
	SCPI_CMD_GET_CEC_OTP_MSG	= 0xBB,
	SCPI_CMD_GET_CEC_AS_MSG		= 0xBC,
	SCPI_CMD_BL4_WAIT_UNLOCK	= 0xD6,
	SCPI_CMD_BL4_SEND		= 0xD7,
	SCPI_CMD_BL4_LISTEN		= 0xD8,
	SCPI_CMD_LEDS_STATE		= 0xF7,
	SCPI_CMD_PM_FREEZE		= 0xF9,
	SCPI_CMD_GET_SPINLOCK		= 0x69,
	SCPI_CMD_COUNT
};

enum scpi_req_cmd {
	SCPI_REQ_INVALID		= 0x00,
	SCPI_REQ_COUNT
};

enum scpi_get_pfm_type {
	SCPI_CPUINFO_CLUSTER0,
	SCPI_CPUINFO_CLUSTER1,
	SCPI_CPUINFO_VERSION,
	SCPI_CPUINFO_SLT,
	SCPI_CPUINFO_NUMS
};

struct scpi_opp_entry {
	u32 freq_hz;
	u32 volt_mv;
} __packed;

struct scpi_dvfs_info {
	unsigned int count;
	unsigned int latency; /* in usecs */
	struct scpi_opp_entry *opp;
} __packed;

struct bl40_msg_buf {
	int size;
	char buf[512];
} __packed;

struct hifi4syslog {
	char syslogstate[4];
	u32 logbaseaddr;
	u32 syslogsize;
	u32 loghead;
	u32 logtail;
} __packed;

unsigned long scpi_clk_get_val(u16 clk_id);
int scpi_clk_set_val(u16 clk_id, unsigned long rate);
int scpi_dvfs_get_idx(u8 domain);
int scpi_dvfs_set_idx(u8 domain, u8 idx);
struct scpi_dvfs_info *scpi_dvfs_get_opps(u8 domain);
int scpi_get_sensor(char *name);
int scpi_get_sensor_value(u16 sensor, u32 *val);
int scpi_send_usr_data(u32 client_id, u32 *val, u32 size);
int scpi_get_vrtc(u32 *p_vrtc);
int scpi_set_vrtc(u32 vrtc_val);
int scpi_get_ring_value(unsigned char *val);
int scpi_get_wakeup_reason(u32 *wakeup_reason);
int scpi_clr_wakeup_reason(void);
int scpi_get_cec_val(enum scpi_std_cmd index, u32 *p_cec);
int scpi_set_cec_val(enum scpi_std_cmd index, u32 cec_data);
int scpi_send_cec_data(u32 cmd_id, u32 *val, u32 size);
int scpi_get_cec_wk_msg(enum scpi_std_cmd index, unsigned char *cec_msg);
u8  scpi_get_ethernet_calc(void);
u32 scpi_set_ethernet_wol(u32 flag);
int scpi_get_cpuinfo(enum scpi_get_pfm_type type, u32 *freq, u32 *vol);
int scpi_init_dsp_cfg0(u32 id, u32 addr, u32 cfg0);
int scpi_unlock_bl40(void);
int scpi_send_bl40(unsigned int cmd, struct bl40_msg_buf *bl40_buf);

enum scpi_chan {
	SCPI_DSPA = 0, /* to dspa */
	SCPI_DSPB = 1, /* to dspb */
	SCPI_AOCPU = 2, /* to aocpu */
	SCPI_SECPU = 3, /* to secpu */
	SCPI_OTHER = 4, /* to other core */
	SCPI_MAXNUM,
};

/* use this api send data to aocpu/secpu/dsp/cm3/cm4
 * donnot need add other api for your self
 * channel: SPCI_DSPA/SCPI_AOCPU and other
 */
int scpi_send_data(void *data, int size, int channel,
		   int cmd, void *revdata, int revsize);
int mbox_message_send_ao_sync(struct device *dev, int cmd, void *sdata,
			      int tx_size, void *rdata, int rx_size, int idx);
int mbox_message_send_sec_sync(struct device *dev, int cmd,
			       void *sdata, size_t tx_size,
			       void *rdata, size_t *rx_size, int idx);
int mbox_message_send_data_sync(struct device *dev, int cmd,
				void *data, int count, int idx);
#endif /*_SCPI_PROTOCOL_H_*/
