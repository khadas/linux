/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _CODEC_MM_TRACKING_H
#define _CODEC_MM_TRACKING_H

void codec_mm_dbuf_dump_help(void);

void codec_mm_dbuf_dump_config(u32 type);

int codec_mm_dbuf_walk(struct seq_file *m);

int codec_mm_sampling_open(void);

void codec_mm_sampling_close(void);

int codec_mm_track_init(void);

void codec_mm_track_exit(void);

#endif //_CODEC_MM_TRACKING_H

