// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/timer.h>

#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>

#include <linux/amlogic/media/frame_sync/tsync_pcr.h>
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>

#ifdef CONFIG_AM_PCRSYNC_LOG
#define AMLOG
#define LOG_LEVEL_ERROR     0
#define LOG_LEVEL_ATTENTION 1
#define LOG_LEVEL_INFO      2
#define LOG_LEVEL_VAR       amlog_level_tsync_pcr
#define LOG_MASK_VAR        amlog_mask_tsync_pcr
#endif
#include <linux/amlogic/media/utils/amlog.h>
MODULE_AMLOG(AMLOG_DEFAULT_LEVEL, 0, LOG_DEFAULT_LEVEL_DESC,
	     LOG_DEFAULT_MASK_DESC);

enum play_mode_e {
	PLAY_MODE_NORMAL = 0,
	PLAY_MODE_SLOW,
	PLAY_MODE_SPEED,
	PLAY_MODE_FORCE_SLOW,
	PLAY_MODE_FORCE_SPEED,
};

enum pcr_init_priority_e {
	INIT_PRIORITY_PCR = 0,
	INIT_PRIORITY_AUDIO = 1,
	INIT_PRIORITY_VIDEO = 2,
};

#define PCR_DISCONTINUE     0x01
#define VIDEO_DISCONTINUE   0x02
#define AUDIO_DISCONTINUE   0x10

#define CHECK_INTERVAL  (HZ * 5)

#define START_AUDIO_LEVEL       256
#define START_VIDEO_LEVEL       2048
#define PAUSE_AUDIO_LEVEL         32
#define PAUSE_VIDEO_LEVEL         2560
#define UP_RESAMPLE_AUDIO_LEVEL      128
#define UP_RESAMPLE_VIDEO_LEVEL      1024
#define DOWN_RESAMPLE_CACHE_TIME     (90000 * 2)
#define NO_DATA_CHECK_TIME           4000

/*
 *the diff of system time and referrence lock,
 * which use the threshold to adjust the system time
 */
#define OPEN_RECOVERY_THRESHOLD 18000
#define CLOSE_RECOVERY_THRESHOLD 300
/*#define RECOVERY_SPAN 3 */
#define FORCE_RECOVERY_SPAN 20
#define PAUSE_CHECK_TIME   2700
#define PAUSE_RESUME_TIME   18000

/* modify it by dolby av sync 20160126 */
static u32 tsync_pcr_recovery_span = 3; /* 10 */

/* the delay from ts demuxer to the amvideo  */
#define DEFAULT_VSTREAM_DELAY 18000

#define RESAMPLE_TYPE_NONE      0
#define RESAMPLE_TYPE_DOWN      1
#define RESAMPLE_TYPE_UP        2
#define RESAMPLE_DOWN_FORCE_PCR_SLOW 3

#define MS_INTERVAL  (HZ / 1000)
#define TEN_MS_INTERVAL  (HZ / 100)

/* local system inited check type */
#define TSYNC_PCR_INITCHECK_PCR       0x0001
#define TSYNC_PCR_INITCHECK_VPTS       0x0002
#define TSYNC_PCR_INITCHECK_APTS       0x0004
#define TSYNC_PCR_INITCHECK_RECORD      0x0008
#define TSYNC_PCR_INITCHECK_END       0x0010

#define MIN_GAP (90000 * 3)		/* 3s */
#define MAX_GAP (90000 * 3)		/* 4s */
#define MAX_AV_DIFF (90000 * 5)

/* av sync monitor threshold */
#define MAX_SYNC_VGAP_TIME   90000
#define MIN_SYNC_VCHACH_TIME   45000

#define MAX_SYNC_AGAP_TIME   45000
#define MIN_SYNC_ACHACH_TIME   27000

#define PLAY_MODE_THRESHOLD 500	/*ms*/
#define PLAY_PCR_INVALID_THRESHOLD	(10 * 90000)
/* ------------------------------------------------------------------ */
/* The const */

static u32 tsync_pcr_discontinue_threshold = (TIME_UNIT90K * 1.5);
static u32 tsync_pcr_ref_latency = (TIME_UNIT90K * 0.3);

/* use for pcr valid mode */
static u32 tsync_pcr_max_cache_time = TIME_UNIT90K * 2.4;
static u32 tsync_pcr_up_cache_time = TIME_UNIT90K * 2.2;
/* modify it by dolby av sync */
static u32 tsync_pcr_down_cache_time = TIME_UNIT90K * 0.8;   /* 0.6 */
static u32 tsync_pcr_min_cache_time = TIME_UNIT90K * 0.4;    /* 0.2 */

static u32 tsync_pcr_max_delay_time = TIME_UNIT90K * 3;
static u32 tsync_pcr_up_delay_time = TIME_UNIT90K * 2;
static u32 tsync_pcr_down_delay_time = TIME_UNIT90K * 1.5;
static u32 tsync_pcr_min_delay_time = TIME_UNIT90K * 0.8;

static u32 tsync_apts_adj_value = 150000;  /* add it by dolby av sync */
static u32 tsync_pcr_adj_value = 27000;	/* 300ms */

static u32 tsync_pcr_first_video_frame_pts;
static u32 tsync_pcr_first_audio_frame_pts;

/* reset control flag */
static u8 tsync_pcr_reset_flag;
static int tsync_pcr_asynccheck_cnt;
static int tsync_pcr_vsynccheck_cnt;

static int init_check_first_systemtime;
static u32 init_check_first_demuxpcr;

/* ------------------------------------------------------------------ */
/* The variate */

static struct timer_list tsync_pcr_check_timer;

static u32 tsync_pcr_tsdemux_startpcr;

static int tsync_pcr_vpause_flag;
static int tsync_pcr_apause_flag;
static int tsync_pcr_vstart_flag;
static int tsync_pcr_astart_flag;
static u8 tsync_pcr_inited_flag;
static u8 tsync_pcr_inited_mode = INIT_PRIORITY_PCR;
static u32 tsync_pcr_freerun_mode;

static int tsync_pcr_latency_value;

static s64 tsync_pcr_stream_delta;

/* the really ts demuxer pcr, haven't delay */
static u32 tsync_pcr_last_tsdemuxpcr;
static u32 tsync_pcr_discontinue_local_point;
static u32 tsync_pcr_discontinue_waited;
/* the time waited the v-discontinue to happen */
static u8 tsync_pcr_tsdemuxpcr_discontinue;
/* the boolean value */
static u32 tsync_pcr_discontinue_point;

static int abuf_level;
static int abuf_size;
static int vbuf_level;
static int vbuf_size;
static int play_mode = PLAY_MODE_NORMAL;
static u8 tsync_pcr_started;
static int tsync_pcr_read_cnt;
static u8 tsync_pcr_usepcr;
/* static int tsync_pcr_debug_pcrscr = 100; */
static u64 first_time_record;
static u8 wait_pcr_count;

static int abuf_fatal_error;
static int vbuf_fatal_error;

static u32 tsync_pcr_first_jiffes;
static u32 tsync_pcr_first_systime;

static u32 tsync_pcr_jiffes_diff;
static u32 tysnc_pcr_systime_diff;
static u32 tsync_last_play_mode;

static u32 tsync_use_demux_pcr = 1;

static u32 tsync_pcr_last_jiffes;
static int tsync_demux_pcr_valid = 1;
static u32 tsync_last_pcr_get;
static u32 tsync_demux_pcr_time_interval;
static u32 tsync_system_time_interval;
static u32 tsync_enable_demuxpcr_check;
static u32 tsync_enable_bufferlevel_tune;

static u32 tsync_pcr_debug;
static u32 tsync_demux_last_pcr;
static u32 tsync_demux_pcr_discontinue_count;
static u32 tsync_demux_pcr_continue_count;

static int tsync_vpts_adjust;
static u32 tsync_disable_demux_pcr;
static u32 tsync_audio_mode;
static u32 tsync_audio_state;
static u32 tsync_video_state;
static int tsync_audio_underrun;
static int tsync_audio_underrun_maxgap = 40;
static int tsync_audio_discontinue;
static int tsync_video_discontinue;
static int tsync_audio_continue_count;
static u32 tsync_video_continue_count;
static u32 last_discontinue_checkin_apts;
static u32 last_discontinue_checkin_vpts;
static u32 last_pcr_checkin_apts;
static u32 last_pcr_checkin_vpts;
static u32 last_pcr_checkin_apts_count;
static u32 last_pcr_checkin_vpts_count;
static DEFINE_SPINLOCK(tsync_pcr_lock);
static bool video_pid_valid;
static bool video_jumped;
static bool audio_jumped;

module_param(tsync_pcr_max_cache_time, uint, 0664);
MODULE_PARM_DESC(tsync_pcr_max_cache_time, "\n tsync pcr max cache time\n");

module_param(tsync_pcr_up_cache_time, uint, 0664);
MODULE_PARM_DESC(tsync_pcr_up_cache_time, "\n tsync pcr up cache time\n");

module_param(tsync_pcr_down_cache_time, uint, 0664);
MODULE_PARM_DESC(tsync_pcr_down_cache_time, "\n tsync pcr down cache time\n");

module_param(tsync_pcr_min_cache_time, uint, 0664);
MODULE_PARM_DESC(tsync_pcr_min_cache_time, "\n tsync pcr min cache time\n");

extern pfun_tsdemux_pcraudio_valid tsdemux_pcraudio_valid_cb;
extern pfun_tsdemux_pcrvideo_valid tsdemux_pcrvideo_valid_cb;
extern pfun_tsdemux_first_pcrscr_get tsdemux_first_pcrscr_get_cb;
extern pfun_stbuf_size stbuf_size_cb;
extern pfun_tsdemux_pcrscr_get tsdemux_pcrscr_get_cb;
extern pfun_get_buf_by_type get_buf_by_type_cb;
extern pfun_stbuf_level stbuf_level_cb;
extern pfun_stbuf_space stbuf_space_cb;
extern pfun_tsdemux_pcrscr_valid tsdemux_pcrscr_valid_cb;
extern pfun_tsdemux_pcrscr_valid tsdemux_pcrscr_valid_cb;

struct stream_buf_s {
	s32 flag;
	u32 type;
	unsigned long buf_start;
	struct page *buf_pages;
	int buf_page_num;
	u32 buf_size;
	u32 default_buf_size;
	u32 canusebuf_size;
	u32 first_tstamp;
	const ulong reg_base;
	wait_queue_head_t wq;
	struct timer_list timer;
	u32 wcnt;
	u32 buf_wp;
	u32 buf_rp;
	u32 max_buffer_delay_ms;
	u64 last_write_jiffies64;
	void *write_thread;
	int for_4k;
	bool is_secure;
} /*stream_buf_t*/;

int get_stream_buffer_level(int type)
{
	struct stream_buf_s *pbuf = NULL;
	u32 buf_level = 0;

	if (type == 0) {
		/* video */
		tsync_get_buf_by_type(PTS_TYPE_VIDEO, &pbuf);
		if (pbuf && tsync_get_stbuf_level(pbuf, &buf_level) &&
			pbuf->flag & 0x02/*BUF_FLAG_IN_USE*/)
			return buf_level;
	} else {
		/* audio */
		tsync_get_buf_by_type(PTS_TYPE_AUDIO, &pbuf);
		if (pbuf && tsync_get_stbuf_level(pbuf, &buf_level) &&
		    pbuf->flag & 0x02/*BUF_FLAG_IN_USE*/)
			return stbuf_level_cb(pbuf);
		}

	return 0;
}
EXPORT_SYMBOL(get_stream_buffer_level);

