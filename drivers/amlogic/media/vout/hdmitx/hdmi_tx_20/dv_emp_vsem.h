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

#define VSEM_CRC_LENGTH 4
#define VSIF_PAYLOAD_LEN   24
#define VSEM_PACKET_BODY_LEN   28
#define VSEM_FIRST_PACKET_EDR_DATA_LEN   15

struct vsem_data_packet {
	unsigned char packet_type;
	unsigned char HB1;
	unsigned char sequence_index;
	unsigned char PB[VSEM_PACKET_BODY_LEN];
	unsigned char padding;
};
#define MAX_VSEM_NUM    128
#define MAX_VSEM_SIZE (MAX_VSEM_NUM * sizeof(struct vsem_data_packet))

/* header field masks */
/* second header byte */
#define FIELD_MASK_FIRST        0x80
#define FIELD_MASK_LAST         0x40

/* packet field masks */
/* first packet byte */
#define FIELD_MASK_NEW     0x80
#define FIELD_MASK_END     0x40
#define FIELD_MASK_DS_TYPE 0x30
#define FIELD_MASK_AFR     0x08
#define FIELD_MASK_VFR     0x04
#define FIELD_MASK_SYNC    0x02

struct emp_edr_config {
	struct vsem_data_packet *vsem_packets;
	unsigned long phys_ptr;
	unsigned int num_packets;
};

struct dv_emp_hdmi_cfg {
	struct hdmitx_dev *hdev;
	struct vsem_data_packet *p_pkts;
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
