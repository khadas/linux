/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AMLOGIC_MBOX_H__
#define __AMLOGIC_MBOX_H__

#define MAILBOX_AOCPU	1
#define MAILBOX_DSP	2
#define MAILBOX_SECPU	3
#define MAILBOX_MF	4

// P1 MBOX DEST
#define P1_DSPA2REE     0
#define P1_REE2DSPA     1
#define P1_DSPB2REE     2
#define P1_REE2DSPB     3
#define P1_AO2REE       4
#define P1_REE2AO       5
#define P1_BL40A2REE    6
#define P1_REE2BL40A    7
#define P1_BL40B2REE    8
#define P1_REE2BL40B    9

// A5 MAILBOX DEST
#define A5_AO2REE       0
#define A5_REE2AO       1
#define A5_DSP2REE      2
#define A5_REE2DSP      3

// S5 MAILBOX DEST
#define S5_AO2REE       0
#define S5_REE2AO       1

// A4 MAILBOX DEST
#define A4_AO2REE       0
#define A4_REE2AO       1
#define A4_DSP2REE      2
#define A4_REE2DSP      3
#endif /* __AMLOGIC_MBOX_H__ */
