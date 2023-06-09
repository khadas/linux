/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_TX21_MODULE_H
#define _HDMI_TX21_MODULE_H
#include "hdmi_info_global.h"
#include "hdmi_config.h"
#include "hdmi_hdcp.h"
#include "hdmi_tx_ddc.h"
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vrr/vrr.h>
#include <drm/amlogic/meson_connector_dev.h>
#include <linux/miscdevice.h>

#define DEVICE_NAME "amhdmitx21"

/* HDMITX driver version */
#define HDMITX_VER "20220601"

/* chip type */
enum amhdmitx_chip_e {
	MESON_CPU_ID_T7,
	MESON_CPU_ID_S5,
	MESON_CPU_ID_MAX,
};

struct amhdmitx_data_s {
	enum amhdmitx_chip_e chip_type;
	const char *chip_name;
};

/*****************************
 *    hdmitx attr management
 ******************************/

/************************************
 *    hdmitx device structure
 *************************************/
/*  VIC_MAX_VALID_MODE and VIC_MAX_NUM are associated with
 *	HDMITX_VIC_MASK in hdmi_common.h
 */
#define VIC_MAX_VALID_MODE	256 /* consider 4k2k */
/* half for valid vic, half for vic with y420*/
#define VIC_MAX_NUM 512
#define AUD_MAX_NUM 60
struct rx_audiocap {
	u8 audio_format_code;
	u8 channel_num_max;
	u8 freq_cc;
	u8 cc3;
};

struct dolby_vsadb_cap {
	unsigned char rawdata[7 + 1]; // padding extra 1 byte
	unsigned int ieeeoui;
	unsigned char length;
	unsigned char dolby_vsadb_ver;
	unsigned char spk_center:1;
	unsigned char spk_surround:1;
	unsigned char spk_height:1;
	unsigned char headphone_only:1;
	unsigned char mat_48k_pcm_only:1;
};

#define MAX_RAW_LEN 64
struct raw_block {
	int len;
	char raw[MAX_RAW_LEN];
};

enum hd_ctrl {
	VID_EN, VID_DIS, AUD_EN, AUD_DIS, EDID_EN, EDID_DIS, HDCP_EN, HDCP_DIS,
};

struct hdr_dynamic_struct {
	u32 type;
	u32 hd_len;/*hdr_dynamic_length*/
	u8 support_flags;
	u8 optional_fields[20];
};

/* get the corresponding bandwidth of current FRL_RATE, Unit: MHz */
u32 get_frl_bandwidth(const enum frl_rate_enum rate);
u32 calc_frl_bandwidth(u32 pixel_freq, enum hdmi_colorspace cs,
	enum hdmi_color_depth cd);
u32 calc_tmds_bandwidth(u32 pixel_freq, enum hdmi_colorspace cs,
	enum hdmi_color_depth cd);

#define VESA_MAX_TIMING 64
/* refer to hdmi2.1 table 7-36, 32 VIC support y420 */
#define Y420_VIC_MAX_NUM 32

