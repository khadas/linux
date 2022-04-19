/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_VRR_DRV_H__
#define __AML_VRR_DRV_H__
#include <linux/amlogic/media/vrr/vrr.h>

/* ver:20210806: initial version */
#define VRR_DRV_VERSION  "20210806"

#define VRRPR(fmt, args...)      pr_info("vrr: " fmt "", ## args)
#define VRRERR(fmt, args...)     pr_err("vrr error: " fmt "", ## args)

#define VRR_DBG_PR_NORMAL       BIT(0)
#define VRR_DBG_PR_ISR          BIT(1)

#define VRR_MAX_DRV    3

enum vrr_chip_e {
	VRR_CHIP_T7 = 0,
	VRR_CHIP_T3 = 1,
	VRR_CHIP_T5W = 2,
	VRR_CHIP_MAX,
};

struct aml_vrr_drv_s;
struct vrr_data_s {
	unsigned int chip_type;
	const char *chip_name;
	unsigned int drv_max;
	unsigned int offset[VRR_MAX_DRV];

	void (*sw_vspin)(struct aml_vrr_drv_s *vdrv);
};

#define VRR_STATE_EN          BIT(0)
#define VRR_STATE_LFC         BIT(1)
#define VRR_STATE_MODE_HW     BIT(4)
#define VRR_STATE_MODE_SW     BIT(5)
#define VRR_STATE_ENCL        BIT(8)
#define VRR_STATE_ENCP        BIT(9)

struct aml_vrr_drv_s {
	unsigned int index;
	unsigned int state;
	unsigned int enable;
	unsigned int line_dly;
	unsigned int sw_timer_cnt;
	unsigned int sw_timer_flag;
	unsigned int adj_vline_max;
	unsigned int adj_vline_min;
	unsigned int lfc_en;

	struct vrr_device_s *vrr_dev;
	struct cdev cdev;
	struct vrr_data_s *data;
	struct device *dev;
	struct timer_list sw_timer;
};

//===========================================================================
// For ENCL VRR
//===========================================================================
#define VRR_ENC_INDEX                              0

extern int lcd_venc_sel;
extern int lcd_vrr_timer_cnt;
struct aml_vrr_drv_s *vrr_drv_get(int index);
int vrr_drv_func_en(struct aml_vrr_drv_s *vdrv, int flag);
int vrr_drv_lfc_update(struct aml_vrr_drv_s *vdrv, int flag, int fps);

ssize_t vrr_active_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf);
int aml_vrr_if_probe(void);
int aml_vrr_if_remove(void);

#endif
