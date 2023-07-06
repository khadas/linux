/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __PER_LCD_DEV_DRV_H
#define __PER_LCD_DEV_DRV_H
#include <linux/amlogic/media/vout/peripheral_lcd.h>
#include <linux/dma-mapping.h>

extern unsigned int rgb888_color_data[8];
extern unsigned int rgb666_color_data[8];
extern unsigned int rgb565_color_data[8];

extern struct peripheral_lcd_driver_s *plcd_drv;

/* peripheral lcd utils GPIO */
void plcd_usleep(unsigned int us);
void plcd_gpio_probe(unsigned int index);
int plcd_gpio_regist(int index, int init_value);
void plcd_gpio_set(int index, int value);
int plcd_gpio_get(int index);
int plcd_gpio_set_irq(int index);
/* peripheral lcd utils config */
int plcd_get_detail_config_dts(void);

/* peripheral lcd node/sys/class/peripheral_lcd/ */
extern unsigned char per_lcd_debug_flag;
int plcd_class_create(void);
void plcd_class_remove(void);

int plcd_spi_driver_add(void);
int plcd_spi_driver_remove(void);
int plcd_spi_vsync_irq_init(void);

/* peripheral lcd spi operation */
void spi_dcx_init(void);
void spi_dcx_high(void);
void spi_dcx_low(void);
void spi_rst_high(void);
void spi_rst_low(void);
int plcd_spi_read(void *tbuf, int tlen, void *rbuf, int rlen);
// cmd on SDA, data on SDA
int plcd_spi_write_dft(void *tbuf, int tlen, int word_bits);
// cmd on D0, data on D0~3
int plcd_spi_write_cmd1_data4(void *cbuf, int clen, void *dbuf, int dlen, int word_bits);
// spi init/test utils
void spi_test_pure_color(unsigned int index);
void spi_test_color_bars(unsigned int index);
int spi_test_dft(const char *buf);
int spi_power_cmd_dynamic_size(int flag);
int spi_power_cmd_fixed_size(int flag);
int spi_power_on_init_dft(void);
int spi_power_off_dft(void);
/* peripheral lcd spi operation DONE */

int plcd_dev_probe(void);
void plcd_dev_remove(void);

/* malloc internal mem, required if using test */
int plcd_set_mem(void);
void plcd_unset_mem(void);

/* peripheral lcd intel_8080 dev */
int plcd_i8080_probe(void);
int plcd_i8080_remove(void);
/* peripheral lcd spi interface st7789 */
int plcd_st7789_probe(void);
int plcd_st7789_remove(void);
/* peripheral lcd spi interface spd2010 */
int plcd_spd2010_probe(void);
int plcd_spd2010_remove(void);

#endif /* __PER_LCD_DEV_DRV_H */
