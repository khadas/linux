/*
 * sound/soc/amlogic/auge/earc.c
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
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
 * Audio External Input/Out drirver
 * such as fratv, frhdmirx
 */
#define DEBUG

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/extcon.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <linux/amlogic/media/sound/hdmi_earc.h>
#include <linux/amlogic/media/sound/mixer.h>
#include "ddr_mngr.h"
#include "earc_hw.h"

#define DRV_NAME "EARC"

struct earc {
	struct aml_audio_controller *actrl;
	struct device *dev;

	struct clk *clk_rx_gate;
	struct clk *clk_rx_cmdc;
	struct clk *clk_rx_dmac;
	struct clk *clk_rx_cmdc_srcpll;
	struct clk *clk_rx_dmac_srcpll;
	struct clk *clk_tx_gate;
	struct clk *clk_tx_cmdc;
	struct clk *clk_tx_dmac;
	struct clk *clk_tx_cmdc_srcpll;
	struct clk *clk_tx_dmac_srcpll;

	struct regmap *tx_cmdc_map;
	struct regmap *tx_dmac_map;
	struct regmap *tx_top_map;
	struct regmap *rx_cmdc_map;
	struct regmap *rx_dmac_map;
	struct regmap *rx_top_map;

	struct toddr *tddr;
	struct frddr *fddr;

	int irq_earc_rx;
	int irq_earc_tx;

	/* external connect */
	struct extcon_dev *rx_edev;
	struct extcon_dev *tx_edev;

	bool rx_dmac_clk_on;
	bool tx_dmac_clk_on;
};

static struct earc *s_earc;

#define PREALLOC_BUFFER_MAX	(256 * 1024)

#define EARC_RATES      (SNDRV_PCM_RATE_8000_192000)
#define EARC_FORMATS    (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S24_LE |\
			SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_pcm_hardware earc_hardware = {
	.info =
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_PAUSE,

	.formats = EARC_FORMATS,

	.period_bytes_min = 64,
	.period_bytes_max = 128 * 1024,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = 256 * 1024,

	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 1,
	.channels_max = 32,
};

static irqreturn_t earc_ddr_isr(int irq, void *data)
{
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)data;

	if (!snd_pcm_running(substream))
		return IRQ_HANDLED;

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}

static void earcrx_update_attend_event(struct earc *p_earc,
				       bool is_earc, bool state)
{
	if (state) {
		if (is_earc) {
			extcon_set_state_sync(p_earc->rx_edev,
				EXTCON_EARCRX_ATNDTYP_ARC, false);
			extcon_set_state_sync(p_earc->rx_edev,
				EXTCON_EARCRX_ATNDTYP_EARC, state);
		} else {
			extcon_set_state_sync(p_earc->rx_edev,
				EXTCON_EARCRX_ATNDTYP_ARC, state);
			extcon_set_state_sync(p_earc->rx_edev,
				EXTCON_EARCRX_ATNDTYP_EARC, false);
		}
	} else {
		extcon_set_state_sync(p_earc->rx_edev,
			EXTCON_EARCRX_ATNDTYP_ARC, state);
		extcon_set_state_sync(p_earc->rx_edev,
			EXTCON_EARCRX_ATNDTYP_EARC, state);
	}
}

static irqreturn_t earc_rx_isr(int irq, void *data)
{
	struct earc *p_earc = (struct earc *)data;
	unsigned int status0 = earcrx_cdmc_get_irqs(p_earc->rx_top_map);

	if (status0)
		earcrx_cdmc_clr_irqs(p_earc->rx_top_map, status0);

	if (status0 & INT_EARCRX_CMDC_TIMEOUT) {
		earcrx_update_attend_event(p_earc,
					   false, false);

		pr_debug("%s EARCRX_CMDC_TIMEOUT\n", __func__);
	}

	if (status0 & INT_EARCRX_CMDC_IDLE2) {
		earcrx_update_attend_event(p_earc,
					   false, true);

		pr_info("%s EARCRX_CMDC_IDLE2\n", __func__);
	}
	if (status0 & INT_EARCRX_CMDC_IDLE1) {
		earcrx_update_attend_event(p_earc,
					   false, false);

		pr_info("%s EARCRX_CMDC_IDLE1\n", __func__);
	}
	if (status0 & INT_EARCRX_CMDC_DISC2)
		pr_debug("%s EARCRX_CMDC_DISC2\n", __func__);
	if (status0 & INT_EARCRX_CMDC_DISC1)
		pr_debug("%s EARCRX_CMDC_DISC1\n", __func__);
	if (status0 & INT_EARCRX_CMDC_EARC) {
		earcrx_update_attend_event(p_earc,
					   true, true);

		pr_info("%s EARCRX_CMDC_EARC\n", __func__);
	}
	/*
	 * if (status0 & INT_EARCRX_CMDC_HB_STATUS)
	 *	pr_debug("%s EARCRX_CMDC_HB_STATUS\n", __func__);
	 */
	if (status0 & INT_EARCRX_CMDC_LOSTHB)
		pr_debug("%s EARCRX_CMDC_LOSTHB\n", __func__);

	if (p_earc->rx_dmac_clk_on) {
		unsigned int status1 = earcrx_dmac_get_irqs(p_earc->rx_top_map);

		if (status1)
			earcrx_dmac_clr_irqs(p_earc->rx_top_map, status1);

		if (status1 & INT_ARCRX_BIPHASE_DECODE_C_FIND_PAPB)
			pr_debug("%s ARCRX_C_FIND_PAPB\n", __func__);
		if (status1 & INT_ARCRX_BIPHASE_DECODE_C_VALID_CHANGE)
			pr_debug("%s ARCRX_C_VALID_CHANGE\n", __func__);
		if (status1 & INT_ARCRX_BIPHASE_DECODE_C_FIND_NONPCM2PCM)
			pr_debug("%s ARCRX_C_FIND_NONPCM2PCM\n", __func__);
		if (status1 & INT_ARCRX_BIPHASE_DECODE_C_PCPD_CHANGE)
			pr_debug("%s ARCRX_C_PCPD_CHANGE\n", __func__);
		if (status1 & INT_ARCRX_BIPHASE_DECODE_C_CH_STATUS_CHANGE)
			pr_debug("%s ARCRX_C_CH_STATUS_CHANGE\n", __func__);
		if (status1 & INT_ARCRX_BIPHASE_DECODE_I_SAMPLE_MODE_CHANGE)
			pr_debug("%s ARCRX_I_SAMPLE_MODE_CHANGE\n", __func__);
		if (status1 & INT_ARCRX_BIPHASE_DECODE_R_PARITY_ERR)
			pr_debug("%s ARCRX_R_PARITY_ERR\n", __func__);
	}

	return IRQ_HANDLED;
}

