/*
 * drivers/amlogic/input/remote/remote_cdev.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include "remote_meson.h"
#include "sysfs.h"

#define AML_REMOTE_NAME "amremote"

static int remote_open(struct inode *inode, struct file *file)
{
	struct remote_chip *chip;

	chip = container_of(inode->i_cdev, struct remote_chip, chrdev);
	file->private_data = chip;
	disable_irq(chip->irqno);
	return 0;
}
static long remote_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct remote_chip *chip = (struct remote_chip *)file->private_data;
	struct ir_sw_decode_para sw_data;
	struct ir_map_tab_list *ir_map;
	struct ir_map_tab_list *ptable;
	void __user *parg = (void __user *)arg;
	unsigned long flags;
	u32 value;
	u8 val;
	int retval = 0;

	if (!parg) {
		dev_err(chip->dev, "%s invalid user space pointer\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&chip->file_lock);
	switch (cmd) {
	case REMOTE_IOC_GET_DATA_VERSION:
		if (copy_to_user(parg, SHARE_DATA_VERSION,
						sizeof(SHARE_DATA_VERSION))) {
			retval = -EFAULT;
			goto err;
		}
	break;

	case REMOTE_IOC_SET_KEY_NUMBER:
		if (copy_from_user(&value, parg, sizeof(u32))) {
			chip->key_num.update_flag = false;
			retval = -EFAULT;
			goto err;
		}

		if (value < 1 || value > MAX_KEYMAP_SIZE) {
			chip->key_num.update_flag = false;
			retval = -EINVAL;
			goto err;
		}

		chip->key_num.update_flag = true;
		chip->key_num.value = value;
		break;

	case REMOTE_IOC_SET_KEY_MAPPING_TAB:
		if (chip->key_num.update_flag) {
			ir_map = kzalloc(sizeof(struct ir_map_tab_list) +
				chip->key_num.value * sizeof(union _codemap),
			    GFP_KERNEL);
			if (!ir_map) {
				dev_err(chip->dev, "%s ir map table alloc err\n",
						__func__);
				retval = -ENOMEM;
				goto err;
			}
			if (copy_from_user(&ir_map->tab, parg,
				sizeof(struct ir_map_tab) +
				chip->key_num.value * sizeof(union _codemap))) {
				ir_tab_free(ir_map);
				retval = -EFAULT;
				goto err;
			}

			/* Check data whether valid or not*/
			if (chip->key_num.value != ir_map->tab.map_size) {
				ir_tab_free(ir_map);
				retval = -EFAULT;
				goto err;
			}
			/*scancode sort*/
			ir_scancode_sort(&ir_map->tab);

			/*overwrite the old map table or insert new map table*/
			spin_lock_irqsave(&chip->slock, flags);
			ptable = seek_map_tab(chip, ir_map->tab.custom_code);
			if (ptable) {
				if (ptable == chip->cur_tab)
					chip->cur_tab = ir_map;
				list_del(&ptable->list);
				ir_tab_free(ptable);
			}
			list_add_tail(&ir_map->list, &chip->map_tab_head);
			spin_unlock_irqrestore(&chip->slock, flags);
			chip->key_num.update_flag = false;
		}
		break;

	case REMOTE_IOC_SET_SW_DECODE_PARA:
		if (copy_from_user(&sw_data, parg,
				sizeof(struct ir_sw_decode_para))) {
			retval = -EFAULT;
			goto err;
		}
		chip->r_dev->max_frame_time = sw_data.max_frame_time;
		break;
	case REMOTE_IOC_GET_IR_LEARNING:
		if (copy_to_user(parg, &chip->r_dev->ir_learning_on,
				 sizeof(u8))) {
			retval = -EFAULT;
			goto err;
		}
		break;

	case REMOTE_IOC_SET_IR_LEARNING:
		/*reset demudolation and carrier detect*/
		if (chip->r_dev->demod_enable)
			demod_reset(chip);

		if (copy_from_user(&val, parg, sizeof(u8))) {
			retval = -EFAULT;
			goto err;
		}

		chip->r_dev->ir_learning_on = !!val;
		if (!!val) {
			if (remote_pulses_malloc(chip) < 0) {
				retval = -ENOMEM;
				goto err;
			}
			chip->set_register_config(chip, REMOTE_TYPE_RAW_NEC);
			/*backup protocol*/
			chip->r_dev->protocol = chip->protocol;
			chip->protocol = REMOTE_TYPE_RAW_NEC;
			irq_set_affinity(chip->irqno,
					 cpumask_of(chip->irq_cpumask));
		} else {
			chip->protocol = chip->r_dev->protocol;
			chip->set_register_config(chip, chip->protocol);
			remote_pulses_free(chip);
			chip->r_dev->ir_learning_done = false;
		}
		break;

	case REMOTE_IOC_GET_RAW_DATA:
		if (copy_to_user(parg, chip->r_dev->pulses,
			sizeof(struct pulse_group) +
			chip->r_dev->max_learned_pulse *
			sizeof(unsigned int))) {
			retval = -EFAULT;
			goto err;
		}
		/*clear to prepear next frame*/
		memset(chip->r_dev->pulses, 0, sizeof(struct pulse_group) +
			chip->r_dev->max_learned_pulse *
			sizeof(unsigned int));

		if (chip->r_dev->demod_enable)
			demod_reset(chip);

		/*finish reading data ,enable state machine */
		remote_reg_update_bits(chip, MULTI_IR_ID, REG_REG1, BIT(15),
				       BIT(15));

		chip->r_dev->ir_learning_done = false;

		break;

	case REMOTE_IOC_GET_SUM_CNT0:
		if (chip->r_dev->demod_enable) {
			remote_reg_read(chip, MULTI_IR_ID, REG_DEMOD_SUM_CNT0,
					&value);
			if (copy_to_user(parg, &value, sizeof(u32))) {
				retval = -EFAULT;
				goto err;
			}
		} else {
			retval = -EFAULT;
			goto err;
		}
		break;

	case REMOTE_IOC_GET_SUM_CNT1:
		if (chip->r_dev->demod_enable) {
			remote_reg_read(chip, MULTI_IR_ID, REG_DEMOD_SUM_CNT1,
					&value);
			if (copy_to_user(parg, &value, sizeof(u32))) {
				retval = -EFAULT;
				goto err;
			}
		} else {
			retval = -EFAULT;
			goto err;
		}
		break;

	default:
		retval = -ENOTTY;
		goto err;
	}
