// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/compiler.h>
#include <linux/arm-smccc.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>

#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/hdmi_tx21/enc_clk_config.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_info_global.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_ddc.h>

#include "common.h"
#include "../hdmi_tx.h"

//scdc:
//0x31: [7:4] FFE_LEVELS; [3:0] FRL_rate
//FRL_Rate = 0: disable FRL
//           1: 3G/3Lanes
//           2: 6G/3Lanes
//           3: 6G/4Lanes
//           4: 8G/4Lanes
//           5: 10G/4Lanes
//           6: 12G/4Lanes
//

#define FRL_RATE_3G_3LANES 1
#define FRL_RATE_6G_3LANES 2
#define FRL_RATE_6G_4LANES 3
#define FRL_RATE_8G_4LANES 4
#define FRL_RATE_10G_4LANES 5
#define FRL_RATE_12G_4LANES 6

//================== LTS 1============================
static void TX_LTS_1_HDMI21_CONFIG(u8 frl_rate)
{
	u8 data8;
	u8 frl_rate_sel;

	//Step1:Tx reads EDID
	//Assume that Rx supports FRL

	//Step2:initial Tx Phy,TODO

	//Step3:initial Tx Controller
	data8  = 0;
	data8 |= (1 << 0); //[7:0] reg_pkt_period
	hdmitx21_wr_reg(H21TXSB_PKT_PRD_IVCTX, data8);

	data8  = 0;
	data8 |= (1 << 7); //[7]reg_sw_tpi_en,SW TPI Enable
	hdmitx21_wr_reg(LM_DDC_IVCTX, data8);
	//Do other initial in config_hdmi20_tx

	//Step4:stop Tx transmission
	//--Disable AV and h21tx_sb
	data8  = 0;
	data8 |= (1 << 1); //reg_block_av,1'b0:Enable to hdmi2sb block write tmds data into DPRAM
	data8 |= (0 << 2); //reg_block_hsync,DSC mode
	data8 |= (0 << 5); //reg_block_over,DSC mode
	data8 |= (0 << 6); //reg_block_over_val,DSC mode
	hdmitx21_wr_reg(H21TXSB_CTRL_1_IVCTX, data8);

	//--Disable h21tx_sb
	frl_rate_sel = (frl_rate > FRL_RATE_6G_3LANES) ? 1 : 0;
	data8  = 0;
	data8 |= (0 << 0); //reg_en,Moudel enable
	data8 |= (1 << 4); //reg_scrambler_en
	data8 |= (frl_rate_sel << 7); //reg_lane_sel,1'b1:4lane enable;1'b0:3lane enable
	hdmitx21_wr_reg(H21TXSB_CTRL_IVCTX, data8);

	data8  = 0;
	data8 |= (0 << 5); //reg_SCDC_sel
	data8 |= (0 << 4); //reg_LTS_req
	data8 |= (0 << 0); //reg_LTS_Cmd[3:0]
	hdmitx21_wr_reg(HT_DIG_CTL0_PHY_IVCTX, data8);

	//Step2:FRL pattern
	data8  = 0;
	data8 |= (0 << 0);//reg_inj_bist_err
	data8 |= (1 << 1);//reg_en_tp,prbs_16b_gen enable
	data8 |= (0 << 2);//reg_swap_tp_bits
	data8 |= (0 << 3);//reg_en_man_fsm,tp_gen module after prbs16b
	data8 |= (0 << 4);//reg_man_fsm[3:0]
	hdmitx21_wr_reg(HT_DIG_CTL22_PHY_IVCTX, data8);

	data8  = 0;
	data8 |= (0 << 0);//reg_tf_alt_clt_en,tc fifo control enable
	data8 |= (0 << 1);//reg_tf_rec_sel,tc fifo exception handling selection
	data8 |= (0 << 2);//reg_tf_err_rcv,tc fifo error recovery scheme
	data8 |= (0 << 3);//reg_en_man_20b_fsm,enable mannual 20bit calibration state
	data8 |= (0 << 4);//reg_man_20b_fsm,mannual 20bit calibration state
	hdmitx21_wr_reg(HT_DIG_CTL23_PHY_IVCTX, data8);
}

