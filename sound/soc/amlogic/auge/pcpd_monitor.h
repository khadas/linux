/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __PCPD_MONITOR_H__
#define __PCPD_MONITOR_H__

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <linux/amlogic/pm.h>
#include <linux/amlogic/clk_measure.h>
#include <linux/amlogic/cpu_version.h>
#include "pcpd_monitor_table.h"

#include "ddr_mngr.h"
#include "audio_io.h"

struct pcpd_monitor {
	struct pcpd_monitor_chipinfo *chipinfo;
	struct device *dev;
	int pcpd_irq;
	int pcpd_id;
	struct toddr *tddr;
	struct aml_audio_controller *actrl;
};

int pcpd_monitor_check_audio_type(struct pcpd_monitor *pc_pd);
int aml_pcpd_monitor_enable(struct pcpd_monitor *pc_pd, int enable);
int aml_pcpd_monitor_init(struct pcpd_monitor *pc_pd);

#endif
