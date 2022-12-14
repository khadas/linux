/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_VRR_DRV_H__
#define __AML_VRR_DRV_H__
#include <linux/amlogic/media/vrr/vrr.h>

/* ver:20210806: initial version */
/* ver:20220718: basic function for freesync */
/* ver:20220816: support fps policy */
/* ver:20220927: support multi drv for t7 */
#define VRR_DRV_VERSION  "20220927"

#define VRRPR(fmt, args...)      pr_info("vrr: " fmt "", ## args)
#define VRRERR(fmt, args...)     pr_err("vrr error: " fmt "", ## args)

#define VRR_DBG_PR_NORMAL       BIT(0)
#define VRR_DBG_PR_ADV          BIT(1)
#define VRR_DBG_PR_ISR          BIT(3)
#define VRR_DBG_TEST            BIT(4)

#define VRR_MAX_DRV             3

#define VRR_TRACE_SIZE          4088
struct vrr_trace_s {
	unsigned int flag;
	unsigned int size;
	char *buf;
	char *pcur;
};

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
#define VRR_STATE_POLICY      BIT(2)
#define VRR_STATE_MODE_HW     BIT(4)
#define VRR_STATE_MODE_SW     BIT(5)
#define VRR_STATE_ENCL        BIT(8)
#define VRR_STATE_ENCP        BIT(9)
#define VRR_STATE_SWITCH_OFF  BIT(10)
#define VRR_STATE_RESET       BIT(11)
#define VRR_STATE_CLR_MASK    0xffff
//static state
#define VRR_STATE_VS_IRQ      BIT(16)
#define VRR_STATE_VS_IRQ_EN   BIT(17)
#define VRR_STATE_ACTIVE      BIT(20)
//for debug
#define VRR_STATE_TRACE       BIT(24)

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
	unsigned int lfc_shift;
	unsigned int policy;

	struct vrr_device_s *vrr_dev;
	struct cdev cdev;
	struct vrr_data_s *data;
	struct device *dev;
	struct timer_list sw_timer;

	unsigned int vsync_irq;
	spinlock_t vrr_isr_lock; /* vsync lock */
	char vs_isr_name[16];
};

//===========================================================================
// For ENCL VRR
//===========================================================================
#define VRR_ENC_INDEX                              0

extern unsigned int vrr_debug_print;

void vrr_drv_trace(struct aml_vrr_drv_s *vdrv, char *str);
struct aml_vrr_drv_s *vrr_drv_get(int index);
int vrr_drv_func_en(struct aml_vrr_drv_s *vdrv, int flag);
int vrr_drv_lfc_update(struct aml_vrr_drv_s *vdrv, int flag, int fps);

ssize_t vrr_active_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf);
int aml_vrr_if_probe(void);
int aml_vrr_if_remove(void);

#endif
