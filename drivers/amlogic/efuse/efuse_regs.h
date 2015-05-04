/*
 * drivers/amlogic/efuse/efuse_regs.h
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

#ifndef __EFUSE_REG_H
#define __EFUSE_REG_H

/* #define EFUSE_DEBUG */

#define	M8_EFUSE_VERSION_OFFSET			509
#define	M8_EFUSE_VERSION_ENC_LEN		1
#define	M8_EFUSE_VERSION_DATA_LEN		1
#define	M8_EFUSE_VERSION_BCH_EN			0
#define	M8_EFUSE_VERSION_BCH_REVERSE	0
#define M8_EFUSE_VERSION_SERIALNUM_V1	20

enum efuse_socchip_type_e {
	EFUSE_SOC_CHIP_M1 = 1,
	EFUSE_SOC_CHIP_M3,
	EFUSE_SOC_CHIP_M6,
	EFUSE_SOC_CHIP_M6TV,
	EFUSE_SOC_CHIP_M6TVLITE,
	EFUSE_SOC_CHIP_M8,
	EFUSE_SOC_CHIP_M6TVD,
	EFUSE_SOC_CHIP_M8BABY,
	EFUSE_SOC_CHIP_UNKNOW,
};

#endif
