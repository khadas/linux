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
#include <linux/reset.h>
#include <linux/sched/clock.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_irq.h>

#include <linux/amlogic/media/frc/frc_reg.h>

#include "frc_drv.h"
#include "frc_rdma.h"
#include "frc_hw.h"
// #include "frc_regs_table.h"

struct reg_test regs_table_test[REG_TEST_NUM] = {
	{0x60, 0x0}, {0x61, 0x1}, {0x62, 0x2}, {0x63, 0x3},
	{0x64, 0x4}, {0x65, 0x5}, {0x66, 0x6}, {0x67, 0x7},
	{0x68, 0x8}, {0x69, 0x9}, {0x6a, 0xa}, {0x6b, 0xb},
	{0x6c, 0xc}, {0x6d, 0xd}, {0x6e, 0xe}, {0x6f, 0xf},
	{0x3210, 0x10}, {0x3211, 0x11}, {0x3212, 0x12}, {0x3213, 0x13},
	{0x3214, 0x14}, {0x3215, 0x15}, {0x3216, 0x16}, {0x3217, 0x17},
	{0x3218, 0x18}, {0x3219, 0x19}, {0x321a, 0x1a}, {0x321b, 0x1b},
	{0x321c, 0x1c}, {0x321d, 0x1d}, {0x321e, 0x1e}, {0x321f, 0x1f},
};

static struct rdma_regadr_s rdma_regadr_t3[RDMA_NUM] = {
	{
		FRC_RDMA_AHB_START_ADDR_MAN,
		FRC_RDMA_AHB_START_ADDR_MAN_MSB,
		FRC_RDMA_AHB_END_ADDR_MAN,
		FRC_RDMA_AHB_END_ADDR_MAN_MSB,
		0, 0,
		FRC_RDMA_ACCESS_MAN, 1,
		FRC_RDMA_ACCESS_MAN, 2,
		24, 24
	},
	{
		FRC_RDMA_AHB_START_ADDR_1,
		FRC_RDMA_AHB_START_ADDR_1_MSB,
		FRC_RDMA_AHB_END_ADDR_1,
		FRC_RDMA_AHB_END_ADDR_1_MSB,
		FRC_RDMA_AUTO_SRC1_SEL,  1,
		FRC_RDMA_ACCESS_AUTO,  1,
		FRC_RDMA_ACCESS_AUTO,  5,
		25, 25
	},
	{
		FRC_RDMA_AHB_START_ADDR_2,
		FRC_RDMA_AHB_START_ADDR_2_MSB,
		FRC_RDMA_AHB_END_ADDR_2,
		FRC_RDMA_AHB_END_ADDR_2_MSB,
		FRC_RDMA_AUTO_SRC2_SEL,  0,
		FRC_RDMA_ACCESS_AUTO,  2,
		FRC_RDMA_ACCESS_AUTO,  6,
		26, 26
	},
	{
		FRC_RDMA_AHB_START_ADDR_3,
		FRC_RDMA_AHB_START_ADDR_3_MSB,
		FRC_RDMA_AHB_END_ADDR_3,
		FRC_RDMA_AHB_END_ADDR_3_MSB,
		FRC_RDMA_AUTO_SRC3_SEL,  0,
		FRC_RDMA_ACCESS_AUTO,  3,
		FRC_RDMA_ACCESS_AUTO,  7,
		27, 27
	},
	{
		FRC_RDMA_AHB_START_ADDR_4,
		FRC_RDMA_AHB_START_ADDR_4_MSB,
		FRC_RDMA_AHB_END_ADDR_4,
		FRC_RDMA_AHB_END_ADDR_4_MSB,
		FRC_RDMA_AUTO_SRC4_SEL, 0,
		FRC_RDMA_ACCESS_AUTO2, 0,
		FRC_RDMA_ACCESS_AUTO2, 4,
		28, 28
	},
	{
		FRC_RDMA_AHB_START_ADDR_5,
		FRC_RDMA_AHB_START_ADDR_5_MSB,
		FRC_RDMA_AHB_END_ADDR_5,
		FRC_RDMA_AHB_END_ADDR_5_MSB,
		FRC_RDMA_AUTO_SRC5_SEL, 0,
		FRC_RDMA_ACCESS_AUTO2, 1,
		FRC_RDMA_ACCESS_AUTO2, 5,
		29, 29
	},
	{
		FRC_RDMA_AHB_START_ADDR_6,
		FRC_RDMA_AHB_START_ADDR_6_MSB,
		FRC_RDMA_AHB_END_ADDR_6,
		FRC_RDMA_AHB_END_ADDR_6_MSB,
		FRC_RDMA_AUTO_SRC6_SEL, 0,
		FRC_RDMA_ACCESS_AUTO2, 2,
		FRC_RDMA_ACCESS_AUTO2, 6,
		30, 30
	},
	{
		FRC_RDMA_AHB_START_ADDR_7,
		FRC_RDMA_AHB_START_ADDR_7_MSB,
		FRC_RDMA_AHB_END_ADDR_7,
		FRC_RDMA_AHB_END_ADDR_7_MSB,
		FRC_RDMA_AUTO_SRC7_SEL, 0,
		FRC_RDMA_ACCESS_AUTO2, 3,
		FRC_RDMA_ACCESS_AUTO2, 7,
		31, 31
	}
};

static int rdma_cnt;

int frc_rdma_debug;
module_param(frc_rdma_debug, int, 0664);
MODULE_PARM_DESC(frc_rdma_debug, "frc rdma debug");

