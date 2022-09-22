// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
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
#include "deinterlace.h"
#include "di_data_l.h"
#include "di_prc.h"

#include "di_api.h"
/**********************************
 * DI api is used for other module
 *********************************/
static const struct di_ext_ops di_ext = {
	.di_post_reg_rd             = NULL, //l_DI_POST_REG_RD,
	.di_post_wr_reg_bits        = NULL, //l_DI_POST_WR_REG_BITS,
	.post_update_mc		    = NULL,
	.post_keep_cmd_release2		= dim_post_keep_cmd_release2_local,//NULL, //
	.polic_cfg			= dim_polic_cfg_local,
	.s_bypass_ch		= di_ls_bypass_ch,
	.new_create_instance	= new_create_instance,
	.new_destroy_instance	= new_destroy_instance,
	.new_empty_input_buffer	= new_empty_input_buffer,
	.new_fill_output_buffer	= new_fill_output_buffer,
	.new_release_keep_buf	= new_release_keep_buf,
	.new_get_output_buffer_num	= new_get_output_buffer_num,
	.new_get_input_buffer_num	= new_get_input_buffer_num,
	.config_crc_ic		= dim_config_crc_icl,
	.pre_vpp_link_display	= dim_pre_vpp_link_display,
	.pre_vpp_link_check_vf	= dpvpp_check_vf,
	.pre_vpp_link_check_act = dpvpp_check_di_act,
	.pre_vpp_link_sw	= dpvpp_sw,
	.pre_vpp_get_ins_id	= dpvpp_get_ins_id
};

void dim_attach_to_local(void)
{
	dil_attach_ext_api(&di_ext);
}

bool dim_attach_ext_api(struct di_ext_ops *di_api)
{
	#ifdef MARK_HIS
	di_api = &di_ext;
	#else
	if (!di_api)
		return false;

	memcpy(di_api, &di_ext, sizeof(struct di_ext_ops));
	#endif
	return true;
}

void diext_clk_b_sw(bool on)
{
	struct di_dev_s *de_devp = get_dim_de_devp();

	if (on)
		vpu_dev_clk_gate_on(de_devp->dim_vpu_clk_gate_dev);
	else
		vpu_dev_clk_gate_off(de_devp->dim_vpu_clk_gate_dev);
}

/*EXPORT_SYMBOL(dim_attach_ext_api);*/

#ifdef MARK_SC2
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
#endif

void sc2wr(unsigned int adr, unsigned int val)
{
}

unsigned int sc2rd(unsigned int adr)
{
	return 0;
}

unsigned int sc2wr_reg_bits(unsigned int adr, unsigned int val,
			    unsigned int start, unsigned int len)
{
	return 0;
}

unsigned int sc2brd(unsigned int adr, unsigned int start,
		    unsigned int len)
{
	return 0;
}

const struct reg_acc sc2reg = {
	.wr = sc2wr,
	.rd	= sc2rd,
	.bwr	= sc2wr_reg_bits,
	.brd	= sc2brd,
};

