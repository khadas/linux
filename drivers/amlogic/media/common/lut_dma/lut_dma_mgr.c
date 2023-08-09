// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>

#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>
#include <linux/delay.h>

/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>

#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/lut_dma/lut_dma.h>

#include "lut_dma_mgr.h"
#include "lut_dma_io.h"

#define DRIVER_NAME "amlogic_lut_dma"
#define MODULE_NAME "amlogic_lut_dma"
#define DEVICE_NAME "lut_dma"
#define CLASS_NAME  "lut_dma"

#define MAX_BUF_SIZE         (498 << 4)
#define BYTE_16_ALIGNED(x)    (((x) + 15) & ~15)

static struct vpu_dev_s *vpu_dma;
static int log_level;
static int lut_dma_test_mode;
static int lut_dma_test_channel;
static struct lut_dma_device_info lut_dma_info;
static int lut_dma_probed;
#define pr_dbg(fmt, args...) \
	do { \
		if (log_level >= 1) \
			pr_info("LUT DMA: " fmt, ## args); \
	} while (0)
#define pr_error(fmt, args...) pr_err("LUT DMA: " fmt, ## args)

static struct lutdma_device_data_s lutdma_meson_dev;

static struct lutdma_device_data_s lut_dma = {
	.cpu_type = MESON_CPU_MAJOR_ID_COMPATIBLE,
	.support_8G_addr = 0,
	.lut_dma_ver = 1,
};

static struct lutdma_device_data_s lut_dma_sc2 = {
	.cpu_type = MESON_CPU_MAJOR_ID_SC2_,
	.support_8G_addr = 0,
	.lut_dma_ver = 1,
};

static struct lutdma_device_data_s lut_dma_t7 = {
	.cpu_type = MESON_CPU_MAJOR_ID_T7_,
	.support_8G_addr = 1,
	.lut_dma_ver = 1,
};

static struct lutdma_device_data_s lut_dma_s5 = {
	.cpu_type = MESON_CPU_MAJOR_ID_S5_,
	.support_8G_addr = 1,
	.lut_dma_ver = 2,
};

static const struct of_device_id lut_dma_dt_match[] = {
	{
		.compatible = "amlogic, meson, lut_dma",
		.data = &lut_dma,
	},
	{
		.compatible = "amlogic, meson-sc2, lut_dma",
		.data = &lut_dma_sc2,
	},
	{
		.compatible = "amlogic, meson-t7, lut_dma",
		.data = &lut_dma_t7,
	},
	{
		.compatible = "amlogic, meson-s5, lut_dma",
		.data = &lut_dma_s5,
	},
	{}
};

static bool lutdma_is_meson_sc2_cpu(void)
{
	if (lutdma_meson_dev.cpu_type ==
		MESON_CPU_MAJOR_ID_SC2_)
		return true;
	else
		return false;
}

static bool lutdma_is_meson_t7_cpu(void)
{
	if (lutdma_meson_dev.cpu_type ==
		MESON_CPU_MAJOR_ID_T7_)
		return true;
	else
		return false;
}

static bool lutdma_is_meson_s5_cpu(void)
{
	if (lutdma_meson_dev.cpu_type ==
		MESON_CPU_MAJOR_ID_S5_)
		return true;
	else
		return false;
}

static u32 get_cur_rd_frame_index(void)
{
	u32 data, frame_index;

	data = lut_dma_reg_read(VPU_DMA_WRMIF_CTRL);
	frame_index = (data & 0x1f00) >> 11;
	pr_dbg("cur rd frame index:%d\n", frame_index);
	return frame_index;
}

static u32 get_cur_wr_frame_index(u32 channel)
{
	u32 data, reg, frame_index;
	u32 channel_index = 0;

	if (channel >= 8)
		channel_index = channel - 8;
	else
		channel_index = channel;
	reg = VPU_DMA_RDMIF0_CTRL + channel_index;
	data = lut_dma_reg_read(reg);
	frame_index = (data & 0x30000000) >> 28;
	pr_dbg("chan(%d)cur wr frame index:%d\n",
	       channel,
	       frame_index);
	return frame_index;
}

/* for dma it is read */
static void set_lut_dma_rdcfg(u32 channel)
{
	int i;
	u32 base_addr, buf_size, trans_size;
	struct lut_dma_device_info *info = &lut_dma_info;
	u32 phy_addr;

	/* set wr mif */
	if (!info->ins[channel].baddr_set) {
		for (i = 0; i < DMA_BUF_NUM; i++) {
			switch (i) {
			case 0:
				base_addr = VPU_DMA_WRMIF_BADDR0;
				break;
			case 1:
				base_addr = VPU_DMA_WRMIF_BADDR1;
				break;
			case 2:
				base_addr = VPU_DMA_WRMIF_BADDR2;
				break;
			case 3:
				base_addr = VPU_DMA_WRMIF_BADDR3;
				break;
			}
			if (lutdma_meson_dev.support_8G_addr)
				phy_addr = info->ins[channel].rd_phy_addr[i] >> 4;
			else
				phy_addr = info->ins[channel].rd_phy_addr[i];

			lut_dma_reg_write(base_addr,
					  phy_addr);
		}
	}
	buf_size = info->ins[channel].rd_table_size[0];
	/* transfer unit:128 bits data */
	trans_size = BYTE_16_ALIGNED(buf_size) / 16;
	/* config transfer data_number */
	lut_dma_reg_set_bits(VPU_DMA_WRMIF_CTRL3,
			     trans_size, 0, 13);
}

/* use phy addr directly */
static void set_lut_dma_phyaddr_wrcfg_manual(u32 channel,
					     u32 index,
					     ulong phy_addr,
					     u32 buf_size)
{
	u32 base_addr, trans_size;
	struct lut_dma_device_info *info = &lut_dma_info;
	u32 addr, channel_index = 0;

	if (channel >= 8)
		channel_index = channel - 8;
	else
		channel_index = channel;
	if (!info->ins[channel].baddr_set) {
		base_addr = VPU_DMA_RDMIF0_BADR0 +
			(channel_index * 4) + index;
		/* set write phy addr */
		if (lutdma_meson_dev.support_8G_addr)
			addr = phy_addr >> 4;
		else
			addr = phy_addr;
		lut_dma_reg_write
			(base_addr,
			 addr);
	}
	/* transfer unit:128 bits data */
	trans_size = BYTE_16_ALIGNED(buf_size) / 16;
	base_addr = VPU_DMA_RDMIF0_CTRL + channel_index;
	/* set buf size */
	lut_dma_reg_set_bits(base_addr,
			     trans_size, 0, 12);
	/* set frame index */
	lut_dma_reg_set_bits(base_addr,
			     index, 24, 2);
}

static void set_lut_dma_phyaddr_wrcfg_auto(u32 channel,
					   u32 index,
					   ulong phy_addr,
					   u32 buf_size)
{
	u32 base_addr, trans_size;
	struct lut_dma_device_info *info = &lut_dma_info;
	u32 addr, channel_index = 0;
	int i;

	if (channel >= 8)
		channel_index = channel - 8;
	else
		channel_index = channel;
	if (!info->ins[channel].baddr_set) {
		for (i = 0; i < DMA_BUF_NUM; i++) {
			base_addr = VPU_DMA_RDMIF0_BADR0 +
				(channel_index * 4) + i;
			/* set write phy addr */
			if (lutdma_meson_dev.support_8G_addr)
				addr = phy_addr >> 4;
			else
				addr = phy_addr;
			lut_dma_reg_write(base_addr,
					  addr);
		}
	}
	/* transfer unit:128 bits data */
	trans_size = BYTE_16_ALIGNED(buf_size) / 16;
	base_addr = VPU_DMA_RDMIF0_CTRL + channel_index;
	/* set buf size */
	lut_dma_reg_set_bits(base_addr,
			     trans_size, 0, 12);
}

#ifdef TEST_LUT_DMA
static void set_lut_dma_phyaddr(u32 dma_dir, u32 channel, ulong phy_addr)
{
	int i;
	u32 base_addr, addr;
	struct lut_dma_device_info *info = &lut_dma_info;
	int channel_index = 0;

	if (lutdma_meson_dev.support_8G_addr)
		addr = phy_addr >> 4;
	else
		addr = phy_addr;
	if (dma_dir == LUT_DMA_RD) {
		channel = LUT_DMA_RD_CHAN_NUM + channel;
		for (i = 0; i < DMA_BUF_NUM; i++) {
			switch (i) {
			case 0:
				base_addr = VPU_DMA_WRMIF_BADDR0;
				break;
			case 1:
				base_addr = VPU_DMA_WRMIF_BADDR1;
				break;
			case 2:
				base_addr = VPU_DMA_WRMIF_BADDR2;
				break;
			case 3:
				base_addr = VPU_DMA_WRMIF_BADDR3;
				break;
			}
			lut_dma_reg_write(base_addr,
					  addr);
		}
		info->ins[channel].baddr_set = 1;
	} else if (dma_dir == LUT_DMA_WR) {
		for (i = 0; i < DMA_BUF_NUM; i++) {
			if (channel >= 8)
				channel_index = channel - 8;
			else
				channel_index = channel;
			base_addr = VPU_DMA_RDMIF0_BADR0 +
				(channel_index * 4) + i;
			/* set write phy addr */
			lut_dma_reg_write(base_addr,
					  addr);
		}
		info->ins[channel].baddr_set = 1;
	}
}
#endif

/* use internal malloc dma addr */
static void set_lut_dma_wrcfg_manual(u32 channel, u32 index)
{
	u32 base_addr, buf_size, trans_size;
	struct lut_dma_device_info *info = &lut_dma_info;
	u32 addr = 0, channel_index = 0;

	if (channel >= 8)
		channel_index = channel - 8;
	else
		channel_index = channel;
	if (!info->ins[channel].baddr_set) {
		base_addr = VPU_DMA_RDMIF0_BADR0 +
			(channel_index * 4) + index;
		/* set write phy addr */
		if (lutdma_meson_dev.support_8G_addr)
			addr = info->ins[channel].wr_phy_addr[index] >> 4;
		else
			addr = info->ins[channel].wr_phy_addr[index];
		lut_dma_reg_write
			(base_addr,
			 addr);
	}
	buf_size = info->ins[channel].wr_table_size[index];
	/* transfer unit:128 bits data */
	trans_size = BYTE_16_ALIGNED(buf_size) / 16;
	base_addr = VPU_DMA_RDMIF0_CTRL + channel_index;
	/* set buf size */
	lut_dma_reg_set_bits(base_addr,
			     trans_size, 0, 12);
	/* set frame index */
	lut_dma_reg_set_bits(base_addr,
			     index, 24, 2);
}

static void set_lut_dma_wrcfg_auto(u32 channel, u32 index)
{
	u32 base_addr, buf_size, trans_size;
	struct lut_dma_device_info *info = &lut_dma_info;
	u32 addr = 0, channel_index = 0;
	int i;

	if (channel >= 8)
		channel_index = channel - 8;
	else
		channel_index = channel;
	if (!info->ins[channel].baddr_set) {
		for (i = 0; i < DMA_BUF_NUM; i++) {
			base_addr = VPU_DMA_RDMIF0_BADR0 +
				(channel_index * 4) + i;
			if (lutdma_meson_dev.support_8G_addr)
				addr = info->ins[channel].wr_phy_addr[index] >> 4;
			else
				addr = info->ins[channel].wr_phy_addr[index];
			/* set write phy addr */
			lut_dma_reg_write(base_addr,
					  addr);
		}
	}
	buf_size = info->ins[channel].wr_size[index];
	/* transfer unit:128 bits data */
	trans_size = BYTE_16_ALIGNED(buf_size) / 16;
	base_addr = VPU_DMA_RDMIF0_CTRL + channel_index;
	/* set buf size */
	lut_dma_reg_set_bits(base_addr,
			     trans_size, 0, 12);
}

static void set_lut_dma_phy_addr(u32 dma_dir, u32 channel)
{
	int i;
	u32 base_addr, addr, channel_index = 0;
	struct lut_dma_device_info *info = &lut_dma_info;

	if (dma_dir == LUT_DMA_RD) {
		channel = LUT_DMA_RD_CHAN_NUM + channel;
		for (i = 0; i < DMA_BUF_NUM; i++) {
			switch (i) {
			case 0:
				base_addr = VPU_DMA_WRMIF_BADDR0;
				break;
			case 1:
				base_addr = VPU_DMA_WRMIF_BADDR1;
				break;
			case 2:
				base_addr = VPU_DMA_WRMIF_BADDR2;
				break;
			case 3:
				base_addr = VPU_DMA_WRMIF_BADDR3;
				break;
			}
			if (lutdma_meson_dev.support_8G_addr)
				addr = info->ins[channel].rd_phy_addr[i] >> 4;
			else
				addr = info->ins[channel].rd_phy_addr[i];
			lut_dma_reg_write(base_addr,
					  addr);
		}
		info->ins[channel].baddr_set = 1;
	} else if (dma_dir == LUT_DMA_WR) {
		for (i = 0; i < DMA_BUF_NUM; i++) {
			if (channel >= 8)
				channel_index = channel - 8;
			else
				channel_index = channel;
			base_addr = VPU_DMA_RDMIF0_BADR0 +
				(channel_index * 4) + i;
			if (lutdma_meson_dev.support_8G_addr)
				addr = info->ins[channel].wr_phy_addr[i] >> 4;
			else
				addr = info->ins[channel].wr_phy_addr[i];
			/* set write phy addr */
			lut_dma_reg_write(base_addr,
					  addr);
		}
		info->ins[channel].baddr_set = 1;
	}
}

static int bit_format;
MODULE_PARM_DESC(bit_format, "\n bit_format\n");
module_param(bit_format, uint, 0664);
static int lut_dma_enable(u32 dma_dir, u32 channel)
{
	int mode = 0, channel_sel = 0;
	u32 channel_index = 0;
	struct lut_dma_device_info *info = &lut_dma_info;

	if (lutdma_meson_dev.lut_dma_ver == 2) {
		if (channel >= 8)
			channel_sel = 1;
		else
			channel_sel = 0;
		lut_dma_reg_set_bits(VPU_DMA_RDMIF_SEL,
			channel_sel, 0, 1);
	}
	if (dma_dir == LUT_DMA_RD) {
		/* manul wr dma mode */
		channel = LUT_DMA_RD_CHAN_NUM + channel;
		if (channel <= 10)
			set_lut_dma_rdcfg(channel);
		/* wr_mif_enable */
		lut_dma_reg_set_bits(VPU_DMA_WRMIF_CTRL,
				     1, 13, 1);
	} else if (dma_dir == LUT_DMA_WR) {
		if (channel < LUT_DMA_WR_CHANNEL &&
		    info->ins[channel].registered &&
			!info->ins[channel].enable) {
			mode = info->ins[channel].mode;
			if (channel >= 8)
				channel_index = channel - 8;
			else
				channel_index = channel;
			if (mode == LUT_DMA_AUTO) {
				lut_dma_reg_set_bits(VPU_DMA_RDMIF0_CTRL +
						     channel_index,
						     0, 26, 1);
			} else if (mode == LUT_DMA_MANUAL) {
				lut_dma_reg_set_bits(VPU_DMA_RDMIF0_CTRL +
						     channel_index,
						     1, 26, 1);
			}
			lut_dma_reg_set_bits
				(VPU_DMA_RDMIF0_CTRL + channel_index,
				 info->ins[channel].trigger_irq_type,
				 16, 8);

			lut_dma_reg_set_bits
				(VPU_DMA_RDMIF0_CTRL + channel_index,
				 bit_format,
				 13, 2);
			info->ins[channel].enable = 1;
		}
	}
	return 0;
}

static void lut_dma_disable(u32 dma_dir, u32 channel)
{
	int mode = 0, channel_index = 0;
	struct lut_dma_device_info *info = &lut_dma_info;

	if (dma_dir == LUT_DMA_RD) {
		channel = LUT_DMA_RD_CHAN_NUM;
		/* wr_mif_disable */
		lut_dma_reg_set_bits(VPU_DMA_WRMIF_CTRL,
				     0, 13, 1);
	} else if (dma_dir == LUT_DMA_WR) {
		if (channel < LUT_DMA_WR_CHANNEL &&
		    info->ins[channel].registered) {
			if (channel >= 8)
				channel_index = channel - 8;
			else
				channel_index = channel;
			mode = info->ins[channel].mode;
			lut_dma_reg_set_bits(VPU_DMA_RDMIF0_CTRL +
					     channel_index,
					     0, 16, 8);
			info->ins[channel].enable = 0;
		}
	}
}

/* lut dma api */
/* use phy addr directly */
int lut_dma_write_phy_addr(u32 channel, ulong phy_addr, u32 size)
{
	struct lut_dma_device_info *info = &lut_dma_info;
	u32 dma_dir = LUT_DMA_WR;
	u32 index, mode = 0;
	u32 pre_size = 0;

	if (!lut_dma_probed)
		return 0;

	if (phy_addr && size > 0) {
		mode = info->ins[channel].mode;
		index = get_cur_wr_frame_index(channel);
		pre_size = info->ins[channel].wr_size[index];

		if (mode == LUT_DMA_MANUAL) {
			if (index == DMA_BUF_NUM - 1)
				index = 0;
			else
				index++;
		}
		pr_dbg("%s:mode=%d, index=%d\n", __func__, mode, index);
		info->ins[channel].wr_size[index] = size;
		if (mode == LUT_DMA_MANUAL) {
			set_lut_dma_phyaddr_wrcfg_manual(channel,
							 index,
							 phy_addr,
							 size);
			lut_dma_enable(dma_dir, channel);
		} else if (mode == LUT_DMA_AUTO) {
			if (pre_size != size) {
				/* if size changed, disable then enable */
				lut_dma_disable(dma_dir, channel);
				set_lut_dma_phyaddr_wrcfg_auto(channel,
							       index,
							       phy_addr,
							       size);
				lut_dma_enable(dma_dir, channel);
				pr_dbg("size changed: pre_size=%d, size=%d\n",
				       pre_size, size);
			} else {
				set_lut_dma_phyaddr_wrcfg_auto(channel,
							       index,
							       phy_addr,
							       size);
			}
		}
	}
	return 0;
}

void lut_dma_update_irq_source(u32 channel, u32 irq_source)
{
	struct lut_dma_device_info *info = &lut_dma_info;
	u32 channel_index = 0;

	if (!lut_dma_probed)
		return;
	pr_dbg("%s: channel=%d, irq_source=%d\n", __func__, channel, irq_source);
	info->ins[channel].trigger_irq_type = 1 << irq_source;
	if (channel >= 8)
		channel_index = channel - 8;
	else
		channel_index = channel;
	lut_dma_reg_set_bits(VPU_DMA_RDMIF0_CTRL + channel_index,
			     info->ins[channel].trigger_irq_type,
			     16, 8);
}

int lut_dma_read(u32 channel, void *paddr)
{
	struct lut_dma_device_info *info = &lut_dma_info;
	u32 index = 0, size = 0;
	struct mutex *lock = NULL;

	if (!lut_dma_probed)
		return 0;

	if (paddr) {
		channel = LUT_DMA_RD_CHAN_NUM + channel;
		lock = &info->ins[channel].lut_dma_lock;
		mutex_lock(lock);
		index = get_cur_rd_frame_index();
		size = info->ins[channel].rd_table_size[index];
		memcpy(paddr,
		       info->ins[channel].rd_table_addr[index],
		       size);
		mutex_unlock(lock);
	}
	return size;
}

int lut_dma_write_internal(u32 channel, void *paddr, u32 size)
{
	struct lut_dma_device_info *info = &lut_dma_info;
	u32 dma_dir = LUT_DMA_WR;
	u32 index, mode = 0;
	u32 pre_size = 0;
	struct mutex *lock = NULL;

	if (!lut_dma_probed)
		return 0;

	if (paddr && size > 0) {
		lock = &info->ins[channel].lut_dma_lock;
		mutex_lock(lock);
		mode = info->ins[channel].mode;
		index = get_cur_wr_frame_index(channel);
		pre_size = info->ins[channel].wr_size[index];

		if (mode == LUT_DMA_MANUAL) {
			if (index == DMA_BUF_NUM - 1)
				index = 0;
			else
				index++;
		}
		pr_dbg("%s:mode=%d, index=%d\n", __func__, mode, index);
		if (size > info->ins[channel].wr_table_size[index]) {
			pr_error("chan(%d)size(%d) larger than tb size(%d)\n",
				 channel,
				 size,
				 info->ins[channel].wr_table_size[index]);
			return -1;
		}
		memcpy(info->ins[channel].wr_table_addr[index],
		       paddr, size);
		info->ins[channel].wr_size[index] = size;
		/* need cache or not ? */
		if (mode == LUT_DMA_MANUAL) {
			set_lut_dma_wrcfg_manual(channel, index);
			lut_dma_enable(dma_dir, channel);
		} else if (mode == LUT_DMA_AUTO) {
			if (pre_size != size) {
				/* if size changed, disable then enable */
				lut_dma_disable(dma_dir, channel);
				set_lut_dma_wrcfg_auto(channel, index);
				lut_dma_enable(dma_dir, channel);
				pr_dbg("size changed: pre_size=%d, size=%d\n",
				       pre_size, size);
			}
		}
		mutex_unlock(lock);
	}
	return 0;
}

static int lut_dma_register_internal(struct lut_dma_set_t *lut_dma_set)
{
	struct lut_dma_device_info *info = &lut_dma_info;
	dma_addr_t dma_handle;
	u32 dma_dir, table_size, channel, mode;
	u32 irq_source = 0;
	struct mutex *lock = NULL;
	int i;

	if (!lut_dma_probed)
		return 0;

	dma_dir = lut_dma_set->dma_dir;
	/* transfer unit:128 bits data */
	table_size = lut_dma_set->table_size;
	if (table_size > MAX_BUF_SIZE)
		table_size = MAX_BUF_SIZE;
	channel = lut_dma_set->channel;
	mode = lut_dma_set->mode;

	if (dma_dir == LUT_DMA_RD) {
		/* only one channel for dimm statistic */
		channel = LUT_DMA_RD_CHAN_NUM;
		if (info->ins[channel].registered) {
			pr_info("already registered, channel:%d, dma_dir:%d\n",
				channel, dma_dir);
			return -1;
		}
		lock = &info->ins[channel].lut_dma_lock;
		mutex_lock(lock);

		info->ins[channel].registered = 1;
		info->ins[channel].trigger_irq_type = 0;
		info->ins[channel].mode = mode;
		info->ins[channel].enable = 0;
		for (i = 0; i < DMA_BUF_NUM; i++) {
			if (info->ins[channel].rd_table_size[i] != 0)
				continue;
			info->ins[channel].rd_table_addr[i] =
				dma_alloc_coherent
					(&info->pdev->dev, table_size,
					&dma_handle, GFP_KERNEL);
			info->ins[channel].rd_phy_addr[i] = (u32)(dma_handle);
			pr_info("%s, dma channel(%d) rd size: %d, table_addr: %p phy_addr: %lx\n",
				__func__,
				channel,
				table_size,
				info->ins[channel].rd_table_addr[i],
				info->ins[channel].rd_phy_addr[i]);
			info->ins[channel].rd_table_size[i] = table_size;
		}
		mutex_unlock(lock);
		return channel;
	} else if (dma_dir == LUT_DMA_WR) {
		/* total 8 channel */
		irq_source = lut_dma_set->irq_source;
		if (channel < LUT_DMA_WR_CHANNEL) {
			if (info->ins[channel].registered) {
				pr_dbg("already registered, channel:%d, dma_dir:%d\n",
				       channel, dma_dir);
				return -1;
			}
			lock = &info->ins[channel].lut_dma_lock;
			mutex_lock(lock);
			info->ins[channel].registered = 1;
			info->ins[channel].trigger_irq_type = (1 << irq_source);
			info->ins[channel].mode = mode;
			info->ins[channel].enable = 0;
			for (i = 0; i < DMA_BUF_NUM; i++) {
				if (info->ins[channel].wr_table_size[i] != 0)
					continue;
				info->ins[channel].wr_table_addr[i] =
					dma_alloc_coherent
						(&info->pdev->dev, table_size,
						&dma_handle, GFP_KERNEL);
				info->ins[channel].wr_phy_addr[i] =
					(u32)(dma_handle);
				pr_info("%s, dma channel(%d) wr size:%d, table_addr: %p phy_addr: %lx\n",
					__func__,
					channel,
					table_size,
					info->ins[channel].wr_table_addr[i],
					info->ins[channel].wr_phy_addr[i]);
				info->ins[channel].wr_table_size[i] =
					table_size;
				info->ins[channel].wr_size[i] = 0;
			}
			mutex_unlock(lock);
			return channel;
		}
	}
	return -1;
}

void lut_dma_unregister_internal(u32 dma_dir, u32 channel)
{
	int i;
	struct lut_dma_device_info *info = &lut_dma_info;
	struct mutex *lock = NULL;

	if (!lut_dma_probed)
		return;

	pr_dbg("%s dma_dir:%d, channel:%d\n",
	       __func__, dma_dir, channel);
	if (dma_dir == LUT_DMA_RD) {
		/* only one channel for dimm statistic */
		channel = LUT_DMA_RD_CHAN_NUM;
		if (!info->ins[channel].registered)
			return;
		lock = &info->ins[channel].lut_dma_lock;
		mutex_lock(lock);
		info->ins[channel].registered = 0;
		for (i = 0; i < DMA_BUF_NUM; i++) {
			if (info->ins[channel].rd_table_addr[i]) {
				dma_free_coherent
					(&info->pdev->dev,
					 info->ins[channel].rd_table_size[i],
					 info->ins[channel].rd_table_addr[i],
					 (dma_addr_t)
					 info->ins[channel].rd_phy_addr[i]);
				info->ins[channel].rd_table_addr[i] = NULL;
			}
			info->ins[channel].rd_table_size[i] = 0;
		}
		mutex_unlock(lock);
	} else if (dma_dir == LUT_DMA_WR) {
		if (channel < LUT_DMA_WR_CHANNEL &&
		    info->ins[channel].registered) {
			struct lut_dma_ins *ins;

			lock = &info->ins[channel].lut_dma_lock;
			mutex_lock(lock);
			ins = &info->ins[channel];
			ins->registered = 0;
			for (i = 0; i < DMA_BUF_NUM; i++) {
				if (info->ins[channel].wr_table_addr[i]) {
					dma_free_coherent
						(&info->pdev->dev,
						 ins->wr_table_size[i],
						 ins->wr_table_addr[i],
						 (dma_addr_t)
						 ins->wr_phy_addr[i]);
					ins->wr_table_addr[i] = NULL;
				}
				ins->wr_table_size[i] = 0;
			}
			mutex_unlock(lock);
		}
	}
}

int lut_dma_register(struct lut_dma_set_t *lut_dma_set)
{
	struct lut_dma_device_info *info = &lut_dma_info;
	u32 dma_dir, table_size, channel, mode;
	u32 irq_source = 0;
	struct mutex *lock = NULL;
	int i;

	if (!lut_dma_probed)
		return -1;
	dma_dir = lut_dma_set->dma_dir;
	/* transfer unit:128 bits data */
	table_size = lut_dma_set->table_size;
	if (table_size > MAX_BUF_SIZE)
		table_size = MAX_BUF_SIZE;
	channel = lut_dma_set->channel;
	mode = lut_dma_set->mode;

	if (dma_dir == LUT_DMA_RD) {
		/* only one channel for dimm statistic */
		channel = LUT_DMA_RD_CHAN_NUM;
		if (info->ins[channel].registered) {
			pr_info("already registered, channel:%d, dma_dir:%d\n",
				channel, dma_dir);
			return -1;
		}
		lock = &info->ins[channel].lut_dma_lock;
		mutex_lock(lock);

		info->ins[channel].registered = 1;
		info->ins[channel].trigger_irq_type = 0;
		info->ins[channel].mode = mode;
		info->ins[channel].enable = 0;
		for (i = 0; i < DMA_BUF_NUM; i++)
			info->ins[channel].rd_table_size[i] = table_size;

		mutex_unlock(lock);
		return channel;
	} else if (dma_dir == LUT_DMA_WR) {
		/* total 8 channel */
		irq_source = lut_dma_set->irq_source;
		if (channel < LUT_DMA_WR_CHANNEL) {
			if (info->ins[channel].registered) {
				pr_dbg("already registered, channel:%d, dma_dir:%d\n",
				       channel, dma_dir);
				return -1;
			}
			lock = &info->ins[channel].lut_dma_lock;
			mutex_lock(lock);
			info->ins[channel].registered = 1;
			info->ins[channel].trigger_irq_type = (1 << irq_source);
			info->ins[channel].mode = mode;
			info->ins[channel].enable = 0;
			for (i = 0; i < DMA_BUF_NUM; i++) {
				info->ins[channel].wr_table_size[i] =
					table_size;
				info->ins[channel].wr_size[i] = 0;
			}

			mutex_unlock(lock);
			return channel;
		}
	}
	return -1;
}
EXPORT_SYMBOL(lut_dma_register);

void lut_dma_unregister(u32 dma_dir, u32 channel)
{
	int i;
	struct lut_dma_device_info *info = &lut_dma_info;
	struct mutex *lock = NULL;

	if (!lut_dma_probed)
		return;
	pr_dbg("%s dma_dir:%d, channel:%d\n",
	       __func__, dma_dir, channel);
	if (dma_dir == LUT_DMA_RD) {
		/* only one channel for dimm statistic */
		channel = LUT_DMA_RD_CHAN_NUM;
		if (!info->ins[channel].registered)
			return;
		lock = &info->ins[channel].lut_dma_lock;
		mutex_lock(lock);
		info->ins[channel].registered = 0;
		for (i = 0; i < DMA_BUF_NUM; i++)
			info->ins[channel].rd_table_size[i] = 0;
		mutex_unlock(lock);
	} else if (dma_dir == LUT_DMA_WR) {
		if (channel < LUT_DMA_WR_CHANNEL &&
		    info->ins[channel].registered) {
			struct lut_dma_ins *ins;

			lock = &info->ins[channel].lut_dma_lock;
			mutex_lock(lock);
			ins = &info->ins[channel];
			ins->registered = 0;
			for (i = 0; i < DMA_BUF_NUM; i++)
				ins->wr_table_size[i] = 0;
			mutex_unlock(lock);
		}
	}
}
EXPORT_SYMBOL(lut_dma_unregister);

static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	if (!token)
		return 0;
	len = strlen(token);
	do {
		token = strsep(&params, " ");
		while (token && (isspace(*token) ||
				 !isgraph(*token)) && len) {
			token++;
			len--;
		}
		if (len == 0 || !token)
			break;
		ret = kstrtoint(token, 0, &res);
		if (ret < 0)
			break;
		len = strlen(token);
		*out++ = res;
		count++;
	} while ((token) && (count < para_num) && (len > 0));

	kfree(params_base);
	return count;
}

