/* SPDX-License-Identifier: (GPL-2.0+ OR MIT)*/
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_PQ_PROC_H__
#define __VPP_PQ_PROC_H__

enum pq_module_s {
	PQ_SR0 = 0,
	PQ_SR1,
	PQ_DNLP,
	PQ_CC,
	PQ_BLE,
	PQ_CM,
	PQ_BLS,
	PQ_BRI,
	PQ_CTST,
	PQ_SAT,
	PQ_HUE,
	PQ_BRI2,
	PQ_CTST2,
	PQ_SAT2,
	PQ_HUE2,
	PQ_3DLUT,
	PQ_PRE_GMA,
	PQ_WB,
	PQ_GMA
};

enum vpp_csc_e {
	VADJ1_CSC = 0,
	PST2_CSC,
	PST_CSC,
};

struct regs_s {
	unsigned int type;
	unsigned int addr;
	unsigned int mask;
	unsigned int value;
};

struct pq_regs_s {
	enum pq_module_s pq_mod;
	unsigned int regs_num;
	struct regs_s *pq_reg;
};

void ioc_pq_load(void);
#endif

