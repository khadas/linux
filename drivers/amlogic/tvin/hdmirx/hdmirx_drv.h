/*
 * hdmirx_drv.h for HDMI device driver, and declare IO function,
 * structure, enum, used in TVIN AFE sub-module processing
 *
 * Copyright (C) 2014 AMLOGIC, INC. All Rights Reserved.
 * Author: Rain Zhang <rain.zhang@amlogic.com>
 * Author: Xiaofei Zhu <xiaofei.zhu@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#ifndef _TVHDMI_H
#define _TVHDMI_H

#include <linux/switch.h>
#include <linux/workqueue.h>

#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/tvin/tvin.h>
#include <linux/amlogic/iomap.h>
#include <linux/cdev.h>
#include <linux/irqreturn.h>

#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "../tvin_frontend.h"


#define RX_VER0 "Ref.2016/11/25"
/*------------------------------*/
#define RX_VER1 "Ref.2017/03/15"
/*------------------------------*/
#define RX_VER2 "Ref.2017/2/25"
/*------------------------------*/
#define RX_VER3 "Ref.2017/02/27"
/*------------------------------*/
#define HDMI_STATE_CHECK_FREQ     (20*5)
#define ABS(x) ((x) < 0 ? -(x) : (x))

#define	LOG_EN		0x01
#define VIDEO_LOG	0x02
#define AUDIO_LOG	0x04
#define HDCP_LOG	0x08
#define PACKET_LOG	0x10
#define EQ_LOG		0x20
#define REG_LOG		0x40
#define ERR_LOG		0x80

#define TRUE 1
#define FALSE 0
#define CFG_CLK 24000
#define MODET_CLK 24000

#define HDCP_AUTH_COME_DELAY 150
#define HDCP_AUTH_END_DELAY (HDCP_AUTH_COME_DELAY + 150)
#define HDCP_AUTH_COUNT_MAX 5
#define HDCP_AUTH_HOLD_TIME 500

struct hdmirx_dev_s {
	int                         index;
	dev_t                       devt;
	struct cdev                 cdev;
	struct device               *dev;
	struct tvin_parm_s          param;
	struct timer_list           timer;
	struct tvin_frontend_s		frontend;
	unsigned int			irq;
	char					irq_name[12];
	struct clk *modet_clk;
	struct clk *cfg_clk;
	struct clk *acr_ref_clk;
	struct clk *audmeas_clk;
};

#define HDMI_IOC_MAGIC 'H'
#define HDMI_IOC_HDCP_ON	_IO(HDMI_IOC_MAGIC, 0x01)
#define HDMI_IOC_HDCP_OFF	_IO(HDMI_IOC_MAGIC, 0x02)
#define HDMI_IOC_EDID_UPDATE	_IO(HDMI_IOC_MAGIC, 0x03)
#define HDMI_IOC_PC_MODE_ON		_IO(HDMI_IOC_MAGIC, 0x04)
#define HDMI_IOC_PC_MODE_OFF	_IO(HDMI_IOC_MAGIC, 0x05)
#define HDMI_IOC_HDCP22_AUTO	_IO(HDMI_IOC_MAGIC, 0x06)
#define HDMI_IOC_HDCP22_FORCE14 _IO(HDMI_IOC_MAGIC, 0x07)
#define HDMI_IOC_HDMI_20_SET	_IO(HDMI_IOC_MAGIC, 0x08)

#define HDMI_IOC_HDCP_GET_KSV _IOR(HDMI_IOC_MAGIC, 0x09, struct _hdcp_ksv)

#define HDCP22_ENABLE
#define HDMI20_ENABLE 1

/* add new value at the end,
 * do not insert new value in the middle
 * to avoid wrong VIC value !!!
 */
enum HDMI_Video_Type {

	HDMI_UNKNOW = 0 ,
	HDMI_640x480p60 = 1 ,
	HDMI_480p60,
	HDMI_480p60_16x9,
	HDMI_720p60,
	HDMI_1080i60,               /* 5 */

	HDMI_480i60,
	HDMI_480i60_16x9,
	HDMI_1440x240p60,
	HDMI_1440x240p60_16x9,
	HDMI_2880x480i60,           /* 10 */

