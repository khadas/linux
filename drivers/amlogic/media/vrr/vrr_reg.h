/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _VRR_REG_H_
#define _VRR_REG_H_
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif

#define VENP_VRR_CTRL                              0x1be8
//Bit   31    cfg_vsp_din      // W/O, pulse  gpu pulse input(sw cfg)
//Bit   30    cfg_vrr_clr      // W/O, pulse
//Bit 31:28   ro_vrr_vsp_cnt   // R/O, input frm cnt per time
//Bit 27:24   ro_vrr_max_err   // R/O, input err pixel cnt
//Bit 23:8    cfg_vsp_dly_num  // R/W, unsigned, default 0
//Bit  7:4    cfg_vrr_frm_ths  // R/W, unsigned, default 0
//Bit  3:2    cfg_vrr_vsp_en   // R/W, unsigned, default 0
//Bit    1    cfg_vrr_mode     // R/W, unsigned, default 0   0:normal   1:vrr
//Bit    0    cfg_vrr_vsp_sel  // R/W, unsigned, default 0   1:hdmi in  0:gpu in
#define VENP_VRR_ADJ_LMT                           0x1be9
//Bit 31:16  cfg_vrr_min_vnum  //R/W, unsigned,
//Bit 15:0   cfg_vrr_max_vnum  //R/W, unsigned,

#define VENP_VRR_CTRL1                             0x1bea
//Bit 31:31  cfg_vsp_cnt_rst   //W/O, unsigned,
//Bit 30:4   reverse           //R/W, unsigned,
//Bit 3:0    cfg_vsp_rst_num   //R/W, unsigned,

#define VENC_VRR_CTRL                              0x1cd8
//Bit   31    cfg_vsp_din      // W/O, pulse  gpu pulse input(sw cfg)
//Bit   30    cfg_vrr_clr      // W/O, pulse
//Bit 31:28   ro_vrr_vsp_cnt   // R/O, input frm cnt per time
//Bit 27:24   ro_vrr_max_err   // R/O, input err pixel cnt
//Bit 23:8    cfg_vsp_dly_num  // R/W, unsigned, default 0
//Bit  7:4    cfg_vrr_frm_ths  // R/W, unsigned, default 0
//Bit  3:2    cfg_vrr_vsp_en   // R/W, unsigned, default 0
//Bit    1    cfg_vrr_mode     // R/W, unsigned, default 0   0:normal   1:vrr
//Bit    0    cfg_vrr_vsp_sel  // R/W, unsigned, default 0   1:hdmi in  0:gpu in
#define VENC_VRR_ADJ_LMT                           0x1cd9
//Bit 31:16  cfg_vrr_min_vnum  //R/W, unsigned,
//Bit 15:0   cfg_vrr_max_vnum  //R/W, unsigned,

#define VENC_VRR_CTRL1                             0x1cda
//Bit 31:31  cfg_vsp_cnt_rst   //W/O, unsigned,
//Bit 30:4   reverse           //R/W, unsigned,
//Bit 3:0    cfg_vsp_rst_num   //R/W, unsigned,

#define VPU_VENCP_STAT                             0x1cec
#define ENCL_SYNC_CTRL                             0x1cde

#define ENCL_VIDEO_MAX_LNCNT                       0x1cbb
#define ENCP_VIDEO_MAX_LNCNT                       0x1bae

static inline unsigned int vrr_reg_read(unsigned int _reg)
{
	unsigned int ret = 0;

#ifdef CONFIG_AMLOGIC_VPU
	ret = vpu_vcbus_read(_reg);
#else
	ret = aml_read_vcbus(_reg);
#endif

	return ret;
};

static inline void vrr_reg_write(unsigned int _reg, unsigned int _value)
{
#ifdef CONFIG_AMLOGIC_VPU
	vpu_vcbus_write(_reg, _value);
#else
	aml_write_vcbus(_reg, _value);
#endif
};

static inline void vrr_reg_setb(unsigned int _reg, unsigned int _value,
				unsigned int _start, unsigned int _len)
{
	vrr_reg_write(_reg, ((vrr_reg_read(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}

static inline unsigned int vrr_reg_getb(unsigned int reg,
					unsigned int _start, unsigned int _len)
{
	return (vrr_reg_read(reg) >> _start) & ((1L << _len) - 1);
}

#endif
