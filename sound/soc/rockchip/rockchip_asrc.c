// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ALSA SoC Audio Layer - Rockchip ASRC Controller driver
 *
 * Copyright (c) 2024 Rockchip Electronics Co. Ltd.
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <sound/core.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "rockchip_asrc.h"

/*
 * structure:
 * tx: memory->asrc->sai->codec
 * rx: codec->sai->asrc->memory
 * dma structure:
 * memory -> asrc dma_tx -> asrc tx_mem -> sai dma_tx -> codec
 * codec -> sai dma_rx -> asrc rx_mem -> asrc dma_rx -> memory
 *
 * So the asrc and other dai's dma driver must be added in the component.
 *
 * The asoc path with asrc should be prepared in rockchip_multicodecs.c
 */

#define DRV_NAME			"rockchip-asrc"
/* directions */
#define IN				0
#define OUT				1
#define MAXBURST_PER_FIFO		8
#define DEFAULT_SAMPLE_RATE		48000
#define ASRC_DEFAULT_CLK		200000000
#define CRU_LRCK_SOURCE_FREQ		49152000

/* rk3576 */
#define RK3576_SYS_GRF_SOC_CON9		0x24
#define ASRC0_4CH_SRC_SEL_SHIFT		0
#define ASRC0_4CH_SRC_SEL_MASK		(0x1f << (ASRC0_4CH_SRC_SEL_SHIFT + 16))
#define ASRC0_4CH_SRC_SEL(x)		(x << ASRC0_4CH_SRC_SEL_SHIFT)
#define ASRC0_4CH_DST_SEL_SHIFT		5
#define ASRC0_4CH_DST_SEL_MASK		(0x1f << (ASRC0_4CH_DST_SEL_SHIFT + 16))
#define ASRC0_4CH_DST_SEL(x)		(x << ASRC0_4CH_DST_SEL_SHIFT)
#define ASRC1_4CH_SRC_SEL_SHIFT		10
#define ASRC1_4CH_SRC_SEL_MASK		(0x1f << ASRC1_4CH_SRC_SEL_SHIFT)
#define ASRC1_4CH_SRC_SEL(x)		(x << (ASRC1_4CH_SRC_SEL_SHIFT + 16))

#define RK3576_SYS_GRF_SOC_CON10	0x28
#define ASRC1_4CH_DST_SEL_SHIFT		0
#define ASRC1_4CH_DST_SEL_MASK		(0x1f << (ASRC1_4CH_DST_SEL_SHIFT + 16))
#define ASRC1_4CH_DST_SEL(x)		(x << ASRC1_4CH_DST_SEL_SHIFT)
#define ASRC2_2CH_SRC_SEL_SHIFT		5
#define ASRC2_2CH_SRC_SEL_MASK		(0x1f << (ASRC2_2CH_SRC_SEL_SHIFT+16))
#define ASRC2_2CH_SRC_SEL(x)		(x << ASRC2_2CH_SRC_SEL_SHIFT)
#define ASRC2_2CH_DST_SEL_SHIFT		10
#define ASRC2_2CH_DST_SEL_MASK		(0x1f << (ASRC2_2CH_DST_SEL_SHIFT + 16))
#define ASRC2_2CH_DST_SEL(x)		(x << ASRC2_2CH_DST_SEL_SHIFT)

#define RK3576_SYS_GRF_SOC_CON11	0x2c
#define ASRC3_2CH_SRC_SEL_SHIFT		0
#define ASRC3_2CH_SRC_SEL_MASK		(0x1f << (ASRC3_2CH_SRC_SEL_SHIFT + 16))
#define ASRC3_2CH_SRC_SEL(x)		(x << ASRC3_2CH_SRC_SEL_SHIFT)
#define ASRC3_2CH_DST_SEL_SHIFT		5
#define ASRC3_2CH_DST_SEL_MASK		(0x1f << (ASRC3_2CH_DST_SEL_SHIFT + 16))
#define ASRC3_2CH_DST_SEL(x)		(x << ASRC3_2CH_DST_SEL_SHIFT)

#define RK3576_SRC_LRCK_FROM_SAI0	0x0
#define RK3576_SRC_LRCK_FROM_SAI1	0x1
#define RK3576_SRC_LRCK_FROM_SAI2	0x2
#define RK3576_SRC_LRCK_FROM_SAI3	0x3
#define RK3576_SRC_LRCK_FROM_SAI4	0x4
#define RK3576_SRC_LRCK_FROM_SAI5	0x5
#define RK3576_SRC_LRCK_FROM_SAI6	0x6
#define RK3576_SRC_LRCK_FROM_SAI7	0x7
#define RK3576_SRC_LRCK_FROM_SAI8	0x8
#define RK3576_SRC_LRCK_FROM_SAI9	0x9
#define RK3576_SRC_LRCK_FROM_PDM0	0xa
#define RK3576_SRC_LRCK_FROM_PDM1	0xb
#define RK3576_SRC_LRCK_FROM_SPDIF_RX0	0xc
#define RK3576_SRC_LRCK_FROM_SPDIF_RX1	0xd
#define RK3576_SRC_LRCK_FROM_SPDIF_RX2	0xe
#define RK3576_SRC_LRCK_FROM_CRU0	0xf
#define RK3576_SRC_LRCK_FROM_CRU1	0x10

#define RK3576_DST_LRCK_FROM_SAI0	0x0
#define RK3576_DST_LRCK_FROM_SAI1	0x1
#define RK3576_DST_LRCK_FROM_SAI2	0x2
#define RK3576_DST_LRCK_FROM_SAI3	0x3
#define RK3576_DST_LRCK_FROM_SAI4	0x4
#define RK3576_DST_LRCK_FROM_SAI5	0x5
#define RK3576_DST_LRCK_FROM_SAI6	0x6
#define RK3576_DST_LRCK_FROM_SAI7	0x7
#define RK3576_DST_LRCK_FROM_SAI8	0x8
#define RK3576_DST_LRCK_FROM_SAI9	0x9
#define RK3576_DST_LRCK_FROM_SPDIF_TX0	0xa
#define RK3576_DST_LRCK_FROM_SPDIF_TX1	0xb
#define RK3576_DST_LRCK_FROM_SPDIF_TX2	0xc
#define RK3576_DST_LRCK_FROM_SPDIF_TX3	0xd
#define RK3576_DST_LRCK_FROM_SPDIF_TX4	0xe
#define RK3576_DST_LRCK_FROM_SPDIF_TX5	0xf
#define RK3576_DST_LRCK_FROM_CRU0	0x10
#define RK3576_DST_LRCK_FROM_CRU1	0x11

#define RK3576_ASRC0			0x2a690000
#define RK3576_ASRC1			0x2a6a0000
#define RK3576_ASRC2			0x2a6b0000
#define RK3576_ASRC3			0x2a6c0000

