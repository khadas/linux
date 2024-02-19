/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) WITH Linux-syscall-note */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef HDMI_RX_H_
#define HDMI_RX_H_

struct _hdcp_ksv {
	int bksv0;
	int bksv1;
};

struct pd_infoframe_s {
	unsigned int HB;
	unsigned int PB0;
	unsigned int PB1;
	unsigned int PB2;
	unsigned int PB3;
	unsigned int PB4;
	unsigned int PB5;
	unsigned int PB6;
};

enum hdcp14_key_mode_e {
	NORMAL_MODE,
	SECURE_MODE,
};

/* source product descriptor infoFrame  - 0x83 */
//0x00 "unknown",
//0x01 "Digital STB",
//0x02 "DVD player",
//0x03 "D-VHS",
//0x04 "HDD Videorecorder",
//0x05 "DVC",
//0x06 "DSC",
//0x07 "Video CD",
//0x08 "Game",
//0x09 "PC general",
//0x0A "Blu-Ray Disc (BD)",
//0x0B "Super Audio CD",
//0x0C "HD DVD",
//0x0D "PMP",
//0x1A "FREESYNC"
struct spd_infoframe_st {
	unsigned char pkttype;
	unsigned char version;
	unsigned char length;
	//u8 rsd;  //note: T3 has not this byte. T5 has it.
	unsigned char checksum;
	//u8 ieee_oui[3]; //data[1:3]
	union meta_u {
		struct freesync_st {
			/*PB1-3*/
			unsigned int ieee:24;
			unsigned int rsvd:8;
			unsigned char rsvd1;
			/*PB6*/
			unsigned char supported:1;
			unsigned char enabled:1;
			unsigned char active:1;
			//u8 cs_active:1;
			unsigned char rsvd2:5;
			//u8 ld_disable:1;
			//u8 rsvd3:3;
			unsigned char min_frame_rate;
			unsigned char max_frame_rate;
			/*pb9-pb27*/
			unsigned char data[19];
		} freesync;
		unsigned char data[27];
		struct spd_data_st {
			/*Vendor Name Character*/
			unsigned char vendor_name[8];
			/*Product Description Character*/
			unsigned char product_des[16];
			/*byte 25*/
			unsigned char source_info;
			unsigned char rsvd[3];
		} spddata;
	} des_u;
};

struct hdmirx_hpd_info {
	int signal;
	int port;
};

#define HDMI_IOC_MAGIC 'H'
#define HDMI_IOC_HDCP_ON	_IO(HDMI_IOC_MAGIC, 0x01)
#define HDMI_IOC_HDCP_OFF	_IO(HDMI_IOC_MAGIC, 0x02)
#define HDMI_IOC_EDID_UPDATE	_IO(HDMI_IOC_MAGIC, 0x03)
#define HDMI_IOC_PC_MODE_ON	_IO(HDMI_IOC_MAGIC, 0x04)
#define HDMI_IOC_PC_MODE_OFF	_IO(HDMI_IOC_MAGIC, 0x05)
#define HDMI_IOC_HDCP22_AUTO	_IO(HDMI_IOC_MAGIC, 0x06)
#define HDMI_IOC_HDCP22_FORCE14	_IO(HDMI_IOC_MAGIC, 0x07)
#define HDMI_IOC_HDCP_GET_KSV	_IOR(HDMI_IOC_MAGIC, 0x09, struct _hdcp_ksv)
#define HDMI_IOC_HDMI_20_SET	_IO(HDMI_IOC_MAGIC, 0x08)
#define HDMI_IOC_PD_FIFO_PKTTYPE_EN	_IOW(HDMI_IOC_MAGIC, 0x0a, unsigned int)
#define HDMI_IOC_PD_FIFO_PKTTYPE_DIS	_IOW(HDMI_IOC_MAGIC, 0x0b, unsigned int)
#define HDMI_IOC_GET_PD_FIFO_PARAM	_IOWR(HDMI_IOC_MAGIC, 0x0c, struct pd_infoframe_s)
#define HDMI_IOC_HDCP14_KEY_MODE	_IOR(HDMI_IOC_MAGIC, 0x0d, enum hdcp14_key_mode_e)
#define HDMI_IOC_HDCP22_NOT_SUPPORT	_IO(HDMI_IOC_MAGIC, 0x0e)
#define HDMI_IOC_SET_AUD_SAD		_IOW(HDMI_IOC_MAGIC, 0x0f, int)
#define HDMI_IOC_GET_AUD_SAD		_IOR(HDMI_IOC_MAGIC, 0x10, int)
#define HDMI_IOC_GET_SPD_SRC_INFO	_IOR(HDMI_IOC_MAGIC, 0x11, struct spd_infoframe_st)
#define HDMI_5V_PIN_STATUS		_IOR(HDMI_IOC_MAGIC, 0x12, unsigned int)
#define HDMI_IOC_EDID_UPDATE_WITH_PORT  _IOW(HDMI_IOC_MAGIC, 0x13, unsigned char)
#define HDMI_IOC_SET_HPD	_IOW(HDMI_IOC_MAGIC, 0x15, struct hdmirx_hpd_info)
#define HDMI_IOC_GET_HPD	_IOR(HDMI_IOC_MAGIC, 0x16, struct hdmirx_hpd_info)

#endif