static void TX_LTS_2_POLL_READY(void)
{
	u8 FLT_ready = 0;
	u8 count = 20;
	u8 data8;

	while (!FLT_ready && count--) {
		scdc21_rd_sink(0x40, &data8);
		FLT_ready = data8 & 0x40; //[6] FLT_ready
	}
}

static void TX_LTS_2_SETTING(u8 frl_rate)
{
	u8 data8;
	//Step2:initial all lanes to FFE_LEVEL0
	//Conifg PLL and Analog Phy,TODO

	//Step3:check FRL max
	//Assume that Rx support FRL Max rate is 10G 4lane

	//Setp4:config Rx FRL_rate and FFE_level
	data8  = 0;
	//FRL_rate 0:disable frl;
	//         4'd1:3G 3Lane;
	//         4'd2:6G 3Lane;
	//         4'd3:6G 4Lane;
	//         4'd4:8G 4Lane;
	//         4'd5:10G 4Lane;
	//         4'd6:12G 4Lane.
	data8 |= (frl_rate << 0);
	//FFE_levels, Max FFE_Level supported for the current FRL rate.
	//  4'd0:FFE_LEVEL0;
	//  4'd1:FFE_LEVEL1;
	//  4'd2:FFE_LEVEL2;
	//  4'd3:FFE_LEVEL3;
	data8 |= (0 << 4);
	scdc21_wr_sink(0x31, data8);
	pr_info("configure SCDC/0x31 as %x\n", data8);
	scdc21_rd_sink(0x30, &data8);
	pr_info("read SCDC/0x30 as %x\n", data8);
	scdc21_wr_sink(0x30, 0);
	pr_info("write SCDC/0x30 as %x\n", 0);
}

static void TX_LTS_3_POLL_FLT_UPDATE(void)
{
	u8 hdmi21_update_reg;
	u8 FLT_update = 0;
	u8 data8;

	while (!FLT_update) {
		scdc21_rd_sink(0x10, &data8);//Update_flag registers,0x10
		hdmi21_update_reg = data8;
		FLT_update = hdmi21_update_reg & 0x20; //[5] FLT_update
	}
}

static void TX_LTS_3_READ_LTP_REQ(u16 *val)
{
	u8 data8;
	u8 ltp01;
	u8 ltp23;

	//
	scdc21_rd_sink(0x41, &data8); //[7:4]Ln1_LTP_req;[3:0]Ln0_LTP_req;
	ltp01 = data8;
	scdc21_rd_sink(0x42, &data8); //[7:4]Ln3_LTP_req;[3:0]Ln2_LTP_req;
	ltp23 = data8;

	*val = ltp01 + (ltp23 << 8);
	hdmitx21_wr_reg(HT_DIG_CTL1_PHY_IVCTX, ltp01);
	hdmitx21_wr_reg(HT_DIG_CTL2_PHY_IVCTX, ltp23);

	data8  = 0;
	data8 |= (0 << 0);//reg_cfg_LTP_req[7:0],prbs initial value
	hdmitx21_wr_reg(HT_DIG_CTL3_PHY_IVCTX, data8);

	data8  = 0;
	data8 |= (0 << 0);//reg_cfg_LTP_req[15:8],prbs initial value
	hdmitx21_wr_reg(HT_DIG_CTL4_PHY_IVCTX, data8);
}

static void TX_FLT_UPDATE_CLEAR(void)
{
	u8 hdmi21_update_reg;

	scdc21_rd_sink(0x10, &hdmi21_update_reg);//Update_flag registers,0x10
	if (hdmi21_update_reg & 0x10)
		scdc21_wr_sink(0x10, 0x20);//Update_flag registers,0x10
	else
		scdc21_wr_sink(0x10, hdmi21_update_reg | 0x20);//Update_flag registers,0x10
}

