/*
 * TVAFE char device driver.
 *
 * Copyright (c) 2010 Bo Yang <bo.yang@amlogic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
/*#include <asm/uaccess.h>*/
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/amlogic/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
/* Amlogic headers */
#include <linux/amlogic/canvas/canvas.h>
/*#include <mach/am_regs.h>*/
#include <linux/amlogic/amports/vframe.h>

/* Local include */
#include <linux/amlogic/tvin/tvin.h>
#include "../tvin_frontend.h"
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "tvafe_regs.h"
#include "tvafe_adc.h"
#include "tvafe_cvd.h"
#include "tvafe_general.h"
#include "tvafe.h"
#include "../vdin/vdin_sm.h"


#define TVAFE_NAME               "tvafe"
#define TVAFE_DRIVER_NAME        "tvafe"
#define TVAFE_MODULE_NAME        "tvafe"
#define TVAFE_DEVICE_NAME        "tvafe"
#define TVAFE_CLASS_NAME         "tvafe"

static dev_t                     tvafe_devno;
static struct class              *tvafe_clsp;
struct mutex pll_mutex;

#define TVAFE_TIMER_INTERVAL    (HZ/100)   /* 10ms, #define HZ 100 */

static bool disableapi;
static bool force_stable;
static struct am_regs_s tvaferegs;
static struct tvafe_pin_mux_s tvafe_pinmux;

static bool enable_db_reg = true;
module_param(enable_db_reg, bool, 0644);
MODULE_PARM_DESC(enable_db_reg, "enable/disable tvafe load reg");

static int vga_yuv422_enable;
module_param(vga_yuv422_enable, int, 0664);
MODULE_PARM_DESC(vga_yuv422_enable, "vga_yuv422_enable");

static int cutwindow_val_v = TVAFE_VS_VE_VAL;
module_param(cutwindow_val_v, int, 0664);
MODULE_PARM_DESC(cutwindow_val_v, "cutwindow_val_v");

static int cutwindow_val_h_level1 = 6;
module_param(cutwindow_val_h_level1, int, 0664);
MODULE_PARM_DESC(cutwindow_val_h_level1, "cutwindow_val_h_level1");

static int cutwindow_val_h_level2 = 8;
module_param(cutwindow_val_h_level2, int, 0664);
MODULE_PARM_DESC(cutwindow_val_h_level2, "cutwindow_val_h_level2");

static int cutwindow_val_h_level3 = 16;
module_param(cutwindow_val_h_level3, int, 0664);
MODULE_PARM_DESC(cutwindow_val_h_level3, "cutwindow_val_h_level3");

static int cutwindow_val_h_level4 = 18;
module_param(cutwindow_val_h_level4, int, 0664);
MODULE_PARM_DESC(cutwindow_val_h_level4, "cutwindow_val_h_level4");

bool tvafe_dbg_enable;
module_param(tvafe_dbg_enable, bool, 0644);
MODULE_PARM_DESC(tvafe_dbg_enable, "enable/disable tvafe debug enable");

/*1: snow function on;
0: off snow function*/
bool tvafe_snow_function_flag;

static unsigned int tvafe_ratio_cnt = 50;
module_param(tvafe_ratio_cnt, uint, 0644);
MODULE_PARM_DESC(tvafe_ratio_cnt, "tvafe aspect ratio valid cnt");

static struct tvafe_info_s *g_tvafe_info;

/***********the  version of changing log************************/
static const char last_version_s[] = "2013-11-29||11-28";
static const char version_s[] = "2016-12-27||17-23";
/***************************************************************************/
void get_afe_version(const char **ver, const char **last_ver)
{
	*ver = version_s;
	*last_ver = last_version_s;
	return;
}

static void tvafe_state(struct tvafe_dev_s *devp)
{
	struct tvin_parm_s *parm = &devp->tvafe.parm;
	struct tvafe_cvd2_s *cvd2 = &devp->tvafe.cvd2;
	struct tvin_info_s *tvin_info = &parm->info;
	struct tvafe_cvd2_info_s *cvd2_info = &cvd2->info;
	struct tvafe_cvd2_lines_s *vlines = &cvd2_info->vlines;
	struct tvafe_cvd2_hw_data_s *hw = &cvd2->hw;
	/* top dev info */
	pr_info("\n!!tvafe_dev_s info:\n");
	pr_info("[tvafe..]tvafe_dev_s->index:%d\n", devp->index);
	pr_info("[tvafe..]tvafe_dev_s->flags:0x%x\n", devp->flags);
	pr_info("[tvafe..]tvafe_dev_s->cma_config_en:%d\n",
		devp->cma_config_en);
	pr_info("[tvafe..]tvafe_dev_s->cma_config_flag:0x%x\n",
		devp->cma_config_flag);
	#ifdef CONFIG_CMA
	pr_info("[tvafe..]devp->cma_mem_size:0x%x\n", devp->cma_mem_size);
	pr_info("[tvafe..]devp->cma_mem_alloc:%d\n", devp->cma_mem_alloc);
	#endif
	pr_info("[tvafe..]tvafe_info_s->aspect_ratio:%d\n",
		devp->tvafe.aspect_ratio);
	pr_info("[tvafe..]tvafe_info_s->aspect_ratio_last:%d\n",
		devp->tvafe.aspect_ratio_last);
	pr_info("[tvafe..]tvafe_info_s->aspect_ratio_cnt:%d\n",
		devp->tvafe.aspect_ratio_cnt);
	/* tvafe_dev_s->tvin_parm_s struct info */
	pr_info("\n!!tvafe_dev_s->tvin_parm_s struct info:\n");
	pr_info("[tvafe..]tvin_parm_s->index:%d\n", parm->index);
	pr_info("[tvafe..]tvin_parm_s->port:0x%x\n", parm->port);
	pr_info("[tvafe..]tvin_parm_s->hist_pow:0x%x\n", parm->hist_pow);
	pr_info("[tvafe..]tvin_parm_s->luma_sum:0x%x\n", parm->luma_sum);
	pr_info("[tvafe..]tvin_parm_s->pixel_sum:0x%x\n", parm->pixel_sum);
	pr_info("[tvafe..]tvin_parm_s->flag:0x%x\n", parm->flag);
	pr_info("[tvafe..]tvin_parm_s->dest_width:0x%x\n", parm->dest_width);
	pr_info("[tvafe..]tvin_parm_s->dest_height:0x%x\n", parm->dest_height);
	pr_info("[tvafe..]tvin_parm_s->h_reverse:%d\n", parm->h_reverse);
	pr_info("[tvafe..]tvin_parm_s->v_reverse:%d\n", parm->v_reverse);
	/* tvafe_dev_s->tvafe_cvd2_s struct info */
	pr_info("\n!!tvafe_dev_s->tvafe_cvd2_s struct info:\n");
	pr_info("[tvafe..]tvafe_cvd2_s->config_fmt:0x%x\n", cvd2->config_fmt);
	pr_info("[tvafe..]tvafe_cvd2_s->manual_fmt:0x%x\n", cvd2->manual_fmt);
	pr_info("[tvafe..]tvafe_cvd2_s->vd_port:0x%x\n", cvd2->vd_port);
	pr_info("[tvafe..]tvafe_cvd2_s->cvd2_init_en:%d\n", cvd2->cvd2_init_en);
	/* tvin_parm_s->tvin_info_s struct info */
	pr_info("\n!!tvin_parm_s->tvin_info_s struct info:\n");
	pr_info("[tvafe..]tvin_info_s->trans_fmt:0x%x\n", tvin_info->trans_fmt);
	pr_info("[tvafe..]tvin_info_s->fmt:0x%x\n", tvin_info->fmt);
	pr_info("[tvafe..]tvin_info_s->status:0x%x\n", tvin_info->status);
	pr_info("[tvafe..]tvin_info_s->cfmt:%d\n", tvin_info->cfmt);
	pr_info("[tvafe..]tvin_info_s->fps:%d\n", tvin_info->fps);
	/* tvafe_cvd2_s->tvafe_cvd2_info_s struct info */
	pr_info("\n!!tvafe_cvd2_s->tvafe_cvd2_info_s struct info:\n");
	pr_info("[tvafe..]tvafe_cvd2_info_s->state:0x%x\n", cvd2_info->state);
	pr_info("[tvafe..]tvafe_cvd2_info_s->state_cnt:%d\n",
		cvd2_info->state_cnt);
	pr_info("[tvafe..]tvafe_cvd2_info_s->non_std_enable:%d\n",
		cvd2_info->non_std_enable);
	pr_info("[tvafe..]tvafe_cvd2_info_s->non_std_config:%d\n",
		cvd2_info->non_std_config);
	pr_info("[tvafe..]tvafe_cvd2_info_s->non_std_worst:%d\n",
		cvd2_info->non_std_worst);
	pr_info("[tvafe..]tvafe_cvd2_info_s->adc_reload_en:%d\n",
		cvd2_info->adc_reload_en);
	pr_info("[tvafe..]tvafe_cvd2_info_s->hs_adj_en:%d\n",
		cvd2_info->hs_adj_en);
	pr_info("[tvafe..]tvafe_cvd2_info_s->hs_adj_dir:%d\n",
		cvd2_info->hs_adj_dir);
	pr_info("[tvafe..]tvafe_cvd2_info_s->hs_adj_level:%d\n",
		cvd2_info->hs_adj_level);
#ifdef TVAFE_SET_CVBS_CDTO_EN
	pr_info("[tvafe..]tvafe_cvd2_info_s->hcnt64[0]:0x%x\n",
		cvd2_info->hcnt64[0]);
	pr_info("[tvafe..]tvafe_cvd2_info_s->hcnt64[1]:0x%x\n",
		cvd2_info->hcnt64[1]);
	pr_info("[tvafe..]tvafe_cvd2_info_s->hcnt64[2]:0x%x\n",
		cvd2_info->hcnt64[2]);
	pr_info("[tvafe..]tvafe_cvd2_info_s->hcnt64[3]:0x%x\n",
		cvd2_info->hcnt64[3]);
#endif
	/* tvafe_cvd2_info_s->tvafe_cvd2_lines_s struct info */
	pr_info("\n!!tvafe_cvd2_info_s->tvafe_cvd2_lines_s struct info:\n");
	pr_info("[tvafe..]tvafe_cvd2_lines_s->check_cnt:0x%x\n",
		vlines->check_cnt);
	pr_info("[tvafe..]tvafe_cvd2_lines_s->de_offset:0x%x\n",
		vlines->de_offset);
	pr_info("[tvafe..]tvafe_cvd2_lines_s->val[0]:0x%x\n",
		vlines->val[0]);
	pr_info("[tvafe..]tvafe_cvd2_lines_s->val[1]:0x%x\n",
		vlines->val[1]);
	pr_info("[tvafe..]tvafe_cvd2_lines_s->val[2]:0x%x\n",
		vlines->val[2]);
	pr_info("[tvafe..]tvafe_cvd2_lines_s->val[3]:0x%x\n",
		vlines->val[3]);
	/* tvafe_cvd2_s->tvafe_cvd2_hw_data_s struct info */
	pr_info("\n!!tvafe_cvd2_s->tvafe_cvd2_hw_data_s struct info:\n");
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->no_sig:%d\n", hw->no_sig);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->h_lock:%d\n", hw->h_lock);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->v_lock:%d\n", hw->v_lock);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->h_nonstd:%d\n", hw->h_nonstd);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->v_nonstd:%d\n", hw->v_nonstd);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->no_color_burst:%d\n",
		hw->no_color_burst);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->comb3d_off:%d\n",
		hw->comb3d_off);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->chroma_lock:%d\n",
		hw->chroma_lock);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->pal:%d\n", hw->pal);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->secam:%d\n", hw->secam);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->line625:%d\n", hw->line625);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->noisy:%d\n", hw->noisy);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->vcr:%d\n", hw->vcr);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->vcrtrick:%d\n", hw->vcrtrick);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->vcrff:%d\n", hw->vcrff);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->vcrrew:%d\n", hw->vcrrew);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->noisy:%d\n", hw->noisy);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->acc4xx_cnt:%d\n",
		hw->acc4xx_cnt);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->acc425_cnt:%d\n",
		hw->acc425_cnt);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->acc3xx_cnt:%d\n",
		hw->acc3xx_cnt);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->acc358_cnt:%d\n",
		hw->acc358_cnt);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->secam_detected:%d\n",
		hw->secam_detected);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->secam_phase:%d\n",
		hw->secam_phase);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->fsc_358:%d\n", hw->fsc_358);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->fsc_425:%d\n", hw->fsc_425);
	pr_info("[tvafe..]tvafe_cvd2_hw_data_s->fsc_443:%d\n", hw->fsc_443);
}

