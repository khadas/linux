/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX_EXT_H__
#define __HDMI_TX_EXT_H__

#include <linux/notifier.h>
#include <linux/kernel.h>
#include <sound/asoundef.h>

/* interface for external module: audio/cec/hdmirx/dv... */

/* for notify to cec/hdmirx */
#define HDMITX_PLUG			1
#define HDMITX_UNPLUG			2
#define HDMITX_PHY_ADDR_VALID		3
#define HDMITX_KSVLIST	4

enum hdcp_ver_e {
	HDCPVER_NONE = 0,
	HDCPVER_14,
	HDCPVER_22,
};

#define MAX_KSV_LISTS 127
struct hdcprp14_topo {
	unsigned char max_cascade_exceeded:1;
	unsigned char depth:3;
	unsigned char rsvd : 4;
	unsigned char max_devs_exceeded:1;
	unsigned char device_count:7; /* 1 ~ 127 */
	unsigned char ksv_list[MAX_KSV_LISTS * 5];
} __packed;

struct hdcprp22_topo {
	unsigned int depth;
	unsigned int device_count;
	unsigned int v1_X_device_down;
	unsigned int v2_0_repeater_down;
	unsigned int max_devs_exceeded;
	unsigned int max_cascade_exceeded;
	unsigned char id_num;
	unsigned char id_lists[MAX_KSV_LISTS * 5];
};

struct hdcprp_topo {
	/* hdcp_ver currently used */
	enum hdcp_ver_e hdcp_ver;
	union {
		struct hdcprp14_topo topo14;
		struct hdcprp22_topo topo22;
	} topo;
};

/* global used by hdmitx21/20, and extern module */
/* -----------------Source Physical Address--------------- */
struct vsdb_phyaddr {
	unsigned char a:4;
	unsigned char b:4;
	unsigned char c:4;
	unsigned char d:4;
	unsigned char valid;
};

/* Sampling Freq Fs:
 * 0 - Refer to Stream Header;
 * 1 - 32KHz;
 * 2 - 44.1KHz;
 * 3 - 48KHz;
 * 4 - 88.2KHz...
 */
enum hdmi_audio_fs {
	FS_REFER_TO_STREAM = 0,
	FS_32K = 1,
	FS_44K1 = 2,
	FS_48K = 3,
	FS_88K2 = 4,
	FS_96K = 5,
	FS_176K4 = 6,
	FS_192K = 7,
	FS_768K = 8,
	FS_MAX,
};

/* HDMI Audio Parmeters */
/* Refer to CEA-861-D Page 88 */
#define DTS_HD_TYPE_MASK 0xff00
#define DTS_HD_MA  (0X1 << 8)
enum hdmi_audio_type {
	CT_REFER_TO_STREAM = 0,
	CT_PCM,
	CT_AC_3, /* DD */
	CT_MPEG1,
	CT_MP3,
	CT_MPEG2,
	CT_AAC,
	CT_DTS,
	CT_ATRAC,
	CT_ONE_BIT_AUDIO,
	CT_DD_P, /* DD+ */
	CT_DTS_HD,
	CT_MAT, /* TrueHD */
	CT_DST,
	CT_WMA,
	CT_CXT = 0xf, /* Audio Coding Extension Type */
	CT_DTS_HD_MA = CT_DTS_HD + (DTS_HD_MA),
	CT_MAX,
	CT_PREPARE, /* prepare for audio mode switching */
};

#define CT_DOLBY_D CT_DD_P

enum hdmi_audio_chnnum {
	CC_REFER_TO_STREAM = 0,
	CC_2CH,
	CC_3CH,
	CC_4CH,
	CC_5CH,
	CC_6CH,
	CC_7CH,
	CC_8CH,
	CC_MAX_CH
};

enum hdmi_audio_format {
	AF_SPDIF = 0, AF_I2S, AF_DSD, AF_HBR, AT_MAX
};

enum hdmi_audio_sampsize {
	SS_REFER_TO_STREAM = 0, SS_16BITS, SS_20BITS, SS_24BITS, SS_MAX
};

enum hdmi_audio_source_if {
	AUD_SRC_IF_SPDIF = 0,
	AUD_SRC_IF_I2S,
	AUD_SRC_IF_TDM, /* for T7 only */
};

/* should sync with sound/soc */
struct aud_para {
	enum hdmi_audio_type type;
	enum hdmi_audio_fs rate;
	enum hdmi_audio_sampsize size;
	enum hdmi_audio_chnnum chs;
	enum hdmi_audio_source_if aud_src_if;
	unsigned char status[24]; /* AES/IEC958 channel status bits */
	u8 i2s_ch_mask;
	bool fifo_rst;
	bool prepare; /* when prepare is true, mute tx audio sample */
};

typedef void (*pf_callback)(bool st);

int get_hpd_state(void);
int hdmitx_event_notifier_regist(struct notifier_block *nb);
int hdmitx_event_notifier_unregist(struct notifier_block *nb);
struct vsdb_phyaddr *get_hdmitx_phy_addr(void);
void get_attr(char attr[16]);
void setup_attr(const char *buf);
void hdmitx_audio_mute_op(unsigned int flag);
void hdmitx_video_mute_op(u32 flag);
int register_earcrx_callback(pf_callback callback);
void unregister_earcrx_callback(void);

/*
 * HDMI TX output enable, such as ACRPacket/AudInfo/AudSample
 * enable: 1, normal output; 0: disable output
 */
void hdmitx_ext_set_audio_output(int enable);

/*
 * return Audio output status
 * 1: normal output status; 0: output disabled
 */
int hdmitx_ext_get_audio_status(void);

#endif