static void earctx_update_attend_event(struct earc *p_earc,
				       bool is_earc, bool state)
{
	if (state) {
		if (is_earc) {
			extcon_set_state_sync(p_earc->tx_edev,
					      EXTCON_EARCTX_ATNDTYP_ARC,
					      false);
			extcon_set_state_sync(p_earc->tx_edev,
					      EXTCON_EARCTX_ATNDTYP_EARC,
					      state);
		} else {
			extcon_set_state_sync(p_earc->tx_edev,
					      EXTCON_EARCTX_ATNDTYP_ARC,
					      state);
			extcon_set_state_sync(p_earc->tx_edev,
					      EXTCON_EARCTX_ATNDTYP_EARC,
					      false);
		}
	} else {
		extcon_set_state_sync(p_earc->tx_edev,
				      EXTCON_EARCTX_ATNDTYP_ARC,
				      state);
		extcon_set_state_sync(p_earc->tx_edev,
				      EXTCON_EARCTX_ATNDTYP_EARC,
				      state);
	}
}

static irqreturn_t earc_tx_isr(int irq, void *data)
{
	struct earc *p_earc = (struct earc *)data;
	unsigned int status0 = earctx_cdmc_get_irqs(p_earc->tx_top_map);

	if (status0)
		earctx_cdmc_clr_irqs(p_earc->tx_top_map, status0);

	if (status0 & INT_EARCTX_CMDC_IDLE2) {
		earctx_update_attend_event(p_earc,
					   false, true);

		pr_debug("%s EARCTX_CMDC_IDLE2\n", __func__);
	}
	if (status0 & INT_EARCTX_CMDC_IDLE1) {
		earctx_update_attend_event(p_earc,
					   false, false);

		pr_debug("%s EARCTX_CMDC_IDLE1\n", __func__);
	}
	if (status0 & INT_EARCTX_CMDC_DISC2)
		pr_debug("%s EARCTX_CMDC_DISC2\n", __func__);
	if (status0 & INT_EARCTX_CMDC_DISC1)
		pr_debug("%s EARCTX_CMDC_DISC1\n", __func__);
	if (status0 & INT_EARCTX_CMDC_EARC) {
		earctx_update_attend_event(p_earc,
					   true, true);

		pr_info("%s EARCTX_CMDC_EARC\n", __func__);
	}
	if (status0 & INT_EARCTX_CMDC_HB_STATUS)
		pr_debug("%s EARCTX_CMDC_HB_STATUS\n", __func__);
	if (status0 & INT_EARCTX_CMDC_LOSTHB)
		pr_debug("%s EARCTX_CMDC_LOSTHB\n", __func__);
	if (status0 & INT_EARCTX_CMDC_TIMEOUT) {
		earctx_update_attend_event(p_earc,
					   false, false);

		pr_debug("%s EARCTX_CMDC_TIMEOUT\n", __func__);
	}
	if (status0 & INT_EARCTX_CMDC_STATUS_CH)
		pr_debug("%s EARCTX_CMDC_STATUS_CH\n", __func__);
	if (status0 & INT_EARCTX_CMDC_RECV_NACK)
		pr_debug("%s EARCTX_CMDC_RECV_NACK\n", __func__);
	if (status0 & INT_EARCTX_CMDC_RECV_NORSP)
		pr_debug("%s EARCTX_CMDC_RECV_NORSP\n", __func__);
	if (status0 & INT_EARCTX_CMDC_RECV_UNEXP)
		pr_debug("%s EARCTX_CMDC_RECV_UNEXP\n", __func__);

	if (p_earc->tx_dmac_clk_on) {
		unsigned int status1 = earctx_dmac_get_irqs(p_earc->tx_top_map);

		if (status1)
			earctx_dmac_clr_irqs(p_earc->tx_top_map, status1);

		if (status1 & INT_EARCTX_FEM_C_HOLD_CLR)
			pr_debug("%s EARCTX_FEM_C_HOLD_CLR\n", __func__);
		if (status1 & INT_EARCTX_FEM_C_HOLD_START)
			pr_debug("%s EARCTX_FEM_C_HOLD_START\n", __func__);
		if (status1 & INT_EARCTX_ERRCORR_C_FIFO_THD_LESS_PASS)
			pr_debug("%s EARCTX_ERRCORR_C_FIFO_THD_LESS_PASS\n",
				 __func__);
		if (status1 & INT_EARCTX_ERRCORR_C_FIFO_OVERFLOW)
			pr_debug("%s EARCTX_ERRCORR_C_FIFO_OVERFLOW\n",
				 __func__);
		if (status1 & INT_EARCTX_ERRCORR_C_FIFO_EMPTY)
			pr_debug("%s EARCTX_ERRCORR_C_FIFO_EMPTY\n", __func__);
	}

	return IRQ_HANDLED;
}