int get_stream_buffer_size(int type)
{
	struct stream_buf_s *pbuf = NULL;
	u32 buf_size = 0;
	if (type == 0) {
		/* video */
		tsync_get_buf_by_type(PTS_TYPE_VIDEO, &pbuf);
		if (pbuf && tsync_get_stbuf_size(pbuf, &buf_size) &&
		    pbuf->flag & 0x02/*BUF_FLAG_IN_USE*/)
			return buf_size;
	} else {
		/* audio */
		tsync_get_buf_by_type(PTS_TYPE_AUDIO, &pbuf);
		if (pbuf && tsync_get_stbuf_size(pbuf, &buf_size) &&
		    pbuf->flag & 0x02/*BUF_FLAG_IN_USE*/)
			return buf_size;
	}

	return 0;
}
EXPORT_SYMBOL(get_stream_buffer_size);

int get_min_cache_delay(void)
{
	/*int video_cache_time = calculation_vcached_delayed();*/
	/*int audio_cache_time = calculation_acached_delayed();*/
	int audio_cache_time =
		90 * calculation_stream_delayed_ms(PTS_TYPE_AUDIO, NULL, NULL);
	int video_cache_time =
		90 * calculation_stream_delayed_ms(PTS_TYPE_VIDEO, NULL, NULL);
	if (video_cache_time > 0 && audio_cache_time > 0)
		return min(video_cache_time, audio_cache_time);
	else if (video_cache_time > 0)
		return video_cache_time;
	else if (audio_cache_time > 0)
		return audio_cache_time;
	else
		return 0;
}
EXPORT_SYMBOL(get_min_cache_delay);

u32 tsync_pcr_vstream_delayed(void)
{
	int cur_delay = calculation_vcached_delayed();

	if (cur_delay == -1)
		return DEFAULT_VSTREAM_DELAY;

	return cur_delay;
}
EXPORT_SYMBOL(tsync_pcr_vstream_delayed);

u32 tsync_pcr_get_min_checkinpts(void)
{
	u32 last_checkin_vpts = get_last_checkin_pts(PTS_TYPE_VIDEO);
	u32 last_checkin_apts = get_last_checkin_pts(PTS_TYPE_AUDIO);

	if (last_checkin_vpts > 0 && last_checkin_apts > 0)
		return min(last_checkin_vpts, last_checkin_apts);
	else if (last_checkin_apts > 0)
		return last_checkin_apts;
	else if (last_checkin_vpts > 0)
		return last_checkin_vpts;
	else
		return 0;
}
EXPORT_SYMBOL(tsync_pcr_get_min_checkinpts);

static void tsync_set_pcr_mode(int mode, u32 param)
{
	if (tsync_pcr_debug & 0x03) {
		pr_info("tsync_use_demux_pcr: %d to %d, param=0x%x\n",
			tsync_use_demux_pcr, mode, param);
	}
	if (mode == 0) {
		tsync_use_demux_pcr = 0;
		timestamp_pcrscr_set(param);
		if (tsync_pcr_vpause_flag != 1)
			timestamp_pcrscr_enable(1);
		timestamp_vpts_set(param);
	} else if (mode == 1) {
		tsync_use_demux_pcr = 1;
		timestamp_pcrscr_enable(0);
	}
}

void tsync_set_av_state(u8 type, int state)
{
	if (type == 0) {
		tsync_video_state = state;
		return;
	} else if (type == 1) {
		tsync_audio_state = state;
		if ((tsync_pcr_debug & 0x01) && state == AUDIO_STOP)
			pr_info("audio stopped!\n");
	}
}
EXPORT_SYMBOL(tsync_set_av_state);

static u32 tsync_calcpcr_by_audio(u32 first_apts, uint64_t audio_cache_pts)
{
	u32 ref_pcr = 0;
	int diff = 0;

	if (audio_cache_pts >= tsync_pcr_ref_latency) {
		ref_pcr = first_apts + audio_cache_pts - tsync_pcr_ref_latency;
		pr_info("use cache audio pts calc:0x%x\n", ref_pcr);
	} else {
		diff = tsync_pcr_ref_latency - audio_cache_pts;
		pr_info("audio need cache diff = %d\n", diff);
		if (first_apts > diff)
			ref_pcr = first_apts - diff;
		else
			ref_pcr = 0;
	}

	return ref_pcr;
}

static u32 tsync_calcpcr_by_video(u32 first_vpts, uint64_t video_cache_pts)
{
	u32 ref_pcr = 0;
	int diff = 0;

	if (video_cache_pts >= tsync_pcr_ref_latency) {
		ref_pcr = first_vpts;
	} else {
		diff = tsync_pcr_ref_latency - video_cache_pts;
		pr_info("video need cache diff = %d\n", diff);
		if (first_vpts > diff)
			ref_pcr = first_vpts - diff;
		else
			ref_pcr = 0;
	}
	return ref_pcr;
}

u32 tsync_pcr_get_ref_pcr(void)
{
	u32 first_vpts = 0, first_apts = 0;
	u32 first_checkin_vpts = 0;
	u32 first_checkin_apts = 0;
	u32 cur_checkin_apts = 0, cur_checkin_vpts = 0;
	u32 ref_pcr = 0;
	u32 min_checkinpts = 0;
	int av_diff = 0;
	u64 audio_cache_pts = 0;
	u64 video_cache_pts = 0;
	u64 pts_cache_tmp = 0;
	int audio_cache_ms = 0;
	int video_cache_ms = 0;

	first_apts = timestamp_firstapts_get();
	first_vpts = timestamp_firstvpts_get();
	first_checkin_apts = timestamp_checkin_firstapts_get();
	first_checkin_vpts = timestamp_checkin_firstvpts_get();

	if (!first_apts)
		first_apts = first_checkin_apts;
	if (!first_vpts)
		first_vpts = first_checkin_vpts;
	/*Use the first output vpts to calc buffer cache and init pcr*/
	if (tsync_pcr_vstart_flag == 1) {
		first_vpts = tsync_pcr_first_video_frame_pts;
		pr_info("Video Start, first vpts:0x%x\n", first_vpts);
	}
	cur_checkin_vpts = get_last_checkin_pts(PTS_TYPE_VIDEO);
	cur_checkin_apts = get_last_checkin_pts(PTS_TYPE_AUDIO);
	if (cur_checkin_vpts != 0xffffffff)
		video_cache_pts = cur_checkin_vpts - first_vpts;
	if (cur_checkin_apts != 0xffffffff)
		audio_cache_pts = cur_checkin_apts - first_apts;
	if (first_apts == 0 && cur_checkin_apts == 0xffffffff)
		audio_cache_pts = 0;
	if (first_vpts == 0 && cur_checkin_vpts == 0xffffffff)
		video_cache_pts = 0;

	pts_cache_tmp = audio_cache_pts;
#ifdef CONFIG_64BIT
	audio_cache_ms = audio_cache_pts / 90;
	video_cache_ms = video_cache_pts / 90;
#else
	audio_cache_ms = do_div(pts_cache_tmp, 90);
	pts_cache_tmp = video_cache_pts;
	video_cache_ms = do_div(pts_cache_tmp, 90);
#endif
	av_diff = first_apts - first_vpts;
	pr_info("get_ref_pcr:apts(%x,%x,%x,cache:%dms),vpts(%x,%x,%x,cache:%dms)\n",
		first_checkin_apts, timestamp_firstapts_get(), cur_checkin_apts,
		audio_cache_ms, first_checkin_vpts, timestamp_firstvpts_get(),
		cur_checkin_vpts, video_cache_ms);
	pr_info("first_apts=%x, first_vpts=%x,av_diff=%d\n",
		first_apts, first_vpts, av_diff);

	if ((first_apts != 0 && first_apts < first_vpts &&
	     (first_vpts - first_apts <= MAX_AV_DIFF) &&
	     (audio_cache_pts < first_vpts - first_apts
					+ tsync_pcr_ref_latency)) ||
	    (first_vpts == 0 && cur_checkin_vpts == 0xffffffff)) {
		ref_pcr = tsync_calcpcr_by_audio(first_apts,
						audio_cache_pts);
		pr_info("calc pcr by audio ref_pcr=0x%x\n", ref_pcr);
	} else if ((first_vpts != 0 && first_apts >= first_vpts) ||
		   (first_apts == 0 && cur_checkin_apts == 0xffffffff)) {
		ref_pcr = tsync_calcpcr_by_video(first_vpts,
						 video_cache_pts);
		pr_info("calc pcr by video ref_pcr=0x%x\n", ref_pcr);
	} else {
		if (first_apts == 0 && first_vpts == 0) {
			min_checkinpts = tsync_pcr_get_min_checkinpts();
			ref_pcr = min_checkinpts - tsync_pcr_ref_latency;
			pr_info("calc ref_pcr by min_checkin:0x%x, pcr:0x%x\n",
				min_checkinpts, ref_pcr);
		} else if ((first_vpts > 0) &&
			   (first_apts == 0 &&
			    cur_checkin_apts != 0xffffffff) &&
			   (first_vpts > cur_checkin_apts) &&
			   ((first_vpts - cur_checkin_apts) <= MAX_AV_DIFF) &&
			   (audio_cache_pts < first_vpts - cur_checkin_apts +
					tsync_pcr_ref_latency)) {
			ref_pcr = tsync_calcpcr_by_audio(cur_checkin_apts,
							 audio_cache_pts);
			pr_info("cal pcr by checkin_apts:0x%x,ref_pcr:0x%x\n",
				cur_checkin_apts, ref_pcr);
		} else {
			if (first_vpts == 0 && cur_checkin_vpts != 0xffffffff) {
				ref_pcr = cur_checkin_vpts -
						tsync_pcr_ref_latency;
				pr_info("use cur_checin vpts to calc\n");
			} else {
				ref_pcr = tsync_calcpcr_by_video(first_vpts,
							 video_cache_pts);
			}
			pr_info("default cal pcr by vpts, pcr:0x%x\n", ref_pcr);
		}
	}
	pr_info("return ref_pcr = 0x%x\n", ref_pcr);
	return ref_pcr;
}

