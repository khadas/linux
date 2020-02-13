// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/frame_provider/decoder/utils/amlogic_fbc_hook.c
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>

#include <linux/amlogic/media/vfm/amlogic_fbc_hook_v1.h>

static AMLOGIC_FBC_vframe_decoder_fun_t g_decoder_fun;
static AMLOGIC_FBC_vframe_encoder_fun_t g_encoder_fun;

int AMLOGIC_FBC_vframe_decoder_v1(void *dstyuv[4],
				  struct vframe_s *vf,
				  int out_format, int flags)
{
	if (g_decoder_fun) {
		return g_decoder_fun(dstyuv,
			vf,
			out_format,
			flags);
	}
	pr_err("no AMLOGIC_FBC_vframe_decoder ERRR!!\n");
	return -1;
}
EXPORT_SYMBOL(AMLOGIC_FBC_vframe_decoder_v1);

int AMLOGIC_FBC_vframe_encoder_v1(void *srcyuv[4],
				  void *dst_header,
				  void *dst_body,
				  int in_format,
				  int flags)
{
	if (g_encoder_fun)
		return g_encoder_fun(srcyuv, dst_header,
				     dst_body, in_format, flags);
	pr_err("no AMLOGIC_FBC_vframe_encoder ERRR!!\n");
	return -1;
}
EXPORT_SYMBOL(AMLOGIC_FBC_vframe_encoder_v1);

int register_amlogic_afbc_dec_fun_v1(AMLOGIC_FBC_vframe_decoder_fun_t fn)
{
	if (g_decoder_fun) {
		pr_err("error!!,AMLOGIC_FBC dec have register\n");
		return -1;
	}
	pr_err("register_amlogic_afbc_dec_fun\n");
	g_decoder_fun = fn;
	return 0;
}
EXPORT_SYMBOL(register_amlogic_afbc_dec_fun_v1);

int register_amlogic_afbc_enc_fun_v1(AMLOGIC_FBC_vframe_encoder_fun_t fn)
{
	if (g_encoder_fun) {
		pr_err("error!!,AMLOGIC_FBC enc have register\n");
		return -1;
	}
	g_encoder_fun = fn;
	return 0;
}
EXPORT_SYMBOL(register_amlogic_afbc_enc_fun_v1);

int unregister_amlogic_afbc_dec_fun_v1(void)
{
	g_decoder_fun = NULL;
	pr_err("unregister_amlogic_afbc_dec_fun\n");
	return 0;
}
EXPORT_SYMBOL(unregister_amlogic_afbc_dec_fun_v1);

int unregister_amlogic_afbc_enc_fun_v1(void)
{
	g_encoder_fun = NULL;
	pr_err("unregister_amlogic_afbc_dec_fun\n");
	return 0;
}
EXPORT_SYMBOL(unregister_amlogic_afbc_enc_fun_v1);

