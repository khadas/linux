// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co. Ltd.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/iopoll.h>

#include "rkx11x_linktx.h"
#include "rkx11x_drv.h"
#include "rkx11x_reg.h"

/* RKX11X RKLINK */
#define SER_RKLINK_BASE			RKX11X_SER_RKLINK_BASE
#define SER_LINK_REG(x)			(SER_RKLINK_BASE + (x))

#define RKLINK_TX_SERDES_CTRL		SER_LINK_REG(0x0000)
#define VIDEO_CH0_EN			BIT(31)
#define VIDEO_CH1_EN			BIT(30)
#define STOP_AUDIO_MASK			BIT(18)
#define STOP_AUDIO(x)			UPDATE(x, 18, 18)
#define STOP_VIDEO_CH1_MASK		BIT(17)
#define STOP_VIDEO_CH1(x)		UPDATE(x, 17, 17)
#define STOP_VIDEO_CH0_MASK		BIT(16)
#define STOP_VIDEO_CH0(x)		UPDATE(x, 16, 16)
#define TRAIN_CLK_SEL_MASK		GENMASK(14, 13)
#define TRAIN_CLK_SEL_CH0		UPDATE(0, 14, 13)
#define TRAIN_CLK_SEL_CH1		UPDATE(1, 14, 13)
#define TRAIN_CLK_SEL_I2S		UPDATE(2, 14, 13)
#define STREAM_TYPE_MASK		BIT(12)
#define SER_STREAM_CAMERA		UPDATE(0, 12, 12)
#define SER_STREAM_DISPLAY		UPDATE(1, 12, 12)
#define VIDEO_ENABLE			BIT(11)
#define LANE_EXCHANGE_EN		BIT(10)
#define CH_REPLICATION_EN		BIT(9)
#define CH1_ENABLE			BIT(8)
#define CH1_DSOURCE_ID_MASK		GENMASK(7, 6)
#define CH1_DSOURCE_ID(x)		UPDATE(x, 7, 6)
#define CH0_DSOURCE_ID_MASK		GENMASK(5, 4)
#define CH0_DSOURCE_ID(x)		UPDATE(x, 5, 4)
#define DATA_WIDTH_MASK			GENMASK(3, 2)
#define DATA_WIDTH_8BIT			UPDATE(0, 3, 2)
#define DATA_WIDTH_16BIT		UPDATE(1, 3, 2)
#define DATA_WIDTH_24BIT		UPDATE(2, 3, 2)
#define DATA_WIDTH_32BIT		UPDATE(3, 3, 2)
#define DUAL_LANE_EN			BIT(1)
#define LINK_SER_EN			BIT(0)

#define RKLINK_TX_VIDEO_CTRL		SER_LINK_REG(0x0004)
#define VIDEO_REPKT_LENGTH_MASK		GENMASK(29, 16)
#define VIDEO_REPKT_LENGTH(x)		UPDATE(x, 29, 16)
#define DUAL_LVDS_CYCLE_DIFF_MASK	GENMASK(13, 4)
#define DUAL_LVDS_CYCLE_DIFF(x)		UPDATE(x, 13, 4)
#define PIXEL_VSYNC_SEL			BIT(3)
#define PSOURCE_ID_MASK			GENMASK(2, 0)
#define CAMERA_SRC_CSI			UPDATE(0, 2, 0)
#define CAMERA_SRC_LVDS			UPDATE(1, 2, 0)
#define CAMERA_SRC_DVP			UPDATE(2, 2, 0)

#define RKLINK_TX_AUDIO_CTRL		SER_LINK_REG(0x0028)
#define RKLINK_TX_AUDIO_FDIV		SER_LINK_REG(0x002c)
#define RKLINK_TX_AUDIO_LRCK		SER_LINK_REG(0x0030)
#define RKLINK_TX_AUDIO_RECOVER		SER_LINK_REG(0x0034)
#define RKLINK_TX_AUDIO_FM_STATUS	SER_LINK_REG(0x0038)
#define RKLINK_TX_FIFO_STATUS		SER_LINK_REG(0x003c)
#define RKLINK_TX_SOURCE_IRQ_EN		SER_LINK_REG(0x0040)

#define RKLINK_TX_TRAIN_CTRL		SER_LINK_REG(0x0044)
#define LINK_TRAIN_CYCLE(x)		UPDATE(x, 31, 2)
#define LINK_TRAIN_DONE			BIT(1)
#define LINK_TRAIN_START		BIT(0)

