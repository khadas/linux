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

#ifndef __SYSTEM_AUTOWRITE_H__
#define __SYSTEM_AUTOWRITE_H__

#include "acamera_types.h"


#define FR_AUTO_WRITE_BASE  0xFE023C00
#define DS1_AUTO_WRITE_BASE 0xFE023C80
#define SC0_AUTO_WRITE_BASE 0xFE023D00

#define DS1_OFFSET 0x80
#define SC0_OFFSET 0x100

#define MIPI_BL_PING_ADDR0_ST     0x00
#define MIPI_BL_PING_ADDR0_ED     0x04
#define MIPI_BL_PING_ADDR1_ST     0x08
#define MIPI_BL_PING_ADDR1_ED     0x0C
#define MIPI_BL_PONG_ADDR0_ST     0x10
#define MIPI_BL_PONG_ADDR0_ED     0x14
#define MIPI_BL_PONG_ADDR1_ST     0x18
#define MIPI_BL_PONG_ADDR1_ED     0x1C
#define MIPI_BL_FRAME_TH          0x20
#define MIPI_BL_FRAME_BUFFER_START_ADDR0    0x24
#define MIPI_BL_FRAME_SIZE0       0x28
#define MIPI_BL_FRAME_BUF_SIZE0   0x2c
#define MIPI_BL_FRAME_BUFFER_START_ADDR1    0x30
#define MIPI_BL_FRAME_SIZE1       0x34
#define MIPI_BL_FRAME_BUF_SIZE1   0x38
#define MIPI_BL_CTRL0             0x3c
#define MIPI_BL_CTRL1             0x40
#define MIPI_BL_COUNT             0x44
#define MIPI_BL_AWADDR_0          0x48
#define MIPI_BL_AWADDR_I          0x4c
#define MIPI_BL_STAT0             0x50
#define MIPI_BL_STAT1             0x54
#define MIPI_BL_STAT2             0x58
#define MIPI_BL_STAT3             0x5c
#define MIPI_BL_FRAME_BUFFER_START_ADDR0P   0x60

typedef struct _auto_write_cfg_t {
	uint32_t p_type;
	uint32_t planes;
	uint32_t ping_st0;
	uint32_t ping_ed0;
	uint32_t pong_st0;
	uint32_t pong_ed0;
	uint32_t ping_st1;
	uint32_t ping_ed1;
	uint32_t pong_st1;
	uint32_t pong_ed1;
	uint32_t frame_buffer_start0;
	uint32_t frame_size0;
	uint32_t frame_buffer_size0;
	uint32_t frame_buffer_start1;
	uint32_t frame_size1;
	uint32_t frame_buffer_size1;
	uint32_t wreay_delay;
	uint32_t bvalid_delay;
	uint32_t drop_jump;
	uint32_t drop_write;
	uint32_t drop_skip;
	uint32_t drop_enable;
	uint32_t plane0_size;
	uint32_t plane1_size;
	uint32_t cap_cnt;
	uint32_t th_enable;
	uint32_t th_cnt;
}auto_write_cfg_t;

void auto_cap_init(void);
void auto_cap_deinit(void);
void auto_write_fr_init(void *cfg);
void auto_write_ds1_init(void *cfg);
void auto_write_sc0_init(void *cfg);
void auto_write_stop(void *cfg);
void auto_write_cfg_ping_pong(void *cfg);

uint32_t autowrite_fr_ping_start( void );
uint32_t autowrite_fr_pong_start( void );
uint32_t autowrite_fr_image_buffer_stride_read( void );
uint32_t autowrite_fr_image_buffer_stride_read1( void );
uint32_t autowrite_fr_writer_frame_wcount_read( void );
uint32_t autowrite_fr_writer_memsize_read( void );
uint32_t autowrite_fr_writer_memsize_read1( void );
uint32_t autowrite_fr_drop_jump_read( void );
uint32_t autowrite_fr_drop_write_read( void );
uint32_t autowrite_fr_drop_mode_status( void );

uint32_t autowrite_ds1_ping_start( void );
uint32_t autowrite_ds1_pong_start( void );
uint32_t autowrite_ds1_image_buffer_stride_read( void );
uint32_t autowrite_ds1_image_buffer_stride_read1( void );
uint32_t autowrite_ds1_writer_frame_wcount_read( void );
uint32_t autowrite_ds1_writer_memsize_read( void );
uint32_t autowrite_ds1_writer_memsize_read1( void );
uint32_t autowrite_ds1_drop_jump_read( void );
uint32_t autowrite_ds1_drop_write_read( void );
uint32_t autowrite_ds1_drop_mode_status( void );

uint32_t autowrite_sc0_ping_start( void );
uint32_t autowrite_sc0_pong_start( void );
uint32_t autowrite_sc0_image_buffer_stride_read( void );
uint32_t autowrite_sc0_image_buffer_stride_read1( void );
uint32_t autowrite_sc0_writer_frame_wcount_read( void );
uint32_t autowrite_sc0_writer_memsize_read( void );
uint32_t autowrite_sc0_writer_memsize_read1( void );
uint32_t autowrite_sc0_drop_jump_read( void );
uint32_t autowrite_sc0_drop_write_read( void );
uint32_t autowrite_sc0_drop_mode_status( void );

uint32_t autowrite_fr_start_address_read( void );
uint32_t autowrite_ds1_start_address_read( void );
uint32_t autowrite_sc0_start_address_read( void );

void autowrite_path_reset(void);

void auto_write_fr_start(void *cfg);
void auto_write_ds1_start(void *cfg);
void auto_write_sc0_start(void *cfg);

uint32_t autowrite_fr_stop( void );
uint32_t autowrite_ds1_stop( void );
uint32_t autowrite_sc0_stop( void );

#endif // __SYSTEM_UART_H__
