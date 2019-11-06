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
#include "acamera_math.h"
#include "acamera_metering_stats_mem_config.h"
#include "system_stdlib.h"
#include "awb_manual_fsm.h"
#include "sbuf.h"

#include <linux/vmalloc.h>
#include <asm/uaccess.h>

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_AWB_MANUAL
#endif

static __inline uint32_t acamera_awb_statistics_data_read( AWB_fsm_t *p_fsm, uint32_t index_inp )
{
    return acamera_metering_stats_mem_array_data_read( p_fsm->cmn.isp_base, index_inp + ISP_METERING_OFFSET_AWB );
}

//==========AWB functions (calling order:  awb.scxml)=============================
// Handle the hardware interrupt
void AWB_fsm_process_interrupt( const AWB_fsm_t *p_fsm, uint8_t irq_event )
{
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;

    switch ( irq_event ) {
    case ACAMERA_IRQ_AWB_STATS:
        awb_read_statistics( (AWB_fsm_t *)p_fsm ); // we know what we are doing
        fsm_raise_event( p_fsm, event_id_awb_stats_ready );
        break;
    case ACAMERA_IRQ_FRAME_END:
        awb_coeffs_write( p_fsm );
        break;
    }
}

// Write matrix coefficients
void awb_coeffs_write( const AWB_fsm_t *p_fsm )
{
    acamera_isp_ccm_coefft_wb_r_write( p_fsm->cmn.isp_base, p_fsm->awb_warming[0] );
    acamera_isp_ccm_coefft_wb_g_write( p_fsm->cmn.isp_base, p_fsm->awb_warming[1] );
    acamera_isp_ccm_coefft_wb_b_write( p_fsm->cmn.isp_base, p_fsm->awb_warming[2] );

    LOG( LOG_INFO, "awb param applied: %u-%u-%u.", (unsigned int)p_fsm->awb_warming[0], (unsigned int)p_fsm->awb_warming[1], (unsigned int)p_fsm->awb_warming[2] );
}

void awb_roi_update( AWB_fsm_ptr_t p_fsm )
{
    uint16_t horz_zones = acamera_isp_metering_awb_nodes_used_horiz_read( p_fsm->cmn.isp_base );
    uint16_t vert_zones = acamera_isp_metering_awb_nodes_used_vert_read( p_fsm->cmn.isp_base );
    uint16_t x, y;

    uint16_t *ptr_awb_zone_whgh_h = _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AWB_ZONE_WGHT_HOR );
    uint16_t *ptr_awb_zone_whgh_v = _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AWB_ZONE_WGHT_VER );

    uint8_t x_start = ( uint8_t )( ( ( ( p_fsm->roi >> 24 ) & 0xFF ) * horz_zones + 128 ) >> 8 );
    uint8_t x_end = ( uint8_t )( ( ( ( p_fsm->roi >> 8 ) & 0xFF ) * horz_zones + 128 ) >> 8 );
    uint8_t y_start = ( uint8_t )( ( ( ( p_fsm->roi >> 16 ) & 0xFF ) * vert_zones + 128 ) >> 8 );
    uint8_t y_end = ( uint8_t )( ( ( ( p_fsm->roi >> 0 ) & 0xFF ) * vert_zones + 128 ) >> 8 );
    uint8_t zone_size_x = x_end - x_start;
    uint8_t zone_size_y = y_end - y_start;
    uint32_t middle_x = zone_size_x * 256 / 2;
    uint32_t middle_y = zone_size_y * 256 / 2;

    uint32_t len_zone_wght_hor = _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AWB_ZONE_WGHT_HOR );
    uint32_t len_zone_wght_ver = _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AWB_ZONE_WGHT_VER );
    uint16_t scale_x = ( horz_zones - 1 ) / ( len_zone_wght_hor > 0 ? len_zone_wght_hor : 1 ) + 1;
    uint16_t scale_y = ( vert_zones - 1 ) / ( len_zone_wght_ver > 0 ? len_zone_wght_ver : 1 ) + 1;

    uint16_t gaus_center_x = ( len_zone_wght_hor * 256 / 2 ) * scale_x;
    uint16_t gaus_center_y = ( len_zone_wght_ver * 256 / 2 ) * scale_y;


    for ( y = 0; y < vert_zones; y++ ) {
        uint8_t awb_coeff = 0;
        for ( x = 0; x < horz_zones; x++ ) {
            if ( y >= y_start && y <= y_end &&
                 x >= x_start && x <= x_end ) {

                uint8_t index_y = ( y - y_start );
                uint8_t index_x = ( x - x_start );
                int32_t distance_x = ( index_x * 256 + 128 ) - middle_x;
                int32_t distance_y = ( index_y * 256 + 128 ) - middle_y;
                uint32_t coeff_x;
                uint32_t coeff_y;

                if ( ( x == x_end && x_start != x_end ) ||
                     ( y == y_end && y_start != y_end ) ) {
                    awb_coeff = 0;
                } else {
                    coeff_x = ( gaus_center_x + distance_x ) / 256;
                    if ( distance_x > 0 && ( distance_x & 0x80 ) )
                        coeff_x--;
                    coeff_y = ( gaus_center_y + distance_y ) / 256;
                    if ( distance_y > 0 && ( distance_y & 0x80 ) )
                        coeff_y--;

                    coeff_x = ptr_awb_zone_whgh_h[coeff_x / scale_x];
                    coeff_y = ptr_awb_zone_whgh_v[coeff_y / scale_y];

                    awb_coeff = ( coeff_x * coeff_y ) >> 4;
                    if ( awb_coeff > 1 )
                        awb_coeff--;
                }
            } else {
                awb_coeff = 0;
            }
            acamera_isp_metering_awb_zones_weight_write( p_fsm->cmn.isp_base, y * vert_zones + x, awb_coeff );
        }
    }
}

