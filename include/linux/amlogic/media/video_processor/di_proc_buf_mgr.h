/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
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

#include <linux/types.h>
#include <linux/atomic.h>

#define VF_LIST_COUNT 64

#define VF_REF_FLAG_DIPROCESS_CHECKIN             0x1
#define VF_REF_FLAG_DECREASE_SELF                 0x2

enum dec_type_t {
	DEC_TYPE_V4L_DEC = 0,
	DEC_TYPE_V4LVIDEO = 1,
	DEC_TYPE_TVIN = 2,
	DEC_TYPE_MAX = 0xff,
};

struct vf_ref_t {
	int index; /*0 1 2....*/
	int omx_index; /*for debug*/
	struct vframe_s *vf;
	//struct vframe_s *src_vf;
	int ref_count; /*ref by other frame, */
	int ref_number; /*ref by other frame*/
	int ref_other_number;/*ref other  number*/
	struct file *file;
	atomic_t on_use;
	bool di_processed;
	int buf_mgr_reset_id;
	int qbuf_id;
	u32 flag;
};

struct dp_buf_mgr_t {
	int dec_type;
	int instance_id;
	//struct vframe_s *vf_2;   /*last frame n-2*/
	//struct vframe_s *vf_1;   /*last frame n-1*/
	struct vf_ref_t *ref_list_2; /*last frame n-2*/
	struct vf_ref_t *ref_list_1; /*last frame n-1*/
	struct vf_ref_t ref_list[VF_LIST_COUNT];
	int get_count;
	void *caller_data;
	void (*recycle_buffer_cb)(void *caller_data, struct file *file, int instance_id);
	struct mutex ref_count_mutex; /*for ref_count*/
	struct mutex file_mutex; /*for file release mutex*/
	int reset_id;
	int receive_count;
};

/**
 * @brief  buf_mgr_creat  creat buf mgr instance
 *
 * @param[in]  dec_type    dec_type
 * @param[in]  id    decoder instance id, need always ++ after system boot
 * @param[in]  caller_data   decoder instance dev
 * @param[in]  recycle_buffer_cb   recycle vf to dec
 *
 * @return     struct buf_manager_t * for  success, or NULL for fail
 */
struct dp_buf_mgr_t *buf_mgr_creat(int dec_type, int id, void *caller_data,
	void (*recycle_buffer_cb)(void *, struct file *, int));

/**
 * @brief  buf_mgr_release  release buf mgr instance
 *
 * @param[in]  buf_manager_t *    buf_manager_t
 *
 * @return     0 for  success, or fail type if < 0
 */
int buf_mgr_release(struct dp_buf_mgr_t *buf_mgr);

/**
 * @brief  buf_mgr_reset  reset buf mgr instance for seek
 *
 * @param[in]  dp_buf_mgr_t *    dp_buf_mgr_t
 *
 * @return     0 for  success, or fail type if < 0
 */
int buf_mgr_reset(struct dp_buf_mgr_t *buf_mgr);

/**
 * @brief  buf_mgr_dq_checkin  dq buffer from decoder need checkin vf info
 *
 * @param[in]  buf_manager_t *buf_manager
 * @param[in]  file    file
 *
 * @return     0 for  success, or fail type if < 0
 */
int buf_mgr_dq_checkin(struct dp_buf_mgr_t *buf_mgr, struct file *file);

/**
 * @brief  buf_mgr_q_checkin  q buffer from hal need checkin vf info
 *
 * @param[in]  file    file
 *
 * @return     0 for  success, or fail type if < 0
 */
int buf_mgr_q_checkin(struct dp_buf_mgr_t *buf_mgr, struct file *file);

/**
 * @brief  get_di_proc_enable  judge whether the di post is enable
 *
 * @return  1 for enable, 0 for disenable
 */
int get_di_proc_enable(void);

