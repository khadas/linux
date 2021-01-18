// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>

#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/amlogic/iomap.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/arm-smccc.h>

#include <linux/amlogic/media/registers/regs/viu_regs.h>
#include <linux/amlogic/media/vpu_secure/vpu_secure.h>
#include <linux/amlogic/media/video_sink/video_signal_notify.h>

#include "vpu_security.h"

#define DEVICE_NAME "vpu_security"
#define CLASS_NAME  "vpu_security"

static struct vpu_security_device_info vpu_security_info;
static struct sec_dev_data_s sec_meson_dev;
static u32 secure_cfg;

static struct sec_dev_data_s vpu_security_sc2 = {
	.cpu_type = MESON_CPU_MAJOR_ID_SC2_,
};

static const struct of_device_id vpu_security_dt_match[] = {
	{
		.compatible = "amlogic, meson-sc2, vpu_security",
		.data = &vpu_security_sc2,
	},
	{}
};

static bool is_meson_sc2_cpu(void)
{
	if (sec_meson_dev.cpu_type ==
		MESON_CPU_MAJOR_ID_SC2_)
		return true;
	else
		return false;
}

static int check_secure_enable(enum secure_module_e module)
{
	int i, secure_enable = 0;
	struct vpu_security_device_info *info = &vpu_security_info;
	struct vpu_secure_ins *ins = NULL;

	for (i = 0; i < MODULE_NUM; i++) {
		if (i == module)
			continue;
		ins = &info->ins[i];
		if (ins->secure_enable)
			secure_enable = 1;
	}
	return secure_enable;
}

bool get_secure_state(enum module_port_e port)
{
	bool secure_enable = false;

	switch (port) {
	case VD1_OUT:
		if (secure_cfg & VD1_INPUT_SECURE)
			secure_enable = true;
		break;
	case VD2_OUT:
		if (secure_cfg & VD2_INPUT_SECURE)
			secure_enable = true;
		break;
	case OSD1_VPP_OUT:
		if ((secure_cfg & OSD1_INPUT_SECURE) ||
		    (secure_cfg & OSD2_INPUT_SECURE) ||
		    (secure_cfg & OSD3_INPUT_SECURE))
			secure_enable = true;
		break;
	case OSD2_VPP_OUT:
		break;
	case POST_BLEND_OUT:
		if (secure_cfg)
			secure_enable = true;
		break;
	}
	return secure_enable;
}

u32 set_vpu_module_security(struct vpu_secure_ins *ins,
			    enum secure_module_e module,
			    u32 secure_src)
{
	static u32 osd_secure, video_secure;
	static int value_save = -1;
	u32 value = 0;
	int secure_update = 0;
	struct vd_secure_info_s vd_secure[MAX_SECURE_OUT];

	if (is_meson_sc2_cpu()) {
		if (check_secure_enable(module) || secure_src)
			secure_src |= VPP_OUTPUT_SECURE;
		switch (module) {
		case OSD_MODULE:
			if ((secure_src & OSD1_INPUT_SECURE) ||
			    (secure_src & OSD2_INPUT_SECURE) ||
			    (secure_src & OSD3_INPUT_SECURE) ||
			    (secure_src & MALI_AFBCD_SECURE)) {
				/* OSD module secure */
				osd_secure = secure_src;
				value = osd_secure | video_secure;
				ins->secure_enable = 1;
				ins->secure_status = value;
			} else {
				/* OSD none secure */
				osd_secure = secure_src;
				value = osd_secure | video_secure;
				ins->secure_enable = 0;
				ins->secure_status = value;
			}
			break;
		case VIDEO_MODULE:
			if ((secure_src & DV_INPUT_SECURE) ||
			    (secure_src & AFBCD_INPUT_SECURE) ||
			    (secure_src & VD2_INPUT_SECURE) ||
			    (secure_src & VD1_INPUT_SECURE)) {
				/* video module secure */
				video_secure = secure_src;
				value = video_secure | osd_secure;
				ins->secure_enable = 1;
				ins->secure_status = value;
			} else {
				/* video module secure */
				video_secure = secure_src;
				value = video_secure | osd_secure;
				ins->secure_enable = 0;
				ins->secure_status = value;
			}
			break;
		case DI_MODULE:
			break;
		case VDIN_MODULE:
			break;
		}
		if (module == OSD_MODULE ||
			module == VIDEO_MODULE ||
			module == DI_MODULE) {
			if (ins->reg_wr_op && value_save != value) {
				ins->reg_wr_op(VIU_DATA_SEC, value);
				secure_update = 1;
			}
			value_save = value;
		}
	}
	secure_cfg = (osd_secure | video_secure) & (~VPP_OUTPUT_SECURE);
	if (secure_update) {
		int i;

		for (i = 0; i < MAX_SECURE_OUT; i++) {
			vd_secure[i].secure_type = i;
			vd_secure[i].secure_enable = get_secure_state(i);
		}
		vd_signal_notifier_call_chain
			(VIDEO_SECURE_TYPE_CHANGED,
			&vd_secure[0]);
	}
	return value;
}

int secure_register(enum secure_module_e module,
		    int config_delay,
		    void *reg_op,
		    void *cb)
{
	struct vpu_security_device_info *info = &vpu_security_info;
	struct vpu_secure_ins *ins = NULL;
	struct mutex *lock = NULL;

	if (!is_meson_sc2_cpu())
		return -1;
	if (!info->probed)
		return -1;
	if (module >= MODULE_NUM) {
		pr_info("%s failed, module = %d\n", __func__, module);
		return -1;
	}
	ins = &info->ins[module];
	if (!ins->registered) {
		lock = &info->ins[module].secure_lock;
		mutex_lock(lock);
		ins->registered = 1;
		ins->config_delay = config_delay;
		ins->reg_wr_op = reg_op;
		ins->secure_cb = cb;
		mutex_unlock(lock);
	}
	pr_info("%s module=%d ok\n", __func__, module);
	return 0;
}

