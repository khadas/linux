// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "meson_saradc.h"

#define MESON_SAR_ADC_REG3					0x0c
	#define MESON_SAR_ADC_REG3_CTRL_CHAN7_MUX_SEL_MASK	GENMASK(25, 23)
	#define MESON_SAR_ADC_REG3_CTRL_CONT_RING_COUNTER_EN	BIT(27)

#define MESON_SAR_ADC_FIFO_RD					0x18
	#define MESON_SAR_ADC_FIFO_RD_CHAN_ID_MASK		GENMASK(14, 12)
	#define MESON_SAR_ADC_FIFO_RD_SAMPLE_VALUE_MASK		GENMASK(11, 0)

#define MESON_SAR_ADC_DELTA_10					0x28
	#define MESON_SAR_ADC_DELTA_10_TEMP_SEL			BIT(27)
	#define MESON_SAR_ADC_DELTA_10_TS_REVE1			BIT(26)
	#define MESON_SAR_ADC_DELTA_10_CHAN1_DELTA_VALUE_MASK	GENMASK(25, 16)
	#define MESON_SAR_ADC_DELTA_10_TS_REVE0			BIT(15)
	#define MESON_SAR_ADC_DELTA_10_TS_C_MASK		GENMASK(14, 11)
	#define MESON_SAR_ADC_DELTA_10_TS_VBG_EN		BIT(10)
	#define MESON_SAR_ADC_DELTA_10_CHAN0_DELTA_VALUE_MASK	GENMASK(9, 0)
/*
 * NOTE: registers from here are undocumented (the vendor Linux kernel driver
 * and u-boot source served as reference). These only seem to be relevant on
 * GXBB and newer.
 */
#define MESON_SAR_ADC_REG11					0x2c
	#define MESON_SAR_ADC_REG11_VREF_SEL			BIT(0)
	#define MESON_SAR_ADC_REG11_EOC				BIT(1)
	#define MESON_SAR_ADC_REG11_VREFP_SEL			BIT(5)
	#define MESON_SAR_ADC_REG11_CMV_SEL			BIT(6)
	#define MESON_SAR_ADC_REG11_TEMP_SEL			BIT(21)
	#define MESON_SAR_ADC_REG11_CHNL_REGS_EN		BIT(30)
	#define MESON_SAR_ADC_REG11_FIFO_EN			BIT(31)
	#define MESON_SAR_ADC_REG11_BANDGAP_EN			BIT(13)

#define MESON_SAR_ADC_REG13					0x34
	#define MESON_SAR_ADC_REG13_12BIT_CALIBRATION_MASK	GENMASK(13, 8)

/* NOTE: the registers below is introduced first on G12A platform */
#define MESON_SAR_ADC_CHNLX_BASE                                0x38
#define MESON_SAR_ADC_CHNLX_SAMPLE_VALUE_SHIFT(_chan)		\
					((_chan) * 16)
#define MESON_SAR_ADC_CHNLX_ID_SHIFT(_chan)			\
					(12 + (_chan) * 16)
#define MESON_SAR_ADC_CHNLX_VALID_SHIFT(_chan)			\
					(15 + (_chan) * 16)
#define MESON_SAR_ADC_CHNL01                                    0x38
#define MESON_SAR_ADC_CHNL23                                    0x3c
#define MESON_SAR_ADC_CHNL45                                    0x40
#define MESON_SAR_ADC_CHNL67                                    0x44

#define MESON_SAR_ADC_VOLTAGE_AND_TEMP_CHANNEL			6

#define MESON_SAR_ADC_CHAN(_chan) {					\
	.type = IIO_VOLTAGE,						\
	.indexed = 1,							\
	.channel = _chan,						\
	.address = _chan,						\
	.scan_index = _chan,						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |			\
				BIT(IIO_CHAN_INFO_AVERAGE_RAW) |	\
				BIT(IIO_CHAN_INFO_PROCESSED),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),		\
	.scan_type = {							\
		.sign = 'u',						\
		.storagebits = 16,					\
		.shift = 0,						\
		.endianness = IIO_CPU,					\
	},								\
	.datasheet_name = "SAR_ADC_CH"#_chan,				\
}

#define MESON_SAR_ADC_TEMP_CHAN(_chan) {				\
	.type = IIO_TEMP,						\
	.channel = _chan,						\
	.address = MESON_SAR_ADC_VOLTAGE_AND_TEMP_CHANNEL,		\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |			\
				BIT(IIO_CHAN_INFO_AVERAGE_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_OFFSET) |		\
					BIT(IIO_CHAN_INFO_SCALE),	\
	.datasheet_name = "TEMP_SENSOR",				\
}

