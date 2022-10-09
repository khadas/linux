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
#include "crop_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_CROP
#endif

void crop_fsm_clear( crop_fsm_t *p_fsm )
{
    p_fsm->width_fr = 0;
    p_fsm->height_fr = 0;
    p_fsm->crop_fr.enable = 0;
    p_fsm->crop_fr.done = 1;
    p_fsm->crop_fr.xoffset = 0;
    p_fsm->crop_fr.yoffset = 0;
    p_fsm->crop_fr.xsize = 0;
    p_fsm->crop_fr.ysize = 0;

#if ISP_HAS_DS1
    p_fsm->width_ds = 0;
    p_fsm->height_ds = 0;
    p_fsm->crop_ds.enable = 0;
    p_fsm->crop_ds.done = 1;
    p_fsm->crop_ds.xoffset = 0;
    p_fsm->crop_ds.yoffset = 0;
    p_fsm->crop_ds.xsize = 0;
    p_fsm->crop_ds.ysize = 0;
    p_fsm->scaler_ds.enable = 0;
    p_fsm->scaler_ds.done = 1;
    p_fsm->scaler_ds.xsize = 0;
    p_fsm->scaler_ds.ysize = 0;
#endif
    p_fsm->need_updating = 0;
}

void crop_request_interrupt( crop_fsm_ptr_t p_fsm, system_fw_interrupt_mask_t mask )
{
    acamera_isp_interrupts_disable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
    p_fsm->mask.irq_mask |= mask;
    acamera_isp_interrupts_enable( ACAMERA_FSM2MGR_PTR( p_fsm ) );
}

void crop_fsm_init( void *fsm, fsm_init_param_t *init_param )
{
    crop_fsm_t *p_fsm = (crop_fsm_t *)fsm;
    p_fsm->cmn.p_fsm_mgr = init_param->p_fsm_mgr;
    p_fsm->cmn.isp_base = init_param->isp_base;
    p_fsm->p_fsm_mgr = init_param->p_fsm_mgr;

    crop_fsm_clear( p_fsm );
}

#ifdef IMAGE_RESIZE_TYPE_ID
static int crop_set_resize_type( crop_fsm_t *p_fsm, uint16_t type )
{
    int rc = 0;

    switch ( type ) {
    case CROP_FR:
        LOG( LOG_DEBUG, "setting resize type CROP_FR" );
        p_fsm->resize_type = CROP_FR;
        break;

#if ISP_HAS_DS1
    case CROP_DS:
        LOG( LOG_DEBUG, "setting resize type CROP_DS" );
        p_fsm->resize_type = CROP_DS;
        break;
    case SCALER:
        LOG( LOG_DEBUG, "setting resize type SCALER" );
        p_fsm->resize_type = SCALER;
        break;
#endif

    default:
        rc = -1;
        LOG( LOG_ERR, "Unsupported resize type %d\n", type );
        break;
    }

    return rc;
}

static int crop_set_resize_enable( crop_fsm_t *p_fsm, uint16_t type, uint8_t enable )
{
    int rc = 0;

    switch ( type ) {
    case CROP_FR:
        if ( enable ) {
            p_fsm->crop_fr.enable = 1;
            p_fsm->crop_fr.done = 0;
        }

        break;
#if ISP_HAS_DS1
    case CROP_DS:
        if ( enable ) {
            p_fsm->crop_ds.enable = 1;
            p_fsm->crop_ds.done = 0;
        }

        break;

    case SCALER:
        if ( enable ) {
            p_fsm->scaler_ds.enable = 1;
            p_fsm->scaler_ds.done = 0;
        }

        break;
#endif

    default:
        rc = -1;
        LOG( LOG_DEBUG, "Invalid resize_type: %d\n", type );
        break;
    }

    if ( 0 == rc ) {

        LOG( LOG_DEBUG, "resize_type: %d, calling acamera_fsm_mgr_raise_event %x\n", type, event_id_crop_changed );
        crop_resolution_changed( p_fsm );
        fsm_raise_event( p_fsm, event_id_crop_changed );
    }

    return rc;
}

static int crop_set_resize_xsize( crop_fsm_t *p_fsm, uint16_t type, uint16_t xsize )
{
    if(crop_validate_size(p_fsm,type,xsize,1)){ //filter out invalid input
    	return -1;
    }

    int rc = 0;

    switch ( type ) {
    case CROP_FR:
        p_fsm->crop_fr.xsize = xsize;
        break;
#if ISP_HAS_DS1
    case CROP_DS:
        p_fsm->crop_ds.xsize = xsize;
        break;

    case SCALER:
        p_fsm->scaler_ds.xsize = xsize;
        break;
#endif

    default:
        rc = -1;
        LOG( LOG_DEBUG, "Invalid resize_type: %d\n", type );
        break;
    }

    return rc;
}

