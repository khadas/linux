/*
 * sound/soc/aml/m8/aml_m8.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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
#define pr_fmt(fmt) "aml_snd_card: " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
/* #include <sound/soc-dapm.h> */
#include <sound/jack.h>
#include <linux/switch.h>
/* #include <linux/amlogic/saradc.h> */
#include <linux/amlogic/iomap.h>

/* #include "aml_i2s_dai.h" */
#include "aml_i2s.h"
#include "aml_m8.h"
#include "aml_audio_hw.h"
#include <linux/amlogic/sound/audin_regs.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/amlogic/jtag.h>
#include <sound/tlv320aic32x4.h>

#define DRV_NAME "aml_snd_m8_card"

static int i2sbuf[32 + 16];
static void aml_i2s_play(void)
{
	audio_util_set_dac_i2s_format(AUDIO_ALGOUT_DAC_FORMAT_DSP);
#ifdef CONFIG_SND_AML_SPLIT_MODE
	audio_set_i2s_mode(AIU_I2S_MODE_PCM16, 2);
#else
	audio_set_i2s_mode(AIU_I2S_MODE_PCM16);
#endif
	memset(i2sbuf, 0, sizeof(i2sbuf));
	audio_set_aiubuf((virt_to_phys(i2sbuf) + 63) & (~63), 128, 2);
	audio_out_i2s_enable(1);

}

static void aml_audio_start_timer(struct aml_audio_private_data *p_aml_audio,
				  unsigned long delay)
{
	p_aml_audio->timer.expires = jiffies + delay;
	p_aml_audio->timer.data = (unsigned long)p_aml_audio;
	p_aml_audio->detect_flag = -1;
	add_timer(&p_aml_audio->timer);
	p_aml_audio->timer_en = 1;
}

static void aml_audio_stop_timer(struct aml_audio_private_data *p_aml_audio)
{
	del_timer_sync(&p_aml_audio->timer);
	cancel_work_sync(&p_aml_audio->work);
	p_aml_audio->timer_en = 0;
	p_aml_audio->detect_flag = -1;
}

static int hp_det_adc_value(struct aml_audio_private_data *p_aml_audio)
{
	int ret, hp_value;
	int hp_val_sum = 0;
	int loop_num = 0;
	unsigned int mic_ret = 0;

	while (loop_num < 8) {
		/* hp_value = get_adc_sample(p_aml_audio->hp_adc_ch); */
		if (hp_value < 0) {
			pr_info("hp detect get error adc value!\n");
			return -1;	/* continue; */
		}
		hp_val_sum += hp_value;
		loop_num++;
		msleep_interruptible(15);
	}
	hp_val_sum = hp_val_sum >> 3;
	/* pr_info("00000000000hp_val_sum = %hx\n",hp_val_sum); */
	if (hp_val_sum >= p_aml_audio->hp_val_h) {
		ret = 0;
	} else if ((hp_val_sum < (p_aml_audio->hp_val_l)) && hp_val_sum >= 0) {
		ret = 1;
		if (p_aml_audio->mic_det) {
			if (hp_val_sum <= p_aml_audio->mic_val) {
				mic_ret = 8;
				ret |= mic_ret;
			}
		}
	} else {
		ret = 2;
		if (p_aml_audio->mic_det) {
			ret = 0;
			mic_ret = 8;
			ret |= mic_ret;
		}

	}

	return ret;
}

static int aml_audio_hp_detect(struct aml_audio_private_data *p_aml_audio)
{
	int loop_num = 0;
	int ret;
	p_aml_audio->hp_det_status = false;
	/* mutex_lock(&p_aml_audio->lock); */

	while (loop_num < 3) {
		ret = hp_det_adc_value(p_aml_audio);
		if (p_aml_audio->hp_last_state != ret) {
			msleep_interruptible(50);
			if (ret < 0)
				ret = p_aml_audio->hp_last_state;
			else
				p_aml_audio->hp_last_state = ret;
		} else
			msleep_interruptible(50);

		loop_num = loop_num + 1;
	}

	/* mutex_unlock(&p_aml_audio->lock); */

	return ret;
}

