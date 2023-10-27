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
#ifdef MODULE
#include <linux/amlogic/gki_module.h>
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
KALLSYMS_FUNC_DEFRET(int,
		     is_vmalloc_or_module_addr,
		     PROTO(const void *x),
		     ARGS(x));
#endif
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
	call_sub_init(dmabuf_manage_init);
	call_sub_init(codec_mm_module_init);
	call_sub_init(codec_io_init);
	call_sub_init(vdec_reg_ops_init);
	call_sub_init(vpu_init);
	call_sub_init(aml_vclk_init_module);
	call_sub_init(vrr_init);
	call_sub_init(amcanvas_init);
	call_sub_init(amrdma_init);
	call_sub_init(vout_mux_init);
	call_sub_init(dsc_enc_init);
	call_sub_init(amhdmitx21_init);
	call_sub_init(amhdmitx_init);
	call_sub_init(aml_vdac_init);
	call_sub_init(cvbs_init_module);
	call_sub_init(lcd_init);
	call_sub_init(peripheral_lcd_init);
	call_sub_init(esm_init);
	call_sub_init(vpu_security_init);
	call_sub_init(osd_init_module);
	call_sub_init(dummy_venc_init);
	call_sub_init(vout_sys_serve_init);
	call_sub_init(vout_init_module);
	call_sub_init(gp_pll_init);
	call_sub_init(ion_init);
	call_sub_init(mua_init);
	call_sub_init(vfm_class_init);
	call_sub_init(ge2d_init_module);
	call_sub_init(vicp_init_module);
	call_sub_init(configs_init_devices);
	call_sub_init(lut_dma_init);
	call_sub_init(vdin_drv_init);
	call_sub_init(video_init);
	call_sub_init(aml_lcd_extern_i2c_dev_init);
	call_sub_init(aml_bl_extern_i2c_init);
	call_sub_init(aml_lcd_extern_init);
	call_sub_init(aml_bl_init);
	call_sub_init(aml_bl_extern_init);
	call_sub_init(ldim_dev_init);
	call_sub_init(vout2_init_module);
	call_sub_init(vout3_init_module);
	call_sub_init(ppmgr_init_module);
	call_sub_init(videosync_init);
	call_sub_init(picdec_init_module);
	call_sub_init(amdolby_vision_init);
	call_sub_init(tsync_module_init);
	call_sub_init(tsync_pcr_init);
	call_sub_init(video_composer_module_init);
	call_sub_init(meson_videotunnel_init);
	call_sub_init(vdetect_init);
	call_sub_init(videoqueue_init);
	call_sub_init(aml_vecm_init);
	call_sub_init(ionvideo_init);
	call_sub_init(v4lvideo_init);
	call_sub_init(amlvideo_init);
	call_sub_init(amlvideo2_init);
	call_sub_init(dil_init);
	call_sub_init(di_module_init);
	call_sub_init(dim_module_init);
	call_sub_init(di_v4l_init);
	call_sub_init(cec_init);
	call_sub_init(gdc_driver_init);
	call_sub_init(adc_init);
	call_sub_init(tvafe_drv_init);
	call_sub_init(vbi_init);
	call_sub_init(tvafe_avin_detect_init);
	call_sub_init(amvdec_656in_init_module);
	call_sub_init(hdmirx_init);
	call_sub_init(hld_init);
	call_sub_init(viuin_init_module);
	call_sub_init(resman_init);
	call_sub_init(aml_atvdemod_init);
	call_sub_init(aml_dtvdemod_init);
	call_sub_init(minfo_init); /* should called before msync */
	call_sub_init(msync_init);
	call_sub_init(frc_init);
	call_sub_init(amlogic_system_secure_dma_buf_init);
	call_sub_init(vpp_drv_init);
	call_sub_init(amprime_sl_init);
	call_sub_init(vpq_init);
	pr_info("### %s() end\n", __func__);
	return 0;
}
static void __exit media_main_exit(void)
{
	pr_info("%s()\n", __func__);
	meson_videotunnel_exit();
	vdetect_exit();
	videoqueue_exit();
	msync_exit();
}
module_init(media_main_init);
module_exit(media_main_exit);
MODULE_LICENSE("GPL v2");
