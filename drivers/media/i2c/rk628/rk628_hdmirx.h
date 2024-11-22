/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 Rockchip Electronics Co. Ltd.
 *
 * Author: Shunqing Chen <csq@rock-chips.com>
 */

#ifndef __RK628_HDMIRX_H
#define __RK628_HDMIRX_H

#include <linux/gpio/consumer.h>
#include <media/cec.h>
#include <media/cec-notifier.h>
#include <media/v4l2-dv-timings.h>

#include "rk628.h"
#include "rk628_cru.h"

/* --------- EDID and HDCP KEY ------- */
#define EDID_BASE			0x000a0000
#define HDCP_KEY_BASE			0x000a8000
#define HDCP_KEY_KSV0			(HDCP_KEY_BASE + 4)
#define HDCP_KEY_DPK0			(HDCP_KEY_BASE + 36)

#define KEY_MAX_REGISTER		0x000a8490

/* --------- GPIO0 REG --------------- */
#define GPIO0_SWPORT_DDR_L		0xd0008
#define GPIO1_SWPORT_DR_L		0xe0000
#define GPIO1_SWPORT_DDR_L		0xe0008
#define GPIO1_VER_ID			0xe0078

/* --------- HDMI RX REG ------------- */
#define HDMI_RX_BASE			0x00030000
#define HDMI_RX_HDMI_SETUP_CTRL		(HDMI_RX_BASE + 0x0000)
#define HOT_PLUG_DETECT_INPUT_A_MASK	BIT(24)
#define HOT_PLUG_DETECT_INPUT_A(x)	UPDATE(x, 24, 24)
#define HOT_PLUG_DETECT_MASK		BIT(0)
#define HOT_PLUG_DETECT(x)		UPDATE(x, 0, 0)
#define HDMI_RX_HDMI_TIMER_CTRL		(HDMI_RX_BASE + 0x0008)
#define VIDEO_PERIOD_MASK		BIT(11)
#define VIDEO_PERIOD(x)			UPDATE(x, 11, 11)
#define HDMI_RX_HDMI_RES_OVR		(HDMI_RX_BASE + 0x0010)
#define HDMI_RX_HDMI_PLL_FRQSET2	(HDMI_RX_BASE + 0x0020)
#define HDMI_RX_HDMI_PCB_CTRL		(HDMI_RX_BASE + 0x0038)
#define SEL_PIXCLKSRC(x)		UPDATE(x, 19, 18)
#define INPUT_SELECT_MASK		BIT(16)
#define INPUT_SELECT(x)			UPDATE(x, 16, 16)
#define HDMI_RX_HDMI_PHS_CTR		(HDMI_RX_BASE + 0x0040)
#define HDMI_RX_HDMI_EQ_MEAS_CTRL	(HDMI_RX_BASE + 0x005c)
#define HDMI_RX_HDMI_CTRL		(HDMI_RX_BASE + 0x0064)
#define HDMI_RX_HDMI_MODE_RECOVER	(HDMI_RX_BASE + 0x0080)
#define SPIKE_FILTER_EN_MASK		BIT(18)
#define SPIKE_FILTER_EN(x)		UPDATE(x, 18, 18)
#define DVI_MODE_HYST_MASK		GENMASK(17, 13)
#define DVI_MODE_HYST(x)		UPDATE(x, 17, 13)
#define HDMI_MODE_HYST_MASK		GENMASK(12, 8)
#define HDMI_MODE_HYST(x)		UPDATE(x, 12, 8)
#define HDMI_MODE_MASK			GENMASK(7, 6)
#define HDMI_MODE(x)			UPDATE(x, 7, 6)
#define GB_DET_MASK			GENMASK(5, 4)
#define GB_DET(x)			UPDATE(x, 5, 4)
#define EESS_OESS_MASK			GENMASK(3, 2)
#define EESS_OESS(x)			UPDATE(x, 3, 2)
#define SEL_CTL01_MASK			GENMASK(1, 0)
#define SEL_CTL01(x)			UPDATE(x, 1, 0)
#define HDMI_RX_HDMI_ERROR_PROTECT	(HDMI_RX_BASE + 0x0084)
#define RG_BLOCK_OFF(x)			UPDATE(x, 20, 20)
#define BLOCK_OFF(x)			UPDATE(x, 19, 19)
#define VALID_MODE(x)			UPDATE(x, 18, 16)
#define CTRL_FILT_SENS(x)		UPDATE(x, 13, 12)
#define VS_FILT_SENS(x)			UPDATE(x, 11, 10)
#define HS_FILT_SENS(x)			UPDATE(x, 9, 8)
#define DE_MEASURE_MODE(x)		UPDATE(x, 7, 6)
#define DE_REGEN(x)			UPDATE(x, 5, 5)
#define DE_FILTER_SENS(x)		UPDATE(x, 4, 3)
#define HDMI_RX_HDMI_ERD_STS		(HDMI_RX_BASE + 0x0088)
#define HDMI_RX_HDMI_SYNC_CTRL		(HDMI_RX_BASE + 0x0090)
#define VS_POL_ADJ_MODE_MASK		GENMASK(4, 3)
#define VS_POL_ADJ_MODE(x)		UPDATE(x, 4, 3)
#define HS_POL_ADJ_MODE_MASK		GENMASK(2, 1)
#define HS_POL_ADJ_MODE(x)		UPDATE(x, 2, 1)
#define HDMI_RX_HDMI_CKM_EVLTM		(HDMI_RX_BASE + 0x0094)
#define HDMI_RX_HDMI_CKM_F		(HDMI_RX_BASE + 0x0098)
#define HDMI_RX_HDMI_CKM_RESULT		(HDMI_RX_BASE + 0x009c)
#define HDMI_RX_HDMI_RESMPL_CTRL	(HDMI_RX_BASE + 0x00a4)
#define HDMI_RX_HDMI_DCM_CTRL		(HDMI_RX_BASE + 0x00a8)
#define DCM_DEFAULT_PHASE(x)		UPDATE(x, 18, 18)
#define DCM_COLOUR_DEPTH_SEL(x)		UPDATE(x, 12, 12)
#define DCM_COLOUR_DEPTH(x)		UPDATE(x, 11, 8)
#define DCM_GCP_ZERO_FIELDS(x)		UPDATE(x, 5, 2)
#define HDMI_VM_CFG_CH0_1		(HDMI_RX_BASE + 0x00b0)
#define HDMI_VM_CFG_CH2			(HDMI_RX_BASE + 0x00b4)
#define HDMI_RX_HDCP_CTRL		(HDMI_RX_BASE + 0x00c0)
#define HDCP_ENABLE_MASK		BIT(24)
#define HDCP_ENABLE(x)			UPDATE(x, 24, 24)
#define FREEZE_HDCP_FSM_MASK		BIT(21)
#define FREEZE_HDCP_FSM(x)		UPDATE(x, 21, 21)
#define FREEZE_HDCP_STATE_MASK		GENMASK(20, 15)
#define FREEZE_HDCP_STATE(x)		UPDATE(x, 20, 15)
#define HDCP_CTL_MASK			GENMASK(9, 8)
#define HDCP_CTL(x)			UPDATE(x, 9, 8)
#define HDCP_RI_RATE_MASK		GENMASK(7, 6)
#define HDCP_RI_RATE(x)			UPDATE(x, 7, 6)
#define HDMI_MODE_ENABLE_MASK		BIT(2)
#define HDMI_MODE_ENABLE(x)		UPDATE(x, 2, 2)
#define KEY_DECRIPT_ENABLE_MASK		BIT(1)
#define KEY_DECRIPT_ENABLE(x)		UPDATE(x, 1, 1)
#define HDCP_ENC_EN_MASK		BIT(0)
#define HDCP_ENC_EN(x)			UPDATE(x, 0, 0)
#define HDMI_RX_HDCP_SETTINGS		(HDMI_RX_BASE + 0x00c4)
#define HDMI_RESERVED(x)		UPDATE(x, 13, 13)
#define HDMI_RESERVED_MASK		BIT(13)
#define FAST_I2C(x)			UPDATE(x, 12, 12)
#define FAST_I2C_MASK			BIT(12)
#define ONE_DOT_ONE(x)			UPDATE(x, 9, 9)
#define ONE_DOT_ONE_MASK		BIT(9)
#define FAST_REAUTH(x)			UPDATE(x, 8, 8)
#define FAST_REAUTH_MASK		BIT(8)
#define HDMI_RX_HDCP_SEED		(HDMI_RX_BASE + 0x00c8)
#define HDMI_RX_HDCP_KIDX		(HDMI_RX_BASE + 0x00d4)
#define HDMI_RX_HDCP_DBG		(HDMI_RX_BASE + 0x00e0)
#define HDMI_RX_HDCP_AN0		(HDMI_RX_BASE + 0x00f0)
#define HDMI_RX_HDCP_STS		(HDMI_RX_BASE + 0x00fc)
#define HDCP_ENC_STATE			BIT(9)
#define HDCP_AUTH_START			BIT(8)
#define HDMI_RX_MD_HCTRL1		(HDMI_RX_BASE + 0x0140)
#define HACT_PIX_ITH(x)			UPDATE(x, 10, 8)
#define HACT_PIX_SRC(x)			UPDATE(x, 5, 5)
#define HTOT_PIX_SRC(x)			UPDATE(x, 4, 4)
#define HDMI_RX_MD_HCTRL2		(HDMI_RX_BASE + 0x0144)
#define HS_CLK_ITH(x)			UPDATE(x, 14, 12)
#define HTOT32_CLK_ITH(x)		UPDATE(x, 9, 8)
#define VS_ACT_TIME(x)			UPDATE(x, 5, 5)
#define HS_ACT_TIME(x)			UPDATE(x, 4, 3)
#define H_START_POS(x)			UPDATE(x, 1, 0)
#define HDMI_RX_MD_HT0			(HDMI_RX_BASE + 0x0148)
#define HDMI_RX_MD_HT1			(HDMI_RX_BASE + 0x014c)
#define HDMI_RX_MD_HACT_PX		(HDMI_RX_BASE + 0x0150)
#define HDMI_RX_MD_VCTRL		(HDMI_RX_BASE + 0x0158)
#define V_OFFS_LIN_MODE(x)		UPDATE(x, 4, 4)
#define V_EDGE(x)			UPDATE(x, 1, 1)
#define V_MODE(x)			UPDATE(x, 0, 0)
#define HDMI_RX_MD_VSC			(HDMI_RX_BASE + 0x015c)
#define HDMI_RX_MD_VOL			(HDMI_RX_BASE + 0x0164)
#define HDMI_RX_MD_VAL			(HDMI_RX_BASE + 0x0168)
#define HDMI_RX_MD_VTH			(HDMI_RX_BASE + 0x016c)
#define VOFS_LIN_ITH(x)			UPDATE(x, 11, 10)
#define VACT_LIN_ITH(x)			UPDATE(x, 9, 8)
#define VTOT_LIN_ITH(x)			UPDATE(x, 7, 6)
#define VS_CLK_ITH(x)			UPDATE(x, 5, 3)
#define VTOT_CLK_ITH(x)			UPDATE(x, 2, 0)
#define HDMI_RX_MD_VTL			(HDMI_RX_BASE + 0x0170)
#define HDMI_RX_MD_IL_POL		(HDMI_RX_BASE + 0x017c)
#define FAFIELDDET_EN(x)		UPDATE(x, 2, 2)
#define FIELD_POL_MODE(x)		UPDATE(x, 1, 0)
#define HDMI_RX_MD_STS			(HDMI_RX_BASE + 0x0180)
#define ILACE_STS			BIT(3)
#define HDMI_RX_AUD_CTRL		(HDMI_RX_BASE + 0x0200)
#define HDMI_RX_AUD_PLL_CTRL		(HDMI_RX_BASE + 0x0208)
#define PLL_LOCK_TOGGLE_DIV_MASK	GENMASK(27, 24)
#define PLL_LOCK_TOGGLE_DIV(x)		UPDATE(x, 27, 24)
#define HDMI_RX_AUD_CLK_CTRL		(HDMI_RX_BASE + 0x0214)
#define CTS_N_REF_MASK			BIT(4)
#define CTS_N_REF(x)			UPDATE(x, 4, 4)
#define HDMI_RX_AUD_FIFO_CTRL		(HDMI_RX_BASE + 0x0240)
#define AFIF_SUBPACKET_DESEL_MASK	GENMASK(27, 24)
#define AFIF_SUBPACKET_DESEL(x)		UPDATE(x, 27, 24)
#define AFIF_SUBPACKETS_MASK		BIT(16)
#define AFIF_SUBPACKETS(x)		UPDATE(x, 16, 16)
#define HDMI_RX_AUD_FIFO_TH		(HDMI_RX_BASE + 0x0244)
#define AFIF_TH_START_MASK		GENMASK(26, 18)
#define AFIF_TH_START(x)		UPDATE(x, 26, 18)
#define AFIF_TH_MAX_MASK		GENMASK(17, 9)
#define AFIF_TH_MAX(x)			UPDATE(x, 17, 9)
#define AFIF_TH_MIN_MASK		GENMASK(8, 0)
#define AFIF_TH_MIN(x)			UPDATE(x, 8, 0)
#define HDMI_RX_AUD_CHEXTR_CTRL		(HDMI_RX_BASE + 0x0254)
#define AUD_LAYOUT_CTRL(x)		UPDATE(x, 1, 0)
#define HDMI_RX_AUD_MUTE_CTRL		(HDMI_RX_BASE + 0x0258)
#define APPLY_INT_MUTE(x)		UPDATE(x, 31, 31)
#define APORT_SHDW_CTRL(x)		UPDATE(x, 22, 21)
#define AUTO_ACLK_MUTE(x)		UPDATE(x, 20, 19)
#define AUD_MUTE_SPEED(x)		UPDATE(x, 16, 10)
#define AUD_AVMUTE_EN(x)		UPDATE(x, 7, 7)
#define AUD_MUTE_SEL(x)			UPDATE(x, 6, 5)
#define AUD_MUTE_MODE(x)		UPDATE(x, 4, 3)
#define HDMI_RX_AUD_FIFO_FILLSTS1	(HDMI_RX_BASE + 0x025c)
#define HDMI_RX_AUD_SAO_CTRL		(HDMI_RX_BASE + 0x0260)
#define I2S_ENABLE_BITS_MASK		GENMASK(10, 5)
#define I2S_ENABLE_BITS(x)		UPDATE(x, 10, 5)
#define I2S_CLK_ENABLE_BITS(x)		UPDATE(x, 10, 9)
#define I2S_DATA_ENABLE_BITS(x)		UPDATE(x, 8, 5)
#define I2S_LPCM_BPCUV_MASK		BIT(11)
#define I2S_LPCM_BPCUV(x)		UPDATE(x, 11, 11)
#define I2S_32_16_MASK			BIT(0)
#define I2S_32_16(x)			UPDATE(x, 0, 0)
#define HDMI_RX_AUD_PAO_CTRL		(HDMI_RX_BASE + 0x0264)
#define PAO_RATE(x)			UPDATE(x, 17, 16)
#define HDMI_RX_AUD_SPARE		(HDMI_RX_BASE + 0x0268)
#define AUDS_MAS_SAMPLE_FLAT		GENMASK(7, 4)
#define HDMI_RX_AUD_FIFO_STS		(HDMI_RX_BASE + 0x027c)
#define HDMI_RX_AUDPLL_GEN_CTS		(HDMI_RX_BASE + 0x0280)
#define HDMI_RX_AUDPLL_GEN_N		(HDMI_RX_BASE + 0x0284)
#define HDMI_RX_SNPS_PHYG3_CTRL		(HDMI_RX_BASE + 0x02c0)
#define PORTSELECT(x)			UPDATE(x, 3, 2)
#define HDMI_RX_I2CM_PHYG3_SLAVE	(HDMI_RX_BASE + 0x02c4)
#define HDMI_RX_I2CM_PHYG3_ADDRESS	(HDMI_RX_BASE + 0x02c8)
#define HDMI_RX_I2CM_PHYG3_DATAO	(HDMI_RX_BASE + 0x02cc)
#define HDMI_RX_I2CM_PHYG3_DATAI	(HDMI_RX_BASE + 0x02d0)
#define HDMI_RX_I2CM_PHYG3_OPERATION	(HDMI_RX_BASE + 0x02d4)
#define HDMI_RX_I2CM_PHYG3_MODE		(HDMI_RX_BASE + 0x02d8)
#define HDMI_RX_I2CM_PHYG3_SS_CNTS	(HDMI_RX_BASE + 0x02e0)
#define HDMI_RX_I2CM_PHYG3_FS_HCNT	(HDMI_RX_BASE + 0x02e4)

