// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/i2c.h>
#include <uapi/linux/dvb/frontend.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_atvdemod.h>
#include <linux/amlogic/media/vout/vdac_dev.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>

#include <tuner-i2c.h>
#include <dvb_frontend.h>

#include "atvdemod_func.h"
#include "atvauddemod_func.h"
#include "atv_demod_debug.h"
#include "atv_demod_driver.h"
#include "atv_demod_ops.h"
#include "atv_demod_v4l2.h"
#include "atv_demod_afc.h"
#include "atv_demod_monitor.h"
#include "atv_demod_isr.h"
#include "atv_demod_ext.h"

#define DEVICE_NAME "aml_atvdemod"


static DEFINE_MUTEX(atv_demod_list_mutex);
static LIST_HEAD(hybrid_tuner_instance_list);

unsigned int reg_23cf = 0x88188832; /*IIR filter*/
unsigned int btsc_sap_mode = 1; /*0: off 1:monitor 2:auto */


/*
 * add interface for audio driver to get atv audio state.
 * state:
 * 0 - no data.
 * 1 - has data.
 */
void aml_fe_get_atvaudio_state(int *state)
{
#if 0 /* delay notification stable */
	static unsigned int count;
	static bool mute = true;
#endif
	int adc_status = 0;
	int av_status = 0;
	unsigned int power = 0;
	int vpll_lock = 0;
	int line_lock = 0;
	struct atv_demod_priv *priv = amlatvdemod_devp != NULL
			? amlatvdemod_devp->v4l2_fe.fe.analog_demod_priv : NULL;

	if (priv == NULL) {
		pr_audio("priv == NULL\n");
		*state = 0;
		return;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_VDIN
	av_status = tvin_get_av_status();
	adc_status = atv_demod_get_adc_status();
#endif
	/* scan mode need mute */
	if (priv->state == ATVDEMOD_STATE_WORK &&
	    !priv->scanning &&
	    !priv->standby &&
	    av_status &&
	    adc_status) {
		retrieve_vpll_carrier_lock(&vpll_lock);
		retrieve_vpll_carrier_line_lock(&line_lock);
		if ((vpll_lock == 0) && (line_lock == 0)) {
			/* retrieve_vpll_carrier_audio_power(&power, 1); */
			*state = 1;
		} else {
			*state = 0;
			pr_audio("vpll_lock: 0x%x, line_lock: 0x%x\n",
					vpll_lock, line_lock);
		}
	} else {
		*state = 0;
		pr_audio("ATV state[%d], scan[%d], standby[%d], av[%d] adc[%d].\n",
				priv->state, priv->scanning,
				priv->standby, av_status, adc_status);
	}

	/* If the atv signal is locked, it means there is audio data,
	 * so no need to check the power value.
	 */
#if 0
	if (power >= 150)
		*state = 1;
	else
		*state = 0;
#endif
#if 0 /* delay notification stable */
	if (*state) {
		if (mute) {
			count++;
			if (count > 100) {
				count = 0;
				mute = false;
			} else {
				*state = 0;
			}
		} else {
			count = 0;
		}
	} else {
		count = 0;
		mute = true;
	}
#endif
	pr_audio("%s: %d, power = %d\n", __func__, *state, power);
}
EXPORT_SYMBOL_GPL(aml_fe_get_atvaudio_state);

int aml_atvdemod_get_btsc_sap_mode(void)
{
	return btsc_sap_mode;
}

static bool atvdemod_check_exited(struct v4l2_frontend *v4l2_fe)
{
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct atv_demod_priv *priv = fe->analog_demod_priv;

	if (priv->state != ATVDEMOD_STATE_WORK ||
		v4l2_fe->async_tune_needexit(v4l2_fe)) {
		pr_err("%s: need to exit.\n", __func__);

		return true;
	}

	return false;
}

int atv_demod_enter_mode(struct dvb_frontend *fe)
{
	int err_code = 0;
	struct atv_demod_priv *priv = fe->analog_demod_priv;

	atvdemod_power_switch(true);

	if (amlatvdemod_devp->pin_name != NULL) {
		amlatvdemod_devp->agc_pin =
			devm_pinctrl_get_select(amlatvdemod_devp->dev,
				amlatvdemod_devp->pin_name);
		if (IS_ERR(amlatvdemod_devp->agc_pin)) {
			amlatvdemod_devp->agc_pin = NULL;
			pr_err("%s: get agc pins fail\n", __func__);
		}
	}

#ifdef CONFIG_AMLOGIC_MEDIA_ADC
	err_code = adc_set_pll_cntl(1, ADC_ATV_DEMOD, NULL);
	if (err_code) {
		pr_dbg("%s: adc set pll error %d.\n", __func__, err_code);

		if (!IS_ERR_OR_NULL(amlatvdemod_devp->agc_pin)) {
			/*
			 * Is executed only if the pointer is not null.
			 */
			/* coverity[var_deref_model:SUPPRESS] */
			devm_pinctrl_put(amlatvdemod_devp->agc_pin);
			amlatvdemod_devp->agc_pin = NULL;
		}

		atvdemod_power_switch(false);

		return -1;
	}

	adc_set_filter_ctrl(true, FILTER_ATV_DEMOD, NULL);
#else
	if (!IS_ERR_OR_NULL(amlatvdemod_devp->agc_pin)) {
		devm_pinctrl_put(amlatvdemod_devp->agc_pin);
		amlatvdemod_devp->agc_pin = NULL;
	}

	atvdemod_power_switch(false);

	pr_dbg("%s: error, adc pll is not enabled.\n", __func__);

	return -1;
#endif

#ifdef CONFIG_AMLOGIC_VDAC
	vdac_enable(1, VDAC_MODULE_AVOUT_ATV);
#else
	if (!IS_ERR_OR_NULL(amlatvdemod_devp->agc_pin)) {
		devm_pinctrl_put(amlatvdemod_devp->agc_pin);
		amlatvdemod_devp->agc_pin = NULL;
	}

	atvdemod_power_switch(false);

#ifdef CONFIG_AMLOGIC_MEDIA_ADC
	adc_set_pll_cntl(0, ADC_ATV_DEMOD, NULL);
	adc_set_filter_ctrl(false, FILTER_ATV_DEMOD, NULL);
#endif

	pr_dbg("%s: error, vdac is not enabled.\n", __func__);

	return -1;
#endif

	usleep_range(2000, 2100);
	atvdemod_clk_init();
	/* err_code = atvdemod_init(); */

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
		aud_demod_clk_gate(1);
		/* atvauddemod_init(); */
	}

	if (priv->isr.enable)
		priv->isr.enable(&priv->isr);

	amlatvdemod_devp->std = 0;
	amlatvdemod_devp->audmode = 0;
	amlatvdemod_devp->sound_mode = 0xFF;

	pr_info("%s: OK.\n", __func__);

	return err_code;
}

