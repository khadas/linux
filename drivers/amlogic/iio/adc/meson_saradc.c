// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/*
 * The sar adc is work in polling mode for single sampling, or work in IRQ mode
 * for periodic sampling.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iio/iio.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include "meson_saradc.h"

#define MESON_SAR_ADC_REG0					0x00
	#define MESON_SAR_ADC_REG0_PANEL_DETECT			BIT(31)
	#define MESON_SAR_ADC_REG0_BUSY_MASK			GENMASK(30, 28)
	#define MESON_SAR_ADC_REG0_DELTA_BUSY			BIT(30)
	#define MESON_SAR_ADC_REG0_AVG_BUSY			BIT(29)
	#define MESON_SAR_ADC_REG0_SAMPLE_BUSY			BIT(28)
	#define MESON_SAR_ADC_REG0_FIFO_FULL			BIT(27)
	#define MESON_SAR_ADC_REG0_FIFO_EMPTY			BIT(26)
	#define MESON_SAR_ADC_REG0_FIFO_COUNT_MASK		GENMASK(25, 21)
	#define MESON_SAR_ADC_REG0_ADC_BIAS_CTRL_MASK		GENMASK(20, 19)
	#define MESON_SAR_ADC_REG0_CURR_CHAN_ID_MASK		GENMASK(18, 16)
	#define MESON_SAR_ADC_REG0_ADC_TEMP_SEN_SEL		BIT(15)
	#define MESON_SAR_ADC_REG0_SAMPLING_STOP		BIT(14)
	#define MESON_SAR_ADC_REG0_CHAN_DELTA_EN_MASK		GENMASK(13, 12)
	#define MESON_SAR_ADC_REG0_DETECT_IRQ_POL		BIT(10)
	#define MESON_SAR_ADC_REG0_DETECT_IRQ_EN		BIT(9)
	#define MESON_SAR_ADC_REG0_FIFO_CNT_IRQ_MASK		GENMASK(8, 4)
	#define MESON_SAR_ADC_REG0_FIFO_IRQ_EN			BIT(3)
	#define MESON_SAR_ADC_REG0_SAMPLING_START		BIT(2)
	#define MESON_SAR_ADC_REG0_CONTINUOUS_EN		BIT(1)
	#define MESON_SAR_ADC_REG0_SAMPLE_ENGINE_ENABLE		BIT(0)

#define MESON_SAR_ADC_CHAN_LIST					0x04
	#define MESON_SAR_ADC_CHAN_LIST_MAX_INDEX_MASK		GENMASK(26, 24)
	#define MESON_SAR_ADC_CHAN_LIST_ENTRY_SHIFT(_chan)	((_chan) * 3)
	#define MESON_SAR_ADC_CHAN_LIST_ENTRY_MASK(_chan)	\
					(GENMASK(2, 0) << ((_chan) * 3))

#define MESON_SAR_ADC_AVG_CNTL					0x08
	#define MESON_SAR_ADC_AVG_CNTL_AVG_MODE_SHIFT(_chan)	\
					(16 + ((_chan) * 2))
	#define MESON_SAR_ADC_AVG_CNTL_AVG_MODE_MASK(_chan)	\
					(GENMASK(17, 16) << ((_chan) * 2))
	#define MESON_SAR_ADC_AVG_CNTL_NUM_SAMPLES_SHIFT(_chan)	\
					(0 + ((_chan) * 2))
	#define MESON_SAR_ADC_AVG_CNTL_NUM_SAMPLES_MASK(_chan)	\
					(GENMASK(1, 0) << ((_chan) * 2))

#define MESON_SAR_ADC_REG3					0x0c
	#define MESON_SAR_ADC_REG3_CNTL_USE_SC_DLY		BIT(31)
	#define MESON_SAR_ADC_REG3_CLK_EN			BIT(30)
	#define MESON_SAR_ADC_REG3_BL30_INITIALIZED		BIT(28)
	#define MESON_SAR_ADC_REG3_CTRL_SAMPLING_CLOCK_PHASE	BIT(26)
	#define MESON_SAR_ADC_REG3_DETECT_EN			BIT(22)
	#define MESON_SAR_ADC_REG3_ADC_EN			BIT(21)
	#define MESON_SAR_ADC_REG3_PANEL_DETECT_COUNT_MASK	GENMASK(20, 18)
	#define MESON_SAR_ADC_REG3_PANEL_DETECT_FILTER_TB_MASK	GENMASK(17, 16)
	#define MESON_SAR_ADC_REG3_ADC_CLK_DIV_SHIFT		10
	#define MESON_SAR_ADC_REG3_ADC_CLK_DIV_WIDTH		6
	#define MESON_SAR_ADC_REG3_BLOCK_DLY_SEL_MASK		GENMASK(9, 8)
	#define MESON_SAR_ADC_REG3_BLOCK_DLY_MASK		GENMASK(7, 0)

#define MESON_SAR_ADC_DELAY					0x10
	#define MESON_SAR_ADC_DELAY_INPUT_DLY_SEL_MASK		GENMASK(25, 24)
	#define MESON_SAR_ADC_DELAY_BL30_BUSY			BIT(15)
	#define MESON_SAR_ADC_DELAY_KERNEL_BUSY			BIT(14)
	#define MESON_SAR_ADC_DELAY_INPUT_DLY_CNT_MASK		GENMASK(23, 16)
	#define MESON_SAR_ADC_DELAY_SAMPLE_DLY_SEL_MASK		GENMASK(9, 8)
	#define MESON_SAR_ADC_DELAY_SAMPLE_DLY_CNT_MASK		GENMASK(7, 0)

#define MESON_SAR_ADC_LAST_RD					0x14
	#define MESON_SAR_ADC_LAST_RD_LAST_CHANNEL1_MASK	GENMASK(23, 16)
	#define MESON_SAR_ADC_LAST_RD_LAST_CHANNEL0_MASK	GENMASK(9, 0)

#define MESON_SAR_ADC_AUX_SW					0x1c
	#define MESON_SAR_ADC_AUX_SW_MUX_SEL_CHAN_SHIFT(_chan)	\
					(8 + (((_chan) - 2) * 3))
	#define MESON_SAR_ADC_AUX_SW_VREF_P_MUX			BIT(6)
	#define MESON_SAR_ADC_AUX_SW_VREF_N_MUX			BIT(5)
	#define MESON_SAR_ADC_AUX_SW_MODE_SEL			BIT(4)
	#define MESON_SAR_ADC_AUX_SW_YP_DRIVE_SW		BIT(3)
	#define MESON_SAR_ADC_AUX_SW_XP_DRIVE_SW		BIT(2)
	#define MESON_SAR_ADC_AUX_SW_YM_DRIVE_SW		BIT(1)
	#define MESON_SAR_ADC_AUX_SW_XM_DRIVE_SW		BIT(0)

#define MESON_SAR_ADC_CHAN_10_SW				0x20
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_MUX_SEL_MASK	GENMASK(25, 23)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_VREF_P_MUX	BIT(22)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_VREF_N_MUX	BIT(21)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_MODE_SEL		BIT(20)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_YP_DRIVE_SW	BIT(19)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_XP_DRIVE_SW	BIT(18)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_YM_DRIVE_SW	BIT(17)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN1_XM_DRIVE_SW	BIT(16)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_MUX_SEL_MASK	GENMASK(9, 7)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_VREF_P_MUX	BIT(6)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_VREF_N_MUX	BIT(5)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_MODE_SEL		BIT(4)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_YP_DRIVE_SW	BIT(3)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_XP_DRIVE_SW	BIT(2)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_YM_DRIVE_SW	BIT(1)
	#define MESON_SAR_ADC_CHAN_10_SW_CHAN0_XM_DRIVE_SW	BIT(0)