#define HDMI_RX_PDEC_CTRL		(HDMI_RX_BASE + 0x0300)
#define PFIFO_STORE_FILTER_EN_MASK	BIT(31)
#define PFIFO_STORE_FILTER_EN(x)	UPDATE(x, 31, 31)
#define PFIFO_STORE_DRM_IF_MASK		BIT(29)
#define PFIFO_STORE_DRM_IF(x)		UPDATE(x, 29, 29)
#define PFIFO_STORE_AMP_MASK		BIT(28)
#define PFIFO_STORE_AMP(x)		UPDATE(x, 28, 28)
#define PFIFO_STORE_NTSCVBI_IF_MASK	BIT(27)
#define PFIFO_STORE_NTSCVBI_IF(x)	UPDATE(x, 27, 27)
#define PFIFO_STORE_MPEGS_IF_MASK	BIT(26)
#define PFIFO_STORE_MPEGS_IF(x)		UPDATE(x, 26, 26)
#define PFIFO_STORE_AUD_IF_MASK		BIT(25)
#define PFIFO_STORE_AUD_IF(x)		UPDATE(x, 25, 25)
#define PFIFO_STORE_SPD_IF_MASK		BIT(24)
#define PFIFO_STORE_SPD_IF(x)		UPDATE(x, 24, 24)
#define PFIFO_STORE_AVI_IF_MASK		BIT(23)
#define PFIFO_STORE_AVI_IF(x)		UPDATE(x, 23, 23)
#define PFIFO_STORE_VS_IF_MASK		BIT(22)
#define PFIFO_STORE_VS_IF(x)		UPDATE(x, 22, 22)
#define PFIFO_STORE_GMTP_MASK		BIT(21)
#define PFIFO_STORE_GMTP(x)		UPDATE(x, 21, 21)
#define PFIFO_STORE_ISRC2_MASK		BIT(20)
#define PFIFO_STORE_ISRC2(x)		UPDATE(x, 20, 20)
#define PFIFO_STORE_ISRC1_MASK		BIT(19)
#define PFIFO_STORE_ISRC1(x)		UPDATE(x, 19, 19)
#define PFIFO_STORE_ACP_MASK		BIT(18)
#define PFIFO_STORE_ACP(x)		UPDATE(x, 18, 18)
#define PFIFO_STORE_GCP_MASK		BIT(17)
#define PFIFO_STORE_GCP(x)		UPDATE(x, 17, 17)
#define PFIFO_STORE_ACR_MASK		BIT(16)
#define PFIFO_STORE_ACR(x)		UPDATE(x, 16, 16)
#define GCPFORCE_CLRAVMUTE_MASK		BIT(14)
#define GCPFORCE_CLRAVMUTE(x)		UPDATE(x, 14, 14)
#define GCPFORCE_SETAVMUTE_MASK		BIT(13)
#define GCPFORCE_SETAVMUTE(x)		UPDATE(x, 13, 13)
#define PDEC_BCH_EN_MASK		BIT(0)
#define PDEC_BCH_EN(x)			UPDATE(x, 0, 0)
#define HDMI_RX_PDEC_FIFO_CFG		(HDMI_RX_BASE + 0x0304)
#define HDMI_RX_PDEC_AUDIODET_CTRL	(HDMI_RX_BASE + 0x0310)
#define AUDIODET_THRESHOLD(x)		UPDATE(x, 13, 9)
#define HDMI_RX_PDEC_ACRM_CTRL		(HDMI_RX_BASE + 0x0330)
#define DELTACTS_IRQTRIG(x)		UPDATE(x, 4, 2)
#define HDMI_RX_PDEC_ERR_FILTER		(HDMI_RX_BASE + 0x033c)
#define HDMI_RX_PDEC_ASP_CTRL		(HDMI_RX_BASE + 0x0340)
#define HDMI_RX_PDEC_STS		(HDMI_RX_BASE + 0x0360)
#define DVI_DET				BIT(28)
#define HDMI_RX_PDEC_GCP_AVMUTE		(HDMI_RX_BASE + 0x0380)
#define PKTDEC_GCP_CD_MASK		GENMASK(7, 4)
#define PKTDEC_GCP_SETAVMUTE_MASK	GENMASK(1, 1)
#define PKTDEC_GCP_CLRAVMUTE_MASK	GENMASK(0, 0)
#define HDMI_RX_PDEC_AVI_HB		(HDMI_RX_BASE + 0x03a0)
#define HDMI_RX_PDEC_AVI_PB		(HDMI_RX_BASE + 0x03a4)
#define VID_IDENT_CODE_VIC7		BIT(31)
#define VID_IDENT_CODE_MASK		GENMASK(30, 24)
#define EXT_COLORIMETRY_MASK		GENMASK(22, 20)
#define COLORIMETRY_MASK		GENMASK(15, 14)
#define VIDEO_FORMAT_MASK		GENMASK(6, 5)
#define VIDEO_FORMAT(x)			UPDATE(x, 6, 5)
#define RGB_COLORRANGE_MASK		GENMASK(19, 18)
#define YUV_COLORRANGE_MASK		GENMASK(31, 30)
#define RGB_COLORRANGE(x)		UPDATE(x, 19, 18)
#define ACT_INFO_PRESENT_MASK		BIT(4)
#define HDMI_RX_PDEC_ACR_CTS		(HDMI_RX_BASE + 0x0390)
#define HDMI_RX_PDEC_ACR_N		(HDMI_RX_BASE + 0x0394)
#define HDMI_RX_PDEC_AIF_CTRL		(HDMI_RX_BASE + 0x03c0)
#define FC_LFE_EXCHG(x)			UPDATE(x, 18, 18)
#define HDMI_RX_PDEC_AIF_PB0		(HDMI_RX_BASE + 0x03c8)

