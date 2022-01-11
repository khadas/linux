/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_RX_HW_H__
#define __HDMI_RX_HW_H__

#define K_TEST_CHK_ERR_CNT

/**
 * Bit field mask
 * @param m	width
 * @param n shift
 */
#define MSK(m, n)		(((1  <<  (m)) - 1)  <<  (n))
/**
 * Bit mask
 * @param n shift
 */
#define _BIT(n)			MSK(1, (n))

#define HHI_GCLK_MPEG0			(0x50  <<  2) /* (0xC883C000 + 0x140) */
#define HHI_HDMIRX_CLK_CNTL		0x200 /* (0xC883C000 + 0x200)  */
#define HHI_HDMIRX_AUD_CLK_CNTL	0x204 /* 0x1081 */
#define HHI_AXI_CLK_CTNL		(0xb8 * 4)
#define HHI_VDAC_CNTL0			(0xbb * 4)
#define HHI_VDAC_CNTL1			(0xbc * 4)
#define HHI_AUD_PLL_CNTL		(0xf8 * 4)
#define HHI_AUD_PLL_CNTL2		(0xf9 * 4)
#define HHI_AUD_PLL_CNTL3		(0xfa * 4)
#define HHI_AUD_PLL_CNTL4		(0xfb * 4)
#define HHI_AUD_PLL_CNTL5		(0xfc * 4)
#define HHI_AUD_PLL_CNTL6		(0xfd * 4)
#define HHI_AUD_PLL_CNTL_I		(0xfe * 4) /* audio pll lock bit31 */
#define HHI_ADC_PLL_CNTL4		(0xad * 4)
#define HHI_HDCP22_CLK_CNTL		(0x7c * 4)
#define HHI_GCLK_MPEG2			(0x52 * 4)

/* T7 0xfe008280*/
#define HHI_AUD_PLL_CNTL_T7		(0x20 * 4)
#define HHI_AUD_PLL_CNTL2_T7		(0x21 * 4)
#define HHI_AUD_PLL_CNTL3_T7		(0x22 * 4)
#define HHI_AUD_PLL_CNTL_I_T7		(0x23 * 4)
#define HHI_AUD_PLL4X_CNTL_T7		(0x24 * 4)

/* T3 move aud pll to analog modules */
#define ANACTL_AUD_PLL_CNTL		(0xa0 * 4)
#define ANACTL_AUD_PLL_CNTL2	(0xa1 * 4)
#define ANACTL_AUD_PLL_CNTL3	(0xa2 * 4)
#define ANACTL_AUD_PLL_STS		(0xa3 * 4)
#define ANACTL_AUD_PLL4X_CNTL		(0xa4 * 4)

/* TXLX */
/* unified_register.h by wujun */
#define HHI_AUDPLL_CLK_OUT_CNTL (0x8c  <<  2)
#define HHI_VDAC_CNTL0_TXLX		(0xBD * 4)
#define PREG_PAD_GPIO0_EN_N		(0x0c * 4)
#define PREG_PAD_GPIO0_O		(0x0d * 4)
#define PREG_PAD_GPIO0_I		(0x0e * 4)
#define PERIPHS_PIN_MUX_6		(0x32 * 4)
#define PERIPHS_PIN_MUX_10		(0x36 * 4)
#define PERIPHS_PIN_MUX_11		(0x37 * 4)
#define PAD_PULL_UP_REG2		(0x3c * 4)

#define AUD_RESAMPLE_CTRL0		0x28bf

/** PHY Gen3 Clock measurement lock threshold - default 8*/
#define LOCK_THRES  0x3f	/* 0x08 */
/** register address: PHY Gen3 clock measurement unit configuration */
#define PHY_CMU_CONFIG			(0x02UL)
/** register address: PHY Gen3 system configuration */
#define PHY_SYSTEM_CONFIG		(0x03UL)
#define PHY_MAINFSM_CTL			(0x05UL)
/** register address: PHY Gen3 main FSM status 1 */
#define PHY_MAINFSM_STATUS1	   (0x09UL)

#define PHY_RESISTOR_CALIBRATION_1 (0x10UL)
#define PHY_MAIN_FSM_OVERRIDE1	(0x07UL)
#define PHY_TERM_OVERRIDE _BIT(8)
#define PHY_TERM_OV_VALUE MSK(4, 4)
#define PHY_TERM_OV_PORT0 _BIT(4)
#define PHY_TERM_OV_PORT1 _BIT(5)
#define PHY_TERM_OV_PORT2 _BIT(6)
#define PHY_TERM_OV_PORT3 _BIT(7)
#define PHY_MAIN_FSM_OVERRIDE2	(0x08UL)

#define PHY_MAIN_BIST_CONTROL	(0x0BUL)
#define PHY_MAIN_BIST_RESULTS	(0x0CUL)

#define PHY_CORESTATUS_CH0		(0x30UL)
#define PHY_CORESTATUS_CH1		(0x50UL)
#define PHY_CORESTATUS_CH2		(0x70UL)

#define PHY_EQCTRL1_CH0			(0x32UL)
#define PHY_EQCTRL1_CH1			(0x52UL)
#define PHY_EQCTRL1_CH2			(0x72UL)

#define PHY_EQLSTATS_CH0		(0x34UL)
#define PHY_EQLSTATS_CH1		(0x54UL)
#define PHY_EQLSTATS_CH2		(0x74UL)

#define PHY_CH0_EQ_CTRL3		(0x3EUL)
#define PHY_CH1_EQ_CTRL3		(0x5EUL)
#define PHY_CH2_EQ_CTRL3		(0x7EUL)

#define PHY_EQCTRL4_CH0			(0x3FUL)
#define PHY_EQCTRL4_CH1			(0x5FUL)
#define PHY_EQCTRL4_CH2			(0x7FUL)

#define PHY_EQCTRL2_CH0			(0x33UL)
#define PHY_EQCTRL2_CH1			(0x53UL)
#define PHY_EQCTRL2_CH2			(0x73UL)

#define PHY_EQLCKST2_CH0		(0x40UL)
#define PHY_EQLCKST2_CH1		(0x60UL)
#define PHY_EQLCKST2_CH2		(0x80UL)

#define PHY_EQSTAT3_CH0			(0x42UL)
#define PHY_EQSTAT3_CH1			(0x62UL)
#define PHY_EQSTAT3_CH2			(0x82UL)

#define PHY_EQCTRL6_CH0		(0x43UL)
#define PHY_EQCTRL6_CH1		(0x63UL)
#define PHY_EQCTRL6_CH2		(0x83UL)

#define OVL_PROT_CTRL				(0x0DUL)
#define PHY_CDR_CTRL_CNT			(0x0EUL)
	#define CLK_RATE_BIT		_BIT(8)
#define PHY_VOLTAGE_LEVEL			(0x22UL)
#define PHY_MPLL_CTRL				(0x24UL)
#define MPLL_DIVIDER_CONTROL		(0x25UL)
#define MPLL_PARAMETERS2                (0x27UL)
#define MPLL_PARAMETERS3                (0x28UL)
#define MPLL_PARAMETERS4                (0x29UL)
#define MPLL_PARAMETERS5                (0x2AUL)
#define MPLL_PARAMETERS6                (0x2BUL)
#define MPLL_PARAMETERS7                (0x2CUL)
#define MPLL_PARAMETERS8                (0x2DUL)
#define MPLL_PARAMETERS9                (0x2EUL)
#define MPLL_PARAMETERS10                (0xC0UL)
#define MPLL_PARAMETERS11                (0xC1UL)
#define MPLL_PARAMETERS12                (0xC2UL)
#define MPLL_PARAMETERS13                (0xC3UL)
#define MPLL_PARAMETERS14                (0xC4UL)
#define MPLL_PARAMETERS15                (0xC5UL)
#define MPLL_PARAMETERS16                (0xC6UL)
#define MPLL_PARAMETERS17               (0xC7UL)
#define MPLL_PARAMETERS18                (0xC8UL)
#define MPLL_PARAMETERS19                (0xC9UL)
#define MPLL_PARAMETERS20                (0xCAUL)
#define MPLL_PARAMETERS21                (0xCBUL)
#define MPLL_PARAMETERS22                (0xCCUL)
#define MPLL_PARAMETERS23                (0xCDUL)

/* ------------------------------------- */
/* TOP-level wrapper registers addresses */
/* ------------------------------------- */

#define TOP_SW_RESET                     0x000
	#define HDCP22_TMDSCLK_EN		_BIT(3)
#define TOP_CLK_CNTL                     0x001
#define TOP_HPD_PWR5V                    0x002
#define TOP_PORT_SEL                     0x003
#define TOP_EDID_GEN_CNTL                0x004
#define TOP_EDID_ADDR_CEC                0x005
#define TOP_EDID_DATA_CEC_PORT01         0x006
#define TOP_EDID_DATA_CEC_PORT23         0x007
#define TOP_EDID_GEN_STAT                0x008
#define TOP_INTR_MASKN                   0x009
#define TOP_INTR_STAT                    0x00A
#define TOP_INTR_STAT_CLR                0x00B
#define TOP_VID_CNTL                     0x00C
#define TOP_VID_STAT                     0x00D
#define TOP_ACR_CNTL_STAT                0x00E
#define TOP_ACR_AUDFIFO                  0x00F
#define TOP_ARCTX_CNTL                   0x010
#define TOP_METER_HDMI_CNTL              0x011
#define TOP_METER_HDMI_STAT              0x012
#define TOP_VID_CNTL2                    0x013

/* hdmi2.0 new start */
#define TOP_MEM_PD  0x014
#define TOP_EDID_RAM_OVR0                0x015
#define TOP_EDID_RAM_OVR0_DATA           0x016
#define TOP_EDID_RAM_OVR1                0x017
#define TOP_EDID_RAM_OVR1_DATA           0x018
#define TOP_EDID_RAM_OVR2                0x019
#define TOP_EDID_RAM_OVR2_DATA           0x01a
#define TOP_EDID_RAM_OVR3                0x01b
#define TOP_EDID_RAM_OVR3_DATA           0x01c
#define TOP_EDID_RAM_OVR4                0x01d
#define TOP_EDID_RAM_OVR4_DATA           0x01e
#define TOP_EDID_RAM_OVR5                0x01f
#define TOP_EDID_RAM_OVR5_DATA           0x020
#define TOP_EDID_RAM_OVR6                0x021
#define TOP_EDID_RAM_OVR6_DATA           0x022
#define TOP_EDID_RAM_OVR7                0x023
#define TOP_EDID_RAM_OVR7_DATA           0x024
#define TOP_EDID_GEN_STAT_B              0x025
#define TOP_EDID_GEN_STAT_C              0x026
#define TOP_EDID_GEN_STAT_D              0x027
/* tl1 */
#define TOP_CHAN_SWITCH_0				0x028
#define TOP_TMDS_ALIGN_CNTL0			0x029
#define TOP_TMDS_ALIGN_CNTL1			0x02a
#define TOP_TMDS_ALIGN_STAT				0x02b

/* GXTVBB/TXL/TXLX */
#define	TOP_ACR_CNTL2					 0x02a
/* Gxtvbb */
#define	TOP_INFILTER_GXTVBB				 0x02b
/* Gxtvbb */

#define	TOP_INFILTER_HDCP				 0x02C
#define	TOP_INFILTER_I2C0				 0x02D
#define	TOP_INFILTER_I2C1				 0x02E
#define	TOP_INFILTER_I2C2				 0x02F
#define	TOP_INFILTER_I2C3				 0x030
/* tl1 */
#define	TOP_PRBS_GEN					0x033
#define	TOP_PRBS_ANA_0					0x034
#define TOP_PRBS_ANA_1					0x035
#define	TOP_PRBS_ANA_STAT				0x036
#define	TOP_PRBS_ANA_BER_CH0			0x037
#define	TOP_PRBS_ANA_BER_CH1			0x038
#define	TOP_PRBS_ANA_BER_CH2			0x039
#define	TOP_METER_CABLE_CNTL			0x03a
#define	TOP_METER_CABLE_STAT			0x03b
#define	TOP_CHAN_SWITCH_1				0x03c
/* tl1 */
#define	TOP_AUDPLL_LOCK_FILTER			0x040

/* tl1 */
#define	TOP_CHAN01_ERRCNT				0x041
#define	TOP_CHAN2_ERRCNT				0x042
#define	TOP_TL1_ACR_CNTL2				0x043
#define	TOP_ACR_N_STAT					0x044
#define	TOP_ACR_CTS_STAT				0x045
#define	TOP_AUDMEAS_CTRL				0x046
#define	TOP_AUDMEAS_CYCLES_M1			0x047
#define	TOP_AUDMEAS_INTR_MASKN			0x048
#define	TOP_AUDMEAS_INTR_STAT			0x049
#define	TOP_AUDMEAS_REF_CYCLES_STAT0	0x04a
#define	TOP_AUDMEAS_REF_CYCLES_STAT1	0x04b
#define TOP_SHIFT_PTTN_012				0x050
#define	TOP_HDCP22_BSOD					0x060

#define	TOP_SKP_CNTL_STAT				0x061
#define	TOP_NONCE_0						0x062
#define	TOP_NONCE_1						0x063
#define	TOP_NONCE_2						0x064
#define	TOP_NONCE_3						0x065
#define	TOP_PKF_0						0x066
#define	TOP_PKF_1						0x067
#define	TOP_PKF_2						0x068
#define	TOP_PKF_3						0x069
#define	TOP_DUK_0						0x06a
#define	TOP_DUK_1						0x06b
#define	TOP_DUK_2						0x06c
#define	TOP_DUK_3						0x06d
#define TOP_NSEC_SCRATCH				0x06e
#define	TOP_SEC_SCRATCH					0x06f
#define TOP_EDID_OFFSET					0x200

/* TL1 */
#define	TOP_EMP_DDR_START_A				0x070
#define	TOP_EMP_DDR_START_B				0x071
#define	TOP_EMP_DDR_FILTER				0x072
#define	TOP_EMP_CNTL_0					0x073
#define	TOP_EMP_CNTL_1					0x074
#define	TOP_EMP_CNTMAX					0x075
#define	TOP_EMP_ERR_STAT				0x076
#define	TOP_EMP_RCV_CNT_CUR				0x077
#define	TOP_EMP_RCV_CNT_BUF				0x078
#define	TOP_EMP_DDR_ADDR_CUR			0x079
#define	TOP_EMP_DDR_PTR_S_BUF			0x07a
#define	TOP_EMP_STAT_0					0x07b
#define	TOP_EMP_STAT_1					0x07c
/* SYPS */
#define TOP_AXI_CNTL_0					0x080
#define	TOP_AXI_ASYNC_HOLD_ESM			0x081
#define	TOP_AXI_ASYNC_HOLD_EMP			0x082
/* COR */
#define TOP_PHYIF_CNTL0					0x080

#define	TOP_AXI_STAT_0					0x083
#define	TOP_MISC_STAT0					0x084
#define TOP_OVID_OVERRIDE0				0x090
#define TOP_OVID_OVERRIDE1				0x091

#define TOP_EDID_ADDR_S					0x1000
#define TOP_EDID_ADDR_E					0x11ff

#define TOP_SECURE_INDEX                 0x0a0  /* 0x280 */
#define TOP_SECURE_DATA                  0x0a1  /* 0x284 */
#define TOP_SECURE_MODE                  0x0a2  /* 0x288 */

/* TM2 */
#define TOP_EDID_PORT2_ADDR_S			0x1200
#define TOP_EDID_PORT2_ADDR_E			0x13ff
#define TOP_EDID_PORT3_ADDR_S			0x1400
#define TOP_EDID_PORT3_ADDR_E			0x15ff

/* TL1/TM2/T5/T5D */
#define TOP_DWC_BASE_OFFSET				0x8000
/* T5/T5D */
#define TOP_AMLPHY_BASE_OFFSET_T5		0xc000
/* t7 top base addr */
#define TOP_AMLPHY_BASE_OFFSET_T7		0x4000
#define TOP_COR_BASE_OFFSET_T7			0x8000

#define TOP_DONT_TOUCH0                  0x0fe
#define TOP_DONT_TOUCH1                  0x0ff

/*
 * HDMI registers
 */
/** Register address: setup control */
#define DWC_HDMI_SETUP_CTRL      (0x000UL)
/** Hot plug detect signaled */
#define		HOT_PLUG_DETECT			_BIT(0)
/** Register address: override control */
#define DWC_HDMI_OVR_CTRL        (0x004UL)
/** Register address: timer control */
#define DWC_HDMI_TIMEDWC_CTRL     (0x008UL)
/** Register address: resistor override */
#define DWC_HDMI_RES_OVR         (0x010UL)
/** Register address: resistor status */
#define DWC_HDMI_RES_STS         (0x014UL)
/** Register address: TMDS PLL control */
#define DWC_HDMI_PLL_CTRL        (0x018UL)
/** Register address: TMDS PLL frequency range */
#define DWC_HDMI_PLL_FRQSET1     (0x01CUL)
/** Register address: TMDS PLL frequency range */
#define DWC_HDMI_PLL_FRQSET2     (0x020UL)
/** Register address: TMDS PLL PCP and ICP range */
#define DWC_HDMI_PLL_PAR1        (0x024UL)
/** Register address: TMDS PLL PCP and ICP range */
#define DWC_HDMI_PLL_PAR2        (0x028UL)
/** Register address: TMDS PLL KOSC and CCOLF range */
#define DWC_HDMI_PLL_PAR3        (0x02CUL)
/** Register address: PLL post lock filter */
#define DWC_HDMI_PLL_LCK_STS     (0x030UL)
/** Register address: PLL clock control */
#define DWC_HDMI_CLK_CTRL        (0x034UL)
/** Register address: PCB diversity control */
#define DWC_HDMI_PCB_CTRL        (0x038UL)
/** Input selector */
#define		INPUT_SELECT			_BIT(16)
/** Register address: phase control */
#define DWC_HDMI_PHS_CTRL        (0x040UL)
/** Register address: used phases */
#define DWC_HDMI_PHS_USD         (0x044UL)
/** Register address: miscellaneous operations control */
#define DWC_HDMI_MISC_CTRL       (0x048UL)
/** Register address: EQ offset calibration */
#define DWC_HDMI_EQOFF_CTRL      (0x04CUL)
/** Register address: EQ gain control */
#define DWC_HDMI_EQGAIN_CTRL     (0x050UL)
/** Register address: EQ status */
#define DWC_HDMI_EQCAL_STS       (0x054UL)
/** Register address: EQ results */
#define DWC_HDMI_EQRESULT        (0x058UL)
/** Register address: EQ measurement control */
#define DWC_HDMI_EQ_MEAS_CTRL    (0x05CUL)
/** Register address: HDMI mode recover */
#define DWC_HDMI_MODE_RECOVER    (0x080UL)
/** Register address: HDMI error protection */
#define DWC_HDMI_ERROR_PROTECT  (0x084UL)
/** Register address: validation and production test */
#define DWC_HDMI_ERD_STS         (0x088UL)
/** Register address: video output sync signals control */
#define DWC_HDMI_SYNC_CTRL       (0x090UL)
/** VS polarity adjustment */
#define		VS_POL_ADJ_MODE			MSK(2, 3)
/** HS polarity adjustment automatic */
#define		VS_POL_ADJ_AUTO			(2)
/** HS polarity adjustment */
#define		HS_POL_ADJ_MODE			MSK(2, 1)
/** HS polarity adjustment automatic inversion */
#define		HS_POL_ADJ_AUTO			(2)
/** Register address: clock measurement */
#define DWC_HDMI_CKM_EVLTM       (0x094UL)
/** Evaluation period */
#define		EVAL_TIME				MSK(12, 4)
/** active wait period for TMDS stabilisation */
#define		TMDS_STABLE_TIMEOUT			(30)
/** Register address: legal clock count */
#define DWC_HDMI_CKM_F           (0x098UL)
/** Maximum count for legal count */
#define		CKM_MAXFREQ					MSK(16, 16)
/** Minimum count for legal count */
#define		MINFREQ					MSK(16, 0)
/** Register address: measured clock results */
#define DWC_HDMI_CKM_RESULT      (0x09CUL)
/** Measured clock is stable */
#define		CLOCK_IN_RANGE			_BIT(17)
/** Measured clock rate in bits */
#define		CLKRATE					MSK(16, 0)
/** Register address: sub-sampling control */
#define DWC_HDMI_RESMPL_CTRL     (0x0A4UL)
/** Register address: deep color mode control */
#define DWC_HDMI_DCM_CTRL        (0x0A8UL)
/** Register address: video output mute configuration */
#define DWC_HDMI_VM_CFG_CH_0_1   (0x0B0UL)
/** Register address: video output mute configuration */
#define DWC_HDMI_VM_CFG_CH2      (0x0B4UL)
/** Register address: spare */
#define DWC_HDMI_SPARE           (0x0B8UL)
/** Register address: HDMI status */
#define DWC_HDMI_STS             (0x0BCUL)
/** Current deep color mode */
#define		DCM_CURRENT_MODE		MSK(4, 28)
/** Deep color mode, 24 bit */
#define		DCM_CURRENT_MODE_24b	4
/** Deep color mode, 30 bit */
#define		DCM_CURRENT_MODE_30b	5
/** Deep color mode, 36 bit */
#define		DCM_CURRENT_MODE_36b	6
/** Deep color mode, 48 bit */
#define		DCM_CURRENT_MODE_48b	7

/* HDMI 2.0 feature registers */
/* bit0-1  scramble ctrl */
#define DWC_HDMI20_CONTROL				0x0800
#define DWC_SCDC_I2CCONFIG				0x0804
#define DWC_SCDC_CONFIG					0x0808
#define DWC_CHLOCK_CONFIG				0x080C
#define DWC_HDCP22_CONTROL				0x081C
#define DWC_SCDC_REGS0                  0x0820
#define DWC_SCDC_REGS1                  0x0824
#define DWC_SCDC_REGS2                  0x0828
#define DWC_SCDC_REGS3                  0x082C
#define DWC_SCDC_MANSPEC0               0x0840
#define DWC_SCDC_MANSPEC1               0x0844
#define DWC_SCDC_MANSPEC2               0x0848
#define DWC_SCDC_MANSPEC3               0x084C
#define DWC_SCDC_MANSPEC4               0x0850
#define DWC_SCDC_WRDATA0                0x0860
#define DWC_SCDC_WRDATA1                0x0864
#define DWC_SCDC_WRDATA2                0x0868
#define DWC_SCDC_WRDATA3                0x086C
#define DWC_SCDC_WRDATA4                0x0870
#define DWC_SCDC_WRDATA5                0x0874
#define DWC_SCDC_WRDATA6                0x0878
#define DWC_SCDC_WRDATA7                0x087C
#define DWC_HDMI20_STATUS               0x08E0
#define DWC_HDCP22_STATUS               0x08FC
#define HDCP_DECRYPTED	_BIT(0)
#define HDCP22_CAPABLE	_BIT(16)
#define HDCP22_NOT_CAPABLE	_BIT(17)
#define HDCP22_AUTH_LOST	_BIT(18)
#define HDCP22_AUTH_PASS	_BIT(19)
#define HDCP22_AUTH_FAIL	_BIT(20)

#define DWC_PDEC_DRM_HB				     0x4c0
#define DWC_PDEC_DRM_PAYLOAD0			 0x4c4
#define DWC_PDEC_DRM_PAYLOAD1			 0x4c8
#define DWC_PDEC_DRM_PAYLOAD2			 0x4cc
#define DWC_PDEC_DRM_PAYLOAD3			 0x4d0
#define DWC_PDEC_DRM_PAYLOAD4			 0x4d4
#define DWC_PDEC_DRM_PAYLOAD5			 0x4d8
#define DWC_PDEC_DRM_PAYLOAD6			 0x4dc

/*
 * hdcp register
 */
/** Register address: HDMI status */
#define DWC_HDCP_DBG             (0x0E0UL)
/*
 * Video Mode registers
 */
/** Register address: video mode control */
#define DWC_MD_HCTRL1            (0x140UL)
/** Register address: video mode control */
#define DWC_MD_HCTRL2            (0x144UL)
/** Register address: horizontal sync */
#define DWC_MD_HT0               (0x148UL)
/** Register address: horizontal offset */
#define DWC_MD_HT1               (0x14CUL)
/** Horizontal total length */
#define		HTOT_PIX				MSK(16, 16)
/** Horizontal offset length */
#define		HOFS_PIX				MSK(16, 0)
/** Register address: horizontal active length */
#define DWC_MD_HACT_PX           (0x150UL)
/** Horizontal active length */
#define		HACT_PIX				MSK(16, 0)
/** Register address: horizontal active time */
#define DWC_MD_HACT_PXA          (0x154UL)
/** Register address: vertical control */
#define DWC_MD_VCTRL             (0x158UL)
/** Register address: vertical timing - sync pulse duration */
#define DWC_MD_VSC               (0x15CUL)
/** Register address: vertical timing - frame duration */
#define DWC_MD_VTC               (0x160UL)
/** Frame duration */
#define		VTOT_CLK				(~0)
/** Register address: vertical offset length */
#define DWC_MD_VOL               (0x164UL)
/** Vertical offset length */
#define		VOFS_LIN				MSK(16, 0)
/** Register address: vertical active length */
#define DWC_MD_VAL               (0x168UL)
/** Vertical active length */
#define		VACT_LIN				MSK(16, 0)
/** Register address: vertical timing */
#define DWC_MD_VTH               (0x16CUL)
/** Register address: vertical total length */
#define DWC_MD_VTL               (0x170UL)
/** Vertical total length */
#define		VTOT_LIN				MSK(16, 0)
/** Register address: skew measurement trigger */
#define DWC_MD_IL_CTRL           (0x174UL)
/** Register address: VS and HS skew */
#define DWC_MD_IL_SKEW           (0x178UL)
/** Register address: V&H skew and filed detection */
#define DWC_MD_IL_POL            (0x17CUL)
/** Register address: video mode status */
#define DWC_MD_STS               (0x180UL)
/** Interlace active status */
#define		ILACE					_BIT(3)
/*
 * Audio registers
 */
/** Register address: audio mode control */
#define DWC_AUD_CTRL             (0x200UL)
#define DWC_AUD_HBR_ENABLE	_BIT(8)
/** Register address: audio PLL control */
#define DWC_AUD_PLL_CTRL         (0x208UL)
/** Register address: audio PLL lock */
/* #define DWC_AUD_PLL_LOCK         (0x20CUL) */
/** Register address: DDS audio clock control */
/* #define DWC_AUD_PLL_RESET        (0x210UL) */
/** Register address: audio clock control */
#define DWC_AUD_CLK_CTRL         (0x214UL)
/** Register address: ASP sync intervals */
#define DWC_AUD_CLK_MASP         (0x218UL)
/** Register address: audio sync interval */
#define DWC_AUD_CLK_MAUD         (0x21CUL)
/** Register address: sync interval reset */
#define DWC_AUD_FILT_CTRL1       (0x220UL)
/** Register address: phase filter control */
#define DWC_AUD_FILT_CTRL2       (0x224UL)
/** Register address: manual CTS control */
#define DWC_AUD_CTS_MAN          (0x228UL)
/** Register address: manual N control */
#define DWC_AUD_N_MAN            (0x22CUL)
/** Register address: audio clock status */
#define DWC_AUD_CLK_STS          (0x23CUL)
/** Register address: audio FIFO control */
#define DWC_AUD_FIFO_CTRL        (0x240UL)
/** Audio FIFO reset */
#define AFIF_INIT                 _BIT(0)
#define AFIF_SUBPACKETS           _BIT(16)
/** Register address: audio FIFO threshold */
#define DWC_AUD_FIFO_TH          (0x244UL)
/** Register address: audio FIFO fill */
#define DWC_AUD_FIFO_FILL_S      (0x248UL)
/** Register address: audio FIFO fill minimum/maximum */
#define DWC_AUD_FIFO_CLDWC_MM     (0x24CUL)
/** Register address: audio FIFO fill status */
#define DWC_AUD_FIFO_FILLSTS     (0x250UL)
/** Register address: audio output interface configuration */
#define DWC_AUD_CHEXTR_CTRL     (0x254UL)
#define AUD_CH_MAP_CFG MSK(5, 2)
/** Register address: audio mute control */
#define DWC_AUD_MUTE_CTRL        (0x258UL)
/** Manual/automatic audio mute control */
#define		AUD_MUTE_SEL			MSK(2, 5)
/** Force unmute (overrules all) */
#define		AUD_MUTE_FORCE_UN		(0)
/** Automatic mute when FIFO thresholds are exceeded */
#define		AUD_MUTE_FIFO_TH		(1)
/** Automatic mute when FIFO over/underflows */
#define		AUD_MUTE_FIFO_FL		(2)
/** Force mute (overrules all) */
#define		AUD_MUTE_FORCE			(3)
/** Register address: serial audio output control */
#define DWC_AUD_SAO_CTRL         (0x260UL)
/** Register address: parallel audio output control */
#define DWC_AUD_PAO_CTRL         (0x264UL)
/** Register address: audio FIFO status */
#define DWC_AUD_FIFO_STS         (0x27CUL)
#define OVERFL_STS _BIT(4)
#define UNDERFL_STS _BIT(3)
#define THS_PASS_STS _BIT(2)

