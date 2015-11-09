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
/* #include <linux/amports/canvas.h> */
#include <linux/uaccess.h>
#include <linux/delay.h>
/* #include <mach/clock.h> */
/* #include <mach/register.h> */
/* #include <mach/power_gate.h> */

/* if (is_meson_g9tv_cpu() || is_meson_m8_cpu() || */
/* is_meson_m8m2_cpu() || is_meson_gxbb_cpu() || */
/* is_meson_m8b_cpu()) */

#include <linux/amlogic/tvin/tvin.h>
/* Local include */
#include "hdmirx_drv.h"
#include "hdmi_rx_reg.h"
#ifdef CEC_FUNC_ENABLE
#include "hdmirx_cec.h"
#endif
#ifdef CONFIG_TVIN_VDIN_CTRL
/* #define CONFIG_AML_AUDIO_DSP */
#endif
#ifdef CONFIG_AML_AUDIO_DSP
#define M2B_IRQ0_DSP_AUDIO_EFFECT (7)
#define DSP_CMD_SET_HDMI_SR   (6)


#endif

#define HDMI_STATE_CHECK_FREQ     (20*5)
#define HW_MONITOR_TIME_UNIT    (1000/HDMI_STATE_CHECK_FREQ)

static int audio_enable = 1;
MODULE_PARM_DESC(audio_enable, "\naudio_enable\n");
module_param(audio_enable, int, 0664);

static int sample_rate_change_th = 1000;
MODULE_PARM_DESC(sample_rate_change_th, "\n sample_rate_change_th\n");
module_param(sample_rate_change_th, int, 0664);

static int aud_sr_stable_th = 15;
MODULE_PARM_DESC(aud_sr_stable_th, "\n aud_sr_stable_th\n");
module_param(aud_sr_stable_th, int, 0664);

static int sig_pll_unlock_cnt;
static int sig_pll_unlock_max = 100;
MODULE_PARM_DESC(sig_pll_unlock_max, "\n sig_pll_unlock_max\n");
module_param(sig_pll_unlock_max, int, 0664);

static int sig_pll_lock_cnt;
static unsigned sig_pll_lock_max = 20;
MODULE_PARM_DESC(sig_pll_lock_max, "\n sig_pll_lock_max\n");
module_param(sig_pll_lock_max, int, 0664);

static bool force_hdmi_5v_high;
MODULE_PARM_DESC(force_hdmi_5v_high, "\n force_hdmi_5v_high\n");
module_param(force_hdmi_5v_high, bool, 0664);

static int sig_lost_lock_cnt;
static int sig_lost_lock_max = 3;
MODULE_PARM_DESC(sig_lost_lock_max, "\n sig_lost_lock_max\n");
module_param(sig_lost_lock_max, int, 0664);

static int sig_stable_cnt;
static int sig_stable_max = 30;
MODULE_PARM_DESC(sig_stable_max, "\n sig_stable_max\n");
module_param(sig_stable_max, int, 0664);

static int hpd_wait_cnt;
static int hpd_wait_max = 10;
MODULE_PARM_DESC(hpd_wait_max, "\n hpd_wait_max\n");
module_param(hpd_wait_max, int, 0664);

static int sig_unstable_cnt;
static int sig_unstable_max = 50;
MODULE_PARM_DESC(sig_unstable_max, "\n sig_unstable_max\n");
module_param(sig_unstable_max, int, 0664);

static int sig_unready_cnt;
static int sig_unready_max = 6;/* 10; */
MODULE_PARM_DESC(sig_unready_max, "\n sig_unready_max\n");
module_param(sig_unready_max, int, 0664);

static bool enable_hpd_reset;
MODULE_PARM_DESC(enable_hpd_reset, "\n enable_hpd_reset\n");
module_param(enable_hpd_reset, bool, 0664);

static int pow5v_max_cnt = 10;
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

static int it_content;
MODULE_PARM_DESC(it_content, "\n it_content\n");
module_param(it_content, int, 0664);

static bool current_port_hpd_ctl;
MODULE_PARM_DESC(current_port_hpd_ctl, "\n current_port_hpd_ctl\n");
module_param(current_port_hpd_ctl, bool, 0664);

static bool reset_phy_enable = true;
MODULE_PARM_DESC(reset_phy_enable, "\n reset_phy_enable\n");
module_param(reset_phy_enable, bool, 0664);

static int force_format;
MODULE_PARM_DESC(force_format, "\n force_format\n");
module_param(force_format, int, 0664);
/* timing diff offset */
static int diff_pixel_th = 30;
static int diff_line_th = 20;
static int diff_frame_th = 50;	/* 50*10 khz offset */
MODULE_PARM_DESC(diff_pixel_th, "\n diff_pixel_th\n");
module_param(diff_pixel_th, int, 0664);
MODULE_PARM_DESC(diff_line_th, "\n diff_line_th\n");
module_param(diff_line_th, int, 0664);
MODULE_PARM_DESC(diff_frame_th, "\n diff_frame_th\n");
module_param(diff_frame_th, int, 0664);

static int port_map = 0x3210;
MODULE_PARM_DESC(port_map, "\n port_map\n");
module_param(port_map, int, 0664);

static int cfg_clk = 25000;	/* 510/20*1000 */
MODULE_PARM_DESC(cfg_clk, "\n cfg_clk\n");
module_param(cfg_clk, int, 0664);

static int lock_thres = LOCK_THRES;
MODULE_PARM_DESC(lock_thres, "\n lock_thres\n");
module_param(lock_thres, int, 0664);

static int fast_switching;/* 1; */
MODULE_PARM_DESC(fast_switching, "\n fast_switching\n");
module_param(fast_switching, int, 0664);

static int fsm_enhancement = 1;
MODULE_PARM_DESC(fsm_enhancement, "\n fsm_enhancement\n");
module_param(fsm_enhancement, int, 0664);

static int port_select_ovr_en;
MODULE_PARM_DESC(port_select_ovr_en, "\n port_select_ovr_en\n");
module_param(port_select_ovr_en, int, 0664);

static int phy_cmu_config_force_val;
MODULE_PARM_DESC(phy_cmu_config_force_val, "\n phy_cmu_config_force_val\n");
module_param(phy_cmu_config_force_val, int, 0664);

static int phy_system_config_force_val;
MODULE_PARM_DESC(phy_system_config_force_val,
		 "\n phy_system_config_force_val\n");
module_param(phy_system_config_force_val, int, 0664);

static int acr_mode;
MODULE_PARM_DESC(acr_mode, "\n acr_mode\n");
module_param(acr_mode, int, 0664);

static int edid_mode;
MODULE_PARM_DESC(edid_mode, "\n edid_mode\n");
module_param(edid_mode, int, 0664);

static int force_vic;
MODULE_PARM_DESC(force_vic, "\n force_vic\n");
module_param(force_vic, int, 0664);

static int force_ready;
MODULE_PARM_DESC(force_ready, "\n force_ready\n");
module_param(force_ready, int, 0664);

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

static bool use_test_hdcp_key;
MODULE_PARM_DESC(use_test_hdcp_key, "\n use_test_hdcp_key\n");
module_param(use_test_hdcp_key, bool, 0664);

static bool use_audioresample_reset;
MODULE_PARM_DESC(use_audioresample_reset, "\n use_audioresample_reset\n");
module_param(use_audioresample_reset, bool, 0664);

/* 0x100--irq print; */
/* 0x200-other print */
/* bit 0, printk; bit 8 enable irq log */
int log_flag = 0x1;
MODULE_PARM_DESC(log_flag, "\n log_flag\n");
module_param(log_flag, int, 0664);

static bool frame_skip_en = 1;	/* skip frame when signal unstable */
MODULE_PARM_DESC(frame_skip_en, "\n frame_skip_en\n");
module_param(frame_skip_en, bool, 0664);

static bool use_frame_rate_check;
MODULE_PARM_DESC(use_frame_rate_check, "\n use_frame_rate_check\n");
module_param(use_frame_rate_check, bool, 0664);

/* video mode clock above or below 3.4Gbps */
static int pre_tmds_clk_div4;
static int force_clk_rate;

MODULE_PARM_DESC(force_clk_rate, "\n force_clk_rate\n");
module_param(force_clk_rate, int, 0664);

static bool hw_dbg_en = 1;	/* only for hardware test */
MODULE_PARM_DESC(hw_dbg_en, "\n hw_dbg_en\n");
module_param(hw_dbg_en, bool, 0664);

static int sig_dwc_wait_cnt;
static int sig_dwc_wait_max = 30;	/* 5;//10; */
MODULE_PARM_DESC(sig_dwc_wait_max, "\n sig_dwc_wait_max\n");
module_param(sig_dwc_wait_max, int, 0664);
static int last_color_fmt;
static bool reset_sw = true;
static int sm_pause;

/***********************
  TVIN driver interface
************************/

#define HDMIRX_HWSTATE_INIT					0
#define HDMIRX_HWSTATE_HDMI5V_LOW			1
#define HDMIRX_HWSTATE_HDMI5V_HIGH			2
#define HDMIRX_HWSTATE_HPD_READY			3
#define HDMIRX_HWSTATE_SIG_UNSTABLE         4
#define HDMIRX_HWSTATE_SIG_DWC_WAIT_STABLE  5
#define HDMIRX_HWSTATE_SIG_STABLE			6
#define HDMIRX_HWSTATE_TIMINGCHANGE			7
#define HDMIRX_HWSTATE_SIG_READY			8

struct rx rx;

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


static void dump_state(unsigned char enable);
static void dump_audio_info(unsigned char enable);

static unsigned int get_index_from_ref(struct hdmi_rx_ctrl_video *video_par);


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

static int clock_handler(struct hdmi_rx_ctrl *ctx)
{
	int error = 0;
	unsigned long tclk = 0;

	if (sm_pause)
		return 0;


	if (ctx == 0)
		return -EINVAL;

	tclk = hdmi_rx_ctrl_get_tmds_clk(ctx);
	/* hdmirx_get_video_info(ctx, &rx.video_params); */
	if (((tmds_clock_old + TMDS_TOLERANCE) > tclk) &&
	    ((tmds_clock_old - TMDS_TOLERANCE) < tclk)) {
		return 0;
	}
	if ((tclk == 0) && (tmds_clock_old != 0)) {
		/* if(video_format_change()) */
		rx.change = sig_unready_max;
		/* if (log_flag & 0x100) */
			/* rx_print("[hdmirx:] clk change\n", 0); */

		/* TODO Review if we need to turn off the display
		   video_if_mode(false, 0, 0, 0);
		 */
#if 0
		/* workaround for sticky HDMI mode */
		error |= rx_ctrl_config(ctx, rx.port, &rx.hdcp);
#endif
	} else {
#if 0
		error |=
		    hdmi_rx_phy_config(&rx.phy, rx.port, tclk,
				       v.deep_color_mode);
#endif
	}
	tmds_clock_old = ctx->tmds_clk;
/* #if MESSAGES */
	/* ctx->log_info("TMDS clock: %3u.%03uMHz", */
	/* ctx->tmds_clk / 1000, ctx->tmds_clk % 1000); */
/* #endif */
	return error;
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



/**
 * Video event handler
 * @param[in,out] ctx context information
 * @return error code
 */
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
	unsigned i = 0;
	uint32_t intr_hdmi = 0;
	uint32_t intr_md = 0;
#ifdef CEC_FUNC_ENABLE
	uint32_t intr_cec = 0;
#endif
	uint32_t intr_pedc = 0;
	/* uint32_t intr_aud_clk = 0; */
	uint32_t intr_aud_fifo = 0;
	uint32_t data = 0;
	unsigned long tclk = 0;
	unsigned long ref_clk;
	unsigned evaltime = 0;

	bool clk_handle_flag = false;
	bool video_handle_flag = false;
	/* bool audio_handle_flag = false; */
#ifdef CEC_FUNC_ENABLE
	bool cec_get_msg_flag = false;	/* cec */
	bool cec_get_ack_flag = false;
#endif
	bool vsi_handle_flag = false;

	ref_clk = ctx->md_clk;

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
#ifdef CEC_FUNC_ENABLE
	intr_cec =
	    hdmirx_rd_dwc(DWC_AUD_CLK_ISTS) &
	    hdmirx_rd_dwc(DWC_AUD_CLK_IEN);
	if (intr_cec != 0)
		hdmirx_wr_dwc(DWC_AUD_CLK_ICLR, intr_cec);

	/* EOM irq */
	if (intr_cec & (1 << 17)) {
		if (log_flag & CEC_LOG_ENABLE)
			rx_print("\nEOM\n");
		cec_get_msg_flag = true;
	}
	/* ACK irq */
	if (intr_cec & (1 << 16)) {
		if (log_flag & CEC_LOG_ENABLE)
			rx_print("\nACK\n");
		cec_get_ack_flag = true;
	}
#endif

	intr_aud_fifo =
	    hdmirx_rd_dwc(DWC_AUD_FIFO_ISTS) &
	    hdmirx_rd_dwc(DWC_AUD_FIFO_IEN);
	if (intr_aud_fifo != 0)
		hdmirx_wr_dwc(DWC_AUD_FIFO_ICLR, intr_aud_fifo);


	if (intr_hdmi != 0) {
		if (get(intr_hdmi, CLK_CHANGE) != 0) {
			clk_handle_flag = true;
			evaltime = (ref_clk * 4095) / 158000;
			data = hdmirx_rd_dwc(DWC_HDMI_CKM_RESULT);
			tclk = ((ref_clk * get(data, CLKRATE)) / evaltime);
			if (log_flag & 0x100)
				rx_print("[HDMIrx isr] CLK_CHANGE (%d)\n",
					     tclk);
			if (tclk == 0) {
				/* error |= hdmirx_interrupts_hpd(false); */
				error |=
				    hdmirx_control_clk_range(TMDS_CLK_MIN,
							     TMDS_CLK_MAX);
			} else {
				for (i = 0; i < TMDS_STABLE_TIMEOUT; i++)
					;

				tclk =
				    ((ref_clk * get(data, CLKRATE)) / evaltime);
				error |=
				    hdmirx_control_clk_range(tclk -
							     TMDS_CLK_DELTA,
							     tclk +
							     TMDS_CLK_DELTA);
				/* error |= hdmirx_interrupts_hpd(true); */
			}
			ctx->tmds_clk = tclk;
		}
		if (get(intr_hdmi, DCM_CURRENT_MODE_CHG) != 0) {
			if (log_flag & 0x400)
				rx_print
				    ("[HDMIrx isr] DMI DCM_CURRENT_MODE_CHG\n");
			video_handle_flag = true;
		}
		/* if (get(intr_hdmi, AKSV_RCV) != 0) { */
		/* if(log_flag&0x400) */
		/* rx_print("[HDMIrx isr] AKSV_RCV\n"); */
		/* //execute[hdmi_rx_ctrl_event_aksv_reception] = true; */
		/* } */
		ctx->debug_irq_hdmi++;
	}

	if (intr_md != 0) {
		if (get(intr_md, VIDEO_MODE) != 0) {
			if (log_flag & 0x400)
				rx_print("[HDMIrx isr] VIDEO_MODE: %x\n",
					     intr_md);
			video_handle_flag = true;
		}
		ctx->debug_irq_video_mode++;
	}

	if (intr_pedc != 0) {
		/* hdmirx_wr_dwc(RA_PDEC_ICLR, intr_pedc); */
		if (get(intr_pedc, DVIDET | AVI_CKS_CHG) != 0) {
			if (log_flag & 0x400)
				rx_print("[HDMIrx isr] AVI_CKS_CHG\n");
			video_handle_flag = true;
		}
		if (get(intr_pedc, VSI_CKS_CHG) != 0) {
			if (log_flag & 0x400)
				rx_print("[HDMIrx isr] VSI_CKS_CHG\n");
			vsi_handle_flag = true;
		}
		/* if (get(intr_pedc, AIF_CKS_CHG) != 0) { */
		/* if(log_flag&0x400) */
		/* rx_print("[HDMIrx isr] AIF_CKS_CHG\n"); */
		/* audio_handle_flag = true; */
		/* } */
		/* if (get(intr_pedc, PD_FIFO_NEW_ENTRY) != 0) { */
		/* if(log_flag&0x400) */
		/* rx_print("[HDMIrx isr] PD_FIFO_NEW_ENTRY\n"); */
		/* //execute[hdmi_rx_ctrl_event_packet_reception] = true; */
		/* } */
		if (get(intr_pedc, PD_FIFO_OVERFL) != 0) {
			if (log_flag & 0x100)
				rx_print("[HDMIrx isr] PD_FIFO_OVERFL\n");
			error |= hdmirx_packet_fifo_rst();
		}
		ctx->debug_irq_packet_decoder++;
	}
	/* if (intr_aud_clk != 0) { */
	/* if(log_flag&0x400) */
	/* rx_print("[HDMIrx isr] RA_AUD_CLK\n"); */
	/* ctx->debug_irq_audio_clock++; */
	/* } */

	if (intr_aud_fifo != 0) {
		if (get(intr_aud_fifo, AFIF_OVERFL | AFIF_UNDERFL) != 0) {
			if (log_flag & 0x100)
				rx_print
				    ("[HDMIrx isr] AFIF_OVERFL|AFIF_UNDERFL\n");
			error |= hdmirx_audio_fifo_rst();
		}
		ctx->debug_irq_audio_fifo++;
	}

	/* if (clk_handle_flag) */
	/*	clock_handler(ctx); */

	/* if (video_handle_flag) */
	/*	video_handler(ctx); */

	/* if (vsi_handle_flag) */
	/*	vsi_handler(); */

#ifdef CEC_FUNC_ENABLE
	if ((cec_get_msg_flag) || (cec_get_ack_flag))
		cec_handler(cec_get_msg_flag, cec_get_ack_flag);

#endif
	return error;
}


