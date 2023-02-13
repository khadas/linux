/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/meson_uvm_core.h>

enum aicolor_get_info_type_e {
	AICOLOR_GET_INVALID = 0,
	AICOLOR_GET_RGB_DATA = 1,
};

/*hwc attach aicolor info*/
struct uvm_aicolor_info {
	s32 shared_fd;
	s32 aicolor_fd;
	u8 color_value[AI_COLOR_COUNT];
	s32 aicolor_buf_index;
	s32 aicolor_value_index;
	s32 get_info_type;
	s32 need_do_aicolor;
	s32 repeat_frame;
	s32 dw_width;
	s32 dw_height;
	s32 nn_input_frame_width;
	s32 nn_input_frame_height;
	s32 nn_status;
	s32 omx_index;
};

int attach_aicolor_hook_mod_info(int shared_fd,
		char *buf, struct uvm_hook_mod_info *mod_info);
int aicolor_setinfo(void *arg, char *buf);
int aicolor_getinfo(void *arg, char *buf);
