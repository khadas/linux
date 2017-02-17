/*
 * drivers/amlogic/input/remote/remote_meson.c
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


#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_platform.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/pm.h>
#include <linux/of_address.h>

#include "remote_meson.h"

#include <linux/amlogic/iomap.h>
#include "sysfs.h"

#include <linux/cdev.h>


static void amlremote_tasklet(unsigned long data);


DECLARE_TASKLET_DISABLED(tasklet, amlremote_tasklet, 0);

int remote_reg_read(struct remote_chip *chip, unsigned char id,
	unsigned int reg, unsigned int *val)
{
	if (id >= IR_ID_MAX) {
		pr_err("remote: invalid id:[%d] in %s\n", id, __func__);
		return -EINVAL;
	}

	*val = readl((chip->ir_contr[id].remote_regs+reg));

	return 0;
}

int remote_reg_write(struct remote_chip *chip, unsigned char id,
	unsigned int reg, unsigned int val)
{
	if (id >= IR_ID_MAX) {
		pr_err("remote: invalid id:[%d] in %s\n", id, __func__);
		return -EINVAL;
	}

	writel(val, (chip->ir_contr[id].remote_regs+reg));

	return 0;
}

static int remote_open(struct inode *inode, struct file *file)
{
	struct remote_chip *chip;

	chip = container_of(inode->i_cdev, struct remote_chip, chrdev);
	file->private_data = chip;

	pr_info("remote:remote_open\n");
	return 0;
}


static int remote_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static const struct file_operations remote_fops = {
	.owner = THIS_MODULE,
	.open = remote_open,
	.release = remote_release,
};

#define AML_REMOTE_NAME "amremote"

static int ir_cdev_init(struct remote_chip *chip)
{
	int ret = 0;

	chip->dev_name  = AML_REMOTE_NAME;
	ret = alloc_chrdev_region(&chip->chr_devno,
		0, 1, AML_REMOTE_NAME);
	if (ret < 0) {
		pr_err("remote: failed to allocate major number\n");
		ret = -ENODEV;
		goto err_end;
	}
	cdev_init(&chip->chrdev, &remote_fops);
	chip->chrdev.owner = THIS_MODULE;
	ret = cdev_add(&chip->chrdev, chip->chr_devno, 1);
	if (ret < 0) {
		pr_err("remote: failed to cdev_add\n");
		goto err_cdev_add;
	}

	ret = ir_sys_device_attribute_init(chip);
	if (ret < 0) {
		pr_err("remote: failed to ir_sys create %d\n", ret);
		goto err_ir_sys;
	}
	return 0;

err_ir_sys:
	pr_err("remote: err_ir_sys\n");
	cdev_del(&chip->chrdev);
err_cdev_add:
	pr_err("remote: err_cdev_add\n");
	unregister_chrdev_region(chip->chr_devno, 1);
err_end:
	return ret;
}

static void ir_cdev_free(struct remote_chip *chip)
{
	ir_sys_device_attribute_sys(chip);
	cdev_del(&chip->chrdev);
	unregister_chrdev_region(chip->chr_devno, 1);
}

static u32 getkeycode(struct remote_dev *dev, u32 scancode)
{
	struct remote_chip *chip = (struct remote_chip *)dev->platform_data;
	struct custom_table *ct = chip->cur_custom;
	int i;


	if (!ct) {
		pr_err("cur_custom is nulll\n");
		return KEY_RESERVED;
	}
	for (i = 0; i < ct->map_size; i++) {
		if ((ct->codemap + i) &&
			(ct->codemap[i].map.scancode == scancode))
			return ct->codemap[i].map.keycode;
	}

	remote_printk(2, "scancode %d undefined\n", scancode);
	return KEY_RESERVED;
}

static bool is_valid_custom(struct remote_dev *dev)
{
	struct remote_chip *chip = (struct remote_chip *)dev->platform_data;
	struct custom_table *ct = chip->custom_tables;
	int i;
	int custom_code;
	if (!chip->ir_contr[chip->ir_work].get_custom_code)
		return true;
	custom_code = chip->ir_contr[chip->ir_work].get_custom_code(chip);
	for (i = 0; i < chip->custom_num; i++) {
		if (ct->custom_code == custom_code) {
			chip->cur_custom = ct;
			return true;
		}
		ct++;
	}
	return false;
}

static bool is_next_repeat(struct remote_dev *dev)
{
	unsigned int val;
	unsigned char fbusy = 0;
	unsigned char cnt;

	struct remote_chip *chip = (struct remote_chip *)dev->platform_data;

	for (cnt = 0; cnt < (ENABLE_LEGACY_IR(chip->protocol) ? 2:1); cnt++) {
		remote_reg_read(chip, cnt, REG_STATUS, &val);
		fbusy |= IR_CONTROLLER_BUSY(val);
	}
	remote_printk(2, "ir controller busy flag = %d\n", fbusy);
	if (!dev->wait_next_repeat && fbusy)
		return true;
	else
		return false;
}

static bool set_custom_code(struct remote_dev *dev, u32 code)
{
	struct remote_chip *chip = (struct remote_chip *)dev->platform_data;

	return chip->ir_contr[chip->ir_work].set_custom_code(chip, code);
}

static void amlremote_tasklet(unsigned long data)
{
	struct remote_chip *chip = (struct remote_chip *)data;
	unsigned long flags;
	int status = -1;
	int scancode = -1;

	/**
	  *need first get_scancode, then get_decode_status, the status
	  *may was set flag from get_scancode function
	  */
	spin_lock_irqsave(&chip->slock, flags);
	if (chip->ir_contr[chip->ir_work].get_scancode)
		scancode = chip->ir_contr[chip->ir_work].get_scancode(chip);
	if (chip->ir_contr[chip->ir_work].get_decode_status)
		status = chip->ir_contr[chip->ir_work].get_decode_status(chip);
	if (status == REMOTE_NORMAL) {
		remote_printk(2, "receive scancode=0x%x\n", scancode);
		remote_keydown(chip->r_dev, scancode, status);
	} else if (status & REMOTE_REPEAT) {
		remote_printk(4, "receive repeat\n");
		remote_keydown(chip->r_dev, scancode, status);
	} else
		remote_printk(4, "receive error %d\n", status);
	spin_unlock_irqrestore(&chip->slock, flags);

}