#define MESON_SAR_ADC_DETECT_IDLE_SW				0x24
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_SW_EN	BIT(26)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_MUX_MASK	GENMASK(25, 23)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_VREF_P_MUX	BIT(22)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_VREF_N_MUX	BIT(21)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_MODE_SEL	BIT(20)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_YP_DRIVE_SW	BIT(19)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_XP_DRIVE_SW	BIT(18)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_YM_DRIVE_SW	BIT(17)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_DETECT_XM_DRIVE_SW	BIT(16)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_MUX_SEL_MASK	GENMASK(9, 7)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_VREF_P_MUX	BIT(6)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_VREF_N_MUX	BIT(5)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_MODE_SEL	BIT(4)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_YP_DRIVE_SW	BIT(3)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_XP_DRIVE_SW	BIT(2)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_YM_DRIVE_SW	BIT(1)
	#define MESON_SAR_ADC_DETECT_IDLE_SW_IDLE_XM_DRIVE_SW	BIT(0)

#define MESON_SAR_ADC_MAX_FIFO_SIZE				32
#define MESON_SAR_ADC_TIMEOUT					100 /* ms */
#define MESON_SAR_ADC_TEMP_OFFSET				27
#define MESON_SAR_ADC_PM_TIMEOUT				5000 /* ms */

/* temperature sensor calibration information in eFuse */
#define MESON_SAR_ADC_EFUSE_BYTES				4
#define MESON_SAR_ADC_EFUSE_BYTE3_UPPER_ADC_VAL			GENMASK(6, 0)
#define MESON_SAR_ADC_EFUSE_BYTE3_IS_CALIBRATED			BIT(7)

/* for use with IIO_VAL_INT_PLUS_MICRO */
#define MILLION							1000000

#define SAR_ADC_DEF_VREF					1800000	/* uV */

static const char * const chan7_vol[] = {
	"gnd",
	"vdd/4",
	"vdd/2",
	"vdd*3/4",
	"vdd",
	"ch7_input",
	"ch7_input",
	"ch7_input",
};

enum meson_sar_adc_filter_mode {
	NO_FILTER = 0x0,
	MEAN_AVERAGING_FILTER = 0x1,
	MEDIAN_AVERAGING_FILTER = 0x2,
	DECIMATION_FILTER = 0x3,
};

enum meson_sar_adc_num_samples {
	ONE_SAMPLE = 0x0,
	TWO_SAMPLES = 0x1,
	FOUR_SAMPLES = 0x2,
	EIGHT_SAMPLES = 0x3,
};

struct meson_sar_adc_data {
	const struct meson_sar_adc_param	*param;
	const char				*name;
};

static bool meson_sar_adc_pm_runtime_supported(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	/* if the saradc is shared with bl30 it should't enable pm runtime */
	if (priv->param->has_bl30_integration)
		return false;

	return true;
}

static unsigned int meson_sar_adc_get_fifo_count(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	u32 regval;

	regmap_read(priv->regmap, MESON_SAR_ADC_REG0, &regval);

	return FIELD_GET(MESON_SAR_ADC_REG0_FIFO_COUNT_MASK, regval);
}

static int meson_sar_adc_calib_val(struct iio_dev *indio_dev, int val)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int tmp;

	/* use val_calib = scale * val_raw + offset calibration function */
	tmp = div_s64((s64)val * priv->calibscale, MILLION) + priv->calibbias;

	return clamp(tmp, 0, (1 << priv->param->resolution) - 1);
}

static int meson_sar_adc_wait_busy_clear(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	/* C2/A5 need more time to sample while decim filter is enable */
	int regval, timeout = 20000;

	/*
	 * NOTE: we need a small delay before reading the status, otherwise
	 * the sample engine may not have started internally (which would
	 * seem to us that sampling is already finished).
	 */
	do {
		udelay(1);
		regmap_read(priv->regmap, MESON_SAR_ADC_REG0, &regval);
	} while (FIELD_GET(MESON_SAR_ADC_REG0_BUSY_MASK, regval) && timeout--);

	if (timeout < 0)
		return -ETIMEDOUT;

	return 0;
}

static int meson_sar_adc_read_raw_sample(struct iio_dev *indio_dev,
					 const struct iio_chan_spec *chan,
					 int *val)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int fifo_val, count;

	if (meson_sar_adc_wait_busy_clear(indio_dev))
		return -ETIMEDOUT;

	count = meson_sar_adc_get_fifo_count(indio_dev);
	if (count != 1) {
		dev_err(&indio_dev->dev,
			"ADC FIFO has %d element(s) instead of one\n", count);
		return -EINVAL;
	}

	fifo_val = priv->param->dops->read_fifo(indio_dev, chan, true);

	if (priv->param->calib_enable)
		/* to fix the sample value by software */
		*val = meson_sar_adc_calib_val(indio_dev, fifo_val);
	else
		*val = fifo_val;

	return 0;
}

static void meson_sar_adc_set_averaging(struct iio_dev *indio_dev,
					const struct iio_chan_spec *chan,
					enum meson_sar_adc_filter_mode mode,
					enum meson_sar_adc_num_samples samples)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int val, address = chan->address;

	val = samples << MESON_SAR_ADC_AVG_CNTL_NUM_SAMPLES_SHIFT(address);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_AVG_CNTL,
			   MESON_SAR_ADC_AVG_CNTL_NUM_SAMPLES_MASK(address),
			   val);

	val = mode << MESON_SAR_ADC_AVG_CNTL_AVG_MODE_SHIFT(address);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_AVG_CNTL,
			   MESON_SAR_ADC_AVG_CNTL_AVG_MODE_MASK(address), val);
}

static void meson_sar_adc_enable_channel(struct iio_dev *indio_dev,
					 const struct iio_chan_spec *chan,
					 unsigned char idx)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	u32 regval;

	regval = FIELD_PREP(MESON_SAR_ADC_CHAN_LIST_MAX_INDEX_MASK, idx);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_CHAN_LIST,
			   MESON_SAR_ADC_CHAN_LIST_MAX_INDEX_MASK, regval);

	regval = chan->channel << MESON_SAR_ADC_CHAN_LIST_ENTRY_SHIFT(idx);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_CHAN_LIST,
			   MESON_SAR_ADC_CHAN_LIST_ENTRY_MASK(idx), regval);

	if (priv->param->dops->select_temp)
		priv->param->dops->select_temp(indio_dev, chan);
}

static void meson_sar_adc_start_sample_engine(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			   MESON_SAR_ADC_REG0_SAMPLE_ENGINE_ENABLE,
			   MESON_SAR_ADC_REG0_SAMPLE_ENGINE_ENABLE);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			   MESON_SAR_ADC_REG0_SAMPLING_START,
			   MESON_SAR_ADC_REG0_SAMPLING_START);
}

static void meson_sar_adc_stop_sample_engine(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			   MESON_SAR_ADC_REG0_SAMPLING_STOP,
			   MESON_SAR_ADC_REG0_SAMPLING_STOP);

	/* wait until all modules are stopped */
	meson_sar_adc_wait_busy_clear(indio_dev);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			   MESON_SAR_ADC_REG0_SAMPLE_ENGINE_ENABLE, 0);
}

