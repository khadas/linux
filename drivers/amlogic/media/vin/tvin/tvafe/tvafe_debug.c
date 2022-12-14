// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Standard Linux Headers */
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
/* Local Headers */
#include "../tvin_global.h"
#include "tvafe_cvd.h"
#include "tvafe_regs.h"
#include "tvafe_debug.h"
#include "tvafe.h"
#include "../vdin/vdin_ctl.h"

bool disable_api;
bool force_stable;

static void tvafe_state(struct tvafe_dev_s *devp)
{
	struct tvin_parm_s *parm = &devp->tvafe.parm;
	struct tvafe_cvd2_s *cvd2 = &devp->tvafe.cvd2;
	struct tvin_info_s *tvin_info = &parm->info;
	struct tvafe_cvd2_info_s *cvd2_info = &cvd2->info;
	struct tvafe_cvd2_lines_s *vlines = &cvd2_info->vlines;
	struct tvafe_cvd2_hw_data_s *hw = &cvd2->hw;
	struct tvafe_user_param_s *user_param = tvafe_get_user_param();
	int i;

	/* top dev info */
	tvafe_pr_info("\n!!tvafe_dev_s info:\n");
	tvafe_pr_info("size of tvafe_dev_s:%d\n", devp->sizeof_tvafe_dev_s);
	tvafe_pr_info("tvafe_dev_s->index:%d\n", devp->index);
	tvafe_pr_info("tvafe_dev_s->flags:0x%x\n", devp->flags);
	tvafe_pr_info("tvafe_dev_s->cma_config_en:%d\n",
		devp->cma_config_en);
	tvafe_pr_info("tvafe_dev_s->cma_config_flag:0x%x\n",
		devp->cma_config_flag);
	tvafe_pr_info("tvafe_dev_s->frame_skip_enable:%d\n",
		devp->frame_skip_enable);
	#ifdef CONFIG_CMA
	tvafe_pr_info("devp->cma_mem_size:0x%x\n", devp->cma_mem_size);
	tvafe_pr_info("devp->cma_mem_alloc:%d\n", devp->cma_mem_alloc);
	#endif
	tvafe_pr_info("devp->mem.start:0x%x\n", devp->mem.start);
	tvafe_pr_info("devp->mem.end:%d\n", devp->mem.start + devp->mem.size);
	tvafe_pr_info("devp->mem.size:0x%x\n", devp->mem.size);
	tvafe_pr_info("tvafe_info_s->aspect_ratio:%d\n",
		devp->tvafe.aspect_ratio);
	tvafe_pr_info("tvafe_info_s->aspect_ratio_cnt:%d\n",
		devp->tvafe.aspect_ratio_cnt);
	/* tvafe_dev_s->tvin_parm_s struct info */
	tvafe_pr_info("\n!!tvafe_dev_s->tvin_parm_s struct info:\n");
	tvafe_pr_info("tvin_parm_s->index:%d\n", parm->index);
	tvafe_pr_info("tvin_parm_s->port:0x%x\n", parm->port);
	tvafe_pr_info("tvin_parm_s->hist_pow:0x%x\n", parm->hist_pow);
	tvafe_pr_info("tvin_parm_s->luma_sum:0x%x\n", parm->luma_sum);
	tvafe_pr_info("tvin_parm_s->pixel_sum:0x%x\n", parm->pixel_sum);
	tvafe_pr_info("tvin_parm_s->flag:0x%x\n", parm->flag);
	tvafe_pr_info("tvin_parm_s->dest_width:0x%x\n", parm->dest_width);
	tvafe_pr_info("tvin_parm_s->dest_height:0x%x\n", parm->dest_height);
	tvafe_pr_info("tvin_parm_s->h_reverse:%d\n", parm->h_reverse);
	tvafe_pr_info("tvin_parm_s->v_reverse:%d\n", parm->v_reverse);
	/* tvafe_dev_s->tvafe_cvd2_s struct info */
	tvafe_pr_info("\n!!tvafe_dev_s->tvafe_cvd2_s struct info:\n");
	tvafe_pr_info("tvafe_cvd2_s->config_fmt:0x%x\n", cvd2->config_fmt);
	tvafe_pr_info("tvafe_cvd2_s->manual_fmt:0x%x\n", cvd2->manual_fmt);
	tvafe_pr_info("tvafe_cvd2_s->vd_port:0x%x\n", cvd2->vd_port);
	tvafe_pr_info("tvafe_cvd2_s->cvd2_init_en:%d\n", cvd2->cvd2_init_en);
	tvafe_pr_info("tvafe_cvd2_s->nonstd_detect_dis:%d\n",
		cvd2->nonstd_detect_dis);
	/* tvin_parm_s->tvin_info_s struct info */
	tvafe_pr_info("\n!!tvin_parm_s->tvin_info_s struct info:\n");
	tvafe_pr_info("tvin_info_s->trans_fmt:0x%x\n", tvin_info->trans_fmt);
	tvafe_pr_info("tvin_info_s->fmt:0x%x\n", tvin_info->fmt);
	tvafe_pr_info("tvin_info_s->status:0x%x\n", tvin_info->status);
	tvafe_pr_info("tvin_info_s->cfmt:%d\n", tvin_info->cfmt);
	tvafe_pr_info("tvin_info_s->fps:%d\n", tvin_info->fps);
	/* tvafe_cvd2_s->tvafe_cvd2_info_s struct info */
	tvafe_pr_info("\n!!tvafe_cvd2_s->tvafe_cvd2_info_s struct info:\n");
	tvafe_pr_info("tvafe_cvd2_info_s->state:0x%x\n", cvd2_info->state);
	tvafe_pr_info("tvafe_cvd2_info_s->state_cnt:%d\n",
		cvd2_info->state_cnt);
	tvafe_pr_info("tvafe_cvd2_info_s->scene_colorful:%d\n",
		cvd2_info->scene_colorful);
	tvafe_pr_info("tvafe_cvd2_info_s->non_std_enable:%d\n",
		cvd2_info->non_std_enable);
	tvafe_pr_info("tvafe_cvd2_info_s->non_std_config:%d\n",
		cvd2_info->non_std_config);
	tvafe_pr_info("tvafe_cvd2_info_s->non_std_worst:%d\n",
		cvd2_info->non_std_worst);
	tvafe_pr_info("tvafe_cvd2_info_s->adc_reload_en:%d\n",
		cvd2_info->adc_reload_en);
	tvafe_pr_info("tvafe_cvd2_info_s->cdto_value:%d\n",
		cvd2_info->cdto_value);
	tvafe_pr_info("tvafe_cvd2_info_s->hs_adj_en:%d\n",
		cvd2_info->hs_adj_en);
	tvafe_pr_info("tvafe_cvd2_info_s->hs_adj_dir:%d\n",
		cvd2_info->hs_adj_dir);
	tvafe_pr_info("tvafe_cvd2_info_s->hs_adj_level:%d\n",
		cvd2_info->hs_adj_level);
	tvafe_pr_info("tvafe_cvd2_info_s->vs_adj_level:%d\n",
		cvd2_info->vs_adj_level);
	tvafe_pr_info("tvafe_cvd2_info_s->vs_adj_en:%d\n",
		cvd2_info->vs_adj_en);
	tvafe_pr_info("tvafe_cvd2_info_s->auto_hs_flag:0x%08x\n",
		cvd2_info->auto_hs_flag);
#ifdef TVAFE_SET_CVBS_CDTO_EN
	tvafe_pr_info("tvafe_cvd2_info_s->hcnt64[0]:0x%x\n",
		cvd2_info->hcnt64[0]);
	tvafe_pr_info("tvafe_cvd2_info_s->hcnt64[1]:0x%x\n",
		cvd2_info->hcnt64[1]);
	tvafe_pr_info("tvafe_cvd2_info_s->hcnt64[2]:0x%x\n",
		cvd2_info->hcnt64[2]);
	tvafe_pr_info("tvafe_cvd2_info_s->hcnt64[3]:0x%x\n",
		cvd2_info->hcnt64[3]);
#endif

	/* tvafe_cvd2_info_s->tvafe_cvd2_lines_s struct info */
	tvafe_pr_info("\n!!tvafe_cvd2_info_s->tvafe_cvd2_lines_s struct info:\n");
	tvafe_pr_info("tvafe_cvd2_lines_s->check_cnt:0x%x\n",
		vlines->check_cnt);
	tvafe_pr_info("tvafe_cvd2_lines_s->de_offset:0x%x\n",
		vlines->de_offset);
	tvafe_pr_info("tvafe_cvd2_lines_s->val[0]:0x%x\n",
		vlines->val[0]);
	tvafe_pr_info("tvafe_cvd2_lines_s->val[1]:0x%x\n",
		vlines->val[1]);
	tvafe_pr_info("tvafe_cvd2_lines_s->val[2]:0x%x\n",
		vlines->val[2]);
	tvafe_pr_info("tvafe_cvd2_lines_s->val[3]:0x%x\n",
		vlines->val[3]);
	/* tvafe_cvd2_s->tvafe_cvd2_hw_data_s struct info */
	tvafe_pr_info("\n!!tvafe_cvd2_s->tvafe_cvd2_hw_data_s struct info:\n");
	tvafe_pr_info("tvafe_cvd2_hw_data_s->no_sig:%d\n", hw->no_sig);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->h_lock:%d\n", hw->h_lock);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->v_lock:%d\n", hw->v_lock);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->h_nonstd:%d\n", hw->h_nonstd);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->v_nonstd:%d\n", hw->v_nonstd);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->no_color_burst:%d\n",
		hw->no_color_burst);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->comb3d_off:%d\n",
		hw->comb3d_off);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->chroma_lock:%d\n",
		hw->chroma_lock);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->pal:%d\n", hw->pal);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->secam:%d\n", hw->secam);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->line625:%d\n", hw->line625);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->noisy:%d\n", hw->noisy);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->vcr:%d\n", hw->vcr);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->vcrtrick:%d\n", hw->vcrtrick);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->vcrff:%d\n", hw->vcrff);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->vcrrew:%d\n", hw->vcrrew);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->acc4xx_cnt:%d\n",
		hw->acc4xx_cnt);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->acc425_cnt:%d\n",
		hw->acc425_cnt);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->acc3xx_cnt:%d\n",
		hw->acc3xx_cnt);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->acc358_cnt:%d\n",
		hw->acc358_cnt);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->secam_detected:%d\n",
		hw->secam_detected);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->secam_phase:%d\n",
		hw->secam_phase);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->fsc_358:%d\n", hw->fsc_358);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->fsc_425:%d\n", hw->fsc_425);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->fsc_443:%d\n", hw->fsc_443);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->noise_level:%d\n",
		hw->noise_level);
	tvafe_pr_info("tvafe_cvd2_hw_data_s->low_amp:%d\n", hw->low_amp);

	tvafe_pr_info("\n tvafe_cvd2_info_s->smr_cnt:%d\n",
		cvd2_info->smr_cnt);
	tvafe_pr_info("tvafe_cvd2_info_s->isr_cnt:%d\n",
		cvd2_info->isr_cnt);
	tvafe_pr_info("tvafe_cvd2_info_s->unlock_cnt:%d\n\n",
		cvd2_info->unlock_cnt);

	for (i = 0; i < 5; i++) {
		tvafe_pr_info("cutwindow_val_h[%d]:%d\n",
			i, user_param->cutwindow_val_h[i]);
	}
	for (i = 0; i < 5; i++) {
		tvafe_pr_info("cutwindow_val_v[%d]:%d\n",
			i, user_param->cutwindow_val_v[i]);
	}
	for (i = 0; i < 5; i++) {
		tvafe_pr_info("horizontal_dir0[%d]:%d\n",
			i, user_param->horizontal_dir0[i]);
	}
	for (i = 0; i < 5; i++) {
		tvafe_pr_info("horizontal_dir1[%d]:%d\n",
			i, user_param->horizontal_dir1[i]);
	}
	for (i = 0; i < 5; i++) {
		tvafe_pr_info("horizontal_stp0[%d]:%d\n",
			i, user_param->horizontal_stp0[i]);
	}
	for (i = 0; i < 5; i++) {
		tvafe_pr_info("horizontal_stp1[%d]:%d\n",
			i, user_param->horizontal_stp1[i]);
	}
	tvafe_pr_info("cutwindow_val_vs_ve:%d\n",
		user_param->cutwindow_val_vs_ve);
	tvafe_pr_info("cdto_adj_hcnt_th:0x%x\n", user_param->cdto_adj_hcnt_th);
	tvafe_pr_info("cdto_adj_ratio_p:0x%x\n", user_param->cdto_adj_ratio_p);
	tvafe_pr_info("cdto_adj_offset_p:0x%x\n",
		      user_param->cdto_adj_offset_p);
	tvafe_pr_info("cdto_adj_ratio_n:0x%x\n", user_param->cdto_adj_ratio_n);
	tvafe_pr_info("cdto_adj_offset_n:0x%x\n",
		      user_param->cdto_adj_offset_n);
	tvafe_pr_info("auto_adj_en:0x%x\n", user_param->auto_adj_en);
	tvafe_pr_info("vline_chk_cnt:%d\n", user_param->vline_chk_cnt);
	tvafe_pr_info("hline_chk_cnt:%d\n", user_param->hline_chk_cnt);
	tvafe_pr_info("nostd_vs_th:0x%x\n", user_param->nostd_vs_th);
	tvafe_pr_info("nostd_no_vs_th:0x%x\n", user_param->nostd_no_vs_th);
	tvafe_pr_info("nostd_vs_cntl:0x%x\n", user_param->nostd_vs_cntl);
	tvafe_pr_info("nostd_vloop_tc:0x%x\n", user_param->nostd_vloop_tc);
	tvafe_pr_info("force_vs_th_flag:%d\n", user_param->force_vs_th_flag);
	tvafe_pr_info("nostd_stable_cnt:%d\n", user_param->nostd_stable_cnt);
	tvafe_pr_info("nostd_dmd_clp_step:0x%x\n",
		user_param->nostd_dmd_clp_step);
	tvafe_pr_info("low_amp_level:%d\n", user_param->low_amp_level);
	tvafe_pr_info("skip_vf_num:%d\n", user_param->skip_vf_num);
	tvafe_pr_info("unlock_cnt_max:%d\n", user_param->unlock_cnt_max);
	tvafe_pr_info("nostd_bypass_iir:%d\n", user_param->nostd_bypass_iir);
	tvafe_pr_info("try_fmt_max_atv:%d\n", try_fmt_max_atv);
	tvafe_pr_info("try_fmt_max_av:%d\n", try_fmt_max_av);
	tvafe_pr_info("avout_en:%d\n", user_param->avout_en);
	tvafe_pr_info("tvafe version :  %s\n", TVAFE_VER);
}

