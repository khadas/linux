// SPDX-License-Identifier: GPL-2.0
//
// ASoC simple sound card support
//
// Copyright (C) 2012 Renesas Solutions Corp.
// Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>

//#define DEBUG
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
//#include <linux/extcon.h>
#include <linux/extcon-provider.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/control.h>
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
#endif
#include "card.h"

#include "effects.h"
#include "iomap.h"
#include "regs.h"
#include "../common/misc.h"
#include "../common/audio_uevent.h"
#include "audio_controller.h"
#include "tdm_hw.h"
#include "spdif_hw.h"
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif

/*the same as audio hal type define!*/
static const char * const audio_format[] = {
	"PCM",
	"DTS_EXPRESS",
	"DOLBY_DIGITAL",
	"DTS",
	"DOLBY_DIGITAL_PLUS",
	"DTS_HD",
	"MULTI_CHANNEL PCM",
	"DOLBY_TRUEHD",
	"DTS_HD_MA",
	"HIFI PCM",
	"DOLBY_AC4",
	"DOLBY_MAT",
	"DOLBY_DDP_ATMOS",
	"DOLBY_THD_ATMOS",
	"DOLBY_MAT_ATMOS",
	"DOLBY_AC4_ATMOS",
	"DTS_HP",
	"DOLBY_DDP_ATMOS_PROMPT_ON_ATMOS",
	"DOLBY_THD_ATMOS_PROMPT_ON_ATMOS",
	"DOLBY_MAT_ATMOS_PROMPT_ON_ATMOS",
	"DOLBY_AC4_ATMOS_PROMPT_ON_ATMOS",
};

enum audio_hal_format {
	TYPE_PCM = 0,
	TYPE_DTS_EXPRESS = 1,
	TYPE_AC3 = 2,
	TYPE_DTS = 3,
	TYPE_EAC3 = 4,
	TYPE_DTS_HD = 5,
	TYPE_MULTI_PCM = 6,
	TYPE_TRUE_HD = 7,
	TYPE_DTS_HD_MA = 8,
	TYPE_PCM_HIGH_SR = 9,
	TYPE_AC4 = 10,
	TYPE_MAT = 11,
	TYPE_DDP_ATMOS = 12,
	TYPE_TRUE_HD_ATMOS = 13,
	TYPE_MAT_ATMOS = 14,
	TYPE_AC4_ATMOS = 15,
	TYPE_DTS_HP = 16,
	TYPE_DDP_ATMOS_PROMPT_ON_ATMOS = 17,
	TYPE_TRUE_HD_ATMOS_PROMPT_ON_ATMOS = 18,
	TYPE_MAT_ATMOS_PROMPT_ON_ATMOS = 19,
	TYPE_AC4_ATMOS_PROMPT_ON_ATMOS = 20,
};

struct aml_jack {
	struct snd_soc_jack jack;
	struct snd_soc_jack_pin pin;
	struct snd_soc_jack_gpio gpio;
};

struct aml_card_data {
	struct snd_soc_card snd_card;
	struct aml_dai_props {
		/* sync with android audio hal,
		 * dai link is used for which output,
		 */
		const char *suffix_name;

		struct aml_dai cpu_dai;
		struct aml_dai codec_dai;
		struct snd_soc_dai_link_component cpus;   /* single cpu */
		struct snd_soc_dai_link_component codecs; /* single codec */
		struct snd_soc_dai_link_component platforms;
		unsigned int mclk_fs;
	} *dai_props;
	unsigned int mclk_fs;
	struct aml_jack hp_jack;
	struct aml_jack mic_jack;
	struct snd_soc_dai_link *dai_link;
	int spk_mute_gpio;
	bool spk_mute_active_low;
	bool spk_mute_flag;
	struct gpio_desc *avout_mute_desc;
	struct timer_list timer;
	struct work_struct work;
	struct work_struct init_work;
	int hp_last_state;
	int hp_cur_state;
	int hp_det_status;
	int hp_gpio_det;
	int hp_detect_flag;
	bool hp_det_enable;
	enum of_gpio_flags hp_det_flags;
	int micphone_last_state;
	int micphone_cur_state;
	int micphone_det_status;
	int micphone_gpio_det;
	int mic_detect_flag;
	bool mic_det_enable;
	bool av_mute_enable;
	bool spk_mute_enable;
	int irq_exception64;
	enum audio_hal_format hal_fmt;
	int inskew_tdm_index;
	int audio_inskew;
	int binv_tdm_index;
	int audio_binv;
	/* for I2S to HDMI output audio type */
	enum aud_codec_types hdmi_audio_type;
	enum hdmitx_src hdmitx_src;
	int i2s_to_hdmitx_mask;
	/* soft locker attached to */
	struct soft_locker slocker;
};

#define aml_priv_to_dev(priv) ((priv)->snd_card.dev)
#define aml_priv_to_link(priv, i) ((priv)->snd_card.dai_link + (i))
#define aml_priv_to_props(priv, i) ((priv)->dai_props + (i))
#define aml_card_to_priv(card) \
	(container_of(card, struct aml_card_data, snd_card))

struct soft_locker *aml_get_card_locker(struct snd_soc_card *card)
{
	struct aml_card_data *priv = aml_card_to_priv(card);

	return &priv->slocker;
}

#define DAI	"sound-dai"
#define CELL	"#sound-dai-cells"
#define PREFIX	"aml-audio-card,"

#define aml_card_init_hp(card, sjack, prefix)\
	aml_card_init_jack(card, sjack, 1, prefix)
#define aml_card_init_mic(card, sjack, prefix)\
	aml_card_init_jack(card, sjack, 0, prefix)

enum hdmitx_src get_hdmitx_audio_src(struct snd_soc_card *card)
{
	struct aml_card_data *priv = aml_card_to_priv(card);

	return priv->hdmitx_src;
}

enum aud_codec_types get_i2s2hdmitx_audio_format(struct snd_soc_card *card)
{
	struct aml_card_data *priv = aml_card_to_priv(card);

	return priv->hdmi_audio_type;
}

int get_hdmitx_i2s_mask(struct snd_soc_card *card)
{
	struct aml_card_data *priv = aml_card_to_priv(card);

	return priv->i2s_to_hdmitx_mask;
}

