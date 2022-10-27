/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_PDM_H__
#define __AML_PDM_H__

#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include "../common/misc.h"

#define DEFAULT_FS_RATIO		256

#define PDM_CHANNELS_MIN		1
/* 8ch pdm in, 8 ch tdmin_lb */
#define PDM_CHANNELS_LB_MAX		(PDM_CHANNELS_MAX + 8)

#define PDM_RATES			(SNDRV_PCM_RATE_96000 |\
					SNDRV_PCM_RATE_64000 |\
					SNDRV_PCM_RATE_48000 |\
					SNDRV_PCM_RATE_32000 |\
					SNDRV_PCM_RATE_16000 |\
					SNDRV_PCM_RATE_8000)

#define PDM_FORMATS			(SNDRV_PCM_FMTBIT_S16_LE |\
					SNDRV_PCM_FMTBIT_S24_LE |\
					SNDRV_PCM_FMTBIT_S32_LE)

#define PDM_TRAIN_VERSION_V1		(1)
#define PDM_TRAIN_VERSION_V2		(2)
#define PDM_A 0
#define PDM_B 1
enum {
	PDM_RUN_MUTE_VAL = 0,
	PDM_RUN_MUTE_CHMASK,

	PDM_RUN_MAX,
};

struct pdm_chipinfo {
	/* pdm supports mute function */
	bool mute_fn;
	/* truncate invalid data when filter init */
	bool truncate_data;
	/* train */
	bool train;

	int train_version;
	int id;
	bool use_arb;
	/* vad top */
	bool vad_top;
	bool regulator;
	bool oscin_divide;
};

struct aml_pdm {
	struct device *dev;
	struct aml_audio_controller *actrl;
	struct pinctrl *pdm_pins;
	struct clk *clk_gate;
	/* sel: fclk_div3(666M) */
	struct clk *sysclk_srcpll;
	/* consider same source with tdm, 48k(24576000) */
	struct clk *dclk_srcpll;
	struct clk *clk_pdm_sysclk;
	struct clk *clk_pdm_dclk;
	struct toddr *tddr;

	struct pdm_chipinfo *chipinfo;
	struct snd_kcontrol *controls[PDM_RUN_MAX];

	/* sample rate */
	int rate;
	/*
	 * filter mode:0~4,
	 * from mode 0 to 4, the performance is from high to low,
	 * the group delay (latency) is from high to low.
	 */
	int filter_mode;
	/* dclk index */
	int dclk_idx;
	/* PCM or Raw Data */
	int bypass;

	/* lane mask in, each lane carries two channels */
	int lane_mask_in;

	/* PDM clk on/off, only clk on, pdm registers can be accessed */
	bool clk_on;

	/* train */
	bool train_en;

	/* low power mode, for dclk_sycpll to 24m */
	bool islowpower;
	/* force to lower power when suspend */
	bool force_lowpower;
	/* Hibernation for vad, suspended or not */
	bool vad_hibernation;
	/* whether vad buffer is used, for xrun */
	bool vad_buf_occupation;
	bool vad_buf_recovery;
	int pdm_gain_index;

	int train_sample_count;
	int pdm_id;
	enum trigger_state pdm_trigger_state;
	int pdm_train_debug;
	struct work_struct debug_work;
	unsigned int syssrc_clk_rate;
	struct regulator *regulator_vcc3v3;
	struct regulator *regulator_vcc5v;
};

int pdm_get_train_sample_count_from_dts(void);
int pdm_get_train_version(void);
#endif /*__AML_PDM_H__*/
