/*
 * Copyright (C) 2015 Amlogic
 *
 * Written by Xing Xu <xing.xu@amlogic.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 *
 */

#ifndef __SOC_IO_H
#define __SOC_IO_H

enum{
	IO_A9_PERIPH_BASE = 0,
	IO_MMC_BUS_BASE,
	IO_USB_A_BASE,
	IO_USB_B_BASE,
	IO_ETH_BASE,
	IO_CBUS_BASE,
	IO_AXI_BUS_BASE,
	IO_APB_BUS_BASE,
	IO_AOBUS_BASE,
	IO_BUS_MAX,
};
extern int aml_reg_read(u32 bus_type, unsigned int reg, unsigned int *val);
extern int aml_reg_write(u32 bus_type, unsigned int reg, unsigned int val);
extern int aml_regmap_update_bits(u32 bus_type,
			unsigned int reg, unsigned int mask,
			unsigned int val);


#endif