struct rx_cap {
	u32 native_Mode;
	/*video*/
	u32 VIC[VIC_MAX_NUM];
	u32 y420_vic[Y420_VIC_MAX_NUM];
	u32 VIC_count;
	u32 native_vic;
	u32 native_vic2; /* some Rx has two native mode, normally only one */
	enum hdmi_vic vesa_timing[VESA_MAX_TIMING]; /* Max 64 */
	/*audio*/
	struct rx_audiocap RxAudioCap[AUD_MAX_NUM];
	u8 AUD_count;
	u8 RxSpeakerAllocation;
	struct dolby_vsadb_cap dolby_vsadb_cap;
	/*vendor*/
	u32 ieeeoui;
	u8 Max_TMDS_Clock1; /* HDMI1.4b TMDS_CLK */
	u32 hf_ieeeoui;	/* For HDMI Forum */
	u32 Max_TMDS_Clock2; /* HDMI2.0 TMDS_CLK */
	/* CEA861-F, Table 56, Colorimetry Data Block */
	u32 colorimetry_data;
	u32 colorimetry_data2;
	u32 scdc_present:1;
	u32 scdc_rr_capable:1; /* SCDC read request */
	u32 lte_340mcsc_scramble:1;
	u32 dc_y444:1;
	u32 dc_30bit:1;
	u32 dc_36bit:1;
	u32 dc_48bit:1;
	u32 dc_30bit_420:1;
	u32 dc_36bit_420:1;
	u32 dc_48bit_420:1;
	enum frl_rate_enum max_frl_rate;
	u32 cnc0:1; /* Graphics */
	u32 cnc1:1; /* Photo */
	u32 cnc2:1; /* Cinema */
	u32 cnc3:1; /* Game */
	u32 qms_tfr_max:1;
	u32 qms:1;
	u32 mdelta:1;
	u32 qms_tfr_min:1;
	u32 neg_mvrr:1;
	u32 fva:1;
	u32 allm:1;
	u32 fapa_start_loc:1;
	u32 fapa_end_extended:1;
	u32 cinemavrr:1;
	u32 vrr_max;
	u32 vrr_min;
	struct hdr_info hdr_info;
	struct dv_info dv_info;
	/* When hdr_priority is 1, then dv_info will be all 0;
	 * when hdr_priority is 2, then dv_info/hdr_info will be all 0
	 * App won't get real dv_cap/hdr_cap, but can get real dv_cap2/hdr_cap2
	 */
	struct hdr_info hdr_info2;
	struct dv_info dv_info2;
	u8 IDManufacturerName[4];
	u8 IDProductCode[2];
	u8 IDSerialNumber[4];
	u8 ReceiverProductName[16];
	u8 manufacture_week;
	u8 manufacture_year;
	u16 physical_width;
	u16 physical_height;
	u8 edid_version;
	u8 edid_revision;
	u8 ColorDeepSupport;
	u32 vLatency;
	u32 aLatency;
	u32 i_vLatency;
	u32 i_aLatency;
	u32 threeD_present;
	u32 threeD_Multi_present;
	u32 hdmi_vic_LEN;
	u32 HDMI_3D_LEN;
	u32 threeD_Structure_ALL_15_0;
	u32 threeD_MASK_15_0;
	struct {
		u8 frame_packing;
		u8 top_and_bottom;
		u8 side_by_side;
	} support_3d_format[VIC_MAX_NUM];
	enum hdmi_vic preferred_mode;
	struct dtd dtd[16];
	u8 dtd_idx;
	u8 flag_vfpdb;
	u8 number_of_dtd;
	struct raw_block asd;
	struct raw_block vsd;
	/* for DV cts */
	bool ifdb_present;
	/* IFDB, currently only use below node */
	u8 additional_vsif_num;
	/*blk0 check sum*/
	u8 blk0_chksum;
	u8 chksum[10];
};

struct cts_conftab {
	u32 fixed_n;
	u32 tmds_clk;
	u32 fixed_cts;
};

struct vic_attrmap {
	enum hdmi_vic VIC;
	u32 tmds_clk;
};

enum hdmi_event_t {
	HDMI_TX_NONE = 0,
	HDMI_TX_HPD_PLUGIN = 1,
	HDMI_TX_HPD_PLUGOUT = 2,
	HDMI_TX_INTERNAL_INTR = 4,
};

struct hdmi_phy_t {
	unsigned long reg;
	unsigned long val_sleep;
	unsigned long val_save;
};

struct audcts_log {
	u32 val:20;
	u32 stable:1;
};

struct frac_rate_table {
	char *hz;
	u32 sync_num_int;
	u32 sync_den_int;
	u32 sync_num_dec;
	u32 sync_den_dec;
};

struct ced_cnt {
	bool ch0_valid;
	u16 ch0_cnt:15;
	bool ch1_valid;
	u16 ch1_cnt:15;
	bool ch2_valid;
	u16 ch2_cnt:15;
	u8 chksum;
};

struct scdc_locked_st {
	u8 clock_detected:1;
	u8 ch0_locked:1;
	u8 ch1_locked:1;
	u8 ch2_locked:1;
};

enum hdmi_hdr_transfer {
	T_UNKNOWN = 0,
	T_BT709,
	T_UNDEF,
	T_BT601,
	T_BT470M,
	T_BT470BG,
	T_SMPTE170M,
	T_SMPTE240M,
	T_LINEAR,
	T_LOG100,
	T_LOG316,
	T_IEC61966_2_4,
	T_BT1361E,
	T_IEC61966_2_1,
	T_BT2020_10,
	T_BT2020_12,
	T_SMPTE_ST_2084,
	T_SMPTE_ST_28,
	T_HLG,
};

enum hdmi_hdr_color {
	C_UNKNOWN = 0,
	C_BT709,
	C_UNDEF,
	C_BT601,
	C_BT470M,
	C_BT470BG,
	C_SMPTE170M,
	C_SMPTE240M,
	C_FILM,
	C_BT2020,
};

