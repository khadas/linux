// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#define DEBUG

#include <linux/types.h>
#include <linux/kernel.h>
#include <sound/asoundef.h>

#include "earc_hw.h"

void earctx_dmac_mute(struct regmap *dmac_map, bool enable)
{
	int val = 0;

	if (enable)
		val = 3;
	mmio_update_bits(dmac_map, EARCTX_SPDIFOUT_CTRL0, 0x3 << 21, val << 21);
}

void earcrx_pll_refresh(struct regmap *top_map,
			enum pll_rst_src rst_src,
			bool level)
{
	pr_info("%s, level %d\n", __func__, level);
	if (rst_src == RST_BY_SELF) {
		/* pll tdc mode */
		mmio_update_bits(top_map, EARCRX_PLL_CTRL3,
				 0x1 << 15, 0x1 << 15);

		/* pll self reset */
		mmio_update_bits(top_map, EARCRX_PLL_CTRL0,
				 0x1 << 29, 0x1 << 29);
		mmio_update_bits(top_map, EARCRX_PLL_CTRL0,
				 0x1 << 29, 0x0 << 29);

		mmio_update_bits(top_map, EARCRX_PLL_CTRL3,
				 0x1 << 15, 0x0 << 15);
	} else if (rst_src == RST_BY_D2A) {
		int rst_src_val = level ? 0x10 : 0x0;

		mmio_update_bits(top_map, EARCRX_PLL_CTRL0,
				 0x3 << 23, rst_src_val << 23);
	} else if (rst_src == RST_BY_DMACRX) {
		int rst_src_val = level ? 0x01 : 0x0;

		mmio_update_bits(top_map, EARCRX_PLL_CTRL0,
				 0x3 << 23, rst_src_val << 23);
	}
}

void earcrx_cmdc_int_mask(struct regmap *top_map)
{
	/* set irq mask */
	mmio_write(top_map, EARCRX_CMDC_INT_MASK,
		   (1 << 15) |  /* idle2_int */
		   (1 << 14) |  /* idle1_int */
		   (1 << 13) |  /* disc2_int */
		   (1 << 12) |  /* disc1_int */
		   (1 << 11) |  /* earc_int */
		   (1 << 10) |  /* hb_status_int */
		   (1 <<  9) |  /* losthb_int */
		   (1 <<  8) |  /* timeout_int */
		   (1 <<  7) |  /* status_ch_int */
		   (0 <<  6) |  /* int_rec_invalid_id */
		   (0 <<  5) |  /* int_rec_invalid_offset */
		   (0 <<  4) |  /* int_rec_unexp */
		   (0 <<  3) |  /* int_rec_ecc_err */
		   (0 <<  2) |  /* int_rec_parity_err */
		   (0 <<  1) |  /* int_recv_packet */
		   (0 <<  0)	 /* int_rec_time_out */
		  );
}

void earcrx_cmdc_init(struct regmap *top_map, bool en, bool rx_dmac_sync_int, bool rterm_on)
{
	if (rterm_on) {
		mmio_write(top_map, EARCRX_ANA_CTRL0,
			   en  << 31 | /* earcrx_en_d2a */
			   0x10 << 25 | /* earcrx_cmdcrx_reftrim */
			   0x10  << 20 | /* earcrx_idr_trim */
			   0x10 << 15 | /* earcrx_rterm_trim */
			   0x4  << 12 | /* earcrx_cmdctx_ack_hystrim */
			   0x10 << 7  | /* earcrx_cmdctx_ack_reftrim */
			   0x1  << 4  | /* earcrx_cmdcrx_rcfilter_sel */
			   0x4  << 0    /* earcrx_cmdcrx_hystrim */
			  );
		mmio_write(top_map, EARCRX_ANA_CTRL1,
				 0x1 << 11 | 0x1 << 10 | 0x8 << 4 | 0x8 << 0);
	} else {
		mmio_write(top_map, EARCRX_ANA_CTRL0,
			   en  << 31 | /* earcrx_en_d2a */
			   0x10 << 24 | /* earcrx_cmdcrx_reftrim */
			   0x8  << 20 | /* earcrx_idr_trim */
			   0x10 << 15 | /* earcrx_rterm_trim */
			   0x4  << 12 | /* earcrx_cmdctx_ack_hystrim */
			   0x10 << 7  | /* earcrx_cmdctx_ack_reftrim */
			   0x1  << 4  | /* earcrx_cmdcrx_rcfilter_sel */
			   0x4  << 0    /* earcrx_cmdcrx_hystrim */
			  );
	}

	mmio_write(top_map, EARCRX_PLL_CTRL3,
		   0x2 << 20 | /* earcrx_pll_bias_adj */
		   0x4 << 16 | /* earcrx_pll_rou */
		   en << 13   /* earcrx_pll_dco_sdm_e */
		  );

	mmio_write(top_map, EARCRX_PLL_CTRL0,
		   en << 28 | /* earcrx_pll_en */
		   0x1 << 23 | /* earcrx_pll_dmacrx_sqout_rstn_sel */
		   0x1 << 10   /* earcrx_pll_n */
		  );
	mmio_write(top_map,
		EARCRX_PLL_CTRL1,
		0x1410a8);
	mmio_update_bits(top_map,
		EARCRX_PLL_CTRL2,
		0x3,
		0x2);
	mmio_write(top_map,
		EARCRX_PLL_CTRL3,
		0x20046000);
}

void earcrx_cmdc_arc_connect(struct regmap *cmdc_map, bool init)
{
	if (init)
		mmio_update_bits(cmdc_map,
				 EARC_RX_CMDC_VSM_CTRL0,
				 0x7 << 25,
				 0x1 << 27 | /* arc_initiated */
				 0x0 << 26 | /* arc_terminated */
				 0x1 << 25   /* arc_enable */
				);
	else
		mmio_update_bits(cmdc_map,
				 EARC_RX_CMDC_VSM_CTRL0,
				 0x7 << 25,
				 0x0 << 27 | /* arc_initiated */
				 0x1 << 26 | /* arc_terminated */
				 0x0 << 25   /* arc_enable */
				);
}

void earcrx_cmdc_hpd_detect(struct regmap *cmdc_map, bool st)
{
	if (st) {
		mmio_update_bits(cmdc_map,
				 EARC_RX_CMDC_VSM_CTRL0,
				 0x1 << 19,
				 0x1 << 19	/* comma_cnt_rst */
				);

		mmio_update_bits(cmdc_map,
				 EARC_RX_CMDC_VSM_CTRL0,
				 0x1 << 19 | 0xff << 0,
				 0x0 << 19 | /* comma_cnt_rst */
				 0xa << 0
				);
	} else {
		/* soft reset */
		mmio_update_bits(cmdc_map,
				 EARC_RX_CMDC_TOP_CTRL1,
				 0xf << 1,
				 0xf << 1);
		mmio_update_bits(cmdc_map,
				 EARC_RX_CMDC_TOP_CTRL1,
				 0xf << 1,
				 0x0 << 1);
	}
}

void earcrx_dmac_sync_int_enable(struct regmap *top_map, int enable)
{
	/* dmac_sync dmac_valid_auto neg_int_se */
	mmio_update_bits(top_map, EARCRX_DMAC_INT_MASK,
			 (0x1 << 28),  (enable << 28));
}

