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

#include "amlogic_fbc_hook.h"
static AMLOGIC_FBC_vframe_decoder_fun_t g_decoder_fun;
static AMLOGIC_FBC_vframe_encoder_fun_t g_encoder_fun;


int AMLOGIC_FBC_vframe_decoder(
	void *dstyuv[4],
	struct vframe_s *vf,
	int out_format,
	int flags)

{
	if (g_decoder_fun) {
		return g_decoder_fun(dstyuv,
			vf,
			out_format,
			flags);
	}
	printk("no AMLOGIC_FBC_vframe_decoder ERRR!!\n");
	return -1;
}
EXPORT_SYMBOL(AMLOGIC_FBC_vframe_decoder);

int AMLOGIC_FBC_vframe_encoder(
	void *srcyuv[4],
	void *dst_header,
	void *dst_body,
	int in_format,
	int flags)

{
	if (g_encoder_fun) {
		return g_encoder_fun(
				srcyuv,
				dst_header,
				dst_body,
				in_format,
				flags);
	}
	printk("no AMLOGIC_FBC_vframe_encoder ERRR!!\n");
	return -1;
}
EXPORT_SYMBOL(AMLOGIC_FBC_vframe_encoder);

int register_amlogic_afbc_dec_fun(AMLOGIC_FBC_vframe_decoder_fun_t fn)
{
	if (g_decoder_fun) {
		pr_err("error!!,AMLOGIC_FBC dec have register\n");
		return -1;
	}
	printk("register_amlogic_afbc_dec_fun\n");
	g_decoder_fun = fn;
	return 0;
}
EXPORT_SYMBOL(register_amlogic_afbc_dec_fun);

int register_amlogic_afbc_enc_fun(AMLOGIC_FBC_vframe_encoder_fun_t fn)
{
	if (g_encoder_fun) {
		pr_err("error!!,AMLOGIC_FBC enc have register\n");
		return -1;
	}
	g_encoder_fun = fn;
	return 0;
}
EXPORT_SYMBOL(register_amlogic_afbc_enc_fun);

int unregister_amlogic_afbc_dec_fun(void)
{
	g_decoder_fun = NULL;
	pr_err("unregister_amlogic_afbc_dec_fun\n");
	return 0;
}
EXPORT_SYMBOL(unregister_amlogic_afbc_dec_fun);

int unregister_amlogic_afbc_enc_fun(void)
{
	g_encoder_fun = NULL;
	pr_err("unregister_amlogic_afbc_dec_fun\n");
	return 0;
}
EXPORT_SYMBOL(unregister_amlogic_afbc_enc_fun);


