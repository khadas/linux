// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/dma-mapping.h>
#include <linux/spi/spi.h>
#include <linux/amlogic/aml_spi.h>
#include "spicc_test.h"

static LIST_HEAD(testdev_list);

int spicc_make_argv(char *s, int argvsz, char *argv[], char *delim)
{
	char *tok;
	int i;

	/* split into argv */
	for (i = 0; i < argvsz; i++) {
		tok = strsep(&s, delim);
		if (!tok)
			break;

		if  (*tok == '\0')
			tok = strsep(&s, delim);

		tok = strim(tok);
		argv[i] = tok;
	}

	return i;
}

int spicc_getopt(int argc, char *argv[], char *name,
		 unsigned long *value, char **str, unsigned int base)
{
	unsigned long v;
	char *s;
	int i, ret;

	for (i = 0; i < argc; i++) {
		s = argv[i] + strlen(name);
		ret = memcmp(name, argv[i], strlen(name));
		if (!ret && ((*s == ' ') || (*s == '\0'))) {
			if (value) {
				ret = kstrtoul(s + 1, base, &v);
				if (!ret)
					*value = v;
			}
			if (str)
				*str = s + 1;
			return ret;
		}
	}

	return -EINVAL;
}

void spicc_strtohex(char *str, int pass, u8 *buf, int len)
{
	char *token;
	unsigned long v;
	int i;

	/* pass over */
	for (i = 0; i < pass; i++)
		strsep(&str, ", ");

	/* filled buffer with str data */
	for (i = 0; i < len; i++) {
		token = strsep(&str, ", ");
		if (token == 0 || kstrtoul(token, 16, &v))
			break;
		buf[i] = (u8)(v & 0xff);
	}

	/* set first tx data default 1 if no any str data */
	if (i == 0) {
		buf[0] = 0x1;
		i++;
	}

	/* fill next buffer incrementally */
	for (; i < len; i++)
		buf[i] = buf[i - 1] + 1;
}

int spicc_compare(u8 *buf1, u8 *buf2, int len, bool pr_diff)
{
	int i, diff = 0;

	for (i = 0; i < len; i++) {
		if (buf1[i] != buf2[i]) {
			diff++;
			if (pr_diff)
				pr_info("[%d]: 0x%x, 0x%x\n",
					i, buf1[i], buf2[i]);
		}
	}
	pr_info("total %d, diff %d\n", len,  diff);

	return diff;
}

static void testdev_print_xfer(struct test_device *testdev)
{
	struct spi_transfer *xfer;

	list_for_each_entry(xfer, &testdev->msg.transfers, transfer_list) {
		if (xfer && xfer->rx_buf) {
			if (testdev->hexdump)
				spicc_hexdump(xfer->rx_buf, xfer->len);
			if (testdev->compare && xfer->tx_buf)
				spicc_compare((u8 *)xfer->tx_buf,
					      xfer->rx_buf,
					      xfer->len,
					      testdev->pr_diff);
		}
	}
}

static void testdev_clean_rxbuf(struct test_device *testdev)
{
	struct spi_transfer *xfer;

	list_for_each_entry(xfer, &testdev->msg.transfers, transfer_list) {
		if (xfer && xfer->rx_buf)
			memset(xfer->rx_buf, 0, xfer->len);
	}
}

static struct spi_transfer *
testdev_get_current_xfer(struct test_device *testdev)
{
	struct spi_transfer *xfer = NULL;

	list_for_each_entry(xfer, &testdev->msg.transfers, transfer_list) {
		if (list_is_last(&xfer->transfer_list, &testdev->msg.transfers))
			break;
	}

	return xfer;
}