static void aml_asoc_work_func(struct work_struct *work)
{
	struct aml_audio_private_data *p_aml_audio = NULL;
	struct snd_soc_card *card = NULL;
	int flag = -1;
	int status = SND_JACK_HEADPHONE;
	p_aml_audio = container_of(work, struct aml_audio_private_data, work);
	card = (struct snd_soc_card *)p_aml_audio->data;

	flag = aml_audio_hp_detect(p_aml_audio);

	if (p_aml_audio->detect_flag != flag) {

		p_aml_audio->detect_flag = flag;

		if (flag & 0x1) {
			/* 1 :have mic ;  2 no mic */
			switch_set_state(&p_aml_audio->sdev, 2);
			pr_info("aml aduio hp pluged 3 jack_type: %d\n",
			       SND_JACK_HEADPHONE);
			snd_soc_jack_report(&p_aml_audio->jack, status,
					    SND_JACK_HEADPHONE);

			/* mic port detect */
			if (p_aml_audio->mic_det) {
				if (flag & 0x8) {
					switch_set_state(&p_aml_audio->mic_sdev,
							 1);
					pr_info("aml aduio mic pluged jack_type: %d\n",
					       SND_JACK_MICROPHONE);
					snd_soc_jack_report(&p_aml_audio->jack,
						status, SND_JACK_HEADPHONE);
				}
			}

		} else if (flag & 0x2) {
			/* 1 :have mic ;  2 no mic */
			switch_set_state(&p_aml_audio->sdev, 1);
			pr_info("aml aduio hp pluged 4 jack_type: %d\n",
			       SND_JACK_HEADSET);
			snd_soc_jack_report(&p_aml_audio->jack, status,
					    SND_JACK_HEADPHONE);
		} else {
			pr_info("aml audio hp unpluged\n");
			switch_set_state(&p_aml_audio->sdev, 0);
			snd_soc_jack_report(&p_aml_audio->jack, 0,
					    SND_JACK_HEADPHONE);

			/* mic port detect */
			if (p_aml_audio->mic_det) {
				if (flag & 0x8) {
					switch_set_state(&p_aml_audio->mic_sdev,
							 1);
					pr_info("aml aduio mic pluged jack_type: %d\n",
					       SND_JACK_MICROPHONE);
					snd_soc_jack_report(&p_aml_audio->jack,
						status, SND_JACK_HEADPHONE);
				}
			}
		}

	}
	p_aml_audio->hp_det_status = true;
}

static void aml_asoc_timer_func(unsigned long data)
{
	struct aml_audio_private_data *p_aml_audio =
	    (struct aml_audio_private_data *)data;
	unsigned long delay = msecs_to_jiffies(150);

	if (p_aml_audio->hp_det_status &&
			!p_aml_audio->suspended) {
		schedule_work(&p_aml_audio->work);
	}
	mod_timer(&p_aml_audio->timer, jiffies + delay);
}

static struct aml_audio_private_data *p_audio;
static int aml_m8_spk_enabled;

static int aml_m8_set_spk(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	aml_m8_spk_enabled = ucontrol->value.integer.value[0];
	pr_info("aml_m8_set_spk: aml_m8_spk_enabled=%d\n",
	       aml_m8_spk_enabled);

	msleep_interruptible(10);
	if (!IS_ERR(p_audio->mute_desc))
		gpiod_direction_output(p_audio->mute_desc, aml_m8_spk_enabled);

	if (aml_m8_spk_enabled == 1)
		msleep_interruptible(100);

	return 0;
}

static int aml_m8_get_spk(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = aml_m8_spk_enabled;
	return 0;
}

