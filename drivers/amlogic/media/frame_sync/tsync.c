// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#ifdef ARC_700
/* #include <asm/arch/am_regs.h> */
#else
/* #include <mach/am_regs.h> */
#endif
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/frame_sync/tsync_pcr.h>
#include <linux/amlogic/media/registers/register.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/major.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/registers/cpu_version.h>

/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
/* TODO: for stream buffer register bit define only */
/* #endif */
#define TSYNC_DEVICE_NAME   "tsync"

#if !defined(CONFIG_PREEMPT)
#define CONFIG_AM_TIMESYNC_LOG
#endif

#ifdef CONFIG_AM_TIMESYNC_LOG
#define AMLOG
#define LOG_LEVEL_ERROR     0
#define LOG_LEVEL_ATTENTION 1
#define LOG_LEVEL_INFO      2
#define LOG_LEVEL_VAR       amlog_level_tsync
#define LOG_MASK_VAR        amlog_mask_tsync
#endif
#include <linux/amlogic/media/utils/amlog.h>
MODULE_AMLOG(AMLOG_DEFAULT_LEVEL, 0, LOG_DEFAULT_LEVEL_DESC,
			 LOG_DEFAULT_MASK_DESC);

/* #define DEBUG */
#define AVEVENT_FLAG_PARAM  0x01

/* #define TSYNC_SLOW_SYNC */

#define PCR_CHECK_INTERVAL  (HZ * 5)
#define PCR_DETECT_MARGIN_SHIFT_AUDIO_HI     7
#define PCR_DETECT_MARGIN_SHIFT_AUDIO_LO     7
#define PCR_DETECT_MARGIN_SHIFT_VIDEO_HI     4
#define PCR_DETECT_MARGIN_SHIFT_VIDEO_LO     4
#define PCR_MAINTAIN_MARGIN_SHIFT_AUDIO      4
#define PCR_MAINTAIN_MARGIN_SHIFT_VIDEO      1
#define PCR_RECOVER_PCR_ADJ 15

#define TSYNC_INIT_STATE (0X01)
unsigned int tsync_flag;

enum {
	PCR_SYNC_UNSET,
	PCR_SYNC_HI,
	PCR_SYNC_LO,
};

enum {
	PCR_TRIGGER_AUDIO,
	PCR_TRIGGER_VIDEO
};

enum tsync_stat_e {
	TSYNC_STAT_PCRSCR_SETUP_NONE,
	TSYNC_STAT_PCRSCR_SETUP_VIDEO,
	TSYNC_STAT_PCRSCR_SETUP_AUDIO
};

enum {
	/* Fixed: use the Nominal M/N values */
	TOGGLE_MODE_FIXED = 0,
	/* Toggle between the Nominal M/N values and the Low M/N values */
	TOGGLE_MODE_NORMAL_LOW,
	/* Toggle between the Nominal M/N values and the High M/N values */
	TOGGLE_MODE_NORMAL_HIGH,
	/* Toggle between the Low M/N values and the High M/N Values */
	TOGGLE_MODE_LOW_HIGH,
};

static const struct {
	const char *token;
	const u32 token_size;
	const enum avevent_e event;
	const u32 flag;
} avevent_token[] = {
	{
		"VIDEO_START", 11, VIDEO_START, AVEVENT_FLAG_PARAM
	}, {
		"VIDEO_STOP", 10, VIDEO_STOP, 0
	}, {
		"VIDEO_PAUSE", 11, VIDEO_PAUSE, AVEVENT_FLAG_PARAM
	}, {
		"VIDEO_TSTAMP_DISCONTINUITY", 26, VIDEO_TSTAMP_DISCONTINUITY,
		AVEVENT_FLAG_PARAM
	}, {
		"AUDIO_START", 11, AUDIO_START, AVEVENT_FLAG_PARAM
	}, {
		"AUDIO_RESUME", 12, AUDIO_RESUME, 0
	}, {
		"AUDIO_STOP", 10, AUDIO_STOP, 0
	}, {
		"AUDIO_PAUSE", 11, AUDIO_PAUSE, 0
	}, {
		"AUDIO_TSTAMP_DISCONTINUITY", 26, AUDIO_TSTAMP_DISCONTINUITY,
		AVEVENT_FLAG_PARAM
	}, {
		"AUDIO_PRE_START", 15, AUDIO_PRE_START, 0
	}, {
	    "AUDIO_WAIT", 10, AUDIO_WAIT, 0
	}
};

static const char * const tsync_mode_str[] = {
	"vmaster", "amaster", "pcrmaster"
};

static struct device *tsync_dev;
static DEFINE_SPINLOCK(lock);
static enum tsync_mode_e tsync_mode = TSYNC_MODE_AMASTER;
static enum tsync_stat_e tsync_stat = TSYNC_STAT_PCRSCR_SETUP_NONE;
static int tsync_enable;	/* 1; */
static int apts_discontinue;
static int vpts_discontinue;
static int pts_discontinue;
static u32 apts_discontinue_diff;
static u32 vpts_discontinue_diff;
static int tsync_abreak;
static bool tsync_pcr_recover_enable;
static int pcr_sync_stat = PCR_SYNC_UNSET;
static int pcr_recover_trigger;
static struct timer_list tsync_pcr_recover_timer;
static int tsync_trickmode;
static int vpause_flag;
static int apause_flag;
static bool dobly_avsync_test;
static int slowsync_enable;
static int apts_lookup_offset;

static bool new_arch;
static u32 checkin_apts_from_audiohal;
u8 demux_pcrscr_valid;
u8 audio_pid_valid;
u8 video_pid_valid;
u32 demux_pcrscr;
static u32 first_demux_pcrscr;
struct dmx_info demux_info;

pfun_amldemux_pcrscr_get amldemux_pcrscr_get_cb;

pfun_tsdemux_pcrscr_valid tsdemux_pcrscr_valid_cb;

pfun_tsdemux_pcrscr_get tsdemux_pcrscr_get_cb;

pfun_tsdemux_first_pcrscr_get tsdemux_first_pcrscr_get_cb;

pfun_tsdemux_pcraudio_valid tsdemux_pcraudio_valid_cb;

pfun_tsdemux_pcrvideo_valid tsdemux_pcrvideo_valid_cb;

pfun_get_buf_by_type get_buf_by_type_cb;

pfun_stbuf_level stbuf_level_cb;

pfun_stbuf_space stbuf_space_cb;

pfun_stbuf_size stbuf_size_cb;

/*
 *used to set player start sync mode, 0-none; 1-smoothsync; 2-droppcm;
 *default drop pcm
 */
static int startsync_mode = 2;

/*
 *		  threshold_min              threshold_max
 *			 |                          |
 *AMASTER<-------------->  |<-----DYNAMIC VMASTER---->|   VMASTER
 *static mode  | a dynamic  |            dynamic mode  |static mode
 *
 *static mode(S), Amster and Vmaster..
 *dynamic mode (D)  : discontinue....>min,< max,can switch to static mode,
 *		if diff is <min,and become to Sd mode if timerout.
 *sdynamic mode (A): dynamic mode become to static mode,because timer out,
 *		Don't do switch  befome timeout.
 *
 *tsync_av_mode switch...
 *(AMASTER S)<-->(D VMASTER)<--> (S VMASTER)
 *(D VMASTER)--time out->(A AMASTER)--time out->((AMASTER S))
 */
unsigned int tsync_av_threshold_min = AV_DISCONTINUE_THREDHOLD_MIN;
unsigned int tsync_av_threshold_max = AV_DISCONTINUE_THREDHOLD_MAX;
#define TSYNC_STATE_S  ('S')
#define TSYNC_STATE_A ('A')
#define TSYNC_STATE_D  ('D')
static unsigned int tsync_av_mode = TSYNC_STATE_S;	/* S=1,A=2,D=3; */
static u64
tsync_av_latest_switch_time_ms;	/* the time on latset switch */
static unsigned int tsync_av_dynamic_duration_ms;/* hold for dynamic mode; */
static u64 tsync_av_dynamic_timeout_ms;/* hold for dynamic mode; */
static struct timer_list tsync_state_switch_timer;
#define jiffies_ms div64_u64(get_jiffies_64() * 1000, HZ)
#define jiffies_ms32 (jiffies * 1000 / HZ)

static unsigned int tsync_syncthresh = 1;
static int tsync_dec_reset_flag;
static int tsync_dec_reset_video_start;
static int tsync_automute_on;
static int tsync_video_started;
static int is_tunnel_mode;

static int debug_pts_checkin;
static int debug_pts_checkout;
static int debug_vpts;
static int debug_apts;
static int apts_error_num;
static int vpts_error_num;

#define M_HIGH_DIFF    2
#define M_LOW_DIFF     2
#define PLL_FACTOR   10000

#define LOW_TOGGLE_TIME           99
#define NORMAL_TOGGLE_TIME        499
#define HIGH_TOGGLE_TIME          99

#define PTS_CACHED_LO_NORMAL_TIME (90000)
#define PTS_CACHED_NORMAL_LO_TIME (45000)
#define PTS_CACHED_HI_NORMAL_TIME (135000)
#define PTS_CACHED_NORMAL_HI_TIME (180000)

int register_tsync_callbackfunc(enum tysnc_func_type_e ntype, void *pfunc)
{
	if (new_arch) {
		pr_info("no need to register_tync_func.\n");
		return -1;
	}
	if (ntype >= TSYNC_FUNC_TYPE_MAX) {
		pr_info("register_tync_func ntype is err.\n");
		return -1;
	}
	switch (ntype) {
	case TSYNC_PCRSCR_VALID:
		tsdemux_pcrscr_valid_cb =
				(pfun_tsdemux_pcrscr_valid)pfunc;
		break;
	case TSYNC_PCRSCR_GET:
		tsdemux_pcrscr_get_cb =
				(pfun_tsdemux_pcrscr_get)pfunc;
		break;
	case TSYNC_FIRST_PCRSCR_GET:
		tsdemux_first_pcrscr_get_cb =
				(pfun_tsdemux_first_pcrscr_get)pfunc;
		break;
	case TSYNC_PCRAUDIO_VALID:
		tsdemux_pcraudio_valid_cb  =
				(pfun_tsdemux_pcraudio_valid)pfunc;
		break;
	case TSYNC_PCRVIDEO_VALID:
		tsdemux_pcrvideo_valid_cb =
				(pfun_tsdemux_pcrvideo_valid)pfunc;
		break;
	case TSYNC_BUF_BY_BYTE:
		get_buf_by_type_cb =
				(pfun_get_buf_by_type)pfunc;
		break;
	case TSYNC_STBUF_LEVEL:
		stbuf_level_cb =
				(pfun_stbuf_level)pfunc;
		break;
	case TSYNC_STBUF_SPACE:
		stbuf_space_cb =
				(pfun_stbuf_space)pfunc;
		break;
	case TSYNC_STBUF_SIZE:
		stbuf_size_cb =
				(pfun_stbuf_size)pfunc;
		break;
	default:
		break;
	}
	return 0;
}
EXPORT_SYMBOL(register_tsync_callbackfunc);

