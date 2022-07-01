/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * drivers/amlogic/media/di_local/di_local.h
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

#ifndef __DI_LOCAL_H__
#define __DI_LOCAL_H__
#include <linux/amlogic/media/di/di_interface.h>
#include <linux/amlogic/media/di/di.h>

int get_current_vscale_skip_count(struct vframe_s *vf);

void dil_set_cpuver_flag(unsigned int para);

unsigned int dil_get_cpuver_flag(void);

struct di_ext_ops {
	unsigned int (*di_post_reg_rd)(unsigned int addr);
	int (*di_post_wr_reg_bits)(u32 adr, u32 val, u32 start, u32 len);
	void (*post_update_mc)(void);
	void (*post_keep_cmd_release2)(struct vframe_s *vframe);
	void (*polic_cfg)(unsigned int cmd, bool on);
	/* new interface */
	int (*new_create_instance)(struct di_init_parm parm);
	int (*new_destroy_instance)(int index);
	enum DI_ERRORTYPE (*new_empty_input_buffer)(int index, struct di_buffer *buffer);
	enum DI_ERRORTYPE (*new_fill_output_buffer)(int index, struct di_buffer *buffer);
	int (*new_get_state)(int index, struct di_status *status);
	int (*new_write)(struct di_buffer *buffer, struct composer_dst *dst);
	int (*new_release_keep_buf)(struct di_buffer *buffer);
	int (*new_get_output_buffer_num)(int index);
	int (*new_get_input_buffer_num)(int index);
	int (*pre_vpp_link_display)(struct vframe_s *vfm,
				    struct pvpp_dis_para_in_s *in_para, void *out_para);
	int (*pre_vpp_link_check_vf)(struct vframe_s *vfm);
	int (*pre_vpp_link_check_act)(void);
	int (*pre_vpp_link_sw)(bool on);
	u32 (*pre_vpp_get_ins_id)(void);
	bool (*config_crc_ic)(void);
	int (*s_bypass_ch)(int index, bool on);
};

#endif	/*__DI_LOCAL_H__*/
