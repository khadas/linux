// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 *
 */

#include <linux/debugfs.h>
#include "rkx11x_debugfs.h"
#include "rkx11x_dump.h"
#include "rkx11x_reg.h"

struct rkx11x_reg {
	const char *name;
	uint32_t reg_base;
	uint32_t reg_len;
};

static const struct rkx11x_reg rkx11x_regs[] = {
	{
		.name = "cru",
		.reg_base = RKX11X_SER_CRU_BASE,
		.reg_len = 0xF04,
	},
	{
		.name = "grf",
		.reg_base = RKX11X_SER_GRF_BASE,
		.reg_len = 0x220,
	},
	{
		.name = "grf_mipi0",
		.reg_base = RKX11X_GRF_MIPI0_BASE,
		.reg_len = 0x600,
	},
	{
		.name = "grf_mipi1",
		.reg_base = RKX11X_GRF_MIPI1_BASE,
		.reg_len = 0x600,
	},
	{
		.name = "mipi_lvds_phy0",
		.reg_base = RKX11X_MIPI_LVDS_RX_PHY0_BASE,
		.reg_len = 0xb0,
	},
	{
		.name = "mipi_lvds_phy1",
		.reg_base = RKX11X_MIPI_LVDS_RX_PHY1_BASE,
		.reg_len = 0xb0,
	},
	{
		.name = "csi2_host0",
		.reg_base = RKX11X_CSI2HOST0_BASE,
		.reg_len = 0x60,
	},
	{
		.name = "csi2_host1",
		.reg_base = RKX11X_CSI2HOST1_BASE,
		.reg_len = 0x60,
	},
	{
		.name = "vicap",
		.reg_base = RKX11X_VICAP_BASE,
		.reg_len = 0x220,
	},
	{
		.name = "gpio0",
		.reg_base = RKX11X_GPIO0_BASE,
		.reg_len = 0x80,
	},
	{
		.name = "gpio1",
		.reg_base = RKX11X_GPIO1_BASE,
		.reg_len = 0x80,
	},
	{
		.name = "dsi_rx0",
		.reg_base = RKX11X_DSI_RX0_BASE,
		.reg_len = 0x1D0,
	},
	{
		.name = "dsi_rx1",
		.reg_base = RKX11X_DSI_RX1_BASE,
		.reg_len = 0x1D0,
	},
	{
		.name = "rklink",
		.reg_base = RKX11X_SER_RKLINK_BASE,
		.reg_len = 0xD4,
	},
	{
		.name = "pcs0",
		.reg_base = RKX11X_SER_PCS0_BASE,
		.reg_len = 0x1c0,
	},
	{
		.name = "pcs1",
		.reg_base = RKX11X_SER_PCS1_BASE,
		.reg_len = 0x1c0,
	},
	{
		.name = "pma0",
		.reg_base = RKX11X_SER_PMA0_BASE,
		.reg_len = 0x100,
	},
	{
		.name = "pma1",
		.reg_base = RKX11X_SER_PMA1_BASE,
		.reg_len = 0x100,
	},
	{ /* sentinel */ }
};

static int rkx11x_reg_show(struct seq_file *s, void *v)
{
	const struct rkx11x_reg *regs = rkx11x_regs;
	struct rkx11x *rkx11x = s->private;
	u32 i = 0, val = 0;

	if (rkx11x == NULL || rkx11x->client == NULL)
		return -EINVAL;

	if (!rkser_is_inited(rkx11x->parent))
		return -EINVAL;

	while (regs->name) {
		if (!strcmp(regs->name, file_dentry(s->file)->d_iname))
			break;
		regs++;
	}

	if (!regs->name) {
		seq_printf(s, "Input Error: %s\n", file_dentry(s->file)->d_iname);
		return -ENODEV;
	}

	seq_printf(s, "rk11x %s (0x%08x):\n", regs->name, regs->reg_base);

	for (i = 0; i <= regs->reg_len; i += 4) {
		rkx11x->i2c_reg_read(rkx11x->client, regs->reg_base + i, &val);

		if (i % 16 == 0)
			seq_printf(s, "\n0x%04x:", i);
		seq_printf(s, " %08x", val);
	}
	seq_puts(s, "\n");

	return 0;
}

static ssize_t rkx11x_reg_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	const struct rkx11x_reg *regs = rkx11x_regs;
	struct rkx11x *rkx11x = file->f_path.dentry->d_inode->i_private;
	u32 addr = 0, val = 0;
	char kbuf[25];
	int ret = 0;

	if (rkx11x == NULL || rkx11x->client == NULL)
		return -EINVAL;

	if (!rkser_is_inited(rkx11x->parent))
		return -EINVAL;

	if (count >= sizeof(kbuf))
		return -ENOSPC;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;

	kbuf[count] = '\0';

	ret = sscanf(kbuf, "%x%x", &addr, &val);
	if (ret != 2)
		return -EINVAL;

	while (regs->name) {
		if (!strcmp(regs->name, file_dentry(file)->d_iname))
			break;
		regs++;
	}

	if (!regs->name)
		return -ENODEV;

	addr += regs->reg_base;

	rkx11x->i2c_reg_write(rkx11x->client, addr, val);

	return count;
}

static int rkx11x_reg_open(struct inode *inode, struct file *file)
{
	struct rkx11x *rkx11x = inode->i_private;

	return single_open(file, rkx11x_reg_show, rkx11x);
}

