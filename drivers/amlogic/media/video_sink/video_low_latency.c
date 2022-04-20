// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/sched.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include "video_priv.h"
#include "video_reg.h"
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/registers/register.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "videolog.h"
#include "video_reg.h"
#ifdef CONFIG_AM_VIDEO_LOG
#define AMLOG
#endif
#include <linux/amlogic/media/utils/amlog.h>
MODULE_AMLOG(LOG_LEVEL_ERROR, 0, LOG_DEFAULT_LEVEL_DESC, LOG_MASK_DESC);

#include <linux/amlogic/media/video_sink/vpp.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#include "../common/rdma/rdma.h"
#endif
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include "../common/vfm/vfm.h"
#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
#include <linux/amlogic/media/vpu_secure/vpu_secure.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
#include "linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h"
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_PRIME_SL
#include <linux/amlogic/media/amprime_sl/prime_sl.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_MSYNC
#include <uapi/linux/amlogic/msync.h>
#endif
#include <linux/amlogic/gki_module.h>

#include <linux/math64.h>
#include "video_receiver.h"
#include "video_low_latency.h"

static u32 lowlatency_proc_drop;
static u32 lowlatency_err_drop;
static u32 lowlatency_proc_frame_cnt;
static u32 lowlatency_skip_frame_cnt;
static u32 lowlatency_proc_done;
static u32 lowlatency_overflow_cnt;
static u32 lowlatency_overrun_cnt;
u32 lowlatency_overrun_recovery_cnt;
static u32 lowlatency_max_proc_lines;
static u32 lowlatency_max_enter_lines;
static u32 lowlatency_max_exit_lines;
static u32 lowlatency_min_enter_lines = 0xffffffff;
static u32 lowlatency_min_exit_lines = 0xffffffff;
static u32 vsync_proc_done;
static u32 line_threshold = 10;
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
static bool first_irq = true;
#endif
/* vout */
static const struct vinfo_s *vinfo;
static bool over_sync;

u32 vsync_proc_drop;
ulong lowlatency_vsync_count;
bool overrun_flag;