static irqreturn_t ir_interrupt(int irq, void *dev_id)
{
	struct remote_chip *rc = (struct remote_chip *)dev_id;
	int contr_status;
	int val = 0;
	u32 duration;
	char buf[50];
	unsigned char cnt;
	enum raw_event_type type = RAW_SPACE;

	remote_reg_read(rc, MULTI_IR_ID, REG_REG1, &val);
	val = (val & 0x1FFF0000) >> 16;
	sprintf(buf, "d:%d\n", val);
	debug_log_printk(rc->r_dev, buf);
	/**
	  *software decode multiple protocols by using Time Measurement of
	  *multif-format IR controller
	  */
	if (MULTI_IR_SOFTWARE_DECODE(rc->protocol)) {
		rc->ir_work = MULTI_IR_ID;
		duration = val*10*1000;
		type    = RAW_PULSE;
		sprintf(buf, "------\n");
		debug_log_printk(rc->r_dev, buf);
		remote_raw_event_store_edge(rc->r_dev, type, duration);
		remote_raw_event_handle(rc->r_dev);
	} else {
		for (cnt = 0; cnt < (ENABLE_LEGACY_IR(rc->protocol)
				? 2:1); cnt++) {
			remote_reg_read(rc, cnt, REG_STATUS, &contr_status);
			if (IR_DATA_IS_VALID(contr_status)) {
				rc->ir_work = cnt;
				break;
			}
		}

		if (cnt == IR_ID_MAX) {
			pr_err("remote: invalid interrupt.\n");
			return IRQ_HANDLED;
		}

		tasklet_schedule(&tasklet);
	}
	return IRQ_HANDLED;
}

