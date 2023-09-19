// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <uapi/linux/dvb/frontend.h>

#include "atvdemod_func.h"
#include "atv_demod_afc.h"
#include "atv_demod_ops.h"
#include "atv_demod_debug.h"

static DEFINE_MUTEX(afc_mutex);


unsigned int afc_limit = 2100;/* +/-2.1Mhz */
unsigned int afc_timer_delay = 1;
unsigned int afc_timer_delay2 = 10;
unsigned int afc_timer_delay3 = 10; /* 100ms */
unsigned int afc_wave_cnt = 4;

static int afc_range[11] = {0, -500, 500, -1000, 1000,
		-1500, 1500, -2000, 2000, -2500, 2500};

bool afc_timer_en = true;


static void atv_demod_afc_sync_frontend(struct atv_demod_afc *afc,
		int freq_offset)
{
	struct atv_demod_priv *priv =
			container_of(afc, struct atv_demod_priv, afc);
	struct dvb_frontend *fe = afc->fe;
	struct v4l2_frontend *v4l2_fe =
				container_of(fe, struct v4l2_frontend, fe);
	struct analog_parameters *param = &priv->atvdemod_param.param;
	s32 tuner_afc = 0;

	v4l2_fe->params.frequency = param->frequency + freq_offset;

	/* just play mode need sync */
	if (!(v4l2_fe->params.flag & ANALOG_FLAG_ENABLE_AFC)) {
		/* Get tuners internally correct frequency offset */
		if (fe->ops.tuner_ops.get_afc)
			fe->ops.tuner_ops.get_afc(fe, &tuner_afc);

		freq_offset += tuner_afc;
		v4l2_fe->params.frequency = param->frequency + freq_offset;

		pr_afc("%s, sync frequency: %d.\n", __func__,
				v4l2_fe->params.frequency);
	}
}

static void atv_demod_afc_do_work_pre(struct atv_demod_afc *afc)
{
	struct atv_demod_priv *priv =
			container_of(afc, struct atv_demod_priv, afc);
	struct dvb_frontend *fe = afc->fe;
	struct analog_parameters *param = &priv->atvdemod_param.param;
	struct analog_parameters p = priv->atvdemod_param.param;
	unsigned int tuner_id = priv->atvdemod_param.tuner_id;
	int freq_offset = 0;

	afc->pre_unlock_cnt++;

	if (afc->lock)
		retrieve_frequency_offset(&freq_offset);

	if (!afc->lock || (afc->lock && abs(freq_offset) >= (ATV_AFC_400KHZ / 1000))) {
		afc->pre_lock_cnt = 0;
		if (afc->status != AFC_LOCK_STATUS_PRE_UNLOCK && afc->offset) {
			param->frequency -= afc->offset * 1000;
			afc->offset = 0;
			afc->pre_step = 0;
			afc->status = AFC_LOCK_STATUS_PRE_UNLOCK;
		}

		if (afc->pre_unlock_cnt <= afc_wave_cnt) {/*40ms*/
			afc->status = AFC_LOCK_STATUS_PRE_UNLOCK;
			return;
		}

		if (afc->offset == afc_range[afc->pre_step]) {
			param->frequency -= afc_range[afc->pre_step] * 1000;
			afc->offset -= afc_range[afc->pre_step];
		}

		afc->pre_step++;
		if (afc->pre_step < AFC_LOCK_PRE_STEP_NUM) {
			param->frequency += afc_range[afc->pre_step] * 1000;
			afc->offset += afc_range[afc->pre_step];
			afc->status = AFC_LOCK_STATUS_PRE_UNLOCK;
		} else {
			param->frequency -= afc->offset * 1000;
			afc->offset = 0;
			afc->pre_step = 0;
			afc->status = AFC_LOCK_STATUS_PRE_OVER_RANGE;
		}

		/* tuner config, no need cvbs format, just audio mode. */
		p.frequency = param->frequency;
		p.std = (param->std & 0xFF000000) | p.audmode;
		if (fe->ops.tuner_ops.set_frequency)
			fe->ops.tuner_ops.set_frequency(fe, p.frequency);
		else if (fe->ops.tuner_ops.set_analog_params)
			fe->ops.tuner_ops.set_analog_params(fe, &p);

		if (tuner_id == AM_TUNER_MXL661)
			usleep_range(30 * 1000, 30 * 1000 + 100);
		else if (tuner_id == AM_TUNER_R840 ||
				tuner_id == AM_TUNER_R842)
			usleep_range(40 * 1000, 40 * 1000 + 100);
		else if (tuner_id == AM_TUNER_ATBM2040 ||
				tuner_id == AM_TUNER_ATBM253)
			usleep_range(50 * 1000, 50 * 1000 + 100);
		else
			usleep_range(10 * 1000, 10 * 1000 + 100);

		afc->pre_unlock_cnt = 0;
		pr_afc("%s,unlock, afc offset:%d KHz, freq_offset:%d KHz, set freq:%d\n",
				__func__, afc->offset, freq_offset, param->frequency);
	} else {
		afc->pre_lock_cnt++;
		pr_afc("%s,afc_pre_lock_cnt:%d, freq_offset:%d KHz\n",
				__func__, afc->pre_lock_cnt, freq_offset);
		if (afc->pre_lock_cnt >= afc_wave_cnt * 2) {/*100ms*/
			afc->pre_lock_cnt = 0;
			afc->pre_unlock_cnt = 0;
			afc->status = AFC_LOCK_STATUS_PRE_LOCK;
		}
	}
}

