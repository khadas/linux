// SPDX-License-Identifier: GPL-2.0
/*
 * ad82128.c  --  ad82128 ALSA SoC Audio driver
 *
 * Copyright 1998 Elite Semiconductor Memory Technology
 *
 * Author: ESMT Audio/Power Product BU Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/initval.h>
#include <linux/regmap.h>
#include <linux/of_gpio.h>
#include "ad82128.h"

// Define how often to check (and clear) the fault status register (in ms)
#define AD82128_FAULT_CHECK_INTERVAL 500

enum ad82128_type {
	AD82128,
};

static const char * const ad82128_supply_names[] = {
	"dvdd", /* Digital power supply. Connect to 3.3-V supply. */
	"pvdd", /* Class-D amp and analog power supply (connected). */
};

#define AD82128_NUM_SUPPLIES ARRAY_SIZE(ad82128_supply_names)

struct ad82128_data {
	struct snd_soc_component *component;
	struct regmap *regmap;
	struct i2c_client *ad82128_client;
	enum ad82128_type devtype;
	struct regulator_bulk_data supplies[AD82128_NUM_SUPPLIES];
	struct delayed_work fault_check_work;
	struct work_struct work;
	unsigned int last_fault;
	int reset_pin;
	int init_done;
};

static int ad82128_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	unsigned int rate = params_rate(params);
	bool ssz_ds;
	int ret;

	switch (rate) {
	case 44100:
	case 48000:
		ssz_ds = false;
		break;
	case 88200:
	case 96000:
		ssz_ds = true;
		break;
	default:
		dev_err(component->dev, "unsupported sample rate: %u\n", rate);
		return -EINVAL;
	}

	ret = snd_soc_component_update_bits(component, AD82128_STATE_CTRL2_REG,
		AD82128_SSZ_DS, ssz_ds);
	if (ret < 0) {
		dev_err(component->dev, "error setting sample rate: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ad82128_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *component = dai->component;
	u8 serial_format;
	int ret;

	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) != SND_SOC_DAIFMT_CBS_CFS) {
		dev_vdbg(component->dev, "DAI Format master is not found\n");
		return -EINVAL;
	}

	switch (fmt & (SND_SOC_DAIFMT_FORMAT_MASK | SND_SOC_DAIFMT_INV_MASK)) {
	case (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF):
		/* 1st data bit occur one BCLK cycle after the frame sync */
		serial_format = AD82128_SAIF_I2S;
		break;
	case (SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_NB_NF):
		/*
		 * Note that although the AD82128 does not have a dedicated DSP
		 * mode it doesn't care about the LRCLK duty cycle during TDM
		 * operation. Therefore we can use the device's I2S mode with
		 * its delaying of the 1st data bit to receive DSP_A formatted
		 * data. See device datasheet for additional details.
		 */
		serial_format = AD82128_SAIF_I2S;
		break;
	case (SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_NB_NF):
		/*
		 * Similar to DSP_A, we can use the fact that the AD82128 does
		 * not care about the LRCLK duty cycle during TDM to receive
		 * DSP_B formatted data in LEFTJ mode (no delaying of the 1st
		 * data bit).
		 */
		serial_format = AD82128_SAIF_LEFTJ;
		break;
	case (SND_SOC_DAIFMT_LEFT_J | SND_SOC_DAIFMT_NB_NF):
		/* No delay after the frame sync */
		serial_format = AD82128_SAIF_LEFTJ;
		break;
	default:
		dev_vdbg(component->dev, "DAI Format is not found\n");
		return -EINVAL;
	}

	ret = snd_soc_component_update_bits(component, AD82128_STATE_CTRL1_REG,
		AD82128_SAIF_FORMAT_MASK,
		serial_format);
	if (ret < 0) {
		dev_err(component->dev, "error setting SAIF format: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ad82128_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_component *component = dai->component;
	int ret;

	ret = snd_soc_component_update_bits(component, AD82128_STATE_CTRL3_REG,
		AD82128_MUTE, mute ? AD82128_MUTE : 0);
	if (ret < 0) {
		dev_err(component->dev, "error (un-)muting device: %d\n", ret);
		return ret;
	}

	return 0;
}

static void ad82128_fault_check_work(struct work_struct *work)
{
	struct ad82128_data *ad82128 = container_of(work, struct ad82128_data,
		fault_check_work.work);
	struct device *dev = ad82128->component->dev;
	unsigned int curr_fault;
	int ret;

	ret = regmap_read(ad82128->regmap, AD82128_FAULT_REG, &curr_fault);
	if (ret < 0) {
		dev_err(dev, "failed to read FAULT register: %d\n", ret);
		goto out;
	}

	/* Check/handle all errors except SAIF clock errors */
	curr_fault &= AD82128_OCE | AD82128_DCE | AD82128_OTE;

	/*
	 * Only flag errors once for a given occurrence. This is needed as
	 * the AD82128 will take time clearing the fault condition internally
	 * during which we don't want to bombard the system with the same
	 * error message over and over.
	 */
	if (!(curr_fault & AD82128_OCE) && (ad82128->last_fault & AD82128_OCE))
		dev_crit(dev, "experienced an over current hardware fault\n");

	if (!(curr_fault & AD82128_DCE) && (ad82128->last_fault & AD82128_DCE))
		dev_crit(dev, "experienced a DC detection fault\n");

	if (!(curr_fault & AD82128_OTE) && (ad82128->last_fault & AD82128_OTE))
		dev_crit(dev, "experienced an over temperature fault\n");

	/* Store current fault value so we can detect any changes next time */
	ad82128->last_fault = curr_fault;

	if (curr_fault)
		goto out;

	/*
	 * Periodically toggle SDZ (shutdown bit) H->L->H to clear any latching
	 * faults as long as a fault condition persists. Always going through
	 * the full sequence no matter the first return value to minimizes
	 * chances for the device to end up in shutdown mode.
	 */
	if (ad82128->reset_pin > 0) {
		ret = gpio_request(ad82128->reset_pin, NULL); // request amp PD pin control GPIO
		if (ret < 0)
			dev_err(dev, "failed to request gpio: %d\n", ret);

		gpio_direction_output(ad82128->reset_pin, 0); // pull low amp PD pin
		msleep(20);
		gpio_direction_output(ad82128->reset_pin, 1); // pull high amp PD pin
	}
out:
	/* Schedule the next fault check at the specified interval */
	schedule_delayed_work(&ad82128->fault_check_work,
		msecs_to_jiffies(AD82128_FAULT_CHECK_INTERVAL));
}

static void ad82128_init_func(struct work_struct *p_work)
{
	struct ad82128_data *ad82128;
	struct snd_soc_component *component;

	int ret;
	int i;
	int reg_data;

	ad82128 = container_of(p_work, struct ad82128_data, work);

	component = ad82128->component;

	dev_dbg(component->dev, "ad82128 i2c address = %p,  %s!\n",
		component, __func__);
	ret = regulator_bulk_enable(ARRAY_SIZE(ad82128->supplies),
		ad82128->supplies);
	if (ret != 0) {
		dev_err(component->dev, "failed to enable supplies: %d\n", ret);
		return;
	}

	/* Set device to mute */
	ret = snd_soc_component_update_bits(component, AD82128_STATE_CTRL3_REG,
		AD82128_MUTE, AD82128_MUTE);
	if (ret < 0)
		goto error_snd_soc_component_update_bits;

	// Write register table
	for (i = 0; i < AD82128_REGISTER_COUNT; i++) {
		reg_data = m_reg_tab[i][1];

		if (m_reg_tab[i][0] == 0x02)
			continue;

		if (m_reg_tab[i][0] >= 0x71 && m_reg_tab[i][0] <= 0x7C)
			continue;

		// set stereo
		if (m_reg_tab[i][0] == 0x1A)
			reg_data &= (~0x40);

		if (m_reg_tab[i][0] == 0x5B)
			reg_data = 0x00;

		if (m_reg_tab[i][0] == 0x5C)
			reg_data = 0x00;

		// set stereo end
		ret = regmap_write(ad82128->regmap, m_reg_tab[i][0], reg_data);
		if (ret < 0)
			goto error_snd_soc_component_update_bits;
	}

	// Write ram1
	for (i = 0; i < AD82128_RAM_TABLE_COUNT; i++) {
		regmap_write(ad82128->regmap, CFADDR, m_ram1_tab[i][0]);
		regmap_write(ad82128->regmap, A1CF1, m_ram1_tab[i][1]);
		regmap_write(ad82128->regmap, A1CF2, m_ram1_tab[i][2]);
		regmap_write(ad82128->regmap, A1CF3, m_ram1_tab[i][3]);
		regmap_write(ad82128->regmap, A1CF4, m_ram1_tab[i][4]);
		regmap_write(ad82128->regmap, CFUD, 0x01);
	}
	// Write ram2
	for (i = 0; i < AD82128_RAM_TABLE_COUNT; i++) {
		regmap_write(ad82128->regmap, CFADDR, m_ram2_tab[i][0]);
		regmap_write(ad82128->regmap, A1CF1, m_ram2_tab[i][1]);
		regmap_write(ad82128->regmap, A1CF2, m_ram2_tab[i][2]);
		regmap_write(ad82128->regmap, A1CF3, m_ram2_tab[i][3]);
		regmap_write(ad82128->regmap, A1CF4, m_ram2_tab[i][4]);
		regmap_write(ad82128->regmap, CFUD, 0x41);
	}

	mdelay(2);

	/* Set device to unmute */
	ret = snd_soc_component_update_bits(component, AD82128_STATE_CTRL3_REG,
		AD82128_MUTE, 0);
	if (ret < 0)
		goto error_snd_soc_component_update_bits;
	INIT_DELAYED_WORK(&ad82128->fault_check_work, ad82128_fault_check_work);
	ad82128->init_done = 1;
	return;
error_snd_soc_component_update_bits:
	dev_err(component->dev, "error configuring device registers: %d\n", ret);
}

static int ad82128_codec_probe(struct snd_soc_component *component)
{
	struct ad82128_data *ad82128 = snd_soc_component_get_drvdata(component);

	ad82128->component = component;

	// software reset amp
	snd_soc_component_update_bits(component, AD82128_STATE_CTRL5_REG,
		AD82128_SW_RESET, 0);
	mdelay(5);
	snd_soc_component_update_bits(component, AD82128_STATE_CTRL5_REG,
		AD82128_SW_RESET, AD82128_SW_RESET);
	msleep(20);
	INIT_WORK(&ad82128->work, ad82128_init_func);
	schedule_work(&ad82128->work);

	return 0;
}

static void ad82128_codec_remove(struct snd_soc_component *component)
{
	struct ad82128_data *ad82128 = snd_soc_component_get_drvdata(component);
	int ret;

	cancel_delayed_work_sync(&ad82128->fault_check_work);

	ret = regulator_bulk_disable(ARRAY_SIZE(ad82128->supplies),
		ad82128->supplies);
	if (ret < 0)
		dev_err(component->dev, "failed to disable supplies: %d\n", ret);
};

static int ad82128_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct ad82128_data *ad82128 = snd_soc_component_get_drvdata(component);
	int ret;
	// wait until codec ready
	while (!ad82128->init_done) {
		dev_err(component->dev, "wait for ad82128 init done\n");
		msleep(20);
	}
	if (event & SND_SOC_DAPM_POST_PMU) {
		/*
		 * Observe codec shutdown-to-active time. The datasheet only
		 * lists a nominal value however just use-it as-is without
		 * additional padding to minimize the delay introduced in
		 * starting to play audio (actually there is other setup done
		 * by the ASoC framework that will provide additional delays,
		 * so we should always be safe).
		 */
		msleep(25);

		ret = snd_soc_component_update_bits(component, AD82128_STATE_CTRL3_REG,
			AD82128_MUTE, 0);
		if (ret < 0)
			dev_err(component->dev, "failed to write MUTE register: %d\n", ret);

		/* Turn on AD82128 periodic fault checking/handling */
		ad82128->last_fault = 0xFE;
		schedule_delayed_work(&ad82128->fault_check_work,
			msecs_to_jiffies(AD82128_FAULT_CHECK_INTERVAL));
	} else if (event & SND_SOC_DAPM_PRE_PMD) {
		/* Disable AD82128 periodic fault checking/handling */
		cancel_delayed_work_sync(&ad82128->fault_check_work);

		/* Place AD82128 in shutdown mode to minimize current draw */
		ret = snd_soc_component_update_bits(component, AD82128_STATE_CTRL3_REG,
			AD82128_MUTE, AD82128_MUTE);
		if (ret < 0)
			dev_err(component->dev, "failed to write MUTE register: %d\n", ret);

		msleep(20);
	}

	return 0;
}

