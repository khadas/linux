// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/arm-smccc.h>
#include <linux/slab.h>
#include "ddr_port.h"
#include "dmc_monitor.h"

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct vpu_sub_desc vpu_sub_desc_txlx[] __initdata = {
	{ .sub_id = 0x0, .vpu_r0_2 = "OSD1", .vpu_r1 = "DI_IF1",
			.vpu_w0 = "VDIN0", .vpu_w1 = "NR"		},

	{ .sub_id = 0x1, .vpu_r0_2 = "OSD2", .vpu_r1 = "DI_MEM",
			.vpu_w0 = "VDIN1", .vpu_w1 = "DI"		},

	{ .sub_id = 0x2, .vpu_r0_2 = "VD1", .vpu_r1 = "DI_INP",
			.vpu_w0 = "VDIN_DOLBY", .vpu_w1 = "DI_SUB"	},

	{ .sub_id = 0x3, .vpu_r0_2 = "VD2", .vpu_r1 = "DI_CHAN2",
			.vpu_w0 = "AFBC_ENC", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x4, .vpu_r0_2 = "OSD3", .vpu_r1 = "DI_SUB",
			.vpu_w0 = "TVFE", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x5, .vpu_r0_2 = "AFBC_DEC", .vpu_r1 = "DI_IF2",
			.vpu_w0 = "TCON1", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x6, .vpu_r0_2 = "DOLBY0", .vpu_r1 = "DI_IF0",
			.vpu_w0 = "RDMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x7, .vpu_r0_2 = "MALI_AFBCD", .vpu_r1 = "NULL",
			.vpu_w0 = "TCON2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x8, .vpu_r0_2 = "VIU2", .vpu_r1 = "NULL",
			.vpu_w0 = "TCON3", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x9, .vpu_r0_2 = "TCON1", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xA, .vpu_r0_2 = "RDMA", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xB, .vpu_r0_2 = "TVFE", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xC, .vpu_r0_2 = "TCON_P2", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xD, .vpu_r0_2 = "TCON_P3", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xE, .vpu_r0_2 = "AFBC_ENC", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xF, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		}
};

//static struct vpu_sub_desc vpu_sub_desc_txl[] __initdata = vpu_sub_desc_txlx[]
//static struct vpu_sub_desc vpu_sub_desc_txhd[] __initdata = vpu_sub_desc_txlx[]

static struct vpu_sub_desc vpu_sub_desc_tl1[] __initdata = {
	{ .sub_id = 0x0, .vpu_r0_2 = "OSD1", .vpu_r1 = "DI_IF1",
			.vpu_w0 = "VDIN0", .vpu_w1 = "NR"		},

	{ .sub_id = 0x1, .vpu_r0_2 = "OSD2", .vpu_r1 = "DI_MEM",
			.vpu_w0 = "VDIN1", .vpu_w1 = "DI"		},

	{ .sub_id = 0x2, .vpu_r0_2 = "VD1", .vpu_r1 = "DI_INP",
			.vpu_w0 = "VDIN_DOLBY", .vpu_w1 = "DI_SUB"	},

	{ .sub_id = 0x3, .vpu_r0_2 = "VD2", .vpu_r1 = "DI_CHAN2",
			.vpu_w0 = "AFBC_ENC", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x4, .vpu_r0_2 = "OSD3", .vpu_r1 = "DI_SUB",
			.vpu_w0 = "TVFE", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x5, .vpu_r0_2 = "AFBC_DEC", .vpu_r1 = "DI_IF2",
			.vpu_w0 = "TCON1", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x6, .vpu_r0_2 = "DOLBY0", .vpu_r1 = "DI_IF0",
			.vpu_w0 = "RDMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x7, .vpu_r0_2 = "MALI_AFBCD", .vpu_r1 = "DI_AFBCD",
			.vpu_w0 = "TCON2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x8, .vpu_r0_2 = "VIU2", .vpu_r1 = "NULL",
			.vpu_w0 = "TCON3", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x9, .vpu_r0_2 = "TCON1", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xA, .vpu_r0_2 = "RDMA", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xB, .vpu_r0_2 = "TVFE", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xC, .vpu_r0_2 = "TCON_P2", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xD, .vpu_r0_2 = "TCON_P3", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xE, .vpu_r0_2 = "AFBC_ENC", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xF, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		}
};
#endif

