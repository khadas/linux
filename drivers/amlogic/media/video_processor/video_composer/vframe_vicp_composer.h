/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef VFRAME_VICP_COMPOSER_H
#define VFRAME_VICP_COMPOSER_H
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vicp/vicp.h>
#include "vc_util.h"

bool is_vicp_supported(void);
enum vicp_rotation_mode_e map_rotationmode_from_vc_to_vicp(int rotation_vc);
int config_vicp_input_data(struct vframe_s *vf, ulong addr, int stride, int width, int height,
	int endian, int color_fmt, int color_depth, struct input_data_param_t *input_data);
int config_vicp_output_data(int fbc_out_en, int mif_out_en, ulong *phy_addr, int stride,
	int width, int height, int endian, enum vicp_color_format_e cfmt_mif, int cdep_mif,
	enum vicp_color_format_e cfmt_fbc, int cdep_fbc, int init_ctrl, int pip_mode,
	struct output_data_param_t *output_data);
int vicp_data_composer(struct vicp_data_config_t *data_config);
#endif