#define RKLINK_TX_I2C_CFG		SER_LINK_REG(0x00c0)
#define RKLINK_TX_SPI_CFG		SER_LINK_REG(0x00c4)
#define RKLINK_TX_UART_CFG		SER_LINK_REG(0x00c8)
#define RKLINK_TX_GPIO_CFG		SER_LINK_REG(0x00cc)
#define RKLINK_TX_IO_DEF_INV_CFG	SER_LINK_REG(0x00d0)
#define LINK_CMD_DISABLE_SINK		BIT(21)
#define LINK_CMD_DISABLE_SOURCE		BIT(20)

#define SER_RKLINK_AUDIO_CTRL		SER_LINK_REG(0x0028)
#define SER_RKLINK_AUDIO_FDIV		SER_LINK_REG(0x002c)
#define SER_RKLINK_AUDIO_LRCK		SER_LINK_REG(0x0030)
#define SER_RKLINK_AUDIO_RECOVER	SER_LINK_REG(0x0034)
#define SER_RKLINK_AUDIO_FM_STATUS	SER_LINK_REG(0x0038)
#define SER_RKLINK_FIFO_STATUS		SER_LINK_REG(0x003c)
#define CH1_CMD_FIFO_UNDERRUN		BIT(24)
#define CH0_CMD_FIFO_UNDERRUN		BIT(23)
#define DATA1_FIFO_UNDERRUN		BIT(22)
#define DATA0_FIFO_UNDERRUN		BIT(21)
#define AUDIO_FIFO_UNDERRUN		BIT(20)
#define LVDS1_FIFO_UNDERRUN		BIT(19)
#define LVDS0_FIFO_UNDERRUN		BIT(18)
#define DSI_CH1_FIFO_UNDERRUN		BIT(17)
#define DSI_CH0_FIFO_UNDERRUN		BIT(16)
#define CH1_CMD_FIFO_OVERFLOW		BIT(8)
#define CH0_CMD_FIFO_OVERFLOW		BIT(7)
#define DATA0_FIFO_OVERFLOW		BIT(6)
#define DATA1_FIFO_OVERFLOW		BIT(5)
#define AUDIO_FIFO_OVERFLOW		BIT(4)
#define LVDS1_FIFO_OVERFLOW		BIT(3)
#define LVDS0_FIFO_OVERFLOW		BIT(2)
#define DSI_CH1_FIFO_OVERFLOW		BIT(1)
#define DSI_CH0_FIFO_OVERFLOW		BIT(0)

#define SER_RKLINK_SOURCE_IRQ_EN	SER_LINK_REG(0x0040)
#define TRAIN_DONE_IRQ_FLAG		BIT(19)
#define FIFO_UNDERRUN_IRQ_FLAG		BIT(18)
#define FIFO_OVERFLOW_IRQ_FLAG		BIT(17)
#define AUDIO_FM_IRQ_OUTPUT_FLAG	BIT(16)
#define TRAIN_DONE_IRQ_OUTPUT_EN	BIT(3)
#define FIFO_UNDERRUN_IRQ_OUTPUT_EN	BIT(2)
#define FIFO_OVERFLOW_IRQ_OUTPUT_EN	BIT(1)
#define AUDIO_FM_IRQ_OUTPUT_EN		BIT(0)

#define SER_RKLINK_TRAIN_CTRL		SER_LINK_REG(0x0044)
#define SER_RKLINK_I2C_CFG		SER_LINK_REG(0x00c0)
#define SER_RKLINK_SPI_CFG		SER_LINK_REG(0x00c4)
#define SER_RKLINK_UART_CFG		SER_LINK_REG(0x00c8)
#define SER_RKLINK_GPIO_CFG		SER_LINK_REG(0x00cc)
#define GPIO_GROUP1_EN			BIT(17)
#define GPIO_GROUP0_EN			BIT(16)

#define SER_RKLINK_IO_DEF_INV_CFG	SER_LINK_REG(0x00d0)
#define OTHER_LANE_ACTIVE(x)		HIWORD_UPDATE(x, 12, 12)
#define FORWARD_NON_ACK(x)		HIWORD_UPDATE(x, 11, 11)
#define SER_RKLINK_DISP_DSI_SPLIT	BIT(1)
#define SER_RKLINK_DISP_MIRROR		BIT(2)
#define SER_RKLINK_DISP_DUAL_CHANNEL	BIT(3)

