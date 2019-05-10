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

#if AMLOGIC_GPU_USE_GPPLL
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 16)
#include <linux/amlogic/amports/gp_pll.h>
#else
#include <linux/amlogic/media/clk/gp_pll.h>
#endif
#endif

#define LOG_MALI_SCALING 1
#include "meson_main2.h"
#include "mali_clock.h"

static int currentStep;
#ifndef CONFIG_MALI_DVFS
static int num_cores_enabled;
static int lastStep;
static struct work_struct wq_work;
static mali_plat_info_t* pmali_plat = NULL;
#endif
static int  scaling_mode = MALI_SCALING_DISABLE;
extern int  mali_pm_statue;
//static int  scaling_mode = MALI_SCALING_DISABLE;
//static int  scaling_mode = MALI_PP_SCALING;

#if AMLOGIC_GPU_USE_GPPLL
static struct gp_pll_user_handle_s *gp_pll_user_gpu;
static int is_gp_pll_get;
static int is_gp_pll_put;
#endif
static unsigned scaling_dbg_level = 0;
module_param(scaling_dbg_level, uint, 0644);
MODULE_PARM_DESC(scaling_dbg_level , "scaling debug level");

#define scalingdbg(level, fmt, arg...)				   \
	do {											   \
		if (scaling_dbg_level >= (level))			   \
		printk(fmt , ## arg);						   \
	} while (0)

#ifndef CONFIG_MALI_DVFS
static inline void mali_clk_exected(void)
{
	mali_dvfs_threshold_table * pdvfs = pmali_plat->dvfs_table;
	uint32_t execStep = currentStep;
	mali_dvfs_threshold_table *dvfs_tbl = &pmali_plat->dvfs_table[currentStep];

	//if (pdvfs[currentStep].freq_index == pdvfs[lastStep].freq_index) return;
	if ((pdvfs[execStep].freq_index == pdvfs[lastStep].freq_index) ||
		(pdvfs[execStep].clk_freq == pdvfs[lastStep].clk_freq)){
		return;
	}

#if AMLOGIC_GPU_USE_GPPLL
	if (0 == strcmp(dvfs_tbl->clk_parent, "gp0_pll")) {
		gp_pll_request(gp_pll_user_gpu);
		if (!is_gp_pll_get) {
			//printk("not get pll\n");
			execStep = currentStep - 1;
		}
	} else {
		//not get the gp pll, do need put
		is_gp_pll_get = 0;
		is_gp_pll_put = 0;
		gp_pll_release(gp_pll_user_gpu);
	}
#else
	if ((0 == strcmp(dvfs_tbl->clk_parent, "gp0_pll")) &&
			!IS_ERR(dvfs_tbl->clkp_handle) &&
			(0 != dvfs_tbl->clkp_freq)) {
		clk_prepare_enable(dvfs_tbl->clkp_handle);
		clk_set_rate(dvfs_tbl->clkp_handle, dvfs_tbl->clkp_freq);
	}

#endif
	//mali_dev_pause();
	mali_clock_set(pdvfs[execStep].freq_index);
	//mali_dev_resume();
#if AMLOGIC_GPU_USE_GPPLL==0
	if ((0 == strcmp(pdvfs[lastStep].clk_parent,"gp0_pll")) &&
		(0 != strcmp(pdvfs[execStep].clk_parent, "gp0_pll"))) {
			clk_disable_unprepare(pdvfs[lastStep].clkp_handle);
	}
#endif

	lastStep = execStep;
#if AMLOGIC_GPU_USE_GPPLL
	if (is_gp_pll_put) {
		//printk("release gp0 pll\n");
		gp_pll_release(gp_pll_user_gpu);
		gp_pll_request(gp_pll_user_gpu);
		is_gp_pll_get = 0;
		is_gp_pll_put = 0;
	}
#endif

}
#if AMLOGIC_GPU_USE_GPPLL
static int gp_pll_user_cb_gpu(struct gp_pll_user_handle_s *user,
		int event)
{
	if (event == GP_PLL_USER_EVENT_GRANT) {
		//printk("granted\n");
		is_gp_pll_get = 1;
		is_gp_pll_put = 0;
		schedule_work(&wq_work);
	} else if (event == GP_PLL_USER_EVENT_YIELD) {
		//printk("ask for yield\n");
		is_gp_pll_get = 0;
		is_gp_pll_put = 1;
		schedule_work(&wq_work);
	}

	return 0;
}
#endif

int mali_perf_set_num_pp_cores(int cores)
{
    cores = cores;
    return 0;
}

static void do_scaling(struct work_struct *work)
{
	mali_dvfs_threshold_table * pdvfs = pmali_plat->dvfs_table;
	int err = mali_perf_set_num_pp_cores(num_cores_enabled);
    if (err < 0) scalingdbg(1, "set pp failed");

	scalingdbg(1, "set pp cores to %d\n", num_cores_enabled);
	scalingdbg(1, "pdvfs[%d].freq_index=%d, pdvfs[%d].freq_index=%d\n",
			currentStep, pdvfs[currentStep].freq_index,
			lastStep, pdvfs[lastStep].freq_index);
	mali_clk_exected();
#ifdef CONFIG_MALI400_PROFILING
	_mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
			MALI_PROFILING_EVENT_CHANNEL_GPU |
			MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE,
			get_current_frequency(),
			0,	0,	0,	0);
#endif
}
#endif

