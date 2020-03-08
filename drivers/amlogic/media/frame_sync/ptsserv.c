// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#undef DEBUG
#define DEBUG
#include <linux/module.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/media/utils/amports_config.h>
/* #include <mach/am_regs.h> */

#include <linux/amlogic/media/utils/vdec_reg.h>

#define VIDEO_REC_SIZE  (8192 * 2)
#define AUDIO_REC_SIZE  (8192 * 2)
#define VIDEO_LOOKUP_RESOLUTION 2500
#define AUDIO_LOOKUP_RESOLUTION 1024

#define INTERPOLATE_AUDIO_PTS
#define INTERPOLATE_AUDIO_RESOLUTION 9000
#define PTS_VALID_OFFSET_TO_CHECK      0x08000000

#define OFFSET_DIFF(x, y)  ((int)((x) - (y)))
#define OFFSET_LATER(x, y) (OFFSET_DIFF(x, y) > 0)
#define OFFSET_EQLATER(x, y) (OFFSET_DIFF(x, y) >= 0)

#define VAL_DIFF(x, y) ((int)((x) - (y)))

enum {
	PTS_IDLE = 0,
	PTS_INIT = 1,
	PTS_LOADING = 2,
	PTS_RUNNING = 3,
	PTS_DEINIT = 4
};

struct pts_rec_s {
	struct list_head list;
	u32 offset;
	u32 val;
	u32 size;
	u64 pts_us64;
} /*pts_rec_t */;

struct pts_table_s {
	u32 status;
	int rec_num;
	int lookup_threshold;
	u32 lookup_cache_offset;
	bool lookup_cache_valid;
	u32 lookup_cache_pts;
	u64 lookup_cache_pts_us64;
	unsigned long buf_start;
	u32 buf_size;
	int first_checkin_pts;
	u64 first_checkin_pts_us64;
	int first_lookup_ok;
	int first_lookup_is_fail;	/*1: first lookup fail;*/
					   /*0: first lookup success */

	struct pts_rec_s *pts_recs;
	unsigned long *pages_list;
	struct list_head *pts_search;
	struct list_head valid_list;
	struct list_head free_list;
#ifdef CALC_CACHED_TIME
	u32 last_checkin_offset;
	u32 last_checkin_pts;
	u32 last_checkout_pts;
	u32 last_checkout_offset;
	u32 last_checkin_jiffies;
	u32 last_bitrate;
	u32 last_avg_bitrate;
	u32 last_pts_delay_ms;
#endif
	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	u32 hevc;
	/* #endif */
} /*pts_table_t */;

static DEFINE_SPINLOCK(lock);

extern pfun_get_buf_by_type get_buf_by_type_cb;
extern pfun_stbuf_level stbuf_level_cb;
extern pfun_stbuf_space stbuf_space_cb;
extern pfun_tsdemux_pcrscr_valid tsdemux_pcrscr_valid_cb;
extern pfun_tsdemux_pcrscr_valid tsdemux_pcrscr_valid_cb;

static struct pts_table_s pts_table[PTS_TYPE_MAX] = {
	{
		.status = PTS_IDLE,
		.rec_num = VIDEO_REC_SIZE,
		.lookup_threshold = VIDEO_LOOKUP_RESOLUTION,
	},
	{
		.status = PTS_IDLE,
		.rec_num = AUDIO_REC_SIZE,
		.lookup_threshold = AUDIO_LOOKUP_RESOLUTION,
	},
};

static inline void get_wrpage_offset(u8 type, u32 *page, u32 *page_offset)
{
	ulong flags;
	u32 page1, page2, offset;

	if (type == PTS_TYPE_VIDEO) {
		do {
			local_irq_save(flags);

			page1 = READ_PARSER_REG(PARSER_AV_WRAP_COUNT) & 0xffff;
			offset = READ_PARSER_REG(PARSER_VIDEO_WP);
			page2 = READ_PARSER_REG(PARSER_AV_WRAP_COUNT) & 0xffff;

			local_irq_restore(flags);
		} while (page1 != page2);

		*page = page1;
		*page_offset = offset - pts_table[PTS_TYPE_VIDEO].buf_start;
	} else if (type == PTS_TYPE_AUDIO) {
		do {
			local_irq_save(flags);

			page1 = READ_PARSER_REG(PARSER_AV_WRAP_COUNT) >> 16;
			offset = READ_PARSER_REG(PARSER_AUDIO_WP);
			page2 = READ_PARSER_REG(PARSER_AV_WRAP_COUNT) >> 16;

			local_irq_restore(flags);
		} while (page1 != page2);

		*page = page1;
		*page_offset = offset - pts_table[PTS_TYPE_AUDIO].buf_start;
	}
}

static inline void get_rdpage_offset(u8 type, u32 *page, u32 *page_offset)
{
	ulong flags;
	u32 page1, page2, offset;

	if (type == PTS_TYPE_VIDEO) {
		do {
			local_irq_save(flags);

			page1 = READ_VREG(VLD_MEM_VIFIFO_WRAP_COUNT) & 0xffff;
			offset = READ_VREG(VLD_MEM_VIFIFO_RP);
			page2 = READ_VREG(VLD_MEM_VIFIFO_WRAP_COUNT) & 0xffff;

			local_irq_restore(flags);
		} while (page1 != page2);

		*page = page1;
		*page_offset = offset - pts_table[PTS_TYPE_VIDEO].buf_start;
	} else if (type == PTS_TYPE_AUDIO) {
		do {
			local_irq_save(flags);

			page1 =
				READ_AIU_REG(AIU_MEM_AIFIFO_BUF_WRAP_COUNT) &
				0xffff;
			offset = READ_AIU_REG(AIU_MEM_AIFIFO_MAN_RP);
			page2 =
				READ_AIU_REG(AIU_MEM_AIFIFO_BUF_WRAP_COUNT) &
				0xffff;
			local_irq_restore(flags);
		} while (page1 != page2);

		*page = page1;
		*page_offset = offset - pts_table[PTS_TYPE_AUDIO].buf_start;
	}
}

