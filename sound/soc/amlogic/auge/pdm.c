// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>

#include <sound/soc.h>
#include <sound/tlv.h>

#include <linux/amlogic/pm.h>

#include "pdm.h"
#include "pdm_hw.h"
#include "pdm_match_table.h"
#include "audio_io.h"
#include "iomap.h"
#include "regs.h"
#include "ddr_mngr.h"
#include "vad.h"
#include "pdm_hw_coeff.h"

#define DRV_NAME "snd_pdm"
#define DRV_NAME_B "snd_pdm_b"
#define PDM_BUFFER_BYTES (512 * 1024)

static struct snd_pcm_hardware aml_pdm_hardware = {
	.info			=
					SNDRV_PCM_INFO_MMAP |
					SNDRV_PCM_INFO_MMAP_VALID |
					SNDRV_PCM_INFO_INTERLEAVED |
					SNDRV_PCM_INFO_PAUSE |
					SNDRV_PCM_INFO_RESUME,

	.formats		=	SNDRV_PCM_FMTBIT_S16 |
					SNDRV_PCM_FMTBIT_S24 |
					SNDRV_PCM_FMTBIT_S32,

	.rate_min			=	8000,
	.rate_max			=	96000,

	.channels_min		=	PDM_CHANNELS_MIN,
	.channels_max		=	PDM_CHANNELS_LB_MAX,

	.buffer_bytes_max	=	PDM_BUFFER_BYTES,
	.period_bytes_max	=	256 * 1024,
	.period_bytes_min	=	32,
	.periods_min		=	2,
	.periods_max		=	1024,
	.fifo_size		=	0,
};

static const char *const pdm_filter_mode_texts[] = {
	"Filter Mode 0",
	"Filter Mode 1",
	"Filter Mode 2",
	"Filter Mode 3",
	"Filter Mode 4"
};

static const struct soc_enum pdm_filter_mode_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(pdm_filter_mode_texts),
			pdm_filter_mode_texts);

static struct aml_pdm *s_pdm;

static struct aml_pdm *get_pdm(void)
{
	if (!s_pdm) {
		pr_err("Not init pdm\n");
		return NULL;
	}

	return s_pdm;
}

int pdm_get_train_sample_count_from_dts(void)
{
	struct aml_pdm *p_pdm = get_pdm();

	if (p_pdm)
		return  p_pdm->train_sample_count;
	else
		return -1;
}

int pdm_get_train_version(void)
{
	struct aml_pdm *p_pdm = get_pdm();

	if (p_pdm && p_pdm->chipinfo)
		return p_pdm->chipinfo->train_version;

	return 0;
}

static int aml_pdm_filter_mode_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_pdm *p_pdm = dev_get_drvdata(component->dev);

	if (!p_pdm)
		return 0;

	ucontrol->value.enumerated.item[0] = p_pdm->filter_mode;

	return 0;
}

static int aml_pdm_filter_mode_set_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_pdm *p_pdm = dev_get_drvdata(component->dev);

	if (!p_pdm)
		return 0;

	p_pdm->filter_mode = ucontrol->value.enumerated.item[0];

	return 0;
}

int pdm_hcic_shift_gain;

static const char *const pdm_hcic_shift_gain_texts[] = {
	"keep with coeff",
	"shift with -0x4",
};

static const struct soc_enum pdm_hcic_shift_gain_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(pdm_hcic_shift_gain_texts),
			pdm_hcic_shift_gain_texts);

static int pdm_hcic_shift_gain_get_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = pdm_hcic_shift_gain;

	return 0;
}

static int pdm_hcic_shift_gain_set_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	pdm_hcic_shift_gain = ucontrol->value.enumerated.item[0];

	return 0;
}

static const char *const pdm_dclk_texts[] = {
	"PDM Dclk 3.072m, support 8k/16k/32k/48k/64k/96k",
	"PDM Dclk 1.024m, support 8k/16k",
	"PDM Dclk   768k, support 8k/16k",
};

static const struct soc_enum pdm_dclk_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(pdm_dclk_texts),
			pdm_dclk_texts);

static int pdm_dclk_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_pdm *p_pdm = dev_get_drvdata(component->dev);

	if (!p_pdm)
		return 0;

	ucontrol->value.enumerated.item[0] = p_pdm->dclk_idx;

	return 0;
}

static int pdm_dclk_set_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_pdm *p_pdm = dev_get_drvdata(component->dev);

	if (!p_pdm)
		return 0;

	p_pdm->dclk_idx = ucontrol->value.enumerated.item[0];

	return 0;
}

static const char *const pdm_bypass_texts[] = {
	"PCM Data",
	"Raw Data/Bypass Data",
};

static const struct soc_enum pdm_bypass_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(pdm_bypass_texts),
			pdm_bypass_texts);

static int pdm_bypass_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_pdm *p_pdm = dev_get_drvdata(component->dev);

	if (!p_pdm)
		return 0;

	ucontrol->value.enumerated.item[0] = p_pdm->bypass;

	return 0;
}

static int pdm_bypass_set_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_pdm *p_pdm = dev_get_drvdata(component->dev);

	if (!p_pdm)
		return 0;

	p_pdm->bypass = ucontrol->value.enumerated.item[0];

	if (p_pdm->clk_on)
		pdm_set_bypass_data((bool)p_pdm->bypass, p_pdm->chipinfo->id);

	return 0;
}

static const char *const pdm_lowpower_texts[] = {
	"PDM Normal Mode",
	"PDM Low Power Mode",
};

