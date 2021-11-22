// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/ldim_fw.h>
#include "../../lcd_common.h"
#include "ldim_drv.h"
#include "ldim_reg.h"

#include <linux/amlogic/gki_module.h>

#define LDIM_DBG_REG_RW_MODE_NULL              0
#define LDIM_DBG_REG_RW_MODE_RN                1
#define LDIM_DBG_REG_RW_MODE_RS                2
#define LDIM_DBG_REG_RW_MODE_WM                3
#define LDIM_DBG_REG_RW_MODE_WN                4
#define LDIM_DBG_REG_RW_MODE_WS                5
#define LDIM_DBG_REG_RW_MODE_ERR               6

#define LDIM_DBG_REG_FLAG_VCBUS                0
#define LDIM_DBG_REG_FLAG_APBBUS               1

/* 1: unlocked, 0: locked, negative: locked, possible waiters */
static struct mutex ldim_dbg_mutex;
/*for dbg reg use*/
struct ldim_dbg_reg_s {
	unsigned int rw_mode;
	unsigned int bus_flag;
	unsigned int addr;
	unsigned int size;
};

#define LDIM_DBG_ATTR_CMD_NULL                0
#define LDIM_DBG_ATTR_CMD_RD                  1
#define LDIM_DBG_ATTR_CMD_WR                  2
#define LDIM_DBG_ATTR_CMD_ERR                 3

#define LDIM_DBG_ATTR_MODE_SINGLE             0
#define LDIM_DBG_ATTR_MODE_BL_MATRIX          1
#define LDIM_DBG_ATTR_MODE_TEST_MATRIX        2
#define LDIM_DBG_ATTR_MODE_SEG_HIST           3
#define LDIM_DBG_ATTR_MODE_GLB_HIST           4
#define LDIM_DBG_ATTR_MODE_MAX_RGB            5
#define LDIM_DBG_ATTR_MODE_GAIN_LUT           6
#define LDIM_DBG_ATTR_MODE_MIN_GAIN_LUT       7
#define LDIM_DBG_ATTR_MODE_DTH_LUT            8
#define LDIM_DBG_ATTR_MODE_BL_DIM_CURVE       9
#define LDIM_DBG_ATTR_MODE_LD_WHIST           10

/*for dbg cmd use*/
struct ldim_dbg_attr_s {
	unsigned int cmd;
	unsigned int chip_type;
	unsigned int mode;
	unsigned int data;
};

static struct ldim_dbg_reg_s dbg_reg = {
	.rw_mode = LDIM_DBG_REG_RW_MODE_NULL,
	.bus_flag = LDIM_DBG_REG_FLAG_VCBUS,
	.addr = 0,
	.size = 0,
};

struct ldim_dbg_attr_s dbg_attr = {
	.cmd = LDIM_DBG_ATTR_CMD_NULL,
	.chip_type = LCD_CHIP_MAX,
	.mode = LDIM_DBG_ATTR_MODE_SINGLE,
	.data = 0,
};

static int ldim_debug_buf_save(char *path, unsigned char *save_buf,
			       unsigned int size)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	mm_segment_t old_fs = get_fs();

	if (!save_buf) {
		LDIMERR("%s: save_buf is null\n", __func__);
		return -1;
	}
	if (size == 0) {
		LDIMERR("%s: size is zero\n", __func__);
		return -1;
	}

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	if (IS_ERR(filp)) {
		LDIMERR("%s: create %s error\n", __func__, path);
		set_fs(old_fs);
		return -1;
	}

	pos = 0;
	buf = (void *)save_buf;
	vfs_write(filp, buf, size, &pos);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);

	pr_info("debug file %s save finished\n", path);

	return 0;
}

static void ldim_int_table_print(unsigned int n, unsigned int *table)
{
	int i = 0, len;
	char *buf;

	if (!table) {
		LDIMERR("table is null\n");
		return;
	}

	len = n * 8 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	pr_info("table:\n");
	len = 0;
	for (i = 0; i < n; i++)
		len += sprintf(buf + len, "\t%d", table[i]);
	pr_info("%s\n", buf);

	kfree(buf);
}

static void ldim_comp_gain_lut_print(int index)
{
	int i, j, len;
	char *buf;

	len = 32 * 8 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	if (index == 0xff) {
		pr_info("comp gain lut:\n");
		for (i = 0; i < 16; i++) {
			pr_info("  %d:\n", i);
			len = 0;
			for (j = 0; j < 32; j++) {
				if (j == 16)
					len += sprintf(buf + len, "\n");
				len += sprintf(buf + len, "\t%d",
					ld_remap_lut[i][j]);
			}
			pr_info("%s\n", buf);
		}
	} else if (index < 16) {
		pr_info("comp gain lut %d:\n", index);
		len = 0;
		for (j = 0; j < 32; j++) {
			if (j == 16)
				len += sprintf(buf + len, "\n");
			len += sprintf(buf + len, "\t%d",
				ld_remap_lut[index][j]);
		}
		pr_info("%s\n", buf);
	} else {
		pr_info("comp gain lut invalid index %d\n", index);
	}
	pr_info("\n");

	kfree(buf);
}

static void ldim_min_gain_lut_print(void)
{
	int j, len;
	unsigned int *table;
	char *buf;

	len = 64 * 8 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	pr_info("min_gain_lut:\n");
	len = 0;
	table = ldc_min_gain_lut;
	for (j = 0; j < 64; j++) {
		if (j == 16)
			len += sprintf(buf + len, "\n");
		len += sprintf(buf + len, " %d", table[j]);
	}
	pr_info("%s\n", buf);

	pr_info("\n");

	kfree(buf);
}

static void ldim_dither_lut_print(void)
{
	int i, j, len;
	char *buf;

	len = 16 * 8 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	pr_info("dither_lut:\n");

	for (i = 0; i < 32; i++) {
		len = 0;
		for (j = 0; j < 16; j++)
			len += sprintf(buf + len, " %d", ldc_dither_lut[i][j]);
		pr_info("%s\n", buf);
	}

	pr_info("\n");

	kfree(buf);
}

static void ldim_dump_histgram(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, k, n, len;
	unsigned int *p = NULL;
	char *buf;

	n = 16 * 10 + 20;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = ldim_drv->conf->seg_row * ldim_drv->conf->seg_col * 16;
	p = kcalloc(len, sizeof(unsigned int), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return;
	}
	memcpy(p, ldim_drv->stts->hist_matrix, len * sizeof(unsigned int));

	for (i = 0; i < ldim_drv->conf->seg_row; i++) {
		pr_info("%s: row %d:\n", __func__, i);
		for (j = 0; j < ldim_drv->conf->seg_col; j++) {
			len = sprintf(buf, "col %d:\n", j);
			for (k = 0; k < 16; k++) {
				n = i * 16 * ldim_drv->conf->seg_col
					+ j * 16 + k;
				len += sprintf(buf + len,
					"\t0x%x", *(p + n));
				if (k == 7)
					len += sprintf(buf + len, "\n");
			}
			pr_info("%s\n\n", buf);
		}
	}

	kfree(buf);
	kfree(p);
}

static void ldim_dump_histgram_t7(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, k, n, size;
	struct ldim_seg_hist_s *seg_hist = NULL;
	char *buf;

	n = 256 * 10 + 48;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	size = ldim_drv->conf->seg_col * ldim_drv->conf->seg_row;
	seg_hist = kcalloc(size, sizeof(struct ldim_seg_hist_s), GFP_KERNEL);
	if (!seg_hist)
		goto ldim_dump_histgram_t7_err;

	memcpy(seg_hist, ldim_drv->stts->seg_hist,
	       size * sizeof(struct ldim_seg_hist_s));

	pr_info("%s: seg_hist:\n", __func__);
	for (i = 0; i < ldim_drv->conf->seg_row; i++) {
		pr_info("row[%d]:\n", i);
		n = 0;
		for (j = 0; j < ldim_drv->conf->seg_col; j++) {
			k = i * ldim_drv->conf->seg_col + j;
			n += sprintf(buf + n, "  %6u %6u %6u %6u\n",
				seg_hist[k].weight_avg,
				seg_hist[k].weight_avg_95,
				seg_hist[k].max_index,
				seg_hist[k].min_index);
		}
		pr_info("%s\n", buf);
	}
	pr_info("\n");

	kfree(seg_hist);
ldim_dump_histgram_t7_err:
	kfree(buf);
}

static void ldim_dump_matrix_info_max_rgb(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, n, len;
	unsigned int *p = NULL;
	char *buf;

	n = ldim_drv->conf->seg_col * 30 + 20;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = ldim_drv->conf->seg_col * ldim_drv->conf->seg_row * 3;
	p = kcalloc(len, sizeof(unsigned int), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return;
	}
	memcpy(p, ldim_drv->stts->max_rgb, len * sizeof(unsigned int));

	pr_info("%s max_rgb:\n", __func__);
	for (i = 0; i < ldim_drv->conf->seg_row; i++) {
		len = 0;
		for (j = 0; j < ldim_drv->conf->seg_col; j++) {
			n =  i * ldim_drv->conf->seg_col + j;
			len += sprintf(buf + len, "\t(R:%4d, G:%4d, B:%4d)",
				       p[n * 3],
				       p[n * 3 + 1],
				       p[n * 3 + 2]);
			if ((j + 1) % 4 == 0)
				len += sprintf(buf + len, "\n\n");
		}
		pr_info("%s\n\n", buf);
	}

	kfree(buf);
	kfree(p);
}

static void ldim_dump_bl_matrix(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, n, len;
	unsigned int *local_buf = NULL;
	unsigned int *spi_buf = NULL;
	char *buf;

	n = ldim_drv->conf->seg_col * 10 + 20;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = ldim_drv->conf->seg_row * ldim_drv->conf->seg_col;
	local_buf = kcalloc(len, sizeof(unsigned int), GFP_KERNEL);
	if (!local_buf) {
		kfree(buf);
		return;
	}
	spi_buf = kcalloc(len, sizeof(unsigned int), GFP_KERNEL);
	if (!spi_buf) {
		kfree(buf);
		kfree(local_buf);
		return;
	}

	memcpy(local_buf, &ldim_drv->local_bl_matrix[0],
	       len * sizeof(unsigned int));
	memcpy(spi_buf, &ldim_drv->bl_matrix_cur[0],
	       len * sizeof(unsigned int));

	pr_info("%s: (bl_matrix_dbg = %d)\n",
		__func__, ldim_drv->fw->bl_matrix_dbg);
	for (i = 0; i < ldim_drv->conf->seg_row; i++) {
		len = 0;
		for (j = 0; j < ldim_drv->conf->seg_col; j++) {
			n = ldim_drv->conf->seg_col * i + j;
			len += sprintf(buf + len, "\t%4d", local_buf[n]);
		}
		pr_info("%s\n", buf);
	}

	pr_info("current black_frm: %d, litgain: %d\n",
		ldim_drv->fw->ctrl->black_frm,
		ldim_drv->litgain);
	pr_info("ldim_dev brightness transfer_matrix:\n");
	for (i = 0; i < ldim_drv->conf->seg_row; i++) {
		len = 0;
		for (j = 0; j < ldim_drv->conf->seg_col; j++) {
			n = ldim_drv->conf->seg_col * i + j;
			len += sprintf(buf + len, "\t%4d", spi_buf[n]);
		}
		pr_info("%s\n", buf);
	}
	pr_info("\n");

	kfree(buf);
	kfree(local_buf);
	kfree(spi_buf);
}

static void ldim_get_test_matrix_info(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, n, len;
	unsigned int *temp_buf = ldim_drv->test_matrix;
	char *buf;

	n = ldim_drv->conf->seg_col * ldim_drv->conf->seg_row;
	len = n * 10 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	pr_info("%s:\n", __func__);
	pr_info("ldim test_mode: %d, test_matrix:\n", ldim_drv->test_bl_en);
	len = 0;
	for (i = 1; i < n; i++)
		len += sprintf(buf + len, "\t%4d", temp_buf[i]);
	pr_info("%s\n", buf);

	kfree(buf);
}

