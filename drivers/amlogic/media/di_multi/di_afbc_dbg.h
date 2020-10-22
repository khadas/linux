/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_multi/sc2/di_afbc_dbg.h
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

#ifndef __DI_AFBC_DBG_H__
#define __DI_AFBC_DBG_H__

void dbg_afbcd_bits_show(struct seq_file *s, enum EAFBC_DEC eidx);
//void dbg_afd_reg(struct seq_file *s, enum EAFBC_DEC eidx);
void dbg_afbce_bits_show(struct seq_file *s, enum EAFBC_ENC eidx);

#endif	/*__DI_AFBC_DBG_H__*/