u32 revise_set_clk(u32 val, u32 flush)
{
	u32 ret = 0;
#ifndef CONFIG_MALI_DVFS
	mali_scale_info_t* pinfo;

	pinfo = &pmali_plat->scale_info;

	if (val < pinfo->minclk)
		val = pinfo->minclk;
	else if (val >  pinfo->maxclk)
		val =  pinfo->maxclk;

	if (val != currentStep) {
		currentStep = val;
		if (flush)
			schedule_work(&wq_work);
		else
			ret = 1;
	}
#endif
	return ret;
}

void get_mali_rt_clkpp(u32* clk, u32* pp)
{
#ifndef CONFIG_MALI_DVFS
	*clk = currentStep;
	*pp = num_cores_enabled;
#endif
}

u32 set_mali_rt_clkpp(u32 clk, u32 pp, u32 flush)
{
	u32 ret = 0;
#ifndef CONFIG_MALI_DVFS
	mali_scale_info_t* pinfo;
	u32 flush_work = 0;

	pinfo = &pmali_plat->scale_info;
	if (clk < pinfo->minclk)
		clk = pinfo->minclk;
	else if (clk >  pinfo->maxclk)
		clk =  pinfo->maxclk;

	if (clk != currentStep) {
		currentStep = clk;
		if (flush)
			flush_work++;
		else
			ret = 1;
	}
	if (pp < pinfo->minpp)
		pp = pinfo->minpp;
	else if (pp > pinfo->maxpp)
		pp = pinfo->maxpp;

	if (pp != num_cores_enabled) {
		num_cores_enabled = pp;
		if (flush)
			flush_work++;
		else
			ret = 1;
	}

	if (flush_work)
		schedule_work(&wq_work);
#endif
	return ret;
}

void revise_mali_rt(void)
{
#ifndef CONFIG_MALI_DVFS
	set_mali_rt_clkpp(currentStep, num_cores_enabled, 1);
#endif
}

void flush_scaling_job(void)
{
#ifndef CONFIG_MALI_DVFS
	cancel_work_sync(&wq_work);
#endif
}

#ifndef CONFIG_MALI_DVFS
static u32 enable_one_core(void)
{
	scalingdbg(2, "meson:	 one more pp, curent has %d pp cores\n",  num_cores_enabled + 1);
	return set_mali_rt_clkpp(currentStep, num_cores_enabled + 1, 0);
}

