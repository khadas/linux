/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __FRHDMIRX_HW_H__
#define __FRHDMIRX_HW_H__

#define INT_PAO_PAPB_MASK    24
#define INT_PAO_PCPD_MASK    16

void frhdmirx_enable(bool enable);
void frhdmirx_src_select(int src);
void frhdmirx_ctrl(int channels, int src);
void frhdmirx_clr_PAO_irq_bits(void);
void frhdmirx_clr_SPDIF_irq_bits(void);
unsigned int frhdmirx_get_ch_status0to31(void);
unsigned int frhdmirx_get_chan_status_pc(void);
void frhdmirx_clr_all_irq_bits(void);

#endif
