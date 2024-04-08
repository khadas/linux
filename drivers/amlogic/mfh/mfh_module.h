/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MFH_MODULE_H__
#define __MFH_MODULE_H__

void mfh_poweron(struct device *dev, int cpuid, bool poweron);
/*soc:g12a/sm1*/
#define SMC_SUBID_MFH_V1_BOOT	0x20
/*soc:p1*/
#define SMC_SUBID_MFH_V2_BOOT	0x21
#define SMC_SUBID_MFH_V2_RESET	0x22
#define SMC_SUBID_SHIFT		0x8
#define PACK_SMC_SUBID_ID(subid, id) (((subid) << SMC_SUBID_SHIFT) | (id))

enum m4_index {
	M4A = 0,
	M4B
};

enum m4_online_status {
	M4_NONE,
	M4A_ONLINE = 1,
	M4B_ONLINE,
	M4AB_ONLINE
};

enum m4_health_status {
	M4_GOOD,
	M4A_HANG = 1,
	M4B_HANG,
	M4AB_HANG
};

#endif /*__MFH_MODULE_H__*/