static int get_custom_tables(struct device_node *node,
	struct remote_chip *chip)
{
	const phandle *phandle;
	struct device_node *custom_maps, *map;
	u32 value;
	int ret = -1;
	int index;
	char *propname;
	const char *uname;
	struct custom_table *ptable;

	phandle = of_get_property(node, "map", NULL);
	if (!phandle) {
		pr_err("%s:don't find match custom\n", __func__);
		return -1;
	} else {
		custom_maps = of_find_node_by_phandle(be32_to_cpup(phandle));
		if (!custom_maps) {
			pr_err("can't find device node key\n");
			return -1;
		}
	}

	ret = of_property_read_u32(custom_maps, "mapnum", &value);
	if (ret) {
		pr_err("please config correct mapnum item\n");
		return -1;
	}
	chip->custom_num = value;
	if (chip->custom_num > CUSTOM_NUM_MAX)
		chip->custom_num = CUSTOM_NUM_MAX;

	pr_info("custom_number = %d\n", chip->custom_num);

	if (chip->custom_num) {
		chip->custom_tables = kzalloc(chip->custom_num *
			sizeof(struct custom_table), GFP_KERNEL);
		if (!chip->custom_tables) {
			pr_err("%s custom_tables alloc err\n", __func__);
			return -1;
		}
	}

	for (index = 0; index < chip->custom_num; index++) {
		ptable = &chip->custom_tables[index];
		propname = kasprintf(GFP_KERNEL, "map%d", index);
		phandle = of_get_property(custom_maps, propname, NULL);
		if (!phandle) {
			pr_err("%s:don't find match map%d\n", __func__, index);
			goto err;
		}
		map = of_find_node_by_phandle(be32_to_cpup(phandle));
		if (!map) {
			pr_err("can't find device node key\n");
			goto err;
		}
		ret = of_property_read_string(map, "mapname", &uname);
		if (ret) {
			pr_err("please config mapname item\n");
			goto err;
		}
		ptable->custom_name = uname;
		pr_info("ptable->custom_name = %s\n", ptable->custom_name);
		ret = of_property_read_u32(map, "customcode", &value);
		if (ret) {
			pr_err("please config customcode item\n");
			goto err;
		}
		ptable->custom_code = value;
		pr_info("ptable->custom_code = 0x%x\n", ptable->custom_code);
		ret = of_property_read_u32(map, "size", &value);
		if (ret || value > MAX_KEYMAP_SIZE) {
			pr_err("no config size item or err\n");
			goto err;
		}
		ptable->map_size = value;
		pr_info("ptable->map_size = 0x%x\n", ptable->map_size);

		/*alloc space of codemap*/
		ptable->codemap = kzalloc(value*sizeof(*ptable->codemap),
								GFP_KERNEL);
		if (!ptable->codemap) {
			pr_err("%s custom_tables alloc err\n", __func__);
			goto err;
		}
		ret = of_property_read_u32_array(map,
			    "keymap", (u32 *)ptable->codemap, ptable->map_size);
		if (ret) {
			pr_err("please config keymap item\n");
			goto err;
		}
	}
	return 0;
err:
	kfree(chip->custom_tables);
	return -1;
}


