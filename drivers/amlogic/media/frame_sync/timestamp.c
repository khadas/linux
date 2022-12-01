// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define DEBUG
#include <linux/module.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/frame_sync/tsync_pcr.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/registers/register.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_TSYNC
#include <trace/events/meson_atrace.h>

u32 acc_apts_inc;
u32 acc_apts_dec;
u32 acc_pcrscr_inc;
u32 acc_pcrscr_dec;

static s32 system_time_inc_adj;
static u32 system_time;
static u32 system_time_up;
static u32 audio_pts_up;
static u32 audio_pts_started;
static u32 first_vpts;
static u32 first_checkin_vpts;
static u32 first_checkin_apts;
static u32 first_apts;
static u32 pcrscr_lantcy = 200 * 90;
static u32 pcrscr_additional_lantency;
static u32 video_pts;
static u64 video_pts_u64;
static u32 audio_pts;
static u32 avsync_counts;
static u32 last_apts_gap;
static u32 last_vpts_gap;

static u32 system_time_scale_base = 1;
static u32 system_time_scale_remainder;

#ifdef MODIFY_TIMESTAMP_INC_WITH_PLL
#define PLL_FACTOR 10000
static u32 timestamp_inc_factor = PLL_FACTOR;

void set_timestamp_inc_factor(u32 factor)
{
	timestamp_inc_factor = factor;
}
#endif

u32 timestamp_vpts_get(void)
{
	return video_pts;
}
EXPORT_SYMBOL(timestamp_vpts_get);

void timestamp_vpts_set(u32 pts)
{
	video_pts = pts;
	ATRACE_COUNTER("VPTS", video_pts);
}
EXPORT_SYMBOL(timestamp_vpts_set);

void timestamp_vpts_inc(s32 val)
{
	video_pts += val;
	ATRACE_COUNTER("VPTS", video_pts);
}
EXPORT_SYMBOL(timestamp_vpts_inc);

u64 timestamp_vpts_get_u64(void)
{
	return video_pts_u64;
}
EXPORT_SYMBOL(timestamp_vpts_get_u64);

void timestamp_vpts_set_u64(u64 pts)
{
	video_pts_u64 = pts;
}
EXPORT_SYMBOL(timestamp_vpts_set_u64);

void timestamp_vpts_inc_u64(s32 val)
{
	video_pts_u64 += val;
}
EXPORT_SYMBOL(timestamp_vpts_inc_u64);

u32 timestamp_apts_get(void)
{
	return audio_pts;
}
EXPORT_SYMBOL(timestamp_apts_get);

u64 timestamp_apts_get_u64(void)
{
	u64 pts_val_video;
	u64 pts_val;

	pts_val_video = div64_u64(timestamp_vpts_get_u64() * 9, 100);
	pts_val = ((u64)timestamp_apts_get()) & 0x00000000FFFFFFFF;
	if (pts_val_video & (1LL << 32))
		pts_val |= (1LL << 32);

	pts_val_video = div64_u64(pts_val * 100, 9);

	return pts_val_video;
}
EXPORT_SYMBOL(timestamp_apts_get_u64);

void timestamp_apts_set(u32 pts)
{
	audio_pts = pts;
	ATRACE_COUNTER("APTS", audio_pts);
}
EXPORT_SYMBOL(timestamp_apts_set);

void timestamp_apts_inc(s32 inc)
{
	if (audio_pts_up) {
#ifdef MODIFY_TIMESTAMP_INC_WITH_PLL
		inc = inc * timestamp_inc_factor / PLL_FACTOR;
#endif
		audio_pts += inc;
		ATRACE_COUNTER("APTS", audio_pts);
	}
}
EXPORT_SYMBOL(timestamp_apts_inc);

void timestamp_apts_enable(u32 enable)
{
	audio_pts_up = enable;
	pr_info("%s enable:%x,\n", __func__, enable);
}
EXPORT_SYMBOL(timestamp_apts_enable);

void timestamp_apts_start(u32 enable)
{
	audio_pts_started = enable;
	pr_info("audio pts started::::::: %d\n", enable);
}
EXPORT_SYMBOL(timestamp_apts_start);

u32 timestamp_apts_started(void)
{
	return audio_pts_started;
}
EXPORT_SYMBOL(timestamp_apts_started);

u32 timestamp_pcrscr_get(void)
{
	u32 tmp_pcr;
	if (tsync_get_mode() != TSYNC_MODE_PCRMASTER)
		return system_time;

	if (tsync_get_demux_pcrscr_valid()) {
		if (tsync_pcr_demux_pcr_used() == 0) {
			return system_time;
		} else {
			if (tsync_get_demux_pcr(&tmp_pcr)) {
				if (tmp_pcr > (pcrscr_lantcy + pcrscr_additional_lantency))
					return
					tmp_pcr - pcrscr_lantcy - pcrscr_additional_lantency;
				else
					return 0;
			} else {
				return system_time;
			}
		}
	} else {
		return system_time;
	}
}
EXPORT_SYMBOL(timestamp_pcrscr_get);