static struct vpu_sub_desc vpu_sub_desc_sm1[] __initdata = {
	{ .sub_id = 0x0, .vpu_r0_2 = "OSD1", .vpu_r1 = "DI_IF1",
			.vpu_w0 = "VDIN0", .vpu_w1 = "NR"		},

	{ .sub_id = 0x1, .vpu_r0_2 = "OSD2", .vpu_r1 = "DI_MEM",
			.vpu_w0 = "VDIN1", .vpu_w1 = "DI"		},

	{ .sub_id = 0x2, .vpu_r0_2 = "VD1", .vpu_r1 = "DI_INP",
			.vpu_w0 = "VDIN_DOLBY", .vpu_w1 = "DI_SUB"	},

	{ .sub_id = 0x3, .vpu_r0_2 = "VD2", .vpu_r1 = "DI_CHAN2",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x4, .vpu_r0_2 = "OSD3", .vpu_r1 = "DI_SUB",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x5, .vpu_r0_2 = "NULL", .vpu_r1 = "DI_IF2",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x6, .vpu_r0_2 = "DOLBY0", .vpu_r1 = "DI_IF0",
			.vpu_w0 = "RDMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x7, .vpu_r0_2 = "MALI_AFBCD", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x8, .vpu_r0_2 = "VIU2", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x9, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xA, .vpu_r0_2 = "RDMA", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xB, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xC, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xD, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xE, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xF, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},
};

// static struct vpu_sub_desc vpu_sub_desc_g12b[] __initdata = vpu_sub_desc_sm1[]
// static struct vpu_sub_desc vpu_sub_desc_g12a[] __initdata = vpu_sub_desc_sm1[]

static struct vpu_sub_desc vpu_sub_desc_tm2[] __initdata = {
	{ .sub_id = 0x0, .vpu_r0_2 = "OSD1", .vpu_r1 = "DI_IF1",
			.vpu_w0 = "VDIN0", .vpu_w1 = "NR"		},

	{ .sub_id = 0x1, .vpu_r0_2 = "OSD2", .vpu_r1 = "DI_MEM",
			.vpu_w0 = "VDIN1", .vpu_w1 = "DI"		},

	{ .sub_id = 0x2, .vpu_r0_2 = "VD1", .vpu_r1 = "DI_INP",
			.vpu_w0 = "VDIN_DOLBY", .vpu_w1 = "DI_SUB"	},

	{ .sub_id = 0x3, .vpu_r0_2 = "VD2", .vpu_r1 = "DI_CHAN2",
			.vpu_w0 = "VDIN_AFBCE", .vpu_w1 = "NULL"	},

	{ .sub_id = 0x4, .vpu_r0_2 = "OSD3", .vpu_r1 = "DI_SUB",
			.vpu_w0 = "TVFE", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x5, .vpu_r0_2 = "AFBC_DEC", .vpu_r1 = "DI_IF2",
			.vpu_w0 = "TCON1", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x6, .vpu_r0_2 = "DOLBY0", .vpu_r1 = "DI_IF0",
			.vpu_w0 = "RDMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x7, .vpu_r0_2 = "MALI_AFBCD", .vpu_r1 = "NULL",
			.vpu_w0 = "TCON2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x8, .vpu_r0_2 = "VIU2", .vpu_r1 = "NULL",
			.vpu_w0 = "TCON3", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x9, .vpu_r0_2 = "TCON1", .vpu_r1 = "NULL",
			.vpu_w0 = "VDIN2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xA, .vpu_r0_2 = "RDMA", .vpu_r1 = "NULL",
			.vpu_w0 = "VPU_DMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xB, .vpu_r0_2 = "TVFE", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xC, .vpu_r0_2 = "TCON_P2", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xD, .vpu_r0_2 = "TCON_P3", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xE, .vpu_r0_2 = "VDIN_AFBCE", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xF, .vpu_r0_2 = "VPU_DMA", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},
};

static struct vpu_sub_desc vpu_sub_desc_t5[] __initdata = {
	{ .sub_id = 0x0, .vpu_r0_2 = "OSD1", .vpu_r1 = "DI_IF1",
			.vpu_w0 = "VDIN0", .vpu_w1 = "NR"		},

