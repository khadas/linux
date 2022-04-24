// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//needed for gdc/gdc configuration
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>

//data types and prototypes
#include "gdc_api.h"
#include "system_log.h"
#include "gdc_config.h"

#define GDC_AUTO_POWER_OFF_DELAY (200) /* ms */

/**
 *   Configure the output gdc configuration address/size
 *
 *   and buffer address/size; and resolution.
 *
 *   More than one gdc settings can be accessed by index to a gdc_config_t.
 *
 *   @param  gdc_cmd - overall gdc settings and state
 *   @param  gdc_config_num - selects the current gdc config to be applied
 *
 *   @return 0 - success
 *	 -1 - fail.
 */
int gdc_init(struct gdc_cmd_s *gdc_cmd, struct gdc_dma_cfg_t *dma_cfg,
	     u32 core_id)
{
	u32 dev_type = gdc_cmd->dev_type;

	gdc_cmd->is_waiting_gdc = 0;
	gdc_cmd->current_addr = gdc_cmd->buffer_addr;

	if (gdc_cmd->gdc_config.output_width == 0 ||
	    gdc_cmd->gdc_config.output_height == 0) {
		gdc_log(LOG_ERR, "Wrong GDC output resolution.\n");
		return -1;
	}
	//stop gdc
	gdc_start_flag_write(0, dev_type, core_id);

	// secure mem access
	gdc_secure_set(gdc_cmd->use_sec_mem, dev_type, core_id);

	//set the configuration address and size to the gdc block
	gdc_config_addr_write(&dma_cfg->config_cfg,
			      gdc_cmd->gdc_config.config_addr, dev_type,
			      core_id);
	gdc_config_size_write(gdc_cmd->gdc_config.config_size, dev_type,
			      core_id);

	//set the gdc input resolution
	gdc_datain_width_write(gdc_cmd->gdc_config.input_width, dev_type,
			       core_id);
	gdc_datain_height_write(gdc_cmd->gdc_config.input_height, dev_type,
				core_id);

	//set the gdc output resolution
	gdc_dataout_width_write(gdc_cmd->gdc_config.output_width, dev_type,
				core_id);
	gdc_dataout_height_write(gdc_cmd->gdc_config.output_height, dev_type,
				 core_id);

	return 0;
}

/**
 *   This function stops the gdc block
 *
 *   @param  gdc_cmd - overall gdc settings and state
 *
 */
void gdc_stop(struct gdc_cmd_s *gdc_cmd, u32 core_id)
{
	gdc_cmd->is_waiting_gdc = 0;
	gdc_start_flag_write(0, gdc_cmd->dev_type, core_id);
}

/**
 *   This function starts the gdc block
 *
 *   Writing 0->1 transition is necessary for trigger
 *
 *   @param  gdc_cmd - overall gdc settings and state
 *
 */
void gdc_start(struct gdc_cmd_s *gdc_cmd, u32 core_id)
{
	/* do a stop for sync */
	gdc_start_flag_write(0, gdc_cmd->dev_type, core_id);
	gdc_start_flag_write(1, gdc_cmd->dev_type, core_id);
	gdc_cmd->is_waiting_gdc = 1;
}

static void set_ext_8g_msb(struct gdc_dma_cfg_t *dma_cfg, u32 format_plane)
{
	u32 i, in_msb, out_msb, config_msb, curr;

	struct gdc_dmabuf_cfg_s *input_cfg = &dma_cfg->input_cfg[0];
	struct gdc_dmabuf_cfg_s *output_cfg = &dma_cfg->output_cfg[0];
	struct gdc_dmabuf_cfg_s *config_cfg = &dma_cfg->config_cfg;

	in_msb = input_cfg[0].paddr_8g_msb;
	out_msb = output_cfg[0].paddr_8g_msb;
	config_msb = config_cfg->paddr_8g_msb;

	/* check in/out/config MSB
	 * in/out MSB of every plane should be same as config MSB
	 */
	if (in_msb != out_msb || config_msb != out_msb) {
		gdc_log(LOG_ERR, "in_msb:%d out_msb:%d config_msb:%d, not same\n",
			in_msb, out_msb, config_msb);
		return;
	}
	for (i = 0; i < format_plane; i++) {
		if (input_cfg[i].paddr_8g_msb != in_msb ||
		    output_cfg[i].paddr_8g_msb != out_msb) {
			gdc_log(LOG_ERR, "Plane%d in_msb:%d out_msb:%d\n", i,
				input_cfg[i].paddr_8g_msb,
				output_cfg[i].paddr_8g_msb);
			return;
		}
	}

	/* write ext_8g_msb reg
	 * SYSCTRL_BUS_CFG1 bit[15:14] extadr_gdc
	 */
	curr = system_ext_8g_msb_read_32();
	curr &= ~(3 << 14);
	curr |= in_msb << 14;
	system_ext_8g_msb_write_32(curr);
}

