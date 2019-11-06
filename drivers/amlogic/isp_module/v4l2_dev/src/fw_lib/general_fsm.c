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
#include "general_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_GENERAL
#endif

void general_fsm_clear( general_fsm_t *p_fsm )
{
    p_fsm->api_reg_addr = 0;
    p_fsm->api_reg_size = 8;
    p_fsm->api_reg_source = 0;
    p_fsm->calibration_read_status = 0;
    p_fsm->wdr_mode = ISP_WDR_DEFAULT_MODE;

#if ISP_WDR_SWITCH
    p_fsm->wdr_mode_req = ISP_WDR_DEFAULT_MODE;
    p_fsm->wdr_auto_mode = 0;
    p_fsm->wdr_mode_frames = 0;
    p_fsm->cur_exp_number = SENSOR_DEFAULT_EXP_NUM;
#endif
}

void general_request_interrupt( general_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void general_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    general_fsm_t *p_fsm = (general_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    general_initialize( p_fsm );
}

static int general_set_reg_value( general_fsm_t *p_fsm, uint32_t value )
{
    int rc = 0;

#if REGISTERS_SOURCE_ID
    switch ( p_fsm->api_reg_source ) {
    case ISP:
        switch ( p_fsm->api_reg_size ) {
        case 8:
            acamera_sbus_write_u8( &( p_fsm->isp_sbus ), p_fsm->api_reg_addr, value );
            break;
        case 16:
            acamera_sbus_write_u16( &( p_fsm->isp_sbus ), p_fsm->api_reg_addr, value );
            break;
        case 32:
            acamera_sbus_write_u32( &( p_fsm->isp_sbus ), p_fsm->api_reg_addr, value );
            break;
        default:
            rc = -1;
            break;
        }

        break;

    case SENSOR: {
        fsm_param_reg_cfg_t reg_cfg;
        reg_cfg.reg_addr = p_fsm->api_reg_addr;
        reg_cfg.reg_value = value;
        rc = acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_SENSOR_REG, &reg_cfg, sizeof( reg_cfg ) );
        break;
    }

#if defined( ISP_HAS_AF_LMS_FSM ) || defined( ISP_HAS_AF_MANUAL_FSM )
    case LENS: {
        fsm_param_reg_cfg_t reg_cfg;
        reg_cfg.reg_addr = p_fsm->api_reg_addr;
        reg_cfg.reg_value = value;
        rc = acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_AF_LENS_REG, &reg_cfg, sizeof( reg_cfg ) );
        break;
    }
#endif

    default:
        rc = -1;
        break;
    }
#endif

    return rc;
}