#define DWC_AUDPLL_GEN_CTS       (0x280UL)
#define DWC_AUDPLL_GEN_N         (0x284UL)

/** Register address: lock detector threshold */
#define DWC_CI_PLLAUDIO_5        (0x28CUL)
/** Register address: test mode selection */
#define DWC_CI_PLLAUDIO_4        (0x290UL)
/** Register address: bypass divider control */
#define DWC_CI_PLLAUDIO_3        (0x294UL)
/** Register address: monitoring */
#define DWC_CI_PLLAUDIO_2        (0x298UL)
/** Register address: control */
#define DWC_CI_PLLAUDIO_1        (0x29CUL)
/** Register address: SNPS PHY GEN3 control */
#define PDDQ_DW	_BIT(1)
#define PHY_RST	_BIT(0)
#define DWC_SNPS_PHYG3_CTRL	(0x2C0UL)
/** Register address:  I2C Master: */
/** slave address - starting version 1.30a */
#define DWC_I2CM_PHYG3_SLAVE		(0x2C4UL)
/** Register address: I2C Master: */
/** register address - starting version 1.30a */
#define DWC_I2CM_PHYG3_ADDRESS		(0x2C8UL)
/** Register address: I2C Master: */
/** data to write to slave-starting version 1.30a*/
#define DWC_I2CM_PHYG3_DATAO		(0x2CCUL)
/** Register address: I2C Master: */
/** data read from slave-starting version 1.30a */
#define DWC_I2CM_PHYG3_DATAI		(0x2D0UL)
/** Register address: I2C Master: */
/** operation RD/WR - starting version 1.30a */
#define DWC_I2CM_PHYG3_OPERATION		(0x2D4UL)
/** Register address: I2C Master: */
/** SS/HS mode - starting version 1.30a */
#define DWC_I2CM_PHYG3_MODE			(0x2D8UL)
/** Register address: I2C Master: */
/** soft reset - starting version 1.30a */
#define DWC_I2CM_PHYG3_SOFTRST		(0x2DCUL)
/** Register address: I2C Master: */
/** ss mode counters  - starting version 1.30a */
#define DWC_I2CM_PHYG3_SS_CNTS		(0x2E0UL)
/** Register address:I2C Master:  */
/** hs mode counters  - starting version 1.30a */
#define DWC_I2CM_PHYG3_FS_HCNT		(0x2E4UL)
/*
 * Packet Decoder and FIFO Control registers
 */
/** Register address: packet decoder and FIFO control */
#define DWC_PDEC_CTRL            (0x300UL)
/** Packet FIFO store filter enable */
#define PFIFO_STORE_FILTER_EN	_BIT(31)

/** Packet FIFO store packet */
#define PFIFO_DRM_EN		_BIT(29)/*type:0x87*/
#define PFIFO_AMP_EN		_BIT(28)/*type:0x0D*/
#define PFIFO_NTSCVBI_EN	_BIT(27)/*type:0x86*/
#define PFIFO_MPEGS_EN		_BIT(26)/*type:0x85*/
#define PFIFO_AUD_EN		_BIT(25)/*type:0x84*/
#define PFIFO_SPD_EN		_BIT(24)/*type:0x83*/
#define PFIFO_AVI_EN		_BIT(23)/*type:0x82*/
#define PFIFO_VS_EN			_BIT(22)/*type:0x81*/
#define PFIFO_GMT_EN		_BIT(21)/*type:0x0A*/
#define PFIFO_ISRC2_EN		_BIT(20)/*type:0x06*/
#define PFIFO_ISRC1_EN		_BIT(19)/*type:0x05*/
#define PFIFO_ACP_EN		_BIT(18)/*type:0x04*/
#define PFIFO_GCP_EN		_BIT(17)/*type:0x03*/
#define PFIFO_ACR_EN		_BIT(16)/*type:0x01*/
/*tl1*/
#define PFIFO_EMP_EN		_BIT(30)/*type:0x7f*/

#define		GCP_GLOBAVMUTE			_BIT(15)
/** Packet FIFO clear min/max information */
#define		PD_FIFO_FILL_INFO_CLR	_BIT(8)
/** Packet FIFO skip one packet */
#define		PD_FIFO_SKIP			_BIT(6)
/** Packet FIFO clear */
#define		PD_FIFO_CLR				_BIT(5)
/** Packet FIFO write enable */
#define		PD_FIFO_WE				_BIT(4)
/** Packet error detection/correction enable */
#define		PDEC_BCH_EN				_BIT(0)
/** Register address: packet decoder and FIFO configuration */
#define DWC_PDEC_FIFO_CFG        (0x304UL)
/** Register address: packet decoder and FIFO status */
#define DWC_PDEC_FIFO_STS        (0x308UL)
/** Register address: packet decoder and FIFO byte data */
#define DWC_PDEC_FIFO_DATA       (0x30CUL)
/** Register address: packet decoder and FIFO debug control */
#define DWC_PDEC_DBG_CTRL        (0x310UL)/*968 966 txl define*/
#define DWC_PDEC_AUDDET_CTRL     (0x310UL)/*txlx define*/
/** Register address: packet decoder and FIFO measured timing gap */
#define DWC_PDEC_DBG_TMAX        (0x314UL)
/** Register address: CTS loop */
#define DWC_PDEC_DBG_CTS         (0x318UL)
/** Register address: ACP frequency count */
#define DWC_PDEC_DBG_ACP         (0x31CUL)
/** Register address: signal errors in data island packet */
#define DWC_PDEC_DBG_ERDWC_CORR   (0x320UL)
/** Register address: packet decoder and FIFO status */
#define DWC_PDEC_FIFO_STS1        (0x324UL)
/** Register address: CTS reset measurement control */
#define DWC_PDEC_ACRM_CTRL       (0x330UL)
/** Register address: maximum CTS div N value */
#define DWC_PDEC_ACRM_MAX        (0x334UL)
/** Register address: minimum CTS div N value */
#define DWC_PDEC_ACRM_MIN        (0x338UL)
#define DWC_PDEC_ERR_FILTER			(0x33CUL)
/** Register address: audio sub packet control */
#define DWC_PDEC_ASP_CTRL        (0x340UL)
/** Automatic mute all video channels */
#define		AUTO_VMUTE				_BIT(6)
/** Automatic mute audio sub packets */
#define		AUTO_SPFLAT_MUTE		MSK(4, 2)
/** Register address: audio sub packet errors */
#define DWC_PDEC_ASP_ERR         (0x344UL)
/** Register address: packet decoder status, see packet interrupts */
#define PD_NEW_ENTRY	MSK(1, 8)
#define PD_TH_START		MSK(1, 2)
#define PD_AUD_LAYOUT	_BIT(11)
#define PD_GCP_MUTE_EN	_BIT(21)
#define DWC_PDEC_STS             (0x360UL)
/** Register address: Packet Decoder Audio Status*/
#define DWC_PDEC_AUD_STS         (0x364UL)
#define AUDS_RCV					MSK(1, 0)
#define AUDS_HBR_RCV			_BIT(3)
/** Register address: general control packet AV mute */
#define DWC_PDEC_GCP_AVMUTE      (0x380UL)
/** Register address: audio clock regeneration */
#define DWC_PDEC_ACR_CTS        (0x390UL)
/** Audio clock regeneration, CTS parameter */
#define		CTS_DECODED				MSK(20, 0)
/** Register address: audio clock regeneration */
#define DWC_PDEC_ACR_N		(0x394UL)
/** Audio clock regeneration, N parameter */
#define		N_DECODED				MSK(20, 0)
/** Register address: auxiliary video information info frame */
#define DWC_PDEC_AVI_HB		(0x3A0UL)
/** AVI content type*/
#define CONETNT_TYPE		MSK(2, 28)
/** PR3-0, pixel repetition factor */
#define		PIX_REP_FACTOR			MSK(4, 24)
/** Q1-0, YUV quantization range */
#define		YUV_QUANT_RANGE			MSK(2, 30)
/** Register address: auxiliary video information info frame */
#define DWC_PDEC_AVI_PB		(0x3A4UL)
/** VIC6-0, video mode identification code */
#define		VID_IDENT_CODE			MSK(8, 24)
/** ITC, IT content */
#define		IT_CONTENT				_BIT(23)
/** EC2-0, extended colorimetry */
#define		EXT_COLORIMETRY			MSK(3, 20)
/** Q1-0, RGB quantization range */
#define		RGB_QUANT_RANGE			MSK(2, 18)
/** SC1-0, non-uniform scaling information */
#define		NON_UNIF_SCALE			MSK(2, 16)
/** C1-0, colorimetry information */
#define		COLORIMETRY				MSK(2, 14)
/** M1-0, picture aspect ratio */
#define		PIC_ASPECT_RATIO		MSK(2, 12)
/** R3-0, active format aspect ratio */
#define		ACT_ASPECT_RATIO		MSK(4, 8)
/** Y1-0, video format */
#define		VIDEO_FORMAT			MSK(2, 5)
/** A0, active format information present */
#define		ACT_INFO_PRESENT		_BIT(4)
/** B1-0, bar valid information */
#define		BAR_INFO_VALID			MSK(2, 2)
/** S1-0, scan information from packet extraction */
#define		SCAN_INFO				MSK(2, 0)
/** Register address: auxiliary video information info frame */
#define DWC_PDEC_AVI_TBB		(0x3A8UL)
/** Line number to start of bottom bar */
#define		LIN_ST_BOT_BAR			MSK(16, 16)
/** Line number to end of top bar */
#define		LIN_END_TOP_BAR			MSK(16, 0)
/** Register address: auxiliary video information info frame */
#define DWC_PDEC_AVI_LRB		(0x3ACUL)
/** Pixel number of start right bar */
#define		PIX_ST_RIG_BAR			MSK(16, 16)
/** Pixel number of end left bar */
#define		PIX_END_LEF_BAR			MSK(16, 0)
/** Register addr: special audio layout control for multi-channel audio */
#define DWC_PDEC_AIF_CTRL	(0x3C0UL)
/** Register address: audio info frame */
#define DWC_PDEC_AIF_HB		(0x3C4UL)
/** Register address: audio info frame */
#define DWC_PDEC_AIF_PB0		(0x3C8UL)
/** CA7-0, channel/speaker allocation */
#define		CH_SPEAK_ALLOC			MSK(8, 24)
/** CTX, coding extension */
#define		AIF_DATA_BYTE_3			MSK(8, 16)
/** SF2-0, sample frequency */
#define		SAMPLE_FREQ				MSK(3, 10)
/** SS1-0, sample size */
#define		SAMPLE_SIZE				MSK(2, 8)
/** CT3-0, coding type */
#define		CODING_TYPE				MSK(4, 4)
/** CC2-0, channel count */
#define		CHANNEL_COUNT			MSK(3, 0)
/** Register address: audio info frame */
#define DWC_PDEC_AIF_PB1		(0x3CCUL)
/** DM_INH, down-mix inhibit */
#define		DWNMIX_INHIBIT			_BIT(7)
/** LSV3-0, level shift value */
#define		LEVEL_SHIFT_VAL			MSK(4, 3)
/** Register address: gamut sequence number */
#define DWC_PDEC_GMD_HB		(0x3D0UL)
/** Register address: gamut meta data */
#define DWC_PDEC_GMD_PB		(0x3D4UL)

/* Vendor Specific Info Frame */
#define DWC_PDEC_VSI_ST0         (0x3E0UL)
#define IEEE_REG_ID         MSK(24, 0)

#define DWC_PDEC_VSI_ST1         (0x3E4UL)
#define H3D_STRUCTURE       MSK(4, 16)
#define H3D_EXT_DATA        MSK(4, 20)
#define HDMI_VIDEO_FORMAT   MSK(3, 5)
#define VSI_LENGTH	    MSK(5, 0)

#define DWC_PDEC_VSI_PLAYLOAD0 (0x368UL)
#define DWC_PDEC_VSI_PLAYLOAD1 (0x36CUL)
#define DWC_PDEC_VSI_PLAYLOAD2 (0x370UL)
#define DWC_PDEC_VSI_PLAYLOAD3 (0x374UL)
#define DWC_PDEC_VSI_PLAYLOAD4 (0x378UL)
#define DWC_PDEC_VSI_PLAYLOAD5 (0x37CUL)

#define DWC_PDEC_VSI_PB0 (0x3e8UL)
#define DWC_PDEC_VSI_PB1 (0x3ecUL)
#define DWC_PDEC_VSI_PB2 (0x3f0UL)
#define DWC_PDEC_VSI_PB3 (0x3f4UL)
#define DWC_PDEC_VSI_PB4 (0x3f8UL)
#define DWC_PDEC_VSI_PB5 (0x3fcUL)

#define DWC_PDEC_AMP_HB			(0x480UL)
#define	DWC_PDEC_AMP_PB0		(0x484UL)
#define	DWC_PDEC_AMP_PB1		(0x488UL)
#define	DWC_PDEC_AMP_PB2		(0x48cUL)
#define	DWC_PDEC_AMP_PB3		(0x490UL)
#define	DWC_PDEC_AMP_PB4		(0x494UL)
#define	DWC_PDEC_AMP_PB5		(0x498UL)
#define	DWC_PDEC_AMP_PB6		(0x49cUL)

#define DWC_PDEC_NTSCVBI_HB		(0x4a0UL)
#define	DWC_PDEC_NTSCVBI_PB0	(0x4a4UL)
#define	DWC_PDEC_NTSCVBI_PB1	(0x4a8UL)
#define	DWC_PDEC_NTSCVBI_PB2	(0x4acUL)
#define	DWC_PDEC_NTSCVBI_PB3	(0x4b0UL)
#define	DWC_PDEC_NTSCVBI_PB4	(0x4b4UL)
#define	DWC_PDEC_NTSCVBI_PB5	(0x4b8UL)
#define	DWC_PDEC_NTSCVBI_PB6	(0x4bcUL)

/*
 * DTL Interface registers
 */
/** Register address: dummy register for testing */
#define DWC_DUMMY_IP_REG		(0xF00UL)
/*
 * Packet Decoder Interrupt registers
 */
/** Register address: packet decoder interrupt clear enable */
#define DWC_PDEC_IEN_CLR		(0xF78UL)
/** Register address: packet decoder interrupt set enable */
#define DWC_PDEC_IEN_SET		(0xF7CUL)
/** Register address: packet decoder interrupt status */
#define DWC_PDEC_ISTS		(0xF80UL)
/** Register address: packet decoder interrupt enable */
#define DWC_PDEC_IEN		(0xF84UL)
/** Register address: packet decoder interrupt clear status */
#define DWC_PDEC_ICLR		(0xF88UL)
/** Register address: packet decoder interrupt set status */
#define DWC_PDEC_ISET		(0xF8CUL)
/** Drm set entry txlx*/
#define		DRM_CKS_CHG_TXLX			_BIT(31)
#define		DRM_RCV_EN_TXLX				_BIT(30)
/** DVI detection status */
#define		DVIDET					_BIT(28)
/** Vendor Specific Info frame changed */
#define		VSI_CKS_CHG				_BIT(27)

/** AIF checksum changed */
#define		AIF_CKS_CHG				_BIT(25)
/** AVI checksum changed */
#define		AVI_CKS_CHG				_BIT(24)
/** GCP AVMUTE changed */
#define		GCP_AV_MUTE_CHG			_BIT(21)
#define		AVI_RCV					_BIT(18)
#define		GCP_RCV					_BIT(16)
/** Vendor Specific Info frame rcv*/
#define		VSI_RCV				_BIT(15)
/** Drm set entry */
#define		DRM_CKS_CHG				_BIT(10)
#define	DRM_RCV_EN				_BIT(9)
/** Packet FIFO new entry */
#define		PD_FIFO_NEW_ENTRY		_BIT(8)
/** Packet FIFO overflow */
#define		PD_FIFO_OVERFL			_BIT(4)
/** Packet FIFO underflow */
#define		PD_FIFO_UNDERFL			_BIT(3)
#define		PD_FIFO_START_PASS		_BIT(2)
/*
 * Audio Clock Interrupt registers
 */
/** Register address: audio clock and cec interrupt clear enable */
#define DWC_AUD_CEC_IEN_CLR	(0xF90UL)
/** Register address: audio clock and cec interrupt set enable */
#define DWC_AUD_CEC_IEN_SET	(0xF94UL)
/** Register address: audio clock and cec interrupt status */
#define DWC_AUD_CEC_ISTS		(0xF98UL)
/** Register address: audio clock and cec interrupt enable */
#define DWC_AUD_CEC_IEN		(0xF9CUL)
/** Register address: audio clock and cec interrupt clear status */
#define DWC_AUD_CEC_ICLR		(0xFA0UL)
/** Register address: audio clock and cec interrupt set status */
#define DWC_AUD_CEC_ISET		(0xFA4UL)
/*
 * Audio FIFO Interrupt registers
 */
/** Register address: audio FIFO interrupt clear enable */
#define DWC_AUD_FIFO_IEN_CLR	(0xFA8UL)
/** Register address: audio FIFO interrupt set enable */
#define DWC_AUD_FIFO_IEN_SET	(0xFACUL)
/** Register address: audio FIFO interrupt status */
#define DWC_AUD_FIFO_ISTS	(0xFB0UL)
/** Register address: audio FIFO interrupt enable */
#define DWC_AUD_FIFO_IEN		(0xFB4UL)
/** Register address: audio FIFO interrupt clear status */
#define DWC_AUD_FIFO_ICLR	(0xFB8UL)
/** Register address: audio FIFO interrupt set status */
#define DWC_AUD_FIFO_ISET	(0xFBCUL)
/** Audio FIFO overflow interrupt */
#define	OVERFL		_BIT(4)
/** Audio FIFO underflow interrupt */
#define	UNDERFL		_BIT(3)
/*
 * Mode Detection Interrupt registers
 */
/** Register address: mode detection interrupt clear enable */
#define DWC_MD_IEN_CLR		(0xFC0UL)
/** Register address: mode detection interrupt set enable */
#define DWC_MD_IEN_SET		(0xFC4UL)
/** Register address: mode detection interrupt status */
#define DWC_MD_ISTS		(0xFC8UL)
/** Register address: mode detection interrupt enable */
#define DWC_MD_IEN		(0xFCCUL)
/** Register address: mode detection interrupt clear status */
#define DWC_MD_ICLR		(0xFD0UL)
/** Register address: mode detection interrupt set status */
#define DWC_MD_ISET		(0xFD4UL)
/** Video mode interrupts */
#define	VIDEO_MODE		(MSK(3, 9) | MSK(2, 6) | _BIT(3))
/*
 * HDMI Interrupt registers
 */
/** Register address: HDMI interrupt clear enable */
#define DWC_HDMI_IEN_CLR		(0xFD8UL)
/** Register address: HDMI interrupt set enable */
#define DWC_HDMI_IEN_SET		(0xFDCUL)
/** Register address: HDMI interrupt status */
#define DWC_HDMI_ISTS		(0xFE0UL)
/** Register address: HDMI interrupt enable */
#define DWC_HDMI_IEN		(0xFE4UL)
/** Register address: HDMI interrupt clear status */
#define DWC_HDMI_ICLR		(0xFE8UL)
/** Register address: HDMI interrupt set status */
#define DWC_HDMI_ISET		(0xFECUL)
/** AKSV receive interrupt */
#define		AKSV_RCV				_BIT(25)
#define	SCDC_TMDS_CFG_CHG		_BIT(19)
/** Deep color mode change interrupt */
#define		DCM_CURRENT_MODE_CHG	_BIT(16)
#define		CTL3			_BIT(13)
#define		CTL2			_BIT(12)
#define		CTL1			_BIT(11)
#define		CTL0			_BIT(10)

/** Clock change interrupt */
#define		CLK_CHANGE				_BIT(6)
#define		PLL_LCK_CHG				_BIT(5)

#define DWC_HDMI2_IEN_CLR		(0xf60UL)
#define DWC_HDMI2_IEN_SET		(0xF64UL)
#define DWC_HDMI2_ISTS			(0xF68UL)
#define DWC_HDMI2_IEN			(0xF6CUL)
#define DWC_HDMI2_ICLR			(0xF70UL)

/*
 * DMI registers
 */
/** Register address: DMI software reset */
#define DWC_DMI_SW_RST          (0xFF0UL)
#define		IAUDIOCLK_DOMAIN_RESET	_BIT(4)
/** Register address: DMI disable interface */
#define DWC_DMI_DISABLE_IF      (0xFF4UL)
/** Register address: DMI module ID */
#define DWC_DMI_MODULE_ID       (0xFFCUL)

/*
 * HDCP registers
 */
/** Register address: control */
#define DWC_HDCP_CTRL			(0x0C0UL)
/** HDCP enable */
#define		HDCP_ENABLE		_BIT(24)
/** HDCP key decryption */
#define		KEY_DECRYPT_ENABLE		_BIT(1)
/** HDCP activation */
#define		ENCRIPTION_ENABLE				_BIT(0)
/** Register address: configuration */
#define DWC_HDCP_SETTINGS		(0x0C4UL)
/*fast mode*/
#define HDCP_FAST_MODE			_BIT(12)
#define HDCP_BCAPS				_BIT(13)
/** Register address: key description seed */
#define DWC_HDCP_SEED			(0x0C8UL)
/** Register address: receiver key selection */
#define DWC_HDCP_BKSV1			(0x0CCUL)
/** Register address: receiver key selection */
#define DWC_HDCP_BKSV0			(0x0D0UL)
/** Register address: key index */
#define DWC_HDCP_KIDX			(0x0D4UL)
/** Register address: encrypted key */
#define DWC_HDCP_KEY1			(0x0D8UL)
/** Register address: encrypted key */
#define DWC_HDCP_KEY0			(0x0DCUL)
/** Register address: debug */
#define DWC_HDCP_DBG				(0x0E0UL)
/** Register address: transmitter key selection vector */
#define DWC_HDCP_AKSV1			(0x0E4UL)
/** Register address: transmitter key selection vector */
#define DWC_HDCP_AKSV0			(0x0E8UL)
/** Register address: session random number */
#define DWC_HDCP_AN1				(0x0ECUL)
/** Register address: session random number */
#define DWC_HDCP_AN0			(0x0F0UL)
/** Register address: EESS, WOO */
#define DWC_HDCP_EESS_WOO		(0x0F4UL)
/** Register address: key set writing status */
#define DWC_HDCP_STS				(0x0FCUL)
/** HDCP encrypted status */
#define ENCRYPTED_STATUS			_BIT(9)
/** HDCP key set writing status */
#define		HDCP_KEY_WR_OK_STS		_BIT(0)
/** Register address: repeater KSV list control */
#define	DWC_HDCP_RPT_CTRL		(0x600UL)
/** KSV list key set writing status */
#define		KSV_HOLD				_BIT(6)
/** KSV list waiting status */
#define		WAITING_KSV				_BIT(5)
/** V` waiting status */
#define		FIFO_READY				_BIT(4)
/** Repeater capability */
#define		REPEATER				_BIT(3)
/** KSV list ready */
#define		KSVLIST_READY			_BIT(2)
#define		KSVLIST_TIMEOUT			_BIT(1)
#define		KSVLIST_LOSTAUTH		_BIT(0)
/** Register address: repeater status */
#define	DWC_HDCP_RPT_BSTATUS		(0x604UL)
/** Topology error indicator */
#define		MAX_CASCADE_EXCEEDED	_BIT(11)
/** Repeater cascade depth */
#define		DEPTH					MSK(3, 8)
/** Topology error indicator */
#define		MAX_DEVS_EXCEEDED		_BIT(7)
/** Attached downstream device count */
#define		DEVICE_COUNT			MSK(7, 0)
/** Register address: repeater KSV FIFO control */
#define	DWC_HDCP_RPT_KSVFIFOCTRL	(0x608UL)
/** Register address: repeater KSV FIFO */
#define	DWC_HDCP_RPT_KSVFIFO1	(0x60CUL)
/** Register address: repeater KSV FIFO */
#define	DWC_HDCP_RPT_KSVFIFO0	(0x610UL)

/*
 * ESM registers
 */

/** HPI Register */
#define HPI_REG_IRQ_STATUS				0x24
#define IRQ_STATUS_UPDATE_BIT			_BIT(3)
#define HPI_REG_EXCEPTION_STATUS		0x60
#define EXCEPTION_CODE					MSK(8, 1)
#define AUD_PLL_THRESHOLD	1000000

/* tl1 HIU related register */
#define HHI_HDMIRX_AXI_CLK_CNTL			(0xb8 << 2)

/* tl1/TM2 HIU apll register */
#define HHI_HDMIRX_APLL_CNTL0			(0xd2 << 2)
#define HHI_HDMIRX_APLL_CNTL1			(0xd3 << 2)
#define HHI_HDMIRX_APLL_CNTL2			(0xd4 << 2)
#define HHI_HDMIRX_APLL_CNTL3			(0xd5 << 2)
#define HHI_HDMIRX_APLL_CNTL4			(0xd6 << 2)

/* tl1/TM2 HIU PHY register */
#define HHI_HDMIRX_PHY_MISC_CNTL0		(0xd7 << 2)
#define MISCI_COMMON_RST				_BIT(10)
#define HHI_HDMIRX_PHY_MISC_CNTL1		(0xd8 << 2)
#define MISCI_MANUAL_MODE				_BIT(22)
#define HHI_HDMIRX_PHY_MISC_CNTL2		(0xe0 << 2)
#define HHI_HDMIRX_PHY_MISC_CNTL3		(0xe1 << 2)
#define HHI_HDMIRX_PHY_DCHA_CNTL0		(0xe2 << 2)
#define HHI_HDMIRX_PHY_DCHA_CNTL1		(0xe3 << 2)
	#define DFE_OFSETCAL_START			_BIT(27)
	#define DFE_TAP1_EN					_BIT(17)
	#define DEF_SUM_RS_TRIM				MSK(3, 12)
	/*[4:5] in trim,[6:7] im trim*/
	#define DFE_SUM_TRIM				MSK(4, 4)
#define HHI_HDMIRX_PHY_DCHA_CNTL2		(0xe4 << 2)
	#define DFE_VADC_EN					_BIT(21)
#define HHI_HDMIRX_PHY_DCHA_CNTL3		(0xc5 << 2)/*for revB*/
#define HHI_HDMIRX_PHY_DCHD_CNTL0		(0xe5 << 2)
	#define CDR_LKDET_EN				_BIT(28)
	/*bit'24:eq rst bit'25:cdr rst*/
	#define CDR_EQ_RSTB					MSK(2, 24)
	/*0:manual 1:c only 2:r only 3:both rc*/
	#define EQ_ADP_MODE					MSK(2, 10)
	#define EQ_ADP_STG					MSK(2, 8)
#define HHI_HDMIRX_PHY_DCHD_CNTL1		(0xe6 << 2)
	/*tm2b*/
	#define OFST_CAL_START				_BIT(31)
	#define EQ_BYP_VAL					MSK(5, 12)
#define HHI_HDMIRX_PHY_DCHD_CNTL2		(0xe7 << 2)
	#define DFE_DBG_STL					MSK(3, 28)
	#define	DFE_EN						_BIT(27)
	#define DFE_RSTB					_BIT(26)
	#define TAP1_BYP_EN					_BIT(19)
