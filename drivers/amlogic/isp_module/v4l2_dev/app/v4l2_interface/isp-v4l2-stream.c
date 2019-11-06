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
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/freezer.h>
#include <linux/random.h>
#include <asm/div64.h>
#include <linux/sched.h>

#include <linux/videodev2.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <linux/dma-mapping.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-contiguous.h>

#include "acamera_firmware_api.h"
#include "acamera_firmware_config.h"

#if defined( ISP_HAS_METADATA_FSM )
#include "metadata_api.h"
#endif

#include "acamera_logger.h"

#include "isp-v4l2-common.h"
#include "isp-v4l2.h"
#include "fw-interface.h"

#include "isp-v4l2-stream.h"

#define CHECK_METADATA_ID 0

#define ISP_FW_FRAME_BUF_INVALID 0 /* buffer data is invalid  */
#define ISP_FW_FRAME_BUF_VALID 1   /* buffer data is valid  */

#define CMA_ALLOC_SIZE 64
/* metadata size */
#if defined( ISP_HAS_METADATA_FSM )
#define ISP_V4L2_METADATA_SIZE sizeof( firmware_metadata_t )
#else
#define ISP_V4L2_METADATA_SIZE 4096
#endif

/* max size */
#define ISP_V4L2_MAX_WIDTH 4000
#define ISP_V4L2_MAX_HEIGHT 3000

/* default size & format */
#define ISP_DEFAULT_FORMAT V4L2_PIX_FMT_RGB32

typedef struct _isp_v4l2_fmt {
    const char *name;
    uint32_t fourcc;
    uint8_t depth;
    bool is_yuv;
    uint8_t planes;
} isp_v4l2_fmt_t;

static isp_v4l2_fmt_t isp_v4l2_supported_formats[] =
    {
        {
            .name = "ARGB32",
            .fourcc = V4L2_PIX_FMT_RGB32,
            .depth = 32,
            .is_yuv = false,
            .planes = 1,
        },
        {
            .name = "ARGB24",
            .fourcc = V4L2_PIX_FMT_RGB24,
            .depth = 24,
            .is_yuv = false,
            .planes = 1,
        },
		{
			.name = "NV12",
			.fourcc = V4L2_PIX_FMT_NV12,
			.depth = 8,
			.is_yuv = true,
			.planes = 2,
		},
		{
			.name = "AYUV",
			.fourcc = V4L2_PIX_FMT_YUV444,
			.depth = 32,
			.is_yuv = true,
			.planes = 1,
		},
		{
			.name = "YUY2",
			.fourcc = V4L2_PIX_FMT_YUYV,
			.depth = 16,
			.is_yuv = true,
			.planes = 1,
		},
		{
			.name = "UYVY",
			.fourcc = V4L2_PIX_FMT_UYVY,
			.depth = 16,
			.is_yuv = true,
			.planes = 1,
		},
		{
			.name = "GREY",
			.fourcc = V4L2_PIX_FMT_GREY,
			.depth = 8,
			.is_yuv = true,
			.planes = 1,
		},
        {
            .name = "RAW 16",
            .fourcc = V4L2_PIX_FMT_SBGGR16,
            .depth = 16,
            .is_yuv = false,
            .planes = 1,
        },

};

static isp_v4l2_fmt_t ext_supported_formats[] =
    {
        {
            .name = "ARGB30",
            .fourcc = ISP_V4L2_PIX_FMT_ARGB2101010,
            .depth = 32,
            .is_yuv = false,
            .planes = 1,
        },
#if ISP_HAS_META_CB
        {
            .name = "META",
            .fourcc = ISP_V4L2_PIX_FMT_META,
            .depth = 8,
            .is_yuv = false,
            .planes = 1,
        },
#endif
};

extern uint8_t *isp_kaddr;
extern resource_size_t isp_paddr;

extern void cache_flush(uint32_t buf_start, uint32_t buf_size);

/* ----------------------------------------------------------------
 * temporal frame sync before DDR access is available
 */
#if V4L2_FRAME_ID_SYNC
#define SYNC_FLAG_META ( 1 << 0 )
#define SYNC_FLAG_RAW ( 1 << 1 )
#define SYNC_FLAG_FR ( 1 << 2 )
#define SYNC_FLAG_DS1 ( 1 << 3 )
#define SYNC_FLAG_DS2 ( 1 << 4 )
static spinlock_t sync_slock;
static uint32_t sync_started = 0;
static uint32_t sync_flag = 0;
static uint32_t sync_frame_id = 0;
static uint32_t sync_highest_id = 0;
static uint32_t sync_prev_ctx_id[V4L2_STREAM_TYPE_MAX] = {
    0,
};
static uint32_t sync_done_meta_cnt = 0;

int sync_frame( int stream_type, uint32_t ctx_num, uint32_t fid, uint32_t flag )
{
    unsigned long sflags;
    int rc = -1;

    spin_lock_irqsave( &sync_slock, sflags );
    if ( sync_prev_ctx_id[stream_type] != ctx_num ) {
        sync_highest_id = 0;
        sync_prev_ctx_id[stream_type] = ctx_num;
    }

    if ( sync_flag > 0 ) {
        LOG( LOG_DEBUG, "[Stream#%d] Sync_flag on - sync_highest_id (%d), New fid (%d) / sync_frame_id (%d). sync flag: %x, flag: %x",
             stream_type, sync_highest_id, fid, sync_frame_id, sync_flag, flag);

        if ( fid > sync_highest_id )
            sync_highest_id = fid;

        if ( fid != sync_frame_id || sync_flag & flag ) {
            if (fid != sync_frame_id)
                rc = -2;
            else
                rc = -3;

#if ISP_HAS_META_CB
            if ( stream_type == V4L2_STREAM_TYPE_META && fid > sync_frame_id ) {
                sync_flag &= ~flag;
                /* FIXME: check if need reset the sync_done_meta_cnt*/
                sync_done_meta_cnt = 0;
                if (!sync_flag) {
                    sync_flag |= flag;
                    sync_highest_id = fid;
                    sync_frame_id = fid;
                    rc = 0;
                }
            }
#endif
            spin_unlock_irqrestore( &sync_slock, sflags );
            return rc;
        }
        sync_flag |= flag;
#if ISP_HAS_META_CB
        if (sync_done_meta_cnt > 0) {
            sync_flag &= ~SYNC_FLAG_META;
            sync_done_meta_cnt--;
        }
        if (sync_flag & SYNC_FLAG_META) {
            LOG( LOG_ERR, "[Stream#%d] Sync meta not done - sync_highest_id (%d), New fid (%d) / sync_frame_id (%d). sync flag: %x, flag: %x",
                        stream_type, sync_highest_id, fid, sync_frame_id, sync_flag, flag);
        }
#endif
    } else {
        if ( fid > sync_highest_id ) {
            LOG( LOG_DEBUG, "[Stream#%d] New synced frame started with fid %d", stream_type, fid );
            sync_highest_id = fid;
            sync_frame_id = fid;
            sync_flag |= flag;
        } else {
            LOG( LOG_DEBUG, "[Stream#%d] returning since new fid is not the highest, (id = %d, highest = %d) ",
                 stream_type, fid, sync_highest_id );
            spin_unlock_irqrestore( &sync_slock, sflags );
            return rc;
        }
    }
    spin_unlock_irqrestore( &sync_slock, sflags );

    return 0;
}
#endif


/* ----------------------------------------------------------------
 * Stream callback interface
 */
