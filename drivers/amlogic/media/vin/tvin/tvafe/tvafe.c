// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/pinctrl/consumer.h>
/*#include <asm/uaccess.h>*/
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/amlogic/iomap.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/compat.h>
#include <linux/clk.h>
/* Amlogic headers */
#include <linux/amlogic/media/canvas/canvas.h>
/*#include <mach/am_regs.h>*/
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/aml_atvdemod.h>

/* Local include */
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "../tvin_frontend.h"
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "tvafe_regs.h"
#include "tvafe_cvd.h"
#include "tvafe_general.h"
#include "tvafe_vbi.h"
#include "tvafe.h"
#include "tvafe_debug.h"
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
#include "tvafe_avin_detect.h"
#endif
#include "../vdin/vdin_sm.h"

#define TVAFE_NAME               "tvafe"
#define TVAFE_DRIVER_NAME        "tvafe"
#define TVAFE_MODULE_NAME        "tvafe"
#define TVAFE_DEVICE_NAME        "tvafe"
#define TVAFE_CLASS_NAME         "tvafe"

static dev_t tvafe_devno;
static struct class *tvafe_clsp;

#define TVAFE_TIMER_INTERVAL    (HZ / 100)   /* 10ms, #define HZ 100 */
#define TVAFE_RATIO_CNT			40

static struct am_regs_s tvafe_regs;
static struct tvafe_pin_mux_s tvafe_pinmux;
static struct meson_tvafe_data *s_tvafe_data;
static struct tvafe_clkgate_type tvafe_clkgate;
static struct tvafe_dev_s *tvafe_dev_local;

static bool enable_db_reg = true;
module_param(enable_db_reg, bool, 0644);
MODULE_PARM_DESC(enable_db_reg, "enable/disable tvafe load reg");

int top_init_en;

/*0: atv playmode*/
/*1: atv search mode*/
static bool tvafe_mode;

/*tvconfig snow config*/
static bool snow_cfg;
/*1: snow function on;*/
/*0: off snow function*/
bool tvafe_snow_function_flag;

/*1: tvafe clk enabled;*/
/*0: tvafe clk disabled*/
/*read write cvd acd reg will crash when clk disabled*/
bool tvafe_clk_status;

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
/*opened port,1:av1, 2:av2, 0:none av*/
unsigned int avport_opened;
/*0:in, 1:out*/
unsigned int av1_plugin_state;
unsigned int av2_plugin_state;
#endif

/*tvafe_dbg_print:
 *bit[0]:normal flow debug print
 *bit[1]:
 *bit[2]:vsync isr debug print
 *bit[3]:smr debug print
 *bit[4]:nonstd debug print
 */
unsigned int tvafe_dbg_print;
static enum tvin_sig_fmt_e tvafe_manual_fmt_save;
bool tvafe_atv_search_channel;

#ifdef CONFIG_AMLOGIC_ATV_DEMOD
static struct tvafe_info_s *g_tvafe_info;
#endif

static unsigned int vs_adj_val_pre;

//project NTSC-M pass
//tvin_afe: cutwindow_h: 0 8 16 10 8
//cutwindow_v: 4 8 14 16 24
//horizontal_dir0: 136 146 140 150 150
//horizontal_dir1: 146 154 150 152 154
//horizontal_stp0: 32 46 46 50 32
//horizontal_stp1: 18 46 46 50 32

//project PAL-I pass
//tvin_afe: cutwindow_h: 0 0 0 0 8
//cutwindow_v: 4 8 14 16 24
//horizontal_dir0: 136 154 148 150 136
//horizontal_dir1: 136 154 150 152 154
//horizontal_stp0: 32 46 46 50 32
//horizontal_stp1: 32 46 46 50 32

static struct tvafe_user_param_s tvafe_user_param = {
	.cutwindow_val_h = {0, 10, 18, 20, 62},
	/*level4: 48-->62 for ntsc-m*/
	.cutwindow_val_v = {4, 8, 14, 16, 24},
	.horizontal_dir0 = {148, 148, 148, 148, 148},
	.horizontal_dir1 = {148, 148, 148, 148, 148},
	.horizontal_stp0 = {0, 0, 0, 0, 0},
	.horizontal_stp1 = {0, 0, 0, 0, 0},
	.cutwindow_val_vs_ve = TVAFE_VS_VE_VAL,
	.cdto_adj_hcnt_th = 0x260,
	.cdto_adj_ratio_p = 1019, /* val/1000 */
	.cdto_adj_offset_p = 0xfcc395,
	.cdto_adj_ratio_n = 1036, /* val/1000 */
	.cdto_adj_offset_n = 0x1b6b7d0,
	/* auto_adj_en:
	 * bit[7]: auto_vs_mode(0=dual side, 1=one side)
	 * bit[6]: auto_hs_test_mode(0=hs side, 1=he side)
	 * bit[5]: auto pga
	 * bit[4]: auto 3d comb
	 * bit[3]: auto de
	 * bit[2]: auto vs
	 * bit[1]: auto hs
	 * bit[0]: auto cdto
	 */
	.auto_adj_en = 0x3e,
	.vline_chk_cnt = 100, /* 100*10ms */
	.hline_chk_cnt = 300, /* 300*10ms */
	.low_amp_level = 0,

	.nostd_vs_th = 0x0,
	.nostd_no_vs_th = 0xf0,
	.nostd_vs_cntl = 0x1,
	.nostd_vloop_tc = 0x2,
	.force_vs_th_flag = 0,
	.nostd_stable_cnt = 3,
	.nostd_dmd_clp_step = 0x10,

	/*4 is the test result@20171101 on fluke-54200 and DVD*/
	.skip_vf_num = 4,
	.unlock_cnt_max = 3,

	.avout_en = 1,

	/* cutwin_test_en:
	 * bit[3]: test_vcut
	 * bit[2]: test_hcut
	 * bit[1]: test vcnt
	 * bit[0]: test hcnt
	 */
	.cutwin_test_en = 0,
	.cutwin_test_hcnt = 0,
	.cutwin_test_vcnt = 0,
	.cutwin_test_hcut = 0,
	.cutwin_test_vcut = 0,
	.nostd_bypass_iir = 0,
};

struct tvafe_user_param_s *tvafe_get_user_param(void)
{
	return &tvafe_user_param;
}

struct tvafe_dev_s *tvafe_get_dev(void)
{
	return tvafe_dev_local;
}

static int tvafe_config_init(struct tvafe_dev_s *devp,
				  enum tvin_port_e port)
{
	/* init variable */
	memset(&devp->tvafe, 0, sizeof(struct tvafe_info_s));

	if (IS_TVAFE_AVIN_SRC(port)) {
		if (tvafe_cpu_type() >= TVAFE_CPU_TYPE_TM2_B)
			devp->acd_table = cvbs_acd_table_tm2_b;
		else
			devp->acd_table = cvbs_acd_table;

		devp->pq_conf = s_tvafe_data->cvbs_pq_conf;
		if (tvafe_dbg_print & TVAFE_DBG_NORMAL)
			tvafe_pr_info("%s: select cvbs config\n", __func__);
	} else {
		if (tvafe_cpu_type() >= TVAFE_CPU_TYPE_TM2_B)
			devp->acd_table = rf_acd_table_tm2_b;
		else
			devp->acd_table = rf_acd_table;

		devp->pq_conf = s_tvafe_data->rf_pq_conf;
		if (tvafe_dbg_print & TVAFE_DBG_NORMAL)
			tvafe_pr_info("%s: select rf config\n", __func__);
	}

	return 0;
}

/*
 * tvafe check support port
 */
static int tvafe_dec_support(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct tvafe_dev_s *devp = container_of(fe,
				struct tvafe_dev_s, frontend);

	/* check afe port and index */
	if (!IS_TVAFE_SRC(port) || fe->index != devp->index)
		return -1;

	return 0;
}

#ifdef CONFIG_CMA
static int tvafe_cma_alloc(struct tvafe_dev_s *devp)
{
	unsigned int mem_size = devp->cma_mem_size;
	int flags = CODEC_MM_FLAGS_CMA_FIRST | CODEC_MM_FLAGS_CMA_CLEAR |
		CODEC_MM_FLAGS_CPU;
	if (devp->cma_config_en == 0)
		return 0;
	if (devp->cma_mem_alloc == 1)
		return 0;
	devp->cma_mem_alloc = 1;
	if (devp->cma_config_flag == 1) {
		devp->mem.start = codec_mm_alloc_for_dma("tvafe",
			mem_size / PAGE_SIZE, 0, flags);
		devp->mem.size = mem_size;
		if (devp->mem.start == 0) {
			tvafe_pr_err("tvafe codec alloc fail!!!\n");
			return 1;
		} else {
			tvafe_pr_info("mem_start = 0x%x, mem_size = 0x%x\n",
				devp->mem.start, devp->mem.size);
			tvafe_pr_info("codec cma alloc ok!\n");
		}
	} else if (devp->cma_config_flag == 0) {
		devp->venc_pages =
			dma_alloc_from_contiguous(&devp->this_pdev->dev,
			mem_size >> PAGE_SHIFT, 0, 0);
		if (devp->venc_pages) {
			devp->mem.start = page_to_phys(devp->venc_pages);
			devp->mem.size  = mem_size;
			tvafe_pr_info("mem_start = 0x%x, mem_size = 0x%x\n",
				devp->mem.start, devp->mem.size);
			tvafe_pr_info("cma alloc ok!\n");
		} else {
			tvafe_pr_err("tvafe cma mem undefined2.\n");
		}
	}

	return 0;
}

