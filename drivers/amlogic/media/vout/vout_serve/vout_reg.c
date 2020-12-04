// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vout/vclk_serve.h>
#include "vout_func.h"
#include "vout_reg.h"

/* ********************************
 * register access api
 * *********************************
 */
unsigned int vout_clk_read(unsigned int _reg)
{
	return vclk_clk_reg_read(_reg);
};

void vout_clk_write(unsigned int _reg, unsigned int _value)
{
	vclk_clk_reg_write(_reg, _value);
};

void vout_clk_setb(unsigned int _reg, unsigned int _value,
		   unsigned int _start, unsigned int _len)
{
	vout_clk_write(_reg, ((vout_clk_read(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}

unsigned int vout_clk_getb(unsigned int reg,
			   unsigned int _start, unsigned int _len)
{
	return (vout_clk_read(reg) >> _start) & ((1L << _len) - 1);
}

unsigned int vout_vcbus_read(unsigned int _reg)
{
	unsigned int ret = 0;

#ifdef CONFIG_AMLOGIC_VPU
	ret = vpu_vcbus_read(_reg);
#else
	ret = aml_read_vcbus(_reg);
#endif

	return ret;
};

void vout_vcbus_write(unsigned int _reg, unsigned int _value)
{
#ifdef CONFIG_AMLOGIC_VPU
	vpu_vcbus_write(_reg, _value);
#else
	aml_write_vcbus(_reg, _value);
#endif
};

void vout_vcbus_setb(unsigned int _reg, unsigned int _value,
		     unsigned int _start, unsigned int _len)
{
	vout_vcbus_write(_reg, ((vout_vcbus_read(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}

unsigned int vout_vcbus_getb(unsigned int reg,
			     unsigned int _start, unsigned int _len)
{
	return (vout_vcbus_read(reg) >> _start) & ((1L << _len) - 1);
}