#ifdef CALC_CACHED_TIME
int pts_cached_time(u8 type)
{
	struct pts_table_s *p_table;

	if (type >= PTS_TYPE_MAX)
		return 0;

	p_table = &pts_table[type];

	if ((p_table->last_checkin_pts == -1) ||
	    (p_table->last_checkout_pts == -1))
		return 0;

	return p_table->last_checkin_pts - p_table->last_checkout_pts;
}
EXPORT_SYMBOL(pts_cached_time);

int calculation_stream_delayed_ms(u8 type, u32 *latestbitrate,
				  u32 *avg_bitare)
{
	struct pts_table_s *p_table;
	int timestampe_delayed = 0;
	unsigned long outtime;

	if (type >= PTS_TYPE_MAX)
		return 0;
	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	if (has_hevc_vdec() && type == PTS_TYPE_HEVC)
		p_table = &pts_table[PTS_TYPE_VIDEO];
	else
		/* #endif */
		p_table = &pts_table[type];

	if ((p_table->last_checkin_pts == -1) ||
	    (p_table->last_checkout_pts == -1))
		return 0;
	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	if (has_hevc_vdec() && type == PTS_TYPE_HEVC)
		outtime = timestamp_vpts_get();
	else
		/* #endif */
		if (type == PTS_TYPE_VIDEO)
			outtime = timestamp_vpts_get();
		else if (type == PTS_TYPE_AUDIO)
			outtime = timestamp_apts_get();
		else
			outtime = timestamp_pcrscr_get();
	if (outtime == 0 || outtime == 0xffffffff)
		outtime = p_table->last_checkout_pts;
	timestampe_delayed = (p_table->last_checkin_pts - outtime) / 90;
	p_table->last_pts_delay_ms = timestampe_delayed;
	if (get_buf_by_type_cb && stbuf_level_cb && stbuf_space_cb) {
		if (timestampe_delayed < 10 ||
		    ((abs(p_table->last_pts_delay_ms - timestampe_delayed)
			> 3000) && p_table->last_avg_bitrate > 0)) {
			int diff = p_table->last_checkin_offset -
			p_table->last_checkout_offset;
		int diff2;
		int delay_ms;

		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
		if (has_hevc_vdec()) {
			if (p_table->hevc)
				diff2 = stbuf_level_cb
						(get_buf_by_type_cb
							(PTS_TYPE_HEVC));
			else
				diff2 = stbuf_level_cb
						(get_buf_by_type_cb(type));
			} else {
			/* #endif */
				diff2 = stbuf_level_cb
					(get_buf_by_type_cb(type));
			}
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
		if (has_hevc_vdec() == 1) {
			if (p_table->hevc > 0) {
				if (diff2 >
				    stbuf_space_cb
				    (get_buf_by_type_cb(PTS_TYPE_HEVC)))
					diff = diff2;
			} else {
				if (diff2 >
				    stbuf_space_cb(get_buf_by_type_cb(type)))
					diff = diff2;
			}
		} else {
			/* #endif */
			if (diff2 > stbuf_space_cb(get_buf_by_type_cb(type)))
				diff = diff2;
		}
		delay_ms = diff * 1000 / (1 + p_table->last_avg_bitrate / 8);
		if (timestampe_delayed < 10 ||
		    ((abs(timestampe_delayed - delay_ms) > (3 * 1000)) &&
			delay_ms > 1000)) {
			/*
			 *pr_info
			 *("%d:recalculated ptsdelay=%dms bitratedelay=%d ",
			 *type, timestampe_delayed, delay_ms);
			 *pr_info
			 *("diff=%d,p_table->last_avg_bitrate=%d\n",
			 *diff, p_table->last_avg_bitrate);
			 */
			timestampe_delayed = delay_ms;
		}
	}
	}

	if (latestbitrate)
		*latestbitrate = p_table->last_bitrate;

	if (avg_bitare)
		*avg_bitare = p_table->last_avg_bitrate;
	return timestampe_delayed;
}
EXPORT_SYMBOL(calculation_stream_delayed_ms);

/* return the 1/90000 unit time */
int calculation_vcached_delayed(void)
{
	struct pts_table_s *p_table;
	u32 delay = 0;

	p_table = &pts_table[PTS_TYPE_VIDEO];

	delay = p_table->last_checkin_pts - timestamp_vpts_get();

	if (delay > 0 && (delay < 5 * 90000))
		return delay;

	if (p_table->last_avg_bitrate > 0) {
		int diff =
			p_table->last_checkin_offset -
			p_table->last_checkout_offset;
		delay = diff * 90000 / (1 + p_table->last_avg_bitrate / 8);

		return delay;
	}

	return -1;
}
EXPORT_SYMBOL(calculation_vcached_delayed);

/* return the 1/90000 unit time */
int calculation_acached_delayed(void)
{
	struct pts_table_s *p_table;
	u32 delay = 0;

	p_table = &pts_table[PTS_TYPE_AUDIO];

	delay = p_table->last_checkin_pts - timestamp_apts_get();
	if (delay > 0 && delay < 5 * 90000)
		return delay;

	if (p_table->last_avg_bitrate > 0) {
		int diff =
			p_table->last_checkin_offset -
			p_table->last_checkout_offset;
		delay = diff * 90000 / (1 + p_table->last_avg_bitrate / 8);

		return delay;
	}

	return -1;
}
EXPORT_SYMBOL(calculation_acached_delayed);

int calculation_stream_ext_delayed_ms(u8 type)
{
	struct pts_table_s *p_table;
	int extdelay_ms;

	if (type >= PTS_TYPE_MAX)
		return 0;
	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	if (has_hevc_vdec() && type == PTS_TYPE_HEVC)
		p_table = &pts_table[PTS_TYPE_VIDEO];
	else
		/* #endif */
		p_table = &pts_table[type];

	extdelay_ms = jiffies - p_table->last_checkin_jiffies;

	if (extdelay_ms < 0)
		extdelay_ms = 0;

	return extdelay_ms * 1000 / HZ;
}
EXPORT_SYMBOL(calculation_stream_ext_delayed_ms);

