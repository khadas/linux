// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/delay.h>

#include "locker_hw.h"

static void audiolocker_reset(void)
{
	audiolocker_write(AUD_LOCK_SW_RESET, 0x1);
	audiolocker_write(AUD_LOCK_EN, 0x1);
}

void audiolocker_disable(void)
{
	audiolocker_write(AUD_LOCK_SW_RESET, 0x0);
	audiolocker_write(AUD_LOCK_EN, 0x0);
}

void audiolocker_irq_config(void)
{
	audiolocker_write(AUD_LOCK_REFCLK_DS_INT, 0);
	audiolocker_write(AUD_LOCK_IMCLK_DS_INT, 0);
	audiolocker_write(AUD_LOCK_OMCLK_DS_INT, 0);

	audiolocker_write(AUD_LOCK_REFCLK_LAT_INT, 0x4000000);

	audiolocker_write(AUD_LOCK_SW_LATCH, 0xf);
	audiolocker_write(AUD_LOCK_HW_LATCH, 0xf);
	audiolocker_write(AUD_LOCK_INT_CTRL, 0xe);

	audiolocker_reset();
}

int add_cnt = 0, reduce_cnt = 0;

void audiolocker_update_clks(struct clk *clk_calc, struct clk *clk_ref)
{
	int in_count, out_count;
	int mpll2_rate, mpll1_rate;

	in_count = audiolocker_read(RO_REF2IMCLK_CNT_L);
	out_count = audiolocker_read(RO_REF2OMCLK_CNT_L);
	pr_info("\tin count:%d, out count:%d\n", in_count, out_count);

	if (in_count < out_count) {
		add_cnt++;
		mpll1_rate = clk_get_rate(clk_calc);
		mpll2_rate = clk_get_rate(clk_ref);

		pr_info("\t    add cnt:%d, mpll1_rate:%d mpll2 rate:%d\n",
			add_cnt, mpll1_rate, mpll2_rate);

		clk_set_rate(clk_ref, mpll2_rate + 600);
		/*udelay(1);*/
		audiolocker_reset();
	} else if (in_count > out_count) {
		reduce_cnt++;
		mpll1_rate = clk_get_rate(clk_calc);
		mpll2_rate = clk_get_rate(clk_ref);

		pr_info("\t reduce cnt:%d, mpll1_rate:%d, mpll2 rate:%d\n",
			reduce_cnt, mpll1_rate, mpll2_rate);

		clk_set_rate(clk_ref, mpll2_rate - 600);
		/*udelay(1);*/
		audiolocker_reset();
	}

	audiolocker_write(AUD_LOCK_INT_CLR, 0x3);
}

