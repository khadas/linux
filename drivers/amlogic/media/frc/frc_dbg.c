// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/ioport.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>

#include <linux/amlogic/media/frc/frc_reg.h>
#include <linux/amlogic/media/frc/frc_common.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/power_domain.h>

#include "frc_drv.h"
#include "frc_dbg.h"
#include "frc_buf.h"
#include "frc_hw.h"
#include "frc_proc.h"

static void frc_debug_parse_param(char *buf_orig, char **parm)
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

void frc_status(struct frc_dev_s *devp)
{
	struct frc_fw_data_s *fw_data;
	struct vinfo_s *vinfo = get_current_vinfo();

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	pr_frc(0, "%s\n", FRC_FW_VER);
	pr_frc(0, "%s\n", fw_data->frc_alg_ver);
	pr_frc(0, "probe_ok sts:%d power_on_flag:%d hw_pos:%d (1:after) fw_pause:%d\n",
		devp->probe_ok, devp->power_on_flag, devp->frc_hw_pos, devp->frc_fw_pause);
	pr_frc(0, "frs state:%d (%s) new:%d\n", devp->frc_sts.state,
	       frc_state_ary[devp->frc_sts.state], devp->frc_sts.new_state);
	pr_frc(0, "input in_hsize=%d in_vsize=%d\n", devp->in_sts.in_hsize, devp->in_sts.in_vsize);
	pr_frc(0, "game_mode=%d secure_mode=%d pic_type=%d\n", devp->in_sts.game_mode,
		devp->in_sts.secure_mode, devp->in_sts.pic_type);
	pr_frc(0, "dbg en:%d in_out_ratio=0x%x\n", devp->dbg_force_en, devp->dbg_in_out_ratio);
	pr_frc(0, "dbg hsize=%d vsize=%d\n", devp->dbg_input_hsize, devp->dbg_input_vsize);
	pr_frc(0, "vf_sts:%d, vf_type:0x%x, signal_type=0x%x, source_type=0x%x\n",
	       devp->in_sts.vf_sts,
	       devp->in_sts.vf_type, devp->in_sts.signal_type, devp->in_sts.source_type);
	pr_frc(0, "duration=%d\n", devp->in_sts.duration);
	pr_frc(0, "vout hsize:%d vsize:%d\n", devp->out_sts.vout_width, devp->out_sts.vout_height);
	pr_frc(0, "vout out_framerate:%d\n", devp->out_sts.out_framerate);
	pr_frc(0, "vout sync_duration_num:%d sync_duration_den:%d hz:%d\n",
		vinfo->sync_duration_num, vinfo->sync_duration_den,
		vinfo->sync_duration_num / vinfo->sync_duration_den);
	pr_frc(0, "data_compress_rate:%d, real total size:%d\n",
		FRC_COMPRESS_RATE, devp->buf.real_total_size);
	pr_frc(0, "buf secured:%d\n", devp->buf.secured);
	pr_frc(0, "vpu int vs_duration:%d timestamp:%ld\n",
	       devp->vs_duration, (ulong)devp->vs_timestamp);
	pr_frc(0, "frc in vs_duration:%d timestamp:%ld\n",
	       devp->in_sts.vs_duration, (ulong)devp->in_sts.vs_timestamp);
	pr_frc(0, "frc out vs_duration:%d timestamp:%ld\n",
	       devp->out_sts.vs_duration, (ulong)devp->out_sts.vs_timestamp);
	pr_frc(0, "int in vs_cnt:%d, vs_tsk_cnt:%d, inp_err:0x%x\n",
		devp->in_sts.vs_cnt, devp->in_sts.vs_tsk_cnt,
		devp->ud_dbg.inp_undone_err);
	pr_frc(0, "int out vs_cnt:%d, vs_tsk_cnt:%d\n",
		devp->out_sts.vs_cnt, devp->out_sts.vs_tsk_cnt);
	pr_frc(0, "frc_st vs_cnt:%d vf_repeat_cnt:%d vf_null_cnt:%d\n", devp->frc_sts.vs_cnt,
		devp->in_sts.vf_repeat_cnt, devp->in_sts.vf_null_cnt);
	pr_frc(0, "mc_loss_en = %d me_loss_en = %d\n", fw_data->frc_top_type.mc_loss_en,
		fw_data->frc_top_type.me_loss_en);
	pr_frc(0, "loss_ratio = %d\n", devp->loss_ratio);
	pr_frc(0, "frc_prot_mode = %d\n", devp->prot_mode);
	pr_frc(0, "film_mode = %d\n", devp->film_mode);
	pr_frc(0, "film_mode_det = %d\n", devp->film_mode_det);
	pr_frc(0, "frc in hsize = %d\n", fw_data->frc_top_type.hsize);
	pr_frc(0, "frc in vsize = %d\n", fw_data->frc_top_type.vsize);
	pr_frc(0, "vfb = %d\n", fw_data->frc_top_type.vfb);
	pr_frc(0, "frc_fb_num = %d\n", fw_data->frc_top_type.frc_fb_num);
	pr_frc(0, "frc_ratio_mode = %d\n", fw_data->frc_top_type.frc_ratio_mode);
	pr_frc(0, "film_mode = %d\n", fw_data->frc_top_type.film_mode);
	pr_frc(0, "film_hwfw_sel = %d\n", fw_data->frc_top_type.film_hwfw_sel);
	pr_frc(0, "is_me1mc4 = %d\n", fw_data->frc_top_type.is_me1mc4);
	pr_frc(0, "frc out_hsize = %d\n", fw_data->frc_top_type.out_hsize);
	pr_frc(0, "frc out_vsize = %d\n", fw_data->frc_top_type.out_vsize);
	pr_frc(0, "me_hold_line = %d\n", fw_data->holdline_parm.me_hold_line);
	pr_frc(0, "mc_hold_line = %d\n", fw_data->holdline_parm.mc_hold_line);
	pr_frc(0, "inp_hold_line = %d\n", fw_data->holdline_parm.inp_hold_line);
	pr_frc(0, "reg_post_dly_vofst = %d\n", fw_data->holdline_parm.reg_post_dly_vofst);
	pr_frc(0, "reg_mc_dly_vofst0 = %d\n", fw_data->holdline_parm.reg_mc_dly_vofst0);
	pr_frc(0, "auto_ctrl = %d\n", devp->frc_sts.auto_ctrl);
	pr_frc(0, "force_en = %d, force_hsize = %d, force_vsize = %d\n",
		devp->force_size.force_en, devp->force_size.force_hsize,
		devp->force_size.force_vsize);
	pr_frc(0, "get_video_latency = %d\n", frc_get_video_latency());
	pr_frc(0, "get_frc_adj_me_out_line = %d\n", devp->out_line);
	pr_frc(0, "vendor = %d\n", fw_data->frc_fw_alg_ctrl.frc_algctrl_u8vendor);
	pr_frc(0, "mc_fb = %d\n", fw_data->frc_fw_alg_ctrl.frc_algctrl_u8mcfb);

}