#endif

#ifdef CALC_CACHED_TIME
static inline void pts_checkin_offset_calc_cached(u32 offset, u32 val,
						  struct pts_table_s *p_table)
{
	s32 diff = offset - p_table->last_checkin_offset;

	if (diff > 0) {
		if ((val - p_table->last_checkin_pts) > 0) {
			int newbitrate =
				diff * 8 * 90 / (1 +
						(val
						- p_table->last_checkin_pts)
						/ 1000);
			if (p_table->last_bitrate > 100) {
				if (newbitrate < p_table->last_bitrate * 5 &&
				    newbitrate > p_table->last_bitrate / 5) {
					p_table->last_avg_bitrate
						=
						(p_table->last_avg_bitrate
						 * 19 + newbitrate) / 20;
				} else {
					/*
					 * newbitrate is >5*lastbitrate
					 *or < bitrate/5;
					 *we think a pts discontinue.
					 *we must double check it.
					 *ignore update  birate.;
					 */
				}
			} else if (newbitrate > 100) {
				/*first init. */
				p_table->last_avg_bitrate =
					p_table->last_bitrate = newbitrate;
			}
			p_table->last_bitrate = newbitrate;
		}
		p_table->last_checkin_offset = offset;
		p_table->last_checkin_pts = val;
		p_table->last_checkin_jiffies = jiffies;
	}
}

#endif
static int pts_checkin_offset_inline(u8 type, u32 offset, u32 val, u64 us64)
{
	ulong flags;
	struct pts_table_s *p_table;

	if (type >= PTS_TYPE_MAX)
		return -EINVAL;

	p_table = &pts_table[type];

	spin_lock_irqsave(&lock, flags);

	if (likely(p_table->status == PTS_RUNNING ||
		   p_table->status == PTS_LOADING)) {
		struct pts_rec_s *rec = NULL;
		struct pts_rec_s *rec_prev = NULL;

		if (type == PTS_TYPE_VIDEO &&
		    p_table->first_checkin_pts == -1) {
			p_table->first_checkin_pts = val;
			timestamp_checkin_firstvpts_set(val);
			/*
			 *if(tsync_get_debug_pts_checkin() &&
			 * tsync_get_debug_vpts()) {
			 */
			pr_debug("first check in vpts <0x%x:0x%x> ok!\n",
				 offset, val);
			/* } */
		}
		if (type == PTS_TYPE_AUDIO &&
		    p_table->first_checkin_pts == -1) {
			p_table->first_checkin_pts = val;
			timestamp_checkin_firstapts_set(val);
			/*
			 *if (tsync_get_debug_pts_checkin() &&
			 * tsync_get_debug_apts()) {
			 */
			pr_info("first check in apts <0x%x:0x%x> ok!\n", offset,
				val);
			/* } */
		}

		if (tsync_get_debug_pts_checkin()) {
			if (tsync_get_debug_vpts() &&
			    type == PTS_TYPE_VIDEO) {
				pr_info("check in vpts <0x%x:0x%x>,",
					offset, val);
				pr_info("current vpts 0x%x\n",
					timestamp_vpts_get());
			}

			if (tsync_get_debug_apts() &&
			    type == PTS_TYPE_AUDIO) {
				pr_info("check in apts <0x%x:0x%x>\n", offset,
					val);
			}
		}

		if (list_empty(&p_table->free_list)) {
			rec =
				list_entry(p_table->valid_list.next,
					   struct pts_rec_s, list);
		} else {
			rec =
				list_entry(p_table->free_list.next,
					   struct pts_rec_s, list);
		}

		if (!list_empty(&p_table->valid_list)) {
			rec_prev = list_entry(p_table->valid_list.prev,
					      struct pts_rec_s, list);
			if (rec_prev->offset == p_table->last_checkin_offset) {
				if (offset > p_table->last_checkin_offset)
					rec_prev->size =
					offset - p_table->last_checkin_offset;
				else
					rec_prev->size = 0;
			}
		}

		if (p_table->last_checkin_offset > 0 &&
		    offset > p_table->last_checkin_offset)
			rec->size =
				offset - p_table->last_checkin_offset;
		else
			rec->size = rec_prev ? rec_prev->size : 0;

		rec->offset = offset;
		rec->val = val;
		rec->pts_us64 = us64;

#ifdef CALC_CACHED_TIME
		pts_checkin_offset_calc_cached(offset, val, p_table);
#endif
		timestamp_clac_pts_latency(type, val);

		list_move_tail(&rec->list, &p_table->valid_list);

		spin_unlock_irqrestore(&lock, flags);

		if (p_table->status == PTS_LOADING) {
			if (tsync_get_debug_vpts() && type == PTS_TYPE_VIDEO)
				pr_info("init vpts[%d] at 0x%x\n", type, val);

			if (tsync_get_debug_apts() && type == PTS_TYPE_AUDIO)
				pr_info("init apts[%d] at 0x%x\n", type, val);

			if (type == PTS_TYPE_VIDEO && !tsync_get_tunnel_mode())
				timestamp_vpts_set(val);
			else if (type == PTS_TYPE_AUDIO)
				timestamp_apts_set(val);

			p_table->status = PTS_RUNNING;
		}

		return 0;

	} else {
		spin_unlock_irqrestore(&lock, flags);

		return -EINVAL;
	}
}

int pts_checkin_offset(u8 type, u32 offset, u32 val)
{
	u64 us;

	us = div64_u64((u64)val * 100, 9);
	return pts_checkin_offset_inline(type, offset, val, us);
}
EXPORT_SYMBOL(pts_checkin_offset);

int pts_checkin_offset_us64(u8 type, u32 offset, u64 us)
{
	u64 pts_val;

	pts_val = div64_u64(us * 9, 100);
	return pts_checkin_offset_inline(type, offset, (u32)pts_val, us);
}
EXPORT_SYMBOL(pts_checkin_offset_us64);