irqreturn_t irq_handler(int irq, void *params)
{
	int error = 0;
	unsigned long hdmirx_top_intr_stat;
	if (params == 0) {
		rx_print("%s: %s\n",
			__func__,
			"RX IRQ invalid parameter");
		return IRQ_HANDLED;
	}

	hdmirx_top_intr_stat = hdmirx_rd_top(TOP_INTR_STAT);
reisr:hdmirx_wr_top(TOP_INTR_STAT_CLR, hdmirx_top_intr_stat);

	/* must clear ip interrupt quickly */
	if (hdmirx_top_intr_stat & (1 << 31)) {
		error = hdmi_rx_ctrl_irq_handler(&((struct rx *)params)->ctrl);
		if (error < 0) {
			if (error != -EPERM) {
				rx_print("%s: RX IRQ handler %d\n",
					__func__,
					error);
			}
		}
	}

	/* top interrupt handler */
	if (hdmirx_top_intr_stat & (0xf << 2)) {
		/* rx.tx_5v_status = true; */
		if (log_flag & 0x400)
			rx_print("[HDMIrx isr] 5v rise\n");
	}
	/* if (hdmirx_top_intr_stat & (0xf << 2)) */
	if (hdmirx_top_intr_stat & (0xf << 6)) {
		/* rx.tx_5v_status = false; */
		if (log_flag & 0x400)
			rx_print("[HDMIrx isr] 5v fall\n");
	}

	/* if (hdmirx_top_intr_stat & (0xf << 6)) */
	/* check the ip interrupt again */
	hdmirx_top_intr_stat = hdmirx_rd_top(TOP_INTR_STAT);
	if (hdmirx_top_intr_stat & (1 << 31)) {
		if (log_flag & 0x1000)
			rx_print("[HDMIrx isr] need clear ip irq---\n");
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
			    sample_rate_change_th) {
				ret_sample_rate =
				    sample_rate_info[i].sample_rate;
				break;
			}
		} else {
			if ((sample_rate_info[i].sample_rate -
			     rx.aud_info.arc) <
			    sample_rate_change_th) {
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
		sample_rate_change_th)
		ret = 1;

	return ret;
}

bool hdmirx_hw_check_frame_skip(void)
{
	if ((force_state & 0x10) || (!frame_skip_en))
		return false;
	else if ((rx.state != HDMIRX_HWSTATE_SIG_READY) || (rx.change > 0))
		return true;

	return false;
}

int hdmirx_hw_get_color_fmt(void)
{
	int color_format = 0;
	int format = rx.pre_video_params.video_format;

	if (force_format & 0x10)
		format = force_format & 0xf;

	if (rx.change > 0)
		return last_color_fmt;

	switch (format) {
	case 1:
		color_format = 3;	/* YUV422 */
		break;
	case 2:
		color_format = 1;	/* YUV444 */
		break;
	case 3:		/* YUV420 */
		color_format = 1;	/* YUV444 */
		break;
	case 0:
	default:
		color_format = 0;	/* RGB444 */
		break;
	}

	last_color_fmt = color_format;

	return color_format;
}

int hdmirx_hw_get_dvi_info(void)
{
	int ret = 0;

	if (rx.pre_video_params.sw_dvi)
		ret = 1;

	return ret;
}

int hdmirx_hw_get_3d_structure(unsigned char *_3d_structure,
			       unsigned char *_3d_ext_data)
{
	if ((rx.vendor_specific_info.identifier == 0x000c03) &&
	    (rx.vendor_specific_info.vd_fmt == 0x2)) {
		*_3d_structure = rx.vendor_specific_info._3d_structure;
		*_3d_ext_data = rx.vendor_specific_info._3d_ext_data;
		return 0;
	}
	return -1;
}

int hdmirx_hw_get_pixel_repeat(void)
{
	return rx.pre_video_params.pixel_repetition + 1;
}

unsigned char is_frame_packing(void)
{

#if 1
	return rx.pre_video_params.sw_fp;
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
	return rx.pre_video_params.sw_alternative;
}

struct freq_ref_s {
	unsigned int vic;
	unsigned char vesa_format;
	unsigned int ref_freq;	/* 8 bit tmds clock */
	uint16_t active_pixels;
	uint16_t active_lines;
	uint16_t active_lines_fp;
	uint16_t active_lines_alternative;
	uint8_t repetition_times;
	uint16_t frame_rate;
};

struct freq_ref_s freq_ref[] = {
/* basic format*/
	{HDMI_640x480p60, 0, 25000, 640, 480, 480, 480, 0, 3000},
	{HDMI_480p60, 0, 27000, 720, 480, 1005, 480, 0, 3000},
	{HDMI_480p60_16x9, 0, 27000, 720, 480, 1005, 480, 0, 3000},
	{HDMI_480i60, 0, 27000, 1440, 240, 240, 240, 1, 3000},
	{HDMI_480i60_16x9, 0, 27000, 1440, 240, 240, 240, 1, 3000},
	{HDMI_576p50, 0, 27000, 720, 576, 1201, 576, 0, 2500},
	{HDMI_576p50_16x9, 0, 27000, 720, 576, 1201, 576, 0, 2500},
	{HDMI_576i50, 0, 27000, 1440, 288, 288, 288, 1, 2500},
	{HDMI_576i50_16x9, 0, 27000, 1440, 288, 288, 288, 1, 2500},
	{HDMI_576i50_16x9, 0, 27000, 1440, 145, 145, 145, 2, 2500},
	{HDMI_720p60, 0, 74250, 1280, 720, 1470, 720, 0, 3000},
	{HDMI_720p50, 0, 74250, 1280, 720, 1470, 720, 0, 2500},
	{HDMI_1080i60, 0, 74250, 1920, 540, 2228, 1103, 0, 3000},
	{HDMI_1080i50, 0, 74250, 1920, 540, 2228, 1103, 0, 2500},
	{HDMI_1080p60, 0, 148500, 1920, 1080, 1080, 2160, 0, 3000},
	{HDMI_1080p24, 0, 74250, 1920, 1080, 2205, 2160, 0, 1200},
	{HDMI_1080p25, 0, 74250, 1920, 1080, 2205, 2160, 0, 1250},
	{HDMI_1080p30, 0, 74250, 1920, 1080, 2205, 2160, 0, 1500},
	{HDMI_1080p50, 0, 148500, 1920, 1080, 1080, 2160, 0, 2500},
	/* extend format */
	{HDMI_1440x240p60, 0, 27000, 1440, 240, 240, 240, 1, 3000},
	{HDMI_1440x240p60_16x9, 0, 27000, 1440, 240, 240, 240, 1, 3000},
	{HDMI_2880x480i60, 0, 54000, 2880, 240, 240, 240, 2, 3000},
	{HDMI_2880x480i60_16x9, 0, 54000, 2880, 240, 240, 240, 2, 3000},
	{HDMI_2880x240p60, 0, 54000, 2880, 240, 240, 240, 2, 3000},
	{HDMI_2880x240p60_16x9, 0, 54000, 2880, 240, 240, 240, 2, 3000},
	{HDMI_1440x480p60, 0, 54000, 1440, 480, 480, 480, 1, 3000},
	{HDMI_1440x480p60_16x9, 0, 54000, 1440, 480, 480, 480, 1, 3000},

	{HDMI_1440x288p50, 0, 27000, 1440, 288, 288, 288, 1, 2500},
	{HDMI_1440x288p50_16x9, 0, 27000, 1440, 288, 288, 288, 1, 2500},
	{HDMI_2880x576i50, 0, 54000, 2880, 288, 288, 288, 2, 2500},
	{HDMI_2880x576i50_16x9, 0, 54000, 2880, 288, 288, 288, 2, 2500},
	{HDMI_2880x288p50, 0, 54000, 2880, 288, 288, 288, 2, 2500},
	{HDMI_2880x288p50_16x9, 0, 54000, 2880, 288, 288, 288, 2, 2500},
	{HDMI_1440x576p50, 0, 54000, 1440, 576, 576, 576, 1, 2500},
	{HDMI_1440x576p50_16x9, 0, 54000, 1440, 576, 576, 576, 1, 2500},

	{HDMI_2880x480p60, 0, 108000, 2880, 480, 480, 480, 2, 3000},
	{HDMI_2880x480p60_16x9, 0, 108000, 2880, 480, 480, 480, 2, 3000},
	{HDMI_2880x576p50, 0, 108000, 2880, 576, 576, 576, 2, 2500},
	{HDMI_2880x576p50_16x9, 0, 108000, 2880, 576, 576, 576, 2, 2500},
	{HDMI_1080i50_1250, 0, 72000, 1920, 540, 540, 540, 0, 2500},
	{HDMI_720p24, 0, 74250, 1280, 720, 1470, 720, 0, 1200},
	{HDMI_720p30, 0, 74250, 1280, 720, 1470, 720, 0, 1500},

/* vesa format*/
	{HDMI_800_600, 1, 0, 800, 600, 600, 600, 0, 0},
	{HDMI_1024_768, 1, 0, 1024, 768, 768, 768, 0, 0},
	{HDMI_720_400, 1, 0, 720, 400, 400, 400, 0, 0},
	{HDMI_1280_768, 1, 0, 1280, 768, 768, 768, 0, 0},
	{HDMI_1280_800, 1, 0, 1280, 800, 800, 800, 0, 0},
	{HDMI_1280_960, 1, 0, 1280, 960, 960, 960, 0, 0},
	{HDMI_1280_1024, 1, 0, 1280, 1024, 1024, 1024, 0, 0},
	{HDMI_1360_768, 1, 0, 1360, 768, 768, 768, 0, 0},
	{HDMI_1366_768, 1, 0, 1366, 768, 768, 768, 0, 0},
	{HDMI_1600_1200, 1, 0, 1600, 1200, 1200, 1200, 0, 0},
	{HDMI_1920_1200, 1, 0, 1920, 1200, 1200, 1200, 0, 0},
	{HDMI_1440_900, 1, 0, 1440, 900, 900, 900, 0, 0},
	{HDMI_1400_1050, 1, 0, 1400, 1050, 1050, 1050, 0, 0},
	{HDMI_1680_1050, 1, 0, 1680, 1050, 1050, 1050, 0, 0},
	/* 4k2k mode */
	{HDMI_3840_2160p, 0, 0, 3840, 2160, 2160, 2160, 0, 0},
	{HDMI_4096_2160p, 0, 0, 4096, 2160, 2160, 2160, 0, 0},
	/* 4k2k 420mode hactive = hactive/2 */
	{HDMI_2160p_50hz_420, 0, 0, 1920, 2160, 2160, 2160, 0, 0},
	{HDMI_2160p_60hz_420, 0, 0, 1920, 2160, 2160, 2160, 0, 0},
	{HDMI_1080p60, 0, 74250, 960, 1080, 1080, 1080, 0, 3000},