ssize_t testdev_dump(struct test_device *testdev, char *buf)
{
	struct spi_device *spi = testdev->spi;
	struct spicc_controller_data *cdata;
	ssize_t len;

	cdata = (struct spicc_controller_data *)spi->controller_data;
	len = snprintf(buf, PAGE_SIZE, "hexdump: %d\n", testdev->hexdump);
	len += snprintf(buf + len, PAGE_SIZE, "compare: %d\n", testdev->compare);
	len += snprintf(buf + len, PAGE_SIZE, "pr_diff: %d\n", testdev->pr_diff);
	len += snprintf(buf + len, PAGE_SIZE, "cs_gpio: %d\n", spi->cs_gpio);
	len += snprintf(buf + len, PAGE_SIZE, "cs: %d\n", spi->chip_select);
	len += snprintf(buf + len, PAGE_SIZE, "speed: %d\n", spi->max_speed_hz);
	len += snprintf(buf + len, PAGE_SIZE, "mode: 0x%x\n", spi->mode);
	len += snprintf(buf + len, PAGE_SIZE, "bw: %d\n", spi->bits_per_word);
	len += snprintf(buf + len, PAGE_SIZE, "ccxfer_en: %d\n", cdata->ccxfer_en);
	len += snprintf(buf + len, PAGE_SIZE, "timing_en: %d\n", cdata->timing_en);
	len += snprintf(buf + len, PAGE_SIZE, "ss_leading_gap: %d\n", cdata->ss_leading_gap);
	len += snprintf(buf + len, PAGE_SIZE, "ss_trailing_gap: %d\n", cdata->ss_trailing_gap);
	len += snprintf(buf + len, PAGE_SIZE, "tx_tuning: %d\n", cdata->tx_tuning);
	len += snprintf(buf + len, PAGE_SIZE, "rx_tuning: %d\n", cdata->rx_tuning);
	len += snprintf(buf + len, PAGE_SIZE, "dummy: %d\n", cdata->dummy_ctl);
	len += snprintf(buf + len, PAGE_SIZE, "nxfers: %d\n", testdev->nxfers);

	return len;
}

struct test_device *testdev_get(struct device *dev)
{
	struct test_device *testdev;

	list_for_each_entry(testdev, &testdev_list, list) {
		if (!strcmp(dev_name(dev),
			dev_name(&testdev->spi->controller->dev)))
			return testdev;
	}
	return NULL;
}

struct test_device *testdev_new(struct device *dev, int argc, char *argv[])
{
	struct test_device *testdev;
	struct spi_controller *ctlr;
	struct spi_device *spi;

	testdev = testdev_get(dev);
	if (testdev) {
		dev_warn(dev, "testdev exist already\n");
		return testdev;
	}

	ctlr = container_of(dev, struct spi_controller, dev);
	testdev = kzalloc(sizeof(*testdev), GFP_KERNEL);
	spi = spi_alloc_device(ctlr);
	if (IS_ERR_OR_NULL(spi)) {
		kfree(testdev);
		dev_err(dev, "spi device alloc failed\n");
		return NULL;
	}

	testdev->spi = spi;
	spi->controller_data = (void *)&testdev->cdata;
	spi->controller_state = NULL;
	memset(spi->controller_data, 0, sizeof(struct spicc_controller_data));
	spi->chip_select = 0;
	spi->cs_gpio = -ENOENT;
	spi->max_speed_hz = 10000000;
	spi->bits_per_word = 8;
	spi->mode = 0;

	if (testdev_setup(testdev, argc, argv)) {
		spi_dev_put(spi);
		kfree(testdev);
		testdev = NULL;
		dev_err(dev, "testdev new failed\n");
	} else {
		spi_add_device(spi);
		spi_message_init(&testdev->msg);
		list_add_tail(&testdev->list, &testdev_list);
		dev_info(dev, "testdev new success\n");
	}

	return testdev;
}

int testdev_free(struct test_device *testdev)
{
	if (device_is_registered(&testdev->spi->dev))
		spi_unregister_device(testdev->spi);
	else
		spi_dev_put(testdev->spi);
	testdev_free_xfer(testdev);
	list_del(&testdev->list);
	dev_info(&testdev->spi->controller->dev, "testdev free\n");
	kfree(testdev);

	return 0;
}

