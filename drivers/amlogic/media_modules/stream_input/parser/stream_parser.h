/*
 * drivers/amlogic/media/stream_input/parser/stream_parser.c
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#ifndef CONFIG_AMLOGIC_MEDIA_NO_PARSER
#include "tsdemux.h"
#include "psparser.h"
#include "esparser.h"
#include "rmparser.h"
#else

/* ES parser api */
static inline  s32 esparser_init(struct stream_buf_s *buf, struct vdec_s *vdec) { return -1; }
static inline  void esparser_release(struct stream_buf_s *buf) { return; }
static inline  s32 es_vpts_checkin(struct stream_buf_s *buf, u32 pts) { return -1; }
static inline  s32 es_vpts_checkin_us64(struct stream_buf_s *buf, u64 us64) { return -1; }
static inline  s32 es_apts_checkin(struct stream_buf_s *buf, u32 pts) { return -1; }
static inline  void esparser_audio_reset_s(struct stream_buf_s *buf) { return; }
static inline  void esparser_sub_reset(void) { return; }
static inline  ssize_t esparser_write(struct file *file,
				      struct stream_buf_s *stbuf,
				      const char __user *buf, size_t count)
				      { return -1; }
static inline  ssize_t drm_write(struct file *file, struct stream_buf_s *stbuf,
				 const char __user *buf, size_t count) { return -1; }

/* TS demux api */
static inline  s32 tsdemux_init(u32 vid, u32 aid, u32 sid, u32 pcrid, bool is_hevc,
				struct vdec_s *vdec) { return -1; }
static inline  void tsdemux_release(void) { return; }
static inline  void tsdemux_tsync_func_init(void) { return; }
static inline  void tsdemux_change_sid(u32 sid) { return; }
static inline  void tsdemux_change_avid(u32 vid, u32 aid) { return; }
static inline  void tsdemux_audio_reset(void) { return; }
static inline  void tsdemux_sub_reset(void) { return; }
static inline  void tsdemux_set_demux(int dev) { return; }
static inline  void tsdemux_set_skipbyte(int skipbyte) { return; }
static inline  int tsdemux_class_register(void) { return 0; }
static inline  void tsdemux_class_unregister(void) { return; }
static inline  int get_discontinue_counter(void) { return -1; }
static inline  ssize_t tsdemux_write(struct file *file,
				     struct stream_buf_s *vbuf,
				     struct stream_buf_s *abuf,
				     const char __user *buf, size_t count)
				     { return -1; }
static inline  ssize_t drm_tswrite(struct file *file,
				   struct stream_buf_s *vbuf,
				   struct stream_buf_s *abuf,
				   const char __user *buf, size_t count)
				   { return -1; }

/* PS parser api */
static inline s32 psparser_init(u32 vid, u32 aid, u32 sid, struct vdec_s *vdec) { return -1; }
static inline  void psparser_release(void) { return; }
static inline  void psparser_change_avid(unsigned int vid, unsigned int aid) { return; }
static inline  void psparser_change_sid(unsigned int sid) { return; }
static inline  void psparser_sub_reset(void) { return; }
static inline  void psparser_audio_reset(void) { return; }
static inline  u8 psparser_get_sub_info(struct subtitle_info **sub_infos) { return -1; }
static inline  u8 psparser_get_sub_found_num(void) { return 0; }
static inline  ssize_t psparser_write(struct file *file,
				      struct stream_buf_s *vbuf,
				      struct stream_buf_s *abuf,
				      const char __user *buf, size_t count)
				      { return -1; }

/* RM parser api */
static inline s32 rmparser_init(struct vdec_s *vdec) { return -1; }
static inline void rmparser_release(void) { return; }
static inline  void rm_audio_reset(void) { return; }
static inline  void rm_set_vasid(u32 vid, u32 aid) { return; }
static inline  ssize_t rmparser_write(struct file *file,
				      struct stream_buf_s *vbuf,
				      struct stream_buf_s *abuf,
				      const char __user *buf, size_t count)
				      { return -1; }
#endif