int atv_demod_leave_mode(struct dvb_frontend *fe)
{
	struct atv_demod_priv *priv = fe->analog_demod_priv;

	priv->state = ATVDEMOD_STATE_IDEL;
	priv->standby = true;

	usleep_range(30 * 1000, 30 * 1000 + 100);

	if (priv->isr.disable)
		priv->isr.disable(&priv->isr);

	if (priv->afc.disable)
		priv->afc.disable(&priv->afc);

	if (priv->monitor.disable)
		priv->monitor.disable(&priv->monitor);

	atvdemod_uninit();
	if (!IS_ERR_OR_NULL(amlatvdemod_devp->agc_pin)) {
		devm_pinctrl_put(amlatvdemod_devp->agc_pin);
		amlatvdemod_devp->agc_pin = NULL;
	}

#ifdef CONFIG_AMLOGIC_VDAC
	vdac_enable(0, VDAC_MODULE_AVOUT_ATV);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ADC
	adc_set_pll_cntl(0, ADC_ATV_DEMOD, NULL);
	adc_set_filter_ctrl(false, FILTER_ATV_DEMOD, NULL);
#endif
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX))
		aud_demod_clk_gate(0);

	amlatvdemod_devp->std = 0;
	amlatvdemod_devp->audmode = 0;
	amlatvdemod_devp->sound_mode = 0xFF;

	atvdemod_power_switch(false);

	pr_info("%s: OK.\n", __func__);

	return 0;
}

static void atv_demod_set_params(struct dvb_frontend *fe,
		struct analog_parameters *params)
{
	int ret = -1;
	u32 if_info[2] = { 0 };
	struct atv_demod_priv *priv = fe->analog_demod_priv;
	struct atv_demod_parameters *p = &priv->atvdemod_param;
	struct analog_parameters ptuner = { 0 };

	priv->standby = true;

	/* tuner config, no need cvbs format, just audio mode. */
	ptuner.frequency = params->frequency;
	ptuner.mode = params->mode;
	ptuner.audmode = params->audmode;
	ptuner.std = (params->std & 0xFF000000) | ptuner.audmode;

	/* afc tune disable,must cancel wq before set tuner freq*/
	if (priv->afc.pause)
		priv->afc.pause(&priv->afc);

	if (priv->monitor.pause)
		priv->monitor.pause(&priv->monitor);

	if (fe->ops.tuner_ops.set_analog_params)
		ret = fe->ops.tuner_ops.set_analog_params(fe, &ptuner);

	if (fe->ops.tuner_ops.get_if_frequency)
		ret = fe->ops.tuner_ops.get_if_frequency(fe, if_info);

	p->param.frequency = params->frequency;
	p->param.mode = params->mode;
	p->param.audmode = params->audmode;
	p->param.std = params->std;
	p->last_frequency = params->frequency;

	p->if_inv = if_info[0];
	p->if_freq = if_info[1];

	atvdemod_init(priv);

	if (!priv->scanning)
		atvauddemod_init();

	/* afc tune enable */
	/* analog_search_flag == 0 or afc_range != 0 means searching */
	if ((fe->ops.info.type == FE_ANALOG)
			&& (priv->scanning == false)
			&& (p->param.mode == 0)) {
		if (priv->afc.enable && non_std_en == 0)
			priv->afc.enable(&priv->afc);

		if (priv->monitor.enable && non_std_en == 0)
			priv->monitor.enable(&priv->monitor);

		/* for searching mute audio */
		priv->standby = false;

		pr_dbg("%s: frequency %d.\n", __func__, p->param.frequency);
	}
}

static int atv_demod_has_signal(struct dvb_frontend *fe, u16 *signal)
{
	int vpll_lock = 0;
	int line_lock = 0;

	retrieve_vpll_carrier_lock(&vpll_lock);

	/* add line lock status for atv scan */
	retrieve_vpll_carrier_line_lock(&line_lock);

	if (vpll_lock == 0 && line_lock == 0) {
		*signal = V4L2_HAS_LOCK;
		pr_info("%s locked [vpll_lock: 0x%x, line_lock:0x%x]\n",
				__func__, vpll_lock, line_lock);
	} else {
		*signal = V4L2_TIMEDOUT;
		pr_dbg("%s unlocked [vpll_lock: 0x%x, line_lock:0x%x]\n",
				__func__, vpll_lock, line_lock);
	}

	return 0;
}

static void atv_demod_standby(struct dvb_frontend *fe)
{
	struct atv_demod_priv *priv = fe->analog_demod_priv;

	if (priv->state != ATVDEMOD_STATE_IDEL) {
		atv_demod_leave_mode(fe);
		priv->state = ATVDEMOD_STATE_SLEEP;
		priv->standby = true;
	}

	pr_info("%s: OK.\n", __func__);
}

static void atv_demod_tuner_status(struct dvb_frontend *fe)
{
	pr_info("%s.\n", __func__);
}

static int atv_demod_get_afc(struct dvb_frontend *fe, s32 *afc)
{
	*afc = retrieve_vpll_carrier_afc();

	return 0;
}

static void atv_demod_release(struct dvb_frontend *fe)
{
	int instance = 0;
	struct atv_demod_priv *priv = fe->analog_demod_priv;

	mutex_lock(&atv_demod_list_mutex);

	atv_demod_leave_mode(fe);

	if (priv) {
		if (amlatvdemod_devp->irq > 0)
			atv_demod_isr_uninit(&priv->isr);
		instance = hybrid_tuner_release_state(priv);
	}

	if (instance == 0)
		fe->analog_demod_priv = NULL;

	mutex_unlock(&atv_demod_list_mutex);

	pr_info("%s: OK.\n", __func__);
}

