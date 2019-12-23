/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __TXLX_REG_H__
#define __TXLX_REG_H__
#include "reg_ops.h"

#define RESET0_REGISTER 0x401
#define P_RESET0_REGISTER CBUS_REG_ADDR(RESET0_REGISTER)
#define RESET2_REGISTER 0x403
#define P_RESET2_REGISTER CBUS_REG_ADDR(RESET2_REGISTER)

#define ISA_DEBUG_REG0 0x3c00
#define P_ISA_DEBUG_REG0 CBUS_REG_ADDR(ISA_DEBUG_REG0)

#endif
