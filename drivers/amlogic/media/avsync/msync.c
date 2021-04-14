/*
 * Copyright (C) 2020 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */
#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/pid.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/major.h>
#include <uapi/linux/amlogic/msync.h>

#define UNIT90K    (90000)
#define DEFAULT_WALL_ADJ_THRES (UNIT90K / 10) //100ms
#define MAX_SESSION_NUM 8
#define CHECK_INTERVAL ((HZ / 10) * 3) //300ms
#define TRANSIT_INTERVAL (HZ) //1s
#define DISC_THRE_MIN (UNIT90K * 3)
#define DISC_THRE_MAX (UNIT90K * 60)

#define TEN_MS_INTERVAL  (HZ / 100)
#define MIN_GAP (UNIT90K * 3)		/* 3s */
#define MAX_GAP (UNIT90K * 3)		/* 4s */
#define MAX_AV_DIFF (UNIT90K * 5)
#define PCR_INVALID_THRES (10 * UNIT90K)
#define PCR_DISC_THRES (UNIT90K * 3 / 2)
#define VALID_PTS(pts) ((pts) != AVS_INVALID_PTS)

enum av_sync_stat {
	AVS_STAT_INIT,
	AVS_STAT_STARTING,
	AVS_STAT_STARTED,
	AVS_STAT_PAUSED,
	AVS_STAT_TRANSITION,
};

enum pcr_init_flag {
	INITCHECK_PCR = 1,
	INITCHECK_VPTS = 2,
	INITCHECK_APTS = 4,
	INITCHECK_DONE = 7, /* all set */
};

enum pcr_init_priority_e {
	INIT_PRIORITY_PCR = 0,
	INIT_PRIORITY_AUDIO = 1,
	INIT_PRIORITY_VIDEO = 2,
};

enum pcr_disc_flag {
	PCR_DISC = 1,
	VIDEO_DISC = 2,
	AUDIO_DISC = 4
};

#define A_DISC_SET(flag) (((flag) & AUDIO_DISC) == AUDIO_DISC)
#define V_DISC_SET(flag) (((flag) & VIDEO_DISC) == VIDEO_DISC)
#define PCR_DISC_SET(flag) (((flag) & PCR_DISC) == PCR_DISC)

struct sync_session {
	u32 id;
	atomic_t refcnt;
	struct list_head node;
	/* mutext for event handling */
	struct mutex session_mutex;
	struct class session_class;
	char name[16];

	/* target mode */
	enum av_sync_mode mode;
	/* current working mode */
	enum av_sync_mode cur_mode;
	enum av_sync_stat stat;
	u32 start_policy;

	bool clock_start;
	/* lock for ISR resource */
	spinlock_t lock;
	u32 wall_clock;
	u32 wall_clock_inc_remainer;
	u32 wall_adj_thres;
	/* 1000 for 1.0f, 2000 for 2.0f, 100 for 0.1f */
	u32 rate;

	bool v_active;
	bool a_active;

	/* pts wrapping */
	u32 freerun_period;
	u32 disc_thres_min;
	u32 disc_thres_max;
	bool v_disc;
	bool a_disc;

	/* pcr master */
	bool use_pcr;
	u32 pcr_clock;
	u32 pcr_init_flag;
	u32 pcr_init_mode;
	struct timer_list pcr_timer;
	bool pcr_timer_added;
	u64 first_time_record;
	u32 last_check_vpts;
	u32 last_check_vpts_cnt;
	u32 last_check_apts;
	u32 last_check_apts_cnt;
	u32 last_check_pcr_clock;
	u32 pcr_disc_flag;
	u32 pcr_disc_cnt;
	u32 pcr_cont_cnt;
	u32 pcr_disc_clock;

	struct pts_tri first_vpts;
	struct pts_tri last_vpts;
	struct pts_tri first_apts;
	struct pts_tri last_apts;

	struct device *session_dev;
	/* wait queue for poll */
	wait_queue_head_t poll_wait;
	bool event_pending;

	/* start policy timer */
	struct timer_list start_timer;
	bool timer_on;
	bool start_posted;

	/* debug */
	bool debug_freerun;
};

struct msync {
	s32 multi_sync_major;
	s32 session_major;
	/* vsync isr context */
	spinlock_t lock;
	/* session management */
	struct list_head head;
	struct device *msync_dev;
	u8 id_pool[MAX_SESSION_NUM];

	/* callback for vout_register_client() */
	struct notifier_block msync_notifier;
	/* ready to receive vsync */
	bool ready;

	/* vout info */
	u32 sync_duration_den;
	u32 sync_duration_num;
	u32 vsync_pts_inc;
};

struct msync_priv {
	int session_id;
};

enum {
	LOG_ERR = 0,
	LOG_WARN = 1,
	LOG_INFO = 2,
	LOG_DEBUG = 3,
	LOG_TRACE = 4,
};

#define msync_dbg(level, x...) \
	do { \
		if ((level) <= log_level) \
			pr_info(x); \
	} while (0)

static struct msync sync;
static int log_level;

static u32 abs_diff(u32 a, u32 b)
{
	return (int)(a - b) > 0 ? a - b : b - a;
}

static void session_set_wall_clock(struct sync_session *session, u32 clock)
{
		unsigned long flags;

		spin_lock_irqsave(&session->lock, flags);
		session->wall_clock = clock;
		session->wall_clock_inc_remainer = 0;
		spin_unlock_irqrestore(&session->lock, flags);
}

static void session_vsync_update(struct sync_session *session)
{
	if (session->clock_start) {
		unsigned long flags;
		u32 temp = 0;
		u32 r = 0;
		u32 den = sync.sync_duration_den;

		den = den * session->rate / 1000;
		spin_lock_irqsave(&session->lock, flags);
		temp = div_u64_rem(90000ULL * den, sync.sync_duration_num, &r);
		session->wall_clock += temp;
		session->wall_clock_inc_remainer += r;
		if (session->wall_clock_inc_remainer >= sync.sync_duration_num) {
			session->wall_clock++;
			session->wall_clock_inc_remainer -= sync.sync_duration_num;
		}
		spin_unlock_irqrestore(&session->lock, flags);
	}
}

int msync_vsync_update(void)
{
	struct sync_session *session;
	struct list_head *p;
	unsigned long flags;

	if (!sync.ready)
		return 0;

	spin_lock_irqsave(&sync.lock, flags);
	list_for_each(p, &sync.head) {
		session = list_entry(p, struct sync_session, node);
		session_vsync_update(session);
	}
	spin_unlock_irqrestore(&sync.lock, flags);
	return 0;
}
EXPORT_SYMBOL(msync_vsync_update);

static void msync_update_mode(void)
{
	u32 inc;
	unsigned long flags;
	const struct vinfo_s *info;

	info = get_current_vinfo();
	/* pre-calculate vsync_pts_inc in 90k unit */
	inc = 90000 * info->sync_duration_den /
		info->sync_duration_num;
	sync.vsync_pts_inc = inc;
	spin_lock_irqsave(&sync.lock, flags);
	sync.sync_duration_den = info->sync_duration_den;
	sync.sync_duration_num = info->sync_duration_num;
	spin_unlock_irqrestore(&sync.lock, flags);
	msync_dbg(0, "vsync_pts_inc %d %d/%d\n", inc,
		sync.sync_duration_den, sync.sync_duration_num);
}

static int msync_notify_callback(struct notifier_block *block,
		unsigned long cmd, void *para)
{
	switch (cmd) {
	case VOUT_EVENT_MODE_CHANGE:
		msync_update_mode();
		break;
	default:
		break;
	}
	return 0;
}

