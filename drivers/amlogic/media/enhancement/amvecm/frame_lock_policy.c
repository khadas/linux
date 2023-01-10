// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/amlogic/media/vrr/vrr.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/vpp.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include "reg_helper.h"
#include "frame_lock_policy.h"

#define framelock_pr_info(fmt, args...)      pr_info("FrameLock: " fmt "", ## args)
#define FrameLockERR(fmt, args...)     pr_err("FrameLock ERR: " fmt "", ## args)

#define VRR_VARIANCE_CNT 10
#define VRR_POLICY_DEBUG_FLAG				BIT(0)
#define VRR_POLICY_VARIANCE_FLAG			BIT(1)
#define VRR_POLICY_LOCK_STATUS_DEBUG_FLAG	BIT(2)
#define VRR_POLICY_DEBUG_RANGE_FLAG			BIT(3)
#define VRR_POLICY_DEBUG_FREERUN_FLAG		BIT(4)

#define VRRLOCK_SUP_MODE	(VRRLOCK_SUPPORT_HDMI | VRRLOCK_SUPPORT_CVBS)

int frame_lock_debug;
module_param(frame_lock_debug, int, 0664);
MODULE_PARM_DESC(frame_lock_debug, "frame lock debug flg");

int sum1, sum2;
int avg_val;
int frame_in_cnt, vrr_lcnt_variance;
int lcnt_reg_val[VRR_VARIANCE_CNT] = {0};
unsigned int frame_lock_en = 1;
unsigned int vrr_priority;
unsigned int vrr_delay_line = 200;
unsigned int vrr_delay_line_50hz = 600;

static unsigned int vrrlock_support = VRRLOCK_SUP_MODE;
static unsigned int vrr_dis_cnt_no_vf_limit = 5;
static unsigned int vrr_outof_rge_cnt = 10;
static unsigned int vrr_skip_frame_cnt = 15;

static unsigned int vrr_display_mode_chg_cmd;
//static unsigned int vrr_display_mode_chg_cmd_pre;
static unsigned int vrr_mode_chg_skip_cnt = 10;

static struct completion vrr_off_done;

struct vrr_sig_sts frame_sts = {
	.vrr_support = false,
	.vrr_lfc_mode = false,
	.vrr_cinema_flg = true,
	.vrr_m_const_flg = false,
	.vrr_pre_en = 0,
	.vrr_en = 0,
	.vrr_policy = 0,
	.vrr_policy_pre = 0,
	.vrr_frame_sts = FRAMELOCK_INVALID,
	.vrr_frame_pre_sts = FRAMELOCK_INVALID,
	.vrr_frame_lock_type = 0,
	.vrr_frame_cur = 0,
	.vrr_frame_in_frame_cnt = 0,
	.vrr_frame_in_frame_cnt = 0,
	.vrr_frame_in_fps_min = 48,
	.vrr_frame_in_fps_max = 120,
	.vrr_frame_out_fps_min = 48,
	.vrr_frame_out_fps_max = 120,
};

void frame_lock_set_vrr_support_flag(bool support_flag)
{
	frame_sts.vrr_support = support_flag;
}

void frame_lock_param_config(struct device_node *node)
{
	unsigned int val;
	int ret;

	ret = of_property_read_u32(node, "vrr_priority", &val);
	if (!ret)
		vrr_priority = val;
}

unsigned int frame_lock_show_vout_framerate(void)
{
	unsigned int fr = 0;

	#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	fr = vout_frame_rate_measure(1);
	#endif

	return fr;
}

void frame_lock_vrr_off_done_init(void)
{
	init_completion(&vrr_off_done);
}

void frame_lock_mode_chg(unsigned int cmd)
{
	int ret;

	if (cmd == VOUT_EVENT_MODE_CHANGE) {
		vrr_mode_chg_skip_cnt = 10;
		vrr_display_mode_chg_cmd = cmd;
	} else if (cmd == VOUT_EVENT_MODE_CHANGE_PRE) {
		vrr_display_mode_chg_cmd = cmd;
		if (frame_sts.vrr_en) {
			reinit_completion(&vrr_off_done);
			ret = wait_for_completion_timeout(&vrr_off_done, msecs_to_jiffies(100));
			if (!ret)
				framelock_pr_info("%s: wait_for_completion_timeout\n", __func__);
		}
	}
}

