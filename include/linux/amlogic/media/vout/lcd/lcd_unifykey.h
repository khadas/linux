/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#ifndef _INC_AML_LCD_UNIFYKEY_H__
#define _INC_AML_LCD_UNIFYKEY_H__
#include <linux/amlogic/media/vout/lcd/lcd_tcon_data.h>

#define LCD_UNIFYKEY_WAIT_TIMEOUT      8000
#define LCD_UNIFYKEY_RETRY_INTERVAL    20   /* ms */

unsigned int cal_crc32(unsigned int crc, const unsigned char *buf,
		       int buf_len);

/* declare external unifykey function */
void *get_ukdev(void);
int key_unify_read(void *ukdev, char *keyname, unsigned char *keydata,
		   unsigned int datalen, unsigned int *reallen);
int key_unify_size(void *ukdev, char *keyname, unsigned int *reallen);
int key_unify_query(void *ukdev, char *keyname,
		    unsigned int *keystate, unsigned int *keypermit);
int key_unify_get_init_flag(void);

#define LCD_UKEY_RETRY_CNT_MAX   5
/*
 *  lcd unifykey data struct: little-endian, for example:
 *    4byte: d[0]=0x01, d[1]=0x02, d[2] = 0x03, d[3]= 0x04,
 *	   data = 0x04030201
 */

/* define lcd unifykey length */

#define LCD_UKEY_HEAD_SIZE        10
#define LCD_UKEY_HEAD_CRC32       4
#define LCD_UKEY_HEAD_DATA_LEN    2
#define LCD_UKEY_HEAD_VERSION     2
#define LCD_UKEY_HEAD_RESERVED    2

struct aml_lcd_unifykey_header_s {
	unsigned int crc32;
	unsigned short data_len;
	unsigned char version;
	unsigned char block_next_flag;
	unsigned short block_cur_size;
};

/* ********************************
 * lcd
 * *********************************
 */
/* V1: 265 */
/* V2: 319 */
#define LCD_UKEY_LCD_SIZE          700 //265+424

/* header (10Byte) */
/* LCD_UKEY_HEAD_SIZE */
/* basic (36Byte) */
#define LCD_UKEY_MODEL_NAME      (LCD_UKEY_HEAD_SIZE + 0)
#define LCD_UKEY_INTERFACE       (LCD_UKEY_MODEL_NAME + 30)
#define LCD_UKEY_LCD_BITS        (LCD_UKEY_MODEL_NAME + 31)
#define LCD_UKEY_SCREEN_WIDTH    (LCD_UKEY_MODEL_NAME + 32)
#define LCD_UKEY_SCREEN_HEIGHT   (LCD_UKEY_MODEL_NAME + 34)
/* timing (18Byte) */
#define LCD_UKEY_H_ACTIVE        (LCD_UKEY_MODEL_NAME + 36)/* +36 byte */
#define LCD_UKEY_V_ACTIVE        (LCD_UKEY_MODEL_NAME + 38)
#define LCD_UKEY_H_PERIOD        (LCD_UKEY_MODEL_NAME + 40)
#define LCD_UKEY_V_PERIOD        (LCD_UKEY_MODEL_NAME + 42)
#define LCD_UKEY_HS_WIDTH        (LCD_UKEY_MODEL_NAME + 44)
#define LCD_UKEY_HS_BP           (LCD_UKEY_MODEL_NAME + 46)
#define LCD_UKEY_HS_POL          (LCD_UKEY_MODEL_NAME + 48)
#define LCD_UKEY_VS_WIDTH        (LCD_UKEY_MODEL_NAME + 49)
#define LCD_UKEY_VS_BP           (LCD_UKEY_MODEL_NAME + 51)
#define LCD_UKEY_VS_POL          (LCD_UKEY_MODEL_NAME + 53)
/* customer (31Byte) */
#define LCD_UKEY_FR_ADJ_TYPE     (LCD_UKEY_MODEL_NAME + 54)/* +36+18 byte */
#define LCD_UKEY_SS_LEVEL        (LCD_UKEY_MODEL_NAME + 55)
#define LCD_UKEY_CLK_AUTO_GEN    (LCD_UKEY_MODEL_NAME + 56)
#define LCD_UKEY_PCLK            (LCD_UKEY_MODEL_NAME + 57)
#define LCD_UKEY_H_PERIOD_MIN    (LCD_UKEY_MODEL_NAME + 61)
#define LCD_UKEY_H_PERIOD_MAX    (LCD_UKEY_MODEL_NAME + 63)
#define LCD_UKEY_V_PERIOD_MIN    (LCD_UKEY_MODEL_NAME + 65)
#define LCD_UKEY_V_PERIOD_MAX    (LCD_UKEY_MODEL_NAME + 67)
#define LCD_UKEY_PCLK_MIN        (LCD_UKEY_MODEL_NAME + 69)
#define LCD_UKEY_PCLK_MAX        (LCD_UKEY_MODEL_NAME + 73)
#define LCD_UKEY_VLOCK_VAL_0     (LCD_UKEY_MODEL_NAME + 77)
#define LCD_UKEY_VLOCK_VAL_1     (LCD_UKEY_MODEL_NAME + 78)
#define LCD_UKEY_VLOCK_VAL_2     (LCD_UKEY_MODEL_NAME + 79)
#define LCD_UKEY_VLOCK_VAL_3     (LCD_UKEY_MODEL_NAME + 80)
#define LCD_UKEY_CUST_PINMUX     (LCD_UKEY_MODEL_NAME + 81)
#define LCD_UKEY_FR_AUTO_DIS     (LCD_UKEY_MODEL_NAME + 82)
#define LCD_UKEY_FRAME_RATE_MIN  (LCD_UKEY_MODEL_NAME + 83)
#define LCD_UKEY_FRAME_RATE_MAX  (LCD_UKEY_MODEL_NAME + 84)

