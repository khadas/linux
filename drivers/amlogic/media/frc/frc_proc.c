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
#include "frc_reg.h"

int frc_enable_cnt = 2;
module_param(frc_enable_cnt, int, 0664);
MODULE_PARM_DESC(frc_enable_cnt, "frc enable counter");

int frc_disable_cnt = 2;
module_param(frc_disable_cnt, int, 0664);
MODULE_PARM_DESC(frc_disable_cnt, "frc disable counter");

int frc_re_cfg_cnt = 4;/*need bigger than frc_disable_cnt*/
module_param(frc_re_cfg_cnt, int, 0664);
MODULE_PARM_DESC(frc_re_cfg_cnt, "frc reconfig counter");

void frc_fw_initial(struct frc_dev_s *devp)
{
	if (!devp)
		return;

	devp->in_sts.vs_cnt = 0;
	devp->in_sts.vs_tsk_cnt = 0;
	devp->in_sts.vs_timestamp = sched_clock();
	devp->in_sts.vf_repeat_cnt = 0;
	devp->in_sts.vf_null_cnt = 0;

	devp->out_sts.vs_cnt = 0;
	devp->out_sts.vs_tsk_cnt = 0;
	devp->out_sts.vs_timestamp = sched_clock();
	devp->in_sts.vf = NULL;
	devp->frc_sts.vs_cnt = 0;
	devp->vs_timestamp = sched_clock();
}

void frc_hw_initial(struct frc_dev_s *devp)
{
	frc_fw_initial(devp);
	frc_mtx_set(devp);
	frc_top_init(devp);
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
}

void frc_in_reg_monitor(struct frc_dev_s *devp)
{
	u32 i;
	u32 reg;
	//char *buf = devp->dbg_buf;

	for (i = 0; i < MONITOR_REG_MAX; i++) {
		reg = devp->dbg_in_reg[i];
		if (reg != 0 && reg < 0x3fff) {
			if (devp->dbg_buf_len > 300) {
				devp->dbg_reg_monitor_i = 0;
				devp->dbg_buf_len = 0;
				return;
			}
			devp->dbg_buf_len++;
			pr_info("ivs:%d 0x%x=0x%08x\n", devp->out_sts.vs_cnt,
				reg, READ_FRC_REG(reg));
		}
	}
}

void frc_vf_monitor(struct frc_dev_s *devp)
{
	if (devp->dbg_buf_len > 300) {
		devp->dbg_vf_monitor = 0;
		return;
	}
	devp->dbg_buf_len++;
	pr_info("ivs:%d 0x%lx\n", devp->frc_sts.vs_cnt, (ulong)devp->in_sts.vf);
}

void frc_out_reg_monitor(struct frc_dev_s *devp)
{
	u32 i;
	u32 reg;

	for (i = 0; i < MONITOR_REG_MAX; i++) {
		reg = devp->dbg_out_reg[i];
		if (reg != 0 && reg < 0x3fff) {
			if (devp->dbg_buf_len > 300) {
				devp->dbg_reg_monitor_o = 0;
				devp->dbg_buf_len = 0;
				return;
			}
			devp->dbg_buf_len++;
			pr_info("\t\t\t\tovs:%d 0x%x=0x%08x\n", devp->out_sts.vs_cnt,
				reg, READ_FRC_REG(reg));
		}
	}
}

void frc_dump_monitor_data(struct frc_dev_s *devp)
{
	//char *buf = devp->dbg_buf;

	//pr_info("%d, %s\n", devp->dbg_buf_len, buf);
	devp->dbg_buf_len = 0;
}

irqreturn_t frc_input_isr(int irq, void *dev_id)
{
	struct frc_dev_s *devp = (struct frc_dev_s *)dev_id;
	u64 timestamp = sched_clock();

	devp->in_sts.vs_cnt++;
	/*update vs time*/
	devp->in_sts.vs_duration = timestamp - devp->in_sts.vs_timestamp;
	devp->in_sts.vs_timestamp = timestamp;

	me_undown_read(devp);

	if (devp->dbg_reg_monitor_i)
		frc_in_reg_monitor(devp);

	tasklet_schedule(&devp->input_tasklet);

	return IRQ_HANDLED;
}