#ifdef CONFIG_PM
static int ad82128_suspend(struct snd_soc_component *component)
{
	struct ad82128_data *ad82128 = snd_soc_component_get_drvdata(component);
	int ret;

	regcache_cache_only(ad82128->regmap, true);
	regcache_mark_dirty(ad82128->regmap);

	ret = regulator_bulk_disable(ARRAY_SIZE(ad82128->supplies),
		ad82128->supplies);
	if (ret < 0)
		dev_err(component->dev, "failed to disable supplies: %d\n", ret);

	return ret;
}

static int ad82128_resume(struct snd_soc_component *component)
{
	struct ad82128_data *ad82128 = snd_soc_component_get_drvdata(component);
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ad82128->supplies),
		ad82128->supplies);
	if (ret < 0) {
		dev_err(component->dev, "failed to enable supplies: %d\n", ret);
		return ret;
	}

	regcache_cache_only(ad82128->regmap, false);

	ret = regcache_sync(ad82128->regmap);
	if (ret < 0) {
		dev_err(component->dev, "failed to sync regcache: %d\n", ret);
		return ret;
	}

	return 0;
}
#else
#define ad82128_suspend NULL
#define ad82128_resume NULL
#endif

static bool ad82128_is_volatile_reg(struct device *dev, unsigned int reg)
{
#ifdef AD82128_REG_RAM_CHECK
	if (reg < AD82128_MAX_REG)
		return true;
	else
		return false;
#else
	switch (reg) {
	case AD82128_FAULT_REG:
	case AD82128_STATE_CTRL1_REG:
	case AD82128_STATE_CTRL2_REG:
	case AD82128_STATE_CTRL3_REG:
	case AD82128_STATE_CTRL5_REG:
		return true;
	default:
		return false;
	}
#endif
}