/* interface (20Byte) */
#define LCD_UKEY_IF_ATTR_0       (LCD_UKEY_MODEL_NAME + 85)/* +36+18+31 byte */
#define LCD_UKEY_IF_ATTR_1       (LCD_UKEY_MODEL_NAME + 87)
#define LCD_UKEY_IF_ATTR_2       (LCD_UKEY_MODEL_NAME + 89)
#define LCD_UKEY_IF_ATTR_3       (LCD_UKEY_MODEL_NAME + 91)
#define LCD_UKEY_IF_ATTR_4       (LCD_UKEY_MODEL_NAME + 93)
#define LCD_UKEY_IF_ATTR_5       (LCD_UKEY_MODEL_NAME + 95)
#define LCD_UKEY_IF_ATTR_6       (LCD_UKEY_MODEL_NAME + 97)
#define LCD_UKEY_IF_ATTR_7       (LCD_UKEY_MODEL_NAME + 99)
#define LCD_UKEY_IF_ATTR_8       (LCD_UKEY_MODEL_NAME + 101)
#define LCD_UKEY_IF_ATTR_9       (LCD_UKEY_MODEL_NAME + 103)

#define LCD_UKEY_DATA_LEN_V1        (LCD_UKEY_MODEL_NAME + 105)
/* power (5Byte * n) */
/* v1/v2  p + offsite*/
/* 10+36+18+31+20 byte */
/* 10+36+18+31+20+44+10 byte */
#define LCD_UKEY_PWR_STEP          (LCD_UKEY_MODEL_NAME + 105)
#define LCD_UKEY_PWR_TYPE          (0)
#define LCD_UKEY_PWR_INDEX         (1)
#define LCD_UKEY_PWR_VAL           (2)
#define LCD_UKEY_PWR_DELAY         (3)

/* version 2 */
#define LCD_UKEY_DATA_LEN_V2        424

