/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef VPU_SECURE_H_
#define VPU_SECURE_H_

enum vpp_top_e {
	VPP_TOP,
	VPP_TOP_1,
	VPP_TOP_2,
	VPP_TOP_MAX
};

enum secure_module_e {
	OSD_MODULE,
	VIDEO_MODULE,
	DI_MODULE,
	VDIN_MODULE,
	MODULE_NUM
};

enum module_port_e {
	VD1_OUT,
	VD2_OUT,
	VD3_OUT,
	OSD1_VPP_OUT,
	OSD2_VPP_OUT,
	POST_BLEND_OUT,
	MAX_SECURE_OUT
};

/* v4 extern secure bits */
#define VD1_SLICE3_SECURE       BIT(12)
#define VD1_SLICE2_SECURE       BIT(11)
#define VD1_SLICE1_SECURE       BIT(10)

/* v2 extend secure bits */
#define VPP2_OUTPUT_SECURE      BIT(12)
#define VPP1_OUTPUT_SECURE      BIT(11)
#define VD3_INPUT_SECURE        BIT(10)
#define OSD4_INPUT_SECURE       BIT(9)

/* v1 secure bits */
#define VPP_OUTPUT_SECURE       BIT(8)
#define MALI_AFBCD_SECURE       BIT(7)
#define DV_INPUT_SECURE         BIT(6)
#define AFBCD_INPUT_SECURE      BIT(5)
#define OSD3_INPUT_SECURE       BIT(4)
#define VD2_INPUT_SECURE        BIT(3)
#define VD1_INPUT_SECURE        BIT(2)
#define OSD2_INPUT_SECURE       BIT(1)
#define OSD1_INPUT_SECURE       BIT(0)

struct vd_secure_info_s {
	enum module_port_e secure_type;
	u32 secure_enable;
};

int secure_register(enum secure_module_e module,
		    int config_delay,
		    void **reg_op,
		    void *cb);
int secure_unregister(enum secure_module_e module);
int secure_config(enum secure_module_e module, int secure_src,
		  u32 secure_config);
bool get_secure_state(enum module_port_e port);
#endif
