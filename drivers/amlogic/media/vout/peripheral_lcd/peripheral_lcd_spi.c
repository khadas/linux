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
#include <linux/delay.h>
#include <linux/amlogic/media/vout/peripheral_lcd.h>
#include <linux/interrupt.h>
#include "peripheral_lcd_drv.h"

inline void spi_dcx_init(void)
{
	plcd_gpio_set(plcd_drv->pcfg->spi_cfg.dcx_index, 2);
}

inline void spi_dcx_high(void)
{
	plcd_gpio_set(plcd_drv->pcfg->spi_cfg.dcx_index, 1);
}

inline void spi_dcx_low(void)
{
	plcd_gpio_set(plcd_drv->pcfg->spi_cfg.dcx_index, 0);
}

inline void spi_rst_high(void)
{
	plcd_gpio_set(plcd_drv->pcfg->spi_cfg.reset_index, 1);
}

inline void spi_rst_low(void)
{
	plcd_gpio_set(plcd_drv->pcfg->spi_cfg.reset_index, 0);
}

int plcd_spi_write_dft(void *tbuf, int tlen, int word_bits)
{
	struct spi_transfer xfer;
	struct spi_message msg;
	struct spi_config_s *spi_cfg = &plcd_drv->pcfg->spi_cfg;
	int ret;

	plcd_usleep(spi_cfg->cs_hold_delay);

	spi_cfg->spi_dev->bits_per_word = word_bits;
	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = (void *)tbuf;
	xfer.rx_buf = NULL;
	xfer.tx_nbits = 1;
	xfer.rx_nbits = 1;
	xfer.len = tlen;
	spi_message_add_tail(&xfer, &msg);
	ret = spi_sync(spi_cfg->spi_dev, &msg);
	if (ret)
		LCDERR("%s\n", __func__);

	return ret;
}

int plcd_spi_write_cmd1_data4(void *cbuf, int clen, void *dbuf, int dlen, int word_bits)
{
	struct spi_transfer xfer_head;
	struct spi_transfer xfer_data;
	struct spi_message msg;
	struct spi_config_s *spi_cfg = &plcd_drv->pcfg->spi_cfg;
	int ret;

	plcd_usleep(spi_cfg->cs_hold_delay);

	spi_cfg->spi_dev->bits_per_word = word_bits;

	memset(&msg, 0, sizeof(msg));
	memset(&xfer_head, 0, sizeof(xfer_head));
	memset(&xfer_data, 0, sizeof(xfer_head));
	spi_message_init(&msg);

	xfer_head.tx_buf = (void *)cbuf;
	xfer_head.rx_buf = NULL;
	xfer_head.tx_nbits = 1;
	xfer_head.rx_nbits = 1;
	xfer_head.len = clen;
	spi_message_add_tail(&xfer_head, &msg);
	xfer_data.tx_buf =  (void *)dbuf;
	xfer_data.rx_buf = NULL;
	xfer_data.tx_nbits = 4;
	xfer_data.rx_nbits = 4;
	xfer_data.len = dlen;
	spi_message_add_tail(&xfer_data, &msg);
	ret = spi_sync(spi_cfg->spi_dev, &msg);

	if (ret)
		LCDERR("%s\n", __func__);

	return ret;
}

int plcd_spi_read(void *tbuf, int tlen, void *rbuf, int rlen)
{
	struct spi_transfer xfer[2];
	struct spi_config_s *spi_cfg = &plcd_drv->pcfg->spi_cfg;
	struct spi_message msg;
	int ret;

	usleep_range(spi_cfg->cs_hold_delay, spi_cfg->cs_hold_delay + 5);

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
	ret = spi_sync(spi_cfg->spi_dev, &msg);
	if (ret)
		LCDERR("%s\n", __func__);

	return ret;
}

static int plcd_spi_dev_probe(struct spi_device *spi)
{
	int ret;

	if (per_lcd_debug_flag)
		LCDPR("%s\n", __func__);

	plcd_drv->pcfg->spi_cfg.spi_dev = spi;
	dev_set_drvdata(&spi->dev, plcd_drv);
	spi->bits_per_word = 8;

	ret = spi_setup(spi);
	if (ret)
		LCDERR("spi setup failed\n");

	return ret;
}