static unsigned int session_poll(struct file *file, poll_table *wait_table)
{
	struct sync_session *session = file->private_data;

	poll_wait(file, &session->poll_wait, wait_table);
	if (session->event_pending)
		return POLLPRI;
	return 0;
}

static void wait_up_poll(struct sync_session *session)
{
	session->event_pending = true;
	wake_up_interruptible(&session->poll_wait);
}

static void transit_timer_func(struct timer_list *t)
{
	struct sync_session *session = from_timer(session, t, start_timer);

	mutex_lock(&session->session_mutex);
	if (session->timer_on) {
		session_set_wall_clock(session, session->last_apts.wall_clock);
		session->stat = AVS_STAT_STARTED;
		session->cur_mode = AVS_MODE_A_MASTER;
		session->timer_on = false;
		wait_up_poll(session);
	}
	mutex_unlock(&session->session_mutex);
}

static void wait_timer_func(struct timer_list *t)
{
	struct sync_session *session = from_timer(session, t, start_timer);

	mutex_lock(&session->session_mutex);
	if (!VALID_PTS(session->first_vpts.pts)) {
		msync_dbg(LOG_WARN, "[%d]wait video timeout\n",
			session->id);
		session->clock_start = true;
	}
	session->timer_on = false;

	if (session->start_policy == AMSYNC_START_ALIGN &&
			!session->start_posted) {
		session->start_posted = true;
		wait_up_poll(session);
	}
	mutex_unlock(&session->session_mutex);
}

static void use_pcr_clock(struct sync_session *session, bool enable, u32 pts)
{
	if (enable) {
		/* use pcr as reference */
		session->use_pcr = true;
		session->clock_start = false;
		session_set_wall_clock(session, session->pcr_clock);
		msync_dbg(LOG_INFO, "[%d]%s clock stop\n",
			session->id, __func__);
	} else {
		/* use wall clock as reference */
		session->use_pcr = false;
		session->clock_start = true;
		session_set_wall_clock(session, pts);
		session->last_vpts.pts = pts;
	}
}

static u32 get_ref_pcr(struct sync_session *session,
	u32 cur_vpts, u32 cur_apts, u32 min_pts)
{
	u32 v_cache_pts = AVS_INVALID_PTS;
	u32 a_cache_pts = AVS_INVALID_PTS;
	u32 first_vpts = AVS_INVALID_PTS;
	u32 first_apts = AVS_INVALID_PTS;
	u32 ref_pcr = AVS_INVALID_PTS;

	if (VALID_PTS(cur_vpts))
		v_cache_pts = cur_vpts - session->first_vpts.pts;
	if (VALID_PTS(cur_apts))
		a_cache_pts = cur_apts - session->first_apts.pts;
	if (VALID_PTS(session->first_vpts.pts))
		first_vpts = session->first_vpts.pts;
	if (VALID_PTS(session->first_apts.pts))
		first_apts = session->first_apts.pts;

	if ((VALID_PTS(first_apts) && VALID_PTS(first_vpts) &&
		first_apts < first_vpts &&
		(first_vpts - first_apts <= MAX_AV_DIFF) &&
		(a_cache_pts < first_vpts - first_apts)) ||
		(!VALID_PTS(first_vpts) && !VALID_PTS(cur_vpts))) {
		//use audio
		if (VALID_PTS(cur_apts))
			ref_pcr = cur_apts;
		else
			ref_pcr = first_apts;
	} else if ((VALID_PTS(first_vpts) && VALID_PTS(first_apts) &&
		first_apts >= first_vpts) ||
		(!VALID_PTS(first_apts) && !VALID_PTS(cur_apts))) {
		//use video
		if (VALID_PTS(cur_vpts))
			ref_pcr = cur_vpts;
		else
			ref_pcr = first_vpts;
	} else if (!VALID_PTS(first_vpts) && !VALID_PTS(first_apts)) {
		//use smaller one
		ref_pcr = min_pts;
	} else if (!VALID_PTS(first_apts) && VALID_PTS(cur_apts) &&
		VALID_PTS(first_vpts) &&
		(first_vpts > cur_apts) &&
		(first_vpts - cur_apts <= MAX_AV_DIFF) &&
		(a_cache_pts < first_vpts - cur_apts)) {
		//use audio
		ref_pcr = cur_apts;
	} else {
		//use video by default
		if (VALID_PTS(cur_vpts))
			ref_pcr = cur_vpts;
		else
			ref_pcr = first_vpts;
	}

	return ref_pcr;
}

/* only handle first arrived A/V/PCR */
static void pcr_set(struct sync_session *session)
{
	u32 cur_pcr = AVS_INVALID_PTS;
	u32 cur_vpts = AVS_INVALID_PTS;
	u32 cur_apts = AVS_INVALID_PTS;
	u32 ref_pcr = AVS_INVALID_PTS;
	u32 min_pts = AVS_INVALID_PTS;

	if (session->pcr_init_flag & INITCHECK_DONE)
		return;

	if (VALID_PTS(session->first_vpts.pts))
		cur_vpts = session->first_vpts.pts;
	if (VALID_PTS(session->first_apts.pts))
		cur_apts = session->first_apts.pts;
	if (VALID_PTS(session->pcr_clock))
		cur_pcr = session->pcr_clock;

	if (VALID_PTS(session->last_vpts.pts))
		cur_vpts = session->last_vpts.pts;
	if (VALID_PTS(session->last_apts.pts))
		cur_apts = session->last_apts.pts;

	if (VALID_PTS(cur_vpts) && VALID_PTS(cur_apts))
		min_pts = min(cur_vpts, cur_apts);

	if (VALID_PTS(cur_pcr) && VALID_PTS(cur_vpts) && VALID_PTS(cur_apts)) {
		u32 gap_pa, gap_pv, gap_av;

		gap_pa = abs_diff(cur_pcr, cur_apts);
		gap_av = abs_diff(cur_apts, cur_vpts);
		gap_pv = abs_diff(cur_pcr, cur_vpts);
		if (gap_pa > MAX_GAP && gap_pv > MAX_GAP) {
			if (gap_av > MAX_GAP)
				ref_pcr = cur_vpts;
			else
				ref_pcr = min_pts;

			session->pcr_init_flag |= INITCHECK_VPTS;
			session->pcr_init_mode = INIT_PRIORITY_VIDEO;
			use_pcr_clock(session, false, ref_pcr);
			msync_dbg(LOG_TRACE, "[%d]%d pcr %u v %u a %u\n",
				session->id, __LINE__,
				cur_pcr, cur_vpts, cur_apts);
			msync_dbg(LOG_TRACE, "[%d]%d disable pcr %u\n",
				session->id, __LINE__, ref_pcr);
			return;
		}
		if (cur_vpts < cur_pcr && cur_vpts < cur_apts) {
			session->pcr_init_flag |= INITCHECK_VPTS;
			session->pcr_init_mode = INIT_PRIORITY_VIDEO;
			use_pcr_clock(session, false, cur_vpts);
			msync_dbg(LOG_TRACE, "[%d]%d disable pcr %u\n",
				session->id, __LINE__, cur_vpts);
			return;
		}
	}

	if (VALID_PTS(cur_pcr) && VALID_PTS(min_pts)) {
		session->pcr_init_flag |= INITCHECK_PCR;
		if (abs_diff(cur_pcr, cur_vpts) > PCR_INVALID_THRES) {
			if (VALID_PTS(session->first_vpts.pts))
				ref_pcr = session->first_vpts.pts;
			else
				ref_pcr = cur_vpts;
			session->pcr_init_mode = INIT_PRIORITY_VIDEO;
			use_pcr_clock(session, false, ref_pcr);
			msync_dbg(LOG_TRACE, "[%d]%d disable pcr %u\n",
				session->id, __LINE__, ref_pcr);
		} else if (((cur_pcr > min_pts) &&
			   (cur_pcr - min_pts) > (UNIT90K / 2)) ||
			   session->use_pcr) {
			if (abs_diff(cur_apts, cur_vpts) > MAX_GAP) {
				if (VALID_PTS(session->first_vpts.pts))
					ref_pcr = session->first_vpts.pts;
				else
					ref_pcr = cur_vpts;
			} else {
				ref_pcr = min_pts;
			}
			session->pcr_init_mode = INIT_PRIORITY_VIDEO;
			use_pcr_clock(session, false, ref_pcr);
			msync_dbg(LOG_TRACE, "[%d]%d disable pcr %u\n",
				session->id, __LINE__, ref_pcr);
		} else {
			session->pcr_init_mode = INIT_PRIORITY_PCR;
			use_pcr_clock(session, true, 0);
			msync_dbg(LOG_TRACE, "[%d]%d enable pcr %u\n",
				session->id, __LINE__, ref_pcr);
		}
		return;
	}

	if (VALID_PTS(session->first_apts.pts) && VALID_PTS(min_pts)) {
		session->pcr_init_flag |= INITCHECK_APTS;
		ref_pcr = get_ref_pcr(session, cur_vpts, cur_apts, min_pts);
		session->pcr_init_mode = INIT_PRIORITY_AUDIO;
		use_pcr_clock(session, false, ref_pcr);
		msync_dbg(LOG_TRACE, "[%d]%d disable pcr %u\n",
			session->id, __LINE__, ref_pcr);
	}

	if (VALID_PTS(session->first_vpts.pts) && VALID_PTS(min_pts)) {
		session->pcr_init_flag |= INITCHECK_VPTS;
		ref_pcr = get_ref_pcr(session, cur_vpts, cur_apts, min_pts);
		session->pcr_init_mode = INIT_PRIORITY_VIDEO;
		use_pcr_clock(session, false, ref_pcr);
		msync_dbg(LOG_TRACE, "[%d]%d disable pcr %u\n",
			session->id, __LINE__, ref_pcr);
	}
}

