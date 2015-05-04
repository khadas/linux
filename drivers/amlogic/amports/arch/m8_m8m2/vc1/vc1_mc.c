/*
 * drivers/amlogic/amports/arch/m8_m8m2/vc1/vc1_mc.c
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

#define VERSTR "00000"

#define MicroCode vc1_mc
#include "vc1_vc1_linux.h"

#define FOR_VFORMAT VFORMAT_VC1

#define REG_FIRMWARE_ALL()\
		DEF_FIRMWARE_VER(vc1_mc, VERSTR);\

INIT_DEF_FIRMWARE();
