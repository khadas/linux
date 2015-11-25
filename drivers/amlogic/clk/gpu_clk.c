/*
 * drivers/amlogic/clk/hdmi-clk.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/clk-provider.h>
#include <linux/clk-private.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <linux/of_address.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>


#ifndef AML_CLK_LOCK_ERROR
#define AML_CLK_LOCK_ERROR 1
#endif
static unsigned debug = 0;
module_param(debug, uint, 0644);
MODULE_PARM_DESC(debug, "activates debug info");

#ifndef HHI_MALI_CLK_CNTL
#define HHI_MALI_CLK_CNTL   0x6C
#define mplt_read(r)		readl((gpu_clkgen->base) + ((r)<<2))
#define mplt_write(r, v)	writel((v), ((gpu_clkgen->base) + ((r)<<2)))
#define mplt_setbits(r, m)  mplt_write((r), (mplt_read(r) | (m)));
#define mplt_clrbits(r, m)  mplt_write((r), (mplt_read(r) & (~(m))));
#endif

#define dprintk(level, fmt, arg...)		\
	do {                                \
		if (debug >= level)				\
			pr_debug("gpu clk : " fmt, ## arg);\
	} while (0)

#define GPU_CLK_DBG(fmt, arg...)	\
	{	\
		dprintk(1, "line(%d), clk_cntl=0x%08x\n" fmt, \
		__LINE__, mplt_read(HHI_MALI_CLK_CNTL), ## arg);\
	}

#define to_gpu_clkgen(_hw) container_of(_hw, struct gpu_clkgen, clk_hw)

struct gpu_dvfs_threshold_table {
	uint32_t	freq_index;
	uint32_t	voltage;
	uint32_t	keep_count;
	uint32_t	downthreshold;
	uint32_t	upthreshold;
	uint32_t	clk_freq;
	const char  *clk_parent;
	struct clk  *clkp_handle;
	uint32_t	clkp_freq;
};

struct gpu_clkgen {
	void __iomem *base;
	struct clk_hw clk_hw;
	struct gpu_dvfs_threshold_table *dvfs_table;
	int dvfs_table_size;
	struct clk *clk_gpu_0;
	struct clk *clk_gpu_1;
	struct clk *clk_gpu;
	struct dentry *debugfs_dir;
	uint32_t rate;
};

static unsigned long gpu_clkgen_recal(struct clk_hw *hw,
		unsigned long parent_rate)
{
	/* unsigned int idx = 0; */
	struct gpu_clkgen *gpu_clkgen = to_gpu_clkgen(hw);
	struct clk *clk_gpu_x = NULL;

	dprintk(3, "func:%s, %d, parent_rate=%ld\n",
			__func__, __LINE__, parent_rate);
	if (NULL == gpu_clkgen->clk_gpu) {
		dprintk(0, "clk_gpu not init\n");
		return 0;
	}

	clk_gpu_x = clk_get_parent(gpu_clkgen->clk_gpu);
	if (NULL == clk_gpu_x) {
		dprintk(0, "clk_gpu parent not init\n");
		return 0;
	}
	parent_rate = clk_get_rate(clk_gpu_x);
	dprintk(3, "get rate=%ld\n", parent_rate);
	return parent_rate;
}

static long	gpu_clkgen_determine(struct clk_hw *hw, unsigned long rate,
		unsigned long *best_parent_rate,
		struct clk **best_parent_clk)
{
	int idx = 0;
	struct gpu_clkgen *gpu_clkgen = to_gpu_clkgen(hw);
	struct gpu_dvfs_threshold_table *dvfs_tbl = &gpu_clkgen->dvfs_table[0];

