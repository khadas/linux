/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_DDR_BANDWIDTH_H__
#define __AML_DDR_BANDWIDTH_H__

#define MODE_DISABLE			0
#define MODE_ENABLE			1
#define MODE_AUTODETECT			2

#define DEFAULT_THRESHOLD		5000

#define DEFAULT_CLK_CNT			48000000
#define DEFAULT_XTAL_FREQ		24000000UL

#define DMC_QOS_IRQ			BIT(30)
#define MAX_CHANNEL			8

/* for soc_feature */
#define DDR_WIDTH_IS_16BIT	BIT(3)
#define DDR_DEVICE_8BIT		BIT(2)
#define PLL_IS_SEC		BIT(1)

#include "ddr_port.h"
/*
 * register offset for chips before g12
 */
#define DMC_MON_CTRL1			(0x25 << 2)
#define DMC_MON_CTRL2			(0x26 << 2)
#define DMC_MON_CTRL3			(0x27 << 2)
#define DMC_MON_ALL_REQ_CNT		(0x28 << 2)
#define DMC_MON_ALL_GRANT_CNT		(0x29 << 2)
#define DMC_MON_ONE_GRANT_CNT		(0x2a << 2)
#define DMC_MON_SEC_GRANT_CNT		(0x2b << 2)
#define DMC_MON_THD_GRANT_CNT		(0x2c << 2)
#define DMC_MON_FOR_GRANT_CNT		(0x2d << 2)

#define DMC_MON_CTRL4			(0x18 << 2)
#define DMC_MON_CTRL5			(0x19 << 2)
#define DMC_MON_CTRL6			(0x1a << 2)

#define DMC_AM0_CHAN_CTRL		(0x60 << 2)
#define DMC_AM1_CHAN_CTRL		(0x6a << 2)
#define DMC_AM2_CHAN_CTRL		(0x74 << 2)
#define DMC_AM3_CHAN_CTRL		(0x7e << 2)
#define DMC_AM4_CHAN_CTRL		(0x88 << 2)
#define DMC_AM5_CHAN_CTRL		(0x92 << 2)
#define DMC_AM6_CHAN_CTRL		(0x9c << 2)
#define DMC_AM7_CHAN_CTRL		(0xa6 << 2)
#define DMC_AXI0_CHAN_CTRL		(0xb0 << 2)
#define DMC_AXI1_CHAN_CTRL		(0xba << 2)
#define DMC_AXI2_CHAN_CTRL		(0xc4 << 2)
#define DMC_AXI3_CHAN_CTRL		(0xce << 2)
#define DMC_AXI4_CHAN_CTRL		(0xd8 << 2)
#define DMC_AXI5_CHAN_CTRL		(0xe2 << 2)
#define DMC_AXI6_CHAN_CTRL		(0xec << 2)
#define DMC_AXI7_CHAN_CTRL		(0xf6 << 2)

/*
 * register offset for g12a
 */
#define DMC_MON_G12_CTRL0		(0x20  << 2)
#define DMC_MON_G12_CTRL1		(0x21  << 2)
#define DMC_MON_G12_CTRL2		(0x22  << 2)
#define DMC_MON_G12_CTRL3		(0x23  << 2)
#define DMC_MON_G12_CTRL4		(0x24  << 2)
#define DMC_MON_G12_CTRL5		(0x25  << 2)
#define DMC_MON_G12_CTRL6		(0x26  << 2)
#define DMC_MON_G12_CTRL7		(0x27  << 2)
#define DMC_MON_G12_CTRL8		(0x28  << 2)

#define DMC_MON_G12_ALL_REQ_CNT		(0x29  << 2)
#define DMC_MON_G12_ALL_GRANT_CNT	(0x2a  << 2)
#define DMC_MON_G12_ONE_GRANT_CNT	(0x2b  << 2)
#define DMC_MON_G12_SEC_GRANT_CNT	(0x2c  << 2)
#define DMC_MON_G12_THD_GRANT_CNT	(0x2d  << 2)
#define DMC_MON_G12_FOR_GRANT_CNT	(0x2e  << 2)
#define DMC_MON_G12_TIMER		(0x2f  << 2)

#define DMC_AM0_G12_CHAN_CTRL		(0x60 << 2)
#define DMC_AM1_G12_CHAN_CTRL		(0x64 << 2)
#define DMC_AM2_G12_CHAN_CTRL		(0x68 << 2)
#define DMC_AM3_G12_CHAN_CTRL		(0x6c << 2)
#define DMC_AM4_G12_CHAN_CTRL		(0x70 << 2)
#define DMC_AM5_G12_CHAN_CTRL		(0x74 << 2)
#define DMC_AM6_G12_CHAN_CTRL		(0x78 << 2)
#define DMC_AM7_G12_CHAN_CTRL		(0x7c << 2)
#define DMC_AXI0_G12_CHAN_CTRL		(0x80 << 2)
#define DMC_AXI1_G12_CHAN_CTRL		(0x84 << 2)
#define DMC_AXI2_G12_CHAN_CTRL		(0x88 << 2)
#define DMC_AXI3_G12_CHAN_CTRL		(0x8c << 2)
#define DMC_AXI4_G12_CHAN_CTRL		(0x90 << 2)
#define DMC_AXI5_G12_CHAN_CTRL		(0x94 << 2)
#define DMC_AXI6_G12_CHAN_CTRL		(0x98 << 2)
#define DMC_AXI7_G12_CHAN_CTRL		(0x9c << 2)
#define DMC_AXI8_G12_CHAN_CTRL		(0xa0 << 2)
#define DMC_AXI9_G12_CHAN_CTRL		(0xa4 << 2)
#define DMC_AXI10_G12_CHAN_CTRL		(0xa8 << 2)
#define DMC_AXI11_G12_CHAN_CTRL		(0xac << 2)
#define DMC_AXI12_G12_CHAN_CTRL		(0xb0 << 2)