void earcrx_dmac_init(struct regmap *top_map,
		      struct regmap *dmac_map,
		      bool unstable_tick_sel,
		      bool chnum_mult_mode)
{
	/* can't open bit 5 as it will cause stuck on t7 chips */
	mmio_update_bits(top_map, EARCRX_DMAC_INT_MASK,
			 0x3FFFF,
			 (0x1 << 17) | /* earcrx_ana_rst c_new_format_set */
			 (0x1 << 16) | /* earcrx_ana_rst c_earcrx_div2_hold_set */
			 (0x1 << 15) | /* earcrx_err_correct c_bcherr_int_set */
			 (0x0 << 14) | /* earcrx_err_correct r_afifo_overflow_set */
			 (0x0 << 13) | /* earcrx_err_correct r_fifo_overflow_set */
			 (0x0 << 12) | /* earcrx_user_bit_check r_fifo_overflow */
			 (0x0 << 11) | /* earcrx_user_bit_check c_fifo_thd_pass */
			 (0x0 << 10) | /* earcrx_user_bit_check c_u_pk_lost_int_set */
			 (0x0 << 9)	| /* arcrx_user_bit_check c_iu_pk_end */
			 (0x0 << 8)	| /* arcrx_biphase_decode c_chst_mute_clr */
			 (0x1 << 7)	| /* arcrx_biphase_decode c_find_papb */
			 (0x1 << 6)	| /* arcrx_biphase_decode c_valid_change */
			 (0x0 << 5)	| /* arcrx_biphase_decode c_find_nonpcm2pcm */
			 (0x1 << 4)	| /* arcrx_biphase_decode c_pcpd_change */
			 (0x1 << 3)	| /* arcrx_biphase_decode c_ch_status_change */
			 (0x1 << 2)	| /* arcrx_biphase_decode sample_mod_change */
			 (0x1 << 1)	| /* arcrx_biphase_decode r_parity_err */
			 (0x0 << 0)	  /* arcrx_dmac_sync afifo_overflow */
		  );

	mmio_write(dmac_map, EARCRX_DMAC_SYNC_CTRL0,
		   (1 << 16)	|	 /* reg_ana_buf_data_sel_en */
		   (3 << 12)	|	 /* reg_ana_buf_data_sel */
		   (7 << 8)	|	 /* reg_ana_clr_cnt */
		   (7 << 4)		 /* reg_ana_set_cnt */
		  );
	mmio_write(dmac_map, EARCRX_DMAC_UBIT_CTRL0,
		   (47 << 16) | /* reg_fifo_thd */
		   (1 << 12)  | /* reg_user_lr */
		   (29 << 0)	/* reg_data_bit */
		  );
	mmio_write(dmac_map, EARCRX_ANA_RST_CTRL0, 1 << 31 | 2000 << 0);
	if (chnum_mult_mode)
		mmio_update_bits(dmac_map, EARCRX_SPDIFIN_CTRL6, 0x1 << 27, 0x1 << 27);
	if (unstable_tick_sel) {
		mmio_update_bits(top_map, EARCRX_DMAC_INT_MASK, 0x1 << 28, 0x1 << 28);
		mmio_write(dmac_map, EARCRX_DMAC_SYNC_CTRL3, 0x1 << 19 | 0x2 << 16 | 0x64 << 0);
		mmio_write(dmac_map, EARCRX_DMAC_SYNC_CTRL4, 0x1 << 19 | 0x2 << 16 | 0x32 << 0);
		mmio_write(dmac_map, EARCRX_DMAC_SYNC_CTRL5, 0x1 << 19 | 0x2 << 16 | 0x32 << 0);
	}
}

void earcrx_arc_init(struct regmap *dmac_map)
{
	unsigned int spdifin_clk = 500000000;

	/* sysclk/rate/32(bit)/2(ch)/2(bmc) */
	unsigned int counter_32k  = (spdifin_clk / (32000  * 64));
	unsigned int counter_44k  = (spdifin_clk / (44100  * 64));
	unsigned int counter_48k  = (spdifin_clk / (48000  * 64));
	unsigned int counter_88k  = (spdifin_clk / (88200  * 64));
	unsigned int counter_96k  = (spdifin_clk / (96000  * 64));
	unsigned int counter_176k = (spdifin_clk / (176400 * 64));
	unsigned int counter_192k = (spdifin_clk / (192000 * 64));
	unsigned int mode0_th = 3 * (counter_32k + counter_44k) >> 1;
	unsigned int mode1_th = 3 * (counter_44k + counter_48k) >> 1;
	unsigned int mode2_th = 3 * (counter_48k + counter_88k) >> 1;
	unsigned int mode3_th = 3 * (counter_88k + counter_96k) >> 1;
	unsigned int mode4_th = 3 * (counter_96k + counter_176k) >> 1;
	unsigned int mode5_th = 3 * (counter_176k + counter_192k) >> 1;
	unsigned int mode0_timer = counter_32k >> 1;
	unsigned int mode1_timer = counter_44k >> 1;
	unsigned int mode2_timer = counter_48k >> 1;
	unsigned int mode3_timer = counter_88k >> 1;
	unsigned int mode4_timer = counter_96k >> 1;
	unsigned int mode5_timer = (counter_176k >> 1);
	unsigned int mode6_timer = (counter_192k >> 1);

	mmio_write(dmac_map,
		   EARCRX_SPDIFIN_SAMPLE_CTRL0,
		   0x0 << 28 |                /* detect by max_width */
		   (spdifin_clk / 10000) << 0 /* base timer */
		  );

	mmio_write(dmac_map,
		   EARCRX_SPDIFIN_SAMPLE_CTRL1,
		   mode0_th << 20 |
		   mode1_th << 10 |
		   mode2_th << 0);

	mmio_write(dmac_map,
		   EARCRX_SPDIFIN_SAMPLE_CTRL2,
		   mode3_th << 20 |
		   mode4_th << 10 |
		   mode5_th << 0);

	mmio_write(dmac_map,
		   EARCRX_SPDIFIN_SAMPLE_CTRL3,
		   (mode0_timer << 24) |
		   (mode1_timer << 16) |
		   (mode2_timer << 8)	|
		   (mode3_timer << 0)
		  );

	mmio_write(dmac_map,
		   EARCRX_SPDIFIN_SAMPLE_CTRL4,
		   (mode4_timer << 24) |
		   (mode5_timer << 16) |
		   (mode6_timer << 8)
		  );

	mmio_write(dmac_map,
		   EARCRX_SPDIFIN_CTRL0,
		   0x1 << 31 | /* reg_work_en */
		   0x0 << 30 | /* reg_chnum_sel */
		   0x1 << 25 | /* reg_findpapb_en */
		   0x1 << 24 | /* nonpcm2pcm_th enable */
		   0xFFF << 12 |  /* reg_nonpcm2pcm_th */
		   0x1 << 2    /* reg_check_parity */
		  );
	mmio_write(dmac_map,
		   EARCRX_SPDIFIN_CTRL2,
		   (1 << 16) | /* auto clear compress mode if cs not compress */
		   (1 << 15) | /* auto clear compress mode when nonpcm2pcm */
		   (1 << 14) | /* earc_auto */
		   (1 << 13) | /* earcin_papb_lr */
		   (1 << 12) | /* earcin_check_papb */
		   (19 << 4)   /* earc_papb_msb */
		  );
	mmio_write(dmac_map,
		   EARCRX_SPDIFIN_CTRL3,
		   (0xEC37 << 16) | /* reg_earc_pa_value */
		   (0x5A5A << 0)    /* reg_earc_pb_value */
		  );
}

static void earcrx_mute_block_enable(struct regmap *dmac_map, bool en)
{
	/* time thd, tick */
	mmio_update_bits(dmac_map,
			 EARCRX_SPDIFIN_CTRL1,
			 0x7fff << 9,
			 0x500 << 12 | /* thd */
			 0x4 << 9      /* tick, 1ms */
	);

	/* Mute bit in CS
	 * mute mini block number, enable
	 * check channel A status always
	 */
	mmio_update_bits(dmac_map,
			 EARCRX_SPDIFIN_CTRL2,
			 0x7fff << 17,
			 IEC_CS_MUTE_OFFSET | 0x2 << 19 | en << 17
	);
}

static DEFINE_SPINLOCK(earcrx_cs_mutex);

/* Note: mask without offset */
static unsigned int earcrx_get_cs_bits(struct regmap *dmac_map,
				       int cs_offset, int mask)
{
	int reg_offset;
	int bits_offset;
	enum channel_status_type cs_type;
	int stats_sel, val, cs_a, cs_b;
	unsigned long flags = 0;

	spin_lock_irqsave(&earcrx_cs_mutex, flags);

	reg_offset = cs_offset / REG_CS_LEN;
	bits_offset = cs_offset % REG_CS_LEN;

	/* channel A status */
	cs_type = CS_TYPE_A;
	stats_sel = reg_offset + cs_type * 8;
	mmio_update_bits(dmac_map,
			 EARCRX_SPDIFIN_CTRL0,
			 0xf << 8,
			 stats_sel << 8);
	val = mmio_read(dmac_map, EARCRX_SPDIFIN_STAT1);
	cs_a = (val >> bits_offset) & mask;

	/* channel B status */
	cs_type = CS_TYPE_B;
	stats_sel = reg_offset + cs_type * 8;
	mmio_update_bits(dmac_map,
			 EARCRX_SPDIFIN_CTRL0,
			 0xf << 8,
			 stats_sel << 8);
	val = mmio_read(dmac_map, EARCRX_SPDIFIN_STAT1);
	cs_b = (val >> bits_offset) & mask;

	if (cs_a != cs_b)
		pr_warn("use CHANNEL A STATUS as default 0x%x, 0x%x .\n", cs_offset, mask);

	spin_unlock_irqrestore(&earcrx_cs_mutex, flags);

	return cs_a;
}

unsigned int earcrx_get_cs_iec958(struct regmap *dmac_map, int offset)
{
	return earcrx_get_cs_bits(dmac_map, offset * 32, 0xffffffff);
}

unsigned int earcrx_get_cs_layout(struct regmap *dmac_map)
{
	return earcrx_get_cs_bits(dmac_map,
			IEC_CS_AUDIO_LAYOUT_OFFSET,
			IEC_CS_AUDIO_LAYOUT_MASK);
}

unsigned int earcrx_get_cs_ca(struct regmap *dmac_map)
{
	return earcrx_get_cs_bits(dmac_map,
			IEC_CS_AIF_DB4_OFFSET,
			IEC_CS_AIF_DB4_MASK);
}