	/* for AG-506 */
	{HDMI_480p60, 0, 27000, 720, 483, 483, 483, 0, 3000},
	{0, 0, 0, 0, 0, 0, 0, 0, 0}
};

#if 0
unsigned int get_vic_from_timing(struct hdmi_rx_ctrl_video *video_par)
{
	int i;
	for (i = 0; freq_ref[i].vic; i++) {
		if ((abs
		     ((signed int)video_par->hactive -
		      (signed int)freq_ref[i].active_pixels) <= diff_pixel_th)
		    &&
		    ((abs
		      ((signed int)video_par->vactive -
		       (signed int)freq_ref[i].active_lines) <= diff_line_th)
		     ||
		     (abs
		      ((signed int)video_par->vactive -
		       (signed int)freq_ref[i].active_lines_fp) <= diff_line_th)
		     ||
		     (abs
		      ((signed int)video_par->vactive -
		       (signed int)freq_ref[i].active_lines_alternative) <=
		      diff_line_th)
		    )) {
			if ((abs
			     (video_par->refresh_rate -
			      freq_ref[i].frame_rate) <= diff_frame_th)
			    || (freq_ref[i].frame_rate == 0)) {
				break;
			}
		}
	}
	return freq_ref[i].vic;
}
#endif
unsigned int get_index_from_ref(struct hdmi_rx_ctrl_video *video_par)
{
	int i;
	for (i = 0; freq_ref[i].vic; i++) {
		if ((abs(video_par->hactive - freq_ref[i].active_pixels) <=
		     diff_pixel_th)
		    &&
		    ((abs(video_par->vactive - freq_ref[i].active_lines) <=
		      diff_line_th)
		     || (abs(video_par->vactive - freq_ref[i].active_lines_fp)
			 <= diff_line_th)
		     ||
		     (abs
		      (video_par->vactive -
		       freq_ref[i].active_lines_alternative) <= diff_line_th)
		    )) {
			if ((abs
			     (video_par->refresh_rate -
			      freq_ref[i].frame_rate) <= diff_frame_th)
			    || (freq_ref[i].frame_rate == 0)) {
				if ((HDMI_1360_768 == freq_ref[i].vic)
				    || (HDMI_1366_768 == freq_ref[i].vic)) {
					if (abs
					    (video_par->hactive -
					     freq_ref[i].active_pixels) <= 2)
						break;
				} else
					break;
			}
		}
	}
	return i;
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
	unsigned int vic = rx.pre_video_params.sw_vic;

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
		if (is_alternative() && (rx.pre_video_params.video_format == 3))
			fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
		break;
	case HDMI_1080p24:	/*32 */
		if (is_frame_packing())
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_24HZ_FRAME_PACKING;
		else if (is_alternative())
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_24HZ_ALTERNATIVE;
		else
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
		if (is_alternative() && (rx.pre_video_params.video_format == 3))
			fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
		break;
	case HDMI_1080p25:	/*33 */
		fmt = TVIN_SIG_FMT_HDMI_1920X1080P_25HZ;
		break;
	case HDMI_1080p30:	/*34 */
		if (is_frame_packing())
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_30HZ_FRAME_PACKING;
		else if (is_alternative())
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_30HZ_ALTERNATIVE;
		else
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_30HZ;
		break;
	case HDMI_720p24:	/*60 */
		if (is_frame_packing())
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_24HZ_FRAME_PACKING;
		else
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_24HZ;
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
		/* 4k2k mode */
	case HDMI_3840_2160p:
	case HDMI_2160p_50hz_420:
	case HDMI_2160p_60hz_420:
		fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
		break;
	case HDMI_4096_2160p:
		fmt = TVIN_SIG_FMT_HDMI_4096_2160_00HZ;
		break;
	default:
		break;
	}

	return fmt;
}

bool hdmirx_hw_pll_lock(void)
{
	if (rx.state == HDMIRX_HWSTATE_SIG_READY)
		return true;
	else
		return false;
}

bool hdmirx_hw_is_nosig(void)
{
	return rx.no_signal;
}

#if 1
static bool is_packetinfo_change(struct hdmi_rx_ctrl_video *pre,
				 struct hdmi_rx_ctrl_video *cur)
{
	/* 1. dvi */
	/* if (cur->dvi != cur->sw_dvi) */
	/* return true; */
	/* 2. hdcp encrypted */
	/* if((cur->hdcp_enc_state != pre->hdcp_enc_state)){ */
	/* printk("cur->hdcp_enc_state=%d,pre->hdcp_enc_state=%d\n",
	    cur->hdcp_enc_state,pre->hdcp_enc_state); */
	/* return true; */
	/* } */
	/* 3. colorspace change */
	if (cur->video_format != pre->video_format) {
		if (log_flag & VIDEO_LOG_ENABLE) {
			rx_print("cur->video_format=%d, pre->video_format=%d\n",
				cur->video_format,
				pre->video_format);
		}
		return true;
	}
	return false;
}
#endif
/*
 * check timing info
 */
static bool is_timing_stable(struct hdmi_rx_ctrl_video *pre,
			     struct hdmi_rx_ctrl_video *cur)
{
	bool ret = true;
	if ((abs((signed int)pre->hactive - (signed int)cur->hactive) >
			diff_pixel_th)
		|| (abs((signed int)pre->vactive - (signed int)cur->vactive) >
			diff_line_th)) {
		/* (pre->pixel_repetition != cur->pixel_repetition)) { */
		ret = false;

		if (log_flag & 0x200) {
			rx_print("[hdmirx] timing unstable:");
			rx_print("hactive(%d=>%d),",
				     pre->hactive,
				     cur->hactive);
			rx_print("vactive(%d=>%d),",
				     pre->vactive,
				     cur->vactive);
			rx_print("pixel_repeat(%d=>%d),",
				     pre->pixel_repetition,
				     cur->pixel_repetition);
			rx_print("video_format(%d=>%d)\n",
			     pre->video_format,
			     cur->video_format);
		}
	}
	return ret;
}

/*
 * check frame rate
 */
static bool is_frame_rate_change(struct hdmi_rx_ctrl_video *pre,
				 struct hdmi_rx_ctrl_video *cur)
{
	bool ret = false;
	unsigned int pre_rate = (unsigned int)pre->refresh_rate * 2;
	unsigned int cur_rate = (unsigned int)cur->refresh_rate * 2;

	if ((abs((signed int)pre_rate - (signed int)cur_rate) >
		diff_frame_th)) {
		ret = true;

		if (log_flag & 0x200) {
			rx_print("[hdmirx] frame rate");
			rx_print("change:refresh_rate");
			rx_print("(%d=>%d),frame_rate:%d\n",
			     pre->refresh_rate,
			     cur->refresh_rate,
			     cur_rate);
		}
	}
	return ret;
}

static int get_timing_fmt(struct hdmi_rx_ctrl_video *video_par)
{
	int i;
	int ret = 1;

	video_par->sw_vic = 0;
	video_par->sw_dvi = 0;
	video_par->sw_fp = 0;
	video_par->sw_alternative = 0;
	frame_rate = video_par->refresh_rate * 2;

	if ((frame_rate > 9000) && use_frame_rate_check) {
		if (log_flag & 0x200)
			rx_print("[hdmirx] frame_rate");
			rx_print("not support,sw_vic:%d,",
				     video_par->sw_vic);
			rx_print("hw_vic:%d,frame_rate:%d\n",
			     video_par->video_mode,
			     frame_rate);
		return ret;
	}
	/* HDMI format fast detection */
	for (i = 0; freq_ref[i].vic; i++) {
		if (freq_ref[i].vic == video_par->video_mode) {
			if ((abs(video_par->hactive - freq_ref[i].active_pixels)
			     <= diff_pixel_th)
			    &&
			    ((abs(video_par->vactive - freq_ref[i].active_lines)
			      <= diff_line_th)
			     ||
			     (abs
			      (video_par->vactive -
			       freq_ref[i].active_lines_fp) <= diff_line_th)
			     ||
			     (abs
			      (video_par->vactive -
			       freq_ref[i].active_lines_alternative) <=
			      diff_line_th))) {
				break;
			}
		}
	}
	/* hdmi mode */
	if (freq_ref[i].vic != 0) {
		/*found standard hdmi mode */
		video_par->sw_vic = freq_ref[i].vic;
		if ((freq_ref[i].active_lines != freq_ref[i].active_lines_fp)
		    && (abs(video_par->vactive - freq_ref[i].active_lines_fp) <=
			diff_line_th))
			video_par->sw_fp = 1;
		else if ((freq_ref[i].active_lines !=
			  freq_ref[i].active_lines_alternative)
			 &&
			 (abs
			  (video_par->vactive -
			   freq_ref[i].active_lines_alternative) <=
			  diff_line_th))
			video_par->sw_alternative = 1;
		/*********** repetition Check patch start ***********/
		if (repeat_check) {
			/* if(video_par->pixel_repetition != 0) { */
			if (video_par->pixel_repetition !=
			    freq_ref[i].repetition_times) {
				if (log_flag & PACKET_LOG_ENABLE)
					rx_print("\n repetition err1 %d",
						 video_par->pixel_repetition);
					rx_print(": %d(standard)",
					     freq_ref[i].repetition_times);
				video_par->pixel_repetition =
				    freq_ref[i].repetition_times;
			}
			/* } */
		}
		/************ repetition Check patch end ************/
		if (log_flag & 0x200)
			rx_print("[hdmirx] standard hdmi");
			rx_print("mode,sw_vic:%d,",
					video_par->sw_vic);
			rx_print("hw_vic:%d,",
					video_par->video_mode);
			rx_print("frame_rate:%d\n",
			     frame_rate);
		return ret;
	}

	/* check the timing information */
	i = get_index_from_ref(video_par);
	video_par->sw_vic = freq_ref[i].vic;

	/* if (video_par->video_mode != 0) */
	if (video_par->sw_vic != 0) {
		/* non standard vic mode */
		video_par->sw_dvi = video_par->dvi;
		if ((freq_ref[i].active_lines != freq_ref[i].active_lines_fp)
		    && (abs(video_par->vactive - freq_ref[i].active_lines_fp) <=
			diff_line_th))
			video_par->sw_fp = 1;
		else if ((freq_ref[i].active_lines !=
			  freq_ref[i].active_lines_alternative)
			 &&
			 (abs
			  (video_par->vactive -
			   freq_ref[i].active_lines_alternative) <=
			  diff_line_th))
			video_par->sw_alternative = 1;
		/*********** repetition Check patch start ***********/
		if (repeat_check) {
			/* if(video_par->pixel_repetition != 0) { */
			if (video_par->pixel_repetition !=
			    freq_ref[i].repetition_times) {
				if (log_flag & PACKET_LOG_ENABLE)
					rx_print("\n repetition error2");
					rx_print("%d : %d(standard)",
					     video_par->pixel_repetition,
					     freq_ref[i].repetition_times);
				video_par->pixel_repetition =
				    freq_ref[i].repetition_times;
			}
			/* } */
		}
		/************ repetition Check patch end ************/
		if (log_flag & 0x200)
			rx_print("[hdmirx] non standard");
			rx_print("hdmi mode,sw_vic:%d,",
				     video_par->sw_vic);
			rx_print("hw_vic:%d,",
				     video_par->video_mode);
			rx_print("frame_rate:%d\n",
			     frame_rate);
		return ret;
	}
	return ret;
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
	pre_tmds_clk_div4 = 0;
	rx.no_signal = true;
}

