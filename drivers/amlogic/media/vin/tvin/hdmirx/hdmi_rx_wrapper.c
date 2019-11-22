/*
 * drivers/amlogic/media/vin/tvin/hdmirx/hdmi_rx_wrapper.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/sound/hdmi_earc.h>

/* Local include */
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_eq.h"
#include "hdmi_rx_wrapper.h"
#include "hdmi_rx_pktinfo.h"
#include "hdmi_rx_edid.h"

static int pll_unlock_cnt;
static int pll_unlock_max = 30;

static int pll_lock_cnt;
static int pll_lock_max;

static int dwc_rst_wait_cnt;
static int dwc_rst_wait_cnt_max = 1;

static int sig_stable_cnt;
static int sig_stable_max = 10;

static int sig_stable_err_cnt;
static int sig_stable_err_max = 5;
static int max_err_cnt = 10;

static bool clk_debug;

static int hpd_wait_cnt;
/* increase time of hpd low, to avoid some source like */
/* MTK box/KaiboerH9 i2c communicate error */
static int hpd_wait_max = 40;

static int sig_unstable_cnt;
static int sig_unstable_max = 40;/* 80 */

bool vic_check_en;
bool dvi_check_en;

static int sig_unready_cnt;
static int sig_unready_max = 5;/* 10; */

static int pow5v_max_cnt = 3;

int rgb_quant_range;

int yuv_quant_range;

int it_content;

/* timing diff offset */
static int diff_pixel_th = 2;
static int diff_line_th = 10;
static int diff_frame_th = 40; /* (25hz-24hz)/2 = 50/100 */
static int err_dbg_cnt;
static int err_dbg_cnt_max = 500;
int force_vic;
uint32_t fsm_log_en;
uint32_t err_chk_en;

static int aud_sr_stb_max = 30;

/* used in other module */
static int audio_sample_rate;
module_param(audio_sample_rate, int, 0664);
MODULE_PARM_DESC(audio_sample_rate, "audio_sample_rate");

static int auds_rcv_sts;
module_param(auds_rcv_sts, int, 0664);
MODULE_PARM_DESC(auds_rcv_sts, "auds_rcv_sts");

static int audio_coding_type;
static int audio_channel_count;

int log_level = LOG_EN;/*| HDCP_LOG;*/

static bool auto_switch_off;	/* only for hardware test */

int clk_unstable_cnt;
static int clk_unstable_max;

int clk_stable_cnt;
static int clk_stable_max = 3;

static int unnormal_wait_max = 200;

static int wait_no_sig_max = 600;

/* to inform ESM whether the cable is connected or not */
bool hpd_to_esm;
MODULE_PARM_DESC(hpd_to_esm, "\n hpd_to_esm\n");
module_param(hpd_to_esm, bool, 0664);

bool hdcp22_kill_esm;
MODULE_PARM_DESC(hdcp22_kill_esm, "\n hdcp22_kill_esm\n");
module_param(hdcp22_kill_esm, bool, 0664);

bool hdcp_mode_sel;
MODULE_PARM_DESC(hdcp_mode_sel, "\n hdcp_mode_sel\n");
module_param(hdcp_mode_sel, bool, 0664);

static int hdcp22_capable_sts = 0xff;

bool esm_auth_fail_en;
MODULE_PARM_DESC(esm_auth_fail_en, "\n esm_auth_fail_en\n");
module_param(esm_auth_fail_en, bool, 0664);

/* to inform hdcp_rx22 whether there's any device connected */
bool pwr_sts_to_esm;

static int hdcp22_auth_sts = 0xff;

/*the esm reset flag for hdcp_rx22*/
bool esm_reset_flag;
MODULE_PARM_DESC(esm_reset_flag, "\n esm_reset_flag\n");
module_param(esm_reset_flag, bool, 0664);

/* to inform ESM whether the cable is connected or not */
bool video_stable_to_esm;
MODULE_PARM_DESC(video_stable_to_esm, "\n video_stable_to_esm\n");
module_param(video_stable_to_esm, bool, 0664);

bool enable_hdcp22_esm_log;
MODULE_PARM_DESC(enable_hdcp22_esm_log, "\n enable_hdcp22_esm_log\n");
module_param(enable_hdcp22_esm_log, bool, 0664);

bool esm_error_flag;
MODULE_PARM_DESC(esm_error_flag, "\n esm_error_flag\n");
module_param(esm_error_flag, bool, 0664);

bool hdcp22_esm_reset2;
MODULE_PARM_DESC(hdcp22_esm_reset2, "\n hdcp22_esm_reset2\n");
module_param(hdcp22_esm_reset2, bool, 0664);

bool hdcp22_stop_auth;
module_param(hdcp22_stop_auth, bool, 0664);
MODULE_PARM_DESC(hdcp22_stop_auth, "hdcp22_stop_auth");

int hdcp14_on;
MODULE_PARM_DESC(hdcp14_on, "\n hdcp14_on\n");
module_param(hdcp14_on, int, 0664);

/*esm recovery mode for changing resolution & hdmi2.0*/
int esm_recovery_mode = ESM_REC_MODE_TMDS;
module_param(esm_recovery_mode, int, 0664);
MODULE_PARM_DESC(esm_recovery_mode, "esm_recovery_mode");

int phy_retry_times = 1;
/* No need to judge  frame rate while checking timing stable,as there are
 * some out-spec sources whose framerate change a lot(e.g:59.7~60.16hz).
 * Other brands of tv can support this,we also need to support.
 */
static int stable_check_lvl;

/* If dvd source received the frequent pulses on HPD line,
 * It will sent a length of dirty audio data sometimes.it's TX's issues.
 * Compared with other brands TV, delay 1.5S to avoid this noise.
 */
static int edid_update_delay = 150;
int skip_frame_cnt = 1;
static bool hdcp22_reauth_enable;
unsigned int edid_update_flag;
unsigned int downstream_hpd_flag;
static bool hdcp22_stop_auth_enable;
static bool hdcp22_esm_reset2_enable;
int sm_pause;
int pre_port = 0xff;
static int hdcp_none_wait_max = 50;/* 100 */
static int esd_phy_rst_cnt;
static int esd_phy_rst_max;
static int cec_dev_info;
struct rx_s rx;
static bool term_flag = 1;

void hdmirx_init_params(void)
{
	if (rx.chip_id >= CHIP_ID_TL1) {
		clk_unstable_max = 10;
		esd_phy_rst_max = 80;
		stable_check_lvl = 0x7df;
		pll_lock_max = 5;
	} else {
		clk_unstable_max = 200;
		esd_phy_rst_max = 2;
		stable_check_lvl = 0x17df;
		pll_lock_max = 2;
	}
}

void rx_hpd_to_esm_handle(struct work_struct *work)
{
	cancel_delayed_work(&esm_dwork);
	/* switch_set_state(&rx.hpd_sdev, 0x0); */
	extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 0);
	rx_pr("esm_hpd-0\n");
	mdelay(80);
	/* switch_set_state(&rx.hpd_sdev, 0x01); */
	extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 1);
	rx_pr("esm_hpd-1\n");
}

int cec_set_dev_info(uint8_t dev_idx)
{
	cec_dev_info |= 1 << dev_idx;

	if (dev_idx == 1)
		hdcp_enc_mode = 1;

	return 0;
}
EXPORT_SYMBOL(cec_set_dev_info);

/*
 *func: irq tasklet
 *param: flag:
 *	0x01:	audio irq
 *	0x02:	packet irq
 */
void rx_tasklet_handler(unsigned long arg)
{
	struct rx_s *prx = (struct rx_s *)arg;
	uint8_t irq_flag = prx->irq_flag;

	prx->irq_flag = 0;

	/*rx_pr("irq_flag = 0x%x\n", irq_flag);*/
	if (irq_flag & IRQ_PACKET_FLAG)
		rx_pkt_handler(PKT_BUFF_SET_FIFO);

	/*pkt overflow or underflow*/
	if (irq_flag & IRQ_PACKET_ERR)
		hdmirx_packet_fifo_rst();

	if (irq_flag & IRQ_AUD_FLAG)
		hdmirx_audio_fifo_rst();

	/*irq_flag = 0;*/
}

static int hdmi_rx_ctrl_irq_handler(void)
{
	int error = 0;
	/* unsigned i = 0; */
	uint32_t intr_hdmi = 0;
	uint32_t intr_md = 0;
	uint32_t intr_pedc = 0;
	uint32_t intr_aud_cec = 0;
	/* uint32_t intr_aud_clk = 0; */
	uint32_t intr_aud_fifo = 0;
	uint32_t intr_hdcp22 = 0;
	bool vsi_handle_flag = false;
	bool drm_handle_flag = false;
	bool emp_handle_flag = false;
	uint32_t rx_top_intr_stat = 0;
	bool irq_need_clr = 0;

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

	intr_aud_fifo =
	    hdmirx_rd_dwc(DWC_AUD_FIFO_ISTS) &
	    hdmirx_rd_dwc(DWC_AUD_FIFO_IEN);
	if (intr_aud_fifo != 0)
		hdmirx_wr_dwc(DWC_AUD_FIFO_ICLR, intr_aud_fifo);

	if (hdcp22_on) {
		intr_hdcp22 =
			hdmirx_rd_dwc(DWC_HDMI2_ISTS) &
		    hdmirx_rd_dwc(DWC_HDMI2_IEN);
	}

	if (is_meson_txl_cpu()) {
		intr_aud_cec =
				hdmirx_rd_dwc(DWC_AUD_CEC_ISTS) &
				hdmirx_rd_dwc(DWC_AUD_CEC_IEN);
		if (intr_aud_cec != 0) {
			cecb_irq_handle();
			hdmirx_wr_dwc(DWC_AUD_CEC_ICLR, intr_aud_cec);
		}
	}

	if (intr_hdcp22 != 0) {
		hdmirx_wr_dwc(DWC_HDMI2_ICLR, intr_hdcp22);
		skip_frame(skip_frame_cnt);
		if (log_level & HDCP_LOG)
			rx_pr("intr=%#x\n", intr_hdcp22);
		switch (intr_hdcp22) {
		case _BIT(0):
			hdcp22_capable_sts = HDCP22_AUTH_STATE_CAPABLE;
			rx.hdcp.hdcp_version = HDCP_VER_22;
			break;
		case _BIT(1):
			hdcp22_capable_sts = HDCP22_AUTH_STATE_NOT_CAPABLE;
			break;
		case _BIT(2):
			hdcp22_auth_sts = HDCP22_AUTH_STATE_LOST;
			break;
		case _BIT(3):
			hdcp22_auth_sts = HDCP22_AUTH_STATE_SUCCESS;
			break;
		case _BIT(4):
			hdcp22_auth_sts = HDCP22_AUTH_STATE_FAILED;
			break;
		default:
			break;
		}
	}

	if (rx.chip_id < CHIP_ID_TL1) {
		rx_top_intr_stat = hdmirx_rd_top(TOP_INTR_STAT);
		if (rx_top_intr_stat & _BIT(31))
			irq_need_clr = 1;
	}

	/* check hdmi open status before dwc isr */
	if (!rx.open_fg) {
		if (irq_need_clr)
			error = 1;
		if (log_level & 0x1000)
			rx_pr("[isr] ignore dwc isr ---\n");
		return error;
	}

	if (intr_hdmi != 0) {
		if (rx_get_bits(intr_hdmi, AKSV_RCV) != 0) {
			/*if (log_level & HDCP_LOG)*/
			rx_pr("[*aksv*\n");
			rx.hdcp.hdcp_version = HDCP_VER_14;
			if (hdmirx_repeat_support())
				rx_start_repeater_auth();
		}
	}

	if (intr_md != 0) {
		if (rx_get_bits(intr_md, md_ists_en) != 0) {
			if (log_level & 0x100)
				rx_pr("md_ists:%x\n", intr_md);
		}
	}

	if (intr_pedc != 0) {
		if (rx_get_bits(intr_pedc, DVIDET | AVI_CKS_CHG) != 0) {
			if (log_level & 0x400)
				rx_pr("[irq] AVI_CKS_CHG\n");
		}
		if (rx_get_bits(intr_pedc, VSI_RCV) != 0) {
			if (log_level & 0x400)
				rx_pr("[irq] VSI_RCV\n");
			vsi_handle_flag = true;
		}
		if (rx_get_bits(intr_pedc, VSI_CKS_CHG) != 0) {
			if (log_level & 0x400)
				rx_pr("[irq] VSI_CKS_CHG\n");
		}
		if (rx_get_bits(intr_pedc, PD_FIFO_START_PASS) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] FIFO START\n");
			rx.irq_flag |= IRQ_PACKET_FLAG;
		}
		if (rx_get_bits(intr_pedc, _BIT(1)) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] FIFO MAX\n");
		}
		if (rx_get_bits(intr_pedc, _BIT(0)) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] FIFO MIN\n");
		}

		if (rx.chip_id >= CHIP_ID_TL1) {
			if (rx_get_bits(intr_pedc,
				_BIT(9)) != 0) {
				if (log_level & 0x400)
					rx_pr("[irq] EMP_RCV %#x\n",
					intr_pedc);
				emp_handle_flag = true;
			} else if (rx_get_bits(intr_pedc,
				DRM_RCV_EN_TXLX) != 0) {
				if (log_level & 0x400)
					rx_pr("[irq] DRM_RCV_EN %#x\n",
					intr_pedc);
				drm_handle_flag = true;
			}
		} else if (rx.chip_id == CHIP_ID_TXLX) {
			if (rx_get_bits(intr_pedc,
				DRM_RCV_EN_TXLX) != 0) {
				if (log_level & 0x400)
					rx_pr("[irq] DRM_RCV_EN %#x\n",
					intr_pedc);
				drm_handle_flag = true;
			}
		} else {
			if (rx_get_bits(intr_pedc,
				DRM_RCV_EN) != 0) {
				if (log_level & 0x400)
					rx_pr("[irq] DRM_RCV_EN %#x\n",
					intr_pedc);
				drm_handle_flag = true;
			}
		}
		if (rx_get_bits(intr_pedc, PD_FIFO_OVERFL) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] PD_FIFO_OVERFL\n");
			rx.irq_flag |= IRQ_PACKET_ERR;
			/*hdmirx_packet_fifo_rst();*/
		}
		if (rx_get_bits(intr_pedc, PD_FIFO_UNDERFL) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] PD_FIFO_UNDFLOW\n");
			rx.irq_flag |= IRQ_PACKET_ERR;
			/*hdmirx_packet_fifo_rst();*/
		}
	}

	if (intr_aud_fifo != 0) {
		if (rx_get_bits(intr_aud_fifo, OVERFL) != 0) {
			if (log_level & 0x100)
				rx_pr("[irq] OVERFL\n");
			rx.irq_flag |= IRQ_AUD_FLAG;
			/* when afifo overflow in multi-channel case(VG-877),
			 * then store all subpkts into afifo, 8ch in and 8ch out
			 */
			if (rx.aud_info.auds_layout)
				rx_afifo_store_all_subpkt(true);
			else
				rx_afifo_store_all_subpkt(false);
			//if (rx.aud_info.real_sr != 0)
				//error |= hdmirx_audio_fifo_rst();
		}
		if (rx_get_bits(intr_aud_fifo, UNDERFL) != 0) {
			if (log_level & 0x100)
				rx_pr("[irq] UNDERFL\n");
			rx.irq_flag |= IRQ_AUD_FLAG;
			rx_afifo_store_all_subpkt(false);
			//if (rx.aud_info.real_sr != 0)
				//error |= hdmirx_audio_fifo_rst();
		}
	}
	if (vsi_handle_flag)
		rx_pkt_handler(PKT_BUFF_SET_VSI);

	if (drm_handle_flag)
		rx_pkt_handler(PKT_BUFF_SET_DRM);

	if (emp_handle_flag)
		rx_pkt_handler(PKT_BUFF_SET_EMP);

	if (rx.irq_flag)
		tasklet_schedule(&rx_tasklet);

	if (irq_need_clr)
		error = 1;

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

