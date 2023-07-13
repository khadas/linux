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
#include <uapi/linux/amlogic/amvdec_ioc.h>

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

#define _A_M_VS 'S'

/*
 * #define AMSTREAM_IOC_UD_BUF_STATUS _IOR((_A_M),
 * 0x5c, struct userdata_buf_state_t)
 */
#define GET_FIRST_APTS_FLAG                       _IOR((_A_M_VS), 0x82, int)

#define TRICKMODE_NONE       0x00
#define TRICKMODE_I          0x01
#define TRICKMODE_FFFB       0x02
#define TRICKMODE_I_HEVC     0x07

#define TRICK_STAT_DONE     0x01
#define TRICK_STAT_WAIT     0x00

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

struct codec_profile_t {
	char *name;		/* video codec short name */
	char *profile;		/* Attributes,separated by commas */
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
