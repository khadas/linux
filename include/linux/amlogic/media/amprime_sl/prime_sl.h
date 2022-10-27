/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _PRIMSE_SL_H_
#define _PRIMSE_SL_H_

#include <linux/types.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/vfm/vframe.h>

#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define SL_VSYNC_WR_MPEG_REG(adr, val) WRITE_VPP_REG(adr, val)
#define SL_VSYNC_RD_MPEG_REG(adr) READ_VPP_REG(adr)
#define SL_VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
	WRITE_VPP_REG_BITS(adr, val, start, len)
#else
#define SL_VSYNC_RD_MPEG_REG(adr) VSYNC_RD_MPEG_REG(adr)
#define SL_VSYNC_WR_MPEG_REG(adr, val) VSYNC_WR_MPEG_REG(adr, val)
#define SL_VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
	VSYNC_WR_MPEG_REG_BITS(adr, val, start, len)
#endif

void prime_sl_process(struct vframe_s *vf);
bool is_prime_sl_enable(void);
bool is_prime_sl_on(void);

#endif
