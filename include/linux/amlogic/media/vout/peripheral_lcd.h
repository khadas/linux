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
enum per_dev_type_e {
	PER_DEV_TYPE_SPI = 0,
	PER_DEV_TYPE_NORMAL,
	PER_DEV_TYPE_MCU_8080 = 4,
	PER_DEV_TYPE_MAX,
};

struct per_gpio_s {
	char name[15];
	struct gpio_desc *gpio;
	int probe_flag;
	int register_flag;
};

#define PER_INIT_ON_MAX     300
#define PER_INIT_OFF_MAX    20
struct per_lcd_dev_config_s {
	char name[20];
	unsigned char type;
	int cs_hold_delay;
	int cs_clk_delay;
	int data_format;
	int color_format;
	unsigned short col;
	unsigned short row;
	int reset_index;
	int dcx_index;
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

	unsigned char init_loaded;
	unsigned char cmd_size;
	unsigned char *init_on;
	unsigned char *init_off;
	unsigned int init_on_cnt;
	unsigned int init_off_cnt;
	unsigned int max_gpio_num;
};

struct per_lcd_reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

struct peripheral_lcd_driver_s {
	unsigned char valid_flag;
	unsigned char dev_index;
	struct per_lcd_reg_map_s *per_lcd_reg_map;
	struct per_gpio_s per_gpio[PER_GPIO_NUM_MAX];

	struct per_lcd_config_s *per_lcd_conf;
	struct per_lcd_dev_config_s *per_lcd_dev_conf;

	int (*enable)(void);
	int (*disable)(void);
	int (*test)(const char *buf);
	int (*reg_read)(unsigned char *buf, unsigned int wlen,
			unsigned int rlen);
	int (*reg_write)(unsigned char *buf, unsigned int wlen);
	int (*frame_post)(unsigned char *addr, unsigned short x0,
			  unsigned short x1, unsigned short y0,
			  unsigned short y1);
	int (*frame_flush)(void);
	int (*set_color_format)(unsigned int cfmt);
	int (*set_gamma)(unsigned char *table, unsigned int rgb_sel);
	int (*set_flush_rate)(unsigned int rate);
	int (*vsync_isr_cb)(void);
	int irq_num;

	struct resource *res_vs_irq;

	struct device *dev;
	struct spi_device *spi_dev;
	struct spi_board_info *spi_info;
};

int te_cb(void);
struct peripheral_lcd_driver_s *peripheral_lcd_get_driver(void);
#endif
