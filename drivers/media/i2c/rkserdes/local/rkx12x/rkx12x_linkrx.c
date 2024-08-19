// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co. Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/iopoll.h>

#include "rkx12x_reg.h"
#include "rkx12x_linkrx.h"
#include "rkx12x_compact.h"

/* RKX12X RKLINK */
#define RKLINK_DES_REG(x)		(RKX12X_DES_RKLINK_BASE + (x))

#define RKLINK_DES_LANE_ENGINE_CFG	RKLINK_DES_REG(0x0000)
#define TRAIN_CLK_SEL_MASK		GENMASK(31, 30)
#define TRAIN_CLK_SEL_E0		UPDATE(0, 31, 30)
#define TRAIN_CLK_SEL_E1		UPDATE(1, 31, 30)
#define TRAIN_CLK_SEL_I2S		UPDATE(2, 31, 30)
#define VIDEO_FREQ_AUTO_EN		BIT(28)
#define ENGINE1_LANE_MASK		BIT(23)
#define ENGINE1_1_LANE			UPDATE(0, 23, 23)
#define ENGINE1_2_LANE			UPDATE(1, 23, 23)
#define ENGINE1_EN			BIT(22)
#define ENGINE0_LANE_MASK		BIT(21)
#define ENGINE0_1_LANE			UPDATE(0, 21, 21)
#define ENGINE0_2_LANE			UPDATE(1, 21, 21)
#define ENGINE0_EN			BIT(20)
#define LANE1_DATA_WIDTH_MASK		GENMASK(15, 14)
#define LANE1_DATA_WIDTH_8BIT		UPDATE(0, 15, 14)
#define LANE1_DATA_WIDTH_16BIT		UPDATE(1, 15, 14)
#define LANE1_DATA_WIDTH_24BIT		UPDATE(2, 15, 14)
#define LANE1_DATA_WIDTH_32BIT		UPDATE(3, 15, 14)
#define LANE0_DATA_WIDTH_MASK		GENMASK(13, 12)
#define LANE0_DATA_WIDTH_8BIT		UPDATE(0, 13, 12)
#define LANE0_DATA_WIDTH_16BIT		UPDATE(1, 13, 12)
#define LANE0_DATA_WIDTH_24BIT		UPDATE(2, 13, 12)
#define LANE0_DATA_WIDTH_32BIT		UPDATE(3, 13, 12)
#define LANE1_PKT_LOSE_NUM_CLR		BIT(9)
#define LANE0_PKT_LOSE_NUM_CLR		BIT(8)
#define LANE1_EN			BIT(5)
#define LANE0_EN			BIT(4)
#define LAST_CHECK_EN			BIT(1)
#define LINK_DES_EN			BIT(0)

#define RKLINK_DES_LANE_ENGINE_DST	RKLINK_DES_REG(0x0004)
#define LANE1_ENGINE_CFG_MASK		GENMASK(7, 4)
#define LANE1_ENGINE1			BIT(5)
#define LANE1_ENGINE0			BIT(4)
#define LANE0_ENGINE_CFG_MASK		GENMASK(3, 0)
#define LANE0_ENGINE1			BIT(1)
#define LANE0_ENGINE0			BIT(0)

#define RKLINK_DES_DATA_ID_CFG		RKLINK_DES_REG(0x0008)
#define DATA_FIFO3_RD_ID_MASK		GENMASK(30, 28)
#define DATA_FIFO3_RD_ID(x)		UPDATE(x, 30, 28)
#define DATA_FIFO2_RD_ID_MASK		GENMASK(26, 24)
#define DATA_FIFO2_RD_ID(x)		UPDATE(x, 26, 24)
#define DATA_FIFO1_RD_ID_MASK		GENMASK(22, 20)
#define DATA_FIFO1_RD_ID(x)		UPDATE(x, 22, 20)
#define DATA_FIFO0_RD_ID_MASK		GENMASK(18, 16)
#define DATA_FIFO0_RD_ID(x)		UPDATE(x, 18, 16)
#define DATA_FIFO3_WR_ID_MASK		GENMASK(14, 12)
#define DATA_FIFO3_WR_ID(x)		UPDATE(x, 14, 12)
#define DATA_FIFO2_WR_ID_MASK		GENMASK(10, 8)
#define DATA_FIFO2_WR_ID(x)		UPDATE(x, 10, 8)
#define DATA_FIFO1_WR_ID_MASK		GENMASK(6, 4)
#define DATA_FIFO1_WR_ID(x)		UPDATE(x, 6, 4)
#define DATA_FIFO0_WR_ID_MASK		GENMASK(2, 0)
#define DATA_FIFO0_WR_ID(x)		UPDATE(x, 2, 0)

#define RKLINK_DES_ORDER_ID_CFG		RKLINK_DES_REG(0x000C)
#define ORDER_FIFO1_RD_ID_MASK		GENMASK(22, 20)
#define ORDER_FIFO1_RD_ID(x)		UPDATE(x, 22, 20)
#define ORDER_FIFO0_RD_ID_MASK		GENMASK(18, 16)
#define ORDER_FIFO0_RD_ID(x)		UPDATE(x, 18, 16)
#define ORDER_FIFO1_WR_ID_MASK		GENMASK(6, 4)
#define ORDER_FIFO1_WR_ID(x)		UPDATE(x, 6, 4)
#define ORDER_FIFO0_WR_ID_MASK		GENMASK(2, 0)
#define ORDER_FIFO0_WR_ID(x)		UPDATE(x, 2, 0)

#define RKLINK_DES_PKT_LOSE_NUM_LANE0	RKLINK_DES_REG(0x0010)
#define RKLINK_DES_PKT_LOSE_NUM_LANE1	RKLINK_DES_REG(0x0014)

#define RKLINK_DES_ECC_CRC_RESULT	RKLINK_DES_REG(0x0020)
#define LANE1_LAST_NOT_MATCH		BIT(15)
#define LANE1_CRC_CHECK_RESULT		BIT(14)
#define LANE1_PKTHDR_ERR_INFO		GENMASK(13, 8)
#define LANE0_LAST_NOT_MATCH		BIT(7)
#define LANE0_CRC_CHECK_RESULT		BIT(6)
#define LANE0_PKTHDR_ERR_INFO		GENMASK(5, 0)

#define RKLINK_DES_SOURCE_ID_CFG	RKLINK_DES_REG(0x0024)
#define ENGINE1_PSOURCE_ID_MASK		GENMASK(23, 21)
#define ENGINE1_PID_CAMERA_CSI		UPDATE(0, 23, 21)
#define ENGINE1_PID_CAMERA_LVDS		UPDATE(1, 23, 21)
#define ENGINE1_PID_CAMERA_DVP		UPDATE(2, 23, 21)
#define ENGINE1_PID_DISPLAY_DSI		UPDATE(0, 23, 21)
#define ENGINE1_PID_DISPLAY_DUAL_LDVS	UPDATE(1, 23, 21)
#define ENGINE1_PID_DISPLAY_LVDS0	UPDATE(2, 23, 21)
#define ENGINE1_PID_DISPLAY_LVDS1	UPDATE(3, 23, 21)
#define ENGINE1_PID_DISPLAY_RGB		UPDATE(5, 23, 21)
#define ENGINE1_STREAM_TYPE_MASK	GENMASK(20, 20)
#define ENGINE1_STREAM_CAMERA		UPDATE(0, 20, 20)
#define ENGINE1_STREAM_DISPLAY		UPDATE(1, 20, 20)
#define ENGINE0_PSOURCE_ID_MASK		GENMASK(19, 17)
#define ENGINE0_PID_CAMERA_CSI		UPDATE(0, 19, 17)
#define ENGINE0_PID_CAMERA_LVDS		UPDATE(1, 19, 17)
#define ENGINE0_PID_CAMERA_DVP		UPDATE(2, 19, 17)
#define ENGINE0_PID_DISPLAY_DSI		UPDATE(0, 19, 17)
#define ENGINE0_PID_DISPLAY_DUAL_LDVS	UPDATE(1, 19, 17)
#define ENGINE0_PID_DISPLAY_LVDS0	UPDATE(2, 19, 17)
#define ENGINE0_PID_DISPLAY_LVDS1	UPDATE(3, 19, 17)
#define ENGINE0_PID_DISPLAY_RGB		UPDATE(5, 19, 17)
#define ENGINE0_STREAM_TYPE_MASK	GENMASK(16, 16)
#define ENGINE0_STREAM_CAMERA		UPDATE(0, 16, 16)
#define ENGINE0_STREAM_DISPLAY		UPDATE(1, 16, 16)
#define LANE1_DSOURCE_ID_MASK		GENMASK(7, 4)
#define LANE1_DID_ENGINE(x)		UPDATE(x, 7, 6)
#define LANE1_DID_VLANE(x)		UPDATE(x, 5, 5)
#define LANE1_DID_SEL(x)		UPDATE(x, 4, 4)
#define LANE0_DSOURCE_ID_MASK		GENMASK(3, 0)
#define LANE0_DID_ENGINE(x)		UPDATE(x, 3, 2)
#define LANE0_DID_VLANE(x)		UPDATE(x, 1, 1)
#define LANE0_DID_SEL(x)		UPDATE(x, 0, 0)

