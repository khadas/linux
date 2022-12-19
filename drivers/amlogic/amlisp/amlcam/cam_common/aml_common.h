/*
*
* SPDX-License-Identifier: GPL-2.0
*
* Copyright (C) 2020 Amlogic or its affiliates
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

#ifndef __AML_P1_COMMON_H__
#define __AML_P1_COMMON_H__

#include <media/media-device.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-ctrls.h>

#ifdef T7C_CHIP
#define ISP_REG_BASE  0xfe3b0000
#define ISP_REG_LEN   0xb000
#else
#define ISP_REG_BASE  0xff000000
#define ISP_REG_LEN   0x11000
#endif
#define ISP_SIZE_ALIGN(data, aln)   (((data) + (aln) -1) & (~((aln) - 1)))
#define ISP_CHN_CNT 3
#define DATA_ALIGN(data, align) ((data + align - 1) & (~(align - 1)))

enum aml_plane {
	AML_PLANE_A  = 0,
	AML_PLANE_B = 1,
	AML_PLANE_C = 2,
	AML_PLANE_MAX
};

enum {
	AML_OFF = 0,
	AML_ON,
};

enum {
	STATUS_STOP,
	STATUS_START,
	STATUS_MAX
};

enum {
	AML_FMT_YUV444,
	AML_FMT_YUV422,
	AML_FMT_YUV420,
	AML_FMT_YUV400,
	AML_FMT_RAW,
	AML_FMT_HDR_RAW,
	AML_FMT_MAX
};

enum {
	WDR_MODE_NONE,
	WDR_MODE_2To1_LINE,
	WDR_MODE_2To1_FRAME,
	SDR_DDR_MODE,
	ISP_SDR_DCAM_MODE,
};

struct aml_format {
	u32 xstart;
	u32 ystart;
	u32 width;
	u32 height;
	u32 code;
	u32 fourcc;
	u32 nplanes;
	u32 bpp;
	u32 size;
};

struct aml_reg {
	u32 val;
	u32 addr;
};

struct aml_crop {
	u32 hstart;
	u32 vstart;
	u32 hsize;
	u32 vsize;
};

struct aml_dbuffer {
	int fd;
	struct sg_table *sgt;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
};

struct aml_buffer {
	struct vb2_v4l2_buffer vb;
	u32 nplanes;
	u32 bsize;
	u32 devno;
	void *vaddr[AML_PLANE_MAX];
	dma_addr_t addr[AML_PLANE_MAX];
	struct aml_dbuffer dbuffer;
	struct list_head list;
};

struct aml_control {
	struct v4l2_ctrl_handler hdl_std_ctrl; /* v4l2 ctrl hdl */
	u32 fps_sensor;
	u32 fps_output;
};

struct aml_slice {
	int pos;
	int xsize;
	int pleft_hsize;
	int pright_hsize;
	int pleft_ovlp;
	int pright_ovlp;
	int left_hsize;
	int right_hsize;
	int left_ovlp;
	int right_ovlp;
	int whole_frame_hcenter;
	int right_frame_hstart;

	int reg_crop_hstart;
	int reg_crop_hsize;
	int reg_hsc_ini_integer[2];
	int reg_hsc_ini_phs0[2];
	int right_hsc_ini_integer[2];
	int right_hsc_ini_phs0[2];
};

struct aml_cap_ops {
	int (*cap_irq_handler)(void *video, int status);
	int (*cap_set_format)(void *video);
	int (*cap_cfg_buffer)(void *video, void *buff);
	void (*cap_stream_on)(void *video);
	void (*cap_stream_off)(void *video);
	void (*cap_flush_buffer)(void *video);
};

struct aml_sub_ops {
	int (*set_format)(void *priv, void *fmt, void *p_fmt);
	int (*stream_on)(void *priv);
	void (*stream_off)(void *priv);
	void (*log_status)(void *priv);
};

struct aml_subdev {
	char *name;
	u32 pad_max;
	u32 fmt_cnt;
	u32 function;
	struct device *dev;
	struct v4l2_subdev *sd;
	struct v4l2_device *v4l2_dev;
	struct media_pad *pads;
	const struct aml_format *formats;
	struct v4l2_mbus_framefmt *pfmt;
	const struct aml_sub_ops *ops;
	void *priv;
};

struct aml_video {
	int id;
	int status;
    int rot_type;
	char *name;
	char *bus_info;
	struct device *dev;
	struct v4l2_device *v4l2_dev;
	struct media_pad pad;
	struct vb2_queue vb2_q;
	struct video_device vdev;
	const struct aml_format *format;
	u32 fmt_cnt;
	u32 frm_cnt;
    u32 disp_sel;
	u32 min_buffer_count;
	struct aml_crop acrop;
	struct aml_format afmt;
	struct v4l2_format f_current;
	struct aml_buffer *b_current;
	enum v4l2_buf_type type;
	struct media_pipeline *pipe;
	const struct aml_cap_ops *ops;
	spinlock_t buff_list_lock;
	struct mutex lock;
	struct mutex q_lock;
	struct list_head head;
	struct aml_control actrl;
	void *priv;
};
int aml_subdev_register(struct aml_subdev *subdev);
void aml_subdev_unregister(struct aml_subdev *subdev);

int aml_video_register(struct aml_video *video);
void aml_video_unregister(struct aml_video *video);

int aml_subdev_cma_alloc(struct platform_device *pdev, u32 *paddr, void *addr, unsigned long size);
void aml_subdev_cma_free(struct platform_device *pdev, void *paddr, unsigned long size);
void * aml_subdev_map_vaddr(u32 phys_addr, u32 length);
void aml_subdev_unmap_vaddr(void *vaddr);

#endif /* __AML_P1_COMMON_H__ */