static u32 disable_one_core(void)
{
	scalingdbg(2, "meson: disable one pp, current has %d pp cores\n",  num_cores_enabled - 1);
	return set_mali_rt_clkpp(currentStep, num_cores_enabled - 1, 0);
}

static u32 enable_max_num_cores(void)
{
	return set_mali_rt_clkpp(currentStep, pmali_plat->scale_info.maxpp, 0);
}

static u32 enable_pp_cores(u32 val)
{
	scalingdbg(2, "meson: enable %d pp cores\n", val);
	return set_mali_rt_clkpp(currentStep, val, 0);
}
#endif

int mali_core_scaling_init(mali_plat_info_t *mali_plat)
{
#ifndef CONFIG_MALI_DVFS
	if (mali_plat == NULL) {
		scalingdbg(2, " Mali platform data is NULL!!!\n");
		return -1;
	}

	pmali_plat = mali_plat;
    printk("mali_plat=%p\n", mali_plat);
	num_cores_enabled = pmali_plat->sc_mpp;
#if AMLOGIC_GPU_USE_GPPLL
	gp_pll_user_gpu = gp_pll_user_register("gpu", 1,
		gp_pll_user_cb_gpu);
	//not get the gp pll, do need put
	is_gp_pll_get = 0;
	is_gp_pll_put = 0;
	if (gp_pll_user_gpu == NULL) printk("register gp pll user for gpu failed\n");
#endif

	currentStep = pmali_plat->def_clock;
	lastStep = currentStep;
	INIT_WORK(&wq_work, do_scaling);
#endif
	return 0;
	/* NOTE: Mali is not fully initialized at this point. */
}

void mali_core_scaling_term(void)
{
#ifndef CONFIG_MALI_DVFS
	flush_scheduled_work();
#if AMLOGIC_GPU_USE_GPPLL
	gp_pll_user_unregister(gp_pll_user_gpu);
#endif
#endif
}

#ifndef CONFIG_MALI_DVFS
static u32 mali_threshold [] = {
	40, /* 40% */
	50, /* 50% */
	90, /* 90% */
};
#endif

void mali_pp_scaling_update(int utilization_pp)
{
#ifndef CONFIG_MALI_DVFS
	int ret = 0;

	if (mali_threshold[2] < utilization_pp)
		ret = enable_max_num_cores();
	else if (mali_threshold[1]< utilization_pp)
		ret = enable_one_core();
	else if (0 < utilization_pp)
		ret = disable_one_core();
	if (ret == 1)
		schedule_work(&wq_work);
#endif
}

#if LOG_MALI_SCALING
void trace_utilization(int utilization_gpu, u32 current_idx, u32 next,
		u32 current_pp, u32 next_pp)
{
	char direction;
	if (next > current_idx)
		direction = '>';
	else if ((current_idx > pmali_plat->scale_info.minpp) && (next < current_idx))
		direction = '<';
	else
		direction = '~';

	scalingdbg(2, "[SCALING]%c (%3d-->%3d)@%3d{%3d - %3d}. pp:(%d-->%d)\n",
			direction,
			get_mali_freq(current_idx),
			get_mali_freq(next),
			utilization_gpu,
			pmali_plat->dvfs_table[current_idx].downthreshold,
			pmali_plat->dvfs_table[current_idx].upthreshold,
			current_pp, next_pp);
}
#endif