static int meson_sar_adc_lock(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int val, timeout = 10000;

	mutex_lock(&indio_dev->mlock);

	if (priv->param->has_bl30_integration) {
again:
		/* wait until BL30 releases it's lock (so we can use
		 * the SAR ADC)
		 */
		do {
			udelay(1);
			regmap_read(priv->regmap, MESON_SAR_ADC_DELAY, &val);
		} while (val & MESON_SAR_ADC_DELAY_BL30_BUSY && timeout--);

		if (timeout < 0) {
			mutex_unlock(&indio_dev->mlock);
			return -ETIMEDOUT;
		}
		/* prevent BL30 from using the SAR ADC while we are using it */
		regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELAY,
				   MESON_SAR_ADC_DELAY_KERNEL_BUSY,
				   MESON_SAR_ADC_DELAY_KERNEL_BUSY);
		isb();
		dsb(sy);
		udelay(5);
		regmap_read(priv->regmap, MESON_SAR_ADC_DELAY, &val);
		if (val & MESON_SAR_ADC_DELAY_BL30_BUSY)
			goto again;
	}

	return 0;
}

static void meson_sar_adc_unlock(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	if (priv->param->has_bl30_integration) {
		/* allow BL30 to use the SAR ADC again */
		regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELAY,
				   MESON_SAR_ADC_DELAY_KERNEL_BUSY, 0);
		isb();
		dsb(sy);
		udelay(5);
	}

	mutex_unlock(&indio_dev->mlock);
}

static void meson_sar_adc_clear_fifo(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	unsigned int count;

	for (count = 0; count < MESON_SAR_ADC_MAX_FIFO_SIZE; count++) {
		if (!meson_sar_adc_get_fifo_count(indio_dev))
			break;

		priv->param->dops->read_fifo(indio_dev, NULL, false);
	}
}

static int
meson_sar_adc_read_raw_sample_from_chnl(struct iio_dev *indio_dev,
					const struct iio_chan_spec *chan,
					int *val)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int fifo_val;

	fifo_val = priv->param->dops->read_chnl(indio_dev, chan);

	if (priv->param->calib_enable)
		/* to fix the sample value by software */
		*val = meson_sar_adc_calib_val(indio_dev, fifo_val);
	else
		*val = fifo_val;

	return 0;
}

static int meson_sar_adc_get_sample(struct iio_dev *indio_dev,
				    const struct iio_chan_spec *chan,
				    enum meson_sar_adc_filter_mode filter_mode,
				    enum meson_sar_adc_num_samples samples,
				    int *val)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int ret;

	if (chan->type == IIO_TEMP && !priv->temperature_sensor_calibrated)
		return -ENOTSUPP;

	ret = meson_sar_adc_lock(indio_dev);
	if (ret)
		return ret;

	if (iio_buffer_enabled(indio_dev)) {
		ret = meson_sar_adc_read_raw_sample_from_chnl(indio_dev,
							      chan,
							      val);
		meson_sar_adc_unlock(indio_dev);

		return (ret == 0) ? IIO_VAL_INT : ret;
	}

	if (meson_sar_adc_pm_runtime_supported(indio_dev)) {
		ret = pm_runtime_get_sync(indio_dev->dev.parent);
		if (ret < 0) {
			meson_sar_adc_unlock(indio_dev);
			return ret;
		}
	}

	/* clear the FIFO to make sure we're not reading old values */
	meson_sar_adc_clear_fifo(indio_dev);

	if (filter_mode == DECIMATION_FILTER) {
		if (priv->param->dops->enable_decim_filter)
			priv->param->dops->enable_decim_filter(indio_dev,
							       chan, true);
	} else {
		if (priv->param->dops->enable_decim_filter)
			priv->param->dops->enable_decim_filter(indio_dev,
							       chan, false);
		meson_sar_adc_set_averaging(indio_dev, chan,
					    filter_mode, samples);
	}
	meson_sar_adc_enable_channel(indio_dev, chan, 0);

	meson_sar_adc_start_sample_engine(indio_dev);
	ret = meson_sar_adc_read_raw_sample(indio_dev, chan, val);
	meson_sar_adc_stop_sample_engine(indio_dev);

	if (meson_sar_adc_pm_runtime_supported(indio_dev)) {
		pm_runtime_mark_last_busy(indio_dev->dev.parent);
		pm_runtime_put_autosuspend(indio_dev->dev.parent);
	}

	meson_sar_adc_unlock(indio_dev);

	if (ret) {
		dev_warn(indio_dev->dev.parent,
			 "failed to read sample for channel %lu: %d\n",
			 chan->address, ret);
		return ret;
	}

	return IIO_VAL_INT;
}

static int meson_sar_adc_iio_info_read_raw(struct iio_dev *indio_dev,
					   const struct iio_chan_spec *chan,
					   int *val, int *val2, long mask)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
	case IIO_CHAN_INFO_AVERAGE_RAW:
		return meson_sar_adc_get_sample(indio_dev, chan,
						MEDIAN_AVERAGING_FILTER,
						EIGHT_SAMPLES,
						val);

	case IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY:
		return meson_sar_adc_get_sample(indio_dev, chan,
						DECIMATION_FILTER,
						ONE_SAMPLE, val);

	case IIO_CHAN_INFO_PROCESSED:
		ret = meson_sar_adc_get_sample(indio_dev, chan,
					       MEDIAN_AVERAGING_FILTER,
					       EIGHT_SAMPLES,
					       val);
		if (ret < 0)
			return ret;

		/* return the 10-bit sampling value */
		if (priv->param->resolution == 12)
			*val = *val >> 2;

		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		if (chan->type == IIO_VOLTAGE) {
			ret = regulator_get_voltage(priv->vref);
			if (ret < 0) {
				/* use the default vref voltage */
				ret = SAR_ADC_DEF_VREF;
			}

			*val = ret / 1000;
			*val2 = priv->param->resolution;
			return IIO_VAL_FRACTIONAL_LOG2;
		} else if (chan->type == IIO_TEMP) {
			/* SoC specific multiplier and divider */
			*val = priv->param->temperature_multiplier;
			*val2 = priv->param->temperature_divider;

			/* celsius to millicelsius */
			*val *= 1000;

			return IIO_VAL_FRACTIONAL;
		} else {
			return -EINVAL;
		}

	case IIO_CHAN_INFO_CALIBBIAS:
		if (!priv->param->calib_enable)
			return -EINVAL;

		*val = priv->calibbias;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_CALIBSCALE:
		if (!priv->param->calib_enable)
			return -EINVAL;

		*val = priv->calibscale / MILLION;
		*val2 = priv->calibscale % MILLION;
		return IIO_VAL_INT_PLUS_MICRO;

	case IIO_CHAN_INFO_OFFSET:
		*val = DIV_ROUND_CLOSEST(MESON_SAR_ADC_TEMP_OFFSET *
					 priv->param->temperature_divider,
					 priv->param->temperature_multiplier);
		*val -= priv->temperature_sensor_adc_val;
		return IIO_VAL_INT;

	default:
		return -EINVAL;
	}
}

