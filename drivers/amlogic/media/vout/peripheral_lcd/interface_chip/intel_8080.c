// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/time.h>
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
#include <linux/sched/clock.h>
#include <linux/amlogic/media/vout/peripheral_lcd.h>
#include <linux/io.h>
#include <linux/delay.h>
#include "../peripheral_lcd_drv.h"

#define CFG_WR_DATA_TOGETHER	1
#define CFG_VIRTUL_REG	1

#define CFG_RS_SEL_OUT	(1)

static unsigned int vir_reg_pad_gpio0_o;
static void *vir_io_base;

unsigned long long test_t[3];
/**
 * GPIO simulating intel 8080 8bits interface
 *
 * data 0 - 7:
 * 0-1: gpioa_4-5
 * 2-3: gpioa_10-11
 * 4-7: gpioa_15-18
 * cs: gpioa_0
 * rd: gpioa_1
 * rs: gpioa_9
 * reset: gpioa_8
 * wr: gpioa_14
 */
static inline void clrCS(void)
{
	//CLR_CS;
#if (CFG_VIRTUL_REG)
	vir_reg_pad_gpio0_o &= ~(1 << plcd_drv->pcfg->i8080_cfg.nCS_index);
	writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.nCS_index, 0);
#endif
};

static inline void setCS(void)
{
	//SET_CS;
#if (CFG_VIRTUL_REG)
	vir_reg_pad_gpio0_o |= 1 << plcd_drv->pcfg->i8080_cfg.nCS_index;
	writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.nCS_index, 1);
#endif
};

static inline void clrRESET(void)
{
	//clrRESET();
#if (CFG_VIRTUL_REG)
	vir_reg_pad_gpio0_o &= ~(1 << plcd_drv->pcfg->i8080_cfg.reset_index);
	writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.reset_index, 0);
#endif
};

static inline void setRESET(void)
{
	//setRESET();
#if (CFG_VIRTUL_REG)
	vir_reg_pad_gpio0_o |= 1 << plcd_drv->pcfg->i8080_cfg.reset_index;
	writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.reset_index, 1);
#endif
};

static inline void clrRS(void)
{
	//CLR_RS;
#if (CFG_VIRTUL_REG)
	vir_reg_pad_gpio0_o &= ~(1 << plcd_drv->pcfg->i8080_cfg.nRS_index);
	writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.nRS_index, 0);
#endif
};

static inline void setRS(void)
{
	//setRS();
#if (CFG_VIRTUL_REG)
	vir_reg_pad_gpio0_o |= 1 << plcd_drv->pcfg->i8080_cfg.nRS_index;
	writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.nRS_index, 1);
#endif
};

static inline void clrWR(void)
{
	//clrWR();
#if (CFG_VIRTUL_REG)
	vir_reg_pad_gpio0_o &= ~(1 << plcd_drv->pcfg->i8080_cfg.nWR_index);
	writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.nWR_index, 0);
#endif
};

static inline void setWR(void)
{
	//setWR();
#if (CFG_VIRTUL_REG)
	vir_reg_pad_gpio0_o |= 1 << plcd_drv->pcfg->i8080_cfg.nWR_index;
	writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.nWR_index, 1);
#endif
};

static inline void clrRD(void)
{
	//clrRD();
#if (CFG_VIRTUL_REG)
	vir_reg_pad_gpio0_o &= ~(1 << plcd_drv->pcfg->i8080_cfg.nRD_index);
	writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.nRD_index, 0);
#endif
};

static inline void setRD(void)
{
	//setRD();
#if (CFG_VIRTUL_REG)
	vir_reg_pad_gpio0_o |= 1 << plcd_drv->pcfg->i8080_cfg.nRD_index;
	writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.nRD_index, 1);
#endif
};