#if ISP_HAS_META_CB
void callback_meta( uint32_t ctx_num, const void *fw_metadata )
{
    isp_v4l2_stream_t *pstream = NULL;
    isp_v4l2_buffer_t *pbuf = NULL;
    void *vb2_buf = NULL;
    uint32_t frame_id = 0;
    int rc;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
    struct vb2_v4l2_buffer *vvb;
#endif
    struct vb2_buffer *vb;
    unsigned int buf_index;
    unsigned int buf_type;

    //add atomic started here to prevent raise condition with closing
    if ( fw_metadata == NULL ) {
        LOG( LOG_DEBUG, "metadata is null (ctx %d)", ctx_num );
        return;
    }

    /* find stream pointer */
    rc = isp_v4l2_find_stream( &pstream, ctx_num, V4L2_STREAM_TYPE_META );
    if ( rc < 0 ) {
        LOG( LOG_DEBUG, "can't find stream on ctx %d (errno = %d)", ctx_num, rc );
        return;
    }

    /* check if stream is on */
    if ( !pstream->stream_started ) {
        LOG( LOG_DEBUG, "[Stream#%d] stream META is not started yet on ctx %d", pstream->stream_id, ctx_num );
        return;
    }

    /* filter redundant frame id */
    frame_id = *(uint32_t *)fw_metadata;
#if CHECK_METADATA_ID
    if ( pstream->last_frame_id == frame_id ) {
        LOG( LOG_ERR, "[Stream#%d] Redundant frame ID %d on ctx#%d", pstream->stream_id, frame_id, ctx_num );
        return;
    }
#endif
    pstream->last_frame_id = frame_id;
    LOG( LOG_INFO, "[Stream#%d] Meta Frame ID %d.", pstream->stream_id, frame_id );

#if V4L2_FRAME_ID_SYNC
    rc = sync_frame( pstream->stream_type, ctx_num, frame_id, SYNC_FLAG_META );
    if ( rc  < 0 ) {
        LOG( LOG_DEBUG, "sync_frame on ctx %d (errno = %d)", ctx_num, rc );
        return;
    }
#endif

    /* get buffer from vb2 queue  */
    spin_lock( &pstream->slock );
    if ( !list_empty( &pstream->stream_buffer_list ) ) {
        pbuf = list_entry( pstream->stream_buffer_list.next, isp_v4l2_buffer_t, list );
        list_del( &pbuf->list );
    }
    spin_unlock( &pstream->slock );
    if ( !pbuf ) {
#if V4L2_FRAME_ID_SYNC
        LOG( LOG_ERR, "[Stream#%d] No active buffer in queue !", pstream->stream_id );
#endif
        return;
    }
    if ( atomic_inc_and_test( &pstream->running ) ) {
        LOG( LOG_ERR, "[Stream#%d] Already deinited stream !", pstream->stream_id );
        return;
    }

    /* fill buffer */
    pstream->fw_frame_seq_count++;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
    vvb = &pbuf->vvb;
    vb = &vvb->vb2_buf;

    buf_index = vb->index;
    buf_type = vb->type;

    vvb->field = V4L2_FIELD_NONE;
    /* update frame id */
    vvb->sequence = *(uint32_t *)fw_metadata;
#else
    vb = &pbuf->vb;

    buf_index = vb->v4l2_buf.index;
    buf_type = vb->v4l2_buf.type;

    vb->v4l2_buf.field = V4L2_FIELD_NONE;
    /* update frame id */
    vb->v4l2_buf.sequence = *(uint32_t *)fw_metadata;
#endif

    vb2_buf = vb2_plane_vaddr( vb, 0 );
    //
    //mutiplanar?
    if ( buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE )
        memcpy( vb2_buf, fw_metadata, pstream->cur_v4l2_fmt.fmt.pix_mp.plane_fmt[0].sizeimage );
    else
        memcpy( vb2_buf, fw_metadata, pstream->cur_v4l2_fmt.fmt.pix.sizeimage );


    /* Put buffer back to vb2 queue */
    vb2_buffer_done( vb, VB2_BUF_STATE_DONE );

#if V4L2_FRAME_ID_SYNC
    {
        unsigned long sflags;

        spin_lock_irqsave( &sync_slock, sflags );
        sync_done_meta_cnt++;
        spin_unlock_irqrestore( &sync_slock, sflags );
    }
#endif

    /* Notify buffer ready */
    isp_v4l2_notify_event( pstream->stream_id, V4L2_EVENT_ACAMERA_FRAME_READY );

    LOG( LOG_DEBUG, "[Stream#%d] vid_cap buffer %d done (size=%d, m.w=%d, m.h=%d)",
         pstream->stream_id, buf_index,
         pstream->cur_v4l2_fmt.fmt.pix.sizeimage,
         ( (firmware_metadata_t *)vb2_buf )->sensor_width, ( (firmware_metadata_t *)vb2_buf )->sensor_height );

    atomic_set( &pstream->running, 0 );
}
#endif

#if ISP_HAS_RAW_CB
void callback_raw( uint32_t ctx_num, aframe_t *aframe, const metadata_t *metadata, uint8_t exposures_num )
{
    isp_v4l2_stream_t *pstream = NULL;
    struct isp_fw_frame_mgr *frame_mgr;
    unsigned long flags;
    int i, rc;

    LOG( LOG_DEBUG, "[Stream#2] v4l2 callback_raw called" );

    if ( !metadata ) {
        LOG( LOG_ERR, "callback_raw: metadata is NULL" );
        return;
    }

    /* find stream pointer */
    rc = isp_v4l2_find_stream( &pstream, ctx_num, V4L2_STREAM_TYPE_RAW );
    if ( rc < 0 ) {
        LOG( LOG_DEBUG, "can't find stream on ctx %d (errno = %d)", ctx_num, rc );
        return;
    }

    /* check if stream is on */
    if ( !pstream->stream_started ) {
        LOG( LOG_CRIT, "[Stream#%d] stream RAW is not started yet on ctx %d", pstream->stream_id, ctx_num );
        return;
    }

#if CHECK_METADATA_ID
    /* filter redundant frame id */
    if ( pstream->last_frame_id == metadata->frame_id ) {
        LOG( LOG_ERR, "[Stream#%d] Redundant frame ID %d on ctx#%d", pstream->stream_id, metadata->frame_id, ctx_num );
        return;
    }
    pstream->last_frame_id = metadata->frame_id;
#endif

#if V4L2_FRAME_ID_SYNC
    if ( sync_frame( pstream->stream_type, ctx_num, metadata->frame_id, SYNC_FLAG_RAW ) < 0 ) {
        LOG( LOG_CRIT, "[Stream#2] stream RAW sync_frame error" );
        return;
    }
#endif

    frame_mgr = &pstream->frame_mgr;
    int wake_up = 0;

    spin_lock_irqsave( &frame_mgr->frame_slock, flags );
    if ( ISP_FW_FRAME_BUF_INVALID == frame_mgr->frame_buffer.state ) {
        /* lock buffer from firmware */

        /* save current frame  */

        for ( i = 0; i < exposures_num; i++ ) {
            frame_mgr->frame_buffer.addr[i] = aframe[i].address;
            LOG( LOG_INFO, "[Stream#2] v4l2 addresses:0x%x", aframe[i].address );
            aframe[i].status = dma_buf_purge;
        }
        if ( pstream->stream_common->sensor_info.preset[pstream->stream_common->sensor_info.preset_cur].exposures[pstream->stream_common->sensor_info.preset[pstream->stream_common->sensor_info.preset_cur].fps_cur] > exposures_num ) {
            LOG( LOG_CRIT, "V4L2 Raw exposures expecting %d got %d.", pstream->stream_common->sensor_info.preset[pstream->stream_common->sensor_info.preset_cur].exposures[pstream->stream_common->sensor_info.preset[pstream->stream_common->sensor_info.preset_cur].fps_cur], exposures_num );
        }
        frame_mgr->frame_buffer.meta = *metadata;
        frame_mgr->frame_buffer.state = ISP_FW_FRAME_BUF_VALID;

        frame_mgr->frame_buffer.tframe.primary = *aframe;

        /* wake up thread */
        wake_up = 1;
    }
    spin_unlock_irqrestore( &frame_mgr->frame_slock, flags );

    /* wake up the kernel thread to copy the frame data  */
    if ( wake_up )
        wake_up_interruptible( &frame_mgr->frame_wq );

    if ( metadata )
        LOG( LOG_DEBUG, "metadata: width: %u, height: %u, line_size: %u, frame_number: %u.",
             metadata->width, metadata->height, metadata->line_size, metadata->frame_number );

    LOG( LOG_INFO, "[Stream#2] v4l2 callback_raw end" );
}
#endif

