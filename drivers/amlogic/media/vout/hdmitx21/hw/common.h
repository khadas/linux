/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMITX_HW_COMMON_H__
#define __HDMITX_HW_COMMON_H__

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_ddc.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_common.h>
#include "../hdmi_tx.h"
#include "hdmi_tx_reg.h"
#include "register.h"

enum map_addr_idx_e {
	VPUCTRL_REG_IDX,
	HDMITX_COR_REG_IDX,
	HDMITX_TOP_REG_IDX,
	SYSCTRL_REG_IDX,
	PWRCTRL_REG_IDX,
	ANACTRL_REG_IDX,
	RESETCTRL_REG_IDX,
	CLKCTRL_REG_IDX,
	PADCTRL_REG_IDX,
	REG_IDX_END
};

struct reg_map {
	u32 phy_addr;
	u32 size;
	void __iomem *p;
};

struct reg_s {
	u32 reg;
	u32 val;
};

extern struct reg_map reg21_maps[REG_IDX_END];

u32 TO21_PHY_ADDR(u32 addr);
void __iomem *TO21_PMAP_ADDR(u32 addr);

u32 hd21_read_reg(u32 addr);
void hd21_write_reg(u32 addr, u32 val);
void hd21_set_reg_bits(u32 addr, u32 value,
		u32 offset, u32 len);
void init_reg_map(u32 type);
u32 hdmitx21_rd_reg(u32 addr);
void hdmitx21_wr_reg(u32 addr, u32 val);
void hdmitx21_poll_reg(u32 addr, u8 exp_data, u8 mask, ulong timeout);
void hdmitx21_set_reg_bits(u32 addr, u32 value, u32 offset, u32 len);

enum tvenc_type {
	TV_ENC_160p = 0,
	TV_ENC_234,
	TV_ENC_234_2,
	TV_ENC_480i,
	TV_ENC_480p,
	TV_ENCP_480p,
	TV_ENCI_480i,
	TV_ENC_576i,
	TV_ENC_576p,
	TV_ENC_600p,
	TV_ENC_720p,
	TV_ENC_1080i,
	TV_ENC_1080p,
	TV_ENC_1280p,
	TV_ENC_2048x1536,
	TV_ENC_2205p,
	TV_ENC_2440p,
	TV_ENC_3840x2160p_vic01,
	TV_ENC_3840x2160p_vic03,
	TV_ENC_4096x2160p_vic04,
	TV_ENC_4096x2160p_vic102,
	TV_ENC_4096x540p_240hz,
	TV_ENC_640x480p,
	TV_ENC_320p,
	TV_ENC_512p,
	TV_ENC_TYPE_MAX
};   /* tv encoder output format */

#define VID_PLL_DIV_1 0
#define VID_PLL_DIV_2      1
#define VID_PLL_DIV_3      2
#define VID_PLL_DIV_3p5    3
#define VID_PLL_DIV_3p75   4
#define VID_PLL_DIV_4      5
#define VID_PLL_DIV_5      6
#define VID_PLL_DIV_6      7
#define VID_PLL_DIV_6p25   8
#define VID_PLL_DIV_7      9
#define VID_PLL_DIV_7p5    10
#define VID_PLL_DIV_12     11
#define VID_PLL_DIV_14     12
#define VID_PLL_DIV_15     13
#define VID_PLL_DIV_2p5    14
#define VID_PLL_DIV_3p25   15

#define GROUP_MAX	8
struct hw_enc_clk_val_group {
	enum hdmi_vic group[GROUP_MAX];
	u32 hpll_clk_out; /* Unit: kHz */
	u32 od1;
	u32 od2; /* HDMI_CLK_TODIG */
	u32 od3;
	u32 vid_pll_div;
	u32 vid_clk_div;
	u32 hdmi_tx_pixel_div;
	u32 encp_div;
	u32 enci_div;
};

void hdmitx21_set_clk(struct hdmitx_dev *hdev);
void hdmitx21_set_cts_sys_clk(struct hdmitx_dev *hdev);
void hdmitx21_set_top_pclk(struct hdmitx_dev *hdev);
void hdmitx21_set_hdcp_pclk(struct hdmitx_dev *hdev);
void hdmitx21_set_cts_hdcp22_clk(struct hdmitx_dev *hdev);
void hdmitx21_set_sys_clk(struct hdmitx_dev *hdev, u8 flag);
void vclk_set_hdmitx(u32 vformat, u32 color_depth, u32 mode420);

void set_tv_encp_new(u32 enc_index, enum tvenc_type output_type, u32 enable);
void adjust_encp_for_hdmi(u8 enc_index, u8 interlace_mode, u32 total_pixels_venc,
	u32 active_pixels_venc, u32 front_porch_venc, u32 hsync_pixels_venc,
	u32 total_lines, u32 active_lines, u32 sof_lines, u32 vsync_lines,
	u32 vs_adjust_420);

/***********************************************************************
 *                   hdmi debug printk
 * pr_info(EDID "edid bad\");
 * pr_debug(AUD "set audio format: AC-3\n");
 * pr_err(REG "write reg\n")
 **********************************************************************/
#undef pr_fmt
#define pr_fmt(fmt) "hdmitx: " fmt

#define VID         "video: "
#define AUD         "audio: "
#define CEC         "cec: "
#define EDID        "edid: "
#define HDCP        "hdcp: "
#define SYS         "system: "
#define HPD         "hpd: "
#define HW          "hw: "
#define REG         "reg: "

void hdmitx21_phy_bandgap_en_t7(void);

void set21_phy_by_mode_t7(u32 mode);

void hdmitx21_sys_reset_t7(void);

void set21_t7_hpll_clk_out(u32 frac_rate, u32 clk);
void set21_hpll_od1_t7(u32 div);
void set21_hpll_od2_t7(u32 div);
void set21_hpll_od3_t7(u32 div);

void set21_hpll_sspll_t7(enum hdmi_vic vic);

void dump_hdmitx_reg(void);

#endif
