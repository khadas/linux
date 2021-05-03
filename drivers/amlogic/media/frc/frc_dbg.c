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

#include "frc_reg.h"
#include "frc_common.h"
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

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	pr_frc(0, "%s\n", FRC_FW_VER);
	pr_frc(0, "probe_ok sts:%d hw_pos:%d (1:after) fw_pause:%d\n", devp->probe_ok,
		devp->frc_hw_pos, devp->frc_fw_pause);
	pr_frc(0, "frs state:%d (%s) new:%d\n", devp->frc_sts.state,
	       frc_state_ary[devp->frc_sts.state], devp->frc_sts.new_state);
	pr_frc(0, "input in_hsize=%d in_vsize=%d\n", devp->in_sts.in_hsize, devp->in_sts.in_vsize);
	pr_frc(0, "dbg en:%d in_out_ratio=0x%x\n", devp->dbg_force_en, devp->dbg_in_out_ratio);
	pr_frc(0, "dbg hsize=%d vsize=%d\n", devp->dbg_input_hsize, devp->dbg_input_vsize);
	pr_frc(0, "vf_sts:%d, vf_type:0x%x, signal_type=0x%x, source_type=0x%x\n",
	       devp->in_sts.vf_sts,
	       devp->in_sts.vf_type, devp->in_sts.signal_type, devp->in_sts.source_type);
	pr_frc(0, "duration=%d\n", devp->in_sts.duration);
	pr_frc(0, "vout hsize:%d vsize:%d\n", devp->out_sts.vout_width, devp->out_sts.vout_height);
	pr_frc(0, "vout out_framerate:%d\n", devp->out_sts.out_framerate);
	pr_frc(0, "mem_alloced:%d size:0x%x\n", devp->buf.cma_mem_alloced,
	       devp->buf.cma_mem_size);
	pr_frc(0, "vpu int vs_duration:%d timestamp:%ld\n",
	       devp->vs_duration, (ulong)devp->vs_timestamp);
	pr_frc(0, "frc in vs_duration:%d timestamp:%ld\n",
	       devp->in_sts.vs_duration, (ulong)devp->in_sts.vs_timestamp);
	pr_frc(0, "frc out vs_duration:%d timestamp:%ld\n",
	       devp->out_sts.vs_duration, (ulong)devp->out_sts.vs_timestamp);
	pr_frc(0, "int in vs_cnt:%d, vs_tsk_cnt:%d\n",
		devp->in_sts.vs_cnt, devp->in_sts.vs_tsk_cnt);
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

}

ssize_t frc_debug_if_help(struct frc_dev_s *devp, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len, "status\t: print frc internal status\n");
	len += sprintf(buf + len, "dbg_level\t: set frc debug log level (0, 1, 2 ..)\n");
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
	len += sprintf(buf + len, "ud_dbg 0/1 0/1\t: meud_en, mcud_en\n");
	len += sprintf(buf + len, "auto_ctrl 0/1 \t: frc auto on off work mode\n");
	len += sprintf(buf + len, "mc_lossy 0/1 \t: 0:off 1:on\n");
	len += sprintf(buf + len, "me_lossy 0/1 \t: 0:off 1:on\n");

	return len;
}

void frc_debug_if(struct frc_dev_s *devp, const char *buf, size_t count)
{
	char *buf_orig, *parm[47] = {NULL};
	int val1;
	int val2;
	struct frc_fw_data_s *fw_data;
	struct st_search_final_line_para *search_final_line_para;
	struct st_me_ctrl_para *me_ctrl_para;
	struct st_me_rule_en *me_rule_en;

	struct st_vp_ctrl_para *g_stvpctrl_para;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	search_final_line_para = &fw_data->search_final_line_para;
	me_ctrl_para = &fw_data->g_stMeCtrl_Para;
	me_rule_en = &fw_data->g_stMeRule_EN;
	g_stvpctrl_para = &fw_data->g_stvpctrl_para;

	if (!buf)
		return;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return;

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
		if (!parm[2])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->ud_dbg.meud_dbg_en = val1;
		if (kstrtoint(parm[2], 10, &val1) == 0)
			devp->ud_dbg.mcud_dbg_en = val1;
	} else if (!strcmp(parm[0], "auto_ctrl")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			devp->frc_sts.auto_ctrl = val1;
	} else if (!strcmp(parm[0], "me_lossy")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			fw_data->frc_top_type.me_loss_en  = val1;
	} else if (!strcmp(parm[0], "mc_lossy")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			fw_data->frc_top_type.mc_loss_en  = val1;
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
	struct st_search_final_line_para *param;
	ssize_t len = 0;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->search_final_line_para;

	len += sprintf(buf + len, "search_final_line_para info:\n");
	len += sprintf(buf + len, "bbd_en=%d\n", param->bbd_en);
	len += sprintf(buf + len, "pattern_detect_en=%d\n", param->pattern_detect_en);
	len += sprintf(buf + len, "pattern_dect_ratio=%d\n", param->pattern_dect_ratio);
	len += sprintf(buf + len, "mode_switch=%d\n", param->mode_switch);
	len += sprintf(buf + len, "ds_xyxy_force=%d\n", param->ds_xyxy_force);
	len += sprintf(buf + len, "sel2_high_mode=%d\n", param->sel2_high_mode);
	len += sprintf(buf + len, "motion_sel1_high_mode=%d\n", param->motion_sel1_high_mode);
	len += sprintf(buf + len, "black_th_gen_en=%d\n", param->black_th_gen_en);
	len += sprintf(buf + len, "black_th_max=%d\n", param->black_th_max);
	len += sprintf(buf + len, "black_th_min=%d\n", param->black_th_min);
	len += sprintf(buf + len, "edge_th_gen_en=%d\n", param->edge_th_gen_en);
	len += sprintf(buf + len, "edge_th_max=%d\n", param->edge_th_max);
	len += sprintf(buf + len, "edge_th_min=%d\n", param->edge_th_min);
	len += sprintf(buf + len, "final_iir_mode=%d\n", param->final_iir_mode);
	len += sprintf(buf + len, "bbd_7_seg_en=%d\n", param->bbd_7_seg_en);

	return len;
}