static int earc_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = rtd->platform->dev;
	struct earc *p_earc;

	pr_info("asoc debug: %s\n", __func__);

	p_earc = (struct earc *)dev_get_drvdata(dev);

	snd_soc_set_runtime_hwparams(substream, &earc_hardware);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		p_earc->fddr = aml_audio_register_frddr(dev,
			p_earc->actrl,
			earc_ddr_isr, substream, false);
		if (p_earc->fddr == NULL) {
			dev_err(dev, "failed to claim from ddr\n");
			return -ENXIO;
		}
	} else {
		p_earc->tddr = aml_audio_register_toddr(dev,
			p_earc->actrl,
			earc_ddr_isr, substream);
		if (p_earc->tddr == NULL) {
			dev_err(dev, "failed to claim to ddr\n");
			return -ENXIO;
		}
	}

	runtime->private_data = p_earc;

	return 0;
}

static int earc_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = runtime->private_data;

	pr_info("asoc debug: %s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		aml_audio_unregister_frddr(p_earc->dev, substream);
	else
		aml_audio_unregister_toddr(p_earc->dev, substream);

	runtime->private_data = NULL;

	return 0;
}

static int earc_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream,
			params_buffer_bytes(hw_params));
}

static int earc_hw_free(struct snd_pcm_substream *substream)
{
	snd_pcm_lib_free_pages(substream);

	return 0;
}

static int earc_trigger(struct snd_pcm_substream *substream, int cmd)
{
	return 0;
}

static int earc_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = runtime->private_data;
	unsigned int start_addr, end_addr, int_addr;
	unsigned int period, threshold;

	start_addr = runtime->dma_addr;
	end_addr = start_addr + runtime->dma_bytes - FIFO_BURST;
	period	 = frames_to_bytes(runtime, runtime->period_size);
	int_addr = period / FIFO_BURST;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_earc->fddr;

		/*
		 * Contrast minimum of period and fifo depth,
		 * and set the value as half.
		 */
		threshold = min(period, fr->fifo_depth);
		threshold /= 2;
		/* Use all the fifo */
		aml_frddr_set_fifos(fr, fr->fifo_depth, threshold);

		aml_frddr_set_buf(fr, start_addr, end_addr);
		aml_frddr_set_intrpt(fr, int_addr);
	} else {
		struct toddr *to = p_earc->tddr;

		/*
		 * Contrast minimum of period and fifo depth,
		 * and set the value as half.
		 */
		threshold = min(period, to->fifo_depth);
		threshold /= 2;
		aml_toddr_set_fifos(to, threshold);

		aml_toddr_set_buf(to, start_addr, end_addr);
		aml_toddr_set_intrpt(to, int_addr);
	}

	return 0;
}

static snd_pcm_uframes_t earc_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = runtime->private_data;
	unsigned int addr, start_addr;
	snd_pcm_uframes_t frames;

	start_addr = runtime->dma_addr;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		addr = aml_frddr_get_position(p_earc->fddr);
	else
		addr = aml_toddr_get_position(p_earc->tddr);

	frames = bytes_to_frames(runtime, addr - start_addr);
	if (frames > runtime->buffer_size)
		frames = 0;

	return frames;
}

int earc_silence(struct snd_pcm_substream *substream, int channel,
		    snd_pcm_uframes_t pos, snd_pcm_uframes_t count)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	char *ppos;
	int n;

	n = frames_to_bytes(runtime, count);
	ppos = runtime->dma_area + frames_to_bytes(runtime, pos);
	memset(ppos, 0, n);

	return 0;
}