reisr:hdmirx_top_intr_stat = hdmirx_rd_top(TOP_INTR_STAT);
	hdmirx_wr_top(TOP_INTR_STAT_CLR, hdmirx_top_intr_stat);
	/* modify interrupt flow for isr loading */
	/* top interrupt handler */
	if (rx.chip_id >= CHIP_ID_TL1) {
		if (hdmirx_top_intr_stat & (1 << 29)) {
			skip_frame(skip_frame_cnt);
			if (log_level & 0x100)
				rx_pr("[isr] sqofclk_fall\n");
		}
		if (hdmirx_top_intr_stat & (1 << 28))
			if (log_level & 0x100)
				rx_pr("[isr] sqofclk_rise\n");
		if (hdmirx_top_intr_stat & (1 << 27))
			if (log_level & 0x400)
				rx_pr("[isr] DE rise edge.\n");
		if (hdmirx_top_intr_stat & (1 << 26)) {
			rx_emp_lastpkt_done_irq();
			if (log_level & 0x400)
				rx_pr("[isr] last_emp_done\n");
		}
		if (hdmirx_top_intr_stat & (1 << 25)) {
			rx_emp_field_done_irq();
			if (log_level & 0x400)
				rx_pr("[isr] emp_field_done\n");
		}
		if (hdmirx_top_intr_stat & (1 << 24))
			if (log_level & 0x100)
				rx_pr("[isr] tmds_align_stable_chg\n");
		if (hdmirx_top_intr_stat & (1 << 23))
			if (log_level & 0x100)
				rx_pr("[isr] meter_stable_chg_cable\n");
	}
	if (hdmirx_top_intr_stat & (1 << 13))
		rx_pr("[isr] auth rise\n");
	if (hdmirx_top_intr_stat & (1 << 14))
		rx_pr("[isr] auth fall\n");
	if (hdmirx_top_intr_stat & (1 << 15))
		rx_pr("[isr] enc rise\n");
	if (hdmirx_top_intr_stat & (1 << 16))
		rx_pr("[isr] enc fall\n");

	/* must clear ip interrupt quickly */
	if (rx.chip_id >= CHIP_ID_TL1) {
		hdmirx_top_intr_stat &= 0x1;
	} else {
		hdmirx_top_intr_stat &= (~(1 << 30));
	}

	if (hdmirx_top_intr_stat) {
		error = hdmi_rx_ctrl_irq_handler();
		if (error < 0) {
			if (error != -EPERM) {
				rx_pr("%s: RX IRQ handler %d\n",
					__func__,
					error);
			}
		}
	}

	if (rx.chip_id < CHIP_ID_TL1) {
		if (error == 1)
			goto reisr;
	} else {
		hdmirx_top_intr_stat = hdmirx_rd_top(TOP_INTR_STAT);
		hdmirx_top_intr_stat &= 0x1;
		if (hdmirx_top_intr_stat) {
			if (log_level & ERR_LOG)
				rx_pr("\n irq_miss");
			goto reisr;
		}
	}
	/* check the ip interrupt again */
	/*hdmirx_top_intr_stat = hdmirx_rd_top(TOP_INTR_STAT);
	 *if (hdmirx_top_intr_stat & (1 << 31)) {
	 *	if (log_level & 0x100)
	 *		rx_pr("[isr] need clear ip irq---\n");
	 *	goto reisr;
	 *
	 *}
	 */
	return IRQ_HANDLED;
}

static const uint32_t sr_tbl[][2] = {
	{32000, 3000},
	{44100, 2000},
	{48000, 2000},
	{88200, 3000},
	{96000, 3000},
	{176400, 3000},
	{192000, 3000},
	{0, 0}
};

static bool check_real_sr_change(void)
{
	uint8_t i;
	bool ret = false;
	/* note: if arc is missmatch with LUT, then return 0 */
	uint32_t ret_sr = 0;

	for (i = 0; sr_tbl[i][0] != 0; i++) {
		if (abs(rx.aud_info.arc - sr_tbl[i][0]) < sr_tbl[i][1]) {
			ret_sr = sr_tbl[i][0];
			break;
		}
	}
	if (ret_sr != rx.aud_info.real_sr) {
		rx.aud_info.real_sr = ret_sr;
		ret = true;
		if (log_level & AUDIO_LOG)
			dump_state(RX_DUMP_AUDIO);
	}
	return ret;
}

static const struct freq_ref_s freq_ref[] = {
	/* interlace 420 3d hac vac index */
	/* 420mode */
	{0,	3,	0,	1920,	2160,	HDMI_2160p50_16x9_Y420},
	{0, 3,	0,	2048,	2160,	HDMI_4096p50_256x135_Y420},
	{0, 3,	0,	960,	1080,	HDMI_1080p_420},
	/* interlace */
	/* {1,	0,	0,	720,	240,	HDMI_720x480i}, */
	{1,	0,	0,	1440,	240,	HDMI_480i60},
	/* {1, 0,	0,	720,	288,	HDMI_720x576i}, */
	{1, 0,	0,	1440,	288,	HDMI_576i50},
	{1, 0,	0,	1920,	540,	HDMI_1080i50},
	{1, 0,	2,	1920,	1103,	HDMI_1080i_ALTERNATIVE},
	{1, 0,	1,	1920,	2228,	HDMI_1080i_FRAMEPACKING},

	{0, 0,	0,	1440,	240,	HDMI_1440x240p60},
	{0, 0,	0,	2880,	240,	HDMI_2880x240p60},
	{0, 0,	0,	1440,	288,	HDMI_1440x288p50},
	{0, 0,	0,	2880,	288,	HDMI_2880x288p50},

	{0, 0,	0,	720,	480,	HDMI_480p60},
	{0, 0,	0,	1440,	480,	HDMI_1440x480p60},
	{0, 0,	1,	720,	1005,	HDMI_480p_FRAMEPACKING},

	{0, 0,	0,	720,	576,	HDMI_576p50},
	{0, 0,	0,	1440,	576,	HDMI_1440x576p50},
	{0, 0,	1,	720,	1201,	HDMI_576p_FRAMEPACKING},

	{0, 0,	0,	1280,	720,	HDMI_720p50},
	{0, 0,	1,	1280,	1470,	HDMI_720p_FRAMEPACKING},

	{0, 0,	0,	1920,	1080,	HDMI_1080p50},
	{0, 0,	2,	1920,	2160,	HDMI_1080p_ALTERNATIVE},
	{0, 0,	1,	1920,	2205,	HDMI_1080p_FRAMEPACKING},

	{1, 0,	0,	2880,	240,	HDMI_2880x480i60},
	{1, 0,	0,	2880,	288,	HDMI_2880x576i50},
	{0, 0,	0,	2880,	480,	HDMI_2880x480p60},
	{0, 0,	0,	2880,	576,	HDMI_2880x576p50},
	/* vesa format*/
	{0, 0,	0,	640,	480,	HDMI_640x480p60},
	{0, 0,	0,	720,	400,	HDMI_720_400},
	{0, 0,	0,	800,	600,	HDMI_800_600},
	{0, 0,	0,	1024,	768,	HDMI_1024_768},
	{0, 0,	0,	1280,	768,	HDMI_1280_768},
	{0, 0,	0,	1280,	800,	HDMI_1280_800},
	{0, 0,	0,	1280,	960,	HDMI_1280_960},
	{0, 0,	0,	1280,	1024,	HDMI_1280_1024},
	{0, 0,	0,	1360,	768,	HDMI_1360_768},
	{0, 0,	0,	1366,	768,	HDMI_1366_768},
	{0, 0,	0,	1440,	900,	HDMI_1440_900},
	{0, 0,	0,	1400,	1050,	HDMI_1400_1050},
	{0, 0,	0,	1600,	900,	HDMI_1600_900},
	{0, 0,	0,	1600,	1200,	HDMI_1600_1200},
	{0, 0,	0,	1680,	1050,	HDMI_1680_1050},
	{0, 0,	0,	1920,	1200,	HDMI_1920_1200},

	/* 4k2k mode */
	{0, 0,	0,	3840,	2160,	HDMI_2160p24_16x9},
	{0, 0,	0,	4096,	2160,	HDMI_4096p24_256x135},
	{0, 0,	0,	2560,	1440,	HDMI_2560_1440},
	{0, 0,	1,	2560,	3488,	HDMI_2560_1440},
	{0, 0,	2,	2560,	2986,	HDMI_2560_1440},

	/* for AG-506 */
	{0, 0,	0,	720,	483,	HDMI_480p60},
};

static bool fmt_vic_abnormal(void)
{
	/* if format is unknown or unsupported after
	 * timing match, but TX send normal VIC, then
	 * abnormal format is detected.
	 */
	if ((rx.pre.sw_vic == HDMI_UNKNOWN) ||
		(rx.pre.sw_vic == HDMI_UNSUPPORT)) {
		if (log_level & VIDEO_LOG)
			rx_pr("fmt_vic_abnormal\n");
		return true;
	} else
		return false;
}

enum tvin_sig_fmt_e hdmirx_hw_get_fmt(void)
{
	enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
	enum hdmi_vic_e vic = HDMI_UNKNOWN;

	if (fmt_vic_abnormal())
		vic = rx.pre.hw_vic;
	else
		vic = rx.pre.sw_vic;
	if (force_vic)
		vic = force_vic;

