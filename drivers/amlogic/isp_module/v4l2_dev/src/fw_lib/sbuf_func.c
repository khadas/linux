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

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>
#include <linux/version.h>
#include "sbuf.h"
#include "acamera.h"
#include "sbuf_fsm.h"
#include "acamera_firmware_settings.h"


#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_SBUF
#endif

static const char *sbuf_status_str[] = {
    "DATA_EMPTY",
    "DATA_PREPARE",
    "DATA_DONE",
    "DATA_USING",
    "ERROR"};

static const char *sbuf_type_str[] = {
    "AE",
    "AWB",
    "AF",
    "GAMMA",
    "IRIDIX",
    "ERROR"};

/**
 * sbuf_item_arr_info - struct to describle sbuf array current status
 *
 * @item_total_count: number of sbuf for each buffer type.
 * @item_status_count: number of sbuf of each buffer status, sum of this
 *                     array should equals to item_total_count after each operation.
 * @write_idx: array index of sbuf for next write, the status should be DATA_EMPTY.
 * @read_idx: array index of sbuf for next read, the status should be DATA_DONE.
 *
 */
struct sbuf_item_arr_info {
    uint32_t item_total_count;
    uint32_t item_status_count[SBUF_STATUS_MAX];
    uint32_t write_idx;
    uint32_t read_idx;
};

struct sbuf_mgr {
    spinlock_t sbuf_lock;
    int sbuf_inited;

    uint32_t len_allocated;
    uint32_t len_used;
    void *buf_allocated;
    void *buf_used;

    uint32_t cur_wdr_mode;

    struct fw_sbuf *sbuf_base;

#if defined( ISP_HAS_AE_MANUAL_FSM )
    /* AE: array to describe sbuf item status */
    struct sbuf_item ae_sbuf_arr[SBUF_STATS_ARRAY_SIZE];
    /* AE: structure to describe array */
    struct sbuf_item_arr_info ae_arr_info;
#endif

#if defined( ISP_HAS_AWB_MANUAL_FSM )
    /* AWB: array to describe sbuf item status */
    struct sbuf_item awb_sbuf_arr[SBUF_STATS_ARRAY_SIZE];
    /* AWB: structure to describe array */
    struct sbuf_item_arr_info awb_arr_info;
#endif

#if defined( ISP_HAS_AF_MANUAL_FSM )
    /* AF: array to describe sbuf item status */
    struct sbuf_item af_sbuf_arr[SBUF_STATS_ARRAY_SIZE];
    /* AF: structure to describe array */
    struct sbuf_item_arr_info af_arr_info;
#endif

#if defined( ISP_HAS_GAMMA_MANUAL_FSM )
    /* Gamma: array to describe sbuf item status */
    struct sbuf_item gamma_sbuf_arr[SBUF_STATS_ARRAY_SIZE];
    struct sbuf_item_arr_info gamma_arr_info;
#endif

#if defined( ISP_HAS_IRIDIX_MANUAL_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM )
    /* Gamma: array to describe sbuf item status */
    struct sbuf_item iridix_sbuf_arr[SBUF_STATS_ARRAY_SIZE];
    struct sbuf_item_arr_info iridix_arr_info;
#endif
};

struct sbuf_context {
    struct sbuf_mgr sbuf_mgr;

    struct mutex fops_lock;
    struct miscdevice sbuf_dev;
    int dev_minor_id;
    char dev_name[SBUF_DEV_NAME_LEN];
    int dev_opened;

    int fw_id;
    sbuf_fsm_t *p_fsm;

    struct mutex idx_set_lock;
    struct sbuf_idx_set idx_set;
    wait_queue_head_t idx_set_wait_queue;
};

static struct sbuf_context sbuf_contexts[FIRMWARE_CONTEXT_NUMBER];

static int is_sbuf_inited( struct sbuf_mgr *p_sbuf_mgr )
{
    int tmp_inited;
    unsigned long irq_flags;

    spin_lock_irqsave( &p_sbuf_mgr->sbuf_lock, irq_flags );
    tmp_inited = p_sbuf_mgr->sbuf_inited;
    spin_unlock_irqrestore( &p_sbuf_mgr->sbuf_lock, irq_flags );

    return tmp_inited;
}

static int sbuf_mgr_alloc_sbuf( struct sbuf_mgr *p_sbuf_mgr )
{
    int i;

    if ( is_sbuf_inited( p_sbuf_mgr ) ) {
        LOG( LOG_ERR, "Error: sbuf alloc should not be called twice." );
        return -1;
    }

    /* round up to whole number of pages  */
    p_sbuf_mgr->len_used = ( sizeof( struct fw_sbuf ) + 1 + PAGE_SIZE ) & PAGE_MASK;

    /* allocate one more page for user-sapce mapping */
    p_sbuf_mgr->len_allocated = p_sbuf_mgr->len_used - 1 + PAGE_SIZE;

    p_sbuf_mgr->buf_allocated = kzalloc( p_sbuf_mgr->len_allocated, GFP_KERNEL );
    if ( !p_sbuf_mgr->buf_allocated ) {
        LOG( LOG_CRIT, "alloc memory failed." );
        return -ENOMEM;
    }

    /* make the used buffer page aligned  */
    p_sbuf_mgr->buf_used = (void *)( ( (unsigned long)p_sbuf_mgr->buf_allocated + PAGE_SIZE - 1 ) & PAGE_MASK );

    LOG( LOG_CRIT, "sbuf: len_needed: %zu, len_alloc: %u, len_used: %u, page_size: %lu, buf_alloc: %p, buf_used: %p.",
         sizeof( struct fw_sbuf ), p_sbuf_mgr->len_allocated, p_sbuf_mgr->len_used, PAGE_SIZE, p_sbuf_mgr->buf_allocated, p_sbuf_mgr->buf_used );

    /* set the page as reserved so that it won't be swapped out */
    for ( i = 0; i < p_sbuf_mgr->len_used; i += PAGE_SIZE ) {
        SetPageReserved( virt_to_page( p_sbuf_mgr->buf_used + i ) );
    }

    return 0;
}

static void sbuf_mgr_init_sbuf( struct sbuf_mgr *p_sbuf_mgr )
{
    int i;
    unsigned long irq_flags;

    spin_lock_irqsave( &p_sbuf_mgr->sbuf_lock, irq_flags );
    p_sbuf_mgr->sbuf_inited = 1;
    p_sbuf_mgr->sbuf_base = (struct fw_sbuf *)p_sbuf_mgr->buf_used;
    p_sbuf_mgr->cur_wdr_mode = 0xFFFF; // Invalid

#if defined( ISP_HAS_AE_MANUAL_FSM )
    /***  For AE  ***/
    for ( i = 0; i < SBUF_STATS_ARRAY_SIZE; i++ ) {
        p_sbuf_mgr->ae_sbuf_arr[i].buf_idx = i;
        p_sbuf_mgr->ae_sbuf_arr[i].buf_status = SBUF_STATUS_DATA_EMPTY;
        p_sbuf_mgr->ae_sbuf_arr[i].buf_type = SBUF_TYPE_AE;
        p_sbuf_mgr->ae_sbuf_arr[i].buf_base = (void *)&( p_sbuf_mgr->sbuf_base->ae_sbuf[i] );
    }

    memset( &p_sbuf_mgr->ae_arr_info, 0, sizeof( p_sbuf_mgr->ae_arr_info ) );
    p_sbuf_mgr->ae_arr_info.item_total_count = SBUF_STATS_ARRAY_SIZE;
    p_sbuf_mgr->ae_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY] = SBUF_STATS_ARRAY_SIZE;

    /* init read_idx is ARRAY_SIZE, which is invalid. */
    p_sbuf_mgr->ae_arr_info.write_idx = 0;
    p_sbuf_mgr->ae_arr_info.read_idx = SBUF_STATS_ARRAY_SIZE;
#endif

#if defined( ISP_HAS_AWB_MANUAL_FSM )
    /***  For AWB  ***/
    for ( i = 0; i < SBUF_STATS_ARRAY_SIZE; i++ ) {
        p_sbuf_mgr->awb_sbuf_arr[i].buf_idx = i;
        p_sbuf_mgr->awb_sbuf_arr[i].buf_status = SBUF_STATUS_DATA_EMPTY;
        p_sbuf_mgr->awb_sbuf_arr[i].buf_type = SBUF_TYPE_AWB;
        p_sbuf_mgr->awb_sbuf_arr[i].buf_base = (void *)&( p_sbuf_mgr->sbuf_base->awb_sbuf[i] );
    }

    memset( &p_sbuf_mgr->awb_arr_info, 0, sizeof( p_sbuf_mgr->awb_arr_info ) );
    p_sbuf_mgr->awb_arr_info.item_total_count = SBUF_STATS_ARRAY_SIZE;
    p_sbuf_mgr->awb_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY] = SBUF_STATS_ARRAY_SIZE;

    p_sbuf_mgr->awb_arr_info.write_idx = 0;
    p_sbuf_mgr->awb_arr_info.read_idx = SBUF_STATS_ARRAY_SIZE;
#endif

#if defined( ISP_HAS_AF_MANUAL_FSM )
    /***  For AF  ***/
    for ( i = 0; i < SBUF_STATS_ARRAY_SIZE; i++ ) {
        p_sbuf_mgr->af_sbuf_arr[i].buf_idx = i;
        p_sbuf_mgr->af_sbuf_arr[i].buf_status = SBUF_STATUS_DATA_EMPTY;
        p_sbuf_mgr->af_sbuf_arr[i].buf_type = SBUF_TYPE_AF;
        p_sbuf_mgr->af_sbuf_arr[i].buf_base = (void *)&( p_sbuf_mgr->sbuf_base->af_sbuf[i] );
    }

    memset( &p_sbuf_mgr->af_arr_info, 0, sizeof( p_sbuf_mgr->af_arr_info ) );
    p_sbuf_mgr->af_arr_info.item_total_count = SBUF_STATS_ARRAY_SIZE;
    p_sbuf_mgr->af_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY] = SBUF_STATS_ARRAY_SIZE;

    p_sbuf_mgr->af_arr_info.write_idx = 0;
    p_sbuf_mgr->af_arr_info.read_idx = SBUF_STATS_ARRAY_SIZE;
#endif

