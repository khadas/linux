/*
 * drivers/amlogic/amports/arch/secprot.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#include "secprot.h"

int tee_config_device_secure(int dev_id, int secure)
{
	int ret = 0;
	register unsigned x0 asm("x0");
	register unsigned x1 asm("x1");
	register unsigned x2 asm("x2");

	x0 = OPTEE_SMC_CONFIG_DEVICE_SECURE;
	x1 = dev_id;
	x2 = secure;

	asm volatile(
		__asmeq("%0", "x0")
		__asmeq("%1", "x0")
		__asmeq("%2", "x1")
		__asmeq("%3", "x2")
		"smc    #0\n"
		: "=r"(x0)
		: "r"(x0), "r"(x1), "r"(x2));
	ret = x0;

	return ret;
}