static ssize_t lut_dma_loglevel_show(struct class *cla,
				     struct class_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%x\n", log_level);
}

static ssize_t lut_dma_loglevel_stroe(struct class *cla,
				      struct class_attribute *attr,
				      const char *buf, size_t count)
{
	int ret = 0;

	ret = kstrtoint(buf, 0, &log_level);
	if (ret < 0)
		return -EINVAL;
	return count;
}

static ssize_t lut_dma_test_show(struct class *cla,
				 struct class_attribute *attr, char *buf)
{
	pr_info("test mode - 1: WR MANUAL; 2: WR AUTO; 3:RD AUTO\n");
	return snprintf(buf, PAGE_SIZE, "mode:%d, channel:%d\n",
		lut_dma_test_mode,
		lut_dma_test_channel);
}

static ssize_t lut_dma_test_stroe(struct class *cla,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	char *table_data = NULL;
	int table_size = 1024;
	int parsed[2];

	if (likely(parse_para(buf, 2, parsed) != 2))
		return -EINVAL;
	lut_dma_test_mode = parsed[0];
	lut_dma_test_channel = parsed[1];

	table_data = kmalloc(table_size,
			     GFP_KERNEL);
	if (!table_data) {
		pr_error("kmalloc failed\n");
		return -ENOMEM;
	}
	switch (lut_dma_test_mode) {
	case 1:
	case 2:
		/* do read test */
		lut_dma_write_internal(lut_dma_test_channel,
				       table_data,
				       table_size);
		break;
	case 3:
		/* do read test */
		lut_dma_enable(LUT_DMA_RD,
			       lut_dma_test_channel);
		table_size = lut_dma_read(0, table_data);
		break;
	}
	kfree(table_data);
	return count;
}