static const struct soc_enum pdm_lowpower_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(pdm_lowpower_texts),
			pdm_lowpower_texts);

static void pdm_set_lowpower_mode(struct aml_pdm *p_pdm, bool islowpower, int id)
{
	if (p_pdm->islowpower == islowpower)
		return;

	p_pdm->islowpower = islowpower;

	if (p_pdm->clk_on) {
		/* check to set pdm sysclk */
		pdm_force_sysclk_to_oscin(p_pdm->islowpower, id, p_pdm->chipinfo->vad_top);

		if (p_pdm->islowpower) {
			/* set dclk clk_sel is oscin,and set rate is 3M */
			pdm_force_dclk_to_oscin(id, p_pdm->chipinfo->vad_top);
			pdm_set_channel_ctrl(3, id);
		} else {
			clk_set_parent(p_pdm->clk_pdm_dclk, p_pdm->dclk_srcpll);
			clk_set_rate(p_pdm->clk_pdm_dclk, pdm_dclkidx2rate(p_pdm->dclk_idx));
			pdm_set_channel_ctrl(pdm_get_sample_count(false, p_pdm->dclk_idx), id);
		}

		pr_info("\n%s, pdm_sysclk:%lu pdm_dclk:%lu, dclk_srcpll:%lu\n",
			__func__,
			clk_get_rate(p_pdm->clk_pdm_sysclk),
			clk_get_rate(p_pdm->clk_pdm_dclk),
			clk_get_rate(p_pdm->dclk_srcpll));

		/* Check to set vad for Low Power */
		if (vad_pdm_is_running())
			vad_set_lowerpower_mode(p_pdm->islowpower);
	}
}

static int pdm_lowpower_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_pdm *p_pdm = dev_get_drvdata(component->dev);

	if (!p_pdm)
		return 0;

	ucontrol->value.enumerated.item[0] = p_pdm->islowpower;

	return 0;
}

static int pdm_lowpower_set_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_pdm *p_pdm = dev_get_drvdata(component->dev);
	bool islowpower;

	if (!p_pdm)
		return 0;
	islowpower = (bool)ucontrol->value.enumerated.item[0];
	pdm_set_lowpower_mode(p_pdm, islowpower, p_pdm->pdm_id);

	return 0;
}

static const char *const pdm_train_texts[] = {
	"Disabled",
	"Enable",
};

static const struct soc_enum pdm_train_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(pdm_train_texts),
			pdm_train_texts);

static int pdm_train_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_pdm *p_pdm = dev_get_drvdata(component->dev);

	if (!p_pdm)
		return 0;

	ucontrol->value.enumerated.item[0] = p_pdm->train_en;

	return 0;
}

static int pdm_train_set_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_pdm *p_pdm = dev_get_drvdata(component->dev);
	if (!p_pdm)
		return 0;
	if (!p_pdm ||
		!p_pdm->chipinfo ||
		!p_pdm->chipinfo->train ||
		p_pdm->train_en == ucontrol->value.enumerated.item[0])
		return 0;

	p_pdm->train_en = ucontrol->value.enumerated.item[0];

	if (p_pdm->clk_on)
		pdm_train_en(p_pdm->train_en, p_pdm->pdm_id);

	return 0;
}

/* set pdm gain index. */
static int pdm_gain_get_enum(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_pdm *p_pdm = dev_get_drvdata(component->dev);

	ucontrol->value.integer.value[0] = p_pdm->pdm_gain_index;

	return 0;
}

static int pdm_gain_set_enum(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_pdm *p_pdm = dev_get_drvdata(component->dev);
	int value = ucontrol->value.integer.value[0];

	if (value < 0 || value > (NUM_PDM_GAIN_INDEX - 1)) {
		pr_info("%s, invalid value: %d [Range:0~48]", __func__, value);
		return 0;
	}

	p_pdm->pdm_gain_index = value;

	return 0;
}

static int pdm_train_debug_get_enum(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_pdm *p_pdm = dev_get_drvdata(component->dev);

	ucontrol->value.integer.value[0] = p_pdm->pdm_train_debug;

	return 0;
}

static int pdm_train_debug_set_enum(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_pdm *p_pdm = dev_get_drvdata(component->dev);
	int value = ucontrol->value.integer.value[0];
	if (!component->active)
		p_pdm->pdm_train_debug = 0;
	else
		p_pdm->pdm_train_debug = value;
	schedule_work(&p_pdm->debug_work);

	return 0;
}

static const struct snd_kcontrol_new snd_pdm_controls[] = {
	/* which set */
	SOC_ENUM_EXT("PDM Filter Mode",
		     pdm_filter_mode_enum,
		     aml_pdm_filter_mode_get_enum,
		     aml_pdm_filter_mode_set_enum),

	/* fix HCIC shift gain according current dmic */
	SOC_ENUM_EXT("PDM HCIC shift gain from coeff",
		     pdm_hcic_shift_gain_enum,
		     pdm_hcic_shift_gain_get_enum,
		     pdm_hcic_shift_gain_set_enum),

	SOC_ENUM_EXT("PDM Dclk",
		     pdm_dclk_enum,
		     pdm_dclk_get_enum,
		     pdm_dclk_set_enum),

	SOC_ENUM_EXT("PDM Low Power mode",
			pdm_lowpower_enum,
			pdm_lowpower_get_enum,
			pdm_lowpower_set_enum),

	SOC_ENUM_EXT("PDM Train",
		     pdm_train_enum,
		     pdm_train_get_enum,
		     pdm_train_set_enum),

	SOC_ENUM_EXT("PDM Bypass",
		     pdm_bypass_enum,
		     pdm_bypass_get_enum,
		     pdm_bypass_set_enum),

	/* index of pdm_gain_table[49], index: 0~48 */
	SOC_SINGLE_EXT("PDM Gain",
		     SND_SOC_NOPM, 0,
		     (NUM_PDM_GAIN_INDEX - 1), 0,
		     pdm_gain_get_enum,
		     pdm_gain_set_enum),

	SOC_SINGLE_EXT("PDM A Train Debug Enable",
		     SND_SOC_NOPM, 0,
		     1, 0,
		     pdm_train_debug_get_enum,
		     pdm_train_debug_set_enum),

};

