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

#include <linux/videodev2.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-memops.h>

#include "acamera_logger.h"
#include "isp-v4l2-common.h"
#include "isp-v4l2-stream.h"
#include "isp-vb2.h"
#include "system_am_sc.h"
#include "system_am_sc1.h"
#include "system_am_sc2.h"
#include "system_am_sc3.h"
#include "acamera_autocap.h"
/* ----------------------------------------------------------------
 * VB2 operations
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0))
static int isp_vb2_queue_setup( struct vb2_queue *vq,
                                unsigned int *nbuffers, unsigned int *nplanes,
                                unsigned int sizes[], struct device *alloc_devs[] )
#else
static int isp_vb2_queue_setup( struct vb2_queue *vq, const struct v4l2_format *fmt,
                                unsigned int *nbuffers, unsigned int *nplanes,
                                unsigned int sizes[], void *alloc_ctxs[] )
#endif
{
    static unsigned long cnt = 0;
    isp_v4l2_stream_t *pstream = vb2_get_drv_priv( vq );
    struct v4l2_format vfmt;
    //unsigned int size;
    int i;
    LOG( LOG_INFO, "Enter id:%d, cnt: %lu.", pstream->stream_id, cnt++ );

    // get current format
    if ( isp_v4l2_stream_get_format( pstream, &vfmt ) < 0 ) {
        LOG( LOG_ERR, "fail to get format from stream" );
        return -EBUSY;
    }

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0))
    if ( fmt )
        LOG( LOG_INFO, "fmt: %p, width: %u, height: %u, sizeimage: %u.",
             fmt, fmt->fmt.pix.width, fmt->fmt.pix.height, fmt->fmt.pix.sizeimage );
#endif

    if ( vq->num_buffers + *nbuffers < 3 )
        *nbuffers = 3 - vq->num_buffers;

    if ( vfmt.type == V4L2_BUF_TYPE_VIDEO_CAPTURE ) {
        *nplanes = 1;
        sizes[0] = vfmt.fmt.pix.sizeimage;
    } else if ( vfmt.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ) {
        *nplanes = vfmt.fmt.pix_mp.num_planes;
        for ( i = 0; i < vfmt.fmt.pix_mp.num_planes; i++ ) {
            sizes[i] = vfmt.fmt.pix_mp.plane_fmt[i].sizeimage;
            LOG( LOG_INFO, "size: %u", sizes[i] );
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0))
            alloc_ctxs[i] = 0;
#endif
        }
    } else {
        LOG( LOG_ERR, "Wrong type: %u", vfmt.type );
        return -EINVAL;
    }

    /*
     * videobuf2-vmalloc allocator is context-less so no need to set
     * alloc_ctxs array.
     */

    return 0;
}

static int isp_vb2_buf_prepare( struct vb2_buffer *vb )
{
    unsigned long size;
    isp_v4l2_stream_t *pstream = vb2_get_drv_priv( vb->vb2_queue );
    static unsigned long cnt = 0;
    struct v4l2_format vfmt;
    int i;
    LOG( LOG_DEBUG, "Enter id:%d, cnt: %lu.", pstream->stream_id, cnt++ );

    // get current format
    if ( isp_v4l2_stream_get_format( pstream, &vfmt ) < 0 ) {
        LOG( LOG_ERR, "fail to get format from stream" );
        return -EBUSY;
    }

    if ( vfmt.type == V4L2_BUF_TYPE_VIDEO_CAPTURE ) {
        size = vfmt.fmt.pix.sizeimage;

        if ( vb2_plane_size( vb, 0 ) < size ) {
            LOG( LOG_ERR, "buffer too small (%lu < %lu)", vb2_plane_size( vb, 0 ), size );
            return -EINVAL;
        }

        vb2_set_plane_payload( vb, 0, size );
    } else if ( vfmt.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ) {
        for ( i = 0; i < vfmt.fmt.pix_mp.num_planes; i++ ) {
            size = vfmt.fmt.pix_mp.plane_fmt[i].sizeimage;
            if ( vb2_plane_size( vb, i ) < size ) {
                LOG( LOG_ERR, "i:%d buffer too small (%lu < %lu)", i, vb2_plane_size( vb, 0 ), size );
                return -EINVAL;
            }
            vb2_set_plane_payload( vb, i, size );
        }
    }

    return 0;
}

