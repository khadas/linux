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

#include <linux/kernel.h> /* //printk() */
#include <asm/uaccess.h>
#include <linux/gfp.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <asm/types.h>
#include <asm/io.h>
#include <linux/time.h>
#include "acamera_types.h"
#include "acamera_logger.h"
#include "system_am_dma.h"
#include "system_dma.h"

#define SYSTEM_DMA_TOGGLE_COUNT 2
#define SYSTEM_DMA_MAX_CHANNEL 2

#if FW_USE_SYSTEM_DMA

#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <asm/dma-mapping.h>
#include <asm/cacheflush.h>

typedef struct {
    //in case scatter and gather is not supported, need more than one channel
    struct dma_chan *dma_channel[SYSTEM_DMA_MAX_CHANNEL];
    //for sg
    struct sg_table sg_device_table[FIRMWARE_CONTEXT_NUMBER][SYSTEM_DMA_TOGGLE_COUNT];
    unsigned int sg_device_nents[FIRMWARE_CONTEXT_NUMBER][SYSTEM_DMA_TOGGLE_COUNT];

    struct sg_table sg_fwmem_table[FIRMWARE_CONTEXT_NUMBER][SYSTEM_DMA_TOGGLE_COUNT];
    unsigned int sg_fwmem_nents[FIRMWARE_CONTEXT_NUMBER][SYSTEM_DMA_TOGGLE_COUNT];

    //for flushing
    fwmem_addr_pair_t *fwmem_pair_flush[FIRMWARE_CONTEXT_NUMBER][SYSTEM_DMA_TOGGLE_COUNT];
    //for callback unmapping
    int32_t buff_loc;
    uint32_t direction;
    uint32_t cur_fw_ctx_id;

    //conf synchronization and completion
    dma_completion_callback complete_func;
    atomic_t nents_done;
    struct completion comp;

} system_dma_device_t;

#else

#include <linux/interrupt.h>

#if FW_USE_AML_DMA
typedef struct {
    unsigned long dev_addr;
    unsigned long fw_addr;
    size_t size;
    void *sys_back_ptr;
} mem_addr_pair_t;
extern void cache_flush(uint32_t buf_start, uint32_t buf_size);
extern void cache_flush_for_device(uint32_t buf_start, uint32_t buf_size);
#else
typedef struct {
    void __iomem *dev_addr;
    void *fw_addr;
    size_t size;
    void *sys_back_ptr;
} mem_addr_pair_t;
#endif

typedef struct {
    struct tasklet_struct m_task;
    mem_addr_pair_t *mem_data;
} mem_tasklet_t;


typedef struct {
    char *name;
    unsigned int sg_device_nents[FIRMWARE_CONTEXT_NUMBER][SYSTEM_DMA_TOGGLE_COUNT];

    unsigned int sg_fwmem_nents[FIRMWARE_CONTEXT_NUMBER][SYSTEM_DMA_TOGGLE_COUNT];

    mem_addr_pair_t *mem_addrs[FIRMWARE_CONTEXT_NUMBER][SYSTEM_DMA_TOGGLE_COUNT];

    mem_tasklet_t task_list[FIRMWARE_CONTEXT_NUMBER][SYSTEM_DMA_TOGGLE_COUNT][SYSTEM_DMA_MAX_CHANNEL];

    int32_t buff_loc;
    uint32_t direction;
    uint32_t cur_fw_ctx_id;

    //conf synchronization and completion
    dma_completion_callback complete_func;
    atomic_t nents_done;
    struct completion comp;

} system_dma_device_t;

#endif


