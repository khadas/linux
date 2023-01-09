/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_DRV_H__
#define __VPP_DRV_H__

#define VPP_DEVNO       1
#define VPP_NAME        "aml_vpp"
#define VPP_CLS_NAME    "aml_vpp"

enum vpp_chip_type_e {
	CHIP_TXHD = 1,
	CHIP_G12A,
	CHIP_G12B,
	CHIP_SM1,
	CHIP_TL1,
	CHIP_TM2,
	CHIP_SC2,
	CHIP_T5,
	CHIP_T5D,
	CHIP_T7,
	CHIP_S4,
	CHIP_T3,
	CHIP_T5W,
	CHIP_T5M,
	CHIP_S4D,
	CHIP_MAX,
};

enum vout_e {
	OUT_NULL = 0,
	OUT_PANEL,
	OUT_HDMI
};

struct vpp_out_s {
	enum vout_e vpp0_out;
	enum vout_e vpp1_out;
	enum vout_e vpp2_out;
};

enum dnlp_hw_e {
	SR0_DNLP = 0,
	SR1_DNLP,
	VPP_DNLP
};

enum gamma_hw_e {
	GM_V1 = 0, /*old gamma hw, can not rdma rw*/
	GM_V2, /*new gamma hw, after T7*/
};

enum sr_hw_e {
	SR0_ONLY = 0,
	SR1_ONLY,
	SR0_SR1,
};

enum bit_dp_e {
	BDP_10 = 0,
	BDP_12
};

struct vlk_data_s {
	//enum vlk_chiptype vlk_chip;
	unsigned int vlk_support;
	unsigned int vlk_new_fsm;
	//enum vlock_hw_ver_e vlk_hwver;
	unsigned int vlk_phlock_en;
	unsigned int vlk_pll_sel;/*independent panel pll and hdmitx pll*/
	unsigned int reg_addr_vlock;
	unsigned int reg_addr_hiu;
	unsigned int reg_addr_anactr;
};

struct match_data_s {
	enum vpp_chip_type_e chip_id;
};

struct vpp_cfg_data_s {
	enum dnlp_hw_e dnlp_hw;
	enum sr_hw_e sr_hw;
	enum bit_dp_e bit_depth;
	enum gamma_hw_e gamma_hw;
	struct vpp_out_s vpp_out;
	struct vlk_data_s vlk_data;
};

struct vpp_ops_s {
	int opt_num;
	//const struct hw_ops_s hw_ops;
};

struct vpp_dev_s {
	struct device *dev;
	struct cdev vpp_cdev;
	dev_t devno;
	struct class *clsp;
	struct platform_device *pdev;

	unsigned int probe_ok;
	unsigned int pq_en;
	unsigned int dbg_flag;
	unsigned int pr_lvl;

	void *fw_data;
	struct match_data_s *pm_data;
	struct vpp_cfg_data_s vpp_cfg_data;

	struct vpp_ops_s vpp_ops;

	unsigned int virq_cnt;
	struct resource *res_lc_irq; /*lc curve irq*/
	unsigned int lcirq_cnt;
	int lcisr_defined;

	spinlock_t isr_lock;/*isr lock*/
	struct mutex lut3d_lock;

	struct workqueue_struct *cabc_queue;
};

struct vpp_dev_s *get_vpp_drv(void);
#endif

