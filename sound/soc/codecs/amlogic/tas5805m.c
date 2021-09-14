// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/regmap.h>

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/pcm_params.h>
#include <linux/amlogic/aml_gpio_consumer.h>

#include "tas5805m.h"
#include "tas5805m_woofer.h"
#include "tas5805m_tweeter.h"

#define TAS5805M_DRV_NAME    "tas5805m"

#define TAS5805M_RATES	     (SNDRV_PCM_RATE_8000 | \
		       SNDRV_PCM_RATE_11025 | \
		       SNDRV_PCM_RATE_16000 | \
		       SNDRV_PCM_RATE_22050 | \
		       SNDRV_PCM_RATE_32000 | \
		       SNDRV_PCM_RATE_44100 | \
		       SNDRV_PCM_RATE_48000 | \
		       SNDRV_PCM_RATE_96000)
#define TAS5805M_FORMATS     (SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE)

#define TAS5805M_BIT_MSK	(0x3)
#define TAS5805M_BCLK_INV	(0x20)
#define TAS5805M_DATA_FMT	(0x30)

#define TAS5805M_REG_00      (0x00)
#define TAS5805M_REG_02      (0x02)
#define TAS5805M_REG_03      (0x03)
#define TAS5805M_REG_08      (0x08)
#define TAS5805M_REG_24      (0x24)
#define TAS5805M_REG_25      (0x25)
#define TAS5805M_REG_26      (0x26)
#define TAS5805M_REG_27      (0x27)
#define TAS5805M_REG_28      (0x28)
#define TAS5805M_REG_29      (0x29)
#define TAS5805M_REG_2A      (0x2a)
#define TAS5805M_REG_2B      (0x2b)
#define TAS5805M_REG_31      (0x31)
#define TAS5805M_REG_33      (0x33)
#define TAS5805M_REG_35      (0x35)
#define TAS5805M_REG_70      (0x70)
#define TAS5805M_REG_71      (0x71)
#define TAS5805M_REG_72      (0x72)
#define TAS5805M_REG_78      (0x78)
#define TAS5805M_REG_4C      (0x4c)
#define TAS5805M_REG_54      (0x54)
#define TAS5805M_REG_6B      (0x6b)
#define TAS5805M_REG_6C      (0x6c)
#define TAS5805M_REG_61      (0x61)
#define TAS5805M_REG_60      (0x60)

#define TAS5805M_REG_7F      (0x7f)
// Special registers for engineering mode
#define TAS5805M_REG_7D      (0x7d)
#define TAS5805M_REG_7E      (0x7e)
#define TAS5805M_PAGE_02     (0x02)
#define TAS5805M_PAGE_11     (0x11)
#define TAS5805M_PAGE_FF     (0xff)

#define TAS5805M_MAX_REG     TAS5805M_REG_7F
#define TAS5805M_PAGE_00     (0x00)
#define TAS5805M_PAGE_2A     (0x2a)

#define TAS5805M_BOOK_00     (0x00)
#define TAS5805M_BOOK_8C     (0x8c)

#define TAS5805M_VOLUME_MAX  (158)
#define TAS5805M_VOLUME_MIN  (0)

#define TAS5805M_DEVICE_DEEPSLEEP_MODE  (0x00)
#define TAS5805M_DEVICE_SLEEP_MODE      (0x01)
#define TAS5805M_DEVICE_HIZ_MODE        (0x02)
#define TAS5805M_DEVICE_PLAY_MODE       (0x03)
#define TAS5805M_DEVICE_BD_MOD_MODE     (0x00)
#define TAS5805M_DEVICE_HYBRID_MOD_MODE (0x02)
#define TAS5805M_SPREAD_SPECTRUM_MAX  (7)
#define TAS5805M_SPREAD_SPECTRUM_MIN  (0)
#define TAS5805M_STATE_MASK  (0x03)
#define TAS5805M_CLEAR_FAULT (0x80)
#define	TAS5805M_DIE_A1 (0x1)
#define	TAS5805M_DIE_B0 (0x3)
#define TAS5805M_DAMP_PBTL_MASK (0x04)

static int master_vol = 0x6e;   // 0dB
static int digital_gain = 0x30; // 0dB
static int analog_gain = 0x0a;  // -5dB

enum speaker_config {
	STEREO = 0,
	MONO,
	WOOFER,
	TWEETER,
};

struct tas5805m_info {
	int info_id;
};