#if defined( ISP_HAS_GAMMA_MANUAL_FSM )
    /***  For Gamma Stats ***/
    for ( i = 0; i < SBUF_STATS_ARRAY_SIZE; i++ ) {
        p_sbuf_mgr->gamma_sbuf_arr[i].buf_idx = i;
        p_sbuf_mgr->gamma_sbuf_arr[i].buf_status = SBUF_STATUS_DATA_EMPTY;
        p_sbuf_mgr->gamma_sbuf_arr[i].buf_type = SBUF_TYPE_GAMMA;
        p_sbuf_mgr->gamma_sbuf_arr[i].buf_base = (void *)&( p_sbuf_mgr->sbuf_base->gamma_sbuf[i] );
    }

    memset( &p_sbuf_mgr->gamma_arr_info, 0, sizeof( p_sbuf_mgr->gamma_arr_info ) );
    p_sbuf_mgr->gamma_arr_info.item_total_count = SBUF_STATS_ARRAY_SIZE;
    p_sbuf_mgr->gamma_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY] = SBUF_STATS_ARRAY_SIZE;

    p_sbuf_mgr->gamma_arr_info.write_idx = 0;
    p_sbuf_mgr->gamma_arr_info.read_idx = SBUF_STATS_ARRAY_SIZE;
#endif

#if defined( ISP_HAS_IRIDIX_MANUAL_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM )
    /***  For Iridix  ***/
    for ( i = 0; i < SBUF_STATS_ARRAY_SIZE; i++ ) {
        p_sbuf_mgr->iridix_sbuf_arr[i].buf_idx = i;
        p_sbuf_mgr->iridix_sbuf_arr[i].buf_status = SBUF_STATUS_DATA_EMPTY;
        p_sbuf_mgr->iridix_sbuf_arr[i].buf_type = SBUF_TYPE_IRIDIX;
        p_sbuf_mgr->iridix_sbuf_arr[i].buf_base = (void *)&( p_sbuf_mgr->sbuf_base->iridix_sbuf[i] );
    }

    memset( &p_sbuf_mgr->iridix_arr_info, 0, sizeof( p_sbuf_mgr->iridix_arr_info ) );
    p_sbuf_mgr->iridix_arr_info.item_total_count = SBUF_STATS_ARRAY_SIZE;
    p_sbuf_mgr->iridix_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY] = SBUF_STATS_ARRAY_SIZE;

    p_sbuf_mgr->iridix_arr_info.write_idx = 0;
    p_sbuf_mgr->iridix_arr_info.read_idx = SBUF_STATS_ARRAY_SIZE;
#endif

    spin_unlock_irqrestore( &p_sbuf_mgr->sbuf_lock, irq_flags );
}

static int sbuf_mgr_init( struct sbuf_mgr *p_sbuf_mgr )
{
    int rc;

    p_sbuf_mgr->sbuf_inited = 0;
    spin_lock_init( &( p_sbuf_mgr->sbuf_lock ) );

    rc = sbuf_mgr_alloc_sbuf( p_sbuf_mgr );
    if ( rc ) {
        LOG( LOG_ERR, "sbuf_mgr alloc buffer failed, ret: %d.", rc );
        return rc;
    }

    sbuf_mgr_init_sbuf( p_sbuf_mgr );

    return 0;
}

#define _GET_LUT_SIZE( lut ) ( lut->rows * lut->cols * lut->width )

static uint32_t get_cur_calibration_total_size( void *fw_instance )
{
    uint32_t result = 0;
    uint32_t idx = 0;
    LookupTable *p_lut = NULL;

    LOG( LOG_ERR, "fw_instance: %p, CALIBRATION_TOTAL_SIZE: %d.", fw_instance, CALIBRATION_TOTAL_SIZE );

    for ( idx = 0; idx < CALIBRATION_TOTAL_SIZE; idx++ ) {

        p_lut = _GET_LOOKUP_PTR( fw_instance, idx );

        if ( p_lut ) {
            result += _GET_LUT_SIZE( p_lut );
        } else {
            LOG( LOG_ERR, "Error: LUT %d is NULL(%p), not inited.", idx, p_lut );
        }

        result += sizeof( LookupTable );
    }

    LOG( LOG_ERR, "Total size for all IQ LUTs is %d bytes", result );
    return result;
}

/*
    sbuf calibration memory layout: N is CALIBRATION_TOTAL_SIZE

    ----------
    |  lut1  |---
    |--------|  |
    |  lut2  |  |
    |--------|  |
    |  ....  |  |
    |--------|  |
    |  lutN  |--|----
    |--------|  |   |
    |  data1 |<--   |
    |--------|      |
    |  data2 |      |
    |--------|      |
    |  ....  |      |
    |--------|      |
    |  dataN |<------
    |--------|
*/
static int update_cur_calibration_to_sbuf( void *fw_instance, struct sbuf_mgr *p_sbuf_mgr )
{
    int rc = 0;
    uint32_t idx = 0;
    uint32_t lut_size = 0;
    LookupTable *p_lut = NULL;

    uint8_t *sbuf_cali_base = (uint8_t *)&p_sbuf_mgr->sbuf_base->kf_info.cali_info.cali_data;
    struct sbuf_lookup_table *p_sbuf_lut_arr = (struct sbuf_lookup_table *)sbuf_cali_base;
    uint8_t *p_sbuf_cali_data = sbuf_cali_base + sizeof( struct sbuf_lookup_table ) * CALIBRATION_TOTAL_SIZE;


    LOG( LOG_ERR, "sbuf_cali_base: %p, p_sbuf_lut_arr: %p, p_sbuf_cali_data: %p", sbuf_cali_base, p_sbuf_lut_arr, p_sbuf_cali_data );

    for ( idx = 0; idx < CALIBRATION_TOTAL_SIZE; idx++ ) {
        p_lut = _GET_LOOKUP_PTR( fw_instance, idx );
        if ( !p_lut ) {
            // NOTE: Don't touch ptr values, UF will manage it.
            // p_sbuf_lut_arr[idx].ptr = 0;
            p_sbuf_lut_arr[idx].rows = 0;
            p_sbuf_lut_arr[idx].cols = 0;
            p_sbuf_lut_arr[idx].width = 0;
            continue;
        }

        lut_size = _GET_LUT_SIZE( p_lut );

        if ( !p_lut->ptr ) {
            rc = -1;
            LOG( LOG_CRIT, "IQ LUT %d is NULL", idx );
            break;
        }

        // NOTE: Don't touch ptr values, UF will manage it.
        // p_sbuf_lut_arr[idx].ptr = NULL;
        p_sbuf_lut_arr[idx].rows = p_lut->rows;
        p_sbuf_lut_arr[idx].cols = p_lut->cols;
        p_sbuf_lut_arr[idx].width = p_lut->width;

        memcpy( p_sbuf_cali_data, p_lut->ptr, lut_size );
        p_sbuf_cali_data += lut_size;
    }

    p_sbuf_mgr->sbuf_base->kf_info.cali_info.is_fetched = 0;

    return rc;
}

static uint32_t sbuf_mgr_item_count_in_using( struct sbuf_mgr *p_sbuf_mgr )
{
    uint32_t rc = 0;
    unsigned long irq_flags;

    spin_lock_irqsave( &p_sbuf_mgr->sbuf_lock, irq_flags );

#if defined( ISP_HAS_AE_MANUAL_FSM )
    rc += p_sbuf_mgr->ae_arr_info.item_status_count[SBUF_STATUS_DATA_USING];
#endif

#if defined( ISP_HAS_AWB_MANUAL_FSM )
    rc += p_sbuf_mgr->awb_arr_info.item_status_count[SBUF_STATUS_DATA_USING];
#endif

#if defined( ISP_HAS_AF_MANUAL_FSM )
    rc += p_sbuf_mgr->af_arr_info.item_status_count[SBUF_STATUS_DATA_USING];
#endif

#if defined( ISP_HAS_GAMMA_MANUAL_FSM )
    rc += p_sbuf_mgr->gamma_arr_info.item_status_count[SBUF_STATUS_DATA_USING];
#endif

#if defined( ISP_HAS_IRIDIX_MANUAL_FSM )
    rc += p_sbuf_mgr->iridix_arr_info.item_status_count[SBUF_STATUS_DATA_USING];
#endif

    spin_unlock_irqrestore( &p_sbuf_mgr->sbuf_lock, irq_flags );

    LOG( LOG_ERR, "sbuf item using total count: %u.", rc );

    return rc;
}

static uint8_t sbuf_calibration_is_ready_to_update( struct sbuf_context *p_ctx )
{
    uint32_t rc = 1;

    // We can update the sbuf-calibration if no sbuf item used by UF.
    // or if UF is running but not fetched yet.
    if ( sbuf_mgr_item_count_in_using( &p_ctx->sbuf_mgr ) ||
         ( p_ctx->dev_opened && ( p_ctx->sbuf_mgr.sbuf_base->kf_info.cali_info.is_fetched == 0 ) ) ) {
        rc = 0;
    }

    return rc;
}

static int sbuf_calibration_init( struct sbuf_context *p_ctx, uint8_t enable, uint32_t mode )
{
    int rc = 0;
    uint32_t cnt = 0;
    uint32_t cali_total_size = 0;

    LOG( LOG_ERR, "sbuf_calibration_init." );

    uint32_t wdr_mode = 0;

    acamera_fsm_mgr_get_param( p_ctx->p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_WDR_MODE, NULL, 0, &wdr_mode, sizeof( wdr_mode ) );

    if ( enable == 0 ) {
        if ( wdr_mode == p_ctx->sbuf_mgr.cur_wdr_mode ) {
            LOG( LOG_INFO, "same wdr_mode, already inited, return." );
            return 0;
        }
    }

    p_ctx->p_fsm->is_paused = 1;

    // get total calibration size, including LUTs
    cali_total_size = get_cur_calibration_total_size( ACAMERA_FSM2CTX_PTR( p_ctx->p_fsm ) );

    if ( cali_total_size > ISP_MAX_CALIBRATION_DATA_SIZE ) {
        LOG( LOG_CRIT, "Error: Not enough memory for calibration data, total_size: %d, max_size: %d.", cali_total_size, ISP_MAX_CALIBRATION_DATA_SIZE );
        p_ctx->p_fsm->is_paused = 0;
        return -1;
    }

    while ( !sbuf_calibration_is_ready_to_update( p_ctx ) ) {
        LOG( LOG_ERR, "wait for UF to finish using" );
        // sleep 3 ms
        system_timer_usleep( 3 * 1000 );

        cnt++;

        // 3 seconds timeout
        if ( cnt >= 1000 ) {
            LOG( LOG_CRIT, "timeout to wait for calibration ready to update" );
            p_ctx->p_fsm->is_paused = 0;
            return -1;
        }
    }

    rc = update_cur_calibration_to_sbuf( ACAMERA_FSM2CTX_PTR( p_ctx->p_fsm ), &p_ctx->sbuf_mgr );

    p_ctx->p_fsm->is_paused = 0;

    LOG( LOG_CRIT, "sbuf_calibration wdr_mode updated: %d -> %d.", p_ctx->sbuf_mgr.cur_wdr_mode, wdr_mode );

    p_ctx->sbuf_mgr.cur_wdr_mode = wdr_mode;

    return rc;
}


