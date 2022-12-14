// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "mfh: " fmt

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/reset.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <asm/cacheflush.h>
#include <linux/firmware.h>
#include <linux/arm-smccc.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/amlogic/scpi_protocol.h>
#include <linux/mailbox_client.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/pm_runtime.h>
#include <linux/of_fdt.h>
#include <linux/pm_domain.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-contiguous.h>
#include <linux/clk-provider.h>
#include "mfh_module.h"
#include "mfh_sysfs.h"

#define AMLOGIC_MFH_BOOTUP 0x8200004E
#define AMLOGIC_MFH_BOOT   0x82000097
#define AMLOGIC_MFH_RESET  0x820000a8
#define AMLOGIC_M4_BANK    0xFCB87430
#define PWR_START           1
#define PWR_STOP            2
#define PWR_RESET           3

#define MFH_IOC_MAGIC  'H'
#define MFH_FIRMWARE_START     _IOWR(MFH_IOC_MAGIC, 1, struct mfh_info)
#define MFH_CMD_SEND           _IOWR(MFH_IOC_MAGIC, 2, struct mfh_msg_buf)
#define MFH_CMD_LISTEN         _IOWR(MFH_IOC_MAGIC, 3, struct mfh_msg_buf)
#define MFH_FIRMWARE_STOP      _IOWR(MFH_IOC_MAGIC, 5, struct mfh_info)

#define DTS_PARAM_CONFIGED_NO      0
#define DTS_PARAM_CONFIGED_YES     1
#define DTS_PARAM_CONFIGED_UNKNOWN 2

struct mfh_info {
	int cpuid;
	char name[30];
};

struct mfh_struct {
	phys_addr_t base;
	phys_addr_t size;
	struct clk *clkm40;
	struct clk *clkm41;
	struct clk *clkm4;
	struct clk *clkm4_pll;
	struct device *deva;
	struct device *devb;
	struct device *p_dev;
	void __iomem *vaddr;
	void __iomem *health_reg[2];
	unsigned int m4hang[2];
	unsigned int health_check_period;
	unsigned long online;
	struct task_struct *health_monitor_task;
	int m4_cnt;
	int addr_offset;
	int mem_region_configed;
	int power_domain_configed;
	int clock_configed;
};

struct mfh_struct mfh_dev;

static int mfh_startup(struct mfh_struct *mfh_struct,
			struct mfh_info *mfh_info);

static int __init mfh_reserve_res_setup(struct reserved_mem *rmem)
{
	phys_addr_t align = PAGE_SIZE;
	phys_addr_t mask = align - 1;

	if ((rmem->base & mask) || (rmem->size & mask)) {
		pr_err("Reserved memory: incorrect alignment of region\n");
		return -EINVAL;
	}
	mfh_dev.base = rmem->base;
	mfh_dev.size = rmem->size;
	pr_info("Reserved memory: created fb at 0x%p, size %ld KB\n",
		     (void *)rmem->base, (unsigned long)rmem->size / 1024);
	return 0;
}

RESERVEDMEM_OF_DECLARE(mfh_reserved, "amlogic, aml_mfh_reserve_mem",
	mfh_reserve_res_setup);

static void mfh_power_reset(int cpuid, bool power_off)
{
	struct arm_smccc_res res = {0};

	if (power_off)
		arm_smccc_smc(AMLOGIC_MFH_BOOT, cpuid, 0,
			0, PWR_STOP, 0, 0, 0, &res);
	else
		arm_smccc_smc(AMLOGIC_MFH_BOOT, cpuid, 0,
			0, PWR_RESET, 0, 0, 0, &res);
}

static int mfh_load_firmware(struct mfh_struct *mfh_struct,
			      struct mfh_info *mfh_info)
{
	void __iomem *vaddr = mfh_struct->vaddr;
	phys_addr_t phy_addr = mfh_struct->base;
	const struct firmware *firmware;
	size_t size;
	int ret = 0;
	int cpuid = mfh_info->cpuid;

	ret = request_firmware(&firmware, mfh_info->name, mfh_struct->p_dev);
	if (ret < 0) {
		pr_err("req firmware fail %d\n", ret);
		return ret;
	}

	size = firmware->size;

