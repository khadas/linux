/*
 * drivers/amlogic/hdmi/hdmi_tx_20/hw/hdmi_tx_hw.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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
*/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
/* #include <linux/amports/canvas.h> */
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/amlogic/vout/vinfo.h>
#include <linux/amlogic/vout/enc_clk_config.h>
#ifdef CONFIG_PANEL_IT6681
#include <linux/it6681.h>
#endif
#include <linux/amlogic/hdmi_tx/hdmi_info_global.h>
#include <linux/amlogic/hdmi_tx/hdmi_tx_module.h>
/*#include <linux/amlogic/hdmi_tx/hdmi_tx_cec.h>*/
#include <linux/reset.h>
#include "mach_reg.h"
#include "hdmi_tx_reg.h"

#if 0   /* todo */
#include "../hdmi_tx_hdcp.h"
#include "../hdmi_tx_compliance.h"
#endif
#include "tvenc_conf.h"

#define EDID_RAM_ADDR_SIZE	 (8)

static void hdmi_audio_init(unsigned int spdif_flag);
static void hdmitx_dump_tvenc_reg(int cur_VIC, int pr_info_flag);

static void mode420_half_horizontal_para(void);
static void hdmi_phy_suspend(void);
static void hdmi_phy_wakeup(struct hdmitx_dev *hdev);
static void hdmitx_set_phy(struct hdmitx_dev *hdev);
static void C_Entry(enum hdmi_vic vic);
static void set_hdmi_audio_source(unsigned int src);
static void hdmitx_csc_config(unsigned char input_color_format,
	unsigned char output_color_format, unsigned char color_depth);
unsigned char hdmi_pll_mode = 0; /* 1, use external clk as hdmi pll source */

/* HSYNC polarity: active high */
#define HSYNC_POLARITY	 1
/* VSYNC polarity: active high */
#define VSYNC_POLARITY	 1
/* Pixel bit width: 0=24-bit; 1=30-bit; 2=36-bit; 3=48-bit. */
#define TX_INPUT_COLOR_DEPTH	0
/* Pixel format: 0=RGB444; 1=YCbCr444; 2=Rsrv; 3=YCbCr422. */
#define TX_INPUT_COLOR_FORMAT   1
/* Pixel range: 0=16-235/240; 1=16-240; 2=1-254; 3=0-255. */
#define TX_INPUT_COLOR_RANGE	0
/* Pixel bit width: 4=24-bit; 5=30-bit; 6=36-bit; 7=48-bit. */
#define TX_COLOR_DEPTH		 hdmi_color_depth_24B
/* Pixel format: 0=RGB444; 1=YCbCr422; 2=YCbCr444; 3=YCbCr420. */
#define TX_OUTPUT_COLOR_FORMAT  hdmi_color_format_444
#define TX_OUTPUT_COLOR_RANGE 0

#if 1
/* spdif */
/* 0=SPDIF; 1=I2S. */
/* 0=SPDIF; 1=I2S. Note: Must select I2S if CHIP_HAVE_HDMI_RX is defined. */
#define TX_I2S_SPDIF		1
/* 0=I2S 2-channel; 1=I2S 4 x 2-channel. */
#define TX_I2S_8_CHANNEL	0
#else
/* i2s 8 channel */
#define TX_I2S_SPDIF		1
#define TX_I2S_8_CHANNEL	1
#endif

/* static struct tasklet_struct EDID_tasklet; */
static unsigned delay_flag;
static unsigned long serial_reg_val = 0x1;
static unsigned char i2s_to_spdif_flag = 1;
static unsigned long color_depth_f;
static unsigned long color_space_f;
static unsigned char new_reset_sequence_flag = 1;
static unsigned char power_mode = 1;
static unsigned char power_off_vdac_flag;
/* 0, do not use fixed tvenc val for all mode;
 * 1, use fixed tvenc val mode for 480i;
 * 2, use fixed tvenc val mode for all modes
 */
static unsigned char use_tvenc_conf_flag = 1;

static unsigned char cur_vout_index = 1; /* CONFIG_AM_TV_OUTPUT2 */

static void hdmitx_set_packet(int type, unsigned char *DB, unsigned char *HB);
static void hdmitx_setaudioinfoframe(unsigned char *AUD_DB,
	unsigned char *CHAN_STAT_BUF);
static int hdmitx_set_dispmode(struct hdmitx_dev *hdev,
	struct hdmitx_vidpara *param);
static int hdmitx_set_audmode(struct hdmitx_dev *hdev,
	struct hdmitx_audpara *audio_param);
static void hdmitx_setupirq(struct hdmitx_dev *hdev);
static void hdmitx_debug(struct hdmitx_dev *hdev, const char *buf);
static void hdmitx_uninit(struct hdmitx_dev *hdev);
static int hdmitx_cntl(struct hdmitx_dev *hdev, unsigned cmd, unsigned argv);
static int hdmitx_cntl_ddc(struct hdmitx_dev *hdev, unsigned cmd,
	unsigned long argv);
static int hdmitx_get_state(struct hdmitx_dev *hdev, unsigned cmd,
	unsigned argv);
static int hdmitx_cntl_config(struct hdmitx_dev *hdev, unsigned cmd,
	unsigned argv);
static int hdmitx_cntl_misc(struct hdmitx_dev *hdev, unsigned cmd,
	unsigned argv);
static void digital_clk_on(unsigned char flag);
static void digital_clk_off(unsigned char flag);
static void tmp_generate_1080p(void);
/*
 * HDMITX HPD HW related operations
 */
enum hpd_op {
	HPD_INIT_DISABLE_PULLUP,
	HPD_INIT_SET_FILTER,
	HPD_IS_HPD_MUXED,
	HPD_MUX_HPD,
	HPD_UNMUX_HPD,
	HPD_READ_HPD_GPIO,
};

static int hdmitx_hpd_hw_op(enum hpd_op cmd)
{
	int ret = 0;
	switch (cmd) {
	case HPD_INIT_DISABLE_PULLUP:
		hd_set_reg_bits(P_PAD_PULL_UP_REG1, 0, 21, 1);
		break;
	case HPD_INIT_SET_FILTER:
		hdmitx_wr_reg(HDMITX_TOP_HPD_FILTER,
			((0xa << 12) | (0xa0 << 0)));
		break;
	case HPD_IS_HPD_MUXED:
		ret = !!(hd_read_reg(P_PERIPHS_PIN_MUX_1)&(1<<26));
		break;
	case HPD_MUX_HPD:
/* GPIOH_5 input */
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 1, 21, 1);
/* clear other pinmux */
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_1, 0, 19, 1);
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_1, 1, 26, 1);
		break;
	case HPD_UNMUX_HPD:
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_1, 0, 26, 1);
/* GPIOH_5 input */
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 1, 21, 1);
		break;
	case HPD_READ_HPD_GPIO:
		ret = !!(hd_read_reg(P_PREG_PAD_GPIO1_I) & (1 << 21));
		break;
	default:
		pr_info("error hpd cmd %d\n", cmd);
		break;
	}
	return ret;
}

int read_hpd_gpio(void)
{
	return !!(hd_read_reg(P_PREG_PAD_GPIO1_I) & (1 << 21));
}
EXPORT_SYMBOL(read_hpd_gpio);

/*
 * HDMITX DDC HW related operations
 */
enum ddc_op {
	DDC_INIT_DISABLE_PULL_UP_DN,
	DDC_MUX_DDC,
	DDC_UNMUX_DDC,
};

static int hdmitx_ddc_hw_op(enum ddc_op cmd)
{
	int ret = 0;

	switch (cmd) {
	case DDC_INIT_DISABLE_PULL_UP_DN:
/* Disable GPIOH_3/4 pull-up/down */
		hd_set_reg_bits(P_PAD_PULL_UP_EN_REG1, 0, 19, 2);
		hd_set_reg_bits(P_PAD_PULL_UP_REG1, 0, 19, 2);
		break;
	case DDC_MUX_DDC:
/* GPIOH_3/4 input */
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 3, 2, 2);
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_1, 3, 24, 2);
		break;
	case DDC_UNMUX_DDC:
/* GPIOH_3/4 input */
		hd_set_reg_bits(P_PREG_PAD_GPIO1_EN_N, 3, 19, 2);
		hd_set_reg_bits(P_PERIPHS_PIN_MUX_1, 0, 24, 2);
		break;
	default:
		pr_info("error ddc cmd %d\n", cmd);
	}
	return ret;
}


static void configure_hdcp_dpk(unsigned long sw_enc_key)
{
	const unsigned long hw_enc_key	= 0xcac;
	const unsigned char dpk_aksv[5] = {0x12};
	unsigned char dpk_key[280] = {
	/* test */
	};
	unsigned long sr_1;
	unsigned long sr_2;
	unsigned char mask;
	unsigned int i, j;
	unsigned int dpk_index;
	unsigned long sr2_b24_b1, sr2_b0, sr1_b24_b0;
	unsigned long sr1_b26_b0, sr1_b27, sr1_b24;

	pr_info("[HDMITX.C] Configure HDCP keys -- Begin\n");

	/* init key encrypt vectors */
	sr_1 = ((hw_enc_key&0xfff)<<16) | (sw_enc_key&0xffff);
	sr_2 = 0x1978F5E;

	/* encrypt keys loop */
	for (j = 0; j < 40; j++) {
		for (i = 0; i < 7; i++) {
			mask =   (((sr_2>>0)&0x1) << 7)  |
				(((sr_2>>2)&0x1) << 6)  |
				(((sr_2>>4)&0x1) << 5)  |
				(((sr_2>>6)&0x1) << 4)  |
				(((sr_2>>1)&0x1) << 3)  |
				(((sr_2>>3)&0x1) << 2)  |
				(((sr_2>>5)&0x1) << 1)  |
				(((sr_2>>7)&0x1) << 0);
		/* sr_2 shift + bit manipulation */
		sr2_b24_b1 = (sr_2>>1)&0xffffff;
		sr2_b0	= sr_2&0x1;
		sr1_b24_b0 = sr_1&0x1ffffff;
		sr_2 = (((sr2_b0<<24) | sr2_b24_b1) ^ sr1_b24_b0) & 0x1ffffff;
/* sr_2 = ((((sr_2&0x1)<<24) | ((sr_2>>1)&0xffffff)) ^
 * (sr_1&0x1ffffff)) & 0x1ffffff; */
/* sr_1 shift left + bit manipulation */
		sr1_b26_b0 = sr_1&0x7ffffff;
		sr1_b27 = (sr_1>>27)&0x1;
		sr1_b24 = (sr_1>>24)&0x1;
		sr_1 = (((sr1_b26_b0<<1) | sr1_b27) & 0xffffffe) +
			(sr1_b27 ^ sr1_b24);
/* sr_1 = ((((sr_1&0x7ffffff)<<1) | ((sr_1>>27)&0x1)) & 0xffffffe) +
 * (((sr_1>>27)0x1) ^ ((sr_1>>24)&0x1)); */
		/* Encrypt Key */
			dpk_key[j*7+(6-i)] = dpk_key[j*7+(6-i)] ^ mask;
		}
	}

	/* Disable key encryption for writing KSV */
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_RMLCTL, 0);
	/* wait for memory access ok */
	hdmitx_poll_reg(HDMITX_DWC_HDCPREG_RMLSTS, (1<<6), HZ);
	/* write AKSV (unecrypted) */
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_DPK6, 0);
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_DPK5, 0);
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_DPK4, dpk_aksv[4]);
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_DPK3, dpk_aksv[3]);
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_DPK2, dpk_aksv[2]);
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_DPK1, dpk_aksv[1]);
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_DPK0, dpk_aksv[0]);

	/* wait for memory access ok */
	hdmitx_poll_reg(HDMITX_DWC_HDCPREG_RMLSTS, (1<<6)|1, HZ);

	/* enable encryption */
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_RMLCTL, 1);

	/* configure seed */
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_SEED1, (sw_enc_key>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_SEED0, sw_enc_key&0xff);

	/* store encrypted keys */
	for (dpk_index = 0; dpk_index < 40; dpk_index++) {
		hdmitx_wr_reg(HDMITX_DWC_HDCPREG_DPK6, dpk_key[dpk_index*7+6]);
		hdmitx_wr_reg(HDMITX_DWC_HDCPREG_DPK5, dpk_key[dpk_index*7+5]);
		hdmitx_wr_reg(HDMITX_DWC_HDCPREG_DPK4, dpk_key[dpk_index*7+4]);
		hdmitx_wr_reg(HDMITX_DWC_HDCPREG_DPK3, dpk_key[dpk_index*7+3]);
		hdmitx_wr_reg(HDMITX_DWC_HDCPREG_DPK2, dpk_key[dpk_index*7+2]);
		hdmitx_wr_reg(HDMITX_DWC_HDCPREG_DPK1, dpk_key[dpk_index*7+1]);
		hdmitx_wr_reg(HDMITX_DWC_HDCPREG_DPK0, dpk_key[dpk_index*7+0]);
		hdmitx_poll_reg(HDMITX_DWC_HDCPREG_RMLSTS,
			(1<<6)|((dpk_index == 39) ? 40:(dpk_index+2)), HZ);
	}

	pr_info("[HDMITX.C] Configure HDCP keys -- End\n");
}   /* configure_hdcp_dpk */

#define HDCP_AN_SW_VAL_HI 0x88a663a8
#define HDCP_AN_SW_VAL_LO 0xb4317416

static void hdmitx_hdcp_test(void)
{
	unsigned int data32 = 0;

	/* Configure HDCP */
	data32 = 0;
	data32 |= (0 << 7);  /* [  7] hdcp_engaged_int_mask */
	data32 |= (0 << 6);  /* [  6] hdcp_failed_int_mask */
	data32 |= (0 << 4);  /* [  4] i2c_nack_int_mask */
	data32 |= (0 << 3);  /* [  3] lost_arbitration_int_mask */
	data32 |= (0 << 2);  /* [  2] keepout_error_int_mask */
	data32 |= (0 << 1);  /* [  1] ksv_sha1_calc_int_mask */
	data32 |= (1 << 0);  /* [  0] ksv_access_int_mask */
	hdmitx_wr_reg(HDMITX_DWC_A_APIINTMSK, data32);

	data32 = 0;
	data32 |= (0 << 5);  /* [6:5] unencryptconf */
	data32 |= (1 << 4);  /* [  4] dataenpol */
	data32 |= (1 << 3);  /* [  3] vsyncpol */
	data32 |= (1 << 1);  /* [  1] hsyncpol */
	hdmitx_wr_reg(HDMITX_DWC_A_VIDPOLCFG, data32);

	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_AN0, (HDCP_AN_SW_VAL_LO >> 0)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_AN1, (HDCP_AN_SW_VAL_LO >> 8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_AN2, (HDCP_AN_SW_VAL_LO>>16)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_AN3, (HDCP_AN_SW_VAL_LO>>24)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_AN4, (HDCP_AN_SW_VAL_HI >> 0)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_AN5, (HDCP_AN_SW_VAL_HI >> 8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_AN6, (HDCP_AN_SW_VAL_HI>>16)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_AN7, (HDCP_AN_SW_VAL_HI>>24)&0xff);
/* 0=AN is from HW; 1=AN is from SW register. */
	hdmitx_wr_reg(HDMITX_DWC_HDCPREG_ANCONF, 0);

	hdmitx_wr_reg(HDMITX_DWC_A_OESSWCFG, 0x40);

	data32 = 0;
	data32 |= (0 << 3);  /* [  3] sha1_fail */
	data32 |= (0 << 2);  /* [  2] ksv_ctrl_update */
	data32 |= (0 << 1);  /* [  1] Rsvd for read-only ksv_mem_access */
	data32 |= (1 << 0);  /* [  0] ksv_mem_request */
	hdmitx_wr_reg(HDMITX_DWC_A_KSVMEMCTRL, data32);
	hdmitx_poll_reg(HDMITX_DWC_A_KSVMEMCTRL, (1<<1), 2 * HZ);
	hdmitx_wr_reg(HDMITX_DWC_HDCP_REVOC_SIZE_0, 0);
	hdmitx_wr_reg(HDMITX_DWC_HDCP_REVOC_SIZE_1, 0);
	data32 = 0;
	data32 |= (0 << 3);  /* [  3] sha1_fail */
	data32 |= (0 << 2);  /* [  2] ksv_ctrl_update */
	data32 |= (0 << 1);  /* [  1] Rsvd for read-only ksv_mem_access */
	data32 |= (0 << 0);  /* [  0] ksv_mem_request */
	hdmitx_wr_reg(HDMITX_DWC_A_KSVMEMCTRL, data32);
	data32 = 0;
	data32 |= (0 << 4);  /* [  4] hdcp_lock */
	data32 |= (0 << 3);  /* [  3] dissha1check */
	data32 |= (1 << 2);  /* [  2] ph2upshiftenc */
	data32 |= ((0?0:1) << 1);  /* [  1] encryptiondisable */
/* [  0] swresetn. Write 0 to activate, self-clear to 1. */
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_A_HDCPCFG1, data32);

	configure_hdcp_dpk(0xa938);

	/* initialize HDCP, with rxdetect low */
	data32 = 0;
	data32 |= (0 << 7);  /* [  7] ELV_ena */
	data32 |= (1 << 6);  /* [  6] i2c_fastmode */
	data32 |= ((0?0:1) << 5);  /* [  5] byp_encryption */
	data32 |= (1 << 4);  /* [  4] sync_ri_check */
	data32 |= (0 << 3);  /* [  3] avmute */
	data32 |= (0 << 2);  /* [  2] rxdetect */
	data32 |= (1 << 1);  /* [  1] en11_feature */
	data32 |= (1 << 0);  /* [  0] hdmi_dvi */
	hdmitx_wr_reg(HDMITX_DWC_A_HDCPCFG0, data32);

	pr_info("[TEST.C] Start HDCP\n");
	hdmitx_wr_reg(HDMITX_DWC_A_HDCPCFG0,
		hdmitx_rd_reg(HDMITX_DWC_A_HDCPCFG0) | (1<<2));

}

static int hdmitx_uboot_already_display(void)
{
	if ((hd_read_reg(P_HHI_HDMI_CLK_CNTL) & (1 << 8))
		&& (hd_read_reg(P_HHI_HDMI_PLL_CNTL) & (1 << 31))
		&& (hdmitx_rd_reg(HDMITX_DWC_FC_AVIVID))) {
		pr_info("hdmitx: alread display in uboot\n");
		return 1;
	} else
		return 0;
}

static struct hdmitx_clk hdmitx_clk[] = {
	/* vic		clk_sys  clk_phy   clk_encp clk_enci clk_pixel */
	{HDMI_1080p60, 24000000, 1485000000, 148500000, -1, 148500000},
	{HDMI_1080p50, 24000000, 1485000000, 148500000, -1, 148500000},
	{HDMI_1080p24, 24000000, 742500000, 74250000, -1, 74250000},
	{HDMI_1080i60, 24000000, 742500000, 148500000, -1, 74250000},
	{HDMI_1080i50, 24000000, 742500000, 148500000, -1, 74250000},
	{HDMI_720p60, 24000000, 742500000, 148500000, -1, 74250000},
	{HDMI_720p50, 24000000, 742500000, 148500000, -1, 74250000},
	{HDMI_576p50, 24000000, 270000000, 54000000, -1, 27000000},
	{HDMI_576i50, 24000000, 270000000, -1, 54000000, 27000000},
	{HDMI_480p60, 24000000, 270000000, 54000000, -1, 27000000},
	{HDMI_480i60, 24000000, 270000000, -1, 54000000, 27000000},
	{HDMI_4k2k_24, 24000000, 2970000000UL, 297000000, -1, 297000000},
	{HDMI_4k2k_25, 24000000, 2970000000UL, 297000000, -1, 297000000},
	{HDMI_4k2k_30, 24000000, 2970000000UL, 297000000, -1, 297000000},
	{HDMI_4k2k_smpte_24, 24000000, 2970000000UL, 297000000, -1, 297000000},
};

static void set_vmode_clk(struct hdmitx_dev *hdev, enum hdmi_vic vic)
{
	int i;
	struct hdmitx_clk *clk = NULL;

	for (i = 0; i < ARRAY_SIZE(hdmitx_clk); i++) {
		if (vic == hdmitx_clk[i].vic)
			clk = &hdmitx_clk[i];
	}

	if (!clk) {
		pr_info("hdmitx: not find clk of VIC = %d\n", vic);
		return;
	} else {
#if 0
		if (hdev->clk_sys)
			clk_set_rate(hdev->clk_sys, clk->clk_sys);
		if (hdev->clk_phy)
			clk_set_rate(hdev->clk_phy, clk->clk_phy);
		if ((clk->clk_encp != -1) && (hdev->clk_encp))
			clk_set_rate(hdev->clk_encp, clk->clk_encp);
		if ((clk->clk_enci != -1) && (hdev->clk_enci))
			clk_set_rate(hdev->clk_enci, clk->clk_enci);
		if (hdev->clk_pixel)
			clk_set_rate(hdev->clk_pixel, clk->clk_pixel);
#endif
		pr_info("hdmitx: set clk of VIC = %d done\n", vic);
	}
}

static void hdmi_hwp_init(struct hdmitx_dev *hdev)
{
	/* Enable clocks and bring out of reset */

	/* Enable hdmitx_sys_clk */
	/* .clk0 ( cts_oscin_clk ), */
	/* .clk1 ( fclk_div4 ), */
	/* .clk2 ( fclk_div3 ), */
	/* .clk3 ( fclk_div5 ), */
/* [10: 9] clk_sel. select cts_oscin_clk=24MHz */
/* [	8] clk_en. Enable gated clock */
/* [ 6: 0] clk_div. Divide by 1. = 24/1 = 24 MHz */
	hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 0x100, 0, 16);

/* Enable clk81_hdmitx_pclk */
	hd_set_reg_bits(P_HHI_GCLK_MPEG2, 1, 4, 1);
	/* wire	wr_enable = control[3]; */
	/* wire	fifo_enable = control[2]; */
	/* assign phy_clk_en = control[1]; */
/* Bring HDMITX MEM output of power down */
	hd_set_reg_bits(P_HHI_MEM_PD_REG0, 0, 8, 8);
	tmp_generate_1080p();

	/* Enable APB3 fail on error */
	hd_set_reg_bits(P_HDMITX_CTRL_PORT, 1, 15, 1);
	hd_set_reg_bits((P_HDMITX_CTRL_PORT + 0x10), 1, 15, 1);

	hdmitx_hpd_hw_op(HPD_INIT_DISABLE_PULLUP);
	hdmitx_hpd_hw_op(HPD_INIT_SET_FILTER);
	hdmitx_ddc_hw_op(DDC_INIT_DISABLE_PULL_UP_DN);
/* 1=Map data pins from Venc to Hdmi Tx as RGB mode. */
/* Configure HDMI TX analog, and use HDMI PLL to generate TMDS clock */
/* Enable APB3 fail on error */
/* WRITE_APB_REG(HDMI_CNTL_PORT, READ_APB_REG(HDMI_CNTL_PORT)|(1<<15));*/
/* \\ TODO */
	if (hdmitx_uboot_already_display())
		return;