#define HDMI_RX_HDMI20_CONTROL		(HDMI_RX_BASE + 0x0800)
#define PVO1UNMUTE(x)			UPDATE(x, 29, 29)
#define PIXELMODE(x)			UPDATE(x, 28, 28)
#define CTRLCHECKEN(x)			UPDATE(x, 8, 8)
#define SCDC_ENABLE_MASK		BIT(4)
#define SCDC_ENABLE(x)			UPDATE(x, 4, 4)
#define SCRAMBEN_SEL(x)			UPDATE(x, 1, 0)
#define HDMI_RX_SCDC_I2CCONFIG		(HDMI_RX_BASE + 0x0804)
#define I2CSPIKESUPPR(x)		UPDATE(x, 25, 24)
#define HDMI_RX_SCDC_CONFIG		(HDMI_RX_BASE + 0x0808)
#define POWERPROVIDED_MASK		BIT(0)
#define HDMI_RX_CHLOCK_CONFIG		(HDMI_RX_BASE + 0x080c)
#define CHLOCKMAXER(x)			UPDATE(x, 29, 20)
#define MILISECTIMERLIMIT(x)		UPDATE(x, 15, 0)
#define HDMI_RX_HDCP22_CONTROL		(HDMI_RX_BASE + 0x081c)
#define HDMI_RX_SCDC_REGS0		(HDMI_RX_BASE + 0x0820)
#define SCDC_TMDSBITCLKRATIO		BIT(17)
#define HDMI_RX_SCDC_REGS1		(HDMI_RX_BASE + 0x0824)
#define HDMI_RX_SCDC_REGS2		(HDMI_RX_BASE + 0x0828)
#define SCDC_ERRDET_MASK		GENMASK(14, 0)
#define HDMI_RX_SCDC_REGS3		(HDMI_RX_BASE + 0x082c)
#define HDMI_RX_SCDC_WRDATA0		(HDMI_RX_BASE + 0x0860)
#define MANUFACTUREROUI(x)		UPDATE(x, 31, 8)
#define SINKVERSION(x)			UPDATE(x, 7, 0)
#define HDMI_RX_HDMI20_STATUS		(HDMI_RX_BASE + 0x08e0)
#define SCRAMBDET_MASK			BIT(0)