ssize_t frc_bbd_final_line_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int value = 0;
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_search_final_line_para *param;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	frc_debug_parse_param(buf_orig, (char **)(&parm));

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->search_final_line_para;

	if (!parm[1] || !parm[0]) {
		pr_frc(0, "err input param\n");
		return count;
	}
	rc = kstrtoint(parm[1], 10, &value);

	if (!strcmp(parm[0], "bbd_en"))
		param->bbd_en = value;
	else if (!strcmp(parm[0], "pattern_detect_en"))
		param->pattern_detect_en = value;
	else if (!strcmp(parm[0], "pattern_dect_ratio"))
		param->pattern_dect_ratio = value;
	else if (!strcmp(parm[0], "mode_switch"))
		param->mode_switch = value;
	else if (!strcmp(parm[0], "ds_xyxy_force"))
		param->ds_xyxy_force = value;
	else if (!strcmp(parm[0], "sel2_high_mode"))
		param->sel2_high_mode = value;
	else if (!strcmp(parm[0], "motion_sel1_high_mode"))
		param->motion_sel1_high_mode = value;
	else if (!strcmp(parm[0], "black_th_gen_en"))
		param->black_th_gen_en = value;
	else if (!strcmp(parm[0], "black_th_max"))
		param->black_th_max = value;
	else if (!strcmp(parm[0], "black_th_min"))
		param->black_th_min = value;
	else if (!strcmp(parm[0], "edge_th_gen_en"))
		param->edge_th_gen_en = value;
	else if (!strcmp(parm[0], "edge_th_max"))
		param->edge_th_max = value;
	else if (!strcmp(parm[0], "edge_th_min"))
		param->edge_th_min = value;
	else if (!strcmp(parm[0], "final_iir_mode"))
		param->final_iir_mode = value;
	else if (!strcmp(parm[0], "bbd_7_seg_en"))
		param->bbd_7_seg_en = value;


	kfree(buf_orig);
	return count;
}

ssize_t frc_vp_ctrl_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_vp_ctrl_para *param;
	ssize_t len = 0;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stvpctrl_para;

	len += sprintf(buf + len, "g_stvpctrl_para info:\n");
	len += sprintf(buf + len, "vp_en=%d\n", param->vp_en);
	len += sprintf(buf + len, "vp_ctrl_en=%d\n", param->vp_ctrl_en);
	len += sprintf(buf + len, "global_oct_cnt_th=%d\n", param->global_oct_cnt_th);
	len += sprintf(buf + len, "global_oct_cnt_en=%d\n", param->global_oct_cnt_en);
	len += sprintf(buf + len, "region_oct_cnt_en=%d\n", param->region_oct_cnt_en);
	len += sprintf(buf + len, "region_oct_cnt_th=%d\n", param->region_oct_cnt_th);
	len += sprintf(buf + len, "global_dtl_cnt_th=%d\n", param->global_dtl_cnt_th);
	len += sprintf(buf + len, "global_dtl_cnt_en=%d\n", param->global_dtl_cnt_en);
	len += sprintf(buf + len, "mv_activity_th=%d\n", param->mv_activity_th);
	len += sprintf(buf + len, "mv_activity_en=%d\n", param->mv_activity_en);
	len += sprintf(buf + len, "global_t_consis_th=%d\n", param->global_t_consis_th);
	len += sprintf(buf + len, "global_t_consis_en=%d\n", param->global_t_consis_en);
	len += sprintf(buf + len, "region_t_consis_th=%d\n", param->region_t_consis_th);
	len += sprintf(buf + len, "region_t_consis_en=%d\n", param->region_t_consis_en);
	len += sprintf(buf + len, "occl_mv_diff_th=%d\n", param->occl_mv_diff_th);
	len += sprintf(buf + len, "occl_mv_diff_en=%d\n", param->occl_mv_diff_en);
	len += sprintf(buf + len, "occl_exist_most_th=%d\n", param->occl_exist_most_th);
	len += sprintf(buf + len, "occl_exist_most_en=%d\n", param->occl_exist_most_en);
	len += sprintf(buf + len, "occl_exist_region_th=%d\n", param->occl_exist_region_th);
	len += sprintf(buf + len, "add_7_flag_en=%d\n", param->add_7_flag_en);

	return len;
}