/*	tmp_generate_1080p(); */
	set_vmode_clk(hdev, HDMI_1920x1080p60_16x9);
	C_Entry(HDMI_1920x1080p60_16x9);
	hdmitx_set_phy(hdev);

	hd_write_reg(P_ENCP_VIDEO_EN, 1);
	set_hdmi_audio_source(2);
	hdmitx_set_reg_bits(HDMITX_DWC_FC_INVIDCONF, 1, 3, 1);
	hdmitx_wr_reg(HDMITX_DWC_FC_AVIVID, HDMI_1920x1080p60_16x9);
	/* TODO */

	/* clock gate on */

	/* detect display alread on uboot */

	/* tx h/w init */
}

static void hdmi_hwi_init(struct hdmitx_dev *hdev)
{
	unsigned int data32 = 0;
/* Configure E-DDC interface */
	data32 = 0;
	data32 |= (0 << 6);  /* [  6] read_req_mask */
	data32 |= (0 << 2);  /* [  2] done_mask */
	hdmitx_wr_reg(HDMITX_DWC_I2CM_INT, data32);

	data32 = 0;
	data32 |= (0 << 6);  /* [  6] nack_mask */
	data32 |= (0 << 2);  /* [  2] arbitration_error_mask */
	hdmitx_wr_reg(HDMITX_DWC_I2CM_CTLINT, data32);

/* [  3] i2c_fast_mode: 0=standard mode; 1=fast mode. */
	data32 = 0;
	data32 |= (0 << 3);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_DIV, data32);

	hdmitx_wr_reg(HDMITX_DWC_I2CM_SS_SCL_HCNT_1, 0);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SS_SCL_HCNT_0, 0x60);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SS_SCL_LCNT_1, 0);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SS_SCL_LCNT_0, 0x71);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_FS_SCL_HCNT_1, 0);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_FS_SCL_HCNT_0, 0x0f);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_FS_SCL_LCNT_1, 0);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_FS_SCL_LCNT_0, 0x20);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SDA_HOLD,	0x08);

	data32 = 0;
	data32 |= (0 << 5);  /* [  5] updt_rd_vsyncpoll_en */
	data32 |= (0 << 4);  /* [  4] read_request_en  // scdc */
	data32 |= (0 << 0);  /* [  0] read_update */
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SCDC_UPDATE,  data32);
}

void HDMITX_Meson_Init(struct hdmitx_dev *hdev)
{
	hdev->HWOp.SetPacket = hdmitx_set_packet;
	hdev->HWOp.SetAudioInfoFrame = hdmitx_setaudioinfoframe;
	hdev->HWOp.SetDispMode = hdmitx_set_dispmode;
	hdev->HWOp.SetAudMode = hdmitx_set_audmode;
	hdev->HWOp.SetupIRQ = hdmitx_setupirq;
	hdev->HWOp.DebugFun = hdmitx_debug;
	hdev->HWOp.UnInit = hdmitx_uninit;
	hdev->HWOp.Cntl = hdmitx_cntl;	/* todo */
	hdev->HWOp.CntlDDC = hdmitx_cntl_ddc;
	hdev->HWOp.GetState = hdmitx_get_state;
	hdev->HWOp.CntlPacket = hdmitx_cntl;
	hdev->HWOp.CntlConfig = hdmitx_cntl_config;
	hdev->HWOp.CntlMisc = hdmitx_cntl_misc;

	digital_clk_on(0xff);
	hdmi_hwp_init(hdev);
	hdmi_hwi_init(hdev);
	hdmitx_set_audmode(NULL, NULL);
}

static irqreturn_t intr_handler(int irq, void *dev)
{
	unsigned int data32 = 0;
	struct hdmitx_dev *hdev = (struct hdmitx_dev *)dev;
	/* get interrupt status */
	data32 = hdmitx_rd_reg(HDMITX_TOP_INTR_STAT);
	hdmi_print(IMP, SYS "irq %x\n", data32);
	if (hdev->hpd_lock == 1) {
		hdmitx_wr_reg(HDMITX_TOP_INTR_STAT_CLR, 0xf);
		hdmi_print(IMP, HPD "HDMI hpd locked\n");
		return IRQ_HANDLED;
	}
	/* check HPD status */
	if ((data32 & (1 << 1)) && (data32 & (1 << 2))) {
		if (hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO))
			data32 &= ~(1 << 2);
		else
			data32 &= ~(1 << 1);
	}
	/* internal interrupt */
	if (data32 & (1 << 0)) {
		hdev->hdmitx_event |= HDMI_TX_INTERNAL_INTR;
		PREPARE_WORK(&hdev->work_internal_intr,
			hdmitx_internal_intr_handler);
		queue_work(hdev->hdmi_wq, &hdev->work_internal_intr);
	}
	/* HPD rising */
	if (data32 & (1 << 1)) {
		hdev->hdmitx_event |= HDMI_TX_HPD_PLUGIN;
		hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGOUT;
		PREPARE_DELAYED_WORK(&hdev->work_hpd_plugin,
			hdmitx_hpd_plugin_handler);
		queue_delayed_work(hdev->hdmi_wq,
			&hdev->work_hpd_plugin, HZ / 3);
	}
	/* HPD falling */
	if (data32 & (1 << 2)) {
		hdev->hdmitx_event |= HDMI_TX_HPD_PLUGOUT;
		hdev->hdmitx_event &= ~HDMI_TX_HPD_PLUGIN;
		PREPARE_DELAYED_WORK(&hdev->work_hpd_plugout,
			hdmitx_hpd_plugout_handler);
		queue_delayed_work(hdev->hdmi_wq,
			&hdev->work_hpd_plugout, HZ / 3);
	}
	hdmitx_wr_reg(HDMITX_TOP_INTR_STAT_CLR, data32 | 0x6);
	return IRQ_HANDLED;
}

static unsigned long modulo(unsigned long a, unsigned long b)
{
	if (a >= b)
		return a - b;
	else
		return a;
}

static signed int to_signed(unsigned int a)
{
	if (a <= 7)
		return a;
	else
		return a - 16;
}

static void delay_us(int us)
{
	/* udelay(us); */
	if (delay_flag&0x1)
		mdelay((us+999)/1000);
} /* delay_us */

/*
 * mode: 1 means Progressive;  0 means interlaced
 */
static void enc_vpu_bridge_reset(int mode)
{
	unsigned int wr_clk = 0;

	wr_clk = (hd_read_reg(P_VPU_HDMI_SETTING) & 0xf00) >> 8;
	if (mode) {
		hd_write_reg(P_ENCP_VIDEO_EN, 0);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 0, 0, 2);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 0, 8, 4);
		mdelay(1);
		hd_write_reg(P_ENCP_VIDEO_EN, 1);
		mdelay(1);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, wr_clk, 8, 4);
		mdelay(1);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 2, 0, 2);
	} else {
		hd_write_reg(P_ENCI_VIDEO_EN, 0);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 0, 0, 2);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 0, 8, 4);
		mdelay(1);
		hd_write_reg(P_ENCI_VIDEO_EN, 1);
		mdelay(1);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, wr_clk, 8, 4);
		mdelay(1);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 0, 2);
	}
}