void tsync_pcr_pcrscr_set(void)
{
	u32 first_pcr = 0, first_vpts = 0, first_apts = 0;
	u32 cur_checkin_apts = 0, cur_checkin_vpts = 0, min_checkinpts = 0;
	u32 cur_pcr = 0, ref_pcr = 0;
	u32 tmp_pcr = 0;
	u8 complete_init_flag =
		TSYNC_PCR_INITCHECK_PCR | TSYNC_PCR_INITCHECK_VPTS |
		TSYNC_PCR_INITCHECK_APTS;

	if (tsync_pcr_inited_flag & complete_init_flag)
		return;

	tsync_get_first_demux_pcr(&first_pcr);
	first_apts = timestamp_firstapts_get();
	first_vpts = timestamp_firstvpts_get();
	if (!first_apts)
		first_apts = timestamp_checkin_firstapts_get();
	if (!first_vpts)
		first_vpts = timestamp_checkin_firstvpts_get();
	cur_checkin_vpts = get_last_checkin_pts(PTS_TYPE_VIDEO);
	cur_checkin_apts = get_last_checkin_pts(PTS_TYPE_AUDIO);
	min_checkinpts = tsync_pcr_get_min_checkinpts();

	tsync_get_demux_pcr(&cur_pcr);
	abuf_level = get_stream_buffer_level(1);
	vbuf_level = get_stream_buffer_level(0);

	if (tsync_get_video_pid_valid() &&
	    tsync_get_audio_pid_valid()) {
		if (cur_checkin_apts == 0xffffffff)
			pr_info("%s not checkin_apts\n ", __func__);
	} else {
		if (tsync_pcr_debug & 0x01)
			pr_info("now and the cur_checkin_apts is %x\n ",
				cur_checkin_apts);
	}
	if ((!cur_checkin_vpts || cur_checkin_vpts == 0xffffffff) &&
	    first_vpts)
		cur_checkin_vpts = first_vpts;
	if ((!cur_checkin_apts || cur_checkin_apts == 0xffffffff) &&
	    first_apts)
		cur_checkin_apts = first_apts;
	if (tsync_pcr_debug & 0x01) {
		pr_info("check first_pcr=%x,first_apts=%x,first_vpts %x\n",
			first_pcr, first_apts, first_vpts);
		pr_info("cur_pcr=%x,checkin_apts=%x,checkin_vpts=%x\n",
			cur_pcr, cur_checkin_apts, cur_checkin_vpts);
		pr_info("gap_pa=%d,gap_pv=%d,gap_av=%d\n",
			(int)(cur_pcr - cur_checkin_apts) / 90,
			(int)(cur_pcr - cur_checkin_vpts) / 90,
			(int)(cur_checkin_apts - cur_checkin_vpts) / 90);
		pr_info("alevel=%d vlevel=%d mode=%d min_checkinpts=0x%x\n",
			abuf_level, vbuf_level, tsync_use_demux_pcr,
			min_checkinpts);
	}
	/* check the valid of the pcr */
	if (cur_pcr && cur_checkin_vpts && cur_checkin_apts &&
	    cur_checkin_vpts != 0xffffffff &&
	    cur_checkin_apts != 0xffffffff &&
	    !(tsync_pcr_inited_flag & complete_init_flag)) {
		u32 gap_pa, gap_pv, gap_av;

		gap_pa = abs(cur_pcr - cur_checkin_apts);
		gap_av = abs(cur_checkin_apts - cur_checkin_vpts);
		gap_pv = abs(cur_pcr - cur_checkin_vpts);
		if (gap_pa > MAX_GAP && gap_pv > MAX_GAP) {
			if (gap_av > MAX_GAP)
				ref_pcr = cur_checkin_vpts;
			else
				ref_pcr = min_checkinpts -
					tsync_pcr_ref_latency;
			tsync_set_pcr_mode(0, ref_pcr);
			tsync_pcr_inited_mode =
				INIT_PRIORITY_VIDEO;
			tsync_pcr_inited_flag |= TSYNC_PCR_INITCHECK_VPTS;
			pr_info("tsync_set:pcrsrc %x,vpts %x, mode %d\n",
				timestamp_pcrscr_get(), timestamp_vpts_get(),
				tsync_use_demux_pcr);
			return;
		}
		if (cur_checkin_vpts < cur_pcr &&
		    cur_checkin_vpts < cur_checkin_apts) {
			ref_pcr = cur_checkin_vpts -
				tsync_pcr_ref_latency;
			tsync_set_pcr_mode(0, ref_pcr);
			tsync_pcr_inited_mode =
				INIT_PRIORITY_VIDEO;
			tsync_pcr_inited_flag |= TSYNC_PCR_INITCHECK_VPTS;
			pr_info("tsync_set:pcrscr %x,vpts %x,mode %d,ref_pcr %x\n",
				timestamp_pcrscr_get(), timestamp_vpts_get(),
				tsync_use_demux_pcr, ref_pcr);
			return;
		}
	}
	/* decide use which para to init */
	if (cur_pcr && !(tsync_pcr_inited_flag & complete_init_flag) &&
	    min_checkinpts != 0) {
		tsync_pcr_inited_flag |= TSYNC_PCR_INITCHECK_PCR;
		if ((abs(cur_pcr - cur_checkin_vpts) >
			PLAY_PCR_INVALID_THRESHOLD) &&
			cur_checkin_vpts != 0xffffffff) {
			if (first_vpts != 0) {
				pr_info("use first_vpts:0x%x init\n",
					first_vpts);
				ref_pcr = first_vpts - tsync_pcr_ref_latency;
			} else {
				ref_pcr = cur_checkin_vpts -
					tsync_pcr_ref_latency;
			}
			tsync_set_pcr_mode(0, ref_pcr);
			tsync_pcr_inited_mode = INIT_PRIORITY_VIDEO;
		} else if (((cur_pcr > min_checkinpts) &&
			(cur_pcr - min_checkinpts) > 500 * 90) ||
			tsync_disable_demux_pcr == 1) {
			if (abs(cur_checkin_apts - cur_checkin_vpts)
				> MAX_GAP) {
				if (first_vpts != 0) {
					pr_info("use first_vpts calc pcr\n");
					ref_pcr = first_vpts -
						tsync_pcr_ref_latency;
				} else {
					ref_pcr = cur_checkin_vpts -
						tsync_pcr_ref_latency;
				}
			} else {
				ref_pcr = min_checkinpts;
			}
			tsync_set_pcr_mode(0, ref_pcr);
			tsync_pcr_inited_mode = INIT_PRIORITY_VIDEO;
		} else {
			tsync_set_pcr_mode(1, ref_pcr);
			tsync_pcr_inited_mode = INIT_PRIORITY_PCR;
		}
		pr_info("1-tsync_set:pcrsrc %x,vpts %x,mode:%d\n",
			timestamp_pcrscr_get(), timestamp_firstvpts_get(),
			tsync_use_demux_pcr);
		tsync_get_demux_pcr(&init_check_first_demuxpcr);
		return;
	}

	if (first_apts && !(tsync_pcr_inited_flag & complete_init_flag) &&
	    min_checkinpts != 0) {
		tsync_pcr_inited_flag |= TSYNC_PCR_INITCHECK_APTS;
		if (tsync_pcr_debug & 0x02)
			pr_info("cur_pcr=%x first_apts=%x\n",
				cur_pcr, first_apts);
		if (cur_pcr && abs(cur_pcr - first_apts)
			> PLAY_PCR_INVALID_THRESHOLD) {
			ref_pcr = first_apts;
			tsync_use_demux_pcr = 0;
			if (tsync_pcr_debug & 0x01) {
				pr_info("check init.first_pcr=0x%x, first_apts=0x%x, ",
				first_pcr, first_apts);
				pr_info("first_vpts=0x%x, cur_pcr = 0x%x, checkin_vpts=0x%x, ",
				first_vpts, cur_pcr, cur_checkin_vpts);
				pr_info("checkin_apts=0x%x alevel=%d vlevel=%d\n",
				cur_checkin_apts, abuf_level, vbuf_level);
				pr_info("[%d]init by apts. pcr=%x usepcr=%d\n",
				__LINE__, ref_pcr, tsync_pcr_usepcr);
			}
		} else if (cur_pcr) {
			tsync_use_demux_pcr = 1;
			tsync_pcr_inited_mode = INIT_PRIORITY_PCR;
			ref_pcr = timestamp_pcrscr_get();
			if ((tsync_pcr_debug & 0x01) &&
			    tsync_get_demux_pcr(&tmp_pcr)) {
				pr_info("use the pcr from stream , the cur demux pcr is %x\n",
				tmp_pcr);
				pr_info("the timestamp pcr is %x\n",
				timestamp_pcrscr_get());
				pr_info("the timestamp video pts is %x\n",
				timestamp_firstvpts_get());
			}
		}
		ref_pcr = tsync_pcr_get_ref_pcr();
		pr_info("check apts, get min checkin ref_pcr = 0x%x", ref_pcr);
		tsync_set_pcr_mode(0, ref_pcr);
		tsync_pcr_inited_mode = INIT_PRIORITY_AUDIO;
		init_check_first_systemtime = ref_pcr;
		tsync_get_demux_pcr(&init_check_first_demuxpcr);
	}

	if (first_vpts && !(tsync_pcr_inited_flag & complete_init_flag) &&
	    min_checkinpts != 0) {
		tsync_pcr_inited_flag |= TSYNC_PCR_INITCHECK_VPTS;
		if (tsync_pcr_debug & 0x02)
			pr_info("cur_pcr=%x first_vpts=%x\n",
				cur_pcr, first_vpts);
		if (cur_pcr && abs(cur_pcr - first_vpts)
			> PLAY_PCR_INVALID_THRESHOLD) {
			if (min_checkinpts == 0xffffffff)
				ref_pcr = first_vpts > 90 * 300 ?
				first_vpts - 90 * 300 : 0;
			else
				ref_pcr = min_checkinpts;
			tsync_use_demux_pcr = 0;
			if (tsync_pcr_debug & 0x01) {
				pr_info("check init.first_pcr=0x%x, first_apts=0x%x, ",
					first_pcr, first_apts);
				pr_info("first_vpts=0x%x, cur_pcr = 0x%x, checkin_vpts=0x%x, ",
					first_vpts, cur_pcr, cur_checkin_vpts);
				pr_info("checkin_apts=0x%x alevel=%d vlevel=%d\n",
				cur_checkin_apts, abuf_level, vbuf_level);
				pr_info("[%d]init by pcr. pcr=%x usepcr=%d\n",
					__LINE__, ref_pcr, tsync_pcr_usepcr);
			}
		} else if (cur_pcr) {
			ref_pcr = timestamp_pcrscr_get();
			tsync_use_demux_pcr = 1;
			if ((tsync_pcr_debug & 0x01) &&
			    tsync_get_demux_pcr(&tmp_pcr)) {
				pr_info("use the pcr from stream , the cur demux pcr is %x\n",
				tmp_pcr);
				pr_info("the timestamp pcr is %x\n",
					timestamp_pcrscr_get());
				pr_info("the timestamp video pts is %x\n",
					timestamp_firstvpts_get());
			}
		}
		ref_pcr = tsync_pcr_get_ref_pcr();
		pr_info("check vpts, get min checkin ref_pcr = 0x%x", ref_pcr);
		tsync_set_pcr_mode(0, ref_pcr);
		tsync_pcr_inited_mode = INIT_PRIORITY_VIDEO;
	}
}