static int meson_sar_adc_clk_init(struct iio_dev *indio_dev,
				  void __iomem *base)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	struct clk_init_data init;
	const char *clk_parents[1];

	init.name = devm_kasprintf(&indio_dev->dev, GFP_KERNEL, "%s#adc_div",
				   dev_name(indio_dev->dev.parent));
	if (!init.name)
		return -ENOMEM;

	init.flags = 0;
	init.ops = &clk_divider_ops;
	clk_parents[0] = __clk_get_name(priv->clkin);
	init.parent_names = clk_parents;
	init.num_parents = 1;

	priv->clk_div.reg = base + MESON_SAR_ADC_REG3;
	priv->clk_div.shift = MESON_SAR_ADC_REG3_ADC_CLK_DIV_SHIFT;
	priv->clk_div.width = MESON_SAR_ADC_REG3_ADC_CLK_DIV_WIDTH;
	priv->clk_div.hw.init = &init;
	priv->clk_div.flags = 0;

	priv->adc_div_clk = devm_clk_register(&indio_dev->dev,
					      &priv->clk_div.hw);
	if (WARN_ON(IS_ERR(priv->adc_div_clk)))
		return PTR_ERR(priv->adc_div_clk);

	init.name = devm_kasprintf(&indio_dev->dev, GFP_KERNEL, "%s#adc_en",
				   dev_name(indio_dev->dev.parent));
	if (!init.name)
		return -ENOMEM;

	init.flags = CLK_SET_RATE_PARENT;
	init.ops = &clk_gate_ops;
	clk_parents[0] = __clk_get_name(priv->adc_div_clk);
	init.parent_names = clk_parents;
	init.num_parents = 1;

	priv->clk_gate.reg = base + MESON_SAR_ADC_REG3;
	priv->clk_gate.bit_idx = __ffs(MESON_SAR_ADC_REG3_CLK_EN);
	priv->clk_gate.hw.init = &init;

	priv->adc_clk = devm_clk_register(&indio_dev->dev, &priv->clk_gate.hw);
	if (WARN_ON(IS_ERR(priv->adc_clk)))
		return PTR_ERR(priv->adc_clk);

	return 0;
}

static int meson_sar_adc_temp_sensor_init(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	u8 *buf, trimming_bits, trimming_mask, upper_adc_val;
	struct nvmem_cell *temperature_calib;
	size_t read_len;
	int ret;

	temperature_calib = devm_nvmem_cell_get(&indio_dev->dev,
						"temperature_calib");
	if (IS_ERR(temperature_calib)) {
		ret = PTR_ERR(temperature_calib);

		/*
		 * leave the temperature sensor disabled if no calibration data
		 * was passed via nvmem-cells.
		 */
		if (ret == -ENODEV)
			return 0;

		if (ret != -EPROBE_DEFER)
			dev_err(indio_dev->dev.parent,
				"failed to get temperature_calib cell\n");

		return ret;
	}

	read_len = MESON_SAR_ADC_EFUSE_BYTES;
	buf = nvmem_cell_read(temperature_calib, &read_len);
	if (IS_ERR(buf)) {
		dev_err(indio_dev->dev.parent,
			"failed to read temperature_calib cell\n");
		return PTR_ERR(buf);
	} else if (read_len != MESON_SAR_ADC_EFUSE_BYTES) {
		kfree(buf);
		dev_err(indio_dev->dev.parent,
			"invalid read size of temperature_calib cell\n");
		return -EINVAL;
	}

	trimming_bits = priv->param->temperature_trimming_bits;
	trimming_mask = BIT(trimming_bits) - 1;

	priv->temperature_sensor_calibrated =
		buf[3] & MESON_SAR_ADC_EFUSE_BYTE3_IS_CALIBRATED;
	priv->temperature_sensor_coefficient = buf[2] & trimming_mask;

	upper_adc_val = FIELD_GET(MESON_SAR_ADC_EFUSE_BYTE3_UPPER_ADC_VAL,
				  buf[3]);

	priv->temperature_sensor_adc_val = buf[2];
	priv->temperature_sensor_adc_val |= upper_adc_val << BITS_PER_BYTE;
	priv->temperature_sensor_adc_val >>= trimming_bits;

	kfree(buf);

	return 0;
}

static int meson_sar_adc_uninit(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	clk_disable_unprepare(priv->core_clk);

	return 0;
}

static int meson_sar_adc_init(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int regval, i, ret;

	ret = clk_prepare_enable(priv->core_clk);
	if (ret) {
		dev_err(indio_dev->dev.parent, "failed to enable core clk\n");
		return ret;
	}

	if (priv->param->has_bl30_integration) {
		/*
		 * leave sampling delay and the input clocks as configured by
		 * BL30 to make sure BL30 gets the values it expects when
		 * reading the temperature sensor.
		 */
		regmap_read(priv->regmap, MESON_SAR_ADC_REG3, &regval);
		if (regval & MESON_SAR_ADC_REG3_BL30_INITIALIZED)
			return 0;
	}

	meson_sar_adc_stop_sample_engine(indio_dev);

	/*
	 * disable this bit as seems to be only relevant for Meson6 (based
	 * on the vendor driver), which we don't support at the moment.
	 */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			   MESON_SAR_ADC_REG0_ADC_TEMP_SEN_SEL, 0);

	/* disable all channels by default */
	regmap_write(priv->regmap, MESON_SAR_ADC_CHAN_LIST, 0x0);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_CTRL_SAMPLING_CLOCK_PHASE, 0);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_CNTL_USE_SC_DLY,
			   MESON_SAR_ADC_REG3_CNTL_USE_SC_DLY);

	/* delay between two samples = (10+1) * 1uS */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELAY,
			   MESON_SAR_ADC_DELAY_INPUT_DLY_CNT_MASK,
			   FIELD_PREP(MESON_SAR_ADC_DELAY_SAMPLE_DLY_CNT_MASK,
				      10));
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELAY,
			   MESON_SAR_ADC_DELAY_SAMPLE_DLY_SEL_MASK,
			   FIELD_PREP(MESON_SAR_ADC_DELAY_SAMPLE_DLY_SEL_MASK,
				      0));

	/* delay between two samples = (10+1) * 1uS */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELAY,
			   MESON_SAR_ADC_DELAY_INPUT_DLY_CNT_MASK,
			   FIELD_PREP(MESON_SAR_ADC_DELAY_INPUT_DLY_CNT_MASK,
				      10));
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_DELAY,
			   MESON_SAR_ADC_DELAY_INPUT_DLY_SEL_MASK,
			   FIELD_PREP(MESON_SAR_ADC_DELAY_INPUT_DLY_SEL_MASK,
				      1));

	/*
	 * set up the input channel muxes in MESON_SAR_ADC_CHAN_10_SW
	 * (0 = SAR_ADC_CH0, 1 = SAR_ADC_CH1)
	 */
	regval = FIELD_PREP(MESON_SAR_ADC_CHAN_10_SW_CHAN0_MUX_SEL_MASK, 0);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_CHAN_10_SW,
			   MESON_SAR_ADC_CHAN_10_SW_CHAN0_MUX_SEL_MASK,
			   regval);
	regval = FIELD_PREP(MESON_SAR_ADC_CHAN_10_SW_CHAN1_MUX_SEL_MASK, 1);
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_CHAN_10_SW,
			   MESON_SAR_ADC_CHAN_10_SW_CHAN1_MUX_SEL_MASK,
			   regval);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_CHAN_10_SW,
			   MESON_SAR_ADC_CHAN_10_SW_CHAN0_XP_DRIVE_SW,
			   MESON_SAR_ADC_CHAN_10_SW_CHAN0_XP_DRIVE_SW);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_CHAN_10_SW,
			   MESON_SAR_ADC_CHAN_10_SW_CHAN0_YP_DRIVE_SW,
			   MESON_SAR_ADC_CHAN_10_SW_CHAN0_YP_DRIVE_SW);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_CHAN_10_SW,
			   MESON_SAR_ADC_CHAN_10_SW_CHAN1_XP_DRIVE_SW,
			   MESON_SAR_ADC_CHAN_10_SW_CHAN1_XP_DRIVE_SW);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_CHAN_10_SW,
			   MESON_SAR_ADC_CHAN_10_SW_CHAN1_YP_DRIVE_SW,
			   MESON_SAR_ADC_CHAN_10_SW_CHAN1_YP_DRIVE_SW);

	/*
	 * set up the input channel muxes in MESON_SAR_ADC_AUX_SW
	 * (2 = SAR_ADC_CH2, 3 = SAR_ADC_CH3, ...) and enable
	 * MESON_SAR_ADC_AUX_SW_YP_DRIVE_SW and
	 * MESON_SAR_ADC_AUX_SW_XP_DRIVE_SW like the vendor driver.
	 */
	regval = 0;
	for (i = 2; i <= 7; i++)
		regval |= i << MESON_SAR_ADC_AUX_SW_MUX_SEL_CHAN_SHIFT(i);
	regval |= MESON_SAR_ADC_AUX_SW_YP_DRIVE_SW;
	regval |= MESON_SAR_ADC_AUX_SW_XP_DRIVE_SW;
	regmap_write(priv->regmap, MESON_SAR_ADC_AUX_SW, regval);

	if (priv->param->dops->extra_init)
		priv->param->dops->extra_init(indio_dev);

	ret = clk_set_parent(priv->adc_sel_clk, priv->clkin);
	if (ret) {
		dev_err(indio_dev->dev.parent,
			"failed to set adc parent to clkin\n");
		return ret;
	}

	ret = clk_set_rate(priv->adc_clk, priv->param->clock_rate);
	if (ret) {
		dev_err(indio_dev->dev.parent,
			"failed to set adc clock rate\n");
		return ret;
	}

	return 0;
}

