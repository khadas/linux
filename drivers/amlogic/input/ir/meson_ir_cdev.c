// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include "meson_ir_main.h"
#include "meson_ir_sysfs.h"

#define AML_REMOTE_NAME "amremote"

static int meson_ir_open(struct inode *inode, struct file *file)
{
	struct meson_ir_chip *chip;

	chip = container_of(inode->i_cdev, struct meson_ir_chip, chrdev);
	file->private_data = chip;
	disable_irq(chip->irqno);
	return 0;
}

static long meson_ir_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	struct meson_ir_chip *chip = (struct meson_ir_chip *)file->private_data;
	struct ir_sw_decode_para sw_data;
	struct meson_ir_map_tab_list *ir_map;
	struct meson_ir_map_tab_list *ptable;
	void __user *parg = (void __user *)arg;
	unsigned long flags;
	u32 value;
	int retval = 0;

	if (!parg) {
		dev_err(chip->dev, "%s invalid user space pointer\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&chip->file_lock);
	switch (cmd) {
	case IR_IOC_GET_DATA_VERSION:
		if (copy_to_user(parg, SHARE_DATA_VERSION,
				 sizeof(SHARE_DATA_VERSION))) {
			retval = -EFAULT;
			goto err;
		}
		break;

	case IR_IOC_SET_KEY_NUMBER:
		if (copy_from_user(&value, parg, sizeof(u32))) {
			chip->key_num.update_flag = false;
			retval = -EFAULT;
			goto err;
		}
		chip->key_num.update_flag = true;
		chip->key_num.value = value;
		break;

	case IR_IOC_SET_KEY_MAPPING_TAB:
		if (chip->key_num.update_flag) {
			ir_map = kzalloc(sizeof(*ir_map) + chip->key_num.value *
					 sizeof(union _codemap),
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
				meson_ir_tab_free(ir_map);
				retval = -EFAULT;
				goto err;
			}

			/* Check data whether valid or not*/
			if (chip->key_num.value != ir_map->tab.map_size) {
				meson_ir_tab_free(ir_map);
				retval = -EFAULT;
				goto err;
			}
			/*scancode sort*/
			meson_ir_scancode_sort(&ir_map->tab);

			/*overwrite the old map table or insert new map table*/
			spin_lock_irqsave(&chip->slock, flags);
			ptable = meson_ir_seek_map_tab(chip,
						       ir_map->tab.custom_code);
			if (ptable) {
				if (ptable == chip->cur_tab)
					chip->cur_tab = ir_map;
				list_del(&ptable->list);
				meson_ir_tab_free(ptable);
			}
			list_add_tail(&ir_map->list, &chip->map_tab_head);
			spin_unlock_irqrestore(&chip->slock, flags);
			chip->key_num.update_flag = false;
		}
		break;

	case IR_IOC_SET_SW_DECODE_PARA:
		if (copy_from_user(&sw_data, parg,
				   sizeof(struct ir_sw_decode_para))) {
			retval = -EFAULT;
			goto err;
		}
		chip->r_dev->max_frame_time = sw_data.max_frame_time;
		break;

	default:
		retval = -ENOTTY;
		goto err;
	}
err:
	mutex_unlock(&chip->file_lock);
	return retval;
}

static int meson_ir_release(struct inode *inode, struct file *file)
{
	struct meson_ir_chip *chip = (struct meson_ir_chip *)file->private_data;

	enable_irq(chip->irqno);
	file->private_data = NULL;
	return 0;
}

static const struct file_operations meson_ir_fops = {
	.owner = THIS_MODULE,
	.open = meson_ir_open,
#ifdef CONFIG_COMPAT
	.compat_ioctl = meson_ir_ioctl,
#endif
	.unlocked_ioctl = meson_ir_ioctl,
	.release = meson_ir_release,
};

int meson_ir_cdev_init(struct meson_ir_chip *chip)
{
	int ret = 0;

	chip->dev_name  = AML_REMOTE_NAME;
	ret = alloc_chrdev_region(&chip->chr_devno,
				  0, 1, AML_REMOTE_NAME);
	if (ret < 0) {
		dev_err(chip->dev, "failed to allocate major number\n");
		return ret;
	}
	cdev_init(&chip->chrdev, &meson_ir_fops);
	chip->chrdev.owner = THIS_MODULE;
	ret = cdev_add(&chip->chrdev, chip->chr_devno, 1);
	if (ret < 0) {
		dev_err(chip->dev, "failed to cdev_add\n");
		goto err_cdev_add;
	}

	ret = meson_ir_sysfs_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "failed to ir_sys create %d\n", ret);
		goto err_ir_sys;
	}
	return 0;

err_ir_sys:
	cdev_del(&chip->chrdev);
err_cdev_add:
	unregister_chrdev_region(chip->chr_devno, 1);
	return ret;
}
EXPORT_SYMBOL(meson_ir_cdev_init);

void meson_ir_cdev_free(struct meson_ir_chip *chip)
{
	meson_ir_sysfs_exit(chip);
	cdev_del(&chip->chrdev);
	unregister_chrdev_region(chip->chr_devno, 1);
}
EXPORT_SYMBOL(meson_ir_cdev_free);