/*
 *This type of PTS could happen in the past,
 * e.g. from TS demux when the real time (wr_page, wr_ptr)
 * could be bigger than pts parameter here.
 */
int pts_checkin_wrptr(u8 type, u32 ptr, u32 val)
{
	u32 offset, cur_offset = 0, page = 0, page_no;

	if (type >= PTS_TYPE_MAX)
		return -EINVAL;

	offset = ptr - pts_table[type].buf_start;
	get_wrpage_offset(type, &page, &cur_offset);

	page_no = (offset > cur_offset) ? (page - 1) : page;
	if (type == PTS_TYPE_VIDEO)
		val += tsync_get_vpts_adjust();
	return pts_checkin_offset(type,
			pts_table[type].buf_size * page_no + offset,
			val);
}
EXPORT_SYMBOL(pts_checkin_wrptr);

int pts_checkin(u8 type, u32 val)
{
	u32 page, offset;

	get_wrpage_offset(type, &page, &offset);

	if (type == PTS_TYPE_VIDEO) {
		offset = page * pts_table[PTS_TYPE_VIDEO].buf_size + offset;
		pts_checkin_offset(PTS_TYPE_VIDEO, offset, val);
		return 0;
	} else if (type == PTS_TYPE_AUDIO) {
		offset = page * pts_table[PTS_TYPE_AUDIO].buf_size + offset;
		pts_checkin_offset(PTS_TYPE_AUDIO, offset, val);
		return 0;
	} else {
		return -EINVAL;
	}
}
EXPORT_SYMBOL(pts_checkin);
/*
 * The last checkin pts means the position in the stream.
 */
int get_last_checkin_pts(u8 type)
{
	struct pts_table_s *p_table = NULL;
	u32 last_checkin_pts = 0;
	ulong flags;

	spin_lock_irqsave(&lock, flags);

	if (type == PTS_TYPE_VIDEO) {
		p_table = &pts_table[PTS_TYPE_VIDEO];
		last_checkin_pts = p_table->last_checkin_pts;
	} else if (type == PTS_TYPE_AUDIO) {
		p_table = &pts_table[PTS_TYPE_AUDIO];
		last_checkin_pts = p_table->last_checkin_pts;
	} else {
		spin_unlock_irqrestore(&lock, flags);
		return -EINVAL;
	}

	spin_unlock_irqrestore(&lock, flags);
	return last_checkin_pts;
}
EXPORT_SYMBOL(get_last_checkin_pts);

/*
 *
 */

int get_last_checkout_pts(u8 type)
{
	struct pts_table_s *p_table = NULL;
	u32 last_checkout_pts = 0;
	ulong flags;

	spin_lock_irqsave(&lock, flags);
	if (type == PTS_TYPE_VIDEO) {
		p_table = &pts_table[PTS_TYPE_VIDEO];
		last_checkout_pts = p_table->last_checkout_pts;
	} else if (type == PTS_TYPE_AUDIO) {
		p_table = &pts_table[PTS_TYPE_AUDIO];
		last_checkout_pts = p_table->last_checkout_pts;
	} else {
		spin_unlock_irqrestore(&lock, flags);
		return -EINVAL;
	}

	spin_unlock_irqrestore(&lock, flags);
	return last_checkout_pts;
}
EXPORT_SYMBOL(get_last_checkout_pts);