static void tsync_pcr_recover_with_audio(void)
{
}

static void tsync_pcr_recover_with_video(void)
{
	u32 vb_level = READ_VREG(VLD_MEM_VIFIFO_LEVEL);
	u32 vb_size = READ_VREG(VLD_MEM_VIFIFO_END_PTR)
				  - READ_VREG(VLD_MEM_VIFIFO_START_PTR) + 8;

	if (vb_level < (vb_size >> PCR_DETECT_MARGIN_SHIFT_VIDEO_LO)) {
		timestamp_pcrscr_set_adj(-PCR_RECOVER_PCR_ADJ);
		pr_info(" timestamp_pcrscr_set_adj(-%d);\n",
				PCR_RECOVER_PCR_ADJ);
	} else if ((vb_level + (vb_size >> PCR_DETECT_MARGIN_SHIFT_VIDEO_HI)) >
			   vb_size) {
		timestamp_pcrscr_set_adj(PCR_RECOVER_PCR_ADJ);
		pr_info(" timestamp_pcrscr_set_adj(%d);\n",
			PCR_RECOVER_PCR_ADJ);
	} else if ((vb_level > (vb_size >> PCR_MAINTAIN_MARGIN_SHIFT_VIDEO)) ||
		   (vb_level <
				(vb_size -
				 (vb_size >> PCR_MAINTAIN_MARGIN_SHIFT_VIDEO))))
		timestamp_pcrscr_set_adj(0);
}

static bool tsync_pcr_recover_use_video(void)
{
#define MEM_CTRL_EMPTY_EN       BIT(2)
	/*
	 *This is just a hacking to use audio output enable
	 *as the flag to check if this is a video only playback.
	 *Such processing can not handle an audio output with a
	 *mixer so audio playback has no direct relationship with
	 *applications. TODO.
	 */
	return ((READ_AIU_REG(AIU_MEM_I2S_CONTROL) &
			 (MEM_CTRL_EMPTY_EN | MEM_CTRL_EMPTY_EN)) == 0) ?
		true : false;
}

static void tsync_pcr_recover_timer_real(void)
{
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (tsync_pcr_recover_enable) {
		if (tsync_pcr_recover_use_video()) {
			tsync_pcr_recover_with_video();
		} else {
			timestamp_pcrscr_set_adj(0);
			tsync_pcr_recover_with_audio();
		}
	}

	spin_unlock_irqrestore(&lock, flags);
}

static void tsync_pcr_recover_timer_func(struct timer_list *arg)
{
	tsync_pcr_recover_timer_real();
	tsync_pcr_recover_timer.expires = jiffies + PCR_CHECK_INTERVAL;
	add_timer(&tsync_pcr_recover_timer);
}

/*
 *mode:
 *mode='V': diff_pts=|vpts-pcrscr|,jump_pts=vpts jump, video discontinue
 *mode='A': diff_pts=|apts-pcrscr|,jump_pts=apts jump, audio discontinue
 *mode='T': diff_pts=|vpts-apts|,timeout mode switch,
 */

static int tsync_mode_switch(int mode, unsigned long diff_pts, int jump_pts)
{
	int debugcnt = 0;
	int old_tsync_mode = tsync_mode;
	int old_tsync_av_mode = tsync_av_mode;
	char VA[] = "VA--";
	unsigned long oldtimeout = tsync_av_dynamic_timeout_ms;
	u32 pcr, vpts, apts;

	pcr = timestamp_pcrscr_get();
	vpts = timestamp_vpts_get();
	apts = timestamp_apts_get();

	if (tsync_mode == TSYNC_MODE_PCRMASTER) {
		pr_info
		("[%s]tsync_mode is pcr master, do nothing\n", __func__);
		return 0;
	}

	pr_info
	("%c-discontinue,pcr=%d,vpts=%d,apts=%d,diff_pts=%lu,jump_Pts=%d\n",
	 mode, timestamp_pcrscr_get(), timestamp_vpts_get(),
	 timestamp_apts_get(), diff_pts, jump_pts);
	if (!tsync_enable || !timestamp_apts_started()) {
		if (tsync_mode != TSYNC_MODE_VMASTER)
			tsync_mode = TSYNC_MODE_VMASTER;
		tsync_av_mode = TSYNC_STATE_S;
		tsync_av_dynamic_duration_ms = 0;
		pr_info("tsync_enable [%d]\n", tsync_enable);
		return 0;
	}
	if (mode == 'T') {	/*D/A--> ... */
		if (tsync_av_mode == TSYNC_STATE_D) {
			debugcnt |= 1 << 1;
			tsync_av_mode = TSYNC_STATE_A;
			tsync_mode = TSYNC_MODE_AMASTER;
			tsync_av_latest_switch_time_ms = jiffies_ms;
			tsync_av_dynamic_duration_ms =
				200 + diff_pts * 1000 / TIME_UNIT90K;
			if (tsync_av_dynamic_duration_ms > 5 * 1000)
				tsync_av_dynamic_duration_ms = 5 * 1000;
		} else if (tsync_av_mode == TSYNC_STATE_A) {
			debugcnt |= 1 << 2;
			tsync_av_mode = TSYNC_STATE_S;
			tsync_mode = TSYNC_MODE_AMASTER;
			tsync_av_latest_switch_time_ms = jiffies_ms;
			tsync_av_dynamic_duration_ms = 0;
		} else {
			/* / */
			if (abs(vpts - apts) < tsync_av_threshold_min ||
			diff_pts < tsync_av_threshold_min) {
				pr_info("-wcs-< tsync_av_threshold_min--\n");
				tsync_av_mode = TSYNC_STATE_S;
				tsync_mode = TSYNC_MODE_AMASTER;
				tsync_av_latest_switch_time_ms = jiffies_ms;
				tsync_av_dynamic_duration_ms = 0;
			} else if (abs(vpts - apts) < tsync_av_threshold_max ||
			diff_pts < tsync_av_threshold_max) {
				pr_info("-wcs-< tsync_av_threshold_max--\n");
				tsync_av_mode = TSYNC_STATE_D;
				tsync_mode = TSYNC_MODE_VMASTER;
				tsync_av_latest_switch_time_ms = jiffies_ms;
				tsync_av_dynamic_duration_ms = 20 * 1000;
			}
		}
		if (tsync_mode != old_tsync_mode ||
		    tsync_av_mode != old_tsync_av_mode)
			pr_info("mode changes:tsync_mode:%c->%c,state:%c->%c,",
				VA[old_tsync_mode], VA[tsync_mode],
				old_tsync_av_mode, tsync_av_mode);
			pr_info("debugcnt=0x%x,diff_pts=%lu\n",
				debugcnt,
				diff_pts);
		return 0;
	}

	if (diff_pts < tsync_av_threshold_min) {
		/*->min*/
		debugcnt |= 1 << 1;
		tsync_av_mode = TSYNC_STATE_S;
		tsync_mode = TSYNC_MODE_AMASTER;
		tsync_av_latest_switch_time_ms = jiffies_ms;
		tsync_av_dynamic_duration_ms = 0;
	} else if (diff_pts <= tsync_av_threshold_max) {	/*min<-->max */
		if (tsync_av_mode == TSYNC_STATE_S) {
			debugcnt |= 1 << 2;
			tsync_av_mode = TSYNC_STATE_D;
			tsync_mode = TSYNC_MODE_VMASTER;
			tsync_av_latest_switch_time_ms = jiffies_ms;
			tsync_av_dynamic_duration_ms = 20 * 1000;
		} else if (tsync_av_mode ==
				   TSYNC_STATE_D) {
			/*new discontinue,continue wait.... */
			debugcnt |= 1 << 7;
			tsync_av_mode = TSYNC_STATE_D;
			tsync_mode = TSYNC_MODE_VMASTER;
			tsync_av_latest_switch_time_ms = jiffies_ms;
			tsync_av_dynamic_duration_ms = 20 * 1000;
		}
	} else if (diff_pts >= tsync_av_threshold_max) {	/*max<-- */
		if (tsync_av_mode == TSYNC_STATE_D) {
			debugcnt |= 1 << 3;
			tsync_av_mode = TSYNC_STATE_S;
			tsync_mode = TSYNC_MODE_VMASTER;
			tsync_av_latest_switch_time_ms = jiffies_ms;
			tsync_av_dynamic_duration_ms = 0;
		} else if (tsync_mode != TSYNC_MODE_VMASTER) {
			debugcnt |= 1 << 4;
			tsync_av_mode = TSYNC_STATE_S;
			tsync_mode = TSYNC_MODE_VMASTER;
			tsync_av_latest_switch_time_ms = jiffies_ms;
			tsync_av_dynamic_duration_ms = 0;
		}
	} else {
		debugcnt |= 1 << 16;
	}

	if (oldtimeout != tsync_av_latest_switch_time_ms +
		tsync_av_dynamic_duration_ms) {
		/*duration changed,update new timeout. */
		tsync_av_dynamic_timeout_ms =
			tsync_av_latest_switch_time_ms +
			tsync_av_dynamic_duration_ms;
	}
	pr_info("discontinue-tsync_mode:%c->%c,state:%c->%c,jiffies=%x",
		VA[old_tsync_mode], VA[tsync_mode],
		old_tsync_av_mode, tsync_av_mode,
		(u32)jiffies_ms32);
	pr_info("debugcnt=0x%x,diff_pts=%lu,tsync_mode=%d\n",
		debugcnt, diff_pts, tsync_mode);
	return 0;
}

static void tsync_state_switch_timer_fun(struct timer_list *arg)
{
	if (!vpause_flag && !apause_flag && timestamp_apts_started()) {
		if (tsync_av_mode == TSYNC_STATE_D ||
		    tsync_av_mode == TSYNC_STATE_A) {
			if (tsync_av_dynamic_timeout_ms < jiffies_ms) {
				/* to be amaster? */
				tsync_mode_switch('T',
						  abs(timestamp_vpts_get() -
							timestamp_apts_get()),
						0);
				if (tsync_mode == TSYNC_MODE_AMASTER &&
				    abs(timestamp_apts_get() -
						   timestamp_pcrscr_get()) >
					(TIME_UNIT90K * 50 / 1000)) {
					timestamp_pcrscr_set
					(timestamp_apts_get());
				}
			}
		}
	} else {
		/* video&audio paused,changed the timeout time to latter. */
		tsync_av_dynamic_timeout_ms =
			jiffies_ms + tsync_av_dynamic_duration_ms;
	}
	tsync_state_switch_timer.expires = jiffies + msecs_to_jiffies(200);
	add_timer(&tsync_state_switch_timer);
}

