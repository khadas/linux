/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX_H__
#define __HDMI_TX_H__
#include <linux/hdmi.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>

/* L_0 will always be printed, set log level to L_1/2/3 for detail */
#define L_0 0
#define L_1 1
#define L_2 2
#define L_3 3

#define HDCP_STAGE1_RETRY_TIMER 2000 /* unit: ms */
#define HDCP_BSKV_CHECK_TIMER 100
#define HDCP_FAILED_RETRY_TIMER 200
#define HDCP_DS_KSVLIST_RETRY_TIMER 5000//200
#define HDCP_RCVIDLIST_CHECK_TIMER 3000//200
#define HDMI_INFOFRAME_TYPE_EMP 0x7f
#define DEFAULT_STREAM_TYPE 0
#define VIDEO_MUTE_PATH_1 0x8000000 //mute by vid_mute sysfs node
#define VIDEO_MUTE_PATH_2 0x4000000 //mute by stream type 1
#define VIDEO_MUTE_PATH_3 0x2000000 //mute by rx request auth

#define AUDIO_MUTE_PATH_1 0x8000000 //mute by audio module
#define AUDIO_MUTE_PATH_2 0x4000000 //mute by aud_mote sysfs node
#define AUDIO_MUTE_PATH_3 0x2000000 //mute by stream type 1
#define AUDIO_MUTE_PATH_4 0x1000000 //mute by rx request auth

#define AVMUTE_PATH_1 0x80 //mute by avmute sysfs node
#define AVMUTE_PATH_2 0x40 //mute by upstream side request re-auth

struct emp_packet_st;
enum emp_component_conf;

struct hdmi_packet_t {
	u8 hb[3];
	u8 pb[28];
	u8 no_used; /* padding to 32 bytes */
};

#define HDCPTX_IOOPR		0x820000ab
enum hdcptx_oprcmd {
	HDCP_DEFAULT,
	HDCP14_KEY_READY,
	HDCP14_LOADKEY,
	HDCP14_RESULT,
	HDCP22_KEY_READY,
	HDCP22_LOADKEY,
	HDCP22_RESULT,
	HDCP22_SET_TOPO,
	HDCP22_GET_TOPO,
	CONF_ENC_IDX, /* 0: get idx; 1: set idx */
	HDMITX_GET_RTERM, /* get the rterm value */
};

/* DDC bus error codes */
enum ddc_err_t {
	DDC_ERR_NONE = 0x00,
	DDC_ERR_TIMEOUT = 0x01,
	DDC_ERR_NACK = 0x02,
	DDC_ERR_BUSY = 0x03,
	DDC_ERR_HW = 0x04,
	DDC_ERR_LIM_EXCEED = 0x05,
};

enum vrr_type {
	T_VRR_NONE,
	T_VRR_GAME,
	T_VRR_QMS,
};

enum emp_type {
	EMP_TYPE_NONE,
	EMP_TYPE_VRR_GAME = T_VRR_GAME,
	EMP_TYPE_VRR_QMS = T_VRR_QMS,
	EMP_TYPE_SBTM,
	EMP_TYPE_DSC,
	EMP_TYPE_DHDR,
};

#define HDMI_INFOFRAME_EMP_VRR_GAME ((HDMI_INFOFRAME_TYPE_EMP << 8) | (EMP_TYPE_VRR_GAME))
#define HDMI_INFOFRAME_EMP_VRR_QMS ((HDMI_INFOFRAME_TYPE_EMP << 8) | (EMP_TYPE_VRR_QMS))
#define HDMI_INFOFRAME_EMP_VRR_SBTM ((HDMI_INFOFRAME_TYPE_EMP << 8) | (EMP_TYPE_SBTM))
#define HDMI_INFOFRAME_EMP_VRR_DSC ((HDMI_INFOFRAME_TYPE_EMP << 8) | (EMP_TYPE_DSC))
#define HDMI_INFOFRAME_EMP_VRR_DHDR ((HDMI_INFOFRAME_TYPE_EMP << 8) | (EMP_TYPE_DHDR))

u8 hdmi_ddc_status_check(void);
u8 hdmi_ddc_busy_check(void);
void hdmi_ddc_error_reset(void);

void hdmitx21_fmt_attr(struct hdmitx_dev *hdev);
int hdmitx21_hdcp_init(void);
int hdmitx21_uboot_audio_en(void);