static void tvafe_cma_release(struct tvafe_dev_s *devp)
{
	if (devp->cma_config_en == 0)
		return;
	if (devp->cma_config_flag == 1 && devp->mem.start &&
	    devp->cma_mem_alloc == 1) {
		codec_mm_free_for_dma("tvafe", devp->mem.start);
		devp->mem.start = 0;
		devp->mem.size = 0;
		devp->cma_mem_alloc = 0;
		tvafe_pr_info("codec cma release ok!\n");
	} else if (devp->venc_pages && devp->cma_mem_size &&
		   (devp->cma_mem_alloc == 1) &&
		   (devp->cma_config_flag == 0)) {
		devp->cma_mem_alloc = 0;
		dma_release_from_contiguous(&devp->this_pdev->dev,
			devp->venc_pages,
			devp->cma_mem_size >> PAGE_SHIFT);
		tvafe_pr_info("cma release ok!\n");
	}
}
#endif

#ifdef CONFIG_AMLOGIC_ATV_DEMOD
static int tvafe_work_mode(bool mode)
{
	tvafe_pr_info("%s: %d\n", __func__, mode);
	tvafe_mode = mode;
	if (mode) {
		reinit_scan = true;
		if (tvafe_atv_search_channel) {
			tvafe_manual_fmt_save = TVIN_SIG_FMT_NULL;
			manual_flag = 0;
		}
	} else {
		if (tvafe_atv_search_channel) {
			if (g_tvafe_info) {
				g_tvafe_info->cvd2.manual_fmt =
					tvafe_manual_fmt_save;
		tvafe_pr_info("%s: set cvd2 manual fmt:%s.\n",
			      __func__,
			      tvin_sig_fmt_str(tvafe_manual_fmt_save));
			if (tvafe_manual_fmt_save != TVIN_SIG_FMT_NULL)
				manual_flag = 1;
			}
		}
	}

	return 0;
}

static int tvafe_get_v_fmt(void)
{
	int fmt = 0;
	struct tvafe_dev_s *devp = NULL;

	devp = tvafe_get_dev();
	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED) ||
		(devp->flags & TVAFE_POWERDOWN_IN_IDLE)) {
		if (tvafe_dbg_print & TVAFE_DBG_NORMAL)
			tvafe_pr_err("tvafe haven't opened OR suspend:flags:0x%x!!!\n",
					devp->flags);
		return 0;
	}

	if (tvin_get_sm_status(0) != TVIN_SM_STATUS_STABLE)
		return 0;

	if (g_tvafe_info)
		fmt = tvafe_cvd2_get_format(&g_tvafe_info->cvd2);
	tvafe_manual_fmt_save = fmt;
	return fmt;
}
#endif

/*
 * tvafe bringup detect signal code
 */
int tvafe_bringup_detect_signal(struct tvafe_dev_s *devp, enum tvin_port_e port)
{
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvafe_adc_ch_e adc_ch = TVAFE_ADC_CH_NULL;

	adc_ch = tvafe_port_to_channel(port, devp->pinmux);

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
	/*only txlx chip enabled*/
	if (tvafe_cpu_type() >= TVAFE_CPU_TYPE_TL1) {
		/*synctip set to 0 when tvafe working&&av connected*/
		/*enable clamp if av connected*/
		if (adc_ch == TVAFE_ADC_CH_0) {
			/*channel-1*/
			tvafe_cha1_SYNCTIP_close_config();
		} else if (adc_ch == TVAFE_ADC_CH_1) {
			/*channel-2*/
			tvafe_cha2_SYNCTIP_close_config();
		}
	}
#endif
	tvafe_config_init(devp, port);
	/**enable and reset tvafe clock**/
	tvafe_enable_module(true);

	/* set APB bus register accessing error exception */
	tvafe_set_apb_bus_err_ctrl();
	/**set cvd2 reset to high**/
	/*tvafe_cvd2_hold_rst();?????*/

	/* init tvafe registers */
	tvafe_init_reg(&tvafe->cvd2, &devp->mem, port);

	/* must reload mux if you change adc reg table!!! */
	tvafe_adc_pin_mux(adc_ch);

	W_APB_BIT(TVFE_CLAMP_INTF, 1, CLAMP_EN_BIT, CLAMP_EN_WID);
	devp->flags |= TVAFE_FLAG_DEV_OPENED;

	tvafe_pr_info("%s open port:0x%x ok.\n", __func__, port);

	return 0;
}

/*
 * tvafe open port and init register
 */