ssize_t frc_vp_ctrl_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int value = 0;
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_vp_ctrl_para *param;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	frc_debug_parse_param(buf_orig, (char **)(&parm));

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stvpctrl_para;

	if (!parm[1] || !parm[0]) {
		pr_frc(0, "err input param\n");
		return count;
	}
	rc = kstrtoint(parm[1], 10, &value);

	if (!strcmp(parm[0], "vp_en"))
		param->vp_en = value;
	else if (!strcmp(parm[0], "vp_ctrl_en"))
		param->vp_ctrl_en = value;
	else if (!strcmp(parm[0], "global_oct_cnt_th"))
		param->global_oct_cnt_th = value;
	else if (!strcmp(parm[0], "global_oct_cnt_en"))
		param->global_oct_cnt_en = value;
	else if (!strcmp(parm[0], "region_oct_cnt_th"))
		param->region_oct_cnt_th = value;
	else if (!strcmp(parm[0], "global_dtl_cnt_th"))
		param->global_dtl_cnt_th = value;
	else if (!strcmp(parm[0], "global_dtl_cnt_en"))
		param->global_dtl_cnt_en = value;
	else if (!strcmp(parm[0], "mv_activity_th"))
		param->mv_activity_th = value;
	else if (!strcmp(parm[0], "mv_activity_en"))
		param->mv_activity_en = value;
	else if (!strcmp(parm[0], "global_t_consis_th"))
		param->global_t_consis_th = value;
	else if (!strcmp(parm[0], "global_t_consis_en"))
		param->global_t_consis_en = value;
	else if (!strcmp(parm[0], "region_t_consis_th"))
		param->region_t_consis_th = value;
	else if (!strcmp(parm[0], "region_t_consis_en"))
		param->region_t_consis_en = value;
	else if (!strcmp(parm[0], "occl_mv_diff_th"))
		param->occl_mv_diff_th = value;
	else if (!strcmp(parm[0], "occl_mv_diff_en"))
		param->occl_mv_diff_en = value;
	else if (!strcmp(parm[0], "occl_exist_most_th"))
		param->occl_exist_most_th = value;
	else if (!strcmp(parm[0], "occl_exist_most_en"))
		param->occl_exist_most_en = value;
	else if (!strcmp(parm[0], "occl_exist_region_th"))
		param->occl_exist_region_th = value;
	else if (!strcmp(parm[0], "add_7_flag_en"))
		param->add_7_flag_en = value;

	kfree(buf_orig);
	return count;
}

ssize_t frc_iplogo_ctrl_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_iplogo_ctrl_para *param;
	ssize_t len = 0;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stiplogoctrl_para;

	len += sprintf(buf + len, "st_iplogo_ctrl_para info:\n");
	len += sprintf(buf + len, "xsize=%d\n", param->xsize);
	len += sprintf(buf + len, "ysize=%d\n", param->ysize);
	len += sprintf(buf + len, "gmv_invalid_check_en=%d\n", param->gmv_invalid_check_en);
	len += sprintf(buf + len, "gmv_ctrl_corr_clr_en=%d\n", param->gmv_ctrl_corr_clr_en);
	len += sprintf(buf + len, "gmv_ctrl_corr_clr_th=%d\n", param->gmv_ctrl_corr_clr_th);
	len += sprintf(buf + len, "gmv_ctrl_corr_clr_msize_coring=%d\n",
			param->gmv_ctrl_corr_clr_msize_coring);
	len += sprintf(buf + len, "gmv_ctrl_corr_clr_msize_en=%d\n",
			param->gmv_ctrl_corr_clr_msize_en);
	len += sprintf(buf + len, "fw_iplogo_en=%d\n", param->fw_iplogo_en);
	len += sprintf(buf + len, "scc_glb_clr_rate_th=%d\n", param->scc_glb_clr_rate_th);
	len += sprintf(buf + len, "area_ctrl_dir4_ratio_en=%d\n", param->area_ctrl_dir4_ratio_en);
	len += sprintf(buf + len, "area_ctrl_corr_th_en=%d\n", param->area_ctrl_corr_th_en);
	len += sprintf(buf + len, "area_th_ub=%d\n", param->area_th_ub);
	len += sprintf(buf + len, "area_th_mb=%d\n", param->area_th_mb);
	len += sprintf(buf + len, "area_th_lb=%d\n", param->area_th_lb);

	return len;
}

ssize_t frc_iplogo_ctrl_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int value = 0;
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_iplogo_ctrl_para *param;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	frc_debug_parse_param(buf_orig, (char **)(&parm));

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stiplogoctrl_para;

	if (!parm[1] || !parm[0]) {
		pr_frc(0, "err input param\n");
		return count;
	}
	rc = kstrtoint(parm[1], 10, &value);

	if (!strcmp(parm[0], "xsize"))
		param->xsize = value;
	else if (!strcmp(parm[0], "ysize"))
		param->ysize = value;
	else if (!strcmp(parm[0], "gmv_invalid_check_en"))
		param->gmv_invalid_check_en = value;
	else if (!strcmp(parm[0], "gmv_ctrl_corr_clr_en"))
		param->gmv_ctrl_corr_clr_en = value;
	else if (!strcmp(parm[0], "gmv_ctrl_corr_clr_th"))
		param->gmv_ctrl_corr_clr_th = value;
	else if (!strcmp(parm[0], "gmv_ctrl_corr_clr_msize_coring"))
		param->gmv_ctrl_corr_clr_msize_coring = value;
	else if (!strcmp(parm[0], "gmv_ctrl_corr_clr_msize_en"))
		param->gmv_ctrl_corr_clr_msize_en = value;
	else if (!strcmp(parm[0], "fw_iplogo_en"))
		param->fw_iplogo_en = value;
	else if (!strcmp(parm[0], "scc_glb_clr_rate_th"))
		param->scc_glb_clr_rate_th = value;
	else if (!strcmp(parm[0], "area_ctrl_dir4_ratio_en"))
		param->area_ctrl_dir4_ratio_en = value;
	else if (!strcmp(parm[0], "area_ctrl_corr_th_en"))
		param->area_ctrl_corr_th_en = value;
	else if (!strcmp(parm[0], "area_th_ub"))
		param->area_th_ub = value;
	else if (!strcmp(parm[0], "area_th_mb"))
		param->area_th_mb = value;
	else if (!strcmp(parm[0], "area_th_lb"))
		param->area_th_lb = value;

	kfree(buf_orig);
	return count;
}