static const u32 tas5805m_volume[] = {
	0x0000001B,    //0, -110dB
	0x0000001E,    //1, -109dB
	0x00000021,    //2, -108dB
	0x00000025,    //3, -107dB
	0x0000002A,    //4, -106dB
	0x0000002F,    //5, -105dB
	0x00000035,    //6, -104dB
	0x0000003B,    //7, -103dB
	0x00000043,    //8, -102dB
	0x0000004B,    //9, -101dB
	0x00000054,    //10, -100dB
	0x0000005E,    //11, -99dB
	0x0000006A,    //12, -98dB
	0x00000076,    //13, -97dB
	0x00000085,    //14, -96dB
	0x00000095,    //15, -95dB
	0x000000A7,    //16, -94dB
	0x000000BC,    //17, -93dB
	0x000000D3,    //18, -92dB
	0x000000EC,    //19, -91dB
	0x00000109,    //20, -90dB
	0x0000012A,    //21, -89dB
	0x0000014E,    //22, -88dB
	0x00000177,    //23, -87dB
	0x000001A4,    //24, -86dB
	0x000001D8,    //25, -85dB
	0x00000211,    //26, -84dB
	0x00000252,    //27, -83dB
	0x0000029A,    //28, -82dB
	0x000002EC,    //29, -81dB
	0x00000347,    //30, -80dB
	0x000003AD,    //31, -79dB
	0x00000420,    //32, -78dB
	0x000004A1,    //33, -77dB
	0x00000532,    //34, -76dB
	0x000005D4,    //35, -75dB
	0x0000068A,    //36, -74dB
	0x00000756,    //37, -73dB
	0x0000083B,    //38, -72dB
	0x0000093C,    //39, -71dB
	0x00000A5D,    //40, -70dB
	0x00000BA0,    //41, -69dB
	0x00000D0C,    //42, -68dB
	0x00000EA3,    //43, -67dB
	0x0000106C,    //44, -66dB
	0x0000126D,    //45, -65dB
	0x000014AD,    //46, -64dB
	0x00001733,    //47, -63dB
	0x00001A07,    //48, -62dB
	0x00001D34,    //49, -61dB
	0x000020C5,    //50, -60dB
	0x000024C4,    //51, -59dB
	0x00002941,    //52, -58dB
	0x00002E49,    //53, -57dB
	0x000033EF,    //54, -56dB
	0x00003A45,    //55, -55dB
	0x00004161,    //56, -54dB
	0x0000495C,    //57, -53dB
	0x0000524F,    //58, -52dB
	0x00005C5A,    //59, -51dB
	0x0000679F,    //60, -50dB
	0x00007444,    //61, -49dB
	0x00008274,    //62, -48dB
	0x0000925F,    //63, -47dB
	0x0000A43B,    //64, -46dB
	0x0000B845,    //65, -45dB
	0x0000CEC1,    //66, -44dB
	0x0000E7FB,    //67, -43dB
	0x00010449,    //68, -42dB
	0x0001240C,    //69, -41dB
	0x000147AE,    //70, -40dB
	0x00016FAA,    //71, -39dB
	0x00019C86,    //72, -38dB
	0x0001CEDC,    //73, -37dB
	0x00020756,    //74, -36dB
	0x000246B5,    //75, -35dB
	0x00028DCF,    //76, -34dB
	0x0002DD96,    //77, -33dB
	0x00033718,    //78, -32dB
	0x00039B87,    //79, -31dB
	0x00040C37,    //80, -30dB
	0x00048AA7,    //81, -29dB
	0x00051884,    //82, -28dB
	0x0005B7B1,    //83, -27dB
	0x00066A4A,    //84, -26dB
	0x000732AE,    //85, -25dB
	0x00081385,    //86, -24dB
	0x00090FCC,    //87, -23dB
	0x000A2ADB,    //88, -22dB
	0x000B6873,    //89, -21dB
	0x000CCCCD,    //90, -20dB
	0x000E5CA1,    //91, -19dB
	0x00101D3F,    //92, -18dB
	0x0012149A,    //93, -17dB
	0x00144961,    //94, -16dB
	0x0016C311,    //95, -15dB
	0x00198A13,    //96, -14dB
	0x001CA7D7,    //97, -13dB
	0x002026F3,    //98, -12dB
	0x00241347,    //99, -11dB
	0x00287A27,    //100, -10dB
	0x002D6A86,    //101, -9dB
	0x0032F52D,    //102, -8dB
	0x00392CEE,    //103, -7dB
	0x004026E7,    //104, -6dB
	0x0047FACD,    //105, -5dB
	0x0050C336,    //106, -4dB
	0x005A9DF8,    //107, -3dB
	0x0065AC8C,    //108, -2dB
	0x00721483,    //109, -1dB
	0x00800000,    //110, 0dB
	0x008F9E4D,    //111, 1dB
	0x00A12478,    //112, 2dB
	0x00B4CE08,    //113, 3dB
	0x00CADDC8,    //114, 4dB
	0x00E39EA9,    //115, 5dB
	0x00FF64C1,    //116, 6dB
	0x011E8E6A,    //117, 7dB
	0x0141857F,    //118, 8dB
	0x0168C0C6,    //119, 9dB
	0x0194C584,    //120, 10dB
	0x01C62940,    //121, 11dB
	0x01FD93C2,    //122, 12dB
	0x023BC148,    //123, 13dB
	0x02818508,    //124, 14dB
	0x02CFCC01,    //125, 15dB
	0x0327A01A,    //126, 16dB
	0x038A2BAD,    //127, 17dB
	0x03F8BD7A,    //128, 18dB
	0x0474CD1B,    //129, 19dB
	0x05000000,    //130, 20dB
	0x059C2F02,    //131, 21dB
	0x064B6CAE,    //132, 22dB
	0x07100C4D,    //133, 23dB
	0x07ECA9CD,    //134, 24dB
	0x08E43299,    //135, 25dB
	0x09F9EF8E,    //136, 26dB
	0x0B319025,    //137, 27dB
	0x0C8F36F2,    //138, 28dB
	0x0E1787B8,    //139, 29dB
	0x0FCFB725,    //140, 30dB
	0x11BD9C84,    //141, 31dB
	0x13E7C594,    //142, 32dB
	0x16558CCB,    //143, 33dB
	0x190F3254,    //144, 34dB
	0x1C1DF80E,    //145, 35dB
	0x1F8C4107,    //146, 36dB
	0x2365B4BF,    //147, 37dB
	0x27B766C2,    //148, 38dB
	0x2C900313,    //149, 39dB
	0x32000000,    //150, 40dB
	0x3819D612,    //151, 41dB
	0x3EF23ECA,    //152, 42dB
	0x46A07B07,    //153, 43dB
	0x4F3EA203,    //154, 44dB
	0x58E9F9F9,    //155, 45dB
	0x63C35B8E,    //156, 46dB
	0x6FEFA16D,    //157, 47dB
	0x7D982575,    //158, 48dB
};