ssize_t frc_debug_if_help(struct frc_dev_s *devp, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len, "status:%d\t: (%s)\n", devp->frc_sts.state,
				frc_state_ary[devp->frc_sts.state]);
	len += sprintf(buf + len, "dbg_level=%d\n", frc_dbg_en);//style for tool
	len += sprintf(buf + len, "dbg_mode\t: 0:disable 1:enable 2:bypass\n");
	len += sprintf(buf + len, "dbg_ratio\t: set debug input ratio\n");
	len += sprintf(buf + len, "dbg_force\t: force debug mode\n");
	len += sprintf(buf + len, "test_pattern disable or enable\t:\n");
	len += sprintf(buf + len, "dump_bufcfg\t: dump buf address, size\n");
	len += sprintf(buf + len, "dump_linkbuf\t: dump link buffer data\n");
	len += sprintf(buf + len, "dump_init_reg\t: dump initial table\n");
	len += sprintf(buf + len, "dump_buf_reg\t: dump buffer register\n");
	len += sprintf(buf + len, "dump_buf_reg\t: dump buffer register\n");
	len += sprintf(buf + len, "dump_data addr size\t: dump cma buf data\n");
	len += sprintf(buf + len, "frc_pos 0,1\t: 0:before postblend; 1:after postblend\n");
	len += sprintf(buf + len, "frc_pause 0/1 \t: 0: fw work 1:fw not work\n");
	len += sprintf(buf + len, "monitor_ireg idx reg_addr\t: monitor frc register (max 6)\n");
	len += sprintf(buf + len, "monitor_oreg idx reg_addr\t: monitor frc register (max 6)\n");
	len += sprintf(buf + len, "monitor_dump\n");
	len += sprintf(buf + len, "monitor_vf 1/0\t: monitor current vf on, off\n");
	len += sprintf(buf + len, "crc_read 0/1\t: 0: disable crc read, 1: enable crc read\n");
	len += sprintf(buf + len, "crc_en 0/1 0/1 0/1 0/1\t: mewr/merd/mcwr/vs_print crc enable\n");
	len += sprintf(buf + len, "ratio val\t: 0:112 1:213 2:2-5 3:5-6 6:1-1\n");
	len += sprintf(buf + len, "film_mode val\t: 0:video 1:22 2:32 3:3223 4:2224\n");
	len += sprintf(buf + len, "buf_num val\t: val(1 - 16)\n");
	len += sprintf(buf + len, " \t\t 5:32322 6:44\n");
	len += sprintf(buf + len, "force_mode en(0/1) hize vsize\n");
	len += sprintf(buf + len, "ud_dbg 0/1 0/1 0/1 0/1\t: meud_en,mcud_en,in,out alg time\n");
	len += sprintf(buf + len, "auto_ctrl 0/1 \t: frc auto on off work mode\n");
	len += sprintf(buf + len, "mc_lossy 0/1 \t: 0:off 1:on\n");
	len += sprintf(buf + len, "me_lossy 0/1 \t: 0:off 1:on\n");
	len += sprintf(buf + len, "powerdown : power down memc\n");
	len += sprintf(buf + len, "poweron : power on memc\n");
	len += sprintf(buf + len, "memc_level : memc_dejudder\n");

	return len;
}

