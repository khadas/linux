/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef AMSTREAM_H
#define AMSTREAM_H

/* #include <linux/interrupt.h> */
#include <linux/amlogic/media/utils/vformat.h>
#include <linux/amlogic/media/utils/aformat.h>

//#ifdef CONFIG_AMLOGIC_DEBUG_ATRACE
#include <trace/events/meson_atrace.h>
//#endif
#include <uapi/linux/amlogic/amvideo.h>

#ifdef __KERNEL__
#define PORT_FLAG_IN_USE    0x0001
#define PORT_FLAG_VFORMAT   0x0002
#define PORT_FLAG_AFORMAT   0x0004
#define PORT_FLAG_FORMAT    (PORT_FLAG_VFORMAT | PORT_FLAG_AFORMAT)
#define PORT_FLAG_VID       0x0008
#define PORT_FLAG_AID       0x0010
#define PORT_FLAG_SID       0x0020
#define PORT_FLAG_UD        0x0040
#define PORT_FLAG_DRM       0x0080
#define PORT_FLAG_ID        (PORT_FLAG_VID | \
		PORT_FLAG_AID | PORT_FLAG_SID | PORT_FLAG_UD)
#define PORT_FLAG_INITED    0x100
#define PORT_FLAG_DMABUF    0x200

#define PORT_TYPE_VIDEO         0x01
#define PORT_TYPE_AUDIO         0x02
#define PORT_TYPE_MPTS          0x04
#define PORT_TYPE_MPPS          0x08
#define PORT_TYPE_ES            0x10
#define PORT_TYPE_RM            0x20
#define PORT_TYPE_SUB           0x40
#define PORT_TYPE_SUB_RD        0x80
#define PORT_TYPE_HEVC          0x100
#define PORT_TYPE_USERDATA      0x200
#define PORT_TYPE_FRAME         0x400
#define PORT_TYPE_DECODER_SCHED 0x800
#define PORT_TYPE_DUALDEC       0x1000
#endif

#define _A_M  'S'
#define AMSTREAM_IOC_VB_START                     _IOW((_A_M), 0x00, int)
#define AMSTREAM_IOC_VB_SIZE                      _IOW((_A_M), 0x01, int)
#define AMSTREAM_IOC_AB_START                     _IOW((_A_M), 0x02, int)
#define AMSTREAM_IOC_AB_SIZE                      _IOW((_A_M), 0x03, int)
#define AMSTREAM_IOC_VFORMAT                      _IOW((_A_M), 0x04, int)
#define AMSTREAM_IOC_AFORMAT                      _IOW((_A_M), 0x05, int)
#define AMSTREAM_IOC_VID                          _IOW((_A_M), 0x06, int)
#define AMSTREAM_IOC_AID                          _IOW((_A_M), 0x07, int)
#define AMSTREAM_IOC_VB_STATUS                    _IOR((_A_M), 0x08, int)
#define AMSTREAM_IOC_AB_STATUS                    _IOR((_A_M), 0x09, int)
#define AMSTREAM_IOC_SYSINFO                      _IOW((_A_M), 0x0a, int)
#define AMSTREAM_IOC_ACHANNEL                     _IOW((_A_M), 0x0b, int)
#define AMSTREAM_IOC_SAMPLERATE                   _IOW((_A_M), 0x0c, int)
#define AMSTREAM_IOC_DATAWIDTH                    _IOW((_A_M), 0x0d, int)
#define AMSTREAM_IOC_TSTAMP                       _IOW((_A_M), 0x0e, int)
#define AMSTREAM_IOC_VDECSTAT                     _IOR((_A_M), 0x0f, int)
#define AMSTREAM_IOC_ADECSTAT                     _IOR((_A_M), 0x10, int)

#define AMSTREAM_IOC_PORT_INIT                    _IO((_A_M), 0x11)

#define AMSTREAM_IOC_AUDIO_INFO                   _IOW((_A_M), 0x13, int)
#define AMSTREAM_IOC_AUDIO_RESET                  _IO((_A_M), 0x15)
#define AMSTREAM_IOC_SID                          _IOW((_A_M), 0x16, int)
#define AMSTREAM_IOC_SUB_RESET                    _IOW((_A_M), 0x1a, int)
#define AMSTREAM_IOC_SUB_LENGTH                   _IOR((_A_M), 0x1b, int)
#define AMSTREAM_IOC_SET_DEC_RESET                _IOW((_A_M), 0x1c, int)
#define AMSTREAM_IOC_TS_SKIPBYTE                  _IOW((_A_M), 0x1d, int)
#define AMSTREAM_IOC_SUB_TYPE                     _IOW((_A_M), 0x1e, int)
#define AMSTREAM_IOC_VDECINFO                     _IOR((_A_M), 0x20, int)