static int tvafe_dec_open(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvafe_adc_ch_e adc_ch = TVAFE_ADC_CH_NULL;

	mutex_lock(&devp->afe_mutex);
	if (devp->flags & TVAFE_FLAG_DEV_OPENED) {
		tvafe_pr_err("%s(%d), %s opened already\n", __func__,
				devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return 1;
	}
	if (!IS_TVAFE_SRC(port)) {
		tvafe_pr_err("%s(%d), %s unsupport\n",
			     __func__, devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return 1;
	}

	adc_ch = tvafe_port_to_channel(port, devp->pinmux);

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
	/*only txlx chip enabled*/
	if (tvafe_cpu_type() >= TVAFE_CPU_TYPE_TL1) {
		/*synctip set to 0 when tvafe working&&av connected*/
		/*enable clamp if av connected*/
		if (adc_ch == TVAFE_ADC_CH_0) {
			/*channel-1*/
			tvafe_cha1_SYNCTIP_close_config();
		} else if (adc_ch == TVAFE_ADC_CH_1) {
			/*channel-2*/
			tvafe_cha2_SYNCTIP_close_config();
		}
	}
#endif

#ifdef CONFIG_CMA
	if (tvafe_cma_alloc(devp)) {
		mutex_unlock(&devp->afe_mutex);
		return 1;
	}
#endif
	tvafe_config_init(devp, port);
	/**enable and reset tvafe clock**/
	tvafe_enable_module(true);
	devp->flags &= (~TVAFE_POWERDOWN_IN_IDLE);

	/* set APB bus register accessing error exception */
	tvafe_set_apb_bus_err_ctrl();
	/**set cvd2 reset to high**/
	/*tvafe_cvd2_hold_rst();?????*/

	/* init tvafe registers */
	tvafe_init_reg(&tvafe->cvd2, &devp->mem, port);

	/* must reload mux if you change adc reg table!!! */
	tvafe_adc_pin_mux(adc_ch);

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
	/*only txlx chip enabled*/
	if (tvafe_cpu_type() >= TVAFE_CPU_TYPE_TL1) {
		/*synctip set to 0 when tvafe working&&av connected*/
		/*enable clamp if av connected*/
		if (adc_ch == TVAFE_ADC_CH_0) {
			avport_opened = TVAFE_PORT_AV1;
			W_APB_BIT(TVFE_CLAMP_INTF, 1,
				  CLAMP_EN_BIT, CLAMP_EN_WID);
		} else if (adc_ch == TVAFE_ADC_CH_1) {
			avport_opened = TVAFE_PORT_AV2;
			W_APB_BIT(TVFE_CLAMP_INTF, 1,
				  CLAMP_EN_BIT, CLAMP_EN_WID);
		} else {
			avport_opened = 0;
			W_APB_BIT(TVFE_CLAMP_INTF, 1,
					CLAMP_EN_BIT, CLAMP_EN_WID);
		}
	} else {
		W_APB_BIT(TVFE_CLAMP_INTF, 1, CLAMP_EN_BIT, CLAMP_EN_WID);
	}
#else
	W_APB_BIT(TVFE_CLAMP_INTF, 1, CLAMP_EN_BIT, CLAMP_EN_WID);
#endif
	tvafe->parm.port = port;
	vs_adj_val_pre = 0;

	/* set the flag to enable ioctl access */
	devp->flags |= TVAFE_FLAG_DEV_OPENED;
	tvafe_clk_status = true;
#ifdef CONFIG_AMLOGIC_ATV_DEMOD
	g_tvafe_info = tvafe;
	/* register aml_fe hook for atv search */
	aml_fe_hook_cvd(tvafe_cvd2_get_atv_format, tvafe_cvd2_get_hv_lock,
		tvafe_get_v_fmt, tvafe_work_mode);
#endif
	tvafe_pr_info("%s open port:0x%x ok.\n", __func__, port);

	mutex_unlock(&devp->afe_mutex);

	return 0;
}

/*
 * tvafe start after signal stable
 */
static void tvafe_dec_start(struct tvin_frontend_s *fe, enum tvin_sig_fmt_e fmt)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = devp->tvafe.parm.port;
	enum tvafe_adc_ch_e adc_ch = TVAFE_ADC_CH_NULL;

	mutex_lock(&devp->afe_mutex);
	manual_flag = 0;
	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {
		tvafe_pr_err("%s:(%d) decode haven't opened\n",
			     __func__, devp->index);
		mutex_unlock(&devp->afe_mutex);
		return;
	}
	if (!IS_TVAFE_SRC(port)) {
		tvafe_pr_err("%s(%d), %s unsupport\n", __func__,
				devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return;
	}

	if (devp->flags & TVAFE_FLAG_DEV_STARTED) {
		tvafe_pr_err("%s(%d), %s started already\n", __func__,
				devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return;
	}

	/*fix vg877 machine ntsc443 flash*/
	if (fmt == TVIN_SIG_FMT_CVBS_NTSC_443 && (IS_TVAFE_AVIN_SRC(port)))
		W_APB_REG(CVD2_H_LOOP_MAXSTATE, 0x9);

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
	if (tvafe_cpu_type() >= TVAFE_CPU_TYPE_TL1) {
		adc_ch = tvafe_port_to_channel(port, devp->pinmux);
		if (adc_ch == TVAFE_ADC_CH_0)
			tvafe_avin_detect_ch1_anlog_enable(0);
		else if (adc_ch == TVAFE_ADC_CH_1)
			tvafe_avin_detect_ch2_anlog_enable(0);
	}
#endif

	tvafe->parm.info.fmt = fmt;
	tvafe->parm.info.status = TVIN_SIG_STATUS_STABLE;
	tvafe_vbi_set_wss();
	devp->flags |= TVAFE_FLAG_DEV_STARTED;

	tvafe_pr_info("%s start fmt:%s ok.\n",
			__func__, tvin_sig_fmt_str(fmt));

	mutex_unlock(&devp->afe_mutex);
}

/*
 * tvafe stop port
 */
static void tvafe_dec_stop(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvafe_adc_ch_e adc_ch = TVAFE_ADC_CH_NULL;

	mutex_lock(&devp->afe_mutex);
	if (!(devp->flags & TVAFE_FLAG_DEV_STARTED)) {
		tvafe_pr_err("%s:(%d) decode haven't started\n",
			     __func__, devp->index);
		mutex_unlock(&devp->afe_mutex);
		return;
	}
	if (!IS_TVAFE_SRC(port)) {
		tvafe_pr_err("%s(%d), %s unsupport\n", __func__,
				devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return;
	}

	/* init variable */
	memset(&tvafe->cvd2.info, 0, sizeof(struct tvafe_cvd2_info_s));
	memset(&tvafe->parm.info, 0, sizeof(struct tvin_info_s));

	tvafe->parm.port = port;
	/* need to do ... */
	/** write 7740 register for cvbs clamp **/
	if (!(devp->flags & TVAFE_POWERDOWN_IN_IDLE)) {
		tvafe->cvd2.fmt_loop_cnt = 0;
		/* reset loop cnt after channel switch */
#ifdef TVAFE_SET_CVBS_PGA_EN
		tvafe_cvd2_reset_pga();
#endif

#ifdef TVAFE_SET_CVBS_CDTO_EN
		tvafe_cvd2_set_default_cdto(&tvafe->cvd2);
#endif
		tvafe_cvd2_set_default_de(&tvafe->cvd2);
	}
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
	if (tvafe_cpu_type() >= TVAFE_CPU_TYPE_TL1) {
		adc_ch = tvafe_port_to_channel(port, devp->pinmux);
		if (adc_ch == TVAFE_ADC_CH_0)
			tvafe_avin_detect_ch1_anlog_enable(1);
		else if (adc_ch == TVAFE_ADC_CH_1)
			tvafe_avin_detect_ch2_anlog_enable(1);
	}
#endif

	devp->flags &= (~TVAFE_FLAG_DEV_STARTED);

	tvafe_pr_info("%s stop port:0x%x ok.\n", __func__, port);

	mutex_unlock(&devp->afe_mutex);
}

/*
 * tvafe close port
 */
static void tvafe_dec_close(struct tvin_frontend_s *fe)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvafe_adc_ch_e adc_ch = TVAFE_ADC_CH_NULL;

	tvafe_pr_info("%s begin to close afe.\n", __func__);
	mutex_lock(&devp->afe_mutex);
	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED) ||
		(devp->flags & TVAFE_POWERDOWN_IN_IDLE)) {
		tvafe_pr_err("%s:(%d) decode haven't opened\n",
			     __func__, devp->index);
		mutex_unlock(&devp->afe_mutex);
		return;
	}
	if (!IS_TVAFE_SRC(tvafe->parm.port)) {
		tvafe_pr_err("%s(%d), %s unsupport\n", __func__,
				devp->index, tvin_port_str(tvafe->parm.port));
		mutex_unlock(&devp->afe_mutex);
		return;
	}
	tvafe_clk_status = false;
	/*del_timer_sync(&devp->timer);*/
#ifdef CONFIG_AMLOGIC_ATV_DEMOD
	g_tvafe_info = NULL;
	/* register aml_fe hook for atv search */
	aml_fe_hook_cvd(NULL, NULL, NULL, NULL);
#endif
	/**set cvd2 reset to high**/
	tvafe_cvd2_hold_rst();
	/**disable av out**/
	tvafe_enable_avout(tvafe->parm.port, false);

	if (IS_TVAFE_AVIN_SRC(tvafe->parm.port))
		avport_opened = 0;

#ifdef TVAFE_POWERDOWN_IN_IDLE
	/**disable tvafe clock**/
	devp->flags |= TVAFE_POWERDOWN_IN_IDLE;
	tvafe_enable_module(false);
#ifdef CONFIG_AMLOGIC_MEDIA_ADC
	if (IS_TVAFE_ATV_SRC(tvafe->parm.port)) {
		adc_set_pll_cntl(0, ADC_EN_ATV_DEMOD, NULL);
		adc_set_filter_ctrl(0, FILTER_ATV_DEMOD, NULL);
	} else if (IS_TVAFE_AVIN_SRC(tvafe->parm.port)) {
		adc_set_pll_cntl(0, ADC_EN_TVAFE, NULL);
		adc_set_filter_ctrl(0, FILTER_TVAFE, NULL);
	}
#endif
#endif

#ifdef CONFIG_CMA
	tvafe_cma_release(devp);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
	if (tvafe_cpu_type() >= TVAFE_CPU_TYPE_TL1) {
		adc_ch = tvafe_port_to_channel(tvafe->parm.port, devp->pinmux);
		/*avsync tip set 1 to resume av detect*/
		if (adc_ch == TVAFE_ADC_CH_0) {
			avport_opened = 0;
			W_APB_BIT(TVFE_CLAMP_INTF, 0,
				  CLAMP_EN_BIT, CLAMP_EN_WID);
			/*channel1*/
			tvafe_cha1_detect_restart_config();
		} else if (adc_ch == TVAFE_ADC_CH_1) {
			W_APB_BIT(TVFE_CLAMP_INTF, 0,
				  CLAMP_EN_BIT, CLAMP_EN_WID);
			avport_opened = 0;
			/*channel2*/
			tvafe_cha2_detect_restart_config();
		} else {
			W_APB_BIT(TVFE_CLAMP_INTF, 0,
				  CLAMP_EN_BIT, CLAMP_EN_WID);
			tvafe_cha1_detect_restart_config();
			tvafe_cha2_detect_restart_config();
		}
	}
#endif
	/* init variable */
	memset(tvafe, 0, sizeof(struct tvafe_info_s));

	devp->flags &= (~TVAFE_FLAG_DEV_STARTED);
	devp->flags &= (~TVAFE_FLAG_DEV_OPENED);

	tvafe_pr_info("%s close afe ok.\n", __func__);

	mutex_unlock(&devp->afe_mutex);
}

/*
 * tvafe vsync interrupt function
 */
static int tvafe_dec_isr(struct tvin_frontend_s *fe, unsigned int hcnt64)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;
	enum tvin_aspect_ratio_e aspect_ratio = TVIN_ASPECT_NULL;
	struct tvafe_user_param_s *user_param = tvafe_get_user_param();
	static int count[10] = {0};
	int i, unlock = 0;

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED) ||
		(devp->flags & TVAFE_POWERDOWN_IN_IDLE)) {
		if (tvafe_dbg_print & TVAFE_DBG_ISR)
			tvafe_pr_err("tvafe haven't opened, isr error!!!\n");
		return TVIN_BUF_SKIP;
	}

	if (force_stable)
		return TVIN_BUF_NULL;

	/* if there is any error or overflow, do some reset, then return -1;*/
	if (tvafe->parm.info.status != TVIN_SIG_STATUS_STABLE ||
	    tvafe->parm.info.fmt == TVIN_SIG_FMT_NULL) {
		if (devp->flags & TVAFE_FLAG_DEV_SNOW_FLAG)
			return TVIN_BUF_NULL;
		else
			return TVIN_BUF_SKIP;
	}

	if (!IS_TVAFE_SRC(port))
		return TVIN_BUF_SKIP;

	if (tvafe->parm.info.status == TVIN_SIG_STATUS_STABLE &&
	    IS_TVAFE_AVIN_SRC(port)) {
		if (tvafe->cvd2.info.h_unlock_cnt >
		    user_param->unlock_cnt_max) {
			if (tvafe_dbg_print & TVAFE_DBG_ISR) {
				pr_info("%s: h_unlock cnt: %d\n",
					__func__,
					tvafe->cvd2.info.h_unlock_cnt);
			}
			unlock = 1;
		}
		if (tvafe->cvd2.info.v_unlock_cnt >
		    user_param->unlock_cnt_max) {
			if (tvafe_dbg_print & TVAFE_DBG_ISR) {
				pr_info("%s: v_unlock cnt: %d\n",
					__func__,
					tvafe->cvd2.info.v_unlock_cnt);
			}
			unlock = 1;
		}
		if (tvafe->cvd2.info.sig_unlock_cnt >
		    user_param->unlock_cnt_max) {
			if (tvafe_dbg_print & TVAFE_DBG_ISR) {
				pr_info("%s: sig_unlock cnt: %d\n",
					__func__,
					tvafe->cvd2.info.sig_unlock_cnt);
			}
			unlock = 1;
		}
		if (unlock) {
			if (tvafe->cvd2.info.unlock_cnt++ >= 65536)
				tvafe->cvd2.info.unlock_cnt = 0;
			return TVIN_BUF_SKIP;
		}
	}

	if (tvafe->cvd2.info.isr_cnt++ >= 65536)
		tvafe->cvd2.info.isr_cnt = 0;

	/* TVAFE CVD2 3D works abnormally => reset cvd2 */
	tvafe_cvd2_check_3d_comb(&tvafe->cvd2);

#ifdef TVAFE_SET_CVBS_PGA_EN
	if (!IS_TVAFE_ATV_SRC(port))
		tvafe_cvd2_adj_pga(&tvafe->cvd2);