	dprintk(2, "func:%s, %d, parent_rate=%ld, best=%ld\n",
				__func__, __LINE__, rate, *best_parent_rate);
	for (idx = 0; idx < gpu_clkgen->dvfs_table_size; idx++) {
		dprintk(2, "idx=%d, clk_freq=%d\n", idx, dvfs_tbl->clk_freq);
		if (rate == dvfs_tbl->clk_freq) {
			*best_parent_rate = rate;
			*best_parent_clk = gpu_clkgen->clk_gpu;
			return rate;
		}
		dvfs_tbl++;
	}

	dprintk(2, "no matched clk, rate=%ld\n", rate);
	dvfs_tbl = &gpu_clkgen->dvfs_table[0];
	for (idx = 0; idx < gpu_clkgen->dvfs_table_size; idx++) {
		if (rate <= dvfs_tbl->clk_freq) {
			*best_parent_rate = dvfs_tbl->clk_freq;
			*best_parent_clk = gpu_clkgen->clk_gpu;
			return dvfs_tbl->clk_freq;
		}
		dvfs_tbl++;
	}

	dprintk(2, "no matched roud clk, %ld\n", rate);
	dvfs_tbl--;
	*best_parent_rate = dvfs_tbl->clk_freq;
	*best_parent_clk = gpu_clkgen->clk_gpu;
	return dvfs_tbl->clk_freq;
}

static long gpu_clkgen_round(struct clk_hw *hw, unsigned long drate,
		unsigned long *prate)
{
	unsigned int idx = 0;
	struct gpu_clkgen *gpu_clkgen = to_gpu_clkgen(hw);
	struct gpu_dvfs_threshold_table *dvfs_tbl = &gpu_clkgen->dvfs_table[0];

	dprintk(3, "func:%s, %d, parent_rate=%ld\n",
					__func__, __LINE__, *prate);
	for (idx = 0; idx < gpu_clkgen->dvfs_table_size; idx++) {
		dprintk(3, "idx=%d, clk_freq=%d\n", idx, dvfs_tbl->clk_freq);
		if (drate == dvfs_tbl->clk_freq) {
			*prate = dvfs_tbl->clkp_freq;
			return drate;
		}
		dvfs_tbl++;
	}

	return 0;
}

static int	gpu_clkgen_set(struct clk_hw *hw, unsigned long drate,
		unsigned long prate)
{
	int ret = 0;
	unsigned int idx = 0;
	struct gpu_clkgen *gpu_clkgen = to_gpu_clkgen(hw);
	struct gpu_dvfs_threshold_table *dvfs_tbl = &gpu_clkgen->dvfs_table[0];
	struct timeval start;
	struct timeval end;

	struct clk *clk_gpu_0 = gpu_clkgen->clk_gpu_0;
	struct clk *clk_gpu_1 = gpu_clkgen->clk_gpu_1;
	struct clk *clk_gpu_x   = NULL;
	struct clk *clk_gpu_x_parent = NULL;
	struct clk *clk_gpu_x_old = NULL;
	struct clk *clk_gpu   = gpu_clkgen->clk_gpu;
	unsigned long time_use = 0;

	/* Assumming rate_table is in ascending order */
	for (idx = 0; idx < gpu_clkgen->dvfs_table_size; idx++) {
		if (drate == dvfs_tbl->clk_freq) {
			dprintk(2, "drate =%ld,prate=%ld\n", drate, prate);
			break;
		}
		dprintk(2, "idx=%d, clk_freq=%d\n", idx, dvfs_tbl->clk_freq);
		dvfs_tbl++;
	}

	gpu_clkgen->rate = drate;
	clk_gpu_x_old  = clk_get_parent(clk_gpu);

	if (!clk_gpu_x_old) {
		dprintk(0, "gpu: could not get clk_gpu_x_old\n");
		return 0;
	}
	if (clk_gpu_x_old == clk_gpu_0) {
		clk_gpu_x = clk_gpu_1;
	} else if (clk_gpu_x_old == clk_gpu_1) {
		clk_gpu_x = clk_gpu_0;
	} else {
		dprintk(0, "gpu: unmatched clk_gpu_x_old\n");
		return 0;
	}

	GPU_CLK_DBG("idx=%d, clk_freq=%d\n", idx, dvfs_tbl->clk_freq);
	clk_gpu_x_parent = dvfs_tbl->clkp_handle;
	if (!clk_gpu_x_parent) {
		dprintk(0, "gpu: could not get clk_gpu_x_parent\n");
		return 0;
	}

	GPU_CLK_DBG();
	ret = clk_set_rate(clk_gpu_x_parent, dvfs_tbl->clkp_freq);
	GPU_CLK_DBG();
	ret = clk_set_parent(clk_gpu_x, clk_gpu_x_parent);
	GPU_CLK_DBG();
	ret = clk_set_rate(clk_gpu_x, dvfs_tbl->clk_freq);
	GPU_CLK_DBG();
#ifndef AML_CLK_LOCK_ERROR
	ret = clk_prepare_enable(clk_gpu_x);
#endif
	GPU_CLK_DBG("new %s:enable(%d)\n",
			clk_gpu_x->name, clk_gpu_x->enable_count);
	do_gettimeofday(&start);
	udelay(1);/* delay 10ns */
	do_gettimeofday(&end);
	ret = clk_set_parent(clk_gpu, clk_gpu_x);
	GPU_CLK_DBG();

#ifndef AML_CLK_LOCK_ERROR
	clk_disable_unprepare(clk_gpu_x_old);
#endif
#if 0
	GPU_CLK_DBG("old %s:enable(%d)\n",
	clk_gpu_x_old->name,  clk_gpu_x_old->enable_count);
#endif
	time_use = (end.tv_sec - start.tv_sec)*1000000
		+ end.tv_usec - start.tv_usec;
	GPU_CLK_DBG("step 1, mali_mux use: %ld us\n", time_use);

	dprintk(3, "drate=%ld, prate=%ld\n", drate, prate);

	return 0;
}

