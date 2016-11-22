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
#include <sound/tas57xx.h>
#include <linux/amlogic/aml_gpio_consumer.h>

#include "tas5731.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
static void tas5731_early_suspend(struct early_suspend *h);
static void tas5731_late_resume(struct early_suspend *h);
#endif

#define tas5731_RATES (SNDRV_PCM_RATE_8000 | \
		       SNDRV_PCM_RATE_11025 | \
		       SNDRV_PCM_RATE_16000 | \
		       SNDRV_PCM_RATE_22050 | \
		       SNDRV_PCM_RATE_32000 | \
		       SNDRV_PCM_RATE_44100 | \
		       SNDRV_PCM_RATE_48000)

#define tas5731_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE | \
	 SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S20_3BE | \
	 SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_BE | \
	 SNDRV_PCM_FMTBIT_S32_LE)

/* Power-up register defaults */
static const u8 tas5731_regs[DDX_NUM_BYTE_REG] = {
	0x6c, 0x70, 0x00, 0xA0, 0x05, 0x40, 0x00, 0xFF,
	0x30, 0x30, 0xFF, 0x00, 0x00, 0x00, 0x91, 0x00,
	0x02, 0xAC, 0x54, 0xAC, 0x54, 0x00, 0x00, 0x00,
	0x00, 0x30, 0x0F, 0x82, 0x02,
};