#define TAS5805_EQPARAM_LENGTH 610
#define TAS5805_EQ_LENGTH 245
#define FILTER_PARAM_BYTE 244
static  int m_eq_tab[TAS5805_EQPARAM_LENGTH][2];
#define TAS5805_DRC_PARAM_LENGTH 29
#define TAS5805_DRC_PARAM_COUNT  58
static  int m_drc_tab[TAS5805_DRC_PARAM_LENGTH][2];
struct tas5805m_priv {
	struct regmap *regmap;
	struct mutex lock;/*mutex*/
	struct tas5805m_platform_data *pdata;
	struct device *dev;
	int vol;
	int mute;
	int enable_lpm_mode;
	int use_sleep_mode_for_lpm;
	int die_rev;
	struct snd_soc_component *component;
};

static bool tas5805m_reg_is_volatile(struct device *dev, unsigned int reg)
{
	return reg >= TAS5805M_REG_70 && reg < TAS5805M_MAX_REG;
}

static const struct regmap_config tas5805m_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = tas5805m_reg_is_volatile
};

static int tas5805m_get_die_revision(struct snd_soc_component *component)
{
	unsigned int die_rev;

	snd_soc_component_write(component, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_component_write(component, TAS5805M_REG_7F, TAS5805M_BOOK_00);
	snd_soc_component_write(component, TAS5805M_REG_7D, TAS5805M_PAGE_11);
	snd_soc_component_write(component, TAS5805M_REG_7E, TAS5805M_PAGE_FF);
	snd_soc_component_write(component, TAS5805M_REG_00, TAS5805M_PAGE_02);
	die_rev = snd_soc_component_read32(component, TAS5805M_REG_08) & 0x3;
	snd_soc_component_write(component, TAS5805M_REG_7E, TAS5805M_REG_00);
	snd_soc_component_write(component, TAS5805M_REG_7D, TAS5805M_REG_00);
	snd_soc_component_write(component, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_component_write(component, TAS5805M_REG_7F, TAS5805M_BOOK_00);
	snd_soc_component_write(component, TAS5805M_REG_00, TAS5805M_PAGE_00);
	pr_info("%s(): 5805m die %x detected\n", __func__, die_rev);

	return die_rev;
}

static int tas5805m_vol_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access =
	    (SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE);
	uinfo->count = 1;

	uinfo->value.integer.min = TAS5805M_VOLUME_MIN;
	uinfo->value.integer.max = TAS5805M_VOLUME_MAX;
	uinfo->value.integer.step = 1;

	return 0;
}

static int tas5805m_mute_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access =
	    (SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE);
	uinfo->count = 1;

	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	uinfo->value.integer.step = 1;

	return 0;
}

static int tas5805m_lpm_mode_locked_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
					snd_soc_kcontrol_component(kcontrol);
	struct tas5805m_priv *tas5805m =
	snd_soc_component_get_drvdata(component);

	mutex_lock(&tas5805m->lock);
	ucontrol->value.integer.value[0] = tas5805m->enable_lpm_mode;
	mutex_unlock(&tas5805m->lock);
	pr_info("%s(): LPM mode is %s", __func__,
	tas5805m->enable_lpm_mode ? "Enabled" : "Disabled");

	return 0;
}

static int tas5805m_vol_locked_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	snd_soc_kcontrol_component(kcontrol);
	struct tas5805m_priv *tas5805m =
	snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = tas5805m->vol;

	return 0;
}

static inline int get_volume_index(int vol)
{
	int index;

	index = vol;

	if (index < TAS5805M_VOLUME_MIN)
		index = TAS5805M_VOLUME_MIN;

	if (index > TAS5805M_VOLUME_MAX)
		index = TAS5805M_VOLUME_MAX;

	return index;
}