static const unsigned int headphone_cable[] = {
	EXTCON_JACK_HEADPHONE,
	EXTCON_NONE,
};

static const unsigned int microphone_cable[] = {
	EXTCON_JACK_MICROPHONE,
	EXTCON_NONE,
};

struct extcon_dev *audio_extcon_headphone;
struct extcon_dev *audio_extcon_microphone;

int get_aml_audio_binv(struct snd_soc_card *card)
{
	struct aml_card_data *priv = aml_card_to_priv(card);

	return priv->audio_binv;
}

int get_aml_audio_binv_index(struct snd_soc_card *card)
{
	struct aml_card_data *priv = aml_card_to_priv(card);

	return priv->binv_tdm_index;
}

int set_aml_audio_binv(struct snd_soc_card *card, int audio_binv)
{
	struct aml_card_data *priv = aml_card_to_priv(card);

	priv->audio_binv = audio_binv;
	return 0;
}

int set_aml_audio_binv_index(struct snd_soc_card *card, int binv_tdm_index)
{
	struct aml_card_data *priv = aml_card_to_priv(card);

	priv->binv_tdm_index = binv_tdm_index;
	return 0;
}

int get_aml_audio_inskew(struct snd_soc_card *card)
{
	struct aml_card_data *priv = aml_card_to_priv(card);

	return priv->audio_inskew;
}

int get_aml_audio_inskew_index(struct snd_soc_card *card)
{
	struct aml_card_data *priv = aml_card_to_priv(card);

	return priv->inskew_tdm_index;
}

int set_aml_audio_inskew(struct snd_soc_card *card, int audio_inskew)
{
	struct aml_card_data *priv = aml_card_to_priv(card);

	priv->audio_inskew = audio_inskew;
	return 0;
}

int set_aml_audio_inskew_index(struct snd_soc_card *card, int inskew_tdm_index)
{
	struct aml_card_data *priv = aml_card_to_priv(card);

	priv->inskew_tdm_index = inskew_tdm_index;
	return 0;
}

static const struct soc_enum audio_hal_format_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(audio_format),
			audio_format);

static int aml_audio_hal_format_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct aml_card_data *p_aml_audio;

	p_aml_audio = snd_soc_card_get_drvdata(card);
	ucontrol->value.integer.value[0] = p_aml_audio->hal_fmt;

	return 0;
}

static int aml_audio_hal_format_set_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct aml_card_data *p_aml_audio;
	int hal_format = ucontrol->value.integer.value[0];
	struct snd_card *snd = card->snd_card;

	p_aml_audio = snd_soc_card_get_drvdata(card);

	audio_send_uevent(&snd->ctl_dev, AUDIO_SPDIF_FMT_EVENT, hal_format);
	pr_info("update audio atmos flag! audio_type = %d\n", hal_format);

	if (p_aml_audio->hal_fmt != hal_format)
		p_aml_audio->hal_fmt = hal_format;

	return 0;
}

static int aml_chip_id_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	(void)kcontrol;
	ucontrol->value.integer.value[0] = aml_return_chip_id();
	return 0;
}

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
static int aml_media_video_delay_get_enum(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	(void)kcontrol;
	ucontrol->value.integer.value[0] = get_playback_delay_duration();
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_HDMITX
static int i2s_to_hdmitx_format_get_enum(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct aml_card_data *card_data = snd_soc_card_get_drvdata(card);

	ucontrol->value.enumerated.item[0] = card_data->hdmi_audio_type;
	return 0;
}

static int i2s_to_hdmitx_format_put_enum(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct aml_card_data *card_data = snd_soc_card_get_drvdata(card);
	int index = ucontrol->value.enumerated.item[0];

	if (index >= AUD_CODEC_TYPE_OBA) {
		pr_err("bad parameter for i2s2hdmitx format set: %d\n", index);
		return -EINVAL;
	}

	card_data->hdmi_audio_type = index;

	return 0;
}

static int i2s_to_hdmitx_mask_get_enum(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct aml_card_data *card_data = snd_soc_card_get_drvdata(card);

	ucontrol->value.enumerated.item[0] = card_data->i2s_to_hdmitx_mask;
	return 0;
}

static int i2s_to_hdmitx_mask_put_enum(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct aml_card_data *card_data = snd_soc_card_get_drvdata(card);
	int mask = ucontrol->value.enumerated.item[0];

	card_data->i2s_to_hdmitx_mask = mask & 0xf;
	return 0;
}

static const char *const hdmitx_src_select[] = {"Spdif", "Spdif_b",
		"Tdm_A", "Tdm_B", "Tdm_C"};
static const struct soc_enum hdmitx_src_select_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(hdmitx_src_select), hdmitx_src_select);
static int hdmitx_src_select_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct aml_card_data *card_data = snd_soc_card_get_drvdata(card);

	ucontrol->value.enumerated.item[0] = card_data->hdmitx_src;
	return 0;
}

static int hdmitx_src_select_put_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct aml_card_data *card_data = snd_soc_card_get_drvdata(card);
	enum hdmitx_src src = ucontrol->value.enumerated.item[0];

	if (src < HDMITX_SRC_SPDIF || src >= HDMITX_SRC_NUM) {
		pr_err("inval hdmitx src: %d\n", src);
		return 0;
	}

	card_data->hdmitx_src = ucontrol->value.enumerated.item[0];

	return 0;
}
#endif

static const struct snd_kcontrol_new snd_user_controls[] = {
	SOC_ENUM_EXT("Audio HAL Format",
			audio_hal_format_enum,
			aml_audio_hal_format_get_enum,
			aml_audio_hal_format_set_enum),

	SND_SOC_BYTES_EXT("AML chip id", 1,
			aml_chip_id_get, NULL),

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	SOC_SINGLE_EXT("Media Video Delay",
			0, 0, 0, 0,
			aml_media_video_delay_get_enum,
			NULL),
#endif
#ifdef CONFIG_AMLOGIC_HDMITX
	SOC_ENUM_EXT("HDMITX Audio Source Select",
			hdmitx_src_select_enum,
			hdmitx_src_select_get_enum,
			hdmitx_src_select_put_enum),
	SOC_ENUM_EXT("Audio I2S to HDMITX Format",
			aud_codec_type_enum,
			i2s_to_hdmitx_format_get_enum,
			i2s_to_hdmitx_format_put_enum),
	SOC_SINGLE_EXT("Audio I2S to HDMITX Mask",
			0, 0, 0xf, 0,
			i2s_to_hdmitx_mask_get_enum,
			i2s_to_hdmitx_mask_put_enum),
#endif
};