static int atv_demod_set_config(struct dvb_frontend *fe, void *priv_cfg)
{
	int ret = 0;
	int *state = (int *) priv_cfg;
	struct atv_demod_priv *priv = fe->analog_demod_priv;

	if (!state) {
		pr_err("%s: state == NULL.\n", __func__);
		return -1;
	}

	mutex_lock(&atv_demod_list_mutex);

	switch (*state) {
	case AML_ATVDEMOD_INIT:
		if (priv->state != ATVDEMOD_STATE_WORK) {
			priv->standby = true;
			if (fe->ops.tuner_ops.set_config)
				fe->ops.tuner_ops.set_config(fe, NULL);

			ret = atv_demod_enter_mode(fe);
			if (!ret) {
				priv->state = ATVDEMOD_STATE_WORK;
			} else {
				if (fe->ops.tuner_ops.release)
					fe->ops.tuner_ops.release(fe);
			}
		}
		break;

	case AML_ATVDEMOD_UNINIT:
		if (priv->state != ATVDEMOD_STATE_IDEL) {
			atv_demod_leave_mode(fe);
			if (fe->ops.tuner_ops.release)
				fe->ops.tuner_ops.release(fe);
		}
		break;

	case AML_ATVDEMOD_RESUME:
		if (priv->state == ATVDEMOD_STATE_SLEEP) {
			ret = atv_demod_enter_mode(fe);
			if (!ret) {
				priv->state = ATVDEMOD_STATE_WORK;
				priv->standby = false;
			}
		}
		break;

	case AML_ATVDEMOD_SCAN_MODE:
		priv->scanning = true;
		if (priv->afc.disable)
			priv->afc.disable(&priv->afc);

		if (priv->monitor.disable)
			priv->monitor.disable(&priv->monitor);

		aml_fe_hook_call_set_mode(true);
		break;

	case AML_ATVDEMOD_UNSCAN_MODE:
		priv->scanning = false;
		/* No need to enable when exiting the scan,
		 * but enable when actually played.
		 */
#if 0
		if (priv->afc.enable)
			priv->afc.enable(&priv->afc);

		if (priv->monitor.enable)
			priv->monitor.enable(&priv->monitor);
#endif
		aml_fe_hook_call_set_mode(false);
		break;
	}

	mutex_unlock(&atv_demod_list_mutex);

	return ret;
}

static struct analog_demod_ops atvdemod_ops = {
	.info = {
		.name = DEVICE_NAME,
	},
	.set_params     = atv_demod_set_params,
	.has_signal     = atv_demod_has_signal,
	.standby        = atv_demod_standby,
	.tuner_status   = atv_demod_tuner_status,
	.get_afc        = atv_demod_get_afc,
	.release        = atv_demod_release,
	.set_config     = atv_demod_set_config,
	.i2c_gate_ctrl  = NULL,
};


unsigned int tuner_status_cnt = 4; /* 4-->16 test on sky mxl661 */
/* 0: no check, 1: check */
bool check_rssi;
/* Less than -85, it means no signal */
int tuner_rssi = -80;

/* when need to support secam-l, will enable it */
bool support_secam_l;

bool slow_mode;


static v4l2_std_id atvdemod_fmt_2_v4l2_std(int fmt)
{
	v4l2_std_id std = 0;

	switch (fmt) {
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_DK:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK:
		std = V4L2_STD_PAL_DK;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_I:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_I:
		std = V4L2_STD_PAL_I;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_BG:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_BG:
		std = V4L2_STD_PAL_BG;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M:
		std = V4L2_STD_PAL_M;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC_M:
		std = V4L2_STD_NTSC_M;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L:
		std = V4L2_STD_SECAM_L;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_LC:
		std = V4L2_STD_SECAM_LC;
		break;
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK2:
	case AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_DK3:
		std = V4L2_STD_SECAM_DK;
		break;
	default:
		pr_err("%s: Unsupport fmt: 0x%0x.\n", __func__, fmt);
	}

	return std;
}

static v4l2_std_id atvdemod_fe_tvin_fmt_to_v4l2_std(int fmt)
{
	v4l2_std_id std = 0;

	switch (fmt) {
	case TVIN_SIG_FMT_CVBS_NTSC_M:
		std = V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_M;
		break;
	case TVIN_SIG_FMT_CVBS_NTSC_443:
		std = V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_443;
		break;
	case TVIN_SIG_FMT_CVBS_NTSC_50:
		std = V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_M;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_I:
		std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_I;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_M:
		std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_M;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_60:
		std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_60;
		break;
	case TVIN_SIG_FMT_CVBS_PAL_CN:
		std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_Nc;
		break;
	case TVIN_SIG_FMT_CVBS_SECAM:
		std = V4L2_COLOR_STD_SECAM | V4L2_STD_SECAM_L;
		break;
	default:
		pr_err("%s: Unsupport fmt: 0x%x\n", __func__, fmt);
		break;
	}

	return std;
}