#define AMSTREAM_IOC_APTS                         _IOR((_A_M), 0x40, int)
#define AMSTREAM_IOC_VPTS                         _IOR((_A_M), 0x41, int)
#define AMSTREAM_IOC_PCRSCR                       _IOR((_A_M), 0x42, int)
#define AMSTREAM_IOC_SET_PCRSCR                   _IOW((_A_M), 0x4a, int)
#define AMSTREAM_IOC_PCRID                        _IOW((_A_M), 0x4f, int)

#define AMSTREAM_IOC_SUB_NUM	                  _IOR((_A_M), 0x50, int)
#define AMSTREAM_IOC_SUB_INFO	                  _IOR((_A_M), 0x51, int)
#define AMSTREAM_IOC_UD_LENGTH                    _IOR((_A_M), 0x54, int)
#define AMSTREAM_IOC_UD_POC                       _IOR((_A_M), 0x55, int)
#define AMSTREAM_IOC_UD_FLUSH_USERDATA            _IOR((_A_M), 0x56, int)

#define AMSTREAM_IOC_UD_BUF_READ                  _IOR((_A_M), 0x57, int)

#define AMSTREAM_IOC_UD_AVAILABLE_VDEC            _IOR((_A_M), 0x5c, u32)
#define AMSTREAM_IOC_GET_VDEC_ID                  _IOR((_A_M), 0x5d, int)

/*
 * #define AMSTREAM_IOC_UD_BUF_STATUS _IOR((_A_M),
 * 0x5c, struct userdata_buf_state_t)
 */

#define AMSTREAM_IOC_APTS_LOOKUP                  _IOR((_A_M), 0x81, int)
#define GET_FIRST_APTS_FLAG                       _IOR((_A_M), 0x82, int)
#define AMSTREAM_IOC_GET_FIRST_FRAME_LATENCY      _IOR((_A_M), 0x8c, int)
#define AMSTREAM_IOC_CLEAR_FIRST_FRAME_LATENCY    _IOR((_A_M), 0x8d, int)
#define AMSTREAM_IOC_SET_DEMUX                    _IOW((_A_M), 0x90, int)
#define AMSTREAM_IOC_SET_DRMMODE                  _IOW((_A_M), 0x91, int)
#define AMSTREAM_IOC_TSTAMP_uS64                  _IOW((_A_M), 0x95, int)

#define AMSTREAM_IOC_SET_VIDEO_DELAY_LIMIT_MS     _IOW((_A_M), 0xa0, int)
#define AMSTREAM_IOC_GET_VIDEO_DELAY_LIMIT_MS     _IOR((_A_M), 0xa1, int)
#define AMSTREAM_IOC_SET_AUDIO_DELAY_LIMIT_MS     _IOW((_A_M), 0xa2, int)
#define AMSTREAM_IOC_GET_AUDIO_DELAY_LIMIT_MS     _IOR((_A_M), 0xa3, int)
#define AMSTREAM_IOC_GET_AUDIO_CUR_DELAY_MS       _IOR((_A_M), 0xa4, int)
#define AMSTREAM_IOC_GET_VIDEO_CUR_DELAY_MS       _IOR((_A_M), 0xa5, int)
#define AMSTREAM_IOC_GET_AUDIO_AVG_BITRATE_BPS    _IOR((_A_M), 0xa6, int)
#define AMSTREAM_IOC_GET_VIDEO_AVG_BITRATE_BPS    _IOR((_A_M), 0xa7, int)
#define AMSTREAM_IOC_SET_APTS                     _IOW((_A_M), 0xa8, int)
#define AMSTREAM_IOC_GET_LAST_CHECKIN_APTS        _IOR((_A_M), 0xa9, int)
#define AMSTREAM_IOC_GET_LAST_CHECKIN_VPTS        _IOR((_A_M), 0xaa, int)
#define AMSTREAM_IOC_GET_LAST_CHECKOUT_APTS       _IOR((_A_M), 0xab, int)
#define AMSTREAM_IOC_GET_LAST_CHECKOUT_VPTS       _IOR((_A_M), 0xac, int)

/* subtitle.c get/set subtitle info */
#define AMSTREAM_IOC_GET_SUBTITLE_INFO            _IOR((_A_M), 0xad, int)
#define AMSTREAM_IOC_SET_SUBTITLE_INFO            _IOW((_A_M), 0xae, int)
#define AMSTREAM_IOC_GET_VIDEO_LATENCY _IOW((_A_M), 0xb4, int)

#define AMSTREAM_IOC_GET_AUDIO_CHECKIN_BITRATE_BPS _IOR((_A_M), 0xf2, int)
#define AMSTREAM_IOC_GET_VIDEO_CHECKIN_BITRATE_BPS _IOR((_A_M), 0xf3, int)

