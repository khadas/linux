/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_SND_IOMAP_H__
#define __AML_SND_IOMAP_H__

#include "ddr_mngr.h"

enum{
	IO_PDM_BUS = 0,
	IO_AUDIO_BUS,
	IO_AUDIO_LOCKER,
	IO_EQDRC_BUS,
	IO_VAD,
	IO_RESAMPLEA,
	IO_RESAMPLEB,
	IO_MAX,
};

int aml_pdm_read(unsigned int reg);
void aml_pdm_write(unsigned int reg, unsigned int val);
void aml_pdm_update_bits(unsigned int reg, unsigned int mask,
			 unsigned int val);

int audiobus_read(unsigned int reg);
void audiobus_write(unsigned int reg, unsigned int val);
void audiobus_update_bits(unsigned int reg, unsigned int mask,
			  unsigned int val);

int audiolocker_read(unsigned int reg);
void audiolocker_write(unsigned int reg, unsigned int val);
void audiolocker_update_bits(unsigned int reg, unsigned int mask,
			     unsigned int val);

int eqdrc_read(unsigned int reg);
void eqdrc_write(unsigned int reg, unsigned int val);
void eqdrc_update_bits(unsigned int reg, unsigned int mask,
		       unsigned int val);

int audioreset_read(unsigned int reg);
void audioreset_write(unsigned int reg, unsigned int val);
void audioreset_update_bits(unsigned int reg, unsigned int mask,
			    unsigned int val);

int vad_read(unsigned int reg);
void vad_write(unsigned int reg, unsigned int val);
void vad_update_bits(unsigned int reg, unsigned int mask,
		     unsigned int val);

unsigned int new_resample_read(enum resample_idx id, unsigned int reg);
void new_resample_write(enum resample_idx id, unsigned int reg,
			unsigned int val);
void new_resample_update_bits(enum resample_idx id, unsigned int reg,
			      unsigned int mask, unsigned int val);

#endif