	HDMI_2880x480i60_16x9,
	HDMI_2880x240p60,
	HDMI_2880x240p60_16x9,
	HDMI_1440x480p60,
	HDMI_1440x480p60_16x9,      /* 15 */

	HDMI_1080p60,
	HDMI_576p50,
	HDMI_576p50_16x9,
	HDMI_720p50,
	HDMI_1080i50,               /* 20 */

	HDMI_576i50,
	HDMI_576i50_16x9,
	HDMI_1440x288p50,
	HDMI_1440x288p50_16x9,
	HDMI_2880x576i50,           /* 25 */

	HDMI_2880x576i50_16x9,
	HDMI_2880x288p50,
	HDMI_2880x288p50_16x9,
	HDMI_1440x576p50,
	HDMI_1440x576p50_16x9,      /* 30 */

	HDMI_1080p50,
	HDMI_1080p24,
	HDMI_1080p25,
	HDMI_1080p30,
	HDMI_2880x480p60,           /* 35 */

	HDMI_2880x480p60_16x9,
	HDMI_2880x576p50,
	HDMI_2880x576p50_16x9,
	HDMI_1080i50_1250,          /* 39 */

	HDMI_1080I120 = 46,
	HDMI_720p120  = 47,

	HDMI_720p24   = 60,
	HDMI_720p25   = 61,
	HDMI_720p30   = 62,
	HDMI_1080p120 = 63,
	HDMI_800_600  = 65,

	HDMI_1024_768,              /* 66 */
	HDMI_720_400,
	HDMI_1280_768,
	HDMI_1280_800,
	HDMI_1280_960,              /* 70 */

	HDMI_1280_1024,
	HDMI_1360_768,
	HDMI_1366_768,
	HDMI_1600_900,
	HDMI_1600_1200,             /* 75 */

	HDMI_1920_1200,
	HDMI_1440_900,
	HDMI_1400_1050,
	HDMI_1680_1050,             /* 79 */

	HDMI_3840_2160p,
	HDMI_4096_2160p,			/* 81 */

	HDMI_640x480p72,
	HDMI_640x480p75,
	HDMI_2160p_50hz_420 = 96,
	HDMI_2160p_60hz_420 = 97,
	HDMI_4096p_50hz_420 = 101,
	HDMI_4096p_60hz_420 = 102,

	HDMI_2560_1440,
	HDMI_UNSUPPORT,
};

enum fsm_states_e {
	FSM_INIT,
	FSM_HPD_LOW,
	FSM_HPD_HIGH,
	FSM_EQ_INIT,
	FSM_EQ_CALIBRATION,
	FSM_EQ_END,
	FSM_PHY_RST,
	FSM_WAIT_CLK_STABLE,
	FSM_WAIT_HDCP_SWITCH,
	FSM_SIG_UNSTABLE,
	FSM_DWC_RST_WAIT,
	FSM_SIG_STABLE,
	FSM_CHECK_DDC_CORRECT,
	FSM_SIG_READY,
	FSM_WAIT_AUDIO_STABLE,
	FSM_PHY_RESET,
	FSM_DWC_RESET,
};

enum colorspace_e {
	E_COLOR_RGB,
	E_COLOR_YUV422,
	E_COLOR_YUV444,
	E_COLOR_YUV420,
};

enum port_sts {
	E_PORT0,
	E_PORT1,
	E_PORT2,
	E_5V_LOST = 0xfd,
	E_HPD_RESET = 0xfe,
	E_INIT = 0xff,
};

enum hdcp_type {
	E_HDCP14,
	E_HDCP22
};

enum eq_sts {
	E_EQ_NONE,
	E_EQ_LOW_FREQ,
	E_EQ_HD_FREQ,
	E_EQ_3G,
	E_EQ_6G
};

enum repeater_state_e {
	REPEATER_STATE_WAIT_KSV,
	REPEATER_STATE_WAIT_ACK,
	REPEATER_STATE_IDLE,
	REPEATER_STATE_START,
};

enum hdcp_version_e {
	HDCP_VERSION_NONE,
	HDCP_VERSION_14,
	HDCP_VERSION_22,
};

enum hdmirx_port_e {
	HDMIRX_PORT0,
	HDMIRX_PORT1,
	HDMIRX_PORT2,
	HDMIRX_PORT3,
};