	{ .sub_id = 0x1, .vpu_r0_2 = "OSD2", .vpu_r1 = "DI_MEM",
			.vpu_w0 = "VDIN1", .vpu_w1 = "DI"		},

	{ .sub_id = 0x2, .vpu_r0_2 = "VD1", .vpu_r1 = "DI_INP",
			.vpu_w0 = "VDIN_DOLBY", .vpu_w1 = "DI_SUB"	},

	{ .sub_id = 0x3, .vpu_r0_2 = "VD2", .vpu_r1 = "DI_CHAN2",
			.vpu_w0 = "VDIN_AFBCE", .vpu_w1 = "DI_AFBCE0"	},

	{ .sub_id = 0x4, .vpu_r0_2 = "OSD3", .vpu_r1 = "DI_SUB",
			.vpu_w0 = "DCNTR_GRID", .vpu_w1 = "DI_AFBCE1"	},

	{ .sub_id = 0x5, .vpu_r0_2 = "AFBC_DEC", .vpu_r1 = "DI_IF2",
			.vpu_w0 = "TCON1", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x6, .vpu_r0_2 = "DOLBY0", .vpu_r1 = "DI_IF0",
			.vpu_w0 = "RDMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x7, .vpu_r0_2 = "MALI_AFBCD", .vpu_r1 = "DI_AFBCD",
			.vpu_w0 = "TCON2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x8, .vpu_r0_2 = "VIU2", .vpu_r1 = "NULL",
			.vpu_w0 = "LDIM", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x9, .vpu_r0_2 = "TCON1", .vpu_r1 = "NULL",
			.vpu_w0 = "VDIN2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xA, .vpu_r0_2 = "RDMA", .vpu_r1 = "NULL",
			.vpu_w0 = "VPU_DMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xB, .vpu_r0_2 = "TVFE", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xC, .vpu_r0_2 = "TCON_P2", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xD, .vpu_r0_2 = "DCNTR_GRID", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xE, .vpu_r0_2 = "AFBC_ENC", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xF, .vpu_r0_2 = "VPU_DMA", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},
};

static struct vpu_sub_desc vpu_sub_desc_t5d[] __initdata = {
	{ .sub_id = 0x0, .vpu_r0_2 = "OSD1", .vpu_r1 = "DI_IF1",
			.vpu_w0 = "VDIN0", .vpu_w1 = "NR"		},

	{ .sub_id = 0x1, .vpu_r0_2 = "OSD2", .vpu_r1 = "DI_MEM",
			.vpu_w0 = "VDIN1", .vpu_w1 = "DI"		},

	{ .sub_id = 0x2, .vpu_r0_2 = "VD1", .vpu_r1 = "DI_INP",
			.vpu_w0 = "VDIN_DOLBY", .vpu_w1 = "DI_SUB"	},

	{ .sub_id = 0x3, .vpu_r0_2 = "VD2", .vpu_r1 = "DI_CHAN2",
			.vpu_w0 = "AFBC_ENC", .vpu_w1 = "DI_AFBCE"	},

	{ .sub_id = 0x4, .vpu_r0_2 = "OSD3", .vpu_r1 = "DI_SUB",
			.vpu_w0 = "TVFE", .vpu_w1 = "NULL"	},