static int ir_get_devtree_pdata(struct platform_device *pdev)
{
	struct resource *res_irq;
	struct resource *res_mem;
	resource_size_t *res_start[2];
	struct pinctrl *p;
	int ret;
	int value;
	unsigned char i;


	struct remote_chip *chip = platform_get_drvdata(pdev);

	ret = of_property_read_u32(pdev->dev.of_node,
			"protocol", &chip->protocol);
	if (ret) {
		pr_info("remote:don't find the node <protocol>\n");
		chip->protocol = 1;
	}
	pr_info("remote: protocol = 0x%x\n", chip->protocol);

	p = devm_pinctrl_get_select_default(&pdev->dev);
	if (IS_ERR(p)) {
		pr_err("remote: pinctrl error, %ld\n", PTR_ERR(p));
		return -1;
	}

	for (i = 0; i < 2; i++) {
		res_mem = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (IS_ERR_OR_NULL(res_mem)) {
			pr_err("remote: Get IORESOURCE_MEM error, %ld\n",
					PTR_ERR(p));
			return PTR_ERR(res_mem);
		}
		res_start[i] = devm_ioremap_resource(&pdev->dev, res_mem);
		chip->ir_contr[i].remote_regs = (void __iomem *)res_start[i];
	}

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (IS_ERR_OR_NULL(res_irq)) {
		pr_err("remote: Get IORESOURCE_IRQ error, %ld\n", PTR_ERR(p));
		return PTR_ERR(res_irq);
	}

	chip->irqno = res_irq->start;


	pr_info("remote: platform_data irq =%d\n", chip->irqno);

	ret = of_property_read_u32(pdev->dev.of_node,
				"release_delay", &value);
	if (ret) {
		pr_info("remote:don't find the node <release_delay>\n");
		value = 70;
	}

	chip->r_dev->keyup_delay = value;

	ret = of_property_read_u32(pdev->dev.of_node,
				"max_frame_time", &value);
	if (ret) {
		pr_info("remote:don't find the node <max_frame_time>\n");
		value = 200; /*default value*/
	}

	chip->r_dev->max_frame_time = value;


	/*create custom table */
	get_custom_tables(pdev->dev.of_node, chip);

	return 0;
}

static int ir_hardware_init(struct platform_device *pdev)
{
	int ret;

	struct remote_chip *chip = platform_get_drvdata(pdev);

	if (!pdev->dev.of_node) {
		pr_err("remote: pdev->dev.of_node == NULL!\n");
		return -1;
	}

	ret = ir_get_devtree_pdata(pdev);
	if (ret < 0)
		return ret;
	chip->set_register_config(chip, chip->protocol);
	ret = request_irq(chip->irqno, ir_interrupt, IRQF_SHARED,
				"keypad", (void *)chip);
	if (ret < 0)
		goto error_irq;

	chip->irq_cpumask = 1;
	irq_set_affinity(chip->irqno, cpumask_of(chip->irq_cpumask));

	tasklet_enable(&tasklet);
	tasklet.data = (unsigned long)chip;

	return 0;

error_irq:
	pr_info("remote:request_irq error %d\n", ret);

	return ret;

}

static int ir_hardware_free(struct platform_device *pdev)
{
	struct remote_chip *chip = platform_get_drvdata(pdev);
	free_irq(chip->irqno, chip);
	return 0;
}

static void ir_input_device_init(struct input_dev *dev,
	struct device *parent, const char *name)
{
	dev->name = name;
	dev->phys = "keypad/input0";
	dev->dev.parent = parent;
	dev->id.bustype = BUS_ISA;
	dev->id.vendor  = 0x0001;
	dev->id.product = 0x0001;
	dev->id.version = 0x0100;
	dev->rep[REP_DELAY] = 0xffffffff;  /*close input repeat*/
	dev->rep[REP_PERIOD] = 0xffffffff; /*close input repeat*/
}

