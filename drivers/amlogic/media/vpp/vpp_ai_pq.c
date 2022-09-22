// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "vpp_common.h"
#include "vpp_ai_pq.h"
#include "vpp_modules_inc.h"

#define FILTER_SIZE (7)

struct _cm_section_s {
	unsigned char start;
	unsigned char key;
	unsigned char end;
};

static unsigned int ai_pq_ctrl;
static struct vpp_single_scene_s detect_scenes[EN_SCENE_MAX] = {
	{false, NULL},
	{false, NULL},
	{false, NULL},
	{false, NULL},
	{false, NULL},
	{false, NULL},
	{false, NULL},
};

static struct _cm_section_s skin_section = {10, 11, 12};
static struct _cm_section_s green_section = {17, 18, 19};
static struct _cm_section_s cyan_section = {24, 24, 26};
static struct _cm_section_s blue_section = {28, 28, 30};
static unsigned int filter[FILTER_SIZE] = {
	10, 50, 80, 100, 80, 50, 10
};

/*Internal functions*/
static int _ai_pq_offset_smooth(int base_val,
	int reg_val, int offset, int step_rs)
{
/*base value : from db
 *reg value : last frame setting
 *offset : target offset for the scene
 *step_rs : used for value smooth change
 *step : change value frame base
 */
	int step = 0;
	int tmp_offset = 0;
	int tmp = 0;

	if (reg_val == (base_val + offset)) {/*base = current reg setting*/
		step = 0;
	} else {
		if (step_rs == 0) {
			step = 1;
		} else {
			tmp = (1 << step_rs) - 1;

			if (!offset) {/*offset = 0, reset to base value*/
				tmp_offset = abs(base_val - reg_val);
				step = (tmp_offset + tmp) >> step_rs;
			} else {
				step = (abs(offset) + tmp) >> step_rs;
			}
		}
	}

	return step;
}

static void _ai_pq_cm_calculate(struct cm_ai_pq_param_s *pparam,
	bool *pfirst, int *preg_val, int *poffset_val,
	int offset, bool enable,
	enum vpp_detect_scene_e scene, int start, int key, int end)
{
	static char cm_sat_offset[CM2_CURVE_SIZE] = {0};

	int step_rs = 1;
	int base_val = 0;
	int step;
	int tmp;
	int i;

	if (!enable || !(ai_pq_ctrl & (1 << scene))) {
		for (i = start; i < end; i++)
			cm_sat_offset[i] = 0;

		*pfirst = true;
	} else {
		if (*pfirst) {
			*preg_val = base_val;
			*pfirst = false;
		}

		step = _ai_pq_offset_smooth(base_val, *preg_val, offset, step_rs);

		if (step != 0) {
			tmp = *preg_val - base_val;

			if (offset == 0)
				if (tmp < 0)
					*poffset_val = tmp >= (offset - step) ?
						offset : (tmp + step);
				else
					*poffset_val = tmp < (offset + step) ?
						offset : (tmp - step);
			else
				if (offset > 0)
					*poffset_val = tmp >= (offset - step) ?
						offset : (tmp + step);
				else
					*poffset_val = tmp < (offset + step) ?
						offset : (tmp - step);

			*preg_val = base_val + *poffset_val;

			for (i = start; i <= end; i++) {
				if (i < key)
					tmp = (FILTER_SIZE >> 1) - (key - i);
				else
					tmp = (FILTER_SIZE >> 1) + (i - key);

				tmp = vpp_check_range(tmp, 0, FILTER_SIZE - 1);
				cm_sat_offset[i] = *poffset_val * filter[tmp]
					/ filter[FILTER_SIZE >> 1];
			}
		}
	}

	for (i = 0; i < CM2_CURVE_SIZE; i++) {
		pparam->sat[i] = cm_sat_offset[i];
		pparam->sat[i + CM2_CURVE_SIZE] = cm_sat_offset[i];
		pparam->sat[i + CM2_CURVE_SIZE * 2] = cm_sat_offset[i];
	}
}

static int _ai_pq_blue_scene_process(int offset, bool enable)
{
	static bool first_frame[2] = {true, true};
	static int reg_val[2];
	static int offset_val[2] = {0};

	int start = cyan_section.start;
	int key = cyan_section.key;
	int end = cyan_section.end + 1;
	enum vpp_detect_scene_e scene = EN_BLUE_SCENE;
	struct cm_ai_pq_param_s param;

	memset(&param, 0, sizeof(param));

	_ai_pq_cm_calculate(&param,
		&first_frame[0], &reg_val[0], &offset_val[0],
		offset, enable,
		scene, start, key, end);

	start = blue_section.start;
	key = blue_section.key;
	end = blue_section.end + 1;

	_ai_pq_cm_calculate(&param,
		&first_frame[1], &reg_val[1], &offset_val[1],
		offset, enable,
		scene, start, key, end);

	vpp_module_cm_set_ai_pq_offset(&param);

	return 0;
}

