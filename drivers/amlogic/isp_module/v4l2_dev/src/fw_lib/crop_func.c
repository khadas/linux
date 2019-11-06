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

#if ISP_HAS_DS1
#include "acamera_ds1_scaler_hfilt_coefmem_config.h"
#include "acamera_ds1_scaler_vfilt_coefmem_config.h"
#endif

//for h_filter and v_filter
#include "acamera_command_api.h"
#include "acamera_firmware_api.h"

#include "crop_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_CROP
#endif

int crop_validate_size( crop_fsm_t *p_fsm, uint16_t type, uint16_t sizeto, int isWidth ){

	if(sizeto==0){ //filter out invalid input
		return -1;
	}

	uint16_t sizefrom = 0;
	if(isWidth==1)
		sizefrom= acamera_isp_top_active_width_read( p_fsm->cmn.isp_base );
	else
		sizefrom= acamera_isp_top_active_height_read( p_fsm->cmn.isp_base );
    switch ( type ) {
    case CROP_FR:
    	if ( isWidth==1 && ( (p_fsm->crop_fr.xoffset + sizeto) > sizefrom) )
			return (((int)sizefrom - (int)p_fsm->crop_fr.xoffset) > 0 ? 0 : -1);
        else if ( isWidth==0 && ( (p_fsm->crop_fr.yoffset + sizeto) > sizefrom) )
        	return (((int)sizefrom - (int)p_fsm->crop_fr.yoffset) > 0 ? 0 : -1);
        break;
#if ISP_HAS_DS1
    case CROP_DS:
    	if ( isWidth==1 && ( (p_fsm->crop_ds.xoffset + sizeto) > sizefrom) )
			return (((int)sizefrom - (int)p_fsm->crop_ds.xoffset) > 0 ? 0 : -1);
		else if ( isWidth==0 && ( (p_fsm->crop_ds.yoffset + sizeto) > sizefrom) )
			return (((int)sizefrom - (int)p_fsm->crop_ds.yoffset) > 0 ? 0 : -1);
        break;
    case SCALER:
    	if ( p_fsm->crop_ds.enable ) {
    		if(isWidth==1 && ( (p_fsm->crop_ds.xoffset + sizeto) > sizefrom))
    			sizefrom = sizefrom - p_fsm->crop_ds.xoffset;
    		else if(isWidth==0 && ( (p_fsm->crop_ds.yoffset + sizeto) > sizefrom))
    			sizefrom = sizefrom - p_fsm->crop_ds.yoffset;
    	}
		if ( sizefrom >= 0x1000 && ((((uint32_t)sizefrom << 18 ) / sizeto ) << 2) >= 1<<24 )
			return -1;
		else if( (((uint32_t)sizefrom << 20 ) / sizeto) >= 1<<24  )
			return -1;
		else
			return 0;
        break;
#endif
    default:
        return -1;
        break;
    }

	return 0;
}

#if ISP_HAS_DS1