static void tvafe_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

/*default only one tvafe ,echo cvdfmt pali/palm/ntsc/secam >dir*/
static ssize_t debug_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t count)
{
	unsigned char fmt_index = 0;
	struct tvafe_dev_s *devp;
	struct tvafe_user_param_s *user_param = tvafe_get_user_param();
	char *buf_orig, *parm[47] = {NULL};
	unsigned int val;

	devp = dev_get_drvdata(dev);
	if (!buff)
		return count;
	buf_orig = kstrdup(buff, GFP_KERNEL);
	tvafe_parse_param(buf_orig, (char **)&parm);

	if (!strncmp(buff, "cvdfmt", strlen("cvdfmt"))) {
		fmt_index = strlen("cvdfmt") + 1;
		if (!strncmp(buff + fmt_index, "ntscm", strlen("ntscm"))) {
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_NTSC_M;
		} else if (!strncmp(buff + fmt_index,
					"ntsc443", strlen("ntsc443"))) {
			devp->tvafe.cvd2.manual_fmt =
					TVIN_SIG_FMT_CVBS_NTSC_443;
		} else if (!strncmp(buff + fmt_index, "pali", strlen("pali"))) {
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_PAL_I;
		} else if (!strncmp(buff + fmt_index, "palm", strlen("plam"))) {
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_PAL_M;
		} else if (!strncmp(buff + fmt_index, "pal60", strlen("pal60"))) {
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_PAL_60;
		} else if (!strncmp(buff + fmt_index, "palcn", strlen("palcn"))) {
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_PAL_CN;
		} else if (!strncmp(buff + fmt_index, "secam", strlen("secam"))) {
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_CVBS_SECAM;
		} else if (!strncmp(buff + fmt_index, "null", strlen("null"))) {
			devp->tvafe.cvd2.manual_fmt = TVIN_SIG_FMT_NULL;
		} else {
			tvafe_pr_info("%s:invalid command.", buff);
		}
	} else if (!strncmp(buff, "disable_api", strlen("disable_api"))) {
		if (kstrtouint(buff + strlen("disable_api") + 1, 10, &val) == 0)
			disable_api = val;

	} else if (!strncmp(buff, "force_stable", strlen("force_stable"))) {
		if (kstrtouint(buff + strlen("force_stable") + 1, 10, &val) == 0)
			force_stable = val;
	} else if (!strncmp(buff, "tvafe_enable", strlen("tvafe_enable"))) {
		if (kstrtouint(parm[1], 10, &val) < 0)
			goto tvafe_store_err;
		if (val) {
			tvafe_enable_module(true);
			devp->flags &= (~TVAFE_POWERDOWN_IN_IDLE);
			tvafe_pr_info("%s:tvafe enable\n", __func__);
		} else {
			devp->flags |= TVAFE_POWERDOWN_IN_IDLE;
			tvafe_enable_module(false);
			tvafe_pr_info("%s:tvafe down\n", __func__);
		}
	} else if (!strncmp(buff, "afe_ver", strlen("afe_ver"))) {
		tvafe_pr_info("tvafe version :  %s\n", TVAFE_VER);
	} else if (!strncmp(buff, "snow_cfg", strlen("snow_cfg"))) {
		if (!parm[1])
			goto tvafe_store_err;
		if (kstrtouint(parm[1], 10, &val) < 0)
			goto tvafe_store_err;
		if (val) {
			tvafe_set_snow_cfg(true);
			tvafe_pr_info("[tvafe..]hadware snow cfg en\n");
		} else {
			tvafe_set_snow_cfg(false);
			tvafe_pr_info("[tvafe..]hadware snow cfg dis\n");
		}
	} else if (!strncmp(buff, "snowon", strlen("snowon"))) {
		if (!parm[1])
			goto tvafe_store_err;
		if (kstrtouint(parm[1], 10, &val) < 0)
			goto tvafe_store_err;
		if (val) {
			tvafe_snow_config(1);
			tvafe_snow_config_clamp(1);
			devp->flags |= TVAFE_FLAG_DEV_SNOW_FLAG;
			tvafe_snow_function_flag = true;
			tvafe_pr_info("%s:tvafe snowon\n", __func__);
		} else {
			tvafe_snow_config(0);
			tvafe_snow_config_clamp(0);
			devp->flags &= (~TVAFE_FLAG_DEV_SNOW_FLAG);
			tvafe_pr_info("%s:tvafe snowoff\n", __func__);
		}
	} else if (!strcmp(parm[0], "frame_skip_enable")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto tvafe_store_err;
			devp->frame_skip_enable = val;
		}
		tvafe_pr_info("frame_skip_enable:%d\n",
			devp->frame_skip_enable);
	} else if (!strncmp(buff, "state", strlen("state"))) {
		tvafe_state(devp);
	} else if (!strncmp(buff, "nonstd_detect_dis",
		strlen("nonstd_detect_dis"))) {
		if (!parm[1])
			goto tvafe_store_err;
		/*patch for Very low probability hanging issue on atv close*/
		/*only appeared in one project,this for reserved debug*/
		/*default setting to disable the nonstandard signal detect*/
		if (kstrtouint(parm[1], 10, &val) < 0)
			goto tvafe_store_err;
		if (val) {
			devp->tvafe.cvd2.nonstd_detect_dis = true;
			pr_info("[tvafe..]%s:disable nonstd detect\n",
				__func__);
		} else {
			devp->tvafe.cvd2.nonstd_detect_dis = false;
			pr_info("[tvafe..]%s:enable nonstd detect\n",
				__func__);
		}
	} else if (!strncmp(buff, "rf_ntsc50_en", strlen("rf_ntsc50_en"))) {
		if (!parm[1])
			goto tvafe_store_err;
		if (kstrtouint(parm[1], 10, &val) < 0)
			goto tvafe_store_err;
		if (val) {
			tvafe_cvd2_rf_ntsc50_en(true);
			pr_info("[tvafe..]%s:tvafe_cvd2_rf_ntsc50_en\n",
				__func__);
		} else {
			tvafe_cvd2_rf_ntsc50_en(false);
			pr_info("[tvafe..]%s:tvafe_cvd2_rf_ntsc50_dis\n",
				__func__);
		}
	} else if (!strncmp(buff, "force_nostd", strlen("force_nostd"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &force_nostd) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: force_nostd = %d\n",
			__func__, force_nostd);
	} else if (!strncmp(buff, "force_vs_th", strlen("force_vs_th"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10,
				&user_param->force_vs_th_flag) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: force_vs_th_flag = %d\n",
			__func__, user_param->force_vs_th_flag);
	} else if (!strncmp(buff, "nostd_vs_th", strlen("nostd_vs_th"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16,
				&user_param->nostd_vs_th) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: nostd_vs_th = 0x%x\n",
			__func__, user_param->nostd_vs_th);
	} else if (!strncmp(buff, "nostd_no_vs_th", strlen("nostd_no_vs_th"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16,
				&user_param->nostd_no_vs_th) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: nostd_no_vs_th = 0x%x\n",
			__func__, user_param->nostd_no_vs_th);
	} else if (!strncmp(buff, "nostd_vs_cntl", strlen("nostd_vs_cntl"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16,
				&user_param->nostd_vs_cntl) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: nostd_vs_cntl = 0x%x\n",
			__func__, user_param->nostd_vs_cntl);
	} else if (!strncmp(buff, "nostd_vloop_tc", strlen("nostd_vloop_tc"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16,
				&user_param->nostd_vloop_tc) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: nostd_vloop_tc = 0x%x\n",
			__func__, user_param->nostd_vloop_tc);
	} else if (!strncmp(buff, "nostd_cnt", strlen("nostd_cnt"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10,
				&user_param->nostd_stable_cnt) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: nostd_stable_cnt = %d\n",
			__func__, user_param->nostd_stable_cnt);
	} else if (!strncmp(buff, "auto_adj", strlen("auto_adj"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16,
				&user_param->auto_adj_en) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: auto_adj_en = 0x%x\n",
			__func__, user_param->auto_adj_en);
	} else if (!strncmp(buff, "vline_chk_cnt",
		strlen("vline_chk_cnt"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10,
				&user_param->vline_chk_cnt) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: vline_chk_cnt = 0x%x\n",
			__func__, user_param->vline_chk_cnt);
	} else if (!strncmp(buff, "hline_chk_cnt",
		strlen("hline_chk_cnt"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10,
				&user_param->hline_chk_cnt) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: hline_chk_cnt = 0x%x\n",
			__func__, user_param->hline_chk_cnt);
	} else if (!strncmp(buff, "nostd_dmd_clp_step",
		strlen("nostd_dmd_clp_step"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16,
				&user_param->nostd_dmd_clp_step) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: nostd_dmd_clp_step = 0x%x\n",
			__func__, user_param->nostd_dmd_clp_step);
	} else if (!strncmp(buff, "skip_vf_num", strlen("skip_vf_num"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10,
				&user_param->skip_vf_num) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: skip_vf_num = %d\n",
			__func__, user_param->skip_vf_num);
	} else if (!strncmp(buff, "unlock_cnt", strlen("unlock_cnt"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10,
				&user_param->unlock_cnt_max) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: unlock_cnt_max = %d\n",
			__func__, user_param->unlock_cnt_max);
	} else if (!strncmp(buff, "try_fmt_max_atv",
		strlen("try_fmt_max_atv"))) {
		if (kstrtouint(parm[1], 10, &try_fmt_max_atv) < 0)
			goto tvafe_store_err;
		pr_info("[tvafe..]%s: set try_fmt_max_atv = %d\n",
			__func__, try_fmt_max_atv);
	} else if (!strncmp(buff, "try_fmt_max_av",
		strlen("try_fmt_max_av"))) {
		if (kstrtouint(parm[1], 10, &try_fmt_max_av) < 0)
			goto tvafe_store_err;
		pr_info("[tvafe..]%s: set try_fmt_max_av = %d\n",
			__func__, try_fmt_max_av);
	} else if (!strncmp(buff, "avout_en", strlen("avout_en"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16, &user_param->avout_en) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: avout_en = 0x%x\n",
			__func__, user_param->avout_en);
	} else if (!strncmp(buff, "search", strlen("search"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16, &val) < 0)
				goto tvafe_store_err;
			tvafe_atv_search_channel = (val ? true : false);
		}
		pr_info("[tvafe..]%s: tvafe_atv_search_channel = %d\n",
			__func__, tvafe_atv_search_channel);
	} else if (!strncmp(buff, "dbg_print", strlen("dbg_print"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16, &tvafe_dbg_print) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: tvafe_dbg_print = 0x%x\n",
			__func__, tvafe_dbg_print);
	} else if (!strncmp(buff, "print", strlen("print"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16, &tvafe_dbg_print) < 0)
				goto tvafe_store_err;
		}
		pr_info("[tvafe..]%s: tvafe_dbg_print = 0x%x\n",
			__func__, tvafe_dbg_print);
	} else if  (!strcmp(parm[0], "shift_cnt_av")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &val) < 0) {
				pr_info("str->uint error.\n");
				goto tvafe_store_err;
			} else {
				cvd_set_shift_cnt(TVAFE_CVD2_SHIFT_CNT_AV,
						  val);
				pr_info("[tvafe]%s: av shift cnt = %d\n",
					__func__, val);
			}
		} else {
			pr_info("[shift_cnt_av]miss parameter,ori val = %d\n",
				cvd_get_shift_cnt(TVAFE_CVD2_SHIFT_CNT_AV));
		}
	} else if  (!strcmp(parm[0], "shift_cnt_atv")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &val) < 0) {
				pr_info("str->uint error.\n");
				goto tvafe_store_err;
			} else {
				cvd_set_shift_cnt(TVAFE_CVD2_SHIFT_CNT_ATV,
						  val);
				pr_info("[tvafe]%s: atv shift cnt = %d\n",
					__func__, val);
			}
		} else {
			pr_info("[shift_cnt_atv]miss parameter,ori val = %d\n",
				cvd_get_shift_cnt(TVAFE_CVD2_SHIFT_CNT_ATV));
		}
	} else if (!strncmp(parm[0], "nostd_bypass_iir", strlen("nostd_bypass_iir"))) {
		/* bypass iir influence electric performance test */
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &val) == 0)
				user_param->nostd_bypass_iir = val;
			pr_info("[tvafe]%s nostd_bypass_iir val = %u\n",
				__func__, user_param->nostd_bypass_iir);
		} else {
			pr_info("[tvafe]%s nostd_bypass_iir, ori val = %u\n",
				__func__, user_param->nostd_bypass_iir);
		}
	} else if (!strncmp(parm[0], "tvafe_init", strlen("tvafe_init"))) {
		tvafe_bringup_detect_signal(devp, TVIN_PORT_CVBS1);
	} else if (!strncmp(parm[0], "low_amp_level",
		   strlen("low_amp_level"))) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &val) == 0)
				user_param->low_amp_level = val;
			pr_info("[tvafe]%s low_amp_level val = %u\n",
				__func__, user_param->low_amp_level);
		} else {
			pr_info("[tvafe]%s low_amp_level, ori val = %u\n",
				__func__, user_param->low_amp_level);
		}
	} else {
		tvafe_pr_info("[%s]:invalid command.\n", __func__);
	}
	kfree(buf_orig);
	return count;

