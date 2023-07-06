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

static unsigned long long reverse_bytes(unsigned long long value)
{
	return (value & 0x00000000000000FFUL) << 48 |
	       (value & 0x000000000000FF00UL) << 48 |
	       (value & 0x0000000000FF0000UL) << 16 |
	       (value & 0x00000000FF000000UL) << 16 |
	       (value & 0x000000FF00000000UL) >> 16 |
	       (value & 0x0000FF0000000000UL) >> 16 |
	       (value & 0x00FF000000000000UL) >> 48 |
	       (value & 0xFF00000000000000UL) >> 48;
}

static int endian64bit_convert(void *src_data, int size)
{
	unsigned char *src = (unsigned char *)src_data;

	if (size % 8) {
		LCDERR("%s: size error: %d\n", __func__, size);
		return -1;
	}
	while (size > 0) {
		*(unsigned long long *)src =
			reverse_bytes(*(unsigned long long *)src);
		src += 8;
		size -= 8;
	}
	return 0;
}

static void write_comm(unsigned char command)
{
	spi_dcx_low();
	udelay(1);
	plcd_spi_write_dft(&command, 1, 8);
}

static void write_data(unsigned char data)
{
	spi_dcx_high();
	udelay(1);
	plcd_spi_write_dft(&data, 1, 8);
}

static void set_spi_display_window(unsigned int x_start, unsigned int y_start,
				   unsigned int x_end, unsigned int y_end)
{
	write_comm(0x2A);
	write_data(x_start >> 8);
	write_data(x_start & 0xff);
	write_data(x_end >> 8);
	write_data(x_end & 0xff);

	write_comm(0x2B);
	write_data(y_start >> 8);
	write_data(y_start & 0xff);
	write_data(y_end >> 8);
	write_data(y_end & 0xff);

	write_comm(0x2c);
}

static int st7789_write_frame(unsigned char *addr, unsigned short x0,
			     unsigned short x1, unsigned short y0, unsigned short y1)
{
	int ret, size;

	set_spi_display_window(x0, y0, x1, y1);
	spi_dcx_high();
	plcd_usleep(1000);
	size = (x1 - x0) * (y1 - y0) * (plcd_drv->pcfg->cfmt == CFMT_RGB565 ? 2 : 3);
	endian64bit_convert(addr, size);

	ret = plcd_spi_write_dft(addr, size, 8);
	if (ret)
		LCDERR("%s: fail\n", __func__);
	return ret;
}

static int st7789_flush_frame(unsigned char *addr)
{
	st7789_write_frame(addr, 0, 0, plcd_drv->pcfg->h, plcd_drv->pcfg->v);
	return 0;
}

int plcd_st7789_probe(void)
{
	int ret;

	if (!plcd_drv->pcfg->spi_cfg.spi_dev) {
		LCDERR("spi_dev is null\n");
		return -1;
	}

	plcd_drv->enable = spi_power_on_init_dft;
	plcd_drv->disable = spi_power_off_dft;
	plcd_drv->frame_flush = st7789_flush_frame;
	plcd_drv->frame_post = st7789_write_frame;
	plcd_drv->test = spi_test_dft;

	plcd_spi_vsync_irq_init();
	plcd_gpio_probe(plcd_drv->pcfg->spi_cfg.dcx_index);

	ret = plcd_drv->enable();
	if (!ret)
		spi_test_pure_color(0);
	return 0;
}

int plcd_st7789_remove(void)
{
	return 0;
}