void atv_demod_afc_do_work(struct work_struct *work)
{
	struct atv_demod_afc *afc =
			container_of(work, struct atv_demod_afc, work);
	struct atv_demod_priv *priv =
				container_of(afc, struct atv_demod_priv, afc);
	struct dvb_frontend *fe = afc->fe;
	struct analog_parameters *param = &priv->atvdemod_param.param;
	struct analog_parameters p = priv->atvdemod_param.param;
	unsigned int tuner_id = priv->atvdemod_param.tuner_id;

	int freq_offset = 100;
	int tmp = 0;
	int field_lock = 0;
	int adc_status = 0;

	adc_status = atv_demod_get_adc_status();

	if (afc->state != AFC_ENABLE || !adc_status)
		return;

	retrieve_vpll_carrier_lock(&tmp);/* 0 means lock, 1 means unlock */
	afc->lock = (tmp == 0);

	/*pre afc:speed up afc*/
	if ((afc->status != AFC_LOCK_STATUS_POST_PROCESS) &&
		(afc->status != AFC_LOCK_STATUS_POST_LOCK) &&
		(afc->status != AFC_LOCK_STATUS_PRE_LOCK)) {
		atv_demod_afc_do_work_pre(afc);
		return;
	}

	afc->pre_step = 0;

	retrieve_frequency_offset(&freq_offset);

	if (++(afc->wave_cnt) <= afc_wave_cnt) {/*40ms*/
		afc->status = AFC_LOCK_STATUS_POST_PROCESS;
		pr_afc("%s,wave_cnt:%d is wave, afc lock:%d, freq_offset:%d KHz ignore\n",
				__func__, afc->wave_cnt, afc->lock,
				freq_offset);
		return;
	}

	/*retrieve_frequency_offset(&freq_offset);*/
	retrieve_field_lock(&tmp);
	field_lock = tmp;

	pr_afc("%s,afc offset:%d KHz, freq_offset:%d KHz, afc lock:%d, field_lock:%d, freq:%d\n",
			__func__, afc->offset, freq_offset, afc->lock, field_lock,
			param->frequency);

	if (afc->lock && (abs(freq_offset) < AFC_BEST_LOCK &&
		abs(afc->offset) <= afc_limit) && field_lock) {
		afc->status = AFC_LOCK_STATUS_POST_LOCK;
		afc->wave_cnt = 0;

		atv_demod_afc_sync_frontend(afc, freq_offset * 1000);

		pr_afc("%s,afc lock, set wave_cnt 0\n", __func__);
		return;
	}

	/* add "(lock && !field_lock)", horizontal synchronization test NG */
	if (!afc->lock/* || (afc->lock && !field_lock)*/) {
		afc->status = AFC_LOCK_STATUS_POST_UNLOCK;
		afc->pre_lock_cnt = 0;
		param->frequency -= afc->offset * 1000;

		/* tuner config, no need cvbs format, just audio mode. */
		p.frequency = param->frequency;
		p.std = (param->std & 0xFF000000) | p.audmode;
		if (fe->ops.tuner_ops.set_frequency)
			fe->ops.tuner_ops.set_frequency(fe, p.frequency);
		else if (fe->ops.tuner_ops.set_analog_params)
			fe->ops.tuner_ops.set_analog_params(fe, &p);

		if (tuner_id == AM_TUNER_MXL661)
			usleep_range(30 * 1000, 30 * 1000 + 100);
		else if (tuner_id == AM_TUNER_R840 ||
				tuner_id == AM_TUNER_R842)
			usleep_range(40 * 1000, 40 * 1000 + 100);
		else if (tuner_id == AM_TUNER_ATBM2040 ||
				tuner_id == AM_TUNER_ATBM253)
			usleep_range(50 * 1000, 50 * 1000 + 100);
		else
			usleep_range(10 * 1000, 10 * 1000 + 100);

		pr_afc("%s,freq_offset:%d KHz, set freq:%d\n",
				__func__, freq_offset, param->frequency);

		afc->wave_cnt = 0;
		afc->offset = 0;
		pr_afc("%s, [post lock --> unlock] set afc offset 0.\n", __func__);
		return;
	}

	if (abs(afc->offset) > afc_limit) {
		afc->no_sig_cnt++;
		if (afc->no_sig_cnt == 20) {
			param->frequency -= afc->offset * 1000;
			pr_afc("%s,afc no_sig trig, set freq:%d\n",
						__func__, param->frequency);

			/*tuner config, no need cvbs format, just audio mode.*/
			p.frequency = param->frequency;
			p.std = (param->std & 0xFF000000) | p.audmode;
			if (fe->ops.tuner_ops.set_frequency)
				fe->ops.tuner_ops.set_frequency(fe, p.frequency);
			else if (fe->ops.tuner_ops.set_analog_params)
				fe->ops.tuner_ops.set_analog_params(fe, &p);

			if (tuner_id == AM_TUNER_MXL661)
				usleep_range(30 * 1000, 30 * 1000 + 100);
			else if (tuner_id == AM_TUNER_R840 ||
					tuner_id == AM_TUNER_R842)
				usleep_range(40 * 1000, 40 * 1000 + 100);
			else if (tuner_id == AM_TUNER_ATBM2040 ||
					tuner_id == AM_TUNER_ATBM253)
				usleep_range(50 * 1000, 50 * 1000 + 100);
			else
				usleep_range(10 * 1000, 10 * 1000 + 100);

			afc->wave_cnt = 0;
			afc->offset = 0;
			afc->status = AFC_LOCK_STATUS_POST_OVER_RANGE;
		}
		return;
	}

	afc->no_sig_cnt = 0;
	if (abs(freq_offset) >= AFC_BEST_LOCK) {
		param->frequency += freq_offset * 1000;
		afc->offset += freq_offset;

		/* tuner config, no need cvbs format, just audio mode. */
		p.frequency = param->frequency;
		p.std = (param->std & 0xFF000000) | p.audmode;
		if (fe->ops.tuner_ops.set_frequency)
			fe->ops.tuner_ops.set_frequency(fe, p.frequency);
		else if (fe->ops.tuner_ops.set_analog_params)
			fe->ops.tuner_ops.set_analog_params(fe, &p);

		if (tuner_id == AM_TUNER_MXL661)
			usleep_range(30 * 1000, 30 * 1000 + 100);
		else if (tuner_id == AM_TUNER_R840 ||
				tuner_id == AM_TUNER_R842)
			usleep_range(40 * 1000, 40 * 1000 + 100);
		else if (tuner_id == AM_TUNER_ATBM2040 ||
				tuner_id == AM_TUNER_ATBM253)
			usleep_range(50 * 1000, 50 * 1000 + 100);
		else
			usleep_range(10 * 1000, 10 * 1000 + 100);

		pr_afc("%s,afc offset:%d KHz, freq_offset:%d KHz, set freq:%d\n",
				__func__, afc->offset, freq_offset, param->frequency);
	}

	afc->wave_cnt = 0;
	afc->status = AFC_LOCK_STATUS_POST_PROCESS;
}