u8 tsync_get_demux_pcrscr_valid_for_newarch(void)
{
	return demux_pcrscr_valid;
}
EXPORT_SYMBOL(tsync_get_demux_pcrscr_valid_for_newarch);

u8 tsync_get_demux_pcrscr_valid(void)
{
	if (new_arch)
		return tsync_get_demux_pcrscr_valid_for_newarch();

	if (tsdemux_pcrscr_valid_cb)
		demux_pcrscr_valid = tsdemux_pcrscr_valid_cb();
	else
		demux_pcrscr_valid = 0;
	return demux_pcrscr_valid;
}
EXPORT_SYMBOL(tsync_get_demux_pcrscr_valid);

u8 tsync_get_audio_pid_valid_for_newarch(void)
{
	return audio_pid_valid;
}
EXPORT_SYMBOL(tsync_get_audio_pid_valid_for_newarch);

u8 tsync_get_audio_pid_valid(void)
{
	if (new_arch)
		return tsync_get_audio_pid_valid_for_newarch();

	if (tsdemux_pcraudio_valid_cb)
		audio_pid_valid = tsdemux_pcraudio_valid_cb();
	else
		audio_pid_valid = 0;
	return audio_pid_valid;
}
EXPORT_SYMBOL(tsync_get_audio_pid_valid);

u8 tsync_get_video_pid_valid_for_newarch(void)
{
	return video_pid_valid;
}
EXPORT_SYMBOL(tsync_get_video_pid_valid_for_newarch);

u8 tsync_get_video_pid_valid(void)
{
	if (new_arch)
		return tsync_get_video_pid_valid_for_newarch();
	if (tsdemux_pcrvideo_valid_cb)
		video_pid_valid = (tsdemux_pcrvideo_valid_cb)();
	else
		video_pid_valid = 0;
	return video_pid_valid;
}
EXPORT_SYMBOL(tsync_get_video_pid_valid);

void tsync_get_demux_pcr_for_newarch(void)
{
	if (!amldemux_pcrscr_get_cb)
		amldemux_pcrscr_get_cb = symbol_request(demux_get_pcr);

	if (amldemux_pcrscr_get_cb) {
		amldemux_pcrscr_get_cb(demux_info.demux_device_id,
				       demux_info.index, (u64 *)&demux_pcrscr);
		if (first_demux_pcrscr == 0)
			first_demux_pcrscr = demux_pcrscr;
	}
}
EXPORT_SYMBOL(tsync_get_demux_pcr_for_newarch);

u8 tsync_get_demux_pcr(u32 *pcr)
{
	u8 ret = 0;

	if (new_arch) {
		if (!demux_pcrscr_valid) {
			*pcr = 0;
			return 0;
		}
		tsync_get_demux_pcr_for_newarch();
		*pcr = demux_pcrscr;
		ret = 1;
	} else {
		if (tsdemux_pcrscr_get_cb) {
			demux_pcrscr = tsdemux_pcrscr_get_cb();
			*pcr = demux_pcrscr;
			ret = 1;
		} else {
			ret = 0;
		}
	}
	return ret;
}
EXPORT_SYMBOL(tsync_get_demux_pcr);

void tsync_get_first_demux_pcr_for_newarch(void)
{
	tsync_get_demux_pcr_for_newarch();
}
EXPORT_SYMBOL(tsync_get_first_demux_pcr_for_newarch);

u8 tsync_get_first_demux_pcr(u32 *first_pcr)
{
	u8 ret = 0;

	if (new_arch) {
		if (!demux_pcrscr_valid) {
			*first_pcr = 0;
			return 0;
		}
		tsync_get_first_demux_pcr_for_newarch();
		*first_pcr = first_demux_pcrscr;
		ret = 1;
	} else {
		if (tsdemux_first_pcrscr_get_cb) {
			first_demux_pcrscr = tsdemux_first_pcrscr_get_cb();
			*first_pcr = first_demux_pcrscr;
			ret = 1;
		} else {
			ret = 0;
		}
	}
	return ret;
}
EXPORT_SYMBOL(tsync_get_first_demux_pcr);

void tsync_get_buf_by_type_for_newarch(u8 type, struct stream_buf_s *pbuf)
{
	pbuf = NULL;
}
EXPORT_SYMBOL(tsync_get_buf_by_type_for_newarch);

u8 tsync_get_buf_by_type(u8 type, struct stream_buf_s **pbuf)
{
	u8 ret = 0;

	if (new_arch) {
		tsync_get_buf_by_type_for_newarch(type, *pbuf);
		ret = 0;
	} else {
		if (get_buf_by_type_cb) {
			*pbuf = get_buf_by_type_cb(type);
			ret = 1;
		} else {
			ret = 0;
		}
	}
	return ret;
}
EXPORT_SYMBOL(tsync_get_buf_by_type);

void tsync_get_stbuf_level_for_newarch(struct stream_buf_s *pbuf,
				       u32 *buf_level)
{
	*buf_level = 0;
}
EXPORT_SYMBOL(tsync_get_stbuf_level_for_newarch);

u8 tsync_get_stbuf_level(struct stream_buf_s *pbuf, u32 *buf_level)
{
	u8 ret = 0;

	if (!pbuf)
		return 0;

	if (new_arch) {
		tsync_get_stbuf_level_for_newarch(pbuf, buf_level);
		ret = 0;
	} else {
		if (stbuf_level_cb) {
			*buf_level = stbuf_level_cb(pbuf);
			ret = 1;
		} else {
			ret = 0;
		}
	}
	return ret;
}
EXPORT_SYMBOL(tsync_get_stbuf_level);

void tsync_get_stbuf_space_for_newarch(struct stream_buf_s *pbuf,
				       u32 *buf_space)
{
	*buf_space = 0;
}
EXPORT_SYMBOL(tsync_get_stbuf_space_for_newarch);

u8 tsync_get_stbuf_space(struct stream_buf_s *pbuf, u32 *buf_space)
{
	u8 ret = 0;

	if (!pbuf)
		return 0;

	if (new_arch) {
		tsync_get_stbuf_space_for_newarch(pbuf, buf_space);
		ret = 0;
	} else {
		if (stbuf_space_cb) {
			*buf_space = stbuf_space_cb(pbuf);
			ret = 1;
		} else {
			ret = 0;
		}
	}
	return ret;
}
EXPORT_SYMBOL(tsync_get_stbuf_space);

void tsync_get_stbuf_size_for_newarch(struct stream_buf_s *pbuf, u32 *buf_size)
{
	*buf_size = 0;
}
EXPORT_SYMBOL(tsync_get_stbuf_size_for_newarch);

u8 tsync_get_stbuf_size(struct stream_buf_s *pbuf, u32 *buf_size)
{
	u8 ret = 0;

	if (!pbuf)
		return 0;

	if (new_arch) {
		tsync_get_stbuf_size_for_newarch(pbuf, buf_size);
		ret = 1;
	} else {
		if (stbuf_size_cb) {
			*buf_size = stbuf_size_cb(pbuf);
			ret = 1;
		} else {
			ret = 0;
		}
	}
	return ret;
}
EXPORT_SYMBOL(tsync_get_stbuf_size);

int tsync_check_pid_valid_for_newarch(struct dmx_info demux_info)
{
	if (!new_arch)
		return 0;
	if (demux_info.vpid != 0x1FFF)
		video_pid_valid = 1;
	else
		video_pid_valid = 0;

	if (demux_info.apid != 0x1FFF)
		audio_pid_valid = 1;
	else
		audio_pid_valid = 0;

	if (demux_info.pcrpid != 0x1FFF)
		demux_pcrscr_valid = 1;
	else
		demux_pcrscr_valid = 0;
	if (!video_pid_valid)
		pr_info("tsync exception video pid is valid.");
	if (!audio_pid_valid)
		pr_info("tsync exception audio pid is valid.");
	if (!demux_pcrscr_valid)
		pr_info("tsync exception demux pcr pid is valid.");
	return 1;
}

void tsync_reset(void)
{
	checkin_apts_from_audiohal = 0xffffffff;
	first_demux_pcrscr = 0;
}

