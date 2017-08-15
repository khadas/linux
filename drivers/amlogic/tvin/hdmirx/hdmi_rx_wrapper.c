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
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/tvin/tvin.h>

/* Local include */
#include "hdmirx_drv.h"
#include "hdmi_rx_reg.h"
#include "hdmi_rx_eq.h"

static int pll_unlock_cnt;
static int pll_unlock_max = 150;
MODULE_PARM_DESC(pll_unlock_max, "\n pll_unlock_max\n");
module_param(pll_unlock_max, int, 0664);

static int pll_lock_cnt;
static int pll_lock_max = 3;
MODULE_PARM_DESC(pll_lock_max, "\n pll_lock_max\n");
module_param(pll_lock_max, int, 0664);

static int dwc_rst_wait_cnt;
static int dwc_rst_wait_cnt_max = 1;
MODULE_PARM_DESC(dwc_rst_wait_cnt_max, "\n dwc_rst_wait_cnt_max\n");
module_param(dwc_rst_wait_cnt_max, int, 0664);


static int sig_stable_cnt;
static int sig_stable_max = 10;
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
static int sig_unready_max = 5;/* 10; */
MODULE_PARM_DESC(sig_unready_max, "\n sig_unready_max\n");
module_param(sig_unready_max, int, 0664);

static bool enable_hpd_reset;
MODULE_PARM_DESC(enable_hpd_reset, "\n enable_hpd_reset\n");
module_param(enable_hpd_reset, bool, 0664);

static int pow5v_max_cnt = 3;
MODULE_PARM_DESC(pow5v_max_cnt, "\n pow5v_max_cnt\n");
module_param(pow5v_max_cnt, int, 0664);

static int sig_unstable_reset_hpd_cnt;
static int sig_unstable_reset_hpd_max = 5;
MODULE_PARM_DESC(sig_unstable_reset_hpd_max,
		 "\n sig_unstable_reset_hpd_max\n");
module_param(sig_unstable_reset_hpd_max, int, 0664);

int rgb_quant_range;
MODULE_PARM_DESC(rgb_quant_range, "\n rgb_quant_range\n");
module_param(rgb_quant_range, int, 0664);

int yuv_quant_range;
MODULE_PARM_DESC(yuv_quant_range, "\n yuv_quant_range\n");
module_param(yuv_quant_range, int, 0664);

int it_content;
MODULE_PARM_DESC(it_content, "\n it_content\n");
module_param(it_content, int, 0664);

/* timing diff offset */
static int diff_pixel_th = 2;
static int diff_line_th = 10;
static int diff_frame_th = 40; /* (25hz-24hz)/2 = 50/100 */
MODULE_PARM_DESC(diff_pixel_th, "\n diff_pixel_th\n");
module_param(diff_pixel_th, int, 0664);
MODULE_PARM_DESC(diff_line_th, "\n diff_line_th\n");
module_param(diff_line_th, int, 0664);
MODULE_PARM_DESC(diff_frame_th, "\n diff_frame_th\n");
module_param(diff_frame_th, int, 0664);

static int port_map = 0x4231;
MODULE_PARM_DESC(port_map, "\n port_map\n");
module_param(port_map, int, 0664);

static int edid_mode;
MODULE_PARM_DESC(edid_mode, "\n edid_mode\n");
module_param(edid_mode, int, 0664);

static int force_vic;
MODULE_PARM_DESC(force_vic, "\n force_vic\n");
module_param(force_vic, int, 0664);

static int repeat_check = 1;
MODULE_PARM_DESC(repeat_check, "\n repeat_check\n");
module_param(repeat_check, int, 0664);

static int force_audio_sample_rate;
MODULE_PARM_DESC(force_audio_sample_rate, "\n force_audio_sample_rate\n");
module_param(force_audio_sample_rate, int, 0664);

/* used in other module */
static int audio_sample_rate;
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
/* used in other module */

int log_level = LOG_EN | HDCP_LOG | EQ_LOG;
MODULE_PARM_DESC(log_level, "\n log_level\n");
module_param(log_level, int, 0664);

static bool auto_switch_off;	/* only for hardware test */
MODULE_PARM_DESC(auto_switch_off, "\n auto_switch_off\n");
module_param(auto_switch_off, bool, 0664);

int clk_unstable_cnt;
static int clk_unstable_max = 600;
MODULE_PARM_DESC(clk_unstable_max, "\n clk_unstable_max\n");
module_param(clk_unstable_max, int, 0664);

int clk_stable_cnt;
static int clk_stable_max = 8;
MODULE_PARM_DESC(clk_stable_max, "\n clk_stable_max\n");
module_param(clk_stable_max, int, 0664);

static int unnormal_wait_max = 200;
MODULE_PARM_DESC(unnormal_wait_max, "\n unnormal_wait_max\n");
module_param(unnormal_wait_max, int, 0664);

static int wait_no_sig_max = 600;
MODULE_PARM_DESC(wait_no_sig_max, "\n wait_no_sig_max\n");
module_param(wait_no_sig_max, int, 0664);

/*edid original data from device*/
static unsigned char receive_hdr_lum[MAX_HDR_LUMI];
static int hdr_lum_len = MAX_HDR_LUMI;
MODULE_PARM_DESC(receive_hdr_lum, "\n receive_hdr_lum\n");
module_param_array(receive_hdr_lum, byte, &hdr_lum_len, 0664);

static bool new_hdr_lum;
MODULE_PARM_DESC(new_hdr_lum, "\n new_hdr_lum\n");
module_param(new_hdr_lum, bool, 0664);


#ifdef HDCP22_ENABLE
/* to inform ESM whether the cable is connected or not */
bool hpd_to_esm;
MODULE_PARM_DESC(hpd_to_esm, "\n hpd_to_esm\n");
module_param(hpd_to_esm, bool, 0664);

bool hdcp22_kill_esm;
MODULE_PARM_DESC(hdcp22_kill_esm, "\n hdcp22_kill_esm\n");
module_param(hdcp22_kill_esm, bool, 0664);

static int hdcp22_capable_sts = 0xff;
MODULE_PARM_DESC(hdcp22_capable_sts, "\n hdcp22_capable_sts\n");
module_param(hdcp22_capable_sts, int, 0664);

static bool esm_auth_fail_en;
MODULE_PARM_DESC(esm_auth_fail_en, "\n esm_auth_fail_en\n");
module_param(esm_auth_fail_en, bool, 0664);

static int hdcp22_reset_max = 20;
MODULE_PARM_DESC(hdcp22_reset_max, "\n hdcp22_reset_max\n");
module_param(hdcp22_reset_max, int, 0664);

static int hdcp22_auth_sts = 0xff;
MODULE_PARM_DESC(hdcp22_auth_sts, "\n hdcp22_auth_sts\n");
module_param(hdcp22_auth_sts, int, 0664);
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

unsigned int esm_data_base_addr;
/* MODULE_PARM_DESC(esm_data_base_addr,"\n esm_data_base_addr\n"); */
/* module_param(esm_data_base_addr, unsigned, 0664); */
#endif

bool is_hdcp_source = true;
MODULE_PARM_DESC(is_hdcp_source, "\n is_hdcp_source\n");
module_param(is_hdcp_source, bool, 0664);

int stable_check_lvl = 0x7ff;
module_param(stable_check_lvl, int, 0664);
MODULE_PARM_DESC(stable_check_lvl, "stable_check_lvl");

bool hdcp22_stop_auth;
module_param(hdcp22_stop_auth, bool, 0664);
MODULE_PARM_DESC(hdcp22_stop_auth, "hdcp22_stop_auth");

bool hdcp22_esm_reset2;
module_param(hdcp22_esm_reset2, bool, 0664);
MODULE_PARM_DESC(hdcp22_esm_reset2, "hdcp22_esm_reset2");

bool hdcp22_reauth_enable;
int do_hpd_reset_flag;
bool hdcp22_stop_auth_enable;
bool hdcp22_esm_reset2_enable;
static int sm_pause;
int pre_port = 0xff;
/*uint32_t irq_flag;*/

/*------------------------external function------------------------------*/
static void dump_state(unsigned char enable);
static void dump_hdcp_data(void);
/*------------------------external function end------------------------------*/
struct rx_s rx;


struct hdmi_rx_ctrl_hdcp init_hdcp_data;
static char key_buf[MAX_KEY_BUF_SIZE];
static int key_size;

static unsigned char edid_temp[EDID_SIZE];
static char edid_buf[MAX_EDID_BUF_SIZE];
static int edid_size;

/* hdmi1.4 edid */
static unsigned char edid_14[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x52, 0x74, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x19, 0x1B, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x54,
0x45, 0x53, 0x54, 0x0A, 0x20, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xEE,
0x02, 0x03, 0x34, 0xF0, 0x55, 0x5F, 0x10, 0x1F,
0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06,
0x01, 0x5D, 0x26, 0x09, 0x07, 0x03, 0x15, 0x07,
0x50, 0x83, 0x01, 0x00, 0x00, 0x6E, 0x03, 0x0C,
0x00, 0x10, 0x00, 0x98, 0x3C, 0x20, 0x80, 0x80,
0x01, 0x02, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x96,
};
/* 1.4 edid,support dobly,MAT,DTS,ATMOS*/
static unsigned char edid_14_aud[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x52, 0x74, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x19, 0x1B, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x54,
0x45, 0x53, 0x54, 0x0A, 0x20, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xEE,
0x02, 0x03, 0x40, 0xF0, 0x55, 0x5F, 0x10, 0x1F,
0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06,
0x01, 0x5D, 0x32, 0x09, 0x07, 0x03, 0x15, 0x07,
0x50, 0x55, 0x07, 0x01, 0x67, 0x7E, 0x01, 0x0F,
0x7F, 0x07, 0x3D, 0x07, 0x50, 0x83, 0x01, 0x00,
0x00, 0x6E, 0x03, 0x0C, 0x00, 0x10, 0x00, 0x98,
0x3C, 0x20, 0x80, 0x80, 0x01, 0x02, 0x03, 0x04,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12,
};
/* 1.4 edid,support 420 capability map block*/
static unsigned char edid_14_420c[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x52, 0x74, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x19, 0x1B, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x54,
0x45, 0x53, 0x54, 0x0A, 0x20, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xEE,
0x02, 0x03, 0x3E, 0xF0, 0x59, 0x5F, 0x10, 0x1F,
0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06,
0x01, 0x5D, 0x61, 0x60, 0x65, 0x66, 0x26, 0x09,
0x07, 0x03, 0x15, 0x07, 0x50, 0x83, 0x01, 0x00,
0x00, 0x6E, 0x03, 0x0C, 0x00, 0x10, 0x00, 0x98,
0x3C, 0x20, 0x80, 0x80, 0x01, 0x02, 0x03, 0x04,
0xE5, 0x0F, 0x00, 0x00, 0xE0, 0x01, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x27,
};

/* 1.4 edid,support 420  video data block*/
static unsigned char edid_14_420vd[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x52, 0x74, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x19, 0x1B, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x54,
0x45, 0x53, 0x54, 0x0A, 0x20, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xEE,
0x02, 0x03, 0x3A, 0xF0, 0x55, 0x5F, 0x10, 0x1F,
0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06,
0x01, 0x5D, 0x26, 0x09, 0x07, 0x03, 0x15, 0x07,
0x50, 0x83, 0x01, 0x00, 0x00, 0x6E, 0x03, 0x0C,
0x00, 0x10, 0x00, 0x98, 0x3C, 0x20, 0x80, 0x80,
0x01, 0x02, 0x03, 0x04, 0xE5, 0x0E, 0x61, 0x60,
0x65, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
};

/* 2.0 edid,support HDR,DOLBYVISION */
static unsigned char edid_20[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x52, 0x74, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x19, 0x1B, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x54,
0x45, 0x53, 0x54, 0x0A, 0x20, 0x20, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xEE,
0x02, 0x03, 0x60, 0xF0, 0x5C, 0x5F, 0x10, 0x1F,
0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x3C, 0x3E,
0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06,
0x01, 0x5D, 0x60, 0x61, 0x65, 0x66, 0x62, 0x63,
0x64, 0x26, 0x09, 0x07, 0x03, 0x15, 0x07, 0x50,
0x83, 0x01, 0x00, 0x00, 0x6E, 0x03, 0x0C, 0x00,
0x10, 0x00, 0x98, 0x3C, 0x20, 0x80, 0x80, 0x01,
0x02, 0x03, 0x04, 0x67, 0xD8, 0x5D, 0xC4, 0x01,
0x78, 0x88, 0x03, 0xE3, 0x05, 0x40, 0x01, 0xE5,
0x0F, 0x00, 0x00, 0xE0, 0x01, 0xE3, 0x06, 0x05,
0x01, 0xEE, 0x01, 0x46, 0xD0, 0x00, 0x26, 0x0F,
0x8B, 0x00, 0xA8, 0x53, 0x4B, 0x9D, 0x27, 0x0B,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83,
};