void callback_fr( uint32_t ctx_num, tframe_t *tframe, const metadata_t *metadata )
{
    isp_v4l2_stream_t *pstream = NULL;
    struct isp_fw_frame_mgr *frame_mgr;
    unsigned long flags;
    int rc;

    if ( !metadata ) {
        LOG( LOG_ERR, "callback_fr: metadata is NULL" );
        return;
    }

    /* find stream pointer */
    rc = isp_v4l2_find_stream( &pstream, ctx_num, V4L2_STREAM_TYPE_FR );
    if ( rc < 0 ) {
        LOG( LOG_DEBUG, "can't find stream on ctx %d (errno = %d)", ctx_num, rc );
        return;
    }

    /* check if stream is on */
    if ( !pstream->stream_started ) {
        LOG( LOG_DEBUG, "[Stream#%d] stream FR is not started yet on ctx %d", pstream->stream_id, ctx_num );
        return;
    }

#if CHECK_METADATA_ID
    /* filter redundant frame id */
    if ( pstream->last_frame_id == metadata->frame_id ) {
        LOG( LOG_ERR, "[Stream#%d] Redundant frame ID %d on ctx#%d", pstream->stream_id, metadata->frame_id, ctx_num );
        return;
    }
    pstream->last_frame_id = metadata->frame_id;
#endif

#if V4L2_FRAME_ID_SYNC
    rc = sync_frame( pstream->stream_type, ctx_num, metadata->frame_id, SYNC_FLAG_FR );
    if ( rc  < 0 ) {
        LOG( LOG_DEBUG, "sync_frame on ctx %d (errno = %d)", ctx_num, rc );
        return;
    }
#endif

    frame_mgr = &pstream->frame_mgr;
    int wake_up = 0;

    spin_lock_irqsave( &frame_mgr->frame_slock, flags );
    if ( ISP_FW_FRAME_BUF_INVALID == frame_mgr->frame_buffer.state ) {
        /* save current frame  */
        //only 2 planes are possible
        frame_mgr->frame_buffer.addr[0] = tframe->primary.address;
        frame_mgr->frame_buffer.addr[1] = tframe->secondary.address;
        frame_mgr->frame_buffer.meta = *metadata;
        frame_mgr->frame_buffer.state = ISP_FW_FRAME_BUF_VALID;
        frame_mgr->frame_buffer.tframe = *tframe;
        /* lock buffer from firmware */
        tframe->primary.status = dma_buf_purge;
        tframe->secondary.status = dma_buf_purge;

        /* wake up thread */
        wake_up = 1;
    }
    spin_unlock_irqrestore( &frame_mgr->frame_slock, flags );

    /* wake up the kernel thread to copy the frame data  */
    if ( wake_up )
        wake_up_interruptible( &frame_mgr->frame_wq );

    if ( metadata )
        LOG( LOG_DEBUG, "metadata: width: %u, height: %u, line_size: %u, frame_number: %u.",
             metadata->width, metadata->height, metadata->line_size, metadata->frame_number );
}

// Callback from DS1 output pipe
void callback_ds1( uint32_t ctx_num, tframe_t *tframe, const metadata_t *metadata )
{
#if ISP_HAS_DS1
    isp_v4l2_stream_t *pstream = NULL;
    struct isp_fw_frame_mgr *frame_mgr;
    unsigned long flags;
    int rc;

    if ( !metadata ) {
        LOG( LOG_ERR, "callback_ds1: metadata is NULL" );
        return;
    }

    /* find stream pointer */
    rc = isp_v4l2_find_stream( &pstream, ctx_num, V4L2_STREAM_TYPE_DS1 );
    if ( rc < 0 ) {
        LOG( LOG_DEBUG, "can't find stream on ctx %d (errno = %d)", ctx_num, rc );
        return;
    }

    /* check if stream is on */
    if ( !pstream->stream_started ) {
        LOG( LOG_DEBUG, "[Stream#%d] stream DS1 is not started yet on ctx %d", pstream->stream_id, ctx_num );
        return;
    }

#if CHECK_METADATA_ID
    /* filter redundant frame id */
    if ( pstream->last_frame_id == metadata->frame_id ) {
        LOG( LOG_ERR, "[Stream#%d] Redundant frame ID %d on ctx#%d", pstream->stream_id, metadata->frame_id, ctx_num );
        return;
    }
    pstream->last_frame_id = metadata->frame_id;
#endif

#if V4L2_FRAME_ID_SYNC
    rc = sync_frame( pstream->stream_type, ctx_num, metadata->frame_id, SYNC_FLAG_DS1 );
    if ( rc  < 0 ) {
        LOG( LOG_DEBUG, "sync_frame on ctx %d (errno = %d)", ctx_num, rc );
        return;
    }
#endif

    frame_mgr = &pstream->frame_mgr;
    int wake_up = 0;

    spin_lock_irqsave( &frame_mgr->frame_slock, flags );
    if ( ISP_FW_FRAME_BUF_INVALID == frame_mgr->frame_buffer.state ) {
        /* save current frame  */
        //only 2 planes are possible
        frame_mgr->frame_buffer.addr[0] = tframe->primary.address;
        frame_mgr->frame_buffer.addr[1] = tframe->secondary.address;
        frame_mgr->frame_buffer.meta = *metadata;
        frame_mgr->frame_buffer.state = ISP_FW_FRAME_BUF_VALID;
        frame_mgr->frame_buffer.tframe = *tframe;

        /* lock buffer from firmware */
        tframe->primary.status = dma_buf_purge;
        tframe->secondary.status = dma_buf_purge;

        /* wake up thread */
        wake_up = 1;
    }
    spin_unlock_irqrestore( &frame_mgr->frame_slock, flags );

    /* wake up the kernel thread to copy the frame data  */
    if ( wake_up )
        wake_up_interruptible( &frame_mgr->frame_wq );

    if ( metadata )
        LOG( LOG_DEBUG, "metadata: width: %u, height: %u, line_size: %u, frame_number: %u.",
             metadata->width, metadata->height, metadata->line_size, metadata->frame_number );
#endif
}

// Callback from DS2 output pipe
void callback_ds2( uint32_t ctx_num, tframe_t *tframe, const metadata_t *metadata )
{
#if ISP_HAS_DS2
		isp_v4l2_stream_t *pstream = NULL;
		struct isp_fw_frame_mgr *frame_mgr;
		unsigned long flags;
		int rc;

		if ( !metadata ) {
			LOG( LOG_ERR, "callback_ds2: metadata is NULL" );
			return;
		}

		/* find stream pointer */
		rc = isp_v4l2_find_stream( &pstream, ctx_num, V4L2_STREAM_TYPE_DS2 );
		if ( rc < 0 ) {
			LOG( LOG_DEBUG, "can't find stream on ctx %d (errno = %d)", ctx_num, rc );
			return;
		}

		/* check if stream is on */
		if ( !pstream->stream_started ) {
			LOG( LOG_DEBUG, "[Stream#%d] stream DS2 is not started yet on ctx %d", pstream->stream_id, ctx_num );
			return;
		}

#if CHECK_METADATA_ID
		/* filter redundant frame id */
		if ( pstream->last_frame_id == metadata->frame_id ) {
			LOG( LOG_ERR, "[Stream#%d] Redundant frame ID %d on ctx#%d", pstream->stream_id, metadata->frame_id, ctx_num );
			//return;
		}
		pstream->last_frame_id = metadata->frame_id;
#endif

#if 0
#if V4L2_FRAME_ID_SYNC
		if ( sync_frame( pstream->stream_type, ctx_num, metadata->frame_id, SYNC_FLAG_DS2 ) < 0 ) {
			LOG(LOG_ERR, "callback_ds2 sync frame failed");
			return;
		}
#endif
#endif
		frame_mgr = &pstream->frame_mgr;
		int wake_up = 0;

		spin_lock_irqsave( &frame_mgr->frame_slock, flags );
		if ( ISP_FW_FRAME_BUF_INVALID == frame_mgr->frame_buffer.state ) {
			/* save current frame  */
			//only 2 planes are possible
			frame_mgr->frame_buffer.addr[0] = tframe->primary.address;
			frame_mgr->frame_buffer.addr[1] = tframe->secondary.address;
			frame_mgr->frame_buffer.meta = *metadata;
			frame_mgr->frame_buffer.state = ISP_FW_FRAME_BUF_VALID;
			frame_mgr->frame_buffer.tframe = *tframe;

			/* lock buffer from firmware */
			tframe->primary.status = dma_buf_purge;
			tframe->secondary.status = dma_buf_purge;

			/* wake up thread */
			wake_up = 1;
		}
		spin_unlock_irqrestore( &frame_mgr->frame_slock, flags );

		/* wake up the kernel thread to copy the frame data  */
		if ( wake_up )
			wake_up_interruptible( &frame_mgr->frame_wq );

		if ( metadata )
			LOG( LOG_INFO, "metadata: width: %u, height: %u, line_size: %u, frame_number: %u.",
				 metadata->width, metadata->height, metadata->line_size, metadata->frame_number );
#endif

}


