/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2020 Amlogic, Inc. All rights reserved.
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
#ifndef AML_MSYNC_H
#define AML_MSYNC_H

#ifndef __KERNEL__
#include <stdint.h>
#endif

struct pts_tri {
	uint32_t wall_clock;
	uint32_t pts;
	uint32_t delay;
	uint64_t mono_ts;
};

struct pcr_pair {
	uint32_t pts;
	uint64_t mono_clock;
};

struct pts_wall {
	uint32_t wall_clock;
	uint32_t interval;
	uint32_t dis_delay;
};

enum av_sync_mode {
	AVS_MODE_A_MASTER,
	AVS_MODE_V_MASTER,
	AVS_MODE_PCR_MASTER,
	AVS_MODE_IPTV,
	AVS_MODE_FREE_RUN
};

enum av_sync_stat {
	AVS_STAT_INIT,
	AVS_STAT_STARTING,
	AVS_STAT_STARTED,
	AVS_STAT_PAUSED,
	AVS_STAT_TRANSITION,
	AVS_STAT_TRANSITION_V_2_A,
	AVS_STAT_TRANSITION_A_2_V
};

enum src_flag {
	SRC_V = 1,
	SRC_A = 2,
};

struct session_sync_stat {
	uint32_t v_active;
	uint32_t a_active;
	/* enum av_sync_mode */
	uint32_t mode;
	/* valid in aligned sync */
	uint32_t v_timeout;
	/* in audio switching process */
	uint32_t audio_switch;
	/* input: query source src_flag */
	uint32_t flag;
	/* enum av_sync_stat */
	uint32_t stat;
	/* clean poll stat after stat */
	uint32_t clean_poll;
};

enum avs_event {
	AVS_VIDEO_START,
	AVS_PAUSE,
	AVS_RESUME,
	AVS_VIDEO_STOP,
	AVS_AUDIO_STOP,
	AVS_VIDEO_TSTAMP_DISCONTINUITY,
	AVS_AUDIO_TSTAMP_DISCONTINUITY,
	AVS_AUDIO_SWITCH,
	AVS_EVENT_MAX,
};

enum avs_astart_mode {
	AVS_START_SYNC = 0,
	AVS_START_ASYNC,
	AVS_START_AGAIN,
	AVS_START_MAX
};

struct audio_start {
	/* in first audio pts */
	uint32_t pts;
	/* in render delay */
	uint32_t delay;
	/* out enum avs_astart_mode */
	uint32_t mode;
};

struct session_event {
	/* enum avs_event */
	uint32_t event;
	uint32_t value;
};

struct session_debug {
	uint32_t debug_freerun;
	uint32_t pcr_init_flag;
	uint32_t pcr_init_mode;
};

struct ker_start_policy {
	/* start policy */
	uint32_t policy;
	/* in audio timeout value for no video case, in ms
	 * -1 means ignore this value
	 */
	int timeout;
};

#define AVS_INVALID_PTS 0xFFFFFFFFUL

#define AMSYNC_START_V_FIRST 0x1
#define AMSYNC_START_A_FIRST 0x2
#define AMSYNC_START_ASAP  0x3
#define AMSYNC_START_ALIGN 0x4

/* msync ioctl */
#define _A_M_S  'M'
#define AMSYNC_IOC_ALLOC_SESSION   _IOW((_A_M_S), 0x00, int)
#define AMSYNC_IOC_REMOVE_SESSION   _IOR((_A_M_S), 0x01, int)

/* session ioctl */
#define _A_M_SS  'S'
#define AMSYNCS_IOC_SET_MODE		_IOW((_A_M_SS), 0x00, unsigned int)
#define AMSYNCS_IOC_GET_MODE		_IOR((_A_M_SS), 0x01, unsigned int)
#define AMSYNCS_IOC_SET_START_POLICY	_IOW((_A_M_SS), 0x02, struct ker_start_policy)
#define AMSYNCS_IOC_GET_START_POLICY	_IOR((_A_M_SS), 0x03, struct ker_start_policy)
#define AMSYNCS_IOC_SET_V_TS		_IOW((_A_M_SS), 0x04, struct pts_tri)
#define AMSYNCS_IOC_GET_V_TS		_IOWR((_A_M_SS), 0x05, struct pts_tri)
#define AMSYNCS_IOC_SET_A_TS		_IOW((_A_M_SS), 0x06, struct pts_tri)
#define AMSYNCS_IOC_GET_A_TS		_IOWR((_A_M_SS), 0x07, struct pts_tri)
#define AMSYNCS_IOC_SEND_EVENT		_IOWR((_A_M_SS), 0x08, struct session_event)
//For PCR/IPTV mode only
#define AMSYNCS_IOC_GET_SYNC_STAT	_IOWR((_A_M_SS), 0x09, struct session_sync_stat)
#define AMSYNCS_IOC_SET_PCR		_IOW((_A_M_SS), 0x0a, struct pcr_pair)
#define AMSYNCS_IOC_GET_PCR		_IOWR((_A_M_SS), 0x0b, struct pcr_pair)
#define AMSYNCS_IOC_GET_WALL		_IOR((_A_M_SS), 0x0c, struct pts_wall)
#define AMSYNCS_IOC_SET_RATE		_IOW((_A_M_SS), 0x0d, unsigned int)
#define AMSYNCS_IOC_GET_RATE		_IOR((_A_M_SS), 0x0e, unsigned int)
#define AMSYNCS_IOC_SET_NAME		_IOR((_A_M_SS), 0x0f, char *)
#define AMSYNCS_IOC_SET_WALL_ADJ_THRES	_IOW((_A_M_SS), 0x10, unsigned int)
#define AMSYNCS_IOC_GET_WALL_ADJ_THRES	_IOR((_A_M_SS), 0x11, unsigned int)
#define AMSYNCS_IOC_GET_CLOCK_START	_IOR((_A_M_SS), 0x12, unsigned int)
#define AMSYNCS_IOC_AUDIO_START	_IOW((_A_M_SS), 0x13, struct audio_start)
#define AMSYNCS_IOC_SET_CLK_DEV	_IOW((_A_M_SS), 0x14, int)
#define AMSYNCS_IOC_GET_CLK_DEV	_IOR((_A_M_SS), 0x15, int)
#define AMSYNCS_IOC_SET_STOP_AUDIO_WAIT	_IOR((_A_M_SS), 0x16, int)

//For debuging
#define AMSYNCS_IOC_GET_DEBUG_MODE	_IOR((_A_M_SS), 0x100, struct session_debug)

int msync_vsync_update(void);

#endif