#ifndef CONFIG_MALI_DVFS
static int mali_stay_count = 0;
static void mali_decide_next_status(int utilization_pp, int* next_fs_idx,
		int* pp_change_flag)
{
	u32 mali_up_limit, decided_fs_idx;
	u32 ld_left, ld_right;
	u32 ld_up, ld_down;
	u32 change_mode;

	*pp_change_flag = 0;
	change_mode = 0;

	scalingdbg(5, "line(%d), scaling_mode=%d, MALI_TURBO_MODE=%d, turbo=%d, maxclk=%d\n",
			__LINE__,  scaling_mode, MALI_TURBO_MODE,
		    pmali_plat->turbo_clock, pmali_plat->scale_info.maxclk);

	mali_up_limit = (scaling_mode ==  MALI_TURBO_MODE) ?
		pmali_plat->turbo_clock : pmali_plat->scale_info.maxclk;
	decided_fs_idx = currentStep;

	ld_up = pmali_plat->dvfs_table[currentStep].upthreshold;
	ld_down = pmali_plat->dvfs_table[currentStep].downthreshold;

	scalingdbg(2, "utilization=%d,  ld_up=%d\n ", utilization_pp,  ld_up);
	if (utilization_pp >= ld_up) { /* go up */

		scalingdbg(2, "currentStep=%d,  mali_up_limit=%d\n ", currentStep, mali_up_limit);
		if (currentStep < mali_up_limit) {
			change_mode = 1;
			if ((currentStep < pmali_plat->def_clock) && (utilization_pp > pmali_plat->bst_gpu))
				decided_fs_idx = pmali_plat->def_clock;
			else
				decided_fs_idx++;
		}
		if ((utilization_pp >= ld_up) &&
				(num_cores_enabled < pmali_plat->scale_info.maxpp)) {
			if ((num_cores_enabled < pmali_plat->sc_mpp) && (utilization_pp >= pmali_plat->bst_pp)) {
				*pp_change_flag = 1;
				change_mode = 1;
			} else if (change_mode == 0) {
				*pp_change_flag = 2;
				change_mode = 1;
			}
		}
#if LOG_MALI_SCALING
		scalingdbg(2, "[nexting..] [LD:%d]-> FS[CRNT:%d LMT:%d NEXT:%d] PP[NUM:%d LMT:%d MD:%d][F:%d]\n",
				utilization_pp, currentStep, mali_up_limit, decided_fs_idx,
				num_cores_enabled, pmali_plat->scale_info.maxpp, *pp_change_flag, change_mode);
#endif
	} else if (utilization_pp <= ld_down) { /* go down */
		if (mali_stay_count > 0) {
			*next_fs_idx = decided_fs_idx;
			mali_stay_count--;
			return;
		}

		if (num_cores_enabled > pmali_plat->sc_mpp) {
			change_mode = 1;
			if (utilization_pp <= ld_down) {
				ld_left = utilization_pp * num_cores_enabled;
				ld_right = (pmali_plat->dvfs_table[currentStep].upthreshold) *
					(num_cores_enabled - 1);
				if (ld_left < ld_right) {
					change_mode = 2;
				}
			}
		} else if (currentStep > pmali_plat->scale_info.minclk) {
			change_mode = 1;
		} else if (num_cores_enabled > 1) { /* decrease PPS */
			if (utilization_pp <= ld_down) {
				ld_left = utilization_pp * num_cores_enabled;
				ld_right = (pmali_plat->dvfs_table[currentStep].upthreshold) *
					(num_cores_enabled - 1);
				scalingdbg(2, "ld_left=%d, ld_right=%d\n", ld_left, ld_right);
				if (ld_left < ld_right) {
					change_mode = 2;
				}
			}
		}

		if (change_mode == 1) {
			decided_fs_idx--;
		} else if (change_mode == 2) { /* decrease PPS */
			*pp_change_flag = -1;
		}
	}

	if (decided_fs_idx < 0 ) {
		printk("gpu debug, next index below 0\n");
		decided_fs_idx = 0;
	}
	if (decided_fs_idx > pmali_plat->scale_info.maxclk) {
		decided_fs_idx = pmali_plat->scale_info.maxclk;
		printk("gpu debug, next index above max-1, set to %d\n", decided_fs_idx);
	}

	if (change_mode)
		mali_stay_count = pmali_plat->dvfs_table[decided_fs_idx].keep_count;

	*next_fs_idx = decided_fs_idx;
}
#endif