static struct clk_ops gpu_clkgen_ops = {
	.set_rate	    = gpu_clkgen_set,
	.determine_rate = gpu_clkgen_determine,
	.recalc_rate	= gpu_clkgen_recal,
	.round_rate	 = gpu_clkgen_round,
};

static int gpu_clkgen_debugfs_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	dprintk(3, "gpu_clkgen=%p\n", file->private_data);
	return 0;
}

static ssize_t gpu_clkgen_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *offp)
{
	int ret;
	char buffer[32];
	unsigned long val;
	struct gpu_clkgen  *g = (struct gpu_clkgen *)filp->private_data;

	dprintk(3, "gpu clkgen =%p\n", g);
	if (NULL == g)
		return 0;

	if (count >= sizeof(buffer))
		return -ENOMEM;

	if (copy_from_user(&buffer[0], buf, count))
		return -EFAULT;

	buffer[count] = '\0';

	ret = kstrtoul(&buffer[0], 10, &val);
	if (0 != ret)
		return -EINVAL;

	dprintk(3, "test gpu_clk write val=%ld\n", val);
	gpu_clkgen_set(&g->clk_hw, val, 0);

	*offp += count;
	return count;
}

static ssize_t gpu_clkgen_read(struct file *filp,
		char __user *buf, size_t count, loff_t *offp)
{
	int r;
	char buffer[64];
	struct gpu_clkgen  *g = (struct gpu_clkgen *)filp->private_data;
	unsigned long parent_rate = 0;

	if (NULL == g)
		return 0;

	parent_rate = gpu_clkgen_recal(&g->clk_hw, parent_rate);
	r = snprintf(buffer, 64, "gpu clk rate=%ld\n", parent_rate);

	return simple_read_from_buffer(buf, count, offp, buffer, r);
}

static const struct file_operations gpu_clkgen_fops = {
	.owner = THIS_MODULE,
	.open  = gpu_clkgen_debugfs_open,
	.write = gpu_clkgen_write,
	.read  = gpu_clkgen_read,
	.llseek = default_llseek,
};