static void hdmi_tvenc1080i_set(struct hdmitx_vidpara *param)
{
	unsigned long VFIFO2VD_TO_HDMI_LATENCY = 2;
	unsigned long TOTAL_PIXELS = 0, PIXEL_REPEAT_HDMI = 0,
		PIXEL_REPEAT_VENC = 0, ACTIVE_PIXELS = 0;
	unsigned FRONT_PORCH = 88, HSYNC_PIXELS = 0, ACTIVE_LINES = 0,
		INTERLACE_MODE = 0, TOTAL_LINES = 0, SOF_LINES = 0,
		VSYNC_LINES = 0;
	unsigned LINES_F0 = 0, LINES_F1 = 563, BACK_PORCH = 0,
		EOF_LINES = 2, TOTAL_FRAMES = 0;

	unsigned long total_pixels_venc = 0;
	unsigned long active_pixels_venc = 0;
	unsigned long front_porch_venc = 0;
	unsigned long hsync_pixels_venc = 0;

	unsigned long de_h_begin = 0, de_h_end = 0;
	unsigned long de_v_begin_even = 0, de_v_end_even = 0,
		de_v_begin_odd = 0, de_v_end_odd = 0;
	unsigned long hs_begin = 0, hs_end = 0;
	unsigned long vs_adjust = 0;
	unsigned long vs_bline_evn = 0, vs_eline_evn = 0,
		vs_bline_odd = 0, vs_eline_odd = 0;
	unsigned long vso_begin_evn = 0, vso_begin_odd = 0;

	if (param->VIC == HDMI_1080i60) {
		INTERLACE_MODE = 1;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (1920*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (1080/(1+INTERLACE_MODE));
		LINES_F0 = 562;
		LINES_F1 = 563;
		FRONT_PORCH = 88;
		HSYNC_PIXELS = 44;
		BACK_PORCH = 148;
		EOF_LINES = 2;
		VSYNC_LINES = 5;
		SOF_LINES = 15;
		TOTAL_FRAMES = 4;
	} else if (param->VIC == HDMI_1080i50) {
		INTERLACE_MODE = 1;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (1920*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (1080/(1+INTERLACE_MODE));
		LINES_F0 = 562;
		LINES_F1 = 563;
		FRONT_PORCH = 528;
		HSYNC_PIXELS = 44;
		BACK_PORCH = 148;
		EOF_LINES = 2;
		VSYNC_LINES = 5;
		SOF_LINES = 15;
		TOTAL_FRAMES = 4;
	}
	TOTAL_PIXELS = (FRONT_PORCH+HSYNC_PIXELS+BACK_PORCH+ACTIVE_PIXELS);
	TOTAL_LINES = (LINES_F0+(LINES_F1*INTERLACE_MODE));

	total_pixels_venc = (TOTAL_PIXELS / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	active_pixels_venc = (ACTIVE_PIXELS / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	front_porch_venc = (FRONT_PORCH / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	hsync_pixels_venc =
		(HSYNC_PIXELS / (1+PIXEL_REPEAT_HDMI)) * (1+PIXEL_REPEAT_VENC);

	hd_write_reg(P_ENCP_VIDEO_MODE, hd_read_reg(P_ENCP_VIDEO_MODE)|(1<<14));

	/* Program DE timing */
	de_h_begin = modulo(hd_read_reg(P_ENCP_VIDEO_HAVON_BEGIN) +
		VFIFO2VD_TO_HDMI_LATENCY, total_pixels_venc);
	de_h_end  = modulo(de_h_begin + active_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCP_DE_H_BEGIN, de_h_begin);
	hd_write_reg(P_ENCP_DE_H_END, de_h_end);
	/* Program DE timing for even field */
	de_v_begin_even = hd_read_reg(P_ENCP_VIDEO_VAVON_BLINE);
	de_v_end_even  = de_v_begin_even + ACTIVE_LINES;
	hd_write_reg(P_ENCP_DE_V_BEGIN_EVEN, de_v_begin_even);
	hd_write_reg(P_ENCP_DE_V_END_EVEN,  de_v_end_even);
	/* Program DE timing for odd field if needed */
	if (INTERLACE_MODE) {
		de_v_begin_odd = to_signed((
			hd_read_reg(P_ENCP_VIDEO_OFLD_VOAV_OFST) & 0xf0)>>4)
			+ de_v_begin_even + (TOTAL_LINES-1)/2;
		de_v_end_odd = de_v_begin_odd + ACTIVE_LINES;
		hd_write_reg(P_ENCP_DE_V_BEGIN_ODD, de_v_begin_odd);/* 583 */
		hd_write_reg(P_ENCP_DE_V_END_ODD, de_v_end_odd);  /* 1123 */
	}

	/* Program Hsync timing */
	if (de_h_end + front_porch_venc >= total_pixels_venc) {
		hs_begin = de_h_end + front_porch_venc - total_pixels_venc;
		vs_adjust  = 1;
	} else {
		hs_begin = de_h_end + front_porch_venc;
		vs_adjust  = 0;
	}
	hs_end = modulo(hs_begin + hsync_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCP_DVI_HSO_BEGIN,  hs_begin);
	hd_write_reg(P_ENCP_DVI_HSO_END, hs_end);

	/* Program Vsync timing for even field */
	if (de_v_begin_even >= SOF_LINES + VSYNC_LINES + (1-vs_adjust))
		vs_bline_evn = de_v_begin_even - SOF_LINES - VSYNC_LINES
			- (1-vs_adjust);
	else
		vs_bline_evn = TOTAL_LINES + de_v_begin_even - SOF_LINES
			- VSYNC_LINES - (1-vs_adjust);

	vs_eline_evn = modulo(vs_bline_evn + VSYNC_LINES, TOTAL_LINES);
	hd_write_reg(P_ENCP_DVI_VSO_BLINE_EVN, vs_bline_evn);   /* 0 */
	hd_write_reg(P_ENCP_DVI_VSO_ELINE_EVN, vs_eline_evn);   /* 5 */
	vso_begin_evn = hs_begin; /* 2 */
	hd_write_reg(P_ENCP_DVI_VSO_BEGIN_EVN, vso_begin_evn);  /* 2 */
	hd_write_reg(P_ENCP_DVI_VSO_END_EVN, vso_begin_evn);  /* 2 */
	/* Program Vsync timing for odd field if needed */
	if (INTERLACE_MODE) {
		vs_bline_odd = de_v_begin_odd-1 - SOF_LINES - VSYNC_LINES;
		vs_eline_odd = de_v_begin_odd-1 - SOF_LINES;
		vso_begin_odd  = modulo(hs_begin + (total_pixels_venc>>1),
			total_pixels_venc);
		hd_write_reg(P_ENCP_DVI_VSO_BLINE_ODD, vs_bline_odd);
		hd_write_reg(P_ENCP_DVI_VSO_ELINE_ODD, vs_eline_odd);
		hd_write_reg(P_ENCP_DVI_VSO_BEGIN_ODD, vso_begin_odd);
		hd_write_reg(P_ENCP_DVI_VSO_END_ODD, vso_begin_odd);
	}

	hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
		(0 << 1) |
		(HSYNC_POLARITY << 2) |
		(VSYNC_POLARITY << 3) |
		(0 << 4) |
		(4 << 5) |
		(0 << 8) |
		(0 << 12)
	);
	hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);

}

static void hdmi_tvenc4k2k_set(struct hdmitx_vidpara *param)
{
	unsigned long VFIFO2VD_TO_HDMI_LATENCY = 2;
	unsigned long TOTAL_PIXELS = 4400, PIXEL_REPEAT_HDMI = 0,
		PIXEL_REPEAT_VENC = 0, ACTIVE_PIXELS = 3840;
	unsigned FRONT_PORCH = 1020, HSYNC_PIXELS = 0, ACTIVE_LINES = 2160,
		INTERLACE_MODE = 0, TOTAL_LINES = 0, SOF_LINES = 0,
		VSYNC_LINES = 0;
	unsigned LINES_F0 = 2250, LINES_F1 = 2250, BACK_PORCH = 0,
		EOF_LINES = 8, TOTAL_FRAMES = 0;

	unsigned long total_pixels_venc = 0;
	unsigned long active_pixels_venc = 0;
	unsigned long front_porch_venc = 0;
	unsigned long hsync_pixels_venc = 0;

	unsigned long de_h_begin = 0, de_h_end = 0;
	unsigned long de_v_begin_even = 0, de_v_end_even = 0,
		de_v_begin_odd = 0, de_v_end_odd = 0;
	unsigned long hs_begin = 0, hs_end = 0;
	unsigned long vs_adjust = 0;
	unsigned long vs_bline_evn = 0, vs_eline_evn = 0, vs_bline_odd = 0,
		vs_eline_odd = 0;
	unsigned long vso_begin_evn = 0, vso_begin_odd = 0;

	if ((param->VIC == HDMI_4k2k_30) ||
		(param->VIC == HDMI_3840x2160p60_16x9)) {
		INTERLACE_MODE = 0;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (3840*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (2160/(1+INTERLACE_MODE));
		LINES_F0 = 2250;
		LINES_F1 = 2250;
		FRONT_PORCH = 176;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 8 + 1;
		VSYNC_LINES = 10;
		SOF_LINES = 72 + 1;
		TOTAL_FRAMES = 3;
	} else if ((param->VIC == HDMI_4k2k_25) ||
		(param->VIC == HDMI_3840x2160p50_16x9)) {
		INTERLACE_MODE = 0;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (3840*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (2160/(1+INTERLACE_MODE));
		LINES_F0 = 2250;
		LINES_F1 = 2250;
		FRONT_PORCH = 1056;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 8 + 1;
		VSYNC_LINES = 10;
		SOF_LINES = 72 + 1;
		TOTAL_FRAMES = 3;
	} else if (param->VIC == HDMI_4k2k_24) {
		INTERLACE_MODE = 0;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (3840*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (2160/(1+INTERLACE_MODE));
		LINES_F0 = 2250;
		LINES_F1 = 2250;
		FRONT_PORCH = 1276;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 8 + 1;
		VSYNC_LINES = 10;
		SOF_LINES = 72 + 1;
		TOTAL_FRAMES = 3;
	} else if (param->VIC == HDMI_4k2k_smpte_24) {
		INTERLACE_MODE = 0;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS = (4096*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (2160/(1+INTERLACE_MODE));
		LINES_F0 = 2250;
		LINES_F1 = 2250;
		FRONT_PORCH = 1020;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 8 + 1;
		VSYNC_LINES = 10;
		SOF_LINES = 72 + 1;
		TOTAL_FRAMES = 3;
	}

	TOTAL_PIXELS = (FRONT_PORCH+HSYNC_PIXELS+BACK_PORCH+ACTIVE_PIXELS);
	TOTAL_LINES = (LINES_F0+(LINES_F1*INTERLACE_MODE));

	total_pixels_venc = (TOTAL_PIXELS  / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	active_pixels_venc = (ACTIVE_PIXELS / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	front_porch_venc = (FRONT_PORCH   / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	hsync_pixels_venc = (HSYNC_PIXELS  / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);

	de_h_begin = modulo(hd_read_reg(P_ENCP_VIDEO_HAVON_BEGIN) +
		VFIFO2VD_TO_HDMI_LATENCY, total_pixels_venc);
	de_h_end  = modulo(de_h_begin + active_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCP_DE_H_BEGIN, de_h_begin);
	hd_write_reg(P_ENCP_DE_H_END, de_h_end);
	/* Program DE timing for even field */
	de_v_begin_even = hd_read_reg(P_ENCP_VIDEO_VAVON_BLINE);
	de_v_end_even  = modulo(de_v_begin_even + ACTIVE_LINES, TOTAL_LINES);
	hd_write_reg(P_ENCP_DE_V_BEGIN_EVEN, de_v_begin_even);
	hd_write_reg(P_ENCP_DE_V_END_EVEN,  de_v_end_even);
	/* Program DE timing for odd field if needed */
	if (INTERLACE_MODE) {
		de_v_begin_odd = to_signed(
			(hd_read_reg(P_ENCP_VIDEO_OFLD_VOAV_OFST) & 0xf0)>>4)
			+ de_v_begin_even + (TOTAL_LINES-1)/2;
		de_v_end_odd = modulo(de_v_begin_odd + ACTIVE_LINES,
			TOTAL_LINES);
		hd_write_reg(P_ENCP_DE_V_BEGIN_ODD, de_v_begin_odd);
		hd_write_reg(P_ENCP_DE_V_END_ODD, de_v_end_odd);
	}

	/* Program Hsync timing */
	if (de_h_end + front_porch_venc >= total_pixels_venc) {
		hs_begin = de_h_end + front_porch_venc - total_pixels_venc;
		vs_adjust  = 1;
	} else {
		hs_begin = de_h_end + front_porch_venc;
		vs_adjust  = 1;
	}
	hs_end = modulo(hs_begin + hsync_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCP_DVI_HSO_BEGIN,  hs_begin);
	hd_write_reg(P_ENCP_DVI_HSO_END, hs_end);

	/* Program Vsync timing for even field */
	if (de_v_begin_even >= SOF_LINES + VSYNC_LINES + (1-vs_adjust))
		vs_bline_evn = de_v_begin_even - SOF_LINES - VSYNC_LINES
			- (1-vs_adjust);
	else
		vs_bline_evn = TOTAL_LINES + de_v_begin_even - SOF_LINES
			- VSYNC_LINES - (1-vs_adjust);
	vs_eline_evn = modulo(vs_bline_evn + VSYNC_LINES, TOTAL_LINES);
	hd_write_reg(P_ENCP_DVI_VSO_BLINE_EVN, vs_bline_evn);
	hd_write_reg(P_ENCP_DVI_VSO_ELINE_EVN, vs_eline_evn);
	vso_begin_evn = hs_begin;
	hd_write_reg(P_ENCP_DVI_VSO_BEGIN_EVN, vso_begin_evn);
	hd_write_reg(P_ENCP_DVI_VSO_END_EVN, vso_begin_evn);
	/* Program Vsync timing for odd field if needed */
	if (INTERLACE_MODE) {
		vs_bline_odd = de_v_begin_odd-1 - SOF_LINES - VSYNC_LINES;
		vs_eline_odd = de_v_begin_odd-1 - SOF_LINES;
		vso_begin_odd  = modulo(hs_begin + (total_pixels_venc>>1),
			total_pixels_venc);
		hd_write_reg(P_ENCP_DVI_VSO_BLINE_ODD, vs_bline_odd);
		hd_write_reg(P_ENCP_DVI_VSO_ELINE_ODD, vs_eline_odd);
		hd_write_reg(P_ENCP_DVI_VSO_BEGIN_ODD, vso_begin_odd);
		hd_write_reg(P_ENCP_DVI_VSO_END_ODD, vso_begin_odd);
	}
	hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
			(0 << 1) |
			(HSYNC_POLARITY << 2) |
			(VSYNC_POLARITY << 3) |
			(0 << 4) |
			(4 << 5) |
			(0 << 8) |
			(0 << 12)
	);
	hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
	hd_write_reg(P_ENCP_VIDEO_EN, 1);
}

static void hdmi_tvenc480i_set(struct hdmitx_vidpara *param)
{
	unsigned long VFIFO2VD_TO_HDMI_LATENCY = 1;
	unsigned long TOTAL_PIXELS = 0, PIXEL_REPEAT_HDMI = 0,
		PIXEL_REPEAT_VENC = 0, ACTIVE_PIXELS = 0;
	unsigned FRONT_PORCH = 38, HSYNC_PIXELS = 124, ACTIVE_LINES = 0,
		INTERLACE_MODE = 0, TOTAL_LINES = 0, SOF_LINES = 0,
		VSYNC_LINES = 0;
	unsigned LINES_F0 = 262, LINES_F1 = 263, BACK_PORCH = 114,
		EOF_LINES = 2, TOTAL_FRAMES = 0;

	unsigned long total_pixels_venc = 0;
	unsigned long active_pixels_venc = 0;
	unsigned long front_porch_venc = 0;
	unsigned long hsync_pixels_venc = 0;

	unsigned long de_h_begin = 0, de_h_end = 0;
	unsigned long de_v_begin_even = 0, de_v_end_even = 0,
		de_v_begin_odd = 0, de_v_end_odd = 0;
	unsigned long hs_begin = 0, hs_end = 0;
	unsigned long vs_adjust = 0;
	unsigned long vs_bline_evn = 0, vs_eline_evn = 0,
		vs_bline_odd = 0, vs_eline_odd = 0;
	unsigned long vso_begin_evn = 0, vso_begin_odd = 0;

	hd_set_reg_bits(P_HHI_GCLK_OTHER, 1, 8, 1);
	switch (param->VIC) {
	case HDMI_480i60:
	case HDMI_480i60_16x9:
	case HDMI_480i60_16x9_rpt:
		INTERLACE_MODE = 1;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 1;
		ACTIVE_PIXELS	= (720*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (480/(1+INTERLACE_MODE));
		LINES_F0 = 262;
		LINES_F1 = 263;
		FRONT_PORCH = 38;
		HSYNC_PIXELS = 124;
		BACK_PORCH = 114;
		EOF_LINES = 4;
		VSYNC_LINES = 3;
		SOF_LINES = 15;
		TOTAL_FRAMES = 4;
	break;
	case HDMI_576i50:
	case HDMI_576i50_16x9:
	case HDMI_576i50_16x9_rpt:
		INTERLACE_MODE = 1;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 1;
		ACTIVE_PIXELS	= (720*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (576/(1+INTERLACE_MODE));
		LINES_F0 = 312;
		LINES_F1 = 313;
		FRONT_PORCH = 24;
		HSYNC_PIXELS = 126;
		BACK_PORCH = 138;
		EOF_LINES = 2;
		VSYNC_LINES = 3;
		SOF_LINES = 19;
		TOTAL_FRAMES = 4;
		break;
	default:
		break;
	}

	TOTAL_PIXELS = (FRONT_PORCH+HSYNC_PIXELS+BACK_PORCH+ACTIVE_PIXELS);
	TOTAL_LINES = (LINES_F0+(LINES_F1*INTERLACE_MODE));

	total_pixels_venc = (TOTAL_PIXELS  / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC); /* 1716 / 2 * 2 = 1716 */
	active_pixels_venc = (ACTIVE_PIXELS / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	front_porch_venc = (FRONT_PORCH   / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC); /* 38   / 2 * 2 = 38 */
	hsync_pixels_venc = (HSYNC_PIXELS  / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC); /* 124  / 2 * 2 = 124 */

	de_h_begin = modulo(hd_read_reg(P_ENCI_VFIFO2VD_PIXEL_START) +
		VFIFO2VD_TO_HDMI_LATENCY, total_pixels_venc);
	de_h_end  = modulo(de_h_begin + active_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCI_DE_H_BEGIN, de_h_begin);	/* 235 */
	hd_write_reg(P_ENCI_DE_H_END, de_h_end);	 /* 1675 */

	de_v_begin_even = hd_read_reg(P_ENCI_VFIFO2VD_LINE_TOP_START);
	de_v_end_even  = de_v_begin_even + ACTIVE_LINES;
	de_v_begin_odd = hd_read_reg(P_ENCI_VFIFO2VD_LINE_BOT_START);
	de_v_end_odd = de_v_begin_odd + ACTIVE_LINES;
	hd_write_reg(P_ENCI_DE_V_BEGIN_EVEN, de_v_begin_even);
	hd_write_reg(P_ENCI_DE_V_END_EVEN,  de_v_end_even);
	hd_write_reg(P_ENCI_DE_V_BEGIN_ODD, de_v_begin_odd);
	hd_write_reg(P_ENCI_DE_V_END_ODD, de_v_end_odd);

	/* Program Hsync timing */
	if (de_h_end + front_porch_venc >= total_pixels_venc) {
		hs_begin = de_h_end + front_porch_venc - total_pixels_venc;
		vs_adjust  = 1;
	} else {
		hs_begin = de_h_end + front_porch_venc;
		vs_adjust  = 0;
	}
	hs_end = modulo(hs_begin + hsync_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCI_DVI_HSO_BEGIN,  hs_begin);  /* 1713 */
	hd_write_reg(P_ENCI_DVI_HSO_END, hs_end);	/* 121 */

	/* Program Vsync timing for even field */
	if (de_v_end_odd-1 + EOF_LINES + vs_adjust >= LINES_F1) {
		vs_bline_evn = de_v_end_odd-1 + EOF_LINES + vs_adjust
			- LINES_F1;
		vs_eline_evn = vs_bline_evn + VSYNC_LINES;
		hd_write_reg(P_ENCI_DVI_VSO_BLINE_EVN, vs_bline_evn);
		/* vso_bline_evn_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_ELINE_EVN, vs_eline_evn);
		/* vso_eline_evn_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_BEGIN_EVN, hs_begin);
		hd_write_reg(P_ENCI_DVI_VSO_END_EVN, hs_begin);
	} else {
		vs_bline_odd = de_v_end_odd-1 + EOF_LINES + vs_adjust;
		hd_write_reg(P_ENCI_DVI_VSO_BLINE_ODD, vs_bline_odd);
		/* vso_bline_odd_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_BEGIN_ODD, hs_begin);
	if (vs_bline_odd + VSYNC_LINES >= LINES_F1) {
		vs_eline_evn = vs_bline_odd + VSYNC_LINES - LINES_F1;
		hd_write_reg(P_ENCI_DVI_VSO_ELINE_EVN, vs_eline_evn);
		/* vso_eline_evn_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_END_EVN, hs_begin);
	} else {
		vs_eline_odd = vs_bline_odd + VSYNC_LINES;
		hd_write_reg(P_ENCI_DVI_VSO_ELINE_ODD, vs_eline_odd);
		/* vso_eline_odd_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_END_ODD, hs_begin);
	}
	}
	/* Program Vsync timing for odd field */
	if (de_v_end_even-1 + EOF_LINES + 1 >= LINES_F0) {
		vs_bline_odd = de_v_end_even-1 + EOF_LINES + 1 - LINES_F0;
		vs_eline_odd = vs_bline_odd + VSYNC_LINES;
		hd_write_reg(P_ENCI_DVI_VSO_BLINE_ODD, vs_bline_odd);
		/* vso_bline_odd_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_ELINE_ODD, vs_eline_odd);
		/* vso_eline_odd_reg_wr_cnt ++; */
		vso_begin_odd  = modulo(hs_begin + (total_pixels_venc>>1),
			total_pixels_venc);
		hd_write_reg(P_ENCI_DVI_VSO_BEGIN_ODD, vso_begin_odd);
		hd_write_reg(P_ENCI_DVI_VSO_END_ODD, vso_begin_odd);
	} else {
		vs_bline_evn = de_v_end_even-1 + EOF_LINES + 1;
		hd_write_reg(P_ENCI_DVI_VSO_BLINE_EVN, vs_bline_evn); /* 261 */
		/* vso_bline_evn_reg_wr_cnt ++; */
		vso_begin_evn  = modulo(hs_begin + (total_pixels_venc>>1),
			total_pixels_venc);
		hd_write_reg(P_ENCI_DVI_VSO_BEGIN_EVN, vso_begin_evn);
	if (vs_bline_evn + VSYNC_LINES >= LINES_F0) {
		vs_eline_odd = vs_bline_evn + VSYNC_LINES - LINES_F0;
		hd_write_reg(P_ENCI_DVI_VSO_ELINE_ODD, vs_eline_odd);
		/* vso_eline_odd_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_END_ODD, vso_begin_evn);
	} else {
		vs_eline_evn = vs_bline_evn + VSYNC_LINES;
		hd_write_reg(P_ENCI_DVI_VSO_ELINE_EVN, vs_eline_evn);
		/* vso_eline_evn_reg_wr_cnt ++; */
		hd_write_reg(P_ENCI_DVI_VSO_END_EVN, vso_begin_evn);
	}
	}

	hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
			(0 << 1) |
			(0 << 2) |
			(0 << 3) |
			(0 << 4) |
			(4 << 5) |
			(1 << 8) |
			(1 << 12)
	);
	if ((param->VIC == HDMI_480i60_16x9_rpt) ||
		(param->VIC == HDMI_576i50_16x9_rpt))
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 3, 12, 4);
	hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 0, 1);
}

static void hdmi_tvenc_set(struct hdmitx_vidpara *param)
{
	unsigned long VFIFO2VD_TO_HDMI_LATENCY = 2;
	unsigned long TOTAL_PIXELS = 0, PIXEL_REPEAT_HDMI = 0,
		PIXEL_REPEAT_VENC = 0, ACTIVE_PIXELS = 0;
	unsigned FRONT_PORCH = 0, HSYNC_PIXELS = 0, ACTIVE_LINES = 0,
		INTERLACE_MODE = 0, TOTAL_LINES = 0, SOF_LINES = 0,
		VSYNC_LINES = 0;
	unsigned LINES_F0 = 0, LINES_F1 = 0, BACK_PORCH = 0,
		EOF_LINES = 0, TOTAL_FRAMES = 0;

	unsigned long total_pixels_venc = 0;
	unsigned long active_pixels_venc = 0;
	unsigned long front_porch_venc = 0;
	unsigned long hsync_pixels_venc = 0;

	unsigned long de_h_begin = 0, de_h_end = 0;
	unsigned long de_v_begin_even = 0, de_v_end_even = 0,
		de_v_begin_odd = 0, de_v_end_odd = 0;
	unsigned long hs_begin = 0, hs_end = 0;
	unsigned long vs_adjust = 0;
	unsigned long vs_bline_evn = 0, vs_eline_evn = 0,
		vs_bline_odd = 0, vs_eline_odd = 0;
	unsigned long vso_begin_evn = 0, vso_begin_odd = 0;

	switch (param->VIC) {
	case HDMI_3840x1080p120hz:
		INTERLACE_MODE = 0;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= 3840;
		ACTIVE_LINES = 1080;
		LINES_F0 = 1125;
		LINES_F1 = 1125;
		FRONT_PORCH = 176;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 4;
		VSYNC_LINES = 5;
		SOF_LINES = 36;
		TOTAL_FRAMES = 0;
		break;
	case HDMI_3840x1080p100hz:
		INTERLACE_MODE = 0;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= 3840;
		ACTIVE_LINES = 1080;
		LINES_F0 = 1125;
		LINES_F1 = 1125;
		FRONT_PORCH = 1056;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 4;
		VSYNC_LINES = 5;
		SOF_LINES = 36;
		TOTAL_FRAMES = 0;
		break;
	case HDMI_3840x540p240hz:
		INTERLACE_MODE = 0;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= 3840;
		ACTIVE_LINES = 1080;
		LINES_F0 = 562;
		LINES_F1 = 562;
		FRONT_PORCH = 176;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 2;
		VSYNC_LINES = 2;
		SOF_LINES = 18;
		TOTAL_FRAMES = 0;
		break;
	case HDMI_3840x540p200hz:
		INTERLACE_MODE = 0;
		PIXEL_REPEAT_VENC = 0;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= 3840;
		ACTIVE_LINES = 1080;
		LINES_F0 = 562;
		LINES_F1 = 562;
		FRONT_PORCH = 1056;
		HSYNC_PIXELS = 88;
		BACK_PORCH = 296;
		EOF_LINES = 2;
		VSYNC_LINES = 2;
		SOF_LINES = 18;
		TOTAL_FRAMES = 0;
		break;
	case HDMI_480p60:
	case HDMI_480p60_16x9:
	case HDMI_480p60_16x9_rpt:
		INTERLACE_MODE = 0;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= (720*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (480/(1+INTERLACE_MODE));
		LINES_F0 = 525;
		LINES_F1 = 525;
		FRONT_PORCH = 16;
		HSYNC_PIXELS = 62;
		BACK_PORCH = 60;
		EOF_LINES = 9;
		VSYNC_LINES = 6;
		SOF_LINES = 30;
		TOTAL_FRAMES = 4;
		break;
	case HDMI_576p50:
	case HDMI_576p50_16x9:
	case HDMI_576p50_16x9_rpt:
		INTERLACE_MODE = 0;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= (720*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (576/(1+INTERLACE_MODE));
		LINES_F0 = 625;
		LINES_F1 = 625;
		FRONT_PORCH = 12;
		HSYNC_PIXELS = 64;
		BACK_PORCH = 68;
		EOF_LINES = 5;
		VSYNC_LINES = 5;
		SOF_LINES = 39;
		TOTAL_FRAMES = 4;
		break;
	case HDMI_720p60:
		INTERLACE_MODE = 0;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= (1280*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (720/(1+INTERLACE_MODE));
		LINES_F0 = 750;
		LINES_F1 = 750;
		FRONT_PORCH = 110;
		HSYNC_PIXELS = 40;
		BACK_PORCH = 220;
		EOF_LINES = 5;
		VSYNC_LINES = 5;
		SOF_LINES = 20;
		TOTAL_FRAMES = 4;
		break;
	case HDMI_720p50:
		INTERLACE_MODE = 0;
		PIXEL_REPEAT_VENC = 1;
		PIXEL_REPEAT_HDMI = 0;
		ACTIVE_PIXELS	= (1280*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (720/(1+INTERLACE_MODE));
		LINES_F0 = 750;
		LINES_F1 = 750;
		FRONT_PORCH = 440;
		HSYNC_PIXELS = 40;
		BACK_PORCH = 220;
		EOF_LINES = 5;
		VSYNC_LINES = 5;
		SOF_LINES = 20;
		TOTAL_FRAMES = 4;
		break;
	case HDMI_1080p50:
		INTERLACE_MODE	= 0;
		PIXEL_REPEAT_VENC  = 0;
		PIXEL_REPEAT_HDMI  = 0;
		ACTIVE_PIXELS = (1920*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (1080/(1+INTERLACE_MODE));
		LINES_F0 = 1125;
		LINES_F1 = 1125;
		FRONT_PORCH = 528;
		HSYNC_PIXELS = 44;
		BACK_PORCH = 148;
		EOF_LINES = 4;
		VSYNC_LINES = 5;
		SOF_LINES = 36;
		TOTAL_FRAMES = 4;
		break;
	case HDMI_1080p24:
		INTERLACE_MODE	= 0;
		PIXEL_REPEAT_VENC  = 0;
		PIXEL_REPEAT_HDMI  = 0;
		ACTIVE_PIXELS = (1920*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (1080/(1+INTERLACE_MODE));
		LINES_F0 = 1125;
		LINES_F1 = 1125;
		FRONT_PORCH = 638;
		HSYNC_PIXELS = 44;
		BACK_PORCH = 148;
		EOF_LINES = 4;
		VSYNC_LINES = 5;
		SOF_LINES = 36;
		TOTAL_FRAMES = 4;
		break;
	case HDMI_1080p60:
	case HDMI_1080p30:
		INTERLACE_MODE	= 0;
		PIXEL_REPEAT_VENC  = 0;
		PIXEL_REPEAT_HDMI  = 0;
		ACTIVE_PIXELS = (1920*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (1080/(1+INTERLACE_MODE));
		LINES_F0 = 1125;
		LINES_F1 = 1125;
		FRONT_PORCH = 88;
		HSYNC_PIXELS = 44;
		BACK_PORCH = 148;
		EOF_LINES = 4;
		VSYNC_LINES = 5;
		SOF_LINES = 36;
		TOTAL_FRAMES = 4;
		break;
	default:
		break;
	}

	TOTAL_PIXELS = (FRONT_PORCH+HSYNC_PIXELS+BACK_PORCH+ACTIVE_PIXELS);
	TOTAL_LINES = (LINES_F0+(LINES_F1*INTERLACE_MODE));

	total_pixels_venc = (TOTAL_PIXELS  / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	active_pixels_venc = (ACTIVE_PIXELS / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	front_porch_venc = (FRONT_PORCH   / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	hsync_pixels_venc = (HSYNC_PIXELS  / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);

	hd_write_reg(P_ENCP_VIDEO_MODE, hd_read_reg(P_ENCP_VIDEO_MODE)|(1<<14));
	/* Program DE timing */
	de_h_begin = modulo(hd_read_reg(P_ENCP_VIDEO_HAVON_BEGIN) +
		VFIFO2VD_TO_HDMI_LATENCY,  total_pixels_venc);
	de_h_end  = modulo(de_h_begin + active_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCP_DE_H_BEGIN, de_h_begin);	/* 220 */
	hd_write_reg(P_ENCP_DE_H_END, de_h_end);	 /* 1660 */
	/* Program DE timing for even field */
	de_v_begin_even = hd_read_reg(P_ENCP_VIDEO_VAVON_BLINE);
	de_v_end_even  = de_v_begin_even + ACTIVE_LINES;
	hd_write_reg(P_ENCP_DE_V_BEGIN_EVEN, de_v_begin_even);
	hd_write_reg(P_ENCP_DE_V_END_EVEN,  de_v_end_even);	/* 522 */
	/* Program DE timing for odd field if needed */
	if (INTERLACE_MODE) {
		de_v_begin_odd = to_signed(
			(hd_read_reg(P_ENCP_VIDEO_OFLD_VOAV_OFST)
			& 0xf0)>>4) + de_v_begin_even + (TOTAL_LINES-1)/2;
		de_v_end_odd = de_v_begin_odd + ACTIVE_LINES;
		hd_write_reg(P_ENCP_DE_V_BEGIN_ODD, de_v_begin_odd);
		hd_write_reg(P_ENCP_DE_V_END_ODD, de_v_end_odd);
	}

	/* Program Hsync timing */
	if (de_h_end + front_porch_venc >= total_pixels_venc) {
		hs_begin = de_h_end + front_porch_venc - total_pixels_venc;
		vs_adjust  = 1;
	} else {
		hs_begin = de_h_end + front_porch_venc;
		vs_adjust  = 0;
	}
	hs_end = modulo(hs_begin + hsync_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCP_DVI_HSO_BEGIN,  hs_begin);
	hd_write_reg(P_ENCP_DVI_HSO_END, hs_end);

	/* Program Vsync timing for even field */
	if (de_v_begin_even >= SOF_LINES + VSYNC_LINES + (1-vs_adjust))
		vs_bline_evn = de_v_begin_even - SOF_LINES - VSYNC_LINES -
			(1-vs_adjust);
	else
		vs_bline_evn = TOTAL_LINES + de_v_begin_even - SOF_LINES -
			VSYNC_LINES - (1-vs_adjust);
	vs_eline_evn = modulo(vs_bline_evn + VSYNC_LINES, TOTAL_LINES);
	hd_write_reg(P_ENCP_DVI_VSO_BLINE_EVN, vs_bline_evn);   /* 5 */
	hd_write_reg(P_ENCP_DVI_VSO_ELINE_EVN, vs_eline_evn);   /* 11 */
	vso_begin_evn = hs_begin; /* 1692 */
	hd_write_reg(P_ENCP_DVI_VSO_BEGIN_EVN, vso_begin_evn);  /* 1692 */
	hd_write_reg(P_ENCP_DVI_VSO_END_EVN, vso_begin_evn);  /* 1692 */
	/* Program Vsync timing for odd field if needed */
	if (INTERLACE_MODE) {
		vs_bline_odd = de_v_begin_odd-1 - SOF_LINES - VSYNC_LINES;
		vs_eline_odd = de_v_begin_odd-1 - SOF_LINES;
		vso_begin_odd  = modulo(hs_begin + (total_pixels_venc>>1),
			total_pixels_venc);
		hd_write_reg(P_ENCP_DVI_VSO_BLINE_ODD, vs_bline_odd);
		hd_write_reg(P_ENCP_DVI_VSO_ELINE_ODD, vs_eline_odd);
		hd_write_reg(P_ENCP_DVI_VSO_BEGIN_ODD, vso_begin_odd);
		hd_write_reg(P_ENCP_DVI_VSO_END_ODD, vso_begin_odd);
	}
	if ((param->VIC == HDMI_3840x540p240hz) ||
		(param->VIC == HDMI_3840x540p200hz))
		hd_write_reg(P_ENCP_DE_V_END_EVEN, 0x230);
	switch (param->VIC) {
	case HDMI_3840x1080p120hz:
	case HDMI_3840x1080p100hz:
	case HDMI_3840x540p240hz:
	case HDMI_3840x540p200hz:
		hd_write_reg(P_VPU_HDMI_SETTING, 0x8e);
		break;
	case HDMI_480i60:
	case HDMI_480i60_16x9:
	case HDMI_576i50:
	case HDMI_576i50_16x9:
	case HDMI_480i60_16x9_rpt:
	case HDMI_576i50_16x9_rpt:
		hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
				(0 << 1) |
				(0 << 2) |
				(0 << 3) |
				(0 << 4) |
				(4 << 5) |
				(1 << 8) |
				(1 << 12)
		);
		if ((param->VIC == HDMI_480i60_16x9_rpt) ||
			(param->VIC == HDMI_576i50_16x9_rpt))
			hd_set_reg_bits(P_VPU_HDMI_SETTING, 3, 12, 4);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 0, 1);
		break;
	case HDMI_1080i60:
	case HDMI_1080i50:
		hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
				(0 << 1) |
				(HSYNC_POLARITY << 2) |
				(VSYNC_POLARITY << 3) |
				(0 << 4) |
				(((TX_INPUT_COLOR_FORMAT == 0)?1:0)  << 5) |
				(1 << 8) |
				(0 << 12)
		);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
		break;
	case HDMI_4k2k_30:
	case HDMI_4k2k_25:
	case HDMI_4k2k_24:
	case HDMI_4k2k_smpte_24:
	case HDMI_3840x2160p50_16x9:
	case HDMI_3840x2160p60_16x9:
		hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
			(0 << 1) |
			(HSYNC_POLARITY << 2) |
			(VSYNC_POLARITY << 3) |
			(0 << 4) |
			(4 << 5) |
			(0 << 8) |
			(0 << 12)
		);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
		hd_write_reg(P_ENCP_VIDEO_EN, 1); /* Enable VENC */
		break;
	case HDMI_480p60_16x9_rpt:
	case HDMI_576p50_16x9_rpt:
	case HDMI_480p60:
	case HDMI_480p60_16x9:
	case HDMI_576p50:
	case HDMI_576p50_16x9:
		hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
				(0 << 1) |
				(0 << 2) |
				(0 << 3) |
				(0 << 4) |
				(4 << 5) |
				(1 << 8) |
				(0 << 12)
		);
		if ((param->VIC == HDMI_480p60_16x9_rpt) ||
			(param->VIC == HDMI_576p50_16x9_rpt))
			hd_set_reg_bits(P_VPU_HDMI_SETTING, 3, 12, 4);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
		break;
	case HDMI_720p60:
	case HDMI_720p50:
		hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
				(0 << 1) |
				(HSYNC_POLARITY << 2) |
				(VSYNC_POLARITY << 3) |
				(0 << 4) |
				(4 << 5) |
				(1 << 8) |
				(0 << 12)
		);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
		break;
	default:
		hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
				(0 << 1) | /* [	1] src_sel_encp */
				(HSYNC_POLARITY << 2) |
				(VSYNC_POLARITY << 3) |
				(0 << 4) |
				(4 << 5) |
				(0 << 8) |
				(0 << 12)
		);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
	}
	if ((param->VIC == HDMI_480p60_16x9_rpt) ||
		(param->VIC == HDMI_576p50_16x9_rpt))
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 3, 12, 4);
	hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
}

static void digital_clk_off(unsigned char flag)
{
	/* TODO */
}

static void digital_clk_on(unsigned char flag)
{
/* clk81_set(); */
	if (flag&4) {
		hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 0, 0, 7);
		hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 0, 9, 3);
		hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 1, 8, 1);
	}
	if (flag&2) {
		/* on hdmi pixel clock */
		hd_write_reg(P_HHI_GCLK_MPEG2,
			hd_read_reg(P_HHI_GCLK_MPEG2) | (1<<4));
		hd_write_reg(P_HHI_GCLK_OTHER,
			hd_read_reg(P_HHI_GCLK_OTHER)|(1<<17));
	}
}

static void phy_pll_off(void)
{
	hdmi_phy_suspend();
}

/**/
void hdmi_hw_set_powermode(struct hdmitx_dev *hdev)
{
	int vic = hdev->cur_VIC;

	switch (vic) {
	case HDMI_480i60:
	case HDMI_480i60_16x9:
	case HDMI_576p50:
	case HDMI_576p50_16x9:
	case HDMI_576i50:
	case HDMI_576i50_16x9:
	case HDMI_480p60:
	case HDMI_480p60_16x9:
	case HDMI_720p50:
	case HDMI_720p60:
	case HDMI_1080i50:
	case HDMI_1080i60:
	case HDMI_1080p24:/* 1080p24 support */
	case HDMI_1080p50:
	case HDMI_1080p60:
	default:
	/* hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x08c38d0b); */
	break;
	}
	/* hd_write_reg(P_HHI_HDMI_PHY_CNTL1, 2); */
}

#if 0
/* When have below format output, we shall manually configure */
/* bolow register to get stable Video Timing. */
static void hdmi_reconfig_packet_setting(enum hdmi_vic vic)
{
	/* TODO */
}
#endif

static void hdmi_hw_reset(struct hdmitx_dev *hdev, struct hdmitx_vidpara *param)
{
	/* reset REG init */
	/* TODO */
}

static void hdmi_audio_init(unsigned int spdif_flag)
{
	return;
	/* TODO */
}

static void enable_audio_spdif(void)
{
	hdmi_print(INF, AUD "Enable audio spdif to HDMI\n");

	/* TODO */
}

static void enable_audio_i2s(void)
{
	hdmi_print(INF, AUD "Enable audio i2s to HDMI\n");
	/* TODO */
}

/************************************
*	hdmitx hardware level interface
*************************************/

static void hdmitx_dump_tvenc_reg(int cur_VIC, int pr_info_flag)
{
}

static void hdmitx_config_tvenc_reg(int vic, unsigned reg, unsigned val)
{
}

static void hdmitx_set_pll(struct hdmitx_dev *hdev,
	struct hdmitx_vidpara *param)
{
	hdmi_print(IMP, SYS "set pll\n");
	hdmi_print(IMP, SYS "param->VIC:%d\n", param->VIC);

	cur_vout_index = get_cur_vout_index();
/* TODO
#ifdef CONFIG_AML_VOUT_FRAMERATE_AUTOMATION
	if (hdmitx_set_pll_fr_auto(hdev))
		return;
#endif
*/
	set_vmode_clk(hdev, param->VIC);
	pr_info("TODO: 4k60hz/420\n");
}

static void hdmitx_set_phy(struct hdmitx_dev *hdev)
{
	if (!hdev)
		return;

	switch (hdev->cur_VIC) {
	case HDMI_1080p60:
	case HDMI_4k2k_24:
	case HDMI_4k2k_25:
	case HDMI_4k2k_30:
	case HDMI_4k2k_smpte_24:
	default:
		hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x08c31ebb);
		break;
	}
#if 1
/* P_HHI_HDMI_PHY_CNTL1	bit[1]: enable clock	bit[0]: soft reset */
#define RESET_HDMI_PHY() \
do { \
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0xf, 0, 4); \
	mdelay(2); \
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0xe, 0, 4); \
	mdelay(2); \
} while (0)

	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0x0390, 16, 16);
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0x1, 17, 1);
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0x0, 0, 4);
	RESET_HDMI_PHY();
	RESET_HDMI_PHY();
	RESET_HDMI_PHY();
#undef RESET_HDMI_PHY
#endif
	hdmi_print(IMP, SYS "phy setting done\n");
}

static void set_tmds_clk_div40(unsigned int div40)
{
	if (div40 == 1) {
		hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_01, 0);
		hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_23, 0x03ff03ff);
	} else {
		hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_01, 0x001f001f);
		hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_23, 0x001f001f);
	}

	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_CNTL, 0x1);
	msleep(20);
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_CNTL, 0x2);
}