/* ----------------------------------------------------------------
 * Stream control interface
 */
/* DDR ioremap parameters
 */
/* sensor static informations */
static isp_v4l2_stream_common g_stream_common;

int isp_v4l2_stream_init_static_resources(struct platform_device *pdev)
{
    isp_v4l2_stream_common *sc = &g_stream_common;
    int i;
    /* initialize stream common field */
    memset( sc, 0, sizeof( isp_v4l2_stream_common ) );
    fw_intf_isp_get_sensor_info( &sc->sensor_info );
    sc->snapshot_sizes.frmsize_num = sc->sensor_info.preset_num;
    for ( i = 0; i < sc->sensor_info.preset_num; i++ ) {
        sc->snapshot_sizes.frmsize[i].width = sc->sensor_info.preset[i].width;
        sc->snapshot_sizes.frmsize[i].height = sc->sensor_info.preset[i].height;
    }

    return 0;
}

void isp_v4l2_stream_deinit_static_resources( struct platform_device *pdev )
{
    return;
}

int isp_v4l2_stream_init( isp_v4l2_stream_t **ppstream, int stream_id )
{
    isp_v4l2_stream_t *new_stream = NULL;
    //int current_sensor_preset;
    LOG( LOG_DEBUG, "[Stream#%d] Initializing stream ...", stream_id );

    /* allocate isp_v4l2_stream_t */
    new_stream = kzalloc( sizeof( isp_v4l2_stream_t ), GFP_KERNEL );
    if ( new_stream == NULL ) {
        LOG( LOG_ERR, "[Stream#%d] Failed to allocate isp_v4l2_stream_t.", stream_id );
        return -ENOMEM;
    }

    /* set default format */

    //all stream multiplanar
    new_stream->cur_v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

    /* set input stream info */
    new_stream->stream_common = &g_stream_common;

    /* init control fields */
    new_stream->stream_id = stream_id;
    new_stream->stream_type = V4L2_STREAM_TYPE_MAX;
    new_stream->stream_started = 0;
    new_stream->last_frame_id = 0xFFFFFFFF;

    //format new stream to default isp settings
    isp_v4l2_stream_try_format( new_stream, &( new_stream->cur_v4l2_fmt ) );

    /* init list */
    INIT_LIST_HEAD( &new_stream->stream_buffer_list );

    /* init locks */
    spin_lock_init( &new_stream->slock );
    spin_lock_init( &new_stream->frame_mgr.frame_slock );

#if V4L2_FRAME_ID_SYNC
    if ( !sync_started ) {
        spin_lock_init( &sync_slock );
        sync_started = 1;
    }
#endif

    /* initialize waitqueue for frame manager */
    init_waitqueue_head( &new_stream->frame_mgr.frame_wq );

    /* return stream private ptr to caller */
    *ppstream = new_stream;

    return 0;
}

void isp_v4l2_stream_deinit( isp_v4l2_stream_t *pstream )
{
    if ( !pstream ) {
        LOG( LOG_ERR, "Null stream passed" );
        return;
    }

    LOG( LOG_INFO, "[Stream#%d] Deinitializing stream ...", pstream->stream_id );

    /* do stream-off first if it's on */
    isp_v4l2_stream_off( pstream );

    /* release fw_info */
    if ( pstream ) {
        kzfree( pstream );
        pstream = NULL;
    }
}

void isp_v4l2_stream_fill_buf( isp_v4l2_stream_t *pstream, isp_v4l2_buffer_t *buf, uint32_t *hw_buf_offset )
{
    unsigned int img_frame_size;
    struct timeval begin, end;
    unsigned long interval_val;
    void *vbuf;
    uint8_t *ddr_mem = NULL;
    uint32_t map_size = 0;
    resource_size_t paddr = 0;
    int i;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
    struct vb2_v4l2_buffer *vvb;
#endif
    struct vb2_buffer *vb;

    if ( !pstream ) {
        LOG( LOG_ERR, "ISP FW not inited yet" );
        return;
    }


    LOG( LOG_DEBUG, "[Stream#%d] Enter: pstream->fw_frame_seq_count: %u.", pstream->stream_id, pstream->fw_frame_seq_count );

    pstream->fw_frame_seq_count++;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
    vvb = &buf->vvb;
    vb = &vvb->vb2_buf;

    vvb->field = V4L2_FIELD_NONE;
#else
    vb = &buf->vb;

    vb->v4l2_buf.field = V4L2_FIELD_NONE;
#endif

    do_gettimeofday( &begin );

    ddr_mem = (uint8_t *)isp_kaddr;
    paddr = isp_paddr;
    map_size = ISP_DDR_SIZE;
#if ISP_HAS_RAW_CB
#if JUNO_DIRECT_DDR_ACCESS
    if ( pstream->stream_type != V4L2_STREAM_TYPE_RAW ) {
        ddr_mem = juno_ddr_mem;
        map_size = JUNO_DDR_SIZE;
    }
#endif
#endif


#if ISP_HAS_RAW_CB
    /* stop DMA_FE to prevent tearing */
    if ( pstream->stream_type == V4L2_STREAM_TYPE_RAW ) {
        fw_intf_stream_pause( pstream->stream_type, 1 );
    }
#endif

    if ( ddr_mem ) {

        if ( pstream->cur_v4l2_fmt.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ) {
            for ( i = 0; i < pstream->cur_v4l2_fmt.fmt.pix_mp.num_planes; i++ ) {
                vbuf = vb2_plane_vaddr( vb, i );
                img_frame_size = pstream->cur_v4l2_fmt.fmt.pix_mp.plane_fmt[i].sizeimage;
                LOG( LOG_INFO, "copying for plane %u %u dest:%p src:%p addr:0x%x", i, img_frame_size, vbuf, ddr_mem + hw_buf_offset[i], hw_buf_offset[i] );
                memcpy( vbuf, ddr_mem + hw_buf_offset[i] - paddr, img_frame_size );
            }
        } else if ( pstream->cur_v4l2_fmt.type == V4L2_BUF_TYPE_VIDEO_CAPTURE ) {
            vbuf = vb2_plane_vaddr( vb, 0 );
            img_frame_size = pstream->cur_v4l2_fmt.fmt.pix.sizeimage;
            LOG( LOG_INFO, "copying for single plane %u", img_frame_size );
            memcpy( vbuf, ddr_mem + hw_buf_offset[0] - paddr, img_frame_size );
        } else {
            LOG( LOG_ERR, "v4l2 bufer format not supported" );
        }
    } else {
        LOG( LOG_ERR, "[Stream#%d] Error: ddr_mem is NULL.", pstream->stream_id );
    }

#if ISP_HAS_RAW_CB
    /* stop DMA_FE to prevent tearing */
    if ( pstream->stream_type == V4L2_STREAM_TYPE_RAW ) {
        fw_intf_stream_pause( pstream->stream_type, 0 );
    }
#endif

    do_gettimeofday( &end );
    /* Get milliseconds interval */
    interval_val = ( end.tv_sec - begin.tv_sec ) * 1000;
    interval_val += ( ( end.tv_usec - begin.tv_usec ) / 1000 );
    LOG( LOG_DEBUG, "[Stream#%d] copy time is: %lu.", pstream->stream_id, interval_val );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
    vb->timestamp = ktime_get_ns();
#else
    v4l2_get_timestamp( &vb->v4l2_buf.timestamp );
#endif
}

