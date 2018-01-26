/*
 * Amlogic G9TV
 * HDMI RX
 * Copyright (C) 2014 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the named License,
 * or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
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
/* #include <mach/clock.h> */
/* #include <mach/register.h> */
/* #include <mach/power_gate.h> */

/* if (is_meson_g9tv_cpu() || is_meson_m8_cpu() || */
/* is_meson_m8m2_cpu() || is_meson_gxbb_cpu() || */
/* is_meson_m8b_cpu()) */
#include <linux/of_gpio.h>

#include <linux/amlogic/tvin/tvin.h>
/* Local include */
#include "hdmirx_drv.h"
#include "hdmi_rx_reg.h"
#include "hdmi_rx_eq.h"

#define HDMI_STATE_CHECK_FREQ     (20*5)
#define HW_MONITOR_TIME_UNIT    (1000/HDMI_STATE_CHECK_FREQ)

/* EDID */
#define MAX_RECEIVE_EDID	33
#define MAX_HDR_LUMI		3
#define MAX_KSV_SIZE		5
#define MAX_REPEAT_COUNT	127
#define MAX_REPEAT_DEPTH	7
#define MAX_KSV_LIST_SIZE	(MAX_KSV_SIZE*MAX_REPEAT_COUNT)
/*size of one format in edid*/
#define FORMAT_SIZE			sizeof(struct edid_audio_block_t)
#define is_audio_support(x) (((x) == AUDIO_FORMAT_LPCM) || \
		((x) == AUDIO_FORMAT_DTS) || ((x) == AUDIO_FORMAT_DDP))
#define EDID_SIZE			256
#define EDID_HDR_SIZE			7
#define HDR_IGNOR_TIME			500 /*5s*/
#define MAX_EDID_BUF_SIZE 1024
#define EDID_DEFAULT_START		132
#define EDID_DESCRIP_OFFSET		2
#define EDID_BLOCK1_OFFSET		128
#define MAX_KEY_BUF_SIZE 512
#define KSV_LIST_WR_TH			100
#define KSV_V_WR_TH			500
#define KSV_LIST_WR_MAX			5
#define KSV_LIST_WAIT_DELAY		500/*according to the timer,5s*/

/* aud sample rate stable range */
#define AUD_SR_RANGE 1000
#define AUD_SR_STB_MAX 20
/* PHY config */
#define CFG_CLK 24000
#define PHY_LOCK_THRES 0x3F
#define DVI_FIXED_TO_RGB  1

static int sig_pll_unlock_cnt;
static int sig_pll_unlock_max = 150;
MODULE_PARM_DESC(sig_pll_unlock_max, "\n sig_pll_unlock_max\n");
module_param(sig_pll_unlock_max, int, 0664);

static int sig_pll_lock_cnt;
static unsigned sig_pll_lock_max = 3;
MODULE_PARM_DESC(sig_pll_lock_max, "\n sig_pll_lock_max\n");
module_param(sig_pll_lock_max, int, 0664);

static int dwc_rst_wait_cnt;
static int dwc_rst_wait_cnt_max = 40;
MODULE_PARM_DESC(dwc_rst_wait_cnt_max, "\n dwc_rst_wait_cnt_max\n");
module_param(dwc_rst_wait_cnt_max, int, 0664);

static bool force_hdmi_5v_high;
MODULE_PARM_DESC(force_hdmi_5v_high, "\n force_hdmi_5v_high\n");
module_param(force_hdmi_5v_high, bool, 0664);

static int sig_lost_lock_cnt;
static int sig_lost_lock_max = 5;
MODULE_PARM_DESC(sig_lost_lock_max, "\n sig_lost_lock_max\n");
module_param(sig_lost_lock_max, int, 0664);

static int sig_stable_cnt;
static int sig_stable_max = 40;
MODULE_PARM_DESC(sig_stable_max, "\n sig_stable_max\n");
module_param(sig_stable_max, int, 0664);

static bool clk_debug;
MODULE_PARM_DESC(clk_debug, "\n clk_debug\n");
module_param(clk_debug, bool, 0664);

static int hpd_wait_cnt;
static int hpd_wait_max = 15;
MODULE_PARM_DESC(hpd_wait_max, "\n hpd_wait_max\n");
module_param(hpd_wait_max, int, 0664);

static int sig_unstable_cnt;
static int sig_unstable_max = 80;
MODULE_PARM_DESC(sig_unstable_max, "\n sig_unstable_max\n");
module_param(sig_unstable_max, int, 0664);

static int sig_unready_cnt;
static int sig_unready_max = 8;/* 10; */
MODULE_PARM_DESC(sig_unready_max, "\n sig_unready_max\n");
module_param(sig_unready_max, int, 0664);

static int hdcp22_lost_max = 30;
MODULE_PARM_DESC(hdcp22_lost_max, "\n hdcp22_lost_max\n");
module_param(hdcp22_lost_max, int, 0664);

static bool enable_hpd_reset;
MODULE_PARM_DESC(enable_hpd_reset, "\n enable_hpd_reset\n");
module_param(enable_hpd_reset, bool, 0664);

static int pow5v_max_cnt = 9;
MODULE_PARM_DESC(pow5v_max_cnt, "\n pow5v_max_cnt\n");
module_param(pow5v_max_cnt, int, 0664);

static int sig_unstable_reset_hpd_cnt;
static int sig_unstable_reset_hpd_max = 5;
MODULE_PARM_DESC(sig_unstable_reset_hpd_max,
		 "\n sig_unstable_reset_hpd_max\n");
module_param(sig_unstable_reset_hpd_max, int, 0664);

int rgb_quant_range = 0;
MODULE_PARM_DESC(rgb_quant_range, "\n rgb_quant_range\n");
module_param(rgb_quant_range, int, 0664);

int yuv_quant_range = 0;
MODULE_PARM_DESC(yuv_quant_range, "\n yuv_quant_range\n");
module_param(yuv_quant_range, int, 0664);

int it_content;
MODULE_PARM_DESC(it_content, "\n it_content\n");
module_param(it_content, int, 0664);

/* timing diff offset */
static int diff_pixel_th = 2;
static int diff_line_th = 5;
static int diff_frame_th = 40; /* (25hz-24hz)/2 = 50/100 */
MODULE_PARM_DESC(diff_pixel_th, "\n diff_pixel_th\n");
module_param(diff_pixel_th, int, 0664);
MODULE_PARM_DESC(diff_line_th, "\n diff_line_th\n");
module_param(diff_line_th, int, 0664);
MODULE_PARM_DESC(diff_frame_th, "\n diff_frame_th\n");
module_param(diff_frame_th, int, 0664);

static int port_map = 0x3210;
MODULE_PARM_DESC(port_map, "\n port_map\n");
module_param(port_map, int, 0664);

int real_port_map = 0x3120;
MODULE_PARM_DESC(real_port_map, "\n real_port_map\n");
module_param(real_port_map, int, 0664);




static int edid_mode;
MODULE_PARM_DESC(edid_mode, "\n edid_mode\n");
module_param(edid_mode, int, 0664);

static int force_vic;
MODULE_PARM_DESC(force_vic, "\n force_vic\n");
module_param(force_vic, int, 0664);

static int force_ready;
MODULE_PARM_DESC(force_ready, "\n force_ready\n");
module_param(force_ready, int, 0664);

bool hdcp22_kill_esm;
MODULE_PARM_DESC(hdcp22_kill_esm, "\n hdcp22_kill_esm\n");
module_param(hdcp22_kill_esm, bool, 0664);

static int repeat_check = 1;
MODULE_PARM_DESC(repeat_check, "\n repeat_check\n");
module_param(repeat_check, int, 0664);

static int force_state;
MODULE_PARM_DESC(force_state, "\n force_state\n");
module_param(force_state, int, 0664);

static int force_audio_sample_rate;
MODULE_PARM_DESC(force_audio_sample_rate, "\n force_audio_sample_rate\n");
module_param(force_audio_sample_rate, int, 0664);

static int audio_sample_rate;/* used in other module */
MODULE_PARM_DESC(audio_sample_rate, "\n audio_sample_rate\n");
module_param(audio_sample_rate, int, 0664);

static int auds_rcv_sts;
MODULE_PARM_DESC(auds_rcv_sts, "\n auds_rcv_sts\n");
module_param(auds_rcv_sts, int, 0664);

static int audio_coding_type;
MODULE_PARM_DESC(audio_coding_type, "\n audio_coding_type\n");
module_param(audio_coding_type, int, 0664);

static int audio_channel_count;
MODULE_PARM_DESC(audio_channel_count, "\n audio_channel_count\n");
module_param(audio_channel_count, int, 0664);

static int frame_rate;
MODULE_PARM_DESC(frame_rate, "\n frame_rate\n");
module_param(frame_rate, int, 0664);
static int hdcp22_sts = 0xff;
MODULE_PARM_DESC(hdcp22_sts, "\n hdcp22_sts\n");
module_param(hdcp22_sts, int, 0664);

static int hdcp22_capable_sts = 0xff;
MODULE_PARM_DESC(hdcp22_capable_sts, "\n hdcp22_capable_sts\n");
module_param(hdcp22_capable_sts, int, 0664);

static bool need_rst_esm;
MODULE_PARM_DESC(need_rst_esm, "\n need_rst_esm\n");
module_param(need_rst_esm, bool, 0664);

static bool esm_auth_fail_en;
MODULE_PARM_DESC(esm_auth_fail_en, "\n esm_auth_fail_en\n");
module_param(esm_auth_fail_en, bool, 0664);

static int hdcp22_auth_sts = 0xff;
MODULE_PARM_DESC(hdcp22_auth_sts, "\n hdcp22_auth_sts\n");
module_param(hdcp22_auth_sts, int, 0664);

static bool do_link_lost_reset;
MODULE_PARM_DESC(do_link_lost_reset, "\n do_link_lost_reset\n");
module_param(do_link_lost_reset, bool, 0664);

/* 0x100--irq print; */
/* 0x200-other print */
/* bit 0, printk; bit 8 enable irq log */
int log_level = LOG_EN | HDCP_LOG | EQ_LOG;
MODULE_PARM_DESC(log_level, "\n log_level\n");
module_param(log_level, int, 0664);

static bool frame_skip_en = 1;	/* skip frame when signal unstable */
MODULE_PARM_DESC(frame_skip_en, "\n frame_skip_en\n");
module_param(frame_skip_en, bool, 0664);

static bool use_frame_rate_check;
MODULE_PARM_DESC(use_frame_rate_check, "\n use_frame_rate_check\n");
module_param(use_frame_rate_check, bool, 0664);

static bool hw_dbg_en = 1;	/* only for hardware test */
MODULE_PARM_DESC(hw_dbg_en, "\n hw_dbg_en\n");
module_param(hw_dbg_en, bool, 0664);

static bool auto_switch_off;	/* only for hardware test */
MODULE_PARM_DESC(auto_switch_off, "\n auto_switch_off\n");
module_param(auto_switch_off, bool, 0664);

int wait_clk_stable_cnt;
static int wait_clk_stable_max = 400;
MODULE_PARM_DESC(wait_clk_stable_max, "\n wait_clk_stable_max\n");
module_param(wait_clk_stable_max, int, 0664);

int is_clk_stable_cnt;
static int is_clk_stable_max = 3;
MODULE_PARM_DESC(is_clk_stable_max, "\n is_clk_stable_max\n");
module_param(is_clk_stable_max, int, 0664);

int force_wait_cnt;
static int force_wait_max = 30;
MODULE_PARM_DESC(force_wait_max, "\n force_wait_max\n");
module_param(force_wait_max, int, 0664);

static bool is_phy_reset = true;
MODULE_PARM_DESC(is_phy_reset, "\n is_phy_reset\n");
module_param(is_phy_reset, bool, 0664);

static bool need_clk_stable;
MODULE_PARM_DESC(need_clk_stable, "\n need_clk_stable\n");
module_param(need_clk_stable, bool, 0664);

static int eq_dbg_ch0;
MODULE_PARM_DESC(eq_dbg_ch0, "\n eq_dbg_ch0\n");
module_param(eq_dbg_ch0, int, 0664);

static int eq_dbg_ch1;
MODULE_PARM_DESC(eq_dbg_ch1, "\n eq_dbg_ch1\n");
module_param(eq_dbg_ch1, int, 0664);

static int eq_dbg_ch2;
MODULE_PARM_DESC(eq_dbg_ch2, "\n eq_dbg_ch2\n");
module_param(eq_dbg_ch2, int, 0664);

static int wait_no_sig_max = 600;
MODULE_PARM_DESC(wait_no_sig_max, "\n wait_no_sig_max\n");
module_param(wait_no_sig_max, int, 0664);

static int wait_no_sig_cnt;
MODULE_PARM_DESC(wait_no_sig_cnt, "\n wait_no_sig_cnt\n");
module_param(wait_no_sig_cnt, int, 0664);

static bool hdr_enable = true;
MODULE_PARM_DESC(hdr_enable, "\n hdr_enable\n");
module_param(hdr_enable, bool, 0664);

/*edid original data from device*/
static unsigned char receive_edid[MAX_RECEIVE_EDID];
static int receive_edid_len = MAX_RECEIVE_EDID;
MODULE_PARM_DESC(receive_edid, "\n receive_edid\n");
module_param_array(receive_edid, byte, &receive_edid_len, 0664);

static bool new_edid;
MODULE_PARM_DESC(new_edid, "\n new_edid\n");
module_param(new_edid, bool, 0664);

/*edid original data from device*/
static unsigned char receive_hdr_lum[MAX_HDR_LUMI];
static int hdr_lum_len = MAX_HDR_LUMI;
MODULE_PARM_DESC(receive_hdr_lum, "\n receive_hdr_lum\n");
module_param_array(receive_hdr_lum, byte, &hdr_lum_len, 0664);

static bool new_hdr_lum;
MODULE_PARM_DESC(new_hdr_lum, "\n new_hdr_lum\n");
module_param(new_hdr_lum, bool, 0664);

/*edid original data from device*/
static unsigned char receive_hdcp[MAX_KSV_LIST_SIZE];
static int hdcp_array_len = MAX_KSV_LIST_SIZE;
MODULE_PARM_DESC(receive_hdcp, "\n receive_hdcp\n");
module_param_array(receive_hdcp, byte, &hdcp_array_len, 0664);
static int hdcp_len;
MODULE_PARM_DESC(hdcp_len, "\n hdcp_len\n");
module_param(hdcp_len, int, 0664);

static int hdcp_repeat_depth;
MODULE_PARM_DESC(hdcp_repeat_depth, "\n hdcp_repeat_depth\n");
module_param(hdcp_repeat_depth, int, 0664);

static bool new_hdcp;
MODULE_PARM_DESC(new_hdcp, "\n new_hdcp\n");
module_param(new_hdcp, bool, 0664);

static bool repeat_plug;
MODULE_PARM_DESC(repeat_plug, "\n repeat_plug\n");
module_param(repeat_plug, bool, 0664);

static int up_phy_addr;/*d c b a 4bit*/
MODULE_PARM_DESC(up_phy_addr, "\n up_phy_addr\n");
module_param(up_phy_addr, int, 0664);

static int hdcp22_reset_max = 20;
MODULE_PARM_DESC(hdcp22_reset_max, "\n hdcp22_reset_max\n");
module_param(hdcp22_reset_max, int, 0664);

#ifdef HDCP22_ENABLE
/* to inform ESM whether the cable is connected or not */
bool hpd_to_esm;
MODULE_PARM_DESC(hpd_to_esm, "\n hpd_to_esm\n");
module_param(hpd_to_esm, bool, 0664);

bool reset_esm_flag;
MODULE_PARM_DESC(reset_esm_flag, "\n reset_esm_flag\n");
module_param(reset_esm_flag, bool, 0664);

/* to inform ESM whether the cable is connected or not */
bool video_stable_to_esm;
MODULE_PARM_DESC(video_stable_to_esm, "\n video_stable_to_esm\n");
module_param(video_stable_to_esm, bool, 0664);

static bool hdcp_mode_sel;
MODULE_PARM_DESC(hdcp_mode_sel, "\n hdcp_mode_sel\n");
module_param(hdcp_mode_sel, bool, 0664);

static bool hdcp_auth_status;
MODULE_PARM_DESC(hdcp_auth_status, "\n hdcp_auth_status\n");
module_param(hdcp_auth_status, bool, 0664);

static int loadkey_22_hpd_delay = 110;
MODULE_PARM_DESC(loadkey_22_hpd_delay, "\n loadkey_22_hpd_delay\n");
module_param(loadkey_22_hpd_delay, int, 0664);

static int wait_hdcp22_cnt = 900;
MODULE_PARM_DESC(wait_hdcp22_cnt, "\n wait_hdcp22_cnt\n");
module_param(wait_hdcp22_cnt, int, 0664);

static int wait_hdcp22_cnt1 = 200;
MODULE_PARM_DESC(wait_hdcp22_cnt1, "\n wait_hdcp22_cnt1\n");
module_param(wait_hdcp22_cnt1, int, 0664);

static int wait_hdcp22_cnt2 = 50;
MODULE_PARM_DESC(wait_hdcp22_cnt2, "\n wait_hdcp22_cnt2\n");
module_param(wait_hdcp22_cnt2, int, 0664);

static int wait_hdcp22_cnt3 = 900;
MODULE_PARM_DESC(wait_hdcp22_cnt3, "\n wait_hdcp22_cnt3\n");
module_param(wait_hdcp22_cnt3, int, 0664);

static int enable_hdcp22_loadkey = 1;
MODULE_PARM_DESC(enable_hdcp22_loadkey, "\n enable_hdcp22_loadkey\n");
module_param(enable_hdcp22_loadkey, int, 0664);

int do_esm_rst_flag;
MODULE_PARM_DESC(do_esm_rst_flag, "\n do_esm_rst_flag\n");
module_param(do_esm_rst_flag, int, 0664);

bool enable_hdcp22_esm_log;
MODULE_PARM_DESC(enable_hdcp22_esm_log, "\n enable_hdcp22_esm_log\n");
module_param(enable_hdcp22_esm_log, bool, 0664);

bool hdcp22_firmware_ok_flag = 1;
MODULE_PARM_DESC(hdcp22_firmware_ok_flag, "\n hdcp22_firmware_ok_flag\n");
module_param(hdcp22_firmware_ok_flag, bool, 0664);

int esm_err_force_14;
MODULE_PARM_DESC(esm_err_force_14, "\n esm_err_force_14\n");
module_param(esm_err_force_14, int, 0664);

static int reboot_esm_done;
MODULE_PARM_DESC(reboot_esm_done, "\n reboot_esm_done\n");
module_param(reboot_esm_done, int, 0664);

int esm_reboot_lvl = 1;
MODULE_PARM_DESC(esm_reboot_lvl, "\n esm_reboot_lvl\n");
module_param(esm_reboot_lvl, int, 0664);

int enable_esm_reboot;
MODULE_PARM_DESC(enable_esm_reboot, "\n enable_esm_reboot\n");
module_param(enable_esm_reboot, int, 0664);

bool esm_error_flag;
MODULE_PARM_DESC(esm_error_flag, "\n esm_error_flag\n");
module_param(esm_error_flag, bool, 0664);

int esm_data_base_addr;
MODULE_PARM_DESC(esm_data_base_addr, "\n esm_data_base_addr\n");
module_param(esm_data_base_addr, int, 0664);
#endif

int scdc_tmds_try_max = 3;
module_param(scdc_tmds_try_max, int, 0664);
MODULE_PARM_DESC(scdc_tmds_try_max, "scdc_tmds_try_max");

int do_hpd_reset_flag;
int wait_hpd_reset_max = 300;
module_param(wait_hpd_reset_max, int, 0664);
MODULE_PARM_DESC(wait_hpd_reset_max, "wait_hpd_reset_max");

bool use_dwc_reset = true;
MODULE_PARM_DESC(use_dwc_reset, "\n use_dwc_reset\n");
module_param(use_dwc_reset, bool, 0664);
bool is_hdcp_source = true;
MODULE_PARM_DESC(is_hdcp_source, "\n is_hdcp_source\n");
module_param(is_hdcp_source, bool, 0664);

int stable_check_lvl = 0x2ff;
module_param(stable_check_lvl, int, 0664);
MODULE_PARM_DESC(stable_check_lvl, "stable_check_lvl");

static bool use_eq_workaround;
module_param(use_eq_workaround, bool, 0664);
MODULE_PARM_DESC(use_eq_workaround, "use_eq_workaround");

/****************************/
/*  func enhancements  */
/****************************/

bool hdcp22_reauth_enable = 1;
static int audio_enable = 1;
static int hdcp22_lost_cnt;
static int last_color_fmt;
static bool reset_sw = true;
static int sm_pause;
static int irq_video_mute_flag;
static bool edid_addr_intr_flag;
static bool use_audioresample_reset;
static int fsm_enhancement;
static int port_select_ovr_en;
static int phy_cmu_config_force_val;
static int phy_system_config_force_val;
static int acr_mode;
int pre_port = 0xff;
int skip_frame_num = 3;
int skip_frame_max = 15;

/***********************
  TVIN driver interface
************************/

struct rx_s rx;

/* static unsigned long tmds_clock_old = 0; */

/** TMDS clock delta [kHz] */
#define TMDS_CLK_DELTA			(125)
/** Pixel clock minimum [kHz] */
#define PIXEL_CLK_MIN			TMDS_CLK_MIN
/** Pixel clock maximum [kHz] */
#define PIXEL_CLK_MAX			TMDS_CLK_MAX
/** Horizontal resolution minimum */
#define HRESOLUTION_MIN			(320)
/** Horizontal resolution maximum */
#define HRESOLUTION_MAX			(4096)
/** Vertical resolution minimum */
#define VRESOLUTION_MIN			(240)
/** Vertical resolution maximum */
#define VRESOLUTION_MAX			(4455)
/** Refresh rate minimum [Hz] */
#define REFRESH_RATE_MIN		(100)
/** Refresh rate maximum [Hz] */
#define REFRESH_RATE_MAX		(25000)

#define TMDS_TOLERANCE  (4000)
#define MAX_AUDIO_SAMPLE_RATE		(192000+1000)	/* 192K */
#define MIN_AUDIO_SAMPLE_RATE		(8000-1000)	/* 8K */

