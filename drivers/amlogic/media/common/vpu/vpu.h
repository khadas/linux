/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPU_PARA_H__
#define __VPU_PARA_H__
#include <linux/clk.h>
#include <linux/clk-provider.h>

#define VPUPR(fmt, args ...)     pr_info("vpu: " fmt, ## args)
#define VPUERR(fmt, args ...)    pr_err("vpu: error: " fmt, ## args)

enum vpu_chip_e {
	VPU_CHIP_G12A = 0,
	VPU_CHIP_G12B,
	VPU_CHIP_TL1,
	VPU_CHIP_SM1,
	VPU_CHIP_TM2,
	VPU_CHIP_MAX,
};

struct fclk_div_s {
	unsigned int fclk_id;
	unsigned int fclk_mux;
	unsigned int fclk_div;
};

struct vpu_clk_s {
	unsigned int freq;
	unsigned int mux;
	unsigned int div;
};

struct vpu_ctrl_s {
	unsigned int vmod;
	unsigned int reg;
	unsigned int val;
	unsigned int bit;
	unsigned int len;
};

struct vpu_reset_s {
	unsigned int reg;
	unsigned int mask;
};

#define VPU_RESET_CNT_MAX       10
#define VPU_HDMI_ISO_CNT_MAX    5
#define VPU_MOD_INIT_CNT_MAX    20
#define VPU_MEM_PD_CNT_MAX      150
#define VPU_CLK_GATE_CNT_MAX    150

struct vpu_data_s {
	enum vpu_chip_e chip_type;
	const char *chip_name;
	unsigned char clk_level_dft;
	unsigned char clk_level_max;
	struct fclk_div_s *fclk_div_table;

	unsigned char gp_pll_valid;
	unsigned int mem_pd_reg0;
	unsigned int mem_pd_reg1;
	unsigned int mem_pd_reg2;
	unsigned int mem_pd_reg3;
	unsigned int mem_pd_reg4;

	struct vpu_reset_s *reset_table;
	struct vpu_ctrl_s *hdmi_iso_pre_table;
	struct vpu_ctrl_s *hdmi_iso_table;
	struct vpu_ctrl_s *module_init_table;
	struct vpu_ctrl_s *mem_pd_table;
	struct vpu_ctrl_s *clk_gate_table;
};

struct vpu_conf_s {
	struct vpu_data_s *data;
	unsigned int clk_level;

	/* clktree */
	struct clk *gp_pll;
	struct clk *vpu_clk0;
	struct clk *vpu_clk1;
	struct clk *vpu_clk;

	unsigned int *clk_vmod;
};

/* VPU memory power down */
#define VPU_MEM_POWER_ON		0
#define VPU_MEM_POWER_DOWN		1

#define VPU_CLK_GATE_ON			1
#define VPU_CLK_GATE_OFF		0

/* ************************************************ */
extern struct vpu_conf_s vpu_conf;

extern int vpu_debug_print_flag;

int vpu_chip_valid_check(void);
void vpu_ctrl_probe(void);

void vpu_mem_pd_init_off(void);
void vpu_module_init_config(void);
void vpu_power_on(void);
void vpu_power_off(void);

#endif
