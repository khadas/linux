// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "vpp_common.h"
#include "vpp_drv.h"
#include "vpp_pq_mgr.h"
#include "vpp_debug.h"

unsigned int pr_lvl;
struct vpp_dev_s *vpp_dev;

struct vpp_dev_s *get_vpp_dev(void)
{
	return vpp_dev;
}

const struct class_attribute vpp_class_attr[] = {
	__ATTR(vpp_debug, 0644,
		vpp_debug_show,
		vpp_debug_store),
	__ATTR(vpp_brightness, 0644,
		vpp_debug_brightness_show,
		vpp_debug_brightness_store),
	__ATTR(vpp_brightness_post, 0644,
		vpp_debug_brightness_post_show,
		vpp_debug_brightness_post_store),
	__ATTR(vpp_contrast, 0644,
		vpp_debug_contrast_show,
		vpp_debug_contrast_store),
	__ATTR(vpp_contrast_post, 0644,
		vpp_debug_contrast_post_show,
		vpp_debug_contrast_post_store),
	__ATTR(vpp_sat, 0644,
		vpp_debug_saturation_show,
		vpp_debug_saturation_store),
	__ATTR(vpp_sat_post, 0644,
		vpp_debug_saturation_post_show,
		vpp_debug_saturation_post_store),
	__ATTR(vpp_hue, 0644,
		vpp_debug_hue_show,
		vpp_debug_hue_store),
	__ATTR(vpp_hue_post, 0644,
		vpp_debug_hue_post_show,
		vpp_debug_hue_post_store),
	__ATTR(vpp_pre_gamma, 0644,
		vpp_debug_pre_gamma_show,
		vpp_debug_pre_gamma_store),
	__ATTR(vpp_gamma, 0644,
		vpp_debug_gamma_show,
		vpp_debug_gamma_store),
	__ATTR(vpp_pre_gamma_pattern, 0644,
		vpp_debug_pre_gamma_pattern_show,
		vpp_debug_pre_gamma_pattern_store),
	__ATTR(vpp_gamma_pattern, 0644,
		vpp_debug_gamma_pattern_show,
		vpp_debug_gamma_pattern_store),
	__ATTR(vpp_white_balance, 0644,
		vpp_debug_whitebalance_show,
		vpp_debug_whitebalance_store),
	__ATTR(vpp_module_ctrl, 0644,
		vpp_debug_module_ctrl_show,
		vpp_debug_module_ctrl_store),
	__ATTR_NULL,
};

const struct match_data_s vpp_t3_match = {
	.chip_id = CHIP_T3,
};

const struct match_data_s vpp_match = {
	.chip_id = CHIP_MAX,
};

const struct of_device_id vpp_dts_match[] = {
	{
		.compatible = "amlogic, vpp-t3",
		.data = &vpp_t3_match,
	},
	{
		.compatible = "amlogic, vpp",
		.data = &vpp_match,
	},
};

static int vpp_open(struct inode *inode, struct file *filp)
{
	struct vpp_dev_s *vpp_devp;

	vpp_devp = container_of(inode->i_cdev, struct vpp_dev_s, vpp_cdev);
	filp->private_data = vpp_devp;
	return 0;
}

static int vpp_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	return 0;
}