#define DAI_ID_MEM			0
#define DAI_ID_SAI0			1
#define DAI_ID_SAI1			2
#define DAI_ID_SAI2			3
#define DAI_ID_SAI3			4
#define DAI_ID_SAI4			5
#define DAI_ID_SAI5			6
#define DAI_ID_SAI6			7
#define DAI_ID_SAI7			8
#define DAI_ID_SAI8			9
#define DAI_ID_SAI9			10
#define DAI_ID_SAI10			11
#define DAI_ID_SAI11			12
#define DAI_ID_SAI12			13
#define DAI_ID_SAI13			14
#define DAI_ID_SAI14			15
#define DAI_ID_SAI15			16
#define DAI_ID_ASRC0			17
#define DAI_ID_ASRC1			18
#define DAI_ID_ASRC2			19
#define DAI_ID_ASRC3			20
#define DAI_ID_ASRC4			21
#define DAI_ID_ASRC5			22
#define DAI_ID_ASRC6			23
#define DAI_ID_ASRC7			24
#define DAI_ID_ASRC8			25
#define DAI_ID_ASRC9			26
#define DAI_ID_ASRC10			27
#define DAI_ID_ASRC11			28
#define DAI_ID_ASRC12			29
#define DAI_ID_ASRC13			30
#define DAI_ID_ASRC14			31
#define DAI_ID_ASRC15			32
#define DAI_ID_PDM0			33
#define DAI_ID_PDM1			34
#define DAI_ID_PDM2			35
#define DAI_ID_PDM3			36
#define DAI_ID_PDM4			37
#define DAI_ID_PDM5			38
#define DAI_ID_PDM6			39
#define DAI_ID_PDM7			40
#define DAI_ID_SPDIF_TX0		41
#define DAI_ID_SPDIF_TX1		42
#define DAI_ID_SPDIF_TX2		43
#define DAI_ID_SPDIF_TX3		44
#define DAI_ID_SPDIF_TX4		45
#define DAI_ID_SPDIF_TX5		46
#define DAI_ID_SPDIF_TX6		47
#define DAI_ID_SPDIF_TX7		48
#define DAI_ID_SPDIF_RX0		49
#define DAI_ID_SPDIF_RX1		50
#define DAI_ID_SPDIF_RX2		51
#define DAI_ID_SPDIF_RX3		52
#define DAI_ID_SPDIF_RX4		53
#define DAI_ID_SPDIF_RX5		54
#define DAI_ID_SPDIF_RX6		55
#define DAI_ID_SPDIF_RX7		56

struct rk_asrc_soc_data {
	int (*select_lrck_clk)(struct device *dev);
};

struct rockchip_asrc_pair {
	struct rockchip_asrc *asrc;
	unsigned int error;
	unsigned int channels;
	struct dma_async_tx_descriptor *desc[2];
	struct dma_chan *dma_chan[2];
	unsigned int pos;
	bool req_dma_chan;
	void *private;
	void *private_m2m;
};

struct rockchip_asrc {
	struct device *dev;
	struct platform_device *pdev;
	struct regmap *grf;
	struct regmap *regmap;
	struct clk *mclk;
	struct clk *hclk;
	struct clk *cru_src0;
	struct clk *cru_src1;
	unsigned long paddr;
	struct snd_pcm_substream *substreams[SNDRV_PCM_STREAM_LAST + 1];
	const struct rk_asrc_soc_data *soc_data;
	struct rockchip_asrc_pair *pair[2];
	struct snd_dmaengine_dai_dma_data dma_data_rx;
	struct snd_dmaengine_dai_dma_data dma_data_tx;
	int sample_rate;
	int resample_rate;
	int tx_link_dai_id;	/* This must be set firstly by amixer/tinymixer */
	int rx_link_dai_id;	/* This must be set firstly by amixer/tinymixer */
};

static int asrc_calculate_ratio(struct rockchip_asrc *asrc,
				int numerator, int denominator)
{
	int i, integerPart, remainder, ratio, digit;
	unsigned int temp = 0;

	if (denominator == 0) {
		dev_err(asrc->dev, "The denominator can not be zero.\n");
		return 0;
	}

	integerPart = numerator / denominator;
	remainder = numerator % denominator;
	ratio = integerPart << 22;

	for (i = 0; i < 8; i++) {
		remainder <<= 4;
		digit = remainder / denominator;
		temp |= (digit << (28 - i * 4));
		remainder %= denominator;
	}

	ratio += (temp >> 10);

	return ratio;
}

static struct dma_chan *rockchip_asrc_get_dma_channel(struct rockchip_asrc *asrc, bool dir)
{
	struct dma_chan *chan = NULL;

	if (dir == OUT)
		chan = dma_request_chan(asrc->dev, "rx");
	else
		chan = dma_request_chan(asrc->dev, "tx");

	return chan;
}

static void rockchip_asrc_ratio_update_start(struct rockchip_asrc *asrc, int stream)
{
	/* get the sampling frequency, then set the ratio */
	regmap_write(asrc->regmap, ASRC_SAMPLE_RATE, asrc->sample_rate);
	regmap_write(asrc->regmap, ASRC_RESAMPLE_RATE, asrc->resample_rate);

	regmap_write(asrc->regmap, ASRC_MANUAL_RATIO,
		     asrc_calculate_ratio(asrc, asrc->sample_rate, asrc->resample_rate));
}

static int rockchip_asrc_clks_setup(struct rockchip_asrc *asrc)
{
	/*
	 * Set which device attach to asrc, then set their
	 * clks which connect to asrc.
	 */
	return asrc->soc_data->select_lrck_clk(asrc->dev);
}

static void rockchip_asrc_start(struct rockchip_asrc *asrc, int stream)
{
	unsigned int val = 0;

	rockchip_asrc_ratio_update_start(asrc, stream);
	/* Set the real time here */
	if (!asrc->rx_link_dai_id && asrc->tx_link_dai_id)
		val = ASRC_M2D;
	else if (asrc->rx_link_dai_id && !asrc->tx_link_dai_id)
		val = ASRC_S2M;
	else if (!asrc->rx_link_dai_id && !asrc->tx_link_dai_id)
		val = ASRC_M2M;
	else
		val = ASRC_S2D;

	regmap_update_bits(asrc->regmap, ASRC_CON,
			   ASRC_MODE_MSK | ASRC_OUT_MSK |
			   ASRC_IN_MSK | ASRC_REAL_TIME_MODE_MSK,
			   ASRC_REAL_TIME | ASRC_OUT_START |
			   ASRC_IN_START | val);
	regmap_update_bits(asrc->regmap, ASRC_INT_CON,
			   ASRC_CONV_ERROR_MSK, ASRC_CONV_ERROR_EN);
	/* Now the dma is single direction */
	regmap_update_bits(asrc->regmap, ASRC_CON, ASRC_MSK, ASRC_EN);
}

static void rockchip_asrc_stop(struct rockchip_asrc *asrc, int stream)
{
	regmap_update_bits(asrc->regmap, ASRC_CON,
			   ASRC_OUT_MSK | ASRC_IN_MSK,
			   ASRC_OUT_STOP | ASRC_IN_STOP);
	regmap_update_bits(asrc->regmap, ASRC_INT_CON,
			   ASRC_CONV_ERROR_MSK, ASRC_CONV_ERROR_DIS);
	regmap_update_bits(asrc->regmap, ASRC_CON, ASRC_MSK, ASRC_DIS);
}