ssize_t frc_melogo_ctrl_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_melogo_ctrl_para *param;
	ssize_t len = 0;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stmelogoctrl_para;

	len += sprintf(buf + len, "g_stmelogoctrl_para info:\n");
	len += sprintf(buf + len, "fw_melogo_en=%d\n", param->fw_melogo_en);

	return len;
}

ssize_t frc_melogo_ctrl_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int value = 0;
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_melogo_ctrl_para *param;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	frc_debug_parse_param(buf_orig, (char **)(&parm));

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stmelogoctrl_para;

	if (!parm[1] || !parm[0]) {
		pr_frc(0, "err input param\n");
		return count;
	}
	rc = kstrtoint(parm[1], 10, &value);
	if (!strcmp(parm[0], "fw_melogo_en"))
		param->fw_melogo_en = value;

	kfree(buf_orig);
	return count;
}

ssize_t frc_sence_chg_detect_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_scene_change_detect_para *param;
	ssize_t len = 0;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stScnChgDet_Para;

	len += sprintf(buf + len, "g_stScnChgDet_Para info:\n");
	len += sprintf(buf + len, "schg_det_en0=%d\n", param->schg_det_en0);
	len += sprintf(buf + len, "schg_strict_mod0=%d\n", param->schg_strict_mod0);

	return len;
}

ssize_t frc_sence_chg_detect_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int value = 0;
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_scene_change_detect_para *param;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	frc_debug_parse_param(buf_orig, (char **)(&parm));

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stScnChgDet_Para;

	if (!parm[1] || !parm[0]) {
		pr_frc(0, "err input param\n");
		return count;
	}
	rc = kstrtoint(parm[1], 10, &value);

	if (!strcmp(parm[0], "schg_det_en0"))
		param->schg_det_en0 = value;
	else if (!strcmp(parm[0], "schg_strict_mod0"))
		param->schg_strict_mod0 = value;

	kfree(buf_orig);
	return count;
}

ssize_t frc_fb_ctrl_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_fb_ctrl_para *param;
	ssize_t len = 0;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stFbCtrl_Para;

	len += sprintf(buf + len, "g_stFbCtrl_Para info:\n");
	len += sprintf(buf + len, "glb_tc_iir_up=%d\n", param->glb_tc_iir_up);
	len += sprintf(buf + len, "glb_tc_iir_dn=%d\n", param->glb_tc_iir_dn);
	len += sprintf(buf + len, "fb_level_iir_up=%d\n", param->fb_level_iir_up);
	len += sprintf(buf + len, "fb_level_iir_dn=%d\n", param->fb_level_iir_dn);
	len += sprintf(buf + len, "fb_gain_gmv_th_l=%d\n", param->fb_gain_gmv_th_l);
	len += sprintf(buf + len, "fb_gain_gmv_th_s=%d\n", param->fb_gain_gmv_th_s);
	len += sprintf(buf + len, "fb_gain_gmv_ratio_l=%d\n", param->fb_gain_gmv_ratio_l);
	len += sprintf(buf + len, "fb_gain_gmv_ratio_s=%d\n", param->fb_gain_gmv_ratio_s);
	len += sprintf(buf + len, "base_TC_th_s=%d\n", param->base_TC_th_s);
	len += sprintf(buf + len, "base_TC_th_l=%d\n", param->base_TC_th_l);
	len += sprintf(buf + len, "TC_th_iir_up=%d\n", param->TC_th_iir_up);
	len += sprintf(buf + len, "TC_th_iir_dn=%d\n", param->TC_th_iir_dn);
	len += sprintf(buf + len, "fallback_level_max=%d\n", param->fallback_level_max);
	len += sprintf(buf + len, "region_TC_iir_up=%d\n", param->region_TC_iir_up);
	len += sprintf(buf + len, "region_TC_iir_dn=%d\n", param->region_TC_iir_dn);
	len += sprintf(buf + len, "region_TC_th_iir_up=%d\n", param->region_TC_th_iir_up);
	len += sprintf(buf + len, "region_TC_th_iir_dn=%d\n", param->region_TC_th_iir_dn);
	len += sprintf(buf + len, "region_fb_level_iir_up=%d\n", param->region_fb_level_iir_up);
	len += sprintf(buf + len, "region_fb_level_iir_dn=%d\n", param->region_fb_level_iir_dn);
	len += sprintf(buf + len, "region_TC_bad_th=%d\n", param->region_TC_bad_th);
	len += sprintf(buf + len, "region_fb_level_b_th=%d\n", param->region_fb_level_b_th);
	len += sprintf(buf + len, "region_fb_level_s_th=%d\n", param->region_fb_level_s_th);
	len += sprintf(buf + len, "region_fb_level_ero_cnt_b_th=%d\n",
			param->region_fb_level_ero_cnt_b_th);
	len += sprintf(buf + len, "region_fb_level_dil_cnt_s_th=%d\n",
			param->region_fb_level_dil_cnt_s_th);
	len += sprintf(buf + len, "region_fb_level_dil_cnt_b_th=%d\n",
			param->region_fb_level_dil_cnt_b_th);
	len += sprintf(buf + len, "region_fb_gain_gmv_th_s=%d\n", param->region_fb_gain_gmv_th_s);
	len += sprintf(buf + len, "region_fb_gain_gmv_th_l=%d\n", param->region_fb_gain_gmv_th_l);
	len += sprintf(buf + len, "region_fb_gain_gmv_ratio_s=%d\n",
			param->region_fb_gain_gmv_ratio_s);
	len += sprintf(buf + len, "region_fb_gain_gmv_ratio_l=%d\n",
			param->region_fb_gain_gmv_ratio_l);
	len += sprintf(buf + len, "region_dtl_sum_th=%d\n", param->region_dtl_sum_th);
	len += sprintf(buf + len, "region_fb_ext_th=%d\n", param->region_fb_ext_th);
	len += sprintf(buf + len, "region_fb_ext_coef=%d\n", param->region_fb_ext_coef);
	len += sprintf(buf + len, "base_region_TC_th_s=%d\n", param->base_region_TC_th_s);
	len += sprintf(buf + len, "base_region_TC_th_l=%d\n", param->base_region_TC_th_l);

	return len;
}

