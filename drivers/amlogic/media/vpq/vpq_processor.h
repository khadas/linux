/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_PROCESSOR_H__
#define __VPQ_PROCESSOR_H__

#include "vpq_common.h"
#include "vpq_drv.h"

int vpq_process_set_frame(struct vpq_dev_s *dev, struct vpq_frame_info_s *frame_info);
int vpq_processor_set_frame_status(enum vpq_frame_status_e status);

#endif //__VPQ_PROCESSOR_H__
