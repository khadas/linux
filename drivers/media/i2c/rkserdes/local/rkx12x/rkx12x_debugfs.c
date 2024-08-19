// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 Rockchip Electronics Co., Ltd.
 *
 * Author: Cai Wenzhong <cwz@rock-chips.com>
 *
 */

#include <linux/debugfs.h>
#include <media/v4l2-subdev.h>
#include "rkx12x_debugfs.h"
#include "rkx12x_drv.h"
#include "rkx12x_dump.h"
#include "rkx12x_reg.h"

struct rkx12x_reg {
	const char *name;
	uint32_t reg_base;
	uint32_t reg_len;
};

static const struct rkx12x_reg rkx12x_regs[] = {
	{
		.name = "grf",
		.reg_base = RKX12X_DES_GRF_BASE,
		.reg_len = 0x410,
	},
	{
		.name = "cru",
		.reg_base = RKX12X_DES_CRU_BASE,
		.reg_len = 0xF04,
	},
	{
		.name = "grf_mipi0",
		.reg_base = RKX12X_GRF_MIPI0_BASE,
		.reg_len = 0x600,
	},
	{
		.name = "grf_mipi1",
		.reg_base = RKX12X_GRF_MIPI1_BASE,
		.reg_len = 0x600,
	},
	{
		.name = "mipi_lvds_phy0",
		.reg_base = RKX12X_MIPI_LVDS_TX_PHY0_BASE,
		.reg_len = 0x70,
	},
	{
		.name = "mipi_lvds_phy1",
		.reg_base = RKX12X_MIPI_LVDS_TX_PHY1_BASE,
		.reg_len = 0x70,
	},
	{
		.name = "dvp_tx",
		.reg_base = RKX12X_DVP_TX_BASE,
		.reg_len = 0x1C0,
	},
	{
		.name = "gpio0",
		.reg_base = RKX12X_GPIO0_TX_BASE,
		.reg_len = 0x80,
	},
	{
		.name = "gpio1",
		.reg_base = RKX12X_GPIO1_TX_BASE,
		.reg_len = 0x80,
	},
	{
		.name = "csi_tx0",
		.reg_base = RKX12X_CSI_TX0_BASE,
		.reg_len = 0x1D0,
	},
	{
		.name = "csi_tx1",
		.reg_base = RKX12X_CSI_TX1_BASE,
		.reg_len = 0x1D0,
	},
	{
		.name = "rklink",
		.reg_base = RKX12X_DES_RKLINK_BASE,
		.reg_len = 0xD4,
	},
	{
		.name = "pcs0",
		.reg_base = RKX12X_DES_PCS0_BASE,
		.reg_len = 0x1c0,
	},
	{
		.name = "pcs1",
		.reg_base = RKX12X_DES_PCS1_BASE,
		.reg_len = 0x1c0,
	},
	{
		.name = "pma0",
		.reg_base = RKX12X_DES_PMA0_BASE,
		.reg_len = 0x100,
	},
	{
		.name = "pma1",
		.reg_base = RKX12X_DES_PMA1_BASE,
		.reg_len = 0x100,
	},
	{ /* sentinel */ }
};

static int rkx12x_reg_show(struct seq_file *s, void *v)
{
	const struct rkx12x_reg *regs = rkx12x_regs;
	struct rkx12x *rkx12x = s->private;
	u32 i = 0, val = 0;

	if (rkx12x == NULL || rkx12x->client == NULL)
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

	seq_printf(s, "rkx12x %s (0x%08x):\n", regs->name, regs->reg_base);

	for (i = 0; i <= regs->reg_len; i += 4) {
		rkx12x->i2c_reg_read(rkx12x->client, regs->reg_base + i, &val);

		if (i % 16 == 0)
			seq_printf(s, "\n0x%04x:", i);
		seq_printf(s, " %08x", val);
	}
	seq_puts(s, "\n");

	return 0;
}