void frc_debug_if(struct frc_dev_s *devp, const char *buf, size_t count)
{
	char *buf_orig, *parm[47] = {NULL};
	int val1;
	int val2;
	struct frc_fw_data_s *fw_data;

	if (!devp)
		return;

	if (!buf)
		return;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;

	frc_debug_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "status")) {
		frc_status(devp);
	} else if (!strcmp(parm[0], "dbg_level")) {
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_dbg_en = (int)val1;
		pr_frc(0, "frc_dbg_en=%d\n", frc_dbg_en);
	} else if (!strcmp(parm[0], "dump_bufcfg")) {
		frc_buf_dump_memory_size_info(devp);
		frc_buf_dump_memory_addr_info(devp);
	} else if (!strcmp(parm[0], "dump_linkbuf")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1 < 4)
				frc_buf_dump_link_tab(devp, (u32)val1);
		}
	} else if (!strcmp(parm[0], "dbg_mode")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1 < FRC_STATE_NULL) {
				frc_set_mode((u32)val1);
			}
		}
	} else if (!strcmp(parm[0], "dbg_size")) {
		if (!parm[1] || !parm[2])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->dbg_input_hsize = (u32)val1;

		if (kstrtoint(parm[2], 10, &val2) == 0)
			devp->dbg_input_vsize = (u32)val2;
	} else if (!strcmp(parm[0], "dbg_ratio")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->dbg_in_out_ratio = val1;
		pr_frc(0, "dbg_in_out_ratio:0x%x\n", devp->dbg_in_out_ratio);
	} else if (!strcmp(parm[0], "dbg_force")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->dbg_force_en = (u32)val1;
	} else if (!strcmp(parm[0], "test_pattern")) {
		if (!parm[1])
			goto exit;

		if (!strcmp(parm[1], "enable"))
			devp->frc_test_ptn = 1;
		else if (!strcmp(parm[1], "disable"))
			devp->frc_test_ptn = 0;
		frc_pattern_on(devp->frc_test_ptn);
	} else if (!strcmp(parm[0], "dump_init_reg")) {
		frc_dump_reg_tab();
	} else if (!strcmp(parm[0], "dump_fixed_reg")) {
		frc_dump_fixed_table();
	} else if (!strcmp(parm[0], "dump_buf_reg")) {
		frc_dump_buf_reg();
	} else if (!strcmp(parm[0], "dump_data")) {
		if (kstrtoint(parm[1], 16, &val1))
			goto exit;
		if (kstrtoint(parm[2], 16, &val2))
			goto exit;
		frc_dump_buf_data(devp, (u32)val1, (u32)val2);
	} else if (!strcmp(parm[0], "frc_pos")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->frc_hw_pos = (u32)val1;
		frc_init_config(devp);
		pr_frc(0, "frc_hw_pos:0x%x (0:before 1:after)\n", devp->frc_hw_pos);
	} else if (!strcmp(parm[0], "frc_pause")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->frc_fw_pause = (u32)val1;
	} else if (!strcmp(parm[0], "monitor_ireg")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1 < MONITOR_REG_MAX) {
				if (kstrtoint(parm[2], 16, &val2) == 0) {
					if (val1 < 0x3fff) {
						devp->dbg_in_reg[val1] = val2;
						devp->dbg_reg_monitor_i = 1;
					}
				}
			} else {
				devp->dbg_reg_monitor_i = 0;
			}
		}
	} else if (!strcmp(parm[0], "monitor_oreg")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1 < MONITOR_REG_MAX) {
				if (kstrtoint(parm[2], 16, &val2) == 0) {
					if (val1 < 0x3fff) {
						devp->dbg_out_reg[val1] = val2;
						devp->dbg_reg_monitor_o = 1;
					}
				}
			} else {
				devp->dbg_reg_monitor_o = 0;
			}
		}
	} else if (!strcmp(parm[0], "monitor_vf")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			devp->dbg_vf_monitor = val1;
			devp->dbg_buf_len = 0;
		}
	} else if (!strcmp(parm[0], "monitor_dump")) {
		frc_dump_monitor_data(devp);
	} else if (!strcmp(parm[0], "crc_read")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->frc_crc_data.frc_crc_read = val1;
	} else if (!strcmp(parm[0], "crc_en")) {
		if (!parm[4])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->frc_crc_data.me_wr_crc.crc_en = val1;
		if (kstrtoint(parm[2], 10, &val1) == 0)
			devp->frc_crc_data.me_rd_crc.crc_en = val1;
		if (kstrtoint(parm[3], 10, &val1) == 0)
			devp->frc_crc_data.mc_wr_crc.crc_en = val1;
		if (kstrtoint(parm[4], 10, &val1) == 0)
			devp->frc_crc_data.frc_crc_pr = val1;
		frc_crc_enable(devp);
	} else if (!strcmp(parm[0], "ratio")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->in_out_ratio = val1;
	} else if (!strcmp(parm[0], "film_mode")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->film_mode = val1;
	} else if (!strcmp(parm[0], "buf_num")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_set_buf_num((u32)val1);
	} else if (!strcmp(parm[0], "force_mode")) {
		if (!parm[3])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->force_size.force_en = val1;
		if (kstrtoint(parm[2], 10, &val1) == 0)
			devp->force_size.force_hsize = val1;
		if (kstrtoint(parm[3], 10, &val1) == 0)
			devp->force_size.force_vsize = val1;
	} else if (!strcmp(parm[0], "ud_dbg")) {
		if (!parm[4]) {
			pr_frc(0, "err:input parameters error!\n");
			goto exit;
		}
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->ud_dbg.inpud_dbg_en = val1;
		if (kstrtoint(parm[2], 10, &val1) == 0) {
			devp->ud_dbg.meud_dbg_en = val1;
			devp->ud_dbg.mcud_dbg_en = val1;
			devp->ud_dbg.vpud_dbg_en = val1;
		}
		if (kstrtoint(parm[3], 10, &val1) == 0)
			devp->ud_dbg.inud_time_en = val1;
		if (kstrtoint(parm[4], 10, &val1) == 0)
			devp->ud_dbg.outud_time_en = val1;
	} else if (!strcmp(parm[0], "auto_ctrl")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->frc_sts.auto_ctrl = val1;
	} else if (!strcmp(parm[0], "osdbit_fcolr")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_osdbit_setfalsecolor(val1);
	} else if (!strcmp(parm[0], "me_lossy")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			fw_data->frc_top_type.me_loss_en  = val1;
			cfg_me_loss(fw_data->frc_top_type.me_loss_en);
		}
	} else if (!strcmp(parm[0], "mc_lossy")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			fw_data->frc_top_type.mc_loss_en  = val1;
			cfg_me_loss(fw_data->frc_top_type.mc_loss_en);
		}
	} else if (!strcmp(parm[0], "secure_on")) {
		if (!parm[1] || !parm[2])
			goto exit;
		if (kstrtoint(parm[1], 16, &val1) == 0) {
			if (kstrtoint(parm[2], 16, &val2) == 0)
				frc_test_mm_secure_set_on(devp, val1, val2);
		}
	} else if (!strcmp(parm[0], "secure_off")) {
		frc_test_mm_secure_set_off(devp);
	} else if (!strcmp(parm[0], "prot_mode")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->prot_mode = val1;
	} else if (!strcmp(parm[0], "powerdown")) {
		frc_power_domain_ctrl(devp, 0);
	} else if (!strcmp(parm[0], "poweron")) {
		frc_power_domain_ctrl(devp, 1);
	} else if (!strcmp(parm[0], "memc_level")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_memc_set_level((u8)val1);
	} else if (!strcmp(parm[0], "set_seg")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_set_seg_display((u8)val1, 8, 8, 8);
	} else if (!strcmp(parm[0], "set_demo")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_memc_set_demo((u8)val1);
	} else if (!strcmp(parm[0], "out_line")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			devp->out_line = (u32)val1;
			pr_frc(2, "set frc adj me out line is %d\n",
				devp->out_line);
		}
	} else if (!strcmp(parm[0], "chk_motion")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->ud_dbg.res0_dbg_en = val1;
	} else if (!strcmp(parm[0], "chk_vd")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->ud_dbg.res1_dbg_en = val1;
	} else if (!strcmp(parm[0], "inp_err")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->ud_dbg.res1_time_en = val1;
	} else if (!strcmp(parm[0], "vendor")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_tell_alg_vendor(val1 & 0xFF);
	} else if (!strcmp(parm[0], "mcfb")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_set_memc_fallback(val1 & 0x1F);
	} else if (!strcmp(parm[0], "filmset")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_set_film_support(val1);
	}