	if (mfh_struct->mem_region_configed != DTS_PARAM_CONFIGED_NO) {
		/* defined reserved memory region,
		 * use its virtual & physical address
		 */
		if (cpuid == 0) {
			if (size > mfh_struct->addr_offset) {
				pr_err("m4a firmware size overflow\n");
				ret = -ENOMEM;
				goto fail;
			}
		} else if (cpuid == 1) {
			if (size > mfh_struct->size - mfh_struct->addr_offset) {
				pr_err("m4b firmware size overflow\n");
				ret = -ENOMEM;
				goto fail;
			}
			phy_addr += mfh_struct->addr_offset;
			vaddr += mfh_struct->addr_offset;
		}
	} else {
		/* no reserved memory region ,
		 * alloc continus physical memory in kernel
		 */
		vaddr = devm_kzalloc(mfh_struct->p_dev, size, GFP_KERNEL);
		if (!vaddr) {
			release_firmware(firmware);
			pr_err("memory request fail\n");
			return -ENOMEM;
		}
		phy_addr = virt_to_phys(vaddr);
		mfh_struct->vaddr = vaddr;
		mfh_struct->base = phy_addr;
		mfh_struct->size = size;
	}

	memcpy(vaddr, firmware->data, size);

	dma_sync_single_for_cpu(mfh_struct->p_dev,
				(dma_addr_t)(phy_addr),
				size, DMA_FROM_DEVICE);
fail:
	release_firmware(firmware);
	return ret;
}

static int mfh_startup(struct mfh_struct *mfh_struct,
			struct mfh_info *mfh_info)
{
	struct arm_smccc_res res = {0};
	struct clk *clkm4 = mfh_struct->clkm4;
	struct clk *clkm4_pll = mfh_struct->clkm4_pll;
	unsigned long pclk_rate = 1200 * 1000 * 1000;
	unsigned long clk_rate = 300 * 1000 * 1000;
	phys_addr_t phy_addr = mfh_struct->base;
	int cpuid = mfh_info->cpuid;
	struct device *pd_dev = mfh_struct->deva;
	int ret = 0;

	pr_debug("mfh id %d\n\n", cpuid);

	/*set mpll clk, or clkm4 can't set 300M*/
	if (mfh_struct->clock_configed != DTS_PARAM_CONFIGED_NO) {
		if (!IS_ERR_OR_NULL(clkm4_pll)) {
			if (!__clk_is_enabled(clkm4_pll))
				clk_set_rate(clkm4_pll, pclk_rate);
		}
		if (!__clk_is_enabled(clkm4))
			clk_set_rate(clkm4, clk_rate);
		clk_prepare_enable(clkm4);
	}

	if (cpuid == 1) {
		pd_dev = mfh_struct->devb;
		phy_addr += mfh_struct->addr_offset;
	}

	/*enable power domain*/
	if (mfh_struct->power_domain_configed != DTS_PARAM_CONFIGED_NO)
		ret = pm_runtime_get_sync(pd_dev);

	if (mfh_struct->mem_region_configed != DTS_PARAM_CONFIGED_NO) {
		arm_smccc_smc(AMLOGIC_MFH_BOOT, cpuid, phy_addr,
			AMLOGIC_M4_BANK, PWR_START, 0, 0, 0, &res);
	} else {
		arm_smccc_smc(AMLOGIC_MFH_BOOTUP, mfh_struct->base,
		mfh_struct->size, 0, 0, 0, 0, 0, &res);
		devm_kfree(mfh_struct->p_dev, mfh_struct->vaddr);
	}
	return ret;
}

static int mfh_stop(struct mfh_struct *mfh_struct,
		    struct mfh_info *mfh_info)
{
	struct clk *clkm4 = mfh_struct->clkm4;
	struct arm_smccc_res res = {0};
	int cpuid;
	int ret = 0;
	struct device *pd_dev;

	cpuid = mfh_info->cpuid;
	arm_smccc_smc(AMLOGIC_MFH_BOOT, cpuid,
			0, 0, PWR_STOP, 0, 0, 0, &res);
	clk_disable_unprepare(clkm4);
	/*disable power domain*/
	pd_dev = cpuid == 0 ? mfh_struct->deva : mfh_struct->devb;
	ret = pm_runtime_put_sync(pd_dev);
	return ret;
}

static bool valid_cpuid(int cpuid)
{
	return (cpuid == 0 || cpuid == 1);
}