unsigned int earcrx_get_cs_mute(struct regmap *dmac_map)
{
	return earcrx_get_cs_bits(dmac_map,
		IEC_CS_MUTE_OFFSET,
		IEC_CS_MUTE_MASK);
}

static unsigned int ecc_syndrome(unsigned int val1, unsigned int val2)
{
	val1 &= ~0x5555;
	val1 |= val2 & 0x5555;

	return val1;
}

static int earcrx_get_cs_pcpd(struct regmap *dmac_map, bool ecc_check)
{
	unsigned int val = earcrx_get_cs_bits(dmac_map, 192, 0xffffffff);
	unsigned int pc_e = (val >> 16) & 0xffff;
	unsigned int pd_e = val & 0xffff;

	if (ecc_check)
		return ecc_syndrome(pc_e, pd_e) << 16 |
			ecc_syndrome(pd_e, pc_e);
	else
		return val;
}

unsigned int earcrx_get_cs_fmt(struct regmap *dmac_map, enum attend_type type)
{
	enum audio_coding_types coding_type = AUDIO_CODING_TYPE_UNDEFINED;
	unsigned int val, layout;

	val = earcrx_get_cs_bits(dmac_map, 0x0, 0x3b);
	layout = earcrx_get_cs_layout(dmac_map);

	/* One Bit Audio, Channel Status Bits 0,1,3,4, and 5
	 * use case: 0 0 0 1 1 for bit 0,1,3,4 and 5
	 * use case: 0 1 1 1 1 for bit 0,1,3,4 and 5
	 */
	if (unlikely((val & 0x30) == 0x30)) {
		unsigned int val1 = earcrx_get_cs_bits(dmac_map, 0x32, 0x7);

		if (val1 == 0x0 &&
		    (((val & 0x3b) == 0x30) || ((val & 0x3b) == 0x3a))) {
			int layout = earcrx_get_cs_bits(dmac_map,
				IEC_CS_AUDIO_LAYOUT_OFFSET,
				IEC_CS_AUDIO_LAYOUT_MASK);
			if (layout == LO12_OBA) {
				coding_type = AUDIO_CODING_TYPE_SACD_12CH;
			} else if (layout == LO6_OBA) {
				coding_type = AUDIO_CODING_TYPE_SACD_6CH;
			} else {
				pr_err("unknown for DSD(One Bit Audio), default as 6ch\n");
				coding_type = AUDIO_CODING_TYPE_SACD_6CH;
			}
		}
	} else if ((val & 0x30) == 0x20) {
		if (layout == LO32_LPCM) {
			coding_type = AUDIO_CODING_TYPE_MULTICH_32CH_LPCM;
		} else if (layout == LO16_LPCM) {
			coding_type = AUDIO_CODING_TYPE_MULTICH_16CH_LPCM;
		} else if (layout == LO8_LPCM) {
			coding_type = AUDIO_CODING_TYPE_MULTICH_8CH_LPCM;
		} else if (layout == LO2_LPCM) {
			coding_type = AUDIO_CODING_TYPE_MULTICH_2CH_LPCM;
		} else {
			pr_err("unknown for Multi-Ch LPCM, default as 2ch\n");
			coding_type = AUDIO_CODING_TYPE_MULTICH_2CH_LPCM;
		}
	} else if ((val & IEC958_AES0_NONAUDIO) == IEC958_AES0_NONAUDIO) {
		if ((val & 0x38) == 0x0) {
			int pcpd, pc_v;

			if (layout == 0x7) //Layout B
				mmio_update_bits(dmac_map, EARCRX_SPDIFIN_CTRL2,
					0x1 << 9, 0x1 << 9);
			else
				mmio_update_bits(dmac_map, EARCRX_SPDIFIN_CTRL2, 0x1 << 9, 0);
			pcpd = earcrx_get_cs_pcpd(dmac_map, type == ATNDTYP_EARC);
			pc_v = (pcpd >> 16) & 0xffff;

			/* compressed audio  type */
			coding_type = iec_61937_pc_to_coding_type(pc_v);

			if (layout == 0x7) {
				switch (coding_type) {
				case AUDIO_CODING_TYPE_AC3:
					coding_type = AUDIO_CODING_TYPE_AC3_LAYOUT_B;
					break;
				case AUDIO_CODING_TYPE_EAC3:
					coding_type = AUDIO_CODING_TYPE_EAC3_LAYOUT_B;
					break;
				case AUDIO_CODING_TYPE_MLP:
					coding_type = AUDIO_CODING_TYPE_MLP_LAYOUT_B;
					break;
				case AUDIO_CODING_TYPE_DTS:
					coding_type = AUDIO_CODING_TYPE_DTS_LAYOUT_B;
					break;
				case AUDIO_CODING_TYPE_DTS_HD:
					coding_type = AUDIO_CODING_TYPE_DTS_HD_LAYOUT_B;
					break;
				case AUDIO_CODING_TYPE_DTS_HD_MA:
					coding_type = AUDIO_CODING_TYPE_DTS_HD_MA_LAYOUT_B;
					break;
				default:
					coding_type = AUDIO_CODING_TYPE_PAUSE;
					break;
				}
			}

		}
	} else {
		coding_type = AUDIO_CODING_TYPE_STEREO_LPCM;
	}

	return coding_type;
}

static int earcrx_get_cs_channels(struct regmap *dmac_map,
				  enum audio_coding_types coding_type)
{
	int channels = 0;

	switch (coding_type) {
	case AUDIO_CODING_TYPE_MULTICH_32CH_LPCM:
		channels = 32;
		break;
	case AUDIO_CODING_TYPE_MULTICH_16CH_LPCM:
		channels = 16;
		break;
	case AUDIO_CODING_TYPE_MULTICH_8CH_LPCM:
		channels = 8;
		break;
	case AUDIO_CODING_TYPE_MULTICH_2CH_LPCM:
		channels = 2;
		break;
	case AUDIO_CODING_TYPE_SACD_12CH:
		channels = 12;
		break;
	case AUDIO_CODING_TYPE_SACD_6CH:
		channels = 6;
		break;
	case AUDIO_CODING_TYPE_AC3_LAYOUT_B:
		channels = 8; /* Layout for coding type */
		break;
	default:
		channels = 2;
		break;
	}

	return channels;
}

unsigned int earcrx_get_cs_freq(struct regmap *dmac_map,
				enum audio_coding_types coding_type)
{
	unsigned int val;
	unsigned int csfs, freq, channels;

	val = earcrx_get_cs_bits(dmac_map,
				 IEC_CS_SFREQ_OFFSET,
				 IEC_CS_SFREQ_MASK);

	csfs = val & 0xf;
	freq = iec_rate_from_csfs(csfs);

	/* Fix to really fs */
	channels = earcrx_get_cs_channels(dmac_map, coding_type);
	freq /= (channels / 2);

	return freq;
}

unsigned int earcrx_get_cs_word_length(struct regmap *dmac_map)
{
	unsigned int val;
	unsigned int max_len, len = 0;

	val = earcrx_get_cs_bits(dmac_map,
				 IEC_CS_WLEN_OFFSET,
				 IEC_CS_WLEN_MASK);

	/* Maximum audio sample word length */
	if (val & (1 << 0))
		max_len = 24;
	else
		max_len = 20;

	switch ((val >> 1) & 0x7) {
	case 0:
		len = (max_len == 24) ? 20 : 24; /* Not indicated */
		break;
	case 1:
		len = max_len - 4;
		break;
	case 0x5:
		len = max_len - 0;
		break;
	default:
		pr_warn("%s, Not support world length code:%#x\n",
			__func__, (val >> 1));
		break;
	}

	pr_info("max len:%d, %slen:%d, len_mask:%#x\n",
		max_len,
		((val >> 1) & 0x7) == 0 ? "(n.a.)" : "",
		len,
		((val >> 1) & 0x7));

	return len;
}

enum cmdc_st earcrx_cmdc_get_state(struct regmap *cmdc_map)
{
	int val = mmio_read(cmdc_map, EARC_RX_CMDC_STATUS0);
	enum cmdc_st state = (enum cmdc_st)(val & 0x7);

	return state;
}

enum attend_type earcrx_cmdc_get_attended_type(struct regmap *cmdc_map)
{
	int val = mmio_read(cmdc_map, EARC_RX_CMDC_STATUS0);
	enum cmdc_st state = (enum cmdc_st)(val & 0x7);
	enum attend_type type = ATNDTYP_DISCNCT;

	if ((val & (1 << 0x3)) && state == CMDC_ST_ARC)
		type = ATNDTYP_ARC;
	else if ((val & (1 << 0x4)) && state == CMDC_ST_EARC)
		type = ATNDTYP_EARC;

	return type;
}

void earcrx_cdmc_clr_irqs(struct regmap *top_map, int clr)
{
	mmio_write(top_map, EARCRX_CMDC_INT_PENDING, clr);
}

