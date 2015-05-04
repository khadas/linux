/*
 * drivers/amlogic/input/saradc/saradc_reg.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#ifndef __SARADC_REG_H__
#define __SARADC_REG_H__


struct saradc_regs {
	unsigned int reg0;
	unsigned int ch_list;
	unsigned int avg_cntl;
	unsigned int reg3;
	unsigned int delay;
	unsigned int last_rd;
	unsigned int fifo_rd;
	unsigned int aux_sw;
	unsigned int ch10_sw;
	unsigned int detect_idle_sw;
	unsigned int delta_10;
	/* unsigned int reg11; for temp-sensor */
};

/* REG0 */
struct saradc_reg0 {
	unsigned int sample_engine_en:1;        /* [0] */
	unsigned int continuous_sample_en:1;    /* [1] */
	unsigned int start_sample:1;            /* [2] */
	unsigned int fifo_irq_en:1;             /* [3] */
	unsigned int fifo_irq_count:5;          /* [4-8] */
	unsigned int detect_irq_en:1;           /* [9] */
	unsigned int detect_irq_pol:1;          /* [10] */
	unsigned int unused:1;                  /* [11] */
	unsigned int ch0_delta_en:1;            /* [12] */
	unsigned int ch1_delta_en:1;            /* [13] */
	unsigned int stop_sample:1;             /* [14] */
	unsigned int temp_sens_sel:1;           /* [15] */
	unsigned int cur_ch_id:3;               /* [16-18] */
	unsigned int unused2:2;                 /* [19-20] */
	unsigned int fifo_count:5;              /* [21-25] */
	unsigned int fifo_empty:1;              /* [26] */
	unsigned int fifo_full:1;               /* [27] */
	unsigned int sample_busy:1;             /* [28] */
	unsigned int avg_busy:1;                /* [29] */
	unsigned int delta_busy:1;              /* [30] */
	unsigned int detect_level:1;            /* [31] */
};

/* REG3 */
struct saradc_reg3 {
	unsigned int block_delay_count:8;         /* [0-7] */
	unsigned int block_delay_tb:2;            /* [8-9] */
	unsigned int clk_div:6;                   /* [10-15] */
	unsigned int panel_detect_filter_tb:2;    /* [16-17] */
	unsigned int panel_detect_filter_count:3; /* [18-20] */
	unsigned int adc_en:1;                    /* [21] */
	unsigned int detect_pullup_en:1;          /* [22] */
	unsigned int cal_cntl:3;                  /* [23-25] */
	unsigned int sc_phase:1;                  /* [26] */
	unsigned int continuous_ring_counter_en:1;/* [27] */
	unsigned int unused:2;                    /* [28-29] */
	unsigned int clk_en:1;                    /* [30] */
	unsigned int cntl_use_sc_delay:1;         /* [31] */
};

#endif
