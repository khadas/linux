/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __IEC_INFO_H__
#define __IEC_INFO_H__

#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/control.h>

/* mpll clk range from 5M to 500M */
#define AML_MPLL_FREQ_MIN   5000000
#define AML_MPLL_FREQ_MAX   500000000

/* IEC958_mode_codec */
#define STEREO_PCM              0
#define DTS_RAW_MODE            1
#define DOLBY_DIGITAL           2
#define DTS                     3
#define DD_PLUS         4
#define DTS_HD                  5
#define MULTI_CHANNEL_LPCM      6
#define TRUEHD                  7
#define _DTS_HD_MA              8
#define HIGH_SR_STEREO_LPCM     9

/* info for iec60958/iec60937 */

#define IEC_BLOCK_USE   (0x1 << 0)
#define IEC_BLOCK_FMT   (0x1 << 1)

/* EARC differential falling edge modulation audio clk ratio */
#define EARC_DMAC_MUTIPLIER     5

/* IEC Block Use */
enum iec_block_use {
	IEC_BU_CONSUMER,
	IEC_BU_PROFESSIONAL
};

/* IEC Block, audio format */
enum iec_audio_fmt {
	IEC_FMT_LPCM,     /* Linear PCM Audio */
	IEC_FMT_NON_LPCM  /* Non-Linear PCM audio */
};

/* IEC Block, additional audio format, depends on audio format */
enum iec_lpcm_addl_fmt {
	LPCM_ADDL_FMT_NO_PRE_EMPHASIS = 0x0,
	/* 50us/15us pre-emphasis */
	LPCM_ADDL_FMT_PRE_EMPHASIS = 0x4,
	/* Reserved(for 2 audio channels with pre-emphasis) */
	LPCM_ADDL_FMT_RESERVED0 = 0x2,
	/* Reserved(for 2 audio channels with pre-emphasis) */
	LPCM_ADDL_FMT_RESERVED1 = 0x6,
};

enum iec_non_lpcm_addl_fmt {
	/* Default state for application other than linear PCM */
	NON_LPCM_ADDL_FMT_DEFAULT = 0x0,
};

/* channel status in IEC 60958-3 for consumer applications */
/* IEC Block, whether copyright is asserted */
enum iec_cnsmr_copyright {
	IEC_COPYRIGHT,    /* copyright is asserted */
	IEC_NO_COPYRIGHT  /* no copyright is asserted */
};

enum channel_status_type {
	CS_TYPE_A, /* channel A status */
	CS_TYPE_B  /* channel B status */
};

/* consumer channel status structure */
struct iec_cnsmr_cs {
	/* CS bit0, 0: consumer use, 1 for Professional use */
	bool user;
	/* CS bit1, 0: L-PCM, 1: NONE LPCM */
	bool fmt;
	/* CS bit2, copyright bit, 0: sw copyright, 1: no copyright */
	bool cp;
	/* CS bits3,4,5, additional format information
	 * meaning depends on bit 1
	 */
	int emphasis;
	/* CS bit6,7, channel status mode, default: 0 */
	int mode;
	/* CS bit8~15, category code */
	int category_code;
	/* CS bit16~19, source number */
	int source_num;
	/* CS bit20~23, channel number */
	int channel_num;
	/* CS bit24~27, sampling frequency */
	int sampling_freq;
	/* CS bit28,29, clock accuracy */
	int clock_accuracy;
	/* CS bit 32, max word length, 0: 20bits, 1:24bits */
	bool max_wlen;
	/* CS bit33~35, by max word length */
	int wlen;
	/* CS bit36~39, Original sampling frequency */
	int osf;
	/* CS bit, transmitted sampling frequency */
	int tsf;
	/* CS bit40,41, CGMS_A */
	int CGMS_A;
};

/* use new enum for eARC */
enum audio_coding_types {
	AUDIO_CODING_TYPE_UNDEFINED          = 0,

