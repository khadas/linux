// SPDX-License-Identifier: GPL-2.0
/*
 * ALSA SoC Amlogic t9015c internal codec driver
 *
 * Copyright (C) 2019 Amlogic,inc
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/amlogic/aml_gpio_consumer.h>

#include "ad82587d.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static void ad82587d_early_suspend(struct early_suspend *h);
static void ad82587d_late_resume(struct early_suspend *h);
#endif

#define AD82587D_RATES (SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_32000 | \
	SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000 | \
	SNDRV_PCM_RATE_64000 | \
	SNDRV_PCM_RATE_88200 | \
	SNDRV_PCM_RATE_96000 | \
	SNDRV_PCM_RATE_176400 | \
	SNDRV_PCM_RATE_192000)

#define AD82587D_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S24_LE | \
	SNDRV_PCM_FMTBIT_S32_LE)

static const DECLARE_TLV_DB_SCALE(mvol_tlv, -10300, 50, 1);
static const DECLARE_TLV_DB_SCALE(chvol_tlv, -10300, 50, 1);

static int ad82587d_reg_init(struct snd_soc_component *component);

static const struct snd_kcontrol_new ad82587d_snd_controls[] = {
	SOC_SINGLE_TLV("Master Volume", MVOL, 0,
		       0xff, 1, mvol_tlv),
	SOC_SINGLE_TLV("Ch1 Volume", C1VOL, 0,
		       0xff, 1, chvol_tlv),
	SOC_SINGLE_TLV("Ch2 Volume", C2VOL, 0,
		       0xff, 1, chvol_tlv),

	SOC_SINGLE("Ch1 Switch", MUTE, 2, 1, 1),
	SOC_SINGLE("Ch2 Switch", MUTE, 1, 1, 1),
};

/* Power-up register defaults */
static const
struct reg_default ad82587d_reg_defaults[AD82587D_REGISTER_COUNT] = {
	{0x00, 0x00},//##State_Control_1
	{0x01, 0x04},//##State_Control_2
	{0x02, 0x30},//##State_Control_3
	{0x03, 0x4e},//##Master_volume_control
	{0x04, 0x00},//##Channel_1_volume_control
	{0x05, 0x00},//##Channel_2_volume_control
	{0x06, 0x18},//##Channel_3_volume_control
	{0x07, 0x18},//##Channel_4_volume_control
	{0x08, 0x18},//##Channel_5_volume_control
	{0x09, 0x18},//##Channel_6_volume_control
	{0x0a, 0x10},//##Bass_Tone_Boost_and_Cut
	{0x0b, 0x10},//##treble_Tone_Boost_and_Cut
	{0x0c, 0x90},//##State_Control_4
	{0x0d, 0x00},//##Channel_1_configuration_registers
	{0x0e, 0x00},//##Channel_2_configuration_registers
	{0x0f, 0x00},//##Channel_3_configuration_registers
	{0x10, 0x00},//##Channel_4_configuration_registers
	{0x11, 0x00},//##Channel_5_configuration_registers
	{0x12, 0x00},//##Channel_6_configuration_registers
	{0x13, 0x00},//##Channel_7_configuration_registers
	{0x14, 0x00},//##Channel_8_configuration_registers
	{0x15, 0x6a},//##DRC1_limiter_attack/release_rate
	{0x16, 0x6a},//##DRC2_limiter_attack/release_rate
	{0x17, 0x6a},//##DRC3_limiter_attack/release_rate
	{0x18, 0x6a},//##DRC4_limiter_attack/release_rate
	{0x19, 0x06},//##Error_Delay
	{0x1a, 0x32},//##State_Control_5
	{0x1b, 0x01},//##HVUV_selection
	{0x1c, 0x00},//##State_Control_6
	{0x1d, 0x7f},//##Coefficient_RAM_Base_Address
	{0x1e, 0x00},//##Top_8-bits_of_coefficients_A1
	{0x1f, 0x00},//##Middle_8-bits_of_coefficients_A1
	{0x20, 0x00},//##Bottom_8-bits_of_coefficients_A1
	{0x21, 0x00},//##Top_8-bits_of_coefficients_A2
	{0x22, 0x00},//##Middle_8-bits_of_coefficients_A2
	{0x23, 0x00},//##Bottom_8-bits_of_coefficients_A2
	{0x24, 0x00},//##Top_8-bits_of_coefficients_B1
};

