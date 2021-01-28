// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/amlogic/major.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/video_sink/video_signal_notify.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/dma-mapping.h>
#include <linux/dma-contiguous.h>
#include <linux/sched.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include "video_priv.h"
#include "video_reg.h"
#ifdef CONFIG_AMLOGIC_MEDIA_FRAME_SYNC
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#endif
#include <linux/amlogic/media/utils/vdec_reg.h>

#include <linux/amlogic/media/registers/register.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "videolog.h"
#include "video_reg.h"
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEOCAPTURE
#include "amvideocap_priv.h"
#endif
#ifdef CONFIG_AM_VIDEO_LOG
#define AMLOG
#endif
#include <linux/amlogic/media/utils/amlog.h>
MODULE_AMLOG(LOG_LEVEL_ERROR, 0, LOG_DEFAULT_LEVEL_DESC, LOG_MASK_DESC);

#include <linux/amlogic/media/video_sink/vpp.h>
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
#include "linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h"
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#include "../common/rdma/rdma.h"
#endif
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include "../common/vfm/vfm.h"
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
#include <linux/amlogic/media/di/di.h>
#endif
#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
#include <linux/amlogic/media/vpu_secure/vpu_secure.h>
#endif

#include <linux/math64.h>
#include "video_receiver.h"

struct video_layer_s vd_layer_vpp[2];
static int vsync_enter_line_max_vpp[2];
static int vsync_exit_line_max_vpp[2];

static inline bool is_vpp0(u8 layer_id)
{
	if (vd_layer[layer_id].vpp_index == VPP0)
		return true;
	else
		return false;
}

static inline bool is_vpp1(u8 layer_id)
{
	if (vd_layer_vpp[layer_id].vpp_index == VPP1)
		return true;
	else
		return false;
}

static inline bool is_vpp2(u8 layer_id)
{
	if (vd_layer_vpp[layer_id].vpp_index == VPP2)
		return true;
	else
		return false;
}