static void tas5805m_set_volume(struct snd_soc_component *component, int vol)
{
	unsigned int index;
	u32 volume_hex;
	u8 byte4;
	u8 byte3;
	u8 byte2;
	u8 byte1;

	index = get_volume_index(vol);
	volume_hex = tas5805m_volume[index];

	byte4 = ((volume_hex >> 24) & 0xFF);
	byte3 = ((volume_hex >> 16) & 0xFF);
	byte2 = ((volume_hex >> 8) & 0xFF);
	byte1 = ((volume_hex >> 0) & 0xFF);

	//w 58 00 00
	snd_soc_component_write(component, TAS5805M_REG_00, TAS5805M_PAGE_00);
	//w 58 7f 8c
	snd_soc_component_write(component, TAS5805M_REG_7F, TAS5805M_BOOK_8C);
	//w 58 00 2a
	snd_soc_component_write(component, TAS5805M_REG_00, TAS5805M_PAGE_2A);
	//w 58 24 xx
	snd_soc_component_write(component, TAS5805M_REG_24, byte4);
	snd_soc_component_write(component, TAS5805M_REG_25, byte3);
	snd_soc_component_write(component, TAS5805M_REG_26, byte2);
	snd_soc_component_write(component, TAS5805M_REG_27, byte1);
	//w 58 28 xx
	snd_soc_component_write(component, TAS5805M_REG_28, byte4);
	snd_soc_component_write(component, TAS5805M_REG_29, byte3);
	snd_soc_component_write(component, TAS5805M_REG_2A, byte2);
	snd_soc_component_write(component, TAS5805M_REG_2B, byte1);
}

static int tas5805m_vol_locked_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	snd_soc_kcontrol_component(kcontrol);
	struct tas5805m_priv *tas5805m =
	snd_soc_component_get_drvdata(component);

	tas5805m->vol = ucontrol->value.integer.value[0];
	tas5805m_set_volume(component, tas5805m->vol);

	return 0;
}

static int tas5805m_lpm_mode_locked_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	snd_soc_kcontrol_component(kcontrol);
	struct tas5805m_priv *tas5805m =
	snd_soc_component_get_drvdata(component);

	int enable_lpm_mode = ucontrol->value.integer.value[0];

	mutex_lock(&tas5805m->lock);

	tas5805m->enable_lpm_mode = enable_lpm_mode;

	mutex_unlock(&tas5805m->lock);

	pr_info("%s(): LPM mode is %s", __func__,
	enable_lpm_mode ? "Enabled" : "Disabled");

	return 0;
}

static int tas5805m_mute(struct snd_soc_component *component, int mute)
{
	u8 reg03_value = 0;
	u8 reg35_value = 0;

	if (mute) {
		//mute both left & right channels
		reg03_value = 0x0b;
		reg35_value = 0x00;
	} else {
		//unmute
		reg03_value = 0x03;
		reg35_value = 0x11;
	}

	snd_soc_component_write(component, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_component_write(component, TAS5805M_REG_7F, TAS5805M_BOOK_00);
	snd_soc_component_write(component, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_component_write(component, TAS5805M_REG_03, reg03_value);
	snd_soc_component_write(component, TAS5805M_REG_35, reg35_value);

	return 0;
}

static int tas5805m_mute_locked_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	snd_soc_kcontrol_component(kcontrol);
	struct tas5805m_priv *tas5805m =
	snd_soc_component_get_drvdata(component);

	tas5805m->mute = ucontrol->value.integer.value[0];
	tas5805m_mute(component, tas5805m->mute);

	return 0;
}

static int tas5805m_mute_locked_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	snd_soc_kcontrol_component(kcontrol);
	struct tas5805m_priv *tas5805m =
	snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = tas5805m->mute;
	return 0;
}

static const char *const spread_spectrum_texts[] = {
	"24k_5%",
	"24k_10%",
	"24k_20%",
	"24k_25%",
	"48k_5%",
	"48k_10%",
	"48k_20%",
	"48k_25%",
};

static const struct soc_enum spread_spectrum_enum =
	SOC_ENUM_SINGLE(TAS5805M_REG_6C,
		0,
		ARRAY_SIZE(spread_spectrum_texts),
		spread_spectrum_texts);

static int tas5805m_spread_spectrum_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	snd_soc_kcontrol_component(kcontrol);
	u8 val = ucontrol->value.integer.value[0];

	snd_soc_component_update_bits(component, TAS5805M_REG_6C, 0xf, val);
	return 0;
}

static int tas5805m_spread_spectrum_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	snd_soc_kcontrol_component(kcontrol);
	u8 val;

	val = snd_soc_component_read32(component, TAS5805M_REG_6C);
	ucontrol->value.integer.value[0] = (val & 0xf);

	return 0;
}

static int tas5805_set_spread_spectrum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	snd_soc_kcontrol_component(kcontrol);
	u8 val;

	val = (ucontrol->value.integer.value[0]) ? 0x3 : 0;
	snd_soc_component_update_bits(component, TAS5805M_REG_6B, 0x3, val);

	return 0;
}

static int tas5805_get_spread_spectrum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	snd_soc_kcontrol_component(kcontrol);
	u8 val1, val2;

	val1 = snd_soc_component_read32(component, TAS5805M_REG_6B);
	val2 = ((val1 & 0x3) == 0x3) ? 1 : 0;
	ucontrol->value.integer.value[0] = val2;

	return 0;
}