static int isp_v4l2_stream_copy_thread( void *data )
{
    isp_v4l2_stream_t *pstream = data;
    isp_fw_frame_mgr_t *frame_mgr;
    unsigned long flags;

    metadata_t meta;
    tframe_t tframe;
    unsigned int idx_tmp = 0;
    isp_v4l2_buffer_t *pbuf = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
    struct vb2_v4l2_buffer *vvb;
#endif
    struct vb2_buffer *vb;
    unsigned int buf_index;
    void *s_list = NULL;
    void *t_list = NULL;

    if ( !pstream ) {
        LOG( LOG_ERR, "Null stream passed" );
        return -EINVAL;
    }

    LOG( LOG_INFO, "[Stream#%d] Enter HW thread.", pstream->stream_id );

    frame_mgr = &pstream->frame_mgr;
    set_freezable();

    for ( ;; ) {
        try_to_freeze();

        if ( kthread_should_stop() )
            break;

        /* wait for new frame to come  */
        if ( wait_event_interruptible_timeout( frame_mgr->frame_wq,
            (ISP_FW_FRAME_BUF_VALID == frame_mgr->frame_buffer.state),
            msecs_to_jiffies( 10 ) ) < 0 ) {
            LOG( LOG_ERR, "[Stream#%d] Error: wait_event return < 0", pstream->stream_id );
            continue;
        }

        /* get a new frame from ISP FW  */
        spin_lock_irqsave( &frame_mgr->frame_slock, flags );
        /* get current frame only its state is VALID */
        if ( ISP_FW_FRAME_BUF_VALID == frame_mgr->frame_buffer.state ) {
            meta = frame_mgr->frame_buffer.meta;
            tframe = frame_mgr->frame_buffer.tframe;
            frame_mgr->frame_buffer.state = ISP_FW_FRAME_BUF_INVALID;
        } else {
            spin_unlock_irqrestore( &frame_mgr->frame_slock, flags );
            continue;
        }
        spin_unlock_irqrestore( &frame_mgr->frame_slock, flags );

        /* try to get an active buffer from vb2 queue  */
        pbuf = NULL;
        spin_lock( &pstream->slock );
        t_list = tframe.list;

        s_list = NULL;

        list_for_each_entry(pbuf, &pstream->stream_buffer_list, list) {
            s_list = (void *)&pbuf->list;

            if (s_list == t_list) {
                list_del_init(s_list);
                break;
            }
        }

        if ((s_list != t_list) || (t_list == NULL) || (s_list == NULL)) {
            LOG(LOG_ERR, "[Stream#%d] Failed to find vb2 buffer on stream buffer list, s_list:%p, t_list:%p", pstream->stream_id, s_list, t_list);
            spin_unlock( &pstream->slock );
#if V4L2_FRAME_ID_SYNC
            {
                unsigned long sflags;

                spin_lock_irqsave( &sync_slock, sflags );
#if ISP_HAS_RAW_CB
                if ( pstream->stream_type == V4L2_STREAM_TYPE_RAW ) {
                    LOG( LOG_DEBUG, "[Stream#%d] releasing RAW sync flag", pstream->stream_id );
                    sync_flag &= ~SYNC_FLAG_RAW;
                } else
#endif
#if ISP_HAS_DS1
                if ( pstream->stream_type == V4L2_STREAM_TYPE_DS1 ) {
                    LOG( LOG_DEBUG, "[Stream#%d] releasing DS1 sync flag", pstream->stream_id );
                    sync_flag &= ~SYNC_FLAG_DS1;
                } else
#endif
#if ISP_HAS_DS2
                if ( pstream->stream_type == V4L2_STREAM_TYPE_DS2 ) {
                    LOG( LOG_DEBUG, "[Stream#%d] releasing DS2 sync flag", pstream->stream_id );
                    sync_flag &= ~SYNC_FLAG_DS2;
                } else
#endif
#if ISP_HAS_META_CB
                if (pstream->stream_type == V4L2_STREAM_TYPE_META) {
                    LOG( LOG_DEBUG, "[Stream#%d] releasing Meta sync flag", pstream->stream_id );
                    sync_flag &= ~SYNC_FLAG_META;
                    sync_done_meta_cnt = 0;
                } else
#endif
                if ( pstream->stream_type == V4L2_STREAM_TYPE_FR ) {
                    LOG( LOG_DEBUG, "[Stream#%d] releasing FR  sync flag", pstream->stream_id );
                    sync_flag &= ~SYNC_FLAG_FR;
                }
                spin_unlock_irqrestore( &sync_slock, sflags );
            }
#endif
            continue;
        }
        spin_unlock( &pstream->slock );

        cache_flush(tframe.primary.address, tframe.primary.size + tframe.secondary.size);

        vvb = &pbuf->vvb;
        vb = &vvb->vb2_buf;

        buf_index = vb->index;

        vvb->sequence = meta.frame_id;
        vvb->field = V4L2_FIELD_NONE;
        vb->timestamp = ktime_get_ns();
        pstream->fw_frame_seq_count++;

        /* Fill buffer */
        LOG( LOG_DEBUG, "[Stream#%d] filled buffer %d with frame_buf_idx: %d.",
             pstream->stream_id, buf_index, idx_tmp );

        /* Put buffer back to vb2 queue */
        vb2_buffer_done( vb, VB2_BUF_STATE_DONE );
        LOG( LOG_INFO, "[Stream#%d] vid_cap buffer %d done frame_id:%d",
             pstream->stream_id, buf_index, meta.frame_id );

        /* Notify buffer ready */
        isp_v4l2_notify_event( pstream->stream_id, V4L2_EVENT_ACAMERA_FRAME_READY );

#if V4L2_FRAME_ID_SYNC
        {
            unsigned long sflags;

            spin_lock_irqsave( &sync_slock, sflags );
#if ISP_HAS_RAW_CB
            if ( pstream->stream_type == V4L2_STREAM_TYPE_RAW ) {
                LOG( LOG_DEBUG, "[Stream#%d] releasing RAW sync flag", pstream->stream_id );
                sync_flag &= ~SYNC_FLAG_RAW;
            } else
#endif
#if ISP_HAS_DS1
            if ( pstream->stream_type == V4L2_STREAM_TYPE_DS1 ) {
                LOG( LOG_DEBUG, "[Stream#%d] releasing DS1 sync flag", pstream->stream_id );
                sync_flag &= ~SYNC_FLAG_DS1;
            } else
#endif
#if ISP_HAS_DS2
            if ( pstream->stream_type == V4L2_STREAM_TYPE_DS2 ) {
                LOG( LOG_DEBUG, "[Stream#%d] releasing DS2 sync flag", pstream->stream_id );
                sync_flag &= ~SYNC_FLAG_DS2;
            } else
#endif
#if ISP_HAS_META_CB
            if (pstream->stream_type == V4L2_STREAM_TYPE_META) {
                LOG( LOG_DEBUG, "[Stream#%d] releasing Meta sync flag", pstream->stream_id );
                sync_flag &= ~SYNC_FLAG_META;
                sync_done_meta_cnt = 0;
            } else
#endif
            if ( pstream->stream_type == V4L2_STREAM_TYPE_FR ) {
                LOG( LOG_DEBUG, "[Stream#%d] releasing FR  sync flag", pstream->stream_id );
                sync_flag &= ~SYNC_FLAG_FR;
            }
            spin_unlock_irqrestore( &sync_slock, sflags );
        }
#endif

    }

    /* Notify stream off */
    isp_v4l2_notify_event( pstream->stream_id, V4L2_EVENT_ACAMERA_STREAM_OFF );

    LOG( LOG_INFO, "[Stream#%d] Exit HW thread.", pstream->stream_id );

    return 0;
}