static inline void parallel_clk_direction(uint8_t dir)
{
#if (CFG_VIRTUL_REG)
	if (dir)
		vir_reg_pad_gpio0_o |= (1 << plcd_drv->pcfg->i8080_cfg.nRD_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.reset_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.nWR_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.nRS_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.nCS_index);
	else
		vir_reg_pad_gpio0_o &= ~((1 << plcd_drv->pcfg->i8080_cfg.nRD_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.reset_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.nWR_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.nRS_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.nCS_index));
	writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.reset_index, dir);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.nRS_index, dir);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.nRD_index, dir);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.nWR_index, dir);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.nCS_index, dir);
#endif
}

static inline void parallel_data_direction(uint8_t dir)
{
	/* TODO: */
#if (CFG_VIRTUL_REG)
	if (dir)
		vir_reg_pad_gpio0_o |= (1 << plcd_drv->pcfg->i8080_cfg.data0_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.data1_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.data2_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.data3_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.data4_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.data5_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.data6_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.data7_index);
	else
		vir_reg_pad_gpio0_o &=
			~((1 << plcd_drv->pcfg->i8080_cfg.data0_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.data1_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.data2_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.data3_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.data4_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.data5_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.data6_index) |
			(1 << plcd_drv->pcfg->i8080_cfg.data7_index));

	writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data0_index, dir);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data1_index, dir);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data2_index, dir);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data3_index, dir);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data4_index, dir);
	LCDPR("%s: read gpio_reg:0x%x\n", __func__, vir_reg_pad_gpio0_o);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data5_index, dir);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data6_index, dir);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data7_index, dir);
#endif
}

static inline void parallel_data_set(unsigned char data)
{
	unsigned int mask, val;
#if (CFG_VIRTUL_REG)
	/* TODO */
	mask = (0x1 << plcd_drv->pcfg->i8080_cfg.data0_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.data1_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.data2_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.data3_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.data4_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.data5_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.data6_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.data7_index);
	val = ((data & 0x1) << plcd_drv->pcfg->i8080_cfg.data0_index) |
	      (((data >> 0x1) & 0x1) << plcd_drv->pcfg->i8080_cfg.data1_index) |
	      (((data >> 0x2) & 0x1) << plcd_drv->pcfg->i8080_cfg.data2_index) |
	      (((data >> 0x3) & 0x1) << plcd_drv->pcfg->i8080_cfg.data3_index) |
	      (((data >> 0x4) & 0x1) << plcd_drv->pcfg->i8080_cfg.data4_index) |
	      (((data >> 0x5) & 0x1) << plcd_drv->pcfg->i8080_cfg.data5_index) |
	      (((data >> 0x6) & 0x1) << plcd_drv->pcfg->i8080_cfg.data6_index) |
	      (((data >> 0x7) & 0x1) << plcd_drv->pcfg->i8080_cfg.data7_index);
	vir_reg_pad_gpio0_o = ((vir_reg_pad_gpio0_o & (~mask)) | val);
	writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
	/* TODO */
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data0_index, data & 0x1);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data1_index, (data >> 1) & 0x1);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data2_index, (data >> 2) & 0x1);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data3_index, (data >> 3) & 0x1);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data4_index, (data >> 4) & 0x1);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data5_index, (data >> 5) & 0x1);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data6_index, (data >> 6) & 0x1);
	plcd_gpio_set(plcd_drv->pcfg->i8080_cfg.data7_index, (data >> 7) & 0x1);
#endif
}

