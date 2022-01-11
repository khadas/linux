// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/sched/clock.h>

/* Local include */
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_eq.h"
#include "hdmi_rx_wrapper.h"
#include "hdmi_rx_pktinfo.h"
#include "hdmi_rx_edid.h"
#include "hdmi_rx_drv_ext.h"

static int pll_unlock_cnt;
static int pll_unlock_max;
static int pll_lock_cnt;
static int pll_lock_max;
static int dwc_rst_wait_cnt;
static int dwc_rst_wait_cnt_max;
static int sig_stable_cnt;
static int sig_stable_max;
static int sig_stable_err_cnt;
static int sig_stable_err_max;
static int err_cnt_sum_max;
static int hpd_wait_cnt;
static int hpd_wait_max;
static int sig_unstable_cnt;
static int sig_unstable_max;
static int sig_unready_cnt;
static int sig_unready_max;
static int pow5v_max_cnt;
int rgb_quant_range;
int yuv_quant_range;
int it_content;
static int diff_pixel_th;
static int diff_line_th;
static int diff_frame_th;
static int err_dbg_cnt;
static int err_dbg_cnt_max;
static int aud_sr_stb_max;
static int audio_coding_type;
static int audio_channel_count;
int irq_err_max = 30;
int clk_unstable_cnt;
static int clk_unstable_max;
int clk_stable_cnt;
static int clk_stable_max;
static int unnormal_wait_max = 200;
static int wait_no_sig_max = 600;
u8 vrr_func_en = 1;

typedef void (*pf_callback)(int earc_port, bool st);
static pf_callback earc_hdmirx_hpdst;
static cec_callback cec_hdmirx5v_update;

/* for debug */
bool vic_check_en;
bool dvi_check_en;
//bool clk_debug_en;
u32 rx5v_debug_en;
u32 err_chk_en;
u32 force_vic;
int log_level = LOG_EN;
u32 dbg_cs;
u32 dbg_pkt;// = 0x81;
u32 check_cor_irq_via_vsync;
/* used in other module */
static int audio_sample_rate;

static int auds_rcv_sts;
module_param(auds_rcv_sts, int, 0664);
MODULE_PARM_DESC(auds_rcv_sts, "auds_rcv_sts");

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

bool esm_auth_fail_en;
MODULE_PARM_DESC(esm_auth_fail_en, "\n esm_auth_fail_en\n");
module_param(esm_auth_fail_en, bool, 0664);

/* to inform hdcp_rx22 whether there's any device connected */
u32 pwr_sts_to_esm;
static int hdcp22_capable_sts = 0xff;
bool esm_error_flag;

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

int hdcp22_auth_sts = 0xff;
MODULE_PARM_DESC(hdcp22_auth_sts, "\n hdcp22_auth_sts\n");
module_param(hdcp22_auth_sts, int, 0664);

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
u32 hdcp22_reauth_enable = 1;
unsigned int edid_update_flag;
unsigned int downstream_hpd_flag;
static u32 hdcp22_stop_auth_enable;
static u32 hdcp22_esm_reset2_enable;
int sm_pause;
int pre_port = 0xff;
u32 vsync_err_cnt_max = 10;
/* waiting time cannot be reduced 10*10*/
/* it will cause hdcp1.4 cts fail */
static int hdcp_none_wait_max = 10;
static int esd_phy_rst_cnt;
static int esd_phy_rst_max;
static int cec_dev_info;
struct rx_s rx;
static bool term_flag = 1;
/* vpp mute when signal change, used
 * in companion with vlock phase = 84
 */
u32 vpp_mute_enable = 1;
int clk_chg_cnt;
int clk_chg_max = 3;

void hdmirx_phy_var_init(void)
{
	if (rx.phy_ver == PHY_VER_TM2) {
		/* for tm2_b */
		rx.aml_phy.dfe_en = 1;
		rx.aml_phy.ofst_en = 1;
		rx.aml_phy.cdr_mode = 0;
		rx.aml_phy.pre_int = 1;
		rx.aml_phy.pre_int_en = 0;
		rx.aml_phy.phy_bwth = 1;
		rx.aml_phy.alirst_en = 0;
		rx.aml_phy.tap1_byp = 1;
		rx.aml_phy.eq_byp = 0;
		rx.aml_phy.long_cable = 1;
		rx.aml_phy.osc_mode = 0;
		rx.aml_phy.pll_div = 1;
		rx.aml_phy.sqrst_en = 0;
		rx.aml_phy.vga_dbg = 1;
		rx.aml_phy.vga_dbg_delay = 200;
		rx.aml_phy.eq_fix_val = 16;
		rx.aml_phy.cdr_fr_en = 0;
		rx.aml_phy.force_sqo = 0;
	} else if (rx.phy_ver >= PHY_VER_T5) {
		rx.aml_phy.dfe_en = 1;
		rx.aml_phy.ofst_en = 0;
		rx.aml_phy.cdr_mode = 0;
		rx.aml_phy.pre_int = 1;
		rx.aml_phy.pre_int_en = 0;
		rx.aml_phy.phy_bwth = 1;
		rx.aml_phy.alirst_en = 0;
		rx.aml_phy.tap1_byp = 1;
		rx.aml_phy.eq_byp = 1;
		rx.aml_phy.long_cable = 1;
		rx.aml_phy.osc_mode = 0;
		rx.aml_phy.pll_div = 1;
		rx.aml_phy.sqrst_en = 0;
		rx.aml_phy.vga_dbg = 1;
		rx.aml_phy.vga_dbg_delay = 200;
		rx.aml_phy.eq_fix_val = 16;
		rx.aml_phy.cdr_fr_en = 100;
		rx.aml_phy.force_sqo = 0;
		/* add for t5 */
		rx.aml_phy.os_rate = 3;
		rx.aml_phy.cdr_mode = 1;
		rx.aml_phy.vga_gain = 0x1000;
		rx.aml_phy.eq_stg1 = 0x1f;
		rx.aml_phy.eq_stg2 = 0x1f;
		rx.aml_phy.eq_hold = 1;
		rx.aml_phy.dfe_hold  = 0;
		rx.aml_phy.eye_delay = 1000;
		rx.aml_phy.eq_retry = 1;
		rx.aml_phy.tap2_byp = 0;
		rx.aml_phy.long_bist_en = 0;
	}
}

void hdmirx_fsm_var_init(void)
{
	switch (rx.chip_id) {
	case CHIP_ID_TL1:
	case CHIP_ID_TM2:
		sig_stable_err_max = 5;
		sig_stable_max = 10;
		dwc_rst_wait_cnt_max = 1;
		clk_unstable_max = 100;
		esd_phy_rst_max = 8;
		pll_unlock_max = 30;
		stable_check_lvl = 0x7cf;
		pll_lock_max = 5;
		err_cnt_sum_max = 10;
		/* increase time of hpd low, to avoid some source like */
		/* MTK box/KaiboerH9 i2c communicate error */
		hpd_wait_max = 40;
		sig_unstable_max = 20;
		sig_unready_max = 5;
		pow5v_max_cnt = 3;
		diff_pixel_th = 2;
		diff_line_th = 2;
		/* (25hz-24hz)/2 = 50/100 */
		diff_frame_th = 40;
		aud_sr_stb_max = 30;
		err_dbg_cnt_max = 500;
		clk_stable_max = 3;
		break;
	case CHIP_ID_T5:
	case CHIP_ID_T5D:
		sig_stable_err_max = 5;
		sig_stable_max = 10;
		dwc_rst_wait_cnt_max = 30;
		clk_unstable_max = 100;
		esd_phy_rst_max = 8;
		pll_unlock_max = 30;
		stable_check_lvl = 0x7cf;
		pll_lock_max = 5;
		err_cnt_sum_max = 10;
		hpd_wait_max = 40;
		sig_unstable_max = 20;
		sig_unready_max = 5;
		pow5v_max_cnt = 3;
		diff_pixel_th = 2;
		diff_line_th = 2;
		diff_frame_th = 40;
		aud_sr_stb_max = 30;
		err_dbg_cnt_max = 500;
		clk_stable_max = 3;
		break;
	case CHIP_ID_GXTVBB:
	case CHIP_ID_TXL:
	case CHIP_ID_TXLX:
	case CHIP_ID_TXHD:
		sig_stable_err_max = 5;
		sig_stable_max = 10;
		dwc_rst_wait_cnt_max = 110;
		clk_unstable_max = 100;
		esd_phy_rst_max = 4;
		pll_unlock_max = 30;
		stable_check_lvl = 0x17df;
		pll_lock_max = 2;
		err_cnt_sum_max = 10;
		hpd_wait_max = 40;
		sig_unstable_max = 20;
		sig_unready_max = 5;
		pow5v_max_cnt = 3;
		diff_pixel_th = 2;
		diff_line_th = 2;
		diff_frame_th = 40;
		aud_sr_stb_max = 30;
		err_dbg_cnt_max = 500;
		clk_stable_max = 3;
		break;
	case CHIP_ID_T7:
	case CHIP_ID_T3:
	case CHIP_ID_T5W:
	default:
		hbr_force_8ch = 1; //use it to enable hdr2spdif
		sig_stable_err_max = 5;
		sig_stable_max = 10;
		dwc_rst_wait_cnt_max = 30;
		clk_unstable_max = 100;
		esd_phy_rst_max = 8;
		pll_unlock_max = 30;
		stable_check_lvl = 0x7d3;
		pll_lock_max = 2;
		err_cnt_sum_max = 10;
		hpd_wait_max = 40;
		sig_unstable_max = 20;
		sig_unready_max = 5;
		/* decreased to 2 */
		pow5v_max_cnt = 2;
		/* decreased to 2 */
		diff_pixel_th = 1;
		/* decreased to 2 */
		diff_line_th = 1;
		/* (25hz-24hz)/2 = 50/100 */
		diff_frame_th = 40;
		aud_sr_stb_max = 30;
		err_dbg_cnt_max = 500;
		clk_stable_max = 3;
		break;
	}
}

void hdmirx_drv_var_init(void)
{
	rx.var.dbg_ve = 0;
	/* Per hdmi1.4 spec chapter8.2.1: a hdmi source shall always
	 * transmit an AVI InfoFrame at least once per two video fields.
	 * So better to check AVI packets reception per >=2 frames.
	 * However the check frame count affects the delay time before
	 * recovery, so a empirical value is used.
	 */
	rx.var.avi_chk_frames = 4;
}

void hdmirx_init_params(void)
{
	/* for fsm var init */
	hdmirx_fsm_var_init();
	/* for phy variable */
	hdmirx_phy_var_init();
	/* for hw/drv var init*/
	hdmirx_drv_var_init();
}

void rx_hpd_to_esm_handle(struct work_struct *work)
{
	cancel_delayed_work(&esm_dwork);
	/* switch_set_state(&rx.hpd_sdev, 0x0); */
	/* extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 0); */
	rx_hdcp22_send_uevent(0);
	rx_pr("esm_hpd-0\n");
	msleep(80);
	/* switch_set_state(&rx.hpd_sdev, 0x01); */
	/* extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 1); */
	rx_hdcp22_send_uevent(1);
	rx_pr("esm_hpd-1\n");
}

int cec_set_dev_info(u8 dev_idx)
{
	cec_dev_info |= 1 << dev_idx;

	if (dev_idx == 1)
		hdcp_enc_mode = 1;
	if (dev_idx == 2 && cec_dev_en)
		dev_is_appletv_v2 = 1;
	rx_pr("cec special dev = %x", dev_idx);
	return 0;
}
EXPORT_SYMBOL(cec_set_dev_info);

int register_earctx_callback(pf_callback callback)
{
	earc_hdmirx_hpdst = callback;
	return 0;
}
EXPORT_SYMBOL(register_earctx_callback);

void unregister_earctx_callback(void)
{
	earc_hdmirx_hpdst = NULL;
}
EXPORT_SYMBOL(unregister_earctx_callback);

int register_cec_callback(cec_callback callback)
{
	cec_hdmirx5v_update = callback;
	return 0;
}
EXPORT_SYMBOL(register_cec_callback);

void unregister_cec_callback(void)
{
	cec_hdmirx5v_update = NULL;
}
EXPORT_SYMBOL(unregister_cec_callback);

static bool video_mute_enabled(void)
{
	if (rx.state != FSM_SIG_READY)
		return false;

	/* for debug with flicker issues, especially
	 * unplug or switch timing under game mode
	 */
	if (vpp_mute_enable)
		return true;
	else
		return false;
}

/*
 *func: irq tasklet
 *param: flag:
 *	0x01:	audio irq
 *	0x02:	packet irq
 */
void rx_tasklet_handler(unsigned long arg)
{
	struct rx_s *prx = (struct rx_s *)arg;
	u8 irq_flag = prx->irq_flag;

	/* prx->irq_flag = 0; /
	 * if (irq_flag & IRQ_PACKET_FLAG)
	 *	rx_pkt_handler(PKT_BUFF_SET_FIFO);
	 */
	/*pkt overflow or underflow*/
	if (irq_flag & IRQ_PACKET_ERR) {
		hdmirx_packet_fifo_rst();
		irq_flag &= ~IRQ_PACKET_ERR;
	}

	if (irq_flag & IRQ_AUD_FLAG) {
		hdmirx_audio_fifo_rst();
		irq_flag &= ~IRQ_AUD_FLAG;
	}
	prx->irq_flag = irq_flag;
	/*irq_flag = 0;*/
}