exit:
	kfree(buf_orig);
}

void frc_reg_io(const char *buf)
{
	char *buf_orig, *parm[8] = {NULL};
	ulong val;
	unsigned int reg;
	unsigned int regvalue;
	unsigned int len;
	int i;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return;
	frc_debug_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "r")) {
		if (!parm[1])
			goto free_buf;
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg = val;
		regvalue = READ_FRC_REG(reg);
		pr_frc(0, "[0x%x] = 0x%x\n", reg, regvalue);
	} else if (!strcmp(parm[0], "w")) {
		if (!parm[1] || !parm[2])
			goto free_buf;
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg = val;
		if (kstrtoul(parm[2], 16, &val) < 0)
			goto free_buf;
		regvalue = val;
		WRITE_FRC_REG(reg, regvalue);
		pr_frc(0, "[0x%x] = 0x%x\n", reg, regvalue);
	} else if (!strcmp(parm[0], "d")) {
		if (!parm[1] || !parm[2])
			goto free_buf;
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg = val;
		if (kstrtoul(parm[2], 16, &val) < 0)
			goto free_buf;
		len = val;
		for (i = 0; i < len; i++) {
			regvalue = READ_FRC_REG(reg + i);
			pr_frc(0, "[0x%x] = 0x%x\n", reg + i, regvalue);
		}
	}

