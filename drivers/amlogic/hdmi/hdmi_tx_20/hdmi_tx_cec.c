/*
 * drivers/amlogic/hdmi/hdmi_tx_20/hdmi_tx_cec.c
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


#include <linux/version.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/timer.h>

#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/amlogic/tvin/tvin.h>
#include <linux/wakelock_android.h>

#include <linux/amlogic/hdmi_tx/hdmi_tx_cec_20.h>
#include <linux/amlogic/hdmi_tx/hdmi_tx_module.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_irq.h>
#include "hw/mach_reg.h"
#include "hw/hdmi_tx_reg.h"
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/amlogic/pm.h>
#include <linux/of_address.h>

#define CEC_FRAME_DELAY		msecs_to_jiffies(400)
#define CEC_DEV_NAME		"cec"

static void __iomem *exit_reg;
static struct hdmitx_dev *hdmitx_device;
static struct workqueue_struct *cec_workqueue;
static unsigned char rx_msg[MAX_MSG];
static unsigned char rx_len;
static struct completion rx_complete;
#ifdef CONFIG_PM
static int cec_suspend;
#endif

static struct device *dbg_dev;
#define CEC_DEBUG	1

#define CEC_ERR(format, args...)			\
	{if (CEC_DEBUG && dbg_dev)			\
		dev_err(dbg_dev, format, ##args);	\
	}

#define CEC_INFO(format, args...)			\
	{if (CEC_DEBUG && dbg_dev)			\
		dev_info(dbg_dev, format, ##args);	\
	}

DEFINE_SPINLOCK(cec_input_key);

/* global variables */
struct cec_global_info_t cec_info;
bool cec_msg_dbg_en = 0;

/* CEC default setting */
static unsigned char *osd_name = "MBox";
static unsigned int vendor_id = 0x00;

/*
 * cec hw module init befor allocate logical address
 */
static void cec_pre_init(void)
{
	cec_pinmux_set();
	ao_cec_init();

	cec_arbit_bit_time_set(3, 0x118, 0);
	cec_arbit_bit_time_set(5, 0x000, 0);
	cec_arbit_bit_time_set(7, 0x2aa, 0);
}

static int cec_late_check_rx_buffer(void)
{
	int ret;
	struct delayed_work *dwork = &hdmitx_device->cec_work;

	ret = cec_rx_buf_check();
	if (!ret)
		return 0;
	/*
	 * start another check if rx buffer is full
	 */
	if ((-1) == cec_ll_rx(rx_msg, &rx_len)) {
		CEC_INFO("buffer got unrecorgnized msg\n");
		cec_rx_buf_clear();
		return 0;
	} else {
		mod_delayed_work(cec_workqueue, dwork, 0);
		return 1;
	}
}

static void cec_task(struct work_struct *work)
{
	struct hdmitx_dev *hdmitx_device;
	struct delayed_work *dwork;

	hdmitx_device = container_of(work, struct hdmitx_dev, cec_work.work);
	dwork = &hdmitx_device->cec_work;
	if (!cec_late_check_rx_buffer())
		queue_delayed_work(cec_workqueue, dwork, CEC_FRAME_DELAY);
}

static irqreturn_t cec_isr_handler(int irq, void *dev_instance)
{
	struct hdmitx_dev *hdmitx;
	unsigned int intr_stat = 0;
	intr_stat = cec_intr_stat();

	if (intr_stat & (1<<1)) {   /* aocec tx intr */
		tx_irq_handle();
		return IRQ_HANDLED;
	}
	if ((-1) == cec_ll_rx(rx_msg, &rx_len))
		return IRQ_HANDLED;

	complete(&rx_complete);
	hdmitx = (struct hdmitx_dev *)dev_instance;
	queue_delayed_work(cec_workqueue, &hdmitx->cec_work, CEC_FRAME_DELAY);
	return IRQ_HANDLED;
}

/******************** cec hal interface ***************************/
static int hdmitx_cec_open(struct inode *inode, struct file *file)
{
	cec_info.open_count++;
	if (cec_info.open_count) {
		cec_info.hal_ctl = 1;
		/* enable all cec features */
		cec_config(0x2f, 1);
	}
	return 0;
}

static int hdmitx_cec_release(struct inode *inode, struct file *file)
{
	cec_info.open_count--;
	if (!cec_info.open_count) {
		cec_info.hal_ctl = 0;
		/* disable all cec features */
		cec_config(0x0, 1);
	}
	return 0;
}