static int aml_suspend_pre(struct snd_soc_card *card)
{
	struct aml_audio_private_data *p_aml_audio;
	struct pinctrl_state *state;
	int val = 0;

	pr_info("enter %s\n", __func__);
	p_aml_audio = snd_soc_card_get_drvdata(card);
	if (!p_aml_audio->hp_disable) {
		/* stop timer */
		mutex_lock(&p_aml_audio->lock);
		p_aml_audio->suspended = true;
		if (p_aml_audio->timer_en)
			aml_audio_stop_timer(p_aml_audio);

		mutex_unlock(&p_aml_audio->lock);
	}

	if (IS_ERR_OR_NULL(p_aml_audio->pin_ctl)) {
		pr_info("no audio pin_ctrl to suspend\n");
		return 0;
	}

	state = pinctrl_lookup_state(p_aml_audio->pin_ctl, "aml_snd_suspend");
	if (!IS_ERR(state)) {
		pr_info("enter %s set pin_ctl suspend state\n", __func__);
		pinctrl_select_state(p_aml_audio->pin_ctl, state);
	}

	if (!IS_ERR(p_aml_audio->mute_desc)) {
		val = p_aml_audio->mute_inv ?
			GPIOF_OUT_INIT_LOW : GPIOF_OUT_INIT_HIGH;
		gpiod_direction_output(p_aml_audio->mute_desc, val);
	};

	return 0;
}

static int aml_suspend_post(struct snd_soc_card *card)
{
	pr_info("enter %s\n", __func__);
	return 0;
}

static int aml_resume_pre(struct snd_soc_card *card)
{
	pr_info("enter %s\n", __func__);

	return 0;
}

static int aml_resume_post(struct snd_soc_card *card)
{
	struct aml_audio_private_data *p_aml_audio;
	struct pinctrl_state *state;
	int val = 0;

	pr_info("enter %s\n", __func__);
	p_aml_audio = snd_soc_card_get_drvdata(card);
	if (!p_aml_audio->hp_disable) {
		mutex_lock(&p_aml_audio->lock);
		p_aml_audio->suspended = false;
		if (!p_aml_audio->timer_en) {
			aml_audio_start_timer(p_aml_audio,
					      msecs_to_jiffies(100));
		}
		mutex_unlock(&p_aml_audio->lock);
	}

	if (IS_ERR_OR_NULL(p_aml_audio->pin_ctl)) {
		pr_info("no audio pin_ctrl to resume\n");
		return 0;
	}

	state = pinctrl_lookup_state(p_aml_audio->pin_ctl, "aml_snd_m8");
	if (!IS_ERR(state)) {
		pr_info("enter %s set pin_ctl working state\n", __func__);
		pinctrl_select_state(p_aml_audio->pin_ctl, state);
	}

	if (!IS_ERR(p_aml_audio->mute_desc)) {
		if (p_aml_audio->sleep_time)
			msleep(p_aml_audio->sleep_time);
		val = p_aml_audio->mute_inv ?
			GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
		gpiod_direction_output(p_aml_audio->mute_desc, val);
	}
	return 0;
}

static int speaker_events(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	int val = 0;

	if (IS_ERR(p_audio->mute_desc)) {
		pr_info("no mute_gpio setting");
		return 0;
	}
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		pr_info("audio speaker on\n");
		val = p_audio->mute_inv ?
			GPIOF_OUT_INIT_LOW : GPIOF_OUT_INIT_HIGH;
		gpiod_direction_output(p_audio->mute_desc, 1);
		aml_m8_spk_enabled = 1;
		msleep(p_audio->sleep_time);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		pr_info("audio speaker off\n");
		val = p_audio->mute_inv ?
			GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
		gpiod_direction_output(p_audio->mute_desc, val);
		aml_m8_spk_enabled = 0;
		break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget aml_asoc_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Ext Spk", speaker_events),
	SND_SOC_DAPM_HP("HP", NULL),
	SND_SOC_DAPM_MIC("MAIN MIC", NULL),
	SND_SOC_DAPM_MIC("HEADSET MIC", NULL),
};

static const struct snd_kcontrol_new aml_asoc_controls[] = {
	SOC_DAPM_PIN_SWITCH("Ext Spk"),
};

static struct snd_soc_jack_pin jack_pins[] = {
	{
	 .pin = "HP",
	 .mask = SND_JACK_HEADPHONE,
	 }
};

static const struct snd_kcontrol_new aml_m8_controls[] = {
	SOC_SINGLE_BOOL_EXT("Ext Spk Switch", 0,
			    aml_m8_get_spk,
			    aml_m8_set_spk),
};