	{ .sub_id = 0x5, .vpu_r0_2 = "AFBC_DEC", .vpu_r1 = "DI_IF2",
			.vpu_w0 = "TCON1", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x6, .vpu_r0_2 = "DOLBY0", .vpu_r1 = "DI_IF0",
			.vpu_w0 = "RDMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x7, .vpu_r0_2 = "MALI_AFBCD", .vpu_r1 = "DI_AFBCD",
			.vpu_w0 = "TCON2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x8, .vpu_r0_2 = "VIU2", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x9, .vpu_r0_2 = "TCON1", .vpu_r1 = "NULL",
			.vpu_w0 = "VDIN2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xA, .vpu_r0_2 = "RDMA", .vpu_r1 = "NULL",
			.vpu_w0 = "VPU_DMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xB, .vpu_r0_2 = "TVFE", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xC, .vpu_r0_2 = "TCON_P2", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xD, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xE, .vpu_r0_2 = "AFBC_ENC", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xF, .vpu_r0_2 = "VPU_DMA", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},
};

static struct vpu_sub_desc vpu_sub_desc_t7[] __initdata = {
	{ .sub_id = 0x0, .vpu_r0_2 = "OSD1", .vpu_r1 = "DI_IF1",
			.vpu_w0 = "VDIN0", .vpu_w1 = "NR"		},

	{ .sub_id = 0x1, .vpu_r0_2 = "OSD2", .vpu_r1 = "DI_MEM",
			.vpu_w0 = "VDIN1", .vpu_w1 = "DI"		},

	{ .sub_id = 0x2, .vpu_r0_2 = "VD1", .vpu_r1 = "DI_INP",
			.vpu_w0 = "VDIN_DOLBY", .vpu_w1 = "DI_SUB"	},

	{ .sub_id = 0x3, .vpu_r0_2 = "VD2", .vpu_r1 = "DI_CHAN2",
			.vpu_w0 = "VDIN_AFBCE", .vpu_w1 = "DI_AFBCE0"	},

	{ .sub_id = 0x4, .vpu_r0_2 = "OSD3", .vpu_r1 = "DI_SUB",
			.vpu_w0 = "DCNTR_GRID", .vpu_w1 = "DI_AFBCE1"	},

	{ .sub_id = 0x5, .vpu_r0_2 = "OSD4", .vpu_r1 = "DI_IF2",
			.vpu_w0 = "TCON1", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x6, .vpu_r0_2 = "DOLBY0", .vpu_r1 = "DI_IF0",
			.vpu_w0 = "RDMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x7, .vpu_r0_2 = "MALI_AFBCD", .vpu_r1 = "DI_AFBCD",
			.vpu_w0 = "TCON2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x8, .vpu_r0_2 = "VD3", .vpu_r1 = "NULL",
			.vpu_w0 = "LDIM", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x9, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "VDIN2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xA, .vpu_r0_2 = "RDMA", .vpu_r1 = "NULL",
			.vpu_w0 = "VPU_DMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xB, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xC, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xD, .vpu_r0_2 = "LDIM", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xE, .vpu_r0_2 = "VDIN_AFBCE", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xF, .vpu_r0_2 = "VPU_DMA", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},
};

static struct vpu_sub_desc vpu_sub_desc_t3[] __initdata = {
	{ .sub_id = 0x0, .vpu_r0_2 = "OSD1", .vpu_r1 = "DI_IF1",
			.vpu_w0 = "VDIN0", .vpu_w1 = "NR"		},

	{ .sub_id = 0x1, .vpu_r0_2 = "OSD2", .vpu_r1 = "DI_MEM",
			.vpu_w0 = "VDIN1", .vpu_w1 = "DI"		},

	{ .sub_id = 0x2, .vpu_r0_2 = "VD1", .vpu_r1 = "DI_INP",
			.vpu_w0 = "VDIN_DOLBY", .vpu_w1 = "DI_SUB"	},

