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

#include "acamera_logger.h"
#include "system_interrupts.h"
#include "system_stdlib.h"
#include "dma_writer_api.h"
#include "dma_writer.h"
#include "acamera_firmware_config.h"
#include "acamera.h"

#ifdef LOG_MODULE
#undef LOG_MODULE
#define LOG_MODULE LOG_MODULE_DMA_WRITER
#endif

#define TAG "DMA_WRITER"

//#define DMA_PRINTF(a) printf a
#define DMA_PRINTF( a ) (void)0

static dma_handle s_handle[FIRMWARE_CONTEXT_NUMBER];
static int32_t ctx_pos = 0;
dma_error dma_writer_create( void **handle )
{
    dma_error result = edma_ok;
    if ( handle != NULL ) {

        if ( ctx_pos < acamera_get_context_number() ) {
            *handle = &s_handle[ctx_pos];
            system_memset( (void *)*handle, 0, sizeof( dma_handle ) );
            ctx_pos++;
        } else {
            *handle = NULL;
            result = edma_fail;
            LOG( LOG_CRIT, "dma context overshoot\n" );
        }
    } else {
        result = edma_fail;
    }
    return result;
}

void dma_writer_exit( void *handle )
{
    ctx_pos = 0;
}

dma_error dma_writer_update_state( dma_pipe *pipe )
{
    dma_error result = edma_ok;
    if ( pipe != NULL ) {
        LOG( LOG_INFO, "state update %dx%d\n", (int)pipe->settings.width, (int)pipe->settings.height );
        if ( pipe->settings.width == 0 || pipe->settings.height == 0 ) {
            result = edma_invalid_settings;
            pipe->settings.enabled = 0;
            pipe->api.p_acamera_isp_dma_writer_frame_write_on_write( pipe->settings.isp_base, pipe->settings.enabled ); //disable pipe on invalid settings
            pipe->api.p_acamera_isp_dma_writer_frame_write_on_write_uv( pipe->settings.isp_base, pipe->settings.enabled );
            return result;
        }


        //set default settings here
        pipe->api.p_acamera_isp_dma_writer_max_bank_write( pipe->settings.isp_base, 0 );
        pipe->api.p_acamera_isp_dma_writer_active_width_write( pipe->settings.isp_base, pipe->settings.width );
        pipe->api.p_acamera_isp_dma_writer_active_height_write( pipe->settings.isp_base, pipe->settings.height );

        pipe->api.p_acamera_isp_dma_writer_max_bank_write_uv( pipe->settings.isp_base, 0 );
        pipe->api.p_acamera_isp_dma_writer_active_width_write_uv( pipe->settings.isp_base, pipe->settings.width );
        pipe->api.p_acamera_isp_dma_writer_active_height_write_uv( pipe->settings.isp_base, pipe->settings.height );
    } else {
        result = edma_fail;
    }
    return result;
}


dma_error dma_writer_get_settings( void *handle, dma_type type, dma_pipe_settings *settings )
{
    dma_error result = edma_ok;
    if ( handle != NULL ) {
        dma_handle *p_dma = (dma_handle *)handle;
        system_memcpy( (void *)settings, &p_dma->pipe[type].settings, sizeof( dma_pipe_settings ) );
    } else {
        result = edma_fail;
    }
    return result;
}


dma_error dma_writer_set_settings( void *handle, dma_type type, dma_pipe_settings *settings )
{
    dma_error result = edma_ok;
    if ( handle != NULL ) {
        dma_handle *p_dma = (dma_handle *)handle;
        system_memcpy( (void *)&p_dma->pipe[type].settings, settings, sizeof( dma_pipe_settings ) );
        result = dma_writer_update_state( &p_dma->pipe[type] );

    } else {
        LOG( LOG_ERR, "set settings handle null\n" );
        result = edma_fail;
    }
    return result;
}