static void atvdemod_fe_try_analog_format(struct v4l2_frontend *v4l2_fe,
		int auto_search_std, v4l2_std_id *video_fmt,
		unsigned int *audio_fmt, unsigned int *soundsys)
{
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	struct analog_parameters params;
	struct atv_demod_priv *priv = fe->analog_demod_priv;
	unsigned int tuner_id = priv->atvdemod_param.tuner_id;
	int i = 0;
	int try_vfmt_cnt = 300;
	int verify_cnt = 0;
	int cvbs_std = 0;
	v4l2_std_id std_bk = 0;
	unsigned int broad_std = 0;
	unsigned int audio = 0;

	*video_fmt = 0;
	*audio_fmt = 0;
	*soundsys = 0;

	if (auto_search_std & AUTO_DETECT_COLOR) {
		for (i = 0; i < try_vfmt_cnt; i++) {

			if (atvdemod_check_exited(v4l2_fe))
				return;

			/* SECAM-L/L' */
			if ((p->std & (V4L2_STD_SECAM_L | V4L2_STD_SECAM_LC))
				&& (p->std & V4L2_COLOR_STD_SECAM)) {
				cvbs_std = TVIN_SIG_FMT_CVBS_SECAM;
				break;
			}

			if (aml_fe_hook_call_get_fmt(&cvbs_std) == false) {
				pr_err("%s: aml_fe_hook_get_fmt == NULL.\n",
						__func__);
				break;
			}

			if (cvbs_std) {
				verify_cnt++;
				i--;//confirm format prevent switch format
				pr_dbg("get cvbs_std verify_cnt:%d, cnt:%d, cvbs_std:0x%x\n",
						verify_cnt, i,
						(unsigned int) cvbs_std);
				if (((tuner_id == AM_TUNER_R840 ||
					tuner_id == AM_TUNER_R842) &&
					verify_cnt > 0) ||
					verify_cnt > 3)
					break;
			}

			if (i == (try_vfmt_cnt / 3) ||
				(i == (try_vfmt_cnt / 3) * 2)) {
				/* Before enter search,
				 * need set the std,
				 * then, try others std.
				 */
				if (p->std & V4L2_COLOR_STD_PAL) {
					p->std = V4L2_COLOR_STD_NTSC
					| V4L2_STD_NTSC_M;
					p->audmode = V4L2_STD_NTSC_M;
#if 0 /*for secam */
				} else if (p->std & V4L2_COLOR_STD_NTSC) {
					p->std = V4L2_COLOR_STD_SECAM
					| V4L2_STD_SECAM;
					p->audmode = V4L2_STD_SECAM;
#endif
				} else if (p->std & V4L2_COLOR_STD_NTSC) {
					p->std = V4L2_COLOR_STD_PAL
					| V4L2_STD_PAL_DK;
					p->audmode = V4L2_STD_PAL_DK;
				}
				pr_info("%s:%d set new std:%#x %#x %s\n", __func__,
					i, (unsigned int)p->std, p->audmode,
					v4l2_std_to_str(p->std & 0xFF000000));
				p->frequency += 1;
				params.frequency = p->frequency;
				params.mode = p->afc_range;
				params.audmode = p->audmode;
				params.std = p->std;

				fe->ops.analog_ops.set_params(fe, &params);
			}
			usleep_range(30 * 1000, 30 * 1000 + 100);
		}
		if (cvbs_std == 0) {
			if (aml_fe_hook_call_force_fmt(&cvbs_std) == false)
				pr_err("%s: aml_fe_hook_force_fmt == NULL.\n", __func__);
		}
		pr_dbg("get cvbs_std cnt:%d, cvbs_std: 0x%x\n",
				i, (unsigned int) cvbs_std);

		if (cvbs_std == 0) {
			pr_err("%s: failed to get video fmt, assume PAL.\n",
					__func__);
			cvbs_std = TVIN_SIG_FMT_CVBS_PAL_I;
			p->std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_DK;
			p->frequency += 1;
			p->audmode = V4L2_STD_PAL_DK;

			params.frequency = p->frequency;
			params.mode = p->afc_range;
			params.audmode = p->audmode;
			params.std = p->std;

			fe->ops.analog_ops.set_params(fe, &params);

			usleep_range(20 * 1000, 20 * 1000 + 100);
		}

		std_bk = atvdemod_fe_tvin_fmt_to_v4l2_std(cvbs_std);
	} else {
		/* Only search std by user setting,
		 * so no need tvafe identify signal.
		 */
		std_bk = p->std;
	}

	*video_fmt = std_bk;

	if (!(auto_search_std & AUTO_DETECT_AUDIO)) {
		*audio_fmt = p->audmode;
		return;
	}

	if (std_bk & V4L2_COLOR_STD_NTSC) {
#if 1 /* For TV Signal Generator(TG39) test, NTSC need support other audio.*/
		if (cvbs_std == TVIN_SIG_FMT_CVBS_NTSC_M) {
			broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M;
			audio = V4L2_STD_NTSC_M;
		} else {
			amlatvdemod_set_std(AML_ATV_DEMOD_VIDEO_MODE_PROP_NTSC);
			broad_std = aml_audiomode_autodet(v4l2_fe);
			audio = atvdemod_fmt_2_v4l2_std(broad_std);
		}
#if 0 /* I don't know what's going on here */
		if (audio == V4L2_STD_PAL_M)
			audio = V4L2_STD_NTSC_M;
		else
			std_bk = V4L2_COLOR_STD_PAL;
#endif
#else /* Now, force to NTSC_M, Ours demod only support M for NTSC.*/
		audio = V4L2_STD_NTSC_M;
#endif
	} else if (std_bk & V4L2_COLOR_STD_SECAM) {
#if 1 /* For support SECAM-DK/BG/I/L */
		amlatvdemod_set_std(AML_ATV_DEMOD_VIDEO_MODE_PROP_SECAM_L);
		broad_std = aml_audiomode_autodet(v4l2_fe);
		audio = atvdemod_fmt_2_v4l2_std(broad_std);
#else /* For force L */
		audio = V4L2_STD_SECAM_L;
#endif
	} else {
		/* V4L2_COLOR_STD_PAL */
		if (cvbs_std == TVIN_SIG_FMT_CVBS_PAL_M ||
			cvbs_std == TVIN_SIG_FMT_CVBS_PAL_CN) {
			broad_std = AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_M;
			audio = V4L2_STD_PAL_M;
		} else {
			amlatvdemod_set_std(
					AML_ATV_DEMOD_VIDEO_MODE_PROP_PAL_DK);
			broad_std = aml_audiomode_autodet(v4l2_fe);
			audio = atvdemod_fmt_2_v4l2_std(broad_std);
		}
#if 0 /* Why do this to me? We need support PAL_M.*/
		if (audio == V4L2_STD_PAL_M) {
			audio = atvdemod_fmt_2_v4l2_std(broad_std_except_pal_m);
			pr_info("select audmode 0x%x\n", audio);
		}
#endif
	}

	*audio_fmt = audio;

#if 0 /* no detect when searching */
	/* for audio standard detection */
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXLX)) {
		*soundsys = amlfmt_aud_standard(broad_std);
		*soundsys = (*soundsys << 16) | 0x00FFFF;
	} else
#endif
		*soundsys = 0xFFFFFF;

	pr_info("auto detect audio broad_std %d, [%s][0x%x] soundsys[0x%x]\n",
			broad_std, v4l2_std_to_str(audio), audio, *soundsys);
}

