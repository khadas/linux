/*
 * drivers/amlogic/atv_demod/atv_demod_monitor.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/types.h>
#include <uapi/linux/dvb/frontend.h>
#include <linux/amlogic/cpu_version.h>

#include "atvdemod_func.h"
#include "atvauddemod_func.h"
#include "atv_demod_monitor.h"
#include "atv_demod_ops.h"
#include "atv_demod_debug.h"

static DEFINE_MUTEX(monitor_mutex);

bool atvdemod_mixer_tune_en;
bool atvdemod_overmodulated_en;
bool atv_audio_overmodulated_en;
unsigned int atv_audio_overmodulated_cnt = 1;
bool audio_det_en;
bool atvdemod_det_snr_en = true;
bool audio_thd_en;
bool atvdemod_det_nonstd_en;
bool atvaudio_det_outputmode_en = true;
bool audio_carrier_offset_det_en;
bool atvdemod_horiz_freq_det_en = true;

unsigned int atvdemod_timer_delay = 100; /* 1s */
unsigned int atvdemod_timer_delay2 = 10; /* 100ms */

bool atvdemod_timer_en = true;


static void atv_demod_monitor_do_work(struct work_struct *work)
{
	int vpll_lock = 0, line_lock = 0;
	struct atv_demod_monitor *monitor =
			container_of(work, struct atv_demod_monitor, work);

	if (monitor->state == MONI_DISABLE)
		return;

	retrieve_vpll_carrier_lock(&vpll_lock);
	retrieve_vpll_carrier_line_lock(&line_lock);
	if ((vpll_lock != 0) || (line_lock != 0)) {
		monitor->lock_cnt = 0;
		return;
	}

	monitor->lock_cnt++;

	if (atvdemod_mixer_tune_en)
		atvdemod_mixer_tune();

	if (atvdemod_overmodulated_en)
		atvdemod_video_overmodulated();

	if (atv_audio_overmodulated_en) {
		if (monitor->lock_cnt > atv_audio_overmodulated_cnt)
			aml_audio_overmodulation(1);
	}

	if (atvdemod_det_snr_en)
		atvdemod_det_snr_serice();

	if (audio_thd_en)
		audio_thd_det();

	if (atvaudio_det_outputmode_en)
		atvauddemod_set_outputmode();

	if (audio_carrier_offset_det_en)
		audio_carrier_offset_det();

	if (atvdemod_det_nonstd_en)
		atv_dmd_non_std_set(true);

	if (atvdemod_horiz_freq_det_en)
		atvdemod_horiz_freq_detection();
}

static void atv_demod_monitor_timer_handler(unsigned long arg)
{
	struct atv_demod_monitor *monitor = (struct atv_demod_monitor *)arg;

	/* 100ms timer */
	monitor->timer.data = arg;
	monitor->timer.expires = jiffies +
			ATVDEMOD_INTERVAL * atvdemod_timer_delay2;
	add_timer(&monitor->timer);

	if (atvdemod_timer_en == 0)
		return;

	if (vdac_enable_check_dtv())
		return;

	if (monitor->state == MONI_PAUSE)
		return;

	schedule_work(&monitor->work);
}

static void atv_demod_monitor_enable(struct atv_demod_monitor *monitor)
{
	mutex_lock(&monitor->mtx);

	if (atvdemod_timer_en && monitor->state == MONI_DISABLE) {
		atv_dmd_non_std_set(false);

		init_timer(&monitor->timer);
		monitor->timer.data = (ulong) monitor;
		monitor->timer.function = atv_demod_monitor_timer_handler;
		/* after 1s enable demod auto detect */
		monitor->timer.expires = jiffies +
				ATVDEMOD_INTERVAL * atvdemod_timer_delay;
		add_timer(&monitor->timer);
		monitor->state = MONI_ENABLE;
		monitor->lock_cnt = 0;
	} else if (atvdemod_timer_en && monitor->state == MONI_PAUSE) {
		atv_dmd_non_std_set(false);

		monitor->state = MONI_ENABLE;
		monitor->lock_cnt = 0;
	}

	mutex_unlock(&monitor->mtx);

	pr_dbg("%s: state: %d.\n", __func__, monitor->state);
}

static void atv_demod_monitor_disable(struct atv_demod_monitor *monitor)
{
	mutex_lock(&monitor->mtx);

	if (atvdemod_timer_en && monitor->state != MONI_DISABLE) {
		monitor->state = MONI_DISABLE;
		del_timer_sync(&monitor->timer);
		cancel_work_sync(&monitor->work);
	}

	mutex_unlock(&monitor->mtx);

	pr_dbg("%s: state: %d.\n", __func__, monitor->state);
}

static void atv_demod_monitor_pause(struct atv_demod_monitor *monitor)
{
	mutex_lock(&monitor->mtx);

	if (monitor->state == MONI_ENABLE) {
		monitor->state = MONI_PAUSE;
		atv_dmd_non_std_set(false);
		cancel_work_sync(&monitor->work);
	}

	mutex_unlock(&monitor->mtx);
}

void atv_demod_monitor_init(struct atv_demod_monitor *monitor)
{
	mutex_lock(&monitor_mutex);

	mutex_init(&monitor->mtx);

	monitor->state = MONI_DISABLE;
	monitor->lock = false;
	monitor->lock_cnt = 0;
	monitor->disable = atv_demod_monitor_disable;
	monitor->enable = atv_demod_monitor_enable;
	monitor->pause = atv_demod_monitor_pause;

	INIT_WORK(&monitor->work, atv_demod_monitor_do_work);

	mutex_unlock(&monitor_mutex);
}