static u32 pcr_get(struct sync_session *session)
{
	if (session->use_pcr)
		return session->pcr_clock;
	else
		return session->wall_clock;
}

static void update_f_vpts(struct sync_session *session, u32 pts)
{
	session->first_vpts.wall_clock = pts;
	session->first_vpts.pts = pts;
}

static void update_f_apts(struct sync_session *session, u32 pts)
{
	session->first_apts.wall_clock = pts;
	session->first_apts.pts = pts;
}

static void session_video_start(struct sync_session *session, u32 pts)
{
	session->v_active = true;
	mutex_lock(&session->session_mutex);
	if (session->mode == AVS_MODE_V_MASTER) {
		session_set_wall_clock(session, pts);
		session->clock_start = true;
		session->stat = AVS_STAT_STARTED;
		update_f_vpts(session, pts);
		msync_dbg(LOG_INFO, "[%d]%d video start %u\n",
			session->id, __LINE__, pts);
	} else if (session->mode == AVS_MODE_A_MASTER) {
		update_f_vpts(session, pts);

		if (session->start_policy == AMSYNC_START_ALIGN &&
			VALID_PTS(session->first_apts.pts)) {
			if (!session->clock_start) {
				session->clock_start = true;
				session->stat = AVS_STAT_STARTED;
				del_timer_sync(&session->start_timer);
				session->timer_on = false;
				session->start_posted = true;
				wait_up_poll(session);
				msync_dbg(LOG_INFO, "[%d]%d video start %u\n",
					session->id, __LINE__, pts);
			}
		}
	} else if (session->mode == AVS_MODE_IPTV) {
		update_f_vpts(session, pts);

		if (session->start_policy == AMSYNC_START_ASAP &&
				!VALID_PTS(session->first_apts.pts)) {
			session_set_wall_clock(session, pts);
			session->clock_start = true;
			session->stat = AVS_STAT_STARTED;
			session->cur_mode = AVS_MODE_V_MASTER;
			wait_up_poll(session);
			msync_dbg(LOG_INFO, "[%d]%d video start %u\n",
					session->id, __LINE__, pts);
		} else if (session->start_policy == AMSYNC_START_ASAP) {
			msync_dbg(LOG_INFO, "[%d]%d video start %u keep AMASTER\n",
					session->id, __LINE__, pts);
		}
	} else if (session->mode == AVS_MODE_PCR_MASTER) {
		update_f_vpts(session, pts);

		pcr_set(session);
	}
	mutex_unlock(&session->session_mutex);
}

static u32 session_audio_start(struct sync_session *session,
	const struct audio_start *start)
{
	u32 ret = AVS_START_SYNC;
	u32 pts = start->pts;
	u32 start_pts = start->pts - start->delay;

	session->a_active = true;
	mutex_lock(&session->session_mutex);
	if (session->mode == AVS_MODE_A_MASTER) {
		session_set_wall_clock(session, start_pts);
		update_f_apts(session, pts);

		if (session->start_policy == AMSYNC_START_ALIGN &&
			!VALID_PTS(session->first_vpts.pts)) {
			msync_dbg(LOG_INFO, "[%d]%d audio start %u deferred\n",
					session->id, __LINE__, pts);
			if (session->timer_on)
				del_timer_sync(&session->start_timer);
			session->start_timer.function = wait_timer_func;
			session->start_timer.expires = jiffies + CHECK_INTERVAL;
			add_timer(&session->start_timer);
			session->timer_on = true;
			session->stat = AVS_STAT_STARTING;
			ret = AVS_START_ASYNC;
		} else {
			session->clock_start = true;
			session->stat = AVS_STAT_STARTED;
			msync_dbg(LOG_INFO, "[%d]%d audio start %u\n",
					session->id, __LINE__, pts);
			if (session->start_policy == AMSYNC_START_ALIGN &&
					!session->start_posted) {
				session->start_posted = true;
				wait_up_poll(session);
			}
		}
	} else if (session->mode == AVS_MODE_IPTV) {
		update_f_apts(session, pts);

		if (session->start_policy == AMSYNC_START_ASAP &&
			!VALID_PTS(session->first_vpts.pts)) {
			session_set_wall_clock(session, start_pts);
			session->clock_start = true;
			session->stat = AVS_STAT_STARTED;
			session->cur_mode = AVS_MODE_A_MASTER;
			wait_up_poll(session);
			msync_dbg(LOG_INFO, "[%d]%d audio start %u\n",
					session->id, __LINE__, pts);
		} else if (session->start_policy == AMSYNC_START_ASAP) {
			/* transition start from V to A master */
			session->stat = AVS_STAT_TRANSITION;
			/* enter grace period */
			if (session->timer_on)
				del_timer_sync(&session->start_timer);
			session->start_timer.function = transit_timer_func;
			session->start_timer.expires = jiffies + TRANSIT_INTERVAL;
			add_timer(&session->start_timer);
			session->timer_on = true;

			wait_up_poll(session);
			msync_dbg(LOG_INFO, "[%d]%d audio start %u\n",
					session->id, __LINE__, pts);
		}
	} else if (session->mode == AVS_MODE_PCR_MASTER) {
		update_f_apts(session, pts);
		session_set_wall_clock(session, start_pts);
		session->clock_start = true;
		msync_dbg(LOG_INFO, "[%d]%d audio start %u\n",
			session->id, __LINE__, pts);
	}
	mutex_unlock(&session->session_mutex);
	return ret;
}

