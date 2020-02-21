// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "system_log.h"

#include <linux/interrupt.h>
#include <linux/kernel.h>
//gdc api functions
#include "gdc_api.h"

int gdc_run(struct gdc_cmd_s *g)
{
	gdc_stop(g);

	gdc_log(LOG_DEBUG, "Done gdc load..\n");

	//initialise the gdc by the first configuration
	if (gdc_init(g) != 0) {
		gdc_log(LOG_ERR, "Failed to initialise GDC block");
		return -1;
	}

	gdc_log(LOG_DEBUG, "Done gdc config..\n");

	switch (g->gdc_config.format) {
	case NV12:
		gdc_process(g, g->y_base_addr, g->uv_base_addr);
	break;
	case YV12:
		gdc_process_yuv420p(g, g->y_base_addr,
				    g->u_base_addr,
				    g->v_base_addr);
	break;
	case Y_GREY:
		gdc_process_y_grey(g, g->y_base_addr);
	break;
	case YUV444_P:
		gdc_process_yuv444p(g, g->y_base_addr,
				    g->u_base_addr,
				    g->v_base_addr);
	break;
	case RGB444_P:
		gdc_process_rgb444p(g, g->y_base_addr,
				    g->u_base_addr,
				g->v_base_addr);
	break;
	default:
		gdc_log(LOG_ERR, "Error config format\n");
	break;
	}
	gdc_log(LOG_DEBUG, "call gdc process\n");

	return 0;
}