int pts_lookup(u8 type, u32 *val, u32 *frame_size, u32 pts_margin)
{
	u32 page, offset;

	get_rdpage_offset(type, &page, &offset);

	if (type == PTS_TYPE_VIDEO) {
		offset = page * pts_table[PTS_TYPE_VIDEO].buf_size + offset;
		pts_lookup_offset(PTS_TYPE_VIDEO, offset, val, frame_size,
				  pts_margin);
		return 0;
	} else if (type == PTS_TYPE_AUDIO) {
		offset = page * pts_table[PTS_TYPE_AUDIO].buf_size + offset;
		pts_lookup_offset(PTS_TYPE_AUDIO, offset, val, frame_size,
				  pts_margin);
		return 0;
	} else {
		return -EINVAL;
	}
}
EXPORT_SYMBOL(pts_lookup);
static int pts_lookup_offset_inline_locked(u8 type, u32 offset, u32 *val,
					   u32 *frame_size, u32 pts_margin,
					   u64 *us64)
{
	struct pts_table_s *p_table;
	int lookup_threshold;

	int look_cnt = 0;

	if (type >= PTS_TYPE_MAX)
		return -EINVAL;

	p_table = &pts_table[type];

	if (pts_margin == 0)
		lookup_threshold = p_table->lookup_threshold;
	else
		lookup_threshold = pts_margin;

	if (!p_table->first_lookup_ok)
		lookup_threshold <<= 1;

	if (likely(p_table->status == PTS_RUNNING)) {
		struct pts_rec_s *p = NULL;
		struct pts_rec_s *p2 = NULL;

		if (p_table->lookup_cache_valid &&
		    offset == p_table->lookup_cache_offset) {
			*val = p_table->lookup_cache_pts;
			*us64 = p_table->lookup_cache_pts_us64;
			return 0;
		}

		if (type == PTS_TYPE_VIDEO &&
		    !list_empty(&p_table->valid_list)) {
			struct pts_rec_s *rec = NULL;
			struct pts_rec_s *next = NULL;
			int look_cnt1 = 0;

			list_for_each_entry_safe(rec, next,
						 &p_table->valid_list, list) {
				if (OFFSET_DIFF(offset, rec->offset) >
					PTS_VALID_OFFSET_TO_CHECK) {
					if (p_table->pts_search == &rec->list)
						p_table->pts_search =
						rec->list.next;

					if (tsync_get_debug_vpts()) {
						pr_info("remove node  offset: 0x%x cnt:%d\n",
							rec->offset, look_cnt1);
					}

					list_move_tail(&rec->list,
						       &p_table->free_list);
					look_cnt1++;
				} else {
					break;
				}
			}
		}

		if (list_empty(&p_table->valid_list))
			return -1;

		if (p_table->pts_search == &p_table->valid_list) {
			p = list_entry(p_table->valid_list.next,
				       struct pts_rec_s, list);
		} else {
			p = list_entry(p_table->pts_search, struct pts_rec_s,
				       list);
		}

		if (OFFSET_LATER(offset, p->offset)) {
			p2 = p;	/* lookup candidate */

			list_for_each_entry_continue(p, &p_table->valid_list,
						     list) {
				look_cnt++;

				if (OFFSET_LATER(p->offset, offset))
					break;

				if (type == PTS_TYPE_AUDIO) {
					list_move_tail(&p2->list,
						       &p_table->free_list);
				}

				p2 = p;
			}

			/* if p2 lookup fail, set p2 = p */
			if (type == PTS_TYPE_VIDEO && p2 && p &&
			    OFFSET_DIFF(offset, p2->offset) >
							lookup_threshold &&
			    abs(OFFSET_DIFF(offset, p->offset)) <
							lookup_threshold)
				p2 = p;
		} else if (OFFSET_LATER(p->offset, offset)) {
			list_for_each_entry_continue_reverse(p,
							     &p_table
							     ->valid_list,
							     list) {
#ifdef DEBUG
				look_cnt++;
#endif
				if (OFFSET_EQLATER(offset, p->offset)) {
					p2 = p;
					break;
				}
			}
		} else {
			p2 = p;
		}

		if (type == PTS_TYPE_VIDEO)
			*frame_size = p->size;

		if ((p2) &&
		    (OFFSET_DIFF(offset, p2->offset) < lookup_threshold)) {
			if (p2->val == 0)	/* FFT: set valid vpts */
				p2->val = 1;
			if (tsync_get_debug_pts_checkout()) {
				if (tsync_get_debug_vpts() &&
				    type == PTS_TYPE_VIDEO) {
					pr_info("vpts look up offset<0x%x> -->",
						offset);
					pr_info("<0x%x:0x%x>, fsize %x, look_cnt = %d\n",
						p2->offset, p2->val,
						p2->size, look_cnt);
				}

				if (tsync_get_debug_apts() &&
				    type == PTS_TYPE_AUDIO) {
					pr_info("apts look up offset<0x%x> -->",
						offset);
					pr_info("<0x%x:0x%x>, look_cnt = %d\n",
						p2->offset, p2->val, look_cnt);
				}
			}
			*val = p2->val;
			*us64 = p2->pts_us64;
			*frame_size = p2->size;

#ifdef CALC_CACHED_TIME
			p_table->last_checkout_pts = p2->val;
			p_table->last_checkout_offset = offset;
#endif

			p_table->lookup_cache_pts = *val;
			p_table->lookup_cache_pts_us64 = *us64;
			p_table->lookup_cache_offset = offset;
			p_table->lookup_cache_valid = true;

			/* update next look up search start point */
			p_table->pts_search = p2->list.prev;

			list_move_tail(&p2->list, &p_table->free_list);

			if (!p_table->first_lookup_ok) {
				p_table->first_lookup_ok = 1;
				if (type == PTS_TYPE_VIDEO)
					timestamp_firstvpts_set(*val);
				if (tsync_get_debug_pts_checkout()) {
					if (tsync_get_debug_vpts() &&
					    type == PTS_TYPE_VIDEO) {
						pr_info("first vpts look up");
						pr_info("offset<0x%x> -->",
							offset);
						pr_info("<0x%x:0x%x> ok!\n",
							p2->offset,
							p2->val);
					}
					if (tsync_get_debug_apts() &&
					    type == PTS_TYPE_AUDIO) {
						pr_info("first apts look up");
						pr_info("offset<0x%x> -->",
							offset);
						pr_info("<0x%x:0x%x> ok!\n",
							p2->offset,
							p2->val);
					}
				}
			}
			return 0;
		}
#ifdef INTERPOLATE_AUDIO_PTS
		else if ((type == PTS_TYPE_AUDIO) &&
			 p2 &&
			 (!list_is_last(&p2->list,
					&p_table->valid_list))) {
			p = list_entry(p2->list.next, struct pts_rec_s, list);
			if (VAL_DIFF(p->val, p2->val) <
				INTERPOLATE_AUDIO_RESOLUTION &&
				 (VAL_DIFF(p->val, p2->val) >= 0)) {
				/* do interpolation between [p2, p] */
				*val =
					div_u64((((u64)p->val - p2->val) *
						(offset - p2->offset)),
						(p->offset - p2->offset)) +
					p2->val;
				*us64 = (u64)(*val) << 32;

				if (tsync_get_debug_pts_checkout() &&
				    tsync_get_debug_apts() &&
				    type == PTS_TYPE_AUDIO) {
					pr_info("apts look up offset");
					pr_info("<0x%x> --><0x%x> ",
						offset, *val);
					pr_info("<0x%x:0x%x>-<0x%x:0x%x>\n",
						p2->offset, p2->val,
						p->offset, p->val);
				}
#ifdef CALC_CACHED_TIME
				p_table->last_checkout_pts = *val;
				p_table->last_checkout_offset = offset;

#endif
				p_table->lookup_cache_pts = *val;
				p_table->lookup_cache_offset = offset;
				p_table->lookup_cache_valid = true;

				/* update next look up search start point */
				p_table->pts_search = p2->list.prev;

				list_move_tail(&p2->list, &p_table->free_list);

				if (!p_table->first_lookup_ok) {
					p_table->first_lookup_ok = 1;
					if (tsync_get_debug_pts_checkout() &&
					    tsync_get_debug_apts() &&
					    type == PTS_TYPE_AUDIO) {
						pr_info("first apts look up");
						pr_info("offset<0x%x>", offset);
						pr_info("--> <0x%x> ", *val);
						pr_info("<0x%x:0x%x>",
							p2->offset,
							p2->val);
						pr_info("-<0x%x:0x%x>\n",
							p->offset,
							p->val);
					}
				}
				return 0;
			}
		}
#endif
		else {
			/*
			 *when first pts lookup failed,
			 * use first checkin pts instead
			 */
			if (!p_table->first_lookup_ok) {
				*val = p_table->first_checkin_pts;
				*us64 = p_table->first_checkin_pts_us64;
				p_table->first_lookup_ok = 1;
				p_table->first_lookup_is_fail = 1;

				if (type == PTS_TYPE_VIDEO) {
					if (timestamp_vpts_get() == 0)
						timestamp_firstvpts_set(1);
					else
						timestamp_firstvpts_set
						(timestamp_vpts_get());
				}

				if (tsync_get_debug_pts_checkout()) {
					if (tsync_get_debug_vpts() &&
					    type == PTS_TYPE_VIDEO) {
						pr_info("first vpts look up");
						pr_info(" offset<0x%x> failed,",
							offset);
						pr_info("return ");
						pr_info("first_checkin_pts");
						pr_info("<0x%x>\n", *val);
					}
					if (tsync_get_debug_apts() &&
					    type == PTS_TYPE_AUDIO) {
						pr_info("first apts look up");
						pr_info(" offset<0x%x> failed,",
							offset);
						pr_info("return ");
						pr_info("first_checkin_pts");
						pr_info("<0x%x>\n", *val);
					}
				}

				return 0;
			}

			if (tsync_get_debug_pts_checkout()) {
				if (tsync_get_debug_vpts() &&
				    type == PTS_TYPE_VIDEO) {
					pr_info("vpts look up offset<0x%x> ",
						offset);
					pr_info("failed,look_cnt = %d\n",
						look_cnt);
				}
				if (tsync_get_debug_apts() &&
				    type == PTS_TYPE_AUDIO) {
					pr_info("apts look up offset<0x%x> ",
						offset);
					pr_info("failed,look_cnt = %d\n",
						look_cnt);
				}
			}

			return -1;
		}
	}

	return -1;
}