int earcrx_cdmc_get_irqs(struct regmap *top_map)
{
	return mmio_read(top_map, EARCRX_CMDC_INT_PENDING);
}

void earcrx_dmac_sync_clr_irqs(struct regmap *top_map)
{
	mmio_write(top_map, EARCRX_DMAC_INT_PENDING, 0x1 << 28);
}

void earcrx_dmac_clr_irqs(struct regmap *top_map, int clr)
{
	mmio_write(top_map, EARCRX_DMAC_INT_PENDING, clr & 0xEFFFFFFF);
}

int earcrx_dmac_get_irqs(struct regmap *top_map)
{
	return mmio_read(top_map, EARCRX_DMAC_INT_PENDING);
}

int earcrx_dmac_get_mask(struct regmap *top_map)
{
	return mmio_read(top_map, EARCRX_DMAC_INT_MASK);
}

bool earcrx_pll_dmac_valid(struct regmap *top_map)
{
	unsigned int pll_status0 = mmio_read(top_map, EARCRX_PLL_STAT0);

	return (pll_status0 >> 30) & 0x3;
}

void earcrx_reset(struct regmap *dmac_map)
{
	/* top soft reset */
	mmio_update_bits(dmac_map, EARCRX_DMAC_TOP_CTRL0, 1 << 30, 0x1 << 30);
	mmio_update_bits(dmac_map, EARCRX_DMAC_TOP_CTRL0, 1 << 30, 0x0 << 30);
}

void earcrx_enable(struct regmap *cmdc_map,
		   struct regmap *dmac_map, bool enable)
{
	enum attend_type type = earcrx_cmdc_get_attended_type(cmdc_map);

	if (enable) {
		mmio_update_bits(dmac_map, EARCRX_DMAC_SYNC_CTRL0,
				 1 << 30,	 /* reg_rst_afifo_out_n */
				 1 << 30);

		mmio_update_bits(dmac_map, EARCRX_DMAC_SYNC_CTRL0,
				 1 << 29,	 /* reg_rst_afifo_in_n */
				 0x1 << 29);

		mmio_update_bits(dmac_map, EARCRX_ERR_CORRECT_CTRL0,
				 1 << 29,  /* reg_rst_afifo_out_n */
				 1 << 29
				);
		mmio_update_bits(dmac_map, EARCRX_ERR_CORRECT_CTRL0,
				 1 << 28, /* reg_rst_afifo_in_n */
				 1 << 28	/* reg_rst_afifo_in_n */
				);
	} else {
		mmio_update_bits(dmac_map, EARCRX_DMAC_SYNC_CTRL0,
				 0x3 << 29,
				 0x0 << 29);

		mmio_update_bits(dmac_map, EARCRX_ERR_CORRECT_CTRL0,
				 0x3 << 28, 0x0 << 28);
	}

	if (type == ATNDTYP_EARC) {
		/* mute block check */
		earcrx_mute_block_enable(dmac_map, true);

		mmio_update_bits(dmac_map,
				 EARCRX_SPDIFIN_SAMPLE_CTRL0,
				 0x1 << 31, /* reg_work_enable */
				 enable << 31);

		mmio_update_bits(dmac_map, EARCRX_DMAC_SYNC_CTRL0,
				 1 << 31,	 /* reg_work_en */
				 enable << 31);

		mmio_update_bits(dmac_map, EARCRX_DMAC_TOP_CTRL0,
			 1 << 16,
			 1 << 16);
	} else if (type == ATNDTYP_ARC) {
		/* mute block check */
		earcrx_mute_block_enable(dmac_map, false);

		mmio_update_bits(dmac_map,
				 EARCRX_SPDIFIN_SAMPLE_CTRL0,
				 0x1 << 31, /* reg_work_enable */
				 enable << 31);

		mmio_write(dmac_map, EARCRX_DMAC_SYNC_CTRL0, 0x0);
		mmio_update_bits(dmac_map, EARCRX_DMAC_TOP_CTRL0,
			 1 << 16,
			 0);
	}

	mmio_update_bits(dmac_map, EARCRX_DMAC_UBIT_CTRL0,
			 1 << 31, /* reg_work_enable */
			 enable << 31);

	mmio_update_bits(dmac_map, EARCRX_ERR_CORRECT_CTRL0,
			 1 << 31,
			 enable << 31); /* reg_work_en */

	mmio_update_bits(dmac_map, EARCRX_DMAC_TOP_CTRL0,
			 1 << 31,
			 enable << 31); /* reg_top_work_en */
}

void earctx_cmdc_int_mask(struct regmap *top_map)
{
	mmio_write(top_map, EARCTX_CMDC_INT_MASK,
		   (0x0 << 17) | /* hpd_high_int */
		   (0x0 << 16) | /* hpd_low_int */
		   (0x1 << 15) | /* idle2_int */
		   (0x1 << 14) | /* idle1_int */
		   (0x1 << 13) | /* disc2_int */
		   (0x1 << 12) | /* disc1_int */
		   (0x1 << 11) | /* earc_int */
		   (0x0 << 10) | /* hb_status_int */
		   (0x1 << 9) |  /* losthb_int */
		   (0x1 << 8) |  /* timeout_int */
		   (0x1 << 7) |  /* status_ch_int */
		   (0x0 << 6) |  /* int_recv_finished */
		   (0x0 << 5) |  /* int_recv_nack */
		   (0x0 << 4) |  /* int_recv_norsp */
		   (0x0 << 3) |  /* int_recv_unexp */
		   (0x0 << 2) |  /* int_recv_data */
		   (0x0 << 1) |  /* int_recv_ack */
		   (0x0 << 0)	  /* int_recv_ecc_err */
		  );
}

void earctx_enable_d2a(struct regmap *top_map, int enable)
{
	mmio_update_bits(top_map, EARCTX_ANA_CTRL0, 0x1 << 31, enable << 31);
}

void earctx_cmdc_init(struct regmap *top_map, bool en, bool rterm_on)
{
	/* ana */
	if (rterm_on)
		mmio_write(top_map, EARCTX_ANA_CTRL0,
			   en << 31   |  /* earctx_en_d2a */
			   0x1 << 28  |  /* earctx_cmdcrx_rcfilter_sel */
			   0x4 << 24  |  /* earctx_cmdcrx_hystrim */
			   0x10 << 19 |  /* earctx_idr_trim */
			   0x1 << 17  |  /* earctx_rterm_on */
			   0x10 << 12 |  /* earctx_rterm_trim */
			   0x4 << 8   |  /* earctx_dmac_slew_con */
			   0x4 << 5   |  /* earctx_cmdctx_ack_hystrim */
			   0x10 << 0     /* earctx_cmdctx_ack_reftrim */
			  );
	else
		mmio_write(top_map, EARCTX_ANA_CTRL0,
			   en << 31   |  /* earctx_en_d2a */
			   0x1 << 28  |  /* earctx_cmdcrx_rcfilter_sel */
			   0x4 << 24  |  /* earctx_cmdcrx_hystrim */
			   0x8 << 20  |  /* earctx_idr_trim */
			   0x10 << 12 |  /* earctx_rterm_trim */
			   0x4 << 8   |  /* earctx_dmac_slew_con */
			   0x4 << 5   |  /* earctx_cmdctx_ack_hystrim */
			   0x10 << 0     /* earctx_cmdctx_ack_reftrim */
			  );
}

void earctx_cmdc_set_timeout(struct regmap *cmdc_map, int no_timeout)
{
	/* no timeout */
	mmio_update_bits(cmdc_map,
			 EARC_TX_CMDC_VSM_CTRL5,
			 0x3 << 0,
			 no_timeout << 1
			 );

	/* if no_timeout is 0, fill in timeout value, 550ms*/
	if (no_timeout == 0) {
		mmio_update_bits(cmdc_map,
			 EARC_TX_CMDC_VSM_CTRL5,
			 0x7 << 4 | 0xfffff << 12,
			 0x3 << 4 | 550 << 12
			 );
	}
}

void earctx_cmdc_arc_connect(struct regmap *cmdc_map, bool init)
{
	if (init)
		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_VSM_CTRL0,
				 0x7 << 25,
				 0x1 << 27 | /* arc_initiated */
				 0x0 << 26 | /* arc_terminated */
				 0x1 << 25   /* arc_enable */
				);
	else
		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_VSM_CTRL0,
				 0x7 << 25,
				 0x0 << 27 | /* arc_initiated */
				 0x1 << 26 | /* arc_terminated */
				 0x0 << 25   /* arc_enable */
				);
}