int frc_rdma_enable;
module_param(frc_rdma_enable, int, 0664);
MODULE_PARM_DESC(frc_rdma_enable, "frc rdma enable ctrl");

struct frc_rdma_info frc_rdma_s;

struct frc_rdma_info frc_rdma_s2;

struct frc_rdma_info *frc_get_rdma_info(void)
{
	return &frc_rdma_s;
}
EXPORT_SYMBOL(frc_get_rdma_info);

int is_rdma_enable(void)
{
	struct frc_dev_s *devp;
	struct frc_fw_data_s *fw_data;

	devp = get_frc_devp();
	fw_data = (struct frc_fw_data_s *)devp->fw_data;

	if (frc_rdma_enable && fw_data->frc_top_type.rdma_en)
		return 1;
	return 0;
}

void frc_rdma_write_test(dma_addr_t phy_addr, u32 size)
{
	int i;

	// data32  = 0;
	// data32 |= 1 << 10;
	// data32 |= 1 << 8;
	// data32 |= 1 << 7; /* write ddr urgent */
	// data32 |= 1 << 6; /* read ddr urgent */
	// data32 |= 1 << 4;
	// data32 |= 1 << 2;
	// data32 |= 0 << 1;
	// data32 |= 0 << 0;

	// WRITE_FRC_REG_BY_CPU(FRC_RDMA_CTRL, 0x5d4); //ctrl sw reset
	//
	// timestamp = sched_clock();
	WRITE_FRC_REG_BY_CPU(FRC_RDMA_AHB_START_ADDR_MAN, phy_addr); //rdma start address
	WRITE_FRC_REG_BY_CPU(FRC_RDMA_AHB_END_ADDR_MAN,   phy_addr + size);

	//WRITE_FRC_REG_BY_CPU(FRC_RDMA_ACCESS_MAN, 0x4); //man rdma start to work
	// WRITE_FRC_REG_BY_CPU (FRC_RDMA_ACCESS_MAN, 0x101);
	//WRITE_FRC_BITS(FRC_RDMA_ACCESS_MAN, 1, 0, 1);
	// WRITE_FRC_REG_BY_CPU(FRC_RDMA_CTRL, 0x1 << 24);

	for (i = 0x3b00; i < 0x3b3f; ++i)
		pr_frc(0, "addr[%x]=%x\n", i, READ_FRC_REG(i));
}

void frc_rdma_alloc_buf(void)
{
	dma_addr_t dma_handle;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_rdma_info *frc_rdma = frc_get_rdma_info();
	struct frc_rdma_info *frc_rdma2 = &frc_rdma_s2;

	frc_rdma->rdma_table_size = FRC_RDMA_SIZE;

	frc_rdma->rdma_table_addr = dma_alloc_coherent(&devp->pdev->dev,
		FRC_RDMA_SIZE / 2, &dma_handle, GFP_KERNEL);
	frc_rdma->rdma_table_phy_addr = (ulong)(dma_handle);

	frc_rdma2->rdma_table_addr = dma_alloc_coherent(&devp->pdev->dev,
		FRC_RDMA_SIZE / 2, &dma_handle, GFP_KERNEL);
	frc_rdma2->rdma_table_phy_addr = (ulong)(dma_handle);

	pr_frc(0, "%s rdma_table_addr: %lx phy:%lx size:%lx\n",
		__func__, (unsigned long)frc_rdma->rdma_table_addr,
		frc_rdma->rdma_table_phy_addr, FRC_RDMA_SIZE / 2);
	pr_frc(0, "%s rdma_table_addr2: %lx phy2:%lx size2:%lx\n",
		__func__, (unsigned long)frc_rdma2->rdma_table_addr,
		frc_rdma2->rdma_table_phy_addr, FRC_RDMA_SIZE / 2);

	if (frc_rdma->rdma_table_addr)
		frc_rdma->buf_status = 1;
	if (frc_rdma2->rdma_table_addr)
		frc_rdma2->buf_status = 1;
}

void frc_rdma_release_buf(void)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_rdma_info *frc_rdma = frc_get_rdma_info();

	dma_free_coherent(&devp->pdev->dev, frc_rdma->rdma_table_size,
		frc_rdma->rdma_table_addr, (dma_addr_t)frc_rdma->rdma_table_phy_addr);
}

void frc_rdma_reg_status(void)
{
	int i;

	for (i = 0x3b00; i < 0x3b3f; ++i)
		pr_frc(0, "addr[%x]=%x\n", i, READ_FRC_REG(i));
}

void frc_rdma_read_test_reg(void)
{
	int i;

	for (i = 0x0; i < 0x10; i++)
		pr_frc(0, "addr[%x]:%x\n", i + 0x60, READ_FRC_REG(i + 0x60));

	for (i = 0x0; i < 0x10; i++)
		pr_frc(0, "addr[%x]:%x\n", i + 0x3210, READ_FRC_REG(i + 0x3210));
}

void frc_rdma_reset_test_reg(void)
{
	int i, sum = 0;

	for (i = 0x0; i < 0x10; i++)
		WRITE_FRC_REG_BY_CPU(i + 0x60, 0);

	for (i = 0x0; i < 0x10; i++)
		sum += READ_FRC_REG(i + 0x60);

	if (sum == 0)
		pr_frc(0, "reset test reg pass\n");
	else
		pr_frc(0, "reset failed\n");
}

