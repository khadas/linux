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
#include "hdmirx_repeater.h"
#include "hdmi_rx_pktinfo.h"



#define RX_VER0 "Ref.2017/08/08"
/*------------------------------*/

#define RX_VER1 "Ref.2017/08/14"
/*------------------------------*/

#define RX_VER2 "Ref.2017/08/15"
/*------------------------------*/

#define RX_VER3 "Ref.2017/08/02"
/*------------------------------*/

#define RX_VER4 "Ref.2017/07/28"
/*------------------------------*/


#define HDMI_STATE_CHECK_FREQ     (20*5)
#define HW_MONITOR_TIME_UNIT    (1000/HDMI_STATE_CHECK_FREQ)

#define ABS(x) ((x) < 0 ? -(x) : (x))

#define	LOG_EN		0x01
#define VIDEO_LOG	0x02
#define AUDIO_LOG	0x04
#define HDCP_LOG	0x08
#define PACKET_LOG	0x10
#define EQ_LOG		0x20
#define REG_LOG		0x40
#define ERR_LOG		0x80
#define VSI_LOG		0x800

/* stable_check_lvl */
#define HACTIVE_EN		0x01
#define VACTIVE_EN		0x02
#define HTOTAL_EN		0x04
#define VTOTAL_EN		0x08
#define COLSPACE_EN		0x10
#define REFRESH_RATE_EN 0x20
#define REPEAT_EN		0x40
#define DVI_EN			0x80
#define INTERLACED_EN	0x100
#define HDCP_ENC_EN		0x200
#define COLOR_DEP_EN	0x400


#define TRUE 1
#define FALSE 0

#define HDCP_AUTH_COME_DELAY 150
#define HDCP_AUTH_END_DELAY (HDCP_AUTH_COME_DELAY + 150)
#define HDCP_AUTH_COUNT_MAX 5
#define HDCP_AUTH_HOLD_TIME 500
#define HHI_REG_ADDR_TXLX 0xff63c000
#define HHI_REG_ADDR_TXL 0xc883c000
#define DOLBY_VERSION_START_LENGTH 0x18
#define VSI_3D_FORMAT_INDEX	7
#define ESM_KILL_WAIT_TIMES 250
#define DV_STOP_PACKET_MAX 50
#define str_cmp(buff, str) ((strlen((str)) == strlen((buff))) &&	\
		(strncmp((buff), (str), strlen((str))) == 0))
#define pr_var(str, index) rx_pr("%5d %-30s = %#x\n", (index), #str, (str))
#define set_pr_var(buff, str, val, index) do {			\
			char index_c[5] = {'\0'};		\
			sprintf(index_c, "%d", (index));	\
			if (str_cmp((buff), (#str)) ||		\
				str_cmp((buff), (index_c))) {	\
				(str) = (val);			\
				pr_var(str, (index));		\
				return;				\
				}				\
			(index)++;				\
			} while (0)

#define EDID_SIZE			256
#define EDID_HDR_SIZE			7
#define HDR_IGNOR_TIME			500 /*5s*/
#define MAX_EDID_BUF_SIZE 1024
#define MAX_KEY_BUF_SIZE 512


/* aud sample rate stable range */
#define AUD_SR_RANGE 2000
#define AUD_SR_STB_MAX 20
/* PHY config */
#define DVI_FIXED_TO_RGB  1

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
#define FRAME_RATE_MIN		(100)
/** Refresh rate maximum [Hz] */
#define FRAME_MAX		(25000)

#define TMDS_TOLERANCE  (4000)
#define MAX_AUDIO_SAMPLE_RATE		(192000+1000)	/* 192K */
#define MIN_AUDIO_SAMPLE_RATE		(8000-1000)	/* 8K */

#define PFIFO_SIZE 160
#define K_PKT_FIFO_START		0x80

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
	struct mutex			rx_lock;
	/*struct mutex			pd_fifo_lock;*/
	struct clk *modet_clk;
	struct clk *cfg_clk;
	struct clk *acr_ref_clk;
	struct clk *audmeas_clk;
	struct clk *aud_out_clk;
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
#define HDMI_IOC_PD_FIFO_PKTTYPE_EN _IOW(HDMI_IOC_MAGIC, 0x0a,\
	uint32_t)