static const struct regmap_config ad82128_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = AD82128_MAX_REG,
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = ad82128_is_volatile_reg,
};

/*
 * DAC analog gain. There are four discrete values to select from, ranging
 * from 19.2 dB to 26.3dB.
 */
static const DECLARE_TLV_DB_RANGE(dac_analog_tlv,
	0x0, 0x0, TLV_DB_SCALE_ITEM(1920, 0, 0),
	0x1, 0x1, TLV_DB_SCALE_ITEM(2070, 0, 0),
	0x2, 0x2, TLV_DB_SCALE_ITEM(2350, 0, 0),
	0x3, 0x3, TLV_DB_SCALE_ITEM(2630, 0, 0),
);

/*
 * DAC digital volumes. From -103.5 to 24 dB in 0.5 dB steps. Note that
 * setting the gain below -100 dB (register value <0x7) is effectively a MUTE
 * as per device datasheet.
 */
static DECLARE_TLV_DB_SCALE(dac_tlv, -10350, 50, 0);
static const DECLARE_TLV_DB_SCALE(chvol_tlv, -10300, 50, 1);

static const struct snd_kcontrol_new ad82128_snd_controls[] = {
	SOC_SINGLE_TLV("Speaker Driver Playback Volume",
	AD82128_VOLUME_CTRL_REG, 0, 0xff, 0, dac_tlv),
	SOC_SINGLE_TLV("Ch1 Volume", AD82128_VOLUME_CTRL_REG_CH1,
		0, 0xff, 1, chvol_tlv),
	SOC_SINGLE_TLV("Ch2 Volume", AD82128_VOLUME_CTRL_REG_CH2,
		0, 0xff, 1, chvol_tlv),
	SOC_SINGLE_TLV("Speaker Driver Analog Gain", AD82128_ANALOG_CTRL_REG,
		AD82128_ANALOG_GAIN_SHIFT, 3, 0, dac_analog_tlv),
};

