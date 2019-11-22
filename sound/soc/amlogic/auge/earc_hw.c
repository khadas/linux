/*
 * sound/soc/amlogic/auge/earc_hw.c
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/amlogic/media/sound/spdif_info.h>
#include "earc_hw.h"

void earcrx_pll_refresh(struct regmap *top_map)
{
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
}
void earcrx_cmdc_init(struct regmap *top_map)
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
		   (0 <<  7) |  /* status_ch_int */
		   (0 <<  6) |  /* int_rec_invalid_id */
		   (0 <<  5) |  /* int_rec_invalid_offset */
		   (0 <<  4) |  /* int_rec_unexp */
		   (0 <<  3) |  /* int_rec_ecc_err */
		   (0 <<  2) |  /* int_rec_parity_err */
		   (0 <<  1) |  /* int_recv_packet */
		   (0 <<  0)	 /* int_rec_time_out */
		  );

	mmio_write(top_map, EARCRX_ANA_CTRL0,
		   0x1  << 31 | /* earcrx_en_d2a */
		   0x10 << 24 | /* earcrx_cmdcrx_reftrim */
		   0x8  << 20 | /* earcrx_idr_trim */
		   0x10 << 15 | /* earcrx_rterm_trim */
		   0x4  << 12 | /* earcrx_cmdctx_ack_hystrim */
		   0x10 << 7  | /* earcrx_cmdctx_ack_reftrim */
		   0x1  << 4  | /* earcrx_cmdcrx_rcfilter_sel */
		   0x4  << 0    /* earcrx_cmdcrx_hystrim */
		  );

	mmio_write(top_map, EARCRX_PLL_CTRL3,
		   0x2 << 20 | /* earcrx_pll_bias_adj */
		   0x4 << 16 | /* earcrx_pll_rou */
		   0x1 << 13   /* earcrx_pll_dco_sdm_e */
		  );

	mmio_write(top_map, EARCRX_PLL_CTRL0,
		   0x1 << 28 | /* earcrx_pll_en */
		   0x1 << 23 | /* earcrx_pll_dmacrx_sqout_rstn_sel */
		   0x1 << 10   /* earcrx_pll_n */
		  );
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
				 0xf << 0,
				 0xf << 0);
		mmio_update_bits(cmdc_map,
				 EARC_RX_CMDC_TOP_CTRL1,
				 0xf << 0,
				 0x0 << 0);
	}
}