//======================= LTS P ========================
static void TX_LTS_P_SEND_ONLY_GAP(u8 frl_rate)
{
	u8 data8;
	u8 frl_rate_sel;
	//Step1:start FRL transsion with Gap only

	//Enable HDMI2P1
	data8  = 0;
	data8 |= (1 << 0); //reg_hdmi_en
	data8 |= (0 << 1); //reg_mhl3_en
	data8 |= (0 << 2); //reg_mhl3_priv
	data8 |= (0 << 3); //reg_cod_tdm_en
	data8 |= (1 << 4); //reg_tclk_from_io
	data8 |= (1 << 5); //reg_hdmi2p1_en !!!
	data8 |= (0 << 6); //reg_osc_en
	data8 |= (0 << 7); //reg_tclk_frm_8b10b
	hdmitx21_wr_reg(SOC_FUNC_SEL_IVCTX, data8);

	//Enable av_link dual mode
	data8  = 0;
	data8 |= (0 << 0);//reg_mhl3cts_rst,Software reset forMHL3 CTS logic
	data8 |= (0 << 4);//reg_tx_cch_inv,bitwise inversion of clock channel to PHY
	data8 |= (1 << 7);//reg_dualpp_mode.1'b1:dupal pixel mode;1'b0:single pixel mode.
	hdmitx21_wr_reg(SW_RST_IVCTX, data8);

	//--Disable AV and h21tx_sb
	data8  = 0;
	data8 |= (1 << 1); //reg_block_av,1'b0:Enable to hdmi2sb block write tmds data into DPRAM
	data8 |= (0 << 2); //reg_block_hsync,DSC mode
	data8 |= (0 << 5); //reg_block_over,DSC mode
	data8 |= (0 << 6); //reg_block_over_val,DSC mode
	hdmitx21_wr_reg(H21TXSB_CTRL_1_IVCTX, data8);

	//--Enable h21tx_sb
	frl_rate_sel = (frl_rate > FRL_RATE_6G_3LANES) ? 1 : 0;
	data8  = 0;
	data8 |= (1 << 0); //reg_en,Moudel enable
	data8 |= (1 << 4); //reg_scrambler_en
	data8 |= (frl_rate_sel << 7); //reg_lane_sel,1'b1:4lane enable;1'b0:3lane enable
	hdmitx21_wr_reg(H21TXSB_CTRL_IVCTX, data8);

	//Step2:FRL pattern stop
	data8  = 0;
	data8 |= (0 << 0);//reg_inj_bist_err
	data8 |= (0 << 1);//reg_en_tp,prbs_16b_gen enable
	data8 |= (0 << 2);//reg_swap_tp_bits
	data8 |= (0 << 3);//reg_en_man_fsm,tp_gen module after prbs16b
	data8 |= (0 << 4);//reg_man_fsm[3:0]
	hdmitx21_wr_reg(HT_DIG_CTL22_PHY_IVCTX, data8);

	data8  = 0;
	data8 |= (0 << 0);//reg_tf_alt_clt_en,tc fifo control enable
	data8 |= (0 << 1);//reg_tf_rec_sel,tc fifo exception handling selection
	data8 |= (0 << 2);//reg_tf_err_rcv,tc fifo error recovery scheme
	data8 |= (0 << 3);//reg_en_man_20b_fsm,enable mannual 20bit calibration state
	data8 |= (0 << 4);//reg_man_20b_fsm,mannual 20bit calibration state
	hdmitx21_wr_reg(HT_DIG_CTL23_PHY_IVCTX, data8);
}

static void TX_LTS_P_POLL_FRL_START(void)
{
	u8 FRL_start = 0;
	u8 hdmi21_update_reg = 0;
	u8 data8;
	u32 count = 100;

	while (!FRL_start && count--) {
		scdc21_rd_sink(0x10, &hdmi21_update_reg);//Update_flag registers,0x10
		FRL_start = hdmi21_update_reg & 0x10; //[4] FRL_start
	}
	//Step2:write FLT_start to 0
	scdc21_wr_sink(0x10, hdmi21_update_reg & (~0x10));//Update_flag registers,0x10

	//Step3:start FRL transmission with AV data
	//--Enable AV and h21tx_sb
	data8  = 0;
	data8 |= (0 << 1);
	data8 |= (0 << 2); //reg_block_hsync,DSC mode
	data8 |= (0 << 5); //reg_block_over,DSC mode
	data8 |= (0 << 6); //reg_block_over_val,DSC mode
	hdmitx21_wr_reg(H21TXSB_CTRL_1_IVCTX, data8);

	//Step4:Clear FRL_start by writing "1"
	scdc21_wr_sink(0x10, hdmi21_update_reg | 0x10);//Update_flag registers,0x10
}

