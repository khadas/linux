// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "vpp_pq.h"
#include <linux/types.h>
#include <linux/amlogic/media/amvecm/amvecm.h>

/*BLUE_SCENE             default 0*/
/*GREEN_SCENE            default 0*/
/*SKIN_TONE_SCENE        default 0*/
/*PEAKING_SCENE          default 100*/
/*SATURATION_SCENE       default 30*/
/*DYNAMIC_CONTRAST_SCENE default 0*/
/*NOISE_SCENE            default 0*/

/*
 * vpp_pq
 * 0 faceskin
 * 1 bluesky
 * 2 food
 * 3 architecture
 * 4 grass
 * 5 nightscop
 * 6 waterside
 * 7 flowers
 * 8 breads
 * 9 fruits
 * 10 meats
 * 11 document
 * 12 ocean
 * 13 pattern
 * 14 group
 * 15 animals
 * 16 iceskate
 * 17 leaves
 * 18 racetrack
 * 19 fireworks
 * 20 waterfall
 * 21 beach
 * 22 snows
 * 23 sunset
 * 24 default setting
 */
int vpp_pq_data[AI_SCENES_MAX][SCENES_VALUE] = {
	{0, 0, 25, 15, 15, 0, 0, 0, 0, 0},/*faceskin*/
	{30, 0, 0, 0, 10, 0, 0, 0, 0, 0},/*bluesky*/
	{0, 0, 0, 15, 20, 0, 0, 0, 0, 0},/*foods*/
	{0, 0, 0, 25, 0, 2, 0, 0, 0, 0},/*architecture*/
	{0, 30, 0, 15, 10, 0, 0, 0, 0, 0},/*grass*/
	{0, 0, 0, 0, 0, 3, 0, 0, 0, 0},/*nightscop*/
	{0, 0, 0, 20, 0, 2, 0, 0, 0, 0},/*document*/
	{0, 0, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 70, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 70, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 80, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 80, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 30, 0, 0, 0, 0, 0},
	{0, 0, 0, 100, 30, 0, 0, 0, 0, 0}
};

static unsigned int det_stb_cnt = 30;
static unsigned int det_unstb_cnt = 20;
static unsigned int tolrnc_cnt = 6;
static unsigned int timer_filter_en;

/*aipq policy:
 * 0 default policy from algorithm, owner gumin
 * 1: use top 3 blend offset, with timer filter which can select as policy
 */
static unsigned int aipq_set_policy;
static unsigned int color_th = 100;

/*scene_prob[0]: scene, scene_prob[1]: prob*/
int scene_prob[2] = {0, 0};
struct ai_pq_hist_data aipq_hist_data = {
	.pre_skin_pct = 0,
	.pre_green_pct = 0,
	.pre_blue_pct = 0,
	.cur_skin_pct = 0,
	.cur_green_pct = 0,
	.cur_blue_pct = 0
};

/*scene change th: 1/2 scene diff*/
static u32 sc_th = 512;

