// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
		ext_ops.vpu_dev_clk_gate_on(de_devp->di_vpu_clk_gate_dev);
	else
		ext_ops.vpu_dev_clk_gate_off(de_devp->di_vpu_clk_gate_dev);
}

/*EXPORT_SYMBOL(dim_attach_ext_api);*/

/**********************************
 * ext_api used by DI
 ********************************/
#define ARY_TEMP2
#ifdef ARY_TEMP2
void dim_vpu_vmod_mem_pd_on_off(unsigned int mode, bool on)
{
	struct di_dev_s *de_devp = get_dim_de_devp();

	switch (mode) {
	case VPU_AFBC_DEC:
		if (on)
			vpu_dev_mem_power_on(de_devp->di_vpu_pd_dec);
		else
			vpu_dev_mem_power_down(de_devp->di_vpu_pd_dec);
		break;
	case VPU_AFBC_DEC1:
		if (on)
			vpu_dev_mem_power_on(de_devp->di_vpu_pd_dec1);
		else
			vpu_dev_mem_power_down(de_devp->di_vpu_pd_dec1);
		break;
	case VPU_VIU_VD1:
		if (on)
			vpu_dev_mem_power_on(de_devp->di_vpu_pd_vd1);
		else
			vpu_dev_mem_power_down(de_devp->di_vpu_pd_vd1);
		break;
	case VPU_DI_POST:
		if (on)
			vpu_dev_mem_power_on(de_devp->di_vpu_pd_post);
		else
			vpu_dev_mem_power_down(de_devp->di_vpu_pd_post);
		break;
	default:
		pr_info("%s:mode overlow:%d\n", __func__, mode);
		break;
	}
}

const struct ext_ops_s ext_ops = {
	.dim_vpu_mem_pd_vmod	= dim_vpu_vmod_mem_pd_on_off,
/*	.vf_get_receiver_name		= vf_get_receiver_name,*/
	.vpu_dev_clk_gate_on = vpu_dev_clk_gate_on,
	.vpu_dev_clk_gate_off = vpu_dev_clk_gate_off,
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
	.vf_get_receiver_name		= n_vf_get_receiver_name,
	.vpu_dev_clk_gate_on = vpu_dev_clk_gate_on;
	.vpu_dev_clk_gate_off = vpu_dev_clk_gate_off;
	.get_current_vscale_skip_count	= n_get_current_vscale_skip_count,
	.canvas_pool_alloc_canvas_table	= n_canvas_pool_alloc_canvas_table,
};

#endif

