/*
 * drivers/amlogic/media/stream_input/amports/amstream.c
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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
 */
#define DEBUG
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/amlogic/major.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/utils/aformat.h>

#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>

#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/io.h>
/* #include <mach/am_regs.h> */

#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#if 1				/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
/* #include <mach/mod_gate.h> */
/* #include <mach/power_gate.h> */
#endif
#include "../parser/streambuf.h"
#include "../parser/streambuf_reg.h"
#include "../parser/tsdemux.h"
#include "../parser/psparser.h"
#include "../parser/esparser.h"
#include "../../frame_provider/decoder/utils/vdec.h"
#include "adec.h"
#include "../parser/rmparser.h"
#include "amports_priv.h"
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/amlogic/media/frame_sync/tsync_pcr.h>
#include "../parser/thread_rw.h"
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/libfdt_env.h>
#include <linux/of_reserved_mem.h>
#include <linux/reset.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include "../../frame_provider/decoder/utils/firmware.h"
#include "../../common/chips/chips.h"
#include "../../common/chips/decoder_cpu_ver_info.h"
#include "../subtitle/subtitle.h"

//#define G12A_BRINGUP_DEBUG

#define CONFIG_AM_VDEC_REAL //DEBUG_TMP

#define DEVICE_NAME "amstream-dev"
#define DRIVER_NAME "amstream"
#define MODULE_NAME "amstream"

#define MAX_AMSTREAM_PORT_NUM ARRAY_SIZE(ports)
u32 amstream_port_num;
u32 amstream_buf_num;

u32 amstream_audio_reset = 0;

#if 0
#if  MESON_CPU_TYPE == MESON_CPU_TYPE_MESONG9TV
#define NO_VDEC2_INIT 1
#elif MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6TVD
#define NO_VDEC2_INIT IS_MESON_M8M2_CPU
#endif
#endif
#define NO_VDEC2_INIT 1

#define DEFAULT_VIDEO_BUFFER_SIZE       (1024 * 1024 * 3)
#define DEFAULT_VIDEO_BUFFER_SIZE_4K       (1024 * 1024 * 6)
#define DEFAULT_VIDEO_BUFFER_SIZE_TVP       (1024 * 1024 * 10)
#define DEFAULT_VIDEO_BUFFER_SIZE_4K_TVP       (1024 * 1024 * 15)


#define DEFAULT_AUDIO_BUFFER_SIZE       (1024*768*2)
#define DEFAULT_SUBTITLE_BUFFER_SIZE     (1024*256)

static int def_4k_vstreambuf_sizeM =
	(DEFAULT_VIDEO_BUFFER_SIZE_4K >> 20);
static int def_vstreambuf_sizeM =
	(DEFAULT_VIDEO_BUFFER_SIZE >> 20);
static int slow_input;




/* #define DATA_DEBUG */
static int use_bufferlevelx10000 = 10000;
static int reset_canuse_buferlevel(int level);
static struct platform_device *amstream_pdev;
struct device *amports_get_dma_device(void)
{
	return &amstream_pdev->dev;
}
EXPORT_SYMBOL(amports_get_dma_device);

#ifdef DATA_DEBUG
#include <linux/fs.h>

#define DEBUG_FILE_NAME     "/sdcard/debug.tmp"
static struct file *debug_filp;
static loff_t debug_file_pos;

void debug_file_write(const char __user *buf, size_t count)
{
	mm_segment_t old_fs;

	if (!debug_filp)
		return;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (count != vfs_write(debug_filp, buf, count, &debug_file_pos))
		pr_err("Failed to write debug file\n");

	set_fs(old_fs);
}
#endif



static int amstream_open(struct inode *inode, struct file *file);
static int amstream_release(struct inode *inode, struct file *file);
static long amstream_ioctl(struct file *file, unsigned int cmd, ulong arg);
#ifdef CONFIG_COMPAT
static long amstream_compat_ioctl
	(struct file *file, unsigned int cmd, ulong arg);
#endif
static ssize_t amstream_vbuf_write
(struct file *file, const char *buf, size_t count, loff_t *ppos);
static ssize_t amstream_vframe_write
(struct file *file, const char *buf, size_t count, loff_t *ppos);
static ssize_t amstream_abuf_write
(struct file *file, const char *buf, size_t count, loff_t *ppos);
static ssize_t amstream_mpts_write
(struct file *file, const char *buf, size_t count, loff_t *ppos);
static ssize_t amstream_mpps_write
(struct file *file, const char *buf, size_t count, loff_t *ppos);
static ssize_t amstream_sub_read
(struct file *file, char *buf, size_t count, loff_t *ppos);
static ssize_t amstream_sub_write
(struct file *file, const char *buf, size_t count, loff_t *ppos);
static unsigned int amstream_sub_poll
(struct file *file, poll_table *wait_table);
static unsigned int amstream_userdata_poll
(struct file *file, poll_table *wait_table);
static ssize_t amstream_userdata_read
(struct file *file, char *buf, size_t count, loff_t *ppos);
static int (*amstream_adec_status)
(struct adec_status *astatus);
#ifdef CONFIG_AM_VDEC_REAL
static ssize_t amstream_mprm_write
(struct file *file, const char *buf, size_t count, loff_t *ppos);
#endif

static const struct file_operations vbuf_fops = {
	.owner = THIS_MODULE,
	.open = amstream_open,
	.release = amstream_release,
	.write = amstream_vbuf_write,
	.unlocked_ioctl = amstream_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amstream_compat_ioctl,
#endif
};

static const struct file_operations vframe_fops = {
	.owner = THIS_MODULE,
	.open = amstream_open,
	.release = amstream_release,
	.write = amstream_vframe_write,
	.unlocked_ioctl = amstream_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amstream_compat_ioctl,
#endif
};

static const struct file_operations abuf_fops = {
	.owner = THIS_MODULE,
	.open = amstream_open,
	.release = amstream_release,
	.write = amstream_abuf_write,
	.unlocked_ioctl = amstream_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amstream_compat_ioctl,
#endif
};

static const struct file_operations mpts_fops = {
	.owner = THIS_MODULE,
	.open = amstream_open,
	.release = amstream_release,
	.write = amstream_mpts_write,
	.unlocked_ioctl = amstream_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amstream_compat_ioctl,
#endif
};

static const struct file_operations mpps_fops = {
	.owner = THIS_MODULE,
	.open = amstream_open,
	.release = amstream_release,
	.write = amstream_mpps_write,
	.unlocked_ioctl = amstream_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amstream_compat_ioctl,
#endif
};

static const struct file_operations mprm_fops = {
	.owner = THIS_MODULE,
	.open = amstream_open,
	.release = amstream_release,
#ifdef CONFIG_AM_VDEC_REAL
	.write = amstream_mprm_write,
#endif
	.unlocked_ioctl = amstream_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amstream_compat_ioctl,
#endif
};

static const struct file_operations sub_fops = {
	.owner = THIS_MODULE,
	.open = amstream_open,
	.release = amstream_release,
	.write = amstream_sub_write,
	.unlocked_ioctl = amstream_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amstream_compat_ioctl,
#endif
};

static const struct file_operations sub_read_fops = {
	.owner = THIS_MODULE,
	.open = amstream_open,
	.release = amstream_release,
	.read = amstream_sub_read,
	.poll = amstream_sub_poll,
	.unlocked_ioctl = amstream_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amstream_compat_ioctl,
#endif
};

static const struct file_operations userdata_fops = {
	.owner = THIS_MODULE,
	.open = amstream_open,
	.release = amstream_release,
	.read = amstream_userdata_read,
	.poll = amstream_userdata_poll,
	.unlocked_ioctl = amstream_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amstream_compat_ioctl,
#endif
};

static const struct file_operations amstream_fops = {
	.owner = THIS_MODULE,
	.open = amstream_open,
	.release = amstream_release,
	.unlocked_ioctl = amstream_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amstream_compat_ioctl,
#endif
};

/**************************************************/
static struct audio_info audio_dec_info;
static struct class *amstream_dev_class;
static DEFINE_MUTEX(amstream_mutex);

atomic_t subdata_ready = ATOMIC_INIT(0);
static int sub_type;
static int sub_port_inited;
/* wait queue for poll */
static wait_queue_head_t amstream_sub_wait;
atomic_t userdata_ready = ATOMIC_INIT(0);
static int userdata_length;
static wait_queue_head_t amstream_userdata_wait;
#define USERDATA_FIFO_NUM    1024
static struct userdata_poc_info_t userdata_poc_info[USERDATA_FIFO_NUM];
static int userdata_poc_ri, userdata_poc_wi;
static int last_read_wi;
static u32 ud_ready_vdec_flag;



static DEFINE_MUTEX(userdata_mutex);

static struct stream_port_s ports[] = {
	{
		.name = "amstream_vbuf",
		.type = PORT_TYPE_ES | PORT_TYPE_VIDEO,
		.fops = &vbuf_fops,
	},
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	{
		.name = "amstream_vbuf_sched",
		.type = PORT_TYPE_ES | PORT_TYPE_VIDEO |
			PORT_TYPE_DECODER_SCHED,
		.fops = &vbuf_fops,
	},
	{
		.name = "amstream_vframe",
		.type = PORT_TYPE_ES | PORT_TYPE_VIDEO |
			PORT_TYPE_FRAME | PORT_TYPE_DECODER_SCHED,
		.fops = &vframe_fops,
	},
#endif
	{
		.name = "amstream_abuf",
		.type = PORT_TYPE_ES | PORT_TYPE_AUDIO,
		.fops = &abuf_fops,
	},
	{
		.name = "amstream_mpts",
		.type = PORT_TYPE_MPTS | PORT_TYPE_VIDEO |
			PORT_TYPE_AUDIO | PORT_TYPE_SUB,
		.fops = &mpts_fops,
	},
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	{
		.name = "amstream_mpts_sched",
		.type = PORT_TYPE_MPTS | PORT_TYPE_VIDEO |
			PORT_TYPE_AUDIO | PORT_TYPE_SUB |
			PORT_TYPE_DECODER_SCHED,
		.fops = &mpts_fops,
	},
#endif
	{
		.name = "amstream_mpps",
		.type = PORT_TYPE_MPPS | PORT_TYPE_VIDEO |
			PORT_TYPE_AUDIO | PORT_TYPE_SUB,
		.fops = &mpps_fops,
	},
	{
		.name = "amstream_rm",
		.type = PORT_TYPE_RM | PORT_TYPE_VIDEO | PORT_TYPE_AUDIO,
		.fops = &mprm_fops,
	},
	{
		.name = "amstream_sub",
		.type = PORT_TYPE_SUB,
		.fops = &sub_fops,
	},
	{
		.name = "amstream_sub_read",
		.type = PORT_TYPE_SUB_RD,
		.fops = &sub_read_fops,
	},
	{
		.name = "amstream_userdata",
		.type = PORT_TYPE_USERDATA,
		.fops = &userdata_fops,
	},
	{
		.name = "amstream_hevc",
		.type = PORT_TYPE_ES | PORT_TYPE_VIDEO | PORT_TYPE_HEVC,
		.fops = &vbuf_fops,
		.vformat = VFORMAT_HEVC,
	},
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	{
		.name = "amstream_hevc_frame",
		.type = PORT_TYPE_ES | PORT_TYPE_VIDEO | PORT_TYPE_HEVC |
			PORT_TYPE_FRAME | PORT_TYPE_DECODER_SCHED,
		.fops = &vframe_fops,
		.vformat = VFORMAT_HEVC,
	},
	{
		.name = "amstream_hevc_sched",
		.type = PORT_TYPE_ES | PORT_TYPE_VIDEO | PORT_TYPE_HEVC |
			PORT_TYPE_DECODER_SCHED,
		.fops = &vbuf_fops,
		.vformat = VFORMAT_HEVC,
	},
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	{
		.name = "amstream_dves_avc",
		.type = PORT_TYPE_ES | PORT_TYPE_VIDEO |
			PORT_TYPE_DECODER_SCHED | PORT_TYPE_DUALDEC,
		.fops = &vbuf_fops,
	},
	{
		.name = "amstream_dves_hevc",
		.type = PORT_TYPE_ES | PORT_TYPE_VIDEO | PORT_TYPE_HEVC |
			PORT_TYPE_DECODER_SCHED | PORT_TYPE_DUALDEC,
		.fops = &vbuf_fops,
		.vformat = VFORMAT_HEVC,
	},
#endif
#endif
};

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

struct stream_buf_s *get_buf_by_type(u32 type)
{
	if (PTS_TYPE_VIDEO == type)
		return &bufs[BUF_TYPE_VIDEO];
	if (PTS_TYPE_AUDIO == type)
		return &bufs[BUF_TYPE_AUDIO];
	if (has_hevc_vdec()) {
		if (PTS_TYPE_HEVC == type)
			return &bufs[BUF_TYPE_HEVC];
	}

	return NULL;
}

void set_sample_rate_info(int arg)
{
	audio_dec_info.sample_rate = arg;
	audio_dec_info.valid = 1;
}

void set_ch_num_info(int arg)
{
	audio_dec_info.channels = arg;
}

struct audio_info *get_audio_info(void)
{
	return &audio_dec_info;
}
EXPORT_SYMBOL(get_audio_info);

static void amstream_change_vbufsize(struct port_priv_s *priv,
	struct stream_buf_s *pvbuf)
{
	if (pvbuf->buf_start != 0) {
		pr_info("streambuf is alloced before\n");
		return;
	}
	if (pvbuf->for_4k) {
		pvbuf->buf_size = def_4k_vstreambuf_sizeM * SZ_1M;
		if (priv->vdec->port_flag & PORT_FLAG_DRM)
			pvbuf->buf_size = DEFAULT_VIDEO_BUFFER_SIZE_4K_TVP;
		if ((pvbuf->buf_size > 30 * SZ_1M) &&
		(codec_mm_get_total_size() < 220 * SZ_1M)) {
			/*if less than 250M, used 20M for 4K & 265*/
			pvbuf->buf_size = pvbuf->buf_size >> 1;
		}
	} else if (pvbuf->buf_size > def_vstreambuf_sizeM * SZ_1M) {
		if (priv->vdec->port_flag & PORT_FLAG_DRM)
			pvbuf->buf_size = DEFAULT_VIDEO_BUFFER_SIZE_TVP;
	} else {
		pvbuf->buf_size = def_vstreambuf_sizeM * SZ_1M;
		if (priv->vdec->port_flag & PORT_FLAG_DRM)
			pvbuf->buf_size = DEFAULT_VIDEO_BUFFER_SIZE_TVP;
	}
	reset_canuse_buferlevel(10000);
}

static bool port_get_inited(struct port_priv_s *priv)
{
	struct stream_port_s *port = priv->port;

	if (port->type & PORT_TYPE_VIDEO) {
		struct vdec_s *vdec = priv->vdec;

		return vdec->port_flag & PORT_FLAG_INITED;
	}

	return port->flag & PORT_FLAG_INITED;
}

static void port_set_inited(struct port_priv_s *priv)
{
	struct stream_port_s *port = priv->port;

	if (port->type & PORT_TYPE_VIDEO) {
		struct vdec_s *vdec = priv->vdec;

		vdec->port_flag |= PORT_FLAG_INITED;
	} else
		port->flag |= PORT_FLAG_INITED;
}

static void video_port_release(struct port_priv_s *priv,
	  struct stream_buf_s *pbuf, int release_num)
{
	struct stream_port_s *port = priv->port;
	struct vdec_s *vdec = priv->vdec;
	struct vdec_s *slave = NULL;
	bool is_multidec = !vdec_single(vdec);

	switch (release_num) {
	default:
	/*fallthrough*/
	case 0:		/*release all */
	/*fallthrough*/
	case 4:
		if ((port->type & PORT_TYPE_FRAME) == 0)
			esparser_release(pbuf);
	/*fallthrough*/
	case 3:
		if (vdec->slave)
			slave = vdec->slave;
		vdec_release(vdec);
		if (slave)
			vdec_release(slave);
		priv->vdec = NULL;
	/*fallthrough*/
	case 2:
		if ((port->type & PORT_TYPE_FRAME) == 0)
			stbuf_release(pbuf, is_multidec);
	/*fallthrough*/
	case 1:
		;
	}
}