static int isp_vb_mmap_cvt(isp_v4l2_stream_t *pstream, tframe_t *frame, isp_v4l2_buffer_t *buf)
{
    void *p_mem = NULL;
    void *s_mem = NULL;
    unsigned int p_size = 0;
    unsigned int s_size = 0;
    unsigned int bytesline = 0;
    struct page *cma_pages = NULL;

    if (frame == NULL || buf == NULL) {
        LOG(LOG_ERR, "Error input param");
        return -1;
    }

    if (pstream->cur_v4l2_fmt.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
        p_mem = vb2_plane_vaddr(&buf->vvb.vb2_buf, 0);
        p_size = PAGE_ALIGN(buf->vvb.vb2_buf.planes[0].length);

        s_mem = vb2_plane_vaddr(&buf->vvb.vb2_buf, 1);
        s_size = PAGE_ALIGN(buf->vvb.vb2_buf.planes[1].length);

        cma_pages = p_mem;
        frame->primary.address = page_to_phys(cma_pages);
        frame->primary.size = p_size;
        frame->primary.line_offset = pstream->cur_v4l2_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;

        cma_pages = s_mem;
        frame->secondary.address = page_to_phys(cma_pages);
        frame->secondary.size = s_size;
        frame->secondary.line_offset = pstream->cur_v4l2_fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
    } else if (pstream->cur_v4l2_fmt.type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        if ((pstream->cur_v4l2_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_NV21) ||
            (pstream->cur_v4l2_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_NV12)) {
            p_mem = vb2_plane_vaddr(&buf->vvb.vb2_buf, 0);
            bytesline = pstream->cur_v4l2_fmt.fmt.pix.bytesperline;
            p_size = bytesline * pstream->cur_v4l2_fmt.fmt.pix.height;
            s_size = p_size / 2;
            cma_pages = p_mem;
            frame->primary.address = page_to_phys(cma_pages);
            frame->primary.size = p_size;
            frame->secondary.address = frame->primary.address + p_size;
            frame->secondary.size = s_size;
        } else {
            p_mem = vb2_plane_vaddr(&buf->vvb.vb2_buf, 0);
            p_size = PAGE_ALIGN(buf->vvb.vb2_buf.planes[0].length);
            cma_pages = p_mem;
            frame->primary.address = page_to_phys(cma_pages);
            frame->primary.size = p_size;
            frame->secondary.address = 0;
            frame->secondary.size = 0;
        }
    } else {
        LOG(LOG_CRIT, "v4l2 bufer format not supported\n");
    }

    frame->list = (void *)&buf->list;
    return 0;
}

#ifdef CONFIG_AMLOGIC_ION
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
void isp_ion_buffer_to_phys(struct ion_buffer *buffer,
                phys_addr_t *addr, size_t *len)
{
    struct sg_table *sg_table;
    struct page *page;

    if (buffer) {
        sg_table = buffer->sg_table;
        page = sg_page(sg_table->sgl);
        *addr = PFN_PHYS(page_to_pfn(page));
        *len = buffer->size;
    }
}

int isp_ion_share_fd_to_phys(int fd, phys_addr_t *addr, size_t *len)
{
    struct dma_buf *dmabuf;
    struct ion_buffer *buffer;

    dmabuf = dma_buf_get(fd);
    if (IS_ERR_OR_NULL(dmabuf))
        return PTR_ERR(dmabuf);

    buffer = (struct ion_buffer *)dmabuf->priv;
    isp_ion_buffer_to_phys(buffer, addr, len);
    dma_buf_put(dmabuf);

    return 0;
}
#endif