#define AMSTREAM_IOC_VDEC_RESET   _IO((_A_M), 0xf4)
#define AMSTREAM_IOC_GET_VERSION  _IOR((_A_M), 0xc0, int)
#define AMSTREAM_IOC_GET          _IOWR((_A_M), 0xc1, struct am_ioctl_parm)
#define AMSTREAM_IOC_SET          _IOW((_A_M), 0xc2, struct am_ioctl_parm)
#define AMSTREAM_IOC_GET_EX       _IOWR((_A_M), 0xc3, struct am_ioctl_parm_ex)
#define AMSTREAM_IOC_SET_EX       _IOW((_A_M), 0xc4, struct am_ioctl_parm_ex)
#define AMSTREAM_IOC_GET_PTR      _IOWR((_A_M), 0xc5, struct am_ioctl_parm_ptr)
#define AMSTREAM_IOC_SET_PTR      _IOW((_A_M), 0xc6, struct am_ioctl_parm_ptr)
#define AMSTREAM_IOC_GET_AVINFO   _IOR((_A_M), 0xc7, struct av_param_info_t)
#define AMSTREAM_IOC_GET_QOSINFO  _IOR((_A_M), 0xc8, struct av_param_qosinfo_t)
#define AMSTREAM_IOC_SET_CRC _IOW((_A_M), 0xc9, struct usr_crc_info_t)
#define AMSTREAM_IOC_GET_CRC_CMP_RESULT _IOWR((_A_M), 0xca, int)
#define AMSTREAM_IOC_GET_MVDECINFO _IOR((_A_M), 0xcb, int)
/* amstream support external stbuf */
#define AMSTREAM_IOC_INIT_EX_STBUF    _IOW((_A_M), 0xcc, struct stream_buffer_metainfo)
#define AMSTREAM_IOC_WR_STBUF_META  _IOW((_A_M), 0xcd, struct stream_buffer_metainfo)
#define AMSTREAM_IOC_GET_STBUF_STATUS _IOR((_A_M), 0xce, struct stream_buffer_status)

#define TRICKMODE_NONE       0x00
#define TRICKMODE_I          0x01
#define TRICKMODE_FFFB       0x02
#define TRICKMODE_I_HEVC     0x07

#define TRICK_STAT_DONE     0x01
#define TRICK_STAT_WAIT     0x00

#define AUDIO_EXTRA_DATA_SIZE   (4096)
#define MAX_SUB_NUM		32

enum VIDEO_DEC_TYPE {
	VIDEO_DEC_FORMAT_UNKNOWN,
	VIDEO_DEC_FORMAT_MPEG4_3,

	VIDEO_DEC_FORMAT_MPEG4_4,
	VIDEO_DEC_FORMAT_MPEG4_5,

	VIDEO_DEC_FORMAT_H264,

	VIDEO_DEC_FORMAT_MJPEG,
	VIDEO_DEC_FORMAT_MP4,
	VIDEO_DEC_FORMAT_H263,

	VIDEO_DEC_FORMAT_REAL_8,
	VIDEO_DEC_FORMAT_REAL_9,

	VIDEO_DEC_FORMAT_WMV3,

	VIDEO_DEC_FORMAT_WVC1,
	VIDEO_DEC_FORMAT_SW,
	VIDEO_DEC_FORMAT_MAX
};

enum FRAME_BASE_VIDEO_PATH {
	FRAME_BASE_PATH_IONVIDEO = 0,
	FRAME_BASE_PATH_AMLVIDEO_AMVIDEO,
	FRAME_BASE_PATH_AMLVIDEO1_AMVIDEO2,
	FRAME_BASE_PATH_DI_AMVIDEO,
	FRAME_BASE_PATH_AMVIDEO,
	FRAME_BASE_PATH_AMVIDEO2,
	FRAME_BASE_PATH_V4L_VIDEO,
	FRAME_BASE_PATH_TUNNEL_MODE,
	FRAME_BASE_PATH_V4L_OSD,
	FRAME_BASE_PATH_DI_V4LVIDEO,
	FRAME_BASE_PATH_PIP_TUNNEL_MODE,
	FRAME_BASE_PATH_V4LVIDEO,
	FRAME_BASE_PATH_DTV_TUNNEL_MODE,
	FRAME_BASE_PATH_AMLVIDEO_FENCE,
	FRAME_BASE_PATH_TUNNEL_DI_MODE,
	FRAME_BASE_PATH_V4LVIDEO_FENCE,
	FRAME_BASE_PATH_V4LVIDEO_AMLVIDEO,
	FRAME_BASE_PATH_DTV_AMLVIDEO_AMVIDEO,
	FRAME_BASE_PATH_DTV_TUNNEL_MEDIASYNC_MODE,
	FRAME_BASE_PATH_MAX
};

struct buf_status {
	int size;
	int data_len;
	int free_len;
	unsigned int read_pointer;
	unsigned int write_pointer;
};

