// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/hw_random.h>
#include <linux/platform_device.h>

#include <linux/device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/crypto.h>
#include <linux/cryptohash.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <crypto/hash.h>
#include <crypto/internal/hash.h>
#include <linux/of_platform.h>
#include "aml-crypto-dma.h"

static struct dentry *aml_dma_debug_dent;
int debug = 2;

void __iomem *cryptoreg;

struct meson_dma_data {
	u32 thread;
	u32 status;
};

struct meson_dma_data meson_gxl_data = {
	.thread = GXL_DMA_T0,
	.status = GXL_DMA_STS0,
};

struct meson_dma_data meson_txlx_data = {
	.thread = TXLX_DMA_T0,
	.status = TXLX_DMA_STS0,
};

#ifdef CONFIG_OF
static const struct of_device_id aml_dma_dt_match[] = {
	{	.compatible = "amlogic,aml_gxl_dma",
		.data = &meson_gxl_data,
	},
	{	.compatible = "amlogic,aml_txlx_dma",
		.data = &meson_txlx_data,
	},
	{},
};

MODULE_DEVICE_TABLE(of, aml_dma_dt_match);
#else
#define aml_aes_dt_match NULL
#endif

static int aml_dma_init_dbgfs(struct device *dev)
{
	struct dentry *file = NULL;

	if (!aml_dma_debug_dent) {
		aml_dma_debug_dent = debugfs_create_dir("aml_dma", NULL);
		if (!aml_dma_debug_dent) {
			dev_err(dev, "can not create debugfs directory\n");
			return -ENOMEM;
		}
		file = debugfs_create_u32("debug", 0644,
					  aml_dma_debug_dent, &debug);
		if (!file) {
			dev_err(dev, "can not create entry in debugfs directory\n");
			return -ENOMEM;
		}
	}
	return 0;
}

static int aml_dma_probe(struct platform_device *pdev)
{
	struct aml_dma_dev *dma_dd;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct resource *res_base = 0;
	struct resource *res_irq = 0;
	const struct of_device_id *match;
	int err = -EPERM;
	const struct meson_dma_data *priv_data;

	dma_dd = devm_kzalloc(dev, sizeof(struct aml_dma_dev), GFP_KERNEL);
	if (!dma_dd) {
		err = -ENOMEM;
		goto dma_err;
	}

	match = of_match_device(aml_dma_dt_match, &pdev->dev);
	if (!match)
		goto dma_err;
	priv_data = match->data;
	dma_dd->thread = priv_data->thread;
	dma_dd->status = priv_data->status;
	res_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_base) {
		dev_err(dev, "error to get normal IORESOURCE_MEM.\n");
		goto dma_err;
	} else {
		cryptoreg = devm_ioremap_resource(dev, res_base);
		if (!cryptoreg) {
			dev_err(dev, "failed to remap crypto reg\n");
			goto dma_err;
		}
	}

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	dma_dd->irq = res_irq->start;
	dma_dd->dma_busy = 0;
	platform_set_drvdata(pdev, dma_dd);

	err = aml_dma_init_dbgfs(dev);
	if (err)
		goto dma_err;

	dev_info(dev, "Aml dma\n");

	err = of_platform_populate(np, NULL, NULL, dev);
	if (err != 0)
		goto dma_err;

	return err;

dma_err:
	debugfs_remove_recursive(aml_dma_debug_dent);
	dev_err(dev, "initialization failed.\n");

	return err;
}

static int aml_dma_remove(struct platform_device *pdev)
{
	struct aml_dma_dev *dma_dd;

	dma_dd = platform_get_drvdata(pdev);
	if (!dma_dd)
		return -ENODEV;

	debugfs_remove_recursive(aml_dma_debug_dent);

	return 0;
}

static struct platform_driver aml_dma_driver = {
	.probe		= aml_dma_probe,
	.remove		= aml_dma_remove,
	.driver		= {
		.name	= "aml_dma",
		.owner	= THIS_MODULE,
		.of_match_table = aml_dma_dt_match,
	},
};

static int __init aml_dma_driver_init(void)
{
	int ret;

	ret = aml_sha_driver_init();
	if (ret)
		return ret;

	ret = aml_tdes_driver_init();
	if (ret) {
		aml_sha_driver_exit();
		return ret;
	}

	ret = aml_aes_driver_init();
	if (ret) {
		aml_sha_driver_exit();
		aml_tdes_driver_exit();
		return ret;
	}

	ret = platform_driver_register(&aml_dma_driver);
	if (ret) {
		aml_sha_driver_exit();
		aml_tdes_driver_exit();
		aml_aes_driver_exit();
	}

	return ret;
}
module_init(aml_dma_driver_init);

static void __exit aml_dma_driver_exit(void)
{
	aml_sha_driver_exit();
	aml_tdes_driver_exit();
	aml_aes_driver_exit();

	platform_driver_unregister(&aml_dma_driver);
}
module_exit(aml_dma_driver_exit);

MODULE_DESCRIPTION("Aml crypto DMA support.");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("matthew.shyu <matthew.shyu@amlogic.com>");