int isp_v4l2_stream_on( isp_v4l2_stream_t *pstream )
{
    if ( !pstream ) {
        LOG( LOG_ERR, "Null stream passed" );
        return -EINVAL;
    }

    LOG( LOG_DEBUG, "[Stream#%d] called", pstream->stream_id );

#if V4L2_FRAME_ID_SYNC
    {
        unsigned long sflags;

        spin_lock_irqsave( &sync_slock, sflags );
#if ISP_HAS_RAW_CB
        if ( pstream->stream_type == V4L2_STREAM_TYPE_RAW ) {
            LOG( LOG_DEBUG, "[Stream#%d] releasing RAW sync flag", pstream->stream_id );
            sync_flag &= ~SYNC_FLAG_RAW;
        } else
#endif
#if ISP_HAS_DS1
        if (pstream->stream_type == V4L2_STREAM_TYPE_DS1) {
            LOG( LOG_DEBUG, "[Stream#%d] releasing DS1 sync flag", pstream->stream_id );
            sync_flag &= ~SYNC_FLAG_DS1;
        } else
#endif
#if ISP_HAS_DS2
        if (pstream->stream_type == V4L2_STREAM_TYPE_DS2) {
            LOG( LOG_DEBUG, "[Stream#%d] releasing DS2 sync flag", pstream->stream_id );
            sync_flag &= ~SYNC_FLAG_DS2;
        } else
#endif
#if ISP_HAS_META_CB
        if (pstream->stream_type == V4L2_STREAM_TYPE_META) {
            LOG( LOG_DEBUG, "[Stream#%d] releasing Meta sync flag", pstream->stream_id );
            sync_flag &= ~SYNC_FLAG_META;
            sync_done_meta_cnt = 0;
        } else
#endif
        if (pstream->stream_type == V4L2_STREAM_TYPE_FR) {
            LOG( LOG_DEBUG, "[Stream#%d] releasing FR  sync flag", pstream->stream_id );
            sync_flag &= ~SYNC_FLAG_FR;
        }
        spin_unlock_irqrestore( &sync_slock, sflags );
    }
#endif

/* for now, we need memcpy */
#if ISP_HAS_META_CB
    if ( pstream->stream_type != V4L2_STREAM_TYPE_META )
#endif
    {
        /* Resets frame counters */
        pstream->fw_frame_seq_count = 0;

        /* launch copy thread */
        pstream->kthread_stream = kthread_run( isp_v4l2_stream_copy_thread, pstream, "isp-stream-%d", pstream->stream_id );
        if ( IS_ERR( pstream->kthread_stream ) ) {
            LOG( LOG_ERR, "[Stream#%d] create kernel_thread() failed", pstream->stream_id );
            return PTR_ERR( pstream->kthread_stream );
        }
        LOG( LOG_INFO, "[Stream#%d] stream_thread pid: %u", pstream->stream_id, pstream->kthread_stream->pid );

    }
#if ISP_HAS_META_CB
    else { //metadata has no thread
        atomic_set( &pstream->running, 0 );
    }
#endif

    /* hardware stream on */
    if ( fw_intf_stream_start( pstream->stream_type ) < 0 )
        return -1;

    /* control fields update */
    pstream->stream_started = 1;

    return 0;
}

void isp_v4l2_stream_off( isp_v4l2_stream_t *pstream )
{
    isp_v4l2_buffer_t *buf;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
    struct vb2_v4l2_buffer *vvb;
#endif
    struct vb2_buffer *vb;
    unsigned int buf_index;

    if ( !pstream ) {
        LOG( LOG_ERR, "Null stream passed" );
        return;
    }

    LOG( LOG_INFO, "[Stream#%d] called", pstream->stream_id );

    fw_intf_stream_stop( pstream->stream_type );

    /* shutdown control thread */
    if ( pstream->kthread_stream != NULL ) {
        kthread_stop( pstream->kthread_stream );
        pstream->kthread_stream = NULL;
    }
#if ISP_HAS_META_CB
    else if ( pstream->stream_type == V4L2_STREAM_TYPE_META ) {
        while ( atomic_read( &pstream->running ) > 0 ) { //metadata has no thread
            LOG( LOG_INFO, "[Stream#%d] still running %d !", pstream->stream_id, atomic_read( &pstream->running ) );
            schedule();
        }
        atomic_set( &pstream->running, -1 );
    }
#endif
    /* Release all active buffers */
    spin_lock( &pstream->slock );
    while ( !list_empty( &pstream->stream_buffer_list ) ) {
        buf = list_entry( pstream->stream_buffer_list.next,
                          isp_v4l2_buffer_t, list );
        list_del( &buf->list );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
        vvb = &buf->vvb;
        vb = &vvb->vb2_buf;

        buf_index = vb->index;
#else
        vb = &buf->vb;

        buf_index = vb->v4l2_buf.index;
#endif

        vb2_buffer_done( vb, VB2_BUF_STATE_ERROR );

        LOG( LOG_INFO, "[Stream#%d] vid_cap buffer %d done",
             pstream->stream_id, buf_index );
    }
    spin_unlock( &pstream->slock );

    // control fields update

    pstream->stream_started = 0;
}


/* ----------------------------------------------------------------
 * Stream configuration interface
 */
static isp_v4l2_fmt_t *isp_v4l2_stream_find_format( uint32_t pixelformat )
{
    isp_v4l2_fmt_t *fmt;
    unsigned int i;

    for ( i = 0; i < ARRAY_SIZE( isp_v4l2_supported_formats ); i++ ) {
        fmt = &isp_v4l2_supported_formats[i];

        if ( fmt->fourcc == pixelformat )
            return fmt;
    }

    return NULL;
}

int isp_v4l2_stream_enum_framesizes( isp_v4l2_stream_t *pstream, struct v4l2_frmsizeenum *fsize )
{
    LOG( LOG_CRIT, "[Stream#%d] index: %d, pixel_format: 0x%x.\n",
         pstream->stream_id, fsize->index, fsize->pixel_format );

    if ( !isp_v4l2_stream_find_format( fsize->pixel_format ) )
        return -EINVAL;

    /* check index */
    if ( fsize->index >= pstream->stream_common->snapshot_sizes.frmsize_num ) {
        LOG( LOG_INFO, "[Stream#%d] index (%d) should be smaller than %lu.",
             pstream->stream_id, fsize->index, pstream->stream_common->snapshot_sizes.frmsize_num );
        return -EINVAL;
    }


    /* return framesize */
    fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
    fsize->discrete = pstream->stream_common->snapshot_sizes.frmsize[fsize->index];

    return 0;
}

int isp_v4l2_stream_enum_format( isp_v4l2_stream_t *pstream, struct v4l2_fmtdesc *f )
{
    const isp_v4l2_fmt_t *fmt;
    int desc_size = 0;

    /* check index */
    if ( f->index >= ARRAY_SIZE( isp_v4l2_supported_formats ) ) {
        LOG( LOG_INFO, "[Stream#%d] index (%d) should be smaller than %lu.",
             pstream->stream_id, f->index, ARRAY_SIZE( isp_v4l2_supported_formats ) );
        return -EINVAL;
    }

    /* get format from index */
    fmt = &isp_v4l2_supported_formats[f->index];

    /* check description length */
    if ( sizeof( fmt->name ) > sizeof( f->description ) )
        desc_size = sizeof( f->description );
    else
        desc_size = sizeof( fmt->name );

    /* reset flag */
    f->flags = 0;

    /* copy description */
    strlcpy( f->description, fmt->name, desc_size );

    /* copy format code */
    f->pixelformat = fmt->fourcc;

    LOG( LOG_INFO, "[Stream#%d] index: %d, format: 0x%x, desc: %s.\n",
         pstream->stream_id, f->index, f->pixelformat, f->description );

    return 0;
}