enum hdcp22_auth_state_e {
	HDCP22_AUTH_STATE_NOT_CAPBLE,
	HDCP22_AUTH_STATE_CAPBLE,
	HDCP22_AUTH_STATE_LOST,
	HDCP22_AUTH_STATE_SUCCESS,
	HDCP22_AUTH_STATE_FAILED,
	HDCP22_AUTH_STATE_LOST_RST
};

/** Configuration clock minimum [kHz] */
#define CFG_CLK_MIN				(10000UL)
/** Configuration clock maximum [kHz] */
#define CFG_CLK_MAX				(160000UL)

/** TMDS clock minimum [kHz] */
#define TMDS_CLK_MIN			(24000UL)/* (25000UL) */
#define TMDS_CLK_MAX			(340000UL)/* (600000UL) */
#define CLK_RATE_THRESHOLD		(74000000)/*check clock rate*/
struct freq_ref_s {
	unsigned int vic;
	bool interlace;
	unsigned int ref_freq;	/* 8 bit tmds clock */
	unsigned hactive;
	unsigned vactive_lines;
	unsigned vactive_fp;
	unsigned vactive_alt;
	unsigned repeat;
	unsigned frame_rate;
};

struct hdmi_rx_phy {
	/** (@b user) Context status: closed (0), */
	/** opened (<0) and configured (>0) */
	int status;
	/** (@b user) Configuration clock frequency [kHz], */
	/** valid range 10MHz to 160MHz */
	unsigned long cfg_clk;
	/** Peaking configuration */
	uint16_t peaking;
	/** PLL configuration */
	uint32_t pll_cfg;
	/**/
	int lock_thres;
	int fast_switching;
	int fsm_enhancement;
	int port_select_ovr_en;
	int phy_cmu_config_force_val;
	int phy_system_config_force_val;
};

/**
 * @short HDMI RX controller video parameters
 *
 * For Auxiliary Video InfoFrame (AVI) details see HDMI 1.4a section 8.2.2
 */
struct hdmi_rx_ctrl_video {
	/** DVI detection status: DVI (true) or HDMI (false) */
	bool hw_dvi;
	unsigned hdcp_type;
	unsigned hdcp_enc_state;
	/** Deep color mode: 24, 30, 36 or 48 [bits per pixel] */
	unsigned colordepth;
	/** Pixel clock frequency [kHz] */
	unsigned long pixel_clk;
	/** Refresh rate [0.01Hz] */
	unsigned refresh_rate;
	/** Interlaced */
	bool interlaced;
	/** Vertical offset */
	unsigned voffset;
	/** Vertical active */
	unsigned vactive;
	/** Vertical total */
	unsigned vtotal;
	/** Horizontal offset */
	unsigned hoffset;
	/** Horizontal active */
	unsigned hactive;
	/** Horizontal total */
	unsigned htotal;
	/** AVI Y1-0, video format */
	unsigned colorspace;
	/** AVI A0, active format information present */
	unsigned active_valid;
	/** AVI B1-0, bar valid information */
	unsigned bar_valid;
	/** AVI S1-0, scan information */
	unsigned scan_info;
	/** AVI C1-0, colorimetry information */
	unsigned colorimetry;
	/** AVI M1-0, picture aspect ratio */
	unsigned picture_ratio;
	/** AVI R3-0, active format aspect ratio */
	unsigned active_ratio;
	/** AVI ITC, IT content */
	unsigned it_content;
	/** AVI EC2-0, extended colorimetry */
	unsigned ext_colorimetry;
	/** AVI Q1-0, RGB quantization range */
	unsigned rgb_quant_range;
	/** AVI Q1-0, YUV quantization range */
	unsigned yuv_quant_range;
	/** AVI SC1-0, non-uniform scaling information */
	unsigned n_uniform_scale;
	/** AVI VIC6-0, video mode identification code */
	unsigned hw_vic;
	/** AVI PR3-0, pixel repetition factor */
	unsigned repeat;
	/** AVI, line number of end of top bar */
	unsigned bar_end_top;
	/** AVI, line number of start of bottom bar */
	unsigned bar_start_bottom;
	/** AVI, pixel number of end of left bar */
	unsigned bar_end_left;
	/** AVI, pixel number of start of right bar */
	unsigned bar_start_right;

	/* for sw info */
	unsigned int sw_vic;
	unsigned int sw_dvi;
	bool           sw_fp;
	bool           sw_alternative;
};

