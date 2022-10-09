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

#ifndef __ACAMERA_3AALG_PRESET_H__
#define __ACAMERA_3AALG_PRESET_H__

#include "acamera_types.h"

#define IOCTL_3AALG_AEPRE           101
#define IOCTL_3AALG_AWBPRE          102
#define IOCTL_3AALG_GAMMAPRE        103
#define IOCTL_3AALG_IRIDIXPRE       104

//useful preset value for isp
typedef struct _isp_ae_preset_t {
	int32_t skip_cnt;
	int32_t exposure_log2;
	int32_t error_log2;
	int32_t integrator;
	int32_t exposure_ratio;
} isp_ae_preset_t;

typedef struct _isp_awb_preset_t {
	uint32_t skip_cnt;
	int32_t wb_log2[4];
	int32_t wb[4];
	uint16_t global_awb_red_gain;
	uint16_t global_awb_blue_gain;
    uint8_t p_high;
    int32_t temperature_detected;
    uint8_t light_source_candidate;
} isp_awb_preset_t;


typedef struct _isp_gamma_preset_t {
	uint32_t skip_cnt;
	uint32_t gamma_gain;
    uint32_t gamma_offset;
} isp_gamma_preset_t;

typedef struct _isp_iridix_preset_t {
	uint32_t skip_cnt;
	uint16_t strength_target;
	uint32_t iridix_contrast;
    uint16_t dark_enh;
    uint16_t iridix_global_DG;
	int32_t diff;
	uint16_t iridix_strength;
} isp_iridix_preset_t;

typedef struct _isp_other_param_t {
    uint32_t first_frm_timestamp;
} isp_other_param_t;

typedef struct _acamera_alg_preset_t {
	isp_ae_preset_t ae_pre_info;
	isp_awb_preset_t awb_pre_info;
	isp_gamma_preset_t gamma_pre_info;
	isp_iridix_preset_t iridix_pre_info;
	isp_other_param_t other_param;
} acamera_alg_preset_t;

#define ACAMERA_ALG_PRE_BASE (0x6FFFF000)

//#define ACAMERA_PRESET_FREERTOS

int ctrl_channel_3aalg_param_init(isp_ae_preset_t *ae, isp_awb_preset_t *awb, isp_gamma_preset_t *gamma, isp_iridix_preset_t *iridix);

#endif