void frc_read_table(void)
{
	int i, temp, temp2;
	struct frc_rdma_info *frc_rdma = &frc_rdma_s;
	struct frc_rdma_info *frc_rdma2 = &frc_rdma_s2;

	temp = frc_rdma->rdma_item_count;
	temp2 = frc_rdma2->rdma_item_count;

	pr_frc(0, "--------------table1----------------\n");
	for (i = 0; i < temp; i++) {
		// if (frc_rdma->rdma_table_addr[i * 2] != 0)
		pr_frc(0, "addr:0x%04x, value:0x%08x\n",
			frc_rdma->rdma_table_addr[i * 2],
			frc_rdma->rdma_table_addr[i * 2 + 1]);
		// else
			// break;
	}
	pr_frc(0, "--------------table2----------------\n");
	for (i = 0; i < temp2; i++) {
		// if (frc_rdma->rdma_table_addr[i * 2] != 0)
		pr_frc(0, "addr:0x%04x, value:0x%08x\n",
			frc_rdma2->rdma_table_addr[i * 2],
			frc_rdma2->rdma_table_addr[i * 2 + 1]);
		// else
			// break;
	}
	pr_frc(0, "--------------table2 done----------------\n");
}

void frc_read_table2_clear(void)
{
	int i, count;
	struct frc_rdma_info *frc_rdma2 = &frc_rdma_s2;

	// memset(frc_rdma2->rdma_table_addr[0], 0,
	// sizeof(frc_rdma2->rdma_table_addr[0]) * frc_rdma2->rdma_item_count);

	count = frc_rdma2->rdma_item_count;

	for (i = 0; i < count; i++) {
		frc_rdma2->rdma_table_addr[i * 2] = 0;
		frc_rdma2->rdma_table_addr[i * 2 + 1] = 0;
	}

	frc_rdma2->rdma_item_count = 0;

	pr_frc(0, "clear table2 done\n");
}

int frc_check_table(u32 addr)
{
	int i;
	int index = -1;
	struct frc_rdma_info *frc_rdma = &frc_rdma_s;

	if (frc_rdma->rdma_item_count == 0)
		return -1;

	pr_frc(21, "rdma_item_count:%d addr:%x\n",
		frc_rdma->rdma_item_count, addr);

	for (i = (frc_rdma->rdma_item_count - 1) * 2; i >= 0; i -= 2) {
		pr_frc(21, "i:%d, table_addr[%d]=%x, table_value[%d]=%x",
			i, i, frc_rdma->rdma_table_addr[i], i + 1,
			frc_rdma->rdma_table_addr[i + 1]);
		if (frc_rdma->rdma_table_addr[i] == addr) {
			//value = frc_rdma->rdma_table_addr[i+1];
			index = i / 2 + 1;
			break;
		}
	}

	pr_frc(21, "%s index:%d\n", __func__, index);
	return index;
}

/*
 * interrupt call
 */
int frc_rdma_config(int handle, u32 trigger_type)
{
	// int i;
	int flag = 30;
	// u32 temp, count = 0;
	struct frc_rdma_info *frc_rdma = &frc_rdma_s;

	if (frc_rdma->rdma_item_count > 0 && handle == 0) {
		// manual RDMA
		struct rdma_regadr_s *man_ins = &rdma_regadr_t3[0];

		WRITE_FRC_REG_BY_CPU(FRC_RDMA_ACCESS_MAN,
			READ_FRC_REG(FRC_RDMA_ACCESS_MAN) & (~1));

		WRITE_FRC_REG_BY_CPU(man_ins->rdma_ahb_start_addr,
			frc_rdma->rdma_table_phy_addr & 0xffffffff);
		WRITE_FRC_REG_BY_CPU(man_ins->rdma_ahb_end_addr,
			(frc_rdma->rdma_table_phy_addr & 0xffffffff) +
			frc_rdma->rdma_item_count * 8 - 1);

		WRITE_FRC_BITS(man_ins->addr_inc_reg, 0,
			man_ins->addr_inc_reg_bitpos, 1);
		WRITE_FRC_BITS(man_ins->rw_flag_reg, 1,   //1:write
			man_ins->rw_flag_reg_bitpos, 1);

		WRITE_FRC_REG_BY_CPU(FRC_RDMA_ACCESS_MAN,
			READ_FRC_REG(FRC_RDMA_ACCESS_MAN) | 1);

		//rdma_status = READ_FRC_REG(FRC_RDMA_STATUS);
		pr_frc(flag, "config manual write done\n");
		// frc_rdma->rdma_item_count = 0;

	} else if (frc_rdma->rdma_item_count > 0 && handle == 1) {
		//auto RDMA
		struct rdma_regadr_s *man_ins = &rdma_regadr_t3[1];

		WRITE_FRC_BITS(man_ins->trigger_mask_reg, 0,
			man_ins->trigger_mask_reg_bitpos, 1);

		WRITE_FRC_REG_BY_CPU(man_ins->rdma_ahb_start_addr,
			frc_rdma->rdma_table_phy_addr & 0xffffffff);
		WRITE_FRC_REG_BY_CPU(man_ins->rdma_ahb_end_addr,
			(frc_rdma->rdma_table_phy_addr & 0xffffffff) +
			frc_rdma->rdma_item_count * 8 - 1);

		WRITE_FRC_BITS(man_ins->addr_inc_reg, 0,
			man_ins->addr_inc_reg_bitpos, 1);
		WRITE_FRC_BITS(man_ins->rw_flag_reg, 1,  // 1:write
			man_ins->rw_flag_reg_bitpos, 1);

		WRITE_FRC_BITS(man_ins->trigger_mask_reg, handle,
			man_ins->trigger_mask_reg_bitpos, 1);

		pr_frc(flag, "config auto write done\n");
	} else if (frc_rdma->rdma_item_count > 0 && handle == 2) {
		//auto2 RDMA
		struct rdma_regadr_s *man_ins = &rdma_regadr_t3[2];

		WRITE_FRC_BITS(man_ins->trigger_mask_reg, 0,
			man_ins->trigger_mask_reg_bitpos, 1);

		//
		WRITE_FRC_REG_BY_CPU(man_ins->rdma_ahb_start_addr,
			frc_rdma->rdma_table_phy_addr & 0xffffffff);
		WRITE_FRC_REG_BY_CPU(man_ins->rdma_ahb_end_addr,
			(frc_rdma->rdma_table_phy_addr & 0xffffffff) +
			frc_rdma->rdma_item_count * 8 - 1);

		WRITE_FRC_BITS(man_ins->addr_inc_reg, 0,
			man_ins->addr_inc_reg_bitpos, 1);
		WRITE_FRC_BITS(man_ins->rw_flag_reg, 1,  // 1:write
			man_ins->rw_flag_reg_bitpos, 1);

		WRITE_FRC_BITS(man_ins->trigger_mask_reg, 1,
			man_ins->trigger_mask_reg_bitpos, 1);

		pr_frc(flag, "config auto2 write done\n");
	}
	// memset(frc_rdma->rdma_table_addr, 0,
	//     sizeof(u32) * frc_rdma->rdma_item_count);
	pr_frc(flag, "write rdma_item_count: %d\n", frc_rdma->rdma_item_count);

	frc_rdma->rdma_item_count = 0;
	// rdma_cnt = 0;
	// pr_frc(flag, "rdma_cnt:%d, count:%d\n", rdma_cnt, count);
	return 0;
}

