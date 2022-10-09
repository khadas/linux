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
#include <linux/version.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>

#include "acamera_logger.h"

#include "isp-v4l2-common.h"
#include "isp-v4l2-ctrl.h"
#include "fw-interface.h"

static int isp_v4l2_ctrl_check_valid( struct v4l2_ctrl *ctrl )
{
    if ( ctrl->is_int == 1 ) {
        if ( ctrl->val < ctrl->minimum || ctrl->val > ctrl->maximum )
            return -EINVAL;
    }

    return 0;
}

static int isp_v4l2_ctrl_s_ctrl_standard( struct v4l2_ctrl *ctrl )
{
    int ret = 0;
    struct v4l2_ctrl_handler *hdl = ctrl->handler;

    isp_v4l2_ctrl_t *isp_ctrl = std_hdl_to_isp_ctrl( hdl );
    int ctx_id = isp_ctrl->ctx_id;

    LOG( LOG_DEBUG, "Control - id:0x%x, val:%d, is_int:%d, min:%d, max:%d.\n",
         ctrl->id, ctrl->val, ctrl->is_int, ctrl->minimum, ctrl->maximum );

    if ( isp_v4l2_ctrl_check_valid( ctrl ) < 0 ) {
        return -EINVAL;
    }

    switch ( ctrl->id ) {
    case V4L2_CID_BRIGHTNESS:
        ret = fw_intf_set_brightness( ctx_id, ctrl->val );
        break;
    case V4L2_CID_CONTRAST:
        ret = fw_intf_set_contrast( ctx_id, ctrl->val );
        break;
    case V4L2_CID_SATURATION:
        ret = fw_intf_set_saturation( ctx_id, ctrl->val );
        break;
    case V4L2_CID_HUE:
        ret = fw_intf_set_hue( ctx_id, ctrl->val );
        break;
    case V4L2_CID_SHARPNESS:
        ret = fw_intf_set_sharpness( ctx_id, ctrl->val );
        break;
    case V4L2_CID_COLORFX:
        ret = fw_intf_set_color_fx( ctx_id, ctrl->val );
        break;
    case V4L2_CID_HFLIP:
        ret = fw_intf_set_hflip( ctx_id, ctrl->val );
        break;
    case V4L2_CID_VFLIP:
        ret = fw_intf_set_vflip( ctx_id, ctrl->val );
        break;
    case V4L2_CID_AUTOGAIN:
        ret = fw_intf_set_autogain( ctx_id, ctrl->val );
        break;
    case V4L2_CID_GAIN:
        ret = fw_intf_set_gain( ctx_id, ctrl->val );
        break;
    case V4L2_CID_EXPOSURE_AUTO:
        ret = fw_intf_set_exposure_auto( ctx_id, ctrl->val );
        break;
    case V4L2_CID_EXPOSURE_ABSOLUTE:
        ret = fw_intf_set_exposure( ctx_id, ctrl->val );
        break;
    case V4L2_CID_EXPOSURE_AUTO_PRIORITY:
        ret = fw_intf_set_variable_frame_rate( ctx_id, ctrl->val );
        break;
    case V4L2_CID_AUTO_WHITE_BALANCE:
        ret = fw_intf_set_white_balance_auto( ctx_id, ctrl->val );
        break;
    case V4L2_CID_WHITE_BALANCE_TEMPERATURE:
        ret = fw_intf_set_white_balance( ctx_id, ctrl->val );
        break;
    case V4L2_CID_FOCUS_AUTO:
        ret = fw_intf_set_focus_auto( ctx_id, ctrl->val );
        break;
    case V4L2_CID_FOCUS_ABSOLUTE:
        ret = fw_intf_set_focus( ctx_id, ctrl->val );
        break;
    case V4L2_CID_SCENE_MODE:
        ret = fw_intf_set_ae_scene( ctx_id, ctrl->val );
        break;
    }

    return ret;
}