/**
 *   This function points gdc to its input resolution
 *
 *   and yuv address and offsets
 *
 *   Shown inputs to GDC are Y and UV plane address and offsets
 *
 *   @param  gdc_cmd - overall gdc settings and state
 *   @param  active_width -  input width resolution
 *   @param  active_height - input height resolution
 *   @param  y_base_addr -  input Y base address
 *   @param  uv_base_addr - input UV base address
 *   @param  y_line_offset - input Y line buffer offset
 *   @param  uv_line_offset-  input UV line buffer offer
 *
 *   @return 0 - success
 *	 -1 - no interrupt from GDC.
 */
int gdc_process(struct gdc_cmd_s *gdc_cmd,
		u32 y_base_addr, u32 uv_base_addr,
		struct gdc_dma_cfg_t *dma_cfg, u32 core_id)
{
	u32 gdc_out_base_addr = gdc_cmd->current_addr;
	u32 output_height = gdc_cmd->gdc_config.output_height;
	u32 i_y_line_offset = gdc_cmd->gdc_config.input_y_stride;
	u32 i_uv_line_offset = gdc_cmd->gdc_config.input_c_stride;
	u32 o_y_line_offset = gdc_cmd->gdc_config.output_y_stride;
	u32 o_uv_line_offset = gdc_cmd->gdc_config.output_c_stride;
	u32 dev_type = gdc_cmd->dev_type;

	if (gdc_cmd->is_waiting_gdc) {
		gdc_start_flag_write(0, dev_type, core_id);
		gdc_log(LOG_CRIT, "No interrupt Still waiting...\n");
		gdc_start_flag_write(1, dev_type, core_id);

		return -1;
	}

	gdc_log(LOG_DEBUG, "starting GDC process.\n");

	if (GDC_DEV_T(dev_type)->bit_width_ext)
		gdc_bit_width_write(gdc_cmd->gdc_config.format, core_id);

	//input y plane
	gdc_data1in_addr_write(dma_cfg->input_cfg[0].paddr_8g_msb,
			       y_base_addr, dev_type, core_id);
	gdc_data1in_line_offset_write(i_y_line_offset, dev_type, core_id);

	//input uv plane
	gdc_data2in_addr_write(dma_cfg->input_cfg[1].paddr_8g_msb,
			       uv_base_addr, dev_type, core_id);
	gdc_data2in_line_offset_write(i_uv_line_offset, dev_type, core_id);

	//gdc y output
	gdc_data1out_addr_write(dma_cfg->output_cfg[0].paddr_8g_msb,
				gdc_out_base_addr, dev_type, core_id);
	gdc_data1out_line_offset_write(o_y_line_offset, dev_type, core_id);

	//gdc uv output
	if (gdc_cmd->outplane == 1)
		gdc_out_base_addr += output_height * o_y_line_offset;
	else
		gdc_out_base_addr = gdc_cmd->uv_out_base_addr;
	gdc_data2out_addr_write(dma_cfg->output_cfg[1].paddr_8g_msb,
				gdc_out_base_addr, dev_type, core_id);
	gdc_data2out_line_offset_write(o_uv_line_offset, dev_type, core_id);

	if (GDC_DEV_T(dev_type)->ext_msb_8g)
		set_ext_8g_msb(dma_cfg, 2);

	gdc_start(gdc_cmd, core_id);

	return 0;
}

/**
 *   This function points gdc to its input resolution
 *
 *   and yuv address and offsets
 *
 *   Shown inputs to GDC are Y and UV plane address and offsets
 *
 *   @param  gdc_cmd - overall gdc settings and state
 *   @param  active_width -  input width resolution
 *   @param  active_height - input height resolution
 *   @param  y_base_addr -  input Y base address
 *   @param  uv_base_addr - input UV base address
 *   @param  y_line_offset - input Y line buffer offset
 *   @param  uv_line_offset-  input UV line buffer offer
 *
 *   @return 0 - success
 *	 -1 - no interrupt from GDC.
 */