static u8 TAS5731_drc1_table[3][8] = {
	/* 0x3A drc1_ae */
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	/* 0x3B drc1_aa */
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	/* 0x3C drc1_ad */
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

static u8 TAS5731_drc2_table[3][8] = {
	/* 0x3D drc2_ae */
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	/* 0x3E drc2_aa */
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	/* 0x3F drc2_ad */
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

static u8 tas5731_drc1_tko_table[3][4] = {
	/* 0x40 drc1_t */
	{ 0x00, 0x00, 0x00, 0x00 },
	/* 0x41 drc1_k */
	{ 0x00, 0x00, 0x00, 0x00 },
	/* 0x42 drc1_o */
	{ 0x00, 0x00, 0x00, 0x00 }
};

static u8 tas5731_drc2_tko_table[3][4] = {
	/* 0x43 drc2_t */
	{ 0x00, 0x00, 0x00, 0x00 },
	/* 0x44 drc2_k */
	{ 0x00, 0x00, 0x00, 0x00 },
	/* 0x45 drc2_o */
	{ 0x00, 0x00, 0x00, 0x00 }
};


/* codec private data */
struct tas5731_priv {
	struct snd_soc_codec *codec;
	struct tas57xx_platform_data *pdata;

	enum snd_soc_control_type control_type;
	void *control_data;

	/*Platform provided EQ configuration */
	int num_eq_conf_texts;
	const char **eq_conf_texts;
	int eq_cfg;
	struct soc_enum eq_conf_enum;
	unsigned char Ch1_vol;
	unsigned char Ch2_vol;
	unsigned char master_vol;
	unsigned mclk;
	struct early_suspend early_suspend;
};

static const DECLARE_TLV_DB_SCALE(mvol_tlv, -12700, 50, 1);
static const DECLARE_TLV_DB_SCALE(chvol_tlv, -10300, 50, 1);

static const struct snd_kcontrol_new tas5731_snd_controls[] = {
	SOC_SINGLE_TLV("Master Volume", DDX_MASTER_VOLUME, 0,
		       0xff, 1, mvol_tlv),
	SOC_SINGLE_TLV("Ch1 Volume", DDX_CHANNEL1_VOL, 0,
		       0xff, 1, chvol_tlv),
	SOC_SINGLE_TLV("Ch2 Volume", DDX_CHANNEL2_VOL, 0,
		       0xff, 1, chvol_tlv),
	SOC_SINGLE("Ch1 Switch", DDX_SOFT_MUTE, 0, 1, 1),
	SOC_SINGLE("Ch2 Switch", DDX_SOFT_MUTE, 1, 1, 1),
	SOC_SINGLE_RANGE("Fine Master Volume", DDX_CHANNEL3_VOL, 0,
			   0x80, 0x83, 0),
};

static int tas5731_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct tas5731_priv *tas5731 = snd_soc_codec_get_drvdata(codec);

	tas5731->mclk = freq;
	/* 0x74 = 512fs; 0x6c = 256fs */
	if (freq == 512 * 48000)
		snd_soc_write(codec, DDX_CLOCK_CTL, 0x74);
	else
		snd_soc_write(codec, DDX_CLOCK_CTL, 0x6c);
	return 0;
}

static int tas5731_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tas5731_hw_params(struct snd_pcm_substream *substream,
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
	/* fall through */
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

static int tas5731_set_bias_level(struct snd_soc_codec *codec,
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
	codec->dapm.bias_level = level;
	return 0;
}

static const struct snd_soc_dai_ops tas5731_dai_ops = {
	.hw_params = tas5731_hw_params,
	.set_sysclk = tas5731_set_dai_sysclk,
	.set_fmt = tas5731_set_dai_fmt,
};

static struct snd_soc_dai_driver tas5731_dai = {
	.name = "tas5731",
	.playback = {
		.stream_name = "HIFI Playback",
		.channels_min = 2,
		.channels_max = 8,
		.rates = tas5731_RATES,
		.formats = tas5731_FORMATS,
	},
	.ops = &tas5731_dai_ops,
};

static int tas5731_set_master_vol(struct snd_soc_codec *codec)
{
	struct tas57xx_platform_data *pdata = dev_get_platdata(codec->dev);

	/* using user BSP defined master vol config; */
	if (pdata && pdata->custom_master_vol) {
		pr_debug("tas5731_set_master_vol::%d\n",
			pdata->custom_master_vol);
		snd_soc_write(codec, DDX_MASTER_VOLUME,
			      (0xff - pdata->custom_master_vol));
	} else {
		pr_debug
			("get dtd master_vol failed:using default setting\n");
		snd_soc_write(codec, DDX_MASTER_VOLUME, 0x30);
	}

	return 0;
}

/* tas5731 DRC for channel L/R */
static int tas5731_set_drc1(struct snd_soc_codec *codec)
{
	int i = 0, j = 0;
	u8 *p = NULL;
	struct tas57xx_platform_data *pdata = dev_get_platdata(codec->dev);

	if (pdata && pdata->custom_drc1_table
	    && pdata->custom_drc1_table_len == 24) {
		p = pdata->custom_drc1_table;
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 8; j++)
				TAS5731_drc1_table[i][j] = p[i * 8 + j];

			regmap_raw_write(codec->control_data, DDX_DRC1_AE + i,
					 TAS5731_drc1_table[i], 8);
			/*for (j = 0; j < 8; j++)
				pr_info("TAS5731_drc1_table[%d][%d]: %x\n",
					 i, j, TAS5731_drc1_table[i][j]);*/
		}
	} else {
		return -1;
	}

	if (pdata && pdata->custom_drc1_tko_table
	    && pdata->custom_drc1_tko_table_len == 12) {
		p = pdata->custom_drc1_tko_table;
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 4; j++)
				tas5731_drc1_tko_table[i][j] = p[i * 4 + j];

			regmap_raw_write(codec->control_data, DDX_DRC1_T + i,
					 tas5731_drc1_tko_table[i], 4);
			/*for (j = 0; j < 4; j++)
				pr_info("tas5731_drc1_tko_table[%d][%d]: %x\n",
					 i, j, tas5731_drc1_tko_table[i][j]);*/
		}
	} else {
		return -1;
	}

	return 0;
}