#define HDMI_IOC_PD_FIFO_PKTTYPE_DIS _IOW(HDMI_IOC_MAGIC, 0x0b,\
		uint32_t)
#define HDMI_IOC_GET_PD_FIFO_PARAM _IOWR(HDMI_IOC_MAGIC, 0x0c,\
	struct pd_infoframe_s)

#define IOC_SPD_INFO  _BIT(0)
#define IOC_AUD_INFO  _BIT(1)
#define IOC_MPEGS_INFO _BIT(2)
#define IOC_AVI_INFO _BIT(3)

#define HDCP22_ENABLE

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
	FSM_HPD_LOW,
	FSM_INIT,
	FSM_HPD_HIGH,
	FSM_WAIT_CLK_STABLE,
	FSM_PHY_INIT,
	FSM_WAIT_EQ_DONE,
	FSM_SIG_UNSTABLE,
	FSM_SIG_WAIT_STABLE,
	FSM_SIG_STABLE,
	FSM_SIG_READY,
};

enum irq_flag_e {
	IRQ_AUD_FLAG = 0x01,
	IRQ_PACKET_FLAG = 0x02,
};

enum colorspace_e {
	E_COLOR_RGB,
	E_COLOR_YUV422,
	E_COLOR_YUV444,
	E_COLOR_YUV420,
};

enum colorrange_e {
	E_RANGE_DEFAULT,
	E_RANGE_LIMIT,
	E_RANGE_FULL,
};

enum colorfmt_e {
	E_COLORFMT_RGB,
	E_COLORFMT_YUV422,
	E_COLORFMT_YUV444,
	E_COLORFMT_YUV420,
};

enum colordepth_e {
	E_COLORDEPTH_8 = 8,
	E_COLORDEPTH_10 = 10,
	E_COLORDEPTH_12 = 12,
	E_COLORDEPTH_16 = 16,
};

enum hdmi_mode_e {
	E_HDMI,
	E_DVI,
};

enum port_sts {
	E_PORT0,
	E_PORT1,
	E_PORT2,
	E_PORT3,
	E_PORT_NUM,
	E_5V_LOST = 0xfd,
	E_HPD_RESET = 0xfe,
	E_INIT = 0xff,
};

enum hdcp_type {
	E_HDCP14,
	E_HDCP22
};

enum e_eq_freq {
	E_EQ_NONE,
	E_EQ_SD,
	E_EQ_HD,
	E_EQ_3G,
	E_EQ_6G
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
	HDCP22_AUTH_STATE_LOST_RST = 0xff
};

enum map_addr_module_e {
	MAP_ADDR_MODULE_CBUS,
	MAP_ADDR_MODULE_HIU,
	MAP_ADDR_MODULE_HDMIRX_CAPB3,
	MAP_ADDR_MODULE_SEC_AHB,
	MAP_ADDR_MODULE_SEC_AHB2,
	MAP_ADDR_MODULE_APB4,
	MAP_ADDR_MODULE_TOP,
	MAP_ADDR_MODULE_NUM
};

enum chip_id_e {
	CHIP_ID_GXTVBB,
	CHIP_ID_TXL,
	CHIP_ID_TXLX,
};

/** Configuration clock minimum [kHz] */
#define CFG_CLK_MIN				(10000UL)
/** Configuration clock maximum [kHz] */
#define CFG_CLK_MAX				(160000UL)

/** TMDS clock minimum [kHz] */
#define TMDS_CLK_MIN			(24000UL)/* (25000UL) */
#define TMDS_CLK_MAX			(340000UL)/* (600000UL) */
struct freq_ref_s {
	bool interlace;
	uint8_t cd420;
	bool type_3d;
	unsigned int ref_freq;	/* 8 bit tmds clock */
	unsigned hactive;
	unsigned vactive;
	unsigned repeat;
	unsigned frame_rate;
	unsigned int vic;
};

