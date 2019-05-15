/*
 * drivers/amlogic/media/frame_provider/decoder/utils/frame_check.h
 *
 * Copyright (C) 2016 Amlogic, Inc. All rights reserved.
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

#ifndef __FRAME_CHECK_H__
#define __FRAME_CHECK_H__


#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/kfifo.h>

#define FRAME_CHECK

#define YUV_MAX_DUMP_NUM  20

#define SIZE_CRC	64
#define SIZE_CHECK_Q 128

struct pic_dump_t{
	struct file *yuv_fp;
	loff_t yuv_pos;
	unsigned int start;
	unsigned int num;
	unsigned int end;
	unsigned int dump_cnt;

	unsigned int buf_size;
	char *buf_addr;
};

struct pic_check_t{
	struct file *check_fp;
	loff_t check_pos;

	struct file *compare_fp;
	loff_t compare_pos;
	unsigned int cmp_crc_cnt;
	void *fbc_planes[4];
	void *check_addr;
	DECLARE_KFIFO(new_chk_q, char *, SIZE_CHECK_Q);
	DECLARE_KFIFO(wr_chk_q, char *, SIZE_CHECK_Q);
};

struct pic_check_mgr_t{
	int id;
	int enable;
	unsigned int frame_cnt;
	/* pic info */
	unsigned int canvas_w;
	unsigned int canvas_h;
	unsigned int size_y;	//real size
	unsigned int size_uv;
	unsigned int size_pic;
	unsigned int last_size_pic;
	void *y_vaddr;
	void *uv_vaddr;
	ulong y_phyaddr;
	ulong uv_phyaddr;

	int file_cnt;
	atomic_t work_inited;
	struct work_struct frame_check_work;

	struct pic_check_t pic_check;
	struct pic_dump_t  pic_dump;
};

int dump_yuv_trig(struct pic_check_mgr_t *mgr,
	int id, int start, int num);

int decoder_do_frame_check(struct vdec_s *vdec, struct vframe_s *vf);

int frame_check_init(struct pic_check_mgr_t *mgr, int id);

void frame_check_exit(struct pic_check_mgr_t *mgr);

ssize_t frame_check_show(struct class *class,
		struct class_attribute *attr, char *buf);

ssize_t frame_check_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size);

ssize_t dump_yuv_show(struct class *class,
		struct class_attribute *attr, char *buf);

ssize_t dump_yuv_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t size);

void vdec_frame_check_exit(struct vdec_s *vdec);
int vdec_frame_check_init(struct vdec_s *vdec);

#endif /* __FRAME_CHECK_H__ */