	{ .sub_id = 0x3, .vpu_r0_2 = "VD2", .vpu_r1 = "DI_CHAN2",
			.vpu_w0 = "VDIN_AFBCE", .vpu_w1 = "DI_AFBCE0"	},

	{ .sub_id = 0x4, .vpu_r0_2 = "OSD3", .vpu_r1 = "DI_SUB",
			.vpu_w0 = "DCNTR_GRID", .vpu_w1 = "DI_AFBCE1"	},

	{ .sub_id = 0x5, .vpu_r0_2 = "OSD4", .vpu_r1 = "DI_IF2",
			.vpu_w0 = "TCON1", .vpu_w1 = "AISR_HPF"		},

	{ .sub_id = 0x6, .vpu_r0_2 = "DOLBY0", .vpu_r1 = "DI_IF0",
			.vpu_w0 = "RDMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x7, .vpu_r0_2 = "MALI_AFBCD", .vpu_r1 = "DI_AFBCD",
			.vpu_w0 = "TCON2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x8, .vpu_r0_2 = "NPU", .vpu_r1 = "NULL",
			.vpu_w0 = "LDIM", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x9, .vpu_r0_2 = "TCON", .vpu_r1 = "NULL",
			.vpu_w0 = "VDIN2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xA, .vpu_r0_2 = "RDMA", .vpu_r1 = "NULL",
			.vpu_w0 = "VPU_DMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xB, .vpu_r0_2 = "DCNTR_GRID", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xC, .vpu_r0_2 = "TCON_P2", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xD, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xE, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xF, .vpu_r0_2 = "VPU_SUBRD", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		}
};

// static struct vpu_sub_desc vpu_sub_desc_s4d[] __initdata = vpu_sub_desc_s4[]
static struct vpu_sub_desc vpu_sub_desc_s4[] __initdata = {
	{ .sub_id = 0x0, .vpu_r0_2 = "OSD1", .vpu_r1 = "DI_IF1",
			.vpu_w0 = "VDIN0", .vpu_w1 = "NR"		},

	{ .sub_id = 0x1, .vpu_r0_2 = "OSD2", .vpu_r1 = "DI_MEM",
			.vpu_w0 = "VDIN1", .vpu_w1 = "DI"		},

	{ .sub_id = 0x2, .vpu_r0_2 = "VD1", .vpu_r1 = "DI_INP",
			.vpu_w0 = "VDIN_DOLBY", .vpu_w1 = "DI_SUB"	},

	{ .sub_id = 0x3, .vpu_r0_2 = "VD2", .vpu_r1 = "DI_CHAN2",
			.vpu_w0 = "AFBC_ENC", .vpu_w1 = "DI_AFBCE0"		},

	{ .sub_id = 0x4, .vpu_r0_2 = "OSD3", .vpu_r1 = "DI_SUB",
			.vpu_w0 = "TVFE", .vpu_w1 = "DI_AFBCE1"		},

	{ .sub_id = 0x5, .vpu_r0_2 = "AFBC_DEC", .vpu_r1 = "DI_IF2",
			.vpu_w0 = "TCON1", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x6, .vpu_r0_2 = "DOLBY0", .vpu_r1 = "DI_IF0",
			.vpu_w0 = "RDMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x7, .vpu_r0_2 = "MALI_AFBCD", .vpu_r1 = "DI_AFBCD",
			.vpu_w0 = "TCON2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x8, .vpu_r0_2 = "VIU2", .vpu_r1 = "NULL",
			.vpu_w0 = "TCON3", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x9, .vpu_r0_2 = "TCON1", .vpu_r1 = "NULL",
			.vpu_w0 = "VDIN2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xA, .vpu_r0_2 = "RDMA", .vpu_r1 = "NULL",
			.vpu_w0 = "VPU_DMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xB, .vpu_r0_2 = "TVFE", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xC, .vpu_r0_2 = "TCON_P2", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xD, .vpu_r0_2 = "TCON_P3", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xE, .vpu_r0_2 = "AFBC_ENC", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xF, .vpu_r0_2 = "VPU_DMA", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		}
};

static struct vpu_sub_desc vpu_sub_desc_sc2[] __initdata = {
	{ .sub_id = 0x0, .vpu_r0_2 = "OSD1", .vpu_r1 = "DI_IF1",
			.vpu_w0 = "VDIN0", .vpu_w1 = "NR"		},