static void jack_audio_start_timer(struct aml_card_data *card_data,
				  unsigned long delay)
{
	card_data->timer.expires = jiffies + delay;
	add_timer(&card_data->timer);
}

static void jack_audio_stop_timer(struct aml_card_data *card_data)
{
	del_timer_sync(&card_data->timer);
	cancel_work_sync(&card_data->work);
}

static void jack_timer_func(struct timer_list *t)
{
	struct aml_card_data *card_data = from_timer(card_data, t, timer);
	unsigned long delay = msecs_to_jiffies(150);

	schedule_work(&card_data->work);
	mod_timer(&card_data->timer, jiffies + delay);
}

static int jack_audio_hp_detect(struct aml_card_data *card_data)
{
	int loop_num = 0;
	int change_cnt = 0;

	card_data->hp_cur_state =
		gpio_get_value_cansleep(card_data->hp_jack.gpio.gpio);
	if (card_data->hp_last_state != card_data->hp_cur_state) {
		while (loop_num < 5) {
			card_data->hp_cur_state =
				gpio_get_value_cansleep(card_data->hp_jack.gpio.gpio);

			if (card_data->hp_last_state != card_data->hp_cur_state)
				change_cnt++;
			else
				change_cnt = 0;

			msleep_interruptible(50);
			loop_num = loop_num + 1;
		}
		if (change_cnt >= 5) {
			card_data->hp_last_state = card_data->hp_cur_state;
			card_data->hp_det_status = card_data->hp_last_state;
		}
		return card_data->hp_det_status;
	}
	return -1;
}

static int jack_audio_micphone_detect(struct aml_card_data *card_data)
{
	int loop_num = 0;
	int change_cnt = 0;

	card_data->micphone_cur_state =
		gpio_get_value_cansleep(card_data->mic_jack.gpio.gpio);
	if (card_data->micphone_last_state != card_data->micphone_cur_state) {
		while (loop_num < 5) {
			card_data->micphone_cur_state =
				gpio_get_value_cansleep(card_data->mic_jack.gpio.gpio);
			if (card_data->micphone_last_state !=
				card_data->micphone_cur_state)
				change_cnt++;
			else
				change_cnt = 0;

			msleep_interruptible(50);
			loop_num = loop_num + 1;
		}
		if (change_cnt >= 5) {
			card_data->micphone_last_state =
				card_data->micphone_cur_state;
			card_data->micphone_det_status =
				card_data->micphone_last_state;
		}
		return card_data->micphone_det_status;
	}
	return -1;
}

static void jack_work_func(struct work_struct *work)
{
	struct aml_card_data *card_data = NULL;
	int status = SND_JACK_HEADPHONE;
	int flag = 0;

	card_data = container_of(work, struct aml_card_data, work);

	if (card_data->hp_det_enable == 1) {
		flag = jack_audio_hp_detect(card_data);
		if (flag == -1)
			return;

		if (card_data->hp_detect_flag != flag) {
			card_data->hp_detect_flag = flag;

			if (card_data->hp_det_flags == OF_GPIO_ACTIVE_LOW)
				flag = (flag) ? 0 : 1;

			if (flag) {
				/*
				 * pr_info("headphone is pluged, mute speaker!\n");
				 * aml_tdmout_mute_speaker(TDM_A, 1);
				 * aml_tdmout_mute_speaker(TDM_B, 1);
				 */
				extcon_set_state_sync(audio_extcon_headphone,
					EXTCON_JACK_HEADPHONE, 1);
				snd_soc_jack_report(&card_data->hp_jack.jack,
					1, SND_JACK_HEADPHONE);
			} else {
				extcon_set_state_sync(audio_extcon_headphone,
					EXTCON_JACK_HEADPHONE, 0);
				snd_soc_jack_report(&card_data->hp_jack.jack,
					0, SND_JACK_HEADPHONE);
				/*
				 * pr_info("headphone is unpluged, unmute speaker!\n");
				 * aml_tdmout_mute_speaker(TDM_A, 0);
				 * aml_tdmout_mute_speaker(TDM_B, 0);
				 */
			}
		}
	}
	if (card_data->mic_det_enable == 1) {
		flag = jack_audio_micphone_detect(card_data);
		if (flag == -1)
			return;
		if (card_data->mic_detect_flag != flag) {
			card_data->mic_detect_flag = flag;

			if (flag) {
				extcon_set_state_sync(audio_extcon_microphone,
					EXTCON_JACK_MICROPHONE, 1);
				snd_soc_jack_report(&card_data->mic_jack.jack,
					status, SND_JACK_MICROPHONE);
				audio_send_uevent(card_data->snd_card.dev,
					MICROPHONE_DETECTION_EVENT, 1);
			} else {
				extcon_set_state_sync(audio_extcon_microphone,
					EXTCON_JACK_MICROPHONE, 0);
				snd_soc_jack_report(&card_data->mic_jack.jack,
					0, SND_JACK_MICROPHONE);
				audio_send_uevent(card_data->snd_card.dev,
					MICROPHONE_DETECTION_EVENT, 0);
			}
		}
	}
}