/* ---------------------------------------------------------- */
/* func:         port A,B,C,D  hdmitx-5v monitor & HPD control */
/* note:         G9TV portD no used */
/* ---------------------------------------------------------- */
void HPD_controller(void)
{
	uint32_t tmp_5v = 0;

	tmp_5v = hdmirx_rd_top(TOP_HPD_PWR5V);

	if ((tmp_5v >> 20) & 0x01) {
		if (rx.portA_pow5v_state < pow5v_max_cnt)
			rx.portA_pow5v_state++;
	} else {
		if (rx.portA_pow5v_state > 0)
			rx.portA_pow5v_state--;
	}
	if ((tmp_5v >> 21) & 0x01) {
		if (rx.portB_pow5v_state < pow5v_max_cnt)
			rx.portB_pow5v_state++;
	} else {
		if (rx.portB_pow5v_state > 0)
			rx.portB_pow5v_state--;
	}
	if ((tmp_5v >> 22) & 0x01) {
		if (rx.portC_pow5v_state < pow5v_max_cnt)
			rx.portC_pow5v_state++;
	} else {
		if (rx.portC_pow5v_state > 0)
			rx.portC_pow5v_state--;
	}

	if (multi_port_edid_enable) {
		/* ------------port A-------------// */
		if (rx.portA_pow5v_state == 0) {
			if (rx.port == 0)
				rx.current_port_tx_5v_status = 0;
			if ((current_port_hpd_ctl)
			    && (rx.portA_pow5v_state_pre == 1)) {
				hdmirx_set_hpd(0, 0);
				rx.portA_pow5v_state_pre = 0;
			}
		} else if (rx.portA_pow5v_state == pow5v_max_cnt) {
			if (rx.port == 0)
				rx.current_port_tx_5v_status = 1;
			if ((current_port_hpd_ctl)
			    && (rx.portA_pow5v_state_pre == 0)) {
				rx.portA_pow5v_state_pre = 1;
				hdmirx_set_hpd(0, 1);
			}
		}
		/* ------------port B-------------// */
		if (rx.portB_pow5v_state == 0) {
			if (rx.port == 1)
				rx.current_port_tx_5v_status = 0;
			if ((current_port_hpd_ctl)
			    && (rx.portB_pow5v_state_pre == 1)) {
				hdmirx_set_hpd(1, 0);
				rx.portB_pow5v_state_pre = 0;
			}
		} else if (rx.portB_pow5v_state == pow5v_max_cnt) {
			if (rx.port == 1)
				rx.current_port_tx_5v_status = 1;
			if ((current_port_hpd_ctl)
			    && (rx.portB_pow5v_state_pre == 0)) {
				rx.portB_pow5v_state_pre = 1;
				hdmirx_set_hpd(1, 1);
			}
		}
		/* -------------port C-------------// */
		if (rx.portC_pow5v_state == 0) {
			if (rx.port == 2)
				rx.current_port_tx_5v_status = 0;
			if ((current_port_hpd_ctl)
			    && (rx.portC_pow5v_state_pre == 1)) {
				hdmirx_set_hpd(2, 0);
				rx.portC_pow5v_state_pre = 0;
			}
		} else if (rx.portC_pow5v_state == pow5v_max_cnt) {
			if (rx.port == 2)
				rx.current_port_tx_5v_status = 1;
			if ((current_port_hpd_ctl)
			    && (rx.portC_pow5v_state_pre == 0)) {
				rx.portC_pow5v_state_pre = 1;
				hdmirx_set_hpd(2, 1);
			}
		}
		/* ------------------------------// */
	} else {
		if (rx.port == 0) {
			if (rx.portA_pow5v_state == pow5v_max_cnt)
				rx.current_port_tx_5v_status = 1;
			else
				rx.current_port_tx_5v_status = 0;
		} else if (rx.port == 1) {
			if (rx.portB_pow5v_state == pow5v_max_cnt)
				rx.current_port_tx_5v_status = 1;
			else
				rx.current_port_tx_5v_status = 0;
		} else if (rx.port == 2) {
			if (rx.portC_pow5v_state == pow5v_max_cnt)
				rx.current_port_tx_5v_status = 1;
			else
				rx.current_port_tx_5v_status = 0;
		}
	}
	if (force_hdmi_5v_high)
		rx.current_port_tx_5v_status = 1;
}

void hdmirx_error_count_config(void)
{
}

