/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef CODEC_MM_KEEP_PRIV_HEADER
#define CODEC_MM_KEEP_PRIV_HEADER
int codec_mm_keeper_mgr_init(void);
int codec_mm_keeper_dump_info(void *buf, int size);
int codec_mm_keeper_free_all_keep(int force);

#endif