void sbuf_update_calibration_data( sbuf_fsm_ptr_t p_fsm, uint8_t enable, uint32_t mode )
{
    uint32_t fw_id = p_fsm->cmn.ctx_id;
    struct sbuf_context *p_ctx = NULL;

    p_ctx = &( sbuf_contexts[fw_id] );
    if ( p_ctx->fw_id != fw_id ) {
        LOG( LOG_CRIT, "Error: ctx_id not match, fsm fw_id: %d, ctx_id: %d.", fw_id, p_ctx->fw_id );
        return;
    }

    sbuf_calibration_init( p_ctx, enable, mode );
}


static int sbuf_ctx_init( struct sbuf_context *p_ctx )
{
    int rc;

    rc = sbuf_mgr_init( &p_ctx->sbuf_mgr );
    if ( rc ) {
        LOG( LOG_ERR, "init failed, error: sbuf_mgr init failed, ret: %d.", rc );

        return rc;
    }

    rc = sbuf_calibration_init( p_ctx, 0, 0 );
    if ( rc ) {
        LOG( LOG_ERR, "init failed, error: calibration init failed, ret: %d.", rc );
    }

    return rc;
}

static void sbuf_mgr_reset( struct sbuf_mgr *p_sbuf_mgr )
{
    int i;
    unsigned long irq_flags;

    spin_lock_init( &( p_sbuf_mgr->sbuf_lock ) );

    spin_lock_irqsave( &p_sbuf_mgr->sbuf_lock, irq_flags );

    p_sbuf_mgr->sbuf_base->kf_info.cali_info.is_fetched = 0;

    for ( i = 0; i < SBUF_STATS_ARRAY_SIZE; i++ ) {
#if defined( ISP_HAS_AE_MANUAL_FSM )
        /***  For AE  ***/
        if ( SBUF_STATUS_DATA_USING == p_sbuf_mgr->ae_sbuf_arr[i].buf_status ) {
            p_sbuf_mgr->ae_sbuf_arr[i].buf_status = SBUF_STATUS_DATA_EMPTY;
            p_sbuf_mgr->ae_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY]++;
            p_sbuf_mgr->ae_arr_info.item_status_count[SBUF_STATUS_DATA_USING]--;
        }

        if ( i == SBUF_STATS_ARRAY_SIZE - 1 ) {
            LOG( LOG_DEBUG, "ae sbuf arr info: read_idx: %u, write_idx: %u, status_count: %u-%u-%u-%u.",
                 p_sbuf_mgr->ae_arr_info.read_idx,
                 p_sbuf_mgr->ae_arr_info.write_idx,
                 p_sbuf_mgr->ae_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY],
                 p_sbuf_mgr->ae_arr_info.item_status_count[SBUF_STATUS_DATA_PREPARE],
                 p_sbuf_mgr->ae_arr_info.item_status_count[SBUF_STATUS_DATA_DONE],
                 p_sbuf_mgr->ae_arr_info.item_status_count[SBUF_STATUS_DATA_USING] );
        }
#endif

#if defined( ISP_HAS_AWB_MANUAL_FSM )
        /***  For AWB  ***/
        if ( SBUF_STATUS_DATA_USING == p_sbuf_mgr->awb_sbuf_arr[i].buf_status ) {
            p_sbuf_mgr->awb_sbuf_arr[i].buf_status = SBUF_STATUS_DATA_EMPTY;
            p_sbuf_mgr->awb_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY]++;
            p_sbuf_mgr->awb_arr_info.item_status_count[SBUF_STATUS_DATA_USING]--;
        }

        if ( i == SBUF_STATS_ARRAY_SIZE - 1 ) {
            LOG( LOG_DEBUG, "awb sbuf arr info: read_idx: %u, write_idx: %u, status_count: %u-%u-%u-%u.",
                 p_sbuf_mgr->awb_arr_info.read_idx,
                 p_sbuf_mgr->awb_arr_info.write_idx,
                 p_sbuf_mgr->awb_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY],
                 p_sbuf_mgr->awb_arr_info.item_status_count[SBUF_STATUS_DATA_PREPARE],
                 p_sbuf_mgr->awb_arr_info.item_status_count[SBUF_STATUS_DATA_DONE],
                 p_sbuf_mgr->awb_arr_info.item_status_count[SBUF_STATUS_DATA_USING] );
        }
#endif

#if defined( ISP_HAS_AF_MANUAL_FSM )
        /***  For AF  ***/
        if ( SBUF_STATUS_DATA_USING == p_sbuf_mgr->af_sbuf_arr[i].buf_status ) {
            p_sbuf_mgr->af_sbuf_arr[i].buf_status = SBUF_STATUS_DATA_EMPTY;
            p_sbuf_mgr->af_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY]++;
            p_sbuf_mgr->af_arr_info.item_status_count[SBUF_STATUS_DATA_USING]--;
        }

        if ( i == SBUF_STATS_ARRAY_SIZE - 1 ) {
            LOG( LOG_DEBUG, "af sbuf arr info: read_idx: %u, write_idx: %u, status_count: %u-%u-%u-%u.",
                 p_sbuf_mgr->af_arr_info.read_idx,
                 p_sbuf_mgr->af_arr_info.write_idx,
                 p_sbuf_mgr->af_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY],
                 p_sbuf_mgr->af_arr_info.item_status_count[SBUF_STATUS_DATA_PREPARE],
                 p_sbuf_mgr->af_arr_info.item_status_count[SBUF_STATUS_DATA_DONE],
                 p_sbuf_mgr->af_arr_info.item_status_count[SBUF_STATUS_DATA_USING] );
        }
#endif

#if defined( ISP_HAS_GAMMA_MANUAL_FSM )
        /***  For Gamma Stats ***/
        if ( SBUF_STATUS_DATA_USING == p_sbuf_mgr->gamma_sbuf_arr[i].buf_status ) {
            p_sbuf_mgr->gamma_sbuf_arr[i].buf_status = SBUF_STATUS_DATA_EMPTY;
            p_sbuf_mgr->gamma_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY]++;
            p_sbuf_mgr->gamma_arr_info.item_status_count[SBUF_STATUS_DATA_USING]--;
        }

        if ( i == SBUF_STATS_ARRAY_SIZE - 1 ) {
            LOG( LOG_DEBUG, "gamma sbuf arr info: read_idx: %u, write_idx: %u, status_count: %u-%u-%u-%u.",
                 p_sbuf_mgr->gamma_arr_info.read_idx,
                 p_sbuf_mgr->gamma_arr_info.write_idx,
                 p_sbuf_mgr->gamma_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY],
                 p_sbuf_mgr->gamma_arr_info.item_status_count[SBUF_STATUS_DATA_PREPARE],
                 p_sbuf_mgr->gamma_arr_info.item_status_count[SBUF_STATUS_DATA_DONE],
                 p_sbuf_mgr->gamma_arr_info.item_status_count[SBUF_STATUS_DATA_USING] );
        }
#endif

#if defined( ISP_HAS_IRIDIX_MANUAL_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM )
        /***  For Iridix  ***/
        if ( SBUF_STATUS_DATA_USING == p_sbuf_mgr->iridix_sbuf_arr[i].buf_status ) {
            p_sbuf_mgr->iridix_sbuf_arr[i].buf_status = SBUF_STATUS_DATA_EMPTY;
            p_sbuf_mgr->iridix_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY]++;
            p_sbuf_mgr->iridix_arr_info.item_status_count[SBUF_STATUS_DATA_USING]--;
        }

        if ( i == SBUF_STATS_ARRAY_SIZE - 1 ) {
            LOG( LOG_DEBUG, "iridix sbuf arr info: read_idx: %u, write_idx: %u, status_count: %u-%u-%u-%u.",
                 p_sbuf_mgr->iridix_arr_info.read_idx,
                 p_sbuf_mgr->iridix_arr_info.write_idx,
                 p_sbuf_mgr->iridix_arr_info.item_status_count[SBUF_STATUS_DATA_EMPTY],
                 p_sbuf_mgr->iridix_arr_info.item_status_count[SBUF_STATUS_DATA_PREPARE],
                 p_sbuf_mgr->iridix_arr_info.item_status_count[SBUF_STATUS_DATA_DONE],
                 p_sbuf_mgr->iridix_arr_info.item_status_count[SBUF_STATUS_DATA_USING] );
        }
#endif
    }

    spin_unlock_irqrestore( &p_sbuf_mgr->sbuf_lock, irq_flags );
}

static int sbuf_mgr_free( struct sbuf_mgr *p_sbuf_mgr )
{
    unsigned long irq_flags;

    if ( !is_sbuf_inited( p_sbuf_mgr ) ) {
        LOG( LOG_ERR, "Error: sbuf alloc is not inited, can't free." );
        return -ENOMEM;
    }

    /* set the flag before we free in case sb use it after free but before flag is clear. */
    spin_lock_irqsave( &p_sbuf_mgr->sbuf_lock, irq_flags );
    p_sbuf_mgr->sbuf_inited = 0;
    spin_unlock_irqrestore( &p_sbuf_mgr->sbuf_lock, irq_flags );

    if ( p_sbuf_mgr->buf_allocated ) {
        int i;
        /* clear the reserved flag before free so that no bug showed when freeed */
        for ( i = 0; i < p_sbuf_mgr->len_used; i += PAGE_SIZE ) {
            ClearPageReserved( virt_to_page( p_sbuf_mgr->buf_used + i ) );
        }

        kfree( p_sbuf_mgr->buf_allocated );
        p_sbuf_mgr->buf_allocated = NULL;
        p_sbuf_mgr->buf_used = NULL;
    } else {
        LOG( LOG_ERR, "Error: sbuf allocated memory is NULL." );
    }

    return 0;
}