static int speed_check_count;
static int normal_count;
static u8 tsync_process_checkspeed(void)
{
	u32 demuxpcr_diff = 0;
	u32 jiffies_diff = 0;
	u32 cur_jiffiestime = 0;
	u32 cur_demuxpcr = 0;
	u32 checkin_apts = 0;
	u32 checkin_vpts = 0;
	u32 checkout_vpts = 0;
	u32 checkout_apts = 0;

	if (tsync_pcr_first_jiffes == 0)
		return tsync_last_play_mode;

	cur_jiffiestime = jiffies_to_msecs(jiffies);
	tsync_get_demux_pcr(&cur_demuxpcr);

	if (timestamp_pcrscr_enable_state() == 0)
		return PLAY_MODE_NORMAL;

	jiffies_diff = cur_jiffiestime - tsync_pcr_first_jiffes;
	demuxpcr_diff = cur_demuxpcr - init_check_first_demuxpcr;
	demuxpcr_diff = demuxpcr_diff / 90;

	if (demuxpcr_diff > jiffies_diff) {
		u32 diff_delta = demuxpcr_diff - jiffies_diff;

		if (diff_delta > PLAY_MODE_THRESHOLD) {
			tsync_last_play_mode = PLAY_MODE_SPEED;
			if (tsync_pcr_debug & 0x01) {
				pr_info("pcr_diff %x ", demuxpcr_diff);
				pr_info("jiffies_diff %x ", jiffies_diff);
				pr_info("normal_count %d\n", normal_count);
			}
		} else {
			tsync_last_play_mode = PLAY_MODE_NORMAL;
		}

	} else if (demuxpcr_diff < jiffies_diff) {
		u32 diff_delta = jiffies_diff - demuxpcr_diff;

		if (diff_delta > PLAY_MODE_THRESHOLD) {
			tsync_last_play_mode = PLAY_MODE_SLOW;
			if (tsync_pcr_debug & 0x01) {
				pr_info("pcr_diff %x ", demuxpcr_diff);
				pr_info("jiffies_diff %x ", jiffies_diff);
				pr_info("normal_count %d\n", normal_count);
			}

		} else {
			tsync_last_play_mode = PLAY_MODE_NORMAL;
		}

	} else {
		tsync_last_play_mode = PLAY_MODE_NORMAL;
	}

	{
		checkin_apts = get_last_checkin_pts(PTS_TYPE_AUDIO);
		checkin_vpts = get_last_checkin_pts(PTS_TYPE_VIDEO);
		checkout_vpts = timestamp_vpts_get();
		checkout_apts = timestamp_apts_get();

		if (checkin_apts > checkout_apts) {
			if ((checkin_apts - checkout_apts) >
			    (PLAY_MODE_THRESHOLD * 90)) {
				tsync_last_play_mode = PLAY_MODE_SPEED;
				if (tsync_pcr_debug & 0x01)
					pr_info(" audio cache time %d normal_count %d\n",
						(checkin_apts - checkout_apts),
						normal_count);
			} else {
				tsync_last_play_mode = PLAY_MODE_NORMAL;
			}
		}

		if (checkin_vpts > checkout_vpts) {
			if ((checkin_vpts - checkout_vpts) >
			    (PLAY_MODE_THRESHOLD * 90)) {
				tsync_last_play_mode = PLAY_MODE_SPEED;
				if (tsync_pcr_debug & 0x01)
					pr_info(" video cached time %d normal_count %d\n",
						(checkin_vpts - checkout_vpts),
						normal_count);
			} else {
				tsync_last_play_mode = PLAY_MODE_NORMAL;
			}
		}
	}
	if (tsync_last_play_mode == PLAY_MODE_NORMAL)
		normal_count++;

	return tsync_last_play_mode;
}

static void tsync_process_demux_pcr_valid(u32 jiffis_diff)
{
	u32 ratio = 100;

	if (tsync_use_demux_pcr == 0 || tsync_demux_pcr_valid == 0)
		return;

	if (tsync_system_time_interval > 1000) {
		ratio = tsync_demux_pcr_time_interval * 100
			/ tsync_system_time_interval;
		pr_info("demux interval: %u, %u, ratio:%d.",
				tsync_demux_pcr_time_interval,
				tsync_system_time_interval, ratio);
		tsync_demux_pcr_time_interval = 0;
		tsync_system_time_interval = 0;
	}

	if (ratio > 150 && ratio < 400) {
		tsync_use_demux_pcr = 0;
		tsync_demux_pcr_valid = 0;
		timestamp_pcrscr_set(tsync_last_pcr_get +
				jiffis_diff * 90);
		timestamp_pcrscr_enable(1);
		pr_info("demux pcr not valid, use system time,pcr=0x%x, tsync_last_pcr_get=%x\n",
				timestamp_pcrscr_get(), tsync_last_pcr_get);
	}
}

static void tsync_adjust_system_clock(void)
{
	abuf_level = get_stream_buffer_level(1);
	abuf_size = get_stream_buffer_size(1);
	vbuf_level = get_stream_buffer_level(0);
	vbuf_size = get_stream_buffer_size(0);

	pr_info("adjust vlevel=%x,vsize=%x\n",
			vbuf_level, vbuf_size);
	pr_info("adjust alevel=%x asize=%x play_mode=%d\n",
			abuf_level, abuf_size, play_mode);

	if (((vbuf_level * 3 > vbuf_size * 2 && vbuf_size > 0) ||
		(abuf_level * 3 > abuf_size * 2 && abuf_size > 0)) &&
		play_mode != PLAY_MODE_FORCE_SPEED) {
		pr_info("adjust case 1.\n");
		play_mode = PLAY_MODE_FORCE_SPEED;
	} else if ((vbuf_level * 5 > vbuf_size * 4 && vbuf_size > 0) ||
			   (abuf_level * 5 > abuf_size * 4 && abuf_size > 0)) {
		u32 new_pcr = 0;

		play_mode = PLAY_MODE_FORCE_SPEED;
		new_pcr = timestamp_pcrscr_get() + 72000;	/* 90000*0.8 */
		timestamp_pcrscr_set(new_pcr);
		pr_info("adjust case 2.\n");
	}

	if (play_mode == PLAY_MODE_FORCE_SLOW) {
		if ((vbuf_level * 50 > vbuf_size &&
			abuf_level * 50 > abuf_size &&
			vbuf_size > 0 && abuf_size > 0) ||
			(vbuf_level * 20 > vbuf_size && vbuf_size > 0) ||
			(abuf_level * 20 > abuf_size && abuf_size > 0)) {
			play_mode = PLAY_MODE_NORMAL;
			pr_info("adjust case 3.\n");
		}
	} else if (play_mode == PLAY_MODE_FORCE_SPEED) {
		if ((vbuf_size > 0 && vbuf_level * 4 < vbuf_size &&
			abuf_size > 0 && abuf_level * 4 < abuf_size) ||
			(vbuf_size > 0 && vbuf_level * 4 < vbuf_size &&
			abuf_level == 0) || (abuf_size > 0 &&
			abuf_level * 4 < abuf_size &&
			vbuf_level == 0)) {
			play_mode = PLAY_MODE_NORMAL;
			pr_info("adjust case 4.\n");
		}
	}

	if (play_mode != PLAY_MODE_FORCE_SLOW &&
		play_mode != PLAY_MODE_FORCE_SPEED) {
		int min_cache_time = get_min_cache_delay();

		pr_info("adjust cache:%d.", min_cache_time);
		if (min_cache_time > tsync_pcr_max_delay_time) {
			if (play_mode != PLAY_MODE_SPEED) {
				play_mode = PLAY_MODE_SPEED;
				pr_info("adjust case 5.");
			}
		} else if (min_cache_time < tsync_pcr_min_delay_time &&
				   min_cache_time > 0) {
			if (play_mode != PLAY_MODE_SLOW) {
				play_mode = PLAY_MODE_SLOW;
				pr_info("adjust case 6.");
			}
		} else {
			if (tsync_pcr_down_delay_time <= min_cache_time &&
				min_cache_time <= tsync_pcr_up_delay_time &&
				play_mode != PLAY_MODE_NORMAL) {
				play_mode = PLAY_MODE_NORMAL;
				pr_info("adjust case 7.");
			}
		}
	}

	pr_info("adjust play_mode:%d,tsync_pcr_vpause_flag:%d.",
			play_mode, tsync_pcr_vpause_flag);

	if (!tsync_pcr_vpause_flag) {
		if (play_mode == PLAY_MODE_SLOW) {
			timestamp_pcrscr_set(timestamp_pcrscr_get() -
					tsync_pcr_recovery_span);
		} else if (play_mode == PLAY_MODE_FORCE_SLOW) {
			timestamp_pcrscr_set(timestamp_pcrscr_get() -
					FORCE_RECOVERY_SPAN);
		} else if (play_mode == PLAY_MODE_SPEED) {
			timestamp_pcrscr_set(timestamp_pcrscr_get() +
					tsync_pcr_recovery_span);
		} else if (play_mode == PLAY_MODE_FORCE_SPEED) {
			timestamp_pcrscr_set(timestamp_pcrscr_get() +
					FORCE_RECOVERY_SPAN);
		}
	}
}

static void tsync_process_discontinue(void)
{
	u32 cur_vpts = timestamp_vpts_get();
	u32 cur_tsdemuxpcr = 0;
	u32 ref_pcr  = 0, tsdemux_pcr_diff, tsdemux_pcr_diff_ms;
	u32 cur_pcr_jiffes = 0;
	u32 pcr_jeffes_diff_ms = 0;

	u32 cur_checkin_vpts = get_last_checkin_pts(PTS_TYPE_VIDEO);

	if (tsync_get_demux_pcr(&tsync_demux_last_pcr) &&
	    (tsync_demux_last_pcr == 0 ||
	    tsync_demux_last_pcr == 0xffffffff)) {
		tsync_last_pcr_get = timestamp_pcrscr_get();
		tsync_pcr_last_jiffes = jiffies_to_msecs(jiffies);
	}
	if (tsync_get_demux_pcr(&cur_tsdemuxpcr))
		cur_pcr_jiffes = jiffies_to_msecs(jiffies);
	else
		return;

	tsdemux_pcr_diff = abs(cur_tsdemuxpcr - tsync_demux_last_pcr);
	tsdemux_pcr_diff_ms = tsdemux_pcr_diff / 90;
	pcr_jeffes_diff_ms = cur_pcr_jiffes - tsync_pcr_last_jiffes;
	tsync_demux_pcr_time_interval += tsdemux_pcr_diff_ms;
	tsync_system_time_interval += pcr_jeffes_diff_ms;

	if (cur_tsdemuxpcr == 0xffffffff || cur_tsdemuxpcr == 0 ||
		pcr_jeffes_diff_ms == 0)
		return;

	if (tsync_enable_demuxpcr_check)
		tsync_process_demux_pcr_valid(pcr_jeffes_diff_ms);

	tsync_pcr_last_jiffes = jiffies_to_msecs(jiffies);
	tsync_last_pcr_get = timestamp_pcrscr_get();

	if (tsync_pcr_inited_mode != INIT_PRIORITY_PCR) {
		if ((tsync_pcr_tsdemuxpcr_discontinue & VIDEO_DISCONTINUE)
			== VIDEO_DISCONTINUE) {
			if (tsync_pcr_debug & 0x03) {
				pr_info("inited_mode=%d, cur_checkin_vpts %x\n",
					tsync_pcr_inited_mode,
					cur_checkin_vpts);
			}
			tsync_pcr_tsdemuxpcr_discontinue = 0;
		}
		return;
	}
	//tsdemux_pcr_diff = abs(cur_tsdemuxpcr - tsync_demux_last_pcr);
	if (tsync_demux_last_pcr != 0 && cur_tsdemuxpcr != 0 &&
		tsdemux_pcr_diff > tsync_pcr_discontinue_threshold) {
		tsync_demux_pcr_discontinue_count++;
		tsync_demux_pcr_continue_count = 0;
		tsync_demux_last_pcr = cur_tsdemuxpcr;
	} else {
		tsync_demux_pcr_discontinue_count = 0;
		tsync_demux_last_pcr = cur_tsdemuxpcr;
		tsync_demux_pcr_continue_count++;
		if (tsync_pcr_demux_pcr_used() == 0 &&
		    abs(cur_tsdemuxpcr - cur_vpts) <
			tsync_vpts_discontinuity_margin() &&
			tsync_demux_pcr_continue_count > 10 &&
			abs(cur_checkin_vpts - cur_vpts) <
			tsync_vpts_discontinuity_margin() &&
			tsync_demux_pcr_valid) {
			tsync_set_pcr_mode(1, cur_vpts);
			tsync_pcr_tsdemuxpcr_discontinue = 0;
			if (tsync_pcr_debug & 0x03) {
				pr_info("discontinue,resume checkvpts %x,curvpts %x\n",
					cur_checkin_vpts, cur_vpts);
			}
			return;
		}
	}
	if (tsync_demux_pcr_discontinue_count > 100) {
		if (tsync_pcr_debug & 0x03) {
			pr_info("PCR_DISCONTINUE, reset!,checkvpts %x,curvpts %x\n",
				cur_checkin_vpts, cur_vpts);
		}
		tsync_pcr_inited_mode = INIT_PRIORITY_VIDEO;
		cur_checkin_vpts -= tsync_pcr_ref_latency;
		tsync_set_pcr_mode(0, cur_checkin_vpts);
		if (tsync_pcr_debug & 0x03) {
			pr_info("pcr_dis_count=%d, cur_checkin_vpts %x\n",
				tsync_demux_pcr_discontinue_count,
				cur_checkin_vpts);
		}
		return;
	}
	if ((tsync_pcr_tsdemuxpcr_discontinue &
		(VIDEO_DISCONTINUE | PCR_DISCONTINUE)) ==
		VIDEO_DISCONTINUE) {
		if (abs(cur_checkin_vpts - cur_vpts) >
			tsync_vpts_discontinuity_margin()) {
			pr_info("v_dis cur_checkin_vpts=0x%x, cur_vpts=0x%x\n",
				cur_checkin_vpts, cur_vpts);
			tsync_set_pcr_mode(0, cur_vpts);
			tsync_pcr_tsdemuxpcr_discontinue = 0;
		} else if (!tsync_check_vpts_discontinuity(cur_vpts) &&
			tsync_pcr_demux_pcr_used() == 0 &&
			tsync_demux_pcr_valid) {
			tsync_set_pcr_mode(1, cur_vpts);
			tsync_pcr_tsdemuxpcr_discontinue = 0;
		} else if (tsync_pcr_demux_pcr_used() == 0 &&
			tsync_video_continue_count > 100) {
			tsync_pcr_tsdemuxpcr_discontinue = 0;
			tsync_set_pcr_mode(1, cur_checkin_vpts);
		} else {
			tsync_pcr_tsdemuxpcr_discontinue = 0;
			return;
		}
		if (tsync_pcr_debug & 0x03) {
			pr_info("VIDEO_DISCONTINUE checkvpts %x,curvpts %x\n",
				cur_checkin_vpts, cur_vpts);
		}
	} else if ((tsync_pcr_tsdemuxpcr_discontinue &
		   (VIDEO_DISCONTINUE | PCR_DISCONTINUE)) ==
		   PCR_DISCONTINUE) {
		if (tsync_use_demux_pcr == 1) {
			ref_pcr -= tsync_pcr_ref_latency;
			timestamp_pcrscr_set(ref_pcr);
			timestamp_pcrscr_enable(1);
			tsync_use_demux_pcr = 0;
		}
	} else if ((tsync_pcr_tsdemuxpcr_discontinue &
		   (VIDEO_DISCONTINUE | PCR_DISCONTINUE)) ==
		   (VIDEO_DISCONTINUE | PCR_DISCONTINUE)) {
		tsync_use_demux_pcr = 1;
		tsync_pcr_tsdemuxpcr_discontinue = 0;
	}
}