bool hdmirx_tmds_pll_lock(void)
{
	return true;
	if ((hdmirx_rd_dwc(DWC_HDMI_PLL_LCK_STS) & 0x01) == 0)
		return false;
	else
		return true;
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

void rx_aud_pll_ctl(bool en)
{
	int tmp = 0;
	if (en) {
		tmp = hdmirx_rd_top(TOP_ACR_CNTL_STAT) | (1<<11);
		hdmirx_wr_top(TOP_ACR_CNTL_STAT, tmp);
		tmp = hdmirx_rd_phy(PHY_MAINFSM_STATUS1);
		wr_reg(HHI_AUD_PLL_CNTL6, (tmp >> 9 & 3) << 28);
		wr_reg(HHI_AUD_PLL_CNTL5, 0x0000002e);
		wr_reg(HHI_AUD_PLL_CNTL4, 0x30000000);
		wr_reg(HHI_AUD_PLL_CNTL3, 0x00000000);
		wr_reg(HHI_AUD_PLL_CNTL, 0x40000000);
		wr_reg(HHI_ADC_PLL_CNTL4, 0x805);
		tmp = hdmirx_rd_top(TOP_ACR_CNTL_STAT) | (1<<11);
		hdmirx_wr_top(TOP_ACR_CNTL_STAT, tmp);

		if (use_audioresample_reset) {
			wr_reg(AUD_RESAMPLE_CTRL0,
				(rd_reg(AUD_RESAMPLE_CTRL0) |
					0x10000000) & 0x7fffffff);
		}
	} else{
		/* disable pll, into reset mode */
		wr_reg(HHI_AUD_PLL_CNTL, 0x20000000);
		if (use_audioresample_reset) {
			/* reset resample module */
			wr_reg(AUD_RESAMPLE_CTRL0,
				(rd_reg(AUD_RESAMPLE_CTRL0) |
					0x80000000) & 0xefffffff);
		}
	}
}

void hdmirx_clock_rate_monitor(void)
{
	static int clk_rate_status[5] = { 0 };
	static int idx;

	if (force_clk_rate & 0x10) {
		hdmirx_wr_phy(PHY_CDR_CTRL_CNT,
			      (hdmirx_rd_phy(PHY_CDR_CTRL_CNT) &
			       (~(1 << 8))) | ((force_clk_rate & 1) << 8));
	} else {
		idx++;
		if (idx >= 5)
			idx = 0;
		clk_rate_status[idx] =
		    (hdmirx_rd_dwc(DWC_SCDC_REGS0) >> 17) & 1;
		if ((clk_rate_status[0] == clk_rate_status[1])
		    && (clk_rate_status[1] == clk_rate_status[2])
		    && (clk_rate_status[2] == clk_rate_status[3])
		    && (clk_rate_status[3] == clk_rate_status[4])) {
			if (pre_tmds_clk_div4 != clk_rate_status[0]) {
				pre_tmds_clk_div4 = clk_rate_status[0];
				if (pre_tmds_clk_div4)
					hdmirx_wr_phy(PHY_CDR_CTRL_CNT,
						      hdmirx_rd_phy
						      (PHY_CDR_CTRL_CNT)
						      | (1 << 8));
				else
					hdmirx_wr_phy(PHY_CDR_CTRL_CNT,
						      hdmirx_rd_phy
						      (PHY_CDR_CTRL_CNT)
						      & (~(1 << 8)));
			}
		}
	}
}

void hdmirx_sw_reset(int level)
{
	unsigned long data32 = 0;
	if (level == 1) {
		data32 |= 0 << 7;	/* [7]vid_enable */
#ifdef CEC_FUNC_ENABLE
		data32 |= 1 << 5;	/* [5]cec_enable */
#else
		data32 |= 0 << 5;	/* [5]cec_enable */
#endif
		data32 |= 0 << 4;	/* [4]aud_enable */
		data32 |= 0 << 3;	/* [3]bus_enable */
		data32 |= 1 << 2;	/* [2]hdmi_enable */
		data32 |= 1 << 1;	/* [1]modet_enable */
		data32 |= 0 << 0;	/* [0]cfg_enable */

	} else if (level == 2) {
		data32 |= 0 << 7;	/* [7]vid_enable */
#ifdef CEC_FUNC_ENABLE
		data32 |= 1 << 5;	/* [5]cec_enable */
#else
		data32 |= 0 << 5;	/* [5]cec_enable */
#endif
		data32 |= 1 << 4;	/* [4]aud_enable */
		data32 |= 0 << 3;	/* [3]bus_enable */
		data32 |= 1 << 2;	/* [2]hdmi_enable */
		data32 |= 1 << 1;	/* [1]modet_enable */
		data32 |= 0 << 0;	/* [0]cfg_enable */
	}
	hdmirx_wr_dwc(DWC_DMI_SW_RST, data32);
}

void rx_dwc_reset(void)
{
	if (log_flag & VIDEO_LOG_ENABLE)
		rx_print("rx_dwc_reset\n");
	/* audio_status_init(); */
	/* Signal_status_init(); */
	hdmirx_packet_fifo_rst();
	hdmirx_audio_fifo_rst();
	hdmirx_sw_reset(2);
	/* hdmirx_wr_dwc(DWC_DMI_SW_RST, 0x10092); */
}

void rx_module_ctl(bool en)
{
	unsigned long data32;
	/* Enable functional modules */
	data32 = hdmirx_rd_dwc(DWC_DMI_DISABLE_IF);
	if (en) {
		/* data32  |=  (_BIT(7));  //vid_enable */
		data32 |= (_BIT(4));	/* audio_enable */
		data32 |= (_BIT(2));	/* hdmi_enable */
		data32 |= (_BIT(1));	/* modet_enable */
	} else {
		/* data32  &=  ~(_BIT(7));  //vid_enable */
		data32 &= ~(_BIT(4));	/* audio_enable */
		data32 &= ~(_BIT(2));	/* hdmi_enable */
		data32 &= ~(_BIT(1));	/* modet_enable */
	}
	hdmirx_wr_dwc(DWC_DMI_DISABLE_IF, data32);
	if (log_flag & REG_LOG_ENABLE)
		rx_print("hdmirx disable module %lx\n", data32);
}

void hdmirx_hw_monitor(void)
{
	int pre_sample_rate;
	if (sm_pause)
		return;
	HPD_controller();
	hdmirx_clock_rate_monitor();
	switch (rx.state) {
	case HDMIRX_HWSTATE_INIT:
		if (!multi_port_edid_enable) {
			hdmirx_set_hpd(rx.port, 0);
			hdmirx_hw_config();
			hdmi_rx_ctrl_edid_update();
		}
		audio_status_init();
		Signal_status_init();
		if (!multi_port_edid_enable)
			rx.state = HDMIRX_HWSTATE_HDMI5V_LOW;
		else
			rx.state = HDMIRX_HWSTATE_SIG_UNSTABLE;

		rx.pre_state = HDMIRX_HWSTATE_INIT;
		/* rx_print("Hdmirx driver version: %s\n", VER); */
		break;
	case HDMIRX_HWSTATE_HDMI5V_LOW:
		if (rx.current_port_tx_5v_status) {
			if (!multi_port_edid_enable) {
				if (hpd_wait_cnt++ < hpd_wait_max)
					break;
			}
			rx.state = HDMIRX_HWSTATE_HDMI5V_HIGH;
			rx.pre_state = HDMIRX_HWSTATE_HDMI5V_LOW;
			hpd_wait_cnt = 0;
			rx_print("\n[HDMIRX State] 5v low->5v high\n");
		}
		break;
	case HDMIRX_HWSTATE_HDMI5V_HIGH:
		if (!rx.current_port_tx_5v_status) {
			rx.no_signal = true;
			rx.state = HDMIRX_HWSTATE_HDMI5V_LOW;
			rx.pre_state = HDMIRX_HWSTATE_HDMI5V_HIGH;
			rx_print("\n[HDMIRX State] 5v high->HPD_LOW\n");
		} else {
			if (!multi_port_edid_enable)
				hdmirx_set_hpd(rx.port, 1);

			rx.state = HDMIRX_HWSTATE_HPD_READY;
			rx.pre_state = HDMIRX_HWSTATE_HDMI5V_HIGH;
			/* rx.no_signal = false; */
			rx_print("\n[HDMIRX State] 5v high->hpd ready\n");
		}
		break;
	case HDMIRX_HWSTATE_HPD_READY:
		if (!rx.current_port_tx_5v_status) {
			rx.no_signal = true;
			rx.state = HDMIRX_HWSTATE_HDMI5V_LOW;
			rx.pre_state = HDMIRX_HWSTATE_HPD_READY;
			/* hdmirx_set_hpd(rx.port, 0); */
			rx_print("\n[HDMIRX State] hpd ready ->HPD_LOW\n");
		} else {
			if (hpd_wait_cnt++ <= hpd_wait_max)
				break;
			hpd_wait_cnt = 0;
			rx.state = HDMIRX_HWSTATE_SIG_UNSTABLE;
			rx.pre_state = HDMIRX_HWSTATE_HPD_READY;
			rx_print("\n[HDMIRX State] hpd ready->unstable\n");
		}
		break;
	case HDMIRX_HWSTATE_TIMINGCHANGE:
		rx_print("[HDMIRX State] HDMIRX_HWSTATE_TIMINGCHANGE\n");
		if (reset_phy_enable)
			hdmirx_phy_init(rx.port, 0);
		pre_tmds_clk_div4 = 0;
		rx.state = HDMIRX_HWSTATE_SIG_UNSTABLE;
		rx.pre_state = HDMIRX_HWSTATE_TIMINGCHANGE;
		break;
	case HDMIRX_HWSTATE_SIG_UNSTABLE:
		if (rx.current_port_tx_5v_status == 0) {
			rx.no_signal = true;
			rx_print("[HDMIRX State] unstable->HPD_LOW\n");
			rx.state = HDMIRX_HWSTATE_HDMI5V_LOW;
			rx.pre_state = HDMIRX_HWSTATE_SIG_UNSTABLE;
			break;
		}
		/* hdmirx_clock_rate_monitor(); */
		/* tmds valid flag ,dwc0x30 bit0 */
		if (hdmirx_tmds_pll_lock()) {
			if (sig_pll_lock_cnt++ > sig_pll_lock_max) {
				memset(&rx.vendor_specific_info, 0,
				       sizeof(struct vendor_specific_info_s));
				/* hdmirx_get_video_info(&rx.ctrl, */
					/* &rx.cur_video_params); */
				if (reset_sw) {
					rx_module_ctl(1);
					rx_dwc_reset();
				}
				sig_pll_unlock_cnt = 0;
				sig_pll_lock_cnt = 0;
				rx.no_signal = false;
				rx.state = HDMIRX_HWSTATE_SIG_DWC_WAIT_STABLE;
				rx.pre_state = HDMIRX_HWSTATE_SIG_UNSTABLE;
				rx_print("[HDMIRX] unstable->");
				rx_print("DWC_WAIT_STABLE\n");
			}
		} else {
			if (sig_pll_lock_cnt) {
				rx_print("[HDMIRX] unstable:");
				rx_print("sig_pll_lock_cnt = %d\n",
				     sig_pll_lock_cnt);
			sig_pll_lock_cnt = 0;
			sig_pll_unlock_cnt++;
			if (sig_pll_unlock_cnt == sig_pll_unlock_max)
				rx.no_signal = true;

			if (sig_pll_unlock_cnt == sig_pll_unlock_max << 2) {
				hdmirx_error_count_config();
				rx.state = HDMIRX_HWSTATE_TIMINGCHANGE;
				rx.pre_state = HDMIRX_HWSTATE_SIG_UNSTABLE;
				pre_tmds_clk_div4 = 0;
				sig_pll_unlock_cnt = 0;
			}
		}
		break;
	case HDMIRX_HWSTATE_SIG_DWC_WAIT_STABLE:
		if (sig_dwc_wait_cnt++ > sig_dwc_wait_max) {
			sig_dwc_wait_cnt = 0;
			rx_print("[HDMIRX State]");
			rx_print("DWC_WAIT_STABLE->STABLE\n");
			rx.state = HDMIRX_HWSTATE_SIG_STABLE;
			rx.pre_state = HDMIRX_HWSTATE_SIG_DWC_WAIT_STABLE;
		}
		break;
	case HDMIRX_HWSTATE_SIG_STABLE:
		if (rx.current_port_tx_5v_status == 0) {
			rx.no_signal = true;
			rx_print("[HDMIRX State]");
			rx_print("stable->HPD_LOW\n");
			rx.state = HDMIRX_HWSTATE_HDMI5V_LOW;
			rx.pre_state = HDMIRX_HWSTATE_SIG_STABLE;
			break;
		}
		memcpy(&rx.pre_video_params, &rx.cur_video_params,
		       sizeof(struct hdmi_rx_ctrl_video));
		hdmirx_get_video_info(&rx.ctrl, &rx.cur_video_params);
		if (is_timing_stable(&rx.pre_video_params, &rx.cur_video_params)
		    || (force_ready)) {
			if (sig_stable_cnt++ > sig_stable_max) {
				get_timing_fmt(&rx.pre_video_params);
				if ((rx.pre_video_params.sw_vic ==
				     HDMI_MAX_IS_UNSUPPORT)
				    || (rx.pre_video_params.sw_vic ==
					HDMI_Unkown)) {
					rx_print("[HDMIRX] stable->");
					rx_print("unkown vic\n");
					if (sig_stable_cnt <
					    (sig_stable_max << 3))
						break;
					rx.state = HDMIRX_HWSTATE_TIMINGCHANGE;
					rx.pre_state =
					    HDMIRX_HWSTATE_SIG_STABLE;
					break;
				}
				if (rx.pre_video_params.sw_dvi == 1) {
					if (sig_stable_cnt <
					    (sig_stable_max * 7))
						break;
				}
				sig_stable_cnt = 0;
				sig_unstable_cnt = 0;
				rx.ctrl.tmds_clk2 = hdmirx_get_tmds_clock();
				rx.change = 0;
				/* if(reset_sw) */
				/* hdmirx_sw_reset(2); */
				/* rx_module_ctl(1); */
				rx.state = HDMIRX_HWSTATE_SIG_READY;
				rx.pre_state = HDMIRX_HWSTATE_SIG_STABLE;
				rx.no_signal = false;
				/* memcpy(&rx.video_params, */
				/* &rx.pre_video_params, */
				/* sizeof(struct hdmi_rx_ctrl_video)); */
				memset(&rx.aud_info, 0,
				       sizeof(struct aud_info_s));
				hdmirx_config_video(&rx.pre_video_params);
				rx_print("[HDMIRX] stable->ready\n");
				if (log_flag & VIDEO_LOG_ENABLE)
					dump_state(0x1);
			}
		} else {
			sig_unstable_cnt++;
			/* if(sig_unstable_cnt > 6) */
			/* sig_stable_cnt = 0; */
			if (sig_unstable_cnt > sig_unstable_max) {
				rx.state = HDMIRX_HWSTATE_TIMINGCHANGE;
				rx.pre_state = HDMIRX_HWSTATE_SIG_STABLE;
				sig_stable_cnt = 0;
				sig_unstable_cnt = 0;
				hdmirx_error_count_config();
				if (enable_hpd_reset) {
					sig_unstable_reset_hpd_cnt++;
					if (sig_unstable_reset_hpd_cnt >=
					    sig_unstable_reset_hpd_max) {
						rx.state =
						    HDMIRX_HWSTATE_HDMI5V_LOW;
						hdmirx_set_hpd(rx.port, 0);
						sig_unstable_reset_hpd_cnt = 0;
						rx_print("[HDMIRX] unstable");
						rx_print("->hpd_low\n");
						break;
					}
				}
				rx_print("[HDMIRX] stable->");
				rx_print("timingchange\n");
			}
		}
		break;
	case HDMIRX_HWSTATE_SIG_READY:
		if (rx.current_port_tx_5v_status == 0) {
			rx.no_signal = true;
			rx_print("[HDMIRX State] ready->HPD_LOW\n");
			hdmirx_audio_enable(0);
			hdmirx_audio_fifo_rst();
			/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
			if (is_meson_gxtvbb_cpu()) {
				rx_aud_pll_ctl(0);
				if (reset_sw)
					rx_module_ctl(0);
			}
			/* #endif */
			rx.state = HDMIRX_HWSTATE_INIT;
			rx.pre_state = HDMIRX_HWSTATE_SIG_READY;
			break;
		}
		hdmirx_get_video_info(&rx.ctrl, &rx.cur_video_params);
		rgb_quant_range = rx.cur_video_params.rgb_quant_range;
		it_content = rx.cur_video_params.it_content;
		if (hdmirx_tmds_pll_lock() == false) {
			if (sig_lost_lock_cnt++ >= sig_lost_lock_max) {
				sig_lost_lock_cnt = 0;
				rx.state = HDMIRX_HWSTATE_SIG_UNSTABLE;
				rx.pre_state = HDMIRX_HWSTATE_SIG_READY;
				audio_sample_rate = 0;
				/* #if (MESON_CPU_TYPE_MESONG9TV) */
				if (is_meson_gxtvbb_cpu()) {
					if (reset_sw)
						rx_module_ctl(0);

					rx_aud_pll_ctl(0);
				}
				/* #endif */
				hdmirx_audio_enable(0);
				hdmirx_audio_fifo_rst();
				if (log_flag & VIDEO_LOG_ENABLE) {
					rx_print("[hdmi] pll unlock !!");
					rx_print("TMDS clock = %d,",
						hdmirx_get_tmds_clock());
					rx_print("Pixel clock = %d\n",
						hdmirx_get_pixel_clock());
					rx_print("[HDMIRX State] pll");
					rx_print("unlock ready");
					rx_print("->timingchange\n");
				}
				break;
			}
		} else if ((!is_timing_stable(&rx.pre_video_params,
						&rx.cur_video_params)) ||
			    is_frame_rate_change(&rx.pre_video_params,
						 &rx.cur_video_params)) {
			sig_lost_lock_cnt = 0;
			/* memcpy(&rx.video_params, */
			/* &rx.pre_video_params, */
			/* sizeof(struct hdmi_rx_ctrl_video)); */
			if (sig_unready_cnt++ > sig_unready_max) {
				sig_lost_lock_cnt = 0;
				sig_unready_cnt = 0;
				audio_sample_rate = 0;
				rx_module_ctl(0);
				rx_aud_pll_ctl(0);
				hdmirx_audio_enable(0);
				hdmirx_audio_fifo_rst();
				rx.state = HDMIRX_HWSTATE_SIG_UNSTABLE;
				rx.pre_state = HDMIRX_HWSTATE_SIG_READY;
				memcpy(&rx.pre_video_params,
				       &rx.cur_video_params,
				       sizeof(struct
					      hdmi_rx_ctrl_video));
				memset(&rx.vendor_specific_info, 0,
				       sizeof(struct
					      vendor_specific_info_s));
				rx_print("[HDMIRX] ready->timing");
				rx_print("change:unstable\n");
				break;
			}
		} else if (is_packetinfo_change
				(&rx.pre_video_params, &rx.cur_video_params)) {
			/* memcpy(&rx.video_params, */
			/* &rx.pre_video_params, */
			/* sizeof(struct hdmi_rx_ctrl_video)); */
			if (sig_unready_cnt++ > sig_unready_max << 2) {
				sig_lost_lock_cnt = 0;
				sig_unready_cnt = 0;
				audio_sample_rate = 0;
/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
				if (is_meson_gxtvbb_cpu()) {
					rx_module_ctl(0);
					rx_aud_pll_ctl(0);
				}
				/* #endif */
				hdmirx_audio_enable(0);
				hdmirx_audio_fifo_rst();
				rx.state = HDMIRX_HWSTATE_SIG_UNSTABLE;
				rx.pre_state = HDMIRX_HWSTATE_SIG_READY;
				memcpy(&rx.pre_video_params,
				       &rx.cur_video_params,
				       sizeof(struct
					      hdmi_rx_ctrl_video));
				memset(&rx.vendor_specific_info, 0,
				       sizeof(struct
					      vendor_specific_info_s));
				rx_print("[HDMIRX] ready->timing");
				rx_print("change:colorspace\n");
				break;
			}
		} else {
				sig_unready_cnt = 0;
				if (enable_hpd_reset)
					sig_unstable_reset_hpd_cnt = 0;
		}
		if ((0 == audio_enable) ||
			(rx.pre_video_params.sw_dvi == 1))
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
		    (abs
		     ((signed int)rx.aud_info.real_sample_rate -
		      (signed int)pre_sample_rate) >
		     sample_rate_change_th)
		    ) {
			if (log_flag & AUDIO_LOG_ENABLE)
				dump_audio_info(1);
		}

		if (!is_sample_rate_stable
		    (pre_sample_rate,
		     rx.aud_info.real_sample_rate)) {
			if (log_flag & AUDIO_LOG_ENABLE)
				dump_audio_info(1);
			rx.aud_sr_stable_cnt = 0;
			break;
		}
		if (rx.aud_sr_stable_cnt <
		    aud_sr_stable_th) {
			rx.aud_sr_stable_cnt++;
			if (rx.aud_sr_stable_cnt ==
				aud_sr_stable_th) {
				dump_state(0x2);
				rx_aud_pll_ctl(1);
				/* hdmirx_config_audio(); */
				hdmirx_audio_enable(1);
				hdmirx_audio_fifo_rst();

				audio_sample_rate =
				    rx.aud_info.real_sample_rate;
				audio_coding_type =
				    rx.aud_info.coding_type;
				audio_channel_count =
				    rx.aud_info.channel_count;

				if (hdmirx_get_audio_clock() < 100000) {
					rx_print("update audio\n");
					hdmirx_wr_top(TOP_ACR_CNTL_STAT,
					     hdmirx_rd_top(TOP_ACR_CNTL_STAT)
					     |(1<<11));
				}
			}
		} else {

		}
		auds_rcv_sts =
		    rx.aud_info.audio_samples_packet_received;
	}
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

/*
* EDID & hdcp
*/
struct hdmi_rx_ctrl_hdcp init_hdcp_data;
#define MAX_KEY_BUF_SIZE 512

static char key_buf[MAX_KEY_BUF_SIZE];
static int key_size;

#define MAX_EDID_BUF_SIZE 1024
static char edid_buf[MAX_EDID_BUF_SIZE];
static int edid_size;

static unsigned char amlogic_hdmirx_edid_port1[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x05, 0xAC, 0x30, 0x00,
	0x01, 0x00, 0x00, 0x00,
0x0A, 0x18, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78, 0x0A, 0xCF, 0x74, 0xA3,
	0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0xFF, 0x01, 0xFF,
	0xFF, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x20, 0x08, 0xE8, 0x00, 0x30, 0xF2, 0x70,
	0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E, 0x02, 0x3A, 0x80, 0x18,
	0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E, 0x00, 0x00,
	0x00, 0xFC, 0x00, 0x41,
0x4D, 0x4C, 0x20, 0x38, 0x36, 0x36, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x01, 0xAF,
0x02, 0x03, 0x36, 0xF0, 0x5A, 0x1F, 0x10, 0x14, 0x05, 0x13, 0x04, 0x20,
	0x22, 0x3C, 0x3E, 0x12,
0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06, 0x01, 0x5F, 0x5D, 0x60, 0x61,
	0x64, 0x65, 0x66, 0x23,
0x09, 0x07, 0x01, 0x83, 0x01, 0x00, 0x00, 0x6E, 0x03, 0x0C, 0x00, 0x10,
	0x00, 0x88, 0x3C, 0x20,
0x80, 0x80, 0x01, 0x02, 0x03, 0x04, 0x02, 0x3A, 0x80, 0xD0, 0x72, 0x38,
	0x2D, 0x40, 0x10, 0x2C,
0x45, 0x80, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F, 0x01, 0x1D, 0x00, 0xBC,
	0x52, 0xD0, 0x1E, 0x20,
0xB8, 0x28, 0x55, 0x40, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F, 0x01, 0x1D,
	0x80, 0xD0, 0x72, 0x1C,
0x16, 0x20, 0x10, 0x2C, 0x25, 0x80, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x9F,
	0x8C, 0x0A, 0xD0, 0x8A,
0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0x13, 0x8E, 0x21, 0x00,
	0x00, 0x18, 0x00, 0xAB
};

static unsigned char amlogic_hdmirx_edid_port2[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x05, 0xAC, 0x30, 0x00,
	0x01, 0x00, 0x00, 0x00,
0x0A, 0x18, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78, 0x0A, 0xCF, 0x74, 0xA3,
	0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0xFF, 0x01, 0xFF,
	0xFF, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x20, 0x08, 0xE8, 0x00, 0x30, 0xF2, 0x70,
	0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E, 0x02, 0x3A, 0x80, 0x18,
	0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E, 0x00, 0x00,
	0x00, 0xFC, 0x00, 0x41,
0x4D, 0x4C, 0x20, 0x38, 0x36, 0x36, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x01, 0xAF,
0x02, 0x03, 0x36, 0xF0, 0x5A, 0x1F, 0x10, 0x14, 0x05, 0x13, 0x04, 0x20,
	0x22, 0x3C, 0x3E, 0x12,
0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06, 0x01, 0x5F, 0x5D, 0x60, 0x61,
	0x64, 0x65, 0x66, 0x23,
0x09, 0x07, 0x01, 0x83, 0x01, 0x00, 0x00, 0x6E, 0x03, 0x0C, 0x00, 0x20,
	0x00, 0x88, 0x3C, 0x20,
0x80, 0x80, 0x01, 0x02, 0x03, 0x04, 0x02, 0x3A, 0x80, 0xD0, 0x72, 0x38,
	0x2D, 0x40, 0x10, 0x2C,
0x45, 0x80, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F, 0x01, 0x1D, 0x00, 0xBC,
	0x52, 0xD0, 0x1E, 0x20,
0xB8, 0x28, 0x55, 0x40, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F, 0x01, 0x1D,
	0x80, 0xD0, 0x72, 0x1C,
0x16, 0x20, 0x10, 0x2C, 0x25, 0x80, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x9F,
	0x8C, 0x0A, 0xD0, 0x8A,
0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0x13, 0x8E, 0x21, 0x00,
	0x00, 0x18, 0x00, 0x9B
};

static unsigned char amlogic_hdmirx_edid_port3[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x05, 0xAC, 0x30, 0x00,
	0x01, 0x00, 0x00, 0x00,
0x0A, 0x18, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78, 0x0A, 0xCF, 0x74, 0xA3,
	0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0xFF, 0x01, 0xFF,
	0xFF, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x20, 0x08, 0xE8, 0x00, 0x30, 0xF2, 0x70,
	0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E, 0x02, 0x3A, 0x80, 0x18,
	0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E, 0x00, 0x00,
	0x00, 0xFC, 0x00, 0x41,
0x4D, 0x4C, 0x20, 0x38, 0x36, 0x36, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x01, 0xAF,
0x02, 0x03, 0x36, 0xF0, 0x5A, 0x1F, 0x10, 0x14, 0x05, 0x13, 0x04, 0x20,
	0x22, 0x3C, 0x3E, 0x12,
0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06, 0x01, 0x5F, 0x5D, 0x60, 0x61,
	0x64, 0x65, 0x66, 0x23,
0x09, 0x07, 0x01, 0x83, 0x01, 0x00, 0x00, 0x6E, 0x03, 0x0C, 0x00, 0x30,
	0x00, 0x88, 0x3C, 0x20,
0x80, 0x80, 0x01, 0x02, 0x03, 0x04, 0x02, 0x3A, 0x80, 0xD0, 0x72, 0x38,
	0x2D, 0x40, 0x10, 0x2C,
0x45, 0x80, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F, 0x01, 0x1D, 0x00, 0xBC,
	0x52, 0xD0, 0x1E, 0x20,
0xB8, 0x28, 0x55, 0x40, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F, 0x01, 0x1D,
	0x80, 0xD0, 0x72, 0x1C,
0x16, 0x20, 0x10, 0x2C, 0x25, 0x80, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x9F,
	0x8C, 0x0A, 0xD0, 0x8A,
0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0x13, 0x8E, 0x21, 0x00,
	0x00, 0x18, 0x00, 0x8B
};

/* test EDID skyworth mst/mtk */
static unsigned char hdmirx_test_edid_port_skyworth_mstmtk[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x4D, 0x77, 0x01, 0x00,
	0x01, 0x00, 0x00, 0x00,
0x1C, 0x16, 0x01, 0x03, 0x80, 0x3C, 0x22, 0x78, 0x0A, 0x0D, 0xC9, 0xA0,
	0x57, 0x47, 0x98, 0x27,
0x12, 0x48, 0x4C, 0xBF, 0xEF, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1D, 0x00, 0x72, 0x51, 0xD0,
	0x1E, 0x20, 0x6E, 0x28,
0x55, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E, 0x01, 0x1D, 0x80, 0x18,
	0x71, 0x1C, 0x16, 0x20,
0x58, 0x2C, 0x25, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x9E, 0x00, 0x00,
	0x00, 0xFC, 0x00, 0x20,
0x38, 0x4B, 0x35, 0x35, 0x20, 0x20, 0x20, 0x20, 0x4C, 0x45, 0x44, 0x0A,
	0x00, 0x00, 0x00, 0xFD,
0x00, 0x31, 0x4C, 0x0F, 0x50, 0x0E, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x01, 0xBB,
0x02, 0x03, 0x29, 0x74, 0x4B, 0x84, 0x10, 0x1F, 0x05, 0x13, 0x14, 0x01,
	0x02, 0x11, 0x06, 0x15,
0x26, 0x09, 0x7F, 0x03, 0x11, 0x7F, 0x18, 0x83, 0x01, 0x00, 0x00, 0x6D,
	0x03, 0x0C, 0x00, 0x10,
0x00, 0xB8, 0x3C, 0x2F, 0x80, 0x60, 0x01, 0x02, 0x03, 0x01, 0x1D, 0x00,
	0xBC, 0x52, 0xD0, 0x1E,
0x20, 0xB8, 0x28, 0x55, 0x40, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E, 0x01,
	0x1D, 0x80, 0xD0, 0x72,
0x1C, 0x16, 0x20, 0x10, 0x2C, 0x25, 0x80, 0xC4, 0x8E, 0x21, 0x00, 0x00,
	0x9E, 0x8C, 0x0A, 0xD0,
0x8A, 0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0x13, 0x8E, 0x21,
	0x00, 0x00, 0x18, 0x8C,
0x0A, 0xD0, 0x90, 0x20, 0x40, 0x31, 0x20, 0x0C, 0x40, 0x55, 0x00, 0x13,
	0x8E, 0x21, 0x00, 0x00,
0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x22
};

static unsigned char hdmirx_test_edid_4K_MSD[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x36, 0x74, 0x30, 0x00,
	0x01, 0x00, 0x00, 0x00,
0x0A, 0x18, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78, 0x0A, 0xCF, 0x74, 0xA3,
	0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0xFF, 0x01, 0xFF,
	0xFF, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x20, 0x08, 0xE8, 0x00, 0x30, 0xF2, 0x70,
	0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E, 0x02, 0x3A, 0x80, 0x18,
	0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0xC4, 0x8E, 0x21, 0x00, 0x00, 0x1E, 0x00, 0x00,
	0x00, 0xFC, 0x00, 0x4D,
0x53, 0x74, 0x61, 0x72, 0x20, 0x44, 0x65, 0x6D, 0x6F, 0x0A, 0x20, 0x20,
	0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x01, 0x68,
0x02, 0x03, 0x4C, 0xF2, 0x5B, 0x05, 0x84, 0x03, 0x01, 0x12, 0x13, 0x14,
	0x16, 0x07, 0x90, 0x1F,
0x20, 0x22, 0x5D, 0x5F, 0x60, 0x61, 0x62, 0x64, 0x65, 0x66, 0x5E, 0x63,
	0x02, 0x06, 0x11, 0x15,
0x26, 0x09, 0x07, 0x07, 0x11, 0x07, 0x06, 0x83, 0x01, 0x00, 0x00, 0x6E,
	0x03, 0x0C, 0x00, 0x10,
0x00, 0x78, 0x44, 0x20, 0x00, 0x80, 0x01, 0x02, 0x03, 0x04, 0x67, 0xD8,
	0x5D, 0xC4, 0x01, 0x78,
0xC8, 0x07, 0xE3, 0x05, 0x03, 0x01, 0xE5, 0x0F, 0x00, 0x80, 0x19, 0x00,
	0x8C, 0x0A, 0xD0, 0x8A,
0x20, 0xE0, 0x2D, 0x10, 0x10, 0x3E, 0x96, 0x00, 0xC4, 0x8E, 0x21, 0x00,
	0x00, 0x18, 0x8C, 0x0A,
0xA0, 0x14, 0x51, 0xF0, 0x16, 0x00, 0x26, 0x7C, 0x43, 0x00, 0xC4, 0x8E,
	0x21, 0x00, 0x00, 0x98,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x71
};

/* AML EDID JIASHI 15.06.30 */
static unsigned char hdmirx_aml_edid_domy_port1[] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x11, 0xB9, 0x30, 0x00,
	0x01, 0x00, 0x00, 0x00,
0x20, 0x19, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78, 0x0A, 0xCF, 0x74, 0xA3,
	0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x08, 0xE8, 0x00, 0x30, 0xF2, 0x70,
	0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E, 0x02, 0x3A, 0x80, 0x18,
	0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E, 0x00, 0x00,
	0x00, 0xFC, 0x00, 0x44,
0x6F, 0x6D, 0x79, 0x20, 0x54, 0x56, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x01, 0xD3,
0x02, 0x03, 0x35, 0xF0, 0x5A, 0x61, 0x60, 0x10, 0x1F, 0x14, 0x05, 0x13,
	0x04, 0x20, 0x22, 0x3C,
0x3E, 0x12, 0x16, 0x03, 0x07, 0x11, 0x15, 0x02, 0x06, 0x01, 0x5F, 0x5D,
	0x64, 0x65, 0x66, 0x29,
0x09, 0x07, 0x01, 0x15, 0x07, 0x00, 0x57, 0x06, 0x00, 0x83, 0x01, 0x00,
	0x00, 0x67, 0x03, 0x0C,
0x00, 0x10, 0x00, 0x88, 0x3C, 0x02, 0x3A, 0x80, 0xD0, 0x72, 0x38, 0x2D,
	0x40, 0x10, 0x2C, 0x45,
0x80, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F, 0x01, 0x1D, 0x00, 0xBC, 0x52,
	0xD0, 0x1E, 0x20, 0xB8,
0x28, 0x55, 0x40, 0x30, 0xEB, 0x52, 0x00, 0x00, 0x1F, 0x8C, 0x0A, 0xD0,
	0x8A, 0x20, 0xE0, 0x2D,
0x10, 0x10, 0x3E, 0x96, 0x00, 0x13, 0x8E, 0x21, 0x00, 0x00, 0x18, 0x00,
	0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x7D
};

int hdmi_rx_ctrl_edid_update(void)
{
	int i, ram_addr, byte_num;
	unsigned char value = 0;
	/* unsigned int data32 = 0; */
	unsigned char check_sum;
	/* unsigned long tmp32; */

	if (((edid_mode & 0x100) == 0) && edid_size > 4 && edid_buf[0] == 'E'
	    && edid_buf[1] == 'D' && edid_buf[2] == 'I' && edid_buf[3] == 'D') {
		rx_print("edid: use Top edid\n");
		check_sum = 0;
		for (i = 0; i < (edid_size - 4); i++) {
			value = edid_buf[i + 4];
			if (((i + 1) & 0x7f) != 0) {
				check_sum += value;
				check_sum &= 0xff;
			} else {
				if (value != ((0x100 - check_sum) & 0xff)) {
					rx_print("HDMIRX: origin edid[%d]",
							i);
					rx_print("checksum %x incorrect,",
							value);
					rx_print("change to %x\n",
						(0x100 - check_sum) & 0xff);
				}
				value = (0x100 - check_sum) & 0xff;
				check_sum = 0;
			}
			ram_addr = TOP_EDID_OFFSET + i;
			hdmirx_wr_top(ram_addr, value);
		}
	} else if (((edid_mode & 0x100) == 0) && (pEdid_buffer != NULL)) {
		unsigned char *p_edid_array;
		int array_offset = 0;
		if (rx.port >= 1)
			array_offset = 256 * (rx.port - 1);
		p_edid_array = pEdid_buffer;
		rx_print("edid: use Bsp edid\n");
		/*recalculate check sum */
		for (check_sum = 0, i = 0; i < 127; i++) {
			check_sum += p_edid_array[i + array_offset];
			check_sum &= 0xff;
		}
		p_edid_array[127 + array_offset] = (0x100 - check_sum) & 0xff;

		for (check_sum = 0, i = 128; i < 255; i++) {
			check_sum += p_edid_array[i + array_offset];
			check_sum &= 0xff;
		}
		p_edid_array[255 + array_offset] = (0x100 - check_sum) & 0xff;
		 /**/ for (i = 0; i < 256; i++) {
			value = p_edid_array[i + array_offset];
			ram_addr = TOP_EDID_OFFSET + i;
			hdmirx_wr_top(ram_addr, value);
		}
	} else {
		if ((edid_mode & 0xf) == 0) {
			unsigned char *p_edid_array;
			byte_num =
			    sizeof(amlogic_hdmirx_edid_port1) /
			    sizeof(unsigned char);
			p_edid_array = amlogic_hdmirx_edid_port1;
			if (multi_port_edid_enable == 0) {
				if (rx.port == 1) {
					byte_num =
					    sizeof(amlogic_hdmirx_edid_port2) /
					    sizeof(unsigned char);
					p_edid_array =
					    amlogic_hdmirx_edid_port2;
				} else if (rx.port == 2) {
					byte_num =
					    sizeof(amlogic_hdmirx_edid_port3) /
					    sizeof(unsigned char);
					p_edid_array =
					    amlogic_hdmirx_edid_port3;
				} else if (rx.port == 3) {
					byte_num =
					    sizeof(amlogic_hdmirx_edid_port3) /
					    sizeof(unsigned char);
					p_edid_array =
					    amlogic_hdmirx_edid_port3;
				}
			}
#if 1
			/*recalculate check sum */
			for (check_sum = 0, i = 0; i < 127; i++) {
				check_sum += p_edid_array[i];
				check_sum &= 0xff;
			}
			p_edid_array[127] = (0x100 - check_sum) & 0xff;

			for (check_sum = 0, i = 128; i < 255; i++) {
				check_sum += p_edid_array[i];
				check_sum &= 0xff;
			}
			p_edid_array[255] = (0x100 - check_sum) & 0xff;
			 /**/
#endif
			for (i = 0; i < byte_num; i++) {
				value = p_edid_array[i];
				ram_addr = TOP_EDID_OFFSET + i;
				hdmirx_wr_top(ram_addr, value);
			}
		} else if ((edid_mode & 0xf) == 6) {
			byte_num =
			    sizeof(hdmirx_test_edid_port_skyworth_mstmtk) /
			    sizeof(unsigned char);
			for (i = 0; i < byte_num; i++) {
				value =
				    hdmirx_test_edid_port_skyworth_mstmtk[i];
				ram_addr = TOP_EDID_OFFSET + i;
				hdmirx_wr_top(ram_addr, value);
			}
		} else if ((edid_mode & 0xf) == 8) {
			byte_num =
			    sizeof(hdmirx_test_edid_4K_MSD) /
			    sizeof(unsigned char);
			for (i = 0; i < byte_num; i++) {
				value = hdmirx_test_edid_4K_MSD[i];
				ram_addr = TOP_EDID_OFFSET + i;
				hdmirx_wr_top(ram_addr, value);
			}
		} else if ((edid_mode & 0xf) == 9) {
			byte_num =
			    sizeof(hdmirx_aml_edid_domy_port1) /
			    sizeof(unsigned char);
			for (i = 0; i < byte_num; i++) {
				value = hdmirx_aml_edid_domy_port1[i];
				ram_addr = TOP_EDID_OFFSET + i;
				hdmirx_wr_top(ram_addr, value);
			}
		}
	}

/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
	/* hdmirx_wr_top(TOP_EDID_ADDR_CEC,       0x00a600a5); */
	if (is_meson_gxtvbb_cpu()) {
		if ((multi_port_edid_enable) && (edid_mode == 0)) {
			hdmirx_wr_top(TOP_EDID_RAM_OVR1,
				      0xAB | (0x0f << 16));
			hdmirx_wr_top(TOP_EDID_RAM_OVR1_DATA,
				      0x10 | 0x20 << 8 | 0x30 << 16);

			hdmirx_wr_top(TOP_EDID_RAM_OVR0,
				      0xff | (0x0f << 16));
			hdmirx_wr_top(TOP_EDID_RAM_OVR0_DATA,
				      0xAB | 0x9B << 8 | 0x8B << 16);
		} else if ((multi_port_edid_enable) &&
			((edid_mode & 0xf) == 9)) {
			hdmirx_wr_top(TOP_EDID_RAM_OVR1,
				      0xB1 | (0x0f << 16));
			hdmirx_wr_top(TOP_EDID_RAM_OVR1_DATA,
				      0x10 | 0x20 << 8 | 0x30 << 16);

			hdmirx_wr_top(TOP_EDID_RAM_OVR0,
				      0xff | (0x0f << 16));
			hdmirx_wr_top(TOP_EDID_RAM_OVR0_DATA,
				      0x7D | 0x6D << 8 | 0x5D << 16);
		}
	}
/* #endif */

	return 0;
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
		rx_print("Error: %s,key size %d",
				__func__, key_size);
		rx_print("is larger than the buf size of %d\n", max_size);
		return 0;
	}
	memcpy(buf, key_buf, key_size);
	rx_print("HDMIRX: read key buf\n");
	return key_size;
}