#define HDMI_RX_HDMI2_IEN_CLR		(HDMI_RX_BASE + 0x0f60)
#define HDMI_RX_HDMI2_ISTS		(HDMI_RX_BASE + 0x0f68)
#define HDMI_RX_PDEC_IEN_CLR		(HDMI_RX_BASE + 0x0f78)
#define AVI_CKS_CHG_ICLR		BIT(24)
#define ACR_N_CHG_ICLR			BIT(23)
#define ACR_CTS_CHG_ICLR		BIT(22)
#define GCP_AV_MUTE_CHG_ENCLR		BIT(21)
#define AIF_RCV_ENCLR			BIT(19)
#define AVI_RCV_ENCLR			BIT(18)
#define GCP_RCV_ENCLR			BIT(16)
#define HDMI_RX_PDEC_IEN_SET		(HDMI_RX_BASE + 0x0f7c)
#define ACR_N_CHG_IEN			BIT(23)
#define ACR_CTS_CHG_IEN			BIT(22)
#define GCP_AV_MUTE_CHG_ENSET		BIT(21)
#define AIF_RCV_ENSET			BIT(19)
#define AVI_RCV_ENSET			BIT(18)
#define GCP_RCV_ENSET			BIT(16)
#define AMP_RCV_ENSET			BIT(14)
#define HDMI_RX_PDEC_ISTS		(HDMI_RX_BASE + 0x0f80)
#define AVI_CKS_CHG_ISTS		BIT(24)
#define GCP_AV_MUTE_CHG_ISTS		BIT(21)
#define AIF_RCV_ISTS			BIT(19)
#define AVI_RCV_ISTS			BIT(18)
#define GCP_RCV_ISTS			BIT(16)
#define AMP_RCV_ISTS			BIT(14)
#define HDMI_RX_PDEC_IEN		(HDMI_RX_BASE + 0x0f84)
#define HDMI_RX_PDEC_ICLR		(HDMI_RX_BASE + 0x0f88)
#define HDMI_RX_PDEC_ISET		(HDMI_RX_BASE + 0x0f8c)
#define HDMI_RX_AUD_CEC_IEN_CLR		(HDMI_RX_BASE + 0x0f90)
#define HDMI_RX_AUD_CEC_IEN_SET		(HDMI_RX_BASE + 0x0f94)
#define ERROR_INIT_ENSET		BIT(20)
#define ARBLST_ENSET			BIT(19)
#define NACK_ENSET			BIT(18)
#define EOM_ENSET			BIT(17)
#define DONE_ENSET			BIT(16)
#define HDMI_RX_AUD_CEC_ISTS		(HDMI_RX_BASE + 0x0f98)
#define ERROR_INIT			BIT(20)
#define ARBLST				BIT(19)
#define NACK				BIT(18)
#define EOM				BIT(17)
#define DONE				BIT(16)
#define HDMI_RX_AUD_CEC_IEN		(HDMI_RX_BASE + 0x0f9c)
#define HDMI_RX_AUD_CEC_ICLR		(HDMI_RX_BASE + 0x0fa0)
#define HDMI_RX_AUD_CEC_ISET		(HDMI_RX_BASE + 0x0fa4)
#define HDMI_RX_AUD_FIFO_IEN_CLR	(HDMI_RX_BASE + 0x0fa8)
#define HDMI_RX_AUD_FIFO_IEN_SET	(HDMI_RX_BASE + 0x0fac)
#define AFIF_OVERFL_ENSET		BIT(4)
#define AFIF_UNDERFL_ENSET		BIT(3)
#define AFIF_THS_PASS_ENSET		BIT(2)
#define AFIF_TH_MAX_ENSET		BIT(1)
#define AFIF_TH_MIN_ENSET		BIT(0)
#define HDMI_RX_AUD_FIFO_ISTS		(HDMI_RX_BASE + 0x0fb0)
#define AFIF_OVERFL_ISTS		BIT(4)
#define AFIF_UNDERFL_ISTS		BIT(3)
#define AFIF_THS_PASS_ISTS		BIT(2)
#define AFIF_TH_MAX_ISTS		BIT(1)
#define AFIF_TH_MIN_ISTS		BIT(0)
#define HDMI_RX_AUD_FIFO_IEN		(HDMI_RX_BASE + 0x0fb4)
#define HDMI_RX_AUD_FIFO_ICLR		(HDMI_RX_BASE + 0x0fb8)
#define HDMI_RX_MD_IEN_CLR		(HDMI_RX_BASE + 0x0fc0)
#define HDMI_RX_MD_IEN_SET		(HDMI_RX_BASE + 0x0fc4)
#define VACT_LIN_ENSET			BIT(9)
#define VS_CLK_ENSET			BIT(8)
#define VTOT_CLK_ENSET			BIT(7)
#define HACT_PIX_ENSET			BIT(6)
#define HS_CLK_ENSET			BIT(5)
#define DE_ACTIVITY_ENSET		BIT(2)
#define VS_ACT_ENSET			BIT(1)
#define HS_ACT_ENSET			BIT(0)
#define HDMI_RX_MD_ISTS			(HDMI_RX_BASE + 0x0fc8)
#define VACT_LIN_ISTS			BIT(9)
#define VS_CLK_ISTS			BIT(8)
#define VTOT_CLK_ISTS			BIT(7)
#define HACT_PIX_ISTS			BIT(6)
#define HS_CLK_ISTS			BIT(5)
#define DE_ACTIVITY_ISTS		BIT(2)
#define VS_ACT_ISTS			BIT(1)
#define HS_ACT_ISTS			BIT(0)
#define HDMI_RX_MD_IEN			(HDMI_RX_BASE + 0x0fcc)
#define HDMI_RX_MD_ICLR			(HDMI_RX_BASE + 0x0fd0)
#define HDMI_RX_MD_ISET			(HDMI_RX_BASE + 0x0fd4)
#define HDMI_RX_HDMI_IEN_CLR		(HDMI_RX_BASE + 0x0fd8)
#define CLK_CHANGE_ENCLR		BIT(6)
#define HDMI_RX_HDMI_IEN_SET		(HDMI_RX_BASE + 0x0fdc)
#define CLK_CHANGE_ENSET		BIT(6)
#define HDMI_RX_HDMI_ISTS		(HDMI_RX_BASE + 0x0fe0)
#define CLK_CHANGE_ISTS			BIT(6)
#define HDMI_RX_HDMI_IEN		(HDMI_RX_BASE + 0x0fe4)
#define HDMI_RX_HDMI_ICLR		(HDMI_RX_BASE + 0x0fe8)
#define HDMI_RX_HDMI_ISET		(HDMI_RX_BASE + 0x0fec)
#define CLK_CHANGE_CLR			BIT(6)
#define HDCP_DKSET_DONE_ISTS_MASK	BIT(31)
#define HDMI_RX_DMI_SW_RST		(HDMI_RX_BASE + 0x0ff0)
#define HDMI_RX_DMI_DISABLE_IF		(HDMI_RX_BASE + 0x0ff4)
#define VID_ENABLE(x)			UPDATE(x, 7, 7)
#define VID_ENABLE_MASK			BIT(7)
#define CEC_ENABLE(x)			UPDATE(x, 5, 5)
#define CEC_ENABLE_MASK			BIT(5)
#define AUD_ENABLE(x)			UPDATE(x, 4, 4)
#define AUD_ENABLE_MASK			BIT(4)
#define HDMI_ENABLE(x)			UPDATE(x, 2, 2)
#define HDMI_ENABLE_MASK		BIT(2)

