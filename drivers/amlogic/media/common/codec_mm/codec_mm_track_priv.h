/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _CODEC_MM_TRACKING_H
#define _CODEC_MM_TRACKING_H

int codec_mm_walk_dbuf(void);

int codec_mm_track_open(void **trk_out);

void codec_mm_track_close(void *trk_h);

#endif //_CODEC_MM_TRACKING_H

