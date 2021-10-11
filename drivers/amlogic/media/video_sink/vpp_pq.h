/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef VPP_PQ_H
#define VPP_PQ_H

#include <linux/amlogic/media/vfm/vframe.h>

#define AI_SCENES_MAX 25
#define SCENES_VALUE 10

#define AI_SCENES_CUR_USE_MAX 7
#define SCENES_CUR_USE_VALUE 6

enum ai_scenes {
	AI_SCENES_FACESKIN = 0,
	AI_SCENES_BLUESKY,
	AI_SCENES_FOODS,
	AI_SCENES_ARCHITECTURE,
	AI_SCENES_GRASS,
	AI_SCENES_NIGHTSCOP,
	AI_SCENES_DOCUMENT,
	AI_SCENES_WATERSIDE,
	AI_SCENES_FLOWERS,
	AI_SCENES_BREADS,
	AI_SCENES_FRUITS,
	AI_SCENES_MEATS,
	AI_SCENES_OCEAN,
	AI_SCENES_PATTERN,
	AI_SCENES_GROUP,
	AI_SCENES_ANIMALS,
	AI_SCENES_ICESKATE,
	AI_SCENES_LEAVES,
	AI_SCENES_RACETRACK,
	AI_SCENES_FIREWORKS,
	AI_SCENES_WATERFALL,
	AI_SCENES_BEACH,
	AI_SCENES_SNOWS,
	AI_SCENES_SUNSET,
	DEFAUT_SETTING,
};

struct ai_scenes_pq {
	enum ai_scenes pq_scenes;
	int pq_values[SCENES_VALUE];
};

struct ai_pq_hist_data {
	unsigned int pre_skin_pct;
	unsigned int pre_green_pct;
	unsigned int pre_blue_pct;
	unsigned int cur_skin_pct;
	unsigned int cur_green_pct;
	unsigned int cur_blue_pct;
};

enum iir_policy_e {
	SC_INVALID = 1,
	SC_DETECTED
};

enum aipq_state_mach {
	AIPQ_IDLE = 0,
	AIPQ_DET_UNSTABLE,
	AIPQ_DET_STATBLE
};

#define DET_STABLE_CNT 30
#define UN_STABLE_CNT 20
#define ACCEPT_CNT 6

void vf_pq_process(struct vframe_s *vf,
		   struct ai_scenes_pq *vpp_scenes, int *pq_debug);

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
u32 get_stb_cnt(void);
u32 get_unstb_cnt(void);
u32 get_tolrnc_cnt(void);
u32 get_timer_filter_en(void);
u32 get_aipq_set_policy(void);
u32 get_color_th(void);
#endif

extern int vpp_pq_data[AI_SCENES_MAX][SCENES_VALUE];
extern int scene_prob[2];
extern struct ai_pq_hist_data aipq_hist_data;
#endif
