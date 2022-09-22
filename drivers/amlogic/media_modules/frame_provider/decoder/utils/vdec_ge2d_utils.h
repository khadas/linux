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
#ifndef __GE2D_UTILS_H__
#define __GE2D_UTILS_H__

//#include "../../../amvdec_ports/aml_vcodec_drv.h"
#include <linux/amlogic/media/vfm/vframe.h>


/* define ge2d work mode. */
#define GE2D_MODE_CONVERT_NV12		(1 << 0)
#define GE2D_MODE_CONVERT_NV21		(1 << 1)
#define GE2D_MODE_CONVERT_LE		(1 << 2)
#define GE2D_MODE_CONVERT_BE		(1 << 3)
#define GE2D_MODE_SEPARATE_FIELD	(1 << 4)
#define GE2D_MODE_422_TO_420		(1 << 5)

struct vdec_canvas_res {
	int	cid;
	u8	name[32];
};

struct vdec_canvas_cache {
	int	ref;
	struct vdec_canvas_res	res[6];
	struct mutex		lock;
};

struct vdec_ge2d {
	u32	work_mode; /* enum ge2d_work_mode */
	struct ge2d_context_s	*ge2d_context; /* handle of GE2D */
	struct aml_vcodec_ctx	*ctx;
	struct vdec_canvas_cache	canche;
	void *hw;
};

struct vdec_ge2d_info {
	struct vframe_s *dst_vf;
	u32 src_canvas0Addr;
	u32 src_canvas1Addr;;
	struct canvas_config_s src_canvas0_config[3];
	struct canvas_config_s src_canvas1_config[3];
};

int vdec_ge2d_init(struct vdec_ge2d** ge2d_handle, int mode);

int vdec_ge2d_copy_data(struct vdec_ge2d *ge2d, struct vdec_ge2d_info *ge2d_info);

int vdec_ge2d_destroy(struct vdec_ge2d *ge2d);

#endif