void tsync_mode_reinit(u8 type)
{
	tsync_av_mode = TSYNC_STATE_S;
	tsync_av_dynamic_duration_ms = 0;
	if (tsync_mode == TSYNC_MODE_PCRMASTER)
		tsync_pcr_mode_reinit(type);
}
EXPORT_SYMBOL(tsync_mode_reinit);
void tsync_avevent_locked(enum avevent_e event, u32 param)
{
	u32 t;

	if (tsync_mode == TSYNC_MODE_PCRMASTER) {
		amlog_level(LOG_LEVEL_INFO,
			    "[%s]", __func__);
		amlog_level(LOG_LEVEL_INFO,
			    "PCR MASTER to use tsync pcr cmd deal ");
		tsync_pcr_avevent_locked(event, param);
		return;
	}

	switch (event) {
	case VIDEO_START:
		if (tsync_video_started == 0) {
			pr_info("[%s %d]VIDEO_START-tsync_mode:%d(%c)\n",
				__func__, __LINE__,
				tsync_mode, tsync_av_mode);
			pr_info("pcr:0x%x,param:0x%x,vpts:0x%x,apts:0x%x\n",
				timestamp_pcrscr_get(), param,
				timestamp_vpts_get(),
				timestamp_apts_get());
		}
		tsync_video_started = 1;
		tsync_set_av_state(0, 2);

	//set tsync mode to vmaster to avoid video block caused
	// by avpts-diff too much
	//threshold 120s is an arbitrary value

		if (tsync_enable && !get_vsync_pts_inc_mode()) {
			t = abs(param - timestamp_apts_get());
			if (tsync_av_mode == TSYNC_STATE_S ||
			    (tsync_av_mode == TSYNC_STATE_D &&
			    t < tsync_av_threshold_min)) {
				tsync_mode = TSYNC_MODE_AMASTER;
			}
		} else {
			tsync_mode = TSYNC_MODE_VMASTER;
			if (get_vsync_pts_inc_mode())
				tsync_stat = TSYNC_STAT_PCRSCR_SETUP_NONE;

		}

		if (tsync_dec_reset_flag)
			tsync_dec_reset_video_start = 1;

		if (slowsync_enable == 1) {	/* slow sync enable */
			timestamp_pcrscr_set(param);
			set_pts_realign();
			tsync_stat = TSYNC_STAT_PCRSCR_SETUP_VIDEO;
		} else {
			if (tsync_stat == TSYNC_STAT_PCRSCR_SETUP_NONE) {
				if (tsync_syncthresh &&
				    tsync_mode == TSYNC_MODE_AMASTER) {
					timestamp_pcrscr_set(param -
							VIDEO_HOLD_THRESHOLD);
				} else {
					timestamp_pcrscr_set(param);
				}
				set_pts_realign();
				tsync_stat = TSYNC_STAT_PCRSCR_SETUP_VIDEO;
			}
		}
		amlog_level(LOG_LEVEL_INFO,
			    "vpts to scr, apts = 0x%x, vpts = 0x%x\n",
				timestamp_apts_get(), timestamp_vpts_get());

		if (tsync_stat == TSYNC_STAT_PCRSCR_SETUP_AUDIO) {
			t = timestamp_pcrscr_get();
			if (abs(param - t) > tsync_av_threshold_max) {
				/* if this happens, then play */
				tsync_stat = TSYNC_STAT_PCRSCR_SETUP_VIDEO;
				tsync_mode = TSYNC_MODE_VMASTER;
				tsync_enable = 0;
				timestamp_pcrscr_set(param);
				set_pts_realign();
			}
		}
		if (!vpause_flag && !tsync_get_tunnel_mode())
			timestamp_pcrscr_enable(1);

		if (!timestamp_firstvpts_get() && param)
			timestamp_firstvpts_set(param);
		break;

	case VIDEO_STOP:
		tsync_stat = TSYNC_STAT_PCRSCR_SETUP_NONE;
		tsync_set_av_state(0, 3);
		timestamp_vpts_set(0);
		timestamp_vpts_set_u64(0);
		timestamp_pcrscr_set(0);
		timestamp_pcrscr_enable(0);
		timestamp_firstvpts_set(0);
		tsync_reset();
		tsync_video_started = 0;
		break;

	/*
	 *Note:
	 *Video and audio PTS discontinue happens typically with a loopback
	 *playback, with same bit stream play in loop and PTS wrap back from
	 *start point.
	 *When VIDEO_TSTAMP_DISCONTINUITY happens early, PCRSCR is set
	 *immedately to make video still keep running in VMATSER mode. This
	 *mode is restored to AMASTER mode when AUDIO_TSTAMP_DISCONTINUITY
	 *reports, or apts is close to scr later.
	 *When AUDIO_TSTAMP_DISCONTINUITY happens early, VMASTER mode is
	 *set to make video still keep running w/o setting PCRSCR. This mode
	 *is restored to AMASTER mode when VIDEO_TSTAMP_DISCONTINUITY
	 *reports, and scr is restored along with new video time stamp also.
	 */
	case VIDEO_TSTAMP_DISCONTINUITY: {
		unsigned int oldpts = timestamp_vpts_get();
		int oldmod = tsync_mode;

		if (tsync_mode == TSYNC_MODE_VMASTER &&
		    timestamp_apts_started())
			t = timestamp_apts_get();
		else
			t = timestamp_pcrscr_get();
		if (tsync_get_demux_pcrscr_valid()) {
			if (abs(param - oldpts) > tsync_av_threshold_min) {
				vpts_discontinue = 1;
				if (apts_discontinue == 1) {
					apts_discontinue = 0;
					vpts_discontinue = 0;
					pr_info("set apts->pcrsrc,pcrsrc %x to %x,diff %d\n",
						timestamp_pcrscr_get(),
						timestamp_apts_get(),
						timestamp_apts_get()
						- timestamp_pcrscr_get());
					timestamp_pcrscr_set(param);
				} else {
					pr_info("set para->pcrsrc,pcrsrc %x to %x,diff %d\n",
						timestamp_pcrscr_get(), param,
						param - timestamp_pcrscr_get());
					timestamp_pcrscr_set
					(timestamp_vpts_get());
				}
			}
			timestamp_vpts_set(param);
			vpts_error_num++;
			break;
		}
		/*
		 *amlog_level(LOG_LEVEL_ATTENTION,
		 *"VIDEO_TSTAMP_DISCONTINUITY, 0x%x, 0x%x\n", t, param);
		 */
		if ((abs(param - oldpts) > tsync_av_threshold_min) &&
		    (!get_vsync_pts_inc_mode())) {
			vpts_discontinue = 1;
			vpts_discontinue_diff = abs(param - t);
			tsync_mode_switch('V', abs(param - t),
					  param - oldpts);
		}

		timestamp_vpts_set(param);
		vpts_error_num++;
		if (tsync_mode == TSYNC_MODE_VMASTER) {
			timestamp_pcrscr_set(param);
			set_pts_realign();
		} else if (tsync_mode != oldmod &&
			   tsync_mode == TSYNC_MODE_AMASTER) {
			timestamp_pcrscr_set(timestamp_apts_get());
			set_pts_realign();
		}
	}
	break;

	case AUDIO_TSTAMP_DISCONTINUITY: {
		unsigned int oldpts = timestamp_apts_get();
		int oldmod = tsync_mode;

		amlog_level(LOG_LEVEL_ATTENTION,
			    "audio discontinue, reset apts, 0x%x\n",
				param);
		if (tsync_get_demux_pcrscr_valid()) {
			if (abs(param - oldpts) > tsync_av_threshold_min) {
				apts_discontinue = 1;
				if (vpts_discontinue == 1) {
					pr_info("set para->pcrsrc,pcrsrc from %x to %x,diff %d\n",
						timestamp_pcrscr_get(), param,
						param - timestamp_pcrscr_get());
					apts_discontinue = 0;
					vpts_discontinue = 0;
					timestamp_pcrscr_set(param);
				} else {
					pr_info("set vpts->pcrsrc,pcrsrc from %x to %x,diff %d\n",
						timestamp_pcrscr_get(),
						timestamp_vpts_get(),
						timestamp_vpts_get()
						- timestamp_pcrscr_get());
					timestamp_pcrscr_set
					(timestamp_vpts_get());
				}
			}
			timestamp_apts_set(param);
			apts_error_num++;
			break;
		}
		timestamp_apts_set(param);
		apts_error_num++;
		if (!tsync_enable) {
			timestamp_apts_set(param);
			break;
		}
		if (tsync_mode == TSYNC_MODE_AMASTER)
			t = timestamp_vpts_get();
		else
			t = timestamp_pcrscr_get();
		amlog_level(LOG_LEVEL_ATTENTION,
			    "AUDIO_TSTAMP_DISCONTINUITY, 0x%x, 0x%x\n",
				t, param);
		if ((abs(param - oldpts) > tsync_av_threshold_min) &&
		    (!get_vsync_pts_inc_mode())) {
			apts_discontinue = 1;
			apts_discontinue_diff = abs(param - t);
			tsync_mode_switch('A', abs(param - t),
					  param - oldpts);
		}
		timestamp_apts_set(param);
		if (tsync_mode == TSYNC_MODE_AMASTER) {
			timestamp_pcrscr_set(param);
			set_pts_realign();
		} else if (tsync_mode != oldmod &&
			   tsync_mode == TSYNC_MODE_VMASTER) {
			//When no first pic coming, the vpts value is invalid
			if (get_first_pic_coming())
				t = timestamp_vpts_get();
			else
				t = timestamp_firstvpts_get();
			timestamp_pcrscr_set(t);
			set_pts_realign();
		}
	}
	break;

	case AUDIO_PRE_START:
		timestamp_apts_start(0);
		break;

	case AUDIO_WAIT:
		timestamp_pcrscr_set(timestamp_vpts_get());
		break;
	case AUDIO_START:
		/* reset discontinue var */
		tsync_set_av_state(1, AUDIO_START);
		tsync_set_sync_adiscont(0);
		tsync_set_sync_adiscont_diff(0);
		tsync_set_sync_vdiscont(0);
		tsync_set_sync_vdiscont_diff(0);

		timestamp_apts_set(param);

		amlog_level(LOG_LEVEL_INFO, "audio start, reset apts = 0x%x\n",
			    param);

		timestamp_apts_enable(1);

		timestamp_apts_start(1);

		if (!tsync_enable)
			break;

		t = timestamp_pcrscr_get();

		pr_info("[%s]param %d, t %d\n",
					__func__, param, t);

		tsync_abreak = 0;
		/* after reset, video should be played first */
		if (tsync_dec_reset_flag) {
			int vpts = timestamp_vpts_get();

			if (param < vpts || !tsync_dec_reset_video_start)
				timestamp_pcrscr_set(param);
			else
				timestamp_pcrscr_set(vpts);
			set_pts_realign();
			tsync_dec_reset_flag = 0;
			tsync_dec_reset_video_start = 0;
		} else if (tsync_mode == TSYNC_MODE_AMASTER) {
			timestamp_pcrscr_set(param);
			set_pts_realign();
		}

		tsync_stat = TSYNC_STAT_PCRSCR_SETUP_AUDIO;

		amlog_level(LOG_LEVEL_INFO, "apts reset scr = 0x%x\n", param);

		timestamp_pcrscr_enable(1);
		apause_flag = 0;
		break;

	case AUDIO_RESUME:
		timestamp_apts_enable(1);
		apause_flag = 0;
		if (!tsync_enable)
			break;
		timestamp_pcrscr_enable(1);

		break;

	case AUDIO_STOP:
		tsync_set_av_state(1, AUDIO_STOP);
		timestamp_apts_enable(0);
		timestamp_apts_set(-1);
		tsync_abreak = 0;
		if (tsync_trickmode)
			tsync_stat = TSYNC_STAT_PCRSCR_SETUP_VIDEO;
		else
			tsync_stat = TSYNC_STAT_PCRSCR_SETUP_NONE;
		apause_flag = 0;
		timestamp_apts_start(0);
		tsync_reset();

		break;

	case AUDIO_PAUSE:
		apause_flag = 1;
		timestamp_apts_enable(0);

		if (!tsync_enable)
			break;

		timestamp_pcrscr_enable(0);
		break;

	case VIDEO_PAUSE:
		if (param == 1)
			vpause_flag = 1;
		else
			vpause_flag = 0;
		if (param == 1) {
			timestamp_pcrscr_enable(0);
			amlog_level(LOG_LEVEL_INFO, "video pause!\n");
		} else {
			if (!apause_flag || !tsync_enable) {
				timestamp_pcrscr_enable(1);
				amlog_level(LOG_LEVEL_INFO, "video resume\n");
			}
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
		/*
		 *if (vpause_flag)
		 *	amvdev_pause();
		 *else
		 *	amvdev_resume();
		 */
		//DEBUG_TMP
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(tsync_avevent_locked);

void tsync_avevent(enum avevent_e event, u32 param)
{
	ulong flags;
	ulong fiq_flag;

	amlog_level(LOG_LEVEL_INFO, "[%s]event:%d, param %d\n",
		    __func__, event, param);
	spin_lock_irqsave(&lock, flags);

	raw_local_save_flags(fiq_flag);

	local_fiq_disable();

	tsync_avevent_locked(event, param);

	raw_local_irq_restore(fiq_flag);

	spin_unlock_irqrestore(&lock, flags);
}
EXPORT_SYMBOL(tsync_avevent);

void tsync_audio_break(int audio_break)
{
	tsync_abreak = audio_break;
}
EXPORT_SYMBOL(tsync_audio_break);

void tsync_trick_mode(int trick_mode)
{
	tsync_trickmode = trick_mode;
}
EXPORT_SYMBOL(tsync_trick_mode);

void tsync_set_avthresh(unsigned int av_thresh)
{
	/* tsync_av_thresh = av_thresh; */
}
EXPORT_SYMBOL(tsync_set_avthresh);

void tsync_set_syncthresh(unsigned int sync_thresh)
{
	tsync_syncthresh = sync_thresh;
}
EXPORT_SYMBOL(tsync_set_syncthresh);

void tsync_set_dec_reset(void)
{
	tsync_dec_reset_flag = 1;
}
EXPORT_SYMBOL(tsync_set_dec_reset);

void tsync_set_enable(int enable)
{
	tsync_enable = enable;
	tsync_av_mode = TSYNC_STATE_S;
}
EXPORT_SYMBOL(tsync_set_enable);

int tsync_get_sync_adiscont(void)
{
	return apts_discontinue;
}
EXPORT_SYMBOL(tsync_get_sync_adiscont);

int tsync_get_sync_vdiscont(void)
{
	return vpts_discontinue;
}
EXPORT_SYMBOL(tsync_get_sync_vdiscont);

u32 tsync_get_sync_adiscont_diff(void)
{
	return apts_discontinue_diff;
}
EXPORT_SYMBOL(tsync_get_sync_adiscont_diff);

u32 tsync_get_sync_vdiscont_diff(void)
{
	return vpts_discontinue_diff;
}
EXPORT_SYMBOL(tsync_get_sync_vdiscont_diff);

void tsync_set_sync_adiscont_diff(u32 discontinue_diff)
{
	apts_discontinue_diff = discontinue_diff;
}
EXPORT_SYMBOL(tsync_set_sync_adiscont_diff);

void tsync_set_sync_vdiscont_diff(u32 discontinue_diff)
{
	vpts_discontinue_diff = discontinue_diff;
}
EXPORT_SYMBOL(tsync_set_sync_vdiscont_diff);

void tsync_set_sync_adiscont(int syncdiscont)
{
	apts_discontinue = syncdiscont;
}
EXPORT_SYMBOL(tsync_set_sync_adiscont);

void tsync_set_sync_vdiscont(int syncdiscont)
{
	vpts_discontinue = syncdiscont;
}
EXPORT_SYMBOL(tsync_set_sync_vdiscont);

int tsync_set_video_runmode(void)
{
	if (tsync_get_demux_pcrscr_valid()) {
		if (vpts_discontinue == 1 || apts_discontinue == 1)
			return 1;
	}
	return 0;
}
EXPORT_SYMBOL(tsync_set_video_runmode);

void tsync_set_automute_on(int automute_on)
{
	tsync_automute_on = automute_on;
}
EXPORT_SYMBOL(tsync_set_automute_on);

int tsync_set_apts(unsigned int pts)
{
	unsigned int t;
	/* ssize_t r; */
	unsigned int oldpts = timestamp_apts_get();
	int oldmod = tsync_mode;

	if (tsync_mode == TSYNC_MODE_PCRMASTER) {
		tsync_pcr_set_apts(pts);
		return 0;
	}

	if (tsync_abreak)
		tsync_abreak = 0;
	if (!tsync_enable) {
		timestamp_apts_set(pts);
		return 0;
	}
	if (tsync_mode == TSYNC_MODE_AMASTER)
		t = timestamp_vpts_get();
	else
		t = timestamp_pcrscr_get();
	if (tsync_get_demux_pcrscr_valid()) {
		timestamp_apts_set(pts);
		if ((int)(timestamp_apts_get() - timestamp_pcrscr_get())
			> 30 * TIME_UNIT90K / 1000 ||
			(int)(timestamp_pcrscr_get() - timestamp_apts_get())
			> 80 * TIME_UNIT90K / 1000) {
			timestamp_pcrscr_set(pts);
			set_pts_realign();
		}
		return 0;
	}
	/* do not switch tsync mode until first video toggled. */
	if ((abs(oldpts - pts) > tsync_av_threshold_min) &&
	    (timestamp_firstvpts_get() > 0)) {	/* is discontinue */
		apts_discontinue = 1;
		/*if in VMASTER ,just wait */
		tsync_mode_switch('A', abs(pts - t),
				  pts - oldpts);
	}
	timestamp_apts_set(pts);

	if (get_vsync_pts_inc_mode() && tsync_mode != TSYNC_MODE_VMASTER)
		tsync_mode = TSYNC_MODE_VMASTER;

	if (tsync_mode == TSYNC_MODE_AMASTER)
		t = timestamp_pcrscr_get();

	if (tsync_mode == TSYNC_MODE_AMASTER) {
		/* special used for Dobly Certification AVSync test */
		if (dobly_avsync_test) {
			if (get_vsync_pts_inc_mode() &&
			    (((int)(timestamp_apts_get() - t) >
				  60 * TIME_UNIT90K / 1000) ||
				 (int)(t - timestamp_apts_get()) >
				 100 * TIME_UNIT90K / 1000)) {
				pr_info
				("[%d:avsync_test]reset apts:0x%x-->0x%x,",
				 __LINE__, oldpts, pts);
				pr_info(" pcr 0x%x, diff %d\n",
					t, pts - t);
				timestamp_pcrscr_set(pts);
			} else if ((!get_vsync_pts_inc_mode()) &&
					   ((int)(timestamp_apts_get() - t) >
						30 * TIME_UNIT90K / 1000 ||
						(int)(t -
							timestamp_apts_get()) >
						80 * TIME_UNIT90K / 1000))
				timestamp_pcrscr_set(pts);
		} else {
			if (get_vsync_pts_inc_mode() &&
			    (((int)(timestamp_apts_get() - t) >
				  100 * TIME_UNIT90K / 1000) ||
				 (int)(t - timestamp_apts_get()) >
				 500 * TIME_UNIT90K / 1000)) {
				pr_info
				("[%d]reset apts:0x%x-->0x%x, ",
				 __LINE__, oldpts, pts);
				pr_info("pcr 0x%x, diff %d\n",
					t, pts - t);
				timestamp_pcrscr_set(pts);
				set_pts_realign();
			} else if ((!get_vsync_pts_inc_mode()) &&
				   (abs(timestamp_apts_get() - t) >
						   100 * TIME_UNIT90K / 1000)) {
				pr_info
				("[%d]reset apts:0x%x-->0x%x, ",
				 __LINE__, oldpts, pts);
				pr_info("pcr 0x%x, diff %d\n",
					t, pts - t);
				timestamp_pcrscr_set(pts);
				set_pts_realign();
			}
		}
	} else if ((oldmod != tsync_mode) && (tsync_mode == TSYNC_MODE_VMASTER))
		timestamp_pcrscr_set(get_first_pic_coming()
		? timestamp_vpts_get() : timestamp_firstvpts_get());
	return 0;
}
EXPORT_SYMBOL(tsync_set_apts);

/*********************************************************/

static ssize_t pcr_recover_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s %s\n",
			((tsync_pcr_recover_enable) ? "on" : "off"),
			((pcr_sync_stat ==
			  PCR_SYNC_UNSET) ? ("UNSET") : ((pcr_sync_stat ==
					  PCR_SYNC_LO) ? "LO" :
							 "HI")));
}

void tsync_pcr_recover(void)
{
}
EXPORT_SYMBOL(tsync_pcr_recover);

int tsync_get_mode(void)
{
	return tsync_mode;
}
EXPORT_SYMBOL(tsync_get_mode);

int tsync_get_debug_pts_checkin(void)
{
	return debug_pts_checkin;
}
EXPORT_SYMBOL(tsync_get_debug_pts_checkin);

int tsync_get_debug_pts_checkout(void)
{
	return debug_pts_checkout;
}
EXPORT_SYMBOL(tsync_get_debug_pts_checkout);

int tsync_get_debug_vpts(void)
{
	return debug_vpts;
}
EXPORT_SYMBOL(tsync_get_debug_vpts);

int tsync_get_debug_apts(void)
{
	return debug_apts;
}
EXPORT_SYMBOL(tsync_get_debug_apts);

int tsync_get_av_threshold_min(void)
{
	return tsync_av_threshold_min;
}
EXPORT_SYMBOL(tsync_get_av_threshold_min);

int tsync_get_av_threshold_max(void)
{
	return tsync_av_threshold_max;
}
EXPORT_SYMBOL(tsync_get_av_threshold_max);

int tsync_set_av_threshold_min(int min)
{
	return tsync_av_threshold_min = min;
}
EXPORT_SYMBOL(tsync_set_av_threshold_min);

int tsync_set_av_threshold_max(int max)
{
	return tsync_av_threshold_max = max;
}
EXPORT_SYMBOL(tsync_set_av_threshold_max);

int tsync_get_vpause_flag(void)
{
	return vpause_flag;
}
EXPORT_SYMBOL(tsync_get_vpause_flag);

int tsync_set_vpause_flag(int mode)
{
	return vpause_flag = mode;
}
EXPORT_SYMBOL(tsync_set_vpause_flag);

int tsync_get_slowsync_enable(void)
{
	return slowsync_enable;
}
EXPORT_SYMBOL(tsync_get_slowsync_enable);

int tsync_set_slowsync_enable(int enable)
{
	return slowsync_enable = enable;
}
EXPORT_SYMBOL(tsync_set_slowsync_enable);

int tsync_get_startsync_mode(void)
{
	return startsync_mode;
}
EXPORT_SYMBOL(tsync_get_startsync_mode);

int tsync_set_startsync_mode(int mode)
{
	return startsync_mode = mode;
}
EXPORT_SYMBOL(tsync_set_startsync_mode);

int tsync_set_tunnel_mode(int mode)
{
	tsync_mode = TSYNC_MODE_AMASTER;
	return is_tunnel_mode = mode;
}
EXPORT_SYMBOL(tsync_set_tunnel_mode);

int tsync_get_tunnel_mode(void)
{
	return is_tunnel_mode;
}
EXPORT_SYMBOL(tsync_get_tunnel_mode);

bool tsync_check_vpts_discontinuity(unsigned int vpts)
{
	unsigned int systemtime;

	if (tsync_get_mode() != TSYNC_MODE_PCRMASTER)
		return false;
	if (tsync_pcr_demux_pcr_used() == 0)
		systemtime = timestamp_pcrscr_get();
	else
		systemtime = timestamp_pcrscr_get()
			+ timestamp_get_pcrlatency();
	if (vpts > systemtime &&
	    (vpts - systemtime) > tsync_vpts_discontinuity_margin())
		return true;
	else if (systemtime > vpts &&
		 (systemtime - vpts) > tsync_vpts_discontinuity_margin())
		return true;
	else
		return false;
}
EXPORT_SYMBOL(tsync_check_vpts_discontinuity);

int tsync_get_vpts_error_num(void)
{
	return vpts_error_num;
}
EXPORT_SYMBOL(tsync_get_vpts_error_num);

int tsync_get_apts_error_num(void)
{
	return apts_error_num;
}
EXPORT_SYMBOL(tsync_get_apts_error_num);

bool tsync_get_new_arch(void)
{
	return new_arch;
}
EXPORT_SYMBOL(tsync_get_new_arch);

u32 tsync_get_checkin_apts(void)
{
	return checkin_apts_from_audiohal;
}
EXPORT_SYMBOL(tsync_get_checkin_apts);

static ssize_t pcr_recover_store(struct class *class,
				 struct class_attribute *attr,
				 const char *buf, size_t size)
{
	return size;
}

static ssize_t pts_video_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_vpts_get());
}

