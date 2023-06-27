// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#undef pr_fmt
#define pr_fmt(fmt) "snd_soft_locker: " fmt

#include "card.h"
#include "soft_locker.h"

static const char *const locker_sel_texts[] = {
	"device 0", "device 1", "device 2", "device 3",
	"device 4", "device 5", "device 6", "device 7"
};

static const struct soc_enum locker_input_source_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(locker_sel_texts),
		locker_sel_texts);

static int locker_src_enum_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *soc_card = snd_kcontrol_chip(kcontrol);
	struct soft_locker *locker = aml_get_card_locker(soc_card);

	ucontrol->value.enumerated.item[0] = locker->devin_id;

	return 0;
}

static int locker_src_enum_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *soc_card = snd_kcontrol_chip(kcontrol);
	struct soft_locker *locker = aml_get_card_locker(soc_card);
	int id = ucontrol->value.enumerated.item[0];

	if (id >= LOCKER_DEVICE_MAX || id < 0)
		return 0;

	locker->devin_id = id;
	locker->to = locker->src_tddrs[id];

	return 0;
}

static const struct soc_enum locker_output_sink_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(locker_sel_texts),
		locker_sel_texts);

static int locker_sink_enum_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *soc_card = snd_kcontrol_chip(kcontrol);
	struct soft_locker *locker = aml_get_card_locker(soc_card);

	ucontrol->value.enumerated.item[0] = locker->devout_id;

	return 0;
}

static int locker_sink_enum_set(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *soc_card = snd_kcontrol_chip(kcontrol);
	struct soft_locker *locker = aml_get_card_locker(soc_card);
	int id = ucontrol->value.enumerated.item[0];

	if (id >= LOCKER_DEVICE_MAX || id < 0)
		return 0;

	locker->devout_id = id;
	locker->fr = locker->sink_fddrs[id];

	return 0;
}

static void reset_ddr_status(struct ddr_status *st, unsigned int pos)
{
	st->start_pos = pos;
	st->last_pos = pos;
	st->wrap_cnt = 0;
}

static int locker_reset_ddr_status(struct soft_locker *locker,
	unsigned int fr_pos, unsigned int to_pos)
{
	struct ddr_status *to_status = &locker->to_status;
	struct ddr_status *fr_status = &locker->fr_status;

	reset_ddr_status(to_status, to_pos);
	reset_ddr_status(fr_status, fr_pos);
	locker->reset = false;

	return 0;
}

static int locker_ddr_elapsed_time_ms(struct ddr_status *st)
{
	int frames_total, frames;
	int wrap_num = st->wrap_cnt;

	if (st->last_pos >= st->start_pos) {
		frames = (st->last_pos - st->start_pos) / st->frame_size;
	} else {
		frames = (st->buf_frames * st->frame_size +
			st->last_pos - st->start_pos) / st->frame_size;
		wrap_num = st->wrap_cnt - 1;
	}
	frames_total = wrap_num * st->buf_frames + frames;

	return frames_total * 10 / (st->sample_rate / 100);
}

static int locker_calc_sink_src_diffs(struct soft_locker *locker)
{
	struct ddr_status *to_status = &locker->to_status;
	struct ddr_status *fr_status = &locker->fr_status;
	int fr_elapsed_ms, to_elapsed_ms, diff_ms;
	static int i;

	fr_elapsed_ms = locker_ddr_elapsed_time_ms(fr_status);
	to_elapsed_ms = locker_ddr_elapsed_time_ms(to_status);
	diff_ms = fr_elapsed_ms - to_elapsed_ms;

	if (i % 500 == 0) {
		pr_debug("fr time: %d, to time %d, diff %dms\n",
			fr_elapsed_ms, to_elapsed_ms,
			diff_ms);
	}
	i++;
	return diff_ms;
}

static int ddr_status_update_pos(struct ddr_status *st, unsigned int pos)
{
	if (pos < st->last_pos)
		st->wrap_cnt++;

	st->last_pos = pos;
	return 0;
}

/* Ugly but fix the DDR released by user asynchronously */
static int locker_update_ddrs(struct soft_locker *locker)
{
	int ret = 0;
	struct ddr_status *fr_st = &locker->fr_status;
	struct ddr_status *to_st = &locker->to_status;

	locker->fr = locker->sink_fddrs[locker->devout_id];
	locker->to = locker->src_tddrs[locker->devin_id];
	if (!locker->fr || !locker->to)
		return -EINVAL;

	fr_st->buf_frames = locker->fr->buf_frames;
	fr_st->frame_size = locker->fr->frame_size;
	fr_st->sample_rate = locker->fr->rate;
	to_st->buf_frames = locker->to->buf_frames;
	to_st->frame_size = locker->to->frame_size;
	to_st->sample_rate = locker->to->rate;

	return ret;
}

#define DELAY_TIMER_MS 4
#define DELAY_NO_READY_MS 1000
static void locker_timer_func(struct timer_list *t)
{
	struct soft_locker *locker = from_timer(locker, t, timer);
	unsigned long delay = 0;
	unsigned int fr_pos, to_pos;
	int ret = 0;

	if (locker->update_ddr) {
		/* update until find valid ddrs */
		ret = locker_update_ddrs(locker);
		if (ret < 0) {
			/*pr_warn("%s(), fr %p, to %p\n",
			 *	__func__, locker->fr, locker->to);
			 */
			locker->diff_ms = 0;
			/* ddr not ready, set timer interval longer */
			delay = msecs_to_jiffies(DELAY_NO_READY_MS);
			goto exit;
		}
		locker->update_ddr = 0;
	}

	fr_pos = aml_frddr_get_position(locker->fr) - locker->fr->start_addr;
	to_pos = aml_toddr_get_position(locker->to) - locker->to->start_addr;
	delay = msecs_to_jiffies(DELAY_TIMER_MS);

	if (locker->reset) {
		locker_reset_ddr_status(locker, fr_pos, to_pos);
		locker->diff_ms = 0;
		goto exit;
	}

	ddr_status_update_pos(&locker->to_status, to_pos);
	ddr_status_update_pos(&locker->fr_status, fr_pos);
	locker->diff_ms = locker_calc_sink_src_diffs(locker);

exit:
	mod_timer(&locker->timer, jiffies + delay);
}

