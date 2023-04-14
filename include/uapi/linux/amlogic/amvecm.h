/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _TVIN_EXT_H_
#define _TVIN_EXT_H_

/* Register table structure */
struct am_reg_s {
	unsigned int type; /* 32-bits; 0: CBUS; 1: APB BUS... */
	unsigned int addr; /* 32-bits; Register address */
	unsigned int mask; /* 32-bits; Valid bits */
	unsigned int  val; /* 32-bits; Register Value */
};

#define am_reg_size 900
struct am_regs_s {
	unsigned int    length; /* Length of total am_reg */
	struct am_reg_s am_reg[am_reg_size];
};

#endif
