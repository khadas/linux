/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX_DEBUG_REG_H_
#define __HDMI_TX_DEBUG_REG_H_

#include <linux/amlogic/media/vout/hdmi_tx/hdmi_tx_module.h>
#include "hdmi_tx_reg.h"
#include "mach_reg.h"
#include "reg_sc2.h"

#define REGS_END 0xffffffff

struct hdmitx_dbg_reg_s {
	const unsigned int (*rd_reg_func)(const unsigned int add);
	const unsigned int (*get_reg_paddr)(const unsigned int add);
	const char *name;
	const unsigned int *reg;
};

const struct hdmitx_dbg_reg_s **hdmitx_get_dbg_regs(enum amhdmitx_chip_e type);

#endif