static ssize_t lut_dma_register_show(struct class *cla,
				     struct class_attribute *attr, char *buf)
{
	pr_info("register mode - 1: WR MANUAL; 2: WR AUTO; 3:RD AUTO\n");
	return snprintf(buf, PAGE_SIZE, "mode:%d, channel:%d\n",
		lut_dma_test_mode,
		lut_dma_test_channel);
}

static ssize_t lut_dma_register_stroe(struct class *cla,
				      struct class_attribute *attr,
				      const char *buf, size_t count)
{
	struct lut_dma_device_info *info = &lut_dma_info;
	struct lut_dma_set_t lut_dma_set;
	int table_size = 1024;
	int ret = -1;
	int parsed[2];

	if (likely(parse_para(buf, 2, parsed) != 2))
		return -EINVAL;
	lut_dma_test_mode = parsed[0];
	lut_dma_test_channel = parsed[1];

	switch (lut_dma_test_mode) {
	case 1:
		/* do write test*/
		lut_dma_set.channel = lut_dma_test_channel;
		lut_dma_set.dma_dir = LUT_DMA_WR;
		lut_dma_set.irq_source = VIU1_VSYNC;
		lut_dma_set.mode = LUT_DMA_MANUAL;
		lut_dma_set.table_size = table_size;
		ret = lut_dma_register_internal(&lut_dma_set);
		if (ret < 0 &&
		    info->ins[lut_dma_set.channel].mode !=
		     LUT_DMA_MANUAL) {
			return count;
		}
		set_lut_dma_phy_addr(LUT_DMA_WR,
				     lut_dma_set.channel);
		break;
	case 2:
		/* do read test */
		lut_dma_set.channel = lut_dma_test_channel;
		lut_dma_set.dma_dir = LUT_DMA_WR;
		lut_dma_set.irq_source = VIU1_VSYNC;
		lut_dma_set.mode = LUT_DMA_AUTO;
		lut_dma_set.table_size = table_size;
		ret = lut_dma_register_internal(&lut_dma_set);
		if (ret < 0 &&
		    info->ins[lut_dma_set.channel].mode !=
		     LUT_DMA_AUTO) {
			return count;
		}
		set_lut_dma_phy_addr(LUT_DMA_WR,
				     lut_dma_set.channel);
		break;
	case 3:
		/* do read test */
		lut_dma_set.channel = lut_dma_test_channel;
		lut_dma_set.dma_dir = LUT_DMA_RD;
		lut_dma_set.table_size = table_size;
		ret = lut_dma_register_internal(&lut_dma_set);
		if (ret >= 0)
			set_lut_dma_phy_addr(LUT_DMA_RD,
					     lut_dma_set.channel);
		break;
	}
	return count;
}

