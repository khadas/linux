// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "bl40: " fmt

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
#include "bl40_module.h"

#define AMLOGIC_BL40_BOOTUP	0x8200004E
struct bl40_info {
	char name[30];
};

struct bl40_msg {
	struct list_head list;
	struct bl40_msg_buf msg_buf;
	struct completion complete;
};

/*for listen list*/
spinlock_t lock;
struct device *device;
static LIST_HEAD(bl40_list);
#define BL40_IOC_MAGIC  'H'
#define BL40_FIRMWARE_LOAD      _IOWR(BL40_IOC_MAGIC, 1, struct bl40_info)
#define BL40_CMD_SEND           _IOWR(BL40_IOC_MAGIC, 2, struct bl40_msg_buf)
#define BL40_CMD_LISTEN         _IOWR(BL40_IOC_MAGIC, 3, struct bl40_msg_buf)

void bl40_rx_msg(void *msg, int size)
{
	struct list_head *list;
	struct bl40_msg *bl40_msg;
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);
	if (list_empty(&bl40_list)) {
		spin_unlock_irqrestore(&lock, flags);
		dev_err(device, "List is NULL\n");
		return;
	}
	list_for_each(list, &bl40_list) {
		bl40_msg = list_entry(list, struct bl40_msg, list);
		bl40_msg->msg_buf.size = size;
		memcpy(bl40_msg->msg_buf.buf, msg, size);
		complete(&bl40_msg->complete);
		break;
	}
	spin_unlock_irqrestore(&lock, flags);
}

static long bl40_miscdev_ioctl(struct file *fp, unsigned int cmd,
			       unsigned long arg)
{
	int ret = 0;
	const struct firmware *firmware;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case BL40_FIRMWARE_LOAD: {
		struct bl40_info bl40_info;
		unsigned long phy_addr;
		void *virt_addr = NULL;
		struct arm_smccc_res res = {0};
		size_t size;

		ret = copy_from_user((void *)&bl40_info,
				     argp, sizeof(bl40_info));
		if (ret < 0)
			return ret;
		ret = request_firmware(&firmware, bl40_info.name, device);
		if (ret < 0) {
			pr_err("req firmware fail %d\n", ret);
			return ret;
		}

		size = firmware->size;
		virt_addr = devm_kzalloc(device, size, GFP_KERNEL);
		if (!virt_addr) {
			release_firmware(firmware);
			pr_err("memory request fail\n");
			return -ENOMEM;
		}
		memcpy(virt_addr, firmware->data, size);
		release_firmware(firmware);
		dma_map_single(device, virt_addr, size, DMA_FROM_DEVICE);
		phy_addr = virt_to_phys(virt_addr);

		/* unlock bl40 */
		scpi_unlock_bl40();
		arm_smccc_smc(AMLOGIC_BL40_BOOTUP, phy_addr,
			      size, 0, 0, 0, 0, 0, &res);
		pr_info("free memory\n");
		devm_kfree(device, virt_addr);
		ret = res.a0;
	}
	break;
	case BL40_CMD_SEND: {
		struct bl40_msg_buf bl40_buf;

		ret = copy_from_user((void *)&bl40_buf,
				     argp, sizeof(bl40_buf));
		pr_debug("Enter BL40_CMD_SEND\n");
		scpi_send_bl40(SCPI_CMD_BL4_SEND, &bl40_buf);
		ret = copy_to_user(argp, &bl40_buf, sizeof(bl40_buf));
	}
	break;
	case BL40_CMD_LISTEN: {
		struct bl40_msg bl40_msg;
		unsigned long flags;

		init_completion(&bl40_msg.complete);
		spin_lock_irqsave(&lock, flags);
		list_add_tail(&bl40_msg.list, &bl40_list);
		spin_unlock_irqrestore(&lock, flags);
		wait_for_completion(&bl40_msg.complete);
		ret = copy_to_user(argp, &bl40_msg.msg_buf,
				   sizeof(struct bl40_msg_buf));
		spin_lock_irqsave(&lock, flags);
		list_del(&bl40_msg.list);
		spin_unlock_irqrestore(&lock, flags);
	}
	break;
	default:
		pr_info("Not have this cmd\n");
	break;
	};
	pr_debug("bl40 ioctl\n");
	return ret;
}

static const struct file_operations bl40_miscdev_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.unlocked_ioctl = bl40_miscdev_ioctl,
	.compat_ioctl = bl40_miscdev_ioctl,
};

static struct miscdevice bl40_miscdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "bl40",
	.fops	= &bl40_miscdev_fops,
};

static int bl40_platform_probe(struct platform_device *pdev)
{
	int ret;

	device = &pdev->dev;
	platform_set_drvdata(pdev, NULL);
	ret = misc_register(&bl40_miscdev);
	pr_info("bl40 probe\n");
	return ret;
}

static int bl40_platform_remove(struct platform_device *pdev)
{
	misc_deregister(&bl40_miscdev);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id bl40_device_id[] = {
	{
		.compatible = "amlogic, bl40-bootup",
	},
	{}
};

static struct platform_driver bl40_platform_driver = {
	.driver = {
		.name  = "bl40",
		.owner = THIS_MODULE,
		.of_match_table = bl40_device_id,
	},
	.probe  = bl40_platform_probe,
	.remove = bl40_platform_remove,
};
module_platform_driver(bl40_platform_driver);

MODULE_AUTHOR("Amlogic Bl40");
MODULE_DESCRIPTION("BL40 Boot Module Driver");
MODULE_LICENSE("GPL v2");
