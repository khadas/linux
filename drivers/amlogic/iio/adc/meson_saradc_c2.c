// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "meson_saradc.h"

#define MESON_C2_SAR_ADC_FIFO_RD				0x18
	#define MESON_C2_SAR_ADC_FIFO_RD_CHAN_ID_MASK		GENMASK(24, 22)
	#define MESON_C2_SAR_ADC_FIFO_RD_SAMPLE_VALUE_MASK	GENMASK(21, 0)

#define MESON_C2_SAR_ADC_DELTA_10				0x28

#define MESON_C2_SAR_ADC_REG11					0x2c
	#define MESON_C2_SAR_ADC_REG11_EOC			BIT(7)
	#define MESON_C2_SAR_ADC_REG11_CHNL_REGS_EN		BIT(30)
	#define MESON_C2_SAR_ADC_REG11_FIFO_EN			BIT(31)

#define MESON_C2_SAR_ADC_REG12					0x30

#define MESON_C2_SAR_ADC_REG13					0x34
	#define MESON_C2_SAR_ADC_REG13_VREF_SEL			BIT(19)
	#define MESON_C2_SAR_ADC_REG13_VBG_SEL			BIT(16)
	#define MESON_C2_SAR_ADC_REG13_EN_VCM0P9		BIT(1)

#define	MESON_C2_SAR_ADC_REG14					0x38

#define MESON_C2_SAR_ADC_CH0_CTRL1				0x4c
#define MESON_C2_SAR_ADC_CH0_CTRL2				0x50
#define MESON_C2_SAR_ADC_CH0_CTRL3				0x54

#define MESON_C2_SAR_ADC_CHX_CTRL1(_chan)			\
	(MESON_C2_SAR_ADC_CH0_CTRL1 + (_chan) * 12)

#define MESON_C2_SAR_ADC_CHX_CTRL1_DECIM_FILTER			BIT(0)
#define MESON_C2_SAR_ADC_CHX_CTRL1_DIFF_EN			BIT(17)
#define MESON_C2_SAR_ADC_CHX_CTRL1_CMV_SEL			BIT(18)
#define MESON_C2_SAR_ADC_CHX_CTRL1_VREFP_SEL			BIT(19)
#define MESON_C2_SAR_ADC_CHX_CTRL1_IN_CTRL_MASK			GENMASK(23, 21)

#define MESON_C2_SAR_ADC_CHX_CTRL2(_chan)			\
	(MESON_C2_SAR_ADC_CH0_CTRL2 + (_chan) * 12)

#define MESON_C2_SAR_ADC_CHX_CTRL3(_chan)			\
	(MESON_C2_SAR_ADC_CH0_CTRL3 + (_chan) * 12)

#define MESON_C2_SAR_ADC_HCIC_CTRL1				0xac
#define MESON_C2_SAR_ADC_F1_CTRL				0xb0
#define MESON_C2_SAR_ADC_F2_CTRL				0xb4
#define MESON_C2_SAR_ADC_F3_CTRL				0xb8
#define MESON_C2_SAR_ADC_DECI_FILTER_CTRL			0xbc
#define MESON_C2_SAR_ADC_COEF_RAM_CNTL				0xc0
#define MESON_C2_SAR_ADC_COEF_RAM_DATA				0xc4

#define MESON_C2_SAR_ADC_FIFO_RD_NEW				0xc8

#define	MESON_C2_SAR_ADC_RAW					0xcc

#define MESON_C2_SAR_ADC_CHNLX_BASE				0xd0

#define MESON_C2_SAR_ADC_CHNLX_VALUE_MASK			GENMASK(21, 0)
#define MESON_C2_SAR_ADC_CHNLX_ID_MASK				GENMASK(24, 22)
#define MESON_C2_SAR_ADC_CHNLX_VALID_MASK			BIT(25)

