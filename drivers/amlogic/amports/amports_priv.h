/*
 * drivers/amlogic/amports/amports_priv.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#ifndef AMPORTS_PRIV_HEAD_HH
#define AMPORTS_PRIV_HEAD_HH
#include "streambuf.h"
#include "amports_gate.h"
#include <linux/amlogic/amports/vframe.h>
#include "arch/firmware.h"
#include "arch/register.h"
#include "arch/log.h"


struct port_priv_s {
	struct vdec_s *vdec;
	struct stream_port_s *port;
};

struct stream_buf_s *get_buf_by_type(u32 type);

/*video.c provide*/
extern u32 trickmode_i;
struct amvideocap_req;
extern u32 set_blackout_policy(int policy);
extern u32 get_blackout_policy(void);
int calculation_stream_ext_delayed_ms(u8 type);
int ext_get_cur_video_frame(struct vframe_s **vf, int *canvas_index);
int ext_put_video_frame(struct vframe_s *vf);
int ext_register_end_frame_callback(struct amvideocap_req *req);
int amstream_request_firmware_from_sys(const char *file_name,
		char *buf, int size);
void set_vsync_pts_inc_mode(int inc);

void set_real_audio_info(void *arg);
#define dbg() pr_info("on %s,line %d\n", __func__, __LINE__);

struct device *amports_get_dma_device(void);
struct device *get_codec_cma_device(void);
int amports_get_debug_flags(void);

#endif