/**
 * @short HDMI RX controller context information
 *
 * Initialize @b user fields (set status to zero).
 * After opening this data is for internal use only.
 */
struct hdmi_rx_ctrl {
	/** (@b user) Context status: closed (0), */
	/** opened (<0) and configured (>0) */
	int status;
	/** (@b user) Configuration clock frequency [kHz], */
	/** valid range 10MHz to 160MHz */
	unsigned long cfg_clk;
	/** Mode detection clock frequency [kHz], */
	/** valid range 10MHz to 50MHz */
	unsigned long md_clk;
	/** TDMS clock frequency [kHz] */
	unsigned long tmds_clk;
	/** Debug status, audio FIFO reset count */
	int acr_mode;
	/**/
	unsigned debug_audio_fifo_rst;
	/** Debug status, packet FIFO reset count */
	unsigned debug_packet_fifo_rst;
	/** Debug status, IRQ handling count */
	unsigned debug_irq_handling;
	/** Debug status, IRQ packet decoder count */
	unsigned debug_irq_packet_decoder;
	/** Debug status, IRQ audio clock count */
	unsigned debug_irq_audio_clock;
	/** Debug status, IRQ audio FIFO count */
	unsigned debug_irq_audio_fifo;
	/** Debug status, IRQ video mode count */
	unsigned debug_irq_video_mode;
	/** Debug status, IRQ HDMI count */
	unsigned debug_irq_hdmi;
};

/** Receiver key selection size - 40 bits */
#define HDCP_BKSV_SIZE	(2 *  1)
/** Encrypted keys size - 40 bits x 40 keys */
#define HDCP_KEYS_SIZE	(2 * 40)

/**
 * @short HDMI RX controller HDCP configuration
 */
struct hdmi_rx_ctrl_hdcp {
	/*hdcp auth state*/
	enum repeater_state_e state;
	/** Repeater mode else receiver only */
	bool repeat;
	bool cascade_exceed;
	bool dev_exceed;
	/*downstream depth*/
	unsigned char depth;
	/*downstream count*/
	uint32_t count;
	/** Key description seed */
	uint32_t seed;
	uint32_t delay;/*according to the timer,5s*/
	/**
	 * Receiver key selection
	 * @note 0: high order, 1: low order
	 */
	uint32_t bksv[HDCP_BKSV_SIZE];
	/**
	 * Encrypted keys
	 * @note 0: high order, 1: low order
	 */
	uint32_t keys[HDCP_KEYS_SIZE];
	struct switch_dev switch_hdcp_auth;
	enum hdcp_version_e hdcp_version;/* 0 no hdcp;1 hdcp14;2 hdcp22 */
	bool onoff;
	bool pass;
	uint32_t hdcp_auth_count;/*hdcp auth times*/
	uint32_t hdcp_auth_time;/*the time to clear auth count*/
};

#define CHANNEL_STATUS_SIZE   24

struct aud_info_s {
    /* info frame*/
    /*
    unsigned char cc;
    unsigned char ct;
    unsigned char ss;
    unsigned char sf;
    */
	int coding_type;
	int channel_count;
	int sample_frequency;
	int sample_size;
	int coding_extension;
	int channel_allocation;
	int down_mix_inhibit;
	int level_shift_value;
	int aud_packet_received;

	/* channel status */
	unsigned char channel_status[CHANNEL_STATUS_SIZE];
	unsigned char channel_status_bak[CHANNEL_STATUS_SIZE];
    /**/
	unsigned int cts;
	unsigned int n;
	unsigned int arc;
	/**/
	int real_channel_num;
	int real_sample_size;
	int real_sample_rate;
};

struct vendor_specific_info_s {
	unsigned identifier;
	unsigned char vd_fmt;
	unsigned char _3d_structure;
	unsigned char _3d_ext_data;
};

struct rx_s {
	/** HDMI RX received signal changed */
	uint change;
	/** HDMI RX input port 0 (A) or 1 (B) (or 2(C) or 3 (D)) */
	unsigned port;
	/*first boot flag*/
	bool boot_flag;
	/** HDMI RX PHY context */
	struct hdmi_rx_phy phy;
	/** HDMI RX controller context */
	struct hdmi_rx_ctrl ctrl;
	/** HDMI RX controller HDCP configuration */
	struct hdmi_rx_ctrl_hdcp hdcp;
	/*report hpd status to app*/
	struct switch_dev hpd_sdev;