static ssize_t lut_dma_unregister_show(struct class *cla,
				       struct class_attribute *attr, char *buf)
{
	pr_info("unregister mode - 1: WR MANUAL; 2: WR AUTO; 3:RD AUTO\n");
	return snprintf(buf, PAGE_SIZE, "mode:%d, channel:%d\n",
		lut_dma_test_mode,
		lut_dma_test_channel);
}

static ssize_t lut_dma_unregister_stroe(struct class *cla,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	int parsed[2];

	if (likely(parse_para(buf, 2, parsed) != 2))
		return -EINVAL;
	lut_dma_test_mode = parsed[0];
	lut_dma_test_channel = parsed[1];
	switch (lut_dma_test_mode) {
	case 1:
	case 2:
		lut_dma_unregister_internal(LUT_DMA_WR,
					    lut_dma_test_channel);
		break;
	case 3:
		lut_dma_unregister_internal(LUT_DMA_WR,
					    lut_dma_test_channel);
		break;
	}
	return count;
}

static struct class_attribute lut_dma_attrs[] = {
	__ATTR(level, 0664,
	       lut_dma_loglevel_show, lut_dma_loglevel_stroe),
	__ATTR(test, 0664,
	       lut_dma_test_show, lut_dma_test_stroe),
	__ATTR(register, 0664,
	       lut_dma_register_show, lut_dma_register_stroe),
	__ATTR(unregister, 0664,
	       lut_dma_unregister_show, lut_dma_unregister_stroe),
};