tvafe_store_err:
	kfree(buf_orig);
	return -EINVAL;
}

static const char *tvafe_debug_usage_str = {
"Usage:\n"
"    echo cvdfmt ntsc/ntsc443/pali/palm/palcn/pal60/secam/null > /sys/class/tvafe/tvafe0/debug;config manual fmt\n"
"\n"
"    echo disable_api val(d) > /sys/class/tvafe/tvafe0/debug;enable/ignore api cmd\n"
"\n"
"    echo force_stable val(d) > /sys/class/tvafe/tvafe0/debug;force stable or not force\n"
"\n"
"    echo tvafe_enable val(d) > /sys/class/tvafe/tvafe0/debug;tvafe enable/disable\n"
"\n"
"    echo afe_ver > /sys/class/tvafe/tvafe0/debug;show tvafe version\n"
"\n"
"    echo snowon val(d) > /sys/class/tvafe/tvafe0/debug;snow on/off\n"
"\n"
"    echo frame_skip_enable val(d) > /sys/class/tvafe/tvafe0/debug;frame skip enable/disable\n"
"\n"
"    echo state > /sys/class/tvafe/tvafe0/debug;show tvafe status\n"
"\n"
"    echo force_nostd val(d) > /sys/class/tvafe/tvafe0/debug;set force_nostd policy\n"
"\n"
"    echo nostd_vs_th val(h) > /sys/class/tvafe/tvafe0/debug;set nostd_vs_th\n"
"\n"
"    echo force_vs_th val(h) > /sys/class/tvafe/tvafe0/debug;set force_vs_th flag\n"
"\n"
"    echo nostd_cnt val(d) > /sys/class/tvafe/tvafe0/debug;set nostd_stable_cnt\n"
"\n"
"    echo skip_vf_num val(d) > /sys/class/tvafe/tvafe0/debug;set skip_vf_num for vdin\n"
"\n"
"    echo avout_en val(d) > /sys/class/tvafe/tvafe0/debug;set avout\n"
"\n"
"    echo print val(h) > /sys/class/tvafe/tvafe0/debug;enable debug print\n"
"    bit[0]: normal debug info\n"
"    bit[4]: vsync isr debug info\n"
"    bit[8]: smr debug info\n"
"    bit[12]: nostd debug info\n"
};