/*default only one tvafe ,echo cvdfmt pali/palm/ntsc/secam >dir*/
static ssize_t tvafe_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t count)
{
	unsigned char fmt_index = 0;

	struct tvafe_dev_s *devp;
	unsigned long tmp = 0;
	devp = dev_get_drvdata(dev);

	if (!strncmp(buff, "cvdfmt", strlen("cvdfmt"))) {

		fmt_index = strlen("cvdfmt") + 1;
		if (!strncmp(buff+fmt_index, "ntscm", strlen("ntscm")))
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_NTSC_M;
		else if (!strncmp(buff+fmt_index,
					"ntsc443", strlen("ntsc443")))
			devp->tvafe.cvd2.manual_fmt =
					TVIN_SIG_FMT_CVBS_NTSC_443;
		else if (!strncmp(buff+fmt_index, "pali", strlen("pali")))
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_PAL_I;
		else if (!strncmp(buff+fmt_index, "palm", strlen("plam")))
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_PAL_M;
		else if (!strncmp(buff+fmt_index, "pal60", strlen("pal60")))
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_PAL_60;
		else if (!strncmp(buff+fmt_index, "palcn", strlen("palcn")))
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_PAL_CN;
		else if (!strncmp(buff+fmt_index, "secam", strlen("secam")))
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_SECAM;
		else if (!strncmp(buff+fmt_index, "null", strlen("null")))
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_NULL;
		else
			pr_info("%s:invaild command.", buff);

	} else if (!strncmp(buff, "disableapi", strlen("disableapi"))) {
		if (kstrtoul(buff+strlen("disableapi")+1, 10, &tmp) == 0)
			disableapi = tmp;

	} else if (!strncmp(buff, "force_stable", strlen("force_stable"))) {
		if (kstrtoul(buff+strlen("force_stable")+1, 10, &tmp) == 0)
			force_stable = tmp;
#if 0
	} else if (!strncmp(buff, "cphasepr", strlen("cphasepr")))
		tvafe_adc_comphase_pr();
	} else if (!strncmp(buff, "vdin_bbld", strlen("vdin_bbld"))) {
		tvin_vdin_bbar_init(devp->tvafe.parm.info.fmt);
		devp->tvafe.adc.vga_auto.phase_state = VGA_VDIN_BORDER_DET;
#endif
	} else if (!strncmp(buff, "pdown", strlen("pdown"))) {
		devp->flags |= TVAFE_POWERDOWN_IN_IDLE;
		tvafe_enable_module(false);
#if 0
	} else if (!strncmp(buff, "vga_edid", strlen("vga_edid"))) {
		struct tvafe_vga_edid_s edid;
		int i = 0;
		tvafe_vga_get_edid(&edid);
		for (i = 0; i < 32; i++) {

			pr_info("0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x.\n",
				edid.value[(i<<3)+0], edid.value[(i<<3)+1],
				edid.value[(i<<3)+2], edid.value[(i<<3)+3],
				edid.value[(i<<3)+4], edid.value[(i<<3)+5],
				edid.value[(i<<3)+6], edid.value[(i<<3)+7]);
		}

	} else if (!strncmp(buff, "set_edid", strlen("set_edid"))) {
		int i = 0;
		struct tvafe_vga_edid_s edid = {
				{0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
				0x30, 0xa4, 0x21, 0x00, 0x01, 0x01, 0x01, 0x01,
				0x0e, 0x17, 0x01, 0x03, 0x80, 0x24, 0x1d, 0x78,
				0xee, 0x00, 0x0c, 0x0a, 0x05, 0x04, 0x09, 0x02,
				0x00, 0x20, 0x80, 0xa1, 0x08, 0x70, 0x01, 0x01,
				0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
				0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3a,
				0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
				0x45, 0x00, 0xa0, 0x5a, 0x00, 0x00, 0x00, 0x1e,
				0x00, 0x00, 0x00, 0xfd, 0x00, 0x3b, 0x3c, 0x1f,
				0x2d, 0x08, 0x00, 0x0a, 0x20, 0x20, 0x20, 0x20,
				0x20, 0x20, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x44,
				0x35, 0x30, 0x4c, 0x57, 0x37, 0x31, 0x30, 0x30,
				0x0a, 0x20, 0x20, 0x20, 0x21, 0x39, 0x90, 0x30,
				0x62, 0x1a, 0x27, 0x40, 0x68, 0xb0, 0x36, 0x00,
				0xa0, 0x64, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x88,
				0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
				0x30, 0xa4, 0x21, 0x00, 0x01, 0x01, 0x01, 0x01,
				0x0e, 0x17, 0x01, 0x03, 0x80, 0x24, 0x1d, 0x78,
				0xee, 0x00, 0x0c, 0x0a, 0x05, 0x04, 0x09, 0x02,
				0x00, 0x20, 0x80, 0xa1, 0x08, 0x70, 0x01, 0x01,
				0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
				0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3a,
				0x80, 0x18, 0x71, 0x38, 0x2d, 0x40, 0x58, 0x2c,
				0x45, 0x00, 0xa0, 0x5a, 0x00, 0x00, 0x00, 0x1e,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
								}
									};

		for (i = 0; i < 32; i++) {

			pr_info("0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x.\n",
			edid.value[(i<<3)+0] , edid.value[(i<<3)+1],
			edid.value[(i<<3)+2] , edid.value[(i<<3)+3],
			edid.value[(i<<3)+4] , edid.value[(i<<3)+5],
			edid.value[(i<<3)+6] , edid.value[(i<<3)+7]);
		}

		tvafe_vga_set_edid(&edid);
#endif
	} else if (!strncmp(buff, "tvafe_enable", strlen("tvafe_enable"))) {
		tvafe_enable_module(true);
		devp->flags &= (~TVAFE_POWERDOWN_IN_IDLE);
		pr_info("[tvafe..]%s:tvafe enable\n", __func__);
	} else if (!strncmp(buff, "tvafe_down", strlen("tvafe_down"))) {
		devp->flags |= TVAFE_POWERDOWN_IN_IDLE;
		tvafe_enable_module(false);
		pr_info("[tvafe..]%s:tvafe down\n", __func__);
	} else if (!strncmp(buff, "afe_ver", strlen("afe_ver"))) {
		const char *afe_version = NULL, *last_afe_version = NULL;
		const char *adc_version = NULL, *last_adc_version = NULL;
		const char *cvd_version = NULL, *last_cvd_version = NULL;
		get_afe_version(&afe_version, &last_afe_version);
		get_adc_version(&adc_version, &last_adc_version);
		get_cvd_version(&cvd_version, &last_cvd_version);

		pr_info("NEW VERSION :[tvafe version]:%s\t[cvd2 version]:%s\t[adc version]:%s\t[format table version]:NUll\n",
		afe_version, cvd_version, adc_version);
		pr_info("LAST VERSION:[tvafe version]:%s\t[cvd2 version]:%s\t[adc version]:%s\t[format table version]:NUll\n",
		last_afe_version, last_cvd_version, last_adc_version);
	} else if (!strncmp(buff, "snowon", strlen("snowon"))) {
		tvafe_snow_config(1);
		tvafe_snow_config_clamp(1);
		devp->flags |= TVAFE_FLAG_DEV_SNOW_FLAG;
		tvafe_snow_function_flag = true;
		pr_info("[tvafe..]%s:tvafe snowon\n", __func__);
	} else if (!strncmp(buff, "snowoff", strlen("snowoff"))) {
		tvafe_snow_config(0);
		tvafe_snow_config_clamp(0);
		devp->flags &= (~TVAFE_FLAG_DEV_SNOW_FLAG);
		pr_info("[tvafe..]%s:tvafe snowoff\n", __func__);
	} else if (!strncmp(buff, "state", strlen("state"))) {
		tvafe_state(devp);
	} else
		pr_info("[%s]:invaild command.\n", __func__);
	return count;
}

/*************************************************
before to excute the func of dumpmem_store ,it must be setting the steps below:
echo wa 0x30b2 4c > /sys/class/register/reg
echo wa 0x3122 3000000 > /sys/class/register/reg
echo wa 0x3096 0 > /sys/class/register/reg
echo wa 0x3150 2 > /sys/class/register/reg

echo wa 0x3151 11b40000 > /sys/class/register/reg   //start mem adr
echo wa 0x3152 11bf61f0 > /sys/class/register/reg	//end mem adr
echo wa 0x3150 3 > /sys/class/register/reg
echo wa 0x3150 1 > /sys/class/register/reg

the steps above set  the cvd that  will  write the signal data to mem
*************************************************/
static ssize_t dumpmem_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t len)
{
	unsigned int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[6] = {NULL};
	struct tvafe_dev_s *devp;
	char delim1[2] = " ";
	char delim2[2] = "\n";
	strcat(delim1, delim2);
	if (!buff)
		return len;
	buf_orig = kstrdup(buff, GFP_KERNEL);
	/* printk(KERN_INFO "input cmd : %s",buf_orig); */
	devp = dev_get_drvdata(dev);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, delim1);
		if (token == NULL)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
		}
	if (!strncmp(parm[0], "dumpmem", strlen("dumpmem"))) {
		if (parm[1] != NULL) {
			struct file *filp = NULL;
			loff_t pos = 0;
			void *buf = NULL;
/* unsigned int canvas_real_size = devp->canvas_h * devp->canvas_w; */
			mm_segment_t old_fs = get_fs();
			set_fs(KERNEL_DS);
			filp = filp_open(parm[1], O_RDWR|O_CREAT, 0666);
			if (IS_ERR(filp)) {
				pr_err(KERN_ERR"create %s error.\n",
					parm[1]);
				return len;
			}
			if (devp->cma_config_flag == 1)
				buf = codec_mm_phys_to_virt(devp->mem.start);
			else
				buf = phys_to_virt(devp->mem.start);
			vfs_write(filp, buf, devp->mem.size, &pos);
			pr_info("write buffer %2d of %s.\n",
				devp->mem.size, parm[1]);
			pr_info("devp->mem.start   %x .\n",
				devp->mem.start);
			vfs_fsync(filp, 0);
			filp_close(filp, NULL);
			set_fs(old_fs);
		}
	}
	return len;

}


static ssize_t tvafe_show(struct device *dev,
		struct device_attribute *attr, char *buff)
{
	ssize_t len = 0;