#define HDMI_RX_CEC_CTRL		(HDMI_RX_BASE + 0x1f00)
#define CEC_CTRL_FRAME_TYP		(3 << 1)
#define CEC_CTRL_IMMED			(2 << 1)
#define CEC_CTRL_NORMAL			(1 << 1)
#define CEC_CTRL_RETRY			(0 << 1)
#define CEC_SEND			BIT(0)
#define HDMI_RX_CEC_MASK		(HDMI_RX_BASE + 0x1f08)
#define HDMI_RX_CEC_ADDR_L		(HDMI_RX_BASE + 0x1f14)
#define HDMI_RX_CEC_ADDR_H		(HDMI_RX_BASE + 0x1f18)
#define HDMI_RX_CEC_TX_CNT		(HDMI_RX_BASE + 0x1f1c)
#define HDMI_RX_CEC_RX_CNT		(HDMI_RX_BASE + 0x1f20)
#define HDMI_RX_CEC_TX_DATA_0		(HDMI_RX_BASE + 0x1f40)
#define HDMI_RX_CEC_RX_DATA_0		(HDMI_RX_BASE + 0x1f80)
#define HDMI_RX_CEC_LOCK		(HDMI_RX_BASE + 0x1fc0)
#define HDMI_RX_CEC_WAKEUPCTRL		(HDMI_RX_BASE + 0x1fc4)

