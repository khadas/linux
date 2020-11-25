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

void frhdmirx_afifo_reset(void)
{
	unsigned int enable = audiobus_read(EE_AUDIO_FRHDMIRX_CTRL0) >> 31;

	if (enable) {
		audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
				     0x1 << 29, 0x0 << 29);
		audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
				     0x1 << 29, 0x1 << 29);
	}
}

void frhdmirx_enable(bool enable)
{
	if (enable) {
		audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
			0x1 << 29,
			0x1 << 29);
		audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
			0x1 << 28,
			0x1 << 28);
	} else {
		audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
			0x3 << 28,
			0x0 << 28);
	}

	audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0, 0x1 << 31, enable << 31);

	/* from tm2 revb, need enable pao separately */
	audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0, 0x1 << 19, enable << 19);
}

/* source select
 * 0: select spdif lane;
 * 1: select PAO mode;
 */
void frhdmirx_src_select(int src)
{
	audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
		0x1 << 23,
		(bool)src << 23);
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
	int lane, lane_mask = 0, i;

	lane = (channels % 2) ? (channels / 2 + 1) : (channels / 2);
	for (i = 0; i < lane; i++)
		lane_mask |= (1 << i);

	/* PAO mode */
	if (src) {
		audiobus_write(EE_AUDIO_FRHDMIRX_CTRL0,
			lane_mask << 24 | /* chnum_sel */
			0x1 << 22 | /* capture input by fall edge*/
			0x1 << 8  | /* start detect PAPB */
			0x1 << 7  | /* add channel num */
			0x4 << 4    /* chan status sel: pao pc/pd value */
			);
	} else {
		audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0,
			0x1 << 30 | 0xf << 24 | 0x1 << 22 |
			0x3 << 11 | 0x1 << 8 | 0x1 << 7 | 0x7 << 0,
			0x1 << 30 | /* chnum_sel */
			lane_mask << 24 | /* chnum_sel */
			0x1 << 22 | /* clk_inv */
			0x0 << 11 | /* req_sel, Sync 4 spdifin by which */
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
		0x1 << INT_PAO_PAPB_MASK | 0x1 << INT_PAO_PCPD_MASK,
		0x1 << INT_PAO_PAPB_MASK | 0x1 << INT_PAO_PCPD_MASK);

	audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL4,
		0x1 << INT_PAO_PAPB_MASK | 0x1 << INT_PAO_PCPD_MASK,
		0x0 << INT_PAO_PAPB_MASK | 0x0 << INT_PAO_PCPD_MASK);
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

/*
 * reg_ status_sel[6:4];
 * 0: spdif lane0;
 * 1: spdif lane1;
 * 2: spdif lane2;
 * 3: spdif lane3;
 * 4: pao pc/pd value;
 * 5: valid bits;
 */
static void frhdmirx_set_reg_status_sel(uint32_t sel)
{
	audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0, 0x7 << 4, sel << 4);
}

/*
 * reg_status_sel[3]: 0: channel A; 1: channel B;
 * reg_status_sel[2:0]:
 * 0: ch_status[31:0];
 * 1: ch_status[63:32];
 * 2: ch_status[95:64];
 * 3: ch_status[127:96];
 * 4: ch_status[159:128];
 * 5: ch_status[191:160];
 * 6: pc[15:0],pd[15:0];
 */
static void frhdmirx_spdif_channel_status_sel(uint32_t sel)
{
	/* alway select chanel A */
	audiobus_update_bits(EE_AUDIO_FRHDMIRX_CTRL0, 0xf, sel);
}

static DEFINE_SPINLOCK(frhdmirx_lock);

unsigned int frhdmirx_get_ch_status(int num)
{
	unsigned int val;
	unsigned long flags = 0;

	spin_lock_irqsave(&frhdmirx_lock, flags);
	/* default spdif lane 0, channal A */
	frhdmirx_set_reg_status_sel(0);
	frhdmirx_spdif_channel_status_sel(num);
	val = audiobus_read(EE_AUDIO_FRHDMIRX_STAT1);
	spin_unlock_irqrestore(&frhdmirx_lock, flags);

	return val;
}

unsigned int frhdmirx_get_chan_status_pc(enum hdmirx_mode mode)
{
	unsigned int val;
	unsigned long flags = 0;

	spin_lock_irqsave(&frhdmirx_lock, flags);
	if (mode == HDMIRX_MODE_PAO) {
		frhdmirx_set_reg_status_sel(4);
	} else if (mode == HDMIRX_MODE_SPDIFIN) {
		frhdmirx_set_reg_status_sel(0);
		frhdmirx_spdif_channel_status_sel(6);
	}

	val = audiobus_read(EE_AUDIO_FRHDMIRX_STAT1);
	spin_unlock_irqrestore(&frhdmirx_lock, flags);
	return (val >> 16) & 0xff;
}

/* this is used for TL1, arc source select and enable */
void arc_source_enable(int src, bool enable)
{
	/* bits[1:0], 0x2: common; 0x1: single; 0x0: disabled */
	/* depend on hiubus API
	 * aml_hiubus_update_bits(HHI_HDMIRX_ARC_CNTL,
	 * 0x1f << 0,
	 * src << 2 | (enable ? 0x1 : 0) << 0);
	 */
}

/* this is used for TM2, arc/earc source select */
void arc_earc_source_select(int src)
{
	/* depend on hiubus API
	 * if (src == SPDIFA_TO_HDMIRX || src == SPDIFB_TO_HDMIRX) {
	 * // spdif_a = 1; spdif_b = 2
	 * aml_write_hiubus(HHI_HDMIRX_ARC_CNTL, 0xfffffff8 | src);
	 * // analog registers: single mode, about 520mv
	 * aml_write_hiubus(HHI_HDMIRX_EARCTX_CNTL0, 0x14710490);
	 * aml_write_hiubus(HHI_HDMIRX_EARCTX_CNTL1, 0x40011508);
	 * } else {
	 * // earctx_spdif
	 * aml_write_hiubus(HHI_HDMIRX_ARC_CNTL, 0x0);
	 * }
	 */
}

/* this is used for TM2, arc source enable */
void arc_enable(bool enable)
{
	/* depend on hiubus API
	 * aml_hiubus_update_bits(HHI_HDMIRX_EARCTX_CNTL0, 0x1 << 31,
	 * (enable ? 0x1 : 0) << 31);
	 */
}