	/* wrapper */
	unsigned int state;
	unsigned int pre_state;

	unsigned char pre_5v_sts;
	unsigned char cur_5v_sts;
	bool no_signal;
	int audio_wait_time;
	int aud_sr_stable_cnt;
	int aud_sr_unstable_cnt;
	int video_wait_time;
	/* info */
	struct aud_info_s aud_info;
	struct hdmi_rx_ctrl_video pre;
	struct hdmi_rx_ctrl_video cur;
	struct vendor_specific_info_s vendor_specific_info;
	struct tvin_hdr_info_s hdr_info;
	bool open_fg;
	unsigned char scdc_tmds_cfg;
	unsigned int pwr_sts;
};

struct _hdcp_ksv {
	uint32_t bksv0;
	uint32_t bksv1;
};

enum edid_audio_format_e {
	AUDIO_FORMAT_HEADER,
	AUDIO_FORMAT_LPCM,
	AUDIO_FORMAT_AC3,
	AUDIO_FORMAT_MPEG1,
	AUDIO_FORMAT_MP3,
	AUDIO_FORMAT_MPEG2,
	AUDIO_FORMAT_AAC,
	AUDIO_FORMAT_DTS,
	AUDIO_FORMAT_ATRAC,
	AUDIO_FORMAT_OBA,
	AUDIO_FORMAT_DDP,
	AUDIO_FORMAT_DTSHD,
	AUDIO_FORMAT_MAT,
	AUDIO_FORMAT_DST,
	AUDIO_FORMAT_WMAPRO,
};

enum edid_tag_e {
	EDID_TAG_NONE,
	EDID_TAG_AUDIO = 1,
	EDID_TAG_VIDEO = 2,
	EDID_TAG_VENDOR = 3,
	EDID_TAG_HDR = ((0x7<<8)|6),
};

struct edid_audio_block_t {
	unsigned char max_channel:3;
	unsigned char format_code:5;
	unsigned char freq_32khz:1;
	unsigned char freq_441khz:1;
	unsigned char freq_48khz:1;
	unsigned char freq_882khz:1;
	unsigned char freq_96khz:1;
	unsigned char freq_1764khz:1;
	unsigned char freq_192khz:1;
	unsigned char freq_reserv:1;
	union bit_rate_u {
		struct pcm_t {
			unsigned char rate_16bit:1;
			unsigned char rate_20bit:1;
			unsigned char rate_24bit:1;
			unsigned char rate_reserv:5;
		} pcm;
		unsigned char others;
	} bit_rate;
};

struct edid_hdr_block_t {
	unsigned char ext_tag_code;
	unsigned char sdr:1;
	unsigned char hdr:1;
	unsigned char smtpe_2048:1;
	unsigned char future:5;
	unsigned char meta_des_type1:1;
	unsigned char reserv:7;
	unsigned char max_lumi;
	unsigned char avg_lumi;
	unsigned char min_lumi;
};

enum edid_list_e {
	EDID_LIST_BUFF,
	EDID_LIST_AML,
	EDID_LIST_AML_DD,
	EDID_LIST_SKYWORTH,
	EDID_LIST_4K,
	EDID_LIST_DOMY,
	EDID_LIST_V2,
	EDID_LIST_ATMOS,
	EDID_LIST_TRUEHD,
	EDID_LIST_NUM
};

enum vsi_vid_format_e {
	VSI_FORMAT_NO_DATA,
	VSI_FORMAT_EXT_RESOLUTION,
	VSI_FORMAT_3D_FORMAT,
	VSI_FORMAT_FUTURE,
};

struct vsi_infoframe_t {
	unsigned int checksum:8;
	unsigned int ieee_id:24;
	unsigned char reserv:5;
	enum vsi_vid_format_e vid_format:3;
	union detail_u {
		unsigned char hdmi_vic:8;
		struct struct_3d_t {
			unsigned char reserv1:4;
			unsigned char struct_3d:4;

		} data_3d;
	} detail;
	unsigned char reserv2:4;
	unsigned char struct_3d_ext:4;
};