static ssize_t pts_video_u64_show(struct class *class,
			     struct class_attribute *attr, char *buf)
{
	u64 pts_val;

	pts_val = div64_u64(timestamp_vpts_get_u64() * 9, 100);
	return sprintf(buf, "0x%llx\n", pts_val);
}

static ssize_t pts_video_store(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t size)
{
	unsigned int pts;
	ssize_t r;

	/*r = sscanf(buf, "0x%x", &pts);*/
	r = kstrtouint(buf, 0, &pts);

	if (r != 0)
		return -EINVAL;

	timestamp_vpts_set(pts);
	return size;
}

static ssize_t demux_pcr_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_tsdemux_pcr_get());
}

static ssize_t pts_audio_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_apts_get());
}

static ssize_t pts_audio_u64_show(struct class *class,
			     struct class_attribute *attr, char *buf)
{
	u64 pts_val;

	pts_val = div64_u64(timestamp_apts_get_u64() * 9, 100);
	return sprintf(buf, "0x%llx\n", pts_val);
}

static ssize_t pts_audio_store(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t size)
{
	unsigned int pts;
	ssize_t r;

	r = kstrtouint(buf, 0, &pts);
	if (r != 0)
		return -EINVAL;

	tsync_set_apts(pts);

	return size;
}

static ssize_t dobly_av_sync_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	return sprintf(buf,
		"avsync:cur value is %d\n 0:no set(default)\n 1:avsync test\n",
		dobly_avsync_test ? 1 : 0);
}