dma_error dma_writer_get_ptr_settings( void *handle, dma_type type, dma_pipe_settings **settings )
{
    dma_error result = edma_ok;
    if ( handle != NULL ) {
        dma_handle *p_dma = (dma_handle *)handle;
        *settings = &p_dma->pipe[type].settings;
    } else {
        result = edma_fail;
    }
    return result;
}

void dma_writer_set_initialized( void *handle, dma_type type, uint8_t initialized )
{
    if ( handle != NULL ) {
        dma_handle *p_dma = (dma_handle *)handle;
        p_dma->pipe[type].state.initialized = initialized;
    }
}

dma_error dma_writer_init( void *handle, dma_type type, dma_pipe_settings *settings, dma_api *api )
{
    dma_error result = edma_ok;
    if ( handle != NULL ) {
        dma_handle *p_dma = (dma_handle *)handle;
        p_dma->pipe[type].type = type;
        system_memcpy( (void *)&p_dma->pipe[type].api, api, sizeof( dma_api ) );
        result = dma_writer_set_settings( handle, type, settings );
        result |= dma_writer_reset( handle, type );

        if ( result == edma_ok ) {
            LOG( LOG_INFO, "%s: initialized a dma writer output. %dx%d ", TAG, (int)settings->width, (int)settings->height );
        } else {
            LOG( LOG_ERR, "%s: Failed to initialize a dma writer type:%d output. %dx%d ", TAG, (int)type, (int)settings->width, (int)settings->height );
        }
    } else {
        result = edma_fail;
    }
    return result;
}

void dma_writer_init_frame_queue( dma_pipe *pipe )
{ //init all buffer to inuse so we can remove it from the queue
    int i;
    for ( i = 0; i < MAX_DMA_QUEUE_FRAMES; i++ ) {
        pipe->settings.frame_buf_queue[i].primary.status = dma_buf_purge;
        pipe->settings.frame_buf_queue[i].secondary.status = dma_buf_purge;
    }
    DMA_PRINTF( ( "all frames to dma_buf_purge\n" ) );
    return;
}

dma_error dma_writer_reset( void *handle, dma_type type )
{
    dma_error result = edma_ok;
    if ( handle != NULL ) {
        dma_handle *p_dma = (dma_handle *)handle;
        dma_pipe *pipe = &p_dma->pipe[type];
        pipe->state.buf_num_rdi = 0;
        pipe->state.buf_num_wri = 0;
        pipe->state.buf_num_size = 0;
        pipe->settings.last_tframe = NULL;
        pipe->settings.inqueue_tframe[0] = NULL;
        pipe->settings.inqueue_tframe[1] = NULL;
        pipe->settings.init_delay = 0;
        dma_writer_init_frame_queue( pipe );
        pipe->api.p_acamera_isp_dma_writer_frame_write_on_write( pipe->settings.isp_base, 0 );
        pipe->api.p_acamera_isp_dma_writer_frame_write_on_write_uv( pipe->settings.isp_base, 0 );
        pipe->settings.enabled = 0;
        pipe->settings.c_fps = 0;
        pipe->settings.t_fps = 0;
        pipe->settings.inter_val = 0;
        pipe->settings.back_tframe = NULL;
    } else {
        result = edma_fail;
    }
    return result;
}

dma_error dma_writer_set_pipe_fps(void *handle, dma_type type,
                                uint32_t c_fps, uint32_t t_fps)
{
    dma_handle *p_dma = NULL;
    dma_pipe *pipe = NULL;
    uint32_t inter_val = 0;

    if (handle == NULL || c_fps == 0 || t_fps == 0 || (t_fps > c_fps)) {
        LOG(LOG_ERR, "Error input param\n");
        return edma_wrong_parameters;
    }

    p_dma = handle;
    pipe = &p_dma->pipe[type];
    pipe->settings.c_fps = c_fps;
    pipe->settings.t_fps = t_fps;

    inter_val = c_fps / t_fps;
    if (c_fps % t_fps != 0)
        inter_val += 1;

    pipe->settings.inter_val = inter_val;

    LOG(LOG_INFO, "c_fps: %d, t_fps: %d, interval %d\n", c_fps, t_fps, inter_val);

    return edma_ok;
}