// update functions must be called inside interrupt to avoid broken frames
void _update_ds( crop_fsm_ptr_t p_fsm, int isr )
{
    // get actual image resolution from sensor
    uint16_t width = 0;
    uint16_t height = 0;

    //width and height should be from the top in case that RAW SCALER was updated
    width = acamera_isp_top_active_width_read( p_fsm->cmn.isp_base );
    height = acamera_isp_top_active_height_read( p_fsm->cmn.isp_base );

    // configure crop
    if ( p_fsm->crop_ds.enable ) {
        // check limits

        if ( p_fsm->crop_ds.xoffset >= width )
            p_fsm->crop_ds.xoffset = width - 1;
        if ( p_fsm->crop_ds.yoffset >= height )
            p_fsm->crop_ds.yoffset = height - 1;
        if ( p_fsm->crop_ds.xoffset + p_fsm->crop_ds.xsize > width )
            p_fsm->crop_ds.xsize = width - p_fsm->crop_ds.xoffset;
        if ( p_fsm->crop_ds.yoffset + p_fsm->crop_ds.ysize > height )
            p_fsm->crop_ds.ysize = height - p_fsm->crop_ds.yoffset;

        // apply crop
        acamera_isp_ds1_crop_start_x_write( p_fsm->cmn.isp_base, p_fsm->crop_ds.xoffset );
        acamera_isp_ds1_crop_start_y_write( p_fsm->cmn.isp_base, p_fsm->crop_ds.yoffset );
        acamera_isp_ds1_crop_size_x_write( p_fsm->cmn.isp_base, p_fsm->crop_ds.xsize );
        acamera_isp_ds1_crop_size_y_write( p_fsm->cmn.isp_base, p_fsm->crop_ds.ysize );
        width = p_fsm->crop_ds.xsize;
        height = p_fsm->crop_ds.ysize;
    }
    acamera_isp_ds1_crop_enable_crop_write( p_fsm->cmn.isp_base, p_fsm->crop_ds.enable );
    p_fsm->crop_ds.done = 1; //done
    // configure downscaler
    if ( p_fsm->scaler_ds.enable ) {
        // check limits
        if ( p_fsm->scaler_ds.xsize > width )
            p_fsm->scaler_ds.xsize = width;
        if ( p_fsm->scaler_ds.ysize > height )
            p_fsm->scaler_ds.ysize = height;

        // apply parameters
        acamera_isp_ds1_scaler_width_write( p_fsm->cmn.isp_base, width );
        acamera_isp_ds1_scaler_height_write( p_fsm->cmn.isp_base, height );
        acamera_isp_ds1_scaler_owidth_write( p_fsm->cmn.isp_base, p_fsm->scaler_ds.xsize );
        acamera_isp_ds1_scaler_oheight_write( p_fsm->cmn.isp_base, p_fsm->scaler_ds.ysize );
        if ( ( p_fsm->scaler_ds.xsize != 0 ) && ( p_fsm->scaler_ds.ysize != 0 ) ) {
            if ( width >= 0x1000 )
                acamera_isp_ds1_scaler_hfilt_tinc_write( p_fsm->cmn.isp_base, ( ( (uint32_t)width << 18 ) / p_fsm->scaler_ds.xsize ) << 2 ); // division by zero is checked
            else
                acamera_isp_ds1_scaler_hfilt_tinc_write( p_fsm->cmn.isp_base, ( (uint32_t)width << 20 ) / p_fsm->scaler_ds.xsize ); // division by zero is checked
            if ( height >= 0x1000 )
                acamera_isp_ds1_scaler_vfilt_tinc_write( p_fsm->cmn.isp_base, ( ( (uint32_t)height << 18 ) / p_fsm->scaler_ds.ysize ) << 2 ); // division by zero is checked
            else
                acamera_isp_ds1_scaler_vfilt_tinc_write( p_fsm->cmn.isp_base, ( (uint32_t)height << 20 ) / p_fsm->scaler_ds.ysize ); // division by zero is checked
            // filt_coefset_table
            //0    1.000
            //1    0.75
            //2    0.5
            //3    0.25
            if ( width >= 3 * p_fsm->scaler_ds.xsize )
                acamera_isp_ds1_scaler_hfilt_coefset_write( p_fsm->cmn.isp_base, 3 );
            else
                acamera_isp_ds1_scaler_hfilt_coefset_write( p_fsm->cmn.isp_base, 2 * width / p_fsm->scaler_ds.xsize - 2 ); // division by zero is checked
            if ( height >= 3 * p_fsm->scaler_ds.ysize )
                acamera_isp_ds1_scaler_vfilt_coefset_write( p_fsm->cmn.isp_base, 3 );
            else
                acamera_isp_ds1_scaler_vfilt_coefset_write( p_fsm->cmn.isp_base, 2 * height / p_fsm->scaler_ds.ysize - 2 ); // division by zero is checked
        } else {
            LOG( LOG_CRIT, "WRONG DS DOWNSCALER PARAMETERS: width: %d, height: %d", p_fsm->scaler_ds.xsize, p_fsm->scaler_ds.ysize );
        }
        width = p_fsm->scaler_ds.xsize;
        height = p_fsm->scaler_ds.ysize;
    }
    acamera_isp_top_bypass_ds1_scaler_write( p_fsm->cmn.isp_base, !p_fsm->scaler_ds.enable );
    p_fsm->scaler_ds.done = 1; //done
    p_fsm->width_ds = width;
    p_fsm->height_ds = height;
    if ( !isr )
        LOG( LOG_NOTICE, "DS update: Crop: e %d x %d, y %d, w %d, h %d, Downscaler: e %d, w %d, h %d", p_fsm->crop_ds.enable, p_fsm->crop_ds.xoffset, p_fsm->crop_ds.yoffset, p_fsm->crop_ds.xsize, p_fsm->crop_ds.ysize, p_fsm->scaler_ds.enable, p_fsm->scaler_ds.xsize, p_fsm->scaler_ds.ysize );
    LOG( LOG_INFO, "DS info: Crop: e %d x %d, y %d, w %d, h %d, Downscaler: e %d, w %d, h %d", p_fsm->crop_ds.enable, p_fsm->crop_ds.xoffset, p_fsm->crop_ds.yoffset, p_fsm->crop_ds.xsize, p_fsm->crop_ds.ysize, p_fsm->scaler_ds.enable, p_fsm->scaler_ds.xsize, p_fsm->scaler_ds.ysize );
}
#endif //ISP_HAS_DS1