void hdmirx_fill_key_buf(const char *buf, int size)
{
	if (size > MAX_KEY_BUF_SIZE) {
		rx_print("Error: %s,key size %d",
				__func__,
				size);
		rx_print("is larger than the max size of %d\n",
			MAX_KEY_BUF_SIZE);
		return;
	}
	if (buf[0] == 'k' && buf[1] == 'e' && buf[2] == 'y') {
		set_hdcp(&init_hdcp_data, buf + 3);
	} else {
		memcpy(key_buf, buf, size);
		key_size = size;
		rx_print("HDMIRX: fill key buf, size %d\n", size);
	}
}

int hdmirx_read_edid_buf(char *buf, int max_size)
{
	if (edid_size > max_size) {
		rx_print("Error: %s,edid size %d",
				__func__,
				edid_size);
		rx_print(" is larger than the buf size of %d\n",
			max_size);
		return 0;
	}
	memcpy(buf, edid_buf, edid_size);
	rx_print("HDMIRX: read edid buf\n");
	return edid_size;
}

void hdmirx_fill_edid_buf(const char *buf, int size)
{
	if (size > MAX_EDID_BUF_SIZE) {
		rx_print("Error: %s,edid size %d",
				__func__,
				size);
		rx_print(" is larger than the max size of %d\n",
			MAX_EDID_BUF_SIZE);
		return;
	}
	memcpy(edid_buf, buf, size);
	edid_size = size;
	rx_print("HDMIRX: fill edid buf, size %d\n",
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
	int error = 0;
	/* int i = 0; */
	struct hdmi_rx_ctrl_video v;
	static struct aud_info_s a;

	if (enable & 1) {
		hdmirx_get_video_info(&rx.ctrl, &v);
		rx_print("[HDMI info]error %d", error);
		rx_print("video_format %d,", v.video_format);
		rx_print("VIC %d dvi %d", v.video_mode, v.dvi);
		rx_print("interlace %d\n", v.interlaced);
		rx_print(" htotal %d", v.htotal);
		rx_print(" hactive %d", v.hactive);
		rx_print(" vtotal %d", v.vtotal);
		rx_print(" vactive %d", v.vactive);
		rx_print(" pixel_repetition %d\n", v.pixel_repetition);

		rx_print(" deep_color %d", v.deep_color_mode);
		rx_print(" refresh_rate %d\n", v.refresh_rate);

	}
	if (enable & 2) {
		hdmirx_read_audio_info(&a);
		rx_print("AudioInfo:");
		rx_print(" CT=%u CC=%u",
				a.coding_type,
				a.channel_count);
		rx_print(" SF=%u SS=%u",
				a.sample_frequency,
				a.sample_size);
		rx_print(" CA=%u",
			a.channel_allocation);

		rx_print(" CTS=%d, N=%d,",
				a.cts, a.n);
		rx_print("recovery clock is %d\n",
			a.arc);
	}
	rx_print("TMDS clock = %d,",
			hdmirx_get_tmds_clock());
	rx_print("Pixel clock = %d\n",
		hdmirx_get_pixel_clock());

	rx_print("rx.no_signal=%d,rx.state=%d,",
			rx.no_signal,
			rx.state);
	rx_print("fmt=0x%x,sw_vic:%d,",
			hdmirx_hw_get_fmt(),
			rx.pre_video_params.sw_vic);
	rx_print("sw_dvi:%d,sw_fp:%d,",
			rx.pre_video_params.sw_dvi,
			rx.pre_video_params.sw_fp);
	rx_print("sw_alternative:%d\n",
		rx.pre_video_params.sw_alternative);

	rx_print("HDCP debug value=0x%x\n",
		hdmirx_rd_dwc(DWC_HDCP_DBG));
	rx_print("HDCP encrypted state:%d\n",
		v.hdcp_enc_state);
}

static void dump_audio_info(unsigned char enable)
{
	static struct aud_info_s a;

	if (enable) {
		hdmirx_read_audio_info(&a);
		rx_print("AudioInfo: CT=%u",
				a.coding_type);
		rx_print(" CC=%u SF=%u SS=%u CA=%u",
			a.channel_count,
			a.sample_frequency,
			a.sample_size,
			a.channel_allocation);
		rx_print(" [hdmirx]CTS=%d, N=%d,",
				a.cts,
				a.n);
		rx_print("recovery clock is %d\n",
			a.arc);
	}
}

void dump_reg(void)
{
	int i = 0;

	rx_print("\n***Top registers***\n");
	rx_print("[addr ]  addr + 0x0,");
	rx_print("addr + 0x1,  addr + 0x2,  addr + 0x3\n");
	for (i = 0; i <= 0x24;) {
		rx_print("[0x%-3x]", i);
		rx_print("0x%-8x" , hdmirx_rd_top(i));
		rx_print("0x%-8x,0x%-8x,0x%-8x\n",
			hdmirx_rd_top(i + 1),
			hdmirx_rd_top(i + 2),
			hdmirx_rd_top(i + 3));
		i = i + 4;
	}
	#if 0
	rx_print("[0x%-3x]  0x%-8x,", i,
		       hdmirx_rd_top(i));
	rx_print("0x%-8x,  0x%-8x\n",
	       hdmirx_rd_top(i + 1),
	       hdmirx_rd_top(i + 2));
	rx_print("0x60 : [0x%-3x]\n",
		hdmirx_rd_top(0x60));
	rx_print("\n***EDID data***\n");
	rx_print("[addr ]  addr + 0x0,");
	rx_print("addr + 0x1,  addr + 0x2,");
	rx_print("addr + 0x3\n");
	for (i = 0; i < 256;) {
		rx_print("[0x%-3x]", i);
		rx_print("0x%-8x", hdmirx_rd_top(TOP_EDID_OFFSET + i));
		rx_print("0x%-8x,0x%-8x,0x%-8x\n",
			hdmirx_rd_top(TOP_EDID_OFFSET + i + 1),
			hdmirx_rd_top(TOP_EDID_OFFSET + i + 2),
			hdmirx_rd_top(TOP_EDID_OFFSET + i + 3));
		i = i + 4;
	}
	#endif
	rx_print("\n***PHY registers***\n");
	rx_print("[addr ]  addr + 0x0,");
	rx_print("addr + 0x1,  addr + 0x2,");
	rx_print("addr + 0x3\n");
	for (i = 0; i <= 0x9a;) {
		rx_print("[0x%-3x]", i);
		rx_print("0x%-8x", hdmirx_rd_phy(i));
		rx_print("0x%-8x,0x%-8x,0x%-8x\n",
			hdmirx_rd_phy(i + 1),
			hdmirx_rd_phy(i + 2),
			hdmirx_rd_phy(i + 3));
		i = i + 4;
	}
	rx_print("\n**Controller registers**\n");
	rx_print("[addr ]  addr + 0x0,");
	rx_print("addr + 0x4,  addr + 0x8,");
	rx_print("addr + 0xc\n");
	for (i = 0; i <= 0xffc;) {
		rx_print("[0x%-3x]", i);
		rx_print("0x%-8x", hdmirx_rd_dwc(i));
		rx_print("0x%-8x", hdmirx_rd_dwc(i + 1));
		rx_print("0x%-8x", hdmirx_rd_dwc(i + 2));
		rx_print("0x%-8x\n", hdmirx_rd_dwc(i + 3));

		i = i + 16;
	}

}

void dump_hdcp_data(void)
{
	int i = 0;
	rx_print("\n*************HDCP");
	rx_print("***************");
	rx_print("\n hdcp-seed = %d ",
		rx.hdcp.seed);
	/* KSV CONFIDENTIAL */
	rx_print("\n hdcp-ksv = %x---%x",
		rx.hdcp.bksv[0],
		rx.hdcp.bksv[1]);
	rx_print("\n hdcp-key:");
	for (i = 0; i < HDCP_KEYS_SIZE; i += 2) {
		rx_print("\n%x    %x",
			rx.hdcp.keys[i],
			rx.hdcp.keys[i + 1]);
	}
	rx_print("\n*************HDCP");
}

void dump_edid_reg(void)
{
	int i = 0;
	int j = 0;
	rx_print("\n***********************\n");
	rx_print("0x107 enable rgb range block\n");
	rx_print("0x106 skyworth mst_or_mtk edid\n");
	rx_print("0x105 mst sharp porduction edid\n");
	rx_print("0x104 mst ATSC production edid\n");
	rx_print("0x103 aml old edid, 4k*2k unsupported\n");
	rx_print("********************************\n");
	/* 1024 = 64*16 */
	for (i = 0; i < 16; i++) {
		rx_print("[%2d] ", i);
		for (j = 0; j < 16; j++) {
			rx_print("0x%02lx, ",
			       hdmirx_rd_top(TOP_EDID_OFFSET +
					     (i * 16 + j)));
		}
		rx_print("\n");
	}
}

void timer_state(void)
{
	rx_print("timer state:");
	switch (rx.state) {
	case HDMIRX_HWSTATE_INIT:
		rx_print("HWSTATE_INIT\n");
		break;
	case HDMIRX_HWSTATE_HDMI5V_LOW:
		rx_print("HDMIRX_HWSTATE_HDMI5V_LOW\n");
		break;
	case HDMIRX_HWSTATE_TIMINGCHANGE:
		rx_print("HDMIRX_HWSTATE_TIMINGCHANGE\n");
		break;
	case HDMIRX_HWSTATE_SIG_UNSTABLE:
		rx_print("HDMIRX_HWSTATE_SIG_UNSTABLE\n");
		break;
	case HDMIRX_HWSTATE_SIG_STABLE:
		rx_print("HDMIRX_HWSTATE_SIG_STABLE\n");
		break;
	case HDMIRX_HWSTATE_SIG_READY:
		rx_print("HDMIRX_HWSTATE_SIG_READY\n");
		break;
	}
}

int hdmirx_debug(const char *buf, int size)
{
	char tmpbuf[128];
	int i = 0;
	long adr;
	long value = 0;

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;
	if (strncmp(tmpbuf, "hpd", 3) == 0)
		hdmirx_set_hpd(rx.port, tmpbuf[3] == '0' ? 0 : 1);
	else if (strncmp(tmpbuf, "cable_status", 12) == 0) {
		size = hdmirx_rd_top(TOP_HPD_PWR5V) >> 20;
		rx_print("cable_status = %x\n", size);
	} else if (strncmp(tmpbuf, "signal_status", 13) == 0) {
		size = rx.no_signal;
		rx_print("signal_status = %d\n", size);
	} else if (strncmp(tmpbuf, "input_mode", 10) == 0) {
		size = rx.pre_video_params.sw_vic;
		rx_print("input_mode = %d", size);
	}
#ifdef CEC_FUNC_ENABLE
	else if (strncmp(tmpbuf, "cec", 3) == 0) {
		if (tmpbuf[3] == '0')
			cec_state(0);
		else if (tmpbuf[3] == '1')
			cec_state(1);
		else if (tmpbuf[3] == '2')
			/* clean_cec_message(); */
		else if (tmpbuf[3] == '3')
			dump_cec_message(tmpbuf[4] - '0');
		else if (tmpbuf[3] == '5')
			cec_dump_dev_map();
	}
#endif
	else if (strncmp(tmpbuf, "reset", 5) == 0) {
		if (tmpbuf[5] == '0') {
			rx_print(" hdmirx hw config\n");
			hdmirx_hw_config();
			/* hdmi_rx_ctrl_edid_update(); */
			/* hdmirx_config_video(&rx.video_params); */
			/* hdmirx_config_audio(); */
		} else if (tmpbuf[5] == '1') {
			rx_print(" hdmirx phy init 8bit\n");
			hdmirx_phy_init(rx.port, 0);
		} else if (tmpbuf[5] == '4') {
			rx_print(" edid update\n");
			hdmi_rx_ctrl_edid_update();
		} else if (tmpbuf[5] == '2') {
			rx_print(" hdmirx phy init 10bit\n");
			hdmirx_phy_init(rx.port, 1);
		} else if (tmpbuf[5] == '3') {
			rx_print(" hdmirx phy init 12bit\n");
			hdmirx_phy_init(rx.port, 2);
		} else if (strncmp(tmpbuf + 5, "_on", 3) == 0) {
			reset_sw = 1;
			rx_print("reset on!\n");
		} else if (strncmp(tmpbuf + 5, "_off", 4) == 0) {
			reset_sw = 0;
			rx_print(" reset off!\n");
		}
	} else if (strncmp(tmpbuf, "state", 5) == 0) {
		dump_state(0xff);
	} else if (strncmp(tmpbuf, "pause", 5) == 0) {
		if (kstrtol(tmpbuf + 5, 10, &value) < 0)
			return -EINVAL;
		rx_print("%s the state machine\n",
			value ? "pause" : "enable");
		sm_pause = value;
	} else if (strncmp(tmpbuf, "reg", 3) == 0) {
		dump_reg();
	} else if (strncmp(tmpbuf, "edid", 4) == 0) {
		dump_edid_reg();
	} else if (strncmp(tmpbuf, "hdcp123", 7) == 0) {
		dump_hdcp_data();
	} else if (strncmp(tmpbuf, "loadkey", 7) == 0) {
		rx_print("load hdcp key\n");
		memcpy(&rx.hdcp, &init_hdcp_data,
		       sizeof(struct hdmi_rx_ctrl_hdcp));
		hdmirx_hw_config();
	} else if (strncmp(tmpbuf, "timer_state", 11) == 0) {
		timer_state();
	} else if (strncmp(tmpbuf, "clock", 5) == 0) {
		if (kstrtol(tmpbuf + 5, 10, &value) < 0)
			return -EINVAL;
		rx_print("clock[%d] = %d\n",
			value, hdmirx_get_clock(value));
	} else if (strncmp(tmpbuf, "sample_rate", 11) == 0) {
		/* nothing */
	} else if (strncmp(tmpbuf, "prbs", 4) == 0) {
		/* nothing */
	} else if (tmpbuf[0] == 'w') {
		if (kstrtol(tmpbuf + 3, 16, &adr) < 0)
				return -EINVAL;
		rx_print("adr = %x\n", adr);
		if (kstrtol(buf + i + 1, 16, &value) < 0)
			return -EINVAL;
		rx_print("value = %x\n", value);
		if (tmpbuf[1] == 'h') {
			if (buf[2] == 't') {
				hdmirx_wr_top(adr, value);
				rx_print("write %x to TOP [%x]\n",
					value, adr);
			} else if (buf[2] == 'd') {
				hdmirx_wr_dwc(adr, value);
				rx_print("write %x to DWC [%x]\n",
					value, adr);
			} else if (buf[2] == 'p') {
				hdmirx_wr_phy(adr, value);
				rx_print("write %x to PHY [%x]\n",
					value, adr);
			}
		} else if (buf[1] == 'c') {
			aml_write_cbus(adr, value);
			rx_print("write %x to CBUS [%x]\n", value, adr);
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
				rx_print("TOP [%x]=%x\n", adr, value);
			} else if (tmpbuf[2] == 'd') {
				value = hdmirx_rd_dwc(adr);
				rx_print("DWC [%x]=%x\n", adr, value);
			} else if (tmpbuf[2] == 'p') {
				value = hdmirx_rd_phy(adr);
				rx_print("PHY [%x]=%x\n", adr, value);
			}
		} else if (buf[1] == 'c') {
			/* value = READ_MPEG_REG(adr); */
			rx_print("CBUS reg[%x]=%x\n", adr, value);
		} else if (buf[1] == 'p') {
			/* value = READ_APB_REG(adr); */
			rx_print("APB reg[%x]=%x\n", adr, value);
		} else if (buf[1] == 'l') {
			/* aml_write_cbus(MDB_CTRL, 2); */
			/* aml_write_cbus(MDB_ADDR_REG, adr); */
			/* value = READ_MPEG_REG(MDB_DATA_REG); */
			rx_print("LMEM[%x]=%x\n", adr, value);
		} else if (buf[1] == 'r') {
			/* aml_write_cbus(MDB_CTRL, 1); */
			/* aml_write_cbus(MDB_ADDR_REG, adr); */
			/* value = READ_MPEG_REG(MDB_DATA_REG); */
			rx_print("amrisc reg[%x]=%x\n", adr, value);
		}
	} else if (tmpbuf[0] == 'v') {
		rx_print("------------------\n");
		rx_print("Hdmirx version: %s\n", HDMIRX_VER);
		rx_print("------------------\n");
	}
	return 0;
}

