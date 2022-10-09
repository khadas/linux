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

#ifndef _ACAMERA_COMMAND_API_IMPL_H_
#define _ACAMERA_COMMAND_API_IMPL_H_
#include "acamera_types.h"
#include "acamera_fw.h"

uint8_t general_context_number(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t general_active_context(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t selftest_fw_revision(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_streaming(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_supported_presets(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_preset(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_wdr_mode(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_fps(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_name(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_test_pattern(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_width(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_height(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_exposures(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_info_preset(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_info_wdr_mode(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_info_fps(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_info_width(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_info_height(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_info_exposures(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_lines_per_second(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_integration_time_min(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_integration_time_limit(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_info_physical_width(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_info_physical_height(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_bayer_pattern( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t lens_info_minfocus_distance(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t lens_info_hyperfocal_distance(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t lens_info_focal_length(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t lens_info_aperture(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sensor_ir_cut_set( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t sensor_mode_dynamic_switch( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t sensor_antiflicker_switch( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t sensor_hw_id( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t sensor_dcam_mode( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t system_logger_level(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_logger_mask(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t buffer_data_type(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t test_pattern_enable(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t test_pattern(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t temper_mode(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_freeze_firmware(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_manual_exposure(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_manual_exposure_ratio(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_manual_integration_time(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_manual_max_integration_time(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_manual_sensor_analog_gain(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_manual_sensor_digital_gain(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_manual_isp_digital_gain(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_manual_awb(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_manual_ccm( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t system_manual_saturation(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_exposure(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_exposure_ratio(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_max_exposure_ratio(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_integration_time(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_long_integration_time(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_short_integration_time(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_max_integration_time(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_sensor_analog_gain(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_max_sensor_analog_gain(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_sensor_digital_gain(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_max_sensor_digital_gain(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_isp_digital_gain(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_max_isp_digital_gain(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_awb_red_gain(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_awb_green_even_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t system_awb_green_odd_gain( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t system_awb_blue_gain(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_ccm_matrix_rr(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_ccm_matrix_rg(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_ccm_matrix_rb(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_ccm_matrix_gr(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_ccm_matrix_gg(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_ccm_matrix_gb(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_ccm_matrix_br(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_ccm_matrix_bg(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_ccm_matrix_bb(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_saturation_target(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_antiflicker_enable(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_anti_flicker_frequency(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t system_dynamic_gamma_enable(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t calibration_update(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t calibration_customer( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t isp_modules_manual_iridix(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t isp_modules_manual_sinter(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t isp_modules_manual_temper(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t isp_modules_manual_auto_level(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t isp_modules_manual_frame_stitch(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t isp_modules_manual_raw_frontend(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t isp_modules_manual_black_level(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t isp_modules_manual_shading(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t isp_modules_manual_demosaic(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t isp_modules_manual_cnr(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t isp_modules_manual_sharpen(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t status_info_exposure_log2(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t status_info_gain_ones(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t status_info_gain_log2(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t status_info_awb_mix_light_contrast(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t status_info_af_lens_pos(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t status_info_af_focus_value(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t dma_reader_output(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t fr_format_base_plane(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t ds1_format_base_plane(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t scaler_startx(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t scaler_starty(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t scaler_crop_width(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t scaler_crop_height(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t scaler_crop_enable(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t scaler_width(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t scaler_height(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t scaler_src_width(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t scaler_src_height(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t scaler_output_mode(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
void scaler_streaming_on(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
void scaler_streaming_off(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t orientation_vflip(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t orientation_hflip(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t image_resize_type(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t image_resize_enable(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t image_resize_width(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t image_resize_height(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t image_crop_xoffset(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t image_crop_yoffset(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t af_lens_status(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t af_mode(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t af_state(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t af_range_low(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t af_range_high(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t af_roi(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t af_manual_control(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t ae_mode(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t ae_state(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t ae_split_preset(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t ae_gain(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t ae_exposure(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t ae_roi(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t ae_compensation(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t ae_zone_weight(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t awb_zone_weight(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t awb_mode(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t awb_state(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t awb_temperature(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t noise_reduction_mode(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t antiflicker_mode(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t color_mode(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t brightness_strength(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t contrast_strength(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t saturation_strength(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t sharpening_strength(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t shading_strength(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t register_address(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t register_size(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t register_source(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t register_value(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t hue_theta( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t snr_manual( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t snr_strength( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t tnr_manual( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t tnr_offset( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t tnr_md_level( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t tnr_md_mtn( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t tnr_md_mode( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t tnr_md_thrd1( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t tnr_md_thrd2( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t tnr_md_log( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t defog_alg_mode( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t defog_alg_ratio_delta( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t defog_alg_black_pctg( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t defog_alg_white_pctg( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t status_info_exposure_correction_log2( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t status_info_total_gain_log2( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t status_ae_stats_info( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t sensor_vmax_fps( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t sensor_md_en( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t sensor_decmpr_en( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t sensor_decmpr_lossless_en( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t sensor_flicker_en( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t status_md_stats_info( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t status_flicker_stats_info( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t sensor_power_on( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t sensor_power_off( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t status_awb_stats_info( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t status_af_stats_info( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t status_info_total_gain_db( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t status_info_cmos_again_db( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t status_info_cmos_dgain_db( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t status_info_isp_dgain_db( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t status_info_iso( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t isp_modules_dpc_thres_slope( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t isp_modules_black_level_r( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t isp_modules_black_level_gr( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t isp_modules_black_level_gb( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t isp_modules_black_level_b( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t isp_modules_demosaic_sharp( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t isp_modules_cnr_strength( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t isp_modules_iridix_enable( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t isp_modules_iridix_strength( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t isp_modules_fr_sharpen_strength( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t isp_modules_ds1_sharpen_strength( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t isp_modules_manual_pf(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t daynight_detect( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );
uint8_t ae_scene_mode(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t scaler_fps(acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value);
uint8_t isp_modules_is_capturing( acamera_fsm_mgr_t *instance, uint32_t value, uint8_t direction, uint32_t *ret_value );

#endif//_ACAMERA_COMMAND_API_IMPL_H_
