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
#include "acamera_sensor_api.h"
#include "acamera_firmware_settings.h"
#include "acamera_logger.h"
#include <linux/module.h>

extern uint32_t get_calibrations_static_linear_imx227( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx227( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_dummy( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_dummy( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx290( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx290( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_imx290( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_imx290( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx290_slt( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx290_slt( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx290_lens_8mm( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx290_lens_8mm( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_imx290_lens_8mm( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_imx290_lens_8mm( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx290_lens_4mm( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx290_lens_4mm( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_imx290_lens_4mm( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_imx290_lens_4mm( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_os08a10_ipc( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_os08a10_ipc( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_os08a10_ipc( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_os08a10_ipc( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_os08a10_slt( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_os08a10_slt( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_os08a10_slt( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_os08a10_slt( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_linear_os08a10_tv( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_os08a10_tv( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_os08a10_tv( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_os08a10_tv( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_os08a10_panel( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_os08a10_panel( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx481( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx481( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx307_demo( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx307_demo( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_imx307_demo( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_imx307_demo( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx307( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx307( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_imx307( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_imx307( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx224( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_imx224( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx224( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_imx224( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx335( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx335( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_imx335( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_imx335( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx415( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx415( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_imx415( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_imx415( ACameraCalibrations *c );


extern uint32_t get_calibrations_static_linear_ov13858( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_ov13858( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_ov13858( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_ov13858( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_sc2232h( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_sc2232h( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_sc4238( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_sc4238( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_sc4238( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_sc4238( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_sc4238_night( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_sc4238_night( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_sc2335( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_sc2335( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_imx334( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_imx334( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_imx334( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_imx334( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_sc8238cs_tv( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_sc8238cs_tv( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_sc8238cs_tv( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_sc8238cs_tv( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_ov2718( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_ov2718( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_ov2718( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_ov2718( ACameraCalibrations *c );

extern uint32_t get_calibrations_static_linear_os04a10_tv( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_os04a10_tv( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_fs_lin_os04a10_tv( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_fs_lin_os04a10_tv( ACameraCalibrations *c );
extern uint32_t get_calibrations_static_linear_ov5675( ACameraCalibrations *c );
extern uint32_t get_calibrations_dynamic_linear_ov5675( ACameraCalibrations *c );
