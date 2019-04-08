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
#include "aml_vcodec_vfm.h"
#include "aml_vcodec_vfq.h"
#include "aml_vcodec_util.h"
#include "aml_vcodec_adapt.h"
#include <media/v4l2-mem2mem.h>

#define RECEIVER_NAME	"v4l2-video"
#define PROVIDER_NAME	"v4l2-video"

static struct vframe_s *vdec_vf_peek(void *op_arg)
{
	struct vcodec_vfm_s *vfm = (struct vcodec_vfm_s *)op_arg;

	return vfq_peek(&vfm->vf_que);
}

static struct vframe_s *vdec_vf_get(void *op_arg)
{
	struct vcodec_vfm_s *vfm = (struct vcodec_vfm_s *)op_arg;

	return vfq_pop(&vfm->vf_que);
}

static void vdec_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct vcodec_vfm_s *vfm = (struct vcodec_vfm_s *)op_arg;

	vf_put(vf, vfm->recv_name);
	vf_notify_provider(vfm->recv_name, VFRAME_EVENT_RECEIVER_PUT, NULL);
}

static int vdec_event_cb(int type, void *data, void *private_data)
{

	if (type & VFRAME_EVENT_RECEIVER_PUT) {
	} else if (type & VFRAME_EVENT_RECEIVER_GET) {
	} else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT) {
	}
	return 0;
}

static int vdec_vf_states(struct vframe_states *states, void *op_arg)
{
	struct vcodec_vfm_s *vfm = (struct vcodec_vfm_s *)op_arg;

	states->vf_pool_size	= POOL_SIZE;
	states->buf_recycle_num	= 0;
	states->buf_free_num	= POOL_SIZE - vfq_level(&vfm->vf_que);
	states->buf_avail_num	= vfq_level(&vfm->vf_que);

	return 0;
}

void video_vf_put(char *receiver, struct vdec_fb *fb, int id)
{
	struct vframe_provider_s *vfp = vf_get_provider(receiver);
	struct vframe_s *vf = (struct vframe_s *)fb->vf_handle;

	aml_v4l2_debug(2, "[%d] TO   (%s) vf: %p, idx: %d",
		id, vfp->name, vf, vf->index);

	if (vfp && vf)
		vf_put(vf, receiver);
}

static const struct vframe_operations_s vf_provider = {
	.peek		= vdec_vf_peek,
	.get		= vdec_vf_get,
	.put		= vdec_vf_put,
	.event_cb	= vdec_event_cb,
	.vf_states	= vdec_vf_states,
};

static int video_receiver_event_fun(int type, void *data, void *private_data)
{
	int ret = 0;
	struct vframe_states states;
	struct vcodec_vfm_s *vfm = (struct vcodec_vfm_s *)private_data;

	//aml_v4l2_debug(4, "[%d] type: %d, vfm: %p", vfm->ctx->id, type, vfm);

	switch (type) {
	case VFRAME_EVENT_PROVIDER_UNREG: {
		if (vf_get_receiver(vfm->prov_name)) {
			aml_v4l2_debug(4, "[%d] unreg %s provider.",
				vfm->ctx->id, vfm->prov_name);
			vf_unreg_provider(&vfm->vf_prov);
		}

		break;
	}

	case VFRAME_EVENT_PROVIDER_START: {
		if (vf_get_receiver(vfm->prov_name)) {
			aml_v4l2_debug(4, "[%d] reg %s provider.",
				vfm->ctx->id, vfm->prov_name);
			vf_provider_init(&vfm->vf_prov, vfm->prov_name,
				&vf_provider, vfm);
			vf_reg_provider(&vfm->vf_prov);
			vf_notify_receiver(vfm->prov_name,
				VFRAME_EVENT_PROVIDER_START, NULL);
		}

		vfq_init(&vfm->vf_que, POOL_SIZE + 1, &vfm->pool[0]);

		break;
	}

	case VFRAME_EVENT_PROVIDER_QUREY_STATE: {
		vdec_vf_states(&states, vfm);
		if (states.buf_avail_num > 0)
			ret = RECEIVER_ACTIVE;
		break;
	}

	case VFRAME_EVENT_PROVIDER_VFRAME_READY: {
		if (vfq_level(&vfm->vf_que) > POOL_SIZE - 1)
			ret = -1;

		if (!vf_peek(vfm->recv_name))
			ret = -1;

		vfm->vf = vf_get(vfm->recv_name);
		if (!vfm->vf)
			ret = -1;

		if (ret < 0) {
			pr_err("[%d] receiver vf err.\n", vfm->ctx->id);
			break;
		}

		vfq_push(&vfm->vf_que, vfm->vf);

		/*vf_notify_receiver(vfm->prov_name,
			VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);*/

		/* schedule capture work. */
		vdec_device_vf_run(vfm->ctx);

		aml_v4l2_debug(2, "[%d] FROM (%s) vf: %p, idx: %d",
			vfm->ctx->id, vf_get_provider(vfm->recv_name)->name,
			vfm->vf, vfm->vf->index);
		break;
	}

	default:
		aml_v4l2_debug(4, "[%d] the vf event is %d", vfm->ctx->id, type);
	}

	return ret;
}

static const struct vframe_receiver_op_s vf_receiver = {
	.event_cb	= video_receiver_event_fun
};

struct vframe_s *peek_video_frame(struct vcodec_vfm_s *vfm)
{
	return vfq_peek(&vfm->vf_que);
}

struct vframe_s *get_video_frame(struct vcodec_vfm_s *vfm)
{
	return vfq_pop(&vfm->vf_que);
}

int vcodec_vfm_init(struct vcodec_vfm_s *vfm)
{
	snprintf(vfm->recv_name, VF_NAME_SIZE, "%s-%d",
		RECEIVER_NAME, vfm->ctx->id);
	snprintf(vfm->prov_name, VF_NAME_SIZE, "%s-%d",
		PROVIDER_NAME, vfm->ctx->id);

	vfm->ada_ctx->recv_name = vfm->recv_name;

	vf_receiver_init(&vfm->vf_recv, vfm->recv_name, &vf_receiver, vfm);
	vf_reg_receiver(&vfm->vf_recv);

	return 0;
}

void vcodec_vfm_release(struct vcodec_vfm_s *vfm)
{
	vf_unreg_receiver(&vfm->vf_recv);
}