struct hdmi_rx_ctrl_hdcp init_hdcp_data;
static char key_buf[MAX_KEY_BUF_SIZE];
static int key_size;

static unsigned char edid_temp[EDID_SIZE];
static char edid_buf[MAX_EDID_BUF_SIZE];
static int edid_size;

static unsigned char aml_edid_DD[] = {
0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
0x05, 0xac, 0x03, 0xb1, 0x01, 0x00, 0x00, 0x00,
0x28, 0x19, 0x01, 0x03, 0x80, 0x7a, 0x44, 0x78,
0x0a, 0x0d, 0xc9, 0xa0, 0x57, 0x47, 0x98, 0x27,
0x12, 0x48, 0x4c, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xf2, 0x70, 0x5a, 0x80, 0xb0, 0x58,
0x8a, 0x00, 0x20, 0xc2, 0x31, 0x00, 0x00, 0x1e,
0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40,
0x58, 0x2c, 0x45, 0x00, 0x20, 0xc2, 0x31, 0x00,
0x00, 0x1e, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x53,
0x4b, 0x59, 0x20, 0x54, 0x56, 0x0a, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfd,
0x00, 0x30, 0x3e, 0x0e, 0x46, 0x0f, 0x00, 0x0a,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x3e,
0x02, 0x03, 0x44, 0xf0, 0x59, 0x1f, 0x10, 0x14,
0x05, 0x13, 0x04, 0x20, 0x22, 0x3c, 0x3e, 0x12,
0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06, 0x01,
0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x29, 0x09,
0x07, 0x07, 0x15, 0x07, 0x50, 0x57, 0x07, 0x00,
0x83, 0x01, 0x00, 0x00, 0x6e, 0x03, 0x0c, 0x00,
0x30, 0x00, 0x88, 0x3c, 0x2f, 0x00, 0x80, 0x01,
0x02, 0x03, 0x04, 0xe2, 0x00, 0xfb, 0xe5, 0x0e,
0x60, 0x61, 0x65, 0x66, 0x02, 0x3a, 0x80, 0xd0,
0x72, 0x38, 0x2d, 0x40, 0x10, 0x2c, 0x45, 0x80,
0xc2, 0xad, 0x42, 0x00, 0x00, 0x1e, 0x01, 0x1d,
0x00, 0xbc, 0x52, 0xd0, 0x1e, 0x20, 0xb8, 0x28,
0x55, 0x40, 0xc2, 0xad, 0x42, 0x00, 0x00, 0x1e,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0,
};

static unsigned char aml_edid[] = {
0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
0x05, 0xac, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x20, 0x19, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0a, 0xcf, 0x74, 0xa3, 0x57, 0x4c, 0xb0, 0x23,
0x09, 0x48, 0x4c, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xf2, 0x70, 0x5a, 0x80, 0xb0, 0x58,
0x8a, 0x00, 0x20, 0xc2, 0x31, 0x00, 0x00, 0x1e,
0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40,
0x58, 0x2c, 0x45, 0x00, 0x20, 0xc2, 0x31, 0x00,
0x00, 0x1e, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x41,
0x4d, 0x4c, 0x20, 0x54, 0x56, 0x0a, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfd,
0x00, 0x3b, 0x46, 0x1f, 0x8c, 0x3c, 0x00, 0x0a,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xda,
0x02, 0x03, 0x44, 0xF0, 0x5B, 0x5F, 0x10, 0x1F,
0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06,
0x01, 0x61, 0x5D, 0x64, 0x65, 0x66, 0x62, 0x60,
0x23, 0x09, 0x07, 0x03, 0x83, 0x01, 0x00, 0x00,
0x6E, 0x03, 0x0C, 0x00, 0x20, 0x00, 0x98, 0x3C,
0x20, 0x80, 0x80, 0x01, 0x02, 0x03, 0x04, 0xE2,
0x00, 0xfb, 0xE5, 0x0F, 0x00, 0x00, 0x90, 0x05,
0xE3, 0x06, 0x05, 0x01, 0x02, 0x3A, 0x80, 0xD0,
0x72, 0x38, 0x2D, 0x40, 0x10, 0x2C, 0x45, 0x80,
0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F, 0x01, 0x1D,
0x00, 0xBC, 0x52, 0xD0, 0x1E, 0x20, 0xB8, 0x28,
0x55, 0x40, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F,
0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10,
0x10, 0x3E, 0x96, 0x00, 0x13, 0x8E, 0x21, 0x00,
0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe2,
};

static unsigned char v2_edid[] = {
0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
0x08, 0xc0, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x20, 0x19, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0a, 0xcf, 0x74, 0xa3, 0x57, 0x4c, 0xb0, 0x23,
0x09, 0x48, 0x4c, 0x00, 0x00, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xf2, 0x70, 0x5a, 0x80, 0xb0, 0x58,
0x8a, 0x00, 0x20, 0xc2, 0x31, 0x00, 0x00, 0x1e,
0x02, 0x3a, 0x80, 0x18, 0x71, 0x38, 0x2d, 0x40,
0x58, 0x2c, 0x45, 0x00, 0x20, 0xc2, 0x31, 0x00,
0x00, 0x1e, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x42,
0x46, 0x20, 0x54, 0x56, 0x0a, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xfd,
0x00, 0x3b, 0x46, 0x1f, 0x8c, 0x3c, 0x00, 0x0a,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x1e,
0x02, 0x03, 0x45, 0xf0, 0x5b, 0x5f, 0x10, 0x1f,
0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x3c, 0x3e,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06,
0x01, 0x61, 0x5d, 0x64, 0x65, 0x66, 0x62, 0x60,
0x23, 0x09, 0x07, 0x03, 0x83, 0x01, 0x00, 0x00,
0x6a, 0x03, 0x0c, 0x00, 0x20, 0x00, 0x88, 0x3c,
0x20, 0x80, 0x00, 0x67, 0xd8, 0x5d, 0xc4, 0x01,
0x78, 0x88, 0x01, 0xe5, 0x0f, 0x00, 0x00, 0x90,
0x05, 0xe3, 0x06, 0x05, 0x01, 0x02, 0x3a, 0x80,
0xd0, 0x72, 0x38, 0x2d, 0x40, 0x10, 0x2c, 0x45,
0x80, 0x30, 0xeb, 0x52, 0x00, 0x00, 0x1f, 0x01,
0x1d, 0x00, 0xbc, 0x52, 0xd0, 0x1e, 0x20, 0xb8,
0x28, 0x55, 0x40, 0x30, 0xeb, 0x52, 0x00, 0x00,
0x1f, 0x8c, 0x0a, 0xd0, 0x8a, 0x20, 0xe0, 0x2d,
0x10, 0x10, 0x3e, 0x96, 0x00, 0x13, 0x8e, 0x21,
0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0xfa,
};


/* test EDID skyworth mst/mtk */
static unsigned char edid_skyworth[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x4D, 0x77,
0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
0x1C, 0x16, 0x01, 0x03, 0x80, 0x3C, 0x22, 0x78, 0x0A, 0x0D,
0xC9, 0xA0, 0x57, 0x47, 0x98, 0x27,
0x12, 0x48, 0x4C, 0xBF, 0xEF, 0x00, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1D, 0x00, 0x72,
0x51, 0xD0, 0x1E, 0x20, 0x6E, 0x28,
0x55, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E, 0x01, 0x1D,
0x80, 0x18, 0x71, 0x1C, 0x16, 0x20,
0x58, 0x2C, 0x25, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x9E,
0x00, 0x00, 0x00, 0xFC, 0x00, 0x20,
0x38, 0x4B, 0x35, 0x35, 0x20, 0x20, 0x20, 0x20, 0x4C, 0x45,
0x44, 0x0A, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x31, 0x4C, 0x0F, 0x50, 0x0E, 0x00, 0x0A, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x01, 0xBB,
0x02, 0x03, 0x29, 0x74, 0x4B, 0x84, 0x10, 0x1F, 0x05, 0x13,
0x14, 0x01, 0x02, 0x11, 0x06, 0x15,
0x26, 0x09, 0x7F, 0x03, 0x11, 0x7F, 0x18, 0x83, 0x01, 0x00,
0x00, 0x6D, 0x03, 0x0C, 0x00, 0x10,
0x00, 0xB8, 0x3C, 0x2F, 0x80, 0x60, 0x01, 0x02, 0x03, 0x01,
0x1D, 0x00, 0xBC, 0x52, 0xD0, 0x1E,
0x20, 0xB8, 0x28, 0x55, 0x40, 0xC4, 0x8E, 0x21, 0x00, 0x00,
0x1E, 0x01, 0x1D, 0x80, 0xD0, 0x72,
0x1C, 0x16, 0x20, 0x10, 0x2C, 0x25, 0x80, 0xC4, 0x8E, 0x21,
0x00, 0x00, 0x9E, 0x8C, 0x0A, 0xD0,
0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0x13,
0x8E, 0x21, 0x00, 0x00, 0x18, 0x8C,
0x0A, 0xD0, 0x90, 0x20, 0x40, 0x31, 0x20, 0x0C, 0x40, 0x55,
0x00, 0x13, 0x8E, 0x21, 0x00, 0x00,
0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x22
};
/* test EDID skyworth mst/mtk */
static unsigned char edid_4K_MSD[] = {
0x00 , 0xFF , 0xFF , 0xFF , 0xFF , 0xFF , 0xFF , 0x00 , 0x36 ,
0x74 , 0x30 , 0x00 , 0x01 , 0x00 , 0x00 , 0x00,
0x0A , 0x18 , 0x01 , 0x03 , 0x80 , 0x73 , 0x41 , 0x78 , 0x0A ,
0xCF , 0x74 , 0xA3 , 0x57 , 0x4C , 0xB0 , 0x23,
0x09 , 0x48 , 0x4C , 0x00 , 0x00 , 0x00 , 0x01 , 0x01 , 0x01 ,
0xFF , 0x01 , 0xFF , 0xFF , 0x01 , 0x01 , 0x01,
0x01 , 0x01 , 0x01 , 0x01 , 0x01 , 0x20 , 0x08 , 0xE8 , 0x00 ,
0x30 , 0xF2 , 0x70 , 0x5A , 0x80 , 0xB0 , 0x58,
0x8A , 0x00 , 0xC4 , 0x8E , 0x21 , 0x00 , 0x00 , 0x1E , 0x02 ,
0x3A , 0x80 , 0x18 , 0x71 , 0x38 , 0x2D , 0x40,
0x58 , 0x2C , 0x45 , 0x00 , 0xC4 , 0x8E , 0x21 , 0x00 , 0x00 ,
0x1E , 0x00 , 0x00 , 0x00 , 0xFC , 0x00 , 0x4D,
0x53 , 0x74 , 0x61 , 0x72 , 0x20 , 0x44 , 0x65 , 0x6D , 0x6F ,
0x0A , 0x20 , 0x20 , 0x00 , 0x00 , 0x00 , 0xFD,
0x00 , 0x3B , 0x46 , 0x1F , 0x8C , 0x3C , 0x00 , 0x0A , 0x20 ,
0x20 , 0x20 , 0x20 , 0x20 , 0x20 , 0x01 , 0x68,
0x02 , 0x03 , 0x4C , 0xF2 , 0x5B , 0x05 , 0x84 , 0x03 , 0x01 ,
0x12 , 0x13 , 0x14 , 0x16 , 0x07 , 0x90 , 0x1F,
0x20 , 0x22 , 0x5D , 0x5F , 0x60 , 0x61 , 0x62 , 0x64 , 0x65 ,
0x66 , 0x5E , 0x63 , 0x02 , 0x06 , 0x11 , 0x15,
0x26 , 0x09 , 0x07 , 0x07 , 0x11 , 0x07 , 0x06 , 0x83 , 0x01 ,
0x00 , 0x00 , 0x6E , 0x03 , 0x0C , 0x00 , 0x10,
0x00 , 0x78 , 0x44 , 0x20 , 0x00 , 0x80 , 0x01 , 0x02 , 0x03 ,
0x04 , 0x67 , 0xD8 , 0x5D , 0xC4 , 0x01 , 0x78,
0xC8 , 0x07 , 0xE3 , 0x05 , 0x03 , 0x01 , 0xE5 , 0x0F , 0x00 ,
0x80 , 0x19 , 0x00 , 0x8C , 0x0A , 0xD0 , 0x8A,
0x20 , 0xE0 , 0x2D , 0x10 , 0x10 , 0x3E , 0x96 , 0x00 , 0xC4 ,
0x8E , 0x21 , 0x00 , 0x00 , 0x18 , 0x8C , 0x0A,
0xA0 , 0x14 , 0x51 , 0xF0 , 0x16 , 0x00 , 0x26 , 0x7C , 0x43 ,
0x00 , 0xC4 , 0x8E , 0x21 , 0x00 , 0x00 , 0x98,
0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 ,
0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x71
};

/* AML EDID JIASHI 15.06.30 */
static unsigned char edid_domy[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x11, 0xB9,
0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x20, 0x19, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78, 0x0A, 0xCF,
0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74, 0x00, 0x30,
0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E, 0x02, 0x3A,
0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x00, 0x00, 0x00, 0xFC, 0x00, 0x44,
0x6F, 0x6D, 0x79, 0x20, 0x54, 0x56, 0x0A, 0x20, 0x20, 0x20,
0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x01, 0x4B,
0x02, 0x03, 0x36, 0xF0, 0x5B, 0x5F, 0x10, 0x1F, 0x14, 0x05,
0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06, 0x01, 0x61,
0x5D, 0x64, 0x65, 0x66, 0x62, 0x60,
0x29, 0x09, 0x07, 0x01, 0x15, 0x07, 0x00, 0x57, 0x06, 0x00,
0x83, 0x01, 0x00, 0x00, 0x67, 0x03,
0x0C, 0x00, 0x10, 0x00, 0x88, 0x3C, 0x02, 0x3A, 0x80, 0xD0,
0x72, 0x38, 0x2D, 0x40, 0x10, 0x2C,
0x45, 0x80, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F, 0x01, 0x1D,
0x00, 0xBC, 0x52, 0xD0, 0x1E, 0x20,
0xB8, 0x28, 0x55, 0x40, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F,
0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0,
0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0x13, 0x8E, 0x21, 0x00,
0x00, 0x18, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x19
};

/* AML EDID for Dolby atmos 0815 */
static unsigned char edid_aud_atmos[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x05, 0xA0, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x20, 0x19, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x41,
0x4D, 0x4C, 0x20, 0x54, 0x56, 0x0A, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x0F,
0x02, 0x03, 0x4B, 0xF0, 0x5B, 0x5F, 0x10, 0x1F,
0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06,
0x01, 0x61, 0x5D, 0x64, 0x65, 0x66, 0x62, 0x60,
0x29, 0x09, 0x07, 0x03, 0x57, 0x06, 0x01, 0x17,
0x07, 0x50, 0x83, 0x01, 0x00, 0x00, 0x6A, 0x03,
0x0C, 0x00, 0x20, 0x00, 0x88, 0x3C, 0x20, 0x80,
0x00, 0x67, 0xD8, 0x5D, 0xC4, 0x01, 0x78, 0x88,
0x01, 0xE5, 0x0F, 0x00, 0x00, 0x90, 0x05, 0xE3,
0x06, 0x05, 0x01, 0x02, 0x3A, 0x80, 0xD0, 0x72,
0x38, 0x2D, 0x40, 0x10, 0x2C, 0x45, 0x80, 0x30,
0xEB, 0x52, 0x00, 0x00, 0x1F, 0x01, 0x1D, 0x00,
0xBC, 0x52, 0xD0, 0x1E, 0x20, 0xB8, 0x28, 0x55,
0x40, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D,
};

/* AML EDID for Dolby atmos 0815 */
static unsigned char edid_aud_truehd[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x05, 0xA0, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x20, 0x19, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x41,
0x4D, 0x4C, 0x20, 0x54, 0x56, 0x0A, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0x0F,
0x02, 0x03, 0x30, 0xF2, 0x4B, 0x01, 0x03, 0x04,
0x05, 0x07, 0x90, 0x12, 0x13, 0x14, 0x16, 0x1F,
0x2C, 0x09, 0x07, 0x07, 0x15, 0x07, 0x50, 0x57,
0x07, 0x01, 0x67, 0x57, 0x01, 0x83, 0x01, 0x00,
0x00, 0x6E, 0x03, 0x0C, 0x00, 0x10, 0x00, 0xF8,
0x3C, 0x20, 0x80, 0x80, 0x01, 0x02, 0x03, 0x04,
0x8C, 0x0A, 0xD0, 0x8A, 0x20, 0xE0, 0x2D, 0x10,
0x10, 0x3E, 0x96, 0x00, 0xC4, 0x8E, 0x21, 0x00,
0x00, 0x18, 0x8C, 0x0A, 0xD0, 0x90, 0x20, 0x40,
0x31, 0x20, 0x0C, 0x40, 0x55, 0x00, 0xC4, 0x8E,
0x21, 0x00, 0x00, 0x18, 0x01, 0x1D, 0x00, 0xBC,
0x52, 0xD0, 0x1E, 0x20, 0xB8, 0x28, 0x55, 0x40,
0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E, 0x01, 0x1D,
0x80, 0xD0, 0x72, 0x1C, 0x16, 0x20, 0x10, 0x2C,
0x25, 0x80, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x9E,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6D
};



unsigned char *edid_list[] = {
	edid_buf,
	aml_edid,
	aml_edid_DD,
	edid_skyworth,
	edid_4K_MSD,
	edid_domy,
	v2_edid,
	edid_aud_atmos,
	edid_aud_truehd,
};

static void dump_state(unsigned char enable);
static void dump_audio_info(unsigned char enable);
static void rx_modify_edid(unsigned char *buffer,
				int len, unsigned char *addition);
static void rx_start_repeater_auth(void);
static void dump_hdcp_data(void);
void eq_algorithm(struct work_struct *work)
{
	unsigned int i;

	cancel_delayed_work(&eq_dwork);
	if (hdmirx_repeat_support()) {
		if (rx.hdcp.hdcp_version && hdmirx_is_key_write()
			&& rx.open_fg) {
			switch_set_state(&rx.hdcp.switch_hdcp_auth, 0);
			switch_set_state(&rx.hdcp.switch_hdcp_auth,
					rx.hdcp.hdcp_version);
			rx.hdcp.hdcp_version = HDCP_VERSION_NONE;
		}
	}
	rx_pr("eq run\n");
	for (i = 0; i < 3; i++) {
		if (SettingFinder() == 1) {
			rx_pr("EQ-%d-%d-%d-",
					eq_ch0.bestsetting,
					eq_ch1.bestsetting,
					eq_ch2.bestsetting);

			if (1 == eq_maxvsmin(eq_ch0.bestsetting,
					eq_ch1.bestsetting,
					eq_ch2.bestsetting)) {
					if (log_level & EQ_LOG)
						rx_pr("pass\n");
					rx.state = FSM_EQ_END;
					break;
			} else {
				if (log_level & EQ_LOG)
					rx_pr("fail\n");
			}

		}
	}
	if (i >= MINMAX_nTrys) {
		eq_ch0.bestsetting = ErrorcableSetting;
		eq_ch1.bestsetting = ErrorcableSetting;
		eq_ch2.bestsetting = ErrorcableSetting;
		rx.state = FSM_EQ_END;
		if (log_level & EQ_LOG)
			rx_pr("EQ fail-retry\n");
	}
	return;
}
bool is_hdcp14_on(void)
{
	return (rx.hdcp.bksv[1] == 0) ? false : true;
}
void rx_hpd_to_esm_handle(struct work_struct *work)
{
	cancel_delayed_work(&esm_dwork);
	switch_set_state(&rx.hpd_sdev, 0x0);
	rx_pr("esm_hpd-0\n");
	mdelay(80);
	switch_set_state(&rx.hpd_sdev, 0x01);
	rx_pr("esm_hpd-1\n");
	return;
}
/**
 * Clock event handler
 * @param[in,out] ctx context information
 * @return error code
 */
#if 0
static long hdmi_rx_ctrl_get_tmds_clk(struct hdmi_rx_ctrl *ctx)
{
	return ctx->tmds_clk;
}
#endif
#if 0
static unsigned char is_3d_sig(void)
{
	if ((rx.vendor_specific_info.identifier == 0x000c03) &&
	    (rx.vendor_specific_info.vd_fmt == 0x2)) {
		return 1;
	}
	return 0;
}
#endif

static int clock_handler(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;

	if (rx.state != FSM_SIG_READY)
		return 0;

	if (sm_pause)
		return 0;

	if (ctx == 0)
		return -EINVAL;

	return error;
}

static int drm_handler(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;

	if ((rx.state != FSM_SIG_READY) || (hdr_enable == false))
		return 0;

	if (sm_pause)
		return 0;

	if (ctx == 0)
		return -EINVAL;

	/* waiting, before send the hdr data to post modules */
	if (rx.hdr_info.hdr_state != HDR_STATE_NULL)
		return -EBUSY;

	rx.hdr_info.hdr_state = HDR_STATE_GET;
	rx.hdr_info.hdr_data.eotf =
		(hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD0) >> 8) & 0xFF;
	rx.hdr_info.hdr_data.metadata_id =
		(hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD0) >> 16) & 0xFF;
	rx.hdr_info.hdr_data.lenght =
		(unsigned char)(hdmirx_rd_dwc(TOP_PDEC_DRM_HB) >> 8);

	rx.hdr_info.hdr_data.primaries[0].x =
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD0) >> 24) & 0xFF) +
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD1) << 8) & 0xFF00);
	rx.hdr_info.hdr_data.primaries[0].y =
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD1) >> 8) & 0xFF) +
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD1) >> 8) & 0xFF00);
	rx.hdr_info.hdr_data.primaries[1].x =
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD1) >> 24) & 0xFF) +
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD2) << 8) & 0xFF00);
	rx.hdr_info.hdr_data.primaries[1].y =
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD2) >> 8) & 0xFF) +
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD2) >> 8) & 0xFF00);
	rx.hdr_info.hdr_data.primaries[2].x =
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD2) >> 24) & 0xFF) +
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD3) << 8) & 0xFF00);
	rx.hdr_info.hdr_data.primaries[2].y =
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD3) >> 8) & 0xFF) +
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD3) >> 8) & 0xFF00);
	rx.hdr_info.hdr_data.white_points.x =
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD3) >> 24) & 0xFF) +
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD4) << 8) & 0xFF00);
	rx.hdr_info.hdr_data.white_points.y =
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD4) >> 8) & 0xFF) +
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD4) >> 8) & 0xFF00);
	rx.hdr_info.hdr_data.master_lum.x =
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD4) >> 24) & 0xFF) +
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD5) << 8) & 0xFF00);
	rx.hdr_info.hdr_data.master_lum.y =
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD5) >> 8) & 0xFF) +
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD5) >> 8) & 0xFF00);
	rx.hdr_info.hdr_data.mcll =
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD5) >> 24) & 0xFF) +
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD6) << 8) & 0xFF00);
	rx.hdr_info.hdr_data.mfall =
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD6) >> 8) & 0xFF) +
		((hdmirx_rd_dwc(TOP_PDEC_DRM_PAYLOAD6) >> 8) & 0xFF00);

	rx.hdr_info.hdr_state = HDR_STATE_SET;

	return error;
}