static int hdmitx_set_dispmode(struct hdmitx_dev *hdev,
	struct hdmitx_vidpara *param)
{
	if (param == NULL) /* disable HDMI */
		return 0;
	else
		if (!hdmitx_edid_VIC_support(param->VIC))
			return -1;

	if (color_depth_f == 24)
		param->color_depth = hdmi_color_depth_24B;
	else if (color_depth_f == 30)
		param->color_depth = hdmi_color_depth_30B;
	else if (color_depth_f == 36)
		param->color_depth = hdmi_color_depth_36B;
	else if (color_depth_f == 48)
		param->color_depth = hdmi_color_depth_48B;
	hdmi_print(INF, SYS "set mode VIC %d (cd%d,cs%d,pm%d,vd%d,%x)\n",
		param->VIC, color_depth_f, color_space_f, power_mode,
		power_off_vdac_flag, serial_reg_val);
	if (color_space_f != 0)
		param->color = color_space_f;
	/* set_AVMUTE to 1 */
	hdmitx_set_reg_bits(HDMITX_DWC_FC_GCP, 1, 1, 1);
	/* clear_AVMUTE to 0 */
	hdmitx_set_reg_bits(HDMITX_DWC_FC_GCP, 0, 0, 1);
	msleep(50);
	hdmitx_set_pll(hdev, param);
	hdmitx_set_phy(hdev);
	switch (param->VIC) {
	case HDMI_480i60:
	case HDMI_480i60_16x9:
	case HDMI_576i50:
	case HDMI_576i50_16x9:
	case HDMI_480i60_16x9_rpt:
	case HDMI_576i50_16x9_rpt:
		hdmi_tvenc480i_set(param);
		break;
	case HDMI_1080i60:
	case HDMI_1080i50:
		hdmi_tvenc1080i_set(param);
		break;
	case HDMI_4k2k_30:
	case HDMI_4k2k_25:
	case HDMI_4k2k_24:
	case HDMI_4k2k_smpte_24:
	case HDMI_3840x2160p50_16x9:
	case HDMI_3840x2160p60_16x9:
		hdmi_tvenc4k2k_set(param);
		break;
	default:
		hdmi_tvenc_set(param);
	}
/* [ 3: 2] chroma_dnsmp. 0=use pixel 0; 1=use pixel 1; 2=use average. */
/* [	5] hdmi_dith_md: random noise selector. */
	hd_write_reg(P_VPU_HDMI_FMT_CTRL, (((TX_INPUT_COLOR_FORMAT ==
		hdmi_color_format_420)?2:0)  << 0) | (2 << 2) |
			(((1) ? 1:0) << 4) |
			(0  << 5) |
			(0 << 6)); /* [ 9: 6] hdmi_dith10_cntl. */
	if (hdev->mode420 == 1) {
		hd_set_reg_bits(P_VPU_HDMI_FMT_CTRL, 2, 0, 2);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 0, 4, 4);
		hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 8, 1);
	}
	switch (param->VIC) {
	case HDMI_480i60:
	case HDMI_480i60_16x9:
	case HDMI_576i50:
	case HDMI_576i50_16x9:
	case HDMI_480i60_16x9_rpt:
	case HDMI_576i50_16x9_rpt:
		enc_vpu_bridge_reset(0);
		break;
	default:
		enc_vpu_bridge_reset(1);
		break;
	}
	C_Entry(param->VIC);

	hdmi_hw_reset(hdev, param);
	/* move hdmitx_set_pll() to the end of this function. */
	/* hdmitx_set_pll(param); */
	hdev->cur_VIC = param->VIC;
	hdmitx_set_phy(hdev);

	if (hdev->mode420 == 1) {
		/* change AVI packet */
		hdmitx_wr_reg(HDMITX_DWC_FC_AVICONF0, 0x43);
		mode420_half_horizontal_para();
	} else
		/* change AVI packet */
		hdmitx_wr_reg(HDMITX_DWC_FC_AVICONF0, 0x42);
	if (((hdev->cur_VIC == HDMI_3840x2160p50_16x9) ||
		(hdev->cur_VIC == HDMI_3840x2160p60_16x9))
		&& (hdev->mode420 != 1))
		set_tmds_clk_div40(1);
	else
		set_tmds_clk_div40(0);
	hdmitx_set_reg_bits(HDMITX_DWC_FC_INVIDCONF, 0, 3, 1);
	mdelay(1);
	hdmitx_set_reg_bits(HDMITX_DWC_FC_INVIDCONF, 1, 3, 1);
	/* set_AVMUTE to 0 */
	hdmitx_set_reg_bits(HDMITX_DWC_FC_GCP, 0, 1, 1);
	/* clear_AVMUTE to 1 */
	hdmitx_set_reg_bits(HDMITX_DWC_FC_GCP, 1, 0, 1);

	return 0;
}

static void hdmitx_set_packet(int type, unsigned char *DB, unsigned char *HB)
{
	/* TODO */
	/* AVI frame */
	int i;
	unsigned char ucData;
	/* unsigned int pkt_reg_base= 0x0;	//TODO */
	int pkt_data_len = 0;

	switch (type) {
	case HDMI_PACKET_AVI:
		pkt_data_len = 13;
		break;
	case HDMI_PACKET_VEND:
		pkt_data_len = 6;
		break;
	case HDMI_AUDIO_INFO:
		pkt_data_len = 9;
		break;
	case HDMI_SOURCE_DESCRIPTION:
		pkt_data_len = 25;
	default:
		break;
	}

	if (DB) {
		for (i = 0, ucData = 0; i < pkt_data_len ; i++)
			ucData -= DB[i];
		for (i = 0; i < 3; i++)
			ucData -= HB[i];
	}

}


static void hdmitx_setaudioinfoframe(unsigned char *AUD_DB,
	unsigned char *CHAN_STAT_BUF)
{
	int i;
	unsigned char AUD_HB[3] = {0x84, 0x1, 0xa};
	hdmitx_set_packet(HDMI_AUDIO_INFO, AUD_DB, AUD_HB);
	/* channel status */
	if (CHAN_STAT_BUF) {
		for (i = 0; i < 24; i++)
			;
	}
}


/* set_hdmi_audio_source(unsigned int src) */
/* Description: */
/* Select HDMI audio clock source, and I2S input data source. */
/* Parameters: */
/* src -- 0=no audio clock to HDMI; 1=pcmout to HDMI; 2=Aiu I2S out to HDMI. */
static void set_hdmi_audio_source(unsigned int src)
{
	unsigned long data32;
	unsigned int i;

	/* Disable HDMI audio clock input and its I2S input */
/* [1:0] hdmi_clk_sel: 00=Disable hdmi audio clock input; 01=Select pcm clock;
 *     10=Select AIU aoclk; 11=Not allowed. */
/* [5:4] hdmi_data_sel: 00=disable hdmi i2s input; 01=Select pcm data;
 *     10=Select AIU I2S data; 11=Not allowed. */
	data32 = 0;
	data32 |= (0 << 4);
	data32 |= (0 << 0);
	hd_write_reg(P_AIU_HDMI_CLK_DATA_CTRL, data32);

	/* Enable HDMI audio clock from the selected source */
/* [1:0] hdmi_clk_sel: 00=Disable hdmi audio clock input; 01=Select pcm clock;
 *     10=Select AIU aoclk; 11=Not allowed. */
/* [5:4] hdmi_data_sel: 00=disable hdmi i2s input; 01=Select pcm data;
 *      10=Select AIU I2S data; 11=Not allowed. */
	data32 = 0;
	data32 |= (0 << 4);
	data32 |= (src  << 0);
	hd_write_reg(P_AIU_HDMI_CLK_DATA_CTRL, data32);

	/* Wait until clock change is settled */
	i = 0;
#if 0
	msleep_interruptible(1000);
#endif
	data32 = hd_read_reg(P_AIU_HDMI_CLK_DATA_CTRL);
	if (((data32 >> 8) & 0x3) != src)
		pr_info("audio clock wait time out\n");

	/* Enable HDMI I2S input from the selected source */
/* [1:0] hdmi_clk_sel: 00=Disable hdmi audio clock input; 01=Select pcm clock;
 *     10=Select AIU aoclk; 11=Not allowed. */
/* [5:4] hdmi_data_sel: 00=disable hdmi i2s input; 01=Select pcm data;
 *     10=Select AIU I2S data; 11=Not allowed. */
	data32 = 0;
	data32 |= (src  << 4);
	data32 |= (src  << 0);
	hd_write_reg(P_AIU_HDMI_CLK_DATA_CTRL, data32);

	/* Wait until data change is settled */
#if 0
	msleep_interruptible(1000);
#endif
	data32 = hd_read_reg(P_AIU_HDMI_CLK_DATA_CTRL);
	if (((data32 >> 12) & 0x3) != src)
		pr_info("audio data wait time out\n");
} /* set_hdmi_audio_source */

static void hdmitx_set_aud_pkt_type(enum hdmi_audio_type type)
{
	/* TX_AUDIO_CONTROL [5:4] */
	/* 0: Audio sample packet (HB0 = 0x02) */
	/* 1: One bit audio packet (HB0 = 0x07) */
	/* 2: HBR Audio packet (HB0 = 0x09) */
	/* 3: DST Audio packet (HB0 = 0x08) */
	switch (type) {
	case CT_MAT:
	break;
	case CT_ONE_BIT_AUDIO:
	break;
	case CT_DST:
	break;
	default:
	break;
	}
}

#if 0
static Cts_conf_tab cts_table_192k[] = {
	{24576,  27000,  27000},
	{24576,  54000,  54000},
	{24576, 108000, 108000},
	{24576,  74250,  74250},
	{24576, 148500, 148500},
	{24576, 297000, 297000},
};

static unsigned int get_cts(unsigned int clk)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cts_table_192k); i++) {
		if (clk == cts_table_192k[i].tmds_clk)
			return cts_table_192k[i].fixed_cts;
	}

	return 0;
}

static Vic_attr_map vic_attr_map_table[] = {
	{HDMI_640x480p60,  27000 },
	{HDMI_480p60,  27000 },
	{HDMI_480p60_16x9, 27000 },
	{HDMI_720p60,  74250 },
	{HDMI_1080i60, 74250 },
	{HDMI_480i60,  27000 },
	{HDMI_480i60_16x9, 27000 },
	{HDMI_480i60_16x9_rpt,  54000 },
	{HDMI_1440x480p60, 27000 },
	{HDMI_1440x480p60_16x9, 27000 },
	{HDMI_1080p60, 148500},
	{HDMI_576p50,  27000 },
	{HDMI_576p50_16x9, 27000 },
	{HDMI_720p50,  74250 },
	{HDMI_1080i50, 74250 },
	{HDMI_576i50,  27000 },
	{HDMI_576i50_16x9, 27000 },
	{HDMI_576i50_16x9_rpt,  54000 },
	{HDMI_1080p50, 148500},
	{HDMI_1080p24, 74250 },
	{HDMI_1080p25, 74250 },
	{HDMI_1080p30, 74250 },
	{HDMI_480p60_16x9_rpt,  108000},
	{HDMI_576p50_16x9_rpt,  108000},
	{HDMI_4k2k_24, 247500},
	{HDMI_4k2k_25, 247500},
	{HDMI_4k2k_30, 247500},
	{HDMI_4k2k_smpte_24, 247500},
};

static unsigned int vic_map_clk(enum hdmi_vic vic)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vic_attr_map_table); i++) {
		if (vic == vic_attr_map_table[i].VIC)
			return vic_attr_map_table[i].tmds_clk;
	}

	return 0;
}
#endif

#if 0
static void hdmitx_set_aud_cts(enum hdmi_audio_type type,
	Hdmi_tx_audio_cts_t cts_mode, enum hdmi_vic vic)
{
	unsigned int cts_val = 0;

	switch (type) {
	case CT_MAT:
		if (cts_mode == AUD_CTS_FIXED) {
			unsigned int clk = vic_map_clk(vic);
			if (clk) {
				cts_val = get_cts(clk);
				if (!cts_val)
					hdmi_print(ERR, AUD "not find cts\n");
		} else
				hdmi_print(ERR, AUD "not find tmds clk\n");
		}
		if (cts_mode == AUD_CTS_CALC)
			;/* TODO */
		break;
	default:
		break;
	}

	if (cts_mode == AUD_CTS_FIXED)
		hdmi_print(IMP, AUD "type: %d CTS Mode: %d  VIC: %d  CTS: %d\n",
			type, cts_mode, vic, cts_val);
}
#endif

static void hdmitx_set_aud_chnls(void)
{
	int i;
	pr_info("set default 48k 2ch pcm channel status\n");
	for (i = 0; i < 9; i++)
		/* set all status to 0 */
		hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS0+i, 0x00);
	/* set default 48k 2ch pcm */
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS2, 0x02);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS3, 0x01);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS5, 0x02);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS7, 0x02);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS8, 0xd2);
}

static int hdmitx_set_audmode(struct hdmitx_dev *hdev,
	struct hdmitx_audpara *audio_param)
{
	unsigned int data32;
	unsigned int aud_n_para = 6144;

	pr_info("test hdmi audio\n");
	if (TX_I2S_SPDIF)
		set_hdmi_audio_source(2);
	else
		set_hdmi_audio_source(1);

/* config IP */
/* Configure audio */
	/* I2S Sampler config */
	data32 = 0;
/* [  3] fifo_empty_mask: 0=enable int; 1=mask int. */
	data32 |= (1 << 3);
/* [  2] fifo_full_mask: 0=enable int; 1=mask int. */
	data32 |= (1 << 2);
	hdmitx_wr_reg(HDMITX_DWC_AUD_INT, data32);

	data32 = 0;
/* [  4] fifo_overrun_mask: 0=enable int; 1=mask int.
 * Enable it later when audio starts. */
	data32 |= (1 << 4);
	hdmitx_wr_reg(HDMITX_DWC_AUD_INT1,  data32);
/* [  5] 0=select SPDIF; 1=select I2S. */
	data32 = 0;
	data32 |= (0 << 7);  /* [  7] sw_audio_fifo_rst */
	data32 |= (TX_I2S_SPDIF << 5);
	data32 |= (0 << 0);  /* [3:0] i2s_in_en: enable it later in test.c */
/* if enable it now, fifo_overrun will happen, because packet don't get sent
 * out until initial DE detected. */
	hdmitx_wr_reg(HDMITX_DWC_AUD_CONF0, data32);

	data32 = 0;
	data32 |= (0 << 5);  /* [7:5] i2s_mode: 0=standard I2S mode */
	data32 |= (24 << 0);  /* [4:0] i2s_width */
	hdmitx_wr_reg(HDMITX_DWC_AUD_CONF1, data32);

	data32 = 0;
	data32 |= (0 << 1);  /* [  1] NLPCM */
	data32 |= (0 << 0);  /* [  0] HBR */
	hdmitx_wr_reg(HDMITX_DWC_AUD_CONF2, data32);

	/* spdif sampler config */
/* [  2] SPDIF fifo_full_mask: 0=enable int; 1=mask int. */
/* [  3] SPDIF fifo_empty_mask: 0=enable int; 1=mask int. */
	data32 = 0;
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIFINT,  data32);
	/* [  4] SPDIF fifo_overrun_mask: 0=enable int; 1=mask int. */
	data32 = 0;
	data32 |= (0 << 4);
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIFINT1, data32);

	data32 = 0;
	data32 |= (0 << 7);  /* [  7] sw_audio_fifo_rst */
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIF0, data32);

	data32 = 0;
	data32 |= (0 << 7);  /* [  7] setnlpcm */
	data32 |= (0 << 6);  /* [  6] spdif_hbr_mode */
	data32 |= (24 << 0);  /* [4:0] spdif_width */
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIF1, data32);

	/* ACR packet configuration */

	data32 = 0;
	data32 |= (1 << 7);  /* [  7] ncts_atomic_write */
	data32 |= (0 << 0);  /* [3:0] AudN[19:16] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_N3, data32);

	data32 = 0;
	data32 |= (0 << 7);  /* [7:5] N_shift */
	data32 |= (0 << 4);  /* [  4] CTS_manual */
	data32 |= (0 << 0);  /* [3:0] manual AudCTS[19:16] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_CTS3, data32);

	hdmitx_wr_reg(HDMITX_DWC_AUD_CTS2, 0); /* manual AudCTS[15:8] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_CTS1, 0); /* manual AudCTS[7:0] */

	data32 = 0;
	data32 |= (1 << 7);  /* [  7] ncts_atomic_write */
	data32 |= (((aud_n_para>>16)&0xf) << 0);  /* [3:0] AudN[19:16] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_N3, data32);
	hdmitx_wr_reg(HDMITX_DWC_AUD_N2, (aud_n_para>>8)&0xff); /* AudN[15:8] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_N1, aud_n_para&0xff); /* AudN[7:0] */

	/* audio packetizer config */
	hdmitx_wr_reg(HDMITX_DWC_AUD_INPUTCLKFS, TX_I2S_SPDIF ? 4 : 0);

	hdmitx_set_aud_chnls();

	if (TX_I2S_SPDIF) {
		hdmitx_wr_reg(HDMITX_DWC_AUD_CONF0,
			hdmitx_rd_reg(HDMITX_DWC_AUD_CONF0) |
			((TX_I2S_8_CHANNEL ? 0xf : 0x1) << 0));
		/* Enable audi2s_fifo_overrun interrupt */
		hdmitx_wr_reg(HDMITX_DWC_AUD_INT1,
			hdmitx_rd_reg(HDMITX_DWC_AUD_INT1) & (~(1<<4)));
		/* Wait for 40 us for TX I2S decoder to settle */
		msleep(20);
	}
	hdmitx_set_reg_bits(HDMITX_DWC_FC_DATAUTO3, 1, 0, 1);

	enable_audio_spdif();
	enable_audio_i2s();
	hdmitx_set_aud_pkt_type(CT_PCM);
	return 1;
}

static void hdmitx_setupirq(struct hdmitx_dev *phdev)
{
	int r;
	hdmitx_wr_reg(HDMITX_TOP_INTR_STAT_CLR, 0x7);
	r = request_irq(phdev->irq_hpd, &intr_handler,
			IRQF_SHARED, "hdmitx",
			(void *)phdev);
}

static void hdmitx_uninit(struct hdmitx_dev *phdev)
{
	free_irq(phdev->irq_hpd, (void *)phdev);
	hdmi_print(1, "power off hdmi, unmux hpd\n");

	phy_pll_off();
	digital_clk_off(7); /* off sys clk */
	hdmitx_hpd_hw_op(HPD_UNMUX_HPD);
}