/*struct vdec_status.status*/
#define STAT_TIMER_INIT     0x01
#define STAT_MC_LOAD        0x02
#define STAT_ISR_REG        0x04
#define STAT_VF_HOOK        0x08
#define STAT_TIMER_ARM      0x10
#define STAT_VDEC_RUN       0x20
#define STAT_TIMER_BACK_ARM 0x40
#define STAT_TIMER_BACK_INIT 0x80

/*struct vdec_status.status on error*/

#define PARSER_FATAL_ERROR              (0x10 << 16)
#define DECODER_ERROR_VLC_DECODE_TBL    (0x20 << 16)
#define PARSER_ERROR_WRONG_HEAD_VER     (0x40 << 16)
#define PARSER_ERROR_WRONG_PACKAGE_SIZE (0x80 << 16)
#define DECODER_FATAL_ERROR_SIZE_OVERFLOW     (0x100 << 16)
#define DECODER_FATAL_ERROR_UNKNOWN             (0x200 << 16)
#define DECODER_FATAL_ERROR_NO_MEM		(0x400 << 16)

#define DECODER_ERROR_MASK	(0xffff << 16)
/* The total slot number for fifo_buf */
#define NUM_FRAME_VDEC  128  //This must be 2^n
#define QOS_FRAME_NUM 8

enum E_ASPECT_RATIO {
	ASPECT_RATIO_4_3,
	ASPECT_RATIO_16_9,
	ASPECT_UNDEFINED = 255
};

struct aspect_ratio_info {
	s32 sar_width; /* -1 :invalid value */
	s32 sar_height; /* -1 :invalid value */
	s32 dar_width; /* -1 :invalid value */
	s32 dar_height; /* -1 :invalid value */
};

struct vdec_status {
	unsigned int width;
	unsigned int height;
	unsigned int fps;
	unsigned int error_count;
	unsigned int status;
	enum E_ASPECT_RATIO euAspectRatio;
	struct aspect_ratio_info aspect_ratio;
	u64 arg;
	unsigned int size;
	char reserved[60];
};

struct vdec_info {
	char vdec_name[16];
	u32 ver;
	u32 frame_width;
	u32 frame_height;
	u32 frame_rate;
	union {
		u32 bit_rate;
		/* Effective in h265,vp9,avs2 multi-instance */
		u32 bit_depth_luma;
	};
	u32 frame_dur;
	union {
		u32 frame_data;
		/* Effective in h265,vp9,avs2 multi-instance */
		u32 bit_depth_chroma;
	};
	u32 error_count;
	u32 status;
	u32 frame_count;
	u32 error_frame_count;
	u32 drop_frame_count;
	u64 total_data;
	union {
		u32 samp_cnt;
		/* Effective in h265,vp9,avs2 multi-instance */
		u32 double_write_mode;
	};
	u32 offset;
	u32 ratio_control;
	char reserved[0];
	unsigned int i_decoded_frames;/*i frames decoded*/
	unsigned int i_lost_frames;/*i frames can not be decoded*/
	unsigned int i_concealed_frames;/*i frames decoded but have some error*/
	unsigned int p_decoded_frames;
	unsigned int p_lost_frames;
	unsigned int p_concealed_frames;
	unsigned int b_decoded_frames;
	unsigned int b_lost_frames;
	unsigned int b_concealed_frames;
	char endipb_line[0];
};

struct adec_status {
	unsigned int channels;
	unsigned int sample_rate;
	unsigned int resolution;
	unsigned int error_count;
	unsigned int status;
};

struct am_io_param {
	union {
		int data;
		int id;		/* get bufstatus? or others */
	};
	int len;		/* buffer size; */
	union {
		char buf[1];
		struct buf_status status;
		struct vdec_status vstatus;
		struct adec_status astatus;
	};
};

struct am_io_info {
	union {
		int data;
		int id;
	};
	int len;
	union {
		char buf[1];
		struct vdec_info vinfo;
	};
};

struct audio_info {
	int valid;
	int sample_rate;
	int channels;
	int bitrate;
	int codec_id;
	int block_align;
	int extradata_size;
	char extradata[AUDIO_EXTRA_DATA_SIZE];
};

struct dec_sysinfo {
	unsigned int format;
	unsigned int width;
	unsigned int height;
	unsigned int rate;
	unsigned int extra;
	unsigned int status;
	unsigned int ratio;
	void *param;
	unsigned long long ratio64;
};

struct subtitle_info {
	unsigned char id;
	unsigned char width;
	unsigned char height;
	unsigned char type;
};

struct codec_profile_t {
	char *name;		/* video codec short name */
	char *profile;		/* Attributes,separated by commas */
};

struct userdata_poc_info_t {
	unsigned int poc_info;
	unsigned int poc_number;
	/*
	 * bit 0:
	 *	1, group start
	 *	0, not group start
	 * bit 1-2:
	 *	0, extension_and_user_data( 0 )
	 *	1, extension_and_user_data( 1 )
	 *	2, extension_and_user_data( 2 )
	 */
	unsigned int flags;
	unsigned int vpts;
	unsigned int vpts_valid;
	unsigned int duration;
};