static const struct snd_kcontrol_new snd_pdmb_controls[] = {
	/* which set */
	SOC_ENUM_EXT("PDM Filter Mode B",
		     pdm_filter_mode_enum,
		     aml_pdm_filter_mode_get_enum,
		     aml_pdm_filter_mode_set_enum),

	/* fix HCIC shift gain according current dmic */
	SOC_ENUM_EXT("PDM HCIC shift gain from coeff B",
		     pdm_hcic_shift_gain_enum,
		     pdm_hcic_shift_gain_get_enum,
		     pdm_hcic_shift_gain_set_enum),

	SOC_ENUM_EXT("PDM Dclk B",
		     pdm_dclk_enum,
		     pdm_dclk_get_enum,
		     pdm_dclk_set_enum),

	SOC_ENUM_EXT("PDM Low Power mode B",
			pdm_lowpower_enum,
			pdm_lowpower_get_enum,
			pdm_lowpower_set_enum),

	SOC_ENUM_EXT("PDM Train B",
		     pdm_train_enum,
		     pdm_train_get_enum,
		     pdm_train_set_enum),

	SOC_ENUM_EXT("PDM Bypass B",
		     pdm_bypass_enum,
		     pdm_bypass_get_enum,
		     pdm_bypass_set_enum),

	/* index of pdm_gain_table[49], index: 0~48 */
	SOC_SINGLE_EXT("PDM Gain B",
		     SND_SOC_NOPM, 0,
		     (NUM_PDM_GAIN_INDEX - 1), 0,
		     pdm_gain_get_enum,
		     pdm_gain_set_enum),
	SOC_SINGLE_EXT("PDM B Train Debug Enable",
		     SND_SOC_NOPM, 0,
		     1, 0,
		     pdm_train_debug_get_enum,
		     pdm_train_debug_set_enum),

};
static irqreturn_t aml_pdm_isr_handler(int irq, void *data)
{
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->cpu_dai->dev;
	struct aml_pdm *p_pdm = (struct aml_pdm *)dev_get_drvdata(dev);
	unsigned int status;
	int train_sts = 0;

	if (p_pdm->chipinfo &&
	    p_pdm->chipinfo->train &&
	    p_pdm->train_en)
		train_sts = pdm_train_sts(p_pdm->chipinfo->id);

	if (!snd_pcm_running(substream))
		return IRQ_NONE;

	status = aml_toddr_get_status(p_pdm->tddr) & MEMIF_INT_MASK;
	if (status & MEMIF_INT_COUNT_REPEAT) {
		snd_pcm_period_elapsed(substream);

		aml_toddr_ack_irq(p_pdm->tddr, MEMIF_INT_COUNT_REPEAT);
	} else {
		dev_dbg(dev, "unexpected irq - STS 0x%02x\n",
			status);
	}

	if (train_sts) {
		pr_debug("%s train result:0x%x\n", __func__, train_sts);
		pdm_train_clr(p_pdm->chipinfo->id);
	}

	return !status ? IRQ_NONE : IRQ_HANDLED;
}

static int aml_pdm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->cpu_dai->dev;
	struct aml_pdm *p_pdm = (struct aml_pdm *)dev_get_drvdata(dev);
	int ret;

	pr_debug("%s, stream:%d\n", __func__, substream->stream);
	snd_soc_set_runtime_hwparams(substream, &aml_pdm_hardware);

	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
						SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		dev_err(substream->pcm->card->dev,
			"%s() setting constraints failed: %d\n",
			__func__, ret);
		return -EINVAL;
	}

	snd_pcm_lib_preallocate_pages(substream, SNDRV_DMA_TYPE_DEV,
		dev, PDM_BUFFER_BYTES / 2, PDM_BUFFER_BYTES);

	runtime->private_data = p_pdm;

	p_pdm->tddr = aml_audio_register_toddr
		(dev, aml_pdm_isr_handler, substream);
	if (!p_pdm->tddr) {
		ret = -ENXIO;
		dev_err(dev, "failed to claim to ddr\n");
		goto err_ddr;
	}

	return 0;

err_ddr:
	snd_pcm_lib_preallocate_free(substream);
	return ret;
}

static int aml_pdm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_pdm *p_pdm = runtime->private_data;

	pr_debug("enter %s type: %d\n", __func__, substream->stream);
	aml_audio_unregister_toddr(p_pdm->dev, substream);
	snd_pcm_lib_preallocate_free(substream);

	return 0;
}

static int aml_pdm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
}