int hdmitx21_init_reg_map(struct platform_device *pdev);
void hdmitx21_set_audioclk(u8 hdmitx_aud_clk_div);
void hdmitx21_disable_clk(struct hdmitx_dev *hdev);
u32 hdcp21_rd_hdcp22_ver(void);
void hdmitx_infoframe_send(u16 info_type, u8 *body);
void hdmitx_dhdr_send(u8 *body, int max_size);

/* there are 2 ways to send out infoframes
 * xxx_infoframe_set() will take use of struct xxx_infoframe_set
 * xxx_infoframe_rawset() will directly send with rawdata
 * if info, hb, or pb == NULL, disable send infoframe
 */
void hdmi_vend_infoframe_set(struct hdmi_vendor_infoframe *info);
void hdmi_vend_infoframe_rawset(u8 *hb, u8 *pb);
void hdmi_vend_infoframe2_rawset(u8 *hb, u8 *pb);
int hdmi_vend_infoframe_get(struct hdmitx_dev *hdev, u8 *body);
void hdmi_avi_infoframe_set(struct hdmi_avi_infoframe *info);
void hdmi_avi_infoframe_rawset(u8 *hb, u8 *pb);
int hdmi_avi_infoframe_get(struct hdmitx_dev *hdev, u8 *body);
void hdmi_spd_infoframe_set(struct hdmi_spd_infoframe *info);
void hdmi_audio_infoframe_set(struct hdmi_audio_infoframe *info);
void hdmi_audio_infoframe_rawset(u8 *hb, u8 *pb);
void hdmi_drm_infoframe_set(struct hdmi_drm_infoframe *info);
void hdmi_drm_infoframe_rawset(u8 *hb, u8 *pb);
void hdmi_emp_infoframe_set(enum emp_type type, struct emp_packet_st *info);
void hdmi_emp_frame_set_member(struct emp_packet_st *info,
	enum emp_component_conf conf, u32 val);

/* refer to HDMI2.1A P447 */
enum TARGET_FRAME_RATE {
	TFR_QMSVRR_INACTIVE = 0,
	TFR_23P97,
	TFR_24,
	TFR_25,
	TFR_29P97,
	TFR_30,
	TFR_47P95,
	TFR_48,
	TFR_50,
	TFR_59P94,
	TFR_60,
	TFR_100,
	TFR_119P88,
	TFR_120,
	TFR_MAX,
};

struct mvrr_const_val {
	/* unit: 100  6000, 5994, 5000, 3000, 2997, 2500, 2400, 2397 */
	u16 duration;
	u16 vtotal_fixed; /* vtotal_fixed is mutex with bit_len */
	u8 bit_len; /* current max value is 16 * 8, 128 */
	u8 frac_array[16];
};

struct mvrr_const_st {
	enum hdmi_vic brr_vic; /* the vic of brr */
	const struct mvrr_const_val *val[];
};

/* VRR parameters configuration */
struct vrr_conf_para {
	enum vrr_type type;
	u8 vrr_enabled;
	u8 fva_factor; /* should be larger than 0 */
	u8 brr_vic;
	int duration;
	/* EDID capability, refer to 2.1A P380 */
	u8 fapa_end_extended:1;
	u8 qms_sup:1;
	u8 mdelta_bit:1;
	u8 cinemavrr_bit:1;
	u8 neg_mvrr_bit:1;
	u8 fva_sup:1;
	u8 fapa_start_loc:1;
	u8 qms_tfrmin:1;
	u8 qms_tfrmax:1;
	u8 vrrmin;
	u16 vrrmax;
};

/* Class 0 video timing extended metedata structure for game/fva, 2.1A P445 */
struct vtem_gamevrr_st {
	u8 vrr_en:1; /* MD0 */
	u8 fva_factor_m1:4;
	u8 base_vfront; /* MD1 */
	u16 brr_rate; /* MD2/3 */
};

/* Class 1 video timing extended metedata structure for qms, 2.1A P445 */
struct vtem_qmsvrr_st {
	u8 m_const:1; /* MD0 */
	u8 qms_en:1;
	u8 base_vfront; /* MD1 */
	u16 brr_rate; /* MD2/3 */
	enum TARGET_FRAME_RATE next_tfr:4;
};