static int rockchip_asrc_startup(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct rockchip_asrc *asrc = snd_soc_dai_get_drvdata(dai);

	if (asrc->substreams[substream->stream])
		return -EBUSY;

	asrc->substreams[substream->stream] = substream;

	return 0;
}

static void rockchip_asrc_shutdown(struct snd_pcm_substream *substream,
				   struct snd_soc_dai *dai)
{
	struct rockchip_asrc *asrc = snd_soc_dai_get_drvdata(dai);

	asrc->substreams[substream->stream] = NULL;
}

static int rockchip_asrc_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params,
				   struct snd_soc_dai *dai)
{
	struct rockchip_asrc *asrc = snd_soc_dai_get_drvdata(dai);
	unsigned int val;

	/* Set sample rate and resample rate */
	asrc->sample_rate = params_rate(params);

	/* Set channel */
	regmap_update_bits(asrc->regmap, ASRC_CON, ASRC_CHAN_NUM_MSK,
			   ASRC_CHAN_NUM(params_channels(params)));
	/* Set size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		val = ASRC_IWL_16BIT | ASRC_OWL_16BIT |
		      ASRC_OFMT_16 | ASRC_IFMT_16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
		val = ASRC_IWL_24BIT | ASRC_OWL_24BIT |
		ASRC_OFMT_32 | ASRC_IFMT_32;
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits(asrc->regmap, ASRC_DATA_FMT,
			   ASRC_OWL_MSK | ASRC_IWL_MSK |
			   ASRC_OFMT_MSK | ASRC_IFMT_MSK,
			   val);

	if (rockchip_asrc_clks_setup(asrc))
		return -EINVAL;

	return 0;
}

static int rockchip_asrc_trigger(struct snd_pcm_substream *substream,
				 int cmd, struct snd_soc_dai *dai)
{
	struct rockchip_asrc *asrc = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		rockchip_asrc_start(asrc, substream->stream);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		rockchip_asrc_stop(asrc, substream->stream);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int rockchip_asrc_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	/* No need to set here */
	return 0;
}

static int rockchip_asrc_set_sysclk(struct snd_soc_dai *dai, int clk_id,
				    unsigned int freq, int dir)
{
	struct rockchip_asrc *asrc = snd_soc_dai_get_drvdata(dai);
	int ret;

	/* Set the module clock */
	ret = clk_set_rate(asrc->mclk, ASRC_DEFAULT_CLK);
	if (ret)
		dev_err(asrc->dev, "Failed to set mclk %d\n", ret);

	return 0;
}

static const struct snd_soc_dai_ops rockchip_asrc_dai_ops = {
	.startup = rockchip_asrc_startup,
	.shutdown = rockchip_asrc_shutdown,
	.hw_params = rockchip_asrc_hw_params,
	.trigger = rockchip_asrc_trigger,
	.set_fmt = rockchip_asrc_set_fmt,
	.set_sysclk = rockchip_asrc_set_sysclk,
};

static int rockchip_asrc_dai_probe(struct snd_soc_dai *dai)
{
	struct rockchip_asrc *asrc = snd_soc_dai_get_drvdata(dai);

	snd_soc_dai_init_dma_data(dai, &asrc->dma_data_tx,
				  &asrc->dma_data_rx);

	return 0;
}

static struct snd_soc_dai_driver rockchip_asrc_dai = {
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 4,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			   SNDRV_PCM_FMTBIT_S24_LE |
			   SNDRV_PCM_FMTBIT_S32_LE,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 4,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			   SNDRV_PCM_FMTBIT_S24_LE |
			   SNDRV_PCM_FMTBIT_S32_LE,
	},
	.probe = rockchip_asrc_dai_probe,
	.ops = &rockchip_asrc_dai_ops,
	.symmetric_rate = 1,
};

static bool rockchip_asrc_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ASRC_VERSION:
	case ASRC_CON:
	case ASRC_CLKDIV_CON:
	case ASRC_DATA_FMT:
	case ASRC_LOOP_CON0:
	case ASRC_LOOP_CON1:
	case ASRC_LOOP_CON2:
	case ASRC_MANUAL_RATIO:
	case ASRC_SAMPLE_RATE:
	case ASRC_RESAMPLE_RATE:
	case ASRC_TRACK_PERIOD:
	case ASRC_RATIO_MARGIN:
	case ASRC_LRCK_MARGIN:
	case ASRC_FETCH_LEN:
	case ASRC_DMA_THRESH:
	case ASRC_INT_CON:
	case ASRC_INT_ST:
	case ASRC_ST:
	case ASRC_RATIO_ST:
	case ASRC_RESAMPLE_RATE_ST:
	case ASRC_THETA_CNT_ST:
	case ASRC_DECI_THETA_ACC_ST:
	case ASRC_FIFO_IN_WRCNT:
	case ASRC_FIFO_IN_RDCNT:
	case ASRC_FIFO_OUT_WRCNT:
	case ASRC_FIFO_OUT_RDCNT:
	case ASRC_RXDR:
	case ASRC_TXDR:
		return true;
	default:
		return false;
	}
}

static bool rockchip_asrc_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ASRC_CON:
	case ASRC_CLKDIV_CON:
	case ASRC_DATA_FMT:
	case ASRC_LOOP_CON0:
	case ASRC_LOOP_CON1:
	case ASRC_LOOP_CON2:
	case ASRC_MANUAL_RATIO:
	case ASRC_SAMPLE_RATE:
	case ASRC_RESAMPLE_RATE:
	case ASRC_TRACK_PERIOD:
	case ASRC_RATIO_MARGIN:
	case ASRC_LRCK_MARGIN:
	case ASRC_FETCH_LEN:
	case ASRC_DMA_THRESH:
	case ASRC_INT_CON:
	case ASRC_FIFO_IN_WRCNT:
	case ASRC_FIFO_IN_RDCNT:
	case ASRC_RXDR:
	case ASRC_TXDR:
		return true;
	default:
		return false;
	}
}

static bool rockchip_asrc_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ASRC_INT_CON:
	case ASRC_FIFO_IN_WRCNT:
	case ASRC_FIFO_IN_RDCNT:
	case ASRC_FIFO_OUT_WRCNT:
	case ASRC_FIFO_OUT_RDCNT:
	case ASRC_RXDR:
	case ASRC_TXDR:
		return true;
	default:
		return false;
	}
}

static bool rockchip_asrc_precious_reg(struct device *dev, unsigned int reg)
{
	if (reg == ASRC_RXDR)
		return true;

	return false;
}

static const struct reg_default rockchip_asrc_reg[] = {
	{ ASRC_VERSION, 0x00000001 },
};

static const struct regmap_config rockchip_asrc_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = ASRC_FIFO_OUT_DATA,
	.reg_defaults = rockchip_asrc_reg,
	.num_reg_defaults = ARRAY_SIZE(rockchip_asrc_reg),
	.readable_reg = rockchip_asrc_readable_reg,
	.writeable_reg = rockchip_asrc_writeable_reg,
	.volatile_reg = rockchip_asrc_volatile_reg,
	.precious_reg = rockchip_asrc_precious_reg,
	.cache_type = REGCACHE_FLAT,
};