static int _ai_pq_green_scene_process(int offset, bool enable)
{
	static bool first_frame = true;
	static int reg_val;
	static int offset_val;

	int start = green_section.start;
	int key = green_section.key;
	int end = green_section.end + 1;
	enum vpp_detect_scene_e scene = EN_GREEN_SCENE;
	struct cm_ai_pq_param_s param;

	memset(&param, 0, sizeof(param));

	_ai_pq_cm_calculate(&param,
		&first_frame, &reg_val, &offset_val,
		offset, enable,
		scene, start, key, end);

	vpp_module_cm_set_ai_pq_offset(&param);

	return 0;
}

static int _ai_pq_skintone_scene_process(int offset, bool enable)
{
	static bool first_frame = true;
	static int reg_val;
	static int offset_val;

	int start = skin_section.start;
	int key = skin_section.key;
	int end = skin_section.end + 1;
	enum vpp_detect_scene_e scene = EN_SKIN_TONE_SCENE;
	struct cm_ai_pq_param_s param;

	memset(&param, 0, sizeof(param));

	_ai_pq_cm_calculate(&param,
		&first_frame, &reg_val, &offset_val,
		offset, enable,
		scene, start, key, end);

	vpp_module_cm_set_ai_pq_offset(&param);

	return 0;
}

static int _ai_pq_peaking_scene_process(int offset, bool enable)
{
	static bool first_frame = true;
	static int reg_val[4];
	static int offset_val[4] = {0};

	int step_rs = 1;
	int base_val[4];
	int step;
	int tmp;
	int i;
	struct sr_ai_pq_param_s param;

	memset(&param, 0, sizeof(param));

	if (!enable || !(ai_pq_ctrl & (1 << EN_PEAKING_SCENE))) {
		vpp_module_sr_set_ai_pq_offset(&param);
		first_frame = true;
		return 0;
	}

	vpp_module_sr_get_ai_pq_base(&param);
	base_val[0] = param.hp_final_gain[EN_MODE_SR_0];
	base_val[1] = param.hp_final_gain[EN_MODE_SR_1];
	base_val[2] = param.bp_final_gain[EN_MODE_SR_0];
	base_val[3] = param.bp_final_gain[EN_MODE_SR_1];

	if (first_frame) {
		for (i = 0; i < 4; i++)
			reg_val[i] = base_val[i];
		first_frame = false;
	}

	for (i = 0; i < 4; i++) {
		step = _ai_pq_offset_smooth(base_val[i], reg_val[i], offset, step_rs);

		if (step != 0) {
			tmp = reg_val[i] - base_val[i];

			if (offset == 0)
				if (tmp < 0)
					offset_val[i] = tmp >= (offset - step) ?
						offset : (tmp + step);
				else
					offset_val[i] = tmp < (offset + step) ?
						offset : (tmp - step);
			else
				if (offset > 0)
					offset_val[i] = tmp >= (offset - step) ?
						offset : (tmp + step);
				else
					offset_val[i] = tmp < (offset + step) ?
						offset : (tmp - step);

			reg_val[i] = base_val[i] + offset_val[i];
		}
	}

	param.hp_final_gain[EN_MODE_SR_0] = offset_val[0];
	param.hp_final_gain[EN_MODE_SR_1] = offset_val[1];
	param.bp_final_gain[EN_MODE_SR_0] = offset_val[2];
	param.bp_final_gain[EN_MODE_SR_1] = offset_val[3];
	vpp_module_sr_set_ai_pq_offset(&param);

	return 0;
}

