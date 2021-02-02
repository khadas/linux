/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __REG_DTV_DEMOD_H__
#define __REG_DTV_DEMOD_H__
/* common frontend */
#define AFIFO_ADC		(0x20)
#define ADC_2S_COMPLEMENT_BIT	8
#define ADC_2S_COMPLEMENT_WID	1
#define AFIFO_NCO_RATE_BIT	0
#define AFIFO_NCO_RATE_WID	8

#define SFIFO_OUT_LENS		(0x2f)

#define TEST_BUS		(0x39)
#define DC_ARB_EN_BIT		30
#define DC_ARB_EN_WID		1

#define TEST_BUS_VLD		(0x3b)

/* j.83b fec */
#define J83B_FEC_TS_CLK		(0x84)
#define PARALLEL_CLK_DIV_BIT	16
#define PARALLEL_CLK_DIV_WID	6
#endif