static int aml_card_init_jack(struct snd_soc_card *card,
				      struct aml_jack *sjack,
				      int is_hp, char *prefix)
{
	struct aml_card_data *priv = aml_card_to_priv(card);
	struct device *dev = card->dev;
	enum of_gpio_flags flags = 0;
	char prop[128];
	char *pin_name;
	char *gpio_name;
	int mask;
	int det;

	sjack->gpio.gpio = -ENOENT;

	if (is_hp) {
		snprintf(prop, sizeof(prop), "%shp-det-gpio", prefix);
		pin_name	= "Headphones";
		gpio_name	= "Headphone detection";
		mask		= SND_JACK_HEADPHONE;

		det = of_get_named_gpio_flags(dev->of_node, prop, 0, &priv->hp_det_flags);
		if (det < 0) {
			priv->hp_det_enable = 0;
			return -1;
		}
		priv->hp_det_enable = 1;
		gpio_request(det, "hp-det-gpio");

		pr_info("find headphone detection pin success! hp_det_flags:%d\n",
				priv->hp_det_flags);
	} else {
		snprintf(prop, sizeof(prop), "%smic-det-gpio", prefix);
		pin_name	= "Mic Jack";
		gpio_name	= "Mic detection";
		mask		= SND_JACK_MICROPHONE;

		det = of_get_named_gpio_flags(dev->of_node, prop, 0, &flags);
		if (det < 0) {
			priv->mic_det_enable = 0;
			return -1;
		}
		priv->mic_det_enable = 1;
		gpio_request(det, "mic-det-gpio");
	}

	if (gpio_is_valid(det)) {
		int state;

		sjack->pin.pin		= pin_name;
		sjack->pin.mask		= mask;

		sjack->gpio.name	= gpio_name;
		sjack->gpio.report	= mask;
		sjack->gpio.gpio	= det;
		sjack->gpio.invert	= !!(flags & OF_GPIO_ACTIVE_LOW);
		sjack->gpio.debounce_time = 150;

		gpio_direction_input(det);
		state = gpiod_set_pull(gpio_to_desc(det), GPIOD_PULL_DIS);
		if (state)
			pr_err("set gpiod pull failed, ret %d\n", state);
		snd_soc_card_jack_new(card, pin_name, mask,
				      &sjack->jack,
				      &sjack->pin, 1);
	} else {
		pr_info("detect gpio is invalid\n");
	}

	if (is_hp) {
		if (det >= 0)
			priv->hp_gpio_det = det;
	} else {
		if (det >= 0)
			priv->micphone_gpio_det = det;
	}
	return 0;
}

static void audio_jack_detect(struct aml_card_data *card_data)
{
	timer_setup(&card_data->timer, jack_timer_func, 0);

	INIT_WORK(&card_data->work, jack_work_func);

	jack_audio_start_timer(card_data,
			msecs_to_jiffies(5000));
}

static void audio_extcon_register(struct aml_card_data *priv,
	struct device *dev)
{
	struct extcon_dev *edev;
	int ret;

	if (priv->hp_det_enable == 1) {
		/*audio extcon headphone*/
		edev = devm_extcon_dev_allocate(dev, headphone_cable);
		if (IS_ERR(edev)) {
			pr_info("failed to allocate audio extcon headphone\n");
			return;
		}
		/*
		 * edev->dev.parent = dev;
		 * edev->name = "audio_extcon_headphone";
		 * dev_set_name(&edev->dev, "headphone");
		 */
		ret = devm_extcon_dev_register(dev, edev);
		if (ret < 0) {
			pr_info("failed to register audio extcon headphone\n");
			return;
		}
		audio_extcon_headphone = edev;
	}
	if (priv->mic_det_enable == 1) {
		/*audio extcon microphone*/
		edev = devm_extcon_dev_allocate(dev, microphone_cable);
		if (IS_ERR(edev)) {
			pr_info("failed to allocate audio extcon microphone\n");
			return;
		}

		/*
		 * edev->dev.parent = dev;
		 * edev->name = "audio_extcon_microphone";
		 * dev_set_name(&edev->dev, "microphone");
		 */
		ret = devm_extcon_dev_register(dev, edev);
		if (ret < 0) {
			pr_info("failed to register audio extcon microphone\n");
			return;
		}
		audio_extcon_microphone = edev;
	}
}

static void aml_card_remove_jack(struct aml_jack *sjack)
{
	if (gpio_is_valid(sjack->gpio.gpio))
		snd_soc_jack_free_gpios(&sjack->jack, 1, &sjack->gpio);
}

static int aml_card_hw_params(struct snd_pcm_substream *substream,
				      struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct aml_card_data *priv = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai_link *dai_link = aml_priv_to_link(priv, rtd->num);
	struct aml_dai_props *dai_props =
		aml_priv_to_props(priv, rtd->num);
	unsigned int mclk = 0, mclk_fs = 0;
	int i = 0, ret = 0;

	if (priv->mclk_fs)
		mclk_fs = priv->mclk_fs;
	else if (dai_props->mclk_fs)
		mclk_fs = dai_props->mclk_fs;

	if (mclk_fs) {
		mclk = params_rate(params) * mclk_fs;

		for (i = 0; i < rtd->num_codecs; i++) {
			codec_dai = rtd->codec_dais[i];
			ret = snd_soc_dai_set_sysclk(codec_dai, 0, mclk,
				SND_SOC_CLOCK_IN);

			if (ret && ret != -ENOTSUPP) {
				pr_err("codec_dai soc dai set sysclk failed\n");
				goto err;
			}
		}

		ret = snd_soc_dai_set_fmt(cpu_dai, dai_link->dai_fmt);
		if (ret && ret != -ENOTSUPP) {
			pr_err("cpu_dai soc dai set fmt failed\n");
			goto err;
		}
	}

	return 0;
err:
	return ret;
}

static struct snd_soc_ops aml_card_ops = {
	.hw_params  = aml_card_hw_params,
};

static int aml_card_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct aml_card_data *priv = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *codec = rtd->codec_dai;
	struct snd_soc_dai *cpu = rtd->cpu_dai;
	struct aml_dai_props *dai_props =
		aml_priv_to_props(priv, rtd->num);
	static int hp_mic_detect_cnt;
	bool idle_clk = false;
	int ret, i;

	/* enable dai-link mclk when CONTINUOUS clk setted */
	idle_clk = !!(rtd->dai_link->dai_fmt & SND_SOC_DAIFMT_CONT);

	for (i = 0; i < rtd->num_codecs; i++) {
		codec = rtd->codec_dais[i];

		ret = aml_card_init_dai(codec, &dai_props->codec_dai, idle_clk);
		if (ret < 0)
			return ret;
	}

	ret = aml_card_init_dai(cpu, &dai_props->cpu_dai, idle_clk);
	if (ret < 0)
		return ret;

	if (hp_mic_detect_cnt == 0) {
		aml_card_init_hp(rtd->card, &priv->hp_jack, PREFIX);
		aml_card_init_mic(rtd->card, &priv->mic_jack, PREFIX);
		hp_mic_detect_cnt = 1;
	}

	return 0;
}

