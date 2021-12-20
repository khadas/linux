/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/meson_uvm_core.h>

enum get_info_type_e {
	GET_INVALID = 0,
	GET_HF_INFO = 1,
	GET_VINFO_SIZE = 2,
};

/*hwc attach nn info*/
struct uvm_ai_sr_info {
	s32 shared_fd;
	s32 nn_out_fd;
	s32 fence_fd;
	s64 nn_out_phy_addr;
	s32 nn_out_width;
	s32 nn_out_height;
	s64 hf_phy_addr;
	s32 hf_width;
	s32 hf_height;
	s32 hf_align_w;
	s32 hf_align_h;
	s32 buf_align_w;
	s32 buf_align_h;
	s32 nn_status;
	s32 nn_index;
	s32 nn_mode;
	s32 get_info_type;
	s32 src_interlace_flag;
	s32 vinfo_width;
	s32 vinfo_height;
	s32 need_do_aisr;
};

struct meson_nn_mod_ops {
	const struct nn_private_data *(*nn_mod_setinfo)(void);
	const struct nn_private_data *(*nn_mod_getinfo)(void);
	/* TODO */
};

struct nn_private_data {
	/* TODO */
};

int attach_nn_hook_mod_info(int shared_fd,
		char *buf, struct uvm_hook_mod_info *mod_info);
int nn_mod_setinfo(void *arg, char *buf);
int nn_mod_getinfo(void *arg, char *buf);