/*static int md_handler(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;

	if (rx.state != FSM_SIG_READY)
		return 0;

	if (sm_pause)
		return 0;

	if (ctx == 0)
		return -EINVAL;

	if (irq_video_mute_flag == false) {
		irq_video_mute_flag = true;
		hdmirx_set_video_mute(1);

		if (log_level&0x400)
			rx_pr("\nmd_mute\n");
	}
	return error;
}*/

#if 0
static int video_handler(struct hdmi_rx_ctrl *ctx)
{}

static int vsi_handler(void)
{}
#endif
/**
 * Audio event handler
 * @param[in,out] ctx context information
 * @return error code
 */
#if 0
static int audio_handler(struct hdmi_rx_ctrl *ctx)
{}
#endif

static int hdmi_rx_ctrl_irq_handler(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;
	/* unsigned i = 0; */
	uint32_t intr_hdmi = 0;
	uint32_t intr_md = 0;
	uint32_t intr_pedc = 0;
	/* uint32_t intr_aud_clk = 0; */
	uint32_t intr_aud_fifo = 0;
	uint32_t intr_hdcp22 = 0;
	uint32_t intr_aud_cec = 0;

	bool clk_handle_flag = false;
	bool video_handle_flag = false;
	/* bool audio_handle_flag = false; */
	bool vsi_handle_flag = false;
	bool drm_handle_flag = false;

	/* clear interrupt quickly */
	intr_hdmi =
	    hdmirx_rd_dwc(DWC_HDMI_ISTS) &
	    hdmirx_rd_dwc(DWC_HDMI_IEN);
	if (intr_hdmi != 0)
		hdmirx_wr_dwc(DWC_HDMI_ICLR, intr_hdmi);


	intr_md =
	    hdmirx_rd_dwc(DWC_MD_ISTS) &
	    hdmirx_rd_dwc(DWC_MD_IEN);
	if (intr_md != 0)
		hdmirx_wr_dwc(DWC_MD_ICLR, intr_md);


	intr_pedc =
	    hdmirx_rd_dwc(DWC_PDEC_ISTS) &
	    hdmirx_rd_dwc(DWC_PDEC_IEN);
	if (intr_pedc != 0)
		hdmirx_wr_dwc(DWC_PDEC_ICLR, intr_pedc);

	/* intr_aud_clk = hdmirx_rd_dwc(RA_AUD_CLK_ISTS) */
		/* & hdmirx_rd_dwc(RA_AUD_CLK_IEN); */
	/* if (intr_aud_clk != 0) { */
	/* hdmirx_wr_dwc(RA_AUD_CLK_ICLR, intr_aud_clk); */
	/* } */

	intr_aud_fifo =
	    hdmirx_rd_dwc(DWC_AUD_FIFO_ISTS) &
	    hdmirx_rd_dwc(DWC_AUD_FIFO_IEN);
	if (intr_aud_fifo != 0)
		hdmirx_wr_dwc(DWC_AUD_FIFO_ICLR, intr_aud_fifo);

	intr_aud_cec =
			hdmirx_rd_dwc(DWC_AUD_CEC_ISTS) &
			hdmirx_rd_dwc(DWC_AUD_CEC_IEN);
		if (intr_aud_cec != 0)
			hdmirx_wr_dwc(DWC_AUD_CEC_ICLR, intr_aud_cec);

	intr_hdcp22 =
		hdmirx_rd_dwc(DWC_HDMI2_ISTS) &
	    hdmirx_rd_dwc(DWC_HDMI2_IEN);

	if (intr_hdcp22 != 0)
		hdmirx_wr_dwc(DWC_HDMI2_ICLR, intr_hdcp22);

	/* check hdmi open status before dwc isr */
	if (!rx.open_fg) {
		if (log_level & 0x1000)
			rx_pr("[isr] ingore dwc isr ---\n");
		return error;
	}

	if (intr_hdmi != 0) {
		/*if (get(intr_hdmi, CLK_CHANGE) != 0) */
			/* clk_handle_flag = true; */
		if (get(intr_hdmi, AKSV_RCV) != 0) {
			if (log_level & HDCP_LOG)
				rx_pr("[**receive aksv**\n");
			/*clk_handle_flag = true;*/
			is_hdcp_source = true;
			rx.hdcp.hdcp_version = HDCP_VERSION_14;
			if (hdmirx_repeat_support()) {
				queue_delayed_work(eq_wq, &eq_dwork,
						msecs_to_jiffies(5));
				rx_start_repeater_auth();
			}
		}
		if (get(intr_hdmi, DCM_CURRENT_MODE_CHG) != 0) {
			if (log_level & 0x400)
				rx_pr("[isr]DCM_CHG\n");
			video_handle_flag = true;
		}
		/* if (get(intr_hdmi, AKSV_RCV) != 0) { */
		/* if(log_level&0x400) */
		/* rx_print("[HDMIrx isr] AKSV_RCV\n"); */
		/* //execute[hdmi_rx_ctrl_event_aksv_reception] = true; */
		/* } */
		ctx->debug_irq_hdmi++;
	}

	if (intr_md != 0) {
		if (get(intr_md, md_ists_en) != 0) {
			if (log_level & 0x100)
				rx_pr("md_ists:%x\n", intr_md);
			video_handle_flag = true;
		}
		ctx->debug_irq_video_mode++;
	}

	if (intr_hdcp22 != 0) {
		rx.change = skip_frame_num;
		if (get(intr_hdcp22, _BIT(0)) != 0)
			hdcp22_capable_sts = HDCP22_AUTH_STATE_CAPBLE;
		if (get(intr_hdcp22, _BIT(1)) != 0)
			hdcp22_capable_sts = HDCP22_AUTH_STATE_NOT_CAPBLE;
		if (get(intr_hdcp22, _BIT(2)) != 0)
			hdcp22_auth_sts = HDCP22_AUTH_STATE_LOST;
		if (get(intr_hdcp22, _BIT(3)) != 0) {
			hdcp22_auth_sts = HDCP22_AUTH_STATE_SUCCESS;
			if ((intr_hdcp22 & 0x01) == 0) {
				rx.hdcp.hdcp_version = HDCP_VERSION_22;
				rx.hdcp.pass = 1;
			}
		}
		if (get(intr_hdcp22, _BIT(4)) != 0) {
			hdcp22_auth_sts = HDCP22_AUTH_STATE_FAILED;
			if (hdcp22_capable_sts == HDCP22_AUTH_STATE_CAPBLE)
				esm_set_stable(0);
		}
		/* if (get(intr_hdcp22, _BIT(5)) != 0) {
			rx.change = skip_frame_num;
		} */
		if (log_level & HDCP_LOG) {
			rx_pr("intr=%#x\n", intr_hdcp22);
			rx_pr("capble sts = %d\n",
					hdcp22_capable_sts);
			rx_pr("auth sts=%d\n", hdcp22_auth_sts);
		}
	}

	if (intr_pedc != 0) {
		/* hdmirx_wr_dwc(RA_PDEC_ICLR, intr_pedc); */
		if (get(intr_pedc, DVIDET | AVI_CKS_CHG) != 0) {
			if (log_level & 0x400)
				rx_pr("[irq] AVI_CKS_CHG\n");
			video_handle_flag = true;
		}
		if (get(intr_pedc, VSI_CKS_CHG) != 0) {
			if (log_level & 0x400)
				rx_pr("[irq] VSI_CKS_CHG\n");
			vsi_handle_flag = true;
		}
		if (get(intr_pedc, DRM_RCV_EN | DRM_CKS_CHG) != 0) {
			if (log_level & 0x400)
				rx_pr("[irq] DRM_RCV_EN %#x\n", intr_pedc);
				drm_handle_flag = true;
		}
		/* if (get(intr_pedc, AIF_CKS_CHG) != 0) { */
		/* if(log_level&0x400) */
		/* rx_pr("[HDMIrx isr] AIF_CKS_CHG\n"); */
		/* audio_handle_flag = true; */
		/* } */
		/* if (get(intr_pedc, PD_FIFO_NEW_ENTRY) != 0) { */
		/* if(log_level&0x400) */
		/* rx_pr("[HDMIrx isr] PD_FIFO_NEW_ENTRY\n"); */
		/* //execute[hdmi_rx_ctrl_event_packet_reception] = true; */
		/* } */
		if (get(intr_pedc, PD_FIFO_OVERFL) != 0) {
			if (log_level & 0x100)
				rx_pr("[irq] PD_FIFO_OVERFL\n");
			error |= hdmirx_packet_fifo_rst();
		}
		ctx->debug_irq_packet_decoder++;
	}
	/* if (intr_aud_clk != 0) { */
	/* if(log_level&0x400) */
	/* rx_print("[HDMIrx isr] RA_AUD_CLK\n"); */
	/* ctx->debug_irq_audio_clock++; */
	/* } */

	if (intr_aud_fifo != 0) {
		if (get(intr_aud_fifo, OVERFL) != 0) {
			if (log_level & 0x100)
				rx_pr("[irq] OVERFL\n");
			error |= hdmirx_audio_fifo_rst();
			/* rx.change = skip_frame_num; */
		}
		if (get(intr_aud_fifo, UNDERFL) != 0) {
			if (log_level & 0x100)
				rx_pr("[irq] UNDERFL\n");
			error |= hdmirx_audio_fifo_rst();
			/* rx.change = skip_frame_num; */
		}
		ctx->debug_irq_audio_fifo++;
	}

	if (clk_handle_flag)
		clock_handler(ctx);

	/*if (video_handle_flag)
		md_handler(ctx);*/

	/* if (vsi_handle_flag) */
	/*	vsi_handler(); */

	if (drm_handle_flag)
		drm_handler(ctx);

	return error;
}

irqreturn_t irq_handler(int irq, void *params)
{
	int error = 0;
	unsigned long hdmirx_top_intr_stat;
	if (params == 0) {
		rx_pr("%s: %s\n",
			__func__,
			"RX IRQ invalid parameter");
		return IRQ_HANDLED;
	}
	hdmirx_top_intr_stat = hdmirx_rd_top(TOP_INTR_STAT);
reisr:hdmirx_wr_top(TOP_INTR_STAT_CLR, hdmirx_top_intr_stat);
	/* modify interrupt flow for isr loading */
	/* top interrupt handler */
	if (hdmirx_top_intr_stat & (0x7 << 17)) {
		if (hdmirx_top_intr_stat & (0x1 << 17))
			hdmirx_wr_top(TOP_EDID_GEN_STAT,
			hdmirx_rd_top(TOP_EDID_GEN_STAT) | (1 << 16));
		if (hdmirx_top_intr_stat & (0x2 << 17))
			hdmirx_wr_top(TOP_EDID_GEN_STAT_B,
			hdmirx_rd_top(TOP_EDID_GEN_STAT_B) | (1 << 16));
		if (hdmirx_top_intr_stat & (0x4 << 17))
			hdmirx_wr_top(TOP_EDID_GEN_STAT_C,
			hdmirx_rd_top(TOP_EDID_GEN_STAT_C) | (1 << 16));
		edid_addr_intr_flag = true;
		if (log_level & ERR_LOG)
			rx_pr("ddc err-%x",
			(hdmirx_top_intr_stat & (0x7 << 17)));
	}
	#if 0
	if ((hdmirx_top_intr_stat & (0xf << 2)) ||
		(hdmirx_top_intr_stat & (0xf << 6))) {
		/* rx_pr("%s: %s\n", __func__, " enable queue"); */
		queue_delayed_work(hpd_wq, &eq_dwork, msecs_to_jiffies(5));
	}
	#endif
	/* top interrupt handler */
	/* if (hdmirx_top_intr_stat & (0xf << 2)) { */
	/* schedule_work(&rx->plug_wq); */
	/*  rx.tx_5v_status = true; */
	/* if (log_level & 0x400) */
	/* rx_print("[HDMIrx isr] 5v rise\n"); */
	/* } */
	/* if (hdmirx_top_intr_stat & (0xf << 2)) */
	/* if (hdmirx_top_intr_stat & (0xf << 6)) { */
	/* schedule_work(&rx->plug_wq); */
	/*  rx.tx_5v_status = false; */
	/* if (log_level & 0x400) */
	/* rx_print("[HDMIrx isr] 5v fall\n"); */
	/* } */

	if (hdmirx_top_intr_stat & (1 << 15))
		rx_pr("[isr] enc rise---\n");
	else if (hdmirx_top_intr_stat & (1 << 16))
		rx_pr("[isr] enc fall---\n");

	/* must clear ip interrupt quickly */
	if (hdmirx_top_intr_stat & (~(1 << 30))) {
		error = hdmi_rx_ctrl_irq_handler(
				&((struct rx_s *)params)->ctrl);
		if (error < 0) {
			if (error != -EPERM) {
				rx_pr("%s: RX IRQ handler %d\n",
					__func__,
					error);
			}
		}
	}

	/* if (hdmirx_top_intr_stat & (0xf << 6)) */
	/* check the ip interrupt again */

	hdmirx_top_intr_stat = hdmirx_rd_top(TOP_INTR_STAT);
	if (hdmirx_top_intr_stat & (1 << 31)) {
		if (log_level & 0x100)
			rx_pr("[isr] need clear ip irq---\n");
		goto reisr;

	}
	return IRQ_HANDLED;
}


struct sample_rate_info_s {
	unsigned int sample_rate;
	unsigned char aud_info_sf;
	unsigned char channel_status_id;
};

struct sample_rate_info_s sample_rate_info[] = {
	{32000, 0x1, 0x3},
	{44100, 0x2, 0x0},
	{48000, 0x3, 0x2},
	{88200, 0x4, 0x8},
	{96000, 0x5, 0xa},
	{176400, 0x6, 0xc},
	{192000, 0x7, 0xe},
	/* {768000, 0, 0x9}, */
	{0, 0, 0}
};

static int get_real_sample_rate(void)
{
	int i;
	int ret_sample_rate = rx.aud_info.arc;
	for (i = 0; sample_rate_info[i].sample_rate; i++) {
		if (rx.aud_info.arc >
		    sample_rate_info[i].sample_rate) {
			if ((rx.aud_info.arc -
			     sample_rate_info[i].sample_rate) <
			    AUD_SR_RANGE) {
				ret_sample_rate =
				    sample_rate_info[i].sample_rate;
				break;
			}
		} else {
			if ((sample_rate_info[i].sample_rate -
			     rx.aud_info.arc) <
			    AUD_SR_RANGE) {
				ret_sample_rate =
				    sample_rate_info[i].sample_rate;
				break;
			}
		}
	}
	return ret_sample_rate;
}

static unsigned char is_sample_rate_stable(int sample_rate_pre,
					   int sample_rate_cur)
{
	unsigned char ret = 0;
	if (ABS(sample_rate_pre - sample_rate_cur) <
		AUD_SR_RANGE)
		ret = 1;

	return ret;
}

bool hdmirx_hw_check_frame_skip(void)
{
	if ((force_state & 0x10) || (!frame_skip_en))
		return false;

	else if ((rx.state != FSM_SIG_READY) || (rx.change > 0))
		return true;

	return false;
}

int hdmirx_hw_get_color_fmt(void)
{
	int color_format = 0;
	int format = rx.pre.colorspace;
	if (rx.pre.sw_dvi) {
		if (HDMI_640x480p60 == rx.pre.sw_vic ||
			HDMI_640x480p72 == rx.pre.sw_vic ||
			HDMI_640x480p75 == rx.pre.sw_vic)
			format = E_COLOR_RGB;

		if ((HDMI_800_600 <= rx.pre.sw_vic) &&
			(HDMI_1680_1050 >= rx.pre.sw_vic))
			format = E_COLOR_RGB;

		#if (DVI_FIXED_TO_RGB)
			format = E_COLOR_RGB;
		#endif
	}

	if (rx.change > 0)
		return last_color_fmt;

	switch (format) {
	case 1:
		color_format = 3;	/* YUV422 */
		break;
	case 2:
		color_format = 1;	/* YUV444 */
		break;
	case 3:
		color_format = 3;	/* YUV422 */
		/* color_format = 1; */	/* YUV444 */
		break;
	case 0:
	default:
		color_format = 0;	/* RGB444 */
		break;
	}

	last_color_fmt = color_format;

	return color_format;
}

int rx_get_colordepth(void)
{
	int ret = rx.pre.colordepth / 3;
	if (pc_mode_en == 1) {
		if ((rx.pre.sw_vic == HDMI_2160p_60hz_420) ||
			(rx.pre.sw_vic == HDMI_4096p_60hz_420))
			ret = 8;
	}
	return ret;
}

int hdmirx_hw_get_dvi_info(void)
{
	int ret = 0;

	if (rx.pre.sw_dvi)
		ret = 1;

	return ret;
}

int hdmirx_hw_get_3d_structure(unsigned char *_3d_structure,
			       unsigned char *_3d_ext_data)
{
	hdmirx_read_vendor_specific_info_frame(&rx.vendor_specific_info);
	if ((rx.vendor_specific_info.identifier == 0x000c03) &&
	    (rx.vendor_specific_info.vd_fmt == VSI_FORMAT_3D_FORMAT)) {
		*_3d_structure = rx.vendor_specific_info._3d_structure;
		*_3d_ext_data = rx.vendor_specific_info._3d_ext_data;
		return 0;
	}
	return -1;
}

int hdmirx_hw_get_pixel_repeat(void)
{
	return rx.pre.repeat + 1;
}

unsigned char is_frame_packing(void)
{

#if 1
	return rx.pre.sw_fp;
#else
	if ((rx.vendor_specific_info.identifier == 0x000c03) &&
	    (rx.vendor_specific_info.vd_fmt == 0x2) &&
	    (rx.vendor_specific_info._3d_structure == 0x0)) {
		return 1;
	}
	return 0;
#endif
}

unsigned char is_alternative(void)
{
	return rx.pre.sw_alternative;
}



struct freq_ref_s freq_ref[] = {
/* basic format*/
	{HDMI_640x480p60, 0, 25000, 640, 480, 480, 480, 0, 3000},
	{HDMI_480p60, 0, 27000, 720, 480, 1005, 480, 0, 3000},
	{HDMI_480p60_16x9, 0, 27000, 720, 480, 1005, 480, 0, 3000},
	{HDMI_480i60, 1, 27000, 1440, 240, 240, 240, 1, 3000},
	{HDMI_480i60_16x9, 1, 27000, 1440, 240, 240, 240, 1, 3000},
	{HDMI_576p50, 0, 27000, 720, 576, 1201, 576, 0, 2500},
	{HDMI_576p50_16x9, 0, 27000, 720, 576, 1201, 576, 0, 2500},
	{HDMI_576i50, 1, 27000, 1440, 288, 288, 288, 1, 2500},
	{HDMI_576i50_16x9, 1, 27000, 1440, 288, 288, 288, 1, 2500},
	{HDMI_576i50_16x9, 1, 27000, 1440, 145, 145, 145, 2, 2500},
	{HDMI_720p60, 0, 74250, 1280, 720, 1470, 720, 0, 3000},
	{HDMI_720p50, 0, 74250, 1280, 720, 1470, 720, 0, 2500},
	{HDMI_1080i60, 1, 74250, 1920, 540, 2228, 1103, 0, 3000},
	{HDMI_1080i50, 1, 74250, 1920, 540, 2228, 1103, 0, 2500},
	{HDMI_1080p60, 0, 148500, 1920, 1080, 1080, 2160, 0, 3000},
	{HDMI_1080p24, 0, 74250, 1920, 1080, 2205, 2160, 0, 1200},
	{HDMI_1080p25, 0, 74250, 1920, 1080, 2205, 2160, 0, 1250},
	{HDMI_1080p30, 0, 74250, 1920, 1080, 2205, 2160, 0, 1500},
	{HDMI_1080p50, 0, 148500, 1920, 1080, 1080, 2160, 0, 2500},
	/* extend format */
	{HDMI_1440x240p60, 0, 27000, 1440, 240, 240, 240, 1, 3000},
	{HDMI_1440x240p60_16x9, 0, 27000, 1440, 240, 240, 240, 1, 3000},
	{HDMI_2880x480i60, 1, 54000, 2880, 240, 240, 240, 9, 3000},
	{HDMI_2880x480i60_16x9, 1, 54000, 2880, 240, 240, 240, 9, 3000},
	{HDMI_2880x240p60, 0, 54000, 2880, 240, 240, 240, 9, 3000},
	{HDMI_2880x240p60_16x9, 0, 54000, 2880, 240, 240, 240, 9, 3000},
	{HDMI_1440x480p60, 0, 54000, 1440, 480, 480, 480, 9, 3000},
	{HDMI_1440x480p60_16x9, 0, 54000, 1440, 480, 480, 480, 9, 3000},

	{HDMI_1440x288p50, 0, 27000, 1440, 288, 288, 288, 1, 2500},
	{HDMI_1440x288p50_16x9, 0, 27000, 1440, 288, 288, 288, 1, 2500},
	{HDMI_2880x576i50, 1, 54000, 2880, 288, 288, 288, 9, 2500},
	{HDMI_2880x576i50_16x9, 1, 54000, 2880, 288, 288, 288, 9, 2500},
	{HDMI_2880x288p50, 0, 54000, 2880, 288, 288, 288, 9, 2500},
	{HDMI_2880x288p50_16x9, 0, 54000, 2880, 288, 288, 288, 9, 2500},
	{HDMI_1440x576p50, 0, 54000, 1440, 576, 576, 576, 9, 2500},
	{HDMI_1440x576p50_16x9, 0, 54000, 1440, 576, 576, 576, 9, 2500},

