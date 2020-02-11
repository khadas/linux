/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __PWRDET_H__
#define __PWRDET_H__

struct audio_buffer {
	/* ring buffer for wakeup */
	unsigned char  *buffer;
	unsigned int   wr;
	unsigned int   rd;
	unsigned int   size;
};

struct pwrdet_chipinfo {
	/* new address offset */
	int address_shift;
	/* match id function */
	bool matchid_fn;
};

struct aml_pwrdet {
	struct device *dev;

	unsigned int det_src;
	int irq;
	unsigned int hi_th;
	unsigned int lo_th;

	struct audio_buffer abuf;
	struct pwrdet_chipinfo *chipinfo;
};

#endif /*__PWRDET_H__*/
