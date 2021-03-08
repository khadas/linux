/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef PTSSERV_H
#define PTSSERV_H

enum {
	PTS_TYPE_VIDEO = 0,
	PTS_TYPE_AUDIO = 1,
	PTS_TYPE_HEVC = 2,
	PTS_TYPE_MAX = 3
};

#define apts_checkin(x) pts_checkin(PTS_TYPE_AUDIO, (x))
#define vpts_checkin(x) pts_checkin(PTS_TYPE_VIDEO, (x))

#ifndef CALC_CACHED_TIME
#define CALC_CACHED_TIME
#endif
int pts_checkin(u8 type, u32 val);

int pts_checkin_wrptr(u8 type, u32 ptr, u32 val);

int pts_checkin_wrptr_pts33(u8 type, u32 ptr, u64 val);

int pts_checkin_offset(u8 type, u32 offset, u32 val);

int pts_checkin_offset_us64(u8 type, u32 offset, u64 us);

int get_last_checkin_pts(u8 type);

int get_last_checkout_pts(u8 type);

int pts_lookup(u8 type, u32 *val, u32 *frame_size, u32 pts_margin);

int pts_lookup_offset(u8 type, u32 offset, u32 *val,
		      u32 *frame_size, u32 pts_margin);

int pts_lookup_offset_us64(u8 type, u32 offset, u32 *val,
			   u32 *frame_size, u32 pts_margin, u64 *us64);

int pts_pickout_offset_us64(u8 type, u32 offset,
			    u32 *val, u32 pts_margin,
			    u64 *us64);

int pts_set_resolution(u8 type, u32 level);

int pts_set_rec_size(u8 type, u32 val);

int pts_get_rec_num(u8 type, u32 val);

int pts_start(u8 type);

int pts_stop(u8 type);

int first_lookup_pts_failed(u8 type);

int first_pts_checkin_complete(u8 type);
int calculation_stream_delayed_ms(u8 type, u32 *latestbirate,
				  u32 *avg_bitare);

int calculation_vcached_delayed(void);

int calculation_acached_delayed(void);
#endif /* PTSSERV_H */