static void ldim_stts_data_dump(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, k, m, n, hist_len, max_len;
	unsigned int *p_hist = NULL, *p_max = NULL;
	char *buf;

	n = 16 * 20 + 20;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	hist_len = ldim_drv->conf->seg_row * ldim_drv->conf->seg_col * 16;
	p_hist = kcalloc(hist_len, sizeof(unsigned int), GFP_KERNEL);
	if (!p_hist)
		goto ldim_stts_data_dump_exit_0;

	max_len = ldim_drv->conf->seg_col * ldim_drv->conf->seg_row * 3;
	p_max = kcalloc(max_len, sizeof(unsigned int), GFP_KERNEL);
	if (!p_max)
		goto ldim_stts_data_dump_exit_1;

	memcpy(p_hist, ldim_drv->stts->hist_matrix,
	       hist_len * sizeof(unsigned int));
	memcpy(p_max, ldim_drv->stts->max_rgb, max_len * sizeof(unsigned int));

	pr_info("%s: hist data:\n", __func__);
	for (i = 0; i < ldim_drv->conf->seg_row; i++) {
		for (j = 0; j < ldim_drv->conf->seg_col; j++) {
			n = 0;
			m = (i * ldim_drv->conf->seg_col + j) * 16;
			for (k = 0; k < 16; k++)
				n += sprintf(buf + n, " 0x%x,", p_hist[m + k]);
			pr_info("%s\n", buf);
		}
		pr_info("\n");
	}

	pr_info("\n%s: maxrgb data:\n", __func__);
	for (i = 0; i < ldim_drv->conf->seg_row; i++) {
		n = 0;
		for (j = 0; j < ldim_drv->conf->seg_col; j++) {
			m = (i * ldim_drv->conf->seg_col + j) * 3;
			n += sprintf(buf + n, "  %d, %d, %d,",
				     p_max[m], p_max[m + 1], p_max[m + 2]);
		}
		pr_info("%s\n", buf);
	}

	kfree(p_max);
ldim_stts_data_dump_exit_1:
	kfree(p_hist);
ldim_stts_data_dump_exit_0:
	kfree(buf);
}

static void ldim_stts_data_dump_t7(struct aml_ldim_driver_s *ldim_drv)
{
	unsigned int i, j, k, n, size, glb_len;
	unsigned int *glb_hist = NULL;
	struct ldim_seg_hist_s *seg_hist = NULL;
	unsigned int glb_sum, glb_cnt, glb_apl;
	char *buf;

	n = 80 * 20 + 20;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	glb_len = 64;
	glb_hist = kcalloc(glb_len, sizeof(unsigned int), GFP_KERNEL);
	if (!glb_hist)
		goto ldim_stts_data_dump_t7_exit_0;

	size = ldim_drv->conf->seg_col * ldim_drv->conf->seg_row;
	seg_hist = kcalloc(size, sizeof(struct ldim_seg_hist_s), GFP_KERNEL);
	if (!seg_hist)
		goto ldim_stts_data_dump_t7_exit_1;

	memcpy(glb_hist, ldim_drv->stts->global_hist,
	       glb_len * sizeof(unsigned int));
	memcpy(seg_hist, ldim_drv->stts->seg_hist,
	       size * sizeof(struct ldim_seg_hist_s));
	glb_sum = ldim_drv->stts->global_hist_sum;
	glb_cnt = ldim_drv->stts->global_hist_cnt;
	glb_apl = ldim_drv->stts->global_apl;

	n = 0;
	pr_info("%s: glb_hist:\n", __func__);
	for (i = 0; i < glb_len; i++) {
		n += sprintf(buf + n, "  0x%08x\n", glb_hist[i]);
		if ((i + 1) % 8 == 0)
			n += sprintf(buf + n, "\n");
	}
	pr_info("%s\n", buf);
	pr_info("glb_hist sum=0x%x, cnt=0x%x, apl=%d\n\n",
		glb_sum, glb_cnt, glb_apl);

	pr_info("%s: seg_hist:\n", __func__);
	for (i = 0; i < ldim_drv->conf->seg_row; i++) {
		pr_info("row[%d]:\n", i);
		n = 0;
		for (j = 0; j < ldim_drv->conf->seg_col; j++) {
			k = i * ldim_drv->conf->seg_col + j;
			n += sprintf(buf + n, "  %6u %6u %6u %6u\n",
				seg_hist[k].weight_avg,
				seg_hist[k].weight_avg_95,
				seg_hist[k].max_index,
				seg_hist[k].min_index);
		}
		pr_info("%s\n", buf);
	}
	pr_info("\n");

	kfree(seg_hist);
ldim_stts_data_dump_t7_exit_1:
	kfree(glb_hist);
ldim_stts_data_dump_t7_exit_0:
	kfree(buf);
}

static int ldim_int_table_dbg_print(char *buf, unsigned int n,
				    unsigned int *table)
{
	unsigned int i, len = 0;

	len = sprintf(buf, "for_tool:");
	for (i = 0; i < n; i++)
		len += sprintf(buf + len, " %d", table[i]);
	len += sprintf(buf + len, "\n");

	return len;
}

static int ldim_gain_lut_dbg_line_print(char *buf, unsigned int index)
{
	unsigned int j, k, len = 0;
	unsigned int *table;

	if (index >= 16)
		return sprintf(buf, "invalid index: %d", index);

	if (dbg_attr.chip_type == LCD_CHIP_T7 ||
	    dbg_attr.chip_type == LCD_CHIP_T3) {
		k = 64;
		table = ldc_gain_lut_array[index];
	} else {
		k = 32;
		table = ld_remap_lut[index];
	}
	len = sprintf(buf, "for_tool: %d", k);
	for (j = 0; j < k; j++)
		len += sprintf(buf + len, " %d", table[j]);
	len += sprintf(buf + len, "\n");

	return len;
}

static int ldim_min_gain_lut_dbg_print(char *buf)
{
	unsigned int j, len = 0;
	unsigned int *table;

	len = sprintf(buf, "for_tool:");
	table = ldc_min_gain_lut;
	for (j = 0; j < 64; j++)
		len += sprintf(buf + len, " %d", table[j]);
	len += sprintf(buf + len, "\n");

	return len;
}

static void ldim_dither_lut_file_save(char *path)
{
	unsigned int i, j, len;
	char *buf;

	len = 32 * 16 * 4 + 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = sprintf(buf, "for_tool: 32 16");
	for (i = 0; i < 32; i++) {
		for (j = 0; j < 16; j++)
			len += sprintf(buf + len, " %d", ldc_dither_lut[i][j]);
	}
	len += sprintf(buf + len, "\n");

	ldim_debug_buf_save(path, buf, len);

	kfree(buf);
}

static void ldim_bl_matrix_file_save(struct aml_ldim_driver_s *ldim_drv,
				     char *path)
{
	unsigned int i, n, len;
	unsigned int *p = NULL;
	char *buf;

	len = ldim_drv->conf->seg_row * ldim_drv->conf->seg_col * 8 + 30;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	n = ldim_drv->conf->seg_row * ldim_drv->conf->seg_col;
	p = kcalloc(n, sizeof(unsigned int), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return;
	}
	memcpy(p, ldim_drv->local_bl_matrix, (n * sizeof(unsigned int)));

	len = sprintf(buf, "for_tool: %d %d",
		      ldim_drv->conf->seg_row, ldim_drv->conf->seg_col);
	for (i = 0; i < n; i++)
		len += sprintf(buf + len, " %d", p[i]);
	len += sprintf(buf + len, "\n");

	ldim_debug_buf_save(path, buf, len);

	kfree(buf);
	kfree(p);
}

static void ldim_hist_file_save(struct aml_ldim_driver_s *ldim_drv, char *path)
{
	unsigned int i, j, k, n, len, seg_row, seg_col;
	unsigned int *p = NULL;
	char *buf;

	seg_row = ldim_drv->conf->seg_row;
	seg_col = ldim_drv->conf->seg_col;

	n = seg_row * seg_col * 16;
	len = n * 10 + 30;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	p = kcalloc(n, sizeof(unsigned int), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return;
	}

	memcpy(p, ldim_drv->stts->hist_matrix, n * sizeof(unsigned int));

	len = sprintf(buf, "for_tool: %d 16", seg_row * seg_col);
	for (i = 0; i < seg_row; i++) {
		for (j = 0; j < seg_col; j++) {
			for (k = 0; k < 16; k++) {
				n = i * 16 * seg_col + j * 16 + k;
				len += sprintf(buf + len, " 0x%x", *(p + n));
			}
		}
	}
	len += sprintf(buf + len, "\n");

	ldim_debug_buf_save(path, buf, len);

	kfree(buf);
	kfree(p);
}

static void ldim_hist_file_save_t7(struct aml_ldim_driver_s *ldim_drv,
				   char *path)
{
	unsigned int i, j, k, n, len;
	struct ldim_seg_hist_s *p = NULL;
	char *buf;

	n = ldim_drv->conf->seg_row * ldim_drv->conf->seg_col;
	len = n * 4 * 10 + 30;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	p = kcalloc(n, sizeof(*p), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return;
	}

	memcpy(p, ldim_drv->stts->seg_hist, n * sizeof(struct ldim_seg_hist_s));

	len = sprintf(buf, "for_tool: %d 4", n);
	for (i = 0; i < ldim_drv->conf->seg_row; i++) {
		for (j = 0; j < ldim_drv->conf->seg_col; j++) {
			k = i * ldim_drv->conf->seg_col + j;
			len += sprintf(buf + len, " 0x%x 0x%x 0x%x 0x%x",
				       p[k].weight_avg,
				       p[k].weight_avg_95,
				       p[k].max_index,
				       p[k].min_index);
		}
	}
	len += sprintf(buf + len, "\n");

	ldim_debug_buf_save(path, buf, len);

	kfree(buf);
	kfree(p);
}

