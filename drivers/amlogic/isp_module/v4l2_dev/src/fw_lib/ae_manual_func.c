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

#include "acamera_firmware_api.h"
#include "acamera_fw.h"
#include "acamera_math.h"
#include "acamera_command_api.h"

#include "acamera_aexp_hist_stats_mem_config.h"
#include "acamera_metering_stats_mem_config.h"

#include "acamera_math.h"
#include "ae_manual_fsm.h"

#include "sbuf.h"

#include "system_am_sc1.h"
#include "system_am_sc2.h"

#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include "system_am_flicker.h"
#include "system_am_decmpr.h"

#include "nr_fw.h"
#include "cmos_fsm.h"

#define DEFAULT_AE_EXPOSURE_LOG2 2250000 //3390000

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_AE_MANUAL
#endif

void ae_roi_update( AE_fsm_ptr_t p_fsm )
{
    uint16_t horz_zones = acamera_isp_metering_hist_aexp_nodes_used_horiz_read( p_fsm->cmn.isp_base );
    uint16_t vert_zones = acamera_isp_metering_hist_aexp_nodes_used_vert_read( p_fsm->cmn.isp_base );
    uint16_t x, y;

    uint16_t *ptr_ae_zone_whgh_h = _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_ZONE_WGHT_HOR );
    uint16_t *ptr_ae_zone_whgh_v = _GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_ZONE_WGHT_VER );

    uint8_t x_start = ( uint8_t )( ( ( ( p_fsm->roi >> 24 ) & 0xFF ) * horz_zones + 128 ) >> 8 );
    uint8_t x_end = ( uint8_t )( ( ( ( p_fsm->roi >> 8 ) & 0xFF ) * horz_zones + 128 ) >> 8 );
    uint8_t y_start = ( uint8_t )( ( ( ( p_fsm->roi >> 16 ) & 0xFF ) * vert_zones + 128 ) >> 8 );
    uint8_t y_end = ( uint8_t )( ( ( ( p_fsm->roi >> 0 ) & 0xFF ) * vert_zones + 128 ) >> 8 );

    uint8_t zone_size_x = x_end - x_start;
    uint8_t zone_size_y = y_end - y_start;
    uint32_t middle_x = zone_size_x * 256 / 2 + x_start * 256;
    uint32_t middle_y = zone_size_y * 256 / 2 + y_start * 256;
    uint16_t scale_x = 0;
    uint16_t scale_y = 0;
    uint32_t ae_zone_wght_hor_len = _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_ZONE_WGHT_HOR );
    uint32_t ae_zone_wght_ver_len = _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_ZONE_WGHT_VER );

    if ( ae_zone_wght_hor_len ) {
        scale_x = ( horz_zones - 1 ) * 256 / ae_zone_wght_hor_len + 1;
    } else {
        LOG( LOG_ERR, "ae_zone_wght_hor_len is zero" );
        return;
    }
    if ( ae_zone_wght_ver_len ) {
        scale_y = ( vert_zones - 1 ) * 256 / ae_zone_wght_ver_len + 1;
    } else {
        LOG( LOG_ERR, "ae_zone_wght_ver_len is zero" );
        return;
    }

    uint16_t gaus_center_x = ( _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_ZONE_WGHT_HOR ) * scale_x / 2 );
    uint16_t gaus_center_y = ( _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_AE_ZONE_WGHT_VER ) * scale_y / 2 );

    for ( y = 0; y < vert_zones; y++ ) {
        uint8_t ae_coeff = 0;
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
                    ae_coeff = 0;
                } else {
                    coeff_x = ( gaus_center_x + distance_x * scale_x / 256 ) / 256;
                    if ( distance_x > 0 && ( distance_x & 0x80 ) )
                        coeff_x--;
                    coeff_y = ( gaus_center_y + distance_y * scale_y / 256 ) / 256;
                    if ( distance_y > 0 && ( distance_y & 0x80 ) )
                        coeff_y--;

                    coeff_x = ptr_ae_zone_whgh_h[coeff_x * 256 / scale_x];
                    coeff_y = ptr_ae_zone_whgh_v[coeff_y * 256 / scale_y];

                    ae_coeff = ( coeff_x * coeff_y ) >> 4;
                    if ( ae_coeff > 1 )
                        ae_coeff--;
                }
            } else {
                ae_coeff = 0;
            }
            acamera_isp_metering_hist_aexp_zones_weight_write( p_fsm->cmn.isp_base, ISP_METERING_ZONES_MAX_H * y + x, ae_coeff );
        }
    }
}