static ssize_t hdmitx_cec_read(struct file *f, char __user *buf,
			   size_t size, loff_t *p)
{
	int ret;

	rx_len = 0;
	ret = wait_for_completion_timeout(&rx_complete, msecs_to_jiffies(50));
	if (ret <= 0)
		return ret;
	if (rx_len == 0)
		return 0;

	if (copy_to_user(buf, rx_msg, rx_len))
		return -EINVAL;
	return rx_len;
}

static ssize_t hdmitx_cec_write(struct file *f, const char __user *buf,
			    size_t size, loff_t *p)
{
	unsigned char tempbuf[16] = {};

	if (size > 16)
		size = 16;
	if (size <= 0)
		return 0;

	if (copy_from_user(tempbuf, buf, size))
		return -EINVAL;

	return cec_ll_tx(tempbuf, size);
}

static long hdmitx_cec_ioctl(struct file *f,
			     unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	unsigned long tmp;
	struct hdmi_port_info port_info;
	int a, b, c, d;

	switch (cmd) {
	case CEC_IOC_GET_PHYSICAL_ADDR:
		if (hdmitx_device->hdmi_info.vsdb_phy_addr.valid == 0)
			return -EINVAL;
		a = hdmitx_device->hdmi_info.vsdb_phy_addr.a;
		b = hdmitx_device->hdmi_info.vsdb_phy_addr.b;
		c = hdmitx_device->hdmi_info.vsdb_phy_addr.c;
		d = hdmitx_device->hdmi_info.vsdb_phy_addr.d;
		tmp = ((a << 12) | (b << 8) | (c << 4) | (d << 0));
		cec_phyaddr_config(tmp, 1);
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_VERSION:
		tmp = CEC_VERSION_14A;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_VENDOR_ID:
		tmp = vendor_id;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_PORT_NUM:
		tmp = 1;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_PORT_INFO:
		/* for tx only 1 port */
		tmp  = ((hdmitx_device->hdmi_info.vsdb_phy_addr.a << 12) |
			(hdmitx_device->hdmi_info.vsdb_phy_addr.b <<  8) |
			(hdmitx_device->hdmi_info.vsdb_phy_addr.c <<  4) |
			(hdmitx_device->hdmi_info.vsdb_phy_addr.d <<  0));
		port_info.type = HDMI_OUTPUT;
		port_info.port_id = 1;
		port_info.cec_supported = 1;
		/* not support arc */
		port_info.arc_supported = 0;
		port_info.physical_address = tmp & 0xffff;
		if (copy_to_user(argp, &port_info, sizeof(port_info)))
			return -EINVAL;
		break;

	case CEC_IOC_GET_SEND_FAIL_REASON:
		tmp = get_cec_tx_fail();
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_SET_OPTION_WAKEUP:
		/* TODO: */
		break;

	case CEC_IOC_SET_OPTION_ENALBE_CEC:
		/* TODO: */
		break;

	case CEC_IOC_SET_OPTION_SYS_CTRL:
		/* TODO: */
		break;

	case CEC_IOC_SET_OPTION_SET_LANG:
		cec_info.menu_lang = arg;
		break;

	case CEC_IOC_GET_CONNECT_STATUS:
		tmp = hdmitx_device->hpd_state;
		if (copy_to_user(argp, &tmp, _IOC_SIZE(cmd)))
			return -EINVAL;
		break;

	case CEC_IOC_ADD_LOGICAL_ADDR:
		tmp = arg & 0xf;
		cec_logicaddr_set(tmp);
		/* add by hal, to init some data structure */
		cec_info.log_addr = tmp;
		cec_info.power_status = POWER_ON;
		cec_logicaddr_config(cec_info.log_addr, 1);
		cec_info.log_addr = tmp;

		cec_info.cec_version = CEC_VERSION_14A;
		cec_info.vendor_id = vendor_id;
		strcpy(cec_info.osd_name, osd_name);

		cec_info.menu_status = DEVICE_MENU_ACTIVE;
		break;

	case CEC_IOC_CLR_LOGICAL_ADDR:
		/* TODO: clear global info */
		break;

	default:
		break;
	}
	return 0;
}

#ifdef CONFIG_COMPAT
static long hdmitx_cec_compat_ioctl(struct file *f,
				    unsigned int cmd, unsigned long arg)
{
	arg = (unsigned long)compat_ptr(arg);
	return hdmitx_cec_ioctl(f, cmd, arg);
}
#endif