#define RK_ASRC_DMABUF_SIZE	(16 * 1024)

static struct snd_pcm_hardware snd_rk_hardware = {
	.info = SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER |
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID,
	.buffer_bytes_max = RK_ASRC_DMABUF_SIZE,
	.period_bytes_min = 128,
	.period_bytes_max = 0x2000, /* Limited by asrc, max 0x4000 */
	.periods_min = 2,
	.periods_max = 2,
	.fifo_size = 0,
};

static void rockchip_asrc_dma_complete(void *arg)
{
	struct snd_pcm_substream *substream = arg;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rockchip_asrc_pair *pair = runtime->private_data;

	pair->pos += snd_pcm_lib_period_bytes(substream);
	if (pair->pos >= snd_pcm_lib_buffer_bytes(substream))
		pair->pos = 0;

	snd_pcm_period_elapsed(substream);
}

static int rockchip_asrc_dma_prepare_and_submit(struct snd_pcm_substream *substream,
						struct snd_soc_component *component)
{
	u8 dir = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ? OUT : IN;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rockchip_asrc_pair *pair = runtime->private_data;
	struct rockchip_asrc *asrc = pair->asrc;
	struct device *dev = component->dev;
	unsigned long flags = DMA_CTRL_ACK;

	/* Prepare and submit Front-End DMA channel */
	if (!substream->runtime->no_period_wakeup)
		flags |= DMA_PREP_INTERRUPT;

	pair->pos = 0;
	pair->desc[!dir] = dmaengine_prep_dma_cyclic(
			pair->dma_chan[!dir], runtime->dma_addr,
			snd_pcm_lib_buffer_bytes(substream),
			snd_pcm_lib_period_bytes(substream),
			dir == OUT ? DMA_MEM_TO_DEV : DMA_DEV_TO_MEM, flags);
	if (!pair->desc[!dir]) {
		dev_err(dev, "failed to prepare slave DMA for Front-End\n");
		return -ENOMEM;
	}

	pair->desc[!dir]->callback = rockchip_asrc_dma_complete;
	pair->desc[!dir]->callback_param = substream;

	dmaengine_submit(pair->desc[!dir]);

	/* Prepare and submit Back-End DMA channel */
	pair->desc[dir] = dmaengine_prep_dma_cyclic(
			pair->dma_chan[dir],
			dir == OUT ? (asrc->paddr + ASRC_FIFO_OUT_DATA) : (asrc->paddr + ASRC_FIFO_IN_DATA),
			RK_ASRC_DMABUF_SIZE, RK_ASRC_DMABUF_SIZE / 2,
			dir == OUT ? DMA_MEM_TO_DEV : DMA_DEV_TO_MEM, 0);
	if (!pair->desc[dir]) {
		dev_err(dev, "failed to prepare slave DMA for Back-End\n");
		return -ENOMEM;
	}

	dmaengine_submit(pair->desc[dir]);

	return 0;
}

