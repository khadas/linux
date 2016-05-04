/*
 * drivers/amlogic/amports/arch/m8_m8m2/avs/vavs_mc.c
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

#include "../firmware_def.h"

#define MicroCode vavs_mc
#include "avs_linux.h"
#undef MicroCode
#define MicroCode vavs_mc_debug
#include "avs_linux_debug.h"

#undef MicroCode
#define MicroCode vavs_mc_old
#include "avs_linux_old.h"

#define FOR_VFORMAT VFORMAT_AVS

#define REG_FIRMWARE_ALL()\
	do {\
		DEF_FIRMWARE(vavs_mc);\
		DEF_FIRMWARE(vavs_mc_debug);\
		DEF_FIRMWARE(vavs_mc_old);\
	} while (0)

INIT_DEF_FIRMWARE();