int ltp_en = 1;
MODULE_PARM_DESC(ltp_en, "\n LTP enable\n");
module_param(ltp_en, int, 0644);

//=================================
//     hdmitx_frl_config
//=================================
static void hdmitx_frl_config(u8 color_depth, u8 frl_rate)
{
	u8 data8;
	u8 frl_rate_sel;
	//=============================================================
	//                   HDMI2p1 configuration begin
	//=============================================================
	data8  = 0;
	data8 |= (1 << 0); //reg_hdmi_en
	data8 |= (0 << 1); //reg_mhl3_en
	data8 |= (0 << 2); //reg_mhl3_priv
	data8 |= (0 << 3); //reg_cod_tdm_en
	data8 |= (1 << 4); //reg_tclk_from_io
	data8 |= (1 << 5); //reg_hdmi2p1_en !!!
	data8 |= (0 << 6); //reg_osc_en
	data8 |= (0 << 7); //reg_tclk_frm_8b10b
	hdmitx21_wr_reg(SOC_FUNC_SEL_IVCTX, data8);

	data8  = 0;
	data8 |= (1 << 0);
	data8 |= (1 << 2);
	data8 |= (0 << 7); //reg_clkration_sw_en
	hdmitx21_wr_reg(CLKRATIO_IVCTX, data8);

	data8  = 0;
	data8 |= ((color_depth & 0x03) << 0);
	data8 |= (0 << 2); //reg_force_idle
	data8 |= (1 << 3); //reg_dualpp_en
	data8 |= (1 << 4); //reg_in_dualpp_en
	data8 |= (0 << 5); //reg_null_pkt_en_vs_hi,Null packet Enable at VYSNC High.
	data8 |= (0 << 6); //reg_null_pkt_en,Null packet Enable.
	data8 |= (1 << 7); //reg_dc_pkt_en,Deep Color Packet Enable.
	hdmitx21_wr_reg(P2T_CTRL_IVCTX, data8);

	data8  = 0;
	data8 |= (0 << 0);//reg_mhl3cts_rst,Software reset forMHL3 CTS logic
	data8 |= (0 << 4);//reg_tx_cch_inv,bitwise inversion of clock channel to PHY
	data8 |= (1 << 7);//reg_dualpp_mode.1'b1:dupal pixel mode;1'b0:single pixel mode.
	hdmitx21_wr_reg(SW_RST_IVCTX, data8);

	data8  = 0;
	data8 |= (1 << 0);//reg_legacy_dev_support
	data8 |= (0 << 1);
	data8 |= (10 << 4);
	hdmitx21_wr_reg(HDPLL_FIX0_IVCTX, data8);

	data8  = 0;
	data8 |= (0 << 0);//reg_pkt_num_cycles[5:0],Number of cycles for a data island packet
	data8 |= (1 << 6);//reg_interlace_support.
	data8 |= (0 << 7);//reg_pkt_num_cycles[6]
	hdmitx21_wr_reg(HDPLL_FIX1_IVCTX, data8);

	data8  = 0;
	data8 |= (10 << 0);//reg_con_num_cycles[3:0]
	data8 |= (0 << 4);//reg_offset_cycles_new
	data8 |= (1 << 6);//reg_hw_intrlcd_vnt_ctl
	hdmitx21_wr_reg(HDPLL_FIX_NEW_0_IVCTX, data8);

	data8  = 0;
	data8 |= (0 << 0);//reg_con_pkt_num_cycles[6:0]
	data8 |= (1 << 7);//reg_vrr_hcnt_ctl_en
	hdmitx21_wr_reg(HDPLL_FIX_NEW_1_IVCTX, data8);

	data8  = 0;
	data8 |= (1 << 0); //[7:0] reg_pkt_period
	hdmitx21_wr_reg(H21TXSB_PKT_PRD_IVCTX, data8);

	//--Enable AV and h21tx_sb
	data8  = 0;
	data8 |= (0 << 1); //reg_block_av,1'b1:Enable to hdmi2sb block write tmds data into DPRAM
	data8 |= (0 << 2); //reg_block_hsync,DSC mode
	data8 |= (0 << 5); //reg_block_over,DSC mode
	data8 |= (0 << 6); //reg_block_over_val,DSC mode
	hdmitx21_wr_reg(H21TXSB_CTRL_1_IVCTX, data8);

	//--Disable h21tx_sb
	frl_rate_sel = (frl_rate > FRL_RATE_6G_3LANES) ? 1 : 0;
	data8  = 0;
	data8 |= (1 << 0); //reg_en,Moudel enable
	//data8 |= (1 << 4); //reg_scrambler_en
	data8 |= (0 << 4); //reg_scrambler_en
	data8 |= (frl_rate_sel << 7); //reg_lane_sel,1'b1:4lane enable;1'b0:3lane enable
	hdmitx21_wr_reg(H21TXSB_CTRL_IVCTX, data8);

	//--- regtx p0 ---

	hdmitx21_wr_reg(FRL_LINK_RATE_CONFIG_IVCTX, frl_rate);

	//=============================================================
	//                   HDMI2p1 configuration end
	//=============================================================
}