struct userdata_meta_info_t {
	u32 poc_number;
	/************ flags bit defination ***********/
	/*
	 * bit 0:		//used for mpeg2
	 *	1, group start
	 *	0, not group start
	 * bit 1-2:	//used for mpeg2
	 *	0, extension_and_user_data( 0 )
	 *	1, extension_and_user_data( 1 )
	 *	2, extension_and_user_data( 2 )
	 * bit 3-6:	//video format
	 *	0,	VFORMAT_MPEG12
	 *	1,	VFORMAT_MPEG4
	 *	2,	VFORMAT_H264
	 *	3,	VFORMAT_MJPEG
	 *	4,	VFORMAT_REAL
	 *	5,	VFORMAT_JPEG
	 *	6,	VFORMAT_VC1
	 *	7,	VFORMAT_AVS
	 *	8,	VFORMAT_SW
	 *	9,	VFORMAT_H264MVC
	 *	10, VFORMAT_H264_4K2K
	 *	11, VFORMAT_HEVC
	 *	12, VFORMAT_H264_ENC
	 *	13, VFORMAT_JPEG_ENC
	 *	14, VFORMAT_VP9
	 * bit 7-9:	//frame type
	 *	0, Unknown Frame Type
	 *	1, I Frame
	 *	2, B Frame
	 *	3, P Frame
	 *	4, D_Type_MPEG2
	 * bit 10:  //top_field_first_flag valid
	 *	0: top_field_first_flag is not valid
	 *	1: top_field_first_flag is valid
	 * bit 11: //top_field_first bit val
	 */
	u32 flags;
	u32 vpts;			/*video frame pts*/
	/*
	 * 0: pts is invalid, please use duration to calculate
	 * 1: pts is valid
	 */
	u32 vpts_valid;
	/*duration for frame*/
	u32 duration;
	/* how many records left in queue waiting to be read*/
	u32 records_in_que;
	unsigned long long priv_data;
	u32 padding_data[4];
};

struct userdata_param_t {
	u32 version;
	u32 instance_id; /*input, 0~9*/
	u32 buf_len; /*input*/
	u32 data_size; /*output*/
	void *pbuf_addr; /*input*/
	struct userdata_meta_info_t meta_info; /*output*/
};

struct usr_crc_info_t {
	u32 id;
	u32 pic_num;
	u32 y_crc;
	u32 uv_crc;
};

/*******************************************************************
 * 0x100~~0x1FF : set cmd
 * 0x200~~0x2FF : set ex cmd
 * 0x300~~0x3FF : set ptr cmd
 * 0x400~~0x7FF : set reserved cmd
 * 0x800~~0x8FF : get cmd
 * 0x900~~0x9FF : get ex cmd
 * 0xA00~~0xAFF : get ptr cmd
 * 0xBFF~~0xFFF : get reserved cmd
 * 0xX00~~0xX5F : amstream cmd(X: cmd type)
 * 0xX60~~0xXAF : video cmd(X: cmd type)
 * 0xXAF~~0xXFF : reserved cmd(X: cmd type)
 ******************************************************************
 */

/*  amstream set cmd */
#define AMSTREAM_SET_VB_START                 0x101
#define AMSTREAM_SET_VB_SIZE                  0x102
#define AMSTREAM_SET_AB_START                 0x103
#define AMSTREAM_SET_AB_SIZE                  0x104
#define AMSTREAM_SET_VFORMAT                  0x105
#define AMSTREAM_SET_AFORMAT                  0x106
#define AMSTREAM_SET_VID                      0x107
#define AMSTREAM_SET_AID                      0x108
#define AMSTREAM_SET_SID                      0x109
#define AMSTREAM_SET_PCRID                    0x10A
#define AMSTREAM_SET_ACHANNEL                 0x10B
#define AMSTREAM_SET_SAMPLERATE               0x10C
#define AMSTREAM_SET_DATAWIDTH                0x10D
#define AMSTREAM_SET_TSTAMP                   0x10E
#define AMSTREAM_SET_TSTAMP_US64              0x10F
#define AMSTREAM_SET_APTS                     0x110
#define AMSTREAM_PORT_INIT                    0x111
#define AMSTREAM_SET_TRICKMODE                0x112
/* amstream,video */
#define AMSTREAM_AUDIO_RESET                  0x013
#define AMSTREAM_SUB_RESET                    0x114
#define AMSTREAM_DEC_RESET                    0x115
#define AMSTREAM_SET_TS_SKIPBYTE              0x116
#define AMSTREAM_SET_SUB_TYPE                 0x117
#define AMSTREAM_SET_PCRSCR                   0x118
#define AMSTREAM_SET_DEMUX                    0x119
#define AMSTREAM_SET_VIDEO_DELAY_LIMIT_MS     0x11A
#define AMSTREAM_SET_AUDIO_DELAY_LIMIT_MS     0x11B
#define AMSTREAM_SET_DRMMODE                  0x11C
#define AMSTREAM_SET_VIDEO_ID                 0x11E