#define HDMI_RX_IVECTOR_INDEX_CB	(HDMI_RX_BASE + 0x32e4)
#define HDMI_RX_MAX_REGISTER		HDMI_RX_IVECTOR_INDEX_CB

#define HDCP_KEY_KSV_SIZE		8
#define HDCP_PRIVATE_KEY_SIZE		280
#define HDCP_KEY_SHA_SIZE		20
#define HDCP_KEY_SIZE			308
#define HDCP_KEY_SEED_SIZE		2
#define KSV_LEN				5

#define HDMIRX_HDCP1X_ID		13

#define HDMIRX_GET_TIMING_CNT		20
#define HDMIRX_MODETCLK_CNT_NUM		1000
#define HDMIRX_MODETCLK_HZ		(CPLL_REF_CLK / 24)

#define EDID_NUM_BLOCKS_MAX		2
#define EDID_BLOCK_SIZE			128

#define RK628_CSI_LINK_FREQ_LOW		350000000
#define RK628_CSI_LINK_FREQ_HIGH	650000000
#define RK628_CSI_LINK_FREQ_925M	925000000
#define RK628_CSI_PIXEL_RATE_LOW	400000000
#define RK628_CSI_PIXEL_RATE_HIGH	600000000
#define MIPI_DATARATE_MBPS_LOW		700
#define MIPI_DATARATE_MBPS_HIGH		1300