static long mfh_miscdev_ioctl(struct file *fp, unsigned int cmd,
			       unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct mfh_struct *mfh_struct = fp->private_data;
	struct mfh_info mfh_info;
	int ret = 0;
	int cpuid = -1;

	ret = copy_from_user((void *)&mfh_info,
			argp, sizeof(mfh_info));

	if (ret < 0)
		return ret;
	cpuid = mfh_info.cpuid;
	if (!valid_cpuid(cpuid)) {
		pr_err("Invalid cpu id %d\n", cpuid);
		return -EINVAL;
	}
	switch (cmd) {
	case MFH_FIRMWARE_START:
		if (mfh_struct->mem_region_configed != DTS_PARAM_CONFIGED_NO) {
			mfh_power_reset(cpuid, PWR_START);
			ret = mfh_load_firmware(mfh_struct, &mfh_info);
			if (ret) {
				pr_err("load firmware error\n");
				return ret;
			}
			mfh_startup(mfh_struct, &mfh_info);
		} else {
			struct arm_smccc_res res = {0};

			mfh_load_firmware(mfh_struct, &mfh_info);
			arm_smccc_smc(AMLOGIC_MFH_BOOTUP, mfh_struct->base,
			      mfh_struct->size, 0, 0, 0, 0, 0, &res);
			devm_kfree(mfh_struct->p_dev, mfh_struct->vaddr);
		}
	break;
	case MFH_FIRMWARE_STOP:
		mfh_power_reset(cpuid, PWR_STOP);
		mfh_stop(mfh_struct, &mfh_info);
	break;
	default:
	break;
	};
	return ret;
}

static int mfh_miscdev_open(struct inode *node, struct file *file)
{
	file->private_data = &mfh_dev;
	return 0;
}

static const struct file_operations mfh_miscdev_fops = {
	.owner = THIS_MODULE,
	.open = mfh_miscdev_open,
	.unlocked_ioctl = mfh_miscdev_ioctl,
	.compat_ioctl = mfh_miscdev_ioctl,
};

static struct miscdevice mfh_miscdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "mfh",
	.fops	= &mfh_miscdev_fops,
};

static void __iomem *mfh_get_memory(struct device *dev,
				     struct mfh_struct *mfh_struct)
{
	void __iomem *vaddr = NULL;
	struct page **pages = NULL;
	u32 npages = DIV_ROUND_UP(mfh_struct->size, PAGE_SIZE);
	phys_addr_t phys = mfh_struct->base;
	int i = 0;

	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return NULL;
	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}

	vaddr = vmap(pages, npages, VM_MAP, pgprot_dmacoherent(PAGE_KERNEL));
	if (!vaddr) {
		pr_err("vmaped fail, size: %d\n",
		       npages << PAGE_SHIFT);
		vfree(pages);
		return NULL;
	}

	vfree(pages);
	mfh_struct->vaddr = vaddr;
	of_property_read_u32(dev->of_node, "mfh-addr-offset",
			     &mfh_struct->addr_offset);
	pr_debug("Init vaddr %lx physaddr %lx\n",
		(unsigned long)vaddr, (unsigned long)mfh_struct->base);
	return vaddr;
}

static int mfh_parse_dt(struct device *dev, struct mfh_struct *mfh_struct)
{
	const char *clk_name[10] = { "m4_clk0", "m4_clk1", "m4_clk", "m4_pll" };
	const char *pd_name[10] = { "m4a-core", "m4b-core" };
	struct device *deva;
	struct device *devb;
	void __iomem *vaddr;
	int ret = 0;

	mfh_struct->mem_region_configed = DTS_PARAM_CONFIGED_UNKNOWN;
	mfh_struct->power_domain_configed = DTS_PARAM_CONFIGED_UNKNOWN;
	mfh_struct->clock_configed = DTS_PARAM_CONFIGED_UNKNOWN;

	/* get memory region parameter is configed */
	ret = of_property_read_u32(dev->of_node,
				   "memory-region-configed",
				   &mfh_struct->mem_region_configed);
	if (ret < 0)
		dev_err(dev, "Can't retrieve memory-region-configed\n");

	/* get power domains parameter is configed */
	ret = of_property_read_u32(dev->of_node,
				   "power-domain-configed",
				   &mfh_struct->power_domain_configed);
	if (ret < 0)
		dev_err(dev, "Can't retrieve power-domain-configed\n");

	/* get clocks parameter is configed */
	ret = of_property_read_u32(dev->of_node,
				   "clock-configed",
				   &mfh_struct->clock_configed);
	if (ret < 0)
		dev_err(dev, "Can't retrieve clock-configed\n");

	ret = of_property_read_u32(dev->of_node, "mfh-cnt", &mfh_struct->m4_cnt);
	if (ret < 0)
		dev_err(dev, "Can't retrieve mfh-cnt\n");

	ret = of_property_read_u32(dev->of_node, "mfh-monitor-period-ms",
		&mfh_struct->health_check_period);
	if (ret && ret != -EINVAL)
		dev_err(dev, "of get mfh-monitor-period-ms property failed\n");

	/* attach clocks */
	if (mfh_struct->clock_configed != DTS_PARAM_CONFIGED_NO) {
		mfh_struct->clkm40 = of_clk_get_by_name(dev->of_node, clk_name[0]);
		mfh_struct->clkm41 = of_clk_get_by_name(dev->of_node, clk_name[1]);
		mfh_struct->clkm4 = of_clk_get_by_name(dev->of_node, clk_name[2]);
		if (IS_ERR_OR_NULL(mfh_struct->clkm4)) {
			pr_err("invalid m4 clk\n");
			return -EINVAL;
		}
		mfh_struct->clkm4_pll = of_clk_get_by_name(dev->of_node, clk_name[3]);
	}

	/* attach power domains */
	if (mfh_struct->power_domain_configed != DTS_PARAM_CONFIGED_NO) {
		deva = genpd_dev_pm_attach_by_name(dev, pd_name[0]);
		if (!deva) {
			pr_err("power domain a not match\n");
			return -EINVAL;
		}
		deva->parent = mfh_struct->p_dev;
		devb = genpd_dev_pm_attach_by_name(dev, pd_name[1]);
		if (!devb) {
			pr_err("power domain b not match\n");
			return -EINVAL;
		}
		devb->parent = mfh_struct->p_dev;
		mfh_struct->deva = deva;
		mfh_struct->devb = devb;
	}

	/* memory region mapping */
	if (mfh_struct->power_domain_configed != DTS_PARAM_CONFIGED_NO) {
		vaddr = mfh_get_memory(dev, mfh_struct);
		if (!vaddr)
			return -ENOMEM;
	}

	return 0;
}