int32_t system_dma_init( void **ctx )
{
    int32_t i, result = 0;
    int32_t idx;

    if ( ctx != NULL ) {

        *ctx = system_malloc( sizeof( system_dma_device_t ) );
        system_dma_device_t *system_dma_device = (system_dma_device_t *)*ctx;

        if ( !( *ctx ) ) {
            LOG( LOG_CRIT, "No memory for ctx" );
            return -1;
        }

#if FW_USE_SYSTEM_DMA
        struct dma_chan *dma_channel = NULL;
        dma_cap_mask_t mask;
        dma_cap_zero( mask );
        dma_cap_set( DMA_MEMCPY, mask );

        for ( i = 0; i < SYSTEM_DMA_MAX_CHANNEL; i++ ) {
            dma_channel = dma_request_channel( mask, 0, NULL );
            LOG( LOG_INFO, "allocating dma" );
            if ( dma_channel != NULL ) {
                system_dma_device->dma_channel[i] = dma_channel;
            } else {
                kfree( *ctx );
                LOG( LOG_CRIT, "Failed to request DMA channel" );
                return -1;
            }
        }

        for ( idx = 0; idx < FIRMWARE_CONTEXT_NUMBER; idx++ ) {
            for ( i = 0; i < SYSTEM_DMA_TOGGLE_COUNT; i++ ) {
                system_dma_device->sg_device_nents[idx][i] = 0;
                system_dma_device->sg_fwmem_nents[idx][i] = 0;
            }
        }

#else
        system_dma_device->name = "TSK_DMA";
        for ( idx = 0; idx < FIRMWARE_CONTEXT_NUMBER; idx++ ) {
            for ( i = 0; i < SYSTEM_DMA_TOGGLE_COUNT; i++ ) {
                system_dma_device->sg_device_nents[idx][i] = 0;
                system_dma_device->sg_fwmem_nents[idx][i] = 0;
                system_dma_device->mem_addrs[idx][i] = 0;
            }
        }
#endif
    } else {
        result = -1;
        LOG( LOG_CRIT, "Input ctx pointer is NULL" );
    }

#if FW_USE_AML_DMA
    am_dma_init();
#endif

    return result;
}


int32_t system_dma_destroy( void *ctx )
{
    int32_t i, result = 0;
    int32_t idx;

    if ( ctx != 0 ) {
        system_dma_device_t *system_dma_device = (system_dma_device_t *)ctx;

#if FW_USE_AML_DMA
        am_dma_deinit();
#endif

#if FW_USE_SYSTEM_DMA
        for ( i = 0; i < SYSTEM_DMA_MAX_CHANNEL; i++ ) {
            dma_release_channel( system_dma_device->dma_channel[i] );
        }

        for ( idx = 0; idx < FIRMWARE_CONTEXT_NUMBER; idx++ ) {
            for ( i = 0; i < SYSTEM_DMA_TOGGLE_COUNT; i++ ) {
                if ( system_dma_device->sg_device_nents[idx][i] )
                    sg_free_table( &system_dma_device->sg_device_table[idx][i] );

                if ( system_dma_device->sg_fwmem_nents[idx][i] )
                    sg_free_table( &system_dma_device->sg_device_table[idx][i] );

                if ( system_dma_device->fwmem_pair_flush[idx][i] )
                    kfree( system_dma_device->fwmem_pair_flush[idx][i] );
            }
        }

#elif FW_USE_AML_DMA
        for ( idx = 0; idx < FIRMWARE_CONTEXT_NUMBER; idx++ ) {
            for ( i = 0; i < SYSTEM_DMA_TOGGLE_COUNT; i++ ) {
                if ( system_dma_device->mem_addrs[idx][i] ) {
                    kfree( system_dma_device->mem_addrs[idx][i] );
                }
            }
        }

#else
        for ( idx = 0; idx < FIRMWARE_CONTEXT_NUMBER; idx++ ) {
            for ( i = 0; i < SYSTEM_DMA_TOGGLE_COUNT; i++ ) {
                if ( system_dma_device->mem_addrs[idx][i] ) {
                    int j;
                    for ( j = 0; j < system_dma_device->sg_device_nents[idx][i]; j++ )
                        iounmap( system_dma_device->mem_addrs[idx][i][j].dev_addr );
                    kfree( system_dma_device->mem_addrs[idx][i] );
                }
            }
        }
#endif

        kfree( ctx );

    } else {
        LOG( LOG_CRIT, "Input ctx pointer is NULL" );
    }
    return result;
}

static void dma_complete_func( void *ctx )
{
    LOG( LOG_DEBUG, "\nIRQ completion called" );
    system_dma_device_t *system_dma_device = (system_dma_device_t *)ctx;

    unsigned int nents_done = atomic_inc_return( &system_dma_device->nents_done );
    if ( nents_done >= system_dma_device->sg_device_nents[system_dma_device->cur_fw_ctx_id][system_dma_device->buff_loc] ) {
        if ( system_dma_device->complete_func ) {
            system_dma_device->complete_func( ctx );
            LOG( LOG_DEBUG, "async completed on buff:%d dir:%d", system_dma_device->buff_loc, system_dma_device->direction );
        } else {
            complete( &system_dma_device->comp );
            LOG( LOG_DEBUG, "sync completed on buff:%d dir:%d", system_dma_device->buff_loc, system_dma_device->direction );
        }
    }
}

