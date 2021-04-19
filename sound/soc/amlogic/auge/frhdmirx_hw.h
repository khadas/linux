/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __FRHDMIRX_HW_H__
#define __FRHDMIRX_HW_H__

#define INT_PAO_PAPB_MASK    24
#define INT_PAO_PCPD_MASK    16

enum hdmirx_mode {
	HDMIRX_MODE_SPDIFIN = 0,
	HDMIRX_MODE_PAO = 1,
};

enum {
	EARCTX_SPDIF_TO_HDMIRX = 0,
	SPDIFA_TO_HDMIRX = 1,
	SPDIFB_TO_HDMIRX = 2,
	HDMIRX_SPDIF_TO_HDMIRX = 3,
};

enum {
	TL1_FRHDMIRX = 0,
	T7_FRHDMIRX = 1,
};

/*
 * TXLX_ARC: hdmirx arc from spdif
 * TL1_ARC: hdmirx arc from spdifA/spdifB
 * T7_ARC: same with tm2, but register changed
 *
 */
enum {
	TXLX_ARC = 0,
	TL1_ARC = 1,
	TM2_ARC = 2,
	T7_ARC = 3,
};

void arc_source_enable(int src, bool enable);
void arc_earc_source_select(int src);
void arc_enable(bool enable, int version);

void frhdmirx_enable(bool enable, int version);
void frhdmirx_src_select(int src);
void frhdmirx_ctrl(int channels, int src, int version);
void frhdmirx_clr_PAO_irq_bits(void);
void frhdmirx_clr_SPDIF_irq_bits(void);
void frhdmirx_clr_SPDIF_irq_bits_for_t7_version(void);
unsigned int frhdmirx_get_ch_status(int num, int version);
unsigned int frhdmirx_get_chan_status_pc(enum hdmirx_mode mode, int version);
void frhdmirx_clr_all_irq_bits(int version);
void frhdmirx_afifo_reset(void);

#endif