	{HDMI_2880x480p60, 0, 108000, 2880, 480, 480, 480, 9, 3000},
	{HDMI_2880x480p60_16x9, 0, 108000, 2880, 480, 480, 480, 9, 3000},
	{HDMI_2880x576p50, 0, 108000, 2880, 576, 576, 576, 9, 2500},
	{HDMI_2880x576p50_16x9, 0, 108000, 2880, 576, 576, 576, 9, 2500},
	{HDMI_1080i50_1250, 1, 72000, 1920, 540, 540, 540, 0, 2500},
	{HDMI_720p24, 0, 74250, 1280, 720, 1470, 720, 0, 1200},
	{HDMI_720p25, 0, 74250, 1280, 720, 1470, 720, 0, 1250},
	{HDMI_720p30, 0, 74250, 1280, 720, 1470, 720, 0, 1500},

/*extend format 100hz,120hz,200hz,240hz*/
	{HDMI_480p60, 0, 54000, 720, 480, 1005, 480, 0, 6000},
	{HDMI_480p60_16x9, 0, 54000, 720, 480, 1005, 480, 0, 6000},
	{HDMI_480i60, 1, 54000, 1440, 240, 240, 240, 1, 6000},
	{HDMI_480i60_16x9, 1, 54000, 1440, 240, 240, 240, 1, 6000},
	{HDMI_480p60, 0, 108000, 720, 480, 1005, 480, 0, 12000},
	{HDMI_480p60_16x9, 0, 108000, 720, 480, 1005, 480, 0, 12000},
	{HDMI_480i60, 1, 108000, 1440, 240, 240, 240, 1, 12000},
	{HDMI_480i60_16x9, 1, 108000, 1440, 240, 240, 240, 1, 12000},

	{HDMI_576p50, 0, 54000, 720, 576, 1201, 576, 0, 5000},
	{HDMI_576p50_16x9, 0, 54000, 720, 576, 1201, 576, 0, 5000},
	{HDMI_576i50, 1, 54000, 1440, 288, 288, 288, 1, 5000},
	{HDMI_576i50_16x9, 1, 54000, 1440, 288, 288, 288, 1, 5000},
	{HDMI_576i50_16x9, 1, 54000, 1440, 145, 145, 145, 2, 5000},
	{HDMI_576p50, 0, 108000, 720, 576, 1201, 576, 0, 10000},
	{HDMI_576p50_16x9, 0, 108000, 720, 576, 1201, 576, 0, 10000},
	{HDMI_576i50, 1, 108000, 1440, 288, 288, 288, 1, 10000},
	{HDMI_576i50_16x9, 1, 108000, 1440, 288, 288, 288, 1, 10000},
	{HDMI_576i50_16x9, 1, 108000, 1440, 145, 145, 145, 2, 10000},

	{HDMI_720p60, 0, 148500, 1280, 720, 1470, 720, 0, 6000},
	{HDMI_720p60, 0, 297000, 1280, 720, 1470, 720, 0, 12000},
	{HDMI_720p50, 0, 148500, 1280, 720, 1470, 720, 0, 5000},
	{HDMI_720p50, 0, 297000, 1280, 720, 1470, 720, 0, 10000},

	{HDMI_1080i60, 1, 148500, 1920, 540, 2228, 1103, 0, 6000},
	{HDMI_1080i60, 1, 297000, 1920, 540, 2228, 1103, 0, 12000},
	{HDMI_1080i50, 1, 148500, 1920, 540, 2228, 1103, 0, 5000},
	{HDMI_1080i50, 1, 297000, 1920, 540, 2228, 1103, 0, 10000},

	{HDMI_1080p60, 0, 297000, 1920, 1080, 1080, 2160, 0, 6000},
	{HDMI_1080p50, 0, 297000, 1920, 1080, 1080, 2160, 0, 5000},

/* vesa format*/
	{HDMI_800_600, 0, 0, 800, 600, 600, 600, 0, 0},
	{HDMI_1024_768, 0, 0, 1024, 768, 768, 768, 0, 0},
	{HDMI_720_400, 0, 0, 720, 400, 400, 400, 0, 0},
	{HDMI_1280_768, 0, 0, 1280, 768, 768, 768, 0, 0},
	{HDMI_1280_800, 0, 0, 1280, 800, 800, 800, 0, 0},
	{HDMI_1280_960, 0, 0, 1280, 960, 960, 960, 0, 0},
	{HDMI_1280_1024, 0, 0, 1280, 1024, 1024, 1024, 0, 0},
	{HDMI_1360_768, 0, 0, 1360, 768, 768, 768, 0, 0},
	{HDMI_1366_768, 0, 0, 1366, 768, 768, 768, 0, 0},
	{HDMI_1600_1200, 0, 0, 1600, 1200, 1200, 1200, 0, 0},
	{HDMI_1600_900, 0, 0, 1600, 900, 900, 900, 0, 0},
	{HDMI_1920_1200, 0, 0, 1920, 1200, 1200, 1200, 0, 0},
	{HDMI_1440_900, 0, 0, 1440, 900, 900, 900, 0, 0},
	{HDMI_1400_1050, 0, 0, 1400, 1050, 1050, 1050, 0, 0},
	{HDMI_1680_1050, 0, 0, 1680, 1050, 1050, 1050, 0, 0},

	/* 4k2k mode */
	{HDMI_3840_2160p, 0, 0, 3840, 2160, 2160, 2160, 0, 0},
	{HDMI_4096_2160p, 0, 0, 4096, 2160, 2160, 2160, 0, 0},

	{HDMI_640x480p72, 0, 30000, 640, 480, 480, 480, 0, 3600},
	{HDMI_640x480p75, 0, 31250, 640, 480, 480, 480, 0, 3750},

	/* 4k2k 420mode hactive = hactive/2 */
	{HDMI_2160p_50hz_420, 0, 0, 1920, 2160, 2160, 2160, 0, 0},
	{HDMI_2160p_60hz_420, 0, 0, 1920, 2160, 2160, 2160, 0, 0},
	{HDMI_4096p_50hz_420, 0, 0, 2048, 2160, 2160, 2160, 0, 0},
	{HDMI_4096p_60hz_420, 0, 0, 2048, 2160, 2160, 2160, 0, 0},
	{HDMI_1080p60, 0, 74250, 960, 1080, 1080, 1080, 0, 3000},
	{HDMI_2560_1440, 0, 312951, 2560, 1440, 3488, 2986, 0, 0},

	/* for AG-506 */
	{HDMI_480p60, 0, 27000, 720, 483, 483, 483, 0, 3000},
	{0, 0, 0, 0, 0, 0, 0, 0, 0}
};

enum tvin_sig_fmt_e hdmirx_hw_get_fmt(void)
{
	/* to do:
	   TVIN_SIG_FMT_HDMI_1280x720P_24Hz_FRAME_PACKING,
	   TVIN_SIG_FMT_HDMI_1280x720P_30Hz_FRAME_PACKING,

	   TVIN_SIG_FMT_HDMI_1920x1080P_24Hz_FRAME_PACKING,
	   TVIN_SIG_FMT_HDMI_1920x1080P_30Hz_FRAME_PACKING, // 150
	 */
	enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
	unsigned int vic = rx.pre.sw_vic;

	if (force_vic)
		vic = force_vic;


	switch (vic) {
		/* basic format */
	case HDMI_640x480p60:	/*1 */
		fmt = TVIN_SIG_FMT_HDMI_640X480P_60HZ;
		break;
	case HDMI_480p60:	/*2 */
	case HDMI_480p60_16x9:	/*3 */
		if (is_frame_packing())
			fmt = TVIN_SIG_FMT_HDMI_720X480P_60HZ_FRAME_PACKING;
		else
			fmt = TVIN_SIG_FMT_HDMI_720X480P_60HZ;
		break;
	case HDMI_720p60:	/*4 */
		if (is_frame_packing())
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_60HZ_FRAME_PACKING;
		else
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_60HZ;
		break;
	case HDMI_1080i60:	/*5 */
		if (is_frame_packing())
			fmt = TVIN_SIG_FMT_HDMI_1920X1080I_60HZ_FRAME_PACKING;
		else if (is_alternative())
			fmt = TVIN_SIG_FMT_HDMI_1920X1080I_60HZ_ALTERNATIVE;
		else
			fmt = TVIN_SIG_FMT_HDMI_1920X1080I_60HZ;
		break;
	case HDMI_480i60:	/*6 */
	case HDMI_480i60_16x9:	/*7 */
		fmt = TVIN_SIG_FMT_HDMI_1440X480I_60HZ;
		break;
	case HDMI_1080p60:	/*16 */
		fmt = TVIN_SIG_FMT_HDMI_1920X1080P_60HZ;
		if (is_alternative() && (rx.pre.colorspace == 3))
			fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
		break;
	case HDMI_1080p24:	/*32 */
		if (is_frame_packing())
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_24HZ_FRAME_PACKING;
		else if (is_alternative()) {
			if (rx.pre.colorspace == 3)
				fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
			else
				fmt =
				TVIN_SIG_FMT_HDMI_1920X1080P_24HZ_ALTERNATIVE;
		} else
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_24HZ;
		break;
	case HDMI_576p50:	/*17 */
	case HDMI_576p50_16x9:	/*18 */
		if (is_frame_packing())
			fmt = TVIN_SIG_FMT_HDMI_720X576P_50HZ_FRAME_PACKING;
		else
			fmt = TVIN_SIG_FMT_HDMI_720X576P_50HZ;
		break;
	case HDMI_720p50:	/*19 */
		if (is_frame_packing())
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_50HZ_FRAME_PACKING;
		else
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_50HZ;
		break;
	case HDMI_1080i50:	/*20 */
		if (is_frame_packing())
			fmt = TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_FRAME_PACKING;
		else if (is_alternative())
			fmt = TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_ALTERNATIVE;
		else
			fmt = TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_A;
		break;
	case HDMI_576i50:	/*21 */
	case HDMI_576i50_16x9:	/*22 */
		fmt = TVIN_SIG_FMT_HDMI_1440X576I_50HZ;
		break;
	case HDMI_1080p50:	/*31 */
		fmt = TVIN_SIG_FMT_HDMI_1920X1080P_50HZ;
		if (is_alternative() && (rx.pre.colorspace == 3))
			fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
		break;
	case HDMI_1080p25:	/*33 */
		if (is_alternative() && (rx.pre.colorspace == 3))
			fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
		else
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_25HZ;
		break;
	case HDMI_1080p30:	/*34 */
		if (is_frame_packing())
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_30HZ_FRAME_PACKING;
		else if (is_alternative()) {
			if (rx.pre.colorspace == 3)
				fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
			else
				fmt =
				TVIN_SIG_FMT_HDMI_1920X1080P_30HZ_ALTERNATIVE;
		} else
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_30HZ;
		break;
	case HDMI_720p24:	/*60 */
		if (is_frame_packing())
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_24HZ_FRAME_PACKING;
		else
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_24HZ;
		break;
	case HDMI_720p25:	/*61 */
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_25HZ;
		break;
	case HDMI_720p30:	/*62 */
		if (is_frame_packing())
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_30HZ_FRAME_PACKING;
		else
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_30HZ;
		break;

		/* extend format */
	case HDMI_1440x240p60:
	case HDMI_1440x240p60_16x9:
		fmt = TVIN_SIG_FMT_HDMI_1440X240P_60HZ;
		break;
	case HDMI_2880x480i60:
	case HDMI_2880x480i60_16x9:
		fmt = TVIN_SIG_FMT_HDMI_2880X480I_60HZ;
		break;
	case HDMI_2880x240p60:
	case HDMI_2880x240p60_16x9:
		fmt = TVIN_SIG_FMT_HDMI_2880X240P_60HZ;
		break;
	case HDMI_1440x480p60:
	case HDMI_1440x480p60_16x9:
		fmt = TVIN_SIG_FMT_HDMI_1440X480P_60HZ;
		break;
	case HDMI_1440x288p50:
	case HDMI_1440x288p50_16x9:
		fmt = TVIN_SIG_FMT_HDMI_1440X288P_50HZ;
		break;
	case HDMI_2880x576i50:
	case HDMI_2880x576i50_16x9:
		fmt = TVIN_SIG_FMT_HDMI_2880X576I_50HZ;
		break;
	case HDMI_2880x288p50:
	case HDMI_2880x288p50_16x9:
		fmt = TVIN_SIG_FMT_HDMI_2880X288P_50HZ;
		break;
	case HDMI_1440x576p50:
	case HDMI_1440x576p50_16x9:
		fmt = TVIN_SIG_FMT_HDMI_1440X576P_50HZ;
		break;

	case HDMI_2880x480p60:
	case HDMI_2880x480p60_16x9:
		fmt = TVIN_SIG_FMT_HDMI_2880X480P_60HZ;
		break;
	case HDMI_2880x576p50:
	case HDMI_2880x576p50_16x9:
		fmt = TVIN_SIG_FMT_HDMI_2880X576P_50HZ;
		break;
	case HDMI_1080i50_1250:
		fmt = TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_B;
		break;
	case HDMI_1080I120:	/*46 */
		fmt = TVIN_SIG_FMT_HDMI_1920X1080I_120HZ;
		break;
	case HDMI_720p120:	/*47 */
		fmt = TVIN_SIG_FMT_HDMI_1280X720P_120HZ;
		break;
	case HDMI_1080p120:	/*63 */
		fmt = TVIN_SIG_FMT_HDMI_1920X1080P_120HZ;
		break;

		/* vesa format */
	case HDMI_800_600:	/*65 */
		fmt = TVIN_SIG_FMT_HDMI_800X600_00HZ;
		break;
	case HDMI_1024_768:	/*66 */
		fmt = TVIN_SIG_FMT_HDMI_1024X768_00HZ;
		break;
	case HDMI_720_400:
		fmt = TVIN_SIG_FMT_HDMI_720X400_00HZ;
		break;
	case HDMI_1280_768:
		fmt = TVIN_SIG_FMT_HDMI_1280X768_00HZ;
		break;
	case HDMI_1280_800:
		fmt = TVIN_SIG_FMT_HDMI_1280X800_00HZ;
		break;
	case HDMI_1280_960:
		fmt = TVIN_SIG_FMT_HDMI_1280X960_00HZ;
		break;
	case HDMI_1280_1024:
		fmt = TVIN_SIG_FMT_HDMI_1280X1024_00HZ;
		break;
	case HDMI_1360_768:
		fmt = TVIN_SIG_FMT_HDMI_1360X768_00HZ;
		break;
	case HDMI_1366_768:
		fmt = TVIN_SIG_FMT_HDMI_1366X768_00HZ;
		break;
	case HDMI_1600_1200:
		fmt = TVIN_SIG_FMT_HDMI_1600X1200_00HZ;
		break;
	case HDMI_1600_900:
		fmt = TVIN_SIG_FMT_HDMI_1600X900_60HZ;
		break;
	case HDMI_1920_1200:
		fmt = TVIN_SIG_FMT_HDMI_1920X1200_00HZ;
		break;
	case HDMI_1440_900:
		fmt = TVIN_SIG_FMT_HDMI_1440X900_00HZ;
		break;
	case HDMI_1400_1050:
		fmt = TVIN_SIG_FMT_HDMI_1400X1050_00HZ;
		break;
	case HDMI_1680_1050:
		fmt = TVIN_SIG_FMT_HDMI_1680X1050_00HZ;
		break;
	case HDMI_640x480p72:
		fmt = TVIN_SIG_FMT_HDMI_640X480P_72HZ;
		break;
	case HDMI_640x480p75:
		fmt = TVIN_SIG_FMT_HDMI_640X480P_75HZ;
		break;
		/* 4k2k mode */
	case HDMI_3840_2160p:
	case HDMI_2160p_50hz_420:
	case HDMI_2160p_60hz_420:
		fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
		break;
	case HDMI_4096_2160p:
	case HDMI_4096p_50hz_420:
	case HDMI_4096p_60hz_420:
		fmt = TVIN_SIG_FMT_HDMI_4096_2160_00HZ;
		break;

	case HDMI_2560_1440:
		fmt = TVIN_SIG_FMT_HDMI_1920X1200_00HZ;
		break;

	default:
		break;
	}

	return fmt;
}

bool hdmirx_hw_pll_lock(void)
{
	if (rx.state == FSM_SIG_READY)
		return true;
	else
		return false;
}

bool hdmirx_hw_is_nosig(void)
{
	return rx.no_signal;
}

/*
 * check timing info
 */
static bool is_timing_stable(void)
{
	bool ret = true;

	if ((abs(rx.cur.hactive - rx.pre.hactive) > diff_pixel_th) &&
		(stable_check_lvl & 1)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("hactive(%d=>%d),",
				rx.pre.hactive,
				rx.cur.hactive);
	}
	if ((abs(rx.cur.vactive - rx.pre.vactive) > diff_line_th) &&
		(stable_check_lvl & 2)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("vactive(%d=>%d),",
				rx.pre.hactive,
				rx.cur.hactive);
	}
	if ((abs(rx.cur.htotal - rx.pre.htotal) > diff_pixel_th) &&
		(stable_check_lvl & 4)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("htotal(%d=>%d),",
				rx.pre.htotal,
				rx.cur.htotal);
	}
	if ((abs(rx.cur.vtotal - rx.pre.vtotal) > diff_line_th) &&
		(stable_check_lvl & 8)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("vtotal(%d=>%d),",
				rx.pre.vtotal,
				rx.cur.vtotal);
	}
	if ((rx.pre.colorspace != rx.cur.colorspace) &&
		(stable_check_lvl & 0x10)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("colorspace(%d=>%d),",
				rx.pre.colorspace,
				rx.cur.colorspace);
	}
	if ((abs(rx.pre.refresh_rate - rx.cur.refresh_rate) > diff_frame_th) &&
		(stable_check_lvl & 0x20)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("refresh_rate(%d=>%d),",
				rx.pre.refresh_rate,
				rx.cur.refresh_rate);
	}
	if ((rx.pre.repeat != rx.cur.repeat) &&
		(stable_check_lvl & 0x40)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("repeat(%d=>%d),",
				rx.pre.repeat,
				rx.cur.repeat);
	}
	if ((rx.pre.hw_dvi != rx.cur.hw_dvi) &&
		(stable_check_lvl & 0x80)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("dvi(%d=>%d),",
				rx.pre.hw_dvi,
				rx.cur.hw_dvi);
	}
	if ((rx.pre.interlaced != rx.cur.interlaced) &&
		(stable_check_lvl & 0x100)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("interlaced(%d=>%d),",
				rx.pre.interlaced,
				rx.cur.interlaced);
	}
	if ((ret == false) & (log_level & VIDEO_LOG))
		rx_pr("\n");
	return ret;
}

static bool is_hdcp_enc_stable(void)
{
	bool ret = true;
	if ((rx.pre.hdcp_enc_state != rx.cur.hdcp_enc_state) &&
		(rx.pre.hdcp_type == E_HDCP14) &&
		(stable_check_lvl & 0x200)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("hdcp_enc_state(%d=>%d)\n",
				rx.pre.hdcp_enc_state,
				rx.cur.hdcp_enc_state);
	}
	return ret;
}

static int get_timing_fmt(void)
{
	int i;
	rx.pre.sw_vic = 0;
	rx.pre.sw_dvi = 0;
	rx.pre.sw_fp = 0;
	rx.pre.sw_alternative = 0;
	frame_rate = rx.pre.refresh_rate * 2;

	for (i = 0; i < sizeof(freq_ref)/sizeof(struct freq_ref_s); i++) {
		if ((abs(rx.pre.hactive - freq_ref[i].hactive)
				<= diff_pixel_th)
			&& (freq_ref[i].interlace == rx.pre.interlaced)
			&& ((abs(rx.pre.refresh_rate - freq_ref[i].frame_rate)
				<= diff_frame_th) ||
				(freq_ref[i].frame_rate == 0))) {
			if ((abs(rx.pre.vactive - freq_ref[i].vactive_lines)
					<= diff_line_th) ||
				(abs(rx.pre.vactive - freq_ref[i].vactive_fp)
					<= diff_line_th))
				break;
			if ((abs(rx.pre.vactive - freq_ref[i].vactive_alt)
					<= diff_line_th) &&
				(rx.pre.colorspace != E_COLOR_YUV420))
				break;
		}
	}
	if (i == sizeof(freq_ref)/sizeof(struct freq_ref_s))
		return i;

	rx.pre.sw_vic = freq_ref[i].vic;
	rx.pre.sw_dvi = rx.pre.hw_dvi;

	if ((freq_ref[i].vactive_lines != freq_ref[i].vactive_fp)
		&& (abs(rx.pre.vactive - freq_ref[i].vactive_fp)
			<= diff_line_th))
		rx.pre.sw_fp = 1;
	else if ((freq_ref[i].vactive_lines != freq_ref[i].vactive_alt)
		&& (abs(rx.pre.vactive - freq_ref[i].vactive_alt)
			<= diff_line_th))
		rx.pre.sw_alternative = 1;
	/*********** repetition Check patch start ***********/
	if (repeat_check) {
		if (freq_ref[i].repeat != 9) {
			if (rx.pre.repeat != freq_ref[i].repeat) {
				if (log_level & PACKET_LOG) {
						rx_pr("\n repeat err");
						rx_pr("%d:%d(standard)",
							rx.pre.repeat,
							freq_ref[i].repeat);
					}
				rx.pre.repeat = freq_ref[i].repeat;
			}
		}
	}
	/************ repetition Check patch end ************/
	if (log_level & 0x200) {
		rx_pr("sw_vic:%d,",
			rx.pre.sw_vic);
		rx_pr("hw_vic:%d,",
			rx.pre.hw_vic);
		rx_pr("frame_rate:%d\n",
			frame_rate);
	}
	return i;
}