	/* LINEAR PCM */
	AUDIO_CODING_TYPE_STEREO_LPCM        = 1,
	AUDIO_CODING_TYPE_MULTICH_2CH_LPCM   = 2,
	AUDIO_CODING_TYPE_MULTICH_8CH_LPCM   = 3,
	AUDIO_CODING_TYPE_MULTICH_16CH_LPCM  = 4,
	AUDIO_CODING_TYPE_MULTICH_32CH_LPCM  = 5,
	/* High bit rate */
	AUDIO_CODING_TYPE_HBR_LPCM           = 6,

	/*
	 * NON-LINEAR PCM
	 * IEC61937-2, Burst-info, Data type
	 */
	/* Dolby */
	/* AC3 Layout A */
	AUDIO_CODING_TYPE_AC3                = 7,
	/* AC3 Layout B */
	AUDIO_CODING_TYPE_AC3_LAYOUT_B       = 8,
	AUDIO_CODING_TYPE_EAC3               = 9,
	AUDIO_CODING_TYPE_MLP                = 10,
	/* DTS */
	AUDIO_CODING_TYPE_DTS                = 11,
	AUDIO_CODING_TYPE_DTS_HD             = 12,
	AUDIO_CODING_TYPE_DTS_HD_MA          = 13,

	/* Super Audio CD, DSD (One Bit Audio) */
	AUDIO_CODING_TYPE_SACD_6CH           = 14,
	AUDIO_CODING_TYPE_SACD_12CH          = 15,

	/* Pause */
	AUDIO_CODING_TYPE_PAUSE              = 16,
	AUDIO_CODING_TYPE_EAC3_LAYOUT_B      = 17,
	AUDIO_CODING_TYPE_MLP_LAYOUT_B       = 18,
	AUDIO_CODING_TYPE_DTS_LAYOUT_B       = 19,
	AUDIO_CODING_TYPE_DTS_HD_LAYOUT_B    = 20,
	AUDIO_CODING_TYPE_DTS_HD_MA_LAYOUT_B = 21,

};

static const char * const audio_coding_type_names[] = {
	/*  0 */ "UNDEFINED",
	/*  1 */ "STEREO LPCM",
	/*  2 */ "MULTICH 2CH LPCM",
	/*  3 */ "MULTICH 8CH LPCM",
	/*  4 */ "MULTICH 16CH LPCM",
	/*  5 */ "MULTICH 32CH LPCM",
	/*  6 */ "High Bit Rate LPCM",
	/*  7 */ "AC-3 (Dolby Digital)", /* Layout A */
	/*  8 */ "AC-3 (Dolby Digital Layout B)",
	/*  9 */ "E-AC-3/DD+ (Dolby Digital Plus)",
	/* 10 */ "MLP (Dolby TrueHD)",
	/* 11 */ "DTS",
	/* 12 */ "DTS-HD",
	/* 13 */ "DTS-HD MA",
	/* 14 */ "DSD (One Bit Audio 6CH)",
	/* 15 */ "DSD (One Bit Audio 12CH)",
	/* 16 */ "PAUSE",
	/* 17 */ "E-AC-3/DD+ (Dolby Digital Plus Layout B)",
	/* 18 */ "MLP (Dolby TrueHD Layout B)",
	/* 19 */ "DTS Layout B",
	/* 20 */ "DTS-HD Layout B",
	/* 21 */ "DTS-HD MA Layout B",
};

/* spdif in audio format detect: LPCM or NONE-LPCM */
struct spdif_audio_info {
	unsigned char aud_type;
	/*IEC61937 package presamble Pc value*/
	short pc;
	char *aud_type_str;
};

static const struct spdif_audio_info type_texts[] = {
	{0, 0, "LPCM"},
	{1, 0x1, "AC3"},
	{2, 0x15, "EAC3"},
	{3, 0xb, "DTS-I"},
	{3, 0x0c, "DTS-II"},
	{3, 0x0d, "DTS-III"},
	{3, 0x11, "DTS-IV"},
	{4, 0, "DTS-HD"},
	{5, 0x16, "TRUEHD"},
	{6, 0x103, "PAUSE"},
	{6, 0x003, "PAUSE"},
	{6, 0x100, "PAUSE"},
};