struct st_eq_data {
	/* Best long cable setting */
	uint16_t bestLongSetting;
	/* long cable setting detected and valid */
	uint8_t validLongSetting;
	/* best short cable setting */
	uint16_t bestShortSetting;
	/* best short cable setting detected and valid */
	uint8_t validShortSetting;
	/* TMDS Valid for channel */
	uint8_t tmdsvalid;
	/* best setting to be programed */
	uint16_t bestsetting;
	/* Accumulator register */
	uint16_t acc;
	/* Aquisition register */
	uint16_t acq;
	uint16_t lastacq;
};

extern struct delayed_work     eq_dwork;
extern struct workqueue_struct *eq_wq;
extern struct delayed_work		esm_dwork;
extern struct workqueue_struct	*esm_wq;
extern unsigned int pwr_sts;
extern unsigned char *pEdid_buffer;
extern bool multi_port_edid_enable;
extern int md_ists_en;
extern int hdmi_ists_en;
extern int real_port_map;
extern bool hpd_to_esm;
extern bool video_stable_to_esm;
extern bool hdcp_enable;
extern int it_content;
extern struct rx_s rx;
extern int log_level;
extern bool downstream_repeat_support;
extern int esm_data_base_addr;

extern int suspend_pddq;
extern unsigned int hdmirx_addr_port;
extern unsigned int hdmirx_data_port;
extern unsigned int hdmirx_ctrl_port;
extern unsigned char is_alternative(void);
extern unsigned char is_frame_packing(void);
extern void clk_init(void);
extern void clk_off(void);
extern void set_scdc_cfg(int hpdlow, int pwrprovided);
extern bool irq_ctrl_reg_en; /* enable/disable reg rd/wr in irq  */

extern int rgb_quant_range;
extern int yuv_quant_range;
extern int hdcp22_on;
extern int do_esm_rst_flag;
extern bool hdcp22_firmware_ok_flag;
extern int force_hdcp14_en;
extern int pre_port;
extern struct device *hdmirx_dev;

extern int esm_err_force_14;
extern int pc_mode_en;
extern int do_hpd_reset_flag;
extern bool hdcp22_kill_esm;
extern bool mute_kill_en;
extern char pre_eq_freq;

unsigned int rd_reg(unsigned int addr);
void wr_reg(unsigned int addr, unsigned int val);

void hdmirx_wr_top(unsigned long addr, unsigned long data);
void hdmirx_wr_ctl_port(unsigned int offset, unsigned long data);

unsigned long hdmirx_rd_top(unsigned long addr);
void hdmirx_wr_dwc(uint16_t addr, uint32_t data);
uint32_t hdmirx_rd_dwc(uint16_t addr);
int hdmirx_wr_phy(uint8_t reg_address, uint16_t data);
uint16_t hdmirx_rd_phy(uint8_t reg_address);
uint32_t hdmirx_rd_bits_dwc(uint16_t addr, uint32_t mask);
void hdmirx_wr_bits_dwc(uint16_t addr, uint32_t mask, uint32_t value);

#ifdef HDCP22_ENABLE
void rx_hdcp22_wr_reg(uint32_t addr, uint32_t data);
uint32_t rx_hdcp22_rd_reg(uint32_t addr);
void hdcp22_wr_top(uint32_t addr, uint32_t data);
uint32_t hdcp22_rd_top(uint32_t addr);
uint32_t rx_hdcp22_rd(uint32_t addr);

void rx_sec_reg_write(unsigned *addr, unsigned value);
unsigned rx_sec_reg_read(unsigned *addr);
unsigned sec_top_read(unsigned *addr);
void sec_top_write(unsigned *addr, unsigned value);
void hdmirx_hdcp22_esm_rst(void);
unsigned rx_sec_set_duk(void);
void hdmirx_hdcp22_init(void);
#endif

uint32_t get(uint32_t data, uint32_t mask);
uint32_t set(uint32_t data, uint32_t mask, uint32_t value);
int rx_set_receiver_edid(unsigned char *data, int len);
int rx_set_hdr_lumi(unsigned char *data, int len);
void rx_set_repeater_support(bool enable);
bool rx_set_receive_hdcp(unsigned char *data, int len, int depth, bool , bool);
void rx_repeat_hpd_state(bool plug);
void rx_repeat_hdcp_ver(int version);
void rx_edid_physical_addr(int a, int b, int c, int d);
void rx_check_repeat(void);
int hdmirx_control_clk_range(unsigned long min, unsigned long max);
int hdmirx_packet_fifo_rst(void);
void hdmirx_audio_enable(bool en);
int hdmirx_audio_fifo_rst(void);
void hdmirx_phy_init(int rx_port_sel, int dcm);