void to_init_state(void)
{
	if (sm_pause)
		return;
}

#if 0
void hdmirx_hw_init2(void)
{
	if (sm_pause)
		return;


	memset(&rx, 0, sizeof(struct rx));
	memset(&rx.pre_video_params, 0, sizeof(struct hdmi_rx_ctrl_video));
	memcpy(&rx.hdcp, &init_hdcp_data, sizeof(struct hdmi_rx_ctrl_hdcp));

	rx.phy.cfg_clk = cfg_clk;
	rx.phy.lock_thres = lock_thres;
	rx.phy.fast_switching = fast_switching;
	rx.phy.fsm_enhancement = fsm_enhancement;
	rx.phy.port_select_ovr_en = port_select_ovr_en;
	rx.phy.phy_cmu_config_force_val = phy_cmu_config_force_val;
	rx.phy.phy_system_config_force_val = phy_system_config_force_val;
	rx.ctrl.md_clk = 24000;
	rx.ctrl.tmds_clk = 0;
	rx.ctrl.tmds_clk2 = 0;
	rx.ctrl.acr_mode = acr_mode;

	hdmirx_set_pinmux();
	/* hdmirx_set_hpd(rx.port, 0); */

	rx_print("%s %d\n", __func__, rx.port);

}
#endif

const unsigned int test_keys[40 * 2] = {

	0xa2221e, 0x8fc4a2fa, 0x19e77f, 0xd6fe98ce,
	0xe759a7, 0x27b0265a, 0xc0a1ba, 0xfccd5c1c,
	0x779a27, 0xbc220ad4, 0xa8f3b3, 0x7ed4a95d,
	0x21158e, 0x124c8b07, 0x8d8330, 0x145b62fe,
	0x45fcf, 0xf4fd1bbb, 0x3afdf8, 0x2e9e537d,
	0x5c057b, 0xeed1d013, 0x8380d7, 0xa0f06107,
	0x6474d2, 0xf268ab23, 0xdce3ba, 0x5680e63c,
	0xce7443, 0x39cac73d, 0x5d14bc, 0x5e7ed7df,
	0xf98228, 0x3adb47ab, 0x864d64, 0x582b4b41,
	0x409fee, 0x69223242, 0x8351c5, 0x67330c4c,
	0x9d8be, 0x96a5ed5f, 0x402b8d, 0xa803a3aa,
	0xde5e24, 0x8bcbeb63, 0x2257e9, 0x28b1be70,
	0xf0d9b3, 0x72ee5eef, 0x442361, 0xaa778dbc,
	0x29c64e, 0x8006246e, 0x6e1780, 0x2efda8c4,
	0x5c663, 0x9241315d, 0x486e9c, 0xe47e790f,
	0xf60223, 0x435cc941, 0x1381a8, 0x56afa37f,
	0xb2e31a, 0xffeb9803, 0x8ad0e7, 0x5f27a9f1,
	0xbb62e2, 0x868f7adb, 0xd14f90, 0xe0671132,
	0xb936b5, 0xa489c126, 0x230984, 0x2485f5fe,
	0x146d62, 0x849be1e7, 0xabbf4, 0x1571f024
};

