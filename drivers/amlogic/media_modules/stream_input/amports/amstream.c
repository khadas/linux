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
#include <linux/compat.h>
#include <uapi/linux/major.h>
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
#include "../amports/streambuf.h"
#include "../amports/streambuf_reg.h"
#include "../parser/tsdemux.h"
#include "../parser/psparser.h"
#include "../parser/esparser.h"
#include "../../frame_provider/decoder/utils/vdec.h"
#include "adec.h"
#include "../parser/rmparser.h"
#include "amports_priv.h"
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/amlogic/media/frame_sync/tsync_pcr.h>
#include "../amports/thread_rw.h"
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
#include "stream_buffer_base.h"
#include "../../frame_provider/decoder/utils/vdec_feature.h"

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
static struct userdata_poc_info_t *userdata_poc_info;
static int userdata_poc_ri, userdata_poc_wi;
static int last_read_wi;

/*bit 1 force dual layer
 *bit 2 force frame mode
 */
static u32 force_dv_mode;

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
	{
		.name = "amstream_dves_avc_frame",
		.type = PORT_TYPE_ES | PORT_TYPE_VIDEO | PORT_TYPE_FRAME |
			PORT_TYPE_DECODER_SCHED | PORT_TYPE_DUALDEC,
		.fops = &vframe_fops,
	},
	{
		.name = "amstream_dves_hevc_frame",
		.type = PORT_TYPE_ES | PORT_TYPE_VIDEO | PORT_TYPE_HEVC | PORT_TYPE_FRAME |
			PORT_TYPE_DECODER_SCHED | PORT_TYPE_DUALDEC,
		.fops = &vframe_fops,
		.vformat = VFORMAT_HEVC,
	},
	{
		.name = "amstream_dves_av1",
		.type = PORT_TYPE_ES | PORT_TYPE_VIDEO | PORT_TYPE_HEVC | PORT_TYPE_FRAME |
			PORT_TYPE_DECODER_SCHED | PORT_TYPE_DUALDEC,
		.fops = &vframe_fops,
		.vformat = VFORMAT_AV1,
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
	if (pvbuf->buf_start != 0 || pvbuf->ext_buf_addr != 0) {
		pr_info("streambuf is alloced, buf_start 0x%lx, extbuf 0x%lx\n",
			pvbuf->buf_start, pvbuf->ext_buf_addr);
		return;
	}
	if (priv->is_4k) {
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

		return vdec ? vdec->port_flag & PORT_FLAG_INITED : 0;
	}

	return port->flag & PORT_FLAG_INITED;
}

static void port_set_inited(struct port_priv_s *priv)
{
	struct stream_port_s *port = priv->port;

	if (port->type & PORT_TYPE_VIDEO) {
		struct vdec_s *vdec = priv->vdec;

		vdec->port_flag |= PORT_FLAG_INITED;
		port->flag |= PORT_FLAG_INITED;
		pr_info("vdec->port_flag=0x%x, port_flag=0x%x\n",
			vdec->port_flag, port->flag);
	} else
		port->flag |= PORT_FLAG_INITED;
}

static void video_port_release(struct port_priv_s *priv,
	  struct stream_buf_s *pbuf, int release_num)
{
	struct vdec_s *vdec = priv->vdec;
	struct vdec_s *slave = NULL;

	if (!vdec)
		return;