/* header (10Byte) */
/* LCD_UKEY_HEAD_SIZE */
/* phy (356Byte) */
#define LCD_UKEY_PHY_ATTR_FLAG     (LCD_UKEY_HEAD_SIZE + 0)
#define LCD_UKEY_PHY_ATTR_0        (LCD_UKEY_HEAD_SIZE + 4)
#define LCD_UKEY_PHY_ATTR_1        (LCD_UKEY_HEAD_SIZE + 6)
#define LCD_UKEY_PHY_ATTR_2        (LCD_UKEY_HEAD_SIZE + 8)
#define LCD_UKEY_PHY_ATTR_3        (LCD_UKEY_HEAD_SIZE + 10)
#define LCD_UKEY_PHY_ATTR_4        (LCD_UKEY_HEAD_SIZE + 12)
#define LCD_UKEY_PHY_ATTR_5        (LCD_UKEY_HEAD_SIZE + 14)
#define LCD_UKEY_PHY_ATTR_6        (LCD_UKEY_HEAD_SIZE + 16)
#define LCD_UKEY_PHY_ATTR_7        (LCD_UKEY_HEAD_SIZE + 18)
#define LCD_UKEY_PHY_ATTR_8        (LCD_UKEY_HEAD_SIZE + 20)
#define LCD_UKEY_PHY_ATTR_9        (LCD_UKEY_HEAD_SIZE + 22)
#define LCD_UKEY_PHY_ATTR_10       (LCD_UKEY_HEAD_SIZE + 24)
#define LCD_UKEY_PHY_ATTR_11       (LCD_UKEY_HEAD_SIZE + 26)
#define LCD_UKEY_PHY_LANE_CTRL     (LCD_UKEY_HEAD_SIZE + 28)//64*4
#define LCD_UKEY_PHY_LANE_PN_SWAP  (LCD_UKEY_HEAD_SIZE + 284)//8
#define LCD_UKEY_PHY_LANE_SWAP     (LCD_UKEY_HEAD_SIZE + 292)//64
/* custom ctrl (76Byte) */
#define LCD_UKEY_CUS_CTRL_ATTR_FLAG    (LCD_UKEY_HEAD_SIZE + 356)
#define LCD_UKEY_CUS_CTRL_ATTR_0       (LCD_UKEY_HEAD_SIZE + 360)
#define LCD_UKEY_CUS_CTRL_ATTR_0_PARM0 (LCD_UKEY_HEAD_SIZE + 362)
#define LCD_UKEY_CUS_CTRL_ATTR_0_PARM1 (LCD_UKEY_HEAD_SIZE + 364)
#define LCD_UKEY_CUS_CTRL_ATTR_0_PARM2 (LCD_UKEY_HEAD_SIZE + 366)
#define LCD_UKEY_CUS_CTRL_ATTR_0_PARM3 (LCD_UKEY_HEAD_SIZE + 368)
#define LCD_UKEY_CUS_CTRL_ATTR_0_PARM4 (LCD_UKEY_HEAD_SIZE + 370)
#define LCD_UKEY_CUS_CTRL_ATTR_0_PARM5 (LCD_UKEY_HEAD_SIZE + 372)
#define LCD_UKEY_CUS_CTRL_ATTR_0_PARM6 (LCD_UKEY_HEAD_SIZE + 374)
#define LCD_UKEY_CUS_CTRL_ATTR_0_PARM7 (LCD_UKEY_HEAD_SIZE + 376)
#define LCD_UKEY_CUS_CTRL_END          (LCD_UKEY_HEAD_SIZE + 432)

/* ********************************
 * lcd extern
 * *********************************
 */
#define LCD_UKEY_LCD_EXT_SIZE       550

/* header (10Byte) */
/* LCD_UKEY_HEAD_SIZE */
/* basic (33Byte) */
#define LCD_UKEY_EXT_NAME           (LCD_UKEY_HEAD_SIZE + 0)
#define LCD_UKEY_EXT_INDEX          (LCD_UKEY_EXT_NAME + 30)
#define LCD_UKEY_EXT_TYPE           (LCD_UKEY_EXT_NAME + 31)
#define LCD_UKEY_EXT_STATUS         (LCD_UKEY_EXT_NAME + 32)
/* type (10Byte) */
#define LCD_UKEY_EXT_TYPE_VAL_0     (LCD_UKEY_EXT_NAME + 33)/* +33 byte */
#define LCD_UKEY_EXT_TYPE_VAL_1     (LCD_UKEY_EXT_NAME + 34)
#define LCD_UKEY_EXT_TYPE_VAL_2     (LCD_UKEY_EXT_NAME + 35)
#define LCD_UKEY_EXT_TYPE_VAL_3     (LCD_UKEY_EXT_NAME + 36)
#define LCD_UKEY_EXT_TYPE_VAL_4     (LCD_UKEY_EXT_NAME + 37)
#define LCD_UKEY_EXT_TYPE_VAL_5     (LCD_UKEY_EXT_NAME + 38)
#define LCD_UKEY_EXT_TYPE_VAL_6     (LCD_UKEY_EXT_NAME + 39)
#define LCD_UKEY_EXT_TYPE_VAL_7     (LCD_UKEY_EXT_NAME + 40)
#define LCD_UKEY_EXT_TYPE_VAL_8     (LCD_UKEY_EXT_NAME + 41)
#define LCD_UKEY_EXT_TYPE_VAL_9     (LCD_UKEY_EXT_NAME + 42)
/* init (cmd_size) */
#define LCD_UKEY_EXT_INIT           (LCD_UKEY_EXT_NAME + 43)/* +33+10 byte */
/*#define LCD_UKEY_EXT_INIT_TYPE      (0)*/
/*#define LCD_UKEY_EXT_INIT_VAL        1   //not defined */
/*#define LCD_UKEY_EXT_INIT_DELAY     (n)*/