static char m_reg_tab[AD82587D_REGISTER_COUNT][2] = {
	{0x00, 0x00},//##State_Control_1
	{0x01, 0x04},//##State_Control_2
	{0x02, 0x30},//##State_Control_3
	{0x03, 0x4e},//##Master_volume_control
	{0x04, 0x00},//##Channel_1_volume_control
	{0x05, 0x00},//##Channel_2_volume_control
	{0x06, 0x18},//##Channel_3_volume_control
	{0x07, 0x18},//##Channel_4_volume_control
	{0x08, 0x18},//##Channel_5_volume_control
	{0x09, 0x18},//##Channel_6_volume_control
	{0x0a, 0x10},//##Bass_Tone_Boost_and_Cut
	{0x0b, 0x10},//##treble_Tone_Boost_and_Cut
	{0x0c, 0x90},//##State_Control_4
	{0x0d, 0x00},//##Channel_1_configuration_registers
	{0x0e, 0x00},//##Channel_2_configuration_registers
	{0x0f, 0x00},//##Channel_3_configuration_registers
	{0x10, 0x00},//##Channel_4_configuration_registers
	{0x11, 0x00},//##Channel_5_configuration_registers
	{0x12, 0x00},//##Channel_6_configuration_registers
	{0x13, 0x00},//##Channel_7_configuration_registers
	{0x14, 0x00},//##Channel_8_configuration_registers
	{0x15, 0x6a},//##DRC1_limiter_attack/release_rate
	{0x16, 0x6a},//##DRC2_limiter_attack/release_rate
	{0x17, 0x6a},//##DRC3_limiter_attack/release_rate
	{0x18, 0x6a},//##DRC4_limiter_attack/release_rate
	{0x19, 0x06},//##Error_Delay
	{0x1a, 0x32},//##State_Control_5
	{0x1b, 0x01},//##HVUV_selection
	{0x1c, 0x00},//##State_Control_6
	{0x1d, 0x7f},//##Coefficient_RAM_Base_Address
	{0x1e, 0x00},//##Top_8-bits_of_coefficients_A1
	{0x1f, 0x00},//##Middle_8-bits_of_coefficients_A1
	{0x20, 0x00},//##Bottom_8-bits_of_coefficients_A1
	{0x21, 0x00},//##Top_8-bits_of_coefficients_A2
	{0x22, 0x00},//##Middle_8-bits_of_coefficients_A2
	{0x23, 0x00},//##Bottom_8-bits_of_coefficients_A2
	{0x24, 0x00},//##Top_8-bits_of_coefficients_B1
};

/* private data */
struct ad82587d_priv {
	struct regmap *regmap;
	struct snd_soc_component *component;
	struct ad82587d_platform_data *pdata;

	unsigned char Ch1_vol;
	unsigned char Ch2_vol;
	unsigned char master_vol;
	unsigned char mute_val;

	char *m_reg_tab;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

static int ad82587d_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return 0;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
		break;
	default:
		return 0;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		return 0;
	}

	return 0;
}

static int ad82587d_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	unsigned int rate;

	rate = params_rate(params);
	pr_debug("rate: %u\n", rate);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_BE:
		pr_debug("24bit\n");
	/* go through */
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S20_3BE:
		pr_debug("20bit\n");

		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_S16_BE:
		pr_debug("16bit\n");

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ad82587d_set_bias_level(struct snd_soc_component *component,
				  enum snd_soc_bias_level level)
{
	pr_debug("level = %d\n", level);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		/* Full power on */
		break;

	case SND_SOC_BIAS_STANDBY:
		break;

	case SND_SOC_BIAS_OFF:
		/* The chip runs through the power down sequence for us. */
		break;
	}
	component->dapm.bias_level = level;

	return 0;
}

static int ad82587d_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *codec_dai)
{
	struct ad82587d_priv *ad82587d = snd_soc_dai_get_drvdata(codec_dai);
	struct snd_soc_component *component = ad82587d->component;
	unsigned char mute_val;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			mute_val = snd_soc_component_read32(component, MUTE);
			mute_val &= ~(1 << 3);
			snd_soc_component_write(component, MUTE, mute_val);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			mute_val = snd_soc_component_read32(component, MUTE);
			mute_val |= (1 << 3);
			snd_soc_component_write(component, MUTE, mute_val);
			break;
		}
	}
	return 0;
}

static const struct snd_soc_dai_ops ad82587d_dai_ops = {
	.hw_params = ad82587d_hw_params,
	.set_fmt = ad82587d_set_dai_fmt,
	.trigger = ad82587d_trigger,
};

static struct snd_soc_dai_driver ad82587d_dai = {
	.name = "ad82587d",
	.playback = {
		.stream_name = "HIFI Playback",
		.channels_min = 2,
		.channels_max = 16,
		.rates = AD82587D_RATES,
		.formats = AD82587D_FORMATS,
	},
	.ops = &ad82587d_dai_ops,
};

