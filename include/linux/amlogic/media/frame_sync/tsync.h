/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef TSYNC_H
#define TSYNC_H

#define TIME_UNIT90K    (90000)
#define VIDEO_HOLD_THRESHOLD        (TIME_UNIT90K * 3)
#define VIDEO_HOLD_SLOWSYNC_THRESHOLD        (TIME_UNIT90K / 10)
#define AV_DISCONTINUE_THREDHOLD_MIN    (TIME_UNIT90K * 3)
#define AV_DISCONTINUE_THREDHOLD_MAX    (TIME_UNIT90K * 60)

#define TSYNC_IOC_MAGIC 'T'
#define TSYNC_IOC_SET_TUNNEL_MODE  _IOW(TSYNC_IOC_MAGIC, 0x00, int)
#define TSYNC_IOC_SET_VIDEO_PEEK _IOW(TSYNC_IOC_MAGIC, 0x01, int)

#define TSYNC_IOC_GET_FIRST_FRAME_TOGGLED _IOR(TSYNC_IOC_MAGIC, 0x20, int)
#define TSYNC_IOC_SET_FIRST_CHECKIN_APTS _IOW(TSYNC_IOC_MAGIC, 0x02, u32)
#define TSYNC_IOC_SET_LAST_CHECKIN_APTS _IOW(TSYNC_IOC_MAGIC, 0x03, u32)
#define TSYNC_IOC_SET_DEMUX_INFO _IOW(TSYNC_IOC_MAGIC, 0x06, struct dmx_info)
#define TSYNC_IOC_STOP_TSYNC_PCR _IO((TSYNC_IOC_MAGIC), 0x08)
#define AM_ABSSUB(a, b) ({ unsigned int _a = a; \
	unsigned int _b = b; \
	((_a) >= (_b)) ? ((_a) - (_b)) : ((_b) - (_a)); })

struct dmx_info {
int demux_device_id;
int index;
int vpid;
int apid;
int pcrpid;
};

enum avevent_e {
	VIDEO_START,
	VIDEO_PAUSE,
	VIDEO_STOP,
	VIDEO_TSTAMP_DISCONTINUITY,
	AUDIO_START,
	AUDIO_PAUSE,
	AUDIO_RESUME,
	AUDIO_STOP,
	AUDIO_TSTAMP_DISCONTINUITY,
	AUDIO_PRE_START,
	AUDIO_WAIT
};

enum tsync_mode_e {
	TSYNC_MODE_VMASTER,
	TSYNC_MODE_AMASTER,
	TSYNC_MODE_PCRMASTER,
};

enum tysnc_func_type_e {
	TSYNC_PCRSCR_VALID,
	TSYNC_PCRSCR_GET,
	TSYNC_FIRST_PCRSCR_GET,
	TSYNC_PCRAUDIO_VALID,
	TSYNC_PCRVIDEO_VALID,
	TSYNC_BUF_BY_BYTE,
	TSYNC_STBUF_LEVEL,
	TSYNC_STBUF_SPACE,
	TSYNC_STBUF_SIZE,
	TSYNC_FUNC_TYPE_MAX,
};

//bool disable_slow_sync;

int demux_get_pcr(int demux_device_index, int index, u64 *stc);

typedef u8 (*pfun_tsdemux_pcrscr_valid)(void);
//pfun_tsdemux_pcrscr_valid tsdemux_pcrscr_valid_cb;

typedef u32 (*pfun_tsdemux_pcrscr_get)(void);
//pfun_tsdemux_pcrscr_get tsdemux_pcrscr_get_cb;

typedef int (*pfun_amldemux_pcrscr_get)(int demux_device_index, int index,
					u64 *stc);
//pfun_amldemux_pcrscr_get amldemux_pcrscr_get_cb;

typedef u32 (*pfun_tsdemux_first_pcrscr_get)(void);
//pfun_tsdemux_first_pcrscr_get tsdemux_first_pcrscr_get_cb;

typedef u8 (*pfun_tsdemux_pcraudio_valid)(void);
//pfun_tsdemux_pcraudio_valid tsdemux_pcraudio_valid_cb;

typedef u8 (*pfun_tsdemux_pcrvideo_valid)(void);
//pfun_tsdemux_pcrvideo_valid tsdemux_pcrvideo_valid_cb;

typedef struct stream_buf_s *(*pfun_get_buf_by_type)(u32 type);
//pfun_get_buf_by_type get_buf_by_type_cb;

typedef u32 (*pfun_stbuf_level)(struct stream_buf_s *buf);
//pfun_stbuf_level stbuf_level_cb;

typedef u32 (*pfun_stbuf_space)(struct stream_buf_s *buf);
//pfun_stbuf_space stbuf_space_cb;

typedef u32 (*pfun_stbuf_size)(struct stream_buf_s *buf);
//pfun_stbuf_size stbuf_size_cb;