void earctx_cmdc_hpd_detect(struct regmap *top_map,
			    struct regmap *cmdc_map,
			    int earc_port, bool st)
{
	/* select hdmirx_hpd */
	mmio_update_bits(cmdc_map,
			 EARC_TX_CMDC_VSM_CTRL1,
			 0x1 << 8, /* cntl_hpd_sel */
			 0x1 << 8);

	/* select hdmi_hpd mux */
	mmio_update_bits(top_map, EARCTX_TOP_CTRL0,
			 0x1 << 11 | 0x3 << 8 | 0x3 << 4,
			 0x1 << 11 |       /* hdmi_hpd invent */
			 earc_port << 8 |  /* hdmi_hpd mux, port 0/1/2 */
			 /* earctx_hd_hdp mux,3:register value 2:gpiow_5 1:gpiow_9 0:gpiow_1 */
			 earc_port << 4
			);

	if (st) {
		/* reset for comma_cnt, timeout_sts */
		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_VSM_CTRL0,
				 0x3 << 18,
				 0x3 << 18
				);

		/* reset and hpd sel */
		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_VSM_CTRL0,
				 0x3f << 18,
				 0x0 << 18 |
				 0x3 << 20 | 0x3 << 22
				);

		earctx_cmdc_set_timeout(cmdc_map, 0);

		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_VSM_CTRL1,
				 0xff << 0,
				 0x3 << 0   /* comma_cnt_th */
				);

		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_VSM_CTRL4,
				 0xfffff << 12,
				 0x19 << 12   /* no heartbeat ack timing */
				);

		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_BIPHASE_CTRL2,
				 0xffff << 0,
				 0x640
				);
	} else {
		/* soft reset */
		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_TOP_CTRL1,
				 0xf << 0,
				 0xf << 0);
		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_TOP_CTRL1,
				 0xf << 0,
				 0x0 << 0);
		earctx_cmdc_set_timeout(cmdc_map, 1);
		/* set default value */
		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_VSM_CTRL1,
				 0x1 << 8,
				 0);
		/* set by register value */
		mmio_update_bits(top_map, EARCTX_TOP_CTRL0,
				 0x1 << 11 | 0x3 << 8 | 0x3 << 4,
				 0x3 << 8 | 0x3 << 4);
	}
}

void earctx_dmac_init(struct regmap *top_map,
		      struct regmap *dmac_map,
		      int earc_spdifout_lane_mask,
		      unsigned int chmask,
		      unsigned int swap_masks,
		      bool mute)
{
	unsigned int lswap_masks, rswap_masks;

	mmio_update_bits(dmac_map, EARCTX_SPDIFOUT_CTRL0,
			 0x3 << 28,
			 0x0 << 28);
	mmio_update_bits(dmac_map, EARCTX_SPDIFOUT_CTRL0,
			 0x1 << 29, /* afifo out reset */
			 0x1 << 29);
	mmio_update_bits(dmac_map, EARCTX_SPDIFOUT_CTRL0,
			 0x1 << 28 | /* afifo in reset */
			 0x1 << 26 | /* user Bit select */
			 0x1 << 24 | /* chdata select*/
			 0x1 << 20 | /* reg_data_sel, 1: data from 27bit */
			 0x1 << 19 | /* 0: lsb first */
			 0x1 << 18,  /* biphase encode valid Bit value sel */
			 0x1 << 28 |
			 0x1 << 26 |
			 0x1 << 24 |
			 0x1 << 20 |
			 0x0 << 19 |
			 0x1 << 18
			);

	if (earc_spdifout_lane_mask == EARC_SPDIFOUT_LANE_MASK_V2)
		mmio_update_bits(dmac_map, EARCTX_SPDIFOUT_CTRL2,
				 0xffff,   /* lane mask */
				 chmask);     /*  ODO: lane 0 now */
	else
		mmio_update_bits(dmac_map, EARCTX_SPDIFOUT_CTRL0,
				 0xff << 4,   /* lane mask */
				 chmask << 4);   /*  ODO: lane 0 now */
	mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL0,
			 0x1 << 23 | /* reg_bch_in_reverse */
			 0x1 << 21 | /* reg_bch_out_data_reverse */
			 0x1 << 16,  /* reg_ubit_fifo_init_n */
			 0x1 << 23 |
			 0x1 << 21 |
			 0x1 << 16);
	lswap_masks = swap_masks & 0xf;
	rswap_masks = (swap_masks & 0xf0) >> 4;
	if (lswap_masks > 7)
		mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL0,
				 0x1 << 14 | 0x7 << 4,
				 0x1 << 14 | (lswap_masks - 8) << 4);
	else
		mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL0,
				 0x1 << 14 | 0x7 << 4,
				 0x0 << 14 | lswap_masks << 4);
	if (rswap_masks > 7)
		mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL0,
				 0x1 << 15 | 0x7 << 8,
				 0x1 << 15 | (rswap_masks - 8) << 8);
	else
		mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL0,
				 0x1 << 15 | 0x7 << 8,
				 0x0 << 15 | rswap_masks << 8);

	mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL4,
			 0xf << 17,
			 0x1 << 19 |  /* userBit sel, 1: reg_value */
			 0x1 << 18 |  /* validBit sel, 1: reg_value */
			 0x1 << 17    /* reg_chst_sel, 1: reg_value */
			);

	earctx_dmac_mute(dmac_map, mute);
}

void earctx_dmac_set_format(struct regmap *dmac_map,
			    int frddr_idx, int msb, int frddr_type)
{
	mmio_update_bits(dmac_map, EARCTX_SPDIFOUT_CTRL1,
			 0x7 << 24 |  /* frddr src */
			 0xff << 16 | /* waiting count after enabled */
			 0x1f << 8  | /* msb position */
			 0x7 << 4,    /* frddr type */
			 frddr_idx << 24 |
			 0x40 << 16 |
			 msb << 8 |
			 frddr_type << 4);

	mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL4,
			 0x1f << 25,
			 0x1f << 25  /* msb */
			);

	/* for raw data */
	mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL3,
			 0x1f << 24,
			 0x17 << 24
			);
}

static unsigned int earctx_get_cs_bits(struct regmap *dmac_map,
				       int cs_offset, int mask)
{
	int reg_offset = cs_offset / REG_CS_LEN;
	int bits_offset = cs_offset % REG_CS_LEN;
	int reg, val, cs_a, cs_b;

	/* CHANNEL A STATUS */
	reg = EARCTX_SPDIFOUT_CHSTS0 + reg_offset;
	val = mmio_read(dmac_map, reg);
	cs_a = (val >> bits_offset) & mask;

	/* CHANNEL B STATUS */
	reg = EARCTX_SPDIFOUT_CHSTS6 + reg_offset;
	val = mmio_read(dmac_map, reg);
	cs_b = (val >> bits_offset) & mask;

	if (cs_a != cs_b)
		pr_warn("use CHANNEL A STATUS as default.\n");

	return cs_a;
}

/* Note: mask without offset */
static void earctx_update_cs_bits(struct regmap *dmac_map,
				  int cs_offset, int mask, int val)
{
	int reg_offset = cs_offset / REG_CS_LEN;
	int bits_offset = cs_offset % REG_CS_LEN;
	int reg, orig_val;

	/* CHANNEL A STATUS */
	reg = EARCTX_SPDIFOUT_CHSTS0 + reg_offset;
	orig_val = mmio_read(dmac_map, reg);
	orig_val &= ~(mask << bits_offset);
	orig_val |= (val & mask) << bits_offset;
	mmio_write(dmac_map, reg, orig_val);

	/* CHANNEL B STATUS */
	reg = EARCTX_SPDIFOUT_CHSTS6 + reg_offset;
	orig_val &= ~(mask << bits_offset);
	orig_val |= (val & mask) << bits_offset;
	mmio_write(dmac_map, reg, orig_val);
}

int earctx_get_cs_iec958(struct regmap *dmac_map)
{
	/* channel status A/B bits [31 - 0]*/
	return earctx_get_cs_bits(dmac_map,
		0x0, 0xffffffff);
}

void earctx_set_cs_iec958(struct regmap *dmac_map, int cs)
{
	/* channel status A/B bits [31 - 0]*/
	earctx_update_cs_bits(dmac_map, 0x0, 0xffffffff, cs);
}

void earctx_set_cs_mute(struct regmap *dmac_map, bool mute)
{
	earctx_update_cs_bits(dmac_map,
			      IEC_CS_MUTE_OFFSET,
			      IEC_CS_MUTE_MASK,
			      mute);
}

static void earctx_set_cs_fmt(struct regmap *dmac_map, enum earc_audio_fmt fmt)
{
	unsigned int offset, mask;

	if (fmt == EARC_FMT_LPCM || fmt == EARC_FMT_NONLPCM) {
		offset = 0x1;
		mask = 0x1;
	} else if (fmt == EARC_FMT_OBA) {
		int val;

		/* word length is not indicated for One Bit Audio */
		offset = IEC_CS_WLEN_OFFSET;
		mask = IEC_CS_WLEN_MASK;
		val = 0x0;
		earctx_update_cs_bits(dmac_map, offset, mask, val);

		/* use case 0 0 0 1 1 for bit 0,1,3,4 and 5 */
		offset = 0x0;
		mask = 0x3;
		val = 0;
		earctx_update_cs_bits(dmac_map, offset, mask, val);
		offset = 0x3;
		mask = 0x7;
		fmt = 0x6;
	} else {
		pr_err("%s, Not support eARC audio format:%#x\n",
		       __func__, fmt);
		return;
	}

	earctx_update_cs_bits(dmac_map, offset, mask, fmt);
}