static int hdmi_rx_ctrl_irq_handler(void)
{
	int error = 0;
	/* unsigned i = 0; */
	u32 intr_hdmi = 0;
	u32 intr_md = 0;
	u32 intr_pedc = 0;
	/* u32 intr_aud_clk = 0; */
	u32 intr_aud_fifo = 0;
	u32 intr_hdcp22 = 0;
	bool vsi_handle_flag = false;
	bool drm_handle_flag = false;
	bool emp_handle_flag = false;
	u32 rx_top_intr_stat = 0;
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
	if (intr_hdcp22 != 0) {
		hdmirx_wr_dwc(DWC_HDMI2_ICLR, intr_hdcp22);
		skip_frame(skip_frame_cnt);
		rx_pr("intr=%#x\n", intr_hdcp22);
		switch (intr_hdcp22) {
		case _BIT(0):
			hdcp22_capable_sts = HDCP22_AUTH_STATE_CAPABLE;
			/* rx.hdcp.hdcp_version = HDCP_VER_22; */
			break;
		case _BIT(1):
			hdcp22_capable_sts = HDCP22_AUTH_STATE_NOT_CAPABLE;
			break;
		case _BIT(2):
			hdcp22_auth_sts = HDCP22_AUTH_STATE_LOST;
			break;
		case _BIT(3):
			hdcp22_auth_sts = HDCP22_AUTH_STATE_SUCCESS;
			rx.hdcp.hdcp_version = HDCP_VER_22;
			rx.hdcp.hdcp_source = true;
			break;
		case _BIT(4):
			hdcp22_auth_sts = HDCP22_AUTH_STATE_FAILED;
			break;
		case _BIT(5):
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
		if (log_level & DBG_LOG)
			rx_pr("[isr] ignore dwc isr ---\n");
		return error;
	}

	if (intr_hdmi != 0) {
		if (rx_get_bits(intr_hdmi, AKSV_RCV) != 0) {
			/*if (log_level & HDCP_LOG)*/
			rx_pr("[*aksv*\n");
			rx.hdcp.hdcp_version = HDCP_VER_14;
			rx.hdcp.hdcp_source = true;
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
		if (rx_get_bits(intr_pedc, AVI_RCV) != 0) {
			if (log_level & 0x400)
				rx_pr("[irq] AVI_RCV\n");
			if (rx.var.de_stable)
				rx.var.avi_rcv_cnt++;
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
			if (log_level & VSI_LOG)
				rx_pr("[irq] FIFO START\n");
			/* rx.irq_flag |= IRQ_PACKET_FLAG; */
		}
		if (rx_get_bits(intr_pedc, PD_FIFO_NEW_ENTRY) != 0) {
			if (log_level & VSI_LOG)
				rx_pr("[irq] FIFO NEW_ENTRY\n");
			rx.irq_flag |= IRQ_PACKET_FLAG;
			/* rx_pkt_handler(PKT_BUFF_SET_FIFO); */
		}
		if (rx_get_bits(intr_pedc, _BIT(1)) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] FIFO MAX\n");
		}
		if (rx_get_bits(intr_pedc, _BIT(0)) != 0) {
			if (log_level & 0x200)
				rx_pr("[irq] FIFO MIN\n");
		}
		if (rx_get_bits(intr_pedc, GCP_AV_MUTE_CHG) != 0) {
			if (log_level & 0x100)
				rx_pr("[irq] GCP_AV_MUTE_CHG\n");
		}

		if (rx.chip_id >= CHIP_ID_TL1) {
			if (rx_get_bits(intr_pedc,
					_BIT(9)) != 0) {
				if (log_level & IRQ_LOG)
					rx_pr("[irq] EMP_RCV %#x\n",
					      intr_pedc);
				emp_handle_flag = true;
			} else if (rx_get_bits(intr_pedc,
					DRM_RCV_EN_TXLX) != 0) {
				if (log_level & IRQ_LOG)
					rx_pr("[irq] DRM_RCV_EN %#x\n",
					      intr_pedc);
				drm_handle_flag = true;
			}
		} else if (rx.chip_id == CHIP_ID_TXLX) {
			if (rx_get_bits(intr_pedc,
					DRM_RCV_EN_TXLX) != 0) {
				if (log_level & IRQ_LOG)
					rx_pr("[irq] DRM_RCV_EN %#x\n",
					      intr_pedc);
				drm_handle_flag = true;
			}
		} else {
			if (rx_get_bits(intr_pedc,
					DRM_RCV_EN) != 0) {
				if (log_level & IRQ_LOG)
					rx_pr("[irq] DRM_RCV_EN %#x\n",
					      intr_pedc);
				drm_handle_flag = true;
			}
		}
		if (rx_get_bits(intr_pedc, PD_FIFO_OVERFL) != 0) {
			if (log_level & VSI_LOG)
				rx_pr("[irq] PD_FIFO_OVERFL\n");
			rx.irq_flag |= IRQ_PACKET_ERR;
			hdmirx_packet_fifo_rst();
		}
		if (rx_get_bits(intr_pedc, PD_FIFO_UNDERFL) != 0) {
			if (log_level & VSI_LOG)
				rx_pr("[irq] PD_FIFO_UNDFLOW\n");
			rx.irq_flag |= IRQ_PACKET_ERR;
			hdmirx_packet_fifo_rst();
		}
	}

	if (intr_aud_fifo != 0) {
		if (rx_get_bits(intr_aud_fifo, OVERFL) != 0) {
			if (log_level & 0x100)
				rx_pr("[irq] OVERFL\n");
			/* rx.irq_flag |= IRQ_AUD_FLAG; */
			/* when afifo overflow in multi-channel case(VG-877),
			 * then store all subpkts into afifo, 8ch in and 8ch out
			 */
			if (rx.aud_info.auds_layout)
				rx_afifo_store_all_subpkt(true);
			else
				rx_afifo_store_all_subpkt(false);
			//if (rx.aud_info.real_sr != 0)
			error |= hdmirx_audio_fifo_rst();
		}
		if (rx_get_bits(intr_aud_fifo, UNDERFL) != 0) {
			if (log_level & 0x100)
				rx_pr("[irq] UNDERFL\n");
			/* rx.irq_flag |= IRQ_AUD_FLAG; */
			rx_afifo_store_all_subpkt(false);
			//if (rx.aud_info.real_sr != 0)
			error |= hdmirx_audio_fifo_rst();
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

static int hdmi_rx_ctrl_irq_handler_t7(void)
{
	int error = 0;
	u8 intr_0 = 0;
	u8 intr_1 = 0;
	u8 intr_2 = 0;
	u8 intr_3 = 0;
	u8 intr_4 = 0;
	u8 intr_5 = 0;
	u8 intr_6 = 0;
	u8 rx_intr_1 = 0;
	u8 rx_intr_2 = 0;
	u8 rx_intr_3 = 0;
	u8 rx_intr_4 = 0;
	u8 rx_intr_5 = 0;
	//u8 rx_intr_6 = 0;
	//u8 rx_intr_7 = 0;
	//u8 rx_intr_8 = 0;
	//u8 rx_intr_9 = 0;
	//u8 rx_intr_10 = 0;
	//u8 rx_intr_11 = 0;
	//u8 rx_intr_12 = 0;
	//u8 rx_intr_13 = 0;
	//u8 rx_intr_14 = 0;
	u8 rx_depack2_intr0;
	u8 rx_depack2_intr1;
	u8 rx_depack2_intr2;
	u8 grp_intr1;
	u8 rx_hdcp1x_intr0;
	u8 rx_hdcp2x_intr0;
	u8 rx_hdcp2x_intr1;
	u8 hdcp_2x_ecc_intr;
	//bool vsi_handle_flag = false;
	//bool drm_handle_flag = false;
	//bool emp_handle_flag = false;
	//u32 rx_top_intr_stat = 0;
	//bool irq_need_clr = 0;
	/* clear interrupt quickly */
	hdcp_2x_ecc_intr = hdmirx_rd_cor(HDCP2X_RX_ECC_INTR);
	if (hdcp_2x_ecc_intr != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("1-%x\n", hdcp_2x_ecc_intr);
		hdmirx_wr_cor(HDCP2X_RX_ECC_INTR, hdcp_2x_ecc_intr);
	}
	grp_intr1 = hdmirx_rd_cor(RX_GRP_INTR1_STAT_PWD_IVCRX);
	if (grp_intr1 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("grp_intr1-%x\n", grp_intr1);
		hdmirx_wr_cor(RX_GRP_INTR1_STAT_PWD_IVCRX, grp_intr1);
	}
	intr_0 = hdmirx_rd_cor(RX_DEPACK_INTR0_DP2_IVCRX);
	if (intr_0 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("3-%x\n", intr_0);
		hdmirx_wr_cor(RX_DEPACK_INTR0_DP2_IVCRX, intr_0);
	}
	intr_1 = hdmirx_rd_cor(RX_DEPACK_INTR1_DP2_IVCRX);
	if (intr_1 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("4-%x\n", intr_1);
		hdmirx_wr_cor(RX_DEPACK_INTR1_DP2_IVCRX, intr_1);
	}
	intr_2 = hdmirx_rd_cor(RX_DEPACK_INTR2_DP2_IVCRX);
	if (intr_2 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("5-%x\n", intr_2);
		hdmirx_wr_cor(RX_DEPACK_INTR2_DP2_IVCRX, intr_2);
	}
	intr_3 = hdmirx_rd_cor(RX_DEPACK_INTR3_DP2_IVCRX);
	if (intr_3 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("6-%x\n", intr_3);
		hdmirx_wr_cor(RX_DEPACK_INTR3_DP2_IVCRX, intr_3);
	}
	intr_4 = hdmirx_rd_cor(RX_DEPACK_INTR4_DP2_IVCRX);
	if (intr_4 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("7-%x\n", intr_4);
		hdmirx_wr_cor(RX_DEPACK_INTR4_DP2_IVCRX, intr_4);
	}
	intr_5 = hdmirx_rd_cor(RX_DEPACK_INTR5_DP2_IVCRX);
	if (intr_5 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("8-%x\n", intr_5);
		hdmirx_wr_cor(RX_DEPACK_INTR5_DP2_IVCRX, intr_5);
	}
	intr_6 = hdmirx_rd_cor(RX_DEPACK_INTR6_DP2_IVCRX);
	if (intr_6 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("9-%x\n", intr_6);
		hdmirx_wr_cor(RX_DEPACK_INTR6_DP2_IVCRX, intr_6);
	}
	rx_intr_1 = hdmirx_rd_cor(RX_INTR1_PWD_IVCRX);
	if (rx_intr_1 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("10-%x\n", rx_intr_1);
		hdmirx_wr_cor(RX_INTR1_PWD_IVCRX, rx_intr_1);
	}
	rx_intr_2 = hdmirx_rd_cor(RX_INTR2_PWD_IVCRX);
	if (rx_intr_2 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("11-%x\n", rx_intr_2);
		hdmirx_wr_cor(RX_INTR2_PWD_IVCRX, rx_intr_2);
	}
	rx_intr_3 = hdmirx_rd_cor(RX_INTR3_PWD_IVCRX);
	if (rx_intr_3 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("12-%x\n", rx_intr_3);
		hdmirx_wr_cor(RX_INTR3_PWD_IVCRX, rx_intr_3);
	}
	rx_intr_4 = hdmirx_rd_cor(RX_INTR4_PWD_IVCRX);
	if (rx_intr_4 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("13-%x\n", rx_intr_4);
		hdmirx_wr_cor(RX_INTR4_PWD_IVCRX, rx_intr_4);
	}
	rx_intr_5 = hdmirx_rd_cor(RX_INTR5_PWD_IVCRX);
	if (rx_intr_5 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("14-%x\n", rx_intr_5);
		hdmirx_wr_cor(RX_INTR5_PWD_IVCRX, rx_intr_5);
	}
	rx_depack2_intr0 = hdmirx_rd_cor(RX_DEPACK2_INTR0_DP0B_IVCRX);
	if (rx_depack2_intr0 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("15-%x\n", rx_depack2_intr0);
		hdmirx_wr_cor(RX_DEPACK2_INTR0_DP0B_IVCRX, rx_depack2_intr0);
	}
	rx_depack2_intr1 = hdmirx_rd_cor(RX_DEPACK2_INTR1_DP0B_IVCRX);
	if (rx_depack2_intr1 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("16-%x\n", rx_depack2_intr1);
		hdmirx_wr_cor(RX_DEPACK2_INTR1_DP0B_IVCRX, rx_depack2_intr1);
	}
	rx_depack2_intr2 = hdmirx_rd_cor(RX_DEPACK2_INTR2_DP0B_IVCRX);
	if (rx_depack2_intr2 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("17-%x\n", rx_depack2_intr2);
		hdmirx_wr_cor(RX_DEPACK2_INTR2_DP0B_IVCRX, rx_depack2_intr2);
	}
	//hdcp1x
	rx_hdcp1x_intr0 = hdmirx_rd_cor(RX_HDCP1X_INTR0_HDCP1X_IVCRX);
	if (rx_hdcp1x_intr0 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("18-%x\n", rx_hdcp1x_intr0);
		hdmirx_wr_cor(RX_HDCP1X_INTR0_HDCP1X_IVCRX, rx_hdcp1x_intr0);
	}
	//hdcp2x
	rx_hdcp2x_intr0 = hdmirx_rd_cor(CP2PAX_INTR0_HDCP2X_IVCRX);
	if (rx_hdcp2x_intr0 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("19-%x\n", rx_hdcp2x_intr0);
		hdmirx_wr_cor(CP2PAX_INTR0_HDCP2X_IVCRX, rx_hdcp2x_intr0);
	}
	rx_hdcp2x_intr1 = hdmirx_rd_cor(CP2PAX_INTR1_HDCP2X_IVCRX);
	if (rx_hdcp2x_intr1 != 0) {
		if (log_level & DBG1_LOG)
			rx_pr("20-%x\n", rx_hdcp2x_intr1);
		hdmirx_wr_cor(CP2PAX_INTR1_HDCP2X_IVCRX, rx_hdcp2x_intr1);
	}
	if (rx_hdcp1x_intr0) {
		if (log_level & IRQ_LOG)
			rx_pr("irq_hdcp1-%x\n", rx_hdcp1x_intr0);
		if (rx_get_bits(rx_hdcp1x_intr0, _BIT(0))) {
			if (rx.hdcp.hdcp_version != HDCP_VER_14)
				skip_frame(skip_frame_cnt);
			rx.hdcp.hdcp_version = HDCP_VER_14;
			rx.hdcp.hdcp_source = true;
			rx_pr("14\n");
		}
	}

	if (rx_hdcp2x_intr0) {
		if (log_level & IRQ_LOG)
			rx_pr("irq_hdcp2-%x\n", rx_hdcp2x_intr0);
		if (rx_get_bits(rx_hdcp2x_intr0, _BIT(0)))
			hdcp22_auth_sts = HDCP22_AUTH_STATE_SUCCESS;
		if (rx_get_bits(rx_hdcp2x_intr0, _BIT(1)))
			hdcp22_auth_sts = HDCP22_AUTH_STATE_FAILED;
	}

	if (rx_hdcp2x_intr1) {
		if (log_level & IRQ_LOG)
			rx_pr("irq1_hdcp2-%x\n", rx_hdcp2x_intr1);
		if (rx_get_bits(rx_hdcp2x_intr1, _BIT(2))) {
			if (rx.hdcp.hdcp_version != HDCP_VER_22)
				skip_frame(skip_frame_cnt);
			rx.hdcp.hdcp_version = HDCP_VER_22;
			rx.hdcp.hdcp_source = true;
			rx_pr("22\n");
		}
	}
	if (intr_2) {
		if (log_level & IRQ_LOG)
			rx_pr("irq2-%x\n", intr_2);
		if (rx_get_bits(intr_2, INTR2_BIT1_SPD))
			rx_spd_type = 1;
		if (rx_get_bits(intr_2, INTR2_BIT2_AUD))
			rx_vsif_type |= VSIF_TYPE_HDR10P;
		if (rx_get_bits(intr_2, INTR2_BIT4_UNREC))
			rx_vsif_type |= VSIF_TYPE_HDMI21;
	}
	if (intr_3) {
		if (log_level & IRQ_LOG)
			rx_pr("irq3-%x\n", intr_3);
		if (rx_get_bits(intr_3, INTR3_BIT34_HF_VSI))
			rx_vsif_type |= VSIF_TYPE_DV15;
		if (rx_get_bits(intr_3, INTR3_BIT2_VSI))
			rx_vsif_type |= VSIF_TYPE_HDMI14;
	}

	if (rx_depack2_intr0) {
		if (log_level & IRQ_LOG)
			rx_pr("dp2-irq0-%x\n", rx_depack2_intr0);
		if (rx_get_bits(rx_depack2_intr0, _BIT(2))) {
			/* parse hdr in EMP pkt */
			if (log_level & IRQ_LOG)
				rx_pr("HDR\n");
		}
		if (rx_get_bits(rx_depack2_intr0, _BIT(3))) {
			rx_emp_type |= EMP_TYPE_VTEM;
			if (log_level & IRQ_LOG)
				rx_pr("VTEM\n");
		}
	}
	if (rx_depack2_intr2) {
		if (log_level & IRQ_LOG)
			rx_pr("dp2-irq2-%x\n", rx_depack2_intr2);
		if (rx_get_bits(rx_depack2_intr2, _BIT(4)))
			rx_emp_type |= EMP_TYPE_VSIF;
		if (rx_get_bits(rx_depack2_intr2, _BIT(2)))
			rx_emp_type |= EMP_TYPE_HDR;
	}

	if (hdcp_2x_ecc_intr) {
		if (log_level & IRQ_LOG)
			rx_pr("ecc_err-%x\n", hdcp_2x_ecc_intr);
		if (hdcp22_auth_sts == HDCP22_AUTH_STATE_SUCCESS)  {
			if (hdcp22_reauth_enable) {
				rx_hdcp_22_sent_reauth();
				if (rx.state >= FSM_SIG_STABLE)
					rx.state = FSM_SIG_WAIT_STABLE;
			}
		}
	}
	//if (vsif_type)
		//rx_pkt_handler(PKT_BUFF_SET_VSI);

	//if (drm_handle_flag)
		//rx_pkt_handler(PKT_BUFF_SET_DRM);

	//if (emp_handle_flag)
		//rx_pkt_handler(PKT_BUFF_SET_EMP);

	//if (rx.irq_flag)
		//tasklet_schedule(&rx_tasklet);

	//if (irq_need_clr)
		//error = 1;
	return error;
}

irqreturn_t irq_handler(int irq, void *params)
{
	unsigned long hdmirx_top_intr_stat;
	unsigned long long cur_clks, irq_duration;
	static unsigned long long last_clks;
	static u32 irq_err_cnt;
	int error = 0;
	u8 tmp = 0;

	cur_clks = sched_clock();
	irq_duration = cur_clks - last_clks;
	last_clks = cur_clks;

	if (irq_duration <= TIME_1MS) {
		if (log_level & DBG1_LOG)
			rx_pr("!:%ld\n", irq_duration);
		irq_err_cnt++;
	} else {
		irq_err_cnt = 0;
	}
	if (irq_err_cnt >= irq_err_max) {
		rx_pr("DE ERR\n");
		hdmirx_top_irq_en(false);
		hdmirx_output_en(false);
		if (rx.state > FSM_WAIT_CLK_STABLE)
			rx.state = FSM_WAIT_CLK_STABLE;
		irq_err_cnt = 0;
	}

	if (params == 0) {
		rx_pr("%s: %s\n", __func__,
		      "RX IRQ invalid parameter");
		return IRQ_HANDLED;
	}

reisr:hdmirx_top_intr_stat = hdmirx_rd_top(TOP_INTR_STAT);
	hdmirx_wr_top(TOP_INTR_STAT_CLR, hdmirx_top_intr_stat);

	/* must clear ip interrupt quickly */
	if (rx.chip_id >= CHIP_ID_TL1 &&
	    rx.chip_id <= CHIP_ID_T5D) {
		tmp = hdmirx_top_intr_stat & 0x1;
	} else if (rx.chip_id >= CHIP_ID_T7) {
		if (check_cor_irq_via_vsync)
			tmp = check_cor_irq_via_vsync;
		else
			tmp = hdmirx_top_intr_stat & 0x2;
	} else {
		tmp = hdmirx_top_intr_stat & (~(1 << 30));
	}
	if (tmp) {
		if (rx.chip_id >= CHIP_ID_T7)
			error = hdmi_rx_ctrl_irq_handler_t7();
		else
			error = hdmi_rx_ctrl_irq_handler();
		if (error < 0) {
			if (error != -EPERM) {
				rx_pr("%s: RX IRQ handler %d\n",
				      __func__, error);
			}
		}
	}
	/* modify interrupt flow for isr loading */
	/* top interrupt handler */
	if (rx.chip_id >= CHIP_ID_TL1) {
		if (hdmirx_top_intr_stat & (1 << 29)) {
			if (video_mute_enabled()) {
				set_video_mute(true);
				rx.var.mute_cnt = 0;
				if (log_level & 0x100)
					rx_pr("vpp mute\n");
			}
			skip_frame(skip_frame_cnt);
			if (log_level & 0x100)
				rx_pr("[isr] sqofclk_fall\n");
		}
		if (hdmirx_top_intr_stat & (1 << 28)) {
			if (log_level & 0x100)
				rx_pr("[isr] sqofclk_rise\n");
		}
		if (hdmirx_top_intr_stat & (1 << 27)) {
			if (rx.var.de_stable)
				rx.var.de_cnt++;
			#ifdef VSIF_PKT_READ_FROM_PD_FIFO
			if (rx.chip_id < CHIP_ID_T7)
				rx_pkt_handler(PKT_BUFF_SET_FIFO);
			#endif
			if (rx.state == FSM_SIG_READY) {
				rx.vsync_cnt++;
				if (rx_vsif_type) {
					rx_pkt_handler(PKT_BUFF_SET_VSI);
					rx_vsif_type = 0;
				}
				if (rx_spd_type) {
					rx_pkt_handler(PKT_BUFF_SET_SPD);
					rx_spd_type = 0;
				}
				if (rx_emp_type & EMP_TYPE_VTEM) {
					rx.vrr_en = true;
					rx_emp_type &= (~EMP_TYPE_VTEM);
				} else {
					rx.vrr_en = false;
				}
				if (rx.chip_id >= CHIP_ID_T7)
					rx_check_ecc_error();
				rx_update_sig_info();
			}
			if (log_level & 0x400)
				rx_pr("[isr] DE rise.\n");
		}
		if (hdmirx_top_intr_stat & (1 << 26)) {
			rx_emp_lastpkt_done_irq();
			if (log_level & 0x400)
				rx_pr("[isr] last_emp_done\n");
		}
		if (hdmirx_top_intr_stat & (1 << 25)) {
			if (rx.state >= FSM_SIG_STABLE) {
				rx_emp_field_done_irq();
				rx_pkt_handler(PKT_BUFF_SET_EMP);
			}
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
	if (rx.chip_id >= CHIP_ID_T7) {
		if (hdmirx_top_intr_stat & (1 << 2))
			if (log_level & COR_LOG)
				rx_pr("[isr] phy dig\n");
		if (hdmirx_top_intr_stat & (1 << 1))
			if (log_level & COR_LOG)
				rx_pr("[isr] ctrl pwd\n");
		if (hdmirx_top_intr_stat & (1 << 0))
			if (log_level & COR_LOG)
				rx_pr("[isr] ctrl aon\n");
	}
	if (rx.chip_id < CHIP_ID_TL1) {
		if (error == 1)
			goto reisr;
	} else if (rx.chip_id >= CHIP_ID_T7) {
		//nothing
	} else {
		hdmirx_top_intr_stat = hdmirx_rd_top(TOP_INTR_STAT);
		hdmirx_top_intr_stat &= 0x1;
		if (hdmirx_top_intr_stat) {
			if (log_level & ERR_LOG)
				rx_pr("\n irq_miss");
			goto reisr;
		}
	}
	return IRQ_HANDLED;
}

static const u32 sr_tbl[][2] = {
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
	u8 i;
	bool ret = false;
	/* note: if arc is mismatch with LUT, then return 0 */
	u32 ret_sr = 0;

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
	{0,	3,	0,	2048,	2160,	HDMI_4096p50_256x135_Y420},
	{0,	3,	0,	960,	1080,	HDMI_1080p_420},
	/* interlace */
	{1,	0,	0,	720,	240,	HDMI_720x480i},
	{1,	0,	0,	1440,	240,	HDMI_480i60},
	/* {1, 0,	0,	720,	288,	HDMI_720x576i}, */
	{1,	0,	0,	1440,	288,	HDMI_576i50},
	{1,	0,	0,	1920,	540,	HDMI_1080i50},
	{1,	0,	2,	1920,	1103,	HDMI_1080i_ALTERNATIVE},
	{1,	0,	1,	1920,	2228,	HDMI_1080i_FRAMEPACKING},
	{0,	0,	0,	1440,	240,	HDMI_1440x240p60},
	{0,	0,	0,	2880,	240,	HDMI_2880x240p60},
	{0,	0,	0,	1440,	288,	HDMI_1440x288p50},
	{0,	0,	0,	2880,	288,	HDMI_2880x288p50},

	{0,	0,	0,	720,	480,	HDMI_480p60},
	{0,	0,	0,	1440,	480,	HDMI_1440x480p60},
	{0,	0,	1,	720,	1005,	HDMI_480p_FRAMEPACKING},

	{0,	0,	0,	720,	576,	HDMI_576p50},
	{0,	0,	0,	1440,	576,	HDMI_1440x576p50},
	{0,	0,	1,	720,	1201,	HDMI_576p_FRAMEPACKING},

	{0,	0,	0,	1280,	720,	HDMI_720p50},
	{0,	0,	1,	1280,	1470,	HDMI_720p_FRAMEPACKING},

	{0,	0,	0,	1920,	1080,	HDMI_1080p50},
	{0,	0,	2,	1920,	2160,	HDMI_1080p_ALTERNATIVE},
	{0,	0,	1,	1920,	2205,	HDMI_1080p_FRAMEPACKING},

	{1,	0,	0,	2880,	240,	HDMI_2880x480i60},
	{1,	0,	0,	2880,	288,	HDMI_2880x576i50},
	{0,	0,	0,	2880,	480,	HDMI_2880x480p60},
	{0,	0,	0,	2880,	576,	HDMI_2880x576p50},
	/* vesa format*/
	{0,	0,	0,	640,	480,	HDMI_640x480p60},
	{0,	0,	0,	720,	400,	HDMI_720_400},
	{0,	0,	0,	800,	600,	HDMI_800_600},
	{0,	0,	0,	1024,	768,	HDMI_1024_768},
	{0,	0,	0,	1280,	768,	HDMI_1280_768},
	{0,	0,	0,	1280,	800,	HDMI_1280_800},
	{0,	0,	0,	1280,	960,	HDMI_1280_960},
	{0,	0,	0,	1280,	1024,	HDMI_1280_1024},
	{0,	0,	0,	1360,	768,	HDMI_1360_768},
	{0,	0,	0,	1366,	768,	HDMI_1366_768},
	{0,	0,	0,	1440,	900,	HDMI_1440_900},
	{0,	0,	0,	1400,	1050,	HDMI_1400_1050},
	{0,	0,	0,	1600,	900,	HDMI_1600_900},
	{0,	0,	0,	1600,	1200,	HDMI_1600_1200},
	{0,	0,	0,	1680,	1050,	HDMI_1680_1050},
	{0,	0,	0,	1920,	1200,	HDMI_1920_1200},
	{0,	0,	0,	1152,	864,	HDMI_1152_864},
	{0,	0,	0,	3840,	600,	HDMI_3840_600},
	{0, 0,	0,	2688,	1520,	HDMI_2688_1520},
	/* 4k2k mode */
	{0,	0,	0,	3840,	2160,	HDMI_2160p24_16x9},
	{0,	0,	0,	1920,	2160,	HDMI_2160p25_16x9},
	{0,	0,	0,	4096,	2160,	HDMI_4096p24_256x135},
	{0,	0,	0,	2560,	1440,	HDMI_2560_1440},
	{0,	0,	1,	2560,	3488,	HDMI_2560_1440},
	{0,	0,	2,	2560,	2986,	HDMI_2560_1440},
	/* for AG-506 */
	{0,	0,	0,	720,	483,	HDMI_480p60},
	/* for NUC8BEK */
	{0,	0,	0,	1920,	2160,	HDMI_1920x2160p60_16x9},
	{0,	0,	0,	960,	540,	HDMI_960x540},
};

static bool fmt_vic_abnormal(void)
{
	/* if format is unknown or unsupported after
	 * timing match, but TX send normal VIC, then
	 * abnormal format is detected.
	 */
	if (rx.pre.sw_vic == HDMI_UNKNOWN ||
	    rx.pre.sw_vic == HDMI_UNSUPPORT) {
		if (log_level & VIDEO_LOG)
			rx_pr("unsupport fmt!\n");
		return true;
	} else if (rx.pre.sw_vic >= HDMI_VESA_OFFSET &&
		   rx.pre.sw_vic < HDMI_UNSUPPORT &&
		   rx.pre.repeat != 0) {
		/* no pixel repeatition for VESA mode */
		if (log_level & VIDEO_LOG)
			rx_pr("repeatition abnormal for vesa\n");
		return true;
	} else if ((rx.pre.sw_vic == HDMI_1080p_ALTERNATIVE) &&
			   (rx.pre.sw_dvi ||
			    rx.pre.colorspace == E_COLOR_YUV420 ||
			    rx.vs_info_details._3d_structure == 0)) {
		if (log_level & VIDEO_LOG)
			rx_pr("avi abnormal for 3dmode\n");
		return true;
	} else if ((rx.cur.hdcp22_state & 1) &&
			   rx.cur.hdcp14_state == 3) {
		if (log_level & VIDEO_LOG)
			rx_pr("hdcp sts err\n");
		return true;
	}
	return false;
}

enum fps_e get_fps_index(void)
{
	enum fps_e ret = E_60HZ;

	if ((abs(rx.pre.frame_rate - 2400)
		< diff_frame_th))
		ret = E_24HZ;
	else if ((abs(rx.pre.frame_rate - 2500)
		  < diff_frame_th))
		ret = E_25HZ;
	else if ((abs(rx.pre.frame_rate - 3000)
		  < diff_frame_th))
		ret = E_30HZ;
	else if ((abs(rx.pre.frame_rate - 5000)
		  < diff_frame_th))
		ret = E_50HZ;
	else if ((abs(rx.pre.frame_rate - 7200)
		  < diff_frame_th))
		ret = E_72HZ;
	else if ((abs(rx.pre.frame_rate - 7500)
		  < diff_frame_th))
		ret = E_75HZ;
	else if ((abs(rx.pre.frame_rate - 10000)
		  < diff_frame_th))
		ret = E_100HZ;
	else if ((abs(rx.pre.frame_rate - 12000)
		  < diff_frame_th))
		ret = E_120HZ;
	else
		ret = E_60HZ;

	return ret;
}

enum tvin_sig_fmt_e hdmirx_hw_get_fmt(void)
{
	enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
	enum hdmi_vic_e vic = HDMI_UNKNOWN;

	/*
	 * if (fmt_vic_abnormal())
	 *	vic = rx.pre.hw_vic;
	 * else
	 */
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
		if (get_fps_index() == E_24HZ)
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_24HZ;
		else if (get_fps_index() == E_25HZ)
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_25HZ;
		else if (get_fps_index() == E_30HZ)
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_30HZ;
		else if (get_fps_index() == E_50HZ)
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_50HZ;
		else
			fmt = TVIN_SIG_FMT_HDMI_1280X720P_60HZ;
		break;
	case HDMI_720p_FRAMEPACKING:
		fmt = TVIN_SIG_FMT_HDMI_1280X720P_60HZ_FRAME_PACKING;
		break;
	case HDMI_1080i50:	/* 20 */
	case HDMI_1080i100:	/* 40 */
	case HDMI_1080i60:	/* 5 */
	case HDMI_1080i120:	/* 46 */
		if (get_fps_index() == E_50HZ)
			fmt = TVIN_SIG_FMT_HDMI_1920X1080I_50HZ_A;
		else
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
	case HDMI_720x480i:
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
		if (get_fps_index() == E_24HZ)
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_24HZ;
		else if (get_fps_index() == E_25HZ)
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_25HZ;
		else if (get_fps_index() == E_30HZ)
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_30HZ;
		else if (get_fps_index() == E_50HZ)
			fmt = TVIN_SIG_FMT_HDMI_1920X1080P_50HZ;
		else
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
	case HDMI_720_350:
		fmt = TVIN_SIG_FMT_HDMI_720X350_00HZ;
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
	case HDMI_1152_864:
		fmt = TVIN_SIG_FMT_HDMI_1152X864_00HZ;
		break;
	case HDMI_3840_600:
		fmt = TVIN_SIG_FMT_HDMI_3840X600_00HZ;
		break;
	case HDMI_2688_1520:
		fmt = TVIN_SIG_FMT_HDMI_2688X1520_00HZ;
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
	case HDMI_1920x2160p60_16x9:
		fmt = TVIN_SIG_FMT_HDMI_1920X2160_60HZ;
		break;
	case HDMI_960x540:
		fmt = TVIN_SIG_FMT_HDMI_960X540_60HZ;
		break;
	default:
		break;
	}
	return fmt;
}

bool rx_is_sig_ready(void)
{
	if (rx.state == FSM_SIG_READY || force_vic)
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
	u32 ch0 = 0, ch1 = 0, ch2 = 0;

	if (stable_check_lvl & TMDS_VALID_EN) {
		if (!is_tmds_valid()) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("TMDS invalid\n");
		}
	}
	if (stable_check_lvl & HACTIVE_EN) {
		if (abs(rx.cur.hactive - rx.pre.hactive) > diff_pixel_th) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("hactive(%d=>%d),",
				      rx.pre.hactive,
				      rx.cur.hactive);
		}
	}
	if (stable_check_lvl & VACTIVE_EN) {
		if (abs(rx.cur.vactive - rx.pre.vactive) > diff_line_th) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("vactive(%d=>%d),",
				      rx.pre.vactive,
				      rx.cur.vactive);
		}
	}
	if (stable_check_lvl & HTOTAL_EN) {
		if (abs(rx.cur.htotal - rx.pre.htotal) > diff_pixel_th) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("htotal(%d=>%d),",
				      rx.pre.htotal,
				      rx.cur.htotal);
		}
	}
	if (stable_check_lvl & VTOTAL_EN) {
		if (abs(rx.cur.vtotal - rx.pre.vtotal) > diff_line_th) {
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
		if (abs(rx.pre.frame_rate - rx.cur.frame_rate)
			> diff_frame_th) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("frame_rate(%d=>%d),",
				      rx.pre.frame_rate,
				      rx.cur.frame_rate);
		}
	}
	if (stable_check_lvl & REPEAT_EN) {
		if (rx.pre.repeat != rx.cur.repeat &&
		    stable_check_lvl & REPEAT_EN) {
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
		if ((ch0 + ch1 + ch2) > err_cnt_sum_max) {
			if (sig_stable_err_cnt++ > sig_stable_err_max) {
				rx_pr("warning: more err counter\n");
				sig_stable_err_cnt = 0;
				/*phy setting is fail, need reset phy*/
				sig_unstable_cnt = sig_unstable_max;
				rx.clk.cable_clk = 0;
			}
			ret = false;
		}
	}
	if (stable_check_lvl & ECC_ERR_CNT_EN) {
		if (rx.cur.hdcp22_state == 3)
			rx.ecc_err = rx_get_ecc_err();

		if (rx.ecc_err) {
			ret = false;
			if (log_level & VIDEO_LOG)
				rx_pr("ecc err\n");
		}
	}
	if (!ret && log_level & VIDEO_LOG)
		rx_pr("\n");

	if (force_vic)
		ret = true;
	return ret;
}

static int get_timing_fmt(void)
{
	int i;
	int size = sizeof(freq_ref) / sizeof(struct freq_ref_s);

	rx.pre.sw_vic = HDMI_UNKNOWN;
	rx.pre.sw_dvi = 0;
	rx.pre.sw_fp = 0;
	rx.pre.sw_alternative = 0;

	for (i = 0; i < size; i++) {
		if (abs(rx.pre.hactive - freq_ref[i].hactive) > diff_pixel_th)
			continue;
		if ((abs(rx.pre.vactive - freq_ref[i].vactive)) > diff_line_th)
			continue;
		if (rx.pre.colorspace != freq_ref[i].cd420 &&
		    freq_ref[i].cd420 != 0)
			continue;
		if (freq_ref[i].interlace != rx.pre.interlaced)
			continue;
		if (freq_ref[i].type_3d != rx.vs_info_details._3d_structure)
			continue;
		break;
	}
	if (force_vic) {
		i = 0;
		rx.pre.sw_vic = force_vic;
		rx.pre.sw_dvi = 0;
		return i;
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
	clk_chg_cnt = 0;
	/*rx.wait_no_sig_cnt = 0;*/
	/* rx.no_signal = false; */
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
	rx_esm_reset(2);
	set_scdc_cfg(1, 0);
	rx.state = FSM_INIT;
	rx.fsm_ext_state = FSM_NULL;
	/*if (hdcp22_on)*/
		/*esm_set_stable(0);*/
	rx.hdcp.hdcp_version = HDCP_VER_NONE;
	rx.hdcp.hdcp_source = false;
	rx.skip = 0;
	rx.var.mute_cnt = 0;
	rx.var.de_cnt = 0;
	rx.var.de_stable = false;
}

bool edid_ver_chg(enum hdcp_version_e pre, enum hdcp_version_e cur)
{
	bool flag = false;

	if (pre == HDCP_VER_NONE || pre == HDCP_VER_14) {
		/* if detect hdcp22 auth, update to edid2.0 */
		if (cur == HDCP_VER_22 &&
		    rx.fs_mode.edid_ver[rx.port] != EDID_V20)
			flag = true;
	}
	/* if change from hdcp22 to hdcp none/14,
	 * need to update to edid1.4
	 * else {
	 * if (((cur == HDCP_VER_NONE) || (cur == HDCP_VER_14)) &&
	 * (rx.fs_mode.edid_ver[rx.port] != EDID_V14))
	 * flag = true;
	 * }
	 */
	return flag;
}

void fs_mode_init(void)
{
	unsigned char i = 0;

	for (i = 0; i < E_PORT_NUM; i++) {
		rx.fs_mode.hdcp_ver[i] = HDCP_VER_NONE;
		rx.fs_mode.edid_ver[i] = EDID_V14;
	}
}

void hdcp_sts_update(void)
{
	unsigned char edid_auto = (edid_select >> (rx.port * 4)) & 0x2;
	bool edid_chg = edid_ver_chg(rx.fs_mode.hdcp_ver[rx.port],
				     rx.hdcp.hdcp_version);

	if (rx.fs_mode.hdcp_ver[rx.port] != rx.hdcp.hdcp_version) {
		rx.fs_mode.hdcp_ver[rx.port] = rx.hdcp.hdcp_version;
		if (edid_auto && edid_chg) {
			hdmi_rx_top_edid_update();
			rx.state = FSM_HPD_LOW;
		}
	}
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
	if (hdcp22_capable_sts == HDCP22_AUTH_STATE_CAPABLE &&
	    (hdcp22_auth_sts == HDCP22_AUTH_STATE_LOST ||
	    hdcp22_auth_sts == HDCP22_AUTH_STATE_FAILED)) {
		hdmirx_hdcp22_reauth();
		/*rx_pr("\n auth lost force hpd rst\n");*/
	}
}

void rx_dwc_reset(void)
{
	u8 rst_lvl;

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
		rst_lvl = 2;
	else
		rst_lvl = 1;

	if (log_level & VIDEO_LOG)
		rx_pr("%s-%d.\n", __func__, rst_lvl);
	rx_sw_reset(rst_lvl);
	//rx_irq_en(true);
	/* for hdcp1.4 interact very early cases, don't do
	 * esm reset to avoid interaction be interferenced.
	 */
	rx_esm_reset(3);
}

bool rx_hpd_keep_low(void)
{
	bool ret = false;

	if (downstream_hpd_flag || edid_update_flag) {
		if (hpd_wait_cnt <= hpd_wait_max * 4)
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

bool is_unnormal_format(u8 wait_cnt)
{
	bool ret = false;

	if (rx.pre.sw_vic == HDMI_UNSUPPORT ||
	    rx.pre.sw_vic == HDMI_UNKNOWN) {
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
	if (rx.pre.hdcp14_state != 3 &&
	    rx.pre.hdcp14_state != 0 &&
	    rx.hdcp.hdcp_version == HDCP_VER_14) {
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
	/*
	 * 1.for dv_cts HDMI 40-47 switch input per 2s
	 * cannot keep waiting hdcp.
	 * It will cause test fail
	 * 2.fix appletv BSOD issue.
	 */
	if (rx.hdcp.hdcp_version == HDCP_VER_NONE) {
		ret = true;
		if (dev_is_appletv_v2) {
			if (wait_cnt == hdcp_none_wait_max * 30)
				ret = false;
		} else if (rx.hdcp.hdcp_pre_ver != HDCP_VER_14) {
			if (wait_cnt == hdcp_none_wait_max)
				ret = false;
		} else {
			if (wait_cnt == hdcp_none_wait_max * 10)
				ret = false;
		}
		if (log_level & VIDEO_LOG)
			rx_pr("hdcp none waiting\n");
	}
	if (!ret && wait_cnt != sig_stable_max)
		if (log_level & VIDEO_LOG)
			rx_pr("unnormal_format wait cnt = %d\n",
			      wait_cnt - sig_stable_max);
	return ret;
}

void fsm_restart(void)
{
	rx_esm_reset(2);
	hdmi_rx_top_edid_update();
	hdmirx_hw_config();
	set_scdc_cfg(1, 0);
	vic_check_en = false;
	dvi_check_en = true;
	rx.fsm_ext_state = FSM_INIT;
	rx.clk.cable_clk = 0;
	rx.phy.pll_rate = 0;
	rx.phy.phy_bw = 0;
	rx.phy.pll_bw = 0;
	rx.aml_phy.force_sqo = 0;
	i2c_err_cnt = 0;
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
	rx.fsm_ext_state = FSM_HPD_LOW;
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

void rx_get_global_variable(const char *buf)
{
	int i = 1;

	rx_pr("index %-30s   value\n", "varaible");
	pr_var(dwc_rst_wait_cnt_max, i++);
	pr_var(sig_stable_max, i++);
	pr_var(hpd_wait_max, i++);
	pr_var(sig_unstable_max, i++);
	pr_var(sig_unready_max, i++);
	pr_var(pow5v_max_cnt, i++);
	pr_var(diff_pixel_th, i++);
	pr_var(diff_line_th, i++);
	pr_var(diff_frame_th, i++);
	pr_var(force_vic, i++);
	pr_var(aud_sr_stb_max, i++);
	pr_var(pwr_sts_to_esm, i++);
	pr_var(log_level, i++);
	pr_var(rx5v_debug_en, i++);
	pr_var(irq_err_max, i++);
	pr_var(clk_unstable_max, i++);
	pr_var(clk_stable_cnt, i++);
	pr_var(clk_stable_max, i++);
	pr_var(wait_no_sig_max, i++);
	pr_var(vrr_func_en, i++);
	pr_var(receive_edid_len, i++);
	pr_var(hdcp_array_len, i++);
	pr_var(hdcp_len, i++);
	pr_var(hdcp_repeat_depth, i++);
	pr_var(up_phy_addr, i++);
	pr_var(stable_check_lvl, i++);
	pr_var(hdcp22_reauth_enable, i++);
	pr_var(esm_recovery_mode, i++);
	pr_var(unnormal_wait_max, i++);
	pr_var(hdmi_yuv444_enable, i++);
	pr_var(pc_mode_en, i++);
	pr_var(en_4k_2_2k, i++);
	pr_var(en_4096_2_3840, i++);
	pr_var(en_4k_timing, i++);
	pr_var(acr_mode, i++);
	pr_var(force_clk_rate, i++);
	pr_var(rx_afifo_dbg_en, i++);
	pr_var(auto_aclk_mute, i++);
	pr_var(aud_avmute_en, i++);
	pr_var(rx_ecc_err_thres, i++);
	pr_var(rx_ecc_err_frames, i++);
	pr_var(aud_mute_sel, i++);
	pr_var(md_ists_en, i++);
	pr_var(pdec_ists_en, i++);
	pr_var(packet_fifo_cfg, i++);
	pr_var(pd_fifo_start_cnt, i++);
	pr_var(hdcp22_on, i++);
	pr_var(delay_ms_cnt, i++);
	pr_var(eq_max_setting, i++);
	pr_var(eq_dbg_ch0, i++);
	pr_var(eq_dbg_ch1, i++);
	pr_var(eq_dbg_ch2, i++);
	pr_var(edid_mode, i++);
	pr_var(phy_pddq_en, i++);
	pr_var(long_cable_best_setting, i++);
	pr_var(port_map, i++);
	pr_var(skip_frame_cnt, i++);
	pr_var(vdin_drop_frame_cnt, i++);
	pr_var(atmos_edid_update_hpd_en, i++);
	pr_var(suspend_pddq_sel, i++);
	pr_var(aud_ch_map, i++);
	pr_var(hdcp_none_wait_max, i++);
	pr_var(pll_unlock_max, i++);
	pr_var(esd_phy_rst_max, i++);
	pr_var(vsync_err_cnt_max, i++);
	pr_var(ignore_sscp_charerr, i++);
	pr_var(ignore_sscp_tmds, i++);
	pr_var(err_chk_en, i++);
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
	pr_var(ddc_dbg_en, i++);
	pr_var(hdcp_hpd_ctrl_en, i++);
	pr_var(eq_dbg_lvl, i++);
	pr_var(edid_select, i++);
	pr_var(vpp_mute_enable, i++);
	pr_var(dbg_cs, i++);
	pr_var(dbg_pkt, i++);
	pr_var(aud_compose_type, i++);
	pr_var(vrr_range_dynamic_update_en, i++);
	pr_var(rx.var.force_pattern, i++);
	/* phy var definitioin */
	pr_var(rx.aml_phy.sqrst_en, i++);
	pr_var(rx.aml_phy.vga_dbg, i++);
	pr_var(rx.aml_phy.dfe_en, i++);
	pr_var(rx.aml_phy.ofst_en, i++);
	pr_var(rx.aml_phy.cdr_mode, i++);
	pr_var(rx.aml_phy.pre_int_en, i++);
	pr_var(rx.aml_phy.pre_int, i++);
	pr_var(rx.aml_phy.phy_bwth, i++);
	pr_var(rx.aml_phy.vga_dbg_delay, i++);
	pr_var(rx.aml_phy.alirst_en, i++);
	pr_var(rx.aml_phy.tap1_byp, i++);
	pr_var(rx.aml_phy.eq_byp, i++);
	pr_var(rx.aml_phy.long_cable, i++);
	pr_var(vsvdb_update_hpd_en, i++);
	pr_var(rx.aml_phy.osc_mode, i++);
	pr_var(rx.aml_phy.pll_div, i++);
	pr_var(clk_chg_max, i++);
	pr_var(rx.aml_phy.eq_fix_val, i++);
	pr_var(rx.var.dbg_ve, i++);
	pr_var(rx.var.avi_chk_frames, i++);
	pr_var(rx.aml_phy.cdr_fr_en, i++);
	pr_var(rx.aml_phy.force_sqo, i++);
	pr_var(rx.aml_phy.os_rate, i++);
	pr_var(rx.aml_phy.vga_gain, i++);
	pr_var(rx.aml_phy.eq_stg1, i++);
	pr_var(rx.aml_phy.eq_stg2, i++);
	pr_var(rx.aml_phy.dfe_hold, i++);
	pr_var(rx.aml_phy.eq_hold, i++);
	pr_var(rx.aml_phy.eye_delay, i++);
	pr_var(rx.aml_phy.eq_retry, i++);
	pr_var(rx.aml_phy.tap2_byp, i++);
	pr_var(rx.aml_phy.long_bist_en, i++);
}

bool str_cmp(unsigned char *buff, unsigned char *str)
{
	if (strlen(str) == strlen(buff) &&
	    strncmp(buff, str, strlen(str)) == 0)
		return true;
	else
		return false;
}

bool comp_set_pr_var(u8 *buff, u8 *var_str, void *var, u32 val)
{
	char index_c[5] = {'\0'};

	if (str_cmp(buff, var_str) || str_cmp((buff), (index_c))) {
		memcpy(var, &val, sizeof(val));
		return true;
	}
	return false;
}

bool set_pr_var(unsigned char *buff, unsigned char *str, void *var, u32 value)
{
	return comp_set_pr_var(buff, str, var, value);
}

int rx_set_global_variable(const char *buf, int size)
{
	char tmpbuf[60];
	int i = 0;
	u32 value = 0;
	int ret = 0;
	int index = 1;

	if (buf == 0 || size == 0 || size > 60)
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
	if (ret != 0) {
		rx_pr("No value set:%d\n", ret);
		return -2;
	}

	if (set_pr_var(tmpbuf, var_to_str(dwc_rst_wait_cnt_max), &dwc_rst_wait_cnt_max, value))
		return pr_var(dwc_rst_wait_cnt_max, index);
	if (set_pr_var(tmpbuf, var_to_str(sig_stable_max), &sig_stable_max, value))
		return pr_var(sig_stable_max, index);
	if (set_pr_var(tmpbuf, var_to_str(hpd_wait_max), &hpd_wait_max, value))
		return pr_var(hpd_wait_max, index);
	if (set_pr_var(tmpbuf, var_to_str(log_level), &log_level, value))
		return pr_var(log_level, index);
	if (set_pr_var(tmpbuf, var_to_str(sig_unready_max), &sig_unready_max, value))
		return pr_var(sig_unready_max, index);
	if (set_pr_var(tmpbuf, var_to_str(pow5v_max_cnt), &pow5v_max_cnt, value))
		return pr_var(pow5v_max_cnt, index);
	if (set_pr_var(tmpbuf, var_to_str(diff_pixel_th), &diff_pixel_th, value))
		return pr_var(diff_pixel_th, index);
	if (set_pr_var(tmpbuf, var_to_str(diff_line_th), &diff_line_th, value))
		return pr_var(diff_line_th, index);
	if (set_pr_var(tmpbuf, var_to_str(diff_frame_th), &diff_frame_th, value))
		return pr_var(diff_frame_th, index);
	if (set_pr_var(tmpbuf, var_to_str(force_vic), &force_vic, value))
		return pr_var(force_vic, index);
	if (set_pr_var(tmpbuf, var_to_str(aud_sr_stb_max), &aud_sr_stb_max, value))
		return pr_var(aud_sr_stb_max, index);
	if (set_pr_var(tmpbuf, var_to_str(pwr_sts_to_esm), &pwr_sts_to_esm, value))
		return pr_var(pwr_sts_to_esm, index);
	if (set_pr_var(tmpbuf, var_to_str(rx5v_debug_en), &rx5v_debug_en, value))
		return pr_var(rx5v_debug_en, index);
	if (set_pr_var(tmpbuf, var_to_str(irq_err_max), &irq_err_max, value))
		return pr_var(irq_err_max, index);
	if (set_pr_var(tmpbuf, var_to_str(clk_unstable_max), &clk_unstable_max, value))
		return pr_var(clk_unstable_max, index);
	if (set_pr_var(tmpbuf, var_to_str(clk_stable_cnt), &clk_stable_cnt, value))
		return pr_var(clk_stable_cnt, index);
	if (set_pr_var(tmpbuf, var_to_str(clk_stable_max), &clk_stable_max, value))
		return pr_var(clk_stable_max, index);
	if (set_pr_var(tmpbuf, var_to_str(wait_no_sig_max), &wait_no_sig_max, value))
		return pr_var(wait_no_sig_max, index);
	if (set_pr_var(tmpbuf, var_to_str(vrr_func_en), &vrr_func_en, value))
		return pr_var(vrr_func_en, index);
	if (set_pr_var(tmpbuf, var_to_str(receive_edid_len), &receive_edid_len, value))
		return pr_var(receive_edid_len, index);
	if (set_pr_var(tmpbuf, var_to_str(hdcp_len), &hdcp_len, value))
		return pr_var(hdcp_len, index);
	if (set_pr_var(tmpbuf, var_to_str(hdcp_repeat_depth), &hdcp_repeat_depth, value))
		return pr_var(hdcp_repeat_depth, index);
	if (set_pr_var(tmpbuf, var_to_str(up_phy_addr), &up_phy_addr, value))
		return pr_var(up_phy_addr, index);
	if (set_pr_var(tmpbuf, var_to_str(stable_check_lvl), &stable_check_lvl, value))
		return pr_var(stable_check_lvl, index);
	if (set_pr_var(tmpbuf, var_to_str(hdcp22_reauth_enable), &hdcp22_reauth_enable, value))
		return pr_var(hdcp22_reauth_enable, index);
	if (set_pr_var(tmpbuf, var_to_str(esm_recovery_mode), &esm_recovery_mode, value))
		return pr_var(esm_recovery_mode, index);
	if (set_pr_var(tmpbuf, var_to_str(unnormal_wait_max), &unnormal_wait_max, value))
		return pr_var(unnormal_wait_max, index);
	if (set_pr_var(tmpbuf, var_to_str(edid_update_delay), &edid_update_delay, value))
		return pr_var(edid_update_delay, index);
	if (set_pr_var(tmpbuf, var_to_str(hdmi_yuv444_enable), &hdmi_yuv444_enable, value))
		return pr_var(hdmi_yuv444_enable, index);
	if (set_pr_var(tmpbuf, var_to_str(pc_mode_en), &pc_mode_en, value))
		return pr_var(pc_mode_en, index);
	if (set_pr_var(tmpbuf, var_to_str(en_4k_2_2k), &en_4k_2_2k, value))
		return pr_var(en_4k_2_2k, index);
	if (set_pr_var(tmpbuf, var_to_str(en_4096_2_3840), &en_4096_2_3840, value))
		return pr_var(en_4096_2_3840, index);
	if (set_pr_var(tmpbuf, var_to_str(en_4k_timing), &en_4k_timing, value))
		return pr_var(en_4k_timing, index);
	if (set_pr_var(tmpbuf, var_to_str(acr_mode), &acr_mode, value))
		return pr_var(acr_mode, index);
	if (set_pr_var(tmpbuf, var_to_str(force_clk_rate), &force_clk_rate, value))
		return pr_var(force_clk_rate, index);
	if (set_pr_var(tmpbuf, var_to_str(rx_afifo_dbg_en),
					&rx_afifo_dbg_en, value))
		return pr_var(rx_afifo_dbg_en, index);
	if (set_pr_var(tmpbuf, var_to_str(auto_aclk_mute), &auto_aclk_mute, value))
		return pr_var(auto_aclk_mute, index);
	if (set_pr_var(tmpbuf, var_to_str(aud_avmute_en), &aud_avmute_en, value))
		return pr_var(aud_avmute_en, index);
	if (set_pr_var(tmpbuf, var_to_str(rx_ecc_err_thres),
		&rx_ecc_err_thres, value))
		return pr_var(rx_ecc_err_thres, index);
	if (set_pr_var(tmpbuf, var_to_str(rx_ecc_err_frames),
		&rx_ecc_err_frames, value))
		return pr_var(rx_ecc_err_frames, index);
	if (set_pr_var(tmpbuf, var_to_str(aud_mute_sel), &aud_mute_sel, value))
		return pr_var(aud_mute_sel, index);
	if (set_pr_var(tmpbuf, var_to_str(md_ists_en), &md_ists_en, value))
		return pr_var(md_ists_en, index);
	if (set_pr_var(tmpbuf, var_to_str(pdec_ists_en), &pdec_ists_en, value))
		return pr_var(pdec_ists_en, index);
	if (set_pr_var(tmpbuf, var_to_str(packet_fifo_cfg), &packet_fifo_cfg, value))
		return pr_var(packet_fifo_cfg, index);
	if (set_pr_var(tmpbuf, var_to_str(pd_fifo_start_cnt), &pd_fifo_start_cnt, value))
		return pr_var(pd_fifo_start_cnt, index);
	if (set_pr_var(tmpbuf, var_to_str(hdcp22_on), &hdcp22_on, value))
		return pr_var(hdcp22_on, index);
	if (set_pr_var(tmpbuf, var_to_str(delay_ms_cnt), &delay_ms_cnt, value))
		return pr_var(delay_ms_cnt, index);
	if (set_pr_var(tmpbuf, var_to_str(eq_max_setting), &eq_max_setting, value))
		return pr_var(eq_max_setting, index);
	if (set_pr_var(tmpbuf, var_to_str(eq_dbg_ch0), &eq_dbg_ch0, value))
		return pr_var(eq_dbg_ch0, index);
	if (set_pr_var(tmpbuf, var_to_str(eq_dbg_ch1), &eq_dbg_ch1, value))
		return pr_var(eq_dbg_ch1, index);
	if (set_pr_var(tmpbuf, var_to_str(eq_dbg_ch2), &eq_dbg_ch2, value))
		return pr_var(eq_dbg_ch2, index);
	if (set_pr_var(tmpbuf, var_to_str(edid_mode), &edid_mode, value))
		return pr_var(edid_mode, index);
	if (set_pr_var(tmpbuf, var_to_str(phy_pddq_en), &phy_pddq_en, value))
		return pr_var(phy_pddq_en, index);
	if (set_pr_var(tmpbuf, var_to_str(long_cable_best_setting),
		&long_cable_best_setting, value))
		return pr_var(long_cable_best_setting, index);
	if (set_pr_var(tmpbuf, var_to_str(port_map), &port_map, value))
		return pr_var(port_map, index);
	if (set_pr_var(tmpbuf, var_to_str(skip_frame_cnt), &skip_frame_cnt, value))
		return pr_var(skip_frame_cnt, index);
	if (set_pr_var(tmpbuf, var_to_str(vdin_drop_frame_cnt), &vdin_drop_frame_cnt, value))
		return pr_var(vdin_drop_frame_cnt, index);
	if (set_pr_var(tmpbuf, var_to_str(atmos_edid_update_hpd_en),
		&atmos_edid_update_hpd_en, value))
		return pr_var(atmos_edid_update_hpd_en, index);
	if (set_pr_var(tmpbuf, var_to_str(suspend_pddq_sel), &suspend_pddq_sel, value))
		return pr_var(suspend_pddq_sel, index);
	if (set_pr_var(tmpbuf, var_to_str(aud_ch_map), &aud_ch_map, value))
		return pr_var(aud_ch_map, index);
	if (set_pr_var(tmpbuf, var_to_str(hdcp_none_wait_max), &hdcp_none_wait_max, value))
		return pr_var(hdcp_none_wait_max, index);
	if (set_pr_var(tmpbuf, var_to_str(pll_unlock_max), &pll_unlock_max, value))
		return pr_var(pll_unlock_max, index);
	if (set_pr_var(tmpbuf, var_to_str(esd_phy_rst_max), &esd_phy_rst_max, value))
		return pr_var(esd_phy_rst_max, index);
	if (set_pr_var(tmpbuf, var_to_str(vsync_err_cnt_max), &vsync_err_cnt_max, value))
		return pr_var(vsync_err_cnt_max, index);
	if (set_pr_var(tmpbuf, var_to_str(ignore_sscp_charerr), &ignore_sscp_charerr, value))
		return pr_var(ignore_sscp_charerr, index);
	if (set_pr_var(tmpbuf, var_to_str(ignore_sscp_tmds), &ignore_sscp_tmds, value))
		return pr_var(ignore_sscp_tmds, index);
	if (set_pr_var(tmpbuf, var_to_str(err_chk_en), &err_chk_en, value))
		return pr_var(err_chk_en, index);
	if (set_pr_var(tmpbuf, var_to_str(find_best_eq), &find_best_eq, value))
		return pr_var(find_best_eq, index);
	if (set_pr_var(tmpbuf, var_to_str(eq_try_cnt), &eq_try_cnt, value))
		return pr_var(eq_try_cnt, index);
	if (set_pr_var(tmpbuf, var_to_str(pll_rst_max), &pll_rst_max, value))
		return pr_var(pll_rst_max, index);
	if (set_pr_var(tmpbuf, var_to_str(hdcp_enc_mode), &hdcp_enc_mode, value))
		return pr_var(hdcp_enc_mode, index);
	if (set_pr_var(tmpbuf, var_to_str(hbr_force_8ch), &hbr_force_8ch, value))
		return pr_var(hbr_force_8ch, index);
	if (set_pr_var(tmpbuf, var_to_str(cdr_lock_level), &cdr_lock_level, value))
		return pr_var(cdr_lock_level, index);
	if (set_pr_var(tmpbuf, var_to_str(top_intr_maskn_value), &top_intr_maskn_value, value))
		return pr_var(top_intr_maskn_value, index);
	if (set_pr_var(tmpbuf, var_to_str(pll_lock_max), &pll_lock_max, value))
		return pr_var(pll_lock_max, index);
	if (set_pr_var(tmpbuf, var_to_str(clock_lock_th), &clock_lock_th, value))
		return pr_var(clock_lock_th, index);
	if (set_pr_var(tmpbuf, var_to_str(en_take_dtd_space), &en_take_dtd_space, value))
		return pr_var(en_take_dtd_space, index);
	if (set_pr_var(tmpbuf, var_to_str(earc_cap_ds_update_hpd_en),
		&earc_cap_ds_update_hpd_en, value))
		return pr_var(earc_cap_ds_update_hpd_en, index);
	if (set_pr_var(tmpbuf, var_to_str(scdc_force_en), &scdc_force_en, value))
		return pr_var(scdc_force_en, index);
	if (set_pr_var(tmpbuf, var_to_str(ddc_dbg_en), &ddc_dbg_en, value))
		return pr_var(ddc_dbg_en, index);
	if (set_pr_var(tmpbuf, var_to_str(hdcp_hpd_ctrl_en), &hdcp_hpd_ctrl_en, value))
		return pr_var(hdcp_hpd_ctrl_en, index);
	if (set_pr_var(tmpbuf, var_to_str(eq_dbg_lvl), &eq_dbg_lvl, value))
		return pr_var(eq_dbg_lvl, index);
	if (set_pr_var(tmpbuf, var_to_str(edid_select), &edid_select, value))
		return pr_var(edid_select, index);
	if (set_pr_var(tmpbuf, var_to_str(vpp_mute_enable), &vpp_mute_enable, value))
		return pr_var(vpp_mute_enable, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.var.dbg_ve), &rx.var.dbg_ve, value))
		return pr_var(rx.var.dbg_ve, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.var.avi_chk_frames), &rx.var.avi_chk_frames, value))
		return pr_var(rx.var.avi_chk_frames, index);
	if (set_pr_var(tmpbuf, var_to_str(vsvdb_update_hpd_en), &vsvdb_update_hpd_en, value))
		return pr_var(vsvdb_update_hpd_en, index);
	if (set_pr_var(tmpbuf, var_to_str(clk_chg_max), &clk_chg_max, value))
		return pr_var(clk_chg_max, index);
	if (set_pr_var(tmpbuf, var_to_str(dbg_cs), &dbg_cs, value))
		return pr_var(dbg_cs, index);
	if (set_pr_var(tmpbuf, var_to_str(dbg_pkt), &dbg_pkt, value))
		return pr_var(dbg_pkt, index);
	if (set_pr_var(tmpbuf, var_to_str(aud_compose_type),
	    &aud_compose_type, value))
		return pr_var(aud_compose_type, index);
	if (set_pr_var(tmpbuf, var_to_str(vrr_range_dynamic_update_en),
	    &vrr_range_dynamic_update_en, value))
		return pr_var(vrr_range_dynamic_update_en, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.var.force_pattern), &rx.var.force_pattern, value))
		return pr_var(rx.var.force_pattern, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.sqrst_en), &rx.aml_phy.sqrst_en, value))
		return pr_var(rx.aml_phy.sqrst_en, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.vga_dbg), &rx.aml_phy.vga_dbg, value))
		return pr_var(rx.aml_phy.vga_dbg, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.dfe_en), &rx.aml_phy.dfe_en, value))
		return pr_var(rx.aml_phy.dfe_en, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.ofst_en), &rx.aml_phy.ofst_en, value))
		return pr_var(rx.aml_phy.ofst_en, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.cdr_mode), &rx.aml_phy.cdr_mode, value))
		return pr_var(rx.aml_phy.cdr_mode, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.pre_int_en), &rx.aml_phy.pre_int_en, value))
		return pr_var(rx.aml_phy.pre_int_en, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.pre_int), &rx.aml_phy.pre_int, value))
		return pr_var(rx.aml_phy.pre_int, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.phy_bwth), &rx.aml_phy.phy_bwth, value))
		return pr_var(rx.aml_phy.phy_bwth, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.vga_dbg_delay),
		&rx.aml_phy.vga_dbg_delay, value))
		return pr_var(rx.aml_phy.vga_dbg_delay, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.alirst_en), &rx.aml_phy.alirst_en, value))
		return pr_var(rx.aml_phy.alirst_en, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.tap1_byp), &rx.aml_phy.tap1_byp, value))
		return pr_var(rx.aml_phy.tap1_byp, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.eq_byp), &rx.aml_phy.eq_byp, value))
		return pr_var(rx.aml_phy.eq_byp, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.long_cable), &rx.aml_phy.long_cable, value))
		return pr_var(rx.aml_phy.long_cable, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.osc_mode), &rx.aml_phy.osc_mode, value))
		return pr_var(rx.aml_phy.osc_mode, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.pll_div), &rx.aml_phy.pll_div, value))
		return pr_var(rx.aml_phy.pll_div, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.eq_fix_val), &rx.aml_phy.eq_fix_val, value))
		return pr_var(rx.aml_phy.eq_fix_val, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.cdr_fr_en), &rx.aml_phy.cdr_fr_en, value))
		return pr_var(rx.aml_phy.cdr_fr_en, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.force_sqo), &rx.aml_phy.force_sqo, value))
		return pr_var(rx.aml_phy.force_sqo, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.os_rate), &rx.aml_phy.os_rate, value))
		return pr_var(rx.aml_phy.os_rate, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.vga_gain), &rx.aml_phy.vga_gain, value))
		return pr_var(rx.aml_phy.vga_gain, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.eq_stg1), &rx.aml_phy.eq_stg1, value))
		return pr_var(rx.aml_phy.eq_stg1, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.eq_stg2), &rx.aml_phy.eq_stg2, value))
		return pr_var(rx.aml_phy.eq_stg2, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.dfe_hold), &rx.aml_phy.dfe_hold, value))
		return pr_var(rx.aml_phy.dfe_hold, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.eq_hold), &rx.aml_phy.eq_hold, value))
		return pr_var(rx.aml_phy.eq_hold, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.eye_delay), &rx.aml_phy.eye_delay, value))
		return pr_var(rx.aml_phy.eye_delay, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.eq_retry), &rx.aml_phy.eq_retry, value))
		return pr_var(rx.aml_phy.eq_retry, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.tap2_byp), &rx.aml_phy.tap2_byp, value))
		return pr_var(rx.aml_phy.tap2_byp, index);
	if (set_pr_var(tmpbuf, var_to_str(rx.aml_phy.long_bist_en),
		&rx.aml_phy.long_bist_en, value))
		return pr_var(rx.aml_phy.long_bist_en, index);
	return 0;
}

