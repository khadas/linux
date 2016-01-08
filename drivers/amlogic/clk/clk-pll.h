/*
 * drivers/amlogic/clk/clk-pll.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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


#ifndef __AMLOGIC_CLK_PLL_H
#define __AMLOGIC_CLK_PLL_H

extern void __init hdmi_clk_init(void __iomem *reg_base_cbus);
extern void __init gxbb_hdmi_clk_init(void __iomem *reg_base_cbus);
extern void __init gxtvbb_hdmi_clk_init(void __iomem *reg_base_cbus);

enum amlogic_pll_type {
	pll_1500,
	pll_2500,
	pll_2550,
	pll_3000_lvds,
	pll_3000_clk,
};

/* NOTE: Rate table should be kept sorted in descending order. */

struct amlogic_pll_rate_table {
	unsigned long rate;
	unsigned int prate;
	unsigned int m;         /* aml */
	unsigned int n;         /* aml */
	unsigned int od;        /* aml */
	unsigned int od2;        /* aml */
	unsigned int ext_div_n; /* aml */
	unsigned int afc_dsel_bp_en; /* aml */
	unsigned int afc_dsel_bp_in; /* aml */
};



#define PLL_2500_RATE(_rate, _m, _n, _od, _ext_div_n, _afc_dsel_bp_en, \
		_afc_dsel_bp_in)  \
	{							\
		.rate	=	(_rate * 1000000),		\
		.m    = (_m),           \
		.n    = (_n),           \
		.od   = (_od),          \
		.ext_div_n = (_ext_div_n), \
		.afc_dsel_bp_en = (_afc_dsel_bp_en), \
		.afc_dsel_bp_in = (_afc_dsel_bp_in), \
	}

struct amlogic_clk_pll {
	struct clk_hw		hw;
	void __iomem		*lock_reg;
	void __iomem		*con_reg;
	enum amlogic_pll_type	type;
	unsigned int		rate_count;
	struct amlogic_pll_rate_table *rate_table;
};

#define HHI_GCLK_MPEG0    0x1050
#define HHI_GCLK_MPEG1    0x1051
#define HHI_GCLK_MPEG2    0x1052
#define HHI_GCLK_OTHER    0x1054
#define HHI_GCLK_AO       0x1055

#define HHI_VID_CLK_DIV   0x1059
#define HHI_MPEG_CLK_CNTL 0x105d

#define HHI_VID_CLK_CNTL  0x105f

#define HHI_HDMI_CLK_CNTL 0x1073


/*fixed pll registers*/
#define HHI_MPLL_CNTL  0x10a0
#define HHI_MPLL_CNTL2 0x10a1
#define HHI_MPLL_CNTL3 0x10a2
#define HHI_MPLL_CNTL4 0x10a3
#define HHI_MPLL_CNTL5 0x10a4

#define HHI_MPLL_CNTL10 0x10a9


/*vid pll registers*/
#define HHI_VID_PLL_CNTL 0x10c8
#define HHI_VID_PLL_CNTL2 0x10c9
#define HHI_VID_PLL_CNTL3 0x10ca
#define HHI_VID_PLL_CNTL4 0x10cb
#define HHI_VID_PLL_CNTL5 0x10cc
#define HHI_VID_PLL_CNTL6 0x10cd

#define HHI_VID2_PLL_CNTL2 0x10e1

/*gpll registers*/
#define HHI_GP_PLL_CNTL 0x1010
#define HHI_GP_PLL_CNTL2 0x1011
#define HHI_GP_PLL_CNTL3 0x1012
#define HHI_GP_PLL_CNTL4 0x1013
#define HHI_GP_PLL_CNTL5 0x1014

/* mali clk control*/
#define HHI_MALI_CLK_CNTL   0x106c

/*hdmi pll registers*/
#define HHI_HDMI_PLL_CNTL 0x107c
#define HHI_HDMI_PLL_CNTL1 0x107d
#define HHI_HDMI_PLL_CNTL2 0x107e
#define HHI_VID_DIVIDER_CNTL 0x1066


/*FIXED PLL/Multi-phase PLL */
#define M8_MPLL_CNTL     (0xc00009a9)
#define M8_MPLL_CNTL_2 (0xadc80000)
#define M8_MPLL_CNTL_3 (0x0a57ca21)
#define M8_MPLL_CNTL_4 (0x00010006)
#define M8_MPLL_CNTL_5 (0xa5500e1a)
#define M8_MPLL_CNTL_6 (0xf4454545)
#define M8_MPLL_CNTL_7 (0x00000000)
#define M8_MPLL_CNTL_8 (0x00000000)
#define M8_MPLL_CNTL_9 (0x00000000)
/*SYS PLL */
#define M8_SYS_PLL_CNTL_2 (0x59C88000)
#define M8_SYS_PLL_CNTL_3 (0xCA45B823)
#define M8_SYS_PLL_CNTL_4 (0x0001D407)
#define M8_SYS_PLL_CNTL_5 (0x00000870)

