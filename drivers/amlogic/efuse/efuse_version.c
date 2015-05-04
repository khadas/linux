/*
 * drivers/amlogic/efuse/efuse_version.c
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

#include <linux/types.h>
/* #include <mach/am_regs.h> */
#include "efuse_regs.h"
#include "efuse.h"
/* #include <linux/amlogic/efuse-amlogic.h> */


/**
 * efuse version 0.1 (for M3 )
 * M3 efuse: read all free efuse data maybe fail on addr 0 and addr 0x40
 * so M3 EFUSE layout avoid using 0 and 0x40
title    offset datasize checksize totalsize
reserved  0       0        0        4
usid      4       33       2        35
mac_wifi  39      6        1        7
mac_bt    46      6        1        7
mac       53      6        1        7
licence   60      3        1        4
reserved  64      0        0        4
hdcp      68      300      10       310
reserved  378     0        0        2
version   380     3        1        4 (version+machid, version=1)
*/

/* m8 efuse layout according to haixiang.bao allocation */
static struct efuseinfo_item_t efuseinfo_M8_serialNum_v1[] = {
	{
		.title = "licence",
		.id = EFUSE_LICENCE_ID,
		.offset = 0,
		.enc_len = 4,
		.data_len = 4,
		.bch_en = 0,
		.bch_reverse = 0,
	},
	{
		.title = "nandextcmd",
		.id = EFUSE_NANDEXTCMD_ID,
		.offset = 4,
		.enc_len = 16,
		.data_len = 16,
		.bch_en = 0,
		.bch_reverse = 0,
	},
	{
		.title = "mac",  /* for the main network interface */
		.id = EFUSE_MAC_ID,
		.offset = 436,
		.enc_len = 6,
		.data_len = 6,
		.bch_en = 0,
		.bch_reverse = 0,
	},
	{
		/* for the second network interface or bt */
		.title = "mac_bt",
		.id = EFUSE_MAC_BT_ID,
		.offset = 442,
		.enc_len = 6,
		.data_len = 6,
		.bch_en = 0,
		.bch_reverse = 0,
	},
	{
		/* for the second network interface or bt */
		.title = "mac_wifi",
		.id = EFUSE_MAC_WIFI_ID,
		.offset = 448,
		.enc_len = 6,
		.data_len = 6,
		.bch_en = 0,
		.bch_reverse = 0,
	},
	{
		.title = "usid",
		.id = EFUSE_USID_ID,
		.offset = 454,
		.enc_len = 48,
		.data_len = 48,
		.bch_en = 0,
		.bch_reverse = 0,
	},
	{
		.title = "version",
		.id = EFUSE_VERSION_ID,
		.offset = M8_EFUSE_VERSION_OFFSET, /* 509 */
		.enc_len = M8_EFUSE_VERSION_ENC_LEN,
		.data_len = M8_EFUSE_VERSION_DATA_LEN,
		.bch_en = M8_EFUSE_VERSION_BCH_EN,
		.bch_reverse = M8_EFUSE_VERSION_BCH_REVERSE,
	},
};


struct efuseinfo_t efuseinfo[] = {
	{
		.efuseinfo_version = efuseinfo_M8_serialNum_v1,
		.size = sizeof(efuseinfo_M8_serialNum_v1)/
						sizeof(struct efuseinfo_item_t),
		.version = M8_EFUSE_VERSION_SERIALNUM_V1,
	},
};

int efuseinfo_num = sizeof(efuseinfo)/sizeof(struct efuseinfo_t);