#define MESON_C2_SAR_ADC_CHNL0					0xd0
#define MESON_C2_SAR_ADC_CHNL1					0xd4
#define MESON_C2_SAR_ADC_CHNL2					0xd8
#define MESON_C2_SAR_ADC_CHNL3					0xdc
#define MESON_C2_SAR_ADC_CHNL4					0xe0
#define MESON_C2_SAR_ADC_CHNL5					0xe4
#define MESON_C2_SAR_ADC_CHNL6					0xe8
#define MESON_C2_SAR_ADC_CHNL7					0xec

#define SARADC_C2_DISCARD_DATA_CNT				0x2b
#define SARADC_C2_SAVE_DATA_CNT					0x01
#define SARADC_C2_DECIM_FILTER_RESOLUTION			22

#define SARADC_C2_CONTINUOUSLY_MODE_CLOCK			8000000

#define MESON_C2_SAR_ADC_CHAN(_chan) {					       \
	.type = IIO_VOLTAGE,						       \
	.indexed = 1,							       \
	.channel = _chan,						       \
	.address = _chan,						       \
	.scan_index = _chan,						       \
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |			       \
			      BIT(IIO_CHAN_INFO_AVERAGE_RAW) |	               \
			      BIT(IIO_CHAN_INFO_PROCESSED) |		       \
			      BIT(IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY),\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),		       \
	.scan_type = {							       \
		.sign = 'u',						       \
		.storagebits = 32,					       \
		.shift = 0,						       \
		.endianness = IIO_CPU,					       \
	},								       \
	.datasheet_name = "SAR_ADC_CH"#_chan,				       \
}

static struct iio_chan_spec meson_c2_sar_adc_iio_channels[] = {
	MESON_C2_SAR_ADC_CHAN(0),
	MESON_C2_SAR_ADC_CHAN(1),
	MESON_C2_SAR_ADC_CHAN(2),
	MESON_C2_SAR_ADC_CHAN(3),
	MESON_C2_SAR_ADC_CHAN(4),
	MESON_C2_SAR_ADC_CHAN(5),
	MESON_C2_SAR_ADC_CHAN(6),
	MESON_C2_SAR_ADC_CHAN(7),
	IIO_CHAN_SOFT_TIMESTAMP(8),
};

static int meson_c2_sar_adc_extra_init(struct iio_dev *indio_dev)
{
	unsigned char i;
	unsigned int regval;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	regval = FIELD_PREP(MESON_C2_SAR_ADC_REG11_EOC, priv->param->adc_eoc);
	regmap_update_bits(priv->regmap, MESON_C2_SAR_ADC_REG11,
			   MESON_C2_SAR_ADC_REG11_EOC, regval);

	/* to select the VDDA if the vref is optional */
	regval = FIELD_PREP(MESON_C2_SAR_ADC_REG13_VBG_SEL, VDDA_AS_VREF);
	regval |= FIELD_PREP(MESON_C2_SAR_ADC_REG13_EN_VCM0P9, VDDA_AS_VREF);

	regmap_update_bits(priv->regmap, MESON_C2_SAR_ADC_REG13,
			   MESON_C2_SAR_ADC_REG13_VBG_SEL |
			   MESON_C2_SAR_ADC_REG13_EN_VCM0P9,
			   priv->param->vref_is_optional ? regval : 0);
	regmap_update_bits(priv->regmap, MESON_C2_SAR_ADC_REG14,
			   MESON_C2_SAR_ADC_REG13_VBG_SEL |
			   MESON_C2_SAR_ADC_REG13_EN_VCM0P9,
			   priv->param->vref_is_optional ? regval : 0);

	for (i = 0; i < indio_dev->num_channels; i++)
		regmap_update_bits(priv->regmap, MESON_C2_SAR_ADC_CHX_CTRL1(i),
				   MESON_C2_SAR_ADC_REG13_VBG_SEL |
				   MESON_C2_SAR_ADC_REG13_EN_VCM0P9,
				   priv->param->vref_is_optional ? regval : 0);

	return 0;
}