/* vd2 vpp2 */
irqreturn_t vsync_isr_viu2(int irq, void *dev_id)
{
	int hold_line;
	int enc_line;
	s32 vout_type;
	struct vframe_s *path0_new_frame = NULL;
	struct vframe_s *new_frame = NULL;
	u32 cur_blackout;
	static s32 cur_vd2_path_id = VFM_PATH_INVALID;
	u32 next_afbc_request = atomic_read(&gafbc_request);
	int axis[4];
	int crop[4];
	u8 layer_id = 1, vpp_id = 0, recv_id = 3;
	s32 vd2_path_id = glayer_info[layer_id].display_path_id;

	if (video_suspend && video_suspend_cycle >= 1) {
		if (log_out)
			pr_info("video suspend, vsync exit\n");
		log_out = 0;
		return IRQ_HANDLED;
	}
	if (debug_flag & DEBUG_FLAG_VSYNC_DONONE)
		return IRQ_HANDLED;
	/*if (is_vpp1(layer_id)) */

	if (cur_vd2_path_id == 0xff)
		cur_vd2_path_id = vd2_path_id;

	vout_type = detect_vout_type(vinfo);
	hold_line = calc_hold_line();

	glayer_info[layer_id].need_no_compress =
		(next_afbc_request & 2) ? true : false;
	vd_layer_vpp[vpp_id].bypass_pps = bypass_pps;
	vd_layer_vpp[vpp_id].global_debug = debug_flag;
	vd_layer_vpp[vpp_id].vout_type = vout_type;

#ifdef CONFIG_AMLOGIC_VIDEO_COMPOSER
	vsync_notify_video_composer();
#endif
	enc_line = get_cur_enc_line();
	if (enc_line > vsync_enter_line_max_vpp[vpp_id])
		vsync_enter_line_max_vpp[vpp_id] = enc_line;

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	vsync_rdma_config_pre();
#endif

	if (atomic_read(&video_unreg_flag))
		goto exit;

	if (atomic_read(&video_pause_flag) &&
	    !(vd_layer_vpp[vpp_id].global_output == 1 &&
	      vd_layer_vpp[vpp_id].enabled != vd_layer_vpp[vpp_id].enabled_status_saved))
		goto exit;

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	if (is_vsync_rdma_enable()) {
		vd_layer_vpp[vpp_id].cur_canvas_id = vd_layer_vpp[vpp_id].next_canvas_id;
	} else {
		if (rdma_enable_pre)
			goto exit;
		vd_layer_vpp[vpp_id].cur_canvas_id = 0;
		vd_layer_vpp[vpp_id].next_canvas_id = 1;
	}
#endif
	if (gvideo_recv[recv_id])
		gvideo_recv[recv_id]->func->early_proc(gvideo_recv[recv_id], 0);

	/* video_render.3 toggle frame */
	if (gvideo_recv[recv_id]) {
		path0_new_frame =
			gvideo_recv[recv_id]->func->dequeue_frame(gvideo_recv[recv_id]);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
		if (vd2_path_id == gvideo_recv[recv_id]->path_id) {
			amvecm_on_vs
				((gvideo_recv[recv_id]->cur_buf !=
				 &gvideo_recv[recv_id]->local_buf)
				? gvideo_recv[recv_id]->cur_buf : NULL,
				path0_new_frame,
				CSC_FLAG_CHECK_OUTPUT,
				0,
				0,
				0,
				0,
				0,
				0,
				VD2_PATH);
		}
#endif
	}
	if (!vd_layer_vpp[vpp_id].global_output) {
		cur_vd2_path_id = VFM_PATH_INVALID;
		vd2_path_id = VFM_PATH_INVALID;
	}

	if (gvideo_recv[recv_id] &&
	    vd_layer_vpp[vpp_id].dispbuf_mapping == &gvideo_recv[recv_id]->cur_buf &&
	    (gvideo_recv[recv_id]->cur_buf == &gvideo_recv[recv_id]->local_buf ||
	     !gvideo_recv[recv_id]->cur_buf) &&
	    vd_layer_vpp[vpp_id].dispbuf != gvideo_recv[recv_id]->cur_buf)
		vd_layer_vpp[vpp_id].dispbuf = gvideo_recv[recv_id]->cur_buf;

	/* vd2 config */
	if (gvideo_recv[recv_id] &&
	    gvideo_recv[recv_id]->path_id == vd2_path_id) {
		/* video_render.3 display on VD2 */
		new_frame = path0_new_frame;
		if (!new_frame) {
			if (!gvideo_recv[recv_id]->cur_buf) {
				/* video_render.3 no frame in display */
				if (cur_vd2_path_id != vd2_path_id)
					safe_switch_videolayer(layer_id, false, true);
				vd_layer_vpp[vpp_id].dispbuf = NULL;
			} else if (gvideo_recv[recv_id]->cur_buf ==
				&gvideo_recv[recv_id]->local_buf) {
				/* video_render.3 keep frame */
				vd_layer_vpp[vpp_id].dispbuf = gvideo_recv[recv_id]->cur_buf;
			} else if (vd_layer_vpp[vpp_id].dispbuf
				!= gvideo_recv[recv_id]->cur_buf) {
				/* video_render.3 has frame in display */
				new_frame = gvideo_recv[recv_id]->cur_buf;
			}
		}
		if (new_frame || gvideo_recv[recv_id]->cur_buf)
			vd_layer_vpp[vpp_id].dispbuf_mapping = &gvideo_recv[recv_id]->cur_buf;
		cur_blackout = 1;
	}

	if (!new_frame && vd_layer_vpp[vpp_id].dispbuf &&
	    is_local_vf(vd_layer_vpp[vpp_id].dispbuf)) {
		if (cur_blackout) {
			vd_layer_vpp[vpp_id].property_changed = false;
		} else if (vd_layer_vpp[vpp_id].dispbuf) {
			vd_layer_vpp[vpp_id].dispbuf->canvas0Addr =
				get_layer_display_canvas(1);
		}
	}

	if (vd_layer_vpp[vpp_id].dispbuf &&
	    (vd_layer_vpp[vpp_id].dispbuf->flag & (VFRAME_FLAG_VIDEO_COMPOSER |
		VFRAME_FLAG_VIDEO_DRM)) &&
	    !(debug_flag & DEBUG_FLAG_AXIS_NO_UPDATE)) {
		axis[0] = vd_layer_vpp[vpp_id].dispbuf->axis[0];
		axis[1] = vd_layer_vpp[vpp_id].dispbuf->axis[1];
		axis[2] = vd_layer_vpp[vpp_id].dispbuf->axis[2];
		axis[3] = vd_layer_vpp[vpp_id].dispbuf->axis[3];
		crop[0] = vd_layer_vpp[vpp_id].dispbuf->crop[0];
		crop[1] = vd_layer_vpp[vpp_id].dispbuf->crop[1];
		crop[2] = vd_layer_vpp[vpp_id].dispbuf->crop[2];
		crop[3] = vd_layer_vpp[vpp_id].dispbuf->crop[3];
		_set_video_window(&glayer_info[layer_id], axis);
		_set_video_crop(&glayer_info[layer_id], crop);
		set_alpha_scpxn(&vd_layer_vpp[vpp_id],
				vd_layer_vpp[vpp_id].dispbuf->componser_info);
		glayer_info[layer_id].zorder = vd_layer_vpp[vpp_id].dispbuf->zorder;
	}

	/* setting video display property in underflow mode */
	if (!new_frame &&
	    vd_layer_vpp[vpp_id].dispbuf &&
	    vd_layer_vpp[vpp_id].property_changed) {
		pip_swap_frame(&vd_layer_vpp[vpp_id], vd_layer_vpp[vpp_id].dispbuf);
		need_disable_vd2 = false;
	} else if (new_frame) {
		pip_swap_frame(&vd_layer_vpp[vpp_id], new_frame);
		need_disable_vd2 = false;
	}
	vd_layer_vpp[vpp_id].keep_frame_id = 0xff;

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
	amvecm_on_vs
		(!is_local_vf(vd_layer_vpp[vpp_id].dispbuf)
		? vd_layer_vpp[vpp_id].dispbuf : NULL,
		new_frame,
		new_frame ? CSC_FLAG_TOGGLE_FRAME : 0,
		curpip_frame_par ?
		curpip_frame_par->supsc1_hori_ratio :
		0,
		curpip_frame_par ?
		curpip_frame_par->supsc1_vert_ratio :
		0,
		curpip_frame_par ?
		curpip_frame_par->spsc1_w_in :
		0,
		curpip_frame_par ?
		curpip_frame_par->spsc1_h_in :
		0,
		curpip_frame_par ?
		curpip_frame_par->cm_input_w :
		0,
		curpip_frame_par ?
		curpip_frame_par->cm_input_h :
		0,
		VD2_PATH);
#endif

	if (need_disable_vd2)
		safe_switch_videolayer(layer_id, false, true);

	/* filter setting management */
	pip_render_frame(&vd_layer_vpp[vpp_id]);
	//video_secure_set();

	if (vd_layer_vpp[vpp_id].dispbuf &&
	    (vd_layer_vpp[vpp_id].dispbuf->flag & VFRAME_FLAG_FAKE_FRAME))
		safe_switch_videolayer(layer_id, false, true);

	/* all frames has been renderred, so reset new frame flag */
	vd_layer_vpp[vpp_id].new_frame = false;
exit:
	vppx_blend_update(vinfo, VPP1);
	if (gvideo_recv[recv_id])
		gvideo_recv[recv_id]->func->late_proc(gvideo_recv[recv_id]);

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	//pip_rdma_buf = cur_pipbuf;
	//pip2_rdma_buf = cur_pipbuf2;
	/* vsync_rdma_config(); */
	vsync_rdma_process();
	rdma_enable_pre = is_vsync_rdma_enable();
#endif

	enc_line = get_cur_enc_line();
	if (enc_line > vsync_exit_line_max_vpp[vpp_id])
		vsync_exit_line_max_vpp[vpp_id] = enc_line;
	if (video_suspend)
		video_suspend_cycle++;
	//vpu_work_process();
	cur_vd2_path_id = vd2_path_id;

	return IRQ_HANDLED;
}

irqreturn_t vsync_isr_viu3(int irq, void *dev_id)
{
	return IRQ_HANDLED;
}