static int hdmitx_cntl(struct hdmitx_dev *hdev, unsigned cmd, unsigned argv)
{
	if (cmd == HDMITX_AVMUTE_CNTL) {
		return 0;
	} else if (cmd == HDMITX_EARLY_SUSPEND_RESUME_CNTL) {
		if (argv == HDMITX_EARLY_SUSPEND) {
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0, 30, 1);
			hdmi_phy_suspend();
		}
		if (argv == HDMITX_LATE_RESUME)
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 1, 30, 1);
			/* hdmi_phy_wakeup();	// no need */
		return 0;
	} else if (cmd == HDMITX_HDCP_MONITOR) {
		/* TODO */
		return 0;
	} else if (cmd == HDMITX_IP_SW_RST) {
		return 0;	/* TODO */
	} else if (cmd == HDMITX_CBUS_RST) {
		return 0;/* todo */
		hd_set_reg_bits(P_RESET2_REGISTER, 1, 15, 1);
		return 0;
	} else if (cmd == HDMITX_INTR_MASKN_CNTL)
		/* TODO */
		return 0;
	else if (cmd == HDMITX_HWCMD_MUX_HPD_IF_PIN_HIGH) {
		/* turnon digital module if gpio is high */
		if (hdmitx_hpd_hw_op(HPD_IS_HPD_MUXED) == 0) {
			if (hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO)) {
				hdev->internal_mode_change = 0;
				msleep(500);
				if (hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO)) {
					hdmi_print(IMP, HPD "mux hpd\n");
					digital_clk_on(4);
					delay_us(1000*100);
					hdmitx_hpd_hw_op(HPD_MUX_HPD);
				}
			}
		}
	} else if (cmd == HDMITX_HWCMD_MUX_HPD)
		hdmitx_hpd_hw_op(HPD_MUX_HPD);
/* For test only. */
	else if (cmd == HDMITX_HWCMD_TURNOFF_HDMIHW) {
		int unmux_hpd_flag = argv;
		if (unmux_hpd_flag) {
			hdmi_print(IMP, SYS "power off hdmi, unmux hpd\n");
			phy_pll_off();
			digital_clk_off(4); /* off sys clk */
			hdmitx_hpd_hw_op(HPD_UNMUX_HPD);
		} else {
			hdmi_print(IMP, SYS "power off hdmi\n");
			digital_clk_on(6);
			phy_pll_off();
			digital_clk_off(3); /* do not off sys clk */
		}
#ifdef CONFIG_HDMI_TX_PHY
	digital_clk_off(7);
#endif
	}
	return 0;
}

static void hdmitx_print_info(struct hdmitx_dev *hdev, int pr_info_flag)
{
	hdmi_print(INF, "------------------\nHdmitx driver version: ");
	hdmi_print(INF, "%s\nSerial %x\nColor Depth %d\n", HDMITX_VER,
		serial_reg_val, color_depth_f);
	hdmi_print(INF, "current vout index %d\n", cur_vout_index);
	hdmi_print(INF, "reset sequence %d\n", new_reset_sequence_flag);
	hdmi_print(INF, "power mode %d\n", power_mode);
	hdmi_print(INF, "%spowerdown when unplug\n",
		hdev->unplug_powerdown?"":"do not ");
	hdmi_print(INF, "use_tvenc_conf_flag=%d\n", use_tvenc_conf_flag);
	hdmi_print(INF, "vdac %s\n", power_off_vdac_flag?"off":"on");
	hdmi_print(INF, "hdmi audio %s\n", hdmi_audio_off_flag?"off":"on");
	if (!hdmi_audio_off_flag)
		hdmi_print(INF, "audio out type %s\n",
			i2s_to_spdif_flag?"spdif":"i2s");
	hdmi_print(INF, "delay flag %d\n", delay_flag);
	hdmi_print(INF, "------------------\n");
}

struct aud_cts_log {
	unsigned int val:20;
};

static inline unsigned int get_msr_cts(void)
{
	unsigned int ret = 0;

	ret = hdmitx_rd_reg(HDMITX_DWC_AUD_CTS1);
	ret += (hdmitx_rd_reg(HDMITX_DWC_AUD_CTS2) << 8);
	ret += ((hdmitx_rd_reg(HDMITX_DWC_AUD_CTS3) & 0xf) << 16);

	return ret;
}

#define AUD_CTS_LOG_NUM	1000
struct aud_cts_log cts_buf[AUD_CTS_LOG_NUM];
static void cts_test(struct hdmitx_dev *hdev)
{
	int i;
	unsigned int min = 0, max = 0, total = 0;

	pr_info("\nhdmitx: audio: cts test\n");
	memset(cts_buf, 0, sizeof(cts_buf));
	for (i = 0; i < AUD_CTS_LOG_NUM; i++) {
		cts_buf[i].val = get_msr_cts();
		mdelay(1);
	}

	pr_info("\ncts change:\n");
	for (i = 1; i < AUD_CTS_LOG_NUM; i++) {
		if (cts_buf[i].val > cts_buf[i-1].val)
			pr_info("dis: +%d  [%d] %d  [%d] %d\n",
				cts_buf[i].val - cts_buf[i-1].val, i,
				cts_buf[i].val, i - 1, cts_buf[i - 1].val);
		if (cts_buf[i].val < cts_buf[i-1].val)
			pr_info("dis: %d  [%d] %d  [%d] %d\n",
				cts_buf[i].val - cts_buf[i-1].val, i,
				cts_buf[i].val, i - 1, cts_buf[i - 1].val);
		}

	for (i = 0; i < AUD_CTS_LOG_NUM; i++) {
		total += cts_buf[i].val;
		if (min > cts_buf[i].val)
			min = cts_buf[i].val;
		if (max < cts_buf[i].val)
			max = cts_buf[i].val;
	}
	pr_info("\nCTS Min: %d   Max: %d   Avg: %d/1000\n\n", min, max, total);
}

void hdmitx_dump_inter_timing(void)
{
	unsigned int tmp = 0;
#define CONNECT2REG(reg) ((hdmitx_rd_reg(reg)) + (hdmitx_rd_reg(reg + 1) << 8))
	tmp = CONNECT2REG(HDMITX_DWC_FC_INHACTV0);
	pr_info("Hactive = %d\n", tmp);

	tmp = CONNECT2REG(HDMITX_DWC_FC_INHBLANK0);
	pr_info("Hblank = %d\n", tmp);

	tmp = CONNECT2REG(HDMITX_DWC_FC_INVACTV0);
	pr_info("Vactive = %d\n", tmp);

	tmp = hdmitx_rd_reg(HDMITX_DWC_FC_INVBLANK);
	pr_info("Vblank = %d\n", tmp);

	tmp = CONNECT2REG(HDMITX_DWC_FC_HSYNCINDELAY0);
	pr_info("Hfront = %d\n", tmp);

	tmp = CONNECT2REG(HDMITX_DWC_FC_HSYNCINWIDTH0);
	pr_info("Hsync = %d\n", tmp);

	tmp = hdmitx_rd_reg(HDMITX_DWC_FC_VSYNCINDELAY);
	pr_info("Vfront = %d\n", tmp);

	tmp = hdmitx_rd_reg(HDMITX_DWC_FC_VSYNCINWIDTH);
	pr_info("Vsync = %d\n", tmp);

	/* HDMITX_DWC_FC_INFREQ0 ??? */
}

#define DUMP_CVREG_SECTION(start, end) \
do { \
	if (start > end) { \
		pr_info("Error start = 0x%x > end = 0x%x\n", \
			((start & 0xffff) >> 2), ((end & 0xffff) >> 2)); \
		break; \
	} \
	pr_info("Start = 0x%x[0x%x]   End = 0x%x[0x%x]\n", \
		start, ((start & 0xffff) >> 2), end, ((end & 0xffff) >> 2)); \
	for (addr = start; addr < end + 1; addr += 4) {	\
		val = hd_read_reg(addr); \
		if (val) \
			pr_info("0x%08x[0x%04x]: 0x%08x\n", addr, \
				((addr & 0xffff) >> 2), val); \
		} \
} while (0)

static void hdmitx_dump_all_cvregs(void)
{
#if 0
	unsigned addr = 0, val = 0;

	DUMP_CVREG_SECTION(P_STB_TOP_CONFIG, P_CIPLUS_ENDIAN);
	DUMP_CVREG_SECTION(P_PREG_CTLREG0_ADDR, P_AHB_BRIDGE_CNTL_REG2);
	DUMP_CVREG_SECTION(P_BT_CTRL, P_BT656_ADDR_END);
	DUMP_CVREG_SECTION(P_VERSION_CTRL, P_RESET7_LEVEL);
	DUMP_CVREG_SECTION(P_SCR_HIU, P_HHI_HDMIRX_AUD_PLL_CNTL6);
	DUMP_CVREG_SECTION(P_PARSER_CONTROL, P_PARSER_AV2_WRAP_COUNT);
	DUMP_CVREG_SECTION(P_DVIN_FRONT_END_CTRL, P_DVIN_CTRL_STAT);
	DUMP_CVREG_SECTION(P_AIU_958_BPF, P_AIU_I2S_CBUS_DDR_ADDR);
	DUMP_CVREG_SECTION(P_GE2D_GEN_CTRL0, P_GE2D_GEN_CTRL4);
	DUMP_CVREG_SECTION(P_AUDIO_COP_CTL2, P_EE_ASSIST_MBOX3_FIQ_SEL);
	DUMP_CVREG_SECTION(P_AUDIN_SPDIF_MODE, P_AUDIN_ADDR_END);
	DUMP_CVREG_SECTION(P_VDIN_SCALE_COEF_IDX, P_VDIN0_SCALE_COEF_IDX);
	DUMP_CVREG_SECTION(P_VDIN0_SCALE_COEF, P_VDIN1_ASFIFO_CTRL3);
	DUMP_CVREG_SECTION(P_L_GAMMA_CNTL_PORT, P_MLVDS_RESET_CONFIG_LO);
	DUMP_CVREG_SECTION(P_VPP2_DUMMY_DATA, P_DI_CHAN2_URGENT_CTRL);
	DUMP_CVREG_SECTION(P_DI_PRE_CTRL, P_DI_CANVAS_URGENT2);
	DUMP_CVREG_SECTION(P_ENCP_VFIFO2VD_CTL, P_VIU2_VD1_FMT_W);
	DUMP_CVREG_SECTION(P_VPU_OSD1_MMC_CTRL, P_VPU_PROT3_REQ_ONOFF);
	DUMP_CVREG_SECTION(P_D2D3_GLB_CTRL, P_D2D3_RESEV_STATUS2);
	DUMP_CVREG_SECTION(P_VI_HIST_CTRL, P_DEMO_CRTL);
	DUMP_CVREG_SECTION(P_AO_RTI_STATUS_REG0, P_AO_SAR_ADC_REG12);
	DUMP_CVREG_SECTION(P_STB_VERSION, P_DEMUX_SECTION_RESET_3);
#endif
}

#define DUMP_HDMITXREG_SECTION(start, end) \
do { \
	if (start > end) { \
		pr_info("Error start = 0x%lx > end = 0x%lx\n", start, end); \
		break; \
	} \
	pr_info("Start = 0x%lx   End = 0x%lx\n", start, end); \
	for (addr = start; addr < end + 1; addr++) { \
		val = hdmitx_rd_reg(addr); \
		if (val) \
			pr_info("[0x%08x]: 0x%08x\n", addr, val); \
	} \
} while (0)

static void hdmitx_dump_intr(void)
{
	unsigned addr = 0, val = 0;

	DUMP_HDMITXREG_SECTION(HDMITX_DWC_IH_FC_STAT0, HDMITX_DWC_IH_MUTE);
}

static void mode420_half_horizontal_para(void)
{
	unsigned int hactive = 0;
	unsigned int hblank = 0;
	unsigned int hfront = 0;
	unsigned int hsync = 0;

	hactive =  hdmitx_rd_reg(HDMITX_DWC_FC_INHACTV0);
	hactive += (hdmitx_rd_reg(HDMITX_DWC_FC_INHACTV1) & 0x3f) << 8;
	hblank =  hdmitx_rd_reg(HDMITX_DWC_FC_INHBLANK0);
	hblank += (hdmitx_rd_reg(HDMITX_DWC_FC_INHBLANK1) & 0x1f) << 8;
	hfront =  hdmitx_rd_reg(HDMITX_DWC_FC_HSYNCINDELAY0);
	hfront += (hdmitx_rd_reg(HDMITX_DWC_FC_HSYNCINDELAY1) & 0x1f) << 8;
	hsync =  hdmitx_rd_reg(HDMITX_DWC_FC_HSYNCINWIDTH0);
	hsync += (hdmitx_rd_reg(HDMITX_DWC_FC_HSYNCINWIDTH1) & 0x3) << 8;

	hactive = hactive / 2;
	hblank = hblank / 2;
	hfront = hfront / 2;
	hsync = hsync / 2;

	hdmitx_wr_reg(HDMITX_DWC_FC_INHACTV0, (hactive & 0xff));
	hdmitx_wr_reg(HDMITX_DWC_FC_INHACTV1, ((hactive >> 8) & 0x3f));
	hdmitx_wr_reg(HDMITX_DWC_FC_INHBLANK0, (hblank  & 0xff));
	hdmitx_wr_reg(HDMITX_DWC_FC_INHBLANK1, ((hblank >> 8) & 0x1f));
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINDELAY0, (hfront & 0xff));
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINDELAY1, ((hfront >> 8) & 0x1f));
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINWIDTH0, (hsync & 0xff));
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINWIDTH1, ((hsync >> 8) & 0x3));
}

static void hdmitx_4k2k60hz444_debug(void)
{
	pr_info("4k2k60hzYCBCR444\n");
	pr_info("set clk:data = 1 : 40 set double rate\n");
	hd_write_reg(P_HHI_VID_CLK_DIV, 0x101);
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_01, 0x0);
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_23, 0xffffffff);
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_CNTL, 1);
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_CNTL, 2);
	hdmitx_wr_reg(HDMITX_DWC_FC_AVIVID, HDMI_3840x2160p60_16x9);
}

static void hdmitx_4k2k5g_debug(void)
{
	pr_info("4k2k5g\n");
	pr_info("set clk:data = 1 : 40 set double rate\n");
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_01, 0x0);
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_23, 0xffffffff);
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_CNTL, 1);
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_CNTL, 2);
	hdmitx_wr_reg(HDMITX_DWC_FC_AVIVID, HDMI_3840x2160p50_16x9);
/*	set_vmode_clk(VMODE_4K2K_5G); */
	hd_write_reg(P_HHI_VID_CLK_DIV, 0x100);
}

static void hdmitx_4k2k5g420_debug(void)
{
	pr_info("4k2k5g420\n");
	pr_info("set clk:data = 1 : 10 set double rate\n");
	hdmitx_wr_reg(HDMITX_DWC_FC_AVIVID, HDMI_3840x2160p50_16x9);
/*	set_vmode_clk(VMODE_4K2K_5G); */
	hdmitx_wr_reg(HDMITX_DWC_FC_AVICONF0, 0x43);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x404e00);
	hd_write_reg(P_HHI_VID_CLK_DIV, 0x100);
	hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 1, 16, 4);
	hd_write_reg(P_VPU_HDMI_SETTING, 0x10e);
	hd_write_reg(P_VPU_HDMI_FMT_CTRL, 0x1a);
	hdmitx_wr_reg(HDMITX_DWC_FC_SCRAMBLER_CTRL, 0);
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_01, 0x001f001f);
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_23, 0x001f001f);
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_CNTL, 1);
	hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_CNTL, 2);
	mode420_half_horizontal_para();
}

static void hdmitx_debug(struct hdmitx_dev *hdev, const char *buf)
{
	char tmpbuf[128];
	int i = 0;
	int ret;
	unsigned long adr = 0;
	unsigned long value = 0;
	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;
	if ((strncmp(tmpbuf, "dumpreg", 7) == 0) ||
		(strncmp(tmpbuf, "dumptvencreg", 12) == 0)) {
		hdmitx_dump_tvenc_reg(hdev->cur_VIC, 1);
		return;
	} else if (strncmp(tmpbuf, "4k2k60hz444", 11) == 0)
		hdmitx_4k2k60hz444_debug();
	else if (strncmp(tmpbuf, "testpll", 7) == 0)
		return;
	else if (strncmp(tmpbuf, "4k2k5g420", 9) == 0) {
		hdmitx_4k2k5g420_debug();
		return;
	} else if (strncmp(tmpbuf, "4k2k5g", 6) == 0) {
		hdmitx_4k2k5g_debug();
		return;
	} else if (strncmp(tmpbuf, "testedid", 8) == 0) {
		dd();
		hdev->HWOp.CntlDDC(hdev, DDC_RESET_EDID, 0);
		hdev->HWOp.CntlDDC(hdev, DDC_EDID_READ_DATA, 0);
	} else if (strncmp(tmpbuf, "dumptiming", 10) == 0) {
		hdmitx_dump_inter_timing();
		return;
	} else if (strncmp(tmpbuf, "testaudio", 9) == 0) {
		hdmitx_set_audmode(hdev, NULL);
	} else if (strncmp(tmpbuf, "dumpintr", 8) == 0) {
		hdmitx_dump_intr();
	} else if (strncmp(tmpbuf, "testhdcp", 8) == 0) {
		hdmitx_hdcp_test();
	} else if (strncmp(tmpbuf, "dumpallregs", 11) == 0) {
		hdmitx_dump_all_cvregs();
		return;
	} else if (strncmp(tmpbuf, "chkfmt", 6) == 0) {
		check_detail_fmt();
		return;
	} else if (strncmp(tmpbuf, "testcts", 7) == 0) {
		cts_test(hdev);
		return;
	} else if (strncmp(tmpbuf, "ss", 2) == 0) {
		pr_info("hdev->output_blank_flag: 0x%x\n",
			hdev->output_blank_flag);
		pr_info("hdev->hpd_state: 0x%x\n", hdev->hpd_state);
		pr_info("hdev->cur_VIC: 0x%x\n", hdev->cur_VIC);
	} else if (strncmp(tmpbuf, "hpd_lock", 8) == 0) {
		if (tmpbuf[8] == '1') {
			hdev->hpd_lock = 1;
			hdmi_print(INF, HPD "hdmitx: lock hpd\n");
		} else {
			hdev->hpd_lock = 0;
			hdmi_print(INF, HPD "hdmitx: unlock hpd\n");
		}
		return;
	} else if (strncmp(tmpbuf, "vic", 3) == 0) {
		pr_info("hdmi vic count = %d\n", hdev->vic_count);
		if ((tmpbuf[3] >= '0') && (tmpbuf[3] <= '9')) {
			hdev->vic_count = tmpbuf[3] - '0';
			hdmi_print(INF, SYS "set hdmi vic count = %d\n",
				hdev->vic_count);
		}
	} else if (strncmp(tmpbuf, "cec", 3) == 0)
		return;
	else if (strncmp(tmpbuf, "dumphdmireg", 11) == 0) {
		unsigned char reg_val = 0;
		unsigned int reg_adr = 0;
		for (reg_adr = HDMITX_TOP_SW_RESET;
			reg_adr < HDMITX_TOP_STAT0 + 1; reg_adr++) {
			reg_val = hdmitx_rd_reg(reg_adr);
			if (reg_val)
				pr_info("TOP[0x%x]: 0x%x\n", reg_adr, reg_val);
		}
		for (reg_adr = HDMITX_DWC_DESIGN_ID;
			reg_adr < HDMITX_DWC_I2CM_SCDC_UPDATE1 + 1; reg_adr++) {
			if ((reg_adr > HDMITX_DWC_HDCP_BSTATUS_0 - 1) &&
				(reg_adr < HDMITX_DWC_HDCPREG_BKSV0)) {
				hdmitx_wr_reg(HDMITX_DWC_A_KSVMEMCTRL, 0x1);
				hdmitx_poll_reg(HDMITX_DWC_A_KSVMEMCTRL,
					(1<<1), 2 * HZ);
				reg_val = hdmitx_rd_reg(reg_adr);
			} else
				reg_val = hdmitx_rd_reg(reg_adr);
			if (reg_val) {
				if (reg_val)
					/* print all HDCP regisiters */
					pr_info("DWC[0x%x]: 0x%x\n",
						reg_adr, reg_val);
			}
		}
		return;
	} else if (strncmp(tmpbuf, "dumpcecreg", 10) == 0) {
		unsigned char cec_val = 0;
		unsigned int cec_adr = 0;
		/* HDMI CEC Regs address range:0xc000~0xc01c;0xc080~0xc094 */
		for (cec_adr = 0xc000; cec_adr < 0xc01d; cec_adr++) {
			cec_val = hdmitx_rd_reg(cec_adr);
			hdmi_print(INF, "HDMI CEC Regs[0x%x]: 0x%x\n",
				cec_adr, cec_val);
		}
		for (cec_adr = 0xc080; cec_adr < 0xc095; cec_adr++) {
			cec_val = hdmitx_rd_reg(cec_adr);
			hdmi_print(INF, "HDMI CEC Regs[0x%x]: 0x%x\n",
				cec_adr, cec_val);
		}
		return;
	} else if (strncmp(tmpbuf, "dumpcbusreg", 11) == 0) {
		unsigned i, val;
		for (i = 0; i < 0x3000; i++) {
			val = hd_read_reg(CBUS_REG_ADDR(i));
			if (val)
				pr_info("CBUS[0x%x]: 0x%x\n", i, val);
		}
		return;
	} else if (strncmp(tmpbuf, "dumpvcbusreg", 12) == 0) {
		unsigned i, val;
		for (i = 0; i < 0x3000; i++) {
			val = hd_read_reg(VCBUS_REG_ADDR(i));
			if (val)
				pr_info("VCBUS[0x%x]: 0x%x\n", i, val);
		}
		return;
	} else if (strncmp(tmpbuf, "log", 3) == 0) {
		if (strncmp(tmpbuf+3, "hdcp", 4) == 0) {
			static unsigned int i = 1;
			if (i & 1)
				hdev->log |= HDMI_LOG_HDCP;
			else
				hdev->log &= ~HDMI_LOG_HDCP;
			i++;
		}
		return;
	} else if (strncmp(tmpbuf, "pllcalc", 7) == 0) {
		/* TODO	clk_measure(0xff); */
		return;
	} else if (strncmp(tmpbuf, "hdmiaudio", 9) == 0) {
		ret = kstrtoul(tmpbuf+9, 16, &value);
		if (value == 1) {
			hdmi_audio_off_flag = 0;
			hdmi_audio_init(i2s_to_spdif_flag);
		} else if (value == 0)
			;
		return;
	} else if (strncmp(tmpbuf, "cfgreg", 6) == 0) {
		ret = kstrtoul(tmpbuf+6, 16, &adr);
		ret = kstrtoul(buf+i+1, 16, &value);
		hdmitx_config_tvenc_reg(hdev->cur_VIC, adr, value);
		return;
	} else if (strncmp(tmpbuf, "tvenc_flag", 10) == 0) {
		use_tvenc_conf_flag = tmpbuf[10]-'0';
		hdmi_print(INF, "set use_tvenc_conf_flag = %d\n",
			use_tvenc_conf_flag);
	} else if (strncmp(tmpbuf, "reset", 5) == 0) {
		if (tmpbuf[5] == '0')
			new_reset_sequence_flag = 0;
		else
			new_reset_sequence_flag = 1;
		return;
	} else if (strncmp(tmpbuf, "delay_flag", 10) == 0)
		delay_flag = tmpbuf[10]-'0';
	else if (tmpbuf[0] == 'v') {
		hdmitx_print_info(hdev, 1);
		return;
	} else if (tmpbuf[0] == 's') {
		ret = kstrtoul(tmpbuf+1, 16, &serial_reg_val);
		return;
	} else if (tmpbuf[0] == 'c') {
		if (tmpbuf[1] == 'd') {
			ret = kstrtoul(tmpbuf+2, 10, &color_depth_f);
			if ((color_depth_f != 24) && (color_depth_f != 30) &&
				(color_depth_f != 36)) {
				pr_info("Color depth %lu is not supported\n",
					color_depth_f);
			color_depth_f = 0;
			}
		return;
	} else if (tmpbuf[1] == 's') {
		ret = kstrtoul(tmpbuf+2, 10, &color_space_f);
		if (color_space_f > 2) {
			pr_info("Color space %lu is not supported\n",
				color_space_f);
			color_space_f = 0;
		}
	}
	} else if (strncmp(tmpbuf, "i2s", 2) == 0) {
		if (strncmp(tmpbuf+3, "off", 3) == 0)
			i2s_to_spdif_flag = 1;
		else
			i2s_to_spdif_flag = 0;
	} else if (strncmp(tmpbuf, "pattern_on", 10) == 0) {
		/* turn_on_shift_pattern(); */
		hdmi_print(INF, "Shift Pattern On\n");
		return;
	} else if (strncmp(tmpbuf, "pattern_off", 11) == 0) {
		hdmi_print(INF, "Shift Pattern Off\n");
		return;
	} else if (strncmp(tmpbuf, "prbs", 4) == 0)
		/* int prbs_mode =kstrtoul(tmpbuf+4, NULL, 10); */
		return;
	else if (tmpbuf[0] == 'w') {
		unsigned read_back = 0;
		ret = kstrtoul(tmpbuf+2, 16, &adr);
		ret = kstrtoul(buf+i+1, 16, &value);
		if (buf[1] == 'h') {
			hdmitx_wr_reg((unsigned int)adr, (unsigned int)value);
			read_back = hdmitx_rd_reg(adr);
		}
		hdmi_print(INF, "write %x to %s reg[%x]\n", value, "HDMI", adr);
/* Add read back function in order to judge writting is OK or NG. */
		hdmi_print(INF, "Read Back %s reg[%x]=%x\n", "HDMI",
			adr, read_back);
	} else if (tmpbuf[0] == 'r') {
		ret = kstrtoul(tmpbuf+2, 16, &adr);
		if (buf[1] == 'h')
			value = hdmitx_rd_reg(adr);
		hdmi_print(INF, "%s reg[%x]=%x\n", "HDMI", adr, value);
	}
}