void _update_fr( crop_fsm_ptr_t p_fsm, int isr )
{
    // get actual image resolution from sensor
    uint16_t width = 0;
    uint16_t height = 0;
    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

    width = acamera_isp_top_active_width_read( p_fsm->cmn.isp_base );
    height = acamera_isp_top_active_height_read( p_fsm->cmn.isp_base );
    LOG( LOG_INFO, "crop was configured with w:%u h:%u\n", width, height );

    if ( p_fsm->crop_fr.enable ) {
        // check limits

        if ( p_fsm->crop_fr.xoffset >= width )
            p_fsm->crop_fr.xoffset = width - 1;
        if ( p_fsm->crop_fr.yoffset >= height )
            p_fsm->crop_fr.yoffset = height - 1;
        if ( p_fsm->crop_fr.xoffset + p_fsm->crop_fr.xsize > width )
            p_fsm->crop_fr.xsize = width - p_fsm->crop_fr.xoffset;
        if ( p_fsm->crop_fr.yoffset + p_fsm->crop_fr.ysize > height )
            p_fsm->crop_fr.ysize = height - p_fsm->crop_fr.yoffset;
        // apply crop
        acamera_isp_fr_crop_start_x_write( p_fsm->cmn.isp_base, p_fsm->crop_fr.xoffset );
        acamera_isp_fr_crop_start_y_write( p_fsm->cmn.isp_base, p_fsm->crop_fr.yoffset );
        acamera_isp_fr_crop_size_x_write( p_fsm->cmn.isp_base, p_fsm->crop_fr.xsize );
        acamera_isp_fr_crop_size_y_write( p_fsm->cmn.isp_base, p_fsm->crop_fr.ysize );
        width = p_fsm->crop_fr.xsize;
        height = p_fsm->crop_fr.ysize;
    }


    acamera_isp_fr_crop_enable_crop_write( p_fsm->cmn.isp_base, p_fsm->crop_fr.enable );
    p_fsm->crop_fr.done = 1; //done
    p_fsm->width_fr = width;
    p_fsm->height_fr = height;
    if ( !isr )
        LOG( LOG_NOTICE, "FR update: Crop: e %d x %d, y %d, w %d, h %d", p_fsm->crop_fr.enable, p_fsm->crop_fr.xoffset, p_fsm->crop_fr.yoffset, p_fsm->crop_fr.xsize, p_fsm->crop_fr.ysize );

}

void crop_fsm_process_interrupt( crop_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;

    int no_log_in_isr = 1;
    if ( p_fsm->need_updating ) {
        LOG( LOG_INFO, "_update_ds from crop_fsm_process_interrupt irq_event:%d", irq_event );
        switch ( irq_event ) {

        case ACAMERA_IRQ_FRAME_WRITER_FR:
            _update_fr( (crop_fsm_ptr_t)p_fsm, no_log_in_isr );
#if ISP_HAS_DS1
            _update_ds( (crop_fsm_ptr_t)p_fsm, no_log_in_isr );
#endif
            fsm_raise_event( p_fsm, event_id_crop_updated );
            break;
        }
    }
}