void frame_lock_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

int flock_vrr_nfy_callback(struct notifier_block *block, unsigned long cmd,
			  void *para)
{
	struct vrr_notifier_data_s *vrr_data;

	vrr_data = (struct vrr_notifier_data_s *)para;

	if (!vrr_data)
		return 0;

	switch (cmd) {
	case FRAME_LOCK_EVENT_ON:
		if (frame_sts.vrr_support) {
			frame_sts.vrr_policy = vrr_data->vrr_mode;
			frame_sts.vrr_en = vrr_priority ? 1 : vrr_data->vrr_mode;
		} else {
			frame_sts.vrr_en = 0;
		}

		if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG)
			framelock_pr_info("%s FRAME_LOCK_EVENT_ON vrr_en:%d, line_dly:%d\n",
			__func__, frame_sts.vrr_en, vrr_data->line_dly);

		break;
	case FRAME_LOCK_EVENT_OFF:
		frame_sts.vrr_en = 0;

		if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG)
			framelock_pr_info("%s FRAME_LOCK_EVENT_OFF vrr_en:%d\n",
			__func__, frame_sts.vrr_en);

		break;
	case VRR_EVENT_UPDATE:
		frame_sts.vrr_frame_out_fps_min = vrr_data->dev_vfreq_min;
		frame_sts.vrr_frame_out_fps_max = vrr_data->dev_vfreq_max;

		if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG)
			framelock_pr_info("%s vrr_frame_out_fps_min:%d  vrr_frame_out_fps_max:%d\n",
			__func__, frame_sts.vrr_frame_out_fps_min,
			frame_sts.vrr_frame_out_fps_max);

		break;
	default:
		break;
	}

	return 0;
}

u16 frame_lock_get_vrr_status(void)
{
	return frame_sts.vrr_policy;//0:fixed frame rate,1:variable frame rate
}

u32 frame_lock_chk_is_small_win(struct vpp_frame_par_s *cur_video_sts, struct vinfo_s *vinfo)
{
	u32 scaler_vout;
	u32 panel_vout;

	if (!cur_video_sts || !vinfo)
		return 1;

	panel_vout = (vinfo->vtotal * 75) / 100;
	scaler_vout = cur_video_sts->VPP_vsc_endp -
		cur_video_sts->VPP_vsc_startp;

	if (scaler_vout < panel_vout &&
	    cur_video_sts->VPP_vsc_endp > cur_video_sts->VPP_vsc_startp)
		return 1;

	return 0;
}

bool frame_lock_check_freerun_mode(struct vinfo_s *vinfo)
{
	int ret = false;

	if (!vinfo)
		return ret;

	if (vinfo->fr_adj_type == VOUT_FR_ADJ_FREERUN &&
		frame_sts.vrr_frame_cur == 60)
		ret = true;
	else
		ret = false;

	if ((frame_lock_debug & VRR_POLICY_DEBUG_FREERUN_FLAG) &&
		vinfo->fr_adj_type == VOUT_FR_ADJ_FREERUN)
		framelock_pr_info("%s vrr_frame_cur:%d fr_adj_type:%d ret:%d\n",
			__func__, frame_sts.vrr_frame_cur,
			vinfo->fr_adj_type, ret);

	return ret;
}

bool frame_lock_vrr_lock_status(void)
{
	int ret = false;

	if (aml_vrr_state()/*&& vrr_lcnt_variance <= 5*/)
		ret = true;

	if (frame_lock_debug & VRR_POLICY_LOCK_STATUS_DEBUG_FLAG)
		framelock_pr_info("lock_status = %d\n", ret);

	return ret;
}

bool frame_lock_lfc_mode_check(void)
{
	bool ret = false;

	if (frame_sts.vrr_cinema_flg)
		if (frame_sts.vrr_m_const_flg)
			ret = true;
		else if (frame_sts.vrr_policy)
			ret = false;
		else
			ret = vrr_priority ? true : false;
	else if (frame_sts.vrr_policy)
		ret = false;
	else
		ret = vrr_priority ? true : false;

	return ret;
}