dma_error dma_writer_get_default_settings( void *handle, dma_type type, dma_pipe_settings *settings )
{
    dma_error result = edma_ok;
    return result;
}

uint16_t dma_writer_write_frame_queue( void *handle, dma_type type, tframe_t *frame_buf_array, uint16_t frame_buf_len )
{

    int i, set_i = 0;
    dma_handle *p_dma = (dma_handle *)handle;
    dma_pipe *pipe = &p_dma->pipe[type];

    if ( handle == NULL || frame_buf_array == NULL || pipe == NULL ) {
        LOG( LOG_CRIT, "NULL detected handle:%p,frame_buf_array:%p,pipe:%p\n", handle, frame_buf_array, pipe );
        return 0;
    }

    if ( frame_buf_len == 0 )
        return 0;

    int index = 0;
    for ( i = 0; i < MAX_DMA_QUEUE_FRAMES; i++ ) {
        index = ( i + pipe->state.buf_num_rdi ) % MAX_DMA_QUEUE_FRAMES;

        if ( pipe->settings.frame_buf_queue[index].primary.status == dma_buf_purge ) { //replace the buffer when it is in use
            /* init tframe status first */
            frame_buf_array[set_i].primary.status = dma_buf_purge;
            frame_buf_array[set_i].secondary.status = dma_buf_purge;
            pipe->settings.frame_buf_queue[index] = frame_buf_array[set_i];
            pipe->settings.frame_buf_queue[index].primary.status = dma_buf_empty;
            pipe->settings.frame_buf_queue[index].secondary.status = dma_buf_empty;
            LOG( LOG_INFO, "enqueue:0x%lx at %d\n", pipe->settings.frame_buf_queue[index].primary.address, (int)i );
            set_i++;
        }
        if ( set_i >= frame_buf_len ) {
            set_i = i++;
            break;
        }
    }
    LOG( LOG_DEBUG, "ctx_id: %d, isp_base: %x, dma_writer_bank0_base_addr: 0x%08x.", (int)pipe->settings.ctx_id, (unsigned int)pipe->settings.isp_base, (unsigned int)frame_buf_array[0].primary.address );
    if ( i > pipe->state.buf_num_size ) {
        pipe->state.buf_num_size = i;
        DMA_PRINTF( ( "dma buf_num_size changed %d\n", (int)pipe->state.buf_num_size ) );
    }

    if ( pipe->settings.enabled == 0 ) {
        DMA_PRINTF( ( "dma re-enabling\n" ) );
        pipe->api.p_acamera_isp_dma_writer_bank0_base_write( pipe->settings.isp_base, pipe->settings.frame_buf_queue[set_i].primary.address );
        pipe->api.p_acamera_isp_dma_writer_bank0_base_write_uv( pipe->settings.isp_base, pipe->settings.frame_buf_queue[set_i].secondary.address );
        LOG( LOG_INFO, "active_width: %u, active_height: %u.", (unsigned int)pipe->settings.width, (unsigned int)pipe->settings.height );
        pipe->settings.inqueue_tframe[1] = &pipe->settings.frame_buf_queue[set_i];
        pipe->settings.frame_buf_queue[set_i].primary.status = dma_buf_busy;
        pipe->settings.enabled = 1;
        if ( pipe->api.p_acamera_isp_dma_writer_format_read( pipe->settings.isp_base ) != DMA_FORMAT_DISABLE )
            pipe->api.p_acamera_isp_dma_writer_frame_write_on_write( pipe->settings.isp_base, 1 );
        else
            pipe->api.p_acamera_isp_dma_writer_frame_write_on_write( pipe->settings.isp_base, 0 );

        //check if format is for UV
        if ( pipe->api.p_acamera_isp_dma_writer_format_read_uv( pipe->settings.isp_base ) != DMA_FORMAT_DISABLE ) {
            pipe->settings.frame_buf_queue[set_i].secondary.status = dma_buf_busy;
            pipe->api.p_acamera_isp_dma_writer_frame_write_on_write_uv( pipe->settings.isp_base, 1 );
            LOG( LOG_DEBUG, "enabled uv %d", pipe->api.p_acamera_isp_dma_writer_format_read_uv( pipe->settings.isp_base ) );
        } else {
            //cannot enable
            pipe->api.p_acamera_isp_dma_writer_frame_write_on_write_uv( pipe->settings.isp_base, 0 );
            LOG( LOG_DEBUG, "cannot enable uv %d", pipe->api.p_acamera_isp_dma_writer_format_read_uv( pipe->settings.isp_base ) );
        }
    }
    return i;
}