static int pts_pick_by_offset_inline_locked(u8 type, u32 offset, u32 *val,
					    u32 pts_margin, u64 *us64)
{
	struct pts_table_s *p_table;
	int lookup_threshold;

	int look_cnt = 0;

	if (type >= PTS_TYPE_MAX)
		return -EINVAL;

	p_table = &pts_table[type];

	if (pts_margin == 0)
		lookup_threshold = p_table->lookup_threshold;
	else
		lookup_threshold = pts_margin;

	if (!p_table->first_lookup_ok)
		lookup_threshold <<= 1;

	if (likely(p_table->status == PTS_RUNNING)) {
		struct pts_rec_s *p = NULL;
		struct pts_rec_s *p2 = NULL;

		if (p_table->lookup_cache_valid &&
		    offset == p_table->lookup_cache_offset) {
			*val = p_table->lookup_cache_pts;
			return 0;
		}

		if (type == PTS_TYPE_VIDEO &&
		    !list_empty(&p_table->valid_list)) {
			struct pts_rec_s *rec = NULL;
			struct pts_rec_s *next = NULL;
			int look_cnt1 = 0;

			list_for_each_entry_safe(rec, next,
						 &p_table->valid_list, list) {
				if (OFFSET_DIFF(offset, rec->offset) >
					PTS_VALID_OFFSET_TO_CHECK) {
					if (p_table->pts_search == &rec->list)
						p_table->pts_search =
						rec->list.next;

					if (tsync_get_debug_vpts()) {
						pr_info("remove node  offset: 0x%x cnt:%d\n",
							rec->offset, look_cnt1);
					}

					list_move_tail(&rec->list,
						       &p_table->free_list);
					look_cnt1++;
				} else {
					break;
				}
			}
		}

		if (list_empty(&p_table->valid_list))
			return -1;

		if (p_table->pts_search == &p_table->valid_list) {
			p = list_entry(p_table->valid_list.next,
				       struct pts_rec_s, list);
		} else {
			p = list_entry(p_table->pts_search, struct pts_rec_s,
				       list);
		}

		if (OFFSET_LATER(offset, p->offset)) {
			p2 = p;	/* lookup candidate */

			list_for_each_entry_continue(p, &p_table->valid_list,
						     list) {
				look_cnt++;

				if (OFFSET_LATER(p->offset, offset))
					break;

				p2 = p;
			}
		} else if (OFFSET_LATER(p->offset, offset)) {
			list_for_each_entry_continue_reverse(p,
							     &p_table
							     ->valid_list,
							     list) {
#ifdef DEBUG
				look_cnt++;
#endif
				if (OFFSET_EQLATER(offset, p->offset)) {
					p2 = p;
					break;
				}
			}
		} else {
			p2 = p;
		}

		if ((p2) &&
		    (OFFSET_DIFF(offset, p2->offset) < lookup_threshold)) {
			if (tsync_get_debug_pts_checkout()) {
				if (tsync_get_debug_vpts() &&
				    type == PTS_TYPE_VIDEO) {
					pr_info
					("vpts look up offset<0x%x> -->",
					 offset);
					pr_info
					("<0x%x:0x%x>, look_cnt = %d\n",
					 p2->offset, p2->val, look_cnt);
				}

				if (tsync_get_debug_apts() &&
				    type == PTS_TYPE_AUDIO) {
					pr_info
					("apts look up offset<0x%x> -->",
					 offset);
					pr_info
					("<0x%x:0x%x>, look_cnt = %d\n",
					 p2->offset, p2->val, look_cnt);
				}
			}
			*val = p2->val;
			*us64 = p2->pts_us64;

			return 0;
		}
	}

	return -1;
}

static int pts_lookup_offset_inline(u8 type, u32 offset, u32 *val,
				    u32 *frame_size, u32 pts_margin, u64 *us64)
{
	unsigned long flags;
	int res;

	spin_lock_irqsave(&lock, flags);
	res = pts_lookup_offset_inline_locked(type, offset, val,
					      frame_size, pts_margin, us64);
	if (timestamp_firstapts_get() == 0 && res == 0 && (*val) != 0 &&
	    type == PTS_TYPE_AUDIO)
		timestamp_firstapts_set(*val);
	spin_unlock_irqrestore(&lock, flags);

	return res;
}