static void get_m4_health_regs(struct platform_device *pdev)
{
	struct mfh_struct *mfh = platform_get_drvdata(pdev);
	struct resource *res;
	int i;

	for (i = 0; i < mfh->m4_cnt; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			dev_err(&pdev->dev, "failed to get m4-%d status register.\n", i);
			return;
		}
		mfh->health_reg[i] = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(mfh->health_reg[i]))
			mfh->health_reg[i] = NULL;
	}

	mfh->health_monitor_task = NULL;
}

static enum m4_health_status get_m4_health_status(struct mfh_struct *mfh)
{
	enum m4_health_status ret = M4_GOOD;
	static u32 last_cnt[2] = {0};
	u32 this_cnt[2];

	switch (mfh->online & 0x03) {
	case M4_NONE:
		break;
	case M4A_ONLINE:
		this_cnt[M4A] = readl(mfh->health_reg[M4A]);
		pr_debug("[%s]m4a[%u %u]\n", __func__, last_cnt[M4A], this_cnt[M4A]);
		if (this_cnt[M4A] == last_cnt[M4A])
			ret = M4A_HANG;
		last_cnt[M4A] = this_cnt[M4A];
		break;
	case M4B_ONLINE:
		this_cnt[M4B] = readl(mfh->health_reg[M4B]);
		pr_debug("[%s]m4b[%u %u]\n", __func__, last_cnt[M4B], this_cnt[M4B]);
		if (this_cnt[M4B] == last_cnt[M4B])
			ret = M4B_HANG;
		last_cnt[M4B] = this_cnt[M4B];
		break;
	case M4AB_ONLINE:
		this_cnt[M4A] = readl(mfh->health_reg[M4A]);
		this_cnt[M4B] = readl(mfh->health_reg[M4B]);
		pr_debug("[%s]m4a[%u %u] m4b[%u %u]\n", __func__, last_cnt[M4A], this_cnt[M4A],
			last_cnt[M4B], this_cnt[M4B]);
		if (this_cnt[M4A] == last_cnt[M4A] && this_cnt[M4B] == last_cnt[M4B])
			ret = M4AB_HANG;
		else if (this_cnt[M4A] == last_cnt[M4A])
			ret = M4A_HANG;
		else if (this_cnt[M4B] == last_cnt[M4B])
			ret = M4B_HANG;
		last_cnt[M4A] = this_cnt[M4A];
		last_cnt[M4B] = this_cnt[M4B];
		break;
	}
	return ret;
}