/* function will be called when this FSM received ae_stats_data_ready event */
void sbuf_update_ae_idx( sbuf_fsm_t *p_fsm )
{
    int rc = 0;
    struct sbuf_item sbuf;
#if defined( ISP_HAS_IRIDIX_MANUAL_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM )
    struct sbuf_item sbuf_iridix;
#endif
    struct sbuf_context *p_ctx = NULL;
    uint32_t fw_id = p_fsm->cmn.ctx_id;

    if ( fw_id >= acamera_get_context_number() ) {
        LOG( LOG_CRIT, "Fatal error: Invalid FW context ID: %d, max is: %d", fw_id, acamera_get_context_number() - 1 );
        return;
    }

    p_ctx = &( sbuf_contexts[fw_id] );

    /* no matter UF is running or not, we need drain stats from the queue,
       otherwise when UF launches, it will not get latest stats */
    memset( &sbuf, 0, sizeof( sbuf ) );
    sbuf.buf_status = SBUF_STATUS_DATA_DONE;
    sbuf.buf_type = SBUF_TYPE_AE;

    if ( sbuf_get_item( p_ctx->fw_id, &sbuf ) ) {
        LOG( LOG_ERR, "Failed to get sbuf for ae done buffer, fw_id: %d.", p_ctx->fw_id );
        return;
    }

#if defined( ISP_HAS_IRIDIX_MANUAL_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM )
    /*NOTE: iridix */
    memset( &sbuf_iridix, 0, sizeof( sbuf_iridix ) );
    sbuf_iridix.buf_status = SBUF_STATUS_DATA_DONE;
    sbuf_iridix.buf_type = SBUF_TYPE_IRIDIX;

    if ( sbuf_get_item( p_ctx->fw_id, &sbuf_iridix ) ) {
        LOG( LOG_DEBUG, "Failed to get sbuf for iridix done buffer, fw_id: %d.", p_ctx->fw_id );

        // we need to return AE sbuf to sbuf_manager when error is happened.
        sbuf.buf_status = SBUF_STATUS_DATA_EMPTY;
        sbuf_set_item( p_ctx->fw_id, &sbuf );

        return;
    }
#endif

    if ( !p_ctx->dev_opened || p_fsm->is_paused ) {
        LOG( LOG_DEBUG, "device is not opened or paused, skip, fw_id: %d.", fw_id );
        sbuf.buf_status = SBUF_STATUS_DATA_EMPTY;
        sbuf_set_item( p_ctx->fw_id, &sbuf );

#if defined( ISP_HAS_IRIDIX_MANUAL_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM )
        sbuf_iridix.buf_status = SBUF_STATUS_DATA_EMPTY;
        sbuf_set_item( p_ctx->fw_id, &sbuf_iridix );
#endif
        return;
    }


    rc = mutex_lock_interruptible( &p_ctx->idx_set_lock );
    if ( rc ) {
        LOG( LOG_ERR, "Error: access lock failed, rc: %d.", rc );
        // we need to return this sbuf to sbuf_manager when error is happened.
        sbuf.buf_status = SBUF_STATUS_DATA_EMPTY;
        sbuf_set_item( p_ctx->fw_id, &sbuf );

#if defined( ISP_HAS_IRIDIX_MANUAL_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM )
        sbuf_iridix.buf_status = SBUF_STATUS_DATA_EMPTY;
        sbuf_set_item( p_ctx->fw_id, &sbuf_iridix );
#endif
        return;
    }

    p_ctx->idx_set.ae_idx = sbuf.buf_idx;
    p_ctx->idx_set.ae_idx_valid = 1;

#if defined( ISP_HAS_IRIDIX_MANUAL_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM )
    p_ctx->idx_set.iridix_idx = sbuf_iridix.buf_idx;
    p_ctx->idx_set.iridix_idx_valid = 1;

    // iridix depends on AE stats data
    sbuf_ae_t *p_sbuf_ae;
    sbuf_iridix_t *p_sbuf_iridix;

    p_sbuf_ae = (sbuf_ae_t *)&( p_ctx->sbuf_mgr.sbuf_base->ae_sbuf[sbuf.buf_idx] );
    p_sbuf_iridix = (sbuf_iridix_t *)&( p_ctx->sbuf_mgr.sbuf_base->iridix_sbuf[sbuf_iridix.buf_idx] );

    p_sbuf_iridix->frame_id = p_sbuf_ae->frame_id;

    acamera_fsm_mgr_set_param( p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_IRIDIX_FRAME_ID, &p_sbuf_iridix->frame_id, sizeof( p_sbuf_iridix->frame_id ) );
#endif

    mutex_unlock( &p_ctx->idx_set_lock );

    wake_up_interruptible( &p_ctx->idx_set_wait_queue );
}

void sbuf_update_awb_idx( sbuf_fsm_t *p_fsm )
{
    int rc = 0;
    struct sbuf_item sbuf;
    struct sbuf_context *p_ctx = NULL;
    uint32_t fw_id = p_fsm->cmn.ctx_id;

    if ( fw_id >= acamera_get_context_number() ) {
        LOG( LOG_CRIT, "Fatal error: Invalid FW context ID: %d, max is: %d", fw_id, acamera_get_context_number() - 1 );
        return;
    }

    p_ctx = &( sbuf_contexts[fw_id] );

    /* no matter UF is running or not, we need drain stats from the queue,
       otherwise when UF launches, it will not get latest stats */
    memset( &sbuf, 0, sizeof( sbuf ) );
    sbuf.buf_status = SBUF_STATUS_DATA_DONE;
    sbuf.buf_type = SBUF_TYPE_AWB;

    if ( sbuf_get_item( p_ctx->fw_id, &sbuf ) ) {
        LOG( LOG_DEBUG, "Failed to get sbuf for awb done buffer, fw_id: %d.", p_ctx->fw_id );
        return;
    }


    if ( !p_ctx->dev_opened || p_fsm->is_paused ) {
        LOG( LOG_DEBUG, "device is not opened or paused, skip, fw_id: %d.", fw_id );
        sbuf.buf_status = SBUF_STATUS_DATA_EMPTY;
        sbuf_set_item( p_ctx->fw_id, &sbuf );
        return;
    }


    rc = mutex_lock_interruptible( &p_ctx->idx_set_lock );
    if ( rc ) {
        LOG( LOG_ERR, "Error: access lock failed, rc: %d.", rc );
        // we need to return this sbuf to sbuf_manager when error is happened.
        sbuf.buf_status = SBUF_STATUS_DATA_EMPTY;
        sbuf_set_item( p_ctx->fw_id, &sbuf );
        return;
    }

    p_ctx->idx_set.awb_idx = sbuf.buf_idx;
    p_ctx->idx_set.awb_idx_valid = 1;

    mutex_unlock( &p_ctx->idx_set_lock );

    wake_up_interruptible( &p_ctx->idx_set_wait_queue );
}

void sbuf_update_af_idx( sbuf_fsm_t *p_fsm )
{
    int rc = 0;
    struct sbuf_item sbuf;
    struct sbuf_context *p_ctx = NULL;
    uint32_t fw_id = p_fsm->cmn.ctx_id;

    if ( fw_id >= acamera_get_context_number() ) {
        LOG( LOG_CRIT, "Fatal error: Invalid FW context ID: %d, max is: %d", fw_id, acamera_get_context_number() - 1 );
        return;
    }

    p_ctx = &( sbuf_contexts[fw_id] );

    /* no matter UF is running or not, we need drain stats from the queue,
       otherwise when UF launches, it will not get latest stats */
    memset( &sbuf, 0, sizeof( sbuf ) );
    sbuf.buf_status = SBUF_STATUS_DATA_DONE;
    sbuf.buf_type = SBUF_TYPE_AF;

    if ( sbuf_get_item( p_ctx->fw_id, &sbuf ) ) {
        LOG( LOG_DEBUG, "Failed to get sbuf for af done buffer, fw_id: %d.", p_ctx->fw_id );
        return;
    }

    if ( !p_ctx->dev_opened || p_fsm->is_paused ) {
        LOG( LOG_DEBUG, "device is not opened or paused, skip, fw_id: %d.", fw_id );
        sbuf.buf_status = SBUF_STATUS_DATA_EMPTY;
        sbuf_set_item( p_ctx->fw_id, &sbuf );
        return;
    }


    rc = mutex_lock_interruptible( &p_ctx->idx_set_lock );
    if ( rc ) {
        LOG( LOG_ERR, "Error: access lock failed, rc: %d.", rc );
        // we need to return this sbuf to sbuf_manager when error is happened.
        sbuf.buf_status = SBUF_STATUS_DATA_EMPTY;
        sbuf_set_item( p_ctx->fw_id, &sbuf );
        return;
    }

    p_ctx->idx_set.af_idx = sbuf.buf_idx;
    p_ctx->idx_set.af_idx_valid = 1;

    mutex_unlock( &p_ctx->idx_set_lock );

    wake_up_interruptible( &p_ctx->idx_set_wait_queue );
}

void sbuf_update_gamma_idx( sbuf_fsm_t *p_fsm )
{
    int rc = 0;
    struct sbuf_item sbuf;
    struct sbuf_context *p_ctx = NULL;
    uint32_t fw_id = p_fsm->cmn.ctx_id;

    if ( fw_id >= acamera_get_context_number() ) {
        LOG( LOG_CRIT, "Fatal error: Invalid FW context ID: %d, max is: %d", fw_id, acamera_get_context_number() - 1 );
        return;
    }

    p_ctx = &( sbuf_contexts[fw_id] );

    /* no matter UF is running or not, we need drain stats from the queue,
       otherwise when UF launches, it will not get latest stats */
    memset( &sbuf, 0, sizeof( sbuf ) );
    sbuf.buf_status = SBUF_STATUS_DATA_DONE;
    sbuf.buf_type = SBUF_TYPE_GAMMA;

    if ( sbuf_get_item( p_ctx->fw_id, &sbuf ) ) {
        LOG( LOG_DEBUG, "Failed to get sbuf for gamma done buffer, fw_id: %d.", p_ctx->fw_id );
        return;
    }

    if ( !p_ctx->dev_opened || p_fsm->is_paused ) {
        LOG( LOG_DEBUG, "device is not opened or paused, skip, fw_id: %d.", fw_id );
        sbuf.buf_status = SBUF_STATUS_DATA_EMPTY;
        sbuf_set_item( p_ctx->fw_id, &sbuf );
        return;
    }


    rc = mutex_lock_interruptible( &p_ctx->idx_set_lock );
    if ( rc ) {
        LOG( LOG_ERR, "Error: access lock failed, rc: %d.", rc );
        // we need to return this sbuf to sbuf_manager when error is happened.
        sbuf.buf_status = SBUF_STATUS_DATA_EMPTY;
        sbuf_set_item( p_ctx->fw_id, &sbuf );
        return;
    }

    p_ctx->idx_set.gamma_idx = sbuf.buf_idx;
    p_ctx->idx_set.gamma_idx_valid = 1;

    mutex_unlock( &p_ctx->idx_set_lock );

    wake_up_interruptible( &p_ctx->idx_set_wait_queue );
}