#if FW_USE_SYSTEM_DMA
//sg from here
int32_t system_dma_sg_device_setup( void *ctx, int32_t buff_loc, dma_addr_pair_t *device_addr_pair, int32_t addr_pairs, uint32_t fw_ctx_id )
{
    system_dma_device_t *system_dma_device = (system_dma_device_t *)ctx;
    struct scatterlist *sg;
    int i, ret;

    if ( !system_dma_device || !device_addr_pair || !addr_pairs || buff_loc >= SYSTEM_DMA_TOGGLE_COUNT || fw_ctx_id >= FIRMWARE_CONTEXT_NUMBER )
        return -1;

    struct sg_table *table = &system_dma_device->sg_device_table[fw_ctx_id][buff_loc];

    /* Allocate the scatterlist table */
    ret = sg_alloc_table( table, addr_pairs, GFP_KERNEL );
    if ( ret ) {
        LOG( LOG_CRIT, "unable to allocate DMA table\n" );
        return ret;
    }
    system_dma_device->sg_device_nents[fw_ctx_id][buff_loc] = addr_pairs;

    /* Add the DATA FPGA registers to the scatterlist */
    sg = table->sgl;
    for ( i = 0; i < addr_pairs; i++ ) {
        sg_dma_address( sg ) = device_addr_pair[i].address;
        sg_dma_len( sg ) = device_addr_pair[i].size;
        sg = sg_next( sg );
    }
    LOG( LOG_INFO, "dma device setup success %d", system_dma_device->sg_device_nents[fw_ctx_id][buff_loc] );
    return 0;
}

int32_t system_dma_sg_fwmem_setup( void *ctx, int32_t buff_loc, fwmem_addr_pair_t *fwmem_pair, int32_t addr_pairs, uint32_t fw_ctx_id )
{
    //unsigned int nr_pages;
    int i, ret;
    struct scatterlist *sg;
    system_dma_device_t *system_dma_device = (system_dma_device_t *)ctx;

    if ( !system_dma_device || !fwmem_pair || !addr_pairs || buff_loc >= SYSTEM_DMA_TOGGLE_COUNT || fw_ctx_id >= FIRMWARE_CONTEXT_NUMBER ) {
        LOG( LOG_CRIT, "null param problems" );
        return -1;
    }

    struct sg_table *table = &system_dma_device->sg_fwmem_table[fw_ctx_id][buff_loc];
    /* Allocate the scatterlist table */
    ret = sg_alloc_table( table, addr_pairs, GFP_KERNEL );
    if ( ret ) {
        LOG( LOG_CRIT, "unable to allocate DMA table\n" );
        return ret;
    }
    system_dma_device->sg_fwmem_nents[fw_ctx_id][buff_loc] = addr_pairs;
    system_dma_device->fwmem_pair_flush[fw_ctx_id][buff_loc] = kmalloc( sizeof( fwmem_addr_pair_t ) * addr_pairs, GFP_KERNEL );
    if ( !system_dma_device->fwmem_pair_flush[fw_ctx_id][buff_loc] ) {
        LOG( LOG_CRIT, "Failed to allocate virtual address pairs for flushing!!" );
        return -1;
    }
    for ( i = 0; i < addr_pairs; i++ ) {
        system_dma_device->fwmem_pair_flush[fw_ctx_id][buff_loc][i] = fwmem_pair[i];
    }
    sg = table->sgl;
    for ( i = 0; i < addr_pairs; i++ ) {
        sg_set_buf( sg, fwmem_pair[i].address, fwmem_pair[i].size );
        sg = sg_next( sg );
    }

    LOG( LOG_INFO, "fwmem setup success %d", system_dma_device->sg_device_nents[fw_ctx_id][buff_loc] );

    return 0;
}

void system_dma_unmap_sg( void *ctx )
{
    if ( !ctx )
        return;
    system_dma_device_t *system_dma_device = (system_dma_device_t *)ctx;
    int32_t buff_loc = system_dma_device->buff_loc;
    uint32_t direction = system_dma_device->direction;
    uint32_t cur_fw_ctx_id = system_dma_device->cur_fw_ctx_id;
    int i;
    for ( i = 0; i < SYSTEM_DMA_MAX_CHANNEL; i++ ) {
        struct dma_chan *chan = system_dma_device->dma_channel[i];
        dma_unmap_sg( chan->device->dev, system_dma_device->sg_fwmem_table[cur_fw_ctx_id][buff_loc].sgl, system_dma_device->sg_fwmem_nents[cur_fw_ctx_id][buff_loc], direction );
    }
}

