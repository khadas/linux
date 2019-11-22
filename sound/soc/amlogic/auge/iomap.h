/*
 * sound/soc/amlogic/auge/iomap.h
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#ifndef __AML_SND_IOMAP_H__
#define __AML_SND_IOMAP_H__

#include "ddr_mngr.h"

enum{
	IO_PDM_BUS = 0,
	IO_AUDIO_BUS,
	IO_AUDIO_LOCKER,
	IO_EQDRC_BUS,
	IO_RESET,
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
