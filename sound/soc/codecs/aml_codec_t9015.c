#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/of.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/amlogic/iomap.h>
#include <linux/amlogic/sound/aiu_regs.h>

#include "aml_codec_t9015.h"

static struct mutex acodec;
void acodec_reg_write(unsigned data, unsigned addr)
{
	void __iomem *vaddr;
	mutex_lock(&acodec);
	vaddr = ioremap((addr), 0x4);
	writel(data, vaddr);
	iounmap(vaddr);
	mutex_unlock(&acodec);
}

unsigned acodec_reg_read(unsigned addr)
{
	unsigned tmp;
	void __iomem *vaddr;
	mutex_lock(&acodec);
	vaddr = ioremap((addr), 0x4);
	tmp = readl(vaddr);
	iounmap(vaddr);
	mutex_unlock(&acodec);
	return tmp;
}

struct aml_T9015_audio_priv {
	struct snd_soc_codec *codec;
	struct snd_pcm_hw_params *params;
};

struct T9015_audio_init_reg {
	u32 reg;
	u32 val;
};

static struct T9015_audio_init_reg init_list[] = {
	{AUDIO_CONFIG_BLOCK_ENABLE, 0x0000B00F},
	{ADC_VOL_CTR_PGA_IN_CONFIG, 0x00000000},
	{DAC_VOL_CTR_DAC_SOFT_MUTE, 0xFBFB0000},
	{LINE_OUT_CONFIG, 0x00001111},
	{POWER_CONFIG, 0x00010000},
};

#define T9015_AUDIO_INIT_REG_LEN ARRAY_SIZE(init_list)

static int aml_T9015_audio_reg_init(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < T9015_AUDIO_INIT_REG_LEN; i++)
		snd_soc_write(codec, init_list[i].reg, init_list[i].val);

	return 0;
}

static unsigned int aml_T9015_audio_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	u32 val;
	u32 int_reg = reg & (~0x3);
	val = acodec_reg_read(ACODEC_BASE_ADD + int_reg);
	return val;

}

static int aml_T9015_audio_write(struct snd_soc_codec *codec, unsigned int reg,
				unsigned int val)
{
	u32 int_reg = reg & (~0x3);
	acodec_reg_write(val, (ACODEC_BASE_ADD + int_reg));
	return 0;
}

static int aml_DAC_Gain_get_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	u32 add = ACODEC_BASE_ADD + ADC_VOL_CTR_PGA_IN_CONFIG;
	u32 val = acodec_reg_read(add);
	u32 val1 = (val & (0x1 <<  DAC_GAIN_SEL_L)) >> DAC_GAIN_SEL_L;
	u32 val2 = (val & (0x1 <<  DAC_GAIN_SEL_H)) >> (DAC_GAIN_SEL_H - 1);
	val = val1 | val2;
	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

static int aml_DAC_Gain_set_enum(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	u32 add = ACODEC_BASE_ADD + ADC_VOL_CTR_PGA_IN_CONFIG;
	u32 val = acodec_reg_read(add);

	if (ucontrol->value.enumerated.item[0] == 0) {
		val &= ~(0x1 << DAC_GAIN_SEL_H);
		val &= ~(0x1 << DAC_GAIN_SEL_L);
		acodec_reg_write(val, add);
	} else if (ucontrol->value.enumerated.item[0] == 1) {
		val &= ~(0x1 << DAC_GAIN_SEL_H);
		val |= (0x1 << DAC_GAIN_SEL_L);
		acodec_reg_write(val, add);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 2) {
		val |= (0x1 << DAC_GAIN_SEL_H);
		val &= ~(0x1 << DAC_GAIN_SEL_L);
		acodec_reg_write(val, add);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 3) {
		val |= (0x1 << DAC_GAIN_SEL_H);
		val |= (0x1 << DAC_GAIN_SEL_L);
		acodec_reg_write(val, add);
		pr_info("It has risk of distortion!\n");
	}
	return 0;
}

static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -95250, 375, 1);

static const char *const DAC_Gain_texts[] = { "0dB", "6dB", "12dB", "18dB" };

static const struct soc_enum DAC_Gain_enum = SOC_ENUM_SINGLE(
			SND_SOC_NOPM, 0, ARRAY_SIZE(DAC_Gain_texts),
			DAC_Gain_texts);

