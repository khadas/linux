/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _OSD_ANTIFLICKER_H_
#define _OSD_ANTIFLICKER_H_

#include "osd_log.h"

#ifdef CONFIG_AMLOGIC_MEDIA_GE2D
#define OSD_GE2D_ANTIFLICKER_SUPPORT 1
#endif

#ifdef OSD_GE2D_ANTIFLICKER_SUPPORT
void osd_antiflicker_update_pan(u32 yoffset, u32 yres);
int osd_antiflicker_task_start(void);
void osd_antiflicker_task_stop(void);
void osd_antiflicker_enable(u32 enable);
#else
static inline void osd_antiflicker_enable(u32 enable) {}
static inline void osd_antiflicker_update_pan(u32 yoffset, u32 yres) {}
static inline int osd_antiflicker_task_start(void)
{
	osd_log_info("++ osd_antiflicker depends on GE2D module!\n");
	return 0;
}

static inline void osd_antiflicker_task_stop(void)
{
	osd_log_info("-- osd_antiflicker depends on GE2D module!\n");
}
#endif

#endif

