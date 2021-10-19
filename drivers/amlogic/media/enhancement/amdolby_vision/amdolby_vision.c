/*
 * drivers/amlogic/media/enhancement/amdolby_vision/amdolby_vision.c
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

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include "../amvecm/arch/vpp_regs.h"
#include "../amvecm/arch/vpp_hdr_regs.h"
#include "../amvecm/arch/vpp_dolbyvision_regs.h"
#include "../amvecm/amcsc.h"
#include <linux/amlogic/media/registers/regs/viu_regs.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/dma-contiguous.h>
#include <linux/amlogic/iomap.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include "amdolby_vision.h"

#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/ctype.h>/* for parse_para_pq */
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/arm-smccc.h>
#include <linux/spinlock.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>


int get_dv_support_info(void)
{
	return 0;
}
EXPORT_SYMBOL(get_dv_support_info);

void dolby_vision_update_pq_config(char *pq_config_buf)
{

}

void dolby_vision_update_vsvdb_config(char *vsvdb_buf, u32 tbl_size)
{

}

int dolby_vision_update_setting(void)
{
	return -1;
}
EXPORT_SYMBOL(dolby_vision_update_setting);

void update_graphic_width_height(unsigned int width,
	unsigned int height)
{

}

void enable_dolby_vision(int enable)
{
	return;
}
EXPORT_SYMBOL(enable_dolby_vision);


int get_dolby_vision_src_format(void)
{
	return 0;
}
EXPORT_SYMBOL(get_dolby_vision_src_format);


void dolby_vision_set_provider(char *prov_name)
{

}
EXPORT_SYMBOL(dolby_vision_set_provider);

/* 0: no dv, 1: dv std, 2: dv ll */
int is_dovi_frame(struct vframe_s *vf)
{
	return 0;
}
EXPORT_SYMBOL(is_dovi_frame);

bool is_dovi_dual_layer_frame(struct vframe_s *vf)
{
	return false;
}
EXPORT_SYMBOL(is_dovi_dual_layer_frame);



int dolby_vision_check_mvc(struct vframe_s *vf)
{
	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_mvc);

int dolby_vision_check_hlg(struct vframe_s *vf)
{
	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_hlg);

int dolby_vision_check_hdr10plus(struct vframe_s *vf)
{
	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_hdr10plus);

int dolby_vision_check_hdr10(struct vframe_s *vf)
{

	return 0;
}
EXPORT_SYMBOL(dolby_vision_check_hdr10);

void dolby_vision_vf_put(struct vframe_s *vf)
{

}
EXPORT_SYMBOL(dolby_vision_vf_put);

struct vframe_s *dolby_vision_vf_peek_el(struct vframe_s *vf)
{
	return NULL;
}
EXPORT_SYMBOL(dolby_vision_vf_peek_el);


bool is_dv_standard_es(
	int dvel, int mflag, int width)
{
	return true;
}

int dolby_vision_parse_metadata(
	struct vframe_s *vf, u8 toggle_mode,
	bool bypass_release, bool drop_flag)
{
	return 0;
}
EXPORT_SYMBOL(dolby_vision_parse_metadata);


int dolby_vision_wait_metadata(struct vframe_s *vf)
{
	return 0;
}

int dolby_vision_update_metadata(struct vframe_s *vf, bool drop_flag)
{
	return -1;
}
EXPORT_SYMBOL(dolby_vision_update_metadata);


int dolby_vision_process(
	struct vframe_s *vf, u32 display_size,
	u8 toggle_mode, u8 pps_state)
{
	return 0;
}
EXPORT_SYMBOL(dolby_vision_process);

bool is_dolby_vision_on(void)
{
	return 0;
}
EXPORT_SYMBOL(is_dolby_vision_on);

bool is_dolby_vision_video_on(void)
{
	return 0;
}
EXPORT_SYMBOL(is_dolby_vision_video_on);

bool for_dolby_vision_certification(void)
{
	return 0;
}
EXPORT_SYMBOL(for_dolby_vision_certification);

bool for_dolby_vision_video_effect(void)
{
	return 0;
}
EXPORT_SYMBOL(for_dolby_vision_video_effect);


void dolby_vision_set_toggle_flag(int flag)
{

}
EXPORT_SYMBOL(dolby_vision_set_toggle_flag);


int get_dolby_vision_mode(void)
{
	return 0;
}
EXPORT_SYMBOL(get_dolby_vision_mode);

int get_dolby_vision_target_mode(void)
{
	return 0;
}
EXPORT_SYMBOL(get_dolby_vision_target_mode);

bool is_dolby_vision_enable(void)
{
	return 0;
}
EXPORT_SYMBOL(is_dolby_vision_enable);

bool is_dolby_vision_stb_mode(void)
{
	return 0;
}
EXPORT_SYMBOL(is_dolby_vision_stb_mode);

bool is_dolby_vision_el_disable(void)
{
	return 1;
}
EXPORT_SYMBOL(is_dolby_vision_el_disable);