/* RKX11X PCS */
#define SER_PCS0_BASE			RKX11X_SER_PCS0_BASE
#define SER_PCS1_BASE			RKX11X_SER_PCS1_BASE

#define SER_PCS_DEV(id)			((id) == 0 ? (SER_PCS0_BASE) : (SER_PCS1_BASE))

#define SER_PCS_REG00			0x0000
#define PCS_DUAL_LANE_MODE_EN		HIWORD_UPDATE(1, GENMASK(12, 12), 12)
#define PCS_FORWARD_NON_ACK		HIWORD_UPDATE(1, GENMASK(11, 11), 11)
#define PCS_AUTO_START_EN		HIWORD_UPDATE(1, GENMASK(3, 3), 3)
#define PCS_EN_MASK			HIWORD_MASK(2, 2)
#define PCS_ENABLE			HIWORD_UPDATE(1, GENMASK(2, 2), 2)
#define PCS_DISABLE			HIWORD_UPDATE(0, GENMASK(2, 2), 2)
#define PCS_ECU_MODE			HIWORD_UPDATE(0, GENMASK(2, 2), 1)
#define PCS_REMOTE_MODE			HIWORD_UPDATE(1, GENMASK(2, 2), 1)
#define PCS_SAFE_MODE_EN		HIWORD_UPDATE(1, GENMASK(0, 0), 0)

#define SER_PCS_REG04			0x0004
#define SER_PCS_REG08			0x0008
#define PCS_VIDEO_SUSPEND_MASK		HIWORD_MASK(15, 15)
#define PCS_VIDEO_SUSPEND_ENABLE	HIWORD_UPDATE(1, GENMASK(15, 15), 15)
#define PCS_VIDEO_SUSPEND_DISABLE	HIWORD_UPDATE(0, GENMASK(15, 15), 15)
#define BACKWARD_SUPERHIGH_EN		HIWORD_UPDATE(1, GENMASK(14, 14), 14)
#define BACKWARD_HIGHSPEED_EN		HIWORD_UPDATE(1, GENMASK(12, 12), 12)

#define SER_PCS_REG10			0x0010
#define SER_PCS_REG14			0x0014
#define SER_PCS_REG18			0x0018
#define SER_PCS_REG1C			0x001c
#define SER_PCS_REG20			0x0020
#define SER_PCS_REG24			0x0024
#define SER_PCS_REG28			0x0028
#define SER_PCS_REG30			0x0030
#define PCS_INT_STARTUP(x)		HIWORD_UPDATE(x, GENMASK(15, 0), 0)

#define SER_PCS_REG34			0x0034
#define PCS_INT_REMOTE_MODE(x)		HIWORD_UPDATE(x, GENMASK(15, 0), 0)

#define SER_PCS_REG40			0x0040

/* RKX11X PMA */
#define SER_PMA0_BASE			RKX11X_SER_PMA0_BASE
#define SER_PMA1_BASE			RKX11X_SER_PMA1_BASE

#define SER_PMA_DEV(id)			((id) == 0 ? (SER_PMA0_BASE) : (SER_PMA1_BASE))

#define SER_PMA_STATUS			0x0000
#define PMA_FORCE_INIT_STA		BIT(8)

#define SER_PMA_CTRL			0x0004
#define PMA_FORCE_INIT_MASK		HIWORD_MASK(8, 8)
#define PMA_FORCE_INIT_EN		HIWORD_UPDATE(1, BIT(8), 8)
#define PMA_FORCE_INIT_DISABLE		HIWORD_UPDATE(0, BIT(8), 8)
#define PMA_DUAL_CHANNEL		HIWORD_UPDATE(1, BIT(3), 3)
#define PMA_INIT_CNT_CLR_MASK		HIWORD_MASK(2, 2)
#define PMA_INIT_CNT_CLR		HIWORD_UPDATE(1, BIT(2), 2)

#define SER_PMA_LOAD00			0x0010
#define PMA_RATE_MODE_MASK		HIWORD_MASK(2, 0)
#define PMA_FDR_MODE			HIWORD_UPDATE(1, GENMASK(2, 0), 0)
#define PMA_HDR_MODE			HIWORD_UPDATE(2, GENMASK(2, 0), 0)
#define PMA_QDR_MODE			HIWORD_UPDATE(4, GENMASK(2, 0), 0)

