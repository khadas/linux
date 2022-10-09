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

#ifndef __DMA_WRITER__
#define __DMA_WRITER__

#include "acamera_types.h"
#include "dma_writer_api.h"
#include "acamera_fw.h"


typedef struct _dma_pipe_state {

    uint16_t buf_num_rdi;
    uint16_t buf_num_wri;
    uint16_t buf_num_size;

    uint32_t cleared_bufs;
    uint8_t initialized;
} dma_pipe_state;


typedef struct _dma_pipe {
    dma_pipe_settings settings;
    dma_pipe_state state;
    dma_api api;
    dma_type type;
} dma_pipe;


typedef struct _dma_handle {
    dma_pipe pipe[dma_max];
} dma_handle;


/**
 *   Get default settings for dis processor.
 *
 *   To be sure that all settings are correct you have to call this function before changing any parameters.
 *   It sets all settings in their default value. After that it can be changed by caller.
 *
 *   @param settings - pointer to the settings structure.
 *
 *   @return 0 - success
 *           1 - fail to set settings.
 */
dma_error dma_writer_update_state( dma_pipe *pipe );


#endif
