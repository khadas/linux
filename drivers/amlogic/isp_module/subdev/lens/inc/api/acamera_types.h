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

#ifndef __ACAMERA__TYPES_H__
#define __ACAMERA__TYPES_H__

#include "acamera_firmware_config.h"

#if KERNEL_MODULE == 1
#include "linux/types.h"
#else
#include <stdint.h>
#endif

typedef enum dma_buf_state {
    dma_buf_empty = 0,
    dma_buf_busy,
    dma_buf_ready,
    dma_buf_purge
} dma_buf_state;

/**
 *   supported pipe types
 *   please note that dma_max is used
 *   only as a counter for supported pipes
 */
typedef enum dma_type {
    dma_fr = 0,
    dma_ds1,
    dma_ds2,
    dma_max
} dma_type;

typedef struct LookupTable {
    void *ptr;
    uint16_t rows;
    uint16_t cols;
    uint16_t width;
} LookupTable;

#endif /* __ACAMERA__TYPES_H__ */