void earcrx_dmac_init(struct regmap *top_map, struct regmap *dmac_map)
{
	mmio_write(top_map, EARCRX_DMAC_INT_MASK,
		   (0x0 << 17) | /* earcrx_ana_rst c_new_format_set */
		   (0x0 << 16) | /* earcrx_ana_rst c_earcrx_div2_hold_set */
		   (0x0 << 15) | /* earcrx_err_correct c_bcherr_int_set */
		   (0x0 << 14) | /* earcrx_err_correct r_afifo_overflow_set */
		   (0x0 << 13) | /* earcrx_err_correct r_fifo_overflow_set */
		   (0x0 << 12) | /* earcrx_user_bit_check r_fifo_overflow */
		   (0x0 << 11) | /* earcrx_user_bit_check c_fifo_thd_pass */
		   (0x0 << 10) | /* earcrx_user_bit_check c_u_pk_lost_int_set */
		   (0x0 << 9)	| /* arcrx_user_bit_check c_iu_pk_end */
		   (0x0 << 8)	| /* arcrx_biphase_decode c_chst_mute_clr */
		   (0x1 << 7)	| /* arcrx_biphase_decode c_find_papb */
		   (0x1 << 6)	| /* arcrx_biphase_decode c_valid_change */
		   (0x1 << 5)	| /* arcrx_biphase_decode c_find_nonpcm2pcm */
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
	mmio_write(dmac_map, EARCRX_ANA_RST_CTRL0, 1 << 31);
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
		   0x1 << 30 | /* reg_chnum_sel */
		   0x1 << 25 | /* reg_findpapb_en */
		   0x1 << 24 | /* nonpcm2pcm_th enable */
		   0xFFF << 12 |  /* reg_nonpcm2pcm_th */
		   0x1 << 2    /* reg_check_parity */
		  );
	mmio_write(dmac_map,
		   EARCRX_SPDIFIN_CTRL2,
		   (1 << 14) | /* reg_earc_auto */
		   (1 << 13)  /* reg_earcin_papb_lr */
		  );
	mmio_write(dmac_map,
		   EARCRX_SPDIFIN_CTRL3,
		   (0xEC37 << 16) | /* reg_earc_pa_value */
		   (0x5A5A << 0)    /* reg_earc_pb_value */
		  );
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

	if ((val & (1 << 0x3)) && (state == CMDC_ST_ARC))
		type = ATNDTYP_ARC;
	else if ((val & (1 << 0x4)) && (state == CMDC_ST_EARC))
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

void earcrx_dmac_clr_irqs(struct regmap *top_map, int clr)
{
	mmio_write(top_map, EARCRX_DMAC_INT_PENDING, clr);
}

int earcrx_dmac_get_irqs(struct regmap *top_map)
{
	return mmio_read(top_map, EARCRX_DMAC_INT_PENDING);
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

	if (type == ATNDTYP_EARC)
		mmio_update_bits(dmac_map, EARCRX_DMAC_SYNC_CTRL0,
				 1 << 31,	 /* reg_work_en */
				 enable << 31);
	else if (type == ATNDTYP_ARC) {
		mmio_update_bits(dmac_map,
				 EARCRX_SPDIFIN_SAMPLE_CTRL0,
				 0x1 << 31, /* reg_work_enable */
				 enable << 31);

		mmio_write(dmac_map, EARCRX_DMAC_SYNC_CTRL0, 0x0);
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
		   (0x0 << 7) |  /* status_ch_int */
		   (0x0 << 6) |  /* int_recv_finished */
		   (0x0 << 5) |  /* int_recv_nack */
		   (0x0 << 4) |  /* int_recv_norsp */
		   (0x0 << 3) |  /* int_recv_unexp */
		   (0x0 << 2) |  /* int_recv_data */
		   (0x0 << 1) |  /* int_recv_ack */
		   (0x0 << 0)	  /* int_recv_ecc_err */
		  );
}

void earctx_cmdc_init(struct regmap *top_map)
{
	/* ana */
	mmio_write(top_map, EARCTX_ANA_CTRL0,
		   0x1 << 31 |  /* earctx_en_d2a */
		   0x1 << 28 |  /* earctx_cmdcrx_rcfilter_sel */
		   0x4 << 26 |  /* earctx_cmdcrx_hystrim */
		   0x8 << 20 |  /* earctx_idr_trim */
		   0x10 << 12 | /* earctx_rterm_trim */
		   0x4 << 8 |   /* earctx_dmac_slew_con */
		   0x4 << 5 |  /* earctx_cmdctx_ack_hystrim */
		   0x10 << 0   /* earctx_cmdctx_ack_reftrim */
		  );
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
			 0xf << 8,
			 0x1 << 11 |    /* hdmi_hpd invent */
			 earc_port << 8 /* hdmi_hpd mux, port 0/1/2 */
			);

	if (st) {
		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_VSM_CTRL0,
				 0x1 << 19,
				 0x1 << 19	/* comma_cnt_rst */
				);

		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_VSM_CTRL0,
				 0x1 << 19 | 0xf << 20,
				 0x0 << 19 |   /* comma_cnt_rst */
				 0x3 << 20 | 0x3 << 22
				);

		mmio_update_bits(cmdc_map,
				 EARC_TX_CMDC_VSM_CTRL1,
				 0xff << 0,
				 0x4 << 0   /* comma_cnt_th */
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
	}
}

void earctx_dmac_init(struct regmap *top_map, struct regmap *dmac_map)
{
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
			 0x1 << 18 | /* biphase encode valid Bit value sel */
			 0xff << 4,   /* lane mask */
			 0x1 << 28 |
			 0x1 << 26 |
			 0x1 << 24 |
			 0x1 << 20 |
			 0x0 << 19 |
			 0x1 << 18 |
			 0x3 << 4   //  ODO: lane 0 now
			);

	mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL0,
			 0x1 << 16 | /* reg_ubit_fifo_init_n */
			 0x7 << 8  | /* r channel select */
			 0x7 << 4,   /* l channel select */
			 0x1 << 16 |
			 0x1 << 8 |
			 0x0 << 4);

	mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL4,
			 0xf << 17,
			 0x1 << 19 |  /* userBit sel, 1: reg_value */
			 0x1 << 18 |  /* validBit sel, 1: reg_value */
			 0x1 << 17    /* reg_chst_sel, 1: reg_value */
			);
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

void earctx_set_channel_status_info(struct regmap *dmac_map,
				    struct iec958_chsts *chsts)
{
	/* ch status = reg_chsts0~B */