/* data structure */
#define DDR_BANDWIDTH_DEBUG		1
#define DDR_PRIORITY_DEBUG		BIT(31)
#define DDR_PRIORITY_POWER		BIT(30)

struct ddr_bandwidth;

struct ddr_grant {
	unsigned long long tick;
	unsigned int all_grant;
	unsigned int all_req;
	unsigned int all_grant16;
	unsigned int channel_grant[MAX_CHANNEL];
};

struct ddr_bandwidth_ops {
	void (*init)(struct ddr_bandwidth *db);
	void (*config_port)(struct ddr_bandwidth *db, int channel, int port);
	void (*config_range)(struct ddr_bandwidth *db, int channel,
			     unsigned long start, unsigned long end);
	int  (*handle_irq)(struct ddr_bandwidth *db, struct ddr_grant *dg);
	void (*bandwidth_enable)(struct ddr_bandwidth *db);
	unsigned long (*get_freq)(struct ddr_bandwidth *db);
#if	DDR_BANDWIDTH_DEBUG
	int (*dump_reg)(struct ddr_bandwidth *db, char *buf);
#endif
};

struct ddr_bandwidth_sample {
	unsigned long long tick;
	unsigned int total_usage;
	unsigned int total_bandwidth;
	unsigned int bandwidth[MAX_CHANNEL];
};

struct ddr_avg_bandwidth {
	unsigned long long avg_bandwidth;
	unsigned long long avg_usage;
	unsigned long long avg_port[MAX_CHANNEL];
	unsigned long max_bandwidth[MAX_CHANNEL];
	unsigned int sample_count;
};

struct bandwidth_addr_range {
	unsigned long start;
	unsigned long end;
};

struct ddr_increase_tool {
	char *buf_addr;
	unsigned int increase_MB;
	unsigned int once_size;
	u64 t_ns;
};

struct ddr_bandwidth {
	unsigned short cpu_type;
	unsigned short real_ports;
	char busy;
	char mode;
	char bytes_per_cycle;
	char soc_feature;		/* some special feature of it */
	int mali_port[2];
	int stat_flag;
	unsigned int ddr_priority_num;
	unsigned int threshold;
	unsigned int irq_num;
	unsigned int clock_count;
	unsigned int channels;
	unsigned int dmc_number;
	unsigned int usage_stat[10];
	spinlock_t lock;		/* lock for usage statistics */
	struct ddr_bandwidth_sample cur_sample;
	struct ddr_bandwidth_sample prev_sample;
	struct ddr_bandwidth_sample max_sample;
	struct ddr_avg_bandwidth    avg;
	struct bandwidth_addr_range range[MAX_CHANNEL];
	u64	     port[MAX_CHANNEL];
	void __iomem *ddr_reg1;		/* dmc 1 */
	void __iomem *ddr_reg2;		/* dmc 2 */
	void __iomem *ddr_reg3;		/* dmc 3 */
	void __iomem *ddr_reg4;		/* dmc 4 */
	void __iomem *pll_reg;
	struct class *class;
	struct ddr_port_desc *port_desc;
	struct ddr_priority *ddr_priority_desc;
	struct ddr_bandwidth_ops *ops;
	struct work_struct work_bandwidth;
	struct ddr_increase_tool increase_tool;
};

extern struct ddr_bandwidth *aml_db;
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_G12
extern struct ddr_bandwidth_ops g12_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_GX
extern struct ddr_bandwidth_ops gx_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_GXL
extern struct ddr_bandwidth_ops gxl_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_A1
extern struct ddr_bandwidth_ops a1_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T5
extern struct ddr_bandwidth_ops t5_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T7
extern struct ddr_bandwidth_ops t7_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_S4
extern struct ddr_bandwidth_ops s4_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_S5
extern struct ddr_bandwidth_ops s5_ddr_bw_ops;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_A4
extern struct ddr_bandwidth_ops a4_ddr_bw_ops;
#endif

unsigned int aml_get_ddr_usage(void);

#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH
int __init ddr_bandwidth_init(void);
void ddr_bandwidth_exit(void);
#else
static int ddr_bandwidth_init(void)
{
	return 0;
}

static void ddr_bandwidth_exit(void)
{
}
#endif

#endif /* __AML_DDR_BANDWIDTH_H__ */