static int _ai_pq_saturation_scene_process(int offset, bool enable)
{
	static bool first_frame = true;
	static int base_val_pre;
	static int reg_val;

	int offset_val = 0;
	int step_rs = 1;
	int base_val;
	int step;
	int tmp;
	struct vadj_ai_pq_param_s param;

	memset(&param, 0, sizeof(param));

	if (!enable || !(ai_pq_ctrl & (1 << EN_SATURATION_SCENE))) {
		vpp_module_vadj_set_ai_pq_offset(&param);
		first_frame = true;
		return 0;
	}

	vpp_module_vadj_get_ai_pq_base(&param);
	base_val = param.sat_hue_mad >> 16;

	if (first_frame || base_val_pre != base_val) {
		reg_val = base_val;
		first_frame = false;
	}

	base_val_pre = base_val;
	step = _ai_pq_offset_smooth(base_val, reg_val, offset, step_rs);

	if (step != 0) {
		tmp = reg_val - base_val;

		if (offset == 0)
			if (tmp < 0)
				offset_val = tmp >= (offset - step) ?
					offset : (tmp + step);
			else
				offset_val = tmp < (offset + step) ?
					offset : (tmp - step);
		else
			if (offset > 0)
				offset_val = tmp >= (offset - step) ?
					offset : (tmp + step);
			else
				offset_val = tmp < (offset + step) ?
					offset : (tmp - step);

		reg_val = base_val + offset_val;

		param.sat_hue_mad = offset_val << 16;
		vpp_module_vadj_set_ai_pq_offset(&param);
	}

	return 0;
}

static int _ai_pq_contrast_scene_process(int offset, bool enable)
{
	static bool first_frame = true;
	static int reg_val;

	int offset_val = 0;
	int step_rs = 0;
	int base_val;
	int step;
	int tmp;
	struct dnlp_ai_pq_param_s param;

	memset(&param, 0, sizeof(param));

	if (!enable || !(ai_pq_ctrl & (1 << EN_DYNAMIC_CONTRAST_SCENE))) {
		vpp_module_dnlp_set_ai_pq_offset(&param);
		first_frame = true;
		return 0;
	}

	vpp_module_dnlp_get_ai_pq_base(&param);
	base_val = param.final_gain;

	if (first_frame) {
		reg_val = base_val;
		first_frame = false;
	}

	step = _ai_pq_offset_smooth(base_val, reg_val, offset, step_rs);

	if (step != 0) {
		tmp = reg_val - base_val;

		if (offset == 0)
			if (tmp < 0)
				offset_val = tmp >= (offset - step) ?
					offset : (tmp + step);
			else
				offset_val = tmp < (offset + step) ?
					offset : (tmp - step);
		else
			if (offset > 0)
				offset_val = tmp >= (offset - step) ?
					offset : (tmp + step);
			else
				offset_val = tmp < (offset + step) ?
					offset : (tmp - step);

		reg_val = base_val + offset_val;

		param.final_gain = offset_val;
		vpp_module_dnlp_set_ai_pq_offset(&param);
	}

	return 0;
}

static int _ai_pq_noise_scene_process(int offset, bool enable)
{
	return 0;
}

/*External functions*/
int vpp_ai_pq_init(void)
{
	enum vpp_detect_scene_e index;

	ai_pq_ctrl =
		(1 << EN_BLUE_SCENE) |
		(1 << EN_GREEN_SCENE) |
		(1 << EN_SKIN_TONE_SCENE) |
		(1 << EN_PEAKING_SCENE) |
		(1 << EN_SATURATION_SCENE) |
		(1 << EN_DYNAMIC_CONTRAST_SCENE) |
		(1 << EN_NOISE_SCENE);

	for (index = EN_BLUE_SCENE; index <= EN_SCENE_MAX; index++) {
		switch (index) {
		case EN_BLUE_SCENE:
			detect_scenes[index].enable = true;
			detect_scenes[index].func =
				_ai_pq_blue_scene_process;
			break;
		case EN_GREEN_SCENE:
			detect_scenes[index].enable = true;
			detect_scenes[index].func =
				_ai_pq_green_scene_process;
			break;
		case EN_SKIN_TONE_SCENE:
			detect_scenes[index].enable = true;
			detect_scenes[index].func =
				_ai_pq_skintone_scene_process;
			break;
		case EN_PEAKING_SCENE:
			detect_scenes[index].enable = true;
			detect_scenes[index].func =
				_ai_pq_peaking_scene_process;
			break;
		case EN_SATURATION_SCENE:
			detect_scenes[index].enable = true;
			detect_scenes[index].func =
				_ai_pq_saturation_scene_process;
			break;
		case EN_DYNAMIC_CONTRAST_SCENE:
			detect_scenes[index].enable = true;
			detect_scenes[index].func =
				_ai_pq_contrast_scene_process;
			break;
		case EN_NOISE_SCENE:
			detect_scenes[index].enable = true;
			detect_scenes[index].func =
				_ai_pq_noise_scene_process;
			break;
		default:
			break;
		}
	}

	return 0;
}

struct vpp_single_scene_s *vpp_ai_pq_get_scene_proc(enum vpp_detect_scene_e index)
{
	if (index == EN_SCENE_MAX)
		return NULL;

	return &detect_scenes[index];
}