/*ddr ctrl reg
#define AM_DDR_PLL_CNTL     0xc8000400
#define AM_DDR_PLL_CNTL1    0xc8000404
#define AM_DDR_PLL_CNTL2    0xc8000408
#define AM_DDR_PLL_CNTL3    0xc800040c
#define AM_DDR_PLL_CNTL4    0xc8000410
#define AM_DDR_PLL_STS      0xc8000414
*/

/**
 * struct amlogic_pll_clock: information about pll clock
 * @id: platform specific id of the clock.
 * @dev_name: name of the device to which this clock belongs.
 * @name: name of this pll clock.
 * @parent_name: name of the parent clock.
 * @flags: optional flags for basic clock.
 * @con_offset: offset of the register for configuring the PLL.
 * @lock_offset: offset of the register for locking the PLL.
 * @type: Type of PLL to be registered.
 * @alias: optional clock alias name to be assigned to this clock.
 */
struct aml_pll_conf {
		unsigned int od_mask;
		unsigned int od_shift;
		unsigned int od2_mask;
		unsigned int od2_shift;
		unsigned int n_mask;
		unsigned int n_shift;
		unsigned int m_mask;
		unsigned int m_shift;
		unsigned int m_frac_shift;
		unsigned int m_frac_mask;
};
#define PLL_CONF(__co_ctrl_offset, __data) \
{ \
	.co_ctrl_offset = OFFSET(__co_ctrl_offset), \
	.data = __data, \
}
struct aml_pll_co_ctrl {
	unsigned int co_ctrl_offset;
	unsigned int data;
};
struct aml_pll_ctrl {
	unsigned int con_reg_offset;
	unsigned int lock_reg_offset;
	void __iomem *con_reg;
	void __iomem *lock_reg;
};
struct amlogic_pll_clock {
	struct clk_hw		hw;
	unsigned int		id;
	const char		*dev_name;
	const char		*name;
	const char		*parent_name;
	unsigned long		flags;
	unsigned int con_offset;
	struct aml_pll_ctrl *pll_ctrl;
	enum amlogic_pll_type	type;
	struct amlogic_pll_rate_table *rate_table;
	unsigned int rate_table_cnt;
	const char              *alias;
	struct aml_pll_conf *pll_conf;
	int (*waite_pll_lock)(struct amlogic_pll_clock *,
			      struct amlogic_pll_rate_table *);
	unsigned long (*pll_recalc_rate)(struct amlogic_pll_clock *,
					 unsigned long);
	struct aml_pll_co_ctrl *co_ctrl;
	unsigned int co_ctrl_num;
	unsigned int calc_no_od;
	struct clk_ops  *ops;
};

#define HDMI_PLL(_typ, _id, _name, _pname, _flags, _pll_ctrl,	\
		_alias, _waite_pll_lock, _rate_table, _pll_conf, \
		 _co_ctrl, _pll_recalc_rate)				\
{								\
		.id		= _id,					\
		.type		= _typ,					\
		.name		= _name,				\
		.parent_name	= _pname,				\
		.flags		= _flags|CLK_GET_RATE_NOCACHE,		\
		.pll_ctrl	= _pll_ctrl,				\
		.waite_pll_lock = _waite_pll_lock,     \
		.rate_table = _rate_table,      \
		.rate_table_cnt = ARRAY_SIZE(_rate_table), \
		.pll_conf = _pll_conf, \
		.co_ctrl = _co_ctrl, \
		.co_ctrl_num = ARRAY_SIZE(_co_ctrl), \
		.pll_recalc_rate = _pll_recalc_rate, \
}
static inline void aml_write_reg32(void __iomem *_reg,  u32 _value)
{
	writel_relaxed(_value, (_reg));
}
static inline void aml_set_reg32_bits(void __iomem *_reg,
		const uint32_t _value,
		const uint32_t _start,
		const uint32_t _len)
{
	writel_relaxed(((readl_relaxed(_reg) &
					~(((1L << (_len))-1) << (_start))) |
				(((_value)&((1L<<(_len))-1)) << (_start))),
			_reg);
}

#endif /* __AMLOGIC_CLK_PLL_H */