static const struct snd_soc_dapm_widget ad82128_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("DAC IN", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC_E("DAC", NULL, SND_SOC_NOPM, 0, 0, ad82128_dac_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUTPUT("OUT")
};

static const struct snd_soc_dapm_route ad82128_audio_map[] = {
	{ "DAC", NULL, "DAC IN" },
	{ "OUT", NULL, "DAC" },
};

static const struct snd_soc_component_driver soc_component_dev_ad82128 = {
	.probe = ad82128_codec_probe,
	.remove = ad82128_codec_remove,
	.suspend = ad82128_suspend,
	.resume = ad82128_resume,
	.controls = ad82128_snd_controls,
	.num_controls = ARRAY_SIZE(ad82128_snd_controls),
	.dapm_widgets = ad82128_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(ad82128_dapm_widgets),
	.dapm_routes = ad82128_audio_map,
	.num_dapm_routes = ARRAY_SIZE(ad82128_audio_map),
	.idle_bias_on = 1,
	.use_pmdown_time = 1,
	.endianness = 1,
	.non_legacy_dai_naming = 1,
};

/* PCM rates supported by the AD82128 driver */
#define AD82128_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 | \
	SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)

/* Formats supported by AD82128 driver */
#define AD82128_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S20_LE | SNDRV_PCM_FMTBIT_S24_LE)