free_buf:
	kfree(buf_orig);
}

void frc_tool_dbg_store(struct frc_dev_s *devp, const char *buf)
{
	char *buf_orig, *parm[8] = {NULL};
	ulong val;
	unsigned int reg;
	unsigned int regvalue;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return;
	frc_debug_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "r")) {
		if (!parm[1])
			goto free_buf;
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg = val;
		devp->tool_dbg.reg_read = reg;
		devp->tool_dbg.reg_read_val = READ_FRC_REG(reg);
	} else if (!strcmp(parm[0], "w")) {
		if (!parm[1] || !parm[2])
			goto free_buf;
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg = val;
		if (kstrtoul(parm[2], 16, &val) < 0)
			goto free_buf;
		regvalue = val;
		WRITE_FRC_REG(reg, regvalue);
	}

free_buf:
	kfree(buf_orig);
}

ssize_t frc_bbd_final_line_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	ssize_t len = 0;
	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	len =  fw_data->frc_alg_dbg_show(fw_data, MEMC_DBG_BBD_FINAL_LINE, buf);
	return len;
}

ssize_t frc_bbd_final_line_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	char *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	count = fw_data->frc_alg_dbg_stor(fw_data, MEMC_DBG_BBD_FINAL_LINE, buf_orig, count);
	kfree(buf_orig);
	return count;
}