static int aml_card_dai_link_of(struct device_node *node,
					struct aml_card_data *priv,
					int idx,
					bool is_top_level_node)
{
	struct device *dev = aml_priv_to_dev(priv);
	struct snd_soc_dai_link *dai_link = aml_priv_to_link(priv, idx);
	struct aml_dai_props *dai_props = aml_priv_to_props(priv, idx);
	struct aml_dai *cpu_dai = &dai_props->cpu_dai;
	struct aml_dai *codec_dai = &dai_props->codec_dai;
	struct device_node *cpu = NULL;
	struct device_node *plat = NULL;
	struct device_node *codec = NULL;
	char prop[128];
	char *prefix = "";
	int ret, single_cpu;

	/* For single DAI link & old style of DT node */
	if (is_top_level_node)
		prefix = PREFIX;

	snprintf(prop, sizeof(prop), "%scpu", prefix);
	cpu = of_get_child_by_name(node, prop);

	snprintf(prop, sizeof(prop), "%splat", prefix);
	plat = of_get_child_by_name(node, prop);

	snprintf(prop, sizeof(prop), "%scodec", prefix);
	codec = of_get_child_by_name(node, prop);

	if (!cpu || !codec) {
		ret = -EINVAL;
		dev_err(dev, "%s: Can't find %s DT node\n", __func__, prop);
		goto dai_link_of_err;
	}

	dai_link->cpus->of_node = of_parse_phandle(cpu, DAI, 0);
	if (!dai_link->cpus->of_node) {
		dev_err(dev, "error getting cpu phandle\n");
		return -EINVAL;
	}

	ret = aml_card_parse_daifmt(dev, node, codec,
					    prefix, &dai_link->dai_fmt);
	if (ret < 0) {
		dev_err(dev, "%s, dai fmt not found\n",
			__func__);
		goto dai_link_of_err;
	}
	of_property_read_u32(node, "mclk-fs", &dai_props->mclk_fs);

	ret = aml_card_parse_cpu(cpu, dai_link,
					 DAI, CELL, &single_cpu);
	if (ret < 0) {
		dev_err(dev, "%s, dai-link idx:%d, error getting cpu dai name:%s\n",
			__func__,
			idx,
			dai_link->cpus->dai_name);
		goto dai_link_of_err;
	}

	ret = snd_soc_of_get_dai_link_codecs(dev, codec, dai_link);

	if (ret < 0) {
		dev_err(dev, "%s, error dai-link idx:%d, ret %d\n", __func__, idx, ret);
		goto dai_link_of_err;
	}

	ret = aml_card_parse_platform(plat, dai_link, DAI, CELL);
	if (ret < 0) {
		dev_err(dev, "%s, platform not found\n",
			__func__);

		goto dai_link_of_err;
	}

	ret = snd_soc_of_parse_tdm_slot(cpu,	&cpu_dai->tx_slot_mask,
						&cpu_dai->rx_slot_mask,
						&cpu_dai->slots,
						&cpu_dai->slot_width);
	if (ret < 0)
		goto dai_link_of_err;

	ret = snd_soc_of_parse_tdm_slot(codec,	&codec_dai->tx_slot_mask,
						&codec_dai->rx_slot_mask,
						&codec_dai->slots,
						&codec_dai->slot_width);
	if (ret < 0)
		goto dai_link_of_err;

	ret = aml_card_parse_codec_confs(codec, &priv->snd_card);
	if (ret < 0)
		goto dai_link_of_err;

	ret = aml_card_parse_clk_cpu(cpu, dai_link, cpu_dai);
	if (ret < 0)
		goto dai_link_of_err;

	ret = aml_card_canonicalize_dailink(dai_link);
	if (ret < 0)
		goto dai_link_of_err;

	/* sync with android audio hal, what's the link used for. */
	of_property_read_string(node, "suffix-name", &dai_props->suffix_name);

	if (dai_props->suffix_name)
		ret = aml_card_set_dailink_name(dev, dai_link,
					"%s-%s-%s",
					dai_link->cpus->dai_name,
					dai_link->codecs->dai_name,
					dai_props->suffix_name);
	else
		ret = aml_card_set_dailink_name(dev, dai_link,
					"%s-%s",
					dai_link->cpus->dai_name,
					dai_link->codecs->dai_name);
	if (ret < 0)
		goto dai_link_of_err;

	locker_add_dai_name(&priv->slocker, idx, dai_link->cpus->dai_name);

	dai_link->ops = &aml_card_ops;
	dai_link->init = aml_card_dai_init;
	dai_link->nonatomic = 1;
	dev_dbg(dev, "\tname : %s\n", dai_link->stream_name);
	dev_dbg(dev, "\tformat : %04x\n", dai_link->dai_fmt);
	dev_dbg(dev, "\tcpu : %s / %d\n",
		dai_link->cpus->dai_name,
		dai_props->cpu_dai.sysclk);
	dev_dbg(dev, "\tcodec : %s / %d\n",
		dai_link->codecs->dai_name,
		dai_props->codec_dai.sysclk);

	aml_card_canonicalize_cpu(dai_link, single_cpu);

dai_link_of_err:
	of_node_put(cpu);
	of_node_put(codec);

	return ret;
}

static int aml_card_parse_aux_devs(struct device_node *node,
					   struct aml_card_data *priv)
{
	struct device *dev = aml_priv_to_dev(priv);
	struct device_node *aux_node;
	int i, n, len;

	if (!of_find_property(node, PREFIX "aux-devs", &len))
		return 0;		/* Ok to have no aux-devs */

	n = len / sizeof(__be32);
	if (n <= 0)
		return -EINVAL;

	priv->snd_card.aux_dev = devm_kzalloc(dev,
			n * sizeof(*priv->snd_card.aux_dev), GFP_KERNEL);
	if (!priv->snd_card.aux_dev)
		return -ENOMEM;

	for (i = 0; i < n; i++) {
		aux_node = of_parse_phandle(node, PREFIX "aux-devs", i);
		if (!aux_node)
			return -EINVAL;
		priv->snd_card.aux_dev[i].dlc.of_node = aux_node;
	}