static long vpp_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int val = 0;
	void __user *argp;
	unsigned int buffer_size;
	struct vpp_pq_state_s pq_state;
	struct vpp_white_balance_s wb_data;
	struct vpp_pre_gamma_table_s *ppre_gamma_data = NULL;
	struct vpp_gamma_table_s *pgamma_data = NULL;

	memset(&pq_state, 0, sizeof(struct vpp_pq_state_s));
	memset(&wb_data, 0, sizeof(struct vpp_white_balance_s));

	pr_vpp(PR_IOC, "%s cmd = %d\n", __func__, cmd);

	argp = (void __user *)arg;

	switch (cmd) {
	case VPP_IOC_SET_BRIGHTNESS:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_BRIGHTNESS failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_BRIGHTNESS success\n");
			ret = vpp_pq_mgr_set_brightness(val);
		}
		break;
	case VPP_IOC_SET_CONTRAST:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_CONTRAST failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_CONTRAST success\n");
			ret = vpp_pq_mgr_set_contrast(val);
		}
		break;
	case VPP_IOC_SET_SATURATION:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_SATURATION failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_SATURATION success\n");
			ret = vpp_pq_mgr_set_saturation(val);
		}
		break;
	case VPP_IOC_SET_HUE:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_HUE failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_HUE success\n");
			ret = vpp_pq_mgr_set_hue(val);
		}
		break;
	case VPP_IOC_SET_BRIGHTNESS_POST:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_BRIGHTNESS_POST failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_BRIGHTNESS_POST success\n");
			ret = vpp_pq_mgr_set_brightness_post(val);
		}
		break;
	case VPP_IOC_SET_CONTRAST_POST:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_CONTRAST_POST failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_CONTRAST_POST success\n");
			ret = vpp_pq_mgr_set_contrast_post(val);
		}
		break;
	case VPP_IOC_SET_SATURATION_POST:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_SATURATION_POST failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_SATURATION_POST success\n");
			ret = vpp_pq_mgr_set_saturation_post(val);
		}
		break;
	case VPP_IOC_SET_HUE_POST:
		if (copy_from_user(&val, argp, sizeof(int))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_HUE_POST failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_HUE_POST success\n");
			ret = vpp_pq_mgr_set_hue_post(val);
		}
		break;
	case VPP_IOC_SET_WB:
		if (copy_from_user(&wb_data, argp,
			sizeof(struct vpp_white_balance_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_WB failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_WB success\n");
			ret = vpp_pq_mgr_set_whitebalance(&wb_data);
		}
		break;
	case VPP_IOC_SET_PRE_GAMMA_DATA:
		buffer_size = sizeof(struct vpp_pre_gamma_table_s);
		ppre_gamma_data = kmalloc(buffer_size, GFP_KERNEL);
		if (!ppre_gamma_data) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_PRE_GAMMA_DATA malloc failed\n");
			ret = -ENOMEM;
		} else {
			if (copy_from_user(ppre_gamma_data, argp, buffer_size)) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_PRE_GAMMA_DATA failed\n");
				ret = -EFAULT;
			} else {
				pr_vpp(PR_IOC, "VPP_IOC_SET_PRE_GAMMA_DATA success\n");
				ret = vpp_pq_mgr_set_pre_gamma_table(ppre_gamma_data);
			}
			kfree(ppre_gamma_data);
		}
		break;
	case VPP_IOC_SET_GAMMA_DATA:
		buffer_size = sizeof(struct vpp_gamma_table_s);
		pgamma_data = kmalloc(buffer_size, GFP_KERNEL);
		if (!pgamma_data) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_GAMMA_DATA malloc failed\n");
			ret = -ENOMEM;
		} else {
			if (copy_from_user(pgamma_data, argp, buffer_size)) {
				pr_vpp(PR_IOC, "VPP_IOC_SET_GAMMA_DATA failed\n");
				ret = -EFAULT;
			} else {
				pr_vpp(PR_IOC, "VPP_IOC_SET_GAMMA_DATA success\n");
				ret = vpp_pq_mgr_set_gamma_table(pgamma_data);
			}
			kfree(pgamma_data);
		}
		break;
	case VPP_IOC_SET_PQ_STATE:
		if (copy_from_user(&pq_state, argp,
			sizeof(struct vpp_pq_state_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_SET_PQ_STATE failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_SET_PQ_STATE success\n");
			ret = vpp_pq_mgr_set_status(&pq_state);
		}
		break;
	case VPP_IOC_GET_PQ_STATE:
		vpp_pq_mgr_get_status(&pq_state);
		if (copy_to_user(argp, &pq_state,
			sizeof(struct vpp_pq_state_s))) {
			pr_vpp(PR_IOC, "VPP_IOC_GET_PQ_STATE failed\n");
			ret = -EFAULT;
		} else {
			pr_vpp(PR_IOC, "VPP_IOC_GET_PQ_STATE success\n");
		}
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static long vpp_compat_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = vpp_ioctl(filp, cmd, arg);
	return ret;
}

const struct file_operations vpp_fops = {
	.owner = THIS_MODULE,
	.open = vpp_open,
	.release = vpp_release,
	.unlocked_ioctl = vpp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vpp_compat_ioctl,
#endif
};

static void fw_align_hw_config(struct vpp_dev_s *devp)
{
	enum vpp_chip_type_e chip_id;

	chip_id = devp->pm_data->chip_id;

	switch (chip_id) {
	case CHIP_T3:
		devp->pq_en = 1;
		devp->vpp_cfg_data.dnlp_hw = SR1_DNLP;
		devp->vpp_cfg_data.sr_hw = SR0_SR1;
		devp->vpp_cfg_data.bit_depth = BDP_10;
		devp->vpp_cfg_data.gamma_hw = GM_V2;
		devp->vpp_cfg_data.vpp_out.vpp0_out = OUT_PANEL;
		devp->vpp_cfg_data.vpp_out.vpp1_out = OUT_NULL;
		devp->vpp_cfg_data.vpp_out.vpp2_out = OUT_NULL;
		/*vlock data*/
		//devp->vpp_cfg_data.vlk_data.vlk_support = true;
		//devp->vpp_cfg_data.vlk_data.vlk_new_fsm = 1;
		//devp->vpp_cfg_data.vlk_data.vlk_hwver = vlock_hw_tm2verb;
		//devp->vpp_cfg_data.vlk_data.vlk_phlock_en = true;
		//devp->vpp_cfg_data.vlk_data.vlk_pll_sel = vlock_pll_sel_tcon;
		break;
	case CHIP_MAX:
	default:
		devp->vpp_cfg_data.dnlp_hw = SR1_DNLP;
		devp->vpp_cfg_data.sr_hw = SR0_SR1;
		devp->vpp_cfg_data.bit_depth = BDP_10;
		devp->vpp_cfg_data.gamma_hw = GM_V2;
		devp->vpp_cfg_data.vpp_out.vpp0_out = OUT_PANEL;
		devp->vpp_cfg_data.vpp_out.vpp1_out = OUT_NULL;
		devp->vpp_cfg_data.vpp_out.vpp2_out = OUT_NULL;
		/*vlock data*/
		//devp->vpp_cfg_data.vlk_data.vlk_support = false;
		//devp->vpp_cfg_data.vlk_data.vlk_new_fsm = 0;
		//devp->vpp_cfg_data.vlk_data.vlk_hwver = vlock_hw_tm2verb;
		//devp->vpp_cfg_data.vlk_data.vlk_phlock_en = false;
		//devp->vpp_cfg_data.vlk_data.vlk_pll_sel = vlock_pll_sel_tcon;
		devp->pq_en = 0;
		break;
	}
}

void vpp_attach_init(struct vpp_dev_s *devp)
{
	PR_DRV("%s attach_init in\n", __func__);

	vpp_pq_mgr_init(devp);

	return;
	//hw_ops_attach(devp->vpp_ops.hw_ops);
}

static int vpp_dts_parse(struct vpp_dev_s *vpp_devp)
{
	int ret = 0;
	int val;
	struct device_node *of_node;
	const struct of_device_id *of_id;
	struct platform_device *pdev = vpp_devp->pdev;

	of_id = of_match_device(vpp_dts_match, &pdev->dev);
	if (of_id) {
		PR_DRV("%s compatible\n", of_id->compatible);
		vpp_devp->pm_data = (struct match_data_s *)of_id->data;
	}

	of_node = pdev->dev.of_node;
	if (of_node) {
		ret = of_property_read_u32(of_node, "pq_en", &val);
		if (ret)
			PR_DRV("of get node pq_en failed\n");
		else
			vpp_devp->pq_en = val;
	}

	return ret;
}

static int vpp_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	struct vpp_dev_s *vpp_devp = NULL;

	PR_DRV("%s:In\n", __func__);

	vpp_dev = kzalloc(sizeof(*vpp_dev), GFP_KERNEL);
	if (!vpp_dev) {
		PR_ERR("vpp dev kzalloc error\n");
		ret = -1;
		goto fail_alloc_dev;
	}
	memset(vpp_dev, 0, sizeof(struct vpp_dev_s));
	vpp_devp = vpp_dev;

	ret = alloc_chrdev_region(&vpp_devp->devno, 0, VPP_DEVNO, VPP_NAME);
	if (ret < 0) {
		PR_ERR("vpp devno alloc failed\n");
		goto fail_alloc_region;
	}

	vpp_devp->clsp = class_create(THIS_MODULE, VPP_CLS_NAME);
	if (IS_ERR(vpp_devp->clsp)) {
		PR_ERR("vpp class create failed\n");
		ret = -1;
		goto fail_create_class;
	}

	for (i = 0; vpp_class_attr[i].attr.name; i++) {
		ret = class_create_file(vpp_devp->clsp, &vpp_class_attr[i]);
		if (ret < 0) {
			PR_ERR("vpp class create file failed\n");
			goto fail_create_class_file;
		}
	}

	cdev_init(&vpp_devp->vpp_cdev, &vpp_fops);
	vpp_devp->vpp_cdev.owner = THIS_MODULE;
	ret = cdev_add(&vpp_devp->vpp_cdev, vpp_devp->devno, VPP_DEVNO);
	if (ret < 0) {
		PR_ERR("vpp add cdev failed\n");
		goto fail_add_cdev;
	}

	vpp_devp->dev = device_create(vpp_devp->clsp, NULL,
		vpp_devp->devno, vpp_devp, VPP_NAME);
	if (!vpp_devp->dev) {
		PR_ERR("vpp device_create failed\n");
		ret = -1;
		goto fail_create_dev;
	}

	dev_set_drvdata(vpp_devp->dev, vpp_devp);
	platform_set_drvdata(pdev, vpp_devp);
	vpp_devp->pdev = pdev;

	vpp_dts_parse(vpp_devp);

	/*need get vout first, need confirm which vpp out, panel or tx*/

	/*get vout end*/
	fw_align_hw_config(vpp_devp);
	vpp_attach_init(vpp_devp);

	return ret;

fail_create_dev:
	cdev_del(&vpp_devp->vpp_cdev);
fail_add_cdev:
	for (i = 0; vpp_class_attr[i].attr.name; i++)
		class_remove_file(vpp_devp->clsp, &vpp_class_attr[i]);
fail_create_class_file:
	class_destroy(vpp_devp->clsp);
fail_create_class:
	unregister_chrdev_region(vpp_devp->devno, VPP_DEVNO);
fail_alloc_region:
	kfree(vpp_dev);
	vpp_dev = NULL;
fail_alloc_dev:
	return ret;
}