tframe_t *dma_get_next_empty_frame( dma_pipe *pipe )
{
    int i, index = 0, last_index;
    for ( i = 0; i < MAX_DMA_QUEUE_FRAMES; i++ ) {
        index = ( i + pipe->state.buf_num_wri ) % MAX_DMA_QUEUE_FRAMES;
        last_index = pipe->state.buf_num_wri;
        if ( pipe->settings.frame_buf_queue[index].primary.status == dma_buf_empty ) {
            pipe->state.buf_num_wri = ( index + 1 ) % MAX_DMA_QUEUE_FRAMES;
            //recover busy frames not returned by dma
            if (( pipe->settings.frame_buf_queue[last_index].primary.status == dma_buf_busy )
                && (&pipe->settings.frame_buf_queue[last_index] != pipe->settings.last_tframe)
                && (&pipe->settings.frame_buf_queue[last_index] != pipe->settings.inqueue_tframe[0])
                && (&pipe->settings.frame_buf_queue[last_index] != pipe->settings.inqueue_tframe[1])) {
                pipe->settings.frame_buf_queue[last_index].primary.status = dma_buf_empty;
                pipe->settings.frame_buf_queue[last_index].secondary.status = dma_buf_empty;
                DMA_PRINTF( ( "recovered 0x%lx\n", pipe->settings.frame_buf_queue[last_index].primary.address ) );
            }
            return &( pipe->settings.frame_buf_queue[index] );
        }
    }

    return 0;
}


tframe_t *dma_get_next_ready_frame( dma_pipe *pipe )
{
    int i, index;
    for ( i = 0; i < MAX_DMA_QUEUE_FRAMES; i++ ) {
        index = ( i + pipe->state.buf_num_rdi ) % MAX_DMA_QUEUE_FRAMES;
        if ( pipe->settings.frame_buf_queue[index].primary.status == dma_buf_ready ) {
            pipe->state.buf_num_rdi = ( index + 1 ) % MAX_DMA_QUEUE_FRAMES;
            return &( pipe->settings.frame_buf_queue[index] );
        }
    }
    return 0;
}


tframe_t *dma_writer_api_return_next_ready_frame( void *handle, dma_type type )
{
    dma_handle *p_dma = (dma_handle *)handle;
    dma_pipe *pipe = &p_dma->pipe[type];
    return dma_get_next_ready_frame( pipe );
}


extern int32_t acamera_get_api_context( void );