err:
	mutex_unlock(&chip->file_lock);
	return retval;
}
static int remote_release(struct inode *inode, struct file *file)
{
	struct remote_chip *chip = (struct remote_chip *)file->private_data;

	enable_irq(chip->irqno);
	file->private_data = NULL;
	return 0;
}

static const struct file_operations remote_fops = {
	.owner = THIS_MODULE,
	.open = remote_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl = remote_ioctl,
#endif
	.unlocked_ioctl = remote_ioctl,
	.release = remote_release,
};

int ir_cdev_init(struct remote_chip *chip)
{
	int ret = 0;

	chip->dev_name  = AML_REMOTE_NAME;
	ret = alloc_chrdev_region(&chip->chr_devno,
		0, 1, AML_REMOTE_NAME);
	if (ret < 0) {
		dev_err(chip->dev, "failed to allocate major number\n");
		ret = -ENODEV;
		goto err_end;
	}
	cdev_init(&chip->chrdev, &remote_fops);
	chip->chrdev.owner = THIS_MODULE;
	ret = cdev_add(&chip->chrdev, chip->chr_devno, 1);
	if (ret < 0) {
		dev_err(chip->dev, "failed to cdev_add\n");
		goto err_cdev_add;
	}

	ret = ir_sys_device_attribute_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "failed to ir_sys create %d\n", ret);
		goto err_ir_sys;
	}
	return 0;

err_ir_sys:
	dev_err(chip->dev, "err_ir_sys\n");
	cdev_del(&chip->chrdev);
err_cdev_add:
	dev_err(chip->dev, "err_cdev_add\n");
	unregister_chrdev_region(chip->chr_devno, 1);
err_end:
	return ret;
}
EXPORT_SYMBOL(ir_cdev_init);

void ir_cdev_free(struct remote_chip *chip)
{
	ir_sys_device_attribute_sys(chip);
	cdev_del(&chip->chrdev);
	unregister_chrdev_region(chip->chr_devno, 1);
}
EXPORT_SYMBOL(ir_cdev_free);

