/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __WT20_1811_H__
#define __WT20_1811_H__

#include "lnb_controller.h"

#define WT20_1811_CONFIG_VOLTAGE_LOW_13_333V        0
#define WT20_1811_CONFIG_VOLTAGE_LOW_13_667V        1
#define WT20_1811_CONFIG_VOLTAGE_LOW_14_333V        2
#define WT20_1811_CONFIG_VOLTAGE_LOW_15_667V        3
#define WT20_1811_CONFIG_VOLTAGE_HIGH_18_667V      10
#define WT20_1811_CONFIG_VOLTAGE_HIGH_19_000V      11
#define WT20_1811_CONFIG_VOLTAGE_HIGH_19_667V      12
#define WT20_1811_CONFIG_VOLTAGE_HIGH_20_000V      13

extern int lnb_high_voltage;

int wt20_1811_create(struct lnbc *lnbc, struct i2c_adapter *i2c_adap,
		unsigned char i2c_addr);

#define LNBC_DIAG_STATUS_DIS             0x1
#define LNBC_DIAG_STATUS_CPOK            0x2
#define LNBC_DIAG_STATUS_OCP             0x4
#define LNBC_DIAG_STATUS_UNUSED1         0x8
#define LNBC_DIAG_STATUS_PNG             0x10
#define LNBC_DIAG_STATUS_UNUSED2         0x20
#define LNBC_DIAG_STATUS_TSD             0x40
#define LNBC_DIAG_STATUS_UVLO            0x40

#endif /* __WT20_1811_H__ */
