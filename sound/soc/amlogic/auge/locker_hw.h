/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_AUDIO_LOCKER_HW_H__
#define __AML_AUDIO_LOCKER_HW_H__
#include <linux/clk.h>

#include "regs.h"
#include "iomap.h"

void audiolocker_disable(void);
void audiolocker_irq_config(void);
void audiolocker_update_clks(struct clk *clk_calc, struct clk *clk_ref);
#endif