unsigned char *edid_list[] = {
	edid_buf,
	edid_14,
	edid_14_aud,
	edid_14_420c,
	edid_14_420vd,
	edid_20,
};

const uint32_t sr_tbl[] = {
	32000,
	44100,
	48000,
	88200,
	96000,
	176400,
	192000,
	0
};

struct freq_ref_s freq_ref[] = {
	/* interlace 420 3d ref_freq hac vac repeat frame_rate vic */
		/* 4k2k 420mode hactive = hactive/2 */
	{0, 3, 0, 0, 1920, 2160, 0, 0, HDMI_2160p_50hz_420},
	{0, 3, 0, 0, 1920, 2160, 0, 0, HDMI_2160p_60hz_420},
	{0, 3, 0, 0, 2048, 2160, 0, 0, HDMI_4096p_50hz_420},
	{0, 3, 0, 0, 2048, 2160, 0, 0, HDMI_4096p_60hz_420},
	{0, 3, 0, 74250, 960, 1080, 0, 5000, HDMI_1080p50},
	{0, 3, 0, 74250, 960, 1080, 0, 6000, HDMI_1080p60},
	/* basic format*/
	{0, 0, 0, 25000, 640,  480,  0, 6000, HDMI_640x480p60},
	{0, 0, 0, 27000, 720,  480,  0, 6000, HDMI_480p60},
	{0, 0, 1, 27000, 720,  1005, 0, 6000, HDMI_480p60},
	{1, 0, 0, 27000, 1440, 240,  1, 6000, HDMI_480i60},
	{0, 0, 0, 27000, 720,  576,  0, 5000, HDMI_576p50},
	{0, 0, 1, 27000, 720,  1201, 0, 5000, HDMI_576p50},
	{1, 0, 0, 27000, 1440, 288,  1, 5000, HDMI_576i50},
	{1, 0, 0, 27000, 1440, 145,  2, 5000, HDMI_576i50_16x9},
	{0, 0, 0, 74250, 1280, 720,  0, 6000, HDMI_720p60},
	{0, 0, 1, 74250, 1280, 1470, 0, 6000, HDMI_720p60},
	{0, 0, 0, 74250, 1280, 720,  0, 5000, HDMI_720p50},
	{0, 0, 1, 74250, 1280, 1470, 0, 5000, HDMI_720p50},
	{1, 0, 0, 74250, 1920, 540,  0, 6000, HDMI_1080i60},
	{1, 0, 1, 74250, 1920, 2228, 0, 6000, HDMI_1080i60},
	{1, 0, 2, 74250, 1920, 1103, 0, 6000, HDMI_1080i60},
	{1, 0, 0, 74250, 1920, 540,  0, 5000, HDMI_1080i50},
	{1, 0, 1, 74250, 1920, 2228, 0, 5000, HDMI_1080i50},
	{1, 0, 2, 74250, 1920, 1103, 0, 5000, HDMI_1080i50},
	{0, 0, 0, 148500, 1920, 1080, 0, 6000, HDMI_1080p60},
	{0, 0, 2, 148500, 1920, 2160, 0, 6000, HDMI_1080p60},
	{0, 0, 0, 74250, 1920, 1080, 0, 2400, HDMI_1080p24},
	{0, 0, 1, 74250, 1920, 2205, 0, 2400, HDMI_1080p24},
	{0, 0, 2, 74250, 1920, 2160, 0, 2400, HDMI_1080p24},
	{0, 0, 0, 74250, 1920, 1080, 0, 2500, HDMI_1080p25},
	{0, 0, 1, 74250, 1920, 2205, 0, 2500, HDMI_1080p25},
	{0, 0, 2, 74250, 1920, 2160, 0, 2500, HDMI_1080p25},
	{0, 0, 0, 74250, 1920, 1080, 0, 3000, HDMI_1080p30},
	{0, 0, 1, 74250, 1920, 2205, 0, 3000, HDMI_1080p30},
	{0, 0, 2, 74250, 1920, 2160, 0, 3000, HDMI_1080p30},
	{0, 0, 0, 148500, 1920, 1080, 0, 5000, HDMI_1080p50},
	{0, 0, 2, 148500, 1920, 2160, 0, 5000, HDMI_1080p50},
	/* extend format */
	{0, 0, 0, 27000, 1440, 240, 1, 6000, HDMI_1440x240p60},
	{1, 0, 0, 54000, 2880, 240, 9, 6000, HDMI_2880x480i60},
	{0, 0, 0, 54000, 2880, 240, 9, 6000, HDMI_2880x240p60},
	{0, 0, 0, 54000, 1440, 480, 9, 6000, HDMI_1440x480p60},

	{0, 0, 0, 27000, 1440, 288, 1, 5000, HDMI_1440x288p50},
	{1, 0, 0, 54000, 2880, 288, 9, 5000, HDMI_2880x576i50},
	{0, 0, 0, 54000, 2880, 288, 9, 5000, HDMI_2880x288p50},
	{0, 0, 0, 54000, 1440, 576, 9, 5000, HDMI_1440x576p50},

	{0, 0, 0, 108000, 2880, 480, 9, 6000, HDMI_2880x480p60},
	{0, 0, 0, 108000, 2880, 576, 9, 5000, HDMI_2880x576p50},
	{1, 0, 0, 72000, 1920, 540, 0, 5000, HDMI_1080i50_1250},
	{0, 0, 0, 74250, 1280, 720, 0, 2400, HDMI_720p24},
	{0, 0, 1, 74250, 1280, 1470, 0, 2400, HDMI_720p24},
	{0, 0, 0, 74250, 1280, 720, 0, 2500, HDMI_720p25},
	{0, 0, 1, 74250, 1280, 1470, 0, 2500, HDMI_720p25},
	{0, 0, 0, 74250, 1280, 720, 0, 3000, HDMI_720p30},
	{0, 0, 1, 74250, 1280, 1470, 0, 3000, HDMI_720p30},

	/*extend format 100hz,120hz,200hz,240hz*/
	{0, 0, 0, 54000, 720, 480, 0, 12000, HDMI_480p60},
	{0, 0, 1, 54000, 720, 1005, 0, 12000, HDMI_480p60},
	{1, 0, 0, 54000, 1440, 240, 1, 12000, HDMI_480i60},
	{0, 0, 0, 108000, 720, 480, 0, 24000, HDMI_480p60},
	{0, 0, 1, 108000, 720, 1005, 0, 24000, HDMI_480p60},
	{1, 0, 0, 108000, 1440, 240, 1, 24000, HDMI_480i60},

	{0, 0, 0, 54000, 720, 576, 0, 10000, HDMI_576p50},
	{0, 0, 1, 54000, 720, 1201, 0, 10000, HDMI_576p50},
	{1, 0, 0, 54000, 1440, 288, 1, 10000, HDMI_576i50},
	{1, 0, 0, 54000, 1440, 145, 2, 10000, HDMI_576i50_16x9},
	{0, 0, 0, 108000, 720, 576, 0, 20000, HDMI_576p50},
	{0, 0, 1, 108000, 720, 1201, 0, 20000, HDMI_576p50},
	{1, 0, 0, 108000, 1440, 288, 1, 20000, HDMI_576i50},
	{1, 0, 0, 108000, 1440, 145, 2, 20000, HDMI_576i50_16x9},

	{0, 0, 0, 148500, 1280, 720, 0, 12000, HDMI_720p60},
	{0, 0, 1, 148500, 1280, 1470, 0, 12000, HDMI_720p60},
	{0, 0, 0, 297000, 1280, 720, 0, 24000, HDMI_720p60},
	{0, 0, 1, 297000, 1280, 1470, 0, 24000, HDMI_720p60},
	{0, 0, 0, 148500, 1280, 720, 0, 10000, HDMI_720p50},
	{0, 0, 1, 148500, 1280, 1470, 0, 10000, HDMI_720p50},
	{0, 0, 0, 297000, 1280, 720, 0, 20000, HDMI_720p50},
	{0, 0, 1, 297000, 1280, 1470, 0, 20000, HDMI_720p50},

	{1, 0, 0, 148500, 1920, 540, 0, 12000, HDMI_1080i60},
	{1, 0, 1, 148500, 1920, 2228, 0, 12000, HDMI_1080i60},
	{1, 0, 2, 148500, 1920, 1103, 0, 12000, HDMI_1080i60},
	{1, 0, 0, 297000, 1920, 540, 0, 24000, HDMI_1080i60},
	{1, 0, 1, 297000, 1920, 2228, 0, 24000, HDMI_1080i60},
	{1, 0, 2, 297000, 1920, 1103, 0, 24000, HDMI_1080i60},
	{1, 0, 0, 148500, 1920, 540, 0, 10000, HDMI_1080i50},
	{1, 0, 1, 148500, 1920, 2228, 0, 10000, HDMI_1080i50},
	{1, 0, 2, 148500, 1920, 1103, 0, 10000, HDMI_1080i50},
	{1, 0, 0, 297000, 1920, 540, 0, 20000, HDMI_1080i50},
	{1, 0, 1, 297000, 1920, 2228, 0, 20000, HDMI_1080i50},
	{1, 0, 2, 297000, 1920, 1103, 0, 20000, HDMI_1080i50},

	{0, 0, 0, 297000, 1920, 1080, 0, 12000, HDMI_1080p60},
	{0, 0, 0, 297000, 1920, 1080, 0, 10000, HDMI_1080p50},

	/* vesa format*/
	{0, 0, 0, 0, 800, 600, 0, 0, HDMI_800_600},
	{0, 0, 0, 0, 1024, 768, 0, 0, HDMI_1024_768},
	{0, 0, 0, 0, 720, 400, 0, 0, HDMI_720_400},
	{0, 0, 0, 0, 1280, 768, 0, 0, HDMI_1280_768},
	{0, 0, 0, 0, 1280, 800, 0, 0, HDMI_1280_800},
	{0, 0, 0, 0, 1280, 960, 0, 0, HDMI_1280_960},
	{0, 0, 0, 0, 1280, 1024, 0, 0, HDMI_1280_1024},
	{0, 0, 0, 0, 1360, 768, 0, 0, HDMI_1360_768},
	{0, 0, 0, 0, 1366, 768, 0, 0, HDMI_1366_768},
	{0, 0, 0, 0, 1600, 1200, 0, 0, HDMI_1600_1200},
	{0, 0, 0, 0, 1600, 900, 0, 0, HDMI_1600_900},
	{0, 0, 0, 0, 1920, 1200, 0, 0, HDMI_1920_1200},
	{0, 0, 0, 0, 1440, 900, 0, 0, HDMI_1440_900},
	{0, 0, 0, 0, 1400, 1050, 0, 0, HDMI_1400_1050},
	{0, 0, 0, 0, 1680, 1050, 0, 0, HDMI_1680_1050},

	/* 4k2k mode */
	{0, 0, 0, 0, 3840, 2160, 0, 0, HDMI_3840_2160p},
	{0, 0, 0, 0, 4096, 2160, 0, 0, HDMI_4096_2160p},

	{0, 0, 0, 30000, 640, 480, 0, 3600, HDMI_640x480p72},
	{0, 0, 0, 31250, 640, 480, 0, 3750, HDMI_640x480p75},
	/* interlace 420 3d ref_freq hac vac repeat frame_rate vic */
	{0, 0, 0, 312951, 2560, 1440, 0, 0, HDMI_2560_1440},
	{0, 0, 1, 312951, 2560, 3488, 0, 0, HDMI_2560_1440},
	{0, 0, 2, 312951, 2560, 2986, 0, 0, HDMI_2560_1440},

	/* for AG-506 */
	{0, 0, 0, 27000, 720, 483, 0, 6000, HDMI_480p60},
	{0, 0, 0, 0, 0, 0, 0, 0, 0}
};