static void isp_vb_userptr_cvt(isp_v4l2_stream_t *pstream, tframe_t *frame, isp_v4l2_buffer_t *buf)
{
    int i = 0;
    int ret = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
    phys_addr_t addr;
#else
    ion_phys_addr_t addr;
#endif
    size_t size = 0;
    uint32_t p_len = 0;
    uint32_t p_off = 0;
    uint32_t p_addr = 0;
    struct vb2_cmalloc_buf *cma_buf;

    for (i = 0; i < buf->vvb.vb2_buf.num_planes; i++) {
        cma_buf = buf->vvb.vb2_buf.planes[i].mem_priv;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0))
        ret = isp_ion_share_fd_to_phys((unsigned long)cma_buf->vaddr,
                            &addr, &size);
#else
        ret = meson_ion_share_fd_to_phys(pstream->ion_client, (unsigned long)cma_buf->vaddr,
                            &addr, &size);
#endif
        if (ret < 0) {
            LOG(LOG_CRIT, "Failed to get phys addr\n");
            return;
        }

        p_len = buf->vvb.vb2_buf.planes[i].length;
        p_addr = addr + p_off;

        switch (i) {
        case 0:
            frame->primary.address = p_addr;
            frame->primary.size = p_len;
            break;
        case 1:
            frame->secondary.address = p_addr;
            frame->secondary.size = p_len;
            break;
        default:
            LOG(LOG_CRIT, "Failed to support %d planes\n", i + 1);
            break;
        }

        LOG(LOG_DEBUG, "idx %u, plane[%d], addr 0x%x, len %u, off %u, size %u",
                buf->vvb.vb2_buf.index, i, p_addr, p_len, p_off, size);

        p_off += p_len;
    }

    frame->list = (void *)&buf->list;
}
#else
static void isp_vb_userptr_cvt(isp_v4l2_stream_t *pstream, tframe_t *frame, isp_v4l2_buffer_t *buf)
{
}
#endif
static int isp_vb_to_tframe(isp_v4l2_stream_t *pstream,
                                tframe_t *frame,
                                isp_v4l2_buffer_t *buf)
{
    if (frame == NULL || buf == NULL) {
        LOG(LOG_ERR, "Error input param");
        return -1;
    }

    if (buf->vvb.vb2_buf.memory == VB2_MEMORY_MMAP)
        isp_vb_mmap_cvt(pstream, frame, buf);
    else if (buf->vvb.vb2_buf.memory == VB2_MEMORY_USERPTR)
        isp_vb_userptr_cvt(pstream, frame, buf);

    return 0;
}