int32_t system_dma_copy_sg( void *ctx, int32_t buff_loc, uint32_t direction, dma_completion_callback complete_func, uint32_t fw_ctx_id )
{
    int32_t i, result = 0;
    if ( !ctx ) {
        LOG( LOG_ERR, "Input ctx pointer is NULL" );
        return -1;
    }

    int32_t async_dma = 0;

    if ( complete_func != NULL ) {
        async_dma = 1;
    }

    system_dma_device_t *system_dma_device = (system_dma_device_t *)ctx;

    struct scatterlist *dst_sg, *src_sg;
    unsigned int dst_nents, src_nents;
    struct dma_chan *chan = system_dma_device->dma_channel[0]; //probe the first channel
    struct dma_async_tx_descriptor *tx = NULL;
    dma_cookie_t cookie;
    enum dma_ctrl_flags flags = DMA_CTRL_ACK | DMA_PREP_FENCE | DMA_PREP_INTERRUPT;

    if ( direction == SYS_DMA_TO_DEVICE ) {
        dst_sg = system_dma_device->sg_device_table[fw_ctx_id][buff_loc].sgl;
        dst_nents = system_dma_device->sg_device_nents[fw_ctx_id][buff_loc];
        src_sg = system_dma_device->sg_fwmem_table[fw_ctx_id][buff_loc].sgl;
        src_nents = system_dma_device->sg_fwmem_nents[fw_ctx_id][buff_loc];
        direction = DMA_TO_DEVICE;
    } else {
        src_sg = system_dma_device->sg_device_table[fw_ctx_id][buff_loc].sgl;
        src_nents = system_dma_device->sg_device_nents[fw_ctx_id][buff_loc];
        dst_sg = system_dma_device->sg_fwmem_table[fw_ctx_id][buff_loc].sgl;
        dst_nents = system_dma_device->sg_fwmem_nents[fw_ctx_id][buff_loc];
        direction = DMA_FROM_DEVICE;
    }

    if ( src_nents != dst_nents || !src_nents || !dst_nents ) {
        LOG( LOG_CRIT, "Unbalance src_nents:%d dst_nents:%d", src_nents, dst_nents );
        return -1;
    }

    //flush memory before transfer
    for ( i = 0; i < src_nents; i++ ) {
        flush_icache_range( (unsigned long)( system_dma_device->fwmem_pair_flush[fw_ctx_id][buff_loc][i].address ), (unsigned long)( (uintptr_t)system_dma_device->fwmem_pair_flush[fw_ctx_id][buff_loc][i].address + system_dma_device->fwmem_pair_flush[fw_ctx_id][buff_loc][i].size ) );
    }

    for ( i = 0; i < SYSTEM_DMA_MAX_CHANNEL; i++ ) {
        chan = system_dma_device->dma_channel[i];

        result = dma_map_sg( chan->device->dev, system_dma_device->sg_fwmem_table[fw_ctx_id][buff_loc].sgl, system_dma_device->sg_fwmem_nents[fw_ctx_id][buff_loc], direction );
        if ( result <= 0 ) {
            LOG( LOG_CRIT, "unable to map %d", result );
            return -1;
        }
        LOG( LOG_DEBUG, "src_nents:%d src_sg:%p dst_nents:%d dst_sg:%p dma map res:%d", src_nents, src_sg, dst_nents, dst_sg, result );
    }
    system_dma_device->cur_fw_ctx_id = fw_ctx_id;
    /*
     * All buffers passed to this function should be ready and mapped
     * for DMA already. Therefore, we don't need to do anything except
     * submit it to the Freescale DMA Engine for processing
     */
    atomic_set( &system_dma_device->nents_done, 0 ); //set the number of nents done
    if ( async_dma == 0 ) {
        system_dma_device->complete_func = NULL; //async mode is not allowed to have callback
        init_completion( &system_dma_device->comp );
    } else {
        system_dma_device->complete_func = complete_func; //call this function if all nents are done;
    }
    system_dma_device->direction = direction;
    system_dma_device->buff_loc = buff_loc;

    if ( !chan->device->device_prep_dma_sg ) {
        LOG( LOG_DEBUG, "missing device_prep_dma_sg %p %p", chan->device->device_prep_dma_sg, chan->device->device_prep_interleaved_dma );

        for ( i = 0; i < src_nents; i++ ) {

            struct dma_chan *chan = system_dma_device->dma_channel[i];

            uint32_t dst = sg_dma_address( dst_sg );
            uint32_t src = sg_dma_address( src_sg );
            uint32_t size_to_copy = sg_dma_len( src_sg );
            LOG( LOG_DEBUG, "src:0x%x (%d) to dst:0x%x (%d)", src, size_to_copy, dst, sg_dma_len( dst_sg ) );

            tx = chan->device->device_prep_dma_memcpy( chan, dst, src, size_to_copy, flags );
            if ( tx ) {
                tx->callback = dma_complete_func;
                tx->callback_param = ctx;
                cookie = tx->tx_submit( tx );
                if ( dma_submit_error( cookie ) ) {
                    LOG( LOG_CRIT, "unable to submit scatterlist DMA\n" );
                    return -ENOMEM;
                }
            } else {
                LOG( LOG_CRIT, "unable to prep scatterlist DMA\n" );
                return -ENOMEM;
            }
            dma_async_issue_pending( chan );
            //}
            dst_sg = sg_next( dst_sg );
            src_sg = sg_next( src_sg );
        }

    } else {
        chan = system_dma_device->dma_channel[0];
        /* setup the scatterlist to scatterlist transfer */
        tx = chan->device->device_prep_dma_sg( chan,
                                               dst_sg, dst_nents,
                                               src_sg, src_nents,
                                               0 );
        if ( tx ) {
            tx->callback = dma_complete_func;
            tx->callback_param = ctx;
            cookie = tx->tx_submit( tx );
            if ( dma_submit_error( cookie ) ) {
                LOG( LOG_CRIT, "unable to submit scatterlist DMA\n" );
                return -ENOMEM;
            }
        } else {
            LOG( LOG_CRIT, "unable to prep scatterlist DMA\n" );
            return -ENOMEM;
        }

        atomic_set( &system_dma_device->nents_done, dst_nents - 1 ); //only need to issue once
        dma_async_issue_pending( chan );
    }


    if ( async_dma == 0 ) {
        LOG( LOG_DEBUG, "scatterlist DMA waiting completion\n" );
        wait_for_completion( &system_dma_device->comp );
        for ( i = 0; i < SYSTEM_DMA_MAX_CHANNEL; i++ ) {
            chan = system_dma_device->dma_channel[i];
            dma_unmap_sg( chan->device->dev, system_dma_device->sg_fwmem_table[fw_ctx_id][buff_loc].sgl, system_dma_device->sg_fwmem_nents[fw_ctx_id][buff_loc], direction );
        }
    }

    LOG( LOG_DEBUG, "scatterlist DMA success\n" );
    return result;
}