static int aml_asoc_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	pr_info("enter %s stream: %s rate: %d format: %d\n", __func__,
			(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
				"playback" : "capture",
			params_rate(params), params_format(params));
	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0) {
		pr_err("%s: set codec dai fmt failed!\n", __func__);
		return ret;
	}
	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_IB_NF | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0) {
		pr_err("%s: set cpu dai fmt failed!\n", __func__);
		return ret;
	}
	/* set codec DAI clock */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0,
			params_rate(params) * DEFAULT_MCLK_RATIO_SR,
			SND_SOC_CLOCK_IN);
	if (ret < 0) {
		pr_err("%s: set codec dai sysclk failed (rate: %d)!\n",
				__func__, params_rate(params));
		return ret;
	}
	/* set cpu DAI clock */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0,
			params_rate(params) * DEFAULT_MCLK_RATIO_SR,
			SND_SOC_CLOCK_OUT);
	if (ret < 0) {
		pr_err("%s: set cpu dai sysclk failed (rate: %d)!\n",
				__func__, params_rate(params));
		return ret;
	}

	return 0;
}
static struct snd_soc_ops aml_asoc_ops = {
	.hw_params = aml_asoc_hw_params,
};

static int aml_asoc_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct aml_audio_private_data *p_aml_audio;
	int ret = 0;
	int hp_paraments[5];

	p_aml_audio = snd_soc_card_get_drvdata(card);
	ret = snd_soc_add_card_controls(codec->card, aml_m8_controls,
					ARRAY_SIZE(aml_m8_controls));
	if (ret)
		return ret;
	/* Add specific widgets */
	snd_soc_dapm_new_controls(dapm, aml_asoc_dapm_widgets,
				  ARRAY_SIZE(aml_asoc_dapm_widgets));

	p_aml_audio->hp_disable =
	    of_property_read_bool(card->dev->of_node, "hp_disable");

	pr_info("headphone detection disable=%d\n", p_aml_audio->hp_disable);

	if (!p_aml_audio->hp_disable) {
		/* for report headphone to android */
		p_aml_audio->sdev.name = "h2w";
		ret = switch_dev_register(&p_aml_audio->sdev);
		if (ret < 0) {
			pr_err("ASoC: register hp switch dev failed\n");
			return ret;
		}

		/* for micphone detect */
		p_aml_audio->mic_sdev.name = "mic_dev";
		ret = switch_dev_register(&p_aml_audio->mic_sdev);
		if (ret < 0) {
			pr_err("ASoC: register mic switch dev failed\n");
			return ret;
		}
		ret =
		    snd_soc_jack_new(codec, "hp switch", SND_JACK_HEADPHONE,
				     &p_aml_audio->jack);
		if (ret < 0) {
			pr_info("Failed to alloc resource for hp switch\n");
		} else {
			ret = snd_soc_jack_add_pins(&p_aml_audio->jack,
						  ARRAY_SIZE(jack_pins),
						  jack_pins);
			if (ret < 0)
				pr_info("Failed to setup hp pins\n");
		}

		p_aml_audio->hp_det_status = true;
		p_aml_audio->mic_det =
		    of_property_read_bool(card->dev->of_node, "mic_det");

		pr_info("entern %s : mic_det=%d\n", __func__,
		       p_aml_audio->mic_det);
		ret =
		    of_property_read_u32_array(card->dev->of_node,
					       "hp_paraments", &hp_paraments[0],
					       5);
		if (ret)
			pr_info("falied to get hp detect paraments\n");
		else {
			/* hp adc value higher base, hp unplugged */
			p_aml_audio->hp_val_h = hp_paraments[0];
			/* hp adc value low base, 3 section hp plugged. */
			p_aml_audio->hp_val_l = hp_paraments[1];
			/* hp adc value mic detect value. */
			p_aml_audio->mic_val = hp_paraments[2];
			/* hp adc value test toerance */
			p_aml_audio->hp_detal = hp_paraments[3];
			/* get adc value from which adc port for hp detect */
			p_aml_audio->hp_adc_ch = hp_paraments[4];

			pr_info("hp detect paraments: h=%d,	l=%d,mic=%d,det=%d,ch=%d\n",
				p_aml_audio->hp_val_h, p_aml_audio->hp_val_l,
				p_aml_audio->mic_val, p_aml_audio->hp_detal,
				p_aml_audio->hp_adc_ch);
		}

		init_timer(&p_aml_audio->timer);
		p_aml_audio->timer.function = aml_asoc_timer_func;
		p_aml_audio->timer.data = (unsigned long)p_aml_audio;
		p_aml_audio->data = (void *)card;
		p_aml_audio->suspended = false;

		INIT_WORK(&p_aml_audio->work, aml_asoc_work_func);
		mutex_init(&p_aml_audio->lock);

		mutex_lock(&p_aml_audio->lock);
		if (!p_aml_audio->timer_en) {
			aml_audio_start_timer(p_aml_audio,
					      msecs_to_jiffies(100));
		}
		mutex_unlock(&p_aml_audio->lock);
	}

	ret =
	    of_property_read_u32(card->dev->of_node, "sleep_time",
				 &p_aml_audio->sleep_time);
	if (ret)
		pr_info("no spk event delay time set\n");

	return 0;
}

