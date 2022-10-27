/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_HDCP_H__
#define __HDMI_HDCP_H__

#define DDC_HDCP_DEVICE_ADDR 0x74
#define REG_DDC_HDCP_VERSION 0x50

enum hdcp_ver_t {
	HDCP_VER_NONE,
	HDCP_VER_HDCP1X,
	HDCP_VER_HDCP2X,
};

#define HDCP1X_MAX_DEPTH 7
/* for repeater enable case, as maximum ksv fifo 16*5 bytes */
#define HDCP1X_MAX_TX_DEV_RPT 16 //127
/* for source case(hdcp repeater disabled) */
#define HDCP1X_MAX_TX_DEV_SRC 127
#define HDCP1X_MAX_RPTR_DEV 32

#define HDCP2X_MAX_DEV 31
#define HDCP2X_MAX_DEPTH 4

#define KSV_SIZE 5

struct hdcp_ksv_t {
	u8 b[KSV_SIZE]; /* 40-bit ksv */
};

struct hdcp_topo_t {
	/* hdcp1x compliant device in the topology if true, for hdcp2x */
	bool ds_hdcp1x_dev;
	/* hdcp 2.0 compliant repeater in the topology if true, for hdcp2x */
	bool ds_hdcp2x_dev;
	/* more than 7 level for hdcp1x or 4 level for hdcp2x cascaded together if true */
	bool max_cas_exceed;
	/* more than 127 devices(for hdcp1x) or 31 devices(for hdcp2x) as attached if true */
	bool max_devs_exceed;
	/* total number of attached downstream devices */
	u8 dev_count;
	/* repeater cascade depth */
	u8 rp_depth;
	/* seq_num_v value, for hdcp2x */
	u32 seq_num_v;
};

enum hdcp_content_type_t {
	HDCP_CONTENT_TYPE_0,
	HDCP_CONTENT_TYPE_1,
	HDCP_CONTENT_TYPE_NONE = 0xff,
};

struct hdcp_csm_t {
	u32 seq_num_m; /* seq_num_m value, hdcp2x use only */
	u16 streamid_type; /* streamid_type */
};

enum hdcp_fail_types_t {
	HDCP_FAIL_NONE,
	HDCP_FAIL_DDC_NACK,
	HDCP_FAIL_BKSV_RXID,
	HDCP_FAIL_AUTH_FAIL,
	HDCP_FAIL_READY_TO,
	HDCP_FAIL_V,
	HDCP_FAIL_TOPOLOGY,
	HDCP_FAIL_RI,
	HDCP_FAIL_REAUTH_REQ,
	HDCP_FAIL_CONTENT_TYPE,
	HDCP_FAIL_AUTH_TIME_OUT,
	HDCP_FAIL_HASH,
	HDCP_FAIL_UNKNOWN,
};

enum hdcp_stat_t {
	HDCP_STAT_NONE,
	HDCP_STAT_AUTH,
	HDCP_STAT_CSM,
	HDCP_STAT_SUCCESS,
	HDCP_STAT_FAILED,
};

#endif /* __HDMI_HDCP_H__ */