static const struct snd_soc_dai_ops ad82128_speaker_dai_ops = {
	.hw_params = ad82128_hw_params,
	.set_fmt = ad82128_set_dai_fmt,
	.digital_mute = ad82128_mute,
};

/*
 * AD82128 DAI structure
 *
 * Note that were are advertising .playback.channels_max = 2 despite this being
 * a mono amplifier. The reason for that is that some serial ports such as ESMT's
 * McASP module have a minimum number of channels (2) that they can output.
 * Advertising more channels than we have will allow us to interface with such
 * a serial port without really any negative side effects as the AD82128 will
 * simply ignore any extra channel(s) asides from the one channel that is
 * configured to be played back.
 */
static struct snd_soc_dai_driver ad82128_dai[] = {
	{
		.name = "ad82128",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AD82128_RATES,
			.formats = AD82128_FORMATS,
		},
		.ops = &ad82128_speaker_dai_ops,
	},
};

static int ad82128_parse_dt(struct ad82128_data *ad82128,
	struct device_node *np)
{
	int ret = 0;
	int reset_pin = -1;

	reset_pin = of_get_named_gpio(np, "reset_pin", 0);
	if (reset_pin < 0) {
		ret = -1;
		reset_pin = -1;
	} else {
		pr_info("%s pdata->reset_pin = %d!\n", __func__,
			reset_pin);
	}
	ad82128->reset_pin = reset_pin;

	return ret;
}

static int ad82128_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct ad82128_data *data;
	const struct regmap_config *regmap_config;
	int ret;
	int i;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->init_done = 0;
	data->ad82128_client = client;
	data->devtype = id->driver_data;
	ret = ad82128_parse_dt(data, client->dev.of_node);
	if (data->reset_pin > 0) {
		// request amp PD pin control GPIO
		ret = gpio_request(data->reset_pin, NULL);
		if (ret < 0)
			dev_err(dev, "failed to request gpio: %d\n", ret);
		// pull high amp PD pin
		gpio_direction_output(data->reset_pin, 1);
		msleep(150);
	}

	switch (id->driver_data) {
	case AD82128:
		regmap_config = &ad82128_regmap_config;
		break;
	default:
		dev_err(dev, "unexpected private driver data\n");
		return -EINVAL;
	}
	data->regmap = devm_regmap_init_i2c(client, regmap_config);
	if (IS_ERR(data->regmap)) {
		ret = PTR_ERR(data->regmap);
		dev_err(dev, "failed to allocate register map: %d\n", ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(data->supplies); i++)
		data->supplies[i].supply = ad82128_supply_names[i];

	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(data->supplies),
		data->supplies);
	if (ret != 0) {
		dev_err(dev, "failed to request supplies: %d\n", ret);
		return ret;
	}

	dev_set_drvdata(dev, data);

	ret = devm_snd_soc_register_component(&client->dev,
		&soc_component_dev_ad82128,
		ad82128_dai, ARRAY_SIZE(ad82128_dai));
	if (ret < 0) {
		dev_err(dev, "failed to register component: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct i2c_device_id ad82128_id[] = {
	{ "ad82128", AD82128 },
	{}
};

MODULE_DEVICE_TABLE(i2c, ad82128_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ad82128_of_match[] = {
	{
		.compatible = "ESMT,ad82128",
	},
	{},
};
MODULE_DEVICE_TABLE(of, ad82128_of_match);
#endif

static struct i2c_driver ad82128_i2c_driver = {
	.driver = {
		.name = "ad82128",
		.of_match_table = of_match_ptr(ad82128_of_match),
	},
	.probe = ad82128_probe,
	.id_table = ad82128_id,
};

module_i2c_driver(ad82128_i2c_driver);

MODULE_AUTHOR("ESMT BU2");
MODULE_DESCRIPTION("AD82128 Audio amplifier driver");
MODULE_LICENSE("GPL");