void hdmirx_hw_config(void);
void hdmirx_hw_reset(void);
void hdmirx_hw_probe(void);
void hdmi_rx_load_edid_data(unsigned char *buffer, int port);
int hdmi_rx_ctrl_edid_update(void);
void hdmirx_set_hpd(int port, unsigned char val);
bool hdmirx_repeat_support(void);

int hdmirx_irq_close(void);
int hdmirx_irq_open(void);

void hdmirx_phy_pddq(int enable);
void hdmirx_get_video_info(void);
void hdmirx_set_video_mute(bool mute);
void hdmirx_config_video(void);
unsigned int hdmirx_get_tmds_clock(void);
unsigned int hdmirx_get_pixel_clock(void);
unsigned int hdmirx_get_audio_clock(void);
unsigned int hdmirx_get_audio_pll_clock(void);
unsigned int hdmirx_get_esm_clock(void);

void hdmirx_read_audio_info(struct aud_info_s *audio_info);
void hdmirx_read_vendor_specific_info_frame(struct vendor_specific_info_s *vs);
unsigned int hdmirx_get_clock(int index);
irqreturn_t irq_handler(int irq, void *params);

/**
 * all functions declare
 */
extern enum tvin_sig_fmt_e hdmirx_hw_get_fmt(void);
extern void edid_update(void);
extern void hdmirx_hw_monitor(void);
extern bool hdmirx_hw_is_nosig(void);
extern bool hdmirx_hw_pll_lock(void);
extern void hdmirx_hw_init(enum tvin_port_e port);
extern void hdmirx_hw_uninit(void);
extern void hdmirx_fill_edid_buf(const char *buf, int size);
extern int hdmirx_read_edid_buf(char *buf, int max_size);
extern void hdmirx_fill_key_buf(const char *buf, int size);
extern int hdmirx_read_key_buf(char *buf, int max_size);
extern int hdmirx_debug(const char *buf, int size);
extern int hdmirx_hw_get_color_fmt(void);
extern int hdmirx_hw_get_3d_structure(unsigned char*, unsigned char*);
extern int hdmirx_hw_get_dvi_info(void);
extern int rx_get_colordepth(void);
extern bool hdmirx_is_key_write(void);
extern int hdmirx_hw_get_pixel_repeat(void);
extern bool hdmirx_hw_check_frame_skip(void);
extern int rx_pr(const char *fmt, ...);
extern int hdmirx_hw_dump_reg(unsigned char *buf, int size);
extern bool hdmirx_audio_pll_lock(void);
extern bool hdmirx_tmds_pll_lock(void);
extern void hdmirx_check_new_edid(void);
extern void eq_algorithm(struct work_struct *work);
extern void hdmirx_wait_query(void);
extern bool hdmirx_tmds_6g(void);
extern void rx_hpd_to_esm_handle(struct work_struct *work);

/* extern int cec_has_irq(void); */
extern void cecrx_hw_init(void);
extern int  meson_clk_measure(unsigned int clk_mux);
extern void esm_set_stable(bool stable);
extern void hdcp22_suspend(void);
extern void hdcp22_resume(void);
extern void rx_5v_det(void);
extern void hdmirx_hdcp22_hpd(bool value);
extern void hdmirx_set_hdmi20_force(int port, bool value);
extern bool hdmirx_get_hdmi20_force(int port);
extern bool esm_print_device_info(void);
extern void hdmi_rx_ctrl_hdcp_config(const struct hdmi_rx_ctrl_hdcp *hdcp);
/* vdac ctrl,adc/dac ref signal,cvbs out signal
 * module index: atv demod:0x01; dtv demod:0x02;
 * tvafe:0x4; dac:0x8, audio pll:0x10
*/
extern void vdac_enable(bool on, unsigned int module_sel);
extern void hdmirx_phy_bist_test(int lvl);
extern int hdmirx_dev_init(void);
#endif  /* _TVHDMI_H */