#elif FW_USE_AML_DMA
int32_t system_dma_sg_device_setup( void *ctx, int32_t buff_loc, dma_addr_pair_t *device_addr_pair, int32_t addr_pairs, uint32_t fw_ctx_id )
{
    system_dma_device_t *system_dma_device = (system_dma_device_t *)ctx;
    int i;

    if ( !system_dma_device || !device_addr_pair || !addr_pairs || buff_loc >= SYSTEM_DMA_TOGGLE_COUNT || addr_pairs > SYSTEM_DMA_MAX_CHANNEL || fw_ctx_id >= FIRMWARE_CONTEXT_NUMBER )
        return -1;

    system_dma_device->sg_device_nents[fw_ctx_id][buff_loc] = addr_pairs;
    if ( !system_dma_device->mem_addrs[fw_ctx_id][buff_loc] )
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc] = kmalloc( sizeof( mem_addr_pair_t ) * SYSTEM_DMA_MAX_CHANNEL, GFP_KERNEL );

    if ( !system_dma_device->mem_addrs[fw_ctx_id][buff_loc] ) {
        LOG( LOG_CRIT, "Failed to allocate virtual address pairs for flushing!!" );
        return -1;
    }
    for ( i = 0; i < addr_pairs; i++ ) {
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc][i].dev_addr = device_addr_pair[i].address & (~0x1f);
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc][i].size = device_addr_pair[i].size + (device_addr_pair[i].address & 0x1f);
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc][i].sys_back_ptr = ctx;
    }

    LOG( LOG_INFO, "dma device setup success %d", system_dma_device->sg_device_nents[buff_loc] );
    return 0;
}