int testdev_setup(struct test_device *testdev, int argc, char *argv[])
{
	struct spi_device *spi;
	struct spicc_controller_data *cdata;
	unsigned long v;
	int ret;

	spi = testdev->spi;
	if (!spicc_getopt(argc, argv, "hexdump", &v, NULL, 10))
		testdev->hexdump = !!v;
	if (!spicc_getopt(argc, argv, "compare", &v, NULL, 10))
		testdev->compare = !!v;
	if (!spicc_getopt(argc, argv, "pr_diff", &v, NULL, 10))
		testdev->pr_diff = !!v;
	if (!spicc_getopt(argc, argv, "cs_gpio", &v, NULL, 10))
		spi->cs_gpio = v;
	if (!spicc_getopt(argc, argv, "cs", &v, NULL, 10))
		spi->chip_select = v;
	if (!spicc_getopt(argc, argv, "speed", &v, NULL, 10))
		spi->max_speed_hz = v;
	if (!spicc_getopt(argc, argv, "mode", &v, NULL, 16))
		spi->mode = v;
	if (!spicc_getopt(argc, argv, "bw", &v, NULL, 10))
		spi->bits_per_word = v;

	cdata = (struct spicc_controller_data *)spi->controller_data;
	if (!spicc_getopt(argc, argv, "ccxfer_en", &v, NULL, 10))
		cdata->ccxfer_en = v;
	if (!spicc_getopt(argc, argv, "timing_en", &v, NULL, 10))
		cdata->timing_en = v;
	if (!spicc_getopt(argc, argv, "ss_leading_gap", &v, NULL, 10))
		cdata->ss_leading_gap = v;
	if (!spicc_getopt(argc, argv, "ss_trailing_gap", &v, NULL, 10))
		cdata->ss_trailing_gap = v;
	if (!spicc_getopt(argc, argv, "tx_tuning", &v, NULL, 10))
		cdata->tx_tuning = v;
	if (!spicc_getopt(argc, argv, "rx_tuning", &v, NULL, 10))
		cdata->rx_tuning = v;
	if (!spicc_getopt(argc, argv, "dummy", &v, NULL, 10))
		cdata->dummy_ctl = v;

	ret = spi_setup(spi);
	dev_info(&spi->controller->dev,
		 "spi device setup %s\n", ret ? "failed" : "success");

	return ret;
}

int testdev_new_xfer(struct test_device *testdev, int argc, char *argv[])
{
	struct device *dev;
	struct spicc_transfer *ccxfer;
	struct spi_transfer *xfer;
	char *data_str;
	unsigned long v;
	bool vm = false, coherent = false;

	dev = testdev->spi->controller->dev.parent;
	if (testdev->cdata.ccxfer_en) {
		ccxfer = kzalloc(sizeof(*ccxfer), GFP_KERNEL);
		xfer = &ccxfer->xfer;
	} else {
		xfer = kzalloc(sizeof(*xfer), GFP_KERNEL);
	}
	if (!spicc_getopt(argc, argv, "vm", NULL, NULL, 0))
		vm = true;
	if (!spicc_getopt(argc, argv, "coherent", NULL, NULL, 0))
		coherent = true;
	if (!testdev->cdata.ccxfer_en)
		goto xfer_opt;
	if (!spicc_getopt(argc, argv, "dc_level", &v, NULL, 10))
		ccxfer->dc_level = v;
	if (!spicc_getopt(argc, argv, "read_turn_around", &v, NULL, 10))
		ccxfer->read_turn_around = v;
	if (!spicc_getopt(argc, argv, "dc_mode", &v, NULL, 10))
		ccxfer->dc_mode = v;
xfer_opt:
	if (!spicc_getopt(argc, argv, "speed", &v, NULL, 10))
		xfer->speed_hz = v;
	if (!spicc_getopt(argc, argv, "bw", &v, NULL, 10))
		xfer->bits_per_word = v;
	if (!spicc_getopt(argc, argv, "txnbits", &v, NULL, 10))
		xfer->tx_nbits = v;
	if (!spicc_getopt(argc, argv, "rxnbits", &v, NULL, 10))
		xfer->rx_nbits = v;
	if (!spicc_getopt(argc, argv, "len", &v, NULL, 10))
		xfer->len = v;
	if (!xfer->len) {
		dev_err(dev, "data length invalid\n");
		return -EINVAL;
	}

	if (spicc_getopt(argc, argv, "notx", NULL, NULL, 0)) {
		if (vm) {
			xfer->tx_buf = vmalloc(xfer->len);
		} else if (coherent) {
			xfer->tx_buf = dma_alloc_coherent(dev, xfer->len,
					&xfer->tx_dma, GFP_KERNEL | GFP_DMA);
			testdev->msg.is_dma_mapped = true;
		} else {
			xfer->tx_buf = kzalloc(xfer->len, GFP_KERNEL | GFP_DMA);
		}
		if (IS_ERR_OR_NULL(xfer->tx_buf)) {
			dev_err(dev, "alloc tx buf failed\n");
			return -ENOMEM;
		}

		spicc_getopt(argc, argv, "data", NULL, &data_str, 0);
		spicc_strtohex(data_str, 0, (u8 *)xfer->tx_buf, xfer->len);
	}

	if (spicc_getopt(argc, argv, "norx", NULL, NULL, 0)) {
		if (vm) {
			xfer->rx_buf = vmalloc(xfer->len);
		} else if (coherent) {
			xfer->rx_buf = dma_alloc_coherent(dev, xfer->len,
					&xfer->rx_dma, GFP_KERNEL | GFP_DMA);
			testdev->msg.is_dma_mapped = true;
		} else {
			xfer->rx_buf = kzalloc(xfer->len, GFP_KERNEL | GFP_DMA);
		}
		if (IS_ERR_OR_NULL(xfer->rx_buf)) {
			dev_err(dev, "alloc rx buf failed\n");
			kfree(xfer->tx_buf);
			return -ENOMEM;
		}
	}

	if (!xfer->tx_buf && !xfer->rx_buf) {
		dev_err(dev, "either tx or rx must be exist\n");
		return -EINVAL;
	}

	spi_message_add_tail(xfer, &testdev->msg);
	testdev->nxfers++;
	dev_info(dev, "add a xfer success(%s)\n",
		 vm ? "vm" : (coherent ? "coherent" : "kalloc"));

	return 0;
}