ssize_t frc_vp_ctrl_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;
	ssize_t len = 0;

	len = fw_data->frc_alg_dbg_show(fw_data, MEMC_DBG_VP_CTRL, buf);
	return len;
}

ssize_t frc_vp_ctrl_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	char *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	count = fw_data->frc_alg_dbg_stor(fw_data, MEMC_DBG_VP_CTRL, buf_orig, count);
	kfree(buf_orig);
	return count;

}

ssize_t frc_logo_ctrl_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;
	ssize_t len = 0;

	len = fw_data->frc_alg_dbg_show(fw_data, MEMC_DBG_LOGO_CTRL, buf);
	return len;
}

ssize_t frc_logo_ctrl_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	char *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	count = fw_data->frc_alg_dbg_stor(fw_data, MEMC_DBG_LOGO_CTRL, buf_orig, count);
	kfree(buf_orig);
	return count;
}

ssize_t frc_iplogo_ctrl_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;
	ssize_t len = 0;

	len = fw_data->frc_alg_dbg_show(fw_data, MEMC_DBG_IPLOGO_CTRL, buf);
	return len;

}

ssize_t frc_iplogo_ctrl_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	char *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	count = fw_data->frc_alg_dbg_stor(fw_data, MEMC_DBG_IPLOGO_CTRL, buf_orig, count);
	kfree(buf_orig);
	return count;
}

ssize_t frc_melogo_ctrl_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;
	ssize_t len = 0;

	len = fw_data->frc_alg_dbg_show(fw_data, MEMC_DBG_MELOGO_CTRL, buf);
	return len;
}