bool frame_lock_lfc_rate_check(struct vframe_s *vf, struct vinfo_s *vinfo)
{
	bool ret = false;

	if (!vf || !vinfo)
		return ret;

	if ((frame_sts.vrr_frame_cur == 50 && vinfo->std_duration == 100) ||
		(frame_sts.vrr_frame_cur == 60 && vinfo->std_duration == 120))
		ret = true;

	return ret;
}

int frame_lock_calc_lcnt_variance_val(struct vframe_s *vf)
{
	int i;
	int lcnt_reg_status;

	if (!vf) {
		//FrameLockERR("vf NULL,return!!!, cur_out_frame_rate:%d\n",
		//	frame_lock_show_vout_framerate());
		frame_in_cnt = 0;
		sum1 = 0;
		sum2 = 0;
		return -1;
	}

	lcnt_reg_status = READ_VPP_REG_EX(0x1204, 0);
	lcnt_reg_val[frame_in_cnt] = lcnt_reg_status & 0xffff;

	if (frame_in_cnt == 0)
		vrr_lcnt_variance = 0;

	if (frame_lock_debug & VRR_POLICY_VARIANCE_FLAG)
		framelock_pr_info("lcnt_reg_val:0x%x  lcnt_reg_status[%d] = %d\n",
			lcnt_reg_status, frame_in_cnt, lcnt_reg_val[frame_in_cnt]);

	if (frame_in_cnt >= VRR_VARIANCE_CNT - 1) {
		for (i = 0; i < VRR_VARIANCE_CNT; i++)
			sum1 += lcnt_reg_val[i];

		avg_val = sum1 / VRR_VARIANCE_CNT;

		for (i = 0; i < VRR_VARIANCE_CNT; i++)
			sum2 += (lcnt_reg_val[i] - avg_val) *
				(lcnt_reg_val[i] - avg_val);
		vrr_lcnt_variance = sum2 / VRR_VARIANCE_CNT;

		sum1 = 0;
		sum2 = 0;
		frame_in_cnt = 0;

		if (frame_lock_debug & VRR_POLICY_VARIANCE_FLAG)
			framelock_pr_info("avg_val:%d vrr_lcnt_variance = %d\n",
				avg_val, vrr_lcnt_variance);
		return 0;
	}
	frame_in_cnt++;

	return 0;
}

int frame_lock_frame_rate_check(struct vframe_s *vf, struct vinfo_s *vinfo)
{
	bool ret = false;

	if (!vinfo || !vf)
		return ret;

	if (frame_lock_check_freerun_mode(vinfo)) {
		ret = true;
		frame_sts.vrr_frame_outof_range_cnt = 0;

		if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG)
			framelock_pr_info("%s freerun mode fps_cur:%d  fr_adj_type:%d\n",
				__func__, frame_sts.vrr_frame_cur,
				vinfo->fr_adj_type);

		return ret;
	}

	if (frame_lock_lfc_mode_check() && frame_lock_lfc_rate_check(vf, vinfo)) {
		frame_sts.vrr_frame_outof_range_cnt = 0;
		frame_sts.vrr_lfc_mode = true;
		ret =  true;
	} else {
		if (frame_sts.vrr_frame_cur >= frame_sts.vrr_frame_out_fps_min &&
			frame_sts.vrr_frame_cur <= frame_sts.vrr_frame_out_fps_max) {
			if (frame_sts.vrr_frame_outof_range_cnt == vrr_outof_rge_cnt)
				ret = false;
			else
				ret = true;

			frame_sts.vrr_frame_outof_range_cnt = 0;
		} else {
			if (frame_sts.vrr_frame_outof_range_cnt < vrr_outof_rge_cnt) {
				frame_sts.vrr_frame_outof_range_cnt++;
				ret = true;
			} else {
				ret = false;
			}
			//frame_sts.vrr_lfc_mode = false;
		}
		frame_sts.vrr_lfc_mode = false;

		if (frame_lock_debug & VRR_POLICY_DEBUG_RANGE_FLAG)
			framelock_pr_info("%s fps_cur:%d o_min:%d o_max:%d outof_cnt:%d f_a_t:%d lfc:%d\n",
				__func__, frame_sts.vrr_frame_cur,
				frame_sts.vrr_frame_out_fps_min, frame_sts.vrr_frame_out_fps_max,
				frame_sts.vrr_frame_outof_range_cnt, vinfo->fr_adj_type,
				frame_sts.vrr_lfc_mode);
	}

	return ret;
}

