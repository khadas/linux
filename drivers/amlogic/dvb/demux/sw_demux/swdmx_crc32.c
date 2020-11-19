// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "swdemux_internal.h"

static u32 crc32_table[256];

static void crc32_table_init(void)
{
	u32 i, j, k;

	for (i = 0; i < 256; i++) {
		k = 0;
		for (j = (i << 24) | 0x800000; j != 0x80000000; j <<= 1)
			k = (k << 1) ^ (((k ^ j) & 0x80000000)
					? 0x04c11db7 : 0);
		crc32_table[i] = k;
	}
}

u32 swdmx_crc32(const u8 *p, int len)
{
	u32 i_crc = 0xffffffff;

	if (crc32_table[1] == 0)
		crc32_table_init();

	while (len) {
		i_crc = (i_crc << 8) ^ crc32_table[(i_crc >> 24) ^ *p];
		p++;
		len--;
	}
	return i_crc;
}
