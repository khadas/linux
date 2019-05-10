/*
 * platform.c
 *
 * clock source setting and resource config
 *
 *  Created on: Dec 4, 2013
 *      Author: amlogic
 */

#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#include <linux/of.h>
#include <linux/module.h>            /* kernel module definitions */
#include <linux/ioport.h>            /* request_mem_region */
#include <linux/slab.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 29))
#include <mach/register.h>
#include <mach/irqs.h>
#include <mach/io.h>
#endif
#include <asm/io.h>
#ifdef CONFIG_AMLOGIC_GPU_THERMAL
#include <linux/amlogic/gpu_cooling.h>
#include <linux/amlogic/gpucore_cooling.h>
//#include <linux/amlogic/aml_thermal_hw.h>
#include <linux/amlogic/meson_cooldev.h>
#endif

#include "mali_scaling.h"
#include "mali_clock.h"
#include "meson_main2.h"

/*
 *    For Meson 8 M2.
 *
 */
static void mali_plat_preheat(void);
static mali_plat_info_t mali_plat_data = {
    .bst_gpu = 210, /* threshold for boosting gpu. */
    .bst_pp = 160, /* threshold for boosting PP. */
    .have_switch = 1,
    .limit_on = 1,
    .plat_preheat = mali_plat_preheat,
};

static void mali_plat_preheat(void)
{
#ifndef CONFIG_MALI_DVFS
    u32 pre_fs;
    u32 clk, pp;

    if (get_mali_schel_mode() != MALI_PP_FS_SCALING)
        return;

    get_mali_rt_clkpp(&clk, &pp);
    pre_fs = mali_plat_data.def_clock + 1;
    if (clk < pre_fs)
        clk = pre_fs;
    if (pp < mali_plat_data.sc_mpp)
        pp = mali_plat_data.sc_mpp;
    set_mali_rt_clkpp(clk, pp, 1);
#endif
}

mali_plat_info_t* get_mali_plat_data(void) {
    return &mali_plat_data;
}

int get_mali_freq_level(int freq)
{
    int i = 0, level = -1;
    int mali_freq_num;

    if (freq < 0)
        return level;

    mali_freq_num = mali_plat_data.dvfs_table_size - 1;
    if (freq <= mali_plat_data.clk_sample[0])
        level = mali_freq_num-1;
    else if (freq >= mali_plat_data.clk_sample[mali_freq_num - 1])
        level = 0;
    else {
        for (i=0; i<mali_freq_num - 1 ;i++) {
            if (freq >= mali_plat_data.clk_sample[i] && freq <= mali_plat_data.clk_sample[i + 1]) {
                level = i;
                level = mali_freq_num-level - 1;
            }
        }
    }
    return level;
}

unsigned int get_mali_max_level(void)
{
    return mali_plat_data.dvfs_table_size - 1;
}

int get_gpu_max_clk_level(void)
{
    return mali_plat_data.cfg_clock;
}

#ifdef CONFIG_AMLOGIC_GPU_THERMAL
static void set_limit_mali_freq(u32 idx)
{
    if (mali_plat_data.limit_on == 0)
        return;
    if (idx > mali_plat_data.turbo_clock || idx < mali_plat_data.scale_info.minclk)
        return;
    if (idx > mali_plat_data.maxclk_sysfs) {
        printk("idx > max freq\n");
        return;
    }
    mali_plat_data.scale_info.maxclk= idx;
    revise_mali_rt();
}

static u32 get_limit_mali_freq(void)
{
    return mali_plat_data.scale_info.maxclk;
}

#ifdef CONFIG_DEVFREQ_THERMAL
static u32 get_mali_utilization(void)
{
#ifndef MESON_DRV_BRING
    return 55;
#else
    return (_mali_ukk_utilization_pp() * 100) / 256;
#endif
}
#endif
#endif

#ifdef CONFIG_AMLOGIC_GPU_THERMAL
static u32 set_limit_pp_num(u32 num)
{
    u32 ret = -1;
    if (mali_plat_data.limit_on == 0)
        goto quit;
    if (num > mali_plat_data.cfg_pp ||
            num < mali_plat_data.scale_info.minpp)
        goto quit;

    if (num > mali_plat_data.maxpp_sysfs) {
        printk("pp > sysfs set pp\n");
        goto quit;
    }

    mali_plat_data.scale_info.maxpp = num;
    revise_mali_rt();
    ret = 0;
quit:
    return ret;
}
#ifdef CONFIG_DEVFREQ_THERMAL
static u32 mali_get_online_pp(void)
{
    unsigned int val;
    mali_plat_info_t* pmali_plat = get_mali_plat_data();

    val = readl(pmali_plat->reg_base_aobus + 0xf0) & 0xff;
    if (val == 0x07)    /* No pp is working */
        return 0;

#ifndef MESON_DRV_BRING
    return 2;
#else
    return mali_executor_get_num_cores_enabled();
#endif
}
#endif
#endif

int mali_meson_init_start(struct platform_device* ptr_plt_dev)
{
    //dev_set_drvdata(&ptr_plt_dev->dev, &mali_plat_data);
    mali_dt_info(ptr_plt_dev, &mali_plat_data);
    mali_clock_init_clk_tree(ptr_plt_dev);
    return 0;
}

int mali_meson_init_finish(struct platform_device* ptr_plt_dev)
{
    if (mali_core_scaling_init(&mali_plat_data) < 0)
        return -1;
    return 0;
}

int mali_meson_uninit(struct platform_device* ptr_plt_dev)
{
    mali_core_scaling_term();
    return 0;
}

void mali_post_init(void)
{
#ifdef CONFIG_AMLOGIC_GPU_THERMAL
    int err;
    struct gpufreq_cooling_device *gcdev = NULL;
    struct gpucore_cooling_device *gccdev = NULL;

    gcdev = gpufreq_cooling_alloc();
    register_gpu_freq_info(get_current_frequency);
    if (IS_ERR(gcdev))
        printk("malloc gpu cooling buffer error!!\n");
    else if (!gcdev)
        printk("system does not enable thermal driver\n");
    else {
        gcdev->get_gpu_freq_level = get_mali_freq_level;
        gcdev->get_gpu_max_level = get_mali_max_level;
        gcdev->set_gpu_freq_idx = set_limit_mali_freq;
        gcdev->get_gpu_current_max_level = get_limit_mali_freq;
#ifdef CONFIG_DEVFREQ_THERMAL
        gcdev->get_gpu_freq = get_mali_freq;
        gcdev->get_gpu_loading = get_mali_utilization;
        gcdev->get_online_pp = mali_get_online_pp;
#endif
        err = gpufreq_cooling_register(gcdev);
#ifdef CONFIG_DEVFREQ_THERMAL
        meson_gcooldev_min_update(gcdev->cool_dev);
#endif
        if (err < 0)
            printk("register GPU  cooling error\n");
        printk("gpu cooling register okay with err=%d\n",err);
    }

    gccdev = gpucore_cooling_alloc();
    if (IS_ERR(gccdev))
        printk("malloc gpu core cooling buffer error!!\n");
    else if (!gccdev)
        printk("system does not enable thermal driver\n");
    else {
        gccdev->max_gpu_core_num=mali_plat_data.cfg_pp;
        gccdev->set_max_pp_num=set_limit_pp_num;
        err = (int)gpucore_cooling_register(gccdev);
#ifdef CONFIG_DEVFREQ_THERMAL
        meson_gcooldev_min_update(gccdev->cool_dev);
#endif
        if (err < 0)
            printk("register GPU  cooling error\n");
        printk("gpu core cooling register okay with err=%d\n",err);
    }
#endif
}