static int pts_pick_by_offset_inline(u8 type, u32 offset, u32 *val,
				     u32 pts_margin, u64 *us64)
{
	unsigned long flags;
	int res;

	spin_lock_irqsave(&lock, flags);
	res = pts_pick_by_offset_inline_locked(type, offset, val, pts_margin,
					       us64);

	spin_unlock_irqrestore(&lock, flags);

	return res;
}

int pts_lookup_offset(u8 type, u32 offset, u32 *val,
		      u32 *frame_size, u32 pts_margin)
{
	u64 pts_us;

	return pts_lookup_offset_inline(type, offset, val,
				frame_size, pts_margin, &pts_us);
}
EXPORT_SYMBOL(pts_lookup_offset);

int pts_lookup_offset_us64(u8 type, u32 offset, u32 *val,
			   u32 *frame_size, u32 pts_margin, u64 *us64)
{
	return pts_lookup_offset_inline(type, offset, val,
				frame_size, pts_margin, us64);
}
EXPORT_SYMBOL(pts_lookup_offset_us64);

int pts_pickout_offset_us64(u8 type, u32 offset, u32 *val, u32 pts_margin,
			    u64 *us64)
{
	return pts_pick_by_offset_inline(type, offset, val, pts_margin, us64);
}
EXPORT_SYMBOL(pts_pickout_offset_us64);

int pts_set_resolution(u8 type, u32 level)
{
	if (type >= PTS_TYPE_MAX)
		return -EINVAL;

	pts_table[type].lookup_threshold = level;
	return 0;
}
EXPORT_SYMBOL(pts_set_resolution);

int pts_set_rec_size(u8 type, u32 val)
{
	ulong flags;

	if (type >= PTS_TYPE_MAX)
		return -EINVAL;

	spin_lock_irqsave(&lock, flags);

	if (pts_table[type].status == PTS_IDLE) {
		pts_table[type].rec_num = val;

		spin_unlock_irqrestore(&lock, flags);

		return 0;

	} else {
		spin_unlock_irqrestore(&lock, flags);

		return -EBUSY;
	}
}
EXPORT_SYMBOL(pts_set_rec_size);

/**
 * return number of recs if the offset is bigger
 */
int pts_get_rec_num(u8 type, u32 val)
{
	ulong flags;
	struct pts_table_s *p_table;
	struct pts_rec_s *p;
	int r = 0;

	if (type >= PTS_TYPE_MAX)
		return 0;

	p_table = &pts_table[type];

	spin_lock_irqsave(&lock, flags);

	if (p_table->status != PTS_RUNNING)
		goto out;

	if (list_empty(&p_table->valid_list))
		goto out;

	if (p_table->pts_search == &p_table->valid_list) {
		p = list_entry(p_table->valid_list.next,
			       struct pts_rec_s, list);
	} else {
		p = list_entry(p_table->pts_search, struct pts_rec_s,
			       list);
	}

	if (OFFSET_LATER(val, p->offset)) {
		list_for_each_entry_continue(p, &p_table->valid_list,
					     list) {
			if (OFFSET_LATER(p->offset, val))
				break;
		}
	}

	list_for_each_entry_continue(p, &p_table->valid_list, list) {
		r++;
	}

out:
	spin_unlock_irqrestore(&lock, flags);

	return r;
}
EXPORT_SYMBOL(pts_get_rec_num);

/* #define SIMPLE_ALLOC_LIST */
static void free_pts_list(struct pts_table_s *p_table)
{
#ifdef SIMPLE_ALLOC_LIST
	if (0) {		/*don't free,used a static memory */
		kfree(p_table->pts_recs);
		p_table->pts_recs = NULL;
	}
#else
	unsigned long *p = p_table->pages_list;
	void *onepage = (void *)p[0];

	while (onepage) {
		free_page((unsigned long)onepage);
		p++;
		onepage = (void *)p[0];
	}
	kfree(p_table->pages_list);
	p_table->pages_list = NULL;
#endif
	INIT_LIST_HEAD(&p_table->valid_list);
	INIT_LIST_HEAD(&p_table->free_list);
}

static int alloc_pts_list(struct pts_table_s *p_table)
{
	int i;
	int page_nums;

	INIT_LIST_HEAD(&p_table->valid_list);
	INIT_LIST_HEAD(&p_table->free_list);
#ifdef SIMPLE_ALLOC_LIST
	if (!p_table->pts_recs) {
		p_table->pts_recs = kcalloc(p_table->rec_num,
					    sizeof(struct pts_rec_s),
					    GFP_KERNEL);
	}
	if (!p_table->pts_recs) {
		p_table->status = 0;
		return -ENOMEM;
	}
	for (i = 0; i < p_table->rec_num; i++)
		list_add_tail(&p_table->pts_recs[i].list, &p_table->free_list);
	return 0;
#else
	page_nums = p_table->rec_num * sizeof(struct pts_rec_s) / PAGE_SIZE;
	if (PAGE_SIZE / sizeof(struct pts_rec_s) != 0) {
		page_nums =
			(p_table->rec_num + page_nums +
			 1) * sizeof(struct pts_rec_s) / PAGE_SIZE;
	}
	p_table->pages_list = kzalloc((page_nums + 1) * sizeof(void *),
				      GFP_KERNEL);
	if (!p_table->pages_list)
		return -ENOMEM;
	for (i = 0; i < page_nums; i++) {
		int j;
		void *one_page = (void *)__get_free_page(GFP_KERNEL);
		struct pts_rec_s *recs = one_page;

		if (!one_page)
			goto error_alloc_pages;
		for (j = 0; j < PAGE_SIZE / sizeof(struct pts_rec_s); j++)
			list_add_tail(&recs[j].list, &p_table->free_list);
		p_table->pages_list[i] = (unsigned long)one_page;
	}
	p_table->pages_list[page_nums] = 0;
	return 0;
error_alloc_pages:
	free_pts_list(p_table);
#endif
	return -ENOMEM;
}