static int aml_pdm_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int aml_pdm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_pdm *p_pdm = runtime->private_data;

	struct toddr *to = p_pdm->tddr;
	unsigned int start_addr, end_addr, int_addr;
	unsigned int period, threshold;

	if (p_pdm->pdm_trigger_state == TRIGGER_START_ALSA_BUF ||
	    p_pdm->pdm_trigger_state == TRIGGER_START_VAD_BUF) {
		pr_err("%s, trigger state is %d\n", __func__,
			p_pdm->pdm_trigger_state);
		return 0;
	}

	start_addr = runtime->dma_addr;
	end_addr = start_addr + runtime->dma_bytes - FIFO_BURST;
	period   = frames_to_bytes(runtime, runtime->period_size);
	int_addr = period / FIFO_BURST;

	/*
	 * Contrast minimum of period and fifo depth,
	 * and set the value as half.
	 */
	threshold = min(period, p_pdm->tddr->chipinfo->fifo_depth);
	threshold /= 2;
	aml_toddr_set_fifos(to, threshold);

	aml_toddr_set_buf(to, start_addr, end_addr);
	aml_toddr_set_intrpt(to, int_addr);

	return 0;
}

static snd_pcm_uframes_t aml_pdm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct aml_pdm *p_pdm = runtime->private_data;
	unsigned int addr = 0, start_addr = 0;

	start_addr = runtime->dma_addr;

	addr = aml_toddr_get_position(p_pdm->tddr);

	return bytes_to_frames(runtime, addr - start_addr);
}