static int ad82587d_GPIO_enable(struct snd_soc_component *component, bool enable)
{
	struct ad82587d_priv *ad82587d = snd_soc_component_get_drvdata(component);
	struct ad82587d_platform_data *pdata = ad82587d->pdata;

	if (pdata->reset_pin < 0)
		return 0;

	if (enable) {
		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
		usleep_range(10 * 1000, 11 * 1000);
		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_HIGH);
		dev_info(component->dev, "ad82587d start status = %d\n",
			gpio_get_value(pdata->reset_pin));
		usleep_range(1 * 1000, 2 * 1000);
	} else {
		/*gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);*/
		gpio_set_value(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
		dev_info(component->dev, "ad82587d stop status = %d\n",
			gpio_get_value(pdata->reset_pin));
		/*devm_gpio_free(component->dev, pdata->reset_pin);*/
	}

	return 0;
}

static char init_tab[][2] = {
	{0x02, 0x0f},
	{0x02, 0x1f},
	{0x00, 0x01},
	{0x01, 0x01},
	{0x03, 0x00},
	{0x04, 0x10}, //volume l
	{0x05, 0x10}, // volume r
	{0x07, 0x30},
	{0x08, 0x77},
	{0x13, 0x08},
	{0x14, 0x00},
	{0x15, 0x00},
	{0x06, 0x01},
	{0x16, 0x00},
	{0x09, 0x0d},
	{0x0b, 0x0b},
	{0x02, 0x11},
	{0x02, 0x11},
	{0x10, 0x04},
	{0x11, 0x7f},
	{0x12, 0xac},
	{0x22, 0x04},
	{0x23, 0x02},
	{0x24, 0x6e},
	{0x02, 0x11},
	{0x10, 0x04},
	{0x11, 0x0a},
	{0x12, 0x55},
	{0x22, 0x03},
	{0x23, 0x78},
	{0x24, 0xda},
	{0x08, 0x7a},
	{0x20, 0x00},
	{0x21, 0x2f},
	{0x00, 0x03},
	{0x04, 0x04},
	{0x05, 0x04},
};

static int ad82587d_reg_init(struct snd_soc_component *component)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(init_tab); i++) {
		snd_soc_component_write(component, init_tab[i][0], init_tab[i][1]);
	};

	return 0;
}

static int ad82587d_init(struct snd_soc_component *component)
{
	ad82587d_GPIO_enable(component, true);

	dev_info(component->dev, "%s!\n", __func__);

	ad82587d_reg_init(component);

	return 0;
}

static int ad82587d_probe(struct snd_soc_component *component)
{
	struct ad82587d_priv *ad82587d = snd_soc_component_get_drvdata(component);
	struct ad82587d_platform_data *pdata = ad82587d->pdata;
	int ret = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
	ad82587d->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	ad82587d->early_suspend.suspend = ad82587d_early_suspend;
	ad82587d->early_suspend.resume = ad82587d_late_resume;
	ad82587d->early_suspend.param = component;
	register_early_suspend(&ad82587d->early_suspend);
#endif

	if (pdata->reset_pin > 0) {
		ret = devm_gpio_request_one(component->dev, pdata->reset_pin,
						GPIOF_OUT_INIT_LOW,
						"ad82587d-reset-pin");

		if (ret < 0) {
			dev_err(component->dev, "ad82587d get gpio error!\n");
			return -1;
		}
	}

	ad82587d_init(component);
	ad82587d->component = component;

	return 0;
}

static void ad82587d_remove(struct snd_soc_component *component)
{
	struct ad82587d_priv *ad82587d = snd_soc_component_get_drvdata(component);
	struct ad82587d_platform_data *pdata = ad82587d->pdata;

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ad82587d->early_suspend);
#endif

	devm_gpio_free(component->dev, pdata->reset_pin);
}

#ifdef CONFIG_PM
static int ad82587d_suspend(struct snd_soc_component *component)
{
	struct ad82587d_priv *ad82587d = snd_soc_component_get_drvdata(component);

	dev_info(component->dev, "%s!\n", __func__);

	/* save volume */
	ad82587d->Ch1_vol = snd_soc_component_read32(component, C1VOL);
	ad82587d->Ch2_vol = snd_soc_component_read32(component, C2VOL);
	ad82587d->master_vol = snd_soc_component_read32(component, MVOL);
	ad82587d->mute_val = snd_soc_component_read32(component, MUTE);

	ad82587d_set_bias_level(component, SND_SOC_BIAS_OFF);

	ad82587d_GPIO_enable(component, false);
	return 0;
}