void testdev_free_xfer(struct test_device *testdev)
{
	struct device *dev = testdev->spi->controller->dev.parent;
	struct spi_transfer *xfer;

	list_for_each_entry(xfer, &testdev->msg.transfers, transfer_list) {
		if (xfer->tx_buf) {
			if (is_vmalloc_addr(xfer->tx_buf))
				vfree(xfer->tx_buf);
			else if (testdev->msg.is_dma_mapped)
				dma_free_coherent(dev,
						xfer->len,
						(void *)xfer->tx_buf,
						xfer->tx_dma);
			else
				kfree(xfer->tx_buf);
		}

		if (xfer->rx_buf) {
			if (is_vmalloc_addr(xfer->rx_buf))
				vfree(xfer->rx_buf);
			else if (testdev->msg.is_dma_mapped)
				dma_free_coherent(dev,
						xfer->len,
						xfer->rx_buf,
						xfer->rx_dma);
			else
				kfree(xfer->rx_buf);
		}

		if (testdev->cdata.ccxfer_en)
			xfer = (struct spi_transfer *)container_of(xfer,
					struct spicc_transfer, xfer);
		kfree(xfer);
	}

	spi_message_init(&testdev->msg);
	testdev->nxfers = 0;
	dev_info(dev, "all xfers free\n");
}

