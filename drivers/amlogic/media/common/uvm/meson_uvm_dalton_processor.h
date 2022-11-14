/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/meson_uvm_core.h>

enum dalton_get_info_type_e {
	DATTON_GET_INVALID = 0,
	DATTON_GET_RGB_DATA = 1,
};

/*hwc attach dalton info*/
struct uvm_dalton_info {
	s32 shared_fd;
	s32 dalton_fd;
	s32 dalton_buf_index;
	s32 dalton_value_index;
	s32 get_info_type;
	s32 need_do_dalton;
	s32 repeat_frame;
	s32 dalton_frame_width;
	s32 dalton_frame_height;
	s32 nn_status;
	s32 omx_index;
};

int attach_dalton_hook_mod_info(int shared_fd,
		char *buf, struct uvm_hook_mod_info *mod_info);
int dalton_setinfo(void *arg, char *buf);
int dalton_getinfo(void *arg, char *buf);