#define DES_RKLINK_REC01_PKT_LENGTH	RKLINK_DES_REG(0x0028)
#define ENGINE1_REPKT_LENGTH_MASK	GENMASK(29, 16)
#define ENGINE1_REPKT_LENGTH(x)		UPDATE(x, 29, 16)
#define ENGINE0_REPKT_LENGTH_MASK	GENMASK(13, 0)
#define ENGINE0_REPKT_LENGTH(x)		UPDATE(x, 13, 0)

#define RKLINK_DES_REG01_ENGINE_DELAY	RKLINK_DES_REG(0x0030)
#define ENGINE1_ENGINE_DELAY_MASK	GENMASK(31, 16)
#define ENGINE1_ENGINE_DELAY(x)		UPDATE(x, 31, 16)
#define ENGINE0_ENGINE_DELAY_MASK	GENMASK(15, 0)
#define ENGINE0_ENGINE_DELAY(x)		UPDATE(x, 15, 0)

#define RKLINK_DES_REC_PATCH_CFG	RKLINK_DES_REG(0X0050)
#define ENGINE1_MON_VC_MASK		GENMASK(27, 26)
#define ENGINE1_MON_VC(x)		UPDATE(x, 27, 26)
#define ENGINE0_MON_VC_MASK		GENMASK(25, 24)
#define ENGINE0_MON_VC(x)		UPDATE(x, 25, 24)
#define ENGINE1_FIRST_FRAME_DEL		BIT(5)
#define ENGINE0_FIRST_FRAME_DEL		BIT(4)
#define ENGINE1_VH_SYNC_POLARITY(x)	UPDATE(x, 1, 1)
#define ENGINE0_VH_SYNC_POLARITY(x)	UPDATE(x, 0, 0)

#define RKLINK_DES_FIFO_STATUS		RKLINK_DES_REG(0x0084)
#define AUDIO_DATA_FIFO_UNDERRUN	BIT(29)
#define AUDIO_ORDER_FIFO_UNDERRUN	BIT(28)
#define VIDEO_DATA_FIFO_UNDERRUN	GENMASK(27, 24)
#define VIDEO_ORDER_FIFO_UNDERRUN	GENMASK(23, 20)
#define CMD_FIFO_UNDERRUN		GENMASK(19, 16)
#define ENGINE1_DATA_ORDER_MIS		BIT(15)
#define ENGINE0_DATA_ORDER_MIS		BIT(14)
#define AUDIO_DATA_FIFO_OVERFLOW	BIT(13)
#define AUDIO_ORDER_FIFO_OVERFLOW	BIT(12)
#define VIDEO_DATA_FIFO_OVERFLOW	GENMASK(11, 8)
#define VIDEO_ORDER_FIFO_OVERFLOW	GENMASK(7, 4)
#define CMD_FIFO_OVERFLOW		GENMASK(3, 0)

#define RKLINK_DES_SINK_IRQ_EN		RKLINK_DES_REG(0x0088)
#define COMP_NOT_ENOUGH_IRQ_FLAG	BIT(26)
#define VIDEO_FM_IRQ_FLAG		BIT(25)
#define AUDIO_FM_IRQ_FLAG		BIT(24)
#define ORDER_MIS_IRQ_FLAG		BIT(23)
#define FIFO_UNDERRUN_IRQ_FLAG		BIT(22)
#define FIFO_OVERFLOW_IRQ_FLAG		BIT(21)
#define PKT_LOSE_IRQ_FLAG		BIT(20)
#define LAST_ERROR_IRQ_FLAG		BIT(19)
#define ECC2BIT_ERROR_IRQ_FLAG		BIT(18)
#define ECC1BIT_ERROR_IRQ_FLAG		BIT(17)
#define CRC_ERROR_IRQ_FLAG		BIT(16)
#define COMP_NOT_ENOUGH_IRQ_OUTPUT_EN	BIT(10)
#define VIDEO_FM_IRQ_OUTPUT_EN		BIT(9)
#define AUDIO_FM_IRQ_OUTPUT_EN		BIT(8)
#define ORDER_MIS_IRQ_OUTPUT_EN		BIT(7)
#define FIFO_UNDERRUN_IRQ_OUTPUT_EN	BIT(6)
#define FIFO_OVERFLOW_IRQ_OUTPUT_EN	BIT(5)
#define PKT_LOSE_IRQ_OUTPUT_EN		BIT(4)
#define LAST_ERROR_IRQ_OUTPUT_EN	BIT(3)
#define ECC2BIT_ERROR_IRQ_OUTPUT_EN	BIT(2)
#define ECC1BIT_ERROR_IRQ_OUTPUT_EN	BIT(1)
#define CRC_ERROR_IRQ_OUTPUT_EN		BIT(0)

#define RKLINK_DES_STOP_CFG		RKLINK_DES_REG(0x009C)
#define STOP_AUDIO_MASK			BIT(4)
#define STOP_AUDIO(x)			UPDATE(x, 4, 4)
#define STOP_ENGINE1_MASK		BIT(1)
#define STOP_ENGINE1(x)			UPDATE(x, 1, 1)
#define STOP_ENGINE0_MASK		BIT(0)
#define STOP_ENGINE0(x)			UPDATE(x, 0, 0)

#define RKLINK_DES_SPI_CFG		RKLINK_DES_REG(0x00C4)
#define RKLINK_DES_UART_CFG		RKLINK_DES_REG(0x00C8)
#define RKLINK_DES_GPIO_CFG		RKLINK_DES_REG(0x00CC)
#define GPIO_GROUP1_EN			BIT(17)
#define GPIO_GROUP0_EN			BIT(16)

#define DES_RKLINK_IO_DEF_INV_CFG	RKLINK_DES_REG(0x00D0)
#define LINK_CMD_DISABLE_SINK		BIT(21)
#define LINK_CMD_DISABLE_SOURCE		BIT(20)

/* RKX12X PCS */
#define DES_PCS0_BASE			RKX12X_DES_PCS0_BASE
#define DES_PCS1_BASE			RKX12X_DES_PCS1_BASE

#define DES_PCS_DEV(id)			((id) == 0 ? (DES_PCS0_BASE) : (DES_PCS1_BASE))

#define DES_PCS_REG00			0x00
#define PCS_FORWARD_NON_ACK		HIWORD_UPDATE(1, GENMASK(11, 11), 11)
#define PCS_DUAL_LANE_MODE_EN		HIWORD_UPDATE(1, GENMASK(8, 8), 8)
#define PCS_AUTO_START_EN		HIWORD_UPDATE(1, GENMASK(4, 4), 4)
#define PCS_ECU_MODE			HIWORD_UPDATE(0, GENMASK(1, 1), 1)
#define PCS_EN_MASK			HIWORD_MASK(0, 0)
#define PCS_ENABLE			HIWORD_UPDATE(1, GENMASK(0, 0), 0)
#define PCS_DISABLE			HIWORD_UPDATE(0, GENMASK(0, 0), 0)

#define DES_PCS_REG04			0x04
#define BACKWARD_SUPERHIGH_EN		HIWORD_UPDATE(1, GENMASK(15, 15), 15)
#define BACKWARD_HIGHSPEED_EN		HIWORD_UPDATE(1, GENMASK(14, 14), 14)

#define DES_PCS_REG08			0x08
#define PMA_RX(x)			HIWORD_UPDATE(x, GENMASK(15, 0), 0)

#define DES_PCS_REG10			0x10
#define DES_PCS_REG14			0x14
#define DES_PCS_REG18			0x18
#define DES_PCS_REG1C			0x1C
#define DES_PCS_REG20			0x20
#define DES_PCS_REG24			0x24
#define DES_PCS_REG28			0x28
#define DES_PCS_REG30			0x30
#define PCS_INI_EN(x)			HIWORD_UPDATE(x, GENMASK(15, 0), 0)

#define DES_PCS_REG34			0x34
#define DES_PCS_REG40			0x40

/* RKX12X PMA */
#define DES_PMA0_BASE			RKX12X_DES_PMA0_BASE
#define DES_PMA1_BASE			RKX12X_DES_PMA1_BASE

#define DES_PMA_DEV(id)			((id) == 0 ? (DES_PMA0_BASE) : (DES_PMA1_BASE))

#define DES_PMA_STATUS			0x00
#define PMA_FORCE_INIT_STA		BIT(23)
#define PMA_RX_LOST			BIT(2)
#define PMA_RX_PLL_LOCK			BIT(1)
#define PMA_RX_RDY			BIT(0)