int register_tsync_callbackfunc(enum tysnc_func_type_e ntype, void *pfunc);

#ifdef MODIFY_TIMESTAMP_INC_WITH_PLL
void set_timestamp_inc_factor(u32 factor);
#endif

#ifdef CALC_CACHED_TIME
int pts_cached_time(u8 type);
#endif

u32 get_first_pic_coming(void);

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
int get_vsync_pts_inc_mode(void);
#else
static inline int get_vsync_pts_inc_mode(void);
#endif

void tsync_avevent_locked(enum avevent_e event, u32 param);

void tsync_mode_reinit(u8 type);

void tsync_avevent(enum avevent_e event, u32 param);

void tsync_audio_break(int audio_break);

void tsync_trick_mode(int trick_mode);

void tsync_set_avthresh(unsigned int av_thresh);

void tsync_set_syncthresh(unsigned int sync_thresh);

void tsync_set_dec_reset(void);

void tsync_set_enable(int enable);

int tsync_get_mode(void);

int tsync_get_sync_adiscont(void);

int tsync_get_sync_vdiscont(void);

void tsync_set_sync_adiscont(int syncdiscont);

void tsync_set_sync_vdiscont(int syncdiscont);

u32 tsync_get_sync_adiscont_diff(void);

u32 tsync_get_sync_vdiscont_diff(void);

void tsync_set_sync_adiscont_diff(u32 discontinue_diff);

void tsync_set_sync_vdiscont_diff(u32 discontinue_diff);
int tsync_set_apts(unsigned int pts);

void tsync_init(void);

void tsync_set_automute_on(int automute_on);

int tsync_get_debug_pts_checkin(void);

int tsync_get_debug_pts_checkout(void);

int tsync_get_debug_vpts(void);

int tsync_get_debug_apts(void);
int tsync_get_av_threshold_min(void);

int tsync_get_av_threshold_max(void);

int tsync_set_av_threshold_min(int min);

int tsync_set_av_threshold_max(int max);

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
void set_pts_realign(void);
#else
static inline void set_pts_realign(void);
#endif

int tsync_set_tunnel_mode(int mode);

int tsync_get_tunnel_mode(void);

void timestamp_set_pcrlatency(u32 latency);
u32 timestamp_get_pcrlatency(void);
bool tsync_check_vpts_discontinuity(unsigned int vpts);
int tsync_get_vpts_error_num(void);
int tsync_get_apts_error_num(void);
void timestamp_clac_pts_latency(u8 type, u32 pts);
u32 get_toggle_frame_count(void);
u32 timestamp_get_pts_latency(u8 type);
void timestamp_clean_pts_latency(u8 type);
int tsync_get_vpts_adjust(void);
void set_video_peek(void);
u32 get_first_frame_toggled(void);
void tsync_set_av_state(u8 type, int state);

u8 tsync_get_demux_pcrscr_valid_for_newarch(void);

u8 tsync_get_demux_pcrscr_valid(void);

void tsync_get_demux_pcr_for_newarch(void);

u8 tsync_get_demux_pcr(u32 *pcr);

void tsync_get_first_demux_pcr_for_newarch(void);

u8 tsync_get_first_demux_pcr(u32 *first_pcr);

u8 tsync_get_audio_pid_valid_for_newarch(void);

u8 tsync_get_audio_pid_valid(void);

u8 tsync_get_video_pid_valid_for_newarch(void);

u8 tsync_get_video_pid_valid(void);

void tsync_get_buf_by_type_for_newarch(u8 type, struct stream_buf_s *pbuf);

u8 tsync_get_buf_by_type(u8 type, struct stream_buf_s **pbuf);

void tsync_get_stbuf_level_for_newarch(struct stream_buf_s *pbuf,
				       u32 *buf_level);

u8 tsync_get_stbuf_level(struct stream_buf_s *pbuf, u32 *buf_level);

void tsync_get_stbuf_space_for_newarch(struct stream_buf_s *pbuf,
				       u32 *buf_space);

u8 tsync_get_stbuf_space(struct stream_buf_s *pbuf, u32 *buf_space);

void tsync_get_stbuf_size_for_newarch(struct stream_buf_s *pbuf, u32 *buf_size);

u8 tsync_get_stbuf_size(struct stream_buf_s *pbuf, u32 *buf_size);

bool tsync_get_new_arch(void);

u32 tsync_get_checkin_apts(void);

void tsync_reset(void);

static inline u32 tsync_vpts_discontinuity_margin(void)
{
	return tsync_get_av_threshold_min();
}

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
int get_vsync_pts_inc_mode(void);
void set_pts_realign(void);
#else
static inline int get_vsync_pts_inc_mode(void) { return 0; }
static inline void set_pts_realign(void) { return; }
#endif

u32 timestamp_get_pcrlatency(void);
bool tsync_check_vpts_discontinuity(unsigned int vpts);
#endif /* TSYNC_H */