static int tas5731_set_drc2(struct snd_soc_codec *codec)
{
	int i = 0, j = 0;
	u8 *p = NULL;
	struct tas57xx_platform_data *pdata = dev_get_platdata(codec->dev);

	if (pdata && pdata->custom_drc2_table
	    && pdata->custom_drc2_table_len == 24) {
		p = pdata->custom_drc2_table;
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 8; j++)
				TAS5731_drc2_table[i][j] = p[i * 8 + j];

			regmap_raw_write(codec->control_data, DDX_DRC2_AE + i,
					 TAS5731_drc2_table[i], 8);
			/*for (j = 0; j < 8; j++)
				pr_info("TAS5731_drc2_table[%d][%d]: %x\n",
					 i, j, TAS5731_drc2_table[i][j]);*/
		}
	} else {
		return -1;
	}

	if (pdata && pdata->custom_drc2_tko_table
	    && pdata->custom_drc2_tko_table_len == 12) {
		p = pdata->custom_drc2_tko_table;
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 4; j++)
				tas5731_drc2_tko_table[i][j] = p[i * 4 + j];

			regmap_raw_write(codec->control_data, DDX_DRC2_T + i,
					 tas5731_drc2_tko_table[i], 4);
			/*for (j = 0; j < 4; j++)
				pr_info("tas5731_drc2_tko_table[%d][%d]: %x\n",
					 i, j, tas5731_drc2_tko_table[i][j]);*/
		}
	} else {
		return -1;
	}

	return 0;
}


static int tas5731_set_drc(struct snd_soc_codec *codec)
{
	struct tas57xx_platform_data *pdata = dev_get_platdata(codec->dev);
	char drc_mask = 0;
	u8 tas5731_drc_ctl_table[] = { 0x00, 0x00, 0x00, 0x00 };

	if (pdata && pdata->enable_ch1_drc
		&& pdata->enable_ch2_drc && pdata->drc_enable) {
		drc_mask |= 0x03;
		tas5731_drc_ctl_table[3] = drc_mask;
		tas5731_set_drc1(codec);
		tas5731_set_drc2(codec);
	    regmap_raw_write(codec->control_data, DDX_DRC_CTL,
			 tas5731_drc_ctl_table, 4);
	    return 0;
	}
	return -1;
}

static int tas5731_set_eq_biquad(struct snd_soc_codec *codec)
{
	int i = 0, j = 0, k = 0;
	u8 *p = NULL;
	u8 addr;
	u8 tas5731_bq_table[20];
	struct tas5731_priv *tas5731 = snd_soc_codec_get_drvdata(codec);
	struct tas57xx_platform_data *pdata = tas5731->pdata;
	struct tas57xx_eq_cfg *cfg;

	if (!pdata)
		return 0;

	cfg = pdata->eq_cfgs;
	if (!(cfg))
		return 0;

	p = cfg[tas5731->eq_cfg].regs;

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 7; j++) {
			addr = (DDX_CH1_BQ_0 + i * 7 + j);
			for (k = 0; k < 20; k++)
				tas5731_bq_table[k] =
					p[i * 7 * 20 + j * 20 + k];
			regmap_raw_write(codec->control_data, addr,
					tas5731_bq_table, 20);
			/*for (k = 0; k < 20; k++)
				pr_info("tas5731_bq_table[%d]: %x\n",
					k, tas5731_bq_table[k]);*/
		}
	}
	return 0;
}

static int tas5731_put_eq_enum(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tas5731_priv *tas5731 = snd_soc_codec_get_drvdata(codec);
	struct tas57xx_platform_data *pdata = tas5731->pdata;
	int value = ucontrol->value.integer.value[0];

	if (value >= pdata->num_eq_cfgs)
		return -EINVAL;

	tas5731->eq_cfg = value;
	tas5731_set_eq_biquad(codec);

	return 0;
}

static int tas5731_get_eq_enum(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tas5731_priv *tas5731 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = tas5731->eq_cfg;

	return 0;
}