int ae_set_zone_weight(AE_fsm_ptr_t p_fsm, void *u_wg_ptr)
{
    int ret = -1;

    int x = 0;
    int y = 0;
    uint8_t ae_coeff = 0;
    uint32_t idx = 0;
    uint8_t * u_wg = NULL;
    if (p_fsm == NULL || u_wg_ptr == NULL) {
        LOG(LOG_CRIT, "Error invalid input param");
        return ret;
    }
    u_wg = (uint8_t * )u_wg_ptr;

    for (y = 0; y < ISP_METERING_ZONES_MAX_V; y++) {
        for (x = 0; x < ISP_METERING_ZONES_MAX_H; x++) {
            idx = y * ISP_METERING_ZONES_MAX_H + x;
            ae_coeff = u_wg[idx];
            acamera_isp_metering_hist_aexp_zones_weight_write(
                                        p_fsm->cmn.isp_base, idx, ae_coeff);
            }
    }

    return 0;
}

int ae_get_zone_weight(AE_fsm_ptr_t p_fsm, uint8_t *u_wg_ptr)
{
    int ret = -1;
    int x = 0;
    int y = 0;
    uint32_t idx = 0;

    if (p_fsm == NULL || u_wg_ptr == NULL) {
        LOG(LOG_ERR, "Error invalid input param");
        return ret;
    }

    for (y = 0; y < ISP_METERING_ZONES_MAX_V; y++) {
        for (x = 0; x < ISP_METERING_ZONES_MAX_H; x++) {
            idx = y * ISP_METERING_ZONES_MAX_H + x;
                u_wg_ptr[idx] = acamera_isp_metering_hist_aexp_zones_weight_read(
                                        p_fsm->cmn.isp_base, idx);
            }
    }

    return 0;
}

void ae_initialize( AE_fsm_ptr_t p_fsm )
{
    acamera_isp_metering_aexp_hist_thresh_0_1_write( p_fsm->cmn.isp_base, 0 );
    acamera_isp_metering_aexp_hist_thresh_1_2_write( p_fsm->cmn.isp_base, 0 );
    acamera_isp_metering_aexp_hist_thresh_3_4_write( p_fsm->cmn.isp_base, 0 );
    acamera_isp_metering_aexp_hist_thresh_4_5_write( p_fsm->cmn.isp_base, 224 );

    int i, j;
    for ( i = 0; i < ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_VERT_DEFAULT; i++ )
        for ( j = 0; j < ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT; j++ )
            acamera_isp_metering_hist_aexp_zones_weight_write( p_fsm->cmn.isp_base, ISP_METERING_ZONES_MAX_H * i + j, 15 );

    ae_roi_update( p_fsm );


    //p_fsm->new_exposure_log2 = DEFAULT_AE_EXPOSURE_LOG2;

    p_fsm->mask.repeat_irq_mask = ACAMERA_IRQ_MASK( ACAMERA_IRQ_AE_STATS );
    AE_request_interrupt( p_fsm, p_fsm->mask.repeat_irq_mask );

    // set default exposure value
    ae_calculate_exposure( p_fsm );
}

#ifdef ISP_HAS_CMPR
int cmpr_cnt;
#endif
static char dump_name[100];
static loff_t pos;

static int dump_flicker_stats (void* data, int size,int frame_id_current) {

    memset((void *)dump_name, 0, 100);

    sprintf(dump_name, "/data/flicker/dump_flicker-frameid-%d.stats", frame_id_current);

    struct file *fp = filp_open(dump_name,O_RDWR|O_CREAT,0666);
    if (fp == NULL) {
        LOG( LOG_CRIT,"open %s fail",dump_name);
        return -1;
    }
    pos = fp->f_pos;
    int ret = vfs_write(fp,data, size, &pos);
    if (ret == -1) {
        LOG( LOG_CRIT,"write file fail:%s",dump_name);
        return -1;
    }
    filp_close(fp,NULL);

    return 0;
}

