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

#define FrameLockPR(fmt, args...)      pr_info("FrameLock: " fmt "", ## args)
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

static unsigned int vrrlock_support = VRRLOCK_SUP_MODE;
static unsigned int vrr_dis_cnt_no_vf_limit = 5;
static unsigned int vrr_outof_rge_cnt = 10;

struct vrr_sig_sts frame_sts = {
	.vrr_support = true,
	.vrr_pre_en = 0,
	.vrr_en = 0,
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

unsigned int frame_lock_show_vout_framerate(void)
{
	unsigned int fr = 0;

	#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	fr = vout_frame_rate_measure();
	#endif

	return fr;
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
		frame_sts.vrr_en = 1;
		if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG)
			FrameLockPR("%s FRAME_LOCK_EVENT_ON vrr_en:%d\n",
			__func__, frame_sts.vrr_en);
		break;
	case FRAME_LOCK_EVENT_OFF:
		frame_sts.vrr_en = 0;
		if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG)
			FrameLockPR("%s FRAME_LOCK_EVENT_OFF vrr_en:%d\n",
			__func__, frame_sts.vrr_en);
		break;
	case VRR_EVENT_UPDATE:
		frame_sts.vrr_frame_out_fps_min = vrr_data->dev_vfreq_min;
		frame_sts.vrr_frame_out_fps_max = vrr_data->dev_vfreq_max;
		if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG)
			FrameLockPR("%s vrr_frame_out_fps_min:%d  vrr_frame_out_fps_max:%d\n",
			__func__, frame_sts.vrr_frame_out_fps_min,
			frame_sts.vrr_frame_out_fps_max);
		break;
	default:
		break;
	}

	return 0;
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
		FrameLockPR("%s vrr_frame_cur:%d fr_adj_type:%d ret:%d\n",
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
		FrameLockPR("lock_status = %d\n", ret);

	return ret;
}

int frame_lock_calc_lcnt_variance_val(struct vframe_s *vf)
{
	int i;
	int lcnt_reg_status;

	if (!vf) {
		FrameLockERR("vf NULL,return!!!\n");
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
		FrameLockPR("lcnt_reg_val:0x%x  lcnt_reg_status[%d] = %d\n",
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
			FrameLockPR("avg_val:%d vrr_lcnt_variance = %d\n",
				avg_val, vrr_lcnt_variance);
		return 0;
	}
	frame_in_cnt++;

	return 0;
}

int frame_lock_frame_rate_check(struct vinfo_s *vinfo)
{
	int ret = false;

	if (!vinfo)
		return ret;

	if (frame_lock_check_freerun_mode(vinfo)) {
		ret = true;
		frame_sts.vrr_frame_outof_range_cnt = 0;

		if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG)
			FrameLockPR("%s freerun mode fps_cur:%d  fr_adj_type:%d\n",
				__func__, frame_sts.vrr_frame_cur,
				vinfo->fr_adj_type);

		return ret;
	}

	if (frame_sts.vrr_frame_cur >= frame_sts.vrr_frame_out_fps_min &&
		frame_sts.vrr_frame_cur <= frame_sts.vrr_frame_out_fps_max) {
		frame_sts.vrr_frame_outof_range_cnt = 0;
		ret = true;
	} else {
		if (frame_sts.vrr_frame_outof_range_cnt < vrr_outof_rge_cnt)
			frame_sts.vrr_frame_outof_range_cnt++;
		ret = false;
	}

	if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG)
		FrameLockPR("%s fps_cur:%d out_min:%d out_max:%d outof_cnt:%d fr_adj_type:%d\n",
			__func__, frame_sts.vrr_frame_cur,
			frame_sts.vrr_frame_out_fps_min, frame_sts.vrr_frame_out_fps_max,
			frame_sts.vrr_frame_outof_range_cnt, vinfo->fr_adj_type);

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
			ret_hz = (96000 + duration / 2) / duration;
	} else if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
	   (vrrlock_support & VRRLOCK_SUPPORT_CVBS)) {
		ret_hz = 0;
	}

	if (duration == 0)
		return ret_hz;

	if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG) {
		fr = frame_lock_show_vout_framerate();
		FrameLockPR("in_fps: %d.%3d out_fps: %d.%3d duration: %d  no_vf_limit: %d\n",
		((96000 + duration / 2) / duration), ((96000 + duration / 2) % duration),
		(fr / 1000), (fr % 1000), duration, vrr_dis_cnt_no_vf_limit);
	}

	return ret_hz;
}

