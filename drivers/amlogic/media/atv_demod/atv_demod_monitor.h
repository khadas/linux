/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __ATV_DEMOD_MONITOR_H__
#define __ATV_DEMOD_MONITOR_H__

#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/mutex.h>


#define MONI_DISABLE (0)
#define MONI_ENABLE  (1)
#define MONI_PAUSE   (2)

struct atv_demod_monitor {
	struct work_struct work;
	struct timer_list timer;

	struct dvb_frontend *fe;

	struct mutex mtx;

	int state;
	bool lock;

	unsigned int lock_cnt;

	void (*disable)(struct atv_demod_monitor *monitor);
	void (*enable)(struct atv_demod_monitor *monitor);
	void (*pause)(struct atv_demod_monitor *monitor);
};

extern void atv_demod_monitor_init(struct atv_demod_monitor *monitor);


#endif /* __ATV_DEMOD_MONITOR_H__ */
