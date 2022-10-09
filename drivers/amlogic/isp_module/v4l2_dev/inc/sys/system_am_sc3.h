/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2018 Amlogic or its affiliates
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

#ifndef __SYSTEM_AM_SC3_H__
#define __SYSTEM_AM_SC3_H__

#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include "acamera_command_api.h"
#include "acamera_firmware_settings.h"
#include "acamera.h"
#include "acamera_fw.h"
#include <linux/kfifo.h>
#include <linux/interrupt.h>
#include "system_am.h"

extern int am_sc3_parse_dt(struct device_node *node, int port);
extern void am_sc3_deinit_parse_dt(void);
extern void am_sc3_api_dma_buffer(int ctx_id, tframe_t * data, unsigned int index);
extern uint32_t am_sc3_get_width(int ctx_id);
extern void am_sc3_set_width(int ctx_id, uint32_t src_w, uint32_t out_w);
extern uint32_t am_sc3_get_height(int ctx_id);
extern uint32_t am_sc3_get_output_format(int ctx_id);
extern void am_sc3_set_height(int ctx_id, uint32_t src_h, uint32_t out_h);
extern void am_sc3_set_input_format(int ctx_id, uint32_t value);
extern void am_sc3_set_output_format(int ctx_id, uint32_t value);
extern void am_sc3_set_buf_num(int ctx_id, uint32_t num);
extern int am_sc3_set_callback(acamera_context_ptr_t p_ctx, buffer_callback_t sc3_callback);
extern int am_sc3_system_init(int ctx_id);
extern int am_sc3_hw_init(int ctx_id, int is_print, int clip_mode);
extern int am_sc3_start(int ctx_id);
extern int am_sc3_reset(int ctx_id);
extern int am_sc3_stop(int ctx_id);
extern int am_sc3_system_deinit(int ctx_id);
extern int am_sc3_hw_deinit(int ctx_id);
extern void am_sc3_set_fps(int ctx_id, uint32_t c_fps, uint32_t t_fps);
extern uint32_t am_sc3_get_fps(int ctx_id);
extern uint32_t am_sc3_get_startx(int ctx_id);
extern uint32_t am_sc3_get_starty(int ctx_id);
extern void am_sc3_set_startx(int ctx_id, uint32_t startx);
extern void am_sc3_set_starty(int ctx_id, uint32_t starty);
extern uint32_t am_sc3_get_crop_width(int ctx_id);
extern uint32_t am_sc3_get_crop_height(int ctx_id);
extern void am_sc3_set_crop_width(int ctx_id, uint32_t c_width);
extern void am_sc3_set_crop_height(int ctx_id, uint32_t c_height);
extern void am_sc3_set_src_width(int ctx_id, uint32_t src_w);
extern void am_sc3_set_src_height(int ctx_id, uint32_t src_h);
extern void am_sc3_set_crop_enable(int ctx_id);
extern void am_sc3_hw_enable(void);
extern void am_sc3_hw_disable(void);
extern void am_sc3_set_workstatus(uint32_t status);
extern uint32_t am_sc3_get_workstatus(void);
extern void am_sc3_set_camid(uint32_t status);
extern void am_sc3_reset_hwstatus(uint32_t status);
extern void am_sc3_dcam(uint32_t status);

#endif