static int tas5731_set_eq(struct snd_soc_codec *codec)
{
	int i = 0, ret = 0;
	struct tas5731_priv *tas5731 = snd_soc_codec_get_drvdata(codec);
	struct tas57xx_platform_data *pdata = tas5731->pdata;
	u8 tas5731_eq_ctl_table[] = { 0x00, 0x00, 0x00, 0x80 };
	struct tas57xx_eq_cfg *cfg = pdata->eq_cfgs;

	if (!pdata || !pdata->eq_enable)
		return -ENOENT;

	if (pdata->num_eq_cfgs && (tas5731->eq_conf_texts == NULL)) {
		struct snd_kcontrol_new control =
			SOC_ENUM_EXT("EQ Mode", tas5731->eq_conf_enum,
				     tas5731_get_eq_enum, tas5731_put_eq_enum);

		tas5731->eq_conf_texts =
			kzalloc(sizeof(char *) * pdata->num_eq_cfgs,
				GFP_KERNEL);
		if (!tas5731->eq_conf_texts) {
			dev_err(codec->dev,
				"Fail to allocate %d EQ config tests\n",
				pdata->num_eq_cfgs);
			return -ENOMEM;
		}

		for (i = 0; i < pdata->num_eq_cfgs; i++)
			tas5731->eq_conf_texts[i] = cfg[i].name;

		tas5731->eq_conf_enum.max = pdata->num_eq_cfgs;
		tas5731->eq_conf_enum.texts = tas5731->eq_conf_texts;

		ret = snd_soc_add_codec_controls(codec, &control, 1);
		if (ret != 0)
			dev_err(codec->dev, "Fail to add EQ mode control: %d\n",
				ret);
	}

	tas5731_set_eq_biquad(codec);

	tas5731_eq_ctl_table[3] &= 0x7F;
	regmap_raw_write(codec->control_data, DDX_BANKSWITCH_AND_EQCTL,
			 tas5731_eq_ctl_table, 4);
	return 0;
}

static int tas5731_customer_init(struct snd_soc_codec *codec)
{
	int i = 0;
	u8 data[4] = {0x00, 0x00, 0x00, 0x00};
	struct tas57xx_platform_data *pdata = dev_get_platdata(codec->dev);

	if (pdata && pdata->init_regs) {
		if (pdata->num_init_regs != 4) {
			dev_err(codec->dev, "num_init_regs = %d\n",
				pdata->num_init_regs);
			return -1;
		}
		for (i = 0; i < pdata->num_init_regs; i++)
			data[i] = pdata->init_regs[i];
		/*pr_info("init_regs[]: [%x][%x][%x][%x]\n",
			data[0], data[1], data[2], data[3]);*/
	} else {
		return -1;
	}

	regmap_raw_write(codec->control_data, DDX_INPUT_MUX, data, 4);
	return 0;
}

static int reset_tas5731_GPIO(struct snd_soc_codec *codec)
{
	struct tas5731_priv *tas5731 = snd_soc_codec_get_drvdata(codec);
	struct tas57xx_platform_data *pdata = tas5731->pdata;

	gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
	udelay(1000);
	gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_HIGH);
	mdelay(15);
	return 0;
}