//extern void dma_input_reset();
dma_error dma_writer_pipe_update( dma_pipe *pipe , bool drop_frame)
{
    dma_error result = edma_ok;
    tframe_t *empty_frame = NULL;
    uint32_t addr = 0;
#if ISP_HAS_FPGA_WRAPPER && ISP_CONTROLS_DMA_READER
    uint32_t addr_uv = 0;
#endif
    //only update if there are frames
    if ( pipe != NULL && pipe->state.buf_num_size ) {
        if ( pipe->state.initialized && pipe->settings.pause == 0 ) {

            //only check primary status
            if ( pipe->settings.last_tframe ) {
                if (pipe->settings.init_delay > 0) {
                    /* recycle the buffer as empty */
                    pipe->settings.init_delay--;
                    pipe->settings.last_tframe->primary.status = dma_buf_empty;
                    pipe->settings.last_tframe->secondary.status = dma_buf_empty;
                    pipe->settings.last_tframe = NULL;
                } else if (drop_frame) {
                    /* recycle the buffer as empty */
                    pipe->settings.last_tframe->primary.status = dma_buf_empty;
                    pipe->settings.last_tframe->secondary.status = dma_buf_empty;
                    pipe->settings.last_tframe = NULL;
                } else {
                    addr = pipe->settings.last_tframe->primary.address;
#if ISP_HAS_FPGA_WRAPPER && ISP_CONTROLS_DMA_READER
                    addr_uv = pipe->settings.last_tframe->secondary.address;
#endif
                    pipe->settings.last_tframe->primary.status = dma_buf_ready;
                    pipe->settings.last_tframe->secondary.status = dma_buf_ready;
                }
                LOG( LOG_DEBUG, "wbase_last_read:0x%lx\n", addr );
            }
// update frame reader address
#if ISP_HAS_FPGA_WRAPPER && ISP_CONTROLS_DMA_READER
            static uint32_t addr_late, addr_late1;
            static uint32_t addr_late_uv, addr_late1_uv;
            if ( pipe->settings.active && pipe->settings.ctx_id == acamera_get_api_context() && pipe->settings.last_tframe ) {
                pipe->api.p_acamera_fpga_frame_reader_rbase_write( pipe->settings.isp_base, addr_late + pipe->settings.read_offset );
                LOG( LOG_DEBUG, "ctx_id: %d, dma_pipe_type: %d, frame_reader_rbase: 0x%08x.", (int)pipe->settings.ctx_id, (int)pipe->type, (unsigned int)( addr_late + pipe->settings.read_offset ) );
                pipe->api.p_acamera_fpga_frame_reader_line_offset_write( pipe->settings.isp_base, pipe->settings.last_tframe->primary.line_offset );
                pipe->api.p_acamera_fpga_frame_reader_format_write( pipe->settings.isp_base, pipe->api.p_acamera_isp_dma_writer_format_read( pipe->settings.isp_base ) );
                pipe->api.p_acamera_fpga_frame_reader_rbase_load_write( pipe->settings.isp_base, 1 );
                pipe->api.p_acamera_fpga_frame_reader_rbase_load_write( pipe->settings.isp_base, 0 );

                if ( pipe->api.p_acamera_isp_dma_writer_format_read_uv( pipe->settings.isp_base ) != DMA_FORMAT_DISABLE ) {
                    pipe->api.p_acamera_fpga_frame_reader_rbase_write_uv( pipe->settings.isp_base, addr_late_uv + pipe->settings.read_offset );
                    pipe->api.p_acamera_fpga_frame_reader_line_offset_write_uv( pipe->settings.isp_base, pipe->settings.last_tframe->secondary.line_offset );
                    pipe->api.p_acamera_fpga_frame_reader_format_write_uv( pipe->settings.isp_base, pipe->api.p_acamera_isp_dma_writer_format_read_uv( pipe->settings.isp_base ) );
                    pipe->api.p_acamera_fpga_frame_reader_rbase_load_write_uv( pipe->settings.isp_base, 1 );
                    pipe->api.p_acamera_fpga_frame_reader_rbase_load_write_uv( pipe->settings.isp_base, 0 );
                }
                addr_late = addr_late1;
                addr_late1 = addr;

                addr_late_uv = addr_late1_uv;
                addr_late1_uv = addr_uv;
            }
#endif
            pipe->settings.last_tframe = pipe->settings.inqueue_tframe[0];
            pipe->settings.inqueue_tframe[0] = pipe->settings.inqueue_tframe[1];
            pipe->settings.inqueue_tframe[1] = NULL;

            if (pipe->settings.back_tframe != NULL)
                empty_frame = pipe->settings.back_tframe;
            else
                empty_frame = dma_get_next_empty_frame(pipe);

            if ( empty_frame != NULL ) {
                empty_frame->primary.type = pipe->api.p_acamera_isp_dma_writer_format_read( pipe->settings.isp_base );
                if ( empty_frame->primary.type != DMA_FORMAT_DISABLE ) {
                    empty_frame->primary.width = pipe->api.p_acamera_isp_dma_writer_active_width_read( pipe->settings.isp_base );
                    empty_frame->primary.height = pipe->api.p_acamera_isp_dma_writer_active_height_read( pipe->settings.isp_base );
                    empty_frame->primary.line_offset = acamera_line_offset( empty_frame->primary.width, _get_pixel_width( empty_frame->primary.type ) );
                    uint32_t frame_size = empty_frame->primary.line_offset * empty_frame->primary.height;
                    addr = empty_frame->primary.address;
                    pipe->settings.inqueue_tframe[1] = empty_frame;
                    LOG( LOG_DEBUG, "next dma addr:0x%lx\n", addr );
                    if ( pipe->settings.vflip ) {
                        addr += frame_size - empty_frame->primary.line_offset;
                        pipe->api.p_acamera_isp_dma_writer_line_offset_write( pipe->settings.isp_base, -(uint32_t)empty_frame->primary.line_offset );
                    } else {
                        pipe->api.p_acamera_isp_dma_writer_line_offset_write( pipe->settings.isp_base, empty_frame->primary.line_offset );
                    }

                    //multicontext change in resolution
                    pipe->api.p_acamera_isp_dma_writer_active_width_write( pipe->settings.isp_base, empty_frame->primary.width );
                    pipe->api.p_acamera_isp_dma_writer_active_height_write( pipe->settings.isp_base, empty_frame->primary.height );

                    addr += pipe->settings.write_offset;
                    if ( frame_size > empty_frame->primary.size ) { //check frame buffer size
                        LOG( LOG_CRIT, "%s: frame_size greater than available buffer. fr %d vs %d ", TAG, (int)frame_size, (int)empty_frame->primary.size );
                        result = edma_fail;
                    } else {
                        LOG( LOG_DEBUG, "ctx_id: %d, isp_base: %x, dma_pipe_type: %d, dma_writer_bank0_base_addr: 0x%08x.", (int)pipe->settings.ctx_id, (unsigned int)pipe->settings.isp_base, (int)pipe->type, (unsigned int)addr );
                        pipe->api.p_acamera_isp_dma_writer_bank0_base_write( pipe->settings.isp_base, addr );
                        empty_frame->primary.status = dma_buf_busy;
                        pipe->settings.last_address = addr;
                        pipe->api.p_acamera_isp_dma_writer_frame_write_on_write( pipe->settings.isp_base, 1 );
                        /*if ( pipe->settings.enabled == 0 ) {
							pipe->api.p_acamera_isp_dma_writer_frame_write_on_write( pipe->settings.isp_base, 1 );
							pipe->settings.enabled = 1;
							DMA_PRINTF( ( "warning pipe is disabled, enabling" ) );
						}*/
                    }
                }

                empty_frame->secondary.type = pipe->api.p_acamera_isp_dma_writer_format_read_uv( pipe->settings.isp_base );
                if ( empty_frame->secondary.type != DMA_FORMAT_DISABLE ) {
                    empty_frame->secondary.width = pipe->api.p_acamera_isp_dma_writer_active_width_read_uv( pipe->settings.isp_base );
                    empty_frame->secondary.height = pipe->api.p_acamera_isp_dma_writer_active_height_read_uv( pipe->settings.isp_base );
                    empty_frame->secondary.line_offset = acamera_line_offset( empty_frame->secondary.width, _get_pixel_width( empty_frame->secondary.type ) );
                    uint32_t frame_size = empty_frame->secondary.line_offset * empty_frame->secondary.height;
                    if (empty_frame->secondary.type == DMA_FORMAT_NV12_UV || empty_frame->secondary.type == DMA_FORMAT_NV12_VU)
                        frame_size = frame_size / 2;
                    addr = empty_frame->secondary.address;
                    if ( pipe->settings.vflip ) {
                        addr += frame_size - empty_frame->secondary.line_offset;
                        pipe->api.p_acamera_isp_dma_writer_line_offset_write_uv( pipe->settings.isp_base, -(uint32_t)empty_frame->secondary.line_offset );
                    } else {
                        pipe->api.p_acamera_isp_dma_writer_line_offset_write_uv( pipe->settings.isp_base, empty_frame->secondary.line_offset );
                    }

                    addr += pipe->settings.write_offset;
                    if ( frame_size > empty_frame->secondary.size ) { //check frame buffer size
                        LOG( LOG_CRIT, "%s: frame_size greater than available buffer. ds %d vs %d ", TAG, (int)frame_size, (int)empty_frame->secondary.size );
                        result = edma_fail;
                    } else {
                        LOG( LOG_DEBUG, "ctx_id: %d, isp_base: %x, dma_pipe_type: %d, dma_writer_bank0_base_addr: 0x%08x.", (int)pipe->settings.ctx_id, (unsigned int)pipe->settings.isp_base, (int)pipe->type, (unsigned int)addr );
                        pipe->api.p_acamera_isp_dma_writer_bank0_base_write_uv( pipe->settings.isp_base, addr );
                        empty_frame->secondary.status = dma_buf_busy;
                        pipe->settings.last_address = addr;
                        pipe->api.p_acamera_isp_dma_writer_frame_write_on_write_uv( pipe->settings.isp_base, 1 );
                        /*if ( pipe->settings.enabled == 0 ) {
							pipe->api.p_acamera_isp_dma_writer_frame_write_on_write_uv( pipe->settings.isp_base,  1 );
							//pipe->settings.enabled = 1;
							DMA_PRINTF( ( "warning pipe is disabled" ) );
						}*/
                    }
                } else {
                    pipe->api.p_acamera_isp_dma_writer_frame_write_on_write_uv( pipe->settings.isp_base, 0 );
                }

            } else {
                //no empty frames
                //LOG( LOG_CRIT, "NO EMPTY FRAMES on dma_pipe_type: %d\n", (int)pipe->type );
                pipe->api.p_acamera_isp_dma_writer_frame_write_on_write( pipe->settings.isp_base, 0 );
                pipe->api.p_acamera_isp_dma_writer_frame_write_on_write_uv( pipe->settings.isp_base, 0 );
            }
        }
    } else {
        result = edma_fail;
    }
    return result;
}