static ssize_t dobly_av_sync_store(struct class *class,
				   struct class_attribute *attr,
				   const char *buf, size_t size)
{
	unsigned int value;
	ssize_t r;

	/*r = sscanf(buf, "%u", &value);*/
	r = kstrtoint(buf, 0, &value);

	if (r != 0)
		return -EINVAL;

	if (value == 1)
		dobly_avsync_test = true;
	else
		dobly_avsync_test = false;

	return size;
}

static ssize_t pts_pcrscr_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_pcrscr_get());
}

static ssize_t pts_pcrscr_u64_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	u64 pts_val;

	pts_val = div64_u64(timestamp_pcrscr_get_u64() * 9, 100);
	return sprintf(buf, "0x%llx\n", pts_val);
}

static ssize_t apts_checkin_flag_show(struct class *class,
				      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", first_pts_checkin_complete(PTS_TYPE_AUDIO));
}

static ssize_t pts_pcrscr_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t size)
{
	unsigned int pts;
	ssize_t r;

	/*r = sscanf(buf, "0x%x", &pts);*/
	r = kstrtouint(buf, 0, &pts);

	if (r != 0)
		return -EINVAL;

	timestamp_pcrscr_set(pts);
	set_pts_realign();

	return size;
}

static ssize_t event_store(struct class *class,
			   struct class_attribute *attr,
			   const char *buf, size_t size)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(avevent_token); i++) {
		if (strncmp
			(avevent_token[i].token, buf,
			 avevent_token[i].token_size) == 0) {
			if (avevent_token[i].flag & AVEVENT_FLAG_PARAM) {
				char *param_str = strchr(buf, ':');
				unsigned long value;
				int ret;

				if (!param_str)
					return -EINVAL;
				ret = kstrtoul(param_str + 1, 0,
					       (unsigned long *)&value);
				if (ret < 0)
					return -EINVAL;
				tsync_avevent(avevent_token[i].event,
					      value);
			} else {
				tsync_avevent(avevent_token[i].event, 0);
			}

			return size;
		}
	}

	return -EINVAL;
}

static ssize_t mode_show(struct class *class,
			 struct class_attribute *attr, char *buf)
{
	if (tsync_mode <= TSYNC_MODE_PCRMASTER) {
		return sprintf(buf, "%d: %s\n", tsync_mode,
			       tsync_mode_str[tsync_mode]);
	}

	return sprintf(buf, "invalid mode");
}

static ssize_t mode_store(struct class *class,
			  struct class_attribute *attr,
			  const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if (r != 0)
		return -EINVAL;

	if (mode == TSYNC_MODE_PCRMASTER)
		tsync_mode = TSYNC_MODE_PCRMASTER;
	else if (mode == TSYNC_MODE_VMASTER)
		tsync_mode = TSYNC_MODE_VMASTER;
	else
		tsync_mode = TSYNC_MODE_AMASTER;

	pr_info("[%s]tsync_mode=%d, buf=%s\n", __func__, tsync_mode, buf);
	return size;
}

static ssize_t enable_show(struct class *class,
			   struct class_attribute *attr, char *buf)
{
	if (tsync_enable)
		return sprintf(buf, "1: enabled\n");

	return sprintf(buf, "0: disabled\n");
}

static ssize_t enable_store(struct class *class,
			    struct class_attribute *attr,
			    const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if (r != 0)
		return -EINVAL;

	if (!tsync_automute_on)
		tsync_enable = mode ? 1 : 0;

	return size;
}

static ssize_t discontinue_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	pts_discontinue = vpts_discontinue || apts_discontinue;
	if (pts_discontinue) {
		return sprintf(buf, "apts diff %d, vpts diff=%d\n",
			apts_discontinue_diff, vpts_discontinue_diff);
	}

	return sprintf(buf, "0: pts_continue\n");
}

static ssize_t discontinue_store(struct class *class,
				 struct class_attribute *attr,
				 const char *buf, size_t size)
{
	unsigned int discontinue;
	ssize_t r;

	/*r = sscanf(buf, "%d", &discontinue);*/
	r = kstrtoint(buf, 0, &discontinue);

	if (r != 0)
		return -EINVAL;

	pts_discontinue = discontinue ? 1 : 0;

	return size;
}

static ssize_t debug_pts_checkin_show(struct class *class,
				      struct class_attribute *attr, char *buf)
{
	if (debug_pts_checkin)
		return sprintf(buf, "1: debug pts checkin on\n");

	return sprintf(buf, "0: debug pts checkin off\n");
}

static ssize_t debug_pts_checkin_store(struct class *class,
				       struct class_attribute *attr,
				       const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if (r != 0)
		return -EINVAL;

	debug_pts_checkin = mode ? 1 : 0;

	return size;
}

static ssize_t debug_pts_checkout_show(struct class *class,
				       struct class_attribute *attr, char *buf)
{
	if (debug_pts_checkout)
		return sprintf(buf, "1: debug pts checkout on\n");

	return sprintf(buf, "0: debug pts checkout off\n");
}

static ssize_t debug_pts_checkout_store(struct class *class,
					struct class_attribute *attr,
					const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if (r != 0)
		return -EINVAL;

	debug_pts_checkout = mode ? 1 : 0;

	return size;
}

static ssize_t debug_video_pts_show(struct class *class,
				    struct class_attribute *attr, char *buf)
{
	if (debug_vpts)
		return sprintf(buf, "1: debug vpts on\n");

	return sprintf(buf, "0: debug vpts off\n");
}

static ssize_t debug_video_pts_store(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if (r != 0)
		return -EINVAL;

	debug_vpts = mode ? 1 : 0;

	return size;
}

static ssize_t debug_audio_pts_show(struct class *class,
				    struct class_attribute *attr, char *buf)
{
	if (debug_apts)
		return sprintf(buf, "1: debug apts on\n");

	return sprintf(buf, "0: debug apts off\n");
}

static ssize_t debug_audio_pts_store(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if (r != 0)
		return -EINVAL;

	debug_apts = mode ? 1 : 0;

	return size;
}

static ssize_t av_threshold_min_show(struct class *class,
				     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "tsync_av_threshold_min=%d\n",
		       tsync_av_threshold_min);
}

static ssize_t av_threshold_min_store(struct class *class,
				      struct class_attribute *attr,
				      const char *buf, size_t size)
{
	unsigned int min;
	ssize_t r;

	/*r = sscanf(buf, "%d", &min);*/
	r = kstrtoint(buf, 0, &min);

	if (r != 0)
		return -EINVAL;

	tsync_set_av_threshold_min(min);
	return size;
}

static ssize_t av_threshold_max_show(struct class *class,
				     struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "tsync_av_threshold_max=%d\n",
		       tsync_av_threshold_max);
}

static ssize_t av_threshold_max_store(struct class *class,
				      struct class_attribute *attr,
				      const char *buf, size_t size)
{
	unsigned int max;
	ssize_t r;

	/*r = sscanf(buf, "%d", &max);*/
	r = kstrtoint(buf, 0, &max);

	if (r != 0)
		return -EINVAL;

	tsync_set_av_threshold_max(max);
	return size;
}