int gdc_process_yuv420p(struct gdc_cmd_s *gdc_cmd,
			u32 y_base_addr, u32 u_base_addr, u32 v_base_addr,
			struct gdc_dma_cfg_t *dma_cfg, u32 core_id)
{
	struct gdc_config_s  *gc = &gdc_cmd->gdc_config;
	u32 gdc_out_base_addr = gdc_cmd->current_addr;
	u32 input_stride = gc->input_y_stride;
	u32 input_u_stride = gc->input_c_stride;
	u32 input_v_stride = gc->input_c_stride;
	u32 dev_type = gdc_cmd->dev_type;

	gdc_log(LOG_DEBUG, "is_waiting_gdc=%d\n", gdc_cmd->is_waiting_gdc);
	if (gdc_cmd->is_waiting_gdc) {
		gdc_start_flag_write(0, dev_type, core_id);
		gdc_log(LOG_CRIT, "No interrupt Still waiting...\n");
		gdc_start_flag_write(1, dev_type, core_id);
		return -1;
	}

	gdc_log(LOG_DEBUG, "starting GDC process.\n");

	/* already set in gdc_init */
	/* u32 output_width = gc->output_width; */
	u32 output_height = gc->output_height;
	u32 output_stride = gc->output_y_stride;
	u32 output_u_stride = gc->output_c_stride;
	u32 output_v_stride = gc->output_c_stride;

	if (GDC_DEV_T(dev_type)->bit_width_ext)
		gdc_bit_width_write(gdc_cmd->gdc_config.format, core_id);

	//input y plane
	gdc_data1in_addr_write(dma_cfg->input_cfg[0].paddr_8g_msb,
			       y_base_addr, dev_type, core_id);
	gdc_data1in_line_offset_write(input_stride, dev_type, core_id);

	//input u plane
	gdc_data2in_addr_write(dma_cfg->input_cfg[1].paddr_8g_msb,
			       u_base_addr, dev_type, core_id);
	gdc_data2in_line_offset_write(input_u_stride, dev_type, core_id);

	//input v plane
	gdc_data3in_addr_write(dma_cfg->input_cfg[2].paddr_8g_msb,
			       v_base_addr, dev_type, core_id);
	gdc_data3in_line_offset_write(input_v_stride, dev_type, core_id);

	//gdc y output
	gdc_data1out_addr_write(dma_cfg->output_cfg[0].paddr_8g_msb,
				gdc_out_base_addr, dev_type, core_id);
	gdc_data1out_line_offset_write(output_stride, dev_type, core_id);

	//gdc u output
	if (gdc_cmd->outplane == 1)
		gdc_out_base_addr += output_height * output_stride;
	else
		gdc_out_base_addr = gdc_cmd->u_out_base_addr;
	gdc_data2out_addr_write(dma_cfg->output_cfg[1].paddr_8g_msb,
				gdc_out_base_addr, dev_type, core_id);
	gdc_data2out_line_offset_write(output_u_stride, dev_type, core_id);

	//gdc v output
	if (gdc_cmd->outplane == 1)
		gdc_out_base_addr += output_height * output_u_stride / 2;
	else
		gdc_out_base_addr = gdc_cmd->v_out_base_addr;
	gdc_data3out_addr_write(dma_cfg->output_cfg[2].paddr_8g_msb,
				gdc_out_base_addr, dev_type, core_id);
	gdc_data3out_line_offset_write(output_v_stride, dev_type, core_id);

	if (GDC_DEV_T(dev_type)->ext_msb_8g)
		set_ext_8g_msb(dma_cfg, 3);

	gdc_start(gdc_cmd, core_id);

	return 0;
}

/**
 *   This function points gdc to its input resolution
 *
 *   and yuv address and offsets
 *
 *   Shown inputs to GDC are Y plane address and offsets
 *
 *   @param  gdc_cmd - overall gdc settings and state
 *   @param  active_width -  input width resolution
 *   @param  active_height - input height resolution
 *   @param  y_base_addr -  input Y base address
 *   @param  y_line_offset - input Y line buffer offset
 *
 *   @return 0 - success
 *	 -1 - no interrupt from GDC.
 */
int gdc_process_y_grey(struct gdc_cmd_s *gdc_cmd,
		       u32 y_base_addr,
		       struct gdc_dma_cfg_t *dma_cfg, u32 core_id)
{
	struct gdc_config_s  *gc = &gdc_cmd->gdc_config;
	u32 gdc_out_base_addr = gdc_cmd->current_addr;
	u32 input_stride = gc->input_y_stride;
	u32 output_stride = gc->output_y_stride;
	u32 dev_type = gdc_cmd->dev_type;