void mali_pp_fs_scaling_update(int utilization_pp)
{
#ifndef CONFIG_MALI_DVFS
	int ret = 0;
	int pp_change_flag = 0;
	u32 next_idx = 0;

#if LOG_MALI_SCALING
	u32 last_pp = num_cores_enabled;
#endif
	mali_decide_next_status(utilization_pp, &next_idx, &pp_change_flag);

	if (pp_change_flag == 1)
		ret = enable_pp_cores(pmali_plat->sc_mpp);
	else if (pp_change_flag == 2)
		ret = enable_one_core();
	else if (pp_change_flag == -1) {
		ret = disable_one_core();
	}

#if LOG_MALI_SCALING
	if (pp_change_flag || (next_idx != currentStep))
		trace_utilization(utilization_pp, currentStep, next_idx, last_pp, num_cores_enabled);
#endif

	if (next_idx != currentStep) {
		ret = 1;
		currentStep = next_idx;
	}

	if (ret == 1)
		schedule_work(&wq_work);
#ifdef CONFIG_MALI400_PROFILING
	else
		_mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
				MALI_PROFILING_EVENT_CHANNEL_GPU |
				MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE,
				get_current_frequency(),
				0,	0,	0,	0);
#endif
#endif
}

u32 get_mali_schel_mode(void)
{
	return scaling_mode;
}

void set_mali_schel_mode(u32 mode)
{
#ifndef CONFIG_MALI_DVFS
	if (mode >= MALI_SCALING_MODE_MAX)
		return;
	scaling_mode = mode;

	//disable thermal in turbo mode
	if (scaling_mode == MALI_TURBO_MODE) {
		pmali_plat->limit_on = 0;
	} else {
		pmali_plat->limit_on = 1;
	}
	/* set default performance range. */
	pmali_plat->scale_info.minclk = pmali_plat->cfg_min_clock;
	pmali_plat->scale_info.maxclk = pmali_plat->cfg_clock;
	pmali_plat->scale_info.minpp = pmali_plat->cfg_min_pp;
	pmali_plat->scale_info.maxpp = pmali_plat->cfg_pp;

	/* set current status and tune max freq */
	if (scaling_mode == MALI_PP_FS_SCALING) {
		pmali_plat->scale_info.maxclk = pmali_plat->cfg_clock;
		enable_pp_cores(pmali_plat->sc_mpp);
	} else if (scaling_mode == MALI_SCALING_DISABLE) {
		pmali_plat->scale_info.maxclk = pmali_plat->cfg_clock;
		enable_max_num_cores();
	} else if (scaling_mode == MALI_TURBO_MODE) {
		pmali_plat->scale_info.maxclk = pmali_plat->turbo_clock;
		enable_max_num_cores();
	}
	currentStep = pmali_plat->scale_info.maxclk;
	schedule_work(&wq_work);
#endif
}

u32 get_current_frequency(void)
{
	return get_mali_freq(currentStep);
}

void mali_gpu_utilization_callback(int utilization_pp)
{
#ifndef CONFIG_MALI_DVFS
	if (mali_pm_statue)
		return;

	switch (scaling_mode) {
		case MALI_PP_FS_SCALING:
			mali_pp_fs_scaling_update(utilization_pp);
			break;
		case MALI_PP_SCALING:
			mali_pp_scaling_update(utilization_pp);
			break;
		default:
			break;
	}
#endif
}
static u32 clk_cntl_save = 0;
void mali_dev_freeze(void)
{
	clk_cntl_save = mplt_read(HHI_MALI_CLK_CNTL);
}

void mali_dev_restore(void)
{

	mplt_write(HHI_MALI_CLK_CNTL, clk_cntl_save);
	if (pmali_plat && pmali_plat->pdev) {
		mali_clock_init_clk_tree(pmali_plat->pdev);
	} else {
		printk("error: init clock failed, pmali_plat=%p, pmali_plat->pdev=%p\n",
				pmali_plat, pmali_plat == NULL ? NULL: pmali_plat->pdev);
	}
}
