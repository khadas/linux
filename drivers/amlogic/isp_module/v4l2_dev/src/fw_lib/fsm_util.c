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

#include "acamera_firmware_config.h"
#include "acamera_fw.h"

uint32_t acamera_fsm_util_is_irq_event_ignored( fsm_irq_mask_t *p_mask, uint8_t irq_event )
{
    uint32_t ignore_irq = 1;
    uint32_t fsm_mask;
    uint32_t mask = 1 << irq_event;
    fsm_mask = p_mask->irq_mask & mask;
    if ( fsm_mask ) {
        p_mask->irq_mask &= ( ~fsm_mask ) | ( p_mask->repeat_irq_mask );
        ignore_irq = 0;
    }

    return ignore_irq;
}

uint32_t acamera_fsm_util_get_cur_frame_id( fsm_common_t *p_cmn )
{
    uint32_t cur_frame_id = 0;
    cur_frame_id = acamera_isp_isp_global_dbg_frame_cnt_ctx0_read( p_cmn->isp_base );
    return cur_frame_id;
}