#if (CFG_WR_DATA_TOGETHER)
static inline void parallel_data_wr_set(unsigned char data, unsigned char wr)
{
	unsigned int mask, val;
	/* TODO */
	mask = (0x1 << plcd_drv->pcfg->i8080_cfg.data0_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.data1_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.data2_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.data3_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.data4_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.data5_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.data6_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.data7_index) |
	       (0x1 << plcd_drv->pcfg->i8080_cfg.nWR_index);
	val = ((data & 0x1) << plcd_drv->pcfg->i8080_cfg.data0_index) |
	      (((data >> 0x1) & 0x1) << plcd_drv->pcfg->i8080_cfg.data1_index) |
	      (((data >> 0x2) & 0x1) << plcd_drv->pcfg->i8080_cfg.data2_index) |
	      (((data >> 0x3) & 0x1) << plcd_drv->pcfg->i8080_cfg.data3_index) |
	      (((data >> 0x4) & 0x1) << plcd_drv->pcfg->i8080_cfg.data4_index) |
	      (((data >> 0x5) & 0x1) << plcd_drv->pcfg->i8080_cfg.data5_index) |
	      (((data >> 0x6) & 0x1) << plcd_drv->pcfg->i8080_cfg.data6_index) |
	      (((data >> 0x7) & 0x1) << plcd_drv->pcfg->i8080_cfg.data7_index) |
	      ((wr & 0x1) << plcd_drv->pcfg->i8080_cfg.nWR_index);
	vir_reg_pad_gpio0_o = ((vir_reg_pad_gpio0_o & (~mask)) | val);
	writel(vir_reg_pad_gpio0_o, vir_io_base);
}
#endif

static inline uint8_t parallel_data_get(void)
{
	return 0;
}

static uint8_t read_data(void)
{
	unsigned char data;

	setRS();
	/* Clear data */
	parallel_data_set(0x00);
	/* Set data to input */
	parallel_data_direction(1);
	clrRD();
	plcd_usleep(1);
	setRD();
	plcd_usleep(10);
	/* read from the port */
	data = parallel_data_get();
	/* Set data to output */
	parallel_data_direction(0);
	plcd_usleep(10);

	return data;
}

static void reset_display(void)
{
	clrRESET();
	plcd_usleep(15);
	setRESET();
	plcd_usleep(10);
	setRD();
}

static void write_command(unsigned char command)
{
	clrRS();
	parallel_data_set(command & 0xFF);
	clrWR();
	setWR();
}

static void write_data(unsigned char data)
{
	setRS();
	setRD();
	parallel_data_set(data & 0xFF);
	clrWR();
	setWR();
}

static void write_regs(unsigned char cmd, unsigned char *buf, unsigned int len)
{
	int i;

	write_command(cmd);

	for (i = 0; i < len; i++)
		write_data(buf[i]);
}

/*  buf[0] is command
 *  buf[1] : buf[len-2] are datas
 *  len
 */
static void intel_8080_wreg_buf(unsigned char *buf, unsigned int len)
{
	if (len == 1)
		write_command(buf[0]);
	else
		write_regs(buf[0], &buf[1], len - 1);
}

static void write_block_888(unsigned int data)
{
#if (!CFG_RS_SEL_OUT)
	setRS();
#endif
	parallel_data_set((data >> 16) & 0xFF);
	clrWR();

#if (!CFG_WR_DATA_TOGETHER)
	setWR();
	parallel_data_set((data >> 8) & 0xFF);
	clrWR();

	setWR();
	parallel_data_set(data & 0xFF);
#else
	parallel_data_wr_set((data >> 8) & 0xFF, 1);
	clrWR();
	parallel_data_wr_set(data & 0xFF, 1);
#endif
	clrWR();
	//udelay(1);
	//udelay(1);
	setWR();
}

static void write_color(unsigned int color)
{
#if (!CFG_RS_SEL_OUT)
	setRS();
#endif
	//udelay(1);
	parallel_data_set((color >> 8) & 0xFF);
	clrWR();
	//udelay(1);
	/* fixme */
#if (!CFG_WR_DATA_TOGETHER)
	setWR();
	parallel_data_set(color & 0xFF);
#else
	parallel_data_wr_set(color & 0xFF, 1);
#endif
	clrWR();
	//udelay(1);
	setWR();
}

static void set_intel_8080_display_window(unsigned short x0, unsigned short y0,
				   unsigned short x1, unsigned short y1)
{
	write_command(0x2a);	// Column Address Set
	write_data(x0 >> 8);	// X address start:
	write_data(x0);		// 0 <= XS <= X
	write_data(x1 >> 8);	// X address end:
	write_data(x1);		// S <= XE <= X

	write_command(0x2b);	// Row Address Set
	write_data(y0 >> 8);	// Y address start:
	write_data(y0);		// 0 <= YS <= Y
	write_data(y1 >> 8);	// Y address start:
	write_data(y1);		// S <= YE <= Y

	/*This command is necessary to ensure*/
	/*the addresses are set correctly, do not remove*/
	write_command(0x2c); // Memory write
}

