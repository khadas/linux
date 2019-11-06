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

#ifndef __ACAMERA_SBUS_H__
#define __ACAMERA_SBUS_H__


#include "acamera_types.h"

typedef struct _acamera_sbus_t acamera_sbus_t;
typedef struct _acamera_sbus_t *acamera_sbus_ptr_t;
typedef const struct _acamera_sbus_t *acamera_sbus_const_ptr_t;

#define SBUS_MASK_SAMPLE_8BITS 0x01
#define SBUS_MASK_SAMPLE_16BITS 0x02
#define SBUS_MASK_SAMPLE_32BITS 0x04
#define SBUS_MASK_ADDR_8BITS 0x08
#define SBUS_MASK_ADDR_16BITS 0x10
#define SBUS_MASK_ADDR_24BITS 0x30
#define SBUS_MASK_ADDR_32BITS 0x20
#define SBUS_MASK_ADDR_BITS_MASK 0x38
#define SBUS_MASK_ADDR_STEP_16BITS 0x40
#define SBUS_MASK_ADDR_STEP_32BITS 0x80
#define SBUS_MASK_SAMPLE_SWAP_BYTES 0x100
#define SBUS_MASK_SAMPLE_SWAP_WORDS 0x200
#define SBUS_MASK_ADDR_SWAP_BYTES 0x400
#define SBUS_MASK_ADDR_SWAP_WORDS 0x800
#define SBUS_MASK_ADDR_SKIP 0x1000
#define SBUS_MASK_SPI_READ_MSB_SET 0x2000
#define SBUS_MASK_SPI_INVERSE_DATA 0x4000
#define SBUS_MASK_SPI_HALF_ADDR 0x8000
#define SBUS_MASK_SPI_LSB 0x10000
#define SBUS_MASK_NO_STOP 0x20000

typedef enum _sbus_type_t {
    sbus_i2c = 0,
    sbus_spi,
    sbus_isp,
    sbus_isp_sw
} sbus_type_t;

struct _acamera_sbus_t {
    uint32_t mask;
    uint32_t bus;
    uint8_t device;
    uint8_t device_id;      // If device is used for SPI CS, and device number is still required
    uint8_t device_id_read; // If device is used for SPI CS, and device number is still required
    uint32_t control;
    void *p_control;
    uint32_t ( *read_sample )( acamera_sbus_ptr_t p_bus, uintptr_t addr, uint8_t sample_size );
    void ( *write_sample )( acamera_sbus_ptr_t p_bus, uintptr_t addr, uint32_t sample, uint8_t sample_size );
};


uint8_t acamera_sbus_read_u8( acamera_sbus_ptr_t p_bus, uintptr_t addr );
uint16_t acamera_sbus_read_u16( acamera_sbus_ptr_t p_bus, uintptr_t addr );
uint32_t acamera_sbus_read_u32( acamera_sbus_ptr_t p_bus, uintptr_t addr );
void acamera_sbus_read_data_u8( acamera_sbus_ptr_t p_bus, uintptr_t addr, uint8_t *p_data, int n_count );
void acamera_sbus_read_data_u16( acamera_sbus_ptr_t p_bus, uintptr_t addr, uint16_t *p_data, int n_count );
void acamera_sbus_read_data_u32( acamera_sbus_ptr_t p_bus, uintptr_t addr, uint32_t *p_data, int n_count );
void acamera_sbus_write_u8( acamera_sbus_ptr_t p_bus, uintptr_t addr, uint8_t sample );
void acamera_sbus_write_u16( acamera_sbus_ptr_t p_bus, uintptr_t addr, uint16_t sample );
void acamera_sbus_write_u32( acamera_sbus_ptr_t p_bus, uintptr_t addr, uint32_t sample );
void acamera_sbus_write_data_u8( acamera_sbus_ptr_t p_bus, uintptr_t addr, uint8_t *p_data, int n_count );
void acamera_sbus_write_data_u16( acamera_sbus_ptr_t p_bus, uintptr_t addr, uint16_t *p_data, int n_count );
void acamera_sbus_write_data_u32( acamera_sbus_ptr_t p_bus, uintptr_t addr, uint32_t *p_data, int n_count );
void acamera_sbus_write_data( acamera_sbus_ptr_t p_bus, uintptr_t addr, void *p_data, int n_size );
void acamera_sbus_copy( acamera_sbus_t *p_bus_to, uintptr_t addr_to, acamera_sbus_t *p_bus_from, uint32_t addr_from, int n_size );

void acamera_sbus_init( acamera_sbus_t *p_bus, sbus_type_t interface_type );
void acamera_sbus_deinit( acamera_sbus_t *p_bus, sbus_type_t interface_type );
void acamera_sbus_reset( sbus_type_t interface_type );


#endif /* __ACAMERA_SBUS_H__ */