static int remote_probe(struct platform_device *pdev)
{
	struct remote_dev *dev;
	int ret;
	struct remote_chip *chip;

	pr_info("remote_probe\n");
	chip = kzalloc(sizeof(struct remote_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("remote: kzalloc remote_chip error!\n");
		ret = -ENOMEM;
		goto err_end;
	}

	dev = remote_allocate_device();
	if (!dev) {
		pr_err("remote: kzalloc remote_dev error!\n");
		ret = -ENOMEM;
		goto err_alloc_remote_dev;
	}

	mutex_init(&chip->file_lock);
	spin_lock_init(&chip->slock);

	chip->r_dev = dev;
	chip->dev = &pdev->dev;
	chip->sys_custom = NULL;

	chip->r_dev->platform_data = (void *)chip;
	chip->r_dev->getkeycode    = getkeycode;
	chip->r_dev->set_custom_code = set_custom_code;
	chip->r_dev->is_valid_custom = is_valid_custom;
	chip->r_dev->is_next_repeat  = is_next_repeat;
	chip->set_register_config = ir_register_default_config;
	remote_debug_set_enable(false);
	platform_set_drvdata(pdev, chip);

	ir_input_device_init(dev->input_device, &pdev->dev, "aml_keypad");
	ret = ir_cdev_init(chip);
	if (ret < 0)
		goto err_cdev_init;

	ret = ir_hardware_init(pdev);
	if (ret < 0)
		goto err_hard_init;

	dev->rc_type = chip->protocol;

	ret = remote_register_device(dev);

	if (ret)
		goto error_register_remote;

	return 0;

error_register_remote:
	ir_hardware_free(pdev);
err_hard_init:
	ir_cdev_free(chip);
err_cdev_init:
	remote_free_device(dev);
err_alloc_remote_dev:
	kfree(chip);
err_end:
	return ret;
}

static int remote_remove(struct platform_device *pdev)
{
	struct remote_chip *chip = platform_get_drvdata(pdev);

	tasklet_disable(&tasklet);
	tasklet_kill(&tasklet);

	free_irq(chip->irqno, chip); /*irq dev_id is chip address*/
	ir_cdev_free(chip);
	remote_unregister_device(chip->r_dev);
	remote_free_device(chip->r_dev);

	kfree(chip);
	return 0;
}

static int remote_resume(struct platform_device *pdev)
{
	struct remote_chip *chip = platform_get_drvdata(pdev);
	unsigned int val;
	unsigned long flags;
	unsigned char cnt;

	pr_info("remote_resume\n");
	/*resume register config*/
	spin_lock_irqsave(&chip->slock, flags);
	chip->set_register_config(chip, chip->protocol);
	/* read REG_STATUS and REG_FRAME to clear status */
	for (cnt = 0; cnt < (ENABLE_LEGACY_IR(chip->protocol) ? 2:1); cnt++) {
		remote_reg_read(chip, cnt, REG_STATUS, &val);
		remote_reg_read(chip, cnt, REG_FRAME, &val);
	}
	spin_unlock_irqrestore(&chip->slock, flags);

	if (get_resume_method() == REMOTE_WAKEUP) {
		input_event(chip->r_dev->input_device,
		    EV_KEY, KEY_POWER, 1);
		input_sync(chip->r_dev->input_device);
		input_event(chip->r_dev->input_device,
		    EV_KEY, KEY_POWER, 0);
		input_sync(chip->r_dev->input_device);
	}

	if (get_resume_method() == REMOTE_CUS_WAKEUP) {
		input_event(chip->r_dev->input_device, EV_KEY, 133, 1);
		input_sync(chip->r_dev->input_device);
		input_event(chip->r_dev->input_device, EV_KEY, 133, 0);
		input_sync(chip->r_dev->input_device);
	}

	irq_set_affinity(chip->irqno, cpumask_of(chip->irq_cpumask));
	enable_irq(chip->irqno);
	return 0;
}

static int remote_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct remote_chip *chip = platform_get_drvdata(pdev);

	pr_info("remote_suspend\n");
	disable_irq(chip->irqno);
	return 0;
}

static const struct of_device_id remote_dt_match[] = {
	{
		.compatible     = "amlogic, aml_remote",
	},
	{},
};

static struct platform_driver remote_driver = {
	.probe = remote_probe,
	.remove = remote_remove,
	.suspend = remote_suspend,
	.resume = remote_resume,
	.driver = {
		.name = "meson-remote",
		.of_match_table = remote_dt_match,
	},
};

static int __init remote_init(void)
{
	pr_info("remote: Driver init\n");
	return platform_driver_register(&remote_driver);
}

static void __exit remote_exit(void)
{
	pr_info("remote: exit\n");
	platform_driver_unregister(&remote_driver);
}

module_init(remote_init);
module_exit(remote_exit);


MODULE_AUTHOR("AMLOGIC");
MODULE_DESCRIPTION("AMLOGIC REMOTE PROTOCOL");
MODULE_LICENSE("GPL");