#endif

	if (!tvafe_atv_search_channel) {
		if (tvafe->parm.info.fmt == TVIN_SIG_FMT_CVBS_PAL_I) {
#ifdef TVAFE_SET_CVBS_CDTO_EN
			tvafe_cvd2_adj_cdto(&tvafe->cvd2, hcnt64);
#endif
			tvafe_cvd2_adj_hs(&tvafe->cvd2, hcnt64);
		} else if (tvafe->parm.info.fmt == TVIN_SIG_FMT_CVBS_NTSC_M) {
			tvafe_cvd2_adj_hs_ntsc(&tvafe->cvd2, hcnt64);
		}
	}

	aspect_ratio = tvafe_cvd2_get_wss();
	switch (aspect_ratio) {
	case TVIN_ASPECT_NULL:
		count[TVIN_ASPECT_NULL]++;
		break;
	case TVIN_ASPECT_1x1:
		count[TVIN_ASPECT_1x1]++;
		break;
	case TVIN_ASPECT_4x3_FULL:
		count[TVIN_ASPECT_4x3_FULL]++;
		break;
	case TVIN_ASPECT_14x9_FULL:
		count[TVIN_ASPECT_14x9_FULL]++;
		break;
	case TVIN_ASPECT_14x9_LB_CENTER:
		count[TVIN_ASPECT_14x9_LB_CENTER]++;
		break;
	case TVIN_ASPECT_14x9_LB_TOP:
		count[TVIN_ASPECT_14x9_LB_TOP]++;
		break;
	case TVIN_ASPECT_16x9_FULL:
		count[TVIN_ASPECT_16x9_FULL]++;
		break;
	case TVIN_ASPECT_16x9_LB_CENTER:
		count[TVIN_ASPECT_16x9_LB_CENTER]++;
		break;
	case TVIN_ASPECT_16x9_LB_TOP:
		count[TVIN_ASPECT_16x9_LB_TOP]++;
		break;
	case TVIN_ASPECT_MAX:
		break;
	}
	/*over 30/40 times,ratio is effective*/
	if (++tvafe->aspect_ratio_cnt > TVAFE_RATIO_CNT) {
		for (i = 0; i < TVIN_ASPECT_MAX; i++) {
			if (count[i] > 30) {
				if (tvafe->aspect_ratio != i)
					pr_info("wss aspect_ratio:%d-%d i:%d\n",
						tvafe->aspect_ratio, aspect_ratio, i);
				tvafe->aspect_ratio = i;
				break;
			}
		}
		for (i = 0; i < TVIN_ASPECT_MAX; i++)
			count[i] = 0;
		tvafe->aspect_ratio_cnt = 0;
	}

	return TVIN_BUF_NULL;
}

static struct tvin_decoder_ops_s tvafe_dec_ops = {
	.support    = tvafe_dec_support,
	.open       = tvafe_dec_open,
	.start      = tvafe_dec_start,
	.stop       = tvafe_dec_stop,
	.close      = tvafe_dec_close,
	.decode_isr = tvafe_dec_isr,
	.callmaster_det = NULL,
};

static bool white_pattern_reset_pag(enum tvin_port_e port,
				    struct tvafe_cvd2_s *cvd2)
{
	if (IS_TVAFE_AVIN_SRC(port)) {
		if (port == TVIN_PORT_CVBS1) {
			if (av1_plugin_state == 1) {
				top_init_en = 1;
				return true;
			}
		}

		if (port == TVIN_PORT_CVBS2) {
			if (av2_plugin_state == 1) {
				top_init_en = 1;
				return true;
			}
		}

		if ((av1_plugin_state == 0 || av2_plugin_state == 0) &&
			!R_APB_BIT(TVFE_CLAMP_INTF,
				CLAMP_EN_BIT, CLAMP_EN_WID)) {
			white_pattern_pga_reset(port);
			tvafe_pr_info("av1:%u av2:%u\n", av1_plugin_state,
				      av2_plugin_state);
			top_init_en = 0;
			return true;
		}
	}

	return false;
}

/*
 * tvafe signal status: signal on/off
 */
static bool tvafe_is_nosig(struct tvin_frontend_s *fe)
{
	bool ret = false;
	/* Get the per-device structure that contains this frontend */
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;
	enum tvafe_adc_ch_e adc_ch = TVAFE_ADC_CH_NULL;

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED) ||
		(devp->flags & TVAFE_POWERDOWN_IN_IDLE)) {
		if (tvafe_dbg_print & TVAFE_DBG_SMR)
			tvafe_pr_err("tvafe haven't opened OR suspend:flags:0x%x!!!\n",
					devp->flags);
		return true;
	}
	if (force_stable)
		return ret;

	if (!IS_TVAFE_SRC(port))
		return ret;

	if (white_pattern_reset_pag(port, &tvafe->cvd2))
		return true;

	if (tvafe->cvd2.info.smr_cnt++ >= 65536)
		tvafe->cvd2.info.smr_cnt = 0;

	if (devp->flags & TVAFE_FLAG_DEV_STARTED)
		ret = tvafe_cvd2_no_sig(&tvafe->cvd2, &devp->mem, 1);
	else
		ret = tvafe_cvd2_no_sig(&tvafe->cvd2, &devp->mem, 0);

	if (!tvafe_mode && IS_TVAFE_ATV_SRC(port) &&
	    (devp->flags & TVAFE_FLAG_DEV_SNOW_FLAG)) { /* playing snow */
		tvafe->cvd2.info.snow_state[3] = tvafe->cvd2.info.snow_state[2];
		tvafe->cvd2.info.snow_state[2] = tvafe->cvd2.info.snow_state[1];
		tvafe->cvd2.info.snow_state[1] = tvafe->cvd2.info.snow_state[0];
		if (ret)
			tvafe->cvd2.info.snow_state[0] = 0;
		else
			tvafe->cvd2.info.snow_state[0] = 1;
		if ((tvafe->cvd2.info.snow_state[3] +
			tvafe->cvd2.info.snow_state[2] +
			tvafe->cvd2.info.snow_state[1] +
			tvafe->cvd2.info.snow_state[0]) == 4)
			ret = false;
		else
			ret = true;
	}
	if (IS_TVAFE_ATV_SRC(port) &&
	    tvafe->cvd2.config_fmt == TVIN_SIG_FMT_CVBS_PAL_I) {
		/*fix black side when config atv snow*/
		if (ret && (devp->flags & TVAFE_FLAG_DEV_SNOW_FLAG) &&
		    tvafe->cvd2.info.state != TVAFE_CVD2_STATE_FIND)
			tvafe_snow_config_acd();
		else if (tvafe->cvd2.info.state == TVAFE_CVD2_STATE_FIND)
			tvafe_snow_config_acd_resume();
	}

	/* normal signal & adc reg error, reload source mux */
	if (tvafe->cvd2.info.adc_reload_en && !ret) {
		adc_ch = tvafe_port_to_channel(port, devp->pinmux);
		tvafe_adc_pin_mux(adc_ch);
	}

	return ret;
}

/*
 * tvafe signal mode status: change/unchangeable
 */
bool tvafe_fmt_chg(struct tvin_frontend_s *fe)
{
	bool ret = false;
	/* Get the per-device structure that contains this frontend */
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {
		tvafe_pr_err("tvafe haven't opened, get fmt chg error!!!\n");
		return true;
	}
	if (force_stable)
		return ret;
	if (IS_TVAFE_SRC(port))
		ret = tvafe_cvd2_fmt_chg(&tvafe->cvd2);

	return ret;
}

/*
 * tvafe adc lock status: lock/unlock
 */
bool tvafe_pll_lock(struct tvin_frontend_s *fe)
{
	bool ret = true;

	return ret;
}

/*
 * tvafe search format number
 */
enum tvin_sig_fmt_e tvafe_get_fmt(struct tvin_frontend_s *fe)
{
	enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;
	/* Get the per-device structure that contains this frontend */
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {
		tvafe_pr_err("tvafe haven't opened, get sig fmt error!!!\n");
		return fmt;
	}
	if (IS_TVAFE_SRC(port))
		fmt = tvafe_cvd2_get_format(&tvafe->cvd2);

	tvafe->parm.info.fmt = fmt;
	if (tvafe_dbg_print & TVAFE_DBG_SMR)
		tvafe_pr_info("%s fmt:%s.\n", __func__,
			tvin_sig_fmt_str(fmt));

	return fmt;
}

static unsigned int tvafe_fmt_fps_table[] = {
	60, /* ntsc_m */
	60, /* ntsc_443 */
	50, /* pal_i */
	60, /* pal_m */
	60, /* pal_60 */
	50, /* pal_cn */
	50, /* secam */
	50, /* ntsc_50 */
	50  /* max */
};

static void tvafe_cutwindow_update(struct tvafe_info_s *tvafe,
				   struct tvin_sig_property_s *prop)
{
	struct tvafe_user_param_s *user_param = &tvafe_user_param;
	unsigned int hs_adj_val = 0;
	unsigned int vs_adj_val = 0;
	unsigned int i;