enum iir_policy_e aipq_tiir_policy_proc(int (*prob)[2], int sc_chg,
					int *pq_debug, int *kp_flag)
{
	static int stable_cnt, unstb_cnt;
	static int pre_det_sc = -1, pre_det_sec = -1, pre_det_thd = -1;
	enum iir_policy_e policy, ret_plcy;
	int prob_sum_2 = 0, prob_sum_3 = 0;
	static enum aipq_state_mach state_mach = AIPQ_IDLE;
	int i;

	for (i = 0; i < 3; i++) {
		prob_sum_3 += prob[i][1];
		if (i < 2)
			prob_sum_2 += prob[i][1];
	}

	if ((pre_det_sc == prob[0][0] && prob[0][1] >= 5000) ||
	    (pre_det_sc == prob[0][0] && pre_det_sec == prob[1][0] &&
	     prob_sum_2 >= 7000) ||
	    (pre_det_sc == prob[0][0] && pre_det_sec == prob[1][0] &&
	     pre_det_thd == prob[2][0] && prob_sum_3 >= 8000)) {
		policy = SC_DETECTED;
	} else {
		policy = SC_INVALID;
	}

	switch (state_mach) {
	case AIPQ_IDLE:
		if (policy == SC_DETECTED) {
			stable_cnt++;
			unstb_cnt = 0;
			state_mach++;
		}
		/*set pre detected scene*/
		pre_det_sc = prob[0][0];
		pre_det_sec = prob[1][0];
		pre_det_thd = prob[2][0];
		ret_plcy = SC_INVALID;
		break;
	case AIPQ_DET_UNSTABLE:
		if (policy == SC_DETECTED)
			stable_cnt++;
		else
			unstb_cnt++;

		det_stb_cnt = get_stb_cnt();
		tolrnc_cnt = get_tolrnc_cnt();

		if (unstb_cnt >= tolrnc_cnt) {
			state_mach--;
			unstb_cnt = 0;
			stable_cnt = 0;
		} else if (stable_cnt >= det_stb_cnt) {
			state_mach++;
			unstb_cnt = 0;
		}
		ret_plcy = SC_INVALID;
		break;
	case AIPQ_DET_STATBLE:
		if (policy == SC_INVALID) {
			unstb_cnt++;
			*kp_flag = 1;
		} else {
			if (unstb_cnt > 0)
				unstb_cnt--;
		}

		det_unstb_cnt = get_unstb_cnt();

		if (unstb_cnt >= det_unstb_cnt) {
			state_mach = AIPQ_IDLE;
			stable_cnt = 0;
			unstb_cnt = 0;
		}
		ret_plcy = SC_DETECTED;
		break;
	default:
		ret_plcy = SC_INVALID;
		break;
	}

	if (pq_debug[2] > 0x10)
		pr_info("%s: cur policy = %d, stable_cnt = %d, unstable_cnt = %d, iir policy = %d\n",
			__func__, policy, stable_cnt, unstb_cnt, ret_plcy);

	return ret_plcy;
}

int aipq_scs_bld_proc(int (*cfg)[SCENES_VALUE], int (*prob)[2],
		      int *out, int *pq_debug)
{
	int i, j;
	int a = 0;
	int m;
	int sc[3];
	int sc_pr[3];
	enum iir_policy_e policy;
	static int pre_out[SCENES_VALUE];
	int kp_flag = 0;

	memset(out, 0, sizeof(int) * SCENES_VALUE);
	timer_filter_en = get_timer_filter_en();

	if (!timer_filter_en) {
		/*prob 10000 as 1.0*/
		if (prob[0][1] >= 8000) {
			m = prob[0][0];
			memcpy(out, cfg[m], sizeof(int) * SCENES_VALUE);
		} else {
			for (i = 0; i < 3; i++)
				a += prob[i][1];

			if (a >= 4500 && prob[0][1] >= 3000) {
				for (i = 0; i < 3; i++) {
					sc[i] = prob[i][0];
					sc_pr[i] = prob[i][1] << 10;
					sc_pr[i] = div64_s64(sc_pr[i], a);
				}

				for (i = 0; i < 3; i++) {
					for (j = 0; j < SCENE_MAX; j++)
						out[j] +=
						(*(cfg + sc[i]))[j] * sc_pr[i] >> 10;
				}
			} else {
				if (pq_debug[2] > 0x10)
					pr_info("sum of top3 prob < 50 percent or top1 prob < 30 per\n");
			}
		}
		return 0;
	}

	policy = aipq_tiir_policy_proc(prob, 0, pq_debug, &kp_flag);

	switch (policy) {
	case SC_DETECTED:
		if (kp_flag) {
			memcpy(out, pre_out, sizeof(int) * SCENES_VALUE);
			if (pq_debug[2] > 0x10)
				pr_info("same policy, keep setting\n");
			break;
		}
		/*prob 10000 as 1.0*/
		if (prob[0][1] >= 8000) {
			m = prob[0][0];
			memcpy(out, cfg[m], sizeof(int) * SCENES_VALUE);
		} else {
			for (i = 0; i < 3; i++)
				a += prob[i][1];

			for (i = 0; i < 3; i++) {
				sc[i] = prob[i][0];
				sc_pr[i] = prob[i][1] << 10;
				sc_pr[i] = div64_s64(sc_pr[i], a);
			}

			for (i = 0; i < 3; i++) {
				for (j = 0; j < SCENE_MAX; j++)
					out[j] +=
					(*(cfg + sc[i]))[j] * sc_pr[i] >> 10;
			}
		}

		memcpy(pre_out, out, sizeof(int) * SCENES_VALUE);

		if (pq_debug[2] > 0x10) {
			pr_info("top1 prob = %d: bld value: %d, %d, %d, %d, %d, %d, %d\n",
				prob[0][1], out[0], out[1], out[2], out[3],
				out[4], out[5], out[6]);
		}
		break;
	case SC_INVALID:
		if (pq_debug[2] > 0x10)
			pr_info("detected unstable: skip aipq seeting\n");
		break;
	default:
		break;
	}

	return 0;
}