#define HHI_HDMIRX_PHY_DCHD_CNTL3		(0xc6 << 2)/*for revB*/
	#define DBG_STS_SEL					MSK(2, 30)
#define HHI_HDMIRX_PHY_ARC_CNTL			(0xe8 << 2)
#define HHI_HDMIRX_EARCTX_CNTL0			(0x69 << 2)
#define HHI_HDMIRX_EARCTX_CNTL1			(0x6a << 2)
#define HHI_HDMIRX_PHY_MISC_STAT		(0xee << 2)
#define HHI_HDMIRX_PHY_DCHD_STAT		(0xef << 2)

/* T5 HIU apll register */
#define HHI_RX_APLL_CNTL0			(0x0 << 2)/*0x0*/
#define HHI_RX_APLL_CNTL1			(0x1 << 2)/*0x4*/
#define HHI_RX_APLL_CNTL2			(0x2 << 2)/*0x8*/
#define HHI_RX_APLL_CNTL3			(0x3 << 2)/*0xc*/
#define HHI_RX_APLL_CNTL4			(0x4 << 2)/*0x10*/

/* T5 HIU PHY register */
#define HHI_RX_PHY_MISC_CNTL0		(0x5 << 2)/*0x14*/
	#define SQO_GATE				_BIT(30)
	#define PLL_SRC_SEL				_BIT(29)
#define HHI_RX_PHY_MISC_CNTL1		(0x6 << 2)/*0x18*/
#define HHI_RX_PHY_MISC_CNTL2		(0x7 << 2)/*0x1c*/
#define HHI_RX_PHY_MISC_CNTL3		(0x8 << 2)/*0x20*/
#define HHI_RX_PHY_DCHA_CNTL0		(0x9 << 2)/*0x24*/
	#define LEQ_BUF_GAIN			MSK(3, 20)
	#define LEQ_POLE				MSK(3, 16)
	#define HYPER_GAIN				MSK(3, 12)
#define HHI_RX_PHY_DCHA_CNTL1		(0xa << 2)/*0x28*/
#define HHI_RX_PHY_DCHA_CNTL2		(0xb << 2)/*0x2c*/
	#define EYE_MONITOR_EN1			_BIT(27)/*The same as dchd_eye[19]*/
	#define AFE_EN					_BIT(17)
#define HHI_RX_PHY_DCHA_CNTL3		(0xc << 2)/*0x30*/
#define HHI_RX_PHY_DCHD_CNTL0		(0xd << 2)/*0x34*/
	#define CDR_MODE				_BIT(31)
	#define CDR_FR_EN				_BIT(30)
	#define EQ_EN					_BIT(29)
	#define CDR_PH_DIV				MSK(3, 0)
	#define CDR_RST					_BIT(25)
	#define EQ_RST					_BIT(24)
#define HHI_RX_PHY_DCHD_CNTL1		(0xe << 2)/*0x38*/
	#define IQ_OFST_SIGN			_BIT(27)
	#define IQ_OFST_VAL				MSK(5, 22)
	#define EQ_BYP_VAL2				MSK(5, 17)
	#define EQ_BYP_VAL1				MSK(5, 12)
#define HHI_RX_PHY_DCHD_CNTL2		(0xf << 2)/*0x3c*/
	#define DFE_HOLD				_BIT(31)
	#define DFE_RST					_BIT(26)
	#define TAP0_BYP				_BIT(23)
	#define EYE_STATUS				MSK(3, 28)
	#define ERROR_CNT				0x0
	#define SCAN_STATE				0x1
	#define POSITIVE_EYE_HEIGHT		0x2
	#define NEGATIVE_EYE_HEIGHT		0x3
	#define LEFT_EYE_WIDTH			0x4
	#define RIGHT_EYE_WIDTH			0x5
#define HHI_RX_PHY_DCHD_CNTL3		(0x10 << 2)/*0x40*/
#define HHI_RX_PHY_DCHD_CNTL4		(0x11 << 2)/*0x44*/
	#define EYE_MONITOR_EN			_BIT(19)
	#define EYE_STATUS_EN			_BIT(18)
	#define EYE_EN_HW_SCAN			_BIT(16)
#define HHI_RX_PHY_MISC_STAT		(0x12 << 2)/*0x48*/
#define HHI_RX_PHY_DCHD_STAT		(0x13 << 2)/*0x4c*/

#define TMDS_CLK_MIN			(15000UL)
#define TMDS_CLK_MAX			(340000UL)

/*
 * SMC CMD define
 * call BL31 interface
 */
#define HDMIRX_RD_SEC_TOP		0x8200001d
#define HDMIRX_WR_SEC_TOP		0x8200001e
#define HDCP22_RX_ESM_READ		0x8200001f
#define HDCP22_RX_ESM_WRITE		0x8200002f
#define HDCP22_RX_SET_DUK_KEY	0x8200002e
#define HDCP22_RP_SET_DUK_KEY	0x8200002c
#define HDCP14_RX_SETKEY		0x8200002d
#define HDMIRX_RD_SEC_TOP_NEW	0x8200008b
#define HDMIRX_WR_SEC_TOP_NEW	0x8200008c
#define HDMIRX_RD_AES			0x8200008d
#define HDMIRX_WR_AES			0x8200008e
#define HDMIRX_RD_COR			0x8200008f
#define HDMIRX_WR_COR			0x82000091
#define HDMI_RX_HDCP_CFG		0x820000aa

/* COR reg start */
#define COR_SCDC_TMDS_CFG	0x7820
/* bit0 1:hdmi 0:dvi*/
#define COR_AUD_PATH_STS	0x1627
/* bit 2 */
#define COR_HDCP14_STS		0x1071
/* */
#define COR_VININ_STS		0x1027
/* 24bit */
#define COR_FRAME_RATE_LO	0x1888
#define COR_FRAME_RATE_MI	0x1889
#define COR_FRAME_RATE_HI	0x188A
/* interlace : bit2
 * vsync polarity : bit1
 * hsync polarity : bit0
 */
#define COR_FDET_STS		0x1881
/* ACR CTS */
#define COR_N_LO		0x1406
#define COR_N_MI		0x1407
#define COR_N_HI		0x1408 /* 4bits */
#define COR_CTS_LO		0x140C
#define COR_CTS_MI		0x140D
#define COR_CTS_HI		0x140E
/* h-ative (pixel per line) */
#define COR_PIXEL_CNT_LO	0x188C
#define COR_PIXEL_CNT_HI	0x188D
#define COR_LINE_CNT_LO		0x188E
#define COR_LINE_CNT_HI		0x188F

/* h-sync */
#define COR_HSYNC_LOW_COUNT_LO      0x1890
#define COR_HSYNC_LOW_COUNT_HI      0x1891
#define COR_HSYNC_HIGH_COUNT_LO     0x1892
#define COR_HSYNC_HIGH_COUNT_HI     0x1893

/* v-sync */
#define COR_VSYNC_LOW_COUNT_LO      0x1898
#define COR_VSYNC_LOW_COUNT_HI      0x1899
#define COR_VSYNC_HIGH_COUNT_LO     0x189A
#define COR_VSYNC_HIGH_COUNT_HI     0x189B

/*t7/t3*/
#define RX_CLK_CTRL			(0x4A << 2)
#define RX_CLK_CTRL1		(0x4B << 2)
#define RX_CLK_CTRL2		(0x4C << 2)
#define RX_CLK_CTRL3		(0x4D << 2)
#define CLKCTRL_SYS_CLK_EN0_REG2	(0x13 << 2)

/*t5w*/
#define RX_CLK_CTRL_T5W			(0x8 << 2)
#define RX_CLK_CTRL1_T5W		(0x9 << 2)
#define RX_CLK_CTRL2_T5W		(0xa << 2)
#define RX_CLK_CTRL3_T5W		(0xb << 2)

/* AONREG */
#define RX_AON_SRST		0x05
#define RX_HDMI2_MODE_CTRL	0x40
#define RX_INT_CTRL		0x79
/* PWDREG */
#define RX_PWD_CTRL		0x1001
#define RX_STATE_PWD		0x1006
#define RX_SYS_CTRL1		0x1007
#define RX_SYS_TMDS_CH_MAP	0x100E
#define RX_SYS_TMDS_D_IR	0x100F
#define RX_TMDS_CCTRL2		0x1013
#define RX_TEST_STAT		0x103B
#define RX_PWD0_CLK_DIV_0	0x10C1
#define RX_VP_INPUT_FORMAT_LO	0x1814
#define RX_VP_INPUT_FORMAT_HI	0x1815
/* IRQ */
#define RX_PWD_INT_CTRL		0x105f
#define RX_DEPACK_INTR2_MASK	0x1135
#define RX_DEPACK_INTR4_MASK	0x1139

//==================== AON ===============
#define	RX_DEV_IDL_AON_IVCRX        0x00000000
#define	RX_DEV_IDH_AON_IVCRX        0x00000001
#define	RX_DEV_REV_AON_IVCRX        0x00000002
#define	RX_VND_IDL_AON_IVCRX        0x00000003
#define	RX_VND_IDH_AON_IVCRX        0x00000004
#define	RX_AON_SRST_AON_IVCRX       0x00000005
#define RX_SYS_SWTCHC_AON_IVCRX     0x00000009
#define RX_DDC_DID_SADDR_AON_IVCRX  0x0000000a
#define RX_C0_SRST2_AON_IVCRX        0x0000000b
#define  RX_STATE_AON_AON_IVCRX        0x0000000c
#define PI_TO_LIMIT_L_AON_IVCRX        0x00000010
#define PI_TO_LIMIT_H_AON_IVCRX        0x00000011
#define RX_DUMMY_CONFIG1_AON_IVCRX        0x00000018
#define RX_DUMMY_CONFIG2_AON_IVCRX        0x00000019
#define RX_DUMMY_STATUS_AON_IVCRX        0x0000001a
#define HDMI2_MODE_CTRL_AON_IVCRX        0x00000040
#define  MHL_MODE_OVR_AON_IVCRX        0x00000041
#define   RX_ISO_CTRL_AON_IVCRX        0x00000042
#define  FUNC_AON_CLK_BYPASS_1_AON_IVCRX        0x00000043
#define  RX_AON_CTRL1_AON_IVCRX        0x00000044
#define RX_INTR_STATE_AON_IVCRX        0x00000070
#define   RX_INT_CTRL_AON_IVCRX        0x00000079
#define  RX_INTR2_AON_AON_IVCRX        0x00000080
#define  RX_INTR6_AON_AON_IVCRX        0x00000081
#define  RX_INTR7_AON_AON_IVCRX        0x00000082
#define  RX_INTR8_AON_AON_IVCRX        0x00000083
#define  RX_INTR9_AON_AON_IVCRX        0x00000084
#define RX_INTR10_AON_AON_IVCRX        0x00000085
#define RX_INTR2_MASK_AON_AON_IVCRX        0x00000090
#define RX_INTR6_MASK_AON_AON_IVCRX        0x00000091
#define RX_INTR7_MASK_AON_AON_IVCRX        0x00000092
#define RX_INTR8_MASK_AON_AON_IVCRX        0x00000093
#define RX_INTR9_MASK_AON_AON_IVCRX        0x00000094
#define RX_INTR10_MASK_AON_AON_IVCRX        0x00000095
#define RX_HPD_C_CTRL_AON_IVCRX        0x000000f5
#define RX_HPD_OEN_CTRL_AON_IVCRX        0x000000f6
#define RX_HPD_PE_CTRL_AON_IVCRX        0x000000f7
#define RX_HPD_PU_CTRL_AON_IVCRX        0x000000f8
#define RX_HPD_OVRT_CTRL_AON_IVCRX        0x000000f9
#define  RX_RSEN_CTRL_AON_IVCRX        0x000000fa
#define CBUS_MHL3_DISC_AON_IVCRX        0x000000fd
#define RX_CBUS_PORT_SEL_AON_IVCRX        0x000000fe

//==================== SCDC ===============
#define SCDCS_SNK_VER_SCDC_IVCRX       0x00000301
#define SCDCS_SRC_VER_SCDC_IVCRX       0x00000302
#define SCDCS_UPD_FLAGS_SCDC_IVCRX       0x00000310
#define SCDCS_TMDS_CONFIG_SCDC_IVCRX       0x00000320
#define SCDCS_SCRAMBLE_STAT_SCDC_IVCRX       0x00000321
#define SCDCS_CONFIG0_SCDC_IVCRX       0x00000330
#define SCDCS_CONFIG1_SCDC_IVCRX       0x00000331
#define SCDCS_SRC_TEST_CONFIG_SCDC_IVCRX       0x00000335
#define SCDCS_STATUS_FLAGS0_SCDC_IVCRX       0x00000340
#define SCDCS_STATUS_FLAGS1_SCDC_IVCRX       0x00000341
#define SCDCS_STATUS_FLAGS2_SCDC_IVCRX       0x00000342
#define  SCDCS_CED0_L_SCDC_IVCRX       0x00000350
#define  SCDCS_CED0_H_SCDC_IVCRX       0x00000351
#define  SCDCS_CED1_L_SCDC_IVCRX       0x00000352
#define  SCDCS_CED1_H_SCDC_IVCRX       0x00000353
#define  SCDCS_CED2_L_SCDC_IVCRX       0x00000354
#define  SCDCS_CED2_H_SCDC_IVCRX       0x00000355
#define SCDCS_CED_CHECKSUM_SCDC_IVCRX       0x00000356
#define SCDCS_CED_LN3_L_SCDC_IVCRX       0x00000357
#define SCDCS_CED_LN3_H_SCDC_IVCRX       0x00000358
#define SCDCS_RS_CORR_L_SCDC_IVCRX       0x00000359
#define SCDCS_RS_CORR_H_SCDC_IVCRX       0x0000035a
#define SCDCS_500NS_CNT_SCDC_IVCRX       0x000003a0
#define SCDCS_4P7_IN_500NS_CNT_SCDC_IVCRX       0x000003a1
#define SCDCS_1MS_IN_500NS_CNT0_SCDC_IVCRX       0x000003a2
#define SCDCS_1MS_IN_500NS_CNT1_SCDC_IVCRX       0x000003a3
#define SCDCS_20MS_IN_1MS_CNT_SCDC_IVCRX       0x000003a4
#define SCDCS_100MS_IN_1MS_CNT_SCDC_IVCRX       0x000003a5
#define SCDCS_1S_IN_20MS_CNT_SCDC_IVCRX       0x000003a6
#define SCDCS_CHR_ERRCNT_MAX0_SCDC_IVCRX       0x000003a7
#define SCDCS_CHR_ERRCNT_MAX1_SCDC_IVCRX       0x000003a8
#define    SCDCS_CNTL_SCDC_IVCRX       0x000003a9
#define SCDCS_DBG_STS_SCDC_IVCRX       0x000003aa
#define SCDCS_FRL_STATUS_SCDC_IVCRX       0x000003ac
#define SCDCS_SRC_WADDR_STAT_SCDC_IVCRX       0x000003ad
#define SCDCS_SRC_RADDR_STAT_SCDC_IVCRX       0x000003ae
#define SCDCS_TEST_CONFIG_SCDC_IVCRX       0x000003c0
#define SCDCS_MFCTR_OUI1_SCDC_IVCRX       0x000003d0
#define SCDCS_MFCTR_OUI2_SCDC_IVCRX       0x000003d1
#define SCDCS_MFCTR_OUI3_SCDC_IVCRX       0x000003d2
#define SCDCS_DEV_ID_STR0_SCDC_IVCRX       0x000003d3
#define SCDCS_DEV_ID_STR1_SCDC_IVCRX       0x000003d4
#define SCDCS_DEV_ID_STR2_SCDC_IVCRX       0x000003d5
#define SCDCS_DEV_ID_STR3_SCDC_IVCRX       0x000003d6
#define SCDCS_DEV_ID_STR4_SCDC_IVCRX       0x000003d7
#define SCDCS_DEV_ID_STR5_SCDC_IVCRX       0x000003d8
#define SCDCS_DEV_ID_STR6_SCDC_IVCRX       0x000003d9
#define SCDCS_DEV_ID_STR7_SCDC_IVCRX       0x000003da
#define  SCDCS_HW_REV_SCDC_IVCRX       0x000003db
#define SCDCS_SW_MJR_REV_SCDC_IVCRX       0x000003dc
#define SCDCS_SW_MNR_REV_SCDC_IVCRX       0x000003dd
#define SCDCS_MFCTR_SPCF0_SCDC_IVCRX       0x000003de
#define SCDCS_MFCTR_SPCF1_SCDC_IVCRX       0x000003df
#define SCDCS_MFCTR_SPCF2_SCDC_IVCRX       0x000003e0
#define SCDCS_MFCTR_SPCF3_SCDC_IVCRX       0x000003e1
#define SCDCS_MFCTR_SPCF4_SCDC_IVCRX       0x000003e2
#define SCDCS_MFCTR_SPCF5_SCDC_IVCRX       0x000003e3
#define SCDCS_MFCTR_SPCF6_SCDC_IVCRX       0x000003e4
#define SCDCS_MFCTR_SPCF7_SCDC_IVCRX       0x000003e5
#define SCDCS_MFCTR_SPCF8_SCDC_IVCRX       0x000003e6
#define SCDCS_MFCTR_SPCF9_SCDC_IVCRX       0x000003e7
#define SCDCS_MFCTR_SPCF10_SCDC_IVCRX       0x000003e8
#define SCDCS_MFCTR_SPCF11_SCDC_IVCRX       0x000003e9
#define SCDCS_MFCTR_SPCF12_SCDC_IVCRX       0x000003ea
#define SCDCS_MFCTR_SPCF13_SCDC_IVCRX       0x000003eb
#define SCDCS_MFCTR_SPCF14_SCDC_IVCRX       0x000003ec
#define SCDCS_MFCTR_SPCF15_SCDC_IVCRX       0x000003ed
#define SCDCS_MFCTR_SPCF16_SCDC_IVCRX       0x000003ee
#define SCDCS_MFCTR_SPCF17_SCDC_IVCRX       0x000003ef
#define SCDCS_MFCTR_SPCF18_SCDC_IVCRX       0x000003f0
#define SCDCS_MFCTR_SPCF19_SCDC_IVCRX       0x000003f1
#define SCDCS_MFCTR_SPCF20_SCDC_IVCRX       0x000003f2
#define SCDCS_MFCTR_SPCF21_SCDC_IVCRX       0x000003f3
#define SCDCS_MFCTR_SPCF22_SCDC_IVCRX       0x000003f4
#define SCDCS_MFCTR_SPCF23_SCDC_IVCRX       0x000003f5
#define SCDCS_MFCTR_SPCF24_SCDC_IVCRX       0x000003f6
#define SCDCS_MFCTR_SPCF25_SCDC_IVCRX       0x000003f7
#define SCDCS_MFCTR_SPCF26_SCDC_IVCRX       0x000003f8
#define SCDCS_MFCTR_SPCF27_SCDC_IVCRX       0x000003f9
#define SCDCS_MFCTR_SPCF28_SCDC_IVCRX       0x000003fa
#define SCDCS_MFCTR_SPCF29_SCDC_IVCRX       0x000003fb
#define SCDCS_MFCTR_SPCF30_SCDC_IVCRX       0x000003fc
#define SCDCS_MFCTR_SPCF31_SCDC_IVCRX       0x000003fd
#define SCDCS_MFCTR_SPCF32_SCDC_IVCRX       0x000003fe
#define SCDCS_MFCTR_SPCF33_SCDC_IVCRX       0x000003ff

//==================== PWD ===============
#define   RX_PWD_CTRL_PWD_IVCRX        0x00001001
#define  RX_PWD_SRST3_PWD_IVCRX        0x00001004
#define   RX_PWD_SRST_PWD_IVCRX        0x00001005
#define  RX_STATE_PWD_PWD_IVCRX        0x00001006
#define  RX_SYS_CTRL1_PWD_IVCRX        0x00001007
#define SYS_TMDS_CH_MAP_PWD_IVCRX        0x0000100e
#define SYS_TMDS_D_IR_PWD_IVCRX        0x0000100f
#define RX_RPT_RDY_CTRL_PWD_IVCRX        0x00001010
#define  RX_PWD_SRST2_PWD_IVCRX        0x00001011
#define RX_TMDS_CCTRL2_PWD_IVCRX        0x00001013
#define RX_HDCP2x_CTRL_PWD_IVCRX        0x00001014
#define VIDEO_MODE_CTRL_PWD_IVCRX        0x00001016
#define STREAM_PAGE_CTRL_PWD_IVCRX        0x00001017
#define PWD_CLK_CTRL0_PWD_IVCRX        0x00001018
#define   RX_T4_THRES_PWD_IVCRX        0x00001020
#define RX_T4_UNTHRES_PWD_IVCRX        0x00001021
#define RX_SW_HDMI_MODE_PWD_IVCRX        0x00001022
#define RX_PREAMBLE_CRIT_PWD_IVCRX        0x00001023
#define RX_HDCP_PREAMBLE_CRIT_PWD_IVCRX        0x00001024
#define  RX_AUDP_FILT_PWD_IVCRX        0x00001025
#define  RX_AUDP_FIFO_PWD_IVCRX        0x00001026
#define  RX_VIDIN_STS_PWD_IVCRX        0x00001027
#define   RX_H21_CTRL_PWD_IVCRX        0x00001028
#define RX_CLK_PXL_DIV_PWD_IVCRX        0x00001029
#define VID_PATH_RATIO_IN_OVR_PWD_IVCRX        0x0000102a
#define VID_PATH_RATIO_OUT_OVR_PWD_IVCRX        0x0000102b
#define  RX_TEST_CTRL_PWD_IVCRX        0x0000103a
#define  RX_TEST_STAT_PWD_IVCRX        0x0000103b
#define     RX_PD_TOT_PWD_IVCRX        0x0000103c
#define    RX_PD_SYS2_PWD_IVCRX        0x0000103e
#define      RX_INTR1_PWD_IVCRX        0x00001040
#define      RX_INTR2_PWD_IVCRX        0x00001041
#define      RX_INTR3_PWD_IVCRX        0x00001042
#define      RX_INTR4_PWD_IVCRX        0x00001043
#define      RX_INTR5_PWD_IVCRX        0x00001044
#define      RX_INTR6_PWD_IVCRX        0x00001045
#define      RX_INTR7_PWD_IVCRX        0x00001046
#define      RX_INTR8_PWD_IVCRX        0x00001047
#define      RX_INTR9_PWD_IVCRX        0x00001048
#define     RX_INTR10_PWD_IVCRX        0x00001049
#define     RX_INTR12_PWD_IVCRX        0x0000104b
#define     RX_INTR13_PWD_IVCRX        0x0000104c
#define     RX_INTR14_PWD_IVCRX        0x0000104d
#define RX_GRP_INTR1_STAT_PWD_IVCRX        0x0000104e
#define RX_GRP_INTR1_MASK_PWD_IVCRX        0x0000104f
#define RX_INTR1_MASK_PWD_IVCRX        0x00001050
#define RX_INTR2_MASK_PWD_IVCRX        0x00001051
#define RX_INTR3_MASK_PWD_IVCRX        0x00001052
#define RX_INTR4_MASK_PWD_IVCRX        0x00001053
#define RX_INTR5_MASK_PWD_IVCRX        0x00001054
#define RX_INTR6_MASK_PWD_IVCRX        0x00001055
#define RX_INTR7_MASK_PWD_IVCRX        0x00001056
#define RX_INTR8_MASK_PWD_IVCRX        0x00001057
#define RX_INTR9_MASK_PWD_IVCRX        0x00001058
#define RX_INTR10_MASK_PWD_IVCRX        0x00001059
#define RX_INTR12_MASK_PWD_IVCRX        0x0000105b
#define RX_INTR13_MASK_PWD_IVCRX        0x0000105c
#define RX_INTR14_MASK_PWD_IVCRX        0x0000105d
#define RX_PWD_INTR_STATE_PWD_IVCRX        0x0000105e
#define RX_PWD_INT_CTRL_PWD_IVCRX        0x0000105f
#define DPLL_CH0_ERR_CNT1_PWD_IVCRX        0x00001060
#define DPLL_CH0_ERR_CNT2_PWD_IVCRX        0x00001061
#define DPLL_CH1_ERR_CNT1_PWD_IVCRX        0x00001062
#define DPLL_CH1_ERR_CNT2_PWD_IVCRX        0x00001063
#define DPLL_CH2_ERR_CNT1_PWD_IVCRX        0x00001064
#define DPLL_CH2_ERR_CNT2_PWD_IVCRX        0x00001065
#define DPLL_SCDC_CTRL_PWD_IVCRX        0x00001066
#define DPLL_LN3_ERR_CNT1_PWD_IVCRX        0x00001067
#define DPLL_LN3_ERR_CNT2_PWD_IVCRX        0x00001068
#define FRL_SCDC_CTRL1_OVR_PWD_IVCRX        0x00001069
#define FRL_SCDC_LTP10_OVR_PWD_IVCRX        0x0000106a
#define FRL_SCDC_LTP32_OVR_PWD_IVCRX        0x0000106b
#define FRL_SCDC_RS_CORR_CNT1_PWD_IVCRX        0x0000106c
#define FRL_SCDC_RS_CORR_CNT2_PWD_IVCRX        0x0000106d
#define RX_HDCP_STATUS_PWD_IVCRX        0x00001071
#define RX_HDMIM_CP_CTRL_PWD_IVCRX        0x00001080
#define  RX_HDMIM_CP_PAD_STAT_PWD_IVCRX        0x00001081
#define RX_CBUS_CONNECTED_PWD_IVCRX        0x00001082
#define   RX_MHL_CTRL_PWD_IVCRX        0x00001084
#define PWD_MHL_MODE_OVR_PWD_IVCRX        0x00001085
#define FUNC_PWD_CLK_BYPASS_1_PWD_IVCRX        0x00001086
#define FUNC_PWD_CLK_BYPASS_2_PWD_IVCRX        0x00001087
#define FUNC_PWD_CLK_BYPASS_3_PWD_IVCRX        0x00001088
#define HSYNC_GEN_CTRL1_PWD_IVCRX        0x00001090
#define HSYNC_GEN_CTRL2_PWD_IVCRX        0x00001091
#define HLINE_WIDTH_STAT1_PWD_IVCRX        0x00001092
#define HLINE_WIDTH_STAT2_PWD_IVCRX        0x00001093
#define VACT_WIDTH_STAT1_PWD_IVCRX        0x00001094
#define VACT_WIDTH_STAT2_PWD_IVCRX        0x00001095
#define HBLNK_WIDTH_STAT1_PWD_IVCRX        0x00001096
#define HBLNK_WIDTH_STAT2_PWD_IVCRX        0x00001097
#define  HSYNC_GEN_EVEN_CTRL1_PWD_IVCRX        0x00001098
#define  HSYNC_GEN_EVEN_CTRL2_PWD_IVCRX        0x00001099
#define HLINE_WIDTH_EVEN_STAT1_PWD_IVCRX        0x0000109a
#define HLINE_WIDTH_EVEN_STAT2_PWD_IVCRX        0x0000109b
#define VACT_WIDTH_EVEN_STAT1_PWD_IVCRX        0x0000109c
#define VACT_WIDTH_EVEN_STAT2_PWD_IVCRX        0x0000109d
#define HBLNK_WIDTH_EVEN_STAT1_PWD_IVCRX        0x0000109e
#define HBLNK_WIDTH_EVEN_STAT2_PWD_IVCRX        0x0000109f
#define   HSYNC_GEN_ODD_CTRL1_PWD_IVCRX        0x000010a0
#define   HSYNC_GEN_ODD_CTRL2_PWD_IVCRX        0x000010a1
#define HLINE_WIDTH_ODD_STAT1_PWD_IVCRX        0x000010a2
#define HLINE_WIDTH_ODD_STAT2_PWD_IVCRX        0x000010a3
#define  VACT_WIDTH_ODD_STAT1_PWD_IVCRX        0x000010a4
#define  VACT_WIDTH_ODD_STAT2_PWD_IVCRX        0x000010a5
#define HBLNK_WIDTH_ODD_STAT1_PWD_IVCRX        0x000010a6
#define HBLNK_WIDTH_ODD_STAT2_PWD_IVCRX        0x000010a7
#define PWD0_CLK_BYP_1_PWD_IVCRX        0x000010b0
#define PWD0_CLK_BYP_2_PWD_IVCRX        0x000010b1
#define PWD0_CLK_BYP_3_PWD_IVCRX        0x000010b2
#define PWD0_CLK_EN_1_PWD_IVCRX        0x000010b3
#define PWD0_CLK_EN_2_PWD_IVCRX        0x000010b4
#define PWD0_CLK_EN_3_PWD_IVCRX        0x000010b5
#define PWD0_CLK_EN_4_PWD_IVCRX        0x000010b6
#define PWD0_RST_EN_1_PWD_IVCRX        0x000010b7
#define PWD0_RST_EN_2_PWD_IVCRX        0x000010b8
#define HDMI2P1_MODE_SEL_PWD_IVCRX        0x000010b9
#define TMDS_FPLL_CFG_CTL0_PWD_IVCRX        0x000010ba
#define TMDS_FPLL_CFG_CTL1_PWD_IVCRX        0x000010bb
#define PXL_FPLL_CFG_CTL0_PWD_IVCRX        0x000010bc
#define PXL_FPLL_CFG_CTL1_PWD_IVCRX        0x000010bd
#define PXL_BIST_CTRL_PWD_IVCRX        0x000010be
#define PWD0_CLK_BYP_4_PWD_IVCRX        0x000010bf
#define PWD0_CLK_EN_5_PWD_IVCRX        0x000010c0
#define PWD0_CLK_DIV_0_PWD_IVCRX        0x000010c1
#define HDCP_BIST_REG_PWD_IVCRX        0x000010c2
#define HDCP_BIST2_REG_PWD_IVCRX        0x000010c3
#define   PWD_SW_CLMP_AUE_OIF_PWD_IVCRX        0x000010c4
#define  AUD_MCLK_OUT_DIV_SEL_PWD_IVCRX        0x000010c5
#define  EXT_MCLK_SEL_PWD_IVCRX        0x000010c6
#define EXHAUST_CHK_EN_PWD_IVCRX        0x000010c7
#define GCP_WINDOW_CHK_1_PWD_IVCRX        0x000010c8
#define GCP_WINDOW_CHK_2_PWD_IVCRX        0x000010c9