	switch (vic) {
	case HDMI_640x480p60:
		fmt = TVIN_SIG_FMT_HDMI_640X480P_60HZ;
		break;
	case HDMI_480p60:	/*2 */
	case HDMI_480p60_16x9:	/*3 */
	case HDMI_480p120:	/* 48 */
	case HDMI_480p120_16x9:	/* 49 */
	case HDMI_480p240:	/* 56 */
	case HDMI_480p240_16x9:	/* 57 */
		fmt = TVIN_SIG_FMT_HDMI_720X480P_60HZ;
		break;
	case HDMI_1440x480p60:	/* 14 */
	case HDMI_1440x480p60_16x9:	/* 15 */
		fmt = TVIN_SIG_FMT_HDMI_1440X480P_60HZ;
		break;
	case HDMI_480p_FRAMEPACKING:
		fmt = TVIN_SIG_FMT_HDMI_720X480P_60HZ_FRAME_PACKING;
		break;
	case HDMI_720p24:	/* 60 */
	case HDMI_720p25:	/* 61 */
	case HDMI_720p30:	/* 62 */
	case HDMI_720p50:	/* 19 */
	case HDMI_720p60:	/* 4 */
	case HDMI_720p100:	/* 41 */
	case HDMI_720p120:	/* 47 */
	case HDMI_720p24_64x27:	/* 65 */
	case HDMI_720p25_64x27:	/* 66 */
	case HDMI_720p30_64x27:	/* 67 */
	case HDMI_720p50_64x27:	/* 68 */
	case HDMI_720p60_64x27:	/* 69 */
	case HDMI_720p100_64x27:	/* 70 */
	case HDMI_720p120_64x27:	/* 71 */
		fmt = TVIN_SIG_FMT_HDMI_1280X720P_60HZ;
		break;
	case HDMI_720p_FRAMEPACKING:
		fmt = TVIN_SIG_FMT_HDMI_1280X720P_60HZ_FRAME_PACKING;
		break;
	case HDMI_1080i50:	/* 20 */
	case HDMI_1080i100:	/* 40 */
	case HDMI_1080i60:	/* 5 */
	case HDMI_1080i120:	/* 46 */
		fmt = TVIN_SIG_FMT_HDMI_1920X1080I_60HZ;
		break;
	case HDMI_1080i_FRAMEPACKING:
		fmt = TVIN_SIG_FMT_HDMI_1920X1080I_60HZ_FRAME_PACKING;
		break;
	case HDMI_1080i_ALTERNATIVE:
		fmt = TVIN_SIG_FMT_HDMI_1920X1080I_60HZ_ALTERNATIVE;
		break;
	case HDMI_480i60:	/* 6 */
	case HDMI_480i60_16x9:	/* 7 */
	case HDMI_480i120:	/* 50 */
	case HDMI_480i120_16x9:	/* 51 */
	case HDMI_480i240:	/* 58 */
	case HDMI_480i240_16x9:	/* 59 */
		fmt = TVIN_SIG_FMT_HDMI_1440X480I_60HZ;
		break;
	case HDMI_1080p24:	/* 32 */
	case HDMI_1080p24_64x27: /* 72 */
	case HDMI_1080p25:	/* 33 */
	case HDMI_1080p25_64x27:	/* 73 */
	case HDMI_1080p30:	/* 34 */
	case HDMI_1080p30_64x27:	/* 74 */
	case HDMI_1080p50:	/* 31 */
	case HDMI_1080p60:	/* 16 */
	case HDMI_1080p50_64x27:	/* 75 */
	case HDMI_1080p60_64x27:	/* 76 */
	case HDMI_1080p100:	/* 64 */
	case HDMI_1080p120:	/* 63 */
	case HDMI_1080p100_64x27:	/* 77 */
	case HDMI_1080p120_64x27:	/* 78 */
	case HDMI_1080p_420:
		fmt = TVIN_SIG_FMT_HDMI_1920X1080P_60HZ;
		break;
	case HDMI_1080p_FRAMEPACKING:
		fmt = TVIN_SIG_FMT_HDMI_1920X1080P_24HZ_FRAME_PACKING;
		break;
	case HDMI_1080p_ALTERNATIVE:
		fmt = TVIN_SIG_FMT_HDMI_1920X1080P_24HZ_ALTERNATIVE;
		break;
	case HDMI_576p50:	/* 17 */
	case HDMI_576p50_16x9: /* 18 */
	case HDMI_576p100:	/* 42 */
	case HDMI_576p100_16x9: /* 43 */
	case HDMI_576p200:	/* 52 */
	case HDMI_576p200_16x9: /* 53 */
		fmt = TVIN_SIG_FMT_HDMI_720X576P_50HZ;
		break;
	case HDMI_1440x576p50:	/* 29 */
	case HDMI_1440x576p50_16x9:	/* 30 */
		fmt = TVIN_SIG_FMT_HDMI_1440X576P_50HZ;
		break;
	case HDMI_576p_FRAMEPACKING:
		fmt = TVIN_SIG_FMT_HDMI_720X576P_50HZ_FRAME_PACKING;
		break;
	case HDMI_576i50:	/* 21 */
	case HDMI_576i50_16x9:	/* 22 */
	case HDMI_576i100:	/* 44 */
	case HDMI_576i100_16x9:	/* 45 */
	case HDMI_576i200:	/* 54 */
	case HDMI_576i200_16x9:	/* 55 */
		fmt = TVIN_SIG_FMT_HDMI_1440X576I_50HZ;
		break;
	case HDMI_1440x240p60:	/* 8 */
	case HDMI_1440x240p60_16x9:	/* 9 */
		fmt = TVIN_SIG_FMT_HDMI_1440X240P_60HZ;
		break;
	case HDMI_2880x240p60:	/* 12 */
	case HDMI_2880x240p60_16x9: /* 13 */
		fmt = TVIN_SIG_FMT_HDMI_2880X240P_60HZ;
		break;
	case HDMI_1440x288p50:	/* 23 */
	case HDMI_1440x288p50_16x9: /* 24 */
		fmt = TVIN_SIG_FMT_HDMI_1440X288P_50HZ;
		break;
	case HDMI_2880x288p50:	/* 27 */
	case HDMI_2880x288p50_16x9: /* 28 */
		fmt = TVIN_SIG_FMT_HDMI_2880X288P_50HZ;
		break;
	case HDMI_2880x480i60:	/* 10 */
	case HDMI_2880x480i60_16x9:	/* 11 */
		fmt = TVIN_SIG_FMT_HDMI_2880X480I_60HZ;
		break;
	case HDMI_2880x576i50:	/* 25 */
	case HDMI_2880x576i50_16x9:	/* 26 */
		fmt = TVIN_SIG_FMT_HDMI_2880X576I_50HZ;
		break;
	case HDMI_2880x480p60:	/* 35 */
	case HDMI_2880x480p60_16x9:	/* 36 */
		fmt = TVIN_SIG_FMT_HDMI_2880X480P_60HZ;
		break;
	case HDMI_2880x576p50:	/* 37 */
	case HDMI_2880x576p50_16x9: /* 38 */
		fmt = TVIN_SIG_FMT_HDMI_2880X576P_50HZ;
		break;
	case HDMI_1080i50_1250: /* 39 */
		fmt = TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_B;
		break;

	/* VESA mode*/
	case HDMI_800_600:
		fmt = TVIN_SIG_FMT_HDMI_800X600_00HZ;
		break;
	case HDMI_1024_768:
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
	case HDMI_2160p24_16x9:
	case HDMI_2160p25_16x9:
	case HDMI_2160p30_16x9:
	case HDMI_2160p50_16x9:
	case HDMI_2160p60_16x9:
	case HDMI_2160p24_64x27:
	case HDMI_2160p25_64x27:
	case HDMI_2160p30_64x27:
	case HDMI_2160p50_64x27:
	case HDMI_2160p60_64x27:
	case HDMI_2160p50_16x9_Y420:
	case HDMI_2160p60_16x9_Y420:
	case HDMI_2160p50_64x27_Y420:
	case HDMI_2160p60_64x27_Y420:
		if (en_4k_timing)
			fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
		else
			fmt = TVIN_SIG_FMT_NULL;
		break;
	case HDMI_4096p24_256x135:
	case HDMI_4096p25_256x135:
	case HDMI_4096p30_256x135:
	case HDMI_4096p50_256x135:
	case HDMI_4096p60_256x135:
	case HDMI_4096p50_256x135_Y420:
	case HDMI_4096p60_256x135_Y420:
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

bool rx_is_sig_ready(void)
{
	if ((rx.state == FSM_SIG_READY) ||
		(force_vic))
		return true;
	else
		return false;
}

bool rx_is_nosig(void)
{
	if (force_vic)
		return false;
	return rx.no_signal;
}

/*
 * check timing info
 */
static bool rx_is_timing_stable(void)
{
	bool ret = true;
	uint32_t ch0 = 0, ch1 = 0, ch2 = 0;
	if (stable_check_lvl & TMDS_VALID_EN) {
		if (!is_tmds_valid()) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("TMDS invalid\n");
		}
	}
	if (stable_check_lvl & HACTIVE_EN) {
		if (diff(rx.cur.hactive, rx.pre.hactive) > diff_pixel_th) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("hactive(%d=>%d),",
					rx.pre.hactive,
					rx.cur.hactive);
		}
	}
	if (stable_check_lvl & VACTIVE_EN) {
		if (diff(rx.cur.vactive, rx.pre.vactive) > diff_line_th) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("vactive(%d=>%d),",
					rx.pre.vactive,
					rx.cur.vactive);
		}
	}
	if (stable_check_lvl & HTOTAL_EN) {
		if (diff(rx.cur.htotal, rx.pre.htotal) > diff_pixel_th) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("htotal(%d=>%d),",
					rx.pre.htotal,
					rx.cur.htotal);
		}
	}
	if (stable_check_lvl & VTOTAL_EN) {
		if (diff(rx.cur.vtotal, rx.pre.vtotal) > diff_line_th) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("vtotal(%d=>%d),",
					rx.pre.vtotal,
					rx.cur.vtotal);
		}
	}
	if (stable_check_lvl & COLSPACE_EN) {
		if (rx.pre.colorspace != rx.cur.colorspace) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("colorspace(%d=>%d),",
					rx.pre.colorspace,
					rx.cur.colorspace);
		}
	}
	if (stable_check_lvl & REFRESH_RATE_EN) {
		if (diff(rx.pre.frame_rate, rx.cur.frame_rate)
			> diff_frame_th) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("frame_rate(%d=>%d),",
					rx.pre.frame_rate,
					rx.cur.frame_rate);
		}
	}
	if (stable_check_lvl & REPEAT_EN) {
		if ((rx.pre.repeat != rx.cur.repeat) &&
			(stable_check_lvl & REPEAT_EN)) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("repeat(%d=>%d),",
					rx.pre.repeat,
					rx.cur.repeat);
		}
	}
	if (stable_check_lvl & DVI_EN) {
		if (rx.pre.hw_dvi != rx.cur.hw_dvi) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("dvi(%d=>%d),",
					rx.pre.hw_dvi,
					rx.cur.hw_dvi);
			}
	}
	if (stable_check_lvl & INTERLACED_EN) {
		if (rx.pre.interlaced != rx.cur.interlaced) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("interlaced(%d=>%d),",
					rx.pre.interlaced,
					rx.cur.interlaced);
		}
	}
	if (stable_check_lvl & COLOR_DEP_EN) {
		if (rx.pre.colordepth != rx.cur.colordepth) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("colordepth(%d=>%d),",
					rx.pre.colordepth,
					rx.cur.colordepth);
			}
	}
	if (stable_check_lvl & ERR_CNT_EN) {
		rx_get_error_cnt(&ch0, &ch1, &ch2);
		if ((ch0 + ch1 + ch2) > max_err_cnt) {
			if (sig_stable_err_cnt++ > sig_stable_err_max) {
				rx_pr("warning: more err counter\n");
				sig_stable_err_cnt = 0;
				/*phy setting is fail, need reset phy*/
				sig_unstable_cnt = sig_unstable_max;
				rx.phy.cable_clk = 0;
			}
			ret = false;
		}
	}
	if ((ret == false) && (log_level & VIDEO_LOG))
		rx_pr("\n");

	if (force_vic)
		ret = true;
	return ret;
}

static int get_timing_fmt(void)
{
	int i;
	int size = sizeof(freq_ref)/sizeof(struct freq_ref_s);

	rx.pre.sw_vic = HDMI_UNKNOWN;
	rx.pre.sw_dvi = 0;
	rx.pre.sw_fp = 0;
	rx.pre.sw_alternative = 0;

	for (i = 0; i < size; i++) {
		if (abs(rx.pre.hactive - freq_ref[i].hactive) > diff_pixel_th)
			continue;
		if ((abs(rx.pre.vactive - freq_ref[i].vactive)) > diff_line_th)
			continue;
		if ((rx.pre.colorspace != freq_ref[i].cd420) &&
			(freq_ref[i].cd420 != 0))
			continue;
		if (freq_ref[i].interlace != rx.pre.interlaced)
			continue;
		break;
	}
	if (i == size) {
		/* if format is not matched, sw_vic will be UNSUPPORT */
		rx.pre.sw_vic = HDMI_UNSUPPORT;
		return i;
	}

	rx.pre.sw_vic = freq_ref[i].vic;
	rx.pre.sw_dvi = rx.pre.hw_dvi;

	return i;
}

static void set_fsm_state(enum fsm_states_e sts)
{
	rx.state = sts;
}

static void signal_status_init(void)
{
	hpd_wait_cnt = 0;
	pll_unlock_cnt = 0;
	pll_lock_cnt = 0;
	sig_unstable_cnt = 0;
	sig_stable_cnt = 0;
	sig_unstable_cnt = 0;
	sig_unready_cnt = 0;
	/*rx.wait_no_sig_cnt = 0;*/
	/* rx.no_signal = false; */
	//rx.pre_state = 0;
	/* audio */
	audio_sample_rate = 0;
	audio_coding_type = 0;
	audio_channel_count = 0;
	auds_rcv_sts = 0;
	rx.aud_sr_stable_cnt = 0;
	rx.aud_sr_unstable_cnt = 0;
	//rx_aud_pll_ctl(0);
	rx_set_eq_run_state(E_EQ_START);
	rx.err_rec_mode = ERR_REC_EQ_RETRY;
	rx.err_code = ERR_NONE;
	rx_irq_en(false);
	if (hdcp22_on) {
		if (esm_recovery_mode == ESM_REC_MODE_TMDS)
			rx_esm_tmdsclk_en(false);
		esm_set_stable(false);
	}
	set_scdc_cfg(1, 0);
	rx.state = FSM_INIT;
	/*if (hdcp22_on)*/
		/*esm_set_stable(0);*/
	rx.hdcp.hdcp_version = HDCP_VER_NONE;
	rx.skip = 0;
}

void packet_update(void)
{
	/*rx_getaudinfo(&rx.aud_info);*/

	rgb_quant_range = rx.cur.rgb_quant_range;
	yuv_quant_range = rx.cur.yuv_quant_range;
	it_content = rx.cur.it_content;
	auds_rcv_sts = rx.aud_info.aud_packet_received;
	audio_sample_rate = rx.aud_info.real_sr;
	audio_coding_type = rx.aud_info.coding_type;
	audio_channel_count = rx.aud_info.channel_count;
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
	if ((hdcp22_capable_sts == HDCP22_AUTH_STATE_CAPABLE) &&
		((hdcp22_auth_sts == HDCP22_AUTH_STATE_LOST) ||
		(hdcp22_auth_sts == HDCP22_AUTH_STATE_FAILED))) {
		hdmirx_hdcp22_reauth();
		/*rx_pr("\n auth lost force hpd rst\n");*/
	}
}

void rx_dwc_reset(void)
{
	if (log_level & VIDEO_LOG)
		rx_pr("rx_dwc_reset\n");
	/*
	 * hdcp14 sts only be cleared by
	 * 1. hdmi swreset
	 * 2. new AKSV is received
	 */
	hdmirx_wr_top(TOP_SW_RESET, 0x280);
	udelay(1);
	hdmirx_wr_top(TOP_SW_RESET, 0);
	if (rx.hdcp.hdcp_version == HDCP_VER_NONE)
	/* dishNXT box only send set_avmute, not clear_avmute
	 * we must clear hdcp avmute status here
	 * otherwise hdcp2.2 module does not work
	 */
		/* (rx_get_hdcp14_sts() != 0)) */
		rx_sw_reset(2);
	else
		rx_sw_reset(1);
	rx_irq_en(true);
	/* for hdcp1.4 interact very early cases, don't do
	 * esm reset to avoid interaction be interferenced.
	 */
	if (hdcp22_on &&
		(rx.hdcp.hdcp_version != HDCP_VER_14)) {
		if (esm_recovery_mode == ESM_REC_MODE_TMDS)
			rx_esm_tmdsclk_en(true);
		esm_set_stable(true);
		if (rx.hdcp.hdcp_version == HDCP_VER_22)
			hdmirx_hdcp22_reauth();
	}
	hdmirx_audio_fifo_rst();
	hdmirx_packet_fifo_rst();
}

bool rx_hpd_keep_low(void)
{
	bool ret = false;

	if (downstream_hpd_flag || edid_update_flag) {
		if (hpd_wait_cnt <= hpd_wait_max*4)
			ret = true;
	} else {
		if (hpd_wait_cnt <= hpd_wait_max)
			ret = true;
	}
	return ret;
}

int rx_get_cur_hpd_sts(void)
{
	int tmp;

	tmp = hdmirx_rd_top(TOP_HPD_PWR5V) & (1 << rx.port);
	tmp >>= rx.port;
	return tmp;
}