struct emp_packet_header {
	u8 header; /* hb0, fixed value 0x7f */
	u8 last:1; /* hb1 */
	u8 first:1;
	u8 seq_idx; /* hb2 */
};

struct emp_packet_0_body {
	u8 sync:1; /* pb0 synchronous metadata */
	u8 vfr:1; /* video format related, cs/cd/resolution */
	u8 afr:1; /* audio format related */
	/* 2b00: periodic pseudo-static MD
	 * 2b01: periodic dynamic MD
	 * 2b10: unique MD
	 */
	u8 ds_type:2;
	u8 end:1;
	u8 new:1;
	/* pb2  0: vendor specific MD 1: defined by 2.1
	 * 2: defined by CTA-861-G  3: defined by VESA
	 */
	u8 org_id;
	u16 ds_tag; /* pb3/4 */
	u16 ds_length; /* pb5/6 */
	union {
		struct vtem_gamevrr_st game_md;
		struct vtem_qmsvrr_st qms_md;
		struct vtem_sbtm_st sbtm_md;
		u8 md[21]; /* pb7~pb27, md0~md20 */
	} md;
};

struct emp_packet_n_body {
	u8 md[28]; /* md(x)~md(x+27) */
};

/* extended metadata packet, 2.1A P304, no checksum in the PB0 */
struct emp_packet_st {
	enum emp_type type;
	struct emp_packet_header header;
	union {
		struct emp_packet_0_body emp0;
		struct emp_packet_n_body empn;
	} body;
};

/* below will be used in the vrr sync handler */
struct tx_vrr_params {
	/* the member conf_params is critical and may change at anytime */
	spinlock_t lock;
	struct vrr_conf_para conf_params;
	const struct mvrr_const_val *mconst_val; /* for qms */
	struct mvrr_const_val game_val; /* for game */
	struct emp_packet_st emp_vrr_pkt;
	u32 frame_cnt; /* set to 0 when vrr_params changed */
	u32 mdelta_limit; /* for mdelta = 1 case */
	u8 fapa_early_cnt;
};

enum avi_component_conf {
	CONF_AVI_CS,
	CONF_AVI_BT2020,
	CONF_AVI_Q01,
	CONF_AVI_YQ01,
	CONF_AVI_VIC,
	CONF_AVI_AR,
	CONF_AVI_CT_TYPE,
};

enum emp_component_conf {
	CONF_HEADER_INIT,
	CONF_HEADER_LAST,
	CONF_HEADER_FIRST,
	CONF_HEADER_SEQ_INDEX,
	CONF_SYNC,
	CONF_VFR,
	CONF_AFR,
	CONF_DS_TYPE,
	CONF_END,
	CONF_NEW,
	CONF_ORG_ID,
	CONF_DATA_SET_TAG,
	CONF_DATA_SET_LENGTH,
	CONF_VRR_EN,
	CONF_FACTOR_M1,
	CONF_QMS_EN,
	CONF_M_CONST,
	CONF_BASE_VFRONT,
	CONF_NEXT_TFR,
	CONF_BASE_REFRESH_RATE,
	CONF_SBTM_VER,
	CONF_SBTM_MODE,
	CONF_SBTM_TYPE,
	CONF_SBTM_GRDM_MIN,
	CONF_SBTM_GRDM_LUM,
	CONF_SBTM_FRMPBLIMITINT,
};

/* CONF_AVI_BT2020 */
#define CLR_AVI_BT2020	0x0
#define SET_AVI_BT2020	0x1
/* CONF_AVI_Q01 */
#define RGB_RANGE_DEFAULT	0
#define RGB_RANGE_LIM		1
#define RGB_RANGE_FUL		2
#define RGB_RANGE_RSVD		3
/* CONF_AVI_YQ01 */
#define YCC_RANGE_LIM		0
#define YCC_RANGE_FUL		1
#define YCC_RANGE_RSVD		2
void hdmi_avi_infoframe_config(enum avi_component_conf conf, u8 val);

int hdmitx_infoframe_rawget(u16 info_type, u8 *body);

void hdmi_gcppkt_manual_set(bool en);

struct intr_t;

typedef void(*hdmi_intr_cb)(struct intr_t *);

