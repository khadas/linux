/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_VCLK_H_
#define _AML_VCLK_H_

#ifdef CONFIG_AMLOGIC_VOUT_CLK_SERVE
unsigned int vclk_hiu_reg_read(unsigned int _reg);
void vclk_hiu_reg_write(unsigned int _reg, unsigned int _value);
void vclk_hiu_reg_setb(unsigned int _reg, unsigned int _value,
		       unsigned int _start, unsigned int _len);
unsigned int vclk_ana_reg_read(unsigned int _reg);
void vclk_ana_reg_write(unsigned int _reg, unsigned int _value);
void vclk_ana_reg_setb(unsigned int _reg, unsigned int _value,
		       unsigned int _start, unsigned int _len);
unsigned int vclk_ana_reg_getb(unsigned int _reg,
			       unsigned int _start, unsigned int _len);

unsigned int vclk_clk_reg_read(unsigned int _reg);
void vclk_clk_reg_write(unsigned int _reg, unsigned int _value);
void vclk_clk_reg_setb(unsigned int _reg, unsigned int _value,
		       unsigned int _start, unsigned int _len);
unsigned int vclk_clk_reg_getb(unsigned int _reg,
			       unsigned int _start, unsigned int _len);
#else
static inline unsigned int vclk_hiu_reg_read(unsigned int _reg)
{
	return 0;
}

static inline void vclk_hiu_reg_write(unsigned int _reg, unsigned int _value)
{
}

static inline void vclk_hiu_reg_setb(unsigned int _reg, unsigned int _value,
				     unsigned int _start, unsigned int _len)
{
}

static inline unsigned int vclk_hiu_reg_getb(unsigned int _reg,
					     unsigned int _start,
					     unsigned int _len)
{
	return 0;
}

static inline unsigned int vclk_ana_reg_read(unsigned int _reg)
{
	return 0;
}

static inline void vclk_ana_reg_write(unsigned int _reg, unsigned int _value)
{
}

static inline void vclk_ana_reg_setb(unsigned int _reg, unsigned int _value,
				     unsigned int _start, unsigned int _len)
{
}

static inline unsigned int vclk_ana_reg_getb(unsigned int _reg,
					     unsigned int _start,
					     unsigned int _len)
{
	return 0;
}

static inline unsigned int vclk_clk_reg_read(unsigned int _reg)
{
	return 0;
}

static inline void vclk_clk_reg_write(unsigned int _reg, unsigned int _value)
{
}

static inline void vclk_clk_reg_setb(unsigned int _reg, unsigned int _value,
				     unsigned int _start, unsigned int _len)
{
}

static inline unsigned int vclk_clk_reg_getb(unsigned int _reg,
					     unsigned int _start,
					     unsigned int _len)
{
	return 0;
}
#endif

#endif