static int isp_v4l2_ctrl_s_ctrl_custom( struct v4l2_ctrl *ctrl )
{
    int ret = 0;
    struct v4l2_ctrl_handler *hdl = ctrl->handler;

    isp_v4l2_ctrl_t *isp_ctrl = cst_hdl_to_isp_ctrl( hdl );
    int ctx_id = isp_ctrl->ctx_id;

    LOG( LOG_DEBUG, "Control - id:0x%x, val:%d, is_int:%d, min:%d, max:%d.\n",
         ctrl->id, ctrl->val, ctrl->is_int, ctrl->minimum, ctrl->maximum);

    if ( isp_v4l2_ctrl_check_valid( ctrl ) < 0 ) {
        LOG( LOG_ERR, "Invalid param: id:0x%x, val:0x%x, is_int:%d, min:0x%x, max:0x%x.\n",
             ctrl->id, ctrl->val, ctrl->is_int, ctrl->minimum, ctrl->maximum );

        return -EINVAL;
    }

    switch ( ctrl->id ) {
    case ISP_V4L2_CID_TEST_PATTERN:
        LOG( LOG_INFO, "new test_pattern: %d.\n", ctrl->val );
        ret = fw_intf_set_test_pattern( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_TEST_PATTERN_TYPE:
        LOG( LOG_INFO, "new test_pattern_type: %d.\n", ctrl->val );
        ret = fw_intf_set_test_pattern_type( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_AF_REFOCUS:
        LOG( LOG_INFO, "new focus: %d.\n", ctrl->val );
        ret = fw_intf_set_af_refocus( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SENSOR_PRESET:
        LOG( LOG_INFO, "new sensor preset: %d.\n", ctrl->val );
        ret = fw_intf_isp_set_sensor_preset( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_AF_ROI:
        // map [0,127] to [0, 254] due to limitaton of V4L2_CTRL_TYPE_INTEGER.
        LOG( LOG_INFO, "new af roi: 0x%x.\n", ctrl->val * 2 );
        ret = fw_intf_set_af_roi( ctx_id, ctrl->val * 2 );
        break;
    case ISP_V4L2_CID_AE_ROI:
        // map [0,127] to [0, 254] due to limitaton of V4L2_CTRL_TYPE_INTEGER.
        LOG( LOG_INFO, "new ae roi: 0x%x.\n", ctrl->val * 2 );
        ret = fw_intf_set_ae_roi( ctx_id, ctrl->val * 2 );
        break;
    case ISP_V4L2_CID_OUTPUT_FR_ON_OFF:
        LOG( LOG_INFO, "output FR on/off: 0x%x.\n", ctrl->val );
        ret = fw_intf_set_output_fr_on_off( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_OUTPUT_DS1_ON_OFF:
        LOG( LOG_INFO, "output DS1 on/off: 0x%x.\n", ctrl->val );
        ret = fw_intf_set_output_ds1_on_off( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_CUSTOM_SENSOR_WDR_MODE:
        LOG( LOG_INFO, "set custom wdr mode: 0x%x.\n", ctrl->val );
        ret = fw_intf_set_custom_sensor_wdr_mode(ctx_id, ctrl->val);
        break;
    case ISP_V4L2_CID_CUSTOM_SENSOR_EXPOSURE:
        LOG( LOG_INFO, "set custom exposure: 0x%x.\n", ctrl->val );
        ret = fw_intf_set_custom_sensor_exposure(ctx_id, ctrl->val);
        break;
    case ISP_V4L2_CID_CUSTOM_FR_FPS:
        LOG( LOG_INFO, "set fr fps: 0x%x.\n", ctrl->val );
        ret = fw_intf_set_custom_fr_fps(ctx_id, ctrl->val);
        *(ctrl->p_new.p_s32) = 0;
        break;
    case ISP_V4L2_CID_CUSTOM_SET_SENSOR_TESTPATTERN:
        LOG( LOG_INFO, "set sensor test pattern: 0x%x.\n", ctrl->val );
        ret = fw_intf_set_custom_sensor_testpattern(ctx_id, ctrl->val);
        break;
    case ISP_V4L2_CID_CUSTOM_SENSOR_IR_CUT:
        LOG( LOG_INFO, "set_customer_sensor_ir_cut = %d\n", ctrl->val );
        ret = fw_intf_set_customer_sensor_ir_cut(ctx_id, ctrl->val);
        break;
    case ISP_V4L2_CID_CUSTOM_AE_ZONE_WEIGHT:
        LOG( LOG_CRIT, "set ae zone weight: 0x%x.\n", *(ctrl->p_new.p_u32));
        ret = fw_intf_set_customer_ae_zone_weight(ctx_id, ctrl->p_new.p_u32);
        *(ctrl->p_new.p_u32) = 0;
        break;
    case ISP_V4L2_CID_CUSTOM_AWB_ZONE_WEIGHT:
        LOG( LOG_INFO, "set awb zone weight: 0x%llx.\n", *(ctrl->p_new.p_u32));
        ret = fw_intf_set_customer_awb_zone_weight(ctx_id, ctrl->p_new.p_u32);
        *(ctrl->p_new.p_u32) = 0;
        break;
    case ISP_V4L2_CID_CUSTOM_SET_MANUAL_EXPOSURE:
        LOG( LOG_INFO, "set_customer_manual_exposure = %d\n", ctrl->val );
        ret = fw_intf_set_customer_manual_exposure(ctx_id, ctrl->val);
        ctrl->val = -1;
        break;
    case ISP_V4L2_CID_CUSTOM_SET_SENSOR_INTEGRATION_TIME:
        LOG( LOG_INFO, "set_customer_integration_time = %d\n", ctrl->val );
        ret = fw_intf_set_customer_sensor_integration_time(ctx_id, ctrl->val);
        ctrl->val = -1;
        break;
    case ISP_V4L2_CID_CUSTOM_SET_SENSOR_ANALOG_GAIN:
        LOG( LOG_INFO, "set_customer_sensor_analog_gain = %d\n", ctrl->val );
        ret = fw_intf_set_customer_sensor_analog_gain(ctx_id, ctrl->val);
        ctrl->val = -1;
        break;
    case ISP_V4L2_CID_CUSTOM_SET_ISP_DIGITAL_GAIN:
        LOG( LOG_INFO, "set_customer_isp_digital_gain = %d\n", ctrl->val );
        ret = fw_intf_set_customer_isp_digital_gain(ctx_id, ctrl->val);
        ctrl->val = -1;
        break;
    case ISP_V4L2_CID_CUSTOM_SET_STOP_SENSOR_UPDATE:
        LOG( LOG_INFO, "set_customer_stop_sensor_update = %d\n", ctrl->val );
        ret = fw_intf_set_customer_stop_sensor_update(ctx_id, ctrl->val);
        ctrl->val = -1;
        break;
    case ISP_V4L2_CID_CUSTOM_SET_DS1_FPS:
        LOG( LOG_INFO, "set ds1 fps: 0x%x.\n", ctrl->val );
        ret = fw_intf_set_custom_ds1_fps(ctx_id, ctrl->val);
        *(ctrl->p_new.p_s32) = 0;
        break;
    case ISP_V4L2_CID_CUSTOM_SET_SENSOR_DIGITAL_GAIN:
        LOG( LOG_INFO, "set_customer_sensor_digital_gain = %d\n", ctrl->val );
        ret = fw_intf_set_customer_sensor_digital_gain(ctx_id, ctrl->val);
        ctrl->val = -1;
        break;
    case ISP_V4L2_CID_CUSTOM_SET_AWB_RED_GAIN:
        LOG( LOG_INFO, "set_customer_awb_red_gain = %d\n", ctrl->val );
        ret = fw_intf_set_customer_awb_red_gain(ctx_id, ctrl->val);
        break;
    case ISP_V4L2_CID_CUSTOM_SET_AWB_BLUE_GAIN:
        LOG( LOG_INFO, "set_customer_awb_blue_gain = %d\n", ctrl->val );
        ret = fw_intf_set_customer_awb_blue_gain(ctx_id, ctrl->val);
        break;
    case ISP_V4L2_CID_CUSTOM_SET_MAX_INTEGRATION_TIME:
        LOG( LOG_INFO, "set_customer_max_integration_time = %d\n", ctrl->val );
        ret = fw_intf_set_customer_max_integration_time(ctx_id, ctrl->val);
        break;
    case ISP_V4L2_CID_CUSTOM_SENSOR_FPS:
        LOG( LOG_INFO, "set custom fps: %d.\n", ctrl->val );
        ret = fw_intf_set_custom_sensor_fps(ctx_id, ctrl->val);
        break;
    case ISP_V4L2_CID_CUSTOM_SNR_MANUAL:
        LOG( LOG_INFO, "set snr manual: %d.\n", ctrl->val );
        ret = fw_intf_set_custom_snr_manual(ctx_id, ctrl->val);
        break;
    case ISP_V4L2_CID_CUSTOM_SNR_STRENGTH:
        LOG( LOG_INFO, "set snr strength: %d.\n", ctrl->val );
        ret = fw_intf_set_custom_snr_strength(ctx_id, ctrl->val);
        break;
    case ISP_V4L2_CID_CUSTOM_TNR_MANUAL:
        LOG( LOG_INFO, "set tnr manual: %d.\n", ctrl->val );
        ret = fw_intf_set_custom_tnr_manual(ctx_id, ctrl->val);
        break;
    case ISP_V4L2_CID_CUSTOM_TNR_OFFSET:
        LOG( LOG_INFO, "set tnr offset: %d.\n", ctrl->val );
        ret = fw_intf_set_custom_tnr_offset(ctx_id, ctrl->val);
        break;
     case ISP_V4L2_CID_CUSTOM_TEMPER_MODE:
        LOG( LOG_INFO, "set temper mode: %d.\n", ctrl->val );
        ret = fw_intf_set_customer_temper_mode(ctx_id, ctrl->val);
        break;
     case ISP_V4L2_CID_CUSTOM_WDR_SWITCH:
        LOG( LOG_INFO, "set wdr mode: %d.\n", ctrl->val );
        ret = fw_intf_set_customer_sensor_mode(ctx_id, ctrl->val);
        break;
     case ISP_V4L2_CID_CUSTOM_ANTIFLICKER:
        LOG( LOG_INFO, "set anti flicker: %d.\n", ctrl->val );
        ret = fw_intf_set_customer_antiflicker(ctx_id, ctrl->val);
        break;
     case ISP_V4L2_CID_CUSTOM_DEFOG_SWITCH:
        LOG( LOG_INFO, "set defog mode: %d.\n", ctrl->val );
        ret = fw_intf_set_customer_defog_mode(ctx_id, ctrl->val);
        break;
     case ISP_V4L2_CID_CUSTOM_DEFOG_STRENGTH:
        LOG( LOG_INFO, "set defog strength: %d.\n", ctrl->val );
        ret = fw_intf_set_customer_defog_ratio(ctx_id, ctrl->val);
        break;
     case ISP_V4L2_CID_CUSTOM_CALIBRATION:
        LOG( LOG_INFO, "set customer cali: %d.\n", ctrl->val );
        ret = fw_intf_set_customer_calibration(ctx_id, ctrl->val);
        break;
    case ISP_V4L2_CID_AWB_MODE_ID:
        LOG( LOG_INFO, "new awb_mode_id: %d.\n", ctrl->val );
        ret = fw_intf_set_awb_mode_id( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_NOISE_REDUCTION_MODE_ID:
        LOG( LOG_INFO, "new noise_reduction_mode_id: %d.\n", ctrl->val );
        ret = fw_intf_set_noise_reduction_mode_id( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_FREEZE_FIRMWARE:
        LOG( LOG_INFO, "new system_freeze_firmware: %d.\n", ctrl->val );
        ret = fw_intf_set_system_freeze_firmware( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_EXPOSURE:
        LOG( LOG_INFO, "new system_manual_exposure: %d.\n", ctrl->val );
        ret = fw_intf_set_system_manual_exposure( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_INTEGRATION_TIME:
        LOG( LOG_INFO, "new system_manual_integration_time: %d.\n", ctrl->val );
        ret = fw_intf_set_system_manual_integration_time( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_MAX_INTEGRATION_TIME:
        LOG( LOG_INFO, "new system_manual_max_integration_time: %d.\n", ctrl->val );
        ret = fw_intf_set_system_manual_max_integration_time( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_SENSOR_ANALOG_GAIN:
        LOG( LOG_INFO, "new system_manual_sensor_analog_gain: %d.\n", ctrl->val );
        ret = fw_intf_set_system_manual_sensor_analog_gain( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN:
        LOG( LOG_INFO, "new system_manual_sensor_digital_gain: %d.\n", ctrl->val );
        ret = fw_intf_set_system_manual_sensor_digital_gain( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_ISP_DIGITAL_GAIN:
        LOG( LOG_INFO, "new system_manual_isp_digital_gain: %d.\n", ctrl->val );
        ret = fw_intf_set_system_manual_isp_digital_gain( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_EXPOSURE_RATIO:
        LOG( LOG_INFO, "new system_manual_exposure_ratio: %d.\n", ctrl->val );
        ret = fw_intf_set_system_manual_exposure_ratio( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_AWB:
        LOG( LOG_INFO, "new system_manual_awb: %d.\n", ctrl->val );
        ret = fw_intf_set_system_manual_awb( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_CCM:
        LOG( LOG_INFO, "new system_manual_ccm: %d.\n", ctrl->val );
        ret = fw_intf_set_system_manual_ccm( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_SATURATION:
        LOG( LOG_INFO, "new system_manual_saturation: %d.\n", ctrl->val );
        ret = fw_intf_set_system_manual_saturation( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MAX_EXPOSURE_RATIO:
        LOG( LOG_INFO, "new system_max_exposure_ratio: %d.\n", ctrl->val );
        ret = fw_intf_set_system_max_exposure_ratio( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_EXPOSURE:
        LOG( LOG_INFO, "new system_exposure: %d.\n", ctrl->val );
        ret = fw_intf_set_system_exposure( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_INTEGRATION_TIME:
        LOG( LOG_INFO, "new system_integration_time: %d.\n", ctrl->val );
        ret = fw_intf_set_system_integration_time( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_EXPOSURE_RATIO:
        LOG( LOG_INFO, "new system_exposure_ratio: %d.\n", ctrl->val );
        ret = fw_intf_set_system_exposure_ratio( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MAX_INTEGRATION_TIME:
        LOG( LOG_INFO, "new system_max_integration_time: %d.\n", ctrl->val );
        ret = fw_intf_set_system_max_integration_time( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_SENSOR_ANALOG_GAIN:
        LOG( LOG_INFO, "new system_sensor_analog_gain: %d.\n", ctrl->val );
        ret = fw_intf_set_system_sensor_analog_gain( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MAX_SENSOR_ANALOG_GAIN:
        LOG( LOG_INFO, "new system_max_sensor_analog_gain: %d.\n", ctrl->val );
        ret = fw_intf_set_system_max_sensor_analog_gain( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_SENSOR_DIGITAL_GAIN:
        LOG( LOG_INFO, "new system_sensor_digital_gain: %d.\n", ctrl->val );
        ret = fw_intf_set_system_sensor_digital_gain( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MAX_SENSOR_DIGITAL_GAIN:
        LOG( LOG_INFO, "new system_max_sensor_digital_gain: %d.\n", ctrl->val );
        ret = fw_intf_set_system_max_sensor_digital_gain( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_ISP_DIGITAL_GAIN:
        LOG( LOG_INFO, "new system_isp_digital_gain: %d.\n", ctrl->val );
        ret = fw_intf_set_system_isp_digital_gain( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MAX_ISP_DIGITAL_GAIN:
        LOG( LOG_INFO, "new system_max_isp_digital_gain: %d.\n", ctrl->val );
        ret = fw_intf_set_system_max_isp_digital_gain( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_AWB_RED_GAIN:
        LOG( LOG_INFO, "new system_awb_red_gain: %d.\n", ctrl->val );
        ret = fw_intf_set_system_awb_red_gain( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_AWB_GREEN_EVEN_GAIN:
        LOG( LOG_INFO, "new system_awb_green_even_gain: %d.\n", ctrl->val );
        ret = fw_intf_set_system_awb_green_even_gain( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_AWB_GREEN_ODD_GAIN:
        LOG( LOG_INFO, "new system_awb_green_odd_gain: %d.\n", ctrl->val );
        ret = fw_intf_set_system_awb_green_odd_gain( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_AWB_BLUE_GAIN:
        LOG( LOG_INFO, "new system_awb_blue_gain: %d.\n", ctrl->val );
        ret = fw_intf_set_system_awb_blue_gain( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_CCM_MATRIX:
        LOG( LOG_INFO, "new system_ccm_matrix\n" );
        ret = fw_intf_set_system_ccm_matrix( ctx_id, ctrl->p_new.p_s32, ctrl->elems );
        break;
    case ISP_V4L2_CID_SHADING_STRENGTH:
        LOG( LOG_INFO, "new shading_strength: %d.\n", ctrl->val );
        ret = fw_intf_set_shading_strength( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_RGB_GAMMA_LUT:
        LOG( LOG_INFO, "new system_rgb_gamma_lut\n" );
        ret = fw_intf_set_system_rgb_gamma_lut( ctx_id, ctrl->p_new.p_u16, ctrl->elems );
        break;
    case ISP_V4L2_CID_SYSTEM_IQ_CALIBRATION_DYNAMIC:
        LOG( LOG_INFO, "new calibration from user space\n" );
        ret = fw_intf_set_system_calibration_dynamic( ctx_id, ctrl->p_new.p_u16, ctrl->elems );
        break;
    case ISP_V4L2_CID_SYSTEM_STATUATION_TARGET:
        LOG( LOG_INFO, "new system_saturation target\n" );
        ret = fw_intf_set_saturation_target( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_IRIDIX:
        LOG( LOG_INFO, "new isp_modules_manual_iridix: %d.\n", ctrl->val );
        ret = fw_intf_set_isp_modules_manual_iridix( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_SINTER:
        LOG( LOG_INFO, "new isp_modules_manual_sinter: %d.\n", ctrl->val );
        ret = fw_intf_set_isp_modules_manual_sinter( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_TEMPER:
        LOG( LOG_INFO, "new isp_modules_manual_temper: %d.\n", ctrl->val );
        ret = fw_intf_set_isp_modules_manual_temper( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_AUTO_LEVEL:
        LOG( LOG_INFO, "new isp_modules_manual_auto_level: %d.\n", ctrl->val );
        ret = fw_intf_set_isp_modules_manual_auto_level( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_FRAME_STITCH:
        LOG( LOG_INFO, "new isp_modules_manual_frame_stitch: %d.\n", ctrl->val );
        ret = fw_intf_set_isp_modules_manual_frame_stitch( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_RAW_FRONTEND:
        LOG( LOG_INFO, "new isp_modules_manual_raw_frontend: %d.\n", ctrl->val );
        ret = fw_intf_set_isp_modules_manual_raw_frontend( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_BLACK_LEVEL:
        LOG( LOG_INFO, "new isp_modules_manual_black_level: %d.\n", ctrl->val );
        ret = fw_intf_set_isp_modules_manual_black_level( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_SHADING:
        LOG( LOG_INFO, "new isp_modules_manual_shading: %d.\n", ctrl->val );
        ret = fw_intf_set_isp_modules_manual_shading( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_DEMOSAIC:
        LOG( LOG_INFO, "new isp_modules_manual_demosaic: %d.\n", ctrl->val );
        ret = fw_intf_set_isp_modules_manual_demosaic( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_CNR:
        LOG( LOG_INFO, "new isp_modules_manual_cnr: %d.\n", ctrl->val );
        ret = fw_intf_set_isp_modules_manual_cnr( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_SHARPEN:
        LOG( LOG_INFO, "new isp_modules_manual_sharpen: %d.\n", ctrl->val );
        ret = fw_intf_set_isp_modules_manual_sharpen( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_PF:
        LOG( LOG_INFO, "new isp_modules_manual_pf: %d.\n", ctrl->val );
        ret = fw_intf_set_isp_modules_manual_pf( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_IMAGE_RESIZE_ENABLE_ID:
        LOG( LOG_INFO, "new image_resize_enable: %d.\n", ctrl->val );
        ret = fw_intf_set_image_resize_enable( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_IMAGE_RESIZE_TYPE_ID:
        LOG( LOG_INFO, "new image_resize_type: %d.\n", ctrl->val );
        ret = fw_intf_set_image_resize_type( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_IMAGE_CROP_XOFFSET_ID:
        LOG( LOG_INFO, "new image_crop_xoffset: %d.\n", ctrl->val );
        ret = fw_intf_set_image_crop_xoffset( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_IMAGE_CROP_YOFFSET_ID:
        LOG( LOG_INFO, "new image_crop_yoffset: %d.\n", ctrl->val );
        ret = fw_intf_set_image_crop_yoffset( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_IMAGE_RESIZE_WIDTH_ID:
        LOG( LOG_INFO, "new image_resize_width: %d.\n", ctrl->val );
        ret = fw_intf_set_image_resize_width( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_IMAGE_RESIZE_HEIGHT_ID:
        LOG( LOG_INFO, "new image_resize_height: %d.\n", ctrl->val );
        ret = fw_intf_set_image_resize_height( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_AE_COMPENSATION:
        LOG( LOG_INFO, "new ae_compensation: %d.\n", ctrl->val );
        ret = fw_intf_set_ae_compensation( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_DYNAMIC_GAMMA_ENABLE:
        LOG( LOG_INFO, "new system_dynamic_gamma_enable: %d\n", ctrl->val );
        ret = fw_intf_set_system_dynamic_gamma_enable( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SENSOR_VMAX_FPS:
        LOG( LOG_INFO, "new sensor fps by vmax %d", ctrl->val );
        ret = fw_intf_set_sensor_vmax_fps( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_DPC_THRES_SLOPE:
        LOG( LOG_INFO, "dpc threshold and slope %d", ctrl->val );
        ret = fw_intf_set_dpc_threshold_slope( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_BLACK_LEVEL_R:
        LOG( LOG_INFO, "black level R %d\n", ctrl->val );
        ret = fw_intf_set_black_level_r( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_BLACK_LEVEL_GR:
        LOG( LOG_INFO, "black level GR %d\n", ctrl->val );
        ret = fw_intf_set_black_level_gr( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_BLACK_LEVEL_GB:
        LOG( LOG_INFO, "black level GB %d\n", ctrl->val );
        ret = fw_intf_set_black_level_gb( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_BLACK_LEVEL_B:
        LOG( LOG_INFO, "black level B %d\n", ctrl->val );
        ret = fw_intf_set_black_level_b( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SHARP_LU_DU_LD:
        LOG( LOG_INFO, "demosaic sharp %d\n", ctrl->val );
        ret = fw_intf_set_demosaic_sharp( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_CNR_STRENGTH:
        LOG( LOG_INFO, "cnr strength %d\n", ctrl->val );
        ret = fw_intf_set_cnr_strength( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_ANTIFLICKER_ENABLE:
        LOG( LOG_INFO, "new system_antiflicker_enable: %d\n", ctrl->val);
        ret = fw_intf_set_system_antiflicker_enable( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_ANTIFLICKER_FREQUENCY:
        LOG( LOG_INFO, "new system_antiflicker_frequency: %d, rc: %d.\n", ctrl->val );
        ret = fw_intf_set_system_antiflicker_frequency( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_IRIDIX_ENABLE:
        LOG( LOG_INFO, "new isp modules iridix enable : %d\n", ctrl->val);
        ret = fw_intf_set_isp_modules_iridix_enable( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_IRIDIX_STRENGTH:
        LOG( LOG_INFO, "new isp module iridix strength : %d\n", ctrl->val);
        ret = fw_intf_set_isp_modules_iridix_strength( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_SWHW_REGISTERS:
        LOG( LOG_INFO, "new ISP_V4L2_CID_ISP_MODULES_SWHW_REGISTERS : %d\n", ctrl->val);
        ret = fw_intf_set_isp_modules_swhw_registers( ctx_id, ctrl->p_new.p_u16, ctrl->elems);
        break;
    case ISP_V4L2_CID_ISP_MODULES_FR_SHARPEN_STRENGTH:
        LOG( LOG_INFO, "new isp module fr sharpen strength : %d\n", ctrl->val);
        ret = fw_intf_set_isp_modules_fr_sharpen_strength( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_DS1_SHARPEN_STRENGTH:
        LOG( LOG_INFO, "new isp module ds1 sharpen strength : %d\n", ctrl->val );
        ret = fw_intf_set_isp_modules_ds1_sharpen_strength( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_BYPASS:
        ret = fw_intf_set_isp_modules_bypass( ctx_id, ctrl->p_new.p_u32 );
        LOG( LOG_INFO, "new isp module by pass  rc: %d.\n", ret );
        break;
    case ISP_V4L2_CID_ISP_DAYNIGHT_DETECT:
        LOG( LOG_INFO, "new isp day night detect : %d.\n", ctrl->val);
        ret = fw_intf_set_day_night_detect( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SCALE_CROP_STARTX:
        LOG( LOG_INFO, "new isp scale crop startx : %d.\n", ctrl->val);
        ret = fw_intf_set_scale_crop_startx( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SCALE_CROP_STARTY:
        LOG( LOG_INFO, "new isp scale crop starty : %d.\n", ctrl->val);
        ret = fw_intf_set_scale_crop_starty( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SCALE_CROP_WIDTH:
        LOG( LOG_INFO, "new isp scale crop width : %d.\n", ctrl->val);
        ret = fw_intf_set_scale_crop_width( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SCALE_CROP_HEIGHT:
        LOG( LOG_INFO, "new isp scale crop height : %d.\n", ctrl->val);
        ret = fw_intf_set_scale_crop_height( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SCALE_CROP_ENABLE:
        LOG( LOG_INFO, "new isp scale crop enable : %d.\n", ctrl->val);
        ret = fw_intf_set_scale_crop_enable( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SET_SENSOR_POWER:
        LOG( LOG_INFO, "set sensor power : %d.\n", ctrl->val);
        ret = fw_intf_set_sensor_power( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SET_SENSOR_MD_EN:
        LOG( LOG_INFO, "set sensor md : %d.\n", ctrl->val);
        ret = fw_intf_set_sensor_md_en( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SET_SENSOR_DECMPR_EN:
        LOG( LOG_INFO, "set sensor decmpr : %d.\n", ctrl->val);
        ret = fw_intf_set_sensor_decmpr_en( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SET_SENSOR_DECMPR_LOSSLESS_EN:
        LOG( LOG_INFO, "set sensor decmpr lossless_en : %d.\n", ctrl->val);
        ret = fw_intf_set_sensor_decmpr_lossless_en( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SET_SENSOR_FLICKER_EN:
        LOG( LOG_INFO, "set sensor flicker : %d.\n", ctrl->val);
        ret = fw_intf_set_sensor_flicker_en( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_SCALER_FPS:
        LOG( LOG_INFO, "set sc fps: %x.\n", ctrl->val);
        ret = fw_intf_set_scaler_fps( ctx_id, ctrl->val );
        break;
    case ISP_V4L2_CID_AE_ROI_EXACT_COORDINATES:
        ret = fw_intf_set_isp_modules_ae_roi_exact_coordinates( ctx_id, ctrl->p_new.p_u32 );
        LOG( LOG_INFO, "new isp module ae roi exact coordinates  rc: %d.\n", ret );
        break;
    case ISP_V4L2_CID_AF_ROI_EXACT_COORDINATES:
        ret = fw_intf_set_isp_modules_af_roi_exact_coordinates( ctx_id, ctrl->p_new.p_u32 );
        LOG( LOG_INFO, "new isp module af roi exact coordinates  rc: %d.\n", ret );
        break;
    case ISP_V4L2_CID_SET_IS_CAPTURING:
        ret = fw_intf_set_isp_modules_is_capturing( ctx_id, ctrl->val );
        LOG( LOG_INFO, "new isp module isp is capturing  rc: %d.\n", ret );
        break;
    case ISP_V4L2_CID_SET_FPS_RANGE:
        ret = fw_intf_set_isp_modules_fps_range( ctx_id, ctrl->p_new.p_u32 );
        LOG( LOG_INFO, "new isp module isp is fps range rc: %d.\n", ret );
        break;
    case ISP_V4L2_CID_ISP_DCAM_MODE:
        LOG( LOG_INFO, "set dcam mode: %d,%d.\n", ctx_id,ctrl->val );
        ret = fw_intf_set_custom_dcam_mode(ctx_id, ctrl->val);
        break;
    }

    return ret;
}

static int isp_v4l2_ctrl_g_ctrl_custom( struct v4l2_ctrl *ctrl )
{
    int ret = 0;
    struct v4l2_ctrl_handler *hdl = ctrl->handler;

    isp_v4l2_ctrl_t *isp_ctrl = cst_hdl_to_isp_ctrl( hdl );
    int ctx_id = isp_ctrl->ctx_id;

    LOG( LOG_DEBUG, "Control - id:0x%x, val:%d, is_int:%d, min:%d, max:%d.\n",
         ctrl->id, ctrl->val, ctrl->is_int, ctrl->minimum, ctrl->maximum);

    switch ( ctrl->id ) {
    case ISP_V4L2_CID_CUSTOM_SNR_MANUAL:
        LOG( LOG_INFO, "get snr manual: %d.\n" );
        ctrl->val = fw_intf_get_custom_snr_manual(ctx_id);
        break;
    case ISP_V4L2_CID_CUSTOM_SNR_STRENGTH:
        LOG( LOG_INFO, "get snr strength: %d.\n" );
        ctrl->val = fw_intf_get_custom_snr_strength(ctx_id);
        break;
    case ISP_V4L2_CID_CUSTOM_TNR_MANUAL:
        LOG( LOG_INFO, "get tnr manual: %d.\n" );
        ctrl->val = fw_intf_get_custom_tnr_manual(ctx_id);
        break;
    case ISP_V4L2_CID_CUSTOM_TNR_OFFSET:
        LOG( LOG_INFO, "get tnr offset: %d.\n" );
        ctrl->val = fw_intf_get_custom_tnr_offset(ctx_id);
        break;
    case ISP_V4L2_CID_CUSTOM_TEMPER_MODE:
        LOG( LOG_INFO, "get temper mode: %d.\n" );
        ctrl->val = fw_intf_get_custom_temper_mode(ctx_id);
        break;
    case ISP_V4L2_CID_CUSTOM_WDR_SWITCH:
        LOG( LOG_INFO, "get wdr mode: %d.\n" );
        ctrl->val = fw_intf_get_customer_sensor_mode(ctx_id);
        break;
    case ISP_V4L2_CID_CUSTOM_ANTIFLICKER:
        LOG( LOG_INFO, "get anti flicker: %d.\n" );
        ctrl->val = fw_intf_get_customer_antiflicker(ctx_id);
        break;
    case ISP_V4L2_CID_SYSTEM_FREEZE_FIRMWARE:
        ret = fw_intf_get_system_freeze_firmware( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_freeze_firmware: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_EXPOSURE:
        ret = fw_intf_get_system_manual_exposure( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_manual_exposure: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_INTEGRATION_TIME:
        ret = fw_intf_get_system_manual_integration_time( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_manual_integration_time: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_MAX_INTEGRATION_TIME:
        ret = fw_intf_get_system_manual_max_integration_time( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_manual_max_integration_time: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_SENSOR_ANALOG_GAIN:
        ret = fw_intf_get_system_manual_sensor_analog_gain( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_manual_sensor_analog_gain: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN:
        ret = fw_intf_get_system_manual_sensor_digital_gain( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_manual_sensor_digital_gain: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_ISP_DIGITAL_GAIN:
        ret = fw_intf_get_system_manual_isp_digital_gain( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_manual_isp_digital_gain: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_EXPOSURE_RATIO:
        ret = fw_intf_get_system_manual_exposure_ratio( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_manual_exposure_ratio: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_AWB:
        ret = fw_intf_get_system_manual_awb( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_manual_awb: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_CCM:
        ret = fw_intf_get_system_manual_ccm( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_manual_ccm: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MANUAL_SATURATION:
        ret = fw_intf_get_system_manual_saturation( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_manual_saturation: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_MAX_EXPOSURE_RATIO:
        ret = fw_intf_get_system_max_exposure_ratio( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_max_exposure_ratio: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_EXPOSURE:
        ret = fw_intf_get_system_exposure( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_exposure: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SYSTEM_INTEGRATION_TIME:
        ret = fw_intf_get_system_integration_time( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_integration_time: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SYSTEM_LONG_INTEGRATION_TIME:
        ret = fw_intf_get_system_long_integration_time( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system long integration time: %d, rc: %d.\n", ctrl->val, ret );
    break;
    case ISP_V4L2_CID_SYSTEM_SHORT_INTEGRATION_TIME:
        ret = fw_intf_get_system_short_integration_time( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system short integration time: %d, rc: %d.\n", ctrl->val, ret );
    break;
    case ISP_V4L2_CID_SYSTEM_MAX_INTEGRATION_TIME:
        ret = fw_intf_get_system_max_integration_time( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_max_integration_time: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_EXPOSURE_RATIO:
        ret = fw_intf_get_system_exposure_ratio( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_exposure_ratio: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SYSTEM_SENSOR_ANALOG_GAIN:
        ret = fw_intf_get_system_sensor_analog_gain( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_sensor_analog_gain: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SYSTEM_MAX_SENSOR_ANALOG_GAIN:
        ret = fw_intf_get_system_max_sensor_analog_gain( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_max_sensor_analog_gain: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_SENSOR_DIGITAL_GAIN:
        ret = fw_intf_get_system_sensor_digital_gain( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_sensor_digital_gain: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SYSTEM_MAX_SENSOR_DIGITAL_GAIN:
        ret = fw_intf_get_system_max_sensor_digital_gain( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_max_sensor_digital_gain: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_ISP_DIGITAL_GAIN:
        ret = fw_intf_get_system_isp_digital_gain( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_isp_digital_gain: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SYSTEM_MAX_ISP_DIGITAL_GAIN:
        ret = fw_intf_get_system_max_isp_digital_gain( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_max_isp_digital_gain: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_SYSTEM_AWB_RED_GAIN:
        ret = fw_intf_get_system_awb_red_gain( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_awb_red_gain: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SYSTEM_AWB_GREEN_EVEN_GAIN:
        ret = fw_intf_get_system_awb_green_even_gain( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_awb_green_even_gain: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SYSTEM_AWB_GREEN_ODD_GAIN:
        ret = fw_intf_get_system_awb_green_odd_gain( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_awb_green_odd_gain: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SYSTEM_AWB_BLUE_GAIN:
        ret = fw_intf_get_system_awb_blue_gain( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_awb_blue_gain: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SYSTEM_CCM_MATRIX:
        ret = fw_intf_get_system_ccm_matrix( ctx_id, ctrl->p_new.p_s32, ctrl->elems );
        LOG( LOG_INFO, "get system_ccm_matrix: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_SHADING_STRENGTH:
        ret = fw_intf_get_shading_strength( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get shading_strength: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SYSTEM_RGB_GAMMA_LUT:
        ret = fw_intf_get_system_rgb_gamma_lut( ctx_id, ctrl->p_new.p_u16, ctrl->elems );
        LOG( LOG_INFO, "get system_rgb_gamma_lut: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_CUSTOM_AE_ZONE_WEIGHT:
        ret = fw_intf_get_customer_ae_zone_weight( ctx_id, (ctrl->p_new.p_u32), ctrl->elems );
        LOG( LOG_INFO, "get ae_zone_weight: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_CUSTOM_AWB_ZONE_WEIGHT:
        ret = fw_intf_get_customer_awb_zone_weight( ctx_id, (ctrl->p_new.p_u32), ctrl->elems );
        LOG( LOG_INFO, "get awb_zone_weight: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_SYSTEM_IQ_CALIBRATION_DYNAMIC:
        ret = fw_intf_get_system_calibration_dynamic( ctx_id, ctrl->p_new.p_u16, ctrl->elems );
        LOG( LOG_INFO, "get system_calibration_dynamic from user space: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_SYSTEM_STATUATION_TARGET:
        ret = fw_intf_get_saturation_target( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get saturation_target: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_IRIDIX:
        ret = fw_intf_get_isp_modules_manual_iridix( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get isp_modules_manual_iridix: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_SINTER:
        ret = fw_intf_get_isp_modules_manual_sinter( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get isp_modules_manual_sinter: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_TEMPER:
        ret = fw_intf_get_isp_modules_manual_temper( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get isp_modules_manual_temper: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_AUTO_LEVEL:
        ret = fw_intf_get_isp_modules_manual_auto_level( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get isp_modules_manual_auto_level: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_FRAME_STITCH:
        ret = fw_intf_get_isp_modules_manual_frame_stitch( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get isp_modules_manual_frame_stitch: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_RAW_FRONTEND:
        ret = fw_intf_get_isp_modules_manual_raw_frontend( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get isp_modules_manual_raw_frontend: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_BLACK_LEVEL:
        ret = fw_intf_get_isp_modules_manual_black_level( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get isp_modules_manual_black_level: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_SHADING:
        ret = fw_intf_get_isp_modules_manual_shading( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get isp_modules_manual_shading: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_DEMOSAIC:
        ret = fw_intf_get_isp_modules_manual_demosaic( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get isp_modules_manual_demosaic: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_CNR:
        ret = fw_intf_get_isp_modules_manual_cnr( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get isp_modules_manual_cnr: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_SHARPEN:
        ret = fw_intf_get_isp_modules_manual_sharpen( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get isp_modules_manual_sharpen: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_ISP_MODULES_MANUAL_PF:
        ret = fw_intf_get_isp_modules_manual_pf( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get manual_pf: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_STATUS_ISO:
        ret = fw_intf_get_isp_status_iso( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get iso: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_IMAGE_RESIZE_ENABLE_ID:
        ret = fw_intf_get_image_resize_enable( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get image_resize_enable: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_IMAGE_RESIZE_TYPE_ID:
        ret = fw_intf_get_image_resize_type( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get image_resize_type: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_IMAGE_CROP_XOFFSET_ID:
        ret = fw_intf_get_image_crop_xoffset( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get image_crop_xoffset: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_IMAGE_CROP_YOFFSET_ID:
        ret = fw_intf_get_image_crop_yoffset( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get image_crop_yoffset: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_IMAGE_RESIZE_WIDTH_ID:
        ret = fw_intf_get_image_resize_width( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get image_resize_width: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_IMAGE_RESIZE_HEIGHT_ID:
        ret = fw_intf_get_image_resize_height( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get image_resize_height: %d.\n", ctrl->val );
        break;
    case ISP_V4L2_CID_STATUS_INFO_EXPOSURE_LOG2:
        ret = fw_intf_get_status_info_exposure_log2( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get status_info_exposure_log2: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_STATUS_INFO_GAIN_LOG2:
        ret = fw_intf_get_status_info_gain_log2( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get status_info_gain_log2: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_STATUS_INFO_GAIN_ONES:
        ret = fw_intf_get_status_info_gain_ones( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get status_info_gain_ones: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_STATUS_INFO_AWB_MIX_LIGHT_CONTRAST:
        ret = fw_intf_get_status_info_awb_mix_light_contrast( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get status_info_awb_mix_light_contrast: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_STATUS_INFO_AF_LENS_POS:
        ret = fw_intf_get_status_info_af_lens_pos( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get status_info_af_lens_pos: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_STATUS_INFO_AF_FOCUS_VALUE:
        ret = fw_intf_get_status_info_af_focus_value( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get status_info_af_focus_value: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_INFO_FW_REVISION:
        ret = fw_intf_get_info_fw_revision( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get info_fw_revision: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SENSOR_LINES_PER_SECOND:
        ret = fw_intf_get_lines_per_second( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_lines_per_second: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SENSOR_INFO_PHYSICAL_WIDTH:
        ret = fw_intf_get_sensor_info_physical_width( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_sensor_info_physical_width: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SENSOR_VMAX_FPS:
        ret = fw_intf_get_sensor_vmax_fps( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_sensor_fps: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SENSOR_INFO_PHYSICAL_HEIGHT:
        ret = fw_intf_get_sensor_info_physical_height( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_sensor_info_physical_height: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_LENS_INFO_MINFOCUS_DISTANCE:
        ret = fw_intf_get_lens_info_minfocus_distance( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_lens_info_minfocus_distance: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_LENS_INFO_HYPERFOCAL_DISTANCE:
        ret = fw_intf_get_lens_info_hyperfocal_distance( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_lens_info_hyperfocal_distance: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_LENS_INFO_FOCAL_LENGTH:
        ret = fw_intf_get_lens_info_focal_length( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_lens_info_focal_length: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_LENS_INFO_APERTURE:
        ret = fw_intf_get_lens_info_aperture( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_lens_info_aperture: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_AE_COMPENSATION:
        ret = fw_intf_get_ae_compensation( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_ae_compensation: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SYSTEM_DYNAMIC_GAMMA_ENABLE:
        ret = fw_intf_get_system_dynamic_gamma_enable( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_system_dynamic_gamma_enable: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_AF_MODE_ID:
        ret = fw_intf_get_af_mode_id( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_af_mode_id: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_AF_MANUAL_CONTROL_ID:
        ret = fw_intf_get_af_manual_control( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_af_manual_control: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_AF_STATE_ID:
        ret = fw_intf_get_af_state_id( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_af_state_id: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_AE_STATE_ID:
        ret = fw_intf_get_ae_state_id( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_ae_state_id: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_AWB_STATE_ID:
        ret = fw_intf_get_awb_state_id( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_awb_state_id: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_AWB_MODE_ID:
        ret = fw_intf_get_awb_mode_id( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_awb_mode_id: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_AWB_TEMPERATURE:
        ret = fw_intf_get_awb_temperature( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_awb_temperature: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_NOISE_REDUCTION_MODE_ID:
        ret = fw_intf_get_noise_reduction_mode_id( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_noise_reduction_mode_id: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_AE_STATS:
        ret = fw_intf_get_ae_stats( ctx_id, ctrl->p_new.p_u32, ctrl->elems );
        LOG( LOG_INFO, "get ae stats: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_AWB_STATS:
        ret = fw_intf_get_awb_stats( ctx_id, ctrl->p_new.p_u32, ctrl->elems );
        LOG( LOG_INFO, "get awb stats: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_AF_STATS:
        ret = fw_intf_get_af_stats( ctx_id, ctrl->p_new.p_u32, ctrl->elems );
        LOG( LOG_INFO, "get af stats: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_MD_STATS:
        ret = fw_intf_get_md_stats( ctx_id, ctrl->p_new.p_u32, ctrl->elems );
        LOG( LOG_INFO, "get md stats: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_FLICKER_STATS:
        ret = fw_intf_get_flicker_stats( ctx_id, ctrl->p_new.p_u32, ctrl->elems );
        LOG( LOG_INFO, "get flkr stats: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_DPC_THRES_SLOPE:
        ret = fw_intf_get_dpc_threshold_slope( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get dpc threshold and slope: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_BLACK_LEVEL_R:
        ret = fw_intf_get_black_level_r( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get black level r: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_BLACK_LEVEL_GR:
        ret = fw_intf_get_black_level_gr( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get black level gr: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_BLACK_LEVEL_GB:
        ret = fw_intf_get_black_level_gb( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get black level gb: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_BLACK_LEVEL_B:
        ret = fw_intf_get_black_level_b( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get black level b: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_SHARP_LU_DU_LD:
        ret = fw_intf_get_demosaic_sharp( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get demosaic sharp: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_CNR_STRENGTH:
        ret = fw_intf_get_cnr_strength( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get cnr strength: rc: %d\n", ret );
        break;
    case ISP_V4L2_CID_SYSTEM_ANTIFLICKER_ENABLE:
        ret = fw_intf_get_system_antiflicker_enable( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system_antiflicker_enable: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SYSTEM_ANTIFLICKER_FREQUENCY:
        ret = fw_intf_get_system_antiflicker_frequency( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get system antiflicker frequency : %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_ISP_MODULES_IRIDIX_ENABLE:
        ret = fw_intf_get_isp_modules_iridix_enable( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get isp module iridix enable : %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_ISP_MODULES_IRIDIX_STRENGTH:
        ret = fw_intf_get_isp_modules_iridix_strength( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "new isp module iridix strength : %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_ISP_MODULES_SWHW_REGISTERS:
        ret = fw_intf_get_isp_modules_swhw_registers( ctx_id, ctrl->p_new.p_u16 );
        LOG( LOG_INFO, "get isp swhw register : %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_AE_GET_GAIN:
        ret = fw_intf_get_gain(ctx_id,&ctrl->val);
        LOG( LOG_INFO, "get gain: %d.\n",ret );
        break;
    case ISP_V4L2_CID_AE_GET_EXPOSURE:
        ret = fw_intf_get_exposure(ctx_id,&ctrl->val);
        LOG( LOG_INFO, "get exposure: %d.\n",ret );
        break;
    case ISP_V4L2_CID_ISP_MODULES_FR_SHARPEN_STRENGTH:
        ret = fw_intf_get_isp_modules_fr_sharpen_strength( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "new isp module fr sharpen strength : %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_ISP_MODULES_DS1_SHARPEN_STRENGTH:
        ret = fw_intf_get_isp_modules_ds1_sharpen_strength( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "new isp module ds1 sharpen strength : %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_ISP_MODULES_BYPASS:
        ret = fw_intf_get_isp_modules_bypass( ctx_id, ctrl->p_new.p_u32 );
        LOG( LOG_INFO, "get isp module by pass rc: %d.\n", ret );
        break;
    case ISP_V4L2_CID_ISP_DAYNIGHT_DETECT:
        ret = fw_intf_get_day_night_detect( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_day_night_detect: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_CMOS_HIST_MEAN:
        ret = fw_intf_get_hist_mean( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_hist_mean: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_AE_TARGET:
        ret = fw_intf_get_ae_target( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_ae_target: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_CUSTOM_FR_FPS:
        ret = fw_intf_get_custom_fr_fps( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_custom_fr_fps: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SCALE_CROP_STARTX:
        ret = fw_intf_get_scale_crop_startx( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_scale_crop_startx: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SCALE_CROP_STARTY:
        ret = fw_intf_get_scale_crop_starty( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_scale_crop_starty: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SCALE_CROP_WIDTH:
        ret = fw_intf_get_scale_crop_width( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_scale_crop_width: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SCALE_CROP_HEIGHT:
        ret = fw_intf_get_scale_crop_height( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_scale_crop_height: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_GET_STREAM_STATUS:
        ret = fw_intf_get_stream_status( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "fw_intf_get_stream_status: %d, rc: %d.\n", ctrl->val, ret );
        break;
    case ISP_V4L2_CID_SCALER_FPS:
        ret = fw_intf_get_scaler_fps( ctx_id, &ctrl->val );
        LOG( LOG_INFO, "get sc fps:  %d, rc: %d.\n", ctrl->val, ret );
        break;
    default:
        ret = 1;
        break;
    }

    return ret;
}

static const struct v4l2_ctrl_ops isp_v4l2_ctrl_ops_custom = {
    .s_ctrl = isp_v4l2_ctrl_s_ctrl_custom,
    .g_volatile_ctrl = isp_v4l2_ctrl_g_ctrl_custom,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_test_pattern = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_TEST_PATTERN,
    .name = "ISP Test Pattern",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_test_pattern_type = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_TEST_PATTERN_TYPE,
    .name = "ISP Test Pattern Type",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 3,
    .step = 1,
    .def = 3,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_af_refocus = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AF_REFOCUS,
    .name = "ISP AF Refocus",
    .type = V4L2_CTRL_TYPE_BUTTON,
    .min = 0,
    .max = 0,
    .step = 0,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_sensor_preset = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SENSOR_PRESET,
    .name = "ISP Sensor Preset",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 7,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_ae_roi = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AE_ROI,
    .name = "ISP AE ROI",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7F7F7F7F,
    .step = 1,
    .def = 0x20206060,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_af_roi = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AF_ROI,
    .name = "ISP AF ROI",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7F7F7F7F,
    .step = 1,
    .def = 0x20206060,
};


static const struct v4l2_ctrl_config isp_v4l2_ctrl_output_fr_on_off = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_OUTPUT_FR_ON_OFF,
    .name = "ISP FR ON/OFF",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7FFFFFFF,
    .step = 1,
    .def = 0,
};


static const struct v4l2_ctrl_config isp_v4l2_ctrl_output_ds1_on_off = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_OUTPUT_DS1_ON_OFF,
    .name = "ISP DS1 ON/OFF",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7FFFFFFF,
    .step = 1,
    .def = 0,
};


static const struct v4l2_ctrl_config isp_v4l2_ctrl_sensor_wdr_mode = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SENSOR_WDR_MODE,
    .name = "ISP Sensor wdr mode",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 3,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_sensor_exposure = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SENSOR_EXPOSURE,
    .name = "ISP Sensor exposure",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 1,
    .max = 4,
    .step = 1,
    .def = 1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_fr_fps = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_FR_FPS,
    .name = "ISP fr fps",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 120,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_ds1_fps = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SET_DS1_FPS,
    .name = "ISP ds1 fps",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 120,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_sensor_testpattern = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SET_SENSOR_TESTPATTERN,
    .name = "ISP Sensor test pattern",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 10,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_sensor_ir_cut = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SENSOR_IR_CUT,
    .name = "sensor ir cut set",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = -1,
    .max = 2,
    .step = 1,
    .def = -1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_ae_zone_weight = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_AE_ZONE_WEIGHT,
    .name = "set/get isp ae zone weight",
    .type = V4L2_CTRL_TYPE_U8,
    .min = 0,
    .max = 0xff,
    .step = 1,
    .def = 15,
    .dims = { 33*33 },
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_awb_zone_weight = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_AWB_ZONE_WEIGHT,
    .name = "set/get isp awb zone weight",
    .type = V4L2_CTRL_TYPE_U8,
    .min = 0,
    .max = 0xff,
    .step = 1,
    .def = 15,
    .dims = { 33*33 },
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_manual_exposure = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SET_MANUAL_EXPOSURE,
    .name = "manual_exposure set",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = -1,
    .max = 2,
    .step = 1,
    .def = -1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_sensor_integration_time = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SET_SENSOR_INTEGRATION_TIME,
    .name = "sensor_integration_timet set",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = -1,
    .max = 4000,
    .step = 1,
    .def = -1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_sensor_analog_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SET_SENSOR_ANALOG_GAIN,
    .name = "sensor_analog_gain set",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = -1,
    .max = 256,
    .step = 1,
    .def = -1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_digital_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SET_ISP_DIGITAL_GAIN,
    .name = "isp_digital_gain set",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = -1,
    .max = 256,
    .step = 1,
    .def = -1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_stop_sensor_update = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SET_STOP_SENSOR_UPDATE,
    .name = "stop_sensor_update set",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = -1,
    .max = 2,
    .step = 1,
    .def = -1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_ae_compensation = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AE_COMPENSATION,
    .name = "ISP AE Compensation",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 255,
    .step = 1,
    .def = 128,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_sensor_digital_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SET_SENSOR_DIGITAL_GAIN,
    .name = "sensor_digital_gain set",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = -1,
    .max = 256,
    .step = 1,
    .def = -1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_awb_red_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SET_AWB_RED_GAIN,
    .name = "awb_red_gain set",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = -1,
    .max = 65535,
    .step = 1,
    .def = -1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_awb_blue_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SET_AWB_BLUE_GAIN,
    .name = "awb_blue_gain set",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = -1,
    .max = 65535,
    .step = 1,
    .def = -1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_max_integration_time = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SET_MAX_INTEGRATION_TIME,
    .name = "max_int_time set",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = -1,
    .max = 5564,
    .step = 1,
    .def = -1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_sensor_fps = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SENSOR_FPS,
    .name = "ISP Sensor fps",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 120,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_snr_manual = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SNR_MANUAL,
    .name = "ISP SNR manual",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_snr_strength = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_SNR_STRENGTH,
    .name = "ISP SNR strength",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 255,
    .step = 1,
    .def = 128,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_tnr_manual = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_TNR_MANUAL,
    .name = "ISP TNR manual",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_tnr_offset = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_TNR_OFFSET,
    .name = "ISP TNR offset",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 255,
    .step = 1,
    .def = 128,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_temper_mode = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_TEMPER_MODE,
    .name = "ISP Temper mode",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 1,
    .max = 2,
    .step = 1,
    .def = 1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_wdr_mode = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_WDR_SWITCH,
    .name = "ISP WDR mode",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 2,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_antiflicker = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_ANTIFLICKER,
    .name = "ISP sensor AntiFlicker mode",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 60,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_defog_mode = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_DEFOG_SWITCH,
    .name = "Defog alg mode",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 2,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_defog_ratio = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_DEFOG_STRENGTH,
    .name = "Defog alg ratio delta",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 4096,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_calibration = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CUSTOM_CALIBRATION,
    .name = "ISP customer cali special",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 15,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_freeze_firmware = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_FREEZE_FIRMWARE,
    .name = "Freeze ISP firmware",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_manual_exposure = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MANUAL_EXPOSURE,
    .name = "Enable manual exposure",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_manual_integration_time = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MANUAL_INTEGRATION_TIME,
    .name = "Enable manual integration time",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_manual_max_integration_time = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MANUAL_MAX_INTEGRATION_TIME,
    .name = "Enable manual max integration time",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_manual_sensor_analog_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MANUAL_SENSOR_ANALOG_GAIN,
    .name = "Enable manual sensor analog gain",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_manual_sensor_digital_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN,
    .name = "Enable manual sensor digital gain",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_manual_isp_digital_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MANUAL_ISP_DIGITAL_GAIN,
    .name = "Enable manual ISP digital gain",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_manual_exposure_ratio = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MANUAL_EXPOSURE_RATIO,
    .name = "Enable manual exposure ratio",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_manual_awb = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MANUAL_AWB,
    .name = "Enable manual AWB",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_manual_ccm = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MANUAL_CCM,
    .name = "Enable manual CCM",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_manual_saturation = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MANUAL_SATURATION,
    .name = "Enable manual saturation",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_max_exposure_ratio = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MAX_EXPOSURE_RATIO,
    .name = "Set maximum exposure ratio",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 1,
    .max = 16777216, // 64^4
    .step = 1,
    .def = 1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_exposure = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_EXPOSURE,
    .name = "Set current exposure",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7FFFFFFF,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_integration_time = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_INTEGRATION_TIME,
    .name = "Set current integration time",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7FFFFFFF,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_long_integration_time = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_LONG_INTEGRATION_TIME,
    .name = "get current long integration time",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7FFFFFFF,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_short_integration_time = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_SHORT_INTEGRATION_TIME,
    .name = "get current short integration time",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7FFFFFFF,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_exposure_ratio = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_EXPOSURE_RATIO,
    .name = "Set current exposure ratio",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7FFFFFFF,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_max_integration_time = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MAX_INTEGRATION_TIME,
    .name = "Set max integration time",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7FFFFFFF,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_sensor_analog_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_SENSOR_ANALOG_GAIN,
    .name = "Set sensor analog gain",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 255,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_max_sensor_analog_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MAX_SENSOR_ANALOG_GAIN,
    .name = "Set max sensor analog gain",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 255,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_sensor_digital_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_SENSOR_DIGITAL_GAIN,
    .name = "Set sensor digital gain",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 255,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_max_sensor_digital_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MAX_SENSOR_DIGITAL_GAIN,
    .name = "Set max sensor digital gain",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 255,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_isp_digital_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_ISP_DIGITAL_GAIN,
    .name = "Set isp digital gain",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 255,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_max_isp_digital_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_MAX_ISP_DIGITAL_GAIN,
    .name = "Set max isp digital gain",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 255,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_awb_red_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_AWB_RED_GAIN,
    .name = "Set awb red gain",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 65535,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_awb_green_even_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_AWB_GREEN_EVEN_GAIN,
    .name = "Set awb green_even gain",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 65535,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_awb_green_odd_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_AWB_GREEN_ODD_GAIN,
    .name = "Set awb green_odd gain",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 65535,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_awb_blue_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_AWB_BLUE_GAIN,
    .name = "Set awb blue gain",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 65535,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_ccm_matrix = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_CCM_MATRIX,
    .name = "Set/Get the ccm matrix coefficients",
    .type = V4L2_CTRL_TYPE_U16,
    .min = 0,
    .max = 65535,
    .step = 1,
    .def = 0,
    .dims = { ISP_V4L2_CCM_MATRIX_SZ },
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_shading_strength = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SHADING_STRENGTH,
    .name = "Set/Get shading strength",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 4096,
    .step = 1,
    .def = 2048,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_rgb_gamma_lut = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_RGB_GAMMA_LUT,
    .name = "Set/Get the RGB gamma lut",
    .type = V4L2_CTRL_TYPE_U16,
    .min = 0,
    .max = 0xffff,
    .step = 1,
    .def = 0,
    .dims = { ISP_V4L2_RGB_GAMMA_LUT_SZ },
};
static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_iq_calibration_dynamic = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_IQ_CALIBRATION_DYNAMIC,
    .name = "Set/Get IQ calibration",
    .type = V4L2_CTRL_TYPE_U16,
    .min = 0,
    .max = 0xffff,
    .step = 1,
    .def = 0,
    .dims = { ISP_V4L2_IQ_CALIBRATION_MAX },
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_saturation_target = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_STATUATION_TARGET,
    .name = "Set/Get the saturation target",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 255,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_manual_iridix = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_MANUAL_IRIDIX,
    .name = "Enable manual iridix",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_manual_sinter = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_MANUAL_SINTER,
    .name = "Enable manual sinter",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_manual_temper = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_MANUAL_TEMPER,
    .name = "Enable manual temper",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_manual_auto_level = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_MANUAL_AUTO_LEVEL,
    .name = "Enable manual auto level",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_manual_frame_stitch = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_MANUAL_FRAME_STITCH,
    .name = "Enable manual frame stitch",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_manual_raw_frontend = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_MANUAL_RAW_FRONTEND,
    .name = "Enable manual raw frontend",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_manual_black_level = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_MANUAL_BLACK_LEVEL,
    .name = "Enable manual black level",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_manual_shading = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_MANUAL_SHADING,
    .name = "Enable manual shading",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_manual_demosaic = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_MANUAL_DEMOSAIC,
    .name = "Enable manual demosaic",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_manual_cnr = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_MANUAL_CNR,
    .name = "Enable manual CNR",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_manual_sharpen = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_MANUAL_SHARPEN,
    .name = "Enable manual sharpen",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_manual_pf = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_MANUAL_PF,
    .name = "Enable manual pf",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_status_iso = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_STATUS_ISO,
    .name = "get iso value",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 100,
    .max = 6400000,
    .step = 1,
    .def = 100,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_image_resize_enable = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_IMAGE_RESIZE_ENABLE_ID,
    .name = "Enable resize",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_image_resize_type = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_IMAGE_RESIZE_TYPE_ID,
    .name = "Image resize_type",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 2,
    .step = 1,
    .def = 2,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_image_crop_xoffset = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_IMAGE_CROP_XOFFSET_ID,
    .name = "Image crop xoffset",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 4000,
    .step = 1,
    .def = 1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_image_crop_yoffset = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_IMAGE_CROP_YOFFSET_ID,
    .name = "Image crop yoffset",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 4000,
    .step = 1,
    .def = 1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_image_resize_width = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_IMAGE_RESIZE_WIDTH_ID,
    .name = "Image resize width",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 5000,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_image_resize_height = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_IMAGE_RESIZE_HEIGHT_ID,
    .name = "Image resize height",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 5000,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_status_info_exposure_log2 = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_STATUS_INFO_EXPOSURE_LOG2,
    .name = "Get expososure log2",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_status_info_gain_log2 = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_STATUS_INFO_GAIN_LOG2,
    .name = "Get gain log2",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_status_info_gain_ones = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_STATUS_INFO_GAIN_ONES,
    .name = "Get gain ones",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_status_info_awb_mix_light_contrast = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_STATUS_INFO_AWB_MIX_LIGHT_CONTRAST,
    .name = "Get AWB mix light contrast",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_status_info_af_lens_pos = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_STATUS_INFO_AF_LENS_POS,
    .name = "Get AF lens position",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_status_info_af_focus_value = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_STATUS_INFO_AF_FOCUS_VALUE,
    .name = "Get AF focus value",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_info_fw_revision = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_INFO_FW_REVISION,
    .name = "Get firmware revision",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_sensor_lines_per_second = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SENSOR_LINES_PER_SECOND,
    .name = "Get Sensor Lines per second",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_sensor_info_physical_width = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SENSOR_INFO_PHYSICAL_WIDTH,
    .name = "Get SENSOR_INFO_PHYSICAL_WIDTH",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_sensor_info_physical_height = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SENSOR_INFO_PHYSICAL_HEIGHT,
    .name = "Get SENSOR_INFO_PHYSICAL_HEIGHT",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_lens_info_minfocus_distance = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_LENS_INFO_MINFOCUS_DISTANCE,
    .name = "Get ISP_V4L2_CID_LENS_INFO_MINFOCUS_DISTANCE",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_lens_info_hyperfocal_distance = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_LENS_INFO_HYPERFOCAL_DISTANCE,
    .name = "Get ISP_V4L2_CID_LENS_INFO_HYPERFOCAL_DISTANCE",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_lens_info_focal_length = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_LENS_INFO_FOCAL_LENGTH,
    .name = "Get ISP_V4L2_CID_LENS_INFO_FOCAL_LENGTH",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_lens_info_aperture = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_LENS_INFO_APERTURE,
    .name = "Get ISP_V4L2_CID_LENS_INFO_APERTURE",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_af_mode_id = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AF_MODE_ID,
    .name = "Set/Get AF MODE ID",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xff,
    .step = 1,
    .def = ISP_V4L2_AF_MODE_AUTO_SINGLE,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_af_manual_control = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AF_MANUAL_CONTROL_ID,
    .name = "Set/Get AF Manual Control",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_dynamic_gamma_enable = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_DYNAMIC_GAMMA_ENABLE,
    .name = "Enable/Disable Dynamic Gamma",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_af_state_id = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AF_STATE_ID,
    .name = "Get AF State",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_awb_mode_id = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AWB_MODE_ID,
    .name = "Set/Get AWB MODE ID",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xff,
    .step = 1,
    .def = ISP_V4L2_AWB_MODE_AUTO,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_awb_temperature = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AWB_TEMPERATURE,
    .name = "Get AWB TEMPERATURE",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xff,
    .step = 1,
    .def = 65,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_noise_reduction_mode_id = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_NOISE_REDUCTION_MODE_ID,
    .name = "Set/Get NR MODE ID",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xff,
    .step = 1,
    .def = ISP_V4L2_NOISE_REDUCTION_MODE_ON,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_ae_stats = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AE_STATS,
    .name = "Get the AE Expo hist",
    .type = V4L2_CTRL_TYPE_U32,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
    .dims = { ISP_V4L2_AE_STATS_SIZE },
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_md_stats = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_MD_STATS,
    .name = "Get the MD stats",
    .type = V4L2_CTRL_TYPE_U8,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
    .dims = { ISP_V4L2_MD_STATS_SIZE },
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_flicker_stats = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_FLICKER_STATS,
    .name = "Get the flkr stats",
    .type = V4L2_CTRL_TYPE_U8,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
    .dims = { ISP_V4L2_FLICKER_STATS_SIZE },
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_sensor_vmax = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SENSOR_VMAX_FPS,
    .name = "Set/Get sensor fps by vmax",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x100,
    .step = 1,
    .def = 30,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_awb_stats = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AWB_STATS,
    .name = "Get the AWB Expo hist",
    .type = V4L2_CTRL_TYPE_U32,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
    .dims = { ISP_V4L2_AWB_STATS_SIZE },
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_dpc_threshold_slope = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_DPC_THRES_SLOPE,
    .name = "Set/Get dpc threshold/slope",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xfff0fff,
    .step = 1,
    .def = 1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_black_level_r = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_BLACK_LEVEL_R,
    .name = "Set/Get black level r",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x1fffff,
    .step = 1,
    .def = 1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_black_level_gr = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_BLACK_LEVEL_GR,
    .name = "Set/Get black level gr",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x1fffff,
    .step = 1,
    .def = 1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_black_level_gb = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_BLACK_LEVEL_GB,
    .name = "Set/Get black level gb",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x1fffff,
    .step = 1,
    .def = 1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_black_level_b = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_BLACK_LEVEL_B,
    .name = "Set/Get black level b",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x1fffff,
    .step = 1,
    .def = 1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_demosaic_sharp = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SHARP_LU_DU_LD,
    .name = "Set/Get demosaic sharp",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffff,
    .step = 1,
    .def = 1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_cnr_strength = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CNR_STRENGTH,
    .name = "Set/Get cnr strength",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffff,
    .step = 1,
    .def = 1,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_antiflicker_enable = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_ANTIFLICKER_ENABLE,
    .name = "Set/Get antiflcker enable ",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x100,
    .step = 1,
    .def = 30,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_system_antiflicker_frequency = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SYSTEM_ANTIFLICKER_FREQUENCY,
    .name = "Set/Get antiflcker frequency ",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x100,
    .step = 1,
    .def = 50,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_iridix_enable = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_IRIDIX_ENABLE,
    .name = "Set/Get iridix enable 1:enable 0:disable ",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_iridix_strength = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_IRIDIX_STRENGTH,
    .name = "Set/Get iridix_strength ",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x3FF,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_swhw_registers = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_SWHW_REGISTERS,
    .name = "Set/Get the ISP sw hw register",
    .type = V4L2_CTRL_TYPE_U16,
    .min = 0,
    .max = 0xffff,
    .step = 1,
    .def = 0,
    .dims = { 6 },
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_ae_get_gain = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AE_GET_GAIN,
    .name = "Get ae gain",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_ae_get_exposure = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AE_GET_EXPOSURE,
    .name = "Get ae exposure",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_fr_sharpen_strength = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_FR_SHARPEN_STRENGTH,
    .name = "Set/Get fr sharpen trength ",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x1FF,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_ds1_sharpen_strength = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_DS1_SHARPEN_STRENGTH,
    .name = "Set/Get ds1 sharpen strength ",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x1FF,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_ae_state = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AE_STATE_ID,
    .name = "AE state",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 2,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_awb_state = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AWB_STATE_ID,
    .name = "AWB state",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 2,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_bypass = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_MODULES_BYPASS,
    .name = "get/set isp modules bypass",
    .type = V4L2_CTRL_TYPE_U32,
    .min = 0,
    .max = 0xFFFFFFFF,
    .step = 1,
    .def = 0,
    .dims = { 4 },
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_day_night = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_DAYNIGHT_DETECT,
    .name = "get/set daynight detect enable",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_hist_mean = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_CMOS_HIST_MEAN,
    .name = "get ae hist mean",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7FFFFFFF,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_ae_target = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AE_TARGET,
    .name = "get ae target",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7FFFFFFF,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_af_stats = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AF_STATS,
    .name = "Get the af stats",
    .type = V4L2_CTRL_TYPE_U32,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
    .dims = { ISP_V4L2_AF_STATS_SIZE },
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_scale_crop_startx = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SCALE_CROP_STARTX,
    .name = "Set/Get Aml SC Crop StartX",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7fffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_scale_crop_starty = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SCALE_CROP_STARTY,
    .name = "Set/Get Aml SC Crop StartY",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7fffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_scale_crop_width = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SCALE_CROP_WIDTH,
    .name = "Set/Get Aml SC Crop Width",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7fffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_scale_crop_height = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SCALE_CROP_HEIGHT,
    .name = "Set/Get Aml SC Crop Height",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7fffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_scale_crop_enable = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SCALE_CROP_ENABLE,
    .name = "Set/Get Aml SC Crop Enable",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7fffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_get_stream_status = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_GET_STREAM_STATUS,
    .name = "Get get stream status",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7fffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_set_sensor_power = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SET_SENSOR_POWER,
    .name = "Set sensor power",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7fffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_set_sensor_md_en = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SET_SENSOR_MD_EN,
    .name = "Set sensor md enable",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7fffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_set_sensor_decmpr_en = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SET_SENSOR_DECMPR_EN,
    .name = "Set sensor power",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7fffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_set_sensor_decmpr_lossless_en = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SET_SENSOR_DECMPR_LOSSLESS_EN,
    .name = "Set sensor power",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7fffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_set_sensor_flicker_en = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SET_SENSOR_FLICKER_EN,
    .name = "Set sensor power",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7fffffff,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_scaler_fps = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SCALER_FPS,
    .name = "Set/Get scaler fps",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 0x7fffffff,
    .step = 1,
    .def = 30,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_ae_roi_exact_coordinates = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AE_ROI_EXACT_COORDINATES,
    .name = "Set/Get ISP ae roi coordinates",
    .type = V4L2_CTRL_TYPE_U32,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
    .dims = { ISP_V4L2_RIO_SIZE },
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_af_roi_exact_coordinates = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_AF_ROI_EXACT_COORDINATES,
    .name = "Set/Get ISP af roi coordinates",
    .type = V4L2_CTRL_TYPE_U32,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
    .dims = { ISP_V4L2_RIO_SIZE },
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_is_capturing = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SET_IS_CAPTURING,
    .name = "Set is capturing or not",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 1,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_isp_modules_ae_fps_range = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_SET_FPS_RANGE,
    .name = "Set/Get sensor fps range",
    .type = V4L2_CTRL_TYPE_U32,
    .min = 0,
    .max = 0xffffffff,
    .step = 1,
    .def = 0,
    .dims = { 2 },
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_dcam_mode = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .id = ISP_V4L2_CID_ISP_DCAM_MODE,
    .name = "ISP Dcam mode",
    .type = V4L2_CTRL_TYPE_INTEGER,
    .min = 0,
    .max = 2,
    .step = 1,
    .def = 0,
};

static const struct v4l2_ctrl_ops isp_v4l2_ctrl_ops = {
    .s_ctrl = isp_v4l2_ctrl_s_ctrl_standard,
};

static const struct v4l2_ctrl_config isp_v4l2_ctrl_class = {
    .ops = &isp_v4l2_ctrl_ops_custom,
    .flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_WRITE_ONLY,
    .id = ISP_V4L2_CID_ISP_V4L2_CLASS,
    .name = "ARM ISP Controls",
    .type = V4L2_CTRL_TYPE_CTRL_CLASS,
};

#define ADD_CTRL_STD( id, min, max, step, def )                  \
    {                                                            \
        if ( fw_intf_validate_control( id ) ) {                  \
            v4l2_ctrl_new_std( hdl_std_ctrl, &isp_v4l2_ctrl_ops, \
                               id, min, max, step, def );        \
        }                                                        \
    }

#define ADD_CTRL_STD_MENU( id, max, skipmask, def )                   \
    {                                                                 \
        if ( fw_intf_validate_control( id ) ) {                       \
            v4l2_ctrl_new_std_menu( hdl_std_ctrl, &isp_v4l2_ctrl_ops, \
                                    id, max, skipmask, def );         \
        }                                                             \
    }

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION( 4, 1, 1 ) )
#define ADD_CTRL_CST_VOLATILE( id, cfg, priv )                          \
    {                                                                   \
        if ( fw_intf_validate_control( id ) ) {                         \
            tmp_ctrl = v4l2_ctrl_new_custom( hdl_cst_ctrl, cfg, priv ); \
            if ( tmp_ctrl )                                             \
                tmp_ctrl->flags |= ( V4L2_CTRL_FLAG_VOLATILE |          \
                                     V4L2_CTRL_FLAG_EXECUTE_ON_WRITE ); \
        }                                                               \
    }

#else
#define ADD_CTRL_CST_VOLATILE( id, cfg, priv )                          \
    {                                                                   \
        if ( fw_intf_validate_control( id ) ) {                         \
            tmp_ctrl = v4l2_ctrl_new_custom( hdl_cst_ctrl, cfg, priv ); \
            if ( tmp_ctrl )                                             \
                tmp_ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;             \
        }                                                               \
    }

#endif

#define ADD_CTRL_CST( id, cfg, priv )                        \
    {                                                        \
        if ( fw_intf_validate_control( id ) ) {              \
            v4l2_ctrl_new_custom( hdl_cst_ctrl, cfg, priv ); \
        }                                                    \
    }

static void update_ctrl_cfg_system_integration_time( uint32_t ctx_id, struct v4l2_ctrl_config *ctrl_cfg )
{
    int integration_time_min;
    int integration_time_max;
    int rc;

    *ctrl_cfg = isp_v4l2_ctrl_system_integration_time;

    rc = fw_intf_get_integration_time_limits( ctx_id, &integration_time_min, &integration_time_max );
    if ( rc == 0) {
        ctrl_cfg->min = integration_time_min;
        ctrl_cfg->max = integration_time_max;
        ctrl_cfg->def = integration_time_min;
    }

}

int isp_v4l2_ctrl_init( uint32_t ctx_id, isp_v4l2_ctrl_t *ctrl )
{
    struct v4l2_ctrl_handler *hdl_std_ctrl = &ctrl->ctrl_hdl_std_ctrl;
    struct v4l2_ctrl_handler *hdl_cst_ctrl = &ctrl->ctrl_hdl_cst_ctrl;
    struct v4l2_ctrl *tmp_ctrl;
    struct v4l2_ctrl_config tmp_ctrl_cfg;

    /* Init and add standard controls */
    v4l2_ctrl_handler_init( hdl_std_ctrl, 10 );
    v4l2_ctrl_new_custom( hdl_std_ctrl, &isp_v4l2_ctrl_class, NULL );

    /* general */
    ADD_CTRL_STD( V4L2_CID_BRIGHTNESS, 0, 255, 1, 128 );
    ADD_CTRL_STD( V4L2_CID_CONTRAST, 0, 255, 1, 128 );
    ADD_CTRL_STD( V4L2_CID_SATURATION, 0, 255, 1, 128 );
    ADD_CTRL_STD( V4L2_CID_HUE, 0, 360, 1, 180 );
    ADD_CTRL_STD( V4L2_CID_SHARPNESS, 0, 255, 1, 128 );
    ADD_CTRL_STD_MENU( V4L2_CID_COLORFX, 4, 0x1F0, 0 );
    /* orientation */
    ADD_CTRL_STD( V4L2_CID_HFLIP, 0, 1, 1, 0 );
    ADD_CTRL_STD( V4L2_CID_VFLIP, 0, 1, 1, 0 );
    /* exposure */
    ADD_CTRL_STD( V4L2_CID_AUTOGAIN, 0, 1, 1, 1 );
    ADD_CTRL_STD( V4L2_CID_GAIN, 100, 3200, 1, 100 );
    ADD_CTRL_STD_MENU( V4L2_CID_EXPOSURE_AUTO,
                       1, 0x0, 0 );
    ADD_CTRL_STD( V4L2_CID_EXPOSURE_ABSOLUTE,
                  1, 1000000, 1, 33 );
    ADD_CTRL_STD( V4L2_CID_EXPOSURE_AUTO_PRIORITY,
                  0, 1, 1, 0 );
    /* white balance */
    ADD_CTRL_STD( V4L2_CID_AUTO_WHITE_BALANCE,
                  0, 1, 1, 1 );
    ADD_CTRL_STD( V4L2_CID_WHITE_BALANCE_TEMPERATURE,
                  2000, 8000, 1000, 5000 );
    /* focus */
    ADD_CTRL_STD( V4L2_CID_FOCUS_AUTO, 0, 1, 1, 1 );
    ADD_CTRL_STD( V4L2_CID_FOCUS_ABSOLUTE,
                  0, 255, 1, 0 );

    /* scene : Max scene modes are increased to support custom scene modes from Camportal as well */
    ADD_CTRL_STD_MENU( V4L2_CID_SCENE_MODE, V4L2_SCENE_MODE_TEXT, ~0xfffffffff, V4L2_SCENE_MODE_NONE);

    /* Init and add custom controls */
    v4l2_ctrl_handler_init( hdl_cst_ctrl, 2 );
    v4l2_ctrl_new_custom( hdl_cst_ctrl, &isp_v4l2_ctrl_class, NULL );

    ADD_CTRL_CST( ISP_V4L2_CID_TEST_PATTERN, &isp_v4l2_ctrl_test_pattern, NULL );
    ADD_CTRL_CST( ISP_V4L2_CID_TEST_PATTERN_TYPE, &isp_v4l2_ctrl_test_pattern_type, NULL );
    ADD_CTRL_CST( ISP_V4L2_CID_AF_REFOCUS, &isp_v4l2_ctrl_af_refocus, NULL );
    ADD_CTRL_CST( ISP_V4L2_CID_SENSOR_PRESET, &isp_v4l2_ctrl_sensor_preset, NULL );
    ADD_CTRL_CST( ISP_V4L2_CID_AF_ROI, &isp_v4l2_ctrl_af_roi, NULL );
    ADD_CTRL_CST( ISP_V4L2_CID_OUTPUT_FR_ON_OFF, &isp_v4l2_ctrl_output_fr_on_off, NULL );
    ADD_CTRL_CST( ISP_V4L2_CID_OUTPUT_DS1_ON_OFF, &isp_v4l2_ctrl_output_ds1_on_off, NULL );
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SENSOR_WDR_MODE, &isp_v4l2_ctrl_sensor_wdr_mode, NULL );
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SENSOR_EXPOSURE, &isp_v4l2_ctrl_sensor_exposure, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_CUSTOM_FR_FPS, &isp_v4l2_ctrl_fr_fps, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SET_SENSOR_TESTPATTERN, &isp_v4l2_ctrl_sensor_testpattern, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SENSOR_IR_CUT, &isp_v4l2_ctrl_sensor_ir_cut, NULL);
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_CUSTOM_AE_ZONE_WEIGHT, &isp_v4l2_ctrl_ae_zone_weight, NULL);
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_CUSTOM_AWB_ZONE_WEIGHT, &isp_v4l2_ctrl_awb_zone_weight, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SET_MANUAL_EXPOSURE, &isp_v4l2_ctrl_manual_exposure, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SET_SENSOR_INTEGRATION_TIME, &isp_v4l2_ctrl_sensor_integration_time, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SET_SENSOR_ANALOG_GAIN, &isp_v4l2_ctrl_sensor_analog_gain, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SET_ISP_DIGITAL_GAIN, &isp_v4l2_ctrl_isp_digital_gain, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SET_STOP_SENSOR_UPDATE, &isp_v4l2_ctrl_stop_sensor_update, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SET_DS1_FPS, &isp_v4l2_ctrl_ds1_fps, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_AE_COMPENSATION, &isp_v4l2_ctrl_ae_compensation, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SET_SENSOR_DIGITAL_GAIN, &isp_v4l2_ctrl_sensor_digital_gain, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SET_AWB_RED_GAIN, &isp_v4l2_ctrl_awb_red_gain, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SET_AWB_BLUE_GAIN, &isp_v4l2_ctrl_awb_blue_gain, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SET_MAX_INTEGRATION_TIME, &isp_v4l2_ctrl_max_integration_time, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SENSOR_FPS, &isp_v4l2_ctrl_sensor_fps, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SNR_MANUAL, &isp_v4l2_ctrl_snr_manual, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_SNR_STRENGTH, &isp_v4l2_ctrl_snr_strength, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_TNR_MANUAL, &isp_v4l2_ctrl_tnr_manual, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_TNR_OFFSET, &isp_v4l2_ctrl_tnr_offset, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_TEMPER_MODE, &isp_v4l2_ctrl_temper_mode, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_WDR_SWITCH, &isp_v4l2_ctrl_wdr_mode, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_DEFOG_SWITCH, &isp_v4l2_ctrl_defog_mode, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_DEFOG_STRENGTH, &isp_v4l2_ctrl_defog_ratio, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_ANTIFLICKER, &isp_v4l2_ctrl_antiflicker, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_CUSTOM_CALIBRATION,  &isp_v4l2_ctrl_calibration, NULL);
    ADD_CTRL_CST( ISP_V4L2_CID_AE_ROI, &isp_v4l2_ctrl_ae_roi, NULL );

    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_FREEZE_FIRMWARE, &isp_v4l2_ctrl_system_freeze_firmware, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MANUAL_EXPOSURE, &isp_v4l2_ctrl_system_manual_exposure, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MANUAL_INTEGRATION_TIME, &isp_v4l2_ctrl_system_manual_integration_time, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MANUAL_MAX_INTEGRATION_TIME, &isp_v4l2_ctrl_system_manual_max_integration_time, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MANUAL_SENSOR_ANALOG_GAIN, &isp_v4l2_ctrl_system_manual_sensor_analog_gain, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN, &isp_v4l2_ctrl_system_manual_sensor_digital_gain, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MANUAL_ISP_DIGITAL_GAIN, &isp_v4l2_ctrl_system_manual_isp_digital_gain, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MANUAL_EXPOSURE_RATIO, &isp_v4l2_ctrl_system_manual_exposure_ratio, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MANUAL_AWB, &isp_v4l2_ctrl_system_manual_awb, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MANUAL_SATURATION, &isp_v4l2_ctrl_system_manual_saturation, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MAX_EXPOSURE_RATIO, &isp_v4l2_ctrl_system_max_exposure_ratio, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_EXPOSURE, &isp_v4l2_ctrl_system_exposure, NULL );

    update_ctrl_cfg_system_integration_time( ctx_id, &tmp_ctrl_cfg );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_INTEGRATION_TIME, &tmp_ctrl_cfg, NULL );

    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_EXPOSURE_RATIO, &isp_v4l2_ctrl_system_exposure_ratio, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MAX_INTEGRATION_TIME, &isp_v4l2_ctrl_system_max_integration_time, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_SENSOR_ANALOG_GAIN, &isp_v4l2_ctrl_system_sensor_analog_gain, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MAX_SENSOR_ANALOG_GAIN, &isp_v4l2_ctrl_system_max_sensor_analog_gain, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_SENSOR_DIGITAL_GAIN, &isp_v4l2_ctrl_system_sensor_digital_gain, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MAX_SENSOR_DIGITAL_GAIN, &isp_v4l2_ctrl_system_max_sensor_digital_gain, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_ISP_DIGITAL_GAIN, &isp_v4l2_ctrl_system_isp_digital_gain, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MAX_ISP_DIGITAL_GAIN, &isp_v4l2_ctrl_system_max_isp_digital_gain, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_AWB_RED_GAIN, &isp_v4l2_ctrl_system_awb_red_gain, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_AWB_BLUE_GAIN, &isp_v4l2_ctrl_system_awb_blue_gain, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_MANUAL_IRIDIX, &isp_v4l2_ctrl_isp_modules_manual_iridix, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_MANUAL_SINTER, &isp_v4l2_ctrl_isp_modules_manual_sinter, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_MANUAL_TEMPER, &isp_v4l2_ctrl_isp_modules_manual_temper, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_MANUAL_AUTO_LEVEL, &isp_v4l2_ctrl_isp_modules_manual_auto_level, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_MANUAL_FRAME_STITCH, &isp_v4l2_ctrl_isp_modules_manual_frame_stitch, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_MANUAL_RAW_FRONTEND, &isp_v4l2_ctrl_isp_modules_manual_raw_frontend, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_MANUAL_BLACK_LEVEL, &isp_v4l2_ctrl_isp_modules_manual_black_level, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_MANUAL_SHADING, &isp_v4l2_ctrl_isp_modules_manual_shading, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_MANUAL_DEMOSAIC, &isp_v4l2_ctrl_isp_modules_manual_demosaic, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_MANUAL_CNR, &isp_v4l2_ctrl_isp_modules_manual_cnr, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_MANUAL_SHARPEN, &isp_v4l2_ctrl_isp_modules_manual_sharpen, NULL );


    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_IMAGE_RESIZE_ENABLE_ID, &isp_v4l2_ctrl_image_resize_enable, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_IMAGE_RESIZE_TYPE_ID, &isp_v4l2_ctrl_image_resize_type, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_IMAGE_CROP_XOFFSET_ID, &isp_v4l2_ctrl_image_crop_xoffset, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_IMAGE_CROP_YOFFSET_ID, &isp_v4l2_ctrl_image_crop_yoffset, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_IMAGE_RESIZE_WIDTH_ID, &isp_v4l2_ctrl_image_resize_width, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_IMAGE_RESIZE_HEIGHT_ID, &isp_v4l2_ctrl_image_resize_height, NULL );


    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_STATUS_INFO_EXPOSURE_LOG2, &isp_v4l2_ctrl_status_info_exposure_log2, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_STATUS_INFO_GAIN_LOG2, &isp_v4l2_ctrl_status_info_gain_log2, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_STATUS_INFO_GAIN_ONES, &isp_v4l2_ctrl_status_info_gain_ones, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_STATUS_INFO_AWB_MIX_LIGHT_CONTRAST, &isp_v4l2_ctrl_status_info_awb_mix_light_contrast, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_STATUS_INFO_AF_LENS_POS, &isp_v4l2_ctrl_status_info_af_lens_pos, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_STATUS_INFO_AF_FOCUS_VALUE, &isp_v4l2_ctrl_status_info_af_focus_value, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_INFO_FW_REVISION, &isp_v4l2_ctrl_info_fw_revision, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SENSOR_LINES_PER_SECOND, &isp_v4l2_ctrl_sensor_lines_per_second, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_DYNAMIC_GAMMA_ENABLE, &isp_v4l2_ctrl_system_dynamic_gamma_enable, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_AF_MODE_ID, &isp_v4l2_ctrl_af_mode_id, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_AF_MANUAL_CONTROL_ID, &isp_v4l2_ctrl_af_manual_control, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_AWB_MODE_ID, &isp_v4l2_ctrl_awb_mode_id, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_AWB_TEMPERATURE, &isp_v4l2_ctrl_awb_temperature, NULL );

    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SENSOR_INFO_PHYSICAL_WIDTH, &isp_v4l2_ctrl_sensor_info_physical_width, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SENSOR_INFO_PHYSICAL_HEIGHT, &isp_v4l2_ctrl_sensor_info_physical_height, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_LENS_INFO_MINFOCUS_DISTANCE, &isp_v4l2_ctrl_lens_info_minfocus_distance, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_LENS_INFO_HYPERFOCAL_DISTANCE, &isp_v4l2_ctrl_lens_info_hyperfocal_distance, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_LENS_INFO_FOCAL_LENGTH, &isp_v4l2_ctrl_lens_info_focal_length, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_LENS_INFO_APERTURE, &isp_v4l2_ctrl_lens_info_aperture, NULL );

    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_AWB_GREEN_EVEN_GAIN, &isp_v4l2_ctrl_system_awb_green_even_gain, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_AWB_GREEN_ODD_GAIN, &isp_v4l2_ctrl_system_awb_green_odd_gain, NULL );

    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_NOISE_REDUCTION_MODE_ID, &isp_v4l2_ctrl_noise_reduction_mode_id, NULL );

    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_CCM_MATRIX, &isp_v4l2_ctrl_system_ccm_matrix, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_MANUAL_CCM, &isp_v4l2_ctrl_system_manual_ccm, NULL );

    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SHADING_STRENGTH, &isp_v4l2_ctrl_shading_strength, NULL );

    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_RGB_GAMMA_LUT, &isp_v4l2_ctrl_system_rgb_gamma_lut, NULL );

    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_STATUATION_TARGET, &isp_v4l2_ctrl_system_saturation_target, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_AE_STATS, &isp_v4l2_ctrl_ae_stats, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SENSOR_VMAX_FPS, &isp_v4l2_ctrl_sensor_vmax, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_IQ_CALIBRATION_DYNAMIC, &isp_v4l2_ctrl_system_iq_calibration_dynamic, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_AWB_STATS, &isp_v4l2_ctrl_awb_stats, NULL );

    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_DPC_THRES_SLOPE, &isp_v4l2_ctrl_dpc_threshold_slope, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_BLACK_LEVEL_R, &isp_v4l2_ctrl_black_level_r, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_BLACK_LEVEL_GR, &isp_v4l2_ctrl_black_level_gr, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_BLACK_LEVEL_GB, &isp_v4l2_ctrl_black_level_gb, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_BLACK_LEVEL_B, &isp_v4l2_ctrl_black_level_b, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SHARP_LU_DU_LD, &isp_v4l2_ctrl_demosaic_sharp, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_CNR_STRENGTH, &isp_v4l2_ctrl_cnr_strength, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_ANTIFLICKER_ENABLE, &isp_v4l2_ctrl_system_antiflicker_enable, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_ANTIFLICKER_FREQUENCY, &isp_v4l2_ctrl_system_antiflicker_frequency, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_IRIDIX_ENABLE, &isp_v4l2_ctrl_isp_modules_iridix_enable, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_IRIDIX_STRENGTH, &isp_v4l2_ctrl_isp_modules_iridix_strength, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_SWHW_REGISTERS, &isp_v4l2_ctrl_isp_modules_swhw_registers, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_AE_GET_GAIN, &isp_v4l2_ctrl_ae_get_gain, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_AE_GET_EXPOSURE, &isp_v4l2_ctrl_ae_get_exposure, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_FR_SHARPEN_STRENGTH, &isp_v4l2_ctrl_isp_modules_fr_sharpen_strength, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_DS1_SHARPEN_STRENGTH, &isp_v4l2_ctrl_isp_modules_ds1_sharpen_strength, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_AE_STATE_ID, &isp_v4l2_ctrl_ae_state, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_AWB_STATE_ID, &isp_v4l2_ctrl_awb_state, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_BYPASS, &isp_v4l2_ctrl_isp_modules_bypass, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_DAYNIGHT_DETECT, &isp_v4l2_ctrl_isp_day_night, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_LONG_INTEGRATION_TIME, &isp_v4l2_ctrl_system_long_integration_time, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SYSTEM_SHORT_INTEGRATION_TIME, &isp_v4l2_ctrl_system_short_integration_time, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_CMOS_HIST_MEAN, &isp_v4l2_ctrl_isp_hist_mean, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_AE_TARGET, &isp_v4l2_ctrl_isp_ae_target, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_MODULES_MANUAL_PF, &isp_v4l2_ctrl_isp_modules_manual_pf, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_STATUS_ISO, &isp_v4l2_ctrl_isp_status_iso, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_AF_STATS, &isp_v4l2_ctrl_af_stats, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SCALE_CROP_STARTX, &isp_v4l2_ctrl_scale_crop_startx, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SCALE_CROP_STARTY, &isp_v4l2_ctrl_scale_crop_starty, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SCALE_CROP_WIDTH, &isp_v4l2_ctrl_scale_crop_width, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SCALE_CROP_HEIGHT, &isp_v4l2_ctrl_scale_crop_height, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SCALE_CROP_ENABLE, &isp_v4l2_ctrl_scale_crop_enable, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_GET_STREAM_STATUS, &isp_v4l2_ctrl_get_stream_status, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SET_SENSOR_POWER, &isp_v4l2_ctrl_set_sensor_power, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SET_SENSOR_MD_EN, &isp_v4l2_ctrl_set_sensor_md_en, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SET_SENSOR_DECMPR_EN, &isp_v4l2_ctrl_set_sensor_decmpr_en, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SET_SENSOR_DECMPR_LOSSLESS_EN, &isp_v4l2_ctrl_set_sensor_decmpr_lossless_en, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SET_SENSOR_FLICKER_EN, &isp_v4l2_ctrl_set_sensor_flicker_en, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_MD_STATS, &isp_v4l2_ctrl_md_stats, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SCALER_FPS, &isp_v4l2_ctrl_scaler_fps, NULL);
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_FLICKER_STATS, &isp_v4l2_ctrl_flicker_stats, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_AE_ROI_EXACT_COORDINATES, &isp_v4l2_ctrl_isp_modules_ae_roi_exact_coordinates, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_AF_ROI_EXACT_COORDINATES, &isp_v4l2_ctrl_isp_modules_ae_roi_exact_coordinates, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SET_IS_CAPTURING, &isp_v4l2_ctrl_isp_modules_is_capturing, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_SET_FPS_RANGE, &isp_v4l2_ctrl_isp_modules_ae_fps_range, NULL );
    ADD_CTRL_CST_VOLATILE( ISP_V4L2_CID_ISP_DCAM_MODE, &isp_v4l2_ctrl_dcam_mode, NULL);

    /* Add control handler to v4l2 device */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
    v4l2_ctrl_add_handler( hdl_std_ctrl, hdl_cst_ctrl, NULL, false );
#else
    v4l2_ctrl_add_handler( hdl_std_ctrl, hdl_cst_ctrl, NULL);
#endif

    ctrl->video_dev->ctrl_handler = hdl_std_ctrl;

    v4l2_ctrl_handler_setup( hdl_std_ctrl );

    return 0;
}

void isp_v4l2_ctrl_deinit( isp_v4l2_ctrl_t *ctrl )
{
    v4l2_ctrl_handler_free( &ctrl->ctrl_hdl_std_ctrl );
    v4l2_ctrl_handler_free( &ctrl->ctrl_hdl_cst_ctrl );
}