static ssize_t last_checkin_apts_show(struct class *class,
				      struct class_attribute *attr, char *buf)
{
	unsigned int last_apts;

	last_apts = get_last_checkin_pts(PTS_TYPE_AUDIO);
	return sprintf(buf, "0x%x\n", last_apts);
}

static ssize_t last_checkin_vpts_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	unsigned int last_vpts;

	last_vpts = get_last_checkin_pts(PTS_TYPE_VIDEO);
	return sprintf(buf, "0x%x\n", last_vpts);
}

static ssize_t firstvpts_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_firstvpts_get());
}

static ssize_t videostarted_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", tsync_video_started);
}

static ssize_t firstapts_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_firstapts_get());
}

static ssize_t firstapts_store(struct class *class,
			       struct class_attribute *attr,
			       const char *buf, size_t size)
{
	unsigned int pts;
	ssize_t r;

	/*r = sscanf(buf, "0x%x", &pts);*/
	r = kstrtoint(buf, 0, &pts);

	if (r != 0)
		return -EINVAL;

	timestamp_firstapts_set(pts);

	return size;
}

static ssize_t checkin_firstvpts_show(struct class *class,
				      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_checkin_firstvpts_get());
}

static ssize_t checkin_firstapts_show(struct class *class,
				      struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", timestamp_checkin_firstapts_get());
}

static ssize_t vpause_flag_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", tsync_get_vpause_flag());
}

static ssize_t vpause_flag_store(struct class *class,
				 struct class_attribute *attr,
				 const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if (r != 0)
		return -EINVAL;
	tsync_set_vpause_flag(mode);
	return size;
}

static ssize_t slowsync_enable_show(struct class *class,
				    struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "slowsync enable:0x%x\n",
		       tsync_get_slowsync_enable());
}

static ssize_t slowsync_enable_store(struct class *class,
				     struct class_attribute *attr,
				     const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);

	if (r != 0)
		return -EINVAL;

	tsync_set_slowsync_enable(mode);
	return size;
}

static ssize_t startsync_mode_show(struct class *class,
				   struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", tsync_get_startsync_mode());
}

static ssize_t pts_latency_show(struct class *class,
			    struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", timestamp_get_pcrlatency());
}

static ssize_t pts_latency_store(struct class *class,
			     struct class_attribute *attr,
			     const char *buf, size_t size)
{
	unsigned int latency = 0;
	ssize_t r;

	r = kstrtoint(buf, 0, &latency);
	if (r != 0)
		return -EINVAL;
	timestamp_set_pcrlatency(latency);
	return size;
}

static ssize_t avsync_count_show(struct class *class,
				  struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", timestamp_avsync_counter_get());
}

static ssize_t avsync_count_store(struct class *class,
				   struct class_attribute *attr,
				   const char *buf, size_t size)
{
	unsigned int counts = 0;
	ssize_t r;

	r = kstrtouint(buf, 0, &counts);
	if (r != 0)
		return -EINVAL;
	timestamp_avsync_counter_set(counts);
	return size;
}

static ssize_t apts_lookup_show(struct class *class,
				struct class_attribute *attrr, char *buf)
{
	u32 frame_size;
	unsigned int  pts = 0xffffffff;

	pts_lookup_offset(PTS_TYPE_AUDIO, apts_lookup_offset,
			  &pts, &frame_size, 300);
	return sprintf(buf, "0x%x\n", pts);
}

static ssize_t apts_lookup_store(struct class *class,
				 struct class_attribute *attr,
				 const char *buf,
				 size_t size)
{
	unsigned int offset;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &offset);

	if (r != 0)
		return -EINVAL;
	apts_lookup_offset = offset;
	return size;
}

static ssize_t startsync_mode_store(struct class *class,
				    struct class_attribute *attr,
				    const char *buf, size_t size)
{
	unsigned int mode;
	ssize_t r;

	/*r = sscanf(buf, "%d", &mode);*/
	r = kstrtoint(buf, 0, &mode);
	if (r != 0)
		return -EINVAL;

	tsync_set_startsync_mode(mode);
	return size;
}

static CLASS_ATTR_RW(pts_video);
static CLASS_ATTR_RO(pts_video_u64);
static CLASS_ATTR_RW(pts_audio);
static CLASS_ATTR_RO(pts_audio_u64);
static CLASS_ATTR_RW(dobly_av_sync);
static CLASS_ATTR_RW(pts_pcrscr);
static CLASS_ATTR_RO(pts_pcrscr_u64);
static CLASS_ATTR_RO(apts_checkin_flag);
static CLASS_ATTR_WO(event);
static CLASS_ATTR_RW(mode);
static CLASS_ATTR_RW(enable);
static CLASS_ATTR_RW(pcr_recover);
static CLASS_ATTR_RW(discontinue);
static CLASS_ATTR_RW(debug_pts_checkin);
static CLASS_ATTR_RW(debug_pts_checkout);
static CLASS_ATTR_RW(debug_video_pts);
static CLASS_ATTR_RW(debug_audio_pts);
static CLASS_ATTR_RW(av_threshold_min);
static CLASS_ATTR_RW(av_threshold_max);
static CLASS_ATTR_RO(last_checkin_apts);
static CLASS_ATTR_RO(firstvpts);
static CLASS_ATTR_RW(vpause_flag);
static CLASS_ATTR_RW(slowsync_enable);
static CLASS_ATTR_RW(startsync_mode);
static CLASS_ATTR_RW(firstapts);
static CLASS_ATTR_RO(checkin_firstvpts);
static CLASS_ATTR_RW(apts_lookup);
static CLASS_ATTR_RO(demux_pcr);
static CLASS_ATTR_RO(checkin_firstapts);
static CLASS_ATTR_RW(pts_latency);
static CLASS_ATTR_RO(videostarted);
static CLASS_ATTR_RW(avsync_count);
static CLASS_ATTR_RO(last_checkin_vpts);

static struct attribute *tsync_class_attrs[] = {
	&class_attr_pts_video.attr,
	&class_attr_pts_video_u64.attr,
	&class_attr_pts_audio.attr,
	&class_attr_pts_audio_u64.attr,
	&class_attr_dobly_av_sync.attr,
	&class_attr_pts_pcrscr.attr,
	&class_attr_pts_pcrscr_u64.attr,
	&class_attr_apts_checkin_flag.attr,
	&class_attr_event.attr,
	&class_attr_mode.attr,
	&class_attr_enable.attr,
	&class_attr_pcr_recover.attr,
	&class_attr_discontinue.attr,
	&class_attr_debug_pts_checkin.attr,
	&class_attr_debug_pts_checkout.attr,
	&class_attr_debug_video_pts.attr,
	&class_attr_debug_audio_pts.attr,
	&class_attr_av_threshold_min.attr,
	&class_attr_av_threshold_max.attr,
	&class_attr_last_checkin_apts.attr,
	&class_attr_firstvpts.attr,
	&class_attr_vpause_flag.attr,
	&class_attr_slowsync_enable.attr,
	&class_attr_startsync_mode.attr,
	&class_attr_firstapts.attr,
	&class_attr_checkin_firstvpts.attr,
	&class_attr_apts_lookup.attr,
	&class_attr_demux_pcr.attr,
	&class_attr_checkin_firstapts.attr,
	&class_attr_pts_latency.attr,
	&class_attr_videostarted.attr,
	&class_attr_avsync_count.attr,
	&class_attr_last_checkin_vpts.attr,
	NULL
};

ATTRIBUTE_GROUPS(tsync_class);

static struct class tsync_class = {
	.name = "tsync",
	.class_groups = tsync_class_groups,
};

int tsync_store_fun(const char *trigger, int id, const char *buf, int size)
{
	int ret = size;

	switch (id) {
	case 0:	return pts_video_store(NULL, NULL, buf, size);
	case 1:	return pts_audio_store(NULL, NULL, buf, size);
	case 2:	return dobly_av_sync_store(NULL, NULL, buf, size);
	case 3:	return pts_pcrscr_store(NULL, NULL, buf, size);
	case 4:	return event_store(NULL, NULL, buf, size);
	case 5:	return mode_store(NULL, NULL, buf, size);
	case 6:	return enable_store(NULL, NULL, buf, size);
	case 7:	return pcr_recover_store(NULL, NULL, buf, size);
	case 8:	return discontinue_store(NULL, NULL, buf, size);
	case 9:	return debug_pts_checkin_store(NULL, NULL, buf, size);
	case 10:return debug_pts_checkout_store(NULL, NULL, buf, size);
	case 11:return debug_video_pts_store(NULL, NULL, buf, size);
	case 12:return debug_audio_pts_store(NULL, NULL, buf, size);
	case 13:return av_threshold_min_store(NULL, NULL, buf, size);
	case 14:return av_threshold_max_store(NULL, NULL, buf, size);
	/*case 15:return -1;*/
	/*case 16:return -1;*/
	case 17:return vpause_flag_store(NULL, NULL, buf, size);
	case 18:return slowsync_enable_store(NULL, NULL, buf, size);
	case 19:return startsync_mode_store(NULL, NULL, buf, size);
	case 20:return firstapts_store(NULL, NULL, buf, size);
	case 21:return -1;
	default:
		ret = -1;
	}
	return size;
}

