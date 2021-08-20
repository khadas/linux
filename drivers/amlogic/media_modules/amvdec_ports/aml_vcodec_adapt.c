/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#include <linux/types.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/utils/aformat.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/amlogic/media/frame_sync/tsync_pcr.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/utils/aformat.h>
#include <linux/amlogic/media/registers/register.h>
#include "../stream_input/amports/adec.h"
#include "../stream_input/amports/streambuf.h"
#include "../stream_input/amports/streambuf_reg.h"
#include "../stream_input/parser/tsdemux.h"
#include "../stream_input/parser/psparser.h"
#include "../stream_input/parser/esparser.h"
#include "../frame_provider/decoder/utils/vdec.h"
#include "../common/media_clock/switch/amports_gate.h"
#include <linux/delay.h>
#include "aml_vcodec_adapt.h"
#include <linux/crc32.h>

#define DEFAULT_VIDEO_BUFFER_SIZE		(1024 * 1024 * 3)
#define DEFAULT_VIDEO_BUFFER_SIZE_4K		(1024 * 1024 * 6)
#define DEFAULT_VIDEO_BUFFER_SIZE_TVP		(1024 * 1024 * 10)
#define DEFAULT_VIDEO_BUFFER_SIZE_4K_TVP	(1024 * 1024 * 15)
#define DEFAULT_AUDIO_BUFFER_SIZE		(1024*768*2)
#define DEFAULT_SUBTITLE_BUFFER_SIZE		(1024*256)

#define PTS_OUTSIDE	(1)
#define SYNC_OUTSIDE	(2)

//#define DATA_DEBUG

extern int dump_output_frame;
extern char dump_path[32];
extern u32 dump_output_start_position;
extern void aml_recycle_dma_buffers(struct aml_vcodec_ctx *ctx, u32 handle);

static int slow_input = 0;

static struct stream_buf_s bufs[BUF_MAX_NUM] = {
	{
		.reg_base = VLD_MEM_VIFIFO_REG_BASE,
		.type = BUF_TYPE_VIDEO,
		.buf_start = 0,
		.buf_size = DEFAULT_VIDEO_BUFFER_SIZE,
		.default_buf_size = DEFAULT_VIDEO_BUFFER_SIZE,
		.first_tstamp = INVALID_PTS
	},
	{
		.reg_base = AIU_MEM_AIFIFO_REG_BASE,
		.type = BUF_TYPE_AUDIO,
		.buf_start = 0,
		.buf_size = DEFAULT_AUDIO_BUFFER_SIZE,
		.default_buf_size = DEFAULT_AUDIO_BUFFER_SIZE,
		.first_tstamp = INVALID_PTS
	},
	{
		.reg_base = 0,
		.type = BUF_TYPE_SUBTITLE,
		.buf_start = 0,
		.buf_size = DEFAULT_SUBTITLE_BUFFER_SIZE,
		.default_buf_size = DEFAULT_SUBTITLE_BUFFER_SIZE,
		.first_tstamp = INVALID_PTS
	},
	{
		.reg_base = 0,
		.type = BUF_TYPE_USERDATA,
		.buf_start = 0,
		.buf_size = 0,
		.first_tstamp = INVALID_PTS
	},
	{
		.reg_base = HEVC_STREAM_REG_BASE,
		.type = BUF_TYPE_HEVC,
		.buf_start = 0,
		.buf_size = DEFAULT_VIDEO_BUFFER_SIZE_4K,
		.default_buf_size = DEFAULT_VIDEO_BUFFER_SIZE_4K,
		.first_tstamp = INVALID_PTS
	},
};

extern int aml_set_vfm_path, aml_set_vdec_type;
extern bool aml_set_vfm_enable, aml_set_vdec_type_enable;