static int aml_pdm_mmap(struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static struct snd_pcm_ops aml_pdm_ops = {
	.open = aml_pdm_open,
	.close = aml_pdm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = aml_pdm_hw_params,
	.hw_free = aml_pdm_hw_free,
	.prepare = aml_pdm_prepare,
	.pointer = aml_pdm_pointer,
	.mmap = aml_pdm_mmap,
};

static int aml_pdm_dai_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct aml_pdm *p_pdm = snd_soc_dai_get_drvdata(cpu_dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int bitwidth;
	unsigned int toddr_type, lsb;
	struct toddr_fmt fmt;
	unsigned int osr = 192, filter_mode, dclk_idx;
	struct pdm_info info;
	int pdm_id;
	struct toddr *to;

	if (!p_pdm->chipinfo)
		return -EINVAL;
	to = p_pdm->tddr;
	pdm_id = p_pdm->pdm_id;
	if (p_pdm->pdm_trigger_state == TRIGGER_START_ALSA_BUF ||
	    p_pdm->pdm_trigger_state == TRIGGER_START_VAD_BUF) {
		pr_err("%s, trigger state is %d\n", __func__,
			p_pdm->pdm_trigger_state);
		return 0;
	}

	p_pdm->rate = runtime->rate;

	/* set bclk */
	bitwidth = snd_pcm_format_width(runtime->format);
	lsb = 32 - bitwidth;

	switch (bitwidth) {
	case 16:
	case 32:
		toddr_type = 0;
		break;
	case 24:
		toddr_type = 4;
		break;
	default:
		pr_err("invalid bit_depth: %d\n", bitwidth);
		return -1;
	}

	pr_debug("%s pdm_id :%d rate:%d, bits:%d, channels:%d\n",
		__func__,
		pdm_id,
		runtime->rate,
		bitwidth,
		runtime->channels);

	/* to ddr pdmin */
	fmt.type      = toddr_type;
	fmt.msb       = 31;
	fmt.lsb       = lsb;
	fmt.endian    = 0;
	fmt.bit_depth = bitwidth;
	fmt.ch_num    = runtime->channels;
	fmt.rate      = runtime->rate;
	if (pdm_id == PDM_A)
		aml_toddr_select_src(to, PDMIN);
	else
		aml_toddr_select_src(to, PDMIN_B);

	aml_toddr_set_format(to, &fmt);

	/* force pdm sysclk to 24m */
	if (p_pdm->islowpower) {
		/* dclk for 768k */
		dclk_idx = 2;
		filter_mode = 4;
		pdm_force_sysclk_to_oscin(true, pdm_id, p_pdm->chipinfo->vad_top);
		if (vad_pdm_is_running())
			vad_set_lowerpower_mode(true);

	} else {
		dclk_idx = p_pdm->dclk_idx;
		filter_mode = p_pdm->filter_mode;
	}

	/* filter for pdm */
	osr = pdm_get_ors(dclk_idx, runtime->rate);
	if (!osr) {
		dev_err(p_pdm->dev,
			"Not support osr for dclk:%d, rate:%d\n",
			pdm_dclkidx2rate(dclk_idx),
			runtime->rate
			);
		return -EINVAL;
	}

	pr_info("%s, pdm_dclk:%d, osr:%d, rate:%d filter mode:%d\n",
		__func__,
		pdm_dclkidx2rate(dclk_idx),
		osr,
		runtime->rate,
		p_pdm->filter_mode);

	info.bitdepth   = bitwidth;
	info.channels   = runtime->channels;
	info.lane_masks = p_pdm->lane_mask_in;
	info.dclk_idx   = dclk_idx;
	info.bypass     = p_pdm->bypass;
	info.sample_count = pdm_get_sample_count(p_pdm->islowpower, dclk_idx);

	aml_pdm_ctrl(&info, pdm_id);
	aml_pdm_filter_ctrl(p_pdm->pdm_gain_index, osr, filter_mode, pdm_id);

	if (p_pdm->chipinfo && p_pdm->chipinfo->truncate_data)
		pdm_init_truncate_data(runtime->rate, pdm_id);

	return 0;
}

static int aml_pdm_dai_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *cpu_dai)
{
	struct aml_pdm *p_pdm = snd_soc_dai_get_drvdata(cpu_dai);
	bool toddr_stopped = false;
	int id = p_pdm->chipinfo->id;
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		dev_info(substream->pcm->card->dev, "PDM Capture start\n");
		if (p_pdm->pdm_trigger_state == TRIGGER_START_VAD_BUF) {
			/* VAD switch to alsa buffer */
			vad_update_buffer(false);
			audio_toddr_irq_enable(p_pdm->tddr, true);
		} else {
			pdm_fifo_reset(id);
			aml_toddr_enable(p_pdm->tddr, 1);
			pdm_enable(1, id);
		}
		p_pdm->pdm_trigger_state = TRIGGER_START_ALSA_BUF;
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		dev_info(substream->pcm->card->dev, "PDM Capture stop\n");
		if (vad_pdm_is_running() && pm_audio_is_suspend() &&
		    p_pdm->pdm_trigger_state == TRIGGER_START_ALSA_BUF) {
			/* switch to VAD buffer */
			vad_update_buffer(true);
			audio_toddr_irq_enable(p_pdm->tddr, false);
			p_pdm->pdm_trigger_state = TRIGGER_START_VAD_BUF;
			break;
		}
		if (p_pdm->pdm_trigger_state == TRIGGER_STOP)
			break;
		pdm_enable(0, id);
		toddr_stopped = aml_toddr_burst_finished(p_pdm->tddr);
		if (toddr_stopped)
			aml_toddr_enable(p_pdm->tddr, false);
		else
			pr_err("%s(), toddr may be stuck\n", __func__);
		p_pdm->pdm_trigger_state = TRIGGER_STOP;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int aml_pdm_dai_set_sysclk(struct snd_soc_dai *cpu_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct aml_pdm *p_pdm = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int sysclk_srcpll_freq, dclk_srcpll_freq;
	unsigned int dclk_idx = p_pdm->dclk_idx;
	char *clk_name = NULL;
	int ret = 0;
	/* lowpower, force dclk to 768k */
	if (p_pdm->islowpower)
		dclk_idx = 2;

	sysclk_srcpll_freq = clk_get_rate(p_pdm->sysclk_srcpll);
	dclk_srcpll_freq = clk_get_rate(p_pdm->dclk_srcpll);
	clk_set_rate(p_pdm->clk_pdm_sysclk, 133333351);

	clk_name = (char *)__clk_get_name(p_pdm->dclk_srcpll);
	if (!strcmp(clk_name, "hifi_pll") || !strcmp(clk_name, "t5_hifi_pll")) {
		pr_info("%s:set hifi pll\n", __func__);
		if (p_pdm->syssrc_clk_rate)
			clk_set_rate(p_pdm->dclk_srcpll, p_pdm->syssrc_clk_rate);
		else
			clk_set_rate(p_pdm->dclk_srcpll, 1806336 * 1000);
	} else {
		if (dclk_srcpll_freq == 0)
			clk_set_rate(p_pdm->dclk_srcpll, 24576000 * 20);
	}
	clk_set_rate(p_pdm->clk_pdm_dclk,
			pdm_dclkidx2rate(dclk_idx));

	ret = clk_prepare_enable(p_pdm->clk_pdm_dclk);
	if (ret) {
		pr_err("Can't enable pcm clk_pdm_dclk clock: %d\n", ret);
		return -EINVAL;
	}

	p_pdm->clk_on = true;

	pr_info("\n%s, pdm_sysclk:%lu pdm_dclk:%lu, dclk_srcpll:%lu\n",
		__func__,
		clk_get_rate(p_pdm->clk_pdm_sysclk),
		clk_get_rate(p_pdm->clk_pdm_dclk),
		clk_get_rate(p_pdm->dclk_srcpll));

	return 0;
}

static int aml_pdm_dai_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *cpu_dai)
{
	unsigned int rate = params_rate(params);

	snd_soc_dai_set_sysclk(cpu_dai, 0, rate, SND_SOC_CLOCK_OUT);

	return 0;
}

int aml_pdm_dai_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct aml_pdm *p_pdm = snd_soc_dai_get_drvdata(cpu_dai);
	int ret;

	/* enable clock gate */
	ret = clk_prepare_enable(p_pdm->clk_gate);

	/* enable clock */
	ret = clk_prepare_enable(p_pdm->sysclk_srcpll);
	if (ret) {
		pr_err("Can't enable pcm sysclk_srcpll clock: %d\n", ret);
		goto err;
	}

	ret = clk_prepare_enable(p_pdm->dclk_srcpll);
	if (ret) {
		pr_err("Can't enable pcm dclk_srcpll clock: %d\n", ret);
		goto err;
	}

	ret = clk_prepare_enable(p_pdm->clk_pdm_sysclk);
	if (ret) {
		pr_err("Can't enable pcm clk_pdm_sysclk clock: %d\n", ret);
		goto err;
	}

	return 0;
err:
	pr_err("failed enable clock\n");
	return -EINVAL;
}