void aipq_scs_proc(struct vframe_s *vf,
		   int (*cfg)[SCENES_VALUE],
		   int (*prob)[2],
		   int *out,
		   int *pq_debug)
{
	static int pre_top_one = -1;
	int top_one, top_one_prob;
	int top_two, top_two_prob;
	int top_three, top_three_prob;
	/*select color: skin/ green/ blue, percent : 1000 as 1.0*/
	static unsigned int pre_skin_pct;
	static unsigned int pre_green_pct;
	static unsigned int pre_blue_pct;
	unsigned int cur_total_hist = 1, cur_skin_pct, cur_green_pct, cur_blue_pct;
	u64 cur_skin_hist = 0, cur_green_hist = 0, cur_blue_hist = 0;
	unsigned int diff_skin_pct, diff_green_pct, diff_blue_pct;
	int i;
	static int pre_hist[32];

	memset(out, 0, sizeof(int) * SCENES_VALUE);

	top_one = prob[0][0];
	top_one_prob = prob[0][1];
	top_two = prob[1][0];
	top_two_prob = prob[1][1];
	top_three = prob[2][0];
	top_three_prob = prob[2][1];

	for (i = 0; i < 3; i++)
		cur_skin_hist += vf->prop.hist.vpp_hue_gamma[11 + i];
	for (i = 0; i < 5; i++)
		cur_green_hist += vf->prop.hist.vpp_hue_gamma[18 + i];
	for (i = 0; i < 5; i++)
		cur_blue_hist += vf->prop.hist.vpp_hue_gamma[27 + i];
	for (i = 0; i < 32; i++)
		cur_total_hist += vf->prop.hist.vpp_hue_gamma[i];

	cur_skin_pct = div64_u64(cur_skin_hist * 1000, cur_total_hist);
	cur_green_pct = div64_u64(cur_green_hist * 1000, cur_total_hist);
	cur_blue_pct = div64_u64(cur_blue_hist * 1000, cur_total_hist);

	aipq_hist_data.cur_skin_pct	= cur_skin_pct;
	aipq_hist_data.cur_blue_pct	= cur_blue_pct;
	aipq_hist_data.cur_green_pct	= cur_green_pct;

	diff_skin_pct = (cur_skin_pct > pre_skin_pct) ?
		(cur_skin_pct - pre_skin_pct) :
		(pre_skin_pct - cur_skin_pct);

	diff_green_pct = (cur_green_pct > pre_green_pct) ?
		(cur_green_pct - pre_green_pct) :
		(pre_green_pct - cur_green_pct);

	diff_blue_pct = (cur_blue_pct > pre_blue_pct) ?
		(cur_blue_pct - pre_blue_pct) :
		(pre_blue_pct - cur_blue_pct);

	color_th = get_color_th();

	if (pre_top_one == top_one) {
		memcpy(out, cfg[pre_top_one], sizeof(int) * SCENES_VALUE);
		scene_prob[0] = top_one;
		scene_prob[1] = top_one_prob;
	} else if (((pre_top_one == top_two) && (top_two_prob > 1000)) ||
			((pre_top_one == top_three) && (top_three_prob > 1000))) {
		memcpy(out, cfg[pre_top_one], sizeof(int) * SCENES_VALUE);

		if (pre_top_one == top_two) {
			scene_prob[0] = top_two;
			scene_prob[1] = top_two_prob;
		} else {
			scene_prob[0] = top_three;
			scene_prob[1] = top_three_prob;
		}
	} else if ((diff_skin_pct + diff_green_pct + diff_blue_pct < color_th) &&
			    (pre_top_one >= 0)) {
		memcpy(out, cfg[pre_top_one], sizeof(int) * SCENES_VALUE);
	} else if ((top_one == 1) && (pre_top_one == 3) && (pre_blue_pct > 500) &&
		(pre_blue_pct < cur_blue_pct)) {
		memcpy(out, cfg[pre_top_one], sizeof(int) * SCENES_VALUE);
	} else {
		if (pq_debug[2] == 0x8) {
			pr_info("pre_top_one = %d, top_one = %d, top_one_prob = %d, diff_skin_pct = %d, diff_green_pct = %d, diff_blue_pct = %d\n",
				pre_top_one, top_one, top_one_prob,
				diff_skin_pct, diff_green_pct, diff_blue_pct);
			for (i = 0; i < 4; i++)
				pr_info("pre: %d, %d, %d, %d, %d, %d, %d, %d\n",
				pre_hist[i * 8],
				pre_hist[i * 8 + 1],
				pre_hist[i * 8 + 2],
				pre_hist[i * 8 + 3],
				pre_hist[i * 8 + 4],
				pre_hist[i * 8 + 5],
				pre_hist[i * 8 + 6],
				pre_hist[i * 8 + 7]);
			for (i = 0; i < 4; i++)
				pr_info("cur: %d, %d, %d, %d, %d, %d, %d, %d\n",
				vf->prop.hist.vpp_hue_gamma[i * 8],
				vf->prop.hist.vpp_hue_gamma[i * 8 + 1],
				vf->prop.hist.vpp_hue_gamma[i * 8 + 2],
				vf->prop.hist.vpp_hue_gamma[i * 8 + 3],
				vf->prop.hist.vpp_hue_gamma[i * 8 + 4],
				vf->prop.hist.vpp_hue_gamma[i * 8 + 5],
				vf->prop.hist.vpp_hue_gamma[i * 8 + 6],
				vf->prop.hist.vpp_hue_gamma[i * 8 + 7]);
		}
		memcpy(out, cfg[top_one], sizeof(int) * SCENES_VALUE);
		pre_top_one = top_one;

		scene_prob[0] = top_one;
		scene_prob[1] = top_one_prob;
	}

	for (i = 0; i < 32; i++)
		pre_hist[i] = vf->prop.hist.vpp_hue_gamma[i];

	if (pq_debug[2] > 0x10)
		pr_info("pre_top_one = %d, diff_skin_pct = %d, diff_green_pct = %d, diff_blue_pct = %d\n",
			pre_top_one, diff_skin_pct,
			diff_green_pct, diff_blue_pct);

	pre_skin_pct = cur_skin_pct;
	pre_green_pct = cur_green_pct;
	pre_blue_pct = cur_blue_pct;

	aipq_hist_data.pre_skin_pct  = pre_skin_pct;
	aipq_hist_data.pre_green_pct = pre_green_pct;
	aipq_hist_data.pre_blue_pct  = pre_blue_pct;
}