	priv->snd_card.num_aux_devs = n;
	return 0;
}

static int spk_mute_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *soc_card = snd_kcontrol_chip(kcontrol);
	struct aml_card_data *priv = aml_card_to_priv(soc_card);
	int gpio = priv->spk_mute_gpio;
	bool active_low = priv->spk_mute_active_low;
	bool mute = ucontrol->value.integer.value[0];

	priv->spk_mute_flag = mute;

	if (gpio_is_valid(gpio)) {
		bool value = active_low ? !mute : mute;

		gpio_set_value(gpio, value);
		pr_info("%s: mute flag = %d\n", __func__, mute);
	}

	return 0;
}

static int spk_mute_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *soc_card = snd_kcontrol_chip(kcontrol);
	struct aml_card_data *priv = aml_card_to_priv(soc_card);
	int gpio = priv->spk_mute_gpio;
	bool active_low = priv->spk_mute_active_low;

	if (gpio_is_valid(gpio)) {
		bool value = gpio_get_value(gpio);
		bool mute = active_low ? !value : value;

		ucontrol->value.integer.value[0] = mute;
	}

	return 0;
}

static const struct snd_kcontrol_new card_controls[] = {
	SOC_SINGLE_BOOL_EXT("SPK mute", 0,
			    spk_mute_get,
			    spk_mute_set),
};

static int aml_card_parse_gpios(struct device_node *node,
					   struct aml_card_data *priv)
{
	struct device *dev = aml_priv_to_dev(priv);
	struct snd_soc_card *soc_card = &priv->snd_card;
	enum of_gpio_flags flags;
	int gpio;
	bool active_low;
	unsigned int sleep_time = 500;

	gpio = of_get_named_gpio_flags(node, "spk_mute-gpios", 0, &flags);
	priv->spk_mute_gpio = gpio;

	if (gpio_is_valid(gpio)) {
		int  ret;
		active_low = !!(flags & OF_GPIO_ACTIVE_LOW);
		flags = active_low ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
		priv->spk_mute_active_low = active_low;

		snd_soc_add_card_controls(soc_card, card_controls,
						ARRAY_SIZE(card_controls));

		if (priv->spk_mute_enable) {
			gpio_set_value(priv->spk_mute_gpio,
				(active_low) ? GPIOF_OUT_INIT_LOW :
				GPIOF_OUT_INIT_HIGH);
		} else {
			msleep(200);
			if (!priv->spk_mute_flag)
				gpio_set_value(priv->spk_mute_gpio,
					(active_low) ? GPIOF_OUT_INIT_HIGH :
					GPIOF_OUT_INIT_LOW);
		}

		ret = devm_gpio_request_one(dev, gpio, flags, "spk_mute");
		if (ret < 0) {
			dev_err(dev, "spk_mute get gpio error!\n");
		}
	}
	if (IS_ERR_OR_NULL(priv->avout_mute_desc)) {
		priv->avout_mute_desc = gpiod_get(dev,
					"avout_mute", GPIOF_OUT_INIT_LOW);
	}
	if (!IS_ERR(priv->avout_mute_desc)) {
		if (!priv->av_mute_enable) {
			if (!of_property_read_u32(node,
				"av_mute_sleep_time", &sleep_time))
				msleep(sleep_time);
			else
				msleep(500);
			gpiod_direction_output(priv->avout_mute_desc,
				GPIOF_OUT_INIT_HIGH);
			pr_debug("av out status: %s\n",
				gpiod_get_value(priv->avout_mute_desc) ?
				"high" : "low");
		} else {
			gpiod_direction_output(priv->avout_mute_desc,
				GPIOF_OUT_INIT_LOW);
			pr_debug("av out status: %s\n",
				gpiod_get_value(priv->avout_mute_desc) ?
				"high" : "low");
		}
	}

	return 0;
}

static void aml_init_work(struct work_struct *init_work)
{
	struct aml_card_data *priv = NULL;
	struct device *dev = NULL;
	struct device_node *np = NULL;

	priv = container_of(init_work,
			struct aml_card_data, init_work);
	dev = aml_priv_to_dev(priv);
	np = dev->of_node;
	aml_card_parse_gpios(np, priv);
}

static void aml_card_gpio(struct aml_card_data *priv)
{
	struct device *dev = aml_priv_to_dev(priv);
	enum of_gpio_flags flags;
	bool active_low;
	int gpio;

	gpio = of_get_named_gpio_flags(dev->of_node, "spk_mute-gpios", 0, &flags);
	priv->spk_mute_gpio = gpio;

	if (gpio_is_valid(priv->spk_mute_gpio)) {
		active_low = !!(flags & OF_GPIO_ACTIVE_LOW);
		flags = active_low ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
		priv->spk_mute_active_low = active_low;
		if (priv->spk_mute_enable) {
			gpio_set_value(priv->spk_mute_gpio,
				(active_low) ? GPIOF_OUT_INIT_LOW :
				GPIOF_OUT_INIT_HIGH);
		} else {
			if (!priv->spk_mute_flag)
				gpio_set_value(priv->spk_mute_gpio,
					(active_low) ? GPIOF_OUT_INIT_HIGH :
					GPIOF_OUT_INIT_LOW);
		}
	}

	if (!IS_ERR(priv->avout_mute_desc)) {
		if (!priv->av_mute_enable) {
			gpiod_direction_output(priv->avout_mute_desc,
				GPIOF_OUT_INIT_HIGH);
		} else {
			gpiod_direction_output(priv->avout_mute_desc,
				GPIOF_OUT_INIT_LOW);
		}
	}
}

static int aml_card_parse_of(struct device_node *node,
				     struct aml_card_data *priv)
{
	struct device *dev = aml_priv_to_dev(priv);
	struct device_node *dai_link;
	int ret;

	if (!node)
		return -EINVAL;

	dai_link = of_get_child_by_name(node, PREFIX "dai-link");

	/* The off-codec widgets */
	if (of_property_read_bool(node, PREFIX "widgets")) {
		ret = snd_soc_of_parse_audio_simple_widgets(&priv->snd_card,
					PREFIX "widgets");
		if (ret)
			goto card_parse_end;
	}