enum hdmitx_aspect_ratio {
	AR_UNKNOWM = 0,
	AR_4X3,
	AR_16X9,
};

struct aspect_ratio_list {
	enum hdmi_vic vic;
	int flag;
	char aspect_ratio_num;
	char aspect_ratio_den;
};

struct hdmitx_clk_tree_s {
	/* hdmitx clk tree */
	struct clk *hdmi_clk_vapb;
	struct clk *hdmi_clk_vpu;
	struct clk *venci_top_gate;
	struct clk *venci_0_gate;
	struct clk *venci_1_gate;
};

struct hdmitx_match_frame_table_s {
	int duration;
	int max_lncnt;
};

enum hdmi_ll_mode {
	HDMI_LL_MODE_AUTO = 0,
	HDMI_LL_MODE_DISABLE,
	HDMI_LL_MODE_ENABLE,
};

struct st_debug_param {
	unsigned int avmute_frame;
};

#define EDID_MAX_BLOCK              8
struct hdmitx_dev {
	struct cdev cdev; /* The cdev structure */
	dev_t hdmitx_id;
	struct proc_dir_entry *proc_file;
	struct task_struct *task_hdmist_check;
	struct task_struct *task_monitor;
	struct task_struct *task_hdcp;
	struct workqueue_struct *hdmi_wq;
	struct workqueue_struct *rxsense_wq;
	struct workqueue_struct *cedst_wq;
	struct device *hdtx_dev;
	struct device *pdev; /* for pinctrl*/
	struct hdmi_format_para *para;
	struct pinctrl_state *pinctrl_i2c;
	struct pinctrl_state *pinctrl_default;
	struct amhdmitx_data_s *data;
	struct notifier_block nb;
	struct delayed_work work_hpd_plugin;
	struct delayed_work work_hpd_plugout;
	struct delayed_work work_aud_hpd_plug;
	struct delayed_work work_rxsense;
	struct delayed_work work_internal_intr;
	struct delayed_work work_cedst;
	struct work_struct work_hdr;
	struct delayed_work work_start_hdcp;
	struct vrr_device_s hdmitx_vrr_dev;
	void *am_hdcp;
#ifdef CONFIG_AML_HDMI_TX_14
	struct delayed_work cec_work;
#endif
	int hdmi_init;
	int hpdmode;
	int ready;	/* 1, hdmi stable output, others are 0 */
	u32 div40;
	bool pre_tmds_clk_div40;
	u32 lstore;
	u32 hdcp_mode;
	struct {
		int (*setdispmode)(struct hdmitx_dev *hdev);
		int (*setaudmode)(struct hdmitx_dev *hdev, struct hdmitx_audpara *audio_param);
		void (*setupirq)(struct hdmitx_dev *hdev);
		void (*debugfun)(struct hdmitx_dev *hdev, const char *buf);
		void (*debug_bist)(struct hdmitx_dev *hdev, u32 num);
		void (*uninit)(struct hdmitx_dev *hdev);
		int (*cntlpower)(struct hdmitx_dev *hdev, u32 cmd, u32 arg);
		/* edid/hdcp control */
		int (*cntlddc)(struct hdmitx_dev *hdev, u32 cmd, unsigned long arg);
		/* Audio/Video/System Status */
		int (*getstate)(struct hdmitx_dev *hdev, u32 cmd, u32 arg);
		int (*cntlpacket)(struct hdmitx_dev *hdev, u32 cmd, u32 arg); /* Packet control */
		/* Configure control */
		int (*cntlconfig)(struct hdmitx_dev *hdev, u32 cmd, u32 arg);
		int (*cntlmisc)(struct hdmitx_dev *hdev, u32 cmd, u32 arg);
		int (*cntl)(struct hdmitx_dev *hdev, u32 cmd, u32 arg); /* Other control */
	} hwop;
	struct {
		u32 enable;
		union hdmi_infoframe vend;
		union hdmi_infoframe avi;
		union hdmi_infoframe spd;
		union hdmi_infoframe aud;
		union hdmi_infoframe drm;
		union hdmi_infoframe emp;
	} infoframes;
	struct hdmi_config_platform_data config_data;
	enum hdmi_event_t hdmitx_event;
	u32 irq_hpd;
	u32 irq_vrr_vsync;
	/*EDID*/
	u32 cur_edid_block;
	u32 cur_phy_block_ptr;
	u8 EDID_buf[EDID_MAX_BLOCK * 128];
	u8 EDID_buf1[EDID_MAX_BLOCK * 128]; /* for second read */
	u8 tmp_edid_buf[128 * EDID_MAX_BLOCK];
	u8 *edid_ptr;
	u32 edid_parsing; /* Indicator that RX edid data integrated */
	u8 EDID_hash[20];
	struct rx_cap rxcap;
	int vic_count;
	struct hdmitx_clk_tree_s hdmitx_clk_tree;
	/*audio*/
	struct hdmitx_audpara cur_audio_param;
	int audio_param_update_flag;
	u8 unplug_powerdown;
	unsigned short physical_addr;
	u32 cur_VIC;
	char fmt_attr[16];
	char backup_fmt_attr[16];
	atomic_t kref_video_mute;
	atomic_t kref_audio_mute;
	/**/
	u8 hpd_event; /* 1, plugin; 2, plugout */
	u8 hpd_state; /* 1, connect; 0, disconnect */
	u8 drm_mode_setting; /* 1, setting; 0, keeping */
	u8 rhpd_state; /* For repeater use only, no delay */
	u8 force_audio_flag;
	u8 mux_hpd_if_pin_high_flag;
	int aspect_ratio;	/* 1, 4:3; 2, 16:9 */
	enum frl_rate_enum manual_frl_rate; /* for manual setting */
	enum frl_rate_enum frl_rate; /* for mode setting */
	enum frl_rate_enum tx_max_frl_rate; /* configure in dts file */
	u8 dsc_en;
	struct hdmitx_info hdmi_info;
	u32 log;
	u32 tx_aud_cfg; /* 0, off; 1, on */
	u32 hpd_lock;
	/* 0: RGB444  1: Y444  2: Y422  3: Y420 */
	/* 4: 24bit  5: 30bit  6: 36bit  7: 48bit */
	/* if equals to 1, means current video & audio output are blank */
	u32 output_blank_flag;
	u32 audio_notify_flag;
	u32 audio_step;
	u32 repeater_tx;
	/* 0.1% clock shift, 1080p60hz->59.94hz */
	u32 frac_rate_policy;
	u32 backup_frac_rate_policy;
	u32 rxsense_policy;
	u32 cedst_policy;
	u32 enc_idx;
	u32 vrr_type; /* 1: GAME-VRR, 2: QMS-VRR */
	struct ced_cnt ced_cnt;
	struct scdc_locked_st chlocked_st;
	u32 allm_mode; /* allm_mode: 1/on 0/off */
	enum hdmi_ll_mode ll_user_set_mode; /* ll setting: 0/AUTOMATIC, 1/Always OFF, 2/ALWAYS ON */
	bool ll_enabled_in_auto_mode; /* ll_mode enabled in auto or not */
	u32 ct_mode; /* 0/off 1/game, 2/graphics, 3/photo, 4/cinema */
	bool it_content;
	u32 sspll;
	/* if HDMI plugin even once time, then set 1 */
	/* if never hdmi plugin, then keep as 0 */
	u32 already_used;
	/* configure for I2S: 8ch in, 2ch out */
	/* 0: default setting  1:ch0/1  2:ch2/3  3:ch4/5  4:ch6/7 */
	u32 aud_output_ch;
	u32 hdmi_ch;
	u32 tx_aud_src; /* 0: SPDIF  1: I2S */
/* if set to 1, then HDMI will output no audio */
/* In KTV case, HDMI output Picture only, and Audio is driven by other
 * sources.
 */
	u8 hdmi_audio_off_flag;
	enum hdmi_hdr_transfer hdr_transfer_feature;
	enum hdmi_hdr_color hdr_color_feature;
	/* 0: sdr 1:standard HDR 2:non standard 3:HLG*/
	u32 colormetry;
	u32 hdmi_last_hdr_mode;
	u32 hdmi_current_hdr_mode;
	u32 dv_src_feature;
	u32 sdr_hdr_feature;
	u32 hdr10plus_feature;
	enum eotf_type hdmi_current_eotf_type;
	enum mode_type hdmi_current_tunnel_mode;
	u32 flag_3dfp:1;
	u32 flag_3dtb:1;
	u32 flag_3dss:1;
	u32 dongle_mode:1;
	u32 pxp_mode:1;
	u32 cedst_en:1; /* configure in DTS */
	u32 hdr_8bit_en:1; /* hdr can output with 8bit */
	u32 aon_output:1; /* always output in bl30 */
	u32 hdr_priority;
	u32 bist_lock:1;
	u32 vend_id_hit:1;
	u32 fr_duration;
	spinlock_t edid_spinlock; /* edid hdr/dv cap lock */
	struct vpu_dev_s *hdmitx_vpu_clk_gate_dev;

