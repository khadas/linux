#ifndef _HDMI_INFO_GLOBAL_H
#define _HDMI_INFO_GLOBAL_H

#include "hdmi_common.h"

/* old definitions move to hdmi_common.h */

enum Hdmi_RX_Video_State {
	STATE_VIDEO__POWERDOWN = 0,
	STATE_VIDEO__MUTED = 1,
	STATE_VIDEO__UNMUTE = 2,
	STATE_VIDEO__ON = 3,
};

struct Pixels_Num {
	short H; /* Number of horizontal pixels */
	short V; /* Number of vertical pixels */
};

enum Hdmi_Pixel_Repeat {
	NO_REPEAT = 0,
	HDMI_2_TIMES_REPEAT,
	HDMI_3_TIMES_REPEAT,
	HDMI_4_TIMES_REPEAT,
	HDMI_5_TIMES_REPEAT,
	HDMI_6_TIMES_REPEAT,
	HDMI_7_TIMES_REPEAT,
	HDMI_8_TIMES_REPEAT,
	HDMI_9_TIMES_REPEAT,
	HDMI_10_TIMES_REPEAT,
	MAX_TIMES_REPEAT,
};

enum Hdmi_Scan {
	SS_NO_DATA = 0,
	/* where some active pixelsand lines at the edges are not displayed. */
	SS_SCAN_OVER,
	/* where all active pixels&lines are displayed,
	 * with or withouta border.
	 */
	SS_SCAN_UNDER,
	SS_RSV
};

enum Hdmi_BarInfo {
	B_UNVALID = 0, B_BAR_VERT, /* Vert. Bar Infovalid */
	B_BAR_HORIZ, /* Horiz. Bar Infovalid */
	B_BAR_VERT_HORIZ,
/* Vert.and Horiz. Bar Info valid */
};

enum Hdmi_Colorimetry {
	CC_NO_DATA = 0, CC_ITU601, CC_ITU709, CC_XVYCC601, CC_XVYCC709,
};

enum Hdmi_Slacing {
	SC_NO_UINFORM = 0,
	/* Picture has been scaled horizontally */
	SC_SCALE_HORIZ,
	SC_SCALE_VERT, /* Picture has been scaled verticallv */
	SC_SCALE_HORIZ_VERT,
/* Picture has been scaled horizontally & SC_SCALE_H_V */
};

struct Hdmirx_VideoInfo {
	enum Hdmi_VIC VIC;
	enum Hdmi_Color_Space_Type color;
	enum Hdmi_Color_Depth color_depth;
	enum Hdmi_BarInfo bar_info;
	enum Hdmi_Pixel_Repeat repeat_time;
	enum Hdmi_Aspect_Ratio aspect_ratio;
	enum Hdmi_Colorimetry cc;
	enum Hdmi_Scan ss;
	enum Hdmi_Slacing sc;
};
/* -------------------HDMI VIDEO END---------------------------- */

/* -------------------HDMI AUDIO-------------------------------- */
#define TYPE_AUDIO_INFOFRAMES       0x84
#define AUDIO_INFOFRAMES_VERSION    0x01
#define AUDIO_INFOFRAMES_LENGTH     0x0A

#define HDMI_E_NONE         0x0
/* HPD Event & Status */
#define E_HPD_PULG_IN       0x1
#define E_HPD_PLUG_OUT      0x2
#define S_HPD_PLUG_IN       0x1
#define S_HPD_PLUG_OUT      0x0

#define E_HDCP_CHK_BKSV      0x1

/* -------------------HDMI AUDIO END---------------------- */

/* -------------------HDCP-------------------------------- */
/* HDCP keys from Efuse are encrypted by default, in this test HDCP keys
 * are written by CPU with encryption manually added
 */
#define ENCRYPT_KEY                                 0xbe