#define DES_PMA_CTRL			0x04
#define PMA_FORCE_INIT_MASK		HIWORD_MASK(8, 8)
#define PMA_FORCE_INIT_EN		HIWORD_UPDATE(1, BIT(8), 8)
#define PMA_FORCE_INIT_DISABLE		HIWORD_UPDATE(0, BIT(8), 8)
#define PMA_DUAL_CHANNEL		HIWORD_UPDATE(1, BIT(3), 3)
#define PMA_INIT_CNT_CLR_MASK		HIWORD_MASK(2, 2)
#define PMA_INIT_CNT_CLR		HIWORD_UPDATE(1, BIT(2), 2)

#define DES_PMA_LOAD00			0x10
#define PMA_RX_POL			BIT(0)
#define PMA_RX_WIDTH			BIT(1)
#define PMA_RX_MSBF_EN			BIT(2)
#define PMA_PLL_PWRDN			BIT(3)

#define DES_PMA_LOAD01			0x14
#define PMA_PLL_FORCE_LK_MASK		HIWORD_MASK(13, 13)
#define PMA_PLL_FORCE_LK		HIWORD_UPDATE(1, GENMASK(13, 13), 13)
#define PMA_LOS_VTH_MASK		HIWORD_MASK(12, 11)
#define PMA_LOS_VTH(x)			HIWORD_UPDATE(x, GENMASK(12, 11), 11)
#define PMA_PD_CP_PD_MASK		HIWORD_MASK(10, 10)
#define PMA_PD_CP_PD			HIWORD_UPDATE(1, GENMASK(10, 10), 10)
#define PMA_PD_CP_FD_MASK		HIWORD_MASK(9, 9)
#define PMA_PD_CP_FP			HIWORD_UPDATE(1, GENMASK(9, 9), 9)
#define PMA_PD_LOOP_DIV_MASK		HIWORD_MASK(8, 8)
#define PMA_PD_LOOP_DIV			HIWORD_UPDATE(1, GENMASK(8, 8), 8)
#define PMA_PD_PFD_MASK			HIWORD_MASK(7, 7)
#define PMA_PD_PFD			HIWORD_UPDATE(1, GENMASK(7, 7), 7)
#define PMA_PD_VBIAS_MASK		HIWORD_MASK(6, 6)
#define PMA_PD_VBIAS			HIWORD_UPDATE(1, GENMASK(6, 6), 6)
#define PMA_AFE_VOS_EN_MASK		HIWORD_MASK(5, 5)
#define PMA_AFE_VOS_EN			HIWORD_UPDATE(1, GENMASK(5, 5), 5)
#define PMA_PD_AFE_MASK			HIWORD_MASK(4, 4)
#define PMA_PD_AFE			HIWORD_UPDATE(1, GENMASK(4, 4), 4)
#define PMA_RX_RTERM_MASK		HIWORD_MASK(3, 0)
#define PMA_RX_RTERM(x)			HIWORD_UPDATE(x, GENMASK(3, 0), 0)

#define DES_PMA_LOAD02			0x18
#define PMA_CDR_MODE_MASK		HIWORD_MASK(13, 11)
#define PMA_CDR_MODE(x)			HIWORD_UPDATE(x, GENMASK(13, 11), 11)
#define PMA_CDR_ISEL_CP_PFD_MASK	HIWORD_MASK(10, 8)
#define PMA_CDR_ISEL_CP_PFD(x)		HIWORD_UPDATE(x, GENMASK(10, 8), 8)
#define PMA_CDR_ISEL_CP_FD_MASK		HIWORD_MASK(7, 5)
#define PMA_CDR_ISEL_CP_FD(x)		HIWORD_MASK(x, GENMASK(7, 5), 5)
#define PMA_CDR_VBIAS_MASK		HIWORD_MASK(4, 3)
#define PMA_CDR_VBIAS(x)		HIWORD_UPDATE(x, GENMASK(4, 3), 3)
#define PMA_PLL_RSEL_MASK		HIWORD_MASK(2, 0)
#define PMA_PLL_RSEL(x)			HIWORD_UPDATE(x, GENMASK(2, 0), 0)

#define DES_PMA_LOAD03			0x1C
#define DES_PMA_LOAD04			0x20

#define DES_PMA_LOAD05			0x24
#define PMA_PLL_REFCLK_DIV_MASK		HIWORD_MASK(15, 12)
#define PMA_PLL_REFCLK_DIV(x)		HIWORD_UPDATE(x, GENMASK(15, 12), 12)

#define DES_PMA_LOAD06			0x28
#define PMA_MDATA_AMP_SEL_MASK		HIWORD_MASK(15, 14)
#define PMA_MDATA_AMP_SEL(x)		HIWORD_UPDATE(x, GENMASK(15, 14), 14)
#define PMA_RX_TSEQ_MASK		HIWORD_MASK(13, 13)
#define PMA_RX_TSEQ(x)			HIWORD_UPDATE(x, BIT(13), 13)
#define PMA_FREZ_ADPT_EQ_MASK		HIWORD_MASK(12, 12)
#define PMA_FREZ_ADPT_EQ(x)		HIWORD_UPDATE(x, BIT(12), 12)
#define PMA_RX_ADPT_EQ_TRIM_MASK	HIWORD_MASK(11, 0)
#define PMA_RX_ADPT_EQ_TRIM(x)		HIWORD_UPDATE(x, GENMASK(11, 0), 0)

#define DES_PMA_LOAD07			0x2C
#define DES_PMA_LOAD08			0x30
#define PMA_RX_REGISTER_MASK		HIWORD_MASK(15, 0)
#define PMA_RX_REGISTER(x)		HIWORD_UPDATE(x, GENMASK(15, 0), 0)

#define DES_PMA_LOAD09			0x34
#define PMA_PLL_DIV_MASK		HIWORD_MASK(14, 0)
#define PLL_I_POST_DIV(x)		HIWORD_UPDATE(x, GENMASK(14, 10), 10)
#define PLL_F_POST_DIV(x)		HIWORD_UPDATE(x, GENMASK(9, 0), 0)
#define PMA_PLL_DIV(x)			HIWORD_UPDATE(x, GENMASK(14, 0), 0)

#define DES_PMA_LOAD0A			0x38
#define PMA_CLK_2X_DIV_MASK		HIWORD_MASK(7, 0)
#define PMA_CLK_2X_DIV(x)		HIWORD_UPDATE(x, GENMASK(7, 0), 0)

#define DES_PMA_LOAD0B			0x3C
#define DES_PMA_LOAD0C			0x40
#define PMA_FCK_VCO_MASK		HIWORD_MASK(15, 15)
#define PMA_FCK_VCO			HIWORD_UPDATE(1, BIT(15), 15)
#define PMA_FCK_VCO_DIV2		HIWORD_UPDATE(0, BIT(15), 15)

#define DES_PMA_LOAD0D			0x44
#define PMA_PLL_DIV4_MASK		HIWORD_MASK(12, 12)
#define PMA_PLL_DIV4			HIWORD_UPDATE(1, GENMASK(12, 12), 12)
#define PMA_PLL_DIV8			HIWORD_UPDATE(0, GENMASK(12, 12), 12)

#define DES_PMA_LOAD0E			0x48
#define DES_PMA_REG58			0x58
#define DES_PMA_REG100			0x100

#define DES_PMA_IRQ_EN			0xF0
#define FORCE_INITIAL_IRQ_EN		HIWORD_UPDATE(1, BIT(6), 6)
#define RX_RDY_NEG_IRQ_EN		HIWORD_UPDATE(1, BIT(5), 5)
#define RX_LOS_IRQ_EN			HIWORD_UPDATE(1, BIT(4), 4)
#define RX_RDY_TIMEOUT_IRQ_EN		HIWORD_UPDATE(1, BIT(2), 2)
#define PLL_LOCK_TIMEOUT_IRQ_EN		HIWORD_UPDATE(1, BIT(0), 0)

#define DES_PMA_IRQ_STATUS		0xF4
#define FORCE_INITIAL_IRQ_STATUS	BIT(6)
#define RX_RDY_NEG_IRQ_STATUS		BIT(5)
#define RX_LOS_IRQ_STATUS		BIT(4)
#define RX_RDY_TIMEOUT_IRQ_STATUS	BIT(2)
#define PLL_LOCK_TIMEOUT_IRQ_STATUS	BIT(0)

/* link passthrough */
struct rkx12x_des_pt {
	u32 id;

	u32 en_reg;
	u32 en_mask;
	u32 en_val;

	u32 dir_reg;
	u32 dir_mask;
	u32 dir_val;
};