void display_home(void)
{
	write_command(0x2c); // Memory write
	// When this command is accepted, the column register and the page
	// register are reset to the start column/start page positions.
}

void enter_sleep(void)
{
	write_command(0x28);	// Display Off
	plcd_usleep(20);
	write_command(0x10);	// Sleep In (Low power mode)
}

void exit_sleep(void)
{
	write_command(0x11); // Exit Sleep Mode
	plcd_usleep(120);
	write_command(0x29); // Display on

	write_command(0x2c); // Memory write
	// When this command is accepted, the column register and the page
	// register are reset to the start column/start page positions.
}

void per_lcd_write_color(unsigned int *addr, unsigned short x0,
			 unsigned short x1, unsigned short y0,
			 unsigned short y1)
{
	unsigned int i, j;

	if (!addr) {
		LCDERR("buff is null\n");
		return;
	}

	set_intel_8080_display_window(x0, y0, (x1 - 1), (y1 - 1));
#if (CFG_RS_SEL_OUT)
	setRS();
#endif
	test_t[0] = sched_clock();
	for (i = 0; i < (y1 - y0); i++)
		for (j = 0; j < (x1 - x0); j++)
			write_color(addr[i]);
	test_t[1] = sched_clock();
	test_t[2] = test_t[1] - test_t[0];
	if (per_lcd_debug_flag)
		LCDPR("%s: cast time: %lld\n", __func__, test_t[2]);
}

static void per_lcd_write_frame(unsigned char *addr, unsigned short x0,
			      unsigned short x1, unsigned short y0,
			      unsigned short y1)
{
	unsigned int i, write_size;
	unsigned int *buf;
	unsigned int j = 0;

	if (!addr) {
		LCDERR("buff is null\n");
		return;
	}

	set_intel_8080_display_window(x0, y0, (x1 - 1), (y1 - 1));
#if (CFG_RS_SEL_OUT)
	setRS();
#endif
	write_size = (x1 - x0) * (y1 - y0);
	buf = kmalloc((sizeof(unsigned int) * write_size), GFP_KERNEL);
	if (!buf) {
		LCDERR("%s: failed to alloc buf\n", __func__);
		return;
	}

	/*data_format: 0: 888, 1: 666, 2: 565*/
	if (plcd_drv->pcfg->cfmt == CFMT_RGB888) {
		for (i = 0; i < (write_size * 3); i += 3) {
			buf[j] = (addr[i] & 0xff) |
				((addr[i + 1] & 0xff) << 8) |
				((addr[i + 2] & 0xff) << 16);
			write_block_888(buf[j]);
			j++;
		}
	} else if (plcd_drv->pcfg->cfmt == CFMT_RGB666_24B) {
		for (i = 0; i < (write_size * 3); i += 3) {
			buf[j] = (addr[i] & 0xff) |
				((addr[i + 1] & 0xff) << 8) |
				((addr[i + 2] & 0x3) << 16);
			write_block_888(buf[j]);
			j++;
		}
	} else if (plcd_drv->pcfg->cfmt == CFMT_RGB565) {
		for (i = 0; i < (write_size * 2); i += 2) {
			buf[j] = (addr[i] & 0xff) |
				((addr[i + 1] & 0xff) << 8);
			write_color(buf[j]);
			j++;
		}
		LCDPR("buf data: %x\n", buf[0]);
	} else {
		LCDERR("unsupport data_format: %d\n", plcd_drv->pcfg->cfmt);
	}
	kfree(buf);
	buf = NULL;
}

static int frame_post(unsigned char *addr, unsigned short x0,
	       unsigned short x1, unsigned short y0, unsigned short y1)
{
	if (!addr) {
		LCDERR("buff is null\n");
		return -1;
	}

	per_lcd_write_frame(addr, x0, x1, y0, y1);
	return 0;
}