#define POLL_INTERVAL_MS		1000
#define RXPHY_CFG_MAX_TIMES		5
#define CSITX_ERR_RETRY_TIMES		3

#define USE_4_LANES			4
#define YUV422_8BIT			0x1e

#define SCDC_CED_ERR_CNT		0xfff

enum bus_format {
	BUS_FMT_RGB = 0,
	BUS_FMT_YUV422 = 1,
	BUS_FMT_YUV444 = 2,
	BUS_FMT_YUV420 = 3,
	BUS_FMT_UNKNOWN,
};

enum lock_status {
	LOCK_OK = 0,
	LOCK_FAIL = 1,
	LOCK_RESET = 2,
};

struct hdcp_keys {
	u8 KSV[HDCP_KEY_KSV_SIZE];
	u8 devicekey[HDCP_PRIVATE_KEY_SIZE];
	u8 sha[HDCP_KEY_SHA_SIZE];
};

struct rk628_hdcp {
	char *seeds;
	struct hdcp_keys *keys;
	struct rk628 *rk628;
	int enable;
	bool hdcp_start;
};

struct rk628_hdmirx_cec {
	struct device *dev;
	struct rk628 *rk628;
	u32 addresses;
	struct cec_adapter *adap;
	struct cec_msg rx_msg;
	unsigned int tx_status;
	bool tx_done;
	bool rx_done;
	bool cec_hpd;
	struct cec_notifier *notify;
	struct delayed_work delayed_work_cec;
};