static struct iio_chan_spec meson_m8_sar_adc_iio_channels[] = {
	MESON_SAR_ADC_CHAN(0),
	MESON_SAR_ADC_CHAN(1),
	MESON_SAR_ADC_CHAN(2),
	MESON_SAR_ADC_CHAN(3),
	MESON_SAR_ADC_CHAN(4),
	MESON_SAR_ADC_CHAN(5),
	MESON_SAR_ADC_CHAN(6),
	MESON_SAR_ADC_CHAN(7),
	IIO_CHAN_SOFT_TIMESTAMP(8),
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct iio_chan_spec meson_m8_sar_adc_and_temp_iio_channels[] = {
	MESON_SAR_ADC_CHAN(0),
	MESON_SAR_ADC_CHAN(1),
	MESON_SAR_ADC_CHAN(2),
	MESON_SAR_ADC_CHAN(3),
	MESON_SAR_ADC_CHAN(4),
	MESON_SAR_ADC_CHAN(5),
	MESON_SAR_ADC_CHAN(6),
	MESON_SAR_ADC_CHAN(7),
	MESON_SAR_ADC_TEMP_CHAN(8),
	IIO_CHAN_SOFT_TIMESTAMP(9),
};
#endif

static int meson_m8_sar_adc_extra_init(struct iio_dev *indio_dev)
{
	int regval;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	if (priv->temperature_sensor_calibrated) {
		regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELTA_10,
				   MESON_SAR_ADC_DELTA_10_TS_REVE1,
				   MESON_SAR_ADC_DELTA_10_TS_REVE1);
		regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELTA_10,
				   MESON_SAR_ADC_DELTA_10_TS_REVE0,
				   MESON_SAR_ADC_DELTA_10_TS_REVE0);

		/*
		 * set bits [3:0] of the TSC (temperature sensor coefficient)
		 * to get the correct values when reading the temperature.
		 */
		regval = FIELD_PREP(MESON_SAR_ADC_DELTA_10_TS_C_MASK,
				    priv->temperature_sensor_coefficient);
		regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELTA_10,
				   MESON_SAR_ADC_DELTA_10_TS_C_MASK, regval);
	} else {
		regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELTA_10,
				   MESON_SAR_ADC_DELTA_10_TS_REVE1, 0);
		regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELTA_10,
				   MESON_SAR_ADC_DELTA_10_TS_REVE0, 0);
	}

	regval = FIELD_PREP(MESON_SAR_ADC_REG11_EOC, priv->param->adc_eoc);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG11,
			   MESON_SAR_ADC_REG11_EOC, regval);

	/* disable internal ring counter */
	regval = FIELD_PREP(MESON_SAR_ADC_REG3_CTRL_CONT_RING_COUNTER_EN,
			    priv->param->disable_ring_counter);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_CTRL_CONT_RING_COUNTER_EN,
			   regval);

	/* to select the VDDA if the vref is optional */
	regval = FIELD_PREP(MESON_SAR_ADC_REG11_VREF_SEL, VDDA_AS_VREF);
	if (priv->param->vref_is_optional) {
		regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG11,
				   MESON_SAR_ADC_REG11_VREF_SEL, regval);
	}

	/* after g12a, select channel 6 input to external input */
	if (priv->param->has_chnl_regs) {
		regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG11,
				   MESON_SAR_ADC_REG11_TEMP_SEL, 0);
	}

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG11,
			   MESON_SAR_ADC_REG11_VREFP_SEL,
			   FIELD_PREP(MESON_SAR_ADC_REG11_VREFP_SEL,
				      priv->param->vrefp_select));

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG11,
			   MESON_SAR_ADC_REG11_CMV_SEL,
			   FIELD_PREP(MESON_SAR_ADC_REG11_CMV_SEL,
				      priv->param->cmv_select));

	return 0;
}

static void meson_m8_sar_adc_set_ch7_mux(struct iio_dev *indio_dev,
					 enum meson_sar_adc_chan7_mux_sel sel)
{
	unsigned int regval;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	regval = FIELD_PREP(MESON_SAR_ADC_REG3_CTRL_CHAN7_MUX_SEL_MASK, sel);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_CTRL_CHAN7_MUX_SEL_MASK, regval);

	priv->chan7_mux_sel = sel;

	usleep_range(10, 20);
}