static ssize_t debug_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", tvafe_debug_usage_str);
}

static DEVICE_ATTR_RW(debug);

static void tvafe_dumpmem_adc(struct tvafe_dev_s *devp)
{
	unsigned int mem_start, mem_end;
	unsigned int i, n;

	tvafe_pr_info("%s: mem_start: 0x%x, mem_end: 0x%x, size: 0x%x\n",
		     __func__, devp->mem.start,
		     devp->mem.start + devp->mem.size,
		     devp->mem.size);
	mem_start = devp->mem.start >> 4;
	mem_end = (devp->mem.start + devp->mem.size) >> 4;

	/* step 1: */
	W_APB_REG(CVD2_REG_B2, 0x4c);
	msleep(20);
	W_APB_REG(ACD_REG_22, 0x3000000);
	msleep(20);
	W_APB_REG(CVD2_REG_96, 0x0);
	msleep(20);
	W_APB_REG(ACD_REG_50, 0x2);
	msleep(20);

	/* step 2: */
	mem_start = devp->mem.start >> 4;
	mem_end = (devp->mem.start + devp->mem.size) >> 4;
	W_APB_REG(ACD_REG_51, mem_start);
	msleep(20);
	W_APB_REG(ACD_REG_52, mem_end);
	msleep(20);
	W_APB_REG(ACD_REG_50, 0x3);
	msleep(20);
	W_APB_REG(ACD_REG_50, 0x1);

	/* wait adc data write to memory */
	n = devp->cma_mem_size / SZ_1M + 1;
	for (i = 0; i < n; i++)
		msleep(500);
	tvafe_pr_info("%s: adc data write done\n", __func__);
}