static void aml_m8_pinmux_init(struct snd_soc_card *card)
{
	struct aml_audio_private_data *p_aml_audio;
	int val;

	p_aml_audio = snd_soc_card_get_drvdata(card);
	p_aml_audio->mute_desc = gpiod_get(card->dev, "mute_gpio");
	p_aml_audio->mute_inv =
	    of_property_read_bool(card->dev->of_node, "mute_inv");
	if (!IS_ERR(p_aml_audio->mute_desc)) {
		if (p_aml_audio->sleep_time)
			msleep(p_aml_audio->sleep_time);
		val = p_aml_audio->mute_inv ?
			GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
		gpiod_direction_output(p_aml_audio->mute_desc, val);
	}

	if (is_jtag_apao())
		return;
	val = aml_read_sec_reg(0xda004004);
	pr_info("audio use jtag pinmux as i2s output, read val =%x\n",
		aml_read_sec_reg(0xda004004));
	val = val & (~((1<<8) | (1<<1)));
	aml_write_sec_reg(0xda004004, val);

	p_aml_audio->pin_ctl = devm_pinctrl_get_select(card->dev, "aml_snd_m8");
	if (IS_ERR(p_aml_audio->pin_ctl)) {
		pr_info("%s,aml_m8_pinmux_init error!\n", __func__);
		return;
	}
}

static int aml_card_dai_parse_of(struct device *dev,
				 struct snd_soc_dai_link *dai_link,
				 int (*init)(struct snd_soc_pcm_runtime *rtd),
				 struct device_node *cpu_node,
				 struct device_node *codec_node,
				 struct device_node *plat_node)
{
	int ret;

	/* get cpu dai->name */
	ret = snd_soc_of_get_dai_name(cpu_node, &dai_link->cpu_dai_name);
	if (ret < 0)
		goto parse_error;

	/* get codec dai->name */
	ret = snd_soc_of_get_dai_name(codec_node, &dai_link->codec_dai_name);
	if (ret < 0)
		goto parse_error;

	dai_link->name = dai_link->stream_name = dai_link->cpu_dai_name;
	dai_link->codec_of_node = of_parse_phandle(codec_node, "sound-dai", 0);
	dai_link->platform_of_node = plat_node;
	dai_link->init = init;

	return 0;

 parse_error:
	return ret;
}

static int get_audio_codec_i2c_info(struct device_node *p_node,
				struct aml_audio_codec_info *audio_codec_dev)
{
	const char *str;
	int ret = 0;
	unsigned i2c_addr;

	ret = of_property_read_string(p_node, "codec_name",
				      &audio_codec_dev->name);
	if (ret) {
		pr_info("get audio codec name failed!\n");
		goto err_out;
	}

	ret = of_property_match_string(p_node, "status", "okay");
	if (ret) {
		pr_info("%s:this audio codec is disabled!\n",
			audio_codec_dev->name);
		goto err_out;
	}

	pr_debug("use audio aux codec %s\n", audio_codec_dev->name);