u64 timestamp_pcrscr_get_u64(void)
{
	u64 pts_val_video;
	u64 pts_val;

	pts_val_video = div64_u64(timestamp_vpts_get_u64() * 9, 100);
	pts_val = ((u64)timestamp_pcrscr_get()) & 0x00000000FFFFFFFF;
	if (pts_val_video & (1LL << 32))
		pts_val |= (1LL << 32);

	pts_val_video = div64_u64(pts_val * 100, 9);

	return pts_val_video;
}
EXPORT_SYMBOL(timestamp_pcrscr_get_u64);

u32 timestamp_avsync_counter_get(void)
{
	return avsync_counts;
}
EXPORT_SYMBOL(timestamp_avsync_counter_get);

void timestamp_avsync_counter_set(u32 counts)
{
	avsync_counts = counts;
}
EXPORT_SYMBOL(timestamp_avsync_counter_set);

void timestamp_set_pcrlatency(u32 latency)
{
	if (latency < 3000 * 90)
		pcrscr_lantcy = latency;
	else
		pcrscr_lantcy = 3000 * 90;
}
EXPORT_SYMBOL(timestamp_set_pcrlatency);

u32 timestamp_get_pcrlatency(void)
{
	return pcrscr_lantcy;
}
EXPORT_SYMBOL(timestamp_get_pcrlatency);

void timestamp_clac_pts_latency(u8 type, u32 pts)
{
	u32 demux_pcr = 0;
	u32 t1, t2, pts_diff;

	if (tsync_get_mode() != TSYNC_MODE_PCRMASTER)
		return;
	if (tsync_get_demux_pcrscr_valid() &&
		tsync_pcr_demux_pcr_used()) {
		if (tsync_get_demux_pcr(&demux_pcr) == 0)
			return;
		if (demux_pcr == 0 ||
		    demux_pcr == 0xffffffff) {
			last_apts_gap = 0;
			last_vpts_gap = 0;
			return;
		}
		if (type == 0) {
			if (demux_pcr > pts) {
				last_vpts_gap = 0;
				return;
			}
			pts_diff = pts - demux_pcr;
			if (pts_diff > 500 * 90)
				return;
			t1 = ((last_vpts_gap >> 16) & 0xff);
			if (t1 > 5)
				t1 = 0;
			t2 = (last_vpts_gap & 0xffff);
			if (t2 == 0) {
				last_vpts_gap = pts_diff;
				return;
			}
			if (abs(t2 - pts_diff) < 30 * 90) {
				last_vpts_gap = t2;
				return;
			}
			t1++;
			if (t1 >= 5)
				last_vpts_gap = pts_diff;
			else
				last_vpts_gap = ((t1 << 16) | t2);
		} else if (type == 1) {
			if (demux_pcr > pts) {
				last_apts_gap = 0;
				return;
			}
			pts_diff = pts - demux_pcr;
			if (pts_diff > 500 * 90)
				return;
			t1 = ((last_apts_gap >> 16) & 0xff);
			if (t1 > 5)
				t1 = 0;
			t2 = (last_apts_gap & 0xffff);
			if (t2 == 0) {
				last_apts_gap = pts_diff;
				return;
			}
			if (abs(t2 - pts_diff) < 30 * 90) {
				last_apts_gap = t2;
				return;
			}
			t1++;
			if (t1 >= 5)
				last_apts_gap = pts_diff;
			else
				last_apts_gap = ((t1 << 16) | t2);
		}
	} else {
		last_apts_gap = 0;
		last_vpts_gap = 0;
	}
}
EXPORT_SYMBOL(timestamp_clac_pts_latency);

u32 timestamp_get_pts_latency(u8 type)
{
	if (type == 0)
		return (last_vpts_gap & 0xffff);
	else if (type == 1)
		return (last_apts_gap & 0xffff);
	return 0;
}
EXPORT_SYMBOL(timestamp_get_pts_latency);

void timestamp_clean_pts_latency(u8 type)
{
	if (type == 0)
		last_vpts_gap = 0;
	else if (type == 1)
		last_apts_gap = 0;
}
EXPORT_SYMBOL(timestamp_clean_pts_latency);

u32 timestamp_tsdemux_pcr_get(void)
{
	u32 tmp_pcr = 0;

	if (tsync_get_demux_pcr(&tmp_pcr))
		return tmp_pcr;

	return (u32)-1;
}
EXPORT_SYMBOL(timestamp_tsdemux_pcr_get);