static int lowlatency_vsync(u8 instance_id)
{
	s32 vout_type;
	struct vframe_s *vf = NULL;
	struct vframe_s *path0_new_frame = NULL;
	struct vframe_s *path1_new_frame = NULL;
	struct vframe_s *path2_new_frame = NULL;
	struct vframe_s *path3_new_frame = NULL;
	struct vframe_s *path4_new_frame = NULL;
	struct vframe_s *path5_new_frame = NULL;

	static s32 cur_vd1_path_id = VFM_PATH_INVALID;
	static s32 cur_vd2_path_id = VFM_PATH_INVALID;
	static s32 cur_vd3_path_id = VFM_PATH_INVALID;
	s32 vd1_path_id = glayer_info[0].display_path_id;
	s32 vd2_path_id = glayer_info[1].display_path_id;
	s32 vd3_path_id = glayer_info[2].display_path_id;
	struct vframe_s *new_frame = NULL;
	struct vframe_s *new_frame2 = NULL;
	struct vframe_s *new_frame3 = NULL;
	u32 cur_blackout;
	enum vframe_signal_fmt_e fmt;
	int pq_process_debug[4];
	int axis[4];
	int crop[4];
	int source_type = 0;
	u32 next_afbc_request = atomic_read(&gafbc_request);
	struct path_id_s path_id;
	int i;

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	set_vsync_rdma_id(EX_VSYNC_RDMA);
#endif

	if (debug_flag & DEBUG_FLAG_VSYNC_DONONE)
		return IRQ_HANDLED;
	path_id.vd1_path_id = vd1_path_id;
	path_id.vd2_path_id = vd2_path_id;
	path_id.vd3_path_id = vd3_path_id;

#ifdef CONFIG_AMLOGIC_MEDIA_MSYNC
	msync_vsync_update();
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (is_dolby_vision_on())
		dolby_vision_update_backlight();
#endif

	if (cur_vd1_path_id == 0xff)
		cur_vd1_path_id = vd1_path_id;
	if (cur_vd2_path_id == 0xff)
		cur_vd2_path_id = vd2_path_id;
	if (cur_vd3_path_id == 0xff)
		cur_vd3_path_id = vd3_path_id;

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	/* Just a workaround to enable RDMA without any register config.
	 * Becasuse rdma is enabled after first rdma config.
	 * Previously, it will write register directly and
	 * maybe out of blanking in first irq.
	 */
	if (first_irq) {
		first_irq = false;
		goto RUN_FIRST_RDMA;
	}
#endif

	vout_type = detect_vout_type(vinfo);

	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		glayer_info[0].need_no_compress =
			(next_afbc_request & (i + 1)) ? true : false;
		vd_layer[i].bypass_pps = bypass_pps;
		vd_layer[i].global_debug = debug_flag;
		vd_layer[i].vout_type = vout_type;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	vsync_rdma_config_pre();
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	/* check video frame before VECM process */
	if (is_dolby_vision_enable() && vf &&
		(vd1_path_id == VFM_PATH_AMVIDEO ||
		 vd1_path_id == VFM_PATH_DEF ||
		 vd1_path_id == VFM_PATH_AUTO)) {
		dolby_vision_check_mvc(vf);
		dolby_vision_check_hdr10(vf);
		dolby_vision_check_hdr10plus(vf);
		dolby_vision_check_hlg(vf);
		dolby_vision_check_primesl(vf);
	}

	if (cur_vd1_path_id != vd1_path_id) {
		char *provider_name = NULL;

		/* FIXME: add more receiver check */
		if (vd1_path_id == VFM_PATH_PIP) {
			provider_name = vf_get_provider_name("videopip");
			while (provider_name) {
				if (!vf_get_provider_name(provider_name))
					break;
				provider_name =
					vf_get_provider_name(provider_name);
			}
			if (provider_name)
				dolby_vision_set_provider(provider_name);
		} else {
			dolby_vision_set_provider("dvbldec");
		}
	}
#endif

	if (atomic_read(&video_unreg_flag))
		goto exit;

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	if (is_vsync_rdma_enable()) {
		vd_layer[0].cur_canvas_id = vd_layer[0].next_canvas_id;
		vd_layer[1].cur_canvas_id = vd_layer[1].next_canvas_id;
		vd_layer[2].cur_canvas_id = vd_layer[2].next_canvas_id;
	} else {
		if (rdma_enable_pre)
			goto exit;

		vd_layer[0].cur_canvas_id = 0;
		vd_layer[0].next_canvas_id = 1;
		vd_layer[1].cur_canvas_id = 0;
		vd_layer[1].next_canvas_id = 1;
		vd_layer[2].cur_canvas_id = 0;
		vd_layer[2].next_canvas_id = 1;
	}
#endif

	if (gvideo_recv[0]) {
		gvideo_recv[0]->irq_mode = false;
		gvideo_recv[0]->func->early_proc(gvideo_recv[0],
						 over_sync ? 1 : 0);
	}
	if (gvideo_recv[1]) {
		gvideo_recv[1]->irq_mode = false;
		gvideo_recv[1]->func->early_proc(gvideo_recv[1],
						 over_sync ? 1 : 0);
	}
	if (gvideo_recv[2]) {
		gvideo_recv[2]->irq_mode = false;
		gvideo_recv[2]->func->early_proc(gvideo_recv[2],
						 over_sync ? 1 : 0);
	}

	/* video_render.0 toggle frame */
	if (gvideo_recv[0]) {
		u32 frame_cnt = 0;
		struct vframe_s *tmp_vf = NULL;

		do {
			tmp_vf =
				gvideo_recv[0]->func->dequeue_frame(gvideo_recv[0], &path_id);
			if (tmp_vf) {
				lowlatency_proc_frame_cnt++;
				if (path3_new_frame)
					frame_cnt++;
				path3_new_frame = tmp_vf;
			}
		} while (tmp_vf);

		lowlatency_skip_frame_cnt += frame_cnt;
		if (path3_new_frame &&
			tvin_vf_is_keeped(path3_new_frame)) {
			new_frame_count = 0;
		} else if (path3_new_frame) {
			new_frame_count = gvideo_recv[0]->frame_count;
			hdmi_in_delay_maxmin_new(path3_new_frame);
		} else if (gvideo_recv[0]->cur_buf) {
			if (tvin_vf_is_keeped(gvideo_recv[0]->cur_buf))
				new_frame_count = 0;
		}
#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
		if (vd1_path_id == VFM_PATH_VIDEO_RENDER0 &&
			cur_frame_par) {
			/*need call every vsync*/
			if (path3_new_frame)
				frame_lock_process(path3_new_frame,
					cur_frame_par);
			else if (vd_layer[0].dispbuf)
				frame_lock_process(vd_layer[0].dispbuf,
					cur_frame_par);
			else
				frame_lock_process(NULL, cur_frame_par);
		}

		if (vd1_path_id == gvideo_recv[0]->path_id) {
			amvecm_on_vs((gvideo_recv[0]->cur_buf !=
				 &gvideo_recv[0]->local_buf)
				? gvideo_recv[0]->cur_buf : NULL,
				path3_new_frame,
				CSC_FLAG_CHECK_OUTPUT,
				0,
				0,
				0,
				0,
				0,
				0,
				VD1_PATH,
				VPP_TOP0);
		}
		if (vd2_path_id == gvideo_recv[0]->path_id)
			amvecm_on_vs((gvideo_recv[0]->cur_buf !=
				 &gvideo_recv[0]->local_buf)
				? gvideo_recv[0]->cur_buf : NULL,
				path3_new_frame,
				CSC_FLAG_CHECK_OUTPUT,
				0,
				0,
				0,
				0,
				0,
				0,
				VD2_PATH,
				VPP_TOP0);
		if (vd3_path_id == gvideo_recv[0]->path_id)
			amvecm_on_vs((gvideo_recv[0]->cur_buf !=
				 &gvideo_recv[0]->local_buf)
				? gvideo_recv[0]->cur_buf : NULL,
				path3_new_frame,
				CSC_FLAG_CHECK_OUTPUT,
				0,
				0,
				0,
				0,
				0,
				0,
				VD3_PATH,
				VPP_TOP0);
#endif
	}

	/* video_render.1 toggle frame */
	if (gvideo_recv[1]) {
		struct vframe_s *tmp_vf = NULL;

		do {
			tmp_vf =
				gvideo_recv[1]->func->dequeue_frame(gvideo_recv[1],
					&path_id);
			if (tmp_vf)
				path4_new_frame = tmp_vf;
		} while (tmp_vf);

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
		if (vd1_path_id == gvideo_recv[1]->path_id) {
			amvecm_on_vs((gvideo_recv[1]->cur_buf !=
				 &gvideo_recv[1]->local_buf)
				? gvideo_recv[1]->cur_buf : NULL,
				path4_new_frame,
				CSC_FLAG_CHECK_OUTPUT,
				0,
				0,
				0,
				0,
				0,
				0,
				VD1_PATH,
				VPP_TOP0);
		}
		if (vd2_path_id == gvideo_recv[1]->path_id)
			amvecm_on_vs((gvideo_recv[1]->cur_buf !=
				 &gvideo_recv[1]->local_buf)
				? gvideo_recv[1]->cur_buf : NULL,
				path4_new_frame,
				CSC_FLAG_CHECK_OUTPUT,
				0,
				0,
				0,
				0,
				0,
				0,
				VD2_PATH,
				VPP_TOP0);
		if (vd3_path_id == gvideo_recv[1]->path_id)
			amvecm_on_vs((gvideo_recv[1]->cur_buf !=
				 &gvideo_recv[1]->local_buf)
				? gvideo_recv[1]->cur_buf : NULL,
				path4_new_frame,
				CSC_FLAG_CHECK_OUTPUT,
				0,
				0,
				0,
				0,
				0,
				0,
				VD3_PATH,
				VPP_TOP0);
#endif
	}

	/* video_render.2 toggle frame */
	if (gvideo_recv[2]) {
		struct vframe_s *tmp_vf = NULL;

		do {
			tmp_vf =
				gvideo_recv[2]->func->dequeue_frame(gvideo_recv[2],
					&path_id);
			if (tmp_vf)
				path5_new_frame = tmp_vf;
		} while (tmp_vf);

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
	if (vd1_path_id == gvideo_recv[2]->path_id) {
		amvecm_on_vs((gvideo_recv[2]->cur_buf !=
			 &gvideo_recv[2]->local_buf)
			? gvideo_recv[2]->cur_buf : NULL,
			path5_new_frame,
			CSC_FLAG_CHECK_OUTPUT,
			0,
			0,
			0,
			0,
			0,
			0,
			VD1_PATH,
			VPP_TOP0);
	}
	if (vd2_path_id == gvideo_recv[2]->path_id)
		amvecm_on_vs((gvideo_recv[2]->cur_buf !=
			 &gvideo_recv[2]->local_buf)
			? gvideo_recv[2]->cur_buf : NULL,
			path5_new_frame,
			CSC_FLAG_CHECK_OUTPUT,
			0,
			0,
			0,
			0,
			0,
			0,
			VD2_PATH,
			VPP_TOP0);
	if (vd3_path_id == gvideo_recv[2]->path_id)
		amvecm_on_vs((gvideo_recv[2]->cur_buf !=
			 &gvideo_recv[2]->local_buf)
			? gvideo_recv[2]->cur_buf : NULL,
			path5_new_frame,
			CSC_FLAG_CHECK_OUTPUT,
			0,
			0,
			0,
			0,
			0,
			0,
			VD3_PATH,
			VPP_TOP0);
#endif
	}
	/* FIXME: if need enable for vd1 */
#ifdef CHECK_LATER
	if (!vd_layer[0].global_output) {
		cur_vd1_path_id = VFM_PATH_INVALID;
		vd1_path_id = VFM_PATH_INVALID;
	}
#endif

	if (!vd_layer[1].global_output) {
		cur_vd2_path_id = VFM_PATH_INVALID;
		vd2_path_id = VFM_PATH_INVALID;
	}
	if (!vd_layer[2].global_output) {
		cur_vd3_path_id = VFM_PATH_INVALID;
		vd3_path_id = VFM_PATH_INVALID;
	}

	if ((cur_vd1_path_id != vd1_path_id ||
	     cur_vd2_path_id != vd2_path_id ||
	     cur_vd3_path_id != vd3_path_id) &&
	    (debug_flag & DEBUG_FLAG_PRINT_PATH_SWITCH)) {
		pr_info("VID: === before path switch ===\n");
		pr_info("VID: \tcur_path_id: %d, %d, %d;\nVID: \tnew_path_id: %d, %d, %d;\nVID: \ttoggle:%p, %p, %p %p, %p, %p\nVID: \tcur:%p, %p, %p, %p, %p, %p;\n",
			cur_vd1_path_id, cur_vd2_path_id, cur_vd3_path_id,
			vd1_path_id, vd2_path_id, vd3_path_id,
			path0_new_frame, path1_new_frame,
			path2_new_frame, path3_new_frame,
			path4_new_frame, path4_new_frame,
			cur_dispbuf, cur_pipbuf, cur_pipbuf2,
			gvideo_recv[0] ? gvideo_recv[0]->cur_buf : NULL,
			gvideo_recv[1] ? gvideo_recv[1]->cur_buf : NULL,
			gvideo_recv[2] ? gvideo_recv[2]->cur_buf : NULL);
		pr_info("VID: \tdispbuf:%p, %p, %p; \tvf_ext:%p, %p, %p;\nVID: \tlocal:%p, %p, %p, %p, %p, %p\n",
			vd_layer[0].dispbuf, vd_layer[1].dispbuf, vd_layer[2].dispbuf,
			vd_layer[0].vf_ext, vd_layer[1].vf_ext, vd_layer[2].vf_ext,
			&vf_local, &local_pip, &local_pip2,
			gvideo_recv[0] ? &gvideo_recv[0]->local_buf : NULL,
			gvideo_recv[1] ? &gvideo_recv[1]->local_buf : NULL,
			gvideo_recv[2] ? &gvideo_recv[2]->local_buf : NULL);
		pr_info("VID: \tblackout:%d %d, %d force:%d;\n",
			blackout, blackout_pip, blackout_pip2, force_blackout);
	}
	if (debug_flag & DEBUG_FLAG_PRINT_DISBUF_PER_VSYNC)
		pr_info("VID: path id: %d, %d, %d; new_frame:%p, %p, %p, %p, %p, %p cur:%p, %p, %p, %p, %p, %p; vd dispbuf:%p, %p, %p; vf_ext:%p, %p, %p; local:%p, %p, %p, %p, %p, %p\n",
			vd1_path_id, vd2_path_id, vd2_path_id,
			path0_new_frame, path1_new_frame,
			path2_new_frame, path3_new_frame,
			path4_new_frame, path5_new_frame,
			cur_dispbuf, cur_pipbuf, cur_pipbuf2,
			gvideo_recv[0] ? gvideo_recv[0]->cur_buf : NULL,
			gvideo_recv[1] ? gvideo_recv[1]->cur_buf : NULL,
			gvideo_recv[2] ? gvideo_recv[2]->cur_buf : NULL,
			vd_layer[0].dispbuf, vd_layer[1].dispbuf, vd_layer[2].dispbuf,
			vd_layer[0].vf_ext, vd_layer[1].vf_ext, vd_layer[2].vf_ext,
			&vf_local, &local_pip, &local_pip2,
			gvideo_recv[0] ? &gvideo_recv[0]->local_buf : NULL,
			gvideo_recv[1] ? &gvideo_recv[1]->local_buf : NULL,
			gvideo_recv[2] ? &gvideo_recv[2]->local_buf : NULL);

	if (vd_layer[0].dispbuf_mapping == &cur_dispbuf &&
	    (cur_dispbuf == &vf_local ||
	     !cur_dispbuf) &&
	    vd_layer[0].dispbuf != cur_dispbuf)
		vd_layer[0].dispbuf = cur_dispbuf;

	if (vd_layer[0].dispbuf_mapping == &cur_pipbuf &&
	    (cur_pipbuf == &local_pip ||
	     !cur_pipbuf) &&
	    vd_layer[0].dispbuf != cur_pipbuf)
		vd_layer[0].dispbuf = cur_pipbuf;

	if (vd_layer[0].dispbuf_mapping == &cur_pipbuf2 &&
	    (cur_pipbuf2 == &local_pip2 ||
	     !cur_pipbuf2) &&
	    vd_layer[0].dispbuf != cur_pipbuf2)
		vd_layer[0].dispbuf = cur_pipbuf2;

	if (gvideo_recv[0] &&
	    vd_layer[0].dispbuf_mapping == &gvideo_recv[0]->cur_buf &&
	    (gvideo_recv[0]->cur_buf == &gvideo_recv[0]->local_buf ||
	     !gvideo_recv[0]->cur_buf) &&
	    vd_layer[0].dispbuf != gvideo_recv[0]->cur_buf)
		vd_layer[0].dispbuf = gvideo_recv[0]->cur_buf;

	if (gvideo_recv[1] &&
	    vd_layer[0].dispbuf_mapping == &gvideo_recv[1]->cur_buf &&
	    (gvideo_recv[1]->cur_buf == &gvideo_recv[1]->local_buf ||
	     !gvideo_recv[1]->cur_buf) &&
	    vd_layer[0].dispbuf != gvideo_recv[1]->cur_buf)
		vd_layer[0].dispbuf = gvideo_recv[1]->cur_buf;

	if (gvideo_recv[2] &&
	    vd_layer[0].dispbuf_mapping == &gvideo_recv[2]->cur_buf &&
	    (gvideo_recv[2]->cur_buf == &gvideo_recv[2]->local_buf ||
	     !gvideo_recv[2]->cur_buf) &&
	    vd_layer[0].dispbuf != gvideo_recv[2]->cur_buf)
		vd_layer[0].dispbuf = gvideo_recv[2]->cur_buf;

	/* vd1 config */
	if (gvideo_recv[0] &&
	    gvideo_recv[0]->path_id == vd1_path_id) {
		/* video_render.0 display on VD1 */
		new_frame = path3_new_frame;
		if (!new_frame) {
			if (!gvideo_recv[0]->cur_buf) {
				/* video_render.0 no frame in display */
				if (cur_vd1_path_id != vd1_path_id)
					safe_switch_videolayer(0, false, true);
				vd_layer[0].dispbuf = NULL;
			} else if (gvideo_recv[0]->cur_buf ==
				&gvideo_recv[0]->local_buf) {
				/* video_render.0 keep frame */
				vd_layer[0].dispbuf = gvideo_recv[0]->cur_buf;
			} else if (vd_layer[0].dispbuf
				!= gvideo_recv[0]->cur_buf) {
				/* video_render.0 has frame in display */
				new_frame = gvideo_recv[0]->cur_buf;
			}
		}
		if (new_frame || gvideo_recv[0]->cur_buf)
			vd_layer[0].dispbuf_mapping = &gvideo_recv[0]->cur_buf;
		cur_blackout = 1;
	} else if (gvideo_recv[1] &&
	    (gvideo_recv[1]->path_id == vd1_path_id)) {
		/* video_render.1 display on VD1 */
		new_frame = path4_new_frame;
		if (!new_frame) {
			if (!gvideo_recv[1]->cur_buf) {
				/* video_render.1 no frame in display */
				if (cur_vd1_path_id != vd1_path_id)
					safe_switch_videolayer(0, false, true);
				vd_layer[0].dispbuf = NULL;
			} else if (gvideo_recv[1]->cur_buf ==
				&gvideo_recv[1]->local_buf) {
				/* video_render.1 keep frame */
				vd_layer[0].dispbuf = gvideo_recv[1]->cur_buf;
			} else if (vd_layer[0].dispbuf
				!= gvideo_recv[1]->cur_buf) {
				/* video_render.1 has frame in display */
				new_frame = gvideo_recv[1]->cur_buf;
			}
		}
		if (new_frame || gvideo_recv[1]->cur_buf)
			vd_layer[0].dispbuf_mapping = &gvideo_recv[1]->cur_buf;
		cur_blackout = 1;
	} else if (gvideo_recv[2] &&
	    (gvideo_recv[2]->path_id == vd1_path_id)) {
		/* video_render.2 display on VD1 */
		new_frame = path5_new_frame;
		if (!new_frame) {
			if (!gvideo_recv[2]->cur_buf) {
				/* video_render.2 no frame in display */
				if (cur_vd1_path_id != vd1_path_id)
					safe_switch_videolayer(0, false, true);
				vd_layer[0].dispbuf = NULL;
			} else if (gvideo_recv[2]->cur_buf ==
				&gvideo_recv[2]->local_buf) {
				/* video_render.2 keep frame */
				vd_layer[0].dispbuf = gvideo_recv[2]->cur_buf;
			} else if (vd_layer[0].dispbuf
				!= gvideo_recv[2]->cur_buf) {
				/* video_render.2 has frame in display */
				new_frame = gvideo_recv[2]->cur_buf;
			}
		}
		if (new_frame || gvideo_recv[2]->cur_buf)
			vd_layer[0].dispbuf_mapping = &gvideo_recv[2]->cur_buf;
		cur_blackout = 1;
	} else if (vd1_path_id == VFM_PATH_PIP2) {
		/* pip2 display on VD1 */
		new_frame = path2_new_frame;
		if (!new_frame) {
			if (!cur_pipbuf2) {
				/* pip2 no frame in display */
				if (cur_vd1_path_id != vd1_path_id)
					safe_switch_videolayer(0, false, true);
				vd_layer[0].dispbuf = NULL;
			} else if (cur_pipbuf2 == &local_pip2) {
				/* pip2 keep frame */
				vd_layer[0].dispbuf = cur_pipbuf2;
			} else if (vd_layer[0].dispbuf
				!= cur_pipbuf2) {
				/* pip2 has frame in display */
				new_frame = cur_pipbuf2;
			}
		}
		if (new_frame || cur_pipbuf2)
			vd_layer[0].dispbuf_mapping = &cur_pipbuf2;
		cur_blackout = blackout_pip2 | force_blackout;
	} else if (vd1_path_id == VFM_PATH_PIP) {
		/* pip display on VD1 */
		new_frame = path1_new_frame;
		if (!new_frame) {
			if (!cur_pipbuf) {
				/* pip no frame in display */
				if (cur_vd1_path_id != vd1_path_id)
					safe_switch_videolayer(0, false, true);
				vd_layer[0].dispbuf = NULL;
			} else if (cur_pipbuf == &local_pip) {
				/* pip keep frame */
				vd_layer[0].dispbuf = cur_pipbuf;
			} else if (vd_layer[0].dispbuf
				!= cur_pipbuf) {
				/* pip has frame in display */
				new_frame = cur_pipbuf;
			}
		}
		if (new_frame || cur_pipbuf)
			vd_layer[0].dispbuf_mapping = &cur_pipbuf;
		cur_blackout = blackout_pip | force_blackout;
	} else if ((vd1_path_id != VFM_PATH_INVALID) &&
		   (vd1_path_id != VFM_PATH_AUTO)) {
		/* priamry display on VD1 */
		new_frame = path0_new_frame;
		if (!new_frame) {
			if (!cur_dispbuf) {
				/* priamry no frame in display */
				if (cur_vd1_path_id != vd1_path_id)
					safe_switch_videolayer(0, false, true);
				vd_layer[0].dispbuf = NULL;
			} else if (cur_dispbuf == &vf_local) {
				/* priamry keep frame */
				vd_layer[0].dispbuf = cur_dispbuf;
			} else if (vd_layer[0].dispbuf
				!= cur_dispbuf) {
				/* primary has frame in display */
				new_frame = cur_dispbuf;
			}
		}
		if (new_frame || cur_dispbuf)
			vd_layer[0].dispbuf_mapping = &cur_dispbuf;
		cur_blackout = blackout | force_blackout;
	} else if (vd1_path_id == VFM_PATH_AUTO) {
		new_frame = path0_new_frame;

		if (path3_new_frame &&
			(path3_new_frame->flag & VFRAME_FLAG_FAKE_FRAME)) {
			new_frame = path3_new_frame;
			pr_info("vsync: auto path2 get a fake\n");
		}

		if (!new_frame) {
			if (cur_dispbuf == &vf_local)
				vd_layer[0].dispbuf = cur_dispbuf;
		}

		if (gvideo_recv[0]->cur_buf &&
			gvideo_recv[0]->cur_buf->flag & VFRAME_FLAG_FAKE_FRAME)
			vd_layer[0].dispbuf = gvideo_recv[0]->cur_buf;

		if (new_frame || cur_dispbuf)
			vd_layer[0].dispbuf_mapping = &cur_dispbuf;
		cur_blackout = blackout | force_blackout;
	} else {
		cur_blackout = 1;
	}

	/* vout mode detection under new non-tunnel mode */
	if (vd_layer[0].dispbuf || vd_layer[1].dispbuf ||
	   vd_layer[2].dispbuf) {
		if (strcmp(old_vmode, new_vmode)) {
			vd_layer[0].property_changed = true;
			vd_layer[1].property_changed = true;
			vd_layer[2].property_changed = true;
			pr_info("detect vout mode change!!!!!!!!!!!!\n");
			strcpy(old_vmode, new_vmode);
		}
	}

	if (debug_flag & DEBUG_FLAG_PRINT_DISBUF_PER_VSYNC)
		pr_info("VID: layer enable status: VD1:e:%d,e_save:%d,g:%d,d:%d,f:%s; VD2:e:%d,e_save:%d,g:%d,d:%d,f:%s; VD3:e:%d,e_save:%d,g:%d,d:%d,f:%s",
			vd_layer[0].enabled, vd_layer[0].enabled_status_saved,
			vd_layer[0].global_output, vd_layer[0].disable_video,
			vd_layer[0].force_disable ? "true" : "false",
			vd_layer[1].enabled, vd_layer[1].enabled_status_saved,
			vd_layer[1].global_output, vd_layer[1].disable_video,
			vd_layer[1].force_disable ? "true" : "false",
			vd_layer[2].enabled, vd_layer[2].enabled_status_saved,
			vd_layer[2].global_output, vd_layer[2].disable_video,
			vd_layer[2].force_disable ? "true" : "false");

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (is_dolby_vision_enable() && vd_layer[0].global_output) {
		/* no new frame but path switched case, */
		if (new_frame && !is_local_vf(new_frame) &&
		    (!path0_new_frame || new_frame != path0_new_frame) &&
		    (!path1_new_frame || new_frame != path1_new_frame) &&
		    (!path2_new_frame || new_frame != path2_new_frame) &&
		    (!path3_new_frame || new_frame != path3_new_frame) &&
		    (!path4_new_frame || new_frame != path4_new_frame) &&
		    (!path5_new_frame || new_frame != path5_new_frame))
			dolby_vision_update_src_format(new_frame, 1);
		else if (!new_frame &&
			 vd_layer[0].dispbuf &&
			 !is_local_vf(vd_layer[0].dispbuf))
			dolby_vision_update_src_format(vd_layer[0].dispbuf, 0);
		/* pause and video off->on case */
	}
#endif

	if (!new_frame && vd_layer[0].dispbuf &&
	    is_local_vf(vd_layer[0].dispbuf)) {
		if (cur_blackout) {
			vd_layer[0].property_changed = false;
		} else if (!is_di_post_mode(vd_layer[0].dispbuf)) {
			if (vd_layer[0].switch_vf && vd_layer[0].vf_ext)
				vd_layer[0].vf_ext->canvas0Addr =
					get_layer_display_canvas(0);
			else
				vd_layer[0].dispbuf->canvas0Addr =
					get_layer_display_canvas(0);
		}
	}

	if (vd_layer[0].dispbuf &&
	    (vd_layer[0].dispbuf->flag & (VFRAME_FLAG_VIDEO_COMPOSER |
		VFRAME_FLAG_VIDEO_DRM)) &&
	    !(vd_layer[0].dispbuf->flag & VFRAME_FLAG_FAKE_FRAME) &&
	    !(debug_flag & DEBUG_FLAG_AXIS_NO_UPDATE)) {
		int mirror = 0;

		axis[0] = vd_layer[0].dispbuf->axis[0];
		axis[1] = vd_layer[0].dispbuf->axis[1];
		axis[2] = vd_layer[0].dispbuf->axis[2];
		axis[3] = vd_layer[0].dispbuf->axis[3];
		crop[0] = vd_layer[0].dispbuf->crop[0];
		crop[1] = vd_layer[0].dispbuf->crop[1];
		crop[2] = vd_layer[0].dispbuf->crop[2];
		crop[3] = vd_layer[0].dispbuf->crop[3];
		_set_video_window(&glayer_info[0], axis);
		source_type = vd_layer[0].dispbuf->source_type;
		if (source_type != VFRAME_SOURCE_TYPE_HDMI &&
			source_type != VFRAME_SOURCE_TYPE_CVBS &&
			source_type != VFRAME_SOURCE_TYPE_TUNER &&
			source_type != VFRAME_SOURCE_TYPE_HWC)
			_set_video_crop(&glayer_info[0], crop);
		if (vd_layer[0].dispbuf->flag & VFRAME_FLAG_MIRROR_H)
			mirror = H_MIRROR;
		if (vd_layer[0].dispbuf->flag & VFRAME_FLAG_MIRROR_V)
			mirror = V_MIRROR;
		_set_video_mirror(&glayer_info[0], mirror);
		set_alpha_scpxn(&vd_layer[0], vd_layer[0].dispbuf->componser_info);
		glayer_info[0].zorder = vd_layer[0].dispbuf->zorder;
	}

	/* setting video display property in underflow mode */
	if (!new_frame &&
		vd_layer[0].dispbuf &&
		(vd_layer[0].property_changed ||
		 is_picmode_changed(0, vd_layer[0].dispbuf))) {
		primary_swap_frame(&vd_layer[0], vd_layer[0].dispbuf, __LINE__);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		dvel_swap_frame(cur_dispbuf2);
#endif
	} else if (new_frame) {
		vframe_walk_delay = (int)div_u64(((jiffies_64 -
			new_frame->ready_jiffies64) * 1000), HZ);
		vframe_walk_delay += 1000 *
			vsync_pts_inc_scale / vsync_pts_inc_scale_base;
		vframe_walk_delay -= new_frame->duration / 96;
#ifdef CONFIG_AMLOGIC_MEDIA_FRC
		vframe_walk_delay += frc_get_video_latency();
#endif
		primary_swap_frame(&vd_layer[0], new_frame, __LINE__);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		dvel_swap_frame(cur_dispbuf2);
#endif
	}

	/* setting video display property in underflow mode */
	if (!new_frame &&
		vd_layer[0].dispbuf &&
		(vd_layer[0].property_changed ||
		 is_picmode_changed(0, vd_layer[0].dispbuf))) {
		primary_swap_frame(&vd_layer[0], vd_layer[0].dispbuf, __LINE__);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		dvel_swap_frame(cur_dispbuf2);
#endif
	} else if (new_frame) {
		vframe_walk_delay = (int)div_u64(((jiffies_64 -
			new_frame->ready_jiffies64) * 1000), HZ);
		vframe_walk_delay += 1000 *
			vsync_pts_inc_scale / vsync_pts_inc_scale_base;
		vframe_walk_delay -= new_frame->duration / 96;
#ifdef CONFIG_AMLOGIC_MEDIA_FRC
		vframe_walk_delay += frc_get_video_latency();
#endif
		primary_swap_frame(&vd_layer[0], new_frame, __LINE__);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		dvel_swap_frame(cur_dispbuf2);
#endif
	}
#if defined(CONFIG_AMLOGIC_MEDIA_FRC)
	frc_input_handle(vd_layer[0].dispbuf, cur_frame_par);
#endif
	if (atomic_read(&axis_changed)) {
		video_prop_status |= VIDEO_PROP_CHANGE_AXIS;
		atomic_set(&axis_changed, 0);
	}

	if (vd1_path_id == VFM_PATH_AMVIDEO ||
	    vd1_path_id == VFM_PATH_DEF)
		vd_layer[0].keep_frame_id = 0;
	else if (vd1_path_id == VFM_PATH_PIP)
		vd_layer[0].keep_frame_id = 1;
	else if (vd1_path_id == VFM_PATH_PIP2)
		vd_layer[0].keep_frame_id = 2;
	else
		vd_layer[0].keep_frame_id = 0xff;

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
	refresh_on_vs(new_frame, vd_layer[0].dispbuf);

	amvecm_on_vs
		(!is_local_vf(vd_layer[0].dispbuf)
		? vd_layer[0].dispbuf : NULL,
		new_frame,
		new_frame ? CSC_FLAG_TOGGLE_FRAME : 0,
		cur_frame_par ?
		cur_frame_par->supsc1_hori_ratio :
		0,
		cur_frame_par ?
		cur_frame_par->supsc1_vert_ratio :
		0,
		cur_frame_par ?
		cur_frame_par->spsc1_w_in :
		0,
		cur_frame_par ?
		cur_frame_par->spsc1_h_in :
		0,
		cur_frame_par ?
		cur_frame_par->cm_input_w :
		0,
		cur_frame_par ?
		cur_frame_par->cm_input_h :
		0,
		VD1_PATH,
		VPP_TOP0);
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_PRIME_SL
	prime_sl_process(vd_layer[0].dispbuf);
#endif

	/* work around which dec/vdin don't call update src_fmt function */
	if (vd_layer[0].dispbuf && !is_local_vf(vd_layer[0].dispbuf)) {
		int new_src_fmt = -1;
		u32 src_map[] = {
			VFRAME_SIGNAL_FMT_INVALID,
			VFRAME_SIGNAL_FMT_HDR10,
			VFRAME_SIGNAL_FMT_HDR10PLUS,
			VFRAME_SIGNAL_FMT_DOVI,
			VFRAME_SIGNAL_FMT_HDR10PRIME,
			VFRAME_SIGNAL_FMT_HLG,
			VFRAME_SIGNAL_FMT_SDR,
			VFRAME_SIGNAL_FMT_MVC
		};
#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (is_dolby_vision_enable())
			new_src_fmt = get_dolby_vision_src_format();
		else
#endif
			new_src_fmt =
				(int)get_cur_source_type(VD1_PATH, VPP_TOP0);
#endif
		if (new_src_fmt > 0 && new_src_fmt < 8)
			fmt = (enum vframe_signal_fmt_e)src_map[new_src_fmt];
		else
			fmt = VFRAME_SIGNAL_FMT_INVALID;
		if (fmt != atomic_read(&cur_primary_src_fmt)) {
			/* atomic_set(&primary_src_fmt, fmt); */
			if (debug_flag & DEBUG_FLAG_TRACE_EVENT) {
				char *old_str = NULL, *new_str = NULL;
				enum vframe_signal_fmt_e old_fmt;

				old_fmt = (enum vframe_signal_fmt_e)
					atomic_read(&cur_primary_src_fmt);
				if (old_fmt != VFRAME_SIGNAL_FMT_INVALID)
					old_str = (char *)src_fmt_str[old_fmt];
				if (fmt != VFRAME_SIGNAL_FMT_INVALID)
					new_str = (char *)src_fmt_str[fmt];
				pr_info("VD1 src fmt changed: %s->%s. vf: %p, signal_type:0x%x\n",
					old_str ? old_str : "invalid",
					new_str ? new_str : "invalid",
					vd_layer[0].dispbuf,
					vd_layer[0].dispbuf->signal_type);
			}
			atomic_set(&cur_primary_src_fmt, fmt);
			atomic_set(&primary_src_fmt, fmt);
			video_prop_status |= VIDEO_PROP_CHANGE_FMT;
		}
	}

	if (vd_layer[1].dispbuf_mapping == &cur_dispbuf &&
	    (cur_dispbuf == &vf_local ||
	     !cur_dispbuf) &&
	    vd_layer[1].dispbuf != cur_dispbuf)
		vd_layer[1].dispbuf = cur_dispbuf;

	if (vd_layer[1].dispbuf_mapping == &cur_pipbuf &&
	    (cur_pipbuf == &local_pip ||
	     !cur_pipbuf) &&
	    vd_layer[1].dispbuf != cur_pipbuf)
		vd_layer[1].dispbuf = cur_pipbuf;

	if (vd_layer[1].dispbuf_mapping == &cur_pipbuf2 &&
	    (cur_pipbuf2 == &local_pip2 ||
	     !cur_pipbuf2) &&
	    vd_layer[1].dispbuf != cur_pipbuf2)
		vd_layer[1].dispbuf = cur_pipbuf2;

	if (gvideo_recv[0] &&
	    vd_layer[1].dispbuf_mapping == &gvideo_recv[0]->cur_buf &&
	    (gvideo_recv[0]->cur_buf == &gvideo_recv[0]->local_buf ||
	     !gvideo_recv[0]->cur_buf) &&
	    vd_layer[1].dispbuf != gvideo_recv[0]->cur_buf)
		vd_layer[1].dispbuf = gvideo_recv[0]->cur_buf;

	if (gvideo_recv[1] &&
	    vd_layer[1].dispbuf_mapping == &gvideo_recv[1]->cur_buf &&
	    (gvideo_recv[1]->cur_buf == &gvideo_recv[1]->local_buf ||
	     !gvideo_recv[1]->cur_buf) &&
	    vd_layer[1].dispbuf != gvideo_recv[1]->cur_buf)
		vd_layer[1].dispbuf = gvideo_recv[1]->cur_buf;

	if (gvideo_recv[2] &&
	    vd_layer[1].dispbuf_mapping == &gvideo_recv[2]->cur_buf &&
	    (gvideo_recv[2]->cur_buf == &gvideo_recv[2]->local_buf ||
	     !gvideo_recv[2]->cur_buf) &&
	    vd_layer[1].dispbuf != gvideo_recv[2]->cur_buf)
		vd_layer[1].dispbuf = gvideo_recv[2]->cur_buf;

	if (vd_layer[1].switch_vf &&
	    vd_layer[1].dispbuf &&
	    vd_layer[1].dispbuf->vf_ext)
		vd_layer[1].vf_ext =
			(struct vframe_s *)vd_layer[1].dispbuf->vf_ext;
	else
		vd_layer[1].vf_ext = NULL;

	/* vd2 config */
	if (gvideo_recv[0] &&
	    gvideo_recv[0]->path_id == vd2_path_id) {
		/* video_render.0 display on VD2 */
		new_frame2 = path3_new_frame;
		if (!new_frame2) {
			if (!gvideo_recv[0]->cur_buf) {
				/* video_render.0 no frame in display */
				if (cur_vd2_path_id != vd2_path_id)
					safe_switch_videolayer(1, false, true);
				vd_layer[1].dispbuf = NULL;
			} else if (gvideo_recv[0]->cur_buf ==
				&gvideo_recv[0]->local_buf) {
				/* video_render.0 keep frame */
				vd_layer[1].dispbuf = gvideo_recv[0]->cur_buf;
			} else if (vd_layer[1].dispbuf
				!= gvideo_recv[0]->cur_buf) {
				/* video_render.0 has frame in display */
				new_frame2 = gvideo_recv[0]->cur_buf;
			}
		}
		if (new_frame2 || gvideo_recv[0]->cur_buf)
			vd_layer[1].dispbuf_mapping = &gvideo_recv[0]->cur_buf;
		cur_blackout = 1;
	} else if (gvideo_recv[1] &&
	    (gvideo_recv[1]->path_id == vd2_path_id)) {
		/* video_render.1 display on VD2 */
		new_frame2 = path4_new_frame;
		if (!new_frame2) {
			if (!gvideo_recv[1]->cur_buf) {
				/* video_render.1 no frame in display */
				if (cur_vd2_path_id != vd2_path_id)
					safe_switch_videolayer(1, false, true);
				vd_layer[1].dispbuf = NULL;
			} else if (gvideo_recv[1]->cur_buf ==
				&gvideo_recv[1]->local_buf) {
				/* video_render.1 keep frame */
				vd_layer[1].dispbuf = gvideo_recv[1]->cur_buf;
			} else if (vd_layer[1].dispbuf
				!= gvideo_recv[1]->cur_buf) {
				/* video_render.1 has frame in display */
				new_frame2 = gvideo_recv[1]->cur_buf;
			}
		}
		if (new_frame2 || gvideo_recv[1]->cur_buf)
			vd_layer[1].dispbuf_mapping = &gvideo_recv[1]->cur_buf;
		cur_blackout = 1;
	} else if (gvideo_recv[2] &&
	    (gvideo_recv[2]->path_id == vd2_path_id)) {
		/* video_render.2 display on VD2 */
		new_frame2 = path5_new_frame;
		if (!new_frame2) {
			if (!gvideo_recv[2]->cur_buf) {
				/* video_render.2 no frame in display */
				if (cur_vd2_path_id != vd2_path_id)
					safe_switch_videolayer(1, false, true);
				vd_layer[1].dispbuf = NULL;
			} else if (gvideo_recv[2]->cur_buf ==
				&gvideo_recv[2]->local_buf) {
				/* video_render.2 keep frame */
				vd_layer[1].dispbuf = gvideo_recv[2]->cur_buf;
			} else if (vd_layer[1].dispbuf
				!= gvideo_recv[2]->cur_buf) {
				/* video_render.2 has frame in display */
				new_frame2 = gvideo_recv[2]->cur_buf;
			}
		}
		if (new_frame2 || gvideo_recv[2]->cur_buf)
			vd_layer[1].dispbuf_mapping = &gvideo_recv[2]->cur_buf;
		pr_info("new_frame2=%p\n", new_frame2);
		cur_blackout = 1;
	} else if (vd2_path_id == VFM_PATH_AMVIDEO) {
		/* priamry display in VD2 */
		new_frame2 = path0_new_frame;
		if (!new_frame2) {
			if (!cur_dispbuf) {
				/* primary no frame in display */
				if (cur_vd2_path_id != vd2_path_id)
					safe_switch_videolayer(1, false, true);
				vd_layer[1].dispbuf = NULL;
			} else if (cur_dispbuf == &vf_local) {
				/* priamry keep frame */
				vd_layer[1].dispbuf = cur_dispbuf;
			} else if (vd_layer[1].dispbuf
				!= cur_dispbuf) {
				new_frame2 = cur_dispbuf;
			}
		}
		if (new_frame2 || cur_dispbuf)
			vd_layer[1].dispbuf_mapping = &cur_dispbuf;
		cur_blackout = blackout | force_blackout;
	} else if (vd2_path_id == VFM_PATH_PIP2) {
		/* pip display in VD3 */
		new_frame2 = path2_new_frame;
		if (!new_frame2) {
			if (!cur_pipbuf2) {
				/* pip no display frame */
				if (cur_vd2_path_id != vd2_path_id)
					safe_switch_videolayer(1, false, true);
				vd_layer[1].dispbuf = NULL;
			} else if (cur_pipbuf2 == &local_pip2) {
				/* pip keep frame */
				vd_layer[1].dispbuf = cur_pipbuf2;
			} else if (vd_layer[1].dispbuf
				!= cur_pipbuf2) {
				new_frame2 = cur_pipbuf2;
			}
		}
		if (new_frame2 || cur_pipbuf2)
			vd_layer[1].dispbuf_mapping = &cur_pipbuf2;
		cur_blackout = blackout_pip2 | force_blackout;
	} else if (vd2_path_id != VFM_PATH_INVALID) {
		/* pip display in VD2 */
		new_frame2 = path1_new_frame;
		if (!new_frame2) {
			if (!cur_pipbuf) {
				/* pip no display frame */
				if (cur_vd2_path_id != vd2_path_id)
					safe_switch_videolayer(1, false, true);
				vd_layer[1].dispbuf = NULL;
			} else if (cur_pipbuf == &local_pip) {
				/* pip keep frame */
				vd_layer[1].dispbuf = cur_pipbuf;
			} else if (vd_layer[1].dispbuf
				!= cur_pipbuf) {
				new_frame2 = cur_pipbuf;
			}
		}
		if (new_frame2 || cur_pipbuf)
			vd_layer[1].dispbuf_mapping = &cur_pipbuf;
		cur_blackout = blackout_pip | force_blackout;
	} else {
		cur_blackout = 1;
	}

	if (!new_frame2 && vd_layer[1].dispbuf &&
	    is_local_vf(vd_layer[1].dispbuf)) {
		if (cur_blackout) {
			vd_layer[1].property_changed = false;
		} else if (vd_layer[1].dispbuf) {
			if (vd_layer[1].switch_vf && vd_layer[1].vf_ext)
				vd_layer[1].vf_ext->canvas0Addr =
					get_layer_display_canvas(1);
			else
				vd_layer[1].dispbuf->canvas0Addr =
					get_layer_display_canvas(1);
		}
	}

	if (vd_layer[1].dispbuf &&
	    (vd_layer[1].dispbuf->flag & (VFRAME_FLAG_VIDEO_COMPOSER |
		VFRAME_FLAG_VIDEO_DRM)) &&
	    !(debug_flag & DEBUG_FLAG_AXIS_NO_UPDATE)) {
		int mirror = 0;

		axis[0] = vd_layer[1].dispbuf->axis[0];
		axis[1] = vd_layer[1].dispbuf->axis[1];
		axis[2] = vd_layer[1].dispbuf->axis[2];
		axis[3] = vd_layer[1].dispbuf->axis[3];
		crop[0] = vd_layer[1].dispbuf->crop[0];
		crop[1] = vd_layer[1].dispbuf->crop[1];
		crop[2] = vd_layer[1].dispbuf->crop[2];
		crop[3] = vd_layer[1].dispbuf->crop[3];
		_set_video_window(&glayer_info[1], axis);
		source_type = vd_layer[1].dispbuf->source_type;
		if (source_type != VFRAME_SOURCE_TYPE_HDMI &&
			source_type != VFRAME_SOURCE_TYPE_CVBS &&
			source_type != VFRAME_SOURCE_TYPE_TUNER &&
			source_type != VFRAME_SOURCE_TYPE_HWC)
			_set_video_crop(&glayer_info[1], crop);
		if (vd_layer[1].dispbuf->flag & VFRAME_FLAG_MIRROR_H)
			mirror = H_MIRROR;
		if (vd_layer[1].dispbuf->flag & VFRAME_FLAG_MIRROR_V)
			mirror = V_MIRROR;
		_set_video_mirror(&glayer_info[1], mirror);
		set_alpha_scpxn(&vd_layer[1], vd_layer[1].dispbuf->componser_info);
		glayer_info[1].zorder = vd_layer[1].dispbuf->zorder;
	}

	/* setting video display property in underflow mode */
	if (!new_frame2 &&
	    vd_layer[1].dispbuf &&
	    (vd_layer[1].property_changed ||
	     is_picmode_changed(1, vd_layer[1].dispbuf))) {
		pip_swap_frame(&vd_layer[1], vd_layer[1].dispbuf, vinfo);
		need_disable_vd2 = false;
	} else if (new_frame2) {
		pip_swap_frame(&vd_layer[1], new_frame2, vinfo);
		need_disable_vd2 = false;
	}

	if (vd2_path_id == VFM_PATH_PIP ||
	    vd2_path_id == VFM_PATH_DEF)
		vd_layer[1].keep_frame_id = 1;
	else if (vd2_path_id == VFM_PATH_PIP2)
		vd_layer[1].keep_frame_id = 2;
	else if (vd2_path_id == VFM_PATH_AMVIDEO)
		vd_layer[1].keep_frame_id = 0;
	else
		vd_layer[1].keep_frame_id = 0xff;

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
	amvecm_on_vs
		(!is_local_vf(vd_layer[1].dispbuf)
		? vd_layer[1].dispbuf : NULL,
		new_frame2,
		new_frame2 ? CSC_FLAG_TOGGLE_FRAME : 0,
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
		VD2_PATH,
		VPP_TOP0);
#endif

	if (need_disable_vd2) {
		safe_switch_videolayer(1, false, true);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		/* reset dvel statue when disable vd2 */
		dvel_status = false;
#endif
	}

	if (cur_dev->max_vd_layers == 3) {
		if (vd_layer[2].dispbuf_mapping == &cur_dispbuf &&
		    (cur_dispbuf == &vf_local ||
		     !cur_dispbuf) &&
		    vd_layer[2].dispbuf != cur_dispbuf)
			vd_layer[2].dispbuf = cur_dispbuf;

		if (vd_layer[2].dispbuf_mapping == &cur_pipbuf &&
		    (cur_pipbuf == &local_pip ||
		     !cur_pipbuf) &&
		    vd_layer[2].dispbuf != cur_pipbuf)
			vd_layer[2].dispbuf = cur_pipbuf;

		if (vd_layer[2].dispbuf_mapping == &cur_pipbuf2 &&
		    (cur_pipbuf2 == &local_pip2 ||
		     !cur_pipbuf2) &&
		    vd_layer[2].dispbuf != cur_pipbuf2)
			vd_layer[2].dispbuf = cur_pipbuf2;

		if (gvideo_recv[0] &&
		    vd_layer[2].dispbuf_mapping == &gvideo_recv[0]->cur_buf &&
		    (gvideo_recv[0]->cur_buf == &gvideo_recv[0]->local_buf ||
		     !gvideo_recv[0]->cur_buf) &&
		    vd_layer[2].dispbuf != gvideo_recv[0]->cur_buf)
			vd_layer[2].dispbuf = gvideo_recv[0]->cur_buf;

		if (gvideo_recv[1] &&
		    vd_layer[2].dispbuf_mapping == &gvideo_recv[1]->cur_buf &&
		    (gvideo_recv[1]->cur_buf == &gvideo_recv[1]->local_buf ||
		     !gvideo_recv[1]->cur_buf) &&
		    vd_layer[2].dispbuf != gvideo_recv[1]->cur_buf)
			vd_layer[2].dispbuf = gvideo_recv[1]->cur_buf;

		if (gvideo_recv[2] &&
		    vd_layer[2].dispbuf_mapping == &gvideo_recv[2]->cur_buf &&
		    (gvideo_recv[2]->cur_buf == &gvideo_recv[2]->local_buf ||
		     !gvideo_recv[2]->cur_buf) &&
		    vd_layer[2].dispbuf != gvideo_recv[2]->cur_buf)
			vd_layer[2].dispbuf = gvideo_recv[2]->cur_buf;

		if (vd_layer[2].switch_vf &&
		    vd_layer[2].dispbuf &&
		    vd_layer[2].dispbuf->vf_ext)
			vd_layer[2].vf_ext =
				(struct vframe_s *)vd_layer[2].dispbuf->vf_ext;
		else
			vd_layer[2].vf_ext = NULL;

		/* vd3 config */
		if (gvideo_recv[0] &&
		    gvideo_recv[0]->path_id == vd3_path_id) {
			/* video_render.0 display on VD3 */
			new_frame3 = path3_new_frame;
			if (!new_frame3) {
				if (!gvideo_recv[0]->cur_buf) {
					/* video_render.0 no frame in display */
					if (cur_vd3_path_id != vd3_path_id)
						safe_switch_videolayer(2, false, true);
					vd_layer[2].dispbuf = NULL;
				} else if (gvideo_recv[0]->cur_buf ==
					&gvideo_recv[0]->local_buf) {
					/* video_render.0 keep frame */
					vd_layer[2].dispbuf = gvideo_recv[0]->cur_buf;
				} else if (vd_layer[2].dispbuf
					!= gvideo_recv[0]->cur_buf) {
					/* video_render.0 has frame in display */
					new_frame3 = gvideo_recv[0]->cur_buf;
				}
			}
			if (new_frame3 || gvideo_recv[0]->cur_buf)
				vd_layer[2].dispbuf_mapping = &gvideo_recv[0]->cur_buf;
			cur_blackout = 1;
		} else if (gvideo_recv[1] &&
		    (gvideo_recv[1]->path_id == vd3_path_id)) {
			/* video_render.1 display on VD3 */
			new_frame3 = path4_new_frame;
			if (!new_frame3) {
				if (!gvideo_recv[1]->cur_buf) {
					/* video_render.1 no frame in display */
					if (cur_vd3_path_id != vd3_path_id)
						safe_switch_videolayer(2, false, true);
					vd_layer[2].dispbuf = NULL;
				} else if (gvideo_recv[1]->cur_buf ==
					&gvideo_recv[1]->local_buf) {
					/* video_render.1 keep frame */
					vd_layer[2].dispbuf = gvideo_recv[1]->cur_buf;
				} else if (vd_layer[2].dispbuf
					!= gvideo_recv[1]->cur_buf) {
					/* video_render.1 has frame in display */
					new_frame3 = gvideo_recv[1]->cur_buf;
				}
			}
			if (new_frame3 || gvideo_recv[1]->cur_buf)
				vd_layer[2].dispbuf_mapping = &gvideo_recv[1]->cur_buf;
			cur_blackout = 1;
		} else if (gvideo_recv[2] &&
		    (gvideo_recv[2]->path_id == vd3_path_id)) {
			/* video_render.2 display on VD3 */
			new_frame3 = path5_new_frame;
			if (!new_frame3) {
				if (!gvideo_recv[2]->cur_buf) {
					/* video_render.2 no frame in display */
					if (cur_vd3_path_id != vd3_path_id)
						safe_switch_videolayer(2, false, true);
					vd_layer[2].dispbuf = NULL;
				} else if (gvideo_recv[2]->cur_buf ==
					&gvideo_recv[2]->local_buf) {
					/* video_render.2 keep frame */
					vd_layer[2].dispbuf = gvideo_recv[2]->cur_buf;
				} else if (vd_layer[2].dispbuf
					!= gvideo_recv[2]->cur_buf) {
					/* video_render.2 has frame in display */
					new_frame3 = gvideo_recv[2]->cur_buf;
				}
			}
			if (new_frame3 || gvideo_recv[2]->cur_buf)
				vd_layer[2].dispbuf_mapping = &gvideo_recv[2]->cur_buf;
			cur_blackout = 1;
		} else if (vd3_path_id == VFM_PATH_AMVIDEO) {
			/* priamry display in VD3 */
			new_frame3 = path0_new_frame;
			if (!new_frame3) {
				if (!cur_dispbuf) {
					/* primary no frame in display */
					if (cur_vd3_path_id != vd3_path_id)
						safe_switch_videolayer(2, false, true);
					vd_layer[2].dispbuf = NULL;
				} else if (cur_dispbuf == &vf_local) {
					/* priamry keep frame */
					vd_layer[2].dispbuf = cur_dispbuf;
				} else if (vd_layer[2].dispbuf
					!= cur_dispbuf) {
					new_frame3 = cur_dispbuf;
				}
			}
			if (new_frame3 || cur_dispbuf)
				vd_layer[2].dispbuf_mapping = &cur_dispbuf;
			cur_blackout = blackout | force_blackout;
		} else if (vd3_path_id == VFM_PATH_PIP) {
			/* pip2 display in VD3 */
			new_frame3 = path1_new_frame;
			if (!new_frame3) {
				if (!cur_pipbuf) {
					/* pip no display frame */
					if (cur_vd3_path_id != vd3_path_id)
						safe_switch_videolayer(2, false, true);
					vd_layer[2].dispbuf = NULL;
				} else if (cur_pipbuf == &local_pip) {
					/* pip keep frame */
					vd_layer[2].dispbuf = cur_pipbuf;
				} else if (vd_layer[2].dispbuf
					!= cur_pipbuf) {
					new_frame3 = cur_pipbuf;
				}
			}
			if (new_frame3 || cur_pipbuf)
				vd_layer[2].dispbuf_mapping = &cur_pipbuf;
			cur_blackout = blackout_pip | force_blackout;
		} else if (vd3_path_id != VFM_PATH_INVALID) {
			/* pip2 display in VD3 */
			new_frame3 = path2_new_frame;
			if (!new_frame3) {
				if (!cur_pipbuf2) {
					/* pip no display frame */
					if (cur_vd3_path_id != vd3_path_id)
						safe_switch_videolayer(2, false, true);
					vd_layer[2].dispbuf = NULL;
				} else if (cur_pipbuf2 == &local_pip2) {
					/* pip keep frame */
					vd_layer[2].dispbuf = cur_pipbuf2;
				} else if (vd_layer[2].dispbuf
					!= cur_pipbuf2) {
					new_frame3 = cur_pipbuf2;
				}
			}
			if (new_frame3 || cur_pipbuf2)
				vd_layer[2].dispbuf_mapping = &cur_pipbuf2;
			cur_blackout = blackout_pip2 | force_blackout;
		} else {
			cur_blackout = 1;
		}

		if (!new_frame3 && vd_layer[2].dispbuf &&
		    is_local_vf(vd_layer[2].dispbuf)) {
			if (cur_blackout) {
				vd_layer[2].property_changed = false;
			} else if (vd_layer[2].dispbuf) {
				if (vd_layer[2].switch_vf && vd_layer[1].vf_ext)
					vd_layer[2].vf_ext->canvas0Addr =
						get_layer_display_canvas(2);
				else
					vd_layer[2].dispbuf->canvas0Addr =
						get_layer_display_canvas(2);
			}
		}

		if (vd_layer[2].dispbuf &&
		    (vd_layer[2].dispbuf->flag & (VFRAME_FLAG_VIDEO_COMPOSER |
			VFRAME_FLAG_VIDEO_DRM)) &&
		    !(debug_flag & DEBUG_FLAG_AXIS_NO_UPDATE)) {
			axis[0] = vd_layer[2].dispbuf->axis[0];
			axis[1] = vd_layer[2].dispbuf->axis[1];
			axis[2] = vd_layer[2].dispbuf->axis[2];
			axis[3] = vd_layer[2].dispbuf->axis[3];
			crop[0] = vd_layer[2].dispbuf->crop[0];
			crop[1] = vd_layer[2].dispbuf->crop[1];
			crop[2] = vd_layer[2].dispbuf->crop[2];
			crop[3] = vd_layer[2].dispbuf->crop[3];
			_set_video_window(&glayer_info[2], axis);
			source_type = vd_layer[2].dispbuf->source_type;
			if (source_type != VFRAME_SOURCE_TYPE_HDMI &&
				source_type != VFRAME_SOURCE_TYPE_CVBS &&
				source_type != VFRAME_SOURCE_TYPE_TUNER &&
				source_type != VFRAME_SOURCE_TYPE_HWC)
				_set_video_crop(&glayer_info[2], crop);
			set_alpha_scpxn(&vd_layer[2], vd_layer[2].dispbuf->componser_info);
			glayer_info[2].zorder = vd_layer[2].dispbuf->zorder;
		}

		/* setting video display property in underflow mode */
		if (!new_frame3 &&
		    vd_layer[2].dispbuf &&
		    (vd_layer[2].property_changed ||
		     is_picmode_changed(2, vd_layer[2].dispbuf))) {
			pip2_swap_frame(&vd_layer[2], vd_layer[2].dispbuf, vinfo);
			need_disable_vd3 = false;
		} else if (new_frame3) {
			pip2_swap_frame(&vd_layer[2], new_frame3, vinfo);
			need_disable_vd3 = false;
		}

		if (vd3_path_id == VFM_PATH_PIP2 ||
		    vd3_path_id == VFM_PATH_DEF)
			vd_layer[2].keep_frame_id = 2;
		else if (vd3_path_id == VFM_PATH_PIP)
			vd_layer[2].keep_frame_id = 1;
		else if (vd3_path_id == VFM_PATH_AMVIDEO)
			vd_layer[2].keep_frame_id = 0;
		else
			vd_layer[2].keep_frame_id = 0xff;

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
	amvecm_on_vs
		(!is_local_vf(vd_layer[2].dispbuf)
		? vd_layer[2].dispbuf : NULL,
		new_frame3,
		new_frame3 ? CSC_FLAG_TOGGLE_FRAME : 0,
		curpip2_frame_par ?
		curpip2_frame_par->supsc1_hori_ratio :
		0,
		curpip2_frame_par ?
		curpip2_frame_par->supsc1_vert_ratio :
		0,
		curpip2_frame_par ?
		curpip2_frame_par->spsc1_w_in :
		0,
		curpip2_frame_par ?
		curpip2_frame_par->spsc1_h_in :
		0,
		curpip2_frame_par ?
		curpip2_frame_par->cm_input_w :
		0,
		curpip2_frame_par ?
		curpip2_frame_par->cm_input_h :
		0,
		VD3_PATH,
		VPP_TOP0);
#endif
		if (need_disable_vd3)
			safe_switch_videolayer(2, false, true);
	}

	/* filter setting management */
	primary_render_frame(&vd_layer[0]);
	pip_render_frame(&vd_layer[1], vinfo);
	pip2_render_frame(&vd_layer[2], vinfo);
	video_secure_set(VPP0);

	if (vd_layer[0].dispbuf &&
		(vd_layer[0].dispbuf->flag & VFRAME_FLAG_FAKE_FRAME)) {
		if ((vd_layer[0].force_black &&
			!(debug_flag & DEBUG_FLAG_NO_CLIP_SETTING)) ||
			!vd_layer[0].force_black) {
			if (vd_layer[0].dispbuf->type & VIDTYPE_RGB_444) {
				/* RGB */
				vd_layer[0].clip_setting.clip_max =
					(0x0 << 20) | (0x0 << 10) | 0;
				vd_layer[0].clip_setting.clip_min =
					vd_layer[0].clip_setting.clip_max;
			} else {
				/* YUV */
				vd_layer[0].clip_setting.clip_max =
					(0x0 << 20) | (0x200 << 10) | 0x200;
				vd_layer[0].clip_setting.clip_min =
					vd_layer[0].clip_setting.clip_max;
			}
			vd_layer[0].clip_setting.clip_done = false;
		}
		if (!vd_layer[0].force_black) {
			pr_debug("vsync: vd1 force black\n");
			vd_layer[0].force_black = true;
		}
	} else if (vd_layer[0].force_black) {
		pr_debug("vsync: vd1 black to normal\n");
		vd_layer[0].clip_setting.clip_max =
			(0x3ff << 20) | (0x3ff << 10) | 0x3ff;
		vd_layer[0].clip_setting.clip_min = 0;
		vd_layer[0].clip_setting.clip_done = false;
		vd_layer[0].force_black = false;
	}

	if (vd_layer[1].dispbuf &&
	    (vd_layer[1].dispbuf->flag & VFRAME_FLAG_FAKE_FRAME))
		safe_switch_videolayer(1, false, true);

	if (vd_layer[2].dispbuf &&
	    (vd_layer[2].dispbuf->flag & VFRAME_FLAG_FAKE_FRAME))
		safe_switch_videolayer(2, false, true);

	if ((cur_vd1_path_id != vd1_path_id ||
	     cur_vd2_path_id != vd2_path_id ||
	     cur_vd3_path_id != vd3_path_id) &&
	    (debug_flag & DEBUG_FLAG_PRINT_PATH_SWITCH)) {
		pr_info("VID: === After path switch ===\n");
		pr_info("VID: \tpath_id: %d, %d, %d;\nVID: \ttoggle:%p, %p, %p %p, %p, %p\nVID: \tcur:%p, %p, %p, %p, %p, %p;\n",
			vd1_path_id, vd2_path_id, vd3_path_id,
			path0_new_frame, path1_new_frame,
			path2_new_frame, path3_new_frame,
			path4_new_frame, path5_new_frame,
			cur_dispbuf, cur_pipbuf, cur_pipbuf2,
			gvideo_recv[0] ? gvideo_recv[0]->cur_buf : NULL,
			gvideo_recv[1] ? gvideo_recv[1]->cur_buf : NULL,
			gvideo_recv[2] ? gvideo_recv[2]->cur_buf : NULL);
		pr_info("VID: \tdispbuf:%p, %p, %p; \tvf_ext:%p, %p, %p;\nVID: \tlocal:%p, %p, %p, %p, %p, %p\n",
			vd_layer[0].dispbuf, vd_layer[1].dispbuf, vd_layer[2].dispbuf,
			vd_layer[0].vf_ext, vd_layer[1].vf_ext, vd_layer[2].vf_ext,
			&vf_local, &local_pip, &local_pip2,
			gvideo_recv[0] ? &gvideo_recv[0]->local_buf : NULL,
			gvideo_recv[1] ? &gvideo_recv[1]->local_buf : NULL,
			gvideo_recv[2] ? &gvideo_recv[2]->local_buf : NULL);
		pr_info("VID: \tblackout:%d %d, %d force:%d;\n",
			blackout, blackout_pip, blackout_pip2, force_blackout);
	}

	if (vd_layer[0].dispbuf &&
	    (vd_layer[0].dispbuf->type & VIDTYPE_MVC))
		vd_layer[0].enable_3d_mode = mode_3d_mvc_enable;
	else if (process_3d_type)
		vd_layer[0].enable_3d_mode = mode_3d_enable;
	else
		vd_layer[0].enable_3d_mode = mode_3d_disable;

	/* all frames has been renderred, so reset new frame flag */
	vd_layer[0].new_frame = false;
	vd_layer[1].new_frame = false;
	vd_layer[2].new_frame = false;

	if (vd_layer[0].dispbuf) {
		pq_process_debug[0] = ai_pq_value;
		pq_process_debug[1] = ai_pq_disable;
		pq_process_debug[2] = ai_pq_debug;
		pq_process_debug[3] = ai_pq_policy;
#ifdef CONFIG_AMLOGIC_VDETECT
		vdetect_get_frame_nn_info(vd_layer[0].dispbuf);
#endif
		vf_pq_process(vd_layer[0].dispbuf, vpp_scenes, pq_process_debug);
		if (ai_pq_debug > 0x10) {
			ai_pq_debug--;
			if (ai_pq_debug == 0x10)
				ai_pq_debug = 0;
		}
		memcpy(nn_scenes_value, vd_layer[0].dispbuf->nn_value,
			   sizeof(nn_scenes_value));
	}
exit:
	vd_clip_setting(0, &vd_layer[0].clip_setting);
	vd_clip_setting(1, &vd_layer[1].clip_setting);
	if (cur_dev->max_vd_layers == 3)
		vd_clip_setting(2, &vd_layer[2].clip_setting);

	vpp_blend_update(vinfo);

	if (gvideo_recv[0])
		gvideo_recv[0]->func->late_proc(gvideo_recv[0]);
	if (gvideo_recv[1])
		gvideo_recv[1]->func->late_proc(gvideo_recv[1]);
	if (gvideo_recv[2])
		gvideo_recv[2]->func->late_proc(gvideo_recv[2]);

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
RUN_FIRST_RDMA:
	/* vsync_rdma_config(); */
	vsync_rdma_process();
#endif

	/* if prop_change not zero, event will be delayed to next vsync */
	if (video_prop_status &&
	    !atomic_read(&video_prop_change)) {
		if (debug_flag & DEBUG_FLAG_TRACE_EVENT)
			pr_info("VD1 send event, changed status: 0x%x\n",
				video_prop_status);
		atomic_set(&video_prop_change, video_prop_status);
		video_prop_status = VIDEO_PROP_CHANGE_NONE;
		wake_up_interruptible(&amvideo_prop_change_wait);
	}
	if (video_info_change_status) {
		struct vd_info_s vd_info;

		if (debug_flag & DEBUG_FLAG_TRACE_EVENT)
			pr_info("VD1 send event to frc, changed status: 0x%x\n",
				video_info_change_status);
		vd_info.flags = video_info_change_status;
		vd_signal_notifier_call_chain(VIDEO_INFO_CHANGED,
					      &vd_info);
		video_info_change_status = VIDEO_INFO_CHANGE_NONE;
	}

	vpp_crc_result = vpp_crc_check(vpp_crc_en, VPP0);

	cur_vd1_path_id = vd1_path_id;
	cur_vd2_path_id = vd2_path_id;
	cur_vd3_path_id = vd3_path_id;
	return 0;
}