static int tas5805_set_EQ_enum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tas5805_get_EQ_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tas5805_set_DRC_enum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tas5805_get_DRC_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tas5805_set_DRC_param(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	snd_soc_kcontrol_component(kcontrol);
	void *data;
	char tmp_string[TAS5805_DRC_PARAM_COUNT];
	char *p_string = &tmp_string[0];
	u8 *val;
	unsigned int i = 0;

	data = kmemdup(ucontrol->value.bytes.data,
		TAS5805_DRC_PARAM_COUNT, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	val = (u8 *)data;
	memcpy(p_string, val, TAS5805_DRC_PARAM_COUNT);

	for (i = 0; i < TAS5805_DRC_PARAM_COUNT / 2; i++) {
		m_drc_tab[i][0] = tmp_string[2 * i];
		m_drc_tab[i][1] = tmp_string[2 * i + 1];
	}

	for (i = 0; i < TAS5805_DRC_PARAM_LENGTH; i++)
		snd_soc_component_write(component,
		m_drc_tab[i][0], m_drc_tab[i][1]);

	kfree(data);
	return 0;
}

static int tas5805_get_DRC_param(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int tas5805_set_EQ_param(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	snd_soc_kcontrol_component(kcontrol);
	void *data;
	char tmp_string[TAS5805_EQ_LENGTH];
	char *p_string = &tmp_string[0];
	u8 *val;
	int band_id;
	unsigned int i = 0, j = 0;

	data = kmemdup(ucontrol->value.bytes.data,
		TAS5805_EQ_LENGTH, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	val = (u8 *)data;
	memcpy(p_string, val, TAS5805_EQ_LENGTH);
	band_id = tmp_string[0];
	for (j = 0, i = band_id * FILTER_PARAM_BYTE / 2;
			j < FILTER_PARAM_BYTE / 2; i++, j++) {
		m_eq_tab[i][0] = tmp_string[2 * j + 1];
		m_eq_tab[i][1] = tmp_string[2 * j + 2];
	}
	if (band_id == 4) {
		for (i = 0; i < TAS5805_EQPARAM_LENGTH; i++)
			snd_soc_component_write(component,
			m_eq_tab[i][0], m_eq_tab[i][1]);
	}
	kfree(data);
	return 0;
}

static int tas5805_get_EQ_param(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static void tas5805m_configure_deepsleep_mode
				(struct snd_soc_component *component,
				int sleep)
{
	struct tas5805m_priv *tas5805m =
	snd_soc_component_get_drvdata(component);
	int sleep_mode;

	snd_soc_component_write(component, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_component_write(component, TAS5805M_REG_7F, TAS5805M_BOOK_00);

	mutex_lock(&tas5805m->lock);
	sleep_mode = tas5805m->use_sleep_mode_for_lpm;
	mutex_unlock(&tas5805m->lock);

	pr_info("%s(): 5805m triggered %s mode\n", __func__,
	sleep ? (sleep_mode ? "sleep" : "deep sleep") : "play");

	if (!sleep) {
		snd_soc_component_write(component,
		TAS5805M_REG_02, TAS5805M_DEVICE_BD_MOD_MODE);
		snd_soc_component_write(component,
		TAS5805M_REG_03, TAS5805M_DEVICE_HIZ_MODE);
		usleep_range(2000, 2200);
		snd_soc_component_write(component,
		TAS5805M_REG_02, TAS5805M_DEVICE_HYBRID_MOD_MODE);
		if (tas5805m->pdata->pbtl_mode) {
			snd_soc_component_update_bits(component,
				TAS5805M_REG_02, TAS5805M_DAMP_PBTL_MASK, 0x07);
		}
		usleep_range(15000, 15200);
		snd_soc_component_write(component,
		TAS5805M_REG_03, TAS5805M_DEVICE_PLAY_MODE);
	} else {
		snd_soc_component_write(component,
		TAS5805M_REG_03, TAS5805M_DEVICE_HIZ_MODE);
		snd_soc_component_write(component,
		TAS5805M_REG_03, sleep_mode);
	}
}

static int get_lpm_mode_status_locked(struct snd_soc_component *component)
{
	int enable_lpm_mode = 0; /* By default disable lpm mode */
	struct tas5805m_priv *tas5805m =
	snd_soc_component_get_drvdata(component);

	mutex_lock(&tas5805m->lock);
	enable_lpm_mode = tas5805m->enable_lpm_mode;
	mutex_unlock(&tas5805m->lock);

	return enable_lpm_mode;
}

static const struct snd_kcontrol_new tas5805m_control[] = {
	{
	 .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	 .name = "Master Volume",
	 .info = tas5805m_vol_info,
	 .get = tas5805m_vol_locked_get,
	 .put = tas5805m_vol_locked_put,
	 },
	{
	 .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	 .name = "Maser Volume Mute",
	 .info = tas5805m_mute_info,
	 .get = tas5805m_mute_locked_get,
	 .put = tas5805m_mute_locked_put,
	},
	SOC_ENUM_EXT("Spread Spectrum",
		spread_spectrum_enum,
		tas5805m_spread_spectrum_get,
		tas5805m_spread_spectrum_put),
	SOC_SINGLE_BOOL_EXT("Spread Spectrum Enable", 0,
			   tas5805_get_spread_spectrum,
			   tas5805_set_spread_spectrum),
	SOC_SINGLE_BOOL_EXT("Set EQ Enable", 0,
			   tas5805_get_EQ_enum,
			   tas5805_set_EQ_enum),
	SOC_SINGLE_BOOL_EXT("Set DRC Enable", 0,
			   tas5805_get_DRC_enum,
			   tas5805_set_DRC_enum),
	SND_SOC_BYTES_EXT("EQ table", TAS5805_EQ_LENGTH,
			   tas5805_get_EQ_param,
			   tas5805_set_EQ_param),
	SND_SOC_BYTES_EXT("DRC table", TAS5805_DRC_PARAM_COUNT,
			   tas5805_get_DRC_param,
			   tas5805_set_DRC_param),
	/* LPM mode toggle */
	SOC_SINGLE_BOOL_EXT("TAS5805m LPM mode", 0,
			   tas5805m_lpm_mode_locked_get,
			   tas5805m_lpm_mode_locked_put),
};

static int tas5805m_set_bias_level(struct snd_soc_component *component,
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

static int tas5805m_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *codec_dai)
{
	//struct tas5805m_priv *tas5805m = snd_soc_dai_get_drvdata(codec_dai);
	//struct snd_soc_component *component = tas5805m->component;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			pr_debug("%s(), start\n", __func__);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			pr_debug("%s(), stop\n", __func__);
			break;
		}
	}
	return 0;
}

static int reset_tas5805m_GPIO(struct device *dev)
{
	struct tas5805m_priv *tas5805m =  dev_get_drvdata(dev);
	struct tas5805m_platform_data *pdata = tas5805m->pdata;
	int ret = 0;

	if (pdata->reset_pin < 0) {
		pr_warn("%s(), no reset pin\n", __func__);
		return -1;
	}

	ret = devm_gpio_request_one(dev, pdata->reset_pin,
					    GPIOF_OUT_INIT_LOW,
					    "tas5805m-reset-pin");
	if (ret < 0) {
		pr_warn("%s(), request reset-pin failed\n", __func__);
		return -1;
	}

	/* GPIOM_10 is set to low in uboot stage */
	gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
	usleep_range(5 * 1000, 6 * 1000);
	gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_HIGH);
	usleep_range(5 * 1000, 6 * 1000);
	pr_info("%s(), tas5805m sets PDN ok\n", __func__);

	return 0;
}

static int tas5805m_snd_suspend(struct snd_soc_component *component)
{
	struct tas5805m_priv *tas5805m =
	snd_soc_component_get_drvdata(component);
	struct tas5805m_platform_data *pdata = tas5805m->pdata;

	dev_info(component->dev, "tas5805m_suspend!\n");
	tas5805m_set_bias_level(component, SND_SOC_BIAS_OFF);

	if (pdata->reset_pin)
		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);
	usleep_range(9, 15);
	return 0;
}

static int tas5805m_apply_seq(struct snd_soc_component *component,
				const int (*tas5805m_seq)[2],
				size_t size_seq)
{
	int i, ret;

	for (i = 0; i < size_seq; i++) {
		ret = snd_soc_component_write(component, tas5805m_seq[i][0],
						tas5805m_seq[i][1]);
		if (ret < 0) {
			dev_err(component->dev, "i2c write failed reg: 0x%x val: 0x%x\n",
				tas5805m_seq[i][0], tas5805m_seq[i][1]);
			return ret;
		}
	}

	return 0;
}

static int tas5805m_reg_init(struct snd_soc_component *component)
{
	struct tas5805m_priv *tas5805m =
	snd_soc_component_get_drvdata(component);

	tas5805m_apply_seq(component, tas5805m_reset,
		ARRAY_SIZE(tas5805m_reset));
	msleep(20);

	switch (tas5805m->pdata->spk_config) {
	case MONO:
		tas5805m_apply_seq(component, tas5805m_init_mono,
			ARRAY_SIZE(tas5805m_init_mono));
		break;

	case WOOFER:
		tas5805m_apply_seq(component, tas5805m_init_woofer,
			ARRAY_SIZE(tas5805m_init_woofer));
		break;

	case TWEETER:
		tas5805m_apply_seq(component, tas5805m_init_tweeter,
			ARRAY_SIZE(tas5805m_init_tweeter));
		break;

	case STEREO:
	default:
		tas5805m_apply_seq(component, tas5805m_init_sequence,
			ARRAY_SIZE(tas5805m_init_sequence));
	};
	msleep(50);

	return 0;
}

static int tas5805m_snd_resume(struct snd_soc_component *component)
{
	int ret;
	struct tas5805m_priv *tas5805m =
	snd_soc_component_get_drvdata(component);
	struct tas5805m_platform_data *pdata = tas5805m->pdata;

	dev_info(component->dev, "%s!\n", __func__);
	if (pdata->reset_pin)
		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_HIGH);

	usleep_range(3 * 1000, 4 * 1000);

	ret = tas5805m_reg_init(component);
//	    regmap_register_patch(tas5805m->regmap, tas5805m_init_sequence,
//				  ARRAY_SIZE(tas5805m_init_sequence));
	if (ret != 0) {
		dev_err(component->dev,
			"Failed to initialize TAS5805M: %d\n", ret);
		goto err;
	}

	tas5805m_set_volume(component, tas5805m->vol);

	snd_soc_component_write(component, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_component_write(component, TAS5805M_REG_7F, TAS5805M_BOOK_00);
	snd_soc_component_write(component, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_component_write(component, TAS5805M_REG_4C, digital_gain);
	snd_soc_component_write(component, TAS5805M_REG_54, analog_gain);

	tas5805m_set_bias_level(component, SND_SOC_BIAS_STANDBY);

	return 0;
err:
	return ret;
}

static int tas5805m_probe(struct snd_soc_component *component)
{
	int ret;
	struct tas5805m_priv *tas5805m =
	snd_soc_component_get_drvdata(component);

	usleep_range(20 * 1000, 21 * 1000);
	ret = tas5805m_reg_init(component);
	if (ret != 0)
		goto err;

	/* set master vol */
	tas5805m_set_volume(component, tas5805m->vol);
	/* set digital gain & analog gain */
	snd_soc_component_write(component, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_component_write(component, TAS5805M_REG_7F, TAS5805M_BOOK_00);
	snd_soc_component_write(component, TAS5805M_REG_00, TAS5805M_PAGE_00);
	snd_soc_component_write(component, TAS5805M_REG_4C, digital_gain);
	snd_soc_component_write(component, TAS5805M_REG_54, analog_gain);
	/*set spread spectrum default to enable*/
	snd_soc_component_update_bits(component, TAS5805M_REG_6B, 0x3, 0x3);
	snd_soc_component_update_bits(component, TAS5805M_REG_6C, 0xf, 1);
	/*enable FAULT PIN*/
	snd_soc_component_write(component, TAS5805M_REG_61, 0xb);
	snd_soc_component_write(component, TAS5805M_REG_60, 0x1);

	snd_soc_add_component_controls(component, tas5805m_control,
			ARRAY_SIZE(tas5805m_control));
	/*  By default use deep sleep for LPM */
	tas5805m->use_sleep_mode_for_lpm = TAS5805M_DEVICE_DEEPSLEEP_MODE;
	/* get DIE version */
	tas5805m->die_rev = tas5805m_get_die_revision(component);
	/* By default enable LPM mode */
	tas5805m->enable_lpm_mode = 1;
	tas5805m->component = component;
	mutex_init(&tas5805m->lock);
	return 0;

err:
	return ret;
}

static void tas5805m_remove(struct snd_soc_component *component)
{
	struct tas5805m_priv *tas5805m =
	snd_soc_component_get_drvdata(component);
	struct tas5805m_platform_data *pdata = tas5805m->pdata;

	if (pdata->reset_pin)
		gpio_direction_output(pdata->reset_pin, GPIOF_OUT_INIT_LOW);

	usleep_range(9, 15);
}

static const struct snd_soc_component_driver soc_codec_tas5805m = {
	.probe = tas5805m_probe,
	.remove = tas5805m_remove,
	.suspend = tas5805m_snd_suspend,
	.resume = tas5805m_snd_resume,
	.set_bias_level = tas5805m_set_bias_level,
};

static int tas5805m_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	u8 iface_reg1 = 0;
	u8 iface_reg2 = 0;

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface_reg1 |= 1;
		break;
	default:
		dev_warn(component->dev, "%s: Invalid DAI clock signal polarity\n",
			__func__);
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		iface_reg2 |= 1;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface_reg2 |= 2;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface_reg2 |= 3;
		break;
	default:
		dev_warn(component->dev, "%s: No Format. Using default I2S\n",
			__func__);
	}

	snd_soc_component_update_bits(component, TAS5805M_REG_31,
		  TAS5805M_BCLK_INV, iface_reg1);
	snd_soc_component_update_bits(component, TAS5805M_REG_33,
		  TAS5805M_DATA_FMT, iface_reg2);

	return 0;
}

static int tas5805m_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *codec_dai)
{
	struct snd_soc_component *component = codec_dai->component;
	//struct tas5805m_priv *tas5805m =
	//snd_soc_component_get_drvdata(component);
	unsigned int bit_depth = params_width(params);
	int iface_reg = 0;

	/* bit depth set */
	switch (bit_depth) {
	case 16:
		break;
	case 20:
		iface_reg = 1;
		break;
	case 24:
		iface_reg = 2;
		break;
	case 32:
		iface_reg = 3;
		break;
	default:
		dev_warn(component->dev, "%s: Invalid bit_depth, using default 16bit\n",
			__func__);
	}
	snd_soc_component_update_bits(component, TAS5805M_REG_33,
		  TAS5805M_BIT_MSK, iface_reg);

	/* get rid of hissing sound for ultrasonic content */
	tas5805m_apply_seq(component, tas5805m_init_rc_filter,
			ARRAY_SIZE(tas5805m_init_rc_filter));

	if (get_lpm_mode_status_locked(component))
		tas5805m_configure_deepsleep_mode(component, false);

	return 0;
}

