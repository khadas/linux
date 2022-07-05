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

#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/frc/frc_reg.h>
#include <linux/amlogic/media/frc/frc_common.h>
#include <linux/amlogic/tee.h>
#include <linux/clk.h>
#include <linux/amlogic/media/video_sink/video.h>

#include "frc_drv.h"
#include "frc_proc.h"
#include "frc_hw.h"

int frc_enable_cnt = 1;
module_param(frc_enable_cnt, int, 0664);
MODULE_PARM_DESC(frc_enable_cnt, "frc enable counter");

int frc_disable_cnt = 1;
module_param(frc_disable_cnt, int, 0664);
MODULE_PARM_DESC(frc_disable_cnt, "frc disable counter");

int frc_re_cfg_cnt = 3;/*need bigger than frc_disable_cnt 3, 15*/
module_param(frc_re_cfg_cnt, int, 0664);
MODULE_PARM_DESC(frc_re_cfg_cnt, "frc reconfig counter");

u32 secure_tee_handle;

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
	return;
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

/*frc-fw input task execution time*/
void frc_in_task_print(u64 timer)
{
	static u64 in_tsk_inx, in_tsk_min, in_tsk_max, in_tsk_sum;

	in_tsk_inx++;
	in_tsk_sum += timer;
	if (in_tsk_min > timer)
		in_tsk_min = timer;
	if (in_tsk_max < timer)
		in_tsk_max = timer;

	if (in_tsk_inx == 60) {
		pr_frc(0, "in_tsk_time  min = %lld max = %lld avg = %lld\n",
			in_tsk_min, in_tsk_max, div64_u64(in_tsk_sum, 60));
		in_tsk_min = in_tsk_max;
		in_tsk_inx = 0;
		in_tsk_sum = 0;
		in_tsk_max = 0;
	}
}

/*frc-fw output task execution time*/
void frc_out_task_print(u64 timer)
{
	static u64 out_tsk_inx, out_tsk_min, out_tsk_max, out_tsk_sum;

	out_tsk_inx++;
	out_tsk_sum += timer;
	if (out_tsk_min > timer)
		out_tsk_min = timer;
	if (out_tsk_max < timer)
		out_tsk_max = timer;

	if (out_tsk_inx == 60) {
		pr_frc(0, "out_tsk_time min = %lld max = %lld avg = %lld\n",
			out_tsk_min, out_tsk_max, div64_u64(out_tsk_sum, 60));
		out_tsk_min = out_tsk_max;
		out_tsk_inx = 0;
		out_tsk_sum = 0;
		out_tsk_max = 0;
	}
}

irqreturn_t frc_input_isr(int irq, void *dev_id)
{
	struct frc_dev_s *devp = (struct frc_dev_s *)dev_id;
	u64 timestamp = sched_clock();

	devp->in_sts.vs_cnt++;
	/*update vs time*/
	devp->in_sts.vs_duration = timestamp - devp->in_sts.vs_timestamp;
	devp->in_sts.vs_timestamp = timestamp;

	if (!devp->probe_ok || !devp->power_on_flag)
		return IRQ_HANDLED;

	inp_undone_read(devp);
	if (devp->dbg_reg_monitor_i)
		frc_in_reg_monitor(devp);

	tasklet_schedule(&devp->input_tasklet);

	return IRQ_HANDLED;
}