static int earc_mmap(struct snd_pcm_substream *substream,
			struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static struct snd_pcm_ops earc_ops = {
	.open      = earc_open,
	.close     = earc_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = earc_hw_params,
	.hw_free   = earc_hw_free,
	.prepare   = earc_prepare,
	.trigger   = earc_trigger,
	.pointer   = earc_pointer,
	.silence   = earc_silence,
	.mmap      = earc_mmap,
};

static int earc_new(struct snd_soc_pcm_runtime *rtd)
{
	return snd_pcm_lib_preallocate_pages_for_all(
			rtd->pcm, SNDRV_DMA_TYPE_DEV,
			rtd->card->snd_card->dev,
			PREALLOC_BUFFER_MAX,
			PREALLOC_BUFFER_MAX);
}

struct snd_soc_platform_driver earc_platform = {
	.ops = &earc_ops,
	.pcm_new = earc_new,
};

static int earc_dai_probe(struct snd_soc_dai *cpu_dai)
{
	pr_info("asoc debug: %s\n", __func__);

	return 0;
}

static int earc_dai_remove(struct snd_soc_dai *cpu_dai)
{
	return 0;
}

static int earc_dai_prepare(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int bit_depth = snd_pcm_format_width(runtime->format);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = p_earc->fddr;
		enum frddr_dest dst = EARCTX_DMAC;
		unsigned int fifo_id, frddr_type = 0;
		struct iec958_chsts chsts;

		pr_info("%s Expected frddr dst:%s\n",
			__func__,
			frddr_src_get_str(dst));

		switch (bit_depth) {
		case 8:
			frddr_type = 0;
			break;
		case 16:
			frddr_type = 1;
			break;
		case 24:
			frddr_type = 4;
			break;
		case 32:
			frddr_type = 3;
			break;
		default:
			pr_err("runtime format invalid bitwidth: %d\n",
			       bit_depth);
			break;
		}
		fifo_id = aml_frddr_get_fifo_id(fr);

		pr_info("%s, frddr_index:%d, bit_depth:%d, frddr_type:%d\n",
			__func__,
			fr->fifo_id, bit_depth, frddr_type);

		aml_frddr_set_format(fr,
				     runtime->channels,
				     bit_depth - 1,
				     frddr_type);
		aml_frddr_select_dst(fr, dst);

		earctx_dmac_init(p_earc->tx_top_map, p_earc->tx_dmac_map);
		earctx_dmac_set_format(p_earc->tx_dmac_map,
				       fr->fifo_id,
				       bit_depth - 1,
				       frddr_type);

		/* check and set channel status info */
		spdif_get_channel_status_info(&chsts, runtime->rate);
		earctx_set_channel_status_info(p_earc->tx_dmac_map, &chsts);
	} else {
		struct toddr *to = p_earc->tddr;
		unsigned int msb = 0, lsb = 0, toddr_type = 0;
		unsigned int src = EARCRX_DMAC;
		struct toddr_fmt fmt;
		enum attend_type type =
			earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map);

		if (type == ATNDTYP_DISCNCT) {
			dev_err(p_earc->dev, "Neither eARC or ARC is attended!\n");
			return -ENOTCONN;
		}

		if (bit_depth == 32)
			toddr_type = 3;
		else if (bit_depth == 24)
			toddr_type = 4;
		else
			toddr_type = 0;

		msb = 28 - 1;
		if (bit_depth == 16)
			lsb = 28 - bit_depth;
		else
			lsb = 4;

		pr_debug("%s Expected toddr src:%s, m:%d, n:%d, toddr type:%d\n",
			__func__, toddr_src_get_str(src),
			msb, lsb, toddr_type);

		fmt.type      = toddr_type;
		fmt.msb       = msb;
		fmt.lsb       = lsb;
		fmt.endian    = 0;
		fmt.bit_depth = bit_depth;
		fmt.ch_num    = runtime->channels;
		fmt.rate      = runtime->rate;

		aml_toddr_select_src(to, src);
		aml_toddr_set_format(to, &fmt);

		earcrx_dmac_init(p_earc->rx_top_map, p_earc->rx_dmac_map);
		earcrx_arc_init(p_earc->rx_dmac_map);
	}

	return 0;
}

static int earc_dai_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "eARC/ARC TX enable\n");

			aml_frddr_enable(p_earc->fddr, true);
			earctx_enable(p_earc->tx_top_map,
				      p_earc->tx_cmdc_map,
				      p_earc->tx_dmac_map,
				      true);
		} else {
			dev_info(substream->pcm->card->dev, "eARC/ARC RX enable\n");

			aml_toddr_enable(p_earc->tddr, true);
			earcrx_enable(p_earc->rx_cmdc_map,
				      p_earc->rx_dmac_map,
				      true);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dev_info(substream->pcm->card->dev, "eARC/ARC TX disable\n");

			earctx_enable(p_earc->tx_top_map,
				      p_earc->tx_cmdc_map,
				      p_earc->tx_dmac_map,
				      false);
			aml_frddr_enable(p_earc->fddr, false);
		} else {
			dev_info(substream->pcm->card->dev, "eARC/ARC RX disable\n");

			earcrx_enable(p_earc->rx_cmdc_map,
				      p_earc->rx_dmac_map,
				      false);
			aml_toddr_enable(p_earc->tddr, false);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int earc_dai_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int rate = params_rate(params);
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		int freq = rate * 128 * 5;

		if (spdif_is_4x_clk()) {
			pr_debug("set 4x audio clk for 958\n");
			freq *= 4;
		} else {
			pr_debug("set normal 512 fs /4 fs\n");
		}

		clk_set_rate(p_earc->clk_tx_dmac_srcpll, freq * 4);
		clk_set_rate(p_earc->clk_tx_dmac, freq);

		pr_info("%s, tx dmac clk, set freq: %d, get freq:%lu\n",
			__func__,
			freq,
			clk_get_rate(p_earc->clk_tx_dmac));
	}

	return ret;
}

static int earc_dai_set_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);

	pr_info("asoc earc_dai_set_fmt, %#x, %p\n", fmt, p_earc);

	return 0;
}

static int earc_dai_set_sysclk(struct snd_soc_dai *cpu_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);

	pr_info("%s, %d, %d, %d\n",
		__func__, clk_id, freq, dir);

	if (clk_id == 1) {
		clk_set_rate(p_earc->clk_rx_dmac, 500000000);

		pr_info("earc rx cmdc clk:%lu rx dmac clk:%lu\n",
			clk_get_rate(p_earc->clk_rx_cmdc),
			clk_get_rate(p_earc->clk_rx_dmac));
	}

	return 0;
}

static int earc_dai_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);
	int ret;

	pr_info("%s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* tx dmac clk */
		if (!IS_ERR(p_earc->clk_tx_dmac)) {
			ret = clk_prepare_enable(p_earc->clk_tx_dmac);
			if (ret) {
				dev_err(p_earc->dev,
					"Can't enable earc clk_tx_dmac: %d\n",
					ret);
				goto err;
			}
			p_earc->tx_dmac_clk_on = true;
		}
		if (!IS_ERR(p_earc->clk_tx_dmac_srcpll)) {
			ret = clk_prepare_enable(p_earc->clk_tx_dmac_srcpll);
			if (ret) {
				dev_err(p_earc->dev,
					"Can't enable earc clk_tx_dmac_srcpll:%d\n",
					ret);
				goto err;
			}
		}
	} else {
		/* rx dmac clk */
		if (!IS_ERR(p_earc->clk_rx_dmac)) {
			ret = clk_prepare_enable(p_earc->clk_rx_dmac);
			if (ret) {
				dev_err(p_earc->dev,
					"Can't enable earc clk_rx_dmac: %d\n",
					ret);
				goto err;
			}
			p_earc->rx_dmac_clk_on = true;
		}

		if (!IS_ERR(p_earc->clk_rx_dmac_srcpll)) {
			ret = clk_prepare_enable(p_earc->clk_rx_dmac_srcpll);
			if (ret) {
				dev_err(p_earc->dev,
					"Can't enable earc clk_rx_dmac_srcpll: %d\n",
					ret);
				goto err;
			}
		}

		earcrx_pll_refresh(p_earc->rx_top_map);
	}

	return 0;