int sc_det(struct vframe_s *vf, int *pq_debug)
{
	int i;
	static u32 hist_diff[3];
	u32 diff;
	u32 sum = 0;
	int sumshft;
	int norm14;
	/*ret=1: scn change*/
	int ret = 0;
	int luma_hist[64];
	static int pre_luma_hist[64];

	for (i = 0; i < 2; i++)
		hist_diff[i] = hist_diff[i + 1];

	hist_diff[2] = 0;

	for (i = 0; i < 64; i++) {
		luma_hist[i] = vf->prop.hist.vpp_gamma[i];
		sum += luma_hist[i];
		diff = (luma_hist[i] > pre_luma_hist[i]) ?
			(luma_hist[i] - pre_luma_hist[i]) :
			(pre_luma_hist[i] - luma_hist[i]);
		hist_diff[2] += diff;
		pre_luma_hist[i] = luma_hist[i];
	}

	if (sum == 0)
		return ret;

	sumshft =
		(sum >= (1 << 24)) ? 8 :
		(sum >= (1 << 22)) ? 6 :
		(sum >= (1 << 20)) ? 4 :
		(sum >= (1 << 18)) ? 2 :
		(sum >= (1 << 16)) ? 0 :
		(sum >= (1 << 14)) ? -2 :
		(sum >= (1 << 12)) ? -4 :
		(sum >= (1 << 10)) ? -6 :
		(sum >= (1 << 8)) ? -8 :
		(sum >= (1 << 6)) ? -10 :
		(sum >= (1 << 4)) ? -12 : -16;

	if (sumshft >= 0)
		norm14 = (1 << 30) / (sum >> sumshft);
	else if (sumshft > -16)
		norm14 = (1 << (30 + sumshft)) / sum;
	else
		norm14 = 1 << 14;

	if (sumshft >= 0) {
		hist_diff[2] = ((hist_diff[2] >> sumshft) * norm14 +
			(1 << 13)) >> 14;
	} else {
		hist_diff[2] = (((hist_diff[2] << (-sumshft)) * norm14) +
			(1 << 13)) >> 14;
	}

	/*normalize to 10bit*/
	hist_diff[2] >>= 6;
	if (hist_diff[2] > sc_th)
		ret = 1;

	if (pq_debug[2] == 2)
		pr_info("scene change ret = %d, hist_diff[2] = %d\n", ret, hist_diff[2]);

	return ret;
}