static int plcd_spi_dev_remove(struct spi_device *spi)
{
	// struct per_lcd_driver_s *plcd_drv = peripheral_lcd_get_driver();

	if (per_lcd_debug_flag)
		LCDPR("%s\n", __func__);

	plcd_drv->pcfg->spi_cfg.spi_dev = NULL;
	dev_set_drvdata(&spi->dev, NULL);
	return 0;
}

static struct spi_driver plcd_spi_dev_driver = {
	.probe = plcd_spi_dev_probe,
	.remove = plcd_spi_dev_remove,
	.driver = {
		.name = "peripheral_lcd",
		.owner = THIS_MODULE,
	},
};

int plcd_spi_driver_add(void)
{
	struct spi_controller *ctlr;
	struct spi_device *spi_device;
	int ret;

	if (!plcd_drv->pcfg->spi_cfg.spi_info) {
		LCDERR("%s: spi_info is null\n", __func__);
		return -1;
	}

	ctlr = spi_busnum_to_master(plcd_drv->pcfg->spi_cfg.spi_info->bus_num);
	if (!ctlr) {
		LCDERR("get busnum failed\n");
		return -1;
	}

	spi_device = spi_new_device(ctlr, plcd_drv->pcfg->spi_cfg.spi_info);
	if (!spi_device) {
		LCDERR("get spi_device failed\n");
		return -1;
	}
	plcd_drv->pcfg->spi_cfg.spi_dev = spi_device;

	ret = spi_register_driver(&plcd_spi_dev_driver);
	if (ret) {
		LCDERR("%s failed\n", __func__);
		return -1;
	}
	if (!plcd_drv->pcfg->spi_cfg.spi_dev) {
		LCDERR("%s failed\n", __func__);
		return -1;
	}

	LCDPR("%s ok\n", __func__);
	return 0;
}

int plcd_spi_driver_remove(void)
{
	if (plcd_drv->pcfg->spi_cfg.spi_dev)
		spi_unregister_driver(&plcd_spi_dev_driver);

	LCDPR("%s ok\n", __func__);

	return 0;
}

static irqreturn_t spi_vsync_isr(int irq, void *dev_id)
{
	// struct per_lcd_driver_s *plcd_drv = peripheral_lcd_get_driver();

	if (plcd_drv->init_flag == 0)
		return IRQ_HANDLED;

	if (plcd_drv->vsync_isr_cb)
		plcd_drv->vsync_isr_cb();

	return IRQ_HANDLED;
}

int plcd_spi_vsync_irq_init(void)
{
	unsigned int spi_vs_irq = 0;
	int ret;

	if (!plcd_drv->res_vs_irq) {
		LCDERR("%s: no spi vsync_irq\n", __func__);
		return -1;
	}
	spi_vs_irq = plcd_drv->res_vs_irq->start;
	ret = request_irq(spi_vs_irq, spi_vsync_isr, IRQF_TRIGGER_RISING,
			"per_lcd_spi_vsync_irq", (void *)"per_lcd");
	if (ret) {
		LCDERR("can't request spi_vs_irq\n");
		return -1;
	}
	if (per_lcd_debug_flag)
		LCDPR("request spi_vs_irq successful\n");
	return 0;
}

void spi_test_pure_color(unsigned int index)
{
	unsigned long c = 0;
	unsigned int size, block_s;
	unsigned int i = 0;

	if (plcd_set_mem()) {
		LCDERR("set mem failed\n");
		return;
	}

	switch (plcd_drv->pcfg->cfmt) {
	case CFMT_RGB888:
		c = rgb888_color_data[index];
		block_s = 3;
		break;
	case CFMT_RGB565:
		c = rgb565_color_data[index];
		block_s = 2;
		break;
	default:
		LCDERR("unsupported color format\n");
		return;
	}

	size = plcd_drv->pcfg->v * plcd_drv->pcfg->h;

	for (i = 0; i < size * block_s; i += block_s) {
		*((unsigned char *)plcd_drv->frame_addr + i) = c & 0xff;
		*((unsigned char *)plcd_drv->frame_addr + i + 1) = (c >> 8) & 0xff;
		if (block_s >= 3)
			*((unsigned char *)plcd_drv->frame_addr + i + 2) = (c >> 16) & 0xff;
	}

	if (plcd_drv->frame_flush)
		plcd_drv->frame_flush(plcd_drv->frame_addr);
	else if (plcd_drv->frame_post)
		plcd_drv->frame_post(plcd_drv->frame_addr,
			0, plcd_drv->pcfg->h, 0, plcd_drv->pcfg->v);
	else
		LCDERR("device not support frame_flush or frame_post\n");

	plcd_unset_mem();
}

