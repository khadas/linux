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

#ifndef __ACAMERA_DS1_SCALER_VFILT_COEFMEM_CONFIG_H__
#define __ACAMERA_DS1_SCALER_VFILT_COEFMEM_CONFIG_H__


#include "system_sw_io.h"

#include "system_hw_io.h"

// ------------------------------------------------------------------------------ //
// Instance 'ds_scaler_vfilt_coefmem' of module 'ds_scaler_vfilt_coefmem'
// ------------------------------------------------------------------------------ //

#define ACAMERA_DS1_SCALER_VFILT_COEFMEM_BASE_ADDR (0x1ca8L)
#define ACAMERA_DS1_SCALER_VFILT_COEFMEM_SIZE (0x800)

#define ACAMERA_DS1_SCALER_VFILT_COEFMEM_ARRAY_DATA_DEFAULT (0x0)
#define ACAMERA_DS1_SCALER_VFILT_COEFMEM_ARRAY_DATA_DATASIZE (32)
#define ACAMERA_DS1_SCALER_VFILT_COEFMEM_ARRAY_DATA_OFFSET (0x1ca8L)

// args: index (0-511), data (32-bit)
static __inline void acamera_ds1_scaler_vfilt_coefmem_array_data_write( uintptr_t base, uint32_t index, uint32_t data) {
    system_hw_write_32(0x1ca8L + (index << 2), data);
}
static __inline uint32_t acamera_ds1_scaler_vfilt_coefmem_array_data_read( uintptr_t base, uint32_t index) {
    return system_hw_read_32(0x1ca8L + (index << 2));
}
// ------------------------------------------------------------------------------ //
#endif //__ACAMERA_DS1_SCALER_VFILT_COEFMEM_CONFIG_H__
