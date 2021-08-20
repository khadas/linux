/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include <linux/platform_device.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/major.h>
#include "pts_server_core.h"
#include "pts_server_dev.h"


#define PTSSERVER_DEVICE_NAME   "ptsserver"
static struct device *ptsserver_dev;

typedef struct pspriv_s {
	s32 mPtsServerInsId;
	ptsserver_ins *pServerIns;
} ptsserver_priv_s;


static int ptsserver_open(struct inode *inode, struct file *file)
{
	ptsserver_priv_s *priv = {0};
	priv = kzalloc(sizeof(ptsserver_priv_s), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;
	priv->mPtsServerInsId = -1;
	file->private_data = priv;
	return 0;

}

static int ptsserver_release(struct inode *inode, struct file *file)
{
	//long ret = 0;
	ptsserver_priv_s *priv = (ptsserver_priv_s *)file->private_data;
	if (priv == NULL) {
		return -ENOMEM;
	}
	if (priv->mPtsServerInsId != -1) {
		ptsserver_ins_release(priv->mPtsServerInsId);
		priv->mPtsServerInsId = -1;
		priv->pServerIns = NULL;
	}
	kfree(priv);
	return 0;
}

static long ptsserver_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;
	s32 PServerInsId = -1;
	ptsserver_priv_s *priv = (ptsserver_priv_s *)file->private_data;
	ptsserver_ins *PServerIns = NULL;
	ptsserver_alloc_para allocparm = {0};
	checkin_pts_size mCheckinPtsSize = {0};
	checkout_pts_offset mCheckOutPtsOffset = {0};
	checkout_pts_offset mPeekPtsOffset = {0};
	start_offset mStartOffset = {0};
	last_checkin_pts mLastCheckinPts = {0};
	last_checkout_pts mLastCheckOutPts = {0};
	switch (cmd) {
		case PTSSERVER_IOC_INSTANCE_ALLOC:
			if (copy_from_user ((void *)&allocparm,
							(void *)arg,
							sizeof(allocparm))) {
				return -EFAULT;
			}

			if (ptsserver_ins_alloc(
						&PServerInsId,
						&PServerIns,
						&allocparm) < 0) {
				return -EFAULT;
			}

			if (PServerIns == NULL) {
				return -EFAULT;
			}

			if (priv != NULL) {
				priv->mPtsServerInsId = PServerInsId;
				priv->pServerIns = PServerIns;
			}
		break;

		case PTSSERVER_IOC_INSTANCE_GET:
			if (priv->pServerIns == NULL) {
				return -EFAULT;
			}

			PServerInsId = priv->mPtsServerInsId;
			if (copy_to_user((void *)arg,
					&PServerInsId,
					sizeof(PServerInsId))) {
				return -EFAULT;
			}
		break;

		case PTSSERVER_IOC_CHECKIN_PTS:
			if (priv->pServerIns == NULL) {
				return -EFAULT;
			}

			if (copy_from_user ((void *)&mCheckinPtsSize,
							(void *)arg,
							sizeof(checkin_pts_size))) {
				return -EFAULT;
			}
			ret = ptsserver_checkin_pts_size(priv->mPtsServerInsId,&mCheckinPtsSize);
		break;

		case PTSSERVER_IOC_CHECKOUT_PTS:
			if (priv->pServerIns == NULL) {
				return -EFAULT;
			}
			if (copy_from_user ((void *)&mCheckOutPtsOffset,
							(void *)arg,
							sizeof(checkout_pts_offset))) {
				return -EFAULT;
			}
			ret = ptsserver_checkout_pts_offset(priv->mPtsServerInsId,&mCheckOutPtsOffset);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&mCheckOutPtsOffset,
						sizeof(checkout_pts_offset)))
					return -EFAULT;
			}
		break;

		case PTSSERVER_IOC_PEEK_PTS:
			if (priv->pServerIns == NULL) {
				return -EFAULT;
			}
			if (copy_from_user ((void *)&mPeekPtsOffset,
							(void *)arg,
							sizeof(mPeekPtsOffset))) {
				return -EFAULT;
			}
			ret = ptsserver_peek_pts_offset(priv->mPtsServerInsId,&mPeekPtsOffset);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&mPeekPtsOffset,
						sizeof(struct checkoutptsoffset)))
					return -EFAULT;
			}
		break;

		case PTSSERVER_IOC_SET_ALIGNMENT_OFFSET:
			if (priv->pServerIns == NULL) {
				return -EFAULT;
			}
			if (copy_from_user ((void *)&mStartOffset,
							(void *)arg,
							sizeof(mStartOffset))) {
				return -EFAULT;
			}
			ret = ptsserver_set_first_checkin_offset(priv->mPtsServerInsId,&mStartOffset);
		break;

		case PTSSERVER_IOC_GET_LAST_CHECKIN_PTS:
			if (priv->pServerIns == NULL) {
				return -EFAULT;
			}
			ret = ptsserver_get_last_checkin_pts(priv->mPtsServerInsId,&mLastCheckinPts);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&mLastCheckinPts,
						sizeof(mLastCheckinPts)))
					return -EFAULT;
			}

		break;

		case PTSSERVER_IOC_GET_LAST_CHECKOUT_PTS:
			if (priv->pServerIns == NULL) {
				return -EFAULT;
			}
			ret = ptsserver_get_last_checkout_pts(priv->mPtsServerInsId,&mLastCheckOutPts);
			if (ret == 0) {
				if (copy_to_user((void *)arg,
						&mLastCheckOutPts,
						sizeof(mLastCheckOutPts)))
					return -EFAULT;
			}
		break;
		case PTSSERVER_IOC_RELEASE:
			if (priv->pServerIns == NULL) {
				return -EFAULT;
			}
			ret = ptsserver_ins_release(priv->mPtsServerInsId);
		break;
		default:
			pr_info("invalid cmd:%d\n", cmd);
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long ptsserver_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;
	switch (cmd) {
		case PTSSERVER_IOC_INSTANCE_ALLOC:
		case PTSSERVER_IOC_INSTANCE_GET:
		case PTSSERVER_IOC_CHECKIN_PTS:
		case PTSSERVER_IOC_CHECKOUT_PTS:
		case PTSSERVER_IOC_PEEK_PTS:
		case PTSSERVER_IOC_SET_ALIGNMENT_OFFSET:
		case PTSSERVER_IOC_GET_LAST_CHECKIN_PTS:
		case PTSSERVER_IOC_GET_LAST_CHECKOUT_PTS:
		case PTSSERVER_IOC_RELEASE:
			return ptsserver_ioctl(file, cmd, arg);
		default:
			return -EINVAL;
	}
	return ret;

}
#endif