static void intel_8080_fill_screen_color(unsigned int index)
{
	unsigned int *buf;
	unsigned long c = 0;
	unsigned int size;
	int i = 0;

	if (plcd_drv->pcfg->cfmt == CFMT_RGB888)
		c = rgb888_color_data[index];
	else if (plcd_drv->pcfg->cfmt == CFMT_RGB666_18B)
		c = rgb666_color_data[index];
	else if (plcd_drv->pcfg->cfmt == CFMT_RGB565)
		c = rgb565_color_data[i];
	else
		LCDERR("unsupport data_format\n");

	size = plcd_drv->pcfg->h * plcd_drv->pcfg->v;
	buf = kmalloc((sizeof(unsigned short) * size), GFP_KERNEL);
	if (!buf) {
		LCDERR("%s: failed to alloc buf\n", __func__);
		return;
	}
	for (i = 0; i < size; i++)
		buf[i] = c;

	per_lcd_write_color(buf, 0, plcd_drv->pcfg->h, 0, plcd_drv->pcfg->v);
	kfree(buf);
	buf = NULL;
}

void read_ID(void)
{
	unsigned char id[4] = {0, 0, 0, 0};
	unsigned char x;

	write_command(0x04);
	for (x = 0; x < 4; x++)
		id[x] = read_data();

	LCDPR("ID: 0x%02x %02x %02x %02x\n", id[0], id[1], id[2], id[3]);
}

void intel_8080_write_color_bars(void)
{
	unsigned int i, j;
	unsigned int c0 = 0, c1 = 0, c2 = 0, c3 = 0;
	unsigned int c4 = 0, c5 = 0, c6 = 0, c7 = 0;

	set_intel_8080_display_window(0x0000, 0x0000, 0x00EF, 0x013F);

#if (CFG_RS_SEL_OUT)
	setRS();
#endif
	if (plcd_drv->pcfg->cfmt == CFMT_RGB888) {
		c0 = rgb888_color_data[0];
		c1 = rgb888_color_data[1];
		c2 = rgb888_color_data[2];
		c3 = rgb888_color_data[3];
		c4 = rgb888_color_data[4];
		c5 = rgb888_color_data[5];
		c6 = rgb888_color_data[6];
		c7 = rgb888_color_data[7];
	} else if (plcd_drv->pcfg->cfmt == CFMT_RGB666_18B) {
		c0 = rgb666_color_data[0];
		c1 = rgb666_color_data[1];
		c2 = rgb666_color_data[2];
		c3 = rgb666_color_data[3];
		c4 = rgb666_color_data[4];
		c5 = rgb666_color_data[5];
		c6 = rgb666_color_data[6];
		c7 = rgb666_color_data[7];
	} else if (plcd_drv->pcfg->cfmt == CFMT_RGB565) {
		c0 = rgb565_color_data[0];
		c1 = rgb565_color_data[1];
		c2 = rgb565_color_data[2];
		c3 = rgb565_color_data[3];
		c4 = rgb565_color_data[4];
		c5 = rgb565_color_data[5];
		c6 = rgb565_color_data[6];
		c7 = rgb565_color_data[7];
	} else {
		LCDERR("unsupport data_format\n");
	}

	for (i = 0; i < plcd_drv->pcfg->h; i++) {
		for (j = 0; j < plcd_drv->pcfg->v; j++) {
			if (i > 279)
				write_color(c7);
			else if (i > 239)
				write_color(c6);
			else if (i > 199)
				write_color(c5);
			else if (i > 159)
				write_color(c4);
			else if (i > 119)
				write_color(c3);
			else if (i > 79)
				write_color(c2);
			else if (i > 39)
				write_color(c1);
			else
				write_color(c0);
		}
	}
}