int secure_unregister(enum secure_module_e module)
{
	struct vpu_security_device_info *info = &vpu_security_info;
	struct vpu_secure_ins *ins = NULL;
	struct mutex *lock = NULL;

	if (!is_meson_sc2_cpu())
		return -1;
	if (!info->probed)
		return -1;
	if (module >= MODULE_NUM)
		return -1;
	ins = &info->ins[module];
	if (ins->registered) {
		lock = &info->ins[module].secure_lock;
		mutex_lock(lock);
		ins->registered = 0;
		ins->config_delay = 0;
		ins->reg_wr_op = NULL;
		ins->secure_cb = NULL;
		mutex_unlock(lock);
	}
	return 0;
}

int secure_config(enum secure_module_e module, int secure_src)
{
	struct vpu_security_device_info *info = &vpu_security_info;
	struct vpu_secure_ins *ins = NULL;
	u32 reg_value = -1;

	if (module >= MODULE_NUM)
		return -1;
	ins = &info->ins[module];
	if (ins->registered)
		reg_value = set_vpu_module_security(ins, module, secure_src);
	return 0;
}

static ssize_t vpu_security_info_show(struct class *cla,
				      struct class_attribute *attr, char *buf)
{
	struct vpu_security_device_info *info = &vpu_security_info;

	return snprintf(buf, PAGE_SIZE, "vpu_security mismatch cnt:%d\n",
		info->mismatch_cnt);
}

static struct class_attribute vpu_security_attrs[] = {
	__ATTR(security_info, 0444,
	       vpu_security_info_show, NULL),
};

irqreturn_t vpu_security_isr(int irq, void *dev_id)
{
	struct vpu_security_device_info *info = &vpu_security_info;
	struct vpu_secure_ins *ins = NULL;
	int i;

	info->mismatch_cnt++;
	for (i = 0; i < MODULE_NUM; i++) {
		ins = &info->ins[i];
		if (ins->registered) {
			if (ins->secure_cb)
				ins->secure_cb(ins->secure_status);
		}
	}
	return IRQ_HANDLED;
}

static int vpu_security_probe(struct platform_device *pdev)
{
	int i, ret = 0, err = 0;
	int int_vpu_security;
	struct vpu_security_device_info *info = &vpu_security_info;

	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		struct sec_dev_data_s *sec_meson;
		struct device_node	*of_node = pdev->dev.of_node;

		match = of_match_node(vpu_security_dt_match, of_node);
		if (match) {
			sec_meson =
				(struct sec_dev_data_s *)match->data;
			if (sec_meson)
				memcpy(&sec_meson_dev, sec_meson,
				       sizeof(struct sec_dev_data_s));
			else
				err = 1;

		} else {
			err = 2;
		}
	} else {
		err = 3;
	}
	if (err) {
		pr_err("dev %s NOT match, err=%d\n", __func__, err);
		return -ENODEV;
	}

	info->pdev = pdev;
	int_vpu_security = platform_get_irq_byname(pdev, "vpu_security");
	if (request_irq(int_vpu_security, &vpu_security_isr,
			IRQF_SHARED, "vpu_security", (void *)"vpu_security")) {
		dev_err(&pdev->dev, "can't request irq for vpu_security\n");
		return -ENODEV;
	}

	info->clsp = class_create(THIS_MODULE,
				  CLASS_NAME);
	if (IS_ERR(info->clsp)) {
		ret = PTR_ERR(info->clsp);
		pr_err("fail to create class\n");
		goto fail_create_class;
	}
	for (i = 0; i < ARRAY_SIZE(vpu_security_attrs); i++) {
		if (class_create_file
			(info->clsp,
			&vpu_security_attrs[i]) < 0) {
			pr_err("fail to class_create_file\n");
			goto fail_class_create_file;
		}
	}
	for (i = 0; i < MODULE_NUM; i++)
		mutex_init(&info->ins[i].secure_lock);

	info->probed = 1;
	pr_info("%s ok.\n", __func__);
	return 0;
fail_class_create_file:
	for (i = 0; i < ARRAY_SIZE(vpu_security_attrs); i++)
		class_remove_file
			(info->clsp, &vpu_security_attrs[i]);
	class_destroy(info->clsp);
	info->clsp = NULL;
fail_create_class:
	return ret;
}

static int vpu_security_remove(struct platform_device *pdev)
{
	int i;
	struct vpu_security_device_info *info = &vpu_security_info;

	for (i = 0; i < ARRAY_SIZE(vpu_security_attrs); i++)
		class_remove_file
			(info->clsp, &vpu_security_attrs[i]);
	class_destroy(info->clsp);
	info->clsp = NULL;
	info->probed = 0;
	return 0;
}

static struct platform_driver vpu_security_driver = {
	.probe = vpu_security_probe,
	.remove = vpu_security_remove,
	.driver = {
		.name = "amlogic_vpu_security",
		.of_match_table = vpu_security_dt_match,
	},
};

int __init vpu_security_init(void)
{
	int r;

	r = platform_driver_register(&vpu_security_driver);
	if (r) {
		pr_err("Unable to register vpu_security driver\n");
		return r;
	}
	return 0;
}

void __exit vpu_security_exit(void)
{
	platform_driver_unregister(&vpu_security_driver);
}

//MODULE_DESCRIPTION("AMLOGIC VPU SECURITY driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("PengCheng.Chen <pengcheng.chen@amlogic.com>");