/*  video set   cmd */
#define AMSTREAM_SET_OMX_VPTS                 0x160
#define AMSTREAM_SET_VPAUSE                   0x161
#define AMSTREAM_SET_AVTHRESH                 0x162
#define AMSTREAM_SET_SYNCTHRESH               0x163
#define AMSTREAM_SET_SYNCENABLE               0x164
#define AMSTREAM_SET_SYNC_ADISCON             0x165
#define AMSTREAM_SET_SYNC_VDISCON             0x166
#define AMSTREAM_SET_SYNC_ADISCON_DIFF        0x167
#define AMSTREAM_SET_SYNC_VDISCON_DIFF        0x168
#define AMSTREAM_SET_VIDEO_DISABLE            0x169
#define AMSTREAM_SET_VIDEO_DISCONTINUE_REPORT 0x16A
#define AMSTREAM_SET_SCREEN_MODE              0x16B
#define AMSTREAM_SET_BLACKOUT_POLICY          0x16C
#define AMSTREAM_CLEAR_VBUF                   0x16D
#define AMSTREAM_SET_CLEAR_VIDEO              0x16E
#define AMSTREAM_SET_FREERUN_MODE             0x16F
#define AMSTREAM_SET_DISABLE_SLOW_SYNC        0x170
#define AMSTREAM_SET_3D_TYPE                  0x171
#define AMSTREAM_SET_VSYNC_UPINT              0x172
#define AMSTREAM_SET_VSYNC_SLOW_FACTOR        0x173
#define AMSTREAM_SET_FRAME_BASE_PATH          0x174
#define AMSTREAM_SET_EOS                      0x175
#define AMSTREAM_SET_RECEIVE_ID               0x176
#define AMSTREAM_SET_IS_RESET                 0x177
#define AMSTREAM_SET_NO_POWERDOWN             0x178
#define AMSTREAM_SET_DV_META_WITH_EL          0x179

/*  video set ex cmd */
#define AMSTREAM_SET_EX_VIDEO_AXIS            0x260
#define AMSTREAM_SET_EX_VIDEO_CROP            0x261

/*  amstream set ptr cmd */
#define AMSTREAM_SET_PTR_AUDIO_INFO           0x300
#define AMSTREAM_SET_PTR_CONFIGS              0x301
#define AMSTREAM_SET_PTR_HDR10P_DATA 0x302

/*  amstream get cmd */
#define AMSTREAM_GET_SUB_LENGTH               0x800
#define AMSTREAM_GET_UD_LENGTH                0x801
#define AMSTREAM_GET_APTS_LOOKUP              0x802
#define AMSTREAM_GET_FIRST_APTS_FLAG          0x803
#define AMSTREAM_GET_APTS                     0x804
#define AMSTREAM_GET_VPTS                     0x805
#define AMSTREAM_GET_PCRSCR                   0x806
#define AMSTREAM_GET_LAST_CHECKIN_APTS        0x807
#define AMSTREAM_GET_LAST_CHECKIN_VPTS        0x808
#define AMSTREAM_GET_LAST_CHECKOUT_APTS       0x809
#define AMSTREAM_GET_LAST_CHECKOUT_VPTS       0x80A
#define AMSTREAM_GET_SUB_NUM                  0x80B
#define AMSTREAM_GET_VIDEO_DELAY_LIMIT_MS     0x80C
#define AMSTREAM_GET_AUDIO_DELAY_LIMIT_MS     0x80D
#define AMSTREAM_GET_AUDIO_CUR_DELAY_MS       0x80E
#define AMSTREAM_GET_VIDEO_CUR_DELAY_MS       0x80F
#define AMSTREAM_GET_AUDIO_AVG_BITRATE_BPS    0x810
#define AMSTREAM_GET_VIDEO_AVG_BITRATE_BPS    0x811
#define AMSTREAM_GET_ION_ID                   0x812
#define AMSTREAM_GET_NEED_MORE_DATA           0x813
#define AMSTREAM_GET_FREED_HANDLE             0x814
#define AMSTREAM_GET_VPTS_U64                 0x815
#define AMSTREAM_GET_APTS_U64                 0x816
/*  video get cmd */
#define AMSTREAM_GET_OMX_VPTS                 0x860
#define AMSTREAM_GET_TRICK_STAT               0x861
#define AMSTREAM_GET_TRICK_VPTS               0x862
#define AMSTREAM_GET_SYNC_ADISCON             0x863
#define AMSTREAM_GET_SYNC_VDISCON             0x864
#define AMSTREAM_GET_SYNC_ADISCON_DIFF        0x865
#define AMSTREAM_GET_SYNC_VDISCON_DIFF        0x866
#define AMSTREAM_GET_VIDEO_DISABLE            0x867
#define AMSTREAM_GET_VIDEO_DISCONTINUE_REPORT 0x868
#define AMSTREAM_GET_SCREEN_MODE              0x869
#define AMSTREAM_GET_BLACKOUT_POLICY          0x86A
#define AMSTREAM_GET_FREERUN_MODE             0x86B
#define AMSTREAM_GET_3D_TYPE                  0x86C
#define AMSTREAM_GET_VSYNC_SLOW_FACTOR        0x86D

