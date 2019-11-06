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

#if defined( ISP_HAS_AWB_MESH_NBP_FSM )
#include "awb_mesh_NBP_fsm.h"
#elif defined( ISP_HAS_AWB_MANUAL_FSM )
#include "awb_manual_fsm.h"
#endif

#if defined( ISP_HAS_AF_LMS_FSM )
#include "af_lms_fsm.h"
#elif defined( ISP_HAS_AF_MANUAL_FSM )
#include "af_manual_fsm.h"
#endif

#include "acamera_lens_api.h"

#ifndef _SHARED_BUFFER_H_
#define _SHARED_BUFFER_H_

/* shared buffer max size */
#define SBUF_STATS_ARRAY_SIZE 4

#define SBUF_DEV_FORMAT "ac_sbuf%d"
#define SBUF_DEV_NAME_LEN 16
#define SBUF_DEV_PATH_FORMAT "/dev/" SBUF_DEV_FORMAT
#define SBUF_DEV_PATH_LEN ( SBUF_DEV_NAME_LEN )

enum sbuf_status {
    SBUF_STATUS_DATA_EMPTY,
    SBUF_STATUS_DATA_PREPARE,
    SBUF_STATUS_DATA_DONE,
    SBUF_STATUS_DATA_USING,
    SBUF_STATUS_MAX,
};

enum sbuf_type {
    SBUF_TYPE_AE,
    SBUF_TYPE_AWB,
    SBUF_TYPE_AF,
    SBUF_TYPE_GAMMA,
    SBUF_TYPE_IRIDIX,
    SBUF_TYPE_MAX,
};

struct sbuf_idx_set {
    uint8_t ae_idx;
    uint8_t ae_idx_valid;

    uint8_t awb_idx;
    uint8_t awb_idx_valid;

    uint8_t af_idx;
    uint8_t af_idx_valid;

    uint8_t gamma_idx;
    uint8_t gamma_idx_valid;

    uint8_t iridix_idx;
    uint8_t iridix_idx_valid;
};

struct sbuf_item {
    uint32_t buf_idx;
    uint32_t buf_status;
    uint32_t buf_type;
    void *buf_base;
};

// NOTE: This struct should exactly match the definition of LookupTable in acamera_types.h
//       except the const qualifier.
//       'const' keyword needs to be removed to construct sbuf calibration data.
struct sbuf_lookup_table {
    void *ptr;
    uint16_t rows;
    uint16_t cols;
    uint16_t width;
};

/* kernel-FW information */
struct cmos_info {
    int32_t max_exposure_log2;
    int32_t total_gain_log2;
};

struct af_info {
    int32_t lens_driver_ok;
    lens_param_t lens_param;
};

struct sensor_info {
    uint32_t modes_num;
    sensor_mode_t modes[ISP_MAX_SENSOR_MODES];
};

struct calibration_info {
    uint8_t is_fetched;
    uint8_t cali_data[ISP_MAX_CALIBRATION_DATA_SIZE];
};

struct kf_info {
    struct cmos_info cmos_info;
    struct af_info af_info;
    struct sensor_info sensor_info;

    struct calibration_info cali_info;
};

#if defined( ISP_HAS_AE_BALANCED_FSM ) || defined( ISP_HAS_AE_MANUAL_FSM )
typedef struct sbuf_ae {
    // KF -> UF: Data shared from kernel-FW to user-FW
    uint32_t stats_data[ISP_FULL_HISTOGRAM_SIZE];
    uint32_t histogram_sum;

    // UF -> KF: Data shared from user-FW to kernel-FW
    int32_t ae_exposure;
    uint32_t ae_exposure_ratio;

    uint32_t frame_id;
} sbuf_ae_t;
#endif

#if defined( ISP_HAS_AWB_MESH_FSM ) || defined( ISP_HAS_AWB_MESH_NBP_FSM ) || defined( ISP_HAS_AWB_MANUAL_FSM )
typedef struct sbuf_awb {
    // KF -> UF: Data shared from kernel-FW to user-FW
    awb_zone_t stats_data[MAX_AWB_ZONES];
    uint32_t curr_AWB_ZONES;

    // UF -> KF: Data shared from user-FW to kernel-FW
    uint32_t awb_red_gain;
    uint32_t awb_blue_gain;
    int32_t temperature_detected;
    uint8_t p_high;
    uint8_t light_source_candidate;
    int32_t awb_warming[3];
    uint32_t mix_light_contrast;

    uint32_t frame_id;
} sbuf_awb_t;
#endif