void frc_rdma_table_config(u32 addr, u32 val)
{
	int i;
	struct frc_rdma_info *frc_rdma = &frc_rdma_s;
	// struct frc_rdma_info *frc_rdma2 = &frc_rdma_s2;

	i = frc_rdma->rdma_item_count;
	pr_frc(8, "i:%d, addr:%x, val:%x\n", i, addr, val);

	if ((i + 1) * 8 > frc_rdma->rdma_table_size) {
		pr_frc(0, "frc rdma buffer overflow\n");
		return;
	}

	frc_rdma->rdma_table_addr[i * 2] = addr & 0xffffffff;
	frc_rdma->rdma_table_addr[i * 2 + 1] = val & 0xffffffff;

	// frc_rdma2->rdma_table_addr[frc_rdma2->rdma_item_count * 2] = addr & 0xffffffff;
	// frc_rdma2->rdma_table_addr[frc_rdma2->rdma_item_count * 2 + 1] = val & 0xffffffff;

	frc_rdma->rdma_item_count++;
	// frc_rdma2->rdma_item_count++;
	pr_frc(8, "addr:%04x, value:%08x\n",
		frc_rdma->rdma_table_addr[i * 2], frc_rdma->rdma_table_addr[i * 2 + 1]);
	rdma_cnt++;
}

int frc_rdma_write_reg(u32 addr, u32 val)
{
	// int ret;
	int flag = 31;
	// u32 write_val;

	// ret = frc_check_table(addr);
	// if (ret == -1)
	// write_val = readl(frc_base + (addr << 2));
	// else
	// write_val = ret;

	pr_frc(flag, "addr:0x%x write_val:0x%x\n", addr, val);
	frc_rdma_table_config(addr, val);

	return 0;
}

int frc_rdma_write_bits(u32 addr, u32 val, u32 start, u32 len)
{
	int ret;
	int flag = 31;
	u32 read_val, write_val, mask;

	// read_val = READ_FRC_REG(addr);
	ret = frc_check_table(addr);
	if (ret == -1)
		read_val = readl(frc_base + (addr << 2));
	else
		read_val = ret;

	mask = (((1L << len) - 1) << start);
	write_val  = read_val & ~mask;
	write_val |= (val << start) & mask;

	pr_frc(flag, "addr:0x%x read_val:0x%x write_val:0x%x\n",
	addr, read_val, write_val);
	frc_rdma_table_config(addr, write_val);

	return 0;
}

int frc_rdma_update_reg_bits(u32 addr, u32 val, u32 mask)
{
	int flag = 31;
	// int index = -1;
	u32 read_val, write_val;
	// struct frc_rdma_info *frc_rdma = &frc_rdma_s;

	/*check rdma_table reg value*/
	if (mask == 0xFFFFFFFF) {
		// WRITE_FRC_REG_BY_CPU(addr, val);
		frc_rdma_table_config(addr, val);
	} else {
		// index = frc_check_table(addr);
		// if (index == -1) {
		read_val = readl(frc_base + (addr << 2));
		val &= mask;
		write_val = read_val & ~mask;
		write_val |= val;
		pr_frc(flag, "addr:0x%04x read_val:0x%x write_val:0x%x\n",
			addr, read_val, write_val);
		frc_rdma_table_config(addr, write_val);
		// } else {
		// read_val = (u32)ret;
		// read_val = frc_rdma->rdma_table_addr[(index - 1) * 2 + 1];
		// val &= mask;
		// write_val = read_val & ~mask;
		// write_val |= val;
		// frc_rdma->rdma_table_addr[(index - 1) * 2 + 1] = write_val & 0xffffffff;
		// pr_frc(flag, "addr:0x%04x re-addr0x%x read_val:0x%x write_val:0x%x\n",
				// addr, frc_rdma->rdma_table_addr[(index - 1) * 2],
				// read_val, write_val);
	}
	// val &= mask;
	// write_val = read_val & ~mask;
	// write_val |= val;

	// pr_frc(flag, "addr:0x%04x read_val:0x%x write_val:0x%x\n",
	//     addr, read_val, write_val);
	// frc_rdma_table_config(addr, write_val);

	return 0;
}