/* ********************************
 * backlight
 * *********************************
 */
/* V1: 92 */
/* V2: 118 */
#define LCD_UKEY_BL_SIZE            118

/* header (10Byte) */
/* LCD_UKEY_HEAD_SIZE */
/* basic (30Byte) */
#define LCD_UKEY_BL_NAME            (LCD_UKEY_HEAD_SIZE + 0)
/* level (12Byte) */
#define LCD_UKEY_BL_LEVEL_UBOOT     (LCD_UKEY_BL_NAME + 30)/* +30 byte */
#define LCD_UKEY_BL_LEVEL_KERNEL    (LCD_UKEY_BL_NAME + 32)
#define LCD_UKEY_BL_LEVEL_MAX       (LCD_UKEY_BL_NAME + 34)
#define LCD_UKEY_BL_LEVEL_MIN       (LCD_UKEY_BL_NAME + 36)
#define LCD_UKEY_BL_LEVEL_MID       (LCD_UKEY_BL_NAME + 38)
#define LCD_UKEY_BL_LEVEL_MID_MAP   (LCD_UKEY_BL_NAME + 40)
/* method (8Byte) */
#define LCD_UKEY_BL_METHOD          (LCD_UKEY_BL_NAME + 42)/* +30+12 byte */
#define LCD_UKEY_BL_EN_GPIO         (LCD_UKEY_BL_NAME + 43)
#define LCD_UKEY_BL_EN_GPIO_ON      (LCD_UKEY_BL_NAME + 44)
#define LCD_UKEY_BL_EN_GPIO_OFF     (LCD_UKEY_BL_NAME + 45)
#define LCD_UKEY_BL_ON_DELAY        (LCD_UKEY_BL_NAME + 46)
#define LCD_UKEY_BL_OFF_DELAY       (LCD_UKEY_BL_NAME + 48)
/* pwm (32Byte) */
#define LCD_UKEY_BL_PWM_ON_DELAY    (LCD_UKEY_BL_NAME + 50)/* +30+12+8 byte */
#define LCD_UKEY_BL_PWM_OFF_DELAY   (LCD_UKEY_BL_NAME + 52)
#define LCD_UKEY_BL_PWM_METHOD      (LCD_UKEY_BL_NAME + 54)
#define LCD_UKEY_BL_PWM_PORT        (LCD_UKEY_BL_NAME + 55)
#define LCD_UKEY_BL_PWM_FREQ        (LCD_UKEY_BL_NAME + 56)
#define LCD_UKEY_BL_PWM_DUTY_MAX    (LCD_UKEY_BL_NAME + 60)
#define LCD_UKEY_BL_PWM_DUTY_MIN    (LCD_UKEY_BL_NAME + 61)
#define LCD_UKEY_BL_PWM_GPIO        (LCD_UKEY_BL_NAME + 62)
#define LCD_UKEY_BL_PWM_GPIO_OFF    (LCD_UKEY_BL_NAME + 63)
#define LCD_UKEY_BL_PWM2_METHOD     (LCD_UKEY_BL_NAME + 64)
#define LCD_UKEY_BL_PWM2_PORT       (LCD_UKEY_BL_NAME + 65)
#define LCD_UKEY_BL_PWM2_FREQ       (LCD_UKEY_BL_NAME + 66)
#define LCD_UKEY_BL_PWM2_DUTY_MAX   (LCD_UKEY_BL_NAME + 70)
#define LCD_UKEY_BL_PWM2_DUTY_MIN   (LCD_UKEY_BL_NAME + 71)
#define LCD_UKEY_BL_PWM2_GPIO       (LCD_UKEY_BL_NAME + 72)
#define LCD_UKEY_BL_PWM2_GPIO_OFF   (LCD_UKEY_BL_NAME + 73)
#define LCD_UKEY_BL_PWM_LEVEL_MAX   (LCD_UKEY_BL_NAME + 74)
#define LCD_UKEY_BL_PWM_LEVEL_MIN   (LCD_UKEY_BL_NAME + 76)
#define LCD_UKEY_BL_PWM2_LEVEL_MAX  (LCD_UKEY_BL_NAME + 78)
#define LCD_UKEY_BL_PWM2_LEVEL_MIN  (LCD_UKEY_BL_NAME + 80)
/* local dimming (16Byte) */ /* V2 */
#define LCD_UKEY_BL_LDIM_ROW        (LCD_UKEY_BL_NAME + 82)
#define LCD_UKEY_BL_LDIM_COL        (LCD_UKEY_BL_NAME + 83)
#define LCD_UKEY_BL_LDIM_MODE       (LCD_UKEY_BL_NAME + 84)
#define LCD_UKEY_BL_LDIM_DEV_INDEX  (LCD_UKEY_BL_NAME + 85)
#define LCD_UKEY_BL_LDIM_ATTR_4     (LCD_UKEY_BL_NAME + 86)
#define LCD_UKEY_BL_LDIM_ATTR_5     (LCD_UKEY_BL_NAME + 88)
#define LCD_UKEY_BL_LDIM_ATTR_6     (LCD_UKEY_BL_NAME + 90)
#define LCD_UKEY_BL_LDIM_ATTR_7     (LCD_UKEY_BL_NAME + 92)
#define LCD_UKEY_BL_LDIM_ATTR_8     (LCD_UKEY_BL_NAME + 94)
#define LCD_UKEY_BL_LDIM_ATTR_9     (LCD_UKEY_BL_NAME + 96)
/* customer(10Byte) */ /* V2 */
#define LCD_UKEY_BL_CUST_VAL_0      (LCD_UKEY_BL_NAME + 98)
#define LCD_UKEY_BL_CUST_VAL_1      (LCD_UKEY_BL_NAME + 100)
#define LCD_UKEY_BL_CUST_VAL_2      (LCD_UKEY_BL_NAME + 102)
#define LCD_UKEY_BL_CUST_VAL_3      (LCD_UKEY_BL_NAME + 104)
#define LCD_UKEY_BL_CUST_VAL_4      (LCD_UKEY_BL_NAME + 106)