int proc_lowlatency_frame(u8 instance_id)
{
	u32 enc_line1, enc_line2, last_line;
	u32 min_line, max_line, start_line, vinfo_height;
	const struct vinfo_s *cur_vinfo;
	bool err_flag = false;
	static ulong last_vsync_count;

	if (legacy_vpp)
		return 0;

	if (atomic_read(&video_recv_cnt) > 1)
		return -1;

	cur_vinfo = get_current_vinfo();
	if (!cur_vinfo) {
		lowlatency_err_drop++;
		return -1;
	}
	if (cur_vinfo->field_height != cur_vinfo->height)
		vinfo_height = cur_vinfo->field_height;
	else
		vinfo_height = cur_vinfo->height;

	start_line = get_active_start_line();
	min_line = (vinfo_height * line_threshold) / 100 + start_line;
	max_line = (vinfo_height * (100 - line_threshold)) / 100 + start_line;
	enc_line1 = get_cur_enc_line();
	if (enc_line1 >= max_line || overrun_flag) {
		lowlatency_proc_drop++;
		return -2;
	}

	if (atomic_inc_return(&video_proc_lock) > 1) {
		lowlatency_proc_drop++;
		atomic_dec(&video_proc_lock);
		return -2;
	}

	if (lowlatency_vsync_count == last_vsync_count) {
		lowlatency_overflow_cnt++;
		atomic_dec(&video_proc_lock);
		return -2;
	}

	last_line = enc_line1;
	while (enc_line1 < min_line) {
		usleep_range(500, 600);
		enc_line1 = get_cur_enc_line();
		if (last_line == enc_line1) {
			/* active line no change */
			err_flag = true;
			break;
		}
		last_line = enc_line1;
	}
	if (err_flag) {
		lowlatency_err_drop++;
		atomic_dec(&video_proc_lock);
		return -1;
	}
	atomic_set(&video_inirq_flag, 1);
	enc_line1 = get_cur_enc_line();

	if (lowlatency_max_enter_lines < enc_line1)
		lowlatency_max_enter_lines = enc_line1;
	if (lowlatency_min_enter_lines > enc_line1)
		lowlatency_min_enter_lines = enc_line1;

	last_vsync_count = lowlatency_vsync_count;
	lowlatency_vsync(instance_id);
	lowlatency_proc_done++;
	enc_line2 = get_cur_enc_line();
	if (enc_line2 < enc_line1) {
		lowlatency_overrun_cnt++;
		overrun_flag = true;
	}
	if (lowlatency_min_exit_lines > enc_line2 && !overrun_flag)
		lowlatency_min_exit_lines = enc_line2;
	if (lowlatency_max_exit_lines < enc_line2)
		lowlatency_max_exit_lines = enc_line2;
	enc_line2 -= enc_line1;
	if (lowlatency_max_proc_lines < enc_line2)
		lowlatency_max_proc_lines = enc_line2;
	atomic_set(&video_inirq_flag, 0);
	atomic_dec(&video_proc_lock);
	return 0;
}
EXPORT_SYMBOL(proc_lowlatency_frame);

