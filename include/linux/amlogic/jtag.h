/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __INCLUDE_AMLOGIC_JTAG_H
#define __INCLUDE_AMLOGIC_JTAG_H

#include <linux/types.h>

#define AMLOGIC_JTAG_STATE_ON		0
#define AMLOGIC_JTAG_STATE_OFF		1

#define AMLOGIC_JTAG_DISABLE		(-1)

#define AMLOGIC_JTAG_ON			0x82000040
#define AMLOGIC_JTAG_OFF		0x82000041

#ifdef CONFIG_AMLOGIC_JTAG_MESON
bool is_jtag_disable(void);
bool is_jtag_a(void);
bool is_jtag_b(void);
#else
static inline bool is_jtag_disable(void) { return true; }
static inline bool is_jtag_a(void) { return false; }
static inline bool is_jtag_b(void) { return false; }
#endif

#endif