err:
	pr_err("failed enable clock\n");
	return -EINVAL;
}


static void earc_dai_shutdown(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct earc *p_earc = snd_soc_dai_get_drvdata(cpu_dai);

	pr_info("%s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (!IS_ERR(p_earc->clk_tx_dmac)) {
			clk_disable_unprepare(p_earc->clk_tx_dmac);
			p_earc->tx_dmac_clk_on = false;
		}
		if (!IS_ERR(p_earc->clk_tx_dmac_srcpll))
			clk_disable_unprepare(p_earc->clk_tx_dmac_srcpll);
	} else {
		if (!IS_ERR(p_earc->clk_rx_dmac)) {
			clk_disable_unprepare(p_earc->clk_rx_dmac);
			p_earc->rx_dmac_clk_on = false;
		}
		if (!IS_ERR(p_earc->clk_rx_dmac_srcpll))
			clk_disable_unprepare(p_earc->clk_rx_dmac_srcpll);
	}
}

static struct snd_soc_dai_ops earc_dai_ops = {
	.prepare    = earc_dai_prepare,
	.trigger    = earc_dai_trigger,
	.hw_params  = earc_dai_hw_params,
	.set_fmt    = earc_dai_set_fmt,
	.set_sysclk = earc_dai_set_sysclk,
	.startup	= earc_dai_startup,
	.shutdown	= earc_dai_shutdown,
};

static struct snd_soc_dai_driver earc_dai[] = {
	{
		.name     = "EARC/ARC",
		.id       = 0,
		.probe    = earc_dai_probe,
		.remove   = earc_dai_remove,
		.playback = {
		      .channels_min = 1,
		      .channels_max = 32,
		      .rates        = EARC_RATES,
		      .formats      = EARC_FORMATS,
		},
		.capture = {
		     .channels_min = 1,
		     .channels_max = 32,
		     .rates        = EARC_RATES,
		     .formats      = EARC_FORMATS,
		},
		.ops    = &earc_dai_ops,
	},
};


static const char *const attended_type[] = {
	"DISCONNECT",
	"ARC",
	"eARC"
};

const struct soc_enum attended_type_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(attended_type),
			attended_type);

int earcrx_get_attend_type(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum attend_type type =
		earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map);

	ucontrol->value.integer.value[0] = type;

	return 0;
}

int earcrx_set_attend_type(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum cmdc_st state = earcrx_cmdc_get_state(p_earc->rx_cmdc_map);

	if (state != CMDC_ST_IDLE2)
		return 0;

	/* only support set cmdc from idle to ARC */

	return 0;
}

static int earcrx_arc_get_enable(
				 struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum attend_type type =
		earcrx_cmdc_get_attended_type(p_earc->rx_cmdc_map);

	ucontrol->value.integer.value[0] = (bool)(type == ATNDTYP_ARC);

	return 0;
}

static int earcrx_arc_set_enable(
				 struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);

	if (!p_earc)
		return 0;

	earcrx_cmdc_arc_connect(
		p_earc->rx_cmdc_map,
		(bool)ucontrol->value.integer.value[0]);

	return 0;
}

int earctx_get_attend_type(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum attend_type type;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	type = earctx_cmdc_get_attended_type(p_earc->tx_cmdc_map);

	ucontrol->value.integer.value[0] = type;

	return 0;
}

int earctx_set_attend_type(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum cmdc_st state;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	state = earctx_cmdc_get_state(p_earc->tx_cmdc_map);

	if (state != CMDC_ST_IDLE2)
		return 0;

	/* only support set cmdc from idle to ARC */

	return 0;
}

