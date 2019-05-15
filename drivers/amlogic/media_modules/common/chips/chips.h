/*
 * drivers/amlogic/media/common/arch/chips/chips.h
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

#ifndef UCODE_MANAGER_HEADER
#define UCODE_MANAGER_HEADER
#include "../media_clock/clk/clk_priv.h"

struct chip_vdec_info_s {

	int cpu_type;

	struct video_firmware_s *firmware;

	struct chip_vdec_clk_s *clk_mgr[VDEC_MAX];

	struct clk_set_setting *clk_setting_array;
};

const char *get_cpu_type_name(void);
const char *get_video_format_name(enum vformat_e type);

struct chip_vdec_info_s *get_current_vdec_chip(void);

bool check_efuse_chip(int vformat);

#endif