static int tvafe_dumpmem_save_buf(struct tvafe_dev_s *devp, const char *str)
{
	unsigned int highmem_flag = 0;
	unsigned long high_addr;
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
/* unsigned int canvas_real_size = devp->canvas_h * devp->canvas_w; */
	mm_segment_t old_fs = get_fs();
	int i;

	set_fs(KERNEL_DS);
	filp = filp_open(str, O_RDWR | O_CREAT, 0666);
	if (IS_ERR(filp)) {
		tvafe_pr_err("create %s error.\n", str);
		return -1;
	}
	highmem_flag = PageHighMem(phys_to_page(devp->mem.start));
	pr_info("highmem_flag:%d\n", highmem_flag);
	if (devp->cma_config_flag == 1 && highmem_flag != 0) {
		/*tvafe dts config 5M memory*/
		for (i = 0; i < devp->cma_mem_size / SZ_1M; i++) {
			high_addr = devp->mem.start + i * SZ_1M;
			buf = vdin_vmap(high_addr, SZ_1M);
			if (!buf) {
				pr_info("vdin_vmap error\n");
				return -1;
			}
			pr_info("buf:0x%p\n", buf);
/*vdin_dma_flush(devp, buf, SZ_1M, DMA_FROM_DEVICE);*/
			vfs_write(filp, buf, SZ_1M, &pos);
			vdin_unmap_phyaddr(buf);
		}
	} else {
		buf = phys_to_virt(devp->mem.start);
		vfs_write(filp, buf, devp->mem.size, &pos);
	}
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);

	tvafe_pr_info("write mem 0x%x (size 0x%x) to %s done\n",
		      devp->mem.start, devp->mem.size, str);
	return 0;
}