void ae_read_full_histogram_data( AE_fsm_ptr_t p_fsm )
{
    int i;
    int shift = 0;
    uint32_t sum = 0;
    sbuf_ae_t *p_sbuf_ae;
    struct sbuf_item sbuf;
    int fw_id = p_fsm->cmn.ctx_id;
    fsm_param_mon_alg_flow_t ae_flow;

    memset( &sbuf, 0, sizeof( sbuf ) );
    sbuf.buf_type = SBUF_TYPE_AE;
    sbuf.buf_status = SBUF_STATUS_DATA_EMPTY;

    if ( sbuf_get_item( fw_id, &sbuf ) ) {
        LOG( LOG_ERR, "Error: Failed to get sbuf, return." );
        return;
    }
#if ISP_HAS_FLICKER
    static T_FLKR_CTRL_PRM prm_flkr_ctrl;
    static T_FLKR_DET_PRM  prm_flkr_det;

    param_flkr_ctrl_init(&prm_flkr_ctrl);
    param_flkr_det_init(&prm_flkr_det);

    if ( fw_flicker_det(&prm_flkr_ctrl, &prm_flkr_det) ) {
        cmos_control_param_t *param = (cmos_control_param_t *)_GET_UINT_PTR( ACAMERA_MGR2CTX_PTR( instance ), CALIBRATION_CMOS_CONTROL );
        if ( prm_flkr_det.flkr_det_50hz )
            param->global_anti_flicker_frequency = 50;
        else
            param->global_anti_flicker_frequency = 60;
    }

#endif
    p_sbuf_ae = (sbuf_ae_t *)sbuf.buf_base;
    LOG( LOG_DEBUG, "Get sbuf ok, idx: %u, status: %u, addr: %p.", sbuf.buf_idx, sbuf.buf_status, sbuf.buf_base );

    ae_flow.frame_id_tracking = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
    p_sbuf_ae->frame_id = ae_flow.frame_id_tracking;

    for ( i = 0; i < ISP_FULL_HISTOGRAM_SIZE; ++i ) {
        uint32_t v = acamera_aexp_hist_stats_mem_array_data_read( p_fsm->cmn.isp_base, i );

        shift = ( v >> 12 ) & 0xF;
        v = v & 0xFFF;
        if ( shift ) {
            v = ( v | 0x1000 ) << ( shift - 1 );
        }

        /* some other FSMs(such as sharpening) in kern-FW also need AE stats data */
        p_fsm->fullhist[i] = v;

        sum += v;
    }

    p_fsm->fullhist_sum = sum;

    /* NOTE: the size should match */
    memcpy( p_sbuf_ae->stats_data, p_fsm->fullhist, sizeof( p_sbuf_ae->stats_data ) );
    p_sbuf_ae->histogram_sum = sum;
    LOG( LOG_DEBUG, "histsum: histogram_sum: %u.", p_sbuf_ae->histogram_sum );

    int j;
    for ( i = 0; i < ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_VERT_DEFAULT; i++ ) {
        for ( j = 0; j < ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT; j++ ) {
            p_sbuf_ae->hist4[i * ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT + j] = ( uint16_t )( acamera_metering_stats_mem_array_data_read( p_fsm->cmn.isp_base, ( i * ACAMERA_ISP_METERING_HIST_AEXP_NODES_USED_HORIZ_DEFAULT + j ) * 2 + 1 ) >> 16 );
        }
    }

    /* read done, set the buffer back for future using */
    sbuf.buf_status = SBUF_STATUS_DATA_DONE;

    if ( sbuf_set_item( fw_id, &sbuf ) ) {
        LOG( LOG_ERR, "Error: Failed to set sbuf, return." );
        return;
    }
    LOG( LOG_DEBUG, "Set sbuf ok, idx: %u, status: %u, addr: %p.", sbuf.buf_idx, sbuf.buf_status, sbuf.buf_base );

    ae_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
    ae_flow.flow_state = MON_ALG_FLOW_STATE_INPUT_READY;
    acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_AE_FLOW, &ae_flow, sizeof( ae_flow ) );

    LOG( LOG_DEBUG, "AE flow: INPUT_READY: frame_id_tracking: %d, cur frame_id: %u.", ae_flow.frame_id_tracking, ae_flow.frame_id_current );
}

void AE_fsm_process_interrupt( AE_fsm_const_ptr_t p_fsm, uint8_t irq_event )
{
    if ( acamera_fsm_util_is_irq_event_ignored( (fsm_irq_mask_t *)( &p_fsm->mask ), irq_event ) )
        return;

    switch ( irq_event ) {
    case ACAMERA_IRQ_AE_STATS:
        ae_read_full_histogram_data( (AE_fsm_ptr_t)p_fsm );
        fsm_raise_event( p_fsm, event_id_ae_stats_ready );
        break;
    }
}