static void set_default_params(struct aml_vdec_adapt *vdec)
{
	ulong sync_mode = (PTS_OUTSIDE | SYNC_OUTSIDE);

	vdec->dec_prop.param = (void *)sync_mode;
	vdec->dec_prop.format = vdec->format;
	vdec->dec_prop.width = 1920;
	vdec->dec_prop.height = 1088;
	vdec->dec_prop.rate = 3200;
}

static int enable_hardware(struct stream_port_s *port)
{
	if (get_cpu_type() < MESON_CPU_MAJOR_ID_M6)
		return -1;

	amports_switch_gate("demux", 1);
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		amports_switch_gate("parser_top", 1);

	if (port->type & PORT_TYPE_VIDEO) {
		amports_switch_gate("vdec", 1);

		if (has_hevc_vdec()) {
			if (port->type & PORT_TYPE_HEVC)
				vdec_poweron(VDEC_HEVC);
			else
				vdec_poweron(VDEC_1);
		} else {
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
				vdec_poweron(VDEC_1);
		}
	}

	return 0;
}

static int disable_hardware(struct stream_port_s *port)
{
	if (get_cpu_type() < MESON_CPU_MAJOR_ID_M6)
		return -1;

	if (port->type & PORT_TYPE_VIDEO) {
		if (has_hevc_vdec()) {
			if (port->type & PORT_TYPE_HEVC)
				vdec_poweroff(VDEC_HEVC);
			else
				vdec_poweroff(VDEC_1);
		}

		amports_switch_gate("vdec", 0);
	}

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8)
		amports_switch_gate("parser_top", 0);

	amports_switch_gate("demux", 0);

	return 0;
}

static void user_buffer_init(void)
{
	struct stream_buf_s *pubuf = &bufs[BUF_TYPE_USERDATA];

	pubuf->buf_size = 0;
	pubuf->buf_start = 0;
	pubuf->buf_wp = 0;
	pubuf->buf_rp = 0;
}

static void video_component_release(struct stream_port_s *port)
{
	struct aml_vdec_adapt *ada_ctx
		= container_of(port, struct aml_vdec_adapt, port);
	struct vdec_s *vdec = ada_ctx->vdec;

	vdec_release(vdec);

}

static int video_component_init(struct stream_port_s *port,
			  struct stream_buf_s *pbuf)
{
	int ret = -1;
	struct aml_vdec_adapt *ada_ctx
		= container_of(port, struct aml_vdec_adapt, port);
	struct vdec_s *vdec = ada_ctx->vdec;

	if ((vdec->port_flag & PORT_FLAG_VFORMAT) == 0) {
		v4l_dbg(ada_ctx->ctx, V4L_DEBUG_CODEC_ERROR, "vformat not set\n");
		return -EPERM;
	}

	port->is_4k = false;

	if (port->type & PORT_TYPE_FRAME ||
		(port->type & PORT_TYPE_ES)) {
		ret = vdec_init(vdec, port->is_4k, true);
		if (ret < 0) {
			v4l_dbg(ada_ctx->ctx, V4L_DEBUG_CODEC_ERROR, "failed\n");
			video_component_release(port);
			return ret;
		}
	}

	return 0;
}

static int vdec_ports_release(struct stream_port_s *port)
{
	struct stream_buf_s *pvbuf = &bufs[BUF_TYPE_VIDEO];

	if (has_hevc_vdec()) {
		if (port->vformat == VFORMAT_HEVC ||
			port->vformat == VFORMAT_VP9)
			pvbuf = &bufs[BUF_TYPE_HEVC];
	}

	if (port->type & PORT_TYPE_MPTS) {
		tsync_pcr_stop();
		tsdemux_release();
	}

	if (port->type & PORT_TYPE_MPPS)
		psparser_release();

	if (port->type & PORT_TYPE_VIDEO)
		video_component_release(port);

	port->pcr_inited = 0;
	port->flag = 0;

	return 0;
}