	if (user_param->cutwin_test_en & 0x8) {
		vs_adj_val = user_param->cutwin_test_vcut;
		if (user_param->auto_adj_en & TVAFE_AUTO_VS_MODE) {
			prop->vs = user_param->cutwin_test_vcut;
			prop->ve = 0;
		} else {
			prop->vs = user_param->cutwin_test_vcut;
			prop->ve = user_param->cutwin_test_vcut;
		}
	} else {
		if (tvafe->cvd2.info.vs_adj_en) {
			if (user_param->auto_adj_en & TVAFE_AUTO_VS_MODE) {
				if (tvafe->cvd2.info.vs_adj_dir) {
					/* < std freq*/
					vs_adj_val = 0;
				} else {/* > std freq*/
					i = tvafe->cvd2.info.vs_adj_level;
					if (i < 5) {
						vs_adj_val =
						user_param->cutwindow_val_v[i];
					} else {
						vs_adj_val = 0;
					}
				}
				prop->vs = vs_adj_val;
				prop->ve = 0;
			} else {
				i = tvafe->cvd2.info.vs_adj_level;
				if (i < 5) {
					vs_adj_val =
						user_param->cutwindow_val_v[i];
				} else {
					vs_adj_val = 0;
				}
				prop->vs = vs_adj_val;
				prop->ve = vs_adj_val;
			}
		} else {
			prop->vs = 0;
			prop->ve = 0;
		}
	}
	if (vs_adj_val != vs_adj_val_pre) {
		if (tvafe_dbg_print & TVAFE_DBG_NOSTD) {
			tvafe_pr_info("%s: vs_adj_en:%d, vs_adj_level:%d, vs_adj_val:%d\n",
				      __func__,
				      tvafe->cvd2.info.vs_adj_en,
				      tvafe->cvd2.info.vs_adj_level,
				      vs_adj_val);
		}
		vs_adj_val_pre = vs_adj_val;
	}

	if (user_param->cutwin_test_en & 0x4) {
		vs_adj_val = user_param->cutwin_test_hcut;
		if (user_param->auto_adj_en & TVAFE_AUTO_HS_MODE) {
			prop->hs = 0;
			prop->he = user_param->cutwin_test_hcut;
		} else {
			prop->hs = user_param->cutwin_test_hcut;
			prop->he = 0;
		}
	} else {
		if (tvafe->cvd2.info.hs_adj_en) {
			i = tvafe->cvd2.info.hs_adj_level;
			if (i < 4) {
				hs_adj_val = user_param->cutwindow_val_h[i];
			} else if (i == 4) {
				hs_adj_val = user_param->cutwindow_val_h[i];
				prop->vs = user_param->cutwindow_val_vs_ve;
				prop->ve = user_param->cutwindow_val_vs_ve;
			} else {
				hs_adj_val = 0;
			}

			if (tvafe->cvd2.info.hs_adj_dir) {
				prop->hs = 0;
				prop->he = hs_adj_val;
			} else {
				prop->hs = hs_adj_val;
				prop->he = 0;
			}
		} else {
			prop->hs = 0;
			prop->he = 0;
		}
	}
}

/*
 * tvafe signal property: 2D/3D, color format, aspect ratio, pixel repeat
 */
static void tvafe_get_sig_property(struct tvin_frontend_s *fe,
		struct tvin_sig_property_s *prop)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	struct tvafe_user_param_s *user_param = &tvafe_user_param;
	enum tvin_port_e port = tvafe->parm.port;
	unsigned int index;

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED) ||
		(devp->flags & TVAFE_POWERDOWN_IN_IDLE)) {
		if (tvafe_dbg_print & TVAFE_DBG_NORMAL)
			tvafe_pr_err("%s tvafe not opened OR suspend:flags:0x%x!\n",
					__func__, devp->flags);
		return;
	}

	prop->trans_fmt = TVIN_TFMT_2D;
	prop->color_format = TVIN_YUV444;
	prop->dest_cfmt = TVIN_YUV422;
	if (tvafe->cvd2.config_fmt < TVIN_SIG_FMT_CVBS_NTSC_M ||
	    tvafe->cvd2.config_fmt >= TVIN_SIG_FMT_CVBS_MAX)
		index = 0;
	else
		index = tvafe->cvd2.config_fmt - TVIN_SIG_FMT_CVBS_NTSC_M;
	prop->fps = tvafe_fmt_fps_table[index];

#ifdef TVAFE_CVD2_AUTO_DE_ENABLE
	if (devp->flags & TVAFE_FLAG_DEV_STARTED) {
		if (IS_TVAFE_SRC(port))
			tvafe_cutwindow_update(tvafe, prop);
	}
#endif
	prop->color_fmt_range = TVIN_YUV_LIMIT;
	prop->aspect_ratio = tvafe->aspect_ratio;
	prop->decimation_ratio = 0;
	prop->dvi_info = 0;
	prop->skip_vf_num = user_param->skip_vf_num;
}

/*
 *get cvbs secam source's phase
 */
static bool tvafe_cvbs_get_secam_phase(struct tvin_frontend_s *fe)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;

	if (tvafe->cvd2.config_fmt == TVIN_SIG_FMT_CVBS_SECAM)
		return tvafe->cvd2.hw.secam_phase;
	else
		return 0;
}

bool tvafe_get_snow_cfg(void)
{
	return snow_cfg;
}
EXPORT_SYMBOL(tvafe_get_snow_cfg);

void tvafe_set_snow_cfg(bool cfg)
{
	snow_cfg = cfg;
}
EXPORT_SYMBOL(tvafe_set_snow_cfg);

/**check frame skip,only for av input*/
static bool tvafe_cvbs_check_frame_skip(struct tvin_frontend_s *fe)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
		frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	struct tvafe_cvd2_s *cvd2 = &tvafe->cvd2;
	enum tvin_port_e port = tvafe->parm.port;
	bool ret = false;

	if (!devp->frame_skip_enable ||
		(devp->flags & TVAFE_FLAG_DEV_SNOW_FLAG)) {
		ret = false;
	} else if ((cvd2->hw.no_sig || !cvd2->hw.h_lock || !cvd2->hw.v_lock) &&
		   IS_TVAFE_AVIN_SRC(port)) {
		if (tvafe_dbg_print & TVAFE_DBG_NORMAL)
			tvafe_pr_err("cvbs signal unstable, skip frame!!!\n");
		ret = true;
	}
	return ret;
}

static struct tvin_state_machine_ops_s tvafe_sm_ops = {
	.nosig            = tvafe_is_nosig,
	.fmt_changed      = tvafe_fmt_chg,
	.get_fmt          = tvafe_get_fmt,
	.fmt_config       = NULL,
	.adc_cal          = NULL,
	.pll_lock         = tvafe_pll_lock,
	.get_sig_property  = tvafe_get_sig_property,
	.vga_set_param    = NULL,
	.vga_get_param    = NULL,
	.check_frame_skip = tvafe_cvbs_check_frame_skip,
	.get_secam_phase = tvafe_cvbs_get_secam_phase,
};

static int tvafe_open(struct inode *inode, struct file *file)
{
	struct tvafe_dev_s *devp;

	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct tvafe_dev_s, cdev);
	file->private_data = devp;

	/* ... */
	if (tvafe_dbg_print & TVAFE_DBG_NORMAL)
		tvafe_pr_info("%s: open device\n", __func__);

	return 0;
}

static int tvafe_release(struct inode *inode, struct file *file)
{
	struct tvafe_dev_s *devp = file->private_data;

	file->private_data = NULL;

	/* Release some other fields */
	/* ... */
	if (tvafe_dbg_print & TVAFE_DBG_NORMAL)
		tvafe_pr_info("tvafe: device %d release ok.\n", devp->index);

	return 0;
}

static long tvafe_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	unsigned int temp = 0;
	void __user *argp = (void __user *)arg;
	struct tvafe_dev_s *devp = file->private_data;
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvafe_cvbs_video_e cvbs_lock_status = TVAFE_CVBS_VIDEO_HV_UNLOCKED;
	enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;

	if (_IOC_TYPE(cmd) != _TM_T) {
		tvafe_pr_err("%s invalid command: %u\n", __func__, cmd);
		return -EINVAL;
	}

	/* tvafe_pr_info("%s command: %u\n", __func__, cmd); */
	if (disable_api)
		return -EPERM;

	mutex_lock(&devp->afe_mutex);
	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED) &&
		cmd != TVIN_IOC_S_AFE_SNOW_CFG) {
		tvafe_pr_info("%s, tvafe device is disable, ignore the command %d\n",
				__func__, cmd);
		mutex_unlock(&devp->afe_mutex);
		return -EPERM;
	}

	switch (cmd) {
	case TVIN_IOC_LOAD_REG:
		{
		if (copy_from_user(&tvafe_regs, argp,
			sizeof(struct am_regs_s))) {
			tvafe_pr_info("load reg errors: can't get buffer length\n");
			ret = -EINVAL;
			break;
		}

		if (!tvafe_regs.length || tvafe_regs.length > 512) {
			tvafe_pr_info("load regs error: buffer length overflow!!!, length=0x%x\n",
				tvafe_regs.length);
			ret = -EINVAL;
			break;
		}
		if (enable_db_reg)
			tvafe_set_regmap(&tvafe_regs);

		break;
		}
	case TVIN_IOC_S_AFE_SNOW_CFG:
		/*tl1/txhd tvconfig snow en/disable*/
		if (copy_from_user(&temp, argp, sizeof(unsigned int))) {
			tvafe_pr_info("snow_cfg: get param err\n");
			ret = -EINVAL;
			break;
		}
		if (temp == 1)
			tvafe_set_snow_cfg(true);
		else
			tvafe_set_snow_cfg(false);
		tvafe_pr_info("tvconfig snow:%d\n", snow_cfg);
		break;
	case TVIN_IOC_S_AFE_SNOW_ON:
		devp->flags |= TVAFE_FLAG_DEV_SNOW_FLAG;
		tvafe_snow_function_flag = true;
		tvafe_snow_config(1);
		tvafe_snow_config_clamp(1);
		if (tvafe_dbg_print & TVAFE_DBG_NORMAL)
			tvafe_pr_info("TVIN_IOC_S_AFE_SNOW_ON\n");
		break;
	case TVIN_IOC_S_AFE_SNOW_OFF:
		tvafe_snow_config(0);
		tvafe_snow_config_clamp(0);
		devp->flags &= (~TVAFE_FLAG_DEV_SNOW_FLAG);
		if (tvafe_dbg_print & TVAFE_DBG_NORMAL)
			tvafe_pr_info("TVIN_IOC_S_AFE_SNOW_OFF\n");
		break;
	case TVIN_IOC_G_AFE_CVBS_LOCK:
		cvbs_lock_status = tvafe_cvd2_get_lock_status(&tvafe->cvd2);
		if (copy_to_user(argp, &cvbs_lock_status, sizeof(int))) {
			ret = -EFAULT;
			break;
		}
		tvafe_pr_info("%s: get cvd2 lock status :%d.\n",
			__func__, cvbs_lock_status);
		break;
	case TVIN_IOC_S_AFE_CVBS_STD:
		if (copy_from_user(&fmt, argp, sizeof(enum tvin_sig_fmt_e))) {
			ret = -EFAULT;
			break;
		}
		tvafe->cvd2.manual_fmt = fmt;
		tvafe_pr_info("%s: ioctl set cvd2 manual fmt:%s.\n",
			__func__, tvin_sig_fmt_str(fmt));
		if (fmt != TVIN_SIG_FMT_NULL)
			manual_flag = 1;
		if (tvin_get_sm_status(tvafe->parm.index)
				== TVIN_SM_STATUS_NOSIG) {
			tvafe_pr_info("%s: reset_tvin_smr.\n", __func__);
			reset_tvin_smr(tvafe->parm.index);
		}
		break;
	case TVIN_IOC_G_AFE_CVBS_STD:
		if (tvafe->cvd2.info.state == TVAFE_CVD2_STATE_FIND)
			fmt = tvafe->cvd2.config_fmt;
		if (copy_to_user(argp, &fmt, sizeof(enum tvin_sig_fmt_e)))
			ret = -EFAULT;
		tvafe_pr_info("%s: ioctl get fmt:%s.\n",
			__func__, tvin_sig_fmt_str(fmt));
		break;
	case TVIN_IOC_S_AFE_ATV_SEARCH:
		if (copy_from_user(&temp, argp, sizeof(unsigned int))) {
			tvafe_pr_info("set atv_search: get param err\n");
			ret = -EINVAL;
			break;
		}
		if (temp)
			tvafe_atv_search_channel = true;
		else
			tvafe_atv_search_channel = false;
		tvafe_pr_info("set atv_search: %d\n", tvafe_atv_search_channel);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	mutex_unlock(&devp->afe_mutex);
	return ret;
}