static unsigned int frame_lock_check_input_hz(struct vframe_s *vf)
{
	unsigned int ret_hz = 0;
	unsigned int duration = 0;
	unsigned int fr = 0;

	if (!vf) {
		frame_sts.vrr_frame_in_frame_cnt++;
		return ret_hz;
	}

	duration = vf->duration;
	frame_sts.vrr_frame_in_frame_cnt = 0;

	if (vf->source_type != VFRAME_SOURCE_TYPE_CVBS &&
	    vf->source_type != VFRAME_SOURCE_TYPE_HDMI)
		ret_hz = 0;
	else if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI &&
		 (vrrlock_support & VRRLOCK_SUP_MODE)) {
		if (duration != 0)
			ret_hz = (96000 + 100) / duration;
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
	   (vrrlock_support & VRRLOCK_SUPPORT_CVBS)) {
		ret_hz = 0;
	}

	if (duration == 0)
		return ret_hz;

	if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG) {
		fr = frame_lock_show_vout_framerate();
		framelock_pr_info("in_fps: %d.%3d out_fps: %d.%3d duration: %d  no_vf_limit: %d\n",
		(96000 / duration), ((96000 * 100) / duration) % 100,
		(fr / 1000), (fr % 1000), duration, vrr_dis_cnt_no_vf_limit);
	}

	return ret_hz;
}

static void frame_lock_vrr_ctrl(bool en, struct vrr_notifier_data_s *data)
{
	if (en)
		aml_vrr_atomic_notifier_call_chain(FRAME_LOCK_EVENT_VRR_ON_MODE, data);
	else
		aml_vrr_atomic_notifier_call_chain(FRAME_LOCK_EVENT_VRR_OFF_MODE, data);
}

void frame_lock_disable_vrr(bool en)
{
	struct vrr_notifier_data_s vdata;

	vdata.line_dly = 500;

	aml_vrr_atomic_notifier_call_chain(FRAME_LOCK_EVENT_VRR_OFF_MODE, &vdata);
	vlock_set_sts_by_frame_lock(true);
}

u16 frame_lock_check_lock_type(struct vpp_frame_par_s *cur_video_sts, struct vframe_s *vf)
{
	int ret = FRAMELOCK_VLOCK;
	struct vinfo_s *vinfo = NULL;

	vinfo = get_current_vinfo();
	if (!vinfo)
		return FRAMELOCK_VLOCK;

	if (frame_lock_chk_is_small_win(cur_video_sts, vinfo)) {
		if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG)
			framelock_pr_info("%s small window, VRR disable!!!\n",
			__func__);

		return FRAMELOCK_VLOCK;
	}

	frame_sts.vrr_frame_cur = frame_lock_check_input_hz(vf);

	if (frame_sts.vrr_en) {
		if (frame_lock_frame_rate_check(vf, vinfo) &&
			frame_sts.vrr_frame_outof_range_cnt < vrr_outof_rge_cnt &&
			vrr_skip_frame_cnt == 0) {
			ret = FRAMELOCK_VRRLOCK;
		} else {
			if (vrr_skip_frame_cnt != 0)
				vrr_skip_frame_cnt--;
			else
				vrr_skip_frame_cnt = 5;
			ret = FRAMELOCK_VLOCK;
		}
	} else {
		ret = FRAMELOCK_VLOCK;
		vrr_skip_frame_cnt = 15;
	}
	frame_sts.vrr_frame_sts = ret;

	return ret;
}