int FRC_RDMA_VSYNC_WR_REG(u32 addr, u32 val)
{
	int flag = 31;

	if (get_frc_devp()->power_on_flag == 0)
		return 0;

	pr_frc(flag, "in: addr:0x%08x, val:0x%x\n", addr, val);

	if (is_rdma_enable()) {
		//frc_rdma_table_config(addr, val);
		frc_rdma_write_reg(addr, val);
	} else {
		writel(val, (frc_base + (addr << 2)));
	}

	return 0;
}
EXPORT_SYMBOL(FRC_RDMA_VSYNC_WR_REG);

int FRC_RDMA_VSYNC_WR_BITS(u32 addr, u32 val, u32 start, u32 len)
{
	if (get_frc_devp()->power_on_flag == 0)
		return 0;

	if (is_rdma_enable()) {
		frc_rdma_write_bits(addr, val, start, len);
	} else {
		u32 write_val, mask;
		u32 read_val = readl(frc_base + (addr << 2));
		// write_val = (write_val & ~(((1L << (len)) - 1) << (start))) |
		//     ((u32)(val) << (start));
		mask = (((1L << len) - 1) << start);
		write_val  = read_val & ~mask;
		write_val |= (val << start) & mask;
		writel(write_val, (frc_base + (addr << 2)));
	}

	return 0;
}
EXPORT_SYMBOL(FRC_RDMA_VSYNC_WR_BITS);

int FRC_RDMA_VSYNC_REG_UPDATE(u32 addr, u32 val, u32 mask)
{
	int flag = 31;

	if (get_frc_devp()->power_on_flag == 0)
		return 0;
	pr_frc(flag, "in: addr:0x%08x, val:0x%x, mask:0x%x\n", addr, val, mask);

	if (is_rdma_enable()) {
		frc_rdma_update_reg_bits(addr, val, mask);
	} else {
		if (mask == 0xFFFFFFFF) {
			writel(val, (frc_base + (addr << 2)));
			//frc_rdma_table_config(addr, val);
		} else {
			u32 write_val = readl(frc_base + (addr << 2));

			val &= mask;
			write_val &= ~mask;
			write_val |= val;
			// writel(write_val, (frc_base + (addr << 2)));
			frc_rdma_table_config(addr, write_val);
		}
	}

	return 0;
}
EXPORT_SYMBOL(FRC_RDMA_VSYNC_REG_UPDATE);

void frc_rdma_reg_list(void)
{
	int i, temp;
	struct frc_rdma_info *frc_rdma = frc_get_rdma_info();

	temp = frc_rdma->rdma_item_count;

	for (i = 0; i < temp; i++) {
		pr_frc(0, "reg list: addr:0x%04x, value:0x%08x\n",
			frc_rdma->rdma_table_addr[i * 2],
			frc_rdma->rdma_table_addr[i * 2 + 1]);
	}
	pr_frc(0, "---------------------------------------\n");
}

/*val 30*/
void frc_rdma_cpu_test(int num)
{
	int i;
	u64 timestamp1, timestamp2;
	struct frc_dev_s *devp = get_frc_devp();

	devp->rdma_time = sched_clock();
	timestamp1 = devp->rdma_time;

	for (i = 0x0; i < num; i++)
		WRITE_FRC_REG_BY_CPU(regs_table_test[i].addr, regs_table_test[i].value);

	timestamp2 = sched_clock();
	pr_frc(0, "CPU interrupt time:%lld\n", timestamp2 - timestamp1);
}

/*val 31*/
void frc_rdma_int_test(void)
{
	int i;
	u64 timestamp1;
	struct frc_dev_s *devp = get_frc_devp();

	devp->rdma_time = sched_clock();
	timestamp1 = devp->rdma_time;

	for (i = 0; i < REG_TEST_NUM; i++)
		FRC_RDMA_VSYNC_WR_REG(regs_table_test[i].addr, regs_table_test[i].value);

	// pr_frc(0, "test rdma interrupt time:%lld\n", timestamp2 - timestamp1);
}