int awb_set_zone_weight(AWB_fsm_ptr_t p_fsm, void *u_wg_ptr)
{
    int ret = -1;
    unsigned long awb_wg_size = 0;
    uint8_t *awb_wg = NULL;
    int x = 0;
    int y = 0;
    uint8_t awb_coeff = 0;
    uint32_t idx = 0;

    if (p_fsm == NULL || u_wg_ptr == NULL) {
        LOG(LOG_ERR, "Error invalid input param");
        return ret;
    }

    awb_wg_size = ISP_METERING_ZONES_AWB_H * ISP_METERING_ZONES_AWB_V;
    awb_wg = vmalloc(awb_wg_size);
    if (awb_wg == NULL) {
        LOG(LOG_ERR, "Failed to malloc mem");
        return ret;
    }

    memset(awb_wg, 0, awb_wg_size);

    if (copy_from_user(awb_wg, u_wg_ptr, awb_wg_size)) {
        vfree(awb_wg);
        LOG(LOG_ERR, "Failed to copy awb weight");
        return ret;
    }

    for (y = 0; y < ISP_METERING_ZONES_AWB_V; y++) {
        for (x = 0; x < ISP_METERING_ZONES_AWB_H; x++) {
            idx = y * ISP_METERING_ZONES_AWB_H + x;
            awb_coeff = awb_wg[idx];
            acamera_isp_metering_awb_zones_weight_write(
                                        p_fsm->cmn.isp_base, idx, awb_coeff);
            }
    }

    vfree(awb_wg);

    return 0;
}