void aml_pdm_dai_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct aml_pdm *p_pdm = snd_soc_dai_get_drvdata(cpu_dai);
	int id = p_pdm->chipinfo->id;
	p_pdm->clk_on = false;
	p_pdm->rate = 0;

	if (p_pdm->islowpower) {
		pdm_force_sysclk_to_oscin(false, id, p_pdm->chipinfo->vad_top);
		vad_set_lowerpower_mode(false);
	}

	/* disable clock and gate */
	clk_disable_unprepare(p_pdm->clk_pdm_dclk);
	clk_disable_unprepare(p_pdm->clk_pdm_sysclk);
	clk_disable_unprepare(p_pdm->sysclk_srcpll);
	clk_disable_unprepare(p_pdm->dclk_srcpll);
	clk_disable_unprepare(p_pdm->clk_gate);
}

static const struct snd_soc_dai_ops aml_pdm_dai_ops = {
	.prepare     = aml_pdm_dai_prepare,
	.trigger     = aml_pdm_dai_trigger,
	.hw_params   = aml_pdm_dai_hw_params,
	.set_sysclk  = aml_pdm_dai_set_sysclk,
	.startup     = aml_pdm_dai_startup,
	.shutdown    = aml_pdm_dai_shutdown,
};

struct snd_soc_dai_driver aml_pdm_dai[] = {
	{
		.name = "PDM",
		.capture = {
			.channels_min =	PDM_CHANNELS_MIN,
			.channels_max = PDM_CHANNELS_LB_MAX,
			.rates        = PDM_RATES,
			.formats      = PDM_FORMATS,
		},
		.ops     = &aml_pdm_dai_ops,
	},
	{
		.name = "PDM_B",
		.capture = {
			.channels_min =	PDM_CHANNELS_MIN,
			.channels_max = PDM_CHANNELS_LB_MAX,
			.rates        = PDM_RATES,
			.formats      = PDM_FORMATS,
		},
		.ops     = &aml_pdm_dai_ops,
	},
};
EXPORT_SYMBOL(aml_pdm_dai);

static const struct snd_soc_component_driver aml_pdm_component[] = {
	{
		.name = DRV_NAME,
		.ops = &aml_pdm_ops,
		.controls = snd_pdm_controls,
		.num_controls = ARRAY_SIZE(snd_pdm_controls),
	},
	{
		.name = DRV_NAME_B,
		.ops = &aml_pdm_ops,
		.controls = snd_pdmb_controls,
		.num_controls = ARRAY_SIZE(snd_pdmb_controls),
	},
};

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void pdm_platform_early_suspend(struct early_suspend *h)
{
	struct platform_device *pdev = h->param;

	if (pdev) {
		struct aml_pdm *p_pdm = dev_get_drvdata(&pdev->dev);

		if (!p_pdm)
			return;

		p_pdm->vad_hibernation = true;
	}
}

static void pdm_platform_late_resume(struct early_suspend *h)
{
	struct platform_device *pdev = h->param;

	if (pdev) {
		struct aml_pdm *p_pdm = dev_get_drvdata(&pdev->dev);

		if (!p_pdm)
			return;

		p_pdm->vad_hibernation    = false;
		p_pdm->vad_buf_occupation = false;
		p_pdm->vad_buf_recovery   = false;
	}
};

static struct early_suspend pdm_platform_early_suspend_handler[] = {
	{
		.suspend = pdm_platform_early_suspend,
		.resume  = pdm_platform_late_resume,
	},
	{
		.suspend = pdm_platform_early_suspend,
		.resume  = pdm_platform_late_resume,
	},
};
#endif

static void aml_pdm_train_debug_work(struct work_struct *debug)
{
	struct aml_pdm *p_pdm;
	int ret;
	p_pdm = container_of(debug, struct aml_pdm, debug_work);
	if (p_pdm->pdm_train_debug) {
		ret = pdm_auto_train_algorithm(p_pdm->pdm_id, p_pdm->pdm_train_debug);
		if (ret > 0)
			pr_info("pdm train sample count = %d\n", ret);
	}
	p_pdm->pdm_train_debug = 0;
}