	{ .sub_id = 0x1, .vpu_r0_2 = "OSD2", .vpu_r1 = "DI_MEM",
			.vpu_w0 = "VDIN1", .vpu_w1 = "DI"		},

	{ .sub_id = 0x2, .vpu_r0_2 = "VD1", .vpu_r1 = "DI_INP",
			.vpu_w0 = "VDIN_DOLBY", .vpu_w1 = "DI_SUB"	},

	{ .sub_id = 0x3, .vpu_r0_2 = "VD2", .vpu_r1 = "DI_CHAN2",
			.vpu_w0 = "VDIN_AFBCE", .vpu_w1 = "DI_AFBCE0"	},

	{ .sub_id = 0x4, .vpu_r0_2 = "OSD3", .vpu_r1 = "DI_SUB",
			.vpu_w0 = "TVFE", .vpu_w1 = "DI_AFBCE1"		},

	{ .sub_id = 0x5, .vpu_r0_2 = "OSD4", .vpu_r1 = "DI_IF2",
			.vpu_w0 = "TCON1", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x6, .vpu_r0_2 = "DOLBY0", .vpu_r1 = "DI_IF0",
			.vpu_w0 = "RDMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x7, .vpu_r0_2 = "MALI_AFBCD", .vpu_r1 = "DI_AFBCD",
			.vpu_w0 = "TCON2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x8, .vpu_r0_2 = "VIU2", .vpu_r1 = "NULL",
			.vpu_w0 = "TCON3", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x9, .vpu_r0_2 = "TCON1", .vpu_r1 = "NULL",
			.vpu_w0 = "VDIN2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xA, .vpu_r0_2 = "RDMA", .vpu_r1 = "NULL",
			.vpu_w0 = "VPU_DMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xB, .vpu_r0_2 = "TVFE", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xC, .vpu_r0_2 = "TCON_P2", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xD, .vpu_r0_2 = "TCON_P3", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xE, .vpu_r0_2 = "VDIN_AFBCE", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xF, .vpu_r0_2 = "VPU_DMA", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		}
};

static struct vpu_sub_desc vpu_sub_desc_t5w[] __initdata = {
	{ .sub_id = 0x0, .vpu_r0_2 = "OSD1", .vpu_r1 = "DI_IF1",
			.vpu_w0 = "VDIN0", .vpu_w1 = "NR"		},

	{ .sub_id = 0x1, .vpu_r0_2 = "OSD2", .vpu_r1 = "DI_MEM",
			.vpu_w0 = "VDIN1", .vpu_w1 = "DI"		},

	{ .sub_id = 0x2, .vpu_r0_2 = "VD1", .vpu_r1 = "DI_INP",
			.vpu_w0 = "VDIN_DOLBY", .vpu_w1 = "DI_SUB"	},

	{ .sub_id = 0x3, .vpu_r0_2 = "VD2", .vpu_r1 = "DI_CHAN2",
			.vpu_w0 = "AFBC_ENC", .vpu_w1 = "DI_AFBCE0"	},

	{ .sub_id = 0x4, .vpu_r0_2 = "OSD3", .vpu_r1 = "DI_SUB",
			.vpu_w0 = "DCNTR_GRID", .vpu_w1 = "DI_AFBCE1"	},

	{ .sub_id = 0x5, .vpu_r0_2 = "AFBC_DEC", .vpu_r1 = "DI_IF2",
			.vpu_w0 = "TCON1", .vpu_w1 = "ASIR_HPF"		},