static int ad82587d_resume(struct snd_soc_component *component)
{
	struct ad82587d_priv *ad82587d = snd_soc_component_get_drvdata(component);

	dev_info(component->dev, "%s\n", __func__);

	ad82587d_init(component);

	snd_soc_component_write(component, C1VOL, ad82587d->Ch1_vol);
	snd_soc_component_write(component, C2VOL, ad82587d->Ch2_vol);
	snd_soc_component_write(component, MVOL, ad82587d->master_vol);
	snd_soc_component_write(component, MUTE, ad82587d->mute_val);

	ad82587d_set_bias_level(component, SND_SOC_BIAS_STANDBY);

	return 0;
}
#else
#define ad82587d_suspend NULL
#define ad82587d_resume NULL
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ad82587d_early_suspend(struct early_suspend *h)
{
}

static void ad82587d_late_resume(struct early_suspend *h)
{
}
#endif

static const struct snd_soc_dapm_widget ad82587d_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("DAC", "HIFI Playback", SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_component_driver soc_codec_dev_ad82587d = {
	.probe = ad82587d_probe,
	.remove = ad82587d_remove,
	.suspend = ad82587d_suspend,
	.resume = ad82587d_resume,
	.set_bias_level = ad82587d_set_bias_level,
	.controls = ad82587d_snd_controls,
	.num_controls = ARRAY_SIZE(ad82587d_snd_controls),
	.dapm_widgets = ad82587d_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(ad82587d_dapm_widgets),
};

static const struct regmap_config ad82587d_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = AD82587D_REGISTER_COUNT,
	.reg_defaults = ad82587d_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(ad82587d_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};

static int ad82587d_parse_dt(struct ad82587d_priv *ad82587d,
	struct device_node *np)
{
	int ret = 0;
	int reset_pin = -1;

	reset_pin = of_get_named_gpio(np, "reset_pin", 0);
	if (reset_pin < 0) {
		pr_err("%s fail to get reset pin from dts!\n", __func__);
		ret = -1;
	} else {
		pr_info("%s pdata->reset_pin = %d!\n", __func__,
				ad82587d->pdata->reset_pin);
	}
	ad82587d->pdata->reset_pin = reset_pin;

	return ret;
}

static int ad82587d_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct ad82587d_priv *ad82587d;
	struct ad82587d_platform_data *pdata;
	int ret;

	ad82587d = devm_kzalloc(&i2c->dev, sizeof(struct ad82587d_priv),
			       GFP_KERNEL);
	if (!ad82587d)
		return -ENOMEM;

	ad82587d->regmap = devm_regmap_init_i2c(i2c, &ad82587d_regmap);
	if (IS_ERR(ad82587d->regmap)) {
		ret = PTR_ERR(ad82587d->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	i2c_set_clientdata(i2c, ad82587d);

	pdata = devm_kzalloc(&i2c->dev,
				sizeof(struct ad82587d_platform_data),
				GFP_KERNEL);
	if (!pdata) {
		pr_err("%s failed to kzalloc for ad82587d pdata\n", __func__);
		return -ENOMEM;
	}
	ad82587d->pdata = pdata;

	ad82587d_parse_dt(ad82587d, i2c->dev.of_node);

	ret = snd_soc_register_component(&i2c->dev, &soc_codec_dev_ad82587d,
					 &ad82587d_dai, 1);
	if (ret != 0)
		dev_err(&i2c->dev, "Failed to register component (%d)\n", ret);

	ad82587d->m_reg_tab =
		kzalloc(sizeof(char) * AD82587D_REGISTER_TABLE_SIZE,
			GFP_KERNEL);
	if (!ad82587d->m_reg_tab)
		return -ENOMEM;

	memcpy(ad82587d->m_reg_tab, m_reg_tab, AD82587D_REGISTER_TABLE_SIZE);

	return ret;
}

static int ad82587d_i2c_remove(struct i2c_client *client)
{
	struct ad82587d_priv *ad82587d =
		(struct ad82587d_priv *)i2c_get_clientdata(client);

	if (ad82587d)
		kfree(ad82587d->m_reg_tab);

	snd_soc_unregister_component(&client->dev);

	return 0;
}

static const struct i2c_device_id ad82587d_i2c_id[] = {
	{ " ad82587d", 0 },
	{}
};

static const struct of_device_id ad82587d_of_id[] = {
	{ .compatible = "ESMT, ad82587d", },
	{ /* senitel */ }
};
MODULE_DEVICE_TABLE(of, ad82587d_of_id);

static struct i2c_driver ad82587d_i2c_driver = {
	.driver = {
		.name = "ad82587d",
		.of_match_table = ad82587d_of_id,
		.owner = THIS_MODULE,
	},
	.probe = ad82587d_i2c_probe,
	.remove = ad82587d_i2c_remove,
	.id_table = ad82587d_i2c_id,
};

module_i2c_driver(ad82587d_i2c_driver);

MODULE_DESCRIPTION("ASoC ad82587d driver");
MODULE_AUTHOR("AML MM team");
MODULE_LICENSE("GPL");