static int lut_dma_probe(struct platform_device *pdev)
{
	int i, ret = 0;
	struct lut_dma_device_info *info = &lut_dma_info;

	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		struct lutdma_device_data_s *lutdma_meson;
		struct device_node	*of_node = pdev->dev.of_node;

		match = of_match_node(lut_dma_dt_match, of_node);
		if (match) {
			lutdma_meson =
				(struct lutdma_device_data_s *)match->data;
			if (lutdma_meson) {
				memcpy(&lutdma_meson_dev, lutdma_meson,
				       sizeof(struct lutdma_device_data_s));
			} else {
				pr_err("%s data NOT match\n", __func__);
				return -ENODEV;
			}

		} else {
			pr_err("%s NOT match\n", __func__);
			return -ENODEV;
		}
	} else {
		pr_err("dev %s NOT found\n", __func__);
		return -ENODEV;
	}
	info->pdev = pdev;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
		vpu_dma = vpu_dev_register(VPU_DMA, "VPU_DMA");
		vpu_dev_mem_power_on(vpu_dma);
	}

	info->clsp = class_create(THIS_MODULE,
				  CLASS_NAME);
	if (IS_ERR(info->clsp)) {
		ret = PTR_ERR(info->clsp);
		pr_err("fail to create class\n");
		goto fail_create_class;
	}
	for (i = 0; i < ARRAY_SIZE(lut_dma_attrs); i++) {
		if (class_create_file
			(info->clsp,
			&lut_dma_attrs[i]) < 0) {
			pr_err("fail to class_create_file\n");
			goto fail_class_create_file;
		}
	}
	for (i = 0; i < LUT_DMA_CHANNEL; i++)
		mutex_init(&info->ins[i].lut_dma_lock);
	lut_dma_probed = 1;
	if (lutdma_is_meson_sc2_cpu() ||
	    lutdma_is_meson_t7_cpu() ||
	    lutdma_is_meson_s5_cpu())
		lut_dma_reg_set_bits(VPU_DMA_RDMIF_CTRL2,
				     1, 29, 1);
	return 0;