void vrrlock_process(struct vframe_s *vf,
		   struct vpp_frame_par_s *cur_video_sts)
{
	u16 vrr_en = frame_sts.vrr_en;

	if (vrr_en)
		frame_lock_calc_lcnt_variance_val(vf);

	if (frame_sts.vrr_frame_sts != frame_sts.vrr_frame_pre_sts) {
		if (frame_sts.vrr_frame_sts == FRAMELOCK_VRRLOCK) {
			vlock_set_sts_by_frame_lock(false);
			aml_vrr_atomic_notifier_call_chain(FRAME_LOCK_EVENT_VRR_ON_MODE, &vrr_en);
		} else {
			aml_vrr_atomic_notifier_call_chain(FRAME_LOCK_EVENT_VRR_OFF_MODE, &vrr_en);
			vlock_set_sts_by_frame_lock(true);
		}
	}

	if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG)
		FrameLockPR("vrr_frame_sts:%d vrr_frame_pre_sts:%d vlock_en:%d\n",
			frame_sts.vrr_frame_sts,
			frame_sts.vrr_frame_pre_sts,
			vlock_en);
}

void frame_lock_disable_vrr(bool en)
{
	bool vrr_switch = en;

	if (!vrr_switch) {
		aml_vrr_atomic_notifier_call_chain(FRAME_LOCK_EVENT_VRR_OFF_MODE, &vrr_switch);
		vlock_set_sts_by_frame_lock(true);
	}
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
			FrameLockPR("%s small window, VRR disable!!!\n",
			__func__);
		return FRAMELOCK_VLOCK;
	}

	if (frame_sts.vrr_en) {
		frame_sts.vrr_frame_cur = frame_lock_check_input_hz(vf);

		if (frame_lock_frame_rate_check(vinfo) &&
			frame_sts.vrr_frame_outof_range_cnt < vrr_outof_rge_cnt) {
			ret = FRAMELOCK_VRRLOCK;
		} else {
			ret = FRAMELOCK_VLOCK;
		}
	} else {
		ret = FRAMELOCK_VLOCK;
	}
	frame_sts.vrr_frame_sts = ret;

	return ret;
}

/*
 * Summary:
 *
 * Parameters:
 *
 * Returns:
 *
 * Remarks:
 *
 */

void frame_lock_process(struct vframe_s *vf,
		   struct vpp_frame_par_s *cur_video_sts)
{
	u16 lock_type = 0;

	if (probe_ok == 0) {
		FrameLockERR("amvecm probe_ok = 0, return!!!\n");
		return;
	}

	if (!frame_lock_en) {
		if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG) {
			FrameLockPR("%s frame_lock_en:%d\n",
				__func__, frame_lock_en);
		}
		return;
	}

	lock_type = frame_lock_check_lock_type(cur_video_sts, vf);

	if (frame_lock_debug & VRR_POLICY_DEBUG_FLAG) {
		FrameLockPR("%s lock_type:%d\n",
			__func__, lock_type);
	}

	switch (lock_type) {
	case FRAMELOCK_VRRLOCK:
		vrrlock_process(vf, cur_video_sts);
		break;
	case FRAMELOCK_VLOCK:
		if (frame_sts.vrr_frame_pre_sts != lock_type)
			frame_lock_disable_vrr(false);
		vlock_process(vf, cur_video_sts);
		break;
	default:
		break;
	}

	frame_sts.vrr_frame_pre_sts = lock_type;
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
	pr_info("\n frame_lock_version = %s", FRAME_LOCK_POLICY_VERSION);
	pr_info("\n frame_lock_en = %d", frame_lock_en);
	pr_info("\n vrr_support = %d", frame_sts.vrr_support);
	pr_info("\n vrr_en = %d", frame_sts.vrr_en);
	pr_info("\n vrr_frame_in_fps_min = %d", frame_sts.vrr_frame_in_fps_min);
	pr_info("\n vrr_frame_in_fps_max = %d", frame_sts.vrr_frame_in_fps_max);
	pr_info("\n vrr_frame_out_fps_min = %d", frame_sts.vrr_frame_out_fps_min);
	pr_info("\n vrr_frame_out_fps_maxs = %d", frame_sts.vrr_frame_out_fps_max);

	return len;
}