static void hdmitx_getediddata(unsigned char *des, unsigned char *src)
{
	int i = 0;
	unsigned int blk = src[126] + 1;

	if (blk > 4)
		blk = 4;

	for (i = 0; i < 128 * blk; i++)
		des[i] = src[i];
}

/*
 * Note: read 8 Bytes of EDID data every time
 */
static void hdmitx_read_edid(unsigned char *rx_edid)
{
	unsigned int timeout = 0;
	unsigned int i;
	unsigned int byte_num = 0;
	unsigned char   blk_no = 1;

	/* Program SLAVE/SEGMENT/ADDR */
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, 0x50);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGADDR,  0x30);
	/* Read complete EDID data sequentially */
	while (byte_num < 128 * blk_no) {
		if ((byte_num % 256) == 0)
			hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGPTR, byte_num>>8);
		hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS,  byte_num&0xff);
	/* Do extended sequential read */
		hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1<<3);
	/* Wait until I2C done */
		timeout = 0;
		while ((!(hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0) & (1 << 1)))
			&& (timeout < 3)) {
			mdelay(2);
			timeout++;
		}
		if (timeout == 3)
			pr_info("ddc timeout\n");
		hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 1 << 1);
	/* Read back 8 bytes */
		for (i = 0; i < 8; i++) {
			rx_edid[byte_num] =
				hdmitx_rd_reg(HDMITX_DWC_I2CM_READ_BUFF0 + i);
			if (byte_num == 126) {
				blk_no = rx_edid[byte_num] + 1;
				if (blk_no > 4) {
					pr_info("edid extension block number:");
					pr_info(" %d, reset to MAX 3\n",
						blk_no - 1);
					blk_no = 4; /* Max extended block */
				}
			}
		byte_num++;
		}
	}
} /* hdmi20_tx_read_edid */

static unsigned char tmp_edid_buf[128*EDID_MAX_BLOCK] = { 0 };

static int hdmitx_cntl_ddc(struct hdmitx_dev *hdev, unsigned cmd,
	unsigned long argv)
{
	int i = 0;
	unsigned char *tmp_char = NULL;
	return 0;	/* tmp direct return */
	if (!(cmd & CMD_DDC_OFFSET))
		hdmi_print(ERR, "ddc: " "w: invalid cmd 0x%x\n", cmd);
	else
		hdmi_print(LOW, "ddc: " "cmd 0x%x\n", cmd);

	switch (cmd) {
	case DDC_RESET_EDID:
		hdmitx_wr_reg(HDMITX_DWC_I2CM_SOFTRSTZ, 0);
		memset(tmp_edid_buf, 0, ARRAY_SIZE(tmp_edid_buf));
		break;
	case DDC_IS_EDID_DATA_READY:

		break;
	case DDC_EDID_READ_DATA:
		hdmitx_read_edid(tmp_edid_buf);
		break;
	case DDC_EDID_GET_DATA:
		if (argv == 0)
			hdmitx_getediddata(&hdev->EDID_buf[0], tmp_edid_buf);
		else
			hdmitx_getediddata(&hdev->EDID_buf1[0], tmp_edid_buf);
		break;
	case DDC_PIN_MUX_OP:
		if (argv == PIN_MUX)
			hdmitx_ddc_hw_op(DDC_MUX_DDC);
		if (argv == PIN_UNMUX)
			hdmitx_ddc_hw_op(DDC_UNMUX_DDC);
		break;
	case DDC_EDID_CLEAR_RAM:
		for (i = 0; i < EDID_RAM_ADDR_SIZE; i++)
			hdmitx_wr_reg(HDMITX_DWC_I2CM_READ_BUFF0 + i, 0);
		break;
	case DDC_RESET_HDCP:

		break;
	case DDC_HDCP_OP:
		if (argv == HDCP_ON)
			;
		if (argv == HDCP_OFF)
			;
		break;
	case DDC_IS_HDCP_ON:
/* argv = !!((hdmitx_rd_reg(TX_HDCP_MODE)) & (1 << 7)); */
		break;
	case DDC_HDCP_GET_AKSV:
		tmp_char = (unsigned char *) argv;
		for (i = 0; i < 5; i++)
			;
		break;
	case DDC_HDCP_GET_BKSV:
		tmp_char = (unsigned char *) argv;
		for (i = 0; i < 5; i++)
			;
		break;
	case DDC_HDCP_GET_AUTH:
		break;
	default:
		hdmi_print(INF, "ddc: " "unknown cmd: 0x%x\n", cmd);
	}
	return 1;
}

#if 0
/* clear hdmi packet configure registers */
static void hdmitx_clr_sub_packet(unsigned int reg_base)
{
	int i = 0;
	for (i = 0; i < 0x20; i++)
		hdmitx_wr_reg(reg_base + i, 0x00);
}
#endif

static int hdmitx_cntl_config(struct hdmitx_dev *hdev, unsigned cmd,
	unsigned argv)
{
	if (!(cmd & CMD_CONF_OFFSET))
		hdmi_print(ERR, "config: " "hdmitx: w: invalid cmd 0x%x\n",
			cmd);
	else
		hdmi_print(LOW, "config: " "hdmitx: conf cmd 0x%x\n", cmd);

	switch (cmd) {
	case CONF_HDMI_DVI_MODE:
		if (argv == HDMI_MODE)
			;
		if (argv == DVI_MODE)
			;
		break;
	case CONF_SYSTEM_ST:
		break;
	case CONF_AUDIO_MUTE_OP:
		if (argv == AUDIO_MUTE)
			;
		if ((argv == AUDIO_UNMUTE) && (hdev->tx_aud_cfg != 0))
			;
		break;
	case CONF_VIDEO_BLANK_OP:
		return 1;   /* TODO */
		if (argv == VIDEO_BLANK) {
			/* set blank CrYCb as 0x200 0x0 0x200 */
			hd_write_reg(P_VPU_HDMI_DATA_OVR,
				(0x200 << 20) | (0x0 << 10) | (0x200 << 0));
			/* Output data map: CrYCb */
			hd_set_reg_bits(P_VPU_HDMI_SETTING, 0, 5, 3);
			/* Enable HDMI data override */
			hd_set_reg_bits(P_VPU_HDMI_DATA_OVR, 1, 31, 1);
		}
		if (argv == VIDEO_UNBLANK)
			/* Disable HDMI data override */
			hd_write_reg(P_VPU_HDMI_DATA_OVR, 0);
		break;
	case CONF_CLR_AVI_PACKET:
		hdmitx_wr_reg(HDMITX_DWC_FC_AVIVID, 0);
		break;
	case CONF_CLR_VSDB_PACKET:
		break;
	case CONF_CLR_AUDINFO_PACKET:
		break;
	default:
		hdmi_print(ERR, "config: ""hdmitx: unknown cmd: 0x%x\n", cmd);
	}
	return 1;
}

static int hdmitx_cntl_misc(struct hdmitx_dev *hdev, unsigned cmd,
	unsigned argv)
{
	if (!(cmd & CMD_MISC_OFFSET))
		hdmi_print(ERR, "misc: " "hdmitx: w: invalid cmd 0x%x\n", cmd);
	else
		hdmi_print(LOW, "misc: " "hdmitx: misc cmd 0x%x\n", cmd);

	switch (cmd) {
	case MISC_HPD_MUX_OP:
		if (argv == PIN_MUX)
			argv = HPD_MUX_HPD;
		else
			argv = HPD_UNMUX_HPD;
	return hdmitx_hpd_hw_op(argv);
		break;
	case MISC_HPD_GPI_ST:
		return 1;
/* return hdmitx_hpd_hw_op(HPD_READ_HPD_GPIO); // tmp mark */
		break;
	case MISC_HPLL_OP:
		pr_info("todo\n");
		break;
		if (argv == HPLL_ENABLE)
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 1, 30, 1);
		if (argv == HPLL_DISABLE)
			hd_set_reg_bits(P_HHI_HDMI_PLL_CNTL, 0, 30, 1);
		break;
	case MISC_TMDS_PHY_OP:
		if (argv == TMDS_PHY_ENABLE)
			hdmi_phy_wakeup(hdev);  /* TODO */
		if (argv == TMDS_PHY_DISABLE)
			hdmi_phy_suspend();
		break;
	case MISC_VIID_IS_USING:
		break;
	case MISC_CONF_MODE420:
		hd_write_reg(P_VPU_HDMI_FMT_CTRL, 0x1a);
		hd_write_reg(P_VPU_HDMI_SETTING, 0x10e);
		break;
	case MISC_TMDS_CLK_DIV40:
		hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_CNTL, 1);
		msleep(20);
		hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_CNTL, 3);
		break;
	default:
		hdmi_print(ERR, "misc: " "hdmitx: unknown cmd: 0x%x\n", cmd);
	}
	return 1;
}

static int hdmitx_get_state(struct hdmitx_dev *hdev, unsigned cmd,
	unsigned argv)
{
	if (!(cmd & CMD_STAT_OFFSET))
		hdmi_print(ERR, "stat: " "hdmitx: w: invalid cmd 0x%x\n", cmd);
	else
		hdmi_print(LOW, "stat: " "hdmitx: misc cmd 0x%x\n", cmd);

	switch (cmd) {
	case STAT_VIDEO_VIC:
		return hdmitx_rd_reg(HDMITX_DWC_FC_AVIVID); /* TODO HDMIVIC */
		break;
	case STAT_VIDEO_CLK:
		break;
	default:
		break;
	}
	return 0;
}

/* The following two functions should move to */
/* static struct platform_driver amhdmitx_driver.suspend & .wakeup */
/* For tempelet use only. */
/* Later will change it. */
struct hdmi_phy {
	unsigned long reg;
	unsigned long val_sleep;
	unsigned long val_save;
};

static void hdmi_phy_suspend(void)
{
	hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x08418d00);
}

static void hdmi_phy_wakeup(struct hdmitx_dev *hdev)
{
	hdmitx_set_phy(hdev);
	/* hdmi_print(INF, SYS "phy wakeup\n"); */
}

#define CLK_UTIL_VID_PLL_DIV_1    0
#define CLK_UTIL_VID_PLL_DIV_2      1
#define CLK_UTIL_VID_PLL_DIV_3      2
#define CLK_UTIL_VID_PLL_DIV_3p5    3
#define CLK_UTIL_VID_PLL_DIV_3p75   4
#define CLK_UTIL_VID_PLL_DIV_4      5
#define CLK_UTIL_VID_PLL_DIV_5      6
#define CLK_UTIL_VID_PLL_DIV_6      7
#define CLK_UTIL_VID_PLL_DIV_6p25   8
#define CLK_UTIL_VID_PLL_DIV_7      9
#define CLK_UTIL_VID_PLL_DIV_7p5    10
#define CLK_UTIL_VID_PLL_DIV_12     11
#define CLK_UTIL_VID_PLL_DIV_14     12
#define CLK_UTIL_VID_PLL_DIV_15     13

static void clocks_set_vid_clk_div(uint32_t div_sel)
{
	uint32_t shift_val;
	uint32_t shift_sel;

	/* Disable the output clock */
	hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 0, 15, 1);
	hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 0, 19, 1);

	switch (div_sel) {
	case CLK_UTIL_VID_PLL_DIV_1:
		shift_val = 0xFFFF;
		shift_sel = 0;
		break;
	case CLK_UTIL_VID_PLL_DIV_2:
		shift_val = 0x0aaa;
		shift_sel = 0;
		break;
	case CLK_UTIL_VID_PLL_DIV_3:
		shift_val = 0x0db6;
		shift_sel = 0;
		break;
	case CLK_UTIL_VID_PLL_DIV_3p5:
		shift_val = 0x36cc;
		shift_sel = 1;
		break;
	case CLK_UTIL_VID_PLL_DIV_3p75:
		shift_val = 0x6666;
		shift_sel = 2;
		break;
	case CLK_UTIL_VID_PLL_DIV_4:
		shift_val = 0x0ccc;
		shift_sel = 0;
		break;
	case CLK_UTIL_VID_PLL_DIV_5:
		shift_val = 0x739c;
		shift_sel = 2;
		break;
	case CLK_UTIL_VID_PLL_DIV_6:
		shift_val = 0x0e38;
		shift_sel = 0;
		break;
	case CLK_UTIL_VID_PLL_DIV_6p25:
		shift_val = 0x0000;
		shift_sel = 3;
		break;
	case CLK_UTIL_VID_PLL_DIV_7:
		shift_val = 0x3c78;
		shift_sel = 1;
		break;
	case CLK_UTIL_VID_PLL_DIV_7p5:
		shift_val = 0x78f0;
		shift_sel = 2;
		break;
	case CLK_UTIL_VID_PLL_DIV_12:
		shift_val = 0x0fc0;
		shift_sel = 0;
		break;
	case CLK_UTIL_VID_PLL_DIV_14:
		shift_val = 0x3f80;
		shift_sel = 1;
		break;
	case CLK_UTIL_VID_PLL_DIV_15:
		shift_val = 0x7f80;
		shift_sel = 2;
		break;
	default:
		pr_info("invalid vid pll div\n");
		break;
	}

	if (shift_val == 0xffff)	/* if divide by 1 */
		hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 1, 18, 1);
	else {
		unsigned int val;
		val = hd_read_reg(P_HHI_VID_PLL_CLK_DIV);
		val &= ~((0x3 << 16) | (1 << 15) | (0x3fff << 0));
		val |= ((shift_sel << 16) | (1 << 15) | (shift_val << 0));
		hd_write_reg(P_HHI_VID_PLL_CLK_DIV, val);
		/* Set the selector low */
		hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 0, 15, 1);
	}
	/* Enable the final output clock */
	hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 1, 19, 1);
}

/* CRT_VIDEO SETTING FUNCTIONS */
/* input : */
/* vIdx: 0:V1; 1:V2; there have 2 parallel set clock generator: V1 and V2 */
/* inSel : 0:vid_pll_clk; 1:fclk_div4; 2:flck_div3; 3:fclk_div5; */
/* 4:vid_pll2_clk; 5:fclk_div7; 6:vid_pll2_clk; */
/* DivN : clock divider for enci_clk/encp_clk/encl_clk/vda_clk
 * /hdmi_tx_pixel_clk; */
void set_crt_video_enc(uint32_t vIdx, uint32_t inSel, uint32_t DivN)
{
	if (vIdx == 0) {
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 0, 19, 1);

		hd_set_reg_bits(P_HHI_VID_CLK_CNTL, inSel, 16, 3);
		hd_set_reg_bits(P_HHI_VID_CLK_DIV, (DivN-1), 0, 8);

		hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 1, 19, 1);

	} else { /* V2 */
		hd_set_reg_bits(P_HHI_VIID_CLK_CNTL, 0, 19, 1);

		hd_set_reg_bits(P_HHI_VIID_CLK_CNTL, inSel,  16, 3);
		hd_set_reg_bits(P_HHI_VIID_CLK_DIV, (DivN-1), 0, 8);

		hd_set_reg_bits(P_HHI_VIID_CLK_CNTL, 1, 19, 1);
	}
}


/* function: vclk_set_hdmitx */
/* clock setting for HDMI TX */
/* vformat: video format */
/* color_depth: 4 - 24-bit per pixel. Not for 422 video, always choose 4,
 * even if the bit-per-pixel may be 30-bit. */
/* 5 - 30-bit per pixel */
/* mode420: 0 - Non-420 mode; */
/* 1 - HDMI output 420 video, hdmi_phy_clk and tmds_clk are half of the
 * usual speed.
 */
void vclk_set_hdmitx(uint32_t color_depth, uint32_t mode420)
{
	uint32_t hdmi_clk_out;
	uint32_t hdmi_vx1_clk_od1;
	uint32_t vid_pll_clk_div;
	uint32_t xd;

	if ((color_depth) != 4 && (color_depth != 5)) {
		pr_info("Error: vclk_set_hdmitx sees illegal color_depth=%d!\n",
			color_depth);
		return;
	}

	if ((mode420) != 0 && (mode420 != 1)) {
		pr_info("Error: vclk_set_hdmitx sees illegal mode420=%d!\n",
			mode420);
		return;
	}

	hdmi_clk_out = (color_depth == 5) ? (mode420 ? 928.125 : 1856.25) :
		(mode420 ? 742.5 : 1485);
	hdmi_vx1_clk_od1 = (color_depth == 5) ? (mode420 ? (1>>1) : (2>>1)) :
		(mode420 ? (1>>1) : (2>>1));
	vid_pll_clk_div = (color_depth == 5) ? CLK_UTIL_VID_PLL_DIV_6p25 :
		CLK_UTIL_VID_PLL_DIV_5;
	xd = (color_depth == 5) ? 1 : 1;
	/* configure vid_clk_div_top */
	clocks_set_vid_clk_div(vid_pll_clk_div);

	/* configure crt_video */
	set_crt_video_enc(0, 0, xd);
}   /* vclk_set_hdmitx */

/* Enable CLK_ENCP */
void enable_crt_video_encp(uint32_t enable, uint32_t inSel)
{
	hd_set_reg_bits(P_HHI_VID_CLK_DIV, inSel, 24, 4);
	if (inSel <= 4)
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 1, inSel, 1);
	else
		hd_set_reg_bits(P_HHI_VIID_CLK_CNTL, 1, (inSel-5), 1);

	hd_set_reg_bits(P_HHI_VID_CLK_CNTL2, enable, 2, 1);
}

/* Enable HDMI_TX_PIXEL_CLK */
/* Note: when inSel == 15, select tcon_clko */
void enable_crt_video_hdmi(uint32_t enable, uint32_t inSel)
{
	hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, inSel, 16, 4);
	if (inSel <= 4)
		hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 1, inSel, 1);
	else if (inSel <= 9)
		hd_set_reg_bits(P_HHI_VIID_CLK_CNTL, 1, (inSel-5), 1);
	/* no gated for tcon_clko */

	hd_set_reg_bits(P_HHI_VID_CLK_CNTL2, enable, 5, 1);
}

static void set_enc_clk(void)
{
	/* Set pixel_clk and tmds_clk */
	vclk_set_hdmitx(4, 0);
	enable_crt_video_encp(1, 0);
	enable_crt_video_hdmi(1, 0);
}

/* Fixed 1080p */
static void config_tv_enc(void)
{
	hd_write_reg(P_ENCP_VIDEO_FILT_CTRL, 0x1052);
	hd_write_reg(P_VENC_DVI_SETTING, 0x0001);
	hd_write_reg(P_ENCP_VIDEO_MODE, 0x4040);
	hd_write_reg(P_ENCP_VIDEO_MODE_ADV, 0x0018);
	hd_write_reg(P_ENCP_VIDEO_YFP1_HTIME, 140);
	hd_write_reg(P_ENCP_VIDEO_YFP2_HTIME, 2060);
	hd_write_reg(P_ENCP_VIDEO_MAX_PXCNT, 2199);
	hd_write_reg(P_ENCP_VIDEO_HSPULS_BEGIN, 2156);
	hd_write_reg(P_ENCP_VIDEO_HSPULS_END, 44);
	hd_write_reg(P_ENCP_VIDEO_HSPULS_SWITCH, 44);
	hd_write_reg(P_ENCP_VIDEO_VSPULS_BEGIN, 140);
	hd_write_reg(P_ENCP_VIDEO_VSPULS_END, 2059);
	hd_write_reg(P_ENCP_VIDEO_VSPULS_BLINE, 0);
	hd_write_reg(P_ENCP_VIDEO_VSPULS_ELINE, 4);
	hd_write_reg(P_ENCP_VIDEO_HAVON_BEGIN, 148);
	hd_write_reg(P_ENCP_VIDEO_HAVON_END, 2067);
	hd_write_reg(P_ENCP_VIDEO_VAVON_BLINE, 41);
	hd_write_reg(P_ENCP_VIDEO_VAVON_ELINE, 1120);
	hd_write_reg(P_ENCP_VIDEO_HSO_BEGIN, 44);
	hd_write_reg(P_ENCP_VIDEO_HSO_END, 2156);
	hd_write_reg(P_ENCP_VIDEO_VSO_BEGIN, 2100);
	hd_write_reg(P_ENCP_VIDEO_VSO_END, 2164);
	hd_write_reg(P_ENCP_VIDEO_VSO_BLINE, 0);
	hd_write_reg(P_ENCP_VIDEO_VSO_ELINE, 5);
	hd_write_reg(P_ENCP_VIDEO_MAX_LNCNT, 1124);
	hd_write_reg(P_VPU_VIU_VENC_MUX_CTRL, 0x000a);
	hd_write_reg(P_VENC_VIDEO_PROG_MODE, 0x100);
	hd_write_reg(P_ENCI_VIDEO_EN, 0);
	hd_write_reg(P_ENCP_VIDEO_EN, 1);

} /* config_tv_enc */