	/* DAPM routes */
	if (of_property_read_bool(node, PREFIX "routing")) {
		ret = snd_soc_of_parse_audio_routing(&priv->snd_card,
					PREFIX "routing");
		if (ret)
			goto card_parse_end;
	}

	/* Factor to mclk, used in hw_params() */
	of_property_read_u32(node, PREFIX "mclk-fs", &priv->mclk_fs);

	/* Single/Muti DAI link(s) & New style of DT node */
	if (dai_link) {
		struct device_node *np = NULL;
		int i = 0;

		for_each_child_of_node(node, np) {
			dev_dbg(dev, "\tlink %d:\n", i);
			ret = aml_card_dai_link_of(np, priv,
							   i, false);
			if (ret < 0) {
				dev_err(dev, "parse dai_link-%d fail\n", i);
				of_node_put(np);
				goto card_parse_end;
			}
			i++;
		}
	} else {
		/* For single DAI link & old style of DT node */
		ret = aml_card_dai_link_of(node, priv, 0, true);
		if (ret < 0)
			goto card_parse_end;
	}

	ret = aml_card_parse_card_name(&priv->snd_card, PREFIX);
	if (ret < 0)
		goto card_parse_end;

	ret = aml_card_parse_aux_devs(node, priv);

card_parse_end:
	of_node_put(dai_link);

	return ret;
}

static const struct of_device_id auge_of_match[] = {
	{
		.compatible = "amlogic, auge-sound-card",
	},
	{}
};
MODULE_DEVICE_TABLE(of, auge_of_match);

static int card_suspend_pre(struct snd_soc_card *card)
{
	struct aml_card_data *priv = snd_soc_card_get_drvdata(card);

	priv->av_mute_enable = 1;
	priv->spk_mute_enable = 1;
	aml_card_gpio(priv);
	pr_info("it is card_pre_suspend\n");
	return 0;
}

static int card_resume_post(struct snd_soc_card *card)
{
	struct aml_card_data *priv = snd_soc_card_get_drvdata(card);

	priv->av_mute_enable = 0;
	priv->spk_mute_enable = 0;
	aml_card_gpio(priv);
	pr_info("it is card_post_resume\n");
	return 0;

}

static irqreturn_t aml_audio_exception64_isr(int irq, void *dev_id)
{
	unsigned int intrpt_status0, intrpt_status1;

	intrpt_status0 = audiobus_read(EE_AUDIO_EXCEPTION_IRQ_STS0);
	intrpt_status1 = audiobus_read(EE_AUDIO_EXCEPTION_IRQ_STS1);

	/* clear irq bits immediately */
	audiobus_write(EE_AUDIO_EXCEPTION_IRQ_CLR0, intrpt_status0);
	audiobus_write(EE_AUDIO_EXCEPTION_IRQ_CLR1, intrpt_status1);

	pr_debug("0 - 31 exception status is 0x%x\n", intrpt_status0);
	pr_debug("32 - 63 exception status is 0x%x\n", intrpt_status1);

	/* TODO handle exception */

	return IRQ_HANDLED;
}

static int register_audio_exception64_isr(int irq_exception64)
{
	int ret = 0;

	/* open irq mask, default is close
	 * audiobus_write(EE_AUDIO_EXCEPTION_IRQ_MASK0, 0xfffdff3f);
	 * audiobus_write(EE_AUDIO_EXCEPTION_IRQ_MASK1, 0xffc3777f);

	 * set threshold value
	 * audiobus_write(EE_AUDIO_ARB_CTRL1, 0xffff);
	 * audiobus_write(EE_AUDIO_SPDIFIN_CTRL7, 0xffff);
	 */

	ret = request_irq(irq_exception64,
			  aml_audio_exception64_isr,
			  0,
			  "audio_exception64",
			  NULL);

	if (ret)
		pr_err("failed claim irq_exception64 %u, ret: %d\n", irq_exception64, ret);

	return ret;
}

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void aml_card_early_suspend(struct early_suspend *h)
{
	struct platform_device *pdev = h->param;

	pr_info("entry %s\n", __func__);
	if (pdev) {
		struct snd_soc_card *card = platform_get_drvdata(pdev);
		struct aml_card_data *priv = snd_soc_card_get_drvdata(card);

		if (!priv->spk_mute_flag) {
			int gpio = priv->spk_mute_gpio;
			bool active_low = priv->spk_mute_active_low;
			bool value = active_low ? false : true;

			if (gpio_is_valid(gpio))
				gpio_set_value(gpio, value);
		}

		if (!IS_ERR(priv->avout_mute_desc)) {
			gpiod_direction_output(priv->avout_mute_desc,
					GPIOF_OUT_INIT_LOW);
				pr_info("%s, av out status: %s\n",
					__func__,
					gpiod_get_value(priv->avout_mute_desc) ?
					"high" : "low");
		}
	}
}

static void aml_card_late_resume(struct early_suspend *h)
{
	struct platform_device *pdev = h->param;

	pr_info("entry %s\n", __func__);
	if (pdev) {
		struct snd_soc_card *card = platform_get_drvdata(pdev);
		struct aml_card_data *priv = snd_soc_card_get_drvdata(card);

		if (!priv->spk_mute_flag) {
			int gpio = priv->spk_mute_gpio;
			bool active_low = priv->spk_mute_active_low;
			bool value = active_low ? true : false;

			if (gpio_is_valid(gpio))
				gpio_set_value(gpio, value);
		}

		if (!IS_ERR(priv->avout_mute_desc)) {
			gpiod_direction_output(priv->avout_mute_desc,
					GPIOF_OUT_INIT_HIGH);
				pr_info("%s, av out status: %s\n",
					__func__,
					gpiod_get_value(priv->avout_mute_desc) ?
					"high" : "low");
		}
	}
}

static struct early_suspend card_early_suspend_handler = {
	.suspend = aml_card_early_suspend,
	.resume  = aml_card_late_resume,
};
#endif