static void ldim_max_rgb_file_save(struct aml_ldim_driver_s *ldim_drv,
				   char *path)
{
	unsigned int i, j, n, len;
	unsigned int *p = NULL;
	char *buf;

	n = ldim_drv->conf->seg_col * ldim_drv->conf->seg_row * 3 * 30 + 30;
	buf = kcalloc(n, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = ldim_drv->conf->seg_col * ldim_drv->conf->seg_row * 3;
	p = kcalloc(len, sizeof(unsigned int), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return;
	}
	memcpy(p, ldim_drv->stts->max_rgb, len * sizeof(unsigned int));

	len = sprintf(buf, "for_tool: %d 3",
		      ldim_drv->conf->seg_row * ldim_drv->conf->seg_col);
	for (i = 0; i < ldim_drv->conf->seg_row; i++) {
		for (j = 0; j < ldim_drv->conf->seg_col; j++) {
			n = i * ldim_drv->conf->seg_col + j;
			len += sprintf(buf + len, " %d %d %d",
				       p[n * 3], p[n * 3 + 1], p[n * 3 + 2]);
		}
	}
	len += sprintf(buf + len, "\n");

	ldim_debug_buf_save(path, buf, len);

	kfree(buf);
	kfree(p);
}

static void ldim_test_matrix_file_save(struct aml_ldim_driver_s *ldim_drv,
				       char *path)
{
	unsigned int i, n, len;
	unsigned int *p = ldim_drv->test_matrix;
	char *buf;

	n = ldim_drv->conf->seg_col * ldim_drv->conf->seg_row;
	len = n * 10 + 30;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = sprintf(buf, "for_tool: %d %d",
		      ldim_drv->conf->seg_row, ldim_drv->conf->seg_col);
	for (i = 0; i < n; i++)
		len += sprintf(buf + len, " %d", p[i]);
	len += sprintf(buf + len, "\n");

	ldim_debug_buf_save(path, buf, len);

	kfree(buf);
}

static ssize_t ldim_attr_show(struct class *cla, struct class_attribute *attr,
			      char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_s *fw;
	ssize_t len = 0;

	if (!ldim_drv->fw)
		return sprintf(buf, "%s: error: fw is null\n", __func__);

	mutex_lock(&ldim_dbg_mutex);

	fw = ldim_drv->fw;
	if (dbg_attr.cmd == LDIM_DBG_ATTR_CMD_NULL ||
	    dbg_attr.cmd == LDIM_DBG_ATTR_CMD_ERR) {
		mutex_unlock(&ldim_dbg_mutex);
		return sprintf(buf, "%s: error\n", __func__);
	}

	switch (dbg_attr.mode) {
	case LDIM_DBG_ATTR_MODE_SINGLE:
		len += sprintf(buf + len, "for_tool:%d\n", dbg_attr.data);
		break;
	case LDIM_DBG_ATTR_MODE_BL_MATRIX:
	case LDIM_DBG_ATTR_MODE_TEST_MATRIX:
	case LDIM_DBG_ATTR_MODE_SEG_HIST:
	case LDIM_DBG_ATTR_MODE_MAX_RGB:
	case LDIM_DBG_ATTR_MODE_DTH_LUT:
		//none for temp file read lut
		break;
	case LDIM_DBG_ATTR_MODE_GLB_HIST:
		len = ldim_int_table_dbg_print(buf, 64,
					       ldim_drv->stts->global_hist);
		break;
	case LDIM_DBG_ATTR_MODE_GAIN_LUT:
		len = ldim_gain_lut_dbg_line_print(buf, dbg_attr.data);
		break;
	case LDIM_DBG_ATTR_MODE_MIN_GAIN_LUT:
		len = ldim_min_gain_lut_dbg_print(buf);
		break;
	case LDIM_DBG_ATTR_MODE_BL_DIM_CURVE:
		len = ldim_int_table_dbg_print(buf, 17,
					       fw->ctrl->bl_remap_curve);
		break;
	case LDIM_DBG_ATTR_MODE_LD_WHIST:
		len = ldim_int_table_dbg_print(buf, 16,
					       fw->ctrl->fw_ld_whist);
		break;
	default:
		len = sprintf(buf, "%s: error\n", __func__);
		break;
	}

	mutex_unlock(&ldim_dbg_mutex);
	return len;
}

static int ldim_attr_tuning_new(struct aml_ldim_driver_s *ldim_drv, char **parm)
{
	struct ldc_prm_s *prm;
	struct ldim_comp_s *comp;
	unsigned int val;
	int i, j, k;

	prm = ldim_drv->fw->ctrl->prm_ldc;
	comp = ldim_drv->comp;

	if (!strcmp(parm[0], "fw_sel")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = ldim_drv->fw->fw_sel;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			ldim_drv->fw->fw_sel = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = ldim_drv->fw->fw_sel;
			ldim_config_update_t7(ldim_drv);
		}
		pr_info("fw_sel = %d\n", ldim_drv->fw->fw_sel);
	} else if (!strcmp(parm[0], "ldc_hist_mode")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_hist_mode;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_hist_mode = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_hist_mode;
		}
		pr_info("ldc_hist_mode = %d\n", prm->ldc_hist_mode);
	} else if (!strcmp(parm[0], "ldc_hist_blend_mode")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_hist_blend_mode;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_hist_blend_mode = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_hist_blend_mode;
		}
		pr_info("ldc_hist_blend_mode = %d\n", prm->ldc_hist_blend_mode);
	} else if (!strcmp(parm[0], "ldc_hist_blend_alpha")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_hist_blend_alpha;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_hist_blend_alpha = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_hist_blend_alpha;
		}
		pr_info("ldc_hist_blend_alpha = %d\n",
			prm->ldc_hist_blend_alpha);
	} else if (!strcmp(parm[0], "ldc_hist_adap_blend_max_gain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data =
					prm->ldc_hist_adap_blend_max_gain;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_hist_adap_blend_max_gain = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_hist_adap_blend_max_gain;
		}
		pr_info("ldc_hist_adap_blend_max_gain = %d\n",
			prm->ldc_hist_adap_blend_max_gain);
	} else if (!strcmp(parm[0], "ldc_hist_adap_blend_diff_th1")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data =
					prm->ldc_hist_adap_blend_diff_th1;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_hist_adap_blend_diff_th1 = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_hist_adap_blend_diff_th1;
		}
		pr_info("ldc_hist_adap_blend_diff_th1 = %d\n",
			prm->ldc_hist_adap_blend_diff_th1);
	} else if (!strcmp(parm[0], "ldc_hist_adap_blend_diff_th2")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data =
					prm->ldc_hist_adap_blend_diff_th2;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_hist_adap_blend_diff_th2 = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_hist_adap_blend_diff_th2;
		}
		pr_info("ldc_hist_adap_blend_diff_th2 = %d\n",
			prm->ldc_hist_adap_blend_diff_th2);
	} else if (!strcmp(parm[0], "ldc_hist_adap_blend_th0")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_hist_adap_blend_th0;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_hist_adap_blend_th0 = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_hist_adap_blend_th0;
		}
		pr_info("ldc_hist_adap_blend_th0 = %d\n",
			prm->ldc_hist_adap_blend_th0);
	} else if (!strcmp(parm[0], "ldc_hist_adap_blend_thn")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_hist_adap_blend_thn;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_hist_adap_blend_thn = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_hist_adap_blend_thn;
		}
		pr_info("ldc_hist_adap_blend_thn = %d\n",
			prm->ldc_hist_adap_blend_thn);
	} else if (!strcmp(parm[0], "ldc_hist_adap_blend_gain_0")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_hist_adap_blend_gain_0;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_hist_adap_blend_gain_0 = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_hist_adap_blend_gain_0;
		}
		pr_info("ldc_hist_adap_blend_gain_0 = %d\n",
			prm->ldc_hist_adap_blend_gain_0);
	} else if (!strcmp(parm[0], "ldc_hist_adap_blend_gain_1")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_hist_adap_blend_gain_1;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_hist_adap_blend_gain_1 = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_hist_adap_blend_gain_1;
		}
		pr_info("ldc_hist_adap_blend_gain_1 = %d\n",
			prm->ldc_hist_adap_blend_gain_1);
	} else if (!strcmp(parm[0], "ldc_init_bl_min")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_init_bl_min;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_init_bl_min = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_init_bl_min;
		}
		pr_info("ldc_init_bl_min = %d\n", prm->ldc_init_bl_min);
	} else if (!strcmp(parm[0], "ldc_init_bl_max")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_init_bl_max;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_init_bl_max = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_init_bl_max;
		}
		pr_info("ldc_init_bl_max = %d\n", prm->ldc_init_bl_max);
	} else if (!strcmp(parm[0], "ldc_dimming_curve_en")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_dimming_curve_en;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_dimming_curve_en = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_dimming_curve_en;
		}
		pr_info("ldc_dimming_curve_en = %d\n",
			prm->ldc_dimming_curve_en);
	} else if (!strcmp(parm[0], "ldc_sf_mode")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_sf_mode;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_sf_mode = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_sf_mode;
		}
		pr_info("ldc_sf_mode = %d\n", prm->ldc_sf_mode);
	} else if (!strcmp(parm[0], "ldc_sf_gain_up")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_sf_gain_up;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_sf_gain_up = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_sf_gain_up;
		}
		pr_info("ldc_sf_gain_up = %d\n", prm->ldc_sf_gain_up);
	} else if (!strcmp(parm[0], "ldc_sf_gain_dn")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_sf_gain_dn;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_sf_gain_dn = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_sf_gain_dn;
		}
		pr_info("ldc_sf_gain_dn = %d\n", prm->ldc_sf_gain_dn);
	} else if (!strcmp(parm[0], "ldc_sf_tsf_3x3")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_sf_tsf_3x3;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_sf_tsf_3x3 = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_sf_tsf_3x3;
		}
		pr_info("ldc_sf_tsf_3x3 = %d\n", prm->ldc_sf_tsf_3x3);
	} else if (!strcmp(parm[0], "ldc_sf_tsf_5x5")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_sf_tsf_5x5;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_sf_tsf_5x5 = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_sf_tsf_5x5;
		}
		pr_info("ldc_sf_tsf_5x5 = %d\n", prm->ldc_sf_tsf_5x5);
	} else if (!strcmp(parm[0], "ldc_bs_bl_mode")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_bs_bl_mode;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_bs_bl_mode = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_bs_bl_mode;
		}
		pr_info("ldc_bs_bl_mode = %d\n", prm->ldc_bs_bl_mode);
	} else if (!strcmp(parm[0], "ldc_glb_apl")) { //read only
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_glb_apl;
				goto ldim_attr_tuning_new_end;
			}
			goto ldim_attr_tuning_new_err;
		}
		pr_info("ldc_glb_apl = %d\n", prm->ldc_glb_apl);
	} else if (!strcmp(parm[0], "ldc_bs_glb_apl_gain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_bs_glb_apl_gain;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_bs_glb_apl_gain = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_bs_glb_apl_gain;
		}
		pr_info("ldc_bs_glb_apl_gain = %d\n", prm->ldc_bs_glb_apl_gain);
	} else if (!strcmp(parm[0], "ldc_bs_dark_scene_bl_th")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_bs_dark_scene_bl_th;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_bs_dark_scene_bl_th = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_bs_dark_scene_bl_th;
		}
		pr_info("ldc_bs_dark_scene_bl_th = %d\n",
			prm->ldc_bs_dark_scene_bl_th);
	} else if (!strcmp(parm[0], "ldc_bs_gain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_bs_gain;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_bs_gain = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_bs_gain;
		}
		pr_info("ldc_bs_gain = %d\n", prm->ldc_bs_gain);
	} else if (!strcmp(parm[0], "ldc_bs_limit_gain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_bs_limit_gain;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_bs_limit_gain = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_bs_limit_gain;
		}
		pr_info("ldc_bs_limit_gain = %d\n", prm->ldc_bs_limit_gain);
	} else if (!strcmp(parm[0], "ldc_bs_loc_apl_gain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_bs_loc_apl_gain;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_bs_loc_apl_gain = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_bs_loc_apl_gain;
		}
		pr_info("ldc_bs_loc_apl_gain = %d\n", prm->ldc_bs_loc_apl_gain);
	} else if (!strcmp(parm[0], "ldc_bs_loc_max_min_gain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_bs_loc_max_min_gain;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_bs_loc_max_min_gain = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_bs_loc_max_min_gain;
		}
		pr_info("ldc_bs_loc_max_min_gain = %d\n",
			prm->ldc_bs_loc_max_min_gain);
	} else if (!strcmp(parm[0], "ldc_bs_loc_dark_scene_bl_th")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data =
					prm->ldc_bs_loc_dark_scene_bl_th;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_bs_loc_dark_scene_bl_th = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_bs_loc_dark_scene_bl_th;
		}
		pr_info("ldc_bs_loc_dark_scene_bl_th = %d\n",
			prm->ldc_bs_loc_dark_scene_bl_th);
	} else if (!strcmp(parm[0], "ldc_tf_en")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_tf_en;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_tf_en = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_tf_en;
		}
		pr_info("ldc_tf_en = %d\n", prm->ldc_tf_en);
	} else if (!strcmp(parm[0], "ldc_tf_sc_flag")) { //read only
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_tf_sc_flag;
				goto ldim_attr_tuning_new_end;
			}
			goto ldim_attr_tuning_new_err;
		}
		pr_info("ldc_tf_sc_flag = %d\n", prm->ldc_tf_sc_flag);
	} else if (!strcmp(parm[0], "ldc_tf_low_alpha")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_tf_low_alpha;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_tf_low_alpha = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_tf_low_alpha;
		}
		pr_info("ldc_tf_low_alpha = %d\n", prm->ldc_tf_low_alpha);
	} else if (!strcmp(parm[0], "ldc_tf_high_alpha")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_tf_high_alpha;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_tf_high_alpha = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_tf_high_alpha;
		}
		pr_info("ldc_tf_high_alpha = %d\n", prm->ldc_tf_high_alpha);
	} else if (!strcmp(parm[0], "ldc_tf_low_alpha_sc")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_tf_low_alpha_sc;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_tf_low_alpha_sc = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_tf_low_alpha_sc;
		}
		pr_info("ldc_tf_low_alpha_sc = %d\n", prm->ldc_tf_low_alpha_sc);
	} else if (!strcmp(parm[0], "ldc_tf_high_alpha_sc")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_tf_high_alpha_sc;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_tf_high_alpha_sc = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_tf_high_alpha_sc;
		}
		pr_info("ldc_tf_high_alpha_sc = %d\n",
			prm->ldc_tf_high_alpha_sc);
	} else if (!strcmp(parm[0], "ldc_sc_hist_diff_th")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_sc_hist_diff_th;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_sc_hist_diff_th = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_sc_hist_diff_th;
		}
		pr_info("ldc_sc_hist_diff_th = %d\n",
			prm->ldc_sc_hist_diff_th);
	} else if (!strcmp(parm[0], "ldc_sc_apl_diff_th")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = prm->ldc_sc_apl_diff_th;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			prm->ldc_sc_apl_diff_th = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = prm->ldc_sc_apl_diff_th;
		}
		pr_info("ldc_sc_apl_diff_th = %d\n",
			prm->ldc_sc_apl_diff_th);
	} else if (!strcmp(parm[0], "ldc_bl_buf_diff")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = comp->ldc_bl_buf_diff;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			comp->ldc_bl_buf_diff = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = comp->ldc_bl_buf_diff;
			ldim_config_update_t7(ldim_drv);
		}
		pr_info("ldc_bl_buf_diff = %d\n", comp->ldc_bl_buf_diff);
	} else if (!strcmp(parm[0], "ldc_glb_gain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = comp->ldc_glb_gain;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			comp->ldc_glb_gain = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = comp->ldc_glb_gain;
			ldim_config_update_t7(ldim_drv);
		}
		pr_info("ldc_glb_gain = %d\n", comp->ldc_glb_gain);
	} else if (!strcmp(parm[0], "ldc_dth_en")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = comp->ldc_dth_en;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			comp->ldc_dth_en = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = comp->ldc_dth_en;
			ldim_config_update_t7(ldim_drv);
		}
		pr_info("ldc_dth_en = %d\n", comp->ldc_dth_en);
	} else if (!strcmp(parm[0], "ldc_dth_bw")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = comp->ldc_dth_bw;
				goto ldim_attr_tuning_new_end;
			}
			if (kstrtouint(parm[1], 10, &val) < 0)
				goto ldim_attr_tuning_new_err;
			comp->ldc_dth_bw = val;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = comp->ldc_dth_bw;
			ldim_config_update_t7(ldim_drv);
		}
		pr_info("ldc_dth_bw = %d\n", comp->ldc_dth_bw);
	} else if (!strcmp(parm[0], "glb_hist")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_GLB_HIST;
				goto ldim_attr_tuning_new_end;
			}
			goto ldim_attr_tuning_new_err;
		}
		ldim_int_table_print(64, ldim_drv->stts->global_hist);
	} else if (!strcmp(parm[0], "min_gain_lut")) {
		if (!parm[1]) {
			ldim_min_gain_lut_print();
			goto ldim_attr_tuning_new_end;
		}
		if (!strcmp(parm[1], "r")) {
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_MIN_GAIN_LUT;
			goto ldim_attr_tuning_new_end;
		}
		if (!strcmp(parm[1], "w")) {
			if (parm[65]) {
				for (j = 0; j < 64; j++) {
					if (kstrtouint(parm[j + 2], 10,
							&val) < 0)
						goto ldim_attr_tuning_new_err;
					ldc_min_gain_lut[j] = val;
				}
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_MIN_GAIN_LUT;
				ldc_min_gain_lut_set();
			}
			goto ldim_attr_tuning_new_end;
		}
		if (parm[64]) {
			for (j = 0; j < 64; j++) {
				if (kstrtouint(parm[j + 1], 10,
					       &ldc_min_gain_lut[j]) < 0)
					goto ldim_attr_tuning_new_err;
			}
			ldc_min_gain_lut_set();
		}
		goto ldim_attr_tuning_new_err;
	} else if (!strcmp(parm[0], "dither_lut")) {
		if (!parm[1]) {
			ldim_dither_lut_print();
			goto ldim_attr_tuning_new_end;
		}
		if (!strcmp(parm[1], "rf")) {
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_DTH_LUT;
			ldim_dither_lut_file_save(parm[2]);
			goto ldim_attr_tuning_new_end;
		}
		if (!strcmp(parm[1], "w")) {
			if (!parm[512 + 3])
				goto ldim_attr_tuning_new_err;
			if (kstrtouint(parm[2], 10, &i) < 0)
				goto ldim_attr_tuning_new_err;
			if (kstrtouint(parm[3], 10, &j) < 0)
				goto ldim_attr_tuning_new_err;
			if (i != 32 || j != 16)
				goto ldim_attr_tuning_new_err;
			for (i = 0; i < 32; i++) {
				for (j = 0; j < 16; j++) {
					k = i * 16 + j;
					if (kstrtouint(parm[k + 4], 10,
							&val) < 0)
						goto ldim_attr_tuning_new_err;
					ldc_dither_lut[i][j] = val;
				}
			}
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_DTH_LUT;
			ldc_dither_lut_set();
			goto ldim_attr_tuning_new_end;
		}
		goto ldim_attr_tuning_new_err;
	} else {
		goto ldim_attr_tuning_new_err;
	}