#define SER_PMA_LOAD01			0x0014
#define SER_PMA_LOAD02			0x0018
#define SER_PMA_LOAD03			0x001c
#define SER_PMA_LOAD04			0x0020
#define PMA_PLL_DIV_MASK		HIWORD_MASK(14, 0)
#define PLL_I_POST_DIV(x)		HIWORD_UPDATE(x, GENMASK(14, 10), 10)
#define PLL_F_POST_DIV(x)		HIWORD_UPDATE(x, GENMASK(9, 0), 0)
#define PMA_PLL_DIV(x)			HIWORD_UPDATE(x, GENMASK(14, 0), 0)

#define SER_PMA_LOAD05			0x002c
#define PMA_PLL_REFCLK_DIV_MASK		HIWORD_MASK(3, 0)
#define PMA_PLL_REFCLK_DIV(x)		HIWORD_UPDATE(x, GENMASK(3, 0), 0)

#define SER_PMA_LOAD06			0x0028
#define SER_PMA_LOAD07			0x002c
#define SER_PMA_LOAD08			0x0030
#define PMA_FCK_VCO_MASK		HIWORD_MASK(15, 15)
#define PMA_FCK_VCO			HIWORD_UPDATE(1, BIT(15), 15)
#define PMA_FCK_VCO_DIV2		HIWORD_UPDATE(0, BIT(15), 15)

#define SER_PMA_LOAD09			0x0034
#define PMA_PLL_DIV4_MASK		HIWORD_MASK(12, 12)
#define PMA_PLL_DIV4			HIWORD_UPDATE(1, GENMASK(12, 12), 12)
#define PMA_PLL_DIV8			HIWORD_UPDATE(0, GENMASK(12, 12), 12)

#define SER_PMA_LOAD0A			0x0038
#define PMA_CLK_8X_DIV_MASK		HIWORD_MASK(7, 1)
#define PMA_CLK_8X_DIV(x)		HIWORD_UPDATE(x, GENMASK(7, 1), 1)

#define SER_PMA_IRQ_EN			0x00f0
#define FORCE_INITIAL_IRQ_EN		HIWORD_UPDATE(1, BIT(3), 3)
#define RTERM_ONCE_TIMEOUT_IRQ_EN	HIWORD_UPDATE(1, BIT(1), 1)
#define PLL_LOCK_TIMEOUT_IRQ_EN		HIWORD_UPDATE(1, BIT(0), 0)

#define SER_PMA_IRQ_STATUS		0x00f4
#define FORCE_INITIAL_PULSE_STATUS	BIT(3)
#define RTERM_ONCE_TIMEOUT_STATUS	BIT(1)
#define PLL_LOCK_TIMEOUT_STATUS		BIT(0)

/* link passthrough */
struct rkx11x_ser_pt {
	u32 id;

	u32 en_reg;
	u32 en_mask;
	u32 en_val;

	u32 dir_reg;
	u32 dir_mask;
	u32 dir_val;
};

