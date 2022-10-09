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

#include "acamera_command_api.h"


#include "acamera_command_api_impl.h"
#include "acamera.h"
#include "acamera_firmware_api.h"
#include "acamera_fw.h"

#if FW_HAS_CONTROL_CHANNEL
#include "acamera_ctrl_channel.h"
#endif

extern void * acamera_get_ctx_ptr( uint32_t ctx_id );

uint8_t acamera_command( uint32_t ctx_id, uint8_t command_type, uint8_t command, uint32_t value, uint8_t direction, uint32_t *ret_value){
acamera_fsm_mgr_t *instance = &((acamera_context_ptr_t)acamera_get_ctx_ptr(ctx_id))->fsm_mgr;
uint8_t ret = NOT_EXISTS;
switch (command_type) {
case  TGENERAL:
    switch (command) {
        case  CONTEXT_NUMBER:
            ret = general_context_number(instance, value, direction, ret_value);
            break;
        case  ACTIVE_CONTEXT:
            ret = general_active_context(instance, value, direction, ret_value);
            break;
    }//switch (command)
    break;
case  TSELFTEST:
    switch (command) {
        case  FW_REVISION:
            ret = selftest_fw_revision(instance, value, direction, ret_value);
            break;
    }//switch (command)
    break;
case  TSENSOR:
    switch (command) {
        case  SENSOR_STREAMING:
            ret = sensor_streaming(instance, value, direction, ret_value);
            break;
        case  SENSOR_SUPPORTED_PRESETS:
            ret = sensor_supported_presets(instance, value, direction, ret_value);
            break;
        case  SENSOR_PRESET:
            ret = sensor_preset(instance, value, direction, ret_value);
            break;
        case  SENSOR_WDR_MODE:
            ret = sensor_wdr_mode(instance, value, direction, ret_value);
            break;
        case  SENSOR_FPS:
            ret = sensor_fps(instance, value, direction, ret_value);
            break;
        case SENSOR_TESTPATTERN:
            ret = sensor_test_pattern(instance, value, direction, ret_value);
            break;
        case  SENSOR_NAME:
            ret = sensor_name(instance, value, direction, ret_value);
            break;
        case  SENSOR_WIDTH:
            ret = sensor_width(instance, value, direction, ret_value);
            break;
        case  SENSOR_HEIGHT:
            ret = sensor_height(instance, value, direction, ret_value);
            break;
        case  SENSOR_EXPOSURES:
            ret = sensor_exposures(instance, value, direction, ret_value);
            break;
        case  SENSOR_INFO_PRESET:
            ret = sensor_info_preset(instance, value, direction, ret_value);
            break;
        case  SENSOR_INFO_WDR_MODE:
            ret = sensor_info_wdr_mode(instance, value, direction, ret_value);
            break;
        case  SENSOR_INFO_FPS:
            ret = sensor_info_fps(instance, value, direction, ret_value);
            break;
        case  SENSOR_INFO_WIDTH:
            ret = sensor_info_width(instance, value, direction, ret_value);
            break;
        case  SENSOR_INFO_HEIGHT:
            ret = sensor_info_height(instance, value, direction, ret_value);
            break;
        case  SENSOR_INFO_EXPOSURES:
            ret = sensor_info_exposures(instance, value, direction, ret_value);
            break;
        case  SENSOR_IR_CUT:
            ret = sensor_ir_cut_set(instance, value, direction, ret_value);
            break;
        case  SENSOR_HWID:
            ret = sensor_hw_id(instance, value, direction, ret_value);
            break;
        case  SENSOR_DCAM:
            ret = sensor_dcam_mode(instance, value, direction, ret_value);
            break;
        case  SENSOR_WDRMODE_ID:
            ret = sensor_mode_dynamic_switch(instance, value, direction, ret_value);
            value = *ret_value;
            break;
        case  SENSOR_ANTIFLICKER_ID:
            ret = sensor_antiflicker_switch(instance, value, direction, ret_value);
            break;
        case  SENSOR_LINES_PER_SECOND:
            ret = sensor_lines_per_second(instance, value, direction, ret_value);
            break;
        case  SENSOR_INTEGRATION_TIME_MIN:
            ret = sensor_integration_time_min(instance, value, direction, ret_value);
            break;
        case  SENSOR_INTEGRATION_TIME_LIMIT:
            ret = sensor_integration_time_limit(instance, value, direction, ret_value);
            break;
        case  SENSOR_INFO_PHYSICAL_WIDTH:
            ret = sensor_info_physical_width(instance, value, direction, ret_value);
            break;
        case  SENSOR_INFO_PHYSICAL_HEIGHT:
            ret = sensor_info_physical_height(instance, value, direction, ret_value);
            break;
        case  SENSOR_VMAX_FPS:
            ret = sensor_vmax_fps(instance, value, direction, ret_value);
            break;
        case  SENSOR_BAYER_PATTERN:
            ret = sensor_bayer_pattern(instance, value, direction, ret_value);
            break;
        case  SENSOR_POWER_ON:
            ret = sensor_power_on(instance, value, direction, ret_value);
            break;
        case  SENSOR_POWER_OFF:
            ret = sensor_power_off(instance, value, direction, ret_value);
            break;
        }//switch (command)
    break;
case  TLENS:
    switch (command) {
        case  LENS_INFO_MINFOCUS_DISTANCE:
            ret = lens_info_minfocus_distance(instance, value, direction, ret_value);
            break;
        case  LENS_INFO_HYPERFOCAL_DISTANCE:
            ret = lens_info_hyperfocal_distance(instance, value, direction, ret_value);
            break;
        case  LENS_INFO_FOCAL_LENGTH:
            ret = lens_info_focal_length(instance, value, direction, ret_value);
            break;
        case  LENS_INFO_APERTURE:
            ret = lens_info_aperture(instance, value, direction, ret_value);
            break;
    }//switch (command)
    break;
case  TSYSTEM:
    switch (command) {
        case  SYSTEM_LOGGER_LEVEL:
            ret = system_logger_level(instance, value, direction, ret_value);
            break;
        case  SYSTEM_LOGGER_MASK:
            ret = system_logger_mask(instance, value, direction, ret_value);
            break;
        case  BUFFER_DATA_TYPE:
            ret = buffer_data_type(instance, value, direction, ret_value);
            break;
        case  TEST_PATTERN_ENABLE_ID:
            ret = test_pattern_enable(instance, value, direction, ret_value);
            break;
        case  TEST_PATTERN_MODE_ID:
            ret = test_pattern(instance, value, direction, ret_value);
            break;
        case  TEMPER_MODE_ID:
            ret = temper_mode(instance, value, direction, ret_value);
            break;
        case  SYSTEM_FREEZE_FIRMWARE:
            ret = system_freeze_firmware(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MANUAL_EXPOSURE:
            ret = system_manual_exposure(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MANUAL_EXPOSURE_RATIO:
            ret = system_manual_exposure_ratio(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MANUAL_INTEGRATION_TIME:
            ret = system_manual_integration_time(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MANUAL_MAX_INTEGRATION_TIME:
            ret = system_manual_max_integration_time(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MANUAL_SENSOR_ANALOG_GAIN:
            ret = system_manual_sensor_analog_gain(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MANUAL_SENSOR_DIGITAL_GAIN:
            ret = system_manual_sensor_digital_gain(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MANUAL_ISP_DIGITAL_GAIN:
            ret = system_manual_isp_digital_gain(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MANUAL_AWB:
            ret = system_manual_awb(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MANUAL_CCM:
            ret = system_manual_ccm(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MANUAL_SATURATION:
            ret = system_manual_saturation(instance, value, direction, ret_value);
            break;
        case  SYSTEM_EXPOSURE:
            ret = system_exposure(instance, value, direction, ret_value);
            break;
        case  SYSTEM_EXPOSURE_RATIO:
            ret = system_exposure_ratio(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MAX_EXPOSURE_RATIO:
            ret = system_max_exposure_ratio(instance, value, direction, ret_value);
            break;
        case  SYSTEM_INTEGRATION_TIME:
            ret = system_integration_time(instance, value, direction, ret_value);
            break;
        case  SYSTEM_LONG_INTEGRATION_TIME:
            ret = system_long_integration_time(instance, value, direction, ret_value);
            break;
        case  SYSTEM_SHORT_INTEGRATION_TIME:
            ret = system_short_integration_time(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MAX_INTEGRATION_TIME:
            ret = system_max_integration_time(instance, value, direction, ret_value);
            break;
        case  SYSTEM_SENSOR_ANALOG_GAIN:
            ret = system_sensor_analog_gain(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MAX_SENSOR_ANALOG_GAIN:
            ret = system_max_sensor_analog_gain(instance, value, direction, ret_value);
            break;
        case  SYSTEM_SENSOR_DIGITAL_GAIN:
            ret = system_sensor_digital_gain(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MAX_SENSOR_DIGITAL_GAIN:
            ret = system_max_sensor_digital_gain(instance, value, direction, ret_value);
            break;
        case  SYSTEM_ISP_DIGITAL_GAIN:
            ret = system_isp_digital_gain(instance, value, direction, ret_value);
            break;
        case  SYSTEM_MAX_ISP_DIGITAL_GAIN:
            ret = system_max_isp_digital_gain(instance, value, direction, ret_value);
            break;
        case  SYSTEM_AWB_RED_GAIN:
            ret = system_awb_red_gain(instance, value, direction, ret_value);
            break;
        case  SYSTEM_AWB_GREEN_EVEN_GAIN:
            ret = system_awb_green_even_gain(instance, value, direction, ret_value);
            break;
        case  SYSTEM_AWB_GREEN_ODD_GAIN:
            ret = system_awb_green_odd_gain(instance, value, direction, ret_value);
            break;
        case  SYSTEM_AWB_BLUE_GAIN:
            ret = system_awb_blue_gain(instance, value, direction, ret_value);
            break;
        case  SYSTEM_CCM_MATRIX_RR:
            ret = system_ccm_matrix_rr(instance, value, direction, ret_value);
            break;
        case  SYSTEM_CCM_MATRIX_RG:
            ret = system_ccm_matrix_rg(instance, value, direction, ret_value);
            break;
        case  SYSTEM_CCM_MATRIX_RB:
            ret = system_ccm_matrix_rb(instance, value, direction, ret_value);
            break;
        case  SYSTEM_CCM_MATRIX_GR:
            ret = system_ccm_matrix_gr(instance, value, direction, ret_value);
            break;
        case  SYSTEM_CCM_MATRIX_GG:
            ret = system_ccm_matrix_gg(instance, value, direction, ret_value);
            break;
        case  SYSTEM_CCM_MATRIX_GB:
            ret = system_ccm_matrix_gb(instance, value, direction, ret_value);
            break;
        case  SYSTEM_CCM_MATRIX_BR:
            ret = system_ccm_matrix_br(instance, value, direction, ret_value);
            break;
        case  SYSTEM_CCM_MATRIX_BG:
            ret = system_ccm_matrix_bg(instance, value, direction, ret_value);
            break;
        case  SYSTEM_CCM_MATRIX_BB:
            ret = system_ccm_matrix_bb(instance, value, direction, ret_value);
            break;
        case  SYSTEM_SATURATION_TARGET:
            ret = system_saturation_target(instance, value, direction, ret_value);
            break;
        case  SYSTEM_ANTIFLICKER_ENABLE:
            ret = system_antiflicker_enable(instance, value, direction, ret_value);
            break;
        case  SYSTEM_ANTI_FLICKER_FREQUENCY:
            ret = system_anti_flicker_frequency(instance, value, direction, ret_value);
            break;
        case  SYSTEM_DYNAMIC_GAMMA_ENABLE:
            ret = system_dynamic_gamma_enable(instance, value, direction, ret_value);
            break;
        case  CALIBRATION_UPDATE:
            ret = calibration_update(instance, value, direction, ret_value);
            break;
        case  CALIBRATION_CUSTOMER:
            ret = calibration_customer(instance, value, direction, ret_value);
            break;
    }//switch (command)
    break;
case  TISP_MODULES:
    switch (command) {
        case  ISP_MODULES_MANUAL_IRIDIX:
            ret = isp_modules_manual_iridix(instance, value, direction, ret_value);
            break;
        case  ISP_MODULES_MANUAL_SINTER:
            ret = isp_modules_manual_sinter(instance, value, direction, ret_value);
            break;
        case  ISP_MODULES_MANUAL_TEMPER:
            ret = isp_modules_manual_temper(instance, value, direction, ret_value);
            break;
        case  ISP_MODULES_MANUAL_AUTO_LEVEL:
            ret = isp_modules_manual_auto_level(instance, value, direction, ret_value);
            break;
        case  ISP_MODULES_MANUAL_FRAME_STITCH:
            ret = isp_modules_manual_frame_stitch(instance, value, direction, ret_value);
            break;
        case  ISP_MODULES_MANUAL_RAW_FRONTEND:
            ret = isp_modules_manual_raw_frontend(instance, value, direction, ret_value);
            break;
        case  ISP_MODULES_MANUAL_BLACK_LEVEL:
            ret = isp_modules_manual_black_level(instance, value, direction, ret_value);
            break;
        case  ISP_MODULES_MANUAL_SHADING:
            ret = isp_modules_manual_shading(instance, value, direction, ret_value);
            break;
        case  ISP_MODULES_MANUAL_DEMOSAIC:
            ret = isp_modules_manual_demosaic(instance, value, direction, ret_value);
            break;
        case  ISP_MODULES_MANUAL_CNR:
            ret = isp_modules_manual_cnr(instance, value, direction, ret_value);
            break;
        case  ISP_MODULES_MANUAL_SHARPEN:
            ret = isp_modules_manual_sharpen(instance, value, direction, ret_value);
            break;
        case ISP_DPC_THRES_SLOPE:
            ret = isp_modules_dpc_thres_slope(instance, value, direction, ret_value);
            break;
        case ISP_BLACK_LEVEL_R:
            ret = isp_modules_black_level_r(instance, value, direction, ret_value);
            break;
        case ISP_BLACK_LEVEL_GR:
            ret = isp_modules_black_level_gr(instance, value, direction, ret_value);
            break;
        case ISP_BLACK_LEVEL_GB:
            ret = isp_modules_black_level_gb(instance, value, direction, ret_value);
            break;
        case ISP_BLACK_LEVEL_B:
            ret = isp_modules_black_level_b(instance, value, direction, ret_value);
            break;
        case ISP_DEMOSAIC_SHARP:
            ret = isp_modules_demosaic_sharp(instance, value, direction, ret_value);
            break;
        case ISP_CNR_STRENGTH:
            ret = isp_modules_cnr_strength(instance, value, direction, ret_value);
            break;
        case ISP_MODULES_IRIDIX_ENABLE:
            ret = isp_modules_iridix_enable(instance, value, direction, ret_value);
            break;
        case ISP_MODULES_IRIDIX_STRENGTH:
            ret = isp_modules_iridix_strength(instance, value, direction, ret_value);
            break;
        case ISP_MODULES_FR_SHARPEN_STRENGTH:
            ret = isp_modules_fr_sharpen_strength(instance, value, direction, ret_value);
            break;
        case ISP_MODULES_DS1_SHARPEN_STRENGTH:
            ret = isp_modules_ds1_sharpen_strength(instance, value, direction, ret_value);
            break;
        case  ISP_MODULES_MANUAL_PF:
            ret = isp_modules_manual_pf(instance, value, direction, ret_value);
            break;
        case ISP_IS_CAPTURING:
            ret = isp_modules_is_capturing(instance, value, direction, ret_value);
            break;
    }//switch (command)
    break;
case  TSTATUS:
    switch (command) {
        case  STATUS_INFO_EXPOSURE_LOG2_ID:
            ret = status_info_exposure_log2(instance, value, direction, ret_value);
            break;
        case  STATUS_INFO_GAIN_ONES_ID:
            ret = status_info_gain_ones(instance, value, direction, ret_value);
            break;
        case  STATUS_INFO_GAIN_LOG2_ID:
            ret = status_info_gain_log2(instance, value, direction, ret_value);
            break;
        case  STATUS_INFO_AWB_MIX_LIGHT_CONTRAST:
            ret = status_info_awb_mix_light_contrast(instance, value, direction, ret_value);
            break;
        case  STATUS_INFO_AF_LENS_POS:
            ret = status_info_af_lens_pos(instance, value, direction, ret_value);
            break;
        case  STATUS_INFO_AF_FOCUS_VALUE:
            ret = status_info_af_focus_value(instance, value, direction, ret_value);
            break;
        case  STATUS_INFO_EXPOSURE_CORRECTION_LOG2_ID:
            ret = status_info_exposure_correction_log2( instance, value, direction, ret_value );
            break;
        case  STATUS_INFO_TOTAL_GAIN_LOG2_ID:
            ret = status_info_total_gain_log2(instance, value, direction, ret_value);
            break;
        case  AE_STATS_ID:
            ret = status_ae_stats_info(instance, value, direction, ret_value);
            break;
        case  AWB_STATS_ID:
            ret = status_awb_stats_info(instance, value, direction, ret_value);
            break;
        case  AF_STATS_ID:
            ret = status_af_stats_info(instance, value, direction, ret_value);
            break;
        case  STATUS_INFO_TOTAL_GAIN_DB_ID:
            ret = status_info_total_gain_db(instance, value, direction, ret_value);
            break;
        case  STATUS_INFO_CMOS_AGAIN_DB_ID:
            ret = status_info_cmos_again_db(instance, value, direction, ret_value);
            break;
        case  STATUS_INFO_CMOS_DGAIN_DB_ID:
            ret = status_info_cmos_dgain_db(instance, value, direction, ret_value);
            break;
        case  STATUS_INFO_ISP_DGAIN_DB_ID:
            ret = status_info_isp_dgain_db(instance, value, direction, ret_value);
            break;
        case  STATUS_INFO_ISO_ID:
            ret = status_info_iso(instance, value, direction, ret_value);
            break;
    }//switch (command)
    break;
case  TIMAGE:
    switch (command) {
        case  DMA_READER_OUTPUT_ID:
            ret = dma_reader_output(instance, value, direction, ret_value);
            break;
        case  FR_FORMAT_BASE_PLANE_ID:
            ret = fr_format_base_plane(instance, value, direction, ret_value);
            break;
        case  DS1_FORMAT_BASE_PLANE_ID:
            ret = ds1_format_base_plane(instance, value, direction, ret_value);
            break;
        case  ORIENTATION_VFLIP_ID:
            ret = orientation_vflip(instance, value, direction, ret_value);
            break;
        case  ORIENTATION_HFLIP_ID:
            ret = orientation_hflip(instance, value, direction, ret_value);
            break;
        case  IMAGE_RESIZE_TYPE_ID:
            ret = image_resize_type(instance, value, direction, ret_value);
            break;
        case  IMAGE_RESIZE_ENABLE_ID:
            ret = image_resize_enable(instance, value, direction, ret_value);
            break;
        case  IMAGE_RESIZE_WIDTH_ID:
            ret = image_resize_width(instance, value, direction, ret_value);
            break;
        case  IMAGE_RESIZE_HEIGHT_ID:
            ret = image_resize_height(instance, value, direction, ret_value);
            break;
        case  IMAGE_CROP_XOFFSET_ID:
            ret = image_crop_xoffset(instance, value, direction, ret_value);
            break;
        case  IMAGE_CROP_YOFFSET_ID:
            ret = image_crop_yoffset(instance, value, direction, ret_value);
            break;
    }//switch (command)
    break;
case  TALGORITHMS:
    switch (command) {
        case  AF_LENS_STATUS:
            ret = af_lens_status(instance, value, direction, ret_value);
            break;
        case  AF_MODE_ID:
            ret = af_mode(instance, value, direction, ret_value);
            break;
        case  AF_STATE_ID:
            ret = af_state(instance, value, direction, ret_value);
            break;
        case  AF_RANGE_LOW_ID:
            ret = af_range_low(instance, value, direction, ret_value);
            break;
        case  AF_RANGE_HIGH_ID:
            ret = af_range_high(instance, value, direction, ret_value);
            break;
        case  AF_ROI_ID:
            ret = af_roi(instance, value, direction, ret_value);
            break;
        case  AF_MANUAL_CONTROL_ID:
            ret = af_manual_control(instance, value, direction, ret_value);
            break;
        case  AE_MODE_ID:
            ret = ae_mode(instance, value, direction, ret_value);
            break;
        case  AE_STATE_ID:
            ret = ae_state(instance, value, direction, ret_value);
            break;
        case  AE_SPLIT_PRESET_ID:
            ret = ae_split_preset(instance, value, direction, ret_value);
            break;
        case  AE_GAIN_ID:
            ret = ae_gain(instance, value, direction, ret_value);
            break;
        case  AE_EXPOSURE_ID:
            ret = ae_exposure(instance, value, direction, ret_value);
            break;
        case  AE_ROI_ID:
            ret = ae_roi(instance, value, direction, ret_value);
            break;
        case  AE_COMPENSATION_ID:
            ret = ae_compensation(instance, value, direction, ret_value);
            break;
        case  AWB_MODE_ID:
            ret = awb_mode(instance, value, direction, ret_value);
            break;
        case  AWB_STATE_ID:
            ret = awb_state(instance, value, direction, ret_value);
            break;
        case  AWB_TEMPERATURE_ID:
            ret = awb_temperature(instance, value, direction, ret_value);
            break;
        case  NOISE_REDUCTION_MODE_ID:
            ret = noise_reduction_mode(instance, value, direction, ret_value);
            break;
        case  ANTIFLICKER_MODE_ID:
            ret = antiflicker_mode(instance, value, direction, ret_value);
            break;
        case AE_ZONE_WEIGHT:
            ret = ae_zone_weight(instance, value, direction, ret_value);
            break;
        case AWB_ZONE_WEIGHT:
            ret = awb_zone_weight(instance, value, direction, ret_value);
            break;
        case DEFOG_MODE_ID:
            ret = defog_alg_mode(instance, value, direction, ret_value);
            break;
        case DEFOG_RATIO_DELTA:
            ret = defog_alg_ratio_delta(instance, value, direction, ret_value);
            break;
        case DEFOG_BLACK_PERCENTAGE:
            ret = defog_alg_black_pctg(instance, value, direction, ret_value);
            break;
        case DEFOG_WHITE_PERCENTAGE:
            ret = defog_alg_white_pctg(instance, value, direction, ret_value);
            break;
        case DAYNIGHT_DETECT_ID:
            ret = daynight_detect(instance, value, direction, ret_value);
            break;
        case SENSOR_MD_EN:
            ret = sensor_md_en(instance, value, direction, ret_value);
            break;
        case SENSOR_DECMPR_EN:
            ret = sensor_decmpr_en(instance, value, direction, ret_value);
            break;
        case SENSOR_DECMPR_LOSSLESS_EN:
            ret = sensor_decmpr_lossless_en(instance, value, direction, ret_value);
            break;
        case SENSOR_FLICKER_EN:
            ret = sensor_flicker_en(instance, value, direction, ret_value);
            break;
        case MD_STATS_ID:
            ret = status_md_stats_info(instance, value, direction, ret_value);
            break;
        case FLICKER_STATS_ID:
            ret = status_flicker_stats_info(instance, value, direction, ret_value);
            break;
    }//switch (command)
    break;
case  TSCENE_MODES:
    switch (command) {
        case  COLOR_MODE_ID:
            ret = color_mode(instance, value, direction, ret_value);
            break;
        case  BRIGHTNESS_STRENGTH_ID:
            ret = brightness_strength(instance, value, direction, ret_value);
            break;
        case  CONTRAST_STRENGTH_ID:
            ret = contrast_strength(instance, value, direction, ret_value);
            break;
        case  SATURATION_STRENGTH_ID:
            ret = saturation_strength(instance, value, direction, ret_value);
            break;
        case  SHARPENING_STRENGTH_ID:
            ret = sharpening_strength(instance, value, direction, ret_value);
            break;
        case  SHADING_STRENGTH_ID:
            ret = shading_strength(instance, value, direction, ret_value);
            break;
        case  HUE_THETA_ID:
            ret = hue_theta(instance, value, direction, ret_value);
            break;
        case  SNR_MANUAL_ID:
            ret = snr_manual(instance, value, direction, ret_value);
            break;
        case  SNR_STRENGTH_ID:
            ret = snr_strength(instance, value, direction, ret_value);
            break;
        case  TNR_MANUAL_ID:
            ret = tnr_manual(instance, value, direction, ret_value);
            break;
        case  TNR_OFFSET_ID:
            ret = tnr_offset(instance, value, direction, ret_value);
            break;
        case  TNR_MD_LELVE_ID:
            ret = tnr_md_level(instance, value, direction, ret_value);
            break;
        case  TNR_MD_MODE_ID:
            ret = tnr_md_mode(instance, value, direction, ret_value);
            break;
        case  TNR_MD_THRD1_ID:
            ret = tnr_md_thrd1(instance, value, direction, ret_value);
            break;
        case  TNR_MD_THRD2_ID:
            ret = tnr_md_thrd2(instance, value, direction, ret_value);
            break;
        case  TNR_MD_LOG_ID:
            ret = tnr_md_log(instance, value, direction, ret_value);
            break;
        case  TNR_MD_MTN_ID:
            ret = tnr_md_mtn(instance, value, direction, ret_value);
            break;
        case  AE_SCENE_MODE_ID:
            ret = ae_scene_mode(instance, value, direction, ret_value);
            break;
    }//switch (command)
    break;
case  TREGISTERS:
    switch (command) {
        case  REGISTERS_ADDRESS_ID:
            ret = register_address(instance, value, direction, ret_value);
            break;
        case  REGISTERS_SIZE_ID:
            ret = register_size(instance, value, direction, ret_value);
            break;
        case  REGISTERS_SOURCE_ID:
            ret = register_source(instance, value, direction, ret_value);
            break;
        case  REGISTERS_VALUE_ID:
            ret = register_value(instance, value, direction, ret_value);
            break;
    }//switch (command)
    break;

case TAML_SCALER:
    switch (command) {
        case SCALER_STARTX:
            ret = scaler_startx(instance, value, direction, ret_value);
            break;
        case SCALER_STARTY:
            ret = scaler_starty(instance, value, direction, ret_value);
            break;
        case SCALER_CROP_WIDTH:
            ret = scaler_crop_width(instance, value, direction, ret_value);
            break;
        case SCALER_CROP_HEIGHT:
            ret = scaler_crop_height(instance, value, direction, ret_value);
            break;
        case SCALER_CROP_ENABLE:
            ret = scaler_crop_enable(instance, value, direction, ret_value);
            break;
        case SCALER_WIDTH:
            ret = scaler_width(instance, value, direction, ret_value);
            break;
        case SCALER_HEIGHT:
            ret = scaler_height(instance, value, direction, ret_value);
            break;
        case SCALER_SRC_WIDTH:
            ret = scaler_src_width(instance, value, direction, ret_value);
            break;
        case SCALER_SRC_HEIGHT:
            ret = scaler_src_height(instance, value, direction, ret_value);
            break;
        case SCALER_OUTPUT_MODE:
            ret = scaler_output_mode(instance, value, direction, ret_value);
            break;
        case SCALER_STREAMING_ON:
            scaler_streaming_on(instance, value, direction, ret_value);
            break;
        case SCALER_STREAMING_OFF:
            scaler_streaming_off(instance, value, direction, ret_value);
            break;
        case SCALER_FPS:
            ret = scaler_fps(instance, value, direction, ret_value);
            break;
    }//switch (command)
    break;
}//switch (command_type)

#if FW_HAS_CONTROL_CHANNEL
    if (ret == SUCCESS)
        ctrl_channel_handle_command( ctx_id, command_type, command, value, direction );
#endif

    if (ret == IMPLEMENTED)
        ret = SUCCESS;

    if (ret != SUCCESS) {
        LOG(LOG_WARNING,"API COMMAND FAILED: type %d, cmd %d, value %lu, direction %d, ret_value %lu, result %d",command_type, command, (unsigned long)value, direction, (unsigned long)*ret_value, ret);
    } else {
        LOG(LOG_NOTICE,"API type %d, cmd %d, value %lu, direction %d, ret_value %lu, result %d",command_type, command, (unsigned long)value, direction, (unsigned long)*ret_value, ret);
    }

    return ret;
}