static int video_port_init(struct port_priv_s *priv,
			  struct stream_buf_s *pbuf)
{
	int r;
	struct stream_port_s *port = priv->port;
	struct vdec_s *vdec = priv->vdec;

	if ((vdec->port_flag & PORT_FLAG_VFORMAT) == 0) {
		pr_err("vformat not set\n");
		return -EPERM;
	}
	if (vdec_dual(vdec) && vdec_secure(vdec)) {
		/*copy drm flags for slave dec.*/
		vdec->slave->port_flag |= PORT_FLAG_DRM;
	}
	if (port->vformat == VFORMAT_H264_4K2K ||
		(priv->vdec->sys_info->height *
			priv->vdec->sys_info->width) > 1920*1088) {
		pbuf->for_4k = 1;
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_TXLX
				&& port->vformat == VFORMAT_H264) {
			vdec_poweron(VDEC_HEVC);
		}
	} else {
		pbuf->for_4k = 0;
	}

	if (port->type & PORT_TYPE_FRAME) {
		r = vdec_init(vdec,
			(priv->vdec->sys_info->height *
			priv->vdec->sys_info->width) > 1920*1088);
		if (r < 0) {
			pr_err("video_port_init %d, vdec_init failed\n",
				__LINE__);
			video_port_release(priv, pbuf, 2);
			return r;
		}

		return 0;
	}

	amstream_change_vbufsize(priv, pbuf);

	if (has_hevc_vdec()) {
		if (port->type & PORT_TYPE_MPTS) {
			if (pbuf->type == BUF_TYPE_HEVC)
				vdec_poweroff(VDEC_1);
			else
				vdec_poweroff(VDEC_HEVC);
		}
	}

	r = stbuf_init(pbuf, vdec, false);
	if (r < 0) {
		pr_err("video_port_init %d, stbuf_init failed\n", __LINE__);
		return r;
	}

	/* todo: set path based on port flag */
	r = vdec_init(vdec,
		(priv->vdec->sys_info->height *
		 priv->vdec->sys_info->width) > 1920*1088);

	if (r < 0) {
		pr_err("video_port_init %d, vdec_init failed\n", __LINE__);
		video_port_release(priv, pbuf, 2);
		return r;
	}

	if (vdec_dual(vdec)) {
		r = vdec_init(vdec->slave,
			(priv->vdec->sys_info->height *
			priv->vdec->sys_info->width) > 1920*1088);
		if (r < 0) {
			pr_err("video_port_init %d, vdec_init failed\n",
				__LINE__);
			video_port_release(priv, pbuf, 2);
			return r;
		}
	}

	if (port->type & PORT_TYPE_ES) {
		r = esparser_init(pbuf, vdec);
		if (r < 0) {
			video_port_release(priv, pbuf, 3);
			pr_err("esparser_init() failed\n");
			return r;
		}
	}

	pbuf->flag |= BUF_FLAG_IN_USE;

	vdec_connect(priv->vdec);

	return 0;
}

static void audio_port_release(struct stream_port_s *port,
		   struct stream_buf_s *pbuf, int release_num)
{
	switch (release_num) {
	default:
	/*fallthrough*/
	case 0:		/*release all */
	/*fallthrough*/
	case 4:
		esparser_release(pbuf);
	/*fallthrough*/
	case 3:
		adec_release(port->vformat);
	/*fallthrough*/
	case 2:
		stbuf_release(pbuf, false);
	/*fallthrough*/
	case 1:
		;
	}
	amstream_audio_reset = 0;
	return;
}

static int audio_port_reset(struct stream_port_s *port,
				struct stream_buf_s *pbuf)
{
	int r;

	if ((port->flag & PORT_FLAG_AFORMAT) == 0) {
		pr_err("aformat not set\n");
		return 0;
	}

	pts_stop(PTS_TYPE_AUDIO);

	stbuf_release(pbuf, false);

	r = stbuf_init(pbuf, NULL, false);
	if (r < 0)
		return r;

	r = adec_init(port);
	if (r < 0) {
		audio_port_release(port, pbuf, 2);
		return r;
	}

	if (port->type & PORT_TYPE_ES)
		esparser_audio_reset_s(pbuf);

	if (port->type & PORT_TYPE_MPTS)
		tsdemux_audio_reset();

	if (port->type & PORT_TYPE_MPPS)
		psparser_audio_reset();

#ifdef CONFIG_AM_VDEC_REAL
	if (port->type & PORT_TYPE_RM)
		rm_audio_reset();
#endif

	pbuf->flag |= BUF_FLAG_IN_USE;
	amstream_audio_reset = 1;

	r = pts_start(PTS_TYPE_AUDIO);

	return r;
}

static int sub_port_reset(struct stream_port_s *port,
				struct stream_buf_s *pbuf)
{
	int r;

	port->flag &= (~PORT_FLAG_INITED);

	stbuf_release(pbuf, false);

	r = stbuf_init(pbuf, NULL, false);
	if (r < 0)
		return r;

	if (port->type & PORT_TYPE_MPTS)
		tsdemux_sub_reset();

	if (port->type & PORT_TYPE_MPPS)
		psparser_sub_reset();

	if (port->sid == 0xffff) {	/* es sub */
		esparser_sub_reset();
		pbuf->flag |= BUF_FLAG_PARSER;
	}

	pbuf->flag |= BUF_FLAG_IN_USE;

	port->flag |= PORT_FLAG_INITED;

	return 0;
}

static int audio_port_init(struct stream_port_s *port,
			struct stream_buf_s *pbuf)
{
	int r;

	if ((port->flag & PORT_FLAG_AFORMAT) == 0) {
		pr_err("aformat not set\n");
		return 0;
	}

	r = stbuf_init(pbuf, NULL, false);
	if (r < 0)
		return r;
	r = adec_init(port);
	if (r < 0) {
		audio_port_release(port, pbuf, 2);
		return r;
	}
	if (port->type & PORT_TYPE_ES) {
		r = esparser_init(pbuf, NULL);
		if (r < 0) {
			audio_port_release(port, pbuf, 3);
			return r;
		}
	}
	pbuf->flag |= BUF_FLAG_IN_USE;
	return 0;
}

static void sub_port_release(struct stream_port_s *port,
					struct stream_buf_s *pbuf)
{
	if ((port->sid == 0xffff) &&
		((port->type & (PORT_TYPE_MPPS | PORT_TYPE_MPTS)) == 0)) {
		/* this is es sub */
		esparser_release(pbuf);
	}
	stbuf_release(pbuf, false);
	sub_port_inited = 0;
}

static int sub_port_init(struct stream_port_s *port, struct stream_buf_s *pbuf)
{
	int r;
	r = stbuf_init(pbuf, NULL, false);
	if (r < 0)
		return r;
	if ((port->flag & PORT_FLAG_SID) == 0) {
		pr_err("subtitle id not set\n");
		return 0;
	}

	if ((port->sid == 0xffff) &&
		((port->type & (PORT_TYPE_MPPS | PORT_TYPE_MPTS)) == 0)) {
		/* es sub */
		r = esparser_init(pbuf, NULL);
		if (r < 0) {
			sub_port_release(port, pbuf);
			return r;
		}
	}

	sub_port_inited = 1;
	return 0;
}

static void amstream_user_buffer_init(void)
{
	struct stream_buf_s *pubuf = &bufs[BUF_TYPE_USERDATA];

	pubuf->buf_size = 0;
	pubuf->buf_start = 0;
	pubuf->buf_wp = 0;
	pubuf->buf_rp = 0;
}

static int amstream_port_init(struct port_priv_s *priv)
{
	int r = 0;
	struct stream_buf_s *pvbuf = &bufs[BUF_TYPE_VIDEO];
	struct stream_buf_s *pabuf = &bufs[BUF_TYPE_AUDIO];
	struct stream_buf_s *psbuf = &bufs[BUF_TYPE_SUBTITLE];
	struct stream_port_s *port = priv->port;
	struct vdec_s *vdec = priv->vdec;

	mutex_lock(&amstream_mutex);

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) {
		r = check_efuse_chip(port->vformat);
		if (r) {
			pr_info("No support video format %d.\n", port->vformat);
			mutex_unlock(&amstream_mutex);
			return 0;
		}
	}

	/* try to reload the fw.*/
	r = video_fw_reload(FW_LOAD_TRY);
	if (r)
		pr_err("the firmware reload fail.\n");

	stbuf_fetch_init();

	amstream_user_buffer_init();

	if (port_get_inited(priv)) {
		mutex_unlock(&amstream_mutex);
		return 0;
	}

	if ((port->type & PORT_TYPE_AUDIO) &&
		(port->flag & PORT_FLAG_AFORMAT)) {
		r = audio_port_init(port, pabuf);
		if (r < 0) {
			pr_err("audio_port_init  failed\n");
			goto error1;
		}
	}

	if ((port->type & PORT_TYPE_VIDEO) &&
		(vdec->port_flag & PORT_FLAG_VFORMAT)) {
		pvbuf->for_4k = 0;
		if (has_hevc_vdec()) {
			if (port->vformat == VFORMAT_HEVC ||
				port->vformat == VFORMAT_AVS2 ||
				port->vformat == VFORMAT_VP9)
				pvbuf = &bufs[BUF_TYPE_HEVC];
		}
		mutex_lock(&priv->mutex);
		r = video_port_init(priv, pvbuf);
		if (r < 0) {
			mutex_unlock(&priv->mutex);
			pr_err("video_port_init  failed\n");
			goto error2;
		}
		mutex_unlock(&priv->mutex);
	}

	if ((port->type & PORT_TYPE_SUB) && (port->flag & PORT_FLAG_SID)) {
		r = sub_port_init(port, psbuf);
		if (r < 0) {
			pr_err("sub_port_init  failed\n");
			goto error3;
		}
	}

	if (port->type & PORT_TYPE_MPTS) {
		if (has_hevc_vdec()) {
			r = tsdemux_init(
			(port->flag & PORT_FLAG_VID) ? port->vid : 0xffff,
			(port->flag & PORT_FLAG_AID) ? port->aid : 0xffff,
			(port->flag & PORT_FLAG_SID) ? port->sid : 0xffff,
			(port->pcr_inited == 1) ? port->pcrid : 0xffff,
			(port->vformat == VFORMAT_HEVC) ||
			(port->vformat == VFORMAT_AVS2) ||
			(port->vformat == VFORMAT_VP9),
			vdec);
		} else {
			r = tsdemux_init(
			(port->flag & PORT_FLAG_VID) ? port->vid : 0xffff,
			(port->flag & PORT_FLAG_AID) ? port->aid : 0xffff,
			(port->flag & PORT_FLAG_SID) ? port->sid : 0xffff,
			(port->pcr_inited == 1) ? port->pcrid : 0xffff,
			0,
			vdec);
		}

		if (r < 0) {
			pr_err("tsdemux_init  failed\n");
			goto error4;
		}
		tsync_pcr_start();
	}
	if (port->type & PORT_TYPE_MPPS) {
		r = psparser_init(
			(port->flag & PORT_FLAG_VID) ? port->vid : 0xffff,
			(port->flag & PORT_FLAG_AID) ? port->aid : 0xffff,
			(port->flag & PORT_FLAG_SID) ? port->sid : 0xffff,
			priv->vdec);
		if (r < 0) {
			pr_err("psparser_init  failed\n");
			goto error5;
		}
	}
#ifdef CONFIG_AM_VDEC_REAL
	if (port->type & PORT_TYPE_RM) {
		rm_set_vasid(
			(port->flag & PORT_FLAG_VID) ? port->vid : 0xffff,
			(port->flag & PORT_FLAG_AID) ? port->aid : 0xffff);
	}
#endif
#if 1	/* MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6TVD */
	if (!NO_VDEC2_INIT) {
		if ((port->type & PORT_TYPE_VIDEO)
			&& (port->vformat == VFORMAT_H264_4K2K))
			stbuf_vdec2_init(pvbuf);
	}
#endif

	if ((port->type & PORT_TYPE_VIDEO) &&
		(vdec->port_flag & PORT_FLAG_VFORMAT))
		/* connect vdec at the end after all HW initialization */
		vdec_connect(vdec);

	tsync_audio_break(0);	/* clear audio break */
	set_vsync_pts_inc_mode(0);	/* clear video inc */

	port_set_inited(priv);

	mutex_unlock(&amstream_mutex);
	return 0;
	/*errors follow here */
error5:
	tsdemux_release();
error4:
	sub_port_release(port, psbuf);
error3:
	video_port_release(priv, pvbuf, 0);
error2:
	audio_port_release(port, pabuf, 0);
error1:
	mutex_unlock(&amstream_mutex);
	return r;
}

static int amstream_port_release(struct port_priv_s *priv)
{
	struct stream_port_s *port = priv->port;
	struct stream_buf_s *pvbuf = &bufs[BUF_TYPE_VIDEO];
	struct stream_buf_s *pabuf = &bufs[BUF_TYPE_AUDIO];
	struct stream_buf_s *psbuf = &bufs[BUF_TYPE_SUBTITLE];

	if (has_hevc_vdec()) {
		if (port->vformat == VFORMAT_HEVC
			|| port->vformat == VFORMAT_AVS2
			|| port->vformat == VFORMAT_VP9)
			pvbuf = &bufs[BUF_TYPE_HEVC];
	}

	if (port->type & PORT_TYPE_MPTS) {
		tsync_pcr_stop();
		tsdemux_release();
	}

	if (port->type & PORT_TYPE_MPPS)
		psparser_release();

	if (port->type & PORT_TYPE_VIDEO)
		video_port_release(priv, pvbuf, 0);

	if (port->type & PORT_TYPE_AUDIO)
		audio_port_release(port, pabuf, 0);

	if (port->type & PORT_TYPE_SUB)
		sub_port_release(port, psbuf);

	port->pcr_inited = 0;
	port->flag = 0;
	return 0;
}

static void amstream_change_avid(struct stream_port_s *port)
{
	if (port->type & PORT_TYPE_MPTS) {
		tsdemux_change_avid(
		(port->flag & PORT_FLAG_VID) ? port->vid : 0xffff,
		(port->flag & PORT_FLAG_AID) ? port->aid : 0xffff);
	}

	if (port->type & PORT_TYPE_MPPS) {
		psparser_change_avid(
		(port->flag & PORT_FLAG_VID) ? port->vid : 0xffff,
		(port->flag & PORT_FLAG_AID) ? port->aid : 0xffff);
	}

#ifdef CONFIG_AM_VDEC_REAL
	if (port->type & PORT_TYPE_RM) {
		rm_set_vasid(
		(port->flag & PORT_FLAG_VID) ? port->vid : 0xffff,
		(port->flag & PORT_FLAG_AID) ? port->aid : 0xffff);
	}
#endif
}

static void amstream_change_sid(struct stream_port_s *port)
{
	if (port->type & PORT_TYPE_MPTS) {
		tsdemux_change_sid(
		(port->flag & PORT_FLAG_SID) ? port->sid : 0xffff);
	}

	if (port->type & PORT_TYPE_MPPS) {
		psparser_change_sid(
		(port->flag & PORT_FLAG_SID) ? port->sid : 0xffff);
	}
}

/**************************************************/
static ssize_t amstream_vbuf_write(struct file *file, const char *buf,
					size_t count, loff_t *ppos)
{
	struct port_priv_s *priv = (struct port_priv_s *)file->private_data;
	struct stream_port_s *port = priv->port;
	struct stream_buf_s *pbuf = NULL;
	int r;
	if (has_hevc_vdec()) {
		pbuf = (port->type & PORT_TYPE_HEVC) ? &bufs[BUF_TYPE_HEVC] :
			&bufs[BUF_TYPE_VIDEO];
	} else
		pbuf = &bufs[BUF_TYPE_VIDEO];

	if (!(port_get_inited(priv))) {
		r = amstream_port_init(priv);
		if (r < 0)
			return r;
	}

	if (priv->vdec->port_flag & PORT_FLAG_DRM)
		r = drm_write(file, pbuf, buf, count);
	else
		r = esparser_write(file, pbuf, buf, count);
	if (slow_input) {
		pr_info("slow_input: es codec write size %x\n", r);
		msleep(3000);
	}
#ifdef DATA_DEBUG
	debug_file_write(buf, r);
#endif

	return r;
}