static unsigned int wr_only_register[] = {
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

/*------------------------variable define end------------------------------*/
bool is_hdcp14_on(void)
{
	return (rx.hdcp.bksv[1] == 0) ? false : true;
}

void hdmirx_hdcp_version_set(enum hdcp_version_e version)
{
	if (version <= HDCP_VERSION_22) {
		rx.hdcp.hdcp_version = version;
		rx_pr("set hdcp version:%d\n", rx.hdcp.hdcp_version);
	}
}

void repeater_dwork_handle(struct work_struct *work)
{
	if (hdmirx_repeat_support()) {
		if (rx.hdcp.hdcp_version && hdmirx_is_key_write()
			&& rx.open_fg) {
			switch_set_state(&rx.hdcp.switch_hdcp_auth, 0);
			switch_set_state(&rx.hdcp.switch_hdcp_auth,
					rx.hdcp.hdcp_version);
		}
	}
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

void hdmirx_dv_packet_stop(void)
{
	if (rx.vsi_info.dolby_vision_sts == DOLBY_VERSION_START) {
		if (((hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD0) & 0xFF) != 0)
		|| ((hdmirx_rd_dwc(DWC_PDEC_VSI_PLAYLOAD0) & 0xFF00) != 0))
			return;
		if (rx.vsi_info.dolby_vision) {
			rx.vsi_info.dolby_vision = FALSE;
			rx.vsi_info.packet_stop = 0;
		} else
			rx.vsi_info.packet_stop++;
		if (rx.vsi_info.packet_stop > DV_STOP_PACKET_MAX) {
			rx.vsi_info.dolby_vision_sts =
							DOLBY_VERSION_STOP;
				rx_pr("no dv packet receive stop\n");
			}
	}
}

void rx_getaudinfo(struct aud_info_s *audio_info)
{
	/*get AudioInfo */
	audio_info->coding_type =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CODING_TYPE);
	audio_info->channel_count =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CHANNEL_COUNT);

	audio_info->sample_frequency =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, SAMPLE_FREQ);
	audio_info->sample_size =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, SAMPLE_SIZE);
	audio_info->coding_extension =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, AIF_DATA_BYTE_3);
	audio_info->channel_allocation =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB0, CH_SPEAK_ALLOC);
	/*
	audio_info->down_mix_inhibit =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB1, DWNMIX_INHIBIT);
	audio_info->level_shift_value =
		hdmirx_rd_bits_dwc(DWC_PDEC_AIF_PB1, LEVEL_SHIFT_VAL);
	*/
	audio_info->aud_packet_received =
			hdmirx_rd_bits_dwc(DWC_PDEC_AUD_STS, AUDS_RCV);
	audio_info->cts = hdmirx_rd_dwc(DWC_PDEC_ACR_CTS);
	audio_info->n = hdmirx_rd_dwc(DWC_PDEC_ACR_N);
	if (audio_info->cts != 0) {
		audio_info->arc = (hdmirx_get_tmds_clock()/audio_info->cts)*
			audio_info->n/128;
	} else
		audio_info->arc = 0;
}

/**
 * Clock event handler
 * @param[in,out] ctx context information
 * @return error code
 */
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

/*static int md_handler(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;

	if (rx.state != FSM_SIG_READY)
		return 0;

	if (sm_pause)
		return 0;

	if (ctx == 0)
		return -EINVAL;

		if (log_level&0x400)
			rx_pr("\nmd_mute\n");
	}
	return error;
}*/

void pd_fifo_irq_ctl(bool en)
{
	int i = hdmirx_rd_dwc(DWC_PDEC_IEN);
	if (en == 0)
		hdmirx_wr_dwc(DWC_PDEC_IEN_CLR, _BIT(2));
	else
		hdmirx_wr_dwc(DWC_PDEC_IEN_SET, (_BIT(2) | i));
}

/***************************************************
func: irq tasklet
param: flag:
	0x01:	audio irq
	0x02:	packet irq
***************************************************/
void rx_tasklet_handler(unsigned long arg)
{
	struct rx_s *prx = (struct rx_s *)arg;
	uint32_t irq_flag = prx->ctrl.irq_flag;

	prx->ctrl.irq_flag = 0;

	/*rx_pr("irq_flag = 0x%x\n", irq_flag);*/
	if (irq_flag & IRQ_PACKET_FLAG)
		rx_pkt_handler(PKT_BUFF_SET_FIFO);

	if (irq_flag & IRQ_AUD_FLAG)
		hdmirx_audio_fifo_rst();

	/*irq_flag = 0;*/
}

void skip_frame(void)
{
	if (rx.state == FSM_SIG_READY)
		rx.skip = (1000 * 100 / rx.pre.frame_rate / 10) + 1;
	rx_pr("rx.skip = %d", rx.skip);
}


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
	uint32_t irq_flag = 0;
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

	if (is_meson_gxtvbb_cpu() ||
		is_meson_txl_cpu()) {
		intr_aud_cec =
				hdmirx_rd_dwc(DWC_AUD_CEC_ISTS) &
				hdmirx_rd_dwc(DWC_AUD_CEC_IEN);
		if (intr_aud_cec != 0) {
			cecrx_irq_handle();
			hdmirx_wr_dwc(DWC_AUD_CEC_ICLR, intr_aud_cec);
		}
	}

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
			hdmirx_hdcp_version_set(HDCP_VERSION_14);
			if (hdmirx_repeat_support()) {
				queue_delayed_work(repeater_wq, &repeater_dwork,
						msecs_to_jiffies(5));
				rx_start_repeater_auth();
			}
		}
		if (get(intr_hdmi, DCM_CURRENT_MODE_CHG) != 0) {
			if (log_level & 0x400)
				rx_pr("[isr]DCM_CHG\n");
			video_handle_flag = true;
		}
		if (get(intr_hdmi, SCDC_TMDS_CFG_CHG) != 0) {
			if (log_level & 0x400)
				rx_pr("[isr]scdc_CHG\n");
			video_handle_flag = true;
		}
		if (get(intr_hdmi, _BIT(5)) != 0) {
			if (log_level & 0x400)
				rx_pr("[isr]pllchg\n");
			/* skip_frame(); */
			video_handle_flag = true;
		}
		if (get(intr_hdmi, _BIT(6)) != 0) {
			if (log_level & 0x400)
				rx_pr("[isr]clkchange\n");
			video_handle_flag = true;
		}
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
		if (get(intr_hdcp22, _BIT(1)) != 0)
			hdcp22_capable_sts = HDCP22_AUTH_STATE_NOT_CAPBLE;
		if (get(intr_hdcp22, _BIT(0)) != 0)
			hdcp22_capable_sts = HDCP22_AUTH_STATE_CAPBLE;
		if (get(intr_hdcp22, _BIT(2)) != 0)
			hdcp22_auth_sts = HDCP22_AUTH_STATE_LOST;
		if (get(intr_hdcp22, _BIT(3)) != 0) {
			hdcp22_auth_sts = HDCP22_AUTH_STATE_SUCCESS;
			if ((intr_hdcp22 & 0x01) == 0)
				hdmirx_hdcp_version_set(HDCP_VERSION_22);
		}
		/*if (get(intr_hdcp22, _BIT(4)) != 0) {
			hdcp22_auth_sts = HDCP22_AUTH_STATE_FAILED;
			if (hdcp22_capable_sts == HDCP22_AUTH_STATE_CAPBLE)
				esm_recovery();
		}*/
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
		if (get(intr_pedc, VSI_RCV) != 0) {
			if (log_level & 0x400)
				rx_pr("[irq] VSI_RCV\n");
			vsi_handle_flag = true;
		}
		if (get(intr_pedc, VSI_CKS_CHG) != 0) {
			if (log_level & 0x400)
				rx_pr("[irq] VSI_CKS_CHG\n");
			/* vsi_handle_flag = true; */
		}
		if (get(intr_pedc, PD_FIFO_START_PASS) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] FIFO START\n");
			/* pd_fifo_handle_flag = true; */
			irq_flag |= IRQ_PACKET_FLAG;
			/*pd_fifo_irq_ctl(0);*/
		}
		if (get(intr_pedc, _BIT(1)) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] FIFO MAX\n");
		}
		if (get(intr_pedc, _BIT(0)) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] FIFO MIN\n");
		}
		if (!is_meson_txlx_cpu()) {
			if (get(intr_pedc,
				DRM_RCV_EN | DRM_CKS_CHG) != 0) {
				if (log_level & 0x400)
					rx_pr("[irq] DRM_RCV_EN %#x\n",
					intr_pedc);
				drm_handle_flag = true;
			}
		} else {
			if (get(intr_pedc,
				DRM_RCV_EN_TXLX | DRM_CKS_CHG_TXLX) != 0) {
				if (log_level & 0x400)
					rx_pr("[irq] DRM_RCV_EN %#x\n",
					intr_pedc);
				drm_handle_flag = true;
			}
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
			if (log_level & 0x200)
				rx_pr("[irq] PD_FIFO_OVERFL\n");
			/*  |= hdmirx_packet_fifo_rst(); */
			irq_flag |= IRQ_AUD_FLAG;
		}
		if (get(intr_pedc, PD_FIFO_UNDERFL) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] PD_FIFO_UNDFLOW\n");
			/* error |= hdmirx_packet_fifo_rst(); */
			irq_flag |= IRQ_AUD_FLAG;
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
			if (rx.aud_info.real_sample_rate != 0)
				error |= hdmirx_audio_fifo_rst();
		}
		if (get(intr_aud_fifo, UNDERFL) != 0) {
			if (log_level & 0x100)
				rx_pr("[irq] UNDERFL\n");
			if (rx.aud_info.real_sample_rate != 0)
				error |= hdmirx_audio_fifo_rst();
		}
		ctx->debug_irq_audio_fifo++;
	}

	if (clk_handle_flag)
		clock_handler(ctx);

	/*if (video_handle_flag)
		md_handler(ctx);*/

	if (vsi_handle_flag)
		rx_pkt_handler(PKT_BUFF_SET_VSI);

	if (drm_handle_flag)
		rx_pkt_handler(PKT_BUFF_SET_DRM);

	if (irq_flag) {
		ctx->irq_flag = irq_flag;
		tasklet_schedule(&rx_tasklet);
		/*pd_fifo_irq_ctl(1);*/
	}
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
	if (hdmirx_top_intr_stat & (1 << 13))
		rx_pr("[isr] auth rise\n");
	if (hdmirx_top_intr_stat & (1 << 14))
		rx_pr("[isr] auth fall\n");
	if (hdmirx_top_intr_stat & (1 << 15))
		rx_pr("[isr] enc rise\n");
	if (hdmirx_top_intr_stat & (1 << 16))
		rx_pr("[isr] enc fall\n");

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

	/* check the ip interrupt again */
	hdmirx_top_intr_stat = hdmirx_rd_top(TOP_INTR_STAT);
	if (hdmirx_top_intr_stat & (1 << 31)) {
		if (log_level & 0x100)
			rx_pr("[isr] need clear ip irq---\n");
		goto reisr;

	}
	return IRQ_HANDLED;
}

static uint32_t get_real_sample_rate(void)
{
	int i;
	/* note: if arc is missmatch with LUT, then return 0 */
	uint32_t ret_sr = 0; /* rx.aud_info.arc; */
	for (i = 0; sr_tbl[i] != 0; i++) {
		if (abs(rx.aud_info.arc - sr_tbl[i]) < AUD_SR_RANGE) {
			ret_sr = sr_tbl[i];
			break;
		} else
			ret_sr = 0;
	}
	return ret_sr;
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
		if (en_4k_timing)
			fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
		else
			fmt = TVIN_SIG_FMT_NULL;
		break;
	case HDMI_4096_2160p:
	case HDMI_4096p_50hz_420:
	case HDMI_4096p_60hz_420:
		if (en_4k_timing)
			fmt = TVIN_SIG_FMT_HDMI_4096_2160_00HZ;
		else
			fmt = TVIN_SIG_FMT_NULL;

		break;
	case HDMI_2560_1440:
		fmt = TVIN_SIG_FMT_HDMI_1920X1200_00HZ;
		break;

	default:
		break;
	}

	return fmt;
}

void rx_set_eq_run_state(enum run_eq_state state)
{
	run_eq_flag = state;
	rx_pr("run_eq_flag: %d\n", run_eq_flag);
}


bool rx_sig_is_ready(void)
{
	if (rx.state == FSM_SIG_READY)
		return true;
	else
		return false;
}

bool rx_is_nosig(void)
{
	return rx.no_signal;
}

/*
 * check timing info
 */