#if defined( ISP_HAS_AF_LMS_FSM ) || defined( ISP_HAS_AF_MANUAL_FSM )
typedef struct sbuf_af {
    // KF -> UF: Data shared from kernel-FW to user-FW
    uint32_t stats_data[AF_ZONES_COUNT_MAX][2];
    uint32_t frame_num;
    uint8_t skip_cur_frame;

    // UF -> KF: Data shared from user-FW to kernel-FW
    uint8_t frame_to_skip;
    uint16_t af_position;
    int32_t af_last_sharp;
} sbuf_af_t;
#endif

#if defined( ISP_HAS_GAMMA_CONTRAST_FSM ) || defined( ISP_HAS_GAMMA_MANUAL_FSM )
typedef struct sbuf_gamma {
    // KF -> UF: Data shared from kernel-FW to user-FW
    uint32_t stats_data[ISP_FULL_HISTOGRAM_SIZE];
    uint32_t fullhist_sum;

    // UF -> KF: Data shared from user-FW to kernel-FW
    uint32_t gamma_gain;
    uint32_t gamma_offset;

    uint32_t frame_id;
} sbuf_gamma_t;
#endif

#if defined( ISP_HAS_IRIDIX_HIST_FSM ) || defined( ISP_HAS_IRIDIX8_FSM ) || defined( ISP_HAS_IRIDIX_MANUAL_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM )
typedef struct sbuf_iridix {
    // KF -> UF: Data shared from kernel-FW to user-FW
    // no output data from KF to UF

    // UF -> KF: Data shared from user-FW to kernel-FW
    uint16_t strength_target;

    // iridix8
    uint16_t iridix_dark_enh;
    uint16_t iridix_global_DG;
    uint32_t iridix_contrast;

    uint32_t frame_id;
} sbuf_iridix_t;
#endif

struct fw_sbuf {
    struct kf_info kf_info;

#if defined( ISP_HAS_AE_BALANCED_FSM ) || defined( ISP_HAS_AE_MANUAL_FSM )
    sbuf_ae_t ae_sbuf[SBUF_STATS_ARRAY_SIZE];
#endif

#if defined( ISP_HAS_AWB_MESH_FSM ) || defined( ISP_HAS_AWB_MESH_NBP_FSM ) || defined( ISP_HAS_AWB_MANUAL_FSM )
    sbuf_awb_t awb_sbuf[SBUF_STATS_ARRAY_SIZE];
#endif

#if defined( ISP_HAS_AF_LMS_FSM ) || defined( ISP_HAS_AF_MANUAL_FSM )
    sbuf_af_t af_sbuf[SBUF_STATS_ARRAY_SIZE];
#endif

#if defined( ISP_HAS_GAMMA_CONTRAST_FSM ) || defined( ISP_HAS_GAMMA_MANUAL_FSM )
    sbuf_gamma_t gamma_sbuf[SBUF_STATS_ARRAY_SIZE];
#endif

#if defined( ISP_HAS_IRIDIX_HIST_FSM ) || defined( ISP_HAS_IRIDIX8_FSM ) || defined( ISP_HAS_IRIDIX_MANUAL_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM )
    sbuf_iridix_t iridix_sbuf[SBUF_STATS_ARRAY_SIZE];
#endif
};

/**
 * sbuf_get_item - get a sbuf item from sbuf_mgr to use
 *
 * @item: target item to be get
 *    OUT uint32_t buf_idx;
 *    IN  uint32_t buf_status: Only SBUF_STATUS_DATA_EMPTY and SBUF_STATUS_DATA_DONE are valid.
 *    IN  uint32_t buf_type: values in enum sbuf_type
 *    OUT void *   buf_base;
 *
 * Return 0 when succeed, buf_idx and buf_base will be filled, other values failed.
 */
int sbuf_get_item( int fw_id, struct sbuf_item *item );

/**
 * sbuf_set_item - set a sbuf item to sbuf_mgr
 *
 * @item: target item to be get
 *    IN  uint32_t buf_idx;
 *    IN  uint32_t buf_status: Only SBUF_STATUS_DATA_EMPTY and SBUF_STATUS_DATA_DONE are valid.
 *    IN  uint32_t buf_type: values in enum sbuf_type
 *    IN void *   buf_base;
 *
 * Return 0 when succeed, other values failed.
 */
int sbuf_set_item( int fw_id, struct sbuf_item *item );

static int inline is_idx_set_has_valid_item( struct sbuf_idx_set *p_idx_set )
{
    if ( p_idx_set->ae_idx_valid ||
         p_idx_set->awb_idx_valid ||
         p_idx_set->af_idx_valid ||
         p_idx_set->gamma_idx_valid ||
         p_idx_set->iridix_idx_valid ) {
        return 1;
    } else {
        return 0;
    }
}

#endif /* #ifndef _SHARED_BUFFER_H_ */