static const char *const audio_type_texts[] = {
	"LPCM",
	"AC3",
	"EAC3",
	"DTS",
	"DTS-HD",
	"TRUEHD",
	"PAUSE"
};

/* current sample mode and its sample rate */
static const char *const spdifin_samplerate[] = {
	"N/A",
	"32000",
	"44100",
	"48000",
	"88200",
	"96000",
	"176400",
	"192000"
};

extern const struct spdif_audio_info type_texts[];
extern const char *const audio_type_texts[];

extern const struct soc_enum audio_coding_type_enum;

extern const struct soc_enum spdifin_sample_rate_enum[];

bool audio_coding_is_lpcm(enum audio_coding_types coding_type);
bool audio_coding_is_non_lpcm(enum audio_coding_types coding_type);
int audio_multi_clk(enum audio_coding_types coding_type);
unsigned int iec_rate_from_csfs(unsigned int csfs);
void iec_get_cnsmr_cs_info(struct iec_cnsmr_cs *cs_info,
			   enum audio_coding_types coding_type,
			   unsigned int channels,
			   unsigned int rate);
enum audio_coding_types iec_61937_pc_to_coding_type(unsigned int pc);

/* Keep this enum, inherited from old code */
enum aud_codec_types {
	AUD_CODEC_TYPE_STEREO_PCM     = 0x0,
	AUD_CODEC_TYPE_DTS_RAW_MODE   = 0x1,
	AUD_CODEC_TYPE_AC3            = 0x2,
	AUD_CODEC_TYPE_DTS            = 0x3,
	AUD_CODEC_TYPE_EAC3           = 0x4,
	AUD_CODEC_TYPE_DTS_HD         = 0x5,
	AUD_CODEC_TYPE_MULTI_LPCM     = 0x6,
	AUD_CODEC_TYPE_TRUEHD         = 0x7,
	AUD_CODEC_TYPE_DTS_HD_MA      = 0x8,
	AUD_CODEC_TYPE_HSR_STEREO_PCM = 0x9,
	AUD_CODEC_TYPE_AC3_LAYOUT_B   = 0xa,
	AUD_CODEC_TYPE_OBA            = 0xb,
};

static const char * const aud_codec_type_names[] = {
	/* 0 */"Stereo PCM",
	/* 1 */"DTS RAW Mode",
	/* 2 */"Dolby Digital",
	/* 3 */"DTS",
	/* 4 */"Dolby Digital Plus",
	/* 5 */"DTS-HD",
	/* 6 */"Multi-channel LPCM",
	/* 7 */"Dolby TrueHD",
	/* 8 */"DTS-HD MA",
	/* 9 */"HIGH SR Stereo LPCM",
	/* 10*/"Dolby Digital(Layout B)",
	/* 11*/"One Bit Audio"
};

extern const struct soc_enum aud_codec_type_enum;

struct iec958_chsts {
	unsigned short chstat0_l;
	unsigned short chstat1_l;
	unsigned short chstat2_l;
	unsigned short chstat0_r;
	unsigned short chstat1_r;
	unsigned short chstat2_r;
};

bool codec_is_raw(enum aud_codec_types codec_type);

bool raw_is_4x_clk(enum aud_codec_types codec_type);

unsigned int mpll2sys_clk_ratio_by_type(enum aud_codec_types codec_type);
unsigned int mpll2dmac_clk_ratio_by_type(enum audio_coding_types coding_type);
/* l_bit is to set the consumer copyright if copying allowed */
void iec_get_channel_status_info(struct iec958_chsts *chsts,
				 enum aud_codec_types codec_type,
				 unsigned int rate,
				 unsigned int bit_depth,
				 unsigned int l_bit);

void spdif_notify_to_hdmitx(struct snd_pcm_substream *substream,
			    enum aud_codec_types codec_type);
void notify_hdmitx_to_prepare(void);

#ifdef CONFIG_AMLOGIC_HDMITX
int aml_get_hdmi_out_audio(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);
int aml_set_hdmi_out_audio(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol);
#endif
#endif