int32_t system_dma_sg_fwmem_setup( void *ctx, int32_t buff_loc, fwmem_addr_pair_t *fwmem_pair, int32_t addr_pairs, uint32_t fw_ctx_id )
{
    int i;
    system_dma_device_t *system_dma_device = (system_dma_device_t *)ctx;

    if ( !system_dma_device || !fwmem_pair || !addr_pairs || buff_loc >= SYSTEM_DMA_TOGGLE_COUNT ) {
        LOG( LOG_CRIT, "null param problems" );
        return -1;
    }
    system_dma_device->sg_fwmem_nents[fw_ctx_id][buff_loc] = addr_pairs;

    if ( !system_dma_device->mem_addrs[fw_ctx_id][buff_loc] )
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc] = kmalloc( sizeof( mem_addr_pair_t ) * SYSTEM_DMA_MAX_CHANNEL, GFP_KERNEL );

    if ( !system_dma_device->mem_addrs[fw_ctx_id][buff_loc] ) {
        LOG( LOG_CRIT, "Failed to allocate virtual address pairs for flushing!!" );
        return -1;
    }

    if ( !system_dma_device->mem_addrs[fw_ctx_id][buff_loc] ) {
        LOG( LOG_CRIT, "Failed to allocate virtual address pairs for flushing!!" );
        return -1;
    }
    for ( i = 0; i < addr_pairs; i++ ) {
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc][i].fw_addr = virt_to_phys(fwmem_pair[i].address) & (~0x1f);
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc][i].size = fwmem_pair[i].size + (virt_to_phys(fwmem_pair[i].address) & 0x1f);
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc][i].sys_back_ptr = ctx;
    }

    LOG( LOG_INFO, "fwmem setup success %d", system_dma_device->sg_device_nents[fw_ctx_id][buff_loc] );

    return 0;
}

inline void system_memcpy_dma(unsigned long src_mem, unsigned long dst_mem,uint32_t direction, uint32_t reg_cnt)
{
    uint32_t dma_dir = 0;

    if (direction == SYS_DMA_TO_DEVICE)
        dma_dir = 1;
    else
        dma_dir = 0;

    am_dma_cfg_src(0, src_mem);
    am_dma_cfg_dst(0, dst_mem);
    am_dma_cfg_dir_cnt(0, dma_dir, reg_cnt/4);
    am_dma_start();
    am_dma_done_check();
}

static void memcopy_func( unsigned long p_task )
{
    mem_tasklet_t *mem_task = (mem_tasklet_t *)p_task;
    mem_addr_pair_t *mem_addr = (mem_addr_pair_t *)mem_task->mem_data;
    system_dma_device_t *system_dma_device = (system_dma_device_t *)mem_addr->sys_back_ptr;
    unsigned long src_mem = 0;
    unsigned long dst_mem = 0;

    int32_t buff_loc = system_dma_device->buff_loc;
    uint32_t direction = system_dma_device->direction;

    if ( direction == SYS_DMA_TO_DEVICE ) {
        src_mem = mem_addr->fw_addr;
        dst_mem = mem_addr->dev_addr;
        cache_flush_for_device(src_mem, mem_addr->size);
        system_memcpy_dma(src_mem, dst_mem, direction, mem_addr->size);
    } else {
        src_mem = mem_addr->dev_addr;
        dst_mem = mem_addr->fw_addr;
        system_memcpy_dma(src_mem, dst_mem, direction, mem_addr->size);
        cache_flush(dst_mem, mem_addr->size);
    }

    LOG( LOG_DEBUG, "(%d:%d) d:%p s:%p l:%ld", buff_loc, direction, dst_mem, src_mem, mem_addr->size );

    dma_complete_func( mem_addr->sys_back_ptr );

    return;
}

void system_dma_unmap_sg( void *ctx )
{
    return;
}