//==================== DEPACK 02===============
#define   RX_ECC_CTRL_DP2_IVCRX        0x00001100
#define  RX_BCH_THRES_DP2_IVCRX        0x00001101
#define MUTE_ON_ERR_CTRL_DP2_IVCRX        0x00001102
#define RX_PKT_THRESH_DP2_IVCRX        0x00001104
#define RX_PKT_THRESH2_DP2_IVCRX        0x00001105
#define RX_T4_PKT_THRES_DP2_IVCRX        0x00001106
#define RX_T4_PKT_THRES2_DP2_IVCRX        0x00001107
#define RX_BCH_PKT_THRES_DP2_IVCRX        0x00001108
#define RX_BCH_PKT_THRES2_DP2_IVCRX        0x00001109
#define RX_HDCP_THRES_DP2_IVCRX        0x0000110a
#define RX_HDCP_THRES2_DP2_IVCRX        0x0000110b
#define    RX_PKT_CNT_DP2_IVCRX        0x0000110c
#define   RX_PKT_CNT2_DP2_IVCRX        0x0000110d
#define     RX_T4_ERR_DP2_IVCRX        0x0000110e
#define    RX_T4_ERR2_DP2_IVCRX        0x0000110f
#define    RX_BCH_ERR_DP2_IVCRX        0x00001110
#define   RX_BCH_ERR2_DP2_IVCRX        0x00001111
#define   RX_HDCP_ERR_DP2_IVCRX        0x00001112
#define  RX_HDCP_ERR2_DP2_IVCRX        0x00001113
#define    RX_ACR_DEC_DP2_IVCRX        0x00001114
#define RX_ACR_CTS_MASK_DP2_IVCRX        0x00001115
#define RX_ACR_HEADER0_DP2_IVCRX        0x00001116
#define RX_ACR_HEADER1_DP2_IVCRX        0x00001117
#define RX_ACR_HEADER2_DP2_IVCRX        0x00001118
#define RX_ACR_DBYTE0_DP2_IVCRX        0x00001119
#define RX_ACR_DBYTE1_DP2_IVCRX        0x0000111a
#define RX_ACR_DBYTE2_DP2_IVCRX        0x0000111b
#define RX_ACR_DBYTE3_DP2_IVCRX        0x0000111c
#define RX_ACR_DBYTE4_DP2_IVCRX        0x0000111d
#define RX_ACR_DBYTE5_DP2_IVCRX        0x0000111e
#define RX_ACR_DBYTE6_DP2_IVCRX        0x0000111f
#define RX_INT_IF_CTRL_DP2_IVCRX        0x00001120
#define RX_INT_IF_CTRL2_DP2_IVCRX        0x00001121
#define DEC_AV_MUTE_DP2_IVCRX        0x00001122
#define RX_DC_HEADER_DP2_IVCRX        0x00001123
#define RX_PHASE_LUT_DP2_IVCRX        0x00001124
#define RX_AUDP_STAT_DP2_IVCRX        0x00001127
#define RX_AUTO_CLR_PKT1_DP2_IVCRX        0x00001128
#define RX_AUTO_CLR_PKT2_DP2_IVCRX        0x00001129
#define AV_MUTE_STATUS_DP2_IVCRX        0x0000112a
#define RX_DEPACK_INTR0_DP2_IVCRX        0x00001130
#define  RX_DEPACK_INTR0_MASK_DP2_IVCRX        0x00001131
#define RX_DEPACK_INTR1_DP2_IVCRX        0x00001132
#define  RX_DEPACK_INTR1_MASK_DP2_IVCRX        0x00001133
#define INTR2_BIT1_SPD		0x2
#define INTR2_BIT2_AUD		0x4
#define INTR2_BIT4_UNREC	0x10
#define RX_DEPACK_INTR2_DP2_IVCRX        0x00001134
#define  RX_DEPACK_INTR2_MASK_DP2_IVCRX        0x00001135
#define INTR3_BIT34_HF_VSI	0x18
#define INTR3_BIT2_VSI		0x04
#define RX_DEPACK_INTR3_DP2_IVCRX        0x00001136
#define  RX_DEPACK_INTR3_MASK_DP2_IVCRX        0x00001137
#define RX_DEPACK_INTR4_DP2_IVCRX        0x00001138
#define  RX_DEPACK_INTR4_MASK_DP2_IVCRX        0x00001139
#define RX_DEPACK_INTR5_DP2_IVCRX        0x0000113a
#define  RX_DEPACK_INTR5_MASK_DP2_IVCRX        0x0000113b
#define RX_DEPACK_INTR6_DP2_IVCRX        0x0000113c
#define  RX_DEPACK_INTR6_MASK_DP2_IVCRX        0x0000113d
#define    AVIRX_TYPE_DP2_IVCRX        0x00001140
#define    AVIRX_VERS_DP2_IVCRX        0x00001141
#define  AVIRX_LENGTH_DP2_IVCRX        0x00001142
#define   AVIRX_CHSUM_DP2_IVCRX        0x00001143
#define  AVIRX_DBYTE1_DP2_IVCRX        0x00001144
#define  AVIRX_DBYTE2_DP2_IVCRX        0x00001145
#define  AVIRX_DBYTE3_DP2_IVCRX        0x00001146
#define  AVIRX_DBYTE4_DP2_IVCRX        0x00001147
#define  AVIRX_DBYTE5_DP2_IVCRX        0x00001148
#define  AVIRX_DBYTE6_DP2_IVCRX        0x00001149
#define  AVIRX_DBYTE7_DP2_IVCRX        0x0000114a
#define  AVIRX_DBYTE8_DP2_IVCRX        0x0000114b
#define  AVIRX_DBYTE9_DP2_IVCRX        0x0000114c
#define AVIRX_DBYTE10_DP2_IVCRX        0x0000114d
#define AVIRX_DBYTE11_DP2_IVCRX        0x0000114e
#define AVIRX_DBYTE12_DP2_IVCRX        0x0000114f
#define AVIRX_DBYTE13_DP2_IVCRX        0x00001150
#define AVIRX_DBYTE14_DP2_IVCRX        0x00001151
#define AVIRX_DBYTE15_DP2_IVCRX        0x00001152
#define RX_UNREC_CTRL_DP2_IVCRX        0x0000115e
#define  RX_UNREC_DEC_DP2_IVCRX        0x0000115f
#define    SPDRX_TYPE_DP2_IVCRX        0x00001160
#define    SPDRX_VERS_DP2_IVCRX        0x00001161
#define  SPDRX_LENGTH_DP2_IVCRX        0x00001162
#define   SPDRX_CHSUM_DP2_IVCRX        0x00001163
#define  SPDRX_DBYTE1_DP2_IVCRX        0x00001164
#define  SPDRX_DBYTE2_DP2_IVCRX        0x00001165
#define  SPDRX_DBYTE3_DP2_IVCRX        0x00001166
#define  SPDRX_DBYTE4_DP2_IVCRX        0x00001167
#define  SPDRX_DBYTE5_DP2_IVCRX        0x00001168
#define  SPDRX_DBYTE6_DP2_IVCRX        0x00001169
#define  SPDRX_DBYTE7_DP2_IVCRX        0x0000116a
#define  SPDRX_DBYTE8_DP2_IVCRX        0x0000116b
#define  SPDRX_DBYTE9_DP2_IVCRX        0x0000116c
#define SPDRX_DBYTE10_DP2_IVCRX        0x0000116d
#define SPDRX_DBYTE11_DP2_IVCRX        0x0000116e
#define SPDRX_DBYTE12_DP2_IVCRX        0x0000116f
#define SPDRX_DBYTE13_DP2_IVCRX        0x00001170
#define SPDRX_DBYTE14_DP2_IVCRX        0x00001171
#define SPDRX_DBYTE15_DP2_IVCRX        0x00001172
#define SPDRX_DBYTE16_DP2_IVCRX        0x00001173
#define SPDRX_DBYTE17_DP2_IVCRX        0x00001174
#define SPDRX_DBYTE18_DP2_IVCRX        0x00001175
#define SPDRX_DBYTE19_DP2_IVCRX        0x00001176
#define SPDRX_DBYTE20_DP2_IVCRX        0x00001177
#define SPDRX_DBYTE21_DP2_IVCRX        0x00001178
#define SPDRX_DBYTE22_DP2_IVCRX        0x00001179
#define SPDRX_DBYTE23_DP2_IVCRX        0x0000117a
#define SPDRX_DBYTE24_DP2_IVCRX        0x0000117b
#define SPDRX_DBYTE25_DP2_IVCRX        0x0000117c
#define SPDRX_DBYTE26_DP2_IVCRX        0x0000117d
#define SPDRX_DBYTE27_DP2_IVCRX        0x0000117e
#define       SPD_DEC_DP2_IVCRX        0x0000117f
#define    AUDRX_TYPE_DP2_IVCRX        0x00001180
#define    AUDRX_VERS_DP2_IVCRX        0x00001181
#define  AUDRX_LENGTH_DP2_IVCRX        0x00001182
#define   AUDRX_CHSUM_DP2_IVCRX        0x00001183
#define  AUDRX_DBYTE1_DP2_IVCRX        0x00001184
#define  AUDRX_DBYTE2_DP2_IVCRX        0x00001185
#define  AUDRX_DBYTE3_DP2_IVCRX        0x00001186
#define  AUDRX_DBYTE4_DP2_IVCRX        0x00001187
#define  AUDRX_DBYTE5_DP2_IVCRX        0x00001188
#define  AUDRX_DBYTE6_DP2_IVCRX        0x00001189
#define  AUDRX_DBYTE7_DP2_IVCRX        0x0000118a
#define  AUDRX_DBYTE8_DP2_IVCRX        0x0000118b
#define  AUDRX_DBYTE9_DP2_IVCRX        0x0000118c
#define AUDRX_DBYTE10_DP2_IVCRX        0x0000118d
#define AUDRX_DBYTE11_DP2_IVCRX        0x0000118e
#define AUDRX_DBYTE12_DP2_IVCRX        0x0000118f
#define AUDRX_DBYTE13_DP2_IVCRX        0x00001190
#define AUDRX_DBYTE14_DP2_IVCRX        0x00001191
#define AUDRX_DBYTE15_DP2_IVCRX        0x00001192
#define AUDRX_DBYTE16_DP2_IVCRX        0x00001193
#define AUDRX_DBYTE17_DP2_IVCRX        0x00001194
#define AUDRX_DBYTE18_DP2_IVCRX        0x00001195
#define AUDRX_DBYTE19_DP2_IVCRX        0x00001196
#define AUDRX_DBYTE20_DP2_IVCRX        0x00001197
#define AUDRX_DBYTE21_DP2_IVCRX        0x00001198
#define AUDRX_DBYTE22_DP2_IVCRX        0x00001199
#define AUDRX_DBYTE23_DP2_IVCRX        0x0000119a
#define AUDRX_DBYTE24_DP2_IVCRX        0x0000119b
#define AUDRX_DBYTE25_DP2_IVCRX        0x0000119c
#define AUDRX_DBYTE26_DP2_IVCRX        0x0000119d
#define AUDRX_DBYTE27_DP2_IVCRX        0x0000119e
#define   MPEGRX_TYPE_DP2_IVCRX        0x000011a0
#define   MPEGRX_VERS_DP2_IVCRX        0x000011a1
#define MPEGRX_LENGTH_DP2_IVCRX        0x000011a2
#define  MPEGRX_CHSUM_DP2_IVCRX        0x000011a3
#define MPEGRX_DBYTE1_DP2_IVCRX        0x000011a4
#define MPEGRX_DBYTE2_DP2_IVCRX        0x000011a5
#define MPEGRX_DBYTE3_DP2_IVCRX        0x000011a6
#define MPEGRX_DBYTE4_DP2_IVCRX        0x000011a7
#define MPEGRX_DBYTE5_DP2_IVCRX        0x000011a8
#define MPEGRX_DBYTE6_DP2_IVCRX        0x000011a9
#define MPEGRX_DBYTE7_DP2_IVCRX        0x000011aa
#define MPEGRX_DBYTE8_DP2_IVCRX        0x000011ab
#define MPEGRX_DBYTE9_DP2_IVCRX        0x000011ac
#define MPEGRX_DBYTE10_DP2_IVCRX        0x000011ad
#define MPEGRX_DBYTE11_DP2_IVCRX        0x000011ae
#define MPEGRX_DBYTE12_DP2_IVCRX        0x000011af
#define MPEGRX_DBYTE13_DP2_IVCRX        0x000011b0
#define MPEGRX_DBYTE14_DP2_IVCRX        0x000011b1
#define MPEGRX_DBYTE15_DP2_IVCRX        0x000011b2
#define MPEGRX_DBYTE16_DP2_IVCRX        0x000011b3
#define MPEGRX_DBYTE17_DP2_IVCRX        0x000011b4
#define MPEGRX_DBYTE18_DP2_IVCRX        0x000011b5
#define MPEGRX_DBYTE19_DP2_IVCRX        0x000011b6
#define MPEGRX_DBYTE20_DP2_IVCRX        0x000011b7
#define MPEGRX_DBYTE21_DP2_IVCRX        0x000011b8
#define MPEGRX_DBYTE22_DP2_IVCRX        0x000011b9
#define MPEGRX_DBYTE23_DP2_IVCRX        0x000011ba
#define MPEGRX_DBYTE24_DP2_IVCRX        0x000011bb
#define MPEGRX_DBYTE25_DP2_IVCRX        0x000011bc
#define MPEGRX_DBYTE26_DP2_IVCRX        0x000011bd
#define MPEGRX_DBYTE27_DP2_IVCRX        0x000011be
#define      MPEG_DEC_DP2_IVCRX        0x000011bf
#define RX_UNREC_BYTE1_DP2_IVCRX        0x000011c0
#define RX_UNREC_BYTE2_DP2_IVCRX        0x000011c1
#define RX_UNREC_BYTE3_DP2_IVCRX        0x000011c2
#define RX_UNREC_BYTE4_DP2_IVCRX        0x000011c3
#define RX_UNREC_BYTE5_DP2_IVCRX        0x000011c4
#define RX_UNREC_BYTE6_DP2_IVCRX        0x000011c5
#define RX_UNREC_BYTE7_DP2_IVCRX        0x000011c6
#define RX_UNREC_BYTE8_DP2_IVCRX        0x000011c7
#define RX_UNREC_BYTE9_DP2_IVCRX        0x000011c8
#define RX_UNREC_BYTE10_DP2_IVCRX        0x000011c9
#define RX_UNREC_BYTE11_DP2_IVCRX        0x000011ca
#define RX_UNREC_BYTE12_DP2_IVCRX        0x000011cb
#define RX_UNREC_BYTE13_DP2_IVCRX        0x000011cc
#define RX_UNREC_BYTE14_DP2_IVCRX        0x000011cd
#define RX_UNREC_BYTE15_DP2_IVCRX        0x000011ce
#define RX_UNREC_BYTE16_DP2_IVCRX        0x000011cf
#define RX_UNREC_BYTE17_DP2_IVCRX        0x000011d0
#define RX_UNREC_BYTE18_DP2_IVCRX        0x000011d1
#define RX_UNREC_BYTE19_DP2_IVCRX        0x000011d2
#define RX_UNREC_BYTE20_DP2_IVCRX        0x000011d3
#define RX_UNREC_BYTE21_DP2_IVCRX        0x000011d4
#define RX_UNREC_BYTE22_DP2_IVCRX        0x000011d5
#define RX_UNREC_BYTE23_DP2_IVCRX        0x000011d6
#define RX_UNREC_BYTE24_DP2_IVCRX        0x000011d7
#define RX_UNREC_BYTE25_DP2_IVCRX        0x000011d8
#define RX_UNREC_BYTE26_DP2_IVCRX        0x000011d9
#define RX_UNREC_BYTE27_DP2_IVCRX        0x000011da
#define RX_UNREC_BYTE28_DP2_IVCRX        0x000011db
#define RX_UNREC_BYTE29_DP2_IVCRX        0x000011dc
#define RX_UNREC_BYTE30_DP2_IVCRX        0x000011dd
#define RX_UNREC_BYTE31_DP2_IVCRX        0x000011de
#define    CPRX_BYTE1_DP2_IVCRX        0x000011df
#define  RX_ACP_BYTE1_DP2_IVCRX        0x000011e0
#define  RX_ACP_BYTE2_DP2_IVCRX        0x000011e1
#define  RX_ACP_BYTE3_DP2_IVCRX        0x000011e2
#define  RX_ACP_BYTE4_DP2_IVCRX        0x000011e3
#define  RX_ACP_BYTE5_DP2_IVCRX        0x000011e4
#define  RX_ACP_BYTE6_DP2_IVCRX        0x000011e5
#define  RX_ACP_BYTE7_DP2_IVCRX        0x000011e6
#define  RX_ACP_BYTE8_DP2_IVCRX        0x000011e7
#define  RX_ACP_BYTE9_DP2_IVCRX        0x000011e8
#define RX_ACP_BYTE10_DP2_IVCRX        0x000011e9
#define RX_ACP_BYTE11_DP2_IVCRX        0x000011ea
#define RX_ACP_BYTE12_DP2_IVCRX        0x000011eb
#define RX_ACP_BYTE13_DP2_IVCRX        0x000011ec
#define RX_ACP_BYTE14_DP2_IVCRX        0x000011ed
#define RX_ACP_BYTE15_DP2_IVCRX        0x000011ee
#define RX_ACP_BYTE16_DP2_IVCRX        0x000011ef
#define RX_ACP_BYTE17_DP2_IVCRX        0x000011f0
#define RX_ACP_BYTE18_DP2_IVCRX        0x000011f1
#define RX_ACP_BYTE19_DP2_IVCRX        0x000011f2
#define RX_ACP_BYTE20_DP2_IVCRX        0x000011f3
#define RX_ACP_BYTE21_DP2_IVCRX        0x000011f4
#define RX_ACP_BYTE22_DP2_IVCRX        0x000011f5
#define RX_ACP_BYTE23_DP2_IVCRX        0x000011f6
#define RX_ACP_BYTE24_DP2_IVCRX        0x000011f7
#define RX_ACP_BYTE25_DP2_IVCRX        0x000011f8
#define RX_ACP_BYTE26_DP2_IVCRX        0x000011f9
#define RX_ACP_BYTE27_DP2_IVCRX        0x000011fa
#define RX_ACP_BYTE28_DP2_IVCRX        0x000011fb
#define RX_ACP_BYTE29_DP2_IVCRX        0x000011fc
#define RX_ACP_BYTE30_DP2_IVCRX        0x000011fd
#define RX_ACP_BYTE31_DP2_IVCRX        0x000011fe
#define    RX_ACP_DEC_DP2_IVCRX        0x000011ff

//==================== DEPACK 03===============
#define     VSI_CTRL1_DP3_IVCRX        0x00001200
#define       VSI_ID1_DP3_IVCRX        0x00001201
#define       VSI_ID2_DP3_IVCRX        0x00001202
#define       VSI_ID3_DP3_IVCRX        0x00001203
#define       VSI_ID4_DP3_IVCRX        0x00001204
#define    VSI_PKT_ID_DP3_IVCRX        0x00001205
#define    AIF_PKT_ID_DP3_IVCRX        0x00001206
#define     VSI_CTRL2_DP3_IVCRX        0x00001207
#define      IF_CTRL1_DP3_IVCRX        0x00001208
#define      IF_CTRL2_DP3_IVCRX        0x00001209
#define      VSIF_ID1_DP3_IVCRX        0x0000120a
#define      VSIF_ID2_DP3_IVCRX        0x0000120b
#define      VSIF_ID3_DP3_IVCRX        0x0000120c
#define      VSIF_ID4_DP3_IVCRX        0x0000120d
#define     VSI_CTRL3_DP3_IVCRX        0x0000120e
#define     VSI_CTRL4_DP3_IVCRX        0x0000120f
#define    VSIRX_TYPE_DP3_IVCRX        0x00001220
#define    VSIRX_VERS_DP3_IVCRX        0x00001221
#define  VSIRX_LENGTH_DP3_IVCRX        0x00001222
#define  VSIRX_DBYTE0_DP3_IVCRX        0x00001223
#define  VSIRX_DBYTE1_DP3_IVCRX        0x00001224
#define  VSIRX_DBYTE2_DP3_IVCRX        0x00001225
#define  VSIRX_DBYTE3_DP3_IVCRX        0x00001226
#define  VSIRX_DBYTE4_DP3_IVCRX        0x00001227
#define  VSIRX_DBYTE5_DP3_IVCRX        0x00001228
#define  VSIRX_DBYTE6_DP3_IVCRX        0x00001229
#define  VSIRX_DBYTE7_DP3_IVCRX        0x0000122a
#define  VSIRX_DBYTE8_DP3_IVCRX        0x0000122b
#define  VSIRX_DBYTE9_DP3_IVCRX        0x0000122c
#define VSIRX_DBYTE10_DP3_IVCRX        0x0000122d
#define VSIRX_DBYTE11_DP3_IVCRX        0x0000122e
#define VSIRX_DBYTE12_DP3_IVCRX        0x0000122f
#define VSIRX_DBYTE13_DP3_IVCRX        0x00001230
#define VSIRX_DBYTE14_DP3_IVCRX        0x00001231
#define VSIRX_DBYTE15_DP3_IVCRX        0x00001232
#define VSIRX_DBYTE16_DP3_IVCRX        0x00001233
#define VSIRX_DBYTE17_DP3_IVCRX        0x00001234
#define VSIRX_DBYTE18_DP3_IVCRX        0x00001235
#define VSIRX_DBYTE19_DP3_IVCRX        0x00001236
#define VSIRX_DBYTE20_DP3_IVCRX        0x00001237
#define VSIRX_DBYTE21_DP3_IVCRX        0x00001238
#define VSIRX_DBYTE22_DP3_IVCRX        0x00001239
#define VSIRX_DBYTE23_DP3_IVCRX        0x0000123a
#define VSIRX_DBYTE24_DP3_IVCRX        0x0000123b
#define VSIRX_DBYTE25_DP3_IVCRX        0x0000123c
#define VSIRX_DBYTE26_DP3_IVCRX        0x0000123d
#define VSIRX_DBYTE27_DP3_IVCRX        0x0000123e
#define RX_ISRC1_TYPE_DP3_IVCRX        0x00001240
#define RX_ISRC1_VERS_DP3_IVCRX        0x00001241
#define RX_ISRC1_LENGTH_DP3_IVCRX        0x00001242
#define RX_ISRC1_DBYTE0_DP3_IVCRX        0x00001243
#define RX_ISRC1_DBYTE1_DP3_IVCRX        0x00001244
#define RX_ISRC1_DBYTE2_DP3_IVCRX        0x00001245
#define RX_ISRC1_DBYTE3_DP3_IVCRX        0x00001246
#define RX_ISRC1_DBYTE4_DP3_IVCRX        0x00001247
#define RX_ISRC1_DBYTE5_DP3_IVCRX        0x00001248
#define RX_ISRC1_DBYTE6_DP3_IVCRX        0x00001249
#define RX_ISRC1_DBYTE7_DP3_IVCRX        0x0000124a
#define RX_ISRC1_DBYTE8_DP3_IVCRX        0x0000124b
#define RX_ISRC1_DBYTE9_DP3_IVCRX        0x0000124c
#define RX_ISRC1_DBYTE10_DP3_IVCRX        0x0000124d
#define RX_ISRC1_DBYTE11_DP3_IVCRX        0x0000124e
#define RX_ISRC1_DBYTE12_DP3_IVCRX        0x0000124f
#define RX_ISRC1_DBYTE13_DP3_IVCRX        0x00001250
#define RX_ISRC1_DBYTE14_DP3_IVCRX        0x00001251
#define RX_ISRC1_DBYTE15_DP3_IVCRX        0x00001252
#define  RX_ISRC1_DEC_DP3_IVCRX        0x0000125f
#define RX_ISRC2_TYPE_DP3_IVCRX        0x00001260
#define RX_ISRC2_VERS_DP3_IVCRX        0x00001261
#define RX_ISRC2_LENGTH_DP3_IVCRX        0x00001262
#define RX_ISRC2_DBYTE0_DP3_IVCRX        0x00001263
#define RX_ISRC2_DBYTE1_DP3_IVCRX        0x00001264
#define RX_ISRC2_DBYTE2_DP3_IVCRX        0x00001265
#define RX_ISRC2_DBYTE3_DP3_IVCRX        0x00001266
#define RX_ISRC2_DBYTE4_DP3_IVCRX        0x00001267
#define RX_ISRC2_DBYTE5_DP3_IVCRX        0x00001268
#define RX_ISRC2_DBYTE6_DP3_IVCRX        0x00001269
#define RX_ISRC2_DBYTE7_DP3_IVCRX        0x0000126a
#define RX_ISRC2_DBYTE8_DP3_IVCRX        0x0000126b
#define RX_ISRC2_DBYTE9_DP3_IVCRX        0x0000126c
#define RX_ISRC2_DBYTE10_DP3_IVCRX        0x0000126d
#define RX_ISRC2_DBYTE11_DP3_IVCRX        0x0000126e
#define RX_ISRC2_DBYTE12_DP3_IVCRX        0x0000126f
#define RX_ISRC2_DBYTE13_DP3_IVCRX        0x00001270
#define RX_ISRC2_DBYTE14_DP3_IVCRX        0x00001271
#define RX_ISRC2_DBYTE15_DP3_IVCRX        0x00001272
#define  RX_ISRC2_DEC_DP3_IVCRX        0x0000127f
#define   RX_GCP_TYPE_DP3_IVCRX        0x00001280
#define   RX_GCP_VERS_DP3_IVCRX        0x00001281
#define RX_GCP_LENGTH_DP3_IVCRX        0x00001282
#define RX_GCP_DBYTE0_DP3_IVCRX        0x00001283
#define RX_GCP_DBYTE1_DP3_IVCRX        0x00001284
#define RX_GCP_DBYTE2_DP3_IVCRX        0x00001285
#define RX_GCP_DBYTE3_DP3_IVCRX        0x00001286
#define RX_GCP_DBYTE4_DP3_IVCRX        0x00001287
#define RX_GCP_DBYTE5_DP3_IVCRX        0x00001288
#define RX_GCP_DBYTE6_DP3_IVCRX        0x00001289
#define   RX_GCP_CTRL_DP3_IVCRX        0x0000128f
#define HF_VSIRX_TYPE_DP3_IVCRX        0x00001290
#define HF_VSIRX_VERS_DP3_IVCRX        0x00001291
#define HF_VSIRX_LENGTH_DP3_IVCRX        0x00001292
#define HF_VSIRX_DBYTE0_DP3_IVCRX        0x00001293
#define HF_VSIRX_DBYTE1_DP3_IVCRX        0x00001294
#define HF_VSIRX_DBYTE2_DP3_IVCRX        0x00001295
#define HF_VSIRX_DBYTE3_DP3_IVCRX        0x00001296
#define HF_VSIRX_DBYTE4_DP3_IVCRX        0x00001297
#define HF_VSIRX_DBYTE5_DP3_IVCRX        0x00001298
#define HF_VSIRX_DBYTE6_DP3_IVCRX        0x00001299
#define HF_VSIRX_DBYTE7_DP3_IVCRX        0x0000129a
#define HF_VSIRX_DBYTE8_DP3_IVCRX        0x0000129b
#define HF_VSIRX_DBYTE9_DP3_IVCRX        0x0000129c
#define HF_VSIRX_DBYTE10_DP3_IVCRX        0x0000129d
#define HF_VSIRX_DBYTE11_DP3_IVCRX        0x0000129e
#define HF_VSIRX_DBYTE12_DP3_IVCRX        0x0000129f
#define HF_VSIRX_DBYTE13_DP3_IVCRX        0x000012a0
#define HF_VSIRX_DBYTE14_DP3_IVCRX        0x000012a1
#define HF_VSIRX_DBYTE15_DP3_IVCRX        0x000012a2
#define HF_VSIRX_DBYTE16_DP3_IVCRX        0x000012a3
#define HF_VSIRX_DBYTE17_DP3_IVCRX        0x000012a4
#define HF_VSIRX_DBYTE18_DP3_IVCRX        0x000012a5
#define HF_VSIRX_DBYTE19_DP3_IVCRX        0x000012a6
#define HF_VSIRX_DBYTE20_DP3_IVCRX        0x000012a7
#define HF_VSIRX_DBYTE21_DP3_IVCRX        0x000012a8
#define HF_VSIRX_DBYTE22_DP3_IVCRX        0x000012a9
#define HF_VSIRX_DBYTE23_DP3_IVCRX        0x000012aa
#define HF_VSIRX_DBYTE24_DP3_IVCRX        0x000012ab
#define HF_VSIRX_DBYTE25_DP3_IVCRX        0x000012ac
#define HF_VSIRX_DBYTE26_DP3_IVCRX        0x000012ad
#define HF_VSIRX_DBYTE27_DP3_IVCRX        0x000012ae
#define HF_VSI_PKT_ID_DP3_IVCRX        0x000012af
#define   HF_VSIF_ID1_DP3_IVCRX        0x000012b0
#define   HF_VSIF_ID2_DP3_IVCRX        0x000012b1
#define   HF_VSIF_ID3_DP3_IVCRX        0x000012b2
#define  HF_VSIF_CTRL_DP3_IVCRX        0x000012b3
#define METADATA_HEADER_BYTE0_DP3_IVCRX        0x000012be
#define METADATA_HEADER_BYTE1_DP3_IVCRX        0x000012bf
#define METADATA_HEADER_BYTE2_DP3_IVCRX        0x000012c0
#define METADATA_DBYTE0_DP3_IVCRX        0x000012c1
#define METADATA_DBYTE1_DP3_IVCRX        0x000012c2
#define METADATA_DBYTE2_DP3_IVCRX        0x000012c3
#define METADATA_DBYTE3_DP3_IVCRX        0x000012c4
#define METADATA_DBYTE4_DP3_IVCRX        0x000012c5
#define METADATA_DBYTE5_DP3_IVCRX        0x000012c6
#define METADATA_DBYTE6_DP3_IVCRX        0x000012c7
#define METADATA_DBYTE7_DP3_IVCRX        0x000012c8
#define METADATA_DBYTE8_DP3_IVCRX        0x000012c9
#define METADATA_DBYTE9_DP3_IVCRX        0x000012ca
#define METADATA_DBYTE10_DP3_IVCRX        0x000012cb
#define METADATA_DBYTE11_DP3_IVCRX        0x000012cc
#define METADATA_DBYTE12_DP3_IVCRX        0x000012cd
#define METADATA_DBYTE13_DP3_IVCRX        0x000012ce
#define METADATA_DBYTE14_DP3_IVCRX        0x000012cf
#define METADATA_DBYTE15_DP3_IVCRX        0x000012d0
#define METADATA_DBYTE16_DP3_IVCRX        0x000012d1
#define METADATA_DBYTE17_DP3_IVCRX        0x000012d2
#define METADATA_DBYTE18_DP3_IVCRX        0x000012d3
#define METADATA_DBYTE19_DP3_IVCRX        0x000012d4
#define METADATA_DBYTE20_DP3_IVCRX        0x000012d5
#define METADATA_DBYTE21_DP3_IVCRX        0x000012d6
#define METADATA_DBYTE22_DP3_IVCRX        0x000012d7
#define METADATA_DBYTE23_DP3_IVCRX        0x000012d8
#define METADATA_DBYTE24_DP3_IVCRX        0x000012d9
#define METADATA_DBYTE25_DP3_IVCRX        0x000012da
#define METADATA_DBYTE26_DP3_IVCRX        0x000012db
#define METADATA_DBYTE27_DP3_IVCRX        0x000012dc