	gdc_log(LOG_DEBUG, "is_waiting_gdc=%d\n", gdc_cmd->is_waiting_gdc);
	if (gdc_cmd->is_waiting_gdc) {
		gdc_start_flag_write(0, dev_type, core_id);
		gdc_log(LOG_CRIT, "No interrupt Still waiting...\n");
		gdc_start_flag_write(1, dev_type, core_id);
		return -1;
	}

	gdc_log(LOG_DEBUG, "starting GDC process.\n");

	if (GDC_DEV_T(dev_type)->bit_width_ext)
		gdc_bit_width_write(gdc_cmd->gdc_config.format, core_id);

	//input y plane
	gdc_data1in_addr_write(dma_cfg->input_cfg[0].paddr_8g_msb,
			       y_base_addr, dev_type, core_id);
	gdc_data1in_line_offset_write(input_stride, dev_type, core_id);

	//gdc y output
	gdc_data1out_addr_write(dma_cfg->output_cfg[0].paddr_8g_msb,
				gdc_out_base_addr, dev_type, core_id);
	gdc_data1out_line_offset_write(output_stride, dev_type, core_id);

	if (GDC_DEV_T(dev_type)->ext_msb_8g)
		set_ext_8g_msb(dma_cfg, 1);

	gdc_start(gdc_cmd, core_id);

	return 0;
}

/**
 *   This function points gdc to its input resolution
 *
 *   and yuv address and offsets
 *
 *   Shown inputs to GDC are Y and UV plane address and offsets
 *
 *   @param  gdc_cmd - overall gdc settings and state
 *   @param  active_width -  input width resolution
 *   @param  active_height - input height resolution
 *   @param  y_base_addr -  input Y base address
 *   @param  uv_base_addr - input UV base address
 *   @param  y_line_offset - input Y line buffer offset
 *   @param  uv_line_offset-  input UV line buffer offer
 *
 *   @return 0 - success
 *	 -1 - no interrupt from GDC.
 */
int gdc_process_yuv444p(struct gdc_cmd_s *gdc_cmd,
			u32 y_base_addr, u32 u_base_addr, u32 v_base_addr,
			struct gdc_dma_cfg_t *dma_cfg, u32 core_id)
{
	struct gdc_config_s  *gc = &gdc_cmd->gdc_config;
	u32 gdc_out_base_addr = gdc_cmd->current_addr;
	u32 input_stride = gc->input_y_stride;
	u32 input_u_stride = gc->input_c_stride;
	u32 input_v_stride = gc->input_c_stride;
	u32 output_height = gc->output_height;
	u32 output_stride = gc->output_y_stride;
	u32 output_u_stride = gc->output_c_stride;
	u32 output_v_stride = gc->output_c_stride;
	u32 dev_type = gdc_cmd->dev_type;

	gdc_log(LOG_DEBUG, "is_waiting_gdc=%d\n", gdc_cmd->is_waiting_gdc);
	if (gdc_cmd->is_waiting_gdc) {
		gdc_start_flag_write(0, dev_type, core_id);
		gdc_log(LOG_CRIT, "No interrupt Still waiting...\n");
		gdc_start_flag_write(1, dev_type, core_id);
		return -1;
	}

	gdc_log(LOG_DEBUG, "starting GDC process.\n");

	//input y plane
	gdc_data1in_addr_write(dma_cfg->input_cfg[0].paddr_8g_msb,
			       y_base_addr, dev_type, core_id);
	gdc_data1in_line_offset_write(input_stride, dev_type, core_id);

	//input u plane
	gdc_data2in_addr_write(dma_cfg->input_cfg[1].paddr_8g_msb,
			       u_base_addr, dev_type, core_id);
	gdc_data2in_line_offset_write(input_u_stride, dev_type, core_id);

	//input v plane
	gdc_data3in_addr_write(dma_cfg->input_cfg[2].paddr_8g_msb,
			       v_base_addr, dev_type, core_id);
	gdc_data3in_line_offset_write(input_v_stride, dev_type, core_id);

	//gdc y output
	gdc_data1out_addr_write(dma_cfg->output_cfg[0].paddr_8g_msb,
				gdc_out_base_addr, dev_type, core_id);
	gdc_data1out_line_offset_write(output_stride, dev_type, core_id);