static int meson_sar_adc_hw_enable_unlock(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int ret;

	ret = regulator_enable(priv->vref);
	if (ret < 0) {
		dev_err(indio_dev->dev.parent,
			"failed to enable vref regulator\n");
		goto err_vref;
	}

	if (priv->param->dops->set_bandgap)
		priv->param->dops->set_bandgap(indio_dev, true);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_ADC_EN,
			   MESON_SAR_ADC_REG3_ADC_EN);

	udelay(5);

	ret = clk_prepare_enable(priv->adc_clk);
	if (ret) {
		dev_err(indio_dev->dev.parent, "failed to enable adc clk\n");
		goto err_adc_clk;
	}

	return 0;

err_adc_clk:
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_ADC_EN, 0);
	if (priv->param->dops->set_bandgap)
		priv->param->dops->set_bandgap(indio_dev, false);
	regulator_disable(priv->vref);
err_vref:
	return ret;
}

static int meson_sar_adc_hw_enable(struct iio_dev *indio_dev)
{
	int ret;

	ret = meson_sar_adc_lock(indio_dev);
	if (ret)
		return ret;

	ret = meson_sar_adc_hw_enable_unlock(indio_dev);

	meson_sar_adc_unlock(indio_dev);

	return ret;
}

static int meson_sar_adc_hw_disable_unlock(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	clk_disable_unprepare(priv->adc_clk);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_ADC_EN, 0);

	if (priv->param->dops->set_bandgap)
		priv->param->dops->set_bandgap(indio_dev, false);

	regulator_disable(priv->vref);

	return 0;
}

static int meson_sar_adc_hw_disable(struct iio_dev *indio_dev)
{
	int ret;

	ret = meson_sar_adc_lock(indio_dev);
	if (ret)
		return ret;

	meson_sar_adc_hw_disable_unlock(indio_dev);

	meson_sar_adc_unlock(indio_dev);

	return 0;
}

static irqreturn_t meson_sar_adc_irq(int irq, void *data)
{
	struct iio_dev *indio_dev = data;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	unsigned int cnt, threshold;
	u32 regval;

	regmap_read(priv->regmap, MESON_SAR_ADC_REG0, &regval);
	cnt = FIELD_GET(MESON_SAR_ADC_REG0_FIFO_COUNT_MASK, regval);
	threshold = FIELD_GET(MESON_SAR_ADC_REG0_FIFO_CNT_IRQ_MASK, regval);

	if (cnt < threshold)
		return IRQ_NONE;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t meson_sar_adc_worker(int irq, void *data)
{
	struct iio_dev *indio_dev = data;
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	u16 fifo_cnt;
	u32 fifo_val;
	u16 buf_offset;
	u32 i = 0;
	u32 j = 0;

	fifo_cnt = meson_sar_adc_get_fifo_count(indio_dev);

	memset(priv->datum_buf, 0, indio_dev->scan_bytes);

	if (((indio_dev->scan_bytes - 8) / priv->active_channel_cnt) == 4)
		buf_offset = 2;		/* 4 bytes per channel buf */
	else
		buf_offset = 1;		/* 2 bytes per channel buf */

	for (j = 0; j < fifo_cnt; j = j + i) {
		for (i = 0; i < priv->active_channel_cnt; i++) {
			fifo_val = priv->param->dops->read_fifo(indio_dev, NULL,
								false);

			priv->datum_buf[i << buf_offset] = fifo_val & 0xff;
			priv->datum_buf[(i << buf_offset) + 1] =
							(fifo_val >> 8) & 0xff;

			if (priv->param->dops->enable_decim_filter)
				priv->datum_buf[(i << buf_offset) + 2] =
							(fifo_val >> 16) & 0xff;
		}

		iio_push_to_buffers_with_timestamp(indio_dev, priv->datum_buf,
						   iio_get_time_ns(indio_dev));
	}

	meson_sar_adc_clear_fifo(indio_dev);

	return IRQ_HANDLED;
}

static int meson_sar_adc_calib(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int ret, nominal0, nominal1, value0, value1;

	/* use points 25% and 75% for calibration */
	nominal0 = (1 << priv->param->resolution) / 4;
	nominal1 = (1 << priv->param->resolution) * 3 / 4;

	priv->param->dops->set_ch7_mux(indio_dev, CHAN7_MUX_VDD_DIV4);
	usleep_range(10, 20);
	ret = meson_sar_adc_get_sample(indio_dev,
				       &indio_dev->channels[7],
				       MEDIAN_AVERAGING_FILTER,
				       EIGHT_SAMPLES, &value0);
	if (ret < 0)
		goto out;

	priv->param->dops->set_ch7_mux(indio_dev, CHAN7_MUX_VDD_MUL3_DIV4);
	usleep_range(10, 20);
	ret = meson_sar_adc_get_sample(indio_dev,
				       &indio_dev->channels[7],
				       MEDIAN_AVERAGING_FILTER,
				       EIGHT_SAMPLES, &value1);
	if (ret < 0)
		goto out;

	if (value1 <= value0) {
		ret = -EINVAL;
		goto out;
	}

	priv->calibscale = div_s64((nominal1 - nominal0) * (s64)MILLION,
				   value1 - value0);
	priv->calibbias = nominal0 - div_s64((s64)value0 * priv->calibscale,
					     MILLION);
	ret = 0;
out:
	priv->param->dops->set_ch7_mux(indio_dev, CHAN7_MUX_CH7_INPUT);

	return ret;
}

static int meson_sar_adc_sample_mode_set(struct iio_dev *indio_dev,
					 enum meson_sar_adc_sampling_mode mode)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	if (mode != SINGLE_MODE && mode != PERIOD_MODE)
		return -EINVAL;

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			   MESON_SAR_ADC_REG0_SAMPLING_STOP,
			   (mode == SINGLE_MODE) ?
			    MESON_SAR_ADC_REG0_SAMPLING_STOP : 0);

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			   MESON_SAR_ADC_REG0_CONTINUOUS_EN,
			   (mode == PERIOD_MODE) ?
			    MESON_SAR_ADC_REG0_CONTINUOUS_EN : 0);

	return 0;
}