//==================== DEPACK 0B===============
#define     EMP_CTRL1_DP0B_IVCRX       0x00001300
#define EMP_AUTO_CLR1_DP0B_IVCRX       0x00001301
#define RX_DEPACK2_INTR0_DP0B_IVCRX       0x00001308
#define RX_DEPACK2_INTR1_DP0B_IVCRX       0x00001309
#define RX_DEPACK2_INTR0_MASK_DP0B_IVCRX       0x0000130a
#define RX_DEPACK2_INTR1_MASK_DP0B_IVCRX       0x0000130b
#define RX_DEPACK2_INTR2_DP0B_IVCRX       0x0000130c
#define RX_DEPACK2_INTR2_MASK_DP0B_IVCRX       0x0000130d
#define RX_CVT_EMP_DBYTE0_DP0B_IVCRX       0x00001310
#define RX_CVT_EMP_DBYTE1_DP0B_IVCRX       0x00001311
#define RX_CVT_EMP_DBYTE2_DP0B_IVCRX       0x00001312
#define RX_CVT_EMP_DBYTE3_DP0B_IVCRX       0x00001313
#define RX_CVT_EMP_DBYTE4_DP0B_IVCRX       0x00001314
#define RX_CVT_EMP_DBYTE5_DP0B_IVCRX       0x00001315
#define RX_CVT_EMP_DBYTE6_DP0B_IVCRX       0x00001316
#define RX_CVT_EMP_DBYTE7_DP0B_IVCRX       0x00001317
#define RX_CVT_EMP_DBYTE8_DP0B_IVCRX       0x00001318
#define RX_CVT_EMP_DBYTE9_DP0B_IVCRX       0x00001319
#define RX_CVT_EMP_DBYTE10_DP0B_IVCRX       0x0000131a
#define RX_CVT_EMP_DBYTE11_DP0B_IVCRX       0x0000131b
#define RX_CVT_EMP_DBYTE12_DP0B_IVCRX       0x0000131c
#define RX_CVT_EMP_DBYTE13_DP0B_IVCRX       0x0000131d
#define RX_CVT_EMP_DBYTE14_DP0B_IVCRX       0x0000131e
#define RX_CVT_EMP_DBYTE15_DP0B_IVCRX       0x0000131f
#define RX_CVT_EMP_DBYTE16_DP0B_IVCRX       0x00001320
#define RX_CVT_EMP_DBYTE17_DP0B_IVCRX       0x00001321
#define RX_CVT_EMP_DBYTE18_DP0B_IVCRX       0x00001322
#define RX_CVT_EMP_DBYTE19_DP0B_IVCRX       0x00001323
#define RX_CVT_EMP_DBYTE20_DP0B_IVCRX       0x00001324
#define RX_CVT_EMP_DBYTE21_DP0B_IVCRX       0x00001325
#define RX_CVT_EMP_DBYTE22_DP0B_IVCRX       0x00001326
#define RX_CVT_EMP_DBYTE23_DP0B_IVCRX       0x00001327
#define RX_CVT_EMP_DBYTE24_DP0B_IVCRX       0x00001328
#define RX_CVT_EMP_DBYTE25_DP0B_IVCRX       0x00001329
#define RX_CVT_EMP_DBYTE26_DP0B_IVCRX       0x0000132a
#define RX_CVT_EMP_DBYTE27_DP0B_IVCRX       0x0000132b
#define RX_CVT_EMP_DBYTE28_DP0B_IVCRX       0x0000132c
#define RX_CVT_EMP_DBYTE29_DP0B_IVCRX       0x0000132d
#define RX_CVT_EMP_DBYTE30_DP0B_IVCRX       0x0000132e
#define RX_CVT_EMP_DBYTE31_DP0B_IVCRX       0x0000132f
#define RX_CVT_EMP_DBYTE32_DP0B_IVCRX       0x00001330
#define RX_CVT_EMP_DBYTE33_DP0B_IVCRX       0x00001331
#define RX_CVT_EMP_DBYTE34_DP0B_IVCRX       0x00001332
#define RX_CVT_EMP_DBYTE35_DP0B_IVCRX       0x00001333
#define RX_CVT_EMP_DBYTE36_DP0B_IVCRX       0x00001334
#define RX_CVT_EMP_DBYTE37_DP0B_IVCRX       0x00001335
#define RX_CVT_EMP_DBYTE38_DP0B_IVCRX       0x00001336
#define RX_CVT_EMP_DBYTE39_DP0B_IVCRX       0x00001337
#define RX_CVT_EMP_DBYTE40_DP0B_IVCRX       0x00001338
#define RX_CVT_EMP_DBYTE41_DP0B_IVCRX       0x00001339
#define RX_CVT_EMP_DBYTE42_DP0B_IVCRX       0x0000133a
#define RX_CVT_EMP_DBYTE43_DP0B_IVCRX       0x0000133b
#define RX_CVT_EMP_DBYTE44_DP0B_IVCRX       0x0000133c
#define RX_CVT_EMP_DBYTE45_DP0B_IVCRX       0x0000133d
#define RX_CVT_EMP_DBYTE46_DP0B_IVCRX       0x0000133e
#define RX_CVT_EMP_DBYTE47_DP0B_IVCRX       0x0000133f
#define RX_CVT_EMP_DBYTE48_DP0B_IVCRX       0x00001340
#define RX_CVT_EMP_DBYTE49_DP0B_IVCRX       0x00001341
#define RX_CVT_EMP_DBYTE50_DP0B_IVCRX       0x00001342
#define RX_CVT_EMP_DBYTE51_DP0B_IVCRX       0x00001343
#define RX_CVT_EMP_DBYTE52_DP0B_IVCRX       0x00001344
#define RX_CVT_EMP_DBYTE53_DP0B_IVCRX       0x00001345
#define RX_CVT_EMP_DBYTE54_DP0B_IVCRX       0x00001346
#define RX_CVT_EMP_DBYTE55_DP0B_IVCRX       0x00001347
#define RX_CVT_EMP_DBYTE56_DP0B_IVCRX       0x00001348
#define RX_CVT_EMP_DBYTE57_DP0B_IVCRX       0x00001349
#define RX_CVT_EMP_DBYTE58_DP0B_IVCRX       0x0000134a
#define RX_CVT_EMP_DBYTE59_DP0B_IVCRX       0x0000134b
#define RX_CVT_EMP_DBYTE60_DP0B_IVCRX       0x0000134c
#define RX_CVT_EMP_DBYTE61_DP0B_IVCRX       0x0000134d
#define RX_CVT_EMP_DBYTE62_DP0B_IVCRX       0x0000134e
#define RX_CVT_EMP_DBYTE63_DP0B_IVCRX       0x0000134f
#define RX_CVT_EMP_DBYTE64_DP0B_IVCRX       0x00001350
#define RX_CVT_EMP_DBYTE65_DP0B_IVCRX       0x00001351
#define RX_CVT_EMP_DBYTE66_DP0B_IVCRX       0x00001352
#define RX_CVT_EMP_DBYTE67_DP0B_IVCRX       0x00001353
#define RX_CVT_EMP_DBYTE68_DP0B_IVCRX       0x00001354
#define RX_CVT_EMP_DBYTE69_DP0B_IVCRX       0x00001355
#define RX_CVT_EMP_DBYTE70_DP0B_IVCRX       0x00001356
#define RX_CVT_EMP_DBYTE71_DP0B_IVCRX       0x00001357
#define RX_CVT_EMP_DBYTE72_DP0B_IVCRX       0x00001358
#define RX_CVT_EMP_DBYTE73_DP0B_IVCRX       0x00001359
#define RX_CVT_EMP_DBYTE74_DP0B_IVCRX       0x0000135a
#define RX_CVT_EMP_DBYTE75_DP0B_IVCRX       0x0000135b
#define RX_CVT_EMP_DBYTE76_DP0B_IVCRX       0x0000135c
#define RX_CVT_EMP_DBYTE77_DP0B_IVCRX       0x0000135d
#define RX_CVT_EMP_DBYTE78_DP0B_IVCRX       0x0000135e
#define RX_CVT_EMP_DBYTE79_DP0B_IVCRX       0x0000135f
#define RX_CVT_EMP_DBYTE80_DP0B_IVCRX       0x00001360
#define RX_CVT_EMP_DBYTE81_DP0B_IVCRX       0x00001361
#define RX_CVT_EMP_DBYTE82_DP0B_IVCRX       0x00001362
#define RX_CVT_EMP_DBYTE83_DP0B_IVCRX       0x00001363
#define RX_CVT_EMP_DBYTE84_DP0B_IVCRX       0x00001364
#define RX_CVT_EMP_DBYTE85_DP0B_IVCRX       0x00001365
#define RX_CVT_EMP_DBYTE86_DP0B_IVCRX       0x00001366
#define RX_CVT_EMP_DBYTE87_DP0B_IVCRX       0x00001367
#define RX_CVT_EMP_DBYTE88_DP0B_IVCRX       0x00001368
#define RX_CVT_EMP_DBYTE89_DP0B_IVCRX       0x00001369
#define RX_CVT_EMP_DBYTE90_DP0B_IVCRX       0x0000136a
#define RX_CVT_EMP_DBYTE91_DP0B_IVCRX       0x0000136b
#define RX_CVT_EMP_DBYTE92_DP0B_IVCRX       0x0000136c
#define RX_CVT_EMP_DBYTE93_DP0B_IVCRX       0x0000136d
#define RX_CVT_EMP_DBYTE94_DP0B_IVCRX       0x0000136e
#define RX_CVT_EMP_DBYTE95_DP0B_IVCRX       0x0000136f
#define RX_CVT_EMP_DBYTE96_DP0B_IVCRX       0x00001370
#define RX_CVT_EMP_DBYTE97_DP0B_IVCRX       0x00001371
#define RX_CVT_EMP_DBYTE98_DP0B_IVCRX       0x00001372
#define RX_CVT_EMP_DBYTE99_DP0B_IVCRX       0x00001373
#define   RX_CVT_EMP_DBYTE100_DP0B_IVCRX       0x00001374
#define   RX_CVT_EMP_DBYTE101_DP0B_IVCRX       0x00001375
#define   RX_CVT_EMP_DBYTE102_DP0B_IVCRX       0x00001376
#define   RX_CVT_EMP_DBYTE103_DP0B_IVCRX       0x00001377
#define   RX_CVT_EMP_DBYTE104_DP0B_IVCRX       0x00001378
#define   RX_CVT_EMP_DBYTE105_DP0B_IVCRX       0x00001379
#define   RX_CVT_EMP_DBYTE106_DP0B_IVCRX       0x0000137a
#define   RX_CVT_EMP_DBYTE107_DP0B_IVCRX       0x0000137b
#define   RX_CVT_EMP_DBYTE108_DP0B_IVCRX       0x0000137c
#define   RX_CVT_EMP_DBYTE109_DP0B_IVCRX       0x0000137d
#define   RX_CVT_EMP_DBYTE110_DP0B_IVCRX       0x0000137e
#define   RX_CVT_EMP_DBYTE111_DP0B_IVCRX       0x0000137f
#define   RX_CVT_EMP_DBYTE112_DP0B_IVCRX       0x00001380
#define   RX_CVT_EMP_DBYTE113_DP0B_IVCRX       0x00001381
#define   RX_CVT_EMP_DBYTE114_DP0B_IVCRX       0x00001382
#define   RX_CVT_EMP_DBYTE115_DP0B_IVCRX       0x00001383
#define   RX_CVT_EMP_DBYTE116_DP0B_IVCRX       0x00001384
#define   RX_CVT_EMP_DBYTE117_DP0B_IVCRX       0x00001385
#define   RX_CVT_EMP_DBYTE118_DP0B_IVCRX       0x00001386
#define   RX_CVT_EMP_DBYTE119_DP0B_IVCRX       0x00001387
#define   RX_CVT_EMP_DBYTE120_DP0B_IVCRX       0x00001388
#define   RX_CVT_EMP_DBYTE121_DP0B_IVCRX       0x00001389
#define   RX_CVT_EMP_DBYTE122_DP0B_IVCRX       0x0000138a
#define   RX_CVT_EMP_DBYTE123_DP0B_IVCRX       0x0000138b
#define   RX_CVT_EMP_DBYTE124_DP0B_IVCRX       0x0000138c
#define   RX_CVT_EMP_DBYTE125_DP0B_IVCRX       0x0000138d
#define   RX_CVT_EMP_DBYTE126_DP0B_IVCRX       0x0000138e
#define   RX_CVT_EMP_DBYTE127_DP0B_IVCRX       0x0000138f
#define   RX_CVT_EMP_DBYTE128_DP0B_IVCRX       0x00001390
#define   RX_CVT_EMP_DBYTE129_DP0B_IVCRX       0x00001391
#define   RX_CVT_EMP_DBYTE130_DP0B_IVCRX       0x00001392
#define   RX_CVT_EMP_DBYTE131_DP0B_IVCRX       0x00001393
#define   RX_CVT_EMP_DBYTE132_DP0B_IVCRX       0x00001394
#define   RX_CVT_EMP_DBYTE133_DP0B_IVCRX       0x00001395
#define   RX_CVT_EMP_DBYTE134_DP0B_IVCRX       0x00001396
#define   RX_CVT_EMP_DBYTE135_DP0B_IVCRX       0x00001397
#define  RX_CVT_EMP_SI0_BYTE0_DP0B_IVCRX       0x00001398
#define  RX_CVT_EMP_SI0_BYTE2_DP0B_IVCRX       0x00001399
#define  RX_CVT_EMP_SI0_BYTE3_DP0B_IVCRX       0x0000139a
#define  RX_CVT_EMP_SI0_BYTE4_DP0B_IVCRX       0x0000139b
#define  RX_CVT_EMP_SI0_BYTE5_DP0B_IVCRX       0x0000139c
#define  RX_CVT_EMP_SI0_BYTE6_DP0B_IVCRX       0x0000139d
#define RX_VT_EMP_DBYTE0_DP0B_IVCRX       0x000013a0
#define RX_VT_EMP_DBYTE1_DP0B_IVCRX       0x000013a1
#define RX_VT_EMP_DBYTE2_DP0B_IVCRX       0x000013a2
#define RX_VT_EMP_DBYTE3_DP0B_IVCRX       0x000013a3
#define HDR_EMP_DAT_STAG_0_DP0B_IVCRX       0x000013a8
#define HDR_EMP_DAT_STAG_1_DP0B_IVCRX       0x000013a9
#define HDR_EMP_DAT_SLEN_0_DP0B_IVCRX       0x000013aa
#define HDR_EMP_DAT_SLEN_1_DP0B_IVCRX       0x000013ab
#define RX_VSDS_EMP_DBYTE0_DP0B_IVCRX       0x000013b0
#define RX_VSDS_EMP_DBYTE1_DP0B_IVCRX       0x000013b1
#define RX_VSDS_EMP_DBYTE2_DP0B_IVCRX       0x000013b2
#define  RX_CVT_EMP_TOUT_FCNT_DP0B_IVCRX       0x000013b8
#define   RX_VT_EMP_TOUT_FCNT_DP0B_IVCRX       0x000013b9
#define  RX_HDR_EMP_TOUT_FCNT_DP0B_IVCRX       0x000013ba
#define RX_VSDB_EMP_TOUT_FCNT_DP0B_IVCRX       0x000013bb

//==================== AUD ===============
#define  RX_ACR_CTRL1_AUD_IVCRX        0x00001400
#define  AAC_MCLK_SEL_AUD_IVCRX        0x00001401
#define  RX_FREQ_SVAL_AUD_IVCRX        0x00001402
#define    RX_N_SVAL1_AUD_IVCRX        0x00001403
#define    RX_N_SVAL2_AUD_IVCRX        0x00001404
#define    RX_N_SVAL3_AUD_IVCRX        0x00001405
#define    RX_N_HVAL1_AUD_IVCRX        0x00001406
#define    RX_N_HVAL2_AUD_IVCRX        0x00001407
#define    RX_N_HVAL3_AUD_IVCRX        0x00001408
#define  RX_CTS_SVAL1_AUD_IVCRX        0x00001409
#define  RX_CTS_SVAL2_AUD_IVCRX        0x0000140a
#define  RX_CTS_SVAL3_AUD_IVCRX        0x0000140b
#define  RX_CTS_HVAL1_AUD_IVCRX        0x0000140c
#define  RX_CTS_HVAL2_AUD_IVCRX        0x0000140d
#define  RX_CTS_HVAL3_AUD_IVCRX        0x0000140e
#define  RX_UPLL_SVAL_AUD_IVCRX        0x0000140f
#define  RX_UPLL_HVAL_AUD_IVCRX        0x00001410
#define  RX_POST_SVAL_AUD_IVCRX        0x00001411
#define  RX_POST_HVAL_AUD_IVCRX        0x00001412
#define RX_LK_WIN_SVAL_AUD_IVCRX        0x00001413
#define RX_LK_THRS_SVAL1_AUD_IVCRX        0x00001414
#define RX_LK_THRS_SVAL2_AUD_IVCRX        0x00001415
#define RX_LK_THRS_SVAL3_AUD_IVCRX        0x00001416
#define    RX_TCLK_FS_AUD_IVCRX        0x00001417
#define  RX_ACR_CTRL3_AUD_IVCRX        0x00001418
#define      RX_CHST1_AUD_IVCRX        0x00001419
#define      RX_CHST2_AUD_IVCRX        0x0000141a
#define     RX_CHST3a_AUD_IVCRX        0x0000141b
#define      RX_CHST4_AUD_IVCRX        0x0000141c
#define      RX_CHST5_AUD_IVCRX        0x0000141d
#define      RX_CHST6_AUD_IVCRX        0x0000141e
#define      RX_CHST7_AUD_IVCRX        0x0000141f
#define  AUD4AAC0_HLP_AUD_IVCRX        0x00001425
#define  RX_I2S_CTRL1_AUD_IVCRX        0x00001426
#define  RX_I2S_CTRL2_AUD_IVCRX        0x00001427
#define    RX_I2S_MAP_AUD_IVCRX        0x00001428
#define RX_AUDRX_CTRL_AUD_IVCRX        0x00001429
#define   RX_MUTE_DIV_AUD_IVCRX        0x0000142a
#define      RX_SW_OW_AUD_IVCRX        0x0000142b
#define    RX_OW_15_8_AUD_IVCRX        0x0000142c
#define  RX_AUDO_MUTE_AUD_IVCRX        0x0000142d
#define  RX_AUDP_MUTE_AUD_IVCRX        0x00001437
#define    RX_PD_SYS3_AUD_IVCRX        0x0000143d
#define  RX_TDM_CTRL1_AUD_IVCRX        0x00001440
#define  RX_TDM_CTRL2_AUD_IVCRX        0x00001441
#define   RX_3D_CTRL1_AUD_IVCRX        0x00001442
#define   RX_3D_CTRL2_AUD_IVCRX        0x00001443
#define RX_3D_AUDO_MUTE1_AUD_IVCRX        0x00001444
#define RX_3D_AUDO_MUTE2_AUD_IVCRX        0x00001445
#define RX_3D_I2S_MAP1_AUD_IVCRX        0x00001446
#define RX_3D_I2S_MAP2_AUD_IVCRX        0x00001447
#define RX_3D_I2S_MAP3_AUD_IVCRX        0x00001448
#define RX_3D_I2S_MAP4_AUD_IVCRX        0x00001449
#define RX_3D_I2S_MAP5_AUD_IVCRX        0x0000144a
#define RX_3D_I2S_MAP6_AUD_IVCRX        0x0000144b
#define RX_3D_I2S_MAP7_AUD_IVCRX        0x0000144c
#define  RX_3D_SW_OW1_AUD_IVCRX        0x0000144d
#define  RX_3D_SW_OW2_AUD_IVCRX        0x0000144e
#define VID_XPCLK_MULT_AUD_IVCRX        0x00001466
#define VID_XPCLK_BASE_AUD_IVCRX        0x00001467
#define  VID_XPCLK_EN_AUD_IVCRX        0x00001468
#define   VID_XPBASE0_AUD_IVCRX        0x00001469
#define   VID_XPBASE1_AUD_IVCRX        0x0000146a
#define   VID_XPBASE2_AUD_IVCRX        0x0000146b
#define  VID_XP_THRSH_AUD_IVCRX        0x0000146c
#define RX_VID_XPCNT1_AUD_IVCRX        0x0000146d
#define RX_VID_XPCNT2_AUD_IVCRX        0x0000146e
#define RX_VID_XPCNT3_AUD_IVCRX        0x0000146f
#define  RX_APLL_POLE_AUD_IVCRX        0x00001488
#define  RX_APLL_CLIP_AUD_IVCRX        0x00001489
#define RX_APLL_DINOVR_AUD_IVCRX        0x0000148a
#define RX_APLL_USOVR_AUD_IVCRX        0x0000148b
#define RX_APLL_OVRCTRL_AUD_IVCRX        0x0000148c
#define  RX_APLL_STAT_AUD_IVCRX        0x0000148d
#define  RX_NACR_CRTL_AUD_IVCRX        0x00001490
#define  RX_NACR_STS1_AUD_IVCRX        0x00001491
#define  RX_NACR_STS2_AUD_IVCRX        0x00001492
#define  RX_NACR_STS3_AUD_IVCRX        0x00001493
#define  RX_NACR_STS4_AUD_IVCRX        0x00001494
#define  RX_NACR_STS5_AUD_IVCRX        0x00001495
#define  RX_NACR_STS6_AUD_IVCRX        0x00001496
#define  RX_NACR_STS7_AUD_IVCRX        0x00001497
#define  RX_NACR_STS8_AUD_IVCRX        0x00001498
#define  RX_NACR_STS9_AUD_IVCRX        0x00001499
#define RX_APLL_REFDIV_AUD_IVCRX        0x0000149a
#define RX_APLL_POSTDIV_AUD_IVCRX        0x0000149b
#define RX_APLL_PD_CTRL_AUD_IVCRX        0x0000149c
#define RX_SM_NAPLL_STAT_AUD_IVCRX        0x0000149d
#define RX_AVG_WINDOW_AUD_IVCRX        0x000014a0
#define DACR_MCLK_CTRL_AUD_IVCRX        0x000014a1
#define DACR_REF_CLK_SEL_AUD_IVCRX        0x000014a2
#define     AEC4_CTRL_AUD_IVCRX        0x000014b1
#define     AEC3_CTRL_AUD_IVCRX        0x000014b2
#define     AEC2_CTRL_AUD_IVCRX        0x000014b3
#define     AEC1_CTRL_AUD_IVCRX        0x000014b4
#define     AEC0_CTRL_AUD_IVCRX        0x000014b5
#define    RX_AEC_EN1_AUD_IVCRX        0x000014b6
#define    RX_AEC_EN2_AUD_IVCRX        0x000014b7
#define    RX_AEC_EN3_AUD_IVCRX        0x000014b8
#define  RX_SYS_PSTOP_AUD_IVCRX        0x000014ba
#define   AAC_CSC_ERR_AUD_IVCRX        0x000014f3
#define   AAC_VSC_ERR_AUD_IVCRX        0x000014f4
#define   AAC_ASC_ERR_AUD_IVCRX        0x000014f5
#define AAC_EXP_CAPT_L_AUD_IVCRX        0x000014fb
#define AAC_EXP_CAPT_M_AUD_IVCRX        0x000014fc
#define AAC_EXP_CAPT_H_AUD_IVCRX        0x000014fd
#define     ACRGCTRL0_AUD_IVCRX        0x000014fe