static void session_pause(struct sync_session *session, bool pause)
{
	mutex_lock(&session->session_mutex);
	if (session->stat == AVS_STAT_STARTING) {
		msync_dbg(LOG_INFO, "[%d]%s ignore pause %d during starting\n",
			session->id, __func__, pause);
	} else {
		session->clock_start = !pause;
		msync_dbg(LOG_INFO, "[%d]%s pause %d\n",
			session->id, __func__, pause);
		if (pause)
			session->stat = AVS_STAT_PAUSED;
		else
			session->stat = AVS_STAT_STARTED;
	}
	mutex_unlock(&session->session_mutex);
}

static void session_video_stop(struct sync_session *session)
{
	session->v_active = false;
	update_f_vpts(session, AVS_INVALID_PTS);

	mutex_lock(&session->session_mutex);
	if (!session->a_active) {
		session->clock_start = false;
		msync_dbg(LOG_INFO, "[%d]%s clock stop\n",
			session->id, __func__);
	} else if (session->mode == AVS_MODE_IPTV) {
		session->cur_mode = AVS_MODE_A_MASTER;
		session_set_wall_clock(session, session->last_apts.wall_clock);
		msync_dbg(LOG_INFO, "[%d]%s to Amaster\n",
				session->id, __func__);
	} else if (session->mode == AVS_MODE_PCR_MASTER) {
		if (!session->a_active)
			use_pcr_clock(session, true, 0);
		session->first_vpts.pts = AVS_INVALID_PTS;
		session->last_vpts.pts = AVS_INVALID_PTS;
		session->pcr_disc_clock = AVS_INVALID_PTS;
		session->last_check_vpts_cnt = 0;
		session->pcr_disc_flag = 0;
		session->first_time_record =
			div64_u64((u64)jiffies * UNIT90K, HZ);
	}
	wait_up_poll(session);
	mutex_unlock(&session->session_mutex);
}

static void session_audio_stop(struct sync_session *session)
{
	session->a_active = false;
	update_f_apts(session, AVS_INVALID_PTS);

	mutex_lock(&session->session_mutex);
	if (!session->v_active) {
		session->clock_start = false;
		msync_dbg(LOG_INFO, "[%d]%s clock stop\n",
			session->id, __func__);
	} else if (session->mode == AVS_MODE_IPTV) {
		session->cur_mode = AVS_MODE_V_MASTER;
		session_set_wall_clock(session, session->last_vpts.wall_clock);

		if (session->timer_on) {
			del_timer_sync(&session->start_timer);
			session->timer_on = false;
		}
		msync_dbg(LOG_INFO, "[%d]%s to Vmaster\n",
				session->id, __func__);
	} else if (session->mode == AVS_MODE_IPTV) {
		if (!session->v_active)
			use_pcr_clock(session, true, 0);
		session->first_apts.pts = AVS_INVALID_PTS;
		session->last_apts.pts = AVS_INVALID_PTS;
		session->last_check_apts_cnt = 0;
	}
	wait_up_poll(session);
	mutex_unlock(&session->session_mutex);
}

/*
 *Video and audio PTS discontinuity happen typically with a loopback
 *playback, with same bit stream play in loop and PTS wrap back from
 *starting point.
 *When video discontinuity happens first, wall clock is set
 *immediately to keep video running in VMATSER mode. This
 *mode is restored to AMASTER when audio discontinuity follows,
 *or apts is close to wall clock in a later time.
 *When audio discontinuity happens first, VMASTER mode is
 *set to keep video running w/o setting wall clock. This mode
 *is restored to AMASTER when video discontinuity follows.
 *And wall clock is restored along with video time stamp.
 */
static void session_video_disc_iptv(struct sync_session *session, u32 pts)
{
	u32 last_pts = session->last_vpts.pts;
	u32 wall = session->wall_clock;

	if (session->mode != AVS_MODE_PCR_MASTER &&
		session->mode != AVS_MODE_IPTV)
		return;

	mutex_lock(&session->session_mutex);
	if (VALID_PTS(session->pcr_clock))
		wall = session->pcr_clock;
	if (VALID_PTS(session->pcr_clock) &&
		abs_diff(pts, last_pts) > session->disc_thres_min) {
		session->v_disc = true;
		if (session->a_disc) {
			session->v_disc = false;
			session->a_disc = false;
			session_set_wall_clock(session, pts);
			msync_dbg(LOG_INFO, "[%d]%s reset wall %u\n",
				session->id, __func__, pts);
		} else {
			session_set_wall_clock(session, last_pts);
			msync_dbg(LOG_INFO, "[%d]%s reset wall %u\n",
				session->id, __func__, pts);
		}
		goto exit;
	}
	if (abs_diff(pts, last_pts) > session->disc_thres_min)
		session->v_disc = true;
exit:
	mutex_unlock(&session->session_mutex);
}

static bool pcr_v_disc(struct sync_session *session, u32 pts)
{
	u32 pcr = pcr_get(session);

	if (!VALID_PTS(pcr))
		return false;
	if (pts > pcr && (pts - pcr) > session->disc_thres_min)
		return true;
	if (pcr > pts && (pcr - pts) > session->disc_thres_min)
		return true;
	return false;
}

static void session_video_disc_pcr(struct sync_session *session, u32 pts)
{
	mutex_lock(&session->session_mutex);
	if (session->pcr_init_mode != INIT_PRIORITY_PCR) {
		/* set wall clock */
		session_set_wall_clock(session, pts);
	} else if (pcr_v_disc(session, pts)) {
		u32 pcr = pcr_get(session);

		if (session->use_pcr) {
			if (pcr + 10 * UNIT90K < pts)
				use_pcr_clock(session, false, pts);
			else
				session_set_wall_clock(session, pts);
		} else {
			if (pcr > pts)
				use_pcr_clock(session, true, 0);
			else
				session_set_wall_clock(session, pts);
		}
	}
	session->last_vpts.pts = pts;
	session->last_vpts.wall_clock = session->wall_clock;
	mutex_unlock(&session->session_mutex);
}

static void session_video_disc_v(struct sync_session *session, u32 pts)
{
	mutex_lock(&session->session_mutex);

	msync_dbg(LOG_WARN, "[%d]vdisc reset wall %u --> %u\n",
			session->id, session->wall_clock, pts);
	session_set_wall_clock(session, pts);
	session->last_vpts.pts = pts;
	session->last_vpts.wall_clock = session->wall_clock;

	mutex_unlock(&session->session_mutex);
}

static void session_audio_disc(struct sync_session *session, u32 pts)
{
	//u32 last_pts = session->last_apts.pts;

	if (session->mode != AVS_MODE_PCR_MASTER &&
		session->mode != AVS_MODE_IPTV)
		return;
	if (session->mode == AVS_MODE_PCR_MASTER) {
		session->pcr_disc_flag |= AUDIO_DISC;
		session->last_apts.pts = pts;
		session->last_apts.wall_clock = session->wall_clock;
	}
}

static void session_update_vpts(struct sync_session *session)
{
#if 0
	if (session->mode == AVS_MODE_V_MASTER) {
		struct pts_tri *p = &session->last_vpts;
		u32 pts = p->pts;

		if (pts > p->delay)
			pts -= p->delay;
		if (abs_diff(pts, session->wall_clock) >=
			session->wall_adj_thres) {
			unsigned long flags;

			msync_dbg(LOG_WARN, "[%d]v reset wall %u --> %u\n",
				session->id, session->wall_clock, pts);
			/* correct wall with vpts */
			spin_lock_irqsave(&sync.lock, flags);
			session->wall_clock = pts;
			spin_unlock_irqrestore(&sync.lock, flags);
		}
	}
#endif
}

