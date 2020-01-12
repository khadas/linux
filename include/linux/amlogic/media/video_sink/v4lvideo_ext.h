/*
 * include/linux/amlogic/media/video_sink/v4lvideo_ext.h
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

#ifndef V4LVIDEO_EXT_H
#define V4LVIDEO_EXT_H

int v4lvideo_assign_map(char **receiver_name, int *inst);

int v4lvideo_alloc_map(int *inst);

void v4lvideo_release_map(int inst);

struct file_private_data {
	struct vframe_s vf;
	struct vframe_s *vf_p;
	bool is_keep;
	int keep_id;
	int keep_head_id;
	struct file *file;
	ulong v4l_dev_handle;
	ulong v4l_inst_handle;
	u32 v4l_inst_id;
};

struct v4l_data_t {
	struct vframe_s *vf;
	char *dst_addr;
};

void v4lvideo_data_copy(struct v4l_data_t *v4l_data);
struct vframe_s *v4lvideo_get_vf(int fd);
void dim_post_keep_cmd_release2(struct vframe_s *vframe);

#endif /* V4LVIDEO_EXT_H */

