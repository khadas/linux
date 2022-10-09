/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _I2C_OLED_H_
#define _I2C_OLED_H_

struct ext_oled_orbit_s {
	unsigned char en;
	unsigned char mode_en;
	unsigned char mode_set;
	unsigned char distance_en;
	unsigned char x_axis;
	unsigned char y_axis;
};

#define EXT_OLED_STATE_ON           BIT(0)
#define EXT_OLED_STATE_OFF_RS_EN    BIT(1)
#define EXT_OLED_STATE_JB_EN        BIT(2)
#define EXT_OLED_STATE_ORBIT_EN     BIT(3)
#define EXT_OLED_STATE_HDR_EN       BIT(4)

struct lcd_ext_oled_s {
	int dev_index;
	unsigned long long power_on_time;

	struct lcd_extern_dev_s *ext_dev;
	struct ext_oled_orbit_s orbit_param;
	struct device *dev;
	dev_t devno;
	struct class *clsp;
	struct cdev cdev;
};

/* **********************************
 * IOCTL define
 * **********************************
 */
#define EXT_OLED_IOC_TYPE               'E'
#define EXT_OLED_IOC_GET_OP_TIME       0x0
#define EXT_OLED_IOC_GET_TEMP          0x1
#define EXT_OLED_IOC_GET_OFF_RS        0x2
#define EXT_OLED_IOC_SET_OFF_RS        0x3
#define EXT_OLED_IOC_GET_JB            0x4
#define EXT_OLED_IOC_SET_JB            0x5
#define EXT_OLED_IOC_GET_ORBIT         0x6
#define EXT_OLED_IOC_SET_ORBIT         0x7
#define EXT_OLED_IOC_GET_HDR           0x8
#define EXT_OLED_IOC_SET_HDR           0x9

#define EXT_OLED_IOC_CMD_GET_OP_TIME   \
	_IOR(EXT_OLED_IOC_TYPE, EXT_OLED_IOC_GET_OP_TIME, unsigned int)
#define EXT_OLED_IOC_CMD_GET_TEMP   \
	_IOR(EXT_OLED_IOC_TYPE, EXT_OLED_IOC_GET_TEMP, unsigned int)
#define EXT_OLED_IOC_CMD_GET_OFF_RS   \
	_IOR(EXT_OLED_IOC_TYPE, EXT_OLED_IOC_GET_OFF_RS, unsigned int)
#define EXT_OLED_IOC_CMD_SET_OFF_RS   \
	_IOW(EXT_OLED_IOC_TYPE, EXT_OLED_IOC_SET_OFF_RS, unsigned int)
#define EXT_OLED_IOC_CMD_GET_JB   \
	_IOR(EXT_OLED_IOC_TYPE, EXT_OLED_IOC_GET_JB, unsigned int)
#define EXT_OLED_IOC_CMD_SET_JB   \
	_IOW(EXT_OLED_IOC_TYPE, EXT_OLED_IOC_SET_JB, unsigned int)
#define EXT_OLED_IOC_CMD_GET_ORBIT   \
	_IOR(EXT_OLED_IOC_TYPE, EXT_OLED_IOC_GET_ORBIT, struct ext_oled_orbit_s)
#define EXT_OLED_IOC_CMD_SET_ORBIT   \
	_IOW(EXT_OLED_IOC_TYPE, EXT_OLED_IOC_SET_ORBIT, struct ext_oled_orbit_s)
#define EXT_OLED_IOC_CMD_GET_HDR   \
	_IOR(EXT_OLED_IOC_TYPE, EXT_OLED_IOC_GET_HDR, unsigned int)
#define EXT_OLED_IOC_CMD_SET_HDR   \
	_IOW(EXT_OLED_IOC_TYPE, EXT_OLED_IOC_SET_HDR, unsigned int)

#endif