// Initalisation code
void awb_init( AWB_fsm_t *p_fsm )
{
    uint32_t width;
    uint32_t height;
    // Initial AWB (rg,bg) is the identity
    p_fsm->rg_coef = 0x100;
    p_fsm->bg_coef = 0x100;

    // Set the default awb values
    if ( MAX_AWB_ZONES < acamera_isp_metering_awb_nodes_used_horiz_read( p_fsm->cmn.isp_base ) * acamera_isp_metering_awb_nodes_used_vert_read( p_fsm->cmn.isp_base ) ) {
        LOG( LOG_CRIT, "MAX_AWB_ZONES is less than hardware reported zones" );
    }

    p_fsm->wb_log2[0] = 0;
    p_fsm->wb_log2[1] = 0;
    p_fsm->wb_log2[2] = 0;
    p_fsm->wb_log2[3] = 0;
    awb_coeffs_write( p_fsm );

    width = acamera_isp_top_active_width_read( p_fsm->cmn.isp_base );
    height = acamera_isp_top_active_height_read( p_fsm->cmn.isp_base );

    if ( p_fsm->curr_AWB_ZONES != 0 ) {
        p_fsm->valid_threshold = ( width * height / ( 10 * p_fsm->curr_AWB_ZONES ) ); // division by zero is checked
    } else {
        p_fsm->valid_threshold = 0;
        LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO" );
    }

    p_fsm->awb_warming[0] = (int32_t)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AWB_WARMING_LS_D50 )[0];
    p_fsm->awb_warming[1] = (int32_t)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AWB_WARMING_LS_D50 )[1];
    p_fsm->awb_warming[2] = (int32_t)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AWB_WARMING_LS_D50 )[2];

    acamera_isp_ccm_coefft_wb_r_write( p_fsm->cmn.isp_base, p_fsm->awb_warming[0] );
    acamera_isp_ccm_coefft_wb_g_write( p_fsm->cmn.isp_base, p_fsm->awb_warming[1] );
    acamera_isp_ccm_coefft_wb_b_write( p_fsm->cmn.isp_base, p_fsm->awb_warming[2] );

    // Set the min/max temperatures and their gains:
    if ( ( _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_COLOR_TEMP )[0] != 0 ) && ( _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_COLOR_TEMP )[_GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_COLOR_TEMP ) - 1] != 0 ) && ( _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CT_RG_POS_CALC )[0] != 0 ) && ( _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CT_BG_POS_CALC )[0] != 0 ) && ( _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CT_RG_POS_CALC )[_GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CT_RG_POS_CALC ) - 1] != 0 ) && ( _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CT_BG_POS_CALC )[_GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CT_BG_POS_CALC ) - 1] != 0 ) ) {
        p_fsm->min_temp = 1000000 / _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_COLOR_TEMP )[_GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_COLOR_TEMP ) - 1];            // division by zero is checked
        p_fsm->max_temp = 1000000 / _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_COLOR_TEMP )[0];                                                                               // division by zero is checked
        p_fsm->max_temp_rg = U16_MAX / _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CT_RG_POS_CALC )[0];                                                                        // division by zero is checked
        p_fsm->max_temp_bg = U16_MAX / _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CT_BG_POS_CALC )[0];                                                                        // division by zero is checked
        p_fsm->min_temp_rg = U16_MAX / _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CT_RG_POS_CALC )[_GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CT_RG_POS_CALC ) - 1]; // division by zero is checked
        p_fsm->min_temp_bg = U16_MAX / _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CT_BG_POS_CALC )[_GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CT_BG_POS_CALC ) - 1]; // division by zero is checked
    } else {
        LOG( LOG_CRIT, "AVOIDED DIVISION BY ZERO" );
    }

    awb_roi_update( p_fsm );
}

// Set the 3x3 colour matrix to the identity matrix
void awb_set_identity( AWB_fsm_t *p_fsm )
{
    int i;

    for ( i = 0; i < 9; ++i ) {
        p_fsm->color_wb_matrix[i] = 0;
    }
    p_fsm->color_wb_matrix[0] = p_fsm->color_wb_matrix[4] = p_fsm->color_wb_matrix[8] = 0x100;
}