void hdmitx_frl_training_main(u8 frl_rate)
{
	u16 ltp0123 = 0xAAAA;//reserved LTP
	u8 data8;

	if (ltp_en == 0) {
		pr_info("skip the FRL LTP training...\n");
		hdmitx_frl_config(0, frl_rate);
		return;
	}
	hdmitx21_wr_reg(AON_CYP_CTL_IVCTX, 2); // 70kHz
	scdc21_rd_sink(0x10, &data8);
	scdc21_rd_sink(0x01, &data8);
	pr_info("[FRL TRAINING] read sink version %x\n", data8);
	data8 = 0x01;
	scdc21_wr_sink(0x02, data8);
	pr_info("[FRL TRAINING] write source version %x\n", data8);
	scdc21_rd_sink(0x35, &data8);
	pr_info("[FRL TRAINING] ************** TX_LTS_1_HDMI21_CONFIG************\n");
	TX_LTS_1_HDMI21_CONFIG(frl_rate);

	pr_info("[FRL TRAINING] ************** TX_LTS_2_POLL_READY************\n");
	TX_LTS_2_POLL_READY();
	pr_info("[FRL TRAINING] ************** TX_LTS_2_SETTING************\n");
	TX_LTS_2_SETTING(frl_rate);

	while (ltp0123 != 0) {
		pr_info("[FRL TRAINING] ************** TX_LTS_3_POLL_FLT_UPDATE************\n");
		TX_LTS_3_POLL_FLT_UPDATE();
		pr_info("[FRL TRAINING] ************** TX_LTS_3_READ_LTP_REQ************\n");
		TX_LTS_3_READ_LTP_REQ(&ltp0123);
		pr_info("[FRL TRAINING] ************** TX_FLT_UPDATE_CLEAR************\n");
		TX_FLT_UPDATE_CLEAR();
	}

	pr_info("[FRL TRAINING] ************** TX_LTS_P_SEND_ONLY_GAP************\n");
	TX_LTS_P_SEND_ONLY_GAP(frl_rate);
	pr_info("[FRL TRAINING] ************** TX_FLT_UPDATE_CLEAR************\n");
	TX_FLT_UPDATE_CLEAR();
	pr_info("[FRL TRAINING] ************** TX_LTS_P_POLL_FRL_START************\n");
	TX_LTS_P_POLL_FRL_START();
}