void timestamp_pcrscr_set(u32 pts)
{
	/*pr_info("timestamp_pcrscr_set system time  = %x\n", pts);*/
	system_time = pts;
	ATRACE_COUNTER("PCRSCR",  pts);
}
EXPORT_SYMBOL(timestamp_pcrscr_set);

void timestamp_firstvpts_set(u32 pts)
{
	first_vpts = pts;
	pr_info("video first pts = %x\n", first_vpts);
}
EXPORT_SYMBOL(timestamp_firstvpts_set);

u32 timestamp_firstvpts_get(void)
{
	return first_vpts;
}
EXPORT_SYMBOL(timestamp_firstvpts_get);

void timestamp_checkin_firstvpts_set(u32 pts)
{
	first_checkin_vpts = pts;
	pr_info("video first checkin pts = %x\n", first_checkin_vpts);
	timestamp_clean_pts_latency(0);
}
EXPORT_SYMBOL(timestamp_checkin_firstvpts_set);

void timestamp_checkin_firstapts_set(u32 pts)
{
	first_checkin_apts = pts;
	pr_info("audio first checkin pts =%x\n", first_checkin_apts);
	timestamp_clean_pts_latency(1);
}
EXPORT_SYMBOL(timestamp_checkin_firstapts_set);

u32 timestamp_checkin_firstvpts_get(void)
{
	return first_checkin_vpts;
}
EXPORT_SYMBOL(timestamp_checkin_firstvpts_get);

u32 timestamp_checkin_firstapts_get(void)
{
	return first_checkin_apts;
}
EXPORT_SYMBOL(timestamp_checkin_firstapts_get);

void timestamp_firstapts_set(u32 pts)
{
	first_apts = pts;
	pr_info("audio first pts = %x\n", first_apts);
}
EXPORT_SYMBOL(timestamp_firstapts_set);

u32 timestamp_firstapts_get(void)
{
	return first_apts;
}
EXPORT_SYMBOL(timestamp_firstapts_get);

void timestamp_pcrscr_inc(s32 inc)
{
	if (system_time_up) {
#ifdef MODIFY_TIMESTAMP_INC_WITH_PLL
		inc = inc * (s32)timestamp_inc_factor / PLL_FACTOR;
#endif
		system_time += inc + system_time_inc_adj;
		ATRACE_COUNTER("PCRSCR",  system_time);
	}
}
EXPORT_SYMBOL(timestamp_pcrscr_inc);

void timestamp_pcrscr_inc_scale(s32 inc, u32 base)
{
	if (system_time_scale_base != base) {
		system_time_scale_remainder =
			system_time_scale_remainder *
			base / system_time_scale_base;
		system_time_scale_base = base;
	}

	if (system_time_up) {
		u32 r;

		system_time +=
			div_u64_rem(90000ULL * inc, base, &r) +
			system_time_inc_adj;
		system_time_scale_remainder += r;
		if (system_time_scale_remainder >= system_time_scale_base) {
			system_time++;
			system_time_scale_remainder -= system_time_scale_base;
		}
		ATRACE_COUNTER("PCRSCR",  system_time);
	}
}
EXPORT_SYMBOL(timestamp_pcrscr_inc_scale);

void timestamp_pcrscr_set_adj(s32 inc)
{
	system_time_inc_adj = inc;
}
EXPORT_SYMBOL(timestamp_pcrscr_set_adj);

void timestamp_pcrscr_set_adj_pcr(s32 inc)
{
	const struct vinfo_s *info = get_current_vinfo();

	if (inc != 0) {
		system_time_inc_adj =
			900 * info->sync_duration_den /
			(info->sync_duration_num * inc);
	} else {
		system_time_inc_adj = 0;
	}
}
EXPORT_SYMBOL(timestamp_pcrscr_set_adj_pcr);

void timestamp_pcrscr_enable(u32 enable)
{
	system_time_up = enable;
}
EXPORT_SYMBOL(timestamp_pcrscr_enable);

u32 timestamp_pcrscr_enable_state(void)
{
	return system_time_up;
}
EXPORT_SYMBOL(timestamp_pcrscr_enable_state);

void timestamp_additional_latency_set(u32 pts)
{
	if (pts < 3000 * 90)
		pcrscr_additional_lantency = pts;
	else
		pcrscr_additional_lantency = 3000 * 90;
	pr_info("pcrscr_additional_lantency = %x\n", pcrscr_additional_lantency);
}
EXPORT_SYMBOL(timestamp_additional_latency_set);

u32 timestamp_additional_latency_get(void)
{
	return pcrscr_additional_lantency;
}
EXPORT_SYMBOL(timestamp_additional_latency_get);

//MODULE_DESCRIPTION("AMLOGIC time sync management driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