	struct tvafe_dev_s *devp;
	devp = dev_get_drvdata(dev);
		switch (devp->tvafe.cvd2.manual_fmt) {

		case TVIN_SIG_FMT_CVBS_NTSC_M:
			len = sprintf(buff, "cvdfmt:%s.\n", "ntscm");
			break;
		case TVIN_SIG_FMT_CVBS_NTSC_443:
			len = sprintf(buff, "cvdfmt:%s.\n", "ntsc443");
			break;
		case TVIN_SIG_FMT_CVBS_PAL_I:
			len = sprintf(buff, "cvdfmt:%s.\n", "pali");
			break;
		case TVIN_SIG_FMT_CVBS_PAL_M:
			len = sprintf(buff, "cvdfmt:%s.\n", "palm");
			break;
		case TVIN_SIG_FMT_CVBS_PAL_60:
			len = sprintf(buff, "cvdfmt:%s.\n", "pal60");
			break;
		case TVIN_SIG_FMT_CVBS_PAL_CN:
			len = sprintf(buff, "cvdfmt:%s.\n", "palcn");
			break;
		case TVIN_SIG_FMT_CVBS_SECAM:
			len = sprintf(buff, "cvdfmt:%s.\n", "secam");
			break;
		case TVIN_SIG_FMT_NULL:
			len = sprintf(buff, "cvdfmt:%s.\n", "auto");
		default:
			len = sprintf(buff, "cvdfmt:%s.\n", "invaild command");
			break;

	}
	if (disableapi)
		pr_info("[%s]:diableapi!!!.\n", __func__);
	return len;
}
static DEVICE_ATTR(debug, 0644, tvafe_show, tvafe_store);
static DEVICE_ATTR(dumpmem, 0200, NULL, dumpmem_store);



/*
* echo n >/sys/class/tvafe/tvafe0/cvd_reg8a set register of cvd2
*/
static ssize_t cvd_reg8a_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t count)
{
	unsigned int n;
	struct tvafe_dev_s *devp;
	devp = dev_get_drvdata(dev);

	if (sscanf(buff, "%u", &n) != 1)
		return -1;
	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {

		pr_err("[tvafe..] tvafe havn't opened, error!!!\n");
		return count;
	} else
		tvafe_cvd2_set_reg8a(n);
	pr_info("[tvafe..] set register of cvd 0x8a to %u.\n", n);
	return count;
}
static DEVICE_ATTR(cvd_reg8a, 0200, NULL, cvd_reg8a_store);

static ssize_t reg_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t count)
{
	struct tvafe_dev_s *devp;
	unsigned int argn = 0, addr = 0, value = 0, end = 0;
	char *p, *para, *buf_work, cmd = 0;
	char *argv[3];
	long tmp = 0;

	devp = dev_get_drvdata(dev);

	buf_work = kstrdup(buff, GFP_KERNEL);
	p = buf_work;

	for (argn = 0; argn < 3; argn++) {
		para = strsep(&p, " ");
		if (para == NULL)
			break;
		argv[argn] = para;
	}

	if (argn < 1 || argn > 3)
		return count;

	cmd = argv[0][0];

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {

		pr_err("[tvafe..] tvafe havn't opened, error!!!\n");
		return count;
	}
		switch (cmd) {
		case 'r':
		case 'R':
			if (argn < 2) {
				pr_err("syntax error.\n");
			} else{
				if (kstrtol(argv[1], 16, &tmp) == 0)
					addr = tmp;
				else
					break;
				value = R_APB_REG(addr<<2);
				pr_info("APB[0x%04x]=0x%08x\n", addr, value);
			}
			break;
		case 'w':
		case 'W':
			if (argn < 3) {
				pr_err("syntax error.\n");
			} else{
				if (kstrtol(argv[1], 16, &tmp) == 0)
					value = tmp;
				else
					break;
				if (kstrtol(argv[2], 16, &tmp) == 0)
					addr = tmp;
				else
					break;
				W_APB_REG(addr<<2, value);
				pr_info("Write APB[0x%04x]=0x%08x\n", addr,
						R_APB_REG(addr<<2));
			}
			break;
		case 'd':
		/*case 'D': */
			if (argn < 3) {
				pr_err("syntax error.\n");
			} else{
				if (kstrtol(argv[1], 16, &tmp) == 0)
					addr = tmp;
				else
					break;
				if (kstrtol(argv[2], 16, &tmp) == 0)
					end = tmp;
				else
					break;
				for (; addr <= end; addr++)
					pr_info("APB[0x%04x]=0x%08x\n", addr,
						R_APB_REG(addr<<2));
			}
			break;
		case 'D':
			/* if (argn < 3) {
			*	pr_err("syntax error.\n");
			* } else{
			*/
			pr_info("dump TOP reg----\n");
			for (addr = TOP_BASE_ADD;
				addr <= (TOP_BASE_ADD+0xb2); addr++)
				pr_info("[0x%x]APB[0x%04x]=0x%08x\n",
						(0XC8842000+(addr<<2)), addr,
						R_APB_REG(addr<<2));
			pr_info("dump ACD reg----\n");
			for (addr = ACD_BASE_ADD;
				addr <= (ACD_BASE_ADD+0xA4); addr++)
				pr_info("[0x%x]APB[0x%04x]=0x%08x\n",
						(0XC8842000+(addr<<2)), addr,
						R_APB_REG(addr<<2));
			pr_info("dump CVD2 reg----\n");
			for (addr = CVD_BASE_ADD;
				addr <= (CVD_BASE_ADD+0xf9); addr++)
				pr_info("[0x%x]APB[0x%04x]=0x%08x\n",
						(0XC8842000+(addr<<2)), addr,
						R_APB_REG(addr<<2));
			pr_info("dump reg done----\n");
			/* } */
			break;
		default:
			pr_err("not support.\n");
			break;
	}
	return count;
}


static ssize_t reg_show(struct device *dev,
		struct device_attribute *attr, char *buff)
{
	ssize_t len = 0;

	len += sprintf(buff+len, "Usage:\n");
	len += sprintf(buff+len,
		"\techo [read|write <date>] addr > reg;Access ATV DEMON/TVAFE/HDMIRX logic address.\n");
	len += sprintf(buff+len,
		"\techo dump <start> <end> > reg;Dump ATV DEMON/TVAFE/HDMIRX logic address.\n");
	len += sprintf(buff+len,
		"Address format:\n");
	len += sprintf(buff+len,
		"\taddr    : 0xXXXX, 16 bits register address\n");
	return len;
}
static DEVICE_ATTR(reg, 0644, reg_show, reg_store);
#if 0
/*
 * tvafe 10ms timer handler
 */
void tvafe_timer_handler(unsigned long arg)
{
	struct tvafe_dev_s *devp = (struct tvafe_dev_s *)arg;
	struct tvafe_info_s *tvafe = &devp->tvafe;

	tvafe_vga_auto_adjust_handler(&tvafe->parm, &tvafe->adc);

	devp->timer.expires = jiffies + TVAFE_TIMER_INTERVAL;
	add_timer(&devp->timer);
}
/*
func:come true the callmaster
return:false-------no_sig
	   true-------sig coming
param:void
*/

static int tvafe_callmaster_det(enum tvin_port_e port,
					struct tvin_frontend_s *fe)
{
	int ret = 0;
	struct tvafe_pin_mux_s *pinmux = NULL;
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
					frontend);
	if (!devp || !(devp->pinmux)) {
		pr_err("[tvafe]%s:devp/pinmux is NULL\n", __func__);
		return -1;
	}
	pinmux = devp->pinmux;
	if (get_cpu_type() >= MESON_CPU_TYPE_MESONG9TV)
		return -1;
		switch (port) {

		case TVIN_PORT_VGA0:
			if (pinmux->pin[VGA0_SOG] >= TVAFE_ADC_PIN_SOG_0) {
				if (ret == (int)R_APB_BIT(ADC_REG_34, 4, 1))
					ret = (int)R_APB_BIT(ADC_REG_34, 7, 1);
				else
					ret = 0;
			}
				/* ret=(int)R_APB_BIT(ADC_REG_2B,
				(pinmux->pin[VGA0_SOG] -
				TVAFE_ADC_PIN_SOG_0),1); */
			break;
		case TVIN_PORT_COMP0:
			if (pinmux->pin[COMP0_SOG] >= TVAFE_ADC_PIN_SOG_0)
				ret = (int)R_APB_BIT(ADC_REG_2B,
				(pinmux->pin[COMP0_SOG] - TVAFE_ADC_PIN_SOG_0),
				1);
			break;
		case TVIN_PORT_CVBS0:
			if (pinmux->pin[CVBS0_SOG] >= TVAFE_ADC_PIN_SOG_0)
				ret = (int)R_APB_BIT(ADC_REG_2B,
				(pinmux->pin[CVBS0_SOG] - TVAFE_ADC_PIN_SOG_0),
				1);
			break;
		case TVIN_PORT_CVBS1:
			if (pinmux->pin[CVBS1_SOG] >= TVAFE_ADC_PIN_SOG_0)
				ret = (int)R_APB_BIT(ADC_REG_2B,
				(pinmux->pin[CVBS1_SOG] - TVAFE_ADC_PIN_SOG_0),
				1);
			break;
		case TVIN_PORT_CVBS2:
			if (pinmux->pin[CVBS2_SOG] >= TVAFE_ADC_PIN_SOG_0)
				ret = (int)R_APB_BIT(ADC_REG_2B,
				(pinmux->pin[CVBS2_SOG] - TVAFE_ADC_PIN_SOG_0),
				1);
			break;
		case TVIN_PORT_HDMI0:
			break;
		case TVIN_PORT_HDMI1:
			break;
		case TVIN_PORT_HDMI2:
			break;
		case TVIN_PORT_MAX:
			break;
		default:
			break;
	}
	/* ret=(bool)R_APB_BIT(ADC_REG_34,1,1); */
	/* printk("[%s]:port==%s,ret=%d\n",__func__,tvin_port_str(port),ret); */
	return ret;
}
#endif

/*
 * tvafe check support port
 */
int tvafe_dec_support(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct tvafe_dev_s *devp = container_of(fe,
				struct tvafe_dev_s, frontend);

	/* check afe port and index */
	if (((port < TVIN_PORT_VGA0) || (port > TVIN_PORT_SVIDEO7)) ||
		(fe->index != devp->index))
		return -1;

	return 0;
}

#ifdef CONFIG_CMA
void tvafe_cma_alloc(struct tvafe_dev_s *devp)
{
	unsigned int mem_size = devp->cma_mem_size;
	int flags = CODEC_MM_FLAGS_CMA_FIRST|CODEC_MM_FLAGS_CMA_CLEAR|
		CODEC_MM_FLAGS_CPU;
	if (devp->cma_config_en == 0)
		return;
	if (devp->cma_mem_alloc == 1)
		return;
	devp->cma_mem_alloc = 1;
	if (devp->cma_config_flag == 1) {
		devp->mem.start = codec_mm_alloc_for_dma("tvafe",
			mem_size/PAGE_SIZE, 0, flags);
		devp->mem.size = mem_size;
		if (devp->mem.start == 0)
			pr_err(KERN_ERR "\ntvafe codec alloc fail!!!\n");
		else {
			pr_info("tvafe mem_start = 0x%x, mem_size = 0x%x\n",
				devp->mem.start, devp->mem.size);
			pr_info("tvafe codec cma alloc ok!\n");
		}
	} else if (devp->cma_config_flag == 0) {
		devp->venc_pages =
			dma_alloc_from_contiguous(&(devp->this_pdev->dev),
			mem_size >> PAGE_SHIFT, 0);
		if (devp->venc_pages) {
			devp->mem.start = page_to_phys(devp->venc_pages);
			devp->mem.size  = mem_size;
			pr_info("tvafe mem_start = 0x%x, mem_size = 0x%x\n",
				devp->mem.start, devp->mem.size);
			pr_info("tvafe cma alloc ok!\n");
		} else {
			pr_err("\ntvafe cma mem undefined2.\n");
		}
	}
}