// Read the statistics from hardware
void awb_read_statistics( AWB_fsm_t *p_fsm )
{
    uint32_t _metering_lut_entry;
    uint16_t irg;
    uint16_t ibg;
    sbuf_awb_t *p_sbuf_awb_stats = NULL;
    struct sbuf_item sbuf;
    int fw_id = p_fsm->cmn.ctx_id;

    // Only selected number of zones will contribute
    uint16_t _i;

    p_fsm->sum = 0;

    memset( &sbuf, 0, sizeof( sbuf ) );
    sbuf.buf_type = SBUF_TYPE_AWB;
    sbuf.buf_status = SBUF_STATUS_DATA_EMPTY;

    if ( sbuf_get_item( fw_id, &sbuf ) ) {
        LOG( LOG_ERR, "Error: Failed to get sbuf, return." );
        return;
    }

    p_sbuf_awb_stats = (sbuf_awb_t *)sbuf.buf_base;
    LOG( LOG_DEBUG, "Get sbuf ok, idx: %u, status: %u, addr: %p.", sbuf.buf_idx, sbuf.buf_status, sbuf.buf_base );

    fsm_param_mon_alg_flow_t awb_flow;

    awb_flow.frame_id_tracking = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
    p_sbuf_awb_stats->frame_id = awb_flow.frame_id_tracking;

    // Read out the per zone statistics
    p_fsm->curr_AWB_ZONES = acamera_isp_metering_awb_nodes_used_horiz_read( p_fsm->cmn.isp_base ) *
                            acamera_isp_metering_awb_nodes_used_vert_read( p_fsm->cmn.isp_base );

    for ( _i = 0; _i < p_fsm->curr_AWB_ZONES; ++_i ) {
        _metering_lut_entry = acamera_awb_statistics_data_read( p_fsm, _i * 2 );
        //What we get from HW is G/R.
        //It is also programmable in the latest HW.AWB_STATS_MODE=0-->G/R and AWB_STATS_MODE=1-->R/G
        //rg_coef is actually R_gain appiled to R Pixels.Since we get (G*G_gain)/(R*R_gain) from HW,we multiply by the gain rg_coef to negate its effect.
        irg = ( _metering_lut_entry & 0xfff );
        ibg = ( ( _metering_lut_entry >> 16 ) & 0xfff );

        irg = ( irg * ( p_fsm->rg_coef ) ) >> 8;
        ibg = ( ibg * ( p_fsm->bg_coef ) ) >> 8;
        irg = ( irg == 0 ) ? 1 : irg;
        ibg = ( ibg == 0 ) ? 1 : ibg;

        p_sbuf_awb_stats->stats_data[_i].rg = U16_MAX / irg;
        p_sbuf_awb_stats->stats_data[_i].bg = U16_MAX / ibg;
        p_sbuf_awb_stats->stats_data[_i].sum = acamera_awb_statistics_data_read( p_fsm, _i * 2 + 1 );
        p_fsm->sum += p_sbuf_awb_stats->stats_data[_i].sum;
    }
    p_sbuf_awb_stats->curr_AWB_ZONES = p_fsm->curr_AWB_ZONES;

    /* read done, set the buffer back for future using  */
    sbuf.buf_status = SBUF_STATUS_DATA_DONE;

    if ( sbuf_set_item( fw_id, &sbuf ) ) {
        LOG( LOG_ERR, "Error: Failed to set sbuf, return." );
        return;
    }
    LOG( LOG_DEBUG, "Set sbuf ok, idx: %u, status: %u, addr: %p.", sbuf.buf_idx, sbuf.buf_status, sbuf.buf_base );

    awb_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
    awb_flow.flow_state = MON_ALG_FLOW_STATE_INPUT_READY;
    acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_AWB_FLOW, &awb_flow, sizeof( awb_flow ) );
    LOG( LOG_INFO, "AWB flow: INPUT_READY: frame_id_tracking: %d, cur frame_id: %u.", awb_flow.frame_id_tracking, awb_flow.frame_id_current );
}