static void atvdemod_fe_try_signal(struct v4l2_frontend *v4l2_fe,
		int auto_search, bool *lock)
{
	struct analog_parameters params;
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct atv_demod_priv *priv = fe->analog_demod_priv;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	enum v4l2_status tuner_state = V4L2_TIMEDOUT;
	enum v4l2_status ade_state = V4L2_TIMEDOUT;
	int try_cnt = tuner_status_cnt;
	v4l2_std_id std_bk = 0;
	unsigned int audio = 0;
	bool try_secaml = false;
	bool try_secamlc = false;
	unsigned int tuner_id = priv->atvdemod_param.tuner_id;
	s16 strength = 0;

	if (fe->ops.analog_ops.set_params) {
		params.frequency = p->frequency;
		params.mode = p->afc_range;
		params.audmode = p->audmode;
		params.std = p->std;
		fe->ops.analog_ops.set_params(fe, &params);
	}

	/* backup the std and audio mode */
	std_bk = p->std;
	audio = p->audmode;

	*lock = false;
	do {
		if (atvdemod_check_exited(v4l2_fe))
			break;

		if (tuner_id == AM_TUNER_MXL661) {
			usleep_range(30 * 1000, 30 * 1000 + 100);
		} else if (tuner_id == AM_TUNER_R840 ||
				tuner_id == AM_TUNER_R842) {
			usleep_range(40 * 1000, 40 * 1000 + 100);
			fe->ops.tuner_ops.get_status(fe, (u32 *)&tuner_state);
		} else if (tuner_id == AM_TUNER_ATBM2040 ||
				tuner_id == AM_TUNER_ATBM253) {
			usleep_range(50 * 1000, 50 * 1000 + 100);
		} else {
			/* AM_TUNER_SI2151 and AM_TUNER_SI2159 */
			usleep_range(10 * 1000, 10 * 1000 + 100);
		}

		/* Add tuner rssi strength check */
		if (fe->ops.tuner_ops.get_strength && check_rssi) {
			fe->ops.tuner_ops.get_strength(fe, &strength);
			if (strength < tuner_rssi) {
				pr_err("[%s] freq: %d tuner RSSI [%d] less than [%d].\n",
						__func__, p->frequency,
						strength, tuner_rssi);
				break;
			}
		}

		fe->ops.analog_ops.has_signal(fe, (u16 *)&ade_state);
		try_cnt--;
		if (((ade_state == V4L2_HAS_LOCK ||
			tuner_state == V4L2_HAS_LOCK) &&
			(tuner_id != AM_TUNER_R840 &&
			tuner_id != AM_TUNER_R842)) ||
			((ade_state == V4L2_HAS_LOCK &&
			tuner_state == V4L2_HAS_LOCK) &&
			(tuner_id == AM_TUNER_R840 ||
			tuner_id == AM_TUNER_R842))) {
			*lock = true;
			break;
		}

		if (try_cnt == 0) {
			if (support_secam_l && auto_search) {
				if (!(p->std & V4L2_STD_SECAM_L) &&
					!try_secaml) {
					p->std = (V4L2_COLOR_STD_SECAM
						| V4L2_STD_SECAM_L);
					p->audmode = V4L2_STD_SECAM_L;

					try_secaml = true;
				} else if (!(p->std & V4L2_STD_SECAM_LC) &&
					!try_secamlc &&
					p->frequency <= ATV_SECAM_LC_100MHZ) {

					p->std = (V4L2_COLOR_STD_SECAM
						| V4L2_STD_SECAM_LC);
					p->audmode = V4L2_STD_SECAM_LC;

					try_secamlc = true;
				} else {
					break;
				}

				params.frequency = p->frequency;
				params.mode = p->afc_range;
				params.audmode = p->audmode;
				params.std = p->std;
				fe->ops.analog_ops.set_params(fe, &params);

				if (tuner_status_cnt > 2)
					try_cnt = tuner_status_cnt / 2;
				else
					try_cnt = tuner_status_cnt;

				continue;
			}

			break;
		}
	} while (1);

	if (*lock == false && (try_secaml || try_secamlc)) {
		p->std = std_bk;
		p->audmode = audio;

		params.frequency = p->frequency;
		params.mode = p->afc_range;
		params.audmode = p->audmode;
		params.std = p->std;
		fe->ops.analog_ops.set_params(fe, &params);
	}
}