static uint32_t sbuf_is_ready_to_send_data( struct sbuf_context *p_ctx )
{
    uint32_t rc = 1;

    // If sbuf FSM is paused or UF didn't fetched calibration data yet,
    // we need to wait before send stats data.
    if ( p_ctx->p_fsm->is_paused ||
         !p_ctx->sbuf_mgr.sbuf_base->kf_info.cali_info.is_fetched ) {
        LOG( LOG_INFO, "is_paused: %d, is_fetched: %d.", p_ctx->p_fsm->is_paused, p_ctx->sbuf_mgr.sbuf_base->kf_info.cali_info.is_fetched );

        rc = 0;
    }

    return rc;
}

static void sbuf_recycle_idx_set( struct sbuf_context *p_ctx, struct sbuf_idx_set *p_idx_set )
{
    struct sbuf_item item;

    if ( p_idx_set->ae_idx_valid ) {
        item.buf_idx = p_idx_set->ae_idx;
        item.buf_type = SBUF_TYPE_AE;
        item.buf_status = SBUF_STATUS_DATA_EMPTY;

        sbuf_set_item( p_ctx->fw_id, &item );
    }

    if ( p_idx_set->awb_idx_valid ) {
        item.buf_idx = p_idx_set->awb_idx;
        item.buf_type = SBUF_TYPE_AWB;
        item.buf_status = SBUF_STATUS_DATA_EMPTY;

        sbuf_set_item( p_ctx->fw_id, &item );
    }

    if ( p_idx_set->af_idx_valid ) {
        item.buf_idx = p_idx_set->af_idx;
        item.buf_type = SBUF_TYPE_AF;
        item.buf_status = SBUF_STATUS_DATA_EMPTY;

        sbuf_set_item( p_ctx->fw_id, &item );
    }

    if ( p_idx_set->gamma_idx_valid ) {
        item.buf_idx = p_idx_set->gamma_idx;
        item.buf_type = SBUF_TYPE_GAMMA;
        item.buf_status = SBUF_STATUS_DATA_EMPTY;

        sbuf_set_item( p_ctx->fw_id, &item );
    }

    if ( p_idx_set->iridix_idx_valid ) {
        item.buf_idx = p_idx_set->iridix_idx;
        item.buf_type = SBUF_TYPE_IRIDIX;
        item.buf_status = SBUF_STATUS_DATA_EMPTY;

        sbuf_set_item( p_ctx->fw_id, &item );
    }
}

static void sbuf_mgr_get_latest_idx_set( struct sbuf_context *p_ctx, struct sbuf_idx_set *p_idx_set )
{
    int rc;
    uint32_t wait = 0;

    // set all items to invalid by default.
    memset( p_idx_set, 0, sizeof( *p_idx_set ) );

    rc = mutex_lock_interruptible( &p_ctx->idx_set_lock );
    if ( rc ) {
        LOG( LOG_ERR, "Error: access lock failed, rc: %d.", rc );
        return;
    }

    if ( is_idx_set_has_valid_item( &p_ctx->idx_set ) ) {
        *p_idx_set = p_ctx->idx_set;

        // reset the valid flag to prepare next read
        memset( &p_ctx->idx_set, 0, sizeof( p_ctx->idx_set ) );
    } else {
        wait = 1;
    }

    mutex_unlock( &p_ctx->idx_set_lock );

    if ( wait ) {
        long time_out_in_jiffies = 30; /* jiffies is depend on HW, in x86 Ubuntu, it's 4 ms, 30 is 120ms. */

        /* wait for the event */
        LOG( LOG_DEBUG, "wait for data, timeout_in_jiffies: %ld, HZ: %d.", time_out_in_jiffies, HZ );
        rc = wait_event_interruptible_timeout( p_ctx->idx_set_wait_queue, is_idx_set_has_valid_item( &p_ctx->idx_set ), time_out_in_jiffies );
        LOG( LOG_DEBUG, "after timeout, rc: %d, is_idx_set_has_valid_item: %d.", rc, is_idx_set_has_valid_item( &p_ctx->idx_set ) );

        rc = mutex_lock_interruptible( &p_ctx->idx_set_lock );
        if ( rc ) {
            LOG( LOG_ERR, "Error: 2nd access lock failed, rc: %d.", rc );
            return;
        }

        *p_idx_set = p_ctx->idx_set;

        // reset to invalid flag to prepare for next read since we already send to user space
        memset( &p_ctx->idx_set, 0, sizeof( p_ctx->idx_set ) );
        mutex_unlock( &p_ctx->idx_set_lock );
    }
}

static void sbuf_mgr_apply_new_param( struct sbuf_context *p_ctx, struct sbuf_idx_set *p_idx_set )
{
    struct sbuf_item item;

#if defined( ISP_HAS_AE_MANUAL_FSM )
    /* AE */
    if ( p_idx_set->ae_idx_valid && ( p_idx_set->ae_idx < SBUF_STATS_ARRAY_SIZE ) ) {
        sbuf_ae_t *p_sbuf_ae;

        p_sbuf_ae = (sbuf_ae_t *)&( p_ctx->sbuf_mgr.sbuf_base->ae_sbuf[p_idx_set->ae_idx] );

        acamera_fsm_mgr_set_param( p_ctx->p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_AE_NEW_PARAM, p_sbuf_ae, sizeof( *p_sbuf_ae ) );

        LOG( LOG_DEBUG, "ctx: %d, AE exposure: %d, exp_ratio: %u ae_hist_mean %d", p_ctx->fw_id, (int)p_sbuf_ae->ae_exposure, (unsigned int)p_sbuf_ae->ae_exposure_ratio,p_sbuf_ae->ae_hist_mean);

        /* set this sbuf back to sbuf_mgr */
        item.buf_idx = p_idx_set->ae_idx;
        item.buf_type = SBUF_TYPE_AE;
        item.buf_status = SBUF_STATUS_DATA_EMPTY;

        sbuf_set_item( p_ctx->fw_id, &item );
    }
#endif

#if defined( ISP_HAS_AWB_MANUAL_FSM )
    /* AWB */
    if ( p_idx_set->awb_idx_valid && ( p_idx_set->awb_idx < SBUF_STATS_ARRAY_SIZE ) ) {
        sbuf_awb_t *p_sbuf_awb;

        p_sbuf_awb = (sbuf_awb_t *)&( p_ctx->sbuf_mgr.sbuf_base->awb_sbuf[p_idx_set->awb_idx] );

        acamera_fsm_mgr_set_param( p_ctx->p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_AWB_NEW_PARAM, p_sbuf_awb, sizeof( *p_sbuf_awb ) );

        LOG( LOG_DEBUG, "ctx: %d, AWB param: awb_red_gain: %u, awb_blue_gain: %u, temperature: %d, light_source: %u, p_high: %u",
             p_ctx->fw_id,
             p_sbuf_awb->awb_red_gain,
             p_sbuf_awb->awb_blue_gain,
             p_sbuf_awb->temperature_detected,
             p_sbuf_awb->light_source_candidate,
             p_sbuf_awb->p_high );

        /* set this sbuf back to sbuf_mgr */
        item.buf_idx = p_idx_set->awb_idx;
        item.buf_type = SBUF_TYPE_AWB;
        item.buf_status = SBUF_STATUS_DATA_EMPTY;

        sbuf_set_item( p_ctx->fw_id, &item );
    }
#endif

#if defined( ISP_HAS_AF_MANUAL_FSM )

    /* AF */
    if ( p_idx_set->af_idx_valid && ( p_idx_set->af_idx < SBUF_STATS_ARRAY_SIZE ) ) {
        sbuf_af_t *p_sbuf_af;

        p_sbuf_af = (sbuf_af_t *)&( p_ctx->sbuf_mgr.sbuf_base->af_sbuf[p_idx_set->af_idx] );

        acamera_fsm_mgr_set_param( p_ctx->p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_AF_NEW_PARAM, p_sbuf_af, sizeof( *p_sbuf_af ) );

        LOG( LOG_DEBUG, "ctx: %d, AF pos: %u.", p_ctx->fw_id, p_sbuf_af->af_position );

        /* set this sbuf back to sbuf_mgr */
        item.buf_idx = p_idx_set->af_idx;
        item.buf_type = SBUF_TYPE_AF;
        item.buf_status = SBUF_STATUS_DATA_EMPTY;

        sbuf_set_item( p_ctx->fw_id, &item );
    }
#endif

#if defined( ISP_HAS_GAMMA_MANUAL_FSM )
    /* Gamma */
    if ( p_idx_set->gamma_idx_valid && ( p_idx_set->gamma_idx < SBUF_STATS_ARRAY_SIZE ) ) {
        sbuf_gamma_t *p_sbuf_gamma;

        p_sbuf_gamma = (sbuf_gamma_t *)&( p_ctx->sbuf_mgr.sbuf_base->gamma_sbuf[p_idx_set->gamma_idx] );
        acamera_fsm_mgr_set_param( p_ctx->p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_GAMMA_NEW_PARAM, p_sbuf_gamma, sizeof( *p_sbuf_gamma ) );
        LOG( LOG_DEBUG, "ctx: %d, gamma param: gain: %u, offset: %u.", p_ctx->fw_id, p_sbuf_gamma->gamma_gain, p_sbuf_gamma->gamma_offset );

        /* set this sbuf back to sbuf_mgr */
        item.buf_idx = p_idx_set->gamma_idx;
        item.buf_type = SBUF_TYPE_GAMMA;
        item.buf_status = SBUF_STATUS_DATA_EMPTY;

        sbuf_set_item( p_ctx->fw_id, &item );
    }
#endif

#if defined( ISP_HAS_IRIDIX_MANUAL_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM )
    /* Iridix */
    if ( p_idx_set->iridix_idx_valid && ( p_idx_set->iridix_idx < SBUF_STATS_ARRAY_SIZE ) ) {
        sbuf_iridix_t *p_sbuf_iridix;

        p_sbuf_iridix = (sbuf_iridix_t *)&( p_ctx->sbuf_mgr.sbuf_base->iridix_sbuf[p_idx_set->iridix_idx] );
        acamera_fsm_mgr_set_param( p_ctx->p_fsm->cmn.p_fsm_mgr, FSM_PARAM_SET_IRIDIX_NEW_PARAM, p_sbuf_iridix, sizeof( *p_sbuf_iridix ) );
        LOG( LOG_DEBUG, "ctx: %d, iridix strength: %u, iridix_dark_enh: %u.", p_ctx->fw_id, p_sbuf_iridix->strength_target, p_sbuf_iridix->iridix_dark_enh );

        /* set this sbuf back to sbuf_mgr */
        item.buf_idx = p_idx_set->iridix_idx;
        item.buf_type = SBUF_TYPE_IRIDIX;
        item.buf_status = SBUF_STATUS_DATA_EMPTY;

        sbuf_set_item( p_ctx->fw_id, &item );
    }
#endif
}