void vrrlock_process(struct vframe_s *vf,
		   struct vpp_frame_par_s *cur_video_sts)
{
	u16 vrr_en = frame_sts.vrr_en;
	u32 cur_frame_rate = frame_sts.vrr_frame_cur;
	struct vrr_notifier_data_s vdata;
	struct vinfo_s *vinfo = NULL;
	int state;
	unsigned int ret_hz = 0;
	unsigned int duration = 0;

	duration = vf->duration;
	ret_hz = (96000 / duration);

	memset(&vdata, 0, sizeof(struct vrr_notifier_data_s));

	vinfo = get_current_vinfo();
	if (!vinfo)
		return;

	if (ret_hz == 50 || ret_hz == 100)
		vdata.line_dly = vrr_delay_line_50hz;
	else
		vdata.line_dly = vrr_delay_line;

	if (vrr_en) {
		frame_lock_calc_lcnt_variance_val(vf);

		if (frame_sts.vrr_switch_off) {
			aml_vrr_atomic_notifier_call_chain(VRR_EVENT_GET_STATE, &state);
			if (state == 0) {
				frame_sts.vrr_switch_off = 0;
				complete(&vrr_off_done);
			}
		}
		if (vrr_display_mode_chg_cmd == VOUT_EVENT_MODE_CHANGE_PRE) {
			vrr_display_mode_chg_cmd = 0;
			frame_sts.vrr_switch_off = 1;
			frame_lock_vrr_ctrl(false, &vdata);
		} else if (vrr_display_mode_chg_cmd == VOUT_EVENT_MODE_CHANGE) {
			if (vrr_mode_chg_skip_cnt > 0) {
				vrr_mode_chg_skip_cnt--;
			} else {
				vrr_display_mode_chg_cmd = 0;
				frame_lock_vrr_ctrl(true, &vdata);
			}
		} else if (vrr_display_mode_chg_cmd == 0) {
			if (frame_sts.vrr_frame_sts != frame_sts.vrr_frame_pre_sts) {
				if (frame_sts.vrr_frame_sts == FRAMELOCK_VRRLOCK) {
					vlock_set_sts_by_frame_lock(false);
					frame_lock_vrr_ctrl(true, &vdata);
				} else {
					frame_lock_vrr_ctrl(false, &vdata);
					vlock_set_sts_by_frame_lock(true);
				}
			}
		}
	}

	if (frame_sts.vrr_lfc_mode) {
		vdata.vrr_policy = 0;

		if (frame_sts.vrr_frame_sts != frame_sts.vrr_frame_pre_sts ||
			frame_sts.vrr_policy_pre != frame_sts.vrr_policy) {
			if (frame_sts.vrr_frame_sts == FRAMELOCK_VRRLOCK) {
				vlock_set_sts_by_frame_lock(false);

				aml_vrr_atomic_notifier_call_chain(VRR_EVENT_LFC_ON,
					&cur_frame_rate);

				aml_vrr_atomic_notifier_call_chain(FRAME_LOCK_EVENT_VRR_ON_MODE,
					&vdata);
			} else {
				aml_vrr_atomic_notifier_call_chain(VRR_EVENT_LFC_OFF,
					&cur_frame_rate);

				aml_vrr_atomic_notifier_call_chain(FRAME_LOCK_EVENT_VRR_OFF_MODE,
					&vdata);

				vlock_set_sts_by_frame_lock(true);
			}
		}
	}

	if (frame_lock_debug & VRR_POLICY_LOCK_STATUS_DEBUG_FLAG)
		framelock_pr_info("vrr_frame_sts:%d vrr_frame_pre_sts:%d vlock_en:%d lfc:%d policy_pre:%d policy:%d",
			frame_sts.vrr_frame_sts,
			frame_sts.vrr_frame_pre_sts,
			vlock_en,
			frame_sts.vrr_lfc_mode,
			frame_sts.vrr_policy_pre,
			frame_sts.vrr_policy);
}

void frame_lock_process(struct vframe_s *vf,
		   struct vpp_frame_par_s *cur_video_sts)
{
	if (probe_ok == 0) {
		return;
	}