fail_class_create_file:
	for (i = 0; i < ARRAY_SIZE(lut_dma_attrs); i++)
		class_remove_file
			(info->clsp, &lut_dma_attrs[i]);
	class_destroy(info->clsp);
	info->clsp = NULL;
fail_create_class:
	return ret;
}

#ifdef CONFIG_PM
static int lut_dma_suspend(struct platform_device *dev, pm_message_t state)
{
	struct lut_dma_device_info *info = &lut_dma_info;
	int i;

	for (i = 0; i < LUT_DMA_CHANNEL; i++)
		if (info->ins[i].registered) {
			if (info->ins[i].enable) {
				lut_dma_disable(info->ins[i].dir, i);
				info->ins[i].force_disable = true;
			}
		}
	return 0;
}

static int lut_dma_resume(struct platform_device *dev)
{
	struct lut_dma_device_info *info = &lut_dma_info;
	int i;

	for (i = 0; i < LUT_DMA_CHANNEL; i++)
		if (info->ins[i].registered) {
			if (!info->ins[i].enable && info->ins[i].force_disable)
				lut_dma_enable(info->ins[i].dir, i);
		}
	return 0;
}
#endif

/* static int __devexit rdma_remove(struct platform_device *pdev) */
static int lut_dma_remove(struct platform_device *pdev)
{
	int i;
	struct lut_dma_device_info *info = &lut_dma_info;

	for (i = 0; i < ARRAY_SIZE(lut_dma_attrs); i++)
		class_remove_file
			(info->clsp, &lut_dma_attrs[i]);
	class_destroy(info->clsp);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
		vpu_dev_mem_power_down(vpu_dma);
	info->clsp = NULL;
	lut_dma_probed = 0;
	return 0;
}

static struct platform_driver lut_dma_driver = {
	.probe = lut_dma_probe,
	.remove = lut_dma_remove,
#ifdef CONFIG_PM
	.suspend  = lut_dma_suspend,
	.resume    = lut_dma_resume,
#endif
	.driver = {
		.name = "amlogic_lut_dma",
		.of_match_table = lut_dma_dt_match,
	},
};

int __init lut_dma_init(void)
{
	int r;

	r = platform_driver_register(&lut_dma_driver);
	if (r) {
		pr_error("Unable to register lut dma driver\n");
		return r;
	}

	return 0;
}

void __exit lut_dma_exit(void)
{
	platform_driver_unregister(&lut_dma_driver);
}

//MODULE_DESCRIPTION("AMLOGIC LUT DMA management driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("PengCheng.Chen <pengcheng.chen@amlogic.com>");