static int tas5805m_hw_free(struct snd_pcm_substream *substream,
					struct snd_soc_dai *dai)
{
	int ret = 0;
	struct snd_soc_component *component = dai->component;

	if (get_lpm_mode_status_locked(component))
		tas5805m_configure_deepsleep_mode(component, true);
	return ret;
}

static const struct snd_soc_dai_ops tas5805m_dai_ops = {
	.hw_params = tas5805m_hw_params,
	.set_fmt = tas5805m_set_dai_fmt,
	//.digital_mute = tas5805m_mute,
	.trigger = tas5805m_trigger,
	.hw_free = tas5805m_hw_free,
};

static struct snd_soc_dai_driver tas5805m_dai = {
	.name = "tas5805m-amplifier",
	.playback = {
		     .stream_name = "Playback",
		     .channels_min = 2,
		     .channels_max = 8,
		     .rates = TAS5805M_RATES,
		     .formats = TAS5805M_FORMATS,
		     },
	.ops = &tas5805m_dai_ops,
};

static int tas5805m_parse_dt(struct tas5805m_priv *tas5805m)
{
	int ret = 0;
	int reset_pin = -1;
	int power_pin = -1;

	reset_pin = of_get_named_gpio(tas5805m->dev->of_node, "reset-gpio", 0);
	if (reset_pin < 0)
		pr_warn("%s fail to get reset pin from dts!\n", __func__);
	tas5805m->pdata->reset_pin = reset_pin;
	power_pin = of_get_named_gpio(tas5805m->dev->of_node, "power-gpio", 0);
	tas5805m->pdata->power_pin = power_pin;
	gpio_direction_output(power_pin, GPIOF_OUT_INIT_HIGH);

	ret = device_property_read_u32(tas5805m->dev, "speaker-config",
			&tas5805m->pdata->spk_config);
	if (ret)
		pr_warn("%s: speaker-config not found. Setting to default\n",
			__func__);

	if (device_property_read_bool(tas5805m->dev, "pbtl-mode"))
		tas5805m->pdata->pbtl_mode = true;
	else
		tas5805m->pdata->pbtl_mode = false;
	dev_info(tas5805m->dev, "%s: pbtl mode %d\n", __func__,
					tas5805m->pdata->pbtl_mode);

	return ret;
}