static const struct rkx11x_ser_pt ser_pt_cfg[] = {
	{
		/* gpi_gpo_0 */
		.id = RKX11X_LINK_PT_GPI_GPO_0,

		.en_reg = SER_RKLINK_GPIO_CFG,
		.en_mask = 0x10001,
		.en_val = 0x10001,

		.dir_reg = SER_RKLINK_GPIO_CFG,
		.dir_mask = 0x10,
		.dir_val = 0x10,
	}, {
		/* gpi_gpo_1 */
		.id = RKX11X_LINK_PT_GPI_GPO_1,

		.en_reg = SER_RKLINK_GPIO_CFG,
		.en_mask = 0x10002,
		.en_val = 0x10002,

		.dir_reg = SER_RKLINK_GPIO_CFG,
		.dir_mask = 0x20,
		.dir_val = 0x20,
	}, {
		/* gpi_gpo_2 */
		.id = RKX11X_LINK_PT_GPI_GPO_2,

		.en_reg = SER_RKLINK_GPIO_CFG,
		.en_mask = 0x10004,
		.en_val = 0x10004,

		.dir_reg = SER_RKLINK_GPIO_CFG,
		.dir_mask = 0x40,
		.dir_val = 0x40,
	}, {
		/* gpi_gpo_3 */
		.id = RKX11X_LINK_PT_GPI_GPO_3,

		.en_reg = SER_RKLINK_GPIO_CFG,
		.en_mask = 0x10008,
		.en_val = 0x10008,

		.dir_reg = SER_RKLINK_GPIO_CFG,
		.dir_mask = 0x80,
		.dir_val = 0x80,
	}, {
		/* gpi_gpo_4 */
		.id = RKX11X_LINK_PT_GPI_GPO_4,

		.en_reg = SER_RKLINK_GPIO_CFG,
		.en_mask = 0x20100,
		.en_val = 0x20100,

		.dir_reg = SER_RKLINK_GPIO_CFG,
		.dir_mask = 0x1000,
		.dir_val = 0x1000,
	}, {
		/* gpi_gpo_5 */
		.id = RKX11X_LINK_PT_GPI_GPO_5,

		.en_reg = SER_RKLINK_GPIO_CFG,
		.en_mask = 0x20200,
		.en_val = 0x20200,

		.dir_reg = SER_RKLINK_GPIO_CFG,
		.dir_mask = 0x2000,
		.dir_val = 0x2000,
	}, {
		/* gpi_gpo_6 */
		.id = RKX11X_LINK_PT_GPI_GPO_6,

		.en_reg = SER_RKLINK_GPIO_CFG,
		.en_mask = 0x20400,
		.en_val = 0x20400,

		.dir_reg = SER_RKLINK_GPIO_CFG,
		.dir_mask = 0x4000,
		.dir_val = 0x4000,
	}, {
		/* irq */
		.id = RKX11X_LINK_PT_IRQ,

		.en_reg = SER_RKLINK_GPIO_CFG,
		.en_mask = 0x20800,
		.en_val = 0x20800,

		.dir_reg = SER_RKLINK_GPIO_CFG,
		.dir_mask = 0x8000,
		.dir_val = 0x8000,
	}, {
		/* uart0 */
		.id = RKX11X_LINK_PT_UART_0,

		.en_reg = SER_RKLINK_UART_CFG,
		.en_mask = 0x1,
		.en_val = 0x1,
	}, {
		/* uart1 */
		.id = RKX11X_LINK_PT_UART_1,

		.en_reg = SER_RKLINK_UART_CFG,
		.en_mask = 0x2,
		.en_val = 0x2,
	}, {
		/* spi */
		.id = RKX11X_LINK_PT_SPI,

		.en_reg = SER_RKLINK_SPI_CFG,
		.en_mask = 0x4,
		.en_val = 0x4,

		.dir_reg = SER_RKLINK_SPI_CFG,
		.dir_mask = 0x1,
		.dir_val = 0x0,
	},
};

int rkx11x_linktx_video_enable(struct rkx11x_linktx *linktx, bool enable)
{
	struct i2c_client *client = linktx->client;
	struct device *dev = &client->dev;
	u32 reg, mask, val;

	dev_info(dev, "%s: enable = %d\n", __func__, enable);

	reg = RKLINK_TX_SERDES_CTRL;
	mask = VIDEO_ENABLE;
	val = enable ? VIDEO_ENABLE : 0;
	return linktx->i2c_reg_update(client, reg, mask, val);
}

int rkx11x_linktx_video_stop(struct rkx11x_linktx *linktx, bool stop, u32 lane_id)
{
	struct i2c_client *client = linktx->client;
	struct device *dev = &client->dev;
	u32 reg, mask, val;

	dev_info(dev, "%s: lane_id = %d, stop = %d\n", __func__, lane_id, stop);

	if (lane_id == RKX11X_LINK_LANE0) {
		mask = STOP_VIDEO_CH0_MASK;
		val = stop ? STOP_VIDEO_CH0(1) : STOP_VIDEO_CH0(0);
	} else {
		mask = STOP_VIDEO_CH1_MASK;
		val = stop ? STOP_VIDEO_CH1(1) : STOP_VIDEO_CH1(0);
	}
	reg = RKLINK_TX_SERDES_CTRL;
	return linktx->i2c_reg_update(client, reg, mask, val);
}