int isp_v4l2_stream_try_format( isp_v4l2_stream_t *pstream, struct v4l2_format *f )
{
    isp_v4l2_fmt_t *tfmt;
    int i;
    LOG( LOG_INFO, "[Stream#%d] try fmt type: %u, pixelformat: 0x%x, width: %u, height: %u.\n",
         pstream->stream_id, f->type, f->fmt.pix_mp.pixelformat, f->fmt.pix_mp.width, f->fmt.pix_mp.height );

#if ISP_HAS_META_CB
    if ((pstream->stream_id == V4L2_STREAM_TYPE_META)
        && (f->fmt.pix_mp.pixelformat != ISP_V4L2_PIX_FMT_META)) {
        LOG(LOG_ERR, "[Stream#%d] pixel format for meta port must be 0x%08x, not 0x%08x. Correct it!!",
             pstream->stream_id, ISP_V4L2_PIX_FMT_META, f->fmt.pix_mp.pixelformat);
        f->fmt.pix_mp.pixelformat = ISP_V4L2_PIX_FMT_META;
    }
#endif

    /* check format and modify */
    if (f->fmt.pix_mp.pixelformat == ISP_V4L2_PIX_FMT_META) {
        tfmt = &ext_supported_formats[1];
    } else {
        tfmt = isp_v4l2_stream_find_format( f->fmt.pix_mp.pixelformat );
        if ( !tfmt ) {
            LOG( LOG_CRIT, "[Stream#%d] format 0x%08x is not supported, setting default format 0x%08x.\n",
                 pstream->stream_id, f->fmt.pix.pixelformat, ISP_DEFAULT_FORMAT );
            f->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            f->fmt.pix_mp.pixelformat = ISP_DEFAULT_FORMAT;
            tfmt = isp_v4l2_stream_find_format( f->fmt.pix_mp.pixelformat );
        }
    }
    //corect the exposure number here
    if ( tfmt->fourcc == V4L2_PIX_FMT_SBGGR16 ) {
        uint32_t spreset = 0, exposures_preset, rev_val;

        if ( fw_intf_is_isp_started() ) {
            acamera_command( TSENSOR, SENSOR_PRESET, 0, COMMAND_GET, &spreset );
            acamera_command( TSENSOR, SENSOR_INFO_PRESET, spreset, COMMAND_SET, &rev_val );
            acamera_command( TSENSOR, SENSOR_INFO_EXPOSURES, 0, COMMAND_GET, &exposures_preset );

            LOG( LOG_INFO, "[Stream#%d] Changing the number of planes according preset %d to exposures %d=>%d.\n", pstream->stream_id, spreset, tfmt->planes, exposures_preset );
            tfmt->planes = exposures_preset;
        } else {
            tfmt->planes = 1;
        }
    }
/* adjust width, height for META stream */
#if ISP_HAS_META_CB
    if ( f->fmt.pix.pixelformat == ISP_V4L2_PIX_FMT_META ) {
        f->fmt.pix.width = ISP_V4L2_METADATA_SIZE;
        f->fmt.pix.height = 1;
    } else
#endif
    {
        if ( f->fmt.pix.width == 0 || f->fmt.pix.height == 0 ) {
            if ( fw_intf_is_isp_started() ) {
                uint32_t width_cur, height_cur;
                acamera_command( TSENSOR, SENSOR_WIDTH, 0, COMMAND_GET, &width_cur );
                acamera_command( TSENSOR, SENSOR_HEIGHT, 0, COMMAND_GET, &height_cur );
                f->fmt.pix.width = width_cur;
                f->fmt.pix.height = height_cur;
            } else {
                f->fmt.pix.width = 1920;
                f->fmt.pix.height = 1080;
            }
        }
        v4l_bound_align_image( &f->fmt.pix.width, 48, ISP_V4L2_MAX_WIDTH, 2,
                               &f->fmt.pix.height, 32, ISP_V4L2_MAX_HEIGHT, 0, 0 );
    }

    f->fmt.pix.field = V4L2_FIELD_NONE;


    //all stream multiplanar
    f->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    f->fmt.pix_mp.num_planes = tfmt->planes;
    f->fmt.pix_mp.colorspace = ( tfmt->is_yuv ) ? V4L2_COLORSPACE_SMPTE170M : V4L2_COLORSPACE_SRGB;
    for ( i = 0; i < tfmt->planes; i++ ) {
        if ( tfmt->fourcc == V4L2_PIX_FMT_NV12 && i == 1) {
            f->fmt.pix_mp.plane_fmt[i].bytesperline = ( ( ( f->fmt.pix_mp.width * tfmt->depth / 8 ) + 127 ) >> 7 ) << 7; // for padding
            f->fmt.pix_mp.plane_fmt[i].sizeimage = f->fmt.pix_mp.height * f->fmt.pix_mp.plane_fmt[i].bytesperline / 2;
        } else {
            f->fmt.pix_mp.plane_fmt[i].bytesperline = ( ( ( f->fmt.pix_mp.width * tfmt->depth / 8 ) + 127 ) >> 7 ) << 7; // for padding
            f->fmt.pix_mp.plane_fmt[i].sizeimage = f->fmt.pix_mp.height * f->fmt.pix_mp.plane_fmt[i].bytesperline;
        }
        memset( f->fmt.pix_mp.plane_fmt[i].reserved, 0, sizeof( f->fmt.pix_mp.plane_fmt[i].reserved ) );
        memset( f->fmt.pix_mp.reserved, 0, sizeof( f->fmt.pix_mp.reserved ) );
    }

    return 0;
}

int isp_v4l2_stream_get_format( isp_v4l2_stream_t *pstream, struct v4l2_format *f )
{
    if ( !pstream ) {
        LOG( LOG_ERR, "Null stream passed" );
        return -EINVAL;
    }

    *f = pstream->cur_v4l2_fmt;

    LOG( LOG_INFO, "[Stream#%d]   - GET fmt - width: %4u, height: %4u, format: 0x%x.",
         pstream->stream_id,
         f->fmt.pix_mp.width,
         f->fmt.pix_mp.height,
         f->fmt.pix_mp.pixelformat );

    if ( f->fmt.pix_mp.width == 0 || f->fmt.pix_mp.height == 0 || f->fmt.pix_mp.pixelformat == 0 ) { //not formatted yet
        LOG( LOG_CRIT, "Compliance error, uninitialized format" );
    }

    return 0;
}

int isp_v4l2_stream_set_format( isp_v4l2_stream_t *pstream, struct v4l2_format *f )
{
    int rc = 0;

    if ( !pstream ) {
        LOG( LOG_ERR, "Null stream passed" );
        return -EINVAL;
    }

    LOG( LOG_INFO, "[Stream#%d] VIDIOC_S_FMT operation", pstream->stream_id );

    LOG( LOG_CRIT, "[Stream#%d]   - SET fmt - width: %4u, height: %4u, format: 0x%x.",
         pstream->stream_id,
         f->fmt.pix_mp.width,
         f->fmt.pix_mp.height,
         f->fmt.pix_mp.pixelformat );


    /* try format first */
    isp_v4l2_stream_try_format( pstream, f );

    /* set stream type*/
    switch ( f->fmt.pix.pixelformat ) {
        case V4L2_PIX_FMT_RGB32:
        case ISP_V4L2_PIX_FMT_ARGB2101010:
        case V4L2_PIX_FMT_RGB24:
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_SBGGR16:
        case V4L2_PIX_FMT_YUV444:
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
        case V4L2_PIX_FMT_GREY:
            pstream->stream_type = pstream->stream_id;
            break;
#if ISP_HAS_META_CB
        case ISP_V4L2_PIX_FMT_META:
            pstream->stream_type = V4L2_STREAM_TYPE_META;
            break;
#endif
        default:
            LOG( LOG_ERR, "Shouldn't be here after try_format()." );
            return -EINVAL;
    }

    /* update resolution */
    rc = fw_intf_stream_set_resolution( &pstream->stream_common->sensor_info,
                                        pstream->stream_type, &( f->fmt.pix_mp.width ), &( f->fmt.pix_mp.height ) );
    if ( rc < 0 ) {
        LOG( LOG_CRIT, "set resolution failed ! (rc = %d)", rc );
        return rc;
    }

    LOG( LOG_INFO, "[Stream#%d] Current preset:%d Exposures for this settings %d",
         pstream->stream_id,
         pstream->stream_common->sensor_info.preset_cur,
         pstream->stream_common->sensor_info.preset[pstream->stream_common->sensor_info.preset_cur].exposures[pstream->stream_common->sensor_info.preset[pstream->stream_common->sensor_info.preset_cur].fps_cur] );

    /* update format */
    rc = fw_intf_stream_set_output_format( pstream->stream_type, f->fmt.pix_mp.pixelformat );
    if ( rc < 0 ) {
        LOG( LOG_CRIT, "set format failed ! (rc = %d)", rc );
        return rc;
    }

    /* update format field */
    pstream->cur_v4l2_fmt = *f;


    LOG( LOG_CRIT, "[Stream#%d]   - New fmt - width: %4u, height: %4u, format: 0x%x, type: %5u. ",
         pstream->stream_id,
         pstream->cur_v4l2_fmt.fmt.pix_mp.width,
         pstream->cur_v4l2_fmt.fmt.pix_mp.height,
         pstream->cur_v4l2_fmt.fmt.pix_mp.pixelformat,
         pstream->cur_v4l2_fmt.type );

    return 0;
}