static void meson_sar_adc_chan_spec_update(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	struct iio_chan_spec *chan;
	int i;

	for (i = 0; i < indio_dev->num_channels; i++) {
		chan = (struct iio_chan_spec *)indio_dev->channels + i;
		if (chan->channel < 0)
			continue;
		chan->scan_type.realbits = priv->param->resolution;
	}
}

static int meson_sar_adc_update_scan_mode(struct iio_dev *indio_dev,
					  const unsigned long *scan_mask)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	kfree(priv->datum_buf);
	priv->datum_buf = kmalloc(indio_dev->scan_bytes, GFP_KERNEL);
	if (!priv->datum_buf)
		return -ENOMEM;

	return 0;
}

static int
meson_sar_adc_iio_buffer_setup(struct iio_dev *indio_dev,
			       irqreturn_t (*pollfunc_bh)(int irq, void *p),
			       irqreturn_t (*pollfunc_th)(int irq, void *p),
			       int irq, unsigned long flags,
			       const struct iio_buffer_setup_ops *setup_ops)
{
	struct iio_buffer *buffer;
	int ret;

	buffer = iio_kfifo_allocate();
	if (!buffer)
		return -ENOMEM;

	iio_device_attach_buffer(indio_dev, buffer);

	ret = devm_request_threaded_irq(indio_dev->dev.parent, irq,
					pollfunc_th,
					pollfunc_bh,
					flags,
					indio_dev->name,
					indio_dev);
	if (ret)
		goto error_kfifo_free;

	indio_dev->setup_ops = setup_ops;
	indio_dev->modes |= INDIO_BUFFER_SOFTWARE;

	return 0;

error_kfifo_free:
	iio_kfifo_free(indio_dev->buffer);

	return ret;
}

static int meson_sar_adc_iio_buffer_cleanup(struct iio_dev *indio_dev)
{
	iio_kfifo_free(indio_dev->buffer);

	return 0;
}

static int meson_sar_adc_buffer_postenable(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	const struct iio_chan_spec *chan;
	unsigned char idx = 0;
	unsigned char bit;

	if (meson_sar_adc_pm_runtime_supported(indio_dev)) {
		pm_runtime_dont_use_autosuspend(indio_dev->dev.parent);
		if (pm_runtime_get_sync(indio_dev->dev.parent) < 0)
			return -EINVAL;
	}

	meson_sar_adc_sample_mode_set(indio_dev, PERIOD_MODE);

	if (priv->param->dops->tuning_clock)
		priv->param->dops->tuning_clock(indio_dev, PERIOD_MODE);

	/* set sampling period time */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_DELAY_SAMPLE_DLY_SEL_MASK,
			   FIELD_PREP(MESON_SAR_ADC_DELAY_SAMPLE_DLY_SEL_MASK,
				      priv->delay_per_tick));

	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG3,
			   MESON_SAR_ADC_REG3_BLOCK_DLY_MASK,
			   FIELD_PREP(MESON_SAR_ADC_REG3_BLOCK_DLY_MASK,
				      priv->ticks_per_period));

	meson_sar_adc_clear_fifo(indio_dev);

	for_each_set_bit(bit, indio_dev->active_scan_mask,
			 indio_dev->num_channels) {
		chan = indio_dev->channels + bit;

		if (chan->channel < 0)
			continue;

		/* to enable decimation filter by default if the saradc
		 * support it.
		 */
		if (priv->param->dops->enable_decim_filter)
			priv->param->dops->enable_decim_filter(indio_dev,
							       chan, true);
		meson_sar_adc_enable_channel(indio_dev, chan, idx);

		idx++;
	}

	if (!idx)
		return -EINVAL;

	priv->active_channel_cnt = idx;

	/*
	 * generate interrupt when fifo contains N samples, and the N
	 * is required to align base on the number of active scan channel
	 */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			   MESON_SAR_ADC_REG0_FIFO_CNT_IRQ_MASK,
			   FIELD_PREP(MESON_SAR_ADC_REG0_FIFO_CNT_IRQ_MASK,
				      16 - (16 % idx)));

	/* enable irq */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			   MESON_SAR_ADC_REG0_FIFO_IRQ_EN,
			   MESON_SAR_ADC_REG0_FIFO_IRQ_EN);

	priv->param->dops->enable_chnl(indio_dev, true);

	meson_sar_adc_start_sample_engine(indio_dev);

	return 0;
}

static int meson_sar_adc_buffer_predisable(struct iio_dev *indio_dev)
{
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	meson_sar_adc_stop_sample_engine(indio_dev);

	meson_sar_adc_sample_mode_set(indio_dev, SINGLE_MODE);

	/* There are 2 reasons for showing down saradc clock in C2:
	 * 1. To save cost, there is no input buffer before saradc.
	 * The value of inside resister of input source can not be too high.
	 * And resister value which can be tolerant is inversely of
	 * frequency of saradc clock.
	 * 2. The drive capability of channel 7 internal input is too weak.
	 */
	if (priv->param->dops->tuning_clock)
		priv->param->dops->tuning_clock(indio_dev, SINGLE_MODE);

	/* disable irq */
	regmap_update_bits(priv->regmap, MESON_SAR_ADC_REG0,
			   MESON_SAR_ADC_REG0_FIFO_IRQ_EN, 0);

	priv->param->dops->enable_chnl(indio_dev, false);

	if (meson_sar_adc_pm_runtime_supported(indio_dev)) {
		pm_runtime_use_autosuspend(indio_dev->dev.parent);
		pm_runtime_put_sync(indio_dev->dev.parent);
	}

	return 0;
}

static const struct iio_buffer_setup_ops meson_buffer_setup_ops = {
	.postenable  = meson_sar_adc_buffer_postenable,
	.predisable = meson_sar_adc_buffer_predisable,
};

static ssize_t chan7_mux_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int len = 0;
	int i;

	len = sprintf(buf, "current: [%d]%s\n\n",
		      priv->chan7_mux_sel, chan7_vol[priv->chan7_mux_sel]);
	for (i = 0; i < ARRAY_SIZE(chan7_vol); i++)
		len += sprintf(buf + len, "%d: %s\n", i, chan7_vol[i]);

	return len;
}