static const struct rkx12x_des_pt des_pt_cfg[] = {
	{
		/* gpi_gpo_0 */
		.id = RKX12X_LINK_PT_GPI_GPO_0,

		.en_reg = RKLINK_DES_GPIO_CFG,
		.en_mask = 0x10001,
		.en_val = 0x10001,

		.dir_reg = RKLINK_DES_GPIO_CFG,
		.dir_mask = 0x10,
		.dir_val = 0x10,
	}, {
		/* gpi_gpo_1 */
		.id = RKX12X_LINK_PT_GPI_GPO_1,

		.en_reg = RKLINK_DES_GPIO_CFG,
		.en_mask = 0x10002,
		.en_val = 0x10002,

		.dir_reg = RKLINK_DES_GPIO_CFG,
		.dir_mask = 0x20,
		.dir_val = 0x20,
	}, {
		/* gpi_gpo_2 */
		.id = RKX12X_LINK_PT_GPI_GPO_2,

		.en_reg = RKLINK_DES_GPIO_CFG,
		.en_mask = 0x10004,
		.en_val = 0x10004,

		.dir_reg = RKLINK_DES_GPIO_CFG,
		.dir_mask = 0x40,
		.dir_val = 0x40,
	}, {
		/* gpi_gpo_3 */
		.id = RKX12X_LINK_PT_GPI_GPO_3,

		.en_reg = RKLINK_DES_GPIO_CFG,
		.en_mask = 0x10008,
		.en_val = 0x10008,

		.dir_reg = RKLINK_DES_GPIO_CFG,
		.dir_mask = 0x80,
		.dir_val = 0x80,
	}, {
		/* gpi_gpo_4 */
		.id = RKX12X_LINK_PT_GPI_GPO_4,

		.en_reg = RKLINK_DES_GPIO_CFG,
		.en_mask = 0x20100,
		.en_val = 0x20100,

		.dir_reg = RKLINK_DES_GPIO_CFG,
		.dir_mask = 0x1000,
		.dir_val = 0x1000,
	}, {
		/* gpi_gpo_5 */
		.id = RKX12X_LINK_PT_GPI_GPO_5,

		.en_reg = RKLINK_DES_GPIO_CFG,
		.en_mask = 0x20200,
		.en_val = 0x20200,

		.dir_reg = RKLINK_DES_GPIO_CFG,
		.dir_mask = 0x2000,
		.dir_val = 0x2000,
	}, {
		/* gpi_gpo_6 */
		.id = RKX12X_LINK_PT_GPI_GPO_6,

		.en_reg = RKLINK_DES_GPIO_CFG,
		.en_mask = 0x20400,
		.en_val = 0x20400,

		.dir_reg = RKLINK_DES_GPIO_CFG,
		.dir_mask = 0x4000,
		.dir_val = 0x4000,
	}, {
		/* irq */
		.id = RKX12X_LINK_PT_IRQ,

		.en_reg = RKLINK_DES_GPIO_CFG,
		.en_mask = 0x20800,
		.en_val = 0x20800,

		.dir_reg = RKLINK_DES_GPIO_CFG,
		.dir_mask = 0x8000,
		.dir_val = 0x8000,
	}, {
		/* uart0 */
		.id = RKX12X_LINK_PT_UART_0,

		.en_reg = RKLINK_DES_UART_CFG,
		.en_mask = 0x1,
		.en_val = 0x1,
	}, {
		/* uart1 */
		.id = RKX12X_LINK_PT_UART_1,

		.en_reg = RKLINK_DES_UART_CFG,
		.en_mask = 0x2,
		.en_val = 0x2,
	}, {
		/* spi */
		.id = RKX12X_LINK_PT_SPI,

		.en_reg = RKLINK_DES_SPI_CFG,
		.en_mask = 0x4,
		.en_val = 0x4,

		.dir_reg = RKLINK_DES_SPI_CFG,
		.dir_mask = 0x1,
		.dir_val = 0,
	},
};

/* note: link ready only need local chip */
static bool rkx12x_linkrx_wait_ready(struct rkx12x_linkrx *linkrx, u32 lane_id)
{
	struct i2c_client *client = linkrx->client;
	struct device *dev = &client->dev;
	u32 val, pcs_ready;
	int ret = 0;

	dev_info(dev, "rkx12x link wait lane%d ready\n", lane_id);

	pcs_ready = lane_id ? DES_PCS1_READY : DES_PCS0_READY;
	ret = read_poll_timeout(linkrx->i2c_reg_read, ret,
				!(ret < 0) && (val & pcs_ready),
				1000, USEC_PER_SEC, false, client,
				DES_GRF_SOC_STATUS0, &val);
	if (ret < 0) {
		dev_err(dev, "linkrx wait lane%d ready timeout: 0x%08x\n", lane_id, val);
		linkrx->link_lock = 0;
		return false;
	} else {
		dev_info(dev, "linkrx wait lane%d ready success: 0x%08x\n", lane_id, val);
		linkrx->link_lock = 1;
		return true;
	}
}

int rkx12x_linkrx_cmd_select(struct rkx12x_linkrx *linkrx, u32 lane_id)
{
	struct i2c_client *client = linkrx->client;
	struct device *dev = &client->dev;
	u32 reg, val;
	int ret = 0;

	dev_info(dev, "%s: lane_id = %d\n", __func__, lane_id);

	reg = DES_GRF_SOC_CON0;
	val = lane_id ? CMD_SEL_PCS1 : CMD_SEL_PCS0;
	ret = linkrx->i2c_reg_write(client, reg, val);

	return ret;
}

int rkx12x_des_pma_enable(struct rkx12x_linkrx *linkrx, bool enable, u32 pma_id)
{
	struct i2c_client *client = linkrx->client;
	struct device *dev = &client->dev;
	u32 reg, mask, val;
	int ret = 0;

	dev_info(dev, "%s: pma_id = %d, enable = %d\n", __func__, pma_id, enable);

	if (pma_id == 0) {
		mask = PMA0_EN_MASK;
		val = enable ? PMA0_ENABLE : PMA0_DISABLE;
	} else {
		mask = PMA1_EN_MASK;
		val = enable ? PMA1_ENABLE : PMA1_DISABLE;
	}
	reg = DES_GRF_SOC_CON4;
	ret = linkrx->i2c_reg_update(client, reg, mask, val);

	return ret;
}

int rkx12x_des_pcs_enable(struct rkx12x_linkrx *linkrx, bool enable, u32 pcs_id)
{
	struct i2c_client *client = linkrx->client;
	struct device *dev = &client->dev;
	u32 reg, mask, val;
	int ret = 0;

	dev_info(dev, "%s: pcs_id = %d, enable = %d\n", __func__, pcs_id, enable);

	reg = DES_PCS_DEV(pcs_id) + DES_PCS_REG00;
	mask = PCS_EN_MASK;
	val = enable ? PCS_ENABLE : PCS_DISABLE;

	ret = linkrx->i2c_reg_update(client, reg, mask, val);

	return ret;
}

static int rkx12x_linkrx_lane_engine_cfg(struct rkx12x_linkrx *linkrx,
				u32 lane_id, u32 engine_id)
{
	struct i2c_client *client = linkrx->client;
	u32 reg = 0, mask = 0, val = 0;
	int ret = 0;

	/* lane engine cfg */
	reg = RKLINK_DES_LANE_ENGINE_CFG;
	if (lane_id == RKX12X_LINK_LANE0) {
		mask = (LANE0_EN | LANE0_DATA_WIDTH_MASK);
		val = LANE0_EN; /* lane0 enable */
		val |= LANE0_DATA_WIDTH_32BIT;

		if (engine_id == RKX12X_LINK_ENGINE0) {
			/* lane0: only enable engine0 */
			mask |= (ENGINE0_EN | ENGINE0_2_LANE);
			val |= ENGINE0_EN;
		} else {
			/* lane0: only enable engine1 */
			mask |= (ENGINE1_EN | ENGINE1_2_LANE);
			val |= ENGINE1_EN;
		}
	} else {
		mask = (LANE1_EN | LANE1_DATA_WIDTH_MASK);
		val = LANE1_EN; /* lane1 enable */
		val |= LANE1_DATA_WIDTH_32BIT;

		if (engine_id == RKX12X_LINK_ENGINE0) {
			/* lane1: only enable engine0 */
			mask |= (ENGINE0_EN | ENGINE0_2_LANE);
			val |= ENGINE0_EN;
		} else {
			/* lane1: only enable engine1 */
			mask |= (ENGINE1_EN | ENGINE1_2_LANE);
			val |= ENGINE1_EN;
		}
	}
	ret = linkrx->i2c_reg_update(client, reg, mask, val);

	return ret;
}

static int rkx12x_linkrx_lane_engine_dst_cfg(struct rkx12x_linkrx *linkrx,
				u32 lane_id, u32 engine_id)
{
	struct i2c_client *client = linkrx->client;
	u32 reg = 0, mask = 0, val = 0;
	int ret = 0;

	dev_info(&client->dev, "%s: lane_id = %d, engine_id = %d\n",
				__func__, lane_id, engine_id);

	/* lane engine dst */
	reg = RKLINK_DES_LANE_ENGINE_DST;
	if (lane_id == RKX12X_LINK_LANE0) {
		mask = LANE0_ENGINE_CFG_MASK;
		if (engine_id == RKX12X_LINK_ENGINE0)
			val = LANE0_ENGINE0;
		else
			val = LANE0_ENGINE1;
	} else {
		mask = LANE1_ENGINE_CFG_MASK;
		if (engine_id == RKX12X_LINK_ENGINE0)
			val = LANE1_ENGINE0;
		else
			val = LANE1_ENGINE1;
	}
	ret = linkrx->i2c_reg_update(client, reg, mask, val);

	return ret;
}