//    For CCM switching
void awb_update_ccm( AWB_fsm_t *p_fsm )
{
    int32_t total_gain = 0;
    int high_gain = 0;

    LOG( LOG_DEBUG, "p_high: %d, light_source_candidate: %d, temperature_detected: %d.\n",
         p_fsm->p_high,
         p_fsm->light_source_candidate,
         p_fsm->temperature_detected );

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_TOTAL_GAIN, NULL, 0, &total_gain, sizeof( total_gain ) );

    high_gain = ( total_gain >> ( LOG2_GAIN_SHIFT - 8 ) ) >= _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CCM_ONE_GAIN_THRESHOLD )[0];

    fsm_param_ccm_info_t ccm_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CCM_INFO, NULL, 0, &ccm_info, sizeof( ccm_info ) );

    if ( p_fsm->p_high > 60 ) {
        ++p_fsm->detect_light_source_frames_count;
        if ( p_fsm->detect_light_source_frames_count >= p_fsm->switch_light_source_detect_frames_quantity ) {
#ifdef AWB_PRINT_DEBUG
            if ( ccm_info.light_source != AWB_LIGHT_SOURCE_D50 ) {
                LOG( LOG_DEBUG, "Light source is changed\n" );
                LOG( LOG_DEBUG, "p_high=%d, using AWB_LIGHT_SOURCE_D50\n", p_fsm->p_high );
            }
#endif

            ccm_info.light_source_previous = ccm_info.light_source;
            ccm_info.light_source = p_fsm->light_source_candidate;
            ccm_info.light_source_ccm_previous = ccm_info.light_source_ccm;
            ccm_info.light_source_ccm = high_gain ? AWB_LIGHT_SOURCE_UNKNOWN : p_fsm->light_source_candidate; // for low light set ccm = I
            ccm_info.light_source_change_frames = p_fsm->switch_light_source_change_frames_quantity;
            ccm_info.light_source_change_frames_left = p_fsm->switch_light_source_change_frames_quantity;

            acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_CCM_INFO, &ccm_info, sizeof( ccm_info ) );
        }

    } else if ( p_fsm->light_source_detected == p_fsm->light_source_candidate ) {
        if ( ( p_fsm->light_source_candidate != ccm_info.light_source ) || ( high_gain && ccm_info.light_source_ccm != AWB_LIGHT_SOURCE_UNKNOWN ) || ( !high_gain && ccm_info.light_source_ccm == AWB_LIGHT_SOURCE_UNKNOWN ) ) {
            ++p_fsm->detect_light_source_frames_count;
            if ( p_fsm->detect_light_source_frames_count >= p_fsm->switch_light_source_detect_frames_quantity && !ccm_info.light_source_change_frames_left ) {
                ccm_info.light_source_previous = ccm_info.light_source;
                ccm_info.light_source = p_fsm->light_source_candidate;
                ccm_info.light_source_ccm_previous = ccm_info.light_source_ccm;
                ccm_info.light_source_ccm = high_gain ? AWB_LIGHT_SOURCE_UNKNOWN : p_fsm->light_source_candidate; // for low light set ccm = I
                ccm_info.light_source_change_frames = p_fsm->switch_light_source_change_frames_quantity;
                ccm_info.light_source_change_frames_left = p_fsm->switch_light_source_change_frames_quantity;

                acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_CCM_INFO, &ccm_info, sizeof( ccm_info ) );
#ifdef AWB_PRINT_DEBUG
                // These are rarer so can print wherever they are fired (i.e. not dependent on ittcount)
                LOG( LOG_DEBUG, "Light source is changed\n" );
                if ( ccm_info.light_source == AWB_LIGHT_SOURCE_A )
                    LOG( LOG_DEBUG, "AWB_LIGHT_SOURCE_A\n" );
                if ( ccm_info.light_source == AWB_LIGHT_SOURCE_D40 )
                    LOG( LOG_DEBUG, "AWB_LIGHT_SOURCE_D40\n" );
                if ( ccm_info.light_source == AWB_LIGHT_SOURCE_D50 )
                    LOG( LOG_DEBUG, "AWB_LIGHT_SOURCE_D50\n" );
#endif
            }
        }
    } else {
        p_fsm->detect_light_source_frames_count = 0;
    }
    p_fsm->light_source_detected = p_fsm->light_source_candidate;
}