ssize_t frc_fb_ctrl_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int value = 0;
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_fb_ctrl_para *param;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	frc_debug_parse_param(buf_orig, (char **)(&parm));

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stFbCtrl_Para;

	if (!parm[1] || !parm[0]) {
		pr_frc(0, "err input param\n");
		return count;
	}
	rc = kstrtoint(parm[1], 10, &value);

	if (!strcmp(parm[0], "glb_tc_iir_up"))
		param->glb_tc_iir_up = value;
	else if (!strcmp(parm[0], "glb_tc_iir_dn"))
		param->glb_tc_iir_dn = value;
	else if (!strcmp(parm[0], "fb_level_iir_up"))
		param->fb_level_iir_up = value;
	else if (!strcmp(parm[0], "fb_level_iir_dn"))
		param->fb_level_iir_dn = value;
	else if (!strcmp(parm[0], "fb_gain_gmv_th_l"))
		param->fb_gain_gmv_th_l = value;
	else if (!strcmp(parm[0], "fb_gain_gmv_th_s"))
		param->fb_gain_gmv_th_s = value;
	else if (!strcmp(parm[0], "fb_gain_gmv_ratio_l"))
		param->fb_gain_gmv_ratio_l = value;
	else if (!strcmp(parm[0], "fb_gain_gmv_ratio_s"))
		param->fb_gain_gmv_ratio_s = value;
	else if (!strcmp(parm[0], "base_TC_th_s"))
		param->base_TC_th_s = value;
	else if (!strcmp(parm[0], "base_TC_th_l"))
		param->base_TC_th_l = value;
	else if (!strcmp(parm[0], "TC_th_iir_up"))
		param->TC_th_iir_up = value;
	else if (!strcmp(parm[0], "TC_th_iir_dn"))
		param->TC_th_iir_dn = value;
	else if (!strcmp(parm[0], "fallback_level_max"))
		param->fallback_level_max = value;
	else if (!strcmp(parm[0], "region_TC_iir_up"))
		param->region_TC_iir_up = value;
	else if (!strcmp(parm[0], "region_TC_iir_dn"))
		param->region_TC_iir_dn = value;
	else if (!strcmp(parm[0], "region_TC_th_iir_up"))
		param->region_TC_th_iir_up = value;
	else if (!strcmp(parm[0], "region_TC_th_iir_dn"))
		param->region_TC_th_iir_dn = value;
	else if (!strcmp(parm[0], "region_fb_level_iir_up"))
		param->region_fb_level_iir_up = value;
	else if (!strcmp(parm[0], "region_fb_level_iir_dn"))
		param->region_fb_level_iir_dn = value;
	else if (!strcmp(parm[0], "region_TC_bad_th"))
		param->region_TC_bad_th = value;
	else if (!strcmp(parm[0], "region_fb_level_b_th"))
		param->region_fb_level_b_th = value;
	else if (!strcmp(parm[0], "region_fb_level_s_th"))
		param->region_fb_level_s_th = value;
	else if (!strcmp(parm[0], "region_fb_level_ero_cnt_b_th"))
		param->region_fb_level_ero_cnt_b_th = value;
	else if (!strcmp(parm[0], "region_fb_level_dil_cnt_s_th"))
		param->region_fb_level_dil_cnt_s_th = value;
	else if (!strcmp(parm[0], "region_fb_level_dil_cnt_b_th"))
		param->region_fb_level_dil_cnt_b_th = value;
	else if (!strcmp(parm[0], "region_fb_gain_gmv_th_s"))
		param->region_fb_gain_gmv_th_s = value;
	else if (!strcmp(parm[0], "region_fb_gain_gmv_th_l"))
		param->region_fb_gain_gmv_th_l = value;
	else if (!strcmp(parm[0], "region_fb_gain_gmv_ratio_s"))
		param->region_fb_gain_gmv_ratio_s = value;
	else if (!strcmp(parm[0], "region_fb_gain_gmv_ratio_l"))
		param->region_fb_gain_gmv_ratio_l = value;
	else if (!strcmp(parm[0], "region_dtl_sum_th"))
		param->region_dtl_sum_th = value;
	else if (!strcmp(parm[0], "region_dtl_sum_th"))
		param->region_dtl_sum_th = value;
	else if (!strcmp(parm[0], "region_fb_ext_coef"))
		param->region_fb_ext_coef = value;
	else if (!strcmp(parm[0], "base_region_TC_th_s"))
		param->base_region_TC_th_s = value;
	else if (!strcmp(parm[0], "base_region_TC_th_l"))
		param->base_region_TC_th_l = value;

	kfree(buf_orig);
	return count;
}