	//gdc u output
	if (gdc_cmd->outplane == 1)
		gdc_out_base_addr += output_height * output_stride;
	else
		gdc_out_base_addr = gdc_cmd->u_out_base_addr;
	gdc_data2out_addr_write(dma_cfg->output_cfg[1].paddr_8g_msb,
				gdc_out_base_addr, dev_type, core_id);
	gdc_data2out_line_offset_write(output_u_stride, dev_type, core_id);

	//gdc v output
	if (gdc_cmd->outplane == 1)
		gdc_out_base_addr += output_height * output_u_stride;
	else
		gdc_out_base_addr = gdc_cmd->v_out_base_addr;
	gdc_data3out_addr_write(dma_cfg->output_cfg[2].paddr_8g_msb,
				gdc_out_base_addr, dev_type, core_id);
	gdc_data3out_line_offset_write(output_v_stride, dev_type, core_id);

	if (GDC_DEV_T(dev_type)->ext_msb_8g)
		set_ext_8g_msb(dma_cfg, 3);

	gdc_start(gdc_cmd, core_id);

	return 0;
}

/**
 *   This function points gdc to its input resolution
 *
 *   and rgb address and offsets
 *
 *   Shown inputs to GDC are R\G\B plane address and offsets
 *
 *   @param  gdc_cmd - overall gdc settings and state
 *   @param  active_width -  input width resolution
 *   @param  active_height - input height resolution
 *   @param  y_base_addr -  input R base address
 *   @param  u_base_addr - input G base address
 *   @param  v_base_addr - input B base address
 *   @param  y_line_offset - input R line buffer offset
 *   @param  u_line_offset-  input G line buffer offer
 *   @param  v_line_offset-  input B line buffer offer
 *
 *   @return 0 - success
 *	 -1 - no interrupt from GDC.
 */
int gdc_process_rgb444p(struct gdc_cmd_s *gdc_cmd,
			u32 y_base_addr, u32 u_base_addr, u32 v_base_addr,
			struct gdc_dma_cfg_t *dma_cfg, u32 core_id)
{
	struct gdc_config_s  *gc = &gdc_cmd->gdc_config;
	u32 gdc_out_base_addr = gdc_cmd->current_addr;
	u32 input_stride = gc->input_y_stride;
	u32 input_u_stride = gc->input_c_stride;
	u32 input_v_stride = gc->input_c_stride;
	u32 output_height = gc->output_height;
	u32 output_stride = gc->output_y_stride;
	u32 output_u_stride = gc->output_c_stride;
	u32 output_v_stride = gc->output_c_stride;
	u32 dev_type = gdc_cmd->dev_type;

	gdc_log(LOG_DEBUG, "is_waiting_gdc=%d\n", gdc_cmd->is_waiting_gdc);
	if (gdc_cmd->is_waiting_gdc) {
		gdc_start_flag_write(0, dev_type, core_id);
		gdc_log(LOG_CRIT, "No interrupt Still waiting...\n");
		gdc_start_flag_write(1, dev_type, core_id);
		return -1;
	}

	gdc_log(LOG_DEBUG, "starting GDC process.\n");

	//input y plane
	gdc_data1in_addr_write(dma_cfg->input_cfg[0].paddr_8g_msb,
			       y_base_addr, dev_type, core_id);
	gdc_data1in_line_offset_write(input_stride, dev_type, core_id);

	//input u plane
	gdc_data2in_addr_write(dma_cfg->input_cfg[1].paddr_8g_msb,
			       u_base_addr, dev_type, core_id);
	gdc_data2in_line_offset_write(input_u_stride, dev_type, core_id);

	//input v plane
	gdc_data3in_addr_write(dma_cfg->input_cfg[2].paddr_8g_msb,
			       v_base_addr, dev_type, core_id);
	gdc_data3in_line_offset_write(input_v_stride, dev_type, core_id);

	//gdc y output
	gdc_data1out_addr_write(dma_cfg->output_cfg[0].paddr_8g_msb,
				gdc_out_base_addr, dev_type, core_id);
	gdc_data1out_line_offset_write(output_stride, dev_type, core_id);

	//gdc u output
	if (gdc_cmd->outplane == 1)
		gdc_out_base_addr += output_height * output_stride;
	else
		gdc_out_base_addr = gdc_cmd->u_out_base_addr;
	gdc_data2out_addr_write(dma_cfg->output_cfg[1].paddr_8g_msb,
				gdc_out_base_addr, dev_type, core_id);
	gdc_data2out_line_offset_write(output_u_stride, dev_type, core_id);

