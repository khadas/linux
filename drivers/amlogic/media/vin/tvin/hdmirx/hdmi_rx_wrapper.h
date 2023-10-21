/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef HDMI_RX_WRAPPER_H_
#define HDMI_RX_WRAPPER_H_

/* stable_check_lvl */
#define HACTIVE_EN		0x01
#define VACTIVE_EN		0x02
#define HTOTAL_EN		0x04
#define VTOTAL_EN		0x08
#define COLSPACE_EN		0x10
#define REFRESH_RATE_EN	0x20
#define REPEAT_EN		0x40
#define DVI_EN			0x80
#define INTERLACED_EN		0x100
#define HDCP_ENC_EN		0x200
#define COLOR_DEP_EN		0x400
#define ERR_CNT_EN		0x800
#define TMDS_VALID_EN		0x1000
#define ECC_ERR_CNT_EN      0x2000

/* aud sample rate stable range */
/* #define AUD_SR_RANGE 2000 */
#define PHY_REQUEST_CLK_MIN	170000000
#define PHY_REQUEST_CLK_MAX	370000000
#define TIMER_STATE_CHECK	(1 * HZ / 100)

struct freq_ref_s {
	bool interlace;
	u8 cd420;
	u8 type_3d;
	unsigned int hactive;
	unsigned int vactive;
	enum hdmi_vic_e vic;
};

enum fsm_states_e {
	FSM_5V_LOST,
	FSM_INIT,
	FSM_HPD_LOW,
	FSM_HPD_HIGH,
	FSM_WAIT_CLK_STABLE,
	FSM_EQ_START,
	FSM_WAIT_EQ_DONE,
	FSM_SIG_UNSTABLE,
	FSM_SIG_WAIT_STABLE,
	FSM_SIG_STABLE,
	FSM_SIG_READY,
	FSM_NULL,
};

enum err_code_e {
	ERR_NONE,
	ERR_5V_LOST,
	ERR_CLK_UNSTABLE,
	ERR_PHY_UNLOCK,
	ERR_DE_UNSTABLE,
	ERR_NO_HDCP14_KEY,
	ERR_TIMECHANGE,
	ERR_UNKNOWN
};

enum irq_flag_e {
	IRQ_AUD_FLAG = 0x01,
	IRQ_PACKET_FLAG = 0x02,
	IRQ_PACKET_ERR = 0x04,
};

enum hdcp22_auth_state_e {
	HDCP22_AUTH_STATE_NOT_CAPABLE,
	HDCP22_AUTH_STATE_CAPABLE,
	HDCP22_AUTH_STATE_LOST,
	HDCP22_AUTH_STATE_SUCCESS,
	HDCP22_AUTH_STATE_FAILED,
};

enum esm_recovery_mode_e {
	ESM_REC_MODE_RESET = 1,
	ESM_REC_MODE_TMDS,
};

enum err_recovery_mode_e {
	ERR_REC_EQ_RETRY,
	ERR_REC_HPD_RST,
	ERR_REC_END
};

enum aud_clk_err_e {
	E_AUDPLL_OK,
	E_REQUESTCLK_ERR,
	E_PLLRATE_CHG,
	E_AUDCLK_ERR,
};

enum dumpinfo_e {
	RX_DUMP_VIDEO = 0,
	RX_DUMP_ALL = 1,
	RX_DUMP_AUDIO = 0x02,
	RX_DUMP_HDCP = 0x04,
	RX_DUMP_PHY = 0x08,
	RX_DUMP_CLK = 0x10
};

enum fps_e {
	E_0HZ,
	E_24HZ,
	E_25HZ,
	E_30HZ,
	E_50HZ,
	E_60HZ,
	E_72HZ,
	E_75HZ,
	E_100HZ,
	E_120HZ
};

/* signal */
extern u32 force_vic;
extern u32 vpp_mute_enable;
extern u32 emp_dbg;
extern u32 dbg_cs;

void rx_mute_vpp(void);
void dump_audio_status(void);
enum tvin_sig_fmt_e hdmirx_hw_get_fmt(void);
void rx_main_state_machine(void);
void rx_err_monitor(void);
void rx_nosig_monitor(void);
bool rx_is_nosig(void);
bool rx_is_sig_ready(void);
void hdmirx_open_port(enum tvin_port_e port);
void hdmirx_close_port(void);
bool is_clk_stable(void);
unsigned int rx_get_pll_lock_sts(void);
unsigned int rx_get_scdc_clkrate_sts(void);
void set_scdc_cfg(int hpdlow, int pwr_provided);
void fsm_restart(void);
void rx_5v_monitor(void);
void rx_audio_pll_sw_update(void);
void rx_acr_info_sw_update(void);
void rx_sw_reset(int level);
void rx_aud_pll_ctl(bool en);
void hdmirx_timer_handler(struct timer_list *t);
void rx_tmds_resource_allocate(struct device *dev);
void rx_emp_resource_allocate(struct device *dev);
void rx_emp_data_capture(void);
void rx_tmds_data_capture(void);
void dump_state(int enable);
void hdmirx_init_params(void);
void edid_auto_mode_init(void);
void set_video_mute(u32 owner, bool on);
void __attribute__((weak))set_video_mute(u32 owner, bool on)
{
}

int get_video_mute(void);
int __attribute__((weak))get_video_mute(void)
{
	return 0;
}
#endif