static void tsync_audio_underrun_check(u32 last_pcr_checkin_apts,
				u32 checkin_apts)
{
	if (last_pcr_checkin_apts == checkin_apts) {
		if (tsync_pcr_demux_pcr_used() == 1)
			tsync_audio_underrun_maxgap = 40;
		last_pcr_checkin_apts_count++;
		if (last_pcr_checkin_apts_count > tsync_audio_underrun_maxgap) {
			tsync_audio_underrun = 1;
			if (tsync_pcr_debug & 0x03)
				pr_info("audio underrun\n");
		}
	} else {
		last_pcr_checkin_apts_count = 0;
		tsync_audio_underrun = 0;
	}
}

void tsync_pcr_check_checinpts(void)
{
	u32 checkin_vpts = 0;
	u32 checkin_apts = 0;
	int max_gap;
	u32 cur_vpts = 0;
	u32 cur_apts = 0;
	int clear_audio_discontinue = 0;

	if (tsync_pcr_astart_flag == 1) {
		checkin_apts = get_last_checkin_pts(PTS_TYPE_AUDIO);
		if (checkin_apts != 0xffffffff &&
		    last_pcr_checkin_apts != 0 &&
		    last_pcr_checkin_apts != 0xffffffff) {
			if (abs(last_pcr_checkin_apts - checkin_apts)
				> AV_DISCONTINUE_THREDHOLD_MIN) {
				tsync_pcr_tsdemuxpcr_discontinue |=
				AUDIO_DISCONTINUE;
				tsync_audio_discontinue = 1;
				last_discontinue_checkin_apts = 0;
				tsync_audio_continue_count = 0;
				if (tsync_pcr_debug & 0x03) {
					pr_info("a_discontinue,last %x,cur %x\n",
						last_pcr_checkin_apts,
						checkin_apts);
				}
			}
			if (tsync_get_new_arch() &&
			    tsync_get_demux_pcrscr_valid())
				tsync_audio_underrun_check
							(last_pcr_checkin_apts,
							checkin_apts);
			if (tsync_audio_discontinue == 1 &&
				checkin_apts != 0xffffffff &&
				last_discontinue_checkin_apts == 0 &&
				checkin_apts > last_pcr_checkin_apts &&
				((checkin_apts - last_pcr_checkin_apts)
					< 500 * 90)) {
				last_discontinue_checkin_apts =
					last_pcr_checkin_apts;
				tsync_audio_continue_count = 1;
				if (tsync_pcr_debug & 0x03)
					pr_info("a_last:%x, cur:%x\n",
						last_pcr_checkin_apts,
						checkin_apts);
			}
			last_pcr_checkin_apts = checkin_apts;

			if (tsync_audio_continue_count)
				tsync_audio_continue_count++;
			if (tsync_audio_discontinue && video_pid_valid) {
				cur_vpts = timestamp_vpts_get();
				cur_apts = timestamp_apts_get();
				if (!video_jumped &&
				    last_discontinue_checkin_apts &&
				    abs(cur_vpts -
				    last_discontinue_checkin_apts) <
				    5000 * 90) {
					video_jumped = true;
					pr_info("v_continue:cur_vpts=%x\n",
						cur_vpts);
				}
				if (!audio_jumped &&
				    last_discontinue_checkin_apts &&
				    abs(cur_apts -
				    last_discontinue_checkin_apts) <
				    5000 * 90) {
					audio_jumped = true;
					pr_info("a_continue:cur_apts=%x\n",
						cur_apts);
				}

				if (video_jumped && audio_jumped) {
					pr_info("cur_vpts=%x,cur_apts=%x\n",
						cur_vpts, cur_apts);
					clear_audio_discontinue = 1;
				} else if (tsync_audio_continue_count > 500) {
					pr_info("a_continue timeout,clear\n");
					clear_audio_discontinue = 1;
				}
			}
			if (((!video_pid_valid &&
			      tsync_audio_continue_count > 50) ||
				(video_pid_valid && clear_audio_discontinue)) &&
				tsync_audio_discontinue &&
				last_discontinue_checkin_apts) {
				tsync_audio_discontinue = 0;
				video_jumped = false;
				audio_jumped = false;
				if (tsync_pcr_debug & 0x03)
					pr_info("a_continued,resumed\n");
			}
		} else {
			last_pcr_checkin_apts = checkin_apts;
		}
	}

	if (tsync_pcr_vstart_flag == 1) {
		checkin_vpts = get_last_checkin_pts(PTS_TYPE_VIDEO);
		if (checkin_vpts != 0xffffffff &&
		    last_pcr_checkin_vpts != 0 &&
		    last_pcr_checkin_vpts != 0xffffffff) {
			if (abs(last_pcr_checkin_vpts - checkin_vpts)
				> 2 * 90000) {
				tsync_pcr_tsdemuxpcr_discontinue |=
				VIDEO_DISCONTINUE;
				tsync_video_discontinue = 1;
				last_discontinue_checkin_vpts = 0;
				tsync_video_continue_count = 0;
				if (tsync_pcr_debug & 0x03) {
					pr_info("v_discontinue,last %x,cur %x\n",
						last_pcr_checkin_vpts,
						checkin_vpts);
				}
			}
			if (last_pcr_checkin_vpts == checkin_vpts) {
				if (tsync_pcr_demux_pcr_used() == 1)
					max_gap = 100;
				else
					max_gap = 300;
				last_pcr_checkin_vpts_count++;
				if (last_pcr_checkin_vpts_count > max_gap) {
					tsync_pcr_tsdemuxpcr_discontinue |=
					VIDEO_DISCONTINUE;
					tsync_video_discontinue = 1;
					last_discontinue_checkin_vpts = 0;
					tsync_video_continue_count = 0;
					last_pcr_checkin_vpts_count = 0;
					if (tsync_pcr_debug & 0x03)
						pr_info("v_discontinue,count\n");
				}
			} else {
				if (tsync_video_discontinue == 1 &&
				    checkin_vpts != 0xffffffff &&
				    last_discontinue_checkin_vpts == 0 &&
				    checkin_vpts > last_pcr_checkin_vpts &&
				    (abs(checkin_vpts
						- last_pcr_checkin_vpts)
						< 500 * 90)) {
					last_discontinue_checkin_vpts =
						last_pcr_checkin_vpts;
					tsync_video_continue_count = 1;
				}
				last_pcr_checkin_vpts = checkin_vpts;
				last_pcr_checkin_vpts_count = 0;
			}
			if (tsync_video_continue_count)
				tsync_video_continue_count++;
			if (tsync_video_continue_count > 15 &&
			    tsync_video_discontinue &&
			    last_discontinue_checkin_vpts) {
				tsync_video_discontinue = 0;
				if (tsync_pcr_debug & 0x03)
					pr_info("v_continued,resumed\n");
			}
		} else {
			last_pcr_checkin_vpts = checkin_vpts;
		}
	}
}
EXPORT_SYMBOL(tsync_pcr_pcrscr_set);