static ssize_t amstream_vframe_write(struct file *file, const char *buf,
					   size_t count, loff_t *ppos)
{
	struct port_priv_s *priv = (struct port_priv_s *)file->private_data;
	ssize_t ret;
	int wait_max_cnt = 5;
#ifdef DATA_DEBUG
	debug_file_write(buf, count);
#endif
	do {
		ret = vdec_write_vframe(priv->vdec, buf, count);
		if (file->f_flags & O_NONBLOCK) {
			break;/*alway return for no block mode.*/
		} else if (ret == -EAGAIN) {
			int level;
			level = vdec_input_level(&priv->vdec->input);
			if (wait_max_cnt-- < 0)
				break;
			msleep(20);
		}
	} while (ret == -EAGAIN);
	return ret;
}

static ssize_t amstream_abuf_write(struct file *file, const char *buf,
					size_t count, loff_t *ppos)
{
	struct port_priv_s *priv = (struct port_priv_s *)file->private_data;
	struct stream_port_s *port = priv->port;
	struct stream_buf_s *pbuf = &bufs[BUF_TYPE_AUDIO];
	int r;

	if (!(port_get_inited(priv))) {
		r = amstream_port_init(priv);
		if (r < 0)
			return r;
	}

	if (port->flag & PORT_FLAG_DRM)
		r = drm_write(file, pbuf, buf, count);
	else
		r = esparser_write(file, pbuf, buf, count);

	return r;
}

static ssize_t amstream_mpts_write(struct file *file, const char *buf,
		size_t count, loff_t *ppos)
{
	struct port_priv_s *priv = (struct port_priv_s *)file->private_data;
	struct stream_port_s *port = priv->port;
	struct stream_buf_s *pabuf = &bufs[BUF_TYPE_AUDIO];
	struct stream_buf_s *pvbuf = NULL;
	int r = 0;

	if (has_hevc_vdec()) {
		pvbuf =	(port->vformat == VFORMAT_HEVC ||
					port->vformat == VFORMAT_AVS2 ||
					port->vformat == VFORMAT_VP9) ?
			&bufs[BUF_TYPE_HEVC] : &bufs[BUF_TYPE_VIDEO];
	} else
		pvbuf = &bufs[BUF_TYPE_VIDEO];

	if (!(port_get_inited(priv))) {
		r = amstream_port_init(priv);
		if (r < 0)
			return r;
	}
#ifdef DATA_DEBUG
	debug_file_write(buf, count);
#endif
	if (port->flag & PORT_FLAG_DRM)
		r = drm_tswrite(file, pvbuf, pabuf, buf, count);
	else
		r = tsdemux_write(file, pvbuf, pabuf, buf, count);
	if (slow_input) {
		pr_info("slow_input: ts codec write size %x\n", r);
		msleep(3000);
	}
	return r;
}

static ssize_t amstream_mpps_write(struct file *file, const char *buf,
					size_t count, loff_t *ppos)
{
	struct port_priv_s *priv = (struct port_priv_s *)file->private_data;
	struct stream_buf_s *pvbuf = &bufs[BUF_TYPE_VIDEO];
	struct stream_buf_s *pabuf = &bufs[BUF_TYPE_AUDIO];
	int r;

	if (!(port_get_inited(priv))) {
		r = amstream_port_init(priv);
		if (r < 0)
			return r;
	}
	return psparser_write(file, pvbuf, pabuf, buf, count);
}

#ifdef CONFIG_AM_VDEC_REAL
static ssize_t amstream_mprm_write(struct file *file, const char *buf,
					size_t count, loff_t *ppos)
{
	struct port_priv_s *priv = (struct port_priv_s *)file->private_data;
	struct stream_buf_s *pvbuf = &bufs[BUF_TYPE_VIDEO];
	struct stream_buf_s *pabuf = &bufs[BUF_TYPE_AUDIO];
	int r;

	if (!(port_get_inited(priv))) {
		r = amstream_port_init(priv);
		if (r < 0)
			return r;
	}
	return rmparser_write(file, pvbuf, pabuf, buf, count);
}
#endif

static ssize_t amstream_sub_read(struct file *file, char __user *buf,
					size_t count, loff_t *ppos)
{
	u32 sub_rp, sub_wp, sub_start, data_size, res;
	struct stream_buf_s *s_buf = &bufs[BUF_TYPE_SUBTITLE];

	if (sub_port_inited == 0)
		return 0;

	sub_rp = stbuf_sub_rp_get();
	sub_wp = stbuf_sub_wp_get();
	sub_start = stbuf_sub_start_get();

	if (sub_wp == sub_rp || sub_rp == 0)
		return 0;

	if (sub_wp > sub_rp)
		data_size = sub_wp - sub_rp;
	else
		data_size = s_buf->buf_size - sub_rp + sub_wp;

	if (data_size > count)
		data_size = count;

	if (sub_wp < sub_rp) {
		int first_num = s_buf->buf_size - (sub_rp - sub_start);

		if (data_size <= first_num) {
			res = copy_to_user((void *)buf,
				(void *)(codec_mm_phys_to_virt(sub_rp)),
				data_size);
			stbuf_sub_rp_set(sub_rp + data_size - res);

			return data_size - res;
		} else {
			if (first_num > 0) {
				res = copy_to_user((void *)buf,
				(void *)(codec_mm_phys_to_virt(sub_rp)),
					first_num);
				stbuf_sub_rp_set(sub_rp + first_num -
							res);

				return first_num - res;
			}

			res = copy_to_user((void *)buf,
				(void *)(codec_mm_phys_to_virt(sub_start)),
				data_size - first_num);

			stbuf_sub_rp_set(sub_start + data_size -
				first_num - res);

			return data_size - first_num - res;
		}
	} else {
		res =
			copy_to_user((void *)buf,
				(void *)(codec_mm_phys_to_virt(sub_rp)),
				data_size);

		stbuf_sub_rp_set(sub_rp + data_size - res);

		return data_size - res;
	}
}

static ssize_t amstream_sub_write(struct file *file, const char *buf,
			size_t count, loff_t *ppos)
{
	struct port_priv_s *priv = (struct port_priv_s *)file->private_data;
	struct stream_buf_s *pbuf = &bufs[BUF_TYPE_SUBTITLE];
	int r;

	if (!(port_get_inited(priv))) {
		r = amstream_port_init(priv);
		if (r < 0)
			return r;
	}
	r = esparser_write(file, pbuf, buf, count);
	if (r < 0)
		return r;

	wakeup_sub_poll();

	return r;
}

static unsigned int amstream_sub_poll(struct file *file,
		poll_table *wait_table)
{
	poll_wait(file, &amstream_sub_wait, wait_table);

	if (atomic_read(&subdata_ready)) {
		atomic_dec(&subdata_ready);
		return POLLOUT | POLLWRNORM;
	}

	return 0;
}

static void set_userdata_poc(struct userdata_poc_info_t poc)
{
	userdata_poc_info[userdata_poc_wi] = poc;
	userdata_poc_wi++;
	if (userdata_poc_wi == USERDATA_FIFO_NUM)
		userdata_poc_wi = 0;
}
EXPORT_SYMBOL(set_userdata_poc);

void init_userdata_fifo(void)
{
	userdata_poc_ri = 0;
	userdata_poc_wi = 0;
	userdata_length = 0;
}
EXPORT_SYMBOL(init_userdata_fifo);

void reset_userdata_fifo(int bInit)
{
	struct stream_buf_s *userdata_buf;
	int wi, ri;
	u32 rp, wp;

	mutex_lock(&userdata_mutex);

	wi = userdata_poc_wi;
	ri = userdata_poc_ri;

	userdata_buf = &bufs[BUF_TYPE_USERDATA];
	rp = userdata_buf->buf_rp;
	wp = userdata_buf->buf_wp;
	if (bInit) {
		/* decoder reset */
		userdata_buf->buf_rp = 0;
		userdata_buf->buf_wp = 0;
		userdata_poc_ri = 0;
		userdata_poc_wi = 0;
	} else {
		/* just clean fifo buffer */
		userdata_buf->buf_rp = userdata_buf->buf_wp;
		userdata_poc_ri = userdata_poc_wi;
	}
	userdata_length = 0;
	last_read_wi = userdata_poc_wi;

	mutex_unlock(&userdata_mutex);
	pr_debug("reset_userdata_fifo, bInit=%d, wi=%d, ri=%d, rp=%d, wp=%d\n",
		bInit, wi, ri, rp, wp);
}
EXPORT_SYMBOL(reset_userdata_fifo);

int wakeup_userdata_poll(struct userdata_poc_info_t poc,
						int wp,
						unsigned long start_phyaddr,
						int buf_size,
						 int data_length)
{
	struct stream_buf_s *userdata_buf = &bufs[BUF_TYPE_USERDATA];
	mutex_lock(&userdata_mutex);

	if (data_length & 0x7)
		data_length = (((data_length + 8) >> 3) << 3);
	set_userdata_poc(poc);
	userdata_buf->buf_start = start_phyaddr;
	userdata_buf->buf_wp = wp;
	userdata_buf->buf_size = buf_size;
	atomic_set(&userdata_ready, 1);
	userdata_length += data_length;
	mutex_unlock(&userdata_mutex);

	wake_up_interruptible(&amstream_userdata_wait);
	return userdata_buf->buf_rp;
}
EXPORT_SYMBOL(wakeup_userdata_poll);


void amstream_wakeup_userdata_poll(struct vdec_s *vdec)
{
	int vdec_id;

	vdec_id = vdec->id;
	if (vdec_id > 31) {
		pr_info("Error, not support so many instances(%d) user data push\n",
			vdec_id);
		return;
	}

	mutex_lock(&userdata_mutex);
	ud_ready_vdec_flag |= (1<<vdec_id);

	atomic_set(&userdata_ready, 1);
	mutex_unlock(&userdata_mutex);

	wake_up_interruptible(&amstream_userdata_wait);
}
EXPORT_SYMBOL(amstream_wakeup_userdata_poll);

static unsigned int amstream_userdata_poll(struct file *file,
		poll_table *wait_table)
{
	poll_wait(file, &amstream_userdata_wait, wait_table);
	if (atomic_read(&userdata_ready)) {
		atomic_set(&userdata_ready, 0);
		return POLLIN | POLLRDNORM;
	}
	return 0;
}

static ssize_t amstream_userdata_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	u32 data_size, res, retVal = 0;
	u32 buf_wp, buf_rp, buf_size;
	unsigned long buf_start;
	struct stream_buf_s *userdata_buf = &bufs[BUF_TYPE_USERDATA];
#ifdef DEBUG_USER_DATA
	int old_wi;
#endif

	mutex_lock(&userdata_mutex);

	if (userdata_poc_ri != last_read_wi) {
		/***********************************************
		app picks up poc counter wrong from last read user data
		for H264. So, we need to recalculate userdata_poc_ri
		to the userdata_poc_wi from the last read.
		***********************************************/
#if 0
		pr_info("app pick up poc error: ri = %d, last_wi = %d\n",
			userdata_poc_ri, last_read_wi);
#endif
		userdata_poc_ri = last_read_wi;
	}

	buf_wp = userdata_buf->buf_wp;
	buf_rp = userdata_buf->buf_rp;
	buf_size = userdata_buf->buf_size;
	buf_start = userdata_buf->buf_start;
#ifdef DEBUG_USER_DATA
	old_wi = last_read_wi;
#endif
	last_read_wi = userdata_poc_wi;
	mutex_unlock(&userdata_mutex);

	if (buf_start == 0 || buf_size == 0)
		return 0;
	if (buf_wp == buf_rp)
		return 0;
	if (buf_wp > buf_rp)
		data_size = buf_wp - buf_rp;
	else
		data_size = buf_size - buf_rp + buf_wp;

	if (data_size > count)
		data_size = count;
#ifdef DEBUG_USER_DATA
	pr_info("wi:%d ri:%d wp:%d rp:%d size:%d, last_read_wi=%d\n",
		userdata_poc_wi, userdata_poc_ri,
		buf_wp, buf_rp, data_size, old_wi);
#endif
	if (buf_wp < buf_rp) {
		int first_num = buf_size - buf_rp;
		if (data_size <= first_num) {
			res = copy_to_user((void *)buf,
				(void *)((buf_rp +
				buf_start)), data_size);
			if (res)
				pr_info("p1 read not end res=%d, request=%d\n",
					res, data_size);

			mutex_lock(&userdata_mutex);
			userdata_buf->buf_rp += data_size - res;
			mutex_unlock(&userdata_mutex);
			retVal = data_size - res;
		} else {
			if (first_num > 0) {
				res = copy_to_user((void *)buf,
				(void *)((buf_rp +
				buf_start)), first_num);
				if (res)
					pr_info("p2 read not end res=%d, request=%d\n",
						res, first_num);

				res = copy_to_user((void *)buf+first_num,
				(void *)(buf_start),
				data_size - first_num);

				if (res)
					pr_info("p3 read not end res=%d, request=%d\n",
						res, data_size - first_num);

				mutex_lock(&userdata_mutex);
				userdata_buf->buf_rp += data_size;
				if (userdata_buf->buf_rp >= buf_size)
					userdata_buf->buf_rp =
						userdata_buf->buf_rp - buf_size;
				mutex_unlock(&userdata_mutex);

				retVal = data_size;
			} else {
				/* first_num == 0*/
				res = copy_to_user((void *)buf,
				(void *)((buf_start)),
				data_size - first_num);
				mutex_lock(&userdata_mutex);
				userdata_buf->buf_rp =
					data_size - first_num - res;
				mutex_unlock(&userdata_mutex);
				retVal = data_size - first_num - res;
			}
		}
	} else {
		res = copy_to_user((void *)buf,
			(void *)((buf_rp + buf_start)),
			data_size);
		if (res)
			pr_info("p4 read not end res=%d, request=%d\n",
				res, data_size);

		mutex_lock(&userdata_mutex);
		userdata_buf->buf_rp += data_size - res;
		mutex_unlock(&userdata_mutex);
		retVal = data_size - res;
	}
	return retVal;
}

static int amstream_open(struct inode *inode, struct file *file)
{
	s32 i;
	struct stream_port_s *s;
	struct stream_port_s *port = &ports[iminor(inode)];
	struct port_priv_s *priv;
#ifdef G12A_BRINGUP_DEBUG
	if (vdec_get_debug_flags() & 0xff0000) {
		pr_info("%s force open port %d\n",
			__func__,
			((vdec_get_debug_flags() >> 16) & 0xff) - 1);
		port = &ports[((vdec_get_debug_flags() >> 16) & 0xff) - 1];
	}
	pr_info("%s, port name %s\n", __func__, port->name);
#endif
	if (iminor(inode) >= amstream_port_num)
		return -ENODEV;

	mutex_lock(&amstream_mutex);

	if (port->type & PORT_TYPE_VIDEO) {
		for (s = &ports[0], i = 0; i < amstream_port_num; i++, s++) {
			if ((!is_mult_inc(s->type)) &&
				(s->type & PORT_TYPE_VIDEO) &&
				(s->flag & PORT_FLAG_IN_USE)) {
				mutex_unlock(&amstream_mutex);
				return -EBUSY;
			}
		}
	}

	if ((port->flag & PORT_FLAG_IN_USE) &&
		((port->type & PORT_TYPE_FRAME) == 0)) {
		mutex_unlock(&amstream_mutex);
		return -EBUSY;
	}

	/* check other ports conflicts for audio */
	for (s = &ports[0], i = 0; i < amstream_port_num; i++, s++) {
		if ((s->flag & PORT_FLAG_IN_USE) &&
			((port->type) & (s->type) & PORT_TYPE_AUDIO)) {
			mutex_unlock(&amstream_mutex);
			return -EBUSY;
		}
	}

	priv = kzalloc(sizeof(struct port_priv_s), GFP_KERNEL);
	if (priv == NULL) {
		mutex_unlock(&amstream_mutex);
		return -ENOMEM;
	}

	mutex_init(&priv->mutex);

	priv->port = port;

	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M6) {
		/* TODO: mod gate */
		/* switch_mod_gate_by_name("demux", 1); */
		amports_switch_gate("demux", 1);
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M8) {
			/* TODO: clc gate */
			/* CLK_GATE_ON(HIU_PARSER_TOP); */
			amports_switch_gate("parser_top", 1);
		}

		if (port->type & PORT_TYPE_VIDEO) {
			/* TODO: mod gate */
			/* switch_mod_gate_by_name("vdec", 1); */
			amports_switch_gate("vdec", 1);

			if (has_hevc_vdec()) {
				if (port->type &
					(PORT_TYPE_MPTS | PORT_TYPE_HEVC))
					vdec_poweron(VDEC_HEVC);

				if ((port->type & PORT_TYPE_HEVC) == 0)
					vdec_poweron(VDEC_1);
			} else {
				if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M8)
					vdec_poweron(VDEC_1);
			}
		}

		if (port->type & PORT_TYPE_AUDIO) {
			/* TODO: mod gate */
			/* switch_mod_gate_by_name("audio", 1); */
			amports_switch_gate("audio", 1);
		}
	}

	port->vid = 0;
	port->aid = 0;
	port->sid = 0;
	port->pcrid = 0xffff;
	file->f_op = port->fops;
	file->private_data = priv;

	port->flag = PORT_FLAG_IN_USE;
	port->pcr_inited = 0;