static int tas5731_init(struct snd_soc_codec *codec)
{
	unsigned char burst_data[][4] = {
		{ 0x00, 0x01, 0x77, 0x72 },
		{ 0x00, 0x00, 0x43, 0x03 },
		{ 0x01, 0x02, 0x13, 0x45 },
	};
	struct tas5731_priv *tas5731 = snd_soc_codec_get_drvdata(codec);

	reset_tas5731_GPIO(codec);

	dev_info(codec->dev, "tas5731_init!\n");
	snd_soc_write(codec, DDX_OSC_TRIM, 0x00);
	msleep(50);
	snd_soc_write(codec, DDX_CLOCK_CTL, 0x6c);
	snd_soc_write(codec, DDX_SYS_CTL_1, 0xa0);
	snd_soc_write(codec, DDX_SERIAL_DATA_INTERFACE, 0x05);
	snd_soc_write(codec, DDX_BKND_ERR, 0x02);

	regmap_raw_write(codec->control_data, DDX_INPUT_MUX, burst_data[0], 4);
	regmap_raw_write(codec->control_data, DDX_CH4_SOURCE_SELECT,
			 burst_data[1], 4);
	regmap_raw_write(codec->control_data, DDX_PWM_MUX, burst_data[2], 4);

	/*drc */
	if ((tas5731_set_drc(codec)) < 0)
		dev_err(codec->dev, "fail to set tas5731 drc!\n");
	/*eq */
	if ((tas5731_set_eq(codec)) < 0)
		dev_err(codec->dev, "fail to set tas5731 eq!\n");
	/*init */
	if ((tas5731_customer_init(codec)) < 0)
		dev_err(codec->dev, " fail to set tas5731 customer init!\n");

	snd_soc_write(codec, DDX_VOLUME_CONFIG, 0xD1);
	snd_soc_write(codec, DDX_SYS_CTL_2, 0x00);
	snd_soc_write(codec, DDX_START_STOP_PERIOD, 0x0F);
	snd_soc_write(codec, DDX_PWM_SHUTDOWN_GROUP, 0x30);
	snd_soc_write(codec, DDX_MODULATION_LIMIT, 0x02);
	/*normal operation */
	if ((tas5731_set_master_vol(codec)) < 0)
		dev_err(codec->dev, "fail to set tas5731 master vol!\n");

	snd_soc_write(codec, DDX_CHANNEL1_VOL, tas5731->Ch1_vol);
	snd_soc_write(codec, DDX_CHANNEL2_VOL, tas5731->Ch2_vol);
	snd_soc_write(codec, DDX_SOFT_MUTE, 0x00);
	snd_soc_write(codec, DDX_CHANNEL3_VOL, 0x80);

	return 0;
}

static int tas5731_probe(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct tas5731_priv *tas5731 = snd_soc_codec_get_drvdata(codec);
	struct tas57xx_platform_data *pdata = dev_get_platdata(codec->dev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	tas5731->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	tas5731->early_suspend.suspend = tas5731_early_suspend;
	tas5731->early_suspend.resume = tas5731_late_resume;
	tas5731->early_suspend.param = codec;
	register_early_suspend(&(tas5731->early_suspend));
#endif

	tas5731->pdata = pdata;
	ret = snd_soc_codec_set_cache_io(codec, 8, 8, tas5731->control_type);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}
	tas5731_init(codec);

	return 0;
}

static int tas5731_remove(struct snd_soc_codec *codec)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct tas5731_priv *tas5731 = snd_soc_codec_get_drvdata(codec);
	unregister_early_suspend(&(tas5731->early_suspend));
#endif

	return 0;
}

