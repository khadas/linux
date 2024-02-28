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

#define PRINT_ERROR		0X0
#define PRINT_OTHER		0X0001
#define PRINT_MORE		0X0080

struct uvm_di_mgr_t {
	//struct vframe_s *vf;
	//struct vframe_s *vf_1;   //n-1
	//struct vframe_s *vf_2;   //n-2
	//bool di_processed_flag;  /*true: di process done; false: dropped*/
	struct dp_buf_mgr_t *buf_mgr;
	struct file *file;
	struct uvm_hook_mod *uhmod_v4lvideo;
	struct uvm_hook_mod *uhmod_dec;
};

int buf_mgr_free_checkin(struct dp_buf_mgr_t *buf_mgr, struct file *file);
void buf_mgr_file_lock(struct uvm_di_mgr_t *uvm_di_mgr);
void buf_mgr_file_unlock(struct uvm_di_mgr_t *uvm_di_mgr);
int update_di_process_state(struct file *file);
