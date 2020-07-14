/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX_DDC_H__
#define __HDMI_TX_DDC_H__

#include <linux/types.h>
#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>

#define EDID_SLAVE	0x50
	#define EDIDSEG_ADR	0x30
#define HDCP_SLAVE	0x3a
#define SCDC_SLAVE	0x54

/* Little-Endian format */
enum scdc_addr {
	SINK_VER = 0x01,
	SOURCE_VER, /* RW */
	UPDATE_0 = 0x10, /* RW */
#define STATUS_UPDATE	BIT(0)
#define CED_UPDATE	BIT(1)
#define RR_TEST		BIT(2)
	UPDATE_1, /* RW */
	TMDS_CFG = 0x20, /* RW */
	SCRAMBLER_ST,
	CONFIG_0 = 0x30, /* RW */
	STATUS_FLAGS_0 = 0x40,
	STATUS_FLAGS_1,
	ERR_DET_0_L = 0x50,
	ERR_DET_0_H,
	ERR_DET_1_L,
	ERR_DET_1_H,
	ERR_DET_2_L,
	ERR_DET_2_H,
	ERR_DET_CHKSUM,
	TEST_CONFIG_0 = 0xC0, /* RW */
	MANUFACT_IEEE_OUI_2 = 0xD0,
	MANUFACT_IEEE_OUI_1,
	MANUFACT_IEEE_OUI_0,
	DEVICE_ID = 0xD3, /* 0xD3 ~ 0xDD */
	/* RW   0xDE ~ 0xFF */
	MANUFACT_SPECIFIC = 0xDE,
};

enum hdcp_addr {
	HDCP14_BKSV = 0x00,
	HDCP14_RI_ = 0x08,
	HDCP14_PJ_ = 0x0a,
	HDCP14_AKSV = 0x10,
	HDCP14_AINFO = 0x15,
	HDCP14_AN = 0x18,
	HDCP14_V_H0 = 0x20,
	HDCP14_V_H1 = 0x24,
	HDCP14_V_H2 = 0x28,
	HDCP14_V_H3 = 0x2C,
	HDCP14_V_H4 = 0x30,
	HDCP14_BCAPS = 0x40,
	HDCP14_BSTATUS = 0x41,
	HDCP14_KSV_FIFO = 0x43,
	HDCP2_VERSION = 0x50,
	HDCP2_WR_MSG = 0x60,
	HDCP2_RXSTATUS = 0x70,
	HDCP2_RD_MSG = 0x80,
	HDCP2_DBG = 0xC0,
};

int hdmitx_ddc_hw_op(enum ddc_op cmd);

void scdc_rd_sink(u8 adr, u8 *val);
void scdc_wr_sink(u8 adr, u8 val);
uint32_t hdcp_rd_hdcp14_ver(void);
uint32_t hdcp_rd_hdcp22_ver(void);
void scdc_config(struct hdmitx_dev *hdev);
void edid_read_head_8bytes(void);
int scdc_status_flags(struct hdmitx_dev *hdev);
void hdmitx_read_edid(unsigned char *rx_edid);
#endif  /* __HDMI_TX_SCDC_H__ */
