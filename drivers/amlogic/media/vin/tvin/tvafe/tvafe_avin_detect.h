/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef TVAFE_AVIN_DETECT_H_
#define TVAFE_AVIN_DETECT_H_

#include <linux/cdev.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>

#define ANACTRL_CVBS_DETECT_CNTL 0x9f
#define HHI_CVBS_DETECT_CNTL	0x2e
#define AFE_DETECT_RSV3_BIT		31
#define AFE_DETECT_RSV3_WIDTH	1
#define AFE_DETECT_RSV2_BIT		30
#define AFE_DETECT_RSV2_WIDTH	1
#define AFE_DETECT_RSV1_BIT		29
#define AFE_DETECT_RSV1_WIDTH		1
#define AFE_DETECT_RSV0_BIT		28
#define AFE_DETECT_RSV0_WIDTH		1
#define AFE_CH2_SYNC_HYS_ADJ_BIT	13
#define AFE_CH2_SYNC_HYS_ADJ_WIDTH	2
#define AFE_CH2_SYNC_LEVEL_ADJ_BIT	10
#define AFE_CH2_SYNC_LEVEL_ADJ_WIDTH	3
#define AFE_CH2_EN_SYNC_TIP_BIT		9
#define AFE_CH2_EN_SYNC_TIP_WIDTH	1
#define AFE_CH2_EN_DETECT_BIT		8
#define AFE_CH2_EN_DETECT_WIDTH		1
#define AFE_CH1_SYNC_HYS_ADJ_BIT	5
#define AFE_CH1_SYNC_HYS_ADJ_WIDTH	2
#define AFE_CH1_SYNC_LEVEL_ADJ_BIT	2
#define AFE_CH1_SYNC_LEVEL_ADJ_WIDTH	3
#define AFE_CH1_EN_SYNC_TIP_BIT		1
#define AFE_CH1_EN_SYNC_TIP_WIDTH	1
#define AFE_CH1_EN_DETECT_BIT		0
#define AFE_CH1_EN_DETECT_WIDTH		1

#define AFE_DETECT_RSV_BIT	28
#define AFE_DETECT_RSV_WIDTH	4
#define AFE_CH2_DC_LEVEL_ADJ_BIT	22
#define AFE_CH2_DC_LEVEL_ADJ_WIDTH	3
#define AFE_CH2_COMP_LEVEL_ADJ_BIT	19
#define AFE_CH2_COMP_LEVEL_ADJ_WIDTH	3
#define AFE_CH2_EN_DC_BIAS_BIT		18
#define AFE_CH2_EN_DC_BIAS_WIDTH	1
#define AFE_CH2_DETECT_MODE_SELECT_BIT	17
#define AFE_CH2_DETECT_MODE_SELECT_WIDTH	1
#define AFE_CH2_COMP_HYS_ADJ_BIT	16
#define AFE_CH2_COMP_HYS_ADJ_WIDTH	1
#define AFE_TL_CH2_EN_DETECT_BIT	15
#define AFE_TL_CH2_EN_DETECT_WIDTH	1
#define AFE_CH1_DC_LEVEL_ADJ_BIT	7
#define AFE_CH1_DC_LEVEL_ADJ_WIDTH	3
#define AFE_CH1_COMP_LEVEL_ADJ_BIT	4
#define AFE_CH1_COMP_LEVEL_ADJ_WIDTH	3
#define AFE_CH1_EN_DC_BIAS_BIT			3
#define AFE_CH1_EN_DC_BIAS_WIDTH		1
#define AFE_CH1_DETECT_MODE_SELECT_BIT	2
#define AFE_CH1_DETECT_MODE_SELECT_WIDTH	1
#define AFE_CH1_COMP_HYS_ADJ_BIT		1
#define AFE_CH1_COMP_HYS_ADJ_WIDTH		1

#define AFE_T5_CH2_EN_DC_BIAS_BIT	11
#define AFE_T5_CH2_EN_DC_BIAS_WIDTH	1
#define AFE_T5_AFE_PGA_MANMODE_GAIN_BIT	16
#define AFE_T5_AFE_PGA_MANMODE_GAIN_WIDTH 8
#define AFE_T5_AFE_MAN_MODE_BIT		24
#define AFE_T5_AFE_MAN_MODE_WIDTH	1