static int rkx11x_linktx_video_init(struct rkx11x_linktx *linktx)
{
	struct i2c_client *client = linktx->client;
	struct device *dev = &client->dev;
	u8 ch0_dsource_id, ch1_dsource_id;
	u32 lane0_enable = 0, lane1_enable = 0;
	u32 video_packet_size = 0;
	u32 stream_source = 0;
	u32 reg, mask, val;
	int ret = 0;

#if 0
	ret = linktx->i2c_reg_read(client, RKLINK_TX_SERDES_CTRL, &val);
	if (ret)
		dev_err(dev, "serdes control read error!\n");
	else
		dev_info(dev, "serdes control read val = 0x%08x\n", val);
#endif
	lane0_enable = linktx->lane_cfg[RKX11X_LINK_LANE0].lane_enable;
	lane1_enable = linktx->lane_cfg[RKX11X_LINK_LANE1].lane_enable;

	/* lane dsource_id check */
	ch0_dsource_id = linktx->lane_cfg[RKX11X_LINK_LANE0].dsource_id;
	ch1_dsource_id = linktx->lane_cfg[RKX11X_LINK_LANE1].dsource_id;

	if (linktx->version == RKX11X_LINKTX_V0) {
		dev_info(dev, "Fix IC Bug: dsource id can't set 0!\n");
		ch0_dsource_id = 2;
		ch1_dsource_id = 3;
	}

	if (lane0_enable && lane1_enable) {
		if (ch0_dsource_id == ch1_dsource_id) {
			dev_err(dev, "ch0_dsource_id == ch1_dsource_id error!\n");
			return -EINVAL;
		}
	}

	mask = 0; val = 0;

	/* camera stream select */
	mask |= STREAM_TYPE_MASK;
	val |= SER_STREAM_CAMERA;

	/* data width */
	mask |= DATA_WIDTH_MASK;
	val |= DATA_WIDTH_32BIT;

	if (lane0_enable) {
		/* channel 0: normal status */
		mask |= STOP_VIDEO_CH0_MASK;
		val |= STOP_VIDEO_CH0(0);

		/* dsource id */
		mask |= CH0_DSOURCE_ID_MASK;
		val |= CH0_DSOURCE_ID(ch0_dsource_id);
	}

	if (lane1_enable) {
		/* lane1 enable */
		mask |= (CH1_ENABLE | DUAL_LANE_EN);
		val |= (CH1_ENABLE | DUAL_LANE_EN);

		/* channel 1: normal status */
		mask |= STOP_VIDEO_CH1_MASK;
		val |= STOP_VIDEO_CH1(0);

		/* dsource id */
		mask |= CH1_DSOURCE_ID_MASK;
		val |= CH1_DSOURCE_ID(ch1_dsource_id);
	}

	/* serdes ctrl: setting */
	dev_info(dev, "serdes control set: mask = 0x%x, val = 0x%x\n", mask, val);
	reg = RKLINK_TX_SERDES_CTRL;
	ret = linktx->i2c_reg_update(client, reg, mask, val);
	if (ret) {
		dev_err(dev, "serdes control setting error!\n");
		return ret;
	}

	/* video ctrl */
	if (linktx->stream_source == RKX11X_LINK_LANE_STREAM_MIPI)
		stream_source = CAMERA_SRC_CSI;
	else if (linktx->stream_source == RKX11X_LINK_LANE_STREAM_DVP)
		stream_source = CAMERA_SRC_DVP;
	else
		stream_source = 0;
	mask = (PSOURCE_ID_MASK);
	val = stream_source;

	// if video_packet_size == 0: No configuration required, use default values
	if (linktx->video_packet_size) {
		/* video packet size should be a multiple of 16
		 *     video_packet_size = (video_packet_size + 15) / 16 * 16
		 */
		video_packet_size = linktx->video_packet_size;
		video_packet_size = (video_packet_size + 15) / 16 * 16;
		mask |= VIDEO_REPKT_LENGTH_MASK;
		val |= VIDEO_REPKT_LENGTH(video_packet_size);
	}

	reg = RKLINK_TX_VIDEO_CTRL;
	ret = linktx->i2c_reg_update(client, reg, mask, val);
	if (ret) {
		dev_err(dev, "video control setting error!\n");
		return ret;
	}

	/* serdes ctrl: enable */
	/* notice: lane0 and lane1 should be config before ser_en */
	reg = RKLINK_TX_SERDES_CTRL;
	mask = LINK_SER_EN;
	val = LINK_SER_EN;
	ret = linktx->i2c_reg_update(client, reg, mask, val);
	if (ret) {
		dev_err(dev, "serdes control enable error!\n");
		return ret;
	}

#if 0
	ret = linktx->i2c_reg_read(client, RKLINK_TX_SERDES_CTRL, &val);
	if (ret)
		dev_err(dev, "serdes control read val = 0x%08x\n", val);
#endif

	return 0;
}