static ssize_t dumpmem_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t len)
{
	unsigned int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[6] = {NULL};
	struct tvafe_dev_s *devp;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	devp = dev_get_drvdata(dev);
	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {
		tvafe_pr_err("tvafe haven't opened, error!!!\n");
		return len;
	}

	strcat(delim1, delim2);
	if (!buff)
		return len;
	buf_orig = kstrdup(buff, GFP_KERNEL);
	/* printk(KERN_INFO "input cmd : %s",buf_orig); */
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!parm[1]) {
		tvafe_pr_err("%s: dumpmem file name missed\n", __func__);
		kfree(buf_orig);
		return len;
	}
	if (!strncmp(parm[0], "dumpmem", strlen("dumpmem"))) {
		tvafe_dumpmem_save_buf(devp, parm[1]);
	} else if (!strncmp(parm[0], "adc", strlen("adc"))) {
		tvafe_dumpmem_adc(devp);
		tvafe_dumpmem_save_buf(devp, parm[1]);
	}

	kfree(buf_orig);
	return len;
}

static DEVICE_ATTR_WO(dumpmem);

static ssize_t reg_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t count)
{
	struct tvafe_dev_s *devp;
	unsigned int arg_n = 0, addr = 0, value = 0, end = 0, tmp = 0;
	char *p, *para, *buf_work, cmd = 0;
	char *argv[3];

	devp = dev_get_drvdata(dev);
	if (!(devp->flags & TVAFE_FLAG_DEV_OPENED)) {
		tvafe_pr_err("tvafe haven't opened, error!!!\n");
		return count;
	}

	buf_work = kstrdup(buff, GFP_KERNEL);
	p = buf_work;

	for (arg_n = 0; arg_n < 3; arg_n++) {
		para = strsep(&p, " ");
		if (!para)
			break;
		argv[arg_n] = para;
	}

	if (arg_n < 1 || arg_n > 3) {
		kfree(buf_work);
		return count;
	}

	cmd = argv[0][0];

		switch (cmd) {
		case 'r':
		case 'R':
			if (arg_n < 2) {
				tvafe_pr_err("syntax error.\n");
			} else {
				if (kstrtouint(argv[1], 16, &tmp) == 0)
					addr = tmp;
				else
					break;
				value = R_APB_REG(addr << 2);
				tvafe_pr_info("APB[0x%04x]=0x%08x\n",
					addr, value);
			}
			break;
		case 'w':
		case 'W':
			if (arg_n < 3) {
				tvafe_pr_err("syntax error.\n");
			} else {
				if (kstrtouint(argv[1], 16, &tmp) == 0)
					value = tmp;
				else
					break;
				if (kstrtouint(argv[2], 16, &tmp) == 0)
					addr = tmp;
				else
					break;
				W_APB_REG(addr << 2, value);
				tvafe_pr_info("Write APB[0x%04x]=0x%08x\n",
					addr, R_APB_REG(addr << 2));
			}
			break;
		case 'd':
		/*case 'D': */
			if (arg_n < 3) {
				tvafe_pr_err("syntax error.\n");
			} else {
				if (kstrtouint(argv[1], 16, &tmp) == 0)
					addr = tmp;
				else
					break;
				if (kstrtouint(argv[2], 16, &tmp) == 0)
					end = tmp;
				else
					break;
				for (; addr <= end; addr++)
					tvafe_pr_info("APB[0x%04x]=0x%08x\n",
					addr, R_APB_REG(addr << 2));
			}
			break;
		case 'D':
			tvafe_pr_info("dump TOP reg----\n");
			for (addr = TOP_BASE_ADD;
				addr <= (TOP_BASE_ADD + 0xb2); addr++)
				tvafe_pr_info("APB[0x%04x]=0x%08x\n",
					      addr, R_APB_REG(addr << 2));
			tvafe_pr_info("dump ACD reg----\n");
			for (addr = ACD_BASE_ADD;
				addr <= (ACD_BASE_ADD + 0xc2); addr++)
				tvafe_pr_info("APB[0x%04x]=0x%08x\n",
					      addr, R_APB_REG(addr << 2));
			tvafe_pr_info("dump CVD2 reg----\n");
			for (addr = CVD_BASE_ADD;
				addr <= (CVD_BASE_ADD + 0xfe); addr++)
				tvafe_pr_info("APB[0x%04x]=0x%08x\n",
					      addr, R_APB_REG(addr << 2));
			tvafe_pr_info("dump reg done----\n");
			break;
		default:
			tvafe_pr_err("not support.\n");
			break;
	}
	kfree(buf_work);
	return count;
}

static ssize_t reg_show(struct device *dev,
		struct device_attribute *attr, char *buff)
{
	ssize_t len = 0;

	len += sprintf(buff + len, "Usage:\n");
	len += sprintf(buff + len,
		"\t echo [read|write <date>] addr > reg;Access ATV DEMON/TVAFE/HDMIRX logic address.\n");
	len += sprintf(buff + len,
		"\t echo dump <start> <end> > reg;Dump ATV DEMON/TVAFE/HDMIRX logic address.\n");
	len += sprintf(buff + len,
		"Address format:\n");
	len += sprintf(buff + len,
		"\t addr    : 0xXXXX, 16 bits register address\n");
	return len;
}