void tvafe_cma_release(struct tvafe_dev_s *devp)
{
	if (devp->cma_config_en == 0)
		return;
	if ((devp->cma_config_flag == 1) && devp->mem.start
		&& (devp->cma_mem_alloc == 1)) {
		codec_mm_free_for_dma("tvafe", devp->mem.start);
		devp->mem.start = 0;
		devp->mem.size = 0;
		devp->cma_mem_alloc = 0;
		pr_info("tvafe codec cma release ok!\n");
	} else if (devp->venc_pages
		&& devp->cma_mem_size
		&& (devp->cma_mem_alloc == 1)
		&& (devp->cma_config_flag == 0)) {
		devp->cma_mem_alloc = 0;
		dma_release_from_contiguous(&(devp->this_pdev->dev),
			devp->venc_pages,
			devp->cma_mem_size>>PAGE_SHIFT);
		pr_info("tvafe cma release ok!\n");
	}
}
#endif

static int tvafe_get_v_fmt(void)
{
	int fmt = 0;
	if (TVIN_SM_STATUS_STABLE != tvin_get_sm_status(0)) {
		pr_info("%s tvafe is not STABLE\n", __func__);
		return 0;
	}
	fmt = tvafe_cvd2_get_format(&g_tvafe_info->cvd2);
	return fmt;
}

/*
 * tvafe open port and init register
 */
int tvafe_dec_open(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;

	mutex_lock(&devp->afe_mutex);
	if (devp->flags & TVAFE_FLAG_DEV_OPENED) {

		pr_err("[tvafe..] %s(%d), %s opened already\n", __func__,
				devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return 1;
	}
	if ((port < TVIN_PORT_CVBS0) || (port > TVIN_PORT_CVBS7)) {

		pr_err("[tvafe..] %s(%d), %s unsupport\n", __func__,
				devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return 1;
	}
#ifdef CONFIG_CMA
	tvafe_cma_alloc(devp);
#endif
	/* init variable */
	memset(tvafe, 0, sizeof(struct tvafe_info_s));
	/**enable and reset tvafe clock**/
	tvafe_enable_module(true);
	devp->flags &= (~TVAFE_POWERDOWN_IN_IDLE);

	/* set APB bus register accessing error exception */
	tvafe_set_apb_bus_err_ctrl();
	/**set cvd2 reset to high**/
	/*tvafe_cvd2_hold_rst(&tvafe->cvd2);?????*/

	/* init tvafe registers */
	tvafe_init_reg(&tvafe->cvd2, &devp->mem, port, devp->pinmux);

	tvafe->parm.port = port;
#if 0
	/* timer */
	init_timer(&devp->timer);
	devp->timer.data = (ulong)devp;
	devp->timer.function = tvafe_timer_handler;
	devp->timer.expires = jiffies + (TVAFE_TIMER_INTERVAL);
	add_timer(&devp->timer);
#endif
	/* set the flag to enabble ioctl access */
	devp->flags |= TVAFE_FLAG_DEV_OPENED;
#ifdef CONFIG_AM_DVB
	g_tvafe_info = tvafe;
	/* register aml_fe hook for atv search */
	aml_fe_hook_cvd(tvafe_cvd2_get_atv_format, tvafe_cvd2_get_hv_lock,
		tvafe_get_v_fmt);
#endif
	pr_info("[tvafe..] %s open port:0x%x ok.\n", __func__, port);

	mutex_unlock(&devp->afe_mutex);
	return 0;
}

/*
 * tvafe start after signal stable
 */
void tvafe_dec_start(struct tvin_frontend_s *fe, enum tvin_sig_fmt_e fmt)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = devp->tvafe.parm.port;

	mutex_lock(&devp->afe_mutex);
	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {

		pr_err("[tvafe..] tvafe_dec_start(%d) decode havn't opened\n",
			devp->index);
		mutex_unlock(&devp->afe_mutex);
		return;
	}
	if ((port < TVIN_PORT_CVBS0) || (port > TVIN_PORT_CVBS7)) {

		pr_err("[tvafe..] %s(%d), %s unsupport\n", __func__,
				devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return;
	}

	if (devp->flags & TVAFE_FLAG_DEV_STARTED) {

		pr_err("[tvafe..] %s(%d), %s started already\n", __func__,
				devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return;
	}

	tvafe->parm.info.fmt = fmt;
	tvafe->parm.info.status = TVIN_SIG_STATUS_STABLE;

	devp->flags |= TVAFE_FLAG_DEV_STARTED;

	pr_info("[tvafe..] %s start fmt:%s ok.\n",
			__func__, tvin_sig_fmt_str(fmt));

	mutex_unlock(&devp->afe_mutex);
}

/*
 * tvafe stop port
 */
void tvafe_dec_stop(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;

	mutex_lock(&devp->afe_mutex);
	if (!(devp->flags & TVAFE_FLAG_DEV_STARTED)) {

		pr_err("[tvafe..] tvafe_dec_stop(%d) decode havn't started\n",
				devp->index);
		mutex_unlock(&devp->afe_mutex);
		return;
	}
	if ((port < TVIN_PORT_CVBS0) || (port > TVIN_PORT_CVBS7)) {

		pr_err("[tvafe..] %s(%d), %s unsupport\n", __func__,
				devp->index, tvin_port_str(port));
		mutex_unlock(&devp->afe_mutex);
		return;
	}

	/* init variable */
#if 0
	memset(&tvafe->adc, 0, sizeof(struct tvafe_adc_s));
	memset(&tvafe->cal, 0, sizeof(struct tvafe_cal_s));
	memset(&tvafe->comp_wss, 0, sizeof(struct tvafe_comp_wss_s));
#endif
	memset(&tvafe->cvd2.info, 0, sizeof(struct tvafe_cvd2_info_s));
	memset(&tvafe->parm.info, 0, sizeof(struct tvin_info_s));

	tvafe->parm.port = port;
#if 0
	if (get_cpu_type() < MESON_CPU_TYPE_MESONG9TV)
		tvafe_adc_digital_reset();
#endif
	/* need to do ... */
	/** write 7740 register for cvbs clamp **/
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_SVIDEO7) &&
		!(devp->flags & TVAFE_POWERDOWN_IN_IDLE)) {

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
	devp->flags &= (~TVAFE_FLAG_DEV_STARTED);

	pr_info("[tvafe..] %s stop port:0x%x ok.\n", __func__, port);

	mutex_unlock(&devp->afe_mutex);
}

/*
 * tvafe close port
 */
void tvafe_dec_close(struct tvin_frontend_s *fe)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;

	mutex_lock(&devp->afe_mutex);
	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {

		pr_err("[tvafe..] tvafe_dec_close(%d) decode havn't opened\n",
				devp->index);
		mutex_unlock(&devp->afe_mutex);
		return;
	}
	if ((tvafe->parm.port < TVIN_PORT_CVBS0) ||
		(tvafe->parm.port > TVIN_PORT_CVBS7)) {

		pr_err("[tvafe..] %s(%d), %s unsupport\n", __func__,
				devp->index, tvin_port_str(tvafe->parm.port));
		mutex_unlock(&devp->afe_mutex);
		return;
	}
	/*del_timer_sync(&devp->timer);*/
#ifdef CONFIG_AM_DVB
	g_tvafe_info = NULL;
	/* register aml_fe hook for atv search */
	aml_fe_hook_cvd(NULL, NULL, NULL);
#endif
	/**set cvd2 reset to high**/
	tvafe_cvd2_hold_rst(&tvafe->cvd2);
	/**disable av out**/
	tvafe_enable_avout(tvafe->parm.port, false);

#ifdef TVAFE_POWERDOWN_IN_IDLE
	/**disable tvafe clock**/
	devp->flags |= TVAFE_POWERDOWN_IN_IDLE;
	tvafe_enable_module(false);
	if (tvafe->parm.port == TVIN_PORT_CVBS3)
		adc_set_pll_cntl(0, ADC_EN_ATV_DEMOD);
	else if ((tvafe->parm.port >= TVIN_PORT_CVBS0) &&
		(tvafe->parm.port <= TVIN_PORT_CVBS2))
		adc_set_pll_cntl(0, ADC_EN_TVAFE);
#endif

#ifdef CONFIG_CMA
	tvafe_cma_release(devp);
#endif
	/* init variable */
	memset(tvafe, 0, sizeof(struct tvafe_info_s));

	devp->flags &= (~TVAFE_FLAG_DEV_OPENED);

	pr_info("[tvafe..] %s close afe ok.\n", __func__);

	mutex_unlock(&devp->afe_mutex);
}

/*
 * tvafe vsync interrupt function
 */
int tvafe_dec_isr(struct tvin_frontend_s *fe, unsigned int hcnt64)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;
	enum tvin_aspect_ratio_e aspect_ratio = TVIN_ASPECT_NULL;

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {
		pr_err("[tvafe..] tvafe havn't opened, isr error!!!\n");
		return true;
	}

	if (force_stable)
		return 0;
	/* if there is any error or overflow, do some reset, then rerurn -1;*/
	if ((tvafe->parm.info.status != TVIN_SIG_STATUS_STABLE) ||
		(tvafe->parm.info.fmt == TVIN_SIG_FMT_NULL))
		return -1;

	/* TVAFE CVD2 3D works abnormally => reset cvd2 */
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS7))
		tvafe_cvd2_check_3d_comb(&tvafe->cvd2);

#ifdef TVAFE_SET_CVBS_PGA_EN
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_SVIDEO7) &&
		(port != TVIN_PORT_CVBS3))
		tvafe_cvd2_adj_pga(&tvafe->cvd2);
#endif
#ifdef TVAFE_SET_CVBS_CDTO_EN
	if (tvafe->parm.info.fmt == TVIN_SIG_FMT_CVBS_PAL_I)
		tvafe_cvd2_adj_cdto(&tvafe->cvd2, hcnt64);
#endif
	if (tvafe->parm.info.fmt == TVIN_SIG_FMT_CVBS_PAL_I)
		tvafe_cvd2_adj_hs(&tvafe->cvd2, hcnt64);


	/* tvafe_adc_clamp_adjust(devp); */
#if 0
	/* TVAFE vs counter for VGA */
	if ((port >= TVIN_PORT_VGA0) && (port <= TVIN_PORT_VGA7)) {

		tvafe_vga_vs_cnt(&tvafe->adc);
	if (tvafe->adc.vga_auto.phase_state == VGA_VDIN_BORDER_DET)
				tvin_vdin_bar_detect(tvafe->parm.info.fmt,
							&tvafe->adc);


	}
	/* fetch WSS data must get them during VBI */
	if ((port >= TVIN_PORT_COMP0) && (port <= TVIN_PORT_COMP7))
		tvafe_get_wss_data(&tvafe->comp_wss);
#endif
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS7)) {
		aspect_ratio = tvafe_cvd2_get_wss();
		if (tvafe_dbg_enable)
			pr_info("current aspect_ratio:%d,aspect_ratio_last:%d\n",
				aspect_ratio, tvafe->aspect_ratio_last);
		if (aspect_ratio != tvafe->aspect_ratio_last) {
			tvafe->aspect_ratio_last = aspect_ratio;
			tvafe->aspect_ratio_cnt = 0;
		} else if (++(tvafe->aspect_ratio_cnt) > tvafe_ratio_cnt) {
			tvafe->aspect_ratio = aspect_ratio;
			/* avoid overflow */
			tvafe->aspect_ratio_cnt = tvafe_ratio_cnt;
		}
	}

	return 0;
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

/*
 * tvafe signal signal status: signal on/off
 */
