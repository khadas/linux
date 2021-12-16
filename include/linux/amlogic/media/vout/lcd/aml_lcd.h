/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _INC_AML_LCD_COMMON_H_
#define _INC_AML_LCD_COMMON_H_

struct lcd_i2c_match_s {
	unsigned int bus_id;
	char *bus_str;
};

#define LCD_GPIO_MAX                    0xff
#define LCD_GPIO_OUTPUT_LOW             0
#define LCD_GPIO_OUTPUT_HIGH            1
#define LCD_GPIO_INPUT                  2
#define LCD_CPU_GPIO_NAME_MAX           15

#define LCD_EXT_I2C_BUS_0          0  /* A */
#define LCD_EXT_I2C_BUS_1          1  /* B */
#define LCD_EXT_I2C_BUS_2          2  /* C */
#define LCD_EXT_I2C_BUS_3          3  /* D */
#define LCD_EXT_I2C_BUS_4          4  /* AO */
#define LCD_EXT_I2C_BUS_MAX        0xff

#define LCD_EXT_I2C_BUS_INVALID    0xff
#define LCD_EXT_I2C_ADDR_INVALID   0xff
#define LCD_EXT_GPIO_INVALID       0xff

#define LCD_EXT_SPI_CLK_FREQ_DFT   10 /* unit: KHz */

/*******************************************/
/*        LCD EXT CMD                      */
/*******************************************/
#define LCD_EXT_CMD_TYPE_CMD_DELAY              0x00
#define LCD_EXT_CMD_TYPE_CMD2_DELAY             0x01  /* for i2c device 2nd addr */
#define LCD_EXT_CMD_TYPE_CMD3_DELAY             0x02  /* for i2c device 3rd addr */
#define LCD_EXT_CMD_TYPE_CMD4_DELAY             0x03  /* for i2c device 4th addr */
#define LCD_EXT_CMD_TYPE_NONE                   0x10
#define LCD_EXT_CMD_TYPE_MULTI_FR               0x21 /* for dlg frame_rate list*/
#define LCD_EXT_CMD_TYPE_CMD_BIN2               0xa0  /* with reg offset and data_len*/
#define LCD_EXT_CMD_TYPE_CMD2_BIN2              0xa1  /* for i2c device 2nd addr */
#define LCD_EXT_CMD_TYPE_CMD3_BIN2              0xa2  /* for i2c device 3rd addr */
#define LCD_EXT_CMD_TYPE_CMD4_BIN2              0xa3  /* for i2c device 4th addr */
#define LCD_EXT_CMD_TYPE_CMD_BIN                0xb0
#define LCD_EXT_CMD_TYPE_CMD2_BIN               0xb1  /* for i2c device 2nd addr */
#define LCD_EXT_CMD_TYPE_CMD3_BIN               0xb2  /* for i2c device 3rd addr */
#define LCD_EXT_CMD_TYPE_CMD4_BIN               0xb3  /* for i2c device 4th addr */
#define LCD_EXT_CMD_TYPE_CMD                    0xc0
#define LCD_EXT_CMD_TYPE_CMD2                   0xc1  /* for i2c device 2nd addr */
#define LCD_EXT_CMD_TYPE_CMD3                   0xc2  /* for i2c device 3rd addr */
#define LCD_EXT_CMD_TYPE_CMD4                   0xc3  /* for i2c device 4th addr */
#define LCD_EXT_CMD_TYPE_CMD_BIN_DATA           0xd0 /* without auto fill reg addr 0x0 */
#define LCD_EXT_CMD_TYPE_CMD2_BIN_DATA          0xd1 /* for i2c device 2nd addr */
#define LCD_EXT_CMD_TYPE_CMD3_BIN_DATA          0xd2 /* for i2c device 3rd addr */
#define LCD_EXT_CMD_TYPE_CMD4_BIN_DATA          0xd3 /* for i2c device 4th addr */
#define LCD_EXT_CMD_TYPE_CMD_MULTI              0xe0
#define LCD_EXT_CMD_TYPE_CMD2_MULTI             0xe1
#define LCD_EXT_CMD_TYPE_CMD3_MULTI             0xe2
#define LCD_EXT_CMD_TYPE_CMD4_MULTI             0xe3
#define LCD_EXT_CMD_TYPE_GPIO                   0xf0
#define LCD_EXT_CMD_TYPE_CHECK                  0xfc
#define LCD_EXT_CMD_TYPE_DELAY                  0xfd
#define LCD_EXT_CMD_TYPE_END                    0xff

#define LCD_EXT_CMD_SIZE_DYNAMIC      0xff
#define LCD_EXT_DYNAMIC_SIZE_INDEX    1

unsigned char aml_lcd_i2c_bus_get_str(const char *str);

#endif