void esm_set_reset(bool reset)
{
	if (log_level & HDCP_LOG)
		rx_pr("esm set reset:%d\n", reset);

	esm_reset_flag = reset;
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

bool is_unnormal_format(uint8_t wait_cnt)
{
	bool ret = false;
	if ((rx.pre.sw_vic == HDMI_UNSUPPORT) ||
		(rx.pre.sw_vic == HDMI_UNKNOWN)) {
		ret = true;
		if (wait_cnt == sig_stable_max)
			rx_pr("*unsupport*\n");
		if (unnormal_wait_max == wait_cnt) {
			dump_state(RX_DUMP_VIDEO);
			ret = false;
		}
	}
	if (rx.pre.sw_dvi == 1) {
		ret = true;
		if (wait_cnt == sig_stable_max)
			rx_pr("*DVI*\n");
		if (unnormal_wait_max == wait_cnt) {
			dump_state(RX_DUMP_VIDEO);
			ret = false;
		}
	}
	if ((rx.pre.hdcp14_state != 3) &&
		(rx.pre.hdcp14_state != 0) &&
		(rx.hdcp.hdcp_version == HDCP_VER_14)) {
		ret = true;
		if (sig_stable_max == wait_cnt)
			rx_pr("hdcp14 unfinished\n");
		if (unnormal_wait_max == wait_cnt) {
			if ((hdmirx_rd_dwc(DWC_HDCP_BKSV1) == 0) &&
				(hdmirx_rd_dwc(DWC_HDCP_BKSV0) == 0))
				rx.err_code = ERR_NO_HDCP14_KEY;
			ret = false;
		}
	}
	if (rx.hdcp.hdcp_version == HDCP_VER_NONE) {
		ret = true;
		if (wait_cnt == hdcp_none_wait_max) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("hdcp none waiting\n");
		}
	}
	if ((ret == false) && (wait_cnt != sig_stable_max))
		if (log_level & VIDEO_LOG)
			rx_pr("unnormal_format wait cnt = %d\n",
				wait_cnt-sig_stable_max);
	return ret;
}

void fsm_restart(void)
{
	if (hdcp22_on) {
		if (esm_recovery_mode == ESM_REC_MODE_TMDS)
			rx_esm_tmdsclk_en(false);
		esm_set_stable(false);
	}
	hdmirx_hw_config();
	/* hdmi_rx_top_edid_update(); */
	set_scdc_cfg(1, 0);
	vic_check_en = false;
	/* dvi_check_en = true; */
	rx.state = FSM_INIT;
	rx.phy.cable_clk = 0;
	rx.phy.pll_rate = 0;
	rx.phy.phy_bw = 0;
	rx.phy.pll_bw = 0;
	rx_pr("force_fsm_init\n");
}

void dump_unnormal_info(void)
{
	if (rx.pre.colorspace != rx.cur.colorspace)
		rx_pr("colorspace:%d-%d\n",
			rx.pre.colorspace,
			rx.cur.colorspace);
	if (rx.pre.colordepth != rx.cur.colordepth)
		rx_pr("colordepth:%d-%d\n",
			rx.pre.colordepth,
			rx.cur.colordepth);
	if (rx.pre.interlaced != rx.cur.interlaced)
		rx_pr("interlace:%d-%d\n",
			rx.pre.interlaced,
			rx.cur.interlaced);
	if (rx.pre.htotal != rx.cur.htotal)
		rx_pr("htotal:%d-%d\n",
			rx.pre.htotal,
			rx.cur.htotal);
	if (rx.pre.hactive != rx.cur.hactive)
		rx_pr("hactive:%d-%d\n",
			rx.pre.hactive,
			rx.cur.hactive);
	if (rx.pre.vtotal != rx.cur.vtotal)
		rx_pr("vtotal:%d-%d\n",
			rx.pre.vtotal,
			rx.cur.vtotal);
	if (rx.pre.vactive != rx.cur.vactive)
		rx_pr("vactive:%d-%d\n",
			rx.pre.vactive,
			rx.cur.vactive);
	if (rx.pre.repeat != rx.cur.repeat)
		rx_pr("repetition:%d-%d\n",
			rx.pre.repeat,
			rx.cur.repeat);
	if (rx.pre.frame_rate != rx.cur.frame_rate)
		rx_pr("frame_rate:%d-%d\n",
			rx.pre.frame_rate,
			rx.cur.frame_rate);
	if (rx.pre.hw_dvi != rx.cur.hw_dvi)
		rx_pr("dvi:%d-%d\n,",
			rx.pre.hw_dvi,
			rx.cur.hw_dvi);
	if (rx.pre.hdcp14_state != rx.cur.hdcp14_state)
		rx_pr("HDCP14 state:%d-%d\n",
			rx.pre.hdcp14_state,
			rx.cur.hdcp14_state);
	if (rx.pre.hdcp22_state != rx.cur.hdcp22_state)
		rx_pr("HDCP22 state:%d-%d\n",
			rx.pre.hdcp22_state,
			rx.cur.hdcp22_state);
}

void rx_send_hpd_pulse(void)
{
	/*rx_set_cur_hpd(0);*/
	/*fsm_restart();*/
	rx.state = FSM_HPD_LOW;
}

static void set_hdcp(struct hdmi_rx_hdcp *hdcp, const unsigned char *b_key)
{
	int i, j;
	/*memset(&init_hdcp_data, 0, sizeof(struct hdmi_rx_hdcp));*/
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

#if 0
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
#endif

/*
 * func: hdmirx_fill_key_buf
 */
void hdmirx_fill_key_buf(const char *buf, int size)
{
	if (buf[0] == 'k' && buf[1] == 'e' && buf[2] == 'y') {
		set_hdcp(&rx.hdcp, buf + 3);
	} else {
		//memcpy(key_buf, buf, size);
		//key_size = size;
		//rx_pr("HDMIRX: fill key buf, size %d\n", size);
	}
	hdcp14_on = 1;
	rx_pr("HDMIRX: fill key buf, hdcp14_on %d\n", hdcp14_on);
}

/*
 *debug functions
 */
unsigned int hdmirx_hw_dump_reg(unsigned char *buf, int size)
{
	return 0;
}

bool comp_set_pr_var(unsigned char *buff, unsigned char *var_str,
	void *var, int val, unsigned int *index, int ret, int size)
{
	char index_c[5] = {'\0'};

	sprintf(index_c, "%d", (*index));
	if (str_cmp(buff, var_str) || str_cmp((buff), (index_c))) {
		if (!ret)
			memcpy(var, &val, size);
		return true;
	}
	(*index)++;
	return false;
}

int rx_set_global_variable(const char *buf, int size)
{
	char tmpbuf[60];
	int i = 0;
	uint32_t value = 0;
	int ret = 0;
	int index = 1;

	/* rx_pr("buf: %s size: %#x\n", buf, size); */

	if ((buf == 0) || (size == 0) || (size > 60))
		return -1;

	memset(tmpbuf, 0, sizeof(tmpbuf));
	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ') &&
					(buf[i] != '\n') && (i < size)) {
		tmpbuf[i] = buf[i];
		i++;
	}
	/*skip the space*/
	while (++i < size) {
		if ((buf[i] != ' ') && (buf[i] != ','))
			break;
	}
	if ((buf[i] == '0') && ((buf[i + 1] == 'x') || (buf[i + 1] == 'X')))
		ret = kstrtou32(buf + i + 2, 16, &value);
	else
		ret = kstrtou32(buf + i, 10, &value);
	/*rx_pr("tmpbuf: %s value: %#x index:%#x\n", tmpbuf, value, index);*/

	if (ret != 0)
		rx_pr("No value set:%d\n", ret);

	if (set_pr_var(tmpbuf, dwc_rst_wait_cnt_max, value, &index, ret))
		return pr_var(dwc_rst_wait_cnt_max, index);
	if (set_pr_var(tmpbuf, sig_stable_max, value, &index, ret))
		return pr_var(sig_stable_max, index);
	if (set_pr_var(tmpbuf, clk_debug, value, &index, ret))
		return pr_var(clk_debug, index);
	if (set_pr_var(tmpbuf, hpd_wait_max, value, &index, ret))
		return pr_var(hpd_wait_max, index);
	if (set_pr_var(tmpbuf, sig_unstable_max, value, &index, ret))
		return pr_var(sig_unstable_max, index);
	if (set_pr_var(tmpbuf, sig_unready_max, value, &index, ret))
		return pr_var(sig_unready_max, index);
	if (set_pr_var(tmpbuf, pow5v_max_cnt, value, &index, ret))
		return pr_var(pow5v_max_cnt, index);
	if (set_pr_var(tmpbuf, rgb_quant_range, value, &index, ret))
		return pr_var(rgb_quant_range, index);
	if (set_pr_var(tmpbuf, yuv_quant_range, value, &index, ret))
		return pr_var(yuv_quant_range, index);
	if (set_pr_var(tmpbuf, it_content, value, &index, ret))
		return pr_var(it_content, index);
	if (set_pr_var(tmpbuf, diff_pixel_th, value, &index, ret))
		return pr_var(diff_pixel_th, index);
	if (set_pr_var(tmpbuf, diff_line_th, value, &index, ret))
		return pr_var(diff_line_th, index);
	if (set_pr_var(tmpbuf, diff_frame_th, value, &index, ret))
		return pr_var(diff_frame_th, index);
	if (set_pr_var(tmpbuf, force_vic, value, &index, ret))
		return pr_var(force_vic, index);
	if (set_pr_var(tmpbuf, aud_sr_stb_max, value, &index, ret))
		return pr_var(aud_sr_stb_max, index);
	if (set_pr_var(tmpbuf, hdcp22_kill_esm, value, &index, ret))
		return pr_var(hdcp22_kill_esm, index);
	if (set_pr_var(tmpbuf, pwr_sts_to_esm, value, &index, ret))
		return pr_var(pwr_sts_to_esm, index);
	if (set_pr_var(tmpbuf, audio_sample_rate, value, &index, ret))
		return pr_var(audio_sample_rate, index);
	if (set_pr_var(tmpbuf, auds_rcv_sts, value, &index, ret))
		return pr_var(auds_rcv_sts, index);
	if (set_pr_var(tmpbuf, audio_coding_type, value, &index, ret))
		return pr_var(audio_coding_type, index);
	if (set_pr_var(tmpbuf, audio_channel_count, value, &index, ret))
		return pr_var(audio_channel_count, index);
	if (set_pr_var(tmpbuf, hdcp22_capable_sts, value, &index, ret))
		return pr_var(hdcp22_capable_sts, index);
	if (set_pr_var(tmpbuf, esm_auth_fail_en, value, &index, ret))
		return pr_var(esm_auth_fail_en, index);
	if (set_pr_var(tmpbuf, hdcp22_auth_sts, value, &index, ret))
		return pr_var(hdcp22_auth_sts, index);
	if (set_pr_var(tmpbuf, log_level, value, &index, ret))
		return pr_var(log_level, index);
	if (set_pr_var(tmpbuf, auto_switch_off, value, &index, ret))
		return pr_var(auto_switch_off, index);
	if (set_pr_var(tmpbuf, clk_unstable_cnt, value, &index, ret))
		return pr_var(clk_unstable_cnt, index);
	if (set_pr_var(tmpbuf, clk_unstable_max, value, &index, ret))
		return pr_var(clk_unstable_max, index);
	if (set_pr_var(tmpbuf, clk_stable_cnt, value, &index, ret))
		return pr_var(clk_stable_cnt, index);
	if (set_pr_var(tmpbuf, clk_stable_max, value, &index, ret))
		return pr_var(clk_stable_max, index);
	if (set_pr_var(tmpbuf, wait_no_sig_max, value, &index, ret))
		return pr_var(wait_no_sig_max, index);
	if (set_pr_var(tmpbuf, receive_edid_len, value, &index, ret))
		return pr_var(receive_edid_len, index);
	if (set_pr_var(tmpbuf, new_edid, value, &index, ret))
		return pr_var(new_edid, index);
	if (set_pr_var(tmpbuf, hdcp_array_len, value, &index, ret))
		return pr_var(hdcp_array_len, index);
	if (set_pr_var(tmpbuf, hdcp_len, value, &index, ret))
		return pr_var(hdcp_len, index);
	if (set_pr_var(tmpbuf, hdcp_repeat_depth, value, &index, ret))
		return pr_var(hdcp_repeat_depth, index);
	if (set_pr_var(tmpbuf, new_hdcp, value, &index, ret))
		return pr_var(new_hdcp, index);
	if (set_pr_var(tmpbuf, repeat_plug, value, &index, ret))
		return pr_var(repeat_plug, index);
	if (set_pr_var(tmpbuf, up_phy_addr, value, &index, ret))
		return pr_var(up_phy_addr, index);
	if (set_pr_var(tmpbuf, hpd_to_esm, value, &index, ret))
		return pr_var(hpd_to_esm, index);
	if (set_pr_var(tmpbuf, esm_reset_flag, value, &index, ret))
		return pr_var(esm_reset_flag, index);
	if (set_pr_var(tmpbuf, video_stable_to_esm, value, &index, ret))
		return pr_var(video_stable_to_esm, index);
	if (set_pr_var(tmpbuf, enable_hdcp22_esm_log, value, &index, ret))
		return pr_var(enable_hdcp22_esm_log, index);
	if (set_pr_var(tmpbuf, esm_error_flag, value, &index, ret))
		return pr_var(esm_error_flag, index);
	if (set_pr_var(tmpbuf, stable_check_lvl, value, &index, ret))
		return pr_var(stable_check_lvl, index);
	if (set_pr_var(tmpbuf, hdcp22_reauth_enable, value, &index, ret))
		return pr_var(hdcp22_reauth_enable, index);
	if (set_pr_var(tmpbuf, hdcp22_stop_auth_enable, value, &index, ret))
		return pr_var(hdcp22_stop_auth_enable, index);
	if (set_pr_var(tmpbuf, hdcp22_stop_auth, value, &index, ret))
		return pr_var(hdcp22_stop_auth, index);
	if (set_pr_var(tmpbuf, hdcp22_esm_reset2_enable, value, &index, ret))
		return pr_var(hdcp22_esm_reset2_enable, index);
	if (set_pr_var(tmpbuf, hdcp22_esm_reset2, value, &index, ret))
		return pr_var(hdcp22_esm_reset2, index);
	if (set_pr_var(tmpbuf, esm_recovery_mode, value, &index, ret))
		return pr_var(esm_recovery_mode, index);
	if (set_pr_var(tmpbuf, unnormal_wait_max, value, &index, ret))
		return pr_var(unnormal_wait_max, index);
	/* if (set_pr_var(tmpbuf, edid_update_delay, value, &index, ret)) */
		/*return pr_var(edid_update_delay, index);*/
	if (set_pr_var(tmpbuf, hdmi_yuv444_enable, value, &index, ret))
		return pr_var(hdmi_yuv444_enable, index);
	if (set_pr_var(tmpbuf, pc_mode_en, value, &index, ret))
		return pr_var(pc_mode_en, index);
	if (set_pr_var(tmpbuf, en_4k_2_2k, value, &index, ret))
		return pr_var(en_4k_2_2k, index);
	if (set_pr_var(tmpbuf, en_4096_2_3840, value, &index, ret))
		return pr_var(en_4096_2_3840, index);
	if (set_pr_var(tmpbuf, en_4k_timing, value, &index, ret))
		return pr_var(en_4k_timing, index);
	if (set_pr_var(tmpbuf, acr_mode, value, &index, ret))
		return pr_var(acr_mode, index);
	if (set_pr_var(tmpbuf, force_clk_rate, value, &index, ret))
		return pr_var(force_clk_rate, index);
	if (set_pr_var(tmpbuf, auto_aclk_mute, value, &index, ret))
		return pr_var(auto_aclk_mute, index);
	if (set_pr_var(tmpbuf, aud_avmute_en, value, &index, ret))
		return pr_var(aud_avmute_en, index);
	if (set_pr_var(tmpbuf, aud_mute_sel, value, &index, ret))
		return pr_var(aud_mute_sel, index);
	if (set_pr_var(tmpbuf, md_ists_en, value, &index, ret))
		return pr_var(md_ists_en, index);
	if (set_pr_var(tmpbuf, pdec_ists_en, value, &index, ret))
		return pr_var(pdec_ists_en, index);
	if (set_pr_var(tmpbuf, packet_fifo_cfg, value, &index, ret))
		return pr_var(packet_fifo_cfg, index);
	if (set_pr_var(tmpbuf, pd_fifo_start_cnt, value, &index, ret))
		return pr_var(pd_fifo_start_cnt, index);
	if (set_pr_var(tmpbuf, hdcp22_on, value, &index, ret))
		return pr_var(hdcp22_on, index);
	if (set_pr_var(tmpbuf, dv_nopacket_timeout, value, &index, ret))
		return pr_var(dv_nopacket_timeout, index);
	if (set_pr_var(tmpbuf, delay_ms_cnt, value, &index, ret))
		return pr_var(delay_ms_cnt, index);
	if (set_pr_var(tmpbuf, downstream_repeat_support, value, &index, ret))
		return pr_var(downstream_repeat_support, index);
	if (set_pr_var(tmpbuf, eq_max_setting, value, &index, ret))
		return pr_var(eq_max_setting, index);
	if (set_pr_var(tmpbuf, eq_dbg_ch0, value, &index, ret))
		return pr_var(eq_dbg_ch0, index);
	if (set_pr_var(tmpbuf, eq_dbg_ch1, value, &index, ret))
		return pr_var(eq_dbg_ch1, index);
	if (set_pr_var(tmpbuf, eq_dbg_ch2, value, &index, ret))
		return pr_var(eq_dbg_ch2, index);
	if (set_pr_var(tmpbuf, edid_mode, value, &index, ret))
		return pr_var(edid_mode, index);
	if (set_pr_var(tmpbuf, phy_pddq_en, value, &index, ret))
		return pr_var(edid_mode, index);
	if (set_pr_var(tmpbuf, long_cable_best_setting, value, &index, ret))
		return pr_var(long_cable_best_setting, index);
	if (set_pr_var(tmpbuf, port_map, value, &index, ret))
		return pr_var(port_map, index);
	if (set_pr_var(tmpbuf, new_hdr_lum, value, &index, ret))
		return pr_var(new_hdr_lum, index);
	if (set_pr_var(tmpbuf, skip_frame_cnt, value, &index, ret))
		return pr_var(skip_frame_cnt, index);
	if (set_pr_var(tmpbuf, vdin_drop_frame_cnt, value, &index, ret))
		return pr_var(vdin_drop_frame_cnt, index);
	if (set_pr_var(tmpbuf, atmos_edid_update_hpd_en, value, &index, ret))
		return pr_var(atmos_edid_update_hpd_en, index);
	if (set_pr_var(tmpbuf, suspend_pddq_sel, value, &index, ret))
		return pr_var(suspend_pddq_sel, index);
	if (set_pr_var(tmpbuf, aud_ch_map, value, &index, ret))
		return pr_var(aud_ch_map, index);
	if (set_pr_var(tmpbuf, hdcp_none_wait_max, value, &index, ret))
		return pr_var(hdcp_none_wait_max, index);
	if (set_pr_var(tmpbuf, pll_unlock_max, value, &index, ret))
		return pr_var(pll_unlock_max, index);
	if (set_pr_var(tmpbuf, esd_phy_rst_max, value, &index, ret))
		return pr_var(esd_phy_rst_max, index);
	if (set_pr_var(tmpbuf, ignore_sscp_charerr, value, &index, ret))
		return pr_var(ignore_sscp_charerr, index);
	if (set_pr_var(tmpbuf, ignore_sscp_tmds, value, &index, ret))
		return pr_var(ignore_sscp_tmds, index);
	if (set_pr_var(tmpbuf, fsm_log_en, value, &index, ret))
		return pr_var(fsm_log_en, index);
	if (set_pr_var(tmpbuf, err_chk_en, value, &index, ret))
		return pr_var(err_chk_en, index);
	if (set_pr_var(tmpbuf, phy_retry_times, value, &index, ret))
		return pr_var(phy_retry_times, index);
	if (set_pr_var(tmpbuf, find_best_eq, value, &index, ret))
		return pr_var(find_best_eq, index);
	if (set_pr_var(tmpbuf, eq_try_cnt, value, &index, ret))
		return pr_var(eq_try_cnt, index);
	if (set_pr_var(tmpbuf, pll_rst_max, value, &index, ret))
		return pr_var(pll_rst_max, index);
	if (set_pr_var(tmpbuf, hdcp_enc_mode, value, &index, ret))
		return pr_var(hdcp_enc_mode, index);
	if (set_pr_var(tmpbuf, hbr_force_8ch, value, &index, ret))
		return pr_var(hbr_force_8ch, index);
	if (set_pr_var(tmpbuf, cdr_lock_level, value, &index, ret))
		return pr_var(cdr_lock_level, index);
	if (set_pr_var(tmpbuf, top_intr_maskn_value, value, &index, ret))
		return pr_var(top_intr_maskn_value, index);
	if (set_pr_var(tmpbuf, pll_lock_max, value, &index, ret))
		return pr_var(pll_lock_max, index);
	if (set_pr_var(tmpbuf, clock_lock_th, value, &index, ret))
		return pr_var(clock_lock_th, index);
	if (set_pr_var(tmpbuf, en_take_dtd_space, value, &index, ret))
		return pr_var(en_take_dtd_space, index);
	if (set_pr_var(tmpbuf, earc_cap_ds_update_hpd_en, value, &index, ret))
		return pr_var(earc_cap_ds_update_hpd_en, index);
	if (set_pr_var(tmpbuf, scdc_force_en, value, &index, ret))
		return pr_var(scdc_force_en, index);
	if (set_pr_var(tmpbuf, hdcp_hpd_ctrl_en, value, &index, ret))
		return pr_var(hdcp_hpd_ctrl_en, index);
	if (set_pr_var(tmpbuf, eq_dbg_lvl, value, &index, ret))
		return pr_var(eq_dbg_lvl, index);
	return 0;
}

