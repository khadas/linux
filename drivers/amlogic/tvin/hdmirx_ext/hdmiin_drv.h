/*
 * drivers/amlogic/tvin/hdmirx_ext/hdmiin_drv.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#ifndef __HDMIIN_H__
#define __HDMIIN_H__

#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/hrtimer.h>
#include <linux/time.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/tvin/tvin_v4l2.h>
#include "../tvin_frontend.h"

#define HDMIIN_DRV_VER "Jul042016.1"

#define HDMIIN_DRV_NAME "hdmirx_ext"

#ifdef CONFIG_TVIN_HDMI_EXT_SII9135
#define HDMIIN_DEV_NAME "sii9135"
#else
#define HDMIIN_DEV_NAME "hdmirx_ext"

#endif

#define HDMIIN_PE(fmt, args...) pr_info("hdmirx_ext.error: " fmt, ## args)
#define HDMIIN_PW(fmt, args...) pr_info("hdmirx_ext.warning: " fmt, ## args)
#define HDMIIN_PI(fmt, args...) pr_info("hdmirx_ext: " fmt, ## args)
#define HDMIIN_PD(fmt, args...) pr_info("hdmirx_ext.debug: " fmt, ## args)
#define printf(fmt, args...) pr_info("hdmirx_ext.debug: " fmt, ## args)

#define HDMIIN_PFUNC()  \
	pr_info("hdmirx_ext func: [%s] line: %d\n", __func__, __LINE__)

struct hdmiin_status_s {
	unsigned int  cable; /* whether the hdmi cable is inserted. */
	unsigned int  signal; /* whether the valid signal is got. */
	unsigned int  video_mode; /* format of the output video  */
	unsigned int  audio_sr; /* audio sample rate of the output audio */
};

struct hdmiin_gpio_s {
	char name[15];
	struct gpio_desc *gpio;
	int flag;
};

struct hdmiin_hw_s {
	/* int i2c_addr_pull; */
	unsigned char i2c_addr;
	int i2c_bus_index;
	struct i2c_client *i2c_client;

	/*unsigned int     reset_gpio;*/
	struct hdmiin_gpio_s reset_gpio;

	struct pinctrl *pin;
	unsigned char pinmux_flag;
};

struct hdmiin_vdin_s {
	struct tvin_frontend_s frontend;
	struct vdin_parm_s     parm;
	unsigned int           started;
};

struct video_timming_s {
	int h_active;
	int h_total;
	int hs_frontporch;
	int hs_width;
	int hs_backporch;
	int v_active;
	int v_total;
	int vs_frontporch;
	int vs_width;
	int vs_backporch;
	int mode;
	int hs_pol;
	int vs_pol;
};

struct hdmiin_drv_s {
	char name[20];
	int state; /* 1=on, 0=off */
	struct device *dev;
	struct cdev		cdev;
	struct hdmiin_status_s	status;
	struct hdmiin_hw_s	hw;
	struct vdin_v4l2_ops_s *vops;

	struct {
	/* signal stauts */
	int (*get_cable_status)(void);
	int (*get_signal_status)(void);
	int (*get_input_port)(void);
	void (*set_input_port)(int port);

	/* signal timming */
	int (*get_video_timming)(struct video_timming_s *ptimming);

	/* hdmi/dvi mode */
	int (*is_hdmi_mode)(void);

	/* audio mode */
	int (*get_audio_sample_rate)(void);

	/* debug interface
	// it should support the following format:
	// read registers:	r device_address register_address
	// write registers:	w device_address register_address value
	// dump registers:	dump device_address
				register_address_start register_address_end
	// video timming:	vinfo
	// cmd format help:	help	*/
	int (*debug)(char *buf);

	/* chip id and driver version */
	char* (*get_chip_version)(void);

	/* hardware init related */
	int (*init)(struct hdmiin_drv_s *hdrv);
	} hw_ops;

	struct hdmiin_vdin_s vdin;

	/* 0 to disable from user
	// 1 to enable, driver will trigger to vdin-stop
	// 2 to enable, driver will trigger to vdin-start
	// 3 to enable, driver will trigger to vdin-start/vdin-stop
	// 4 to enable, driver will not trigger to vdin-start/vdin-stop
	// 0xff to enable, and driver will NOT trigger signal-lost/vdin-stop,
			signal-get/vdin-start */
	unsigned int user_cmd;

	struct timer_list timer;
	int timer_cnt;
	void (*timer_func)(void);
	struct hrtimer hr_timer;
	long hrtimer_cnt;
	void (*hrtimer_func)(void);
};

/* according to the CEA-861-D */
enum SII9233_VIDEO_MODE_e {
	CEA_480P60	= 2,
	CEA_720P60	= 4,
	CEA_1080I60	= 5,
	CEA_480I60	= 6,

	CEA_1080P60	= 16,
	CEA_576P50	= 17,
	CEA_720P50	= 19,
	CEA_1080I50	= 20,
	CEA_576I50	= 21,

	CEA_1080P50	= 31,

	CEA_MAX = 60
};

extern struct hdmiin_drv_s *hdmiin_get_driver(void);
#if defined(CONFIG_TVIN_HDMI_EXT_SII9135)
extern void hdmiin_register_hw_sii9135_ops(struct hdmiin_drv_s *hdev);
#endif

extern int hdmiin_debug_print;

extern int it66021_register_tvin_frontend(struct tvin_frontend_s *frontend);

#endif