bool tvafe_is_nosig(struct tvin_frontend_s *fe)
{
	bool ret = false;
	/* Get the per-device structure that contains this frontend */
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED) ||
		(devp->flags & TVAFE_POWERDOWN_IN_IDLE)) {
		pr_err("[tvafe..] tvafe havn't opened OR suspend:flags:0x%x!!!\n",
			devp->flags);
		return true;
	}
	if (force_stable)
		return ret;
#if 0
	if ((port >= TVIN_PORT_VGA0) && (port <= TVIN_PORT_COMP7))
		ret = tvafe_adc_no_sig();
#endif
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_SVIDEO7)) {
		ret = tvafe_cvd2_no_sig(&tvafe->cvd2, &devp->mem);

		/*fix black side when config atv snow*/
		if (ret && (port == TVIN_PORT_CVBS3) &&
			(devp->flags & TVAFE_FLAG_DEV_SNOW_FLAG) &&
			(tvafe->cvd2.config_fmt == TVIN_SIG_FMT_CVBS_PAL_I) &&
			(tvafe->cvd2.info.state != TVAFE_CVD2_STATE_FIND))
			tvafe_snow_config_acd();
		else if ((tvafe->cvd2.config_fmt == TVIN_SIG_FMT_CVBS_PAL_I) &&
			(tvafe->cvd2.info.state == TVAFE_CVD2_STATE_FIND) &&
			(port == TVIN_PORT_CVBS3))
			tvafe_snow_config_acd_resume();

		/* normal sigal & adc reg error, reload source mux */
		if (tvafe->cvd2.info.adc_reload_en && !ret)

			tvafe_set_source_muxing(port, devp->pinmux);

	}

	return ret;
}

/*
 * tvafe signal mode status: change/unchange
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

		pr_err("[tvafe..] tvafe havn't opened, get fmt chg error!!!\n");
		return true;
	}
	if (force_stable)
		return ret;
#if 0
	if ((port >= TVIN_PORT_VGA0) && (port <= TVIN_PORT_COMP7))
		ret = tvafe_adc_fmt_chg(&tvafe->parm, &tvafe->adc);
#endif
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_SVIDEO7))
		ret = tvafe_cvd2_fmt_chg(&tvafe->cvd2);

	return ret;
}

/*
 * tvafe adc lock status: lock/unlock
 */
bool tvafe_pll_lock(struct tvin_frontend_s *fe)
{
	bool ret = true;

#if 0  /* can not trust pll lock status */
	/* Get the per-device structure that contains this frontend */
	struct tvafe_dev_s *devp =
				container_of(fe, struct tvafe_dev_s, frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;

	if ((port >= TVIN_PORT_VGA0) && (port <= TVIN_PORT_COMP7))
		ret = tvafe_adc_get_pll_status();
#endif
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

		pr_err("[tvafe..] tvafe havn't opened, get sig fmt error!!!\n");
		return fmt;
	}
#if 0
	if ((port >= TVIN_PORT_VGA0) && (port <= TVIN_PORT_COMP7))
		fmt = tvafe_adc_search_mode(&tvafe->parm, &tvafe->adc);
#endif
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_SVIDEO7))
		fmt = tvafe_cvd2_get_format(&tvafe->cvd2);

	tvafe->parm.info.fmt = fmt;
	if (tvafe_dbg_enable)
		pr_info("[tvafe..] %s fmt:%s.\n", __func__,
			tvin_sig_fmt_str(fmt));

	return fmt;
}

/*
 * tvafe signal property: 2D/3D, color format, aspect ratio, pixel repeat
 */
void tvafe_get_sig_property(struct tvin_frontend_s *fe,
		struct tvin_sig_property_s *prop)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;
	unsigned int hs_adj_lev = cutwindow_val_h_level1;

	prop->trans_fmt = TVIN_TFMT_2D;
	if ((port >= TVIN_PORT_VGA0) &&
			(port <= TVIN_PORT_VGA7)) {
		prop->color_format = TVIN_RGB444;
		if (vga_yuv422_enable)
			prop->dest_cfmt = TVIN_YUV422;
		else
		prop->dest_cfmt = TVIN_YUV444;

	} else {
		prop->color_format = TVIN_YUV444;
		prop->dest_cfmt = TVIN_YUV422;
	}
#ifdef TVAFE_CVD2_AUTO_DE_ENABLE
	if ((port >= TVIN_PORT_CVBS0) && (port <= TVIN_PORT_CVBS7)) {
		if (tvafe->cvd2.info.vlines.de_offset != 0) {
			prop->vs = cutwindow_val_v;
			prop->ve = cutwindow_val_v;
		} else {
			prop->vs = 0;
			prop->ve = 0;
		}
		if (tvafe->cvd2.info.hs_adj_en) {
			if (tvafe->cvd2.info.hs_adj_level == 1)
				hs_adj_lev = cutwindow_val_h_level1;
			else if (tvafe->cvd2.info.hs_adj_level == 2)
				hs_adj_lev = cutwindow_val_h_level2;
			else if (tvafe->cvd2.info.hs_adj_level == 3)
				hs_adj_lev = cutwindow_val_h_level3;
			else if (tvafe->cvd2.info.hs_adj_level == 4)
				hs_adj_lev = cutwindow_val_h_level4;
			else
				hs_adj_lev = 0;
			if (tvafe->cvd2.info.hs_adj_dir == true) {
				prop->hs = 0;
				prop->he = hs_adj_lev;
			} else {
				prop->hs = hs_adj_lev;
				prop->he = 0;
			}

		} else {
			prop->hs = 0;
			prop->he = 0;
		}
	}
#endif
	prop->color_fmt_range = TVIN_YUV_LIMIT;
	prop->aspect_ratio = tvafe->aspect_ratio;
	prop->decimation_ratio = 0;
	prop->dvi_info = 0;
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
#if 0
/*
 * tvafe set vga parameters: h/v position, phase, clock
 */
void tvafe_vga_set_parm(struct tvafe_vga_parm_s *vga_parm,
		struct tvin_frontend_s *fe)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {

		pr_err("[tvafe..] tvafe havn't opened, vga parm error!!!\n");
		return;
	}

	if (vga_parm == 0)
		tvafe_adc_set_param(&tvafe->parm, &tvafe->adc);
	else
		tvafe_adc_set_deparam(&tvafe->parm, &tvafe->adc);
}

/*
 * tvafe get vga parameters: h/v position, phase, clock
 */
void tvafe_vga_get_parm(struct tvafe_vga_parm_s *vga_parm,
		struct tvin_frontend_s *fe)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	struct tvafe_vga_parm_s *parm = &tvafe->adc.vga_parm;

	vga_parm->clk_step     = parm->clk_step;
	vga_parm->phase        = parm->phase;
	vga_parm->hpos_step    = parm->hpos_step;
	vga_parm->vpos_step    = parm->vpos_step;
	vga_parm->vga_in_clean = parm->vga_in_clean;
}
/*
 * tvafe configure format Reg table
 */
void tvafe_fmt_config(struct tvin_frontend_s *fe)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;


	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {

		pr_err("[tvafe..] tvafe havn't opened, config fmt error!!!\n");
		return;
	}
	/*store the current fmt avoid configuration again*/
	if (tvafe->adc.current_fmt != tvafe->parm.info.fmt)
		tvafe->adc.current_fmt = tvafe->parm.info.fmt;
	else{

		pr_info("[tvafe..] %s,no use to config fmt:%s.\n",
			__func__, tvin_sig_fmt_str(tvafe->parm.info.fmt));
		return;
	}
	if ((port >= TVIN_PORT_VGA0) && (port <= TVIN_PORT_VGA7)) {

		tvafe_set_vga_fmt(&tvafe->parm, &tvafe->cal, devp->pinmux);
		pr_info("[tvafe..] %s, config fmt:%s.\n",
			__func__, tvin_sig_fmt_str(tvafe->parm.info.fmt));

	} else if ((port >= TVIN_PORT_COMP0) && (port <= TVIN_PORT_COMP7)) {

		tvafe_set_comp_fmt(&tvafe->parm, &tvafe->cal, devp->pinmux);
		pr_info("[tvafe..] %s, config fmt:%s.\n",
			__func__, tvin_sig_fmt_str(tvafe->parm.info.fmt));
	}
}
/*
 * tvafe calibration function called by vdin state machine
 */
bool tvafe_cal(struct tvin_frontend_s *fe)
{
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;

	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {

		pr_err("[tvafe..] tvafe havn't opened, calibration error!!!\n");
		return false;
	}

	return tvafe_adc_cal(&tvafe->parm, &tvafe->cal);
}
/*
 * tvafe skip some frame after adjusting vga parameter to avoid picture flicker
 */
bool tvafe_check_frame_skip(struct tvin_frontend_s *fe)
{
	bool ret = false;
	struct tvafe_dev_s *devp = container_of(fe, struct tvafe_dev_s,
						frontend);
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvin_port_e port = tvafe->parm.port;


	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {

		pr_err("[tvafe..] tvafe havn't opened, check frame error!!!\n");
		return ret;
	}

	if (((port >= TVIN_PORT_COMP0) && (port <= TVIN_PORT_COMP7)) ||
			((port >= TVIN_PORT_VGA0) &&
			(port <= TVIN_PORT_VGA7))) {
		ret = tvafe_adc_check_frame_skip(&tvafe->adc);
	}
	return ret;
}
#endif
static struct tvin_state_machine_ops_s tvafe_sm_ops = {
	.nosig            = tvafe_is_nosig,
	.fmt_changed      = tvafe_fmt_chg,
	.get_fmt          = tvafe_get_fmt,
	.fmt_config       = NULL,
	.adc_cal          = NULL,
	.pll_lock         = tvafe_pll_lock,
	.get_sig_propery  = tvafe_get_sig_property,
	.vga_set_param    = NULL,
	.vga_get_param    = NULL,
	.check_frame_skip = NULL,
	.get_secam_phase = tvafe_cvbs_get_secam_phase,
};

static int tvafe_open(struct inode *inode, struct file *file)
{
	struct tvafe_dev_s *devp;

	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct tvafe_dev_s, cdev);
	file->private_data = devp;

	/* ... */

	pr_info("[tvafe..] %s: open device\n", __func__);

	return 0;
}

static int tvafe_release(struct inode *inode, struct file *file)
{
	struct tvafe_dev_s *devp = file->private_data;

	file->private_data = NULL;

	/* Release some other fields */
	/* ... */

	pr_info("[tvafe..] tvafe: device %d release ok.\n", devp->index);

	return 0;
}


static long tvafe_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	struct tvafe_dev_s *devp = file->private_data;
	struct tvafe_info_s *tvafe = &devp->tvafe;
	enum tvafe_cvbs_video_e cvbs_lock_status = TVAFE_CVBS_VIDEO_HV_UNLOCKED;
#if 0
	unsigned char i;
	enum tvin_port_e port = tvafe->parm.port;
	enum tvin_sig_fmt_e fmt = tvafe->parm.info.fmt;
	struct tvafe_vga_edid_s edid;
	struct tvafe_adc_cal_clamp_s clamp_diff;
