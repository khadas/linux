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

#include <linux/kthread.h>
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
#include "aml-sha-dma.h"
#include "aml-aes-dma.h"
#include "aml-tdes-dma.h"
#include "aml-sm4-dma.h"

#define ENABLE_SHA	(1)
#define ENABLE_AES	(1)
#define ENABLE_TDES	(1)
#define ENABLE_SM4	(1)
#define ENABLE_CRYPTO_DEV (1)
#define AML_DMA_QUEUE_LENGTH (50)
static struct dentry *aml_dma_debug_dent;
int debug = 3;
int disable_link_mode;

void __iomem *cryptoreg;

struct meson_dma_data {
	u32 thread;
	u32 status;
	u8  link_mode;
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
struct meson_dma_data meson_gxl_data = {
	.thread = GXL_DMA_T0,
	.status = GXL_DMA_STS0,
	.link_mode = 0,
};
#endif

struct meson_dma_data meson_txlx_data = {
	.thread = TXLX_DMA_T0,
	.status = TXLX_DMA_STS0,
	.link_mode = 0,
};

struct meson_dma_data meson_p1_data = {
	.thread = TXLX_DMA_T0,
	.status = TXLX_DMA_STS0,
	.link_mode = 1,
};
#ifdef CONFIG_OF
static const struct of_device_id aml_dma_dt_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{	.compatible = "amlogic,aml_gxl_dma",
		.data = &meson_gxl_data,
	},
#endif
	{	.compatible = "amlogic,aml_txlx_dma",
		.data = &meson_txlx_data,
	},
	{	.compatible = "amlogic,aml_p1_dma",
		.data = &meson_p1_data,
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
		file = debugfs_create_u32("disable_link_mode", 0644,
					  aml_dma_debug_dent, &disable_link_mode);
		if (!file) {
			dev_err(dev, "can not create entry in debugfs directory\n");
			return -ENOMEM;
		}
	}
	return 0;
}

#if !DMA_IRQ_MODE
static int aml_dma_queue_manage(void *data)
{
	struct aml_dma_dev *dev = (struct aml_dma_dev *)data;
	struct crypto_async_request *async_req;
	struct crypto_async_request *backlog;
	int ret = 0;
	unsigned long flags;

	do {
		__set_current_state(TASK_INTERRUPTIBLE);

		spin_lock_irqsave(&dev->queue_lock, flags);
		backlog = crypto_get_backlog(&dev->queue);
		async_req = crypto_dequeue_request(&dev->queue);
		spin_unlock_irqrestore(&dev->queue_lock, flags);

		if (backlog)
			backlog->complete(backlog, -EINPROGRESS);

		if (async_req) {
			__set_current_state(TASK_RUNNING);
			if (crypto_tfm_alg_type(async_req->tfm) ==
			    CRYPTO_ALG_TYPE_AHASH) {
				struct ahash_request *req =
					ahash_request_cast(async_req);
				ret = aml_sha_process(req);
				aml_sha_finish_req(req, ret);
			} else {
				struct ablkcipher_request *req =
				ablkcipher_request_cast(async_req);
				const char *driver_name =
				crypto_tfm_alg_driver_name(async_req->tfm);

				if (strstr(driver_name, "aes"))
					ret = aml_aes_process(req);
				else if (strstr(driver_name, "des"))
					ret = aml_tdes_process(req);
				else if (strstr(driver_name, "sm4"))
					ret = aml_sm4_process(req);
				else
					ret = -EINVAL;
			}
			async_req->complete(async_req, ret);
			continue;
		}
		schedule();
	} while (!kthread_should_stop());

	return 0;
}
#endif

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
	dma_dd->link_mode = priv_data->link_mode;
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
	spin_lock_init(&dma_dd->dma_lock);
#if !DMA_IRQ_MODE
	crypto_init_queue(&dma_dd->queue, AML_DMA_QUEUE_LENGTH);
	spin_lock_init(&dma_dd->queue_lock);
	dma_dd->kthread = kthread_run(aml_dma_queue_manage,
				      dma_dd, "aml_crypto");
	if (IS_ERR(dma_dd->kthread)) {
		err = PTR_ERR(dma_dd->kthread);
		goto dma_err;
	}
#endif
	err = aml_dma_init_dbgfs(dev);
	if (err)
		goto dma_err;

	dev_info(dev, "Aml dma\n");

	err = of_platform_populate(np, NULL, NULL, dev);
	if (err != 0)
		goto dma_err;

	return err;

dma_err:
#if !DMA_IRQ_MODE
	if (dma_dd && dma_dd->kthread)
		kthread_stop(dma_dd->kthread);
#endif
	debugfs_remove_recursive(aml_dma_debug_dent);
	dev_err(dev, "initialization failed.\n");

	return err;
}

static int aml_dma_remove(struct platform_device *pdev)
{
	struct aml_dma_dev *dma_dd;
	struct device *dev = &pdev->dev;

	dma_dd = platform_get_drvdata(pdev);
	if (!dma_dd)
		return -ENODEV;
#if !DMA_IRQ_MODE
	kthread_stop(dma_dd->kthread);
#endif
	of_platform_depopulate(dev);
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

#if ENABLE_SHA
	ret = aml_sha_driver_init();
	if (ret)
		goto sha_init_failed;
#endif
#if ENABLE_TDES
	ret = aml_tdes_driver_init();
	if (ret)
		goto tdes_init_failed;
#endif
#if ENABLE_AES
	ret = aml_aes_driver_init();
	if (ret)
		goto aes_init_failed;
#endif
#if ENABLE_CRYPTO_DEV
	ret = aml_crypto_device_driver_init();
	if (ret)
		goto crypto_dev_init_failed;
#endif
#if ENABLE_SM4
	ret = aml_sm4_driver_init();
	if (ret)
		goto sm4_init_failed;
#endif
	ret = platform_driver_register(&aml_dma_driver);
	if (ret)
		goto plat_init_failed;

	return ret;

plat_init_failed:
#if ENABLE_SM4
sm4_init_failed:
	aml_sm4_driver_exit();
#endif
#if ENABLE_CRYPTO_DEV
crypto_dev_init_failed:
	aml_crypto_device_driver_exit();
#endif
#if ENABLE_AES
aes_init_failed:
	aml_aes_driver_exit();
#endif
#if ENABLE_TDES
tdes_init_failed:
	aml_tdes_driver_exit();
#endif
#if ENABLE_SHA
sha_init_failed:
	aml_sha_driver_exit();
#endif
	return ret;
}
module_init(aml_dma_driver_init);

static void __exit aml_dma_driver_exit(void)
{
	platform_driver_unregister(&aml_dma_driver);
#if ENABLE_SHA
	aml_sha_driver_exit();
#endif
#if ENABLE_TDES
	aml_tdes_driver_exit();
#endif
#if ENABLE_AES
	aml_aes_driver_exit();
#endif
#if ENABLE_CRYPTO_DEV
	aml_crypto_device_driver_exit();
#endif
#if ENABLE_SM4
	aml_sm4_driver_exit();
#endif
}
module_exit(aml_dma_driver_exit);

MODULE_DESCRIPTION("Aml crypto DMA support.");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("matthew.shyu <matthew.shyu@amlogic.com>");