static int rkx12x_linkrx_data_fifo_cfg(struct rkx12x_linkrx *linkrx,
			u32 lane_id, u32 engine_id)
{
	struct i2c_client *client = linkrx->client;
	u32 data_fifo_id = 0;
	u32 reg = 0, mask = 0, val = 0;
	int ret = 0;

	dev_info(&client->dev, "%s: lane id = %d engine_id = %d\n",
			__func__, lane_id, engine_id);

	/* data fifo id */
	/* engine id: 2bit, lane id: 1bit */
	data_fifo_id = ((engine_id << 1) | (lane_id << 0));

	reg = RKLINK_DES_DATA_ID_CFG;
	mask = 0;
	val = 0;
	if (lane_id == RKX12X_LINK_LANE0) {
		/* lane0: data fifo0 and data fifo1 */
		mask |= (DATA_FIFO0_WR_ID_MASK | DATA_FIFO0_RD_ID_MASK);
		mask |= (DATA_FIFO1_WR_ID_MASK | DATA_FIFO1_RD_ID_MASK);
		val |= (DATA_FIFO0_WR_ID(data_fifo_id) | DATA_FIFO0_RD_ID(data_fifo_id));
		val |= (DATA_FIFO1_WR_ID(data_fifo_id) | DATA_FIFO1_RD_ID(data_fifo_id));
	} else {
		/* lane1: data fifo2 and data fifo3 */
		mask |= (DATA_FIFO2_WR_ID_MASK | DATA_FIFO2_RD_ID_MASK);
		mask |= (DATA_FIFO3_WR_ID_MASK | DATA_FIFO3_RD_ID_MASK);
		val |= (DATA_FIFO2_WR_ID(data_fifo_id) | DATA_FIFO2_RD_ID(data_fifo_id));
		val |= (DATA_FIFO3_WR_ID(data_fifo_id) | DATA_FIFO3_RD_ID(data_fifo_id));
	}
	ret = linkrx->i2c_reg_update(client, reg, mask, val);

	return ret;
}

static int rkx12x_linkrx_order_fifo_cfg(struct rkx12x_linkrx *linkrx,
				u32 lane_id, u32 engine_id)
{
	struct i2c_client *client = linkrx->client;
	u32 order_fifo_id = 0;
	u32 reg = 0, mask = 0, val = 0;
	int ret = 0;

	dev_info(&client->dev, "%s: lane id = %d engine_id = %d\n",
			__func__, lane_id, engine_id);

	/* order fifo id */
	/* engine id: 2bit, lane id: 1bit */
	order_fifo_id = ((engine_id << 1) | (lane_id << 0));

	reg = RKLINK_DES_ORDER_ID_CFG;
	if (lane_id == RKX12X_LINK_LANE0) {
		/* lane0: order fifo0 */
		mask = ORDER_FIFO0_WR_ID_MASK | ORDER_FIFO0_RD_ID_MASK;
		val = ORDER_FIFO0_WR_ID(order_fifo_id) | ORDER_FIFO0_RD_ID(order_fifo_id);
	} else {
		/* lane1: order fifo1 */
		mask = ORDER_FIFO1_WR_ID_MASK | ORDER_FIFO1_RD_ID_MASK;
		val = ORDER_FIFO1_WR_ID(order_fifo_id) | ORDER_FIFO1_RD_ID(order_fifo_id);
	}
	ret = linkrx->i2c_reg_update(client, reg, mask, val);

	return ret;
}

static int rkx12x_linkrx_stream_source_cfg(struct rkx12x_linkrx *linkrx,
			u32 lane_id, u32 engine_id, u32 engine_source)
{
	struct i2c_client *client = linkrx->client;
	u32 stream_type = 0, stream_source = 0;
	u32 reg = 0, mask = 0, val = 0;
	int ret = 0;

	dev_info(&client->dev, "%s: lane id = %d, engine id = %d, engine source = %d\n",
			__func__, lane_id, engine_id, engine_source);

	stream_type = (engine_id == 0) ? ENGINE0_STREAM_CAMERA : ENGINE1_STREAM_CAMERA;
	if (engine_source == RKX12X_LINK_STREAM_CAMERA_CSI)
		stream_source = engine_id ? ENGINE1_PID_CAMERA_CSI : ENGINE0_PID_CAMERA_CSI;
	else if (engine_source == RKX12X_LINK_STREAM_CAMERA_DVP)
		stream_source = engine_id ? ENGINE1_PID_CAMERA_DVP : ENGINE0_PID_CAMERA_DVP;
	else
		return -1;

	mask = 0;
	val = 0;

	if (engine_id == RKX12X_LINK_ENGINE0)
		mask |= (ENGINE0_STREAM_TYPE_MASK | ENGINE0_PSOURCE_ID_MASK);
	else
		mask |= (ENGINE1_STREAM_TYPE_MASK | ENGINE1_PSOURCE_ID_MASK);

	val |= stream_type;
	val |= stream_source;

	/* dsource id */
	/* engine id: 2bit, lane id: 1bit */
	if (lane_id == RKX12X_LINK_LANE0) {
		mask |= LANE0_DSOURCE_ID_MASK;

		val |= LANE0_DID_ENGINE(engine_id);
		val |= LANE0_DID_VLANE(lane_id);
		val |= LANE0_DID_SEL(1);
	} else {
		mask |= LANE1_DSOURCE_ID_MASK;

		val |= LANE1_DID_ENGINE(engine_id);
		val |= LANE1_DID_VLANE(lane_id);
		val |= LANE1_DID_SEL(1);
	}

	reg = RKLINK_DES_SOURCE_ID_CFG;
	ret = linkrx->i2c_reg_update(client, reg, mask, val);

	return ret;
}

static int rkx12x_linkrx_video_packet_size_cfg(struct rkx12x_linkrx *linkrx,
				u8 engine_id, u16 video_packet_size)
{
	struct i2c_client *client = linkrx->client;
	u32 reg = 0, mask = 0, val = 0;
	int ret = 0;

	if (video_packet_size == 0)
		return 0;

	dev_info(&client->dev, "%s: engine_id = %d, video_packet_size = 0x%x\n",
					__func__, engine_id, video_packet_size);

	/*
	 * video_packet_size: video packet size
	 * 1. video_packet_size == 0: No configuration required, use default values
	 * 2. video packet size should be a multiple of 16
	 *        video_packet_size = (video_packet_size + 15) / 16 * 16
	 */
	// video packet size should be a multiple of 16
	video_packet_size = (video_packet_size + 15) / 16 * 16;

	reg = DES_RKLINK_REC01_PKT_LENGTH;

	if (engine_id == RKX12X_LINK_ENGINE0) {
		mask = ENGINE0_REPKT_LENGTH_MASK;
		val = ENGINE0_REPKT_LENGTH(video_packet_size);
	} else {
		mask = ENGINE1_REPKT_LENGTH_MASK;
		val = ENGINE1_REPKT_LENGTH(video_packet_size);
	}

	ret = linkrx->i2c_reg_update(client, reg, mask, val);

	return ret;
}

static int rkx12x_linkrx_engine_delay_cfg(struct rkx12x_linkrx *linkrx,
				u8 engine_id, u16 engine_delay)
{
	struct i2c_client *client = linkrx->client;
	u32 reg = 0, mask = 0, val = 0;
	int ret = 0;

	if (engine_delay == 0)
		return 0;

	dev_info(&client->dev, "%s: engine_id = %d, engine_delay = 0x%x\n",
				__func__, engine_id, engine_delay);

	/* Adjust engine delay cycle */
	reg = RKLINK_DES_REG01_ENGINE_DELAY;
	if (engine_id == RKX12X_LINK_ENGINE0) {
		mask = ENGINE0_ENGINE_DELAY_MASK;
		val = ENGINE0_ENGINE_DELAY(engine_delay);
	} else {
		mask = ENGINE1_ENGINE_DELAY_MASK;
		val = ENGINE1_ENGINE_DELAY(engine_delay);
	}

	ret = linkrx->i2c_reg_update(client, reg, mask, val);

	return ret;
}