static const struct file_operations ptsserver_fops = {
	.owner = THIS_MODULE,
	.open = ptsserver_open,
	.release = ptsserver_release,
	.unlocked_ioctl = ptsserver_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ptsserver_compat_ioctl,
#endif
};

static struct attribute *ptsserver_class_attrs[] = {
	NULL
};

ATTRIBUTE_GROUPS(ptsserver_class);

static struct class ptsserver_class = {
	.name = "ptsserver",
	.class_groups = ptsserver_class_groups,
};

static int __init ptsserver_module_init(void)
{
	int r;

	r = class_register(&ptsserver_class);

	if (r) {
		pr_err("ptsserver class create fail.\n");
		return r;
	}

	/* create tsync device */
	r = register_chrdev(PTSSERVER_MAJOR, "ptsserver", &ptsserver_fops);
	if (r < 0) {
		pr_info("Can't register major for ptsserver\n");
		goto err2;
	}

	ptsserver_dev = device_create(&ptsserver_class, NULL,
				MKDEV(PTSSERVER_MAJOR, 0), NULL, PTSSERVER_DEVICE_NAME);

	if (IS_ERR(ptsserver_dev)) {
		pr_err("Can't create ptsserver_dev device\n");
		goto err1;
	}
	ptsserver_init();
	return 0;

err1:
	unregister_chrdev(PTSSERVER_MAJOR, "ptsserver");
err2:
	class_unregister(&ptsserver_class);

	return 0;
}

static void __exit ptsserver_module_exit(void)
{
	device_destroy(&ptsserver_class, MKDEV(PTSSERVER_MAJOR, 0));
	unregister_chrdev(PTSSERVER_MAJOR, "ptsserver");
	class_unregister(&ptsserver_class);
}

module_init(ptsserver_module_init);
module_exit(ptsserver_module_exit);

MODULE_DESCRIPTION("AMLOGIC pts server management driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yun Min <yunmin.chen@amlogic.com>");