	ret = of_property_read_string(p_node, "i2c_bus", &str);
	if (ret) {
		pr_err("%s: faild to get i2c_bus str,use default i2c bus!\n",
		       audio_codec_dev->name);
		audio_codec_dev->i2c_bus_type = AML_I2C_BUS_D;
	} else {
		if (!strncmp(str, "i2c_bus_ao", 10))
			audio_codec_dev->i2c_bus_type = AML_I2C_BUS_AO;
		else if (!strncmp(str, "i2c_bus_b", 9))
			audio_codec_dev->i2c_bus_type = AML_I2C_BUS_B;
		else if (!strncmp(str, "i2c_bus_c", 9))
			audio_codec_dev->i2c_bus_type = AML_I2C_BUS_C;
		else if (!strncmp(str, "i2c_bus_d", 9))
			audio_codec_dev->i2c_bus_type = AML_I2C_BUS_D;
		else if (!strncmp(str, "i2c_bus_a", 9))
			audio_codec_dev->i2c_bus_type = AML_I2C_BUS_A;
		else
			audio_codec_dev->i2c_bus_type = AML_I2C_BUS_D;
	}

	ret = of_property_read_u32(p_node, "i2c_addr", &i2c_addr);
	if (!ret)
		audio_codec_dev->i2c_addr = i2c_addr;
	pr_info("audio aux codec addr: 0x%x, audio codec i2c bus: %d\n",
	       audio_codec_dev->i2c_addr, audio_codec_dev->i2c_bus_type);
err_out:
	return ret;
}

static int aic32x4_of_get_resetpin_pdata(struct aic32x4_pdata *pdata,
				 struct device_node *p_node)
{
	struct gpio_desc *reset_desc;
	int reset_pin = 0;

	reset_desc = of_get_named_gpiod_flags(p_node, "reset_pin", 0, NULL);
	if (IS_ERR(reset_desc)) {
		pr_err("%s fail to get reset pin from dts!\n", __func__);
	} else {
		reset_pin = desc_to_gpio(reset_desc);
		gpio_request(reset_pin, NULL);
		/* gpio_direction_output(reset_pin, GPIOF_OUT_INIT_HIGH);
		msleep(10); */
		gpio_direction_output(reset_pin, GPIOF_OUT_INIT_LOW);
		usleep_range(10*1000, 10*1000+100);

		pdata->rstn_gpio = reset_pin;
		pr_info("%s pdata->rstn_gpio = %d!\n", __func__,
			pdata->rstn_gpio);
	}
	return 0;
}

static int __maybe_unused aic32x4_reset(struct aic32x4_pdata *pdata, int val)
{
	if (pdata == NULL)
		return 0;
	if (pdata->rstn_gpio)
		return gpio_direction_output(pdata->rstn_gpio, val);

	return 0;
}

static int aml_multi_dev_parse_of(struct snd_soc_card *card,
		struct aic32x4_pdata **ppriv)
{
	struct device_node *audio_codec_node = card->dev->of_node;
	struct device_node *child, *codec_node;
	struct i2c_board_info board_info;
	struct i2c_adapter *adapter;
	struct i2c_client *client[4];
	struct aml_audio_codec_info temp_audio_codec;
	struct aic32x4_pdata *pdata;
	static const char *multi_dev;
	int i = 0;

	pr_info("%s ...\n", __func__);
	if (multi_dev != NULL)
		return 0;

	if (of_property_read_string(audio_codec_node, "multi_dev",
				&multi_dev)) {
		pr_info("no multi dev!\n");
		return -ENODEV;
	}
	pr_info("multi_dev name = %s\n", multi_dev);
	child = of_get_child_by_name(audio_codec_node, multi_dev);
	if (child == NULL) {
		/* pr_info("failed to find multi dev node %s\n", multi_dev);*/
		codec_node = of_parse_phandle(audio_codec_node,
				"codec_list", 0);
		child = of_parse_phandle(codec_node, "sound-dai", 0);
		if (child == NULL) {
			pr_err("%s final can not find of node for %s\n",
					__func__, multi_dev);
			return -1;
		}
	}

	memset(&temp_audio_codec, 0, sizeof(struct aml_audio_codec_info));
	/*pr_info("%s, child name:%s\n", __func__, child->name);*/