struct intr_t {
	const u32 intr_mask_reg;
	const u32 intr_st_reg;
	const u32 intr_clr_reg;
	const unsigned int intr_top_bit;
	u32 mask_data;
	u32 st_data;
	hdmi_intr_cb callback;
};

/* intr_array will be used for ISR quickly save status
 * while the entity.NAME will be used for bottom handler
 */
union intr_u {
	struct {
		struct intr_t top_intr;
		struct intr_t tpi_intr;
		struct intr_t cp2tx_intr0;
		struct intr_t cp2tx_intr1;
		struct intr_t cp2tx_intr2;
		struct intr_t cp2tx_intr3;
		struct intr_t scdc_intr;
		struct intr_t intr2;
		struct intr_t intr5;
	} entity;
};

struct hdcp_work {
	u32 delay_ms;
	u32 period_ms;
	const char *name;
	struct delayed_work dwork;
};

struct hdcp_t {
	bool hdcptx_enabled;
	bool ds_auth;
	bool encryption_enabled;
	bool ds_repeater;
	bool rpt_ready;
	bool update_topology;
	bool update_topo_state;
	bool reauth_ignored;
	bool csm_msg_sent;
	bool csm_valid;
	enum hdcp_fail_types_t fail_type;
	enum hdcp_ver_t req_hdcp_ver;
	enum hdcp_content_type_t content_type;
	enum hdcp_stat_t hdcp_state;
	enum hdcp_ver_t hdcp_type;
	enum hdcp_ver_t hdcp_cap_ds;
	u8 *p_ksv_lists;
	u8 *p_ksv_next;
	struct hdcp_topo_t hdcp_topology;
	struct hdcp_csm_t csm_message;
	struct hdcp_csm_t prev_csm_message;
	union intr_u intr_regs;
	struct workqueue_struct *hdcp_wq;
	struct hdcp_work timer_hdcp_rcv_auth;
	struct hdcp_work timer_hdcp_rpt_auth;
	struct hdcp_work timer_hdcp_auth_fail_retry;
	struct hdcp_work timer_bksv_poll_done;
	struct hdcp_work timer_hdcp_start;
	struct hdcp_work timer_ddc_check_nak;
	struct hdcp_work timer_update_csm;
	struct delayed_work ksv_notify_wk;
	struct delayed_work req_reauth_wk;
	struct delayed_work stream_mute_wk;
	struct delayed_work stream_type_wk;
	/* audio/video mute if upstream type = 1 */
	bool stream_mute;
	/* hdmirx side request flag, see hdmi_rx_repeater.h
	 * STREAMTYPE_UPDATE 0x10
	 * UPSTREAM_INACTIVE 0x20
	 * UPSTREAM_ACTIVE 0x40
	 * 0: notify reauth
	 */
	u8 rx_update_flag;
	/* the stream type notified by upstream side
	 * bit4: if 1, upstream may send stream type before
	 * hdmitx start hdcp(passthrough enabled), need
	 * to save it, and cover the stream type with bit3:0
	 * when hdmitx hdcp propagate stream type. else
	 * hdmitx is the active source and should decide
	 * the stream type itself
	 */
	u8 saved_upstream_type;
	/* 0: auto hdcp version, 1: hdcp1.4, 2: hdcp2.3 */
	u8 req_reauth_ver;
	u8 cont_smng_method;
	/* flag: csm already updated by single csm message */
	bool csm_updated;
	bool hdcp14_second_part_pass;
};