int earcrx_get_latency(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum cmdc_st state;
	u8  val = 0;

	if (!p_earc || IS_ERR(p_earc->rx_top_map))
		return 0;

	state = earcrx_cmdc_get_state(p_earc->rx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earcrx_cmdc_get_latency(p_earc->rx_cmdc_map, &val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

int earcrx_set_latency(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	u8 latency = ucontrol->value.integer.value[0];
	enum cmdc_st state;

	if (!p_earc || IS_ERR(p_earc->rx_top_map))
		return 0;

	state = earcrx_cmdc_get_state(p_earc->rx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earcrx_cmdc_set_latency(p_earc->rx_cmdc_map, &latency);

	return 0;
}

int earcrx_get_cds(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	struct soc_bytes_ext *bytes_ext =
		(struct soc_bytes_ext *)kcontrol->private_value;
	u8 *value = (u8 *)ucontrol->value.bytes.data;
	enum cmdc_st state;
	int i;
	u8 data[256];

	if (!p_earc || IS_ERR(p_earc->rx_top_map))
		return 0;

	state = earcrx_cmdc_get_state(p_earc->rx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earcrx_cmdc_get_cds(p_earc->rx_cmdc_map, data);

	for (i = 0; i < bytes_ext->max; i++)
		*value++ = data[i];

	return 0;
}

int earcrx_set_cds(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	struct soc_bytes_ext *params = (void *)kcontrol->private_value;
	u8 *data;
	enum cmdc_st state;

	if (!p_earc || IS_ERR(p_earc->rx_top_map))
		return 0;

	state = earcrx_cmdc_get_state(p_earc->rx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	data = kmemdup(ucontrol->value.bytes.data,
		       params->max, GFP_KERNEL | GFP_DMA);
	if (!data)
		return -ENOMEM;

	earcrx_cmdc_set_cds(p_earc->rx_cmdc_map, data);

	kfree(data);

	return 0;
}

int earctx_get_latency(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	enum cmdc_st state;
	u8 val = 0;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	state = earctx_cmdc_get_state(p_earc->tx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earctx_cmdc_get_latency(p_earc->tx_cmdc_map, &val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

int earctx_set_latency(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	u8 latency = ucontrol->value.integer.value[0];
	enum cmdc_st state;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	state = earctx_cmdc_get_state(p_earc->tx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earctx_cmdc_set_latency(p_earc->tx_cmdc_map, &latency);

	return 0;
}

int earctx_get_cds(struct snd_kcontrol *kcontrol,
		   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct earc *p_earc = dev_get_drvdata(component->dev);
	struct soc_bytes_ext *bytes_ext =
		(struct soc_bytes_ext *)kcontrol->private_value;
	u8 *value = (u8 *)ucontrol->value.bytes.data;
	enum cmdc_st state;
	u8 data[256];
	int i;

	if (!p_earc || IS_ERR(p_earc->tx_top_map))
		return 0;

	state = earctx_cmdc_get_state(p_earc->tx_cmdc_map);
	if (state != CMDC_ST_EARC)
		return 0;

	earctx_cmdc_get_cds(p_earc->tx_cmdc_map, data);

	for (i = 0; i < bytes_ext->max; i++)
		*value++ = data[i];

	return 0;
}

static const struct snd_kcontrol_new earc_controls[] = {
	SOC_SINGLE_BOOL_EXT("HDMI ARC Switch",
			    0,
			    earcrx_arc_get_enable,
			    earcrx_arc_set_enable),

	SOC_ENUM_EXT("eARC_RX attended type",
		     attended_type_enum,
		     earcrx_get_attend_type,
		     earcrx_set_attend_type),

	SOC_ENUM_EXT("eARC_TX attended type",
		     attended_type_enum,
		     earctx_get_attend_type,
		     earctx_set_attend_type),

	SND_INT("eARC_RX Latency",
		earcrx_get_latency,
		earcrx_set_latency),

	SND_INT("eARC_TX Latency",
		earctx_get_latency,
		earctx_set_latency),

	SND_SOC_BYTES_EXT("eARC_RX CDS",
			  CDS_MAX_BYTES,
			  earcrx_get_cds,
			  earcrx_set_cds),

	SND_SOC_BYTES_EXT("eARC_TX CDS",
			  CDS_MAX_BYTES,
			  earctx_get_cds,
			  NULL),
};

static const struct snd_soc_component_driver earc_component = {
	.controls       = earc_controls,
	.num_controls   = ARRAY_SIZE(earc_controls),
	.name		= DRV_NAME,
};

static const struct of_device_id earc_device_id[] = {
	{
		.compatible = "amlogic, sm1-snd-earc",
	},
	{
		.compatible = "amlogic, tm2-snd-earc",
	},
	{},
};

MODULE_DEVICE_TABLE(of, earc_device_id);

static const unsigned int earcrx_extcon[] = {
	EXTCON_EARCRX_ATNDTYP_ARC,
	EXTCON_EARCRX_ATNDTYP_EARC,
	EXTCON_NONE,
};

static int earcrx_extcon_register(struct earc *p_earc)
{
	int ret = 0;

	/* earc or arc connect */
	p_earc->rx_edev = devm_extcon_dev_allocate(p_earc->dev, earcrx_extcon);
	if (IS_ERR(p_earc->rx_edev)) {
		pr_err("failed to allocate earc extcon!!!\n");
		ret = -ENOMEM;
		return ret;
	}
	p_earc->rx_edev->dev.parent  = p_earc->dev;
	p_earc->rx_edev->name = "earcrx";

	dev_set_name(&p_earc->rx_edev->dev, "earcrx");
	ret = extcon_dev_register(p_earc->rx_edev);
	if (ret < 0) {
		pr_err("earc extcon failed to register!!\n");
		return ret;
	}

	return ret;
}

void earc_hdmitx_hpdst(bool st)
{
	struct earc *p_earc = s_earc;

	if (!p_earc)
		return;

	pr_info("%s, %s\n",
		__func__,
		st ? "plugin" : "plugout");

	/* ensure clock gate */
	audiobus_update_bits(EE_AUDIO_CLK_GATE_EN1, 0x1 << 6, 0x1 << 6);

	earcrx_cmdc_arc_connect(p_earc->rx_cmdc_map, st);

	earcrx_cmdc_hpd_detect(p_earc->rx_cmdc_map, st);
}

static int earcrx_cmdc_setup(struct earc *p_earc)
{
	int ret = 0;

	/* set cmdc clk */
	audiobus_update_bits(EE_AUDIO_CLK_GATE_EN1, 0x1 << 6, 0x1 << 6);
	if (!IS_ERR(p_earc->clk_rx_cmdc)) {
		clk_set_rate(p_earc->clk_rx_cmdc, 10000000);

		ret = clk_prepare_enable(p_earc->clk_rx_cmdc);
		if (ret) {
			pr_err("Can't enable earc clk_rx_cmdc: %d\n", ret);
			return ret;
		}
	}

	/* rx cmdc init */
	earcrx_cmdc_init(p_earc->rx_top_map);
	/* Default: arc arc_initiated */
	earcrx_cmdc_arc_connect(p_earc->rx_cmdc_map, true);

	ret = devm_request_threaded_irq(p_earc->dev,
					p_earc->irq_earc_rx,
					NULL,
					earc_rx_isr,
					IRQF_TRIGGER_HIGH |
					IRQF_ONESHOT,
					"earc_rx",
					p_earc);
	if (ret) {
		dev_err(p_earc->dev, "failed to claim earc_rx %u\n",
			p_earc->irq_earc_rx);
		return ret;
	}

	return ret;
}

static const unsigned int earctx_extcon[] = {
	EXTCON_EARCTX_ATNDTYP_ARC,
	EXTCON_EARCTX_ATNDTYP_EARC,
	EXTCON_NONE,
};

static int earctx_extcon_register(struct earc *p_earc)
{
	int ret = 0;

	/* earc or arc connect */
	p_earc->tx_edev = devm_extcon_dev_allocate(p_earc->dev, earctx_extcon);
	if (IS_ERR(p_earc->tx_edev)) {
		pr_err("failed to allocate earc extcon!!!\n");
		ret = -ENOMEM;
		return ret;
	}
	p_earc->tx_edev->dev.parent  = p_earc->dev;
	p_earc->tx_edev->name = "earctx";

	dev_set_name(&p_earc->tx_edev->dev, "earctx");
	ret = extcon_dev_register(p_earc->tx_edev);
	if (ret < 0) {
		pr_err("earc extcon failed to register!!\n");
		return ret;
	}

	return ret;
}

void earc_hdmirx_hpdst(int earc_port, bool st)
{
	struct earc *p_earc = s_earc;

	if (!p_earc)
		return;

	pr_info("%s, earc_port:%d  %s\n",
		__func__,
		earc_port,
		st ? "plugin" : "plugout");

	earctx_cmdc_int_mask(p_earc->tx_top_map);
	earctx_cmdc_arc_connect(p_earc->tx_cmdc_map, st);
	earctx_cmdc_hpd_detect(p_earc->tx_top_map,
			       p_earc->tx_cmdc_map,
			       earc_port, st);
}

static int earctx_cmdc_setup(struct earc *p_earc)
{
	int ret = 0;

	/* set cmdc clk */
	audiobus_update_bits(EE_AUDIO_CLK_GATE_EN1, 0x1 << 5, 0x1 << 5);
	if (!IS_ERR(p_earc->clk_tx_cmdc)) {
		clk_set_rate(p_earc->clk_tx_cmdc, 10000000);

		ret = clk_prepare_enable(p_earc->clk_tx_cmdc);
		if (ret) {
			pr_err("Can't enable earc clk_tx_cmdc: %d\n", ret);
			return ret;
		}
	}

	ret = devm_request_threaded_irq(p_earc->dev,
					p_earc->irq_earc_tx,
					NULL,
					earc_tx_isr,
					IRQF_TRIGGER_HIGH |
					IRQF_ONESHOT,
					"earc_tx",
					p_earc);
	if (ret) {
		dev_err(p_earc->dev, "failed to claim earc_tx %u\n",
			p_earc->irq_earc_tx);
		return ret;
	}

	/* tx cmdc init */
	earctx_cmdc_init(p_earc->tx_top_map);
	/* Default: arc arc_initiated */
	earctx_cmdc_arc_connect(p_earc->tx_cmdc_map, true);

	return ret;
}

static int earc_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct device *dev = &pdev->dev;
	struct aml_audio_controller *actrl = NULL;
	struct earc *p_earc = NULL;
	int ret = 0;

	pr_info("%s\n", __func__);

	p_earc = devm_kzalloc(dev, sizeof(struct earc), GFP_KERNEL);
	if (!p_earc)
		return -ENOMEM;

	p_earc->dev = dev;
	dev_set_drvdata(dev, p_earc);

	/* get audio controller */
	node_prt = of_get_parent(node);
	if (node_prt == NULL)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
			platform_get_drvdata(pdev_parent);
	p_earc->actrl = actrl;

	p_earc->tx_cmdc_map = regmap_resource(dev, "tx_cmdc");
	if (!p_earc->tx_cmdc_map) {
		dev_err(dev,
			"Can't get earctx_cmdc regmap!!\n");
	}
	p_earc->tx_dmac_map = regmap_resource(dev, "tx_dmac");
	if (!p_earc->tx_dmac_map) {
		dev_err(dev,
			"Can't get earctx_dmac regmap!!\n");
	}
	p_earc->tx_top_map = regmap_resource(dev, "tx_top");
	if (!p_earc->tx_top_map) {
		dev_err(dev,
			"Can't get earctx_top regmap!!\n");
	}

	p_earc->rx_cmdc_map = regmap_resource(dev, "rx_cmdc");
	if (!p_earc->rx_cmdc_map) {
		dev_err(dev,
			"Can't get earcrx_cdmc regmap!!\n");
	}
	p_earc->rx_dmac_map = regmap_resource(dev, "rx_dmac");
	if (!p_earc->rx_dmac_map) {
		dev_err(dev,
			"Can't get earcrx_dmac regmap!!\n");
	}
	p_earc->rx_top_map = regmap_resource(dev, "rx_top");
	if (!p_earc->rx_top_map) {
		dev_err(dev,
			"Can't get earcrx_top regmap!!\n");
	}

	/* clock gate */
	p_earc->clk_rx_gate = devm_clk_get(&pdev->dev, "rx_gate");
	if (IS_ERR(p_earc->clk_rx_gate)) {
		dev_err(&pdev->dev,
			"Can't get earc gate\n");
		/*return PTR_ERR(p_earc->clk_rx_gate);*/
	}
	/* RX */
	p_earc->clk_rx_cmdc = devm_clk_get(&pdev->dev, "rx_cmdc");
	if (IS_ERR(p_earc->clk_rx_cmdc)) {
		dev_err(&pdev->dev,
			"Can't get clk_rx_cmdc\n");
		return PTR_ERR(p_earc->clk_rx_cmdc);
	}
	p_earc->clk_rx_dmac = devm_clk_get(&pdev->dev, "rx_dmac");
	if (IS_ERR(p_earc->clk_rx_dmac)) {
		dev_err(&pdev->dev,
			"Can't get clk_rx_dmac\n");
		return PTR_ERR(p_earc->clk_rx_dmac);
	}
	p_earc->clk_rx_cmdc_srcpll = devm_clk_get(&pdev->dev, "rx_cmdc_srcpll");
	if (IS_ERR(p_earc->clk_rx_cmdc_srcpll)) {
		dev_err(&pdev->dev,
			"Can't get clk_rx_cmdc_srcpll\n");
		return PTR_ERR(p_earc->clk_rx_cmdc_srcpll);
	}
	p_earc->clk_rx_dmac_srcpll = devm_clk_get(&pdev->dev, "rx_dmac_srcpll");
	if (IS_ERR(p_earc->clk_rx_dmac_srcpll)) {
		dev_err(&pdev->dev,
			"Can't get clk_rx_dmac_srcpll\n");
		return PTR_ERR(p_earc->clk_rx_dmac_srcpll);
	}
	ret = clk_set_parent(p_earc->clk_rx_cmdc, p_earc->clk_rx_cmdc_srcpll);
	if (ret) {
		dev_err(dev,
			"Can't set clk_rx_cmdc parent clock\n");
		ret = PTR_ERR(p_earc->clk_rx_cmdc);
		return ret;
	}
	ret = clk_set_parent(p_earc->clk_rx_dmac, p_earc->clk_rx_dmac_srcpll);
	if (ret) {
		dev_err(dev,
			"Can't set clk_rx_dmac parent clock\n");
		ret = PTR_ERR(p_earc->clk_rx_dmac);
		return ret;
	}

	/* TX */
	p_earc->clk_tx_cmdc = devm_clk_get(&pdev->dev, "tx_cmdc");
	if (IS_ERR(p_earc->clk_tx_cmdc)) {
		dev_err(&pdev->dev,
			"Check whether support eARC TX\n");
	}
	p_earc->clk_tx_dmac = devm_clk_get(&pdev->dev, "tx_dmac");
	if (IS_ERR(p_earc->clk_tx_dmac)) {
		dev_err(&pdev->dev,
			"Check whether support eARC TX\n");
	}
	p_earc->clk_tx_cmdc_srcpll = devm_clk_get(&pdev->dev, "tx_cmdc_srcpll");
	if (IS_ERR(p_earc->clk_tx_cmdc_srcpll)) {
		dev_err(&pdev->dev,
			"Check whether support eARC TX\n");
	}
	p_earc->clk_tx_dmac_srcpll = devm_clk_get(&pdev->dev, "tx_dmac_srcpll");
	if (IS_ERR(p_earc->clk_tx_dmac_srcpll)) {
		dev_err(&pdev->dev,
			"Check whether support eARC TX\n");
	}
	if (!IS_ERR(p_earc->clk_tx_cmdc) &&
		!IS_ERR(p_earc->clk_tx_cmdc_srcpll)) {
		ret = clk_set_parent(p_earc->clk_tx_cmdc,
				p_earc->clk_tx_cmdc_srcpll);
		if (ret) {
			dev_err(dev,
				"Can't set clk_tx_cmdc parent clock\n");
			ret = PTR_ERR(p_earc->clk_tx_cmdc);
			return ret;
		}
	}
	if (!IS_ERR(p_earc->clk_tx_dmac) &&
		!IS_ERR(p_earc->clk_tx_dmac_srcpll)) {
		ret = clk_set_parent(p_earc->clk_tx_dmac,
				p_earc->clk_tx_dmac_srcpll);
		if (ret) {
			dev_err(dev,
				"Can't set clk_tx_dmac parent clock\n");
			ret = PTR_ERR(p_earc->clk_tx_dmac);
			return ret;
		}
	}

	/* irqs */
	p_earc->irq_earc_rx =
		platform_get_irq_byname(pdev, "earc_rx");
	if (p_earc->irq_earc_rx < 0) {
		dev_err(dev, "platform get irq earc_rx failed\n");
		return p_earc->irq_earc_rx;
	}
	p_earc->irq_earc_tx =
		platform_get_irq_byname(pdev, "earc_tx");
	if (p_earc->irq_earc_tx < 0)
		dev_err(dev, "platform get irq earc_tx failed, Check whether support eARC TX\n");

	pr_info("%s, irq_earc_rx:%d, irq_earc_tx:%d\n",
		__func__, p_earc->irq_earc_rx, p_earc->irq_earc_tx);

	ret = snd_soc_register_component(&pdev->dev,
				&earc_component,
				earc_dai,
				ARRAY_SIZE(earc_dai));
	if (ret) {
		dev_err(&pdev->dev,
			"snd_soc_register_component failed\n");
		return ret;
	}

	s_earc = p_earc;

	/* RX */
	if (p_earc->irq_earc_rx > 0) {
		earcrx_extcon_register(p_earc);
		earcrx_cmdc_setup(p_earc);
	}

	/* TX */
	if (p_earc->irq_earc_tx > 0) {
		earctx_extcon_register(p_earc);
		earctx_cmdc_setup(p_earc);
	}

	pr_info("%s, register soc platform\n", __func__);

	return devm_snd_soc_register_platform(dev, &earc_platform);
}

struct platform_driver earc_driver = {
	.driver = {
		.name           = DRV_NAME,
		.of_match_table = earc_device_id,
	},
	.probe = earc_platform_probe,
};

static int __init earc_init(void)
{
	return platform_driver_register(&earc_driver);
}
arch_initcall_sync(earc_init);

MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic eARC/ARC TX/RX ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("Platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, earc_device_id);
