/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_INFO_GLOBAL_H
#define _HDMI_INFO_GLOBAL_H

#include "hdmi_common.h"
#include "../hdmi_tx_ext.h"

enum hdmi_pixel_repeat {
	NO_REPEAT = 0,
	HDMI_2_TIMES_REPEAT,
	HDMI_3_TIMES_REPEAT,
	HDMI_4_TIMES_REPEAT,
	HDMI_5_TIMES_REPEAT,
	HDMI_6_TIMES_REPEAT,
	HDMI_7_TIMES_REPEAT,
	HDMI_8_TIMES_REPEAT,
	HDMI_9_TIMES_REPEAT,
	HDMI_10_TIMES_REPEAT,
	MAX_TIMES_REPEAT,
};

enum hdmi_scan {
	SS_NO_DATA = 0,
	/* where some active pixelsand lines at the edges are not displayed. */
	SS_SCAN_OVER,
	/* where all active pixels&lines are displayed,
	 * with or without border.
	 */
	SS_SCAN_UNDER,
	SS_RSV
};

enum hdmi_barinfo {
	B_INVALID = 0, B_BAR_VERT, /* Vert. Bar Infovalid */
	B_BAR_HORIZ, /* Horiz. Bar Infovalid */
	B_BAR_VERT_HORIZ,
/* Vert.and Horiz. Bar Info valid */
};

enum hdmi_colourimetry {
	CC_NO_DATA = 0, CC_ITU601, CC_ITU709, CC_XVYCC601, CC_XVYCC709,
};

enum hdmi_slacing {
	SC_NO_UINFORM = 0,
	/* Picture has been scaled horizontally */
	SC_SCALE_HORIZ,
	SC_SCALE_VERT, /* Picture has been scaled vertically */
	SC_SCALE_HORIZ_VERT,
/* Picture has been scaled horizontally & SC_SCALE_H_V */
};

struct hdmi_videoinfo {
	enum hdmi_vic VIC;
	enum hdmi_color_space color;
	enum hdmi_color_depth color_depth;
	enum hdmi_barinfo bar_info;
	enum hdmi_pixel_repeat repeat_time;
	enum hdmi_aspect_ratio aspect_ratio;
	enum hdmi_colourimetry cc;
	enum hdmi_scan ss;
	enum hdmi_slacing sc;
};

/* -------------------HDMI VIDEO END---------------------------- */

/* -----------------------HDMI TX---------------------------------- */
struct hdmitx_vidpara {
	unsigned int VIC;
	enum hdmi_color_space color_prefer;
	enum hdmi_color_space color;
	enum hdmi_color_depth color_depth;
	enum hdmi_barinfo bar_info;
	enum hdmi_pixel_repeat repeat_time;
	enum hdmi_aspect_ratio aspect_ratio;
	enum hdmi_colourimetry cc;
	enum hdmi_scan ss;
	enum hdmi_slacing sc;
};

struct hdmitx_audpara {
	enum hdmi_audio_type type;
	enum hdmi_audio_chnnum channel_num;
	enum hdmi_audio_fs sample_rate;
	enum hdmi_audio_sampsize sample_size;
	enum hdmi_audio_source_if aud_src_if;
	bool fifo_rst;
};

/* ACR packet CTS parameters have 3 types: */
/* 1. HW auto calculated */
/* 2. Fixed values defined by Spec */
/* 3. Calculated by clock meter */
enum hdmitx_audcts {
	AUD_CTS_AUTO = 0, AUD_CTS_FIXED, AUD_CTS_CALC,
};

struct dispmode_vic {
	const char *disp_mode;
	enum hdmi_vic VIC;
};

struct hdmitx_audinfo {
	/* !< Signal decoding type -- TvAudioType */
	enum hdmi_audio_type type;
	enum hdmi_audio_format format;
	/* !< active audio channels bit mask. */
	enum hdmi_audio_chnnum channels;
	enum hdmi_audio_fs fs; /* !< Signal sample rate in Hz */
	enum hdmi_audio_sampsize ss;
};

#define Y420CMDB_MAX	32
struct hdmitx_info {
	struct hdmi_rx_audioinfo audio_info;
	/* Hdmi_tx_video_info_t            video_info; */
	/* -----------------Source Physical Address--------------- */
	struct vsdb_phyaddr vsdb_phy_addr;


	/* ------------------------------------------------------- */
	/* for total = 32*8 = 256 VICs */
	/* for Y420CMDB bitmap */
	unsigned char bitmap_valid;
	unsigned char bitmap_length;
	unsigned char y420_all_vic;
	unsigned char y420cmdb_bitmap[Y420CMDB_MAX];
	/* ------------------------------------------------------- */
};

#endif  /* _HDMI_RX_GLOBAL_H */