#ifdef CONFIG_PM
static int tas5731_suspend(struct snd_soc_codec *codec)
{
	struct tas5731_priv *tas5731 = snd_soc_codec_get_drvdata(codec);
	struct tas57xx_platform_data *pdata = dev_get_platdata(codec->dev);

	dev_info(codec->dev, "tas5731_suspend!\n");

	if (pdata && pdata->suspend_func)
		pdata->suspend_func();

	/*save volume */
	tas5731->Ch1_vol = snd_soc_read(codec, DDX_CHANNEL1_VOL);
	tas5731->Ch2_vol = snd_soc_read(codec, DDX_CHANNEL2_VOL);
	tas5731->master_vol = snd_soc_read(codec, DDX_MASTER_VOLUME);
	tas5731_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int tas5731_resume(struct snd_soc_codec *codec)
{
	struct tas5731_priv *tas5731 = snd_soc_codec_get_drvdata(codec);
	struct tas57xx_platform_data *pdata = dev_get_platdata(codec->dev);

	dev_info(codec->dev, "tas5731_resume!\n");

	if (pdata && pdata->resume_func)
		pdata->resume_func();

	tas5731_init(codec);
	snd_soc_write(codec, DDX_CHANNEL1_VOL, tas5731->Ch1_vol);
	snd_soc_write(codec, DDX_CHANNEL2_VOL, tas5731->Ch2_vol);
	snd_soc_write(codec, DDX_MASTER_VOLUME, tas5731->master_vol);
	tas5731_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}
#else
#define tas5731_suspend NULL
#define tas5731_resume NULL
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void tas5731_early_suspend(struct early_suspend *h)
{
	return;
}

static void tas5731_late_resume(struct early_suspend *h)
{
	return;
}
#endif

static const struct snd_soc_dapm_widget tas5731_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("DAC", "HIFI Playback", SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_codec_driver tas5731_codec = {
	.probe = tas5731_probe,
	.remove = tas5731_remove,
	.suspend = tas5731_suspend,
	.resume = tas5731_resume,
	.reg_cache_size = DDX_NUM_BYTE_REG,
	.reg_word_size = sizeof(u8),
	.reg_cache_default = tas5731_regs,
	.set_bias_level = tas5731_set_bias_level,
	.controls = tas5731_snd_controls,
	.num_controls = ARRAY_SIZE(tas5731_snd_controls),
	.dapm_widgets = tas5731_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tas5731_dapm_widgets),
};

static int tas5731_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct tas5731_priv *tas5731;
	int ret;

	tas5731 = devm_kzalloc(&i2c->dev, sizeof(struct tas5731_priv),
			       GFP_KERNEL);
	if (!tas5731)
		return -ENOMEM;

	i2c_set_clientdata(i2c, tas5731);
	tas5731->control_type = SND_SOC_I2C;

	ret = snd_soc_register_codec(&i2c->dev, &tas5731_codec,
				     &tas5731_dai, 1);
	if (ret != 0)
		dev_err(&i2c->dev, "Failed to register codec (%d)\n", ret);

	return ret;
}

static int tas5731_i2c_remove(struct i2c_client *client)
{
	devm_kfree(&client->dev, i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id tas5731_i2c_id[] = {
	{ "tas5731", 0 },
	{}
};

static struct i2c_driver tas5731_i2c_driver = {
	.driver = {
		.name = "tas5731",
		.owner = THIS_MODULE,
	},
	.probe = tas5731_i2c_probe,
	.remove = tas5731_i2c_remove,
	.id_table = tas5731_i2c_id,
};

static int aml_tas5731_codec_probe(struct platform_device *pdev)
{
	return 0;
}
static int aml_tas5731_codec_remove(struct platform_device *pdev)
{
	return 0;
}
static const struct of_device_id amlogic_tas5731_codec_dt_match[] = {
	{ .compatible = "amlogic, aml_tas5731_codec", },
	{},
};

static struct platform_driver aml_tas5731_codec_platform_driver = {
	.driver = {
		.name = "tas5731",
		.owner = THIS_MODULE,
		.of_match_table = amlogic_tas5731_codec_dt_match,
	},
	.probe = aml_tas5731_codec_probe,
	.remove = aml_tas5731_codec_remove,
};

static int __init aml_tas5731_modinit(void)
{
	int ret = 0;

	ret = platform_driver_register(&aml_tas5731_codec_platform_driver);
	if (ret != 0)
		pr_err("Failed to register codec tas5731 platform driver\n");
	i2c_add_driver(&tas5731_i2c_driver);
	return ret;
}
static void __exit aml_tas5731_exit(void)
{
	platform_driver_unregister(&aml_tas5731_codec_platform_driver);
	i2c_del_driver(&tas5731_i2c_driver);
}

module_init(aml_tas5731_modinit);
module_exit(aml_tas5731_exit);

MODULE_DESCRIPTION("ASoC Tas5731 driver");
MODULE_AUTHOR("AML MM team");
MODULE_LICENSE("GPL");