void ae_set_new_param( AE_fsm_ptr_t p_fsm, sbuf_ae_t *p_sbuf_ae )
{
    if ( p_sbuf_ae->frame_id == p_fsm->frame_id_tracking ) {
        LOG( LOG_ERR, "Error: Same frame used twice, frame_id: %u.", p_sbuf_ae->frame_id );
        return;
    }

    p_fsm->new_exposure_log2 = p_sbuf_ae->ae_exposure;
    p_fsm->new_exposure_ratio = p_sbuf_ae->ae_exposure_ratio;
    p_fsm->frame_id_tracking = p_sbuf_ae->frame_id;
    p_fsm->ae_hist_mean = p_sbuf_ae->ae_hist_mean;
    p_fsm->max_target = p_sbuf_ae->max_target;
    p_fsm->state = p_sbuf_ae->state;

    if ( p_fsm->daynight != p_sbuf_ae->day_night_light ) {
        p_fsm->daynight = p_sbuf_ae->day_night_light;
        acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_SENSOR_SENSOR_IR_CUT, &p_fsm->daynight, sizeof( uint32_t ) );
    }

    if ( p_fsm->frame_id_tracking ) {
        fsm_param_mon_alg_flow_t ae_flow;
        ae_flow.frame_id_tracking = p_fsm->frame_id_tracking;
        ae_flow.frame_id_current = acamera_fsm_util_get_cur_frame_id( &p_fsm->cmn );
        ae_flow.flow_state = MON_ALG_FLOW_STATE_OUTPUT_READY;
        acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_MON_AE_FLOW, &ae_flow, sizeof( ae_flow ) );

        LOG( LOG_DEBUG, "AE flow: OUTPUT_READY: frame_id_tracking: %d, cur frame_id: %u.", ae_flow.frame_id_tracking, ae_flow.frame_id_current );
    }

    fsm_raise_event( p_fsm, event_id_ae_result_ready );
}

static inline uint32_t full_ratio_to_adjaced( const fsm_param_sensor_info_t *sensor_info, uint32_t ratio )
{
    switch ( sensor_info->sensor_exp_number ) {
    case 4:
        return acamera_math_exp2( acamera_log2_fixed_to_fixed( ratio, 6, 8 ) / 3, 8, 6 ) >> 6;
        break;
    case 3:
        return acamera_sqrt32( ratio >> 6 );
        break;
    default:
    case 2:
        return ratio >> 6;
        break;
    }
}

int ae_calculate_exposure( AE_fsm_ptr_t p_fsm )
{
    int rc = 0;
    fsm_param_sensor_info_t sensor_info;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

    fsm_param_exposure_target_t exp_target;
    exp_target.exposure_log2 = p_fsm->new_exposure_log2;
    exp_target.exposure_ratio = p_fsm->new_exposure_ratio;
    exp_target.frame_id_tracking = p_fsm->frame_id_tracking;

    ACAMERA_FSM2CTX_PTR( p_fsm )
        ->stab.global_exposure_ratio = full_ratio_to_adjaced( &sensor_info, p_fsm->new_exposure_ratio );

    /* CMOS will skip exposure set operation in some conditions, so we need to check the apply result, ret 0 menas succeed  */
    if ( !acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_EXPOSURE_TARGET, &exp_target, sizeof( exp_target ) ) ) {
        p_fsm->exposure_log2 = p_fsm->new_exposure_log2;
        p_fsm->exposure_ratio = p_fsm->new_exposure_ratio;

        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_exposure = ( acamera_math_exp2( p_fsm->new_exposure_log2, LOG2_GAIN_SHIFT, 6 ) );

        // indicate we have new AE parameter, and should post event.
        rc = 1;

        LOG( LOG_INFO, "AE applied OK, exposure_log2 : %d, exposure_ratio: %u.", exp_target.exposure_log2, exp_target.exposure_ratio );
    } else {
        LOG( LOG_ERR, "AE applied failed, exposure_log2 : %d, exposure_ratio: %u.", exp_target.exposure_log2, exp_target.exposure_ratio );
    }

    return rc;
}