void tsync_pcr_avevent_locked(enum avevent_e event, u32 param)
{
	ulong flags;
	u32 first_apts;
	u32 cur_checkin_vpts, cur_checkin_apts;
	u32 ref_pcr;
	u32 cur_pcr;
	u8 complete_init_flag =
		TSYNC_PCR_INITCHECK_PCR | TSYNC_PCR_INITCHECK_VPTS |
		TSYNC_PCR_INITCHECK_APTS;
	spin_lock_irqsave(&tsync_pcr_lock, flags);

	switch (event) {
	case VIDEO_START:
		first_apts = timestamp_firstapts_get();
		cur_checkin_apts = get_last_checkin_pts(PTS_TYPE_AUDIO);
		if (tsync_pcr_vstart_flag == 0) {
			/* play_mode=PLAY_MODE_NORMAL; */
			tsync_pcr_first_video_frame_pts = param;
			pr_info("VIDEO_START! param=%x cur_pcr=%x\n", param,
				   timestamp_pcrscr_get());
		}
		if (!timestamp_firstvpts_get() && param)
			timestamp_firstvpts_set(param);
		tsync_set_av_state(0, 2);
		/*tsync_pcr_inited_mode = INIT_MODE_VIDEO;*/
		tsync_pcr_vstart_flag = 1;

		if (first_apts == 0 &&
			cur_checkin_apts == 0xffffffff &&
			tsync_get_audio_pid_valid()) {
		} else {
			tsync_get_demux_pcr(&cur_pcr);
			cur_checkin_vpts = get_last_checkin_pts(PTS_TYPE_VIDEO);
			if (cur_checkin_vpts != 0xffffffff &&
				(cur_pcr != 0xffffffff) &&
				(cur_pcr != 0) &&
				(abs(cur_pcr - cur_checkin_vpts) >
				PLAY_PCR_INVALID_THRESHOLD) &&
				(tsync_pcr_inited_flag & complete_init_flag) &&
				tsync_get_new_arch())
				tsync_set_pcr_mode(0, cur_checkin_vpts);
			else
				tsync_pcr_pcrscr_set();
		}
		break;

	case VIDEO_STOP:
		timestamp_pcrscr_enable(0);
		pr_info("VIDEO_STOP: reset pts !\n");
		timestamp_firstvpts_set(0);
		timestamp_firstapts_set(0);
		timestamp_checkin_firstvpts_set(0);
		timestamp_checkin_firstapts_set(0);
		timestamp_vpts_set(0);
		timestamp_vpts_set_u64(0);
		/* tsync_pcr_debug_pcrscr=100; */
		tsync_set_av_state(0, 3);
		tsync_pcr_vpause_flag = 0;
		tsync_pcr_vstart_flag = 0;
		tsync_pcr_inited_flag = 0;
		wait_pcr_count = 0;

		pr_info("wait_pcr_count = 0\n");
		tsync_pcr_tsdemuxpcr_discontinue = 0;
		tsync_pcr_discontinue_point = 0;
		tsync_pcr_discontinue_local_point = 0;
		tsync_pcr_discontinue_waited = 0;
		tsync_pcr_first_video_frame_pts = 0;
		video_pid_valid = false;
		video_jumped = false;
		tsync_pcr_tsdemux_startpcr = 0;
		play_mode = PLAY_MODE_NORMAL;
		first_time_record = div64_u64((u64)jiffies * TIME_UNIT90K, HZ);
		pr_info("update time record\n");
		tsync_reset();
		pr_info("video stop!\n");
		break;

	case VIDEO_TSTAMP_DISCONTINUITY:{
		unsigned int systime;
		unsigned int demux_pcr;

		cur_checkin_vpts = get_last_checkin_pts(PTS_TYPE_VIDEO);
		if (tsync_pcr_debug & 0x03)
			pr_info("VIDEO_TSTAMP_DISCONTINUITY param:0x%x,in:%x\n",
				param, cur_checkin_vpts);
		if (tsync_pcr_inited_mode != INIT_PRIORITY_PCR) {
			if (cur_checkin_vpts > param &&
			    (cur_checkin_vpts - param < 90000)) {
				ref_pcr = param - tsync_pcr_ref_latency;
				timestamp_pcrscr_set(ref_pcr);
			} else {
				timestamp_pcrscr_set(param);
			}
			if (tsync_pcr_debug & 0x03) {
				pr_info("vdiscontinue param %x,pcrsrc %x\n",
					param, timestamp_pcrscr_get());
			}
		} else if (tsync_get_demux_pcrscr_valid() &&
			tsync_check_vpts_discontinuity(param)) {
			if (tsync_pcr_debug & 0x03) {
				pr_info("vdiscontinue %d,param %x,pcrsrc %x\n",
					tsync_use_demux_pcr, param,
					timestamp_pcrscr_get());
			}
			if (tsync_pcr_demux_pcr_used() == 1) {
				systime = timestamp_pcrscr_get() +
					timestamp_get_pcrlatency();
				if (tsync_pcr_debug & 0x03) {
					pr_info("used sys=0x%x, param=0x%x\n",
						systime, param);
				}
				if (systime + 900000 < param)
					tsync_set_pcr_mode(0, param);
				else
					timestamp_pcrscr_set(param);
			} else {
				systime = timestamp_pcrscr_get();
				tsync_get_demux_pcr(&demux_pcr);
				if (tsync_pcr_debug & 0x03) {
					pr_info("sys=%x, param=%x, pcr=%x\n",
						systime, param, demux_pcr);
				}
				if ((demux_pcr + 900000 > param) &&
				     tsync_demux_pcr_valid)
					tsync_set_pcr_mode(1, param);
				else
					timestamp_pcrscr_set(param);
			}
		}
		timestamp_vpts_set(param);
		break;
	}
	case AUDIO_TSTAMP_DISCONTINUITY:{
		tsync_pcr_tsdemuxpcr_discontinue |= AUDIO_DISCONTINUE;
			timestamp_apts_set(param);
			param = timestamp_pcrscr_get();
			timestamp_apts_set(param);
	}
	break;

	case AUDIO_PRE_START:
		timestamp_apts_start(0);
		tsync_pcr_astart_flag = 0;
		pr_info("audio prestart!\n");
		break;

	case AUDIO_START:
		tsync_set_av_state(1, AUDIO_START);
		timestamp_apts_set(param);
		timestamp_firstapts_set(param);
		timestamp_apts_enable(1);
		timestamp_apts_start(1);
		tsync_pcr_first_audio_frame_pts = param;
		tsync_pcr_astart_flag = 1;
		tsync_pcr_apause_flag = 0;
		pr_info("AUDIO_START!timestamp_apts_set =%x. cur_pcr %x\n",
			param, timestamp_pcrscr_get());
		break;

	case AUDIO_RESUME:
		timestamp_apts_enable(1);
		tsync_pcr_apause_flag = 0;
		pr_info("audio resume!\n");
		break;

	case AUDIO_STOP:
		tsync_set_av_state(1, AUDIO_STOP);
		timestamp_apts_enable(0);
		timestamp_apts_set(-1);
		timestamp_firstapts_set(0);
		timestamp_checkin_firstvpts_set(0);
		timestamp_checkin_firstapts_set(0);
		timestamp_apts_start(0);
		audio_jumped = false;
		tsync_pcr_astart_flag = 0;
		tsync_pcr_apause_flag = 0;
		tsync_pcr_first_audio_frame_pts = 0;
		pr_info("AUDIO_STOP!\n");
		break;

	case AUDIO_PAUSE:
		timestamp_apts_enable(0);
		tsync_pcr_apause_flag = 1;
		pr_info("audio pause!\n");
		break;

	case VIDEO_PAUSE:
		if (param == 1) {
			timestamp_pcrscr_enable(0);
			tsync_pcr_vpause_flag = 1;
			pr_info("video pause!\n");
		} else {
			timestamp_pcrscr_enable(1);
			tsync_pcr_vpause_flag = 0;
			pr_info("video resume\n");
		}
		break;

	default:
		break;
	}
	switch (event) {
	case VIDEO_START:
	case AUDIO_START:
	case AUDIO_RESUME:
		/*amvdev_resume();*//*DEBUG_TMP*/
		break;
	case VIDEO_STOP:
	case AUDIO_STOP:
	case AUDIO_PAUSE:
		/*amvdev_pause();*//*DEBUG_TMP*/
		break;
	case VIDEO_PAUSE:
		/*if (tsync_pcr_vpause_flag)*/
			/*amvdev_pause();*//*DEBUG_TMP*/
		/*else*/
			/*amvdev_resume();*//*DEBUG_TMP*/
		break;
	default:
		break;
	}

	spin_unlock_irqrestore(&tsync_pcr_lock, flags);
}
EXPORT_SYMBOL(tsync_pcr_avevent_locked);

/* timer to check the system with the referrence time in ts stream. */
static unsigned long tsync_pcr_check(void)
{
	u32 tsdemux_pcr = 0;
	u32 tsdemux_pcr_diff = 0;
	int need_recovery = 1;
	unsigned long res = 1;
	s64 last_checkin_vpts = 0;
	s64 last_checkin_apts = 0;
	s64 last_cur_pcr = 0;
	s64 last_checkin_minpts = 0;
	u32 cur_apts = 0;
	u32 cur_vpts = 0;
	u32 tmp_pcr = 0;

	video_pid_valid = false;
	if (tsync_get_mode() != TSYNC_MODE_PCRMASTER ||
	    tsync_pcr_freerun_mode == 1)
		return res;

	if (tsync_pcr_first_jiffes != 0) {
		tsync_pcr_jiffes_diff = jiffies_to_msecs(jiffies)
		    - tsync_pcr_first_jiffes;
		if (tsync_get_demux_pcr(&tmp_pcr))
			tysnc_pcr_systime_diff =
			tmp_pcr - tsync_pcr_first_systime;
	}

	video_pid_valid = tsync_get_video_pid_valid();

	tsync_pcr_check_checinpts();
	tsync_get_demux_pcr(&tsdemux_pcr);
	last_checkin_vpts = (u32)get_last_checkin_pts(PTS_TYPE_VIDEO);
	last_checkin_apts = (u32)get_last_checkin_pts(PTS_TYPE_AUDIO);

	if (tsync_pcr_usepcr == 1) {
		/* To monitor the pcr discontinue */
		tsdemux_pcr_diff = abs(tsdemux_pcr - tsync_pcr_last_tsdemuxpcr);
		if (tsync_pcr_last_tsdemuxpcr != 0 && tsdemux_pcr != 0 &&
		    tsdemux_pcr_diff > tsync_pcr_discontinue_threshold &&
		    tsync_pcr_inited_flag >= TSYNC_PCR_INITCHECK_PCR) {
			u32 video_delayed = 0;

			tsync_pcr_tsdemuxpcr_discontinue |= PCR_DISCONTINUE;
			video_delayed = tsync_pcr_vstream_delayed();
			if (TIME_UNIT90K * 2 <= video_delayed &&
			    video_delayed <= TIME_UNIT90K * 4) {
				tsync_pcr_discontinue_waited =
					video_delayed + TIME_UNIT90K;
			} else if (TIME_UNIT90K * 2 > video_delayed) {
				tsync_pcr_discontinue_waited = TIME_UNIT90K * 3;
			} else {
				tsync_pcr_discontinue_waited = TIME_UNIT90K * 5;
			}
			if (tsync_pcr_debug & 0x02) {
				pr_info("[%s] refpcr_discontinue.\n", __func__);
				pr_info("tsdemux_pcr_diff=%x, last refpcr=%x,\n",
					tsdemux_pcr_diff,
					tsync_pcr_last_tsdemuxpcr);
			pr_info("repcr=%x,waited=%x\n",
				tsdemux_pcr,
				tsync_pcr_discontinue_waited);
			pr_info("last checkin vpts:%d\n",
				(u32)get_last_checkin_pts(PTS_TYPE_VIDEO));
			pr_info("last checkin apts:%d\n",
				(u32)get_last_checkin_pts(PTS_TYPE_AUDIO));
			}
			tsync_pcr_discontinue_local_point =
				timestamp_pcrscr_get();
			tsync_pcr_discontinue_point =
				tsdemux_pcr - tsync_pcr_ref_latency;
			need_recovery = 0;
		} else if (tsync_pcr_tsdemuxpcr_discontinue & PCR_DISCONTINUE) {
			/* to pause the pcr check */
			if ((abs
				(timestamp_pcrscr_get()
				- tsync_pcr_discontinue_local_point))
				> tsync_pcr_discontinue_waited){
				if (tsync_pcr_debug & 0x02) {
					u32 temp = 0;

					temp =
					tsync_pcr_discontinue_local_point;
					pr_info("[%s] audio or video",
						__func__);
					pr_info("discontinue didn't happen, waited=%x\n",
						tsync_pcr_discontinue_waited);
					pr_info(" timestamp_pcrscr_get() =%x\n",
						timestamp_pcrscr_get());
					pr_info("tsync_pcr_discontinue_local_point =%x\n",
						temp);
				}
				/* the v-discontinue did'n happen */
				tsync_pcr_tsdemuxpcr_discontinue = 0;
				tsync_pcr_discontinue_point = 0;
				tsync_pcr_discontinue_local_point = 0;
				tsync_pcr_discontinue_waited = 0;
			}
			need_recovery = 0;
		}
		tsync_pcr_last_tsdemuxpcr = tsdemux_pcr;
	}
	if ((!(tsync_pcr_inited_flag & TSYNC_PCR_INITCHECK_VPTS)) &&
	    (!(tsync_pcr_inited_flag & TSYNC_PCR_INITCHECK_PCR)) &&
	    (!(tsync_pcr_inited_flag & TSYNC_PCR_INITCHECK_APTS))) {
		u64 cur_system_time =
			div64_u64((u64)jiffies * TIME_UNIT90K, HZ);
		if (last_checkin_vpts == 0xffffffff &&
			last_checkin_apts == 0xffffffff) {
			first_time_record = cur_system_time;
			return res;
		}
		if (cur_system_time - first_time_record <
			tsync_pcr_latency_value &&
			video_pid_valid) {
			if (tsync_pcr_vstart_flag == 1 &&
			    tsync_get_audio_pid_valid() &&
			    last_checkin_apts != 0xffffffff) {
				tsync_pcr_pcrscr_set();
			}
		} else {
			if (video_pid_valid)
				tsync_pcr_inited_mode = INIT_PRIORITY_VIDEO;
			else
				tsync_pcr_inited_mode = INIT_PRIORITY_AUDIO;
			tsync_pcr_pcrscr_set();
		}
		return res;
	}

	abuf_level = get_stream_buffer_level(1);
	abuf_size = get_stream_buffer_size(1);
	vbuf_level = get_stream_buffer_level(0);
	vbuf_size = get_stream_buffer_size(0);

	/*
	 **********************************************************
	 * On S905/S905X Platform, when we play avs video file on dtv
	 *which contian long cabac   some time it will cause amstream
	 *video buffer enter an error status. vbuf_level will over  vbuf_size,
	 *this will cause a lot of print in this function. And that will
	 *prevent reset ts monitor work thread from getting cpu control
	 *and to reset ts module. So we add the following  code to avoid
	 *this case.
	 *
	 *       Rong.Zhang@amlogic.com    2016-08-10
	 **********************************************************
	 */
	if (abuf_level > abuf_size) {
		if (!abuf_fatal_error)
			abuf_fatal_error = 1;
		return res;
	}

	if (vbuf_level > vbuf_size) {
		if (!vbuf_fatal_error)
			vbuf_fatal_error = 1;
		return res;
	}

	last_cur_pcr = timestamp_pcrscr_get();
	last_checkin_minpts = tsync_pcr_get_min_checkinpts();
	cur_apts = timestamp_apts_get();
	cur_vpts = timestamp_vpts_get();

	tsync_process_discontinue();
	if (tsync_use_demux_pcr || tsync_demux_pcr_valid)
		play_mode = tsync_process_checkspeed();
	else if (tsync_enable_bufferlevel_tune)
		tsync_adjust_system_clock();

	return res;
}

