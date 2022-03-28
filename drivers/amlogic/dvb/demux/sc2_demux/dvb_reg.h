/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DVB_REG_H_
#define _DVB_REG_H_

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/platform_device.h>

void aml_write_self(unsigned int reg, unsigned int val);
int aml_read_self(unsigned int reg);
int init_demux_addr(struct platform_device *pdev);

#define WRITE_CBUS_REG(_r, _v)   aml_write_self((_r), _v)
#define READ_CBUS_REG(_r)        aml_read_self((_r))

#endif
