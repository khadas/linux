/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#ifndef STREAMBUF_H
#define STREAMBUF_H
#include <linux/amlogic/media/utils/amports_config.h>

#define BUF_FLAG_ALLOC          0x01
#define BUF_FLAG_IN_USE         0x02
#define BUF_FLAG_PARSER         0x04
#define BUF_FLAG_FIRST_TSTAMP   0x08
#define BUF_FLAG_IOMEM          0x10

#define BUF_TYPE_VIDEO      0
#define BUF_TYPE_AUDIO      1
#define BUF_TYPE_SUBTITLE   2
#define BUF_TYPE_USERDATA   3
#define BUF_TYPE_HEVC       4
#define BUF_MAX_NUM         5

#define INVALID_PTS 0xffffffff

#define FETCHBUF_SIZE   (64*1024)
#define USER_DATA_SIZE  (8*1024)

/* stream_buffer_metainfo stbuf_flag */
#define STBUF_META_FLAG_SECURE		(1 << 0)
#define STBUF_META_FLAG_PTS_SERV	(1 << 1)	/* use pts server flag */
#define STBUF_META_FLAG_XXX1		(1 << 2)

struct vdec_s;
struct stream_buf_s;

struct parser_args {
	u32 vid;
	u32 aid;
	u32 sid;
	u32 pcrid;
};

struct stream_buf_ops {
	int (*init) (struct stream_buf_s *, struct vdec_s *);
	void (*release) (struct stream_buf_s *);
	int (*write) (struct stream_buf_s *, const u8 *, u32);
	u32 (*get_wp) (struct stream_buf_s *);
	void (*set_wp) (struct stream_buf_s *, u32);
	u32 (*get_rp) (struct stream_buf_s *);
	void (*set_rp) (struct stream_buf_s *, u32);
};

struct stream_buf_s {
	int id;
	u8 name[16];
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
	bool is_multi_inst;
	bool no_parser;
	bool is_phybuf;
	bool is_hevc;
	bool use_ptsserv;
	u32 drm_flag;
	ulong ext_buf_addr;
	atomic_t payload;
	u32 stream_offset;
	struct parser_args pars;
	struct stream_buf_ops *ops;
	u32 last_offset[2];
	u32 write_count;
} /*stream_buf_t */;

struct stream_port_s {
	/* driver info */
	const char *name;
	struct device *class_dev;
	const struct file_operations *fops;

	/* ports control */
	s32 type;
	s32 flag;
	s32 pcr_inited;

	/* decoder info */
	s32 vformat;
	s32 aformat;
	s32 achanl;
	s32 asamprate;
	s32 adatawidth;

	/* parser info */
	u32 vid;
	u32 aid;
	u32 sid;
	u32 pcrid;
	bool is_4k;
} /*stream_port_t */;
enum drm_level_e {
	DRM_LEVEL1 = 1,
	DRM_LEVEL2 = 2,
	DRM_LEVEL3 = 3,
	DRM_NONE = 4,
};

struct drm_info {
	enum drm_level_e drm_level;
	u32 drm_flag;
	u32 drm_hasesdata;
	u32 drm_priv;
	u32 drm_pktsize;
	u32 drm_pktpts;
	u32 drm_phy;
	u32 drm_vir;
	u32 drm_remap;
	u32 data_offset;
	u32 handle;
	u32 extpad[7];
} /*drminfo_t */;

struct stream_buffer_metainfo {
	union {
		u32 stbuf_start;
		u32 stbuf_pktaddr; //stbuf_pktaddr + stbuf_pktsize = wp
	};
	union {
		u32 stbuf_size;
		u32 stbuf_pktsize;
	};
	u32 stbuf_flag;
	u32 stbuf_private;
	u32 jump_back_flag;
	u32 pts_server_id;
	u32 reserved[14];
};

struct stream_buffer_status {
	u32 stbuf_wp;
	u32 stbuf_rp;
	u32 stbuf_start;
	u32 stbuf_size;
	u32 reserved[16];
};


#define TYPE_DRMINFO_V2  0x100
#define TYPE_DRMINFO   0x80
#define TYPE_PATTERN   0x40

struct vdec_s;

extern void *fetchbuf;

extern u32 stbuf_level(struct stream_buf_s *buf);
extern u32 stbuf_rp(struct stream_buf_s *buf);
extern u32 stbuf_space(struct stream_buf_s *buf);
extern u32 stbuf_size(struct stream_buf_s *buf);
extern u32 stbuf_canusesize(struct stream_buf_s *buf);
extern s32 stbuf_init(struct stream_buf_s *buf, struct vdec_s *vdec);
extern s32 stbuf_wait_space(struct stream_buf_s *stream_buf, size_t count);
extern void stbuf_release(struct stream_buf_s *buf);
extern int stbuf_change_size(struct stream_buf_s *buf, int size,
				bool is_secure);
extern int stbuf_fetch_init(void);
extern void stbuf_fetch_release(void);
extern u32 stbuf_sub_rp_get(void);
extern void stbuf_sub_rp_set(unsigned int sub_rp);
extern u32 stbuf_sub_wp_get(void);
extern u32 stbuf_sub_start_get(void);
extern u32 stbuf_userdata_start_get(void);
extern struct stream_buf_s *get_stream_buffer(int id);

extern void stbuf_vdec2_init(struct stream_buf_s *buf);

u32 parser_get_wp(struct stream_buf_s *vb);
void parser_set_wp(struct stream_buf_s *vb, u32 val);
u32 parser_get_rp(struct stream_buf_s *vb);
void parser_set_rp(struct stream_buf_s *vb, u32 val);

#endif /* STREAMBUF_H */