static void meson_c2_sar_adc_set_ch7_mux(struct iio_dev *indio_dev,
					 enum meson_sar_adc_chan7_mux_sel sel)
{
	unsigned int regval;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	regval = FIELD_PREP(MESON_C2_SAR_ADC_CHX_CTRL1_IN_CTRL_MASK, sel);
	regmap_update_bits(priv->regmap, MESON_C2_SAR_ADC_CHX_CTRL1(7),
			   MESON_C2_SAR_ADC_CHX_CTRL1_IN_CTRL_MASK, regval);

	priv->chan7_mux_sel = sel;

	usleep_range(10, 20);
}

static int meson_c2_sar_adc_read_fifo(struct iio_dev *indio_dev,
				      const struct iio_chan_spec *chan,
				      bool chk_channel)
{
	int fifo_chan;
	unsigned int regval;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	regmap_read(priv->regmap, MESON_C2_SAR_ADC_FIFO_RD, &regval);
	if (chk_channel) {
		fifo_chan = FIELD_GET(MESON_C2_SAR_ADC_FIFO_RD_CHAN_ID_MASK,
				      regval);
		if (fifo_chan != chan->address) {
			dev_err(&indio_dev->dev,
				"ADC FIFO entry belongs to channel %d instead of %lu\n",
				fifo_chan, chan->address);
			return -EINVAL;
		}
	}

	return FIELD_GET(MESON_C2_SAR_ADC_FIFO_RD_SAMPLE_VALUE_MASK, regval);
}

static void meson_c2_sar_adc_enable_chnl(struct iio_dev *indio_dev, bool en)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	regmap_update_bits(priv->regmap, MESON_C2_SAR_ADC_REG11,
			   MESON_C2_SAR_ADC_REG11_CHNL_REGS_EN,
			   en ? MESON_C2_SAR_ADC_REG11_CHNL_REGS_EN : 0);
}

static int meson_c2_sar_adc_read_chnl(struct iio_dev *indio_dev,
				      const struct iio_chan_spec *chan)
{
	bool is_valid;
	int channel;
	unsigned int regval;
	unsigned int data;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

again:
	regmap_read(priv->regmap,
		    MESON_C2_SAR_ADC_CHNLX_BASE + (chan->channel << 2),
		    &regval);

	is_valid  = FIELD_GET(MESON_C2_SAR_ADC_CHNLX_VALID_MASK, regval);
	if (!is_valid) {
		dev_err(&indio_dev->dev,
			"ADC chnl reg have no valid sampling data\n");

		usleep_range(3 * 1000, 5 * 1000);
		goto again;
	}

	channel = FIELD_GET(MESON_C2_SAR_ADC_CHNLX_ID_MASK, regval);
	if (channel != chan->channel) {
		dev_err(&indio_dev->dev,
			"ADC Dout entry belongs to channel %d instead of %d\n",
			channel, chan->channel);
		return -EINVAL;
	}

	data = FIELD_GET(MESON_C2_SAR_ADC_CHNLX_VALUE_MASK, regval);

	regmap_read(priv->regmap, MESON_C2_SAR_ADC_CHX_CTRL1(chan->address),
		    &regval);
	if (!FIELD_GET(MESON_C2_SAR_ADC_CHX_CTRL1_DECIM_FILTER, regval)) {
		/* check whether data is negative */
		if ((data >> (SARADC_C2_DECIM_FILTER_RESOLUTION - 1)) == 1)
			data = data - (1 << SARADC_C2_DECIM_FILTER_RESOLUTION);
		/* convert to unsigned int */
		data = (data + (1 << (SARADC_C2_DECIM_FILTER_RESOLUTION - 1)));

		/* convert to 12bit */
		data = data >> (SARADC_C2_DECIM_FILTER_RESOLUTION -
				priv->param->resolution);
	}

	return data;
}

