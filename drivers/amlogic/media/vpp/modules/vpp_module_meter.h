/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_MODULE_METER_H__
#define __VPP_MODULE_METER_H__

#define DNLP_HIST_CNT (32)
#define HIST_GM_BIN_CNT (DNLP_HIST_CNT << 1)
#define HIST_HUE_GM_BIN_CNT (32)
#define HIST_SAT_GM_BIN_CNT (32)

struct vpp_hist_report_s {
	unsigned int hist_pow;
	unsigned int luma_sum;
	unsigned int chroma_sum;
	unsigned int pixel_sum;
	unsigned int height;
	unsigned int width;
	unsigned int luma_max;
	unsigned int luma_min;
	unsigned short gamma[HIST_GM_BIN_CNT];
	unsigned short dark_gamma[HIST_GM_BIN_CNT];
	unsigned int hue_gamma[HIST_HUE_GM_BIN_CNT];
	unsigned int sat_gamma[HIST_SAT_GM_BIN_CNT];
};

int vpp_module_meter_init(struct vpp_dev_s *pdev);
int vpp_module_meter_hist_en(bool enable);
void vpp_module_meter_fetch_hist_report(void);
struct vpp_hist_report_s *vpp_module_meter_get_hist_report(void);
bool vpp_module_meter_get_dark_hist_support(void);

void vpp_module_meter_dump_info(enum vpp_dump_module_info_e info_type);

#endif