static void set_vdec_properity(struct vdec_s *vdec,
	struct aml_vdec_adapt *ada_ctx)
{
	vdec->sys_info	= &ada_ctx->dec_prop;
	vdec->port	= &ada_ctx->port;
	vdec->format	= ada_ctx->video_type;
	vdec->sys_info_store = ada_ctx->dec_prop;

	/* binding v4l2 ctx to vdec. */
	vdec->private = ada_ctx->ctx;

	/* set video format, sys info and vfm map.*/
	vdec->port->vformat = vdec->format;
	vdec->port->type |= PORT_TYPE_VIDEO;
	vdec->port_flag |= (vdec->port->flag | PORT_FLAG_VFORMAT);
	if (vdec->slave) {
		vdec->slave->format = ada_ctx->dec_prop.format;
		vdec->slave->port_flag |= PORT_FLAG_VFORMAT;
	}

	vdec->type = VDEC_TYPE_FRAME_BLOCK;
	vdec->port->type |= PORT_TYPE_FRAME;
	vdec->frame_base_video_path = FRAME_BASE_PATH_V4L_OSD;

	if (aml_set_vdec_type_enable) {
		if (aml_set_vdec_type == VDEC_TYPE_STREAM_PARSER) {
			vdec->type = VDEC_TYPE_STREAM_PARSER;
			vdec->port->type &= ~PORT_TYPE_FRAME;
			vdec->port->type |= PORT_TYPE_ES;
		} else if (aml_set_vdec_type == VDEC_TYPE_FRAME_BLOCK) {
			vdec->type = VDEC_TYPE_FRAME_BLOCK;
			vdec->port->type &= ~PORT_TYPE_ES;
			vdec->port->type |= PORT_TYPE_FRAME;
		}
	}

	if (aml_set_vfm_enable)
		vdec->frame_base_video_path = aml_set_vfm_path;

	vdec->port->flag = vdec->port_flag;

	vdec->config_len = ada_ctx->config.length >
		PAGE_SIZE ? PAGE_SIZE : ada_ctx->config.length;
	memcpy(vdec->config, ada_ctx->config.buf, vdec->config_len);

	ada_ctx->vdec = vdec;
}

static int vdec_ports_init(struct aml_vdec_adapt *ada_ctx)
{
	int ret = -1;
	struct stream_buf_s *pvbuf = &bufs[BUF_TYPE_VIDEO];
	struct vdec_s *vdec = NULL;

	/* create the vdec instance.*/
	vdec = vdec_create(&ada_ctx->port, NULL);
	if (IS_ERR_OR_NULL(vdec))
		return -1;

	vdec->disable_vfm = true;
	set_vdec_properity(vdec, ada_ctx);

	/* init hw and gate*/
	ret = enable_hardware(vdec->port);
	if (ret < 0) {
		v4l_dbg(ada_ctx->ctx, V4L_DEBUG_CODEC_ERROR, "enable hw fail.\n");
		return ret;
	}

	stbuf_fetch_init();
	user_buffer_init();

	if ((vdec->port->type & PORT_TYPE_VIDEO)
		&& (vdec->port_flag & PORT_FLAG_VFORMAT)) {
		vdec->port->is_4k = false;
		if (has_hevc_vdec()) {
			if (vdec->port->vformat == VFORMAT_HEVC ||
				vdec->port->vformat == VFORMAT_VP9)
				pvbuf = &bufs[BUF_TYPE_HEVC];
		}

		ret = video_component_init(vdec->port, pvbuf);
		if (ret < 0) {
			v4l_dbg(ada_ctx->ctx, V4L_DEBUG_CODEC_ERROR, "video_component_init  failed\n");
			return ret;
		}

		/* connect vdec at the end after all HW initialization */
		vdec_connect(vdec);
	}

	return 0;
}

