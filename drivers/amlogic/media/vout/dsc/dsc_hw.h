/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DSC_HW_H__
#define __DSC_HW_H__
#include "dsc_drv.h"

unsigned int R_DSC_REG(unsigned int reg);
void W_DSC_REG(unsigned int reg, unsigned int val);
unsigned int R_DSC_BIT(u32 reg, const u32 start, const u32 len);
void W_DSC_BIT(u32 reg, const u32 value, const u32 start, const u32 len);

unsigned int dsc_vcbus_reg_read(unsigned int _reg);
void dsc_vcbus_reg_write(unsigned int _reg, unsigned int _value);
void dsc_vcbus_reg_setb(unsigned int reg, unsigned int value,
			unsigned int _start, unsigned int _len);
unsigned int dsc_vcbus_reg_getb(unsigned int reg, unsigned int _start, unsigned int _len);

void dsc_config_register(struct aml_dsc_drv_s *dsc_drv);
void set_dsc_en(unsigned int enable);

#endif