static void atv_demod_afc_timer_handler(struct timer_list *timer)
{
	struct atv_demod_afc *afc = container_of(timer,
			struct atv_demod_afc, timer);
	struct dvb_frontend *fe = afc->fe;
	unsigned int delay_ms = 0;

	if (afc->state == AFC_DISABLE)
		return;

	if (afc->status == AFC_LOCK_STATUS_POST_OVER_RANGE ||
		afc->status == AFC_LOCK_STATUS_PRE_OVER_RANGE ||
		afc->status == AFC_LOCK_STATUS_POST_LOCK)
		delay_ms = afc_timer_delay2;/*100ms*/
	else
		delay_ms = afc_timer_delay;/*10ms*/

	afc->timer.function = atv_demod_afc_timer_handler;
	afc->timer.expires = jiffies + ATVDEMOD_INTERVAL * delay_ms;
	add_timer(&afc->timer);

	if (afc->timer_delay_cnt > 0) {
		afc->timer_delay_cnt--;
		return;
	}

	if (!afc_timer_en || fe->ops.info.type != FE_ANALOG)
		return;

	if (afc->state == AFC_PAUSE)
		return;

	schedule_work(&afc->work);
}

static void atv_demod_afc_disable(struct atv_demod_afc *afc)
{
	mutex_lock(&afc->mtx);

	if (afc_timer_en && (afc->state != AFC_DISABLE)) {
		afc->state = AFC_DISABLE;
		del_timer_sync(&afc->timer);
		cancel_work_sync(&afc->work);
	}

	mutex_unlock(&afc->mtx);

	pr_afc("%s: state: %d.\n", __func__, afc->state);
}