int video_decoder_init(struct aml_vdec_adapt *vdec)
{
	int ret = -1;

	/* sets configure data */
	set_default_params(vdec);

	/* init the buffer work space and connect vdec.*/
	ret = vdec_ports_init(vdec);
	if (ret < 0) {
		v4l_dbg(vdec->ctx, V4L_DEBUG_CODEC_ERROR, "vdec ports init fail.\n");
		goto out;
	}
out:
	return ret;
}

int video_decoder_release(struct aml_vdec_adapt *vdec)
{
	int ret = -1;
	struct stream_port_s *port = &vdec->port;

	ret = vdec_ports_release(port);
	if (ret < 0) {
		v4l_dbg(vdec->ctx, V4L_DEBUG_CODEC_ERROR, "vdec ports release fail.\n");
		goto out;
	}

	/* disable gates */
	ret = disable_hardware(port);
	if (ret < 0) {
		v4l_dbg(vdec->ctx, V4L_DEBUG_CODEC_ERROR, "disable hw fail.\n");
		goto out;
	}
out:
	return ret;
}

void dump(const char* path, const char *data, unsigned int size)
{
	struct file *fp;

	fp = filp_open(path,
			O_CREAT | O_RDWR | O_LARGEFILE | O_APPEND, 0600);
	if (!IS_ERR(fp)) {
		kernel_write(fp, data, size, 0);
		filp_close(fp, NULL);
	} else {
		pr_info("Dump ES fail, should check RW permission, size:%x\n", size);
	}
}

int vdec_vbuf_write(struct aml_vdec_adapt *ada_ctx,
	const char *buf, unsigned int count)
{
	int ret = -1;
	int try_cnt = 100;
	struct stream_port_s *port = &ada_ctx->port;
	struct vdec_s *vdec = ada_ctx->vdec;
	struct stream_buf_s *pbuf = NULL;

	if (has_hevc_vdec()) {
		pbuf = (port->type & PORT_TYPE_HEVC) ? &bufs[BUF_TYPE_HEVC] :
			&bufs[BUF_TYPE_VIDEO];
	} else
		pbuf = &bufs[BUF_TYPE_VIDEO];

	/*if (!(port_get_inited(priv))) {
		r = video_decoder_init(priv);
		if (r < 0)
			return r;
	}*/

	do {
		if (vdec->port_flag & PORT_FLAG_DRM)
			ret = drm_write(ada_ctx->filp, pbuf, buf, count);
		else
			ret = esparser_write(ada_ctx->filp, pbuf, buf, count);

		if (ret == -EAGAIN)
			msleep(30);
	} while (ret == -EAGAIN && try_cnt--);

	if (slow_input) {
		v4l_dbg(ada_ctx->ctx, V4L_DEBUG_CODEC_PRINFO,
			"slow_input: es codec write size %x\n", ret);
		msleep(10);
	}

#ifdef DATA_DEBUG
	/* dump to file */
	//dump_write(vbuf, size);
	//v4l_dbg(ada_ctx->ctx, V4L_DEBUG_CODEC_PRINFO, "vbuf: %p, size: %u, ret: %d\n", vbuf, size, ret);
#endif

	return ret;
}

bool vdec_input_full(struct aml_vdec_adapt *ada_ctx)
{
	struct vdec_s *vdec = ada_ctx->vdec;

	return (vdec->input.have_frame_num > 60) ? true : false;
}

int vdec_vframe_write(struct aml_vdec_adapt *ada_ctx,
	const char *buf, unsigned int count, u64 timestamp, ulong meta_ptr)
{
	int ret = -1;
	struct vdec_s *vdec = ada_ctx->vdec;

	/* set timestamp */
	vdec_set_timestamp(vdec, timestamp);

	/* set metadata */
	vdec_set_metadata(vdec, meta_ptr);

	ret = vdec_write_vframe(vdec, buf, count);

	if (slow_input) {
		v4l_dbg(ada_ctx->ctx, V4L_DEBUG_CODEC_PRINFO,
			"slow_input: frame codec write size %d\n", ret);
		msleep(30);
	}

	if (dump_output_frame > 0 &&
		(!dump_output_start_position ||
		(dump_output_start_position == crc32_le(0, buf, count)))) {
		char file_name[64] = {0};

		snprintf(file_name, 64, "%s/es.data", dump_path);
		dump(file_name, buf, count);
		dump_output_frame--;
		dump_output_start_position = 0;
	}

	v4l_dbg(ada_ctx->ctx, V4L_DEBUG_CODEC_INPUT,
		"write frames, vbuf: %p, size: %u, ret: %d, crc: %x, ts: %llu\n",
		buf, count, ret, crc32_le(0, buf, count), timestamp);

	return ret;
}

