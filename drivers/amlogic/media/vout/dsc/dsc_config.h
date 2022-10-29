/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DSC_CONFIG_H__
#define __DSC_CONFIG_H__
#include "dsc_drv.h"

#define OFFSET_FRACTIONAL_BITS  11
#define NUM_COMPONENTS			8
#define BPP_FRACTION 10000

#ifndef MAX
#define MAX(a, b) ({ \
			typeof(a) _a = a; \
			typeof(b) _b = b; \
			_a > _b ? _a : _b; \
		})
#endif // MAX

#ifndef MIN
#define MIN(a, b) ({ \
			typeof(a) _a = a; \
			typeof(b) _b = b; \
			_a < _b ? _a : _b; \
		})
#endif // MIN

#define MANUAL_COLOR_FORMAT				BIT(0)
#define MANUAL_PIC_W_H					BIT(1)
#define MANUAL_VIDEO_FPS				BIT(2)
#define MANUAL_SLICE_W_H				BIT(3)
#define MANUAL_BITS_PER_COMPONENT			BIT(4)
#define MANUAL_BITS_PER_PIXEL				BIT(5)
#define MANUAL_LINE_BUF_DEPTH				BIT(6)
#define MANUAL_PREDICTION_MODE				BIT(7)
#define MANUAL_SOME_RC_PARAMETER			BIT(8)
#define MANUAL_DSC_TMG_CTRL				BIT(9)
#define MANUAL_VPU_BIST_TMG_CTRL			BIT(10)
#define MANUAL_VPU_VIDEO_TMG_CTRL			BIT(11)

void dsc_drv_param_init(struct aml_dsc_drv_s *dsc_drv);
void calculate_dsc_data(struct aml_dsc_drv_s *dsc_drv);

#endif