static void meson_c2_sar_adc_init_ch(struct iio_dev *indio_dev,
				     const struct iio_chan_spec *chan)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	/* to select single-ended */
	regmap_update_bits(priv->regmap,
			   MESON_C2_SAR_ADC_CHX_CTRL1(chan->channel),
			   MESON_C2_SAR_ADC_CHX_CTRL1_DIFF_EN, 0x0);

	regmap_update_bits(priv->regmap,
			   MESON_C2_SAR_ADC_CHX_CTRL1(chan->channel),
			   MESON_C2_SAR_ADC_CHX_CTRL1_CMV_SEL,
			   FIELD_PREP(MESON_C2_SAR_ADC_CHX_CTRL1_CMV_SEL,
				      priv->param->vrefp_select));

	regmap_update_bits(priv->regmap,
			   MESON_C2_SAR_ADC_CHX_CTRL1(chan->channel),
			   MESON_C2_SAR_ADC_CHX_CTRL1_VREFP_SEL,
			   FIELD_PREP(MESON_C2_SAR_ADC_CHX_CTRL1_VREFP_SEL,
				      priv->param->cmv_select));
}

static void meson_c2_sar_adc_enable_decim_filter(struct iio_dev *indio_dev,
						 const struct iio_chan_spec *ch,
						 bool en)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	if (en) {
		regmap_write(priv->regmap,
			     MESON_C2_SAR_ADC_CHX_CTRL2(ch->channel),
			     SARADC_C2_DISCARD_DATA_CNT);
		regmap_write(priv->regmap,
			     MESON_C2_SAR_ADC_CHX_CTRL3(ch->channel),
			     SARADC_C2_SAVE_DATA_CNT);
		regmap_update_bits(priv->regmap,
				   MESON_C2_SAR_ADC_CHX_CTRL1(ch->channel),
				   MESON_C2_SAR_ADC_CHX_CTRL1_DECIM_FILTER, 0);
	} else {
		regmap_update_bits(priv->regmap,
				   MESON_C2_SAR_ADC_CHX_CTRL1(ch->channel),
				   MESON_C2_SAR_ADC_CHX_CTRL1_DECIM_FILTER,
				   MESON_C2_SAR_ADC_CHX_CTRL1_DECIM_FILTER);
	}
}

static int meson_c2_sar_adc_tuning_clock(struct iio_dev *indio_dev,
					 enum meson_sar_adc_sampling_mode mode)
{
	int ret;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	if (mode != SINGLE_MODE && mode != PERIOD_MODE)
		return -EINVAL;

	ret = clk_set_rate(priv->adc_clk, (mode == SINGLE_MODE) ?
					  priv->param->clock_rate :
					  SARADC_C2_CONTINUOUSLY_MODE_CLOCK);

	if (ret) {
		dev_err(indio_dev->dev.parent,
			"failed to set adc clock rate\n");
		return ret;
	}

	return 0;
}

static const struct meson_sar_adc_diff_ops meson_c2_diff_ops = {
	.extra_init = meson_c2_sar_adc_extra_init,
	.set_ch7_mux = meson_c2_sar_adc_set_ch7_mux,
	.read_fifo  = meson_c2_sar_adc_read_fifo,
	.enable_chnl = meson_c2_sar_adc_enable_chnl,
	.read_chnl = meson_c2_sar_adc_read_chnl,
	.init_ch = meson_c2_sar_adc_init_ch,
	.enable_decim_filter = meson_c2_sar_adc_enable_decim_filter,
	.tuning_clock = meson_c2_sar_adc_tuning_clock,
};

static const struct regmap_config meson_sar_adc_regmap_config_c2 = {
	.reg_bits = 8,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = MESON_C2_SAR_ADC_CHNL7,
};

const struct meson_sar_adc_param meson_sar_adc_c2_param __initconst = {
	.has_bl30_integration = false,
	.clock_rate = 600000,
	.regmap_config = &meson_sar_adc_regmap_config_c2,
	.resolution = 12,
	.vref_is_optional = false,
	.has_chnl_regs = true,
	.vrefp_select = 0,
	.cmv_select = 0,
	.adc_eoc = 1,
	.dops = &meson_c2_diff_ops,
	.channels = meson_c2_sar_adc_iio_channels,
	.num_channels = ARRAY_SIZE(meson_c2_sar_adc_iio_channels),
};