static int intel_8080_power_cmd_dynamic_size(int flag)
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
				LCDERR("step %d: invalid cmd_size %d for GPIO\n", step, cmd_size);
				goto power_cmd_dynamic_next;
			}
			if (table[i + 2] < PER_GPIO_MAX) {
#if (CFG_VIRTUL_REG)
				if (table[i + 3])
					vir_reg_pad_gpio0_o |= 1 << table[i + 2];
				else
					vir_reg_pad_gpio0_o &= ~(1 << table[i + 2]);
				writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
				plcd_gpio_set(table[i + 2], table[i + 3]);
#endif
			}
			if (cmd_size > 2)
				plcd_usleep(table[i + 4] * 1000);
		} else if (type == PER_LCD_CMD_TYPE_DELAY) {
			delay_ms = 0;
			for (j = 0; j < cmd_size; j++)
				delay_ms += table[i + 2 + j];
			plcd_usleep(delay_ms * 1000);
		} else if (type == PER_LCD_CMD_TYPE_CMD) {
			intel_8080_wreg_buf(&table[i + 2], cmd_size);
			plcd_usleep(1);
		} else if (type == PER_LCD_CMD_TYPE_CMD_DELAY) {
			intel_8080_wreg_buf(&table[i + 2], (cmd_size - 1));
			plcd_usleep(1);
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

static int intel_8080_power_cmd_fixed_size(int flag)
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
		if (per_lcd_debug_flag) {
			LCDPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
			      __func__, step, type, cmd_size);
		}

		if (type == PER_LCD_CMD_TYPE_NONE) {
			/* do nothing */
		} else if (type == PER_LCD_CMD_TYPE_GPIO) {
			if (table[i + 1] < PER_GPIO_MAX) {
#if (CFG_VIRTUL_REG)
				if (table[i + 2])
					vir_reg_pad_gpio0_o |= 1 << table[i + 1];
				else
					vir_reg_pad_gpio0_o &= ~(1 << table[i + 1]);
				writel(vir_reg_pad_gpio0_o, vir_io_base);
#else
				plcd_gpio_set(table[i + 1], table[i + 2]);
#endif
			}
			if (cmd_size > 3)
				plcd_usleep(table[i + 3] * 1000);
		} else if (type == PER_LCD_CMD_TYPE_DELAY) {
			delay_ms = 0;
			for (j = 0; j < (cmd_size - 1); j++)
				delay_ms += table[i + 1 + j];
			plcd_usleep(delay_ms * 1000);
		} else if (type == PER_LCD_CMD_TYPE_CMD) {
			intel_8080_wreg_buf(&table[i + 1], (cmd_size - 1));
			plcd_usleep(1);
		} else if (type == PER_LCD_CMD_TYPE_CMD_DELAY) {
			intel_8080_wreg_buf(&table[i + 1], cmd_size);
			plcd_usleep(table[i + cmd_size - 1] * 1000);
		} else {
			LCDERR("%s: type 0x%02x invalid\n", __func__, type);
		}
		i += cmd_size;
		step++;
	}

	return ret;
}

static void intel_8080_interface_init(void)
{
#if (CFG_VIRTUL_REG)
	int i;
	/*set all pins input*/
	for (i = 0; i < plcd_drv->pcfg->i8080_cfg.max_gpio_num; i++)
		plcd_gpio_set(i, 2);
	vir_reg_pad_gpio0_o = readl(vir_io_base);
	LCDPR("%s: read gpio_reg:0x%x\n", __func__, vir_reg_pad_gpio0_o);
	/* set all pins output*/
	for (i = 0; i < plcd_drv->pcfg->i8080_cfg.max_gpio_num; i++)
		plcd_gpio_set(i, 0);
	LCDPR("%s: read gpio_reg:0x%x\n", __func__, vir_reg_pad_gpio0_o);
#endif
}

