// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifdef MODULE

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/ctype.h>
#include <linux/kallsyms.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/extcon-provider.h>
#include <linux/mm.h>
#include "media_main.h"

#define PROTO(x...) x
#define ARGS(x...) x
#define KALLSYMS_FUNC_DEFRET(ret_type, name, args_proto, args) \
ret_type name(args_proto) \
{ \
	static ret_type (*func)(args_proto); \
	if (!func) \
		func = (void *)kallsyms_lookup_name(__func__); \
	if (!func) { \
		WARN("can't find symbol: %s\n", __func__); \
	} \
	return func(args);\
}

KALLSYMS_FUNC_DEFRET(phys_addr_t,
		     cma_get_base,
		     PROTO(const struct cma *cma),
		     ARGS(cma));

KALLSYMS_FUNC_DEFRET(unsigned long,
		     cma_get_size,
		     PROTO(const struct cma *cma),
		     ARGS(cma));

KALLSYMS_FUNC_DEFRET(struct page *,
		     dma_alloc_from_contiguous,
		     PROTO(struct device *dev, size_t count, unsigned int align,
			   bool no_warn),
		     ARGS(dev, count, align, no_warn));

KALLSYMS_FUNC_DEFRET(int,
		     cma_mmu_op,
		     PROTO(struct page *page, int count, bool set),
		     ARGS(page, count, set));

KALLSYMS_FUNC_DEFRET(struct extcon_dev *,
		     extcon_dev_allocate,
		     PROTO(const unsigned int *supported_cable),
		     ARGS(supported_cable));

KALLSYMS_FUNC_DEFRET(bool,
		     dma_release_from_contiguous,
		     PROTO(struct device *dev, struct page *pages, int count),
		     ARGS(dev, pages, count));

KALLSYMS_FUNC_DEFRET(int,
		     is_vmalloc_or_module_addr,
		     PROTO(const void *x),
		     ARGS(x));

KALLSYMS_FUNC_DEFRET(const void *,
		     of_get_flat_dt_prop,
		     PROTO(unsigned long node, const char *name, int *size),
		     ARGS(node, name, size));

#define call_sub_init(func) \
{ \
	int ret = 0; \
	ret = func(); \
	pr_info("call %s() ret=%d\n", #func, ret); \
}

static int __init media_main_init(void)
{
	pr_info("### %s() start\n", __func__);

	call_sub_init(media_configs_system_init);
	call_sub_init(codec_mm_module_init);
	call_sub_init(codec_io_init);
	call_sub_init(vdec_reg_ops_init);
	call_sub_init(vpu_init);
	call_sub_init(amcanvas_init);
	call_sub_init(amrdma_init);
	call_sub_init(amhdmitx_init);
	call_sub_init(osd_init_module);
	call_sub_init(vout_init_module);
	call_sub_init(cvbs_init_module);
	call_sub_init(lcd_init);
	call_sub_init(aml_vdac_init);
	call_sub_init(gp_pll_init);
	call_sub_init(ion_init);
	call_sub_init(vfm_class_init);
	call_sub_init(meson_cpu_version_init);
	call_sub_init(ge2d_init_module);
	call_sub_init(configs_init_devices);
	call_sub_init(video_init);
	call_sub_init(vout2_init_module);
	call_sub_init(ppmgr_init_module);
	call_sub_init(videosync_init);
	call_sub_init(picdec_init_module);
	call_sub_init(amdolby_vision_init);
	call_sub_init(tsync_module_init);
	call_sub_init(aml_bl_extern_i2c_init);
	call_sub_init(video_composer_module_init);
	call_sub_init(aml_lcd_extern_i2c_dev_init);
	call_sub_init(aml_vecm_init);
	call_sub_init(ionvideo_init);
	call_sub_init(v4lvideo_init);
	call_sub_init(amlvideo_init);
	call_sub_init(amlvideo2_init);
	call_sub_init(dil_init);
	call_sub_init(aml_bl_extern_init);
	call_sub_init(cec_init);
	call_sub_init(gdc_driver_init);
	call_sub_init(aml_lcd_extern_init);
	call_sub_init(ldim_dev_init);
	call_sub_init(aml_bl_init);

	pr_info("### %s() end\n", __func__);
	return 0;
}

static void __exit media_main_exit(void)
{
	pr_info("%s()\n", __func__);
}

module_init(media_main_init);
module_exit(media_main_exit);

MODULE_LICENSE("GPL v2");
#endif
