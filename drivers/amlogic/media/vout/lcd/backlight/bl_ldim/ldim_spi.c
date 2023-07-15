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
#include <linux/amlogic/aml_spi.h>
#include "ldim_drv.h"
#include "ldim_dev_drv.h"

static int ldim_spi_async_busy;
static int ldim_spi_async_busy_cnt;

int ldim_spi_dma_cycle_align_byte(int size)
{
	int new_size, word_cnt, n, i;

	/* spi dma word must be multiple of 8byte(64bit)  */
	word_cnt = (size + 7) / 8;

	/* spi dma cycle must be multiple of word and (2~8) */
	if (word_cnt > 15) {
		for (i = 8; i > 1; i--) {
			n = word_cnt % i;
			if (n == 0)
				break;
		}
		word_cnt += n;
	}

	new_size = word_cnt * 8;
	return new_size;
}

static int ldim_spi_dump_buffer_array(unsigned char *buf, int buf_len)
{
	int i;
	int strlen;
	unsigned char *strbuf;

	LDIMPR("[%s] buf_len = %d\n", __func__, buf_len);
	strlen = 0;
	strbuf = kcalloc(100, sizeof(unsigned char), GFP_KERNEL);
	if (!strbuf)
		return -1;

	for (i = 0; i < buf_len; i++) {
		strlen += sprintf(strbuf + strlen, "0x%02x ", buf[i]);
		if (((i % 8) == 7) || (i == (buf_len - 1))) {
			pr_info("%s\n", strbuf);
			strlen = 0;
			}
	}
	pr_info("\n");
	kfree(strbuf);
	return 0;
}

static u64 _bswap64(u64 a)
{
	a = ((a & 0x00000000000000FFULL) << 56) |
	    ((a & 0x000000000000FF00ULL) << 40) |
	    ((a & 0x0000000000FF0000ULL) << 24) |
	    ((a & 0x00000000FF000000ULL) <<  8) |
	    ((a & 0x000000FF00000000ULL) >>  8) |
	    ((a & 0x0000FF0000000000ULL) >> 24) |
	    ((a & 0x00FF000000000000ULL) >> 40) |
	    ((a & 0xFF00000000000000ULL) >> 56);
	return a;
}

static int ldim_spi_buf_byte_swap_64bit(unsigned char *buf, int tlen, int xlen)
{
	u64 *tmp = (u64 *)buf;
	u64 a = 0;
	int n, i;

	if (!buf)
		return -1;

	if (ldim_debug_print == 20) {
		LDIMPR("dump Original buf len = %d\n", tlen);
		ldim_spi_dump_buffer_array(buf, tlen);
	}

	for (i = tlen; i < xlen; i++)
		buf[i] = 0;
	n = xlen / 8;
	for (i = 0; i < n; i++) {
		a = tmp[i];
		tmp[i] = _bswap64(a);
	}

	if (ldim_debug_print == 20) {
		LDIMPR("dump buf_swap len = %d\n", xlen);
		ldim_spi_dump_buffer_array(buf, xlen);
	}

	return 0;
}

static void ldim_spi_async_callback(void *arg)
{
	int *state = (int *)arg;

	*state = 0;
}

int ldim_spi_write_async(struct spi_device *spi, unsigned char *tbuf,
			 unsigned char *rbuf, int tlen, int dma_mode, int max_len)
{
	struct spicc_controller_data *cdata = spi->controller_data;
	int xlen, ret;

	if (!cdata || !cdata->dirspi_async)
		return -EIO;

	xlen = tlen;
	if (dma_mode) {
		xlen = ldim_spi_dma_cycle_align_byte(tlen);
		if (xlen > max_len) {
			LDIMERR("%s: dma xlen %d out of max_len %d\n",
				__func__, xlen, max_len);
			return -1;
		}
		ldim_spi_buf_byte_swap_64bit(tbuf, tlen, xlen);
		spi->bits_per_word = 64;
	} else {
		spi->bits_per_word = 8;
	}

	if (ldim_spi_async_busy_cnt > 5) {
		ldim_spi_async_busy = 0;
		LDIMPR("%s: ldim spi timeout!! ldim_spi_async_busy_cnt = %d\n",
			__func__, ldim_spi_async_busy_cnt);
	}

	if (ldim_spi_async_busy) {
		ldim_spi_async_busy_cnt++;
		if (ldim_debug_print)
			LDIMERR("%s: spi_async_busy=%d\n", __func__, ldim_spi_async_busy);
		return -1;
	}

	ldim_spi_async_busy_cnt = 0;
	ldim_spi_async_busy = 1;
	ret = cdata->dirspi_async(spi, tbuf, rbuf, xlen,
		ldim_spi_async_callback, (void *)&ldim_spi_async_busy);
	if (ret)
		LDIMERR("%s\n", __func__);

	return ret;
}

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
	if (ldim_spi_async_busy) {
		if (ldim_debug_print)
			LDIMERR("%s: spi_async_busy=%d\n", __func__, ldim_spi_async_busy);
		return -1;
	}
	ldim_spi_async_busy = 1;

	if (dev_drv->cs_hold_delay)
		udelay(dev_drv->cs_hold_delay);

	spi->bits_per_word = 8;
	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = (void *)tbuf;
	xfer.rx_buf = NULL;
	xfer.len = tlen;
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(spi, &msg);
	if (ret)
		LDIMERR("%s\n", __func__);
	ldim_spi_async_busy = 0;

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
	if (ldim_spi_async_busy) {
		if (ldim_debug_print)
			LDIMERR("%s: spi_async_busy=%d\n", __func__, ldim_spi_async_busy);
		return -1;
	}
	ldim_spi_async_busy = 1;

	if (dev_drv->cs_hold_delay)
		udelay(dev_drv->cs_hold_delay);

	spi->bits_per_word = 8;
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
	ldim_spi_async_busy = 0;

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
	if (ldim_spi_async_busy) {
		if (ldim_debug_print)
			LDIMERR("%s: spi_async_busy=%d\n", __func__, ldim_spi_async_busy);
		return -1;
	}
	ldim_spi_async_busy = 1;

	if (dev_drv->cs_hold_delay)
		udelay(dev_drv->cs_hold_delay);

	spi->bits_per_word = 8;
	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = (void *)tbuf;
	xfer.rx_buf = (void *)rbuf;
	xfer.len = len;
	spi_message_add_tail(&xfer, &msg);

	ret = spi_sync(spi, &msg);
	if (ret)
		LDIMERR("%s\n", __func__);
	ldim_spi_async_busy = 0;

	return ret;
}

void ldim_spi_async_busy_clear(void)
{
	ldim_spi_async_busy = 0;
	ldim_spi_async_busy_cnt = 0;
}

static int ldim_spi_dev_probe(struct spi_device *spi)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	int ret = 0;

	if (ldim_debug_print)
		LDIMPR("%s\n", __func__);

	if (!ldim_drv->dev_drv)
		return 0;

	ldim_spi_async_busy = 0;
	ldim_spi_async_busy_cnt = 0;

	dev_drv = ldim_drv->dev_drv;
	dev_drv->spi_dev = spi;

	if (dev_drv->spi_sync) {
		dev_set_drvdata(&spi->dev, dev_drv);
		spi->bits_per_word = 8;

		ret = spi_setup(spi);
		if (ret)
			LDIMERR("spi setup failed\n");

		/* dummy for spi clktree init */
		ldim_spi_write(spi, NULL, 0);
	}
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