static int locker_en_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *soc_card = snd_kcontrol_chip(kcontrol);
	struct soft_locker *locker = aml_get_card_locker(soc_card);
	bool en = ucontrol->value.enumerated.item[0];

	if (locker->en == en)
		return 0;

	locker->en = en;

	if (locker->en) {
		timer_setup(&locker->timer, locker_timer_func, 0);
		locker_update_ddr_en(locker);
		locker_reset(locker);
		locker->timer.expires = jiffies + 1;
		add_timer(&locker->timer);
	} else {
		del_timer(&locker->timer);
	}
	return 0;
}

static int locker_en_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *soc_card = snd_kcontrol_chip(kcontrol);
	struct soft_locker *locker = aml_get_card_locker(soc_card);

	ucontrol->value.enumerated.item[0] = locker->en;

	return 0;
}

static int locker_reset_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	(void)kcontrol;

	ucontrol->value.enumerated.item[0] = false;

	return 0;
}

static int locker_reset_put(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *soc_card = snd_kcontrol_chip(kcontrol);
	struct soft_locker *locker = aml_get_card_locker(soc_card);

	locker->reset = ucontrol->value.enumerated.item[0];

	return 0;
}

static int locker_diff_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *soc_card = snd_kcontrol_chip(kcontrol);
	struct soft_locker *locker = aml_get_card_locker(soc_card);

	ucontrol->value.enumerated.item[0] = locker->diff_ms;

	return 0;
}

static const struct snd_kcontrol_new snd_locker_controls[] = {
	SOC_ENUM_EXT("locker in src",
		locker_input_source_enum,
		locker_src_enum_get,
		locker_src_enum_set),
	SOC_ENUM_EXT("locker out sink",
		locker_output_sink_enum,
		locker_sink_enum_get,
		locker_sink_enum_set),
	SOC_SINGLE_BOOL_EXT("soft locker enable", 0,
			    locker_en_get,
			    locker_en_set),
	SOC_SINGLE_BOOL_EXT("soft locker reset", 0,
			    locker_reset_get,
			    locker_reset_put),
	SOC_SINGLE_EXT("soft locker diff",
			 SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
			 locker_diff_get, NULL)
};

int card_add_locker_kcontrols(struct snd_soc_card *card)
{
	return snd_soc_add_card_controls(card,
			snd_locker_controls, ARRAY_SIZE(snd_locker_controls));
}

/* need update ddr start pos and cnt */
int locker_reset(struct soft_locker *locker)
{
	locker->reset = true;

	return 0;
}

/* need update ddr and configs */
int locker_update_ddr_en(struct soft_locker *locker)
{
	locker->update_ddr = true;

	return 0;
}

int locker_en_ddr_by_dai_name(struct soft_locker *locker,
		const char *dai_name, int input)
{
	const char *name;

	if (input)
		name = locker_id_to_dai_name(locker, locker->devin_id);
	else
		name = locker_id_to_dai_name(locker, locker->devout_id);

	if (!strncmp(name, dai_name, strlen(name)))
		locker_update_ddr_en(locker);

	return 0;
}

int locker_add_dai_name(struct soft_locker *locker, int dev_num, const char *name)
{
	if (dev_num >= LOCKER_DEVICE_MAX || !name)
		return -EINVAL;

	locker->dais_name[dev_num] = name;

	return 0;
}

const char *locker_id_to_dai_name(struct soft_locker *locker, int dev_num)
{
	if (dev_num >= LOCKER_DEVICE_MAX)
		return NULL;

	return locker->dais_name[dev_num];
}

int locker_dai_name_to_id(struct soft_locker *locker, const char *name)
{
	int id = 0, i;

	for (i = 0; i < LOCKER_DEVICE_MAX; i++) {
		if (!strcmp(name, locker->dais_name[i])) {
			id = i;
			break;
		}
	}

	return id;
}

void locker_register_frddr(struct soft_locker *locker,
		struct frddr *fr, const char *name)
{
	int id;

	if (!locker || !fr || !name)
		return;

	id = locker_dai_name_to_id(locker, name);
	locker->sink_fddrs[id] = fr;
}

void locker_register_toddr(struct soft_locker *locker,
		struct toddr *to, const char *name)
{
	int id;

	if (!locker || !to || !name)
		return;

	id = locker_dai_name_to_id(locker, name);
	locker->src_tddrs[id] = to;
}

void locker_release_frddr(struct soft_locker *locker, const char *name)
{
	int id;

	if (!locker || !name)
		return;

	id = locker_dai_name_to_id(locker, name);
	locker->sink_fddrs[id] = NULL;
}

void locker_release_toddr(struct soft_locker *locker, const char *name)
{
	int id;

	if (!locker || !name)
		return;

	id = locker_dai_name_to_id(locker, name);
	locker->src_tddrs[id] = NULL;
}