static void tsync_pcr_check_timer_func(struct timer_list *timer)
{
	ulong flags;

	spin_lock_irqsave(&tsync_pcr_lock, flags);
	tsync_pcr_check();
	spin_unlock_irqrestore(&tsync_pcr_lock, flags);

	tsync_pcr_check_timer.expires =
	(unsigned long)(jiffies + TEN_MS_INTERVAL);

	add_timer(&tsync_pcr_check_timer);
}

static void tsync_pcr_param_reset(void)
{
	tsync_pcr_tsdemux_startpcr = 0;

	tsync_pcr_vpause_flag = 0;
	tsync_pcr_apause_flag = 0;
	tsync_pcr_vstart_flag = 0;
	tsync_pcr_astart_flag = 0;
	tsync_pcr_inited_flag = 0;
	wait_pcr_count = 0;
	tsync_pcr_reset_flag = 0;
	tsync_pcr_asynccheck_cnt = 0;
	tsync_pcr_vsynccheck_cnt = 0;
	tsync_pcr_last_tsdemuxpcr = 0;
	tsync_pcr_discontinue_local_point = 0;
	tsync_pcr_discontinue_point = 0;
	/* the time waited the v-discontinue to happen */
	tsync_pcr_discontinue_waited = 0;
	tsync_pcr_tsdemuxpcr_discontinue = 0;	/* the boolean value */

	abuf_level = 0;
	abuf_size = 0;
	vbuf_level = 0;
	vbuf_size = 0;
	play_mode = PLAY_MODE_NORMAL;
	tsync_pcr_started = 0;
	tsync_reset();
	/*tsync_pcr_inited_mode = INIT_MODE_VIDEO; */
	tsync_pcr_stream_delta = 0;

	last_pcr_checkin_apts = 0;
	last_pcr_checkin_vpts = 0;
	last_pcr_checkin_apts_count = 0;
	last_pcr_checkin_vpts_count = 0;
	tsync_last_play_mode = PLAY_MODE_NORMAL;
	tsync_pcr_first_jiffes = 0;
	tsync_pcr_first_systime = 0;
	speed_check_count = 0;
	tsync_audio_discontinue = 0;
	tsync_audio_continue_count = 0;
	tsync_video_continue_count = 0;
	last_discontinue_checkin_apts = 0;
	tsync_vpts_adjust = 0;
	last_discontinue_checkin_vpts = 0;
	tsync_video_discontinue = 0;
	video_pid_valid = false;
	video_jumped = false;
	audio_jumped = false;
}

int tsync_pcr_set_apts(unsigned int pts)
{
	timestamp_apts_set(pts);
	/* pr_info("[tsync_pcr_set_apts]set apts=%x",pts); */
	return 0;
}
EXPORT_SYMBOL(tsync_pcr_set_apts);

int tsync_get_vpts_adjust(void)
{
	return tsync_vpts_adjust * 90;
}
EXPORT_SYMBOL(tsync_get_vpts_adjust);

int tsync_pcr_start(void)
{
	if (tsync_pcr_started)
		return 0;

	timestamp_pcrscr_enable(0);
	timestamp_pcrscr_set(0);

	tsync_pcr_param_reset();
	if (tsync_get_mode() == TSYNC_MODE_PCRMASTER) {
		pr_info("[tsync_pcr_start]PCRMASTER started success.\n");
		timer_setup(&tsync_pcr_check_timer, tsync_pcr_check_timer_func, 0);
		tsync_pcr_check_timer.expires = (unsigned long)jiffies;

		first_time_record = div64_u64((u64)jiffies * TIME_UNIT90K, HZ);
		tsync_pcr_started = 1;
		if ((tsync_get_demux_pcrscr_valid() == 0) ||
		    tsync_disable_demux_pcr == 1) {
			tsync_use_demux_pcr = 0;
			tsync_pcr_inited_mode = INIT_PRIORITY_AUDIO;
		} else {
			tsync_use_demux_pcr = 1;
			tsync_pcr_inited_mode = INIT_PRIORITY_PCR;
		}
		tsync_pcr_read_cnt = 0;
		add_timer(&tsync_pcr_check_timer);
		pr_info("start:inited_mode=%d,tsync_use_demux_pcr=%d.\n",
				tsync_pcr_inited_mode, tsync_use_demux_pcr);
	}
	abuf_fatal_error = 0;
	vbuf_fatal_error = 0;
	tsync_demux_last_pcr = 0;
	tsync_demux_pcr_discontinue_count = 0;
	tsync_demux_pcr_continue_count = 0;
	tsync_audio_state = 1;
	tsync_video_state = 1;
	tsync_demux_pcr_valid = 1;
	tsync_demux_pcr_time_interval = 0;
	tsync_system_time_interval = 0;
	return 0;
}
EXPORT_SYMBOL(tsync_pcr_start);

int tsync_pcr_demux_pcr_used(void)
{
	return tsync_use_demux_pcr;
}
EXPORT_SYMBOL(tsync_pcr_demux_pcr_used);

void tsync_pcr_mode_reinit(u8 type)
{
	if (type == PTS_TYPE_AUDIO) {
		tsync_audio_state = 0;
		audio_jumped = false;
		tsync_pcr_apause_flag = 0;
		tsync_pcr_astart_flag = 0;
		last_pcr_checkin_apts = 0;
		tsync_audio_discontinue = 0;
	}
}
EXPORT_SYMBOL(tsync_pcr_mode_reinit);

void tsync_pcr_stop(void)
{
	if (tsync_pcr_started == 1) {
		del_timer_sync(&tsync_pcr_check_timer);
		pr_info("[%s]PCRMASTER stop success.\n", __func__);
	}
	tsync_pcr_freerun_mode = 0;
	tsync_pcr_started = 0;
	tsync_audio_state = 0;
	tsync_video_state = 0;
	video_jumped = false;
	audio_jumped = false;
	timestamp_checkin_firstvpts_set(0);
	timestamp_checkin_firstapts_set(0);
	timestamp_vpts_set(0);
	timestamp_vpts_set_u64(0);
	timestamp_pcrscr_set(0);
	tsync_reset();
	tsync_demux_pcr_valid = 1;
}
EXPORT_SYMBOL(tsync_pcr_stop);

/* ------------------------------------------------------------------------ */
/* define of tsync pcr class node */

static ssize_t play_mode_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", play_mode);
}

static ssize_t pcr_diff_show(struct class *class,
			     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d %d %d %s\n", tsync_pcr_jiffes_diff * 90,
		       tysnc_pcr_systime_diff,
		       tsync_pcr_jiffes_diff - tysnc_pcr_systime_diff / 90,
		       "ms");
}

static ssize_t tsync_pcr_discontinue_point_show(struct class *class,
						struct class_attribute *attr,
						char *buf)
{
	pr_info("[%s:%d] tsync_pcr_discontinue_point:%x, HZ:%x,\n",
		__func__, __LINE__, tsync_pcr_discontinue_point, HZ);
	return sprintf(buf, "0x%x\n", tsync_pcr_discontinue_point);
}

static ssize_t tsync_pcr_discontinue_point_store(struct class *class,
						 struct class_attribute *attr,
						 const char *buf, size_t size)
{
	unsigned int pts;
	ssize_t r;

	/*r = sscanf(buf, "0x%x", &pts);*/
	r = kstrtoint(buf, 0, &pts);

	if (r != 0)
		return -EINVAL;

	tsync_pcr_discontinue_point = pts;
	pr_info("[%s:%d] tsync_pcr_discontinue_point:%x,\n", __func__,
		__LINE__, tsync_pcr_discontinue_point);

	return size;
}

static ssize_t audio_resample_type_store(struct class *class,
					 struct class_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned int type;
	ssize_t r;

	/*r = sscanf(buf, "%d", &type);*/
	r = kstrtoint(buf, 0, &type);

	if (r != 0)
		return -EINVAL;

	if (type == RESAMPLE_DOWN_FORCE_PCR_SLOW) {
		play_mode = PLAY_MODE_SLOW;
		pr_info("[%s:%d] Audio to FORCE_PCR_SLOW\n", __func__,
			__LINE__);
	}
	return size;
}

static ssize_t tsync_pcr_mode_show(struct class *cla,
					   struct class_attribute *attr,
					   char *buf)
{
	return sprintf(buf, "%d\n", tsync_use_demux_pcr);
}

static ssize_t tsync_audio_mode_show(struct class *cla,
				     struct class_attribute *attr,
				     char *buf)
{
	return sprintf(buf, "%d\n", tsync_audio_mode);
}

static ssize_t tsync_audio_state_show(struct class *cla,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", tsync_audio_state);
}

static ssize_t tsync_video_state_show(struct class *cla,
				      struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", tsync_video_state);
}

static ssize_t tsync_audio_mode_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &tsync_audio_mode);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t tsync_pcr_latency_value_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", tsync_pcr_latency_value);
}

static ssize_t tsync_pcr_latency_value_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &tsync_pcr_latency_value);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t tsync_disable_demux_pcr_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", tsync_disable_demux_pcr);
}

static ssize_t tsync_disable_demux_pcr_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &tsync_disable_demux_pcr);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t tsync_vpts_adjust_store(struct class *cla,
				       struct class_attribute *attr,
				       const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &tsync_vpts_adjust);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t tsync_vpts_adjust_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", tsync_vpts_adjust);
}

