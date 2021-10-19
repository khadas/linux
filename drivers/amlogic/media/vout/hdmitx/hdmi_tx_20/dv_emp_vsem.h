/*
 * drivers/amlogic/media/vout/hdmitx/hdmi_tx_20/dv_emp_vsem.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#include <linux/amlogic/media/vout/vinfo.h>

#ifndef _EDR_METADATA_OVER_HDMI_H_
#define _EDR_METADATA_OVER_HDMI_H_

#define VSIF_PAYLOAD_LEN   24
#define VSEM_PKT_BODY_SIZE   28
#define VSEM_FIRST_PKT_EDR_DATA_SIZE   15
#define VSEM_CRC_LEN 4

struct vsem_pkt {
	unsigned char pkt_type;
	unsigned char hb1;
	unsigned char seq_index;
	unsigned char pb[VSEM_PKT_BODY_SIZE];
	unsigned char padding;
};

#define MAX_VSEM_NUM    128
#define MAX_VSEM_SIZE (MAX_VSEM_NUM * sizeof(struct vsem_pkt))

#define FIELD_FIRST   0x80
#define FIELD_LAST    0x40

#define FIELD_NEW     0x80
#define FIELD_END     0x40
#define FIELD_DS_TYPE 0x30
#define FIELD_AFR     0x08
#define FIELD_VFR     0x04
#define FIELD_SYNC    0x02

struct emp_edr_config {
	struct vsem_pkt *vsem_packets;
	unsigned long phys_ptr;
	unsigned int num_packets;
};

struct emp_hdmi_cfg {
	struct hdmitx_dev *hdev;
	struct vsem_pkt *p_pkts;
	dma_addr_t pkts_phy_addr;
	size_t size;
	int (*send_vsemds)(enum eotf_type type,
				enum mode_type tunnel_mode,
				struct dv_vsif_para *vsif_data,
				unsigned char *p_vsem,
				int vsem_len,
				bool signal_sdr);
};

extern int hdr_status_pos;
extern void update_current_para(struct hdmitx_dev *hdev);
extern void hdmitx_set_vsif_pkt(enum eotf_type type,
		enum mode_type tunnel_mode,
		struct dv_vsif_para *data,
		bool signal_sdr);
#endif /* _EDR_METADATA_OVER_HDMI_H_ */