ssize_t frc_me_ctrl_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_me_ctrl_para *param;
	ssize_t len = 0;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stMeCtrl_Para;

	len += sprintf(buf + len, "g_stMeCtrl_Para info:\n");
	len += sprintf(buf + len, "me_en=%d\n", param->me_en);
	len += sprintf(buf + len, "me_rule_ctrl_en=%d\n", param->me_rule_ctrl_en);
	len += sprintf(buf + len, "update_strength_add_value=%d\n", param->update_strength_add_value);
	len += sprintf(buf + len, "scene_change_flag=%d\n", param->scene_change_flag);
	len += sprintf(buf + len, "fallback_gmvx_th=%d\n", param->fallback_gmvx_th);
	len += sprintf(buf + len, "fallback_gmvy_th=%d\n", param->fallback_gmvy_th);
	len += sprintf(buf + len, "region_sad_median_num=%d\n", param->region_sad_median_num);
	len += sprintf(buf + len, "region_sad_sum_th=%d\n", param->region_sad_sum_th);
	len += sprintf(buf + len, "region_sad_cnt_th=%d\n", param->region_sad_cnt_th);
	len += sprintf(buf + len, "region_s_consis_th=%d\n", param->region_s_consis_th);
	len += sprintf(buf + len, "region_win3x3_min=%d\n", param->region_win3x3_min);
	len += sprintf(buf + len, "region_win3x3_max=%d\n", param->region_win3x3_max);

	return len;
}

ssize_t frc_me_ctrl_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int value = 0;
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_me_ctrl_para *param;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	frc_debug_parse_param(buf_orig, (char **)(&parm));

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stMeCtrl_Para;

	if (!parm[1] || !parm[0]) {
		pr_frc(0, "err input param\n");
		return count;
	}
	rc = kstrtoint(parm[1], 10, &value);

	if (!strcmp(parm[0], "me_en"))
		param->me_en = value;
	else if (!strcmp(parm[0], "me_rule_ctrl_en"))
		param->me_rule_ctrl_en = value;
	else if (!strcmp(parm[0], "update_strength_add_value"))
		param->update_strength_add_value = value;
	else if (!strcmp(parm[0], "scene_change_flag"))
		param->scene_change_flag = value;
	else if (!strcmp(parm[0], "fallback_gmvx_th"))
		param->fallback_gmvx_th = value;
	else if (!strcmp(parm[0], "fallback_gmvy_th"))
		param->fallback_gmvy_th = value;
	else if (!strcmp(parm[0], "region_sad_median_num"))
		param->region_sad_median_num = value;
	else if (!strcmp(parm[0], "region_sad_sum_th"))
		param->region_sad_sum_th = value;
	else if (!strcmp(parm[0], "region_sad_cnt_th"))
		param->region_sad_cnt_th = value;
	else if (!strcmp(parm[0], "region_s_consis_th"))
		param->region_s_consis_th = value;
	else if (!strcmp(parm[0], "region_win3x3_min"))
		param->region_win3x3_min = value;
	else if (!strcmp(parm[0], "region_win3x3_max"))
		param->region_win3x3_max = value;

	kfree(buf_orig);
	return count;
}

ssize_t frc_search_rang_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_search_range_dynamic_para *param;
	ssize_t len = 0;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stsrch_rng_dym_para;

	len += sprintf(buf + len, "g_stsrch_rng_dym_para info:\n");
	len += sprintf(buf + len, "srch_rng_mvy_th=%d\n", param->srch_rng_mvy_th);
	len += sprintf(buf + len, "force_luma_srch_rng_en=%d\n", param->force_luma_srch_rng_en);
	len += sprintf(buf + len, "force_chrm_srch_rng_en=%d\n", param->force_chrm_srch_rng_en);
	len += sprintf(buf + len, "srch_rng_mode_cnt=%d\n", param->srch_rng_mode_cnt);
	len += sprintf(buf + len, "srch_rng_mode_h_th=%d\n", param->srch_rng_mode_h_th);
	len += sprintf(buf + len, "srch_rng_mode_l_th=%d\n", param->srch_rng_mode_l_th);
	len += sprintf(buf + len, "norm_mode_en=%d\n", param->norm_mode_en);
	len += sprintf(buf + len, "sing_up_en=%d\n", param->sing_up_en);
	len += sprintf(buf + len, "sing_dn_en=%d\n", param->sing_dn_en);
	len += sprintf(buf + len, "norm_asym_en=%d\n", param->norm_asym_en);
	len += sprintf(buf + len, "norm_vd2_en=%d\n", param->norm_vd2_en);
	len += sprintf(buf + len, "sing_up_vd2_en=%d\n", param->sing_up_vd2_en);
	len += sprintf(buf + len, "sing_dn_vd2_en=%d\n", param->sing_dn_vd2_en);
	len += sprintf(buf + len, "norm_vd2hd2_en=%d\n", param->norm_vd2hd2_en);
	len += sprintf(buf + len, "sing_up_vd2hd2_en=%d\n", param->sing_up_vd2hd2_en);
	len += sprintf(buf + len, "sing_dn_vd2hd2_en=%d\n", param->sing_dn_vd2hd2_en);
	len += sprintf(buf + len, "gred_mode_en=%d\n", param->gred_mode_en);
	len += sprintf(buf + len, "luma_norm_vect_th=%d\n", param->luma_norm_vect_th);
	len += sprintf(buf + len, "luma_sing_vect_min_th=%d\n", param->luma_sing_vect_min_th);
	len += sprintf(buf + len, "luma_sing_vect_max_th=%d\n", param->luma_sing_vect_max_th);
	len += sprintf(buf + len, "luma_asym_vect_th=%d\n", param->luma_asym_vect_th);
	len += sprintf(buf + len, "luma_gred_vect_th=%d\n", param->luma_gred_vect_th);
	len += sprintf(buf + len, "chrm_norm_vect_th=%d\n", param->chrm_norm_vect_th);
	len += sprintf(buf + len, "chrm_sing_vect_min_th=%d\n", param->chrm_sing_vect_min_th);
	len += sprintf(buf + len, "chrm_sing_vect_max_th=%d\n", param->chrm_sing_vect_max_th);
	len += sprintf(buf + len, "chrm_asym_vect_th=%d\n", param->chrm_asym_vect_th);
	len += sprintf(buf + len, "chrm_gred_vect_th=%d\n", param->chrm_gred_vect_th);
	len += sprintf(buf + len, "g_stsrch_rng_dym_para=%d\n", param->mclbuf_mode_en);

	return len;
}

