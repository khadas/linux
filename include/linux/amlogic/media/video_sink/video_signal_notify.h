/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_SIGNAL_NOTIFY_H
#define VIDEO_SIGNAL_NOTIFY_H
#include <linux/notifier.h>
#include <linux/list.h>

#define VIDEO_SIGNAL_TYPE_CHANGED   0x0001
#define VIDEO_SECURE_TYPE_CHANGED   0x0002
#define VIDEO_INFO_CHANGED          0x0003

/* video info event flags */
#define VIDEO_INFO_CHANGE_NONE      0x0000
#define VIDEO_SIZE_CHANGE_EVENT     0x0001
#define VIDEO_AISR_FRAME_EVENT      0x0004

enum vd_format_e {
	SIGNAL_INVALID = -1,
	SIGNAL_SDR = 0,
	SIGNAL_HDR10 = 1,
	SIGNAL_HLG = 2,
	SIGNAL_HDR10PLUS = 3,
	SIGNAL_DOVI = 4,
	SIGNAL_CUVA = 5,
};

struct vd_signal_info_s {
	enum vd_format_e signal_type;
	enum vd_format_e vd1_signal_type;
	enum vd_format_e vd2_signal_type;
	u32 reversed;
};

struct vd_info_s {
	u32 flags;
	u32 reversed;
};

int vd_signal_register_client(struct notifier_block *nb);
int vd_signal_unregister_client(struct notifier_block *nb);
int vd_signal_notifier_call_chain(unsigned long val, void *v);

#endif /* VIDEO_SIGNAL_NOTIFY_H */