void update_hpd_sts(int pwr_sts)
{
	int cur_sts, hpd_sts;

	if (is_meson_gxtvbb_cpu())
		return;

	if (rx.port > 4)
		hpd_sts = ~pwr_sts & 0xf;
	else {
		hpd_sts = ~pwr_sts & 0xf;
		cur_sts = hdmirx_rd_top(TOP_HPD_PWR5V) & (1 << rx.port);
		hpd_sts = (hpd_sts & (~(1 << rx.port))) | cur_sts;
	}
	hdmirx_wr_top(TOP_HPD_PWR5V, hpd_sts | _BIT(4));
	if (log_level & VIDEO_LOG)
		rx_pr("set top_hpd->%x\n", hpd_sts);
}


/*
 * init audio information
 */
static void audio_status_init(void)
{
	audio_sample_rate = 0;
	audio_coding_type = 0;
	audio_channel_count = 0;
	auds_rcv_sts = 0;
}

static void Signal_status_init(void)
{
	hpd_wait_cnt = 0;
	sig_pll_unlock_cnt = 0;
	sig_pll_lock_cnt = 0;
	sig_unstable_cnt = 0;
	sig_stable_cnt = 0;
	sig_lost_lock_cnt = 0;
	sig_stable_cnt = 0;
	sig_unstable_cnt = 0;
	sig_unready_cnt = 0;
	sig_unstable_reset_hpd_cnt = 0;
	/*wait_no_sig_cnt = 0;*/
	/* rx.no_signal = false; */
	rx.pre_state = 0;
}

/* ---------------------------------------------------------- */
/* func:         port A,B,C,D  hdmitx-5v monitor & HPD control */
/* note:         G9TV portD no used */
/* ---------------------------------------------------------- */
void rx_5v_det(void)
{
	uint32_t tmp_5v = 0;
	static int check_cnt;
	if (force_hdmi_5v_high)
		rx.cur_5v_sts = 1;
	else
		rx.cur_5v_sts = (pwr_sts >> rx.port) & 1;
	if (auto_switch_off)
		tmp_5v = 0x0f;
	else
		tmp_5v = (hdmirx_rd_top(TOP_HPD_PWR5V) >> 20) & 0xf;
	if (tmp_5v == pwr_sts)
		return;
	else {
		check_cnt++;
		if (check_cnt < pow5v_max_cnt)
			return;
	}
	check_cnt = 0;
	pwr_sts = tmp_5v;
	update_hpd_sts(pwr_sts);
	rx_pr("hotplg-%x", pwr_sts);
	hdmirx_wait_query();
}

void hdmirx_error_count_config(void)
{
}

bool hdmirx_tmds_pll_lock(void)
{
	if ((hdmirx_rd_dwc(0x30) & 1) == 1)
		return true;
	else
		return false;

}

bool hdmirx_audio_pll_lock(void)
{
	if (hw_dbg_en)
		return true;

	if ((hdmirx_rd_dwc(DWC_AUD_PLL_CTRL) & (1 << 31)) == 0)
		return false;
	else
		return true;
}

bool is_clk_stable(void)
{
	int clk;
	clk = hdmirx_rd_phy(PHY_MAINFSM_STATUS1);
	clk = (clk >> 8) & 1;
	if (1 == clk)
		return true;
	else
		return false;
}

void rx_aud_pll_ctl(bool en)
{
	int tmp = 0;
	if (en) {
		tmp = hdmirx_rd_top(TOP_ACR_CNTL_STAT) | (1<<11);
		hdmirx_wr_top(TOP_ACR_CNTL_STAT, tmp);
		tmp = hdmirx_rd_phy(PHY_MAINFSM_STATUS1);
		/* audio pll div depends on input freq */
		/* wr_reg(HHI_AUD_PLL_CNTL6, (tmp >> 9 & 3) << 28); */
		/* audio pll div fixed to N/CTS as below*/
		wr_reg(HHI_AUD_PLL_CNTL6, 0x40000000);
		wr_reg(HHI_AUD_PLL_CNTL5, 0x0000002e);
		wr_reg(HHI_AUD_PLL_CNTL4, 0x30000000);
		wr_reg(HHI_AUD_PLL_CNTL3, 0x00000000);
		wr_reg(HHI_AUD_PLL_CNTL, 0x40000000);
		wr_reg(HHI_ADC_PLL_CNTL4, 0x805);
		tmp = hdmirx_rd_top(TOP_ACR_CNTL_STAT) | (1<<11);
		hdmirx_wr_top(TOP_ACR_CNTL_STAT, tmp);
		#if 0
		if (use_audioresample_reset) {
			aml_write_cbus(AUD_RESAMPLE_CTRL0,
				aml_read_cbus(AUD_RESAMPLE_CTRL0)
					| (1 << 31));
			aml_write_cbus(AUD_RESAMPLE_CTRL0,
				aml_read_cbus(AUD_RESAMPLE_CTRL0) &
					0x7fffffff);
			aml_write_cbus(AUD_RESAMPLE_CTRL0,
				aml_read_cbus(AUD_RESAMPLE_CTRL0)
				| (1 << 29)
				| (1 << 28));
		}
		#endif
	} else{
		/* disable pll, into reset mode */
		wr_reg(HHI_AUD_PLL_CNTL, 0x20000000);
		if (use_audioresample_reset) {
			/* reset resample module */
			aml_write_cbus(AUD_RESAMPLE_CTRL0,
				aml_read_cbus(AUD_RESAMPLE_CTRL0)
					| (1 << 31));
			aml_write_cbus(AUD_RESAMPLE_CTRL0,
				aml_read_cbus(AUD_RESAMPLE_CTRL0) &
					0x7fffffff);
			aml_write_cbus(AUD_RESAMPLE_CTRL0,
				aml_read_cbus(AUD_RESAMPLE_CTRL0)
				| (1 << 29)
				| (1 << 28));
		}
	}
}

void hdmirx_sw_reset(int level)
{
	unsigned long data32 = 0;
	if (level == 1) {
		data32 |= 0 << 7;	/* [7]vid_enable */
		data32 |= 0 << 5;	/* [5]cec_enable */
		data32 |= 0 << 4;	/* [4]aud_enable */
		data32 |= 0 << 3;	/* [3]bus_enable */
		data32 |= 0 << 2;	/* [2]hdmi_enable */
		data32 |= 1 << 1;	/* [1]modet_enable */
		data32 |= 0 << 0;	/* [0]cfg_enable */

	} else if (level == 2) {
		data32 |= 0 << 7;	/* [7]vid_enable */
		data32 |= 0 << 5;	/* [5]cec_enable */
		data32 |= 1 << 4;	/* [4]aud_enable */
		data32 |= 0 << 3;	/* [3]bus_enable */
		data32 |= 1 << 2;	/* [2]hdmi_enable */
		data32 |= 1 << 1;	/* [1]modet_enable */
		data32 |= 0 << 0;	/* [0]cfg_enable */
	}
	hdmirx_wr_dwc(DWC_DMI_SW_RST, data32);
}

void hdmirx_hdcp22_reauth(void)
{
	if (hdcp22_reauth_enable)
		esm_auth_fail_en = 1;
	else {
		hdmirx_set_hpd(rx.port, 0);
		rx.state = FSM_HPD_LOW;
	}
}

void monitor_capable_sts(void)
{
	/*if the auth lost after the success of authentication*/
	if ((HDCP22_AUTH_STATE_CAPBLE == hdcp22_capable_sts) &&
		(HDCP22_AUTH_STATE_LOST == hdcp22_auth_sts)) {
		hdcp22_lost_cnt++;
		if ((hdcp22_lost_cnt > hdcp22_lost_max) &&
			(do_link_lost_reset)) {
			hdcp22_lost_cnt = 0;
			hdcp22_auth_sts = HDCP22_AUTH_STATE_LOST_RST;
			hdmirx_hdcp22_reauth();
			rx_pr("\n auth lost force hpd rst\n");
		}
	} else {
		if (hdcp22_lost_cnt > 0)
			hdcp22_lost_cnt--;
	}
}

void monitor_cable_clk_sts(void)
{
	static bool pre_sts = 0xff;
	bool sts = is_clk_stable();
	if (pre_sts != sts) {
		if (log_level & VIDEO_LOG)
			rx_pr("\nclk stable = %d\n", sts);
		pre_sts = sts;
	}
}
void rx_dwc_reset(void)
{
	if (log_level & VIDEO_LOG)
		rx_pr("rx_dwc_reset\n");
	/* audio_status_init(); */
	/* Signal_status_init(); */
	hdmirx_audio_fifo_rst();
	hdmirx_packet_fifo_rst();
	hdmirx_sw_reset(2);
}

void set_scdc_cfg(int hpdlow, int pwrprovided)
{
	hdmirx_wr_dwc(DWC_SCDC_CONFIG,
		(hpdlow << 1) | (pwrprovided << 0));
}

int get_cur_hpd_sts(void)
{
	int tmp;
	tmp = hdmirx_rd_top(TOP_HPD_PWR5V) & (1 << rx.port);
	if (!is_meson_gxtvbb_cpu())
		tmp = (tmp == 0) ? 1 : 0;
	return tmp;
}

bool hdmirx_tmds_6g(void)
{
	return (rx.scdc_tmds_cfg >= scdc_tmds_try_max) ||
		((hdmirx_rd_dwc(DWC_SCDC_REGS0) >> 17) & 1);
}

bool hdmirx_tmds_34g_max(void)
{
	return rx.scdc_tmds_cfg >= (scdc_tmds_try_max + 1);
}

#ifdef HDCP22_ENABLE
void esm_reboot_func(void)
{
	if (esm_reboot_lvl == 1) {
		hdmirx_set_hpd(rx.port, 0);
		hdcp22_on = 1;
		hdcp22_kill_esm = 1;
		mdelay(wait_hdcp22_cnt1);
		hdcp22_kill_esm = 0;
		mdelay(wait_hdcp22_cnt2);
		hpd_to_esm = 0;
		do_esm_rst_flag = 1;
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x0);
		hdmirx_hdcp22_esm_rst();
		mdelay(loadkey_22_hpd_delay);
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL,
			0x1000);
		hdmirx_hw_config();
		hpd_to_esm = 1;
		rx.state = FSM_HPD_HIGH;
	} else if (esm_reboot_lvl == 2) {
		hdmirx_set_hpd(rx.port, 0);
		hdcp22_on = 1;
		hdcp22_kill_esm = 1;
		mdelay(wait_hdcp22_cnt1);
		hdcp22_kill_esm = 0;
		mdelay(wait_hdcp22_cnt2);
		hpd_to_esm = 0;
		do_esm_rst_flag = 1;
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x0);
		mdelay(loadkey_22_hpd_delay);
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL,
			0x1000);
		hdmirx_hw_config();
		hpd_to_esm = 1;
		rx.state = FSM_HPD_HIGH;
	}
	rx_pr("esm_reboot_func\n");
}
void hdmirx_esm_hw_fault_detect(void)
{
	/* detect hdcp2.2 esm status */
	if (((rx_hdcp22_rd(0x60)>>31) == 1)
		&& (esm_err_force_14 == 0)) {
		if (reboot_esm_done == 0) {
			esm_reboot_func();
			reboot_esm_done = 1;
			rx_pr("esm reboot done\n");
		} else {
			hdmirx_wr_dwc(0x81c,
				0x2);
			esm_err_force_14 = 1;
			rx_pr("force 1.4-0\n");
		}
	}

	if ((reboot_esm_done == 1) && (esm_err_force_14 == 0) &&
		(rx_hdcp22_rd(0x60) == 0)) {
		if ((hdcp22_auth_sts == 0xff) &&
			(hdcp22_capable_sts == 0xff)) {
			hdmirx_wr_dwc(0x81c,
				0x2);
			esm_err_force_14 = 1;
			rx_pr("force 1.4-1\n");
		}
	}
}
#endif

void esm_set_stable(bool stable)
{
	if (log_level & VIDEO_LOG)
		rx_pr("esm set stable:%d\n", stable);
	video_stable_to_esm = stable;
}

void esm_rst_monitor(void)
{
	static int esm_rst_cnt;
	if (video_stable_to_esm == 0) {
		if (esm_rst_cnt++ > hdcp22_reset_max) {
			if (log_level & HDCP_LOG)
				rx_pr("esm=1\n");
			esm_set_stable(1);
			esm_rst_cnt = 0;
		}
	}
}

bool hdcp_14_auth_success(void)
{
	if ((((hdmirx_rd_dwc(DWC_HDCP_STS) >> 8) & 3) >= 2) &&
		((hdmirx_rd_dwc(DWC_HDCP_DBG) & 0xFF) > 0) &&
		(((hdmirx_rd_dwc(DWC_HDCP_DBG)>>16) & 0x3F) >= 0x10)) {
		if (log_level & VIDEO_LOG)
			rx_pr("HDCP 1.4 auth pass\n");
		return TRUE;
	} else {
		if (log_level & VIDEO_LOG)
			rx_pr("HDCP 1.4 auth encryption:%d,dbg:%#x\n",
			((hdmirx_rd_dwc(DWC_HDCP_STS) >> 8) & 3),
			hdmirx_rd_dwc(DWC_HDCP_DBG));
		return FALSE;
	}
}

void hdmirx_hw_monitor(void)
{
	int pre_sample_rate;
	int tmp;

	if (clk_debug)
		monitor_cable_clk_sts();

	if (mute_kill_en) {
		hdmirx_wr_bits_dwc(DWC_HDMI_VM_CFG_CH2, _BIT(16), 1);
		mute_kill_en = 0;
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 2);
		if (log_level & VIDEO_LOG)
			rx_pr("****esm kill mute****\n");
	}
	if (rx.state > FSM_WAIT_CLK_STABLE) {
		if (hdmirx_phy_clk_rate_monitor()) {
			rx.state = FSM_WAIT_CLK_STABLE;
			rx_pr("clk rate changed, try EQ again\n");
		}
	}

	if (sm_pause)
		return;
	#ifdef HDCP22_ENABLE
	if (esm_error_flag && hdcp22_on && hdcp22_firmware_ok_flag) {
		esm_error_flag = 0;
		queue_delayed_work(esm_wq,
				&esm_dwork, msecs_to_jiffies(1));
		hdmirx_set_hpd(rx.port, 0);
		rx.state = FSM_HPD_LOW;
		rx_pr("esm err->FSM_HDMI5V_HIGH\n");
	}
	if ((hdcp22_on) && (rx.state > FSM_SIG_UNSTABLE)) {
		monitor_capable_sts();
		esm_rst_monitor();
	}
	#endif

	if (rx.cur_5v_sts == 0) {
		if (rx.state != FSM_INIT) {
			rx_pr("5v_lost->FSM_INIT\n");
			pre_port = E_5V_LOST;
			use_dwc_reset = true;
			set_scdc_cfg(1, 0);
			hdmirx_audio_enable(0);
			hdmirx_set_hpd(rx.port, 0);
			hdmirx_audio_fifo_rst();
			rx_aud_pll_ctl(0);
			is_hdcp_source = true;
			rx.no_signal = true;
			rx.state = FSM_INIT;
			#ifdef HDCP22_ENABLE
			if (hdcp22_on)
				esm_set_stable(0);
			#endif
		}

		return;
	} else {
		if (rx.state != FSM_SIG_READY) {
			if (wait_no_sig_cnt >= wait_no_sig_max)
				rx.no_signal = true;
			else {
				wait_no_sig_cnt++;
				if (rx.no_signal)
					rx.no_signal = false;
			}
		}
	}
	switch (rx.state) {
	case FSM_INIT:
		if (reset_sw)
			hdmirx_hw_config();
			/* hdmi_rx_ctrl_edid_update(); */
		rx.state = FSM_HPD_LOW;
		rx.pre_state = FSM_INIT;
		rx_pr("INIT->5V_LOW\n");
		break;
	case FSM_HPD_LOW:
		/* set_scdc_cfg(1, 1); */
		use_dwc_reset = true;
		hdmirx_set_hpd(rx.port, 0);
		audio_status_init();
		Signal_status_init();
		hdmirx_phy_init(rx.port, 0);
		rx.state = FSM_HPD_HIGH;
		rx_pr("HPD_LOW\n");
		break;
	case FSM_HPD_HIGH:
		hpd_wait_cnt++;
		if ((0 == get_cur_hpd_sts()) &&
			(hpd_wait_cnt <= hpd_wait_max))
			break;
		if (rx.boot_flag || do_hpd_reset_flag) {
			if (hpd_wait_cnt <= hpd_wait_max*10)
				break;
			rx.boot_flag = FALSE;
		}
		hpd_wait_cnt = 0;
		rx.scdc_tmds_cfg = 0;
		pre_port = rx.port;
		hdmirx_set_hpd(rx.port, 1);
		set_scdc_cfg(0, 1);
		rx.state = FSM_WAIT_CLK_STABLE;
		rx_pr("HPD_HIGH\n");
		break;
	case FSM_WAIT_CLK_STABLE:
		if (is_clk_stable()) {
			if (is_clk_stable_cnt++ > is_clk_stable_max) {
				rx_pr("EQ_INIT\n");
				rx.state = FSM_EQ_INIT;
				wait_clk_stable_cnt = 0;
				is_clk_stable_cnt = 0;
			}
			break;
		}
		is_clk_stable_cnt = 0;
		wait_clk_stable_cnt++;
		if (wait_clk_stable_cnt == wait_clk_stable_max) {
			hdmirx_phy_init(rx.port, 0);
			if (log_level & VIDEO_LOG)
				rx_pr("FSM_WAIT_CLK_STABLE phy init\n");
			break;
		}
		if (wait_clk_stable_cnt >= wait_clk_stable_max*2) {
			rx.state = FSM_HPD_LOW;
			/*hdmirx_set_hpd(rx.port, 0);*/
			pre_port = E_HPD_RESET;
			wait_clk_stable_cnt = 0;
			if (log_level & VIDEO_LOG)
				rx_pr("FSM_WAIT_CLK_STABLE send hpd\n");
			break;
		}
		break;
	case FSM_EQ_INIT:
		/*check mhl 3.4gb*/
		rx_pr("EQ-init\n");

		if (hdmirx_tmds_6g()) {
			hdmirx_wr_phy(PHY_CDR_CTRL_CNT,
				hdmirx_rd_phy(PHY_CDR_CTRL_CNT)|(1<<8));
		} else {
			hdmirx_wr_phy(PHY_CDR_CTRL_CNT,
				hdmirx_rd_phy(PHY_CDR_CTRL_CNT)&(~(1<<8)));
		}

		if ((eq_dbg_ch0 != 0) ||
			(eq_dbg_ch1 != 0) ||
			(eq_dbg_ch2 != 0)) {
			if (log_level & EQ_LOG)
				rx_pr("eq_dbg:%d-%d-%d",
				eq_dbg_ch0,
				eq_dbg_ch1,
				eq_dbg_ch2);
			eq_ch0.bestsetting = eq_dbg_ch0;
			eq_ch1.bestsetting = eq_dbg_ch1;
			eq_ch2.bestsetting = eq_dbg_ch2;
			rx.state = FSM_EQ_END;
			break;
		} else if (!rx_need_eq_workaround()) {
			/*for some DVD, 480i/576i eq0-0-0 no signal*/
			if (use_eq_workaround) {
				if (pre_eq_freq == E_EQ_LOW_FREQ) {
					if (rx.pre_state == FSM_EQ_INIT) {
						rx_pr("use a default EQ\n");
						hdmirx_phy_conf_eq_setting(
						rx.port,
						4,
						4,
						4);
						rx.pre_state = FSM_SIG_UNSTABLE;
					} else
						rx.pre_state = FSM_EQ_INIT;
				}
			}
			rx.state = FSM_SIG_UNSTABLE;
			break;
		} else {
			if (hdcp22_on)
				esm_set_stable(0);
			rx.state = FSM_EQ_CALIBRATION;
			queue_delayed_work(eq_wq,
				&eq_dwork, msecs_to_jiffies(1));
			break;
		}
		break;
	case FSM_EQ_CALIBRATION:
		break;
	case FSM_EQ_END:
		phy_conf_eq_setting(eq_ch0.bestsetting,
				eq_ch1.bestsetting,
				eq_ch2.bestsetting);
		hdmirx_phy_conf_eq_setting(rx.port,
				eq_ch0.bestsetting,
				eq_ch1.bestsetting,
				eq_ch2.bestsetting);
		if (log_level & EQ_LOG)
			rx_pr("EQ_end\n");
		rx.state = FSM_SIG_UNSTABLE;
		rx_pr("->UNSTABLE\n");
		break;
	case FSM_WAIT_HDCP_SWITCH:
		force_wait_cnt++;
		if (force_wait_cnt > force_wait_max) {
			rx.state = rx.pre_state;
			force_wait_cnt = 0;
			break;
		}
		break;
	case FSM_SIG_UNSTABLE:
		if (hdmirx_tmds_pll_lock()) {
			sig_pll_lock_cnt++;
			if (sig_pll_lock_cnt > sig_pll_lock_max) {
				rx.state = FSM_DWC_RST_WAIT;
				rx.scdc_tmds_cfg = 0;
				if (use_dwc_reset) {
					rx_dwc_reset();
					/*use_dwc_reset = false;*/
				}
				#ifdef HDCP22_ENABLE
				if (hdcp22_on)
					esm_set_stable(1);
				#endif
				sig_pll_unlock_cnt = 0;
				sig_pll_lock_cnt = 0;
				rx.no_signal = false;
				rx_pr("UNSTABLE->DWC_RST pll:%d\n",
					    rx.scdc_tmds_cfg);
			} else {
			    if (log_level & VIDEO_LOG)
					rx_pr("SIG_UNSTABLE lock_cnt :%d\n",
							sig_pll_lock_cnt);
			}
		} else {
			if ((sig_pll_lock_cnt) && (log_level & VIDEO_LOG))
				rx_pr("pll_lock_cnt=%d\n", sig_pll_lock_cnt);

			/*for some device sending scdc slow,recheck*/
			/*hdmirx_phy_clk_rate_monitor();*/
			sig_pll_lock_cnt = 0;
			sig_pll_unlock_cnt++;
			if (sig_pll_unlock_cnt >= sig_pll_unlock_max) {
				hdmirx_error_count_config();
				rx.scdc_tmds_cfg++;
				if (hdmirx_tmds_34g_max()) {
					rx.scdc_tmds_cfg = 0;
					hdmirx_set_hpd(rx.port, 0);
					rx.state = FSM_HPD_LOW;
					sig_pll_unlock_cnt = 0;
					break;
				}
				rx.state = FSM_WAIT_CLK_STABLE;
				rx_pr("UNSTABLE->HPD_READY 3g:%d\n",
						rx.scdc_tmds_cfg);
				sig_pll_unlock_cnt = 0;

			}
		}
		break;
	case FSM_DWC_RST_WAIT:
		dwc_rst_wait_cnt++;
		if ((do_hpd_reset_flag) &&
			(dwc_rst_wait_cnt < wait_hpd_reset_max))
			break;
		if ((!do_hpd_reset_flag) &&
			(dwc_rst_wait_cnt < dwc_rst_wait_cnt_max))
			break;
		do_hpd_reset_flag = 0;
		dwc_rst_wait_cnt = 0;
		rx.state = FSM_SIG_STABLE;
		rx_pr("DWC_RST->FSM_SIG_STABLE\n");
		break;

	case FSM_SIG_STABLE:
		memcpy(&rx.pre, &rx.cur,
			sizeof(struct hdmi_rx_ctrl_video));
		hdmirx_get_video_info();
		if ((is_timing_stable() && is_hdcp_enc_stable()) ||
			force_ready) {
			if (sig_stable_cnt++ > sig_stable_max) {
				get_timing_fmt();
				if ((rx.pre.sw_vic == HDMI_UNSUPPORT) ||
					(rx.pre.sw_vic == HDMI_UNKNOW)) {
					if (log_level & VIDEO_LOG)
						rx_pr("*unsupport*\n");
					if (sig_stable_cnt < (sig_stable_max*5))
						break;
					sig_stable_cnt = 0;
					rx_pr(
					"unsupport->wait_clk_stable\n");
					rx.state = FSM_WAIT_CLK_STABLE;
					break;
				}
				if (rx.pre.sw_dvi == 1) {
					if (sig_stable_cnt < (sig_stable_max*7))
						break;
				}
				if ((is_hdcp_source) &&
					(rx.pre.hdcp_type == E_HDCP14) &&
					(rx.pre.hdcp_enc_state != 3) &&
					(rx.pre.hdcp_enc_state != 0)) {
					if (log_level & VIDEO_LOG)
						rx_pr("hdcp14 sts unstable\n");
					if (sig_stable_cnt < (sig_stable_max*7))
						break;
				}
				/*
				if ((rx.pre.hdcp_type == E_HDCP22) &&
					(rx.pre.hdcp_enc_state != 1) &&
					(hdcp22_capable_sts != 0)) {
					if (log_level & VIDEO_LOG)
						rx_pr("hdcp22 sts unstable\n");
					if (sig_stable_cnt < (sig_stable_max*7))
						break;
				} */
				sig_stable_cnt = 0;
				sig_unstable_cnt = 0;
				rx.change = 0;
				rx.state = FSM_CHECK_DDC_CORRECT;
				rx.no_signal = false;
				irq_video_mute_flag = false;
				memset(&rx.aud_info, 0,
					sizeof(struct aud_info_s));
				hdmirx_config_video();
				rx_pr("STABLE->DDC_CORRECT\n");
				if (log_level & VIDEO_LOG)
					dump_state(0x1);
			}
		} else {
			sig_stable_cnt = 0;
			if (sig_unstable_cnt++ > sig_unstable_max) {
				rx.state = FSM_WAIT_CLK_STABLE;
				sig_stable_cnt = 0;
				sig_unstable_cnt = 0;
				hdmirx_error_count_config();
				if (enable_hpd_reset) {
					sig_unstable_reset_hpd_cnt++;
					if (sig_unstable_reset_hpd_cnt >=
						sig_unstable_reset_hpd_max) {
						rx.state = FSM_HPD_HIGH;
						hdmirx_set_hpd(rx.port, 0);
						sig_unstable_reset_hpd_cnt = 0;
						rx_pr(
						"unstable->HDMI5V_HIGH\n");
						break;
					}
				}
				rx_pr("STABLE->HPD_READY\n");
			}
		}
		break;
	case FSM_CHECK_DDC_CORRECT:
		if ((rx.pre.hdcp_enc_state == 0) ||
			(rx.pre.hdcp_enc_state == 2))
			is_hdcp_source = false;
		rx.state = FSM_SIG_READY;
		rx_pr("DDCERROR->READY\n");
		break;
	case FSM_SIG_READY:
	    hdmirx_get_video_info();
	    /* video info change */
	    if ((hdmirx_tmds_pll_lock() == false) ||
			(is_timing_stable() == false)) {
			if (rx.change < skip_frame_max)
				rx.change += skip_frame_num;
			if (++sig_unready_cnt >= sig_unready_max) {
				/*sig_lost_lock_cnt = 0;*/
				sig_unready_cnt = 0;
				audio_sample_rate = 0;
				/* hdmirx_set_video_mute(1); */
				rx_aud_pll_ctl(0);
				hdmirx_audio_enable(0);
				/* hdmirx_audio_fifo_rst(); */
				rx.state = FSM_WAIT_CLK_STABLE;
				rx.pre_state = FSM_SIG_READY;
				wait_no_sig_cnt = 0;
				rx.aud_sr_stable_cnt = 0;
				#ifdef HDCP22_ENABLE
				if (hdcp22_on)
					esm_set_stable(0);
				#endif
				memcpy(&rx.pre, &rx.cur,
					sizeof(struct hdmi_rx_ctrl_video));
				memset(&rx.vendor_specific_info,
					0,
					sizeof(struct vendor_specific_info_s));
				rx_pr("READY->HPD_READY\n");
				break;
			}
	    } else {
			if (sig_unready_cnt != 0) {
				if (log_level & VIDEO_LOG)
					rx_pr("sig_unready_cnt=%d",
						sig_unready_cnt);
				sig_unready_cnt = 0;
			}
		}

		if (rx.no_signal == true)
			rx.no_signal = false;

		if (rx.change > 0) {
			rx.change--;
			if (log_level & VIDEO_LOG)
				rx_pr("rc--%d\n", rx.change);
		}

		if ((0 == audio_enable) ||
			(rx.pre.sw_dvi == 1))
			break;

		pre_sample_rate = rx.aud_info.real_sample_rate;
		hdmirx_read_audio_info(&rx.aud_info);
		if (force_audio_sample_rate == 0)
			rx.aud_info.real_sample_rate =
				get_real_sample_rate();
		else
			rx.aud_info.real_sample_rate =
				force_audio_sample_rate;

		if ((rx.aud_info.real_sample_rate <= 31000)
			&& (rx.aud_info.real_sample_rate >= 193000)
			&&
			(abs((signed int)rx.aud_info.real_sample_rate -
				(signed int)pre_sample_rate) >
					 AUD_SR_RANGE)) {
			if (log_level & AUDIO_LOG)
				dump_audio_info(1);
		}

		if (!is_sample_rate_stable
			(pre_sample_rate, rx.aud_info.real_sample_rate)) {
			if (log_level & AUDIO_LOG)
				dump_audio_info(1);
			rx.aud_sr_stable_cnt = 0;
			break;
		}
		if (rx.aud_sr_stable_cnt <
			AUD_SR_RANGE) {
			rx.aud_sr_stable_cnt++;
			if (rx.aud_sr_stable_cnt ==
				AUD_SR_STB_MAX) {
				dump_state(0x2);
				rx_aud_pll_ctl(1);
				hdmirx_audio_enable(1);
				hdmirx_audio_fifo_rst();

				audio_sample_rate =
					rx.aud_info.real_sample_rate;
				audio_coding_type =
					rx.aud_info.coding_type;
				audio_channel_count =
					rx.aud_info.channel_count;

				if (hdmirx_get_audio_clock() < 100000) {
					rx_pr("update audio\n");
					tmp = hdmirx_rd_top(TOP_ACR_CNTL_STAT);
					hdmirx_wr_top(TOP_ACR_CNTL_STAT,
							tmp | (1<<11));
				}
			}
		} else {

		}
		auds_rcv_sts =
			rx.aud_info.aud_packet_received;
		rgb_quant_range = rx.cur.rgb_quant_range;
		yuv_quant_range = rx.cur.yuv_quant_range;
		it_content = rx.cur.it_content;
		break;
	default:
		break;
	}

	if (force_state & 0x10) {
		rx.state = force_state & 0xf;
		if ((force_state & 0x20) == 0)
			force_state = 0;
	}
}

