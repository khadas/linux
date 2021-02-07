// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_DVB_EXTERN_DRIVER_H__
#define __AML_DVB_EXTERN_DRIVER_H__

#include <linux/device.h>
#include <dvb_frontend.h>

struct dvb_extern_device {
	char *name;
	struct class class;
	struct device *dev;

	struct proc_dir_entry *debug_proc_dir;

	/* for debug. */
	struct dvb_frontend fe;
	struct analog_parameters para;

	struct gpio_config dvb_power;

	int tuner_num;
	int tuner_cur;
	int tuner_cur_attached;

	int demod_num;
	int demod_cur;
	int demod_cur_attached;
};

#endif /* __AML_DVB_EXTERN_DRIVER_H__ */