static int sbuf_get_item_from_arr( struct sbuf_mgr *p_sbuf_mgr, struct sbuf_item *item, struct sbuf_item *arr, struct sbuf_item_arr_info *info )
{
    uint32_t count;
    unsigned long irq_flags;
    int rc = -EINVAL;

    /* status cycle: Empty -> Prepare -> Done -> Using -> Empty  */
    if ( ( SBUF_STATUS_DATA_EMPTY != item->buf_status ) &&
         ( SBUF_STATUS_DATA_DONE != item->buf_status ) ) {
        LOG( LOG_ERR, "Invalid sbuf status: %d.", item->buf_status );
        return -EINVAL;
    }

    spin_lock_irqsave( &p_sbuf_mgr->sbuf_lock, irq_flags );

    LOG( LOG_DEBUG, "+++ sbuf arr info: buf_type: %s, buf_status: %s, read_idx: %u, write_idx: %u, status_count: %u-%u-%u-%u.",
         sbuf_type_str[item->buf_type],
         sbuf_status_str[item->buf_status],
         info->read_idx,
         info->write_idx,
         info->item_status_count[SBUF_STATUS_DATA_EMPTY],
         info->item_status_count[SBUF_STATUS_DATA_PREPARE],
         info->item_status_count[SBUF_STATUS_DATA_DONE],
         info->item_status_count[SBUF_STATUS_DATA_USING] );

    /* sb wants to get empty buffer? */
    if ( SBUF_STATUS_DATA_EMPTY == item->buf_status ) {
        /*
         * only support one buffer to prepare data, if there already
         * has a buffer for prepare, we should not give buffer anymore.
         */
        LOG( LOG_DEBUG, "DATA_EMPTY sbuf is getting." );
        if ( info->item_status_count[SBUF_STATUS_DATA_PREPARE] == 0 ) {
            /* get the buffer information  */
            item->buf_idx = info->write_idx;
            item->buf_base = arr[item->buf_idx].buf_base;

            /* update array information  */
            info->item_status_count[arr[item->buf_idx].buf_status]--;
            arr[item->buf_idx].buf_status = SBUF_STATUS_DATA_PREPARE;
            info->item_status_count[SBUF_STATUS_DATA_PREPARE]++;

            /* prepare for next write index */
            count = info->item_total_count;
            while ( --count ) {
                info->write_idx++;

                if ( info->write_idx >= info->item_total_count )
                    info->write_idx = 0;

                /* find the buffer for next write, we can't use the sbuf which is in using by sb. */
                if ( arr[info->write_idx].buf_status != SBUF_STATUS_DATA_USING ) {
                    /* if we're going to overwrite the next read buffer, we should update the read_idx */
                    if ( info->read_idx == info->write_idx ) {
                        do {
                            info->read_idx++;
                            if ( info->read_idx >= info->item_total_count )
                                info->read_idx = 0;

                            /* check for a whole loop to avoid infinite loop in case of some error conditions */
                            if ( info->read_idx == info->write_idx ) {
                                LOG( LOG_DEBUG, "no DONE buffer after a loop, reset read_idx." );
                                info->read_idx = info->item_total_count;
                                break;
                            }
                        } while ( arr[info->read_idx].buf_status != SBUF_STATUS_DATA_DONE );
                    }

                    /* update status count */
                    info->item_status_count[arr[info->write_idx].buf_status]--;
                    arr[info->write_idx].buf_status = SBUF_STATUS_DATA_EMPTY;
                    info->item_status_count[SBUF_STATUS_DATA_EMPTY]++;
                    break;
                }
            }

            rc = 0;
        } else {
            LOG( LOG_ERR, "Failed to get empty sbuf, prepare count: %u.",
                 info->item_status_count[SBUF_STATUS_DATA_PREPARE] );
        }
    } else {
        /* sb wants to get data_done buffer */
        /*
         * only support one buffer to using the data, if there already
         * has a buffer in using, we should failed this request.
         */
        if ( ( info->item_status_count[SBUF_STATUS_DATA_USING] == 0 ) &&
             ( info->item_status_count[SBUF_STATUS_DATA_DONE] > 0 ) &&
             ( SBUF_STATUS_DATA_DONE == arr[info->read_idx].buf_status ) ) {

            /* get the buffer information  */
            item->buf_idx = info->read_idx;
            item->buf_base = arr[item->buf_idx].buf_base;

            /* update array information  */
            info->item_status_count[arr[item->buf_idx].buf_status]--;
            arr[item->buf_idx].buf_status = SBUF_STATUS_DATA_USING;
            info->item_status_count[SBUF_STATUS_DATA_USING]++;

            /* prepare for next read index */
            count = info->item_total_count;
            while ( --count ) {
                info->read_idx++;

                if ( info->read_idx >= info->item_total_count )
                    info->read_idx = 0;

                /* if we find next DTA_DONE buffer, break */
                if ( arr[info->read_idx].buf_status == SBUF_STATUS_DATA_DONE )
                    break;
            }

            if ( arr[info->read_idx].buf_status != SBUF_STATUS_DATA_DONE ) {
                info->read_idx = info->item_total_count;
                LOG( LOG_DEBUG, "NOTE: no DONE buffer after a loop, reset read_idx." );
            }

            rc = 0;
        } else {
            LOG( LOG_DEBUG, "Failed to get done sbuf, using count: %u, done count: %u, read_idx: %u, read_idx status: %u",
                 info->item_status_count[SBUF_STATUS_DATA_USING],
                 info->item_status_count[SBUF_STATUS_DATA_DONE],
                 info->read_idx,
                 arr[info->read_idx].buf_status );
        }
    }

    LOG( LOG_DEBUG, "--- sbuf arr info: buf_type: %s, buf_status: %s, read_idx: %u, write_idx: %u, status_count: %u-%u-%u-%u.",
         sbuf_type_str[item->buf_type],
         sbuf_status_str[item->buf_status],
         info->read_idx,
         info->write_idx,
         info->item_status_count[SBUF_STATUS_DATA_EMPTY],
         info->item_status_count[SBUF_STATUS_DATA_PREPARE],
         info->item_status_count[SBUF_STATUS_DATA_DONE],
         info->item_status_count[SBUF_STATUS_DATA_USING] );

    spin_unlock_irqrestore( &p_sbuf_mgr->sbuf_lock, irq_flags );

    if ( rc ) {
        LOG( LOG_DEBUG, "Get sbuf item failed: buf_type: %s, buf_status: %s.", sbuf_type_str[item->buf_type], sbuf_status_str[item->buf_status] );
    } else {
        LOG( LOG_DEBUG, "Get sbuf item OK: buf_type: %s, buf_idx: %u, buf_status: %s, buf_base: %p.", sbuf_type_str[item->buf_type], item->buf_idx, sbuf_status_str[item->buf_status], item->buf_base );
    }

    return rc;
}

static int sbuf_set_item_to_arr( struct sbuf_mgr *p_sbuf_mgr, struct sbuf_item *item, struct sbuf_item *arr, struct sbuf_item_arr_info *info )
{
    unsigned long irq_flags;
    int rc = -EINVAL;

    /* status cycle: Empty -> Prepare -> Done -> Using -> Empty  */
    if ( ( SBUF_STATUS_DATA_EMPTY != item->buf_status ) &&
         ( SBUF_STATUS_DATA_DONE != item->buf_status ) ) {
        LOG( LOG_ERR, "Invalid state: %d.", item->buf_status );
        return -EINVAL;
    }

    spin_lock_irqsave( &p_sbuf_mgr->sbuf_lock, irq_flags );

    LOG( LOG_DEBUG, "+++ sbuf arr info: buf_type: %s, buf_status: %s, read_idx: %u, write_idx: %u, status_count: %u-%u-%u-%u.",
         sbuf_type_str[item->buf_type],
         sbuf_status_str[item->buf_status],
         info->read_idx,
         info->write_idx,
         info->item_status_count[SBUF_STATUS_DATA_EMPTY],
         info->item_status_count[SBUF_STATUS_DATA_PREPARE],
         info->item_status_count[SBUF_STATUS_DATA_DONE],
         info->item_status_count[SBUF_STATUS_DATA_USING] );

    /* sb wants to set empty buffer after using? */
    if ( SBUF_STATUS_DATA_EMPTY == item->buf_status ) {
        /* The previous status of this buffer must be USING */
        if ( ( info->item_status_count[SBUF_STATUS_DATA_USING] == 1 ) &&
             ( arr[item->buf_idx].buf_status == SBUF_STATUS_DATA_USING ) ) {

            /* update array information  */
            arr[item->buf_idx].buf_status = SBUF_STATUS_DATA_EMPTY;
            info->item_status_count[SBUF_STATUS_DATA_EMPTY]++;
            info->item_status_count[SBUF_STATUS_DATA_USING]--;

            rc = 0;
        } else {
            LOG( LOG_ERR, "Failed to set empty sbuf, using count: %u, item buf_status: %u, buf_idx: %u.",
                 info->item_status_count[SBUF_STATUS_DATA_USING],
                 arr[item->buf_idx].buf_status,
                 item->buf_idx );
        }
    } else {
        /* sb wants to set data_done buffer after prepare */
        /*
         * only support one buffer to using the data, if there already
         * has a buffer in using, we should failed this request.
         */
        /* The previous status of this buffer must be PREPARE */
        if ( ( info->item_status_count[SBUF_STATUS_DATA_PREPARE] == 1 ) &&
             ( arr[item->buf_idx].buf_status == SBUF_STATUS_DATA_PREPARE ) ) {

            /* update array information  */
            arr[item->buf_idx].buf_status = SBUF_STATUS_DATA_DONE;
            info->item_status_count[SBUF_STATUS_DATA_DONE]++;
            info->item_status_count[SBUF_STATUS_DATA_PREPARE]--;

            /* set read_idx if it hasn't set properly */
            if ( info->read_idx == info->item_total_count ) {
                info->read_idx = item->buf_idx;
            }

            rc = 0;
        } else {
            LOG( LOG_ERR, "Failed to set done sbuf, prepare count: %u, item buf_status: %u, buf_idx: %u.",
                 info->item_status_count[SBUF_STATUS_DATA_PREPARE],
                 arr[item->buf_idx].buf_status,
                 item->buf_idx );
        }
    }

    LOG( LOG_DEBUG, "--- sbuf arr info: buf_type: %s, buf_status: %u, read_idx: %u, write_idx: %u, status_count: %u-%u-%u-%u.",
         sbuf_type_str[item->buf_type],
         item->buf_status,
         info->read_idx,
         info->write_idx,
         info->item_status_count[SBUF_STATUS_DATA_EMPTY],
         info->item_status_count[SBUF_STATUS_DATA_PREPARE],
         info->item_status_count[SBUF_STATUS_DATA_DONE],
         info->item_status_count[SBUF_STATUS_DATA_USING] );

    spin_unlock_irqrestore( &p_sbuf_mgr->sbuf_lock, irq_flags );

    if ( rc ) {
        LOG( LOG_ERR, "Set item failed: buf_idx: %u, buf_type: %s, buf_status: %s, buf_base: %p.", item->buf_idx, sbuf_type_str[item->buf_type], sbuf_status_str[item->buf_status], item->buf_base );
    } else {
        LOG( LOG_DEBUG, "Set item OK: buf_type: %s, buf_idx: %u, buf_status: %s, buf_base: %p.", item->buf_idx, sbuf_type_str[item->buf_type], sbuf_status_str[item->buf_status], item->buf_base );
    }

    return rc;
}