/* 0: horizontal bar; 1: vertical bar; 2: horizontal line bar; 3: vertical line bar*/
void spi_test_color_bars(unsigned int index)
{
	unsigned int i, j, k = 0, block_s, addr_shift;
	unsigned char *_addr;
	unsigned int *c;

	if (plcd_set_mem()) {
		LCDERR("set mem failed\n");
		return;
	}

	switch (plcd_drv->pcfg->cfmt) {
	case CFMT_RGB888:
		c = rgb888_color_data;
		block_s = 3;
		break;
	case CFMT_RGB565:
		c = rgb565_color_data;
		block_s = 2;
		break;
	default:
		LCDERR("unsupported color format\n");
		return;
	}

	for (i = 0; i < plcd_drv->pcfg->v; i++) {
		for (j = 0; j < plcd_drv->pcfg->h; j++) {
			addr_shift = (plcd_drv->pcfg->v * i + j) * block_s;
			_addr = (unsigned char *)plcd_drv->frame_addr + addr_shift;
			if (index == 0)
				k = i / (plcd_drv->pcfg->h / 8 + 1);
			else if (index == 1)
				k = j / (plcd_drv->pcfg->v / 8 + 1);
			else if (index == 2)
				k = i % 8;
			else
				k = j % 8;
			_addr[0] =  c[k] & 0xff;
			_addr[1] = (c[k] >> 8) & 0xff;
			if (block_s == 3)
				_addr[2] = (c[k] >> 16) & 0xff;
		}
	}

	if (plcd_drv->frame_flush)
		plcd_drv->frame_flush(plcd_drv->frame_addr);
	else if (plcd_drv->frame_post)
		plcd_drv->frame_post(plcd_drv->frame_addr,
			0, plcd_drv->pcfg->h, 0, plcd_drv->pcfg->v);
	else
		LCDERR("device not support frame_flush or frame_post\n");

	plcd_unset_mem();
}

int spi_test_dft(const char *buf)
{
	int ret;
	unsigned int index;

	if (!plcd_drv->init_flag)
		return -1;

	switch (buf[0]) {
	case 'c': /*color fill*/
		ret = sscanf(buf, "color %d", &index);
		spi_test_pure_color(index);
		break;
	case 'b': /*colorbar*/
		ret = sscanf(buf, "bar %d", &index);
		spi_test_color_bars(index);
		break;
	default:
		LCDPR("unsupported command\n");
		break;
	}
	return 0;
}

int spi_power_cmd_dynamic_size(int flag)
{
	unsigned char *table;
	int i = 0, j, step = 0, max_len = 0;
	unsigned char type, cmd_size;
	int delay_ms, ret = 0;

	if (flag) {
		table = plcd_drv->pcfg->init_on;
		max_len = plcd_drv->pcfg->init_on_cnt;
	} else {
		table = plcd_drv->pcfg->init_off;
		max_len = plcd_drv->pcfg->init_off_cnt;
	}

	while ((i + 1) < max_len) {
		type = table[i];
		if (type == PER_LCD_CMD_TYPE_END)
			break;
		if (per_lcd_debug_flag) {
			LCDPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
			      __func__, step, type, table[i + 1]);
		}

		cmd_size = table[i + 1];
		if (cmd_size == 0)
			goto power_cmd_dynamic_next;
		if ((i + 2 + cmd_size) > max_len)
			break;

		if (type == PER_LCD_CMD_TYPE_NONE) {
			/* do nothing */
		} else if (type == PER_LCD_CMD_TYPE_GPIO) {
			if (cmd_size < 2) {
				LCDERR("step %d: invalid cmd_size %d for GPIO\n",
				       step, cmd_size);
				goto power_cmd_dynamic_next;
			}
			if (table[i + 2] < PER_GPIO_MAX)
				plcd_gpio_set(table[i + 2], table[i + 3]);
			if (cmd_size > 2) {
				if (table[i + 4] > 0)
					plcd_usleep(table[i + 4] * 1000);
			}
		} else if (type == PER_LCD_CMD_TYPE_DELAY) {
			delay_ms = 0;
			for (j = 0; j < cmd_size; j++)
				delay_ms += table[i + 2 + j];
			if (delay_ms > 0)
				plcd_usleep(delay_ms * 1000);
		} else if (type == PER_LCD_CMD_TYPE_CMD) {
			plcd_spi_write_dft(&table[i + 2], cmd_size, 8);
			plcd_usleep(1);
		} else if (type == PER_LCD_CMD_TYPE_CMD_DELAY) {
			plcd_spi_write_dft(&table[i + 2], (cmd_size - 1), 8);
			plcd_usleep(1);
			if (table[i + cmd_size + 1] > 0)
				plcd_usleep(table[i + cmd_size + 1] * 1000);
		} else {
			LCDERR("%s: type 0x%02x invalid\n", __func__, type);
		}
power_cmd_dynamic_next:
		i += (cmd_size + 2);
		step++;
	}

	return ret;
}

