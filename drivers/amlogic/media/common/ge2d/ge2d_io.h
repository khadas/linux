/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _GE2D_IO_H_
#define _GE2D_IO_H_

#include <linux/io.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/registers/register_map.h>
#include <linux/amlogic/media/registers/regs/ao_regs.h>
#include <linux/amlogic/power_ctrl.h>
#include <linux/amlogic/power_domain.h>

#include "ge2d_log.h"
#include "ge2d_reg.h"

#define GE2DBUS_REG_ADDR(reg) ((((reg) - 0x1800) << 2))
extern unsigned int ge2d_dump_reg_cnt;
extern unsigned int ge2d_dump_reg_enable;
extern void __iomem *ge2d_reg_map;
void ge2d_runtime_pwr(int enable);

static int check_map_flag(void)
{
	int ret = 0;

	if (ge2d_reg_map) {
		ret = 1;
	} else {
		ge2d_log_err("%s reg map failed\n", __func__);
		ret = 0;
	}

	return ret;
}

static uint32_t ge2d_reg_read(unsigned int reg)
{
	unsigned int addr = 0;
	unsigned int val = 0;

	if (ge2d_meson_dev.chip_type < MESON_CPU_MAJOR_ID_GXBB)
#ifdef CONFIG_AMLOGIC_IOMAP
		return (uint32_t)aml_read_cbus(reg);
#else
		return 0;
#endif

	addr = GE2DBUS_REG_ADDR(reg);
	if (check_map_flag())
		val = readl(ge2d_reg_map + addr);

	ge2d_log_dbg2("read(0x%x)=0x%x\n", reg, val);

	return val;
}

static void ge2d_reg_write(unsigned int reg, unsigned int val)
{
	unsigned int addr = 0;

	if (ge2d_meson_dev.chip_type < MESON_CPU_MAJOR_ID_GXBB) {
#ifdef CONFIG_AMLOGIC_IOMAP
		aml_write_cbus(reg, val);
#endif
		return;
	}

	addr = GE2DBUS_REG_ADDR(reg);
	if (check_map_flag())
		writel(val, ge2d_reg_map + addr);

	if (ge2d_dump_reg_enable && ge2d_dump_reg_cnt > 0) {
		ge2d_log_info("write(0x%x) = 0x%x\n", reg, val);
		ge2d_dump_reg_cnt--;
	}
}

static inline u32 ge2d_vcbus_read(u32 reg)
{
#ifdef CONFIG_AMLOGIC_IOMAP
	return (u32)aml_read_vcbus(reg);
#else
	return 0;
#endif
};

static inline u32 ge2d_reg_get_bits(u32 reg,
				    const u32 start,
				    const u32 len)
{
	u32 val;

	val = (ge2d_reg_read(reg) >> (start)) & ((1L << (len)) - 1);
	return val;
}

static inline void ge2d_reg_set_bits(u32 reg,
				     const u32 value,
				     const u32 start,
				     const u32 len)
{
	ge2d_reg_write(reg, ((ge2d_reg_read(reg) &
			       ~(((1L << (len)) - 1) << (start))) |
			      (((value) & ((1L << (len)) - 1)) << (start))));
}

static void ge2d_hiu_setb(unsigned int _reg, unsigned int _value,
			  unsigned int _start, unsigned int _len)
{
	aml_write_hiubus(_reg, ((aml_read_hiubus(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}

static void ge2d_ao_setb(unsigned int _reg, unsigned int _value,
			 unsigned int _start, unsigned int _len)
{
	aml_write_aobus(_reg, ((aml_read_aobus(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}

static void ge2d_c_setb(unsigned int _reg, unsigned int _value,
			unsigned int _start, unsigned int _len)
{
	aml_write_cbus(_reg, ((aml_read_cbus(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}

static inline void ge2d_set_pwr_tbl_bits(unsigned int table_type,
					 unsigned int reg, unsigned int val,
					 unsigned int start, unsigned int len)
{
	switch (table_type) {
	case CBUS_BASE:
		ge2d_c_setb(reg, val, start, len);
		break;
	case AOBUS_BASE:
		ge2d_ao_setb(reg, val, start, len);
		break;
	case HIUBUS_BASE:
		ge2d_hiu_setb(reg, val, start, len);
		break;
	case GEN_PWR_SLEEP0:
		power_ctrl_sleep(val ? 0 : 1, start);
		break;
	case GEN_PWR_ISO0:
		power_ctrl_iso(val ? 0 : 1, start);
		break;
	case MEM_PD_REG0:
		power_ctrl_mempd0(val ? 0 : 1, 0xFF, start);
		break;
	case PWR_SMC:
		ge2d_log_err("PWR_SMC is not supported\n");
		break;
	case PWR_RUNTIME:
		ge2d_runtime_pwr(val);
		break;
	default:
		ge2d_log_err("unsupported bus type\n");
		break;
	}
}
#endif