/*val 20*/
void frc_rdma_speed_test(int num)
{
	int i;
	u64 timestamp1, timestamp2;
	struct frc_rdma_info *frc_rdma = frc_get_rdma_info();

	frc_rdma->rdma_item_count = 0;
	timestamp1 = sched_clock();
	for (i = 0x0; i < num; i++) {
		frc_rdma->rdma_table_addr[i * 2] = regs_table_test[i].addr & 0xffff;
		frc_rdma->rdma_table_addr[i * 2 + 1] = 0xffffffff - i;
		frc_rdma->rdma_item_count++;
		pr_frc(2, "addr:%04x, value:%08x\n",
			frc_rdma->rdma_table_addr[i * 2], frc_rdma->rdma_table_addr[i * 2 + 1]);
	}
	// frc_rdma_write_test(frc_rdma->rdma_table_phy_addr,
	//     sizeof(frc_rdma->rdma_table_addr[1]) * REG_NUM);
	WRITE_FRC_REG_BY_CPU(FRC_RDMA_ACCESS_MAN,
		READ_FRC_REG(FRC_RDMA_ACCESS_MAN) & (~1));
	WRITE_FRC_REG_BY_CPU(FRC_RDMA_AHB_START_ADDR_MAN, frc_rdma->rdma_table_phy_addr);
	WRITE_FRC_REG_BY_CPU(FRC_RDMA_AHB_END_ADDR_MAN,
		frc_rdma->rdma_table_phy_addr +
		sizeof(u32) * frc_rdma->rdma_item_count * 2 - 1);

	//WRITE_FRC_BITS(FRC_RDMA_ACCESS_MAN, 0, 1, 1); // 0 no-inc
	WRITE_FRC_BITS(FRC_RDMA_ACCESS_MAN, 1, 2, 1); // 1 write

	WRITE_FRC_REG_BY_CPU(FRC_RDMA_ACCESS_MAN,
		READ_FRC_REG(FRC_RDMA_ACCESS_MAN) | 1);

	while (1) {
		if ((READ_FRC_REG(FRC_RDMA_STATUS) >> 24 & 0x1) == 1) {
			WRITE_FRC_REG_BY_CPU(FRC_RDMA_CTRL, 0x1 << 24);
			break;
		}
	}
	timestamp2 = sched_clock();
	pr_frc(0, "%sRDMA MANUAL time:%lld\n", __func__, timestamp2 - timestamp1);
}

/*val 40*/
void frc_auto_limit_test(int num)
{
	int i;
	u64 timestamp1, timestamp2;
	struct frc_rdma_info *frc_rdma = frc_get_rdma_info();
	struct frc_dev_s *devp = get_frc_devp();

	frc_rdma->rdma_item_count = 0;
	timestamp1 = sched_clock();
	devp->rdma_time = timestamp1;
	for (i = 0x0; i < num; i++) {
		frc_rdma->rdma_table_addr[i * 2] = regs_table_test[i].addr & 0xffff;
		frc_rdma->rdma_table_addr[i * 2 + 1] = regs_table_test[i].value & 0xffffffff;
		frc_rdma->rdma_item_count++;
		pr_frc(2, "addr:%04x\n", frc_rdma->rdma_table_addr[i]);
	}

	WRITE_FRC_BITS(FRC_RDMA_AUTO_SRC1_SEL, 0x0, 5, 1);
	WRITE_FRC_REG_BY_CPU(FRC_RDMA_AHB_START_ADDR_1, frc_rdma->rdma_table_phy_addr);
	WRITE_FRC_REG_BY_CPU(FRC_RDMA_AHB_END_ADDR_1,
		frc_rdma->rdma_table_phy_addr + sizeof(u32) * frc_rdma->rdma_item_count * 2 - 1);

	WRITE_FRC_BITS(FRC_RDMA_ACCESS_AUTO, 0x0, 1, 1); // 0: non-incremental reg access
	WRITE_FRC_BITS(FRC_RDMA_ACCESS_AUTO, 0x1, 5, 1); // auto write
	WRITE_FRC_BITS(FRC_RDMA_AUTO_SRC1_SEL, 0x1, 5, 1);
	//frc_rdma->rdma_item_count = 0;
	while (1) {
		if ((READ_FRC_REG(FRC_RDMA_STATUS) >> 25 & 0x1) == 1)
			//WRITE_FRC_REG_BY_CPU(FRC_RDMA_CTRL, 0x1 << 24);
			break;
	}
	timestamp2 = sched_clock();
	pr_frc(0, "%sRDMA MANUAL time:%lld\n", __func__, timestamp2 - timestamp1);
}

int frc_auto_test(int val, int val2)
{
	if (val == 1)
		frc_auto_limit_test(val2);
	else if (val == 2)
		frc_rdma_cpu_test(val2);
	else if (val == 3)
		frc_rdma_speed_test(val2);

	return 0;
}

int frc_rdma_test_write(u32 handle, u32 addr, u32 val, u32 start, u32 len)
{
	if (handle == 0) {
		/* code */
		FRC_RDMA_VSYNC_WR_REG(addr, val);
	} else if (handle == 1) {
		/* code */
		// start -> mask
		FRC_RDMA_VSYNC_REG_UPDATE(addr, val, start);
	} else if (handle == 2) {
		/*code */
		FRC_RDMA_VSYNC_WR_BITS(addr, val, start, len);
	}

	return 0;
}