int general_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    general_fsm_t *p_fsm = (general_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_SET_RELOAD_CALIBRATION:
        acamera_reload_isp_calibratons( p_fsm );
        break;

    case FSM_PARAM_SET_WDR_MODE: {

        if ( !input || input_size != sizeof( fsm_param_set_wdr_param_t ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_set_wdr_param_t *wdr_param = (fsm_param_set_wdr_param_t *)input;

#if ISP_WDR_SWITCH
        p_fsm->wdr_mode_req = wdr_param->wdr_mode;

        if ( ( p_fsm->wdr_mode_req != p_fsm->wdr_mode ) || ( p_fsm->cur_exp_number != wdr_param->exp_number ) ) {
            p_fsm->wdr_mode = p_fsm->wdr_mode_req;
            p_fsm->cur_exp_number = wdr_param->exp_number;

            general_set_wdr_mode( p_fsm );

            p_fsm->wdr_mode_frames = 0;
        }
#else
        p_fsm->wdr_mode = wdr_param->wdr_mode;
#endif
        break;
    }

    case FSM_PARAM_SET_REG_SETTING: {
        if ( !input || input_size != sizeof( fsm_param_reg_setting_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_reg_setting_t *p_input = (fsm_param_reg_setting_t *)input;

        if ( p_input->flag & REG_SETTING_BIT_REG_ADDR ) {
            p_fsm->api_reg_addr = p_input->api_reg_addr;
        }

        if ( p_input->flag & REG_SETTING_BIT_REG_SIZE ) {
            p_fsm->api_reg_size = p_input->api_reg_size;
        }

        if ( p_input->flag & REG_SETTING_BIT_REG_SOURCE ) {
            p_fsm->api_reg_source = p_input->api_reg_source;
        }

        if ( p_input->flag & REG_SETTING_BIT_REG_VALUE ) {
            rc = general_set_reg_value( p_fsm, p_input->api_reg_value );
        }
        break;
    }

    case FSM_PARAM_SET_SCENE_MODE:
        if ( !input || input_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_fsm->api_scene_mode = *(uint32_t *)input;

        break;

    default:
        rc = -1;
        break;
    }

    return rc;
}

static int general_get_reg_value( general_fsm_t *p_fsm, uint32_t *value )
{
    int rc = 0;

#if REGISTERS_SOURCE_ID
    switch ( p_fsm->api_reg_source ) {
    case ISP:
        switch ( p_fsm->api_reg_size ) {
        case 8:
            *value = acamera_sbus_read_u8( &( p_fsm->isp_sbus ), p_fsm->api_reg_addr );
            break;
        case 16:
            *value = acamera_sbus_read_u16( &( p_fsm->isp_sbus ), p_fsm->api_reg_addr );
            break;
        case 32:
            *value = acamera_sbus_read_u32( &( p_fsm->isp_sbus ), p_fsm->api_reg_addr );
            break;
        default:
            *value = ERR_BAD_ARGUMENT;
            rc = -1;
            break;
        }

        break;

    case SENSOR:
        rc = acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_REG, &p_fsm->api_reg_addr, sizeof( p_fsm->api_reg_addr ), value, sizeof( uint32_t ) );
        break;

#if defined( ISP_HAS_AF_LMS_FSM ) || defined( ISP_HAS_AF_MANUAL_FSM )
    case LENS:
        rc = acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_AF_LENS_REG, &p_fsm->api_reg_addr, sizeof( p_fsm->api_reg_addr ), value, sizeof( uint32_t ) );
        break;
#endif

    default:
        *value = ERR_BAD_ARGUMENT;
        rc = -1;
        break;
    }
#endif

    return rc;
}

int general_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    general_fsm_t *p_fsm = (general_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_GET_WDR_MODE:

        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->wdr_mode;

        break;

    case FSM_PARAM_GET_CALC_FE_LUT_OUTPUT:

        if ( !input || input_size != sizeof( uint32_t ) ||
             !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Inavlid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        // Based on old code, the input param is an uint32_t, but in function we just used uint16_t.
        *(uint32_t *)output = general_calc_fe_lut_output( p_fsm, *(uint16_t *)input );

        break;

    case FSM_PARAM_GET_REG_SETTING: {
        if ( !input || input_size != sizeof( fsm_param_reg_setting_t ) ||
             !output || output_size != sizeof( fsm_param_reg_setting_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        fsm_param_reg_setting_t *p_input = (fsm_param_reg_setting_t *)input;
        fsm_param_reg_setting_t *p_output = (fsm_param_reg_setting_t *)output;

        if ( p_input->flag & REG_SETTING_BIT_REG_ADDR ) {
            p_output->api_reg_addr = p_fsm->api_reg_addr;
        }

        if ( p_input->flag & REG_SETTING_BIT_REG_SIZE ) {
            p_output->api_reg_size = p_fsm->api_reg_size;
        }

        if ( p_input->flag & REG_SETTING_BIT_REG_SOURCE ) {
            p_output->api_reg_source = p_fsm->api_reg_source;
        }

        if ( p_input->flag & REG_SETTING_BIT_REG_VALUE ) {
            rc = general_get_reg_value( p_fsm, &p_output->api_reg_value );
        }

        break;
    }

    case FSM_PARAM_GET_SCENE_MODE:
        if ( !output || output_size != sizeof( uint32_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        *(uint32_t *)output = p_fsm->api_scene_mode;

        break;

    default:
        rc = -1;
        break;
    }

    return rc;
}

uint8_t general_fsm_process_event( general_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;


    switch ( event_id ) {
    default:
        break;
    case event_id_new_frame:
        acamera_general_interrupt_hanlder( ACAMERA_FSM2CTX_PTR( p_fsm ), ACAMERA_IRQ_FRAME_START );
        acamera_general_interrupt_hanlder( ACAMERA_FSM2CTX_PTR( p_fsm ), ACAMERA_IRQ_FRAME_END );
        acamera_general_interrupt_hanlder( ACAMERA_FSM2CTX_PTR( p_fsm ), ACAMERA_IRQ_ANTIFOG_HIST );
        acamera_general_interrupt_hanlder( ACAMERA_FSM2CTX_PTR( p_fsm ), ACAMERA_IRQ_AF2_STATS );
        acamera_general_interrupt_hanlder( ACAMERA_FSM2CTX_PTR( p_fsm ), ACAMERA_IRQ_AWB_STATS );
        acamera_general_interrupt_hanlder( ACAMERA_FSM2CTX_PTR( p_fsm ), ACAMERA_IRQ_AE_STATS );

        //need to be almost sync as the new address available from FR or DS
        acamera_general_interrupt_hanlder( ACAMERA_FSM2CTX_PTR( p_fsm ), ACAMERA_IRQ_FRAME_WRITER_FR ); //enabled for DMA_WRITER_FSM
        acamera_general_interrupt_hanlder( ACAMERA_FSM2CTX_PTR( p_fsm ), ACAMERA_IRQ_FRAME_WRITER_DS );

        b_event_processed = 1;
        break;

    case event_id_drop_frame:
        acamera_general_interrupt_hanlder( ACAMERA_FSM2CTX_PTR( p_fsm ), ACAMERA_IRQ_FRAME_START );

        //need to drop cur frame from FR or DS
        acamera_general_interrupt_hanlder( ACAMERA_FSM2CTX_PTR( p_fsm ), ACAMERA_IRQ_FRAME_DROP_FR ); //enabled for DMA_WRITER_FSM
        acamera_general_interrupt_hanlder( ACAMERA_FSM2CTX_PTR( p_fsm ), ACAMERA_IRQ_FRAME_DROP_DS );

        b_event_processed = 1;
        break;
    }
    return b_event_processed;
}