int rx_get_edid_index(void)
{
	if ((edid_mode == 0) &&
		edid_size > 4 &&
		edid_buf[0] == 'E' &&
		edid_buf[1] == 'D' &&
		edid_buf[2] == 'I' &&
		edid_buf[3] == 'D') {
		rx_pr("edid: use Top edid\n");
		return EDID_LIST_BUFF;
	} else {
		if (edid_mode == 0)
			return EDID_LIST_AML;
		else if (edid_mode < EDID_LIST_NUM)
			return edid_mode;
		else
			return EDID_LIST_AML;
	}
}

unsigned char *rx_get_edid_buffer(int index)
{
	if (index == EDID_LIST_BUFF)
		return edid_list[index] + 4;
	else
		return edid_list[index];
}

int rx_get_edid_size(int index)
{
	if (index == EDID_LIST_BUFF) {
		if (edid_size > EDID_SIZE + 4)
			edid_size = EDID_SIZE + 4;
		return edid_size - 4;
	} else
		return EDID_SIZE;
}

int rx_set_receiver_edid(unsigned char *data, int len)
{
	if ((data == NULL) || (len == 0) || (len > MAX_RECEIVE_EDID))
		return false;

	memcpy(receive_edid, data, len);
	new_edid = true;
	return true;
}
EXPORT_SYMBOL(rx_set_receiver_edid);

int rx_set_hdr_lumi(unsigned char *data, int len)
{
	if ((data == NULL) || (len == 0) || (len > MAX_HDR_LUMI))
		return false;

	memcpy(receive_hdr_lum, data, len);
	new_hdr_lum = true;
	return true;
}
EXPORT_SYMBOL(rx_set_hdr_lumi);

void rx_set_repeater_support(bool enable)
{
	downstream_repeat_support = enable;
}
EXPORT_SYMBOL(rx_set_repeater_support);

bool rx_poll_dwc(uint16_t addr, uint32_t exp_data,
			uint32_t mask, uint32_t max_try)
{
	uint32_t rd_data;
	uint32_t cnt = 0;
	uint8_t done = 0;

	rd_data = hdmirx_rd_dwc(addr);
	while (((cnt < max_try) || (max_try == 0)) && (done != 1)) {
		if ((rd_data & mask) == (exp_data & mask)) {
			done = 1;
		} else {
			cnt++;
			rd_data = hdmirx_rd_dwc(addr);
		}
	}
	rx_pr("poll dwc cnt :%d\n", cnt);
	if (done == 0) {
		/* if(log_level & ERR_LOG) */
		rx_pr("poll dwc%x time-out!\n", addr);
		return false;
	}
	return true;
}

bool rx_set_repeat_aksv(unsigned char *data, int len, int depth,
	bool dev_exceed, bool cascade_exceed)
{
	int i;
	/*rx_pr("set ksv list len:%d,depth:%d\n", len, depth);*/
	if ((len == 0) || (data == 0) || (depth == 0))
		return false;
	/*set repeat depth*/
	if ((depth <= MAX_REPEAT_DEPTH) && (!cascade_exceed)) {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, MAX_CASCADE_EXCEEDED,
		0);
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, DEPTH, depth);
		rx.hdcp.depth = depth;
	} else {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, MAX_CASCADE_EXCEEDED,
		1);
	}
	/*set repeat count*/
	if ((len <= MAX_REPEAT_COUNT) && (!dev_exceed)) {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, MAX_DEVS_EXCEEDED,
		0);
		rx.hdcp.count = len;
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, DEVICE_COUNT,
								rx.hdcp.count);
	} else {
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_BSTATUS, MAX_DEVS_EXCEEDED,
		1);
	}
	/*set repeat status*/
	if (rx.hdcp.count > 0) {
		rx.hdcp.repeat = true;
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL, REPEATER, 1);
	} else {
		rx.hdcp.repeat = false;
		hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL, REPEATER, 0);
	}
	/*write ksv list to fifo*/
	for (i = 0; i < rx.hdcp.count; i++) {
		if (rx_poll_dwc(DWC_HDCP_RPT_CTRL, ~KSV_HOLD, KSV_HOLD,
		KSV_LIST_WR_TH)) {
			/*check fifo can be written*/
			hdmirx_wr_dwc(DWC_HDCP_RPT_KSVFIFOCTRL, i);
			hdmirx_wr_dwc(DWC_HDCP_RPT_KSVFIFO1,
				*(data + i*MAX_KSV_SIZE + 4));
			hdmirx_wr_dwc(DWC_HDCP_RPT_KSVFIFO0,
				*((uint32_t *)(data + i*MAX_KSV_SIZE)));
			rx_pr(
			"[RX]write ksv list index:%d, ksv hi:%#x, low:%#x\n",
				i, *(data + i*MAX_KSV_SIZE +
			4), *((uint32_t *)(data + i*MAX_KSV_SIZE)));
		} else {
			return false;
		}
	}
	/* Wait for ksv_hold=0*/
	rx_poll_dwc(DWC_HDCP_RPT_CTRL, ~KSV_HOLD, KSV_HOLD, KSV_LIST_WR_TH);
	/*set ksv list ready*/
	hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL, KSVLIST_READY,
					(rx.hdcp.count > 0));
	/* Wait for HW completion of V value*/
	rx_poll_dwc(DWC_HDCP_RPT_CTRL, FIFO_READY, FIFO_READY, KSV_V_WR_TH);
	rx_pr("[RX]write Ready signal!\n");

	return true;
}

void rx_set_repeat_signal(bool repeat)
{
	rx.hdcp.repeat = repeat;
	hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL, REPEATER, repeat);
}

bool rx_set_receive_hdcp(unsigned char *data, int len, int depth,
	bool cas_exceed, bool devs_exceed)
{
	if ((data != 0) && (len != 0) && (len <= MAX_REPEAT_COUNT))
		memcpy(receive_hdcp, data, len*MAX_KSV_SIZE);
	rx_pr("receive ksv list len:%d,depth:%d,cas:%d,dev:%d\n", len,
	depth, cas_exceed, devs_exceed);
	hdcp_len = len;
	hdcp_repeat_depth = depth;
	rx.hdcp.cascade_exceed = cas_exceed;
	rx.hdcp.dev_exceed = devs_exceed;

	return true;
}
EXPORT_SYMBOL(rx_set_receive_hdcp);

void rx_repeat_hpd_state(bool plug)
{
	repeat_plug = plug;
}
EXPORT_SYMBOL(rx_repeat_hpd_state);

void rx_repeat_hdcp_ver(int version)
{

}
EXPORT_SYMBOL(rx_repeat_hdcp_ver);

void rx_edid_physical_addr(int a, int b, int c, int d)
{
	up_phy_addr = ((d & 0xf) << 12)  | ((c & 0xf) << 8) | ((b &
	0xf) << 4) | (a & 0xf);
}
EXPORT_SYMBOL(rx_edid_physical_addr);

void rx_send_hpd_pulse(void)
{
	hdmirx_set_hpd(rx.port, 0);
	rx.state = FSM_HPD_HIGH;
}

void rx_start_repeater_auth(void)
{
	rx.hdcp.state = REPEATER_STATE_START;
	rx.hdcp.delay = 0;
	/*hdcp_len = 0;*/
	/*hdcp_repeat_depth = 0;*/
	rx.hdcp.dev_exceed = 0;
	rx.hdcp.cascade_exceed = 0;
}

void rx_check_repeat(void)
{
	if (!hdmirx_repeat_support())
		return;

	if (rx.hdcp.repeat != repeat_plug) {
		rx_set_repeat_signal(repeat_plug);
		if (!repeat_plug) {
			hdcp_len = 0;
			hdcp_repeat_depth = 0;
			rx.hdcp.dev_exceed = 0;
			rx.hdcp.cascade_exceed = 0;
			memset(&receive_hdcp, 0 , sizeof(receive_hdcp));
			new_edid = true;
			memset(&receive_edid, 0 , sizeof(receive_edid));
			rx_send_hpd_pulse();
		}
	}
	if (new_edid) {
		/*check downstream plug when new plug occur*/
		/*check receive change*/
		hdmi_rx_ctrl_edid_update();
		new_edid = false;
		rx_send_hpd_pulse();
	}
	if (repeat_plug) {
		switch (rx.hdcp.state) {
		case REPEATER_STATE_START:
			rx_pr("[RX] receive aksv\n");
			hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL,
						KSVLIST_TIMEOUT, 0);
			hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL,
						KSVLIST_LOSTAUTH, 0);
			hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL,
						KSVLIST_READY, 0);
			rx.hdcp.state = REPEATER_STATE_WAIT_KSV;
		break;

		case REPEATER_STATE_WAIT_KSV:
		if (!rx.cur_5v_sts) {
			rx.hdcp.state = REPEATER_STATE_IDLE;
			break;
		}
		if (hdmirx_rd_bits_dwc(DWC_HDCP_RPT_CTRL, WAITING_KSV)) {
			rx.hdcp.delay++;
			if (rx.hdcp.delay == 1)
				rx_pr("[RX] receive ksv wait signal\n");
			if (rx.hdcp.delay < KSV_LIST_WR_MAX) {
				break;
			} else if (rx.hdcp.delay >= KSV_LIST_WAIT_DELAY) {
				hdmirx_wr_bits_dwc(DWC_HDCP_RPT_CTRL,
						KSVLIST_TIMEOUT, 1);
				rx.hdcp.state = REPEATER_STATE_IDLE;
				rx_pr("[RX] receive ksv wait timeout\n");
			}
			if (rx_set_repeat_aksv(receive_hdcp, hdcp_len,
				hdcp_repeat_depth, rx.hdcp.dev_exceed,
				rx.hdcp.cascade_exceed)) {
				rx.hdcp.state = REPEATER_STATE_IDLE;
			}
		}
		/*if support hdcp2.2 jump to wait_ack else to idle*/
		break;

		case REPEATER_STATE_WAIT_ACK:
		/*if receive ack jump to idle else send auth_req*/
		break;

		case REPEATER_STATE_IDLE:

		break;

		default:
		break;
		}
	}
	/*hdr change from app*/
	if (new_hdr_lum) {
		hdmi_rx_ctrl_edid_update();
		new_hdr_lum = false;
		rx_send_hpd_pulse();
	}

}

int rx_get_tag_code(uint8_t *edid_data)
{
	int tag_code;

	if ((*edid_data >> 5) != 7)
		tag_code = (*edid_data >> 5);
	else
		tag_code = (7 << 8) | *(edid_data + 1);/*extern tag*/

	return tag_code;
}

int rx_get_ceadata_offset(uint8_t *cur_edid, uint8_t *addition)
{
	int i;
	int type;

	if ((cur_edid == NULL) || (addition == NULL))
		return 0;

	type = rx_get_tag_code(addition);
	i = EDID_DEFAULT_START;/*block check start index*/
	while (i < 255) {
		if (type == rx_get_tag_code(cur_edid + i))
			return i;
		else
			i += (1 + (*(cur_edid + i) & 0x1f));
	}
	if (log_level & VIDEO_LOG)
		rx_pr("type: %#x, start addr: %#x\n", type, i);

	return 0;
}

void rx_mix_edid_audio(uint8_t *cur_data, uint8_t *addition)
{
struct edid_audio_block_t *ori_data;
struct edid_audio_block_t *add_data;
unsigned char ori_len, add_len;
int i, j;

if ((cur_data == 0) || (addition == 0) ||
	(*cur_data >> 5) != (*addition >> 5))
	return;

ori_data = (struct edid_audio_block_t *)(cur_data + 1);
add_data = (struct edid_audio_block_t *)(addition + 1);
ori_len = (*cur_data & 0x1f)/FORMAT_SIZE;
add_len = (*addition & 0x1f)/FORMAT_SIZE;

for (i = 0; i < add_len; i++) {
	if (log_level & VIDEO_LOG)
		rx_pr("mix audio format:%d\n", add_data[i].format_code);
	/*only support lpcm dts dd+*/
	if (!is_audio_support(add_data[i].format_code))
		continue;
	/*mix audio data*/
	for (j = 0; j < ori_len; j++) {
		if (ori_data[j].format_code ==
					add_data[i].format_code) {
			if (log_level & VIDEO_LOG)
				rx_pr("mix audio mix format:%d\n",
					add_data[i].format_code);
			/*choose channel is lager*/
			ori_data[j].max_channel =
				((ori_data[j].max_channel >
				add_data[i].max_channel) ?
				ori_data[j].max_channel :
				add_data[i].max_channel);
			/*mix sample freqence*/
			*(((unsigned char *)&ori_data[j]) + 1) =
			*(((unsigned char *)&ori_data[j]) + 1) |
				*(((unsigned char *)&add_data[i]) + 1);
			/*mix bit rate*/
			if (add_data[i].format_code ==
						AUDIO_FORMAT_LPCM)
				*(((unsigned char *)&ori_data[j]) + 2) =
				*(((unsigned char *)&ori_data[j]) + 2)|
				*(((unsigned char *)&add_data[i]) + 2);
			else
				ori_data[j].bit_rate.others =
				add_data[i].bit_rate.others;
		} else {
			if (j == (ori_len - 1)) {
				if (log_level & VIDEO_LOG)
					rx_pr("mix audio add new format: %d\n",
					add_data[i].format_code);
				if (((*cur_data & 0x1f) + FORMAT_SIZE)
							 <= 0x1f) {
					memcpy(cur_data +
						 (*cur_data & 0x1f) + 1,
					&add_data[i], FORMAT_SIZE);
					*cur_data += FORMAT_SIZE;
				}
			}
		}
	}
}
}
void rx_mix_edid_hdr(uint8_t *cur_data, uint8_t *addition)
{
	struct edid_hdr_block_t *cur_block;
	struct edid_hdr_block_t *add_block;

	if ((cur_data == 0) || (addition == 0) ||
		(*cur_data >> 5) != (*addition >> 5))
		return;

	cur_block = (struct edid_hdr_block_t *)(cur_data + 1);
	add_block = (struct edid_hdr_block_t *)(addition + 1);
	if (add_block->max_lumi | add_block->avg_lumi |
					add_block->min_lumi) {
		cur_block->max_lumi = add_block->max_lumi;
		cur_block->avg_lumi = add_block->avg_lumi;
		cur_block->min_lumi = add_block->min_lumi;
		if ((*cur_data & 0x1f) < (*addition & 0x1f))
			*cur_data = *addition;
	}
}