int pts_start(u8 type)
{
	ulong flags;
	struct pts_table_s *p_table;

	if (type >= PTS_TYPE_MAX)
		return -EINVAL;
	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	if (has_hevc_vdec() && type == PTS_TYPE_HEVC) {
		p_table = &pts_table[PTS_TYPE_VIDEO];
		p_table->hevc = 1;
	} else {
		/* #endif */
		p_table = &pts_table[type];
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_M8B)
			p_table->hevc = 0;
		/* #endif */
	}

	spin_lock_irqsave(&lock, flags);

	if (likely(p_table->status == PTS_IDLE)) {
		p_table->status = PTS_INIT;

		spin_unlock_irqrestore(&lock, flags);

		if (alloc_pts_list(p_table) != 0)
			return -ENOMEM;
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
		if (has_hevc_vdec() && type == PTS_TYPE_HEVC) {
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
			p_table->buf_start =
				READ_PARSER_REG(PARSER_VIDEO_START_PTR);
			p_table->buf_size =
				READ_PARSER_REG(PARSER_VIDEO_END_PTR)
				- p_table->buf_start + 8;
#else
			p_table->buf_start = READ_VREG(HEVC_STREAM_START_ADDR);
			p_table->buf_size = READ_VREG(HEVC_STREAM_END_ADDR)
							- p_table->buf_start;
#endif
			timestamp_vpts_set(0);
			timestamp_pcrscr_set(0);
			/* video always need the pcrscr,*/
			/*Clear it to use later */

			timestamp_firstvpts_set(0);
			p_table->first_checkin_pts = -1;
			p_table->first_lookup_ok = 0;
			p_table->first_lookup_is_fail = 0;
		} else {
			/* #endif */
			if (type == PTS_TYPE_VIDEO) {
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
				p_table->buf_start =
					READ_PARSER_REG(PARSER_VIDEO_START_PTR);
				p_table->buf_size =
					READ_PARSER_REG(PARSER_VIDEO_END_PTR)
					- p_table->buf_start + 8;
#else
				p_table->buf_start =
					READ_VREG(VLD_MEM_VIFIFO_START_PTR);
				p_table->buf_size =
					READ_VREG(VLD_MEM_VIFIFO_END_PTR)
					- p_table->buf_start + 8;
#endif
				/* since the HW buffer wrap counter only have
				 * 16 bits, a too small buf_size will make pts i
				 * lookup fail with streaming offset wrapped
				 * before 32 bits boundary.
				 * This is unlikely to set such a small
				 * streaming buffer though.
				 */
				/* BUG_ON(p_table->buf_size <= 0x10000); */
				timestamp_vpts_set(0);
				timestamp_pcrscr_set(0);
				/* video always need the pcrscr, */
				/*Clear it to use later*/

				timestamp_firstvpts_set(0);
				p_table->first_checkin_pts = -1;
				p_table->first_lookup_ok = 0;
				p_table->first_lookup_is_fail = 0;
			} else if (type == PTS_TYPE_AUDIO) {
				p_table->buf_start =
					READ_AIU_REG(AIU_MEM_AIFIFO_START_PTR);
				p_table->buf_size =
					READ_AIU_REG(AIU_MEM_AIFIFO_END_PTR)
					- p_table->buf_start + 8;

				/* BUG_ON(p_table->buf_size <= 0x10000); */
				timestamp_apts_set(0);
				timestamp_firstapts_set(0);
				p_table->first_checkin_pts = -1;
				p_table->first_lookup_ok = 0;
				p_table->first_lookup_is_fail = 0;
			}
		}
#ifdef CALC_CACHED_TIME
		p_table->last_checkin_offset = 0;
		p_table->last_checkin_pts = -1;
		p_table->last_checkout_pts = -1;
		p_table->last_checkout_offset = -1;
		p_table->last_avg_bitrate = 0;
		p_table->last_bitrate = 0;
#endif

		p_table->pts_search = &p_table->valid_list;
		p_table->status = PTS_LOADING;
		p_table->lookup_cache_valid = false;

		return 0;

	} else {
		spin_unlock_irqrestore(&lock, flags);

		return -EBUSY;
	}
}
EXPORT_SYMBOL(pts_start);

int pts_stop(u8 type)
{
	ulong flags;
	struct pts_table_s *p_table;

	if (type >= PTS_TYPE_MAX)
		return -EINVAL;
	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	if (has_hevc_vdec() && type == PTS_TYPE_HEVC)
		p_table = &pts_table[PTS_TYPE_VIDEO];
	else
		/* #endif */
		p_table = &pts_table[type];

	spin_lock_irqsave(&lock, flags);

	if (likely(p_table->status == PTS_RUNNING ||
		   p_table->status == PTS_LOADING)) {
		p_table->status = PTS_DEINIT;

		spin_unlock_irqrestore(&lock, flags);

		free_pts_list(p_table);

		p_table->status = PTS_IDLE;

		if (type == PTS_TYPE_AUDIO)
			timestamp_apts_set(-1);
		tsync_mode_reinit();
		return 0;

	} else {
		spin_unlock_irqrestore(&lock, flags);

		return -EBUSY;
	}
}
EXPORT_SYMBOL(pts_stop);

int first_lookup_pts_failed(u8 type)
{
	struct pts_table_s *p_table;

	if (type >= PTS_TYPE_MAX)
		return -EINVAL;

	p_table = &pts_table[type];

	return p_table->first_lookup_is_fail;
}
EXPORT_SYMBOL(first_lookup_pts_failed);

int first_pts_checkin_complete(u8 type)
{
	struct pts_table_s *p_table;

	if (type >= PTS_TYPE_MAX)
		return -EINVAL;

	p_table = &pts_table[type];

	if (p_table->first_checkin_pts == -1)
		return 0;
	else
		return 1;
}
EXPORT_SYMBOL(first_pts_checkin_complete);
