/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX_H__
#define __HDMI_TX_H__
#include <linux/hdmi.h>
#include <linux/amlogic/media/vout/hdmi_tx21/hdmi_tx_module.h>

#define HDCP_STAGE1_RETRY_TIMER 2000 /* unit: ms */
#define HDCP_BSKV_CHECK_TIMER 100
#define HDCP_FAILED_RETRY_TIMER 200
#define HDCP_DS_KSVLIST_RETRY_TIMER 5000
#define HDCP_RCVIDLIST_CHECK_TIMER 3000

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
void hdmitx_infoframe_send(u8 info_type, u8 *body);

/* there are 2 ways to send out infoframes
 * xxx_infoframe_set() will take use of struct xxx_infoframe_set
 * xxx_infoframe_rawset() will directly send with rawdata
 * if info, hb, or pb == NULL, disable send infoframe
 */
void hdmi_vend_infoframe_set(struct hdmi_vendor_infoframe *info);
void hdmi_vend_infoframe_rawset(u8 *hb, u8 *pb);
void hdmi_avi_infoframe_set(struct hdmi_avi_infoframe *info);
void hdmi_avi_infoframe_rawset(u8 *hb, u8 *pb);
void hdmi_spd_infoframe_set(struct hdmi_spd_infoframe *info);
void hdmi_audio_infoframe_set(struct hdmi_audio_infoframe *info);
void hdmi_audio_infoframe_rawset(u8 *hb, u8 *pb);
void hdmi_drm_infoframe_set(struct hdmi_drm_infoframe *info);
void hdmi_drm_infoframe_rawset(u8 *hb, u8 *pb);

enum avi_component_conf {
	CONF_AVI_CS,
	CONF_AVI_BT2020,
	CONF_AVI_Q01,
	CONF_AVI_YQ01,
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

int hdmitx_infoframe_rawget(u8 info_type, u8 *body);

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
		struct intr_t intr2;
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
};

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
void ddc_toggle_sw_tpi(void);
bool hdmitx_ddcm_read(u8 seg_index, u8 slave_addr, u8 reg_addr, u8 *p_buf, uint16_t len);

#endif /* __HDMI_TX_H__ */