int rkx12x_linkrx_video_stop(struct rkx12x_linkrx *linkrx,
			u32 engine_id, bool engine_stop)
{
	struct i2c_client *client = linkrx->client;
	u32 reg = 0, mask = 0, val = 0;
	int ret = 0;

	dev_info(&client->dev, "%s: engine_id = %d, engine_stop = %d\n",
			__func__, engine_id, engine_stop);

	/* engine_stop = 0: normal status */
	reg = RKLINK_DES_STOP_CFG;
	if (engine_id == RKX12X_LINK_ENGINE0) {
		mask = STOP_ENGINE0_MASK;
		val = STOP_ENGINE0(engine_stop);
	} else {
		mask = STOP_ENGINE1_MASK;
		val = STOP_ENGINE1(engine_stop);
	}
	ret = linkrx->i2c_reg_update(client, reg, mask, val);

	return ret;
}

int rkx12x_linkrx_audio_stop(struct rkx12x_linkrx *linkrx, bool audio_stop)
{
	struct i2c_client *client = linkrx->client;
	u32 reg = 0, mask = 0, val = 0;
	int ret = 0;

	dev_info(&client->dev, "%s: audio_stop = %d\n", __func__, audio_stop);

	/* audio_stop = 0: normal status */
	reg = RKLINK_DES_STOP_CFG;
	if (audio_stop) {
		mask |= STOP_AUDIO_MASK;
		val |= STOP_AUDIO(audio_stop);
	}

	ret = linkrx->i2c_reg_update(client, reg, mask, val);

	return ret;
}

static int rkx12x_linkrx_link_enbale(struct rkx12x_linkrx *linkrx)
{
	struct i2c_client *client = linkrx->client;
	struct device *dev = &client->dev;
	u32 reg, val, mask;
	int ret = 0;

	dev_info(dev, "%s\n", __func__);

	reg = RKLINK_DES_LANE_ENGINE_CFG;
	mask = LINK_DES_EN | VIDEO_FREQ_AUTO_EN;
	val = LINK_DES_EN | VIDEO_FREQ_AUTO_EN;

	ret = linkrx->i2c_reg_update(client, reg, mask, val);

	return ret;
}

static int rkx12x_linkrx_lane_engine_init(struct rkx12x_linkrx *linkrx)
{
	struct i2c_client *client = linkrx->client;
	struct device *dev = &client->dev;
	u32 lane_id, engine_id;
	int ret = 0;

	lane_id = linkrx->lane_id;
	engine_id = linkrx->engine_id;

	dev_info(dev, "%s: lane id = %d, engine id = %d\n",
			__func__, lane_id, engine_id);

	/* engine: normal status */
	ret = rkx12x_linkrx_video_stop(linkrx, engine_id, false);
	if (ret)
		return ret;

	/* lane engine cfg */
	ret = rkx12x_linkrx_lane_engine_cfg(linkrx, lane_id, engine_id);
	if (ret)
		return ret;

	/* lane engine dst */
	ret = rkx12x_linkrx_lane_engine_dst_cfg(linkrx, lane_id, engine_id);
	if (ret)
		return ret;

	/* lane data and ordef fifo cfg */
	ret = rkx12x_linkrx_data_fifo_cfg(linkrx, lane_id, engine_id);
	if (ret)
		return ret;

	ret = rkx12x_linkrx_order_fifo_cfg(linkrx, lane_id, engine_id);
	if (ret)
		return ret;

	/* lane stream source cfg */
	ret = rkx12x_linkrx_stream_source_cfg(linkrx,
				lane_id, engine_id, linkrx->engine_source);
	if (ret)
		return ret;

	/* engine delay */
	ret = rkx12x_linkrx_engine_delay_cfg(linkrx,
				engine_id, linkrx->engine_delay);
	if (ret)
		return ret;

	/* video packet size */
	ret = rkx12x_linkrx_video_packet_size_cfg(linkrx,
				engine_id, linkrx->video_packet_size);
	if (ret)
		return ret;

	/* link enable */
	ret = rkx12x_linkrx_link_enbale(linkrx);
	if (ret)
		return ret;

	return 0;
}

int rkx12x_linkrx_lane_init(struct rkx12x_linkrx *linkrx)
{
	struct device *dev = NULL;
	int ret = 0;

	if ((linkrx == NULL) || (linkrx->client == NULL)
			|| (linkrx->i2c_reg_read == NULL)
			|| (linkrx->i2c_reg_write == NULL)
			|| (linkrx->i2c_reg_update == NULL))
		return -EINVAL;

	dev = &linkrx->client->dev;

	dev_info(dev, "=== rkx12x link lane init ===\n");

	ret = rkx12x_linkrx_lane_engine_init(linkrx);
	if (ret) {
		dev_info(dev, "=== link lane engine init error ===\n");
		return ret;
	}

	return 0;
}

int rkx12x_linkrx_wait_lane_lock(struct rkx12x_linkrx *linkrx)
{
	struct i2c_client *client = linkrx->client;
	struct device *dev = &client->dev;
	int ret = 0;

	dev_info(dev, "%s: lane id = %d\n", __func__, linkrx->lane_id);

	ret = rkx12x_des_pma_enable(linkrx, true, linkrx->lane_id);
	if (ret)
		return -1;

	if (rkx12x_linkrx_wait_ready(linkrx, linkrx->lane_id) == false)
		return -1;

	return 0;
}

/*
 * rkx12x 3gpll control link inverse rate
 *   PMA0 control PMA_PLL
 */
int rkx12x_des_pma_set_rate(struct rkx12x_linkrx *linkrx,
			const struct rkx12x_pma_pll *pma_pll)
{
	struct i2c_client *client = linkrx->client;
	u32 pma_base = DES_PMA0_BASE; /* Fixed pma0 */
	u32 reg, mask, val;
	int ret = 0;

	dev_info(&client->dev, "%s\n", __func__);

	reg = pma_base + DES_PMA_STATUS;
	ret = linkrx->i2c_reg_read(client, reg, &val);
	if (ret)
		return -EFAULT;
	if (val & PMA_FORCE_INIT_STA) {
		reg = pma_base + DES_PMA_CTRL;
		mask = PMA_INIT_CNT_CLR_MASK;
		val = PMA_INIT_CNT_CLR;
		ret |= linkrx->i2c_reg_update(client, reg, mask, val);
	}

	reg = pma_base + DES_PMA_CTRL;
	mask = PMA_FORCE_INIT_MASK;
	if (pma_pll->force_init_en)
		val = PMA_FORCE_INIT_EN;
	else
		val = PMA_FORCE_INIT_DISABLE;
	ret |= linkrx->i2c_reg_update(client, reg, mask, val);

	reg = pma_base + DES_PMA_LOAD09;
	mask = PMA_PLL_DIV_MASK;
	val = PMA_PLL_DIV(pma_pll->pll_div);
	ret |= linkrx->i2c_reg_update(client, reg, mask, val);

	reg = pma_base + DES_PMA_LOAD05;
	mask = PMA_PLL_REFCLK_DIV_MASK;
	val = PMA_PLL_REFCLK_DIV(pma_pll->pll_refclk_div);
	ret |= linkrx->i2c_reg_update(client, reg, mask, val);

	reg = pma_base + DES_PMA_LOAD0C;
	mask = PMA_FCK_VCO_MASK;
	if (pma_pll->pll_fck_vco_div2)
		val = PMA_FCK_VCO_DIV2;
	else
		val = PMA_FCK_VCO;
	ret |= linkrx->i2c_reg_update(client, reg, mask, val);

	reg = pma_base + DES_PMA_LOAD0D;
	mask = PMA_PLL_DIV4_MASK;
	if (pma_pll->pll_div4)
		val = PMA_PLL_DIV4;
	else
		val = PMA_PLL_DIV8;
	ret |= linkrx->i2c_reg_update(client, reg, mask, val);

	reg = pma_base + DES_PMA_LOAD0A;
	mask = PMA_CLK_2X_DIV_MASK;
	val = PMA_CLK_2X_DIV(pma_pll->clk_div);
	ret |= linkrx->i2c_reg_update(client, reg, mask, val);

	return ret;
}