static const struct file_operations rkx11x_reg_fops = {
	.owner          = THIS_MODULE,
	.open           = rkx11x_reg_open,
	.read           = seq_read,
	.write          = rkx11x_reg_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void rkx11x_register_debugfs_init(struct rkx11x *rkx11x, struct dentry *dentry)
{
	const struct rkx11x_reg *regs = rkx11x_regs;
	struct dentry *dir = NULL;

	dir = debugfs_create_dir("registers", dentry);
	if (!IS_ERR(dir)) {
		while (regs->name) {
			debugfs_create_file(regs->name, 0600, dir, rkx11x, &rkx11x_reg_fops);
			regs++;
		}
	}
}

static int rkx11x_clk_show(struct seq_file *s, void *v)
{
	struct rkx11x *rkx11x = s->private;
	struct hwclk *hwclk = &rkx11x->hwclk;

	if (!rkser_is_inited(rkx11x->parent))
		return -EINVAL;

	if (hwclk->type != CLK_UNDEF)
		rkx11x_hwclk_dump_tree(hwclk);

	return 0;
}

static int rkx11x_clk_open(struct inode *inode, struct file *file)
{
	struct rkx11x *rkx11x = inode->i_private;

	return single_open(file, rkx11x_clk_show, rkx11x);
}

static const struct file_operations rkx11x_clk_fops = {
	.owner          = THIS_MODULE,
	.open           = rkx11x_clk_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

struct rkx11x_module {
	const char *name;
	uint32_t id;
};

static const struct rkx11x_module rkx11x_modules[] = {
	{
		.name = "grf",
		.id = RKX11X_DUMP_GRF,
	},
	{
		.name = "linktx",
		.id = RKX11X_DUMP_LINKTX,
	},
	{
		.name = "grf_mipi0",
		.id = RKX11X_DUMP_GRF_MIPI0,
	},
	{
		.name = "grf_mipi1",
		.id = RKX11X_DUMP_GRF_MIPI1,
	},
	{
		.name = "rxphy0",
		.id = RKX11X_DUMP_RXPHY0,
	},
	{
		.name = "rxphy1",
		.id = RKX11X_DUMP_RXPHY1,
	},
	{
		.name = "csi2host0",
		.id = RKX11X_DUMP_CSI2HOST0,
	},
	{
		.name = "vicap_csi",
		.id = RKX11X_DUMP_VICAP_CSI,
	},
	{
		.name = "vicap_dvp",
		.id = RKX11X_DUMP_VICAP_DVP,
	},
	{ /* sentinel */ }
};

static int rkx11x_module_show(struct seq_file *s, void *v)
{
	const struct rkx11x_module *modules = rkx11x_modules;
	struct rkx11x *rkx11x = s->private;

	if (rkx11x == NULL)
		return -EINVAL;

	if (!rkser_is_inited(rkx11x->parent))
		return -EINVAL;

	while (modules->name) {
		if (!strcmp(modules->name, file_dentry(s->file)->d_iname)) {
			rkx11x_module_dump(rkx11x, modules->id);
			return 0;
		}

		modules++;
	}

	seq_printf(s, "Input Error: %s\n", file_dentry(s->file)->d_iname);

	return -ENODEV;
}

static int rkx11x_module_open(struct inode *inode, struct file *file)
{
	struct rkx11x *rkx11x = inode->i_private;

	return single_open(file, rkx11x_module_show, rkx11x);
}

static const struct file_operations rkx11x_module_fops = {
	.owner          = THIS_MODULE,
	.open           = rkx11x_module_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void rkx11x_module_debugfs_init(struct rkx11x *rkx11x, struct dentry *dentry)
{
	const struct rkx11x_module *modules = rkx11x_modules;
	struct dentry *dir = NULL;

	dir = debugfs_create_dir("module", dentry);
	if (!IS_ERR(dir)) {
		while (modules->name) {
			debugfs_create_file(modules->name, 0600, dir, rkx11x, &rkx11x_module_fops);
			modules++;
		}
	}
}

int rkx11x_debugfs_init(struct rkx11x *rkx11x)
{
	struct dentry *debugfs_serdes = NULL;
	struct dentry *debugfs_root = NULL;
	struct dentry *debugfs_remote = NULL;
	struct device *dev = &rkx11x->client->dev;

	if (!IS_ENABLED(CONFIG_DEBUG_FS))
		return 0;

	debugfs_serdes = debugfs_lookup("rkserdes", NULL);
	if (debugfs_serdes == NULL)
		debugfs_serdes = debugfs_create_dir("rkserdes", NULL);

	/* root: create root dir by dev_name in rkserdes dir */
	debugfs_root = debugfs_create_dir(dev_name(dev), debugfs_serdes);
	rkx11x->debugfs_root = debugfs_root;

	/* remote: create remote dir in root dir */
	debugfs_remote = debugfs_create_dir("rkx11x", debugfs_root);

	rkx11x_register_debugfs_init(rkx11x, debugfs_remote);

	rkx11x_module_debugfs_init(rkx11x, debugfs_remote);

	// cru clock tree dump
	debugfs_create_file("clk", 0400, debugfs_remote, rkx11x, &rkx11x_clk_fops);

	return 0;
}

void rkx11x_debugfs_deinit(struct rkx11x *rkx11x)
{
	if (!IS_ENABLED(CONFIG_DEBUG_FS))
		return;

	debugfs_remove_recursive(rkx11x->debugfs_root);
}
