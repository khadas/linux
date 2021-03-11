// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <asm/div64.h>
#include <linux/sched/clock.h>

#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>

#include "frc_common.h"
#include "frc_drv.h"
#include "frc_scene.h"
#include "frc_bbd.h"
#include "frc_vp.h"
#include "frc_proc.h"
#include "frc_logo.h"
#include "frc_film.h"
#include "frc_me.h"
#include "frc_mc.h"
#include "frc_hw.h"

void frc_fw_initial(struct frc_dev_s *devp)
{
	if (!devp)
		return;

	devp->in_sts.vs_cnt = 0;
	devp->in_sts.vs_tsk_cnt = 0;
	devp->out_sts.vs_cnt = 0;
	devp->out_sts.vs_tsk_cnt = 0;
	devp->frc_sts.vs_cnt = 0;

	devp->vs_timestamp = sched_clock();
	devp->in_sts.vs_timestamp = sched_clock();
	devp->out_sts.vs_timestamp = sched_clock();
}

void frc_hw_initial(struct frc_dev_s *devp)
{
	frc_fw_initial(devp);

//	frc_top_init(devp);

	//  me param init
	frc_me_param_init(devp);
	//  vp param init
	frc_vp_param_init(devp);
	//  mc param init
	frc_mc_param_init(devp);
	//  bbd param init
	frc_bbd_param_init(devp);
	// logo param init
	frc_logo_param_init(devp);
	//  fd  param init
	frc_film_param_init(devp);
	pr_frc(0, "%s ok\n", __func__);
}

irqreturn_t frc_input_isr(int irq, void *dev_id)
{
	struct frc_dev_s *devp = (struct frc_dev_s *)dev_id;
	u64 timestamp = sched_clock();

	/*update vs time*/
	devp->in_sts.vs_cnt++;
	devp->in_sts.vs_duration = timestamp - devp->in_sts.vs_timestamp;
	devp->in_sts.vs_timestamp = timestamp;

	tasklet_schedule(&devp->input_tasklet);

	queue_work(devp->frc_wq, &devp->frc_work);
	return IRQ_HANDLED;
}

void frc_input_tasklet_pro(unsigned long arg)
{
	struct frc_dev_s *devp = (struct frc_dev_s *)arg;

	devp->in_sts.vs_tsk_cnt++;
#ifndef DISABLE_FRC_FW
	frc_scene_detect_input(devp);
	frc_film_detect_ctrl(devp);
	frc_bbd_ctrl(devp);
	frc_iplogo_ctrl(devp);
#endif
}

irqreturn_t frc_output_isr(int irq, void *dev_id)
{
	struct frc_dev_s *devp = (struct frc_dev_s *)dev_id;
	u64 timestamp = sched_clock();

	devp->out_sts.vs_cnt++;
	/*update vs time*/
	devp->out_sts.vs_duration = timestamp - devp->out_sts.vs_timestamp;
	devp->out_sts.vs_timestamp = timestamp;

	tasklet_schedule(&devp->output_tasklet);

	return IRQ_HANDLED;
}

void frc_output_tasklet_pro(unsigned long arg)
{
	struct frc_dev_s *devp = (struct frc_dev_s *)arg;
	struct frc_fw_data_s *fw_data;

	devp->out_sts.vs_tsk_cnt++;
	fw_data = (struct frc_fw_data_s *)devp->fw_data;
#ifndef DISABLE_FRC_FW
	frc_scene_detect_output(devp);
	frc_me_ctrl(devp);
	if (fw_data->g_stvpctrl_para.vp_en == 1)
		frc_vp_ctrl(devp);
	frc_mc_ctrl(devp);
	frc_melogo_ctrl(devp);
#endif
}

void frc_change_to_state(enum frc_state_e state)
{
	struct frc_dev_s *devp = get_frc_devp();

	if (devp->frc_sts.state != state) {
		devp->frc_sts.new_state = state;
		pr_frc(1, "%s %d->%d\n", __func__, devp->frc_sts.state, state);
	}
}

const char * const frc_state_ary[] = {
	"FRC_STATE_DISABLE",
	"FRC_STATE_ENABLE",
	"FRC_STATE_BYPASS",
};

void frc_update_in_sts(struct frc_dev_s *devp, struct st_frc_in_sts *frc_in_sts,
				struct vframe_s *vf, struct vpp_frame_par_s *cur_video_sts)
{
	if (!vf || !cur_video_sts)
		return;

	frc_in_sts->vf_type = vf->type;
	frc_in_sts->duration = vf->duration;
	frc_in_sts->signal_type = vf->signal_type;
	frc_in_sts->source_type = vf->source_type;
	/* for debug */
	if (devp->dbg_force_en && devp->dbg_input_hsize && devp->dbg_input_vsize) {
		frc_in_sts->in_hsize = devp->dbg_input_hsize;
		frc_in_sts->in_vsize = devp->dbg_input_vsize;
	} else {
		//frc_in_sts->in_hsize =
		//cur_video_sts->VPP_post_blend_vd_h_end_ -
		//	cur_video_sts->VPP_post_blend_vd_h_start_ + 1;
		//frc_in_sts->in_vsize  =
		//cur_video_sts->VPP_post_blend_vd_v_end_ -
		//	cur_video_sts->VPP_post_blend_vd_v_start_ + 1;

		//pr_frc(2, "vpp post:hsize=%d, vsize=%d\n",
		//       cur_video_sts->VPP_post_blend_vd_h_start_,
		//       cur_video_sts->VPP_post_blend_vd_h_end_);

		//frc_in_sts->in_hsize = cur_video_sts->video_input_w;
		//frc_in_sts->in_vsize = cur_video_sts->video_input_h;

		frc_in_sts->in_hsize =
			cur_video_sts->VPP_hsc_endp - cur_video_sts->VPP_hsc_startp + 1;
		frc_in_sts->in_vsize =
			cur_video_sts->VPP_vsc_endp - cur_video_sts->VPP_vsc_startp + 1;
	}
}

