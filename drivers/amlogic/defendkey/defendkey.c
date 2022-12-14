// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/memblock.h>
#include <linux/random.h>
#include <linux/compat.h>
#include "defendkey.h"

#define DEFENDKEY_DEVICE_NAME   "defendkey"
#define DEFENDKEY_CLASS_NAME    "defendkey"

static unsigned long mem_size;
static struct defendkey_mem defendkey_rmem;
static struct aml_defendkey_type defendkey_type = {
	.decrypt_dtb = 0,
	.status      = 0,
};

static int defendkey_open(struct inode *inode, struct file *file)
{
	struct aml_defendkey_dev *devp;

	devp = container_of(inode->i_cdev, struct aml_defendkey_dev, cdev);
	file->private_data = devp;
	return 0;
}

static int defendkey_release(struct inode *inode, struct file *file)
{
	return 0;
}

static loff_t defendkey_llseek(struct file *filp, loff_t off, int whence)
{
	return 0;
}

static long defendkey_unlocked_ioctl(struct file *file,
				     unsigned int cmd, unsigned long arg)
{
	unsigned long ret = 0;
	struct aml_defendkey_dev *defendkey_dev;

	defendkey_dev = file->private_data;

	switch (cmd) {
	case CMD_SECURE_CHECK:
		ret = aml_is_secure_set(defendkey_dev->reg_base);
		break;
	case CMD_DECRYPT_DTB:
		if (arg == 1) {
			defendkey_type.decrypt_dtb = E_DECRYPT_DTB;
		} else if (arg == 0) {
			defendkey_type.decrypt_dtb = E_UPGRADE_CHECK;
		} else {
			return -EINVAL;
			pr_info("set defendkey type fail, invalid value\n");
		}
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long defendkey_compat_ioctl(struct file *filp,
				   unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = defendkey_unlocked_ioctl(filp, cmd, args);

	return ret;
}
#endif

static ssize_t defendkey_read(struct file *file,
			      char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t ret_value = RET_ERROR;
	int ret = -EINVAL;
	unsigned long mem_base_phy, check_offset;
	void __iomem *mem_base_virt;
	struct cpumask task_cpumask;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	if (defendkey_type.status == 1) {
		defendkey_type.status = 0;

		mem_base_phy = defendkey_rmem.base;
		mem_base_virt = phys_to_virt(mem_base_phy);

		check_offset = aml_sec_boot_check(AML_D_Q_IMG_SIG_HDR_SIZE,
						  mem_base_phy, mem_size, 0);
		if (AML_D_Q_IMG_SIG_HDR_SIZE == (check_offset & 0xFFFF))
			check_offset = (check_offset >> 16) & 0xFFFF;
		else
			check_offset = 0;

		if (mem_size < count) {
			pr_err("%s: data size overflow\n", __func__);
			ret_value = RET_FAIL;
			goto exit;
		}

		ret = copy_to_user((void __user *)buf,
				   (const void *)(mem_base_virt + check_offset),
				    count);
		if (ret) {
			ret_value = RET_FAIL;
			pr_err("%s: copy_to_user fail! ret:%d\n",
			       __func__, ret);
			goto exit;
		}
	} else {
		ret_value = RET_ERROR;
		pr_err("%s: need to decrypt dtb first\n", __func__);
		goto exit;
	}

	ret_value = RET_SUCCESS;
	pr_info("%s: read decrypted dtb OK\n", __func__);

exit:
	set_cpus_allowed_ptr(current, &task_cpumask);
	return ret_value;
}

static ssize_t defendkey_write(struct file *file,
			       const char __user *buf,
			       size_t count, loff_t *ppos)
{
	ssize_t ret_value = RET_ERROR;
	int ret = -EINVAL;
	unsigned long mem_base_phy, copy_base, copy_size;
	void __iomem *mem_base_virt;
	u64 option = 0, random = 0, option_random = 0;
	int i;
	struct cpumask task_cpumask;

	cpumask_copy(&task_cpumask, current->cpus_ptr);
	set_cpus_allowed_ptr(current, cpumask_of(0));

	defendkey_type.status = 0;

	mem_base_phy = defendkey_rmem.base;
	mem_base_virt = phys_to_virt(mem_base_phy);

	pr_info("defendkey: mem_base_phy:%lx mem_size:%lx mem_base_virt:%p count:%lx\n",
		mem_base_phy, mem_size, mem_base_virt, (unsigned long)count);

	random = get_random_long();

	option_random = (random << 8);
	option_random |= (random << 32);

	for (i = 0; i <= count / mem_size; i++) {
		copy_base = (unsigned long)buf +  mem_size * i;

		if (mem_size * (i + 1) > count)
			copy_size = count - mem_size * i;
		else
			copy_size = mem_size;

		ret = copy_from_user(mem_base_virt,
				     (const void __user *)copy_base, copy_size);
		if (ret) {
			pr_err("%s: copy_from_user fail!!\n", __func__);
			ret =  -EFAULT;
			goto exit;
		}

		if (i == 0) {
			if (count <= mem_size)
				option = NSTATE_ALL | option_random;
			else
				option = NSTATE_INIT | option_random;
		} else if (i == count / mem_size) {
			if (count % mem_size != 0)
				option = NSTATE_FINAL | option_random;
			else
				break;
		} else {
			option = NSTATE_UPDATE | option_random;

			if (count % mem_size == 0 && i == count / mem_size - 1)
				option = NSTATE_FINAL | option_random;
		}

		pr_info("defendkey: copy_size:0x%lx, option:0x%llx\n",
			copy_size, option);

		if (defendkey_type.decrypt_dtb == E_DECRYPT_DTB)
			ret = aml_sec_boot_check(AML_D_P_IMG_DECRYPT,
						 mem_base_phy, copy_size, 0);
		else
			ret = aml_sec_boot_check(AML_D_P_UPGRADE_CHECK,
						 mem_base_phy,
						 copy_size, option);

		if (ret) {
			ret_value = RET_FAIL;
			pr_err("%s: %s fail! ret %d\n", __func__,
			       defendkey_type.decrypt_dtb
			       ? "decrypt dtb" : "upgrade check", ret);
			goto exit;
		}
	}

	if (defendkey_type.decrypt_dtb)
		defendkey_type.status = 1;

	ret_value = RET_SUCCESS;
	pr_info("%s: %s OK\n", __func__,
		defendkey_type.decrypt_dtb ? "decrypt dtb" : "upgrade check");

exit:
	set_cpus_allowed_ptr(current, &task_cpumask);
	return ret_value;
}

static ssize_t version_show(struct class *cla,
			    struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "version:2.00\n");
}

static ssize_t secure_check_show(struct class *cla,
				 struct class_attribute *attr, char *buf)
{
	ssize_t n = 0;
	int ret;

	struct aml_defendkey_dev *defendkey_dev;

	defendkey_dev = container_of(cla, struct aml_defendkey_dev, cls);

	ret = aml_is_secure_set(defendkey_dev->reg_base);
	if (ret < 0)
		n = sprintf(buf, "fail");
	else if (ret == 0)
		n = sprintf(buf, "raw");
	else
		n = sprintf(buf, "encrypt");

	return n;
}

static ssize_t secure_verify_show(struct class *cla,
				  struct class_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t decrypt_dtb_show(struct class *cla,
				struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", defendkey_type.decrypt_dtb);
}

static ssize_t decrypt_dtb_store(struct class *cla,
				 struct class_attribute *attr,
				 const char *buf, size_t count)
{
	unsigned int len;

	if (buf[count - 1] == '\n')
		len = count - 1;
	else
		len = count;

	if (!strncmp(buf, "1", len))
		defendkey_type.decrypt_dtb = E_DECRYPT_DTB;
	else if (!strncmp(buf, "0", len))
		defendkey_type.decrypt_dtb = E_UPGRADE_CHECK;
	else
		pr_info("set defendkey type fail, invalid value\n");

	return count;
}

static const struct file_operations defendkey_fops = {
	.owner      = THIS_MODULE,
	.llseek     = defendkey_llseek,
	.open       = defendkey_open,
	.release    = defendkey_release,
	.read       = defendkey_read,
	.write      = defendkey_write,
	.unlocked_ioctl      = defendkey_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = defendkey_compat_ioctl,
#endif
};

static CLASS_ATTR_RO(version);
static CLASS_ATTR_RO(secure_check);
static CLASS_ATTR_RO(secure_verify);
static CLASS_ATTR_RW(decrypt_dtb);

static struct attribute *defendkey_class_attrs[] = {
	&class_attr_version.attr,
	&class_attr_secure_check.attr,
	&class_attr_secure_verify.attr,
	&class_attr_decrypt_dtb.attr,
	NULL,
};

ATTRIBUTE_GROUPS(defendkey_class);

static int __init early_defendkey_para(char *buf)
{
	int ret;

	if (!buf)
		return -EINVAL;

	ret = sscanf(buf, "%lx,%lx",
		     &defendkey_rmem.base, &defendkey_rmem.size);
	if (ret != 2) {
		pr_err("invalid boot args \"defendkey\"\n");
		return -EINVAL;
	}

	if (defendkey_rmem.base > DEFENDKEY_LIMIT_ADDR) {
		pr_err("defendkey reserved memory base overflow\n");
		return -EINVAL;
	}

	pr_info("%s, base:%lx, size:%lx\n",
		__func__, defendkey_rmem.base, defendkey_rmem.size);

	ret = memblock_reserve(defendkey_rmem.base,
			       PAGE_ALIGN(defendkey_rmem.size));
	if (ret < 0) {
		pr_info("reserve memblock %lx - %lx failed\n",
			defendkey_rmem.base,
			defendkey_rmem.base + PAGE_ALIGN(defendkey_rmem.size));
		return -EINVAL;
	}

	return 0;
}

early_param("defendkey", early_defendkey_para);

static int defendkey_probe(struct platform_device *pdev)
{
	int ret;
	struct device *devp;
	struct resource *reg_mem = NULL;
	void __iomem *reg_base = NULL;
	struct aml_defendkey_dev *defendkey_dev;

	reg_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!IS_ERR_OR_NULL(reg_mem)) {
		reg_base = devm_ioremap_resource(&pdev->dev, reg_mem);
		if (IS_ERR(reg_base)) {
			dev_err(&pdev->dev, "reg0: cannot obtain I/O memory region.\n");
			return PTR_ERR(reg_base);
		}
	} else {
		dev_err(&pdev->dev, "get IORESOURCE_MEM error.\n");
		return PTR_ERR(reg_base);
	}

	defendkey_dev = devm_kzalloc(&pdev->dev,
				     sizeof(*defendkey_dev), GFP_KERNEL);
	if (!defendkey_dev) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "failed to allocate mem for defendkey_dev\n");
		goto out;
	}

	defendkey_dev->pdev = pdev;
	platform_set_drvdata(pdev, defendkey_dev);

	ret = of_property_read_u32(pdev->dev.of_node,
				   "mem_size", (unsigned int *)&mem_size);
	if (ret) {
		dev_err(&pdev->dev, "failed to get mem_size\n");
		goto error1;
	}

	if (mem_size > defendkey_rmem.size) {
		ret = -EINVAL;
		dev_err(&pdev->dev, "reserved memory is not enough\n");
		goto error1;
	}

	ret = alloc_chrdev_region(&defendkey_dev->devno,
				  0, 1, DEFENDKEY_DEVICE_NAME);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to allocate major number\n");
		goto error1;
	}

	defendkey_dev->reg_base = reg_base;
	defendkey_dev->cls.name = DEFENDKEY_CLASS_NAME;
	defendkey_dev->cls.owner = THIS_MODULE;
	defendkey_dev->cls.class_groups = defendkey_class_groups;

	ret = class_register(&defendkey_dev->cls);
	if (ret) {
		dev_err(&pdev->dev, "failed to register class\n");
		goto error2;
	}

	cdev_init(&defendkey_dev->cdev, &defendkey_fops);
	defendkey_dev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&defendkey_dev->cdev, defendkey_dev->devno, 1);
	if (ret) {
		dev_err(&pdev->dev, "failed to add device\n");
		goto error3;
	}

	devp = device_create(&defendkey_dev->cls, NULL,
			     defendkey_dev->devno, NULL, DEFENDKEY_DEVICE_NAME);
	if (IS_ERR(devp)) {
		dev_err(&pdev->dev, "failed to create device node\n");
		ret = PTR_ERR(devp);
		goto error4;
	}

	dev_info(&pdev->dev, "device %s created OK\n", DEFENDKEY_DEVICE_NAME);
	return 0;

error4:
	cdev_del(&defendkey_dev->cdev);
error3:
	class_unregister(&defendkey_dev->cls);
error2:
	unregister_chrdev_region(defendkey_dev->devno, 1);
error1:
	devm_kfree(&pdev->dev, defendkey_dev);
out:
	return ret;
}

static int defendkey_remove(struct platform_device *pdev)
{
	struct aml_defendkey_dev *defendkey_dev = platform_get_drvdata(pdev);

	unregister_chrdev_region(defendkey_dev->devno, 1);
	device_destroy(&defendkey_dev->cls, defendkey_dev->devno);
	cdev_del(&defendkey_dev->cdev);
	class_unregister(&defendkey_dev->cls);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id defendkey_dt_match[] = {
	{	.compatible = "amlogic, defendkey",
	},
	{},
};

static struct platform_driver defendkey_driver = {
	.probe = defendkey_probe,
	.remove = defendkey_remove,
	.driver = {
		.name = DEFENDKEY_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = defendkey_dt_match,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(defendkey_driver);

MODULE_DESCRIPTION("AMLOGIC defendkey driver");
MODULE_LICENSE("GPL");