void frc_input_tasklet_pro(unsigned long arg)
{
	struct frc_dev_s *devp = (struct frc_dev_s *)arg;
	struct frc_fw_data_s *fw_data;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	devp->in_sts.vs_tsk_cnt++;

	if (!devp->probe_ok)
		return;

	if (!devp->frc_fw_pause) {
		frc_scene_detect_input(devp);
		frc_film_detect_ctrl(devp);
		if (fw_data->search_final_line_para.bbd_en == 1)
			frc_bbd_ctrl(devp);
		frc_iplogo_ctrl(devp);
	}
}

irqreturn_t frc_output_isr(int irq, void *dev_id)
{
	struct frc_dev_s *devp = (struct frc_dev_s *)dev_id;
	u64 timestamp = sched_clock();

	devp->out_sts.vs_cnt++;
	/*update vs time*/
	devp->out_sts.vs_duration = timestamp - devp->out_sts.vs_timestamp;
	devp->out_sts.vs_timestamp = timestamp;

	mc_undown_read(devp);

	if (devp->dbg_reg_monitor_o)
		frc_out_reg_monitor(devp);

	tasklet_schedule(&devp->output_tasklet);

	return IRQ_HANDLED;
}

void frc_output_tasklet_pro(unsigned long arg)
{
	struct frc_dev_s *devp = (struct frc_dev_s *)arg;
	struct frc_fw_data_s *fw_data;

	devp->out_sts.vs_tsk_cnt++;
	fw_data = (struct frc_fw_data_s *)devp->fw_data;

	if (!devp->probe_ok)
		return;

	if (!devp->frc_fw_pause) {
		frc_scene_detect_output(devp);
		if (fw_data->g_stMeCtrl_Para.me_en == 1)
			frc_me_ctrl(devp);
		if (fw_data->g_stvpctrl_para.vp_en == 1)
			frc_vp_ctrl(devp);
		frc_mc_ctrl(devp);
		//frc_melogo_ctrl(devp);
	}
}

void frc_change_to_state(enum frc_state_e state)
{
	struct frc_dev_s *devp = get_frc_devp();

	if (devp->frc_sts.state_transing) {
		pr_frc(0, "%s state_transing busy!\n", __func__);
	} else if (devp->frc_sts.state != state) {
		devp->frc_sts.new_state = state;
		devp->frc_sts.state_transing = true;
		pr_frc(1, "%s %d->%d\n", __func__, devp->frc_sts.state, state);
	}
}

const char * const frc_state_ary[] = {
	"FRC_STATE_DISABLE",
	"FRC_STATE_ENABLE",
	"FRC_STATE_BYPASS",
};

int frc_update_in_sts(struct frc_dev_s *devp, struct st_frc_in_sts *frc_in_sts,
				struct vframe_s *vf, struct vpp_frame_par_s *cur_video_sts)
{
	if (!vf || !cur_video_sts)
		return -1;

	frc_in_sts->vf_type = vf->type;
	frc_in_sts->duration = vf->duration;
	frc_in_sts->signal_type = vf->signal_type;
	frc_in_sts->source_type = vf->source_type;
	frc_in_sts->vf = vf;

	/* for debug */
	if (devp->dbg_force_en && devp->dbg_input_hsize && devp->dbg_input_vsize) {
		frc_in_sts->in_hsize = devp->dbg_input_hsize;
		frc_in_sts->in_vsize = devp->dbg_input_vsize;
	} else {
		if (devp->frc_hw_pos == FRC_POS_AFTER_POSTBLEND) {
			frc_in_sts->in_hsize = devp->out_sts.vout_width;
			frc_in_sts->in_vsize = devp->out_sts.vout_height;
		} else {
			frc_in_sts->in_hsize = cur_video_sts->nnhf_input_w;
			frc_in_sts->in_vsize = cur_video_sts->nnhf_input_h;
		}
	}
	//pr_frc(dbg_sts, "in size(%d,%d) sr_out(%d,%d) dbg(%d,%d)\n",
	//	frc_in_sts->in_hsize, frc_in_sts->in_vsize,
	//	cur_video_sts->nnhf_input_w, cur_video_sts->nnhf_input_h,
	//	devp->dbg_input_hsize, devp->dbg_input_vsize);