int frc_rdma_process(u32 val)
{
	int i;
	u32 rdma_status;
	struct frc_rdma_info *frc_rdma = &frc_rdma_s;

	if (is_rdma_enable()) {
		if (!frc_rdma->buf_status) {
			pr_frc(0, "rdma buffer is null\n");
			return 0;
		}
		//frc_rdma_alloc_buf(frc_rdma);
	}

	// val 1: add
	// val 2: rdma reg status
	// val 3: manual non-incremental write start
	// val 4:
	// val 5: manual non-incremental read start
	// val 10: read test reg
	// val 11: reset test reg

	if (val == 1) {
		for (i = 0x0; i < 0x10; i++) {
			frc_rdma->rdma_table_addr[i * 2] = (0x60 + i) & 0xffff;
			frc_rdma->rdma_table_addr[i * 2 + 1] = 0xffffffff - i;
			pr_frc(0, "addr:%04x, value:%08x\n",
				frc_rdma->rdma_table_addr[i * 2],
				frc_rdma->rdma_table_addr[i * 2 + 1]);
		}
		frc_rdma_write_test(frc_rdma->rdma_table_phy_addr, 0x7f);
	} else if (val == 2) {
		frc_rdma_reg_status();
	} else if (val == 3) {
		WRITE_FRC_REG_BY_CPU(FRC_RDMA_ACCESS_MAN, 0x5);
		while (1) {
			rdma_status = READ_FRC_REG(FRC_RDMA_STATUS);
			if (rdma_status & (0x1 << 24)) {
				pr_frc(0, "write done");
				WRITE_FRC_REG_BY_CPU(FRC_RDMA_CTRL, 0x1 << 24);
				break;
			}
		}
		// WRITE_FRC_REG_BY_CPU(FRC_RDMA_CTRL, 0x1 << 24);
		// frc_rdma_manual_start();
	} else if (val == 4) {
		for (i = 0x0; i < 0x10; i++) {
			frc_rdma->rdma_table_addr[i] = (0x60 + i) & 0xffff;
			pr_frc(0, "addr:%04x\n", frc_rdma->rdma_table_addr[i]);
		}
		frc_rdma_write_test(frc_rdma->rdma_table_phy_addr, 0x7f);
	} else if (val == 5) {
		WRITE_FRC_REG_BY_CPU(FRC_RDMA_ACCESS_MAN, 0x1);
		pr_frc(0, "read done");
	} else if (val == 6) {
		frc_read_table();

	} else if (val == 7) {
		// frc_rdma->rdma_table_addr[0] = 0x60 & 0xffff;
		// for (i = 0x1; i < 0x11; i++) {
		//	frc_rdma->rdma_table_addr[i] = 0xffffffff - i * i;
		//	pr_frc(0, "value:%08x\n", frc_rdma->rdma_table_addr[i]);
		// }
		// pr_frc(0, "start addr:%04x\n", frc_rdma->rdma_table_addr[0]);
		// WRITE_FRC_REG_BY_CPU(FRC_RDMA_AHB_START_ADDR_MAN, frc_rdma->rdma_table_phy_addr);
		// WRITE_FRC_REG_BY_CPU(FRC_RDMA_AHB_END_ADDR_MAN,
		//	frc_rdma->rdma_table_phy_addr +
		// sizeof(frc_rdma->rdma_table_addr[1]) * 0x11);
		// WRITE_FRC_REG_BY_CPU(FRC_RDMA_ACCESS_MAN, 0x7);
		frc_read_table2_clear();
		// pr_frc(0, "incremental write done\n");
	} else if (val == 8) {
		WRITE_FRC_REG_BY_CPU(FRC_RDMA_ACCESS_MAN, 0x7);
		pr_frc(0, "incremental write done\n");
	} else if (val == 9) {
		frc_rdma->rdma_item_count = 0;
		for (i = 0x0; i < 0x10; i++) {
			frc_rdma->rdma_table_addr[i * 2] = (0x60 + i) & 0xffff;
			frc_rdma->rdma_table_addr[i * 2 + 1] = 0xffffffff - i * i;
			frc_rdma->rdma_item_count++;
			pr_frc(0, "addr:%04x, value:%08x\n",
				frc_rdma->rdma_table_addr[i * 2],
				frc_rdma->rdma_table_addr[i * 2 + 1]);
		}
		WRITE_FRC_REG_BY_CPU(FRC_RDMA_AUTO_SRC1_SEL, 0x0);
		WRITE_FRC_REG_BY_CPU(FRC_RDMA_AHB_START_ADDR_1, frc_rdma->rdma_table_phy_addr);
		WRITE_FRC_REG_BY_CPU(FRC_RDMA_AHB_END_ADDR_1,
			frc_rdma->rdma_table_phy_addr + frc_rdma->rdma_item_count * 8 - 1);

		WRITE_FRC_BITS(FRC_RDMA_ACCESS_AUTO, 0x0, 1, 1); // 0: non-inc reg access
		WRITE_FRC_BITS(FRC_RDMA_ACCESS_AUTO, 0x1, 5, 1); // auto write
		WRITE_FRC_REG_BY_CPU(FRC_RDMA_AUTO_SRC1_SEL, 0x20);
	} else if (val == 10) {
		frc_rdma_read_test_reg();
	} else if (val == 11) {
		frc_rdma_reset_test_reg();
	} else if (val == 20) {
		//manual write speed test
		frc_rdma_speed_test(REG_TEST_NUM);
	} else if (val == 21) {
		;
	} else if (val == 22) {
		;
	   //read_regs_tables();
	} else if (val == 23) {
		frc_rdma->rdma_item_count = 0;
		for (i = 0x0; i < 0x10; i++) {
			frc_rdma->rdma_table_addr[i * 2] = (0x60 + i) & 0xffff;
			frc_rdma->rdma_table_addr[i * 2 + 1] = 0xfffffff - i * i;
			frc_rdma->rdma_item_count++;
			pr_frc(2, "addr:%04x\n", frc_rdma->rdma_table_addr[i]);
		}
		WRITE_FRC_REG_BY_CPU(FRC_RDMA_AHB_START_ADDR_1, frc_rdma->rdma_table_phy_addr);
		WRITE_FRC_REG_BY_CPU(FRC_RDMA_AHB_END_ADDR_1,
			frc_rdma->rdma_table_phy_addr + frc_rdma->rdma_item_count * 8 - 1);

		WRITE_FRC_BITS(FRC_RDMA_ACCESS_AUTO, 0x0, 1, 1); // 0: non-inc reg access
		WRITE_FRC_BITS(FRC_RDMA_ACCESS_AUTO, 0x1, 5, 1); // auto write
		WRITE_FRC_BITS(FRC_RDMA_AUTO_SRC1_SEL, 0x1, 0, 1);
	} else if (val == 24) {
		//manual trigger
		frc_rdma_config(0, 0);
	} else if (val == 25) {
		//auto trigger
		frc_rdma_config(1, 0);
	} else if (val == 26) {
		//auto2 trigger
		frc_rdma_config(2, 0);
	} else if (val == 30) {
		frc_rdma_cpu_test(REG_TEST_NUM);
	} else if (val == 31) {
		frc_rdma_int_test();
	} else if (val == 40) {
		WRITE_FRC_BITS(FRC_RDMA_AUTO_SRC1_SEL, 0x1, 5, 1);
		// frc_auto_limit_test();
	}

	return 0;
}