static bool is_timing_stable(void)
{
	bool ret = true;

	if (force_vic)
		return true;

	if ((abs(rx.cur.hactive - rx.pre.hactive) > diff_pixel_th) &&
		(stable_check_lvl & HACTIVE_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("hactive(%d=>%d),",
				rx.pre.hactive,
				rx.cur.hactive);
	}
	if ((abs(rx.cur.vactive - rx.pre.vactive) > diff_line_th) &&
		(stable_check_lvl & VACTIVE_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("vactive(%d=>%d),",
				rx.pre.hactive,
				rx.cur.hactive);
	}
	if ((abs(rx.cur.htotal - rx.pre.htotal) > diff_pixel_th) &&
		(stable_check_lvl & HTOTAL_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("htotal(%d=>%d),",
				rx.pre.htotal,
				rx.cur.htotal);
	}
	if ((abs(rx.cur.vtotal - rx.pre.vtotal) > diff_line_th) &&
		(stable_check_lvl & VTOTAL_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("vtotal(%d=>%d),",
				rx.pre.vtotal,
				rx.cur.vtotal);
	}
	if ((rx.pre.colorspace != rx.cur.colorspace) &&
		(stable_check_lvl & COLSPACE_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("colorspace(%d=>%d),",
				rx.pre.colorspace,
				rx.cur.colorspace);
	}
	if ((abs(rx.pre.frame_rate - rx.cur.frame_rate) > diff_frame_th) &&
		(stable_check_lvl & REFRESH_RATE_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("frame_rate(%d=>%d),",
				rx.pre.frame_rate,
				rx.cur.frame_rate);
	}
	if ((rx.pre.repeat != rx.cur.repeat) &&
		(stable_check_lvl & REPEAT_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("repeat(%d=>%d),",
				rx.pre.repeat,
				rx.cur.repeat);
	}
	if ((rx.pre.hw_dvi != rx.cur.hw_dvi) &&
		(stable_check_lvl & DVI_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("dvi(%d=>%d),",
				rx.pre.hw_dvi,
				rx.cur.hw_dvi);
	}
	if ((rx.pre.interlaced != rx.cur.interlaced) &&
		(stable_check_lvl & INTERLACED_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("interlaced(%d=>%d),",
				rx.pre.interlaced,
				rx.cur.interlaced);
	}

	if ((rx.pre.colordepth != rx.cur.colordepth) &&
		(stable_check_lvl & COLOR_DEP_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("colordepth(%d=>%d),",
				rx.pre.colordepth,
				rx.cur.colordepth);
	}
	if ((ret == false) & (log_level & VIDEO_LOG))
		rx_pr("\n");
	return ret;
}

static bool is_hdcp_enc_stable(void)
{
	bool ret = true;
	if ((rx.pre.hdcp14_state != rx.cur.hdcp14_state) &&
		(rx.pre.hdcp_type == E_HDCP14) &&
		(stable_check_lvl & HDCP_ENC_EN)) {
		ret = false;
		if (log_level & VIDEO_LOG)
			rx_pr("hdcp_enc_state(%d=>%d)\n",
				rx.pre.hdcp14_state,
				rx.cur.hdcp14_state);
	}
	return ret;
}

static int get_timing_fmt(void)
{
	int i;
	int size = sizeof(freq_ref)/sizeof(struct freq_ref_s);
	rx.pre.sw_vic = 0;
	rx.pre.sw_dvi = 0;
	rx.pre.sw_fp = 0;
	rx.pre.sw_alternative = 0;
	if (force_vic) {
		for (i = 0; i < size; i++) {
			if (force_vic == freq_ref[i].vic)
				break;
		}
		rx.pre.sw_vic = freq_ref[i].vic;
		return i;
	}
	for (i = 0; i < size; i++) {
		if (abs(rx.pre.hactive - freq_ref[i].hactive) > diff_pixel_th)
			continue;
		if ((abs(rx.pre.vactive - freq_ref[i].vactive)) > diff_line_th)
			continue;
		if ((abs(rx.pre.frame_rate - freq_ref[i].frame_rate)
				> diff_frame_th) &&
			(freq_ref[i].frame_rate != 0))
			continue;
		if ((rx.pre.colorspace != freq_ref[i].cd420) &&
			(freq_ref[i].cd420 != 0))
			continue;
		if (freq_ref[i].interlace != rx.pre.interlaced)
			continue;
		break;
	}
	if (i == size)
		return i;

	rx.pre.sw_vic = freq_ref[i].vic;
	rx.pre.sw_dvi = rx.pre.hw_dvi;


	if (freq_ref[i].type_3d == 1)
		rx.pre.sw_fp = 1;
	else if (freq_ref[i].type_3d == 2)
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
			rx.pre.frame_rate);
	}
	return i;
}

static void Signal_status_init(void)
{
	hpd_wait_cnt = 0;
	pll_unlock_cnt = 0;
	pll_lock_cnt = 0;
	sig_unstable_cnt = 0;
	sig_stable_cnt = 0;
	sig_stable_cnt = 0;
	sig_unstable_cnt = 0;
	sig_unready_cnt = 0;
	sig_unstable_reset_hpd_cnt = 0;
	/*rx.wait_no_sig_cnt = 0;*/
	/* rx.no_signal = false; */
	rx.pre_state = 0;
	/* audio */
	audio_sample_rate = 0;
	audio_coding_type = 0;
	audio_channel_count = 0;
	auds_rcv_sts = 0;
	rx.aud_sr_stable_cnt = 0;
	rx_aud_pll_ctl(0);
	/* */
	pre_port = E_5V_LOST;
	rx_set_eq_run_state(E_EQ_START);
	is_hdcp_source = true;
	#ifdef HDCP22_ENABLE
	/*if (hdcp22_on)
		esm_set_stable(0);*/
	hdmirx_hdcp_version_set(HDCP_VERSION_NONE);
	#endif
	rx.skip = 0;
}

void packet_update(void)
{
	/*rx_getaudinfo(&rx.aud_info);*/

	rgb_quant_range = rx.cur.rgb_quant_range;
	yuv_quant_range = rx.cur.yuv_quant_range;
	it_content = rx.cur.it_content;

	auds_rcv_sts = rx.aud_info.aud_packet_received;
	audio_sample_rate = rx.aud_info.real_sample_rate;
	audio_coding_type = rx.aud_info.coding_type;
	audio_channel_count = rx.aud_info.channel_count;

	hdmirx_dv_packet_stop();
}

/* ---------------------------------------------------------- */
/* func:         port A,B,C,D  hdmitx-5v monitor & HPD control */
/* note:         G9TV portD no used */
/* ---------------------------------------------------------- */
void rx_5v_det(void)
{
	static int check_cnt;
	uint32_t tmp_5v = (hdmirx_rd_top(TOP_HPD_PWR5V) >> 20) & 0xf;

	if (auto_switch_off)
		tmp_5v = 0x0f;

	if (tmp_5v != pwr_sts)
		check_cnt++;

	if (check_cnt > pow5v_max_cnt) {
		check_cnt = 0;
		pwr_sts = tmp_5v;
		rx_pr("hotplg-%x\n", pwr_sts);
		rx.cur_5v_sts = (pwr_sts >> rx.port) & 1;
		if (0 == rx.cur_5v_sts)
			fsm_restart();
		hotplug_wait_query();
	} else
		rx.cur_5v_sts = (pwr_sts >> rx.port) & 1;
}

void hdmirx_error_count_config(void)
{
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
	if (hdcp22_reauth_enable) {
		esm_auth_fail_en = 1;
		hdcp22_auth_sts = 0xff;
	}
}

void monitor_hdcp22_sts(void)
{
	/*if the auth lost after the success of authentication*/
	if ((HDCP22_AUTH_STATE_CAPBLE == hdcp22_capable_sts) &&
		((HDCP22_AUTH_STATE_LOST == hdcp22_auth_sts) ||
		(HDCP22_AUTH_STATE_FAILED == hdcp22_auth_sts))) {
			hdmirx_hdcp22_reauth();
			/*rx_pr("\n auth lost force hpd rst\n");*/
		}
}

void rx_dwc_reset(void)
{
	if (log_level & VIDEO_LOG)
		rx_pr("rx_dwc_reset\n");
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
	return (hdmirx_rd_dwc(DWC_SCDC_REGS0) >> 17) & 1;
}

#ifdef HDCP22_ENABLE
void esm_reboot_func(void)
{
	if (esm_reboot_lvl == 1) {
		rx_set_hpd(0);
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
		rx_set_hpd(0);
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

void rx_force_hdcp14(bool en)
{
	if (en)
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 2);
	else
		hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x1000);
}

void esm_set_stable(bool stable)
{
	if (log_level & HDCP_LOG)
		rx_pr("esm set stable:%d\n", stable);
	video_stable_to_esm = stable;
}

void esm_recovery(void)
{
	if (hdcp22_stop_auth_enable)
		hdcp22_stop_auth = 1;
	if (hdcp22_esm_reset2_enable)
		hdcp22_esm_reset2 = 1;
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

void rx_nosig_monitor(void)
{
	if (rx.cur_5v_sts == 0)
		rx.no_signal = true;
	else if (rx.state != FSM_SIG_READY) {
		if (rx.wait_no_sig_cnt >= wait_no_sig_max)
			rx.no_signal = true;
		else {
			rx.wait_no_sig_cnt++;
			if (rx.no_signal)
				rx.no_signal = false;
		}
	} else {
		rx.wait_no_sig_cnt = 0;
		rx.no_signal = false;
	}
}

void monitor_cable_clk_sts(void)
{
	static int pre_sts = 0xff;
	bool sts = is_clk_stable();
	if (pre_sts != sts) {
		if (log_level & VIDEO_LOG)
			rx_pr("\nclk stable = %d\n", sts);
		pre_sts = sts;
	}
}

void fsm_restart(void)
{
	#ifdef HDCP22_ENABLE
	rx_esm_tmdsclk_en(0);
	#endif
	set_scdc_cfg(1, 0);
	rx.state = FSM_INIT;
	rx_pr("force_fsm_init\n");
}

void rx_esm_exception_monitor(void)
{
	int irq_status, exception;

	irq_status = rx_hdcp22_rd_reg(HPI_REG_IRQ_STATUS);
	/*rx_pr("++++++hdcp22 state irq status:%#x\n", reg);*/
	if (irq_status & IRQ_STATUS_UPDATE_BIT) {
		exception =
		rx_hdcp22_rd_reg_bits(HPI_REG_EXCEPTION_STATUS,
						EXCEPTION_CODE);
		if (exception != rx.hdcp.hdcp22_exception) {
			rx_pr("++++++hdcp22 state:%#x,vec:%#x\n",
				rx_hdcp22_rd_reg(HPI_REG_EXCEPTION_STATUS),
				exception);
			rx.hdcp.hdcp22_exception = exception;
		}
	}
}

void hdmirx_hw_monitor(void)
{
	int pre_sample_rate;
	int tmp;
	/* static int md_sts;
	static int hdmi_sts; */
	enum eq_states_e sts;

	if (clk_debug)
		monitor_cable_clk_sts();

	if (sm_pause)
		return;

	#ifdef HDCP22_ENABLE
	if (log_level & VIDEO_LOG)
		rx_esm_exception_monitor();/* only for debug */
	if ((hdcp22_on) && (rx.state > FSM_SIG_UNSTABLE)) {
		monitor_hdcp22_sts();
		esm_rst_monitor();
	}
	#endif

	switch (rx.state) {
	case FSM_HPD_LOW:
		/* set_scdc_cfg(1, 1); */
		rx_set_hpd(0);
		hdmirx_irq_enable(FALSE);
		rx.state = FSM_INIT;
		set_scdc_cfg(1, 0);
		rx_pr("HPD_LOW\n");
		break;
	case FSM_INIT:
		Signal_status_init();
		rx.state = FSM_HPD_HIGH;
		rx.pre_state = FSM_INIT;
		rx_pr("FSM_INIT->HPD_HIGH\n");
		break;
	case FSM_HPD_HIGH:
		if (rx.cur_5v_sts == 0) {
			rx.no_signal = true;
			break;
		}
		hpd_wait_cnt++;
		if ((0 == get_cur_hpd_sts()) &&
			(hpd_wait_cnt <= hpd_wait_max))
			break;
		if (do_hpd_reset_flag) {
			if (hpd_wait_cnt <= hpd_wait_max*10)
				break;
			do_hpd_reset_flag = 0;
			/*rx.boot_flag = FALSE;*/
		}
		hpd_wait_cnt = 0;
		pre_port = rx.port;
		rx_set_hpd(1);
		set_scdc_cfg(0, 1);
		rx.state = FSM_WAIT_CLK_STABLE;
		rx_pr("HPD_HIGH->CLK_STABLE\n");
		break;
	case FSM_WAIT_CLK_STABLE:
		if (hdmirx_phy_clk_rate_monitor()) {
			rx.state = FSM_INIT;
			rx_pr("clk rate changed\n");
			break;
		}
		if (is_clk_stable()) {
			clk_unstable_cnt = 0;
			if (clk_stable_cnt++ > clk_stable_max) {
				rx.state = FSM_PHY_INIT;
				rx_pr("WAIT_CLK_STABLE->EQ_INIT\n");
				clk_stable_cnt = 0;
			}
		} else {
			clk_stable_cnt = 0;
			if (clk_unstable_cnt++ >= clk_unstable_max) {
				rx.state = FSM_HPD_LOW;
				pre_port = E_HPD_RESET;
				rx_pr("WAIT_CLK_STABLE->HPD_LOW\n");
				clk_unstable_cnt = 0;
			}
		}
		break;
	case FSM_PHY_INIT:
		if (hdmirx_phy_clk_rate_monitor()) {
			rx.state = FSM_INIT;
			rx_pr("clk rate changed\n");
			break;
		}
		sts = rx_need_eq_algorithm();
		switch (sts) {
		case EQ_USE_PRE:
		case EQ_USE_DEF:
			rx.state = FSM_SIG_UNSTABLE;
			rx_pr("EQ_INIT-->FSM_SIG_UNSTABLE\n");
			break;
		case EQ_ENABLE:
		default:
			/*if (hdcp22_on)
				esm_recovery();*/
			run_eq_flag = E_EQ_START;
			rx.state = FSM_WAIT_EQ_DONE;
			rx_pr("EQ_INIT-->FSM_WAIT_EQ_DONE\n");
			break;
		}
	case FSM_WAIT_EQ_DONE:
		if (run_eq_flag == E_EQ_PASS)
			rx.state = FSM_SIG_UNSTABLE;
		break;
	case FSM_SIG_UNSTABLE:
		if (hdmirx_phy_clk_rate_monitor()) {
			rx.state = FSM_INIT;
			rx_pr("clk rate changed\n");
			break;
		}
		if (log_level & VIDEO_LOG)
			rx_pr("lock_cnt:%d,unlock_cnt:%d\n",
					pll_lock_cnt, pll_unlock_cnt);
		if (hdmirx_tmds_pll_lock()) {
			pll_unlock_cnt = 0;
			if (pll_lock_cnt++ > pll_lock_max) {
				rx.state = FSM_SIG_WAIT_STABLE;
				rx_dwc_reset();
				hdmirx_irq_enable(TRUE);
				#ifdef HDCP22_ENABLE
				if (hdcp22_on) {
					#ifdef HDCP22_ENABLE
					rx_esm_tmdsclk_en(1);
					#endif
					esm_set_stable(1);
					if (rx.hdcp.hdcp_version !=
						HDCP_VERSION_NONE)
						hdmirx_hdcp22_reauth();
				}
				#endif
				pll_lock_cnt = 0;
				rx_pr("UNSTABLE->WAIT_STABLE\n");
			}
		} else {
			pll_lock_cnt = 0;
			if (pll_unlock_cnt++ >= pll_unlock_max) {
				hdmirx_error_count_config();
				rx.state = FSM_WAIT_CLK_STABLE;
				pll_unlock_cnt = 0;
				rx_set_eq_run_state(E_EQ_FAIL);
				rx_pr("UNSTABLE->HPD_LOW\n");
			}
		}
		break;
	case FSM_SIG_WAIT_STABLE:
		if (hdmirx_phy_clk_rate_monitor()) {
			rx.state = FSM_INIT;
			rx_pr("clk rate changed\n");
			break;
		}
		dwc_rst_wait_cnt++;
		if (dwc_rst_wait_cnt < dwc_rst_wait_cnt_max)
			break;
		dwc_rst_wait_cnt = 0;
		rx.state = FSM_SIG_STABLE;
		rx_pr("DWC_RST->FSM_SIG_STABLE\n");
		break;
	case FSM_SIG_STABLE:
		if (hdmirx_phy_clk_rate_monitor()) {
			rx.state = FSM_INIT;
			rx_pr("clk rate changed\n");
			break;
		}
		memcpy(&rx.pre, &rx.cur,
			sizeof(struct rx_video_info));
		hdmirx_get_video_info();
		if (is_timing_stable() && is_hdcp_enc_stable()) {
			if (sig_stable_cnt++ > sig_stable_max) {
				get_timing_fmt();
				if ((rx.pre.sw_vic == HDMI_UNSUPPORT) ||
					(rx.pre.sw_vic == HDMI_UNKNOW)) {
					if (log_level & VIDEO_LOG)
						rx_pr("*unsupport*\n");
					if (sig_stable_cnt < (sig_stable_max*5))
						break;
					sig_stable_cnt = 0;
					if (log_level & VIDEO_LOG)
						rx_pr("unsupport->wait_clk\n");
					if (log_level & VIDEO_LOG)
						dump_state(1);
					rx_set_eq_run_state(E_EQ_FAIL);
					rx.state = FSM_WAIT_CLK_STABLE;
					break;
				}
				if (rx.pre.sw_dvi == 1) {
					if (sig_stable_cnt < (sig_stable_max*7))
						break;
				}
				/*For 1.4 source after esm reset,hdcp type will
				be 2.2 untill tx send aksv,for some sources
				which send aksv too late,this state will keep
				2.2,so this state is correct */
				if ((is_hdcp_source) &&
					/* (rx.pre.hdcp_type == E_HDCP14) && */
					(rx.pre.hdcp14_state != 3) &&
					(rx.pre.hdcp14_state != 0)) {
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
				rx.skip = 0;
				if ((rx.pre.hdcp14_state == 0) ||
					(rx.pre.hdcp14_state == 2))
					is_hdcp_source = false;
				rx.state = FSM_SIG_READY;
				rx.aud_sr_stable_cnt = 0;
				rx.no_signal = false;
				memset(&rx.aud_info, 0,
					sizeof(struct aud_info_s));
				hdmirx_config_video();
				hdmirx_audio_fifo_rst();
				rx_pr("STABLE->READY\n");
				if (log_level & VIDEO_LOG)
					dump_state(0x1);
			}
		} else {
			sig_stable_cnt = 0;
			if (sig_unstable_cnt++ > sig_unstable_max) {
				rx.state = FSM_WAIT_CLK_STABLE;
				rx_set_eq_run_state(E_EQ_FAIL);
				sig_stable_cnt = 0;
				sig_unstable_cnt = 0;
				hdmirx_error_count_config();
				if (enable_hpd_reset) {
					sig_unstable_reset_hpd_cnt++;
					if (sig_unstable_reset_hpd_cnt >=
						sig_unstable_reset_hpd_max) {
						rx.state = FSM_HPD_HIGH;
						rx_set_hpd(0);
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
	case FSM_SIG_READY:
		if (hdmirx_phy_clk_rate_monitor()) {
			rx.state = FSM_INIT;
			rx_pr("clk rate changed\n");
			break;
		}
	    hdmirx_get_video_info();
	    /* video info change */
	    if ((hdmirx_tmds_pll_lock() == false) ||
			(is_timing_stable() == false)) {
			skip_frame();
			if (++sig_unready_cnt >= sig_unready_max) {
				/*sig_lost_lock_cnt = 0;*/
				sig_unready_cnt = 0;
				audio_sample_rate = 0;
				rx_aud_pll_ctl(0);
				#ifdef HDCP22_ENABLE
				rx_esm_tmdsclk_en(0);
				#endif
				rx.state = FSM_WAIT_CLK_STABLE;
				rx.pre_state = FSM_SIG_READY;
				rx.skip = 0;
				rx.aud_sr_stable_cnt = 0;
				#ifdef HDCP22_ENABLE
				if (hdcp22_on)
					esm_recovery();
				#endif
				memset(&rx.pre, 0,
					sizeof(struct rx_video_info));
				memset(&rx.vsi_info,
					0,
					sizeof(struct vsi_info_s));
				rx_pr("READY->wait_clk\n");
				break;
			}
	    } else {
			if (sig_unready_cnt != 0) {
				if (log_level & VIDEO_LOG)
					rx_pr("sig_unready_cnt=%d\n",
						sig_unready_cnt);
				sig_unready_cnt = 0;
			}
		}

		/* if (rx.no_signal == true)
			rx.no_signal = false; */

		if (rx.skip > 0) {
			rx.skip--;
			if (log_level & VIDEO_LOG)
				rx_pr("rc--%d\n", rx.skip);
		}

		if (rx.pre.sw_dvi == 1)
			break;

		pre_sample_rate = rx.aud_info.real_sample_rate;
		rx_getaudinfo(&rx.aud_info);
		if (force_audio_sample_rate == 0)
			rx.aud_info.real_sample_rate =
				get_real_sample_rate();
		else
			rx.aud_info.real_sample_rate =
				force_audio_sample_rate;

		if (!is_sample_rate_stable
			(pre_sample_rate, rx.aud_info.real_sample_rate)) {
			if (log_level & AUDIO_LOG)
				dump_state(2);
			rx.aud_sr_stable_cnt = 0;
			break;
		}
		if (rx.aud_sr_stable_cnt <
			AUD_SR_STB_MAX) {
			rx.aud_sr_stable_cnt++;
			if (rx.aud_sr_stable_cnt ==
				AUD_SR_STB_MAX) {
				dump_state(0x2);
				rx_aud_pll_ctl(1);
				hdmirx_audio_fifo_rst();
				if (hdmirx_get_audio_pll_clock() < 100000) {
					rx_pr("update audio\n");
					tmp = hdmirx_rd_top(TOP_ACR_CNTL_STAT);
					hdmirx_wr_top(TOP_ACR_CNTL_STAT,
							tmp | (1<<11));
				}
			}
		}
		packet_update();
		break;
	default:
		break;
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
			return EDID_LIST_14;
		else if (edid_mode < EDID_LIST_NUM)
			return edid_mode;
		else
			return EDID_LIST_14;
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

int rx_set_hdr_lumi(unsigned char *data, int len)
{
	if ((data == NULL) || (len == 0) || (len > MAX_HDR_LUMI))
		return false;

	memcpy(receive_hdr_lum, data, len);
	new_hdr_lum = true;
	return true;
}
EXPORT_SYMBOL(rx_set_hdr_lumi);

void rx_send_hpd_pulse(void)
{
	rx_set_hpd(0);
	fsm_restart();
}

#if 0
void rx_load_edid_data(unsigned char *buffer, int port_map,
								int brepeat)
{
	unsigned char value = 0;
	unsigned char check_sum = 0;
	unsigned char phy_addr_offset = 0;
	int i;
	unsigned char phy_addr[E_PORT_NUM] = {0, 0, 0};
	unsigned char checksum[E_PORT_NUM] = {0, 0, 0};

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
			/* physical address at second 128bytes block */
			check_sum += value;
			check_sum &= 0xff;
		}
		if (i == 255) {
			/* caculacte checksum */
			value = (0x100-check_sum)&0xff;
		}
		/* write edid data to edid buffer */
		hdmirx_wr_top(TOP_EDID_OFFSET + i, value);
		hdmirx_wr_top(0x100 + TOP_EDID_OFFSET + i, value);
	}

	for (i = 0; i < E_PORT_NUM; i++) {
		phy_addr[i] = ((port_map >> i*4) & 0xf) << 4;
		checksum[i] = (0x100 - (check_sum +
			(phy_addr[i] - 0x10))) & 0xff;
	}

	/* clear all and disable all ove register */
	for (i = TOP_EDID_RAM_OVR0; i <= TOP_EDID_RAM_OVR7; ) {
		hdmirx_wr_top(i, 0);
		i += 2;
	}

	/*the first edid ram: physical address idx 0 data*/
	hdmirx_wr_top(TOP_EDID_RAM_OVR1,
		phy_addr_offset | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR1_DATA,
		phy_addr[0]|phy_addr[1]<<8|phy_addr[2]<<16 | phy_addr[3]<<24);

	/*the first edid ram: checksum address */
	hdmirx_wr_top(TOP_EDID_RAM_OVR0,
		0xff | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR0_DATA,
		checksum[0]|checksum[1]<<8|checksum[2]<<16|checksum[3]<<24);

	/*the second edid ram: physical address idx 0 data*/
	hdmirx_wr_top(TOP_EDID_RAM_OVR4,
		(phy_addr_offset + 0x100) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR4_DATA,
		phy_addr[0]|phy_addr[1]<<8|phy_addr[2]<<16 | phy_addr[3]<<24);

	/*the second edid ram: checksum address */
	hdmirx_wr_top(TOP_EDID_RAM_OVR3,
		0x1ff | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR3_DATA,
		checksum[0]|checksum[1]<<8|checksum[2]<<16|checksum[3]<<24);

	for (i = 0; i < E_PORT_NUM; i++) {
		rx_pr("port %d,addr 0x%x,checksum 0x%x\n",
				i, phy_addr[i], checksum[i]);
	}
}

#endif

#if 0
void rx_load_edid_data(unsigned char *buffer,
								int port_map,
								int brepeat)
{
	unsigned char value = 0;
	unsigned char check_sum = 0;
	unsigned char phy_addr_offset = 0;
	int i;
	unsigned int phy_addr[E_PORT_NUM] = {0, 0, 0};
	unsigned char checksum[E_PORT_NUM] = {0, 0, 0};
	unsigned char root_offset = 0;

	for (i = 0; i <= 255; i++) {
		value = buffer[i];

		/*find phy_addr_offset*/
		if (value == 0x03) {
			if ((0x0c == buffer[i+1]) &&
				(0x00 == buffer[i+2]) &&
				(0x00 == buffer[i+4])) {
				if (brepeat)
					buffer[i+3] = 0x00;
				else
					buffer[i+3] = 0x10;
				phy_addr_offset = i+3;
			}
		}
		if ((i >= 128) && (i < 255)) {
			check_sum += value;
			check_sum &= 0xff;
		}
		if (i == 255) {
			value = (0x100-check_sum)&0xff;
			/*check_sum = 0;*/
		}

		/* the first edid buffer */
		hdmirx_wr_top(TOP_EDID_OFFSET + i, value);
		/* the second edid buffer */
		hdmirx_wr_top(0x100+TOP_EDID_OFFSET + i, value);
	}

	if (brepeat) {
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

		rx_pr("port map:%#x,rootoffset=%d,upaddr=%#x\n",
					port_map,
					root_offset,
					up_phy_addr);
		for (i = 0; i < E_PORT_NUM; i++) {
			if (root_offset == 0)
				phy_addr[i] = 0xFFFF;
			else
				phy_addr[i] = (up_phy_addr |
				((((port_map >> i*4) & 0xf) << 12) >>
				(root_offset - 1)*4));


			phy_addr[i] = rx_exchange_bits(phy_addr[i]);
			checksum[i] = (0x100 + value - (phy_addr[i] & 0xFF) -
				((phy_addr[i] >> 8) & 0xFF)) & 0xff;
			rx_pr("port %d phy:%d\n", i, phy_addr[i]);
		}
	} else {
		for (i = 0; i < E_PORT_NUM; i++) {
			phy_addr[i] = ((port_map >> i*4) & 0xf) << 4;
			checksum[i] = (0x100 - (check_sum +
				(phy_addr[i] - 0x10))) & 0xff;
		}
	}

	/* clear all and disable all ove register 0~7*/
	for (i = TOP_EDID_RAM_OVR0; i <= TOP_EDID_RAM_OVR7; ) {
		hdmirx_wr_top(i, 0);
		i += 2;
	}

	/* replace the first edid ram data */
	/* physical address byte 1 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR2,
		(phy_addr_offset + 1) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR2_DATA,
		((phy_addr[E_PORT0] >> 8) & 0xFF) |
		 (((phy_addr[E_PORT1] >> 8) & 0xFF)<<8)
			| (((phy_addr[E_PORT2] >> 8) & 0xFF)<<16)
			| (((phy_addr[E_PORT3] >> 8) & 0xFF)<<24));
	/* physical address byte 0 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR1,
		phy_addr_offset | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR1_DATA,
		(phy_addr[E_PORT0] & 0xFF) | ((phy_addr[E_PORT1] & 0xFF)<<8) |
			((phy_addr[E_PORT2] & 0xFF)<<16) |
			((phy_addr[E_PORT3] & 0xFF) << 24));

	/* checksum */
	hdmirx_wr_top(TOP_EDID_RAM_OVR0,
		0xff | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR0_DATA,
			checksum[E_PORT0]|(checksum[E_PORT1]<<8)|
			(checksum[E_PORT2]<<16) | (checksum[E_PORT3] << 24));


	/* replace the second edid ram data */
	/* physical address byte 1 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR5,
		(phy_addr_offset + 0x101) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR5_DATA,
		((phy_addr[E_PORT0] >> 8) & 0xFF) |
		 (((phy_addr[E_PORT1] >> 8) & 0xFF)<<8)
			| (((phy_addr[E_PORT2] >> 8) & 0xFF)<<16)
			| (((phy_addr[E_PORT3] >> 8) & 0xFF)<<24));
	/* physical address byte 0 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR4,
		(phy_addr_offset + 0x100) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR4_DATA,
		(phy_addr[E_PORT0] & 0xFF) | ((phy_addr[E_PORT1] & 0xFF)<<8) |
			((phy_addr[E_PORT2] & 0xFF)<<16) |
			((phy_addr[E_PORT3] & 0xFF) << 24));
	/* checksum */
	hdmirx_wr_top(TOP_EDID_RAM_OVR3,
		(0xff + 0x100) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR3_DATA,
			checksum[E_PORT0]|(checksum[E_PORT1]<<8)|
			(checksum[E_PORT2]<<16) | (checksum[E_PORT3] << 24));

	for (i = 0; i < E_PORT_NUM; i++) {
		rx_pr(">port %d,addr 0x%x,checksum 0x%x\n",
					i, phy_addr[i], checksum[i]);
	}
}
#endif

unsigned char *rx_get_edid(int edid_index)
{
	/*int edid_index = rx_get_edid_index();*/

	memcpy(edid_temp, rx_get_edid_buffer(edid_index),
				rx_get_edid_size(edid_index));
	rx_pr("index=%d,get size=%d,edid_size=%d,rept=%d\n",
				edid_index,
				rx_get_edid_size(edid_index),
				edid_size, hdmirx_repeat_support());


	rx_pr("edid update port map:%#x,up_phy_addr=%#x\n",
				port_map, up_phy_addr);
	return edid_temp;
}

void rx_edid_update_hdr_info(
							unsigned char *p_edid,
							u_int idx)
{
	u_char hdr_edid[EDID_HDR_SIZE];

	if (p_edid == NULL)
		return;

	/* update hdr info */
	hdr_edid[0] = ((EDID_TAG_HDR >> 3)&0xE0) + (6 & 0x1f);
	hdr_edid[1] = EDID_TAG_HDR & 0xFF;
	memcpy(hdr_edid + 4, receive_hdr_lum,
				sizeof(unsigned char)*3);
	rx_modify_edid(p_edid, rx_get_edid_size(idx),
						hdr_edid);
}

unsigned int rx_edid_cal_phy_addr(
					u_int brepeat,
					u_int up_addr,
					u_int portmap,
					u_char *pedid,
					u_int *phy_offset,
					u_int *phy_addr)
{
	u_int root_offset = 0;
	u_int i;
	u_int flag = 0;

	if (!(pedid && phy_offset && phy_addr))
		return -1;

	for (i = 0; i <= 255; i++) {
		/*find phy_addr_offset*/
		if (pedid[i] == 0x03) {
			if ((0x0c == pedid[i+1]) &&
				(0x00 == pedid[i+2]) &&
				(0x00 == pedid[i+4])) {
				if (brepeat)
					pedid[i+3] = 0x00;
				else
					pedid[i+3] = 0x10;
				*phy_offset = i+3;
				flag = 1;
				break;
			}
		}
	}

	if (brepeat) {
		/*get the root index*/
		i = 0;
		while (i < 4) {
			if (((up_addr << (i*4)) & 0xf000) != 0) {
				root_offset = i;
				break;
			} else
				i++;
		}
		if (i == 4)
			root_offset = 4;

		rx_pr("portmap:%#x,rootoffset=%d,upaddr=%#x\n",
					portmap,
					root_offset,
					up_addr);

		for (i = 0; i < E_PORT_NUM; i++) {
			if (root_offset == 0)
				phy_addr[i] = 0xFFFF;
			else
				phy_addr[i] = (up_addr |
				((((portmap >> i*4) & 0xf) << 12) >>
				(root_offset - 1)*4));

			phy_addr[i] = rx_exchange_bits(phy_addr[i]);
			rx_pr("port %d phy:%d\n", i, phy_addr[i]);
		}
	} else {
		for (i = 0; i < E_PORT_NUM; i++)
			phy_addr[i] = ((portmap >> i*4) & 0xf) << 4;
	}

	return flag;
}

void rx_edid_fill_to_register(
						u_char *pedid,
						u_int brepeat,
						u_int *pphy_addr,
						u_char *pchecksum)
{
	u_int i;
	u_int checksum = 0;
	u_int value = 0;

	if (!(pedid && pphy_addr && pchecksum))
		return;

	/* clear all and disable all overlay register 0~7*/
	for (i = TOP_EDID_RAM_OVR0; i <= TOP_EDID_RAM_OVR7; ) {
		hdmirx_wr_top(i, 0);
		i += 2;
	}

	/* physical address info at second block */
	for (i = 128; i <= 255; i++) {
		value = pedid[i];
		if (i < 255) {
			checksum += pedid[i];
			checksum &= 0xff;
		} else if (i == 255) {
			value = (0x100 - checksum)&0xff;
		}
	}

	/* physical address info at second block */
	for (i = 0; i <= 255; i++) {
		/* fill first edid buffer */
		hdmirx_wr_top(TOP_EDID_OFFSET + i, pedid[i]);
		/* fill second edid buffer */
		hdmirx_wr_top(0x100+TOP_EDID_OFFSET + i, pedid[i]);
	}
	/* caculate 4 port check sum */
	if (brepeat) {
		for (i = 0; i < E_PORT_NUM; i++) {
			pchecksum[i] = (0x100 + value - (pphy_addr[i] & 0xFF) -
			((pphy_addr[i] >> 8) & 0xFF)) & 0xff;
			/*rx_pr("port %d phy:%d\n", i, pphy_addr[i]);*/
		}
	} else {
		for (i = 0; i < E_PORT_NUM; i++) {
			pchecksum[i] = (0x100 - (checksum +
				(pphy_addr[i] - 0x10))) & 0xff;
			/*rx_pr("port %d phy:%d\n", i, pphy_addr[i]);*/
		}
	}
}

void rx_edid_update_overlay(
						u_int phy_addr_offset,
						u_int *pphy_addr,
						u_char *pchecksum)
{
	u_int i;

	if (!(pphy_addr && pchecksum))
		return;

	/* replace the first edid ram data */
	/* physical address byte 1 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR2,
		(phy_addr_offset + 1) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR2_DATA,
		((pphy_addr[E_PORT0] >> 8) & 0xFF) |
		 (((pphy_addr[E_PORT1] >> 8) & 0xFF)<<8)
			| (((pphy_addr[E_PORT2] >> 8) & 0xFF)<<16)
			| (((pphy_addr[E_PORT3] >> 8) & 0xFF)<<24));
	/* physical address byte 0 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR1,
		phy_addr_offset | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR1_DATA,
		(pphy_addr[E_PORT0] & 0xFF) | ((pphy_addr[E_PORT1] & 0xFF)<<8) |
			((pphy_addr[E_PORT2] & 0xFF)<<16) |
			((pphy_addr[E_PORT3] & 0xFF) << 24));

	/* checksum */
	hdmirx_wr_top(TOP_EDID_RAM_OVR0,
		0xff | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR0_DATA,
			pchecksum[E_PORT0]|(pchecksum[E_PORT1]<<8)|
			(pchecksum[E_PORT2]<<16) | (pchecksum[E_PORT3] << 24));


	/* replace the second edid ram data */
	/* physical address byte 1 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR5,
		(phy_addr_offset + 0x101) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR5_DATA,
		((pphy_addr[E_PORT0] >> 8) & 0xFF) |
		 (((pphy_addr[E_PORT1] >> 8) & 0xFF)<<8)
			| (((pphy_addr[E_PORT2] >> 8) & 0xFF)<<16)
			| (((pphy_addr[E_PORT3] >> 8) & 0xFF)<<24));
	/* physical address byte 0 */
	hdmirx_wr_top(TOP_EDID_RAM_OVR4,
		(phy_addr_offset + 0x100) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR4_DATA,
		(pphy_addr[E_PORT0] & 0xFF) | ((pphy_addr[E_PORT1] & 0xFF)<<8) |
			((pphy_addr[E_PORT2] & 0xFF)<<16) |
			((pphy_addr[E_PORT3] & 0xFF) << 24));
	/* checksum */
	hdmirx_wr_top(TOP_EDID_RAM_OVR3,
		(0xff + 0x100) | (0x0f<<16));
	hdmirx_wr_top(TOP_EDID_RAM_OVR3_DATA,
			pchecksum[E_PORT0]|(pchecksum[E_PORT1]<<8)|
			(pchecksum[E_PORT2]<<16) | (pchecksum[E_PORT3] << 24));

	for (i = 0; i < E_PORT_NUM; i++) {
		rx_pr(">port %d,addr 0x%x,checksum 0x%x\n",
					i, pphy_addr[i], pchecksum[i]);
	}
}

int hdmi_rx_ctrl_edid_update(void)
{
	int edid_index = rx_get_edid_index();
	bool brepeat = hdmirx_repeat_support();
	u_char *pedid_data;
	u_int sts;
	u_int phy_addr_offset;
	u_int phy_addr[E_PORT_NUM] = {0, 0, 0};
	u_char checksum[E_PORT_NUM] = {0, 0, 0};

	/* get edid from buffer, return buffer addr */
	pedid_data = rx_get_edid(edid_index);

	/* update hdr info to edid buffer */
	rx_edid_update_hdr_info(pedid_data, edid_index);

	if (brepeat) {
		/* repeater mode */
		rx_edid_update_audio_info(pedid_data,
				rx_get_edid_size(edid_index));
	}

	/* caculate physical address and checksum */
	sts = rx_edid_cal_phy_addr(brepeat,
					up_phy_addr, port_map,
					pedid_data, &phy_addr_offset,
					phy_addr);
	if (!sts) {
		/* not find physical address info */
		rx_pr("err: not finded phy addr info\n");
	}

	/* write edid to edid register */
	rx_edid_fill_to_register(pedid_data, brepeat,
							phy_addr, checksum);
	if (sts) {
		/* update physical and checksum */
		rx_edid_update_overlay(phy_addr_offset, phy_addr, checksum);
	}
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
	static struct aud_info_s a;

	hdmirx_get_video_info();
	if (enable & 1) {
		rx_pr("[HDMI info]");
		rx_pr("colorspace %d,", rx.cur.colorspace);
		rx_pr("hw_vic %d,", rx.cur.hw_vic);
		rx_pr("dvi %d,", rx.cur.hw_dvi);
		rx_pr("interlace %d\n", rx.cur.interlaced);
		rx_pr("htotal %d", rx.cur.htotal);
		rx_pr("hactive %d", rx.cur.hactive);
		rx_pr("vtotal %d", rx.cur.vtotal);
		rx_pr("vactive %d", rx.cur.vactive);
		rx_pr("repetition %d\n", rx.cur.repeat);
		rx_pr("colordepth %d", rx.cur.colordepth);
		rx_pr("frame_rate %d\n", rx.cur.frame_rate);
		rx_pr("TMDS clock = %d,",
			hdmirx_get_tmds_clock());
		rx_pr("Pixel clock = %d\n",
			hdmirx_get_pixel_clock());
		rx_pr("Audio PLL clock = %d",
			hdmirx_get_audio_pll_clock());
		rx_pr("ESM clock = %d",
			hdmirx_get_esm_clock());
		rx_pr("avmute_skip:0x%x\n", rx.avmute_skip);
		rx_pr("rx.no_signal=%d,rx.state=%d,",
				rx.no_signal,
				rx.state);
		rx_pr("skip frame=%d\n", rx.skip);
		rx_pr("fmt=0x%x,sw_vic:%d,",
				hdmirx_hw_get_fmt(),
				rx.cur.sw_vic);
		rx_pr("sw_dvi:%d,sw_fp:%d,",
				rx.cur.sw_dvi,
				rx.cur.sw_fp);
		rx_pr("sw_alternative:%d\n",
			rx.cur.sw_alternative);
		rx_pr("HDCP debug value=0x%x\n",
			hdmirx_rd_dwc(DWC_HDCP_DBG));
		rx_pr("HDCP14 state:%d\n",
			rx.cur.hdcp14_state);
		rx_pr("HDCP22 state:%d\n",
			rx.cur.hdcp22_state);
		rx_pr("audio receive data:%d\n",
			auds_rcv_sts);
		rx_pr("avmute_skip:0x%x\n", rx.avmute_skip);
	}
	if (enable & 2) {
		rx_getaudinfo(&a);
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
	if (enable & 4) {
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
		rx_pr("phy addr: %#x,%#x,port: %d, up phy addr:%#x\n",
			hdmirx_rd_top(TOP_EDID_RAM_OVR1_DATA),
			hdmirx_rd_top(TOP_EDID_RAM_OVR2_DATA),
				rx.port, up_phy_addr);
		rx_pr("downstream come: %d hpd:%d hdr lume:%d\n",
			new_edid, repeat_plug, new_hdr_lum);
	}
}

bool is_wr_only_reg(uint32_t addr)
{
	int i;

	/*sizeof(wr_only_register)/sizeof(uint32_t)*/
	for (i = 0; i < sizeof(wr_only_register)/sizeof(uint32_t); i++) {
		if (addr == wr_only_register[i])
			return false;
	}
	return true;
}

void rx_set_global_varaible(const char *buf, int size)
{
	char tmpbuf[60];
	int i = 0;
	long value = 0;
	int ret = 0;
	int index = 1;

	/* rx_pr("buf: %s size: %#x\n", buf, size); */

	if ((buf == 0) || (size == 0) || (size > 60))
		return;

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ') && (i < size)) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;
	/*skip the space*/
	while (++i < size) {
		if ((buf[i] != ' ') && (buf[i] != ','))
			break;
	}
	if ((buf[i] == '0') && ((buf[i + 1] == 'x') || (buf[i + 1] == 'X')))
		ret = kstrtol(buf + i + 2, 16, &value);
	else
		ret = kstrtol(buf + i, 10, &value);
	/* rx_pr("tmpbuf: %s value: %#x\n", tmpbuf, value); */

	if (ret != 0) {
		rx_pr("strtol error:%d\n", ret);
		return;
	}

	set_pr_var(tmpbuf, dwc_rst_wait_cnt_max, value, index);
	set_pr_var(tmpbuf, sig_stable_max, value, index);
	set_pr_var(tmpbuf, clk_debug, value, index);
	set_pr_var(tmpbuf, hpd_wait_max, value, index);
	set_pr_var(tmpbuf, sig_unstable_max, value, index);
	set_pr_var(tmpbuf, sig_unready_max, value, index);
	set_pr_var(tmpbuf, enable_hpd_reset, value, index);
	set_pr_var(tmpbuf, pow5v_max_cnt, value, index);
	set_pr_var(tmpbuf, sig_unstable_reset_hpd_max, value, index);
	set_pr_var(tmpbuf, rgb_quant_range, value, index);
	set_pr_var(tmpbuf, yuv_quant_range, value, index);
	set_pr_var(tmpbuf, it_content, value, index);
	set_pr_var(tmpbuf, diff_pixel_th, value, index);
	set_pr_var(tmpbuf, diff_line_th, value, index);
	set_pr_var(tmpbuf, diff_frame_th, value, index);
	set_pr_var(tmpbuf, port_map, value, index);
	set_pr_var(tmpbuf, edid_mode, value, index);
	set_pr_var(tmpbuf, force_vic, value, index);
	/* set_pr_var(tmpbuf, force_ready, value, index); */
	set_pr_var(tmpbuf, hdcp22_kill_esm, value, index);
	set_pr_var(tmpbuf, repeat_check, value, index);
	set_pr_var(tmpbuf, force_audio_sample_rate, value, index);
	set_pr_var(tmpbuf, run_eq_flag, value, index);
	set_pr_var(tmpbuf, pre_eq_freq, value, index);
	set_pr_var(tmpbuf, audio_sample_rate, value, index);
	set_pr_var(tmpbuf, auds_rcv_sts, value, index);
	set_pr_var(tmpbuf, audio_coding_type, value, index);
	set_pr_var(tmpbuf, audio_channel_count, value, index);
	set_pr_var(tmpbuf, hdcp22_capable_sts, value, index);
	set_pr_var(tmpbuf, esm_auth_fail_en, value, index);
	set_pr_var(tmpbuf, hdcp22_auth_sts, value, index);
	set_pr_var(tmpbuf, log_level, value, index);
	/* set_pr_var(tmpbuf, frame_skip_en, value, index); */
	set_pr_var(tmpbuf, auto_switch_off, value, index);
	set_pr_var(tmpbuf, clk_unstable_cnt, value, index);
	set_pr_var(tmpbuf, clk_unstable_max, value, index);
	set_pr_var(tmpbuf, clk_stable_cnt, value, index);
	set_pr_var(tmpbuf, clk_stable_max, value, index);
	set_pr_var(tmpbuf, eq_dbg_ch0, value, index);
	set_pr_var(tmpbuf, eq_dbg_ch1, value, index);
	set_pr_var(tmpbuf, eq_dbg_ch2, value, index);
	set_pr_var(tmpbuf, wait_no_sig_max, value, index);
	/*set_pr_var(tmpbuf, hdr_enable, value, index);*/
	set_pr_var(tmpbuf, receive_edid_len, value, index);
	set_pr_var(tmpbuf, new_edid, value, index);
	set_pr_var(tmpbuf, hdr_lum_len, value, index);
	set_pr_var(tmpbuf, new_hdr_lum, value, index);
	set_pr_var(tmpbuf, hdcp_array_len, value, index);
	set_pr_var(tmpbuf, hdcp_len, value, index);
	set_pr_var(tmpbuf, hdcp_repeat_depth, value, index);
	set_pr_var(tmpbuf, new_hdcp, value, index);
	set_pr_var(tmpbuf, repeat_plug, value, index);
	set_pr_var(tmpbuf, up_phy_addr, value, index);
	set_pr_var(tmpbuf, hdcp22_reset_max, value, index);
	set_pr_var(tmpbuf, hpd_to_esm, value, index);
	set_pr_var(tmpbuf, reset_esm_flag, value, index);
	set_pr_var(tmpbuf, video_stable_to_esm, value, index);
	set_pr_var(tmpbuf, hdcp_mode_sel, value, index);
	set_pr_var(tmpbuf, hdcp_auth_status, value, index);
	set_pr_var(tmpbuf, loadkey_22_hpd_delay, value, index);
	set_pr_var(tmpbuf, wait_hdcp22_cnt, value, index);
	set_pr_var(tmpbuf, wait_hdcp22_cnt1, value, index);
	set_pr_var(tmpbuf, wait_hdcp22_cnt2, value, index);
	set_pr_var(tmpbuf, wait_hdcp22_cnt3, value, index);
	set_pr_var(tmpbuf, enable_hdcp22_loadkey, value, index);
	set_pr_var(tmpbuf, do_esm_rst_flag, value, index);
	set_pr_var(tmpbuf, enable_hdcp22_esm_log, value, index);
	set_pr_var(tmpbuf, hdcp22_firmware_ok_flag, value, index);
	set_pr_var(tmpbuf, esm_err_force_14, value, index);
	set_pr_var(tmpbuf, reboot_esm_done, value, index);
	set_pr_var(tmpbuf, esm_reboot_lvl, value, index);
	set_pr_var(tmpbuf, enable_esm_reboot, value, index);
	set_pr_var(tmpbuf, esm_error_flag, value, index);
	set_pr_var(tmpbuf, esm_data_base_addr, value, index);
	set_pr_var(tmpbuf, is_hdcp_source, value, index);
	set_pr_var(tmpbuf, stable_check_lvl, value, index);
	set_pr_var(tmpbuf, hdcp22_reauth_enable, value, index);
	set_pr_var(tmpbuf, hdcp22_stop_auth_enable, value, index);
	set_pr_var(tmpbuf, hdcp22_stop_auth, value, index);
	set_pr_var(tmpbuf, hdcp22_esm_reset2_enable, value, index);
	set_pr_var(tmpbuf, hdcp22_esm_reset2, value, index);
}

void rx_get_global_varaible(const char *buf)
{
	int i = 1;

	rx_pr("index %-30s   value\n", "varaible");
	/* pr_var( sig_pll_unlock_cnt ); */
	/* pr_var( sig_pll_unlock_max ); */
	/* pr_var( sig_pll_lock_max ); */
	pr_var(dwc_rst_wait_cnt_max, i++);
	pr_var(sig_stable_max, i++);
	pr_var(clk_debug, i++);
	pr_var(hpd_wait_max, i++);
	pr_var(sig_unstable_max, i++);
	pr_var(sig_unready_max, i++);
	pr_var(enable_hpd_reset, i++);
	pr_var(pow5v_max_cnt, i++);
	pr_var(sig_unstable_reset_hpd_max, i++);
	pr_var(rgb_quant_range, i++);
	pr_var(yuv_quant_range, i++);
	pr_var(it_content, i++);
	pr_var(diff_pixel_th, i++);
	pr_var(diff_line_th, i++);
	pr_var(diff_frame_th, i++);
	pr_var(port_map, i++);
	pr_var(edid_mode, i++);
	pr_var(force_vic, i++);
	pr_var(hdcp22_kill_esm, i++);
	pr_var(repeat_check, i++);
	pr_var(force_audio_sample_rate, i++);
	pr_var(run_eq_flag, i++);
	pr_var(pre_eq_freq, i++);
	pr_var(audio_sample_rate, i++);
	pr_var(auds_rcv_sts, i++);
	pr_var(audio_coding_type, i++);
	pr_var(audio_channel_count, i++);
	pr_var(hdcp22_capable_sts, i++);
	pr_var(esm_auth_fail_en, i++);
	pr_var(hdcp22_auth_sts, i++);
	pr_var(log_level, i++);
	/* pr_var( frame_skip_en , i++); */
	pr_var(auto_switch_off, i++);
	pr_var(clk_unstable_cnt, i++);
	pr_var(clk_unstable_max, i++);
	pr_var(clk_stable_cnt, i++);
	pr_var(clk_stable_max, i++);
	pr_var(eq_dbg_ch0, i++);
	pr_var(eq_dbg_ch1, i++);
	pr_var(eq_dbg_ch2, i++);
	pr_var(wait_no_sig_max, i++);
	/*pr_var(hdr_enable, i++);*/
	pr_var(receive_edid_len, i++);
	pr_var(new_edid, i++);
	pr_var(hdr_lum_len, i++);
	pr_var(new_hdr_lum, i++);
	pr_var(hdcp_array_len, i++);
	pr_var(hdcp_len, i++);
	pr_var(hdcp_repeat_depth, i++);
	pr_var(new_hdcp, i++);
	pr_var(repeat_plug, i++);
	pr_var(up_phy_addr, i++);
	pr_var(hdcp22_reset_max, i++);
	pr_var(hpd_to_esm, i++);
	pr_var(reset_esm_flag, i++);
	pr_var(video_stable_to_esm, i++);
	pr_var(hdcp_mode_sel, i++);
	pr_var(hdcp_auth_status, i++);
	pr_var(loadkey_22_hpd_delay, i++);
	pr_var(wait_hdcp22_cnt, i++);
	pr_var(wait_hdcp22_cnt1, i++);
	pr_var(wait_hdcp22_cnt2, i++);
	pr_var(wait_hdcp22_cnt3, i++);
	pr_var(enable_hdcp22_loadkey, i++);
	pr_var(do_esm_rst_flag, i++);
	pr_var(enable_hdcp22_esm_log, i++);
	pr_var(hdcp22_firmware_ok_flag, i++);
	pr_var(esm_err_force_14, i++);
	pr_var(reboot_esm_done, i++);
	pr_var(esm_reboot_lvl, i++);
	pr_var(enable_esm_reboot, i++);
	pr_var(esm_error_flag, i++);
	pr_var(esm_data_base_addr, i++);
	pr_var(is_hdcp_source, i++);
	pr_var(stable_check_lvl, i++);
	pr_var(hdcp22_reauth_enable, i++);
	pr_var(hdcp22_stop_auth_enable, i++);
	pr_var(hdcp22_stop_auth, i++);
	pr_var(hdcp22_esm_reset2_enable, i++);
	pr_var(hdcp22_esm_reset2, i++);
}

void print_reg(uint start_addr, uint end_addr)
{
	int i;

	if (end_addr < start_addr)
		return;

	for (i = start_addr; i <= end_addr; i += sizeof(uint)) {
		if ((i - start_addr) % (sizeof(uint)*4) == 0)
			rx_pr("[0x%-4x] ", i);
		if (!is_wr_only_reg(i))
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
	rx_pr("0x1 1.4 edid\n");
	rx_pr("0x2 1.4 edid with audio blocks\n");
	rx_pr("0x3 1.4 edid with 420 capability\n");
	rx_pr("0x4 1.4 edid with 420 video data\n");
	rx_pr("0x5 2.0 edid with HDR,DV,420\n");
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

void timer_state(void)
{
	rx_pr("timer state:%d\n",
		rx.state);
}

int rx_debug_wr_reg(const char *buf, char *tmpbuf, int i)
{
	long adr = 0;
	long value = 0;

	if (kstrtol(tmpbuf + 3, 16, &adr) < 0)
			return -EINVAL;
	rx_pr("adr = %#x\n", adr);
	if (kstrtol(buf + i + 1, 16, &value) < 0)
		return -EINVAL;
	rx_pr("value = %#x\n", value);
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
		} else if (buf[2] == 'u') {
			wr_reg_hhi(adr, value);
			rx_pr("write %x to hiu [%x]\n",
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
	return 0;
}

int rx_debug_rd_reg(const char *buf, char *tmpbuf)
{
	long adr = 0;
	long value = 0;

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
		} else if (tmpbuf[2] == 'u') {
			value = rd_reg_hhi(adr);
			rx_pr("hiu [%x]=%x\n", adr, value);
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
	return 0;
}

void rx_debug_load22key(void)
{
	int ret = 0;

	if (enable_hdcp22_loadkey) {
		rx_pr("load 2.2 key-a\n");
		ret = rx_sec_set_duk();
		rx_pr("ret = %d\n", ret);
		if (ret == 1) {
			if ((hdmirx_rd_dwc(0xf68) & _BIT(3)) == 0) {
				rx_pr("load-1\n");
				sm_pause = 1;
				rx_set_hpd(0);
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
				rx_set_hpd(1);
				/* rx.state = FSM_HDMI5V_HIGH; */
				/* pre_port = 0xee; */
				sm_pause = 0;
			} else {
				rx_pr("load-2\n");
				sm_pause = 1;
				rx_set_hpd(0);
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
				rx_set_hpd(1);
				sm_pause = 0;
			}
		} else
			hdcp22_on = 0;
	} else
		rx_pr("load-2-no\n");
}

void rx_debug_hdcp14(void)
{
	rx_set_hpd(0);
	force_hdcp14_en = 1;
	hdcp22_on = 0;
	hdmirx_wr_dwc(DWC_HDCP22_CONTROL, 0x2);
	video_stable_to_esm = 0;
	rx.state = FSM_HPD_HIGH;
	rx.pre_state = FSM_HPD_HIGH;
	rx_pr("force hdcp1.4\n");
}

void rx_debug_hdcpauto(void)
{
	rx_set_hpd(0);
	hdcp22_on = 1;
	force_hdcp14_en = 0;
	hdmirx_hw_config();
	hpd_to_esm = 1;
	rx.state = FSM_HPD_HIGH;
	rx.pre_state = FSM_HPD_HIGH;
	rx_pr("hdcp22 auto\n");
}

void rx_debug_loadkey(void)
{
	rx_pr("load hdcp key\n");
	memcpy(&rx.hdcp, &init_hdcp_data,
	       sizeof(struct hdmi_rx_ctrl_hdcp));
	hdmirx_hw_config();
	pre_port = 0xfe;
}

int hdmirx_debug(const char *buf, int size)
{
	char tmpbuf[128];
	int i = 0;
	long value = 0;

	char input[5][20];
	char *const delim = " ";
	char *token;
	char *cur;
	int cnt = 0;

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;

	/*cmd interface : cmd + param1 + param2 ...*/
	/*cmd= input[0]*/
	/*param1= input[1]*/
	/*param2= input[2]*/
	for (cnt = 0; cnt < 5; cnt++)
		input[cnt][0] = '\0';

	cur = (char *)buf;
	cnt = 0;
	while ((token = strsep(&cur, delim)) && (cnt < 5)) {
		if (strlen((char *)token) < 20)
			strcpy(&input[cnt][0], (char *)token);
		else
			rx_pr("err input\n");
		cnt++;
	}

	if (strncmp(tmpbuf, "help", 4) == 0) {
		rx_pr("*****************\n");
		rx_pr("reset0--hw_config\n");
		rx_pr("reset1--8bit phy rst\n");
		rx_pr("reset3--irq open\n");
		rx_pr("reset4--edid_update\n");
		rx_pr("reset5--esm rst\n");
		rx_pr("database--esm data addr\n");
		rx_pr("duk--dump duk\n");
		rx_pr("suspend_pddq--pddqdown\n");
		rx_pr("*****************\n");
	} else if (strncmp(tmpbuf, "hpd", 3) == 0)
		rx_set_hpd(tmpbuf[3] == '0' ? 0 : 1);
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
			hdmirx_phy_init();
		} else if (tmpbuf[5] == '3') {
			rx_pr(" irq open\n");
			hdmirx_irq_enable(TRUE);
		} else if (tmpbuf[5] == '4') {
			rx_pr(" edid update\n");
			hdmi_rx_ctrl_edid_update();
		} else if (tmpbuf[5] == '5') {
			#ifdef HDCP22_ENABLE
			hdmirx_hdcp22_esm_rst();
			#endif
		}
	} else if (strncmp(tmpbuf, "state", 5) == 0) {
		if (tmpbuf[5] == '1')
			dump_state(0xff);
		else
			dump_state(1);
	} else if (strncmp(tmpbuf, "database", 5) == 0) {
		rx_pr("data base = 0x%x\n", esm_data_base_addr);
	} else if (strncmp(tmpbuf, "hdcp14", 6) == 0) {
		rx_debug_hdcp14();
	} else if (strncmp(tmpbuf, "hdcpauto", 8) == 0) {
		rx_debug_hdcpauto();
	} else if (strncmp(tmpbuf, "pause", 5) == 0) {
		if (kstrtol(tmpbuf + 5, 10, &value) < 0)
			return -EINVAL;
		rx_pr("%s the state machine\n",
			value ? "pause" : "enable");
		sm_pause = value;
	} else if (strncmp(tmpbuf, "reg", 3) == 0) {
		dump_reg();
	} else if (strncmp(tmpbuf, "eq", 2) == 0) {
		dump_eq_data();
	}  else if (strncmp(tmpbuf, "duk", 3) == 0) {
		rx_pr("hdcp22=%d\n", rx_sec_set_duk());
	} else if (strncmp(tmpbuf, "edid", 4) == 0) {
		dump_edid_reg();
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
		rx_debug_loadkey();
	} else if (strncmp(tmpbuf, "timer_state", 11) == 0) {
		timer_state();
	} else if (strncmp(tmpbuf, "load22key", 9) == 0) {
		rx_debug_load22key();
	} else if (strncmp(tmpbuf, "esm0", 4) == 0) {
		switch_set_state(&rx.hpd_sdev, 0x0);
	} else if (strncmp(tmpbuf, "esm1", 4) == 0) {
		switch_set_state(&rx.hpd_sdev, 0x01);
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
	} else if (strncmp(input[0], "pktinfo", 7) == 0) {
		rx_debug_pktinfo(input);
	} else if (tmpbuf[0] == 'w') {
		rx_debug_wr_reg(buf, tmpbuf, i);
	} else if (tmpbuf[0] == 'r') {
		rx_debug_rd_reg(buf, tmpbuf);
	} else if (tmpbuf[0] == 'v') {
		rx_pr("------------------\n");
		rx_pr("Hdmirx version: %s\n", RX_VER0);
		rx_pr("Hdmirx version: %s\n", RX_VER1);
		rx_pr("Hdmirx version: %s\n", RX_VER2);
		rx_pr("Hdmirx version: %s\n", RX_VER3);
		rx_pr("Hdmirx version: %s\n", RX_VER4);
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
	/* memset(&rx, 0, sizeof(struct rx_s)); */
	/* memset(rx.pow5v_state, */
	/*	0, */
	/*	sizeof(rx.pow5v_state)); */
	rx.port = (port - TVIN_PORT_HDMI0) & 0xf;
	rx.no_signal = false;
	rx.wait_no_sig_cnt = 0;
	is_hdcp_source = true;
	if (hdmirx_repeat_support())
		rx.hdcp.repeat = repeat_plug;
	else
		rx.hdcp.repeat = 0;

	if (pre_port != rx.port) {
		memset(&rx.pre, 0, sizeof(struct rx_video_info));
		memcpy(rx.hdcp.bksv, init_hdcp_data.bksv,
			sizeof(init_hdcp_data.bksv));
		memcpy(rx.hdcp.keys, init_hdcp_data.keys,
			sizeof(init_hdcp_data.keys));
		memset(&rx.vsi_info, 0,
			sizeof(struct vsi_info_s));
		hdmirx_hdcp_version_set(HDCP_VERSION_NONE);
		#ifdef HDCP22_ENABLE
		if (hdcp22_on) {
			esm_set_stable(0);
			hpd_to_esm = 1;
			/* switch_set_state(&rx.hpd_sdev, 0x01); */
			if (log_level & VIDEO_LOG)
				rx_pr("switch_set_state:%d\n", pwr_sts);
		}
		#endif
		if (pre_port != E_5V_LOST) {
			rx.state = FSM_HPD_LOW;
			rx_set_hpd(0);
		}
		/* need reset the whole module when switch port */
		hdmirx_hw_config();
		pre_port = rx.port;
		rx_set_eq_run_state(E_EQ_START);
	} else {
		if (0 == get_cur_hpd_sts())
			rx.state = FSM_HPD_HIGH;
		else if (rx.state >= FSM_SIG_STABLE)
			rx.state = FSM_SIG_STABLE;
		else
			rx.state = FSM_HPD_HIGH;
	}
	do_hpd_reset_flag = 0;

	/*initial packet moudle resource*/
	rx_pkt_initial();

	rx_pr("%s:%d\n", __func__, rx.port);
}

void hdmirx_hw_uninit(void)
{
	if (sm_pause)
		return;
}