enum Hdcp_AuthState {
	HDCP_NO_AUTH = 0,
	HDCP_NO_DEVICE_WITH_SLAVE_ADDR,
	HDCP_BCAP_ERROR,
	HDCP_BKSV_ERROR,
	HDCP_R0S_ARE_MISSMATCH,
	HDCP_RIS_ARE_MISSMATCH,
	HDCP_REAUTHENTATION_REQ,
	HDCP_REQ_AUTHENTICATION,
	HDCP_NO_ACK_FROM_DEV,
	HDCP_NO_RSEN,
	HDCP_AUTHENTICATED,
	HDCP_REPEATER_AUTH_REQ,
	HDCP_REQ_SHA_CALC,
	HDCP_REQ_SHA_HW_CALC,
	HDCP_FAILED_ViERROR,
	HDCP_MAX
};

/* -----------------------HDCP END---------------------------------------- */

/* -----------------------HDMI TX---------------------------------- */
enum Hdmitx_DispType {
	CABLE_UNPLUG = 0,
	CABLE_PLUGIN_CHECK_EDID_I2C_ERROR,
	CABLE_PLUGIN_CHECK_EDID_HEAD_ERROR,
	CABLE_PLUGIN_CHECK_EDID_CHECKSUM_ERROR,
	CABLE_PLUGIN_DVI_OUT,
	CABLE_PLUGIN_HDMI_OUT,
	CABLE_MAX
};

struct Hdmitx_SupStatus {
	int hpd_state:1;
	int support_480i:1;
	int support_576i:1;
	int support_480p:1;
	int support_576p:1;
	int support_720p_60hz:1;
	int support_720p_50hz:1;
	int support_1080i_60hz:1;
	int support_1080i_50hz:1;
	int support_1080p_60hz:1;
	int support_1080p_50hz:1;
	int support_1080p_24hz:1;
	int support_1080p_25hz:1;
	int support_1080p_30hz:1;
};

struct Hdmitx_SupLpcmInfo {
	int support_flag:1;
	int max_channel_num:3;
	int _192k:1;
	int _176k:1;
	int _96k:1;
	int _88k:1;
	int _48k:1;
	int _44k:1;
	int _32k:1;
	int _24bit:1;
	int _20bit:1;
	int _16bit:1;
};

struct Hdmitx_SupCompressedInfo {
	int support_flag:1;
	int max_channel_num:3;
	int _192k:1;
	int _176k:1;
	int _96k:1;
	int _88k:1;
	int _48k:1;
	int _44k:1;
	int _32k:1;
	int _max_bit:10;
};

struct Hdmitx_SupSpeakerFormat {
	int rlc_rrc:1;
	int flc_frc:1;
	int rc:1;
	int rl_rr:1;
	int fc:1;
	int lfe:1;
	int fl_fr:1;
};

struct Hdmitx_VidPara {
	unsigned char VIC;
	enum Hdmi_Color_Space_Type color_prefer;
	enum Hdmi_Color_Space_Type color;
	enum Hdmi_Color_Depth color_depth;
	enum Hdmi_BarInfo bar_info;
	enum Hdmi_Pixel_Repeat repeat_time;
	enum Hdmi_Aspect_Ratio aspect_ratio;
	enum Hdmi_Colorimetry cc;
	enum Hdmi_Scan ss;
	enum Hdmi_Slacing sc;
};

struct Hdmitx_AudPara {
	enum Hdmi_Audio_Type type;
	enum Hdmi_Audio_ChnNum channel_num;
	enum Hdmi_Audio_FS sample_rate;
	enum Hdmi_Audio_SampSize sample_size;
};

struct Hdmitx_SupAudInfo {
	struct Hdmitx_SupLpcmInfo	_60958_PCM;
	struct Hdmitx_SupCompressedInfo	_AC3;
	struct Hdmitx_SupCompressedInfo	_MPEG1;
	struct Hdmitx_SupCompressedInfo	_MP3;
	struct Hdmitx_SupCompressedInfo	_MPEG2;
	struct Hdmitx_SupCompressedInfo	_AAC;
	struct Hdmitx_SupCompressedInfo	_DTS;
	struct Hdmitx_SupCompressedInfo	_ATRAC;
	struct Hdmitx_SupCompressedInfo	_One_Bit_Audio;
	struct Hdmitx_SupCompressedInfo	_Dolby;
	struct Hdmitx_SupCompressedInfo	_DTS_HD;
	struct Hdmitx_SupCompressedInfo	_MAT;
	struct Hdmitx_SupCompressedInfo	_DST;
	struct Hdmitx_SupCompressedInfo	_WMA;
	struct Hdmitx_SupSpeakerFormat		speaker_allocation;
};