ldim_attr_tuning_new_end:
	return 0;

ldim_attr_tuning_new_err:
	return -1;
}

static ssize_t ldim_attr_store(struct class *cla, struct class_attribute *attr,
			       const char *buf, size_t len)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_s *fw = ldim_drv->fw;
	struct ldim_fw_custom_s *fw_cus_pre = aml_ldim_get_fw_cus_pre();
	struct ldim_fw_custom_s *fw_cus_post = aml_ldim_get_fw_cus_post();
	struct fw_ctrl_s *fw_ctrl;
	struct ld_reg_s *nprm;
	unsigned int n = 0;
	char *buf_orig, *ps, *token;
	char **parm = NULL;
	char str[3] = {' ', '\n', '\0'};
	unsigned int seg_size;
	int i, j, k;
	unsigned int *table;
	int ret;

	unsigned int seg_row = 0, seg_col = 0;
	unsigned int blk_mode = 0, ldim_bl_en = 0, hvcnt_bypass = 0;
	unsigned long val1 = 0;

	if (!fw)
		return len;
	if (!fw->ctrl)
		return len;
	fw_ctrl = fw->ctrl;
	nprm = fw_ctrl->nprm;

	if (!buf)
		return len;

	mutex_lock(&ldim_dbg_mutex);

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		mutex_unlock(&ldim_dbg_mutex);
		return len;
	}
	parm = kcalloc(640, sizeof(char *), GFP_KERNEL);
	if (!parm) {
		kfree(buf_orig);
		mutex_unlock(&ldim_dbg_mutex);
		return len;
	}
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	dbg_attr.chip_type = bdrv->data->chip_type;
	seg_size = ldim_drv->conf->seg_row * ldim_drv->conf->seg_col;

	if (bdrv->data->chip_type == LCD_CHIP_T7 ||
	    bdrv->data->chip_type == LCD_CHIP_T3) {
		ret = ldim_attr_tuning_new(ldim_drv, parm);
		if (ret == 0)
			goto ldim_attr_store_end;
	}

	if (!strcmp(parm[0], "hist")) {
		if (parm[2]) {
			if (!strcmp(parm[1], "rf")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SEG_HIST;
				if (dbg_attr.chip_type == LCD_CHIP_T7 ||
				    dbg_attr.chip_type == LCD_CHIP_T3) {
					ldim_hist_file_save_t7(ldim_drv,
							       parm[2]);
				} else {
					ldim_hist_file_save(ldim_drv, parm[2]);
				}
				goto ldim_attr_store_end;
			}
			goto ldim_attr_store_err;
		}
		if (dbg_attr.chip_type == LCD_CHIP_T7 ||
		    dbg_attr.chip_type == LCD_CHIP_T3)
			ldim_dump_histgram_t7(ldim_drv);
		else
			ldim_dump_histgram(ldim_drv);
	} else if (!strcmp(parm[0], "maxrgb")) {
		if (dbg_attr.chip_type == LCD_CHIP_T7 ||
		    dbg_attr.chip_type == LCD_CHIP_T3)
			goto ldim_attr_store_end;

		if (parm[2]) {
			if (!strcmp(parm[1], "rf")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_MAX_RGB;
				ldim_max_rgb_file_save(ldim_drv, parm[2]);
				goto ldim_attr_store_end;
			}
			goto ldim_attr_store_err;
		}
		ldim_dump_matrix_info_max_rgb(ldim_drv);
	} else if (!strcmp(parm[0], "stts")) {
		if (dbg_attr.chip_type == LCD_CHIP_T7 ||
		    dbg_attr.chip_type == LCD_CHIP_T3)
			ldim_stts_data_dump_t7(ldim_drv);
		else
			ldim_stts_data_dump(ldim_drv);
	} else if (!strcmp(parm[0], "matrix")) {
		if (parm[2]) {
			if (!strcmp(parm[1], "rf")) {
				ldim_bl_matrix_file_save(ldim_drv, parm[2]);
				goto ldim_attr_store_end;
			}
			goto ldim_attr_store_err;
		}
		ldim_dump_bl_matrix(ldim_drv);
	} else if (!strcmp(parm[0], "bl_matrix_dbg")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = fw->bl_matrix_dbg;
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw->bl_matrix_dbg) < 0)
				goto ldim_attr_store_err;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = fw->bl_matrix_dbg;
		}
		pr_info("fw bl_matrix_dbg: %d\n", fw->bl_matrix_dbg);
	} else if (!strcmp(parm[0], "db_load")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", ldim_drv->load_db_en);
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->load_db_en = val1 ? 1 : 0;
		}
		pr_info("fw para load db: %d\n", ldim_drv->load_db_en);
	} else if (!strcmp(parm[0], "db_print")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					ldim_drv->db_print_flag);
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->db_print_flag = val1 ? 1 : 0;
		}
		pr_info("db_print_flag: %d\n", ldim_drv->db_print_flag);
	} else if (!strcmp(parm[0], "remap_init")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d %d %d %d %d\n",
					ldim_drv->conf->seg_row,
					ldim_drv->conf->seg_col,
					ldim_drv->conf->bl_mode,
					ldim_drv->conf->func_en,
					ldim_drv->conf->hvcnt_bypass);
				goto ldim_attr_store_end;
			}
		}
		if (parm[5]) {
			if (kstrtouint(parm[1], 10, &seg_row) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[2], 10, &seg_col) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[3], 10, &blk_mode) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[4], 10, &ldim_bl_en) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[5], 10, &hvcnt_bypass) < 0)
				goto ldim_attr_store_err;

			ldim_drv->conf->seg_row = (unsigned char)seg_row;
			ldim_drv->conf->seg_col = (unsigned char)seg_col;
			ldim_drv->conf->bl_mode = (unsigned char)blk_mode;
			ldim_drv->conf->func_en = (unsigned char)ldim_bl_en;
			ldim_drv->conf->hvcnt_bypass = (unsigned char)hvcnt_bypass;
			if (ldim_drv->data->drv_init)
				ldim_drv->data->drv_init(ldim_drv);
			pr_info("**************ldim init ok*************\n");
		}
		pr_info("remap_init param: %d %d %d %d %d\n",
			ldim_drv->conf->seg_row, ldim_drv->conf->seg_col,
			ldim_drv->conf->bl_mode,
			ldim_drv->conf->func_en,
			ldim_drv->conf->hvcnt_bypass);
	} else if (!strcmp(parm[0], "ldim_stts_init")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d %d\n",
					ldim_drv->conf->seg_row,
					ldim_drv->conf->seg_col);
				goto ldim_attr_store_end;
			}
		}
		if (parm[2]) {
			if (kstrtouint(parm[1], 10, &seg_row) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[2], 10, &seg_col) < 0)
				goto ldim_attr_store_err;

			ldim_drv->conf->seg_row = (unsigned char)seg_row;
			ldim_drv->conf->seg_col = (unsigned char)seg_col;
			if (ldim_drv->data->drv_init)
				ldim_drv->data->drv_init(ldim_drv);
			pr_info("************ldim stts init ok*************\n");
		}
		pr_info("ldim_stts_init param: %d %d\n",
			ldim_drv->conf->seg_row, ldim_drv->conf->seg_col);
	} else if (!strcmp(parm[0], "frm_size")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d %d\n",
					ldim_drv->conf->hsize,
					ldim_drv->conf->vsize);
				goto ldim_attr_store_end;
			}
		}
		pr_info("frm_width: %d, frm_height: %d\n",
			ldim_drv->conf->hsize, ldim_drv->conf->vsize);
	} else if (!strcmp(parm[0], "func") ||
		   !strcmp(parm[0], "func_en")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = ldim_drv->conf->func_en;
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->conf->func_en = val1 ? 1 : 0;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = ldim_drv->conf->func_en;
			if (dbg_attr.chip_type != LCD_CHIP_T7 &&
				dbg_attr.chip_type != LCD_CHIP_T3) {
				if (ldim_drv->data &&
					ldim_drv->data->func_ctrl) {
					ldim_drv->data->func_ctrl(ldim_drv,
						ldim_drv->conf->func_en);
				}
			}
		}
		pr_info("ldim_func_en: %d\n", ldim_drv->conf->func_en);
	} else if (!strcmp(parm[0], "remap") ||
		   !strcmp(parm[0], "remapping_en")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = ldim_drv->conf->remap_en;
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			if (val1) {
				if (ldim_drv->func_en) {
					ldim_drv->conf->remap_en = 1;
					if (dbg_attr.chip_type != LCD_CHIP_T7 &&
						dbg_attr.chip_type !=
						LCD_CHIP_T3)
						ldim_remap_ctrl(1);
				} else {
					pr_info("error: ldim_func is disabled\n");
				}
			} else {
				ldim_drv->conf->remap_en = 0;
				if (dbg_attr.chip_type != LCD_CHIP_T7 &&
					dbg_attr.chip_type != LCD_CHIP_T3)
					ldim_remap_ctrl(0);
			}
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = ldim_drv->conf->func_en;
		}
		pr_info("ldim_remap_en: %d\n", ldim_drv->conf->remap_en);
	} else if (!strcmp(parm[0], "demo")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = ldim_drv->conf->demo_en;
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->conf->demo_en = val1 ? 1 : 0;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = ldim_drv->conf->demo_en;
			if (ldim_drv->remap_en) {
				if (ldim_drv->conf->demo_en) {
					ldim_drv->demo_en = 1;
					nprm->reg_ld_rgb_mapping_demo = 1;
					ldim_wr_reg_bits(REG_LD_RGB_MOD,
							 1, 19, 1);
				} else {
					ldim_drv->demo_en = 0;
					nprm->reg_ld_rgb_mapping_demo = 0;
					ldim_wr_reg_bits(REG_LD_RGB_MOD,
							 0, 19, 1);
				}
			} else {
				ldim_drv->demo_en = 0;
				pr_info("error: ldim_remap is disabled\n");
			}
		}
		pr_info("ldim_demo_en: %d\n", ldim_drv->demo_en);
	} else if (!strcmp(parm[0], "data_min")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = ldim_drv->data_min;
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &ldim_drv->data_min) < 0)
				goto ldim_attr_store_err;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = ldim_drv->data_min;
			ldim_drv->set_level(ldim_drv->brightness_level);
		}
		LDIMPR("brightness data_min: %d\n", ldim_drv->data_min);
	} else if (!strcmp(parm[0], "litgain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = ldim_drv->litgain;
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &ldim_drv->litgain) < 0)
				goto ldim_attr_store_err;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = ldim_drv->litgain;
		}
		pr_info("litgain = %d\n", ldim_drv->litgain);
	} else if (!strcmp(parm[0], "black_frm_en")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = ldim_drv->black_frm_en;
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->black_frm_en = (unsigned char)val1;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = ldim_drv->black_frm_en;
		}
		pr_info("black_frm_en = %d\n", ldim_drv->black_frm_en);
	} else if (!strcmp(parm[0], "brightness_bypass")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = ldim_drv->brightness_bypass;
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->brightness_bypass = (unsigned char)val1;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = ldim_drv->brightness_bypass;
			if (ldim_drv->brightness_bypass == 0)
				ldim_drv->set_level(ldim_drv->brightness_level);
		}
		pr_info("brightness_bypass = %d\n",
			ldim_drv->brightness_bypass);
	} else if (!strcmp(parm[0], "test_mode")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = ldim_drv->test_bl_en;
				pr_info("for_tool:%d\n", ldim_drv->test_bl_en);
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->test_bl_en = (unsigned char)val1;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = ldim_drv->test_bl_en;
		}
		LDIMPR("test_mode: %d\n", ldim_drv->test_bl_en);
		ldim_drv->level_update = 1;
	} else if (!strcmp(parm[0], "test_matrix")) {
		if (parm[2]) {
			if (!strcmp(parm[1], "rf")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_TEST_MATRIX;
				ldim_test_matrix_file_save(ldim_drv, parm[2]);
				ldim_drv->level_update = 1;
				goto ldim_attr_store_end;
			}
			goto ldim_attr_store_err;
		}
		if (parm[1]) {
			if (!strcmp(parm[1], "w")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_TEST_MATRIX;
				if (!parm[seg_size + 3])
					goto ldim_attr_store_err;
				if (kstrtouint(parm[2], 10, &i) < 0)
					goto ldim_attr_store_err;
				if (kstrtouint(parm[3], 10, &j) < 0)
					goto ldim_attr_store_err;
				if (i != ldim_drv->conf->seg_row ||
				    j != ldim_drv->conf->seg_col)
					goto ldim_attr_store_err;
				for (i = 0; i < seg_size; i++) {
					if (kstrtouint(parm[i + 4], 10, &j) < 0)
						goto ldim_attr_store_err;
					ldim_drv->test_matrix[i] = j;
				}
				ldim_drv->level_update = 1;
				goto ldim_attr_store_end;
			}
		}
		goto ldim_attr_store_err;
	} else if (!strcmp(parm[0], "test_set")) {
		if (parm[2]) {
			if (kstrtouint(parm[1], 10, &i) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[2], 10, &j) < 0)
				goto ldim_attr_store_err;

			if (i < seg_size) {
				ldim_drv->test_matrix[i] = j;
				LDIMPR("set test_matrix[%d] = %4d\n", i, j);
			} else {
				LDIMERR("invalid index for test_matrix[%d]\n", i);
			}
			ldim_drv->level_update = 1;
			goto ldim_attr_store_end;
		}
		ldim_get_test_matrix_info(ldim_drv);
	} else if (!strcmp(parm[0], "test_set_all")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &j) < 0)
				goto ldim_attr_store_err;

			for (i = 0; i < seg_size; i++)
				ldim_drv->test_matrix[i] = j;

			LDIMPR("set all test_matrix to %4d\n", j);
			ldim_drv->level_update = 1;
			goto ldim_attr_store_end;
		}
		ldim_get_test_matrix_info(ldim_drv);
	} else if (!strcmp(parm[0], "bl_remap_curve")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_BL_DIM_CURVE;
				goto ldim_attr_store_end;
			}
		}
		if (parm[17]) {
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_BL_DIM_CURVE;
			for (i = 0; i < 17; i++) {
				if (kstrtouint(parm[i + 1], 10,
					       &fw_ctrl->bl_remap_curve[i]) < 0)
					goto ldim_attr_store_err;
			}
		}
		ldim_int_table_print(17, fw_ctrl->bl_remap_curve);
	} else if (!strcmp(parm[0], "fw_LD_Whist") ||
		   !strcmp(parm[0], "fw_ld_whist")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_LD_WHIST;
				goto ldim_attr_store_end;
			}
		}
		if (parm[16]) {
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_LD_WHIST;
			for (i = 0; i < 16; i++) {
				if (kstrtouint(parm[i + 1], 10,
					       &fw_ctrl->fw_ld_whist[i]) < 0) {
					goto ldim_attr_store_err;
				}
			}
		}
		ldim_int_table_print(16, fw_ctrl->fw_ld_whist);
	} else if (!strcmp(parm[0], "ld_remap_lut") ||
		   !strcmp(parm[0], "comp_gain_lut")) {
		if (parm[2]) {
			if (!strcmp(parm[1], "lr")) { /* line read */
				if (kstrtouint(parm[2], 10, &i) < 0)
					goto ldim_attr_store_err;
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_GAIN_LUT;
				dbg_attr.data = i;
				goto ldim_attr_store_end;
			}
			if (!strcmp(parm[1], "lw")) { /* line write */
				if (kstrtouint(parm[2], 10, &i) < 0)
					goto ldim_attr_store_err;
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_GAIN_LUT;
				dbg_attr.data = i;
				if (dbg_attr.chip_type == LCD_CHIP_T7 ||
				    dbg_attr.chip_type == LCD_CHIP_T3) {
					k = 64;
					table = ldc_gain_lut_array[i];
				} else {
					k = 32;
					table = ld_remap_lut[i];
				}
				if (!parm[k + 2])
					goto ldim_attr_store_err;
				for (j = 0; j < k; j++) {
					if (kstrtouint(parm[j + 3], 10,
						       &table[j]) < 0)
						goto ldim_attr_store_err;
				}
				if (ldim_drv->data->remap_lut_update)
					ldim_drv->data->remap_lut_update();
				goto ldim_attr_store_end;
			}
			goto ldim_attr_store_err;
		}
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &i) < 0)
				goto ldim_attr_store_err;
			ldim_comp_gain_lut_print(i);
		}
		if (!parm[1]) {
			ldim_comp_gain_lut_print(0xff);
			goto ldim_attr_store_end;
		}
	} else if (!strcmp(parm[0], "Sf_bypass")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->sf_bypass = (unsigned char)val1;
		}
		pr_info("Sf_bypass = %d\n", fw_ctrl->sf_bypass);
	} else if (!strcmp(parm[0], "Boost_light_bypass")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->boost_light_bypass = (unsigned char)val1;
		}
		pr_info("Boost_light_bypass = %d\n",
			fw_ctrl->boost_light_bypass);
	} else if (!strcmp(parm[0], "Lpf_bypass")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->lpf_bypass = (unsigned char)val1;
		}
		pr_info("Lpf_bypass = %d\n", fw_ctrl->lpf_bypass);
	} else if (!strcmp(parm[0], "Ld_remap_bypass")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw_ctrl->ld_remap_bypass);
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->ld_remap_bypass = (unsigned char)val1;
		}
		pr_info("Ld_remap_bypass = %d\n", fw_ctrl->ld_remap_bypass);
	} else if (!strcmp(parm[0], "ov_gain")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &fw_ctrl->ov_gain) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("ov_gain = %d\n", fw_ctrl->ov_gain);
	} else if (!strcmp(parm[0], "fw_LD_ThSF_l")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->fw_ld_thsf_l);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->fw_ld_thsf_l) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("fw_LD_ThSF_l = %d\n", fw_ctrl->fw_ld_thsf_l);
	} else if (!strcmp(parm[0], "fw_LD_ThTF_l")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->fw_ld_thtf_l);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->fw_ld_thtf_l) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("fw_LD_ThTF_l = %d\n", fw_ctrl->fw_ld_thtf_l);
	} else if (!strcmp(parm[0], "fw_ld_thist")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->fw_ld_thist);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->fw_ld_thist) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("fw_ld_thist = %d\n", fw_ctrl->fw_ld_thist);
	} else if (!strcmp(parm[0], "boost_gain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->boost_gain);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->boost_gain) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("boost_gain = %d\n", fw_ctrl->boost_gain);
	} else if (!strcmp(parm[0], "TF_alpha")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->tf_alpha);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->tf_alpha) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("TF_alpha = %d\n", fw_ctrl->tf_alpha);
	} else if (!strcmp(parm[0], "lpf_gain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->lpf_gain);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->lpf_gain) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("lpf_gain = %d\n", fw_ctrl->lpf_gain);
	} else if (!strcmp(parm[0], "lpf_res")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->lpf_res);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->lpf_res) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("lpf_res = %d\n", fw_ctrl->lpf_res);
	} else if (!strcmp(parm[0], "rgb_base")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->rgb_base);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->rgb_base) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("rgb_base = %d\n", fw_ctrl->rgb_base);
	} else if (!strcmp(parm[0], "avg_gain")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->avg_gain);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->avg_gain) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("avg_gain = %d\n", fw_ctrl->avg_gain);
	} else if (!strcmp(parm[0], "fw_rgb_diff_th")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw_ctrl->fw_rgb_diff_th);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->fw_rgb_diff_th) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("fw_rgb_diff_th = %d\n", fw_ctrl->fw_rgb_diff_th);
	} else if (!strcmp(parm[0], "fw_TF_sum_th")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &fw_ctrl->fw_tf_sum_th) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("fw_TF_sum_th = %d\n", fw_ctrl->fw_tf_sum_th);
	} else if (!strcmp(parm[0], "LPF_method")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->lpf_method);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->lpf_method) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("LPF_method = %d\n", fw_ctrl->lpf_method);
	} else if (!strcmp(parm[0], "LD_TF_STEP_TH")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw_ctrl->ld_tf_step_th);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->ld_tf_step_th) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("LD_TF_STEP_TH = %d\n", fw_ctrl->ld_tf_step_th);
	} else if (!strcmp(parm[0], "TF_step_method")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->tf_step_method) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("TF_step_method = %d\n", fw_ctrl->tf_step_method);
	} else if (!strcmp(parm[0], "TF_FRESH_BL")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->tf_fresh_bl);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->tf_fresh_bl) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("TF_FRESH_BL = %d\n", fw_ctrl->tf_fresh_bl);
	} else if (!strcmp(parm[0], "TF_BLK_FRESH_BL")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw_ctrl->tf_blk_fresh_bl);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->tf_blk_fresh_bl) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("TF_BLK_FRESH_BL = %d\n", fw_ctrl->tf_blk_fresh_bl);
	} else if (!strcmp(parm[0], "bbd_detect_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->bbd_detect_en = (unsigned char)val1;
		}
		pr_info("bbd_detect_en = %d\n", fw_ctrl->bbd_detect_en);
	} else if (!strcmp(parm[0], "side_blk_diff_th")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw_ctrl->side_blk_diff_th);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->side_blk_diff_th) < 0) {
				goto ldim_attr_store_err;
			}
		}
		pr_info("side_blk_diff_th = %d\n", fw_ctrl->side_blk_diff_th);
	} else if (!strcmp(parm[0], "bbd_th")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n", fw_ctrl->bbd_th);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw_ctrl->bbd_th) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("bbd_th = %d\n", fw_ctrl->bbd_th);
	} else if (!strcmp(parm[0], "diff_blk_luma_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->diff_blk_luma_en = (unsigned char)val1;
		}
		pr_info("diff_blk_luma_en = %d\n", fw_ctrl->diff_blk_luma_en);
	} else if (!strcmp(parm[0], "avg_update_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->avg_update_en = (unsigned char)val1;
		}
		pr_info("avg_update_en = %d\n", ldim_drv->avg_update_en);
	} else if (!strcmp(parm[0], "matrix_update_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->matrix_update_en = (unsigned char)val1;
		}
		pr_info("matrix_update_en = %d\n", ldim_drv->matrix_update_en);
	} else if (!strcmp(parm[0], "alg_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->alg_en = (unsigned char)val1;
		}
		pr_info("ldim_alg_en = %d\n", ldim_drv->alg_en);
	} else if (!strcmp(parm[0], "top_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->top_en = (unsigned char)val1;
		}
		pr_info("ldim_top_en = %d\n", ldim_drv->top_en);
	} else if (!strcmp(parm[0], "hist_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->hist_en = (unsigned char)val1;
		}
		pr_info("ldim_hist_en = %d\n", ldim_drv->hist_en);
	} else if (!strcmp(parm[0], "white_area_en")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw_ctrl->white_area_remap_en = (unsigned char)val1;
		}
		pr_info("white_area_remap_en = %d\n",
			fw_ctrl->white_area_remap_en);
	} else if (!strcmp(parm[0], "white_area_map")) {
		if (parm[4]) {
			if (kstrtouint(parm[1], 10,
				       &fw_ctrl->white_area_th_max) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[2], 10,
				       &fw_ctrl->white_area_th_min) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[3], 10,
				       &fw_ctrl->white_lvl_th_max) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[4], 10,
				       &fw_ctrl->white_lvl_th_min) < 0)
				goto ldim_attr_store_err;
		}
		pr_info("white_area_th max = %d, min = %d\n",
			fw_ctrl->white_area_th_max,
			fw_ctrl->white_area_th_min);
		pr_info("white_lvl_th max = %d, min = %d\n",
			fw_ctrl->white_lvl_th_max,
			fw_ctrl->white_lvl_th_min);
	} else if (!strcmp(parm[0], "fw_hist_print")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			fw->fw_hist_print = (unsigned char)val1;
		}
		pr_info("fw_hist_print = %d\n", fw->fw_hist_print);
	} else if (!strcmp(parm[0], "fw_print_frequent")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw->fw_print_frequent);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10,
				       &fw->fw_print_frequent) < 0) {
				goto ldim_attr_store_err;
			}
			fw_cus_pre->fw_print_frequent = fw->fw_print_frequent;
			fw_cus_post->fw_print_frequent = fw->fw_print_frequent;
		}
		pr_info("fw_print_frequent = %d\n", fw->fw_print_frequent);
	} else if ((!strcmp(parm[0], "Dbprint_lv")) ||
		   (!strcmp(parm[0], "fw_print"))) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw->fw_print_lv);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw->fw_print_lv) < 0)
				goto ldim_attr_store_err;
			fw_cus_pre->fw_print_lv = fw->fw_print_lv;
			fw_cus_post->fw_print_lv = fw->fw_print_lv;
		}
		pr_info("fw_print_lv = %d\n", fw->fw_print_lv);
	} else if (!strcmp(parm[0], "cus_fw_param")) {
		if (parm[2]) {
			if (kstrtouint(parm[1], 10, &i) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[2], 10, &j) < 0)
				goto ldim_attr_store_err;

			if (fw_cus_pre->fw_alg_frm)
				fw_cus_pre->param[i] = j;

			if (fw_cus_post->fw_alg_frm)
				fw_cus_post->param[i] = j;
		}
		for (k = 0; k < 32; k++) {
			if (fw_cus_pre->fw_alg_frm) {
				pr_info("cus_pre_data[%d] = %d\n",
					k, fw_cus_pre->param[k]);
			}
			if (fw_cus_post->fw_alg_frm) {
				pr_info("cus_post_data[%d] = %d\n",
					k, fw_cus_post->param[k]);
			}
		}
	} else if (!strcmp(parm[0], "info")) {
		pr_info("ldim_drv_ver: %s\n", LDIM_DRV_VER);
		if (ldim_drv->config_print)
			ldim_drv->config_print();
		pr_info("\nldim_seg_row          = %d\n"
			"ldim_seg_col          = %d\n"
			"ldim_bl_mode          = %d\n\n",
			ldim_drv->conf->seg_row, ldim_drv->conf->seg_col,
			ldim_drv->conf->bl_mode);
		pr_info("state                 = 0x%x\n"
			"ldim_on_flag          = %d\n"
			"ldim_func_en          = %d\n"
			"ldim_remap_en         = %d\n"
			"ldim_demo_en          = %d\n"
			"ldim_ld_sel           = %d\n"
			"ldim_func_bypass      = %d\n"
			"ldim_test_bl_en       = %d\n"
			"ldim_test_remap_en    = %d\n"
			"ldim_avg_update_en    = %d\n"
			"ldim_matrix_update_en = %d\n"
			"ldim_alg_en           = %d\n"
			"ldim_top_en           = %d\n"
			"ldim_hist_en          = %d\n"
			"ldim_data_min         = %d\n"
			"ldim_data_max         = %d\n"
			"litgain               = %d\n"
			"fw_valid              = %d\n"
			"fw_sel                = %d\n"
			"fw_flag               = %d\n"
			"ldim_irq_cnt          = %d\n"
			"duty_update_flag      = %d\n\n",
			ldim_drv->state,
			ldim_drv->init_on_flag, ldim_drv->func_en,
			ldim_drv->remap_en, ldim_drv->demo_en,
			ldim_drv->ld_sel, ldim_drv->func_bypass,
			ldim_drv->test_bl_en,
			ldim_drv->test_remap_en,
			ldim_drv->avg_update_en,
			ldim_drv->matrix_update_en,
			ldim_drv->alg_en, ldim_drv->top_en,
			ldim_drv->hist_en,
			ldim_drv->data_min, ldim_drv->data_max,
			ldim_drv->litgain,
			ldim_drv->fw->valid,
			ldim_drv->fw->fw_sel,
			ldim_drv->fw->flag,
			ldim_drv->irq_cnt,
			ldim_drv->duty_update_flag);
	} else if (!strcmp(parm[0], "print")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_debug_print = (unsigned char)val1;
		}
		pr_info("ldim_debug_print = %d\n", ldim_debug_print);
	} else if (!strcmp(parm[0], "duty_update_flag")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->duty_update_flag = (unsigned char)val1;
		}
		pr_info("duty_update_flag = %d\n", ldim_drv->duty_update_flag);
	} else if ((!strcmp(parm[0], "alg")) ||
		   (!strcmp(parm[0], "fw"))) {
		if (fw->fw_alg_para_print)
			fw->fw_alg_para_print(fw);
		else
			pr_info("ldim_fw para_print is null\n");
	} else {
		pr_info("no support cmd!!!\n");
	}