// Perform normalisation
void awb_normalise( AWB_fsm_t *p_fsm )
{
    int32_t wb[4];

    wb[0] = acamera_log2_fixed_to_fixed( _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STATIC_WB )[0], 8, LOG2_GAIN_SHIFT );
    wb[1] = acamera_log2_fixed_to_fixed( _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STATIC_WB )[1], 8, LOG2_GAIN_SHIFT );
    wb[2] = acamera_log2_fixed_to_fixed( _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STATIC_WB )[2], 8, LOG2_GAIN_SHIFT );
    wb[3] = acamera_log2_fixed_to_fixed( _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STATIC_WB )[3], 8, LOG2_GAIN_SHIFT );

    {
        /* For both auto mode and manual mode, we use the same variables to save the red/blue gains */
        wb[0] += acamera_log2_fixed_to_fixed( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_awb_red_gain, 8, LOG2_GAIN_SHIFT );
        wb[3] += acamera_log2_fixed_to_fixed( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_awb_blue_gain, 8, LOG2_GAIN_SHIFT );
        p_fsm->rg_coef = ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_awb_red_gain;
        p_fsm->bg_coef = ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_awb_blue_gain;
    }

    {
        int i;
        int32_t min_wb = wb[0];
        int32_t diff;

        for ( i = 1; i < 4; ++i ) {
            int32_t _wb = wb[i];
            if ( min_wb > _wb ) {
                min_wb = _wb;
            }
        }

        fsm_param_sensor_info_t sensor_info;
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

        diff = ( ISP_INPUT_BITS << LOG2_GAIN_SHIFT ) - acamera_log2_fixed_to_fixed( ( 1 << ISP_INPUT_BITS ) - sensor_info.black_level, 0, LOG2_GAIN_SHIFT ) - min_wb;
        for ( i = 0; i < 4; ++i ) {
            int32_t _wb = wb[i] + diff;
            p_fsm->wb_log2[i] = _wb;
        }
    }
}

void awb_set_new_param( AWB_fsm_ptr_t p_fsm, sbuf_awb_t *p_sbuf_awb )
{
    if ( p_sbuf_awb->frame_id == p_fsm->cur_result_gain_frame_id ) {
        LOG( LOG_ERR, "Error: Same frame used twice, frame_id: %u.", p_sbuf_awb->frame_id );
        return;
    }

    status_info_param_t *p_status_info = (status_info_param_t *)_GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STATUS_INFO );
    p_status_info->awb_info.mix_light_contrast = p_sbuf_awb->mix_light_contrast;

    ACAMERA_FSM2CTX_PTR( p_fsm )
        ->stab.global_awb_red_gain = p_sbuf_awb->awb_red_gain;
    ACAMERA_FSM2CTX_PTR( p_fsm )
        ->stab.global_awb_blue_gain = p_sbuf_awb->awb_blue_gain;

    p_fsm->temperature_detected = p_sbuf_awb->temperature_detected;
    p_fsm->light_source_candidate = p_sbuf_awb->light_source_candidate;
    p_fsm->p_high = p_sbuf_awb->p_high;
    system_memcpy( p_fsm->awb_warming, p_sbuf_awb->awb_warming, sizeof( p_fsm->awb_warming ) );

    p_fsm->cur_result_gain_frame_id = p_sbuf_awb->frame_id;

    if ( p_fsm->cur_result_gain_frame_id && ( p_fsm->pre_result_gain_frame_id != p_fsm->cur_result_gain_frame_id ) ) {
        fsm_param_mon_alg_flow_t awb_flow;

        p_fsm->pre_result_gain_frame_id = p_fsm->cur_result_gain_frame_id;

        awb_flow.frame_id_tracking = p_fsm->cur_result_gain_frame_id;
        awb_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
        awb_flow.flow_state = MON_ALG_FLOW_STATE_OUTPUT_READY;
        acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_AWB_FLOW, &awb_flow, sizeof( awb_flow ) );
        LOG( LOG_INFO, "AWB flow: OUTPUT_READY: frame_id_tracking: %d, cur frame_id: %u.", awb_flow.frame_id_tracking, awb_flow.frame_id_current );
    }

    fsm_raise_event( p_fsm, event_id_awb_result_ready );
}