/* ********************************
 * local dimming device
 * *********************************
 */
#define LCD_UKEY_LDIM_DEV_SIZE             1876 /*1747 //722 + (1 + 1024)*/

/* header (10Byte) */
/* LCD_UKEY_HEAD_SIZE */
/* basic (30Byte) */
#define LCD_UKEY_LDIM_DEV_NAME             (LCD_UKEY_HEAD_SIZE + 0)
#define LCD_UKEY_LDIM_DEV_BASIC_RESERVED   (LCD_UKEY_LDIM_DEV_NAME + 0)
/* interface (25Byte) */
#define LCD_UKEY_LDIM_DEV_IF_TYPE          (LCD_UKEY_LDIM_DEV_NAME + 34)
#define LCD_UKEY_LDIM_DEV_IF_FREQ          (LCD_UKEY_LDIM_DEV_NAME + 35)
#define LCD_UKEY_LDIM_DEV_IF_ATTR_0        (LCD_UKEY_LDIM_DEV_NAME + 39)
#define LCD_UKEY_LDIM_DEV_IF_ATTR_1        (LCD_UKEY_LDIM_DEV_NAME + 41)
#define LCD_UKEY_LDIM_DEV_IF_ATTR_2        (LCD_UKEY_LDIM_DEV_NAME + 43)
#define LCD_UKEY_LDIM_DEV_IF_ATTR_3        (LCD_UKEY_LDIM_DEV_NAME + 45)
#define LCD_UKEY_LDIM_DEV_IF_ATTR_4        (LCD_UKEY_LDIM_DEV_NAME + 47)
#define LCD_UKEY_LDIM_DEV_IF_ATTR_5        (LCD_UKEY_LDIM_DEV_NAME + 49)
#define LCD_UKEY_LDIM_DEV_IF_ATTR_6        (LCD_UKEY_LDIM_DEV_NAME + 51)
#define LCD_UKEY_LDIM_DEV_IF_ATTR_7        (LCD_UKEY_LDIM_DEV_NAME + 53)
#define LCD_UKEY_LDIM_DEV_IF_ATTR_8        (LCD_UKEY_LDIM_DEV_NAME + 55)
#define LCD_UKEY_LDIM_DEV_IF_ATTR_9        (LCD_UKEY_LDIM_DEV_NAME + 57)
/* pwm (48Byte) */
#define LCD_UKEY_LDIM_DEV_PWM_VS_PORT      (LCD_UKEY_LDIM_DEV_NAME + 59)
#define LCD_UKEY_LDIM_DEV_PWM_VS_POL       (LCD_UKEY_LDIM_DEV_NAME + 60)
#define LCD_UKEY_LDIM_DEV_PWM_VS_FREQ      (LCD_UKEY_LDIM_DEV_NAME + 61)
#define LCD_UKEY_LDIM_DEV_PWM_VS_DUTY      (LCD_UKEY_LDIM_DEV_NAME + 65)
#define LCD_UKEY_LDIM_DEV_PWM_VS_ATTR_0    (LCD_UKEY_LDIM_DEV_NAME + 67)
#define LCD_UKEY_LDIM_DEV_PWM_VS_ATTR_1    (LCD_UKEY_LDIM_DEV_NAME + 69)
#define LCD_UKEY_LDIM_DEV_PWM_VS_ATTR_2    (LCD_UKEY_LDIM_DEV_NAME + 71)
#define LCD_UKEY_LDIM_DEV_PWM_VS_ATTR_3    (LCD_UKEY_LDIM_DEV_NAME + 73)
#define LCD_UKEY_LDIM_DEV_PWM_HS_PORT      (LCD_UKEY_LDIM_DEV_NAME + 75)
#define LCD_UKEY_LDIM_DEV_PWM_HS_POL       (LCD_UKEY_LDIM_DEV_NAME + 76)
#define LCD_UKEY_LDIM_DEV_PWM_HS_FREQ      (LCD_UKEY_LDIM_DEV_NAME + 77)
#define LCD_UKEY_LDIM_DEV_PWM_HS_DUTY      (LCD_UKEY_LDIM_DEV_NAME + 81)
#define LCD_UKEY_LDIM_DEV_PWM_HS_ATTR_0    (LCD_UKEY_LDIM_DEV_NAME + 83)
#define LCD_UKEY_LDIM_DEV_PWM_HS_ATTR_1    (LCD_UKEY_LDIM_DEV_NAME + 85)
#define LCD_UKEY_LDIM_DEV_PWM_HS_ATTR_2    (LCD_UKEY_LDIM_DEV_NAME + 87)
#define LCD_UKEY_LDIM_DEV_PWM_HS_ATTR_3    (LCD_UKEY_LDIM_DEV_NAME + 89)
#define LCD_UKEY_LDIM_DEV_PWM_ADJ_PORT     (LCD_UKEY_LDIM_DEV_NAME + 91)
#define LCD_UKEY_LDIM_DEV_PWM_ADJ_POL      (LCD_UKEY_LDIM_DEV_NAME + 92)
#define LCD_UKEY_LDIM_DEV_PWM_ADJ_FREQ     (LCD_UKEY_LDIM_DEV_NAME + 93)
#define LCD_UKEY_LDIM_DEV_PWM_ADJ_DUTY     (LCD_UKEY_LDIM_DEV_NAME + 97)
#define LCD_UKEY_LDIM_DEV_PWM_ADJ_ATTR_0   (LCD_UKEY_LDIM_DEV_NAME + 99)
#define LCD_UKEY_LDIM_DEV_PWM_ADJ_ATTR_1   (LCD_UKEY_LDIM_DEV_NAME + 101)
#define LCD_UKEY_LDIM_DEV_PWM_ADJ_ATTR_2   (LCD_UKEY_LDIM_DEV_NAME + 103)
#define LCD_UKEY_LDIM_DEV_PWM_ADJ_ATTR_3   (LCD_UKEY_LDIM_DEV_NAME + 105)
#define LCD_UKEY_LDIM_DEV_PINMUX_SEL       (LCD_UKEY_LDIM_DEV_NAME + 107)//30
/* ctrl (271Byte) */
#define LCD_UKEY_LDIM_DEV_EN_GPIO          (LCD_UKEY_LDIM_DEV_NAME + 137)
#define LCD_UKEY_LDIM_DEV_EN_GPIO_ON       (LCD_UKEY_LDIM_DEV_NAME + 138)
#define LCD_UKEY_LDIM_DEV_EN_GPIO_OFF      (LCD_UKEY_LDIM_DEV_NAME + 139)
#define LCD_UKEY_LDIM_DEV_ON_DELAY         (LCD_UKEY_LDIM_DEV_NAME + 140)
#define LCD_UKEY_LDIM_DEV_OFF_DELAY        (LCD_UKEY_LDIM_DEV_NAME + 142)
#define LCD_UKEY_LDIM_DEV_ERR_GPIO         (LCD_UKEY_LDIM_DEV_NAME + 144)
#define LCD_UKEY_LDIM_DEV_WRITE_CHECK      (LCD_UKEY_LDIM_DEV_NAME + 145)
#define LCD_UKEY_LDIM_DEV_DIM_MAX          (LCD_UKEY_LDIM_DEV_NAME + 146)
#define LCD_UKEY_LDIM_DEV_DIM_MIN          (LCD_UKEY_LDIM_DEV_NAME + 148)
#define LCD_UKEY_LDIM_DEV_CHIP_COUNT       (LCD_UKEY_LDIM_DEV_NAME + 150)
#define LCD_UKEY_LDIM_DEV_ZONE_MAP_PATH    (LCD_UKEY_LDIM_DEV_NAME + 152)//256
/* profile (273Byte) */
#define LCD_UKEY_LDIM_DEV_PROFILE_MODE     (LCD_UKEY_LDIM_DEV_NAME + 408)//1
#define LCD_UKEY_LDIM_DEV_PROFILE_PATH     (LCD_UKEY_LDIM_DEV_NAME + 409)//256
#define LCD_UKEY_LDIM_DEV_PROFILE_ATTR_0   (LCD_UKEY_LDIM_DEV_NAME + 665)
#define LCD_UKEY_LDIM_DEV_PROFILE_ATTR_1   (LCD_UKEY_LDIM_DEV_NAME + 667)
#define LCD_UKEY_LDIM_DEV_PROFILE_ATTR_2   (LCD_UKEY_LDIM_DEV_NAME + 669)
#define LCD_UKEY_LDIM_DEV_PROFILE_ATTR_3   (LCD_UKEY_LDIM_DEV_NAME + 671)
#define LCD_UKEY_LDIM_DEV_PROFILE_ATTR_4   (LCD_UKEY_LDIM_DEV_NAME + 673)
#define LCD_UKEY_LDIM_DEV_PROFILE_ATTR_5   (LCD_UKEY_LDIM_DEV_NAME + 675)
#define LCD_UKEY_LDIM_DEV_PROFILE_ATTR_6   (LCD_UKEY_LDIM_DEV_NAME + 677)
#define LCD_UKEY_LDIM_DEV_PROFILE_ATTR_7   (LCD_UKEY_LDIM_DEV_NAME + 679)
/* custom (40Byte + 129Byte) */
#define LCD_UKEY_LDIM_DEV_CUST_ATTR_0      (LCD_UKEY_LDIM_DEV_NAME + 681)
#define LCD_UKEY_LDIM_DEV_CUST_ATTR_1      (LCD_UKEY_LDIM_DEV_NAME + 685)
#define LCD_UKEY_LDIM_DEV_CUST_ATTR_2      (LCD_UKEY_LDIM_DEV_NAME + 689)
#define LCD_UKEY_LDIM_DEV_CUST_ATTR_3      (LCD_UKEY_LDIM_DEV_NAME + 693)
#define LCD_UKEY_LDIM_DEV_CUST_ATTR_4      (LCD_UKEY_LDIM_DEV_NAME + 697)
#define LCD_UKEY_LDIM_DEV_CUST_ATTR_5      (LCD_UKEY_LDIM_DEV_NAME + 701)
#define LCD_UKEY_LDIM_DEV_CUST_ATTR_6      (LCD_UKEY_LDIM_DEV_NAME + 705)
#define LCD_UKEY_LDIM_DEV_CUST_ATTR_7      (LCD_UKEY_LDIM_DEV_NAME + 709)
#define LCD_UKEY_LDIM_DEV_CUST_ATTR_8      (LCD_UKEY_LDIM_DEV_NAME + 713)
#define LCD_UKEY_LDIM_DEV_CUST_ATTR_9      (LCD_UKEY_LDIM_DEV_NAME + 717)
/* 1 + 128 */
#define LCD_UKEY_LDIM_DEV_CUST_PARAM_SIZE	(LCD_UKEY_LDIM_DEV_NAME + 721)
#define LCD_UKEY_LDIM_DEV_CUST_PARAM		(LCD_UKEY_LDIM_DEV_NAME + 722)