static const struct snd_kcontrol_new T9015_audio_snd_controls[] = {

	/*DAC Digital Volume control */
	SOC_DOUBLE_TLV("DAC Digital Playback Volume",
			   DAC_VOL_CTR_DAC_SOFT_MUTE,
			   DACL_VC, DACR_VC,
			   0xff, 0, dac_vol_tlv),

    /*DAC extra Digital Gain control */
	SOC_ENUM_EXT("DAC Extra Digital Gain",
			   DAC_Gain_enum,
			   aml_DAC_Gain_get_enum,
			   aml_DAC_Gain_set_enum),

};

/*line out Left Positive mux */
static const char * const T9015_out_lp_txt[] = {
	"None", "LOLP_SEL_DACL", "LOLP_SEL_DACL_INV"
};

static const SOC_ENUM_SINGLE_DECL(T9015_out_lp_enum, LINE_OUT_CONFIG,
				  LOLP_SEL_DACL, T9015_out_lp_txt);

static const struct snd_kcontrol_new line_out_lp_mux =
SOC_DAPM_ENUM("ROUTE_LP_OUT", T9015_out_lp_enum);

/*line out Left Negative mux */
static const char * const T9015_out_ln_txt[] = {
	"None", "LOLN_SEL_DACL_INV", "LOLN_SEL_DACL"
};

static const SOC_ENUM_SINGLE_DECL(T9015_out_ln_enum, LINE_OUT_CONFIG,
				  LOLN_SEL_DACL_INV, T9015_out_ln_txt);

static const struct snd_kcontrol_new line_out_ln_mux =
SOC_DAPM_ENUM("ROUTE_LN_OUT", T9015_out_ln_enum);

/*line out Right Positive mux */
static const char * const T9015_out_rp_txt[] = {
	"None", "LORP_SEL_DACR", "LORP_SEL_DACR_INV"
};

static const SOC_ENUM_SINGLE_DECL(T9015_out_rp_enum, LINE_OUT_CONFIG,
				  LORP_SEL_DACR, T9015_out_rp_txt);

static const struct snd_kcontrol_new line_out_rp_mux =
SOC_DAPM_ENUM("ROUTE_RP_OUT", T9015_out_rp_enum);

/*line out Right Negative mux */
static const char * const T9015_out_rn_txt[] = {
	"None", "LORN_SEL_DACR_INV", "LORN_SEL_DACR"
};

static const SOC_ENUM_SINGLE_DECL(T9015_out_rn_enum, LINE_OUT_CONFIG,
				  LORN_SEL_DACR_INV, T9015_out_rn_txt);

static const struct snd_kcontrol_new line_out_rn_mux =
SOC_DAPM_ENUM("ROUTE_RN_OUT", T9015_out_rn_enum);

static const struct snd_soc_dapm_widget T9015_audio_dapm_widgets[] = {

	/*Output */
	SND_SOC_DAPM_OUTPUT("Lineout left N"),
	SND_SOC_DAPM_OUTPUT("Lineout left P"),
	SND_SOC_DAPM_OUTPUT("Lineout right N"),
	SND_SOC_DAPM_OUTPUT("Lineout right P"),

	/*DAC playback stream */
	SND_SOC_DAPM_DAC("Left DAC", "HIFI Playback",
			 AUDIO_CONFIG_BLOCK_ENABLE,
			 DACL_EN, 0),
	SND_SOC_DAPM_DAC("Right DAC", "HIFI Playback",
			 AUDIO_CONFIG_BLOCK_ENABLE,
			 DACR_EN, 0),

	/*DRV output */
	SND_SOC_DAPM_OUT_DRV("LOLP_OUT_EN", SND_SOC_NOPM,
			     0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LOLN_OUT_EN", SND_SOC_NOPM,
			     0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LORP_OUT_EN", SND_SOC_NOPM,
			     0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LORN_OUT_EN", SND_SOC_NOPM,
			     0, 0, NULL, 0),

	/*MUX output source select */
	SND_SOC_DAPM_MUX("Lineout left P switch", SND_SOC_NOPM,
			 0, 0, &line_out_lp_mux),
	SND_SOC_DAPM_MUX("Lineout left N switch", SND_SOC_NOPM,
			 0, 0, &line_out_ln_mux),
	SND_SOC_DAPM_MUX("Lineout right P switch", SND_SOC_NOPM,
			 0, 0, &line_out_rp_mux),
	SND_SOC_DAPM_MUX("Lineout right N switch", SND_SOC_NOPM,
			 0, 0, &line_out_rn_mux),
};