void rx_mix_block(uint8_t *cur_data, uint8_t *addition)
{
	int tag_code;
	if ((cur_data == 0) || (addition == 0) ||
		(*cur_data >> 5) != (*addition >> 5))
		return;
	if (log_level & VIDEO_LOG)
		rx_pr("before type:%d - %d,len:%d - %d\n",
	(*cur_data >> 5), (*addition >> 5),
	(*cur_data & 0x1f), (*addition & 0x1f));

	tag_code = rx_get_tag_code(cur_data);

	switch (tag_code) {
	case EDID_TAG_AUDIO:
		rx_mix_edid_audio(cur_data, addition);
		break;

	case EDID_TAG_HDR:
		rx_mix_edid_hdr(cur_data, addition);
		break;
	}
	if (log_level & VIDEO_LOG)
		rx_pr("end type:%d - %d,len:%d - %d\n",
	(*cur_data >> 5), (*addition >> 5),
	(*cur_data & 0x1f), (*addition & 0x1f));
}

void rx_modify_edid(unsigned char *buffer,
				int len, unsigned char *addition)
{
	int start_addr = 0;/*the addr in edid need modify*/
	int start_addr_temp = 0;/*edid_temp start addr*/
	int temp_len = 0;
	unsigned char *cur_data = NULL;
	int addition_size = 0;
	int cur_size = 0;
	int i;

	if ((len <= 255) || (buffer == 0) || (addition == 0))
		return;

	/*get the mix block value*/
	if (*addition & 0x1f) {
		/*get addition block index in local edid*/
		start_addr = rx_get_ceadata_offset(buffer, addition);
		if (start_addr > EDID_DEFAULT_START) {
			cur_size = (*(buffer + start_addr) & 0x1f) + 1;
			addition_size = (*addition & 0x1f) + 1;
			cur_data = kmalloc(
				addition_size + cur_size, GFP_KERNEL);
			memcpy(cur_data, buffer + start_addr, cur_size);
			/*add addition block property to local edid*/
			rx_mix_block(cur_data, addition);
			addition_size = (*cur_data & 0x1f) + 1;
		} else
			return;
		if (log_level & VIDEO_LOG)
			rx_pr(
			"start_addr: %#x,cur_size: %d,addition_size: %d\n",
			start_addr, cur_size, addition_size);

		/*set the block value to edid_temp*/
		start_addr_temp = rx_get_ceadata_offset(buffer, addition);
		temp_len = ((buffer[start_addr_temp] & 0x1f) + 1);
		if (log_level & VIDEO_LOG)
			rx_pr("edid_temp start: %#x, len: %d\n",
			start_addr_temp, temp_len);
		/*move data behind current data if need*/
		if (temp_len < addition_size) {
			for (i = 0; i < EDID_SIZE - start_addr_temp -
				addition_size; i++) {
				buffer[255 - i] =
				buffer[255 - (addition_size - temp_len)
						 - i];
			}
		} else if (temp_len > addition_size) {
			for (i = 0; i < EDID_SIZE - start_addr_temp -
						temp_len; i++) {
				buffer[start_addr_temp + i + addition_size]
				 = buffer[start_addr_temp + i + temp_len];
			}
		}
		/*check detail description offset if needs modify*/
		if (buffer[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET] +
				EDID_BLOCK1_OFFSET > start_addr_temp)
			buffer[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET]
				+= (addition_size - temp_len);
		/*copy current edid data*/
		memcpy(buffer + start_addr_temp, cur_data,
						addition_size);
		if (cur_data != 0)
			kfree(cur_data);
	}
}

unsigned int rx_exchange_bits(unsigned int value)
{
	unsigned int temp;
	rx_pr("bfe:%#x\n", value);
	temp = value & 0xF;
	value = (((value >> 4) & 0xF) | (value & 0xFFF0));
	value = ((value & 0xFF0F) | (temp << 4));
	temp = value & 0xF00;
	value = (((value >> 4) & 0xF00) | (value & 0xF0FF));
	value = ((value & 0x0FFF) | (temp << 4));
	rx_pr("aft:%#x\n", value);
	return value;
}

void hdmi_rx_load_edid_data(unsigned char *buffer, int port)
{
	unsigned char value = 0;
	unsigned char check_sum = 0;
	unsigned char phy_addr_offset = 0;
	int i, ram_addr;
	unsigned char phy_addr[3] = {0, 0, 0};
	unsigned char checksum[3] = {0, 0, 0};

	for (i = 0; i <= 255; i++) {
		value = buffer[i];

		/*find phy_addr_offset*/
		if (value == 0x03) {
			if ((0x0c == buffer[i+1]) &&
				(0x00 == buffer[i+2]) &&
				(0x00 == buffer[i+4])) {
				buffer[i+3] = 0x10;
				phy_addr_offset = i+3;
			}
		}
		if ((i >= 128) && (i < 255)) {
			check_sum += value;
			check_sum &= 0xff;
		}
		if (i == 255) {
			value = check_sum & 0xff;
			check_sum = 0;
		}
		ram_addr = TOP_EDID_OFFSET + i;
		hdmirx_wr_top(ram_addr, value);
		hdmirx_wr_top(0x100+ram_addr, value);
	}

	for (i = 0; i < 3; i++) {
		if (((port >> i*4) & 0xf) == 0) {
			phy_addr[0] = ((i + 1) << 4);
			checksum[0] = (0x100 - (value +
				(phy_addr[0] - 0x10))) & 0xff;
		} else if (((port >> i*4) & 0xf) == 1) {
			phy_addr[1] = ((i + 1) << 4);
			checksum[1] = (0x100 - (value +
				(phy_addr[1] - 0x10))) & 0xff;
		} else if (((port >> i*4) & 0xf) == 2) {
			phy_addr[2] = ((i + 1) << 4);
			checksum[2] = (0x100 - (value +
				(phy_addr[2] - 0x10))) & 0xff;
		}
	}
	hdmirx_wr_top(TOP_EDID_RAM_OVR1,
		phy_addr_offset | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR1_DATA,
		phy_addr[0]|phy_addr[1]<<8|phy_addr[2]<<16);

	hdmirx_wr_top(TOP_EDID_RAM_OVR0,
		0xff | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR0_DATA,
			checksum[0]|checksum[1]<<8|checksum[2]<<16);

}



void hdmi_rx_load_edid_data_repeater(unsigned char *buffer, int port)
{
	unsigned char value = 0;
	unsigned char check_sum = 0;
	unsigned char phy_addr_offset = 0;
	int i, ram_addr;
	unsigned int phy_addr[3] = {0, 0, 0};
	unsigned char checksum[3] = {0, 0, 0};
	unsigned char root_offset = 0;

	for (i = 0; i <= 255; i++) {
		value = buffer[i];

		/*find phy_addr_offset*/
		if (value == 0x03) {
			if ((0x0c == buffer[i+1]) &&
				(0x00 == buffer[i+2]) &&
				(0x00 == buffer[i+4])) {
				buffer[i+3] = 0x00;
				phy_addr_offset = i+3;
			}
		}
		if ((i >= 128) && (i < 255)) {
			check_sum += value;
			check_sum &= 0xff;
		}
		if (i == 255) {
			value = (0x100-check_sum)&0xff;
			check_sum = 0;
		}
		ram_addr = TOP_EDID_OFFSET + i;
		hdmirx_wr_top(ram_addr, value);
		hdmirx_wr_top(0x100+ram_addr, value);
	}
	/*get the root index*/
	i = 0;
	while (i < 4) {
		if (((up_phy_addr << (i*4)) & 0xf000) != 0) {
			root_offset = i;
			break;
		} else
			i++;
	}
	if (i == 4)
		root_offset = 4;

	/*i is equal to the UI hdmi number*/
	rx_pr("port map:%#x\n", port);
	for (i = 0; i < 3; i++) {
		/*port 0 , write the phy addr compute from i*/
		if (((port >> i*4) & 0xf) == 0) {
			if (root_offset == 0)
				phy_addr[0] = 0xFFFF;
			else
				phy_addr[0] = (up_phy_addr | (((i + 1) << 12) >>
				(root_offset - 1)*4));
			phy_addr[0] = rx_exchange_bits(phy_addr[0]);
			checksum[0] = (0x100 + value - (phy_addr[0] & 0xFF) -
				((phy_addr[0] >> 8) & 0xFF)) & 0xff;
			rx_pr("port 0 phy:%d\n", phy_addr[0]);
		/*port 1 , write the phy addr compute from i*/
		} else if (((port >> i*4) & 0xf) == 1) {
			if (root_offset == 0)
				phy_addr[1] = 0xFFFF;
			else
				phy_addr[1] = (up_phy_addr | (((i + 1) << 12) >>
				(root_offset - 1)*4));
			phy_addr[1] = rx_exchange_bits(phy_addr[1]);
			checksum[1] = (0x100 + value - (phy_addr[1] & 0xFF) -
				((phy_addr[1] >> 8) & 0xFF));
			rx_pr("port 1 phy:%d\n", phy_addr[1]);
		/*port 2 , write the phy addr compute from i*/
		} else if (((port >> i*4) & 0xf) == 2) {
			if (root_offset == 0)
				phy_addr[2] = 0xFFFF;
			else
				phy_addr[2] = (up_phy_addr | (((i + 1) << 12) >>
				(root_offset - 1)*4));
			phy_addr[2] = rx_exchange_bits(phy_addr[2]);
			checksum[2] = (0x100 + value - (phy_addr[2] & 0xFF) -
				((phy_addr[2] >> 8) & 0xFF));
			rx_pr("port 2 phy:%d\n", phy_addr[2]);
		}
	}

	hdmirx_wr_top(TOP_EDID_RAM_OVR2,
		(phy_addr_offset + 1) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR2_DATA,
		((phy_addr[0] >> 8) & 0xFF) | (((phy_addr[1] >> 8) & 0xFF)<<8)
			| (((phy_addr[2] >> 8) & 0xFF)<<16));

	hdmirx_wr_top(TOP_EDID_RAM_OVR1,
		phy_addr_offset | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR1_DATA,
		(phy_addr[0] & 0xFF) | ((phy_addr[1] & 0xFF)<<8) |
			((phy_addr[2] & 0xFF)<<16));

	hdmirx_wr_top(TOP_EDID_RAM_OVR0,
		0xff | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR0_DATA,
			checksum[0]|checksum[1]<<8|checksum[2]<<16);

}

int hdmi_rx_ctrl_edid_update(void)
{
	unsigned char hdr_edid[EDID_HDR_SIZE];
	int edid_index = rx_get_edid_index();

	memcpy(edid_temp, rx_get_edid_buffer(edid_index),
				rx_get_edid_size(edid_index));
	if (hdmirx_repeat_support()) {
		hdr_edid[0] = ((EDID_TAG_HDR >> 3)&0xE0) + (6 & 0x1f);
		hdr_edid[1] = EDID_TAG_HDR & 0xFF;
		memcpy(hdr_edid + 4, receive_hdr_lum,
					sizeof(unsigned char)*3);
		rx_modify_edid(edid_temp, rx_get_edid_size(edid_index),
							receive_edid);
		rx_modify_edid(edid_temp, rx_get_edid_size(edid_index),
							hdr_edid);
		hdmi_rx_load_edid_data_repeater(edid_temp, real_port_map);
	} else
		hdmi_rx_load_edid_data(edid_temp, real_port_map);

	rx_pr("edid update\n");
	return true;
}

static void set_hdcp(struct hdmi_rx_ctrl_hdcp *hdcp, const unsigned char *b_key)
{
	int i, j;
	memset(&init_hdcp_data, 0, sizeof(struct hdmi_rx_ctrl_hdcp));
	for (i = 0, j = 0; i < 80; i += 2, j += 7) {
		hdcp->keys[i + 1] =
		    b_key[j] | (b_key[j + 1] << 8) | (b_key[j + 2] << 16) |
		    (b_key[j + 3] << 24);
		hdcp->keys[i + 0] =
		    b_key[j + 4] | (b_key[j + 5] << 8) | (b_key[j + 6] << 16);
	}
	hdcp->bksv[1] =
	    b_key[j] | (b_key[j + 1] << 8) | (b_key[j + 2] << 16) |
	    (b_key[j + 3] << 24);
	hdcp->bksv[0] = b_key[j + 4];

}

int hdmirx_read_key_buf(char *buf, int max_size)
{
	if (key_size > max_size) {
		rx_pr("Error: %s,key size %d",
				__func__, key_size);
		rx_pr("is larger than the buf size of %d\n", max_size);
		return 0;
	}
	memcpy(buf, key_buf, key_size);
	rx_pr("HDMIRX: read key buf\n");
	return key_size;
}

void hdmirx_fill_key_buf(const char *buf, int size)
{
	if (size > MAX_KEY_BUF_SIZE) {
		rx_pr("Error: %s,key size %d",
				__func__,
				size);
		rx_pr("is larger than the max size of %d\n",
			MAX_KEY_BUF_SIZE);
		return;
	}
	if (buf[0] == 'k' && buf[1] == 'e' && buf[2] == 'y') {
		set_hdcp(&init_hdcp_data, buf + 3);
	} else {
		memcpy(key_buf, buf, size);
		key_size = size;
		rx_pr("HDMIRX: fill key buf, size %d\n", size);
	}
}

int hdmirx_read_edid_buf(char *buf, int max_size)
{
	if (edid_size > max_size) {
		rx_pr("Error: %s,edid size %d",
				__func__,
				edid_size);
		rx_pr(" is larger than the buf size of %d\n",
			max_size);
		return 0;
	}
	memcpy(buf, edid_buf, edid_size);
	rx_pr("HDMIRX: read edid buf\n");
	return edid_size;
}

void hdmirx_fill_edid_buf(const char *buf, int size)
{
	if (size > MAX_EDID_BUF_SIZE) {
		rx_pr("Error: %s,edid size %d",
				__func__,
				size);
		rx_pr(" is larger than the max size of %d\n",
			MAX_EDID_BUF_SIZE);
		return;
	}
	memcpy(edid_buf, buf, size);

	edid_size = size;
	rx_pr("HDMIRX: fill edid buf, size %d\n",
		size);
}

/********************
    debug functions
*********************/
int hdmirx_hw_dump_reg(unsigned char *buf, int size)
{
	return 0;
}

static void dump_state(unsigned char enable)
{
	/*int error = 0;*/
	/* int i = 0; */
	struct hdmi_rx_ctrl_video v;
	static struct aud_info_s a;
	memset(&v, 0, sizeof(struct hdmi_rx_ctrl_video));

	hdmirx_get_video_info();
	if (enable & 1) {
		rx_pr("[HDMI info]");
		rx_pr("colorspace %d,", rx.pre.colorspace);
		rx_pr("hw_vic %d,", rx.pre.hw_vic);
		rx_pr("dvi %d,", rx.pre.hw_dvi);
		rx_pr("interlace %d\n", rx.pre.interlaced);
		rx_pr("htotal %d", rx.pre.htotal);
		rx_pr("hactive %d", rx.pre.hactive);
		rx_pr("vtotal %d", rx.pre.vtotal);
		rx_pr("vactive %d", rx.pre.vactive);
		rx_pr("repetition %d\n", rx.pre.repeat);
		rx_pr("colordepth %d", rx.pre.colordepth);
		rx_pr("refresh_rate %d\n", rx.pre.refresh_rate*2);
	}
	if (enable & 2) {
		hdmirx_read_audio_info(&a);
		rx_pr("AudioInfo:");
		rx_pr(" CT=%u CC=%u",
				a.coding_type,
				a.channel_count);
		rx_pr(" SF=%u SS=%u",
				a.sample_frequency,
				a.sample_size);
		rx_pr(" CA=%u",
			a.channel_allocation);

		rx_pr(" CTS=%d, N=%d,",
				a.cts, a.n);
		rx_pr("recovery clock is %d\n",
			a.arc);
	}
	rx_pr("TMDS clock = %d,",
		hdmirx_get_tmds_clock());
	rx_pr("Pixel clock = %d\n",
		hdmirx_get_pixel_clock());
	rx_pr("Audio PLL clock = %d",
		hdmirx_get_audio_pll_clock());
	rx_pr("ESM clock = %d",
		hdmirx_get_esm_clock());

	rx_pr("rx.no_signal=%d,rx.state=%d,",
			rx.no_signal,
			rx.state);
	rx_pr("skip frame=%d\n", rx.change);
	rx_pr("fmt=0x%x,sw_vic:%d,",
			hdmirx_hw_get_fmt(),
			rx.pre.sw_vic);
	rx_pr("sw_dvi:%d,sw_fp:%d,",
			rx.pre.sw_dvi,
			rx.pre.sw_fp);
	rx_pr("sw_alternative:%d\n",
		rx.pre.sw_alternative);

	rx_pr("HDCP debug value=0x%x\n",
		hdmirx_rd_dwc(DWC_HDCP_DBG));
	rx_pr("HDCP encrypted state:%d\n",
		rx.pre.hdcp_enc_state);
	rx_pr("audio receive data:%d\n",
		auds_rcv_sts);
	/***************hdcp*****************/
	rx_pr("HDCP version:%d\n", rx.hdcp.hdcp_version);
	if (hdcp22_on) {
		rx_pr("HDCP22 sts = %x\n",
			rx_hdcp22_rd_reg(0x60));
		rx_pr("HDCP22_on = %d\n",
			hdcp22_on);
		rx_pr("HDCP22_auth_sts = %d\n",
			hdcp22_auth_sts);
		rx_pr("HDCP22_capable_sts = %d\n",
			hdcp22_capable_sts);
		rx_pr("video_stable_to_esm = %d\n",
			video_stable_to_esm);
		rx_pr("hpd_to_esm = %d\n",
			hpd_to_esm);
		rx_pr("sts8fc = %x",
			hdmirx_rd_dwc(DWC_HDCP22_STATUS));
		rx_pr("sts81c = %x",
			hdmirx_rd_dwc(DWC_HDCP22_CONTROL));
		dump_hdcp_data();
		if (!esm_print_device_info())
			rx_pr("\n !!No esm rx opened\n");
	}
	/*--------------edid-------------------*/
	rx_pr("edid index: %d\n", edid_mode);
	rx_pr("edid phy addr: %#x,%#x,current port: %d, up phy addr:%#x\n",
		hdmirx_rd_top(TOP_EDID_RAM_OVR1_DATA),
		hdmirx_rd_top(TOP_EDID_RAM_OVR2_DATA), rx.port, up_phy_addr);
	rx_pr("edid downstream come: %d hpd:%d hdr lume:%d\n",
		new_edid, repeat_plug, new_hdr_lum);

}

static void dump_audio_info(unsigned char enable)
{
	static struct aud_info_s a;

	if (enable) {
		hdmirx_read_audio_info(&a);
		rx_pr("AudioInfo: CT=%u",
				a.coding_type);
		rx_pr(" CC=%u SF=%u SS=%u CA=%u",
			a.channel_count,
			a.sample_frequency,
			a.sample_size,
			a.channel_allocation);
		rx_pr(" [hdmirx]CTS=%d, N=%d,",
				a.cts,
				a.n);
		rx_pr("recovery clock is %d\n",
			a.arc);
	}
}

static unsigned int unread_register[] = {
0x0c, 0x3c, 0x60, 0x64, 0x68, 0x6c, 0x70, 0x74, 0x78, 0x7c, 0x8c, 0xa0,
0xac, 0xc8, 0xd8, 0xdc, 0x184, 0x188, 0x18c, 0x190, 0x194, 0x198, 0x19c,
0x1a0, 0x1a4, 0x1a8, 0x1ac, 0x1b0, 0x1b4, 0x1b8, 0x1bc, 0x1c0, 0x1c4,
0x1c8, 0x1cc, 0x1d0, 0x1d4, 0x1d8, 0x1dc, 0x1e0, 0x1e4, 0x1e8, 0x1ec,
0x1f0, 0x1f4, 0x1f8, 0x1fc, 0x204, 0x20c, 0x210, 0x214, 0x218, 0x21c,
0x220, 0x224, 0x228, 0x22c, 0x230, 0x234, 0x238, 0x268, 0x26c, 0x270,
0x274, 0x278, 0x290, 0x294, 0x298, 0x29c, 0x2a8, 0x2ac, 0x2b0, 0x2b4,
0x2b8, 0x2bc, 0x2d4, 0x2dc, 0x2e8, 0x2ec, 0x2f0, 0x2f4, 0x2f8, 0x2fc,
0x314, 0x318, 0x328, 0x32c, 0x348, 0x34c, 0x350, 0x354, 0x358, 0x35c,
0x384, 0x388, 0x38c, 0x398, 0x39c, 0x3d8, 0x3dc, 0x400, 0x404, 0x408,
0x40c, 0x410, 0x414, 0x418, 0x810, 0x814, 0x818, 0x830, 0x834, 0x838,
0x83c, 0x854, 0x858, 0x85c, 0xf60, 0xf64, 0xf70, 0xf74, 0xf78, 0xf7c,
0xf88, 0xf8c, 0xf90, 0xf94, 0xfa0, 0xfa4, 0xfa8, 0xfac, 0xfb8, 0xfbc,
0xfc0, 0xfc4, 0xfd0, 0xfd4, 0xfd8, 0xfdc, 0xfe8, 0xfec, 0xff0, 0x1f04,
0x1f0c, 0x1f10, 0x1f24, 0x1f28, 0x1f2c, 0x1f30, 0x1f34, 0x1f38, 0x1f3c
};

bool is_reg_can_read(uint32_t addr)
{
	int i;

	/*sizeof(unread_register)/sizeof(uint32_t)*/
	for (i = 0; i < sizeof(unread_register)/sizeof(uint32_t); i++) {
		if (addr == unread_register[i])
			return false;
	}

	return true;
}