static void earctx_set_cs_freq(struct regmap *dmac_map, unsigned int con_sf)
{
	unsigned int offset, mask;

	offset = IEC_CS_SFREQ_OFFSET;
	mask   = IEC_CS_SFREQ_MASK;
	earctx_update_cs_bits(dmac_map, offset, mask, con_sf);
}

static void earctx_set_cs_category(struct regmap *dmac_map, int ctg_code)
{
	unsigned int offset, mask;

	offset = IEC_CS_CATEGORY_OFFSET;
	mask   = IEC_CS_CATEGORY_MASK;
	earctx_update_cs_bits(dmac_map, offset, mask, ctg_code);
}

static void earctx_set_cs_multich(struct regmap *dmac_map, bool is_multich)
{
	unsigned int offset, mask, val;

	/* Multi-channel L-PCM */
	offset = IEC_CS_EARC_MULTICH_OFFSET;
	mask   = IEC_CS_EARC_MULTICH_MASK;
	if (is_multich)
		val = 0x2;
	else
		val = 0x0;

	earctx_update_cs_bits(dmac_map, offset, mask, val);
}

static void earctx_update_cs_layout(struct regmap *dmac_map,
				    enum audio_coding_types coding_type)
{
	unsigned int offset, mask;
	enum iec_audio_layout layout;

	switch (coding_type) {
	case AUDIO_CODING_TYPE_AC3_LAYOUT_B:
		layout = LOB_COMPRESS;
		break;
	case AUDIO_CODING_TYPE_AC3:
		layout = LOA_COMPRESS;
		break;
	case AUDIO_CODING_TYPE_MULTICH_32CH_LPCM:
		layout = LO32_LPCM;
		break;
	case AUDIO_CODING_TYPE_MULTICH_16CH_LPCM:
		layout = LO16_LPCM;
		break;
	case AUDIO_CODING_TYPE_MULTICH_8CH_LPCM:
		layout = LO8_LPCM;
		break;
	case AUDIO_CODING_TYPE_MULTICH_2CH_LPCM:
		layout = LO2_LPCM;
		break;
	case AUDIO_CODING_TYPE_SACD_12CH:
		layout = LO12_OBA;
		break;
	case AUDIO_CODING_TYPE_SACD_6CH:
		layout = LO6_OBA;
		break;
	default:
		layout = 0x0;
		break;
	}

	/* audio layout */
	offset = IEC_CS_AUDIO_LAYOUT_OFFSET;
	mask   = IEC_CS_AUDIO_LAYOUT_MASK;
	earctx_update_cs_bits(dmac_map, offset, mask, layout);
}

static unsigned int get_cs_ca(enum audio_coding_types coding_type)
{
	unsigned int ca = 0;

	switch (coding_type) {
	case AUDIO_CODING_TYPE_MULTICH_32CH_LPCM:
		ca = AIF_32CH;
		break;
	case AUDIO_CODING_TYPE_MULTICH_16CH_LPCM:
		ca = AIF_16CH;
		break;
	case AUDIO_CODING_TYPE_MULTICH_8CH_LPCM:
		ca = AIF_8CH;
		break;
	case AUDIO_CODING_TYPE_MULTICH_2CH_LPCM:
		ca = AIF_2CH;
		break;
	case AUDIO_CODING_TYPE_SACD_6CH:
	case AUDIO_CODING_TYPE_SACD_12CH:
		//	TODO: for one bit audio
		pr_warn("to Check Channel Allocation for One Bit Audio\n");
		break;
	default:
		ca = 0x0; /* clear the bits */
		break;
	}

	return ca;
}

void earctx_set_cs_ca(struct regmap *dmac_map, unsigned int ca)
{
	unsigned int offset, mask;

	/* AIF databyte 4 for channel allocation */
	offset = IEC_CS_AIF_DB4_OFFSET;
	mask = IEC_CS_AIF_DB4_MASK;
	earctx_update_cs_bits(dmac_map, offset, mask, ca);
}

void earctx_set_cs_info(struct regmap *dmac_map,
			enum audio_coding_types coding_type,
			struct iec_cnsmr_cs *cs_info,
			unsigned int *lpcm_ca)
{
	bool is_multich = false;
	unsigned int ca;

	if (coding_type == AUDIO_CODING_TYPE_MULTICH_32CH_LPCM ||
	    coding_type == AUDIO_CODING_TYPE_MULTICH_16CH_LPCM ||
	    coding_type == AUDIO_CODING_TYPE_MULTICH_8CH_LPCM ||
	    coding_type == AUDIO_CODING_TYPE_MULTICH_2CH_LPCM) {
		is_multich = true;

		if (*lpcm_ca) {
			ca = *lpcm_ca;
		} else {
			ca = get_cs_ca(coding_type);
			*lpcm_ca = ca;
		}
	} else {
		/* Default Channel Allocation */
		ca = get_cs_ca(coding_type);
	}

	/* fmt */
	earctx_set_cs_fmt(dmac_map, cs_info->fmt);

	/* Multi Channels */
	earctx_set_cs_multich(dmac_map, is_multich);
	/* category code */
	earctx_set_cs_category(dmac_map, cs_info->category_code);
	/* sample frequency */
	earctx_set_cs_freq(dmac_map, cs_info->sampling_freq);
	/* audio layout */
	earctx_update_cs_layout(dmac_map, coding_type);
	/* channel allocation */
	earctx_set_cs_ca(dmac_map, ca);
}

enum cmdc_st earctx_cmdc_get_state(struct regmap *cmdc_map)
{
	int val = mmio_read(cmdc_map, EARC_TX_CMDC_STATUS0);
	enum cmdc_st state = (enum cmdc_st)(val & 0x7);

	return state;
}

enum attend_type earctx_cmdc_get_attended_type(struct regmap *cmdc_map)
{
	int val = mmio_read(cmdc_map, EARC_TX_CMDC_STATUS0);
	enum cmdc_st state = (enum cmdc_st)(val & 0x7);
	enum attend_type tx_type = ATNDTYP_DISCNCT;

	if ((val & (1 << 0x3)) && state == CMDC_ST_ARC)
		tx_type = ATNDTYP_ARC;
	else if ((val & (1 << 0x4)) && (state == CMDC_ST_EARC))
		tx_type = ATNDTYP_EARC;

	return tx_type;
}

void earctx_cdmc_clr_irqs(struct regmap *top_map, int clr)
{
	mmio_write(top_map, EARCTX_CMDC_INT_PENDING, clr);
}

int earctx_cdmc_get_irqs(struct regmap *top_map)
{
	return mmio_read(top_map, EARCTX_CMDC_INT_PENDING);
}

void earctx_dmac_clr_irqs(struct regmap *top_map, int clr)
{
	mmio_write(top_map, EARCTX_DMAC_INT_PENDING, clr);
}

int earctx_dmac_get_irqs(struct regmap *top_map)
{
	return mmio_read(top_map, EARCTX_DMAC_INT_PENDING);
}

void earctx_compressed_enable(struct regmap *dmac_map,
			      enum attend_type type,
			      enum audio_coding_types coding_type,
			      bool enable)
{
	/*
	 * bch generate must be disabled if type is ARC
	 * otherwise there is no sound from ARC
	 */
	if (type == ATNDTYP_ARC)
		mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL3,
				 BIT(29),
				 0x0);

	if (type != ATNDTYP_EARC)
		return;

	if (audio_coding_is_non_lpcm(coding_type)) {
		mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL3,
				 0x1 << 29,
				 enable << 29);

		mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL4,
				 0x1 << 22,
				 enable << 22  /* valid Bit value */
				);

	} else {
		mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL3,
				 0x1 << 29,
				 0x0 << 29);

		mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL4,
				 0x1 << 22,
				 0x0 << 22  /* valid Bit value */
				);
	}

	mmio_update_bits(dmac_map, EARCTX_FE_CTRL0,
			 0x1 << 30,
			 enable << 30);
}

bool get_earctx_enable(struct regmap *cmdc_map, struct regmap *dmac_map)
{
	enum attend_type type = earctx_cmdc_get_attended_type(cmdc_map);

	if (type == ATNDTYP_DISCNCT)
		return false;

	if (mmio_read(dmac_map, EARCTX_SPDIFOUT_CTRL0) & 0x1 << 28)
		return true;

	return false;
}