/**
 * @short HDMI RX controller video parameters
 *
 * For Auxiliary Video InfoFrame (AVI) details see HDMI 1.4a section 8.2.2
 */
struct rx_video_info {
	/** DVI detection status: DVI (true) or HDMI (false) */
	bool hw_dvi;

	uint8_t hdcp_type;
	/** bit'0:auth start  bit'1:enc state(0:not endrypted 1:encrypted) **/
	uint8_t hdcp14_state;
	/** 1:decrypted 0:encrypted **/
	uint8_t hdcp22_state;
	/** Deep color mode: 8, 10, 12 or 16 [bits per pixel] */
	uint8_t colordepth;
	/** Pixel clock frequency [kHz] */
	uint32_t pixel_clk;
	/** Refresh rate [0.01Hz] */
	uint32_t frame_rate;
	/** Interlaced */
	bool interlaced;
	/** Vertical offset */
	uint32_t voffset;
	/** Vertical active */
	uint32_t vactive;
	/** Vertical total */
	uint32_t vtotal;
	/** Horizontal offset */
	uint32_t hoffset;
	/** Horizontal active */
	uint32_t hactive;
	/** Horizontal total */
	uint32_t htotal;
	/** AVI Y1-0, video format */
	uint8_t colorspace;
	/** AVI VIC6-0, video identification code */
	uint8_t hw_vic;
	/** AVI PR3-0, pixel repetition factor */
	uint8_t repeat;
	/* for sw info */
	uint8_t sw_vic;
	uint8_t sw_dvi;
	unsigned it_content;
	/** AVI Q1-0, RGB quantization range */
	unsigned rgb_quant_range;
	/** AVI Q1-0, YUV quantization range */
	unsigned yuv_quant_range;
	bool sw_fp;
	bool sw_alternative;
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

	uint32_t irq_flag;
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
	unsigned char hdcp22_exception;/*esm exception code,reg addr :0x60*/
	uint32_t hdcp_auth_count;/*hdcp auth times*/
	uint32_t hdcp_auth_time;/*the time to clear auth count*/

};

enum dolby_vision_sts_e {
	DOLBY_VERSION_IDLE,
	DOLBY_VERSION_START,
	DOLBY_VERSION_STOP,
};

struct vsi_info_s {
	unsigned identifier;
	unsigned char vd_fmt;
	unsigned char _3d_structure;
	unsigned char _3d_ext_data;
	bool dolby_vision;
	enum dolby_vision_sts_e dolby_vision_sts;
	unsigned char packet_stop;/*dv packet stop count*/
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
	/*
	int down_mix_inhibit;
	int level_shift_value;
	*/

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

#if 0
enum packet_type_e {
	ACR_PACKET = 0x01,
	ASP_PACKET = 0x02,
	GCP_PACKET = 0x03,
	ACP_PACKET = 0x04,
	ISRC1_PACKET = 0x05,
	ISRC2_PACKET = 0x06,
	OBASP_PACKET = 0x07,
	DSTAP_PACKET = 0x08,
	HBR_PACKET = 0x09,
	GMP_PACKET = 0x0a,
	ASP3D_PACKET = 0x0b,
	OB3DASP_PACKET = 0x0c,
	AMP_PACKET = 0x0d,
	MSASP_PACKET = 0x0e,
	OBMSASP_PACKET = 0x0f,
	VS_PACKET = 0x81,
	AVI_PACKET = 0x82,
	SPD_PACKET = 0x83,
	AUD_PACKET = 0x84,
	MPEGS_PACKET = 0x85,
	NTSCVBI_PACKET = 0x86,
	DRM_PACKET = 0x87
};

struct pd_infoframe_s {
	uint32_t HB;
	uint32_t PB0;
	uint32_t PB1;
	uint32_t PB2;
	uint32_t PB3;
	uint32_t PB4;
	uint32_t PB5;
	uint32_t PB6;
};
#endif

struct rx_s {
	enum chip_id_e chip_id;
	/** HDMI RX received signal changed */
	uint8_t skip;
	/*avmute*/
	uint32_t avmute_skip;
	/** HDMI RX input port 0 (A) or 1 (B) (or 2(C) or 3 (D)) */
	uint8_t port;
	/*first boot flag*/
	/*bool boot_flag;*/
	bool open_fg;
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
	uint16_t wait_no_sig_cnt;
	int aud_sr_stable_cnt;
	int aud_sr_unstable_cnt;
	/* info */
	struct rx_video_info pre;
	struct rx_video_info cur;
	struct aud_info_s aud_info;
	struct vsi_info_s vsi_info;
	struct tvin_hdr_info_s hdr_info;
	enum dolby_vision_sts_e dolby_vision_sts;
	unsigned int pwr_sts;
	/* packet type 0x81 vendor-specific */
	struct pd_infoframe_s vs_info;
	/* packet type 0x82 AVI */
	struct pd_infoframe_s avi_info;
	/* packet type 0x83 source product description */
	struct pd_infoframe_s spd_info;
	/* packet type 0x84 Audio */
	struct pd_infoframe_s aud_pktinfo;
	/* packet type 0x85 Mpeg source */
	struct pd_infoframe_s mpegs_info;
	/* packet type 0x86 NTSCVBI */
	struct pd_infoframe_s ntscvbi_info;
	/* packet type 0x87 DRM */
	struct pd_infoframe_s drm_info;

