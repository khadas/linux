/*
 * drivers/amlogic/display/vout2/nulldisp.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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


/* Linux Headers */
#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/delay.h>

/* Amlogic Headers */
#include <linux/amlogic/vout/vout_notify.h>

/* Local Headers */
#include <vout/vpp.h>
#include <vout/vout_reg.h>
#include <vout/vout_io.h>
#include <vout/vout_log.h>

#define DisableVideoLayer() \
	vout_vcbus_clr_mask(VPP2_MISC, \
		(VPP_VD1_PREBLEND|VPP_VD2_PREBLEND \
		|VPP_VD2_POSTBLEND|VPP_VD1_POSTBLEND))

static const struct vinfo_s nulldisp_info = {
	.name              = "null",
	.mode              = VMODE_MAX + 1,
	.width             = 1920,
	.height            = 1080,
	.field_height      = 1080,
	.aspect_ratio_num  = 16,
	.aspect_ratio_den  = 9,
	.sync_duration_num = 50,
	.sync_duration_den = 1,
};

static const struct vinfo_s *nulldisp_get_current_info(void)
{
	return &nulldisp_info;
}

static int nulldisp_set_current_vmode(enum vmode_e mode)
{
	return 0;
}

static const struct vinfo_s *get_valid_vinfo(char *mode)
{
	if (strncmp(nulldisp_info.name, mode, strlen(nulldisp_info.name)) == 0)
		return &nulldisp_info;
	return NULL;
}

static enum vmode_e nulldisp_validate_vmode(char *mode)
{
	const struct vinfo_s *info = get_valid_vinfo(mode);
#ifndef CONFIG_AM_VIDEO2
	int viu1_select = vout_vcbus_read(VPU_VIU_VENC_MUX_CTRL) & 0x3;
#endif
	DisableVideoLayer();
#ifndef CONFIG_AM_VIDEO2
	/*
	 * viu2_select should be different from viu1_select
	 * to fix viu1 video smooth problem)
	 */
	vout_vcbus_set_bits(VPU_VIU_VENC_MUX_CTRL,
			    (viu1_select + 1) & 0x3, 2, 2);
#endif
	if (info)
		return info->mode;
	return VMODE_MAX;
}

static int nulldisp_vmode_is_supported(enum vmode_e mode)
{
	if (nulldisp_info.mode == mode)
		return true;
	return false;
}

static int nulldisp_module_disable(enum vmode_e cur_vmod)
{
	return 0;
}

static struct vout_server_s nulldisp_vout_server = {
	.name = "nulldisp_vout_server",
	.op = {
		.get_vinfo          = nulldisp_get_current_info,
		.set_vmode          = nulldisp_set_current_vmode,
		.validate_vmode     = nulldisp_validate_vmode,
		.vmode_is_supported = nulldisp_vmode_is_supported,
		.disable            = nulldisp_module_disable,
	},
};

static int __init nulldisp_init(void)
{
	vout2_register_server(&nulldisp_vout_server);
	return 0;
}

static void __exit nulldisp_exit(void)
{
	vout2_unregister_server(&nulldisp_vout_server);
}

/* module_init(nulldisp_init); */
subsys_initcall(nulldisp_init);
module_exit(nulldisp_exit);

MODULE_DESCRIPTION("AMLOGIC NullDisp");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tim Yao <timyao@amlogic.com>");