static int gpu_parse_clk_cfg(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gpu_clkgen *gpu_clkgen = dev_get_drvdata(dev);
	struct device_node *gpu_dn = dev->of_node;
	struct device_node *gpu_clk_dn;
	phandle dvfs_clk_hdl;
	struct gpu_dvfs_threshold_table *dvfs_tbl = NULL;
	struct property *prop;
	const __be32 *p;
	int length = 0, i = 0;
	u32 u;
	int ret = 0;

	of_get_property(gpu_dn, "tbl", &length);
	length = length / sizeof(u32);
	_dev_info(&pdev->dev, "clock dvfs table size is %d\n", length);
	dprintk(3, "gpu_clkgen=%p", gpu_clkgen);

	gpu_clkgen->dvfs_table = devm_kzalloc(&pdev->dev,
			sizeof(struct gpu_dvfs_threshold_table)*length,
			GFP_KERNEL);
	dvfs_tbl = gpu_clkgen->dvfs_table;
	if (gpu_clkgen->dvfs_table == NULL) {
		dev_err(&pdev->dev, "failed to alloc dvfs table\n");
		return -ENOMEM;
	}
	gpu_clkgen->dvfs_table_size = length;

	of_property_for_each_u32(gpu_dn, "tbl", prop, p, u) {
		dvfs_clk_hdl = (phandle) u;
		gpu_clk_dn = of_find_node_by_phandle(dvfs_clk_hdl);
		ret = of_property_read_u32(gpu_clk_dn,
		"clk_freq", &dvfs_tbl->clk_freq);
		if (ret)
			dev_notice(&pdev->dev, "read clk_freq failed\n");
		ret = of_property_read_string(gpu_clk_dn, "clk_parent",
				&dvfs_tbl->clk_parent);
		if (ret)
			dev_notice(&pdev->dev, "read clk_parent failed\n");
		dvfs_tbl->clkp_handle = devm_clk_get(&pdev->dev,
				dvfs_tbl->clk_parent);
		if (IS_ERR(dvfs_tbl->clkp_handle))
			dev_notice(&pdev->dev, " get %s's hdl fail\n",
					dvfs_tbl->clk_parent);
		ret = of_property_read_u32(gpu_clk_dn,
		"clkp_freq", &dvfs_tbl->clkp_freq);
		if (ret)
			dev_notice(&pdev->dev, "read clk_parent freq failed\n");
		ret = of_property_read_u32(gpu_clk_dn,
				"voltage", &dvfs_tbl->voltage);
		if (ret)
			dev_notice(&pdev->dev, "read voltage failed\n");
		ret = of_property_read_u32(gpu_clk_dn,
				"keep_count", &dvfs_tbl->keep_count);
		if (ret)
			dev_notice(&pdev->dev, "read keep_count failed\n");
		/* downthreshold and upthreshold shall be u32 */
		ret = of_property_read_u32_array(gpu_clk_dn, "threshold",
			&dvfs_tbl->downthreshold, 2);
		if (ret)
			dev_notice(&pdev->dev, "read threshold failed\n");

		dvfs_tbl->freq_index = i;
		dvfs_tbl++;
		i++;
	}

	dvfs_tbl = gpu_clkgen->dvfs_table;
	for (i = 0; i < length; i++) {
		_dev_info(&pdev->dev, "%d:clk_freq=%10d,",
			i, dvfs_tbl->clk_freq);
		_dev_info(&pdev->dev, "clk_parent=%9s,voltage=%d,",
			dvfs_tbl->clk_parent,
			dvfs_tbl->voltage);
		_dev_info(&pdev->dev, "keep_count=%d, threshod=<%d %d>\n",
			dvfs_tbl->keep_count,
			dvfs_tbl->downthreshold, dvfs_tbl->upthreshold);

			dvfs_tbl++;
	}

	gpu_clkgen->clk_gpu = devm_clk_get(&pdev->dev, "clk_gpu");
	gpu_clkgen->clk_gpu_0 = devm_clk_get(&pdev->dev, "clk_gpu_0");
	gpu_clkgen->clk_gpu_1 = devm_clk_get(&pdev->dev, "clk_gpu_1");
	if (IS_ERR(gpu_clkgen->clk_gpu) || IS_ERR(gpu_clkgen->clk_gpu_0)
			|| IS_ERR(gpu_clkgen->clk_gpu_1)) {
		dev_err(&pdev->dev, "failed to get clock pointer\n");
		return -EFAULT;
	}
	return 0;
}