int32_t system_dma_copy_sg( void *ctx, int32_t buff_loc, uint32_t direction, dma_completion_callback complete_func, uint32_t fw_ctx_id )
{
    int32_t i, result = 0;
    if ( !ctx ) {
        LOG( LOG_ERR, "Input ctx pointer is NULL" );
        return -1;
    }

    int32_t async_dma = 0;

    if ( complete_func != NULL ) {
        async_dma = 1;
    }

    system_dma_device_t *system_dma_device = (system_dma_device_t *)ctx;


    unsigned int src_nents = system_dma_device->sg_device_nents[fw_ctx_id][buff_loc];
    unsigned int dst_nents = system_dma_device->sg_fwmem_nents[fw_ctx_id][buff_loc];
    if ( src_nents != dst_nents || !src_nents || !dst_nents ) {
        LOG( LOG_CRIT, "Unbalance src_nents:%d dst_nents:%d", src_nents, dst_nents );
        return -1;
    }

    system_dma_device->cur_fw_ctx_id = fw_ctx_id;

    atomic_set( &system_dma_device->nents_done, 0 ); //set the number of nents done
    if ( async_dma == 0 ) {
        system_dma_device->complete_func = NULL; //async mode is not allowed to have callback
        init_completion( &system_dma_device->comp );
    } else {
        system_dma_device->complete_func = complete_func; //call this function if all nents are done;
    }
    system_dma_device->direction = direction;
    system_dma_device->buff_loc = buff_loc;


    for ( i = 0; i < SYSTEM_DMA_MAX_CHANNEL; i++ ) {
        system_dma_device->task_list[fw_ctx_id][buff_loc][i].mem_data = &( system_dma_device->mem_addrs[fw_ctx_id][buff_loc][i] );
        system_dma_device->task_list[fw_ctx_id][buff_loc][i].m_task.data = (unsigned long)&system_dma_device->task_list[fw_ctx_id][buff_loc][i];
        system_dma_device->task_list[fw_ctx_id][buff_loc][i].m_task.func = memcopy_func;
        memcopy_func( (unsigned long)&system_dma_device->task_list[fw_ctx_id][buff_loc][i].m_task );
    }

    if ( async_dma == 0 ) {
        LOG( LOG_DEBUG, "scatterlist DMA waiting completion\n" );
        wait_for_completion( &system_dma_device->comp );
    }

    LOG( LOG_DEBUG, "scatterlist DMA success\n" );
    return result;

}

#else

int32_t system_dma_sg_device_setup( void *ctx, int32_t buff_loc, dma_addr_pair_t *device_addr_pair, int32_t addr_pairs, uint32_t fw_ctx_id )
{
    system_dma_device_t *system_dma_device = (system_dma_device_t *)ctx;
    int i;

    if ( !system_dma_device || !device_addr_pair || !addr_pairs || buff_loc >= SYSTEM_DMA_TOGGLE_COUNT || addr_pairs > SYSTEM_DMA_MAX_CHANNEL || fw_ctx_id >= FIRMWARE_CONTEXT_NUMBER )
        return -1;

    system_dma_device->sg_device_nents[fw_ctx_id][buff_loc] = addr_pairs;
    if ( !system_dma_device->mem_addrs[fw_ctx_id][buff_loc] )
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc] = kmalloc( sizeof( mem_addr_pair_t ) * SYSTEM_DMA_MAX_CHANNEL, GFP_KERNEL );

    if ( !system_dma_device->mem_addrs[fw_ctx_id][buff_loc] ) {
        LOG( LOG_CRIT, "Failed to allocate virtual address pairs for flushing!!" );
        return -1;
    }
    for ( i = 0; i < addr_pairs; i++ ) {
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc][i].dev_addr = ioremap( device_addr_pair[i].address, device_addr_pair[i].size );
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc][i].size = device_addr_pair[i].size;
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc][i].sys_back_ptr = ctx;
    }

    LOG( LOG_INFO, "dma device setup success %d", system_dma_device->sg_device_nents[buff_loc] );
    return 0;
}

int32_t system_dma_sg_fwmem_setup( void *ctx, int32_t buff_loc, fwmem_addr_pair_t *fwmem_pair, int32_t addr_pairs, uint32_t fw_ctx_id )
{
    int i;
    system_dma_device_t *system_dma_device = (system_dma_device_t *)ctx;

    if ( !system_dma_device || !fwmem_pair || !addr_pairs || buff_loc >= SYSTEM_DMA_TOGGLE_COUNT ) {
        LOG( LOG_CRIT, "null param problems" );
        return -1;
    }
    system_dma_device->sg_fwmem_nents[fw_ctx_id][buff_loc] = addr_pairs;

    if ( !system_dma_device->mem_addrs[fw_ctx_id][buff_loc] )
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc] = kmalloc( sizeof( mem_addr_pair_t ) * SYSTEM_DMA_MAX_CHANNEL, GFP_KERNEL );

    if ( !system_dma_device->mem_addrs[fw_ctx_id][buff_loc] ) {
        LOG( LOG_CRIT, "Failed to allocate virtual address pairs for flushing!!" );
        return -1;
    }

    if ( !system_dma_device->mem_addrs[fw_ctx_id][buff_loc] ) {
        LOG( LOG_CRIT, "Failed to allocate virtual address pairs for flushing!!" );
        return -1;
    }
    for ( i = 0; i < addr_pairs; i++ ) {
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc][i].fw_addr = fwmem_pair[i].address;
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc][i].size = fwmem_pair[i].size;
        system_dma_device->mem_addrs[fw_ctx_id][buff_loc][i].sys_back_ptr = ctx;
    }

    LOG( LOG_INFO, "fwmem setup success %d", system_dma_device->sg_device_nents[fw_ctx_id][buff_loc] );

    return 0;
}