//==================== M42H ===============
#define  H21RXSB_CTRL_M42H_IVCRX       0x00001500
#define H21RXSB_RST_CTRL_M42H_IVCRX       0x00001501
#define   H21RXSB_KSR_M42H_IVCRX       0x00001502
#define  H21RXSB_KSSB_M42H_IVCRX       0x00001503
#define H21RXSB_SRPRD_M42H_IVCRX       0x00001504
#define   H21RXSB_GN0_M42H_IVCRX       0x00001505
#define   H21RXSB_GN1_M42H_IVCRX       0x00001506
#define   H21RXSB_GN2_M42H_IVCRX       0x00001507
#define  H21RXSB_ITH0_M42H_IVCRX       0x00001508
#define  H21RXSB_ITH1_M42H_IVCRX       0x00001509
#define  H21RXSB_ITH2_M42H_IVCRX       0x0000150a
#define  H21RXSB_ITH3_M42H_IVCRX       0x0000150b
#define  H21RXSB_CTH0_M42H_IVCRX       0x0000150c
#define  H21RXSB_CTH1_M42H_IVCRX       0x0000150d
#define  H21RXSB_CTH2_M42H_IVCRX       0x0000150e
#define   H21RXSB_AWW_M42H_IVCRX       0x0000150f
#define H21RXSB_DIFF1T_M42H_IVCRX       0x00001510
#define  H21RXSB_D2TL_M42H_IVCRX       0x00001511
#define  H21RXSB_D2TH_M42H_IVCRX       0x00001512
#define H21RXSB_PREDIV_M42H_IVCRX       0x00001513
#define H21RXSB_EFDIG_M42H_IVCRX       0x00001514
#define H21RXSB_STEPF0_M42H_IVCRX       0x00001515
#define H21RXSB_STEPF1_M42H_IVCRX       0x00001516
#define H21RXSB_STEPF2_M42H_IVCRX       0x00001517
#define  H21RXSB_GCI0_M42H_IVCRX       0x00001518
#define  H21RXSB_GCI1_M42H_IVCRX       0x00001519
#define   H21RXSB_GCC_M42H_IVCRX       0x0000151a
#define H21RXSB_POSTDIV_M42H_IVCRX       0x0000151b
#define  H21RXSB_NMUL_M42H_IVCRX       0x0000151c
#define H21RXSB_REQM0_M42H_IVCRX       0x0000151d
#define H21RXSB_REQM1_M42H_IVCRX       0x0000151e
#define H21RXSB_REQM2_M42H_IVCRX       0x0000151f
#define H21RXSB_REQD1_M42H_IVCRX       0x00001520
#define H21RXSB_REQD2_M42H_IVCRX       0x00001521
#define H21RXSB_REQF1_M42H_IVCRX       0x00001522
#define H21RXSB_REQF2_M42H_IVCRX       0x00001523
#define H21RXSB_REQF3_M42H_IVCRX       0x00001524
#define H21RXSB_STATUS_M42H_IVCRX       0x00001525
#define   H21RXSB_DBS_M42H_IVCRX       0x00001526
#define H21RXSB_SWGEAR_M42H_IVCRX       0x00001527
#define H21RXSB_HWGEAR_M42H_IVCRX       0x00001528
#define H21RXSB_ECC_CNT_FIXED_LSB_M42H_IVCRX       0x00001529
#define H21RXSB_ECC_CNT_FIXED_MSB_M42H_IVCRX       0x0000152a
#define H21RXSB_ERR_TS0_M42H_IVCRX       0x0000152b
#define H21RXSB_ERR_TS1_M42H_IVCRX       0x0000152c
#define H21RXSB_INTR_STAT_M42H_IVCRX       0x0000152d
#define H21RXSB_INTR1_M42H_IVCRX       0x0000152e
#define H21RXSB_INTR2_M42H_IVCRX       0x0000152f
#define H21RXSB_INTR1_MASK_M42H_IVCRX       0x00001530
#define H21RXSB_INTR2_MASK_M42H_IVCRX       0x00001531
#define H21RXSB_CTRL1_M42H_IVCRX       0x00001532
#define H21RXSB_ECC_CNT_FIXED_LSB_RS0_M42H_IVCRX       0x00001533
#define H21RXSB_ECC_CNT_FIXED_MSB_RS0_M42H_IVCRX       0x00001534
#define H21RXSB_ECC_CNT_FIXED_LSB_RS1_M42H_IVCRX       0x00001535
#define H21RXSB_ECC_CNT_FIXED_MSB_RS1_M42H_IVCRX       0x00001536
#define H21RXSB_ECC_CNT_FIXED_LSB_RS2_M42H_IVCRX       0x00001537
#define H21RXSB_ECC_CNT_FIXED_MSB_RS2_M42H_IVCRX       0x00001538
#define H21RXSB_ECC_CNT_FIXED_LSB_RS3_M42H_IVCRX       0x00001539
#define H21RXSB_ECC_CNT_FIXED_MSB_RS3_M42H_IVCRX       0x0000153a
#define H21RXSB_RS_CNT_LSB_M42H_IVCRX       0x0000153b
#define H21RXSB_RS_CNT_MSB_M42H_IVCRX       0x0000153c
#define H21RXSB_RS_ERR_CNT_PO_LSB_M42H_IVCRX       0x0000153d
#define H21RXSB_RS_ERR_CNT_PO_MSB_M42H_IVCRX       0x0000153e
#define    H21RXSB_RSERRCNT_FIXED_LSB_M42H_IVCRX       0x0000153f
#define   H21RXSB_RSERRCNT_FIXED_LSB1_M42H_IVCRX       0x00001540
#define    H21RXSB_RSERRCNT_FIXED_MSB_M42H_IVCRX       0x00001541
#define H21RXSB_ST_STATUS_M42H_IVCRX       0x00001542
#define H21RXSB_RS_ERRCNT_TSH_MSB_M42H_IVCRX       0x00001543
#define H21RXSB_RS_ERRCNT_TSH_LSB_M42H_IVCRX       0x00001544
#define H21RXSB_CTRL2_M42H_IVCRX       0x00001545
#define H21RXSB_CTRL3_M42H_IVCRX       0x00001546
#define H21RXSB_CTRL4_M42H_IVCRX       0x00001547
#define H21RXSB_STATUS1_M42H_IVCRX       0x00001548
#define    H21RXSB_RS_CNT2CHK_TSH_LSB_M42H_IVCRX       0x00001549
#define    H21RXSB_RS_CNT2CHK_TSH_MSB_M42H_IVCRX       0x0000154a
#define    H21RXSB_RS_ACC_ERR_TSH_LSB_M42H_IVCRX       0x0000154b
#define   H21RXSB_RS_ACC_ERR_TSH_LSB1_M42H_IVCRX       0x0000154c
#define    H21RXSB_RS_ACC_ERR_TSH_MSB_M42H_IVCRX       0x0000154d
#define  H21RXSB_FRAME_RS_ERR_TSH_LSB_M42H_IVCRX       0x0000154e
#define H21RXSB_FRAME_RS_ERR_TSH_LSB1_M42H_IVCRX       0x0000154f
#define  H21RXSB_FRAME_RS_ERR_TSH_MSB_M42H_IVCRX       0x00001550
#define   H21RXSB_CONS_RS_ERR_TSH_LSB_M42H_IVCRX       0x00001551
#define   H21RXSB_CONS_RS_ERR_TSH_MSB_M42H_IVCRX       0x00001552
#define H21RXSB_NO_CRCTRS_TSH_LSB_M42H_IVCRX       0x00001553
#define H21RXSB_NO_CRCTRS_TSH_MSB_M42H_IVCRX       0x00001554
#define H21RXSB_FRAME_TSH_ACCM_RS_ERR_LSB_M42H_IVCRX       0x00001555
#define H21RXSB_FRAME_TSH_ACCM_RS_ERR_MSB_M42H_IVCRX       0x00001556
#define H21RXSB_GIVEN_FRAME_RSERR_ACCM_TSH_LSB_M42H_IVCRX       0x00001557
#define H21RXSB_GIVEN_FRAME_RSERR_ACCM_TSH_LSB1_M42H_IVCRX       0x00001558
#define H21RXSB_GIVEN_FRAME_RSERR_ACCM_TSH_MSB_M42H_IVCRX       0x00001559
#define H21RXSB_KEEP_OUT_START_REGISTER_M42H_IVCRX       0x0000155a
#define H21RXSB_KEEP_OUT_END_REGISTER_M42H_IVCRX       0x0000155b
#define  H21RXSB_GP1_REGISTER_M42H_IVCRX       0x0000155c
#define  H21RXSB_GP2_REGISTER_M42H_IVCRX       0x0000155d
#define  H21RXSB_GP3_REGISTER_M42H_IVCRX       0x0000155e
#define  H21RXSB_GP4_REGISTER_M42H_IVCRX       0x0000155f
#define H21RXSB_TRST_REQM0_M42H_IVCRX       0x00001560
#define H21RXSB_TRST_REQM1_M42H_IVCRX       0x00001561
#define H21RXSB_TRST_REQM2_M42H_IVCRX       0x00001562
#define H21RXSB_INTR3_M42H_IVCRX       0x00001563
#define H21RXSB_INTR3_MASK_M42H_IVCRX       0x00001564
#define H21RXSB_DIFF3T_M42H_IVCRX       0x00001565
#define H21RXSB_STRM_CLK_STAT_M42H_IVCRX       0x00001566
#define H21RXSB_CTRL5_M42H_IVCRX       0x00001567
#define H21RXSB_CTRL6_M42H_IVCRX       0x00001568
#define H21RXSB_CTRL7_M42H_IVCRX       0x00001569
#define H21RXSB_CTRL8_M42H_IVCRX       0x0000156a
#define H21RXSB_CTRL9_M42H_IVCRX       0x0000156b
#define H21RXSB_CTRL10_M42H_IVCRX       0x0000156c
#define H21RXSB_CTRL11_M42H_IVCRX       0x0000156d
#define H21RXSB_GP_STATUS1_M42H_IVCRX       0x0000156e
#define H21RXSB_GP_STATUS2_M42H_IVCRX       0x0000156f
#define H21RXSB_GP_STATUS3_M42H_IVCRX       0x00001570
#define H21RXSB_GP_STATUS4_M42H_IVCRX       0x00001571
#define H21RXSB_GP_STATUS5_M42H_IVCRX       0x00001572

//==================== HDCP1 X===============
#define HDCP_ECC_CTRL_HDCP1X_IVCRX     0x00001670
#define HDCP_ECC_CNT2CHK_0_HDCP1X_IVCRX     0x00001671
#define HDCP_ECC_CNT2CHK_1_HDCP1X_IVCRX     0x00001672
#define   HDCP_ECC_ACCM_ERR_THR_0_HDCP1X_IVCRX     0x00001673
#define   HDCP_ECC_ACCM_ERR_THR_1_HDCP1X_IVCRX     0x00001674
#define   HDCP_ECC_ACCM_ERR_THR_2_HDCP1X_IVCRX     0x00001675
#define HDCP_ECC_FRM_ERR_THR_0_HDCP1X_IVCRX     0x00001676
#define HDCP_ECC_FRM_ERR_THR_1_HDCP1X_IVCRX     0x00001677
#define HDCP_CONS_ERR_THR_HDCP1X_IVCRX     0x00001678
#define   HDCP_ECC_NO_ERR_THR_HDCP1X_IVCRX     0x00001679
#define  HDCP_GVN_FRM_HDCP1X_IVCRX     0x0000167a
#define    HDCP_ECC_GVN_FRM_ERR_THR_0_HDCP1X_IVCRX     0x0000167b
#define    HDCP_ECC_GVN_FRM_ERR_THR_1_HDCP1X_IVCRX     0x0000167c
#define    HDCP_ECC_GVN_FRM_ERR_THR_2_HDCP1X_IVCRX     0x0000167d
#define RX_SHD_BKSV_1_HDCP1X_IVCRX     0x0000168a
#define RX_SHD_BKSV_2_HDCP1X_IVCRX     0x0000168b
#define RX_SHD_BKSV_3_HDCP1X_IVCRX     0x0000168c
#define RX_SHD_BKSV_4_HDCP1X_IVCRX     0x0000168d
#define RX_SHD_BKSV_5_HDCP1X_IVCRX     0x0000168e
#define RX_SHD_RI1_HDCP1X_IVCRX     0x0000168f
#define RX_SHD_RI2_HDCP1X_IVCRX     0x00001690
#define RX_SHD_AKSV1_HDCP1X_IVCRX     0x00001691
#define RX_SHD_AKSV2_HDCP1X_IVCRX     0x00001692
#define RX_SHD_AKSV3_HDCP1X_IVCRX     0x00001693
#define RX_SHD_AKSV4_HDCP1X_IVCRX     0x00001694
#define RX_SHD_AKSV5_HDCP1X_IVCRX     0x00001695
#define RX_SHD_AN1_HDCP1X_IVCRX     0x00001696
#define RX_SHD_AN2_HDCP1X_IVCRX     0x00001697
#define RX_SHD_AN3_HDCP1X_IVCRX     0x00001698
#define RX_SHD_AN4_HDCP1X_IVCRX     0x00001699
#define RX_SHD_AN5_HDCP1X_IVCRX     0x0000169a
#define RX_SHD_AN6_HDCP1X_IVCRX     0x0000169b
#define RX_SHD_AN7_HDCP1X_IVCRX     0x0000169c
#define RX_SHD_AN8_HDCP1X_IVCRX     0x0000169d
#define  RX_BCAPS_SET_HDCP1X_IVCRX     0x0000169e
#define RX_SHD_BSTATUS1_HDCP1X_IVCRX     0x0000169f
#define RX_SHD_BSTATUS2_HDCP1X_IVCRX     0x000016a0
#define RX_HDCP_DEBUG_HDCP1X_IVCRX     0x000016a1
#define  RX_HDCP_STAT_HDCP1X_IVCRX     0x000016a2
#define RX_KSV_SHA_start1_HDCP1X_IVCRX     0x000016a3
#define RX_KSV_SHA_start2_HDCP1X_IVCRX     0x000016a4
#define RX_SHA_length1_HDCP1X_IVCRX     0x000016a5
#define RX_SHA_length2_HDCP1X_IVCRX     0x000016a6
#define   RX_SHA_ctrl_HDCP1X_IVCRX     0x000016a7
#define   RX_KSV_FIFO_HDCP1X_IVCRX     0x000016a8
#define RX_HDCP1X_INTR0_HDCP1X_IVCRX     0x000016a9
#define  RX_HDCP1X_INTR0_MASK_HDCP1X_IVCRX     0x000016aa
#define     HDCP_M0_0_HDCP1X_IVCRX     0x000016b7
#define     HDCP_M0_1_HDCP1X_IVCRX     0x000016b8
#define     HDCP_M0_2_HDCP1X_IVCRX     0x000016b9
#define     HDCP_M0_3_HDCP1X_IVCRX     0x000016ba
#define     HDCP_M0_4_HDCP1X_IVCRX     0x000016bb
#define     HDCP_M0_5_HDCP1X_IVCRX     0x000016bc
#define     HDCP_M0_6_HDCP1X_IVCRX     0x000016bd
#define     HDCP_M0_7_HDCP1X_IVCRX     0x000016be
#define RX_DS_BSTATUS1_HDCP1X_IVCRX     0x000016d5
#define RX_DS_BSTATUS2_HDCP1X_IVCRX     0x000016d6
#define    RX_DS_M0_0_HDCP1X_IVCRX     0x000016d7
#define    RX_DS_M0_1_HDCP1X_IVCRX     0x000016d8
#define    RX_DS_M0_2_HDCP1X_IVCRX     0x000016d9
#define    RX_DS_M0_3_HDCP1X_IVCRX     0x000016da
#define    RX_DS_M0_4_HDCP1X_IVCRX     0x000016db
#define    RX_DS_M0_5_HDCP1X_IVCRX     0x000016dc
#define    RX_DS_M0_6_HDCP1X_IVCRX     0x000016dd
#define    RX_DS_M0_7_HDCP1X_IVCRX     0x000016de
#define      RX_VH0_0_HDCP1X_IVCRX     0x000016df
#define      RX_VH0_1_HDCP1X_IVCRX     0x000016e0
#define      RX_VH0_2_HDCP1X_IVCRX     0x000016e1
#define      RX_VH0_3_HDCP1X_IVCRX     0x000016e2
#define      RX_VH1_0_HDCP1X_IVCRX     0x000016e3
#define      RX_VH1_1_HDCP1X_IVCRX     0x000016e4
#define      RX_VH1_2_HDCP1X_IVCRX     0x000016e5
#define      RX_VH1_3_HDCP1X_IVCRX     0x000016e6
#define      RX_VH2_0_HDCP1X_IVCRX     0x000016e7
#define      RX_VH2_1_HDCP1X_IVCRX     0x000016e8
#define      RX_VH2_2_HDCP1X_IVCRX     0x000016e9
#define      RX_VH2_3_HDCP1X_IVCRX     0x000016ea
#define      RX_VH3_0_HDCP1X_IVCRX     0x000016eb
#define      RX_VH3_1_HDCP1X_IVCRX     0x000016ec
#define      RX_VH3_2_HDCP1X_IVCRX     0x000016ed
#define      RX_VH3_3_HDCP1X_IVCRX     0x000016ee
#define      RX_VH4_0_HDCP1X_IVCRX     0x000016ef
#define      RX_VH4_1_HDCP1X_IVCRX     0x000016f0
#define      RX_VH4_2_HDCP1X_IVCRX     0x000016f1
#define      RX_VH4_3_HDCP1X_IVCRX     0x000016f2
#define       RX_EPST_HDCP1X_IVCRX     0x000016f9
#define       RX_EPCM_HDCP1X_IVCRX     0x000016fa

//==================== VIDEO PATH CORE ===============
#define VP_FEATURES_VID_IVCRX				0x00001800
#define VP_BUILD_TIME_VID_IVCRX				0x00001808
#define VP_SW_RST_VID_IVCRX				0x0000180c
#define VP_DATA_BITS_VALUE_VID_IVCRX			0x0000180e
#define VP_INPUT_MUTE_VID_IVCRX				0x00001810
#define VP_INPUT_SYNC_CFG_VID_IVCRX			0x00001812
#define VP_INPUT_FORMAT_VID_IVCRX			0x00001814
#define VP_INPUT_MAPPING_VID_IVCRX			0x00001816
#define VP_INPUT_MASK_VID_IVCRX				0x00001818
#define VP_PIPELINE_CFG_VID_IVCRX			0x0000181a
#define VP_INPUT_SYNC_ADJUST_CFG_VID_IVCRX		0x0000181c
#define VP_DEGEN_CFG_VID_IVCRX				0x00001820
#define VP_DEGEN_PIXEL_DELAY_VID_IVCRX			0x00001822
#define VP_DEGEN_PIXEL_COUNT_MINUS_ONE_VID_IVCRX	0x00001824
#define VP_DEGEN_LINE_DELAY_VID_IVCRX			0x00001826
#define VP_DEGEN_LINE_COUNT_CFG_VID_IVCRX		0x00001828
#define VP_DEC656_CFG_VID_IVCRX				0x00001830
#define VP_DEC656_DELAY_EAV_TO_HSYNC_ACTIVE_VID_IVCRX	0x00001832
#define VP_DEC656_PULSE_WIDTH_VSYNC_MINUS_ONE_VID_IVCRX	0x00001834
#define VP_DEC656_VSYNC_FRONT_PORCH_VID_IVCRX		0x00001836
#define VP_DEC656_EAV_TO_VSYNC_DELAY_EVEN_VID_IVCRX	0x00001838
#define VP_DEC656_EAV_TO_VSYNC_DELAY_ODD_VID_IVCRX	0x0000183a
#define VP_DEC656_PULSE_WIDTH_VSYNC_VID_IVCRX		0x0000183c
#define VP_DEC656_STATUS_VID_IVCRX			0x0000183e
#define VP_OUTPUT_MUTE_VID_IVCRX			0x00001840
#define VP_OUTPUT_SYNC_CFG_VID_IVCRX			0x00001842
#define VP_OUTPUT_MAPPING_VID_IVCRX			0x00001844
#define VP_OUTPUT_MASK_VID_IVCRX			0x00001846
#define VP_OUTPUT_FORMAT_VID_IVCRX			0x00001848
#define VP_OUTPUT_BLANK_START_LINE_VID_IVCRX		0x0000184c
#define VP_OUTPUT_BLANK_END_LINE_VID_IVCRX		0x0000184e
#define VP_OUTPUT_BLANKING_CFG_VID_IVCRX		0x00001850
#define VP_OUTPUT_BLANKING_STATUS_VID_IVCRX		0x00001852
#define VP_OUTPUT_BLANK_Y_VID_IVCRX			0x00001854
#define VP_OUTPUT_BLANK_CB_VID_IVCRX			0x00001856
#define VP_OUTPUT_BLANK_CR_VID_IVCRX			0x00001858
#define VP_OUTPUT_ACTIVE_Y_VID_IVCRX			0x0000185a
#define VP_OUTPUT_ACTIVE_CB_VID_IVCRX			0x0000185c
#define VP_OUTPUT_ACTIVE_CR_VID_IVCRX			0x0000185e
#define VP_VTG_VHSYNC_END_VID_IVCRX			0x00001860
#define VP_VTG_HACTIVE_VIDEO_START_VID_IVCRX		0x00001862
#define VP_VTG_HALFLINE_VID_IVCRX			0x00001864
#define VP_VTG_HACTIVE_VIDEO_END_VID_IVCRX		0x00001866
#define VP_VTG_END_OF_LINE_VID_IVCRX			0x00001868
#define VP_VTG_VSYNC_END_VID_IVCRX			0x00001870
#define VP_VTG_TRIGGER_START_VID_IVCRX			0x00001872
#define VP_VTG_VACTIVE_VIDEO_START_VID_IVCRX		0x00001874
#define VP_VTG_VACTIVE_VIDEO_END_VID_IVCRX		0x00001876
#define VP_VTG_VEND_OF_FRAME_VID_IVCRX			0x00001878
#define VP_VTG_CFG_VID_IVCRX				0x0000187a
#define VP_VTG_THREHOLD_VID_IVCRX			0x0000187b
#define VP_VTG_CYCLE_DELAY_VID_IVCRX			0x0000187c
#define VP_VTG_UPDATE_REQUEST_VID_IVCRX			0x0000187e
#define VP_VTG_BANK_CFG_VID_IVCRX			0x0000187f
#define VP_FDET_CFG_VID_IVCRX				0x00001880
#define VP_FDET_STATUS_VID_IVCRX			0x00001881
#define VP_FDET_CLEAR_VID_IVCRX				0x00001882
#define VP_FDET_INTERLACE_THRESHOLD_VID_IVCRX		0x00001883
#define VP_FDET_FRMAE_RATE_DELTA_THRESHOLD_VID_IVCRX	0x00001884
#define VP_FDET_FRMAE_RATE_VID_IVCRX			0x00001888
#define VP_FDET_PIXEL_COUNT_VID_IVCRX			0x0000188c
#define VP_FDET_LINE_COUNT_VID_IVCRX			0x0000188e
#define VP_FDET_HSYNC_LOW_COUNT_VID_IVCRX		0x00001890
#define VP_FDET_HSYNC_HIGH_COUNT_VID_IVCRX		0x00001892
#define VP_FDET_HFRONT_COUNT_VID_IVCRX			0x00001894
#define VP_FDET_HBACK_COUNT_VID_IVCRX			0x00001896
#define VP_FDET_VSYNC_LOW_COUNT_EVEN_VID_IVCRX		0x00001898
#define VP_FDET_VSYNC_HIGH_COUNT_EVEN_VID_IVCRX		0x0000189a
#define VP_FDET_VFRONT_COUNT_EVEN_VID_IVCRX		0x0000189c
#define VP_FDET_VBACK_COUNT_EVEN_VID_IVCRX		0x0000189e
#define	VP_FDET_VSYNC_LOW_COUNT_ODD_VID_IVCRX		0x000018a0
#define VP_FDET_VSYNC_HIGH_COUNT_ODD_VID_IVCRX		0x000018a2
#define VP_FDET_VFRONT_COUNT_ODD_VID_IVCRX		0x000018a4
#define VP_FDET_VBACK_COUNT_ODD_VID_IVCRX		0x000018a6
#define VP_FDET_FRAME_COUNT_VID_IVCRX			0x000018a8
#define VP_FDET_LINE_RATE_DELTA_THRESHOLD_VID_IVCRX	0x000018aa
#define VP_FDET_LINE_RATE_VID_IVCRX			0x000018ac
#define VP_FDET_VSYNC_HSYNC_OFFSET_EVEN_VID_IVCRX	0x000018b0
#define VP_FDET_VSYNC_HSYNC_OFFSET_ODD_VID_IVCRX	0x000018b2
#define VP_FDET_IRQ_MASK_VID_IVCRX			0x000018b8
#define VP_FDET_IRQ_STATUS_VID_IVCRX			0x000018bc
#define VP_EMBD_SYNC_ENC_CFG_VID_IVCRX			0x000018c0
#define VP_EMBD_SYNC_ENC_SECT0_CFG_VID_IVCRX		0x000018c4
#define VP_EMBD_SYNC_ENC_SECT1_CFG_VID_IVCRX		0x000018c8
#define VP_EMBD_SYNC_ENC_SECT2_CFG_VID_IVCRX		0x000018cc
#define VP_EMBD_SYNC_ENC_SECT3_CFG_VID_IVCRX		0x000018d0
#define VP_EMBD_SYNC_ENC_SECT4_CFG_VID_IVCRX		0x000018d4
#define VP_EMBD_SYNC_ENC_SECT5_CFG_VID_IVCRX		0x000018d8
#define VP_EMBD_SYNC_ENC_HDATA_PIXELS_VID_IVCRX		0x000018dc
#define VP_EMBD_SYNC_ENC_HBLANKING_PIXELS_VID_IVCRX	0x000018de
#define VP_EMBD_SYNC_ENC_UPDATE_REQUEST_VID_IVCRX	0x000018e0
#define VP_EMBD_SYNC_ENC_BANC_CFG_VID_IVCRX		0x000018e1
#define VP_FRMAES_CNT_VID_IVCRX				0x000018e4
#define VP_PIXEL_CLK_CNT_VID_IVCRX			0x000018e8
#define VP_INTERLACE_FIELD_VID_IVCRX			0x000018ec

//==================== CP2PA HDCP2XCORE ===============
#define    CP2PA_CTRL_HDCP2X_IVCRX     0x00001d00
#define   CP2PA_INTR0_HDCP2X_IVCRX     0x00001d01
#define   CP2PA_INTR1_HDCP2X_IVCRX     0x00001d02
#define CP2PA_INTR0_MASK_HDCP2X_IVCRX     0x00001d03
#define CP2PA_INTR1_MASK_HDCP2X_IVCRX     0x00001d04
#define CP2PA_INTRSTATUS_HDCP2X_IVCRX     0x00001d05
#define CP2PA_GEN_STATUS_HDCP2X_IVCRX     0x00001d06
#define     CP2PA_TP0_HDCP2X_IVCRX     0x00001d10
#define     CP2PA_TP1_HDCP2X_IVCRX     0x00001d11
#define     CP2PA_TP2_HDCP2X_IVCRX     0x00001d12
#define     CP2PA_TP3_HDCP2X_IVCRX     0x00001d13
#define     CP2PA_TP4_HDCP2X_IVCRX     0x00001d14
#define     CP2PA_TP5_HDCP2X_IVCRX     0x00001d15
#define CP2PA_TP_V_CHK_HDCP2X_IVCRX     0x00001d16
#define CP2PA_AESCTL0_HDCP2X_IVCRX     0x00001d17
#define CP2PA_AESCTL1_HDCP2X_IVCRX     0x00001d18
#define CP2PA_AUTH_CTL1_HDCP2X_IVCRX     0x00001d19
#define CP2PA_CIPHER_CTL2_HDCP2X_IVCRX     0x00001d1a
#define     CP2PA_GP0_HDCP2X_IVCRX     0x00001d1b
#define     CP2PA_GP1_HDCP2X_IVCRX     0x00001d1c
#define     CP2PA_GP2_HDCP2X_IVCRX     0x00001d1d
#define     CP2PA_GP3_HDCP2X_IVCRX     0x00001d1e