/***********************
    hdmirx_hw_init
    hdmirx_hw_uninit
    hdmirx_hw_enable
    hdmirx_hw_disable
    hdmirx_irq_init
*************************/

void hdmirx_hw_init(enum tvin_port_e port)
{
	if (sm_pause)
		return;


	memset(&rx, 0, sizeof(struct rx));
	/* memset(rx.pow5v_state, */
	/*	0, */
	/*	sizeof(rx.pow5v_state)); */
	memset(&rx.pre_video_params,
		0,
		sizeof(struct hdmi_rx_ctrl_video));
	if (use_test_hdcp_key) {
		rx.hdcp.seed = 0;
		rx.hdcp.repeat = 0;
		rx.hdcp.bksv[0] = 0x12;
		rx.hdcp.bksv[1] = 0xde3f4686;
		memcpy(&rx.hdcp.keys, test_keys, 80 * sizeof(uint32_t));
		rx_print("**NOTE--USE TEST HDCP KEY**\n");
	} else {
		memcpy(&rx.hdcp, &init_hdcp_data,
		       sizeof(struct hdmi_rx_ctrl_hdcp));
	}

	rx.phy.cfg_clk = cfg_clk;
	rx.phy.lock_thres = lock_thres;
	rx.phy.fast_switching = fast_switching;
	rx.phy.fsm_enhancement = fsm_enhancement;
	rx.phy.port_select_ovr_en = port_select_ovr_en;
	rx.phy.phy_cmu_config_force_val = phy_cmu_config_force_val;
	rx.phy.phy_system_config_force_val = phy_system_config_force_val;
	rx.ctrl.md_clk = 24000;
	rx.ctrl.tmds_clk = 0;
	rx.ctrl.tmds_clk2 = 0;
	rx.ctrl.acr_mode = acr_mode;

	rx.port = (port_map >> ((port - TVIN_PORT_HDMI0) << 2)) & 0xf;

	rx.portA_pow5v_state = 0;
	rx.portB_pow5v_state = 0;
	rx.portC_pow5v_state = 0;
	rx.portD_pow5v_state = 0;
	rx.portA_pow5v_state_pre = 0;
	rx.portB_pow5v_state_pre = 0;
	rx.portC_pow5v_state_pre = 0;
	/* hdmirx_set_pinmux(); */
	if (multi_port_edid_enable) {
		hdmirx_hw_config();
		hdmi_rx_ctrl_edid_update();
	}
	/* hdmirx_set_pinmux(); */
	/* hdmirx_default_hpd(1); */
	/* mdelay(30); */
	rx_print("%s %d\n", __func__, rx.port);

}

void hdmirx_hw_uninit(void)
{
	if (sm_pause)
		return;

	/* set all hpd low  */
	/* aml_write_cbus(PREG_PAD_GPIO5_O, */
	/* READ_CBUS_REG(PREG_PAD_GPIO5_O) | */
	/* ((1<<1)|(1<<5)|(1<<9)|(1<<13))); */

	if (!multi_port_edid_enable)
		hdmirx_set_hpd(rx.port, 0);

#ifndef CEC_FUNC_ENABLE
	hdmirx_wr_top(TOP_INTR_MASKN, 0);
	hdmirx_interrupts_cfg(false);
#endif
	audio_status_init();

	rx.ctrl.status = 0;
	rx.ctrl.tmds_clk = 0;
	/* ctx->bsp_reset(true); */

	hdmirx_phy_reset(true);
	hdmirx_phy_pddq(1);
}

void hdmirx_hw_enable(void)
{
	hdmirx_set_pinmux();
	hdmirx_hw_probe();
	hdmirx_default_hpd(1);
}

void hdmirx_hw_disable(unsigned char flag)
{
}

void hdmirx_default_hpd(bool high)
{
#if 0/* (< MESON_CPU_TYPE_MESONG9TV) */
	if (!high)
		aml_write_cbus(PREG_PAD_GPIO5_O,
			READ_CBUS_REG(PREG_PAD_GPIO5_O) | ((1 << 5) |
			(1 << 9) | (1
			<< 13)));
	else
		aml_write_cbus(PREG_PAD_GPIO5_O,
			READ_CBUS_REG(PREG_PAD_GPIO5_O) &
			(~((1 << 5) | (1 << 9) | (1 << 13))));
#endif
#if 0/* (MESON_CPU_TYPE_MESONG9TV) */
	if (!high)
		aml_write_cbus(PREG_PAD_GPIO0_O,
			READ_CBUS_REG(PREG_PAD_GPIO0_O) | ((1 << 5) |
			(1 << 9) | (1 << 13)));
	else
		aml_write_cbus(PREG_PAD_GPIO0_O,
			       READ_CBUS_REG(PREG_PAD_GPIO0_O) &
			       (~((1 << 5) | (1 << 9) | (1 << 13))));
#endif
#if 0/* (MESON_CPU_TYPE_MESONG9BB) */
	if (high)
		aml_write_cbus(PREG_PAD_GPIO0_EN_N,
			READ_CBUS_REG(PREG_PAD_GPIO0_EN_N) | ((1 << 5) |
			(1 << 9) |
			(1 << 13)));
	else
		aml_write_cbus(PREG_PAD_GPIO0_EN_N,
			       READ_CBUS_REG(PREG_PAD_GPIO0_EN_N) &
			       (~((1 << 5) | (1 << 9) | (1 << 13))));

#endif
	if (is_meson_gxtvbb_cpu())
		;
}