	/* packet type 0x01 info */
	struct pd_infoframe_s acr_info;
	/* packet type 0x03 info */
	struct pd_infoframe_s gcp_info;
	/* packet type 0x04 info */
	struct pd_infoframe_s acp_info;
	/* packet type 0x05 info */
	struct pd_infoframe_s isrc1_info;
	/* packet type 0x06 info */
	struct pd_infoframe_s isrc2_info;
	/* packet type 0x0a info */
	struct pd_infoframe_s gameta_info;
	/* packet type 0x0d audio metadata data */
	struct pd_infoframe_s amp_info;

	/* for debug */
	/*struct pd_infoframe_s dbg_info;*/
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
	EDID_LIST_14,
	EDID_LIST_14_AUD,
	EDID_LIST_14_420C,
	EDID_LIST_14_420VD,
	EDID_LIST_20,
	EDID_LIST_NUM
};

enum vsi_vid_format_e {
	VSI_FORMAT_NO_DATA,
	VSI_FORMAT_EXT_RESOLUTION,
	VSI_FORMAT_3D_FORMAT,
	VSI_FORMAT_FUTURE,
};

struct reg_map {
	unsigned int phy_addr;
	unsigned int size;
	void __iomem *p;
	int flag;
};


extern struct delayed_work     eq_dwork;
extern struct workqueue_struct *eq_wq;
extern struct delayed_work		esm_dwork;
extern struct workqueue_struct	*esm_wq;
extern struct delayed_work	repeater_dwork;
extern struct workqueue_struct	*repeater_wq;
extern unsigned char run_eq_flag;
extern unsigned int pwr_sts;
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
extern unsigned int esm_data_base_addr;
extern uint32_t packet_fifo_cfg;
extern uint32_t *pd_fifo_buf;
extern int packet_init(void);
extern void rx_tasklet_handler(unsigned long arg);
extern struct tasklet_struct rx_tasklet;
extern int suspend_pddq;
extern unsigned int hdmirx_addr_port;
extern unsigned int hdmirx_data_port;
extern unsigned int hdmirx_ctrl_port;
extern struct reg_map reg_maps[][MAP_ADDR_MODULE_NUM];
extern unsigned char is_alternative(void);
extern unsigned char is_frame_packing(void);
extern void clk_init(void);
#ifdef HDCP22_ENABLE
extern void rx_esm_tmdsclk_en(bool en);
#endif
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
extern int en_4k_timing;
extern unsigned int last_clk_rate;
extern int pdec_ists_en;


void wr_reg_hhi(unsigned int offset, unsigned int val);
unsigned int rd_reg_hhi(unsigned int offset);
unsigned int rd_reg(enum map_addr_module_e module, unsigned int reg_addr);
void wr_reg(enum map_addr_module_e module,
		unsigned int reg_addr, unsigned int val);
void hdmirx_wr_top(unsigned long addr, unsigned long data);
void hdmirx_wr_ctl_port(unsigned int offset, unsigned long data);
bool hdmirx_phy_clk_rate_monitor(void);

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
uint32_t rx_hdcp22_rd_reg_bits(uint16_t addr, uint32_t mask);
void rx_hdcp22_wr_reg_bits(uint16_t addr, uint32_t mask, uint32_t value);
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
int hdmirx_audio_fifo_rst(void);
void hdmirx_phy_init(void);
void hdmirx_hw_config(void);
void hdmirx_hw_probe(void);
void rx_hdcp_init(void);
void hdmi_rx_load_edid_data(unsigned char *buffer, int port);
int hdmi_rx_ctrl_edid_update(void);
void rx_set_hpd(uint8_t val);
bool hdmirx_repeat_support(void);
void hdmirx_irq_hdcp22_enable(bool);
void hdmirx_irq_enable(bool);
void hdmirx_phy_pddq(int enable);
void hdmirx_get_video_info(void);
void hdmirx_set_video_mute(bool mute);
void hdmirx_config_video(void);
unsigned int hdmirx_get_tmds_clock(void);
unsigned int hdmirx_get_pixel_clock(void);
unsigned int hdmirx_get_audio_clock(void);
unsigned int hdmirx_get_audio_pll_clock(void);
unsigned int hdmirx_get_esm_clock(void);

unsigned int hdmirx_get_clock(int index);
irqreturn_t irq_handler(int irq, void *params);

/**
 * all functions declare
 */
extern enum tvin_sig_fmt_e hdmirx_hw_get_fmt(void);
extern void edid_update(void);
extern void hdmirx_hw_monitor(void);
extern void rx_nosig_monitor(void);
extern bool rx_is_nosig(void);
extern bool rx_sig_is_ready(void);
extern void hdmirx_hw_init(enum tvin_port_e port);
extern void hdmirx_hw_uninit(void);
extern void hdmirx_fill_edid_buf(const char *buf, int size);
extern int hdmirx_read_edid_buf(char *buf, int max_size);
extern void hdmirx_fill_key_buf(const char *buf, int size);
extern int hdmirx_read_key_buf(char *buf, int max_size);
extern void rx_set_global_varaible(const char *buf, int size);
extern void rx_get_global_varaible(const char *buf);
extern int hdmirx_debug(const char *buf, int size);
extern void hdmirx_hw_get_color_fmt(void);
extern int rx_get_colordepth(void);
extern bool hdmirx_is_key_write(void);
extern int hdmirx_hw_get_pixel_repeat(void);
extern bool hdmirx_hw_check_frame_skip(void);
extern int rx_pr(const char *fmt, ...);
extern int hdmirx_hw_dump_reg(unsigned char *buf, int size);
extern bool hdmirx_audio_pll_lock(void);
extern bool is_clk_stable(void);
extern bool hdmirx_tmds_pll_lock(void);
extern void hdmirx_check_new_edid(void);
extern void eq_algorithm(struct work_struct *work);
extern void hotplug_wait_query(void);
extern bool hdmirx_tmds_6g(void);
extern void rx_hpd_to_esm_handle(struct work_struct *work);
extern void fsm_restart(void);
extern void cecrx_hw_init(void);
extern void cecrx_irq_handle(void);
extern int  meson_clk_measure(unsigned int clk_mux);
extern void esm_set_stable(bool stable);
extern void esm_recovery(void);
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
extern int pd_fifo_start_cnt;
extern int hdmirx_dev_init(void);
extern void dump_eq_data(void);
extern void repeater_dwork_handle(struct work_struct *work);
/* for other modules */
extern int External_Mute(int mute_flag);
extern void vdac_enable(bool on, unsigned int module_sel);
extern void hdmirx_dv_packet_stop(void);
extern void rx_aud_pll_ctl(bool en);
extern void rx_send_hpd_pulse(void);
#endif  /* _TVHDMI_H */