	return 0;
}

enum efrc_event frc_input_sts_check(struct frc_dev_s *devp,
						struct st_frc_in_sts *cur_in_sts)
{
	/* check change */
	enum efrc_event sts_change = FRC_EVENT_NO_EVENT;
	//enum frc_state_e cur_state = devp->frc_sts.state;
	u32 cur_sig_in;

	/*back up*/
	devp->in_sts.vf_type = cur_in_sts->vf_type;
	devp->in_sts.duration = cur_in_sts->duration;
	devp->in_sts.signal_type = cur_in_sts->signal_type;
	devp->in_sts.source_type = cur_in_sts->source_type;
	/* check size change */
	if (devp->in_sts.in_hsize != cur_in_sts->in_hsize) {
		pr_frc(1, "hsize change (%d - %d)\n",
			devp->in_sts.in_hsize, cur_in_sts->in_hsize);
		/*need reconfig*/
		devp->frc_sts.re_cfg_cnt = frc_re_cfg_cnt;
		sts_change |= FRC_EVENT_VF_CHG_IN_SIZE;
	}
	devp->in_sts.in_hsize = cur_in_sts->in_hsize;
	/* check size change */
	if (devp->in_sts.in_hsize != cur_in_sts->in_hsize) {
		pr_frc(1, "vsize change (%d - %d)\n",
			devp->in_sts.in_vsize, cur_in_sts->in_vsize);
		/*need reconfig*/
		devp->frc_sts.re_cfg_cnt = frc_re_cfg_cnt;
		sts_change |= FRC_EVENT_VF_CHG_IN_SIZE;
	}
	devp->in_sts.in_vsize = cur_in_sts->in_vsize;
	/* check is same vframe */
	pr_frc(dbg_sts, "vf (0x%lx, 0x%lx)\n", (ulong)devp->in_sts.vf, (ulong)cur_in_sts->vf);
	if (devp->in_sts.vf == cur_in_sts->vf && cur_in_sts->vf_sts)
		devp->in_sts.vf_repeat_cnt++;

	devp->in_sts.vf = cur_in_sts->vf;

	if (devp->frc_sts.re_cfg_cnt) {
		devp->frc_sts.re_cfg_cnt--;
		cur_sig_in = false;
	} else if (devp->in_sts.in_hsize == 0 || devp->in_sts.in_vsize == 0) {
		cur_sig_in = false;
	} else {
		cur_sig_in = cur_in_sts->vf_sts;
	}

	pr_frc(dbg_sts, "vf_sts: %d, cur_sig_in:0x%x have_cnt:%d no_cnt:%d re_cfg_cnt:%d\n",
		devp->in_sts.vf_sts, cur_sig_in,
		devp->in_sts.have_vf_cnt, devp->in_sts.no_vf_cnt, devp->frc_sts.re_cfg_cnt);

	pr_frc(dbg_sts, "hvsize (%d,%d) cur(%d,%d)\n",
		devp->in_sts.in_hsize, devp->in_sts.in_vsize,
		cur_in_sts->in_hsize, cur_in_sts->in_vsize);

