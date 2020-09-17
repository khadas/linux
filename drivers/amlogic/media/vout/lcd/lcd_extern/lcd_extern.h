/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _LCD_EXTERN_H_
#define _LCD_EXTERN_H_
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

#define EXTPR(fmt, args...)     pr_info("lcd extern: " fmt "", ## args)
#define EXTERR(fmt, args...)    pr_info("lcd extern: error: " fmt "", ## args)

#define LCD_EXTERN_DRIVER		"lcd_extern"

#ifdef CONFIG_USE_OF
struct device_node *aml_lcd_extern_get_dts_child(int index);
#endif
void lcd_extern_gpio_probe(unsigned char index);
void lcd_extern_gpio_set(unsigned char index, int value);
unsigned int lcd_extern_gpio_get(unsigned char index);
void lcd_extern_pinmux_set(int status);

/* common API */
struct aml_lcd_extern_i2c_dev_s *lcd_extern_get_i2c_device(unsigned char addr);
int lcd_extern_i2c_write(struct i2c_client *i2client,
			 unsigned char *buff, unsigned int len);
int lcd_extern_i2c_read(struct i2c_client *i2client,
			unsigned char *buff, unsigned int len);

/* specific API */
int aml_lcd_extern_default_probe
	(struct aml_lcd_extern_driver_s *ext_drv);
int aml_lcd_extern_mipi_default_probe
	(struct aml_lcd_extern_driver_s *ext_drv);

#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_T5800Q
int aml_lcd_extern_i2c_T5800Q_probe
	(struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_ANX6862_7911
int aml_lcd_extern_i2c_ANX6862_7911_probe
	(struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_DLPC3439
int aml_lcd_extern_i2c_DLPC3439_probe
	(struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_TC101
int aml_lcd_extern_i2c_tc101_probe
	(struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_ANX6345
int aml_lcd_extern_i2c_anx6345_probe
	(struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_I2C_CS602
int aml_lcd_extern_i2c_CS602_probe
	(struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_SPI_LD070WS2
int aml_lcd_extern_spi_LD070WS2_probe
	(struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_N070ICN
int aml_lcd_extern_mipi_N070ICN_probe
	(struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_KD080D13
int aml_lcd_extern_mipi_KD080D13_probe
	(struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_TV070WSM
int aml_lcd_extern_mipi_TV070WSM_probe
	(struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_ST7701
int aml_lcd_extern_mipi_st7701_probe
	(struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_P070ACB
int aml_lcd_extern_mipi_p070acb_probe
	(struct aml_lcd_extern_driver_s *ext_drv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_EXTERN_MIPI_TL050FHV02CT
int aml_lcd_extern_mipi_tl050fhv02ct_probe
	(struct aml_lcd_extern_driver_s *ext_drv);
#endif

#endif