void set_dolby_vision_policy(int policy)
{

}
EXPORT_SYMBOL(set_dolby_vision_policy);

int get_dolby_vision_policy(void)
{
	return 0;
}
EXPORT_SYMBOL(get_dolby_vision_policy);

int get_dolby_vision_hdr_policy(void)
{
	return 0;
}
EXPORT_SYMBOL(get_dolby_vision_hdr_policy);

void dv_vf_light_unreg_provider(void)
{

}
EXPORT_SYMBOL(dv_vf_light_unreg_provider);

void dv_vf_light_reg_provider(void)
{

}
EXPORT_SYMBOL(dv_vf_light_reg_provider);

void dolby_vision_update_backlight(void)
{

}
EXPORT_SYMBOL(dolby_vision_update_backlight);

/* to re-init the src format after video off -> on case */
int dolby_vision_update_src_format(struct vframe_s *vf, u8 toggle_mode)
{
	return 1;
}
EXPORT_SYMBOL(dolby_vision_update_src_format);

/*return 0: parse ok; 1,2,3: parse err */
int parse_sei_and_meta_ext(struct vframe_s *vf,
	 char *aux_buf,
	 int aux_size,
	 int *total_comp_size,
	 int *total_md_size,
	 void *fmt,
	 int *ret_flags,
	 char *md_buf,
	 char *comp_buf)
{
	return 1;
}

static int amdolby_vision_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int amdolby_vision_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long amdolby_vision_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static long amdolby_vision_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = amdolby_vision_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations amdolby_vision_fops = {
	.owner   = THIS_MODULE,
	.open    = amdolby_vision_open,
	.write   = NULL,
	.read = NULL,
	.release = amdolby_vision_release,
	.unlocked_ioctl   = amdolby_vision_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amdolby_vision_compat_ioctl,
#endif
	.poll = NULL,
};

unsigned int dolby_vision_check_enable(void)
{
	return 0;
}

static struct dv_device_data_s dolby_vision_gxm = {
	.cpu_id = _CPU_MAJOR_ID_GXM,
};

static struct dv_device_data_s dolby_vision_txlx = {
	.cpu_id = _CPU_MAJOR_ID_TXLX,
};

static struct dv_device_data_s dolby_vision_g12 = {
	.cpu_id = _CPU_MAJOR_ID_G12,
};

static struct dv_device_data_s dolby_vision_tm2 = {
	.cpu_id = _CPU_MAJOR_ID_TM2,
};

static struct dv_device_data_s dolby_vision_tm2_revb = {
	.cpu_id = _CPU_MAJOR_ID_TM2_REVB,
};

static const struct of_device_id amlogic_dolby_vision_match[] = {
	{
		.compatible = "amlogic, dolby_vision_gxm",
		.data = &dolby_vision_gxm,
	},
	{
		.compatible = "amlogic, dolby_vision_txlx",
		.data = &dolby_vision_txlx,
	},
	{
		.compatible = "amlogic, dolby_vision_g12a",
		.data = &dolby_vision_g12,
	},
	{
		.compatible = "amlogic, dolby_vision_g12b",
		.data = &dolby_vision_g12,
	},
	{
		.compatible = "amlogic, dolby_vision_sm1",
		.data = &dolby_vision_g12,
	},
	{
		.compatible = "amlogic, dolby_vision_tm2",
		.data = &dolby_vision_tm2,
	},
	{
		.compatible = "amlogic, dolby_vision_tm2_revb",
		.data = &dolby_vision_tm2_revb,
	},
	{},
};

static int amdolby_vision_probe(struct platform_device *pdev)
{
	pr_info("\n amdolby_vision probe start\n");
	pr_info("[amdolby_vision.] : amdolby_vision error.\n");
	return -ENODEV;
}

static int __exit amdolby_vision_remove(struct platform_device *pdev)
{
	pr_info("[ amdolby_vision.] :  amdolby_vision_exit.\n");
	return 0;
}

static struct platform_driver aml_amdolby_vision_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "aml_amdolby_vision_driver",
		.of_match_table = amlogic_dolby_vision_match,
	},
	.probe = amdolby_vision_probe,
	.remove = __exit_p(amdolby_vision_remove),
};

static int __init amdolby_vision_init(void)
{
	pr_info("%s:module init\n", __func__);

	if (platform_driver_register(&aml_amdolby_vision_driver)) {
		pr_err("failed to register amdolby_vision module\n");
		return -ENODEV;
	}
	return 0;
}

static void __exit amdolby_vision_exit(void)
{
	pr_info("%s:module exit\n", __func__);
	platform_driver_unregister(&aml_amdolby_vision_driver);
}

module_init(amdolby_vision_init);
module_exit(amdolby_vision_exit);

MODULE_DESCRIPTION("AMLOGIC amdolby_vision driver");
MODULE_LICENSE("GPL");


