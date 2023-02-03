/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_COMMON_H__
#define __VPP_COMMON_H__

/*Standard Linux headers*/
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/compat.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/video_sink/video.h>
#ifdef CONFIG_AMLOGIC_LCD
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif
#include "../../video_sink/vpp_pq.h"

#include "vpp_common_def.h"
#include "vpp_reg_io.h"
#include "vpp_drv.h"

/* Commom define */
#define PR_SYS          (0x01)
#define PR_IOC          (0x02)
#define PR_DEBUG        (0x04)
#define PR_DEBUG_CM     (0x08)
#define PR_DEBUG_DNLP   (0x10)
#define PR_DEBUG_GAMMA  (0x20)
#define PR_DEBUG_GO     (0x40)
#define PR_DEBUG_HDR    (0x80)
#define PR_DEBUG_LC     (0x100)
#define PR_DEBUG_LUT3D  (0x200)
#define PR_DEBUG_MATRIX (0x400)
#define PR_DEBUG_METER  (0x800)
#define PR_DEBUG_SR     (0x1000)
#define PR_DEBUG_VADJ   (0x2000)
#define PR_DEBUG_VE     (0x4000)
#define PR_DEBUG_AI     (0x8000)

#define LUT_SIZE_EOTF      (33)
#define LUT_SIZE_OETF_OSD  (41)

extern unsigned int pr_lvl;

#define pr_vpp(level, fmt, args ...)\
	do {\
		if (level & pr_lvl)\
			pr_info("vpp:" fmt, ##args);\
	} while (0)

#define PR_ERR(fmt, args ...)  pr_info("vpp_err:" fmt, ##args)
#define PR_DRV(fmt, args ...)  pr_info("vpp_drv:" fmt, ##args)

#ifndef MIN
#define MIN(a, b) \
	({typeof(a) x = (a);\
	typeof(b) y = (b);\
	(x < y) ? (x) : (y);\
	})
#endif

#ifndef MAX
#define MAX(a, b) \
	({typeof(a) x = (a);\
	typeof(b) y = (b);\
	(x > y) ? (x) : (y);\
	})
#endif

#ifndef XOR
#define XOR(a, b) \
	({typeof(a) x = (a);\
	typeof(b) y = (b);\
	(y & (~x)) + ((~y) & x);\
	})
#endif

enum vpp_dump_module_info_e {
	EN_DUMP_INFO_REG = 0,
	EN_DUMP_INFO_DATA,
};

/*ai detected scenes*/
enum vpp_detect_scene_e {
	EN_BLUE_SCENE = 0,
	EN_GREEN_SCENE,
	EN_SKIN_TONE_SCENE,
	EN_PEAKING_SCENE,
	EN_SATURATION_SCENE,
	EN_DYNAMIC_CONTRAST_SCENE,
	EN_NOISE_SCENE,
	EN_SCENE_MAX,
};

/*ai detected single scene process*/
struct vpp_single_scene_s {
	bool enable;
	int (*func)(int offset, bool enable);
};

/*Common functions*/
int vpp_check_range(int val, int down, int up);
int vpp_mask_int(int val, int start, int len);
int vpp_insert_int(int src_val, int insert_val, int start, int len);

#endif