ldim_attr_store_end:
	kfree(buf_orig);
	kfree(parm);
	mutex_unlock(&ldim_dbg_mutex);
	return len;

ldim_attr_store_err:
	dbg_attr.cmd = LDIM_DBG_ATTR_CMD_ERR;
	pr_info("invalid cmd!!!\n");
	kfree(buf_orig);
	kfree(parm);
	mutex_unlock(&ldim_dbg_mutex);
	return -EINVAL;
}

static ssize_t ldim_func_en_show(struct class *class,
				 struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	ret = sprintf(buf, "%d\n", ldim_drv->func_en);

	return ret;
}

static ssize_t ldim_func_en_store(struct class *class,
				  struct class_attribute *attr,
				  const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int val = 0;
	int ret = 0;

	ret = kstrtouint(buf, 10, &val);
	LDIMPR("local diming function: %s\n", (val ? "enable" : "disable"));
	ldim_drv->conf->func_en = val ? 1 : 0;

	//LCD_CHIP_T3/T7 update in vsync isr
	if (bdrv->data->chip_type != LCD_CHIP_T3 &&
		bdrv->data->chip_type != LCD_CHIP_T7) {
		if (ldim_drv->data && ldim_drv->data->func_ctrl)
			ldim_drv->data->func_ctrl(ldim_drv,
			ldim_drv->conf->func_en);
	}
	return count;
}

