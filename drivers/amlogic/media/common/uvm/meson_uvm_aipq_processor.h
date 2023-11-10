/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/meson_uvm_core.h>

enum aipq_get_info_type_e {
	AIPQ_GET_INVALID = 0,
	AIPQ_GET_224_DATA = 1,
	AIPQ_GET_INDEX_INFO = 2,
	AIPQ_GET_BASIC_INFO = 3,
};

/*hwc attach aipq info*/
struct uvm_aipq_info {
	s32 shared_fd;
	s32 aipq_fd;
	struct nn_value_t nn_value[AI_PQ_TOP];
	s32 aipq_buf_index;
	s32 aipq_value_index;
	s32 get_info_type;
	s32 need_do_aipq;
	s32 repeat_frame;
	s32 dw_width;
	s32 dw_height;
	s32 nn_input_frame_width;
	s32 nn_input_frame_height;
	s32 is_nn_doing;
	s32 omx_index;
	s32 is_start_first_vf;
	s32 is_open_first_vf;
	s32 nn_do_aipq_type;
	s32 reserved[5];
};

struct ge2d_output_t {
	int width;
	int height;
	u32 format;
	phys_addr_t addr;
};

int attach_aipq_hook_mod_info(int shared_fd,
		char *buf, struct uvm_hook_mod_info *mod_info);
int aipq_setinfo(void *arg, char *buf);
int aipq_getinfo(void *arg, char *buf);