void vdec_vframe_input_free(void *priv, u32 handle)
{
	struct aml_vcodec_ctx *ctx = priv;

	aml_recycle_dma_buffers(ctx, handle);
}

int vdec_vframe_write_with_dma(struct aml_vdec_adapt *ada_ctx,
	ulong addr, u32 count, u64 timestamp, u32 handle,
	chunk_free free, void* priv)
{
	int ret = -1;
	struct vdec_s *vdec = ada_ctx->vdec;

	/* set timestamp */
	vdec_set_timestamp(vdec, timestamp);

	ret = vdec_write_vframe_with_dma(vdec, addr, count,
		handle, free, priv);

	if (slow_input) {
		v4l_dbg(ada_ctx->ctx, V4L_DEBUG_CODEC_PRINFO,
			"slow_input: frame codec write size %d\n", ret);
		msleep(30);
	}

	v4l_dbg(ada_ctx->ctx, V4L_DEBUG_CODEC_INPUT,
		"write frames, vbuf: %lx, size: %u, ret: %d, ts: %llu\n",
		addr, count, ret, timestamp);

	return ret;
}

void aml_decoder_flush(struct aml_vdec_adapt *ada_ctx)
{
	struct vdec_s *vdec = ada_ctx->vdec;

	if (vdec)
		vdec_set_eos(vdec, true);
}

int aml_codec_reset(struct aml_vdec_adapt *ada_ctx, int *mode)
{
	struct vdec_s *vdec = ada_ctx->vdec;
	int ret = 0;

	if (vdec) {
		if (ada_ctx->ctx->v4l_resolution_change)
			*mode = V4L_RESET_MODE_LIGHT;
		else
			vdec_set_eos(vdec, false);

		v4l_dbg(ada_ctx->ctx, V4L_DEBUG_CODEC_PRINFO,
			"reset mode: %d, es frames buffering: %d\n",
			*mode, vdec_frame_number(ada_ctx));

		ret = vdec_v4l2_reset(vdec, *mode);
		*mode = V4L_RESET_MODE_NORMAL;
	}

	return ret;
}

bool is_input_ready(struct aml_vdec_adapt *ada_ctx)
{
	struct vdec_s *vdec = ada_ctx->vdec;
	int state = VDEC_STATUS_UNINITIALIZED;

	if (vdec) {
		state = vdec_get_status(vdec);

		if (state == VDEC_STATUS_CONNECTED
			|| state == VDEC_STATUS_ACTIVE)
			return true;
	}

	return false;
}

int vdec_frame_number(struct aml_vdec_adapt *ada_ctx)
{
	struct vdec_s *vdec = ada_ctx->vdec;

	if (vdec)
		return vdec_get_frame_num(vdec);
	else
		return -1;
}

int vdec_get_instance_num(void)
{
	return vdec_get_core_nr();
}

void v4l2_config_vdec_parm(struct aml_vdec_adapt *ada_ctx, u8 *data, u32 len)
{
	struct vdec_s *vdec = ada_ctx->vdec;

	vdec->config_len = len > PAGE_SIZE ? PAGE_SIZE : len;
	memcpy(vdec->config, data, vdec->config_len);
}

void vdec_set_duration(s32 duration)
{
	vdec_frame_rate_uevent(duration);
}