dma_error dma_writer_pipe_set_fps(dma_pipe *pipe)
{
    int frm_count = 0;
    uint32_t inter_val = 0;
    uint32_t c_fps = 0;
    uint32_t t_fps = 0;

    if (pipe == NULL || pipe->settings.p_ctx == NULL) {
        LOG(LOG_ERR, "Error input param:p_ctx %p\n", pipe->settings.p_ctx);
        return edma_invalid_pipe;
    }

    frm_count = pipe->settings.p_ctx->isp_frame_counter;
    inter_val = pipe->settings.inter_val;
    pipe->settings.back_tframe = NULL;
    c_fps = pipe->settings.c_fps;
    t_fps = pipe->settings.t_fps;

    if ((pipe->settings.inqueue_tframe[1] == NULL)
        || (inter_val == 0) || (inter_val == 1))
        return edma_ok;

    if (inter_val > 2) {
        if (frm_count % inter_val != 0) {
            pipe->api.p_acamera_isp_dma_writer_frame_write_on_write( pipe->settings.isp_base, 0 );
            pipe->api.p_acamera_isp_dma_writer_frame_write_on_write_uv( pipe->settings.isp_base, 0 );
            pipe->settings.back_tframe = pipe->settings.inqueue_tframe[1];
            pipe->settings.inqueue_tframe[1] = NULL;
        } else {
            pipe->settings.back_tframe = NULL;
        }
    } else {
        inter_val = c_fps  / (c_fps - t_fps);
        if (frm_count % inter_val == 0) {
            pipe->api.p_acamera_isp_dma_writer_frame_write_on_write( pipe->settings.isp_base, 0 );
            pipe->api.p_acamera_isp_dma_writer_frame_write_on_write_uv( pipe->settings.isp_base, 0 );
            pipe->settings.back_tframe = pipe->settings.inqueue_tframe[1];
            pipe->settings.inqueue_tframe[1] = NULL;
        } else {
            pipe->settings.back_tframe = NULL;
        }
    }

    return edma_ok;
}