int sbuf_get_item( int fw_id, struct sbuf_item *item )
{
    int rc;
    struct sbuf_mgr *p_sbuf_mgr;

    if ( !item ) {
        LOG( LOG_ERR, "Error: Invaid param, item is NULL." );
        return -EINVAL;
    }

    if ( fw_id >= acamera_get_context_number() ) {
        LOG( LOG_ERR, "Error: Invaid param, fw_id: %d, max is: %d.", fw_id, acamera_get_context_number() - 1 );
        return -EINVAL;
    }

    LOG( LOG_DEBUG, "fw_id: %d.", fw_id );

    p_sbuf_mgr = &( sbuf_contexts[fw_id].sbuf_mgr );
    if ( !is_sbuf_inited( p_sbuf_mgr ) ) {
        LOG( LOG_ERR, "Error: sbuf is not inited, can't get_item." );
        return -ENOMEM;
    }

    switch ( item->buf_type ) {
#if defined( ISP_HAS_AE_MANUAL_FSM )
    case SBUF_TYPE_AE:
        rc = sbuf_get_item_from_arr( p_sbuf_mgr, item, p_sbuf_mgr->ae_sbuf_arr, &p_sbuf_mgr->ae_arr_info );
        break;
#endif

#if defined( ISP_HAS_AWB_MANUAL_FSM )
    case SBUF_TYPE_AWB:
        rc = sbuf_get_item_from_arr( p_sbuf_mgr, item, p_sbuf_mgr->awb_sbuf_arr, &p_sbuf_mgr->awb_arr_info );
        break;
#endif

#if defined( ISP_HAS_AF_MANUAL_FSM )
    case SBUF_TYPE_AF:
        rc = sbuf_get_item_from_arr( p_sbuf_mgr, item, p_sbuf_mgr->af_sbuf_arr, &p_sbuf_mgr->af_arr_info );
        break;
#endif

#if defined( ISP_HAS_GAMMA_MANUAL_FSM )
    case SBUF_TYPE_GAMMA:
        rc = sbuf_get_item_from_arr( p_sbuf_mgr, item, p_sbuf_mgr->gamma_sbuf_arr, &p_sbuf_mgr->gamma_arr_info );
        break;
#endif

#if defined( ISP_HAS_IRIDIX_MANUAL_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM )
    case SBUF_TYPE_IRIDIX:
        rc = sbuf_get_item_from_arr( p_sbuf_mgr, item, p_sbuf_mgr->iridix_sbuf_arr, &p_sbuf_mgr->iridix_arr_info );
        break;
#endif
    default:
        LOG( LOG_ERR, "Error: Unsupported buf_type: %d.", item->buf_type );
        rc = -EINVAL;
        break;
    }

    return rc;
}

int sbuf_set_item( int fw_id, struct sbuf_item *item )
{
    int rc;
    struct sbuf_mgr *p_sbuf_mgr;

    if ( !item ) {
        LOG( LOG_ERR, "Error: Invaid param, item is NULL." );
        return -EINVAL;
    }

    if ( fw_id >= acamera_get_context_number() ) {
        LOG( LOG_ERR, "Error: Invaid param, fw_id: %d, max is: %d.", fw_id, acamera_get_context_number() - 1 );
        return -EINVAL;
    }

    LOG( LOG_DEBUG, "fw_id: %d.", fw_id );

    p_sbuf_mgr = &( sbuf_contexts[fw_id].sbuf_mgr );
    if ( !is_sbuf_inited( p_sbuf_mgr ) ) {
        LOG( LOG_ERR, "Error: sbuf is not inited, can't set_item." );
        return -ENOMEM;
    }

    switch ( item->buf_type ) {
#if defined( ISP_HAS_AE_MANUAL_FSM )
    case SBUF_TYPE_AE:
        rc = sbuf_set_item_to_arr( p_sbuf_mgr, item, p_sbuf_mgr->ae_sbuf_arr, &p_sbuf_mgr->ae_arr_info );
        break;
#endif

#if defined( ISP_HAS_AWB_MANUAL_FSM )
    case SBUF_TYPE_AWB:
        rc = sbuf_set_item_to_arr( p_sbuf_mgr, item, p_sbuf_mgr->awb_sbuf_arr, &p_sbuf_mgr->awb_arr_info );
        break;
#endif

#if defined( ISP_HAS_AF_MANUAL_FSM )
    case SBUF_TYPE_AF:
        rc = sbuf_set_item_to_arr( p_sbuf_mgr, item, p_sbuf_mgr->af_sbuf_arr, &p_sbuf_mgr->af_arr_info );
        break;
#endif

#if defined( ISP_HAS_GAMMA_MANUAL_FSM )
    case SBUF_TYPE_GAMMA:
        rc = sbuf_set_item_to_arr( p_sbuf_mgr, item, p_sbuf_mgr->gamma_sbuf_arr, &p_sbuf_mgr->gamma_arr_info );
        break;
#endif

#if defined( ISP_HAS_IRIDIX_MANUAL_FSM ) || defined( ISP_HAS_IRIDIX8_MANUAL_FSM )
    case SBUF_TYPE_IRIDIX:
        rc = sbuf_set_item_to_arr( p_sbuf_mgr, item, p_sbuf_mgr->iridix_sbuf_arr, &p_sbuf_mgr->iridix_arr_info );
        break;
#endif
    default:
        LOG( LOG_ERR, "Error: Unsupported buf_type: %d.", item->buf_type );
        rc = -EINVAL;
        break;
    }

    return rc;
}

static int sbuf_fops_open( struct inode *inode, struct file *f )
{
    int rc;
    int i;
    struct sbuf_context *p_ctx = NULL;
    int minor = iminor( inode );

    for ( i = 0; i < acamera_get_context_number(); i++ ) {
        if ( sbuf_contexts[i].dev_minor_id == minor ) {
            p_ctx = &sbuf_contexts[i];
            break;
        }
    }

    if ( !p_ctx ) {
        LOG( LOG_CRIT, "Fatal error, sbuf contexts is crashed, contents dump:" );
        for ( i = 0; i < acamera_get_context_number(); i++ ) {
            p_ctx = &sbuf_contexts[i];
            LOG( LOG_CRIT, "%d): fw_id: %d, minor_id: %d, name: %s, p_fsm: %p.",
                 i, p_ctx->fw_id, p_ctx->dev_minor_id, p_ctx->dev_name, p_ctx->p_fsm );
        }
        return -ERESTARTSYS;
    }

    rc = mutex_lock_interruptible( &p_ctx->fops_lock );
    if ( rc ) {
        LOG( LOG_ERR, "access lock failed, rc: %d.", rc );
        return rc;
    }

    if ( p_ctx->dev_opened ) {
        LOG( LOG_ERR, "open failed, already opened." );
        rc = -EBUSY;
    } else {
        p_ctx->dev_opened = 1;
        rc = 0;
        f->private_data = p_ctx;
    }

    mutex_unlock( &p_ctx->fops_lock );

    return rc;
}

static int sbuf_fops_release( struct inode *inode, struct file *f )
{
    int rc = 0;
    struct sbuf_context *p_ctx = (struct sbuf_context *)f->private_data;

    rc = mutex_lock_interruptible( &p_ctx->fops_lock );
    if ( rc ) {
        LOG( LOG_ERR, "Error: lock failed." );
        return rc;
    }

    if ( p_ctx->dev_opened ) {
        p_ctx->dev_opened = 0;
        f->private_data = NULL;
        sbuf_mgr_reset( &p_ctx->sbuf_mgr );
    } else {
        LOG( LOG_CRIT, "Fatal error: wrong state dev_opened: %d.", p_ctx->dev_opened );
        rc = -EINVAL;
    }

    mutex_unlock( &p_ctx->fops_lock );

    return 0;
}

static ssize_t sbuf_fops_write( struct file *file, const char __user *buf, size_t count, loff_t *ppos )
{
    int rc = 0;
    struct sbuf_context *p_ctx = (struct sbuf_context *)file->private_data;
    struct sbuf_idx_set idx_set = {0};
    uint32_t len_to_copy = sizeof( struct sbuf_idx_set );

    LOG( LOG_DEBUG, "p_ctx: %p, name: %s, fw_id: %d, minor_id: %d.", p_ctx, p_ctx->dev_name, p_ctx->fw_id, p_ctx->dev_minor_id );

    if ( count != len_to_copy ) {
        LOG( LOG_ERR, "write size mismatch, size: %u, expected: %d.", (uint32_t)count, len_to_copy );
        return -EINVAL;
    }

    rc = copy_from_user( &idx_set, buf, len_to_copy );
    if ( rc ) {
        LOG( LOG_ERR, "copy_from_user failed, not copied: %d, expected: %u.", rc, len_to_copy );
    }

    LOG( LOG_DEBUG, "ctx: %d, write idx_set: %u(%u)-%u(%u)-%u(%u)-%u(%u)-%u(%u).",
         p_ctx->fw_id,
         idx_set.ae_idx_valid, idx_set.ae_idx,
         idx_set.awb_idx_valid, idx_set.awb_idx,
         idx_set.af_idx_valid, idx_set.af_idx,
         idx_set.gamma_idx_valid, idx_set.gamma_idx,
         idx_set.iridix_idx_valid, idx_set.iridix_idx );

    sbuf_mgr_apply_new_param( p_ctx, &idx_set );

    return rc ? rc : len_to_copy;
}