#define HDCP2X_RX_ECC_CTRL				0x1d90
#define HDCP2X_RX_ECC_CNT2CHK_0			0x1d91
#define HDCP2X_RX_ECC_CNT2CHK_1			0x1d92
#define HDCP2X_RX_ECC_FRM_ERR_THR_0		0x1d96
#define HDCP2X_RX_ECC_FRM_ERR_THR_1		0x1d97
#define HDCP2X_RX_ECC_CONS_ERR_THR		0x1d98
#define HDCP2X_RX_ECC_GVN_FRM_ERR_THR_0		0x1d9B
#define HDCP2X_RX_ECC_GVN_FRM_ERR_THR_1		0x1d9C
#define HDCP2X_RX_ECC_GVN_FRM_ERR_THR_2		0x1d9D
#define HDCP2X_RX_ECC_INTR				0x1d9e
#define HDCP2X_RX_ECC_INTR_MASK			0x1d9f
#define HDCP2X_RX_GVN_FRM				0x1d9A

//==================== CP2PA CORE===============
#define CP2PAX_CTRL_0_HDCP2X_IVCRX     0x00001e00
#define CP2PAX_CTRL_1_HDCP2X_IVCRX     0x00001e01
#define CP2PAX_CTRL_2_HDCP2X_IVCRX     0x00001e02
#define  CP2PAX_INTR0_HDCP2X_IVCRX     0x00001e03
#define  CP2PAX_INTR1_HDCP2X_IVCRX     0x00001e04
#define  CP2PAX_INTR2_HDCP2X_IVCRX     0x00001e05
#define CP2PAX_INTR0_MASK_HDCP2X_IVCRX     0x00001e06
#define CP2PAX_INTR1_MASK_HDCP2X_IVCRX     0x00001e07
#define CP2PAX_INTR2_MASK_HDCP2X_IVCRX     0x00001e08
#define CP2PAX_INTRSTATUS_HDCP2X_IVCRX     0x00001e09
#define CP2PAX_AUTH_STAT_HDCP2X_IVCRX     0x00001e0a
#define  CP2PAX_STATE_HDCP2X_IVCRX     0x00001e0b
#define CP2PAX_GEN_STATUS_HDCP2X_IVCRX     0x00001e0c
#define CP2PAX_GP_IN0_HDCP2X_IVCRX     0x00001e0d
#define CP2PAX_GP_IN1_HDCP2X_IVCRX     0x00001e0e
#define CP2PAX_GP_IN2_HDCP2X_IVCRX     0x00001e0f
#define CP2PAX_GP_IN3_HDCP2X_IVCRX     0x00001e10
#define CP2PAX_GP_OUT0_HDCP2X_IVCRX     0x00001e11
#define CP2PAX_GP_OUT1_HDCP2X_IVCRX     0x00001e12
#define CP2PAX_GP_OUT2_HDCP2X_IVCRX     0x00001e13
#define CP2PAX_GP_OUT3_HDCP2X_IVCRX     0x00001e14
#define CP2PAX_RX_ID_CORE_0_HDCP2X_IVCRX     0x00001e15
#define CP2PAX_RX_ID_CORE_1_HDCP2X_IVCRX     0x00001e16
#define CP2PAX_RX_ID_CORE_2_HDCP2X_IVCRX     0x00001e17
#define CP2PAX_RX_ID_CORE_3_HDCP2X_IVCRX     0x00001e18
#define CP2PAX_RX_ID_CORE_4_HDCP2X_IVCRX     0x00001e19
#define CP2PAX_RPT_DETAIL_HDCP2X_IVCRX     0x00001e1a
#define CP2PAX_RPT_SMNG_K_HDCP2X_IVCRX     0x00001e1b
#define CP2PAX_RPT_DEPTH_HDCP2X_IVCRX     0x00001e1c
#define CP2PAX_RPT_DEVCNT_HDCP2X_IVCRX     0x00001e1d
#define CP2PAX_RX_SEQ_NUM_V_0_HDCP2X_IVCRX     0x00001e1e
#define CP2PAX_RX_SEQ_NUM_V_1_HDCP2X_IVCRX     0x00001e1f
#define CP2PAX_RX_SEQ_NUM_V_2_HDCP2X_IVCRX     0x00001e20
#define CP2PAX_RX_SEQ_NUM_M_0_HDCP2X_IVCRX     0x00001e21
#define CP2PAX_RX_SEQ_NUM_M_1_HDCP2X_IVCRX     0x00001e22
#define CP2PAX_RX_SEQ_NUM_M_2_HDCP2X_IVCRX     0x00001e23
#define CP2PAX_RX_CTRL_0_HDCP2X_IVCRX     0x00001e24
#define CP2PAX_RX_STATUS_HDCP2X_IVCRX     0x00001e25
#define CP2PAX_RX_RPT_SMNG_OUT_HDCP2X_IVCRX     0x00001e26
#define CP2PAX_RX_RPT_RCVID_IN_HDCP2X_IVCRX     0x00001e27
#define CP2PAX_ROSC_TEST_HDCP2X_IVCRX     0x00001e28
#define CP2PAX_GP_IN4_HDCP2X_IVCRX     0x00001e29
#define CP2PAX_GP_IN5_HDCP2X_IVCRX     0x00001e2a
#define CP2PAX_GP_CTL_HDCP2X_IVCRX     0x00001e2b
#define CP2PAX_GP_CTL2_HDCP2X_IVCRX     0x00001e2c
#define CP2PAX_GP_OUT4_HDCP2X_IVCRX     0x00001e2d
#define CP2PAX_GP_OUT5_HDCP2X_IVCRX     0x00001e2e

//==================== DPLL ===============
#define     DPLL_CFG1_DPLL_IVCRX       0x00001f00
#define     DPLL_CFG2_DPLL_IVCRX       0x00001f01
#define     DPLL_CFG3_DPLL_IVCRX       0x00001f02
#define     DPLL_CFG4_DPLL_IVCRX       0x00001f03
#define      EV_VAL_B_DPLL_IVCRX       0x00001f04
#define      EV_VAL_G_DPLL_IVCRX       0x00001f05
#define      EV_VAL_R_DPLL_IVCRX       0x00001f06
#define        BV_VAL_DPLL_IVCRX       0x00001f07
#define      PEQ_VAL0_DPLL_IVCRX       0x00001f08
#define      PEQ_VAL1_DPLL_IVCRX       0x00001f09
#define      PEQ_VAL2_DPLL_IVCRX       0x00001f0a
#define      PEQ_VAL3_DPLL_IVCRX       0x00001f0b
#define      PEQ_VAL4_DPLL_IVCRX       0x00001f0c
#define      PEQ_VAL5_DPLL_IVCRX       0x00001f0d
#define      PEQ_VAL6_DPLL_IVCRX       0x00001f0e
#define      PEQ_VAL7_DPLL_IVCRX       0x00001f0f
#define      PBW_VAL0_DPLL_IVCRX       0x00001f10
#define      PBW_VAL1_DPLL_IVCRX       0x00001f11
#define      PBW_VAL2_DPLL_IVCRX       0x00001f12
#define      PBW_VAL3_DPLL_IVCRX       0x00001f13
#define     DPLL_CFG5_DPLL_IVCRX       0x00001f14
#define     DPLL_CFG6_DPLL_IVCRX       0x00001f15
#define     GBOX_RST1_DPLL_IVCRX       0x00001f16
#define     GBOX_RST2_DPLL_IVCRX       0x00001f17
#define    ZCTRL_RST1_DPLL_IVCRX       0x00001f18
#define    ZCTRL_RST2_DPLL_IVCRX       0x00001f19
#define     DPLL_CFG7_DPLL_IVCRX       0x00001f1a
#define        OLOOP1_DPLL_IVCRX       0x00001f1b
#define        OLOOP2_DPLL_IVCRX       0x00001f1c
#define       ZCTRL_1_DPLL_IVCRX       0x00001f1d
#define   ALT_Z_CTRL1_DPLL_IVCRX       0x00001f1e
#define   ALT_Z_CTRL2_DPLL_IVCRX       0x00001f1f
#define   CL_REF_CLK1_DPLL_IVCRX       0x00001f20
#define   CL_REF_CLK2_DPLL_IVCRX       0x00001f21
#define   OL_DIV4CLK1_DPLL_IVCRX       0x00001f22
#define   OL_DIV4CLK2_DPLL_IVCRX       0x00001f23
#define   OL_REF_CLK1_DPLL_IVCRX       0x00001f24
#define   OL_REF_CLK2_DPLL_IVCRX       0x00001f25
#define     ZONE_VAR1_DPLL_IVCRX       0x00001f26
#define     ZONE_VAR2_DPLL_IVCRX       0x00001f27
#define     ZONE_VAR3_DPLL_IVCRX       0x00001f28
#define      N_RUN_CL_DPLL_IVCRX       0x00001f29
#define      N_RUN_OL_DPLL_IVCRX       0x00001f2a
#define        N_IDLE_DPLL_IVCRX       0x00001f2b
#define   CMO_MON_PRD_DPLL_IVCRX       0x00001f2c
#define     INT_PRMBL_DPLL_IVCRX       0x00001f2d
#define     DPLL_MISC_DPLL_IVCRX       0x00001f2e
#define  DPLL_STATUS1_DPLL_IVCRX       0x00001f2f
#define  DPLL_STATUS2_DPLL_IVCRX       0x00001f30
#define  DPLL_STATUS3_DPLL_IVCRX       0x00001f31
#define  DPLL_STATUS4_DPLL_IVCRX       0x00001f32
#define  DPLL_STATUS5_DPLL_IVCRX       0x00001f33
#define  DPLL_STATUS6_DPLL_IVCRX       0x00001f34
#define  DPLL_STATUS7_DPLL_IVCRX       0x00001f35
#define  DPLL_STATUS8_DPLL_IVCRX       0x00001f36
#define  DPLL_STATUS9_DPLL_IVCRX       0x00001f37
#define DPLL_STATUS10_DPLL_IVCRX       0x00001f38
#define DPLL_STATUS11_DPLL_IVCRX       0x00001f39
#define DPLL_STATUS12_DPLL_IVCRX       0x00001f3a
#define DPLL_STATUS13_DPLL_IVCRX       0x00001f3b
#define   DPLLM3_OFE1_DPLL_IVCRX       0x00001f3c
#define   DPLLM3_OFE2_DPLL_IVCRX       0x00001f3d
#define   DPLLM3_OFE3_DPLL_IVCRX       0x00001f3e
#define    DPLLM3_CFG_DPLL_IVCRX       0x00001f3f
#define   DPLLM3_BP_B_DPLL_IVCRX       0x00001f40
#define   DPLLM3_BP_G_DPLL_IVCRX       0x00001f41
#define   DPLLM3_BP_R_DPLL_IVCRX       0x00001f42
#define DPLLM3_VV_I2C_DPLL_IVCRX       0x00001f43
#define DPLLM3_VV_CTL_DPLL_IVCRX       0x00001f44
#define   DPLLM3_PVT0_DPLL_IVCRX       0x00001f45
#define   DPLLM3_PVT1_DPLL_IVCRX       0x00001f46
#define   DPLLM3_PVT2_DPLL_IVCRX       0x00001f47
#define   DPLLM3_PVT3_DPLL_IVCRX       0x00001f48
#define  DPLLM3_MISC0_DPLL_IVCRX       0x00001f49
#define  DPLLM3_SZONE_DPLL_IVCRX       0x00001f4a
#define  DPLL_BW_CFG1_DPLL_IVCRX       0x00001f4b
#define  DPLL_BW_CFG2_DPLL_IVCRX       0x00001f4c
#define      MEQ_VAL0_DPLL_IVCRX       0x00001f4d
#define      MEQ_VAL1_DPLL_IVCRX       0x00001f4e
#define      MEQ_VAL2_DPLL_IVCRX       0x00001f4f
#define      MEQ_VAL3_DPLL_IVCRX       0x00001f50
#define    DPLL_MISC2_DPLL_IVCRX       0x00001f51
#define    DPLL_HDMI2_DPLL_IVCRX       0x00001f52
#define   DPLL_VV_DEQ_DPLL_IVCRX       0x00001f53
#define DPLL_IPS_VAL0_DPLL_IVCRX       0x00001f54
#define DPLL_IPS_VAL1_DPLL_IVCRX       0x00001f55
#define DPLL_IPS_VAL2_DPLL_IVCRX       0x00001f56
#define DPLL_IPS_VAL3_DPLL_IVCRX       0x00001f57
#define DPLL_IPS_VAL4_DPLL_IVCRX       0x00001f58
#define DPLL_IPS_VAL5_DPLL_IVCRX       0x00001f59
#define    DPLL_CTRL0_DPLL_IVCRX       0x00001f5a
#define    DPLL_CTRL1_DPLL_IVCRX       0x00001f5b
#define    DPLL_CTRL2_DPLL_IVCRX       0x00001f5c
#define  DPLL_CH_CTRL_DPLL_IVCRX       0x00001f5d
#define  NEW_DPLL_CFG_DPLL_IVCRX       0x00001f5e
#define  SYNC_PATTERN_TDD_0_DPLL_IVCRX       0x00001f5f
#define  SYNC_PATTERN_TDD_1_DPLL_IVCRX       0x00001f60
#define  DPLL_DFIFO_ERROR_STS_DPLL_IVCRX       0x00001f61
#define  DPLL_6PORT_0_DPLL_IVCRX       0x00001f62
#define  DPLL_6PORT_1_DPLL_IVCRX       0x00001f63
#define  DPLL_6PORT_2_DPLL_IVCRX       0x00001f64
#define  DPLL_6PORT_3_DPLL_IVCRX       0x00001f65
#define  DPLL_6PORT_4_DPLL_IVCRX       0x00001f66
#define  DPLL_6PORT_5_DPLL_IVCRX       0x00001f67
#define  DPLL_6PORT_6_DPLL_IVCRX       0x00001f68
#define  DPLL_6PORT_7_DPLL_IVCRX       0x00001f69
#define  DPLL_6PORT_8_DPLL_IVCRX       0x00001f6a
#define  DPLL_6PORT_9_DPLL_IVCRX       0x00001f6b
#define  DPLL_6PORT_A_DPLL_IVCRX       0x00001f6c
#define  DPLL_6PORT_B_DPLL_IVCRX       0x00001f6d
#define  DPLL_6PORT_C_DPLL_IVCRX       0x00001f6e
#define  DPLL_6PORT_D_DPLL_IVCRX       0x00001f6f
#define  DPLL_6PORT_E_DPLL_IVCRX       0x00001f70
#define  DPLL_6PORT_F_DPLL_IVCRX       0x00001f71
#define DPLL_6PORT_10_DPLL_IVCRX       0x00001f72
#define DPLL_6PORT_11_DPLL_IVCRX       0x00001f73
#define DPLL_H21_LANE_SWAP_DPLL_IVCRX       0x00001f74
#define DPLL_GP_CTRL0_REG_DPLL_IVCRX       0x00001f75
#define DPLL_GP_CTRL1_REG_DPLL_IVCRX       0x00001f76
#define DPLL_GP_STS0_REG_DPLL_IVCRX       0x00001f77
#define DPLL_GP_STS1_REG_DPLL_IVCRX       0x00001f78

//==================== ZONEVCO ===============
#define   ZONE_CTRL_0_ZONE_IVCRX       0x00002000
#define ZONE_STATUS_0_ZONE_IVCRX       0x00002001
#define       DEBUG_0_ZONE_IVCRX       0x00002002
#define       DEBUG_1_ZONE_IVCRX       0x00002003
#define VCOCAL_CTRL_0_ZONE_IVCRX       0x00002004
#define VCOCAL_STATUS_0_ZONE_IVCRX       0x00002005
#define PRECLK_FORCE_CTL_ZONE_IVCRX       0x00002006
#define PRECLK_CNT_MAX_ZONE_IVCRX       0x00002007
#define  OCLK_CNT_MAX_ZONE_IVCRX       0x00002008
#define  OCLK_CNT_MIN_ZONE_IVCRX       0x00002009
#define       PRE_DIV_ZONE_IVCRX       0x0000200a
#define      POST_DIV_ZONE_IVCRX       0x0000200b
#define VCOCAL_STATUS_1_ZONE_IVCRX       0x0000200c
#define     PLL_MODE0_ZONE_IVCRX       0x0000200d
#define      SZONE_OW_ZONE_IVCRX       0x0000200e
#define       ISEL_OW_ZONE_IVCRX       0x0000200f
#define    BIAS_BGR_D_ZONE_IVCRX       0x00002010
#define    DIV_PRE_OW_ZONE_IVCRX       0x00002011
#define    DIV_PST_OW_ZONE_IVCRX       0x00002012
#define     VCOCAL_OW_ZONE_IVCRX       0x00002013
#define     ZONE_INTR_ZONE_IVCRX       0x00002014
#define ZONE_INTR_MASK_ZONE_IVCRX       0x00002015
#define   ZONE_CTRL_1_ZONE_IVCRX       0x00002016
#define   ZONE_CTRL_2_ZONE_IVCRX       0x00002017
#define    PLL_LK_DLY_ZONE_IVCRX       0x00002018
#define   LKDT_CTRL_0_ZONE_IVCRX       0x00002019
#define    LKDT_CTRL1_ZONE_IVCRX       0x0000201a
#define    LKDT_CTRL2_ZONE_IVCRX       0x0000201b
#define    LKDT_CTRL3_ZONE_IVCRX       0x0000201c
#define  OCLK_CNT_THR_ZONE_IVCRX       0x0000201d
#define   ZVTIMER_MAX_ZONE_IVCRX       0x0000201e
#define       ZVSTATE_ZONE_IVCRX       0x0000201f
#define    VCOCAL_ADJ_ZONE_IVCRX       0x00002020
#define ZONECAL_ADJ_0_ZONE_IVCRX       0x00002021
#define ZONECAL_ADJ_1_ZONE_IVCRX       0x00002022
#define ZONECAL_ADJ_2_ZONE_IVCRX       0x00002023
#define ZONECAL_ADJ_3_ZONE_IVCRX       0x00002024
#define    ZONECTRL_3_ZONE_IVCRX       0x00002025
#define    ZONECTRL_4_ZONE_IVCRX       0x00002026
#define    ZONECTRL_5_ZONE_IVCRX       0x00002027
#define    ZONECTRL_6_ZONE_IVCRX       0x00002028
#define    ZONECTRL_7_ZONE_IVCRX       0x00002029
#define    ZONECTRL_8_ZONE_IVCRX       0x0000202a
#define    ZONECTRL_9_ZONE_IVCRX       0x0000202b
#define    ZONECTRL_A_ZONE_IVCRX       0x0000202c
#define    ZONECTRL_B_ZONE_IVCRX       0x0000202d
#define    ZONECTRL_C_ZONE_IVCRX       0x0000202e
#define    ZONECTRL_D_ZONE_IVCRX       0x0000202f
#define    ZONECTRL_E_ZONE_IVCRX       0x00002030
#define    ZONECTRL_F_ZONE_IVCRX       0x00002031
#define ZONE_STATUS_1_ZONE_IVCRX       0x00002032
#define ZONE_STATUS_2_ZONE_IVCRX       0x00002033
#define ZONE_STATUS_3_ZONE_IVCRX       0x00002034
#define ZONE_STATUS_4_ZONE_IVCRX       0x00002035
#define ZONE_STATUS_5_ZONE_IVCRX       0x00002036
#define ZONE_STATUS_6_ZONE_IVCRX       0x00002037
#define ZONE_STATUS_7_ZONE_IVCRX       0x00002038
#define ZONE_STATUS_8_ZONE_IVCRX       0x00002039
#define ZONE_STATUS_9_ZONE_IVCRX       0x0000203a
#define ZONE_STATUS_A_ZONE_IVCRX       0x0000203b

//==================== PHY CLK/RST ===============
#define PWD0_CLK_BYP_1_PHYCK_IVCRX      0x000020a1
#define PWD0_CLK_BYP_2_PHYCK_IVCRX      0x000020a2
#define PWD0_CLK_EN_1_PHYCK_IVCRX      0x000020a3
#define PWD0_CLK_EN_2_PHYCK_IVCRX      0x000020a4
#define PWD0_CLK_EN_3_PHYCK_IVCRX      0x000020a5
#define PWD0_CLK_EN_4_PHYCK_IVCRX      0x000020a6
#define PWD0_RST_EN_1_PHYCK_IVCRX      0x000020a7
#define PWD0_RST_EN_2_PHYCK_IVCRX      0x000020a8
#define PWD0_RST_EN_3_PHYCK_IVCRX      0x000020a9

//==================== DIG_DPHY ===============
#define   SR_CDR_CTL3_DDPHY_IVCRX      0x0000210f
#define    SR_DLL_CTL_DDPHY_IVCRX      0x00002117
#define SR_ADAP_CTL27_DDPHY_IVCRX      0x00002147
#define SR_ADAP_CTL28_DDPHY_IVCRX      0x00002148
#define SR_ADAP_CTL29_DDPHY_IVCRX      0x00002149
#define SR_ADAP_CTL30_DDPHY_IVCRX      0x0000214a
#define SR_ADAP_CTL31_DDPHY_IVCRX      0x0000214b
#define SR_ADAP_CTL40_DDPHY_IVCRX      0x00002154
#define SR_ADAP_CTL44_DDPHY_IVCRX      0x00002158
#define SR_ADAP_CTL47_DDPHY_IVCRX      0x0000215b
#define    SR_DP_CTL3_DDPHY_IVCRX      0x0000216e
#define   SR_PLL_CTL0_DDPHY_IVCRX      0x0000219b
#define  SR_PLL_CTL22_DDPHY_IVCRX      0x000021b1
#define  SR_BIST_CTL0_DDPHY_IVCRX      0x000021b8
#define  SR_BIST_CTL1_DDPHY_IVCRX      0x000021b9
#define  SR_BIST_CTL2_DDPHY_IVCRX      0x000021ba
#define  SR_BIST_CTL3_DDPHY_IVCRX      0x000021bb
#define  SR_FIFO_CTL0_DDPHY_IVCRX      0x000021bc
#define SARAH_BIST_ST_0_DDPHY_IVCRX      0x000021cd
#define SARAH_BIST_ST_1_DDPHY_IVCRX      0x000021ce
#define SARAH_BIST_ST_2_DDPHY_IVCRX      0x000021cf
#define  SR_SCDC_CTL0_DDPHY_IVCRX      0x000021e6
#define  SR_SCDC_CTL1_DDPHY_IVCRX      0x000021e7
#define  SR_SCDC_CTL2_DDPHY_IVCRX      0x000021e8
#define  SR_LCDT_CTL0_DDPHY_IVCRX      0x000021e9
#define  SR_LCDT_CTL1_DDPHY_IVCRX      0x000021ea
#define  SR_LCDT_CTL2_DDPHY_IVCRX      0x000021eb
#define  SR_LCDT_CTL3_DDPHY_IVCRX      0x000021ec
#define SARAH_CK_ST_0_DDPHY_IVCRX      0x000021ed
#define SARAH_CK_ST_1_DDPHY_IVCRX      0x000021ee
#define SARAH_CK_ST_2_DDPHY_IVCRX      0x000021ef
#define     SR_LTP_ST_DDPHY_IVCRX      0x000021f0
#define SR_CLK_MEAS_0_DDPHY_IVCRX      0x000021f2
#define SR_CLK_MEAS_1_DDPHY_IVCRX      0x000021f3
#define SARAH_BIST1_ST_0_DDPHY_IVCRX      0x000021f4
#define SARAH_BIST1_ST_1_DDPHY_IVCRX      0x000021f5
#define SARAH_BIST1_ST_2_DDPHY_IVCRX      0x000021f6
#define SARAH_BIST2_ST_0_DDPHY_IVCRX      0x000021f7
#define SARAH_BIST2_ST_1_DDPHY_IVCRX      0x000021f8
#define SARAH_BIST2_ST_2_DDPHY_IVCRX      0x000021f9
#define SARAH_BIST3_ST_0_DDPHY_IVCRX      0x000021fa
#define SARAH_BIST3_ST_1_DDPHY_IVCRX      0x000021fb
#define SARAH_BIST3_ST_2_DDPHY_IVCRX      0x000021fc