	switch (devp->in_sts.vf_sts) {
	case VFRAME_NO:
		if (cur_sig_in == VFRAME_HAVE) {
			if (devp->in_sts.have_vf_cnt++ >= frc_enable_cnt)  {
				devp->in_sts.vf_sts = cur_sig_in;
				//if (FRC_EVENT_VF_IS_GAME)
				sts_change |= FRC_EVENT_VF_CHG_TO_HAVE;
				devp->in_sts.have_vf_cnt = 0;
				pr_frc(1, "FRC_EVENT_VF_CHG_TO_HAVE\n");
			}
		} else {
			devp->in_sts.have_vf_cnt = 0;
		}
		break;
	case VFRAME_HAVE:
		if (cur_sig_in == VFRAME_NO) {
			if (devp->in_sts.no_vf_cnt++ >= frc_disable_cnt) {
				devp->in_sts.vf_sts = cur_sig_in;
				devp->in_sts.no_vf_cnt = 0;
				pr_frc(1, "FRC_EVENT_VF_CHG_TO_NO\n");
				sts_change |= FRC_EVENT_VF_CHG_TO_NO;
			}
		} else {
			devp->in_sts.no_vf_cnt = 0;
		}
		break;
	}

	/* even attach , mode change */
	if (devp->frc_sts.auto_ctrl && sts_change) {
		if (sts_change & FRC_EVENT_VF_CHG_TO_NO)
			frc_change_to_state(FRC_STATE_DISABLE);
		else if (sts_change & FRC_EVENT_VF_CHG_TO_HAVE)
			frc_change_to_state(FRC_STATE_ENABLE);
	}

	if (devp->dbg_vf_monitor)
		frc_vf_monitor(devp);

	return sts_change;
}

/*video_input_w
 * input vframe and display mode handle
 *
 */
void frc_input_vframe_handle(struct frc_dev_s *devp, struct vframe_s *vf,
					struct vpp_frame_par_s *cur_video_sts)
{
	struct st_frc_in_sts cur_in_sts;
	u32 no_input = false;
	enum efrc_event frc_event;

	if (!devp)
		return;

	if (!devp->probe_ok)
		return;

	if (!vf || !cur_video_sts) {
		devp->in_sts.vf_null_cnt++;
		no_input = true;
	}

	if (devp->frc_hw_pos == FRC_POS_AFTER_POSTBLEND)
		cur_in_sts.vf_sts = true;
	else
		cur_in_sts.vf_sts = no_input ? false : true;

	frc_update_in_sts(devp, &cur_in_sts, vf, cur_video_sts);

	/* check input is change */
	frc_event = frc_input_sts_check(devp, &cur_in_sts);
	if (frc_event)
		pr_frc(1, "event = 0x%08x\n", frc_event);
	/*need disable and bypass frc*/
	//if (no_input && devp->dbg_force_en == FRC_STATE_DISABLE)
	//	frc_change_to_state(FRC_STATE_BYPASS);
}

void frc_state_change_finish(struct frc_dev_s *devp)
{
	devp->frc_sts.state = devp->frc_sts.new_state;
	devp->frc_sts.state_transing = false;
}

void frc_state_handle(struct frc_dev_s *devp)
{
	enum frc_state_e cur_state;
	enum frc_state_e new_state;
	u32 state_changed = 0;
	u32 frame_cnt = 0;
	u32 log = 1;

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
				frc_state_change_finish(devp);
			} else if (new_state == FRC_STATE_ENABLE) {
				frc_hw_initial(devp);
				//first : set bypass off
				set_frc_bypass(OFF);
				//second: set frc enable on
				set_frc_enable(ON);
				devp->frc_sts.frame_cnt = 0;
				pr_frc(log, "sm state change %s -> %s\n",
					frc_state_ary[cur_state], frc_state_ary[new_state]);
				frc_state_change_finish(devp);
			} else {
				pr_frc(0, "err new state %d\n", new_state);
			}
		}
		break;
	case FRC_STATE_ENABLE:
		if (state_changed) {
			if (new_state == FRC_STATE_DISABLE) {
				if (devp->frc_sts.frame_cnt == 0) {
					set_frc_enable(OFF);
					frc_reset(1);
					devp->frc_sts.frame_cnt++;
				} else {
					frc_reset(0);
					devp->frc_sts.frame_cnt = 0;
					pr_frc(log, "sm state change %s -> %s\n",
						frc_state_ary[cur_state],
						frc_state_ary[new_state]);
					frc_state_change_finish(devp);
				}
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
					frc_state_change_finish(devp);
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
				frc_state_change_finish(devp);
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
					frc_state_change_finish(devp);
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