ssize_t lowlatency_states_show(struct class *cla,
	struct class_attribute *attr,
	char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len,
		"low latency process done count: %d\n",
		lowlatency_proc_done);
	len += sprintf(buf + len,
		"low latency process frame count: %d\n",
		lowlatency_proc_frame_cnt);
	len += sprintf(buf + len,
		"low latency skip frame count: %d\n",
		lowlatency_skip_frame_cnt);
	len += sprintf(buf + len,
		"low latency overflow count: %d\n",
		lowlatency_overflow_cnt);
	len += sprintf(buf + len,
		"low latency process drop count: %d\n",
		lowlatency_proc_drop);
	len += sprintf(buf + len,
		"low latency process err count: %d\n",
		lowlatency_err_drop);
	len += sprintf(buf + len,
		"low latency process overrun count: %d\n",
		lowlatency_overrun_cnt);
	len += sprintf(buf + len,
		"low latency process overrun recovery count: %d\n",
		lowlatency_overrun_recovery_cnt);
	len += sprintf(buf + len,
		"low latency process max proc lines: %d\n",
		lowlatency_max_proc_lines);
	len += sprintf(buf + len,
		"low latency process max enter lines: %d\n",
		lowlatency_max_enter_lines);
	len += sprintf(buf + len,
		"low latency process max exit lines: %d\n",
		lowlatency_max_exit_lines);
	len += sprintf(buf + len,
		"low latency process min enter lines: %d\n",
		lowlatency_min_enter_lines);
	len += sprintf(buf + len,
		"low latency process min exit lines: %d\n",
		lowlatency_min_exit_lines);
	len += sprintf(buf + len,
		"vsync process done count: %d\n",
		vsync_proc_done);
	len += sprintf(buf + len,
		"vsync process drop count: %d\n",
		vsync_proc_drop);
	len += sprintf(buf + len,
		"video_receiver cnt: %d\n",
		atomic_read(&video_recv_cnt));
	len += sprintf(buf + len,
		"line threshold %d\n",
		line_threshold);
	return len;
}

ssize_t lowlatency_states_store(struct class *cla,
	struct class_attribute *attr,
	const char *buf, size_t count)
{
	int ret;
	u32 val = 0;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return -EINVAL;

	lowlatency_err_drop = 0;
	lowlatency_proc_done = 0;
	lowlatency_overrun_cnt = 0;
	lowlatency_overrun_recovery_cnt = 0;
	lowlatency_max_enter_lines = 0;
	lowlatency_max_proc_lines = 0;
	lowlatency_max_exit_lines = 0;
	lowlatency_min_enter_lines = 0xffffffff;
	lowlatency_min_exit_lines = 0xffffffff;
	lowlatency_proc_drop = 0;
	lowlatency_overflow_cnt = 0;
	lowlatency_proc_frame_cnt = 0;
	lowlatency_skip_frame_cnt = 0;
	vsync_proc_drop = 0;
	vsync_proc_done = 0;
	pr_info("Clear the lowlatency states information!\n");
	return count;
}