	unsigned int hdcp_ctl_lvl;

	/*DRM related*/
	int drm_hdmitx_id;
	struct connector_hpd_cb drm_hpd_cb;
	struct connector_hdcp_cb drm_hdcp_cb;

	struct miscdevice hdcp_comm_device;
	u8 def_stream_type;
	u8 tv_usage;
	bool systemcontrol_on;
	bool suspend_flag;
	u32 arc_rx_en;
	bool need_filter_hdcp_off;
	u32 filter_hdcp_off_period;
	bool not_restart_hdcp;
	bool dw_hdcp22_cap;
	/* mutex for mode setting, note hdcp should also
	 * mutex with hdmi mode setting
	 */
	struct mutex hdmimode_mutex;
	unsigned long up_hdcp_timeout_sec;
	struct delayed_work work_up_hdcp_timeout;
	struct st_debug_param debug_param;
};

#define CMD_DDC_OFFSET          (0x10 << 24)
#define CMD_STATUS_OFFSET       (0x11 << 24)
#define CMD_PACKET_OFFSET       (0x12 << 24)
#define CMD_MISC_OFFSET         (0x13 << 24)
#define CMD_CONF_OFFSET         (0x14 << 24)
#define CMD_STAT_OFFSET         (0x15 << 24)

/***********************************************************************
 *             DDC CONTROL //cntlddc
 **********************************************************************/
