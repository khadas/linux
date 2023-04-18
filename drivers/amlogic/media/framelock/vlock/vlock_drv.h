/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VLOCK_DRV_H__
#define __VLOCK_DRV_H__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/init.h>
#include <linux/of_device.h>

//#include "vlock.h"

#define VLOCK_DEVNO       1
#define VLOCK_NAME        "aml_vlock"
#define VLOCK_CLS_NAME    "aml_vlock"

#define FLAG_VLOCK_DISABLE         BIT(1)
#define FLAG_VLOCK_ENABLE          BIT(0)

struct vlock_dev_s {
	struct device	*dev;
	struct cdev     vlock_cdev;
	dev_t			devno;
	struct class	*cls;
	struct platform_device *pdev;
	struct match_data_s *pm_data;

	struct clk *vlock_clk;
};

enum vlock_chip_type_e {
	vlk_chip_id_other = 0,
	vlk_chip_id_t3,
	vlk_chip_id_t5w,
	vlk_chip_id_s5
};

enum vlock_chip_cls_e {
	VLK_OTHER_CLS = 0,
	VLK_TV_CHIP,
	VLK_STB_CHIP,
	VLK_SMT_CHIP,
	VLK_AD_CHIP
};

enum vlock_vlk_chiptype_e {
	vlk_chip_txl,
	vlk_chip_txlx,
	vlk_chip_txhd,
	vlk_chip_tl1,
	vlk_chip_tm2,
	vlk_chip_sm1,
	vlk_chip_t5,	//same as t5d/t5w
	vlk_chip_t7,
	vlk_chip_t3,
};

enum vlk_hw_ver_e {
	/*gxtvbb*/
	vlk_hw_org = 0,
	/*
	 *txl
	 *txlx
	 */
	vlk_hw_ver1,
	/* tl1 later
	 * fix bug:i problem
	 * fix bug:affect ss function
	 * add: phase lock
	 * tm2: have separate pll:tcon pll and hdmitx pll
	 */
	vlk_hw_ver2,
	/* tm2 version B
	 * fix some bug
	 */
	vlk_hw_tm2verb,
};

enum vlock_chip_pll_sel_e {
	vlock_chip_pll_sel_tcon = 0,
	vlock_chip_pll_sel_hdmi,
	vlock_chip_pll_sel_disable = 0xf,
};

struct vlock_match_data_s {
	enum vlock_chip_type_e		chip_id;
	enum vlock_chip_cls_e		chip_cls;
	enum vlock_vlk_chiptype_e	vlk_chip;
	enum vlk_hw_ver_e			vlk_hw_ver;
	/*independent panel pll and hdmitx pll*/
	enum vlock_chip_pll_sel_e	vlk_chip_pll_sel;
	u32		vlk_support;
	u32		vlk_new_fsm;
	u32		vlk_phlock_en;
	/*independent panel pll and hdmitx pll*/
	u32		vlk_pll_sel;
	u32		reg_addr_vlock;
	u32		reg_addr_hiu;
	u32		reg_addr_anactr;
	/*control frc flash patch*/
	u32		vlk_ctl_for_frc;
	/*frame lock control type 1:support 0:unsupport*/
	u32		vrr_support_flag;
};

#endif