static ssize_t sbuf_fops_read( struct file *file, char __user *buf, size_t count, loff_t *ppos )
{
    int rc;
    int32_t max_exposure_log2 = 0;
    struct sbuf_context *p_ctx = (struct sbuf_context *)file->private_data;
    struct sbuf_idx_set idx_set;
    uint32_t len_to_copy = sizeof( struct sbuf_idx_set );

    if ( count != len_to_copy ) {
        LOG( LOG_ERR, "read size mismatch, size: %u, expected: %d.", (uint32_t)count, len_to_copy );
        return -EINVAL;
    }

    if ( !sbuf_is_ready_to_send_data( p_ctx ) ) {
        LOG( LOG_INFO, "Not ready to send data." );
        return -ENODATA;
    }

    /* Get latest sbuf index set, it will wait if no data availabe */
    sbuf_mgr_get_latest_idx_set( p_ctx, &idx_set );

    // 2nd Check because sbuf_mgr_get_latest_idx_set() will wait for data available.
    if ( !sbuf_is_ready_to_send_data( p_ctx ) ) {
        // recycle items to sbuf_mgr
        sbuf_recycle_idx_set( p_ctx, &idx_set );

        LOG( LOG_INFO, "Not ready to send data." );
        return -ENODATA;
    }

    rc = copy_to_user( buf, &idx_set, len_to_copy );
    if ( rc ) {
        LOG( LOG_ERR, "copy_to_user failed, rc: %d.", rc );
    }

    LOG( LOG_DEBUG, "ctx: %d, read idx_set: %u(%u)-%u(%u)-%u(%u)-%u(%u)-%u(%u).",
         p_ctx->fw_id,
         idx_set.ae_idx_valid, idx_set.ae_idx,
         idx_set.awb_idx_valid, idx_set.awb_idx,
         idx_set.af_idx_valid, idx_set.af_idx,
         idx_set.gamma_idx_valid, idx_set.gamma_idx,
         idx_set.iridix_idx_valid, idx_set.iridix_idx );

    int32_t type = CMOS_MAX_EXPOSURE_LOG2;
    acamera_fsm_mgr_get_param( p_ctx->p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_EXPOSURE_LOG2, &type, sizeof( type ), &max_exposure_log2, sizeof( max_exposure_log2 ) );
    p_ctx->sbuf_mgr.sbuf_base->kf_info.cmos_info.max_exposure_log2 = max_exposure_log2;
    LOG( LOG_DEBUG, "fw_id: %d, set cmos_info max_exposure_log2: %d.", p_ctx->fw_id, max_exposure_log2 );

    int32_t total_gain_log2 = 0;
    acamera_fsm_mgr_get_param( p_ctx->p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_TOTAL_GAIN, NULL, 0, &total_gain_log2, sizeof( total_gain_log2 ) );
    p_ctx->sbuf_mgr.sbuf_base->kf_info.cmos_info.total_gain_log2 = total_gain_log2;

    return rc ? rc : len_to_copy;
}


static int sbuf_fops_mmap( struct file *file, struct vm_area_struct *vma )
{
    unsigned long user_buf_len = vma->vm_end - vma->vm_start;
    int rc;
    int32_t max_exposure_log2 = 0;
    struct sbuf_context *p_ctx = (struct sbuf_context *)file->private_data;
    struct sbuf_mgr *p_sbuf_mgr = &p_ctx->sbuf_mgr;

    /*
     * the user_buf_len will be page aligned even struct fw_sbuf is not
     * page aligned, in this case, the size maybe unmatched, but the
     * delta should not exceed 1 page
     */
    if ( ( user_buf_len != sizeof( struct fw_sbuf ) ) &&
         ( user_buf_len - sizeof( struct fw_sbuf ) >= PAGE_SIZE ) ) {
        LOG( LOG_CRIT, "Not matched buf size, User app size: %ld, kernel sbuf size: %zu.", user_buf_len, sizeof( struct fw_sbuf ) );
        return -EINVAL;
    }

    if ( !is_sbuf_inited( p_sbuf_mgr ) ) {
        LOG( LOG_ERR, "Error: sbuf is not inited, can't map." );
        return -ENOMEM;
    }

    /* remap the kernel buffer into the user app address space. */
    rc = remap_pfn_range( vma, vma->vm_start, virt_to_phys( p_sbuf_mgr->buf_used ) >> PAGE_SHIFT, user_buf_len, vma->vm_page_prot );
    if ( rc < 0 ) {
        LOG( LOG_ERR, "remap of sbuf failed, return: %d.", rc );
        return rc;
    }

    /* prepare cmos_info */
    int32_t type = CMOS_MAX_EXPOSURE_LOG2;
    acamera_fsm_mgr_get_param( p_ctx->p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_CMOS_EXPOSURE_LOG2, &type, sizeof( type ), &max_exposure_log2, sizeof( max_exposure_log2 ) );
    p_sbuf_mgr->sbuf_base->kf_info.cmos_info.max_exposure_log2 = max_exposure_log2;
    LOG( LOG_INFO, "fw_id: %d, set cmos_info max_exposure_log2: %d.", p_ctx->fw_id, max_exposure_log2 );

    /* prepare lens_info */
    int32_t lens_status = 0;
    acamera_fsm_mgr_get_param( p_ctx->p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_AF_LENS_STATUS, NULL, 0, &lens_status, sizeof( lens_status ) );
    p_sbuf_mgr->sbuf_base->kf_info.af_info.lens_driver_ok = lens_status;
    LOG( LOG_INFO, "fw_id: %d, set af_info lens_driver_ok: %d.", p_ctx->fw_id, lens_status );

    if ( lens_status ) {
        lens_param_t lens_param;
        acamera_fsm_mgr_get_param( p_ctx->p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_LENS_PARAM, NULL, 0, &lens_param, sizeof( lens_param ) );
        p_sbuf_mgr->sbuf_base->kf_info.af_info.lens_param = lens_param;
    }

    /* prepare sensor_info */
    const sensor_param_t *param = NULL;
    acamera_fsm_mgr_get_param( p_ctx->p_fsm->cmn.p_fsm_mgr, FSM_PARAM_GET_SENSOR_PARAM, NULL, 0, &param, sizeof( param ) );

    if ( param ) {
        uint32_t idx = 0;
        uint32_t valid_modes_num = 0;
        valid_modes_num = param->modes_num;

        if ( valid_modes_num > ISP_MAX_SENSOR_MODES ) {
            valid_modes_num = ISP_MAX_SENSOR_MODES;
        }

        for ( idx = 0; idx < valid_modes_num; idx++ ) {
            p_sbuf_mgr->sbuf_base->kf_info.sensor_info.modes[idx] = param->modes_table[idx];

            LOG( LOG_DEBUG, "Sensor_mode[%d]: wdr_mode: %d, exp: %d.", idx,
                 p_sbuf_mgr->sbuf_base->kf_info.sensor_info.modes[idx].wdr_mode,
                 p_sbuf_mgr->sbuf_base->kf_info.sensor_info.modes[idx].exposures );
        }

        p_sbuf_mgr->sbuf_base->kf_info.sensor_info.cur_mode = param->mode;
        p_sbuf_mgr->sbuf_base->kf_info.sensor_info.modes_num = valid_modes_num;
    }

    return 0;
}

static struct file_operations sbuf_mgr_fops = {
    .owner = THIS_MODULE,
    .open = sbuf_fops_open,
    .release = sbuf_fops_release,
    .read = sbuf_fops_read,
    .write = sbuf_fops_write,
    .llseek = noop_llseek,
    .mmap = sbuf_fops_mmap,
};

void sbuf_fsm_initialize( sbuf_fsm_t *p_fsm )
{
    int rc;
    uint32_t fw_id = p_fsm->cmn.ctx_id;
    struct miscdevice *p_dev;
    struct sbuf_context *p_ctx;

    if ( fw_id >= acamera_get_context_number() ) {
        LOG( LOG_CRIT, "Fatal error: Invalid FW context ID: %d, max is: %d", fw_id, acamera_get_context_number() - 1 );
        return;
    }

    p_ctx = &( sbuf_contexts[fw_id] );
    memset( p_ctx, 0, sizeof( *p_ctx ) );
    p_dev = &p_ctx->sbuf_dev;
    snprintf( p_ctx->dev_name, SBUF_DEV_NAME_LEN, SBUF_DEV_FORMAT, fw_id );
    p_dev->name = p_ctx->dev_name;
    p_dev->minor = MISC_DYNAMIC_MINOR,
    p_dev->fops = &sbuf_mgr_fops,

    rc = misc_register( p_dev );
    if ( rc ) {
        LOG( LOG_ERR, "init failed, error: register sbuf device failed, ret: %d.", rc );
        return;
    }

    p_ctx->fw_id = fw_id;
    p_ctx->dev_minor_id = p_dev->minor;
    p_ctx->p_fsm = p_fsm;
    p_ctx->dev_opened = 0;

    rc = sbuf_ctx_init( p_ctx );
    if ( rc ) {
        LOG( LOG_ERR, "init failed, , ret: %d.", rc );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0))
        misc_deregister( p_dev );
#else
        rc = misc_deregister( p_dev );
        if ( rc ) {
            LOG( LOG_ERR, "deregister sbuf dev '%s' failed, ret: %d.", p_dev->name, rc );
        } else {
            LOG( LOG_INFO, "deregister sbuf dev '%s' ok.", p_dev->name );
        }
#endif
        p_dev->name = NULL;

        return;
    }

    mutex_init( &p_ctx->fops_lock );
    mutex_init( &p_ctx->idx_set_lock );
    init_waitqueue_head( &p_ctx->idx_set_wait_queue );

    return;
}

void sbuf_deinit( sbuf_fsm_ptr_t p_fsm )
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 3, 0))
    int rc;
#endif
    uint32_t fw_id = p_fsm->cmn.ctx_id;
    struct sbuf_context *p_ctx = NULL;
    struct miscdevice *p_dev = NULL;

    p_ctx = &( sbuf_contexts[fw_id] );
    if ( p_ctx->fw_id != fw_id ) {
        LOG( LOG_CRIT, "Error: ctx_id not match, fsm fw_id: %d, ctx_id: %d.", fw_id, p_ctx->fw_id );
        return;
    }

    sbuf_mgr_free( &p_ctx->sbuf_mgr );

    p_dev = &p_ctx->sbuf_dev;
    if ( !p_dev->name ) {
        LOG( LOG_CRIT, "skip sbuf[%d] deregister due to NULL name", fw_id );
        return;
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0))
    misc_deregister( p_dev );
#else
    rc = misc_deregister( p_dev );
    if ( rc ) {
        LOG( LOG_ERR, "deregister sbuf dev '%s' failed, ret: %d.", p_dev->name, rc );
    } else {
        LOG( LOG_INFO, "deregister sbuf dev '%s' ok.", p_dev->name );
    }
#endif
}