//==================== DPHY ===============
#define SR_AV_CTL0_DPHY_IVCRX       0x00007000
#define SR_AV_CTL1_DPHY_IVCRX       0x00007001
#define SR_EQ_CTL0_DPHY_IVCRX       0x00007002
#define SR_EQ_CTL1_DPHY_IVCRX       0x00007003
#define   SR_EQ0_CTL0_DPHY_IVCRX       0x00007004
#define   SR_EQ0_CTL1_DPHY_IVCRX       0x00007005
#define   SR_EQ1_CTL0_DPHY_IVCRX       0x00007006
#define   SR_EQ1_CTL1_DPHY_IVCRX       0x00007007
#define   SR_EQ2_CTL0_DPHY_IVCRX       0x00007008
#define   SR_EQ2_CTL1_DPHY_IVCRX       0x00007009
#define   SR_EQ3_CTL0_DPHY_IVCRX       0x0000700a
#define   SR_EQ3_CTL1_DPHY_IVCRX       0x0000700b
#define   SR_CDR_CTL0_DPHY_IVCRX       0x0000700c
#define   SR_CDR_CTL1_DPHY_IVCRX       0x0000700d
#define   SR_CDR_CTL2_DPHY_IVCRX       0x0000700e
#define   SR_CDR_CTL3_DPHY_IVCRX       0x0000700f
#define  SR_CDR0_CTL0_DPHY_IVCRX       0x00007010
#define  SR_CDR0_CTL1_DPHY_IVCRX       0x00007011
#define  SR_CDR1_CTL0_DPHY_IVCRX       0x00007012
#define  SR_CDR1_CTL1_DPHY_IVCRX       0x00007013
#define  SR_CDR2_CTL0_DPHY_IVCRX       0x00007014
#define  SR_CDR2_CTL1_DPHY_IVCRX       0x00007015
#define  SR_CDR3_CTL0_DPHY_IVCRX       0x00007016
#define  SR_DLL_CTL_DPHY_IVCRX       0x00007017
#define  SR_DLL0_CTL0_DPHY_IVCRX       0x00007018
#define  SR_DLL0_CTL1_DPHY_IVCRX       0x00007019
#define  SR_DLL0_CTL2_DPHY_IVCRX       0x0000701a
#define  SR_DLL0_CTL3_DPHY_IVCRX       0x0000701b
#define  SR_DLL0_CTL4_DPHY_IVCRX       0x0000701c
#define  SR_DLL1_CTL0_DPHY_IVCRX       0x0000701d
#define  SR_DLL1_CTL1_DPHY_IVCRX       0x0000701e
#define  SR_DLL1_CTL2_DPHY_IVCRX       0x0000701f
#define  SR_DLL1_CTL3_DPHY_IVCRX       0x00007020
#define  SR_DLL1_CTL4_DPHY_IVCRX       0x00007021
#define  SR_DLL2_CTL0_DPHY_IVCRX       0x00007022
#define  SR_DLL2_CTL1_DPHY_IVCRX       0x00007023
#define  SR_DLL2_CTL2_DPHY_IVCRX       0x00007024
#define  SR_DLL2_CTL3_DPHY_IVCRX       0x00007025
#define  SR_DLL2_CTL4_DPHY_IVCRX       0x00007026
#define  SR_DLL3_CTL0_DPHY_IVCRX       0x00007027
#define  SR_DLL3_CTL1_DPHY_IVCRX       0x00007028
#define  SR_DLL3_CTL2_DPHY_IVCRX       0x00007029
#define  SR_DLL3_CTL3_DPHY_IVCRX       0x0000702a
#define  SR_DLL3_CTL4_DPHY_IVCRX       0x0000702b
#define  SR_ADAP_CTL0_DPHY_IVCRX       0x0000702c
#define  SR_ADAP_CTL1_DPHY_IVCRX       0x0000702d
#define  SR_ADAP_CTL2_DPHY_IVCRX       0x0000702e
#define  SR_ADAP_CTL3_DPHY_IVCRX       0x0000702f
#define  SR_ADAP_CTL4_DPHY_IVCRX       0x00007030
#define  SR_ADAP_CTL5_DPHY_IVCRX       0x00007031
#define  SR_ADAP_CTL6_DPHY_IVCRX       0x00007032
#define  SR_ADAP_CTL7_DPHY_IVCRX       0x00007033
#define  SR_ADAP_CTL8_DPHY_IVCRX       0x00007034
#define SR_ADAP_CTL9_DPHY_IVCRX       0x00007035
#define SR_ADAP_CTL10_DPHY_IVCRX       0x00007036
#define SR_ADAP_CTL11_DPHY_IVCRX       0x00007037
#define SR_ADAP_CTL12_DPHY_IVCRX       0x00007038
#define SR_ADAP_CTL13_DPHY_IVCRX       0x00007039
#define SR_ADAP_CTL14_DPHY_IVCRX       0x0000703a
#define SR_ADAP_CTL15_DPHY_IVCRX       0x0000703b
#define SR_ADAP_CTL16_DPHY_IVCRX       0x0000703c
#define SR_ADAP_CTL17_DPHY_IVCRX       0x0000703d
#define SR_ADAP_CTL18_DPHY_IVCRX       0x0000703e
#define SR_ADAP_CTL19_DPHY_IVCRX       0x0000703f
#define SR_ADAP_CTL20_DPHY_IVCRX       0x00007040
#define SR_ADAP_CTL21_DPHY_IVCRX       0x00007041
#define SR_ADAP_CTL22_DPHY_IVCRX       0x00007042
#define SR_ADAP_CTL23_DPHY_IVCRX       0x00007043
#define SR_ADAP_CTL24_DPHY_IVCRX       0x00007044
#define SR_ADAP_CTL25_DPHY_IVCRX       0x00007045
#define SR_ADAP_CTL26_DPHY_IVCRX       0x00007046
#define SR_ADAP_CTL32_DPHY_IVCRX       0x0000704c
#define SR_ADAP_CTL33_DPHY_IVCRX       0x0000704d
#define SR_ADAP_CTL34_DPHY_IVCRX       0x0000704e
#define SR_ADAP_CTL35_DPHY_IVCRX       0x0000704f
#define SR_ADAP_CTL36_DPHY_IVCRX       0x00007050
#define SR_ADAP_CTL37_DPHY_IVCRX       0x00007051
#define SR_ADAP_CTL38_DPHY_IVCRX       0x00007052
#define SR_ADAP_CTL39_DPHY_IVCRX       0x00007053
#define SR_ADAP_CTL41_DPHY_IVCRX       0x00007055
#define SR_ADAP_CTL42_DPHY_IVCRX       0x00007056
#define SR_ADAP_CTL43_DPHY_IVCRX       0x00007057
#define SR_ADAP_CTL45_DPHY_IVCRX       0x00007059
#define SR_ADAP_CTL46_DPHY_IVCRX       0x0000705a
#define SR_ADAP_CTL47_DPHY_IVCRX       0x0000705b
#define SARAH_ADAP_ST_0_DPHY_IVCRX       0x0000705c
#define SARAH_ADAP_ST_1_DPHY_IVCRX       0x0000705d
#define SARAH_ADAP_ST_2_DPHY_IVCRX       0x0000705e
#define SARAH_ADAP_ST_3_DPHY_IVCRX       0x0000705f
#define SARAH_ADAP_ST_4_DPHY_IVCRX       0x00007060
#define SARAH_ADAP_ST_5_DPHY_IVCRX       0x00007061
#define SARAH_ADAP_ST_6_DPHY_IVCRX       0x00007062
#define SARAH_ADAP_ST_7_DPHY_IVCRX       0x00007063
#define SARAH_ADAP_ST_8_DPHY_IVCRX       0x00007064
#define SARAH_ADAP_ST_9_DPHY_IVCRX       0x00007065
#define SARAH_ADAP_ST_10_DPHY_IVCRX       0x00007066
#define SARAH_ADAP_ST_11_DPHY_IVCRX       0x00007067
#define SARAH_ADAP_ST_12_DPHY_IVCRX       0x00007068
#define SR_CP_CTL0_DPHY_IVCRX       0x00007069
#define SR_CP_CTL1_DPHY_IVCRX       0x0000706a
#define SR_DP_CTL0_DPHY_IVCRX       0x0000706b
#define SR_DP_CTL1_DPHY_IVCRX       0x0000706c
#define SR_DP_CTL2_DPHY_IVCRX       0x0000706d
#define SR_DP_CTL3_DPHY_IVCRX       0x0000706e
#define SR_DFE0_CTL0_DPHY_IVCRX       0x0000706f
#define SR_DFE0_CTL1_DPHY_IVCRX       0x00007070
#define SR_DFE0_CTL2_DPHY_IVCRX       0x00007071
#define SR_DFE0_CTL3_DPHY_IVCRX       0x00007072
#define SR_DFE0_CTL4_DPHY_IVCRX       0x00007073
#define SR_DFE0_CTL5_DPHY_IVCRX       0x00007074
#define SR_DFE0_CTL6_DPHY_IVCRX       0x00007075
#define SR_DFE0_CTL7_DPHY_IVCRX       0x00007076
#define SR_DFE0_CTL8_DPHY_IVCRX       0x00007077
#define SR_DFE0_CTL9_DPHY_IVCRX       0x00007078
#define SR_DFE0_CTL10_DPHY_IVCRX       0x00007079
#define SR_DFE1_CTL0_DPHY_IVCRX       0x0000707a
#define SR_DFE1_CTL1_DPHY_IVCRX       0x0000707b
#define SR_DFE1_CTL2_DPHY_IVCRX       0x0000707c
#define SR_DFE1_CTL3_DPHY_IVCRX       0x0000707d
#define SR_DFE1_CTL4_DPHY_IVCRX       0x0000707e
#define SR_DFE1_CTL5_DPHY_IVCRX       0x0000707f
#define SR_DFE1_CTL6_DPHY_IVCRX       0x00007080
#define SR_DFE1_CTL7_DPHY_IVCRX       0x00007081
#define SR_DFE1_CTL8_DPHY_IVCRX       0x00007082
#define SR_DFE1_CTL9_DPHY_IVCRX       0x00007083
#define SR_DFE1_CTL15_DPHY_IVCRX       0x00007084
#define SR_DFE2_CTL0_DPHY_IVCRX       0x00007085
#define SR_DFE2_CTL1_DPHY_IVCRX       0x00007086
#define SR_DFE2_CTL2_DPHY_IVCRX       0x00007087
#define SR_DFE2_CTL3_DPHY_IVCRX       0x00007088
#define SR_DFE2_CTL4_DPHY_IVCRX       0x00007089
#define SR_DFE2_CTL5_DPHY_IVCRX       0x0000708a
#define SR_DFE2_CTL6_DPHY_IVCRX       0x0000708b
#define SR_DFE2_CTL7_DPHY_IVCRX       0x0000708c
#define SR_DFE2_CTL8_DPHY_IVCRX       0x0000708d
#define SR_DFE2_CTL9_DPHY_IVCRX       0x0000708e
#define SR_DFE2_CTL10_DPHY_IVCRX       0x0000708f
#define SR_DFE3_CTL0_DPHY_IVCRX       0x00007090
#define SR_DFE3_CTL1_DPHY_IVCRX       0x00007091
#define SR_DFE3_CTL2_DPHY_IVCRX       0x00007092
#define SR_DFE3_CTL3_DPHY_IVCRX       0x00007093
#define SR_DFE3_CTL4_DPHY_IVCRX       0x00007094
#define SR_DFE3_CTL5_DPHY_IVCRX       0x00007095
#define SR_DFE3_CTL6_DPHY_IVCRX       0x00007096
#define SR_DFE3_CTL7_DPHY_IVCRX       0x00007097
#define SR_DFE3_CTL8_DPHY_IVCRX       0x00007098
#define SR_DFE3_CTL9_DPHY_IVCRX       0x00007099
#define SR_DFE3_CTL10_DPHY_IVCRX       0x0000709a
#define SR_PLL_CTL0_DPHY_IVCRX       0x0000709b
#define SR_PLL_CTL1_DPHY_IVCRX       0x0000709c
#define SR_PLL_CTL2_DPHY_IVCRX       0x0000709d
#define SR_PLL_CTL3_DPHY_IVCRX       0x0000709e
#define SR_PLL_CTL4_DPHY_IVCRX       0x0000709f
#define SR_PLL_CTL5_DPHY_IVCRX       0x000070a0
#define SR_PLL_CTL6_DPHY_IVCRX       0x000070a1
#define SR_PLL_CTL7_DPHY_IVCRX       0x000070a2
#define SR_PLL_CTL8_DPHY_IVCRX       0x000070a3
#define SR_PLL_CTL9_DPHY_IVCRX       0x000070a4
#define SR_PLL_CTL10_DPHY_IVCRX       0x000070a5
#define SR_PLL_CTL11_DPHY_IVCRX       0x000070a6
#define SR_PLL_CTL12_DPHY_IVCRX       0x000070a7
#define SR_PLL_CTL13_DPHY_IVCRX       0x000070a8
#define SR_PLL_CTL14_DPHY_IVCRX       0x000070a9
#define SR_PLL_CTL15_DPHY_IVCRX       0x000070aa
#define SR_PLL_CTL16_DPHY_IVCRX       0x000070ab
#define SR_PLL_CTL17_DPHY_IVCRX       0x000070ac
#define SR_PLL_CTL18_DPHY_IVCRX       0x000070ad
#define SR_PLL_CTL19_DPHY_IVCRX       0x000070ae
#define SR_PLL_CTL20_DPHY_IVCRX       0x000070af
#define SR_PLL_CTL21_DPHY_IVCRX       0x000070b0
#define	SR_PLL_CTL22_DPHY_IVCRX		0x000070b1
#define SR_PLL_CTL23_DPHY_IVCRX		0x000070b2
#define SR_BIAS_CTL0_DPHY_IVCRX		0x000070b3
#define SR_BIAS_CTL1_DPHY_IVCRX		0x000070b4
#define SR_BIAS_CTL2_DPHY_IVCRX		0x000070b5
#define SR_OSC_CTL0_DPHY_IVCRX		0x000070b6
#define SR_OSC_CTL1_DPHY_IVCRX		0x000070b7
#define SARAH_ANA_ST_0_DPHY_IVCRX       0x000070bd
#define SARAH_ANA_ST_1_DPHY_IVCRX       0x000070be
#define SARAH_ANA_ST_2_DPHY_IVCRX       0x000070bf
#define SARAH_ANA_ST_3_DPHY_IVCRX       0x000070c0
#define SARAH_ANA_ST_4_DPHY_IVCRX       0x000070c1
#define SARAH_ANA_ST_5_DPHY_IVCRX       0x000070c2
#define SARAH_ANA_ST_6_DPHY_IVCRX       0x000070c3
#define SARAH_ANA_ST_7_DPHY_IVCRX       0x000070c4
#define SARAH_ANA_ST_8_DPHY_IVCRX       0x000070c5
#define SARAH_ANA_ST_9_DPHY_IVCRX       0x000070c6
#define SARAH_ANA_ST_10_DPHY_IVCRX	0x000070c7
#define SARAH_ANA_ST_11_DPHY_IVCRX	0x000070c8
#define SARAH_ANA_ST_12_DPHY_IVCRX	0x000070c9
#define SARAH_ANA_ST_13_DPHY_IVCRX	0x000070ca
#define SARAH_ANA_ST_14_DPHY_IVCRX	0x000070cb
#define SARAH_ANA_ST_15_DPHY_IVCRX	0x000070cc
#define SARAH_DBG_CTL0_DPHY_IVCRX       0x000070d0
#define SARAH_DBG_CTL1_DPHY_IVCRX       0x000070d1
#define SARAH_DBG_CTL2_DPHY_IVCRX       0x000070d2
#define SARAH_DBG_CTL3_DPHY_IVCRX       0x000070d3
#define SARAH_DBG_CTL4_DPHY_IVCRX       0x000070d4
#define SARAH_DBG_CTL5_DPHY_IVCRX       0x000070d5
#define SARAH_EQ_SET0_DPHY_IVCRX	0x000070d6
#define SARAH_EQ_SET1_DPHY_IVCRX	0x000070d7
#define SARAH_EQ_SET2_DPHY_IVCRX	0x000070d8
#define SARAH_EQ_SET3_DPHY_IVCRX	0x000070d9
#define SARAH_EQ_SET4_DPHY_IVCRX	0x000070da
#define SARAH_EQ_SET5_DPHY_IVCRX	0x000070db
#define SARAH_EQ_SET6_DPHY_IVCRX	0x000070dc
#define SARAH_EQ_SET7_DPHY_IVCRX	0x000070dd
#define SARAH_EQ_SET8_DPHY_IVCRX	0x000070de
#define SARAH_EQ_SET9_DPHY_IVCRX	0x000070df
#define SARAH_EQ_SET10_DPHY_IVCRX       0x000070e0
#define SARAH_EQ_SET11_DPHY_IVCRX       0x000070e1
#define SARAH_EQ_SET12_DPHY_IVCRX       0x000070e2
#define SARAH_EQ_SET13_DPHY_IVCRX       0x000070e3
#define SARAH_EQ_SET14_DPHY_IVCRX       0x000070e4
#define SARAH_EQ_SET15_DPHY_IVCRX	0x000070e5
#define SR_LCDT_CTL0_DPHY_IVCRX		0x000070e9
#define SR_DLL_CDR_ST_DPHY_IVCRX	0x000070f1
#define SARAH_CKDT_CTL_DPHY_IVCRX	0x000070fd

enum hdcp14_key_mode_e {
	NORMAL_MODE,
	SECURE_MODE,
};

enum measure_clk_top_e {
	TOP_HDMI_TMDSCLK = 0,
	TOP_HDMI_CABLECLK,
	TOP_HDMI_AUDIOCLK,
};

enum measure_clk_src_e {
	MEASURE_CLK_CABLE,
	MEASURE_CLK_TMDS,
	MEASURE_CLK_PIXEL,
	MEASURE_CLK_MPLL,
	MEASURE_CLK_AUD_PLL,
	MEASURE_CLK_AUD_DIV,
	MEASURE_CLK_PCLK,
};

#define MHz	1000000
#define KHz	1000

#define PHY_DEFAULT_FRQ	((100) * MHz)
#define MAX_TMDS_CLK 340000000
#define MIN_TMDS_CLK 23000000

enum phy_frq_band {
	PHY_BW_0 = 0,	/*45Mhz*/
	PHY_BW_1,		/*77Mhz*/
	PHY_BW_2,		/*155Mhz*/
	PHY_BW_3,		/*340Mhz*/
	PHY_BW_4,		/*525Mhz*/
	PHY_BW_5,		/*600Mhz*/
	PHY_BW_NULL = 0xf,
};

enum pll_frq_band {
	PLL_BW_0 = 0,	/*35Mhz*/
	PLL_BW_1,		/*77Mhz*/
	PLL_BW_2,		/*155Mhz*/
	PLL_BW_3,		/*300Mhz*/
	PLL_BW_4,		/*600Mhz*/
	PLL_BW_NULL = 0xf,
};

struct apll_param {
	unsigned int bw;
	unsigned int M;
	unsigned int N;
	unsigned int od;
	unsigned int od_div;
	unsigned int od2;
	unsigned int od2_div;
	unsigned int aud_div;
};

extern unsigned int hdmirx_addr_port;
extern unsigned int hdmirx_data_port;
extern unsigned int hdmirx_ctrl_port;
extern int acr_mode;
extern int hdcp_enc_mode;
extern int force_clk_rate;
extern int rx_afifo_dbg_en;
extern int auto_aclk_mute;
extern int aud_avmute_en;
extern int aud_mute_sel;
extern int pdec_ists_en;
extern int pd_fifo_start_cnt;
extern int md_ists_en;
extern int aud_ch_map;
extern int hdcp14_key_mode;
extern int ignore_sscp_charerr;
extern int ignore_sscp_tmds;
extern int find_best_eq;
extern int eq_try_cnt;
extern int pll_rst_max;
extern int cdr_lock_level;
extern int top_intr_maskn_value;
extern int hbr_force_8ch;
extern bool term_cal_en;
extern int clock_lock_th;
extern int scdc_force_en;
extern u32 hdcp_hpd_ctrl_en;
extern int eq_dbg_lvl;
extern int phy_term_lel;
extern bool phy_tdr_en;
extern int hdcp_tee_path;
extern char emp_buf[1024];
extern int hdcp22_on;
extern int hdcp14_on;
extern bool hdcp22_kill_esm;
extern bool hpd_to_esm;
extern u32 term_cal_val;
extern u32 phy_trim_val;
extern u32 hdcp22_reauth_enable;
extern int i2c_err_cnt;
extern u32 rx_ecc_err_thres;
extern u32 rx_ecc_err_frames;
extern u8 ddc_dbg_en;
extern int kill_esm_fail;

void rx_get_best_eq_setting(void);
void wr_reg_hhi(unsigned int offset, unsigned int val);
void wr_reg_hhi_bits(unsigned int offset, unsigned int mask,
		     unsigned int val);
unsigned int rd_reg_hhi(unsigned int offset);
unsigned int rd_reg_hhi_bits(unsigned int offset, unsigned int mask);
unsigned int rd_reg(enum map_addr_module_e module,
		    unsigned int reg_addr);
void wr_reg(enum map_addr_module_e module,
	    unsigned int reg_addr,
	    unsigned int val);
unsigned char rd_reg_b(enum map_addr_module_e module,
		       unsigned int reg_addr);
void wr_reg_b(enum map_addr_module_e module,
	      unsigned int reg_addr,
	      unsigned char val);
void hdmirx_wr_top(unsigned int addr, unsigned int data);
unsigned int hdmirx_rd_top(unsigned int addr);
void hdmirx_wr_dwc(unsigned int addr, unsigned int data);
unsigned int hdmirx_rd_dwc(unsigned int addr);
unsigned int hdmirx_rd_bits_dwc(unsigned int addr,
				unsigned int mask);
void hdmirx_wr_bits_dwc(unsigned int addr,
			unsigned int mask,
			unsigned int value);
unsigned int hdmirx_wr_phy(unsigned int add,
			   unsigned int data);
unsigned int hdmirx_rd_phy(unsigned int addr);
void hdmirx_wr_bits_top(unsigned int addr,
			unsigned int mask,
			unsigned int value);
unsigned int rx_get_bits(unsigned int data,
			 unsigned int mask);
unsigned int rx_set_bits(unsigned int data,
			 unsigned int mask,
			 unsigned int value);
/*extern unsigned int hdmirx_get_tmds_clock(void);*/
/*extern unsigned int hdmirx_get_pixel_clock(void);*/
/*extern unsigned int hdmirx_get_audio_clock(void);*/
/*extern unsigned int hdmirx_get_esm_clock(void);*/
/*extern unsigned int hdmirx_get_cable_clock(void);*/
/*extern unsigned int hdmirx_get_mpll_div_clk(void);*/
/*extern unsigned int hdmirx_get_clock(int index);*/

/* hdcp22 */
void rx_hdcp22_wr_reg(unsigned int addr, unsigned int data);
unsigned int rx_hdcp22_rd_reg(unsigned int addr);
unsigned int rx_hdcp22_rd_bits_reg(unsigned int addr,
				   unsigned int mask);
void rx_hdcp22_wr_bits_reg(unsigned int addr,
			   unsigned int mask,
			   unsigned int value);
void rx_hdcp22_wr_top(unsigned int addr, unsigned int data);
unsigned int rx_hdcp22_rd_top(unsigned int addr);
unsigned int rx_hdcp22_rd(unsigned int addr);
void rx_sec_reg_write(unsigned int *addr, unsigned int value);
void rx_sec_reg_write(unsigned int *addr, unsigned int value);
unsigned int rx_sec_reg_read(unsigned int *addr);
unsigned int sec_top_read(unsigned int *addr);
void sec_top_write(unsigned int *addr, unsigned int value);
void rx_esm_tmdsclk_en(bool en);
void hdcp22_clk_en(bool en);
void hdmirx_hdcp22_esm_rst(void);
unsigned int rx_sec_set_duk(bool repeater);
void hdmirx_hdcp22_init(void);
void hdcp_22_off(void);
void hdcp_22_on(void);
void hdmirx_hdcp22_hpd(bool value);
void esm_set_reset(bool reset);
void esm_set_stable(bool stable);
void rx_hpd_to_esm_handle(struct work_struct *work);
void rx_hdcp14_resume(void);
void hdmirx_load_firm_reset(int type);
unsigned int hdmirx_packet_fifo_rst(void);
void rx_afifo_store_all_subpkt(bool all_pkt);
unsigned int hdmirx_audio_fifo_rst(void);
void hdmirx_phy_init(void);
void hdmirx_hw_config(void);
void hdmirx_hw_probe(void);
void rx_hdcp_init(void);
void hdmirx_phy_pddq(unsigned int enable);
void rx_get_video_info(void);
void hdmirx_set_video_mute(bool mute);
void hdmirx_config_video(void);
void hdmirx_config_audio(void);
void set_dv_ll_mode(bool en);
void rx_get_audinfo(struct aud_info_s *audio_info);
bool rx_clkrate_monitor(void);
void rx_ddc_calibration(bool en);
unsigned char rx_get_hdcp14_sts(void);
unsigned int rx_hdcp22_rd_reg_bits(unsigned int addr, unsigned int mask);
int rx_get_aud_pll_err_sts(void);
void rx_force_hpd_cfg(u8 hpd_level);
int rx_set_port_hpd(u8 port_id, bool val);
void rx_set_cur_hpd(u8 val, u8 func);
unsigned int rx_get_hdmi5v_sts(void);
unsigned int rx_get_hpd_sts(void);
void cec_hw_reset(unsigned int cec_sel);
void rx_force_hpd_cfg(u8 hpd_level);
void rx_force_rxsense_cfg(u8 level);
void rx_force_hpd_rxsense_cfg(u8 level);
void rx_audio_bandgap_rst(void);
void rx_audio_bandgap_en(void);
void rx_aml_eq_debug(int eq_lvl);
void rx_phy_rxsense_pulse(unsigned int t1, unsigned int t2, bool en);
void rx_phy_power_on(unsigned int onoff);
void dump_reg_phy(void);
int rx_get_clock(enum measure_clk_top_e clk_src);
unsigned int clk_util_clk_msr(unsigned int clk_mux);
unsigned int rx_measure_clock(enum measure_clk_src_e clksrc);
void aml_phy_init(void);
u32 aml_cable_clk_band(u32 cableclk, u32 clkrate);
u32 aml_phy_pll_band(u32 cableclk, u32 clkrate);
void aml_phy_switch_port(void);
void aml_phy_bw_switch(void);
unsigned int aml_phy_pll_lock(void);
unsigned int aml_phy_tmds_valid(void);
void aml_eq_setting(void);
void rx_emp_to_ddr_init(void);
void rx_emp_field_done_irq(void);
void rx_emp_status(void);
void rx_emp_lastpkt_done_irq(void);
void rx_tmds_to_ddr_init(void);
void rx_emp_capture_stop(void);
void rx_get_error_cnt(u32 *ch0, u32 *ch1, u32 *ch2);
void rx_get_audio_N_CTS(u32 *N, u32 *CTS);
void rx_run_eq(void);
bool rx_eq_done(void);
bool is_tmds_valid(void);
void hdmirx_top_irq_en(bool flag);
void rx_phy_rt_cal(void);
bool is_ft_trim_done(void);
void aml_phy_get_trim_val(void);
unsigned int rx_set_hdcp14_secure_key(void);
bool rx_clr_tmds_valid(void);
void rx_set_suspend_edid_clk(bool en);
void aml_phy_init_handler(struct work_struct *work);
bool is_tmds_clk_stable(void);
void rx_phy_short_bist(void);
void aml_phy_iq_skew_monitor(void);
void aml_eq_eye_monitor(void);
void aml_phy_power_off(void);

/* tl1 extern */
void aml_phy_init_tl1(void);
void dump_reg_phy_tl1_tm2(void);
void dump_aml_phy_sts_tl1(void);
void aml_phy_short_bist_tl1(void);
bool aml_get_tmds_valid_tl1(void);
void aml_phy_power_off_tl1(void);
void aml_phy_switch_port_tl1(void);

/* tm2 extern */
void aml_phy_short_bist_tm2(void);
void aml_phy_init_tm2(void);
void dump_aml_phy_sts_tm2(void);
bool aml_get_tmds_valid_tm2(void);
void aml_phy_power_off_tm2(void);
void aml_phy_switch_port_tm2(void);

/* t5 extern */
void dump_reg_phy_t5(void);
void aml_phy_init_t5(void);
void dump_aml_phy_sts_t5(void);
void aml_eq_eye_monitor_t5(void);
void aml_phy_offset_cal_t5(void);
void aml_phy_short_bist_t5(void);
void aml_phy_iq_skew_monitor_t5(void);
bool aml_get_tmds_valid_t5(void);
void hdmirx_wr_bits_amlphy(unsigned int addr,
			   unsigned int mask,
			   unsigned int value);
void hdmirx_wr_amlphy(unsigned int addr, unsigned int data);
u32 hdmirx_rd_bits_amlphy(u16 addr, u32 mask);
unsigned int hdmirx_rd_amlphy(unsigned int addr);
void aml_phy_power_off_t5(void);
void aml_phy_switch_port_t5(void);
void aml_phy_get_trim_val_t5(void);

void hdmirx_irq_hdcp_enable(bool enable);
u8 rx_get_avmute_sts(void);
/* T7 */
u8 hdmirx_rd_cor(u32 addr);
void hdmirx_wr_cor(u32 addr, u8 data);
u8 hdmirx_rd_bits_cor(u32 addr, u32 mask);
void hdmirx_wr_bits_cor(u32 addr, u32 mask, u8 value);
void dump_reg_phy_t7(void);
void aml_phy_init_t7(void);
void dump_aml_phy_sts_t7(void);
void aml_eq_eye_monitor_t7(void);
void aml_phy_offset_cal_t7(void);
void aml_phy_short_bist_t7(void);
void aml_phy_iq_skew_monitor_t7(void);
bool aml_get_tmds_valid_t7(void);
void aml_phy_power_off_t7(void);
void aml_phy_switch_port_t7(void);
unsigned int rx_sec_hdcp_cfg_t7(void);
void dump_vsi_reg_t7(void);
void rx_set_irq_t7(bool en);
void rx_set_aud_output_t7(u32 param);
void rx_sw_reset_t7(int level);
void aml_phy_get_trim_val_t7(void);
void rx_hdcp_22_sent_reauth(void);
void rx_hdcp_14_sent_reauth(void);
u32 rx_get_ecc_err(void);
u32 rx_get_ecc_pkt_cnt(void);
void rx_check_ecc_error(void);
void hdmirx_output_en(bool en);
void hdmirx_hbr2spdif(u8 val);
void rx_hdcp_monitor(void);
bool rx_need_support_fastswitch(void);
bool is_special_func_en(void);
void rx_afifo_monitor(void);
void rx_ddc_active_monitor(void);
void rx_clkmsr_monitor(void);
void rx_clkmsr_handler(struct work_struct *work);
void rx_i2c_err_monitor(void);
void rx_i2c_init(void);
bool is_ddc_filter_en(void);
void rx_aud_fifo_rst(void);
void rx_esm_reset(int level);
void hdmirx_hdcp22_reauth(void);
void rx_earc_hpd_handler(struct work_struct *work);
void rx_kill_esm(void);
#endif