int tsync_show_fun(const char *trigger, int id, char *sbuf, int size)
{
	int ret = -1;
	void *buf, *getbuf = NULL;

	if (size < PAGE_SIZE) {
		getbuf = (void *)__get_free_page(GFP_KERNEL);
		if (!getbuf)
			return -ENOMEM;
		buf = getbuf;
	} else {
		buf = sbuf;
	}

	switch (id) {
	case 0:
		ret = pts_video_show(NULL, NULL, buf);
		break;
	case 1:
		ret = pts_audio_show(NULL, NULL, buf);
		break;
	case 2:
		ret =  dobly_av_sync_show(NULL, NULL, buf);
		break;
	case 3:
		ret = pts_pcrscr_show(NULL, NULL, buf);
		break;
	case 4:
		ret = -1;
		break;
	case 5:
		ret = mode_show(NULL, NULL, buf);
		break;
	case 6:
		ret = enable_show(NULL, NULL, buf);
		break;
	case 7:
		ret = pcr_recover_show(NULL, NULL, buf);
		break;
	case 8:
		ret = discontinue_show(NULL, NULL, buf);
		break;
	case 9:
		ret = debug_pts_checkin_show(NULL, NULL, buf);
		break;
	case 10:
		ret = debug_pts_checkout_show(NULL, NULL, buf);
		break;
	case 11:
		ret = debug_video_pts_show(NULL, NULL, buf);
		break;
	case 12:
		ret = debug_audio_pts_show(NULL, NULL, buf);
		break;
	case 13:
		ret = av_threshold_min_show(NULL, NULL, buf);
		break;
	case 14:
		ret = av_threshold_max_show(NULL, NULL, buf);
		break;
	case 15:
		ret = last_checkin_apts_show(NULL, NULL, buf);
		break;
	case 16:
		ret = firstvpts_show(NULL, NULL, buf);
		break;
	case 17:
		ret = vpause_flag_show(NULL, NULL, buf);
		break;
	case 18:
		ret = slowsync_enable_show(NULL, NULL, buf);
		break;
	case 19:
		ret = startsync_mode_show(NULL, NULL, buf);
		break;
	case 20:
		ret = firstapts_show(NULL, NULL, buf);
		break;
	case 21:
		ret = checkin_firstvpts_show(NULL, NULL, buf);
		break;
	case 22:
		ret = videostarted_show(NULL, NULL, buf);
		break;
	case 23:
		ret = pts_video_u64_show(NULL, NULL, buf);
		break;
	case 24:
		ret = pts_audio_u64_show(NULL, NULL, buf);
		break;
	case 25:
		ret = pts_pcrscr_u64_show(NULL, NULL, buf);
		break;
	case 26:
		ret = last_checkin_vpts_show(NULL, NULL, buf);
		break;
	default:
		ret = -1;
	}
	if (ret > 0 && getbuf) {
		ret = min_t(int, ret, size);
		strncpy(sbuf, buf, ret);
	}
	if (getbuf)
		free_page((unsigned long)getbuf);
	return ret;
}

static struct mconfig tsync_configs[] = {
	MC_FUN_ID("pts_video", tsync_show_fun, tsync_store_fun, 0),
	MC_FUN_ID("pts_audio", tsync_show_fun, tsync_store_fun, 1),
	MC_FUN_ID("dobly_av_sync", tsync_show_fun, tsync_store_fun, 2),
	MC_FUN_ID("pts_pcrscr", tsync_show_fun, tsync_store_fun, 3),
	MC_FUN_ID("event", tsync_show_fun, tsync_store_fun, 4),
	MC_FUN_ID("mode", tsync_show_fun, tsync_store_fun, 5),
	MC_FUN_ID("enable", tsync_show_fun, tsync_store_fun, 6),
	MC_FUN_ID("pcr_recover", tsync_show_fun, tsync_store_fun, 7),
	MC_FUN_ID("discontinue", tsync_show_fun, tsync_store_fun, 8),
	MC_FUN_ID("debug_pts_checkin", tsync_show_fun, tsync_store_fun, 9),
	MC_FUN_ID("debug_pts_checkout", tsync_show_fun, tsync_store_fun, 10),
	MC_FUN_ID("debug_video_pts", tsync_show_fun, tsync_store_fun, 11),
	MC_FUN_ID("debug_audio_pts", tsync_show_fun, tsync_store_fun, 12),
	MC_FUN_ID("av_threshold_min", tsync_show_fun, tsync_store_fun, 13),
	MC_FUN_ID("av_threshold_max", tsync_show_fun, tsync_store_fun, 14),
	MC_FUN_ID("last_checkin_apts", tsync_show_fun, NULL, 15),
	MC_FUN_ID("firstvpts", tsync_show_fun, NULL, 16),
	MC_FUN_ID("vpause_flag", tsync_show_fun, tsync_store_fun, 17),
	MC_FUN_ID("slowsync_enable", tsync_show_fun, tsync_store_fun, 18),
	MC_FUN_ID("startsync_mode", tsync_show_fun, tsync_store_fun, 19),
	MC_FUN_ID("firstapts", tsync_show_fun, tsync_store_fun, 20),
	MC_FUN_ID("checkin_firstvpts", tsync_show_fun, NULL, 21),
	MC_FUN_ID("videostarted", tsync_show_fun, NULL, 22),
	MC_FUN_ID("pts_video_u64", tsync_show_fun, NULL, 23),
	MC_FUN_ID("pts_audio_u64", tsync_show_fun, NULL, 24),
	MC_FUN_ID("pts_pcrscr_u64", tsync_show_fun, NULL, 25),
};

static int tsync_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int tsync_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long tsync_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;
	unsigned int tmppts = 0;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case TSYNC_IOC_SET_TUNNEL_MODE:{
		u32 tunnel_mode;

		if (copy_from_user(&tunnel_mode,
				   argp, sizeof(is_tunnel_mode)) == 0)
			tsync_set_tunnel_mode(tunnel_mode);
		else
			ret = -EFAULT;
		break;
	}
	case TSYNC_IOC_SET_VIDEO_PEEK:
		set_video_peek();
		break;

	case TSYNC_IOC_GET_FIRST_FRAME_TOGGLED:
		put_user(get_first_frame_toggled(), (u32 __user *)argp);
		break;

	case TSYNC_IOC_SET_FIRST_CHECKIN_APTS:
		if (new_arch && get_user(tmppts, (unsigned int *)arg) >= 0) {
			timestamp_checkin_firstapts_set(tmppts);
			if (tsync_get_debug_apts() &&
				tsync_get_debug_pts_checkin())
				pr_info("first check in apts:%x\n", tmppts);
		}
		break;

	case TSYNC_IOC_SET_LAST_CHECKIN_APTS:
		if (new_arch) {
			get_user(checkin_apts_from_audiohal,
				 (unsigned int *)arg);
			if (tsync_get_debug_apts() &&
				tsync_get_debug_pts_checkin())
				pr_info("check in apts:0x%x\n", checkin_apts_from_audiohal);
		}
		break;

	case TSYNC_IOC_SET_DEMUX_INFO:
		if (new_arch &&
		    (copy_from_user((void *)&demux_info, (void *)arg,
				    sizeof(demux_info)) == 0)) {
			tsync_check_pid_valid_for_newarch(demux_info);
			ret = pts_start(PTS_TYPE_VIDEO);
			if (ret < 0) {
				pr_err("pts_start failed, retry\n");
				ret = pts_stop(PTS_TYPE_VIDEO);
				if (ret < 0)
					pr_warn("pts_stop failed when retrying...");
				ret = pts_start(PTS_TYPE_VIDEO);
				if (ret < 0)
					pr_err("pts_start failed\n");
			}
			tsync_pcr_start();
		}
		break;

	case TSYNC_IOC_STOP_TSYNC_PCR:
		if (new_arch) {
			ret = pts_stop(PTS_TYPE_VIDEO);
			if (ret < 0)
				pr_err("pts_stop failed=%ld\n", ret);

			tsync_pcr_stop();
		}
		break;
	default:
		pr_info("invalid cmd:%d\n", cmd);
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long tsync_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;

	switch (cmd) {
	case TSYNC_IOC_SET_TUNNEL_MODE:
	case TSYNC_IOC_SET_VIDEO_PEEK:
	case TSYNC_IOC_GET_FIRST_FRAME_TOGGLED:
	case TSYNC_IOC_SET_FIRST_CHECKIN_APTS:
	case TSYNC_IOC_SET_LAST_CHECKIN_APTS:
	case TSYNC_IOC_SET_DEMUX_INFO:
	case TSYNC_IOC_STOP_TSYNC_PCR:
		return tsync_ioctl(file, cmd, arg);
	default:
		return -EINVAL;
	}

	return ret;
}
#endif

static const struct file_operations tsync_fops = {
	.owner = THIS_MODULE,
	.open = tsync_open,
	.release = tsync_release,
	.unlocked_ioctl = tsync_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tsync_compat_ioctl,
#endif
};

int __init tsync_module_init(void)
{
	int r;
	int chip = -1;

	r = class_register(&tsync_class);

	if (r) {
		amlog_level(LOG_LEVEL_ERROR, "tsync class create fail.\n");
		return r;
	}

	/* create tsync device */
	r = register_chrdev(TSYNC_MAJOR, "tsync", &tsync_fops);
	if (r < 0) {
		pr_info("Can't register major for tsync\n");
		goto err2;
	}

	tsync_dev = device_create(&tsync_class, NULL, MKDEV(TSYNC_MAJOR, 0),
				  NULL, TSYNC_DEVICE_NAME);

	if (IS_ERR(tsync_dev)) {
		amlog_level(LOG_LEVEL_ERROR, "Can't create tsync_dev device\n");
		goto err1;
	}

	/* init audio pts to -1, others to 0 */
	timestamp_apts_set(-1);
	timestamp_vpts_set(0);
	timestamp_vpts_set_u64(0);
	timestamp_pcrscr_set(0);

	timer_setup(&tsync_pcr_recover_timer, tsync_pcr_recover_timer_func, 0);
	tsync_pcr_recover_timer.expires = jiffies + PCR_CHECK_INTERVAL;
	pcr_sync_stat = PCR_SYNC_UNSET;
	pcr_recover_trigger = 0;

	apts_error_num = 0;
	vpts_error_num = 0;

	add_timer(&tsync_pcr_recover_timer);

	timer_setup(&tsync_state_switch_timer, tsync_state_switch_timer_fun, 0);
	tsync_state_switch_timer.expires = jiffies + 1;
	add_timer(&tsync_state_switch_timer);

	REG_PATH_CONFIGS("media.tsync", tsync_configs);

	first_demux_pcrscr = 0;
	demux_pcrscr = 0;
	audio_pid_valid = 0;
	video_pid_valid = 0;
	demux_pcrscr_valid = 0;

	chip = get_cpu_type();
	/*
	 *"new_arch" is true means X4 demux was used.
	 we use the conditions "chip type >= sc2 and exclude some
	 X2 demux chip typs, such as: such as T5/T5D".
	 */
	if (chip >= MESON_CPU_MAJOR_ID_SC2 &&
	    chip != MESON_CPU_MAJOR_ID_T5 &&
	    chip != MESON_CPU_MAJOR_ID_T5D)
		new_arch = true;
	else
		new_arch = false;

	pr_info("new arch is :%d.", new_arch);

	return 0;

err1:
	unregister_chrdev(TSYNC_MAJOR, "tsync");
err2:
	class_unregister(&tsync_class);

	return 0;
}

void __exit tsync_module_exit(void)
{
	del_timer_sync(&tsync_pcr_recover_timer);
	device_destroy(&tsync_class, MKDEV(TSYNC_MAJOR, 0));
	unregister_chrdev(TSYNC_MAJOR, "tsync");
	class_unregister(&tsync_class);
}

MODULE_PARM_DESC(is_tunnel_mode, "\n is_tunnel_mode\n");
module_param(is_tunnel_mode, uint, 0664);
//MODULE_DESCRIPTION("AMLOGIC time sync management driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