	switch (release_num) {
	default:
	/*fallthrough*/
	case 0:		/*release all */
	case 3:
		if (vdec->slave)
			slave = vdec->slave;
		vdec_release(vdec);
		if (slave)
			vdec_release(slave);
		priv->vdec = NULL;
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
	if (vdec_dual(vdec) && vdec_secure(vdec) && (vdec->slave)) {
		/*copy drm flags for slave dec.*/
		vdec->slave->port_flag |= PORT_FLAG_DRM;
	}
	if (port->vformat == VFORMAT_H264_4K2K ||
		(priv->vdec->sys_info->height *
			priv->vdec->sys_info->width) > 1920*1088) {
		priv->is_4k = true;
		if (get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_TXLX
				&& port->vformat == VFORMAT_H264) {
			vdec_poweron(VDEC_HEVC);
		}
	} else {
		priv->is_4k = false;
	}

	if (port->type & PORT_TYPE_FRAME) {
		r = vdec_init(vdec,
			(priv->vdec->sys_info->height *
			priv->vdec->sys_info->width) > 1920*1088, false);
		if (r < 0) {
			pr_err("video_port_init %d, vdec_init failed\n",
				__LINE__);
			return r;
		}
#if 0
		if (vdec_dual(vdec)) {
			if (port->vformat == VFORMAT_AV1)	/* av1 dv only single layer */
				return 0;
			r = vdec_init(vdec->slave,
				(priv->vdec->sys_info->height *
				priv->vdec->sys_info->width) > 1920*1088);
			if (r < 0) {
				vdec_release(vdec);
				pr_err("video_port_init %d, vdec_init failed\n",
					__LINE__);
				return r;
			}
		}
#endif
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

	/* todo: set path based on port flag */
	r = vdec_init(vdec,
		(priv->vdec->sys_info->height *
		 priv->vdec->sys_info->width) > 1920*1088, false);

	if (r < 0) {
		pr_err("video_port_init %d, vdec_init failed\n", __LINE__);
		goto err;
	}

	if (vdec_dual(vdec)) {
		r = vdec_init(vdec->slave,
			(priv->vdec->sys_info->height *
			priv->vdec->sys_info->width) > 1920*1088, false);
		if (r < 0) {
			pr_err("video_port_init %d, vdec_init failed\n", __LINE__);
			goto err;
		}
	}

	return 0;
err:
	if (vdec->slave)
		vdec_release(vdec->slave);
	if (vdec)
		vdec_release(vdec);
	priv->vdec = NULL;

	return r;
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
		stbuf_release(pbuf);
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

	pr_info("audio port reset, flag:0x%x\n", port->flag);
	if ((port->flag & PORT_FLAG_INITED) == 0) {
		pr_info("audio port not inited,return\n");
		return 0;
	}

	pr_info("audio_port_reset begin\n");
	pts_stop(PTS_TYPE_AUDIO);

	stbuf_release(pbuf);

	r = stbuf_init(pbuf, NULL);
	if (r < 0) {
		return r;
	}

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

	//clear audio break flag after reset
	//tsync_audio_break(0);

	pr_info("audio_port_reset done\n");
	return r;
}

static int sub_port_reset(struct stream_port_s *port,
				struct stream_buf_s *pbuf)
{
	int r;

	port->flag &= (~PORT_FLAG_INITED);

	stbuf_release(pbuf);

	r = stbuf_init(pbuf, NULL);
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

	r = stbuf_init(pbuf, NULL);
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
	stbuf_release(pbuf);
	sub_port_inited = 0;
}

static int sub_port_init(struct stream_port_s *port, struct stream_buf_s *pbuf)
{
	int r;
	r = stbuf_init(pbuf, NULL);
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

#if 1
/*DDD*/
struct stream_buf_s *get_vbuf(void)
{
	return &bufs[BUF_TYPE_VIDEO];
}

EXPORT_SYMBOL(get_vbuf);
#endif

static int amstream_port_init(struct port_priv_s *priv)
{
	int r = 0;
	struct stream_buf_s *pvbuf = &bufs[BUF_TYPE_VIDEO];
	struct stream_buf_s *pabuf = &bufs[BUF_TYPE_AUDIO];
	struct stream_buf_s *psbuf = &bufs[BUF_TYPE_SUBTITLE];
	struct stream_port_s *port = priv->port;
	struct vdec_s *vdec = priv->vdec;

	r = vdec_resource_checking(vdec);
	if (r < 0)
		return r;

	mutex_lock(&amstream_mutex);

	if ((get_cpu_major_id() >= AM_MESON_CPU_MAJOR_ID_G12A) &&
		(get_cpu_major_id() < AM_MESON_CPU_MAJOR_ID_SC2)) {
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
		if (vdec_stream_based(vdec)) {
			struct stream_buf_ops *ops = NULL;
			struct parser_args pars	= {
				.vid = (port->flag & PORT_FLAG_VID) ? port->vid : 0xffff,
				.aid = (port->flag & PORT_FLAG_AID) ? port->aid : 0xffff,
				.sid = (port->flag & PORT_FLAG_SID) ? port->sid : 0xffff,
				.pcrid = (port->pcr_inited == 1) ? port->pcrid : 0xffff,
			};

			if (port->type & PORT_TYPE_MPTS) {
				ops = get_tsparser_stbuf_ops();
			} else if (port->type & PORT_TYPE_MPPS) {
				ops = get_psparser_stbuf_ops();
			} else {
				ops = !vdec_single(vdec) ?
					get_stbuf_ops() :
					get_esparser_stbuf_ops();

				/* def used stbuf with parser if the feature disable. */
				if (!is_support_no_parser())
					ops = get_esparser_stbuf_ops();
				else if (vdec->format == VFORMAT_H264MVC ||
					vdec->format == VFORMAT_VC1)
					ops = get_stbuf_ops();
			}

			r = stream_buffer_base_init(&vdec->vbuf, ops, &pars);
			if (r) {
				mutex_unlock(&priv->mutex);
				pr_err("stream buffer base init failed\n");
				goto error2;
			}
		}

		mutex_lock(&priv->mutex);
		r = video_port_init(priv, &vdec->vbuf);
		if (r < 0) {
			mutex_unlock(&priv->mutex);
			pr_err("video_port_init failed\n");
			goto error2;
		}
		mutex_unlock(&priv->mutex);
	}

	if ((port->type & PORT_TYPE_MPTS) &&
		!(port->flag & PORT_FLAG_VFORMAT)) {
		r = tsdemux_init(0xffff,
			(port->flag & PORT_FLAG_AID) ? port->aid : 0xffff,
			(port->flag & PORT_FLAG_SID) ? port->sid : 0xffff,
			(port->pcr_inited == 1) ? port->pcrid : 0xffff,
			0, vdec);
		if (r < 0) {
			pr_err("tsdemux_init  failed\n");
			goto error3;
		}
		tsync_pcr_start();
	}

	if ((port->type & PORT_TYPE_SUB) && (port->flag & PORT_FLAG_SID)) {
		r = sub_port_init(port, psbuf);
		if (r < 0) {
			pr_err("sub_port_init  failed\n");
			goto error4;
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

error4:
	if ((port->type & PORT_TYPE_MPTS) &&
		!(port->flag & PORT_FLAG_VFORMAT))
		tsdemux_release();
error3:
	if ((port->type & PORT_TYPE_VIDEO) &&
		(port->flag & PORT_FLAG_VFORMAT))
		video_port_release(priv, &priv->vdec->vbuf, 0);
error2:
	if ((port->type & PORT_TYPE_AUDIO) &&
		(port->flag & PORT_FLAG_AFORMAT))
		audio_port_release(port, pabuf, 0);
error1:
	mutex_unlock(&amstream_mutex);
	return r;
}

static int amstream_port_release(struct port_priv_s *priv)
{
	struct stream_port_s *port = priv->port;
	struct stream_buf_s *pvbuf = &priv->vdec->vbuf;
	struct stream_buf_s *pabuf = &bufs[BUF_TYPE_AUDIO];
	struct stream_buf_s *psbuf = &bufs[BUF_TYPE_SUBTITLE];

	if ((port->type & PORT_TYPE_MPTS) &&
		!(port->flag & PORT_FLAG_VFORMAT)) {
		tsync_pcr_stop();
		tsdemux_release();
	}

	if ((port->type & PORT_TYPE_MPPS) &&
		!(port->flag & PORT_FLAG_VFORMAT)) {
		psparser_release();
	}

	if (port->type & PORT_TYPE_VIDEO) {
		video_port_release(priv, pvbuf, 0);
	}

	if (port->type & PORT_TYPE_AUDIO)
		audio_port_release(port, pabuf, 0);

	if (port->type & PORT_TYPE_SUB)
		sub_port_release(port, psbuf);

	port->pcr_inited = 0;

	if (!is_mult_inc(port->type) ||
		(is_mult_inc(port->type) &&
		!is_support_no_parser()))
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
	struct stream_buf_s *pbuf = &priv->vdec->vbuf;
	int r;

	if (!(port_get_inited(priv))) {
		r = amstream_port_init(priv);
		if (r < 0)
			return r;
	}

	if (priv->vdec->port_flag & PORT_FLAG_DRM)
		r = drm_write(file, pbuf, buf, count);
	else
		r = stream_buffer_write(file, pbuf, buf, count);
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
	struct stream_buf_s *pvbuf = &priv->vdec->vbuf;
	int r = 0;

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
	/*flush sub buf before read*/
	codec_mm_dma_flush(
			(void*)codec_mm_phys_to_virt(sub_start),
			stbuf_size(s_buf),
			DMA_FROM_DEVICE);
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
	int i;
	st_userdata *userdata = get_vdec_userdata_ctx();

	if (vdec == NULL) {
		pr_info("Error, invalid vdec instance!\n");
		return;
	}

	mutex_lock(&userdata->mutex);

	for (i = 0; i < MAX_USERDATA_CHANNEL_NUM; i++) {
		if (userdata->set_id_flag && (userdata->id[i] == vdec->video_id)) {
			userdata->ready_flag[i] = 1;
			if (vdec_get_debug_flags() & 0x10000000)
				pr_info("%s, wakeup! id = %d\n", __func__, vdec->video_id);
			break;
		} else if (!userdata->set_id_flag) {
			if (!userdata->used[0]) {
				vdec->video_id = vdec->id;
				userdata->id[0] = vdec->id;
				userdata->used[0] = 1;
			}

			if (vdec_get_debug_flags() & 0x10000000)
				pr_info("%s[%d] userdata instance %d ready!\n",
				__func__, i, userdata->id[i]);

			userdata->ready_flag[i] = 1;
			break;
		}
	}

	mutex_unlock(&userdata->mutex);

	wake_up_interruptible(&userdata->userdata_wait);
}
EXPORT_SYMBOL(amstream_wakeup_userdata_poll);

static unsigned int amstream_userdata_poll(struct file *file,
		poll_table *wait_table)
{
	int fd_match = 0;
	int i;
	st_userdata *userdata = get_vdec_userdata_ctx();

	poll_wait(file, &userdata->userdata_wait, wait_table);
	mutex_lock(&userdata->mutex);
	for (i = 0; i < MAX_USERDATA_CHANNEL_NUM; i++) {
		if (userdata->id[i] == userdata->video_id && userdata->ready_flag[i] == 1) {
			fd_match = 1;
			if (vdec_get_debug_flags() & 0x10000000)
				pr_info("%s, success! id = %d\n", __func__, userdata->video_id);
			break;
		}
	}

	if (fd_match) {
		mutex_unlock(&userdata->mutex);
		return POLLIN | POLLRDNORM;
	}

	mutex_unlock(&userdata->mutex);
	return 0;
}

static void amstream_userdata_init(void)
{
	int i;
	st_userdata *userdata = get_vdec_userdata_ctx();

	init_waitqueue_head(&userdata->userdata_wait);
	mutex_init(&userdata->mutex);
	userdata->set_id_flag = 0;

	for (i = 0; i < MAX_USERDATA_CHANNEL_NUM; i++) {
		userdata->ready_flag[i] = 0;
		userdata->id[i] = -1;
		userdata->used[i] = 0;
	}

	return;
}

static int amstream_open(struct inode *inode, struct file *file)
{
	s32 i;
	struct stream_port_s *s;
	struct stream_port_s *port = &ports[iminor(inode)];
	struct port_priv_s *priv;

	VDEC_PRINT_FUN_LINENO(__func__, __LINE__);
	if (vdec_get_debug_flags() & 0x10000000)
		pr_info("%s, port type %d\n", __func__, port->type);
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

	if (!is_support_no_parser()) {
		if ((port->flag & PORT_FLAG_IN_USE) &&
			((port->type & PORT_TYPE_FRAME) == 0)) {
			mutex_unlock(&amstream_mutex);
			return -EBUSY;
		}
	}
	/* force dv frame mode */
	if (force_dv_mode & 0x2) {
		port->type |= PORT_TYPE_FRAME;
		port->fops = &vframe_fops;
		pr_debug("%s, dobly vision force frame mode.\n", __func__);
	}

	/* esplayer stream mode force dv */
	if (force_dv_mode & 0x1)
		port->type |= PORT_TYPE_DUALDEC;

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
		if (!(port->type & PORT_TYPE_FRAME)) {
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
	if (vdec_get_debug_flags() & 0x10000000)
		pr_info("%s, port type %d\n", __func__, port->type);

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

	if (!is_mult_inc(port->type) ||
		(is_mult_inc(port->type) &&
		!is_support_no_parser()))
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
				&& priv->is_4k) {
				vdec_poweroff(VDEC_HEVC);
			}

			if ((port->vformat == VFORMAT_HEVC
					|| port->vformat == VFORMAT_AVS2
					|| port->vformat == VFORMAT_AV1
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
			u32 pts = 0, frame_size, offset;

			offset = parm.data_32;
			pts_lookup_offset(PTS_TYPE_AUDIO, offset, &pts,
				&frame_size, 300);
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
	case AMSTREAM_GET_VPTS_U64:
		parm.data_64 = timestamp_vpts_get_u64();
		break;
	case AMSTREAM_GET_APTS_U64:
		parm.data_64 = timestamp_apts_get_u64();
		break;
	case AMSTREAM_GET_PCRSCR:
		//parm.data_32 = timestamp_pcrscr_get();
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
	int i;
	st_userdata *userdata = get_vdec_userdata_ctx();

	if (copy_from_user
		((void *)&parm, (void *)arg,
		 sizeof(parm)))
		r = -EFAULT;

	switch (parm.cmd) {
	case AMSTREAM_SET_VB_START:
		if ((this->type & PORT_TYPE_VIDEO) &&
			((priv->vdec->vbuf.flag & BUF_FLAG_IN_USE) == 0)) {
			priv->vdec->vbuf.buf_start = parm.data_32;
		} else
			r = -EINVAL;
		break;
	case AMSTREAM_SET_VB_SIZE:
		if ((this->type & PORT_TYPE_VIDEO) &&
			((priv->vdec->vbuf.flag & BUF_FLAG_IN_USE) == 0)) {
			if (priv->vdec->vbuf.flag & BUF_FLAG_ALLOC) {
				r += stbuf_change_size(
						&priv->vdec->vbuf,
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
				//tsync_audio_break(1);
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
		else if ((this->type & PORT_TYPE_VIDEO) ||
			(this->type & PORT_TYPE_HEVC)) {
			struct stream_buf_s *vbuf = &priv->vdec->vbuf;
			if (vbuf->no_parser) {
				pts_checkin_offset(PTS_TYPE_VIDEO,
					vbuf->stream_offset, parm.data_32);
			} else {
				r = es_vpts_checkin(vbuf, parm.data_32);
			}
		} else if (this->type & PORT_TYPE_AUDIO)
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
			} else if ((this->type & PORT_TYPE_HEVC) ||
					(this->type & PORT_TYPE_VIDEO)) {
					r = es_vpts_checkin_us64(
					&priv->vdec->vbuf, pts);
			} else if (this->type & PORT_TYPE_AUDIO) {
					r = es_vpts_checkin_us64(
					&bufs[BUF_TYPE_AUDIO], pts);
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

			mutex_lock(&amstream_mutex);
			r = audio_port_reset(this, pabuf);
			mutex_unlock(&amstream_mutex);
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
		priv->vdec->vbuf.max_buffer_delay_ms = parm.data_32;
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
	case AMSTREAM_SET_VIDEO_ID:
		priv->vdec->video_id = parm.data_32;
		priv->vdec->afd_video_id = parm.data_32;
		mutex_lock(&userdata->mutex);
		for (i = 0;i < MAX_USERDATA_CHANNEL_NUM; i++) {
			if (userdata->used[i] == 0) {
				userdata->id[i] = priv->vdec->video_id;
				userdata->used[i] = 1;
				userdata->video_id = priv->vdec->video_id;
				userdata->set_id_flag = 1;
				break;
			}
		}
		mutex_unlock(&userdata->mutex);

		pr_info("AMSTREAM_SET_VIDEO_ID vdec %p video_id: %d\n", priv->vdec, parm.data_32);
		break;
	default:
		r = -ENOIOCTLCMD;
		break;
	}
	return r;
}

static enum E_ASPECT_RATIO  get_normalized_aspect_ratio(u32 ratio_control)
{
	enum E_ASPECT_RATIO euAspectRatio;

	ratio_control = ratio_control >> DISP_RATIO_ASPECT_RATIO_BIT;

	switch (ratio_control) {
	case 0x8c:
	case 0x90:
		euAspectRatio = ASPECT_RATIO_16_9;
		/*pr_info("ASPECT_RATIO_16_9\n");*/
		break;
	case 0xbb:
	case 0xc0:
		euAspectRatio = ASPECT_RATIO_4_3;
		/*pr_info("ASPECT_RATIO_4_3\n");*/
		break;
	default:
		euAspectRatio = ASPECT_UNDEFINED;
		/*pr_info("ASPECT_UNDEFINED and ratio_control = 0x%x\n",
			ratio_control);*/
		break;
	}

	return euAspectRatio;
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

			mutex_lock(&amstream_mutex);

			/*
			 *todo: check upper layer for decoder
			 * handler lifecycle
			 */
			if (priv->vdec == NULL) {
				r = -EINVAL;
				mutex_unlock(&amstream_mutex);
				break;
			}

			if (this->type & PORT_TYPE_FRAME) {
				struct vdec_input_status_s status;

				r = vdec_input_get_status(&priv->vdec->input,
							&status);
				if (r == 0) {
					p->status.size = status.size;
					p->status.data_len = status.data_len;
					p->status.free_len = status.free_len;
					p->status.read_pointer =
							status.read_pointer;
				}
				mutex_unlock(&amstream_mutex);
				break;
			}

			buf = &priv->vdec->vbuf;
			p->status.size = stbuf_canusesize(buf);
			p->status.data_len = stbuf_level(buf);
			p->status.free_len = stbuf_space(buf);
			p->status.read_pointer = stbuf_rp(buf);
			mutex_unlock(&amstream_mutex);
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
			p->vstatus.euAspectRatio =
				get_normalized_aspect_ratio(
					vstatus.ratio_control);

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
	case AMSTREAM_GET_EX_WR_COUNT:
	{
		struct am_ioctl_parm_ex *p = &parm;
		struct vdec_s *vdec = priv->vdec;

		mutex_lock(&amstream_mutex);
		if (!vdec)
			vdec = vdec_get_vdec_by_id(0); //Use id 0 as default
		if (vdec && vdec->mvfrm)
			p->wr_count = vdec->mvfrm->wr;
		else
			p->wr_count = 0;
		mutex_unlock(&amstream_mutex);
		r = 0;
	}
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
	case AMSTREAM_SET_PTR_HDR10P_DATA:
		if ((this->type & PORT_TYPE_VIDEO) && (this->type & PORT_TYPE_FRAME)) {
			if (!parm.pointer || (parm.len <= 0) ||
				(parm.len > PAGE_SIZE)) {
				r = -EINVAL;
			} else {
				r = copy_from_user(priv->vdec->hdr10p_data_buf,
						parm.pointer, parm.len);
				if (r) {
					priv->vdec->hdr10p_data_size = 0;
					priv->vdec->hdr10p_data_valid = false;
					r = -EINVAL;
				} else {
					priv->vdec->hdr10p_data_size = parm.len;
					priv->vdec->hdr10p_data_valid = true;
				}
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
	case AMSTREAM_IOC_GET_QOSINFO:
	case AMSTREAM_IOC_GET_MVDECINFO:
		{
			u32 slots = 0;
			u32 struct_size = 0;
			int vdec_id = 0;
			struct vdec_s *vdec = priv->vdec;
			struct vframe_counter_s *tmpbuf = kmalloc(QOS_FRAME_NUM *
						sizeof(struct vframe_counter_s),GFP_KERNEL);
			struct av_param_mvdec_t  __user *uarg = (void *)arg;

			mutex_lock(&amstream_mutex);
			if (!tmpbuf) {
				r = -EFAULT;
				pr_err("kmalloc vframe_counter_s failed!\n");
				mutex_unlock(&amstream_mutex);
				break;
			}

			if (get_user(vdec_id, &uarg->vdec_id) < 0 ||
				get_user(struct_size, &uarg->struct_size) < 0) {
				r = -EFAULT;
				kfree(tmpbuf);
				mutex_unlock(&amstream_mutex);
				break;
			}

			if (vdec && !vdec_id) //If vdec_id is > 0, it means user require to use it.
				r = 0;//vdec =priv->vdec;//Nothing to do.
			else
				vdec = vdec_get_vdec_by_id(vdec_id);
			if (!vdec) {
				r = 0;
				kfree(tmpbuf);
				mutex_unlock(&amstream_mutex);
				break;
			}

			slots = vdec_get_frame_vdec(vdec, tmpbuf);
			if (AMSTREAM_IOC_GET_MVDECINFO == cmd)
				put_user(slots, &uarg->slots);
			if (slots) {
				if (AMSTREAM_IOC_GET_MVDECINFO == cmd) {
					if (vdec->mvfrm && copy_to_user((void *)&uarg->comm,
								&vdec->mvfrm->comm,
								sizeof(struct vframe_comm_s))) {
						r = -EFAULT;
						kfree(tmpbuf);
						mutex_unlock(&amstream_mutex);
						break;
					}
					if (struct_size == sizeof(struct av_param_mvdec_t_old)) {//old struct
						struct av_param_mvdec_t_old  __user *uarg_old = (void *)arg;
						int m;
						for (m=0; m<slots; m++)
							if (copy_to_user((void *)&uarg_old->minfo[m],
										&tmpbuf[m],
										sizeof(struct vframe_counter_s_old))) {
								r = -EFAULT;
								kfree(tmpbuf);
								mutex_unlock(&amstream_mutex);
								break;
							}
					} else if (struct_size == sizeof(struct av_param_mvdec_t)) {//new struct
						if (copy_to_user((void *)&uarg->minfo[0],
									tmpbuf,
									slots*sizeof(struct vframe_counter_s))) {
							r = -EFAULT;
							kfree(tmpbuf);
							mutex_unlock(&amstream_mutex);
							break;
						}
					} else {
						pr_err("pass in size %u,old struct size %u,current struct size %u\n",
						struct_size, (u32)sizeof(struct av_param_mvdec_t_old),(u32)sizeof(struct av_param_mvdec_t));
						pr_err("App use another picture size,we haven't support it.\n");
					}
				}else { //For compatibility, only copy the qos
					struct av_param_qosinfo_t  __user *uarg = (void *)arg;
					int i;
					for (i=0; i<slots; i++)
						if (copy_to_user((void *)&uarg->vframe_qos[i],
									&tmpbuf[i].qos,
									sizeof(struct vframe_qos_s))) {
							r = -EFAULT;
							kfree(tmpbuf);
							mutex_unlock(&amstream_mutex);
							break;
						}
				}
			} else {
				/*Vdec didn't produce item,wait for 10 ms to avoid user application
			      infinitely calling*/
				//msleep(10); let user app handle it.
			}
			kfree(tmpbuf);
		}
		mutex_unlock(&amstream_mutex);
		break;
	case AMSTREAM_IOC_GET_AVINFO:
		{
			struct av_param_info_t  __user *uarg = (void *)arg;
			struct av_info_t  av_info;
			int delay;
			u32 avgbps;
			if (this->type & PORT_TYPE_VIDEO) {
				av_info.first_pic_coming = get_first_pic_coming();
				av_info.current_fps = -1;
				av_info.vpts = timestamp_vpts_get();
				//av_info.vpts_err = tsync_get_vpts_error_num();
				av_info.apts = timestamp_apts_get();
				//av_info.apts_err = tsync_get_apts_error_num();
				av_info.ts_error = get_discontinue_counter();
				av_info.first_vpts = timestamp_firstvpts_get();
				av_info.toggle_frame_count = get_toggle_frame_count();
				delay = calculation_stream_delayed_ms(
					PTS_TYPE_VIDEO, NULL, &avgbps);
				if (delay >= 0)
					av_info.dec_video_bps = avgbps;
				else
					av_info.dec_video_bps = 0;
			}
			if (copy_to_user((void *)&uarg->av_info, (void *)&av_info,
						sizeof(struct av_info_t)))
				r = -EFAULT;
		}
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
	int i;

	switch (cmd) {

	case AMSTREAM_IOC_VB_START:
		if ((this->type & PORT_TYPE_VIDEO) &&
			((priv->vdec->vbuf.flag & BUF_FLAG_IN_USE) == 0)) {
			priv->vdec->vbuf.buf_start = arg;
		} else
			r = -EINVAL;
		break;

	case AMSTREAM_IOC_VB_SIZE:
		if ((this->type & PORT_TYPE_VIDEO) &&
			((priv->vdec->vbuf.flag & BUF_FLAG_IN_USE) == 0)) {
			if (priv->vdec->vbuf.flag & BUF_FLAG_ALLOC) {
				r += stbuf_change_size(
						&priv->vdec->vbuf,
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
				//tsync_audio_break(1);
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

			mutex_lock(&amstream_mutex);

			/*
			 *todo: check upper layer for decoder
			 * handler lifecycle
			 */
			if (priv->vdec == NULL) {
				r = -EINVAL;
				mutex_unlock(&amstream_mutex);
				break;
			}

			if (this->type & PORT_TYPE_FRAME) {
				struct vdec_input_status_s status;

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
				mutex_unlock(&amstream_mutex);
				break;
			}

			buf = &priv->vdec->vbuf;
			p->status.size = stbuf_canusesize(buf);
			p->status.data_len = stbuf_level(buf);
			p->status.free_len = stbuf_space(buf);
			p->status.read_pointer = stbuf_rp(buf);
			if (copy_to_user((void *)arg, p, sizeof(para)))
				r = -EFAULT;

			mutex_unlock(&amstream_mutex);
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
		else if ((this->type & PORT_TYPE_VIDEO) ||
			(this->type & PORT_TYPE_HEVC)) {
			struct stream_buf_s *vbuf = &priv->vdec->vbuf;
			if (vbuf->no_parser) {
				pts_checkin_offset(PTS_TYPE_VIDEO,
					vbuf->stream_offset, arg);
			} else {
				r = es_vpts_checkin(vbuf, arg);
			}
		} else if (this->type & PORT_TYPE_AUDIO)
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
			} else if ((this->type & PORT_TYPE_HEVC) ||
					(this->type & PORT_TYPE_VIDEO)) {
					struct stream_buf_s *vbuf = &priv->vdec->vbuf;
					if (vbuf->no_parser && !vdec_single(priv->vdec)) {
						pts_checkin_offset_us64(PTS_TYPE_VIDEO,
							vbuf->stream_offset, pts);
					} else {
						r = es_vpts_checkin_us64(
						&priv->vdec->vbuf, pts);
					}
			} else if (this->type & PORT_TYPE_AUDIO) {
					r = es_vpts_checkin_us64(
					&bufs[BUF_TYPE_AUDIO], pts);
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
			p->vstatus.euAspectRatio =
				get_normalized_aspect_ratio(
					vstatus.ratio_control);

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

			mutex_lock(&amstream_mutex);
			r = audio_port_reset(this, pabuf);
			mutex_unlock(&amstream_mutex);
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
				mutex_lock(&amstream_mutex);
				if (vdec_get_debug_flags() & 0x10000000)
					pr_info("%s, instance_id = %d\n", __func__, p_userdata_param->instance_id);
				vdec = vdec_get_vdec_by_video_id(p_userdata_param->instance_id);
				if (vdec) {
					if (vdec_read_user_data(vdec,
							p_userdata_param) == 0) {
						r = -EFAULT;
						mutex_unlock(&amstream_mutex);
						break;
					}

					if (copy_to_user((void *)arg,
						p_userdata_param,
						sizeof(struct userdata_param_t)))
						r = -EFAULT;
				} else
					r = -EINVAL;
				mutex_unlock(&amstream_mutex);
			}
		}
		break;

	case AMSTREAM_IOC_UD_AVAILABLE_VDEC:
		{
			unsigned int ready_vdec = 0;
			u32 ready_flag = 0;
			st_userdata *userdata = get_vdec_userdata_ctx();

			mutex_lock(&userdata->mutex);
			for (i = 0; i < MAX_USERDATA_CHANNEL_NUM; i++) {
				if (userdata->video_id == userdata->id[i] &&
					userdata->ready_flag[i] == 1) {
					ready_vdec = userdata->id[i];
					userdata->ready_flag[i] = 0;
					ready_flag = 1;
					break;
				}
			}
			if (!ready_flag) {
				pr_info("instance %d not ready!\n", userdata->video_id);
				r = -EINVAL;
			}
			mutex_unlock(&userdata->mutex);

			put_user(ready_vdec, (uint32_t __user *)arg);
			if (vdec_get_debug_flags() & 0x10000000)
				pr_info("%s, ready_vdec = %u\n", __func__, ready_vdec);
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
			int video_id;

			mutex_lock(&amstream_mutex);
			get_user(video_id, (int __user *)arg);
			pr_info("userdata flush id: %d\n", video_id);
			vdec = vdec_get_vdec_by_video_id(video_id);
			if (vdec) {
				vdec_reset_userdata_fifo(vdec, 0);
				pr_info("reset_userdata_fifo for vdec_id: %d video_id: %d\\n",
					vdec->id, vdec->video_id);
			}
			mutex_unlock(&amstream_mutex);
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
			u32 pts = 0, frame_size, offset;

			get_user(offset, (unsigned long __user *)arg);
			pts_lookup_offset(PTS_TYPE_AUDIO, offset, &pts,
				&frame_size, 300);
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
		//put_user(timestamp_pcrscr_get(), (int __user *)arg);
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
			struct subtitle_info *msub_info =
				vzalloc(sizeof(struct subtitle_info) * MAX_SUB_NUM);
			struct subtitle_info **psub_info =
				vzalloc(sizeof(struct subtitle_info) * MAX_SUB_NUM);
			int i;

			if (!msub_info || !psub_info) {
				r = -ENOMEM;
				break;
			}

			for (i = 0; i < MAX_SUB_NUM; i++)
				psub_info[i] = &msub_info[i];

			r = psparser_get_sub_info(psub_info);

			if (r == 0) {
				if (copy_to_user((void __user *)arg, msub_info,
				sizeof(struct subtitle_info) * MAX_SUB_NUM))
					r = -EFAULT;
			}
			vfree(msub_info);
			vfree(psub_info);
		}
		break;
	case AMSTREAM_IOC_SET_DEMUX:
		tsdemux_set_demux((int)arg);
		break;
	case AMSTREAM_IOC_SET_VIDEO_DELAY_LIMIT_MS:
		priv->vdec->vbuf.max_buffer_delay_ms = (int)arg;
		break;
	case AMSTREAM_IOC_SET_AUDIO_DELAY_LIMIT_MS:
		bufs[BUF_TYPE_AUDIO].max_buffer_delay_ms = (int)arg;
		break;
	case AMSTREAM_IOC_GET_VIDEO_DELAY_LIMIT_MS:
		put_user(priv->vdec->vbuf.max_buffer_delay_ms, (int *)arg);
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
			pr_err("set drmmode, input must be secure buffer\n");
			this->flag |= PORT_FLAG_DRM;
			if ((this->type & PORT_TYPE_VIDEO) &&
				(priv->vdec))
				priv->vdec->port_flag |= PORT_FLAG_DRM;
		} else if ((u32)arg == 2) {
			pr_err("set drmmode, input must be normal buffer\n");
			if ((this->type & PORT_TYPE_VIDEO) &&
				(priv->vdec)) {
				pr_err("vdec port_flag with drmmode\n");
				priv->vdec->port_flag |= PORT_FLAG_DRM;
			}
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
	case AMSTREAM_IOC_SET_CRC: {
		struct usr_crc_info_t crc_info;
		struct vdec_s *vdec;

		if (copy_from_user(&crc_info, (void __user *)arg,
			sizeof(struct usr_crc_info_t))) {
			return -EFAULT;
		}
		/*
		pr_info("id %d, frame %d, y_crc: %08x, uv_crc: %08x\n", crc_info.id,
			crc_info.pic_num, crc_info.y_crc, crc_info.uv_crc);
		*/
		vdec = vdec_get_vdec_by_id(crc_info.id);
		if (vdec == NULL)
			return -ENODEV;
		if (vdec->vfc.cmp_pool == NULL) {
			vdec->vfc.cmp_pool =
				vmalloc(USER_CMP_POOL_MAX_SIZE *
					sizeof(struct usr_crc_info_t));
			if (vdec->vfc.cmp_pool == NULL)
				return -ENOMEM;
		}
		if (vdec->vfc.usr_cmp_num >= USER_CMP_POOL_MAX_SIZE) {
			pr_info("warn: could not write any more, max %d",
				USER_CMP_POOL_MAX_SIZE);
			return -EFAULT;
		}
		memcpy(&vdec->vfc.cmp_pool[vdec->vfc.usr_cmp_num], &crc_info,
			sizeof(struct usr_crc_info_t));
		vdec->vfc.usr_cmp_num++;
		break;
	}
	case AMSTREAM_IOC_GET_CRC_CMP_RESULT: {
		int val, vdec_id;
		struct vdec_s *vdec;

		if (get_user(val, (int __user *)arg)) {
			return -EFAULT;
		}
		vdec_id = val & 0x00ff;
		vdec = vdec_get_vdec_by_id(vdec_id);
		if (vdec == NULL)
			return -ENODEV;
		if (val & 0xff00)
			put_user(vdec->vfc.usr_cmp_num, (int *)arg);
		else
			put_user(vdec->vfc.usr_cmp_result, (int *)arg);
		/*
		pr_info("amstream get crc32 cmpare num %d result: %d\n",
			vdec->vfc.usr_cmp_num, vdec->vfc.usr_cmp_result);
		*/
		break;
	}
	case AMSTREAM_IOC_INIT_EX_STBUF: {
		struct stream_buffer_metainfo parm;
		struct stream_buf_s *vbuf = NULL;

		if (priv->vdec == NULL) {
			pr_err("init %s, no vdec.\n", __func__);
			return -EFAULT;
		}

		vbuf = &priv->vdec->vbuf;
		if (vbuf == NULL) {
			pr_err("init %s, no stbuf.\n", __func__);
			return -EFAULT;
		}

		if (copy_from_user(&parm, (void __user *)arg,
			sizeof(struct stream_buffer_metainfo))) {
			return -EFAULT;
		}

		priv->vdec->pts_server_id = parm.pts_server_id;
		pr_info("pts server id = %x\n", parm.pts_server_id);

		stream_buffer_set_ext_buf(vbuf, parm.stbuf_start,
			parm.stbuf_size, parm.stbuf_flag);
		break;
	}
	case AMSTREAM_IOC_WR_STBUF_META: {
		struct stream_buffer_metainfo meta;
		struct stream_buf_s *vbuf = NULL;

		if (priv->vdec == NULL) {
			pr_err("write %s, no vdec.\n", __func__);
			return -EFAULT;
		}

		vbuf = &priv->vdec->vbuf;
		if (vbuf == NULL) {
			pr_err("write %s, no stbuf.\n", __func__);
			return -EFAULT;
		}

		if (vbuf->ops == NULL) {
			pr_err("write %s, no ops.\n", __func__);
			return -EFAULT;
		}

		if (copy_from_user(&meta, (void __user *)arg,
			sizeof(struct stream_buffer_metainfo))) {
			return -EFAULT;
		}
		if (!vbuf->ext_buf_addr)
			return -ENODEV;

		stream_buffer_meta_write(vbuf, &meta);
		break;
	}
	case AMSTREAM_IOC_GET_STBUF_STATUS: {
		struct stream_buffer_status st;
		struct stream_buf_s *pbuf = NULL;

		if (priv->vdec == NULL) {
			pr_err("get status %s, no vdec.\n", __func__);
			return -EFAULT;
		}

		pbuf = &priv->vdec->vbuf;
		if (pbuf == NULL) {
			pr_err("get status %s, no stbuf.\n", __func__);
			return -EFAULT;
		}

		if (pbuf->ops == NULL) {
			pr_err("get status %s, no ops.\n", __func__);
			return -EFAULT;
		}

		st.stbuf_start = pbuf->ext_buf_addr;
		st.stbuf_size = pbuf->buf_size;
		st.stbuf_rp = pbuf->ops->get_rp(pbuf);
		st.stbuf_wp = pbuf->ops->get_wp(pbuf);
		if (copy_to_user((void __user *)arg, &st,
			sizeof(struct stream_buffer_status))) {
			return -EFAULT;
		}
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
	case AMSTREAM_IOC_GET_QOSINFO:
	case AMSTREAM_IOC_GET_MVDECINFO:
	case AMSTREAM_IOC_GET_AVINFO:
		r = amstream_do_ioctl_new(priv, cmd, arg);
		break;
	default:
		r = amstream_do_ioctl_old(priv, cmd, arg);
		break;
	}
	if (r != 0)
		pr_debug("amstream_do_ioctl error :%lx, %x\n", r, cmd);

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
	if (!access_ok(data, sizeof(*data)))
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
	if (!access_ok(data, sizeof(*data)))
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
	if (!access_ok(data, sizeof(*data)))
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

static int show_vbuf_status_cb(struct stream_buf_s *p, char *buf)
{
	char *pbuf = buf;

	if (!p->buf_start)
		return 0;
	/*type */
	pbuf += sprintf(pbuf, "Video-%d buffer:", p->id);
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
	pbuf += sprintf(pbuf, "\tbuf size:%#x\n", p->buf_size);
	pbuf += sprintf(pbuf, "\tbuf canusesize:%#x\n", p->canusebuf_size);
	pbuf += sprintf(pbuf, "\tbuf regbase:%#lx\n", p->reg_base);

	if (p->reg_base && p->flag & BUF_FLAG_IN_USE) {
		pbuf += sprintf(pbuf, "\tbuf level:%#x\n",
				stbuf_level(p));
		pbuf += sprintf(pbuf, "\tbuf space:%#x\n",
				stbuf_space(p));
		pbuf += sprintf(pbuf, "\tbuf read pointer:%#x\n",
				stbuf_rp(p));
	} else
		pbuf += sprintf(pbuf, "\tbuf no used.\n");

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

		if (!p->buf_start)
			continue;

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

	pbuf += show_stream_buffer_status(pbuf, show_vbuf_status_cb);

	return pbuf - buf;
}

static ssize_t videobufused_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	char *pbuf = buf;
	struct stream_buf_s *p = NULL;

	p = &bufs[0];

	if (p->flag & BUF_FLAG_IN_USE)
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

static ssize_t vcodec_feature_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	return vcodec_feature_read(buf);
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

static ssize_t canuse_buferlevel_show(struct class *class,
			struct class_attribute *attr, char *buf)
{
	ssize_t size = sprintf(buf,
		"use_bufferlevel=%d/10000[=(set range[ 0~10000])=\n",
			use_bufferlevelx10000);
	return size;
}

static ssize_t canuse_buferlevel_store(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t size)
{
	unsigned int val;
	ssize_t ret;

	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);

	if (ret != 0)
		return -EINVAL;
	(void)val;
	reset_canuse_buferlevel(val);
	return size;
}

static ssize_t max_buffer_delay_ms_store(struct class *class,
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

static ssize_t max_buffer_delay_ms_show(struct class *class,
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

static ssize_t reset_audio_port_store(struct class *class,
	struct class_attribute *attr,
	const char *buf, size_t size)
{
	unsigned int val = 0;
	int i;
	ssize_t ret;
	struct stream_buf_s *pabuf = &bufs[BUF_TYPE_AUDIO];
	struct stream_port_s *this;

	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	if (val != 1)
		return -EINVAL;
	mutex_lock(&amstream_mutex);
	for (i = 0; i < MAX_AMSTREAM_PORT_NUM; i++) {
		if (strcmp(ports[i].name, "amstream_mpts") == 0 ||
			strcmp(ports[i].name, "amstream_mpts_sched") == 0) {
			this = &ports[i];
			if ((this->flag & PORT_FLAG_AFORMAT) != 0) {
				pr_info("audio_port_reset %s\n", ports[i].name);
				audio_port_reset(this, pabuf);
			}
		}
	}
	mutex_unlock(&amstream_mutex);
	return size;
}

ssize_t dump_stream_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	char *p_buf = buf;

	p_buf += sprintf(p_buf, "\nmdkir -p /data/tmp -m 777;setenforce 0;\n\n");
	p_buf += sprintf(p_buf, "video:\n\t echo 0 > /sys/class/amstream/dump_stream;\n");
	p_buf += sprintf(p_buf, "hevc :\n\t echo 4 > /sys/class/amstream/dump_stream;\n");

	return p_buf - buf;
}

#define DUMP_STREAM_FILE   "/data/tmp/dump_stream.h264"
ssize_t dump_stream_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	struct stream_buf_s *p_buf;
	int ret = 0, id = 0;
	unsigned int stride, remain, level, vmap_size;
	int write_size;
	void *stbuf_vaddr;
	unsigned long offset;
	struct file *fp;
	mm_segment_t old_fs;
	loff_t fpos;

	ret = sscanf(buf, "%d", &id);
	if (ret < 0) {
		pr_info("paser buf id fail, default id = 0\n");
		id = 0;
	}
	if (id != BUF_TYPE_VIDEO && id != BUF_TYPE_HEVC) {
		pr_info("buf id out of range, max %d, id %d, set default id 0\n", BUF_MAX_NUM - 1, id);
		id = 0;
	}
	p_buf = get_stream_buffer(id);
	if (!p_buf) {
		pr_info("get buf fail, id %d\n", id);
		return size;
	}
	if ((!p_buf->buf_size) || (p_buf->is_secure) || (!(p_buf->flag & BUF_FLAG_IN_USE))) {
		pr_info("buf size %d, is_secure %d, in_use %d, it can not dump\n",
			p_buf->buf_size, p_buf->is_secure, (p_buf->flag & BUF_FLAG_IN_USE));
		return size;
	}

	level = stbuf_level(p_buf);
	if (!level || level > p_buf->buf_size) {
		pr_info("stream buf level %d, buf size %d, error return\n", level, p_buf->buf_size);
		return size;
	}

	fp = filp_open(DUMP_STREAM_FILE, O_CREAT | O_RDWR, 0666);
	if (IS_ERR(fp)) {
		fp = NULL;
		pr_info("create dump stream file failed\n");
		return size;
	}

	offset = p_buf->buf_start;
	remain = level;
	stride = SZ_1M;
	vmap_size = 0;
	fpos = 0;
	pr_info("create file success, it will dump from addr 0x%lx, size 0x%x\n", offset, remain);
	while (remain > 0) {
		if (remain > stride)
			vmap_size = stride;
		else {
			stride = remain;
			vmap_size = stride;
		}

		stbuf_vaddr = codec_mm_vmap(offset, vmap_size);
		if (stbuf_vaddr == NULL) {
			stride >>= 1;
			pr_info("vmap fail change vmap stide size 0x%x\n", stride);
			continue;
		}
		codec_mm_dma_flush(stbuf_vaddr, vmap_size, DMA_FROM_DEVICE);

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		write_size = vfs_write(fp, stbuf_vaddr, vmap_size, &fpos);
		if (write_size < vmap_size) {
			write_size += vfs_write(fp, stbuf_vaddr + write_size, vmap_size - write_size, &fpos);
			pr_info("fail write retry, total %d, write %d\n", vmap_size, write_size);
			if (write_size < vmap_size) {
				pr_info("retry fail, interrupt dump stream, break\n");
				set_fs(old_fs);
				break;
			}
		}
		set_fs(old_fs);
		vfs_fsync(fp, 0);
		pr_info("vmap_size 0x%x dump size 0x%x\n", vmap_size, write_size);

		offset += vmap_size;
		remain -= vmap_size;
		codec_mm_unmap_phyaddr(stbuf_vaddr);
	}

	filp_close(fp, current->files);
	pr_info("dump stream buf end\n");

	return size;
}

static CLASS_ATTR_RO(ports);
static CLASS_ATTR_RO(bufs);
static CLASS_ATTR_RO(vcodec_profile);
static CLASS_ATTR_RO(vcodec_feature);
static CLASS_ATTR_RO(videobufused);
static CLASS_ATTR_RW(canuse_buferlevel);
static CLASS_ATTR_RW(max_buffer_delay_ms);
static CLASS_ATTR_WO(reset_audio_port);

static struct attribute *amstream_class_attrs[] = {
	&class_attr_ports.attr,
	&class_attr_bufs.attr,
	&class_attr_vcodec_profile.attr,
	&class_attr_vcodec_feature.attr,
	&class_attr_videobufused.attr,
	&class_attr_canuse_buferlevel.attr,
	&class_attr_max_buffer_delay_ms.attr,
	&class_attr_reset_audio_port.attr,
	NULL
};

ATTRIBUTE_GROUPS(amstream_class);

static struct class amstream_class = {
	.name = "amstream",
	.class_groups = amstream_class_groups,
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

	amstream_userdata_init();
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

module_param(force_dv_mode, uint, 0664);
MODULE_PARM_DESC(force_dv_mode,
	"\n force_dv_mode \n");

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