static ssize_t chan7_mux_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int val;

	if (kstrtoint(buf, 0, &val) != 0)
		return -EINVAL;
	if (val >= ARRAY_SIZE(chan7_vol))
		return -EINVAL;

	if (meson_sar_adc_pm_runtime_supported(indio_dev)) {
		if (pm_runtime_get_sync(indio_dev->dev.parent) < 0)
			return -EINVAL;
	}

	priv->param->dops->set_ch7_mux(indio_dev, val);

	if (meson_sar_adc_pm_runtime_supported(indio_dev)) {
		pm_runtime_mark_last_busy(indio_dev->dev.parent);
		pm_runtime_put_autosuspend(indio_dev->dev.parent);
	}

	return count;
}

static IIO_DEVICE_ATTR(chan7_mux, 0644,
		       chan7_mux_show, chan7_mux_store, -1);

static struct attribute *meson_sar_adc_attrs[] = {
	&iio_dev_attr_chan7_mux.dev_attr.attr,
	NULL, /*need to terminate the list of attributes by NULL*/
};

static const struct attribute_group meson_sar_adc_attr_group = {
	.attrs = meson_sar_adc_attrs,
};

static const struct iio_info meson_sar_adc_iio_info = {
	.read_raw = meson_sar_adc_iio_info_read_raw,
	.update_scan_mode = meson_sar_adc_update_scan_mode,
	.attrs = &meson_sar_adc_attr_group,
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static const struct meson_sar_adc_data meson_sar_adc_meson8_data __initconst = {
	.param = &meson_sar_adc_meson8_param,
	.name = "meson-meson8-saradc",
};

static
const struct meson_sar_adc_data meson_sar_adc_meson8b_data __initconst = {
	.param = &meson_sar_adc_meson8b_param,
	.name = "meson-meson8b-saradc",
};

static
const struct meson_sar_adc_data meson_sar_adc_meson8m2_data __initconst = {
	.param = &meson_sar_adc_meson8b_param,
	.name = "meson-meson8m2-saradc",
};

static const struct meson_sar_adc_data meson_sar_adc_gxbb_data __initconst = {
	.param = &meson_sar_adc_gxbb_param,
	.name = "meson-gxbb-saradc",
};

static const struct meson_sar_adc_data meson_sar_adc_gxl_data __initconst = {
	.param = &meson_sar_adc_gxl_param,
	.name = "meson-gxl-saradc",
};

static const struct meson_sar_adc_data meson_sar_adc_gxm_data __initconst = {
	.param = &meson_sar_adc_gxl_param,
	.name = "meson-gxm-saradc",
};

static const struct meson_sar_adc_data meson_sar_adc_axg_data __initconst = {
	.param = &meson_sar_adc_txlx_param,
	.name = "meson-axg-saradc",
};

static const struct meson_sar_adc_data meson_sar_adc_txlx_data __initconst = {
	.param = &meson_sar_adc_txlx_param,
	.name = "meson-txlx-saradc",
};
#endif

static const struct meson_sar_adc_data meson_sar_adc_g12a_data __initconst = {
	.param = &meson_sar_adc_g12a_param,
	.name = "meson-g12a-saradc",
};

static const struct meson_sar_adc_data meson_sar_adc_c2_data __initconst = {
	.param = &meson_sar_adc_c2_param,
	.name = "meson-c2-saradc",
};

static const struct meson_sar_adc_data meson_sar_adc_a4_data __initconst = {
	.param = &meson_sar_adc_a4_param,
	.name = "meson-a4-saradc",
};

static const struct of_device_id meson_sar_adc_of_match[] __initconst = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic,meson8-saradc",
		.data = &meson_sar_adc_meson8_data,
	},
	{
		.compatible = "amlogic,meson8b-saradc",
		.data = &meson_sar_adc_meson8b_data,
	},
	{
		.compatible = "amlogic,meson8m2-saradc",
		.data = &meson_sar_adc_meson8m2_data,
	},
	{
		.compatible = "amlogic,meson-gxbb-saradc",
		.data = &meson_sar_adc_gxbb_data,
	}, {
		.compatible = "amlogic,meson-gxl-saradc",
		.data = &meson_sar_adc_gxl_data,
	}, {
		.compatible = "amlogic,meson-gxm-saradc",
		.data = &meson_sar_adc_gxm_data,
	}, {
		.compatible = "amlogic,meson-axg-saradc",
		.data = &meson_sar_adc_axg_data,
	},
	{
		.compatible = "amlogic,meson-txlx-saradc",
		.data = &meson_sar_adc_txlx_data,
	},
#endif
	{
		.compatible = "amlogic,meson-g12a-saradc",
		.data = &meson_sar_adc_g12a_data,
	},
	{
		.compatible = "amlogic,meson-c2-saradc",
		.data = &meson_sar_adc_c2_data,
	},
	{
		.compatible = "amlogic,meson-a4-saradc",
		.data = &meson_sar_adc_a4_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, meson_sar_adc_of_match);

