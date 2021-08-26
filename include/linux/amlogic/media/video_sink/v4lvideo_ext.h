/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
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

#define V4LVIDEO_FLAG_DI_NR      1
#define V4LVIDEO_FLAG_DI_DEC     2

#include <linux/dma-buf.h>
#include <linux/file.h>

int v4lvideo_assign_map(char **receiver_name, int *inst);

int v4lvideo_alloc_map(int *inst);

void v4lvideo_dec_count_increase(void);

void v4lvideo_dec_count_decrease(void);

void v4lvideo_release_map(int inst);

struct metadata {
	char *p_md;
	char *p_comp;
};

struct file_private_data {
	struct vframe_s vf;
	struct vframe_s *vf_p;
	bool is_keep;
	int keep_id;
	int keep_head_id;
	struct file *file;
	ulong vb_handle;
	ulong v4l_dec_ctx;
	u32 v4l_inst_id;
	struct vframe_s vf_ext;
	struct vframe_s *vf_ext_p;
	u32 flag;
	struct metadata md;
	void *private;
	struct file *cnt_file;
};

struct v4l_data_t {
	struct file_private_data *file_private_data;
	char *dst_addr;
	u32 phy_addr[3];
	int byte_stride;
	u32 width;
	u32 height;
};

void v4lvideo_data_copy(struct v4l_data_t *v4l_data,
				struct dma_buf *dmabuf, u32 align);
struct file_private_data *v4lvideo_get_vf(int fd);
void dim_post_keep_cmd_release2(struct vframe_s *vframe);
int is_v4lvideo_buf_file(struct file *file);
struct file *v4lvideo_alloc_file(void);
void v4lvideo_keep_vf(struct file *file);
struct file_private_data *v4lvideo_get_file_private_data(struct file *file_vf,
							 bool alloc_if_null);
void init_file_private_data(struct file_private_data *file_private_data);

s32 v4lvideo_import_sei_data(struct vframe_s *vf,
				    struct vframe_s *dup_vf,
				    char *provider);
s32 v4lvideo_release_sei_data(struct vframe_s *vf);


#endif /* V4LVIDEO_EXT_H */
