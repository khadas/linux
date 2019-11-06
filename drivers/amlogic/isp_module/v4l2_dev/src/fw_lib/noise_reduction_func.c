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
#include "acamera_firmware_config.h"
#include "noise_reduction_fsm.h"
#include "cmos_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_NOISE_REDUCTION
#endif

#if ISP_HAS_SINTER_RADIAL_LUT

static void sinter_load_radial_lut( noise_reduction_fsm_t *p_fsm )
{
    const uint8_t *p_sinter_radial_lut = (uint8_t *)_GET_UCHAR_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SINTER_RADIAL_LUT );
    // params must be in the following format : rm_enable, rm_centre_x, rm_centre_y, rm_off_centre_mult,
    const uint16_t *p_sinter_radial_params = (uint16_t *)_GET_USHORT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SINTER_RADIAL_PARAMS );
    int i;
    uint8_t number_of_nodes = _GET_LEN( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SINTER_RADIAL_LUT );

    for ( i = 0; i < number_of_nodes; ++i ) {
        acamera_isp_sinter_shading_rm_shading_lut_write( p_fsm->cmn.isp_base, i, p_sinter_radial_lut[i] );
    }

    acamera_isp_sinter_rm_enable_write( p_fsm->cmn.isp_base, p_sinter_radial_params[0] );
    acamera_isp_sinter_rm_center_x_write( p_fsm->cmn.isp_base, p_sinter_radial_params[1] );
    acamera_isp_sinter_rm_center_y_write( p_fsm->cmn.isp_base, p_sinter_radial_params[2] );
    acamera_isp_sinter_rm_off_center_mult_write( p_fsm->cmn.isp_base, p_sinter_radial_params[3] );
}

#endif

void dynamic_dpc_strength_calculate( noise_reduction_fsm_t *p_fsm )
{
    int32_t total_gain;

    // do not update registers if in manual mode
    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_raw_frontend ) {
        return;
    }

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_TOTAL_GAIN, NULL, 0, &total_gain, sizeof( total_gain ) );

    uint16_t log2_gain = total_gain >> ( LOG2_GAIN_SHIFT - 8 );

    uint16_t dp_slope = acamera_isp_raw_frontend_dp_slope_read( p_fsm->cmn.isp_base );
    uint16_t dp_threshold = acamera_isp_raw_frontend_dp_threshold_read( p_fsm->cmn.isp_base );
    {
        uint32_t dp_slope_table_idx = CALIBRATION_DP_SLOPE;
        uint32_t dp_threshold_table_idx = CALIBRATION_DP_THRESHOLD;

        modulation_entry_t *dp_slope_table = _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), dp_slope_table_idx );
        uint32_t dp_slope_table_len = _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), dp_slope_table_idx );
        modulation_entry_t *dp_threshold_table = _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), dp_threshold_table_idx );
        uint32_t dp_threshold_table_len = _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), dp_threshold_table_idx );

        dp_slope = acamera_calc_modulation_u16( log2_gain, dp_slope_table, dp_slope_table_len );
        dp_threshold = acamera_calc_modulation_u16( log2_gain, dp_threshold_table, dp_threshold_table_len );
    }
    acamera_isp_raw_frontend_dp_slope_write( p_fsm->cmn.isp_base, dp_slope );
    acamera_isp_raw_frontend_dp_threshold_write( p_fsm->cmn.isp_base, dp_threshold );
}