static const struct snd_soc_dapm_route T9015_audio_dapm_routes[] = {
    /*Output path*/
	{"Lineout left P switch", "LOLP_SEL_DACL", "Left DAC"},
	{"Lineout left P switch", "LOLP_SEL_DACL_INV", "Left DAC"},

	{"Lineout left N switch", "LOLN_SEL_DACL_INV", "Left DAC"},
	{"Lineout left N switch", "LOLN_SEL_DACL", "Left DAC"},

	{"Lineout right P switch", "LORP_SEL_DACR", "Right DAC"},
	{"Lineout right P switch", "LORP_SEL_DACR_INV", "Right DAC"},

	{"Lineout right N switch", "LORN_SEL_DACR_INV", "Right DAC"},
	{"Lineout right N switch", "LORN_SEL_DACR", "Right DAC"},

	{"LOLN_OUT_EN", NULL, "Lineout left N switch"},
	{"LOLP_OUT_EN", NULL, "Lineout left P switch"},
	{"LORN_OUT_EN", NULL, "Lineout right N switch"},
	{"LORP_OUT_EN", NULL, "Lineout right P switch"},

	{"Lineout left N", NULL, "LOLN_OUT_EN"},
	{"Lineout left P", NULL, "LOLP_OUT_EN"},
	{"Lineout right N", NULL, "LORN_OUT_EN"},
	{"Lineout right P", NULL, "LORP_OUT_EN"},
};

static int aml_T9015_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		snd_soc_update_bits(codec, AUDIO_CONFIG_BLOCK_ENABLE,
					I2S_MODE, 1);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		snd_soc_update_bits(codec, AUDIO_CONFIG_BLOCK_ENABLE,
					I2S_MODE, 0);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int aml_T9015_set_dai_sysclk(struct snd_soc_dai *dai,
				   int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int aml_T9015_hw_params(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *params,
			      struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct aml_T9015_audio_priv *T9015_audio =
	    snd_soc_codec_get_drvdata(codec);

	T9015_audio->params = params;

	return 0;
}

static int aml_T9015_audio_set_bias_level(struct snd_soc_codec *codec,
					 enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:

		break;

	case SND_SOC_BIAS_PREPARE:

		break;

	case SND_SOC_BIAS_STANDBY:
		if (SND_SOC_BIAS_OFF == codec->dapm.bias_level) {
			codec->cache_only = false;
			codec->cache_sync = 1;
			snd_soc_cache_sync(codec);
		}
		break;

	case SND_SOC_BIAS_OFF:

		break;

	default:
		break;
	}
	codec->dapm.bias_level = level;

	return 0;
}

static int aml_T9015_prepare(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	/*struct snd_soc_codec *codec = dai->codec;*/
	return 0;

}

static int aml_T9015_audio_reset(struct snd_soc_codec *codec)
{
	aml_cbus_update_bits(RESET1_REGISTER, (1 << ACODEC_RESET),
					(1 << ACODEC_RESET));
	udelay(1000);
	return 0;
}

static int aml_T9015_audio_start_up(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, AUDIO_CONFIG_BLOCK_ENABLE, 0xF000);
	msleep(200);
	snd_soc_write(codec, AUDIO_CONFIG_BLOCK_ENABLE, 0xB000);
	return 0;
}

static int aml_T9015_codec_mute_stream(struct snd_soc_dai *dai, int mute,
				      int stream)
{
	struct snd_soc_codec *codec = dai->codec;
	u32 reg;
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		reg = snd_soc_read(codec, DAC_VOL_CTR_DAC_SOFT_MUTE);
		if (mute)
			reg |= 0x1 << DAC_SOFT_MUTE;
		else
			reg &= ~(0x1 << DAC_SOFT_MUTE);

		snd_soc_write(codec, DAC_VOL_CTR_DAC_SOFT_MUTE, reg);
	}
	return 0;
}

static int aml_T9015_audio_probe(struct snd_soc_codec *codec)
{
	struct aml_T9015_audio_priv *T9015_audio = NULL;

	T9015_audio = kzalloc(sizeof(struct aml_T9015_audio_priv), GFP_KERNEL);
	if (NULL == T9015_audio)
		return -ENOMEM;
	snd_soc_codec_set_drvdata(codec, T9015_audio);

	/*reset audio codec register*/
	aml_T9015_audio_reset(codec);
	aml_T9015_audio_start_up(codec);
	aml_T9015_audio_reg_init(codec);

	aml_write_cbus(AIU_ACODEC_CTRL, (1 << 4)
			   |(1 << 6)
			   |(1 << 11)
			   |(1 << 15)
			   |(2 << 2)
	);

	codec->dapm.bias_level = SND_SOC_BIAS_STANDBY;
	T9015_audio->codec = codec;

	return 0;
}