	if (get_audio_codec_i2c_info(child, &temp_audio_codec) == 0) {
		memset(&board_info, 0, sizeof(board_info));
		strncpy(board_info.type, temp_audio_codec.name, I2C_NAME_SIZE);
		adapter = i2c_get_adapter(temp_audio_codec.i2c_bus_type);
		if (adapter == NULL) {
			pr_err("%s failed to i2c_get_adapter\n", __func__);
			return -ENOMEM;
		}
		pdata = kzalloc(sizeof(struct aic32x4_pdata),
				GFP_KERNEL);
		if (!pdata) {
			pr_err("error: malloc aic32x4_pdata!\n");
			return -ENOMEM;
		}
		if (ppriv != NULL)
			*ppriv = pdata;

		aic32x4_of_get_resetpin_pdata(pdata, child);

		for (i = 0; i < 4; i++) {
			board_info.addr = temp_audio_codec.i2c_addr + i;
			board_info.platform_data = pdata;
			client[i] = i2c_new_device(adapter, &board_info);
			if (client[i] == NULL) {
				pr_err("%s id:%d failed to i2c_new_device\n",
						__func__, i);
				kfree(pdata);
				return -ENOMEM;
			}
			client[i]->dev.platform_data = pdata;
			client[i]->dev.of_node = child;
		}
		i2c_put_adapter(adapter);
	}

	pr_info("%s done\n", __func__);
	return 0;
}

static int aml_card_dais_parse_of(struct snd_soc_card *card)
{
	struct device_node *np = card->dev->of_node;
	struct device_node *cpu_node, *codec_node, *plat_node;
	struct device *dev = card->dev;
	struct snd_soc_dai_link *dai_links;
	int num_dai_links, cpu_num, codec_num, plat_num;
	int i, ret;
	int (*init)(struct snd_soc_pcm_runtime *rtd);

	ret = of_count_phandle_with_args(np, "cpu_list", NULL);
	if (ret < 0) {
		dev_err(dev, "AML sound card no cpu_list errno: %d\n", ret);
		goto err;
	} else {
		cpu_num = ret;
	}
	ret = of_count_phandle_with_args(np, "codec_list", NULL);
	if (ret < 0) {
		dev_err(dev, "AML sound card no codec_list errno: %d\n", ret);
		goto err;
	} else {
		codec_num = ret;
	}
	ret = of_count_phandle_with_args(np, "plat_list", NULL);
	if (ret < 0) {
		dev_err(dev, "AML sound card no plat_list errno: %d\n", ret);
		goto err;
	} else {
		plat_num = ret;
	}
	if ((cpu_num == codec_num) && (cpu_num == plat_num)) {
		num_dai_links = cpu_num;
	} else {
		dev_err(dev,
			"AML sound card cpu_dai num, codec_dai num, platform num don't match: %d\n",
			ret);
		ret = -EINVAL;
		goto err;
	}

	dai_links =
	    devm_kzalloc(dev, num_dai_links * sizeof(struct snd_soc_dai_link),
			 GFP_KERNEL);
	if (!dai_links) {
		dev_err(dev, "Can't allocate snd_soc_dai_links\n");
		ret = -ENOMEM;
		goto err;
	}
	card->dai_link = dai_links;
	card->num_links = num_dai_links;
	for (i = 0; i < num_dai_links; i++) {
		init = NULL;
		/* CPU sub-node */
		cpu_node = of_parse_phandle(np, "cpu_list", i);
		if (cpu_node < 0) {
			dev_err(dev, "parse aml sound card cpu list error\n");
			return -EINVAL;
		}
		/* CODEC sub-node */
		codec_node = of_parse_phandle(np, "codec_list", i);
		if (codec_node < 0) {
			dev_err(dev, "parse aml sound card codec list error\n");
			return ret;
		}
		/* Platform sub-node */
		plat_node = of_parse_phandle(np, "plat_list", i);
		if (plat_node < 0) {
			dev_err(dev,
				"parse aml sound card platform list error\n");
			return ret;
		}
		if (i == 0)
			init = aml_asoc_init;

		ret =
		    aml_card_dai_parse_of(dev, &dai_links[i], init, cpu_node,
					  codec_node, plat_node);
	}
	if (NULL != strstr(dai_links[0].codec_dai_name, "tlv320aic32x4"))
		dai_links[0].ops = &aml_asoc_ops;

 err:
	return ret;
}