/* init (dynamic) */
#define LCD_UKEY_LDIM_DEV_CMD_SIZE         (LCD_UKEY_LDIM_DEV_NAME + 850)
#define LCD_UKEY_LDIM_DEV_INIT             (LCD_UKEY_LDIM_DEV_NAME + 851)
/*#define LCD_UKEY_LDIM_DEV_INIT_TYPE      (0)*/
/*#define LCD_UKEY_LDIM_DEV_INIT_VAL        1   //not defined */
/*#define LCD_UKEY_LDIM_DEV_INIT_DELAY     (n)*/

/* ********************************
 * lcd optical
 * *********************************
 */
#define LCD_UKEY_OPTICAL_SIZE              102 /*10 + 52 + 40*/

/* header (10Byte) */
/* LCD_UKEY_HEAD_SIZE */
/* attr (52Byte) */
#define LCD_UKEY_OPT_HDR_SUPPORT           (LCD_UKEY_HEAD_SIZE + 0)
#define LCD_UKEY_OPT_FEATURES              (LCD_UKEY_OPT_HDR_SUPPORT + 4)
#define LCD_UKEY_OPT_PRI_R_X               (LCD_UKEY_OPT_HDR_SUPPORT + 8)
#define LCD_UKEY_OPT_PRI_R_Y               (LCD_UKEY_OPT_HDR_SUPPORT + 12)
#define LCD_UKEY_OPT_PRI_G_X               (LCD_UKEY_OPT_HDR_SUPPORT + 16)
#define LCD_UKEY_OPT_PRI_G_Y               (LCD_UKEY_OPT_HDR_SUPPORT + 20)
#define LCD_UKEY_OPT_PRI_B_X               (LCD_UKEY_OPT_HDR_SUPPORT + 24)
#define LCD_UKEY_OPT_PRI_B_Y               (LCD_UKEY_OPT_HDR_SUPPORT + 28)
#define LCD_UKEY_OPT_WHITE_X               (LCD_UKEY_OPT_HDR_SUPPORT + 32)
#define LCD_UKEY_OPT_WHITE_Y               (LCD_UKEY_OPT_HDR_SUPPORT + 36)
#define LCD_UKEY_OPT_LUMA_MAX              (LCD_UKEY_OPT_HDR_SUPPORT + 40)
#define LCD_UKEY_OPT_LUMA_MIN              (LCD_UKEY_OPT_HDR_SUPPORT + 44)
#define LCD_UKEY_OPT_LUMA_AVG              (LCD_UKEY_OPT_HDR_SUPPORT + 48)
/* adv (40Byte) */
#define LCD_UKEY_OPT_ADV_FLAG0             (LCD_UKEY_OPT_HDR_SUPPORT + 52)
#define LCD_UKEY_OPT_ADV_FLAG1             (LCD_UKEY_OPT_HDR_SUPPORT + 53)
#define LCD_UKEY_OPT_ADV_FLAG2             (LCD_UKEY_OPT_HDR_SUPPORT + 54)
#define LCD_UKEY_OPT_ADV_FLAG3             (LCD_UKEY_OPT_HDR_SUPPORT + 55)
#define LCD_UKEY_OPT_ADV_VAL1              (LCD_UKEY_OPT_HDR_SUPPORT + 56)
#define LCD_UKEY_OPT_ADV_VAL2              (LCD_UKEY_OPT_HDR_SUPPORT + 60)
#define LCD_UKEY_OPT_ADV_VAL3              (LCD_UKEY_OPT_HDR_SUPPORT + 64)
#define LCD_UKEY_OPT_ADV_VAL4              (LCD_UKEY_OPT_HDR_SUPPORT + 68)
#define LCD_UKEY_OPT_ADV_VAL5              (LCD_UKEY_OPT_HDR_SUPPORT + 72)
#define LCD_UKEY_OPT_ADV_VAL6              (LCD_UKEY_OPT_HDR_SUPPORT + 76)
#define LCD_UKEY_OPT_ADV_VAL7              (LCD_UKEY_OPT_HDR_SUPPORT + 80)
#define LCD_UKEY_OPT_ADV_VAL8              (LCD_UKEY_OPT_HDR_SUPPORT + 84)
#define LCD_UKEY_OPT_ADV_VAL9              (LCD_UKEY_OPT_HDR_SUPPORT + 88)

/* ********************************
 * API
 * *********************************
 */
bool lcd_unifykey_init_get(void);
int lcd_unifykey_len_check(int key_len, int len);
int lcd_unifykey_check(char *key_name);
int lcd_unifykey_header_check(unsigned char *buf,
			      struct aml_lcd_unifykey_header_s *header);
int lcd_unifykey_get(char *key_name, unsigned char *buf, int *len);
int lcd_unifykey_get_tcon(char *key_name, unsigned char *buf, int *len);
int lcd_unifykey_check_no_header(char *key_name);
int lcd_unifykey_get_no_header(char *key_name, unsigned char *buf, int *len);
void lcd_unifykey_print(void);

#endif