static int crop_set_resize_ysize( crop_fsm_t *p_fsm, uint16_t type, uint16_t ysize )
{
	if(crop_validate_size(p_fsm,type,ysize,0)){ //filter out invalid input
		return -1;
	}

    int rc = 0;

    switch ( type ) {

    case CROP_FR:
        p_fsm->crop_fr.ysize = ysize;
        break;

#if ISP_HAS_DS1
    case CROP_DS:
        p_fsm->crop_ds.ysize = ysize;
        break;

    case SCALER:
        p_fsm->scaler_ds.ysize = ysize;
        break;
#endif

    default:
        rc = -1;
        LOG( LOG_DEBUG, "Invalid resize_type: %d\n", type );
        break;
    }

    return rc;
}

static int crop_set_resize_xoffset( crop_fsm_t *p_fsm, uint16_t type, uint16_t xoffset )
{
    int rc = 0;

    switch ( type ) {
    case CROP_FR:
        p_fsm->crop_fr.xoffset = xoffset;
        break;

#if ISP_HAS_DS1
    case CROP_DS:
        p_fsm->crop_ds.xoffset = xoffset;
        break;
#endif

    default:
        rc = -1;
        LOG( LOG_DEBUG, "Invalid resize_type: %d\n", type );
        break;
    }

    return rc;
}

static int crop_set_resize_yoffset( crop_fsm_t *p_fsm, uint16_t type, uint16_t yoffset )
{
    int rc = 0;

    switch ( type ) {
    case CROP_FR:
        p_fsm->crop_fr.yoffset = yoffset;
        break;

#if ISP_HAS_DS1
    case CROP_DS:
        p_fsm->crop_ds.yoffset = yoffset;
        break;
#endif
    default:
        rc = -1;
        LOG( LOG_DEBUG, "Invalid resize_type: %d\n", type );
        break;
    }

    return rc;
}
#endif

