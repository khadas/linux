/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DSC_REG_
#define _DSC_REG_

#define LO_KTE_EVEN_00      0
#define LO_KTE_ODD          8
#define LO_ALGO             16
#define LO_PID_PART1        20

#define HI_PID_PART2        0
#define HI_SID              1
#define HI_EVEN_00_IV       7
#define HI_ODD_IV           13
#define HI_SCB_AS_IS        19
#define HI_SCB_OUT          20
#define HI_SCB00            22
#define HI_VALID            31

#define TSD_PID_READY		(SECURE_BASE + 0x2000)
#define TSD_PID_STATUS		(SECURE_BASE + 0x2000 + 0xc)
#define TSD_BASE_ADDR		(SECURE_BASE + 0x2000 + 0x10)

#define TSN_PID_READY		(SECURE_BASE + 0x2400)
#define TSN_PID_STATUS		(SECURE_BASE + 0x2400 + 0xc)
#define TSN_BASE_ADDR		(SECURE_BASE + 0x2400 + 0x10)

#define TSE_PID_READY		(SECURE_BASE + 0x2800)
#define TSE_PID_STATUS		(SECURE_BASE + 0x2800 + 0xc)
#define TSE_BASE_ADDR		(SECURE_BASE + 0x2800 + 0x10)

#endif