void aipq_scs_proc_s5(struct vframe_s *vf,
		   int (*cfg)[SCENES_VALUE],
		   int (*prob)[2],
		   int *out,
		   int *pq_debug)
{
	static int pre_top_one = -1;
	int top_one, top_one_prob;
	int top_two, top_two_prob;
	int top_three, top_three_prob;
	int sc_flag = 0;

	memset(out, 0, sizeof(int) * SCENES_VALUE);

	top_one = prob[0][0];
	top_one_prob = prob[0][1];
	top_two = prob[1][0];
	top_two_prob = prob[1][1];
	top_three = prob[2][0];
	top_three_prob = prob[2][1];

	sc_flag = sc_det(vf, pq_debug);

	if (pre_top_one == top_one) {
		memcpy(out, cfg[pre_top_one], sizeof(int) * SCENES_VALUE);
		scene_prob[0] = top_one;
		scene_prob[1] = top_one_prob;
	} else if (((pre_top_one == top_two) && (top_two_prob > 1000)) ||
			((pre_top_one == top_three) && (top_three_prob > 1000))) {
		memcpy(out, cfg[pre_top_one], sizeof(int) * SCENES_VALUE);

		if (pre_top_one == top_two) {
			scene_prob[0] = top_two;
			scene_prob[1] = top_two_prob;
		} else {
			scene_prob[0] = top_three;
			scene_prob[1] = top_three_prob;
		}
	} else if (!sc_flag) {
		memcpy(out, cfg[pre_top_one], sizeof(int) * SCENES_VALUE);
	} else {
		if (pq_debug[2] == 0x8) {
			pr_info("pre_top_one = %d, top_one = %d, top_one_prob = %d\n",
				pre_top_one, top_one, top_one_prob);
		}
		memcpy(out, cfg[top_one], sizeof(int) * SCENES_VALUE);
		pre_top_one = top_one;

		scene_prob[0] = top_one;
		scene_prob[1] = top_one_prob;
	}

	if (pq_debug[2] > 0x10)
		pr_info("pre_top_one = %d, top_one = %d, sc_flag = %d\n",
			pre_top_one, top_one, sc_flag);
}

