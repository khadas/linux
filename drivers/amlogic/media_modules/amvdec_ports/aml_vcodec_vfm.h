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
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#ifndef __AML_VCODEC_VFM_H_
#define __AML_VCODEC_VFM_H_

#include "aml_vcodec_vfq.h"
#include "aml_vcodec_drv.h"
#include "aml_vcodec_dec.h"
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>

#define VF_NAME_SIZE	(32)
#define POOL_SIZE	(64)

struct vcodec_vfm_s {
	struct aml_vcodec_ctx *ctx;
	struct aml_vdec_adapt *ada_ctx;
	struct vfq_s vf_que;
	struct vframe_s *vf;
	struct vframe_s *pool[POOL_SIZE + 1];
	char recv_name[VF_NAME_SIZE];
	char prov_name[VF_NAME_SIZE];
	struct vframe_provider_s vf_prov;
	struct vframe_receiver_s vf_recv;
};

int vcodec_vfm_init(struct vcodec_vfm_s *vfm);

void vcodec_vfm_release(struct vcodec_vfm_s *vfm);

struct vframe_s *peek_video_frame(struct vcodec_vfm_s *vfm);

struct vframe_s *get_video_frame(struct vcodec_vfm_s *vfm);

int get_fb_from_queue(struct aml_vcodec_ctx *ctx, struct vdec_fb **out_fb);
int put_fb_to_queue(struct aml_vcodec_ctx *ctx, struct vdec_fb *in_fb);

void video_vf_put(char *receiver, struct vdec_fb *fb, int id);

#endif /* __AML_VCODEC_VFM_H_ */