#define DDC_RESET_EDID          (CMD_DDC_OFFSET + 0x00)
#define DDC_PIN_MUX_OP          (CMD_DDC_OFFSET + 0x08)
#define PIN_MUX             0x1
#define PIN_UNMUX           0x2
#define DDC_EDID_READ_DATA      (CMD_DDC_OFFSET + 0x0a)
#define DDC_IS_EDID_DATA_READY  (CMD_DDC_OFFSET + 0x0b)
#define DDC_EDID_GET_DATA       (CMD_DDC_OFFSET + 0x0c)
#define DDC_EDID_CLEAR_RAM      (CMD_DDC_OFFSET + 0x0d)
#define DDC_GLITCH_FILTER_RESET	(CMD_DDC_OFFSET + 0x11)
#define DDC_SCDC_DIV40_SCRAMB	(CMD_DDC_OFFSET + 0x20)
#define DDC_HDCP_SET_TOPO_INFO (CMD_DDC_OFFSET + 0x32)

/***********************************************************************
 *             CONFIG CONTROL //cntlconfig
 **********************************************************************/
/* Video part */
#define CONF_HDMI_DVI_MODE      (CMD_CONF_OFFSET + 0x02)
#define HDMI_MODE           0x1
#define DVI_MODE            0x2
/* set value as COLORSPACE_RGB444, YUV422, YUV444, YUV420 */
#define CONF_CT_MODE		(CMD_CONF_OFFSET + 0X2000 + 0x04)
	#define SET_CT_OFF		0
	#define SET_CT_GAME		1
	#define SET_CT_GRAPHICS	2
	#define SET_CT_PHOTO	3
	#define SET_CT_CINEMA	4
	#define IT_CONTENT	1
#define CONF_VIDEO_MUTE_OP      (CMD_CONF_OFFSET + 0x1000 + 0x04)
#define VIDEO_MUTE          0x1
#define VIDEO_UNMUTE        0x2
#define CONF_EMP_NUMBER         (CMD_CONF_OFFSET + 0x3000 + 0x00)
#define CONF_EMP_PHY_ADDR       (CMD_CONF_OFFSET + 0x3000 + 0x01)

/* Audio part */
#define CONF_CLR_AVI_PACKET     (CMD_CONF_OFFSET + 0x04)
#define CONF_CLR_VSDB_PACKET    (CMD_CONF_OFFSET + 0x05)
#define CONF_VIDEO_MAPPING	(CMD_CONF_OFFSET + 0x06)
#define CONF_GET_HDMI_DVI_MODE	(CMD_CONF_OFFSET + 0x07)
#define CONF_CLR_DV_VS10_SIG	(CMD_CONF_OFFSET + 0x10)

#define CONF_AUDIO_MUTE_OP      (CMD_CONF_OFFSET + 0x1000 + 0x00)
#define AUDIO_MUTE          0x1
#define AUDIO_UNMUTE        0x2
#define CONF_CLR_AUDINFO_PACKET (CMD_CONF_OFFSET + 0x1000 + 0x01)
#define CONF_ASPECT_RATIO	(CMD_CONF_OFFSET + 0x60c)