#ifdef DATA_DEBUG
	debug_filp = filp_open(DEBUG_FILE_NAME, O_WRONLY, 0);
	if (IS_ERR(debug_filp)) {
		pr_err("amstream: open debug file failed\n");
		debug_filp = NULL;
	}
#endif
	mutex_unlock(&amstream_mutex);

	if (port->type & PORT_TYPE_VIDEO) {
		priv->vdec = vdec_create(port, NULL);

		if (priv->vdec == NULL) {
			port->flag = 0;
			kfree(priv);
			pr_err("amstream: vdec creation failed\n");
			return -ENOMEM;
		}

		if ((port->type & PORT_TYPE_DUALDEC) ||
			(vdec_get_debug_flags() & 0x100)) {
			priv->vdec->slave = vdec_create(port, priv->vdec);

			if (priv->vdec->slave == NULL) {
				vdec_release(priv->vdec);
				port->flag = 0;
				kfree(priv);
				pr_err("amstream: sub vdec creation failed\n");
				return -ENOMEM;
			}
		}
	}
	return 0;
}

static int amstream_release(struct inode *inode, struct file *file)
{
	struct port_priv_s *priv = file->private_data;
	struct stream_port_s *port = priv->port;
	struct vdec_s *slave = NULL;
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
	u32 port_flag = 0;
#endif

	if (iminor(inode) >= amstream_port_num)
		return -ENODEV;

	mutex_lock(&amstream_mutex);

	if (port_get_inited(priv))
		amstream_port_release(priv);

	if (priv->vdec) {
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
		port_flag = priv->vdec->port_flag;
#endif
		if (priv->vdec->slave)
			slave = priv->vdec->slave;
		vdec_release(priv->vdec);
		if (slave)
			vdec_release(slave);
		priv->vdec = NULL;
	}

	if ((port->type & (PORT_TYPE_AUDIO | PORT_TYPE_VIDEO)) ==
		PORT_TYPE_AUDIO) {
		s32 i;
		struct stream_port_s *s;

		for (s = &ports[0], i = 0; i < amstream_port_num; i++, s++) {
			if ((s->flag & PORT_FLAG_IN_USE)
				&& (s->type & PORT_TYPE_VIDEO))
				break;
		}
		if (i == amstream_port_num)
			timestamp_firstvpts_set(0);
	}
	port->flag = 0;

	/* timestamp_pcrscr_set(0); */

#ifdef DATA_DEBUG
	if (debug_filp) {
		filp_close(debug_filp, current->files);
		debug_filp = NULL;
		debug_file_pos = 0;
	}
#endif
	if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M6) {
		if (port->type & PORT_TYPE_VIDEO) {
			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M8) {
#ifndef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
				if (has_hevc_vdec())
					vdec_poweroff(VDEC_HEVC);

				vdec_poweroff(VDEC_1);
#else
			if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_TXLX
				&& port->vformat == VFORMAT_H264
				&& bufs[BUF_TYPE_VIDEO].for_4k)
				vdec_poweroff(VDEC_HEVC);

			if ((port->vformat == VFORMAT_HEVC
					|| port->vformat == VFORMAT_AVS2
					|| port->vformat == VFORMAT_VP9)) {
					vdec_poweroff(VDEC_HEVC);
				} else {
					vdec_poweroff(VDEC_1);
				}
#endif
			}
			/* TODO: mod gate */
			/* switch_mod_gate_by_name("vdec", 0); */
			amports_switch_gate("vdec", 0);
		}

		if (port->type & PORT_TYPE_AUDIO) {
			/* TODO: mod gate */
			/* switch_mod_gate_by_name("audio", 0); */
			/* amports_switch_gate("audio", 0); */
		}

		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M8) {
			/* TODO: clc gate */
			/* CLK_GATE_OFF(HIU_PARSER_TOP); */
			amports_switch_gate("parser_top", 0);
		}
		/* TODO: mod gate */
		/* switch_mod_gate_by_name("demux", 0); */
		amports_switch_gate("demux", 0);
	}

	mutex_destroy(&priv->mutex);

	kfree(priv);

	mutex_unlock(&amstream_mutex);
	return 0;
}