void rx_get_global_variable(const char *buf)
{
	int i = 1;

	rx_pr("index %-30s   value\n", "varaible");
	pr_var(dwc_rst_wait_cnt_max, i++);
	pr_var(sig_stable_max, i++);
	pr_var(clk_debug, i++);
	pr_var(hpd_wait_max, i++);
	pr_var(sig_unstable_max, i++);
	pr_var(sig_unready_max, i++);
	pr_var(pow5v_max_cnt, i++);
	pr_var(rgb_quant_range, i++);
	pr_var(yuv_quant_range, i++);
	pr_var(it_content, i++);
	pr_var(diff_pixel_th, i++);
	pr_var(diff_line_th, i++);
	pr_var(diff_frame_th, i++);
	pr_var(force_vic, i++);
	pr_var(aud_sr_stb_max, i++);
	pr_var(hdcp22_kill_esm, i++);
	pr_var(pwr_sts_to_esm, i++);
	pr_var(audio_sample_rate, i++);
	pr_var(auds_rcv_sts, i++);
	pr_var(audio_coding_type, i++);
	pr_var(audio_channel_count, i++);
	pr_var(hdcp22_capable_sts, i++);
	pr_var(esm_auth_fail_en, i++);
	pr_var(hdcp22_auth_sts, i++);
	pr_var(log_level, i++);
	pr_var(auto_switch_off, i++);
	pr_var(clk_unstable_cnt, i++);
	pr_var(clk_unstable_max, i++);
	pr_var(clk_stable_cnt, i++);
	pr_var(clk_stable_max, i++);
	pr_var(wait_no_sig_max, i++);
	pr_var(receive_edid_len, i++);
	pr_var(new_edid, i++);
	pr_var(hdcp_array_len, i++);
	pr_var(hdcp_len, i++);
	pr_var(hdcp_repeat_depth, i++);
	pr_var(new_hdcp, i++);
	pr_var(repeat_plug, i++);
	pr_var(up_phy_addr, i++);
	pr_var(hpd_to_esm, i++);
	pr_var(esm_reset_flag, i++);
	pr_var(video_stable_to_esm, i++);
	pr_var(enable_hdcp22_esm_log, i++);
	pr_var(esm_error_flag, i++);
	pr_var(stable_check_lvl, i++);
	pr_var(hdcp22_reauth_enable, i++);
	pr_var(hdcp22_stop_auth_enable, i++);
	pr_var(hdcp22_stop_auth, i++);
	pr_var(hdcp22_esm_reset2_enable, i++);
	pr_var(hdcp22_esm_reset2, i++);
	pr_var(esm_recovery_mode, i++);
	pr_var(unnormal_wait_max, i++);
	/*pr_var(edid_update_delay, i++);*/
	pr_var(hdmi_yuv444_enable, i++);
	pr_var(pc_mode_en, i++);
	pr_var(en_4k_2_2k, i++);
	pr_var(en_4096_2_3840, i++);
	pr_var(en_4k_timing, i++);
	pr_var(acr_mode, i++);
	pr_var(force_clk_rate, i++);
	pr_var(auto_aclk_mute, i++);
	pr_var(aud_avmute_en, i++);
	pr_var(aud_mute_sel, i++);
	pr_var(md_ists_en, i++);
	pr_var(pdec_ists_en, i++);
	pr_var(packet_fifo_cfg, i++);
	pr_var(pd_fifo_start_cnt, i++);
	pr_var(hdcp22_on, i++);
	pr_var(dv_nopacket_timeout, i++);
	pr_var(delay_ms_cnt, i++);
	pr_var(downstream_repeat_support, i++);
	pr_var(eq_max_setting, i++);
	pr_var(eq_dbg_ch0, i++);
	pr_var(eq_dbg_ch1, i++);
	pr_var(eq_dbg_ch2, i++);
	pr_var(edid_mode, i++);
	pr_var(phy_pddq_en, i++);
	pr_var(long_cable_best_setting, i++);
	pr_var(port_map, i++);
	pr_var(new_hdr_lum, i++);
	pr_var(skip_frame_cnt, i++);
	pr_var(vdin_drop_frame_cnt, i++);
	pr_var(atmos_edid_update_hpd_en, i++);
	pr_var(suspend_pddq_sel, i++);
	pr_var(aud_ch_map, i++);
	pr_var(hdcp_none_wait_max, i++);
	pr_var(pll_unlock_max, i++);
	pr_var(esd_phy_rst_max, i++);
	pr_var(ignore_sscp_charerr, i++);
	pr_var(ignore_sscp_tmds, i++);
	pr_var(fsm_log_en, i++);
	pr_var(err_chk_en, i++);
	pr_var(phy_retry_times, i++);
	pr_var(find_best_eq, i++);
	pr_var(eq_try_cnt, i++);
	pr_var(pll_rst_max, i++);
	pr_var(hdcp_enc_mode, i++);
	pr_var(hbr_force_8ch, i++);
	pr_var(cdr_lock_level, i++);
	pr_var(top_intr_maskn_value, i++);
	pr_var(pll_lock_max, i++);
	pr_var(clock_lock_th, i++);
	pr_var(en_take_dtd_space, i++);
	pr_var(earc_cap_ds_update_hpd_en, i++);
	pr_var(scdc_force_en, i++);
	pr_var(hdcp_hpd_ctrl_en, i++);
	pr_var(eq_dbg_lvl, i++);
}

void skip_frame(unsigned int cnt)
{
	if (rx.state == FSM_SIG_READY) {
		rx.skip = (1000 * 100 / rx.pre.frame_rate / 10) + 1;
		rx.skip = cnt * rx.skip;
	}
	rx_pr("rx.skip = %d\n", rx.skip);
}

void wait_ddc_idle(void)
{
	unsigned char i;
	/* add delays to avoid the edid communication fail */
	for (i = 0; i <= 10; i++) {
		if (!is_ddc_idle(rx.port))
			msleep(20);
	}
}

/***********************
 * hdmirx_open_port
 ***********************/
void hdmirx_open_port(enum tvin_port_e port)
{
	uint32_t fsmst = sm_pause;

	/* stop fsm when swich port */
	sm_pause = 1;
	rx.port = (port - TVIN_PORT_HDMI0) & 0xf;
	//rx.no_signal = false;
	//rx.wait_no_sig_cnt = 0;
	vic_check_en = false;
	/* dvi_check_en = true; */
	if (hdmirx_repeat_support())
		rx.hdcp.repeat = repeat_plug;
	else
		rx.hdcp.repeat = 0;

	if ((pre_port != rx.port) ||
		(rx_get_cur_hpd_sts() == 0) ||
		/* when open specific port, force to enable it */
		(disable_port_en && (rx.port == disable_port_num))) {
		if (hdcp22_on) {
			esm_set_stable(false);
			esm_set_reset(true);
			if (esm_recovery_mode == ESM_REC_MODE_TMDS)
				rx_esm_tmdsclk_en(false);
			/*hpd_to_esm = 1;*/
			/* switch_set_state(&rx.hpd_sdev, 0x01); */
			if (log_level & VIDEO_LOG)
				rx_pr("switch_set_state:%d\n", pwr_sts);
		}
		if (rx.state > FSM_HPD_LOW)
			rx.state = FSM_HPD_LOW;
		rx_set_cur_hpd(0, 0);
		/* need reset the whole module when switch port */
		wait_ddc_idle();
		hdmi_rx_top_edid_update();
		hdmirx_hw_config();
	} else {
		if (rx.state >= FSM_SIG_STABLE)
			rx.state = FSM_SIG_STABLE;
		else if (rx.state >= FSM_HPD_HIGH)
			rx.state = FSM_HPD_HIGH;
	}
	edid_update_flag = 0;
	rx_pkt_initial();
	sm_pause = fsmst;
	extcon_set_state_sync(rx.rx_excton_open, EXTCON_DISP_HDMI, 1);
	rx_pr("%s:%d\n", __func__, rx.port);
}

