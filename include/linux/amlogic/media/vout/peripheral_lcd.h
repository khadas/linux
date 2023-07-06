/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _INC_AML_PERIPHERAL_LCD_H_
#define _INC_AML_PERIPHERAL_LCD_H_
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>

#define LCDPR(fmt, args...)     pr_info("lcd: " fmt "", ## args)
#define LCDERR(fmt, args...)    pr_err("lcd: error: " fmt "", ## args)

#define PER_GPIO_OUTPUT_LOW      0
#define PER_GPIO_OUTPUT_HIGH     1
#define PER_GPIO_INPUT           2

#define PER_GPIO_MAX             0xff
#define PER_GPIO_NUM_MAX         23
enum per_lcd_color_fmt_e {
	CFMT_RGB888 = 0,
	CFMT_RGB565,
	CFMT_RGB666_24B,
	CFMT_RGB666_18B,
	CFMT_YUV420_12B,
	CFMT_YUV422_16B,
	CFMT_YUV444_24B,
	CFMT_MAX,
};

/*******************************************/
/*       PER LCD CMD                      */
/*******************************************/
#define PER_LCD_CMD_TYPE_CMD_DELAY    0x00
#define PER_LCD_CMD_TYPE_CMD2_DELAY   0x01  /* for i2c device 2nd addr */
#define PER_LCD_CMD_TYPE_NONE         0x10
#define PER_LCD_CMD_TYPE_CMD          0xc0
#define PER_LCD_CMD_TYPE_CMD2         0xc1  /* for i2c device 2nd addr */
#define PER_LCD_CMD_TYPE_GPIO         0xf0
#define PER_LCD_CMD_TYPE_CHECK        0xfc
#define PER_LCD_CMD_TYPE_DELAY        0xfd
#define PER_LCD_CMD_TYPE_END          0xff

#define PER_LCD_CMD_SIZE_DYNAMIC      0xff
#define PER_LCD_DYNAMIC_SIZE_INDEX    1
enum per_lcd_type_e {
	PLCD_TYPE_SPI = 0,
	PLCD_TYPE_QSPI = 1,
	PLCD_TYPE_MCU_8080 = 4,
	PLCD_TYPE_MAX,
};

struct per_gpio_s {
	char name[15];
	struct gpio_desc *gpio;
	int probe_flag;
	int register_flag;
};

#define PER_INIT_ON_MAX     3000
#define PER_INIT_OFF_MAX    20

struct spi_config_s {
	unsigned char cs_hold_delay;
	unsigned char cs_clk_delay;
	int dcx_index;
	int reset_index;

	struct spi_device *spi_dev;
	struct spi_board_info *spi_info;
};

struct i8080_config_s {
	unsigned int max_gpio_num;
	int reset_index;
	int nCS_index;
	int nRD_index;
	int nWR_index;
	int nRS_index;
	int data0_index;
	int data1_index;
	int data2_index;
	int data3_index;
	int data4_index;
	int data5_index;
	int data6_index;
	int data7_index;
};

struct per_lcd_config_s {
	char name[30];
	enum per_lcd_type_e type;
	unsigned short h, v;
	enum per_lcd_color_fmt_e cfmt;

	unsigned char cmd_size;
	unsigned char *init_on;
	unsigned char *init_off;
	unsigned int init_on_cnt;
	unsigned int init_off_cnt;

	union {
		struct spi_config_s spi_cfg;
		struct i8080_config_s i8080_cfg;
	};
};

struct per_lcd_reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

struct peripheral_lcd_driver_s {
	unsigned char dev_index;
	unsigned char probe_flag;
	unsigned char init_flag;

	struct resource *res_vs_irq;
	int irq_num;

	struct per_lcd_reg_map_s *plcd_reg_map;
	struct per_lcd_config_s *pcfg;

	void *frame_addr;

	struct device *dev;
	/**** interface implicate ****/
	int (*enable)(void);
	int (*disable)(void);

	int (*reg_read)(unsigned char *buf, unsigned int wlen, unsigned int rlen);
	int (*reg_write)(unsigned char *buf, unsigned int wlen);

	int (*set_flush_rate)(unsigned int rate);

	int (*frame_flush)(unsigned char *addr);
	int (*frame_post)(unsigned char *addr, unsigned short x0, unsigned short x1,
				unsigned short y0, unsigned short y1);

	int (*set_gamma)(unsigned char *table, unsigned int rgb_sel);
	int (*test)(const char *buf);

	int (*vsync_isr_cb)(void);
};

struct peripheral_lcd_driver_s *peripheral_lcd_get_driver(void);
#endif