static void hdmi_tvenc_set_1080p(void)
{
	unsigned long VFIFO2VD_TO_HDMI_LATENCY = 2;
	unsigned long TOTAL_PIXELS = 0, PIXEL_REPEAT_HDMI = 0,
		PIXEL_REPEAT_VENC = 0, ACTIVE_PIXELS = 0;
	unsigned FRONT_PORCH = 0, HSYNC_PIXELS = 0, ACTIVE_LINES = 0,
		INTERLACE_MODE = 0, TOTAL_LINES = 0, SOF_LINES = 0,
		VSYNC_LINES = 0;
	unsigned LINES_F0 = 0, LINES_F1 = 0, BACK_PORCH = 0,
		EOF_LINES = 0, TOTAL_FRAMES = 0;

	unsigned long total_pixels_venc = 0;
	unsigned long active_pixels_venc = 0;
	unsigned long front_porch_venc = 0;
	unsigned long hsync_pixels_venc = 0;

	unsigned long de_h_begin = 0, de_h_end = 0;
	unsigned long de_v_begin_even = 0, de_v_end_even = 0,
		de_v_begin_odd = 0, de_v_end_odd = 0;
	unsigned long hs_begin = 0, hs_end = 0;
	unsigned long vs_adjust = 0;
	unsigned long vs_bline_evn = 0, vs_eline_evn = 0,
		vs_bline_odd = 0, vs_eline_odd = 0;
	unsigned long vso_begin_evn = 0, vso_begin_odd = 0;

		INTERLACE_MODE	= 0;
		PIXEL_REPEAT_VENC  = 0;
		PIXEL_REPEAT_HDMI  = 0;
		ACTIVE_PIXELS = (1920*(1+PIXEL_REPEAT_HDMI));
		ACTIVE_LINES = (1080/(1+INTERLACE_MODE));
		LINES_F0 = 1125;
		LINES_F1 = 1125;
		FRONT_PORCH = 88;
		HSYNC_PIXELS = 44;
		BACK_PORCH = 148;
		EOF_LINES = 4;
		VSYNC_LINES = 5;
		SOF_LINES = 36;
		TOTAL_FRAMES = 4;

	TOTAL_PIXELS = (FRONT_PORCH+HSYNC_PIXELS+BACK_PORCH+ACTIVE_PIXELS);
	TOTAL_LINES = (LINES_F0+(LINES_F1*INTERLACE_MODE));

	total_pixels_venc = (TOTAL_PIXELS  / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	active_pixels_venc = (ACTIVE_PIXELS / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	front_porch_venc = (FRONT_PORCH   / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);
	hsync_pixels_venc = (HSYNC_PIXELS  / (1+PIXEL_REPEAT_HDMI)) *
		(1+PIXEL_REPEAT_VENC);

	hd_write_reg(P_ENCP_VIDEO_MODE, hd_read_reg(P_ENCP_VIDEO_MODE)|(1<<14));
	/* Program DE timing */
	de_h_begin = modulo(hd_read_reg(P_ENCP_VIDEO_HAVON_BEGIN) +
		VFIFO2VD_TO_HDMI_LATENCY,  total_pixels_venc);
	de_h_end  = modulo(de_h_begin + active_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCP_DE_H_BEGIN, de_h_begin);	/* 220 */
	hd_write_reg(P_ENCP_DE_H_END, de_h_end);	 /* 1660 */
	/* Program DE timing for even field */
	de_v_begin_even = hd_read_reg(P_ENCP_VIDEO_VAVON_BLINE);
	de_v_end_even  = de_v_begin_even + ACTIVE_LINES;
	hd_write_reg(P_ENCP_DE_V_BEGIN_EVEN, de_v_begin_even);
	hd_write_reg(P_ENCP_DE_V_END_EVEN,  de_v_end_even);	/* 522 */
	/* Program DE timing for odd field if needed */
	if (INTERLACE_MODE) {
		de_v_begin_odd = to_signed(
			(hd_read_reg(P_ENCP_VIDEO_OFLD_VOAV_OFST)
			& 0xf0)>>4) + de_v_begin_even + (TOTAL_LINES-1)/2;
		de_v_end_odd = de_v_begin_odd + ACTIVE_LINES;
		hd_write_reg(P_ENCP_DE_V_BEGIN_ODD, de_v_begin_odd);
		hd_write_reg(P_ENCP_DE_V_END_ODD, de_v_end_odd);
	}

	/* Program Hsync timing */
	if (de_h_end + front_porch_venc >= total_pixels_venc) {
		hs_begin = de_h_end + front_porch_venc - total_pixels_venc;
		vs_adjust  = 1;
	} else {
		hs_begin = de_h_end + front_porch_venc;
		vs_adjust  = 0;
	}
	hs_end = modulo(hs_begin + hsync_pixels_venc, total_pixels_venc);
	hd_write_reg(P_ENCP_DVI_HSO_BEGIN,  hs_begin);
	hd_write_reg(P_ENCP_DVI_HSO_END, hs_end);

	/* Program Vsync timing for even field */
	if (de_v_begin_even >= SOF_LINES + VSYNC_LINES + (1-vs_adjust))
		vs_bline_evn = de_v_begin_even - SOF_LINES - VSYNC_LINES -
			(1-vs_adjust);
	else
		vs_bline_evn = TOTAL_LINES + de_v_begin_even - SOF_LINES -
			VSYNC_LINES - (1-vs_adjust);
	vs_eline_evn = modulo(vs_bline_evn + VSYNC_LINES, TOTAL_LINES);
	hd_write_reg(P_ENCP_DVI_VSO_BLINE_EVN, vs_bline_evn);   /* 5 */
	hd_write_reg(P_ENCP_DVI_VSO_ELINE_EVN, vs_eline_evn);   /* 11 */
	vso_begin_evn = hs_begin; /* 1692 */
	hd_write_reg(P_ENCP_DVI_VSO_BEGIN_EVN, vso_begin_evn);  /* 1692 */
	hd_write_reg(P_ENCP_DVI_VSO_END_EVN, vso_begin_evn);  /* 1692 */
	/* Program Vsync timing for odd field if needed */
	if (INTERLACE_MODE) {
		vs_bline_odd = de_v_begin_odd-1 - SOF_LINES - VSYNC_LINES;
		vs_eline_odd = de_v_begin_odd-1 - SOF_LINES;
		vso_begin_odd  = modulo(hs_begin + (total_pixels_venc>>1),
			total_pixels_venc);
		hd_write_reg(P_ENCP_DVI_VSO_BLINE_ODD, vs_bline_odd);
		hd_write_reg(P_ENCP_DVI_VSO_ELINE_ODD, vs_eline_odd);
		hd_write_reg(P_ENCP_DVI_VSO_BEGIN_ODD, vso_begin_odd);
		hd_write_reg(P_ENCP_DVI_VSO_END_ODD, vso_begin_odd);
	}

	hd_write_reg(P_VPU_HDMI_SETTING, (0 << 0) |
		(0 << 1) | /* [	1] src_sel_encp */
		(HSYNC_POLARITY << 2) |
		(VSYNC_POLARITY << 3) |
		(0 << 4) |
		(4 << 5) |
		(0 << 8) |
		(0 << 12)
	);
	hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
	hd_set_reg_bits(P_VPU_HDMI_SETTING, 1, 1, 1);
}

static void set_tv_enc_1920x1080p(uint32_t viu1_sel, uint32_t viu2_sel,
	uint32_t enable)
{
	config_tv_enc();
	if (viu1_sel) { /* 1=Connect to ENCP */
		hd_set_reg_bits(P_VPU_VIU_VENC_MUX_CTRL, 2, 0, 2);
	}
	if (viu2_sel) { /* 1=Connect to ENCP */
		hd_set_reg_bits(P_VPU_VIU_VENC_MUX_CTRL, 2, 2, 2);
	}
	hdmi_tvenc_set_1080p();
	hd_write_reg(P_ENCP_VIDEO_EN, 1);
}


void config_hdmi20_tx(uint32_t	base_offset,
	uint8_t	 pixel_repeat,
	uint8_t	 color_depth,
	uint8_t	 input_color_format,
	uint8_t	 input_color_range,
	uint8_t	 output_color_format,
	uint8_t	 output_color_range,
	uint8_t	 vic,
	uint8_t	 interlace,
	uint8_t	 vs_in_pol,
	uint8_t	 hs_in_pol,
	uint32_t active_pixels,
	uint32_t blank_pixels,
	uint32_t active_lines,
	uint32_t blank_lines,
	uint32_t front_porch,
	uint32_t hsync_pixels,
	uint32_t eof_lines,
	uint32_t vsync_lines,
	uint8_t	scrambler_en,
	uint8_t	tmds_clk_div4,
	uint32_t aud_n,
	uint8_t	i2s_spdif,
	uint8_t	i2s_8chan,
	uint8_t	audio_packet_type)
{
	uint32_t	data32;
	uint8_t	 vid_map;
	uint8_t	 csc_en;
	uint8_t	 default_phase   = 0;
	uint8_t	 hdcp14_on	   = (0) ? 1 : 0;
	uint8_t	 csc_limit_en	= 1;
/* 1=3D format that contains active space,
 * e.g. frame-packing; 0=No active space.
 */
	uint8_t	 actspc_hdlr_en  = 0;
/* 1=Active space values that may toggle between 2 values, e.g. interlace. */
	uint8_t	 actspc_hdlr_tgl = 0;

	pr_info("[HDMITX.C] Configure HDMI 2.0 TX -- Begin\n");

	/* Enable clocks and bring out of reset */

	/* Enable hdmitx_sys_clk */
	/* .clk0 ( cts_oscin_clk ), */
	/* .clk1 ( fclk_div4 ), */
	/* .clk2 ( fclk_div3 ), */
	/* .clk3 ( fclk_div5 ), */
	hd_set_reg_bits(P_HHI_HDMI_CLK_CNTL, 0x0100, 0, 16);

	/* Enable clk81_hdmitx_pclk */
	hd_set_reg_bits(P_HHI_GCLK_MPEG2, 1, 4, 1);

	/* wire	wr_enable = control[3]; */
	/* wire	fifo_enable = control[2]; */
	/* assign phy_clk_en = control[1]; */
	/* Enable tmds_clk */
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 1, 1, 1);
	/* Enable the decoupling FIFO */
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 1, 2, 1);
	/* Enable enable the write/read decoupling state machine */
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 1, 3, 1);
	/* Bring HDMITX MEM output of power down */
	hd_set_reg_bits(P_HHI_MEM_PD_REG0, 0, 8, 8);

	/* Bring out of reset */
	hdmitx_wr_reg(HDMITX_TOP_SW_RESET,  0);

	/* Enable internal pixclk, tmds_clk, spdif_clk, i2s_clk, cecclk */
	hdmitx_wr_reg(HDMITX_TOP_CLK_CNTL,  0x0000003f);
	hdmitx_wr_reg(HDMITX_DWC_MC_LOCKONCLOCK, 0xff);

/* But keep spdif_clk and i2s_clk disable
 * until later enable by test.c
 */
	data32  = 0;
	data32 |= (1 << 6);
	data32 |= (0 << 5);
	data32 |= (0 << 4);
	data32 |= (0 << 3);
	data32 |= (0 << 2);
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_MC_CLKDIS, data32);

	/* Enable normal output to PHY */

	if (tmds_clk_div4) {
		data32  = 0;
		data32 |= (0 << 16);
		data32 |= (0 << 0);
		hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_01,  data32);

		data32  = 0;
		data32 |= (0x3ff << 16);
		data32 |= (0x3ff << 0);
		hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_23,  data32);

		data32  = 0;
		data32 |= (0 << 1);
		data32 |= (1 << 0);
		hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_CNTL, data32);
	}

	data32  = 0;
	data32 |= (1 << 12);
	data32 |= (0 << 8);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_TOP_BIST_CNTL, data32);

	if (tmds_clk_div4) {
		data32  = 0;
		data32 |= (1 << 1);
		data32 |= (0 << 0);
		hdmitx_wr_reg(HDMITX_TOP_TMDS_CLK_PTTN_CNTL, data32);
	}

	/* Configure video */

	/* Configure video sampler */

	vid_map = (0) ? ((1) ? 0x01 : (0) ? 0x03 : (0) ? 0x05 : 0x07) : (1)
		? ((1) ? 0x09 : (0) ? 0x0b : (0) ? 0x0d : 0x0f) : (0) ? ((1)
		? 0x09	: (0) ? 0x0b : (0) ? 0x0d : 0x0f) : ((1) ? 0x16	:
		(0) ? 0x14 : 0x12);
	data32  = 0;
	data32 |= (0 << 7);
	data32 |= (vid_map << 0);
	hdmitx_wr_reg(HDMITX_DWC_TX_INVID0, data32);

	data32  = 0;
	data32 |= (0 << 2);
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_TX_INSTUFFING, data32);
	hdmitx_wr_reg(HDMITX_DWC_TX_GYDATA0, 0x00);
	hdmitx_wr_reg(HDMITX_DWC_TX_GYDATA1, 0x00);
	hdmitx_wr_reg(HDMITX_DWC_TX_RCRDATA0, 0x00);
	hdmitx_wr_reg(HDMITX_DWC_TX_RCRDATA1, 0x00);
	hdmitx_wr_reg(HDMITX_DWC_TX_BCBDATA0, 0x00);
	hdmitx_wr_reg(HDMITX_DWC_TX_BCBDATA1, 0x00);

	/* Configure Color Space Converter */

	csc_en  = ((input_color_format != output_color_format) ||
		   (input_color_range  != output_color_range)) ? 1 : 0;

	data32  = 0;
	data32 |= (csc_en   << 0);
	hdmitx_wr_reg(HDMITX_DWC_MC_FLOWCTRL, data32);

	data32  = 0;
	data32 |= (csc_limit_en << 0);
	data32 |= ((((0) && (1)) ? 2 : 0) << 4);
	data32 |= ((((1) && (0)) ? 2 : 0) << 0);
	hdmitx_wr_reg(HDMITX_DWC_CSC_CFG, data32);

	if (0)
		hdmitx_csc_config(0, 0, 0);
/* hdmitx_csc_config(base_offset, input_color_format,
 * output_color_format, color_depth, csc_limit_en); */

	/* Configure video packetizer */

	/* Video Packet color depth and pixel repetition */
	data32  = 0;
	data32 |= (((0) ? 4 : color_depth) << 4);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_VP_PR_CD,  data32);

	/* Video Packet Stuffing */
	data32  = 0;
	data32 |= (default_phase << 5);
	data32 |= (0 << 2);
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_VP_STUFF,  data32);

	/* Video Packet YCC color remapping */
	data32  = 0;
	data32 |= (((0) ? 1 :
		(0) ? 2 : 0)   << 0);
	hdmitx_wr_reg(HDMITX_DWC_VP_REMAP,  data32);

	/* Video Packet configuration */
	data32  = 0;
	data32 |= ((((1) &&
		 (1)) ? 1 : 0) << 6);
	data32 |= ((((0) ||
		 (1)) ? 0 : 1) << 5);
	data32 |= (0 << 4);  /* [  4] pr_en */
	data32 |= (((0) ? 1 : 0) << 3);
	data32 |= (1 << 2);  /* [  2] pr_bypass_select */
	data32 |= (((0) ? 1 :
		(1) ? 2 : 0)  << 0);
	hdmitx_wr_reg(HDMITX_DWC_VP_CONF, data32);

	data32  = 0;
	data32 |= (1 << 7);
	data32 |= (1 << 6);
	data32 |= (1 << 5);
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_VP_MASK, data32);

	/* Configure audio */
#if 0
	/* I2S Sampler config */

	data32  = 0;
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	hdmitx_wr_reg(HDMITX_DWC_AUD_INT, data32);

	data32  = 0;
	data32 |= (1 << 4);
	hdmitx_wr_reg(HDMITX_DWC_AUD_INT1,  data32);

	hdmitx_wr_reg(HDMITX_DWC_FC_MULTISTREAM_CTRL,
		(audio_packet_type == HDMI_AUDIO_PACKET_MUL)?1:0);

/* if enable it now, fifo_overrun will happen, because packet don't get
 * sent out until initial DE detected.
 */
	data32  = 0;
	data32 |= (0 << 7);
	data32 |= (i2s_spdif << 5);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_AUD_CONF0, data32);

	data32  = 0;
	data32 |= (0 << 5);
	data32 |= (24   << 0);
	hdmitx_wr_reg(HDMITX_DWC_AUD_CONF1, data32);

	data32  = 0;
	data32 |= (0 << 1);
	data32 |= (((audio_packet_type == HDMI_AUDIO_PACKET_HBR) ? 1:0) << 0);
	hdmitx_wr_reg(HDMITX_DWC_AUD_CONF2, data32);

	/* spdif sampler config */

	data32  = 0;
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIFINT,  data32);

	data32  = 0;
	data32 |= (0 << 4);
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIFINT1, data32);

	data32  = 0;
	data32 |= (0 << 7);
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIF0,	data32);

	data32  = 0;
	data32 |= (0 << 7);
	data32 |= (((audio_packet_type == HDMI_AUDIO_PACKET_HBR) ? 1:0) << 6);
	data32 |= (24 << 0);
	hdmitx_wr_reg(HDMITX_DWC_AUD_SPDIF1,	data32);

	/* ACR packet configuration */

	data32  = 0;
	data32 |= (1 << 7);  /* [  7] ncts_atomic_write */
	data32 |= (0 << 0);  /* [3:0] AudN[19:16] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_N3, data32);

	data32  = 0;
	data32 |= (0 << 7);  /* [7:5] N_shift */
	data32 |= (0 << 4);  /* [  4] CTS_manual */
	data32 |= (0 << 0);  /* [3:0] manual AudCTS[19:16] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_CTS3, data32);

	hdmitx_wr_reg(HDMITX_DWC_AUD_CTS2, 0); /* manual AudCTS[15:8] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_CTS1, 0); /* manual AudCTS[7:0] */

	data32  = 0;
	data32 |= (1 << 7);  /* [  7] ncts_atomic_write */
	data32 |= (((aud_n>>16)&0xf) << 0);  /* [3:0] AudN[19:16] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_N3, data32);
	hdmitx_wr_reg(HDMITX_DWC_AUD_N2, (aud_n>>8)&0xff);   /* AudN[15:8] */
	hdmitx_wr_reg(HDMITX_DWC_AUD_N1, aud_n&0xff);		/* AudN[7:0] */

	/* audio packetizer config */
	hdmitx_wr_reg(HDMITX_DWC_AUD_INPUTCLKFS, i2s_spdif ? 4 : 0);
#endif
	/* Frame Composer configuration */

	/* Video definitions, as per output video(for packet gen/schedulling) */

	data32  = 0;
	data32 |= (1 << 7);
	data32 |= (vs_in_pol << 6);
	data32 |= (hs_in_pol << 5);
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (interlace << 1);
	data32 |= (interlace << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_INVIDCONF,  data32);

	data32  = active_pixels&0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_INHACTV0, data32);
	data32  = (active_pixels>>8)&0x3f;
	hdmitx_wr_reg(HDMITX_DWC_FC_INHACTV1, data32);

	data32  = blank_pixels&0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_INHBLANK0,  data32);
	data32  = (blank_pixels>>8)&0x1f;
	hdmitx_wr_reg(HDMITX_DWC_FC_INHBLANK1,  data32);

	data32  = active_lines&0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_INVACTV0, data32);
	data32  = (active_lines>>8)&0x1f;
	hdmitx_wr_reg(HDMITX_DWC_FC_INVACTV1, data32);

	data32  = blank_lines&0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_INVBLANK, data32);

	data32  = front_porch&0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINDELAY0,  data32);
	data32  = (front_porch>>8)&0x1f;
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINDELAY1,  data32);

	data32  = hsync_pixels&0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINWIDTH0,  data32);
	data32  = (hsync_pixels>>8)&0x3;
	hdmitx_wr_reg(HDMITX_DWC_FC_HSYNCINWIDTH1,  data32);

	data32  = eof_lines&0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_VSYNCINDELAY, data32);

	data32  = vsync_lines&0x3f;
	hdmitx_wr_reg(HDMITX_DWC_FC_VSYNCINWIDTH, data32);

	/* control period duration (typ 12 tmds periods) */
	hdmitx_wr_reg(HDMITX_DWC_FC_CTRLDUR,	12);
	/* extended control period duration (typ 32 tmds periods) */
	hdmitx_wr_reg(HDMITX_DWC_FC_EXCTRLDUR,  32);
	/* max interval betwen extended control period duration (typ 50) */
	hdmitx_wr_reg(HDMITX_DWC_FC_EXCTRLSPAC, 1);	 /* ?? */
	/* preamble filler */
	hdmitx_wr_reg(HDMITX_DWC_FC_CH0PREAM, 0x0b);
	hdmitx_wr_reg(HDMITX_DWC_FC_CH1PREAM, 0x16);
	hdmitx_wr_reg(HDMITX_DWC_FC_CH2PREAM, 0x21);

	/* write GCP packet configuration */
	data32  = 0;
	data32 |= (default_phase << 2);
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_GCP,	data32);

	/* write AVI Infoframe packet configuration */

	data32  = 0;
	data32 |= (((output_color_format>>2)&0x1) << 7);
	data32 |= (1 << 6);
	data32 |= (0 << 4);
	data32 |= (0 << 2);
	data32 |= ((output_color_format&0x3) << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AVICONF0, data32);

	data32  = 0;
	data32 |= (0 << 6);
	data32 |= (0 << 4);
	data32 |= (8 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AVICONF1, data32);

	data32  = 0;
	data32 |= (0 << 7);
	data32 |= (0 << 4);
	data32 |= (0 << 2);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AVICONF2, data32);

	data32  = 0;
	data32 |= (((1)?1:0) << 2);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AVICONF3, data32);

	hdmitx_wr_reg(HDMITX_DWC_FC_AVIVID, vic);

	/* write Audio Infoframe packet configuration */

	data32  = 0;
	data32 |= ((i2s_8chan ? 7:1) << 4);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF0,  data32);

	data32  = 0;
	data32 |= (3 << 4);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF1, data32);

	hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF2, 0x13);

	data32  = 0;
	data32 |= (1 << 5);
	data32 |= (0 << 4);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDICONF3,  data32);

	/* write audio packet configuration */
	data32  = 0;
	data32 |= (0 << 4);
	data32 |= (i2s_8chan << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCONF, data32);

