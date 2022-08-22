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
/* sequence read registers */
void hdmitx21_seq_rd_reg(u16 offset, u8 *buf, u16 cnt);
void hdmitx21_fifo_read(u16 offset, u8 *buf, u16 cnt);
void hdmitx21_wr_reg(u32 addr, u32 val);
void hdmitx21_poll_reg(u32 addr, u8 exp_data, u8 mask, ulong timeout);
/* set the value in length from offset */
void hdmitx21_set_reg_bits(u32 addr, u32 value, u32 offset, u32 len);
/* set the bits value */
void hdmitx21_set_bit(u32 addr, u32 bit_val, bool st);

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
#define VID_PLL_DIV_10     16

#define GROUP_MAX	10
struct hw_enc_clk_val_group {
	enum hdmi_vic group[GROUP_MAX];
	u32 txpll_vco_clk; /* 3G~6G, Unit: kHz */
	u32 txpll_3_od0; /* div 1, 2, 4, 8 */
	u8  txpll_4_div9_en; /* tx_phy_clk1618 */
	u32 txpll_3_od1; /* div 1, 2, 4, 8 */
	u32 txpll_3_od2; /* div 1, 2, 4, 8 */
	u8  txpll_4_od;	/* div 1, 2, 4, 8 */
	u8  txpll_4_mux; /* a_phy_clock */
	u8  hdmi_ctl2_div5_10; /* hdmi_clk_todig, hdmi_ch_clk */
	u8  fpll_vco_clk; /* 1.6G ~ 3.2G, Unit: kHz */
	u8  fpll_tmds_od1; /* div 1, 2, 4, 8 */
	u8  fpll_tmds_od2; /* div 1, 2, 4, 8, then div2 to tmds_clk */
	u8  fpll_pixel_od; /* div 1, 1.25, 1.5, 2, pixel_clk */
	u8  vid_pll_clk_div; /* div 5, 6.25, 7.5, htx_tmds_clk */
	u8  hdmi_pll_tmds_clk_div; /* div 5, 6.25, 7.5, vid_pll0_clk*/
};

void hdmitx21_set_default_clk(void);
void hdmitx21_set_clk(struct hdmitx_dev *hdev);
void hdmitx21_set_cts_sys_clk(struct hdmitx_dev *hdev);
void hdmitx21_set_top_pclk(struct hdmitx_dev *hdev);
void hdmitx21_set_hdcp_pclk(struct hdmitx_dev *hdev);
void hdmitx21_set_cts_hdcp22_clk(struct hdmitx_dev *hdev);

void set_tv_encp_new(struct hdmitx_dev *hdev, u32 enc_index,
	enum hdmi_vic vic, u32 enable);
void set_tv_enci_new(struct hdmitx_dev *hdev, u32 enc_index,
	enum hdmi_vic vic, u32 enable);
void hdmitx21_venc_en(bool en, bool pi_mode);

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
void hdmitx21_phy_bandgap_en_s5(void);

void set21_phy_by_mode_t7(u32 mode);
void set21_phy_by_mode_s5(u32 mode);

void hdmitx21_sys_reset_t7(void);
void hdmitx21_sys_reset_s5(void);

void hdmitx21_debugfs_init(void);

void set21_t7_hpll_clk_out(u32 frac_rate, u32 clk);
void set21_s5_hpll_clk_out(u32 frac_rate, u32 clk);
void set21_hpll_od0_t7(u32 div);
void set21_hpll_od1_t7(u32 div);
void set21_hpll_od2_t7(u32 div);
void set21_txpll_3_od0_s5(u8 od);
void set21_txpll_3_od1_s5(u8 od);
void set21_txpll_3_od2_s5(u8 od);
void set21_txpll_4_od_s5(u8 od);

void set21_hpll_sspll_t7(enum hdmi_vic vic);
void set21_hpll_sspll_s5(enum hdmi_vic vic);

void dump_hdmitx_reg(void);
void dump_infoframe_packets(struct seq_file *s);

#endif