#ifdef CONFIG_COMPAT
static long tvafe_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = tvafe_ioctl(file, cmd, arg);
	return ret;
}
#endif

/* File operations structure. Defined in linux/fs.h */
static const struct file_operations tvafe_fops = {
	.owner   = THIS_MODULE,         /* Owner */
	.open    = tvafe_open,          /* Open method */
	.release = tvafe_release,       /* Release method */
	.unlocked_ioctl   = tvafe_ioctl,         /* Ioctl method */
#ifdef CONFIG_COMPAT
	.compat_ioctl = tvafe_compat_ioctl,
#endif
	/* ... */
};

static int tvafe_add_cdev(struct cdev *cdevp,
		const struct file_operations *fops, int minor)
{
	int ret;
	dev_t devno = MKDEV(MAJOR(tvafe_devno), minor);

	cdev_init(cdevp, fops);
	cdevp->owner = THIS_MODULE;
	ret = cdev_add(cdevp, devno, 1);
	return ret;
}

static struct device *tvafe_create_device(struct device *parent, int id)
{
	dev_t devno = MKDEV(MAJOR(tvafe_devno),  id);

	return device_create(tvafe_clsp, parent, devno, NULL, "%s0",
			TVAFE_DEVICE_NAME);
	/* @to do this after Middleware API modified */
	/*return device_create(tvafe_clsp, parent, devno, NULL, "%s",*/
	/*  TVAFE_DEVICE_NAME); */
}

static void tvafe_delete_device(int minor)
{
	dev_t devno = MKDEV(MAJOR(tvafe_devno), minor);

	device_destroy(tvafe_clsp, devno);
}

void __iomem *tvafe_reg_base;
void __iomem *ana_addr;
static void __iomem *hiu_reg_base;

int tvafe_reg_read(unsigned int reg, unsigned int *val)
{
	if (tvafe_reg_base)
		*val = readl(tvafe_reg_base + reg);
	return 0;
}
EXPORT_SYMBOL(tvafe_reg_read);

int tvafe_reg_write(unsigned int reg, unsigned int val)
{
	if (tvafe_reg_base)
		writel(val, (tvafe_reg_base + reg));
	return 0;
}
EXPORT_SYMBOL(tvafe_reg_write);

int tvafe_vbi_reg_read(unsigned int reg, unsigned int *val)
{
	if (tvafe_clk_status && tvafe_reg_base)
		*val = readl(tvafe_reg_base + reg);
	else
		return -1;
	return 0;
}
EXPORT_SYMBOL(tvafe_vbi_reg_read);

int tvafe_vbi_reg_write(unsigned int reg, unsigned int val)
{
	if (tvafe_clk_status)
		writel(val, (tvafe_reg_base + reg));
	else
		return -1;
	return 0;
}
EXPORT_SYMBOL(tvafe_vbi_reg_write);

int tvafe_hiu_reg_read(unsigned int reg, unsigned int *val)
{
	if (hiu_reg_base)
		*val = readl(hiu_reg_base + (reg << 2));
	else
		*val =  aml_read_hiubus(reg);
	return 0;
}
EXPORT_SYMBOL(tvafe_hiu_reg_read);

int tvafe_hiu_reg_write(unsigned int reg, unsigned int val)
{
	if (hiu_reg_base)
		writel(val, hiu_reg_base + (reg << 2));
	else
		aml_write_hiubus(reg, val);
	return 0;
}
EXPORT_SYMBOL(tvafe_hiu_reg_write);

int tvafe_cpu_type(void)
{
	return s_tvafe_data->cpu_id;
}
EXPORT_SYMBOL(tvafe_cpu_type);

void tvafe_clk_gate_ctrl(int status)
{
	if (status) {
		if (tvafe_clkgate.clk_gate_state) {
			tvafe_pr_info("clk_gate is already on\n");
			return;
		}

		if (IS_ERR(tvafe_clkgate.vdac_clk_gate))
			tvafe_pr_err("error: %s: vdac_clk_gate\n", __func__);
		else
			clk_prepare_enable(tvafe_clkgate.vdac_clk_gate);

		tvafe_clkgate.clk_gate_state = 1;
	} else {
		if (tvafe_clkgate.clk_gate_state == 0) {
			tvafe_pr_info("clk_gate is already off\n");
			return;
		}

		if (IS_ERR(tvafe_clkgate.vdac_clk_gate))
			tvafe_pr_err("error: %s: vdac_clk_gate\n", __func__);
		else
			clk_disable_unprepare(tvafe_clkgate.vdac_clk_gate);

		tvafe_clkgate.clk_gate_state = 0;
	}
}

static void tvafe_clktree_probe(struct device *dev)
{
	tvafe_clkgate.clk_gate_state = 0;

	tvafe_clkgate.vdac_clk_gate = devm_clk_get(dev, "vdac_clk_gate");
	if (IS_ERR(tvafe_clkgate.vdac_clk_gate))
		tvafe_pr_err("error: %s: clk vdac_clk_gate\n", __func__);
}

static void tvafe_user_parameters_config(struct device_node *of_node)
{
	unsigned int val[6];
	int ret;

	if (!of_node)
		return;

	/* cutwindow config */
	ret = of_property_read_u32_array(of_node, "cutwindow_val_h",
			tvafe_user_param.cutwindow_val_h, 5);
	if (ret)
		tvafe_pr_err("Can't get cutwindow_val_h\n");
	ret = of_property_read_u32_array(of_node, "cutwindow_val_v",
			tvafe_user_param.cutwindow_val_v, 5);
	if (ret)
		tvafe_pr_err("Can't get cutwindow_val_v\n");
	ret = of_property_read_u32_array(of_node, "horizontal_dir0",
			tvafe_user_param.horizontal_dir0, 5);
	if (ret)
		tvafe_pr_err("Can't get horizontal_dir0\n");
	ret = of_property_read_u32_array(of_node, "horizontal_dir1",
			tvafe_user_param.horizontal_dir1, 5);
	if (ret)
		tvafe_pr_err("Can't get horizontal_dir1\n");
	ret = of_property_read_u32_array(of_node, "horizontal_stp0",
			tvafe_user_param.horizontal_stp0, 5);
	if (ret)
		tvafe_pr_err("Can't get horizontal_stp0\n");
	ret = of_property_read_u32_array(of_node, "horizontal_stp1",
			tvafe_user_param.horizontal_stp1, 5);
	if (ret)
		tvafe_pr_err("Can't get horizontal_stp1\n");

	ret = of_property_read_u32(of_node, "auto_adj_en", &val[0]);
	if (ret == 0) {
		tvafe_pr_info("find auto_adj_en: 0x%x\n", val[0]);
		tvafe_user_param.auto_adj_en = val[0];
	}

	ret = of_property_read_u32_array(of_node, "nostd_vs_th", val, 2);
	if (ret == 0) {
		tvafe_user_param.nostd_vs_th = val[0];
		tvafe_user_param.force_vs_th_flag = val[1];
		tvafe_pr_info("find nostd_vs_th: 0x%x %d\n",
			tvafe_user_param.nostd_vs_th,
			tvafe_user_param.force_vs_th_flag);
	}

	ret = of_property_read_u32_array(of_node, "nostd_ctrl", val, 5);
	if (ret == 0) {
		tvafe_user_param.nostd_no_vs_th = val[0];
		tvafe_user_param.nostd_vs_cntl = val[1];
		tvafe_user_param.nostd_vloop_tc = val[2];
		tvafe_user_param.nostd_dmd_clp_step = val[3];
		tvafe_user_param.nostd_bypass_iir = val[4];
		tvafe_pr_info("find nostd_ctrl: 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			      tvafe_user_param.nostd_no_vs_th,
			      tvafe_user_param.nostd_vs_cntl,
			      tvafe_user_param.nostd_vloop_tc,
			      tvafe_user_param.nostd_dmd_clp_step,
			      tvafe_user_param.nostd_bypass_iir);
	}
}

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct meson_tvafe_data meson_tl1_tvafe_data = {
	.cpu_id = TVAFE_CPU_TYPE_TL1,
	.name = "meson-tl1-tvafe",

	.cvbs_pq_conf = NULL,
	.rf_pq_conf = NULL,
};
#endif