static int tas5805m_i2c_probe(struct i2c_client *i2c,
			      const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct regmap_config config = tas5805m_regmap;
	struct tas5805m_priv *tas5805m;
	struct tas5805m_platform_data *pdata;
	int ret = 0;

	tas5805m = devm_kzalloc(&i2c->dev,
		sizeof(struct tas5805m_priv), GFP_KERNEL);
	if (!tas5805m)
		return -ENOMEM;

	regmap = devm_regmap_init_i2c(i2c, &config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	pdata = devm_kzalloc(&i2c->dev,
				sizeof(struct tas5805m_platform_data),
				GFP_KERNEL);
	if (!pdata) {
		pr_err("%s failed to kzalloc for tas5805 pdata\n", __func__);
		return -ENOMEM;
	}
	tas5805m->pdata = pdata;
	tas5805m->dev =  &i2c->dev;

	tas5805m_parse_dt(tas5805m);
	tas5805m->regmap = regmap;
	tas5805m->vol = master_vol;

	dev_set_drvdata(&i2c->dev, tas5805m);

	reset_tas5805m_GPIO(&i2c->dev);

	ret = snd_soc_register_component(&i2c->dev, &soc_codec_tas5805m,
			&tas5805m_dai, 1);
	if (ret != 0)
		return -ENOMEM;

	return ret;
}

static int tas5805m_i2c_remove(struct i2c_client *i2c)
{
	devm_kfree(&i2c->dev, i2c_get_clientdata(i2c));

	return 0;
}

static const struct i2c_device_id tas5805m_i2c_id[] = {
	{"tas5805m",},
	{}
};

static struct tas5805m_info info[] = {
	{
		.info_id = 0,
	},
	{
		.info_id = 1,
	},
	{}
};
MODULE_DEVICE_TABLE(i2c, tas5805m_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id tas5805m_of_match[] = {
	{
		.compatible = "ti,tas5805m_0",
		.data = &info[0],
	},
	{
		.compatible = "ti,tas5805m_1",
		.data = &info[1],
	},
	{}
};

MODULE_DEVICE_TABLE(of, tas5805m_of_match);
#endif

static struct i2c_driver tas5805m_i2c_driver = {
	.probe = tas5805m_i2c_probe,
	.remove = tas5805m_i2c_remove,
	.id_table = tas5805m_i2c_id,
	.driver = {
		   .name = TAS5805M_DRV_NAME,
		   .of_match_table = tas5805m_of_match,
		   },
};

module_i2c_driver(tas5805m_i2c_driver);

MODULE_AUTHOR("Andy Liu <andy-liu@ti.com>");
MODULE_DESCRIPTION("TAS5805M Audio Amplifier Driver");
MODULE_LICENSE("GPL v2");