/* for improve rw permission */
static char *aml_cec_class_devnode(struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0664;
	hdmi_print(INF, "mode is %x\n", *mode);
	return NULL;
}

static const struct file_operations hdmitx_cec_fops = {
	.owner          = THIS_MODULE,
	.open           = hdmitx_cec_open,
	.read           = hdmitx_cec_read,
	.write          = hdmitx_cec_write,
	.release        = hdmitx_cec_release,
	.unlocked_ioctl = hdmitx_cec_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = hdmitx_cec_compat_ioctl,
#endif
};

/************************ cec high level code *****************************/

static int aml_cec_probe(struct platform_device *pdev)
{
	struct device *dev, *cdev;
#ifdef CONFIG_OF
	struct device_node *node = pdev->dev.of_node;
	int irq_idx = 0, r;
	const char *pin_name = NULL, *irq_name = NULL;
	unsigned int reg;
	struct pinctrl *p;
	struct vendor_info_data *vend;
	struct resource *res;
	resource_size_t *base;
#endif

	device_rename(&pdev->dev, "cectx");
	dbg_dev = &pdev->dev;
	hdmitx_device = get_hdmitx_device();

	memset(&cec_info, 0, sizeof(struct cec_global_info_t));
	dev = dev_get_platdata(&pdev->dev);
	if (dev) {
		/* cdev registe */
		cec_info.dev_no = register_chrdev(0, CEC_DEV_NAME,
							 &hdmitx_cec_fops);
		if (cec_info.dev_no < 0) {
			hdmi_print(INF, "alloc chrdev failed\n");
			return -EINVAL;
		}
		hdmi_print(INF, "alloc chrdev %x\n", cec_info.dev_no);
		if (!dev->class->devnode)
			dev->class->devnode = aml_cec_class_devnode;
		cdev = device_create(dev->class, &pdev->dev,
				     MKDEV(cec_info.dev_no, 0),
				     NULL, CEC_DEV_NAME);
		if (IS_ERR(cdev)) {
			hdmi_print(INF, "create chrdev failed, dev:%p\n", cdev);
			unregister_chrdev(cec_info.dev_no, CEC_DEV_NAME);
			return -EINVAL;
		}
	}

	cec_pre_init();
	init_completion(&rx_complete);
	cec_workqueue = create_workqueue("cec_work");
	if (cec_workqueue == NULL) {
		pr_info("create work queue failed\n");
		return -EFAULT;
	}
	INIT_DELAYED_WORK(&hdmitx_device->cec_work, cec_task);
	cec_info.remote_cec_dev = input_allocate_device();
	if (!cec_info.remote_cec_dev)
		CEC_INFO("No enough memory\n");

	cec_info.remote_cec_dev->name = "cec_input";

	cec_info.remote_cec_dev->evbit[0] = BIT_MASK(EV_KEY);
	cec_info.remote_cec_dev->keybit[BIT_WORD(BTN_0)] =
		BIT_MASK(BTN_0);
	cec_info.remote_cec_dev->id.bustype = BUS_ISA;
	cec_info.remote_cec_dev->id.vendor = 0x1b8e;
	cec_info.remote_cec_dev->id.product = 0x0cec;
	cec_info.remote_cec_dev->id.version = 0x0001;

	set_bit(KEY_POWER, cec_info.remote_cec_dev->keybit);

	if (input_register_device(cec_info.remote_cec_dev)) {
		CEC_INFO("Failed to register device\n");
		input_free_device(cec_info.remote_cec_dev);
	}

#ifdef CONFIG_OF
	/* pinmux set */
	if (of_get_property(node, "pinctrl-names", NULL)) {
		r = of_property_read_string(node,
					    "pinctrl-names",
					    &pin_name);
		if (!r) {
			p = devm_pinctrl_get_select(&pdev->dev, pin_name);
			reg = hd_read_reg(P_AO_RTI_PIN_MUX_REG);
			reg = hd_read_reg(P_AO_RTI_PIN_MUX_REG2);
		}
	}

	/* irq set */
	irq_idx = of_irq_get(node, 0);
	hdmitx_device->irq_cec = irq_idx;
	if (of_get_property(node, "interrupt-names", NULL)) {
		r = of_property_read_string(node, "interrupt-names", &irq_name);
		if (!r) {
			r = request_irq(irq_idx, &cec_isr_handler, IRQF_SHARED,
					irq_name, (void *)hdmitx_device);
		}
	}
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		base = devm_ioremap_resource(&pdev->dev, res);
		exit_reg = (void *)base;
	} else {
		CEC_INFO("no memory resource\n");
		exit_reg = NULL;
	}

	vend = hdmitx_device->config_data.vend_data;
	r = of_property_read_string(node, "vendor_name",
		(const char **)&(vend->vendor_name));
	if (r)
		hdmi_print(INF, SYS "not find vendor name\n");

	r = of_property_read_u32(node, "vendor_id", &(vend->vendor_id));
	if (r)
		hdmi_print(INF, SYS "not find vendor id\n");

	r = of_property_read_string(node, "product_desc",
		(const char **)&(vend->product_desc));
	if (r)
		hdmi_print(INF, SYS "not find product desc\n");

	r = of_property_read_string(node, "cec_osd_string",
		(const char **)&(vend->cec_osd_string));
	if (r)
		hdmi_print(INF, SYS "not find cec osd string\n");