static int rockchip_asrc_dma_trigger(struct snd_soc_component *component,
				     struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rockchip_asrc_pair *pair = runtime->private_data;
	int ret;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		ret = rockchip_asrc_dma_prepare_and_submit(substream, component);
		if (ret)
			return ret;
		dma_async_issue_pending(pair->dma_chan[IN]);
		dma_async_issue_pending(pair->dma_chan[OUT]);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		dmaengine_terminate_async(pair->dma_chan[OUT]);
		dmaengine_terminate_async(pair->dma_chan[IN]);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int rockchip_asrc_dma_hw_params(struct snd_soc_component *component,
				       struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct snd_dmaengine_dai_dma_data *dma_params_fe = NULL;
	struct snd_dmaengine_dai_dma_data *dma_params_be = NULL;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rockchip_asrc_pair *pair = runtime->private_data;
	struct dma_chan *tmp_chan = NULL, *be_chan = NULL;
	struct snd_soc_component *component_be = NULL;
	struct rockchip_asrc *asrc = pair->asrc;
	struct dma_slave_config config_fe = {}, config_be = {};
	struct device *dev = component->dev;
	int stream = substream->stream;
	struct snd_soc_dpcm *dpcm;
	struct device *dev_be;
	u8 dir = tx ? OUT : IN;
	dma_cap_mask_t mask;
	int ret;

	/* Fetch the Back-End dma_data from DPCM */
	for_each_dpcm_be(rtd, stream, dpcm) {
		struct snd_soc_pcm_runtime *be = dpcm->be;
		struct snd_pcm_substream *substream_be;
		struct snd_soc_dai *dai = asoc_rtd_to_cpu(be, 0);

		if (dpcm->fe != rtd)
			continue;

		substream_be = snd_soc_dpcm_get_substream(be, stream);
		dma_params_be = snd_soc_dai_get_dma_data(dai, substream_be);
		dev_be = dai->dev;
		break;
	}

	if (!dma_params_be) {
		dev_err(dev, "failed to get the substream of Back-End\n");
		return -EINVAL;
	}

	/* Override dma_data of the Front-End and config its dmaengine */
	dma_params_fe = snd_soc_dai_get_dma_data(asoc_rtd_to_cpu(rtd, 0), substream);
	if (tx)
		dma_params_fe->addr = asrc->paddr + ASRC_RXDR;
	else
		dma_params_fe->addr = asrc->paddr + ASRC_TXDR;

	dma_params_fe->maxburst = dma_params_be->maxburst;

	pair->dma_chan[!dir] = rockchip_asrc_get_dma_channel(asrc, !dir);
	if (!pair->dma_chan[!dir]) {
		dev_err(dev, "failed to request DMA channel\n");
		return -EINVAL;
	}

	ret = snd_dmaengine_pcm_prepare_slave_config(substream, params, &config_fe);
	if (ret) {
		dev_err(dev, "failed to prepare DMA config for Front-End\n");
		return ret;
	}

	ret = dmaengine_slave_config(pair->dma_chan[!dir], &config_fe);
	if (ret) {
		dev_err(dev, "failed to config DMA channel for Front-End\n");
		return ret;
	}

	/* Request and config DMA channel for Back-End */
	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	dma_cap_set(DMA_CYCLIC, mask);

	/*
	 * The Back-End device might have already requested a DMA channel,
	 * so try to reuse it first, and then request a new one upon NULL.
	 */
	component_be = snd_soc_lookup_component_nolocked(dev_be, SND_DMAENGINE_PCM_DRV_NAME);
	if (component_be) {
		be_chan = soc_component_to_pcm(component_be)->chan[substream->stream];
		tmp_chan = be_chan;
	}

	if (!tmp_chan) {
		tmp_chan = dma_request_chan(dev_be, tx ? "tx" : "rx");
		if (IS_ERR(tmp_chan)) {
			dev_err(dev, "failed to request DMA channel for Back-End\n");
			return -EINVAL;
		}
	}

	pair->dma_chan[dir] = tmp_chan;
	/* Do not flag to release if we are reusing the Back-End one */
	pair->req_dma_chan = !be_chan;

	if (!pair->dma_chan[dir]) {
		dev_err(dev, "failed to request DMA channel for Back-End\n");
		return -EINVAL;
	}

	config_be.direction = (dir == OUT ? DMA_MEM_TO_DEV : DMA_DEV_TO_MEM);
	config_be.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	config_be.src_maxburst = dma_params_be->maxburst;
	config_be.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	config_be.dst_maxburst = dma_params_be->maxburst;

	if (tx) {
		config_be.src_addr = asrc->paddr + ASRC_FIFO_OUT_DATA;
		config_be.dst_addr = dma_params_be->addr;
	} else {
		config_be.dst_addr = asrc->paddr + ASRC_FIFO_IN_DATA;
		config_be.src_addr = dma_params_be->addr;
	}

	ret = dmaengine_slave_config(pair->dma_chan[dir], &config_be);
	if (ret) {
		dev_err(dev, "failed to config DMA channel for Back-End\n");
		if (pair->req_dma_chan)
			dma_release_channel(pair->dma_chan[dir]);
		return ret;
	}

	return 0;
}

static int rockchip_asrc_dma_hw_free(struct snd_soc_component *component,
				     struct snd_pcm_substream *substream)
{
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rockchip_asrc_pair *pair = runtime->private_data;
	u8 dir = tx ? OUT : IN;

	if (pair->dma_chan[!dir])
		dma_release_channel(pair->dma_chan[!dir]);

	/* release dev_to_dev chan if we aren't reusing the Back-End one */
	if (pair->dma_chan[dir] && pair->req_dma_chan)
		dma_release_channel(pair->dma_chan[dir]);

	pair->dma_chan[!dir] = NULL;
	pair->dma_chan[dir] = NULL;

	return 0;
}

static int rockchip_asrc_dma_startup(struct snd_soc_component *component,
				     struct snd_pcm_substream *substream)
{
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_dmaengine_dai_dma_data *dma_data;
	struct device *dev = component->dev;
	struct rockchip_asrc *asrc = dev_get_drvdata(dev);
	struct rockchip_asrc_pair *pair;
	struct dma_chan *tmp_chan = NULL;
	u8 dir = tx ? OUT : IN;
	bool release_pair = true;
	int ret = 0;

	ret = snd_pcm_hw_constraint_integer(substream->runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		dev_err(dev, "failed to set pcm hw params periods\n");
		return ret;
	}

	pair = kzalloc(sizeof(*pair), GFP_KERNEL);
	if (!pair)
		return -ENOMEM;

	pair->asrc = asrc;

	runtime->private_data = pair;

	/* Request a dummy pair, which will be released later.
	 * Request pair function needs channel num as input, for this
	 * dummy pair, we just request "1" channel temporarily.
	 */
	/* Request a dummy dma channel, which will be released later. */
	dma_data = snd_soc_dai_get_dma_data(asoc_rtd_to_cpu(rtd, 0), substream);

	tmp_chan = rockchip_asrc_get_dma_channel(asrc, dir);
	if (IS_ERR(tmp_chan)) {
		dev_err(dev, "failed to get dma channel\n");
		ret = -EINVAL;
		goto req_pair_err;
	}

	/* Refine the snd_rk_hardware according to caps of DMA. */
	ret = snd_dmaengine_pcm_refine_runtime_hwparams(substream,
							dma_data,
							&snd_rk_hardware,
							tmp_chan);
	if (ret < 0) {
		dev_err(dev, "failed to refine runtime hwparams\n");
		goto out;
	}

	release_pair = false;
	snd_soc_set_runtime_hwparams(substream, &snd_rk_hardware);

out:
	dma_release_channel(tmp_chan);

req_pair_err:
	if (release_pair)
		kfree(pair);

	return ret;
}

static int rockchip_asrc_dma_shutdown(struct snd_soc_component *component,
				      struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rockchip_asrc_pair *pair = runtime->private_data;

	if (!pair)
		return 0;

	kfree(pair);
	runtime->private_data = NULL;

	return 0;
}

static snd_pcm_uframes_t rockchip_asrc_dma_pcm_pointer(struct snd_soc_component *component,
						       struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rockchip_asrc_pair *pair = runtime->private_data;

	return bytes_to_frames(substream->runtime, pair->pos);
}

static int rockchip_asrc_dma_pcm_new(struct snd_soc_component *component,
				     struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret;

	ret = dma_coerce_mask_and_coherent(card->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(card->dev, "failed to set DMA mask\n");
		return ret;
	}

	return snd_pcm_set_fixed_buffer_all(pcm, SNDRV_DMA_TYPE_DEV,
					    card->dev, RK_ASRC_DMABUF_SIZE);
}

static const char * const asrc_link_dai_text[] = {
		"MEM", "SAI0", "SAI1", "SAI2", "SAI3",
		"SAI4", "SAI5", "SAI6", "SAI7",
		"SAI8", "SAI9", "SAI10", "SAI11",
		"SAI12", "SAI13", "SAI14", "SAI15",
		"ASRC0", "ASRC1", "ASRC2", "ASRC3",
		"ASRC4", "ASRC5", "ASRC6", "ASRC7",
		"ASRC8", "ASRC9", "ASRC10", "ASRC11",
		"ASRC12", "ASRC13", "ASRC14", "ASRC15",
		"PDM0", "PDM1", "PDM2", "PDM3",
		"PDM4", "PDM5", "PDM6", "PDM7",
		"SPDIF_TX0", "SPDIF_TX1", "SPDIF_TX2", "SPDIF_TX4",
		"SPDIF_TX4", "SPDIF_TX5", "SPDIF_TX6", "SPDIF_TX7",
		"SPDIF_RX0", "SPDIF_RX1", "SPDIF_RX2", "SPDIF_RX4",
		"SPDIF_RX4", "SPDIF_RX5", "SPDIF_RX6", "SPDIF_RX7" };

static const struct soc_enum asrc_tx_link_dai_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(asrc_link_dai_text), asrc_link_dai_text);
static const struct soc_enum asrc_rx_link_dai_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(asrc_link_dai_text), asrc_link_dai_text);

static int rockchip_asrc_tx_dai_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rockchip_asrc *asrc = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = asrc->tx_link_dai_id;

	return 0;
}

static int rockchip_asrc_tx_dai_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rockchip_asrc *asrc = snd_soc_component_get_drvdata(component);
	int num;

	num = ucontrol->value.enumerated.item[0];
	if (num >= ARRAY_SIZE(asrc_link_dai_text))
		return -EINVAL;

	asrc->tx_link_dai_id = num;

	return 1;
}

static int rockchip_asrc_rx_dai_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rockchip_asrc *asrc = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = asrc->rx_link_dai_id;

	return 0;
}

static int rockchip_asrc_rx_dai_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rockchip_asrc *asrc = snd_soc_component_get_drvdata(component);
	int num;

	num = ucontrol->value.enumerated.item[0];
	if (num >= ARRAY_SIZE(asrc_link_dai_text))
		return -EINVAL;

	asrc->rx_link_dai_id = num;

	return 1;
}

static int rockchip_asrc_resample_rate_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rockchip_asrc *asrc = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = asrc->resample_rate;

	return 0;
}

