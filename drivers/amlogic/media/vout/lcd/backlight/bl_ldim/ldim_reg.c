// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "ldim_reg.h"
#include "../../lcd_reg.h"

void ldim_wr_vcbus(unsigned int addr, unsigned int val)
{
	lcd_vcbus_write(addr, val);
}
EXPORT_SYMBOL(ldim_wr_vcbus);

unsigned int ldim_rd_vcbus(unsigned int addr)
{
	return lcd_vcbus_read(addr);
}
EXPORT_SYMBOL(ldim_rd_vcbus);

void ldim_wr_vcbus_bits(unsigned int addr, unsigned int val,
			unsigned int start, unsigned int len)
{
	lcd_vcbus_setb(addr, val, start, len);
}
EXPORT_SYMBOL(ldim_wr_vcbus_bits);

unsigned int ldim_rd_vcbus_bits(unsigned int addr,
				unsigned int start, unsigned int len)
{
	return lcd_vcbus_getb(addr, start, len);
}
EXPORT_SYMBOL(ldim_rd_vcbus_bits);

//below for rdma write/read
void ldim_wr_reg_bits(unsigned int addr, unsigned int val,
				    unsigned int start, unsigned int len)
{
	unsigned int data;

	data = VSYNC_RD_MPEG_REG(addr);
	data = (data & (~(((1 << len) - 1) << start))) |
		((val & ((1 << len) - 1)) << start);
	VSYNC_WR_MPEG_REG(addr, data);
}
EXPORT_SYMBOL(ldim_wr_reg_bits);

void ldim_wr_reg(unsigned int addr, unsigned int val)
{
	VSYNC_WR_MPEG_REG(addr, val);
}
EXPORT_SYMBOL(ldim_wr_reg);

unsigned int ldim_rd_reg(unsigned int addr)
{
	return VSYNC_RD_MPEG_REG(addr);
}
EXPORT_SYMBOL(ldim_rd_reg);
