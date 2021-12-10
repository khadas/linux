// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include "meson_vpu_reg.h"
#include "meson_vpu_util.h"
#include "meson_vpu.h"
#include "meson_vpu_global_mif.h"

static void osd_set_vpp_path_default(u32 osd_index, u32 vpp_index)
{
	/* osd_index is vpp mux input */
	/* default setting osd2 route to vpp0 vsync */
	if (osd_index == 3)
		meson_vpu_write_reg_bits(PATH_START_SEL,
				vpp_index, 24, 2);
	/* default setting osd3 route to vpp0 vsync */
	if (osd_index == 4)
		meson_vpu_write_reg_bits(PATH_START_SEL,
				vpp_index, 28, 2);
}

void fix_vpu_clk2_default_regs(void)
{
	/* default: osd byp osd_blend */
	meson_vpu_write_reg_bits(VPP_OSD1_SCALE_CTRL, 0x2, 0, 3);
	meson_vpu_write_reg_bits(VPP_OSD2_SCALE_CTRL, 0x3, 0, 3);
	meson_vpu_write_reg_bits(VPP_OSD3_SCALE_CTRL, 0x3, 0, 3);
	meson_vpu_write_reg_bits(VPP_OSD4_SCALE_CTRL, 0x3, 0, 3);

	/* default: osd byp dolby */
	meson_vpu_write_reg_bits(VPP_VD1_DSC_CTRL, 0x1, 4, 1);
	meson_vpu_write_reg_bits(VPP_VD2_DSC_CTRL, 0x1, 4, 1);
	meson_vpu_write_reg_bits(VPP_VD3_DSC_CTRL, 0x1, 4, 1);
	meson_vpu_write_reg_bits(MALI_AFBCD_TOP_CTRL, 0x1, 14, 1);
	meson_vpu_write_reg_bits(MALI_AFBCD_TOP_CTRL, 0x1, 19, 1);
	meson_vpu_write_reg_bits(MALI_AFBCD1_TOP_CTRL, 0x1, 19, 1);
	meson_vpu_write_reg_bits(MALI_AFBCD1_TOP_CTRL, 0x1, 19, 1);

	/* default: osd 12bit path */
	meson_vpu_write_reg_bits(VPP_VD1_DSC_CTRL, 0x0, 5, 1);
	meson_vpu_write_reg_bits(VPP_VD2_DSC_CTRL, 0x0, 5, 1);
	meson_vpu_write_reg_bits(VPP_VD3_DSC_CTRL, 0x0, 5, 1);
	meson_vpu_write_reg_bits(MALI_AFBCD_TOP_CTRL, 0x0, 15, 1);
	meson_vpu_write_reg_bits(MALI_AFBCD_TOP_CTRL, 0x0, 20, 1);
	meson_vpu_write_reg_bits(MALI_AFBCD1_TOP_CTRL, 0x0, 20, 1);
	meson_vpu_write_reg_bits(MALI_AFBCD1_TOP_CTRL, 0x0, 20, 1);

	/* OSD3  uses VPP0*/
	osd_set_vpp_path_default(3, 0);
	/* OSD4  uses VPP0*/
	osd_set_vpp_path_default(4, 0);
}

void independ_path_default_regs(void)
{
	/* default: osd1_bld_din_sel -- do not osd_data_byp osd_blend */
	meson_vpu_write_reg_bits(VIU_OSD1_PATH_CTRL, 0x0, 4, 1);
	meson_vpu_write_reg_bits(VIU_OSD2_PATH_CTRL, 0x0, 4, 1);
	meson_vpu_write_reg_bits(VIU_OSD3_PATH_CTRL, 0x0, 4, 1);

	/* default: osd1_sc_path_sel -- before osd_blend or after hdr */
	meson_vpu_write_reg_bits(VIU_OSD1_PATH_CTRL, 0x0, 0, 1);
	meson_vpu_write_reg_bits(VIU_OSD2_PATH_CTRL, 0x1, 0, 1);
	meson_vpu_write_reg_bits(VIU_OSD3_PATH_CTRL, 0x1, 0, 1);

	/* default: osd byp dolby */
	meson_vpu_write_reg_bits(VIU_VD1_PATH_CTRL, 0x1, 16, 1);
	meson_vpu_write_reg_bits(VIU_VD2_PATH_CTRL, 0x1, 16, 1);
	meson_vpu_write_reg_bits(VIU_OSD1_PATH_CTRL, 0x1, 16, 1);
	meson_vpu_write_reg_bits(VIU_OSD2_PATH_CTRL, 0x1, 16, 1);
	meson_vpu_write_reg_bits(VIU_OSD3_PATH_CTRL, 0x1, 16, 1);

	/* default: osd 12bit path */
	meson_vpu_write_reg_bits(VIU_VD1_PATH_CTRL, 0x0, 17, 1);
	meson_vpu_write_reg_bits(VIU_VD2_PATH_CTRL, 0x0, 17, 1);
	meson_vpu_write_reg_bits(VIU_OSD1_PATH_CTRL, 0x0, 17, 1);
	meson_vpu_write_reg_bits(VIU_OSD2_PATH_CTRL, 0x0, 17, 1);
	meson_vpu_write_reg_bits(VIU_OSD3_PATH_CTRL, 0x0, 17, 1);

	/* OSD3  uses VPP0*/
	osd_set_vpp_path_default(3, 0);
	/* OSD4  uses VPP0*/
	osd_set_vpp_path_default(4, 0);
}