static ssize_t tsync_audio_level_show(struct class *cla,
				      struct class_attribute *attr,
				      char *buf)
{
	u32 audio_level;

	audio_level = tsync_audio_discontinue & 0xff;
	return sprintf(buf, "%d\n", audio_level);
}

static ssize_t tsync_audio_underrun_maxgap_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", tsync_audio_underrun_maxgap);
}

static ssize_t tsync_audio_underrun_maxgap_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &tsync_audio_underrun_maxgap);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t tsync_audio_underrun_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", tsync_audio_underrun);
}

static ssize_t tsync_vdiscontinue_show(struct class *cla,
				       struct class_attribute *attr,
				       char *buf)
{
	u32 vdiscontinue;

	vdiscontinue = tsync_video_discontinue & 0xff;
	return sprintf(buf, "%d\n", vdiscontinue);
}

static ssize_t tsync_pcr_apts_diff_show(struct class *cla,
					struct class_attribute *attr,
					char *buf)
{
	u32 t1, apts_diff;

	t1 = timestamp_get_pts_latency(1);
	apts_diff = t1 & 0xffff;
	return sprintf(buf, "%d\n", apts_diff);
}

static ssize_t
tsync_last_discontinue_checkin_apts_show(struct class *cla,
					 struct class_attribute *attr,
					 char *buf)
{
	return sprintf(buf, "0x%x\n", last_discontinue_checkin_apts);
}

static ssize_t tsync_pcr_reset_flag_show(struct class *class,
					 struct class_attribute *attr,
					 char *buf)
{
	ssize_t size = 0;

	size = sprintf(buf, "%d\n", tsync_pcr_reset_flag);
	tsync_pcr_reset_flag = 0;
	return size;
}

static ssize_t tsync_pcr_apause_flag_show(struct class *class,
					  struct class_attribute *attr,
					  char *buf)
{
	return sprintf(buf, "%d\n", tsync_pcr_apause_flag);
}

static ssize_t tsync_pcr_recovery_span_show(struct class *cla,
					    struct class_attribute *attr,
					    char *buf)
{
	return sprintf(buf, "%d\n", tsync_pcr_recovery_span);
}

static ssize_t tsync_pcr_recovery_span_store(struct class *cla,
					     struct class_attribute *attr,
					     const char *buf, size_t count)
{
	size_t r;

	/*r = sscanf(buf, "%d", &tsync_pcr_recovery_span);*/
	r = kstrtoint(buf, 0, &tsync_pcr_recovery_span);

	pr_info("%s(%d)\n", __func__, tsync_pcr_recovery_span);

	if (r != 0)
		return -EINVAL;

	return count;
}

/* add it for dolby av sync 20160126 */
static ssize_t tsync_apts_adj_value_show(struct class *cla,
					 struct class_attribute *attr,
					 char *buf)
{
	return sprintf(buf, "%d\n", tsync_apts_adj_value);
}

static ssize_t tsync_apts_adj_value_store(struct class *cla,
					  struct class_attribute *attr,
					  const char *buf, size_t count)
{
	size_t r;

	/*r = sscanf(buf, "%d", &tsync_apts_adj_value);*/
	r = kstrtoint(buf, 0, &tsync_apts_adj_value);

	pr_info("%s(%d)\n", __func__, tsync_apts_adj_value);

	if (r != 0)
		return -EINVAL;

	return count;
}

static ssize_t tsync_pcr_adj_value_show(struct class *cla,
					struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", tsync_pcr_adj_value);
}

static ssize_t tsync_pcr_adj_value_store(struct class *cla,
					 struct class_attribute *attr,
					 const char *buf, size_t count)
{
	size_t r;

	/*r = sscanf(buf, "%d", &tsync_pcr_adj_value);*/
	r = kstrtoint(buf, 0, &tsync_pcr_adj_value);

	pr_info("%s(%d)\n", __func__, tsync_pcr_adj_value);

	if (r != 0)
		return -EINVAL;

	return count;
}

static ssize_t tsync_pcr_discontinue_show(struct class *cla,
					  struct class_attribute *attr,
					  char *buf)
{
	return sprintf(buf, "0x%x\n", tsync_pcr_tsdemuxpcr_discontinue);
}

static ssize_t tsync_pcr_debug_store(struct class *cla,
				     struct class_attribute *attr,
				     const char *buf, size_t count)
{
	size_t r;

	/*r = sscanf(buf, "%d", &tsync_pcr_debug);*/
	r = kstrtoint(buf, 0, &tsync_pcr_debug);
	pr_info("%s(%d)\n", __func__, tsync_pcr_debug);
	if (r != 0)
		return -EINVAL;

	return count;
}

static ssize_t tsync_pcr_debug_show(struct class *cla,
				    struct class_attribute *attr,
				    char *buf)
{
	return sprintf(buf, "%d\n", tsync_pcr_debug);
}

static ssize_t tsync_vstream_cache_show(struct class *class,
					struct class_attribute *attr, char *buf)
{
	unsigned int cur_vpts = timestamp_vpts_get();
	unsigned int last_checkin_vpts = get_last_checkin_pts(PTS_TYPE_VIDEO);
	unsigned int diff = 0;

	if (last_checkin_vpts > cur_vpts)
		diff = last_checkin_vpts - cur_vpts;
	else
		diff = cur_vpts - last_checkin_vpts;
	return sprintf(buf, "%d\n", diff);
}

static ssize_t tsync_astream_cache_show(struct class *class,
					struct class_attribute *attr,
					char *buf)
{
	unsigned int cur_apts = timestamp_apts_get();
	unsigned int last_checkin_apts = get_last_checkin_pts(PTS_TYPE_AUDIO);
	unsigned int diff = 0;

	if (last_checkin_apts > cur_apts)
		diff = last_checkin_apts - cur_apts;
	else
		diff = cur_apts - last_checkin_apts;

	return sprintf(buf, "%d\n", diff);
}

static ssize_t tsync_enable_demuxpcr_check_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", tsync_enable_demuxpcr_check);
}

static ssize_t tsync_enable_demuxpcr_check_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &tsync_enable_demuxpcr_check);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t tsync_enable_bufferlevel_tune_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", tsync_enable_bufferlevel_tune);
}

static ssize_t tsync_enable_bufferlevel_tune_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &tsync_enable_bufferlevel_tune);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t tsync_pcr_inited_flag_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", tsync_pcr_inited_flag);
}

static ssize_t tsync_pcr_ref_latency_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", tsync_pcr_ref_latency);
}

static ssize_t tsync_pcr_ref_latency_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;

	r = kstrtoint(buf, 0, &tsync_pcr_ref_latency);
	if (r != 0)
		return -EINVAL;
	return count;
}
/* ----------------------------------------------------------------------- */
/* define of tsync pcr module */

static CLASS_ATTR_RO(play_mode);
static CLASS_ATTR_RW(tsync_pcr_discontinue_point);
static CLASS_ATTR_WO(audio_resample_type);
static CLASS_ATTR_RO(tsync_pcr_mode);
static CLASS_ATTR_RW(tsync_audio_mode);
static CLASS_ATTR_RO(tsync_audio_state);
static CLASS_ATTR_RO(tsync_video_state);
static CLASS_ATTR_RW(tsync_disable_demux_pcr);
static CLASS_ATTR_RW(tsync_vpts_adjust);
static CLASS_ATTR_RO(tsync_audio_level);
static CLASS_ATTR_RO(tsync_vdiscontinue);
static CLASS_ATTR_RO(tsync_pcr_apts_diff);
static CLASS_ATTR_RO(tsync_last_discontinue_checkin_apts);
static CLASS_ATTR_RO(tsync_pcr_reset_flag);
static CLASS_ATTR_RO(tsync_pcr_apause_flag);
static CLASS_ATTR_RW(tsync_pcr_recovery_span);
static CLASS_ATTR_RW(tsync_apts_adj_value);
static CLASS_ATTR_RW(tsync_pcr_adj_value);
static CLASS_ATTR_RW(tsync_pcr_debug);
static CLASS_ATTR_RO(tsync_pcr_discontinue);
static CLASS_ATTR_RO(pcr_diff);
static CLASS_ATTR_RO(tsync_vstream_cache);
static CLASS_ATTR_RO(tsync_astream_cache);
static CLASS_ATTR_RW(tsync_enable_demuxpcr_check);
static CLASS_ATTR_RW(tsync_enable_bufferlevel_tune);
static CLASS_ATTR_RO(tsync_pcr_inited_flag);
static CLASS_ATTR_RW(tsync_pcr_ref_latency);
static CLASS_ATTR_RW(tsync_pcr_latency_value);
static CLASS_ATTR_RO(tsync_audio_underrun);
static CLASS_ATTR_RW(tsync_audio_underrun_maxgap);

static struct attribute *tsync_pcr_class_attrs[] = {
	&class_attr_play_mode.attr,
	&class_attr_tsync_pcr_discontinue_point.attr,
	&class_attr_audio_resample_type.attr,
	&class_attr_tsync_pcr_mode.attr,
	&class_attr_tsync_audio_mode.attr,
	&class_attr_tsync_audio_state.attr,
	&class_attr_tsync_video_state.attr,
	&class_attr_tsync_disable_demux_pcr.attr,
	&class_attr_tsync_vpts_adjust.attr,
	&class_attr_tsync_audio_level.attr,
	&class_attr_tsync_vdiscontinue.attr,
	&class_attr_tsync_pcr_apts_diff.attr,
	&class_attr_tsync_last_discontinue_checkin_apts.attr,
	&class_attr_tsync_pcr_reset_flag.attr,
	&class_attr_tsync_pcr_apause_flag.attr,
	&class_attr_tsync_pcr_recovery_span.attr,
	&class_attr_tsync_apts_adj_value.attr,
	&class_attr_tsync_pcr_adj_value.attr,
	&class_attr_tsync_pcr_debug.attr,
	&class_attr_tsync_pcr_discontinue.attr,
	&class_attr_pcr_diff.attr,
	&class_attr_tsync_vstream_cache.attr,
	&class_attr_tsync_astream_cache.attr,
	&class_attr_tsync_enable_demuxpcr_check.attr,
	&class_attr_tsync_enable_bufferlevel_tune.attr,
	&class_attr_tsync_pcr_inited_flag.attr,
	&class_attr_tsync_pcr_ref_latency.attr,
	&class_attr_tsync_pcr_latency_value.attr,
	&class_attr_tsync_audio_underrun.attr,
	&class_attr_tsync_audio_underrun_maxgap.attr,
	NULL
};

ATTRIBUTE_GROUPS(tsync_pcr_class);

static struct class tsync_pcr_class = {
	.name = "tsync_pcr",
	.class_groups = tsync_pcr_class_groups,
};

int __init tsync_pcr_init(void)
{
	int r;

	r = class_register(&tsync_pcr_class);
	if (r) {
		pr_info("[%s]tsync_pcr_class create fail.\n", __func__);
		return r;
	}

	/* init audio pts to -1, others to 0 */
	timestamp_apts_set(-1);
	timestamp_vpts_set(0);
	timestamp_vpts_set_u64(0);
	timestamp_pcrscr_set(0);
	wait_pcr_count = 0;
	tsync_pcr_debug = 0;
	tsync_audio_mode = 0;
	tsync_disable_demux_pcr = 0;
	tsync_pcr_latency_value = 540000;
	pr_info("[%s]init success.\n", __func__);
	return 0;
}

void __exit tsync_pcr_exit(void)
{
	class_unregister(&tsync_pcr_class);
	pr_info("[%s]exit success.\n", __func__);
}