static struct meson_tvafe_data meson_tm2_tvafe_data = {
	.cpu_id = TVAFE_CPU_TYPE_TM2,
	.name = "meson-tm2-tvafe",

	.cvbs_pq_conf = NULL,
	.rf_pq_conf = NULL,
};

static struct meson_tvafe_data meson_tm2_b_tvafe_data = {
	.cpu_id = TVAFE_CPU_TYPE_TM2_B,
	.name = "meson-tm2_b-tvafe",

	.cvbs_pq_conf = NULL,
	.rf_pq_conf = NULL,
};

static struct meson_tvafe_data meson_t5_tvafe_data = {
	.cpu_id = TVAFE_CPU_TYPE_T5,
	.name = "meson-t5-tvafe",

	.cvbs_pq_conf = NULL,
	.rf_pq_conf = NULL,
};

static struct meson_tvafe_data meson_t5d_tvafe_data = {
	.cpu_id = TVAFE_CPU_TYPE_T5D,
	.name = "meson-t5d-tvafe",

	.cvbs_pq_conf = NULL,
	.rf_pq_conf = NULL,
};

static struct meson_tvafe_data meson_t3_tvafe_data = {
	.cpu_id = TVAFE_CPU_TYPE_T3,
	.name = "meson-t3-tvafe",

	.cvbs_pq_conf = NULL,
	.rf_pq_conf = NULL,
};

static struct meson_tvafe_data meson_t5w_tvafe_data = {
	.cpu_id = TVAFE_CPU_TYPE_T5W,
	.name = "meson-t5w-tvafe",

	.cvbs_pq_conf = NULL,
	.rf_pq_conf = NULL,
};

static const struct of_device_id meson_tvafe_dt_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, tvafe-tl1",
		.data		= &meson_tl1_tvafe_data,
	},
#endif
	{
		.compatible = "amlogic, tvafe-tm2",
		.data		= &meson_tm2_tvafe_data,
	}, {
		.compatible = "amlogic, tvafe-tm2_b",
		.data		= &meson_tm2_b_tvafe_data,
	}, {
		.compatible = "amlogic, tvafe-t5",
		.data		= &meson_t5_tvafe_data,
	}, {
		.compatible = "amlogic, tvafe-t5d",
		.data		= &meson_t5d_tvafe_data,
	}, {
		.compatible = "amlogic, tvafe-t3",
		.data		= &meson_t3_tvafe_data,
	}, {
		.compatible = "amlogic, tvafe-t5w",
		.data		= &meson_t5w_tvafe_data,
	},
	{}
};

static unsigned int tvafe_use_reserved_mem;
static struct resource tvafe_memobj;
static int tvafe_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tvafe_dev_s *tdevp;
	int size_io_reg;
	/*const void *name;*/
	/*int offset, size, mem_size_m;*/
	struct resource *res;
	const struct of_device_id *match;
	unsigned int sys_clk_reg_base = HHI_ANA_CLK_BASE;
	/* struct tvin_frontend_s * frontend; */

	match = of_match_device(meson_tvafe_dt_match, &pdev->dev);
	if (!match) {
		tvafe_pr_err("%s,no matched table\n", __func__);
		return -1;
	}

	s_tvafe_data = (struct meson_tvafe_data *)match->data;
	tvafe_pr_info("%s:cpu_id:%d,name:%s\n", __func__,
		s_tvafe_data->cpu_id, s_tvafe_data->name);
	tvafe_pq_config_probe(s_tvafe_data);

	tvafe_clktree_probe(&pdev->dev);

	ret = alloc_chrdev_region(&tvafe_devno, 0, 1, TVAFE_NAME);
	if (ret < 0) {
		tvafe_pr_err("%s: failed to allocate major number\n", __func__);
		goto fail_alloc_cdev_region;
	}
	tvafe_pr_info("%s: major %d\n", __func__, MAJOR(tvafe_devno));

	tvafe_clsp = class_create(THIS_MODULE, TVAFE_NAME);
	if (IS_ERR(tvafe_clsp)) {
		ret = PTR_ERR(tvafe_clsp);
		tvafe_pr_err("%s: failed to create class\n", __func__);
		goto fail_class_create;
	}

	/* allocate memory for the per-device structure */
	tdevp = kzalloc(sizeof(*tdevp), GFP_KERNEL);
	if (!tdevp)
		goto fail_kzalloc_tdev;

	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
					"tvafe_id", &tdevp->index);
		if (ret) {
			tvafe_pr_err("Can't find  tvafe id.\n");
			goto fail_get_id;
		}
	}
	tdevp->flags = 0;

	/* create cdev and register with sysfs */
	ret = tvafe_add_cdev(&tdevp->cdev, &tvafe_fops, tdevp->index);
	if (ret) {
		tvafe_pr_err("%s: failed to add cdev\n", __func__);
		goto fail_add_cdev;
	}
	/* create /dev nodes */
	tdevp->dev = tvafe_create_device(&pdev->dev, tdevp->index);
	if (IS_ERR(tdevp->dev)) {
		tvafe_pr_err("failed to create device node\n");
		/* @todo do with error */
		ret = PTR_ERR(tdevp->dev);
		goto fail_create_device;
	}

	/*create sysfs attribute files*/
	ret = tvafe_device_create_file(tdevp->dev);
	if (ret < 0) {
		tvafe_pr_err("%s: create attribute files fail.\n",
			__func__);
		goto fail_create_dbg_file;
	}

	/* get device memory */
	/* res = platform_get_resource(pdev, IORESOURCE_MEM, 0); */
	res = &tvafe_memobj;
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret == 0)
		tvafe_pr_info("tvafe memory resource done.\n");
	else
		tvafe_pr_info("can't get memory resource\n");
#ifdef CONFIG_CMA
	if (!tvafe_use_reserved_mem) {
		ret = of_property_read_u32(pdev->dev.of_node,
				"flag_cma", &tdevp->cma_config_flag);
		if (ret) {
			tvafe_pr_err("don't find  match flag_cma\n");
			tdevp->cma_config_flag = 0;
		}
		if (tdevp->cma_config_flag == 1) {
			ret = of_property_read_u32(pdev->dev.of_node,
				"cma_size", &tdevp->cma_mem_size);
			if (ret)
				tvafe_pr_err("don't find  match cma_size\n");
			else
				tdevp->cma_mem_size *= SZ_1M;
		} else if (tdevp->cma_config_flag == 0)
			tdevp->cma_mem_size =
				dma_get_cma_size_int_byte(&pdev->dev);
		tdevp->this_pdev = pdev;
		tdevp->cma_mem_alloc = 0;
		tdevp->cma_config_en = 1;
		tvafe_pr_info("cma_mem_size = %d MB\n",
				(u32)tdevp->cma_mem_size / SZ_1M);
	}