static int rkx11x_linktx_pcs_init(struct rkx11x_linktx *linktx)
{
	struct i2c_client *client = linktx->client;
	struct device *dev = &client->dev;
	u32 reg, mask, val;
	int ret = 0;

	if (linktx->version == RKX11X_LINKTX_V1) {
		dev_info(dev, "rkx111 pcs: disable video suspend\n");

		mask = PCS_VIDEO_SUSPEND_MASK;
		val = PCS_VIDEO_SUSPEND_DISABLE;

		/* PCS0 */
		reg = SER_PCS0_BASE + SER_PCS_REG08;
		ret |= linktx->i2c_reg_update(client, reg, mask, val);

		/* PCS1 */
		reg = SER_PCS1_BASE + SER_PCS_REG08;
		ret |= linktx->i2c_reg_update(client, reg, mask, val);

		return ret;
	}

	return 0;
}

int rkx11x_linktx_lane_init(struct rkx11x_linktx *linktx)
{
	int ret = 0;

	if ((linktx == NULL) || (linktx->client == NULL)
			|| (linktx->i2c_reg_read == NULL)
			|| (linktx->i2c_reg_write == NULL)
			|| (linktx->i2c_reg_update == NULL))
		return -EINVAL;

	ret = rkx11x_linktx_pcs_init(linktx);
	if (ret) {
		dev_err(&linktx->client->dev, "linktx pcs init error\n");
		return ret;
	}

	ret = rkx11x_linktx_video_init(linktx);
	if (ret) {
		dev_err(&linktx->client->dev, "linktx video init error\n");
		return ret;
	}

	return 0;
}

/*
 * rkx11x 3gpll control link forward rate
 *   PMA0 or PMA1 control PMA_PLL
 */
int rkx11x_ser_pma_set_rate(struct rkx11x_linktx *linktx,
			const struct rkx11x_pma_pll *ser_pma_pll)
{
	struct i2c_client *client = linktx->client;
	struct device *dev = &client->dev;
	u32 pma_base = 0;
	u32 reg, mask, val;
	int ret = 0;

	dev_info(dev, "%s\n", __func__);

	/* 3gpll control select: pma0 or pma1 */
	ret = linktx->i2c_reg_read(client, SER_GRF_SOC_CON7, &val);
	if (ret)
		return -EFAULT;
	if (val & PMA_PLL_CTRL_BY_PMA1) {
		pma_base = SER_PMA1_BASE;
		dev_info(dev, "PMA1 control PMA_PLL\n");
	} else {
		pma_base = SER_PMA0_BASE;
		dev_info(dev, "PMA0 control PMA_PLL\n");
	}

	reg = pma_base + SER_PMA_STATUS;
	ret = linktx->i2c_reg_read(client, reg, &val);
	if (ret)
		return -EFAULT;
	if (val & PMA_FORCE_INIT_STA) {
		reg = pma_base + SER_PMA_CTRL;
		mask = PMA_INIT_CNT_CLR_MASK;
		val = PMA_INIT_CNT_CLR;
		linktx->i2c_reg_update(client, reg, mask, val);
	}

	reg = pma_base + SER_PMA_CTRL;
	mask = PMA_FORCE_INIT_MASK;
	if (ser_pma_pll->force_init_en)
		val = PMA_FORCE_INIT_EN;
	else
		val = PMA_FORCE_INIT_DISABLE;
	ret |= linktx->i2c_reg_update(client, reg, mask, val);

	reg = pma_base + SER_PMA_LOAD00;
	mask = PMA_RATE_MODE_MASK;
	if (ser_pma_pll->rate_mode == RKX11X_PMA_PLL_FDR_RATE_MODE)
		val = PMA_FDR_MODE;
	else if (ser_pma_pll->rate_mode == RKX11X_PMA_PLL_HDR_RATE_MODE)
		val = PMA_HDR_MODE;
	else
		val = PMA_QDR_MODE;
	ret |= linktx->i2c_reg_update(client, reg, mask, val);

	reg = pma_base + SER_PMA_LOAD04;
	mask = PMA_PLL_DIV_MASK;
	val = PMA_PLL_DIV(ser_pma_pll->pll_div);
	ret |= linktx->i2c_reg_update(client, reg, mask, val);

	reg = pma_base + SER_PMA_LOAD05;
	mask = PMA_PLL_REFCLK_DIV_MASK;
	val = PMA_PLL_REFCLK_DIV(ser_pma_pll->pll_refclk_div);
	ret |= linktx->i2c_reg_update(client, reg, mask, val);

	reg = pma_base + SER_PMA_LOAD08;
	mask = PMA_FCK_VCO_MASK;
	if (ser_pma_pll->pll_fck_vco_div2)
		val = PMA_FCK_VCO_DIV2;
	else
		val = PMA_FCK_VCO;
	ret |= linktx->i2c_reg_update(client, reg, mask, val);

	reg = pma_base + SER_PMA_LOAD09;
	mask = PMA_PLL_DIV4_MASK;
	if (ser_pma_pll->pll_div4)
		val = PMA_PLL_DIV4;
	else
		val = PMA_PLL_DIV8;
	ret |= linktx->i2c_reg_update(client, reg, mask, val);

	reg = pma_base + SER_PMA_LOAD0A;
	mask = PMA_CLK_8X_DIV_MASK;
	val = PMA_CLK_8X_DIV(ser_pma_pll->clk_div);
	ret |= linktx->i2c_reg_update(client, reg, mask, val);

	return ret;
}