enum chg_flag frc_in_sts_check(struct frc_dev_s *devp, struct st_frc_in_sts *frc_in_sts)
{
	enum chg_flag ret = FRC_CHG_NONE;

	return ret;
}

/*video_input_w
 * input vframe and display mode handle
 *
 */
void frc_input_vframe_handle(struct frc_dev_s *devp, struct vframe_s *vf,
					struct vpp_frame_par_s *cur_video_sts)
{
	struct st_frc_in_sts cur_in_sts;
	struct st_frc_in_sts *in_stsp;
	u32 no_input = 0;
	enum chg_flag chg;

	if (!devp)
		return;

	if (!vf || !cur_video_sts)
		no_input = 1;

	cur_in_sts.vf_sts = no_input ? 0 : 1;
	frc_update_in_sts(devp, &cur_in_sts, vf, cur_video_sts);
	in_stsp = &devp->in_sts;

	/* check input is change */
	chg = frc_in_sts_check(devp, &cur_in_sts);
	/*local play*/
	if (cur_in_sts.source_type == VFRAME_SOURCE_TYPE_OTHERS)
		devp->in_out_ratio = 0x0101;	/*1:1*/

	memcpy(in_stsp, &cur_in_sts, sizeof(cur_in_sts));

	/*need disable and bypass frc*/
	//if (no_input && devp->dbg_force_en == FRC_STATE_DISABLE)
	//	frc_change_to_state(FRC_STATE_BYPASS);
}

void frc_state_handle(struct frc_dev_s *devp)
{
	enum frc_state_e cur_state;
	enum frc_state_e new_state;
	u32 state_changed = 0;
	u32 frame_cnt = 0;
	u32 log = 2;

	devp->frc_sts.vs_cnt++;
	cur_state = devp->frc_sts.state;
	new_state = devp->frc_sts.new_state;
	frame_cnt = devp->frc_sts.frame_cnt;
	if (cur_state != new_state) {
		state_changed = 1;
		pr_frc(log, "sm state_changed (%d->%d)\n", cur_state, new_state);
	}
	switch (cur_state) {
	case FRC_STATE_DISABLE:
		if (state_changed) {
			if (new_state == FRC_STATE_BYPASS) {
				set_frc_bypass(ON);
				devp->frc_sts.frame_cnt = 0;
				pr_frc(log, "sm state change %s -> %s\n",
				       frc_state_ary[cur_state], frc_state_ary[new_state]);
				devp->frc_sts.state = devp->frc_sts.new_state;
			} else if (new_state == FRC_STATE_ENABLE) {
				frc_hw_initial(devp);
				//first : set bypass off
				set_frc_bypass(OFF);
				//second: set frc enable on
				set_frc_enable(ON);
				devp->frc_sts.frame_cnt = 0;
				pr_frc(log, "sm state change %s -> %s\n",
					frc_state_ary[cur_state], frc_state_ary[new_state]);
				devp->frc_sts.state = devp->frc_sts.new_state;
			} else {
				pr_frc(0, "err new state %d\n", new_state);
			}
		}
		break;
	case FRC_STATE_ENABLE:
		if (state_changed) {
			if (new_state == FRC_STATE_DISABLE) {
				set_frc_enable(OFF);
				devp->frc_sts.frame_cnt = 0;
				pr_frc(log, "sm state change %s -> %s\n",
					frc_state_ary[cur_state], frc_state_ary[new_state]);
				devp->frc_sts.state = devp->frc_sts.new_state;
			} else if (new_state == FRC_STATE_BYPASS) {
				//first frame set enable off
				if (devp->frc_sts.frame_cnt == 0) {
					set_frc_enable(OFF);
					//set_frc_bypass(OFF);
					devp->frc_sts.frame_cnt++;
				} else {
					//second frame set bypass on
					set_frc_bypass(ON);
					devp->frc_sts.frame_cnt = 0;
					pr_frc(log, "sm state change %s -> %s\n",
					       frc_state_ary[cur_state], frc_state_ary[new_state]);
					devp->frc_sts.state = devp->frc_sts.new_state;
				}
			} else {
				pr_frc(0, "err new state %d\n", new_state);
			}
		}
		break;

	case FRC_STATE_BYPASS:
		if (state_changed) {
			if (new_state == FRC_STATE_DISABLE) {
				set_frc_bypass(OFF);
				set_frc_enable(OFF);
				devp->frc_sts.frame_cnt = 0;
				pr_frc(log, "sm state change %s -> %s\n",
				       frc_state_ary[cur_state], frc_state_ary[new_state]);
				devp->frc_sts.state = devp->frc_sts.new_state;
			} else if (new_state == FRC_STATE_ENABLE) {
				if (devp->frc_sts.frame_cnt == 0) {
					//first frame set bypass off
					set_frc_bypass(OFF);
					set_frc_enable(OFF);
					devp->frc_sts.frame_cnt++;
				} else {
					frc_hw_initial(devp);
					//second frame set enable on
					set_frc_enable(ON);
					devp->frc_sts.frame_cnt = 0;
					pr_frc(log, "sm state change %s -> %s\n",
					       frc_state_ary[cur_state], frc_state_ary[new_state]);
					devp->frc_sts.state = devp->frc_sts.new_state;
				}
			} else {
				pr_frc(0, "err new state %d\n", new_state);
			}
		}
		break;

	default:
		pr_frc(0, "err state %d\n", cur_state);
		break;
	}
}
