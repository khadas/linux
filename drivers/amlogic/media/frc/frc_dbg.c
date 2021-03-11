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
	pr_frc(0, "frs state:%d (%s) new:%d\n", devp->frc_sts.state,
	       frc_state_ary[devp->frc_sts.state], devp->frc_sts.new_state);
	pr_frc(0, "vf in_hsize=%d in_vsize=%d\n", devp->in_sts.in_hsize, devp->in_sts.in_vsize);
	pr_frc(0, "dbg en:%d in_out_ratio=0x%x\n", devp->dbg_force_en, devp->dbg_in_out_ratio);
	pr_frc(0, "dbg hsize=%d vsize=%d\n", devp->dbg_input_hsize, devp->dbg_input_vsize);
	pr_frc(0, "vf_sts:%d, vf_type:0x%x, signal_type=0x%x, source_type=0x%x\n",
	       devp->in_sts.vf_sts,
	       devp->in_sts.vf_type, devp->in_sts.signal_type, devp->in_sts.source_type);
	pr_frc(0, "duration=%d\n", devp->in_sts.duration);
	pr_frc(0, "vout hsize:%d vsize:%d\n", devp->out_sts.vout_width, devp->out_sts.vout_height);
	pr_frc(0, "mem_alloced:%d size:0x%x\n", devp->buf.cma_mem_alloced,
	       devp->buf.cma_mem_size);

	pr_frc(0, "vpu int duration:%d timestamp:%ld\n",
	       devp->vs_duration, (ulong)devp->vs_timestamp);
	pr_frc(0, "frc in duration:%d timestamp:%ld\n",
	       devp->in_sts.vs_duration, (ulong)devp->in_sts.vs_timestamp);
	pr_frc(0, "frc out duration:%d timestamp:%ld\n",
	       devp->out_sts.vs_duration, (ulong)devp->out_sts.vs_timestamp);
	pr_frc(0, "in vs_cnt:%d, vs_tsk_cnt:%d\n", devp->in_sts.vs_cnt, devp->in_sts.vs_tsk_cnt);
	pr_frc(0, "out vs_cnt:%d, vs_tsk_cnt:%d\n", devp->out_sts.vs_cnt, devp->out_sts.vs_tsk_cnt);
	pr_frc(0, "frc_st vs_cnt:%d\n", devp->frc_sts.vs_cnt);

	pr_frc(0, "loss_en = %d\n", devp->loss_en);
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
}

void frc_debug_if(struct frc_dev_s *devp, const char *buf, size_t count)
{
	char *buf_orig, *parm[47] = {NULL};
	ulong val1;
	ulong val2;

	if (!buf)
		return;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return;

	frc_debug_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "status")) {
		frc_status(devp);
	} else if (!strcmp(parm[0], "dbg_level")) {
		if (kstrtol(parm[1], 10, &val1) == 0)
			frc_dbg_en = (int)val1;
		pr_frc(0, "frc_dbg_en=%d\n", frc_dbg_en);
	} else if (!strcmp(parm[0], "bufinfo")) {
		frc_buf_dump_memory_size_info(devp);
		frc_buf_dump_memory_addr_info(devp);
	} else if (!strcmp(parm[0], "dumplink")) {
		if (kstrtol(parm[1], 10, &val1) == 0) {
			if (val1 < 4)
				frc_buf_dump_link_tab(devp, (u32)val1);
		}
	} else if (!strcmp(parm[0], "dbg_mode")) {
		if (kstrtol(parm[1], 10, &val1) == 0) {
			if (val1 < FRC_STATE_NULL) {
				frc_set_mode((u32)val1);
			}
		}
	} else if (!strcmp(parm[0], "dbg_size")) {
		if (kstrtol(parm[1], 10, &val1) == 0)
			devp->dbg_input_hsize = (u32)val1;

		if (kstrtol(parm[2], 10, &val2) == 0)
			devp->dbg_input_vsize = (u32)val2;
	} else if (!strcmp(parm[0], "dbg_ratio")) {
		if (kstrtol(parm[1], 10, &val1) == 0)
			devp->dbg_in_out_ratio =
			((devp->dbg_in_out_ratio & (~0xff00))) | ((val1 << 8) & 0xff00);
		if (kstrtol(parm[2], 10, &val2) == 0)
			devp->dbg_in_out_ratio =
			((devp->dbg_in_out_ratio & (~0xff))) | ((val1 << 8) & 0xff);
		pr_frc(0, "dbg_in_out_ratio:0x%x\n", devp->dbg_in_out_ratio);
	} else if (!strcmp(parm[0], "dbg_force")) {
		if (kstrtol(parm[1], 10, &val1) == 0)
			devp->dbg_force_en = (u32)val1;
	} else if (!strcmp(parm[0], "test_pattern")) {
		if (!strcmp(parm[1], "enable"))
			devp->frc_test_ptn = 1;
		else if (!strcmp(parm[1], "disable"))
			devp->frc_test_ptn = 0;
		frc_test_pattern_cfg(devp->frc_test_ptn);
	} else if (!strcmp(parm[0], "dump_init_reg")) {
		frc_dump_reg_tab();
	} else if (!strcmp(parm[0], "dump_buf_reg")) {
		frc_dump_buf_reg();
	}

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