static DEVICE_ATTR_RW(reg);

static ssize_t cutwin_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len,
		"echo h index(d) val(d) > /sys/class/tvafe/tvafe0/cutwin;config cutwindow_h value\n");
	len += sprintf(buf + len,
		"echo v index(d) val(d) > /sys/class/tvafe/tvafe0/cutwin;config cutwindow_v value\n");
	len += sprintf(buf + len,
		"echo r > /sys/class/tvafe/tvafe0/cutwin;read cutwindow value\n");
	return len;
}

static void tvafe_cutwindow_info_print(void)
{
	struct tvafe_user_param_s *user_param = tvafe_get_user_param();
	char *pr_buf;
	unsigned int pr_len;
	int i;

	pr_buf = kzalloc(sizeof(char) * 400, GFP_KERNEL);
	if (!pr_buf)
		return;
	pr_len = 0;
	pr_len += sprintf(pr_buf + pr_len, "cutwindow_h:");
	for (i = 0; i < 5; i++) {
		pr_len += sprintf(pr_buf + pr_len,
			" %d", user_param->cutwindow_val_h[i]);
	}
	pr_len += sprintf(pr_buf + pr_len, "\n cutwindow_v:");
	for (i = 0; i < 5; i++) {
		pr_len += sprintf(pr_buf + pr_len,
			" %d", user_param->cutwindow_val_v[i]);
	}
	pr_len += sprintf(pr_buf + pr_len, "\n horizontal_dir0:");
	for (i = 0; i < 5; i++) {
		pr_len += sprintf(pr_buf + pr_len,
			" %d", user_param->horizontal_dir0[i]);
	}
	pr_len += sprintf(pr_buf + pr_len, "\n horizontal_dir1:");
	for (i = 0; i < 5; i++) {
		pr_len += sprintf(pr_buf + pr_len,
			" %d", user_param->horizontal_dir1[i]);
	}
	pr_len += sprintf(pr_buf + pr_len, "\n horizontal_stp0:");
	for (i = 0; i < 5; i++) {
		pr_len += sprintf(pr_buf + pr_len,
			" %d", user_param->horizontal_stp0[i]);
	}
	pr_len += sprintf(pr_buf + pr_len, "\n horizontal_stp1:");
	for (i = 0; i < 5; i++) {
		pr_len += sprintf(pr_buf + pr_len,
			" %d", user_param->horizontal_stp1[i]);
	}
	pr_info("%s\n", pr_buf);
	kfree(pr_buf);
}