#endif
	tvafe_use_reserved_mem = 0;
	if (tdevp->cma_config_en != 1) {
		tdevp->mem.start = res->start;
		tdevp->mem.size = res->end - res->start + 1;
		tvafe_pr_info("tvafe cvd memory addr is:0x%x, cvd mem_size is:0x%x .\n",
				tdevp->mem.start,
				tdevp->mem.size);
	}
	if (of_property_read_u32_array(pdev->dev.of_node, "tvafe_pin_mux",
			(u32 *)tvafe_pinmux.pin, TVAFE_SRC_SIG_MAX_NUM)) {
		tvafe_pr_err("Can't get pinmux data.\n");
	}
	tdevp->pinmux = &tvafe_pinmux;
	if (!tdevp->pinmux) {
		tvafe_pr_err("tvafe: no platform data!\n");
		ret = -ENODEV;
	}

	if (of_get_property(pdev->dev.of_node, "pinctrl-names", NULL)) {
		struct pinctrl *p = devm_pinctrl_get_select_default(&pdev->dev);

		if (IS_ERR(p))
			tvafe_pr_err("tvafe request pinmux error!\n");
	}

	/*reg mem*/
	/*tvafe_pr_info("%s:tvafe start get  ioremap .\n", __func__);*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing memory resource\n");
		return -ENODEV;
	}
	size_io_reg = resource_size(res);
	tvafe_pr_info("%s:tvafe reg base=%p,size=0x%x\n",
		__func__, (void *)res->start, size_io_reg);
	if (!devm_request_mem_region(&pdev->dev,
				res->start, size_io_reg, pdev->name)) {
		dev_err(&pdev->dev, "Memory region busy\n");
		return -EBUSY;
	}
	tvafe_reg_base =
		devm_ioremap_nocache(&pdev->dev, res->start, size_io_reg);
	if (!tvafe_reg_base) {
		dev_err(&pdev->dev, "tvafe ioremap failed\n");
		return -ENOMEM;
	}
	tvafe_pr_info("%s: tvafe maped reg_base =%p, size=0x%x\n",
			__func__, tvafe_reg_base, size_io_reg);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		size_io_reg = resource_size(res);
		tvafe_pr_info("%s: hiu reg base=0x%p,size=0x%x\n",
			__func__, (void *)res->start, size_io_reg);
		hiu_reg_base =
			devm_ioremap_nocache(&pdev->dev, res->start, size_io_reg);
		if (!hiu_reg_base) {
			dev_err(&pdev->dev, "hiu ioremap failed\n");
			return -ENOMEM;
		}
		tvafe_pr_info("%s: hiu maped reg_base =0x%p, size=0x%x\n",
			__func__, hiu_reg_base, size_io_reg);
	} else {
		dev_err(&pdev->dev, "missing hiu memory resource\n");
		hiu_reg_base = NULL;
	}

	tvafe_user_parameters_config(pdev->dev.of_node);

	if ((tvafe_cpu_type() == TVAFE_CPU_TYPE_T5) ||
	    (tvafe_cpu_type() == TVAFE_CPU_TYPE_T5D) ||
	    (tvafe_cpu_type() == TVAFE_CPU_TYPE_T5W))
		sys_clk_reg_base = HHI_ANA_CLK_BASE;
	else if (tvafe_cpu_type() == TVAFE_CPU_TYPE_T3)
		sys_clk_reg_base = ATV_DMD_SYS_CLK_CNTL;
	ana_addr = ioremap_nocache(sys_clk_reg_base, 0x5);
	if (!ana_addr) {
		tvafe_pr_err("ana ioremap failure\n");
		return -ENOMEM;
	}

	/* frontend */
	tvin_frontend_init(&tdevp->frontend, &tvafe_dec_ops,
						&tvafe_sm_ops, tdevp->index);
	sprintf(tdevp->frontend.name, "%s", TVAFE_NAME);
	tvin_reg_frontend(&tdevp->frontend);

	mutex_init(&tdevp->afe_mutex);

	dev_set_drvdata(tdevp->dev, tdevp);
	platform_set_drvdata(pdev, tdevp);

	/**disable tvafe clock**/
	tdevp->flags |= TVAFE_POWERDOWN_IN_IDLE;
	tvafe_enable_module(false);

	/*init tvafe param*/
	tdevp->frame_skip_enable = 1;

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
	avport_opened = 0;
	av1_plugin_state = 0;
	av2_plugin_state = 0;
#endif

	tvafe_dev_local = tdevp;
	tdevp->sizeof_tvafe_dev_s = sizeof(struct tvafe_dev_s);

	disable_api = false;
	force_stable = false;
	tvafe_atv_search_channel = false;
	tvafe_manual_fmt_save = TVIN_SIG_FMT_NULL;

	tvafe_pr_info("driver probe ok\n");

	return 0;

fail_create_dbg_file:
	tvafe_delete_device(tdevp->index);
fail_create_device:
	cdev_del(&tdevp->cdev);
fail_add_cdev:
fail_get_id:
	kfree(tdevp);
fail_kzalloc_tdev:
	tvafe_pr_err("tvafe: kzalloc memory failed.\n");
	class_destroy(tvafe_clsp);
fail_class_create:
	unregister_chrdev_region(tvafe_devno, 1);
fail_alloc_cdev_region:
	return ret;
}

static int tvafe_drv_remove(struct platform_device *pdev)
{
	struct tvafe_dev_s *tdevp;

	tdevp = platform_get_drvdata(pdev);
	if (tvafe_reg_base) {
		devm_iounmap(&pdev->dev, tvafe_reg_base);
		devm_release_mem_region(&pdev->dev, tvafe_memobj.start,
			resource_size(&tvafe_memobj));
	}
	if (ana_addr)
		iounmap(ana_addr);

	mutex_destroy(&tdevp->afe_mutex);
	tvin_unreg_frontend(&tdevp->frontend);
	tvafe_remove_device_files(tdevp->dev);
	tvafe_delete_device(tdevp->index);
	cdev_del(&tdevp->cdev);
	kfree(tdevp);
	tvafe_dev_local = NULL;

	class_destroy(tvafe_clsp);
	unregister_chrdev_region(tvafe_devno, 1);

	tvafe_pr_info("driver removed ok.\n");
	return 0;
}

#ifdef CONFIG_PM
static int tvafe_drv_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct tvafe_dev_s *tdevp;
	struct tvafe_info_s *tvafe;

	tdevp = platform_get_drvdata(pdev);
	tvafe = &tdevp->tvafe;
	/* close afe port first */
	if (tdevp->flags & TVAFE_FLAG_DEV_OPENED) {
		tvafe_pr_info("suspend module, close afe port first\n");
		/* tdevp->flags &= (~TVAFE_FLAG_DEV_OPENED); */
		/*del_timer_sync(&tdevp->timer);*/

		/**set cvd2 reset to high**/
		tvafe_cvd2_hold_rst();
		/**disable av out**/
		tvafe_enable_avout(tvafe->parm.port, false);
	}
	/*disable and reset tvafe clock*/
	tdevp->flags |= TVAFE_POWERDOWN_IN_IDLE;
	tvafe_clk_status = false;
	tvafe_enable_module(false);
#ifdef CONFIG_AMLOGIC_MEDIA_ADC
	adc_set_pll_reset();
#endif

	tvafe_pr_info("suspend module\n");

	return 0;
}

static int tvafe_drv_resume(struct platform_device *pdev)
{
	struct tvafe_dev_s *tdevp;

	tdevp = platform_get_drvdata(pdev);
	/*disable and reset tvafe clock*/
#ifdef CONFIG_AMLOGIC_MEDIA_ADC
	adc_set_pll_reset();
#endif
	tvafe_enable_module(true);
	tdevp->flags &= (~TVAFE_POWERDOWN_IN_IDLE);
	tvafe_clk_status = false;
	tvafe_pr_info("resume module\n");
	return 0;
}
#endif

static void tvafe_drv_shutdown(struct platform_device *pdev)
{
	struct tvafe_dev_s *tdevp = NULL;
	struct tvafe_info_s *tvafe;

	if (tvafe_cpu_type() == TVAFE_CPU_TYPE_TL1) {
		W_APB_BIT(TVFE_VAFE_CTRL0, 0, 19, 1);
		W_APB_BIT(TVFE_VAFE_CTRL1, 0, 8, 1);
	}

	tdevp = platform_get_drvdata(pdev);
	if (!tdevp) {
		tvafe_pr_info("tdevp is null\n");
		return;
	}

	tvafe = &tdevp->tvafe;
	mutex_lock(&tdevp->afe_mutex);
	/* close afe port first */
	if (tdevp->flags & TVAFE_FLAG_DEV_OPENED) {
		tdevp->flags &= (~TVAFE_FLAG_DEV_OPENED);
		tdevp->flags |= TVAFE_POWERDOWN_IN_IDLE;
		tdevp->flags &= (~TVAFE_FLAG_DEV_STARTED);
		tvafe_clk_status = false;
		tvafe_pr_info("shutdown close afe port first\n");
		tvafe_cvd2_hold_rst();
		/**disable av out**/
		tvafe_enable_avout(tvafe->parm.port, false);
		/*disable and reset tvafe clock*/
		tvafe_enable_module(false);
	}
	mutex_unlock(&tdevp->afe_mutex);

	tvafe_pr_info("%s: tvafe shutdown ok.\n", __func__);
}

static struct platform_driver tvafe_driver = {
	.probe      = tvafe_drv_probe,
	.remove     = tvafe_drv_remove,
#ifdef CONFIG_PM
	.suspend    = tvafe_drv_suspend,
	.resume     = tvafe_drv_resume,
#endif
	.shutdown   = tvafe_drv_shutdown,
	.driver     = {
		.name   = TVAFE_DRIVER_NAME,
		.of_match_table = meson_tvafe_dt_match,
	}
};

int __init tvafe_drv_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&tvafe_driver);
	if (ret != 0) {
		tvafe_pr_err("%s: failed to register driver\n", __func__);
		return ret;
	}
	/*tvafe_pr_info("tvafe_drv_init.\n");*/
	return 0;
}

void __exit tvafe_drv_exit(void)
{
	platform_driver_unregister(&tvafe_driver);
	tvafe_pr_info("%s: tvafe exit.\n", __func__);
}

static int tvafe_mem_device_init(struct reserved_mem *rmem,
		struct device *dev)
{
	s32 ret = 0;
	struct resource *res = NULL;

	if (!rmem) {
		tvafe_pr_info("Can't get reverse mem!\n");
		ret = -EFAULT;
		return ret;
	}
	res = &tvafe_memobj;
	res->start = rmem->base;
	res->end = rmem->base + rmem->size - 1;
	if (rmem->size >= 0x500000)
		tvafe_use_reserved_mem = 1;

	tvafe_pr_info("init tvafe memsource 0x%lx->0x%lx tvafe_use_reserved_mem:%d\n",
		(unsigned long)res->start, (unsigned long)res->end,
		tvafe_use_reserved_mem);

	return 0;
}

static const struct reserved_mem_ops rmem_tvafe_ops = {
	.device_init = tvafe_mem_device_init,
};

static int __init tvafe_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_tvafe_ops;
	tvafe_pr_info("share mem setup\n");
	return 0;
}

RESERVEDMEM_OF_DECLARE(tvafe, "amlogic, tvafe_memory",
	tvafe_mem_setup);

//MODULE_VERSION(TVAFE_VER);
//MODULE_DESCRIPTION("AMLOGIC TVAFE driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Xu Lin <lin.xu@amlogic.com>");