	{ .sub_id = 0x6, .vpu_r0_2 = "DOLBY0", .vpu_r1 = "DI_IF0",
			.vpu_w0 = "RDMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x7, .vpu_r0_2 = "MALI_AFBCD", .vpu_r1 = "DI_AFBCD",
			.vpu_w0 = "TCON2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x8, .vpu_r0_2 = "NPU", .vpu_r1 = "NULL",
			.vpu_w0 = "LDIM", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x9, .vpu_r0_2 = "TCON1", .vpu_r1 = "NULL",
			.vpu_w0 = "VDIN2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xA, .vpu_r0_2 = "RDMA", .vpu_r1 = "NULL",
			.vpu_w0 = "VPU_DMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xB, .vpu_r0_2 = "DCNTR_GRID", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xC, .vpu_r0_2 = "TCON_P2", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xD, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xE, .vpu_r0_2 = "NULL", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xF, .vpu_r0_2 = "VPU_SUBRD", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		}
};

static struct vpu_sub_desc vpu_sub_desc_s5[] __initdata = {
	{ .sub_id = 0x0, .vpu_r0_2 = "OSD1", .vpu_r1 = "DI_IF1",
			.vpu_w0 = "VDIN0", .vpu_w1 = "NR"		},

	{ .sub_id = 0x1, .vpu_r0_2 = "OSD2", .vpu_r1 = "DI_MEM",
			.vpu_w0 = "VDIN1", .vpu_w1 = "DI"		},

	{ .sub_id = 0x2, .vpu_r0_2 = "VD1", .vpu_r1 = "DI_INP",
			.vpu_w0 = "VDIN_DOLBY", .vpu_w1 = "DI_SUB"	},

	{ .sub_id = 0x3, .vpu_r0_2 = "VD2", .vpu_r1 = "DI_CHAN2",
			.vpu_w0 = "VDIN_AFBCE", .vpu_w1 = "DI_AFBCE0"	},

	{ .sub_id = 0x4, .vpu_r0_2 = "OSD3", .vpu_r1 = "DI_SUB",
			.vpu_w0 = "DCNTR_GRID", .vpu_w1 = "DI_AFBCE1"	},

	{ .sub_id = 0x5, .vpu_r0_2 = "OSD4", .vpu_r1 = "DI_IF2",
			.vpu_w0 = "TCON1", .vpu_w1 = "ASIR_HPF"		},

	{ .sub_id = 0x6, .vpu_r0_2 = "VD3", .vpu_r1 = "DI_IF0",
			.vpu_w0 = "RDMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x7, .vpu_r0_2 = "MALI_AFBCD", .vpu_r1 = "DI_AFBCD",
			.vpu_w0 = "TCON2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x8, .vpu_r0_2 = "NPU READ", .vpu_r1 = "NULL",
			.vpu_w0 = "LDIM", .vpu_w1 = "NULL"		},

	{ .sub_id = 0x9, .vpu_r0_2 = "VD4", .vpu_r1 = "NULL",
			.vpu_w0 = "VDIN2", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xA, .vpu_r0_2 = "VD5", .vpu_r1 = "NULL",
			.vpu_w0 = "VPU_DMA", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xB, .vpu_r0_2 = "NOUSE", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xC, .vpu_r0_2 = "DCNTR_GRID", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xD, .vpu_r0_2 = "TCON_P1", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xE, .vpu_r0_2 = "TCON_P2", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		},

