/*
 * drivers/amlogic/media/di_multi/di_api.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/amlogic/media/vpu/vpu.h>

#include "di_api.h"
/**********************************
 * DI api is used for other module
 *********************************/
static const struct di_ext_ops di_ext = {
	.di_post_reg_rd             = l_DI_POST_REG_RD,
	.di_post_wr_reg_bits        = l_DI_POST_WR_REG_BITS,
	.post_update_mc		    = NULL,
};

void dim_attach_to_local(void)
{
	dil_attach_ext_api(&di_ext);
}

bool dim_attach_ext_api(struct di_ext_ops *di_api)
{
	#if 1
	if (!di_api)
		return false;

	memcpy(di_api, &di_ext, sizeof(struct di_ext_ops));
	#else
	di_api = &di_ext;
	#endif
	return true;
}

/*EXPORT_SYMBOL(dim_attach_ext_api);*/

/**********************************
 * ext_api used by DI
 ********************************/
#define ARY_TEMP2
#ifdef ARY_TEMP2
void ext_switch_vpu_mem_pd_vmod(unsigned int vmod, bool on)
{
	switch_vpu_mem_pd_vmod(vmod,
			       on ? VPU_MEM_POWER_ON : VPU_MEM_POWER_DOWN);
}

const struct ext_ops_s ext_ops = {
	.switch_vpu_mem_pd_vmod		= ext_switch_vpu_mem_pd_vmod,
	/*no use ?*/
/*	.vf_get_receiver_name		= vf_get_receiver_name,*/
	.switch_vpu_clk_gate_vmod	= switch_vpu_clk_gate_vmod,
	.get_current_vscale_skip_count	= get_current_vscale_skip_count,
	.canvas_pool_alloc_canvas_table = canvas_pool_alloc_canvas_table,
};

#else
void n_switch_vpu_mem_pd_vmod(unsigned int vmod, bool on)
{
}

char *n_vf_get_receiver_name(const char *provider_name)
{
	return "";
}

void n_switch_vpu_clk_gate_vmod(unsigned int vmod, int flag)
{
}

int n_get_current_vscale_skip_count(struct vframe_s *vf)
{
	return 0;
}

u32 n_canvas_pool_alloc_canvas_table(const char *owner, u32 *tab,
				     int size,
				     enum canvas_map_type_e type)
{
	return 0;
}

const struct ext_ops_s ext_ops = {
	.switch_vpu_mem_pd_vmod		= n_switch_vpu_mem_pd_vmod,
	.vf_get_receiver_name		= n_vf_get_receiver_name,
	.switch_vpu_clk_gate_vmod	= n_switch_vpu_clk_gate_vmod,
	.get_current_vscale_skip_count	= n_get_current_vscale_skip_count,
	.canvas_pool_alloc_canvas_table	= n_canvas_pool_alloc_canvas_table,
};

#endif