#define CVBS_IRQ0_CNTL			0x3c24
#define CVBS_IRQ_MODE_BIT		6
#define CVBS_IRQ_MODE_WIDTH		3
#define CVBS_IRQ_MODE_MASK		7
#define CVBS_IRQ_TRIGGER_SEL_BIT	5
#define CVBS_IRQ_TRIGGER_SEL_WIDTH	1
#define CVBS_IRQ_TRIGGER_SEL_MASK	1
#define CVBS_IRQ_EDGE_EN_BIT		4
#define CVBS_IRQ_EDGE_EN_WIDTH		1
#define CVBS_IRQ_EDGE_EN_MASK		1
#define CVBS_IRQ_FILTER_BIT		1
#define CVBS_IRQ_FILTER_WIDTH		3
#define CVBS_IRQ_FILTER_MASK		7
#define CVBS_IRQ_POL_BIT		0
#define CVBS_IRQ_POL_WIDTH		1
#define CVBS_IRQ_POL_MASK		1

#define CVBS_IRQ1_CNTL			0x3c25
#define CVBS_IRQ0_COUNTER		0x3c26
#define CVBS_IRQ1_COUNTER		0x3c27

/* add t3 */
#define IRQCTRL_CVBS_IRQ0_CNTL		0x90
#define IRQCTRL_CVBS_IRQ1_CNTL		0x91
#define IRQCTRL_CVBS_IRQ0_COUNTER	0x92
#define IRQCTRL_CVBS_IRQ1_COUNTER	0x93

enum tvafe_avin_status_e {
	TVAFE_AVIN_STATUS_IN = 0,
	TVAFE_AVIN_STATUS_OUT = 1,
	TVAFE_AVIN_STATUS_UNKNOWN = 2,
};

enum tvafe_avin_channel_e {
	TVAFE_AVIN_CHANNEL1 = 0,
	TVAFE_AVIN_CHANNEL2 = 1,
	TVAFE_AVIN_CHANNEL_MAX = 2,
};

struct tvafe_report_data_s {
	enum tvafe_avin_channel_e channel;
	enum tvafe_avin_status_e status;
};

struct tvafe_dts_const_param_s {
	unsigned int device_mask;/*bit0:av1;bit1:av2*/
	/*when mask==3,sequence 0:bit0->av1 bit1->av2;1:bit0->av2 bit1->av1*/
	unsigned int device_sequence;
	unsigned int irq[2];
};

struct tvafe_avin_det_s {
	char config_name[20];
	dev_t  avin_devno;
	struct device *config_dev;
	struct class *config_class;
	struct cdev avin_cdev;
	struct tvafe_dts_const_param_s dts_param;
	struct tvafe_report_data_s report_data_s[2];
	struct work_struct work_struct_update;
	unsigned int irq_counter[2];
	unsigned int device_num;
	unsigned int function_select;
};

enum avin_cpu_type {
	AVIN_CPU_TYPE_TL1   = 3,
	AVIN_CPU_TYPE_TM2   = 4,
	AVIN_CPU_TYPE_T5    = 5,
	AVIN_CPU_TYPE_T5D   = 6,
	AVIN_CPU_TYPE_T3   = 7,
	AVIN_CPU_TYPE_T5W   = 8,
	AVIN_CPU_TYPE_MAX,
};

struct meson_avin_data {
	enum avin_cpu_type cpu_id;
	const char *name;
	void __iomem *irq_reg_base;

	unsigned int detect_cntl;
	unsigned int irq0_cntl;
	unsigned int irq1_cntl;
	unsigned int irq0_cnt;
	unsigned int irq1_cnt;
};

void tvafe_cha1_SYNCTIP_close_config(void);
void tvafe_cha2_SYNCTIP_close_config(void);
void tvafe_cha1_detect_restart_config(void);
void tvafe_cha2_detect_restart_config(void);
void tvafe_avin_detect_ch1_anlog_enable(bool enable);
void tvafe_avin_detect_ch2_anlog_enable(bool enable);

/*opened port,1:av1, 2:av2, 0:none av*/
extern unsigned int avport_opened;
/*0:in, 1:out*/
extern unsigned int av1_plugin_state;
extern unsigned int av2_plugin_state;
extern bool tvafe_clk_status;

#endif /* TVAFE_AVIN_DETECT_H_ */