int spi_power_cmd_fixed_size(int flag)
{
	unsigned char *table;
	int i = 0, j, step = 0, max_len = 0;
	unsigned char type, cmd_size;
	int delay_ms, ret = 0;

	cmd_size = plcd_drv->pcfg->cmd_size;
	if (cmd_size < 2) {
		LCDERR("%s: invalid cmd_size %d\n", __func__, cmd_size);
		return -1;
	}

	if (flag) {
		table = plcd_drv->pcfg->init_on;
		max_len = plcd_drv->pcfg->init_on_cnt;
	} else {
		table = plcd_drv->pcfg->init_off;
		max_len = plcd_drv->pcfg->init_off_cnt;
	}

	while ((i + cmd_size) <= max_len) {
		type = table[i];
		if (type == PER_LCD_CMD_TYPE_END)
			break;
		if (per_lcd_debug_flag)
			LCDPR("%s: step %d type=0x%02x size=%d\n", __func__, step, type, cmd_size);

		if (type == PER_LCD_CMD_TYPE_NONE) {
			/* do nothing */
		} else if (type == PER_LCD_CMD_TYPE_GPIO) {
			if (table[i + 1] < PER_GPIO_MAX)
				plcd_gpio_set(table[i + 1], table[i + 2]);
			if (cmd_size > 3) {
				if (table[i + 3] > 0)
					plcd_usleep(table[i + 3] * 1000);
			}
		} else if (type == PER_LCD_CMD_TYPE_DELAY) {
			delay_ms = 0;
			for (j = 0; j < (cmd_size - 1); j++)
				delay_ms += table[i + 1 + j];
			plcd_usleep(delay_ms * 1000);
		} else if (type == PER_LCD_CMD_TYPE_CMD) {
			plcd_spi_write_dft(&table[i + 1], (cmd_size - 1), 8);
			plcd_usleep(1);
		} else if (type == PER_LCD_CMD_TYPE_CMD_DELAY) {
			plcd_spi_write_dft(&table[i + 1], cmd_size, 8);
			plcd_usleep(table[i + cmd_size - 1] * 1000);
		} else {
			LCDERR("%s: type 0x%02x invalid\n", __func__, type);
		}
		i += cmd_size;
		step++;
	}

	return ret;
}

int spi_power_on_init_dft(void)
{
	int ret = 0;

	if (plcd_drv->pcfg->init_on_cnt < 1) {
		LCDERR("%s: init_on_cnt %d is invalid\n", __func__, plcd_drv->pcfg->cmd_size);
		return -1;
	}
	if (!plcd_drv->pcfg->init_on) {
		LCDERR("%s: init_data is null\n", __func__);
		return -1;
	}

	if (plcd_drv->pcfg->cmd_size == PER_LCD_CMD_SIZE_DYNAMIC)
		ret = spi_power_cmd_dynamic_size(1);
	else
		ret = spi_power_cmd_fixed_size(1);
	plcd_drv->init_flag = 1;
	return ret;
}

int spi_power_off_dft(void)
{
	int ret = 0;

	if (plcd_drv->pcfg->init_off_cnt < 1) {
		LCDERR("%s: init_off_cnt %d is invalid\n", __func__, plcd_drv->pcfg->cmd_size);
		return -1;
	}
	if (!plcd_drv->pcfg->init_off) {
		LCDERR("%s: init_data is null\n", __func__);
		return -1;
	}

	if (plcd_drv->pcfg->cmd_size == PER_LCD_CMD_SIZE_DYNAMIC)
		ret = spi_power_cmd_dynamic_size(0);
	else
		ret = spi_power_cmd_fixed_size(0);
	plcd_drv->init_flag = 0;
	return ret;
}