static ssize_t ldim_remap_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	ret = sprintf(buf, "%d\n", ldim_drv->remap_en);

	return ret;
}

static ssize_t ldim_remap_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int val = 0;
	int ret = 0;

	ret = kstrtouint(buf, 10, &val);
	LDIMPR("local diming remap: %s\n", (val ? "enable" : "disable"));
	ldim_drv->conf->remap_en = val ? 1 : 0;

	//LCD_CHIP_T3/T7 update in vsync isr
	if (bdrv->data->chip_type != LCD_CHIP_T3 &&
		bdrv->data->chip_type != LCD_CHIP_T7) {
		ldim_remap_ctrl(ldim_drv->conf->remap_en);
	}
	return count;
}

static ssize_t ldim_para_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	struct ldim_fw_s *fw = aml_ldim_get_fw();
	int len = 0;

	len = sprintf(buf, "boost_gain=%d\n", fw->ctrl->boost_gain);

	return len;
}

static ssize_t ldim_mem_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int len = 0;

	switch (bdrv->data->chip_type) {
	case LCD_CHIP_TM2:
	case LCD_CHIP_TM2B:
		len = sprintf(buf,
			"wr_mem1_paddr:      0x%lx\n"
			"wr_mem1_vaddr:      0x%px\n"
			"wr_mem2_paddr:      0x%lx\n"
			"wr_mem2_vaddr:      0x%px\n"
			"rd_mem1_paddr:      0x%lx\n"
			"rd_mem1_vaddr:      0x%px\n"
			"rd_mem2_paddr:      0x%lx\n"
			"rd_mem2_vaddr:      0x%px\n",
			(unsigned long)ldim_drv->rmem->wr_mem_paddr1,
			ldim_drv->rmem->wr_mem_vaddr1,
			(unsigned long)ldim_drv->rmem->wr_mem_paddr2,
			ldim_drv->rmem->wr_mem_vaddr2,
			(unsigned long)ldim_drv->rmem->rd_mem_paddr1,
			ldim_drv->rmem->rd_mem_vaddr1,
			(unsigned long)ldim_drv->rmem->rd_mem_paddr2,
			ldim_drv->rmem->rd_mem_vaddr2);
		break;
	case LCD_CHIP_T7:
	case LCD_CHIP_T3:
		len = sprintf(buf,
			"rsv_mem_paddr:         0x%lx\n"
			"rsv_mem_size:          0x%x\n"
			"profile_mem_paddr:     0x%lx\n"
			"profile_mem_size:      0x%x\n"
			"global_hist_paddr:     0x%lx\n"
			"global_hist_vaddr:     0x%px\n"
			"global_hist_mem_size:  0x%x\n"
			"seg_hist_paddr:        0x%lx\n"
			"seg_hist_vaddr:        0x%px\n"
			"seg_hist_mem_size:     0x%x\n"
			"duty_paddr:            0x%lx\n"
			"duty_vaddr:            0x%px\n"
			"duty_mem_size:         0x%x\n",
			(unsigned long)ldim_drv->rmem->rsv_mem_paddr,
			ldim_drv->rmem->rsv_mem_size,
			(unsigned long)ldim_drv->rmem->profile_mem_paddr,
			ldim_drv->rmem->profile_mem_size,
			(unsigned long)ldim_drv->rmem->global_hist_paddr,
			ldim_drv->rmem->global_hist_vaddr,
			ldim_drv->rmem->global_hist_mem_size,
			(unsigned long)ldim_drv->rmem->seg_hist_paddr,
			ldim_drv->rmem->seg_hist_vaddr,
			ldim_drv->rmem->seg_hist_mem_size,
			(unsigned long)ldim_drv->rmem->duty_paddr,
			ldim_drv->rmem->duty_vaddr,
			ldim_drv->rmem->duty_mem_size);
		break;
	default:
		len = sprintf(buf, "invalid");
		break;
	}

	return len;
}

static ssize_t ldim_mem_store(struct class *class, struct class_attribute *attr,
			      const char *buf, size_t count)
{
	int i, ret;
	unsigned int *data_buf, region_num, data, index, temp;
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	region_num = ldim_drv->conf->seg_row * ldim_drv->conf->seg_col;

	switch (buf[0]) {
	case 'w':
		data_buf = (unsigned int *)ldim_drv->rmem->wr_mem_vaddr1;
		if (!data_buf) {
			LDIMERR("wr buffer is null\n");
			return count;
		}
		pr_info("ldim wr buffer:\n");
		for (i = 0; i < region_num * 20; i++) {
			pr_info(" 0x%p : %04x\n", data_buf, *data_buf);
			data_buf++;
		}
		pr_info("\n");
		break;
	case 'r':
		data_buf = (unsigned int *)ldim_drv->rmem->rd_mem_vaddr1;
		if (!data_buf) {
			LDIMERR("rd buffer is null\n");
			return count;
		}
		pr_info("ldim rd buffer:\n");
		for (i = 0; i < region_num; i++) {
			pr_info(" 0x%p : %04x\n", data_buf, *data_buf);
			data_buf++;
		}
		pr_info("\n");
		break;
	case 't':
		if (!ldim_drv->rmem->rd_mem_vaddr1) {
			LDIMERR("read_buf is null\n");
			break;
		}
		data_buf = (unsigned int *)ldim_drv->rmem->rd_mem_vaddr1;

		ret = sscanf(buf, "test %x %d", &data, &index);
		if (ret == 2) {
			ldim_drv->matrix_update_en = 0;
			msleep(20);
			data_buf += (index / 2);
			if (index % 2) {
				temp = *data_buf & 0xffff;
				*data_buf = temp | (data << 16);
			} else {
				temp = *data_buf & 0xffff0000;
				*data_buf = temp | data;
			}
			LDIMPR("set rd buffer %d to 0x%x\n", index, data);
		} else if (ret == 1) {
			ldim_drv->matrix_update_en = 0;
			msleep(20);
			temp = (region_num + 1) / 2;
			for (i = 0; i < temp; i++) {
				*data_buf = data | data << 16;
				data_buf++;
			}
			LDIMPR("set all rd buffer to 0x%x\n", data);
		} else {
			ldim_drv->matrix_update_en = 1;
			LDIMPR("disable mem test\n");
		}
		break;
	default:
		LDIMERR("no support cmd\n");
	}
	return count;
}