/*  amstream get ex cmd */
#define AMSTREAM_GET_EX_VB_STATUS             0x900
#define AMSTREAM_GET_EX_AB_STATUS             0x901
#define AMSTREAM_GET_EX_VDECSTAT              0x902
#define AMSTREAM_GET_EX_ADECSTAT              0x903
#define AMSTREAM_GET_EX_UD_POC                0x904
#define AMSTREAM_GET_EX_WR_COUNT              0x905

/*  video get ex cmd */
#define AMSTREAM_GET_EX_VF_STATUS             0x960
#define AMSTREAM_GET_EX_VIDEO_AXIS            0x961
#define AMSTREAM_GET_EX_VIDEO_CROP            0x962

/*  amstream get ptr cmd */
#define AMSTREAM_GET_PTR_SUB_INFO             0xA00

#define AMSTREAM_IOC_VERSION_FIRST            2
#define AMSTREAM_IOC_VERSION_SECOND           0

struct am_ioctl_parm {
	union {
		u32 data_32;
		u64 data_64;
		enum vformat_e data_vformat;
		enum aformat_e data_aformat;
		enum FRAME_BASE_VIDEO_PATH frame_base_video_path;
		char data[8];
	};
	u32 cmd;
	char reserved[4];
};

struct am_ioctl_parm_ex {
	union {
		struct buf_status status;
		struct vdec_status vstatus;
		struct adec_status astatus;

		struct userdata_poc_info_t data_userdata_info;
		u32 wr_count;
		char data[20];

	};
	u32 cmd;
	char reserved[4];
};

struct am_ioctl_parm_ptr {
	union {
		struct audio_info *pdata_audio_info;
		struct subtitle_info *pdata_sub_info;
		void *pointer;
		char data[8];
	};
	u32 cmd;
	u32 len; /*char reserved[4]; */
};

struct vframe_qos_s {
	u32 num;
	u32 type;
	u32 size;
	u32 pts;
	int max_qp;
	int avg_qp;
	int min_qp;
	int max_skip;
	int avg_skip;
	int min_skip;
	int max_mv;
	int min_mv;
	int avg_mv;
	int decode_buffer;//For padding currently
} /*vframe_qos */;

struct vframe_comm_s {
	int vdec_id;
	char vdec_name[16];
	u32 vdec_type;
};

struct vframe_counter_s {
	struct vframe_qos_s qos;
	u32  decode_time_cost;/*us*/
	u32 frame_width;
	u32 frame_height;
	u32 frame_rate;
	union {
		u32 bit_rate;
		/* Effective in h265,vp9,avs2 multi-instance */
		u32 bit_depth_luma;
	};
	u32 frame_dur;
	union {
		u32 frame_data;
		/* Effective in h265,vp9,avs2 multi-instance */
		u32 bit_depth_chroma;
	};
	u32 error_count;
	u32 status;
	u32 frame_count;
	u32 error_frame_count;
	u32 drop_frame_count;
	u64 total_data;//this member must be 8 bytes alignment
	union {
		u32 samp_cnt;
		/* Effective in h265,vp9,avs2 multi-instance */
		u32 double_write_mode;
	};
	u32 offset;
	u32 ratio_control;
	u32 vf_type;
	u32 signal_type;
	u32 pts;
	u64 pts_us64;
	/*mediacodec report*/
	unsigned int i_decoded_frames; //i frames decoded
	unsigned int i_lost_frames;//i frames can not be decoded
	unsigned int i_concealed_frames;//i frames decoded but have some error
	unsigned int p_decoded_frames;
	unsigned int p_lost_frames;
	unsigned int p_concealed_frames;
	unsigned int b_decoded_frames;
	unsigned int b_lost_frames;
	unsigned int b_concealed_frames;
	unsigned int av_resynch_counter;
};

struct vframe_counter_s_old {
	struct vframe_qos_s qos;
	u32  decode_time_cost;/*us*/
	u32 frame_width;
	u32 frame_height;
	u32 frame_rate;
	union {
		u32 bit_rate;
		/* Effective in h265,vp9,avs2 multi-instance */
		u32 bit_depth_luma;
	};
	u32 frame_dur;
	union {
		u32 frame_data;
		/* Effective in h265,vp9,avs2 multi-instance */
		u32 bit_depth_chroma;
	};
	u32 error_count;
	u32 status;
	u32 frame_count;
	u32 error_frame_count;
	u32 drop_frame_count;
	u64 total_data;//this member must be 8 bytes alignment
	union {
		u32 samp_cnt;
		/* Effective in h265,vp9,avs2 multi-instance */
		u32 double_write_mode;
	};
	u32 offset;
	u32 ratio_control;
	u32 vf_type;
	u32 signal_type;
	u32 pts;
	u64 pts_us64;
};

