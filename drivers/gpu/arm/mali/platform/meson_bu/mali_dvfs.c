/*
 * Copyright (C) 2013 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file arm_core_scaling.c
 * Example core scaling policy.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/mali/mali_utgard.h>
#include <mali_kernel_common.h>
#include <mali_osk_profiling.h>
#include <linux/time.h>

#include <linux/amlogic/amports/gp_pll.h>
#include "meson_main2.h"


static int currentStep;
static int  scaling_mode = MALI_PP_FS_SCALING;
//static int  scaling_mode = MALI_SCALING_DISABLE;
//static int  scaling_mode = MALI_PP_SCALING;

//static struct gp_pll_user_handle_s *gp_pll_user_gpu;
//static int is_gp_pll_get;
//static int is_gp_pll_put;

static unsigned scaling_dbg_level = 0;
module_param(scaling_dbg_level, uint, 0644);
MODULE_PARM_DESC(scaling_dbg_level , "scaling debug level");

static mali_plat_info_t* pmali_plat = NULL;
static struct workqueue_struct *mali_scaling_wq = NULL;
//static DEFINE_SPINLOCK(lock);

static int cur_gpu_clk_index = 0;
static int exec_gpu_clk_index = 0;
#define scalingdbg(level, fmt, arg...)				   \
	do {											   \
		if (scaling_dbg_level >= (level))			   \
		printk(fmt , ## arg);						   \
	} while (0)

struct mali_gpu_clock meson_gpu_clk_info = {
    .item = NULL,
    .num_of_steps = 0,
};

u32 revise_set_clk(u32 val, u32 flush)
{
	u32 ret = 0;
	return ret;
}

void get_mali_rt_clkpp(u32* clk, u32* pp)
{
}

u32 set_mali_rt_clkpp(u32 clk, u32 pp, u32 flush)
{
	u32 ret = 0;
	return ret;
}

void revise_mali_rt(void)
{
}

static void do_scaling(struct work_struct *work)
{
    //unsigned long flags;
    mali_plat_info_t *pinfo = container_of(work, struct mali_plat_info_t, wq_work);

    *pinfo = *pinfo;
	//mali_dev_pause();
    //spin_lock_irqsave(&lock, flags);
    mali_clock_set(exec_gpu_clk_index);
    cur_gpu_clk_index = exec_gpu_clk_index;
    //spin_unlock_irqrestore(&lock, flags);
	//mali_dev_resume();
}
void flush_scaling_job(void)
{
    if (mali_scaling_wq == NULL) return;

	flush_workqueue(mali_scaling_wq);
    printk("%s, %d\n", __func__, __LINE__);
}


int mali_core_scaling_init(mali_plat_info_t *mali_plat)
{
	pmali_plat = mali_plat;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	mali_scaling_wq = alloc_workqueue("gpu_scaling_wq", WQ_HIGHPRI | WQ_UNBOUND, 0);
#else
	mali_scaling_wq = create_workqueue("gpu_scaling_wq");
#endif
	INIT_WORK(&pmali_plat->wq_work, do_scaling);
    if (mali_scaling_wq == NULL) printk("Unable to create gpu scaling workqueue\n");

    meson_gpu_clk_info.num_of_steps = pmali_plat->scale_info.maxclk;

	return 0;
}

void mali_core_scaling_term(void)
{
    flush_scaling_job();
    destroy_workqueue(mali_scaling_wq);
    mali_scaling_wq = NULL;
}

void mali_pp_scaling_update(struct mali_gpu_utilization_data *data)
{
}

void mali_pp_fs_scaling_update(struct mali_gpu_utilization_data *data)
{
}

u32 get_mali_schel_mode(void)
{
	return scaling_mode;
}

void set_mali_schel_mode(u32 mode)
{
    scaling_mode = mode;
	if (scaling_mode == MALI_TURBO_MODE) {
        printk ("turbo mode\n");
		pmali_plat->limit_on = 0;
        meson_gpu_clk_info.num_of_steps = pmali_plat->turbo_clock;
	} else {
        printk ("not turbo mode\n");
		pmali_plat->limit_on = 1;
        meson_gpu_clk_info.num_of_steps  = pmali_plat->scale_info.maxclk;
	}

    printk("total_enable_steps = %d\n", meson_gpu_clk_info.num_of_steps);
}

u32 get_current_frequency(void)
{
	return get_mali_freq(currentStep);
}

void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data)
{
}

void mali_dev_restore(void)
{
    //TO add this
	//mali_perf_set_num_pp_cores(num_cores_enabled);
	if (pmali_plat && pmali_plat->pdev) {
		mali_clock_init_clk_tree(pmali_plat->pdev);
	} else {
		printk("error: init clock failed, pmali_plat=%p, pmali_plat->pdev=%p\n",
				pmali_plat, pmali_plat == NULL ? NULL: pmali_plat->pdev);
	}
}

/* Function that platfrom report it's clock info which driver can set, needed when CONFIG_MALI_DVFS enabled */
static void meson_platform_get_clock_info(struct mali_gpu_clock **data) {
    if (pmali_plat) {
        meson_gpu_clk_info.item = pmali_plat->clk_items;
        meson_gpu_clk_info.num_of_steps = pmali_plat->scale_info.maxclk;
        printk("get clock info\n");
    } else {
        printk("error pmali_plat is null");
    }
    *data = &meson_gpu_clk_info;
}

/* Function that get the current clock info, needed when CONFIG_MALI_DVFS enabled */
static int meson_platform_get_freq(void) {
    scalingdbg(1, "cur_gpu_clk_index =%d\n", cur_gpu_clk_index);
    //dynamically changed the num of steps;
    return  cur_gpu_clk_index;
}

/* Fuction that platform callback for freq setting, needed when CONFIG_MALI_DVFS enabled */
static int meson_platform_set_freq(int setting_clock_step) {

    if (exec_gpu_clk_index == setting_clock_step) {
        return 0;
    }

    queue_work(mali_scaling_wq, &pmali_plat->wq_work);
    exec_gpu_clk_index = setting_clock_step;
    scalingdbg(1, "set cur_gpu_clk_index =%d\n", cur_gpu_clk_index);
    return 0;
}

int mali_meson_get_gpu_data(struct mali_gpu_device_data *mgpu_data)
{
    mgpu_data->get_clock_info = meson_platform_get_clock_info,
    mgpu_data->get_freq = meson_platform_get_freq,
    mgpu_data->set_freq = meson_platform_set_freq,
    mgpu_data->utilization_callback = NULL;
    return 0;
}