static ssize_t ldim_reg_show(struct class *class,
			     struct class_attribute *attr, char *buf)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	int len = 0;

	if (bdrv->data->chip_type == LCD_CHIP_TM2)
		len = ldim_hw_reg_dump_tm2(buf);
	else if (bdrv->data->chip_type == LCD_CHIP_TM2B)
		len = ldim_hw_reg_dump_tm2b(buf);
	else
		len = ldim_hw_reg_dump(buf);

	return len;
}

static ssize_t ldim_reg_store(struct class *class, struct class_attribute *attr,
			      const char *buf, size_t len)
{
	unsigned int n = 0, i;
	char *buf_orig, *ps, *token;
	char *parm[3];
	char str[3] = {' ', '\n', '\0'};
	unsigned int reg, val, size, temp;

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return len;
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!strcmp(parm[0], "rv")) {
		if (!parm[1])
			goto ldim_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_reg_store_err;
		val = lcd_vcbus_read(reg);
		pr_info("reg 0x%04x = 0x%08x\n", reg, val);
	} else if (!strcmp(parm[0], "dv")) {
		if (!parm[2])
			goto ldim_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_reg_store_err;
		if (kstrtouint(parm[2], 10, &size) < 0)
			goto ldim_reg_store_err;
		for (i = 0; i < size; i++) {
			val = lcd_vcbus_read(reg + i);
			pr_info("reg 0x%04x = 0x%08x\n", (reg + i), val);
		}
	} else if (!strcmp(parm[0], "wv")) {
		if (!parm[2])
			goto ldim_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_reg_store_err;
		if (kstrtouint(parm[2], 16, &val) < 0)
			goto ldim_reg_store_err;
		lcd_vcbus_write(reg, val);
		temp = lcd_vcbus_read(reg);
		pr_info("write reg 0x%04x = 0x%08x, readback 0x%08x\n", reg, val, temp);
	} else if (!strcmp(parm[0], "r")) {
		if (!parm[1])
			goto ldim_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_reg_store_err;
		val = ldim_rd_reg(reg);
		pr_info("ldim port reg 0x%04x = 0x%08x\n", reg, val);
	} else if (!strcmp(parm[0], "d")) {
		if (!parm[2])
			goto ldim_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_reg_store_err;
		if (kstrtouint(parm[2], 10, &size) < 0)
			goto ldim_reg_store_err;
		for (i = 0; i < size; i++) {
			val = ldim_rd_reg(reg + i);
			pr_info("ldim port reg 0x%04x = 0x%08x\n",
				(reg + i), val);
		}
	} else if (!strcmp(parm[0], "w")) {
		if (!parm[2])
			goto ldim_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_reg_store_err;
		if (kstrtouint(parm[2], 16, &val) < 0)
			goto ldim_reg_store_err;
		ldim_wr_reg(reg, val);
		msleep(30); /* wait for rdma write */
		temp = ldim_rd_reg(reg);
		pr_info("write ldim port reg 0x%04x = 0x%08x, readback 0x%08x\n",
			reg, val, temp);
	} else {
		pr_info("no support cmd!!!\n");
	}

	kfree(buf_orig);
	return len;

ldim_reg_store_err:
	LDIMERR("invalid parameters\n");
	kfree(buf_orig);
	return -EINVAL;
}

static ssize_t ldim_dbg_reg_show(struct class *class,
				 struct class_attribute *attr, char *buf)
{
	int len = 0;
	unsigned int i, reg;

	mutex_lock(&ldim_dbg_mutex);

	len += sprintf(buf + len, "for_tool:");
	switch (dbg_reg.rw_mode) {
	case LDIM_DBG_REG_RW_MODE_NULL:
		len += sprintf(buf + len, "NULL");
		break;
	case LDIM_DBG_REG_RW_MODE_RN:
		for (i = 0; i < dbg_reg.size; i++) {
			reg = dbg_reg.addr + i;
			len += sprintf(buf + len, "%04x=%08x ",
				       reg, lcd_vcbus_read(reg));
		}
		break;
	case LDIM_DBG_REG_RW_MODE_RS:
		reg = dbg_reg.addr;
		for (i = 0; i < dbg_reg.size; i++) {
			len += sprintf(buf + len, "%04x=%08x ",
				       reg, lcd_vcbus_read(reg));
		}
		break;
	case LDIM_DBG_REG_RW_MODE_WM:
		reg = dbg_reg.addr;
		len += sprintf(buf + len, "%04x=%08x ",
			       reg, lcd_vcbus_read(reg));
		break;
	case LDIM_DBG_REG_RW_MODE_WN:
		for (i = 0; i < dbg_reg.size; i++) {
			reg = dbg_reg.addr + i;
			len += sprintf(buf + len, "%04x=%08x ",
				       reg, lcd_vcbus_read(reg));
		}
		break;
	case LDIM_DBG_REG_RW_MODE_WS:
		reg = dbg_reg.addr;
		for (i = 0; i < dbg_reg.size; i++) {
			len += sprintf(buf + len, "%04x=%08x ",
				       reg, lcd_vcbus_read(reg));
		}
		break;
	case LDIM_DBG_REG_RW_MODE_ERR:
		len += sprintf(buf + len, "ERROR");
		break;
	default:
		len += sprintf(buf + len, "ERROR");
		dbg_reg.rw_mode = LDIM_DBG_REG_RW_MODE_NULL;
		dbg_reg.addr = 0;
		dbg_reg.size = 0;
		break;
	}
	len += sprintf(buf + len, "\n");
	mutex_unlock(&ldim_dbg_mutex);
	return len;
}

static ssize_t ldim_dbg_reg_store(struct class *class, struct class_attribute *attr,
				  const char *buf, size_t len)
{
	unsigned int n = 0, i;
	char *buf_orig, *ps, *token;
	char **parm = NULL;
	char str[3] = {' ', '\n', '\0'};
	unsigned int reg, mask, val, size, temp;

	if (!buf)
		return len;

	mutex_lock(&ldim_dbg_mutex);

	parm = kcalloc(1536, sizeof(char *), GFP_KERNEL);
	if (!parm) {
		mutex_unlock(&ldim_dbg_mutex);
		return len;
	}

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		kfree(parm);
		return len;
	}
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!strcmp(parm[0], "wmv")) {
		if (!parm[3])
			goto ldim_dbg_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_dbg_reg_store_err;
		if (kstrtouint(parm[2], 16, &mask) < 0)
			goto ldim_dbg_reg_store_err;
		if (kstrtouint(parm[3], 16, &val) < 0)
			goto ldim_dbg_reg_store_err;
		dbg_reg.size = 1; /* for cat use */
		dbg_reg.addr = reg;
		dbg_reg.rw_mode = LDIM_DBG_REG_RW_MODE_WM;
		temp = lcd_vcbus_read(reg);
		temp &= ~mask;
		temp |= val & mask;
		lcd_vcbus_write(reg, temp);
	} else if (!strcmp(parm[0], "wnv")) {//write reg auto inc
		if (!parm[2])
			goto ldim_dbg_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_dbg_reg_store_err;
		if (kstrtouint(parm[2], 10, &size) < 0)
			goto ldim_dbg_reg_store_err;
		if (size == 0)
			goto ldim_dbg_reg_store_err;
		if (!parm[3 + size - 1])
			goto ldim_dbg_reg_store_err;
		dbg_reg.size = size; /* for cat use */
		dbg_reg.addr = reg;
		dbg_reg.rw_mode = LDIM_DBG_REG_RW_MODE_WN;
		for (i = 0; i < size; i++) {
			if (kstrtouint(parm[i + 3], 16, &val) < 0)
				goto ldim_dbg_reg_store_err;
			lcd_vcbus_write(reg + i, val);
		}
	} else if (!strcmp(parm[0], "wsv")) { //write reg static
		if (!parm[2])
			goto ldim_dbg_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_dbg_reg_store_err;
		if (kstrtouint(parm[2], 10, &size) < 0)
			goto ldim_dbg_reg_store_err;
		if (size == 0)
			goto ldim_dbg_reg_store_err;
		if (!parm[3 + size - 1])
			goto ldim_dbg_reg_store_err;
		dbg_reg.size = size; /* for cat use */
		dbg_reg.addr = reg;
		dbg_reg.rw_mode = LDIM_DBG_REG_RW_MODE_WS;
		for (i = 0; i < size; i++) {
			if (kstrtouint(parm[i + 3], 16, &val) < 0)
				goto ldim_dbg_reg_store_err;
			lcd_vcbus_write(reg, val);
		}
	} else if (!strcmp(parm[0], "rnv")) { //read reg
		if (!parm[1])
			goto ldim_dbg_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_dbg_reg_store_err;
		if (!parm[2]) { //read one reg
			dbg_reg.size = 1; /* for cat use */
			dbg_reg.addr = reg;
			dbg_reg.rw_mode = LDIM_DBG_REG_RW_MODE_RN;
		} else {
			if (kstrtouint(parm[2], 10, &size) < 0)
				goto ldim_dbg_reg_store_err;
			if (size == 0)
				goto ldim_dbg_reg_store_err;
			dbg_reg.size = size; /* for cat use */
			dbg_reg.addr = reg;
			dbg_reg.rw_mode = LDIM_DBG_REG_RW_MODE_RN;
		}
	} else if (!strcmp(parm[0], "rsv")) { //read static reg
		if (!parm[1])
			goto ldim_dbg_reg_store_err;
		if (kstrtouint(parm[1], 16, &reg) < 0)
			goto ldim_dbg_reg_store_err;
		if (!parm[2]) { //read one reg
			dbg_reg.size = 1; /* for cat use */
			dbg_reg.addr = reg;
			dbg_reg.rw_mode = LDIM_DBG_REG_RW_MODE_RS;
		} else {
			if (kstrtouint(parm[2], 10, &size) < 0)
				goto ldim_dbg_reg_store_err;
			if (size == 0)
				goto ldim_dbg_reg_store_err;
			dbg_reg.size = size; /* for cat use */
			dbg_reg.addr = reg;
			dbg_reg.rw_mode = LDIM_DBG_REG_RW_MODE_RS;
		}
	} else {
		goto ldim_dbg_reg_store_err;
	}

	kfree(buf_orig);
	kfree(parm);
	mutex_unlock(&ldim_dbg_mutex);
	return len;

ldim_dbg_reg_store_err:
	dbg_reg.rw_mode = LDIM_DBG_REG_RW_MODE_ERR;
	LDIMERR("invalid parameters\n");
	kfree(buf_orig);
	kfree(parm);
	mutex_unlock(&ldim_dbg_mutex);
	return -EINVAL;
}

static ssize_t ldim_demo_show(struct class *class, struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	ret = sprintf(buf, "%d\n", ldim_drv->demo_en);

	return ret;
}

static ssize_t ldim_demo_store(struct class *class, struct class_attribute *attr,
			       const char *buf, size_t count)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int val = 0;

	if (kstrtouint(buf, 10, &val) < 0)
		return count;
	LDIMPR("local diming demo: %d\n", val);
	ldim_drv->conf->demo_en = val ? 1 : 0;
	if (val) {
		if (ldim_drv->remap_en) {
			ldim_drv->demo_en = 1;
			ldim_drv->fw->ctrl->nprm->reg_ld_rgb_mapping_demo = 1;
			ldim_hw_remap_demo_en(1);
		} else {
			pr_info("error: ldim_remap is disabled\n");
		}
	} else {
		ldim_drv->demo_en = 0;
		ldim_drv->fw->ctrl->nprm->reg_ld_rgb_mapping_demo = 0;
		ldim_hw_remap_demo_en(0);
	}
	return count;
}

static void ldc_rmem_data_dump(struct aml_ldim_driver_s *ldim_drv)
{
	if (ldim_drv->rmem->flag == 0) {
		pr_info("ldc resv mem invalid\n");
		return;
	}

	pr_info("ldc global hist:\n");
	ldc_mem_dump(ldim_drv->rmem->global_hist_vaddr, ldim_drv->rmem->global_hist_mem_size);
	pr_info("ldc seg hist:\n");
	ldc_mem_dump(ldim_drv->rmem->seg_hist_vaddr, ldim_drv->rmem->seg_hist_mem_size);
	pr_info("ldc duty:\n");
	ldc_mem_dump(ldim_drv->rmem->duty_vaddr, ldim_drv->rmem->duty_mem_size);
}

