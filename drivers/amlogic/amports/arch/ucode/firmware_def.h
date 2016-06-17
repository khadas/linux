/*
 * drivers/amlogic/amports/arch/m8_m8m2/firmware_def.h
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

#ifndef FIRMWARE_DEFFAULT_CONFIG
#define FIRMWARE_DEFFAULT_CONFIG
#include "../firmware.h"
#define FOR_CPUS {\
		MESON_CPU_MAJOR_ID_M8,\
		MESON_CPU_MAJOR_ID_M8M2,\
		MESON_CPU_MAJOR_ID_GXBB,\
		MESON_CPU_MAJOR_ID_GXTVBB,\
		MESON_CPU_MAJOR_ID_GXL,\
		MESON_CPU_MAJOR_ID_GXM,\
		MESON_CPU_MAJOR_ID_TXL,\
		0}

#endif
