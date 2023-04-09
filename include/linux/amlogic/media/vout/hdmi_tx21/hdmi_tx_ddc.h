/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX_DDC_H__
#define __HDMI_TX_DDC_H__

#include <linux/types.h>

enum ddc_op;
struct hdmitx_dev;

#define DDC_HDCP_ADDR 0x74
	#define DDC_HDCP_REG_VERSION 0x50

#define DDC_EDID_ADDR 0xA0
	#define DDC_EDIDSEG_ADDR 0x30
#define DDC_SCDC_ADDR 0xA8

#define SCDC_DEVICE_SOFTWARE_MAJOR_REVISION 0xdc
#define SCDC_DEVICE_SOFTWARE_MINOR_REVISION 0xdd

#define SCDC_MANUFACTURER_SPECIFIC 0xde
#define SCDC_MANUFACTURER_SPECIFIC_SIZE 34

/* Little-Endian format */
enum scdc_addr {
	SCDC_SINK_VER = 0x01,
	SCDC_SOURCE_VER = 0x2, /* RW */
	SCDC_UPDATE_0 = 0x10, /* RW */
#define RSED_UPDATE		BIT(6)
#define FLT_UPDATE		BIT(5)
#define FRL_START		BIT(4)
#define SOURCE_TEST_UPDATE	BIT(3)
#define READ_REQUEST_TEST	BIT(2)
#define CED_UPDATE		BIT(1)
#define STATUS_UPDATE		BIT(0)
#define HDMI21_UPDATE_FLAGS	\
	(RSED_UPDATE | FLT_UPDATE | FRL_START | SOURCE_TEST_UPDATE)
#define HDMI20_UPDATE_FLAGS	\
	(SOURCE_TEST_UPDATE | READ_REQUEST_TEST | CED_UPDATE | STATUS_UPDATE)
	SCDC_UPDATE_1 = 0x11, /* RW */
	SCDC_TMDS_CFG = 0x20, /* RW */
#define TMDS_BIT_CLOCK_RATIO	BIT(1)
#define SCRAMBLING_ENABLE	BIT(0)
	SCDC_TMDS_SCRAMBLER_ST = 0x21,
#define TMDS_SCRAMBLER_STATUS	BIT(0)
	SCDC_CONFIG_0 = 0x30, /* RW */
#define CA_PWR_ERR		BIT(5)
#define MONO_DIR_ERR		BIT(4)
#define MONO_DIR_ON		BIT(3)
#define DAISY_ERR		BIT(2)
#define FLT_NO_RETRAIN		BIT(1)
#define RR_ENABLE		BIT(0)
	SCDC_CONFIG_1 = 0x31, /* RW */
#define BITS_FRL_RATE(x)	((x) & 0xf)
#define BITS_FFE_LEVELS(x)	(((x) >> 4) & 0xf)
	SCDC_SOURCE_TEST_CONFIG = 0x35,
#define FRL_MAX			BIT(7)
#define DSC_FRL_MAX		BIT(6)
#define FLT_NO_TIMEOUT		BIT(5)
#define TXFFE_NO_FFE		BIT(3)
#define TXFFE_DE_EMPHASIS_ONLY	BIT(2)
#define TXFFE_PRE_SHOOT_ONLY	BIT(1)
	SCDC_STATUS_FLAGS_0 = 0x40,
#define DSC_DECODE_FAIL		BIT(7)
#define FLT_READY		BIT(6)
#define LANE3_LOCK		BIT(4)
#define CH2_LN2_LOCK		BIT(3)
#define CH1_LN1_LOCK		BIT(2)
#define CH0_LN0_LOCK		BIT(1)
#define CLOCK_DETECT		BIT(0)
	SCDC_STATUS_FLAGS_1 = 0x41,
	SCDC_STATUS_FLAGS_2 = 0x42,
	SCDC_CH0_ERRCNT_0 = 0x50,
	SCDC_CH0_ERRCNT_1 = 0x51,
#define CH_LN_VALID BIT(7)
	SCDC_CH1_ERRCNT_0 = 0x52,
	SCDC_CH1_ERRCNT_1 = 0x53,
	SCDC_CH2_ERRCNT_0 = 0x54,
	SCDC_CH2_ERRCNT_1 = 0x55,
	SCDC_CHKSUM_OF_CED = 0x56,
	SCDC_LN3_ERRCNT_0 = 0x57,
	SCDC_LN3_ERRCNT_1 = 0x58,
	SCDC_RS_CORRECTION_CNT_0 = 0x59,
	SCDC_RS_CORRECTION_CNT_1 = 0x5A,
	SCDC_TEST_READ_REQUEST = 0xC0, /* RW */
#define TEST_READ_REQ BIT(7)
#define TEST_READ_REQ_DELAY(x) ((x) & 0x7f)
	SCDC_MANUFACT_IEEEOUI_0 = 0xD0,
	SCDC_MANUFACT_IEEEOUI_1 = 0xD1,
	SCDC_MANUFACT_IEEEOUI_2 = 0xD2,
	SCDC_DEV_ID_STRING = 0xD3, /* 0xD3 ~ 0xDA */
#define DEV_ID_STRING_SIZE 8
	/* RW 0xDE ~ 0xFF */
	SCDC_DEV_HW_REVISION = 0xDB,
#define DEV_HW_REV_MAJOR(x) (((x) >> 4) & 0xf)
#define DEV_HW_REV_MINOR(x) (((x) >> 0) & 0xf)
	SCDC_DEV_SW_REV_MAJOR = 0xDC,
	SCDC_DEV_SW_REV_MINOR = 0xDD,
	SCDC_MANUFACT_SPECIFIC = 0xDE,
#define MANUFACT_SPECIFIC_SIZE 34
};