static int atvdemod_fe_afc_closer(struct v4l2_frontend *v4l2_fe, int minafcfreq,
		int maxafcfreq, int isAutoSearch)
{
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	struct analog_parameters params;
	struct atv_demod_priv *priv = fe->analog_demod_priv;
	int afc = 100;
	__u32 set_freq = 0;
	int count = 25;
	int lock_cnt = 0;
	int temp_freq, temp_afc;
	unsigned int tuner_id = priv->atvdemod_param.tuner_id;

	pr_dbg("[%s] freq: %d, minfreq: %d, maxfreq: %d\n",
		__func__, p->frequency, minafcfreq, maxafcfreq);

	/*do the auto afc make sure the afc < 50k or the range from api */
	if ((fe->ops.analog_ops.get_afc || fe->ops.tuner_ops.get_afc) &&
		fe->ops.tuner_ops.set_analog_params) {

		set_freq = p->frequency;
		while (abs(afc) > AFC_BEST_LOCK) {
			if (atvdemod_check_exited(v4l2_fe))
				return -1;

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

			if (fe->ops.analog_ops.get_afc)
				fe->ops.analog_ops.get_afc(fe, &afc);
			else if (fe->ops.tuner_ops.get_afc)
				fe->ops.tuner_ops.get_afc(fe, &afc);

			pr_dbg("[%s] get afc %d khz, freq %u.\n",
					__func__, afc, p->frequency);

			if (afc == 0xffff) {
				/*last lock, but this unlock,so try get afc*/
				if (lock_cnt > 0) {
					p->frequency = temp_freq +
							temp_afc * 1000;
					pr_err("[%s] force lock, f:%d\n",
							__func__, p->frequency);
					break;
				}

				afc = 500;
			} else {
				lock_cnt++;
				temp_freq = p->frequency;
				if (afc > 50)
					temp_afc = 500;
				else if (afc < -50)
					temp_afc = -500;
				else
					temp_afc = afc;
			}

			if (((abs(afc) > (500 - AFC_BEST_LOCK))
				&& (abs(afc) < (500 + AFC_BEST_LOCK))
				&& (abs(afc) != 500))
				|| ((abs(afc) == 500) && (lock_cnt > 0))) {
				p->frequency += afc * 1000;
				break;
			}

			if (afc >= (500 + AFC_BEST_LOCK))
				afc = 500;

			p->frequency += afc * 1000;

			if (unlikely(p->frequency > maxafcfreq)) {
				pr_err("[%s] [%d] is exceed maxafcfreq[%d]\n",
					__func__, p->frequency, maxafcfreq);
				p->frequency = set_freq;
				return -1;
			}
#if 0 /*if enable, it would miss program*/
			if (unlikely(c->frequency < minafcfreq)) {
				pr_dbg("[%s] [%d] is exceed minafcfreq[%d]\n",
						__func__,
						c->frequency, minafcfreq);
				c->frequency = set_freq;
				return -1;
			}
#endif
			if (likely(!(count--))) {
				pr_err("[%s] exceed the afc count\n", __func__);
				p->frequency = set_freq;
				return -1;
			}

			/* delete it will miss program
			 * when c->frequency equal program frequency
			 */
			p->frequency++;
			if (fe->ops.tuner_ops.set_analog_params) {
				params.frequency = p->frequency;
				params.mode = p->afc_range;
				params.audmode = p->audmode;
				params.std = p->std;
				fe->ops.tuner_ops.set_analog_params(fe, &params);
			}
		}

		/* After correcting the frequency offset success,
		 * need to set up tuner.
		 */
		if (fe->ops.tuner_ops.set_analog_params) {
			params.frequency = p->frequency;
			params.mode = p->afc_range;
			params.audmode = p->audmode;
			params.std = p->std;
			fe->ops.tuner_ops.set_analog_params(fe,
					&params);
		}

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

		pr_dbg("[%s] get afc %d khz done, freq %u.\n",
				__func__, afc, p->frequency);
	}

	return 0;
}

static int atvdemod_fe_set_property(struct v4l2_frontend *v4l2_fe,
		struct v4l2_property *tvp)
{
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct atv_demod_priv *priv = fe->analog_demod_priv;
	struct v4l2_analog_parameters *params = &v4l2_fe->params;

	pr_dbg("%s: cmd = 0x%x.\n", __func__, tvp->cmd);

	switch (tvp->cmd) {
	case V4L2_SOUND_SYS:
		/* aud_mode = tvp->data & 0xFF; */
		amlatvdemod_devp->sound_mode = tvp->data & 0xFF;
		if (amlatvdemod_devp->sound_mode != 0xFF) {
			aud_mode = amlatvdemod_devp->sound_mode;
			params->soundsys = params->soundsys | aud_mode;
		}
		priv->atvdemod_sound.output_mode = tvp->data & 0xFF;
		break;

	case V4L2_SLOW_SEARCH_MODE:
		slow_mode = tvp->data;
		break;

	case V4L2_SIF_OVER_MODULATION:
		priv->atvdemod_sound.sif_over_modulation = tvp->data;
		break;

	case V4L2_ENABLE_AFC:
		afc_timer_en = tvp->data;
		break;

	default:
		break;
	}

	return 0;
}

static int atvdemod_fe_get_property(struct v4l2_frontend *v4l2_fe,
		struct v4l2_property *tvp)
{
	pr_dbg("%s: cmd = 0x%x.\n", __func__, tvp->cmd);

	switch (tvp->cmd) {
	case V4L2_SOUND_SYS:
		tvp->data = ((aud_std & 0xFF) << 16)
				| ((signal_audmode & 0xFF) << 8)
				| (aud_mode & 0xFF);
		break;

	case V4L2_SLOW_SEARCH_MODE:
		tvp->data = slow_mode;
		break;

	case V4L2_AFC_STATE:
		tvp->data = afc_timer_en;
		break;

	default:
		break;
	}

	return 0;
}

static int atvdemod_fe_tune(struct v4l2_frontend *v4l2_fe,
		struct v4l2_tune_status *status)
{
	bool lock = false;
	int priv_cfg = 0;
	int try_cnt = 4;
	int auto_search_std = 0;
	enum v4l2_status state = V4L2_TIMEDOUT;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct atv_demod_priv *priv = fe->analog_demod_priv;
	unsigned int tuner_id = priv->atvdemod_param.tuner_id;

	/* for scan tune */
	if (p->flag & ANALOG_FLAG_ENABLE_AFC) {
		if (p->std == 0) {
			if (tuner_id == AM_TUNER_ATBM2040)
				p->std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_DK;
			else
				p->std = V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_M;
			auto_search_std = AUTO_DETECT_COLOR;
			pr_dbg("[%s] user std is 0, so set it to %s.\n",
				__func__, v4l2_std_to_str(p->std & 0xFF000000));
		}

		if (p->audmode == 0) {
			if (auto_search_std) {
				p->audmode = p->std & 0x00FFFFFF;
			} else {
				if (p->std & V4L2_COLOR_STD_PAL)
					p->audmode = V4L2_STD_PAL_DK;
				else if (p->std & V4L2_COLOR_STD_NTSC)
					p->audmode = V4L2_STD_NTSC_M;
				else if (p->std & V4L2_COLOR_STD_SECAM)
					p->audmode = V4L2_STD_PAL_DK;

				p->std = (p->std & 0xFF000000) | p->audmode;
			}
			auto_search_std |= AUTO_DETECT_AUDIO;
			pr_dbg("[%s] user audmode is 0, so set it to %s.\n",
				__func__, v4l2_std_to_str(p->audmode));
		}

		priv_cfg = AML_ATVDEMOD_SCAN_MODE;
		if (fe->ops.analog_ops.set_config)
			fe->ops.analog_ops.set_config(fe, &priv_cfg);

		atvdemod_fe_try_signal(v4l2_fe, 0, &lock);
	} else { /* for play tune */
		if (fe->ops.analog_ops.has_signal)
			fe->ops.analog_ops.has_signal(fe, (u16 *) &state);

		if (state == V4L2_HAS_LOCK)
			lock = true;
		else
			lock = false;
	}