static int intel_8080_power_on_init(void)
{
	int ret = 0;

	if (plcd_drv->pcfg->cmd_size < 1) {
		LCDERR("%s: cmd_size %d is invalid\n", __func__, plcd_drv->pcfg->cmd_size);
		return -1;
	}
	if (!plcd_drv->pcfg->init_on) {
		LCDERR("%s: init_data is null\n", __func__);
		return -1;
	}
	intel_8080_interface_init();
	if (plcd_drv->pcfg->cmd_size == PER_LCD_CMD_SIZE_DYNAMIC)
		ret = intel_8080_power_cmd_dynamic_size(1);
	else
		ret = intel_8080_power_cmd_fixed_size(1);
	plcd_drv->init_flag = 1;
	return ret;
}

int intel_8080_test(const char *buf)
{
	int ret;
	unsigned int index;

	switch (buf[0]) {
	case 'i': /*id*/
		intel_8080_power_on_init();
		plcd_drv->init_flag = 1;
		read_ID();
		break;
	case 'c': /*color fill*/
		if (!plcd_drv->init_flag)
			intel_8080_power_on_init();
		ret = sscanf(buf, "color %d", &index);
		intel_8080_fill_screen_color(index);
		break;
	case 'b': /*colorbar*/
		if (!plcd_drv->init_flag)
			intel_8080_power_on_init();
		intel_8080_write_color_bars();
		break;
	case 'r': /*reset*/
		reset_display();
		break;
	default:
		LCDPR("unsupported test\n");
		break;
	}
	return 0;
}

int intel_8080_clk_test(char *buf)
{
	int ret, value, index;

	switch (buf[0]) {
	case 'r': /*rs*/
		ret = sscanf(buf, "rs %d", &value);
		if (value)
			setRS();
		else
			clrRS();
		LCDPR("set RS value: %d, read_gpio_reg:0x%x\n", value,
		      vir_reg_pad_gpio0_o);
	break;
	case 'w': /*WR*/
		ret = sscanf(buf, "wr %d", &value);
		if (value)
			setWR();
		else
			clrWR();
		LCDPR("set WR value: %d, read_gpio_reg:0x%x\n", value,
		      vir_reg_pad_gpio0_o);
	break;
	case 'd': /*RD*/
		ret = sscanf(buf, "rd %d", &value);
		if (value)
			setRD();
		else
			clrRD();
		LCDPR("set RD value: %d, read_gpio_reg:0x%x\n", value,
		      vir_reg_pad_gpio0_o);
	break;
	case 'c': /*CS*/
		ret = sscanf(buf, "cs %d", &value);
		if (value)
			setCS();
		else
			clrCS();
		LCDPR("set CS value: %d, read_gpio_reg:0x%x\n", value,
		      vir_reg_pad_gpio0_o);
	break;
	case 's': /*reset*/
		ret = sscanf(buf, "set %d", &value);
		if (value)
			setRESET();
		else
			clrRESET();
		LCDPR("set CS value: %d, read_gpio_reg:0x%x\n", value,
		      vir_reg_pad_gpio0_o);
	break;
	case 'i': /*index*/
		ret = sscanf(buf, "index %d %d", &index, &value);
		if (value)
			vir_reg_pad_gpio0_o |= 1 << index;
		else
			vir_reg_pad_gpio0_o &= ~(1 << index);

		writel(vir_reg_pad_gpio0_o, vir_io_base);
		LCDPR("set index: %d value: %d, read_gpio_reg:0x%x\n",
		      index, value, vir_reg_pad_gpio0_o);
	break;
	default:
		LCDPR("no support\n");
	break;
	}
	return 0;
}

int plcd_i8080_probe(void)
{
	int ret;

	plcd_drv->frame_post = frame_post;
	plcd_drv->enable = intel_8080_power_on_init;
	plcd_drv->test = intel_8080_test;

	vir_io_base = plcd_drv->plcd_reg_map->p;
	if (!vir_io_base) {
		LCDERR("io base err, check reg\n");
		return -1;
	}
	LCDPR("%s: io_base:0x%px", __func__, vir_io_base);

	ret = intel_8080_power_on_init();
	if (!ret)
		intel_8080_fill_screen_color(0);
	return 0;
}

int plcd_i8080_remove(void)
{
	return 0;
}