/***********************************************************************
 *             MISC control, hpd, hpll //cntlmisc
 **********************************************************************/
#define MISC_HPD_MUX_OP         (CMD_MISC_OFFSET + 0x00)
#define MISC_HPD_GPI_ST         (CMD_MISC_OFFSET + 0x02)
#define MISC_HPLL_OP            (CMD_MISC_OFFSET + 0x03)
#define		HPLL_ENABLE         0x1
#define		HPLL_DISABLE        0x2
#define		HPLL_SET	    0x3
#define MISC_TMDS_PHY_OP        (CMD_MISC_OFFSET + 0x04)
#define TMDS_PHY_ENABLE     0x1
#define TMDS_PHY_DISABLE    0x2
#define MISC_VIID_IS_USING      (CMD_MISC_OFFSET + 0x05)
#define MISC_CONF_MODE420       (CMD_MISC_OFFSET + 0x06)
#define MISC_TMDS_CLK_DIV40     (CMD_MISC_OFFSET + 0x07)
#define MISC_COMP_HPLL         (CMD_MISC_OFFSET + 0x08)
#define COMP_HPLL_SET_OPTIMIZE_HPLL1    0x1
#define COMP_HPLL_SET_OPTIMIZE_HPLL2    0x2
#define MISC_COMP_AUDIO         (CMD_MISC_OFFSET + 0x09)
#define COMP_AUDIO_SET_N_6144x2          0x1
#define COMP_AUDIO_SET_N_6144x3          0x2
#define MISC_AVMUTE_OP          (CMD_MISC_OFFSET + 0x0a)
#define MISC_FINE_TUNE_HPLL     (CMD_MISC_OFFSET + 0x0b)
	#define OFF_AVMUTE	0x0
	#define CLR_AVMUTE	0x1
	#define SET_AVMUTE	0x2
#define MISC_HPLL_FAKE			(CMD_MISC_OFFSET + 0x0c)
#define MISC_TMDS_RXSENSE	(CMD_MISC_OFFSET + 0x0f)
#define MISC_I2C_REACTIVE       (CMD_MISC_OFFSET + 0x10) /* For gxl */
#define MISC_I2C_RESET		(CMD_MISC_OFFSET + 0x11) /* For g12 */
#define MISC_READ_AVMUTE_OP     (CMD_MISC_OFFSET + 0x12)
#define MISC_TMDS_CEDST		(CMD_MISC_OFFSET + 0x13)
#define MISC_TRIGGER_HPD        (CMD_MISC_OFFSET + 0X14)
#define MISC_SUSFLAG		(CMD_MISC_OFFSET + 0X15)
#define MISC_GET_FRL_MODE	(CMD_MISC_OFFSET + 0X16)
#define MISC_CLK_DIV_RST	(CMD_MISC_OFFSET + 0X17)
#define MISC_HDMI_CLKS_CTRL     (CMD_MISC_OFFSET + 0X18)

/***********************************************************************
 *                          Get State //getstate
 **********************************************************************/
#define STAT_VIDEO_VIC          (CMD_STAT_OFFSET + 0x00)
#define STAT_VIDEO_CLK          (CMD_STAT_OFFSET + 0x01)
#define STAT_AUDIO_FORMAT       (CMD_STAT_OFFSET + 0x10)
#define STAT_AUDIO_CHANNEL      (CMD_STAT_OFFSET + 0x11)
#define STAT_AUDIO_CLK_STABLE   (CMD_STAT_OFFSET + 0x12)
#define STAT_AUDIO_PACK         (CMD_STAT_OFFSET + 0x13)
#define STAT_HDR_TYPE		(CMD_STAT_OFFSET + 0x20)

struct hdmitx_dev *get_hdmitx21_device(void);

/***********************************************************************
 *    hdmitx protocol level interface
 **********************************************************************/
enum hdmi_vic hdmitx21_edid_vic_tab_map_vic(const char *disp_mode);
int hdmitx21_edid_parse(struct hdmitx_dev *hdev);
int check21_dvi_hdmi_edid_valid(u8 *buf);
enum hdmi_vic hdmitx21_edid_get_VIC(struct hdmitx_dev *hdev,
				  const char *disp_mode,
				  char force_flag);

int hdmitx21_edid_dump(struct hdmitx_dev *hdev, char *buffer,
		     int buffer_len);
bool hdmitx21_edid_check_valid_mode(struct hdmitx_dev *hdev,
				  struct hdmi_format_para *para);