void earctx_enable(struct regmap *top_map,
		   struct regmap *cmdc_map,
		   struct regmap *dmac_map,
		   enum audio_coding_types coding_type,
		   bool enable,
		   bool rterm_on)
{
	enum attend_type type = earctx_cmdc_get_attended_type(cmdc_map);

	if (type == ATNDTYP_DISCNCT)
		return;

	if (enable) {
		int offset, mask, val;

		if (rterm_on) {
			offset = 19;
			mask = 0x1f;
			val = 0x10;
		} else {
			offset = 20;
			mask = 0xf;
			val = 0x8;
		}
		if (type == ATNDTYP_ARC) {
			mmio_update_bits(top_map,
					 EARCTX_ANA_CTRL0,
					 mask << offset,
					 (val + 1) << offset);
		} else if (type == ATNDTYP_EARC) {
			mmio_update_bits(top_map,
					 EARCTX_ANA_CTRL0,
					 mask << offset,
					 val << offset);
		}

		/* first biphase work clear, and then start */
		mmio_update_bits(dmac_map, EARCTX_SPDIFOUT_CTRL0,
				 0x1 << 30,
				 0x1 << 30);
		mmio_update_bits(dmac_map, EARCTX_SPDIFOUT_CTRL0,
				 0x3 << 28,
				 0x0);

		mmio_update_bits(dmac_map, EARCTX_SPDIFOUT_CTRL0,
				 0x1 << 29, /* afifo out reset */
				 0x1 << 29);
		mmio_update_bits(dmac_map, EARCTX_SPDIFOUT_CTRL0,
				 0x1 << 28, /* afifo in reset */
				 0x1 << 28);
		mmio_update_bits(dmac_map, EARCTX_SPDIFOUT_CTRL0,
				 0x1 << 31,
				 0x1 << 31);
	} else {
		/* earc tx is not disable, only mute, ensure earc outputs zero data */
		mmio_update_bits(dmac_map, EARCTX_SPDIFOUT_CTRL0,
				 0x3 << 21,
				 0x3 << 21);
		return;
	}

	mmio_write(top_map, EARCTX_DMAC_INT_MASK,
		   (0x1 << 4)	| /* fe_modulation c_hold_clr */
		   (0x1 << 3)	| /* fe_modulation c_hold_start */
		   (0x1 << 2)	| /* err_correct c_fifo_thd_less_pass */
		   (0x1 << 1)	| /* err_correct r_fifo_overflow_set */
		   (0x1 << 0)	  /* err_correct c_fifo_empty_set */
		   );

	earctx_compressed_enable(dmac_map,
				 type,
				 coding_type,
				 enable);
}

static void earcrx_cmdc_get_reg(struct regmap *cmdc_map, int dev_id, int offset,
				u8 *data, int bytes)
{
	int i;

	mmio_update_bits(cmdc_map, EARC_RX_CMDC_DEVICE_ID_CTRL,
			 0x1 << 31 | /* apb_write  */
			 0x1 << 30 | /* apb_read */
			 0x1 << 29 | /* apb_w_r_done */
			 0xff << 8 | /* apb_rwid */
			 0xff << 0,   /* apbrw_start_addr */
			 0x0 << 31 |
			 0x1 << 30 |
			 0x0 << 29 |
			 dev_id << 8 |
			 offset << 0);

	for (i = 0; i < bytes; i++)
		data[i] = mmio_read(cmdc_map, EARC_RX_CMDC_DEVICE_RDATA);

	mmio_update_bits(cmdc_map, EARC_RX_CMDC_DEVICE_ID_CTRL,
			 0x1 << 29, 0x1 << 29);
	mmio_update_bits(cmdc_map, EARC_RX_CMDC_DEVICE_ID_CTRL,
			 0x1 << 29, 0x0 << 29);
}

static void earcrx_cmdc_set_reg(struct regmap *cmdc_map, int dev_id, int offset,
				u8 *data, int bytes)
{
	int i;

	mmio_update_bits(cmdc_map, EARC_RX_CMDC_DEVICE_ID_CTRL,
			 0x1 << 31 | /* apb_write  */
			 0x1 << 30 | /* apb_read */
			 0x1 << 29 | /* apb_w_r_done */
			 0xff << 8 | /* apb_rwid */
			 0xff << 0,   /* apbrw_start_addr */
			 0x1 << 31 |
			 0x0 << 30 |
			 0x0 << 29 |
			 dev_id << 8 |
			 offset << 0);

	for (i = 0; i < bytes; i++)
		mmio_write(cmdc_map, EARC_RX_CMDC_DEVICE_WDATA, data[i]);

	mmio_update_bits(cmdc_map, EARC_RX_CMDC_DEVICE_ID_CTRL,
			 0x1 << 29, 0x1 << 29);
	mmio_update_bits(cmdc_map, EARC_RX_CMDC_DEVICE_ID_CTRL,
			 0x1 << 29, 0x0 << 29);
}

/* Latency */
void earcrx_cmdc_get_latency(struct regmap *cmdc_map, u8 *latency)
{
	earcrx_cmdc_get_reg(cmdc_map,
			    STAT_CTRL_DEV_ID,
			    ERX_LATENCY_REG,
			    latency,
			    1);
}

void earcrx_cmdc_set_latency(struct regmap *cmdc_map, u8 *latency)
{
	u8 stat = (0x1 << STAT_CHNG) | earcrx_cmdc_get_rx_stat_bits(cmdc_map);

	earcrx_cmdc_set_reg(cmdc_map,
			    STAT_CTRL_DEV_ID,
			    ERX_LATENCY_REG,
			    latency,
			    1);

	/* set STAT_CHNG to Notify eARC TX */
	earcrx_cmdc_set_reg(cmdc_map,
			    STAT_CTRL_DEV_ID,
			    EARCRX_STAT_REG,
			    &stat,
			    0x1);
}

void earcrx_cmdc_get_cds(struct regmap *cmdc_map, u8 *cds)
{
	earcrx_cmdc_get_reg(cmdc_map,
			    CAP_DEV_ID,
			    0x0,
			    cds,
			    CDS_MAX_BYTES);
}

void earcrx_cmdc_set_cds(struct regmap *cmdc_map, u8 *cds)
{
	u8 cap_chng = 0x1 << CAP_CHNG |
		      0x1 << EARC_HPD |
		      earcrx_cmdc_get_rx_stat_bits(cmdc_map);

	earcrx_cmdc_set_reg(cmdc_map,
			    CAP_DEV_ID,
			    0x0,
			    cds,
			    CDS_MAX_BYTES);

	/* set CAP_CHNG and EARC_HPD to Notify eARC TX to re-read CDS */
	earcrx_cmdc_set_reg(cmdc_map,
			    STAT_CTRL_DEV_ID,
			    EARCRX_STAT_REG,
			    &cap_chng,
			    0x1);
}

static int earctx_cmdc_get_reg(struct regmap *cmdc_map, int dev_id, int offset,
			       u8 *data, int bytes)
{
	int val = 0, i;
	int ret = -1;

	mmio_update_bits(cmdc_map, EARC_TX_CMDC_MASTER_CTRL,
			 0x1 << 31 | /* master_cmd_rw, read */
			 0x1 << 30 | /* master_hb_ignore */
			 0xf << 24 | /* hb_cmd_val_th */
			 0xff << 16 | /* master_cmd_count */
			 0xff << 8 | /* master_cmd_id */
			 0xff << 0,  /* master_cmd_address */
			 0x0 << 31 |
			 0x1 << 30 |
			 0x4 << 24 |
			 (bytes - 1) << 16 |
			 dev_id << 8 |
			 offset << 0);

	/* wait read from rx done */
	do {
		usleep_range(500, 1500);
		val = mmio_read(cmdc_map, EARC_TX_CMDC_MASTER_CTRL);
	} while (!(val & (1 << 29)));

	mmio_update_bits(cmdc_map, EARC_TX_CMDC_DEVICE_ID_CTRL,
			 0x1 << 31 | /* apb_write */
			 0x1 << 30 | /* apb_read */
			 0x1 << 29 | /* apb_rw_done */
			 0x1 << 16 | /* hpb_rst_enable */
			 0xff << 8 | /* apb_rwid */
			 0xff << 0,   /* apbrw_start_addr */
			 0x0 << 31 |
			 0x1 << 30 |
			 0x0 << 29 |
			 0x1 << 16 |
			 dev_id << 8 |
			 offset << 0);

	for (i = 0; i < bytes; i++)
		data[i] = mmio_read(cmdc_map, EARC_TX_CMDC_DEVICE_RDATA);

	mmio_update_bits(cmdc_map, EARC_TX_CMDC_DEVICE_ID_CTRL,
			 0x1 << 29, 0x1 << 29);
	mmio_update_bits(cmdc_map, EARC_TX_CMDC_DEVICE_ID_CTRL,
			 0x1 << 29, 0x0 << 29);

	if (val & (1 << 29))
		ret = 0;

	return ret;
}