void hdmirx_close_port(void)
{
	/* if (sm_pause) */
	/*	return; */
	/* External_Mute(1); */
	/* when exit hdmi, disable termination & hpd of specific port */
	if (disable_port_en)
		rx_set_port_hpd(disable_port_num, 0);
	extcon_set_state_sync(rx.rx_excton_open, EXTCON_DISP_HDMI, 0);
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

static void rx_cable_clk_monitor(void)
{
	static bool pre_sts;

	bool sts = is_clk_stable();

	if (pre_sts != sts) {
		rx_pr("\nclk stable = %d\n", sts);
		pre_sts = sts;
	}
}

/* ---------------------------------------------------------- */
/* func:         port A,B,C,D  hdmitx-5v monitor & HPD control */
/* note:         G9TV portD no used */
/* ---------------------------------------------------------- */
void rx_5v_monitor(void)
{
	static uint8_t check_cnt;
	uint8_t tmp_5v = rx_get_hdmi5v_sts();
	bool tmp_arc_5v;

	if (auto_switch_off)
		tmp_5v = 0x0f;

	if (tmp_5v != pwr_sts)
		check_cnt++;

	if (check_cnt >= pow5v_max_cnt) {
		check_cnt = 0;
		pwr_sts = tmp_5v;
		rx.cur_5v_sts = (pwr_sts >> rx.port) & 1;
		hotplug_wait_query();
		rx_pr("hotplug-0x%x\n", pwr_sts);
		if (rx.cur_5v_sts == 0) {
			set_fsm_state(FSM_5V_LOST);
			rx.err_code = ERR_5V_LOST;
			vic_check_en = false;
			dvi_check_en = true;
		}
	}
	rx.cur_5v_sts = (pwr_sts >> rx.port) & 1;
	/* inform hdcp_rx22 the 5v sts of rx */
	if (hdcp22_on) {
		if (!pwr_sts)
			pwr_sts_to_esm = true;
		else
			pwr_sts_to_esm = false;
	}
	if (rx.chip_id == CHIP_ID_TM2) {
		tmp_arc_5v = (pwr_sts >> rx.arc_port) & 1;
		if (rx.arc_5vsts != tmp_arc_5v) {
			rx.arc_5vsts = tmp_arc_5v;
			earc_hdmirx_hpdst(rx.arc_port, rx.arc_5vsts);
		}
	}
}

/*
 * function:
 * for check error counter start for tl1
 *
 */
void rx_monitor_error_cnt_start(void)
{
	rx.phy.timestap = get_seconds();
}

/*
 * function:
 *	1min error counter check for tl1 aml phy
 */
void rx_monitor_error_counter(void)
{
	ulong timestap;
	uint32_t ch0, ch1, ch2;

	if (rx.chip_id < CHIP_ID_TL1)
		return;

	timestap = get_seconds();

	if ((timestap - rx.phy.timestap) > 1) {
		rx.phy.timestap = timestap;
		rx_get_error_cnt(&ch0, &ch1, &ch2);
		if (ch0 || ch1 || ch2)
			rx_pr("err cnt:%d,%d,%d\n", ch0, ch1, ch2);
	}
}

void rx_err_monitor(void)
{
	//static bool hdcp14_sts;
	if (clk_debug)
		rx_cable_clk_monitor();

	rx.timestamp++;
	if (rx.err_code == ERR_NONE)
		return;

	if (err_dbg_cnt++ > err_dbg_cnt_max)
		//return;
		err_dbg_cnt = 0;
	//rx_pr("err_code = %d\n", rx.err_code);
	switch (rx.err_code) {
	case ERR_5V_LOST:
		if (err_dbg_cnt == 0)
			rx_pr("hdmi-5v-state = %x\n", pwr_sts);
		break;
	case ERR_CLK_UNSTABLE:
		if (err_dbg_cnt == 0)
			rx_pr("clk unstable = %d\n",
				is_clk_stable());
		break;
	case ERR_PHY_UNLOCK:
		if (err_dbg_cnt == 0) {
			if (rx.chip_id < CHIP_ID_TL1)
				rx_pr("EQ = %d-%d-%d\n",
					eq_ch0.bestsetting,
					eq_ch1.bestsetting,
					eq_ch2.bestsetting);
			else
				rx_pr("PHY unlock\n");
		}
		break;
	case ERR_DE_UNSTABLE:
		if (err_dbg_cnt == 0)
			dump_unnormal_info();
		break;
	case ERR_NO_HDCP14_KEY:
		if (err_dbg_cnt == 0) {
			rx_pr("NO HDCP1.4 KEY\n");
			rx_pr("bksv = %d,%d", rx.hdcp.bksv[0], rx.hdcp.bksv[1]);
		}
		break;
	case ERR_TIMECHANGE:
		//rx_pr("ready time = %d", rx.unready_timestamp);
		//rx_pr("stable time = %d", rx.stable_timestamp);
		if (((rx.unready_timestamp - rx.stable_timestamp) < 30) &&
			(rx_get_eq_run_state() == E_EQ_SAME)) {
			rx.err_cnt++;
			rx_pr("timingchange warning cnt = %d\n", rx.err_cnt);
		}
		if (rx.err_cnt > 3) {
			fsm_restart();
			rx.err_cnt = 0;
		}
		rx.err_code = ERR_NONE;
		break;
	default:
		break;
	}
}

char *fsm_st[] = {
	"FSM_5V_LOST",
	"FSM_INIT",
	"FSM_HPD_LOW",
	"FSM_HPD_HIGH",
	"FSM_WAIT_CLK_STABLE",
	"FSM_EQ_START",
	"FSM_WAIT_EQ_DONE",
	"FSM_SIG_UNSTABLE",
	"FSM_SIG_WAIT_STABLE",
	"FSM_SIG_STABLE",
	"FSM_SIG_READY",
	"FSM_NULL",
};

/*
 * FUNC: rx_main_state_machine
 * signal detection main process
 */
void rx_main_state_machine(void)
{
	int pre_auds_ch_alloc;
	int pre_auds_hbr;

	switch (rx.state) {
	case FSM_5V_LOST:
		if (rx.cur_5v_sts)
			rx.state = FSM_INIT;
		fsm_restart();
		break;
	case FSM_HPD_LOW:
		rx_set_cur_hpd(0, 0);
		set_scdc_cfg(1, 0);
		rx.state = FSM_INIT;
		break;
	case FSM_INIT:
		signal_status_init();
		rx.phy.cable_clk = 0;
		rx.state = FSM_HPD_HIGH;
		break;
	case FSM_HPD_HIGH:
		hpd_wait_cnt++;
		if ((rx_get_cur_hpd_sts() == 0) &&
			rx_hpd_keep_low()) {
			break;
		}
		hpd_wait_cnt = 0;
		clk_unstable_cnt = 0;
		esd_phy_rst_cnt = 0;
		downstream_hpd_flag = 0;
		edid_update_flag = 0;
		pre_port = rx.port;
		rx_set_cur_hpd(1, 0);
		rx.phy.cable_clk = 0;
		rx.phy.cablesel = 0;
		set_scdc_cfg(0, 1);
		/* rx.hdcp.hdcp_version = HDCP_VER_NONE; */
		rx.state = FSM_WAIT_CLK_STABLE;
		break;
	case FSM_WAIT_CLK_STABLE:
		if (is_clk_stable()) {
			if (clk_unstable_cnt != 0) {
				rx_pr("wait clk cnt %d\n", clk_unstable_cnt);
				clk_unstable_cnt = 0;
			}
			if (++clk_stable_cnt > clk_stable_max) {
				rx.state = FSM_EQ_START;
				clk_stable_cnt = 0;
				rx_pr("clk stable=%d\n", rx.phy.cable_clk);
				rx.err_code = ERR_NONE;
			}
		} else {
			clk_stable_cnt = 0;
			if (clk_unstable_cnt < clk_unstable_max) {
				clk_unstable_cnt++;
				break;
			}
			clk_unstable_cnt = 0;
			if (esd_phy_rst_cnt < esd_phy_rst_max) {
				hdmirx_phy_init();
				rx.phy.cable_clk = 0;
				esd_phy_rst_cnt++;
			} else {
				rx.err_code = ERR_NONE;
				rx.state = FSM_HPD_LOW;
				esd_phy_rst_cnt = 0;
				break;
			}
			rx.err_code = ERR_CLK_UNSTABLE;
		}
		break;
	case FSM_EQ_START:
		rx_run_eq();
		rx.state = FSM_WAIT_EQ_DONE;
		break;
	case FSM_WAIT_EQ_DONE:
		if (rx_eq_done()) {
			rx.state = FSM_SIG_UNSTABLE;
			pll_lock_cnt = 0;
			pll_unlock_cnt = 0;
		}
		break;
	case FSM_SIG_UNSTABLE:
		if (is_tmds_valid()) {
			pll_unlock_cnt = 0;
			if (++pll_lock_cnt < pll_lock_max)
				break;
			rx_dwc_reset();
			rx.err_code = ERR_NONE;
			rx.state = FSM_SIG_WAIT_STABLE;
		} else {
			pll_lock_cnt = 0;
			if (pll_unlock_cnt < pll_unlock_max) {
				pll_unlock_cnt++;
				break;
			}
			if (rx.err_rec_mode == ERR_REC_EQ_RETRY) {
				rx.state = FSM_WAIT_CLK_STABLE;
				if (esd_phy_rst_cnt++ < esd_phy_rst_max) {
					rx.phy.cablesel++;
					rx_pr("cablesel=%d\n", rx.phy.cablesel);
					rx.phy.cable_clk = 0;
					/* hdmirx_phy_init(); */
				} else
					rx.err_rec_mode = ERR_REC_HPD_RST;
			} else if (rx.err_rec_mode == ERR_REC_HPD_RST) {
				rx_set_cur_hpd(0, 2);
				rx.phy.cable_clk = 0;
				rx.state = FSM_INIT;
				rx.err_rec_mode = ERR_REC_EQ_RETRY;
			}
			rx_set_eq_run_state(E_EQ_START);
		}
		break;
	case FSM_SIG_WAIT_STABLE:
		dwc_rst_wait_cnt++;
		if (dwc_rst_wait_cnt < dwc_rst_wait_cnt_max)
			break;
		if ((edid_update_flag) &&
			(dwc_rst_wait_cnt < edid_update_delay))
			break;
		edid_update_flag = 0;
		dwc_rst_wait_cnt = 0;
		sig_stable_cnt = 0;
		sig_unstable_cnt = 0;
		sig_stable_err_cnt = 0;
		rx.state = FSM_SIG_STABLE;
		break;
	case FSM_SIG_STABLE:
		memcpy(&rx.pre, &rx.cur,
			sizeof(struct rx_video_info));
		rx_get_video_info();
		if (rx_is_timing_stable()) {
			if (++sig_stable_cnt >= sig_stable_max) {
				get_timing_fmt();
				if (is_unnormal_format(sig_stable_cnt))
					break;
				/* if format vic is abnormal, do hw
				 * reset once to try to recover.
				 */
				if (fmt_vic_abnormal()) {
					if (vic_check_en) {
						hdmi_rx_top_edid_update();
						hdmirx_hw_config();
						rx.state = FSM_HPD_LOW;
					} else {
						rx.state = FSM_WAIT_CLK_STABLE;
						rx_set_eq_run_state(E_EQ_START);
						vic_check_en = true;
					}
					break;
				}
				sig_unready_cnt = 0;
				/* if DVI signal is detected, then try
				 * hpd reset once to recovery, to avoid
				 * recognition to DVI of low probability
				 */
				if (rx.pre.sw_dvi && dvi_check_en &&
					(rx.hdcp.hdcp_version ==
						HDCP_VER_NONE)) {
					rx.state = FSM_HPD_LOW;
					dvi_check_en = false;
					break;
				}
				sig_stable_cnt = 0;
				rx.skip = 0;
				rx.state = FSM_SIG_READY;
				rx.aud_sr_stable_cnt = 0;
				rx.aud_sr_unstable_cnt = 0;
				rx.no_signal = false;
				/*memset(&rx.aud_info, 0,*/
					/*sizeof(struct aud_info_s));*/
				/*rx_set_eq_run_state(E_EQ_PASS);*/
				hdmirx_config_video();
				rx_get_audinfo(&rx.aud_info);
				hdmirx_config_audio();
				rx_aud_pll_ctl(1);
				rx_afifo_store_all_subpkt(false);
				hdmirx_audio_fifo_rst();
				rx.stable_timestamp = rx.timestamp;
				rx_pr("Sig ready\n");
				dump_state(RX_DUMP_VIDEO);
				#ifdef K_TEST_CHK_ERR_CNT
				rx_monitor_error_cnt_start();
				#endif
				sig_stable_err_cnt = 0;
			}
		} else {
			sig_stable_cnt = 0;
			if (sig_unstable_cnt < sig_unstable_max) {
				sig_unstable_cnt++;
				break;
			}
			if (rx.err_rec_mode == ERR_REC_EQ_RETRY) {
				rx.state = FSM_WAIT_CLK_STABLE;
				rx.phy.cablesel++;
				rx_pr("cablesel1=%d\n", rx.phy.cablesel);
				rx.err_rec_mode = ERR_REC_HPD_RST;
				rx_set_eq_run_state(E_EQ_START);
			} else if (rx.err_rec_mode == ERR_REC_HPD_RST) {
				rx_set_cur_hpd(0, 2);
				rx.phy.cable_clk = 0;
				rx.state = FSM_INIT;
				rx.err_rec_mode = ERR_REC_EQ_RETRY;
			}
		}
		break;
	case FSM_SIG_READY:
		rx_get_video_info();
		rx.err_rec_mode = ERR_REC_EQ_RETRY;
		#ifdef K_TEST_CHK_ERR_CNT
		rx_monitor_error_counter();
		#endif
		/* video info change */
		if ((!is_tmds_valid()) ||
			(!rx_is_timing_stable())) {
			skip_frame(skip_frame_cnt);
			if (++sig_unready_cnt >= sig_unready_max) {
				/*sig_lost_lock_cnt = 0;*/
				rx.unready_timestamp = rx.timestamp;
				rx.err_code = ERR_TIMECHANGE;
				dump_unnormal_info();
				rx_pr("sig ready exit: ");
				rx_pr("tmdsvalid:%d, unready:%d\n",
					hdmirx_rd_dwc(DWC_HDMI_PLL_LCK_STS),
					sig_unready_cnt);
				sig_unready_cnt = 0;
				audio_sample_rate = 0;
				rx_aud_pll_ctl(0);
				rx.hdcp.hdcp_version = HDCP_VER_NONE;
				rx.state = FSM_WAIT_CLK_STABLE;
				/* rx.pre_state = FSM_SIG_READY; */
				vic_check_en = false;
				rx.skip = 0;
				rx.aud_sr_stable_cnt = 0;
				rx.aud_sr_unstable_cnt = 0;
				rx.phy.cable_clk = 0;
				esd_phy_rst_cnt = 0;
				if (hdcp22_on) {
					esm_set_stable(false);
					if (esm_recovery_mode
						== ESM_REC_MODE_RESET)
						esm_set_reset(true);
					/* for some hdcp2.2 devices which
					 * don't retry 2.2 interaction
					 * continuously and don't response
					 * to re-auth, such as chroma 2403,
					 * esm needs to be on work even
					 * before tmds is valid so that to
					 * not miss 2.2 interaction
					 */
					/* else */
						/* rx_esm_tmdsclk_en(false); */
				}
				break;
			}
		} else {
			sig_unready_cnt = 0;
			if (rx.skip > 0)
				rx.skip--;
		}
		if (rx.pre.sw_dvi == 1)
			break;

		packet_update();
		pre_auds_ch_alloc = rx.aud_info.auds_ch_alloc;
		pre_auds_hbr = rx.aud_info.aud_hbr_rcv;
		rx_get_audinfo(&rx.aud_info);

		if (check_real_sr_change())
			rx_audio_pll_sw_update();
		if ((pre_auds_ch_alloc != rx.aud_info.auds_ch_alloc) ||
			((pre_auds_hbr != rx.aud_info.aud_hbr_rcv) &&
			hbr_force_8ch)) {
			if (log_level & AUDIO_LOG)
				dump_state(RX_DUMP_AUDIO);
			hdmirx_config_audio();
			hdmirx_audio_fifo_rst();
			rx_audio_pll_sw_update();
		}
		if (is_aud_pll_error()) {
			rx.aud_sr_unstable_cnt++;
			if (rx.aud_sr_unstable_cnt > aud_sr_stb_max) {
				unsigned int aud_sts = rx_get_aud_pll_err_sts();

				if (aud_sts == E_REQUESTCLK_ERR) {
					hdmirx_phy_init();
					rx.state = FSM_WAIT_CLK_STABLE;
					/*timing sw at same FRQ*/
					rx.phy.cable_clk = 0;
					/*rx.pre_state = FSM_SIG_READY;*/
					rx_pr("reqclk err->wait_clk\n");
				} else if (aud_sts == E_PLLRATE_CHG)
					rx_aud_pll_ctl(1);
				else if (aud_sts == E_AUDCLK_ERR)
					rx_audio_bandgap_rst();
				else {
					rx_acr_info_sw_update();
					rx_audio_pll_sw_update();
				}
				if (log_level & AUDIO_LOG)
					rx_pr("update audio-err:%d\n", aud_sts);
				rx.aud_sr_unstable_cnt = 0;
			}
		} else
			rx.aud_sr_unstable_cnt = 0;

		break;
	default:
		break;
	}

	/* for fsm debug */
	if (fsm_log_en && (rx.state != rx.pre_state)) {
		rx_pr("fsm state:%d(%s) to %d(%s)\n", rx.pre_state,
			fsm_st[rx.pre_state], rx.state, fsm_st[rx.state]);
		rx.pre_state = rx.state;
	}
}

unsigned int hdmirx_show_info(unsigned char *buf, int size)
{
	int pos = 0;
	struct drm_infoframe_st *drmpkt;

	drmpkt = (struct drm_infoframe_st *)&(rx_pkt.drm_info);

	pos += snprintf(buf+pos, size-pos,
		"HDMI info\n\n");
	if (rx.cur.colorspace == E_COLOR_RGB)
		pos += snprintf(buf+pos, size-pos,
			"Color Space: %s\n", "0-RGB");
	else if (rx.cur.colorspace == E_COLOR_YUV422)
		pos += snprintf(buf+pos, size-pos,
			"Color Space: %s\n", "1-YUV422");
	else if (rx.cur.colorspace == E_COLOR_YUV444)
		pos += snprintf(buf+pos, size-pos,
			"Color Space: %s\n", "2-YUV444");
	else if (rx.cur.colorspace == E_COLOR_YUV420)
		pos += snprintf(buf+pos, size-pos,
			"Color Space: %s\n", "3-YUV420");
	pos += snprintf(buf+pos, size-pos,
		"Dvi: %d\n", rx.cur.hw_dvi);
	pos += snprintf(buf+pos, size-pos,
		"Interlace: %d\n", rx.cur.interlaced);
	pos += snprintf(buf+pos, size-pos,
		"Htotal: %d\n", rx.cur.htotal);
	pos += snprintf(buf+pos, size-pos,
		"Hactive: %d\n", rx.cur.hactive);
	pos += snprintf(buf+pos, size-pos,
		"Vtotal: %d\n", rx.cur.vtotal);
	pos += snprintf(buf+pos, size-pos,
		"Vactive: %d\n", rx.cur.vactive);
	pos += snprintf(buf+pos, size-pos,
		"Repetition: %d\n", rx.cur.repeat);
	pos += snprintf(buf+pos, size-pos,
		"Color Depth: %d\n", rx.cur.colordepth);
	pos += snprintf(buf+pos, size-pos,
		"Frame Rate: %d\n", rx.cur.frame_rate);
	pos += snprintf(buf+pos, size-pos,
		"Skip frame: %d\n", rx.skip);
	pos += snprintf(buf+pos, size-pos,
		"avmute skip: %d\n", rx.avmute_skip);
	pos += snprintf(buf+pos, size-pos,
		"TMDS clock: %d\n", rx_measure_clock(MEASURE_CLK_TMDS));
	pos += snprintf(buf+pos, size-pos,
		"Pixel clock: %d\n", rx_measure_clock(MEASURE_CLK_PIXEL));
	if (drmpkt->des_u.tp1.eotf == EOTF_SDR)
		pos += snprintf(buf+pos, size-pos,
		"HDR EOTF: %s\n", "SDR");
	else if (drmpkt->des_u.tp1.eotf == EOTF_HDR)
		pos += snprintf(buf+pos, size-pos,
		"HDR EOTF: %s\n", "HDR");
	else if (drmpkt->des_u.tp1.eotf == EOTF_SMPTE_ST_2048)
		pos += snprintf(buf+pos, size-pos,
		"HDR EOTF: %s\n", "SMPTE_ST_2048");
	else if (drmpkt->des_u.tp1.eotf == EOTF_HLG)
		pos += snprintf(buf+pos, size-pos,
		"HDR EOTF: %s\n", "HLG");
	pos += snprintf(buf+pos, size-pos,
		"Dolby Vision: %s\n",
		(rx.vs_info_details.dolby_vision?"on":"off"));

	pos += snprintf(buf+pos, size-pos,
		"\n\nAudio info\n\n");
	pos += snprintf(buf+pos, size-pos,
		"CTS: %d\n", rx.aud_info.cts);
	pos += snprintf(buf+pos, size-pos,
		"N: %d\n", rx.aud_info.n);
	pos += snprintf(buf+pos, size-pos,
		"Recovery clock: %d\n", rx.aud_info.arc);
	pos += snprintf(buf+pos, size-pos,
		"audio receive data: %d\n", auds_rcv_sts);
	pos += snprintf(buf+pos, size-pos,
		"Audio PLL clock: %d\n", rx_measure_clock(MEASURE_CLK_AUD_PLL));
	pos += snprintf(buf+pos, size-pos,
		"mpll_div_clk: %d\n", rx_measure_clock(MEASURE_CLK_MPLL));

	pos += snprintf(buf+pos, size-pos,
		"\n\nHDCP info\n\n");
	pos += snprintf(buf+pos, size-pos,
		"HDCP Debug Value: 0x%x\n", hdmirx_rd_dwc(DWC_HDCP_DBG));
	pos += snprintf(buf+pos, size-pos,
		"HDCP14 state: %d\n", rx.cur.hdcp14_state);
	pos += snprintf(buf+pos, size-pos,
		"HDCP22 state: %d\n", rx.cur.hdcp22_state);
	if (rx.port == E_PORT0)
		pos += snprintf(buf+pos, size-pos,
			"Source Physical address: %d.0.0.0\n", 1);
	else if (rx.port == E_PORT1)
		pos += snprintf(buf+pos, size-pos,
			"Source Physical address: %d.0.0.0\n", 3);
	else if (rx.port == E_PORT2)
		pos += snprintf(buf+pos, size-pos,
			"Source Physical address: %d.0.0.0\n", 2);
	else if (rx.port == E_PORT3)
		pos += snprintf(buf+pos, size-pos,
			"Source Physical address: %d.0.0.0\n", 4);
	pos += snprintf(buf+pos, size-pos,
		"HDCP22_ON: %d\n", hdcp22_on);
	pos += snprintf(buf+pos, size-pos,
		"HDCP22 sts: 0x%x\n", rx_hdcp22_rd_reg(0x60));
	pos += snprintf(buf+pos, size-pos,
		"HDCP22_capable_sts: %d\n", hdcp22_capable_sts);
	pos += snprintf(buf+pos, size-pos,
		"sts0x8fc: 0x%x\n", hdmirx_rd_dwc(DWC_HDCP22_STATUS));
	pos += snprintf(buf+pos, size-pos,
		"sts0x81c: 0x%x\n", hdmirx_rd_dwc(DWC_HDCP22_CONTROL));
	pos += snprintf(buf+pos, size-pos,
		"edid_ver: %s\n", rx.edid_ver == EDID_V20 ? "2.0" : "1.4");

	return pos;
}

static void dump_phy_status(void)
{
	uint32_t val0, val1, val2, data32;

	rx_pr("[PHY info]\n");
	if (rx.chip_id >= CHIP_ID_TL1) {
		rx_get_error_cnt(&val0, &val1, &val2);
		rx_pr("err cnt- ch0: %d,ch1:%d ch2:%d\n", val0, val1, val2);
		rx_pr("PLL_LCK_STS(tmds valid) = 0x%x\n",
			hdmirx_rd_dwc(DWC_HDMI_PLL_LCK_STS));
		rx_pr("MISC_STAT0 sqo = 0x%x\n",
			hdmirx_rd_top(TOP_MISC_STAT0));
		rx_pr("PHY_DCHD_STAT = 0x%x\n",
			rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
		rx_pr("APLL_CNTL0 = 0x%x\n",
			rd_reg_hhi(HHI_HDMIRX_APLL_CNTL0));
		rx_pr("TMDS_ALIGN_STAT = 0x%x\n",
			hdmirx_rd_top(TOP_TMDS_ALIGN_STAT));
		rx_pr("all valid = 0x%x\n", aml_phy_tmds_valid());

		data32 = rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL1);
		data32 = data32 & 0xf0ffffff;
		wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL1, data32);
		rx_pr("0x3bc-0=%x\n", rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
		udelay(10);
		data32 = ((data32 & 0xf0ffffff) | (0x2 << 24));
		wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL1, data32);
		rx_pr("0x3bc-2=%x\n", rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
		udelay(10);
		data32 = ((data32 & 0xf0ffffff) | (0x4 << 24));
		wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL1, data32);
		rx_pr("0x3bc-4=%x\n", rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
		udelay(10);
		data32 = ((data32 & 0xf0ffffff) | (0x6 << 24));
		wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL1, data32);
		rx_pr("0x3bc-6=%x\n", rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
		udelay(10);
		data32 = ((data32 & 0xf0ffffff) | (0xe << 24));
		wr_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL1, data32);
		if (log_level & VIDEO_LOG) {
			rx_pr("0x3bc-e=%x\n",
				rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
			rx_pr("0x3bc-e=%x\n",
				rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
			rx_pr("0x3bc-e=%x\n",
				rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
			rx_pr("0x3bc-e=%x\n",
				rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
			rx_pr("0x3bc-e=%x\n",
				rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
			rx_pr("0x3bc-e=%x\n",
				rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
			rx_pr("0x3bc-e=%x\n",
				rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
			rx_pr("0x3bc-e=%x\n",
				rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
			rx_pr("0x3bc-e=%x\n",
				rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
			rx_pr("0x3bc-e=%x\n",
				rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
			rx_pr("0x3bc-e=%x\n",
				rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
			rx_pr("0x3bc-e=%x\n",
				rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
			rx_pr("0x3bc-e=%x\n",
				rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_STAT));
		}
	}
}

static void dump_clk_status(void)
{
	rx_pr("[HDMI clk info]\n");
	rx_pr("top cableclk=%d\n",
		rx_get_clock(TOP_HDMI_CABLECLK));
	rx_pr("top audio meter clk=%d\n",
		rx_get_clock(TOP_HDMI_AUDIOCLK));
	rx_pr("top tmdsclk=%d\n",
		rx_get_clock(TOP_HDMI_TMDSCLK));
	rx_pr("cable clock = %d\n",
		rx_measure_clock(MEASURE_CLK_CABLE));
	rx_pr("tmds clock = %d\n",
		rx_measure_clock(MEASURE_CLK_TMDS));
	rx_pr("Pixel clock = %d\n",
		rx_measure_clock(MEASURE_CLK_PIXEL));
	rx_pr("audio clock = %d\n",
		rx_measure_clock(MEASURE_CLK_AUD_PLL));
	rx_pr("aud clk in = %d\n",
		rx_measure_clock(MEASURE_CLK_AUD_DIV));
	rx_pr("mpll clock = %d\n",
		rx_measure_clock(MEASURE_CLK_MPLL));
	rx_pr("esm clock = %d\n",
		rx_measure_clock(MEASURE_CLK_ESM));
}

static void dump_video_status(void)
{
	rx_get_video_info();

	rx_pr("[HDMI info]\n");
	rx_pr("colorspace %d,", rx.cur.colorspace);
	rx_pr("dvi %d,", rx.cur.hw_dvi);
	rx_pr("sw_dvi:%d,", rx.cur.sw_dvi);
	rx_pr("interlace %d\n", rx.cur.interlaced);
	rx_pr("htotal %d\n", rx.cur.htotal);
	rx_pr("hactive %d\n", rx.cur.hactive);
	rx_pr("vtotal %d\n", rx.cur.vtotal);
	rx_pr("vactive %d\n", rx.cur.vactive);
	rx_pr("repetition %d\n", rx.cur.repeat);
	rx_pr("colordepth %d\n", rx.cur.colordepth);
	rx_pr("frame_rate %d\n", rx.cur.frame_rate);
	rx_pr("fmt=0x%x,", hdmirx_hw_get_fmt());
	rx_pr("hw_vic %d,", rx.cur.hw_vic);
	rx_pr("sw_vic %d,", rx.pre.sw_vic);
	rx_pr("rx.no_signal=%d,rx.state=%d,",
			rx.no_signal, rx.state);
	rx_pr("skip frame=%d\n", rx.skip);
	rx_pr("avmute_skip:0x%x\n", rx.avmute_skip);

	rx_pr("****vs_info_details:*****\n");
	rx_pr("hdr10plus = %d\n", rx.vs_info_details.hdr10plus);
	rx_pr("allm_mode = %d\n", rx.vs_info_details.allm_mode);
	rx_pr("dolby_vision = %d\n", rx.vs_info_details.dolby_vision);
	rx_pr("dv ll = %d\n", rx.vs_info_details.low_latency);

	rx_pr("DRM = %d\n", rx_pkt_chk_attach_drm());

	rx_pr("phy addr: %#x,%#x,port: %d, up phy addr:%#x\n",
		hdmirx_rd_top(TOP_EDID_RAM_OVR1_DATA),
		hdmirx_rd_top(TOP_EDID_RAM_OVR2_DATA),
			rx.port, up_phy_addr);
	dump_clk_status();
	rx_pr("eq=%x\n", (rd_reg_hhi(HHI_HDMIRX_PHY_DCHD_CNTL1)>>4)&0xffff);
	rx_pr("edid_ver: %s\n", rx.edid_ver == EDID_V20 ? "2.0" : "1.4");
}

static void dump_audio_status(void)
{
	static struct aud_info_s a;
	uint32_t val0, val1;

	rx_get_audinfo(&a);
	rx_pr("[AudioInfo]\n");
	rx_pr(" CT=%u CC=%u", a.coding_type,
			a.channel_count);
	rx_pr(" SF=%u SS=%u", a.sample_frequency,
			a.sample_size);
	rx_pr(" CA=%u\n", a.auds_ch_alloc);
	rx_pr("CTS=%d, N=%d,", a.cts, a.n);
	rx_pr("acr clk=%d\n", a.arc);
	if (rx.chip_id >= CHIP_ID_TL1) {
		rx_get_audio_N_CTS(&val0, &val1);
		rx_pr("top CTS:%d, N:%d\n", val1, val0);
	}
	rx_pr("audio receive data:%d\n",
		auds_rcv_sts);
}

static void dump_hdcp_status(void)
{
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
	}

	rx_pr("ESM clock = %d\n",
		rx_measure_clock(MEASURE_CLK_ESM));
	rx_pr("HDCP debug value=0x%x\n",
	hdmirx_rd_dwc(DWC_HDCP_DBG));
	rx_pr("HDCP14 state:%d\n",
		rx.cur.hdcp14_state);
	rx_pr("HDCP22 state:%d\n",
		rx.cur.hdcp22_state);

	rx_pr("\n hdcp-seed = %d ",
		rx.hdcp.seed);
	/* KSV CONFIDENTIAL */
	rx_pr("hdcp-bksv = %x---%x\n",
		hdmirx_rd_dwc(DWC_HDCP_BKSV1),
		hdmirx_rd_dwc(DWC_HDCP_BKSV0));
}

void dump_state(int enable)
{
	rx_get_video_info();
	if (enable == RX_DUMP_VIDEO) /* video info */
		dump_video_status();
	else if (enable & RX_DUMP_ALL) {
		dump_clk_status();
		dump_phy_status();
		dump_video_status();
		dump_audio_status();
		dump_hdcp_status();
	} else if (enable & RX_DUMP_AUDIO) /* audio info */
		dump_audio_status();
	else if (enable & RX_DUMP_HDCP)	/* hdcp info */
		dump_hdcp_status();
	else if (enable & RX_DUMP_PHY)	/* phy info */
		dump_phy_status();
	else if (enable & RX_DUMP_CLK)	/* clk src info */
		dump_clk_status();
	else
		dump_video_status();
}

void rx_debug_help(void)
{
	rx_pr("*****************\n");
	rx_pr("reset0--hw_config\n");
	rx_pr("reset1--8bit phy rst\n");
	rx_pr("reset3--irq open\n");
	rx_pr("reset4--edid_update\n");
	rx_pr("reset5--esm rst\n");
	rx_pr("database--esm data addr\n");
	rx_pr("duk--dump duk\n");
	rx_pr("v - driver version\n");
	rx_pr("state0 -dump video\n");
	rx_pr("state1 -dump audio\n");
	rx_pr("state2 -dump hdcp\n");
	rx_pr("state3 -dump phy\n");
	rx_pr("state4 -dump clock\n");
	rx_pr("statex -dump all\n");
	rx_pr("port1/2/3 -port swich\n");
	rx_pr("hpd0/1 -set hpd 0:low\n");
	rx_pr("cable_status -5V sts\n");
	rx_pr("pause -pause fsm\n");
	rx_pr("reg -dump all dwc reg\n");
	rx_pr("*****************\n");
}

int hdmirx_debug(const char *buf, int size)
{
	char tmpbuf[128];
	int i = 0;
	uint32_t value = 0;

	char input[5][20];
	char *const delim = " ";
	char *token;
	char *cur;
	int cnt = 0;
	struct edid_info_s edid_info;

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;

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
		rx_debug_help();
	} else if (strncmp(tmpbuf, "hpd", 3) == 0)
		rx_set_cur_hpd((tmpbuf[3] == '0' ? 0 : 1), 4);
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
			hdmi_rx_top_edid_update();
		} else if (tmpbuf[5] == '1') {
			rx_pr(" hdmirx phy init 8bit\n");
			hdmirx_phy_init();
		} else if (tmpbuf[5] == '3') {
			rx_pr(" irq open\n");
			rx_irq_en(true);
		} else if (tmpbuf[5] == '4') {
			rx_pr(" edid update\n");
			hdmi_rx_top_edid_update();
		} else if (tmpbuf[5] == '5') {
			hdmirx_hdcp22_esm_rst();
		}
	} else if (strncmp(tmpbuf, "state", 5) == 0) {
		if (tmpbuf[5] == '0')
			dump_state(RX_DUMP_VIDEO);
		else if (tmpbuf[5] == '1')
			dump_state(RX_DUMP_AUDIO);
		else if (tmpbuf[5] == '2')
			dump_state(RX_DUMP_HDCP);
		else if (tmpbuf[5] == '3')
			dump_state(RX_DUMP_PHY);
		else if (tmpbuf[5] == '4')
			dump_state(RX_DUMP_CLK);
		else
			dump_state(RX_DUMP_ALL);
	} else if (strncmp(tmpbuf, "pause", 5) == 0) {
		if (kstrtou32(tmpbuf + 5, 10, &value) < 0)
			return -EINVAL;
		rx_pr("%s\n", value ? "pause" : "enable");
		sm_pause = value;
	} else if (strncmp(tmpbuf, "reg", 3) == 0) {
		dump_reg();
	}  else if (strncmp(tmpbuf, "duk", 3) == 0) {
		rx_pr("hdcp22=%d\n", rx_sec_set_duk(hdmirx_repeat_support()));
	} else if (strncmp(tmpbuf, "edid", 4) == 0) {
		dump_edid_reg();
	} else if (strncmp(tmpbuf, "load14key", 7) == 0) {
		rx_debug_loadkey();
	} else if (strncmp(tmpbuf, "load22key", 9) == 0) {
		rx_debug_load22key();
	} else if (strncmp(tmpbuf, "esm0", 4) == 0) {
		/* switch_set_state(&rx.hpd_sdev, 0x0); */
		extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 0);
	} else if (strncmp(tmpbuf, "esm1", 4) == 0) {
		/*switch_set_state(&rx.hpd_sdev, 0x01);*/
		extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 1);
	} else if (strncmp(input[0], "pktinfo", 7) == 0) {
		rx_debug_pktinfo(input);
	} else if (strncmp(tmpbuf, "parse_edid", 10) == 0) {
		memset(&edid_info, 0, sizeof(struct edid_info_s));
		rx_edid_parse(edid_temp, &edid_info);
		rx_edid_parse_print(&edid_info);
		rx_blk_index_print(&edid_info.cea_ext_info.blk_parse_info);
	} else if (strncmp(tmpbuf, "parse_capds", 11) == 0) {
		rx_prase_earc_capds_dbg();
	} else if (strncmp(tmpbuf, "splice_capds", 12) == 0) {
		edid_splice_earc_capds_dbg(edid_temp);
	} else if (strncmp(tmpbuf, "splice_db", 9) == 0) {
		if (kstrtou32(tmpbuf + 9, 16, &value) < 0)
			return -EINVAL;
		edid_splice_data_blk_dbg(edid_temp, value);
	} else if (strncmp(tmpbuf, "rm_db_tag", 9) == 0) {
		if (kstrtou32(tmpbuf + 9, 16, &value) < 0)
			return -EINVAL;
		edid_rm_db_by_tag(edid_temp, value);
	} else if (strncmp(tmpbuf, "rm_db_idx", 9) == 0) {
		if (kstrtou32(tmpbuf + 9, 16, &value) < 0)
			return -EINVAL;
		edid_rm_db_by_idx(edid_temp, value);
	} else if (tmpbuf[0] == 'w') {
		rx_debug_wr_reg(buf, tmpbuf, i);
	} else if (tmpbuf[0] == 'r') {
		rx_debug_rd_reg(buf, tmpbuf);
	} else if (tmpbuf[0] == 'v') {
		rx_pr("------------------\n");
		rx_pr("Hdmirx version0: %s\n", RX_VER0);
		rx_pr("Hdmirx version1: %s\n", RX_VER1);
		rx_pr("Hdmirx version2: %s\n", RX_VER2);
		rx_pr("Hdmirx version2: %s\n", "ver.2019/11/18");
		rx_pr("------------------\n");
	} else if (strncmp(input[0], "port0", 5) == 0) {
		hdmirx_open_port(TVIN_PORT_HDMI0);
		signal_status_init();
		rx.open_fg = 1;
	} else if (strncmp(input[0], "port1", 5) == 0) {
		hdmirx_open_port(TVIN_PORT_HDMI1);
		signal_status_init();
		rx.open_fg = 1;
	} else if (strncmp(input[0], "port2", 5) == 0) {
		hdmirx_open_port(TVIN_PORT_HDMI2);
		signal_status_init();
		rx.open_fg = 1;
	} else if (strncmp(input[0], "port3", 5) == 0) {
		hdmirx_open_port(TVIN_PORT_HDMI3);
		signal_status_init();
		rx.open_fg = 1;
	} else if (strncmp(input[0], "empsts", 6) == 0) {
		rx_emp_status();
	} else if (strncmp(input[0], "empstart", 8) == 0) {
		rx_emp_capture_stop();
		rx_emp_resource_allocate(hdmirx_dev);
		rx_emp_to_ddr_init();
	} else if (strncmp(input[0], "tmdsstart", 9) == 0) {
		rx_emp_capture_stop();
		rx_tmds_resource_allocate(hdmirx_dev);
		rx_tmds_to_ddr_init();
	} else if (strncmp(input[0], "empcapture", 10) == 0) {
		rx_emp_data_capture();
	} else if (strncmp(input[0], "tmdscapture", 11) == 0) {
		rx_tmds_data_capture();
	} else if (strncmp(input[0], "tmdscnt", 7) == 0) {
		if (kstrtou32(input[1], 16, &value) < 0)
			rx_pr("error input Value\n");
		rx_pr("set pkt cnt:0x%x\n", value);
		rx.empbuff.tmdspktcnt = value;
	} else if (strncmp(input[0], "phyinit", 7) == 0) {
		aml_phy_bw_switch();
	} else if (strncmp(input[0], "phyeq", 5) == 0) {
		//aml_eq_setting();
		find_best_eq = 0x1111;
		rx.phy.err_sum = 0xffffff;
	} else if (strncmp(tmpbuf, "audio", 5) == 0) {
		hdmirx_audio_fifo_rst();
	} else if (strncmp(tmpbuf, "eqcal", 5) == 0)
		rx_phy_rt_cal();

	return 0;
}

void rx_dw_edid_monitor(void)
{
	if ((!hdmi_cec_en) || (hdmirx_repeat_support()))
		return;
	if (tx_hpd_event == E_RCV) {
		if (rx.open_fg)
			fsm_restart();
		rx_set_port_hpd(ALL_PORTS, 0);
		hdmi_rx_top_edid_update();
		hpd_wait_cnt = 0;
		tx_hpd_event = E_EXE;
	} else if (tx_hpd_event == E_EXE) {
		if (!rx.open_fg)
			hpd_wait_cnt++;
		if (!rx_hpd_keep_low()) {
			rx_set_port_hpd(ALL_PORTS, 1);
			tx_hpd_event = E_IDLE;
		}
	}
}

void hdmirx_timer_handler(unsigned long arg)
{
	struct hdmirx_dev_s *devp = (struct hdmirx_dev_s *)arg;

	if (term_flag && term_cal_en) {
		rx_phy_rt_cal();
		term_flag = 0;
	}
	rx_5v_monitor();
	rx_check_repeat();
	rx_dw_edid_monitor();
	if (rx.open_fg) {
		rx_nosig_monitor();
		if (!hdmirx_repeat_support() || !rx.firm_change) {
			if (!sm_pause) {
				rx_clkrate_monitor();
				rx_main_state_machine();
			}
			/* rx_pkt_check_content(); */
			rx_err_monitor();
			#ifdef K_TEST_CHK_ERR_CNT
			if (err_chk_en)
				rx_monitor_error_counter();
			rx_get_best_eq_setting();
			#endif
		}
	}
	devp->timer.expires = jiffies + TIMER_STATE_CHECK;
	add_timer(&devp->timer);
}