void skip_frame(unsigned int cnt)
{
	if (rx.state == FSM_SIG_READY) {
		rx.skip = (1000 * 100 / rx.pre.frame_rate / 12) + 1;
		rx.skip = cnt * rx.skip;
		rx_pr("rx.skip = %d\n", rx.skip);
	}
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
	u32 fsmst = sm_pause;

	/* stop fsm when swich port */
	sm_pause = 1;
	rx.port = (port - TVIN_PORT_HDMI0) & 0xf;
	rx.no_signal = false;
	rx.wait_no_sig_cnt = 0;
	vic_check_en = false;
	i2c_err_cnt = 0;
	dvi_check_en = true;
	rx.ddc_filter_en = false;
	if (hdmirx_repeat_support())
		rx.hdcp.repeat = repeat_plug;
	else
		rx.hdcp.repeat = 0;
	if (is_special_func_en()) {
		rx.state = FSM_HPD_HIGH;
	} else if (!rx_need_support_fastswitch() &&
		(pre_port != rx.port ||
	    (rx_get_cur_hpd_sts() == 0) ||
	    /* when open specific port, force to enable it */
	    (disable_port_en && rx.port == disable_port_num))) {
		rx_esm_reset(1);
		if (rx.state > FSM_HPD_LOW)
			rx.state = FSM_HPD_LOW;
		wait_ddc_idle();
		rx_i2c_init();
		rx_set_cur_hpd(0, 0);
		/* need reset the whole module when switch port */
		if (need_update_edid())
			hdmi_rx_top_edid_update();
		//hdmirx_hw_config();
	} else {
		if (rx.state >= FSM_SIG_STABLE)
			rx.state = FSM_SIG_STABLE;
		else
			rx.state = FSM_HPD_HIGH;
	}
	edid_update_flag = 0;
	rx_pkt_initial();
	rx.fsm_ext_state = FSM_NULL;
	sm_pause = fsmst;
	rx.pre_state = rx.state;
	if (rx.phy_ver >= PHY_VER_TM2)
		rx.aml_phy.pre_int = 1;
	/* extcon_set_state_sync(rx.rx_excton_open, EXTCON_DISP_HDMI, 1); */
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
	hdmirx_top_irq_en(false);
	/* extcon_set_state_sync(rx.rx_excton_open, EXTCON_DISP_HDMI, 0); */
	/* after port close, stop count DE/AVI infoframe */
	rx.var.de_stable = false;
	rx.var.de_cnt = 0;
	rx.var.avi_rcv_cnt = 0;
}