static int health_monitor_thread(void *data)
{
	struct mfh_struct *mfh = data;
	char str[20], *envp[] = { str, NULL };

	set_freezable();
	while (!kthread_should_stop()) {
		if (!mfh->health_check_period)
			return 0;
		msleep(mfh->health_check_period);

		switch (get_m4_health_status(mfh)) {
		case M4_GOOD:
			mfh->m4hang[M4A] = 0;
			mfh->m4hang[M4B] = 0;
			break;
		case M4A_HANG:
			snprintf(str, sizeof(str), "ACTION=M4_WTD_A");
			mfh->m4hang[M4A] = 1;
			mfh->m4hang[M4B] = 0;
			kobject_uevent_env(&mfh->deva->kobj, KOBJ_CHANGE, envp);
			pr_debug("[%s][M4A_HANG]\n", __func__);
			break;
		case M4B_HANG:
			snprintf(str, sizeof(str), "ACTION=M4_WTD_B");
			mfh->m4hang[M4A] = 0;
			mfh->m4hang[M4B] = 1;
			kobject_uevent_env(&mfh->devb->kobj, KOBJ_CHANGE, envp);
			pr_debug("[%s][M4B_HANG]\n", __func__);
			break;
		case M4AB_HANG:
			snprintf(str, sizeof(str), "ACTION=M4_WTD_WHOLE");
			mfh->m4hang[M4A] = 1;
			mfh->m4hang[M4B] = 1;
			kobject_uevent_env(&mfh->deva->kobj, KOBJ_CHANGE, envp);
			pr_debug("[%s][M4AB_HANG]\n", __func__);
			break;
		}
	}
	return 0;
}

static int create_health_monitor_task(struct mfh_struct *mfh)
{
	int ret = 0;

	mfh->health_monitor_task = kthread_run(health_monitor_thread,
				mfh, "m4-health-monitor");
	if (IS_ERR(mfh->health_monitor_task)) {
		ret = PTR_ERR(mfh->health_monitor_task);
		pr_err("Unable to run kthread err %d\n", ret);
	}
	return ret;
}

void mfh_poweron(struct device *dev, int cpuid, bool poweron)
{
	struct mfh_struct *mfh_struct = dev_get_drvdata(dev);
	struct mfh_info mfh_info;

	if (!valid_cpuid(cpuid)) {
		pr_err("Invalid cpu id %d\n", cpuid);
		return;
	}

	mfh_info.cpuid = cpuid;
	if (poweron) {
		if (cpuid)
			strncpy(mfh_info.name, "m4b.bin", 30);
		else
			strncpy(mfh_info.name, "m4a.bin", 30);
		if (mfh_struct->mem_region_configed != DTS_PARAM_CONFIGED_NO)
			mfh_power_reset(cpuid, poweron);
		mfh_load_firmware(mfh_struct, &mfh_info);
		mfh_startup(mfh_struct, &mfh_info);

		set_bit(cpuid, &mfh_struct->online);
		if (mfh_struct->health_reg[cpuid] && !mfh_struct->health_monitor_task)
			create_health_monitor_task(mfh_struct);
	} else {
		mfh_power_reset(cpuid, poweron);
		mfh_stop(mfh_struct, &mfh_info);

		clear_bit(cpuid, &mfh_struct->online);
		if (!mfh_struct->online) {
			kthread_stop(mfh_struct->health_monitor_task);
			mfh_struct->health_monitor_task = NULL;
		}
	}
}

static int mfh_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;

	mfh_dev.p_dev = dev;
	ret = mfh_parse_dt(dev, &mfh_dev);
	if (ret) {
		pr_info("parse device tree fail\n");
		return ret;
	}
	ret = misc_register(&mfh_miscdev);
	if (ret) {
		pr_info("mfh probe fail\n");
		return ret;
	}

	platform_set_drvdata(pdev, &mfh_dev);
	get_m4_health_regs(pdev);
	mfh_dev_create_file(dev);
	pr_info("mfh probe end\n");
	return ret;
}

static int mfh_platform_remove(struct platform_device *pdev)
{
	dev_set_drvdata(mfh_miscdev.this_device, NULL);
	misc_deregister(&mfh_miscdev);
	platform_set_drvdata(pdev, NULL);
	mfh_dev_remove_file(&pdev->dev);
	return 0;
}

static const struct of_device_id mfh_device_id[] = {
	{
		.compatible = "amlogic, mfh",
	},
	{}
};

static struct platform_driver mfh_platform_driver = {
	.driver = {
		.name  = "mfh",
		.owner = THIS_MODULE,
		.of_match_table = mfh_device_id,
	},
	.probe  = mfh_platform_probe,
	.remove = mfh_platform_remove,
};
module_platform_driver(mfh_platform_driver);

MODULE_AUTHOR("Amlogic M4");
MODULE_DESCRIPTION("M4 Host Module Driver");
MODULE_LICENSE("GPL v2");