void stitching_error_calculate( noise_reduction_fsm_t *p_fsm )
{
    // do not update frame stitch registers if in manual mode
    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_frame_stitch ) {
        return;
    }
    /*
-------------------------------------------------------
        Modulation for frame stitching 
-------------------------------------------------------
*/
    int32_t total_gain = 0;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_TOTAL_GAIN, NULL, 0, &total_gain, sizeof( total_gain ) );

    uint16_t log2_gain = total_gain >> ( LOG2_GAIN_SHIFT - 8 );
    //long medium motion
    uint16_t lm_np = acamera_calc_modulation_u16( log2_gain, (modulation_entry_t *)_GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STITCHING_LM_NP ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STITCHING_LM_NP ) );
    uint16_t lm_mov_mult = acamera_calc_modulation_u16( log2_gain, (modulation_entry_t *)_GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STITCHING_LM_MOV_MULT ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STITCHING_LM_MOV_MULT ) );
    //medium short motion
    uint16_t ms_np = acamera_calc_modulation_u16( log2_gain, (modulation_entry_t *)_GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STITCHING_MS_NP ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STITCHING_MS_NP ) );
    uint16_t ms_mov_mult = acamera_calc_modulation_u16( log2_gain, (modulation_entry_t *)_GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STITCHING_MS_MOV_MULT ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STITCHING_MS_MOV_MULT ) );
    //short very short motion
    uint16_t svs_np = acamera_calc_modulation_u16( log2_gain, (modulation_entry_t *)_GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STITCHING_SVS_NP ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STITCHING_SVS_NP ) );
    uint16_t svs_mov_mult = acamera_calc_modulation_u16( log2_gain, (modulation_entry_t *)_GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STITCHING_SVS_MOV_MULT ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STITCHING_SVS_MOV_MULT ) );


    //change to MC off mode when gain is higher than gain_log2 value found in calibration
    uint16_t MC_off_enable_gain = _GET_UINT_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_FS_MC_OFF )[0];

    if ( log2_gain > MC_off_enable_gain ) {
        acamera_isp_frame_stitch_mcoff_mode_enable_write( p_fsm->cmn.isp_base, 1 );
        // printf("%d %d %d\n",(int)log2_gain, 1,(int)MC_off_enable_gain );
    } else if ( log2_gain <= MC_off_enable_gain ) {
        acamera_isp_frame_stitch_mcoff_mode_enable_write( p_fsm->cmn.isp_base, 0 );
        // printf("%d %d %d\n",(int)log2_gain, 0,(int)MC_off_enable_gain );
    }

    uint16_t stitching_lm_med_noise_intensity_thresh = acamera_calc_modulation_u16( log2_gain, (modulation_entry_t *)_GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STITCHING_LM_MED_NOISE_INTENSITY ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_STITCHING_LM_MED_NOISE_INTENSITY ) );

#if ISP_WDR_SWITCH

    uint32_t wdr_mode = 0;
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );
    if ( WDR_MODE_FS_LIN == wdr_mode ) {
        fsm_param_sensor_info_t sensor_info;
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

        switch ( sensor_info.sensor_exp_number ) {
        case 4:
            acamera_isp_frame_stitch_lm_np_mult_write( p_fsm->cmn.isp_base, lm_np );
            acamera_isp_frame_stitch_lm_alpha_mov_slope_write( p_fsm->cmn.isp_base, lm_mov_mult );
            acamera_isp_frame_stitch_ms_np_mult_write( p_fsm->cmn.isp_base, ms_np );
            acamera_isp_frame_stitch_ms_alpha_mov_slope_write( p_fsm->cmn.isp_base, ms_mov_mult );
            acamera_isp_frame_stitch_svs_np_mult_write( p_fsm->cmn.isp_base, svs_np );
            acamera_isp_frame_stitch_svs_alpha_mov_slope_write( p_fsm->cmn.isp_base, svs_mov_mult );
            acamera_isp_frame_stitch_lm_med_noise_intensity_thresh_write( p_fsm->cmn.isp_base, stitching_lm_med_noise_intensity_thresh );
            break;

        case 3:
            acamera_isp_frame_stitch_lm_np_mult_write( p_fsm->cmn.isp_base, lm_np );
            acamera_isp_frame_stitch_lm_alpha_mov_slope_write( p_fsm->cmn.isp_base, lm_mov_mult );
            acamera_isp_frame_stitch_ms_np_mult_write( p_fsm->cmn.isp_base, ms_np );
            acamera_isp_frame_stitch_ms_alpha_mov_slope_write( p_fsm->cmn.isp_base, ms_mov_mult );
            acamera_isp_frame_stitch_lm_med_noise_intensity_thresh_write( p_fsm->cmn.isp_base, stitching_lm_med_noise_intensity_thresh );
            break;

        case 2:
            acamera_isp_frame_stitch_lm_np_mult_write( p_fsm->cmn.isp_base, lm_np );
            acamera_isp_frame_stitch_lm_alpha_mov_slope_write( p_fsm->cmn.isp_base, lm_mov_mult );
            acamera_isp_frame_stitch_lm_med_noise_intensity_thresh_write( p_fsm->cmn.isp_base, stitching_lm_med_noise_intensity_thresh );
            break;

        default:
            LOG( LOG_ERR, "Unsupported exposures number: %d", sensor_info.sensor_exp_number );
            break;
        }
    }
#endif
}

