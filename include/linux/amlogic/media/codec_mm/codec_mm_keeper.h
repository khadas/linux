/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef CODEC_MM_KEEPER_HEADER
#define CODEC_MM_KEEPER_HEADER
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video_keeper.h>
#endif

#define MEM_TYPE_CODEC_MM			111
#define MEM_TYPE_CODEC_MM_SCATTER	222

/*
 *don't call in interrupt;
 */
int codec_mm_keeper_mask_keep_mem(void *mem_handle, int type);

int is_codec_mm_kept(void *mem_handle);
/*
 *can call in irq
 */
int codec_mm_keeper_unmask_keeper(int keep_id, int delayms);

#ifndef CONFIG_AMLOGIC_MEDIA_VIDEO
static inline void try_free_keep_video(int flags) { return; }
#endif

#endif