static ssize_t rkx12x_reg_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	const struct rkx12x_reg *regs = rkx12x_regs;
	struct rkx12x *rkx12x = file->f_path.dentry->d_inode->i_private;
	u32 addr = 0, val = 0;
	char kbuf[25];
	int ret = 0;

	if (rkx12x == NULL || rkx12x->client == NULL)
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

	rkx12x_i2c_reg_write(rkx12x->client, addr, val);

	return count;
}

static int rkx12x_reg_open(struct inode *inode, struct file *file)
{
	struct rkx12x *rkx12x = inode->i_private;

	return single_open(file, rkx12x_reg_show, rkx12x);
}

static const struct file_operations rkx12x_reg_fops = {
	.owner          = THIS_MODULE,
	.open           = rkx12x_reg_open,
	.read           = seq_read,
	.write          = rkx12x_reg_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void rkx12x_register_debugfs_init(struct rkx12x *rkx12x, struct dentry *dentry)
{
	const struct rkx12x_reg *regs = rkx12x_regs;
	struct dentry *dir = NULL;

	dir = debugfs_create_dir("registers", dentry);
	if (!IS_ERR(dir)) {
		while (regs->name) {
			debugfs_create_file(regs->name, 0600, dir, rkx12x, &rkx12x_reg_fops);
			regs++;
		}
	}
}

static int rkx12x_clk_show(struct seq_file *s, void *v)
{
	struct rkx12x *rkx12x = s->private;
	struct hwclk *hwclk = &rkx12x->hwclk;

	if (hwclk->type != CLK_UNDEF)
		rkx12x_hwclk_dump_tree(hwclk);

	return 0;
}

static int rkx12x_clk_open(struct inode *inode, struct file *file)
{
	struct rkx12x *rkx12x = inode->i_private;

	return single_open(file, rkx12x_clk_show, rkx12x);
}

static const struct file_operations rkx12x_clk_fops = {
	.owner          = THIS_MODULE,
	.open           = rkx12x_clk_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

struct rkx12x_link_rate_info {
	const char *name;
	uint32_t id;
};

static const struct rkx12x_link_rate_info rkx12x_link_rate_infos[] = {
	{
		.name = "forward: 2Gbps, backward: 83Mbps",
		.id = RKX12X_LINK_RATE_2GBPS_83M,
	},
	{
		.name = "forward: 4Gbps, backward: 83Mbps",
		.id = RKX12X_LINK_RATE_4GBPS_83M,
	},
	{
		.name = "forward: 4Gbps, backward: 125Mbps",
		.id = RKX12X_LINK_RATE_4GBPS_125M,
	},
	{
		.name = "forward: 4Gbps, backward: 250Mbps",
		.id = RKX12X_LINK_RATE_4GBPS_250M,
	},
	{
		.name = "forward: 4.5Gbps, backward: 140Mbps",
		.id = RKX12X_LINK_RATE_4_5GBPS_140M,
	},
	{
		.name = "forward: 4.6Gbps, backward: 300Mbps",
		.id = RKX12X_LINK_RATE_4_6GBPS_300M,
	},
	{
		.name = "forward: 4.8Gbps, backward: 150Mbps",
		.id = RKX12X_LINK_RATE_4_8GBPS_150M,
	},
	{ /* sentinel */ }
};

static int rkx12x_link_rate_show(struct seq_file *s, void *v)
{
	const struct rkx12x_link_rate_info *link_rate_info = rkx12x_link_rate_infos;
	struct rkx12x *rkx12x = s->private;

	if (rkx12x == NULL)
		return -EINVAL;

	seq_printf(s, "serdes link current rate: %d\n", rkx12x->linkrx.link_rate);
	seq_printf(s, "\n");

	seq_printf(s, "serdes link set rate command as follow:\n");
	seq_printf(s, "    echo link_rate > link rate node path\n");
	seq_printf(s, "\n");

	seq_printf(s, "serdes link_rate support list as follows:\n");
	while (link_rate_info->name) {
		seq_printf(s, "    %d: %s\n", link_rate_info->id, link_rate_info->name);
		link_rate_info++;
	}
	seq_printf(s, "\n");

	return 0;
}

static ssize_t rkx12x_link_rate_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct rkx12x *rkx12x = file->f_path.dentry->d_inode->i_private;
	u32 link_rate;
	char kbuf[25];
	int ret;

	if (count >= sizeof(kbuf))
		return -ENOSPC;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;

	kbuf[count] = '\0';

	ret = sscanf(kbuf, "%d", &link_rate);
	if (ret != 1)
		return -EINVAL;

	rkx12x_link_set_rate(&rkx12x->linkrx, link_rate, false);

	return count;
}

static int rkx12x_link_rate_open(struct inode *inode, struct file *file)
{
	struct rkx12x *rkx12x = inode->i_private;

	return single_open(file, rkx12x_link_rate_show, rkx12x);
}

static const struct file_operations rkx12x_link_rate_fops = {
	.owner          = THIS_MODULE,
	.open           = rkx12x_link_rate_open,
	.read           = seq_read,
	.write          = rkx12x_link_rate_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

struct rkx12x_link_line_info {
	const char *name;
	uint32_t id;
};

static const struct rkx12x_link_line_info rkx12x_link_line_infos[] = {
	{
		.name = "link line common config",
		.id = RKX12X_LINK_LINE_COMMON_CONFIG,
	},
	{
		.name = "link line short config",
		.id = RKX12X_LINK_LINE_SHORT_CONFIG,
	},
	{
		.name = "link line long config",
		.id = RKX12X_LINK_LINE_LONG_CONFIG,
	},
	{ /* sentinel */ }
};

static int rkx12x_link_line_show(struct seq_file *s, void *v)
{
	const struct rkx12x_link_line_info *link_line_info = rkx12x_link_line_infos;
	struct rkx12x *rkx12x = s->private;

	if (rkx12x == NULL)
		return -EINVAL;

	seq_printf(s, "serdes current link line: %d\n", rkx12x->linkrx.link_line);
	seq_printf(s, "\n");

	seq_printf(s, "serdes link set line config command as follow:\n");
	seq_printf(s, "    echo link_line > link line node path\n");
	seq_printf(s, "\n");

	seq_printf(s, "serdes link_line support list as follows:\n");
	while (link_line_info->name) {
		seq_printf(s, "    %d: %s\n", link_line_info->id, link_line_info->name);
		link_line_info++;
	}
	seq_printf(s, "\n");

	return 0;
}

static ssize_t rkx12x_link_line_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct rkx12x *rkx12x = file->f_path.dentry->d_inode->i_private;
	u32 link_line;
	char kbuf[25];
	int ret;

	if (count >= sizeof(kbuf))
		return -ENOSPC;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;

	kbuf[count] = '\0';

	ret = kstrtou32(kbuf, 10, &link_line);
	if (ret < 0)
		return ret;

	rkx12x->linkrx.link_line = link_line;

	return count;
}

static int rkx12x_link_line_open(struct inode *inode, struct file *file)
{
	struct rkx12x *rkx12x = inode->i_private;

	return single_open(file, rkx12x_link_line_show, rkx12x);
}

static const struct file_operations rkx12x_link_line_fops = {
	.owner          = THIS_MODULE,
	.open           = rkx12x_link_line_open,
	.read           = seq_read,
	.write          = rkx12x_link_line_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int rkx12x_power_ctrl_show(struct seq_file *s, void *v)
{
	seq_printf(s, "serdes power control command as follow:\n");
	seq_printf(s, "    power on: \n");
	seq_printf(s, "        echo 1 > power node path\n");
	seq_printf(s, "    power off: \n");
	seq_printf(s, "        echo 0 > power node path\n");
	seq_printf(s, "\n");

	return 0;
}

static ssize_t rkx12x_power_ctrl_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct rkx12x *rkx12x = file->f_path.dentry->d_inode->i_private;
	u32 power_ctrl = 0;
	char kbuf[25];
	int ret, error;

	if (count >= sizeof(kbuf))
		return -ENOSPC;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;

	kbuf[count] = '\0';

	ret = kstrtou32(kbuf, 10, &power_ctrl);
	if (ret < 0)
		return ret;

	if (rkx12x == NULL || rkx12x->client == NULL)
		return -EFAULT;

	pr_info("power control = %d\n", power_ctrl);
	error = v4l2_subdev_call(&rkx12x->subdev, core, s_power, power_ctrl);
	if (error < 0) {
		pr_err("power control error = %d\n", error);
		return -EFAULT;
	}

	return count;
}

static int rkx12x_power_ctrl_open(struct inode *inode, struct file *file)
{
	struct rkx12x *rkx12x = inode->i_private;

	return single_open(file, rkx12x_power_ctrl_show, rkx12x);
}

static const struct file_operations rkx12x_power_ctrl_fops = {
	.owner          = THIS_MODULE,
	.open           = rkx12x_power_ctrl_open,
	.read           = seq_read,
	.write          = rkx12x_power_ctrl_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

struct rkx12x_module {
	const char *name;
	uint32_t id;
};

static const struct rkx12x_module rkx12x_modules[] = {
	{
		.name = "grf",
		.id = RKX12X_DUMP_GRF,
	},
	{
		.name = "linkrx",
		.id = RKX12X_DUMP_LINKRX,
	},
	{
		.name = "grf_mipi0",
		.id = RKX12X_DUMP_GRF_MIPI0,
	},
	{
		.name = "grf_mipi1",
		.id = RKX12X_DUMP_GRF_MIPI1,
	},
	{
		.name = "txphy0",
		.id = RKX12X_DUMP_TXPHY0,
	},
	{
		.name = "txphy1",
		.id = RKX12X_DUMP_TXPHY1,
	},
	{
		.name = "csi_tx0",
		.id = RKX12X_DUMP_CSITX0,
	},
	{
		.name = "csi_tx1",
		.id = RKX12X_DUMP_CSITX1,
	},
	{
		.name = "dvp_tx",
		.id = RKX12X_DUMP_DVPTX,
	},
	{ /* sentinel */ }
};

static int rkx12x_module_show(struct seq_file *s, void *v)
{
	const struct rkx12x_module *modules = rkx12x_modules;
	struct rkx12x *rkx12x = s->private;

	if (rkx12x == NULL)
		return -EINVAL;

	while (modules->name) {
		if (!strcmp(modules->name, file_dentry(s->file)->d_iname)) {
			rkx12x_module_dump(rkx12x, modules->id);
			return 0;
		}

		modules++;
	}

	seq_printf(s, "Input Error: %s\n", file_dentry(s->file)->d_iname);

	return -ENODEV;
}

static int rkx12x_module_open(struct inode *inode, struct file *file)
{
	struct rkx12x *rkx12x = inode->i_private;

	return single_open(file, rkx12x_module_show, rkx12x);
}

static const struct file_operations rkx12x_module_fops = {
	.owner          = THIS_MODULE,
	.open           = rkx12x_module_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void rkx12x_module_debugfs_init(struct rkx12x *rkx12x, struct dentry *dentry)
{
	const struct rkx12x_module *modules = rkx12x_modules;
	struct dentry *dir = NULL;

	dir = debugfs_create_dir("module", dentry);
	if (!IS_ERR(dir)) {
		while (modules->name) {
			debugfs_create_file(modules->name, 0600, dir, rkx12x, &rkx12x_module_fops);
			modules++;
		}
	}
}

enum {
	T_HSEXIT_DLANE0 = 0,
	T_HSEXIT_DLANE1,
	T_HSEXIT_DLANE2,
	T_HSEXIT_DLANE3,

	T_HSTRAIL_DLANE0,
	T_HSTRAIL_DLANE1,
	T_HSTRAIL_DLANE2,
	T_HSTRAIL_DLANE3,

	T_HSZERO_DLANE0,
	T_HSZERO_DLANE1,
	T_HSZERO_DLANE2,
	T_HSZERO_DLANE3,

	T_HSPREPARE_DLANE0,
	T_HSPREPARE_DLANE1,
	T_HSPREPARE_DLANE2,
	T_HSPREPARE_DLANE3,
};

struct rkx12x_txphy_param {
	const char *name;
	uint32_t id;
};

static const struct rkx12x_txphy_param rkx12x_txphy_params[] = {
	{
		.name = "t_hstrail_dlane0",
		.id = T_HSTRAIL_DLANE0,
	},
	{
		.name = "t_hstrail_dlane1",
		.id = T_HSTRAIL_DLANE1,
	},
	{
		.name = "t_hstrail_dlane2",
		.id = T_HSTRAIL_DLANE2,
	},
	{
		.name = "t_hstrail_dlane3",
		.id = T_HSTRAIL_DLANE3,
	},
	{ /* sentinel */ }
};

static int rkx12x_txphy_param_show(struct seq_file *s, void *v)
{
	const struct rkx12x_txphy_param *txphy_param = rkx12x_txphy_params;
	struct rkx12x *rkx12x = s->private;
	struct rkx12x_txphy *txphy = NULL;
	u32 param_value;

	if (rkx12x == NULL)
		return -EINVAL;
	txphy = &rkx12x->txphy;

	while (txphy_param->name) {
		if (!strcmp(txphy_param->name, file_dentry(s->file)->d_iname))
			break;
		txphy_param++;
	}

	if (!txphy_param->name) {
		seq_printf(s, "Input Error: %s\n", file_dentry(s->file)->d_iname);
		return -ENODEV;
	}

	if (txphy_param->id == T_HSTRAIL_DLANE0) {
		param_value = txphy->mipi_timing.t_hstrail_dlane0;
		if (param_value & RKX12X_MIPI_TIMING_EN) {
			param_value &= (~RKX12X_MIPI_TIMING_EN);
			seq_printf(s, "%s = %d\n", txphy_param->name, param_value);
		} else {
			seq_printf(s, "%s timing setting disable\n", txphy_param->name);
		}
	} else if (txphy_param->id == T_HSTRAIL_DLANE1) {
		param_value = txphy->mipi_timing.t_hstrail_dlane1;
		if (param_value & RKX12X_MIPI_TIMING_EN) {
			param_value &= (~RKX12X_MIPI_TIMING_EN);
			seq_printf(s, "%s = %d\n", txphy_param->name, param_value);
		} else {
			seq_printf(s, "%s timing setting disable\n", txphy_param->name);
		}
	} else if (txphy_param->id == T_HSTRAIL_DLANE2) {
		param_value = txphy->mipi_timing.t_hstrail_dlane2;
		if (param_value & RKX12X_MIPI_TIMING_EN) {
			param_value &= (~RKX12X_MIPI_TIMING_EN);
			seq_printf(s, "%s = %d\n", txphy_param->name, param_value);
		} else {
			seq_printf(s, "%s timing setting disable\n", txphy_param->name);
		}
	} else if (txphy_param->id == T_HSTRAIL_DLANE3) {
		param_value = txphy->mipi_timing.t_hstrail_dlane3;
		if (param_value & RKX12X_MIPI_TIMING_EN) {
			param_value &= (~RKX12X_MIPI_TIMING_EN);
			seq_printf(s, "%s = %d\n", txphy_param->name, param_value);
		} else {
			seq_printf(s, "%s timing setting disable\n", txphy_param->name);
		}
	} else {
		return -EINVAL;
	}

	seq_printf(s, "\n");

	return 0;
}

static int rkx12x_txphy_param_open(struct inode *inode, struct file *file)
{
	struct rkx12x *rkx12x = inode->i_private;

	return single_open(file, rkx12x_txphy_param_show, rkx12x);
}

static ssize_t rkx12x_txphy_param_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	const struct rkx12x_txphy_param *txphy_param = rkx12x_txphy_params;
	struct rkx12x *rkx12x = file->f_path.dentry->d_inode->i_private;
	struct rkx12x_txphy *txphy = NULL;
	u32 param_value = 0;
	char kbuf[25];
	int ret;

	if (count >= sizeof(kbuf))
		return -ENOSPC;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;

	kbuf[count] = '\0';

	ret = sscanf(kbuf, "%d", &param_value);
	if (ret != 1)
		return -EINVAL;

	while (txphy_param->name) {
		if (!strcmp(txphy_param->name, file_dentry(file)->d_iname))
			break;
		txphy_param++;
	}

	if (!txphy_param->name)
		return -ENODEV;

	if (rkx12x == NULL || rkx12x->client == NULL)
		return -EFAULT;
	txphy = &rkx12x->txphy;

	pr_info("%s: value = %d\n", txphy_param->name, param_value);
	if (txphy_param->id == T_HSTRAIL_DLANE0) {
		param_value |= RKX12X_MIPI_TIMING_EN;
		txphy->mipi_timing.t_hstrail_dlane0 = param_value;
	} else if (txphy_param->id == T_HSTRAIL_DLANE1) {
		param_value |= RKX12X_MIPI_TIMING_EN;
		txphy->mipi_timing.t_hstrail_dlane1 = param_value;
	} else if (txphy_param->id == T_HSTRAIL_DLANE2) {
		param_value |= RKX12X_MIPI_TIMING_EN;
		txphy->mipi_timing.t_hstrail_dlane2 = param_value;
	} else if (txphy_param->id == T_HSTRAIL_DLANE3) {
		param_value |= RKX12X_MIPI_TIMING_EN;
		txphy->mipi_timing.t_hstrail_dlane3 = param_value;
	} else {
		return -EINVAL;
	}

	return count;
}

static const struct file_operations rkx12x_txphy_param_fops = {
	.owner          = THIS_MODULE,
	.open           = rkx12x_txphy_param_open,
	.read           = seq_read,
	.write          = rkx12x_txphy_param_write,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void rkx12x_txphy_param_debugfs_init(struct rkx12x *rkx12x, struct dentry *dentry)
{
	const struct rkx12x_txphy_param *txphy_param = rkx12x_txphy_params;
	struct dentry *dir = NULL;

	dir = debugfs_create_dir("txphy", dentry);
	if (!IS_ERR(dir)) {
		while (txphy_param->name) {
			debugfs_create_file(txphy_param->name, 0600, dir, rkx12x, &rkx12x_txphy_param_fops);
			txphy_param++;
		}
	}
}

int rkx12x_debugfs_init(struct rkx12x *rkx12x)
{
	struct dentry *debugfs_serdes = NULL;
	struct dentry *debugfs_root = NULL;
	struct dentry *debugfs_local = NULL;
	struct device *dev = &rkx12x->client->dev;

	if (!IS_ENABLED(CONFIG_DEBUG_FS))
		return 0;

	dev_info(dev, "rkx12x debugfs init\n");

	/* create rkserdes dir */
	debugfs_serdes = debugfs_lookup("rkserdes", NULL);
	if (debugfs_serdes == NULL)
		debugfs_serdes = debugfs_create_dir("rkserdes", NULL);

	/* root: create root dir by dev_name in rkserdes dir */
	debugfs_root = debugfs_create_dir(dev_name(dev), debugfs_serdes);
	rkx12x->debugfs_root = debugfs_root;

	/* local: create local dir in root dir */
	debugfs_local = debugfs_create_dir("rkx12x", debugfs_root);
	rkx12x_register_debugfs_init(rkx12x, debugfs_local);

	rkx12x_module_debugfs_init(rkx12x, debugfs_local);

	rkx12x_txphy_param_debugfs_init(rkx12x, debugfs_local);

	// cru clock tree dump
	debugfs_create_file("clk", 0400, debugfs_local, rkx12x, &rkx12x_clk_fops);

	// link rate
	debugfs_create_file("link_rate", 0400, debugfs_local, rkx12x, &rkx12x_link_rate_fops);

	// link line
	debugfs_create_file("link_line", 0400, debugfs_local, rkx12x, &rkx12x_link_line_fops);

	// power control
	debugfs_create_file("power", 0400, debugfs_local, rkx12x, &rkx12x_power_ctrl_fops);

	return 0;
}

void rkx12x_debugfs_deinit(struct rkx12x *rkx12x)
{
	if (!IS_ENABLED(CONFIG_DEBUG_FS))
		return;

	debugfs_remove_recursive(rkx12x->debugfs_root);
}