bool is_vic_support_y420(struct hdmitx_dev *hdev, enum hdmi_vic vic);
const char *hdmitx21_edid_vic_to_string(enum hdmi_vic vic);
void hdmitx21_edid_clear(struct hdmitx_dev *hdev);
void hdmitx21_edid_ram_buffer_clear(struct hdmitx_dev *hdev);
void hdmitx21_edid_buf_compare_print(struct hdmitx_dev *hdev);
int hdmitx21_read_phy_status(void);
void hdmitx21_dither_config(struct hdmitx_dev *hdev);
u32 hdmitx21_check_edid_all_zeros(u8 *buf);
bool hdmitx21_edid_notify_ng(u8 *buf);

/* VSIF: Vendor Specific InfoFrame
 * It has multiple purposes:
 * 1. HDMI1.4 4K, HDMI_VIC=1/2/3/4, 2160p30/25/24hz, smpte24hz, AVI.VIC=0
 *    In CTA-861-G, matched with AVI.VIC=95/94/93/98
 * 2. 3D application, TB/SS/FP
 * 3. DolbyVision, with Len=0x18
 * 4. HDR10plus
 * 5. HDMI20 3D OSD disparity / 3D dual-view / 3D independent view / ALLM
 * Some functions are exclusive, but some may compound.
 * Consider various state transitions carefully, such as play 3D under HDMI14
 * 4K, exit 3D under 4K, play DV under 4K, enable ALLM under 3D dual-view
 */
enum vsif_type {
	/* Below 4 functions are exclusive */
	VT_HDMI14_4K = 1,
	VT_T3D_VIDEO,
	VT_DOLBYVISION,
	VT_HDR10PLUS,
	/* Maybe compound 3D dualview + ALLM */
	VT_T3D_OSD_DISPARITY = 0x10,
	VT_T3D_DUALVIEW,
	VT_T3D_INDEPENDVEW,
	VT_ALLM,
	/* default: if non-HDMI4K, no any vsif; if HDMI4k, = VT_HDMI14_4K */
	VT_DEFAULT,
	VT_MAX,
};

int hdmitx21_construct_vsif(struct hdmitx_dev *hdev, enum vsif_type type, int on, void *param);

/* if vic is 93 ~ 95, or 98 (HDMI14 4K), return 1 */
bool _is_hdmi14_4k(enum hdmi_vic vic);
/* if vic is 96, 97, 101, 102, 106, 107, 4k 50/60hz, return 1 */
bool _is_y420_vic(enum hdmi_vic vic);

/* set vic to AVI.VIC */
void hdmitx21_set_avi_vic(enum hdmi_vic vic);

/*
 * HDMI Repeater TX I/F
 * RX downstream Information from rptx to rprx
 */
/* send part raw edid from TX to RX */
void rx_repeat_hpd_state(bool st);
/* prevent compile error in no HDMIRX case */
void __attribute__((weak))rx_repeat_hpd_state(bool st)
{
}

void hdmi21_vframe_write_reg(u32 value);
void rx_edid_physical_addr(u8 a, u8 b,
			   u8 c, u8 d);
void __attribute__((weak))rx_edid_physical_addr(u8 a,
						u8 b,
						u8 c,
						u8 d)
{
}

int rx_set_hdr_lumi(u8 *data, int len);
int __attribute__((weak))rx_set_hdr_lumi(u8 *data, int len)
{
	return 0;
}

void rx_set_repeater_support(bool enable);
void __attribute__((weak))rx_set_repeater_support(bool enable)
{
}

void rx_set_receiver_edid(u8 *data, int len);
void __attribute__((weak))rx_set_receiver_edid(u8 *data, int len)
{
}

void rx_set_receive_hdcp(u8 *data, int len, int depth,
			 bool max_cascade, bool max_devs);
void __attribute__((weak))rx_set_receive_hdcp(u8 *data, int len,
					      int depth, bool max_cascade,
					      bool max_devs)
{
}

int hdmitx21_set_display(struct hdmitx_dev *hdev,
		       enum hdmi_vic videocode);

int hdmi21_set_3d(struct hdmitx_dev *hdev, int type,
		u32 param);

int hdmitx21_set_audio(struct hdmitx_dev *hdev,
		     struct hdmitx_audpara *audio_param);

/* for notify to cec */
#define HDMITX_PLUG			1
#define HDMITX_UNPLUG			2
#define HDMITX_PHY_ADDR_VALID		3
#define HDMITX_KSVLIST	4
/* #define HDMITX_HDR_PRIORITY		5 */

#define HDMI_SUSPEND    0
#define HDMI_WAKEUP     1