static void session_update_apts(struct sync_session *session)
{
	if (session->mode == AVS_MODE_A_MASTER) {
		struct pts_tri *p = &session->last_apts;
		u32 pts = p->pts;

		if (session->debug_freerun)
			return;
		if (pts > p->delay)
			pts -= p->delay;
		if (abs_diff(pts, session->wall_clock) >=
			session->wall_adj_thres) {
			unsigned long flags;

			/* correct wall with apts */
			msync_dbg(LOG_WARN, "[%d]a reset wall %u --> %u\n",
				session->id, session->wall_clock, pts);
			spin_lock_irqsave(&sync.lock, flags);
			session->wall_clock = pts;
			spin_unlock_irqrestore(&sync.lock, flags);
		}
	}
}

static void pcr_check(struct sync_session *session)
{
	u32 checkin_vpts = AVS_INVALID_PTS;
	u32 checkin_apts = AVS_INVALID_PTS;
	int max_gap = 40;
	u32 pcr_diff = 0;

	if (session->a_active) {
		checkin_apts = session->last_apts.pts;
		if (VALID_PTS(checkin_apts) &&
			VALID_PTS(session->last_check_apts)) {
			/* apts timeout */
			if (abs_diff(session->last_check_apts, checkin_apts)
				> 2 * UNIT90K) {
				session->pcr_disc_flag |= AUDIO_DISC;
				msync_dbg(LOG_DEBUG, "[%d] adisc\n", __LINE__);
			}
			if (session->last_check_apts == checkin_apts) {
				session->last_check_apts_cnt++;
				if (session->last_check_apts_cnt > max_gap) {
					session->pcr_disc_flag |= AUDIO_DISC;
					session->last_check_apts_cnt = 0;
				}
			} else {
				session->last_check_apts = checkin_apts;
				session->last_check_apts_cnt = 0;
			}
			if (A_DISC_SET(session->pcr_disc_flag))
				msync_dbg(LOG_WARN, "[%d] adisc %x\n",
					__LINE__, session->pcr_disc_flag);
		} else {
			session->last_check_apts = checkin_apts;
		}
	}

	if (session->v_active) {
		checkin_vpts = session->last_vpts.pts;
		if (VALID_PTS(checkin_vpts) &&
			VALID_PTS(session->last_check_vpts)) {
			/* vpts timeout */
			if (abs_diff(session->last_check_vpts, checkin_vpts)
				> 2 * UNIT90K)
				session->pcr_disc_flag |= VIDEO_DISC;
			if (session->last_check_vpts == checkin_vpts) {
				session->last_check_vpts_cnt++;
				if (session->last_check_vpts_cnt > max_gap) {
					session->pcr_disc_flag |= VIDEO_DISC;
					session->last_check_vpts_cnt = 0;
				}
			} else {
				session->last_check_vpts = checkin_vpts;
				session->last_check_vpts_cnt = 0;
			}
			if (V_DISC_SET(session->pcr_disc_flag))
				msync_dbg(LOG_WARN, "[%d] vdisc\n", __LINE__);
		} else {
			session->last_check_vpts = checkin_vpts;
		}
	}

	if (session->use_pcr) {
		if (VALID_PTS(session->pcr_clock) &&
			VALID_PTS(session->last_check_pcr_clock)) {
			/* pcr timeout */
			pcr_diff = abs_diff(session->pcr_clock,
				session->last_check_pcr_clock);
			if (pcr_diff > PCR_DISC_THRES &&
				session->pcr_init_flag >= INITCHECK_PCR) {
				session->pcr_disc_flag |= PCR_DISC;
				session->pcr_disc_clock = session->pcr_clock;
				msync_dbg(LOG_WARN, "[%d] pdisc", __LINE__);
			} else if (PCR_DISC_SET(session->pcr_disc_flag)) {
				if (abs_diff(session->pcr_clock,
					session->pcr_disc_clock) >
					(4 * UNIT90K)) {
					/* to pause the pcr check */
					session->pcr_disc_flag = 0;
					session->pcr_disc_clock =
						AVS_INVALID_PTS;
					msync_dbg(LOG_WARN, "[%d] pdisc reset",
							__LINE__);
				}
			}
			session->last_check_pcr_clock = session->pcr_clock;
		}
	}

	if (!session->pcr_init_flag) {
		u64 cur_time = div64_u64((u64)jiffies * UNIT90K, HZ);

		if (!VALID_PTS(checkin_apts) && !VALID_PTS(checkin_vpts)) {
			session->first_time_record = cur_time;
		} else if ((cur_time - session->first_time_record <
				3 * UNIT90K) && session->v_active) {
			//do nothing
		} else {
			if (session->v_active)
				session->pcr_init_mode = INIT_PRIORITY_VIDEO;
			else
				session->pcr_init_mode = INIT_PRIORITY_AUDIO;
			pcr_set(session);
		}
		return;
	}
	//TODO tsync_process_discontinue

	if (session->pcr_init_mode != INIT_PRIORITY_PCR) {
		if (V_DISC_SET(session->pcr_disc_flag) &&
			VALID_PTS(checkin_apts) &&
			VALID_PTS(checkin_vpts)) {
#if 0 //none sense not used anywhere
			if (abs_diff(checkin_apts, checkin_vpts) > MAX_GAP)
				ref_pcr = checkin_vpts;
			else
				ref_pcr = min(checkin_vpts, checkin_apts);
#endif
			use_pcr_clock(session, false, checkin_vpts);
			session->pcr_disc_flag = 0;
		}
		return;
	}

	if (VALID_PTS(session->pcr_clock) &&
		VALID_PTS(session->last_check_pcr_clock)) {
		pcr_diff = abs_diff(session->pcr_clock,
				session->last_check_pcr_clock);
		if (pcr_diff > PCR_DISC_THRES) {
			session->last_check_pcr_clock = session->pcr_clock;
			session->pcr_disc_cnt++;
			session->pcr_cont_cnt = 0;
		} else {
			session->pcr_cont_cnt++;
			session->pcr_disc_cnt = 0;
			session->last_check_pcr_clock = session->pcr_clock;
			if (!session->use_pcr &&
				abs_diff(session->pcr_clock, checkin_vpts) <
				session->disc_thres_min &&
				session->pcr_cont_cnt > 10) {
				use_pcr_clock(session, true, 0);
				session->pcr_disc_flag = 0;
				return;
			}
		}
	}

	if (session->pcr_disc_cnt > 100 && VALID_PTS(checkin_vpts)) {
		msync_dbg(LOG_INFO, "[%d]pcr_disc reset\n", __LINE__);
		session->pcr_init_mode = INIT_PRIORITY_VIDEO;
		use_pcr_clock(session, false, checkin_vpts);
	}

	if (V_DISC_SET(session->pcr_disc_flag) &&
		!PCR_DISC_SET(session->pcr_disc_flag)) {
		session->pcr_disc_flag = 0;
		if (VALID_PTS(checkin_vpts) && session->use_pcr &&
			pcr_v_disc(session, checkin_vpts))
			use_pcr_clock(session, false, checkin_vpts);
		else if (!session->use_pcr && session->pcr_cont_cnt > 100)
			use_pcr_clock(session, true, 0);
		else
			return;
	} else if (!V_DISC_SET(session->pcr_disc_flag) &&
		PCR_DISC_SET(session->pcr_disc_flag) &&
		session->use_pcr) {
		session_set_wall_clock(session, 0);
		session->clock_start = true;
		session->use_pcr = false;
	} else if (V_DISC_SET(session->pcr_disc_flag) &&
		PCR_DISC_SET(session->pcr_disc_flag)) {
		session->use_pcr = true;
		session->pcr_disc_flag = 0;
	}
}

static void pcr_timer_func(struct timer_list *t)
{
	struct sync_session *session = from_timer(session, t, start_timer);

	pcr_check(session);

	session->pcr_timer.expires =
		(unsigned long)(jiffies + TEN_MS_INTERVAL);

	add_timer(&session->pcr_timer);
}

