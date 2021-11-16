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

int ldim_spi_write(struct spi_device *spi, unsigned char *tbuf, int tlen)
{
	struct ldim_dev_driver_s *dev_drv = dev_get_drvdata(&spi->dev);
	struct spi_transfer xfer;
	struct spi_message msg;
	int ret;

	if (!dev_drv) {
		LDIMERR("%s: dev_drv is null\n", __func__);
		return -1;
	}

	if (dev_drv->cs_hold_delay)
		udelay(dev_drv->cs_hold_delay);

	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = (void *)tbuf;
	xfer.rx_buf = NULL;
	xfer.len = tlen;
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(spi, &msg);
	if (ret)
		LDIMERR("%s\n", __func__);

	return ret;
}

int ldim_spi_read(struct spi_device *spi, unsigned char *tbuf, int tlen,
		  unsigned char *rbuf, int rlen)
{
	struct ldim_dev_driver_s *dev_drv = dev_get_drvdata(&spi->dev);
	struct spi_transfer xfer[2];
	struct spi_message msg;
	int ret;

	if (!dev_drv) {
		LDIMERR("%s: dev_drv is null\n", __func__);
		return -1;
	}

	if (dev_drv->cs_hold_delay)
		udelay(dev_drv->cs_hold_delay);

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
		LDIMERR("%s\n", __func__);

	return ret;
}

/* send and read buf at the same time */
int ldim_spi_read_sync(struct spi_device *spi, unsigned char *tbuf,
		       unsigned char *rbuf, int len)
{
	struct ldim_dev_driver_s *dev_drv = dev_get_drvdata(&spi->dev);
	struct spi_transfer xfer;
	struct spi_message msg;
	int ret;

	if (!dev_drv) {
		LDIMERR("%s: dev_drv is null\n", __func__);
		return -1;
	}

	if (dev_drv->cs_hold_delay)
		udelay(dev_drv->cs_hold_delay);

	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = (void *)tbuf;
	xfer.rx_buf = (void *)rbuf;
	xfer.len = len;
	spi_message_add_tail(&xfer, &msg);

	ret = spi_sync(spi, &msg);
	if (ret)
		LDIMERR("%s\n", __func__);

	return ret;
}

static int ldim_spi_dev_probe(struct spi_device *spi)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	int ret;

	if (ldim_debug_print)
		LDIMPR("%s\n", __func__);

	if (!ldim_drv->dev_drv)
		return 0;
	dev_drv = ldim_drv->dev_drv;
	dev_drv->spi_dev = spi;

	dev_set_drvdata(&spi->dev, dev_drv);
	spi->bits_per_word = 8;

	ret = spi_setup(spi);
	if (ret)
		LDIMERR("spi setup failed\n");

	return ret;
}

static int ldim_spi_dev_remove(struct spi_device *spi)
{
	struct ldim_dev_driver_s *dev_drv = dev_get_drvdata(&spi->dev);

	if (ldim_debug_print)
		LDIMPR("%s\n", __func__);

	if (dev_drv)
		dev_drv->spi_dev = NULL;
	dev_set_drvdata(&spi->dev, NULL);

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

int ldim_spi_driver_add(struct ldim_dev_driver_s *dev_drv)
{
	struct spi_controller *ctlr;
	struct spi_device *spi_device;
	int ret;

	if (!dev_drv->spi_info) {
		LDIMERR("%s: spi_info is null\n", __func__);
		return -1;
	}

	ctlr = spi_busnum_to_master(dev_drv->spi_info->bus_num);
	if (!ctlr) {
		LDIMERR("get busnum failed\n");
		return -1;
	}

	spi_device = spi_new_device(ctlr, dev_drv->spi_info);
	if (!spi_device) {
		LDIMERR("get spi_device failed\n");
		return -1;
	}

	ret = spi_register_driver(&ldim_spi_dev_driver);
	if (ret) {
		LDIMERR("%s failed\n", __func__);
		return -1;
	}
	if (!dev_drv->spi_dev) {
		LDIMERR("%s failed\n", __func__);
		return -1;
	}

	LDIMPR("%s ok\n", __func__);
	return 0;
}

int ldim_spi_driver_remove(struct ldim_dev_driver_s *dev_drv)
{
	if (dev_drv->spi_dev)
		spi_unregister_driver(&ldim_spi_dev_driver);

	LDIMPR("%s ok\n", __func__);

	return 0;
}