void rx_nosig_monitor(void)
{
	if (rx.cur_5v_sts == 0) {
		rx.no_signal = true;
	} else if (rx.state != FSM_SIG_READY) {
		if (rx.wait_no_sig_cnt >= wait_no_sig_max) {
			rx.no_signal = true;
		} else {
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

	if (rx.cur_5v_sts == 0)
		rx.cableclk_stb_flg = false;
	else
		rx.cableclk_stb_flg = is_clk_stable();

	if (pre_sts != rx.cableclk_stb_flg) {
		pre_sts = rx.cableclk_stb_flg;
		rx_pr("\nclk_stb_changed to = %d\n", pre_sts);
	}
}

void rx_clr_fs_sts(unsigned char port)
{
	if (port >= E_PORT_NUM)
		return;

	rx.fs_mode.hdcp_ver[port] = HDCP_VER_NONE;
	rx.fs_mode.edid_ver[port] = EDID_V14;
	/* no need */
	rx.fs_mode.hdmi5v_sts[port] = 0;
}

u8 rx_update_fastswitch_sts(u8 sts)
{
	u8 i = 0;

	if (rx.chip_id < CHIP_ID_TL1)
		return 0;

	for (i = 0; i < E_PORT_NUM; i++) {
		if ((sts & (1 << i)) == 0)
			rx_clr_fs_sts(i);
		else
			rx.fs_mode.hdmi5v_sts[i] = 1;
	}
	return 1;
}

/* inform hdcp_rx22 the 5v sts of rx to control delay time */
void rx_5v_sts_to_esm(unsigned int pwr)
{
	if (rx.chip_id >= CHIP_ID_T7)
		return;
	if (hdcp22_on) {
		if (!pwr)
			pwr_sts_to_esm = true;
		else
			pwr_sts_to_esm = false;
	}
}

/* ---------------------------------------------------------- */
/* func:         port A,B,C,D  hdmitx-5v monitor & HPD control */
/* note:         G9TV portD no used */
/* ---------------------------------------------------------- */
void rx_5v_monitor(void)
{
	static u8 check_cnt;
	u8 tmp_5v;
	bool tmp_arc_5v;

	if (rx5v_debug_en)
		tmp_5v = 0x0f;
	else
		tmp_5v = rx_get_hdmi5v_sts();

	if (tmp_5v != pwr_sts)
		check_cnt++;

	if (check_cnt >= pow5v_max_cnt) {
		check_cnt = 0;
		pwr_sts = tmp_5v;
		rx_update_fastswitch_sts(pwr_sts);
		rx.cur_5v_sts = (pwr_sts >> rx.port) & 1;
		if (cec_hdmirx5v_update)
			cec_hdmirx5v_update(pwr_sts);
		hotplug_wait_query();
		rx_pr("hotplug-0x%x\n", pwr_sts);
		if (rx.cur_5v_sts == 0) {
			set_fsm_state(FSM_5V_LOST);
			rx.err_code = ERR_5V_LOST;
			vic_check_en = false;
			dvi_check_en = true;
			pre_port = 0xff;
		}
	}
	rx.cur_5v_sts = (pwr_sts >> rx.port) & 1;
	/* inform hdcp_rx22 the 5v sts of rx */
	rx_5v_sts_to_esm(pwr_sts);
	tmp_arc_5v = (pwr_sts >> rx.arc_port) & 1;
	if (earc_hdmirx_hpdst && rx.arc_5vsts != tmp_arc_5v) {
		rx.arc_5vsts = tmp_arc_5v;
		earc_hdmirx_hpdst(rx.arc_port, rx.arc_5vsts);
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
	u32 ch0, ch1, ch2;

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
	//if (clk_debug_en)
		//rx_cable_clk_monitor();

	rx.timestamp++;
	if (rx.err_code == ERR_NONE)
		return;

	if (err_dbg_cnt++ > err_dbg_cnt_max)
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

static void hdcp22_decrypt_monitor(void)
{
	if (!hdcp22_on)
		return;

	if (rx.cur.hdcp_type) {
		if (rx.last_hdcp22_state != rx.cur.hdcp22_state) {
			if (rx.state == FSM_SIG_READY)
				skip_frame(skip_frame_cnt);
			if (log_level & VIDEO_LOG)
				rx_pr("hdcp22 decrypt chg(%d->%d)\n",
				      rx.last_hdcp22_state,
				      rx.cur.hdcp22_state);
			rx.last_hdcp22_state = rx.cur.hdcp22_state;
		}
	} else {
		rx.last_hdcp22_state = 0;
	}
}

/*
 * FUNC: rx_main_state_machine
 * signal detection main process
 */
void rx_main_state_machine(void)
{
	int pre_auds_ch_alloc;
	int pre_auds_hbr;
	int one_frame_cnt;

	switch (rx.state) {
	case FSM_5V_LOST:
		if (rx.cur_5v_sts)
			rx.state = FSM_INIT;
		fsm_restart();
		break;
	case FSM_HPD_LOW:
		/* disable irq before hpd low */
		rx_irq_en(false);
		rx_set_cur_hpd(0, 0);
		set_scdc_cfg(1, 0);
		rx.state = FSM_INIT;
		break;
	case FSM_INIT:
		signal_status_init();
		rx.clk.cable_clk = 0;
		rx.state = FSM_HPD_HIGH;
		break;
	case FSM_HPD_HIGH:
		if (rx.cur_5v_sts == 0)
			break;
		hpd_wait_cnt++;
		if (rx_get_cur_hpd_sts() == 0) {
			if (rx_hpd_keep_low())
				break;
			hdmirx_hw_config();
		} else if ((pre_port != rx.port) &&
			(!rx_need_support_fastswitch())) {
			hdmirx_hw_config();
		}
		hpd_wait_cnt = 0;
		clk_unstable_cnt = 0;
		esd_phy_rst_cnt = 0;
		downstream_hpd_flag = 0;
		edid_update_flag = 0;
		pre_port = rx.port;
		rx_set_cur_hpd(1, 0);
		rx.clk.cable_clk = 0;
		rx.phy.cablesel = 0;
		set_scdc_cfg(0, 1);
		/* rx.hdcp.hdcp_version = HDCP_VER_NONE; */
		rx.state = FSM_WAIT_CLK_STABLE;
		break;
	case FSM_WAIT_CLK_STABLE:
		if (rx.cur_5v_sts == 0)
			break;
		if (rx.cableclk_stb_flg && !rx.ddc_filter_en) {
			if (clk_unstable_cnt != 0) {
				if (clk_stable_cnt < clk_stable_max) {
					clk_stable_cnt++;
					rx_pr("wait clk cnt %d\n", clk_unstable_cnt);
					break;
				}
			}
			rx.state = FSM_EQ_START;
			clk_stable_cnt = 0;
			clk_unstable_cnt = 0;
			rx_pr("clk stable=%d\n", rx.clk.cable_clk);
			rx.err_code = ERR_NONE;
			rx.var.de_stable = false;
			hdmirx_irq_hdcp_enable(true);
		} else {
			clk_stable_cnt = 0;
			if (clk_unstable_cnt < clk_unstable_max) {
				clk_unstable_cnt++;
				break;
			}
			clk_unstable_cnt = 0;
			if (esd_phy_rst_cnt < esd_phy_rst_max &&
				!rx.ddc_filter_en) {
				hdmirx_phy_init();
				rx.clk.cable_clk = 0;
				esd_phy_rst_cnt++;
			} else {
				rx.err_code = ERR_NONE;
				rx.state = FSM_HPD_LOW;
				rx_i2c_err_monitor();
				hdmi_rx_top_edid_update();
				rx.ddc_filter_en = false;
				esd_phy_rst_cnt = 0;
				break;
			}
			rx.err_code = ERR_CLK_UNSTABLE;
		}
		break;
	case FSM_EQ_START:
		if (!rx.cableclk_stb_flg) {
			rx.state = FSM_WAIT_CLK_STABLE;
			break;
		}
		rx_run_eq();
		rx.state = FSM_WAIT_EQ_DONE;
		break;
	case FSM_WAIT_EQ_DONE:
		if (!rx.cableclk_stb_flg) {
			rx.state = FSM_WAIT_CLK_STABLE;
			break;
		}
		if (rx_eq_done()) {
			rx.state = FSM_SIG_UNSTABLE;
			pll_lock_cnt = 0;
			pll_unlock_cnt = 0;
			clk_chg_cnt = 0;
			//esd_phy_rst_cnt = 0;
		}
		break;
	case FSM_SIG_UNSTABLE:
		if (!rx.cableclk_stb_flg) {
			rx.state = FSM_WAIT_CLK_STABLE;
			break;
		}
		if (is_tmds_valid()) {
			/* pll_unlock_cnt = 0; */
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
					//rx.clk.cable_clk = 0;
					//hdmirx_phy_init();
				} else {
					esd_phy_rst_cnt = 0;
					rx.err_rec_mode = ERR_REC_HPD_RST;
				}
			} else if (rx.err_rec_mode == ERR_REC_HPD_RST) {
				rx_set_cur_hpd(0, 2);
				rx.clk.cable_clk = 0;
				rx.state = FSM_INIT;
				rx.err_rec_mode = ERR_REC_EQ_RETRY;
			}
			rx_set_eq_run_state(E_EQ_START);
		}
		break;
	case FSM_SIG_WAIT_STABLE:
		if (!rx.cableclk_stb_flg) {
			rx.state = FSM_WAIT_CLK_STABLE;
			break;
		}
		if (!is_tmds_valid()) {
			rx.state = FSM_SIG_UNSTABLE;
			break;
		}
		dwc_rst_wait_cnt++;
		if (dwc_rst_wait_cnt < dwc_rst_wait_cnt_max)
			break;
		if (edid_update_flag &&
		    dwc_rst_wait_cnt < edid_update_delay)
			break;
		edid_update_flag = 0;
		dwc_rst_wait_cnt = 0;
		sig_stable_cnt = 0;
		sig_unstable_cnt = 0;
		sig_stable_err_cnt = 0;
		clk_chg_cnt = 0;
		rx_pkt_initial();
		rx.state = FSM_SIG_STABLE;
		break;
	case FSM_SIG_STABLE:
		if (!rx.cableclk_stb_flg) {
			rx.state = FSM_WAIT_CLK_STABLE;
			break;
		}
		memcpy(&rx.pre, &rx.cur, sizeof(struct rx_video_info));
		rx_get_video_info();
		if (rx_is_timing_stable()) {
			if (++sig_stable_cnt >= sig_stable_max) {
				get_timing_fmt();
				/* timing stable, start count vsync and avi pkt */
				rx.var.de_stable = true;
				sig_unstable_cnt = 0;
				if (is_unnormal_format(sig_stable_cnt))
					break;
				/* if format vic is abnormal, do hw
				 * reset once to try to recover.
				 */
				if (fmt_vic_abnormal()) {
					if (vic_check_en) {
						/* hdmi_rx_top_edid_update(); */
						hdmirx_hw_config();
						rx.state = FSM_HPD_LOW;
						vic_check_en = false;
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
					is_ddc_filter_en() &&
				    rx.hdcp.hdcp_version == HDCP_VER_NONE) {
					rx.state = FSM_HPD_LOW;
					rx_i2c_init();
					dvi_check_en = false;
					break;
				}
				sig_stable_cnt = 0;
				rx.skip = 0;
				rx.var.mute_cnt = 0;
				rx.state = FSM_SIG_READY;
				rx.aud_sr_stable_cnt = 0;
				rx.aud_sr_unstable_cnt = 0;
				rx.no_signal = false;
				rx.ecc_err = 0;
				clk_chg_cnt = 0;
				/*memset(&rx.aud_info, 0,*/
					/*sizeof(struct aud_info_s));*/
				/*rx_set_eq_run_state(E_EQ_PASS);*/
				hdmirx_config_video();
				rx_get_audinfo(&rx.aud_info);
				hdmirx_config_audio();
				rx_aud_pll_ctl(1);
				rx_afifo_store_all_subpkt(false);
				hdmirx_audio_fifo_rst();
				rx.hdcp.hdcp_pre_ver = rx.hdcp.hdcp_version;
				rx.stable_timestamp = rx.timestamp;
				rx_pr("Sig ready\n");
				dump_state(RX_DUMP_VIDEO);
				#ifdef K_TEST_CHK_ERR_CNT
				rx_monitor_error_cnt_start();
				#endif
				sig_stable_err_cnt = 0;
				esd_phy_rst_cnt = 0;
				rx.ddc_filter_en = false;
			}
		} else {
			sig_stable_cnt = 0;
			rx.var.de_stable = false;
			if (sig_unstable_cnt < sig_unstable_max) {
				sig_unstable_cnt++;
				break;
			}
			if (rx.err_rec_mode == ERR_REC_EQ_RETRY) {
				rx.state = FSM_WAIT_CLK_STABLE;
				rx.phy.cablesel++;
				if (esd_phy_rst_cnt++ >= esd_phy_rst_max) {
					rx.err_rec_mode = ERR_REC_HPD_RST;
					rx_set_eq_run_state(E_EQ_START);
					esd_phy_rst_cnt = 0;
				}
			} else if (rx.err_rec_mode == ERR_REC_HPD_RST) {
				rx_set_cur_hpd(0, 2);
				rx.clk.cable_clk = 0;
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
		if (!is_tmds_valid()) {
			if (video_mute_enabled()) {
				set_video_mute(true);
				rx.var.mute_cnt = 0;
				if (log_level & 0x100)
					rx_pr("vpp mute\n");
			}
			skip_frame(skip_frame_cnt);
			rx.unready_timestamp = rx.timestamp;
			rx.err_code = ERR_TIMECHANGE;
			rx_i2c_init();
			dump_unnormal_info();
			rx_pr("tmds_invalid-->unready\n");
			rx.var.de_stable = false;
			sig_unready_cnt = 0;
			audio_sample_rate = 0;
			rx_aud_pll_ctl(0);
			rx.hdcp.hdcp_pre_ver = rx.hdcp.hdcp_version;
			/* need to clr to none, for dishNXT box */
			rx.hdcp.hdcp_version = HDCP_VER_NONE;
			//rx_sw_reset(2);
			hdmirx_top_irq_en(false);
			hdmirx_output_en(false);
			hdmirx_hbr2spdif(0);
			rx.state = FSM_WAIT_CLK_STABLE;
			vic_check_en = false;
			rx.skip = 0;
			rx.var.mute_cnt = 0;
			rx.aud_sr_stable_cnt = 0;
			rx.aud_sr_unstable_cnt = 0;
			rx.clk.cable_clk = 0;
			esd_phy_rst_cnt = 0;
			rx_esm_reset(0);
			break;
		} else if (!rx_is_timing_stable()) {
			skip_frame(skip_frame_cnt);
			if (++sig_unready_cnt >= sig_unready_max) {
				/*sig_lost_lock_cnt = 0;*/
				rx.unready_timestamp = rx.timestamp;
				rx.err_code = ERR_TIMECHANGE;
				rx_i2c_init();
				dump_unnormal_info();
				rx_pr("timing unstable-->unready\n");
				rx.var.de_stable = false;
				sig_unready_cnt = 0;
				audio_sample_rate = 0;
				rx_aud_pll_ctl(0);
				rx.hdcp.hdcp_pre_ver = rx.hdcp.hdcp_version;
				/* need to clr to none, for dishNXT box */
				rx.hdcp.hdcp_version = HDCP_VER_NONE;
				//rx_sw_reset(2);
				hdmirx_top_irq_en(false);
				hdmirx_output_en(false);
				hdmirx_hbr2spdif(0);
				rx.state = FSM_WAIT_CLK_STABLE;
				vic_check_en = false;
				rx.skip = 0;
				rx.var.mute_cnt = 0;
				rx.aud_sr_stable_cnt = 0;
				rx.aud_sr_unstable_cnt = 0;
				rx.clk.cable_clk = 0;
				esd_phy_rst_cnt = 0;
				rx_esm_reset(0);
				break;
			}
		} else {
			sig_unready_cnt = 0;
			one_frame_cnt = (1000 * 100 / rx.pre.frame_rate / 12) + 1;
			if (rx.skip > 0) {
				rx.skip--;
			} else if (video_mute_enabled()) {
				/* clear vpp mute after signal stable */
				if (get_video_mute()) {
					if (rx.var.mute_cnt++ < one_frame_cnt + 1)
						break;
					rx.var.mute_cnt = 0;
					set_video_mute(false);
				}
			}
		}
		if (rx.pre.sw_dvi == 1)
			break;
		packet_update();
		hdcp_sts_update();
		pre_auds_ch_alloc = rx.aud_info.auds_ch_alloc;
		pre_auds_hbr = rx.aud_info.aud_hbr_rcv;
		rx_get_audinfo(&rx.aud_info);

		if (check_real_sr_change())
			rx_audio_pll_sw_update();
		if (pre_auds_ch_alloc != rx.aud_info.auds_ch_alloc ||
		    (pre_auds_hbr != rx.aud_info.aud_hbr_rcv &&
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
					rx.clk.cable_clk = 0;
					rx_pr("reqclk err->wait_clk\n");
				} else if (aud_sts == E_PLLRATE_CHG) {
					rx_aud_pll_ctl(1);
				} else if (aud_sts == E_AUDCLK_ERR) {
					rx_audio_bandgap_rst();
				} else {
					rx_acr_info_sw_update();
					rx_audio_pll_sw_update();
				}
				if (log_level & AUDIO_LOG)
					rx_pr("update audio-err:%d\n", aud_sts);
				rx.aud_sr_unstable_cnt = 0;
			}
		} else {
			rx.aud_sr_unstable_cnt = 0;
		}
		break;
	default:
		break;
	}
	/* for fsm debug */
	if (rx.state != rx.pre_state) {
		if (log_level & LOG_EN)
			rx_pr("fsm %d(%s) to %d(%s)\n",
			      rx.pre_state, fsm_st[rx.pre_state],
			      rx.state, fsm_st[rx.state]);
		rx.pre_state = rx.state;
	}
}

unsigned int hdmirx_show_info(unsigned char *buf, int size)
{
	int pos = 0;
	struct drm_infoframe_st *drmpkt;
	enum edid_ver_e edid_slt = get_edid_selection(rx.port);

	drmpkt = (struct drm_infoframe_st *)&rx_pkt.drm_info;

	pos += snprintf(buf + pos, size - pos,
		"HDMI info\n\n");
	if (rx.cur.colorspace == E_COLOR_RGB)
		pos += snprintf(buf + pos, size - pos,
			"Color Space: %s\n", "0-RGB");
	else if (rx.cur.colorspace == E_COLOR_YUV422)
		pos += snprintf(buf + pos, size - pos,
			"Color Space: %s\n", "1-YUV422");
	else if (rx.cur.colorspace == E_COLOR_YUV444)
		pos += snprintf(buf + pos, size - pos,
			"Color Space: %s\n", "2-YUV444");
	else if (rx.cur.colorspace == E_COLOR_YUV420)
		pos += snprintf(buf + pos, size - pos,
			"Color Space: %s\n", "3-YUV420");
	pos += snprintf(buf + pos, size - pos,
		"Dvi: %d\n", rx.cur.hw_dvi);
	pos += snprintf(buf + pos, size - pos,
		"Interlace: %d\n", rx.cur.interlaced);
	pos += snprintf(buf + pos, size - pos,
		"Htotal: %d\n", rx.cur.htotal);
	pos += snprintf(buf + pos, size - pos,
		"Hactive: %d\n", rx.cur.hactive);
	pos += snprintf(buf + pos, size - pos,
		"Vtotal: %d\n", rx.cur.vtotal);
	pos += snprintf(buf + pos, size - pos,
		"Vactive: %d\n", rx.cur.vactive);
	pos += snprintf(buf + pos, size - pos,
		"Repetition: %d\n", rx.cur.repeat);
	pos += snprintf(buf + pos, size - pos,
		"Color Depth: %d\n", rx.cur.colordepth);
	pos += snprintf(buf + pos, size - pos,
		"Frame Rate: %d\n", rx.cur.frame_rate);
	pos += snprintf(buf + pos, size - pos,
		"Skip frame: %d\n", rx.skip);
	pos += snprintf(buf + pos, size - pos,
		"avmute skip: %d\n", rx.avmute_skip);
	pos += snprintf(buf + pos, size - pos,
		"TMDS clock: %d\n", rx.clk.cable_clk);
	if (rx.chip_id < CHIP_ID_T7)
		pos += snprintf(buf + pos, size - pos,
			"Pixel clock: %d\n", rx.clk.pixel_clk);
	if (drmpkt->des_u.tp1.eotf == EOTF_SDR)
		pos += snprintf(buf + pos, size - pos,
		"HDR EOTF: %s\n", "SDR");
	else if (drmpkt->des_u.tp1.eotf == EOTF_HDR)
		pos += snprintf(buf + pos, size - pos,
		"HDR EOTF: %s\n", "HDR");
	else if (drmpkt->des_u.tp1.eotf == EOTF_SMPTE_ST_2048)
		pos += snprintf(buf + pos, size - pos,
		"HDR EOTF: %s\n", "SMPTE_ST_2048");
	else if (drmpkt->des_u.tp1.eotf == EOTF_HLG)
		pos += snprintf(buf + pos, size - pos,
		"HDR EOTF: %s\n", "HLG");
	pos += snprintf(buf + pos, size - pos,
		"Dolby Vision: %d\n",
		rx.vs_info_details.dolby_vision_flag);

	pos += snprintf(buf + pos, size - pos,
		"\n\nAudio info\n\n");
	pos += snprintf(buf + pos, size - pos,
		"CTS: %d\n", rx.aud_info.cts);
	pos += snprintf(buf + pos, size - pos,
		"N: %d\n", rx.aud_info.n);
	pos += snprintf(buf + pos, size - pos,
		"Recovery clock: %d\n", rx.aud_info.arc);
	pos += snprintf(buf + pos, size - pos,
		"audio receive data: %d\n", auds_rcv_sts);
	pos += snprintf(buf + pos, size - pos,
		"Audio PLL clock: %d\n", rx.clk.aud_pll);
	if (rx.chip_id <= CHIP_ID_TXLX)
		pos += snprintf(buf + pos, size - pos,
			"mpll_div_clk: %d\n", rx.clk.mpll_clk);
	pos += snprintf(buf + pos, size - pos,
		"edid_select_ver: %s\n", edid_slt == EDID_V20 ? "2.0" : "1.4");

	pos += snprintf(buf + pos, size - pos,
		"\n\nHDCP info\n\n");
	pos += snprintf(buf + pos, size - pos,
		"HDCP Debug Value: 0x%x\n", hdmirx_rd_dwc(DWC_HDCP_DBG));
	pos += snprintf(buf + pos, size - pos,
		"HDCP14 state: %d\n", rx.cur.hdcp14_state);
	pos += snprintf(buf + pos, size - pos,
		"HDCP22 state: %d\n", rx.cur.hdcp22_state);
	if (rx.port == E_PORT0)
		pos += snprintf(buf + pos, size - pos,
			"Source Physical address: %d.0.0.0\n", 1);
	else if (rx.port == E_PORT1)
		pos += snprintf(buf + pos, size - pos,
			"Source Physical address: %d.0.0.0\n", 3);
	else if (rx.port == E_PORT2)
		pos += snprintf(buf + pos, size - pos,
			"Source Physical address: %d.0.0.0\n", 2);
	else if (rx.port == E_PORT3)
		pos += snprintf(buf + pos, size - pos,
			"Source Physical address: %d.0.0.0\n", 4);
	pos += snprintf(buf + pos, size - pos,
		"HDCP1.4 secure: %d\n", rx_set_hdcp14_secure_key());
	if (rx.chip_id >= CHIP_ID_T7)
		return pos;
	if (hdcp22_on) {
		pos += snprintf(buf + pos, size - pos,
			"HDCP22_ON: %d\n", hdcp22_on);
		pos += snprintf(buf + pos, size - pos,
			"HDCP22 sts: 0x%x\n", rx_hdcp22_rd_reg(0x60));
		pos += snprintf(buf + pos, size - pos,
			"HDCP22_capable_sts: %d\n", hdcp22_capable_sts);
		pos += snprintf(buf + pos, size - pos,
			"sts0x8fc: 0x%x\n", hdmirx_rd_dwc(DWC_HDCP22_STATUS));
		pos += snprintf(buf + pos, size - pos,
			"sts0x81c: 0x%x\n", hdmirx_rd_dwc(DWC_HDCP22_CONTROL));
	}

	return pos;
}

static void dump_phy_status(void)
{
	if (rx.phy_ver == PHY_VER_TM2)
		dump_aml_phy_sts_tm2();
	else if (rx.phy_ver == PHY_VER_T5)
		dump_aml_phy_sts_t5();
	else if (rx.phy_ver >= PHY_VER_T7)
		dump_aml_phy_sts_t7();
	else
		dump_aml_phy_sts_tl1();
}

static void dump_clk_status(void)
{
	rx_pr("[HDMI clk info]\n");
	rx_pr("top cableclk=%d\n",
	      rx_get_clock(TOP_HDMI_CABLECLK));

	rx_pr("top tmdsclk=%d\n",
	      rx_get_clock(TOP_HDMI_TMDSCLK));
	rx_pr("cable clock = %d\n",
	      rx.clk.cable_clk);
	rx_pr("tmds clock = %d\n",
	      rx.clk.tmds_clk);
	/*rx_pr("Pixel clock = %d\n",*/
	      /*rx_measure_clock(MEASURE_CLK_PIXEL));*/
	rx_pr("audio clock = %d\n",
	      rx.clk.aud_pll);
	/*rx_pr("top audio meter clk=%d\n",*/
	      /*rx_get_clock(TOP_HDMI_AUDIOCLK));*/
	/*rx_pr("aud clk in = %d\n",*/
	     /* rx_measure_clock(MEASURE_CLK_AUD_DIV));*/
	if (rx.chip_id <= CHIP_ID_TXLX)
		rx_pr("mpll clock = %d\n",
		      rx.clk.mpll_clk);
}

static void dump_video_status(void)
{
	enum edid_ver_e edid_slt = (edid_select >> (4 * rx.port)) & 0xF;
	enum edid_ver_e edid_ver =
		rx_parse_edid_ver(rx_get_cur_edid(rx.port));

	rx_get_video_info();
	rx_pr("[HDMI info]\n");
	rx_pr("colorspace %d,", rx.cur.colorspace);
	rx_pr("colordepth %d\n", rx.cur.colordepth);
	rx_pr("interlace %d\n", rx.cur.interlaced);
	rx_pr("htotal %d\n", rx.cur.htotal);
	rx_pr("hactive %d\n", rx.cur.hactive);
	rx_pr("vtotal %d\n", rx.cur.vtotal);
	rx_pr("vactive %d\n", rx.cur.vactive);
	rx_pr("frame_rate %d\n", rx.cur.frame_rate);
	rx_pr("repetition %d\n", rx.cur.repeat);
	rx_pr("dvi %d,", rx.cur.hw_dvi);
	rx_pr("sw_dvi:%d,", rx.pre.sw_dvi);
	rx_pr("fmt=0x%x,", hdmirx_hw_get_fmt());
	rx_pr("hw_vic %d,", rx.cur.hw_vic);
	rx_pr("sw_vic %d,", rx.pre.sw_vic);
	rx_pr("rx.no_signal=%d,rx.state=%d,",
	      rx.no_signal, rx.state);
	rx_pr("VRR en = %d\n", rx.vtem_info.vrr_en);
	rx_pr("skip frame=%d\n", rx.skip);
	rx_pr("avmute_skip:0x%x\n", rx.avmute_skip);
	rx_pr("ecc cnt:%d\n", rx.ecc_err);
	rx_pr("****pkts_info_details:*****\n");
	rx_pr("hdr10plus = %d\n", rx.vs_info_details.hdr10plus);
	rx_pr("allm_mode = %d\n", rx.vs_info_details.allm_mode);
	rx_pr("itcontent = %d\n", rx.cur.it_content);
	rx_pr("cnt_type = %d\n", rx.cur.cn_type);
	rx_pr("dolby_vision = %d\n", rx.vs_info_details.dolby_vision_flag);
	rx_pr("dv ll = %d\n", rx.vs_info_details.low_latency);
	//rx_pr("VTEM = %d\n", rx.vrr_en);
	rx_pr("DRM = %d\n", rx_pkt_chk_attach_drm());
	rx_pr("freesync = %d\n-bit0 supported,bit1:enabled.bit2:active", rx.free_sync_sts);
	rx_pr("edid_selected_ver: %s\n",
	      edid_slt == EDID_AUTO ?
	      "auto" : (edid_slt == EDID_V20 ? "2.0" : "1.4"));
	rx_pr("edid_parse_ver: %s\n",
	      edid_ver == EDID_V20 ? "2.0" : "1.4");
}

static void dump_audio_status(void)
{
	static struct aud_info_s a;
	u32 val0, val1;

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
	rx_pr("audio receive data:%d\n", auds_rcv_sts);
	rx_pr("aud mute = %d", a.aud_mute_en);
}

static void dump_hdcp_status(void)
{
	rx_pr("HDCP version:%d\n", rx.hdcp.hdcp_version);
	rx_pr("HDCP14 state:%d\n",
	      rx.cur.hdcp14_state);
	rx_pr("HDCP22 state:%d\n",
	      rx.cur.hdcp22_state);
}

void dump_state(int enable)
{
	rx_get_video_info();
	/* video info */
	if (enable == RX_DUMP_VIDEO) {
		dump_video_status();
	} else if (enable & RX_DUMP_ALL) {
		dump_clk_status();
		dump_phy_status();
		dump_video_status();
		dump_audio_status();
		dump_hdcp_status();
		dump_pktinfo_status();
	} else if (enable & RX_DUMP_AUDIO) {
		/* audio info */
		dump_audio_status();
	} else if (enable & RX_DUMP_HDCP) {
		/* hdcp info */
		dump_hdcp_status();
	} else if (enable & RX_DUMP_PHY) {
		/* phy info */
		dump_phy_status();
	} else if (enable & RX_DUMP_CLK) {
		/* clk src info */
		dump_clk_status();
	} else {
		dump_video_status();
	}
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
	u32 value = 0;

	char input[5][20];
	char *const delim = " ";
	char *token;
	char *cur;
	int cnt = 0;
	struct edid_info_s edid_info;
	u_char *pedid = rx_get_cur_edid(rx.port);

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
	} else if (strncmp(tmpbuf, "hpd", 3) == 0) {
		rx_set_cur_hpd((tmpbuf[3] == '0' ? 0 : 1), 4);
	} else if (strncmp(tmpbuf, "cable_status", 12) == 0) {
		size = hdmirx_rd_top(TOP_HPD_PWR5V) >> 20;
		rx_pr("cable_status = %x\n", size);
	} else if (strncmp(tmpbuf, "signal_status", 13) == 0) {
		size = rx.no_signal;
		rx_pr("signal_status = %d\n", size);
	} else if (strncmp(tmpbuf, "reset", 5) == 0) {
		if (tmpbuf[5] == '0') {
			rx_pr(" hdmirx hw config\n");
			/* hdmi_rx_top_edid_update(); */
			hdmirx_hw_config();
		} else if (tmpbuf[5] == '1') {
			rx_pr(" hdmirx phy init 8bit\n");
			hdmirx_phy_init();
		} else if (tmpbuf[5] == '3') {
			rx_pr(" irq open\n");
			//rx_irq_en(true);
			dump_vsi_reg_t7();
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
		if (tmpbuf[3] == '1')
			dump_reg_phy();
		else
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
		rx_hdcp22_send_uevent(0);
	} else if (strncmp(tmpbuf, "esm1", 4) == 0) {
		/*switch_set_state(&rx.hpd_sdev, 0x01);*/
		/* extcon_set_state_sync(rx.rx_excton_rx22, EXTCON_DISP_HDMI, 1); */
		rx_hdcp22_send_uevent(1);
	} else if (strncmp(input[0], "pktinfo", 7) == 0) {
		rx_debug_pktinfo(input);
	} else if (strncmp(tmpbuf, "parse_edid", 10) == 0) {
		memset(&edid_info, 0, sizeof(struct edid_info_s));
		rx_edid_parse(pedid, &edid_info);
		rx_edid_parse_print(&edid_info);
		rx_blk_index_print(&edid_info.cea_ext_info.blk_parse_info);
	} else if (strncmp(tmpbuf, "parse_capds", 11) == 0) {
		rx_prase_earc_capds_dbg();
	} else if (strncmp(tmpbuf, "splice_capds", 12) == 0) {
		edid_splice_earc_capds_dbg(pedid);
	} else if (strncmp(tmpbuf, "splice_db", 9) == 0) {
		if (kstrtou32(tmpbuf + 9, 16, &value) < 0)
			return -EINVAL;
		edid_splice_data_blk_dbg(pedid, value);
	} else if (strncmp(tmpbuf, "rm_db_tag", 9) == 0) {
		if (kstrtou32(tmpbuf + 9, 16, &value) < 0)
			return -EINVAL;
		edid_rm_db_by_tag(pedid, value);
	} else if (strncmp(tmpbuf, "rm_db_idx", 9) == 0) {
		if (kstrtou32(tmpbuf + 9, 16, &value) < 0)
			return -EINVAL;
		edid_rm_db_by_idx(pedid, value);
	} else if (tmpbuf[0] == 'w') {
		rx_debug_wr_reg(buf, tmpbuf, i);
	} else if (tmpbuf[0] == 'r') {
		rx_debug_rd_reg(buf, tmpbuf);
	} else if (tmpbuf[0] == 'v') {
		rx_pr("------------------\n");
		rx_pr("Hdmirx version0: %s\n", RX_VER0);
		rx_pr("Hdmirx version1: %s\n", RX_VER1);
		rx_pr("Hdmirx version2: %s\n", RX_VER2);
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
		hdmirx_phy_init();
	} else if (strncmp(input[0], "phyeq", 5) == 0) {
		//aml_eq_setting();
		find_best_eq = 0x1111;
		rx.phy.err_sum = 0xffffff;
	} else if (strncmp(tmpbuf, "audio", 5) == 0) {
		hdmirx_audio_fifo_rst();
	} else if (strncmp(tmpbuf, "eqcal", 5) == 0) {
		rx_phy_rt_cal();
	} else if (strncmp(tmpbuf, "empbuf", 5) == 0) {
		rx_pr("cnt=%d\n", rx.empbuff.emppktcnt);
		cnt = rx.empbuff.emppktcnt;
		rx_pr("0x");
		for (i = 0; i < (cnt * 32); i++)
			rx_pr("%02x", emp_buf[i]);
		rx_pr("\nieee=%x\n", rx.empbuff.emp_tagid);
	} else if (strncmp(tmpbuf, "muteget", 7) == 0) {
		rx_pr("mute sts: %x\n", get_video_mute());
	} else if (strncmp(tmpbuf, "muteset", 7) == 0) {
		if (tmpbuf[7] == '0')
			set_video_mute(false);
		else
			set_video_mute(true);
	} else if (strncmp(tmpbuf, "bist", 4) == 0) {
		rx_phy_short_bist();
	} else if (strncmp(tmpbuf, "eye", 3) == 0) {
		sm_pause = 1;
		aml_eq_eye_monitor();
		sm_pause = 0;
	} else if (strncmp(tmpbuf, "iq", 2) == 0) {
		aml_phy_iq_skew_monitor();
	} else if (strncmp(tmpbuf, "crc", 3) == 0) {
		rx_sec_hdcp_cfg_t7();
	} else if (strncmp(tmpbuf, "phyoff", 6) == 0) {
		aml_phy_power_off();
	}
	return 0;
}

void rx_dw_edid_monitor(void)
{
	if (!hdmi_cec_en || (hdmirx_repeat_support()))
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

void rx_ext_state_monitor(void)
{
	if (rx.fsm_ext_state != FSM_NULL) {
		rx.state = rx.fsm_ext_state;
		rx.fsm_ext_state = FSM_NULL;
	}
}

void rx_hpd_monitor(void)
{
	static u8 hpd_wait_cnt0, hpd_wait_cnt1, hpd_wait_cnt2;

	if (!hdmi_cec_en)
		return;

	if (rx.open_fg)
		port_hpd_rst_flag &= ~rx.port;

	if (port_hpd_rst_flag & 1) {
		if (hpd_wait_cnt0++ > hpd_wait_max) {
			rx_set_port_hpd(0, 1);
			hpd_wait_cnt0 = 0;
			port_hpd_rst_flag &= 0xFE;
		}
	} else {
		hpd_wait_cnt0 = 0;
	}
	if (port_hpd_rst_flag & 2) {
		if (hpd_wait_cnt1++ > hpd_wait_max) {
			rx_set_port_hpd(1, 1);
			hpd_wait_cnt1 = 0;
			port_hpd_rst_flag &= 0xFD;
		}
	} else {
		hpd_wait_cnt1 = 0;
	}
	if (port_hpd_rst_flag & 4) {
		if (hpd_wait_cnt2++ > hpd_wait_max) {
			rx_set_port_hpd(2, 1);
			hpd_wait_cnt2 = 0;
			port_hpd_rst_flag &= 0xFB;
		}
	} else {
		hpd_wait_cnt2 = 0;
	}
}

void hdmirx_timer_handler(struct timer_list *t)
{
	struct hdmirx_dev_s *devp = from_timer(devp, t, timer);

	if (term_flag && term_cal_en) {
		rx_phy_rt_cal();
		term_flag = 0;
	}
	rx_5v_monitor();
	rx_check_repeat();
	rx_dw_edid_monitor();
	rx_clkmsr_monitor();
	if (rx.open_fg) {
		rx_nosig_monitor();
		rx_cable_clk_monitor();
		if (!hdmirx_repeat_support()) {
			if (!sm_pause) {
				rx_clkrate_monitor();
				rx_afifo_monitor();
				rx_ddc_active_monitor();
				rx_hdcp_monitor();
				rx_main_state_machine();
				hdcp22_decrypt_monitor();
				rx_ext_state_monitor();
			}
			/* rx_pkt_check_content(); */
			rx_err_monitor();
			#ifdef K_TEST_CHK_ERR_CNT
			if (err_chk_en)
				rx_monitor_error_counter();
			rx_get_best_eq_setting();
			#endif
		}
	} else {
		rx_hpd_monitor();
	}
	devp->timer.expires = jiffies + TIMER_STATE_CHECK;
	add_timer(&devp->timer);
}