int testdev_run(struct test_device *testdev, int argc, char *argv[])
{
	struct device *dev;
	struct spi_device *spi;
	struct spicc_controller_data *cdata;
	struct spi_transfer *xfer;
	unsigned long v;
	int ret = -EIO;

	spi = testdev->spi;
	dev = &testdev->spi->controller->dev;

	testdev_clean_rxbuf(testdev);
	cdata = (struct spicc_controller_data *)spi->controller_data;
	if (!spicc_getopt(argc, argv, "spi_sync", NULL, NULL, 0)) {
		ret = spi_sync(spi, &testdev->msg);
		if (!ret) {
			dev_info(dev, "spi_sync test success\n");
			testdev_print_xfer(testdev);
		} else {
			dev_info(dev, "spi_sync test failed\n");
		}
	}

	else if (cdata->dirspi_sync &&
		!spicc_getopt(argc, argv, "dirspi_sync", NULL, NULL, 0)) {
		xfer = testdev_get_current_xfer(testdev);
		ret = cdata->dirspi_sync(testdev->spi,
					 (u8 *)xfer->tx_buf,
					 xfer->rx_buf,
					 xfer->len);
		if (!ret) {
			dev_info(dev, "dirspi_sync test success\n");
			testdev_print_xfer(testdev);
		} else {
			dev_info(dev, "dirspi_sync test failed\n");
		}
	}

	else if (cdata->dirspi_stop &&
		!spicc_getopt(argc, argv, "dirspi_stop", NULL, NULL, 0)) {
		cdata->dirspi_stop(spi);
		ret = 0;
		dev_info(dev, "dirspi stop!\n");
	}

	else if (cdata->dirspi_dma_trig &&
		!spicc_getopt(argc, argv, "trig", &v, NULL, 0)) {
		xfer = testdev_get_current_xfer(testdev);
		ret = cdata->dirspi_dma_trig(spi,
					xfer->tx_dma,
					xfer->rx_dma,
					xfer->len,
					v ? DMA_TRIG_LINE_N : DMA_TRIG_VSYNC);
		dev_info(dev, "set trig mode %s\n", v ? "line_n" : "vsync");
	}

	else if (cdata->dirspi_dma_trig_start &&
		!spicc_getopt(argc, argv, "trig_start", NULL, NULL, 0)) {
		ret = cdata->dirspi_dma_trig_start(spi);
		dev_info(dev, "trig start!\n");
	}

	else if (cdata->dirspi_dma_trig_stop &&
		!spicc_getopt(argc, argv, "trig_stop", NULL, NULL, 0)) {
		ret = cdata->dirspi_dma_trig_stop(spi);
		dev_info(dev, "trig stop!\n");
	}

	if (ret == -EIO)
		dev_err(dev, "entry unsupport\n");

	return ret;
}

#define TEST_PARAM_NUM 5
int test(struct device *dev, const char *buf)
{
	unsigned int cs_gpio, speed, mode, bits_per_word, num;
	u8 *tx_buf, *rx_buf;
	char *kstr;
	int ret;
	struct spi_transfer t;
	struct spi_message m;
	struct spi_controller *ctlr;

	if (sscanf(buf, "%d%d%x%d%d", &cs_gpio, &speed,
		   &mode, &bits_per_word, &num) != TEST_PARAM_NUM) {
		dev_err(dev, "error format\n");
		return -EIO;
	}

	kstr = kstrdup(buf, GFP_KERNEL);
	tx_buf = kzalloc(num, GFP_KERNEL | GFP_DMA);
	rx_buf = kzalloc(num, GFP_KERNEL | GFP_DMA);
	if (IS_ERR(kstr) || IS_ERR(tx_buf) || IS_ERR(rx_buf)) {
		dev_err(dev, "failed to alloc tx rx buffer\n");
		ret = -ENOMEM;
		goto test_end2;
	}

	/* pass over "cs_gpio speed mode bits_per_word num" */
	spicc_strtohex(kstr, TEST_PARAM_NUM, tx_buf, num);

	spi_message_init(&m);
	ctlr = container_of(dev, struct spi_controller, dev);
	m.spi = spi_alloc_device(ctlr);
	if (IS_ERR_OR_NULL(m.spi)) {
		dev_err(dev, "spi alloc failed\n");
		ret = -ENODEV;
		goto test_end;
	}

	m.spi->cs_gpio = (cs_gpio > 0) ? cs_gpio : -ENOENT;
	m.spi->max_speed_hz = speed;
	m.spi->mode = mode & 0xffff;
	m.spi->bits_per_word = bits_per_word;
	if (spi_setup(m.spi)) {
		dev_err(dev, "setup failed\n");
		ret = -EIO;
		goto test_end;
	}

	memset(&t, 0, sizeof(t));
	t.tx_buf = (void *)tx_buf;
	t.rx_buf = (void *)rx_buf;
	t.len = num;
	spi_message_add_tail(&t, &m);
	ret = spi_sync(m.spi, &m);
	if (!ret)
		spicc_compare(tx_buf, rx_buf, num, !!(mode & BIT(16)));
	dev_info(dev, "%s ret %d\n", __func__, ret);

test_end:
	//spi_cleanup(m.spi);
	spi_dev_put(m.spi);
test_end2:
	kfree(kstr);
	kfree(tx_buf);
	kfree(rx_buf);
	return ret;
}