static ssize_t cutwin_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t count)
{
	struct tvafe_user_param_s *user_param = tvafe_get_user_param();
	char *buf_orig, *parm[20] = {NULL};
	unsigned int index, val;

	if (!buff)
		return count;
	buf_orig = kstrdup(buff, GFP_KERNEL);
	tvafe_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "h")) {
		if (!parm[1]) {
			tvafe_cutwindow_info_print();
			goto tvafe_cutwindow_store_err;
		}
		if (kstrtouint(parm[1], 10, &index) < 0)
			goto tvafe_cutwindow_store_err;
		if (index < 5) {
			if (kstrtouint(parm[2], 10, &val) < 0)
				goto tvafe_cutwindow_store_err;
			user_param->cutwindow_val_h[index] = val;
			pr_info("set cutwindow_h[%d] = %d\n", index, val);
		} else {
			pr_info("error: invalid index %d\n", index);
		}
	} else if (!strcmp(parm[0], "v")) {
		if (!parm[1]) {
			tvafe_cutwindow_info_print();
			goto tvafe_cutwindow_store_err;
		}
		if (kstrtouint(parm[1], 10, &index) < 0)
			goto tvafe_cutwindow_store_err;
		if (index < 5) {
			if (kstrtouint(parm[2], 10, &val) < 0)
				goto tvafe_cutwindow_store_err;
			user_param->cutwindow_val_v[index] = val;
			pr_info("set cutwindow_v[%d] = %d\n", index, val);
		} else {
			pr_info("error: invalid index %d\n", index);
		}
	} else if (!strcmp(parm[0], "hor0")) {
		if (!parm[1]) {
			tvafe_cutwindow_info_print();
			goto tvafe_cutwindow_store_err;
		}
		if (kstrtouint(parm[1], 10, &index) < 0)
			goto tvafe_cutwindow_store_err;
		if (index < 5) {
			if (kstrtouint(parm[2], 10, &val) < 0)
				goto tvafe_cutwindow_store_err;
			user_param->horizontal_dir0[index] = val;
			pr_info("set horizontal0[%d] = %d\n", index, val);
		} else {
			pr_info("error: invalid index %d\n", index);
		}
	} else if (!strcmp(parm[0], "hor1")) {
		if (!parm[1]) {
			tvafe_cutwindow_info_print();
			goto tvafe_cutwindow_store_err;
		}
		if (kstrtouint(parm[1], 10, &index) < 0)
			goto tvafe_cutwindow_store_err;
		if (index < 5) {
			if (kstrtouint(parm[2], 10, &val) < 0)
				goto tvafe_cutwindow_store_err;
			user_param->horizontal_dir1[index] = val;
			pr_info("set horizontal1[%d] = %d\n", index, val);
		} else {
			pr_info("error: invalid index %d\n", index);
		}
	} else if (!strcmp(parm[0], "stp0")) {
		if (!parm[1]) {
			tvafe_cutwindow_info_print();
			goto tvafe_cutwindow_store_err;
		}
		if (kstrtouint(parm[1], 10, &index) < 0)
			goto tvafe_cutwindow_store_err;
		if (index < 5) {
			if (kstrtouint(parm[2], 10, &val) < 0)
				goto tvafe_cutwindow_store_err;
			user_param->horizontal_stp0[index] = val;
			pr_info("set horizontal stp0[%d] = %d\n", index, val);
		} else {
			pr_info("error: invalid index %d\n", index);
		}
	} else if (!strcmp(parm[0], "stp1")) {
		if (!parm[1]) {
			tvafe_cutwindow_info_print();
			goto tvafe_cutwindow_store_err;
		}
		if (kstrtouint(parm[1], 10, &index) < 0)
			goto tvafe_cutwindow_store_err;
		if (index < 5) {
			if (kstrtouint(parm[2], 10, &val) < 0)
				goto tvafe_cutwindow_store_err;
			user_param->horizontal_stp1[index] = val;
			pr_info("set horizontal stp1[%d] = %d\n", index, val);
		} else {
			pr_info("error: invalid index %d\n", index);
		}
	} else if (!strcmp(parm[0], "r")) {
		tvafe_cutwindow_info_print();
	} else if (!strcmp(parm[0], "test")) {
		if (!parm[2]) {
			pr_info("cutwin test en=0x%x, hcnt=0x%x, vcnt=0x%x, hcut=%d, vcut=%d\n",
				user_param->cutwin_test_en,
				user_param->cutwin_test_hcnt,
				user_param->cutwin_test_vcnt,
				user_param->cutwin_test_hcut,
				user_param->cutwin_test_vcut);
			goto tvafe_cutwindow_store_err;
		}
		if (!strcmp(parm[1], "en")) {
			if (kstrtouint(parm[2], 16,
				       &user_param->cutwin_test_en) < 0)
				goto tvafe_cutwindow_store_err;
			pr_info("set cutwin_test_en = 0x%x\n",
				user_param->cutwin_test_en);
		} else if (!strcmp(parm[1], "hcnt")) {
			if (kstrtouint(parm[2], 16,
				       &user_param->cutwin_test_hcnt) < 0)
				goto tvafe_cutwindow_store_err;
			pr_info("set cutwin_test_hcnt = 0x%x\n",
				user_param->cutwin_test_hcnt);
		} else if (!strcmp(parm[1], "vcnt")) {
			if (kstrtouint(parm[2], 16,
				       &user_param->cutwin_test_vcnt) < 0)
				goto tvafe_cutwindow_store_err;
			pr_info("set cutwin_test_vcnt = 0x%x\n",
				user_param->cutwin_test_vcnt);
		} else if (!strcmp(parm[1], "hcut")) {
			if (kstrtouint(parm[2], 10,
				       &user_param->cutwin_test_hcut) < 0)
				goto tvafe_cutwindow_store_err;
			pr_info("set cutwin_test_hcut = %d\n",
				user_param->cutwin_test_hcut);
		} else if (!strcmp(parm[1], "vcut")) {
			if (kstrtouint(parm[2], 10,
				       &user_param->cutwin_test_vcut) < 0)
				goto tvafe_cutwindow_store_err;
			pr_info("set cutwin_test_vcut = %d\n",
				user_param->cutwin_test_vcut);
		} else {
			pr_info("error: invalid command\n");
		}
	} else if (!strcmp(parm[0], "cdto")) {
		if (!parm[2]) {
			pr_info("cdto_adj hcnt_th=0x%x, ratio_p=%d, offset_p=0x%x, ratio_n=%d, offset_n=0x%x\n",
				user_param->cdto_adj_hcnt_th,
				user_param->cdto_adj_ratio_p,
				user_param->cdto_adj_offset_p,
				user_param->cdto_adj_ratio_n,
				user_param->cdto_adj_offset_n);
			goto tvafe_cutwindow_store_err;
		}
		if (!strcmp(parm[1], "hcnt_th")) {
			if (kstrtouint(parm[2], 16,
				       &user_param->cdto_adj_hcnt_th) < 0)
				goto tvafe_cutwindow_store_err;
			pr_info("set cdto_adj_hcnt_th = 0x%x\n",
				user_param->cdto_adj_hcnt_th);
		} else if (!strcmp(parm[1], "ratio_p")) {
			if (kstrtouint(parm[2], 10,
				       &user_param->cdto_adj_ratio_p) < 0)
				goto tvafe_cutwindow_store_err;
			pr_info("set cdto_adj_ratio_p = %d\n",
				user_param->cdto_adj_ratio_p);
		} else if (!strcmp(parm[1], "offset_p")) {
			if (kstrtouint(parm[2], 16,
				       &user_param->cdto_adj_offset_p) < 0)
				goto tvafe_cutwindow_store_err;
			pr_info("set cdto_adj_offset_p = 0x%x\n",
				user_param->cdto_adj_offset_p);
		} else if (!strcmp(parm[1], "ratio_n")) {
			if (kstrtouint(parm[2], 10,
				       &user_param->cdto_adj_ratio_n) < 0)
				goto tvafe_cutwindow_store_err;
			pr_info("set cdto_adj_ratio_n = %d\n",
				user_param->cdto_adj_ratio_n);
		} else if (!strcmp(parm[1], "offset_n")) {
			if (kstrtouint(parm[2], 16,
				       &user_param->cdto_adj_offset_n) < 0)
				goto tvafe_cutwindow_store_err;
			pr_info("set cdto_adj_offset_n = 0x%x\n",
				user_param->cdto_adj_offset_n);
		} else {
			pr_info("error: invalid command\n");
		}
	} else {
		pr_info("error: invalid command\n");
	}

	kfree(buf_orig);
	return count;

tvafe_cutwindow_store_err:
	kfree(buf_orig);
	return -EINVAL;
}

static DEVICE_ATTR_RW(cutwin);

static ssize_t cvd_store(struct device *dev,
		struct device_attribute *attr, const char *buff, size_t count)
{
	struct tvafe_dev_s *devp;
	char *buf_orig, *parm[47] = {NULL};
	int ret;

	devp = dev_get_drvdata(dev);
	if (!buff)
		return count;
	buf_orig = kstrdup(buff, GFP_KERNEL);
	tvafe_parse_param(buf_orig, (char **)&parm);

	ret = cvd_set_debug_parm(buff, parm);
	if (ret) {
		tvafe_pr_info("pls input:echo cmd val >/sys/class/..");
		goto tvafe_store_err;
	}

	kfree(buf_orig);
	return count;

tvafe_store_err:
	kfree(buf_orig);
	return -EINVAL;
}

static DEVICE_ATTR_WO(cvd);

int tvafe_device_create_file(struct device *dev)
{
	int ret = 0;

	ret |= device_create_file(dev, &dev_attr_debug);
	ret |= device_create_file(dev, &dev_attr_dumpmem);
	ret |= device_create_file(dev, &dev_attr_reg);
	ret |= device_create_file(dev, &dev_attr_cutwin);
	ret |= device_create_file(dev, &dev_attr_cvd);

	return ret;
}

void tvafe_remove_device_files(struct device *dev)
{
	device_remove_file(dev, &dev_attr_cvd);
	device_remove_file(dev, &dev_attr_cutwin);
	device_remove_file(dev, &dev_attr_reg);
	device_remove_file(dev, &dev_attr_dumpmem);
	device_remove_file(dev, &dev_attr_debug);
}