static int rockchip_asrc_resample_rate_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct rockchip_asrc *asrc = snd_soc_component_get_drvdata(component);

	if ((ucontrol->value.integer.value[0] < 0) ||
	    (ucontrol->value.integer.value[0] > 192000))
		return -EINVAL;

	asrc->resample_rate = ucontrol->value.integer.value[0];

	return 1;
}

/*
 * 1.resamplerate
 * 2.dev rx & tx lrck select
 */
static const struct snd_kcontrol_new rockchip_asrc_controls[] = {
	SOC_ENUM_EXT("TX LINK DAI Select", asrc_tx_link_dai_enum,
		     rockchip_asrc_tx_dai_get, rockchip_asrc_tx_dai_put),
	SOC_ENUM_EXT("RX LINK DAI Select", asrc_rx_link_dai_enum,
		     rockchip_asrc_rx_dai_get, rockchip_asrc_rx_dai_put),

	SOC_SINGLE_EXT("SET RESAMPLE RATE", 0, 0, 192000, 0,
			rockchip_asrc_resample_rate_get,
			rockchip_asrc_resample_rate_put),
};

static const struct snd_soc_component_driver rockchip_asrc_component = {
	.name			= DRV_NAME,
	.controls		= rockchip_asrc_controls,
	.num_controls		= ARRAY_SIZE(rockchip_asrc_controls),
	.hw_params		= rockchip_asrc_dma_hw_params,
	.hw_free		= rockchip_asrc_dma_hw_free,
	.trigger		= rockchip_asrc_dma_trigger,
	.open			= rockchip_asrc_dma_startup,
	.close			= rockchip_asrc_dma_shutdown,
	.pointer		= rockchip_asrc_dma_pcm_pointer,
	.pcm_construct		= rockchip_asrc_dma_pcm_new,
	.legacy_dai_naming	= 1,
};

static int rockchip_asrc_init(struct rockchip_asrc *asrc)
{
	/* clear interrupt */
	regmap_write(asrc->regmap, ASRC_INT_CON, 0x0);

	/* Set the dma config */
	regmap_update_bits(asrc->regmap, ASRC_DMA_THRESH,
			   ASRC_DMA_RX_THRESH_MSK | ASRC_DMA_TX_THRESH_MSK |
			   ASRC_OUT_THRESH_MSK | ASRC_IN_THRESH_MSK |
			   ASRC_NEG_THRESH_MSK | ASRC_POS_THRESH_MSK,
			   ASRC_DMA_RX_THRESH(3) | ASRC_DMA_TX_THRESH(0x3) |
			   ASRC_OUT_THRESH(11) | ASRC_IN_THRESH(3) |
			   ASRC_NEG_THRESH(3) | ASRC_POS_THRESH(0x1c));
	/*
	 * Set default mode: real time, S2D, track and ratio exc
	 * track: track clock, but no adjust ratio
	 * ratio exc: adjust ratio
	 * ratio filt: sliding average filtering ratio to prevent anomalies
	 */
	regmap_update_bits(asrc->regmap, ASRC_CON,
			   ASRC_RATIO_TRACK_MODE | ASRC_RATIO_TRACK_MSK | ASRC_RATIO_EXC_MSK |
			   ASRC_RATIO_FILT_MSK | ASRC_REAL_TIME_MODE_MSK | ASRC_MODE_MSK,
			   ASRC_RATIO_TRACK_MODE | ASRC_RATIO_TRACK_DIS | ASRC_RATIO_EXC_DIS |
			   ASRC_RATIO_FILT_EN | ASRC_S2D | ASRC_REAL_TIME);

	regmap_update_bits(asrc->regmap, ASRC_TRACK_PERIOD,
			   ASRC_RATIO_TRACK_DIV_MSK | ASRC_RATIO_TRACK_PERIOD_MSK,
			   ASRC_RATIO_TRACK_DIV(3) | ASRC_RATIO_TRACK_PERIOD(1023));

	return 0;
}

static int rockchip_asrc_runtime_suspend(struct device *dev)
{
	struct rockchip_asrc *asrc = dev_get_drvdata(dev);

	regcache_cache_only(asrc->regmap, true);

	clk_disable_unprepare(asrc->mclk);
	clk_disable_unprepare(asrc->hclk);
	if (asrc->tx_link_dai_id == DAI_ID_MEM) {
		clk_disable_unprepare(asrc->cru_src0);
		regmap_update_bits(asrc->regmap, ASRC_CLKDIV_CON,
				   ASRC_DST_LRCK_DIV_MSK,
				   ASRC_DST_LRCK_DIV_DIS);
	}

	if (asrc->rx_link_dai_id == DAI_ID_MEM) {
		clk_disable_unprepare(asrc->cru_src1);
		regmap_update_bits(asrc->regmap, ASRC_CLKDIV_CON,
				   ASRC_SRC_LRCK_DIV_MSK,
				   ASRC_SRC_LRCK_DIV_DIS);
	}

	return 0;
}

static int rockchip_asrc_runtime_resume(struct device *dev)
{
	struct rockchip_asrc *asrc = dev_get_drvdata(dev);
	int ret;

	ret = clk_prepare_enable(asrc->hclk);
	if (ret)
		goto err_hclk;

	ret = clk_prepare_enable(asrc->mclk);
	if (ret)
		goto err_mclk;

	if (asrc->tx_link_dai_id == DAI_ID_MEM) {
		ret = clk_prepare_enable(asrc->cru_src0);
		if (ret)
			goto error_cru_src0;
	}

	if (asrc->rx_link_dai_id == DAI_ID_MEM) {
		ret = clk_prepare_enable(asrc->cru_src1);
		if (ret)
			goto error_cru_src1;
	}

	regcache_cache_only(asrc->regmap, false);
	regcache_mark_dirty(asrc->regmap);

	ret =  regcache_sync(asrc->regmap);
	if (ret)
		goto err_regmap;
	return 0;
err_regmap:
	if (asrc->rx_link_dai_id == DAI_ID_MEM)
		clk_disable_unprepare(asrc->cru_src1);
error_cru_src1:
	if (asrc->tx_link_dai_id == DAI_ID_MEM)
		clk_disable_unprepare(asrc->cru_src0);
error_cru_src0:
	clk_disable_unprepare(asrc->mclk);
err_mclk:
	clk_disable_unprepare(asrc->hclk);
err_hclk:
	return ret;
}

static irqreturn_t rockchip_asrc_isr(int irq, void *devid)
{
	struct rockchip_asrc *asrc = (struct rockchip_asrc *)devid;
	u32 status;

	regmap_read(asrc->regmap, ASRC_INT_ST, &status);
	/* clear the interrupt */
	regmap_write(asrc->regmap, ASRC_INT_ST, status);
	if (status & ASRC_FIFO_OUT_EMPTY_ST)
		dev_err_ratelimited(asrc->dev, "ASRC FIFO out empty\n");

	if (status & ASRC_FIFO_OUT_FULL_ST)
		dev_err_ratelimited(asrc->dev, "ASRC FIFO out full\n");

	if (status & ASRC_FIFO_IN_EMPTY_ST)
		dev_err_ratelimited(asrc->dev, "ASRC FIFO in empty\n");

	if (status & ASRC_FIFO_IN_FULL_ST)
		dev_err_ratelimited(asrc->dev, "ASRC FIFO in full\n");

	if (status & ASRC_CONV_ERROR_ST)
		dev_err_ratelimited(asrc->dev, "ASRC conv error\n");

	return IRQ_HANDLED;
}