/* the audio setting bellow are only used for I2S audio IEC60958-3 frame
 * insertion
 */
	data32  = 0;
	data32 |= (i2s_8chan << 7);
	data32 |= (i2s_8chan << 6);
	data32 |= (i2s_8chan << 5);
	data32 |= (1 << 4);
	data32 |= (i2s_8chan << 3);
	data32 |= (i2s_8chan << 2);
	data32 |= (i2s_8chan << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSV,  data32);

	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSU,  0);

	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS0, 0x01);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS1, 0x23);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS2, 0x45);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS3, 0x67);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS4, 0x89);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS5, 0xab);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS6, 0xcd);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS7, 0x2f);
	hdmitx_wr_reg(HDMITX_DWC_FC_AUDSCHNLS8, 0xf0);

	/* packet queue priority (auto mode) */
	hdmitx_wr_reg(HDMITX_DWC_FC_CTRLQHIGH,  15);
	hdmitx_wr_reg(HDMITX_DWC_FC_CTRLQLOW, 3);

	/* packet scheduller configuration for SPD, VSD, ISRC1/2, ACP. */
	data32  = 0;
	data32 |= (0 << 4);
	data32 |= (0 << 3);
	data32 |= (0 << 2);
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_DATAUTO0, data32);
	hdmitx_wr_reg(HDMITX_DWC_FC_DATAUTO1, 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_DATAUTO2, 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_DATMAN, 0);

	/* packet scheduller configuration for AVI, GCP, AUDI, ACR. */
	data32  = 0;
	data32 |= (0 << 5);
	data32 |= (0 << 4);
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_DATAUTO3, data32);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB0,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB1,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB2,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB3,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB4,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB5,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB6,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB7,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB8,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB9,  0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB10, 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_RDRB11, 0);

	/* Packet transmission enable */
	data32  = 0;
	data32 |= (0 << 6);
	data32 |= (0 << 5);
	data32 |= (0 << 4);
	data32 |= (0 << 3);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_PACKET_TX_EN, data32);

	/* For 3D video */
	data32  = 0;
	data32 |= (actspc_hdlr_tgl << 1);
	data32 |= (actspc_hdlr_en << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_ACTSPC_HDLR_CFG, data32);

	data32  = active_lines&0xff;
	hdmitx_wr_reg(HDMITX_DWC_FC_INVACT_2D_0,	data32);
	data32  = (active_lines>>8)&0xf;
	hdmitx_wr_reg(HDMITX_DWC_FC_INVACT_2D_1,	data32);

	/* Do not enable these interrupt below, we can check them at RX side. */
	data32  = 0;
	data32 |= (1 << 7);
	data32 |= (1 << 6);
	data32 |= (1 << 5);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_MASK0,  data32);

	data32  = 0;
	data32 |= (1 << 7);
	data32 |= (1 << 6);
	data32 |= (1 << 5);
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_MASK1,  data32);

	data32  = 0;
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_MASK2,  data32);

	/* Pixel repetition ratio the input and output video */
	data32  = 0;
	data32 |= ((pixel_repeat+1) << 4);
	data32 |= (pixel_repeat << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_PRCONF, data32);

	/* Scrambler control */
	data32  = 0;
	data32 |= (0 << 4);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_FC_SCRAMBLER_CTRL, data32);

	/* Configure E-DDC interface */
	data32  = 0;
	data32 |= (1 << 24);
	data32 |= (0 << 16);
	data32 |= (0 << 8);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_TOP_INFILTER,  data32);

	data32  = 0;
	data32 |= (0 << 6);
	data32 |= (0 << 2);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_INT,  data32);

	data32  = 0;
	data32 |= (0 << 6);
	data32 |= (0 << 2);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_CTLINT, data32);

	data32  = 0;
	data32 |= (1 << 3);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_DIV,  data32);

/*
 * I2CM_SS_SCL_HCNT = RndUp(min_ss_scl_htime*Freq(sfrclkInMHz)/1000)
 * I2CM_SS_SCL_LCNT = RndUp(min_ss_scl_ltime*Freq(sfrclkInMHz)/1000)
 * I2CM_FS_SCL_HCNT = RndUp(min_fs_scl_htime*Freq(sfrclkInMHz)/1000)
 * I2CM_FS_SCL_LCNT = RndUp(min_fs_scl_ltime*Freq(sfrclkInMHz)/1000)
 * Where Freq(sfrclkInMHz)=24;
 * min_ss_scl_htime = 4000ns (Spec Value);
 *     4500ns (IP databook Compliance value); 5000ns (IP testbench value).
 * min_ss_scl_ltime = 4700ns (Spec Value);
 *     5200ns (IP databook Compliance value); 5000ns (IP testbench value).
 * min_fs_scl_htime =  600ns (Spec Value);
 *     N/A (IP databook Compliance value); 1100ns (IP testbench value).
 * min_fs_scl_ltime = 1300ns (Spec Value);
 *     N/A (IP databook Compliance value); 1400ns (IP testbench value).
 * Here we use Spec Values
 */
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SS_SCL_HCNT_1,	0);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SS_SCL_HCNT_0,	0x60);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SS_SCL_LCNT_1,	0);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SS_SCL_LCNT_0,	0x71);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_FS_SCL_HCNT_1,	0);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_FS_SCL_HCNT_0,	0x0f);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_FS_SCL_LCNT_1,	0);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_FS_SCL_LCNT_0,	0x20);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SDA_HOLD,		 0x08);

	data32  = 0;
	data32 |= (0 << 5);
	data32 |= (1 << 4);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SCDC_UPDATE,  data32);

	/* Configure HDCP */
	data32  = 0;
	data32 |= (0 << 7);
	data32 |= (0 << 6);
	data32 |= (0 << 4);
	data32 |= (0 << 3);
	data32 |= (0 << 2);
	data32 |= (0 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_A_APIINTMSK, data32);

	data32  = 0;
	data32 |= (0 << 5);
	data32 |= (1 << 4);
	data32 |= (vs_in_pol << 3);
	data32 |= (hs_in_pol << 1);
	hdmitx_wr_reg(HDMITX_DWC_A_VIDPOLCFG, data32);
	hdmitx_wr_reg(HDMITX_DWC_A_OESSWCFG, 0x40);

	data32  = 0;
	data32 |= (0 << 3);
	data32 |= (0 << 2);
	data32 |= (0 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_A_KSVMEMCTRL, data32);
	hdmitx_poll_reg(HDMITX_DWC_A_KSVMEMCTRL, (1<<1), ~(1<<1));
	hdmitx_wr_reg(HDMITX_DWC_HDCP_REVOC_SIZE_0, 0);
	hdmitx_wr_reg(HDMITX_DWC_HDCP_REVOC_SIZE_1, 0);
	data32  = 0;
	data32 |= (0 << 3);
	data32 |= (0 << 2);
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_A_KSVMEMCTRL, data32);

	data32  = 0;
	data32 |= (0 << 4);
	data32 |= (0 << 3);
	data32 |= (1 << 2);
	data32 |= ((hdcp14_on?0:1)  << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_A_HDCPCFG1,	data32);

	/* Interrupts */
	/* Clear interrupts */
	hdmitx_wr_reg(HDMITX_DWC_IH_FC_STAT0,  0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_FC_STAT1,  0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_FC_STAT2,  0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_AS_STAT0,  0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_PHY_STAT0, 0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0,	0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_CEC_STAT0, 0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_VP_STAT0,  0xff);
	hdmitx_wr_reg(HDMITX_DWC_IH_I2CMPHY_STAT0, 0xff);
	hdmitx_wr_reg(HDMITX_DWC_A_APIINTCLR,  0xff);
	hdmitx_wr_reg(HDMITX_DWC_HDCP22REG_STAT, 0xff);

	hdmitx_wr_reg(HDMITX_TOP_INTR_STAT_CLR,	0x0000001f);

	/* Selectively enable/mute interrupt sources */
	data32  = 0;
	data32 |= (1 << 7);
	data32 |= (1 << 6);
	data32 |= (1 << 5);
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_FC_STAT0,  data32);

	data32  = 0;
	data32 |= (1 << 7);
	data32 |= (1 << 6);
	data32 |= (1 << 5);
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_FC_STAT1,  data32);

	data32  = 0;
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_FC_STAT2,  data32);

	data32  = 0;
	data32 |= (0 << 4);
	data32 |= (0 << 3);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_AS_STAT0,  data32);

	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_PHY_STAT0, 0x3f);

	data32  = 0;
	data32 |= (0 << 2);
	data32 |= (1 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_I2CM_STAT0, data32);

	data32  = 0;
	data32 |= (0 << 6);
	data32 |= (0 << 5);
	data32 |= (0 << 4);
	data32 |= (0 << 3);
	data32 |= (0 << 2);
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_CEC_STAT0, data32);

	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_VP_STAT0,  0xff);

	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE_I2CMPHY_STAT0, 0x03);

	data32  = 0;
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_IH_MUTE, data32);

	data32  = 0;
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_TOP_INTR_MASKN, data32);

	/* Reset pulse */
	hdmitx_rd_check_reg(HDMITX_DWC_MC_LOCKONCLOCK, 0xff, 0x9f);

	data32  = 0;
	data32 |= (0 << 7);
	data32 |= (0 << 6);
	data32 |= (0 << 4);
	data32 |= (0 << 3);
	data32 |= (0 << 2);
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmitx_wr_reg(HDMITX_DWC_MC_SWRSTZREQ, data32);
	mdelay(10);

	data32  = 0;
	data32 |= (1 << 7);
	data32 |= (1 << 6);
	data32 |= (1 << 4);
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	data32 |= (0 << 1);
	data32 |= (1 << 0);
	hdmitx_wr_reg(HDMITX_DWC_MC_SWRSTZREQ, data32);

	pr_info("[HDMITX.C] Configure HDMI 2.0 TX -- End\n");
} /* config_hdmi20_tx */

static void set_fixed_1080p_hpll(void)
{
	hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x5800023d);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00444e00);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL3, 0x0d5c5091);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL4, 0x801da72c);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL5, 0x71486980);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL6, 0x00000e55);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x5800023d);
	hd_write_reg(P_HHI_HDMI_PLL_CNTL, 0x4800023d);
	mdelay(10);
	pr_info("hpll reg: 0x%x\n", hd_read_reg(P_HHI_HDMI_PLL_CNTL));
}

static void set_fixed_1080p_phy(void)
{
	hd_write_reg(P_HHI_HDMI_PHY_CNTL0, 0x33b5426b);
	hd_write_reg(P_HHI_HDMI_PHY_CNTL3, 0x01fe115b);
#define RESET_HDMI_PHY() \
do { \
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0xf, 0, 4); \
	mdelay(2); \
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0xe, 0, 4); \
	mdelay(2); \
} while (0)

	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0x0390, 16, 16);
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0x1, 17, 1);
	hd_set_reg_bits(P_HHI_HDMI_PHY_CNTL1, 0x0, 0, 4);
	RESET_HDMI_PHY();
	RESET_HDMI_PHY();
	RESET_HDMI_PHY();
#undef RESET_HDMI_PHY
}

static void tmp_generate_1080p(void)
{
/*
 * clocks_set_sys_defaults()
 */
	/* HDMI (90Mhz) */
	/* .clk0 ( cts_oscin_clk ), */
	/* .clk1 ( fclk_div4 ), */
	/* .clk2 ( fclk_div3 ), */
	/* .clk3 ( fclk_div5 ), */
	hd_write_reg(P_HHI_HDMI_CLK_CNTL,
		((2 << 9) | /* select "fclk_div3" PLL */
		(1 << 8)  | /* Enable gated clock */
		(6 << 0))); /* Divide by 7 */
	/* ----------------------------------------------------- */
	/* 6Ghz HDMI_VX1_PLL <<<<<<< MUST DO THIS >>>>>>>>>> */
	/* ----------------------------------------------------- */
	/* The HDMI PLL has two parts. The first PLL must be set to 96Mhz */
	/* .HDMI_IL_DPLL_EN ( hi_vid2_pll_cntl[0] ), */
	/* .HDMI_IL_DPLL_N ( hi_vid2_pll_cntl[31:27]   ), */
	hd_write_reg(P_HHI_VID2_PLL_CNTL, (1 << 27) | (1 << 0));
	/* .HDMI_IL_DPLL_M  ( hi_vid2_pll_cntl2[16:8]   ), */
	/* .HDMI_IL_DPLL_OD ( hi_vid2_pll_cntl2[18:17]  ), */
	/* .HDMI_IL_DPLL_RESET ( hi_vid2_pll_cntl2[24]	 ), */
	hd_write_reg(P_HHI_VID2_PLL_CNTL2, (0 << 24) | (0 << 17) | (4 << 8));
	hd_write_reg(P_HHI_HDMI_PLL_CNTL2, 0x00820aaa);

    /* disable video clocks */
/* wire            gclk_lcd_an_clk_ph2     = hi_vid_clk_cntl2[7]; */
/* wire            gclk_lcd_an_clk_ph3     = hi_vid_clk_cntl2[6]; */
/* wire            gclk_hdmi_tx_pixel_clk  = hi_vid_clk_cntl2[5]; */
/* wire            gclk_vdac_clk           = hi_vid_clk_cntl2[4]; */
/* wire            gclk_encl_clk           = hi_vid_clk_cntl2[3]; */
/* wire            gclk_encp_clk           = hi_vid_clk_cntl2[2]; */
/* wire            gclk_enct_clk           = hi_vid_clk_cntl2[1]; */
/* wire            gclk_enci_clk           = hi_vid_clk_cntl2[0]; */
	hd_set_reg_bits(P_HHI_VID_CLK_CNTL2, 0, 0, 8);
/* wire            cntl_div12_en       = hi_vid_clk_cntl[4]; */
/* wire            cntl_div6_en        = hi_vid_clk_cntl[3]; */
/* wire            cntl_div4_en        = hi_vid_clk_cntl[2]; */
/* wire            cntl_div2_en        = hi_vid_clk_cntl[1]; */
/* wire            cntl_div1_en        = hi_vid_clk_cntl[0]; */
	hd_set_reg_bits(P_HHI_VID_CLK_CNTL, 0, 0, 5);

/*
 * C_Entry()
 */

	set_fixed_1080p_hpll();
	set_fixed_1080p_phy();
	/* reset hpll div */
	hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 1, 15, 1);
	mdelay(10);
	hd_set_reg_bits(P_HHI_VID_PLL_CLK_DIV, 0, 15, 1);
	hd_write_reg(P_HHI_VID_CLK_DIV, 0x100);

/* Enable Hsync and equalization pulse switch in center; bit[14] cfg_de_v = 1 */
	hd_write_reg(P_ENCP_VIDEO_MODE, 0x0040 | (1<<14));
	hd_write_reg(P_ENCP_VIDEO_MODE_ADV, 0x0008); /* Sampling rate: 1 */
	hd_write_reg(P_ENCP_VIDEO_YFP1_HTIME, 140);
	hd_write_reg(P_ENCP_VIDEO_YFP2_HTIME, 2060);

	set_enc_clk();
	hd_write_reg(P_HHI_VID_CLK_DIV, 0x101);
	set_tv_enc_1920x1080p(1, 0, 0);
	hd_write_reg(P_HHI_VID_CLK_DIV, 0x100);

	config_hdmi20_tx(
/* Int/Ext TX address offset. =0 for internal TX;
 * =0x20 for external TX (sim model).
 */
		   0,
		   0,		/* pixel_repeat, */
/* Pixel bit width: 4=24-bit; 5=30-bit; 6=36-bit; 7=48-bit. */
		     4,
/* input_color_format: 0=RGB444; 1=YCbCr422; 2=YCbCr444; 3=YCbCr420. */
		     2,
		     1,		/* input_color_range: 0=limited; 1=full. */
/* output_color_format: 0=RGB444; 1=YCbCr422; 2=YCbCr444; 3=YCbCr420. */
		     2,
		     1,		/* output_color_range: 0=limited; 1=full. */
		     16,	/* Video format identification code */
		   0,		/* 0=Progressive; 1=Interlaced. */
/* VSync input polarity: 0=active low; 1=active high. */
		     1,
/* HSync input polarity: 0=active low; 1=active high. */
		     1,
		     1920,	/* Number of active pixels per line */
		     280,	/* Number of blank pixels per line */
		     1080,	/* Number of active lines per field */
		     45,	/* Number of blank lines per field */
		     88,	/* Number of pixels from DE end to HS start */
		     44,	/* Number of pixels of HS pulse width */
/* HS count between last active line and start of VS. */
		     4,
		     5,		/* HS count of VS width */
		   0,		/* 0=No scramble; 1=enable scrambler. */
/* 0:TMDS_CLK_rate=TMDS_Character_rate;
 * 1:TMDS_CLK_rate=TMDS_Character_rate/4, for TMDS_Character_rate>340Mcsc.
 */
		   0,
		   0,		/* ACR N */
		   0,		/* 0=SPDIF; 1=I2S. */
		   0,		/* 0=I2S 2-channel; 1=I2S 4 x 2-channel. */
/* 2=audio sample packet; 7=one bit audio;
 * 8=DST audio packet; 9=HBR audio packet.
 */
		   0);

}

/* TODO */
static void hdmitx_csc_config(unsigned char input_color_format,
			unsigned char output_color_format,
			unsigned char color_depth)
{
	unsigned char conv_en;
	unsigned long csc_coeff_a1, csc_coeff_a2, csc_coeff_a3, csc_coeff_a4;
	unsigned long csc_coeff_b1, csc_coeff_b2, csc_coeff_b3, csc_coeff_b4;
	unsigned long csc_coeff_c1, csc_coeff_c2, csc_coeff_c3, csc_coeff_c4;
	unsigned char csc_scale;
	unsigned long data32;

	conv_en = (((input_color_format  == hdmi_color_format_RGB) ||
		(output_color_format == hdmi_color_format_RGB)) &&
		(input_color_format  != output_color_format)) ? 1 : 0;

	if (conv_en) {
		if (output_color_format == hdmi_color_format_RGB) {
			csc_coeff_a1 = 0x2000;
			csc_coeff_a2 = 0x6926;
			csc_coeff_a3 = 0x74fd;
			csc_coeff_a4 = (color_depth == hdmi_color_depth_24B) ?
				0x010e :
			(color_depth == hdmi_color_depth_30B) ? 0x043b :
			(color_depth == hdmi_color_depth_36B) ? 0x10ee :
			(color_depth == hdmi_color_depth_48B) ? 0x10ee : 0x010e;
		csc_coeff_b1 = 0x2000;
		csc_coeff_b2 = 0x2cdd;
		csc_coeff_b3 = 0x0000;
		csc_coeff_b4 = (color_depth == hdmi_color_depth_24B) ? 0x7e9a :
			(color_depth == hdmi_color_depth_30B) ? 0x7a65 :
			(color_depth == hdmi_color_depth_36B) ? 0x6992 :
			(color_depth == hdmi_color_depth_48B) ? 0x6992 : 0x7e9a;
		csc_coeff_c1 = 0x2000;
		csc_coeff_c2 = 0x0000;
		csc_coeff_c3 = 0x38b4;
		csc_coeff_c4 = (color_depth == hdmi_color_depth_24B) ? 0x7e3b :
			(color_depth == hdmi_color_depth_30B) ? 0x78ea :
			(color_depth == hdmi_color_depth_36B) ? 0x63a6 :
			(color_depth == hdmi_color_depth_48B) ? 0x63a6 : 0x7e3b;
		csc_scale = 1;
	} else { /* input_color_format == hdmi_color_format_RGB */
		csc_coeff_a1 = 0x2591;
		csc_coeff_a2 = 0x1322;
		csc_coeff_a3 = 0x074b;
		csc_coeff_a4 = 0x0000;
		csc_coeff_b1 = 0x6535;
		csc_coeff_b2 = 0x2000;
		csc_coeff_b3 = 0x7acc;
		csc_coeff_b4 = (color_depth == hdmi_color_depth_24B) ? 0x0200 :
			(color_depth == hdmi_color_depth_30B) ? 0x0800 :
			(color_depth == hdmi_color_depth_36B) ? 0x2000 :
			(color_depth == hdmi_color_depth_48B) ? 0x2000 : 0x0200;
		csc_coeff_c1 = 0x6acd;
		csc_coeff_c2 = 0x7534;
		csc_coeff_c3 = 0x2000;
		csc_coeff_c4 = (color_depth == hdmi_color_depth_24B) ? 0x0200 :
			(color_depth == hdmi_color_depth_30B) ? 0x0800 :
			(color_depth == hdmi_color_depth_36B) ? 0x2000 :
			(color_depth == hdmi_color_depth_48B) ? 0x2000 : 0x0200;
		csc_scale = 0;
	}
	} else {
		csc_coeff_a1 = 0x2000;
		csc_coeff_a2 = 0x0000;
		csc_coeff_a3 = 0x0000;
		csc_coeff_a4 = 0x0000;
		csc_coeff_b1 = 0x0000;
		csc_coeff_b2 = 0x2000;
		csc_coeff_b3 = 0x0000;
		csc_coeff_b4 = 0x0000;
		csc_coeff_c1 = 0x0000;
		csc_coeff_c2 = 0x0000;
		csc_coeff_c3 = 0x2000;
		csc_coeff_c4 = 0x0000;
		csc_scale = 1;
	}

	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A1_MSB, (csc_coeff_a1>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A1_LSB, csc_coeff_a1&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A2_MSB, (csc_coeff_a2>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A2_LSB, csc_coeff_a2&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A3_MSB, (csc_coeff_a3>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A3_LSB, csc_coeff_a3&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A4_MSB, (csc_coeff_a4>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_A4_LSB, csc_coeff_a4&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B1_MSB, (csc_coeff_b1>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B1_LSB, csc_coeff_b1&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B2_MSB, (csc_coeff_b2>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B2_LSB, csc_coeff_b2&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B3_MSB, (csc_coeff_b3>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B3_LSB, csc_coeff_b3&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B4_MSB, (csc_coeff_b4>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_B4_LSB, csc_coeff_b4&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C1_MSB, (csc_coeff_c1>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C1_LSB, csc_coeff_c1&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C2_MSB, (csc_coeff_c2>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C2_LSB, csc_coeff_c2&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C3_MSB, (csc_coeff_c3>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C3_LSB, csc_coeff_c3&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C4_MSB, (csc_coeff_c4>>8)&0xff);
	hdmitx_wr_reg(HDMITX_DWC_CSC_COEF_C4_LSB, csc_coeff_c4&0xff);

	data32 = 0;
	data32 |= (color_depth  << 4);  /* [7:4] csc_color_depth */
	data32 |= (csc_scale << 0);  /* [1:0] cscscale */
	hdmitx_wr_reg(HDMITX_DWC_CSC_SCALE, data32);
}   /* hdmitx_csc_config */

static void C_Entry(enum hdmi_vic vic)
{
	struct hdmi_format_para *para = hdmi_get_fmt_paras(vic);
	struct hdmi_cea_timing *t = NULL;

	if (para == NULL) {
		pr_info("error at %s[%d] vic = %d\n", __func__, __LINE__, vic);
		return;
	}
	pr_info("%s[%d] set VIC = %d\n", __func__, __LINE__, para->vic);
	t = &para->timing;

	/* -------------------------------------------------------- */
	/* Set TV encoder for HDMI */
	/* -------------------------------------------------------- */
	pr_info("Configure VENC\n");

/* Configure video format timing for HDMI:
 * Based on the corresponding settings in set_tv_enc.c, calculate
 * the register values to meet the timing requirements defined in CEA-861-D
 */
	pr_info("Configure HDMI video format timing\n");

	/* -------------------------------------------------------- */
	/* Set up HDMI */
	/* -------------------------------------------------------- */
#if 0
	config_hdmi20_tx(vic, para,
			TX_COLOR_DEPTH,
			TX_INPUT_COLOR_FORMAT,
			TX_INPUT_COLOR_RANGE,
			TX_OUTPUT_COLOR_FORMAT,
			TX_OUTPUT_COLOR_RANGE
			);
#endif
	return;
}