static int meson_m8_sar_adc_read_fifo(struct iio_dev *indio_dev,
				      const struct iio_chan_spec *chan,
				      bool chk_channel)
{
	int fifo_chan;
	unsigned int regval;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	regmap_read(priv->regmap, MESON_SAR_ADC_FIFO_RD, &regval);

	if (chk_channel) {
		fifo_chan = FIELD_GET(MESON_SAR_ADC_FIFO_RD_CHAN_ID_MASK,
				      regval);
		if (fifo_chan != chan->address) {
			dev_err(&indio_dev->dev,
				"ADC FIFO entry belongs to channel %d instead of %lu\n",
				fifo_chan, chan->address);
			return -EINVAL;
		}
	}
	return FIELD_GET(MESON_SAR_ADC_FIFO_RD_SAMPLE_VALUE_MASK, regval);
}

/* enable chnl regs which save the sampling value for individual channel */
static void meson_m8_sar_adc_enable_chnl(struct iio_dev *indio_dev, bool en)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG11,
			   MESON_SAR_ADC_REG11_CHNL_REGS_EN,
			   en ? MESON_SAR_ADC_REG11_CHNL_REGS_EN : 0);
}

static int meson_m8_sar_adc_read_chnl(struct iio_dev *indio_dev,
				      const struct iio_chan_spec *chan)
{
	int grp_off;
	int chan_off;
	int fifo_chan;
	bool is_valid;
	unsigned int regval;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	grp_off  = (chan->channel / 2) << 2;
	chan_off = chan->channel % 2;

	regmap_read(priv->regmap, MESON_SAR_ADC_CHNLX_BASE + grp_off, &regval);

	is_valid = (regval >> MESON_SAR_ADC_CHNLX_VALID_SHIFT(chan_off)) & 0x1;
	if (!is_valid) {
		dev_err(&indio_dev->dev,
			"ADC chnl reg have no valid sampling data\n");
		return -EINVAL;
	}

	fifo_chan = (regval >> MESON_SAR_ADC_CHNLX_ID_SHIFT(chan_off)) & 0x7;
	if (fifo_chan != chan->channel) {
		dev_err(&indio_dev->dev,
			"ADC Dout entry belongs to channel %d instead of %d\n",
			fifo_chan, chan->channel);
		return -EINVAL;
	}

	regval = regval >> MESON_SAR_ADC_CHNLX_SAMPLE_VALUE_SHIFT(chan_off);
	regval &= GENMASK(priv->param->resolution - 1, 0);

	return regval;
}

static void meson_m8_sar_adc_set_bandgap(struct iio_dev *indio_dev, bool on_off)
{
	u32 enable_mask;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	if (priv->param->bandgap_reg == MESON_SAR_ADC_REG11)
		enable_mask = MESON_SAR_ADC_REG11_BANDGAP_EN;
	else
		enable_mask = MESON_SAR_ADC_DELTA_10_TS_VBG_EN;

	regmap_update_bits(priv->regmap, priv->param->bandgap_reg, enable_mask,
			   on_off ? enable_mask : 0);
}

static void meson_m8_sar_adc_select_temp(struct iio_dev *indio_dev,
					 const struct iio_chan_spec *chan)
{
	unsigned int regval;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	if (chan->address == MESON_SAR_ADC_VOLTAGE_AND_TEMP_CHANNEL) {
		if (chan->type == IIO_TEMP)
			regval = MESON_SAR_ADC_DELTA_10_TEMP_SEL;
		else
			regval = 0;

		regmap_update_bits(priv->regmap,
				   MESON_SAR_ADC_DELTA_10,
				   MESON_SAR_ADC_DELTA_10_TEMP_SEL, regval);
	}
}

static const struct meson_sar_adc_diff_ops meson_m8_diff_ops = {
	.extra_init = meson_m8_sar_adc_extra_init,
	.set_ch7_mux = meson_m8_sar_adc_set_ch7_mux,
	.read_fifo = meson_m8_sar_adc_read_fifo,
	.enable_chnl = meson_m8_sar_adc_enable_chnl,
	.read_chnl = meson_m8_sar_adc_read_chnl,
	.set_bandgap = meson_m8_sar_adc_set_bandgap,
	.select_temp = meson_m8_sar_adc_select_temp,
};

