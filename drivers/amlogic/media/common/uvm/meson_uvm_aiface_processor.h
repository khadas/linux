/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/meson_uvm_core.h>

enum aiface_get_info_type_e {
	AIFACE_GET_INVALID = 0,
	AIFACE_GET_RGB_DATA = 1,
};

/*hwc attach aiface info*/
struct uvm_aiface_info {
	s32 shared_fd;
	s32 aiface_fd;
	u64 buf_phy_addr;
	struct face_value_t face_value[MAX_FACE_COUNT_PER_INPUT];
	s32 aiface_buf_index;
	s32 aiface_value_index;
	s32 get_info_type;
	s32 need_do_aiface;
	s32 repeat_frame;
	s32 dw_width;
	s32 dw_height;
	s32 nn_input_frame_width;
	s32 nn_input_frame_height;
	s32 nn_input_frame_format;
	s32 nn_status;
	s32 omx_index;
	void *dma_buf_addr;
};

int attach_aiface_hook_mod_info(int shared_fd,
		char *buf, struct uvm_hook_mod_info *mod_info);
int aiface_setinfo(void *arg, char *buf);
int aiface_getinfo(void *arg, char *buf);