extern unsigned int rx_hdcp2_ver;
bool get_hdcp1_lstore(void);
bool get_hdcp2_lstore(void);
bool get_hdcp1_result(void);
bool get_hdcp2_result(void);
bool is_rx_hdcp2ver(void);
void hdcp_mode_set(unsigned int mode);
void hdcp_enable_intrs(bool en);
void hdcp1x_intr_handler(struct intr_t *intr);
void hdcp2x_intr_handler(struct intr_t *intr);
void intr_status_save_clr_cp2txs(u8 regs[]);
void hdcptx_init_reg(void);
void hdcptx2_src_auth_start(u8 content_type);
void hdcptx1_auth_start(void);
void hdcptx2_auth_stop(void);
void hdcptx1_auth_stop(void);
void hdcptx1_encryption_update(bool en);
void hdcptx2_encryption_update(bool en);
void hdcptx2_reauth_send(void);
void ddc_toggle_sw_tpi(void);
bool hdcptx2_ds_rptr_capability(void);
bool hdcptx1_ds_rptr_capability(void);
void hdcptx1_ds_bksv_read(u8 *p_bksv, u8 ksv_bytes);
u8 hdcptx1_ksv_v_get(void);
void hdcptx1_protection_enable(bool en);
void hdcptx1_intermed_ri_check_enable(bool en);
void hdcptx2_ds_rcv_id_read(u8 *p_rcv_id);
void hdcptx2_ds_rpt_rcvid_list_read(u8 *p_rpt_rcv_id, u8 dev_count, u8 bytes_to_read);
u8 hdcptx1_ds_cap_status_get(void);
u8 hdcptx1_copp_status_get(void);
u16 hdcptx1_get_prime_ri(void);
void hdcptx1_get_ds_ksvlists(u8 **p_ksv, u8 count);
void hdcptx1_bstatus_get(u8 *p_ds_bstatus);
bool hdcptx2_auth_status(void);
u8 hdcptx2_rpt_dev_cnt_get(void);
u8 hdcptx2_rpt_depth_get(void);
u8 hdcptx2_topology_get(void);
void hdcptx2_csm_send(struct hdcp_csm_t *csm_msg);
void hdcptx2_rpt_smng_xfer_start(void);
void hdcptx1_bcaps_get(u8 *p_bcaps_status);
void hdcptx2_smng_auto(bool en);
u8 hdcp2x_get_state_st(void);
void hdcptx1_query_aksv(struct hdcp_ksv_t *p_val);
void hdmitx_top_intr_handler(struct work_struct *work);
void hdmitx_setupirqs(struct hdmitx_dev *phdev);
void intr_status_init_clear(void);
void ddc_toggle_sw_tpi(void);
bool hdmitx_ddcm_read(u8 seg_index, u8 slave_addr, u8 reg_addr, u8 *p_buf, u16 len);
bool hdmitx_ddcm_write(u8 seg_index, u8 slave_addr, u8 reg_addr, u8 data);
enum ddc_err_t hdmitx_ddc_read_1byte(u8 slave_addr, u8 reg_addr, u8 *p_buf);
bool is_cur_mode_hdmi(void);

extern unsigned long hdcp_reauth_dbg;
extern unsigned long streamtype_dbg;
extern unsigned long en_fake_rcv_id;

void pr_hdcp_info(const char *fmt, ...);
void set_hdcp2_topo(u32 topo_type);
bool get_hdcp2_topo(void);

/* VRR parts */
irqreturn_t hdmitx_vrr_vsync_handler(struct hdmitx_dev *hdev);
irqreturn_t hdmitx_emp_vsync_handler(struct hdmitx_dev *hdev);
void tx_vrr_params_init(void);
void hdmitx_set_vrr_para(const struct vrr_conf_para *para);
void hdmitx_vrr_set_maxlncnt(u32 max_lcnt);
u32 hdmitx_vrr_get_maxlncnt(void);
int hdmitx_set_fr_hint(int duration, void *data);
void hdmitx_unregister_vrr(struct hdmitx_dev *hdev);
void hdmitx_register_vrr(struct hdmitx_dev *hdev);
ssize_t _vrr_cap_show(struct device *dev, struct device_attribute *attr,
	char *buf);
int hdmitx_dump_vrr_status(struct seq_file *s, void *p);
void hdmitx_vrr_enable(void);
void hdmitx_vrr_disable(void);
u8 hdmitx_reauth_request(u8 hdcp_version);
void set_hdcp2_topo(u32 topo_type);
bool get_hdcp2_topo(void);
bool is_current_4k_format(void);
void hdmitx21_enable_hdcp(struct hdmitx_dev *hdev);
void hdmitx21_disable_hdcp(struct hdmitx_dev *hdev);
void hdmitx21_rst_stream_type(struct hdcp_t *hdcp);
bool hdcp_need_control_by_upstream(struct hdmitx_dev *hdev);
int likely_frac_rate_mode(const char *m);
u32 hdmitx21_get_hdcp_mode(void);
extern unsigned long hdcp_reauth_dbg;
extern unsigned long streamtype_dbg;
extern unsigned long en_fake_rcv_id;
extern unsigned long avmute_ms;
extern unsigned long vid_mute_ms;
bool hdmitx21_edid_only_support_sd(struct hdmitx_dev *hdev);
bool is_4k_sink(struct hdmitx_dev *hdev);
void hdmitx21_av_mute_op(u32 flag, unsigned int path);
void hdmitx_clks_gate_ctrl(bool en);