ssize_t frc_search_rang_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int value = 0;
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_search_range_dynamic_para *param;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	frc_debug_parse_param(buf_orig, (char **)(&parm));

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stsrch_rng_dym_para;

	if (!parm[1] || !parm[0]) {
		pr_frc(0, "err input param\n");
		return count;
	}
	rc = kstrtoint(parm[1], 10, &value);

	if (!strcmp(parm[0], "srch_rng_mvy_th"))
		param->srch_rng_mvy_th = value;
	else if (!strcmp(parm[0], "force_luma_srch_rng_en"))
		param->force_luma_srch_rng_en = value;
	else if (!strcmp(parm[0], "force_chrm_srch_rng_en"))
		param->force_chrm_srch_rng_en = value;
	else if (!strcmp(parm[0], "srch_rng_mode_cnt"))
		param->srch_rng_mode_cnt = value;
	else if (!strcmp(parm[0], "srch_rng_mode_h_th"))
		param->srch_rng_mode_h_th = value;
	else if (!strcmp(parm[0], "srch_rng_mode_l_th"))
		param->srch_rng_mode_l_th = value;
	else if (!strcmp(parm[0], "norm_mode_en"))
		param->norm_mode_en = value;
	else if (!strcmp(parm[0], "sing_up_en"))
		param->sing_up_en = value;
	else if (!strcmp(parm[0], "sing_dn_en"))
		param->sing_dn_en = value;
	else if (!strcmp(parm[0], "norm_asym_en"))
		param->norm_asym_en = value;
	else if (!strcmp(parm[0], "norm_vd2_en"))
		param->norm_vd2_en = value;
	else if (!strcmp(parm[0], "sing_up_vd2_en"))
		param->sing_up_vd2_en = value;
	else if (!strcmp(parm[0], "sing_dn_vd2_en"))
		param->sing_dn_vd2_en = value;
	else if (!strcmp(parm[0], "norm_vd2hd2_en"))
		param->norm_vd2hd2_en = value;
	else if (!strcmp(parm[0], "sing_up_vd2hd2_en"))
		param->sing_up_vd2hd2_en = value;
	else if (!strcmp(parm[0], "gred_mode_en"))
		param->gred_mode_en = value;
	else if (!strcmp(parm[0], "luma_norm_vect_th"))
		param->luma_norm_vect_th = value;
	else if (!strcmp(parm[0], "luma_sing_vect_min_th"))
		param->luma_sing_vect_min_th = value;
	else if (!strcmp(parm[0], "luma_sing_vect_max_th"))
		param->luma_sing_vect_max_th = value;
	else if (!strcmp(parm[0], "luma_asym_vect_th"))
		param->luma_asym_vect_th = value;
	else if (!strcmp(parm[0], "luma_gred_vect_th"))
		param->luma_gred_vect_th = value;
	else if (!strcmp(parm[0], "chrm_norm_vect_th"))
		param->chrm_norm_vect_th = value;
	else if (!strcmp(parm[0], "chrm_sing_vect_min_th"))
		param->chrm_sing_vect_min_th = value;
	else if (!strcmp(parm[0], "chrm_sing_vect_max_th"))
		param->chrm_sing_vect_max_th = value;
	else if (!strcmp(parm[0], "chrm_asym_vect_th"))
		param->chrm_asym_vect_th = value;
	else if (!strcmp(parm[0], "chrm_gred_vect_th"))
		param->chrm_gred_vect_th = value;
	else if (!strcmp(parm[0], "mclbuf_mode_en"))
		param->mclbuf_mode_en = value;

	kfree(buf_orig);
	return count;
}

ssize_t frc_pixel_lpf_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_pixel_lpf_para *param;
	ssize_t len = 0;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stpixlpf_para;

	len += sprintf(buf + len, "g_stpixlpf_para info:\n");
	len += sprintf(buf + len, "pixlpf_en=%d\n", param->pixlpf_en);
	len += sprintf(buf + len, "osd_ctrl_pixlpf_en=%d\n", param->osd_ctrl_pixlpf_en);
	len += sprintf(buf + len, "osd_ctrl_pixlpf_th=%d\n", param->osd_ctrl_pixlpf_th);
	len += sprintf(buf + len, "detail_ctrl_pixlpf_en=%d\n", param->detail_ctrl_pixlpf_en);
	len += sprintf(buf + len, "detail_ctrl_pixlpf_th=%d\n", param->detail_ctrl_pixlpf_th);

	return len;
}