int rkx11x_ser_pma_enable(struct rkx11x_linktx *linktx, bool enable, u32 pma_id)
{
	u32 reg, mask, val;
	int ret = 0;

	if (pma_id == 0) {
		mask = PMA0_EN_MASK;
		val = enable ? PMA0_ENABLE : PMA0_DISABLE;
	} else {
		mask = PMA1_EN_MASK;
		val = enable ? PMA1_ENABLE : PMA1_DISABLE;
	}
	reg = SER_GRF_SOC_CON7;
	ret = linktx->i2c_reg_update(linktx->client, reg, mask, val);

	return ret;
}

int rkx11x_ser_pma_set_line(struct rkx11x_linktx *linktx, u32 link_line)
{
	struct i2c_client *client = linktx->client;
	u32 pma_base = 0;
	u32 val = 0;
	int ret = 0;

	/* 3gpll control select: pma0 or pma1 */
	ret = linktx->i2c_reg_read(client, SER_GRF_SOC_CON7, &val);
	if (ret)
		return -EFAULT;
	if (val & PMA_PLL_CTRL_BY_PMA1) {
		pma_base = SER_PMA1_BASE;
		dev_info(&client->dev, "PMA1 control PMA_PLL\n");
	} else {
		pma_base = SER_PMA0_BASE;
		dev_info(&client->dev, "PMA0 control PMA_PLL\n");
	}

	/*
	 * RKx111 TX_FFE (SER_PMA_LOAD01 bit[11:8]): 0x0 - 0xF, default is 0xC
	 *     short line: TX_FFE = 0x6
	 *     common line: TX_FFE = 0xC (default)
	 *     long line: TX_FFE = 0xC (default)
	 */
	if (link_line == RKX11X_LINK_LINE_SHORT_CONFIG) {
		dev_info(&client->dev, "ser: set link line short config\n");

		ret = linktx->i2c_reg_write(client, pma_base + SER_PMA_LOAD01, 0xffffe608);
	}

	return ret;
}

int rkx11x_linktx_passthrough_cfg(struct rkx11x_linktx *linktx, u32 pt_id, bool is_pt_rx)
{
	struct i2c_client *client = linktx->client;
	const struct rkx11x_ser_pt *ser_pt = NULL;
	u32 reg, mask = 0, val = 0;
	int ret = 0, i = 0;

	dev_info(&client->dev, "%s: pt_id = %d, is_pt_rx = %d\n",
			__func__, pt_id, is_pt_rx);

	ser_pt = NULL;
	for (i = 0; i < ARRAY_SIZE(ser_pt_cfg); i++) {
		if (ser_pt_cfg[i].id == pt_id) {
			ser_pt = &ser_pt_cfg[i];
			break;
		}
	}
	if (ser_pt == NULL) {
		dev_err(&client->dev, "find link ser_pt setting error\n");
		return -EINVAL;
	}

	/* config link passthrough */
	if (ser_pt->en_reg) {
		reg = ser_pt->en_reg;
		mask = ser_pt->en_mask;
		val = ser_pt->en_val;
		ret = linktx->i2c_reg_update(client, reg, mask, val);
		if (ret) {
			dev_err(&client->dev, "en_reg setting error\n");
			return ret;
		}
	}

	if (ser_pt->dir_reg) {
		reg = ser_pt->dir_reg;
		mask = ser_pt->dir_mask;
		val = is_pt_rx ? ser_pt->dir_val : ~ser_pt->dir_val;
		ret = linktx->i2c_reg_update(client, reg, mask, val);
		if (ret) {
			dev_err(&client->dev, "dir_reg setting error\n");
			return ret;
		}
	}

	return 0;
}