static uint32_t isp_v4l2_get_gcd(uint32_t width, uint32_t height)
{
    uint32_t gcd = 0;
    uint32_t tmp = 0;
    uint32_t wd = 0;
    uint32_t ht = 0;

    if (width == 0 || height == 0) {
        LOG(LOG_ERR, "Error input param");
        return gcd;
    }

    wd = width;
    ht = height;

    while (ht != 0) {
        tmp = wd % ht;
        wd = ht;
        ht = tmp;
    }

    gcd = wd;

    return gcd;
}

static int isp_v4l2_get_crop_type(isp_v4l2_stream_t *pstream)
{
    int s_type = -1;

    if (pstream == NULL) {
        LOG(LOG_ERR, "Error input param");
        return s_type;
    }

    switch (pstream->stream_type) {
    case V4L2_STREAM_TYPE_FR:
        s_type = CROP_FR;
    break;
    case V4L2_STREAM_TYPE_DS1:
        s_type = CROP_DS;
    break;
    default:
        LOG(LOG_ERR, "Error: this stream cannot surport crop");
    break;
    }

    return s_type;
}

int isp_v4l2_get_cropcap(isp_v4l2_stream_t *pstream,
                        struct v4l2_cropcap *cap)
{
    int ret = -1;
    uint32_t width_cur = 0;
    uint32_t height_cur = 0;
    uint32_t gcd = 0;
    uint32_t ret_val = 0;

    acamera_command( TSENSOR, SENSOR_WIDTH, 0, COMMAND_GET, &width_cur );
    acamera_command( TSENSOR, SENSOR_HEIGHT, 0, COMMAND_GET, &height_cur );

    if (width_cur == 0 || height_cur == 0) {
        LOG(LOG_ERR, "Error sensor current resolution");
        return ret;
    }

    cap->bounds.top = 0;
    cap->bounds.left = 0;
    cap->bounds.width = width_cur;
    cap->bounds.height = height_cur;

    cap->defrect.top = 0;
    cap->defrect.left = 0;
    if (pstream->stream_type ==  V4L2_STREAM_TYPE_DS1) {
        acamera_command( TIMAGE, IMAGE_RESIZE_TYPE_ID,
                            SCALER, COMMAND_SET, &ret_val );
        acamera_command(TIMAGE, IMAGE_RESIZE_WIDTH_ID, 0,
                            COMMAND_GET, &ret_val);
        cap->defrect.width = ret_val;

        acamera_command(TIMAGE, IMAGE_RESIZE_HEIGHT_ID, 0,
                            COMMAND_GET, &ret_val);
        cap->defrect.height = ret_val;
    } else {
        cap->defrect.width = width_cur;
        cap->defrect.height = height_cur;
    }
    gcd = isp_v4l2_get_gcd(width_cur, height_cur);
    if (gcd == 0) {
        LOG(LOG_ERR, "Error get gcd");
        return 0;
    }

    cap->pixelaspect.numerator = width_cur / gcd;
    cap->pixelaspect.denominator = height_cur / gcd;

    return 0;
}

int isp_v4l2_set_crop(isp_v4l2_stream_t *pstream,
                            const struct v4l2_crop *crop)
{
    uint8_t result = 0;
    uint32_t ret_val = 0;
    int s_type = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t x_off = 0;
    uint32_t y_off = 0;

    if (pstream == NULL || crop == NULL) {
        LOG(LOG_ERR, "Error input param");
        return -1;
    }

    s_type = isp_v4l2_get_crop_type(pstream);

    if (s_type < 0) {
        LOG(LOG_ERR, "Error get stream type");
        return s_type;
    }

    x_off = crop->c.left & 0xffff;
    y_off = crop->c.top & 0xffff;
    width = crop->c.width & 0xffff;
    height = crop->c.height & 0xffff;

    result = acamera_command( TIMAGE, IMAGE_RESIZE_TYPE_ID,
                            s_type, COMMAND_SET, &ret_val );
    if (result) {
        LOG( LOG_CRIT, "Failed to set resize_type, ret_value: %d.", result );
        return result;
    }

    result = acamera_command(TIMAGE, IMAGE_CROP_XOFFSET_ID,
                            x_off, COMMAND_SET, &ret_val);
    if (result) {
        LOG(LOG_CRIT, "Failed to set x offset, ret_value: %d.", result);
        return result;
    }

    result = acamera_command(TIMAGE, IMAGE_RESIZE_WIDTH_ID,
                            width, COMMAND_SET, &ret_val);
    if ( result ) {
        LOG( LOG_CRIT, "Failed to set resize_width, ret_value: %d.", result );
        return result;
    }

    result = acamera_command(TIMAGE, IMAGE_CROP_YOFFSET_ID,
                            y_off, COMMAND_SET, &ret_val);
    if (result) {
        LOG(LOG_CRIT, "Failed to set y offset, ret_value: %d.", result);
        return result;
    }

    result = acamera_command( TIMAGE, IMAGE_RESIZE_HEIGHT_ID,
                            height, COMMAND_SET, &ret_val );
    if (result) {
        LOG( LOG_CRIT, "Failed to set resize_height, ret_value: %d.", result );
        return result;
    }

    result = acamera_command( TIMAGE, IMAGE_RESIZE_ENABLE_ID,
                            RUN, COMMAND_SET, &ret_val);
    if (result) {
        LOG(LOG_CRIT, "Failed to set resize_enable, ret_value: %d.", result);
        return result;
    }

    if (pstream->stream_type == V4L2_STREAM_TYPE_FR) {
        acamera_command( TAML_SCALER, SCALER_SRC_WIDTH,
                            width, COMMAND_SET, &ret_val );
        acamera_command( TAML_SCALER, SCALER_SRC_HEIGHT,
                            height, COMMAND_SET, &ret_val );
    }

    return result;
}

int isp_v4l2_get_crop(isp_v4l2_stream_t *pstream,
                            struct v4l2_crop *crop)
{
    uint8_t result = 0;
    uint32_t ret_val = 0;
    int s_type = 0;

    if (pstream == NULL || crop == NULL) {
        LOG(LOG_ERR, "Error input param");
        return -1;
    }

    s_type = isp_v4l2_get_crop_type(pstream);

    if (s_type < 0) {
        LOG(LOG_ERR, "Error get stream type");
        return s_type;
    }

    result = acamera_command( TIMAGE, IMAGE_RESIZE_TYPE_ID,
                            s_type, COMMAND_SET, &ret_val );
    if (result) {
        LOG( LOG_CRIT, "Failed to get resize_type, ret_value: %d.", result );
        return result;
    }

    result = acamera_command(TIMAGE, IMAGE_CROP_XOFFSET_ID,
                            0, COMMAND_GET, &ret_val);
    if (result) {
        LOG(LOG_CRIT, "Failed to get x offset, ret_value: %d.", result);
        return result;
    }
    crop->c.left = ret_val;

    result = acamera_command(TIMAGE, IMAGE_CROP_YOFFSET_ID,
                            0, COMMAND_GET, &ret_val);
    if (result) {
        LOG(LOG_CRIT, "Failed to get x offset, ret_value: %d.", result);
        return result;
    }
    crop->c.top = ret_val;

    result = acamera_command(TIMAGE, IMAGE_RESIZE_WIDTH_ID,
                            0, COMMAND_GET, &ret_val);
    if (result) {
        LOG(LOG_CRIT, "Failed to get x offset, ret_value: %d.", result);
        return result;
    }
    crop->c.width = ret_val;

    result = acamera_command(TIMAGE, IMAGE_RESIZE_HEIGHT_ID,
                            0, COMMAND_GET, &ret_val);
    if (result) {
        LOG(LOG_CRIT, "Failed to get x offset, ret_value: %d.", result);
        return result;
    }
    crop->c.height = ret_val;

    return result;
}
