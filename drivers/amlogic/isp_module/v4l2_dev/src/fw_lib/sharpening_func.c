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
#include "acamera_math.h"
#include "acamera_command_api.h"
#include "sharpening_fsm.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_SHARPENING
#endif

#define VIDEO_OUT_WIDTH 10
void sharpening_initialize( sharpening_fsm_t *p_fsm )
{
}


void sharpening_update( sharpening_fsm_t *p_fsm )
{
    int32_t total_gain = 0;

    acamera_fsm_mgr_get_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_TOTAL_GAIN, NULL, 0, &total_gain, sizeof( total_gain ) );

    uint16_t log2_gain = total_gain >> ( LOG2_GAIN_SHIFT - 8 );
    uint16_t alt_d, alt_ud;
    uint16_t alt_du = 0;

    const modulation_entry_t *sharp_alt_d_table_ptr;
    const modulation_entry_t *sharp_alt_ud_table_ptr;
    modulation_entry_t *sharp_alt_du_table_ptr;

#if ISP_WDR_SWITCH
    uint32_t sharp_alt_d_idx = CALIBRATION_SHARP_ALT_D;
    uint32_t sharp_alt_ud_idx = CALIBRATION_SHARP_ALT_UD;
    sharp_alt_d_table_ptr = _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), sharp_alt_d_idx );
    sharp_alt_ud_table_ptr = _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), sharp_alt_ud_idx );
    uint32_t sharp_alt_du_idx = CALIBRATION_SHARP_ALT_DU;
    sharp_alt_du_table_ptr = _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), sharp_alt_du_idx );

#else //ISP_WDR_SWITCH

    uint32_t sharp_alt_du_idx = CALIBRATION_SHARP_ALT_DU;
    sharp_alt_du_table_ptr = _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), sharp_alt_du_idx );
    sharp_alt_d_table_ptr = ( _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SHARP_ALT_D ) );
    sharp_alt_ud_table_ptr = ( _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SHARP_ALT_UD ) );

#endif //ISP_WDR_SWITCH

    //  Do not update values if manual mode
    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_demosaic == 0 ) {
#if ISP_WDR_SWITCH
        //use luts for modulation instead!
        alt_d = acamera_calc_modulation_u16( log2_gain, sharp_alt_d_table_ptr, _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), sharp_alt_d_idx ) );
#else
        alt_d = acamera_calc_modulation_u16( log2_gain, sharp_alt_d_table_ptr, _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SHARP_ALT_D ) );
#endif

        alt_du = acamera_calc_modulation_u16( log2_gain, sharp_alt_du_table_ptr, _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), sharp_alt_du_idx ) );
        alt_d = ( alt_d * p_fsm->sharpening_mult ) / 128;
        if ( alt_d >= ( 1 << ACAMERA_ISP_DEMOSAIC_RGB_SHARP_ALT_D_DATASIZE ) ) {
            alt_d = ( 1 << ACAMERA_ISP_DEMOSAIC_RGB_SHARP_ALT_D_DATASIZE ) - 1;
        }

#if ISP_WDR_SWITCH
        //use luts for modulation instead!
        alt_ud = acamera_calc_modulation_u16( log2_gain, sharp_alt_ud_table_ptr, _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), sharp_alt_ud_idx ) );
#else
        alt_ud = acamera_calc_modulation_u16( log2_gain, sharp_alt_ud_table_ptr, _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SHARP_ALT_UD ) );
#endif

        alt_ud = ( alt_ud * p_fsm->sharpening_mult ) / 128;
        if ( alt_ud >= ( 1 << ACAMERA_ISP_DEMOSAIC_RGB_SHARP_ALT_UD_DATASIZE ) ) {
            alt_ud = ( 1 << ACAMERA_ISP_DEMOSAIC_RGB_SHARP_ALT_UD_DATASIZE ) - 1;
        }

        acamera_isp_demosaic_rgb_sharp_alt_d_write( p_fsm->cmn.isp_base, alt_d );
        acamera_isp_demosaic_rgb_sharp_alt_ud_write( p_fsm->cmn.isp_base, alt_ud );

        acamera_isp_demosaic_rgb_sharp_alt_ld_write( p_fsm->cmn.isp_base, alt_d );
        acamera_isp_demosaic_rgb_sharp_alt_ldu_write( p_fsm->cmn.isp_base, alt_du );

        acamera_isp_demosaic_rgb_sharp_alt_lu_write( p_fsm->cmn.isp_base, alt_ud );
    }

    if ( ACAMERA_FSM2CTX_PTR( p_fsm )->stab.global_manual_sharpen == 0 ) {
        const modulation_entry_t *sharpen_fr_table = _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SHARPEN_FR );
        acamera_isp_fr_sharpen_strength_write( p_fsm->cmn.isp_base, acamera_calc_modulation_u16( log2_gain, sharpen_fr_table, _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SHARPEN_FR ) ) );

#if ISP_HAS_DS1
        const modulation_entry_t *sharpen_ds1_table = _GET_MOD_ENTRY16_PTR( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SHARPEN_DS1 );
        acamera_isp_ds1_sharpen_strength_write( p_fsm->cmn.isp_base, acamera_calc_modulation_u16( log2_gain, sharpen_ds1_table, _GET_ROWS( ACAMERA_FSM2CTX_PTR( p_fsm ), CALIBRATION_SHARPEN_DS1 ) ) );
#endif //ISP_HAS_DS1
    }
}