void print_reg(uint start_addr, uint end_addr)
{
	int i;

	if (end_addr < start_addr)
		return;

	for (i = start_addr; i <= end_addr; i += sizeof(uint)) {
		if ((i - start_addr) % (sizeof(uint)*4) == 0)
			rx_pr("[0x%-4x] ", i);
		if (is_reg_can_read(i))
			rx_pr("0x%-8x,", hdmirx_rd_dwc(i));
		else
			rx_pr("xxxxxx    ,");

		if ((i - start_addr) % (sizeof(uint)*4) == sizeof(uint)*3)
			rx_pr("\n");
	}

	if ((end_addr - start_addr + sizeof(uint)) % (sizeof(uint)*4) != 0)
		rx_pr("\n");
}

void dump_reg(void)
{
	int i = 0;

	rx_pr("\n***Top registers***\n");
	rx_pr("[addr ]  addr + 0x0,");
	rx_pr("addr + 0x1,  addr + 0x2,	addr + 0x3\n");
	for (i = 0; i <= 0x24;) {
		rx_pr("[0x%-3x]", i);
		rx_pr("0x%-8x" , hdmirx_rd_top(i));
		rx_pr("0x%-8x,0x%-8x,0x%-8x\n",
			hdmirx_rd_top(i + 1),
			hdmirx_rd_top(i + 2),
			hdmirx_rd_top(i + 3));
		i = i + 4;
	}
	rx_pr("\n***PHY registers***\n");
	rx_pr("[addr ]  addr + 0x0,");
	rx_pr("addr + 0x1,addr + 0x2,");
	rx_pr("addr + 0x3\n");
	for (i = 0; i <= 0x9a;) {
		rx_pr("[0x%-3x]", i);
		rx_pr("0x%-8x", hdmirx_rd_phy(i));
		rx_pr("0x%-8x,0x%-8x,0x%-8x\n",
			hdmirx_rd_phy(i + 1),
			hdmirx_rd_phy(i + 2),
			hdmirx_rd_phy(i + 3));
		i = i + 4;
	}
	rx_pr("\n**Controller registers**\n");
	rx_pr("[addr ]  addr + 0x0,");
	rx_pr("addr + 0x4,  addr + 0x8,");
	rx_pr("addr + 0xc\n");
	print_reg(0, 0xfc);
	print_reg(0x140, 0x3ac);
	print_reg(0x3c0, 0x418);
	print_reg(0x480, 0x4bc);
	print_reg(0x600, 0x610);
	print_reg(0x800, 0x87c);
	print_reg(0x8e0, 0x8e0);
	print_reg(0x8fc, 0x8fc);
	print_reg(0xf60, 0xffc);
	print_reg(0x1f00, 0x1fc4);
	/* print_reg(0x2000, 0x21fc); */
	/* print_reg(0x2700, 0x2714); */
	/* print_reg(0x2f00, 0x2f14); */
	/* print_reg(0x3000, 0x3020); */
	/* print_reg(0x3040, 0x3054); */
	/* print_reg(0x3080, 0x3118); */
	/* print_reg(0x3200, 0x32e4); */

}


static void dump_hdcp_data(void)
{
	rx_pr("\n*************HDCP");
	rx_pr("***************");
	rx_pr("\n hdcp-seed = %d ",
		rx.hdcp.seed);
	/* KSV CONFIDENTIAL */
	rx_pr("\n hdcp-ksv = %x---%x",
		rx.hdcp.bksv[0],
		rx.hdcp.bksv[1]);
	rx_pr("\n*************HDCP end**********\n");
}

void dump_edid_reg(void)
{
	int i = 0;
	int j = 0;
	rx_pr("\n***********************\n");
	rx_pr("0x107 enable rgb range block\n");
	rx_pr("0x106 skyworth mst_or_mtk edid\n");
	rx_pr("0x105 mst sharp porduction edid\n");
	rx_pr("0x104 mst ATSC production edid\n");
	rx_pr("0x103 aml old edid, 4k*2k unsupported\n");
	rx_pr("********************************\n");
	/* 1024 = 64*16 */
	for (i = 0; i < 16; i++) {
		rx_pr("[%2d] ", i);
		for (j = 0; j < 16; j++) {
			rx_pr("0x%02lx, ",
			       hdmirx_rd_top(TOP_EDID_OFFSET +
					     (i * 16 + j)));
		}
		rx_pr("\n");
	}
}

void dump_hdr_reg(void)
{
	int i = 0;

	rx_pr("\n********** hdr *************\n");

	for (i = 0; i < sizeof(rx.hdr_info.hdr_data)/4; i++)
		rx_pr("playload[%d]: %#x\n", i ,
		*((unsigned int *)&(rx.hdr_info.hdr_data) + i));
	rx_pr("\n********** hdr end*************\n");
}

void timer_state(void)
{
	rx_pr("timer state:%d\n",
		rx.state);
}

int hdmirx_debug(const char *buf, int size)
{
	char tmpbuf[128];
	int i = 0;
	long adr;
	long value = 0;
	int ret = 0;

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;
	if (strncmp(tmpbuf, "help", 4) == 0) {
		rx_pr("*****************\n");
		rx_pr("reset0--hw_config\n");
		rx_pr("reset1--8bit phy rst\n");
		rx_pr("reset2--10bit phy rst\n");
		rx_pr("reset3--12bit phy rst\n");
		rx_pr("reset4--edid_update\n");
		rx_pr("reset5--esm rst\n");
		rx_pr("database--esm data addr\n");
		rx_pr("duk--dump duk\n");
		rx_pr("suspend_pddq--pddqdown\n");
		rx_pr("*****************\n");
	} else if (strncmp(tmpbuf, "hpd", 3) == 0)
		hdmirx_set_hpd(rx.port, tmpbuf[3] == '0' ? 0 : 1);
	else if (strncmp(tmpbuf, "cable_status", 12) == 0) {
		size = hdmirx_rd_top(TOP_HPD_PWR5V) >> 20;
		rx_pr("cable_status = %x\n", size);
	} else if (strncmp(tmpbuf, "signal_status", 13) == 0) {
		size = rx.no_signal;
		rx_pr("signal_status = %d\n", size);
	} else if (strncmp(tmpbuf, "reset", 5) == 0) {
		if (tmpbuf[5] == '0') {
			rx_pr(" hdmirx hw config\n");
			hdmirx_hw_config();
		} else if (tmpbuf[5] == '1') {
			rx_pr(" hdmirx phy init 8bit\n");
			hdmirx_phy_init(rx.port, 0);
		} else if (tmpbuf[5] == '2') {
			rx_pr(" hdmirx phy init 10bit\n");
			hdmirx_phy_init(rx.port, 1);
		} else if (tmpbuf[5] == '3') {
			rx_pr(" hdmirx phy init 12bit\n");
			hdmirx_phy_init(rx.port, 2);
		} else if (tmpbuf[5] == '4') {
			rx_pr(" edid update\n");
			hdmi_rx_ctrl_edid_update();
		} else if (tmpbuf[5] == '5') {
			#ifdef HDCP22_ENABLE
			hdmirx_hdcp22_esm_rst();
			#endif
		} else if (strncmp(tmpbuf + 5, "_on", 3) == 0) {
			reset_sw = 1;
			rx_pr("reset on!\n");
		} else if (strncmp(tmpbuf + 5, "_off", 4) == 0) {
			reset_sw = 0;
			rx_pr(" reset off!\n");
		}
	} else if (strncmp(tmpbuf, "state", 5) == 0) {
		dump_state(0xff);
	} else if (strncmp(tmpbuf, "database", 5) == 0) {
		rx_pr("data base = 0x%x\n", esm_data_base_addr);
	} else if (strncmp(tmpbuf, "hdcp14", 6) == 0) {
		hdmirx_set_hpd(rx.port, 0);
		force_hdcp14_en = 1;
		hdcp22_on = 0;
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x2);
		video_stable_to_esm = 0;
		rx.state = FSM_HPD_HIGH;
		rx.pre_state = FSM_HPD_HIGH;
		rx_pr("force hdcp1.4\n");
	} else if (strncmp(tmpbuf, "hdcpauto", 8) == 0) {
		hdmirx_set_hpd(rx.port, 0);
		hdcp22_on = 1;
		force_hdcp14_en = 0;
		hdmirx_hw_config();
		hpd_to_esm = 1;
		rx.state = FSM_HPD_HIGH;
		rx.pre_state = FSM_HPD_HIGH;
		rx_pr("hdcp22 auto\n");
	} else if (strncmp(tmpbuf, "pause", 5) == 0) {
		if (kstrtol(tmpbuf + 5, 10, &value) < 0)
			return -EINVAL;
		rx_pr("%s the state machine\n",
			value ? "pause" : "enable");
		sm_pause = value;
	} else if (strncmp(tmpbuf, "reg", 3) == 0) {
		dump_reg();
	}  else if (strncmp(tmpbuf, "duk", 3) == 0) {
		rx_pr("hdcp22=%d\n", rx_sec_set_duk());
	} else if (strncmp(tmpbuf, "edid", 4) == 0) {
		dump_edid_reg();
	} else if (strncmp(tmpbuf, "hdr", 3) == 0) {
		dump_hdr_reg();
	} else if (strncmp(tmpbuf, "hdcp123", 7) == 0) {
		dump_hdcp_data();
	} else if (strncmp(tmpbuf, "esmhpd", 6) == 0) {
		#ifdef HDCP22_ENABLE
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL,
		hdmirx_rd_dwc(DWC_HDCP22_CONTROL) | (1<<12));
		rx_pr("set esm hpd\n");
		#endif
	} else if (strncmp(tmpbuf, "esmclk", 6) == 0) {
		hdmirx_hdcp22_init();
		hdcp22_on = 1;
		rx_pr("clk & 22 on\n");
	} else if (strncmp(tmpbuf, "loadkey", 7) == 0) {
		rx_pr("load hdcp key\n");
		memcpy(&rx.hdcp, &init_hdcp_data,
		       sizeof(struct hdmi_rx_ctrl_hdcp));
		hdmirx_hw_config();
		pre_port = 0xfe;
	} else if (strncmp(tmpbuf, "timer_state", 11) == 0) {
		timer_state();
	} else if (strncmp(tmpbuf, "load22key", 9) == 0) {
		if (enable_hdcp22_loadkey) {
			rx_pr("load 2.2 key-a\n");
			ret = rx_sec_set_duk();
			rx_pr("ret = %d\n", ret);
			if (ret == 1) {
				if ((hdmirx_rd_dwc(0xf68) & _BIT(3)) == 0) {
					rx_pr("load-1\n");
					sm_pause = 1;
					hdmirx_set_hpd(rx.port, 0);
					hdcp22_on = 1;
					hdcp22_kill_esm = 1;
					mdelay(wait_hdcp22_cnt1);
					hdcp22_kill_esm = 0;
					mdelay(wait_hdcp22_cnt2);
					switch_set_state(&rx.hpd_sdev, 0x00);
					hpd_to_esm = 1;
					do_esm_rst_flag = 1;
					hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x0);
					hdmirx_hdcp22_esm_rst();
					mdelay(loadkey_22_hpd_delay);
					hdmirx_wr_dwc(DWC_HDCP22_CONTROL,
								0x1000);
					hdcp22_wr_top(TOP_SKP_CNTL_STAT, 0x1);
					hdmirx_hw_config();
					hdmirx_hdcp22_init();
					switch_set_state(&rx.hpd_sdev, 0x01);
					mdelay(wait_hdcp22_cnt);
					hdmirx_set_hpd(rx.port, 1);
					/* rx.state = FSM_HDMI5V_HIGH; */
					/* pre_port = 0xee; */
					sm_pause = 0;
				} else {
					rx_pr("load-2\n");
					sm_pause = 1;
					hdmirx_set_hpd(rx.port, 0);
					hdcp22_on = 1;
					hdcp22_kill_esm = 1;
					mdelay(wait_hdcp22_cnt1);
					hdcp22_kill_esm = 0;
					mdelay(wait_hdcp22_cnt2);
					switch_set_state(&rx.hpd_sdev, 0x00);
					hpd_to_esm = 1;
					do_esm_rst_flag = 1;
					hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x0);
					hdmirx_hdcp22_esm_rst();
					mdelay(loadkey_22_hpd_delay);
					hdmirx_wr_dwc(DWC_HDCP22_CONTROL,
								0x1000);
					hdcp22_wr_top(TOP_SKP_CNTL_STAT, 0x1);
					hdmirx_hw_config();
					hdmirx_hdcp22_init();
					switch_set_state(&rx.hpd_sdev, 0x01);
					mdelay(wait_hdcp22_cnt3);
					hdmirx_set_hpd(rx.port, 1);
					sm_pause = 0;
				}
			} else
				hdcp22_on = 0;
		} else
			rx_pr("load-2-no\n");
	} else if (strncmp(tmpbuf, "esm0", 4) == 0) {
		switch_set_state(&rx.hpd_sdev, 0x0);
	} else if (strncmp(tmpbuf, "esm1", 4) == 0) {
		switch_set_state(&rx.hpd_sdev, 0x01);
	} else if (strncmp(tmpbuf, "bist", 4) == 0) {
		sm_pause = 1;
		/* reset_sw = 0; */
		hdmirx_phy_bist_test(tmpbuf[4] == '0' ? 0 : 1);
	} else if (strncmp(tmpbuf, "clock", 5) == 0) {
		if (kstrtol(tmpbuf + 5, 10, &value) < 0)
			return -EINVAL;
		rx_pr("clock[%d] = %d\n",
			value, hdmirx_get_clock(value));
	} else if (strncmp(tmpbuf, "sample_rate", 11) == 0) {
		/* nothing */
	} else if (strncmp(tmpbuf, "prbs", 4) == 0) {
		/* nothing */
	} else if (strncmp(tmpbuf, "suspend_pddq", 12) == 0) {
		suspend_pddq = (tmpbuf[12] == '0' ? 0 : 1);
	} else if (tmpbuf[0] == 'w') {
		if (kstrtol(tmpbuf + 3, 16, &adr) < 0)
				return -EINVAL;
		rx_pr("adr = %x\n", adr);
		if (kstrtol(buf + i + 1, 16, &value) < 0)
			return -EINVAL;
		rx_pr("value = %x\n", value);
		if (tmpbuf[1] == 'h') {
			if (buf[2] == 't') {
				hdmirx_wr_top(adr, value);
				rx_pr("write %x to TOP [%x]\n",
					value, adr);
			} else if (buf[2] == 'd') {
				hdmirx_wr_dwc(adr, value);
				rx_pr("write %x to DWC [%x]\n",
					value, adr);
			} else if (buf[2] == 'p') {
				hdmirx_wr_phy(adr, value);
				rx_pr("write %x to PHY [%x]\n",
					value, adr);
			#ifdef HDCP22_ENABLE
			} else if (buf[2] == 'h') {
				hdcp22_wr_top(adr, value);
				/* hdcp22_wr(adr, value); */
				rx_pr("write %x to hdcp [%x]\n",
					value, adr);
			} else if (buf[2] == 'c') {
				rx_hdcp22_wr_reg(adr, value);
				/* hdcp22_wr(adr, value); */
				rx_pr("write %x to chdcp [%x]\n",
					value, adr);
			#endif
			}
		} else if (buf[1] == 'c') {
			aml_write_cbus(adr, value);
			rx_pr("write %x to CBUS [%x]\n", value, adr);
		} else if (buf[1] == 'p') {
			/* WRITE_APB_REG(adr, value); */
		} else if (buf[1] == 'l') {
			/* aml_write_cbus(MDB_CTRL, 2); */
			/* aml_write_cbus(MDB_ADDR_REG, adr); */
			/* aml_write_cbus(MDB_DATA_REG, value); */
		} else if (buf[1] == 'r') {
			/* aml_write_cbus(MDB_CTRL, 1); */
			/* aml_write_cbus(MDB_ADDR_REG, adr); */
			/* aml_write_cbus(MDB_DATA_REG, value); */
		}
	} else if (tmpbuf[0] == 'r') {
		if (tmpbuf[1] == 'h') {
			if (kstrtol(tmpbuf + 3, 16, &adr) < 0)
				return -EINVAL;
			if (tmpbuf[2] == 't') {
				value = hdmirx_rd_top(adr);
				rx_pr("TOP [%x]=%x\n", adr, value);
			} else if (tmpbuf[2] == 'd') {
				value = hdmirx_rd_dwc(adr);
				rx_pr("DWC [%x]=%x\n", adr, value);
			} else if (tmpbuf[2] == 'p') {
				value = hdmirx_rd_phy(adr);
				rx_pr("PHY [%x]=%x\n", adr, value);
			#ifdef HDCP22_ENABLE
			} else if (tmpbuf[2] == 'h') {
				value = hdcp22_rd_top(adr);
				/* value = hdcp22_rd(adr); */
				rx_pr("hdcp [%x]=%x\n", adr, value);
			} else if (tmpbuf[2] == 'c') {
				value = rx_hdcp22_rd_reg(adr);
				/* value = hdcp22_rd(adr); */
				rx_pr("chdcp [%x]=%x\n", adr, value);
			#endif
			}
		} else if (buf[1] == 'c') {
			/* value = READ_MPEG_REG(adr); */
			rx_pr("CBUS reg[%x]=%x\n", adr, value);
		} else if (buf[1] == 'p') {
			/* value = READ_APB_REG(adr); */
			rx_pr("APB reg[%x]=%x\n", adr, value);
		} else if (buf[1] == 'l') {
			/* aml_write_cbus(MDB_CTRL, 2); */
			/* aml_write_cbus(MDB_ADDR_REG, adr); */
			/* value = READ_MPEG_REG(MDB_DATA_REG); */
			rx_pr("LMEM[%x]=%x\n", adr, value);
		} else if (buf[1] == 'r') {
			/* aml_write_cbus(MDB_CTRL, 1); */
			/* aml_write_cbus(MDB_ADDR_REG, adr); */
			/* value = READ_MPEG_REG(MDB_DATA_REG); */
			rx_pr("amrisc reg[%x]=%x\n", adr, value);
		}
	} else if (tmpbuf[0] == 'v') {
		rx_pr("------------------\n");
		rx_pr("Hdmirx version: %s\n", RX_VER0);
		rx_pr("Hdmirx version: %s\n", RX_VER1);
		rx_pr("Hdmirx version: %s\n", RX_VER2);
		rx_pr("Hdmirx version: %s\n", RX_VER3);
		rx_pr("------------------\n");
	}
	return 0;
}

/***********************
    hdmirx_hw_init
    hdmirx_hw_uninit
*************************/
void hdmirx_hw_init(enum tvin_port_e port)
{
	if (sm_pause)
		return;

	/* memset(&rx, 0, sizeof(struct rx_s)); */
	/* memset(rx.pow5v_state, */
	/*	0, */
	/*	sizeof(rx.pow5v_state)); */
	memset(&rx.pre, 0,
		sizeof(struct hdmi_rx_ctrl_video));
	memcpy(rx.hdcp.bksv, init_hdcp_data.bksv,
		sizeof(init_hdcp_data.bksv));
	memcpy(rx.hdcp.keys, init_hdcp_data.keys,
		sizeof(init_hdcp_data.keys));

	memset(&rx.vendor_specific_info, 0,
			sizeof(struct vendor_specific_info_s));
	rx.no_signal = false;
	rx.phy.cfg_clk = CFG_CLK;
	rx.phy.lock_thres = LOCK_THRES;
	rx.phy.fsm_enhancement = fsm_enhancement;
	rx.phy.port_select_ovr_en = port_select_ovr_en;
	rx.phy.phy_cmu_config_force_val = phy_cmu_config_force_val;
	rx.phy.phy_system_config_force_val = phy_system_config_force_val;
	rx.ctrl.md_clk = 24000;
	rx.ctrl.tmds_clk = 0;
	rx.ctrl.acr_mode = acr_mode;
	wait_no_sig_cnt = 0;
	use_dwc_reset = true;
	is_hdcp_source = true;
	if (hdmirx_repeat_support())
		rx.hdcp.repeat = repeat_plug;
	else
		rx.hdcp.repeat = 0;
	rx.port = (port_map >> ((port - TVIN_PORT_HDMI0) << 2)) & 0xf;
	/* if (pre_port == 0xff)
		hdmirx_wr_top(TOP_HPD_PWR5V, 0x1f & (~(1<<rx.port)));
	*/
	if (pre_port != rx.port) {
		#ifdef HDCP22_ENABLE
		if (hdcp22_on) {
			esm_set_stable(0);
			hpd_to_esm = 1;
			/* switch_set_state(&rx.hpd_sdev, 0x01); */
			if (log_level & VIDEO_LOG)
				rx_pr("switch_set_state:%d\n", pwr_sts);
		}
		#endif
		rx.state = FSM_HPD_LOW;
		pre_port = rx.port;
		if (rx.boot_flag) {
			rx.boot_flag = FALSE;
			hdmirx_set_hpd(rx.port, 1);
		} else
			hdmirx_set_hpd(rx.port, 0);
		hdmirx_hw_config();
	} else {
		if (0 == get_cur_hpd_sts())
			rx.state = FSM_HPD_HIGH;
		else
			rx.state = FSM_SIG_STABLE;
	}
	rx_pr("%s %d nosignal:%d\n", __func__, rx.port, rx.no_signal);

}

void hdmirx_hw_uninit(void)
{
	if (sm_pause)
		return;

	/* hdcp_close(); */

	/* set all hpd low  */
	/* aml_write_cbus(PREG_PAD_GPIO5_O, */
	/* READ_CBUS_REG(PREG_PAD_GPIO5_O) | */
	/* ((1<<1)|(1<<5)|(1<<9)|(1<<13))); */

	/*hdmirx_set_hpd(rx.port, 0);*/

	/* hdmirx_wr_top(TOP_INTR_MASKN, 0); */
	/*hdmirx_irq_close();*/
	/* audio_status_init(); */

	/* rx.ctrl.status = 0; */
	/* rx.ctrl.tmds_clk = 0; */
	/* ctx->bsp_reset(true); */

	/* hdmirx_phy_reset(true); */
	/* hdmirx_phy_pddq(1); */
}