#endif
	if (_IOC_TYPE(cmd) != _TM_T) {
		pr_err("%s invalid command: %u\n", __func__, cmd);
		return -ENOSYS;
	}

	/* pr_info("[tvafe..] %s command: %u\n", __func__, cmd); */
	if (disableapi)
		return -ENOSYS;
	mutex_lock(&devp->afe_mutex);

	/* EDID command !!! */
	/*((cmd != TVIN_IOC_S_AFE_VGA_EDID) &&
		(cmd != TVIN_IOC_G_AFE_VGA_EDID))*/
	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {

		pr_info("[tvafe..] %s, tvafe device is disable, ignore the command %d\n",
				__func__, cmd);
		mutex_unlock(&devp->afe_mutex);
		return -EPERM;
	}

	switch (cmd) {

		case TVIN_IOC_LOAD_REG:
			{
		    if (copy_from_user(&tvaferegs, argp,
					sizeof(struct am_regs_s))) {
				pr_info(KERN_ERR "[tvafe..]load reg errors: can't get buffer lenght\n");
				ret = -EINVAL;
				break;
			}

			if (!tvaferegs.length || (tvaferegs.length > 512)) {
				pr_info(KERN_ERR "[tvafe..]load regs error: buffer length overflow!!!, length=0x%x\n",
					tvaferegs.length);
				ret = -EINVAL;
				break;
			}
			if (enable_db_reg)
				tvafe_set_regmap(&tvaferegs);

			break;
		    }
		case TVIN_IOC_S_AFE_SONWON:
			devp->flags |= TVAFE_FLAG_DEV_SNOW_FLAG;
			tvafe_snow_function_flag = true;
			tvafe_snow_config(1);
			tvafe_snow_config_clamp(1);
			if (tvafe_dbg_enable)
				pr_info("[tvafe..]TVIN_IOC_S_AFE_SONWON\n");
			break;
		case TVIN_IOC_S_AFE_SONWOFF:
			tvafe_snow_config(0);
			tvafe_snow_config_clamp(0);
			devp->flags &= (~TVAFE_FLAG_DEV_SNOW_FLAG);
			if (tvafe_dbg_enable)
				pr_info("[tvafe..]TVIN_IOC_S_AFE_SONWOFF\n");
			break;
#if 0
		case TVIN_IOC_S_AFE_ADC_DIFF:
			if (copy_from_user(&clamp_diff, argp,
				sizeof(struct tvafe_adc_cal_clamp_s))) {

				ret = -EFAULT;
				break;
			}
			pr_info(" clamp_diff=%d,%d,%d\n",
				clamp_diff.a_analog_clamp_diff,
				clamp_diff.b_analog_clamp_diff,
				clamp_diff.c_analog_clamp_diff);
			break;

		case TVIN_IOC_S_AFE_ADC_CAL:
			if (copy_from_user(&tvafe->cal.cal_val,
				argp, sizeof(struct tvafe_adc_cal_s))) {

				ret = -EFAULT;
				break;
			}

			tvafe->cal.cal_val.reserved |= TVAFE_ADC_CAL_VALID;
			tvafe_vga_auto_adjust_disable(&tvafe->adc);
		if ((port >= TVIN_PORT_COMP0) && (port <= TVIN_PORT_COMP7)) {

#if defined(CONFIG_MACH_MESON6TV_H40)
			if ((fmt >= TVIN_SIG_FMT_COMP_1080P_23HZ_D976) &&
				(fmt <= TVIN_SIG_FMT_COMP_1080P_60HZ_D000)) {

				tvafe->cal.cal_val.c_analog_clamp -= 1;

			} else if ((fmt >= TVIN_SIG_FMT_COMP_1080I_47HZ_D952) &&
				(fmt <= TVIN_SIG_FMT_COMP_1080I_60HZ_D000)) {

				;

			} else if (fmt == TVIN_SIG_FMT_COMP_720P_59HZ_D940) {

				tvafe->cal.cal_val.b_analog_clamp += 1;

			} else if (fmt == TVIN_SIG_FMT_COMP_720P_50HZ_D000) {

				tvafe->cal.cal_val.b_analog_clamp += 1;
				tvafe->cal.cal_val.c_analog_clamp += 1;

			} else{

			/* tvafe->cal.cal_val.a_analog_clamp += 2; */
				tvafe->cal.cal_val.b_analog_clamp += 1;
				tvafe->cal.cal_val.c_analog_clamp += 2;
				}
#else
				/* if((fmt>=TVIN_SIG_FMT_COMP_1080P_23HZ_D976)&&
				(fmt<=TVIN_SIG_FMT_COMP_1080P_60HZ_D000)) */
				/* tvafe->cal.cal_val.a_analog_clamp += 4; */
				/* else if((fmt>=
				TVIN_SIG_FMT_COMP_1080I_47HZ_D952)&&
				(fmt<=TVIN_SIG_FMT_COMP_1080I_60HZ_D000)) */
				/* tvafe->cal.cal_val.a_analog_clamp += 3; */
				/* else */
				/* tvafe->cal.cal_val.a_analog_clamp += 2; */
				tvafe->cal.cal_val.b_analog_clamp += 1;
				tvafe->cal.cal_val.c_analog_clamp += 1;
#endif
			}

		else if ((port >= TVIN_PORT_VGA0) && (port <= TVIN_PORT_VGA7)) {


				/* tvafe->cal.cal_val.a_analog_clamp += 2; */
				tvafe->cal.cal_val.b_analog_clamp += 1;
				tvafe->cal.cal_val.c_analog_clamp += 1;
			}
			/* pr_info("\nNot allow to use
			TVIN_IOC_S_AFE_ADC_CAL command!!!\n\n"); */
			tvafe_set_cal_value(&tvafe->cal);
#ifdef LOG_ADC_CAL
			pr_info("\nset_adc_cal\n");
			pr_info("A_cl = %4d %4d %4d\n",
				(int)(tvafe->cal.cal_val.a_analog_clamp),
				(int)(tvafe->cal.cal_val.b_analog_clamp),
				(int)(tvafe->cal.cal_val.c_analog_clamp));
			pr_info("A_gn = %4d %4d %4d\n",
				(int)(tvafe->cal.cal_val.a_analog_gain),
				(int)(tvafe->cal.cal_val.b_analog_gain),
				(int)(tvafe->cal.cal_val.c_analog_gain));
			pr_info("D_gn = %4d %4d %4d\n",
				(int)(tvafe->cal.cal_val.a_digital_gain),
				(int)(tvafe->cal.cal_val.b_digital_gain),
				(int)(tvafe->cal.cal_val.c_digital_gain));
			pr_info("D_o1 = %4d %4d %4d\n",
				((int)(tvafe->cal.cal_val.a_digital_offset1) <<
					21) >> 21,
				((int)(tvafe->cal.cal_val.b_digital_offset1) <<
					21) >> 21,
				((int)(tvafe->cal.cal_val.c_digital_offset1) <<
					21) >> 21);
			pr_info("D_o2 = %4d %4d %4d\n",
				((int)(tvafe->cal.cal_val.a_digital_offset2) <<
					21) >> 21,
				((int)(tvafe->cal.cal_val.b_digital_offset2) <<
					21) >> 21,
				((int)(tvafe->cal.cal_val.c_digital_offset2) <<
					21) >> 21);
			pr_info("\n");
#endif

			break;

		case TVIN_IOC_G_AFE_ADC_CAL:
			{
				ret = tvafe_get_cal_value(&tvafe->cal);
#ifdef LOG_ADC_CAL
				pr_info("\nget_adc_cal\n");
				pr_info("A_cl = %4d %4d %4d\n",
				(int)(tvafe->cal.cal_val.a_analog_clamp),
				(int)(tvafe->cal.cal_val.b_analog_clamp),
				(int)(tvafe->cal.cal_val.c_analog_clamp));
				pr_info("A_gn = %4d %4d %4d\n",
				(int)(tvafe->cal.cal_val.a_analog_gain),
				(int)(tvafe->cal.cal_val.b_analog_gain),
				(int)(tvafe->cal.cal_val.c_analog_gain));
				pr_info("D_gn = %4d %4d %4d\n",
				(int)(tvafe->cal.cal_val.a_digital_gain),
				(int)(tvafe->cal.cal_val.b_digital_gain),
				(int)(tvafe->cal.cal_val.c_digital_gain));
				pr_info("D_o1 = %4d %4d %4d\n",
				((int)(tvafe->cal.cal_val.a_digital_offset1) <<
					21) >> 21,
				((int)(tvafe->cal.cal_val.b_digital_offset1) <<
					21) >> 21,
				((int)(tvafe->cal.cal_val.c_digital_offset1) <<
					21) >> 21);
				pr_info("D_o2 = %4d %4d %4d\n",
				((int)(tvafe->cal.cal_val.a_digital_offset2) <<
					21) >> 21,
				((int)(tvafe->cal.cal_val.b_digital_offset2) <<
					21) >> 21,
				((int)(tvafe->cal.cal_val.c_digital_offset2) <<
					21) >> 21);
				pr_info("\n");
#endif
				if (ret) {

					ret = -EFAULT;
					pr_info("[tvafe..] %s, the command %d error,adc calibriation error.\n",
						__func__, cmd);
					break;
				}
				if (copy_to_user(argp,
					&tvafe->cal.cal_val,
					sizeof(struct tvafe_adc_cal_s))) {

					ret = -EFAULT;
					break;
				}

				break;
			}
		case TVIN_IOC_G_AFE_COMP_WSS:
			{
				if (copy_to_user(argp,
					&tvafe->comp_wss,
					sizeof(struct tvafe_comp_wss_s))) {

					ret = -EFAULT;
					break;
				}
				break;
			}
		case TVIN_IOC_S_AFE_VGA_EDID:
			{
				if (copy_from_user(&edid,
				argp, sizeof(struct tvafe_vga_edid_s))) {

					ret = -EFAULT;
					break;
				}
#ifdef LOG_VGA_EDID
				for (i = 0; i < 32; i++) {

					pr_info("0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x.\n",
					edid.value[(i<<3)+0],
					edid.value[(i<<3)+1],
					edid.value[(i<<3)+2],
					edid.value[(i<<3)+3],
					edid.value[(i<<3)+4],
					edid.value[(i<<3)+5],
					edid.value[(i<<3)+6],
					edid.value[(i<<3)+7]);
				}
#endif
				tvafe_vga_set_edid(&edid);

				break;
			}
		case TVIN_IOC_G_AFE_VGA_EDID:
			{
				tvafe_vga_get_edid(&edid);
				if (copy_to_user(argp, &edid,
					sizeof(struct tvafe_vga_edid_s))) {

					ret = -EFAULT;
					break;
				}
				break;
			}
		case TVIN_IOC_S_AFE_VGA_PARM:
			{
				if (copy_from_user(&tvafe->adc.vga_parm, argp,
					sizeof(struct tvafe_vga_parm_s))) {

					ret = -EFAULT;
					break;
				}
				tvafe_vga_auto_adjust_disable(&tvafe->adc);

				break;
			}
		case TVIN_IOC_G_AFE_VGA_PARM:
			{
				if (copy_to_user(argp,
					&tvafe->adc.vga_parm,
					sizeof(struct tvafe_vga_parm_s))) {

					ret = -EFAULT;
					break;
				}
				break;
			}
		case TVIN_IOC_S_AFE_VGA_AUTO:
			{
				ret = tvafe_vga_auto_adjust_enable(&tvafe->adc);
				break;
			}
		case TVIN_IOC_G_AFE_CMD_STATUS:
			{
				if (copy_to_user(argp, &tvafe->adc.cmd_status,
					sizeof(enum tvafe_cmd_status_e))) {

					ret = -EFAULT;
					break;
				}
				if ((tvafe->adc.cmd_status ==
					TVAFE_CMD_STATUS_SUCCESSFUL) ||
					(tvafe->adc.cmd_status ==
					TVAFE_CMD_STATUS_FAILED) ||
					(tvafe->adc.cmd_status ==
					TVAFE_CMD_STATUS_TERMINATED))

					tvafe->adc.cmd_status =
					TVAFE_CMD_STATUS_IDLE;


				break;
			}
#endif
		case TVIN_IOC_G_AFE_CVBS_LOCK:
			{
				cvbs_lock_status =
				tvafe_cvd2_get_lock_status(&tvafe->cvd2);
				if (copy_to_user(argp,
					&cvbs_lock_status, sizeof(int))) {

					ret = -EFAULT;
					break;
				}
				pr_info("[tvafe..] %s: get cvd2 lock status :%d.\n",
					__func__, cvbs_lock_status);
				break;
			}
		case TVIN_IOC_S_AFE_CVBS_STD:
			{
				enum tvin_sig_fmt_e fmt = TVIN_SIG_FMT_NULL;

				if (copy_from_user(&fmt, argp,
					sizeof(enum tvin_sig_fmt_e))) {
					ret = -EFAULT;
					break;
				}
				tvafe->cvd2.manual_fmt = fmt;
				pr_info("[tvafe..] %s: ioctl set cvd2 manual fmt:%s.\n",
					__func__, tvin_sig_fmt_str(fmt));
				break;
			}
#if 0
		case TVIN_IOC_S_AFE_ADC_COMP_CAL:
			memset(&tvafe->cal.fmt_cal_val, 0,
					sizeof(struct tvafe_adc_comp_cal_s));
	    if (copy_from_user(&tvafe->cal.fmt_cal_val,
			argp, sizeof(struct tvafe_adc_comp_cal_s))) {

		ret = -EFAULT;
		break;
	    }

	    tvafe->cal.fmt_cal_val.comp_cal_val[0].reserved |=
							TVAFE_ADC_CAL_VALID;
	    tvafe->cal.fmt_cal_val.comp_cal_val[1].reserved |=
							TVAFE_ADC_CAL_VALID;
	    tvafe->cal.fmt_cal_val.comp_cal_val[2].reserved |=
							TVAFE_ADC_CAL_VALID;

	    tvafe_vga_auto_adjust_disable(&tvafe->adc);
	    /* pr_info("\nNot allow to use
	    TVIN_IOC_S_AFE_ADC_CAL command!!!\n\n"); */
	    if (fmt >= TVIN_SIG_FMT_COMP_1080P_23HZ_D976 &&
		fmt <= TVIN_SIG_FMT_COMP_1080P_60HZ_D000) {
				/* 1080p */
			tvafe->cal.fmt_cal_val.comp_cal_val[0].a_analog_clamp -=
									2;
			tvafe_set_cal_value2(
				&(tvafe->cal.fmt_cal_val.comp_cal_val[0]));
		i = 0;
		} else if ((fmt >= TVIN_SIG_FMT_COMP_720P_59HZ_D940 &&
				fmt <= TVIN_SIG_FMT_COMP_720P_50HZ_D000)
				|| (fmt >= TVIN_SIG_FMT_COMP_1080I_47HZ_D952 &&
				fmt <= TVIN_SIG_FMT_COMP_1080I_60HZ_D000)) {
				/* 720p,1080i */
			tvafe->cal.fmt_cal_val.comp_cal_val[1].b_analog_clamp +=
									1;
			tvafe_set_cal_value2(&
				(tvafe->cal.fmt_cal_val.comp_cal_val[1]));
				i = 1;
		} else{
				/* 480p,576p,480i,576i */
			tvafe->cal.fmt_cal_val.comp_cal_val[2].c_analog_clamp +=
									1;
			tvafe_set_cal_value2(
				&(tvafe->cal.fmt_cal_val.comp_cal_val[2]));
			i = 2;
			}
	    #ifdef LOG_ADC_CAL
		pr_info("\nset_adc_cal:comp[%d]\n", i);
		pr_info("A_cl = %4d %4d %4d\n",
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].a_analog_clamp),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].b_analog_clamp),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].c_analog_clamp));
		pr_info("A_gn = %4d %4d %4d\n",
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].a_analog_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].b_analog_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].c_analog_gain));
		pr_info("D_gn = %4d %4d %4d\n",
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].a_digital_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].b_digital_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].c_digital_gain));
		pr_info("D_o1 = %4d %4d %4d\n",
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].a_digital_offset1)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].b_digital_offset1)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].c_digital_offset1)
			<< 21) >> 21);
		pr_info("D_o2 = %4d %4d %4d\n",
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].a_digital_offset2)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].b_digital_offset2)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[i].c_digital_offset2)
			<< 21) >> 21);
		pr_info("\n");

	    #endif

	    break;

		case TVIN_IOC_G_AFE_ADC_COMP_CAL:
	{
	    #ifdef LOG_ADC_CAL

		pr_info("\nget_adc_cal:comp[0]\n");
		pr_info("A_cl = %4d %4d %4d\n",
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].a_analog_clamp),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].b_analog_clamp),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].c_analog_clamp));
		pr_info("A_gn = %4d %4d %4d\n",
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].a_analog_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].b_analog_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].c_analog_gain));
		pr_info("D_gn = %4d %4d %4d\n",
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].a_digital_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].b_digital_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].c_digital_gain));
		pr_info("D_o1 = %4d %4d %4d\n",
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].a_digital_offset1)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].b_digital_offset1)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].c_digital_offset1)
			<< 21) >> 21);
		pr_info("D_o2 = %4d %4d %4d\n",
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].a_digital_offset2)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].b_digital_offset2)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[0].c_digital_offset2)
			<< 21) >> 21);
		pr_info("\n");

		pr_info("\nget_adc_cal:comp[1]\n");
		pr_info("A_cl = %4d %4d %4d\n",
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].a_analog_clamp),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].b_analog_clamp),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].c_analog_clamp));
		pr_info("A_gn = %4d %4d %4d\n",
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].a_analog_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].b_analog_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].c_analog_gain));
		pr_info("D_gn = %4d %4d %4d\n",
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].a_digital_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].b_digital_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].c_digital_gain));
		pr_info("D_o1 = %4d %4d %4d\n",
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].a_digital_offset1)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].b_digital_offset1)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].c_digital_offset1)
			<< 21) >> 21);
		pr_info("D_o2 = %4d %4d %4d\n",
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].a_digital_offset2)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].b_digital_offset2)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[1].c_digital_offset2)
			<< 21) >> 21);
		pr_info("\n");

		pr_info("\nget_adc_cal:comp[2]\n");
		pr_info("A_cl = %4d %4d %4d\n",
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].a_analog_clamp),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].b_analog_clamp),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].c_analog_clamp));
		pr_info("A_gn = %4d %4d %4d\n",
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].a_analog_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].b_analog_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].c_analog_gain));
		pr_info("D_gn = %4d %4d %4d\n",
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].a_digital_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].b_digital_gain),
		(int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].c_digital_gain));
		pr_info("D_o1 = %4d %4d %4d\n",
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].a_digital_offset1)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].b_digital_offset1)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].c_digital_offset1)
			<< 21) >> 21);
		pr_info("D_o2 = %4d %4d %4d\n",
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].a_digital_offset2)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].b_digital_offset2)
			<< 21) >> 21,
		((int)(tvafe->cal.fmt_cal_val.comp_cal_val[2].c_digital_offset2)
			<< 21) >> 21);
		pr_info("\n");
	    #endif

	    if (ret) {

			ret = -EFAULT;
			pr_info("[tvafe..] %s, the command %d error,adc calibriation error.\n",
				__func__, cmd);
			break;
	    }
	    if (copy_to_user(argp, &tvafe->cal.fmt_cal_val,
			sizeof(struct tvafe_adc_comp_cal_s))) {

		ret = -EFAULT;
		break;
	    }
	    break;
	}