/* ACR packet CTS parameters have 3 types: */
/* 1. HW auto calculated */
/* 2. Fixed values defined by Spec */
/* 3. Calculated by clock meter */
enum Hdmitx_AudCTS {
	AUD_CTS_AUTO = 0, AUD_CTS_FIXED, AUD_CTS_CALC,
};

struct Dispmode_Vic {
	const char *disp_mode;
	enum Hdmi_VIC VIC;
};

struct Hdmitx_AudInfo {
	/* !< Signal decoding type -- TvAudioType */
	enum Hdmi_Audio_Type type;
	enum Hdmi_Audio_Format format;
	/* !< active audio channels bit mask. */
	enum Hdmi_Audio_ChnNum channels;
	enum Hdmi_Audio_FS fs; /* !< Signal sample rate in Hz */
	enum Hdmi_Audio_SampSize ss;
};

/* -----------------Source Physical Address--------------- */
struct Vsdb_PhyAddr {
	unsigned char a:4;
	unsigned char b:4;
	unsigned char c:4;
	unsigned char d:4;
	unsigned char valid;
};

struct Hdmitx_Clk {
	enum Hdmi_VIC vic;
	uint64_t clk_sys;
	uint64_t clk_phy;
	uint64_t clk_encp;
	uint64_t clk_enci;
	uint64_t clk_pixel;
};

struct Hdmitx_Info {
	struct Hdmi_RX_AudioInfo audio_info;
	struct Hdmitx_SupAudInfo tv_audio_info;
	/* Hdmi_tx_video_info_t            video_info; */
	enum Hdcp_AuthState auth_state;
	enum Hdmitx_DispType output_state;
	/* -----------------Source Physical Address--------------- */
	struct Vsdb_PhyAddr vsdb_phy_addr;
	/* ------------------------------------------------------- */
	unsigned video_out_changing_flag:1;
	unsigned support_underscan_flag:1;
	unsigned support_ycbcr444_flag:1;
	unsigned support_ycbcr422_flag:1;
	unsigned tx_video_input_stable_flag:1;
	unsigned auto_hdcp_ri_flag:1;
	unsigned hw_sha_calculator_flag:1;
	unsigned need_sup_cec:1;

	/* ------------------------------------------------------- */
	unsigned audio_out_changing_flag:1;
	unsigned audio_flag:1;
	unsigned support_basic_audio_flag:1;
	unsigned audio_fifo_overflow:1;
	unsigned audio_fifo_underflow:1;
	unsigned audio_cts_status_err_flag:1;
	unsigned support_ai_flag:1;
	unsigned hdmi_sup_480i:1;

	/* ------------------------------------------------------- */
	unsigned hdmi_sup_576i:1;
	unsigned hdmi_sup_480p:1;
	unsigned hdmi_sup_576p:1;
	unsigned hdmi_sup_720p_60hz:1;
	unsigned hdmi_sup_720p_50hz:1;
	unsigned hdmi_sup_1080i_60hz:1;
	unsigned hdmi_sup_1080i_50hz:1;
	unsigned hdmi_sup_1080p_60hz:1;

	/* ------------------------------------------------------- */
	unsigned hdmi_sup_1080p_50hz:1;
	unsigned hdmi_sup_1080p_24hz:1;
	unsigned hdmi_sup_1080p_25hz:1;
	unsigned hdmi_sup_1080p_30hz:1;

	/* ------------------------------------------------------- */

};

#endif  /* _HDMI_RX_GLOBAL_H */
