/*
 * drivers/amlogic/media/di_multi/di_vframe.h
 *
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
 */

#ifndef __DI_VFRAME_H__
#define __DI_VFRAME_H__

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>

void dev_vframe_init(void);
void dev_vframe_exit(void);
void di_vframe_reg(unsigned int ch);
void di_vframe_unreg(unsigned int ch);

bool vf_type_is_prog(unsigned int type);
bool vf_type_is_interlace(unsigned int type);
bool vf_type_is_top(unsigned int type);
bool vf_type_is_bottom(unsigned int type);
bool vf_type_is_inter_first(unsigned int type);
bool vf_type_is_mvc(unsigned int type);
bool vf_type_is_no_video_en(unsigned int type);
bool vf_type_is_VIU422(unsigned int type);
bool vf_type_is_VIU_FIELD(unsigned int type);
bool vf_type_is_VIU_SINGLE(unsigned int type);
bool vf_type_is_VIU444(unsigned int type);
bool vf_type_is_VIUNV21(unsigned int type);
bool vf_type_is_vscale_dis(unsigned int type);
bool vf_type_is_canvas_toggle(unsigned int type);
bool vf_type_is_pre_interlace(unsigned int type);
bool vf_type_is_highrun(unsigned int type);
bool vf_type_is_compress(unsigned int type);
bool vf_type_is_pic(unsigned int type);
bool vf_type_is_scatter(unsigned int type);
bool vf_type_is_vd2(unsigned int type);

extern const char * const di_rev_name[4];

struct vframe_s *pw_vf_get(unsigned int ch);
struct vframe_s *pw_vf_peek(unsigned int ch);
void pw_vf_put(struct vframe_s *vf, unsigned int ch);
int pw_vf_notify_provider(unsigned int channel,
			  int event_type,
			  void *data);
int pw_vf_notify_receiver(unsigned int channel,
			  int event_type,
			  void *data);
void pw_vf_light_unreg_provider(unsigned int ch);

void set_bypass2_complete(unsigned int ch, bool on);
bool is_bypss_complete(struct dev_vfram_t *pvfm);
bool is_bypss2_complete(unsigned int ch);

#endif /*__DI_VFRAME_H__*/
