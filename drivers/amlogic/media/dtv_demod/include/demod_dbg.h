/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DTV_DMD_DBG_H
#define __DTV_DMD_DBG_H
#include <linux/bitops.h>

#define CAP_SIZE (50)
#define PER_WR	(BIT(20))

#define ADC_PLL_CNTL0_TL1	(0xb0 << 2)
#define ADC_PLL_CNTL1_TL1	(0xb1 << 2)
#define ADC_PLL_CNTL2_TL1	(0xb2 << 2)

enum {
	ATSC_READ_STRENGTH = 0,
	ATSC_READ_SNR = 1,
	ATSC_READ_LOCK = 2,
	ATSC_READ_SER = 3,
	ATSC_READ_FREQ = 4,
	ATSC_READ_CK = 5,
};

enum {
	DTMB_READ_STRENGTH = 0,
	DTMB_READ_SNR = 1,
	DTMB_READ_LOCK = 2,
	DTMB_READ_BCH = 3,
};

struct demod_debugfs_files_t {
	const char *name;
	const umode_t mode;
	const struct file_operations *fops;
};

void aml_demod_dbg_init(void);
void aml_demod_dbg_exit(void);
int dtvdemod_create_class_files(struct class *clsp);
void dtvdemod_remove_class_files(struct class *clsp);
unsigned int clear_ddr_bus_data(struct aml_dtvdemod *demod);

#endif