	if (lock) {
		status->lock = 1;
		while (try_cnt--) {
			status->afc = retrieve_vpll_carrier_afc();

			if (status->afc < 1500)
				break;

			usleep_range(5 * 1000, 5 * 1000 + 100);
		}
	} else {
		status->lock = 0;
		status->afc = 0;
	}

	pr_info("[%s] lock: [%d], afc: [%d], freq: [%d], flag: [%d].\n",
				__func__, status->lock, status->afc,
				p->frequency, p->flag);

	if (p->flag & ANALOG_FLAG_ENABLE_AFC) {
		priv_cfg = AML_ATVDEMOD_UNSCAN_MODE;
		if (fe->ops.analog_ops.set_config)
			fe->ops.analog_ops.set_config(fe, &priv_cfg);
	}

	return 0;
}

static int atvdemod_fe_detect(struct v4l2_frontend *v4l2_fe)
{
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	struct dvb_frontend *fe = &v4l2_fe->fe;
	int priv_cfg = 0;
	v4l2_std_id std_bk = 0;
	unsigned int audio = 0;
	unsigned int soundsys = 0;
	int auto_detect = AUTO_DETECT_COLOR | AUTO_DETECT_AUDIO;

	priv_cfg = AML_ATVDEMOD_SCAN_MODE;
	if (fe->ops.analog_ops.set_config)
		fe->ops.analog_ops.set_config(fe, &priv_cfg);

	atvdemod_fe_try_analog_format(v4l2_fe, auto_detect,
			&std_bk, &audio, &soundsys);
	if (std_bk != 0) {
		p->audmode = audio;
		p->std = std_bk;
		p->soundsys = soundsys;
		std_bk = 0;
		audio = 0;
	}

	priv_cfg = AML_ATVDEMOD_UNSCAN_MODE;
	if (fe->ops.analog_ops.set_config)
		fe->ops.analog_ops.set_config(fe, &priv_cfg);

	return 0;
}

static enum v4l2_search atvdemod_fe_search(struct v4l2_frontend *v4l2_fe)
{
	/* struct analog_parameters params; */
	struct dvb_frontend *fe = &v4l2_fe->fe;
	struct atv_demod_priv *priv = NULL;
	struct v4l2_analog_parameters *p = &v4l2_fe->params;
	/*enum v4l2_status tuner_state = V4L2_TIMEDOUT;*/
	/*enum v4l2_status ade_state = V4L2_TIMEDOUT;*/
	bool pll_lock = false;
	/*struct atv_status_s atv_status;*/
	__u32 set_freq = 0;
	__u32 minafcfreq = 0, maxafcfreq = 0;
	__u32 afc_step = 0;
	/* int tuner_status_cnt_local = tuner_status_cnt; */
	v4l2_std_id std_bk = 0;
	unsigned int audio = 0;
	unsigned int soundsys = 0;
	/* int double_check_cnt = 1; */
	int auto_search_std = 0;
	int search_count = 0;
	/* bool try_secam = false; */
	int ret = -1;
	unsigned int tuner_id = 0;
	int priv_cfg = 0;
	int exit_status = 0;
	char *exit_str = "";

	if (unlikely(!fe || !p ||
			!fe->ops.tuner_ops.get_status ||
			!fe->ops.analog_ops.has_signal ||
			!fe->ops.analog_ops.set_params ||
			!fe->ops.analog_ops.set_config ||
			(aml_fe_has_hook_up() == false))) {
		pr_err("[%s] error: NULL function or pointer.\n", __func__);
		return V4L2_SEARCH_INVALID;
	}

	priv = fe->analog_demod_priv;
	if (atvdemod_check_exited(v4l2_fe)) {
		pr_err("[%s] ATV state is not work.\n", __func__);
		return V4L2_SEARCH_INVALID;
	}

	if (p->afc_range == 0) {
		pr_err("[%s] afc_range == 0, skip the search\n", __func__);

		return V4L2_SEARCH_INVALID;
	}

	tuner_id = priv->atvdemod_param.tuner_id;

	pr_info("[%s] afc_range: [%d], tuner: [%d], freq: [%d], flag: [%d].\n",
			__func__, p->afc_range, tuner_id,
			p->frequency, p->flag);

	/* backup the freq by api */
	set_freq = p->frequency;

	/* Before enter search, need set the std first.
	 * If set p->analog.std == 0, will search all std (PAL/NTSC/SECAM),
	 * and need tvafe identify signal type.
	 */
	if (p->std == 0) {
		if (tuner_id == AM_TUNER_ATBM2040)
			p->std = V4L2_COLOR_STD_PAL | V4L2_STD_PAL_DK;
		else
			p->std = V4L2_COLOR_STD_NTSC | V4L2_STD_NTSC_M;
		auto_search_std = AUTO_DETECT_COLOR;
		pr_dbg("[%s] user std is 0, so set it to %s.\n",
				__func__, v4l2_std_to_str(p->std & 0xFF000000));
	}

	if (p->audmode == 0) {
		if (auto_search_std) {
			p->audmode = p->std & 0x00FFFFFF;
		} else {
			if (p->std & V4L2_COLOR_STD_PAL)
				p->audmode = V4L2_STD_PAL_DK;
			else if (p->std & V4L2_COLOR_STD_NTSC)
				p->audmode = V4L2_STD_NTSC_M;
			else if (p->std & V4L2_COLOR_STD_SECAM)
				p->audmode = V4L2_STD_PAL_DK;

			p->std = (p->std & 0xFF000000) | p->audmode;
		}
		auto_search_std |= AUTO_DETECT_AUDIO;
		pr_dbg("[%s] user audmode is 0, so set it to %s.\n",
				__func__, v4l2_std_to_str(p->audmode));
	}

	priv_cfg = AML_ATVDEMOD_SCAN_MODE;
	fe->ops.analog_ops.set_config(fe, &priv_cfg);

	/*set the afc_range and start freq*/
	minafcfreq = p->frequency - p->afc_range;
	maxafcfreq = p->frequency + p->afc_range;