	//gdc v output
	if (gdc_cmd->outplane == 1)
		gdc_out_base_addr += output_height * output_u_stride;
	else
		gdc_out_base_addr = gdc_cmd->v_out_base_addr;
	gdc_data3out_addr_write(dma_cfg->output_cfg[2].paddr_8g_msb,
				gdc_out_base_addr, dev_type, core_id);
	gdc_data3out_line_offset_write(output_v_stride, dev_type, core_id);

	if (GDC_DEV_T(dev_type)->ext_msb_8g)
		set_ext_8g_msb(dma_cfg, 3);

	gdc_start(gdc_cmd, core_id);

	return 0;
}

int gdc_pwr_init(struct device *dev, struct gdc_pd *pd, u32 dev_type)
{
	int i, err = 0;

	if (!of_property_read_bool(dev->of_node, "power-domains"))
		return -EINVAL;

	for (i = 0; i < GDC_DEV_T(dev_type)->core_cnt; i++) {
		if (GDC_DEV_T(dev_type)->core_cnt == 1) {
			pd[i].dev = dev;
			pm_runtime_enable(dev);
		} else {
			pd[i].dev = dev_pm_domain_attach_by_id(dev, i);
			if (IS_ERR_OR_NULL(pd[i].dev)) {
				err = PTR_ERR(pd[i].dev);
				gdc_log(LOG_ERR,
					"dev_pm_domain_attach_by_id %d fail\n",
					i);
			}
		}
		pm_runtime_set_autosuspend_delay(pd[i].dev,
						 GDC_AUTO_POWER_OFF_DELAY);
		pm_runtime_use_autosuspend(pd[i].dev);
	}
	return err;
}

void gdc_pwr_remove(struct gdc_pd *pd)
{
	int i;

	for (i = 0; i < CORE_NUM; i++) {
		if (!IS_ERR_OR_NULL(pd[i].dev)) {
			pm_runtime_disable(pd[i].dev);
			dev_pm_domain_detach(pd[i].dev, true);
		}
	}
}

/**
 *   This function set the GDC power on/off
 *
 *   @param enable - power off/on
 *   @return  0 - success
 *           -1 - fail.
 */
int gdc_pwr_config(bool enable, u32 dev_type, u32 core_id)
{
	struct meson_gdc_dev_t *gdc_dev = NULL;
	struct device *pd_dev;
	int ret = -1;
	int clk_type = 0;

	gdc_dev = GDC_DEV_T(dev_type);

	if (!gdc_dev ||
	    !gdc_dev->clk_core[core_id] ||
	    !gdc_dev->pdev) {
		gdc_log(LOG_ERR, "core clk set err or pdev is null.\n");
		return -1;
	}

	clk_type = gdc_dev->clk_type;

	if (clk_type == CORE_AXI && !gdc_dev->clk_axi[core_id]) {
		gdc_log(LOG_ERR, "axi clk set err.\n");
		return -1;
	}

	pd_dev = gdc_dev->pd[core_id].dev;
	/* power */
	if (enable) {
		ret = pm_runtime_get_sync(pd_dev);
		if (ret < 0)
			gdc_log(LOG_ERR, "runtime get power error\n");
	} else {
		pm_runtime_mark_last_busy(pd_dev);
		pm_runtime_put_autosuspend(pd_dev);
	}

	/* clk */
	if (enable) {
		if (clk_type == CORE_AXI) {
			clk_prepare_enable(gdc_dev->clk_core[core_id]);
			clk_prepare_enable(gdc_dev->clk_axi[core_id]);
		} else if (clk_type == MUXGATE_MUXSEL_GATE ||
			   clk_type == GATE) {
			clk_prepare_enable(gdc_dev->clk_gate[core_id]);
		}
	} else {
		if (clk_type == CORE_AXI) {
			clk_disable_unprepare(gdc_dev->clk_core[core_id]);
			clk_disable_unprepare(gdc_dev->clk_axi[core_id]);
		} else if (clk_type == MUXGATE_MUXSEL_GATE ||
			   clk_type == GATE) {
			clk_disable_unprepare(gdc_dev->clk_gate[core_id]);
		}
	}

	gdc_dev->pd[core_id].status = enable;

	return 0;
}
