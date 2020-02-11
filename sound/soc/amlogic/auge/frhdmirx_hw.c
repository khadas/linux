// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>

#include "frhdmirx_hw.h"
#include "regs.h"
#include "iomap.h"

void frhdmirx_enable(bool enable)
{
	if (enable) {
		audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
				     0x1 << 29, 0x1 << 29);
		audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
				     0x1 << 28, 0x1 << 28);
	} else {
		audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
				     0x3 << 28, 0x0 << 28);
	}

	audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0, 0x1 << 31, enable << 31);
}

/* source select
 * 0: select spdif lane;
 * 1: select PAO mode;
 */
void frhdmirx_src_select(int src)
{
	audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
			     0x1 << 23, (bool)src << 23);
}

static void frhdmirx_enable_irq_bits(int channels, int src)
{
	unsigned int int_bits = 0;

	/* interrupt bits */
	if (src) { /* PAO mode */
		int_bits = (0x1 << INT_PAO_PAPB_MASK | /* find papb */
			    0x1 << INT_PAO_PCPD_MASK   /* find pcpd changed */
		);
	} else { /* SPDIF Lane */
		int lane_irq_bits = (0x1 << 7 | /* lane: find papb */
			0x1 << 6 | /* lane: find valid changed */
			0x1 << 5 | /* lane: find nonpcm to pcm */
			0x1 << 4 | /* lane: find pcpd changed */
			0x1 << 3 | /* lane: find ch status changed */
			0x1 << 1 /* lane: find parity error */
			);

		int lane, i;

		lane = (channels % 2) ? (channels / 2 + 1) : (channels / 2);
		for (i = 0; i < lane; i++)
			int_bits |= (lane_irq_bits << 8 * i);
	}

	int_bits |= audiobus_read(EE_AUDIO_FRHDMIRX_CTRL2);
	audiobus_write(EE_AUDIO_FRHDMIRX_CTRL2, int_bits);
}

void frhdmirx_clr_all_irq_bits(void)
{
	audiobus_write(EE_AUDIO_FRHDMIRX_CTRL3, 0xffffffff);
	audiobus_write(EE_AUDIO_FRHDMIRX_CTRL3, 0x0);
	audiobus_write(EE_AUDIO_FRHDMIRX_CTRL4, 0xffffffff);
	audiobus_write(EE_AUDIO_FRHDMIRX_CTRL4, 0x0);
}

void frhdmirx_ctrl(int channels, int src)
{
	/* PAO mode */
	if (src) {
		audiobus_write(EE_AUDIO_FRHDMIRX_CTRL0,
			       0x1 << 22 | /* capture input by fall edge*/
			       0x1 << 8  | /* start detect PAPB */
			       0x1 << 7  | /* add channel num */
			       0x4 << 4    /* chan status: pao pc/pd value */
			);
	} else {
		int lane, lane_mask = 0, i;

		lane = (channels % 2) ? (channels / 2 + 1) : (channels / 2);
		for (i = 0; i < lane; i++)
			lane_mask |= (1 << i);

		audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
				     0x1 << 30 | 0xf << 24 | 0x1 << 22 |
				     0x3 << 11 | 0x1 << 8 |
				     0x1 << 7  | 0x7 << 0,
				     0x1 << 30 | /* chnum_sel */
				     lane_mask << 24 | /* chnum_sel */
				     0x1 << 22 | /* clk_inv */
				     0x0 << 11 |
				     0x1 << 8  | /* start detect PAPB */
				     0x1 << 7  | /* add channel num*/
				     0x6 << 0    /* channel status*/
			);
	}
	/* nonpcm2pcm_th */
	audiobus_write(EE_AUDIO_FRHDMIRX_CTRL1, 0xff << 20);

	/* enable irq bits */
	frhdmirx_enable_irq_bits(channels, src);
}

void frhdmirx_clr_PAO_irq_bits(void)
{
	audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL4,
			     0x1 << INT_PAO_PAPB_MASK |
			     0x1 << INT_PAO_PCPD_MASK,
			     0x1 << INT_PAO_PAPB_MASK |
			     0x1 << INT_PAO_PCPD_MASK);

	audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL4,
			     0x1 << INT_PAO_PAPB_MASK |
			     0x1 << INT_PAO_PCPD_MASK,
			     0x0 << INT_PAO_PAPB_MASK |
			     0x0 << INT_PAO_PCPD_MASK);
}

void frhdmirx_clr_SPDIF_irq_bits(void)
{
	unsigned int value = audiobus_read(EE_AUDIO_FRHDMIRX_STAT0);
	unsigned int clr_mask = audiobus_read(EE_AUDIO_FRHDMIRX_CTRL4);
	unsigned int reg = 0;
	int i;

	reg = clr_mask | value;
	audiobus_write(EE_AUDIO_FRHDMIRX_CTRL4, reg);

	reg = clr_mask & (~value);
	audiobus_write(EE_AUDIO_FRHDMIRX_CTRL4, reg);

	/*compressed audio only transfer through lane0*/
	for (i = 0; i < 1; i++) {
		/*nonpcm2pcm irq, clear papb/pcpd/nonpcm*/
		if (value & (0x20 << 8 * i)) {
			audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL3,
					     0xf << 8 * i, 0xf << 8 * i);
			audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL3,
					     0xf << 8 * i, 0x0 << 8 * i);
			pr_info("raw to pcm change: irq status:%x, lane: %d\n",
				value, i);
		}
	}
}

unsigned int frhdmirx_get_ch_status0to31(void)
{
	return (unsigned int)audiobus_read(EE_AUDIO_FRHDMIRX_STAT0);
}

unsigned int frhdmirx_get_chan_status_pc(void)
{
	unsigned int val;

	val = audiobus_read(EE_AUDIO_FRHDMIRX_STAT1);
	return (val >> 16) & 0xff;
}