static void rockchip_asrc_set_lrck_div_from_cru(struct rockchip_asrc *asrc)
{
	if (asrc->tx_link_dai_id == DAI_ID_MEM) {
		if (clk_set_rate(asrc->cru_src0, CRU_LRCK_SOURCE_FREQ)) {
			dev_err(asrc->dev, "Failed to set cru_src0\n");
			return;
		}

		clk_prepare_enable(asrc->cru_src0);
		regmap_update_bits(asrc->regmap, ASRC_CLKDIV_CON,
				   ASRC_DST_LRCK_DIV_MSK |
				   ASRC_DST_LRCK_DIV_CON_MSK,
				   ASRC_DST_LRCK_DIV_EN |
				   ASRC_DST_LRCK_DIV(CRU_LRCK_SOURCE_FREQ /
						     asrc->resample_rate * 2));

	}

	if (asrc->rx_link_dai_id == DAI_ID_MEM) {
		if (clk_set_rate(asrc->cru_src1, CRU_LRCK_SOURCE_FREQ)) {
			dev_err(asrc->dev, "Failed to set cru_src1\n");
			return;
		}

		clk_prepare_enable(asrc->cru_src1);
		regmap_update_bits(asrc->regmap, ASRC_CLKDIV_CON,
				   ASRC_SRC_LRCK_DIV_MSK |
				   ASRC_SRC_LRCK_DIV_CON_MSK,
				   ASRC_SRC_LRCK_DIV_EN |
				   ASRC_SRC_LRCK_DIV(CRU_LRCK_SOURCE_FREQ /
						     asrc->sample_rate * 2));
	}
}

static int rk3576_asrc_select_lrck_clk(struct device *dev)
{
	struct rockchip_asrc *asrc = dev_get_drvdata(dev);
	unsigned int tx_val = 0, rx_val = 0;

	switch (asrc->tx_link_dai_id) {
	case DAI_ID_MEM:
		tx_val = RK3576_DST_LRCK_FROM_CRU0;
		break;
	case DAI_ID_SAI0:
		tx_val = RK3576_DST_LRCK_FROM_SAI0;
		break;
	case DAI_ID_SAI1:
		tx_val = RK3576_DST_LRCK_FROM_SAI1;
		break;
	case DAI_ID_SAI2:
		tx_val = RK3576_DST_LRCK_FROM_SAI2;
		break;
	case DAI_ID_SAI3:
		tx_val = RK3576_DST_LRCK_FROM_SAI3;
		break;
	case DAI_ID_SAI4:
		tx_val = RK3576_DST_LRCK_FROM_SAI4;
		break;
	case DAI_ID_SAI5:
		tx_val = RK3576_DST_LRCK_FROM_SAI5;
		break;
	case DAI_ID_SAI6:
		tx_val = RK3576_DST_LRCK_FROM_SAI6;
		break;
	case DAI_ID_SAI7:
		tx_val = RK3576_DST_LRCK_FROM_SAI7;
		break;
	case DAI_ID_SAI8:
		tx_val = RK3576_DST_LRCK_FROM_SAI8;
		break;
	case DAI_ID_SAI9:
		tx_val = RK3576_DST_LRCK_FROM_SAI9;
		break;
	case DAI_ID_SPDIF_TX0:
		tx_val = RK3576_DST_LRCK_FROM_SPDIF_TX0;
		break;
	case DAI_ID_SPDIF_TX1:
		tx_val = RK3576_DST_LRCK_FROM_SPDIF_TX1;
		break;
	case DAI_ID_SPDIF_TX2:
		tx_val = RK3576_DST_LRCK_FROM_SPDIF_TX2;
		break;
	case DAI_ID_SPDIF_TX3:
		tx_val = RK3576_DST_LRCK_FROM_SPDIF_TX3;
		break;
	case DAI_ID_SPDIF_TX4:
		tx_val = RK3576_DST_LRCK_FROM_SPDIF_TX4;
		break;
	case DAI_ID_SPDIF_TX5:
		tx_val = RK3576_DST_LRCK_FROM_SPDIF_TX5;
		break;
	default:
		return -EINVAL;
	}

	switch (asrc->rx_link_dai_id) {
	case DAI_ID_MEM:
		rx_val = RK3576_SRC_LRCK_FROM_CRU1;
		break;
	case DAI_ID_SAI0:
		rx_val = RK3576_SRC_LRCK_FROM_SAI0;
		break;
	case DAI_ID_SAI1:
		rx_val = RK3576_SRC_LRCK_FROM_SAI1;
		break;
	case DAI_ID_SAI2:
		rx_val = RK3576_SRC_LRCK_FROM_SAI2;
		break;
	case DAI_ID_SAI3:
		rx_val = RK3576_SRC_LRCK_FROM_SAI3;
		break;
	case DAI_ID_SAI4:
		rx_val = RK3576_SRC_LRCK_FROM_SAI4;
		break;
	case DAI_ID_SAI5:
		rx_val = RK3576_SRC_LRCK_FROM_SAI5;
		break;
	case DAI_ID_SAI6:
		rx_val = RK3576_SRC_LRCK_FROM_SAI6;
		break;
	case DAI_ID_SAI7:
		rx_val = RK3576_SRC_LRCK_FROM_SAI7;
		break;
	case DAI_ID_SAI8:
		rx_val = RK3576_SRC_LRCK_FROM_SAI8;
		break;
	case DAI_ID_SAI9:
		rx_val = RK3576_SRC_LRCK_FROM_SAI9;
		break;
	case DAI_ID_PDM0:
		rx_val = RK3576_SRC_LRCK_FROM_PDM0;
		break;
	case DAI_ID_PDM1:
		rx_val = RK3576_SRC_LRCK_FROM_PDM1;
		break;
	case DAI_ID_SPDIF_RX0:
		rx_val = RK3576_SRC_LRCK_FROM_SPDIF_RX0;
		break;
	case DAI_ID_SPDIF_RX1:
		rx_val = RK3576_SRC_LRCK_FROM_SPDIF_RX1;
		break;
	case DAI_ID_SPDIF_RX2:
		rx_val = RK3576_SRC_LRCK_FROM_SPDIF_RX2;
		break;
	default:
		return -EINVAL;
	}

	if (asrc->paddr == RK3576_ASRC0) {
		regmap_write(asrc->grf, RK3576_SYS_GRF_SOC_CON9,
			     ASRC0_4CH_SRC_SEL_MASK |  ASRC0_4CH_DST_SEL_MASK |
			     ASRC0_4CH_SRC_SEL(rx_val) | ASRC0_4CH_DST_SEL(tx_val));
	} else if (asrc->paddr == RK3576_ASRC1) {
		regmap_write(asrc->grf, RK3576_SYS_GRF_SOC_CON9,
			     ASRC1_4CH_SRC_SEL_MASK | ASRC1_4CH_SRC_SEL(rx_val));
		regmap_write(asrc->grf, RK3576_SYS_GRF_SOC_CON10,
			     ASRC1_4CH_DST_SEL_MASK | ASRC1_4CH_DST_SEL(tx_val));
	} else if (asrc->paddr == RK3576_ASRC2) {
		regmap_write(asrc->grf, RK3576_SYS_GRF_SOC_CON10,
			     ASRC2_2CH_SRC_SEL_MASK | ASRC2_2CH_DST_SEL_MASK |
			     ASRC2_2CH_SRC_SEL(rx_val) | ASRC2_2CH_DST_SEL(tx_val));
	} else if (asrc->paddr == RK3576_ASRC3) {
		regmap_write(asrc->grf, RK3576_SYS_GRF_SOC_CON11,
			     ASRC3_2CH_SRC_SEL_MASK | ASRC3_2CH_DST_SEL_MASK |
			     ASRC3_2CH_SRC_SEL(rx_val) | ASRC3_2CH_DST_SEL(tx_val));
	} else {
		return -EINVAL;
	}

	rockchip_asrc_set_lrck_div_from_cru(asrc);

	return 0;
}