#endif
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
static int tvafe_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long start, len, off;
	unsigned long pfn, size;
	struct tvafe_dev_s *devp = file->private_data;

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;


	/* capture the vbi data  */
	start = (devp->mem.start + (DECODER_VBI_ADDR_OFFSET << 3)) & PAGE_MASK;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + (DECODER_VBI_VBI_SIZE << 3));

	off = vma->vm_pgoff << PAGE_SHIFT;

	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;


	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;

	size = vma->vm_end - vma->vm_start;
	pfn  = off >> PAGE_SHIFT;

	if (io_remap_pfn_range(vma, vma->vm_start,
				pfn, size, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

/* File operations structure. Defined in linux/fs.h */
static const struct file_operations tvafe_fops = {
	.owner   = THIS_MODULE,         /* Owner */
	.open    = tvafe_open,          /* Open method */
	.release = tvafe_release,       /* Release method */
	.unlocked_ioctl   = tvafe_ioctl,         /* Ioctl method */
#ifdef CONFIG_COMPAT
	.compat_ioctl = tvafe_compat_ioctl,
#endif
	.mmap    = tvafe_mmap,
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
	/*return device_create(tvafe_clsp, parent, devno, NULL, "%s",
	  TVAFE_DEVICE_NAME); */
}

static void tvafe_delete_device(int minor)
{
	dev_t devno = MKDEV(MAJOR(tvafe_devno), minor);
	device_destroy(tvafe_clsp, devno);
}
void __iomem *tvafe_reg_base;
void __iomem *tvafe_hiu_reg_base;

int tvafe_reg_read(unsigned int reg, unsigned int *val)
{
	*val = readl(tvafe_reg_base+reg);
	return 0;
}
EXPORT_SYMBOL(tvafe_reg_read);

int tvafe_reg_write(unsigned int reg, unsigned int val)
{
	writel(val, (tvafe_reg_base+reg));
	return 0;
}
EXPORT_SYMBOL(tvafe_reg_write);

int tvafe_hiu_reg_read(unsigned int reg, unsigned int *val)
{
	*val = readl(tvafe_hiu_reg_base+((reg - 0x1000)<<2));
	return 0;
}
EXPORT_SYMBOL(tvafe_hiu_reg_read);

int tvafe_hiu_reg_write(unsigned int reg, unsigned int val)
{
	writel(val, (tvafe_hiu_reg_base+((reg - 0x1000)<<2)));
	return 0;
}
EXPORT_SYMBOL(tvafe_hiu_reg_write);

static unsigned int tvafe_use_reserved_mem;
static struct resource tvafe_memobj;
static int tvafe_drv_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tvafe_dev_s *tdevp;
	int size_io_reg;
	/*const void *name;
	int offset, size, mem_size_m;*/
	struct resource *res;
	/* struct tvin_frontend_s * frontend; */
	/* allocate memory for the per-device structure */
	tdevp = kmalloc(sizeof(struct tvafe_dev_s), GFP_KERNEL);
	if (!tdevp) {
		pr_err("tvafe: failed to allocate memory for tvafe device\n");
		goto fail_kmalloc_tdev;
	}
	memset(tdevp, 0, sizeof(struct tvafe_dev_s));

	if (pdev->dev.of_node) {
		ret = of_property_read_u32(pdev->dev.of_node,
					"tvafe_id", &(tdevp->index));
		if (ret) {
			pr_err("Can't find  tvafe id.\n");
			goto fail_get_id;
		}
	}
	tdevp->flags = 0;

	/* create cdev and reigser with sysfs */
	ret = tvafe_add_cdev(&tdevp->cdev, &tvafe_fops, tdevp->index);
	if (ret) {
		pr_err("%s: failed to add cdev\n", __func__);
		goto fail_add_cdev;
	}
	/* create /dev nodes */
	tdevp->dev = tvafe_create_device(&pdev->dev, tdevp->index);
	if (IS_ERR(tdevp->dev)) {
		pr_err("tvafe: failed to create device node\n");
		/* @todo do with error */
		ret = PTR_ERR(tdevp->dev);
		goto fail_create_device;
	}

	/*create sysfs attribute files*/
	ret = device_create_file(tdevp->dev, &dev_attr_debug);
	ret = device_create_file(tdevp->dev, &dev_attr_cvd_reg8a);
	ret = device_create_file(tdevp->dev, &dev_attr_dumpmem);
	ret = device_create_file(tdevp->dev, &dev_attr_reg);
	if (ret < 0) {
		pr_err("tvafe: fail to create dbg attribute file\n");
		goto fail_create_dbg_file;
	}

	/* get device memory */
#if 0
	res = &tvafe_memobj;
	ret = find_reserve_block(pdev->dev.of_node->name, 0);
	if (ret < 0) {
#ifdef CONFIG_CMA
		if (of_node) {
			ret =
			of_property_read_u32(of_node, "max_size", &mem_size_m);
			if (ret) {
				pr_err("\nvdin cma mem undefined1.\n");

			} else {
				tdevp->cma_mem_size = mem_size_m;
				tdevp->this_pdev = pdev;
				mem_cma_en = 1;
			}
		}
#endif
		if (mem_cma_en != 1) {
			name = of_get_property(of_node,
						"share-memory-name", NULL);
			if (!name) {
				pr_err("\ntvafe memory resource undefined1.\n");
				ret = -EFAULT;
				goto fail_get_resource_mem;
			} else {
				ret = find_reserve_block_by_name(name);
				if (ret < 0) {
					pr_err("\ntvafe memory resource undefined2.\n");
					ret = -EFAULT;
					goto fail_get_resource_mem;
				}
				name = of_get_property(of_node,
						"share-memory-offset", NULL);
				if (name)
					offset = of_read_ulong(name, 1);
				else {
					pr_err("\ntvafe memory resource undefined3.\n");
					ret = -EFAULT;
					goto fail_get_resource_mem;
				}
				name = of_get_property(of_node,
						"share-memory-size", NULL);
				if (name)
					size = of_read_ulong(name, 1);
				else {
					pr_err("\ntvafe memory resource undefined4.\n");
					ret = -EFAULT;
					goto fail_get_resource_mem;
				}

				tdevp->mem.start =
				(phys_addr_t)get_reserve_block_addr(ret)+
								offset;
				tdevp->mem.size = size;
			}
		}

	} else {
		tdevp->mem.start = (phys_addr_t)get_reserve_block_addr(ret);
		tdevp->mem.size = (phys_addr_t)get_reserve_block_size(ret);
	}
#else
	/* res = platform_get_resource(pdev, IORESOURCE_MEM, 0); */
	res = &tvafe_memobj;
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret == 0)
		pr_info("\n tvafe memory resource done.\n");
	else
		pr_info("tvafe: can't get memory resource\n");
#endif
#ifdef CONFIG_CMA
	if (!tvafe_use_reserved_mem) {
		ret = of_property_read_u32(pdev->dev.of_node,
				"flag_cma", &(tdevp->cma_config_flag));
		if (ret) {
			pr_err("don't find  match flag_cma\n");
			tdevp->cma_config_flag = 0;
		}
		if (tdevp->cma_config_flag == 1) {
			ret = of_property_read_u32(pdev->dev.of_node,
				"cma_size", &(tdevp->cma_mem_size));
			if (ret)
				pr_err("don't find  match cma_size\n");
			else
				tdevp->cma_mem_size *= SZ_1M;
		} else if (tdevp->cma_config_flag == 0)
			tdevp->cma_mem_size =
				dma_get_cma_size_int_byte(&pdev->dev);
		tdevp->this_pdev = pdev;
		tdevp->cma_mem_alloc = 0;
		tdevp->cma_config_en = 1;
		pr_info("tvafe cma_mem_size = %d MB\n",
				(u32)tdevp->cma_mem_size/SZ_1M);
	}
#endif
	tvafe_use_reserved_mem = 0;
	if (tdevp->cma_config_en != 1) {
		tdevp->mem.start = res->start;
		tdevp->mem.size = res->end - res->start + 1;
		pr_info(" tvafe cvd memory addr is:0x%x, cvd mem_size is:0x%x .\n",
				tdevp->mem.start,
				tdevp->mem.size);
	}
	if (of_property_read_u32_array(pdev->dev.of_node, "tvafe_pin_mux",
			(u32 *)tvafe_pinmux.pin, TVAFE_SRC_SIG_MAX_NUM)) {
		pr_err("Can't get pinmux data.\n");
	}
	tdevp->pinmux = &tvafe_pinmux;
	if (!tdevp->pinmux) {
		pr_err("tvafe: no platform data!\n");
		ret = -ENODEV;
	}

	if (of_get_property(pdev->dev.of_node, "pinctrl-names", NULL)) {
		struct pinctrl *p = devm_pinctrl_get_select_default(&pdev->dev);
		if (IS_ERR(p))
			pr_err(KERN_ERR"tvafe request pinmux error!\n");

	}
	/*reg mem*/
	pr_info("%s:tvafe start get  ioremap .\n", __func__);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing memory resource\n");
		return -ENODEV;
	}
	size_io_reg = resource_size(res);
	pr_info("tvafe_probe reg=%p,size=%x\n",
			(void *)res->start, size_io_reg);
	tvafe_reg_base =
		devm_ioremap_nocache(&pdev->dev, res->start, size_io_reg);
	if (!tvafe_reg_base) {
		dev_err(&pdev->dev, "tvafe ioremap failed\n");
		return -ENOMEM;
	}
	pr_info("%s: tvafe maped reg_base =%p, size=%x\n",
			__func__, tvafe_reg_base, size_io_reg);

	/*remap hiu mem*/
	tvafe_hiu_reg_base = ioremap(0xc883c000, 0x2000);
	/* frontend */
	tvin_frontend_init(&tdevp->frontend, &tvafe_dec_ops,
						&tvafe_sm_ops, tdevp->index);
	sprintf(tdevp->frontend.name, "%s", TVAFE_NAME);
	tvin_reg_frontend(&tdevp->frontend);

	mutex_init(&tdevp->afe_mutex);
	mutex_init(&pll_mutex);

	dev_set_drvdata(tdevp->dev, tdevp);
	platform_set_drvdata(pdev, tdevp);

	/**disable tvafe clock**/
	tdevp->flags |= TVAFE_POWERDOWN_IN_IDLE;
	tvafe_enable_module(false);

	pr_info("tvafe: driver probe ok\n");
	return 0;