static const char *event_dbg[AVS_EVENT_MAX] = {
	"video_start",
	"pause",
	"resume",
	"video_stop",
	"audio_stop",
	"video_disc",
	"audio_disc",
};

static void session_handle_event(struct sync_session *session,
	const struct session_event *event)
{
	msync_dbg(LOG_DEBUG, "[%d]event %s/%d\n",
		session->id,
		event_dbg[event->event], event->value);
	switch (event->event) {
	case AVS_VIDEO_START:
		session_video_start(session, event->value);
		break;
	case AVS_PAUSE:
		session_pause(session, true);
		break;
	case AVS_RESUME:
		session_pause(session, false);
		break;
	case AVS_VIDEO_STOP:
		session_video_stop(session);
		break;
	case AVS_AUDIO_STOP:
		session_audio_stop(session);
		break;
	case AVS_VIDEO_TSTAMP_DISCONTINUITY:
		if (session->mode == AVS_MODE_IPTV)
			session_video_disc_iptv(session, event->value);
		else if (session->mode == AVS_MODE_PCR_MASTER)
			session_video_disc_pcr(session, event->value);
		else if (session->mode == AVS_MODE_V_MASTER)
			session_video_disc_v(session, event->value);
		break;
	case AVS_AUDIO_TSTAMP_DISCONTINUITY:
		session_audio_disc(session, event->value);
		break;
	default:
		break;
	}
}

static long session_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	struct sync_session *session = file->private_data;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case AMSYNCS_IOC_SET_MODE:
	{
		get_user(session->mode, (u32 __user *)argp);
		msync_dbg(LOG_INFO, "session[%d] mode %d\n",
			session->id, session->mode);
		mutex_lock(&session->session_mutex);
		if (session->mode == AVS_MODE_PCR_MASTER &&
			!session->pcr_timer_added) {
			session->use_pcr = true;
			session->pcr_init_mode = INIT_PRIORITY_PCR;

			if (session->pcr_timer_added)
				del_timer_sync(&session->pcr_timer);
			session->pcr_timer.function = pcr_timer_func;
			session->pcr_timer.expires = jiffies;

			session->first_time_record =
				div64_u64((u64)jiffies * UNIT90K, HZ);
			add_timer(&session->pcr_timer);
			session->pcr_timer_added = true;
		}
		mutex_unlock(&session->session_mutex);
		break;
	}
	case AMSYNCS_IOC_GET_MODE:
		put_user(session->mode, (u32 __user *)argp);
		break;
	case AMSYNCS_IOC_SET_START_POLICY:
		get_user(session->start_policy, (u32 __user *)argp);
		msync_dbg(LOG_INFO, "session[%d] start policy %d\n",
			session->id, session->start_policy);
		break;
	case AMSYNCS_IOC_GET_START_POLICY:
		put_user(session->start_policy, (u32 __user *)argp);
		break;
	case AMSYNCS_IOC_SET_V_TS:
	{
		struct pts_tri ts;

		if (!copy_from_user(&ts, argp, sizeof(ts))) {
			session->last_vpts = ts;
			session_update_vpts(session);
		}
		break;
	}
	case AMSYNCS_IOC_GET_V_TS:
	{
		if (copy_to_user(argp, &session->last_vpts, sizeof(struct pts_tri)))
			return -EFAULT;
		break;
	}
	case AMSYNCS_IOC_SET_A_TS:
	{
		struct pts_tri ts;

		if (!copy_from_user(&ts, argp, sizeof(ts))) {
			session->last_apts = ts;
			session_update_apts(session);
		}
		break;
	}
	case AMSYNCS_IOC_GET_A_TS:
	{
		if (copy_to_user(argp, &session->last_apts, sizeof(struct pts_tri)))
			return -EFAULT;
		break;
	}
	case AMSYNCS_IOC_SEND_EVENT:
	{
		struct session_event event;

		if (!copy_from_user(&event, argp, sizeof(event)))
			session_handle_event(session, &event);
		break;
	}
	case AMSYNCS_IOC_GET_SYNC_STAT:
	{
		struct session_sync_stat stat;

		if (session->mode != AVS_MODE_PCR_MASTER &&
				session->mode != AVS_MODE_A_MASTER &&
				session->mode != AVS_MODE_IPTV)
			return -EINVAL;
		stat.v_active = session->v_active;
		stat.a_active = session->a_active;
		stat.mode = session->cur_mode;
		if (copy_to_user(argp, &stat, sizeof(stat)))
			return -EFAULT;
		session->event_pending = false;
		break;
	}
	case AMSYNCS_IOC_SET_PCR:
	{
		u32 pcr_clock;

		mutex_lock(&session->session_mutex);
		get_user(pcr_clock, (u32 __user *)argp);
		if (!VALID_PTS(session->pcr_clock))
			msync_dbg(LOG_INFO, "[%d]pcr set %u\n",
				__LINE__, pcr_clock);
		session->pcr_clock = pcr_clock;
		mutex_unlock(&session->session_mutex);
		break;
	}
	case AMSYNCS_IOC_GET_PCR:
		put_user(session->wall_clock, (u32 __user *)argp);
		break;
	case AMSYNCS_IOC_GET_WALL:
	{
		struct pts_wall wall;

		wall.interval = sync.vsync_pts_inc;
		if (session->mode != AVS_MODE_PCR_MASTER) {
			if (session->clock_start)
				wall.wall_clock = session->wall_clock;
			else
				wall.wall_clock = AVS_INVALID_PTS;
		} else {
			if (session->use_pcr)
				wall.wall_clock = session->pcr_clock;
			else
				wall.wall_clock = session->wall_clock;
		}
		if (copy_to_user(argp, &wall, sizeof(struct pts_wall)))
			return -EFAULT;
		break;
	}
	case AMSYNCS_IOC_SET_RATE:
	{
		u32 rate;

		get_user(rate, (u32 __user *)argp);
		if (rate == 0 || rate > 10 * 1000) {
			msync_dbg(LOG_ERR, "[%d]wrong rate %u\n",
				session->id, rate);
			return -EINVAL;
		}
		if (session->rate == rate)
			return 0;
		msync_dbg(LOG_WARN, "[%d]rate %u --> %u\n",
			session->id, session->rate, rate);
		session->rate = rate;
		if (session->mode == AVS_MODE_A_MASTER)
			wait_up_poll(session);
		break;
	}
	case AMSYNCS_IOC_GET_RATE:
		put_user(session->rate, (u32 __user *)argp);
		break;
	case AMSYNCS_IOC_SET_NAME:
		strncpy_from_user(session->name,
			(const char __user *)argp, sizeof(session->name));
		break;
	case AMSYNCS_IOC_SET_WALL_ADJ_THRES:
		get_user(session->wall_adj_thres, (u32 __user *)argp);
		msync_dbg(LOG_WARN, "[%d]wall_adj_thres %d\n",
			session->id, session->wall_adj_thres);
		break;
	case AMSYNCS_IOC_GET_WALL_ADJ_THRES:
		put_user(session->wall_adj_thres, (u32 __user *)argp);
		break;
	case AMSYNCS_IOC_GET_CLOCK_START:
		put_user(session->clock_start, (u32 __user *)argp);
		break;
	case AMSYNCS_IOC_AUDIO_START:
	{
		struct audio_start start;

		if (!copy_from_user(&start, argp, sizeof(struct audio_start))) {
			start.mode = session_audio_start(session, &start);
			msync_dbg(LOG_DEBUG, "[%d]audio start mode %u\n",
				session->id, start.mode);
		}
		if (copy_to_user(argp, &start, sizeof(struct audio_start)))
			return -EFAULT;
		break;
	}
	default:
		break;
	}
	return 0;
}