	if (!frame_lock_en) {
		if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG) {
			framelock_pr_info("%s frame_lock_en:%d\n",
				__func__, frame_lock_en);
		}
		return;
	}

	frame_sts.vrr_frame_lock_type =
		frame_lock_check_lock_type(cur_video_sts, vf);

	if (frame_lock_debug & VRR_POLICY_LOCK_STATUS_DEBUG_FLAG) {
		framelock_pr_info("%s lock_type:%d vrr_skip_frame_cnt:%d\n",
			__func__, frame_sts.vrr_frame_lock_type, vrr_skip_frame_cnt);
	}

	switch (frame_sts.vrr_frame_lock_type) {
	case FRAMELOCK_VRRLOCK:
		vrrlock_process(vf, cur_video_sts);
		break;
	case FRAMELOCK_VLOCK:
		if (frame_sts.vrr_frame_pre_sts != frame_sts.vrr_frame_lock_type)
			frame_lock_disable_vrr(false);
		vlock_process(vf, cur_video_sts);
		break;
	default:
		break;
	}

	frame_sts.vrr_frame_pre_sts = frame_sts.vrr_frame_lock_type;
	frame_sts.vrr_policy_pre = frame_sts.vrr_policy;
}

ssize_t frame_lock_debug_store(struct class *cla,
			  struct class_attribute *attr,
		const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 1;

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	frame_lock_parse_param(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "fl_en", 5)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		frame_lock_en = val;
		pr_info("\n frame_lock_en = %d\n", frame_lock_en);
	} else if (!strncmp(parm[0], "cinemavrr", 9)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		frame_sts.vrr_cinema_flg = val;
		pr_info("\n vrr_cinema_flg = %d\n", frame_sts.vrr_cinema_flg);
	}  else if (!strncmp(parm[0], "m_const", 7)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		frame_sts.vrr_m_const_flg = val;
		pr_info("\n vrr_m_const_flg = %d\n", frame_sts.vrr_m_const_flg);
	} else if (!strncmp(parm[0], "delay_line", 10)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		vrr_delay_line = val;
		frame_sts.vrr_frame_lock_type = FRAMELOCK_INVALID;
		pr_info("\n vrr_delay_line = %d\n", vrr_delay_line);
	} else if (!strncmp(parm[0], "delay_line_50hz", 10)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			return -EINVAL;
		vrr_delay_line_50hz = val;
		frame_sts.vrr_frame_lock_type = FRAMELOCK_INVALID;
		pr_info("\n vrr_delay_line_50hz = %d\n", vrr_delay_line_50hz);
	} else {
		pr_info("\n frame lock debug cmd invalid\n");
	}

	return count;
}

ssize_t frame_lock_debug_show(struct class *cla,
			 struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	frame_sts.vrr_frame_in_fps_max = frame_sts.vrr_frame_out_fps_max;
	pr_info("frame_lock_version = %s", FRAME_LOCK_POLICY_VERSION);
	pr_info("frame_lock_en = %d", frame_lock_en);
	pr_info("vrr_support = %d", frame_sts.vrr_support);
	pr_info("vrr_en = %d", frame_sts.vrr_en);
	pr_info("vrr_frame_in_fps_min = %d", frame_sts.vrr_frame_in_fps_min);
	pr_info("vrr_frame_in_fps_max = %d", frame_sts.vrr_frame_in_fps_max);
	pr_info("vrr_frame_out_fps_min = %d", frame_sts.vrr_frame_out_fps_min);
	pr_info("vrr_frame_out_fps_maxs = %d", frame_sts.vrr_frame_out_fps_max);
	pr_info("vrr_cinema_flg = %d", frame_sts.vrr_cinema_flg);
	pr_info("vrr_m_const_flg = %d", frame_sts.vrr_m_const_flg);
	pr_info("vrr_lfc_mode = %d", frame_sts.vrr_lfc_mode);
	pr_info("vrr_frame_sts = %d", frame_sts.vrr_frame_sts);
	pr_info("vrr_frame_lock_type = %d", frame_sts.vrr_frame_lock_type);
	pr_info("vrr_frame_cur = %d", frame_sts.vrr_frame_cur);
	pr_info("\n");

	return len;
}

