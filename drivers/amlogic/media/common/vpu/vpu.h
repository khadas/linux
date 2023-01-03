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
	VPU_CHIP_AXG = 0,
	VPU_CHIP_G12A,
	VPU_CHIP_G12B,
	VPU_CHIP_TL1,
	VPU_CHIP_SM1,
	VPU_CHIP_TM2,
	VPU_CHIP_TM2B,
	VPU_CHIP_SC2,
	VPU_CHIP_T5,
	VPU_CHIP_T5D,
	VPU_CHIP_T7,
	VPU_CHIP_S4,
	VPU_CHIP_S4D,
	VPU_CHIP_T3,
	VPU_CHIP_T5W,
	VPU_CHIP_S5,
	VPU_CHIP_MAX,
};

#define CLK_FPLL_FREQ          2000 /* MHz */
#define VPU_CLK_TOLERANCE      1000000 /* Hz */

#define VPU_HDMI_ISO_CNT_MAX    5
#define VPU_RESET_CNT_MAX       10
#define VPU_MOD_INIT_CNT_MAX    20
#define VPU_MEM_PD_CNT_MAX      150
#define VPU_CLK_GATE_CNT_MAX    150

#define VPU_PWR_ID_END          0xffff
#define VPU_PWR_ID_MAX          10

#define PM_VPU_HDMI_SC2         5
#define PM_VPU_HDMI_T7          6
#define PM_VI_CLK1_T7           13
#define PM_VI_CLK2_T7           14

#define PM_VPU_HDMI_T3          4
#define PM_VI_CLK1_T3           10
#define PM_VI_CLK2_T3           11
#define PM_NOC_VPU_T3           21

enum vpu_mux_e {
	FCLK_DIV4 = 0,
	FCLK_DIV3,
	FCLK_DIV5,
	FCLK_DIV7,
	MPLL_CLK1,
	VID_PLL_CLK,
	VID2_PLL_CLK,
	GPLL_CLK,
	FCLK_DIV_MAX,
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

#define VPU_MEM_PD_REG_CNT    5
struct vpu_data_s {
	enum vpu_chip_e chip_type;
	const char *chip_name;
	unsigned char clk_level_dft;
	unsigned char clk_level_max;
	struct fclk_div_s *fclk_div_table;
	struct vpu_clk_s *clk_table;
	unsigned int *reg_map_table;
	unsigned int *test_reg_table;

	unsigned int vpu_clk_reg;
	unsigned int vapb_clk_reg;

	unsigned char gp_pll_valid;
	unsigned int mem_pd_reg[VPU_MEM_PD_REG_CNT];
	unsigned int mem_pd_reg_flag;

	unsigned int *pwrctrl_id_table;

	struct vpu_ctrl_s *power_table;
	struct vpu_ctrl_s *iso_table;
	struct vpu_reset_s *reset_table;
	struct vpu_ctrl_s *module_init_table;
	struct vpu_ctrl_s *mem_pd_table;
	struct vpu_ctrl_s *clk_gate_table;

	void (*power_on)(void);
	void (*power_off)(void);
	void (*mem_pd_init_off)(void);
	void (*module_init_config)(void);
	int (*power_init_check)(void);
	int (*mempd_switch)(unsigned int vmod, int flag);
	int (*mempd_get)(unsigned int vmod);
	int (*clk_apply)(unsigned int vclk);
	void (*clktree_init)(struct device *dev);
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
extern int vpu_reg_table[];
extern int vpu_reg_table_new[];

int vpu_chip_valid_check(void);

unsigned int get_vpu_clk_level_max_vmod(void);
int vpu_clk_apply_dft(unsigned int clk_level);
int set_vpu_clk(unsigned int vclk);
void vpu_clktree_init_dft(struct device *dev);

void vpu_mem_pd_init_off(void);
void vpu_module_init_config(void);
int vpu_power_init_check_dft(void);
void vpu_power_on(void);
void vpu_power_off(void);
void vpu_power_on_new(void);
void vpu_power_off_new(void);

#endif