/* FRL */
struct frl_work {
	u32 delay_ms;
	u32 period_ms;
	const char *name;
	struct delayed_work dwork;
};

struct frl_train_t {
	struct workqueue_struct *frl_wq;
	struct frl_work timer_frl_flt;
	u8 src_test_cfg;
	u8 lane_count;
	u8 update_flags;
	enum flt_tx_states flt_tx_state;
	enum flt_tx_states last_state;
	enum frl_rate_enum max_frl_rate;
	enum frl_rate_enum max_edid_frl_rate;
	enum frl_rate_enum user_max_frl_rate;
	enum frl_rate_enum min_frl_rate;
	enum frl_rate_enum frl_rate;
	enum ffe_levels max_ffe_level;
	enum ffe_levels ffe_level[4];
	bool ds_frl_support;
	bool req_legacy_mode;
	bool frl_rate_no_change;
	bool req_frl_mode;
	bool txffe_pre_shoot_only;
	bool txffe_de_emphasis_only;
	bool txffe_no_ffe;
	bool flt_no_timeout;
	bool flt_timeout;
	bool auto_ffe_update;
	bool auto_pattern_update;
	bool flt_running; /* if flt_running is false, return */
};

#define LT_TX_CMD_TXFFE_UPDATE 0x02
#define LT_TX_CMD_LTP_UPDATE   0x03
void scdc_bus_stall_set(bool en);
u16 scdc_tx_ltp0123_get(void);
bool scdc_tx_frl_cfg1_set(u8 cfg1);
u8 scdc_tx_update_flags_get(void);
bool scdc_tx_update_flags_set(u8 update_flags);
u8 scdc_tx_flt_ready_status_get(void);
u8 scdc_tx_sink_version_get(void);
void scdc_tx_source_version_set(u8 src_ver);
u8 scdc_tx_source_test_cfg_get(void);
bool flt_tx_cmd_execute(u8 lt_cmd);
void flt_tx_ltp_req_write(u8 ltp01, u8 ltp23);
void flt_tx_update_set(void);
void frl_tx_start_mod(bool start);
bool flt_tx_update_cleared_wait(void);
bool frl_tx_rate_written(void);
u8 flt_tx_cfg1_get(void);
u8 frl_tx_tx_get_rate(void);
void frl_tx_tx_enable(bool enable);
void frl_tx_av_enable(bool enable);
void frl_tx_sb_enable(bool enable, enum frl_rate_enum frl_rate);
bool frl_tx_pattern_init(u16 patterns);
void frl_tx_pattern_stop(void);
void frl_tx_pin_swap_set(bool en);
bool frl_tx_pattern_set(enum ltp_patterns frl_pat, u8 lane);
bool frl_tx_ffe_set(enum ffe_levels ffe_level, u8 lane);
bool frl_tx_tx_phy_init(bool disable_ffe);
void frl_tx_tx_init(void);
void frl_tx_tx_phy_set(void);
void tmds_tx_phy_set(void);
void frl_tx_lts_1_hdmi21_config(void);
#ifdef CONFIG_AMLOGIC_DSC
void hdmitx_dsc_cvtem_pkt_send(struct dsc_pps_data_s *pps,
	struct hdmi_timing *timing);
#endif
void hdmitx_dsc_cvtem_pkt_disable(void);
enum frl_rate_enum hdmitx21_select_frl_rate(bool dsc_en, enum hdmi_vic vic,
	enum hdmi_colorspace cs, enum hdmi_color_depth cd);
void frl_tx_training_handler(struct hdmitx_dev *hdev);
void frl_tx_stop(struct hdmitx_dev *hdev);
void hdcptx_en_aes_dualpipe(bool en);
bool frl_check_full_bw(enum hdmi_colorspace cs, enum hdmi_color_depth cd, u32 pixel_clock,
	u32 h_active, enum frl_rate_enum frl_rate, u32 *tri_bytes);
void fifo_flow_enable_intrs(bool en);
void hdmitx_soft_reset(u32 bits);
bool ddc_bus_wait_free(void);

#endif /* __HDMI_TX_H__ */
