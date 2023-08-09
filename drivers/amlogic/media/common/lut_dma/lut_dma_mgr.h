/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef LUT_DMA_MGR_H_
#define LUT_DMA_MGR_H_

#define LUT_DMA_WR_CHANNEL     10
#define LUT_DMA_RD_CHANNEL     1
#define LUT_DMA_CHANNEL        (LUT_DMA_WR_CHANNEL + LUT_DMA_RD_CHANNEL)
#define LUT_DMA_RD_CHAN_NUM    10

#define VPU_DMA_RDMIF0_CTRL   0x2750
#define VPU_DMA_RDMIF1_CTRL   0x2751
#define VPU_DMA_RDMIF2_CTRL   0x2752
#define VPU_DMA_RDMIF3_CTRL   0x2753
#define VPU_DMA_RDMIF4_CTRL   0x2754
#define VPU_DMA_RDMIF5_CTRL   0x2755
#define VPU_DMA_RDMIF6_CTRL   0x2756
#define VPU_DMA_RDMIF7_CTRL   0x2757

#define VPU_DMA_RDMIF0_BADR0  0x2758
#define VPU_DMA_RDMIF0_BADR1  0x2759
#define VPU_DMA_RDMIF0_BADR2  0x275a
#define VPU_DMA_RDMIF0_BADR3  0x275b
#define VPU_DMA_RDMIF1_BADR0  0x275c
#define VPU_DMA_RDMIF1_BADR1  0x275d
#define VPU_DMA_RDMIF1_BADR2  0x275e
#define VPU_DMA_RDMIF1_BADR3  0x275f
#define VPU_DMA_RDMIF2_BADR0  0x2760
#define VPU_DMA_RDMIF2_BADR1  0x2761
#define VPU_DMA_RDMIF2_BADR2  0x2762
#define VPU_DMA_RDMIF2_BADR3  0x2763
#define VPU_DMA_RDMIF3_BADR0  0x2764
#define VPU_DMA_RDMIF3_BADR1  0x2765
#define VPU_DMA_RDMIF3_BADR2  0x2766
#define VPU_DMA_RDMIF3_BADR3  0x2767
#define VPU_DMA_RDMIF4_BADR0  0x2768
#define VPU_DMA_RDMIF4_BADR1  0x2769
#define VPU_DMA_RDMIF4_BADR2  0x276a
#define VPU_DMA_RDMIF4_BADR3  0x276b
#define VPU_DMA_RDMIF5_BADR0  0x276c
#define VPU_DMA_RDMIF5_BADR1  0x276d
#define VPU_DMA_RDMIF5_BADR2  0x276e
#define VPU_DMA_RDMIF5_BADR3  0x276f
#define VPU_DMA_RDMIF6_BADR0  0x2770
#define VPU_DMA_RDMIF6_BADR1  0x2771
#define VPU_DMA_RDMIF6_BADR2  0x2772
#define VPU_DMA_RDMIF6_BADR3  0x2773
#define VPU_DMA_RDMIF7_BADR0  0x2774
#define VPU_DMA_RDMIF7_BADR1  0x2775
#define VPU_DMA_RDMIF7_BADR2  0x2776
#define VPU_DMA_RDMIF7_BADR3  0x2777
#define VPU_DMA_RDMIF_SEL     0x2778
//Bit 31:1  reserved
//Bit 0     lut_reg_sel

#define VPU_DMA_WRMIF_CTRL1   0x27d1
#define VPU_DMA_WRMIF_CTRL2   0x27d2
#define VPU_DMA_WRMIF_CTRL3   0x27d3
#define VPU_DMA_WRMIF_BADDR0  0x27d4
#define VPU_DMA_WRMIF_RO_STAT 0x27d7
#define VPU_DMA_RDMIF_CTRL    0x27d8
#define VPU_DMA_RDMIF_BADDR1  0x27d9
#define VPU_DMA_RDMIF_BADDR2  0x27da
#define VPU_DMA_RDMIF_BADDR3  0x27db
#define VPU_DMA_WRMIF_CTRL    0x27dc
#define VPU_DMA_WRMIF_BADDR1  0x27dd
#define VPU_DMA_WRMIF_BADDR2  0x27de
#define VPU_DMA_WRMIF_BADDR3  0x27df

#define VPU_DMA_RDMIF_CTRL1   0x27ca
#define VPU_DMA_RDMIF_CTRL2   0x27cb
#define VPU_DMA_RDMIF_RO_STAT 0x27d0

#define DMA_BUF_NUM   4

struct lut_dma_ins {
	struct mutex lut_dma_lock;/*lut dma mutex*/
	unsigned char registered;
	unsigned char enable;
	unsigned char dir;
	unsigned char mode;
	unsigned char baddr_set;
	unsigned char force_disable;
	u32 trigger_irq_type;
	u32 rd_table_size[DMA_BUF_NUM];
	ulong rd_phy_addr[DMA_BUF_NUM];
	u32 *rd_table_addr[DMA_BUF_NUM];
	u32 wr_table_size[DMA_BUF_NUM];
	ulong wr_phy_addr[DMA_BUF_NUM];
	u32 *wr_table_addr[DMA_BUF_NUM];
	u32 wr_size[DMA_BUF_NUM];
};

struct lut_dma_device_info {
	const char *device_name;
	struct platform_device *pdev;
	struct class *clsp;
	struct lut_dma_ins ins[LUT_DMA_CHANNEL];
};

enum cpu_type_e {
	MESON_CPU_MAJOR_ID_COMPATIBLE = 0x1,
	MESON_CPU_MAJOR_ID_SC2_,
	MESON_CPU_MAJOR_ID_T7_,
	MESON_CPU_MAJOR_ID_S5_,
	MESON_CPU_MAJOR_ID_UNKNOWN_,
};

struct lutdma_device_data_s {
	enum cpu_type_e cpu_type;
	int support_8G_addr;
	int lut_dma_ver;
};
#endif