int rkx12x_des_pma_set_line(struct rkx12x_linkrx *linkrx, u32 link_line)
{
	struct i2c_client *client = linkrx->client;
	u32 pma_base = DES_PMA0_BASE; /* Fixed pma0 */
	int ret = 0;

	dev_info(&client->dev, "%s: link_line = %d\n", __func__, link_line);

	switch (link_line) {
	case RKX12X_LINK_LINE_COMMON_CONFIG:
		dev_info(&client->dev, "des: set link line common config\n");

		ret |= linkrx->i2c_reg_write(client,
					pma_base + DES_PMA_LOAD08,
					PMA_RX(0x2380));
		ret |= linkrx->i2c_reg_write(client,
					pma_base + DES_PMA_LOAD01,
					PMA_LOS_VTH(0) | PMA_RX_RTERM(0x8));
		ret |= linkrx->i2c_reg_write(client,
					pma_base + DES_PMA_LOAD06,
					PMA_MDATA_AMP_SEL(0x3));
		ret |= linkrx->i2c_reg_write(client,
					pma_base + DES_PMA_REG100,
					0xffff0000);
		break;
	case RKX12X_LINK_LINE_SHORT_CONFIG:
		dev_info(&client->dev, "des: set link line short config\n");

		ret |= linkrx->i2c_reg_write(client,
					pma_base + DES_PMA_LOAD08,
					PMA_RX(0x23b1));
		ret |= linkrx->i2c_reg_write(client,
					pma_base + DES_PMA_LOAD01,
					PMA_LOS_VTH(0) | PMA_RX_RTERM(0x8));
		ret |= linkrx->i2c_reg_write(client,
					pma_base + DES_PMA_LOAD06,
					PMA_MDATA_AMP_SEL(0x3));
		ret |= linkrx->i2c_reg_write(client,
					pma_base + DES_PMA_REG100,
					0xffff0000);
		break;
	case RKX12X_LINK_LINE_LONG_CONFIG:
		dev_info(&client->dev, "des: set link line long config\n");

		ret |= linkrx->i2c_reg_write(client,
					pma_base + DES_PMA_LOAD08,
					PMA_RX(0x2380));
		ret |= linkrx->i2c_reg_write(client,
					pma_base + DES_PMA_LOAD01,
					PMA_LOS_VTH(0) | PMA_RX_RTERM(0x8));
		ret |= linkrx->i2c_reg_write(client,
					pma_base + DES_PMA_LOAD06,
					PMA_MDATA_AMP_SEL(0x3));
		ret |= linkrx->i2c_reg_write(client,
					pma_base + DES_PMA_REG100,
					0xffff0000);
		break;
	default:
		dev_info(&client->dev, "des: unknown link line config\n");
		ret = -EFAULT;
		break;
	}

	return ret;
}

int rkx12x_linkrx_passthrough_cfg(struct rkx12x_linkrx *linkrx, u32 pt_id, bool is_pt_rx)
{
	struct i2c_client *client = linkrx->client;
	const struct rkx12x_des_pt *des_pt = NULL;
	u32 reg, mask = 0, val = 0;
	int ret = 0, i = 0;

	dev_info(&client->dev, "%s: pt_id = %d, is_pt_rx = %d\n",
			__func__, pt_id, is_pt_rx);

	des_pt = NULL;
	for (i = 0; i < ARRAY_SIZE(des_pt_cfg); i++) {
		if (des_pt_cfg[i].id == pt_id) {
			des_pt = &des_pt_cfg[i];
			break;
		}
	}
	if (des_pt == NULL) {
		dev_err(&client->dev, "find link des_pt setting error\n");
		return -EINVAL;
	}

	/* config link passthrough */
	if (des_pt->en_reg) {
		reg = des_pt->en_reg;
		mask = des_pt->en_mask;
		val = des_pt->en_val;
		ret = linkrx->i2c_reg_update(client, reg, mask, val);
		if (ret) {
			dev_err(&client->dev, "en_reg setting error\n");
			return ret;
		}
	}

	if (des_pt->dir_reg) {
		reg = des_pt->dir_reg;
		mask = des_pt->dir_mask;
		val = is_pt_rx ? des_pt->dir_val : ~des_pt->dir_val;
		ret = linkrx->i2c_reg_update(client, reg, mask, val);
		if (ret) {
			dev_err(&client->dev, "dir_reg setting error\n");
			return ret;
		}
	}

	return 0;
}

int rkx12x_linkrx_irq_enable(struct rkx12x_linkrx *linkrx)
{
	struct i2c_client *client = NULL;
	u32 reg = 0, val = 0;
	int ret = 0;

	if ((linkrx == NULL) || (linkrx->client == NULL)
			|| (linkrx->i2c_reg_read == NULL)
			|| (linkrx->i2c_reg_write == NULL)
			|| (linkrx->i2c_reg_update == NULL))
		return -EINVAL;
	client = linkrx->client;

	reg = DES_GRF_IRQ_EN;
	val = DES_IRQ_LINK_EN;
	ret |= linkrx->i2c_reg_write(client, reg, val);

	reg = RKLINK_DES_SINK_IRQ_EN;
	val = FIFO_UNDERRUN_IRQ_OUTPUT_EN | FIFO_OVERFLOW_IRQ_OUTPUT_EN;
	ret |= linkrx->i2c_reg_write(client, reg, val);

	return ret;
}

int rkx12x_linkrx_irq_disable(struct rkx12x_linkrx *linkrx)
{
	struct i2c_client *client = NULL;
	u32 reg = 0, val = 0;
	int ret = 0;

	if ((linkrx == NULL) || (linkrx->client == NULL)
			|| (linkrx->i2c_reg_read == NULL)
			|| (linkrx->i2c_reg_write == NULL)
			|| (linkrx->i2c_reg_update == NULL))
		return -EINVAL;
	client = linkrx->client;

	reg = DES_GRF_IRQ_EN;
	val = DES_IRQ_LINK ^ DES_IRQ_LINK_EN;
	ret |= linkrx->i2c_reg_write(client, reg, val);

	reg = RKLINK_DES_SINK_IRQ_EN;
	ret |= linkrx->i2c_reg_read(client, reg, &val);
	val &= ~(FIFO_UNDERRUN_IRQ_OUTPUT_EN | FIFO_OVERFLOW_IRQ_OUTPUT_EN);
	reg |= linkrx->i2c_reg_write(client, reg, val);

	return ret;
}

static int rkx12x_linkrx_fifo_handler(struct rkx12x_linkrx *linkrx)
{
	struct i2c_client *client = linkrx->client;
	u32 reg = 0, val = 0;
	int ret = 0;

	reg = RKLINK_DES_FIFO_STATUS;
	ret = linkrx->i2c_reg_read(client, reg, &val);
	if (ret)
		return ret;

	if (val & AUDIO_DATA_FIFO_UNDERRUN)
		dev_err(&client->dev, "linkrx audio data fifo underrun\n");
	if (val & AUDIO_ORDER_FIFO_UNDERRUN)
		dev_err(&client->dev, "linkrx audio order fifo underrun\n");
	if (val & VIDEO_DATA_FIFO_UNDERRUN)
		dev_err(&client->dev, "linkrx video data fifo underrun\n");
	if (val & VIDEO_ORDER_FIFO_UNDERRUN)
		dev_err(&client->dev, "linkrx video order fifo underrun\n");
	if (val & CMD_FIFO_UNDERRUN)
		dev_err(&client->dev, "linkrx cmd fifo underrun\n");
	if (val & ENGINE1_DATA_ORDER_MIS)
		dev_err(&client->dev, "linkrx engine1 data order mis\n");
	if (val & ENGINE0_DATA_ORDER_MIS)
		dev_err(&client->dev, "linkrx engine0 data order mis\n");
	if (val & AUDIO_DATA_FIFO_OVERFLOW)
		dev_err(&client->dev, "linkrx audio data fifo overflow\n");
	if (val & AUDIO_ORDER_FIFO_OVERFLOW)
		dev_err(&client->dev, "linkrx audio order fifo overflow\n");
	if (val & VIDEO_DATA_FIFO_OVERFLOW)
		dev_err(&client->dev, "linkrx video data fifo overflow\n");
	if (val & VIDEO_ORDER_FIFO_OVERFLOW)
		dev_err(&client->dev, "linkrx video order fifo overflow\n");
	if (val & CMD_FIFO_OVERFLOW)
		dev_err(&client->dev, "linkrx cmd fifo overflow\n");

	ret = linkrx->i2c_reg_write(client, reg, val);

	return ret;
}