	{ .sub_id = 0xF, .vpu_r0_2 = "VPU_SUBRD", .vpu_r1 = "NULL",
			.vpu_w0 = "NULL", .vpu_w1 = "NULL"		}
};

static struct vpu_sub_desc *vpu_port_sub;
static unsigned int vpu_port_sub_num __initdata;

int __init dmc_find_port_sub(int cpu_type, struct vpu_sub_desc **desc)
{
	int desc_size = -EINVAL;

	if (vpu_port_sub) {
		*desc = vpu_port_sub;
		return vpu_port_sub_num;
	}

	switch (cpu_type) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	case DMC_TYPE_TXL:
	case DMC_TYPE_TXLX:
	case DMC_TYPE_TXHD:
		*desc = vpu_sub_desc_txlx;
		desc_size = ARRAY_SIZE(vpu_sub_desc_txlx);
		break;
	case DMC_TYPE_TL1:
		*desc = vpu_sub_desc_tl1;
		desc_size = ARRAY_SIZE(vpu_sub_desc_tl1);
		break;
#endif
	case DMC_TYPE_G12A:
	case DMC_TYPE_G12B:
	case DMC_TYPE_SM1:
		*desc = vpu_sub_desc_sm1;
		desc_size = ARRAY_SIZE(vpu_sub_desc_sm1);
		break;
	case DMC_TYPE_TM2:
		*desc = vpu_sub_desc_tm2;
		desc_size = ARRAY_SIZE(vpu_sub_desc_tm2);
		break;
	case DMC_TYPE_T5:
		*desc = vpu_sub_desc_t5;
		desc_size = ARRAY_SIZE(vpu_sub_desc_t5);
		break;
	case DMC_TYPE_T5D:
		*desc = vpu_sub_desc_t5d;
		desc_size = ARRAY_SIZE(vpu_sub_desc_t5d);
		break;
	case DMC_TYPE_T7:
		*desc = vpu_sub_desc_t7;
		desc_size = ARRAY_SIZE(vpu_sub_desc_t7);
		break;
	case DMC_TYPE_T3:
		*desc = vpu_sub_desc_t3;
		desc_size = ARRAY_SIZE(vpu_sub_desc_t3);
		break;
	case DMC_TYPE_S4:
		*desc = vpu_sub_desc_s4;
		desc_size = ARRAY_SIZE(vpu_sub_desc_s4);
		break;
	case DMC_TYPE_SC2:
		*desc = vpu_sub_desc_sc2;
		desc_size = ARRAY_SIZE(vpu_sub_desc_sc2);
		break;
	case DMC_TYPE_T5W:
	case DMC_TYPE_T5M:
		*desc = vpu_sub_desc_t5w;
		desc_size = ARRAY_SIZE(vpu_sub_desc_t5w);
		break;
	case DMC_TYPE_S5:
		*desc = vpu_sub_desc_s5;
		desc_size = ARRAY_SIZE(vpu_sub_desc_s5);
		break;
	default:
		return -EINVAL;
	}
	/* using once */
	vpu_port_sub = kcalloc(desc_size, sizeof(*vpu_port_sub), GFP_KERNEL);
	if (!vpu_port_sub)
		return -ENOMEM;
	memcpy(vpu_port_sub, *desc, sizeof(struct vpu_sub_desc) * desc_size);
	vpu_port_sub_num = desc_size;
	*desc = vpu_port_sub;

	return desc_size;
}

char *vpu_to_sub_port(char *name, char rw, int sid, char *id_str)
{
	sprintf(id_str, "%s", "none");

	if (!dmc_mon->vpu_port || sid >= dmc_mon->vpu_port_num) {
		pr_info("vpu port null\n");
		return NULL;
	}

	if (dmc_mon->vpu_port[sid].sub_id == sid) {
		if (!strncmp(name, "VPU0", 4)) {
			if (rw == 'r')
				return dmc_mon->vpu_port[sid].vpu_r0_2;
			else
				return dmc_mon->vpu_port[sid].vpu_w0;
		} else if (!strncmp(name, "VPU1", 4)) {
			if (rw == 'r')
				return dmc_mon->vpu_port[sid].vpu_r1;
			else
				return dmc_mon->vpu_port[sid].vpu_w1;
		} else if (!strncmp(name, "VPU2", 4)) {
			if (rw == 'r')
				return dmc_mon->vpu_port[sid].vpu_r0_2;
			else
				return id_str;
		} else if (!strncmp(name, "VPU3", 4)) {
			if (rw == 'r')
				return dmc_mon->vpu_port[sid].vpu_r0_2;
			else
				return id_str;
		}
	}
	return id_str;
}