dma_error dma_writer_pipe_process_interrupt( dma_pipe *pipe, uint32_t irq_event )
{
    switch ( irq_event ) {
    case ACAMERA_IRQ_FRAME_WRITER_FR:
        if ( pipe->type == dma_fr ) {
            dma_writer_pipe_update( pipe , false); // have to change last address and buffer ring
            dma_writer_pipe_set_fps(pipe); //change the fps of fr path
        }
        break;
    case ACAMERA_IRQ_FRAME_WRITER_DS:
        if ( pipe->type == dma_ds1 || pipe->type == dma_ds2 ) {
            dma_writer_pipe_update( pipe , false); // have to change last address and buffer ring
            dma_writer_pipe_set_fps(pipe); //change the fps of ds1 path
        }
        break;
    case ACAMERA_IRQ_FRAME_DROP_FR:
        if ( pipe->type == dma_fr ) {
            dma_writer_pipe_update( pipe , true); // have to change last address and buffer ring
            dma_writer_pipe_set_fps(pipe); //change the fps of fr path
        }
        break;
    case ACAMERA_IRQ_FRAME_DROP_DS:
        if ( pipe->type == dma_ds1 || pipe->type == dma_ds2 ) {
            dma_writer_pipe_update( pipe , true); // have to change last address and buffer ring
            dma_writer_pipe_set_fps(pipe); //change the fps of ds1 path
        }
        break;
    }
    return 0;
}