int rkx12x_linkrx_irq_handler(struct rkx12x_linkrx *linkrx)
{
	struct i2c_client *client = NULL;
	u32 reg = 0, val = 0, flag = 0;
	int ret = 0, i = 0;

	if ((linkrx == NULL) || (linkrx->client == NULL)
			|| (linkrx->i2c_reg_read == NULL)
			|| (linkrx->i2c_reg_write == NULL)
			|| (linkrx->i2c_reg_update == NULL))
		return -EINVAL;
	client = linkrx->client;

	ret = linkrx->i2c_reg_read(client, RKLINK_DES_SINK_IRQ_EN, &flag);
	if (ret)
		return ret;

	flag &= (COMP_NOT_ENOUGH_IRQ_FLAG
			| VIDEO_FM_IRQ_FLAG
			| AUDIO_FM_IRQ_FLAG
			| ORDER_MIS_IRQ_FLAG
			| FIFO_UNDERRUN_IRQ_FLAG
			| FIFO_OVERFLOW_IRQ_FLAG
			| PKT_LOSE_IRQ_FLAG
			| LAST_ERROR_IRQ_FLAG
			| ECC2BIT_ERROR_IRQ_FLAG
			| ECC1BIT_ERROR_IRQ_FLAG
			| CRC_ERROR_IRQ_FLAG
		);
	dev_info(&client->dev, "linkrx irq flag:0x%08x\n", flag);

	i = 0;
	while (flag) {
		switch (flag & BIT(i)) {
		case COMP_NOT_ENOUGH_IRQ_FLAG:
			break;
		case VIDEO_FM_IRQ_FLAG:
			break;
		case AUDIO_FM_IRQ_FLAG:
			break;
		case ORDER_MIS_IRQ_FLAG:
		case FIFO_UNDERRUN_IRQ_FLAG:
		case FIFO_OVERFLOW_IRQ_FLAG:
			flag &= ~(ORDER_MIS_IRQ_FLAG
					| FIFO_UNDERRUN_IRQ_FLAG
					| FIFO_OVERFLOW_IRQ_FLAG
				);
			ret |= rkx12x_linkrx_fifo_handler(linkrx);
			break;
		case PKT_LOSE_IRQ_FLAG:
			/* clear pkt lost irq flag */
			reg = RKLINK_DES_LANE_ENGINE_CFG;
			ret |= linkrx->i2c_reg_read(client, reg, &val);
			val |= LANE0_PKT_LOSE_NUM_CLR | LANE1_PKT_LOSE_NUM_CLR;
			ret |= linkrx->i2c_reg_write(client, reg, val);
			break;
		case LAST_ERROR_IRQ_FLAG:
		case ECC2BIT_ERROR_IRQ_FLAG:
		case ECC1BIT_ERROR_IRQ_FLAG:
		case CRC_ERROR_IRQ_FLAG:
			flag &= ~(LAST_ERROR_IRQ_FLAG
					| ECC2BIT_ERROR_IRQ_FLAG
					| ECC1BIT_ERROR_IRQ_FLAG
					| CRC_ERROR_IRQ_FLAG
				);
			reg = RKLINK_DES_SINK_IRQ_EN;
			ret |= linkrx->i2c_reg_read(client, reg, &val);
			dev_info(&client->dev, "linkrx ecc crc result: 0x%08x\n", val);
			/* clear ecc crc irq flag */
			ret |= linkrx->i2c_reg_write(client, reg, val);
			break;
		default:
			break;
		}
		flag &= ~BIT(i);
		i++;
	}

	return ret;
}

int rkx12x_des_pcs_irq_enable(struct rkx12x_linkrx *linkrx, u32 pcs_id)
{
	struct i2c_client *client = NULL;
	u32 reg = 0, val = 0;
	int ret = 0;

	if ((linkrx == NULL) || (linkrx->client == NULL)
			|| (linkrx->i2c_reg_read == NULL)
			|| (linkrx->i2c_reg_write == NULL)
			|| (linkrx->i2c_reg_update == NULL))
		return -EINVAL;
	client = linkrx->client;

	reg = DES_GRF_IRQ_EN;
	val = pcs_id ? DES_IRQ_PCS1_EN : DES_IRQ_PCS0_EN;
	ret |= linkrx->i2c_reg_write(client, reg, val);

	reg = DES_PCS_DEV(pcs_id) + DES_PCS_REG30;
	val = PCS_INI_EN(0xffff);
	ret |= linkrx->i2c_reg_write(client, reg, val);

	return ret;
}

int rkx12x_des_pcs_irq_disable(struct rkx12x_linkrx *linkrx, u32 pcs_id)
{
	struct i2c_client *client = NULL;
	u32 reg = 0, val = 0;
	int ret = 0;

	if ((linkrx == NULL) || (linkrx->client == NULL)
			|| (linkrx->i2c_reg_read == NULL)
			|| (linkrx->i2c_reg_write == NULL)
			|| (linkrx->i2c_reg_update == NULL))
		return -EINVAL;
	client = linkrx->client;

	reg = DES_GRF_IRQ_EN;
	val = pcs_id ? (DES_IRQ_PCS1 ^ DES_IRQ_PCS1_EN) : (DES_IRQ_PCS0 ^ DES_IRQ_PCS0_EN);
	ret |= linkrx->i2c_reg_write(client, reg, val);

	reg = DES_PCS_DEV(pcs_id) + DES_PCS_REG30;
	val = PCS_INI_EN(0);
	ret |= linkrx->i2c_reg_write(client, reg, val);

	return ret;
}

int rkx12x_des_pcs_irq_handler(struct rkx12x_linkrx *linkrx, u32 pcs_id)
{
	struct i2c_client *client = NULL;
	u32 reg = 0, val = 0;
	int ret = 0;

	if ((linkrx == NULL) || (linkrx->client == NULL)
			|| (linkrx->i2c_reg_read == NULL)
			|| (linkrx->i2c_reg_write == NULL)
			|| (linkrx->i2c_reg_update == NULL))
		return -EINVAL;
	client = linkrx->client;

	reg = DES_PCS_DEV(pcs_id) + DES_PCS_REG20;
	ret |= linkrx->i2c_reg_read(client, reg, &val);
	dev_info(&client->dev, "des pcs fatal status: 0x%08x\n", val);

	/* clear fatal status */
	reg = DES_PCS_DEV(pcs_id) + DES_PCS_REG10;
	ret |= linkrx->i2c_reg_write(client, reg, 0xffffffff);
	ret |= linkrx->i2c_reg_write(client, reg, 0xffff0000);

	return ret;
}

int rkx12x_des_pma_irq_enable(struct rkx12x_linkrx *linkrx, u32 pma_id)
{
	struct i2c_client *client = NULL;
	u32 reg = 0, val = 0;
	int ret = 0;

	if ((linkrx == NULL) || (linkrx->client == NULL)
			|| (linkrx->i2c_reg_read == NULL)
			|| (linkrx->i2c_reg_write == NULL)
			|| (linkrx->i2c_reg_update == NULL))
		return -EINVAL;
	client = linkrx->client;

	reg = DES_GRF_IRQ_EN;
	val = pma_id ? DES_IRQ_PMA_ADAPT1_EN : DES_IRQ_PMA_ADAPT0_EN;
	ret |= linkrx->i2c_reg_write(client, reg, val);

	reg = DES_PMA_DEV(pma_id) + DES_PMA_IRQ_EN;
	val = (FORCE_INITIAL_IRQ_EN
			| RX_RDY_NEG_IRQ_EN
			| RX_LOS_IRQ_EN
			| RX_RDY_TIMEOUT_IRQ_EN
			| PLL_LOCK_TIMEOUT_IRQ_EN);
	ret |= linkrx->i2c_reg_write(client, reg, val);

	return ret;
}

int rkx12x_des_pma_irq_disable(struct rkx12x_linkrx *linkrx, u32 pma_id)
{
	struct i2c_client *client = NULL;
	u32 reg = 0, val = 0;
	int ret = 0;

	if ((linkrx == NULL) || (linkrx->client == NULL)
			|| (linkrx->i2c_reg_read == NULL)
			|| (linkrx->i2c_reg_write == NULL)
			|| (linkrx->i2c_reg_update == NULL))
		return -EINVAL;
	client = linkrx->client;

	reg = DES_GRF_IRQ_EN;
	val = pma_id ? (DES_IRQ_PMA_ADAPT1 ^ DES_IRQ_PMA_ADAPT1_EN) :
			(DES_IRQ_PMA_ADAPT0 ^ DES_IRQ_PMA_ADAPT0_EN);
	ret |= linkrx->i2c_reg_write(client, reg, val);

	reg = DES_PMA_DEV(pma_id) + DES_PMA_IRQ_EN;
	ret |= linkrx->i2c_reg_write(client, reg, 0);

	return ret;
}

int rkx12x_des_pma_irq_handler(struct rkx12x_linkrx *linkrx, u32 pma_id)
{
	struct i2c_client *client = NULL;
	u32 reg = 0, status = 0;
	int ret = 0;

	if ((linkrx == NULL) || (linkrx->client == NULL)
			|| (linkrx->i2c_reg_read == NULL)
			|| (linkrx->i2c_reg_write == NULL)
			|| (linkrx->i2c_reg_update == NULL))
		return -EINVAL;
	client = linkrx->client;

	reg = DES_PMA_DEV(pma_id) + DES_PMA_IRQ_STATUS;
	ret = linkrx->i2c_reg_read(client, reg, &status);
	if (ret)
		return -EINVAL;

	dev_info(&client->dev, "des pma irq status: 0x%08x\n", status);

	if (status == 0)
		return -EINVAL;

	if (status & FORCE_INITIAL_IRQ_STATUS)
		dev_info(&client->dev, "des pma trig force initial pulse status\n");
	else if (status & RX_RDY_NEG_IRQ_STATUS)
		dev_info(&client->dev, "des pma trig rx rdy neg status\n");
	else if (status & RX_LOS_IRQ_STATUS)
		dev_info(&client->dev, "des pma trig rx los status\n");
	else if (status & RX_RDY_TIMEOUT_IRQ_STATUS)
		dev_info(&client->dev, "des pma trig rx rdy timeout status\n");
	else if (status & PLL_LOCK_TIMEOUT_IRQ_STATUS)
		dev_info(&client->dev, "des pma trig pll lock timeout status\n");
	else
		dev_info(&client->dev, "des pma trig unknown\n");

	return 0;
}