static void aml_pinmux_work_func(struct work_struct *pinmux_work)
{
	struct aml_audio_private_data *p_aml_audio = NULL;
	struct snd_soc_card *card = NULL;
	p_aml_audio = container_of(pinmux_work,
				  struct aml_audio_private_data, pinmux_work);
	card = (struct snd_soc_card *)p_aml_audio->data;

	aml_m8_pinmux_init(card);

	return;
}

static int aml_m8_audio_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct snd_soc_card *card;
	struct aml_audio_private_data *p_aml_audio;
	struct aic32x4_pdata *aic32x4_p = NULL;
	int ret;

	p_aml_audio =
	    devm_kzalloc(dev, sizeof(struct aml_audio_private_data),
			 GFP_KERNEL);
	if (!p_aml_audio) {
		dev_err(&pdev->dev, "Can't allocate aml_audio_private_data\n");
		ret = -ENOMEM;
		goto err;
	}
	p_audio = p_aml_audio;

	card = devm_kzalloc(dev, sizeof(struct snd_soc_card), GFP_KERNEL);
	if (!card) {
		dev_err(dev, "Can't allocate snd_soc_card\n");
		ret = -ENOMEM;
		goto err;
	}

	snd_soc_card_set_drvdata(card, p_aml_audio);
	card->dev = dev;
	platform_set_drvdata(pdev, card);
	ret = snd_soc_of_parse_card_name(card, "aml_sound_card,name");
	if (ret < 0) {
		dev_err(dev, "no specific snd_soc_card name\n");
		goto err;
	}

	/* DAPM routes */
	if (of_property_read_bool(np, "aml,audio-routing")) {
		ret = snd_soc_of_parse_audio_routing(card, "aml,audio-routing");
		if (ret < 0) {
			dev_err(dev, "parse aml sound card routing error %d\n",
				ret);
			return ret;
		}
	}
	aml_multi_dev_parse_of(card, &aic32x4_p);

	ret = aml_card_dais_parse_of(card);
	if (ret < 0) {
		dev_err(dev, "parse aml sound card routing error %d\n",
			ret);
		goto err;
	}

	aic32x4_reset(aic32x4_p, 1); /* enable all tlv320aic32x4 */

	card->suspend_pre = aml_suspend_pre,
	card->suspend_post = aml_suspend_post,
	card->resume_pre = aml_resume_pre,
	card->resume_post = aml_resume_post,
	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret < 0) {
		dev_err(dev, "register aml sound card error %d\n", ret);
		goto err;
	}

	aml_i2s_play();

	p_aml_audio->data = (void *)card;
	INIT_WORK(&p_aml_audio->pinmux_work, aml_pinmux_work_func);
	schedule_work(&p_aml_audio->pinmux_work);

	/*aml_m8_pinmux_init(card);*/
	return 0;
 err:
	dev_err(dev, "Can't probe snd_soc_card\n");
	return ret;
}

static void aml_audio_shutdown(struct platform_device *pdev)
{
	struct pinctrl_state *state;

	if (IS_ERR_OR_NULL(p_audio->pin_ctl)) {
		pr_info("no audio pin_ctrl to shutdown\n");
		return;
	}

	state = pinctrl_lookup_state(p_audio->pin_ctl, "aml_snd_suspend");
	if (!IS_ERR(state))
		pinctrl_select_state(p_audio->pin_ctl, state);

	return;
}

static const struct of_device_id amlogic_audio_of_match[] = {
	{.compatible = "aml, aml_snd_m8",},
	{},
};

MODULE_DEVICE_TABLE(of, amlogic_audio_dt_match);

static struct platform_driver aml_m8_audio_driver = {
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = amlogic_audio_of_match,
		   .pm = &snd_soc_pm_ops,
		   },
	.probe = aml_m8_audio_probe,
	.shutdown = aml_audio_shutdown,
};

module_platform_driver(aml_m8_audio_driver);

MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("AML_M8 audio machine Asoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
