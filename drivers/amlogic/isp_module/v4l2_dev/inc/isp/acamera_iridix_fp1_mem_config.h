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

#ifndef __ACAMERA_IRIDIX_FP1_MEM_CONFIG_H__
#define __ACAMERA_IRIDIX_FP1_MEM_CONFIG_H__


#include "system_sw_io.h"

#include "system_hw_io.h"

// ------------------------------------------------------------------------------ //
// Instance 'iridix_fp1_ping_mem' of module 'iridix_fp1_ping_mem'
// ------------------------------------------------------------------------------ //

#define ACAMERA_IRIDIX_FP1_MEM_BASE_ADDR (0x1807cL)
#define ACAMERA_IRIDIX_FP1_MEM_SIZE (0x400)

#define ACAMERA_IRIDIX_FP1_MEM_ARRAY_DATA_DEFAULT (0x0)
#define ACAMERA_IRIDIX_FP1_MEM_ARRAY_DATA_DATASIZE (32)
#define ACAMERA_IRIDIX_FP1_MEM_ARRAY_DATA_OFFSET (0x1807cL)

// args: index (0-128), data (32-bit)
static __inline void acamera_iridix_fp1_mem_array_data_write( uintptr_t base, uint32_t index, uint32_t data) {
    system_sw_write_32(base + 0x1807cL + (index << 2), data);
}
static __inline uint32_t acamera_iridix_fp1_mem_array_data_read( uintptr_t base, uint32_t index) {
    return system_sw_read_32(base + 0x1807cL + (index << 2));
}
// ------------------------------------------------------------------------------ //
#endif //__ACAMERA_IRIDIX_FP1_MEM_CONFIG_H__