static int earctx_cmdc_set_reg(struct regmap *cmdc_map, int dev_id, int offset,
			       u8 *data, int bytes)
{
	int val = 0, i;
	int ret = -1;

	mmio_update_bits(cmdc_map, EARC_TX_CMDC_DEVICE_ID_CTRL,
			 0x1 << 31 | /* apb_write */
			 0x1 << 30 | /* apb_read */
			 0x1 << 29 | /* apb_rw_done */
			 0x1 << 16 | /* hpb_rst_enable */
			 0xff << 8 | /* apb_rwid */
			 0xff << 0,   /* apbrw_start_addr */
			 0x1 << 31 |
			 0x0 << 30 |
			 0x0 << 29 |
			 0x1 << 16 |
			 dev_id << 8 |
			 offset << 0);

	for (i = 0; i < bytes; i++)
		mmio_update_bits(cmdc_map, EARC_TX_CMDC_DEVICE_WDATA,
				 0xff << 0,
				 data[i] << 0);

	mmio_update_bits(cmdc_map, EARC_TX_CMDC_MASTER_CTRL,
			 0x1 << 31 | /* master_cmd_rw, write */
			 0x1 << 30 | /* master_hb_ignore */
			 0xf << 24 | /* hb_cmd_cal_th */
			 0xff << 16 | /* master_cmd_count */
			 0xff << 8 | /* master_cmd_id */
			 0xff << 0,  /* master_cmd_address */
			 0x1 << 31 |
			 0x1 << 30 |
			 4 << 24 |
			 (bytes - 1) << 16 |
			 dev_id << 8 |
			 offset << 0);

	/* wait write done */
	do {
		usleep_range(500, 1500);
		val = mmio_read(cmdc_map, EARC_TX_CMDC_MASTER_CTRL);
	} while (!(val & (1 << 29)));

	mmio_update_bits(cmdc_map, EARC_TX_CMDC_DEVICE_ID_CTRL,
			 0x1 << 29, 0x1 << 29);
	mmio_update_bits(cmdc_map, EARC_TX_CMDC_DEVICE_ID_CTRL,
			 0x1 << 29, 0x0 << 29);

	if (val & (1 << 29))
		ret = 0;

	return ret;
}

/*
 * EARC_TX_CMDC_STATUS0:
 * bit 31: reserved, ro_cmdc_status0, unsigned, R0, default = 0
 * bit 30: rx_stat_chng
 * bit 29: rx_cap_chng
 * bit 28: rx_earc_HPD
 * bit 27: tx_earc_valid
 * bit 26: tx_stat_chng_conf
 * bit 25: tx_cap_chng_conf
 * bit 24: hdmi_HPD
 */
int earctx_cmdc_get_tx_stat_bits(struct regmap *cmdc_map)
{
	return mmio_read(cmdc_map, EARC_TX_CMDC_STATUS0) >> 24;
}

int earctx_cmdc_get_rx_stat_bits(struct regmap *cmdc_map)
{
	u8 stat_bits;

	earctx_cmdc_get_reg(cmdc_map,
			    STAT_CTRL_DEV_ID,
			    EARCRX_STAT_REG,
			    &stat_bits,
			    1);

	return (int)stat_bits;
}

void earctx_cmdc_clr_stat_bits(struct regmap *cmdc_map, int stat_bits)
{
	u8 clr_bits, reg_data = 0;

	earctx_cmdc_get_reg(cmdc_map,
			    STAT_CTRL_DEV_ID,
			    EARCTX_STAT_REG,
			    &reg_data,
			    1);

	clr_bits = reg_data & (~stat_bits);

	earctx_cmdc_set_reg(cmdc_map,
			    STAT_CTRL_DEV_ID,
			    EARCTX_STAT_REG,
			    &clr_bits,
			    1);
}

void earctx_cmdc_set_hdmi_hpd_bit(struct regmap *cmdc_map, bool is_high)
{
	u8 hpd_bits;

	if (is_high)
		hpd_bits = 0x1 << HDMI_HPD;
	else
		hpd_bits = 0x0 << HDMI_HPD;

	earctx_cmdc_set_reg(cmdc_map,
			    STAT_CTRL_DEV_ID,
			    EARCTX_STAT_REG,
			    &hpd_bits,
			    1);
}

void earctx_cmdc_set_hb_valid_bit(struct regmap *cmdc_map, bool hb_valid)
{
	u8 valid;

	if (hb_valid)
		valid = 0x1 << EARC_VALID;
	else
		valid = 0x0 << EARC_VALID;

	earctx_cmdc_set_reg(cmdc_map,
			    STAT_CTRL_DEV_ID,
			    EARCTX_STAT_REG,
			    &valid,
			    1);
}

/* Latency */
void earctx_cmdc_get_latency(struct regmap *cmdc_map, u8 *latency)
{
	earctx_cmdc_get_reg(cmdc_map,
			    STAT_CTRL_DEV_ID,
			    ERX_LATENCY_REG,
			    latency,
			    1);
}

void earctx_cmdc_set_latency(struct regmap *cmdc_map, u8 *latency)
{
	earctx_cmdc_set_reg(cmdc_map,
			    STAT_CTRL_DEV_ID,
			    ERX_LATENCY_REQ_REG,
			    latency,
			    1);
}

/* Capability Data Structure, fetch CDS from RX */
void earctx_cmdc_get_cds(struct regmap *cmdc_map, u8 *cds)
{
	earctx_cmdc_get_reg(cmdc_map,
			    CAP_DEV_ID,
			    0x0,
			    cds,
			    CDS_MAX_BYTES);
}

/* eARC RX analog auto calibration */
void earcrx_ana_auto_cal(struct regmap *top_map)
{
	int stat0 = 0;

	mmio_update_bits(top_map,
			 EARCRX_ANA_CTRL1,
			 0x3 << 30,	/* earcrx_rterm_cal_rstn & enable */
			 0x3 << 30);

	do {
		usleep_range(10, 30);
		stat0 = mmio_read(top_map, EARCRX_ANA_STAT0);
	} while (!(stat0 & (1 << 31)));

	mmio_update_bits(top_map,
			 EARCRX_ANA_CTRL0,
			 0x1f << 15,
			 (stat0 & 0x1f) << 15);
}

/* eARC RX analog auto calibration */
void earctx_ana_auto_cal(struct regmap *top_map)
{
	int stat0 = 0;

	mmio_update_bits(top_map,
			 EARCTX_ANA_CTRL1,
			 0x3 << 30, /* earctx_rterm_cal_rstn & enable */
			 0x3 << 30);

	do {
		usleep_range(10, 30);
		stat0 = mmio_read(top_map, EARCTX_ANA_STAT0);
	} while (!(stat0 & (1 << 31)));

	mmio_update_bits(top_map,
			 EARCTX_ANA_CTRL0,
			 0x1f << 12,
			 (stat0 & 0x1f) << 12);
}

bool earxrx_get_pll_valid(struct regmap *top_map)
{
	int stat0 = 0;
	unsigned int value = 0;

	value = mmio_read(top_map, EARCRX_PLL_STAT0);

	stat0 = (value & 0x80000000) >> 31;

	return stat0;
}

bool earxrx_get_pll_valid_auto(struct regmap *top_map)
{
	int stat0 = 0;
	unsigned int value = 0;

	value = mmio_read(top_map, EARCRX_PLL_STAT0);

	stat0 = (value & 0x40000000) >> 30;

	return stat0;
}

u8 earcrx_cmdc_get_rx_stat_bits(struct regmap *cmdc_map)
{
	u8 stat_bits;

	earcrx_cmdc_get_reg(cmdc_map,
			    STAT_CTRL_DEV_ID,
			    EARCRX_STAT_REG,
			    &stat_bits,
			    1);
	return stat_bits;
}

void earctx_cmdc_earc_mode(struct regmap *cmdc_map, bool enable)
{
	if (enable) {
		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_VSM_CTRL0,
				 0xf << 28 | 0x3 << 8,
				 0x0);
		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_VSM_CTRL0,
				 0x1 << 26,
				 0x1 << 26);
		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_VSM_CTRL0,
				 0x1 << 26,
				 0x0);
	} else {
		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_VSM_CTRL0,
				 0xf << 28 | 0x3 << 8,
				 0xe << 28 | 0x3 << 8);
	}
}

int earcrx_get_sample_rate(struct regmap *dmac_map)
{
	unsigned int val;
	/*EE_AUDIO_SPDIFIN_STAT0*/
	/*r_width_max bit17:8 (the max width of two edge;)*/
	unsigned int max_width = 0;

	val = mmio_read(dmac_map, EARCRX_SPDIFIN_SAMPLE_STAT0);

	/* NA when check min width of two edges */
	if (((val >> 18) & 0x3ff) == 0x3ff)
		return 7;

	/*check the max width of two edge when spdifin sr=32kHz*/
	/*if max_width is more than 0x2f0(magic number),*/
	/*sr(32kHz) is unavailable*/
	max_width = ((val >> 8) & 0x3ff);

	if ((((val >> 28) & 0x7) == 0) && max_width == 0x3ff)
		return 7;

	return (val >> 28) & 0x7;
}