static const struct regmap_config meson_sar_adc_regmap_config_g12a = {
	.reg_bits = 8,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = MESON_SAR_ADC_CHNL67,
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static const struct regmap_config meson_sar_adc_regmap_config_gxbb = {
	.reg_bits = 8,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = MESON_SAR_ADC_REG13,
};

static const struct regmap_config meson_sar_adc_regmap_config_meson8 = {
	.reg_bits = 8,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = MESON_SAR_ADC_DELTA_10,
};
#endif

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
const struct meson_sar_adc_param meson_sar_adc_meson8_param __initconst = {
	.has_bl30_integration = false,
	.clock_rate = 1150000,
	.regmap_config = &meson_sar_adc_regmap_config_meson8,
	.resolution = 10,
	.temperature_trimming_bits = 4,
	.temperature_multiplier = 18 * 10000,
	.temperature_divider = 1024 * 10 * 85,
	.calib_enable = true,
	.dops = &meson_m8_diff_ops,
	.channels = meson_m8_sar_adc_and_temp_iio_channels,
	.num_channels = ARRAY_SIZE(meson_m8_sar_adc_and_temp_iio_channels),
};

const struct meson_sar_adc_param meson_sar_adc_meson8b_param __initconst = {
	.has_bl30_integration = false,
	.clock_rate = 1150000,
	.bandgap_reg = MESON_SAR_ADC_DELTA_10,
	.regmap_config = &meson_sar_adc_regmap_config_meson8,
	.resolution = 10,
	.calib_enable = true,
	.dops = &meson_m8_diff_ops,
	.channels = meson_m8_sar_adc_iio_channels,
	.num_channels = ARRAY_SIZE(meson_m8_sar_adc_iio_channels),
};

const struct meson_sar_adc_param meson_sar_adc_gxbb_param __initconst = {
	.has_bl30_integration = true,
	.clock_rate = 1200000,
	.bandgap_reg = MESON_SAR_ADC_REG11,
	.regmap_config = &meson_sar_adc_regmap_config_gxbb,
	.resolution = 10,
	.vrefp_select = 1,
	.cmv_select = 1,
	.calib_enable = true,
	.dops = &meson_m8_diff_ops,
	.channels = meson_m8_sar_adc_iio_channels,
	.num_channels = ARRAY_SIZE(meson_m8_sar_adc_iio_channels),
};

const struct meson_sar_adc_param meson_sar_adc_gxl_param __initconst = {
	.has_bl30_integration = true,
	.clock_rate = 1200000,
	.bandgap_reg = MESON_SAR_ADC_REG11,
	.regmap_config = &meson_sar_adc_regmap_config_gxbb,
	.resolution = 12,
	.disable_ring_counter = 1,
	.vrefp_select = 1,
	.cmv_select = 1,
	.calib_enable = true,
	.dops = &meson_m8_diff_ops,
	.channels = meson_m8_sar_adc_iio_channels,
	.num_channels = ARRAY_SIZE(meson_m8_sar_adc_iio_channels),
};

const struct meson_sar_adc_param meson_sar_adc_txlx_param __initconst = {
	.has_bl30_integration = true,
	.clock_rate = 1200000,
	.bandgap_reg = MESON_SAR_ADC_REG11,
	.regmap_config = &meson_sar_adc_regmap_config_gxbb,
	.resolution = 12,
	.vref_is_optional = true,
	.disable_ring_counter = 1,
	.vrefp_select = 1,
	.cmv_select = 1,
	.dops = &meson_m8_diff_ops,
	.channels = meson_m8_sar_adc_iio_channels,
	.num_channels = ARRAY_SIZE(meson_m8_sar_adc_iio_channels),
};
#endif

const struct meson_sar_adc_param meson_sar_adc_g12a_param __initconst = {
	.has_bl30_integration = false,
	.clock_rate = 1200000,
	.bandgap_reg = MESON_SAR_ADC_REG11,
	.regmap_config = &meson_sar_adc_regmap_config_g12a,
	.resolution = 12,
	.vref_is_optional = true,
	.disable_ring_counter = 1,
	.has_chnl_regs = true,
	.vrefp_select = 0,
	.cmv_select = 0,
	.adc_eoc = 1,
	.dops = &meson_m8_diff_ops,
	.channels = meson_m8_sar_adc_iio_channels,
	.num_channels = ARRAY_SIZE(meson_m8_sar_adc_iio_channels),
};
