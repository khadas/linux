// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/media/vout/peripheral_lcd.h>
#include "peripheral_lcd_drv.h"
#include "peripheral_lcd_dev.h"

static unsigned int cs_hold_delay;
static unsigned int cs_clk_delay;

int per_lcd_spi_write(struct spi_device *spi, unsigned char *tbuf, int tlen,
		      int word_bits)
{
	struct spi_transfer xfer;
	struct spi_message msg;
	int ret;

	if (cs_hold_delay)
		per_lcd_delay_us(cs_hold_delay);

	spi->bits_per_word = word_bits;
	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = (void *)tbuf;
	xfer.rx_buf = NULL;
	xfer.len = tlen;
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(spi, &msg);
	if (ret)
		LCDERR("%s\n", __func__);

	return ret;
}

int per_lcd_spi_read(struct spi_device *spi, unsigned char *tbuf,
		     int tlen, unsigned char *rbuf, int rlen)
{
	struct spi_transfer xfer[2];
	struct spi_message msg;
	int ret;

	if (cs_hold_delay)
		per_lcd_delay_us(cs_hold_delay);

	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer[0].tx_buf = (void *)tbuf;
	xfer[0].rx_buf = NULL;
	xfer[0].len = tlen;
	spi_message_add_tail(&xfer[0], &msg);
	xfer[1].tx_buf = NULL;
	xfer[1].rx_buf = (void *)rbuf;
	xfer[1].len = rlen;
	spi_message_add_tail(&xfer[1], &msg);
	ret = spi_sync(spi, &msg);
	if (ret)
		LCDERR("%s\n", __func__);

	return ret;
}

static int per_lcd_spi_dev_probe(struct spi_device *spi)
{
	struct peripheral_lcd_driver_s *per_lcd_drv = peripheral_lcd_get_driver();
	int ret;

	if (per_lcd_debug_flag)
		LCDPR("%s\n", __func__);

	per_lcd_drv->spi_dev = spi;
	dev_set_drvdata(&spi->dev, per_lcd_drv);
	spi->bits_per_word = 8;
	cs_hold_delay = per_lcd_drv->per_lcd_dev_conf->cs_hold_delay;
	cs_clk_delay = per_lcd_drv->per_lcd_dev_conf->cs_clk_delay;

	ret = spi_setup(spi);
	if (ret)
		LCDERR("spi setup failed\n");

	/* dummy for spi clktree init */
	per_lcd_spi_write(spi, NULL, 0, 8);

	return ret;
}

static int per_lcd_spi_dev_remove(struct spi_device *spi)
{
	struct peripheral_lcd_driver_s *per_lcd_drv = peripheral_lcd_get_driver();

	if (per_lcd_debug_flag)
		LCDPR("%s\n", __func__);

	per_lcd_drv->spi_dev = NULL;
	dev_set_drvdata(&spi->dev, NULL);
	return 0;
}

static struct spi_driver per_lcd_spi_dev_driver = {
	.probe = per_lcd_spi_dev_probe,
	.remove = per_lcd_spi_dev_remove,
	.driver = {
		.name = "peripheral_lcd",
		.owner = THIS_MODULE,
	},
};

int per_lcd_spi_driver_add(struct peripheral_lcd_driver_s *per_lcd_drv)
{
	struct spi_controller *ctlr;
	struct spi_device *spi_device;
	int ret;

	if (!per_lcd_drv->spi_info) {
		LCDERR("%s: spi_info is null\n", __func__);
		return -1;
	}

	ctlr = spi_busnum_to_master(per_lcd_drv->spi_info->bus_num);
	if (!ctlr) {
		LCDERR("get busnum failed\n");
		return -1;
	}

	spi_device = spi_new_device(ctlr, per_lcd_drv->spi_info);
	if (!spi_device) {
		LCDERR("get spi_device failed\n");
		return -1;
	}

	ret = spi_register_driver(&per_lcd_spi_dev_driver);
	if (ret) {
		LCDERR("%s failed\n", __func__);
		return -1;
	}
	if (!per_lcd_drv->spi_dev) {
		LCDERR("%s failed\n", __func__);
		return -1;
	}

	LCDPR("%s ok\n", __func__);
	return 0;
}

int per_lcd_spi_driver_remove(struct peripheral_lcd_driver_s *per_lcd_drv)
{
	if (per_lcd_drv->spi_dev)
		spi_unregister_driver(&per_lcd_spi_dev_driver);

	LCDPR("%s ok\n", __func__);

	return 0;
}