static int gpu_clkgen_probe(struct platform_device *pdev)
{
	struct gpu_clkgen *gpu_clkgen;
	struct clk_init_data init;
	const char *parent_name = "clk_gpu";
	const char *clk_name;
	struct resource *mem;
	struct clk *clk;

	dprintk(3, "%s, %d\n", __func__, __LINE__);
	gpu_clkgen = devm_kzalloc(&pdev->dev, sizeof(*gpu_clkgen), GFP_KERNEL);
	if (!gpu_clkgen)
		return -ENOMEM;

	dprintk(3, "%s, %d\n", __func__, __LINE__);
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	gpu_clkgen->base = devm_ioremap_resource(&pdev->dev, mem);
	if (IS_ERR(gpu_clkgen->base))
		return PTR_ERR(gpu_clkgen->base);

	dprintk(3, "%s, %d\n", __func__, __LINE__);
	clk_name = pdev->dev.of_node->name;
	of_property_read_string(pdev->dev.of_node, "clock-output-names",
			&clk_name);

	dprintk(3, "dev_get_drvdata=%p\n", dev_get_drvdata(&pdev->dev));
	dev_set_drvdata(&pdev->dev, gpu_clkgen);
	gpu_parse_clk_cfg(&pdev->dev);

	init.name = clk_name;
	init.ops = &gpu_clkgen_ops;
	init.flags = CLK_GET_RATE_NOCACHE;
	init.parent_names = &parent_name;
	init.num_parents = 1;

	dprintk(3, "%s, %d\n", __func__, __LINE__);
	gpu_clkgen->clk_hw.init = &init;
	clk = devm_clk_register(&pdev->dev, &gpu_clkgen->clk_hw);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	gpu_clkgen->debugfs_dir = debugfs_create_dir(pdev->name, NULL);
	if (ERR_PTR(-ENODEV) == gpu_clkgen->debugfs_dir) {
		dprintk(0, "create debugfs dir failed\n");
	} else if (NULL != gpu_clkgen->debugfs_dir) {
		debugfs_create_file("test_clk", 0600,
			gpu_clkgen->debugfs_dir, gpu_clkgen, &gpu_clkgen_fops);
	}

	return of_clk_add_provider(pdev->dev.of_node, of_clk_src_simple_get,
			clk);
}

static int gpu_clkgen_remove(struct platform_device *pdev)
{
	struct gpu_clkgen *gpu_clkgen = dev_get_drvdata(&pdev->dev);

	of_clk_del_provider(pdev->dev.of_node);
	if (NULL != gpu_clkgen->debugfs_dir)
		debugfs_remove_recursive(gpu_clkgen->debugfs_dir);

	return 0;
}

static const struct of_device_id gpu_clkgen_ids[] = {
	{ .compatible = "meson, gpu-clkgen-1.00.a" },
	{ },
};
MODULE_DEVICE_TABLE(of, gpu_clkgen_ids);

static struct platform_driver gpu_clkgen_driver = {
	.driver = {
		.name = "meson-gpu-clkgen",
		.owner = THIS_MODULE,
		.of_match_table = gpu_clkgen_ids,
	},
	.probe = gpu_clkgen_probe,
	.remove = gpu_clkgen_remove,
};
module_platform_driver(gpu_clkgen_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Amlogic SH, MM");
MODULE_DESCRIPTION("Driver for the Amlogic GPU clk");