int crop_fsm_set_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size )
{
    int rc = 0;
    crop_fsm_t *p_fsm = (crop_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_SET_CROP_SETTING: {
        if ( !input || input_size != sizeof( fsm_param_crop_setting_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d, p_fsm: %p.", param_id, p_fsm );
            rc = -1;
            break;
        }

#ifdef IMAGE_RESIZE_TYPE_ID
        fsm_param_crop_setting_t *p_setting = (fsm_param_crop_setting_t *)input;

        if ( p_setting->flag & CROP_SETTING_BIT_RESIZE_TYPE ) {
            rc |= crop_set_resize_type( p_fsm, p_setting->resize_type );
        }

        if ( 0 == p_setting->resize_type )
            p_setting->resize_type = p_fsm->resize_type;

        if ( p_setting->flag & CROP_SETTING_BIT_ENABLE ) {
            rc |= crop_set_resize_enable( p_fsm, p_setting->resize_type, p_setting->enable );
        }

        if ( p_setting->flag & CROP_SETTING_BIT_XSIZE ) {
            rc |= crop_set_resize_xsize( p_fsm, p_setting->resize_type, p_setting->xsize );
        }

        if ( p_setting->flag & CROP_SETTING_BIT_YSIZE ) {
            rc |= crop_set_resize_ysize( p_fsm, p_setting->resize_type, p_setting->ysize );
        }

        if ( p_setting->flag & CROP_SETTING_BIT_XOFFSET ) {
            rc |= crop_set_resize_xoffset( p_fsm, p_setting->resize_type, p_setting->xoffset );
        }

        if ( p_setting->flag & CROP_SETTING_BIT_YOFFSET ) {
            rc |= crop_set_resize_yoffset( p_fsm, p_setting->resize_type, p_setting->yoffset );
        }
#endif
    } break;

    default:
        rc = -1;
        break;
    }

    return rc;
}

#ifdef IMAGE_RESIZE_TYPE_ID
static int crop_get_setting_done( crop_fsm_t *p_fsm, uint16_t type, fsm_param_crop_setting_t *p_output )
{
    int rc = 0;

    switch ( type ) {
    case CROP_FR:
        p_output->done = p_fsm->crop_fr.done;
        break;
#if ISP_HAS_DS1
    case CROP_DS:
        p_output->done = p_fsm->crop_ds.done;
        break;
    case SCALER:
        p_output->done = p_fsm->scaler_ds.done;
        break;
#endif


    default:
        rc = -1;
        break;
    }

    return rc;
}

static int crop_get_setting_xsize( crop_fsm_t *p_fsm, uint16_t type, fsm_param_crop_setting_t *p_output )
{
    int rc = 0;

    switch ( type ) {
    case CROP_FR:
        p_output->xsize = p_fsm->crop_fr.xsize;
        break;

#if ISP_HAS_DS1
    case CROP_DS:
        p_output->xsize = p_fsm->crop_ds.xsize;
        break;
    case SCALER:
        p_output->xsize = p_fsm->scaler_ds.xsize;
        break;
#endif


    default:
        rc = -1;
        break;
    }

    return rc;
}

static int crop_get_setting_ysize( crop_fsm_t *p_fsm, uint16_t type, fsm_param_crop_setting_t *p_output )
{
    int rc = 0;

    switch ( type ) {
    case CROP_FR:
        p_output->ysize = p_fsm->crop_fr.ysize;
        break;


#if ISP_HAS_DS1
    case CROP_DS:
        p_output->ysize = p_fsm->crop_ds.ysize;
        break;
    case SCALER:
        p_output->ysize = p_fsm->scaler_ds.ysize;
        break;
#endif


    default:
        rc = -1;
        break;
    }

    return rc;
}

static int crop_get_setting_xoffset( crop_fsm_t *p_fsm, uint16_t type, fsm_param_crop_setting_t *p_output )
{
    int rc = 0;

    switch ( type ) {
    case CROP_FR:
        p_output->xoffset = p_fsm->crop_fr.xoffset;
        break;

#if ISP_HAS_DS1
    case CROP_DS:
        p_output->xoffset = p_fsm->crop_ds.xoffset;
        break;
#endif

    default:
        rc = -1;
        break;
    }

    return rc;
}

static int crop_get_setting_yoffset( crop_fsm_t *p_fsm, uint16_t type, fsm_param_crop_setting_t *p_output )
{
    int rc = 0;

    switch ( type ) {
    case CROP_FR:
        p_output->yoffset = p_fsm->crop_fr.yoffset;
        break;

#if ISP_HAS_DS1
    case CROP_DS:
        p_output->yoffset = p_fsm->crop_ds.yoffset;
        break;
#endif


    default:
        rc = -1;
        break;
    }

    return rc;
}
#endif


int crop_fsm_get_param( void *fsm, uint32_t param_id, void *input, uint32_t input_size, void *output, uint32_t output_size )
{
    int rc = 0;
    crop_fsm_t *p_fsm = (crop_fsm_t *)fsm;

    switch ( param_id ) {
    case FSM_PARAM_GET_CROP_INFO: {
        fsm_param_crop_info_t *p_crop_info = NULL;

        if ( !output || output_size != sizeof( fsm_param_crop_info_t ) ) {
            LOG( LOG_ERR, "Size mismatch, param_id: %d.", param_id );
            rc = -1;
            break;
        }

        p_crop_info = (fsm_param_crop_info_t *)output;

        p_crop_info->width_fr = p_fsm->width_fr;
        p_crop_info->height_fr = p_fsm->height_fr;
#if ISP_HAS_DS1
        p_crop_info->width_ds = p_fsm->width_ds;
        p_crop_info->height_ds = p_fsm->height_ds;
#endif

        break;
    }

    case FSM_PARAM_GET_CROP_SETTING: {
        if ( !input || input_size != sizeof( fsm_param_crop_setting_t ) ||
             !output || output_size != sizeof( fsm_param_crop_setting_t ) ) {
            LOG( LOG_ERR, "Invalid param, param_id: %d.", param_id );
            rc = -1;
            break;
        }

#ifdef IMAGE_RESIZE_TYPE_ID
        fsm_param_crop_setting_t *p_input = (fsm_param_crop_setting_t *)input;
        fsm_param_crop_setting_t *p_output = (fsm_param_crop_setting_t *)output;

        if ( p_input->flag & CROP_SETTING_BIT_RESIZE_TYPE ) {
            p_output->resize_type = p_fsm->resize_type;
        }

        if ( 0 == p_input->resize_type )
            p_input->resize_type = p_fsm->resize_type;

        if ( p_input->flag & CROP_SETTING_BIT_DONE ) {
            rc |= crop_get_setting_done( p_fsm, p_input->resize_type, p_output );
        }

        if ( p_input->flag & CROP_SETTING_BIT_XSIZE ) {
            rc |= crop_get_setting_xsize( p_fsm, p_input->resize_type, p_output );
        }

        if ( p_input->flag & CROP_SETTING_BIT_YSIZE ) {
            rc |= crop_get_setting_ysize( p_fsm, p_input->resize_type, p_output );
        }

        if ( p_input->flag & CROP_SETTING_BIT_XOFFSET ) {
            rc |= crop_get_setting_xoffset( p_fsm, p_input->resize_type, p_output );
        }

        if ( p_input->flag & CROP_SETTING_BIT_YOFFSET ) {
            rc |= crop_get_setting_yoffset( p_fsm, p_input->resize_type, p_output );
        }
#endif

        break;
    }

    default:
        rc = -1;
        break;
    }

    return rc;
}


uint8_t crop_fsm_process_event( crop_fsm_t *p_fsm, event_id_t event_id )
{
    uint8_t b_event_processed = 0;
    switch ( event_id ) {
    default:
        break;
    case event_id_sensor_ready:
        crop_initialize( p_fsm );
        b_event_processed = 1;
        break;

    case event_id_crop_changed:
        p_fsm->need_updating = 1;
        b_event_processed = 1;
        break;

    case event_id_crop_updated:
        p_fsm->need_updating = 0;
        b_event_processed = 1;
        break;
    }

    return b_event_processed;
}