ssize_t frc_melogo_ctrl_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	char *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	count = fw_data->frc_alg_dbg_stor(fw_data, MEMC_DBG_MELOGO_CTRL, buf_orig, count);
	kfree(buf_orig);
	return count;
}

ssize_t frc_sence_chg_detect_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;
	ssize_t len = 0;

	len = fw_data->frc_alg_dbg_show(fw_data, MEMC_DBG_SENCE_CHG_DETECT, buf);
	return len;

}

ssize_t frc_sence_chg_detect_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	char *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	count = fw_data->frc_alg_dbg_stor(fw_data, MEMC_DBG_SENCE_CHG_DETECT, buf_orig, count);
	kfree(buf_orig);
	return count;
}

ssize_t frc_fb_ctrl_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;
	ssize_t len = 0;

	len = fw_data->frc_alg_dbg_show(fw_data, MEMC_DBG_FB_CTRL, buf);
	return len;
}

ssize_t frc_fb_ctrl_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	char *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	count = fw_data->frc_alg_dbg_stor(fw_data, MEMC_DBG_FB_CTRL, buf_orig, count);
	kfree(buf_orig);
	return count;
}

ssize_t frc_me_ctrl_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;
	ssize_t len = 0;

	len = fw_data->frc_alg_dbg_show(fw_data, MEMC_DBG_ME_CTRL, buf);
	return len;
}

ssize_t frc_me_ctrl_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	char *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	count = fw_data->frc_alg_dbg_stor(fw_data, MEMC_DBG_ME_CTRL, buf_orig, count);
	kfree(buf_orig);
	return count;
}

ssize_t frc_search_rang_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;
	ssize_t len = 0;

	len = fw_data->frc_alg_dbg_show(fw_data, MEMC_DBG_SEARCH_RANG, buf);
	return len;
}

ssize_t frc_search_rang_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	char *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	count = fw_data->frc_alg_dbg_stor(fw_data, MEMC_DBG_SEARCH_RANG, buf_orig, count);
	kfree(buf_orig);
	return count;
}

ssize_t frc_pixel_lpf_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;
	ssize_t len = 0;

	len = fw_data->frc_alg_dbg_show(fw_data, MEMC_DBG_PIXEL_LPF, buf);
	return len;
}

ssize_t frc_pixel_lpf_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	char *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	count = fw_data->frc_alg_dbg_stor(fw_data, MEMC_DBG_PIXEL_LPF, buf_orig, count);
	kfree(buf_orig);
	return count;
}

ssize_t frc_me_rule_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;
	ssize_t len = 0;

	len = fw_data->frc_alg_dbg_show(fw_data, MEMC_DBG_ME_RULE, buf);
	return len;
}

ssize_t frc_me_rule_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	char *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	count = fw_data->frc_alg_dbg_stor(fw_data, MEMC_DBG_ME_RULE, buf_orig, count);
	kfree(buf_orig);
	return count;
}

ssize_t frc_film_ctrl_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;
	ssize_t len = 0;
	len = fw_data->frc_alg_dbg_show(fw_data, MEMC_DBG_FILM_CTRL, buf);
	return len;
}

ssize_t frc_film_ctrl_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	char *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	count = fw_data->frc_alg_dbg_stor(fw_data, MEMC_DBG_FILM_CTRL, buf_orig, count);
	kfree(buf_orig);
	return count;
}

ssize_t frc_glb_ctrl_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;
	ssize_t len = 0;

	len = fw_data->frc_alg_dbg_show(fw_data, MEMC_DBG_GLB_CTRL, buf);
	return len;
}

ssize_t frc_glb_ctrl_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	char *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data = (struct frc_fw_data_s *)devp->fw_data;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	count = fw_data->frc_alg_dbg_stor(fw_data, MEMC_DBG_GLB_CTRL, buf_orig, count);
	kfree(buf_orig);
	return count;
}