void crop_resolution_changed( crop_fsm_ptr_t p_fsm )
{

    _update_fr( p_fsm, 0 );

#if ISP_HAS_DS1
    LOG( LOG_INFO, "_update_ds from crop_resolution_changed" );
    _update_ds( p_fsm, 0 );
#endif
    fsm_raise_event( p_fsm, event_id_crop_updated );
}

void crop_initialize( crop_fsm_ptr_t p_fsm )
{
#if ISP_HAS_DS1
    int i;
#endif

#if FW_DO_INITIALIZATION

#if ISP_HAS_DS1
    acamera_isp_top_bypass_ds1_scaler_write( p_fsm->cmn.isp_base, 0 );
    //  SCALER

    for ( i = 0; i < _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SCALER_H_FILTER ); i++ ) {
        acamera_ds1_scaler_hfilt_coefmem_array_data_write( p_fsm->cmn.isp_base, i, _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SCALER_H_FILTER )[i] );
    }

    for ( i = 0; i < _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SCALER_V_FILTER ); i++ ) {
        acamera_ds1_scaler_vfilt_coefmem_array_data_write( p_fsm->cmn.isp_base, i, _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SCALER_V_FILTER )[i] );
    }

    for ( i = 0; i < _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SCALER_H_FILTER ); i++ ) {
        if ( _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SCALER_H_FILTER )[i] != acamera_ds1_scaler_hfilt_coefmem_array_data_read( p_fsm->cmn.isp_base, i ) ) {
            LOG( LOG_ERR, "Wrong Scaler Coefficient %d read %X write %X",
                 i,
                 (unsigned int)acamera_ds1_scaler_hfilt_coefmem_array_data_read( p_fsm->cmn.isp_base, i ),
                 (unsigned int)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SCALER_H_FILTER )[i] );
            break;
        }
    }
    for ( i = 0; i < _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SCALER_V_FILTER ); i++ ) {
        if ( _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SCALER_V_FILTER )[i] != acamera_ds1_scaler_vfilt_coefmem_array_data_read( p_fsm->cmn.isp_base, i ) ) {
            LOG( LOG_ERR, "Wrong Scaler Coefficient %d read %X write %X",
                 i,
                 (unsigned int)acamera_ds1_scaler_vfilt_coefmem_array_data_read( p_fsm->cmn.isp_base, i ),
                 (unsigned int)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SCALER_V_FILTER )[i] );
            break;
        }
    }

#endif


#endif //FW_DO_INITIALIZATION

    p_fsm->resize_type = 0;
#if ( ISP_HAS_DS1 ) && defined( SCALER )
    p_fsm->resize_type = SCALER;
#elif defined( CROP_FR )
    p_fsm->resize_type = CROP_FR;
#endif

    // default parameters

    uint16_t width = acamera_isp_top_active_width_read( p_fsm->cmn.isp_base );
    uint16_t height = acamera_isp_top_active_height_read( p_fsm->cmn.isp_base );
    p_fsm->width_fr = width;
    p_fsm->height_fr = height;
#if 0  //avoid to cover these ds value
    p_fsm->crop_fr.xsize = width;
    p_fsm->crop_fr.ysize = height;

    p_fsm->width_ds = width;
    p_fsm->height_ds = height;
    p_fsm->crop_ds.xsize = width;
    p_fsm->crop_ds.ysize = height;
    p_fsm->scaler_ds.xsize = width;
    p_fsm->scaler_ds.ysize = height;
#endif


    // Called once at boot up
    p_fsm->mask.repeat_irq_mask = 0;
    p_fsm->mask.repeat_irq_mask |= ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_WRITER_FR );
#if ISP_HAS_DS1
    p_fsm->mask.repeat_irq_mask |= ACAMERA_IRQ_MASK( ACAMERA_IRQ_FRAME_WRITER_DS );
#endif

    // Finally request interrupts
    crop_request_interrupt( p_fsm, p_fsm->mask.repeat_irq_mask );
}