	/* L Channel */
	mmio_write(dmac_map, EARCTX_SPDIFOUT_CHSTS0,
		   ((chsts->chstat1_l >> 8) & 0xf) << 24 | chsts->chstat0_l);

	/* R Channel */
	mmio_write(dmac_map, EARCTX_SPDIFOUT_CHSTS6,
		   ((chsts->chstat1_r >> 8) & 0xf) << 24 | chsts->chstat0_r);
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

	if ((val & (1 << 0x3)) && (state == CMDC_ST_ARC))
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

void earctx_enable(struct regmap *top_map,
		   struct regmap *cmdc_map,
		   struct regmap *dmac_map,
		   bool enable)
{
	mmio_update_bits(dmac_map, EARCTX_SPDIFOUT_CTRL0,
			 0x1 << 31,
			 enable << 31);

	mmio_update_bits(dmac_map, EARC_RX_CMDC_BIPHASE_CTRL1,
			 0x1 << 30,
			 enable << 30);

	if (enable)
		mmio_write(top_map, EARCTX_DMAC_INT_MASK,
			   (0x1 << 4)	| /* fe_modulation c_hold_clr */
			   (0x1 << 3)	| /* fe_modulation c_hold_start */
			   (0x1 << 2)	| /* err_correct c_fifo_thd_less_pass */
			   (0x1 << 1)	| /* err_correct r_fifo_overflow_set */
			   (0x1 << 0)	  /* err_correct c_fifo_empty_set */
			   );
	else
		mmio_write(top_map, EARCTX_DMAC_INT_MASK,
			   0x0
			   );

	if (earctx_cmdc_get_attended_type(cmdc_map) == ATNDTYP_EARC) {
		if (spdifout_is_raw()) {
			mmio_update_bits(dmac_map, EARCTX_ERR_CORRT_CTRL3,
					 0x1 << 29,
					 enable << 29);
		}
		mmio_update_bits(dmac_map, EARCTX_FE_CTRL0,
				 0x1 << 30,
				 enable << 30);
	}
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

	for (i = 0; i < bytes; i++) {
		data[i] = mmio_read(cmdc_map, EARC_RX_CMDC_DEVICE_RDATA);
		pr_info("%s, data[%d]:%#x\n", __func__, i, data[i]);
	}

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

	for (i = 0; i < bytes; i++) {
		pr_info("%s, data[%d]:%#x\n", __func__, i, data[i]);
		mmio_write(cmdc_map, EARC_RX_CMDC_DEVICE_WDATA, data[i]);
	}

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
	earcrx_cmdc_set_reg(cmdc_map,
			    STAT_CTRL_DEV_ID,
			    ERX_LATENCY_REG,
			    latency,
			    1);
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
	earcrx_cmdc_set_reg(cmdc_map,
			    CAP_DEV_ID,
			    0x0,
			    cds,
			    CDS_MAX_BYTES);
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
	while (!(val & (1 << 29))) {
		usleep_range(500, 1500);
		val = mmio_read(cmdc_map, EARC_TX_CMDC_MASTER_CTRL);
	}

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

	for (i = 0; i < bytes; i++) {
		data[i] = mmio_read(cmdc_map, EARC_TX_CMDC_DEVICE_RDATA);
		pr_info("%s, bytes:%d, data[%d]:%#x\n",
			__func__,
			bytes,
			i,
			data[i]);
	}

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
	int cnt = 0;

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

	for (i = 0; i < bytes; i++) {
		mmio_update_bits(cmdc_map, EARC_TX_CMDC_DEVICE_WDATA,
				 0xff << 0,
				 data[i] << 0);
		pr_info("%s, data[%d]:%#x, bytes:%d\n",
			__func__,
			i,
			data[i],
			bytes);
	}

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
	while (!(val & (1 << 29))) {
		usleep_range(500, 1500);
		val = mmio_read(cmdc_map, EARC_TX_CMDC_MASTER_CTRL);
		cnt++;
	}
	pr_info("%s, cnt:%d\n", __func__, cnt);

	mmio_update_bits(cmdc_map, EARC_TX_CMDC_DEVICE_ID_CTRL,
			 0x1 << 29, 0x1 << 29);
	mmio_update_bits(cmdc_map, EARC_TX_CMDC_DEVICE_ID_CTRL,
			 0x1 << 29, 0x0 << 29);

	if (val & (1 << 29))
		ret = 0;

	return ret;
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