#ifdef CONFIG_COMPAT
static long session_compat_ioctl(struct file *filp, u32 cmd, ulong arg)
{
	long ret;

	arg = (ulong)compat_ptr(arg);
	ret = session_ioctl(filp, cmd, arg);
	return ret;
}
#endif

static int session_open(struct inode *inode, struct file *file)
{
	struct sync_session *session = NULL;
	struct list_head *p;
	int id = iminor(inode);
	unsigned long flags;

	spin_lock_irqsave(&sync.lock, flags);
	list_for_each(p, &sync.head) {
		session = list_entry(p, struct sync_session, node);
		if (session->id == id)
			break;
	}
	spin_unlock_irqrestore(&sync.lock, flags);

	if (!session) {
		msync_dbg(LOG_ERR, "can not find %d\n", id);
		return -EBADF;
	}

	atomic_inc(&session->refcnt);
	msync_dbg(LOG_DEBUG, "session[%d] open pid[%d]\n",
		id, task_tgid_nr(current));
	file->private_data = session;
	return 0;
}

static void free_session(struct sync_session *session)
{
	unsigned long flags;

	msync_dbg(LOG_INFO, "free session[%d]\n", session->id);
	mutex_lock(&session->session_mutex);
	if (session->timer_on)
		del_timer_sync(&session->start_timer);
	if (session->pcr_timer_added)
		del_timer_sync(&session->pcr_timer);
	mutex_unlock(&session->session_mutex);

	device_destroy(&session->session_class,
		MKDEV(AMSYNC_SESSION_MAJOR, session->id));
	class_unregister(&session->session_class);
	vfree(session->session_class.name);

	spin_lock_irqsave(&sync.lock, flags);
	sync.id_pool[session->id] = 0;
	spin_unlock_irqrestore(&sync.lock, flags);

	vfree(session);
}

static int session_release(struct inode *inode, struct file *file)
{
	struct sync_session *session = file->private_data;

	msync_dbg(LOG_DEBUG, "session[%d] close pid[%d]\n",
		session->id, task_tgid_nr(current));
	file->private_data = NULL;
	if (atomic_dec_and_test(&session->refcnt))
		free_session(session);
	return 0;
}

static const struct file_operations session_fops = {
	.owner = THIS_MODULE,
	.open = session_open,
	.release = session_release,
	.unlocked_ioctl = session_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = session_compat_ioctl,
#endif
	.poll = session_poll,
};

static ssize_t session_stat_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	struct sync_session *session;

	session = container_of(cla, struct sync_session, session_class);
	return sprintf(buf,
		"active v/%d a/%d\n"
		"first v/%x a/%x\n"
		"last  v/%x a/%x\n"
		"start %d r %d\n"
		"w %x pcr(%c) %x\n",
		session->v_active, session->a_active,
		session->first_vpts.pts,
		session->first_apts.pts,
		session->last_vpts.pts,
		session->last_apts.pts,
		session->clock_start, session->rate,
		session->wall_clock,
		session->use_pcr ? 'y' : 'n',
		session->pcr_clock);
}

static ssize_t disc_thres_min_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	struct sync_session *session;

	session = container_of(cla, struct sync_session, session_class);
	return sprintf(buf, "%d", session->disc_thres_min);
}

static ssize_t disc_thres_min_store(struct class *cla,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct sync_session *session;
	size_t r;

	session = container_of(cla, struct sync_session, session_class);
	r = kstrtoint(buf, 0, &session->disc_thres_min);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t disc_thres_max_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	struct sync_session *session;

	session = container_of(cla, struct sync_session, session_class);
	return sprintf(buf, "%d", session->disc_thres_max);
}

static ssize_t disc_thres_max_store(struct class *cla,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct sync_session *session;
	size_t r;

	session = container_of(cla, struct sync_session, session_class);
	r = kstrtoint(buf, 0, &session->disc_thres_max);
	if (r != 0)
		return -EINVAL;
	return count;
}

static ssize_t session_free_run_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	struct sync_session *session;

	session = container_of(cla, struct sync_session, session_class);
	return sprintf(buf, "%u", session->debug_freerun);
}

static ssize_t session_free_run_store(struct class *cla,
		struct class_attribute *attr, const char *buf, size_t count)
{
	struct sync_session *session;
	size_t r;

	session = container_of(cla, struct sync_session, session_class);
	r = kstrtobool(buf, &session->debug_freerun);
	if (r != 0)
		return -EINVAL;
	return count;
}

static struct class_attribute session_attrs[] = {
	__ATTR_RO(session_stat),
	__ATTR_RW(disc_thres_min),
	__ATTR_RW(disc_thres_max),
	__ATTR_RW(session_free_run),
	__ATTR_NULL
};

static struct attribute *session_class_attrs[] = {
	&session_attrs[0].attr,
	&session_attrs[1].attr,
	&session_attrs[2].attr,
	&session_attrs[3].attr,
	NULL
};
ATTRIBUTE_GROUPS(session_class);

#define AVS_DEV_NAME_MAX 20
static int create_session(u32 id)
{
	int r;
	unsigned long flags;
	char device_name[AVS_DEV_NAME_MAX];
	char *class_name = NULL;
	struct sync_session *session;

	class_name = vmalloc(AVS_DEV_NAME_MAX);
	if (!class_name) {
		r = -ENOMEM;
		goto err;
	}

	snprintf(device_name, AVS_DEV_NAME_MAX, "avsync_s%d", id);
	snprintf(class_name, AVS_DEV_NAME_MAX, "avsync_session%d", id);

	session = vzalloc(sizeof(*session));
	if (!session) {
		r = -ENOMEM;
		goto err;
	}
	session->session_class.name = class_name;
	session->session_class.class_groups = session_class_groups;

	r = class_register(&session->session_class);
	if (r) {
		msync_dbg(LOG_ERR, "session %d class fail\n", id);
		goto err2;
	}
	session->session_dev = device_create(&session->session_class, NULL,
		MKDEV(AMSYNC_SESSION_MAJOR, id), NULL, device_name);

	if (IS_ERR(session->session_dev)) {
		msync_dbg(LOG_ERR, "Can't create avsync_session device %d\n", id);
		r = -ENXIO;
		goto err3;
	}

	session->id = id;
	atomic_set(&session->refcnt, 1);
	init_waitqueue_head(&session->poll_wait);
	session->rate = 1000;
	session->stat = AVS_STAT_INIT;
	strncpy(session->name, current->comm, sizeof(session->name));
	session->first_vpts.pts = AVS_INVALID_PTS;
	session->last_vpts.pts = AVS_INVALID_PTS;
	session->first_apts.pts = AVS_INVALID_PTS;
	session->last_apts.pts = AVS_INVALID_PTS;
	session->wall_adj_thres = DEFAULT_WALL_ADJ_THRES;
	session->lock = __SPIN_LOCK_UNLOCKED(session->lock);
	session->disc_thres_min = DISC_THRE_MIN;
	session->disc_thres_max = DISC_THRE_MAX;
	session->pcr_clock = AVS_INVALID_PTS;
	session->last_check_apts = AVS_INVALID_PTS;
	session->last_check_vpts = AVS_INVALID_PTS;
	session->last_check_pcr_clock = AVS_INVALID_PTS;
	session->pcr_disc_clock = AVS_INVALID_PTS;

	mutex_init(&session->session_mutex);
	spin_lock_irqsave(&sync.lock, flags);
	list_add(&session->node, &sync.head);
	spin_unlock_irqrestore(&sync.lock, flags);
	msync_dbg(LOG_INFO, "av session %d created\n", id);
	return 0;
err3:
	class_unregister(&session->session_class);
err2:
	vfree(session);
err:
	vfree(class_name);
	return r;
}