void sinter_strength_calculate( noise_reduction_fsm_t *p_fsm )
{
    uint32_t cmos_exp_ratio = 0;
    uint32_t wdr_mode = 0;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_EXPOSURE_RATIO, NULL, 0, &cmos_exp_ratio, sizeof( cmos_exp_ratio ) );
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );
    uint32_t snr_thresh_master;

    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_sinter == 0 ) //  Do not update values if manual mode
    {
        // get lut indexes for the current hdr mode
        uint32_t sinter_strength_idx = CALIBRATION_SINTER_STRENGTH;
        uint32_t sinter_strength1_idx = CALIBRATION_SINTER_STRENGTH1;
        uint32_t sinter_thresh1_idx = CALIBRATION_SINTER_THRESH1;
        uint32_t sinter_thresh4_idx = CALIBRATION_SINTER_THRESH4;
        uint32_t sinter_int_config_idx = CALIBRATION_SINTER_INTCONFIG;
        int32_t total_gain = 0;

        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_TOTAL_GAIN, NULL, 0, &total_gain, sizeof( total_gain ) );

        uint16_t log2_gain = total_gain >> ( LOG2_GAIN_SHIFT - 8 );

        // LOG( LOG_ERR, "log2_gain %d total_gain %d", log2_gain, total_gain );
        snr_thresh_master = acamera_calc_modulation_u16( log2_gain, _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), sinter_strength_idx ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), sinter_strength_idx ) );
#if ( defined( ISP_HAS_IRIDIX8_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM ) ) && defined( CALIBRATION_SINTER_STRENGTH_MC_CONTRAST ) //CHECK LOGIC IS CORRECT
        uint32_t sinter_strength_mc_contrast_idx = CALIBRATION_SINTER_STRENGTH_MC_CONTRAST;
        // Adjust strength according to contrast

        uint32_t iridix_contrast = 256; //remove the 8 bit presicion?
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_IRIDIX_CONTRAST, NULL, 0, &iridix_contrast, sizeof( iridix_contrast ) );

        iridix_contrast = iridix_contrast >> 8;
        uint32_t snr_thresh_master_contrast = acamera_calc_modulation_u16( iridix_contrast, _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), sinter_strength_mc_contrast_idx ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), sinter_strength_mc_contrast_idx ) );
        if ( snr_thresh_master_contrast > 0xFF )
            snr_thresh_master_contrast = 0xFF;
        //it will only affect short exposure
        p_fsm->snr_thresh_contrast = snr_thresh_master_contrast;

#endif

        if ( snr_thresh_master > 0xFF )
            snr_thresh_master = 0xFF;

        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_sinter_threshold_target = ( snr_thresh_master );
        uint16_t sinter_strenght1 = acamera_calc_modulation_u16( log2_gain, _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), sinter_strength1_idx ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), sinter_strength1_idx ) );
        // LOG( LOG_CRIT, "sinter_strenght1 %d log2_gain %d ", (int)sinter_strenght1, (int)log2_gain );
        acamera_isp_sinter_strength_1_write( p_fsm->cmn.isp_base, sinter_strenght1 );
        uint16_t sinter_thresh1 = acamera_calc_modulation_u16( log2_gain, _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), sinter_thresh1_idx ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), sinter_thresh1_idx ) );
        acamera_isp_sinter_thresh_1h_write( p_fsm->cmn.isp_base, sinter_thresh1 );
        acamera_isp_sinter_thresh_1v_write( p_fsm->cmn.isp_base, sinter_thresh1 );
        uint16_t sinter_thresh4 = acamera_calc_modulation_u16( log2_gain, _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), sinter_thresh4_idx ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), sinter_thresh4_idx ) );
        acamera_isp_sinter_thresh_4h_write( p_fsm->cmn.isp_base, sinter_thresh4 );
        acamera_isp_sinter_thresh_4v_write( p_fsm->cmn.isp_base, sinter_thresh4 );

        uint16_t sinter_int_config = acamera_calc_modulation_u16( log2_gain, _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), sinter_int_config_idx ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), sinter_int_config_idx ) );
        acamera_isp_sinter_int_config_write( p_fsm->cmn.isp_base, sinter_int_config );

        if ( acamera_isp_isp_global_parameter_status_sinter_version_read( p_fsm->cmn.isp_base ) ) { //sinter 3 is used
            int sinter_sad_inx = CALIBRATION_SINTER_SAD;
            acamera_isp_sinter_sad_filt_thresh_write( p_fsm->cmn.isp_base, acamera_calc_modulation_u16( log2_gain, _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), sinter_sad_inx ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), sinter_sad_inx ) ) );
        }
    } else {
        snr_thresh_master = ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_sinter_threshold_target;
    }

    p_fsm->snr_thresh_master = snr_thresh_master;
}

