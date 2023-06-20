/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_DSC_H_
#define _AML_DSC_H_

#include "sw_demux/swdemux.h"
#include "dvbdev.h"
#include <dmxdev.h>
#include <linux/device.h>

#define MAX_DSC_CHAN_NUM  20

struct dsc_channel;

struct aml_dsc {
	struct dvb_device *dev;
	int id;

	int sid;

	struct dsc_channel *dsc_channels;
	/*protect many user operate*/
	struct mutex mutex;
//	spinlock_t slock;
};

int dsc_init(struct aml_dsc *dsc, struct dvb_adapter *dvb_adapter);
void dsc_release(struct aml_dsc *dsc);
int dsc_set_source(int id, int source);
int dsc_set_sid(int id, int sid);
int dsc_dump_info(char *buf);
#endif