static ssize_t ldim_debug_show(struct class *class, struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len,
		      "echo en <width> <height> <col> <row> > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo reg > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo save <type> <path> > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo clr <type> > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "echo dump <type> > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "    type: profile/lut/mem\n");
	len += sprintf(buf + len,
		       "echo load <type> <path> > /sys/class/aml_ldim/attr\n");
	len += sprintf(buf + len,
		       "    type: profile/lut\n");

	return len;
}

static unsigned int ldc_dbg_point_ctrl[][2] = {
	//pnt_index, bit_width
	{1,          12},
	{2,          12},
	{3,          12},
	{4,          12},
	{5,          12},
	{6,          12},
	{7,          16},
	{8,          14}
};

static int ldc_debug_dummy(void)
{
	pr_info("todo\n");
	return 0;
}

static void ldc_gain_lut_load(struct aml_ldim_driver_s *ldim_drv, char *path)
{
	unsigned int size = 0, file_size = 0, len = 0;
	char *buf;
	unsigned int *p;
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	/*struct kstat stat;*/
	unsigned int i, j, n;
	char *ps, *token;
	char str[4] = {',', ' ', '\n', '\0'};

	set_fs(KERNEL_DS);

	/*vfs_stat(path, &stat);
	 *file_size = stat.size;
	 */

	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("read %s error or filp is NULL.\n", path);
		return;
	}
	file_size = filp->f_inode->i_size;
	buf = kzalloc((file_size + 2), GFP_KERNEL);
	if (!buf) {
		filp_close(filp, NULL);
		set_fs(old_fs);
		return;
	}

	size = vfs_read(filp, buf, file_size, &pos);
	if (size < file_size) {
		pr_info("%s read size %u < %u error.\n",
			__func__, size, file_size);
		filp_close(filp, NULL);
		set_fs(old_fs);
		kfree(buf);
		return;
	}
	vfs_fsync(filp, 0);

	filp_close(filp, NULL);
	set_fs(old_fs);

	ps = buf;
	len = 0;
	i = 0;
	j = 0;
	n = 0;
	while (1) {
		if (len >= file_size)
			break;
		if (!ps)
			break;
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0') {
			len++;
			continue;
		}
		p = ldc_gain_lut_array[n];
		if (kstrtouint(token, 16, &p[j]) < 0) {
			kfree(buf);
			return;
		}
		if (j == 63) {
			j = 0;
			n++;
		} else {
			j++;
		}
		len += (strlen(token) + 1);
		i++;
	}
	if (ldim_drv->data->remap_lut_update)
		ldim_drv->data->remap_lut_update();

	pr_info("load setting file path: %s finish, total line %d\n", path, n);
	kfree(buf);
}

static void ldim_time_print(unsigned long long *table)
{
	unsigned int len, i;
	char *buf;

	buf = kcalloc(1024, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = 0;
	for (i = 0; i < 9; i++)
		len += sprintf(buf + len, " %lld,", table[i]);
	len += sprintf(buf + len, " %lld", table[9]);
	pr_info("%s\n", buf);

	kfree(buf);
}

static ssize_t ldim_debug_store(struct class *class, struct class_attribute *attr,
				const char *buf, size_t len)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int n = 0;
	char *buf_orig, *ps, *token;
	char **parm = NULL;
	char str[3] = {' ', '\n', '\0'};
	unsigned int temp, val, num;
	int i, j;

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return len;
	parm = kcalloc(128, sizeof(char *), GFP_KERNEL);
	if (!parm) {
		kfree(buf_orig);
		return len;
	}
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	num = ldim_drv->conf->seg_col * ldim_drv->conf->seg_row;

	if (!strcmp(parm[0], "en")) {
		if (!parm[1])
			goto ldim_debug_store_err;
		if (kstrtouint(parm[1], 10, &temp) < 0)
			goto ldim_debug_store_err;
		ldim_drv->func_en = temp ? 1 : 0;
		if (ldim_drv->data && ldim_drv->data->func_ctrl)
			ldim_drv->data->func_ctrl(ldim_drv, ldim_drv->func_en);
	} else if (!strcmp(parm[0], "fw_sel")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &temp) < 0)
				goto ldim_debug_store_err;
			ldim_drv->fw->fw_sel = (unsigned char)temp;
			ldim_config_update_t7(ldim_drv);
		}
		pr_info("fw_sel = %d\n", ldim_drv->fw->fw_sel);
	} else if (!strcmp(parm[0], "test")) {
		if (!parm[1])
			goto ldim_debug_store_err;
		if (!strcmp(parm[1], "on")) {
			ldim_drv->test_bl_en = 1;
			goto ldim_debug_store_end;
		}
		if (!strcmp(parm[1], "off")) {
			ldim_drv->test_bl_en = 0;
			goto ldim_debug_store_end;
		}
		if (!strcmp(parm[1], "run")) {
			ldim_drv->test_bl_en = 1;
			for (i = 0; i < num; i++) {
				for (j = 0; j < num; j++)
					ldim_drv->test_matrix[j] = 0;
				ldim_drv->test_matrix[i] = 4095;
				msleep(500);
			}
			ldim_drv->test_bl_en = 0;
			goto ldim_debug_store_end;
		}
		if (parm[2]) {
			if (!strcmp(parm[1], "remap")) {
				if (kstrtouint(parm[2], 10, &temp) < 0)
					goto ldim_debug_store_err;
				ldim_drv->test_remap_en = temp;
				pr_info("test_remap_en = %d\n",
					ldim_drv->test_remap_en);
				goto ldim_debug_store_end;
			}
		}
		if (parm[3]) {
			if (kstrtouint(parm[2], 10, &temp) < 0)
				goto ldim_debug_store_err;
			if (kstrtouint(parm[3], 10, &val) < 0)
				goto ldim_debug_store_err;
			if (temp >= num)
				goto ldim_debug_store_err;
			ldim_drv->test_matrix[temp] = val;
		} else if (parm[2]) {
			if (kstrtouint(parm[2], 10, &val) < 0)
				goto ldim_debug_store_err;
			for (i = 0; i < num; i++)
				ldim_drv->test_matrix[i] = val;
		}
	} else if (!strcmp(parm[0], "bypass")) {
		if (!parm[1])
			goto ldim_debug_store_err;
		if (!strcmp(parm[1], "dev")) {
			if (kstrtouint(parm[2], 10, &temp) < 0)
				goto ldim_debug_store_err;
			ldim_drv->dev_smr_bypass = temp;
		}
	} else if (!strcmp(parm[0], "print")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &temp) < 0)
				goto ldim_debug_store_err;
			ldim_debug_print = (unsigned char)temp;
		}
		pr_info("ldim_debug_print = %d\n", ldim_debug_print);
	} else if (!strcmp(parm[0], "pnt")) {
		if (!parm[1])
			goto ldim_debug_store_err;
		if (!strcmp(parm[1], "off")) {
			lcd_vcbus_setb(LDC_DGB_CTRL, 0, 0, 1);
			pr_info("ldc dbg point off\n");
			goto ldim_debug_store_end;
		}
		if (kstrtouint(parm[1], 10, &i) < 0)
			goto ldim_debug_store_err;
		if (i < 1 || i > 8) {
			lcd_vcbus_setb(LDC_DGB_CTRL, 0, 0, 1);
			pr_info("ldc dbg point off\n");
			goto ldim_debug_store_end;
		}
		if (parm[2]) {
			if (kstrtouint(parm[2], 10, &val) < 0)
				goto ldim_debug_store_err;
		} else {
			val = ldc_dbg_point_ctrl[i - 1][1];
		}
		temp = val - 10;
		lcd_vcbus_setb(LDC_DGB_CTRL, i, 1, 4);
		lcd_vcbus_setb(LDC_DGB_CTRL, temp, 5, 4);
		lcd_vcbus_setb(LDC_DGB_CTRL, 1, 0, 1);
	} else if (!strcmp(parm[0], "time")) {
		pr_info("arithmetic_time:\n");
		ldim_time_print(ldim_drv->arithmetic_time);
		pr_info("xfer_time:\n");
		ldim_time_print(ldim_drv->xfer_time);
	} else if (!strcmp(parm[0], "reg")) {
		//dump reg todo
	} else if (!strcmp(parm[0], "save")) { /* save buf to bin */
		if (!parm[2])
			goto ldim_debug_store_err;

		if (!strcmp(parm[1], "profile")) {
			//todo
			ldc_debug_dummy();
		} else if (!strcmp(parm[1], "lut")) {
			//todo
			ldc_debug_dummy();
		} else if (!strcmp(parm[1], "mem")) {
			if (ldim_drv->rmem->flag == 0) {
				pr_info("ldc resv mem invalid\n");
				goto ldim_debug_store_err;
			}
			ldc_mem_save(parm[2],
				     ldim_drv->rmem->rsv_mem_paddr,
				     ldim_drv->rmem->rsv_mem_size);
		} else {
			goto ldim_debug_store_err;
		}
	} else if (!strcmp(parm[0], "load")) { /*load bin to buf*/
		if (!parm[2])
			goto ldim_debug_store_err;
		if (!strcmp(parm[1], "profile")) {
			if (ldim_drv->rmem->flag == 0) {
				pr_info("ldc resv mem invalid\n");
				goto ldim_debug_store_err;
			}
			ldc_mem_write(parm[2],
				      ldim_drv->rmem->profile_mem_paddr,
				      ldim_drv->rmem->profile_mem_size);
		} else if (!strcmp(parm[1], "lut")) {
			ldc_gain_lut_load(ldim_drv, parm[2]);
		} else {
			goto ldim_debug_store_err;
		}
	} else if (!strcmp(parm[0], "clr")) {
		if (!parm[2])
			goto ldim_debug_store_err;
		if (!strcmp(parm[1], "profile")) {
			//todo
			ldc_debug_dummy();
		} else if (!strcmp(parm[1], "lut")) {
			//todo
			ldc_debug_dummy();
		} else if (!strcmp(parm[1], "mem")) {
			if (ldim_drv->rmem->flag == 0) {
				pr_info("ldc resv mem invalid\n");
				goto ldim_debug_store_err;
			}
			ldc_mem_clear(ldim_drv->rmem->rsv_mem_paddr,
				      ldim_drv->rmem->rsv_mem_size);
		} else {
			goto ldim_debug_store_err;
		}
	} else if (!strcmp(parm[0], "set")) {
		if (!parm[1])
			goto ldim_debug_store_err;
		if (!strcmp(parm[1], "profile")) {
			//todo
			ldc_debug_dummy();
		} else if (!strcmp(parm[1], "lut")) {
			//todo
			ldc_debug_dummy();
		} else if (!strcmp(parm[1], "duty")) {
			if (ldim_drv->rmem->flag == 0) {
				pr_info("ldc resv mem invalid\n");
				goto ldim_debug_store_err;
			}
			ldc_mem_set(ldim_drv->rmem->duty_paddr,
				    ldim_drv->rmem->duty_mem_size);
		} else {
			goto ldim_debug_store_err;
		}
	} else if (!strcmp(parm[0], "dump")) {
		if (!parm[1])
			goto ldim_debug_store_err;
		if (!strcmp(parm[1], "profile")) {
			//todo
			ldc_debug_dummy();
		} else if (!strcmp(parm[1], "lut")) {
			//todo
			ldc_debug_dummy();
		} else if (!strcmp(parm[1], "mem")) {
			ldc_rmem_data_dump(ldim_drv);
		} else {
			goto ldim_debug_store_err;
		}
	} else {
		pr_info("no support cmd!!!\n");
	}

ldim_debug_store_end:
	kfree(buf_orig);
	kfree(parm);
	return len;

ldim_debug_store_err:
	pr_info("invalid parameters\n");
	kfree(buf_orig);
	kfree(parm);
	return -EINVAL;
}

static struct class_attribute aml_ldim_class_attrs[] = {
	__ATTR(attr, 0644, ldim_attr_show, ldim_attr_store),
	__ATTR(func_en, 0644, ldim_func_en_show, ldim_func_en_store),
	__ATTR(remap, 0644, ldim_remap_show, ldim_remap_store),
	__ATTR(para, 0644, ldim_para_show, NULL),
	__ATTR(mem, 0644, ldim_mem_show, ldim_mem_store),
	__ATTR(reg, 0644, ldim_reg_show, ldim_reg_store),
	__ATTR(dbg_reg, 0644, ldim_dbg_reg_show, ldim_dbg_reg_store),
	__ATTR(demo, 0644, ldim_demo_show, ldim_demo_store),
	__ATTR(debug, 0644, ldim_debug_show, ldim_debug_store)
};

int aml_ldim_debug_probe(struct class *ldim_class)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aml_ldim_class_attrs); i++) {
		if (class_create_file(ldim_class, &aml_ldim_class_attrs[i]) < 0)
			return -1;
	}

	mutex_init(&ldim_dbg_mutex);

	return 0;
}

void aml_ldim_debug_remove(struct class *ldim_class)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aml_ldim_class_attrs); i++)
		class_remove_file(ldim_class, &aml_ldim_class_attrs[i]);
}