irqreturn_t frc_rdma_isr(int irq, void *dev_id)
{
	int i;
	u32 rdma_status;
	// struct frc_dev_s *devp = (struct frc_dev_s *)dev_id;
	// u64 timestamp;
	// if (frc_rdma_debug & 0x1)
	//     return IRQ_HANDLED;

	rdma_status = READ_FRC_REG(FRC_RDMA_STATUS);
	pr_frc(9, "%s frc isr frc_rdma status[1/2/3]:[%x][%x][%0x] rdma_cnt:%d\n",
		__func__, READ_FRC_REG(FRC_RDMA_STATUS),
		READ_FRC_REG(FRC_RDMA_STATUS2), READ_FRC_REG(FRC_RDMA_STATUS3), rdma_cnt);

	for (i = 0; i < RDMA_NUM; i++) {
		if (rdma_status & (0x1 << 24)) {
			// pr_frc(2, "manual write done\n");
			// timestamp = sched_clock() - devp->rdma_time;
			pr_frc(9, "rdma manual int\n");
			WRITE_FRC_BITS(FRC_RDMA_CTRL, 0x1, 24, 1);
			rdma_cnt = 0;
			break;
		} else if (rdma_status & (0x1 << 25)) {
			// WRITE_FRC_REG_BY_CPU(FRC_RDMA_CTRL, 0x1 << 25);
			WRITE_FRC_BITS(FRC_RDMA_CTRL, 0x1, 25, 1);
			// WRITE_FRC_REG_BY_CPU(FRC_RDMA_AUTO_SRC1_SEL, 0x0);
			pr_frc(9, "rdma auto1 int\n");
			rdma_cnt = 0;
			//pr_frc(0, "auto write done cnt:%d\n", rdma_cnt);
			break;
		} else if (rdma_status & (0x1 << 26)) {
			// WRITE_FRC_REG_BY_CPU(FRC_RDMA_CTRL, 0x1 << 26);
			WRITE_FRC_BITS(FRC_RDMA_CTRL, 0x1, 26, 1);
			// WRITE_FRC_REG_BY_CPU(FRC_RDMA_AUTO_SRC2_SEL, 0x0);
			// timestamp = sched_clock() - devp->rdma_time;
			pr_frc(0, "rdma auto2 int\n");
			//pr_frc(0, "auto write done cnt:%d\n", rdma_cnt);
			break;
		} else if (rdma_status & (0x1 << 27)) {
			// WRITE_FRC_REG_BY_CPU(FRC_RDMA_CTRL, 0x1 << 26);
			WRITE_FRC_BITS(FRC_RDMA_CTRL, 0x1, 27, 1);
			// WRITE_FRC_REG_BY_CPU(FRC_RDMA_AUTO_SRC3_SEL, 0x0);
			// timestamp = sched_clock() - devp->rdma_time;
			pr_frc(0, "rdma auto3 int\n");
			//pr_frc(0, "auto write done cnt:%d\n", rdma_cnt);
			break;
		}
	}

	return IRQ_HANDLED;
}

int frc_rdma_init(void)
{
	u32 data32;
	struct frc_dev_s *devp;
	struct frc_fw_data_s *fw_data;
	struct frc_rdma_info *frc_rdma = &frc_rdma_s;

	devp = get_frc_devp();
	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	// struct frc_rdma_info *frc_rdma2 = &frc_rdma_s2;

	data32  = 0;
	data32 |= 0 << 7; /* write ddr urgent */
	data32 |= 0 << 6; /* read ddr urgent */
	data32 |= 0 << 4;
	data32 |= 0 << 2;
	data32 |= 0 << 1;
	data32 |= 0 << 0;

	// init clk
	WRITE_FRC_BITS(FRC_RDMA_SYNC_CTRL, 1, 2, 1);
	WRITE_FRC_BITS(FRC_RDMA_SYNC_CTRL, 1, 6, 1);
	// rdma config
	WRITE_FRC_REG_BY_CPU(FRC_RDMA_CTRL, data32);

	// alloc buf
	frc_rdma->rdma_table_size = FRC_RDMA_SIZE / 2;

	// debug buf reserved
	// frc_rdma2->rdma_table_size = FRC_RDMA_SIZE / 2;

	//frc_rdma_alloc_buf(frc_rdma);
	if (frc_rdma->buf_status) {
	   //RDMA buf ready
		frc_rdma_enable = 0;
		fw_data->frc_top_type.rdma_en = 0;  // init rdma on
		pr_frc(0, "int rdma ok with off stats\n");
	} else {
		PR_ERR("alloc frc rdma buffer failed\n");
		return 0;
	}

	return 1;
}