static long amstream_ioctl_get_version(struct port_priv_s *priv,
	ulong arg)
{
	int version = (AMSTREAM_IOC_VERSION_FIRST & 0xffff) << 16
		| (AMSTREAM_IOC_VERSION_SECOND & 0xffff);
	put_user(version, (u32 __user *)arg);

	return 0;
}
static long amstream_ioctl_get(struct port_priv_s *priv, ulong arg)
{
	struct stream_port_s *this = priv->port;
	long r = 0;

	struct am_ioctl_parm parm;

	if (copy_from_user
		((void *)&parm, (void *)arg,
		 sizeof(parm)))
		r = -EFAULT;

	switch (parm.cmd) {
	case AMSTREAM_GET_SUB_LENGTH:
		if ((this->type & PORT_TYPE_SUB) ||
			(this->type & PORT_TYPE_SUB_RD)) {
			u32 sub_wp, sub_rp;
			struct stream_buf_s *psbuf = &bufs[BUF_TYPE_SUBTITLE];
			int val;

			sub_wp = stbuf_sub_wp_get();
			sub_rp = stbuf_sub_rp_get();

			if (sub_wp == sub_rp)
				val = 0;
			else if (sub_wp > sub_rp)
				val = sub_wp - sub_rp;
			else
				val = psbuf->buf_size - (sub_rp - sub_wp);
			parm.data_32 = val;
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_GET_UD_LENGTH:
		if (this->type & PORT_TYPE_USERDATA) {
			parm.data_32 = userdata_length;
			userdata_length = 0;
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_GET_APTS_LOOKUP:
		if (this->type & PORT_TYPE_AUDIO) {
			u32 pts = 0, offset;

			offset = parm.data_32;
			pts_lookup_offset(PTS_TYPE_AUDIO, offset, &pts, 300);
			parm.data_32 = pts;
		}
		break;
	case AMSTREAM_GET_FIRST_APTS_FLAG:
		if (this->type & PORT_TYPE_AUDIO) {
			parm.data_32 = first_pts_checkin_complete(
				PTS_TYPE_AUDIO);
		}
		break;
	case AMSTREAM_GET_APTS:
		parm.data_32 = timestamp_apts_get();
		break;
	case AMSTREAM_GET_VPTS:
		parm.data_32 = timestamp_vpts_get();
		break;
	case AMSTREAM_GET_PCRSCR:
		parm.data_32 = timestamp_pcrscr_get();
		break;
	case AMSTREAM_GET_LAST_CHECKIN_APTS:
		parm.data_32 = get_last_checkin_pts(PTS_TYPE_AUDIO);
		break;
	case AMSTREAM_GET_LAST_CHECKIN_VPTS:
		parm.data_32 = get_last_checkin_pts(PTS_TYPE_VIDEO);
		break;
	case AMSTREAM_GET_LAST_CHECKOUT_APTS:
		parm.data_32 = get_last_checkout_pts(PTS_TYPE_AUDIO);
		break;
	case AMSTREAM_GET_LAST_CHECKOUT_VPTS:
		parm.data_32 = get_last_checkout_pts(PTS_TYPE_VIDEO);
		break;
	case AMSTREAM_GET_SUB_NUM:
		parm.data_32 = psparser_get_sub_found_num();
		break;
	case AMSTREAM_GET_VIDEO_DELAY_LIMIT_MS:
		parm.data_32 = bufs[BUF_TYPE_VIDEO].max_buffer_delay_ms;
		break;
	case AMSTREAM_GET_AUDIO_DELAY_LIMIT_MS:
		parm.data_32 = bufs[BUF_TYPE_AUDIO].max_buffer_delay_ms;
		break;
	case AMSTREAM_GET_VIDEO_CUR_DELAY_MS: {
			int delay;

			delay = calculation_stream_delayed_ms(
				PTS_TYPE_VIDEO, NULL, NULL);
			if (delay >= 0)
				parm.data_32 = delay;
			else
				parm.data_32 = 0;
		}
		break;

	case AMSTREAM_GET_AUDIO_CUR_DELAY_MS: {
			int delay;

			delay = calculation_stream_delayed_ms(
				PTS_TYPE_AUDIO, NULL, NULL);
			if (delay >= 0)
				parm.data_32 = delay;
			else
				parm.data_32 = 0;
		}
		break;
	case AMSTREAM_GET_AUDIO_AVG_BITRATE_BPS: {
			int delay;
			u32 avgbps;

			delay = calculation_stream_delayed_ms(
				PTS_TYPE_AUDIO, NULL, &avgbps);
			if (delay >= 0)
				parm.data_32 = avgbps;
			else
				parm.data_32 = 0;
		}
		break;
	case AMSTREAM_GET_VIDEO_AVG_BITRATE_BPS: {
			int delay;
			u32 avgbps;

			delay = calculation_stream_delayed_ms(
				PTS_TYPE_VIDEO, NULL, &avgbps);
			if (delay >= 0)
				parm.data_32 = avgbps;
			else
				parm.data_32 = 0;
		}
		break;
	case AMSTREAM_GET_ION_ID:
		parm.data_32 = priv->vdec->vf_receiver_inst;
		break;
	case AMSTREAM_GET_NEED_MORE_DATA:
		parm.data_32 = vdec_need_more_data(priv->vdec);
		break;
	case AMSTREAM_GET_FREED_HANDLE:
		parm.data_32 = vdec_input_get_freed_handle(priv->vdec);
		break;
	default:
		r = -ENOIOCTLCMD;
		break;
	}
	/* pr_info("parm size:%d\n", sizeof(parm)); */
	if (r == 0) {
		if (copy_to_user((void *)arg, &parm, sizeof(parm)))
			r = -EFAULT;
	}

	return r;

}
static long amstream_ioctl_set(struct port_priv_s *priv, ulong arg)
{
	struct  stream_port_s *this = priv->port;
	struct am_ioctl_parm parm;
	long r = 0;

	if (copy_from_user
		((void *)&parm, (void *)arg,
		 sizeof(parm)))
		r = -EFAULT;

	switch (parm.cmd) {
	case AMSTREAM_SET_VB_START:
		if ((this->type & PORT_TYPE_VIDEO) &&
			((bufs[BUF_TYPE_VIDEO].flag & BUF_FLAG_IN_USE) == 0)) {
			if (has_hevc_vdec())
				bufs[BUF_TYPE_HEVC].buf_start = parm.data_32;
			bufs[BUF_TYPE_VIDEO].buf_start = parm.data_32;
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_SET_VB_SIZE:
		if ((this->type & PORT_TYPE_VIDEO) &&
			((bufs[BUF_TYPE_VIDEO].flag & BUF_FLAG_IN_USE) == 0)) {
			if (bufs[BUF_TYPE_VIDEO].flag & BUF_FLAG_ALLOC) {
				if (has_hevc_vdec()) {
					r = stbuf_change_size(
						&bufs[BUF_TYPE_HEVC],
						parm.data_32,
						false);
				}
				r += stbuf_change_size(
						&bufs[BUF_TYPE_VIDEO],
						parm.data_32,
						false);
			}
		} else if (this->type & PORT_TYPE_FRAME) {
			/* todo: frame based set max buffer size */
			r = 0;
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_SET_AB_START:
		if ((this->type & PORT_TYPE_AUDIO) &&
			((bufs[BUF_TYPE_AUDIO].flag & BUF_FLAG_IN_USE) == 0))
			bufs[BUF_TYPE_AUDIO].buf_start = parm.data_32;
		else
			r = -EINVAL;
		break;
	case AMSTREAM_SET_AB_SIZE:
		if ((this->type & PORT_TYPE_AUDIO) &&
			((bufs[BUF_TYPE_AUDIO].flag & BUF_FLAG_IN_USE) == 0)) {
			if (bufs[BUF_TYPE_AUDIO].flag & BUF_FLAG_ALLOC) {
				r = stbuf_change_size(
					&bufs[BUF_TYPE_AUDIO],
					parm.data_32,
					false);
			}
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_SET_VFORMAT:
		if ((this->type & PORT_TYPE_VIDEO) &&
			(parm.data_vformat < VFORMAT_MAX)) {
			this->vformat = parm.data_vformat;
			this->flag |= PORT_FLAG_VFORMAT;

			vdec_set_format(priv->vdec, this->vformat);
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_SET_AFORMAT:
		if ((this->type & PORT_TYPE_AUDIO) &&
			(parm.data_aformat < AFORMAT_MAX)) {
			memset(&audio_dec_info, 0,
				   sizeof(struct audio_info));
			/* for new format,reset the audio info. */
			this->aformat = parm.data_aformat;
			this->flag |= PORT_FLAG_AFORMAT;
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_SET_VID:
		if (this->type & PORT_TYPE_VIDEO) {
			this->vid = parm.data_32;
			this->flag |= PORT_FLAG_VID;
		} else
			r = -EINVAL;

		break;
	case AMSTREAM_SET_AID:
		if (this->type & PORT_TYPE_AUDIO) {
			this->aid = parm.data_32;
			this->flag |= PORT_FLAG_AID;

			if (port_get_inited(priv)) {
				tsync_audio_break(1);
				amstream_change_avid(this);
			}
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_SET_SID:
		if (this->type & PORT_TYPE_SUB) {
			this->sid = parm.data_32;
			this->flag |= PORT_FLAG_SID;

			if (port_get_inited(priv))
				amstream_change_sid(this);
		} else
			r = -EINVAL;

		break;
	case AMSTREAM_IOC_PCRID:
		this->pcrid = parm.data_32;
		this->pcr_inited = 1;
		pr_err("set pcrid = 0x%x\n", this->pcrid);
		break;
	case AMSTREAM_SET_ACHANNEL:
		if (this->type & PORT_TYPE_AUDIO) {
			this->achanl = parm.data_32;
			set_ch_num_info(parm.data_32);
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_SET_SAMPLERATE:
		if (this->type & PORT_TYPE_AUDIO) {
			this->asamprate = parm.data_32;
			set_sample_rate_info(parm.data_32);
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_SET_DATAWIDTH:
		if (this->type & PORT_TYPE_AUDIO)
			this->adatawidth = parm.data_32;
		else
			r = -EINVAL;
		break;
	case AMSTREAM_SET_TSTAMP:
		if ((this->type & (PORT_TYPE_AUDIO | PORT_TYPE_VIDEO)) ==
			((PORT_TYPE_AUDIO | PORT_TYPE_VIDEO)))
			r = -EINVAL;
		else if (this->type & PORT_TYPE_FRAME)
			r = vdec_set_pts(priv->vdec, parm.data_32);
		else if (has_hevc_vdec() && this->type & PORT_TYPE_HEVC)
			r = es_vpts_checkin(&bufs[BUF_TYPE_HEVC],
				parm.data_32);
		else if (this->type & PORT_TYPE_VIDEO)
			r = es_vpts_checkin(&bufs[BUF_TYPE_VIDEO],
				parm.data_32);
		else if (this->type & PORT_TYPE_AUDIO)
			r = es_apts_checkin(&bufs[BUF_TYPE_AUDIO],
				parm.data_32);
		break;
	case AMSTREAM_SET_TSTAMP_US64:
		if ((this->type & (PORT_TYPE_AUDIO | PORT_TYPE_VIDEO)) ==
			((PORT_TYPE_AUDIO | PORT_TYPE_VIDEO)))
			r = -EINVAL;
		else {
			u64 pts = parm.data_64;

			if (this->type & PORT_TYPE_FRAME) {
				/*
				 *todo: check upper layer for decoder handler
				 * life sequence or multi-tasking management
				 */
				r = vdec_set_pts64(priv->vdec, pts);
			} else if (has_hevc_vdec()) {
				if (this->type & PORT_TYPE_HEVC) {
					r = es_vpts_checkin_us64(
					&bufs[BUF_TYPE_HEVC], pts);
				} else if (this->type & PORT_TYPE_VIDEO) {
					r = es_vpts_checkin_us64(
					&bufs[BUF_TYPE_VIDEO], pts);
				} else if (this->type & PORT_TYPE_AUDIO) {
					r = es_vpts_checkin_us64(
					&bufs[BUF_TYPE_AUDIO], pts);
				}
			} else {
				if (this->type & PORT_TYPE_VIDEO) {
					r = es_vpts_checkin_us64(
					&bufs[BUF_TYPE_VIDEO], pts);
				} else if (this->type & PORT_TYPE_AUDIO) {
					r = es_vpts_checkin_us64(
					&bufs[BUF_TYPE_AUDIO], pts);
				}
			}
		}
		break;
	case AMSTREAM_PORT_INIT:
		r = amstream_port_init(priv);
		break;
	case AMSTREAM_SET_TRICKMODE:
		if ((this->type & PORT_TYPE_VIDEO) == 0)
			return -EINVAL;
		r = vdec_set_trickmode(priv->vdec, parm.data_32);
		if (r == -1)
			return -ENODEV;
		break;

	case AMSTREAM_AUDIO_RESET:
		if (this->type & PORT_TYPE_AUDIO) {
			struct stream_buf_s *pabuf = &bufs[BUF_TYPE_AUDIO];

			r = audio_port_reset(this, pabuf);
		} else
			r = -EINVAL;

		break;
	case AMSTREAM_SUB_RESET:
		if (this->type & PORT_TYPE_SUB) {
			struct stream_buf_s *psbuf = &bufs[BUF_TYPE_SUBTITLE];

			r = sub_port_reset(this, psbuf);
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_DEC_RESET:
		tsync_set_dec_reset();
		break;
	case AMSTREAM_SET_TS_SKIPBYTE:
		tsdemux_set_skipbyte(parm.data_32);
		break;
	case AMSTREAM_SET_SUB_TYPE:
		sub_type = parm.data_32;
		break;
	case AMSTREAM_SET_PCRSCR:
		timestamp_pcrscr_set(parm.data_32);
		break;
	case AMSTREAM_SET_DEMUX:
		tsdemux_set_demux(parm.data_32);
		break;
	case AMSTREAM_SET_VIDEO_DELAY_LIMIT_MS:
		if (has_hevc_vdec())
			bufs[BUF_TYPE_HEVC].max_buffer_delay_ms = parm.data_32;
		bufs[BUF_TYPE_VIDEO].max_buffer_delay_ms = parm.data_32;
		break;
	case AMSTREAM_SET_AUDIO_DELAY_LIMIT_MS:
		bufs[BUF_TYPE_AUDIO].max_buffer_delay_ms = parm.data_32;
		break;
	case AMSTREAM_SET_DRMMODE:
		if (parm.data_32 == 1) {
			pr_debug("set drmmode\n");
			this->flag |= PORT_FLAG_DRM;
			if ((this->type & PORT_TYPE_VIDEO) &&
				(priv->vdec))
				priv->vdec->port_flag |= PORT_FLAG_DRM;
		} else {
			this->flag &= (~PORT_FLAG_DRM);
			pr_debug("no drmmode\n");
		}
		break;
	case AMSTREAM_SET_APTS: {
		unsigned int pts;

		pts = parm.data_32;
		if (tsync_get_mode() == TSYNC_MODE_PCRMASTER)
			tsync_pcr_set_apts(pts);
		else
			tsync_set_apts(pts);
		break;
	}
	case AMSTREAM_SET_FRAME_BASE_PATH:
		if (is_mult_inc(this->type) &&
			(parm.frame_base_video_path < FRAME_BASE_PATH_MAX)) {
			vdec_set_video_path(priv->vdec, parm.data_32);
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_SET_EOS:
		if (priv->vdec)
			vdec_set_eos(priv->vdec, parm.data_32);
		break;
	case AMSTREAM_SET_RECEIVE_ID:
		if (is_mult_inc(this->type))
			vdec_set_receive_id(priv->vdec, parm.data_32);
		else
			r = -EINVAL;
		break;
	case AMSTREAM_SET_IS_RESET:
		if (priv->vdec)
			vdec_set_isreset(priv->vdec, parm.data_32);
		break;
	case AMSTREAM_SET_DV_META_WITH_EL:
		if (priv->vdec) {
			vdec_set_dv_metawithel(priv->vdec, parm.data_32);
			if (vdec_dual(priv->vdec) && priv->vdec->slave)
				vdec_set_dv_metawithel(priv->vdec->slave,
				parm.data_32);
		}
		break;
	case AMSTREAM_SET_NO_POWERDOWN:
		vdec_set_no_powerdown(parm.data_32);
		break;
	default:
		r = -ENOIOCTLCMD;
		break;
	}
	return r;
}
static long amstream_ioctl_get_ex(struct port_priv_s *priv, ulong arg)
{
	struct stream_port_s *this = priv->port;
	long r = 0;
	struct am_ioctl_parm_ex parm;

	if (copy_from_user
		((void *)&parm, (void *)arg,
		 sizeof(parm)))
		r = -EFAULT;

	switch (parm.cmd) {
	case AMSTREAM_GET_EX_VB_STATUS:
		if (this->type & PORT_TYPE_VIDEO) {
			struct am_ioctl_parm_ex *p = &parm;
			struct stream_buf_s *buf = NULL;

			buf = (this->vformat == VFORMAT_HEVC ||
				this->vformat == VFORMAT_AVS2 ||
				this->vformat == VFORMAT_VP9) ?
				&bufs[BUF_TYPE_HEVC] :
				&bufs[BUF_TYPE_VIDEO];

			if (this->type & PORT_TYPE_FRAME) {
				struct vdec_input_status_s status;

				/*
				 *todo: check upper layer for decoder
				 * handler lifecycle
				 */
				if (priv->vdec == NULL) {
					r = -EINVAL;
					break;
				}

				r = vdec_input_get_status(&priv->vdec->input,
							&status);
				if (r == 0) {
					p->status.size = status.size;
					p->status.data_len = status.data_len;
					p->status.free_len = status.free_len;
					p->status.read_pointer =
							status.read_pointer;
				}
				break;
			}

			p->status.size = stbuf_canusesize(buf);
			p->status.data_len = stbuf_level(buf);
			p->status.free_len = stbuf_space(buf);
			p->status.read_pointer = stbuf_rp(buf);
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_GET_EX_AB_STATUS:
		if (this->type & PORT_TYPE_AUDIO) {
			struct am_ioctl_parm_ex *p = &parm;
			struct stream_buf_s *buf = &bufs[BUF_TYPE_AUDIO];


			p->status.size = stbuf_canusesize(buf);
			p->status.data_len = stbuf_level(buf);
			p->status.free_len = stbuf_space(buf);
			p->status.read_pointer = stbuf_rp(buf);

		} else
			r = -EINVAL;
		break;
	case AMSTREAM_GET_EX_VDECSTAT:
		if ((this->type & PORT_TYPE_VIDEO) == 0) {
			pr_err("no video\n");
			return -EINVAL;
		} else {
			struct vdec_info vstatus;
			struct am_ioctl_parm_ex *p = &parm;

			memset(&vstatus, 0, sizeof(vstatus));

			mutex_lock(&priv->mutex);
			if (vdec_status(priv->vdec, &vstatus) == -1) {
				mutex_unlock(&priv->mutex);
				return -ENODEV;
			}
			mutex_unlock(&priv->mutex);

			p->vstatus.width = vstatus.frame_width;
			p->vstatus.height = vstatus.frame_height;
			p->vstatus.fps = vstatus.frame_rate;
			p->vstatus.error_count = vstatus.error_count;
			p->vstatus.status = vstatus.status;
		}
		break;
	case AMSTREAM_GET_EX_ADECSTAT:
		if ((this->type & PORT_TYPE_AUDIO) == 0) {
			pr_err("no audio\n");
			return -EINVAL;
		}
		if (amstream_adec_status == NULL) {
			/*
			 *pr_err("no amstream_adec_status\n");
			 *return -ENODEV;
			 */
			memset(&parm.astatus, 0, sizeof(parm.astatus));
		} else {
			struct adec_status astatus;
			struct am_ioctl_parm_ex *p = &parm;

			amstream_adec_status(&astatus);
			p->astatus.channels = astatus.channels;
			p->astatus.sample_rate = astatus.sample_rate;
			p->astatus.resolution = astatus.resolution;
			p->astatus.error_count = astatus.error_count;
			p->astatus.status = astatus.status;
		}
		break;

	case AMSTREAM_GET_EX_UD_POC:
		if (this->type & PORT_TYPE_USERDATA) {
			struct userdata_poc_info_t userdata_poc =
					userdata_poc_info[userdata_poc_ri];
			memcpy(&parm.data_userdata_info,
					&userdata_poc,
					sizeof(struct userdata_poc_info_t));

			userdata_poc_ri++;
			if (userdata_poc_ri == USERDATA_FIFO_NUM)
				userdata_poc_ri = 0;
		} else
			r = -EINVAL;
		break;
	default:
		r = -ENOIOCTLCMD;
		break;
	}
	/* pr_info("parm size:%zx\n", sizeof(parm)); */
	if (r == 0) {
		if (copy_to_user((void *)arg, &parm, sizeof(parm)))
			r = -EFAULT;
	}
	return r;

}
static long amstream_ioctl_set_ex(struct port_priv_s *priv, ulong arg)
{
	long r = 0;
	return r;
}
static long amstream_ioctl_get_ptr(struct port_priv_s *priv, ulong arg)
{
	long r = 0;

	struct am_ioctl_parm_ptr parm;

	if (copy_from_user
		((void *)&parm, (void *)arg,
		 sizeof(parm)))
		return -EFAULT;

	switch (parm.cmd) {
	case AMSTREAM_GET_PTR_SUB_INFO:
		{
			struct subtitle_info msub_info[MAX_SUB_NUM];
			struct subtitle_info *psub_info[MAX_SUB_NUM];
			int i;

			for (i = 0; i < MAX_SUB_NUM; i++)
				psub_info[i] = &msub_info[i];

			r = psparser_get_sub_info(psub_info);

			if (r == 0) {
				memcpy(parm.pdata_sub_info, msub_info,
					sizeof(struct subtitle_info)
					* MAX_SUB_NUM);
			}
		}
		break;
	default:
		r = -ENOIOCTLCMD;
		break;
	}
	/* pr_info("parm size:%d\n", sizeof(parm)); */
	if (r == 0) {
		if (copy_to_user((void *)arg, &parm, sizeof(parm)))
			r = -EFAULT;
	}

	return r;

}
static long amstream_ioctl_set_ptr(struct port_priv_s *priv, ulong arg)
{
	struct stream_port_s *this = priv->port;
	struct am_ioctl_parm_ptr parm;
	long r = 0;

	if (copy_from_user
		((void *)&parm, (void *)arg,
		 sizeof(parm))) {
		pr_err("[%s]%d, arg err\n", __func__, __LINE__);
		r = -EFAULT;
	}
	switch (parm.cmd) {
	case AMSTREAM_SET_PTR_AUDIO_INFO:
		if ((this->type & PORT_TYPE_VIDEO)
			|| (this->type & PORT_TYPE_AUDIO)) {
			if (parm.pdata_audio_info != NULL) {
				if (copy_from_user
					((void *)&audio_dec_info, (void *)parm.pdata_audio_info,
					 sizeof(audio_dec_info))) {
					pr_err("[%s]%d, arg err\n", __func__, __LINE__);
					r = -EFAULT;
				}
			}
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_SET_PTR_CONFIGS:
		if (this->type & PORT_TYPE_VIDEO) {
			if (!parm.pointer || (parm.len <= 0) ||
				(parm.len > PAGE_SIZE)) {
				r = -EINVAL;
			} else {
				r = copy_from_user(priv->vdec->config,
						parm.pointer, parm.len);
				if (r)
					r = -EINVAL;
				else
					priv->vdec->config_len = parm.len;
			}
		} else
			r = -EINVAL;
		break;
	default:
		r = -ENOIOCTLCMD;
		break;
	}
	return r;
}

static long amstream_do_ioctl_new(struct port_priv_s *priv,
	unsigned int cmd, ulong arg)
{
	long r = 0;
	struct stream_port_s *this = priv->port;

	switch (cmd) {
	case AMSTREAM_IOC_GET_VERSION:
		r = amstream_ioctl_get_version(priv, arg);
		break;
	case AMSTREAM_IOC_GET:
		r = amstream_ioctl_get(priv, arg);
		break;
	case AMSTREAM_IOC_SET:
		r = amstream_ioctl_set(priv, arg);
		break;
	case AMSTREAM_IOC_GET_EX:
		r = amstream_ioctl_get_ex(priv, arg);
		break;
	case AMSTREAM_IOC_SET_EX:
		r = amstream_ioctl_set_ex(priv, arg);
		break;
	case AMSTREAM_IOC_GET_PTR:
		r = amstream_ioctl_get_ptr(priv, arg);
		break;
	case AMSTREAM_IOC_SET_PTR:
		r = amstream_ioctl_set_ptr(priv, arg);
		break;
	case AMSTREAM_IOC_SYSINFO:
		if (this->type & PORT_TYPE_VIDEO)
			r = vdec_set_decinfo(priv->vdec, (void *)arg);
		else
			r = -EINVAL;
		break;
	default:
		r = -ENOIOCTLCMD;
		break;
	}

	return r;
}

static long amstream_do_ioctl_old(struct port_priv_s *priv,
	unsigned int cmd, ulong arg)
{
	struct stream_port_s *this = priv->port;
	long r = 0;

	switch (cmd) {

	case AMSTREAM_IOC_VB_START:
		if ((this->type & PORT_TYPE_VIDEO) &&
			((bufs[BUF_TYPE_VIDEO].flag & BUF_FLAG_IN_USE) == 0)) {
			if (has_hevc_vdec())
				bufs[BUF_TYPE_HEVC].buf_start = arg;
			bufs[BUF_TYPE_VIDEO].buf_start = arg;
		} else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_VB_SIZE:
		if ((this->type & PORT_TYPE_VIDEO) &&
			((bufs[BUF_TYPE_VIDEO].flag & BUF_FLAG_IN_USE) == 0)) {
			if (bufs[BUF_TYPE_VIDEO].flag & BUF_FLAG_ALLOC) {
				if (has_hevc_vdec()) {
					r = stbuf_change_size(
						&bufs[BUF_TYPE_HEVC],
						arg, false);
				}
				r += stbuf_change_size(
						&bufs[BUF_TYPE_VIDEO],
						arg, false);
			}
		} else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_AB_START:
		if ((this->type & PORT_TYPE_AUDIO) &&
			((bufs[BUF_TYPE_AUDIO].flag & BUF_FLAG_IN_USE) == 0))
			bufs[BUF_TYPE_AUDIO].buf_start = arg;
		else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_AB_SIZE:
		if ((this->type & PORT_TYPE_AUDIO) &&
			((bufs[BUF_TYPE_AUDIO].flag & BUF_FLAG_IN_USE) == 0)) {
			if (bufs[BUF_TYPE_AUDIO].flag & BUF_FLAG_ALLOC) {
				r = stbuf_change_size(
					&bufs[BUF_TYPE_AUDIO], arg, false);
			}
		} else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_VFORMAT:
		if ((this->type & PORT_TYPE_VIDEO) && (arg < VFORMAT_MAX)) {
			this->vformat = (enum vformat_e)arg;
			this->flag |= PORT_FLAG_VFORMAT;

			vdec_set_format(priv->vdec, this->vformat);
		} else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_AFORMAT:
		if ((this->type & PORT_TYPE_AUDIO) && (arg < AFORMAT_MAX)) {
			memset(&audio_dec_info, 0,
				   sizeof(struct audio_info));
			/* for new format,reset the audio info. */
			this->aformat = (enum aformat_e)arg;
			this->flag |= PORT_FLAG_AFORMAT;
		} else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_VID:
		if (this->type & PORT_TYPE_VIDEO) {
			this->vid = (u32) arg;
			this->flag |= PORT_FLAG_VID;
		} else
			r = -EINVAL;

		break;

	case AMSTREAM_IOC_AID:
		if (this->type & PORT_TYPE_AUDIO) {
			this->aid = (u32) arg;
			this->flag |= PORT_FLAG_AID;

			if (port_get_inited(priv)) {
				tsync_audio_break(1);
				amstream_change_avid(this);
			}
		} else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_SID:
		if (this->type & PORT_TYPE_SUB) {
			this->sid = (u32) arg;
			this->flag |= PORT_FLAG_SID;

			if (port_get_inited(priv))
				amstream_change_sid(this);
		} else
			r = -EINVAL;

		break;

	case AMSTREAM_IOC_PCRID:
		this->pcrid = (u32) arg;
		this->pcr_inited = 1;
		pr_err("set pcrid = 0x%x\n", this->pcrid);
		break;

	case AMSTREAM_IOC_VB_STATUS:
		if (this->type & PORT_TYPE_VIDEO) {
			struct am_io_param para;
			struct am_io_param *p = &para;
			struct stream_buf_s *buf = NULL;

			buf = (this->vformat == VFORMAT_HEVC ||
					this->vformat == VFORMAT_AVS2 ||
					this->vformat == VFORMAT_VP9) ?
				&bufs[BUF_TYPE_HEVC] :
				&bufs[BUF_TYPE_VIDEO];

			if (this->type & PORT_TYPE_FRAME) {
				struct vdec_input_status_s status;

				/*
				 *todo: check upper layer for decoder
				 * handler lifecycle
				 */
				if (priv->vdec == NULL) {
					r = -EINVAL;
					break;
				}

				r = vdec_input_get_status(&priv->vdec->input,
							&status);
				if (r == 0) {
					p->status.size = status.size;
					p->status.data_len = status.data_len;
					p->status.free_len = status.free_len;
					p->status.read_pointer =
							status.read_pointer;
					if (copy_to_user((void *)arg, p,
						sizeof(para)))
						r = -EFAULT;
				}
				break;
			}

			p->status.size = stbuf_canusesize(buf);
			p->status.data_len = stbuf_level(buf);
			p->status.free_len = stbuf_space(buf);
			p->status.read_pointer = stbuf_rp(buf);
			if (copy_to_user((void *)arg, p, sizeof(para)))
				r = -EFAULT;
			return r;
		}
		r = -EINVAL;
		break;

	case AMSTREAM_IOC_AB_STATUS:
		if (this->type & PORT_TYPE_AUDIO) {
			struct am_io_param para;
			struct am_io_param *p = &para;
			struct stream_buf_s *buf = &bufs[BUF_TYPE_AUDIO];

			p->status.size = stbuf_canusesize(buf);
			p->status.data_len = stbuf_level(buf);
			p->status.free_len = stbuf_space(buf);
			p->status.read_pointer = stbuf_rp(buf);
			if (copy_to_user((void *)arg, p, sizeof(para)))
				r = -EFAULT;
			return r;
		}
		r = -EINVAL;
		break;

	case AMSTREAM_IOC_SYSINFO:
		if (this->type & PORT_TYPE_VIDEO)
			r = vdec_set_decinfo(priv->vdec, (void *)arg);
		else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_ACHANNEL:
		if (this->type & PORT_TYPE_AUDIO) {
			this->achanl = (u32) arg;
			set_ch_num_info((u32) arg);
		} else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_SAMPLERATE:
		if (this->type & PORT_TYPE_AUDIO) {
			this->asamprate = (u32) arg;
			set_sample_rate_info((u32) arg);
		} else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_DATAWIDTH:
		if (this->type & PORT_TYPE_AUDIO)
			this->adatawidth = (u32) arg;
		else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_TSTAMP:
		if ((this->type & (PORT_TYPE_AUDIO | PORT_TYPE_VIDEO)) ==
			((PORT_TYPE_AUDIO | PORT_TYPE_VIDEO)))
			r = -EINVAL;
		else if (this->type & PORT_TYPE_FRAME)
			r = vdec_set_pts(priv->vdec, arg);
		else if (has_hevc_vdec() && this->type & PORT_TYPE_HEVC)
			r = es_vpts_checkin(&bufs[BUF_TYPE_HEVC], arg);
		else if (this->type & PORT_TYPE_VIDEO)
			r = es_vpts_checkin(&bufs[BUF_TYPE_VIDEO], arg);
		else if (this->type & PORT_TYPE_AUDIO)
			r = es_apts_checkin(&bufs[BUF_TYPE_AUDIO], arg);
		break;

	case AMSTREAM_IOC_TSTAMP_uS64:
		if ((this->type & (PORT_TYPE_AUDIO | PORT_TYPE_VIDEO)) ==
			((PORT_TYPE_AUDIO | PORT_TYPE_VIDEO)))
			r = -EINVAL;
		else {
			u64 pts;

			if (copy_from_user
				((void *)&pts, (void *)arg, sizeof(u64)))
				return -EFAULT;
			if (this->type & PORT_TYPE_FRAME) {
				/*
				 *todo: check upper layer for decoder handler
				 * life sequence or multi-tasking management
				 */
				if (priv->vdec)
					r = vdec_set_pts64(priv->vdec, pts);
			} else if (has_hevc_vdec()) {
				if (this->type & PORT_TYPE_HEVC) {
					r = es_vpts_checkin_us64(
					&bufs[BUF_TYPE_HEVC], pts);
				} else if (this->type & PORT_TYPE_VIDEO) {
					r = es_vpts_checkin_us64(
					&bufs[BUF_TYPE_VIDEO], pts);
				} else if (this->type & PORT_TYPE_AUDIO) {
					r = es_vpts_checkin_us64(
					&bufs[BUF_TYPE_AUDIO], pts);
				}
			} else {
				if (this->type & PORT_TYPE_VIDEO) {
					r = es_vpts_checkin_us64(
					&bufs[BUF_TYPE_VIDEO], pts);
				} else if (this->type & PORT_TYPE_AUDIO) {
					r = es_vpts_checkin_us64(
					&bufs[BUF_TYPE_AUDIO], pts);
				}
			}
		}
		break;

	case AMSTREAM_IOC_VDECSTAT:
		if ((this->type & PORT_TYPE_VIDEO) == 0)
			return -EINVAL;
		{
			struct vdec_info vstatus;
			struct am_io_param para;
			struct am_io_param *p = &para;

			memset(&vstatus, 0, sizeof(vstatus));

			mutex_lock(&priv->mutex);
			if (vdec_status(priv->vdec, &vstatus) == -1) {
				mutex_unlock(&priv->mutex);
				return -ENODEV;
			}
			mutex_unlock(&priv->mutex);

			p->vstatus.width = vstatus.frame_width;
			p->vstatus.height = vstatus.frame_height;
			p->vstatus.fps = vstatus.frame_rate;
			p->vstatus.error_count = vstatus.error_count;
			p->vstatus.status = vstatus.status;

			if (copy_to_user((void *)arg, p, sizeof(para)))
				r = -EFAULT;
			return r;
		}

	case AMSTREAM_IOC_VDECINFO:
		if ((this->type & PORT_TYPE_VIDEO) == 0)
			return -EINVAL;
		{
			struct vdec_info vinfo;
			struct am_io_info para;

			memset(&para, 0x0, sizeof(struct am_io_info));

			mutex_lock(&priv->mutex);
			if (vdec_status(priv->vdec, &vinfo) == -1) {
				mutex_unlock(&priv->mutex);
				return -ENODEV;
			}
			mutex_unlock(&priv->mutex);

			memcpy(&para.vinfo, &vinfo, sizeof(struct vdec_info));
			if (copy_to_user((void *)arg, &para, sizeof(para)))
				r = -EFAULT;
			return r;
		}

	case AMSTREAM_IOC_ADECSTAT:
		if ((this->type & PORT_TYPE_AUDIO) == 0)
			return -EINVAL;
		if (amstream_adec_status == NULL)
			return -ENODEV;
		else {
			struct adec_status astatus;
			struct am_io_param para;
			struct am_io_param *p = &para;

			amstream_adec_status(&astatus);
			p->astatus.channels = astatus.channels;
			p->astatus.sample_rate = astatus.sample_rate;
			p->astatus.resolution = astatus.resolution;
			p->astatus.error_count = astatus.error_count;
			p->astatus.status = astatus.status;
			if (copy_to_user((void *)arg, p, sizeof(para)))
				r = -EFAULT;
			return r;
		}
	case AMSTREAM_IOC_PORT_INIT:
		r = amstream_port_init(priv);
		break;

	case AMSTREAM_IOC_VDEC_RESET:
		if ((this->type & PORT_TYPE_VIDEO) == 0)
			return -EINVAL;

		if (priv->vdec == NULL)
			return -ENODEV;

		r = vdec_reset(priv->vdec);
		break;

	case AMSTREAM_IOC_TRICKMODE:
		if ((this->type & PORT_TYPE_VIDEO) == 0)
			return -EINVAL;
		r = vdec_set_trickmode(priv->vdec, arg);
		if (r == -1)
			return -ENODEV;
		break;

	case AMSTREAM_IOC_AUDIO_INFO:
		if ((this->type & PORT_TYPE_VIDEO)
			|| (this->type & PORT_TYPE_AUDIO)) {
			if (copy_from_user
				(&audio_dec_info, (void __user *)arg,
				 sizeof(audio_dec_info)))
				r = -EFAULT;
		} else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_AUDIO_RESET:
		if (this->type & PORT_TYPE_AUDIO) {
			struct stream_buf_s *pabuf = &bufs[BUF_TYPE_AUDIO];

			r = audio_port_reset(this, pabuf);
		} else
			r = -EINVAL;

		break;

	case AMSTREAM_IOC_SUB_RESET:
		if (this->type & PORT_TYPE_SUB) {
			struct stream_buf_s *psbuf = &bufs[BUF_TYPE_SUBTITLE];

			r = sub_port_reset(this, psbuf);
		} else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_SUB_LENGTH:
		if ((this->type & PORT_TYPE_SUB) ||
			(this->type & PORT_TYPE_SUB_RD)) {
			u32 sub_wp, sub_rp;
			struct stream_buf_s *psbuf = &bufs[BUF_TYPE_SUBTITLE];
			int val;

			sub_wp = stbuf_sub_wp_get();
			sub_rp = stbuf_sub_rp_get();

			if (sub_wp == sub_rp)
				val = 0;
			else if (sub_wp > sub_rp)
				val = sub_wp - sub_rp;
			else
				val = psbuf->buf_size - (sub_rp - sub_wp);
			put_user(val, (int __user *)arg);
		} else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_UD_LENGTH:
		if (this->type & PORT_TYPE_USERDATA) {
			/* *((u32 *)arg) = userdata_length; */
			put_user(userdata_length, (unsigned long __user *)arg);
			userdata_length = 0;
		} else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_UD_POC:
		if (this->type & PORT_TYPE_USERDATA) {
			/* *((u32 *)arg) = userdata_length; */
			int ri;
#ifdef DEBUG_USER_DATA
			int wi;
#endif
			int bDataAvail = 0;

			mutex_lock(&userdata_mutex);
			if (userdata_poc_wi != userdata_poc_ri) {
				bDataAvail = 1;
				ri = userdata_poc_ri;
#ifdef DEBUG_USER_DATA
				wi = userdata_poc_wi;
#endif
				userdata_poc_ri++;
				if (userdata_poc_ri >= USERDATA_FIFO_NUM)
					userdata_poc_ri = 0;
			}
			mutex_unlock(&userdata_mutex);
			if (bDataAvail) {
				int res;
				struct userdata_poc_info_t userdata_poc =
					userdata_poc_info[ri];
#ifdef DEBUG_USER_DATA
				pr_info("read poc: ri=%d, wi=%d, poc=%d, last_wi=%d\n",
					ri, wi,
					userdata_poc.poc_number,
					last_read_wi);
#endif
				res =
				copy_to_user((unsigned long __user *)arg,
					&userdata_poc,
					sizeof(struct userdata_poc_info_t));
				if (res < 0)
					r = -EFAULT;
			} else {
				r = -EFAULT;
			}
		} else {
			r = -EINVAL;
		}
		break;

	case AMSTREAM_IOC_UD_BUF_READ:
		{
			if (this->type & PORT_TYPE_USERDATA) {
				struct userdata_param_t  param;
				struct userdata_param_t  *p_userdata_param;
				struct vdec_s *vdec;

				p_userdata_param = &param;

				if (copy_from_user(p_userdata_param,
					(void __user *)arg,
					sizeof(struct userdata_param_t))) {
					r = -EFAULT;
					break;
				}

				vdec = vdec_get_vdec_by_id(p_userdata_param->instance_id);
				if (vdec) {
					if (vdec_read_user_data(vdec,
							p_userdata_param) == 0) {
						r = -EFAULT;
						break;
					}

					if (copy_to_user((void *)arg,
						p_userdata_param,
						sizeof(struct userdata_param_t)))
						r = -EFAULT;
				} else
					r = -EINVAL;
			}
		}
		break;

	case AMSTREAM_IOC_UD_AVAILABLE_VDEC:
		{
			unsigned int ready_vdec;

			mutex_lock(&userdata_mutex);
			ready_vdec = ud_ready_vdec_flag;
			ud_ready_vdec_flag = 0;
			mutex_unlock(&userdata_mutex);

			put_user(ready_vdec, (uint32_t __user *)arg);
		}
		break;

	case AMSTREAM_IOC_GET_VDEC_ID:
		if (this->type & PORT_TYPE_VIDEO && priv->vdec) {
			put_user(priv->vdec->id, (int32_t __user *)arg);
		} else
			r = -EINVAL;
		break;


	case AMSTREAM_IOC_UD_FLUSH_USERDATA:
		if (this->type & PORT_TYPE_USERDATA) {
			struct vdec_s *vdec;
			int vdec_id;

			get_user(vdec_id, (int __user *)arg);
			vdec = vdec_get_vdec_by_id(vdec_id);
			if (vdec) {
				vdec_reset_userdata_fifo(vdec, 0);
				pr_info("reset_userdata_fifo for vdec: %d\n", vdec_id);
			}
		} else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_SET_DEC_RESET:
		tsync_set_dec_reset();
		break;

	case AMSTREAM_IOC_TS_SKIPBYTE:
		if ((int)arg >= 0)
			tsdemux_set_skipbyte(arg);
		else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_SUB_TYPE:
		sub_type = (int)arg;
		break;

	case AMSTREAM_IOC_APTS_LOOKUP:
		if (this->type & PORT_TYPE_AUDIO) {
			u32 pts = 0, offset;

			get_user(offset, (unsigned long __user *)arg);
			pts_lookup_offset(PTS_TYPE_AUDIO, offset, &pts, 300);
			put_user(pts, (int __user *)arg);
		}
		return 0;
	case GET_FIRST_APTS_FLAG:
		if (this->type & PORT_TYPE_AUDIO) {
			put_user(first_pts_checkin_complete(PTS_TYPE_AUDIO),
					 (int __user *)arg);
		}
		break;

	case AMSTREAM_IOC_APTS:
		put_user(timestamp_apts_get(), (int __user *)arg);
		break;

	case AMSTREAM_IOC_VPTS:
		put_user(timestamp_vpts_get(), (int __user *)arg);
		break;

	case AMSTREAM_IOC_PCRSCR:
		put_user(timestamp_pcrscr_get(), (int __user *)arg);
		break;

	case AMSTREAM_IOC_SET_PCRSCR:
		timestamp_pcrscr_set(arg);
		break;
	case AMSTREAM_IOC_GET_LAST_CHECKIN_APTS:
		put_user(get_last_checkin_pts(PTS_TYPE_AUDIO), (int *)arg);
		break;
	case AMSTREAM_IOC_GET_LAST_CHECKIN_VPTS:
		put_user(get_last_checkin_pts(PTS_TYPE_VIDEO), (int *)arg);
		break;
	case AMSTREAM_IOC_GET_LAST_CHECKOUT_APTS:
		put_user(get_last_checkout_pts(PTS_TYPE_AUDIO), (int *)arg);
		break;
	case AMSTREAM_IOC_GET_LAST_CHECKOUT_VPTS:
		put_user(get_last_checkout_pts(PTS_TYPE_VIDEO), (int *)arg);
		break;
	case AMSTREAM_IOC_SUB_NUM:
		put_user(psparser_get_sub_found_num(), (int *)arg);
		break;

	case AMSTREAM_IOC_SUB_INFO:
		if (arg > 0) {
			struct subtitle_info msub_info[MAX_SUB_NUM];
			struct subtitle_info *psub_info[MAX_SUB_NUM];
			int i;

			for (i = 0; i < MAX_SUB_NUM; i++)
				psub_info[i] = &msub_info[i];

			r = psparser_get_sub_info(psub_info);

			if (r == 0) {
				if (copy_to_user((void __user *)arg, msub_info,
				sizeof(struct subtitle_info) * MAX_SUB_NUM))
					r = -EFAULT;
			}
		}
		break;
	case AMSTREAM_IOC_SET_DEMUX:
		tsdemux_set_demux((int)arg);
		break;
	case AMSTREAM_IOC_SET_VIDEO_DELAY_LIMIT_MS:
		if (has_hevc_vdec())
			bufs[BUF_TYPE_HEVC].max_buffer_delay_ms = (int)arg;
		bufs[BUF_TYPE_VIDEO].max_buffer_delay_ms = (int)arg;
		break;
	case AMSTREAM_IOC_SET_AUDIO_DELAY_LIMIT_MS:
		bufs[BUF_TYPE_AUDIO].max_buffer_delay_ms = (int)arg;
		break;
	case AMSTREAM_IOC_GET_VIDEO_DELAY_LIMIT_MS:
		put_user(bufs[BUF_TYPE_VIDEO].max_buffer_delay_ms, (int *)arg);
		break;
	case AMSTREAM_IOC_GET_AUDIO_DELAY_LIMIT_MS:
		put_user(bufs[BUF_TYPE_AUDIO].max_buffer_delay_ms, (int *)arg);
		break;
	case AMSTREAM_IOC_GET_VIDEO_CUR_DELAY_MS: {
		int delay;

		delay = calculation_stream_delayed_ms(
			PTS_TYPE_VIDEO, NULL, NULL);
		if (delay >= 0)
			put_user(delay, (int *)arg);
		else
			put_user(0, (int *)arg);
	}
	break;

	case AMSTREAM_IOC_GET_AUDIO_CUR_DELAY_MS: {
		int delay;

		delay = calculation_stream_delayed_ms(PTS_TYPE_AUDIO, NULL,
			NULL);
		if (delay >= 0)
			put_user(delay, (int *)arg);
		else
			put_user(0, (int *)arg);
	}
	break;
	case AMSTREAM_IOC_GET_AUDIO_AVG_BITRATE_BPS: {
		int delay;
		u32 avgbps;

		delay = calculation_stream_delayed_ms(PTS_TYPE_AUDIO, NULL,
				&avgbps);
		if (delay >= 0)
			put_user(avgbps, (int *)arg);
		else
			put_user(0, (int *)arg);
		break;
	}
	case AMSTREAM_IOC_GET_VIDEO_AVG_BITRATE_BPS: {
		int delay;
		u32 avgbps;

		delay = calculation_stream_delayed_ms(PTS_TYPE_VIDEO, NULL,
				&avgbps);
		if (delay >= 0)
			put_user(avgbps, (int *)arg);
		else
			put_user(0, (int *)arg);
		break;
	}
	case AMSTREAM_IOC_SET_DRMMODE:
		if ((u32) arg == 1) {
			pr_err("set drmmode\n");
			this->flag |= PORT_FLAG_DRM;
			if ((this->type & PORT_TYPE_VIDEO) &&
				(priv->vdec))
				priv->vdec->port_flag |= PORT_FLAG_DRM;
		} else {
			this->flag &= (~PORT_FLAG_DRM);
			pr_err("no drmmode\n");
		}
		break;
	case AMSTREAM_IOC_SET_APTS: {
		unsigned long pts;

		if (get_user(pts, (unsigned long __user *)arg)) {
			pr_err
			("Get audio pts from user space fault!\n");
			return -EFAULT;
		}
		if (tsync_get_mode() == TSYNC_MODE_PCRMASTER)
			tsync_pcr_set_apts(pts);
		else
			tsync_set_apts(pts);
		break;
	}
	default:
		r = -ENOIOCTLCMD;
		break;
	}

	return r;
}

static long amstream_do_ioctl(struct port_priv_s *priv,
	unsigned int cmd, ulong arg)
{
	long r = 0;

	switch (cmd) {
	case AMSTREAM_IOC_GET_VERSION:
	case AMSTREAM_IOC_GET:
	case AMSTREAM_IOC_SET:
	case AMSTREAM_IOC_GET_EX:
	case AMSTREAM_IOC_SET_EX:
	case AMSTREAM_IOC_GET_PTR:
	case AMSTREAM_IOC_SET_PTR:
	case AMSTREAM_IOC_SYSINFO:
		r = amstream_do_ioctl_new(priv, cmd, arg);
		break;
	default:
		r = amstream_do_ioctl_old(priv, cmd, arg);
		break;
	}
	if (r != 0)
		pr_err("amstream_do_ioctl error :%lx, %x\n", r, cmd);

	return r;
}
static long amstream_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	struct port_priv_s *priv = (struct port_priv_s *)file->private_data;
	struct stream_port_s *this = priv->port;

	if (!this)
		return -ENODEV;

	return amstream_do_ioctl(priv, cmd, arg);
}

#ifdef CONFIG_COMPAT
struct dec_sysinfo32 {

	u32 format;

	u32 width;

	u32 height;

	u32 rate;

	u32 extra;

	u32 status;

	u32 ratio;

	compat_uptr_t param;

	u64 ratio64;
};

struct am_ioctl_parm_ptr32 {
	union {
		compat_uptr_t pdata_audio_info;
		compat_uptr_t pdata_sub_info;
		compat_uptr_t pointer;
		char data[8];
	};
	u32 cmd;
	u32 len;
};

static long amstream_ioc_setget_ptr(struct port_priv_s *priv,
		unsigned int cmd, struct am_ioctl_parm_ptr32 __user *arg)
{
	struct am_ioctl_parm_ptr __user *data;
	struct am_ioctl_parm_ptr32 param;
	int ret;

	if (copy_from_user(&param,
		(void __user *)arg,
		sizeof(struct am_ioctl_parm_ptr32)))
		return -EFAULT;

	data = compat_alloc_user_space(sizeof(*data));
	if (!access_ok(VERIFY_WRITE, data, sizeof(*data)))
		return -EFAULT;

	if (put_user(param.cmd, &data->cmd) ||
		put_user(compat_ptr(param.pointer), &data->pointer) ||
		put_user(param.len, &data->len))
		return -EFAULT;

	ret = amstream_do_ioctl(priv, cmd, (unsigned long)data);
	if (ret < 0)
		return ret;
	return 0;

}

static long amstream_set_sysinfo(struct port_priv_s *priv,
		struct dec_sysinfo32 __user *arg)
{
	struct dec_sysinfo __user *data;
	struct dec_sysinfo32 __user *data32 = arg;
	int ret;
	struct dec_sysinfo32 param;

	if (copy_from_user(&param,
		(void __user *)arg,
		sizeof(struct dec_sysinfo32)))
		return -EFAULT;

	data = compat_alloc_user_space(sizeof(*data));
	if (!access_ok(VERIFY_WRITE, data, sizeof(*data)))
		return -EFAULT;
	if (copy_in_user(data, data32, 7 * sizeof(u32)))
		return -EFAULT;
	if (put_user(compat_ptr(param.param), &data->param))
		return -EFAULT;
	if (copy_in_user(&data->ratio64, &data32->ratio64,
					sizeof(data->ratio64)))
		return -EFAULT;

	ret = amstream_do_ioctl(priv, AMSTREAM_IOC_SYSINFO,
			(unsigned long)data);
	if (ret < 0)
		return ret;

	if (copy_in_user(&arg->format, &data->format, 7 * sizeof(u32)) ||
			copy_in_user(&arg->ratio64, &data->ratio64,
					sizeof(arg->ratio64)))
		return -EFAULT;

	return 0;
}


struct userdata_param32_t {
	uint32_t version;
	uint32_t instance_id; /*input, 0~9*/
	uint32_t buf_len; /*input*/
	uint32_t data_size; /*output*/
	compat_uptr_t pbuf_addr; /*input*/
	struct userdata_meta_info_t meta_info; /*output*/
};


static long amstream_ioc_get_userdata(struct port_priv_s *priv,
		struct userdata_param32_t __user *arg)
{
	struct userdata_param_t __user *data;
	struct userdata_param32_t __user *data32 = arg;
	int ret;
	struct userdata_param32_t param;


	if (copy_from_user(&param,
		(void __user *)arg,
		sizeof(struct userdata_param32_t)))
		return -EFAULT;

	data = compat_alloc_user_space(sizeof(*data));
	if (!access_ok(VERIFY_WRITE, data, sizeof(*data)))
		return -EFAULT;

	if (copy_in_user(data, data32, 4 * sizeof(u32)))
		return -EFAULT;

	if (copy_in_user(&data->meta_info, &data32->meta_info,
					sizeof(data->meta_info)))
		return -EFAULT;

	if (put_user(compat_ptr(param.pbuf_addr), &data->pbuf_addr))
		return -EFAULT;

	ret = amstream_do_ioctl(priv, AMSTREAM_IOC_UD_BUF_READ,
		(unsigned long)data);
	if (ret < 0)
		return ret;

	if (copy_in_user(&data32->version, &data->version, 4 * sizeof(u32)) ||
			copy_in_user(&data32->meta_info, &data->meta_info,
					sizeof(data32->meta_info)))
		return -EFAULT;

	return 0;
}


static long amstream_compat_ioctl(struct file *file,
		unsigned int cmd, ulong arg)
{
	s32 r = 0;
	struct port_priv_s *priv = (struct port_priv_s *)file->private_data;

	switch (cmd) {
	case AMSTREAM_IOC_GET_VERSION:
	case AMSTREAM_IOC_GET:
	case AMSTREAM_IOC_SET:
	case AMSTREAM_IOC_GET_EX:
	case AMSTREAM_IOC_SET_EX:
		return amstream_do_ioctl(priv, cmd, (ulong)compat_ptr(arg));
	case AMSTREAM_IOC_GET_PTR:
	case AMSTREAM_IOC_SET_PTR:
		return amstream_ioc_setget_ptr(priv, cmd, compat_ptr(arg));
	case AMSTREAM_IOC_SYSINFO:
		return amstream_set_sysinfo(priv, compat_ptr(arg));
	case AMSTREAM_IOC_UD_BUF_READ:
		return amstream_ioc_get_userdata(priv, compat_ptr(arg));
	default:
		return amstream_do_ioctl(priv, cmd, (ulong)compat_ptr(arg));
	}

	return r;
}
#endif

static ssize_t ports_show(struct class *class, struct class_attribute *attr,
						  char *buf)
{
	int i;
	char *pbuf = buf;
	struct stream_port_s *p = NULL;

	for (i = 0; i < amstream_port_num; i++) {
		p = &ports[i];
		/*name */
		pbuf += sprintf(pbuf, "%s\t:\n", p->name);
		/*type */
		pbuf += sprintf(pbuf, "\ttype:%d( ", p->type);
		if (p->type & PORT_TYPE_VIDEO)
			pbuf += sprintf(pbuf, "%s ", "Video");
		if (p->type & PORT_TYPE_AUDIO)
			pbuf += sprintf(pbuf, "%s ", "Audio");
		if (p->type & PORT_TYPE_MPTS)
			pbuf += sprintf(pbuf, "%s ", "TS");
		if (p->type & PORT_TYPE_MPPS)
			pbuf += sprintf(pbuf, "%s ", "PS");
		if (p->type & PORT_TYPE_ES)
			pbuf += sprintf(pbuf, "%s ", "ES");
		if (p->type & PORT_TYPE_RM)
			pbuf += sprintf(pbuf, "%s ", "RM");
		if (p->type & PORT_TYPE_SUB)
			pbuf += sprintf(pbuf, "%s ", "Subtitle");
		if (p->type & PORT_TYPE_SUB_RD)
			pbuf += sprintf(pbuf, "%s ", "Subtitle_Read");
		if (p->type & PORT_TYPE_USERDATA)
			pbuf += sprintf(pbuf, "%s ", "userdata");
		pbuf += sprintf(pbuf, ")\n");
		/*flag */
		pbuf += sprintf(pbuf, "\tflag:%d( ", p->flag);
		if (p->flag & PORT_FLAG_IN_USE)
			pbuf += sprintf(pbuf, "%s ", "Used");
		else
			pbuf += sprintf(pbuf, "%s ", "Unused");
		if ((p->type & PORT_TYPE_VIDEO) == 0) {
			if (p->flag & PORT_FLAG_INITED)
				pbuf += sprintf(pbuf, "%s ", "inited");
			else
				pbuf += sprintf(pbuf, "%s ", "uninited");
		}
		pbuf += sprintf(pbuf, ")\n");
		/*others */
		pbuf += sprintf(pbuf, "\tVformat:%d\n",
			(p->flag & PORT_FLAG_VFORMAT) ? p->vformat : -1);
		pbuf += sprintf(pbuf, "\tAformat:%d\n",
			(p->flag & PORT_FLAG_AFORMAT) ? p->aformat : -1);
		pbuf +=	sprintf(pbuf, "\tVid:%d\n",
			(p->flag & PORT_FLAG_VID) ? p->vid : -1);
		pbuf += sprintf(pbuf, "\tAid:%d\n",
			(p->flag & PORT_FLAG_AID) ? p->aid : -1);
		pbuf += sprintf(pbuf, "\tSid:%d\n",
			(p->flag & PORT_FLAG_SID) ? p->sid : -1);
		pbuf += sprintf(pbuf, "\tPCRid:%d\n",
			(p->pcr_inited == 1) ? p->pcrid : -1);
		pbuf += sprintf(pbuf, "\tachannel:%d\n", p->achanl);
		pbuf += sprintf(pbuf, "\tasamprate:%d\n", p->asamprate);
		pbuf += sprintf(pbuf, "\tadatawidth:%d\n\n", p->adatawidth);
	}
	return pbuf - buf;
}

static ssize_t bufs_show(struct class *class, struct class_attribute *attr,
						 char *buf)
{
	int i;
	char *pbuf = buf;
	struct stream_buf_s *p = NULL;
	char buf_type[][12] = { "Video", "Audio", "Subtitle",
				"UserData", "HEVC" };

	for (i = 0; i < amstream_buf_num; i++) {
		p = &bufs[i];
		/*type */
		pbuf += sprintf(pbuf, "%s buffer:", buf_type[p->type]);
		/*flag */
		pbuf += sprintf(pbuf, "\tflag:%d( ", p->flag);
		if (p->flag & BUF_FLAG_ALLOC)
			pbuf += sprintf(pbuf, "%s ", "Alloc");
		else
			pbuf += sprintf(pbuf, "%s ", "Unalloc");
		if (p->flag & BUF_FLAG_IN_USE)
			pbuf += sprintf(pbuf, "%s ", "Used");
		else
			pbuf += sprintf(pbuf, "%s ", "Noused");
		if (p->flag & BUF_FLAG_PARSER)
			pbuf += sprintf(pbuf, "%s ", "Parser");
		else
			pbuf += sprintf(pbuf, "%s ", "noParser");
		if (p->flag & BUF_FLAG_FIRST_TSTAMP)
			pbuf += sprintf(pbuf, "%s ", "firststamp");
		else
			pbuf += sprintf(pbuf, "%s ", "nofirststamp");
		pbuf += sprintf(pbuf, ")\n");
		/*buf stats */

		pbuf += sprintf(pbuf, "\tbuf addr:%p\n", (void *)p->buf_start);

		if (p->type != BUF_TYPE_SUBTITLE) {
			pbuf += sprintf(pbuf, "\tbuf size:%#x\n", p->buf_size);
			pbuf += sprintf(pbuf,
				"\tbuf canusesize:%#x\n",
				p->canusebuf_size);
			pbuf += sprintf(pbuf,
				"\tbuf regbase:%#lx\n", p->reg_base);

			if (p->reg_base && p->flag & BUF_FLAG_IN_USE) {
				if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M6) {
					/* TODO: mod gate */
					/* switch_mod_gate_by_name("vdec", 1);*/
					amports_switch_gate("vdec", 1);
				}
				pbuf += sprintf(pbuf, "\tbuf level:%#x\n",
						stbuf_level(p));
				pbuf += sprintf(pbuf, "\tbuf space:%#x\n",
						stbuf_space(p));
				pbuf += sprintf(pbuf,
						"\tbuf read pointer:%#x\n",
						stbuf_rp(p));
				if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_M6) {
					/* TODO: mod gate */
					/* switch_mod_gate_by_name("vdec", 0);*/
					amports_switch_gate("vdec", 0);
				}
			} else
				pbuf += sprintf(pbuf, "\tbuf no used.\n");

			if (p->type == BUF_TYPE_USERDATA) {
				pbuf += sprintf(pbuf,
					"\tbuf write pointer:%#x\n",
					p->buf_wp);
				pbuf += sprintf(pbuf,
					"\tbuf read pointer:%#x\n",
					p->buf_rp);
			}
		} else {
			u32 sub_wp, sub_rp, data_size;

			sub_wp = stbuf_sub_wp_get();
			sub_rp = stbuf_sub_rp_get();
			if (sub_wp >= sub_rp)
				data_size = sub_wp - sub_rp;
			else
				data_size = p->buf_size - sub_rp + sub_wp;
			pbuf += sprintf(pbuf, "\tbuf size:%#x\n", p->buf_size);
			pbuf +=
				sprintf(pbuf, "\tbuf canusesize:%#x\n",
						p->canusebuf_size);
			pbuf +=
				sprintf(pbuf, "\tbuf start:%#x\n",
						stbuf_sub_start_get());
			pbuf += sprintf(pbuf,
					"\tbuf write pointer:%#x\n", sub_wp);
			pbuf += sprintf(pbuf,
					"\tbuf read pointer:%#x\n", sub_rp);
			pbuf += sprintf(pbuf, "\tbuf level:%#x\n", data_size);
		}

		pbuf += sprintf(pbuf, "\tbuf first_stamp:%#x\n",
				p->first_tstamp);
		pbuf += sprintf(pbuf, "\tbuf wcnt:%#x\n\n", p->wcnt);
		pbuf += sprintf(pbuf, "\tbuf max_buffer_delay_ms:%dms\n",
				p->max_buffer_delay_ms);

		if (p->reg_base && p->flag & BUF_FLAG_IN_USE) {
			int calc_delayms = 0;
			u32 bitrate = 0, avg_bitrate = 0;

			calc_delayms = calculation_stream_delayed_ms(
				(p->type == BUF_TYPE_AUDIO) ? PTS_TYPE_AUDIO :
				PTS_TYPE_VIDEO,
				&bitrate,
				&avg_bitrate);

			if (calc_delayms >= 0) {
				pbuf += sprintf(pbuf,
					"\tbuf current delay:%dms\n",
					calc_delayms);
				pbuf += sprintf(pbuf,
				"\tbuf bitrate latest:%dbps,avg:%dbps\n",
				bitrate, avg_bitrate);
				pbuf += sprintf(pbuf,
				"\tbuf time after last pts:%d ms\n",
				calculation_stream_ext_delayed_ms
				((p->type == BUF_TYPE_AUDIO) ? PTS_TYPE_AUDIO :
				 PTS_TYPE_VIDEO));

				pbuf += sprintf(pbuf,
				"\tbuf time after last write data :%d ms\n",
				(int)(jiffies_64 -
				p->last_write_jiffies64) * 1000 / HZ);
			}
		}
		if (p->write_thread) {
			pbuf += sprintf(pbuf,
					"\twrite thread:%d/%d,fifo %d:%d,passed:%d\n",
						threadrw_buffer_level(p),
						threadrw_buffer_size(p),
						threadrw_datafifo_len(p),
						threadrw_freefifo_len(p),
						threadrw_passed_len(p)
					);
		}
	}

	return pbuf - buf;
}

static ssize_t videobufused_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	struct stream_buf_s *p = NULL;
	struct stream_buf_s *p_hevc = NULL;

	p = &bufs[0];
	if (has_hevc_vdec())
		p_hevc = &bufs[BUF_TYPE_HEVC];

	if (p->flag & BUF_FLAG_IN_USE)
		pbuf += sprintf(pbuf, "%d ", 1);
	else if (has_hevc_vdec() && (p_hevc->flag & BUF_FLAG_IN_USE))
		pbuf += sprintf(pbuf, "%d ", 1);
	else
		pbuf += sprintf(pbuf, "%d ", 0);
	return 1;
}

static ssize_t vcodec_profile_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	return vcodec_profile_read(buf);
}

static int reset_canuse_buferlevel(int levelx10000)
{
	int i;
	struct stream_buf_s *p = NULL;

	if (levelx10000 >= 0 && levelx10000 <= 10000)
		use_bufferlevelx10000 = levelx10000;
	else
		use_bufferlevelx10000 = 10000;
	for (i = 0; i < amstream_buf_num; i++) {
		p = &bufs[i];
		p->canusebuf_size = ((p->buf_size / 1024) *
			use_bufferlevelx10000 / 10000) * 1024;
		p->canusebuf_size += 1023;
		p->canusebuf_size &= ~1023;
		if (p->canusebuf_size > p->buf_size)
			p->canusebuf_size = p->buf_size;
	}
	return 0;
}

static ssize_t show_canuse_buferlevel(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t size = sprintf(buf,
		"use_bufferlevel=%d/10000[=(set range[ 0~10000])=\n",
			use_bufferlevelx10000);
	return size;
}

static ssize_t store_canuse_buferlevel(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t size)
{
	unsigned int val;
	ssize_t ret;

	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);

	if (ret != 0)
		return -EINVAL;
	val = val;
	reset_canuse_buferlevel(val);
	return size;
}

static ssize_t store_maxdelay(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val;
	ssize_t ret;
	int i;

	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	for (i = 0; i < amstream_buf_num; i++)
		bufs[i].max_buffer_delay_ms = val;
	return size;
}

static ssize_t show_maxdelay(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	ssize_t size = 0;

	size += sprintf(buf, "%dms video max buffered data delay ms\n",
		bufs[0].max_buffer_delay_ms);
	size += sprintf(buf, "%dms audio max buffered data delay ms\n",
		bufs[1].max_buffer_delay_ms);
	return size;
}

static struct class_attribute amstream_class_attrs[] = {
	__ATTR_RO(ports),
	__ATTR_RO(bufs),
	__ATTR_RO(vcodec_profile),
	__ATTR_RO(videobufused),
	__ATTR(canuse_buferlevel, S_IRUGO | S_IWUSR | S_IWGRP,
	show_canuse_buferlevel, store_canuse_buferlevel),
	__ATTR(max_buffer_delay_ms, S_IRUGO | S_IWUSR | S_IWGRP, show_maxdelay,
	store_maxdelay),
	__ATTR_NULL
};

static struct class amstream_class = {
		.name = "amstream",
		.class_attrs = amstream_class_attrs,
};

int amstream_request_firmware_from_sys(const char *file_name,
		char *buf, int size)
{
	const struct firmware *firmware;
	int err = 0;
	struct device *micro_dev;

	pr_info("try load %s  ...", file_name);
	micro_dev = device_create(&amstream_class,
			NULL, MKDEV(AMSTREAM_MAJOR, 100),
			NULL, "videodec");
	if (micro_dev == NULL) {
		pr_err("device_create failed =%d\n", err);
		return -1;
	}
	err = request_firmware(&firmware, file_name, micro_dev);
	if (err < 0) {
		pr_err("can't load the %s,err=%d\n", file_name, err);
		goto error1;
	}
	if (firmware->size > size) {
		pr_err("not enough memory size for audiodsp code\n");
		err = -ENOMEM;
		goto release;
	}

	memcpy(buf, (char *)firmware->data, firmware->size);
	/*mb(); don't need it*/
	pr_err("load mcode size=%zd\n mcode name %s\n", firmware->size,
		   file_name);
	err = firmware->size;
release:
	release_firmware(firmware);
error1:
	device_destroy(&amstream_class, MKDEV(AMSTREAM_MAJOR, 100));
	return err;
}

int videobufused_show_fun(const char *trigger, int id, char *sbuf, int size)
{
	int ret = -1;
	void *buf, *getbuf = NULL;
	if (size < PAGE_SIZE) {
		getbuf = (void *)__get_free_page(GFP_KERNEL);
		if (!getbuf)
			return -ENOMEM;
		buf = getbuf;
	} else {
		buf = sbuf;
	}

	switch (id) {
	case 0:
		ret = videobufused_show(NULL, NULL , buf);
		break;
	default:
		ret = -1;
	}
	if (ret > 0 && getbuf != NULL) {
		ret = min_t(int, ret, size);
		strncpy(sbuf, buf, ret);
	}
	if (getbuf != NULL)
		free_page((unsigned long)getbuf);
	return ret;
}

static struct mconfig amports_configs[] = {
	MC_PI32("def_4k_vstreambuf_sizeM", &def_4k_vstreambuf_sizeM),
	MC_PI32("def_vstreambuf_sizeM", &def_vstreambuf_sizeM),
	MC_PI32("slow_input", &slow_input),
	MC_FUN_ID("videobufused", videobufused_show_fun, NULL, 0),
};



/*static struct resource memobj;*/
static int amstream_probe(struct platform_device *pdev)
{
	int i;
	int r;
	struct stream_port_s *st;

	pr_err("Amlogic A/V streaming port init\n");

	amstream_port_num = MAX_AMSTREAM_PORT_NUM;
	amstream_buf_num = BUF_MAX_NUM;
/*
 *	r = of_reserved_mem_device_init(&pdev->dev);
 *	if (r == 0)
 *		pr_info("of probe done");
 *	else {
 *		r = -ENOMEM;
 *		return r;
 *	}
 */
	r = class_register(&amstream_class);
	if (r) {
		pr_err("amstream class create fail.\n");
		return r;
	}

	r = astream_dev_register();
	if (r)
		return r;

	r = register_chrdev(AMSTREAM_MAJOR, "amstream", &amstream_fops);
	if (r < 0) {
		pr_err("Can't allocate major for amstreaming device\n");

		goto error2;
	}

	amstream_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

	for (st = &ports[0], i = 0; i < amstream_port_num; i++, st++) {
		st->class_dev = device_create(amstream_dev_class, NULL,
				MKDEV(AMSTREAM_MAJOR, i), NULL,
				ports[i].name);
	}

	amstream_adec_status = NULL;
	if (tsdemux_class_register() != 0) {
		r = (-EIO);
		goto error3;
	}
	tsdemux_tsync_func_init();
	init_waitqueue_head(&amstream_sub_wait);
	init_waitqueue_head(&amstream_userdata_wait);
	reset_canuse_buferlevel(10000);
	amstream_pdev = pdev;
	amports_clock_gate_init(&amstream_pdev->dev);

	/*prealloc fetch buf to avoid no continue buffer later...*/
	stbuf_fetch_init();
	REG_PATH_CONFIGS("media.amports", amports_configs);

	/* poweroff the decode core because dos can not be reset when reboot */
	if (get_cpu_major_id() == AM_MESON_CPU_MAJOR_ID_G12A)
		vdec_power_reset();

	return 0;

	/*
	 *   error4:
	 *  tsdemux_class_unregister();
	 */
error3:
	for (st = &ports[0], i = 0; i < amstream_port_num; i++, st++)
		device_destroy(amstream_dev_class, MKDEV(AMSTREAM_MAJOR, i));
	class_destroy(amstream_dev_class);
error2:
	unregister_chrdev(AMSTREAM_MAJOR, "amstream");
	/* error1: */
	astream_dev_unregister();
	return r;
}

static int amstream_remove(struct platform_device *pdev)
{
	int i;
	struct stream_port_s *st;

	if (bufs[BUF_TYPE_VIDEO].flag & BUF_FLAG_ALLOC)
		stbuf_change_size(&bufs[BUF_TYPE_VIDEO], 0, false);
	if (bufs[BUF_TYPE_AUDIO].flag & BUF_FLAG_ALLOC)
		stbuf_change_size(&bufs[BUF_TYPE_AUDIO], 0, false);
	stbuf_fetch_release();
	tsdemux_class_unregister();
	for (st = &ports[0], i = 0; i < amstream_port_num; i++, st++)
		device_destroy(amstream_dev_class, MKDEV(AMSTREAM_MAJOR, i));

	class_destroy(amstream_dev_class);

	unregister_chrdev(AMSTREAM_MAJOR, "amstream");

	class_unregister(&amstream_class);

	astream_dev_unregister();

	amstream_adec_status = NULL;

	pr_err("Amlogic A/V streaming port release\n");

	return 0;
}

void set_adec_func(int (*adec_func)(struct adec_status *))
{
	amstream_adec_status = adec_func;
}

void wakeup_sub_poll(void)
{
	atomic_inc(&subdata_ready);
	wake_up_interruptible(&amstream_sub_wait);
}

int get_sub_type(void)
{
	return sub_type;
}

u32 get_audio_reset(void)
{
	return amstream_audio_reset;
}

/*get pes buffers */

struct stream_buf_s *get_stream_buffer(int id)
{
	if (id >= BUF_MAX_NUM)
		return 0;
	return &bufs[id];
}
EXPORT_SYMBOL(get_stream_buffer);
static const struct of_device_id amlogic_mesonstream_dt_match[] = {
	{
		.compatible = "amlogic, codec, streambuf",
	},
	{},
};

static struct platform_driver amstream_driver = {
	.probe = amstream_probe,
	.remove = amstream_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "mesonstream",
		.of_match_table = amlogic_mesonstream_dt_match,
	}
};

static int __init amstream_module_init(void)
{
	if (platform_driver_register(&amstream_driver)) {
		pr_err("failed to register amstream module\n");
		return -ENODEV;
	}

	if (subtitle_init()) {
		pr_err("failed to init subtitle\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit amstream_module_exit(void)
{
	platform_driver_unregister(&amstream_driver);
	subtitle_exit();
}

module_init(amstream_module_init);
module_exit(amstream_module_exit);

module_param(def_4k_vstreambuf_sizeM, uint, 0664);
MODULE_PARM_DESC(def_4k_vstreambuf_sizeM,
	"\nDefault video Stream buf size for 4K MByptes\n");

module_param(def_vstreambuf_sizeM, uint, 0664);
MODULE_PARM_DESC(def_vstreambuf_sizeM,
	"\nDefault video Stream buf size for < 1080p MByptes\n");

module_param(slow_input, uint, 0664);
MODULE_PARM_DESC(slow_input, "\n amstream slow_input\n");


MODULE_DESCRIPTION("AMLOGIC streaming port driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
