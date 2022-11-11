/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DVBC_FUNC_H__
#define __DVBC_FUNC_H__

enum qam_md_e {
	QAM_MODE_16,
	QAM_MODE_32,
	QAM_MODE_64,
	QAM_MODE_128,
	QAM_MODE_256,
	QAM_MODE_AUTO,
	QAM_MODE_NUM
};

#define QAM_MODE_CFG		0x2
#define SYMB_CNT_CFG		0x3
#define SR_OFFSET_ACC		0x8
#define SR_SCAN_SPEED		0xc
#define TIM_SWEEP_RANGE_CFG	0xe
#endif
