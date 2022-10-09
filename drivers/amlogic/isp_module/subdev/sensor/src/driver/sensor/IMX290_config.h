/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2011-2018 ARM or its affiliates
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; version 2.
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#ifndef __IMX290_CONFIG_H__
#define __IMX290_CONFIG_H__


#define FULL_EXTRA_HEIGHT 0
#define FULL_EXTRA_WIDTH 0
#define ISP_IMAGE_HEIGHT SENSOR_IMAGE_HEIGHT
#define ISP_IMAGE_WIDTH SENSOR_IMAGE_WIDTH
#define LOG2_SENSOR_AGAIN_MAXIMUM 0
#define LOG2_SENSOR_DGAIN_MAXIMUM 0
#define PREVIEW_EXTRA_HEIGHT 0
#define PREVIEW_EXTRA_WIDTH 0
#define RESOLUTION_CHANGE_ENABLED 0
#define SENSOR_AF_MOVE_DELAY 20
#define SENSOR_ANALOG_GAIN_APPLY_DELAY 2
#define SENSOR_BLACK_LEVEL_CORRECTION 0
#define SENSOR_BOARD_MASTER_CLOCK 24000
#define SENSOR_BUS spi
#define SENSOR_BUS_ADDR_SHIFT 0
#define SENSOR_CHIP_ID 0xB201
#define SENSOR_DAY_LIGHT_INTEGRATION_TIME_LIMIT 300
#define SENSOR_DEV_ADDRESS 0x6c
#define SENSOR_DIGITAL_GAIN_APPLY_DELAY 0
#define SENSOR_ENDIAN_MASK 1
#define SENSOR_EXP_NUMBER 3
#define SENSOR_IMAGE_HEIGHT 1080
#define SENSOR_IMAGE_HEIGHT_FULL 2448
#define SENSOR_IMAGE_HEIGHT_PREVIEW 1080
#define SENSOR_IMAGE_WIDTH 1920
#define SENSOR_IMAGE_WIDTH_FULL 3264
#define SENSOR_IMAGE_WIDTH_PREVIEW 1920
#define SENSOR_INTEGRATION_TIME_APPLY_DELAY 2
#define SENSOR_MAX_INTEGRATION_TIME 0
#define SENSOR_MAX_INTEGRATION_TIME_LIMIT 1100
#define SENSOR_MAX_INTEGRATION_TIME_NATIVE SENSOR_MAX_INTEGRATION_TIME
#define SENSOR_MAX_INTEGRATION_TIME_PREVIEW 2586 - 14
#define SENSOR_MIN_INTEGRATION_TIME 1
#define SENSOR_MIN_INTEGRATION_TIME_NATIVE SENSOR_MIN_INTEGRATION_TIME
#define SENSOR_OUTPUT_BITS 10
#define SENSOR_SAMPLE_MASK 1
#define SENSOR_SEQUENCE_FULL_RES_HALF_FPS 2
#define SENSOR_SEQUENCE_FULL_RES_MAX_FPS 0
#define SENSOR_SEQUENCE_NAME default
#define SENSOR_SEQUENCE_PREVIEW_RES_HALF_FPS 3
#define SENSOR_SEQUENCE_PREVIEW_RES_MAX_FPS 1
#define SENSOR_TOTAL_HEIGHT 1109
#define SENSOR_TOTAL_HEIGHT_PREVIEW 2586
#define SENSOR_TOTAL_WIDTH 7780
#define SENSOR_TOTAL_WIDTH_PREVIEW 3264
#define SPI_CLOCK_DIV 40
#define SPI_CONTROL_MASK ( RX_NEG_MSK | ( CHAR_LEN_MSK & 24 ) | AUTO_SS_MSK | LSB_MSK )

typedef struct _imx290_private_t {
    uint32_t shs1;
    uint32_t shs2;
    uint32_t shs3;
    uint32_t shs1_old;
    uint32_t shs2_old;
    uint32_t rhs1;
    uint32_t rhs2;
} imx290_private_t;

#endif
