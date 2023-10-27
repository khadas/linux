/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_H__
#define __VPQ_H__

#include <linux/amlogic/media/vfm/vframe.h>

void vpq_vfm_process(struct vframe_s *pvf);
void vpq_vfm_video_enable(int enable);

#endif
