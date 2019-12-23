/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _OSD_CLONE_H_
#define _OSD_CLONE_H_

#include "osd_log.h"

#ifdef CONFIG_AMLOGIC_MEDIA_GE2D
#define OSD_GE2D_CLONE_SUPPORT 1
#endif

#ifdef OSD_GE2D_CLONE_SUPPORT
void osd_clone_set_virtual_yres(u32 osd1_yres, u32 osd2_yres);
void osd_clone_get_virtual_yres(u32 *osd2_yres);
void osd_clone_set_angle(int angle);
void osd_clone_update_pan(int buffer_number);
int osd_clone_task_start(void);
void osd_clone_task_stop(void);
#else
static inline void osd_clone_set_virtual_yres(u32 osd1_yres, u32 osd2_yres) {}
static inline void osd_clone_get_virtual_yres(u32 *osd2_yres) {}
static inline void osd_clone_set_angle(int angle) {}
static inline void osd_clone_update_pan(int buffer_number) {}
static inline int osd_clone_task_start(void)
{
	osd_log_info("++ osd_clone depends on GE2D module!\n");
	return 0;
}

static inline void osd_clone_task_stop(void)
{
	osd_log_info("-- osd_clone depends on GE2D module!\n");
}
#endif

#endif