inline void system_memcpy_toio(volatile void __iomem *to, const void *from, size_t count)
{
#ifdef CONFIG_ARM64
    memcpy_toio(to, from, count);
#else
    const unsigned int *f = from;
    count /= 4;
    while (count) {
        count--;
        __raw_writel(*f, to);
        f++;
        to += 4;
    }
#endif
}

inline void system_memcpy_fromio(void *to, const volatile void __iomem *from, size_t count)
{
#ifdef CONFIG_ARM64
    memcpy_fromio(to, from ,count);
#else
    unsigned int *t = to;
    count /= 4;
    while (count) {
        count--;
        *t = __raw_readl(from);
        t++;
        from += 4;
    }
#endif
}

static void memcopy_func( unsigned long p_task )
{
    mem_tasklet_t *mem_task = (mem_tasklet_t *)p_task;
    mem_addr_pair_t *mem_addr = (mem_addr_pair_t *)mem_task->mem_data;
    system_dma_device_t *system_dma_device = (system_dma_device_t *)mem_addr->sys_back_ptr;
    void *src_mem = 0;
    void *dst_mem = 0;

    int32_t buff_loc = system_dma_device->buff_loc;
    uint32_t direction = system_dma_device->direction;

    if ( direction == SYS_DMA_TO_DEVICE ) {
        src_mem = mem_addr->fw_addr;
        dst_mem = mem_addr->dev_addr;
        system_memcpy_toio( dst_mem, src_mem, mem_addr->size);
    } else {
        dst_mem = mem_addr->fw_addr;
        src_mem = mem_addr->dev_addr;
        system_memcpy_fromio( dst_mem, src_mem, mem_addr->size );
    }

    LOG( LOG_DEBUG, "(%d:%d) d:%p s:%p l:%ld", buff_loc, direction, dst_mem, src_mem, mem_addr->size );

    dma_complete_func( mem_addr->sys_back_ptr );

    return;
}

void system_dma_unmap_sg( void *ctx )
{
    return;
}

int32_t system_dma_copy_sg( void *ctx, int32_t buff_loc, uint32_t direction, dma_completion_callback complete_func, uint32_t fw_ctx_id )
{
    int32_t i, result = 0;
    if ( !ctx ) {
        LOG( LOG_ERR, "Input ctx pointer is NULL" );
        return -1;
    }

    int32_t async_dma = 0;

    if ( complete_func != NULL ) {
        async_dma = 1;
    }

    system_dma_device_t *system_dma_device = (system_dma_device_t *)ctx;


    unsigned int src_nents = system_dma_device->sg_device_nents[fw_ctx_id][buff_loc];
    unsigned int dst_nents = system_dma_device->sg_fwmem_nents[fw_ctx_id][buff_loc];
    if ( src_nents != dst_nents || !src_nents || !dst_nents ) {
        LOG( LOG_CRIT, "Unbalance src_nents:%d dst_nents:%d", src_nents, dst_nents );
        return -1;
    }

    system_dma_device->cur_fw_ctx_id = fw_ctx_id;

    atomic_set( &system_dma_device->nents_done, 0 ); //set the number of nents done
    if ( async_dma == 0 ) {
        system_dma_device->complete_func = NULL; //async mode is not allowed to have callback
        init_completion( &system_dma_device->comp );
    } else {
        system_dma_device->complete_func = complete_func; //call this function if all nents are done;
    }
    system_dma_device->direction = direction;
    system_dma_device->buff_loc = buff_loc;


    for ( i = 0; i < SYSTEM_DMA_MAX_CHANNEL; i++ ) {
        system_dma_device->task_list[fw_ctx_id][buff_loc][i].mem_data = &( system_dma_device->mem_addrs[fw_ctx_id][buff_loc][i] );
        system_dma_device->task_list[fw_ctx_id][buff_loc][i].m_task.data = (unsigned long)&system_dma_device->task_list[fw_ctx_id][buff_loc][i];
        system_dma_device->task_list[fw_ctx_id][buff_loc][i].m_task.func = memcopy_func;
        tasklet_schedule( &system_dma_device->task_list[fw_ctx_id][buff_loc][i].m_task );
    }


    if ( async_dma == 0 ) {
        LOG( LOG_DEBUG, "scatterlist DMA waiting completion\n" );
        wait_for_completion( &system_dma_device->comp );
    }

    LOG( LOG_DEBUG, "scatterlist DMA success\n" );
    return result;
}

#endif