fail_create_dbg_file:
	tvafe_delete_device(tdevp->index);
fail_create_device:
	cdev_del(&tdevp->cdev);
fail_add_cdev:
fail_get_id:
	kfree(tdevp);
fail_kmalloc_tdev:
	return ret;

}

static int tvafe_drv_remove(struct platform_device *pdev)
{
	struct tvafe_dev_s *tdevp;
	tdevp = platform_get_drvdata(pdev);

	mutex_destroy(&tdevp->afe_mutex);
	mutex_destroy(&pll_mutex);
	tvin_unreg_frontend(&tdevp->frontend);
	device_remove_file(tdevp->dev, &dev_attr_debug);
	device_remove_file(tdevp->dev, &dev_attr_dumpmem);
	tvafe_delete_device(tdevp->index);
	cdev_del(&tdevp->cdev);
	kfree(tdevp);
	pr_info("tvafe: driver removed ok.\n");
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

		pr_info("tvafe: suspend module, close afe port first\n");
		/* tdevp->flags &= (~TVAFE_FLAG_DEV_OPENED); */
		/*del_timer_sync(&tdevp->timer);*/

		/**set cvd2 reset to high**/
		tvafe_cvd2_hold_rst(&tdevp->tvafe.cvd2);
		/**disable av out**/
		tvafe_enable_avout(tvafe->parm.port, false);
	}
	/*disable and reset tvafe clock*/
	tdevp->flags |= TVAFE_POWERDOWN_IN_IDLE;
	tvafe_enable_module(false);
	adc_set_pll_reset();

	pr_info("tvafe: suspend module\n");

	return 0;
}

static int tvafe_drv_resume(struct platform_device *pdev)
{
	struct tvafe_dev_s *tdevp;
	tdevp = platform_get_drvdata(pdev);

	/*disable and reset tvafe clock*/
	adc_set_pll_reset();
	tvafe_enable_module(true);
	tdevp->flags &= (~TVAFE_POWERDOWN_IN_IDLE);
	pr_info("tvafe: resume module\n");
	return 0;
}
#endif

static void tvafe_drv_shutdown(struct platform_device *pdev)
{
	struct tvafe_dev_s *tdevp;
	struct tvafe_info_s *tvafe;
	tdevp = platform_get_drvdata(pdev);
	tvafe = &tdevp->tvafe;

	/* close afe port first */
	if (tdevp->flags & TVAFE_FLAG_DEV_OPENED) {

		pr_info("tvafe: shutdown module, close afe port first\n");
		/*close afe port,disable tvafe_is_nosig check*/
		/*tdevp->flags &= (~TVAFE_FLAG_DEV_OPENED);*/
		/*del_timer_sync(&tdevp->timer);*/

		/**set cvd2 reset to high**/
		tvafe_cvd2_hold_rst(&tdevp->tvafe.cvd2);
		/**disable av out**/
		tvafe_enable_avout(tvafe->parm.port, false);
	}

	/*disable and reset tvafe clock*/
	/*this cause crash when cmd reboot..*/
	tdevp->flags |= TVAFE_POWERDOWN_IN_IDLE;
	tvafe_enable_module(false);
	adc_set_pll_reset();
	pr_info("tvafe: shutdown module\n");
	return;
}

static const struct of_device_id tvafe_dt_match[] = {
	{
	.compatible     = "amlogic, tvafe",
	},
	{},
};

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
		.of_match_table = tvafe_dt_match,
	}
};

static int __init tvafe_drv_init(void)
{
	int ret = 0;

	ret = alloc_chrdev_region(&tvafe_devno, 0, 1, TVAFE_NAME);
	if (ret < 0) {
		pr_err("%s: failed to allocate major number\n", __func__);
		goto fail_alloc_cdev_region;
	}
	pr_info("%s: major %d\n", __func__, MAJOR(tvafe_devno));

	tvafe_clsp = class_create(THIS_MODULE, TVAFE_NAME);
	if (IS_ERR(tvafe_clsp)) {

		ret = PTR_ERR(tvafe_clsp);
		pr_err("%s: failed to create class\n", __func__);
		goto fail_class_create;
	}

	ret = platform_driver_register(&tvafe_driver);
	if (ret != 0) {
		pr_err("%s: failed to register driver\n", __func__);
		goto fail_pdrv_register;
	}
	pr_info("tvafe: tvafe_init.\n");
	return 0;

fail_pdrv_register:
	class_destroy(tvafe_clsp);
fail_class_create:
	unregister_chrdev_region(tvafe_devno, 1);
fail_alloc_cdev_region:
	return ret;


}

static void __exit tvafe_drv_exit(void)
{
	class_destroy(tvafe_clsp);
	unregister_chrdev_region(tvafe_devno, 1);
	platform_driver_unregister(&tvafe_driver);
	pr_info("tvafe: tvafe_exit.\n");
}

static int tvafe_mem_device_init(struct reserved_mem *rmem,
		struct device *dev)
{
	s32 ret = 0;
	struct resource *res = NULL;
	if (!rmem) {
		pr_info("Can't get reverse mem!\n");
		ret = -EFAULT;
		return ret;
	}
	res = &tvafe_memobj;
	res->start = rmem->base;
	res->end = rmem->base + rmem->size - 1;
	if (rmem->size >= 0x500000)
		tvafe_use_reserved_mem = 1;

	pr_info("init tvafe memsource 0x%lx->0x%lx tvafe_use_reserved_mem:%d\n",
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
	pr_info("tvafe share mem setup\n");
	return 0;
}

module_init(tvafe_drv_init);
module_exit(tvafe_drv_exit);

RESERVEDMEM_OF_DECLARE(tvafe, "amlogic, tvafe_memory",
	tvafe_mem_setup);

MODULE_VERSION(TVAFE_VER);

MODULE_DESCRIPTION("AMLOGIC TVAFE driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xu Lin <lin.xu@amlogic.com>");