static int aml_T9015_audio_remove(struct snd_soc_codec *codec)
{
	aml_T9015_audio_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int aml_T9015_audio_suspend(struct snd_soc_codec *codec)
{
	aml_T9015_audio_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int aml_T9015_audio_resume(struct snd_soc_codec *codec)
{
	aml_T9015_audio_reset(codec);
	aml_T9015_audio_start_up(codec);
	aml_T9015_audio_reg_init(codec);
	codec->dapm.bias_level = SND_SOC_BIAS_STANDBY;
	aml_T9015_audio_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}

#define T9015_AUDIO_STEREO_RATES SNDRV_PCM_RATE_8000_96000
#define T9015_AUDIO_FORMATS (SNDRV_PCM_FMTBIT_S16_LE \
			| SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE \
			| SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S32_LE)

struct snd_soc_dai_ops T9015_audio_aif_dai_ops = {
	.hw_params = aml_T9015_hw_params,
	.prepare = aml_T9015_prepare,
	.set_fmt = aml_T9015_set_dai_fmt,
	.set_sysclk = aml_T9015_set_dai_sysclk,
	.mute_stream = aml_T9015_codec_mute_stream,
};

struct snd_soc_dai_driver aml_T9015_audio_dai[] = {
	{
	 .name = "T9015-audio-hifi",
	 .id = 0,
	 .playback = {
		      .stream_name = "HIFI Playback",
		      .channels_min = 2,
		      .channels_max = 8,
		      .rates = T9015_AUDIO_STEREO_RATES,
		      .formats = T9015_AUDIO_FORMATS,
		      },
	 .ops = &T9015_audio_aif_dai_ops,
	 },
};

static struct snd_soc_codec_driver soc_codec_dev_aml_T9015_audio = {
	.probe = aml_T9015_audio_probe,
	.remove = aml_T9015_audio_remove,
	.suspend = aml_T9015_audio_suspend,
	.resume = aml_T9015_audio_resume,
	.read = aml_T9015_audio_read,
	.write = aml_T9015_audio_write,
	.set_bias_level = aml_T9015_audio_set_bias_level,
	.controls = T9015_audio_snd_controls,
	.num_controls = ARRAY_SIZE(T9015_audio_snd_controls),
	.dapm_widgets = T9015_audio_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(T9015_audio_dapm_widgets),
	.dapm_routes = T9015_audio_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(T9015_audio_dapm_routes),
	.reg_cache_size = 16,
	.reg_word_size = sizeof(u16),
	.reg_cache_step = 2,
};

static int aml_T9015_audio_codec_probe(struct platform_device *pdev)
{
	int ret;
	dev_info(&pdev->dev, "aml_T9015_audio_codec_probe\n");
	mutex_init(&acodec);
	ret = snd_soc_register_codec(&pdev->dev,
				     &soc_codec_dev_aml_T9015_audio,
				     &aml_T9015_audio_dai[0], 1);
	return ret;
}

static int aml_T9015_audio_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct of_device_id aml_T9015_codec_dt_match[] = {
	{.compatible = "amlogic, aml_codec_T9015",},
	{},
};

static struct platform_driver aml_T9015_codec_platform_driver = {
	.driver = {
		   .name = "aml_codec_T9015",
		   .owner = THIS_MODULE,
		   .of_match_table = aml_T9015_codec_dt_match,
		   },
	.probe = aml_T9015_audio_codec_probe,
	.remove = aml_T9015_audio_codec_remove,
};

static int __init aml_T9015_audio_modinit(void)
{
	int ret = 0;

	ret = platform_driver_register(&aml_T9015_codec_platform_driver);
	if (ret != 0) {
		pr_err(
			"Failed to register AML T9015 codec platform driver: %d\n",
			ret);
	}

	return ret;
}

module_init(aml_T9015_audio_modinit);

static void __exit aml_T9015_audio_exit(void)
{
	platform_driver_unregister(&aml_T9015_codec_platform_driver);
}

module_exit(aml_T9015_audio_exit);

MODULE_DESCRIPTION("ASoC AML T9015 audio codec driver");
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_LICENSE("GPL");