static int aml_pdm_platform_probe(struct platform_device *pdev)
{
	struct aml_pdm *p_pdm;
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent = NULL;
	struct aml_audio_controller *actrl = NULL;
	struct device *dev = &pdev->dev;
	struct pdm_chipinfo *p_chipinfo;
	int ret;
	int pdm_id = 0;

	p_pdm = devm_kzalloc(&pdev->dev, sizeof(struct aml_pdm), GFP_KERNEL);
	if (!p_pdm) {
		/*dev_err(&pdev->dev, "Can't allocate pcm_p\n");*/
		ret = -ENOMEM;
		goto err;
	}

	ret = of_property_read_u32(dev->of_node, "src-clk-freq",
				   &p_pdm->syssrc_clk_rate);
	if (ret < 0)
		p_pdm->syssrc_clk_rate = 0;
	pr_info("%s sys-src clk rate from dts:%d\n",
		__func__, p_pdm->syssrc_clk_rate);

	/* match data */
	p_chipinfo = (struct pdm_chipinfo *)
		of_device_get_match_data(dev);
	if (!p_chipinfo) {
		dev_warn_once(dev, "check whether to update pdm chipinfo\n");
		return -EINVAL;
	}
	p_pdm->chipinfo = p_chipinfo;
	p_pdm->pdm_id = p_chipinfo->id;
	pdm_id = p_pdm->pdm_id;
	/* get audio controller */
	node_prt = of_get_parent(node);
	if (!node_prt)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	if (!pdev_parent)
		return -ENXIO;
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
				platform_get_drvdata(pdev_parent);
	p_pdm->actrl = actrl;
	/* clock gate */
	p_pdm->clk_gate = devm_clk_get(&pdev->dev, "gate");
	if (IS_ERR(p_pdm->clk_gate)) {
		dev_err(&pdev->dev,
			"Can't get pdm gate\n");
		return PTR_ERR(p_pdm->clk_gate);
	}

	/* pinmux */
	p_pdm->pdm_pins = devm_pinctrl_get_select(&pdev->dev, "pdm_pins");
	if (IS_ERR(p_pdm->pdm_pins)) {
		p_pdm->pdm_pins = NULL;
		dev_warn(&pdev->dev, "Can't get pdm pinmux\n");
	}

	p_pdm->sysclk_srcpll = devm_clk_get(&pdev->dev, "sysclk_srcpll");
	if (IS_ERR(p_pdm->sysclk_srcpll)) {
		dev_err(&pdev->dev,
			"Can't retrieve pll clock\n");
		ret = PTR_ERR(p_pdm->sysclk_srcpll);
		goto err;
	}

	p_pdm->dclk_srcpll = devm_clk_get(&pdev->dev, "dclk_srcpll");
	if (IS_ERR(p_pdm->dclk_srcpll)) {
		dev_err(&pdev->dev,
			"Can't retrieve data src clock\n");
		ret = PTR_ERR(p_pdm->dclk_srcpll);
		goto err;
	}

	p_pdm->clk_pdm_sysclk = devm_clk_get(&pdev->dev, "pdm_sysclk");
	if (IS_ERR(p_pdm->clk_pdm_sysclk)) {
		dev_err(&pdev->dev,
			"Can't retrieve clk_pdm_sysclk clock\n");
		ret = PTR_ERR(p_pdm->clk_pdm_sysclk);
		goto err;
	}

	p_pdm->clk_pdm_dclk = devm_clk_get(&pdev->dev, "pdm_dclk");
	if (IS_ERR(p_pdm->clk_pdm_dclk)) {
		dev_err(&pdev->dev,
			"Can't retrieve clk_pdm_dclk clock\n");
		ret = PTR_ERR(p_pdm->clk_pdm_dclk);
		goto err;
	}

	ret = clk_set_parent(p_pdm->clk_pdm_sysclk, p_pdm->sysclk_srcpll);
	if (ret) {
		dev_err(&pdev->dev,
			"Can't set clk_pdm_sysclk parent clock\n");
		ret = PTR_ERR(p_pdm->clk_pdm_sysclk);
		goto err;
	}

	ret = clk_set_parent(p_pdm->clk_pdm_dclk, p_pdm->dclk_srcpll);
	if (ret) {
		dev_err(&pdev->dev,
			"Can't set clk_pdm_dclk parent clock\n");
		ret = PTR_ERR(p_pdm->clk_pdm_dclk);
		goto err;
	}

	ret = snd_soc_of_get_slot_mask(node, "lane-mask-in",
				       &p_pdm->lane_mask_in);
	if (!ret) {
		pr_warn("default set lane_mask_in as all lanes.\n");
		p_pdm->lane_mask_in = 0xf;
	}

	ret = of_property_read_u32(node, "filter_mode", &p_pdm->filter_mode);
	if (ret < 0) {
		/* default set 1 */
		p_pdm->filter_mode = 1;
	}
	pr_debug("%s pdm filter mode from dts:%d\n",
		__func__, p_pdm->filter_mode);

	ret = of_property_read_u32(node, "train_sample_count",
			&p_pdm->train_sample_count);
	if (ret < 0)
		p_pdm->train_sample_count = -1;
	pr_info("%s pdm train sample count from dts:%d\n",
		__func__, p_pdm->train_sample_count);

	if (p_pdm->chipinfo->regulator) {
		p_pdm->regulator_vcc3v3 = devm_regulator_get(dev, "pdm3v3");
		ret = PTR_ERR_OR_ZERO(p_pdm->regulator_vcc3v3);
		if (ret) {
			if (ret == -EPROBE_DEFER) {
				dev_err(&pdev->dev, "regulator pdm3v3 not ready, retry\n");
				return ret;
			}
			dev_err(&pdev->dev, "failed in regulator pdm3v3 %ld\n",
				PTR_ERR(p_pdm->regulator_vcc3v3));
			p_pdm->regulator_vcc3v3 = NULL;
		} else {
			ret = regulator_enable(p_pdm->regulator_vcc3v3);
			if (ret) {
				dev_err(&pdev->dev,
					"regulator pdm3v3 enable failed:   %d\n", ret);
				p_pdm->regulator_vcc3v3 = NULL;
			}
		}
		p_pdm->regulator_vcc5v = devm_regulator_get(dev, "pdm5v");
		ret = PTR_ERR_OR_ZERO(p_pdm->regulator_vcc5v);
		if (ret) {
			if (ret == -EPROBE_DEFER) {
				dev_err(&pdev->dev, "regulator pdm5v not ready, retry\n");
				return ret;
			}
			dev_err(&pdev->dev, "failed in regulator pdm5v %ld\n",
				PTR_ERR(p_pdm->regulator_vcc5v));
			p_pdm->regulator_vcc5v = NULL;
		} else {
			ret = regulator_enable(p_pdm->regulator_vcc5v);
			if (ret) {
				dev_err(&pdev->dev,
					"regulator pdm5v enable failed:   %d\n", ret);
				p_pdm->regulator_vcc5v = NULL;
			}
		}
	}

	s_pdm = p_pdm;

	p_pdm->dev = dev;
	dev_set_drvdata(&pdev->dev, p_pdm);

	/*config ddr arb */
	aml_pdm_arb_config(p_pdm->actrl, p_pdm->chipinfo->use_arb);
	INIT_WORK(&p_pdm->debug_work, aml_pdm_train_debug_work);

	ret = devm_snd_soc_register_component(&pdev->dev,
					      &aml_pdm_component[p_pdm->pdm_id],
					      &aml_pdm_dai[p_pdm->pdm_id], 1);

	if (ret) {
		dev_err(&pdev->dev, "failed to register ASoC DAI\n");
		goto err;
	}

	pr_debug("%s, register soc platform\n", __func__);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	pdm_platform_early_suspend_handler[p_pdm->pdm_id].param = pdev;
	register_early_suspend(&pdm_platform_early_suspend_handler[pdm_id]);
#endif
	return 0;

err:
	return ret;

}