void temper_strength_calculate( noise_reduction_fsm_t *p_fsm )
{
    uint32_t cmos_exp_ratio = 0;
    uint32_t wdr_mode = 0;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_EXPOSURE_RATIO, NULL, 0, &cmos_exp_ratio, sizeof( cmos_exp_ratio ) );
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );

    uint32_t tnr_thresh_master;
    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_temper == 0 ) //  Do not update values if manual mode
    {
        int32_t total_gain = 0;
        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_TOTAL_GAIN, NULL, 0, &total_gain, sizeof( total_gain ) );
        uint16_t log2_gain = total_gain >> ( LOG2_GAIN_SHIFT - 8 );
        //this drives global offset
        tnr_thresh_master = acamera_calc_modulation_u16( log2_gain, _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_TEMPER_STRENGTH ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_TEMPER_STRENGTH ) );
        ACAMERA_FSM2CTX_PTR( p_fsm )
            ->stab.global_temper_threshold_target = ( tnr_thresh_master );

        switch ( wdr_mode ) {
        case WDR_MODE_FS_LIN: {

            fsm_param_sensor_info_t sensor_info;
            acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );

            exposure_data_set_t exp_write_set;
            acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_EXP_WRITE_SET, NULL, 0, &exp_write_set, sizeof( exp_write_set ) );

            switch ( sensor_info.sensor_exp_number ) {

            case 4: {
                const uint32_t SVS_exp_ratio = exp_write_set.exposure_ratio_short;  //Usomething.6
                const uint32_t MS_exp_ratio = exp_write_set.exposure_ratio_medium;  //Usomething.6
                const uint32_t LM_exp_ratio = exp_write_set.exposure_ratio_medium2; //Usomething.6

                // Noise level: 4:1
                // noise level 0 => (very short) = 64:
                // noise level 1 => (short)  = 64 - 8 * log2(svs_expoosure ratio)
                // noise level 2 => (medium) = 64 - 8 * log2(svs_expoosure ratio*sm_expoosure ratio)
                // noise level 3 => (long)   = 64 - 8 * log2(svs_expoosure ratio*sm_expoosure ratio*lm_expoosure ratio)

                int8_t short_thresh = 64 - ( 8 * acamera_log2_int_to_fixed( SVS_exp_ratio >> 6, 8, 0 ) >> 8 );
                int8_t medium_thresh = 64 - ( 8 * acamera_log2_int_to_fixed( ( SVS_exp_ratio * MS_exp_ratio ) >> 12, 8, 0 ) >> 8 );
                int8_t long_thresh = 64 - ( 8 * acamera_log2_int_to_fixed( ( (uint64_t)SVS_exp_ratio * (uint64_t)MS_exp_ratio * (uint64_t)LM_exp_ratio ) >> 18, 8, 0 ) >> 8 );

                if ( short_thresh < 0 ) {
                    short_thresh = 0;
                }
                if ( medium_thresh < 0 ) {
                    medium_thresh = 0;
                }
                if ( long_thresh < 0 ) {
                    long_thresh = 0;
                }

                acamera_isp_temper_noise_profile_noise_level_0_write( p_fsm->cmn.isp_base, 64 );
                acamera_isp_temper_noise_profile_noise_level_1_write( p_fsm->cmn.isp_base, short_thresh );
                acamera_isp_temper_noise_profile_noise_level_2_write( p_fsm->cmn.isp_base, medium_thresh );
                acamera_isp_temper_noise_profile_noise_level_3_write( p_fsm->cmn.isp_base, long_thresh );


                uint32_t mult = ( 2 << 17 ) / SVS_exp_ratio; //u1.11
                if ( mult > 2048 ) {
                    mult = 2048;
                }
                uint32_t FS_thresh = acamera_isp_frame_stitch_svs_thresh_low_read( p_fsm->cmn.isp_base );
                FS_thresh = ( FS_thresh * mult ) << 3;
                uint32_t FS_thresh_sqrt_svs = acamera_sqrt32( FS_thresh ) + acamera_isp_sqrt_black_level_out_read( p_fsm->cmn.isp_base );


                mult = ( 2 << 17 ) / ( ( SVS_exp_ratio * MS_exp_ratio ) >> 6 ); //u1.11
                if ( mult > 2048 ) {
                    mult = 2048;
                }
                FS_thresh = acamera_isp_frame_stitch_ms_thresh_low_read( p_fsm->cmn.isp_base );
                FS_thresh = ( FS_thresh * mult ) << 3;
                uint32_t FS_thresh_sqrt_ms = acamera_sqrt32( FS_thresh ) + acamera_isp_sqrt_black_level_out_read( p_fsm->cmn.isp_base );


                mult = ( 2 << 17 ) / ( ( LM_exp_ratio * MS_exp_ratio * SVS_exp_ratio ) >> 12 ); //u1.11
                if ( mult > 2048 ) {
                    mult = 2048;
                }
                FS_thresh = acamera_isp_frame_stitch_lm_thresh_low_read( p_fsm->cmn.isp_base );
                FS_thresh = ( FS_thresh * mult ) << 3;
                uint32_t FS_thresh_sqrt_lm = acamera_sqrt32( FS_thresh ) + acamera_isp_sqrt_black_level_out_read( p_fsm->cmn.isp_base );

                acamera_isp_temper_noise_profile_thresh3_write( p_fsm->cmn.isp_base, FS_thresh_sqrt_svs );
                acamera_isp_temper_noise_profile_thresh2_write( p_fsm->cmn.isp_base, FS_thresh_sqrt_ms );
                acamera_isp_temper_noise_profile_thresh1_write( p_fsm->cmn.isp_base, FS_thresh_sqrt_lm );

            } break;

            case 3: {
                //if WDR mode: intensity thresholds and noise profile for different exposures need to be updated according to exposure ratio

                const uint32_t MS_exp_ratio = exp_write_set.exposure_ratio_short;  //Usomething.6
                const uint32_t LM_exp_ratio = exp_write_set.exposure_ratio_medium; //Usomething.6

                // Noise level: 3:1
                // noise level 0 => (short)  = 64:
                // noise level 1 => (medium) = 64 - 8 * log2(sm_expoosure ratio)
                // noise level 2 => (long)   = 64 - 8 * log2(sm_expoosure ratio*lm_expoosure ratio)

                int8_t medium_thresh = 64 - ( 8 * acamera_log2_int_to_fixed( MS_exp_ratio >> 6, 8, 0 ) >> 8 );
                int8_t long_thresh = 64 - ( 8 * acamera_log2_int_to_fixed( ( LM_exp_ratio * MS_exp_ratio ) >> 12, 8, 0 ) >> 8 );
                if ( medium_thresh < 0 ) {
                    medium_thresh = 0;
                }
                if ( long_thresh < 0 ) {
                    long_thresh = 0;
                }


                //LOG( LOG_CRIT, "%d ",(int) p_fsm->snr_thresh_contrast);
                acamera_isp_temper_noise_profile_noise_level_0_write( p_fsm->cmn.isp_base, 64 );
                acamera_isp_temper_noise_profile_noise_level_1_write( p_fsm->cmn.isp_base, medium_thresh );
                acamera_isp_temper_noise_profile_noise_level_2_write( p_fsm->cmn.isp_base, long_thresh );
                acamera_isp_temper_noise_profile_noise_level_3_write( p_fsm->cmn.isp_base, 64 );

                // DW_Out = 20;
                // DW_In = 12;
                // ExpRatio = [1:256] * 64;
                // FS_thresh = 3500;
                // % 2 : Compute scaling values
                // DW_Inc = DW_Out - DW_In;
                // mult = min( 2048, fix( ( 2 ^ 17 )./ ExpRatio ) ); % 1.11
                // % 3 : Scale parameters
                // FS_thresh = bitshift( FS_thresh * mult, DW_Inc - 11 ); % 0.DW_Out
                // FS_thresh_sqrt = round( sqrt( FS_thresh * 4096 ) );
                uint32_t mult;
                uint32_t FS_thresh;
                uint32_t FS_thresh_sqrt_ms, FS_thresh_sqrt_lm;

                mult = ( 2 << 17 ) / MS_exp_ratio; //u1.11
                // if ( mult < 2048 ) { mult = 2048;  }
                FS_thresh = acamera_isp_frame_stitch_ms_thresh_low_read( p_fsm->cmn.isp_base );
                // LOG( LOG_CRIT, " 1 FS_thresh %d MS_exp_ratio %d mult %d \n", (int)FS_thresh, (int)MS_exp_ratio, (int)mult );
                if ( mult > 2048 ) {
                    mult = 2048;
                }
                FS_thresh = ( FS_thresh * mult ) << 3;
                FS_thresh_sqrt_ms = acamera_sqrt32( FS_thresh ) + acamera_isp_sqrt_black_level_out_read( p_fsm->cmn.isp_base );
                // LOG(LOG_CRIT, " 1 FS_thresh %d FS_thresh_sqrt_ms %d mult %d \n", (int)FS_thresh, (int)FS_thresh_sqrt_ms, (int)mult );

                mult = ( 2 << 17 ) / ( ( LM_exp_ratio * MS_exp_ratio ) >> 6 ); //u1.11
                if ( mult > 2048 ) {
                    mult = 2048;
                }
                FS_thresh = acamera_isp_frame_stitch_lm_thresh_low_read( p_fsm->cmn.isp_base );
                FS_thresh = ( FS_thresh * mult ) << 3;
                FS_thresh_sqrt_lm = acamera_sqrt32( FS_thresh ) + acamera_isp_sqrt_black_level_out_read( p_fsm->cmn.isp_base );

                // LOG(LOG_CRIT, " 2 FS_thresh %d FS_thresh_sqrt_lm %d mult %d", (int)FS_thresh, (int)FS_thresh_sqrt_lm, (int)mult );

                acamera_isp_temper_noise_profile_thresh3_write( p_fsm->cmn.isp_base, FS_thresh_sqrt_ms );
                acamera_isp_temper_noise_profile_thresh2_write( p_fsm->cmn.isp_base, FS_thresh_sqrt_lm );
                acamera_isp_temper_noise_profile_thresh1_write( p_fsm->cmn.isp_base, 0 );
            } break;

            case 2: {
                // Noise level : 2:1
                // noise level 0 => (short)  = 64:
                // noise level 1 => (long)   = 64 - 8 * log2(lm_expoosure ratio)

                int8_t long_thresh = 64 - ( 8 * acamera_log2_int_to_fixed( exp_write_set.exposure_ratio >> 6, 8, 0 ) >> 8 );
                if ( long_thresh < 0 ) {
                    long_thresh = 0;
                }

                //LOG( LOG_CRIT, "%d ",(int) p_fsm->snr_thresh_contrast);
                acamera_isp_temper_noise_profile_noise_level_0_write( p_fsm->cmn.isp_base, 64 );
                acamera_isp_temper_noise_profile_noise_level_1_write( p_fsm->cmn.isp_base, long_thresh );
                acamera_isp_temper_noise_profile_noise_level_2_write( p_fsm->cmn.isp_base, 64 );
                acamera_isp_temper_noise_profile_noise_level_3_write( p_fsm->cmn.isp_base, 64 );

                // DW_Out = 20;
                // DW_In = 12;
                // ExpRatio = [1:256] * 64;
                // FS_thresh = 3500;
                // % 2 : Compute scaling values
                // DW_Inc = DW_Out - DW_In;
                // mult = min( 2048, fix( ( 2 ^ 17 )./ ExpRatio ) ); % 1.11
                // % 3 : Scale parameters
                // FS_thresh = bitshift( FS_thresh * mult, DW_Inc - 11 ); % 0.DW_Out
                // FS_thresh_sqrt = round( sqrt( FS_thresh * 4096 ) );
                uint32_t mult;
                uint32_t FS_thresh;
                uint32_t FS_thresh_sqrt_lm;

                mult = ( 2 << 17 ) / exp_write_set.exposure_ratio; //u1.11
                if ( mult > 2048 ) {
                    mult = 2048;
                }
                FS_thresh = acamera_isp_frame_stitch_lm_thresh_low_read( p_fsm->cmn.isp_base );
                FS_thresh = ( FS_thresh * mult ) << 3;
                FS_thresh_sqrt_lm = acamera_sqrt32( FS_thresh ) + acamera_isp_sqrt_black_level_out_read( p_fsm->cmn.isp_base );

                acamera_isp_temper_noise_profile_thresh3_write( p_fsm->cmn.isp_base, FS_thresh_sqrt_lm );
                acamera_isp_temper_noise_profile_thresh2_write( p_fsm->cmn.isp_base, 0 );
                acamera_isp_temper_noise_profile_thresh1_write( p_fsm->cmn.isp_base, 0 );

            } break;

            default: {
                LOG( LOG_ERR, "Unsupported exposures number: %d", sensor_info.sensor_exp_number );
            } break;
            }
            break;
        }
        case WDR_MODE_LINEAR:
            acamera_isp_temper_noise_profile_noise_level_0_write( p_fsm->cmn.isp_base, 64 );
            acamera_isp_temper_noise_profile_thresh3_write( p_fsm->cmn.isp_base, 0 );
            acamera_isp_temper_noise_profile_thresh2_write( p_fsm->cmn.isp_base, 0 );
            acamera_isp_temper_noise_profile_thresh1_write( p_fsm->cmn.isp_base, 0 );
            break;

        default:
            break;
        }
    } else {
        tnr_thresh_master = ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_temper_threshold_target;
    }


    if ( tnr_thresh_master > 0xFF )
        tnr_thresh_master = 0xFF;
    p_fsm->tnr_thresh_master = tnr_thresh_master;
}