static void destroy_session(u32 id)
{
	struct sync_session *session = NULL;
	struct list_head *p;
	unsigned long flags;

	spin_lock_irqsave(&sync.lock, flags);
	list_for_each(p, &sync.head) {
		session = list_entry(p, struct sync_session, node);
		if (session->id == id)
			break;
	}
	spin_unlock_irqrestore(&sync.lock, flags);
	if (!session)
		return;

	spin_lock_irqsave(&sync.lock, flags);
	list_del(&session->node);
	spin_unlock_irqrestore(&sync.lock, flags);

	if (atomic_dec_and_test(&session->refcnt))
		free_session(session);
	msync_dbg(LOG_INFO, "av session %d deref\n", id);
}

static long msync_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	u32 i;
	int rc;
	unsigned long flags;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case AMSYNC_IOC_ALLOC_SESSION:
	{
		struct msync_priv *priv = NULL;
		spin_lock_irqsave(&sync.lock, flags);
		for (i = 0 ; i < MAX_SESSION_NUM ; i++) {
			if (!sync.id_pool[i]) {
				sync.id_pool[i] = 1;
				break;
			}
		}
		spin_unlock_irqrestore(&sync.lock, flags);
		if (i >= MAX_SESSION_NUM) {
			msync_dbg(LOG_ERR, "out of sync session\n");
			return -EMFILE;
		}

		priv = vmalloc(sizeof(*priv));
		if (!priv)
			return -ENOMEM;

		rc = create_session(i);
		if (rc) {
			spin_lock_irqsave(&sync.lock, flags);
			sync.id_pool[i] = 0;
			spin_unlock_irqrestore(&sync.lock, flags);
			msync_dbg(LOG_ERR, "fail to create session %d\n", i);
			return rc;
		}
		put_user(i, (u32 __user *)argp);
		priv->session_id = i;
		file->private_data = priv;
		break;
	}
	case AMSYNC_IOC_REMOVE_SESSION:
	{
		u32 id;

		get_user(id, (u32 __user *)argp);
		if (id >= MAX_SESSION_NUM) {
			msync_dbg(LOG_ERR, "destroy invalid id %d\n", id);
			return -EINVAL;
		}

		spin_lock_irqsave(&sync.lock, flags);
		if (!sync.id_pool[id]) {
			spin_unlock_irqrestore(&sync.lock, flags);
			return -EBADF;
		}
		spin_unlock_irqrestore(&sync.lock, flags);

		destroy_session(id);
		vfree(file->private_data);
		file->private_data = NULL;
		break;
	}
	default:
		break;
	}
	return 0;
}

#ifdef CONFIG_COMPAT
static long msync_compat_ioctl(struct file *filp, u32 cmd, ulong arg)
{
	long ret;

	arg = (ulong)compat_ptr(arg);
	ret = msync_ioctl(filp, cmd, arg);
	return ret;
}
#endif

static int msync_open(struct inode *inode, struct file *file)
{
	file->private_data = (void *)-1;
	return 0;
}

static int msync_release(struct inode *inode, struct file *file)
{
	struct msync_priv *priv = file->private_data;
	int id = MAX_SESSION_NUM;
	unsigned long flags;

	if (!priv)
		return 0;
	id = priv->session_id;
	if (id >= MAX_SESSION_NUM) {
		msync_dbg(LOG_ERR, "destroy invalid id %d\n", id);
		return -EINVAL;
	}

	spin_lock_irqsave(&sync.lock, flags);
	if (!sync.id_pool[id]) {
		spin_unlock_irqrestore(&sync.lock, flags);
		return -EBADF;
	}
	spin_unlock_irqrestore(&sync.lock, flags);

	destroy_session(id);
	vfree(priv);
	file->private_data = NULL;
	return 0;
}

static const struct file_operations msync_fops = {
	.owner = THIS_MODULE,
	.open = msync_open,
	.release = msync_release,
	.unlocked_ioctl = msync_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = msync_compat_ioctl,
#endif
};

static ssize_t log_level_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", log_level);
}

static ssize_t log_level_store(struct class *cla,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	int r;

	r = kstrtoint(buf, 0, &log_level);
	if (r < 0)
		return -EINVAL;

	return count;
}

static ssize_t vout_mode_show(struct class *cla,
		struct class_attribute *attr,
		char *buf)
{
	return sprintf(buf, "den %d num %d inc %d\n",
			sync.sync_duration_den,
			sync.sync_duration_num,
			sync.vsync_pts_inc);
}

static ssize_t list_session_show(struct class *cla,
		struct class_attribute *attr, char *buf)
{
	struct sync_session *session = NULL;
	struct list_head *p;
	unsigned long flags;
	char tmp[256];
	int i = 0;

	spin_lock_irqsave(&sync.lock, flags);
	list_for_each(p, &sync.head) {
		session = list_entry(p, struct sync_session, node);
		i += snprintf(tmp + i, sizeof(tmp) - i,
			"%d: %s\n", session->id, session->name);
		if (i >= sizeof(tmp))
			break;
	}
	spin_unlock_irqrestore(&sync.lock, flags);
	if (i == 0)
		return sprintf(buf, "empty\n");

	return snprintf(buf, sizeof(tmp), "%s", tmp);
}

static struct class_attribute msync_attrs[] = {
	__ATTR(log_level,
		0664,
		log_level_show,
		log_level_store),
	__ATTR_RO(list_session),
	__ATTR_RO(vout_mode),
	__ATTR_NULL
};

static struct attribute *msync_class_attrs[] = {
	&msync_attrs[0].attr,
	&msync_attrs[1].attr,
	&msync_attrs[2].attr,
	NULL
};
ATTRIBUTE_GROUPS(msync_class);

static struct class msync_class = {
	.name = "aml_msync",
	.class_groups = msync_class_groups,
};

int __init msync_init(void)
{
	int r = 0;

	r = register_chrdev(AMSYNC_MAJOR, "aml_msync", &msync_fops);
	if (r < 0) {
		msync_dbg(LOG_ERR,
			"Can't register major for aml_msync device\n");
		return r;
	}
	sync.multi_sync_major = r;

	r = register_chrdev(AMSYNC_SESSION_MAJOR, "aml_msync_session", &session_fops);
	if (r < 0) {
		msync_dbg(LOG_ERR,
			"Can't register major for aml_msync_session device\n");
		goto err;
	}
	sync.session_major = r;

	r = class_register(&msync_class);
	if (r) {
		msync_dbg(LOG_ERR, "msync class fail.\n");
		goto err2;
	}

	sync.msync_dev = device_create(&msync_class, NULL,
			MKDEV(AMSYNC_MAJOR, 0), NULL, "aml_msync");

	if (IS_ERR(sync.msync_dev)) {
		msync_dbg(LOG_ERR, "Can't create aml_msync device\n");
		r = -ENXIO;
		goto err3;
	}
	sync.lock = __SPIN_LOCK_UNLOCKED(sync.lock);
	INIT_LIST_HEAD(&sync.head);
	msync_update_mode();
	sync.msync_notifier.notifier_call = msync_notify_callback;
	vout_register_client(&sync.msync_notifier);
	sync.ready = true;
	return 0;
err3:
	class_unregister(&msync_class);
err2:
	unregister_chrdev(AMSYNC_SESSION_MAJOR, "aml_msync_session");
err:
	unregister_chrdev(AMSYNC_MAJOR, "aml_msync");
	return r;
}

void __exit msync_exit(void)
{
	if (sync.msync_dev) {
		class_unregister(&msync_class);
		device_destroy(&msync_class, MKDEV(AMSYNC_MAJOR, 0));
	}
	unregister_chrdev(AMSYNC_MAJOR, "aml_msync");
}

#ifndef MODULE
module_init(msync_init);
module_exit(msync_exit);
#endif

MODULE_LICENSE("GPL");