static int vpp_drv_remove(struct platform_device *pdev)
{
	int i;
	struct vpp_dev_s *vpp_devp = get_vpp_dev();

	device_destroy(vpp_devp->clsp, vpp_devp->devno);
	cdev_del(&vpp_devp->vpp_cdev);
	for (i = 0; vpp_class_attr[i].attr.name; i++)
		class_remove_file(vpp_devp->clsp, &vpp_class_attr[i]);
	class_destroy(vpp_devp->clsp);
	unregister_chrdev_region(vpp_devp->devno, VPP_DEVNO);
	kfree(vpp_dev);
	vpp_dev = NULL;

	return 0;
}

static void vpp_drv_shutdown(struct platform_device *pdev)
{
	int i;
	struct vpp_dev_s *vpp_devp = get_vpp_dev();

	device_destroy(vpp_devp->clsp, vpp_devp->devno);
	cdev_del(&vpp_devp->vpp_cdev);
	for (i = 0; vpp_class_attr[i].attr.name; i++)
		class_remove_file(vpp_devp->clsp, &vpp_class_attr[i]);
	class_destroy(vpp_devp->clsp);
	unregister_chrdev_region(vpp_devp->devno, VPP_DEVNO);
	kfree(vpp_dev);
	vpp_dev = NULL;
}

static int vpp_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int vpp_drv_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver vpp_driver = {
	.probe = vpp_drv_probe,
	.remove = vpp_drv_remove,
	.shutdown = vpp_drv_shutdown,
	.suspend = vpp_drv_suspend,
	.resume = vpp_drv_resume,
	.driver = {
		.name = "aml_vpp",
		.owner = THIS_MODULE,
		.of_match_table = vpp_dts_match,
	},
};

int __init vpp_drv_init(void)
{
	PR_DRV("%s module init\n", __func__);

	if (platform_driver_register(&vpp_driver)) {
		PR_ERR("%s module init failed\n", __func__);
		return -ENODEV;
	}

	return 0;
}

void __exit vpp_drv_exit(void)
{
	PR_DRV("%s:module exit\n", __func__);
	platform_driver_unregister(&vpp_driver);
}