ssize_t frc_pixel_lpf_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int value = 0;
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_pixel_lpf_para *param;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	frc_debug_parse_param(buf_orig, (char **)(&parm));

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stpixlpf_para;

	if (!parm[1] || !parm[0]) {
		pr_frc(0, "err input param\n");
		return count;
	}
	rc = kstrtoint(parm[1], 10, &value);

	if (!strcmp(parm[0], "pixlpf_en"))
		param->pixlpf_en = value;
	else if (!strcmp(parm[0], "osd_ctrl_pixlpf_en"))
		param->osd_ctrl_pixlpf_en = value;
	else if (!strcmp(parm[0], "osd_ctrl_pixlpf_th"))
		param->osd_ctrl_pixlpf_th = value;
	else if (!strcmp(parm[0], "detail_ctrl_pixlpf_en"))
		param->detail_ctrl_pixlpf_en = value;
	else if (!strcmp(parm[0], "detail_ctrl_pixlpf_th"))
		param->detail_ctrl_pixlpf_th = value;

	kfree(buf_orig);
	return count;
}

ssize_t frc_me_rule_param_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_me_rule_en *param;
	ssize_t len = 0;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stMeRule_EN;

	len += sprintf(buf + len, "st_me_rule_en info:\n");
	len += sprintf(buf + len, "rule1_en=%d\n", param->rule1_en);
	len += sprintf(buf + len, "rule2_en=%d\n", param->rule2_en);
	len += sprintf(buf + len, "rule3_en=%d\n", param->rule3_en);
	len += sprintf(buf + len, "rule4_en=%d\n", param->rule4_en);
	len += sprintf(buf + len, "rule5_en=%d\n", param->rule5_en);
	len += sprintf(buf + len, "rule6_en=%d\n", param->rule6_en);
	len += sprintf(buf + len, "rule7_en=%d\n", param->rule7_en);
	len += sprintf(buf + len, "rule8_en=%d\n", param->rule8_en);
	len += sprintf(buf + len, "rule9_en=%d\n", param->rule9_en);
	len += sprintf(buf + len, "rule10_en=%d\n", param->rule10_en);

	return len;
}

ssize_t frc_me_rule_param_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int value = 0;
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_me_rule_en *param;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	frc_debug_parse_param(buf_orig, (char **)(&parm));

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stMeRule_EN;

	if (!parm[1] || !parm[0]) {
		pr_frc(0, "err input param\n");
		return count;
	}
	rc = kstrtoint(parm[1], 10, &value);

	if (!strcmp(parm[0], "rule1_en"))
		param->rule1_en = value;
	else if (!strcmp(parm[0], "rule2_en"))
		param->rule2_en = value;
	else if (!strcmp(parm[0], "rule3_en"))
		param->rule3_en = value;
	else if (!strcmp(parm[0], "rule4_en"))
		param->rule4_en = value;
	else if (!strcmp(parm[0], "rule5_en"))
		param->rule5_en = value;
	else if (!strcmp(parm[0], "rule6_en"))
		param->rule6_en = value;
	else if (!strcmp(parm[0], "rule7_en"))
		param->rule7_en = value;
	else if (!strcmp(parm[0], "rule8_en"))
		param->rule8_en = value;
	else if (!strcmp(parm[0], "rule9_en"))
		param->rule9_en = value;
	else if (!strcmp(parm[0], "rule10_en"))
		param->rule10_en = value;

	kfree(buf_orig);
	return count;
}

ssize_t frc_film_detect_item_show(struct class *class,
	struct class_attribute *attr,
	char *buf)
{
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_film_detect_item *param;
	ssize_t len = 0;

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stfilm_detect_item;

	len += sprintf(buf + len, "g_stfilm_detect_item info:\n");
	len += sprintf(buf + len, "film_7_seg_en=%d\n", param->film_7_seg_en);
	len += sprintf(buf + len, "frame_buf_num=%d\n", param->frame_buf_num);
	len += sprintf(buf + len, "force_mode=%d\n", param->force_mode);
	len += sprintf(buf + len, "force_mode_en=%d\n", param->force_mode_en);

	return len;
}

ssize_t frc_film_detect_item_store(struct class *class,
	struct class_attribute *attr,
	const char *buf,
	size_t count)
{
	int value = 0;
	int rc = 0;
	char *parm[2] = { NULL }, *buf_orig;
	struct frc_dev_s *devp = get_frc_devp();
	struct frc_fw_data_s *fw_data;
	struct st_film_detect_item *param;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	frc_debug_parse_param(buf_orig, (char **)(&parm));

	fw_data = (struct frc_fw_data_s *)devp->fw_data;
	param = &fw_data->g_stfilm_detect_item;

	if (!parm[1] || !parm[0]) {
		pr_frc(0, "err input param\n");
		return count;
	}
	rc = kstrtoint(parm[1], 10, &value);

	if (!strcmp(parm[0], "film_7_seg_en"))
		param->film_7_seg_en = value;
	else if (!strcmp(parm[0], "frame_buf_num"))
		param->frame_buf_num = value;
	else if (!strcmp(parm[0], "force_mode"))
		param->force_mode = value;
	else if (!strcmp(parm[0], "force_mode_en"))
		param->force_mode_en = value;

	kfree(buf_orig);
	return count;
}