static int aml_card_probe(struct platform_device *pdev)
{
	struct aml_card_data *priv;
	struct snd_soc_dai_link *dai_link;
	struct aml_dai_props *dai_props;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	int num, ret, i;

	/* Get the number of DAI links */
	if (np && of_get_child_by_name(np, PREFIX "dai-link"))
		num = of_get_child_count(np);
	else
		num = 1;

	/* Allocate the private data and the DAI link array */
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dai_props = devm_kzalloc(dev, sizeof(*dai_props) * num, GFP_KERNEL);
	dai_link  = devm_kzalloc(dev, sizeof(*dai_link)  * num, GFP_KERNEL);
	if (!dai_props || !dai_link)
		return -ENOMEM;

	priv->dai_props			= dai_props;
	priv->dai_link			= dai_link;

	for (i = 0; i < num; i++) {
		dai_link[i].cpus		= &dai_props[i].cpus;
		dai_link[i].num_cpus		= 1;
		dai_link[i].codecs		= &dai_props[i].codecs;
		dai_link[i].num_codecs		= 1;
		dai_link[i].platforms		= &dai_props[i].platforms;
		dai_link[i].num_platforms	= 1;
	}

	/* Init snd_soc_card */
	priv->snd_card.owner		= THIS_MODULE;
	priv->snd_card.dev		= dev;
	priv->snd_card.dai_link		= priv->dai_link;
	priv->snd_card.num_links	= num;
	priv->snd_card.suspend_pre	= card_suspend_pre;
	priv->snd_card.resume_post	= card_resume_post;
	priv->inskew_tdm_index = -1;
	priv->audio_inskew = -1;
	priv->binv_tdm_index = -1;
	priv->audio_binv = -1;
	if (np && of_device_is_available(np)) {
		ret = aml_card_parse_of(np, priv);
		if (ret < 0) {
			dev_err(dev, "%s, aml_card_parse_of error %d %s\n",
				__func__,
				ret,
				(ret == -EPROBE_DEFER) ? "PROBE RETRY" : "");
			goto err;
		}

	} else {
		struct aml_card_info *cinfo;

		cinfo = dev->platform_data;
		if (!cinfo) {
			dev_err(dev, "no info for asoc-aml-card\n");
			return -EINVAL;
		}

		if (!cinfo->name ||
		    !cinfo->codec_dai.name ||
		    !cinfo->codec ||
		    !cinfo->platform ||
		    !cinfo->cpu_dai.name) {
			dev_err(dev, "insufficient aml_card_info settings\n");
			return -EINVAL;
		}

		priv->snd_card.name	=
				(cinfo->card) ? cinfo->card : cinfo->name;
		dai_link->name		= cinfo->name;
		dai_link->stream_name	= cinfo->name;
		dai_link->platforms->name	= cinfo->platform;
		dai_link->codecs->name	= cinfo->codec;
		dai_link->cpus->dai_name   = cinfo->cpu_dai.name;
		dai_link->codecs->dai_name = cinfo->codec_dai.name;
		dai_link->dai_fmt	= cinfo->daifmt;
		dai_link->init		= aml_card_dai_init;
		memcpy(&priv->dai_props->cpu_dai, &cinfo->cpu_dai,
					sizeof(priv->dai_props->cpu_dai));
		memcpy(&priv->dai_props->codec_dai, &cinfo->codec_dai,
					sizeof(priv->dai_props->codec_dai));
	}

	priv->irq_exception64 =
		platform_get_irq_byname(pdev, "audio_exception64");
	if (priv->irq_exception64 > 0)
		register_audio_exception64_isr(priv->irq_exception64);

	platform_set_drvdata(pdev, priv);
	snd_soc_card_set_drvdata(&priv->snd_card, priv);

	ret = devm_snd_soc_register_card(&pdev->dev, &priv->snd_card);
	if (ret < 0) {
		dev_err(dev, "failed to register sound card\n");
		goto err;
	}

	/* Add controls */
	ret = aml_card_add_controls(&priv->snd_card);
	if (ret < 0) {
		dev_err(dev, "failed to register mixer kcontrols\n");
		goto err;
	}

	card_add_effects_init(&priv->snd_card);

	if (priv->hp_det_enable == 1 || priv->mic_det_enable == 1) {
		priv->hp_detect_flag = -1;
		priv->hp_last_state = -1;
		priv->mic_detect_flag = -1;
		priv->micphone_last_state = -1;
		audio_jack_detect(priv);
		audio_extcon_register(priv, dev);
	}

	snd_soc_add_card_controls(&priv->snd_card, snd_user_controls,
				  ARRAY_SIZE(snd_user_controls));

	priv->av_mute_enable = 0;
	priv->spk_mute_enable = 0;
	INIT_WORK(&priv->init_work, aml_init_work);
	schedule_work(&priv->init_work);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	card_early_suspend_handler.param = pdev;
	register_early_suspend(&card_early_suspend_handler);
#endif

	return 0;
err:
	pr_err("%s error ret:%d\n", __func__, ret);
	aml_card_clean_reference(&priv->snd_card);

	return ret;
}

static int aml_card_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct aml_card_data *priv = snd_soc_card_get_drvdata(card);

	aml_card_remove_jack(&priv->hp_jack);
	aml_card_remove_jack(&priv->mic_jack);
	jack_audio_stop_timer(priv);

	if (priv->irq_exception64 > 0)
		free_irq(priv->irq_exception64, NULL);

	return aml_card_clean_reference(card);
}

static void aml_card_platform_shutdown(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct aml_card_data *priv = snd_soc_card_get_drvdata(card);

	priv->av_mute_enable = 1;
	priv->spk_mute_enable = 1;
	aml_card_parse_gpios(pdev->dev.of_node, priv);
}

static struct platform_driver aml_card = {
	.driver = {
		.name = "asoc-aml-card",
		.pm = &snd_soc_pm_ops,
		.of_match_table = auge_of_match,
	},
	.probe = aml_card_probe,
	.remove = aml_card_remove,
	.shutdown = aml_card_platform_shutdown,
};

int __init aml_card_init(void)
{
	return platform_driver_register(&aml_card);
}

void __exit aml_card_exit(void)
{
	platform_driver_unregister(&aml_card);
}

#ifndef MODULE
module_init(aml_card_init);
module_exit(aml_card_exit);
MODULE_ALIAS("platform:asoc-aml-card");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ASoC aml Sound Card");
MODULE_AUTHOR("AMLogic, Inc.");
#endif