enum frl_rate_enum {
	FRL_NONE = 0,
	FRL_3G3L = 1,
	FRL_6G3L = 2,
	FRL_6G4L = 3,
	FRL_8G4L = 4,
	FRL_10G4L = 5,
	FRL_12G4L = 6,
	FRL_RATE_MAX = 7,
};

enum flt_tx_states {
	FLT_TX_LTS_L,	/* legacy mode */
	FLT_TX_LTS_1,	/* read edid */
	FLT_TX_LTS_2,	/* prepare for frl */
	FLT_TX_LTS_3,	/* training in progress */
	FLT_TX_LTS_4,	/* update frl_rate */
	/* frl training passed, start frl w/o video */
	FLT_TX_LTS_P1,
	/* start frl w/ video */
	FLT_TX_LTS_P2,
	/* maintain frl video */
	FLT_TX_LTS_P3,
};

enum ffe_levels {
	FFE_LEVEL0 = 0,
	FFE_LEVEL1 = 1,
	FFE_LEVEL2 = 2,
	FFE_LEVEL3 = 3,
};

enum ltp_patterns {
	/* No Link Training Pattern requested */
	LTP_PATTERN_NO_LTP = 0,
	/* All 1's pattern */
	LTP_PATTERN_LTP1 = 1,
	/* All 0's pattern */
	LTP_PATTERN_LTP2 = 2,
	/* Nyquist clock pattern */
	LTP_PATTERN_LTP3 = 3,
	/* Source TxFFE Compliance Test Pattern */
	LTP_PATTERN_LTP4 = 4,
	/* LFSR 0 */
	LTP_PATTERN_LTP5 = 5,
	/* LFSR 1 */
	LTP_PATTERN_LTP6 = 6,
	/* LFSR 2 */
	LTP_PATTERN_LTP7 = 7,
	/* LFSR 3 */
	LTP_PATTERN_LTP8 = 8,
	/* reserved 9 ~ 0xd */
	LTP_PATTERN_RSVD = 9,
	/* Sink requests Source to update FFE */
	LTP_PATTERN_UPDATE_FFE = 0xE,
	/* Sink requests Source to change link mode(rate) to a lower value */
	LTP_PATTERN_CHG_LINK_MODE = 0x0F,
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