dma_error dma_writer_process_interrupt( void *handle, uint32_t irq_event )
{
    dma_error result = edma_ok;
    uint32_t idx = 0;

    dma_handle *p_dma = (dma_handle *)handle;
    if ( !p_dma ) {
        LOG( LOG_ERR, "p_dma is NULL of event: %u", (unsigned int)irq_event );
        return edma_fail;
    }

    for ( idx = 0; idx < dma_max; idx++ ) {
        if (p_dma->pipe[idx].state.initialized == 1)
            dma_writer_pipe_process_interrupt( &p_dma->pipe[idx], irq_event );
    }
    return result;
}


metadata_t *dma_writer_return_metadata( void *handle, dma_type type )
{
    if ( handle != NULL ) {

        dma_handle *p_dma = (dma_handle *)handle;

        metadata_t *ret = &( p_dma->pipe[type].settings.curr_metadata );
        return ret;
    }
    return 0;
}

dma_error dma_writer_pipe_get_next_empty_buffer( void *handle, dma_type type )
{
    dma_error result = edma_ok;

    if ( handle != NULL ) {
        dma_handle *p_dma = (dma_handle *)handle;
        dma_pipe *pipe = &p_dma->pipe[type];

        return dma_writer_pipe_update(pipe, false);
    } else {
        result = edma_fail;
    }
    return result;
}