static void atv_demod_afc_enable(struct atv_demod_afc *afc)
{
	mutex_lock(&afc->mtx);

	if (afc_timer_en && (afc->state == AFC_DISABLE)) {
		timer_setup(&afc->timer, atv_demod_afc_timer_handler, 0);
		afc->timer.function = atv_demod_afc_timer_handler;
		/* after afc_timer_delay3 enable demod auto detect */
		afc->timer.expires = jiffies +
				ATVDEMOD_INTERVAL * afc_timer_delay3;
		afc->offset = 0;
		afc->no_sig_cnt = 0;
		afc->pre_step = 0;
		afc->timer_delay_cnt = 20;
		afc->status = AFC_LOCK_STATUS_NULL;
		add_timer(&afc->timer);
		afc->state = AFC_ENABLE;
	} else if (afc_timer_en && (afc->state == AFC_PAUSE)) {
		afc->offset = 0;
		afc->no_sig_cnt = 0;
		afc->pre_step = 0;
		afc->timer_delay_cnt = 20;
		afc->status = AFC_LOCK_STATUS_NULL;
		afc->state = AFC_ENABLE;
	}

	mutex_unlock(&afc->mtx);

	pr_afc("%s: state: %d.\n", __func__, afc->state);
}

static void atv_demod_afc_pause(struct atv_demod_afc *afc)
{
	mutex_lock(&afc->mtx);

	if (afc->state == AFC_ENABLE) {
		afc->state = AFC_PAUSE;
		cancel_work_sync(&afc->work);
	}

	mutex_unlock(&afc->mtx);
}

void atv_demod_afc_init(struct atv_demod_afc *afc)
{
	mutex_lock(&afc_mutex);

	mutex_init(&afc->mtx);

	afc->state = AFC_DISABLE;
	afc->timer_delay_cnt = 0;
	afc->disable = atv_demod_afc_disable;
	afc->enable = atv_demod_afc_enable;
	afc->pause = atv_demod_afc_pause;

	INIT_WORK(&afc->work, atv_demod_afc_do_work);

	mutex_unlock(&afc_mutex);
}