static int aml_pdm_platform_remove(struct platform_device *pdev)
{
	struct aml_pdm *p_pdm = dev_get_drvdata(&pdev->dev);
	int pdm_id = p_pdm->pdm_id;
	clk_disable_unprepare(p_pdm->sysclk_srcpll);
	clk_disable_unprepare(p_pdm->clk_pdm_dclk);

	if (!IS_ERR_OR_NULL(p_pdm->regulator_vcc5v))
		regulator_disable(p_pdm->regulator_vcc5v);
	if (!IS_ERR_OR_NULL(p_pdm->regulator_vcc3v3))
		regulator_disable(p_pdm->regulator_vcc3v3);

	snd_soc_unregister_component(&pdev->dev);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	register_early_suspend(&pdm_platform_early_suspend_handler[pdm_id]);
#endif

	return 0;
}

static int pdm_platform_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct aml_pdm *p_pdm = dev_get_drvdata(&pdev->dev);
	int id = p_pdm->chipinfo->id;
	/* whether in freeze */
	if (is_pm_s2idle_mode() && vad_pdm_is_running()) {
		if (p_pdm->chipinfo->oscin_divide && !p_pdm->islowpower) {
			p_pdm->force_lowpower = true;
			pdm_set_lowpower_mode(p_pdm, p_pdm->force_lowpower, id);
		}
		pr_info("%s, PDM suspend in lowpower mode by force:%d\n",
			__func__,
			p_pdm->force_lowpower);
	}

	if (!IS_ERR_OR_NULL(p_pdm->regulator_vcc5v))
		regulator_disable(p_pdm->regulator_vcc5v);
	if (!IS_ERR_OR_NULL(p_pdm->regulator_vcc3v3))
		regulator_disable(p_pdm->regulator_vcc3v3);

	return 0;
}

static int pdm_platform_resume(struct platform_device *pdev)
{
	struct aml_pdm *p_pdm = dev_get_drvdata(&pdev->dev);
	int id = p_pdm->chipinfo->id;
	int ret = 0;
	/* whether in freeze mode */
	if (is_pm_s2idle_mode() && vad_pdm_is_running()) {
		pr_info("%s, PDM resume by force_lowpower:%d\n",
			__func__,
			p_pdm->force_lowpower);
		if (p_pdm->chipinfo->oscin_divide && p_pdm->force_lowpower) {
			p_pdm->force_lowpower = false;
			pdm_set_lowpower_mode(p_pdm, p_pdm->force_lowpower, id);
		}
	}

	if (!IS_ERR_OR_NULL(p_pdm->regulator_vcc5v))
		ret = regulator_enable(p_pdm->regulator_vcc5v);
	if (ret)
		dev_err(&pdev->dev, "regulator pdm5v enable failed:   %d\n", ret);

	if (!IS_ERR_OR_NULL(p_pdm->regulator_vcc3v3))
		ret = regulator_enable(p_pdm->regulator_vcc3v3);
	if (ret)
		dev_err(&pdev->dev, "regulator pdm3v3 enable failed:   %d\n", ret);

	return 0;
}

static void pdm_platform_shutdown(struct platform_device *pdev)
{
	struct aml_pdm *p_pdm = dev_get_drvdata(&pdev->dev);
	int id = p_pdm->chipinfo->id;

	pr_info("%s\n", __func__);

	/* whether in freeze */
	if (/* is_pm_freeze_mode() && */vad_pdm_is_running()) {
		if (!p_pdm->islowpower) {
			p_pdm->force_lowpower = true;
			pdm_set_lowpower_mode(p_pdm, p_pdm->force_lowpower, id);
		}
		pr_info("%s, PDM suspend in lowpower mode by force:%d\n",
			__func__,
			p_pdm->force_lowpower);
	}
}

struct platform_driver aml_pdm_driver = {
	.driver  = {
		.name           = DRV_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = of_match_ptr(aml_pdm_device_id),
	},
	.probe   = aml_pdm_platform_probe,
	.remove  = aml_pdm_platform_remove,
	.suspend = pdm_platform_suspend,
	.resume  = pdm_platform_resume,
	.shutdown = pdm_platform_shutdown,
};

int __init pdm_init(void)
{
	return platform_driver_register(&(aml_pdm_driver));
}

void __exit pdm_exit(void)
{
	platform_driver_unregister(&aml_pdm_driver);
}

#ifndef MODULE
module_init(pdm_init);
module_exit(pdm_exit);
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_DESCRIPTION("Amlogic PDM ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
#endif