enum hdmitx_event {
	HDMITX_NONE_EVENT = 0,
	HDMITX_HPD_EVENT,
	HDMITX_HDCP_EVENT,
	HDMITX_AUDIO_EVENT,
	HDMITX_HDCPPWR_EVENT,
	HDMITX_HDR_EVENT,
	HDMITX_RXSENSE_EVENT,
	HDMITX_CEDST_EVENT,
};

#define MAX_UEVENT_LEN 64
struct hdmitx_uevent {
	const enum hdmitx_event type;
	int state;
	const char *env;
};

int hdmitx21_set_uevent(enum hdmitx_event type, int val);
int hdmitx21_set_uevent_state(enum hdmitx_event type, int state);

void hdmi_set_audio_para(int para);
int get21_cur_vout_index(void);
void phy_hpll_off(void);
int get21_hpd_state(void);
void hdmitx21_event_notify(unsigned long state, void *arg);
void hdmitx21_hdcp_status(int hdmi_authenticated);

/***********************************************************************
 *    hdmitx hardware level interface
 ***********************************************************************/
void hdmitx21_meson_init(struct hdmitx_dev *hdev);

/*
 * HDMITX HPD HW related operations
 */
enum hpd_op {
	HPD_INIT_DISABLE_PULLUP,
	HPD_INIT_SET_FILTER,
	HPD_IS_HPD_MUXED,
	HPD_MUX_HPD,
	HPD_UNMUX_HPD,
	HPD_READ_HPD_GPIO,
};

int hdmitx21_hpd_hw_op(enum hpd_op cmd);
/*
 * HDMITX DDC HW related operations
 */
enum ddc_op {
	DDC_INIT_DISABLE_PULL_UP_DN,
	DDC_MUX_DDC,
	DDC_UNMUX_DDC,
};

int hdmitx21_ddc_hw_op(enum ddc_op cmd);

#define HDMITX_HWCMD_MUX_HPD_IF_PIN_HIGH       0x3
#define HDMITX_HWCMD_TURNOFF_HDMIHW           0x4
#define HDMITX_HWCMD_MUX_HPD                0x5
#define HDMITX_HWCMD_PLL_MODE                0x6
#define HDMITX_HWCMD_TURN_ON_PRBS           0x7
#define HDMITX_FORCE_480P_CLK                0x8
#define HDMITX_GET_AUTHENTICATE_STATE        0xa
#define HDMITX_SW_INTERNAL_HPD_TRIG          0xb
#define HDMITX_HWCMD_OSD_ENABLE              0xf

#define HDMITX_IP_INTR_MASN_RST              0x12
#define HDMITX_EARLY_SUSPEND_RESUME_CNTL     0x14
#define HDMITX_EARLY_SUSPEND             0x1
#define HDMITX_LATE_RESUME               0x2
/* Refer to HDMI_OTHER_CTRL0 in hdmi_tx_reg.h */
#define HDMITX_IP_SW_RST                     0x15
#define TX_CREG_SW_RST      BIT(5)
#define TX_SYS_SW_RST       BIT(4)
#define CEC_CREG_SW_RST     BIT(3)
#define CEC_SYS_SW_RST      BIT(2)
#define HDMITX_AVMUTE_CNTL                   0x19
#define AVMUTE_SET          0   /* set AVMUTE to 1 */
#define AVMUTE_CLEAR        1   /* set AVunMUTE to 1 */
#define AVMUTE_OFF          2   /* set both AVMUTE and AVunMUTE to 0 */
#define HDMITX_CBUS_RST                      0x1A
#define HDMITX_INTR_MASKN_CNTL               0x1B
#define INTR_MASKN_ENABLE   0
#define INTR_MASKN_DISABLE  1
#define INTR_CLEAR          2

void hdmi_tx_edid_proc(u8 *edid);

void vsem_init_cfg(struct hdmitx_dev *hdev);

enum hdmi_tf_type hdmitx21_get_cur_hdr_st(void);
enum hdmi_tf_type hdmitx21_get_cur_dv_st(void);
enum hdmi_tf_type hdmitx21_get_cur_hdr10p_st(void);
unsigned int hdmitx21_get_vender_infoframe_ieee(void);
bool hdmitx21_hdr_en(void);
bool hdmitx21_dv_en(void);
bool hdmitx21_hdr10p_en(void);
u32 aud_sr_idx_to_val(enum hdmi_audio_fs e_sr_idx);
bool hdmitx21_uboot_already_display(struct hdmitx_dev *hdev);
#endif
