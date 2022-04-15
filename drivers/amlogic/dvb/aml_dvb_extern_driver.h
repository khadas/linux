// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_DVB_EXTERN_DRIVER_H__
#define __AML_DVB_EXTERN_DRIVER_H__

#include <linux/device.h>
#include <dvb_frontend.h>

#define DVB_CFGDEV_BASEMINOR            1
#define DVB_CFGDEV_COUNT                1
#define DVB_CFGDEV_IOC_MAGIC             'y'
#define DVB_CFGDEV_IOC_SET_PARAM         _IOW(DVB_CFGDEV_IOC_MAGIC, 50, struct tuner_config)
#define DVB_CFGDEV_IOC_GET_PARAM         _IO(DVB_CFGDEV_IOC_MAGIC, 51)
#define COMPAT_DVB_CFGDEV_IOC_SET_PARAM  _IOW(DVB_CFGDEV_IOC_MAGIC, 50, struct compact_tuner_config)
#define COMPAT_DVB_CFGDEV_IOC_GET_PARAM  _IO(DVB_CFGDEV_IOC_MAGIC, 51)
#define DVB_CFGDEV_IOC_MAXNR             52

struct compact_tuner_config {
	u32 name;
	u32 code;
	u8 id;
	u8 i2c_addr;
	u8 i2c_id;
	u32 i2c_adap;
	u8 xtal;
	u32 xtal_cap;
	u8 xtal_mode;
	u8 lt_out;
	u8 dual_power;
	u8 if_agc;
	u8 if_hz;
	u8 if_invert;
	u8 if_amp;
	u8 detect;

	struct gpio_config reset;
	struct gpio_config power;

	u32 reserved0;
	u32 reserved1;
};

struct dvb_extern_device {
	char *name;
	struct class class;
	struct device *dev;

	struct proc_dir_entry *debug_proc_dir;

	/* for debug. */
	struct dvb_frontend *tuner_fe;
	struct dvb_frontend *demod_fe;
	struct analog_parameters para;

	struct gpio_config dvb_power;

	int tuner_num;
	int tuner_cur;
	int tuner_cur_attached;

	int demod_num;
	int demod_cur;
	int demod_cur_attached;

	struct work_struct work;

	/* to set config from userspace */
	struct device cdev;
	dev_t cfgdevno;
};

#endif /* __AML_DVB_EXTERN_DRIVER_H__ */