void rk628_hdmirx_set_hdcp(struct rk628 *rk628, struct rk628_hdcp *hdcp, bool en);
void rk628_hdmirx_controller_setup(struct rk628 *rk628);

typedef void *HAUDINFO;
typedef void (*rk628_audio_info_cb)(struct rk628 *rk628, bool on);
HAUDINFO rk628_hdmirx_audioinfo_alloc(struct device *dev,
				      struct mutex *confctl_mutex,
				      struct rk628 *rk628,
				      bool en,
				      rk628_audio_info_cb info_cb);
void rk628_hdmirx_audio_destroy(HAUDINFO info);
void rk628_hdmirx_audio_setup(HAUDINFO info);
void rk628_hdmirx_audio_cancel_work_audio(HAUDINFO info, bool sync);
void rk628_hdmirx_audio_cancel_work_rate_change(HAUDINFO info, bool sync);
bool rk628_hdmirx_audio_present(HAUDINFO info);
int  rk628_hdmirx_audio_fs(HAUDINFO info);
void rk628_hdmirx_audio_i2s_ctrl(HAUDINFO info, bool enable);
bool rk628_hdmirx_get_arc_enable(HAUDINFO info);
int rk628_hdmirx_set_arc_enable(HAUDINFO info, bool enabled);
void rk628_hdmirx_audio_handle_plugged_change(HAUDINFO info, bool plugged);

/* for audio isr process */
bool rk628_audio_fifoints_enabled(HAUDINFO info);
bool rk628_audio_ctsnints_enabled(HAUDINFO info);
void rk628_csi_isr_ctsn(HAUDINFO info, u32 pdec_ints);
void rk628_csi_isr_fifoints(HAUDINFO info, u32 fifo_ints);
int rk628_is_avi_ready(struct rk628 *rk628, bool avi_rcv_rdy);
void rk628_hdmirx_verisyno_phy_power_on(struct rk628 *rk628);
void rk628_hdmirx_verisyno_phy_power_off(struct rk628 *rk628);
void rk628_hdmirx_phy_prepclk_cfg(struct rk628 *rk628);
int rk628_hdmirx_verisyno_phy_init(struct rk628 *rk628);
u8 rk628_hdmirx_get_format(struct rk628 *rk628);
void rk628_set_bg_enable(struct rk628 *rk628, bool en);
bool rk628_hdmirx_tx_5v_power_detect(struct gpio_desc *det_gpio);
u32 rk628_hdmirx_get_tmdsclk_cnt(struct rk628 *rk628);
int rk628_hdmirx_get_timings(struct rk628 *rk628,
			     struct v4l2_dv_timings *timings);
u8 rk628_hdmirx_get_range(struct rk628 *rk628);
u8 rk628_hdmirx_get_color_space(struct rk628 *rk628);
int rk628_hdmirx_get_hdcp_enc_status(struct rk628 *rk628);
void rk628_hdmirx_controller_reset(struct rk628 *rk628);
bool rk628_hdmirx_scdc_ced_err(struct rk628 *rk628);
bool rk628_hdmirx_is_signal_change_ists(struct rk628 *rk628, u32 md_ints, u32 pdec_ints);

void rk628_hdmirx_cec_irq(struct rk628 *rk628, struct rk628_hdmirx_cec *cec);
struct rk628_hdmirx_cec *rk628_hdmirx_cec_register(struct rk628 *rk628);
void rk628_hdmirx_cec_unregister(struct rk628_hdmirx_cec *cec);
void rk628_hdmirx_cec_hpd(struct rk628_hdmirx_cec *cec, bool en);
void rk628_hdmirx_cec_state_reconfiguration(struct rk628 *rk628,
					    struct rk628_hdmirx_cec *cec);
void rk628_hdmirx_phy_debugfs_register_create(struct rk628 *rk628, struct dentry *dir);
void rk628_hdmirx_debugfs_create(struct rk628 *rk628, struct rk628_hdcp *hdcp);
#endif