struct vdec_frames_s {
	u64 hw_decode_start;
	u64 hw_decode_time;
	u32 frame_size;
	u32 rd;
	u32 wr;
	struct vframe_comm_s comm;
	struct vframe_counter_s fifo_buf[NUM_FRAME_VDEC];
};
enum FRAME_FORMAT {
	FRAME_FORMAT_UNKNOWN,
	FRAME_FORMAT_PROGRESS,
	FRAME_FORMAT_INTERLACE,
};

struct av_info_t {
	/*audio info*/
	int sample_rate;
	int channels;
	int aformat_type;
	unsigned int apts;
	unsigned int apts_err;
	/*video info*/
	unsigned int width;
	unsigned int height;
	unsigned int dec_error_count;
	unsigned int first_pic_coming;
	unsigned int fps;
	unsigned int current_fps;
	unsigned int vpts;
	unsigned int vpts_err;
	unsigned int ts_error;
	unsigned int first_vpts;
	int vformat_type;
	enum FRAME_FORMAT frame_format;
	unsigned int toggle_frame_count;/*toggle frame count*/
	unsigned int dec_err_frame_count;/*vdec error frame count*/
	unsigned int dec_frame_count;/*vdec frame count*/
	unsigned int dec_drop_frame_count;/*drop frame num*/
	int tsync_mode;
	unsigned int dec_video_bps;/*video bitrate*/
};

struct av_param_info_t {
	struct av_info_t av_info;
};

struct av_param_qosinfo_t {
	struct vframe_qos_s vframe_qos[QOS_FRAME_NUM];
};

/*This is a versioning structure, the key member is the struct_size.
 *In the 1st version it is not used,but will have its role in future.
 *https://bytes.com/topic/c/answers/811125-struct-versioning
 */
struct av_param_mvdec_t {
	int vdec_id;

	/*This member is used for versioning this structure.
	 *When passed from userspace, its value must be
	 *sizeof(struct av_param_mvdec_t)
	 */
	int struct_size;

	int slots;

	struct vframe_comm_s comm;
	struct vframe_counter_s minfo[QOS_FRAME_NUM];
};

/*old report struct*/
struct av_param_mvdec_t_old {
	int vdec_id;

	int struct_size;

	int slots;

	struct vframe_comm_s comm;
	struct vframe_counter_s_old minfo[QOS_FRAME_NUM];
};

#define SUPPORT_VDEC_NUM	(64)
int vcodec_profile_register(const struct codec_profile_t *vdec_profile);
ssize_t vcodec_profile_read(char *buf);
bool is_support_profile(char *name);
#ifdef __KERNEL__
#include <linux/interrupt.h>

/*TS demux operation interface*/
struct tsdemux_ops {
	int (*reset)(void);

	int (*set_reset_flag)(void);

	int (*request_irq)(irq_handler_t handler, irq_handler_t thread_handler, void *data);

	int (*free_irq)(void);

	int (*set_vid)(int vpid);

	int (*set_aid)(int apid);

	int (*set_sid)(int spid);

	int (*set_pcrid)(int pcrpid);

	int (*set_skipbyte)(int skipbyte);

	int (*set_demux)(int dev);

	unsigned long (*hw_dmx_lock)(unsigned long flags);

	int (*hw_dmx_unlock)(unsigned long flags);
};

void tsdemux_set_ops(struct tsdemux_ops *ops);
int tsdemux_set_reset_flag(void);
int amports_switch_gate(const char *name, int enable);
int tsdemux_get_pcr(int demux_device_index, int index, u64 *pcr);

void set_adec_func(int (*adec_func)(struct adec_status *));
void wakeup_sub_poll(void);
void init_userdata_fifo(void);
void reset_userdata_fifo(int binit);
int wakeup_userdata_poll(struct userdata_poc_info_t poc,
			 int wp, unsigned long start_phyaddr,
			int buf_size, int data_length);
int get_sub_type(void);

#endif				/**/

//#ifndef CONFIG_AMLOGIC_DEBUG_ATRACE
//static inline void ATRACE_COUNTER(const char *name, int val) { return; }
//#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
void set_vsync_pts_inc_mode(int inc);
int query_video_status(int type, int *value);
void set_trickmode_I_frame(u32 trickmode);
u32 get_post_canvas(void);
#else
static inline void set_vsync_pts_inc_mode(int inc) { return; }
static inline void set_trickmode_I_frame(u32 trickmode) { return; }
#endif

#endif /* AMSTREAM_H */