void frc_input_tasklet_pro(unsigned long arg)
{
	struct frc_dev_s *devp = (struct frc_dev_s *)arg;
	struct frc_fw_data_s *pfw_data;
	u64 timestamp;

	pfw_data = (struct frc_fw_data_s *)devp->fw_data;
	if (!pfw_data)
		return;
	if (!devp->probe_ok)
		return;
	if (!devp->power_on_flag) {
		// devp->power_off_flag++;
		return;
	}
	devp->in_sts.vs_tsk_cnt++;
	if (!devp->frc_fw_pause) {
		timestamp = sched_clock();
		if (pfw_data->scene_detect_input)
			pfw_data->scene_detect_input(pfw_data);
		if (pfw_data->film_detect_ctrl)
			pfw_data->film_detect_ctrl(pfw_data);
		if (pfw_data->bbd_ctrl)
			pfw_data->bbd_ctrl(pfw_data);
		if (pfw_data->iplogo_ctrl)
			pfw_data->iplogo_ctrl(pfw_data);
		// if (!devp->power_on_flag)
		//	devp->power_off_flag++;
		if (devp->ud_dbg.inud_time_en)
			frc_in_task_print(sched_clock() - timestamp);
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

	if (!devp->probe_ok || !devp->power_on_flag)
		return IRQ_HANDLED;

	me_undone_read(devp);
	mc_undone_read(devp);
	vp_undone_read(devp);

	get_vout_info(devp);

	if (devp->dbg_reg_monitor_o)
		frc_out_reg_monitor(devp);

	tasklet_schedule(&devp->output_tasklet);

	return IRQ_HANDLED;
}

void frc_output_tasklet_pro(unsigned long arg)
{
	struct frc_dev_s *devp = (struct frc_dev_s *)arg;
	struct frc_fw_data_s *pfw_data;
	u64 timestamp;

	pfw_data = (struct frc_fw_data_s *)devp->fw_data;
	if (!pfw_data)
		return;
	if (!devp->probe_ok)
		return;
	if (!devp->power_on_flag) {
		// devp->power_off_flag++;
		return;
	}
	devp->out_sts.vs_tsk_cnt++;
	if (!devp->frc_fw_pause) {
		timestamp = sched_clock();
		if (pfw_data->scene_detect_output)
			pfw_data->scene_detect_output(pfw_data);
		if (pfw_data->me_ctrl)
			pfw_data->me_ctrl(pfw_data);
		if (pfw_data->vp_ctrl)
			pfw_data->vp_ctrl(pfw_data);
		if (pfw_data->mc_ctrl)
			pfw_data->mc_ctrl(pfw_data);
		if (pfw_data->melogo_ctrl)
			pfw_data->melogo_ctrl(pfw_data);
		if (pfw_data->frc_fw_ctrl_if)
			pfw_data->frc_fw_ctrl_if(pfw_data);
		// if (!devp->power_on_flag)
		//	devp->power_off_flag++;
		if (devp->ud_dbg.outud_time_en)
			frc_out_task_print(sched_clock() - timestamp);
	}
}

void frc_change_to_state(enum frc_state_e state)
{
	struct frc_dev_s *devp = get_frc_devp();

	if (devp->in_sts.vf_sts == 0 && state == FRC_STATE_ENABLE) {
		devp->frc_sts.frame_cnt = 0;
		devp->frc_sts.state_transing = false;
		pr_frc(0, "%s %d->%d, no video, can't change\n", __func__,
				devp->frc_sts.state, state);
	} else if (devp->frc_sts.state_transing) {
		pr_frc(0, "%s state_transing busy!\n", __func__);
		if (state != devp->frc_sts.new_state) {
			devp->frc_sts.state = devp->frc_sts.new_state;
			devp->frc_sts.new_state = state;
			devp->frc_sts.frame_cnt = 0;
			devp->frc_sts.state_transing = false;
			pr_frc(0, "busy broken:%s %d->%d\n", __func__, devp->frc_sts.state, state);
		}
	} else if (devp->frc_sts.state != state) {
		devp->frc_sts.new_state = state;
		devp->frc_sts.state_transing = true;
		pr_frc(0, "%s %d->%d\n", __func__, devp->frc_sts.state, state);
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
	/*secure mode*/
	if (devp->in_sts.secure_mode &&
		!secure_tee_handle && devp->buf.cma_mem_alloced)
		frc_mm_secure_set(devp);

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
	struct vinfo_s *vinfo = get_current_vinfo();

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
	if (devp->in_sts.in_vsize != cur_in_sts->in_vsize) {
		pr_frc(1, "vsize change (%d - %d)\n",
			devp->in_sts.in_vsize, cur_in_sts->in_vsize);
		/*need reconfig*/
		devp->frc_sts.re_cfg_cnt = frc_re_cfg_cnt;
		sts_change |= FRC_EVENT_VF_CHG_IN_SIZE;
	}
	devp->in_sts.in_vsize = cur_in_sts->in_vsize;

	if (devp->frc_sts.out_put_mode_changed || devp->frc_sts.re_config) {
		pr_frc(1, "out_put_mode_changed 0x%x re_config:%d\n",
			devp->frc_sts.out_put_mode_changed,
			devp->frc_sts.re_config);
		if (devp->frc_sts.out_put_mode_changed ==
			FRC_EVENT_VF_CHG_IN_SIZE) {
			devp->frc_sts.re_cfg_cnt = 5;
		} else if (devp->frc_sts.out_put_mode_changed ==
			FRC_EVENT_VOUT_CHG) {
			devp->frc_sts.re_cfg_cnt = 3;
			pr_frc(1, "out_chg-w(%d->%d)-h(%d->%d)\n",
				devp->out_sts.vout_width, vinfo->width,
				devp->out_sts.vout_height, vinfo->height);
			//devp->out_sts.vout_height = vinfo->height;
			//devp->out_sts.vout_width = vinfo->width;
		} else {
			devp->frc_sts.re_cfg_cnt = frc_re_cfg_cnt;
		}
		sts_change |= FRC_EVENT_VOUT_CHG;
		devp->frc_sts.out_put_mode_changed = 0;
		devp->frc_sts.re_config = 0;
	}

	/* check is same vframe */
	pr_frc(dbg_sts, "vf (0x%lx, 0x%lx)\n", (ulong)devp->in_sts.vf, (ulong)cur_in_sts->vf);
	if (devp->in_sts.vf == cur_in_sts->vf && cur_in_sts->vf_sts)
		devp->in_sts.vf_repeat_cnt++;

	devp->in_sts.vf = cur_in_sts->vf;

	if (devp->frc_sts.re_cfg_cnt) {
		devp->frc_sts.re_cfg_cnt--;
		cur_sig_in = false;
	} else if (devp->in_sts.in_hsize < FRC_H_LIMIT ||
			devp->in_sts.in_vsize < FRC_V_LIMIT) {
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
			if (devp->frc_sts.state == FRC_STATE_DISABLE &&
				devp->in_sts.vf_null_cnt == 108) {
				devp->frc_sts.retrycnt = 0;
				frc_change_to_state(FRC_STATE_BYPASS);
				pr_frc(1, "no_frm->chg bypass\n");
			} else if (devp->frc_sts.state == FRC_STATE_BYPASS &&
					devp->in_sts.vf_null_cnt == 120) {
				// clk_set_rate(devp->clk_frc, 333333333);
				devp->clk_state = FRC_CLOCK_2MIN;
				schedule_work(&devp->frc_clk_work);
				pr_frc(1, "no_frm->reduce clk\n");
			}
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
	u32 no_input = false, vd_en_flag, vd_regval;
	enum efrc_event frc_event;

	if (!devp)
		return;

	if (!devp->probe_ok || !devp->power_on_flag)
		return;

	vd_en_flag = get_video_enabled();
	vd_regval = vpu_reg_read(0x1dfb);
	if (devp->ud_dbg.res1_dbg_en == 1)
		pr_frc(1, "get_vd_en=%2d, 0x1dfb=0x%8x\n",
			vd_en_flag, vd_regval);
	if (!vf || !cur_video_sts || vd_en_flag == 0) {
		devp->in_sts.vf_null_cnt++;
		no_input = true;
	} else if ((vd_regval & (BIT_0 | BIT_8)) == 0) {
		devp->in_sts.vf_null_cnt++;
		no_input = true;
	}

	if (vf) {
		if ((vf->flag & VFRAME_FLAG_GAME_MODE)  ==
					VFRAME_FLAG_GAME_MODE) {
			if ((devp->in_sts.st_flag & FRC_FLAG_GAME_MODE) !=
						FRC_FLAG_GAME_MODE) {
				devp->in_sts.st_flag =
				devp->in_sts.st_flag | FRC_FLAG_GAME_MODE;
				pr_frc(1, "video = game_mode");
			}
			no_input = true;
		} else if ((vf->flag & VFRAME_FLAG_PC_MODE)  ==
					VFRAME_FLAG_PC_MODE) {
			if ((devp->in_sts.st_flag & FRC_FLAG_PC_MODE) !=
						FRC_FLAG_PC_MODE) {
				devp->in_sts.st_flag =
					devp->in_sts.st_flag | FRC_FLAG_PC_MODE;
				pr_frc(1, "video = pc_mode");
			}
			no_input = true;
		} else if ((vf->flag & VFRAME_FLAG_HIGH_BANDWIDTH) ==
				VFRAME_FLAG_HIGH_BANDWIDTH) {
			if ((devp->in_sts.st_flag & FRC_FLAG_HIGH_BW) !=
						FRC_FLAG_HIGH_BW) {
				devp->in_sts.st_flag =
					devp->in_sts.st_flag | FRC_FLAG_HIGH_BW;
				pr_frc(1, "video = high_bw");
			}
			no_input = true;
		} else {
			devp->in_sts.st_flag =
				devp->in_sts.st_flag &
					(~(FRC_FLAG_GAME_MODE +
						FRC_FLAG_HIGH_BW +
						FRC_FLAG_PC_MODE));
		}

		if ((vf->type & VIDTYPE_PIC) == VIDTYPE_PIC) {
			if ((devp->in_sts.st_flag & FRC_FLAG_PIC_MODE) !=
						FRC_FLAG_PIC_MODE) {
				devp->in_sts.st_flag =
				devp->in_sts.st_flag | FRC_FLAG_PIC_MODE;
				pr_frc(1, "video = pic_mode");
			}
			no_input = true;
		} else {
			devp->in_sts.st_flag =
				devp->in_sts.st_flag & (~FRC_FLAG_PIC_MODE);
		}

		if (vf->height < FRC_V_LIMIT || vf->width < FRC_H_LIMIT) {
			if ((devp->in_sts.st_flag & FRC_FLAG_LIMIT_SIZE) !=
						FRC_FLAG_LIMIT_SIZE) {
				devp->in_sts.st_flag =
				devp->in_sts.st_flag | FRC_FLAG_LIMIT_SIZE;
				pr_frc(1, "video = limit_size");
			}
			no_input = true;
		} else {
			devp->in_sts.st_flag =
				devp->in_sts.st_flag & (~FRC_FLAG_LIMIT_SIZE);
		}

		if ((vf->flag & VFRAME_FLAG_VIDEO_SECURE) ==
				 VFRAME_FLAG_VIDEO_SECURE) {
			devp->in_sts.secure_mode = true;
			/*for test secure mode disable memc*/
			//no_input = true;
		} else {
			devp->in_sts.secure_mode = false;
		}
		/*check vd status change*/
		if (!no_input)
			frc_chk_vd_sts_chg(devp, vf);
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
	frc_char_flash_check();
}

void frc_state_change_finish(struct frc_dev_s *devp)
{
	devp->frc_sts.state = devp->frc_sts.new_state;
	devp->frc_sts.state_transing = false;
}

void frc_test_mm_secure_set_off(struct frc_dev_s *devp)
{
#ifdef CONFIG_AMLOGIC_TEE
	if (!tee_enabled()) {
		pr_frc(0, "tee is not enable\n");
		return;
	}

	if (secure_tee_handle) {
		tee_unprotect_mem(secure_tee_handle);
		pr_frc(0, "%s handl:%d\n", __func__, secure_tee_handle);
		secure_tee_handle = 0;
	}
#endif
}

void frc_test_mm_secure_set_on(struct frc_dev_s *devp, u32 start, u32 size)
{
#ifdef CONFIG_AMLOGIC_TEE
	if (!tee_enabled()) {
		pr_frc(0, "tee is not enable\n");
		return;
	}

	if (!secure_tee_handle) {
		tee_protect_mem_by_type(TEE_MEM_TYPE_FRC, start, size, &secure_tee_handle);
		pr_frc(0, "%s handl:%d start:0x%x size:0x%x\n", __func__,
			secure_tee_handle, start, size);
	}
#endif
}

void frc_mm_secure_set(struct frc_dev_s *devp)
{
#ifdef CONFIG_AMLOGIC_TEE
	u32 addr_start;
	u32 addr_size;
	enum frc_state_e new_state;

	if (!tee_enabled()) {
		pr_frc(0, "tee is not enable\n");
		return;
	}
	/*data buffer set to secure mode*/
	addr_start = devp->buf.cma_mem_paddr_start + devp->buf.lossy_mc_y_data_buf_paddr[0];
	// data buf size: 0x9a30000
	addr_size = devp->buf.cma_mem_size -
		devp->buf.lossy_mc_y_data_buf_paddr[0] + devp->buf.cma_mem_size2;

	/*data buffer, me/mc info and link buffer set to secure mode*/
	//addr_start = devp->buf.cma_mem_paddr_start + devp->buf.lossy_mc_y_info_buf_paddr;
	//addr_size = devp->buf.norm_hme_data_buf_paddr[0] - devp->buf.lossy_mc_y_info_buf_paddr;

	new_state = devp->frc_sts.new_state;

	/*secure mode check*/
	if (!devp->in_sts.secure_mode) {
		if (devp->buf.secured) {
			devp->buf.secured = false;
			/*call secure api to exit secure mode*/
			tee_unprotect_mem(secure_tee_handle);
			frc_force_secure(false);
			pr_frc(1, "%s tee_unprotect_mem %d\n", __func__,
				secure_tee_handle);
			secure_tee_handle = 0;
		}
	} else {
		/*need set mm to secure mode*/
		if (new_state == FRC_STATE_ENABLE) {
			if (!devp->buf.secured) {
				devp->buf.secured = true;
				/*call secure api enter secure mode*/
				tee_protect_mem_by_type(TEE_MEM_TYPE_FRC, addr_start,
							addr_size, &secure_tee_handle);
				frc_force_secure(true);
				pr_frc(1, "%s handl:%d addr_start:0x%x addr_size:0x%x\n",
					__func__, secure_tee_handle,
					addr_start, addr_size);
			}
		} else {
			if (devp->buf.secured) {
				devp->buf.secured = false;
				/*call secure api to exit secure mode*/
				tee_unprotect_mem(secure_tee_handle);
				frc_force_secure(false);
				pr_frc(1, "%s tee_unprotect_mem %d\n", __func__,
					secure_tee_handle);
				secure_tee_handle = 0;
			}
		}
	}
#else
	if (devp->in_sts.secure_mode)
		pr_frc(1, "err: secure no tee define!!!\n");
#endif
}

void frc_state_handle_old(struct frc_dev_s *devp)
{
	enum frc_state_e cur_state;
	enum frc_state_e new_state;
	struct frc_fw_data_s *pfw_data;
	u32 state_changed = 0;
	u32 frame_cnt = 0;
	u32 log = 1;

	cur_state = devp->frc_sts.state;
	new_state = devp->frc_sts.new_state;
	frame_cnt = devp->frc_sts.frame_cnt;
	pfw_data = (struct frc_fw_data_s *)devp->fw_data;
	if (cur_state != new_state) {
		state_changed = 1;
		pr_frc(log, "sm state_changed (%d->%d) cnt:%d\n", cur_state, new_state,
			devp->frc_sts.frame_cnt);
	}
	switch (cur_state) {
	case FRC_STATE_DISABLE:
		if (state_changed) {
			if (new_state == FRC_STATE_BYPASS) {
				set_frc_bypass(ON);
				frc_mm_secure_set(devp);
				devp->frc_sts.frame_cnt = 0;
				pr_frc(log, "sm state change %s -> %s\n",
				       frc_state_ary[cur_state], frc_state_ary[new_state]);
				frc_state_change_finish(devp);
			} else if (new_state == FRC_STATE_ENABLE) {
				if (devp->frc_sts.frame_cnt == 0) {
					frc_mm_secure_set(devp);
					frc_hw_initial(devp);
					//first : set bypass off
					set_frc_bypass(OFF);
					if (pfw_data->frc_input_cfg)
						pfw_data->frc_input_cfg(devp->fw_data);
					//second: set frc enable on
					set_frc_enable(ON);
					pr_frc(log, "sm state change %s -> %s\n",
						frc_state_ary[cur_state], frc_state_ary[new_state]);
					devp->frc_sts.frame_cnt = 0;
					frc_state_change_finish(devp);
				} else {
					devp->frc_sts.frame_cnt++;
				}
			} else {
				pr_frc(0, "err new state %d\n", new_state);
			}
		}
		break;
	case FRC_STATE_ENABLE:
		if (state_changed) {
			if (new_state == FRC_STATE_DISABLE) {
				if (devp->frc_sts.frame_cnt == 0) {
					frc_mm_secure_set(devp);
					set_frc_enable(OFF);
					devp->frc_sts.frame_cnt++;
				} else {
					devp->frc_sts.frame_cnt = 0;
					pr_frc(log, "sm state change %s -> %s\n",
						frc_state_ary[cur_state],
						frc_state_ary[new_state]);
					frc_state_change_finish(devp);
				}
			} else if (new_state == FRC_STATE_BYPASS) {
				//first frame set enable off
				if (devp->frc_sts.frame_cnt == 0) {
					frc_mm_secure_set(devp);
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
					frc_mm_secure_set(devp);
					//first frame set bypass off
					set_frc_bypass(OFF);
					//set_frc_enable(OFF);
					devp->frc_sts.frame_cnt++;
				} else if (devp->frc_sts.frame_cnt == 1) {
					frc_hw_initial(devp);
					//second frame set enable on
					if (pfw_data->frc_input_cfg)
						pfw_data->frc_input_cfg(devp->fw_data);
					set_frc_enable(ON);
					devp->frc_sts.frame_cnt = 0;
					pr_frc(log, "sm state change %s -> %s\n",
					       frc_state_ary[cur_state], frc_state_ary[new_state]);
					frc_state_change_finish(devp);
				} else {
					devp->frc_sts.frame_cnt++;
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

void frc_state_handle(struct frc_dev_s *devp)
{
	enum frc_state_e cur_state;
	enum frc_state_e new_state;
	struct frc_fw_data_s *pfw_data;
	u32 state_changed = 0;
	u32 frame_cnt = 0;
	static u8 forceidx;
	static u8 framedelay;
	u8 frc_input_fid = 0;
	u32 log = 1;

	cur_state = devp->frc_sts.state;
	new_state = devp->frc_sts.new_state;
	frame_cnt = devp->frc_sts.frame_cnt;
	pfw_data = (struct frc_fw_data_s *)devp->fw_data;
	if (cur_state != new_state) {
		state_changed = 1;
		pr_frc(log, "stat_chg(%d->%d) frm_cnt:%d\n", cur_state,
			new_state, frame_cnt);
	}
	if (new_state == FRC_STATE_ENABLE && !devp->buf.buf_ctrl) {
		devp->buf.buf_ctrl = 1;
		schedule_work(&frc_mem_dyc_proc);
	}
	switch (cur_state) {
	case FRC_STATE_DISABLE:
	if (state_changed) {
		if (new_state == FRC_STATE_BYPASS) {
			set_frc_bypass(ON);
			frc_mm_secure_set(devp);
			devp->frc_sts.frame_cnt = 0;
			pr_frc(log, "stat_chg %s -> %s done\n",
					frc_state_ary[cur_state],
					frc_state_ary[new_state]);
			frc_state_change_finish(devp);
		} else if (new_state == FRC_STATE_ENABLE) {
			if (devp->frc_sts.frame_cnt == 0) {
				if (devp->clk_state != FRC_CLOCK_NOR &&
					devp->clk_state != FRC_CLOCK_2NOR) {
					devp->clk_state = FRC_CLOCK_2NOR;
					schedule_work(&devp->frc_clk_work);
				} else if (devp->clk_state == FRC_CLOCK_NOR &&
					devp->buf.cma_mem_alloced) {
					frc_mm_secure_set(devp);
					// clk_set_rate(devp->clk_frc, 667000000);
					get_vout_info(devp);
					frc_hw_initial(devp);
					//first : set bypass off
					set_frc_bypass(OFF);
					if (pfw_data->frc_input_cfg)
						pfw_data->frc_input_cfg(devp->fw_data);
					//second: set frc enable on
					set_frc_enable(ON);
					framedelay =
					(READ_FRC_REG(FRC_REG_TOP_CTRL9) >> 24) & 0xF;
					pr_frc(log, "frc_frm_dly %d\n",
							framedelay);
					devp->frc_sts.frame_cnt++;
				}
			} else if (devp->frc_sts.frame_cnt == framedelay + 1) {
				forceidx = frc_frame_forcebuf_enable(1);
				frc_frame_forcebuf_count(forceidx);
				pr_frc(log, "d-e_freeze idx:%d, frm:%d\n",
					forceidx, devp->frc_sts.frame_cnt);
				devp->frc_sts.frame_cnt++;
			} else if (devp->frc_sts.frame_cnt > framedelay + 1 &&
						devp->frc_sts.frame_cnt <
						framedelay * 2 + 1) {
				frc_frame_forcebuf_count(forceidx);
				frc_input_fid =
				READ_FRC_REG(FRC_REG_PAT_POINTER) >> 4 & 0xF;
				pr_frc(log, "d-e_freeze readidx:%d, frm:%d\n",
					frc_input_fid, devp->frc_sts.frame_cnt);
				devp->frc_sts.frame_cnt++;
			} else if (devp->frc_sts.frame_cnt ==
						framedelay * 2 + 1) {
				frc_frame_forcebuf_enable(0);
				frc_state_change_finish(devp);
				pr_frc(log, "d-e_stat_chg %s -> %s done\n",
					frc_state_ary[cur_state],
					frc_state_ary[new_state]);
				devp->frc_sts.frame_cnt = 0;
			} else if (devp->frc_sts.frame_cnt < framedelay + 1) {
				devp->frc_sts.frame_cnt++;
			}
		} else {
			pr_frc(0, "err new state %d\n", new_state);
		}
	}
	break;
	case FRC_STATE_ENABLE:
	if (state_changed) {
		if (new_state == FRC_STATE_DISABLE) {
			if (devp->frc_sts.frame_cnt == 0) {
				frc_mm_secure_set(devp);
				frc_frame_forcebuf_enable(0);
				set_frc_enable(OFF);
				devp->frc_sts.frame_cnt++;
			} else {
				devp->frc_sts.frame_cnt = 0;
				pr_frc(log, "stat_chg %s -> %s done\n",
					frc_state_ary[cur_state],
					frc_state_ary[new_state]);
				frc_state_change_finish(devp);
				devp->buf.buf_ctrl = 0;
				schedule_work(&frc_mem_dyc_proc);
			}
		} else if (new_state == FRC_STATE_BYPASS) {
			//first frame set enable off
			if (devp->frc_sts.frame_cnt == 0) {
				frc_mm_secure_set(devp);
				frc_frame_forcebuf_enable(0);
				set_frc_enable(OFF);
				//set_frc_bypass(OFF);
				devp->frc_sts.frame_cnt++;
			} else {
				//second frame set bypass on
				set_frc_bypass(ON);
				devp->frc_sts.frame_cnt = 0;
				pr_frc(log, "stat_chg %s -> %s done\n",
				       frc_state_ary[cur_state],
				       frc_state_ary[new_state]);
				frc_state_change_finish(devp);
				devp->buf.buf_ctrl = 0;
				schedule_work(&frc_mem_dyc_proc);
			}
		} else {
			pr_frc(0, "err new state %d\n", new_state);
		}
	}
	break;
	case FRC_STATE_BYPASS:
	if (state_changed) {
		if (new_state == FRC_STATE_DISABLE) {
			if (devp->frc_sts.frame_cnt == 0) {
				//clk_set_rate(devp->clk_frc, 667000000);
				if (devp->clk_state != FRC_CLOCK_NOR &&
					devp->clk_state != FRC_CLOCK_2NOR)  {
					devp->clk_state = FRC_CLOCK_2NOR;
					schedule_work(&devp->frc_clk_work);
				} else if (devp->clk_state == FRC_CLOCK_NOR) {
					devp->frc_sts.frame_cnt++;
				}
			} else {
				set_frc_bypass(OFF);
				set_frc_enable(OFF);
				devp->frc_sts.frame_cnt = 0;
				pr_frc(log, "stat_chg %s -> %s done\n",
				frc_state_ary[cur_state],
				frc_state_ary[new_state]);
				frc_state_change_finish(devp);
			}
		} else if (new_state == FRC_STATE_ENABLE) {
			if (devp->frc_sts.frame_cnt == 0) {
				//clk_set_rate(devp->clk_frc, 667000000);
				if (devp->clk_state != FRC_CLOCK_NOR &&
					devp->clk_state != FRC_CLOCK_2NOR)  {
					devp->clk_state = FRC_CLOCK_2NOR;
					schedule_work(&devp->frc_clk_work);
				} else if (devp->clk_state == FRC_CLOCK_NOR &&
					devp->buf.cma_mem_alloced) {
					//first frame set bypass off
					frc_mm_secure_set(devp);
					get_vout_info(devp);
					frc_hw_initial(devp);
					set_frc_bypass(OFF);
					devp->frc_sts.frame_cnt++;
				}
			} else if (devp->frc_sts.frame_cnt == 1) {
				//second frame set enable on
				if (pfw_data->frc_input_cfg)
					pfw_data->frc_input_cfg(devp->fw_data);
				set_frc_enable(ON);
				framedelay =
				(READ_FRC_REG(FRC_REG_TOP_CTRL9) >> 24) & 0xF;
				pr_frc(log, "frc_frm_dly %d\n",
							framedelay);
				devp->frc_sts.frame_cnt++;
			} else if (devp->frc_sts.frame_cnt == framedelay + 2) {
				forceidx = frc_frame_forcebuf_enable(1);
				frc_frame_forcebuf_count(forceidx);
				pr_frc(log, "b-e_freeze idx:%d, frm:%d\n",
					forceidx, devp->frc_sts.frame_cnt);
				devp->frc_sts.frame_cnt++;
			} else if (devp->frc_sts.frame_cnt > framedelay + 2 &&
					devp->frc_sts.frame_cnt <
					 framedelay * 2 + 2) {
				frc_frame_forcebuf_count(forceidx);
				frc_input_fid =
				READ_FRC_REG(FRC_REG_PAT_POINTER) >> 4 & 0xF;
				pr_frc(log, "b-e_freeze readidx %d, frm:%d\n",
					frc_input_fid, devp->frc_sts.frame_cnt);
				devp->frc_sts.frame_cnt++;
			} else if (devp->frc_sts.frame_cnt ==
					framedelay * 2 + 2) {
				frc_frame_forcebuf_enable(0);
				frc_state_change_finish(devp);
				pr_frc(log, "b-e stat_chg %s -> %s done",
						frc_state_ary[cur_state],
						frc_state_ary[new_state]);
				devp->frc_sts.frame_cnt = 0;
			} else if (devp->frc_sts.frame_cnt < framedelay + 2) {
				devp->frc_sts.frame_cnt++;
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

int frc_memc_set_level(u8 level)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *pfw_data;

	if (!devp)
		return 0;
	if (!devp->probe_ok)
		return 0;
	if (!devp->fw_data)
		return 0;
	pfw_data = (struct frc_fw_data_s *)devp->fw_data;
	pr_frc(1, "set_memc_level:%d\n", level);
	pfw_data->frc_top_type.frc_memc_level = level;
	if (pfw_data->frc_memc_level)
		pfw_data->frc_memc_level(pfw_data);
	return 1;
}

int frc_fpp_memc_set_level(u8 level, u8 num)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *pfw_data;

	if (!devp || !devp->probe_ok || !devp->fw_data)
		return 0;
	pfw_data = (struct frc_fw_data_s *)devp->fw_data;
	pfw_data->frc_top_type.frc_memc_level = level;
	if (num == 1) {
		pfw_data->frc_top_type.frc_memc_level_1 = 1;
		pr_frc(1, "fpp enhancement!\n");
	} else {
		pfw_data->frc_top_type.frc_memc_level_1 = 0;
		pr_frc(1, "fpp frc_memc_level_1 0\n");
	}
	pr_frc(1, "fpp_set_memc_level:%d\n", level);
	if (pfw_data->frc_memc_level)
		pfw_data->frc_memc_level(pfw_data);

	return 1;
}

int frc_memc_set_demo(u8 setdemo)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *pfw_data;
	u32 tmpstart = 0, tmpend = 0;

	if (!devp)
		return 0;
	if (!devp->probe_ok)
		return 0;
	if (!devp->fw_data)
		return 0;
	pfw_data = (struct frc_fw_data_s *)devp->fw_data;
	pr_frc(1, "set_demo_mode:%d\n", setdemo);
	pr_frc(1, "in_hsize:%4d\n",  pfw_data->frc_top_type.hsize);
	pr_frc(1, "in_vsize:%4d\n",  pfw_data->frc_top_type.vsize);
	pr_frc(1, "out_hsize:%4d\n",  pfw_data->frc_top_type.out_hsize);
	pr_frc(1, "out_vsize:%4d\n",  pfw_data->frc_top_type.out_vsize);
	if (setdemo == 0) {
		WRITE_FRC_BITS(FRC_MC_DEMO_WINDOW, 0, 3, 1);
		WRITE_FRC_BITS(FRC_REG_MC_DEBUG1, 0, 17, 1);
	} else if (setdemo < 3) {
		tmpstart = pfw_data->frc_top_type.hsize / 2 << 16;
		pr_frc(1, "demo_win_start:%4d\n", tmpstart);
		WRITE_FRC_REG(FRC_REG_DEMOWINDOW1_XYXY_ST, tmpstart);
		tmpend = ((pfw_data->frc_top_type.hsize - 1) << 16) +
				(pfw_data->frc_top_type.vsize - 1);
		pr_frc(1, "demo_win_end:%4d\n", tmpend);
		WRITE_FRC_REG(FRC_REG_DEMOWINDOW1_XYXY_ED, tmpend);
		WRITE_FRC_BITS(FRC_REG_MC_DEBUG1, (setdemo - 1), 17, 1);
		WRITE_FRC_BITS(FRC_MC_DEMO_WINDOW, 1, 3, 1);
	} else if (setdemo < 5) {
		WRITE_FRC_REG(FRC_REG_DEMOWINDOW1_XYXY_ST, 0);
		tmpend = ((pfw_data->frc_top_type.hsize / 2 - 1) << 16) +
				(pfw_data->frc_top_type.vsize - 1);
		pr_frc(1, "demo_win_end:%4d\n", tmpend);
		WRITE_FRC_REG(FRC_REG_DEMOWINDOW1_XYXY_ED, tmpend);
		WRITE_FRC_BITS(FRC_REG_MC_DEBUG1, (setdemo - 3), 17, 1);
		WRITE_FRC_BITS(FRC_MC_DEMO_WINDOW, 1, 3, 1);
	}
	return 1;
}

int frc_init_out_line(void)
{
	u32 vfb = 0;

	vfb = aml_read_vcbus_s(ENCL_VIDEO_VAVON_BLINE);
	if (vfb)
		vfb = (vfb / 4) * 3;  // 3/4 point of front vblank, default
	return vfb;
}

void frc_vpp_vs_ir_chk_film(struct frc_dev_s *frc_devp)
{
	if (!frc_devp->probe_ok || !frc_devp->power_on_flag)
		return;
	if (frc_devp->ud_dbg.res0_dbg_en == 1) {
		if (!frc_devp->frc_fw_pause)
			frc_devp->frc_fw_pause = 1;
		pr_frc(6, "vscnt=%7d, glb=%10d, cntglb=%7d\n",
			frc_devp->frc_sts.vs_cnt,
			READ_FRC_REG(FRC_FD_DIF_GL),
			READ_FRC_REG(FRC_FD_DIF_COUNT_GL));
	} // restore prev setting ,need first set fw_pause=0 by
	// echo frc_pause 0 > /sys/class/frc/debug
}

int frc_tell_alg_vendor(u8 vendor_info)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_alg_ctrl_s *pfrc_fw_alg_ctrl;
	struct frc_fw_data_s *pfw_data;

	if (!devp)
		return 0;
	if (!devp->probe_ok)
		return 0;
	if (!devp->fw_data)
		return 0;
	pfw_data = (struct frc_fw_data_s *)devp->fw_data;
	pfrc_fw_alg_ctrl = (struct frc_fw_alg_ctrl_s *)&pfw_data->frc_fw_alg_ctrl;
	pr_frc(1, "tell_alg_vendor:0x%x\n", vendor_info);
	if (pfrc_fw_alg_ctrl->frc_algctrl_u8vendor != vendor_info)
		pfrc_fw_alg_ctrl->frc_algctrl_u8vendor = vendor_info;
	// if (pfw_data->frc_fw_ctrl_if)  // change to output isr
	//	pfw_data->frc_fw_ctrl_if(pfw_data);
	return 1;
}

int frc_set_memc_fallback(u8 fbvale)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_alg_ctrl_s *pfrc_fw_alg_ctrl;
	struct frc_fw_data_s *pfw_data;

	if (!devp)
		return 0;
	if (!devp->probe_ok)
		return 0;
	if (!devp->fw_data)
		return 0;
	pfw_data = (struct frc_fw_data_s *)devp->fw_data;
	pfrc_fw_alg_ctrl = (struct frc_fw_alg_ctrl_s *)&pfw_data->frc_fw_alg_ctrl;
	pr_frc(1, "set mc fallback:0x%x\n", fbvale);
	pfrc_fw_alg_ctrl->frc_algctrl_u8mcfb = (fbvale > 20) ? 20 : fbvale;
	if (pfw_data->frc_fw_ctrl_if)
		pfw_data->frc_fw_ctrl_if(pfw_data);
	return 1;
}

int frc_set_film_support(u32 filmcnt)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_alg_ctrl_s *pfrc_fw_alg_ctrl;
	struct frc_fw_data_s *pfw_data;

	if (!devp)
		return 0;
	if (!devp->probe_ok)
		return 0;
	if (!devp->fw_data)
		return 0;
	pfw_data = (struct frc_fw_data_s *)devp->fw_data;
	pfrc_fw_alg_ctrl = (struct frc_fw_alg_ctrl_s *)&pfw_data->frc_fw_alg_ctrl;
	pr_frc(1, "set support film:0x%x\n", filmcnt);
	pfrc_fw_alg_ctrl->frc_algctrl_u32film = filmcnt;
	if (pfw_data->frc_fw_ctrl_if)
		pfw_data->frc_fw_ctrl_if(pfw_data);
	return 1;
}

static int notify_frc_signal_to_amvideo(int *char_flash_check)
{
	static int pre_char_flash_check;

//#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	if (pre_char_flash_check != *char_flash_check) {
		pr_frc(2, "char_flash_check = %d\n", *char_flash_check);
		pre_char_flash_check = *char_flash_check;
		amvideo_notifier_call_chain
			(AMVIDEO_UPDATE_FRC_CHAR_FLASH,
			(void *)char_flash_check);
	}
//#endif
	return 0;
}

// fix frc char flashing
void frc_char_flash_check(void)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *pfw_data;
	static u16 match_count;
	int char_flash_check;
	int temp, temp2;

	if (!devp || !devp->probe_ok || !devp->fw_data)
		return;
	if (devp->in_sts.high_freq_en) {
		if (devp->in_sts.high_freq_flash) {
			char_flash_check = 0;
			devp->in_sts.high_freq_flash = char_flash_check;
			notify_frc_signal_to_amvideo(&char_flash_check);
		}
		return;
	}
	pfw_data = (struct frc_fw_data_s *)devp->fw_data;
	/*4K input/output && memc on && glb_motion && hist*/
	if (pfw_data->frc_top_type.hsize == 3840 &&
		pfw_data->frc_top_type.vsize == 2160 &&
		devp->frc_sts.state == 1) {
		temp = READ_FRC_REG(FRC_BBD_RO_MAX1_HIST_CNT);
		temp2 = READ_FRC_REG(FRC_BBD_RO_MAX2_HIST_CNT);
		temp = temp + temp2;
		if ((temp > MIN_HIST1 && temp < MAX_HIST1) ||
			(temp > MIN_HIST2 && temp < MAX_HIST2)) {
			match_count++;
			if (match_count > LIMIT * 2)
				match_count = LIMIT * 2;
		} else {
			match_count--;
			if (match_count < 1)
				match_count = 1;
		}

		if (match_count > LIMIT)
			char_flash_check = 1;
		else
			char_flash_check = 0;
		devp->in_sts.high_freq_flash = char_flash_check;
		notify_frc_signal_to_amvideo(&char_flash_check);
	}
}

/*
 * input vf devp
 * output status(is tvin? source changed? fps)
 */
void frc_chk_vd_sts_chg(struct frc_dev_s *devp, struct vframe_s *vf)
{
	static u8 frc_is_tvin_s, frc_source_chg_s;

	if (!vf)
		return;
	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI ||
		vf->source_type == VFRAME_SOURCE_TYPE_CVBS ||
		vf->source_type == VFRAME_SOURCE_TYPE_TUNER)
		devp->in_sts.frc_is_tvin = true;
	else
		devp->in_sts.frc_is_tvin = false;

	if (frc_is_tvin_s != devp->in_sts.frc_is_tvin) {
		frc_is_tvin_s = devp->in_sts.frc_is_tvin;
		pr_frc(1, "input change %d. (1:tvin)\n", frc_is_tvin_s);
		if (devp->in_sts.frc_is_tvin) {
			devp->buf.buf_ctrl = 1;
			schedule_work(&frc_mem_dyc_proc);
		}
	}

	if (vf->vc_private) {
		if (vf->vc_private->flag & VC_FLAG_FIRST_FRAME)
			devp->in_sts.frc_source_chg = true;
		else
			devp->in_sts.frc_source_chg = false;

		if (frc_source_chg_s != devp->in_sts.frc_source_chg) {
			pr_frc(1, "input source change [%d->%d]. (1:changed)\n",
				frc_source_chg_s, devp->in_sts.frc_source_chg);
			frc_source_chg_s = devp->in_sts.frc_source_chg;
		}
	}

	if (frc_source_chg_s != devp->in_sts.frc_source_chg) {
		pr_frc(1, "input source change [%d->%d]. (1:changed)\n",
			frc_source_chg_s, devp->in_sts.frc_source_chg);
		frc_source_chg_s = devp->in_sts.frc_source_chg;
	}

	if (vf->vc_private) {
		pr_frc(6, "last vf disp vsync count =%d\n",
			vf->vc_private->last_disp_count);
		devp->in_sts.frc_last_disp_count =
			vf->vc_private->last_disp_count;
	}
	// every vframe detect frame rate
	frc_check_vf_rate(vf->duration, devp);
}

u16 frc_check_film_mode(struct frc_dev_s *frc_devp)
{
	if (frc_devp->frc_sts.state == FRC_STATE_ENABLE)
		frc_devp->film_mode = READ_FRC_REG(FRC_REG_PHS_TABLE) >> 8 & 0xFF;
	else
		frc_devp->film_mode = EN_VIDEO;
	return (u16)(frc_devp->film_mode);
}

