// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/vout/cvbs/cvbs_out_reg.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/platform_device.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vout/vclk_serve.h>
#include "cvbs_out_reg.h"

/* ********************************
 * register access api
 * **********************************/
unsigned int cvbs_out_reg_read(unsigned int _reg)
{
#ifdef CONFIG_AMLOGIC_VPU
	return vpu_vcbus_read(_reg);
#else
	return aml_read_vcbus(_reg);
#endif
};

void cvbs_out_reg_write(unsigned int _reg, unsigned int _value)
{
#ifdef CONFIG_AMLOGIC_VPU
	return vpu_vcbus_write(_reg, _value);
#else
	return aml_write_vcbus(_reg, _value);
#endif
};

void cvbs_out_reg_setb(unsigned int reg, unsigned int value,
		       unsigned int _start, unsigned int _len)
{
	cvbs_out_reg_write(reg, ((cvbs_out_reg_read(reg) &
		(~(((1L << _len) - 1) << _start))) |
		((value & ((1L << _len) - 1)) << _start)));
}

unsigned int cvbs_out_reg_getb(unsigned int reg,
			       unsigned int _start, unsigned int _len)
{
	return (cvbs_out_reg_read(reg) >> _start) & ((1L << _len) - 1);
}

void cvbs_out_reg_set_mask(unsigned int reg, unsigned int _mask)
{
	cvbs_out_reg_write(reg, (cvbs_out_reg_read(reg) | (_mask)));
}

void cvbs_out_reg_clr_mask(unsigned int reg, unsigned int _mask)
{
	cvbs_out_reg_write(reg, (cvbs_out_reg_read(reg) & (~(_mask))));
}

unsigned int cvbs_out_hiu_read(unsigned int _reg)
{
#ifdef CONFIG_AMLOGIC_VOUT_CLK_SERVE
	return vclk_clk_reg_read(_reg);
#else
	return aml_read_hiubus(_reg);
#endif
}

void cvbs_out_hiu_write(unsigned int _reg, unsigned int _value)
{
#ifdef CONFIG_AMLOGIC_VOUT_CLK_SERVE
	vclk_clk_reg_write(_reg, _value);
#else
	aml_write_hiubus(_reg, _value);
#endif
}

void cvbs_out_hiu_setb(unsigned int _reg, unsigned int _value,
		       unsigned int _start, unsigned int _len)
{
	cvbs_out_hiu_write(_reg, ((cvbs_out_hiu_read(_reg) &
		(~(((1L << _len) - 1) << _start))) |
		((_value & ((1L << _len) - 1)) << _start)));
}

unsigned int cvbs_out_hiu_getb(unsigned int _reg,
			       unsigned int _start, unsigned int _len)
{
	return (cvbs_out_hiu_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void cvbs_out_hiu_set_mask(unsigned int _reg, unsigned int _mask)
{
	cvbs_out_hiu_write(_reg, (cvbs_out_hiu_read(_reg) | (_mask)));
}

void cvbs_out_hiu_clr_mask(unsigned int _reg, unsigned int _mask)
{
	cvbs_out_hiu_write(_reg, (cvbs_out_hiu_read(_reg) & (~(_mask))));
}

unsigned int cvbs_out_ana_read(unsigned int _reg)
{
#ifdef CONFIG_AMLOGIC_VOUT_CLK_SERVE
	return vclk_ana_reg_read(_reg);
#else
	return aml_read_hiubus(_reg);
#endif
}

void cvbs_out_ana_write(unsigned int _reg, unsigned int _value)
{
#ifdef CONFIG_AMLOGIC_VOUT_CLK_SERVE
	vclk_ana_reg_write(_reg, _value);
#else
	return aml_write_hiubus(_reg, _value);
#endif
}

void cvbs_out_ana_setb(unsigned int _reg, unsigned int _value,
		       unsigned int _start, unsigned int _len)
{
	cvbs_out_ana_write(_reg, ((cvbs_out_ana_read(_reg) &
		(~(((1L << _len) - 1) << _start))) |
		((_value & ((1L << _len) - 1)) << _start)));
}

unsigned int cvbs_out_ana_getb(unsigned int _reg,
			       unsigned int _start, unsigned int _len)
{
	return (cvbs_out_ana_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