static const struct rk_asrc_soc_data rk3576_data = {
	.select_lrck_clk = rk3576_asrc_select_lrck_clk,
};

static const struct of_device_id rockchip_asrc_match[] = {
	{ .compatible = "rockchip,rk3576-asrc", .data = &rk3576_data},
	{}
};
MODULE_DEVICE_TABLE(of, rockchip_asrc_match);

static int rockchip_asrc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct rockchip_asrc *asrc;
	struct resource *res;
	void __iomem *regs;
	int ret, irq;

	asrc = devm_kzalloc(&pdev->dev, sizeof(*asrc), GFP_KERNEL);
	if (!asrc)
		return -ENOMEM;

	asrc->grf = syscon_regmap_lookup_by_phandle(node, "rockchip,grf");
	if (IS_ERR(asrc->grf))
		return PTR_ERR(asrc->grf);

	asrc->dev = &pdev->dev;
	asrc->pdev = pdev;
	dev_set_drvdata(&pdev->dev, asrc);
	regs = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	asrc->paddr = res->start;
	asrc->soc_data = device_get_match_data(&pdev->dev);
	if (!asrc->soc_data)
		return -EINVAL;

	asrc->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					     &rockchip_asrc_regmap_config);
	if (IS_ERR(asrc->regmap)) {
		dev_err(&pdev->dev, "failed to init regmap\n");
		return PTR_ERR(asrc->regmap);
	}

	irq = platform_get_irq_optional(pdev, 0);
	if (irq > 0) {
		ret = devm_request_irq(&pdev->dev, irq, rockchip_asrc_isr,
				       IRQF_SHARED, node->name, asrc);
		if (ret) {
			dev_err(&pdev->dev, "Failed to request irq %d\n", irq);
			return ret;
		}
	}

	asrc->mclk = devm_clk_get(&pdev->dev, "mclk");
	if (IS_ERR(asrc->mclk)) {
		dev_err(&pdev->dev, "Failed to get mclk\n");
		return PTR_ERR(asrc->mclk);
	}

	asrc->hclk = devm_clk_get(&pdev->dev, "hclk");
	if (IS_ERR(asrc->hclk)) {
		dev_err(&pdev->dev, "Failed to get hclk\n");
		return PTR_ERR(asrc->hclk);
	}

	asrc->cru_src0 = devm_clk_get(&pdev->dev, "cru_src0");
	if (IS_ERR(asrc->cru_src0)) {
		dev_err(&pdev->dev, "Failed to get cru_src0\n");
		return PTR_ERR(asrc->cru_src0);
	}

	asrc->cru_src1 = devm_clk_get(&pdev->dev, "cru_src1");
	if (IS_ERR(asrc->cru_src1)) {
		dev_err(&pdev->dev, "Failed to get cru_src1\n");
		return PTR_ERR(asrc->cru_src1);
	}

	ret = clk_prepare_enable(asrc->hclk);
	if (ret)
		return ret;
	/* Set the default value here */
	asrc->resample_rate = DEFAULT_SAMPLE_RATE;
	asrc->sample_rate = DEFAULT_SAMPLE_RATE;

	/* Set the default link dai here */
	asrc->tx_link_dai_id = DAI_ID_SAI0;
	asrc->rx_link_dai_id = DAI_ID_SAI1;

	/* DMA data init */
	asrc->dma_data_tx.addr = res->start + ASRC_TXDR;
	asrc->dma_data_tx.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	asrc->dma_data_tx.maxburst = MAXBURST_PER_FIFO;
	asrc->dma_data_rx.addr = res->start + ASRC_RXDR;
	asrc->dma_data_rx.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	asrc->dma_data_rx.maxburst = MAXBURST_PER_FIFO;

	platform_set_drvdata(pdev, asrc);
	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = rockchip_asrc_runtime_resume(&pdev->dev);
		if (ret)
			goto err_runtime_disable;
	}

	ret = pm_runtime_resume_and_get(&pdev->dev);
	if (ret < 0)
		goto err_runtime_suspend;

	ret = rockchip_asrc_init(asrc);
	if (ret) {
		dev_err(&pdev->dev, "Asrc init error.\n");
		goto err_runtime_suspend;
	}

	ret = devm_snd_soc_register_component(&pdev->dev,
					      &rockchip_asrc_component,
					      &rockchip_asrc_dai, 1);
	if (ret)
		goto err_runtime_suspend;

	clk_disable_unprepare(asrc->hclk);

	return 0;

err_runtime_suspend:
	if (!pm_runtime_status_suspended(&pdev->dev))
		rockchip_asrc_runtime_suspend(&pdev->dev);

err_runtime_disable:
	pm_runtime_disable(&pdev->dev);
	clk_disable_unprepare(asrc->hclk);
	return ret;
}

static int rockchip_asrc_remove(struct platform_device *pdev)
{
	struct rockchip_asrc *asrc = dev_get_drvdata(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		rockchip_asrc_runtime_suspend(&pdev->dev);

	clk_disable_unprepare(asrc->hclk);

	if (asrc->tx_link_dai_id == DAI_ID_MEM)
		clk_disable_unprepare(asrc->cru_src0);

	if (asrc->rx_link_dai_id == DAI_ID_MEM)
		clk_disable_unprepare(asrc->cru_src1);

	return 0;
}

static const struct dev_pm_ops rockchip_asrc_pm_ops = {
	SET_RUNTIME_PM_OPS(rockchip_asrc_runtime_suspend, rockchip_asrc_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend, pm_runtime_force_resume)
};

static struct platform_driver rockchip_asrc_driver = {
	.probe = rockchip_asrc_probe,
	.remove = rockchip_asrc_remove,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = rockchip_asrc_match,
		.pm = &rockchip_asrc_pm_ops,
	},
};
module_platform_driver(rockchip_asrc_driver);

MODULE_DESCRIPTION("Rockchip ASRC ASoC Interface");
MODULE_AUTHOR("Jason Zhu <jason.zhu@rock-chips.com>");
MODULE_LICENSE("GPL");
