/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __AMVDEC_IOC_H__
#define __AMVDEC_IOC_H__

#include <linux/types.h>

#define AUDIO_EXTRA_DATA_SIZE   (4096)
#define QOS_FRAME_NUM 8
#define NUM_FRAME_VDEC  128  //This must be 2^n

#define _A_M_V  'S'

enum E_ASPECT_RATIO {
	ASPECT_RATIO_4_3,
	ASPECT_RATIO_16_9,
	ASPECT_UNDEFINED = 255
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

enum vformat_e {
	VFORMAT_MPEG12 = 0,
	VFORMAT_MPEG4,
	VFORMAT_H264,
	VFORMAT_MJPEG,
	VFORMAT_REAL,
	VFORMAT_JPEG,
	VFORMAT_VC1,
	VFORMAT_AVS,
	VFORMAT_YUV,		/* Use SW decoder */
	VFORMAT_H264MVC,
	VFORMAT_H264_4K2K,
	VFORMAT_HEVC,
	VFORMAT_H264_ENC,
	VFORMAT_JPEG_ENC,
	VFORMAT_VP9,
	VFORMAT_AVS2,
	VFORMAT_AV1,
	VFORMAT_AVS3,
	VFORMAT_MAX
};

enum aformat_e {
	AFORMAT_MPEG = 0,
	AFORMAT_PCM_S16LE = 1,
	AFORMAT_AAC = 2,
	AFORMAT_AC3 = 3,
	AFORMAT_ALAW = 4,
	AFORMAT_MULAW = 5,
	AFORMAT_DTS = 6,
	AFORMAT_PCM_S16BE = 7,
	AFORMAT_FLAC = 8,
	AFORMAT_COOK = 9,
	AFORMAT_PCM_U8 = 10,
	AFORMAT_ADPCM = 11,
	AFORMAT_AMR = 12,
	AFORMAT_RAAC = 13,
	AFORMAT_WMA = 14,
	AFORMAT_WMAPRO = 15,
	AFORMAT_PCM_BLURAY = 16,
	AFORMAT_ALAC = 17,
	AFORMAT_VORBIS = 18,
	AFORMAT_AAC_LATM = 19,
	AFORMAT_APE = 20,
	AFORMAT_EAC3 = 21,
	AFORMAT_PCM_WIFIDISPLAY = 22,
	AFORMAT_TRUEHD = 25,
	/* AFORMAT_MPEG-->mp3,AFORMAT_MPEG1-->mp1,AFROMAT_MPEG2-->mp2 */
	AFORMAT_MPEG1 = 26,
	AFORMAT_MPEG2 = 27,
	AFORMAT_WMAVOI = 28,
	AFORMAT_WMALOSSLESS = 29,
	AFORMAT_PCM_S24LE = 30,
	AFORMAT_UNSUPPORT = 31,
	AFORMAT_MAX = 32
};

enum FRAME_FORMAT {
	FRAME_FORMAT_UNKNOWN,
	FRAME_FORMAT_PROGRESS,
	FRAME_FORMAT_INTERLACE,
};

struct stream_buffer_metainfo {
	union {
		__u32 stbuf_start;
		__u32 stbuf_pktaddr; //stbuf_pktaddr + stbuf_pktsize = wp
	};
	union {
		__u32 stbuf_size;
		__u32 stbuf_pktsize;
	};
	__u32 stbuf_flag;
	__u32 stbuf_private;
	__u32 jump_back_flag;
	__u32 pts_server_id;
	__u32 reserved[14];
};

struct stream_buffer_status {
	__u32 stbuf_wp;
	__u32 stbuf_rp;
	__u32 stbuf_start;
	__u32 stbuf_size;
	__u32 reserved[16];
};

struct aspect_ratio_info {
	__s32 sar_width; /* -1 :invalid value */
	__s32 sar_height; /* -1 :invalid value */
	__s32 dar_width; /* -1 :invalid value */
	__s32 dar_height; /* -1 :invalid value */
};

struct am_ioctl_parm {
	union {
		__u32 data_32;
		__u64 data_64;
		enum vformat_e data_vformat;
		enum aformat_e data_aformat;
		enum FRAME_BASE_VIDEO_PATH frame_base_video_path;
		char data[8];
	};
	__u32 cmd;
	char reserved[4];
};

struct buf_status {
	int size;
	int data_len;
	int free_len;
	unsigned int read_pointer;
	unsigned int write_pointer;
};

struct vdec_status {
	unsigned int width;
	unsigned int height;
	unsigned int fps;
	unsigned int error_count;
	unsigned int status;
	enum E_ASPECT_RATIO euAspectRatio;
	struct aspect_ratio_info aspect_ratio;
	__u64 arg;
	unsigned int size;
	char reserved[60];
};

struct adec_status {
	unsigned int channels;
	unsigned int sample_rate;
	unsigned int resolution;
	unsigned int error_count;
	unsigned int status;
};

struct usr_crc_info_t {
	__u32 id;
	__u32 pic_num;
	__u32 y_crc;
	__u32 uv_crc;
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

struct am_ioctl_parm_ex {
	union {
		struct buf_status status;
		struct vdec_status vstatus;
		struct adec_status astatus;

