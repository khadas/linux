// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
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
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include "ldim_drv.h"
#include "ldim_dev_drv.h"

static unsigned int cs_hold_delay;
static unsigned int cs_clk_delay;

int ldim_spi_write(struct spi_device *spi, unsigned char *tbuf, int tlen)
{
	struct spi_transfer xfer;
	struct spi_message msg;
	int ret;

	if (cs_hold_delay)
		udelay(cs_hold_delay);

	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = (void *)tbuf;
	xfer.rx_buf = NULL;
	xfer.len = tlen;
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(spi, &msg);

	return ret;
}

int ldim_spi_read(struct spi_device *spi, unsigned char *tbuf, int tlen,
		  unsigned char *rbuf, int rlen)
{
	struct spi_transfer xfer[2];
	struct spi_message msg;
	int ret;

	if (cs_hold_delay)
		udelay(cs_hold_delay);

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

	return ret;
}

/* send and read buf at the same time, read data is start in rbuf[8]*/
int ldim_spi_read_sync(struct spi_device *spi, unsigned char *tbuf,
		       unsigned char *rbuf, int len)
{
	struct spi_transfer xfer;
	struct spi_message msg;
	int ret;

	if (cs_hold_delay)
		udelay(cs_hold_delay);

	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = (void *)tbuf;
	xfer.rx_buf = (void *)rbuf;
	xfer.len = len;
	spi_message_add_tail(&xfer, &msg);

	ret = spi_sync(spi, &msg);

	return ret;
}

static int ldim_spi_dev_probe(struct spi_device *spi)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret;

	if (ldim_debug_print)
		LDIMPR("%s\n", __func__);

	ldim_drv->ldev_conf->spi_dev = spi;

	dev_set_drvdata(&spi->dev, ldim_drv->ldev_conf);
	spi->bits_per_word = 8;
	cs_hold_delay = ldim_drv->ldev_conf->cs_hold_delay;
	cs_clk_delay = ldim_drv->ldev_conf->cs_clk_delay;

	ret = spi_setup(spi);
	if (ret)
		LDIMERR("spi setup failed\n");

	return ret;
}

static int ldim_spi_dev_remove(struct spi_device *spi)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	if (ldim_debug_print)
		LDIMPR("%s\n", __func__);

	ldim_drv->ldev_conf->spi_dev = NULL;
	return 0;
}

static struct spi_driver ldim_spi_dev_driver = {
	.probe = ldim_spi_dev_probe,
	.remove = ldim_spi_dev_remove,
	.driver = {
		.name = "ldim_dev",
		.owner = THIS_MODULE,
	},
};

int ldim_spi_driver_add(struct ldim_dev_config_s *ldev_conf)
{
	int ret;

	if (!ldev_conf->spi_info) {
		LDIMERR("%s: spi_info is null\n", __func__);
		return -1;
	}

	spi_register_board_info(ldev_conf->spi_info, 1);
	ret = spi_register_driver(&ldim_spi_dev_driver);
	if (ret) {
		LDIMERR("%s failed\n", __func__);
		return -1;
	}
	if (!ldev_conf->spi_dev) {
		LDIMERR("%s failed\n", __func__);
		return -1;
	}

	LDIMPR("%s ok\n", __func__);
	return 0;
}

int ldim_spi_driver_remove(struct ldim_dev_config_s *ldev_conf)
{
	if (ldev_conf->spi_dev)
		spi_unregister_driver(&ldim_spi_dev_driver);

	LDIMPR("%s ok\n", __func__);

	return 0;
}