void noise_reduction_hw_init( noise_reduction_fsm_t *p_fsm )
{
#if ISP_HAS_SINTER_RADIAL_LUT
    sinter_load_radial_lut( p_fsm );
#endif
}

void noise_reduction_initialize( noise_reduction_fsm_t *p_fsm )
{
    p_fsm->tnr_strength_target = 15;
    p_fsm->snr_strength_target = 15;
    p_fsm->tnr_thresh_master = p_fsm->tnr_strength_target; // same as target
    p_fsm->snr_thresh_master = p_fsm->snr_strength_target; // same as target
}

void noise_reduction_update( noise_reduction_fsm_t *p_fsm )
{
    int32_t total_gain = 0;
    uint32_t wdr_mode = 0;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );
    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_TOTAL_GAIN, NULL, 0, &total_gain, sizeof( total_gain ) );

    uint16_t log2_gain = total_gain >> ( LOG2_GAIN_SHIFT - 8 );
    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_demosaic == 0 ) {
        int tbl_inx = CALIBRATION_DEMOSAIC_NP_OFFSET;
        acamera_isp_demosaic_rgb_np_offset_write( p_fsm->cmn.isp_base, acamera_calc_modulation_u16( log2_gain, _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), tbl_inx ), _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), tbl_inx ) ) );
    }

    //  Do not update values if manual mode
    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_cnr == 0 ) {
        const modulation_entry_t *cnr_uv_delta12_slope = _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CNR_UV_DELTA12_SLOPE );
        uint32_t cnr_uv_delta12_slope_len = _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_CNR_UV_DELTA12_SLOPE );
        uint16_t uv_delta_slope = acamera_calc_modulation_u16( log2_gain, cnr_uv_delta12_slope, cnr_uv_delta12_slope_len );
        acamera_isp_cnr_uv_delta1_slope_write( p_fsm->cmn.isp_base, uv_delta_slope );
        acamera_isp_cnr_uv_delta2_slope_write( p_fsm->cmn.isp_base, uv_delta_slope );
    }

    if ( wdr_mode == WDR_MODE_FS_LIN ) {
        stitching_error_calculate( p_fsm );
    }
    //////////////////////////////////////////////////

    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_sinter == 0 ) {
        sinter_strength_calculate( p_fsm );

        uint32_t exp_ratio = 0;

        acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_EXPOSURE_RATIO, NULL, 0, &exp_ratio, sizeof( exp_ratio ) );

        if ( exp_ratio != 0 && wdr_mode == WDR_MODE_FS_LIN ) {
            exposure_data_set_t exp_write_set;
            acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_EXP_WRITE_SET, NULL, 0, &exp_write_set, sizeof( exp_write_set ) );

            fsm_param_sensor_info_t sensor_info;
            acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_INFO, NULL, 0, &sensor_info, sizeof( sensor_info ) );
            switch ( sensor_info.sensor_exp_number ) {
            case 4: {
                // Noise level: 4:1
                // noise level 0 => (very short) = 64:
                // noise level 1 => (short)  = 64 - 8 * log2(svs_expoosure ratio)
                // noise level 2 => (medium) = 64 - 8 * log2(svs_expoosure ratio*sm_expoosure ratio)
                // noise level 3 => (long)   = 64 - 8 * log2(svs_expoosure ratio*sm_expoosure ratio*lm_expoosure ratio)

                int8_t short_thresh = 64 - ( 8 * acamera_log2_int_to_fixed( exp_write_set.exposure_ratio_short >> 6, 8, 0 ) >> 8 );
                int8_t medium_thresh = 64 - ( 8 * acamera_log2_int_to_fixed( ( exp_write_set.exposure_ratio_short * exp_write_set.exposure_ratio_medium ) >> 12, 8, 0 ) >> 8 );
                int8_t long_thresh = 64 - ( 8 * acamera_log2_int_to_fixed( ( (uint64_t)exp_write_set.exposure_ratio_short * (uint64_t)exp_write_set.exposure_ratio_medium * (uint64_t)exp_write_set.exposure_ratio_medium2 ) >> 18, 8, 0 ) >> 8 );

                if ( short_thresh < 0 ) {
                    short_thresh = 0;
                }
                if ( medium_thresh < 0 ) {
                    medium_thresh = 0;
                }
                if ( long_thresh < 0 ) {
                    long_thresh = 0;
                }

                acamera_isp_sinter_noise_profile_noise_level_0_write( p_fsm->cmn.isp_base, 64 + ( p_fsm->snr_thresh_contrast ) );
                acamera_isp_sinter_noise_profile_noise_level_1_write( p_fsm->cmn.isp_base, short_thresh );
                acamera_isp_sinter_noise_profile_noise_level_2_write( p_fsm->cmn.isp_base, medium_thresh );
                acamera_isp_sinter_noise_profile_noise_level_3_write( p_fsm->cmn.isp_base, long_thresh );
            } break;

            case 3: {
                // Noise level: 3:1
                // noise level 0 => (short)  = 64:
                // noise level 1 => (medium) = 64 - 8 * log2(sm_expoosure ratio)
                // noise level 2 => (long)   = 64 - 8 * log2(sm_expoosure ratio*lm_expoosure ratio)

                uint32_t MS_exp_ratio = exp_write_set.exposure_ratio_short;  //Usomething.6
                uint32_t LM_exp_ratio = exp_write_set.exposure_ratio_medium; //Usomething.6

                int8_t medium_thresh = 64 - ( 8 * acamera_log2_int_to_fixed( MS_exp_ratio >> 6, 8, 0 ) >> 8 );
                int8_t long_thresh = 64 - ( 8 * acamera_log2_int_to_fixed( ( LM_exp_ratio * MS_exp_ratio ) >> 12, 8, 0 ) >> 8 );
                if ( medium_thresh < 0 ) {
                    medium_thresh = 0;
                }
                if ( long_thresh < 0 ) {
                    long_thresh = 0;
                }
                acamera_isp_sinter_noise_profile_noise_level_0_write( p_fsm->cmn.isp_base, 64 + ( p_fsm->snr_thresh_contrast ) );
                acamera_isp_sinter_noise_profile_noise_level_1_write( p_fsm->cmn.isp_base, medium_thresh );
                acamera_isp_sinter_noise_profile_noise_level_2_write( p_fsm->cmn.isp_base, long_thresh );
            } break;

            case 2: {
                // Noise level : 2:1
                // noise level 0 => (short)  = 64:
                // noise level 1 => (long)   = 64 - 8 * log2(lm_expoosure ratio)

                int8_t long_thresh = 64 - ( 8 * acamera_log2_int_to_fixed( exp_write_set.exposure_ratio >> 6, 8, 0 ) >> 8 );
                if ( long_thresh < 0 ) {
                    long_thresh = 0;
                }
                acamera_isp_sinter_noise_profile_noise_level_0_write( p_fsm->cmn.isp_base, 64 + ( p_fsm->snr_thresh_contrast ) );
                acamera_isp_sinter_noise_profile_noise_level_1_write( p_fsm->cmn.isp_base, long_thresh );
            } break;

            default: {
                LOG( LOG_ERR, "Unsupported exposures number: %d", sensor_info.sensor_exp_number );
            } break;
            }
        }
        if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_sinter == 0 ) //  Do not update values if manual mode
        {
            acamera_isp_sinter_noise_profile_global_offset_write( p_fsm->cmn.isp_base, ( uint8_t )( p_fsm->snr_thresh_master > 255 ? 255 : p_fsm->snr_thresh_master ) );
        }
    }
    dynamic_dpc_strength_calculate( p_fsm );

    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_temper == 0 ) {
        temper_strength_calculate( p_fsm );
        acamera_isp_temper_noise_profile_global_offset_write( p_fsm->cmn.isp_base, ( uint8_t )( p_fsm->tnr_thresh_master > 255 ? 255 : p_fsm->tnr_thresh_master ) );
    }
}
