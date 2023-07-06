// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/media/vout/peripheral_lcd.h>
#include "../peripheral_lcd_drv.h"
#include <linux/dma-mapping.h>

static int spd2010_qspi_flush_frame(unsigned char *addr)
{
	int ret;
	unsigned short vline;
	void *cbuf, *dbuf;
	unsigned int dlen;

	unsigned char wr_line0_head[4] = {0x32, 0x00, 0x2c, 0x00};
	unsigned char wr_linex_head[4] = {0x32, 0x00, 0x3c, 0x00};

	cbuf = kzalloc(4 * sizeof(unsigned char), GFP_KERNEL);
	dlen = plcd_drv->pcfg->h;
	dlen = plcd_drv->pcfg->cfmt == CFMT_RGB565 ? dlen * 2 : dlen * 3;
	for (vline = 0; vline < plcd_drv->pcfg->v; vline++) {
		if (vline == 0)
			memcpy(cbuf, wr_line0_head, 4 * sizeof(unsigned char));
		else if (vline == 1)
			memcpy(cbuf, wr_linex_head, 4 * sizeof(unsigned char));

		dbuf = ((unsigned char *)addr +
			plcd_drv->pcfg->h * vline * (plcd_drv->pcfg->cfmt == CFMT_RGB565 ? 2 : 3));
		ret = plcd_spi_write_cmd1_data4(cbuf, 4, dbuf, dlen, 8);
	}
	kfree(cbuf);

	if (ret)
		LCDERR("%s\n", __func__);

	return ret;
}

static int spd2010_qspi_post_frame(unsigned char *addr, unsigned short x0, unsigned short x1,
			unsigned short y0, unsigned short y1)
{
	int ret;

	if (x0 || y0 || x1 != plcd_drv->pcfg->h || y1 != plcd_drv->pcfg->v) {
		LCDERR("Only support full frame post currently\n");
		return -1;
	}
	ret = spd2010_qspi_flush_frame(addr);

	return ret;
}

int plcd_spd2010_probe(void)
{
	int ret;

	if (!plcd_drv->pcfg->spi_cfg.spi_dev) {
		LCDERR("spi_dev is null\n");
		return -1;
	}

	plcd_drv->enable = spi_power_on_init_dft;
	plcd_drv->disable = spi_power_off_dft;

	plcd_drv->frame_post = spd2010_qspi_post_frame;
	plcd_drv->frame_flush = spd2010_qspi_flush_frame;
	plcd_drv->test = spi_test_dft;

	ret = plcd_drv->enable();

	if (!ret)
		spi_test_pure_color(3);
	return 0;
}

int plcd_spd2010_remove(void)
{
	return 0;
}
