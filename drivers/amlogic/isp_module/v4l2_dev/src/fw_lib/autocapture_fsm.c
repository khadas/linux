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

#include "acamera_fw.h"
#include "autocapture_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_AUTOCAPTURE
#endif

void autocapture_fsm_clear( autocapture_fsm_t *p_fsm )
{

}

void autocapture_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    autocapture_fsm_t *p_fsm = (autocapture_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    autocapture_fsm_clear( p_fsm );

    autocapture_initialize( p_fsm );
}


int autocapture_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
	
    //autocapture_fsm_t *p_fsm = (autocapture_fsm_t *)fsm;
    switch ( param_id ) {
    case FSM_PARAM_SET_AUTOCAP_FR_ADDR: 
	case FSM_PARAM_SET_AUTOCAP_DS1_ADDR:{
        if ( !input || input_size != sizeof( fsm_param_dma_pipe_setting_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_dma_pipe_setting_t *p_new = (fsm_param_dma_pipe_setting_t *)input;
		struct module_cfg_info m_cfg;
		
        if(param_id == FSM_PARAM_SET_AUTOCAP_FR_ADDR)
		    m_cfg.p_type = dma_fr;
		else
			m_cfg.p_type = dma_ds1;
		
		m_cfg.frame_buffer_start0 = p_new->buf_array->primary.address;
		m_cfg.frame_size0 = p_new->buf_array->primary.size;
		m_cfg.frame_buffer_start1 = p_new->buf_array->secondary.address;
		m_cfg.frame_size1 = p_new->buf_array->secondary.size;
		
		autocap_set_new_param(&m_cfg);

        break;
    }
    case FSM_PARAM_SET_AUTOCAP_HW_RESET:
    {
        autocapture_hwreset((autocapture_fsm_t *)fsm);
        break;
    }
    default:
        rc = -1;
        break;
    }	
    return rc;
}

uint8_t autocapture_fsm_process_event( autocapture_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;

    return b_event_processed;
}