#endif
	hdmitx_device->cec_init_ready = 1;
	/* for init */
	queue_delayed_work(cec_workqueue, &hdmitx_device->cec_work, 0);
	return 0;
}

static int aml_cec_remove(struct platform_device *pdev)
{
	if (!(hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK)))
		return 0;

	CEC_INFO("cec uninit!\n");
	free_irq(hdmitx_device->irq_cec, (void *)hdmitx_device);

	if (cec_workqueue) {
		cancel_delayed_work_sync(&hdmitx_device->cec_work);
		destroy_workqueue(cec_workqueue);
	}
	hdmitx_device->cec_init_ready = 0;
	input_unregister_device(cec_info.remote_cec_dev);
	unregister_chrdev(cec_info.dev_no, CEC_DEV_NAME);
	return 0;
}

#ifdef CONFIG_PM
static int aml_cec_pm_prepare(struct device *dev)
{
	if ((hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK))) {
		cec_suspend = 1;
		CEC_INFO("cec prepare suspend!\n");
	}
	return 0;
}

static void aml_cec_pm_complete(struct device *dev)
{
	int exit = 0;

	if (exit_reg) {
		exit = readl(exit_reg);
		CEC_INFO("wake up flag:%x\n", exit);
	}
	if (((exit >> 28) & 0xf) == CEC_WAKEUP) {
		input_event(cec_info.remote_cec_dev,
			    EV_KEY, KEY_POWER, 1);
		input_sync(cec_info.remote_cec_dev);
		input_event(cec_info.remote_cec_dev,
			    EV_KEY, KEY_POWER, 0);
		input_sync(cec_info.remote_cec_dev);
		CEC_INFO("== WAKE UP BY CEC ==\n");
	}
	cec_suspend = 0;
}

static int aml_cec_resume_noirq(struct device *dev)
{
	if ((hdmitx_device->cec_func_config & (1 << CEC_FUNC_MSAK))) {
		CEC_INFO("cec resume noirq!\n");
		cec_info.power_status = TRANS_STANDBY_TO_ON;
	}
	return 0;
}

static const struct dev_pm_ops aml_cec_pm = {
	.prepare  = aml_cec_pm_prepare,
	.complete = aml_cec_pm_complete,
	.resume_noirq = aml_cec_resume_noirq,
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id aml_cec_dt_match[] = {
	{
		.compatible = "amlogic, amlogic-cec",
	},
	{},
};
#endif

static struct platform_driver aml_cec_driver = {
	.driver = {
		.name  = "cectx",
		.owner = THIS_MODULE,
	#ifdef CONFIG_PM
		.pm     = &aml_cec_pm,
	#endif
	#ifdef CONFIG_OF
		.of_match_table = aml_cec_dt_match,
	#endif
	},
	.probe  = aml_cec_probe,
	.remove = aml_cec_remove,
};

static int __init cec_init(void)
{
	int ret;
	ret = platform_driver_register(&aml_cec_driver);
	return ret;
}

static void __exit cec_uninit(void)
{
	platform_driver_unregister(&aml_cec_driver);
}


module_init(cec_init);
module_exit(cec_uninit);
MODULE_DESCRIPTION("AMLOGIC HDMI TX CEC driver");
MODULE_LICENSE("GPL");
MODULE_PARM_DESC(cec_msg_dbg_en, "\n cec_msg_dbg_en\n");
module_param(cec_msg_dbg_en, bool, 0664);