static void isp_frame_buff_queue(void *stream, isp_v4l2_buffer_t *buf, unsigned int index)
{
    isp_v4l2_stream_t *pstream = NULL;
    int32_t s_type = -1;
    uint8_t d_type = 0;
    uint32_t rtn = 0;
    tframe_t f_buff;
    int rc = -1;

    if (stream == NULL || buf == NULL) {
        LOG(LOG_ERR, "Error input param\n");
        return;
    }

    pstream = stream;
    memset(&f_buff, 0, sizeof(f_buff));

    s_type = pstream->stream_type;

    switch (s_type) {
    case V4L2_STREAM_TYPE_FR:
        d_type = dma_fr;
        break;
    case V4L2_STREAM_TYPE_DS1:
        d_type = dma_ds1;
        break;
#if ISP_HAS_SC0
    case V4L2_STREAM_TYPE_SC0:
        d_type = dma_sc0;
        break;
#endif
#if ISP_HAS_SC1
    case V4L2_STREAM_TYPE_SC1:
        d_type = dma_sc1;
        break;
#endif
#if ISP_HAS_SC2
    case V4L2_STREAM_TYPE_SC2:
        d_type = dma_sc2;
        break;
#endif
#if ISP_HAS_SC3
    case V4L2_STREAM_TYPE_CROP:
        d_type = dma_sc3;
        break;
#endif



    default:
        return;
    }

    if ((s_type == V4L2_STREAM_TYPE_FR) ||
        (s_type == V4L2_STREAM_TYPE_DS1)) {
        rc = isp_vb_to_tframe(pstream, &f_buff, buf);
        if (rc != 0) {
           LOG( LOG_ERR, "isp vb to tframe is error.");
           return;
        }

        acamera_api_dma_buffer(pstream->ctx_id, d_type, &f_buff, 1, &rtn, index);
    }

#if ISP_HAS_SC0
    if (s_type == V4L2_STREAM_TYPE_SC0) {
        rc = isp_vb_to_tframe(pstream, &f_buff, buf);
        if (rc != 0) {
           LOG( LOG_ERR, "isp vb to tframe is error.");
           return;
        }
        am_sc_api_dma_buffer(pstream->ctx_id, &f_buff, index);
    }
#endif

#if ISP_HAS_SC1
    if (s_type == V4L2_STREAM_TYPE_SC1) {
        rc = isp_vb_to_tframe(pstream, &f_buff, buf);
        if (rc != 0) {
           LOG( LOG_ERR, "isp vb to tframe is error.");
           return;
        }
        am_sc1_api_dma_buffer(pstream->ctx_id, &f_buff, index);
    }
#endif
#if ISP_HAS_SC2
    if (s_type == V4L2_STREAM_TYPE_SC2) {
        rc = isp_vb_to_tframe(pstream, &f_buff, buf);
        if (rc != 0) {
           LOG( LOG_ERR, "isp vb to tframe is error.");
           return;
        }
        am_sc2_api_dma_buffer(pstream->ctx_id, &f_buff, index);
    }
#endif
#if ISP_HAS_SC3
    if (s_type == V4L2_STREAM_TYPE_CROP) {
        rc = isp_vb_to_tframe(pstream, &f_buff, buf);
        if (rc != 0) {
           LOG( LOG_ERR, "isp vb to tframe is error.");
           return;
        }
        am_sc3_api_dma_buffer(pstream->ctx_id, &f_buff, index);
    }
#endif



}

#ifdef AUTOWRITE_MODULES_V4L2_API
void isp_autocap_buf_queue(isp_v4l2_stream_t *pstream, isp_v4l2_buffer_t *buf)
{
    int32_t s_type = -1;
    uint8_t d_type = 0;
    tframe_t f_buff;

    f_buff.primary.address = 0;

    isp_vb_to_tframe(pstream, &f_buff, buf);

    s_type = pstream->stream_type;

    switch (s_type) {
        case V4L2_STREAM_TYPE_FR:
            d_type = dma_fr;
            break;
        case V4L2_STREAM_TYPE_DS1:
            d_type = dma_ds1;
            break;
#if ISP_HAS_SC0
        case V4L2_STREAM_TYPE_SC0:
            d_type = dma_sc0;
            break;
#endif
#if ISP_HAS_SC1
        case V4L2_STREAM_TYPE_SC1:
            d_type = dma_sc1;
            break;
#endif
#if ISP_HAS_SC2
        case V4L2_STREAM_TYPE_SC2:
            d_type = dma_sc2;
            break;
#endif
#if ISP_HAS_SC3
        case V4L2_STREAM_TYPE_CROP:
            d_type = dma_sc3;
            break;
#endif



        default:
            return;
    }

    autocap_pushbuf(pstream->ctx_id, d_type, f_buff, pstream);
}
#endif

