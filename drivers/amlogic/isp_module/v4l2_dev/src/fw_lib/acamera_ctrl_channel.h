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

#ifndef ACAMERA_CTRL_CHANNEL_H
#define ACAMERA_CTRL_CHANNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "acamera_types.h"

#define CTRL_CHANNEL_DEV_NAME "ac_isp4uf"
#define CTRL_CHANNEL_DEV_NODE_NAME "/dev/" CTRL_CHANNEL_DEV_NAME
#define CTRL_CHANNEL_MAX_CMD_SIZE ( 8 * 1024 )


enum ctrl_cmd_category {
    CTRL_CMD_CATEGORY_API_COMMAND = 1,
    CTRL_CMD_CATEGORY_API_CALIBRATION,
};

struct ctrl_cmd_item {
    /* command metadata */
    uint32_t cmd_len;
    uint8_t cmd_category;

    /* command content */
    uint8_t cmd_type;
    uint8_t cmd_id;
    uint8_t cmd_direction;
    uint32_t cmd_value;
};

int ctrl_channel_init( void );
void ctrl_channel_process( void );
void ctrl_channel_deinit( void );

void ctrl_channel_handle_command( uint8_t command_type, uint8_t command, uint32_t value, uint8_t direction );
void ctrl_channel_handle_api_calibration( uint8_t type, uint8_t id, uint8_t direction, void *data, uint32_t data_size );

#ifdef __cplusplus
}
#endif

#endif /* ACAMERA_CTRL_CHANNEL_H */
