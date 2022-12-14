/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_OSD_PROC_H_
#define _MESON_OSD_PROC_H_

#define OSD1_PROC_IN_SIZE              0x6061
#define OSD2_PROC_IN_SIZE              0x6062
#define OSD3_PROC_IN_SIZE              0x6063
#define OSD4_PROC_IN_SIZE              0x6064
#define OSD1_PROC_OUT_SIZE             0x6065
#define OSD2_PROC_OUT_SIZE             0x6066
#define OSD3_PROC_OUT_SIZE             0x6067
#define OSD4_PROC_OUT_SIZE             0x6068
#define OSD_BLEND_DOUT0_SIZE           0x6069
#define OSD_BLEND_DOUT1_SIZE           0x606a
#define VPP_PROBE_CTRL                 0x606b
#define VPP_PROBE_POS                  0x606c
#define VPP_HL_COLOR                   0x606d
#define OSD_PROC_1MUX3_SEL             0x6072
#define OSD_2SLICE2PPC_IN_SIZE         0x6073
#define OSD_2SLICE2PPC_MODE            0x6074
#define OSD_2SLICE2PPC_GC_CTRL         0x6075
#define OSD_PI_BYPASS_EN               0x6076
#define OSD_DOLBY_BYPASS_EN            0x6077
#define OSD_SYS_5MUX4_SEL              0x6078
#define OSD_SYS_HWIN0_CUT              0x6079
#define OSD_SYS_HWIN1_CUT              0x607a
#define OSD_SYS_PAD_CTRL               0x607b
#define OSD_SYS_PAD_DUMMY_DATA0        0x607c
#define OSD_SYS_PAD_DUMMY_DATA1        0x607d
#define OSD_SYS_PAD_H_SIZE             0x607e
#define OSD_SYS_PAD_V_SIZE             0x607f
#define OSD_SYS_2SLICE_HWIN_CUT        0x6080

#define VIU_OSD1_MISC                  0x1a15
#define VIU_OSD2_MISC                  0x1a16
#define VIU_OSD3_MISC                  0x1a17
#define VIU_OSD4_MISC                  0x1a18

struct slice2ppc_reg_s {
	u32 osd1_proc_in_size;
	u32 osd2_proc_in_size;
	u32 osd3_proc_in_size;
	u32 osd4_proc_in_size;
	u32 osd1_proc_out_size;
	u32 osd2_proc_out_size;
	u32 osd3_proc_out_size;
	u32 osd4_proc_out_size;
	u32 osd_blend_dout0_size;
	u32 osd_blend_dout1_size;
	u32 osd_proc_1mux3_sel;
	u32 osd_2slice2ppc_in_size;
	u32 osd_2slice2ppc_mode;
	u32 osd_pi_bypass_en;
	u32 osd_sys_5mux4_sel;
	u32 osd_sys_hwin0_cut;
	u32 osd_sys_hwin1_cut;
	u32 osd_sys_pad_ctrl;
	u32 osd_sys_pad_dummy_data0;
	u32 osd_sys_pad_dummy_data1;
	u32 osd_sys_pad_h_size;
	u32 osd_sys_pad_v_size;
	u32 osd_sys_2slice_hwin_cut;
};

#endif
