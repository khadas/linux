/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/enhancement/amvecm/cacb_aadc.h
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

#ifndef CABC_AADC_H
#define CABC_AADC_H

void aml_cabc_alg_process(struct work_struct *work);
void aml_cabc_alg_bypass(struct work_struct *work);
int cabc_aad_debug(char **param);
int cabc_aad_alg_state(void);
int *vf_param_get(void);
int fw_en_get(void);
#endif
