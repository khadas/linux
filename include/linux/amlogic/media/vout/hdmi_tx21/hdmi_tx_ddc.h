/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX_DDC_H__
#define __HDMI_TX_DDC_H__

#include <linux/types.h>
#include "hdmi_tx_module.h"

#define DDC_HDCP_ADDR 0x74
	#define DDC_HDCP_REG_VERSION 0x50

#define DDC_EDID_ADDR 0xA0
	#define DDC_EDIDSEG_ADDR 0x30
#define DDC_SCDC_ADDR 0xA8

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
	/* RW 0xDE ~ 0xFF */
	MANUFACT_SPECIFIC = 0xDE,
};

int hdmitx21_ddc_hw_op(enum ddc_op cmd);

void scdc21_rd_sink(u8 adr, u8 *val);
void scdc21_wr_sink(u8 adr, u8 val);
uint32_t hdcp21_rd_hdcp14_ver(void);
uint32_t hdcp21_rd_hdcp22_ver(void);
void scdc21_config(struct hdmitx_dev *hdev);
void edid21_read_head_8bytes(void);
int scdc21_status_flags(struct hdmitx_dev *hdev);
void hdmitx21_read_edid(u8 *rx_edid);
#endif  /* __HDMI_TX_SCDC_H__ */
