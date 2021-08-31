// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "system_log.h"

#include <linux/interrupt.h>
#include <linux/kernel.h>
//gdc api functions
#include "gdc_api.h"

int gdc_run(struct gdc_cmd_s *g, struct gdc_dma_cfg_t *dma_cfg, u32 core_id)
{
	gdc_stop(g, core_id);

	gdc_log(LOG_DEBUG, "Done gdc%d load..\n", core_id);

	//initialise the gdc by the first configuration
	if (gdc_init(g, dma_cfg, core_id) != 0) {
		gdc_log(LOG_ERR, "Failed to initialise GDC block");
		return -1;
	}

	gdc_log(LOG_DEBUG, "Done gdc%d config..\n", core_id);

	switch (g->gdc_config.format & FORMAT_TYPE_MASK) {
	case NV12:
		gdc_process(g, g->y_base_addr, g->uv_base_addr, dma_cfg,
			    core_id);
	break;
	case YV12:
		gdc_process_yuv420p(g, g->y_base_addr,
				    g->u_base_addr,
				    g->v_base_addr, dma_cfg, core_id);
	break;
	case Y_GREY:
		gdc_process_y_grey(g, g->y_base_addr, dma_cfg, core_id);
	break;
	case YUV444_P:
		gdc_process_yuv444p(g, g->y_base_addr,
				    g->u_base_addr,
				    g->v_base_addr, dma_cfg, core_id);
	break;
	case RGB444_P:
		gdc_process_rgb444p(g, g->y_base_addr,
				    g->u_base_addr,
				g->v_base_addr, dma_cfg, core_id);
	break;
	default:
		gdc_log(LOG_ERR, "Error config format\n");
	break;
	}
	gdc_log(LOG_DEBUG, "call gdc%d process\n", core_id);

	return 0;
}