		struct userdata_poc_info_t data_userdata_info;
		__u32 wr_count;
		char data[20];

	};
	__u32 cmd;
	char reserved[4];
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

struct subtitle_info {
	unsigned char id;
	unsigned char width;
	unsigned char height;
	unsigned char type;
};

struct am_ioctl_parm_ptr {
	union {
		struct audio_info *pdata_audio_info;
		struct subtitle_info *pdata_sub_info;
		void *pointer;
		char data[8];
	};
	__u32 cmd;
	__u32 len; /*char reserved[4]; */
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

struct vframe_qos_s {
	__u32 num;
	__u32 type;
	__u32 size;
	__u32 pts;
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

struct av_param_qosinfo_t {
	struct vframe_qos_s vframe_qos[QOS_FRAME_NUM];
};

struct vdec_info {
	char vdec_name[16];
	__u32 ver;
	__u32 frame_width;
	__u32 frame_height;
	__u32 frame_rate;
	union {
		__u32 bit_rate;
		/* Effective in h265,vp9,avs2 multi-instance */
		__u32 bit_depth_luma;
	};
	__u32 frame_dur;
	union {
		__u32 frame_data;
		/* Effective in h265,vp9,avs2 multi-instance */
		__u32 bit_depth_chroma;
	};
	__u32 error_count;
	__u32 status;
	__u32 frame_count;
	__u32 error_frame_count;
	__u32 drop_frame_count;
	__u64 total_data;
	union {
		__u32 samp_cnt;
		/* Effective in h265,vp9,avs2 multi-instance */
		__u32 double_write_mode;
	};
	__u32 offset;
	__u32 ratio_control;
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

struct userdata_meta_info_t {
	__u32 poc_number;
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
	__u32 flags;
	__u32 vpts;			/*video frame pts*/
	/*
	 * 0: pts is invalid, please use duration to calculate
	 * 1: pts is valid
	 */
	__u32 vpts_valid;
	/*duration for frame*/
	__u32 duration;
	/* how many records left in queue waiting to be read*/
	__u32 records_in_que;
	unsigned long long priv_data;
	__u32 padding_data[4];
};

struct userdata_param_t {
	__u32 version;
	__u32 instance_id; /*input, 0~9*/
	__u32 buf_len; /*input*/
	__u32 data_size; /*output*/
	void *pbuf_addr; /*input*/
	struct userdata_meta_info_t meta_info; /*output*/
};

struct vframe_comm_s {
	int vdec_id;
	char vdec_name[16];
	__u32 vdec_type;
};

struct vframe_counter_s {
	struct vframe_qos_s qos;
	__u32  decode_time_cost;/*us*/
	__u32 frame_width;
	__u32 frame_height;
	__u32 frame_rate;
	union {
		__u32 bit_rate;
		/* Effective in h265,vp9,avs2 multi-instance */
		__u32 bit_depth_luma;
	};
	__u32 frame_dur;
	union {
		__u32 frame_data;
		/* Effective in h265,vp9,avs2 multi-instance */
		__u32 bit_depth_chroma;
	};
	__u32 error_count;
	__u32 status;
	__u32 frame_count;
	__u32 error_frame_count;
	__u32 drop_frame_count;
	__u64 total_data;//this member must be 8 bytes alignment
	union {
		__u32 samp_cnt;
		/* Effective in h265,vp9,avs2 multi-instance */
		__u32 double_write_mode;
	};
	__u32 offset;
	__u32 ratio_control;
	__u32 vf_type;
	__u32 signal_type;
	__u32 pts;
	__u64 pts_us64;
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
	__u32  decode_time_cost;/*us*/
	__u32 frame_width;
	__u32 frame_height;
	__u32 frame_rate;
	union {
		__u32 bit_rate;
		/* Effective in h265,vp9,avs2 multi-instance */
		__u32 bit_depth_luma;
	};
	__u32 frame_dur;
	union {
		__u32 frame_data;
		/* Effective in h265,vp9,avs2 multi-instance */
		__u32 bit_depth_chroma;
	};
	__u32 error_count;
	__u32 status;
	__u32 frame_count;
	__u32 error_frame_count;
	__u32 drop_frame_count;
	__u64 total_data;//this member must be 8 bytes alignment
	union {
		__u32 samp_cnt;
		/* Effective in h265,vp9,avs2 multi-instance */
		__u32 double_write_mode;
	};
	__u32 offset;
	__u32 ratio_control;
	__u32 vf_type;
	__u32 signal_type;
	__u32 pts;
	__u64 pts_us64;
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

#define AMSTREAM_IOC_VB_START                     _IOW((_A_M_V), 0x00, int)
#define AMSTREAM_IOC_VB_SIZE                      _IOW((_A_M_V), 0x01, int)
#define AMSTREAM_IOC_AB_START                     _IOW((_A_M_V), 0x02, int)
#define AMSTREAM_IOC_AB_SIZE                      _IOW((_A_M_V), 0x03, int)
#define AMSTREAM_IOC_VFORMAT                      _IOW((_A_M_V), 0x04, int)
#define AMSTREAM_IOC_AFORMAT                      _IOW((_A_M_V), 0x05, int)
#define AMSTREAM_IOC_VID                          _IOW((_A_M_V), 0x06, int)
#define AMSTREAM_IOC_AID                          _IOW((_A_M_V), 0x07, int)
#define AMSTREAM_IOC_VB_STATUS                    _IOR((_A_M_V), 0x08, int)
#define AMSTREAM_IOC_AB_STATUS                    _IOR((_A_M_V), 0x09, int)
#define AMSTREAM_IOC_SYSINFO                      _IOW((_A_M_V), 0x0a, int)
#define AMSTREAM_IOC_ACHANNEL                     _IOW((_A_M_V), 0x0b, int)
#define AMSTREAM_IOC_SAMPLERATE                   _IOW((_A_M_V), 0x0c, int)
#define AMSTREAM_IOC_DATAWIDTH                    _IOW((_A_M_V), 0x0d, int)
#define AMSTREAM_IOC_TSTAMP                       _IOW((_A_M_V), 0x0e, int)
#define AMSTREAM_IOC_VDECSTAT                     _IOR((_A_M_V), 0x0f, int)
#define AMSTREAM_IOC_ADECSTAT                     _IOR((_A_M_V), 0x10, int)

#define AMSTREAM_IOC_PORT_INIT                    _IO((_A_M_V), 0x11)

#define AMSTREAM_IOC_AUDIO_INFO                   _IOW((_A_M_V), 0x13, int)
#define AMSTREAM_IOC_AUDIO_RESET                  _IO((_A_M_V), 0x15)
#define AMSTREAM_IOC_SID                          _IOW((_A_M_V), 0x16, int)
#define AMSTREAM_IOC_SUB_RESET                    _IOW((_A_M_V), 0x1a, int)
#define AMSTREAM_IOC_SUB_LENGTH                   _IOR((_A_M_V), 0x1b, int)
#define AMSTREAM_IOC_SET_DEC_RESET                _IOW((_A_M_V), 0x1c, int)
#define AMSTREAM_IOC_TS_SKIPBYTE                  _IOW((_A_M_V), 0x1d, int)
#define AMSTREAM_IOC_SUB_TYPE                     _IOW((_A_M_V), 0x1e, int)
#define AMSTREAM_IOC_VDECINFO                     _IOR((_A_M_V), 0x20, int)

#define AMSTREAM_IOC_APTS                         _IOR((_A_M_V), 0x40, int)
#define AMSTREAM_IOC_VPTS                         _IOR((_A_M_V), 0x41, int)
#define AMSTREAM_IOC_PCRSCR                       _IOR((_A_M_V), 0x42, int)
#define AMSTREAM_IOC_SET_PCRSCR                   _IOW((_A_M_V), 0x4a, int)
#define AMSTREAM_IOC_PCRID                        _IOW((_A_M_V), 0x4f, int)

#define AMSTREAM_IOC_SUB_NUM	                  _IOR((_A_M_V), 0x50, int)
#define AMSTREAM_IOC_SUB_INFO	                  _IOR((_A_M_V), 0x51, int)
#define AMSTREAM_IOC_UD_LENGTH                    _IOR((_A_M_V), 0x54, int)
#define AMSTREAM_IOC_UD_POC                       _IOR((_A_M_V), 0x55, int)
#define AMSTREAM_IOC_UD_FLUSH_USERDATA            _IOR((_A_M_V), 0x56, int)

#define AMSTREAM_IOC_UD_BUF_READ                  _IOR((_A_M_V), 0x57, int)

#define AMSTREAM_IOC_UD_AVAILABLE_VDEC            _IOR((_A_M_V), 0x5c, __u32)
#define AMSTREAM_IOC_GET_VDEC_ID                  _IOR((_A_M_V), 0x5d, int)
#define AMSTREAM_IOC_GET_FIRST_FRAME_LATENCY      _IOR((_A_M_V), 0x8c, int)
#define AMSTREAM_IOC_CLEAR_FIRST_FRAME_LATENCY    _IOR((_A_M_V), 0x8d, int)

#define AMSTREAM_IOC_SET_DEMUX                    _IOW((_A_M_V), 0x90, int)
#define AMSTREAM_IOC_SET_DRMMODE                  _IOW((_A_M_V), 0x91, int)
#define AMSTREAM_IOC_TSTAMP_uS64                  _IOW((_A_M_V), 0x95, int)

#define AMSTREAM_IOC_APTS_LOOKUP                  _IOR((_A_M_V), 0x81, int)
#define AMSTREAM_IOC_SET_VIDEO_DELAY_LIMIT_MS     _IOW((_A_M_V), 0xa0, int)
#define AMSTREAM_IOC_GET_VIDEO_DELAY_LIMIT_MS     _IOR((_A_M_V), 0xa1, int)
#define AMSTREAM_IOC_SET_AUDIO_DELAY_LIMIT_MS     _IOW((_A_M_V), 0xa2, int)
#define AMSTREAM_IOC_GET_AUDIO_DELAY_LIMIT_MS     _IOR((_A_M_V), 0xa3, int)
#define AMSTREAM_IOC_GET_AUDIO_CUR_DELAY_MS       _IOR((_A_M_V), 0xa4, int)
#define AMSTREAM_IOC_GET_VIDEO_CUR_DELAY_MS       _IOR((_A_M_V), 0xa5, int)
#define AMSTREAM_IOC_GET_AUDIO_AVG_BITRATE_BPS    _IOR((_A_M_V), 0xa6, int)
#define AMSTREAM_IOC_GET_VIDEO_AVG_BITRATE_BPS    _IOR((_A_M_V), 0xa7, int)
#define AMSTREAM_IOC_SET_APTS                     _IOW((_A_M_V), 0xa8, int)
#define AMSTREAM_IOC_GET_LAST_CHECKIN_APTS        _IOR((_A_M_V), 0xa9, int)
#define AMSTREAM_IOC_GET_LAST_CHECKIN_VPTS        _IOR((_A_M_V), 0xaa, int)
#define AMSTREAM_IOC_GET_LAST_CHECKOUT_APTS       _IOR((_A_M_V), 0xab, int)
#define AMSTREAM_IOC_GET_LAST_CHECKOUT_VPTS       _IOR((_A_M_V), 0xac, int)

/* subtitle.c get/set subtitle info */
#define AMSTREAM_IOC_GET_SUBTITLE_INFO            _IOR((_A_M_V), 0xad, int)
#define AMSTREAM_IOC_SET_SUBTITLE_INFO            _IOW((_A_M_V), 0xae, int)
#define AMSTREAM_IOC_GET_VIDEO_LATENCY            _IOW((_A_M_V), 0xb4, int)

#define AMSTREAM_IOC_GET_AUDIO_CHECKIN_BITRATE_BPS _IOR((_A_M_V), 0xf2, int)
#define AMSTREAM_IOC_GET_VIDEO_CHECKIN_BITRATE_BPS _IOR((_A_M_V), 0xf3, int)

#define AMSTREAM_IOC_VDEC_RESET                   _IO((_A_M_V), 0xf4)
#define AMSTREAM_IOC_GET_VERSION                  _IOR((_A_M_V), 0xc0, int)
#define AMSTREAM_IOC_GET_CRC_CMP_RESULT           _IOWR((_A_M_V), 0xca, int)
#define AMSTREAM_IOC_GET_MVDECINFO                _IOR((_A_M_V), 0xcb, int)

#define AMSTREAM_IOC_GET                          _IOWR((_A_M_V), 0xc1, struct am_ioctl_parm)
#define AMSTREAM_IOC_SET                          _IOW((_A_M_V), 0xc2, struct am_ioctl_parm)
#define AMSTREAM_IOC_GET_EX                       _IOWR((_A_M_V), 0xc3, struct am_ioctl_parm_ex)
#define AMSTREAM_IOC_SET_EX                       _IOW((_A_M_V), 0xc4, struct am_ioctl_parm_ex)
#define AMSTREAM_IOC_GET_PTR                      _IOWR((_A_M_V), 0xc5, struct am_ioctl_parm_ptr)
#define AMSTREAM_IOC_SET_PTR                      _IOW((_A_M_V), 0xc6, struct am_ioctl_parm_ptr)
#define AMSTREAM_IOC_GET_AVINFO                   _IOR((_A_M_V), 0xc7, struct av_param_info_t)
#define AMSTREAM_IOC_GET_QOSINFO                  _IOR((_A_M_V), 0xc8, struct av_param_qosinfo_t)
#define AMSTREAM_IOC_SET_CRC                      _IOW((_A_M_V), 0xc9, struct usr_crc_info_t)

/* amstream support external stbuf */
#define AMSTREAM_IOC_INIT_EX_STBUF      _IOW((_A_M_V), 0xcc, struct stream_buffer_metainfo)
#define AMSTREAM_IOC_WR_STBUF_META      _IOW((_A_M_V), 0xcd, struct stream_buffer_metainfo)
#define AMSTREAM_IOC_GET_STBUF_STATUS   _IOR((_A_M_V), 0xce, struct stream_buffer_status)

#define AMSTREAM_IOC_VERSION_FIRST            2
#define AMSTREAM_IOC_VERSION_SECOND           0

#endif /* __AMVDEC_IOC_H__ */