	/*from the current freq start, and set the afc_step*/
	/*if step is 2Mhz,r840 will miss program*/
	if (slow_mode || p->afc_range == ATV_AFC_1_0MHZ) {
		pr_dbg("[%s] slow mode to search the channel\n", __func__);
		afc_step = ATV_AFC_1_0MHZ;
	} else if (!slow_mode) {
		if ((tuner_id == AM_TUNER_R840 || tuner_id == AM_TUNER_R842) &&
			p->afc_range >= ATV_AFC_2_0MHZ) {
			pr_dbg("[%s] r842/r840 use slow mode to search the channel\n",
					__func__);
			afc_step = ATV_AFC_1_0MHZ;
		} else {
			afc_step = p->afc_range;
		}
	} else {
		pr_dbg("[%s] default use slow mode to search the channel\n",
				__func__);
		afc_step = ATV_AFC_1_0MHZ;
	}

	/**enter auto search mode**/
	pr_dbg("[%s] Auto search std: 0x%08x, audmode: 0x%08x\n",
			__func__, (unsigned int) p->std, p->audmode);

	while (minafcfreq <= p->frequency &&
			p->frequency <= maxafcfreq) {

		if (atvdemod_check_exited(v4l2_fe)) {
			exit_status = 1;
			break;
		}

		pr_dbg("[%s] [%d] is processing, [min=%d, max=%d].\n",
				__func__, p->frequency, minafcfreq, maxafcfreq);

		pll_lock = false;
		atvdemod_fe_try_signal(v4l2_fe, auto_search_std, &pll_lock);

		std_bk = 0;
		audio = 0;

		if (pll_lock) {

			pr_dbg("[%s] freq: [%d] pll lock success\n",
					__func__, p->frequency);

			ret = atvdemod_fe_afc_closer(v4l2_fe, minafcfreq,
					maxafcfreq + ATV_AFC_500KHZ, 1);
			if (ret == 0) {
				atvdemod_fe_try_analog_format(v4l2_fe,
						auto_search_std,
						&std_bk, &audio, &soundsys);

				pr_info("[%s] freq:%d, std_bk:0x%x, audmode:0x%x, search OK.\n",
						__func__, p->frequency,
						(unsigned int) std_bk, audio);

				if (std_bk != 0) {
					p->audmode = audio;
					p->std = std_bk;
					/*avoid std unenable */
					p->frequency -= 1;
					p->soundsys = soundsys;
					std_bk = 0;
					audio = 0;
				} else {
					exit_status = 1;
					break;
				}

				/* sync param */
				/* aml_fe_analog_sync_frontend(fe); */
				priv_cfg = AML_ATVDEMOD_UNSCAN_MODE;
				fe->ops.analog_ops.set_config(fe, &priv_cfg);
				return V4L2_SEARCH_SUCCESS;
			}
		}

		pr_dbg("[%s] freq[analog.std:0x%08x] is[%d] unlock\n",
				__func__,
				(uint32_t) p->std, p->frequency);

		/* when manual search, just search current freq */
		if (p->flag == ANALOG_FLAG_MANUL_SCAN) {
			exit_status = 2;
			break;
		}

#ifdef DOUBLE_CHECK_44_25MHZ
		if (p->frequency >= 44200000 &&
			p->frequency <= 44300000 &&
			double_check_cnt) {
			double_check_cnt--;
			pr_err("%s 44.25Mhz double check\n", __func__);
		} else {
			++search_count;
			p->frequency += afc_step * ((search_count % 2) ?
					-search_count : search_count);
		}
#else
		++search_count;
		p->frequency += afc_step * ((search_count % 2) ?
				-search_count : search_count);
#endif
	}

	if (!exit_status)
		exit_str = "over of range, search failed";
	else if (exit_status == 1)
		exit_str = "search exited";
	else
		exit_str = "search failed";

	pr_dbg("[%s] [%d] %s.\n", __func__, p->frequency, exit_str);

	p->frequency = set_freq;

	priv_cfg = AML_ATVDEMOD_UNSCAN_MODE;
	fe->ops.analog_ops.set_config(fe, &priv_cfg);

	return V4L2_SEARCH_FAILED;
}

static struct v4l2_frontend_ops atvdemod_fe_ops = {
	.set_property = atvdemod_fe_set_property,
	.get_property = atvdemod_fe_get_property,
	.tune = atvdemod_fe_tune,
	.detect = atvdemod_fe_detect,
	.search = atvdemod_fe_search,
};

struct dvb_frontend *aml_atvdemod_attach(struct dvb_frontend *fe,
		struct v4l2_frontend *v4l2_fe,
		struct i2c_adapter *i2c_adap, u8 i2c_addr, u32 tuner_id)
{
	int instance = 0;
	struct atv_demod_priv *priv = NULL;

	mutex_lock(&atv_demod_list_mutex);

	instance = hybrid_tuner_request_state(struct atv_demod_priv, priv,
			hybrid_tuner_instance_list,
			i2c_adap, i2c_addr, DEVICE_NAME);

	priv->atvdemod_param.tuner_id = tuner_id;

	switch (instance) {
	case 0:
		mutex_unlock(&atv_demod_list_mutex);
		return NULL;
	case 1:
		fe->analog_demod_priv = priv;

		priv->afc.fe = fe;
		atv_demod_afc_init(&priv->afc);

		priv->monitor.fe = fe;
		atv_demod_monitor_init(&priv->monitor);

		if (amlatvdemod_devp->irq > 0) {
			priv->isr.irq = amlatvdemod_devp->irq;
			atv_demod_isr_init(&priv->isr);
		}

		priv->standby = true;

		pr_info("%s: aml_atvdemod found.\n", __func__);
		break;
	default:
		fe->analog_demod_priv = priv;
		break;
	}

	mutex_unlock(&atv_demod_list_mutex);

	fe->ops.info.type = FE_ANALOG;

	memcpy(&fe->ops.analog_ops, &atvdemod_ops,
			sizeof(struct analog_demod_ops));

	memcpy(&v4l2_fe->ops, &atvdemod_fe_ops,
			sizeof(struct v4l2_frontend_ops));

	return fe;
}
EXPORT_SYMBOL_GPL(aml_atvdemod_attach);

int atv_demod_get_adc_status(void)
{
	return adc_get_status(ADC_ATV_DEMOD);
}