void vf_pq_process(struct vframe_s *vf,
		   struct ai_scenes_pq *vpp_scenes,
		   int *pq_debug)
{
	int pq_value;
	int value;
	int i = 0;
	int prob[3][2];
	int bld_ofst[SCENES_VALUE];
	static int en_flag;
	int src_w, src_h;

	src_w = (vf->type & VIDTYPE_COMPRESS) ? vf->compWidth : vf->width;
	src_h = (vf->type & VIDTYPE_COMPRESS) ? vf->compHeight : vf->height;

	/*because s5 sr not support 4 slices, aipq only support 4k*/
	if (src_h > 2160 || src_w > 3840) {
		en_flag = 0;
		if (pq_debug[2] == 0x1)
			pr_info("over 4k not support\n");
	} else if (vf->ai_pq_enable && !en_flag) {
		en_flag = 1;
	}

	if (!en_flag) {
		if (pq_debug[2] == 0x1)
			pr_info("vf->ai_pq_enable = %d\n", vf->ai_pq_enable);
		return;
	}

	if (en_flag) {
		if (!vf->ai_pq_enable) {
			i = 0;
			while (i < SCENE_MAX) {
				detected_scenes[i].func(0, 0);
				i++;
			}
			en_flag = 0;

			scene_prob[0] = 0;
			scene_prob[1] = 0;
			if (pq_debug[2] == 0x1)
				pr_info("disable nn detect\n");
			return;
		}
	}

	pq_value = vf->nn_value[0].maxclass;

	for (i = 0; i < 3; i++) {
		prob[i][0] = vf->nn_value[i].maxclass;
		prob[i][1] = vf->nn_value[i].maxprob;
		if (pq_debug[2] > 0x10)
			pr_info("class top%d= %d: prob = %d\n",
				i + 1, prob[i][0], prob[i][1]);
	}

	if (pq_debug[0] != -1)
		pq_value = pq_debug[0];

	if (vf->nn_value[0].maxprob == 0 && pq_debug[0] == -1)
		return;

	if (pq_debug[1])
		pq_value = 23;

	aipq_set_policy = get_aipq_set_policy();
	if (aipq_set_policy == 1)
		aipq_scs_bld_proc(vpp_pq_data, prob, bld_ofst, pq_debug);
	else if (aipq_set_policy == 2)
		aipq_scs_proc_s5(vf, vpp_pq_data, prob, bld_ofst, pq_debug);
	else
		aipq_scs_proc(vf, vpp_pq_data, prob, bld_ofst, pq_debug);

	if (pq_debug[2] == 0x1)
		pr_info("top5:%d,%d; %d,%d; %d,%d; %d,%d; %d,%d;\n",
			vf->nn_value[0].maxclass, vf->nn_value[0].maxprob,
			vf->nn_value[1].maxclass, vf->nn_value[1].maxprob,
			vf->nn_value[2].maxclass, vf->nn_value[2].maxprob,
			vf->nn_value[3].maxclass, vf->nn_value[3].maxprob,
			vf->nn_value[4].maxclass, vf->nn_value[4].maxprob);

	i = 0;
	while (i < SCENE_MAX) {
		vpp_scenes[pq_value].pq_scenes = pq_value;
		if (pq_debug[3]) {
			detected_scenes[i].func(bld_ofst[i], 1);
		} else {
			value = vpp_scenes[pq_value].pq_values[i];
			detected_scenes[i].func(value, 1);
		}
		i++;
	}
}