static void isp_vb2_buf_queue( struct vb2_buffer *vb )
{
    isp_v4l2_stream_t *pstream = vb2_get_drv_priv( vb->vb2_queue );
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
    struct vb2_v4l2_buffer *vvb = to_vb2_v4l2_buffer( vb );
    isp_v4l2_buffer_t *buf = container_of( vvb, isp_v4l2_buffer_t, vvb );
#else
    isp_v4l2_buffer_t *buf = container_of( vb, isp_v4l2_buffer_t, vb );
#endif
    static unsigned long cnt = 0;

    LOG( LOG_DEBUG, "Enter id:%d, cnt: %lu.", pstream->stream_id, cnt++ );

    spin_lock( &pstream->slock );
    list_add_tail( &buf->list, &pstream->stream_buffer_list );
#ifdef AUTOWRITE_MODULES_V4L2_API
    if (autocap_get_mode(pstream->ctx_id) == AUTOCAP_MODE0)
        isp_autocap_buf_queue(pstream, buf);
#endif
    isp_frame_buff_queue(pstream, buf, vb->index);
    spin_unlock( &pstream->slock );
}

static const struct vb2_ops isp_vb2_ops = {
    .queue_setup = isp_vb2_queue_setup, // called from VIDIOC_REQBUFS
    .buf_prepare = isp_vb2_buf_prepare,
    .buf_queue = isp_vb2_buf_queue,
    .wait_prepare = vb2_ops_wait_prepare,
    .wait_finish = vb2_ops_wait_finish,
};

/* ----------------------------------------------------------------
 * VB2 external interface for isp-v4l2
 */
int isp_vb2_queue_init( struct vb2_queue *q, struct mutex *mlock, isp_v4l2_stream_t *pstream, struct device *dev )
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
#ifdef CONFIG_AMLOGIC_ION
    char ion_client_name[32];
#endif
#endif
    memset( q, 0, sizeof( struct vb2_queue ) );

    /* start creating the vb2 queues */

    //all stream multiplanar
    q->type = pstream->cur_v4l2_fmt.type;

    if (pstream->stream_id == V4L2_STREAM_TYPE_FR ||
            pstream->stream_id == V4L2_STREAM_TYPE_DS1) {
        q->mem_ops = &vb2_cmalloc_memops;
    }
#if ISP_HAS_SC0
    else if (pstream->stream_id == V4L2_STREAM_TYPE_SC0) {
        q->mem_ops = &vb2_cmalloc_memops;
    }
#endif
#if ISP_HAS_SC1
    else if (pstream->stream_id == V4L2_STREAM_TYPE_SC1) {
        q->mem_ops = &vb2_cmalloc_memops;
    }
#endif
#if ISP_HAS_SC2
    else if (pstream->stream_id == V4L2_STREAM_TYPE_SC2) {
        q->mem_ops = &vb2_cmalloc_memops;
    }
#endif
#if ISP_HAS_SC3
    else if (pstream->stream_id == V4L2_STREAM_TYPE_CROP) {
        q->mem_ops = &vb2_cmalloc_memops;
    }
#endif
    else {
        q->mem_ops = &vb2_vmalloc_memops;
    }

#ifdef CONFIG_AMLOGIC_ION
    q->io_modes = VB2_MMAP | VB2_READ | VB2_USERPTR;
#else
    q->io_modes = VB2_MMAP | VB2_READ;
#endif
    q->drv_priv = pstream;
    q->buf_struct_size = sizeof( isp_v4l2_buffer_t );

    q->ops = &isp_vb2_ops;
    q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
    q->min_buffers_needed = 3;
    q->lock = mlock;
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION( 4, 8, 0 ) )
    q->dev = dev;
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
#ifdef CONFIG_AMLOGIC_ION
    sprintf(ion_client_name, "isp_stream_%d", pstream->stream_id);
    if (!pstream->ion_client)
        pstream->ion_client = meson_ion_client_create(-1, ion_client_name);
#endif
#endif

    return vb2_queue_init( q );
}

void isp_vb2_queue_release( struct vb2_queue *q, isp_v4l2_stream_t *pstream )
{
    vb2_queue_release( q );

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0))
#ifdef CONFIG_AMLOGIC_ION
    if (pstream->ion_client) {
        ion_client_destroy(pstream->ion_client);
        pstream->ion_client = NULL;
    }
#endif
#endif
}