static int __init meson_sar_adc_probe(struct platform_device *pdev)
{
	const struct meson_sar_adc_data *match_data;
	struct meson_sar_adc_priv *priv;
	struct iio_dev *indio_dev;
	struct resource *res;
	void __iomem *base;
	int irq, ret;
	struct meson_sar_adc_param *match_param;
	struct iio_chan_spec *chan;
	int i;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*priv));
	if (!indio_dev) {
		dev_err(&pdev->dev, "failed allocating iio device\n");
		return -ENOMEM;
	}

	priv = iio_priv(indio_dev);
	match_data = of_device_get_match_data(&pdev->dev);
	if (!match_data) {
		dev_err(&pdev->dev, "failed to get match data\n");
		return -ENODEV;
	}

	match_param = devm_kzalloc(&pdev->dev, sizeof(*match_param),
				   GFP_KERNEL);
	if (!match_param)
		return -ENOMEM;

	memcpy(match_param, match_data->param, sizeof(*match_param));

	priv->param = match_param;

	if (!match_param->dops->extra_init || !match_param->dops->set_ch7_mux ||
	    !match_param->dops->read_fifo || !match_param->dops->enable_chnl ||
	    !match_param->dops->read_chnl) {
		dev_err(&pdev->dev, "necessary operations not supported\n");
		return -ENOTSUPP;
	}

	indio_dev->name = match_data->name;
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->dev.of_node = pdev->dev.of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &meson_sar_adc_iio_info;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (!irq)
		return -EINVAL;

	priv->regmap = devm_regmap_init_mmio(&pdev->dev, base,
					     priv->param->regmap_config);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	priv->clkin = devm_clk_get(&pdev->dev, "clkin");
	if (IS_ERR(priv->clkin)) {
		dev_err(&pdev->dev, "failed to get clkin\n");
		return PTR_ERR(priv->clkin);
	}

	priv->core_clk = devm_clk_get(&pdev->dev, "core");
	if (IS_ERR(priv->core_clk)) {
		dev_err(&pdev->dev, "failed to get core clk\n");
		return PTR_ERR(priv->core_clk);
	}

	priv->adc_clk = devm_clk_get(&pdev->dev, "adc_clk");
	if (IS_ERR(priv->adc_clk)) {
		if (PTR_ERR(priv->adc_clk) == -ENOENT) {
			priv->adc_clk = NULL;
		} else {
			dev_err(&pdev->dev, "failed to get adc clk\n");
			return PTR_ERR(priv->adc_clk);
		}
	}

	priv->adc_sel_clk = devm_clk_get(&pdev->dev, "adc_sel");
	if (IS_ERR(priv->adc_sel_clk)) {
		if (PTR_ERR(priv->adc_sel_clk) == -ENOENT) {
			priv->adc_sel_clk = NULL;
		} else {
			dev_err(&pdev->dev, "failed to get adc_sel clk\n");
			return PTR_ERR(priv->adc_sel_clk);
		}
	}

	/* on pre-GXBB SoCs the SAR ADC itself provides the ADC clock: */
	if (!priv->adc_clk) {
		ret = meson_sar_adc_clk_init(indio_dev, base);
		if (ret)
			return ret;
	}

	priv->vref = devm_regulator_get(&pdev->dev, "vref");
	if (IS_ERR(priv->vref)) {
		dev_err(&pdev->dev, "failed to get vref regulator\n");
		return PTR_ERR(priv->vref);
	}

	priv->calibscale = MILLION;

	if (priv->param->temperature_trimming_bits) {
		ret = meson_sar_adc_temp_sensor_init(indio_dev);
		if (ret)
			return ret;
	}

	indio_dev->channels = priv->param->channels;
	indio_dev->num_channels = priv->param->num_channels;

	ret = of_property_read_bool(pdev->dev.of_node,
				    "amlogic,enable-continuous-sampling");

	if (priv->param->has_chnl_regs && ret) {
		meson_sar_adc_chan_spec_update(indio_dev);

		ret = of_property_read_u32(pdev->dev.of_node,
					   "amlogic,delay-per-tick",
					   &priv->delay_per_tick);
		if (ret) {
			dev_info(&pdev->dev,
				 "set delay per tick to <1ms> by default.");
			/* 1ms per tick */
			priv->delay_per_tick = 3;
		}

		ret = of_property_read_u32(pdev->dev.of_node,
					   "amlogic,ticks-per-period",
					   &priv->ticks_per_period);
		if (ret) {
			dev_info(&pdev->dev,
				 "set ticks per period to <1> by default.");
			/* 1 ticks per sampling period */
			priv->ticks_per_period = 1;
		}

		ret = meson_sar_adc_iio_buffer_setup(indio_dev,
						     &meson_sar_adc_worker,
						     &meson_sar_adc_irq,
						     irq,
						     IRQF_SHARED | IRQF_ONESHOT,
						     &meson_buffer_setup_ops);

		if (ret)
			return ret;
	}

	platform_set_drvdata(pdev, indio_dev);

	if (meson_sar_adc_pm_runtime_supported(indio_dev)) {
		pm_runtime_enable(&pdev->dev);
		pm_runtime_set_autosuspend_delay(&pdev->dev,
						 MESON_SAR_ADC_PM_TIMEOUT);
		pm_runtime_use_autosuspend(&pdev->dev);

		ret = pm_runtime_get_sync(&pdev->dev);
		if (ret < 0)
			goto err;
	} else {
		ret = meson_sar_adc_init(indio_dev);
		if (ret)
			goto err;

		ret = meson_sar_adc_hw_enable(indio_dev);
		if (ret)
			goto err;
	}

	if (priv->param->calib_enable) {
		for (i = 0; i < indio_dev->num_channels; i++) {
			chan = (struct iio_chan_spec *)indio_dev->channels + i;
			if (chan->channel < 0)
				continue;

			chan->info_mask_shared_by_all =
				BIT(IIO_CHAN_INFO_CALIBBIAS) |
				BIT(IIO_CHAN_INFO_CALIBSCALE);
		}

		ret = meson_sar_adc_calib(indio_dev);
		if (ret)
			dev_warn(&pdev->dev, "calibration failed\n");
	}

	ret = iio_device_register(indio_dev);
	if (ret)
		goto err_hw;

	if (meson_sar_adc_pm_runtime_supported(indio_dev)) {
		pm_runtime_mark_last_busy(&pdev->dev);
		pm_runtime_put_autosuspend(&pdev->dev);
	}

	return 0;

err_hw:
	meson_sar_adc_hw_disable(indio_dev);
err:
	if (iio_buffer_enabled(indio_dev))
		meson_sar_adc_iio_buffer_cleanup(indio_dev);

	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return ret;
}

static int meson_sar_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	if (meson_sar_adc_pm_runtime_supported(indio_dev)) {
		pm_runtime_dont_use_autosuspend(&pdev->dev);
		pm_runtime_disable(&pdev->dev);
	}

	iio_device_unregister(indio_dev);
	if (iio_buffer_enabled(indio_dev)) {
		meson_sar_adc_iio_buffer_cleanup(indio_dev);
		kfree(priv->datum_buf);
	}

	meson_sar_adc_hw_disable(indio_dev);

	return meson_sar_adc_uninit(indio_dev);
}

static int __maybe_unused meson_sar_adc_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	if (iio_buffer_enabled(indio_dev))
		meson_sar_adc_buffer_predisable(indio_dev);

	if (meson_sar_adc_pm_runtime_supported(indio_dev)) {
		pm_runtime_force_suspend(indio_dev->dev.parent);

		clk_prepare_enable(priv->core_clk);
		return 0;
	} else {
		return meson_sar_adc_hw_disable(indio_dev);
	}
}

static int __maybe_unused meson_sar_adc_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int ret;

	if (meson_sar_adc_pm_runtime_supported(indio_dev)) {
		clk_disable_unprepare(priv->core_clk);

		pm_runtime_force_resume(indio_dev->dev.parent);
	} else {
		ret = meson_sar_adc_hw_enable(indio_dev);
		if (ret)
			return ret;
	}

	if (iio_buffer_enabled(indio_dev))
		return meson_sar_adc_buffer_postenable(indio_dev);

	return 0;
}

static int __maybe_unused meson_sar_adc_runtime_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);

	meson_sar_adc_hw_disable_unlock(indio_dev);

	return meson_sar_adc_uninit(indio_dev);
}

static int __maybe_unused meson_sar_adc_runtime_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);
	int ret;

	ret = meson_sar_adc_init(indio_dev);
	if (ret)
		return ret;

	ret = meson_sar_adc_hw_enable_unlock(indio_dev);
	if (ret)
		return ret;

	priv->param->dops->set_ch7_mux(indio_dev, priv->chan7_mux_sel);

	return 0;
}

static const struct dev_pm_ops meson_sar_adc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(meson_sar_adc_suspend, meson_sar_adc_resume)
	SET_RUNTIME_PM_OPS(meson_sar_adc_runtime_suspend,
			   meson_sar_adc_runtime_resume, NULL)
};

static void meson_sar_adc_shutdown(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct meson_sar_adc_priv *priv = iio_priv(indio_dev);

	meson_sar_adc_suspend(&pdev->dev);

	clk_disable_unprepare(priv->core_clk);
}

static struct platform_driver meson_sar_adc_driver = {
	.probe		= meson_sar_adc_probe,
	.remove		= meson_sar_adc_remove,
	.shutdown	= meson_sar_adc_shutdown,
	.driver		= {
		.name	= "meson-saradc",
		.of_match_table = meson_sar_adc_of_match,
		.pm = &meson_sar_adc_pm_ops,
	},
};

module_platform_driver(meson_sar_adc_driver);

MODULE_AUTHOR("Martin Blumenstingl <martin.blumenstingl@googlemail.com>");
MODULE_DESCRIPTION("Amlogic Meson SAR ADC driver");
MODULE_LICENSE("GPL v2");
