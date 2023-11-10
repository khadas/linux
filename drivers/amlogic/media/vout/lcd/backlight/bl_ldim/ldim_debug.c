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
	unsigned int index;
};

struct ldim_dbg_attr_s dbg_attr = {
	.cmd = LDIM_DBG_ATTR_CMD_NULL,
	.chip_type = LCD_CHIP_MAX,
	.mode = LDIM_DBG_ATTR_MODE_SINGLE,
	.data = 0,
	.index = 0,
};

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

int ldim_debug_buf_save(char *path, unsigned char *save_buf,
			       unsigned int size)
{
#ifdef CONFIG_AMLOGIC_ENABLE_MEDIA_FILE
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;

	if (!save_buf) {
		LDIMERR("%s: save_buf is null\n", __func__);
		return -1;
	}
	if (size == 0) {
		LDIMERR("%s: size is zero\n", __func__);
		return -1;
	}

	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	if (IS_ERR(filp)) {
		LDIMERR("%s: create %s error\n", __func__, path);
		return -1;
	}

	pos = 0;
	buf = (void *)save_buf;
	vfs_write(filp, buf, size, &pos);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);

	pr_info("debug file %s save finished\n", path);

#endif
	return 0;
}
EXPORT_SYMBOL(ldim_debug_buf_save);

static ssize_t ldim_bl_matrix_get(struct aml_ldim_driver_s *ldim_drv, char *buf, char istest)
{
	unsigned int i, n, len;
	unsigned int *p = NULL;
	unsigned int startpos;
	unsigned int maxlen = PAGE_SIZE - 50;

	n = ldim_drv->conf->seg_row * ldim_drv->conf->seg_col;
	p = kcalloc(n, sizeof(unsigned int), GFP_KERNEL);
	if (!p) {
		kfree(buf);
		return 0;
	}
	if (istest)
		memcpy(p, ldim_drv->test_matrix, (n * sizeof(unsigned int)));
	else
		memcpy(p, ldim_drv->local_bl_matrix, (n * sizeof(unsigned int)));

	startpos = dbg_attr.index;
	if (startpos)
		len = sprintf(buf, "for_tool:");
	else
		len = sprintf(buf, "for_tool: %d %d",
		      ldim_drv->conf->seg_row, ldim_drv->conf->seg_col);

	for (i = startpos; i < n; i++) {
		len += sprintf(buf + len, " %d", p[i]);
		if (len > maxlen) {
			len += sprintf(buf + len, "\n");
			dbg_attr.index = i + 1;
			goto ldim_bl_matrix_get_end;
		}
	}
	len += sprintf(buf + len, "for_tool_end\n");
	dbg_attr.index = 0;

ldim_bl_matrix_get_end:
	kfree(p);
	return len;
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

	pr_info("litgain: %d\n", ldim_drv->litgain);
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

static ssize_t ldim_attr_show(struct class *cla, struct class_attribute *attr,
			      char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_s *fw = ldim_drv->fw;
	ssize_t len;

	len = fw->fw_debug_show(fw, LDC_DBG_ATTR, buf);
	if (len)
		return len;

	mutex_lock(&ldim_dbg_mutex);

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
		len += ldim_bl_matrix_get(ldim_drv, buf, 0);
		break;
	case LDIM_DBG_ATTR_MODE_TEST_MATRIX:
		len += ldim_bl_matrix_get(ldim_drv, buf, 1);
		break;
	default:
		len = sprintf(buf, "%s: error\n", __func__);
		break;
	}

	mutex_unlock(&ldim_dbg_mutex);
	return len;
}

static ssize_t ldim_attr_store(struct class *cla, struct class_attribute *attr,
			       const char *buf, size_t len)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_s *fw = ldim_drv->fw;
	struct ldim_fw_custom_s *cus_fw = ldim_drv->cus_fw;
	unsigned int n = 0;
	unsigned int i, j, k;
	char *buf_orig, *ps, *token;
	char **parm = NULL;
	char str[3] = {' ', '\n', '\0'};

	unsigned long val1 = 0;
	size_t ret = 0;
	unsigned int seg_size;

	if (!fw)
		return len;

	if (!buf)
		return len;

	mutex_lock(&ldim_dbg_mutex);

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		mutex_unlock(&ldim_dbg_mutex);
		return len;
	}
	ret = fw->fw_debug_store(fw, LDC_DBG_ATTR, buf_orig, len);
	if (len == ret)
		goto ldim_attr_store_end1;

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

	seg_size = ldim_drv->conf->seg_row * ldim_drv->conf->seg_col;

	if (!strcmp(parm[0], "fw_sel")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = fw->fw_sel;
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			if (val1)
				fw->fw_sel = val1;
			else
				fw->fw_sel = 0;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = fw->fw_sel;
		}
		pr_info("fw_sel: %d\n", fw->fw_sel);
	} else if (!strcmp(parm[0], "func") ||
		   !strcmp(parm[0], "func_en")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
				dbg_attr.data = ldim_drv->func_en;
				goto ldim_attr_store_end;
			}
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->func_en = val1 ? 1 : 0;
			dbg_attr.cmd = LDIM_DBG_ATTR_CMD_WR;
			dbg_attr.mode = LDIM_DBG_ATTR_MODE_SINGLE;
			dbg_attr.data = ldim_drv->func_en;
		}
		pr_info("ldim_func_en: %d\n", ldim_drv->func_en);
	} else if (!strcmp(parm[0], "matrix")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_BL_MATRIX;
				dbg_attr.index = 0;
				goto ldim_attr_store_end;
			}
			goto ldim_attr_store_err;
		}
		ldim_dump_bl_matrix(ldim_drv);
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
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				dbg_attr.cmd = LDIM_DBG_ATTR_CMD_RD;
				dbg_attr.mode = LDIM_DBG_ATTR_MODE_TEST_MATRIX;
				dbg_attr.index = 0;
				ldim_drv->level_update = 1;
				goto ldim_attr_store_end;
			} else if (!strcmp(parm[1], "w")) {
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
			cus_fw->fw_print_frequent = fw->fw_print_frequent;
		}
		pr_info("fw_print_frequent = %d\n", fw->fw_print_frequent);
	} else if (!strcmp(parm[0], "fw_print")) {
		if (parm[1]) {
			if (!strcmp(parm[1], "r")) {
				pr_info("for_tool:%d\n",
					fw->fw_print_lv);
				goto ldim_attr_store_end;
			}
			if (kstrtouint(parm[1], 10, &fw->fw_print_lv) < 0)
				goto ldim_attr_store_err;
			cus_fw->fw_print_lv = fw->fw_print_lv;
		}
		pr_info("fw_print_lv = %d\n", fw->fw_print_lv);
	} else if (!strcmp(parm[0], "cus_fw_param")) {
		if (parm[2]) {
			if (kstrtouint(parm[1], 10, &i) < 0)
				goto ldim_attr_store_err;
			if (kstrtouint(parm[2], 10, &j) < 0)
				goto ldim_attr_store_err;

			if (cus_fw->fw_alg_frm)
				cus_fw->param[i] = j;
		}
		for (k = 0; k < 32; k++) {
			if (cus_fw->fw_alg_frm) {
				pr_info("cus_fw[%d] = %d\n",
					k, cus_fw->param[k]);
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
			"ldim_demo_mode          = %d\n"
			"ldim_ld_sel           = %d\n"
			"ldim_func_bypass      = %d\n"
			"ldim_test_bl_en       = %d\n"
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
			ldim_drv->fw->conf->remap_en, ldim_drv->demo_mode,
			ldim_drv->ld_sel, ldim_drv->func_bypass,
			ldim_drv->test_bl_en,
			ldim_drv->data_min, ldim_drv->data_max,
			ldim_drv->litgain,
			ldim_drv->fw->valid,
			ldim_drv->fw->fw_sel,
			ldim_drv->fw->flag,
			ldim_drv->irq_cnt,
			ldim_drv->duty_update_flag);
		aml_ldim_rmem_info();
	} else if (!strcmp(parm[0], "print")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_debug_print = (unsigned char)val1;
		}
		pr_info("ldim_debug_print = %d\n", ldim_debug_print);
	} else if (!strcmp(parm[0], "level_idx")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->level_idx = (unsigned char)val1;

			fw->fw_ctrl &= ~0xf;
			fw->fw_ctrl |= ldim_drv->level_idx;

			fw_pq = ldim_pq.pqdata[ldim_drv->level_idx];
			if (ldim_drv->fw->fw_pq_set)
				ldim_drv->fw->fw_pq_set(&fw_pq);

			ldim_drv->brightness_bypass = 0;
		}
		pr_info("level_idx = %d\n", ldim_drv->level_idx);
	} else if (!strcmp(parm[0], "spiout_mode")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val1) < 0)
				goto ldim_attr_store_err;
			ldim_drv->spiout_mode = (unsigned char)val1;
		}
		pr_info("spiout_mode = %d\n", ldim_drv->spiout_mode);
	} else if (!strcmp(parm[0], "fw")) {
		if (fw->fw_alg_para_print)
			fw->fw_alg_para_print(fw);
		else
			pr_info("ldim_fw para_print is null\n");
	} else {
		pr_info("invalid cmd!!!\n");
	}

ldim_attr_store_end:
	kfree(buf_orig);
	kfree(parm);
	mutex_unlock(&ldim_dbg_mutex);
	return len;

ldim_attr_store_end1:
	kfree(buf_orig);
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
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int val = 0;
	int ret = 0;

	ret = kstrtouint(buf, 10, &val);
	LDIMPR("local diming function: %s\n", (val ? "enable" : "disable"));
	ldim_drv->func_en = val ? 1 : 0;

	return count;
}

static ssize_t ldim_remap_show(struct class *class,
			       struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	ret = sprintf(buf, "%d\n", ldim_drv->fw->conf->remap_en);

	return ret;
}

static ssize_t ldim_remap_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	unsigned int val = 0;
	int ret = 0;

	ret = kstrtouint(buf, 10, &val);
	LDIMPR("local diming remap: %s\n", (val ? "enable" : "disable"));
	ldim_drv->fw->conf->remap_en = val ? 1 : 0;

	return count;
}

static ssize_t ldim_para_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_s *fw = ldim_drv->fw;
	ssize_t len;

	len = fw->fw_debug_show(fw, LDC_DBG_PARA, buf);

	return len;
}

static ssize_t ldim_para_store(struct class *class, struct class_attribute *attr,
				  const char *buf, size_t len)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_s *fw = ldim_drv->fw;
	char *buf_orig;

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return len;

	len = fw->fw_debug_store(fw, LDC_DBG_PARA, buf_orig, len);

	kfree(buf_orig);
	return len;
}

static ssize_t ldim_mem_show(struct class *class,
			      struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_s *fw = ldim_drv->fw;
	ssize_t len;

	len = fw->fw_debug_show(fw, LDC_DBG_MEM, buf);

	return len;
}

static ssize_t ldim_mem_store(struct class *class, struct class_attribute *attr,
			      const char *buf, size_t len)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_s *fw = ldim_drv->fw;
	char *buf_orig;

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return len;

	len = fw->fw_debug_store(fw, LDC_DBG_MEM, buf_orig, len);

	kfree(buf_orig);
	return len;
}

static ssize_t ldim_reg_show(struct class *class,
			     struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_s *fw = ldim_drv->fw;
	ssize_t len;

	len = fw->fw_debug_show(fw, LDC_DBG_REG, buf);

	return len;
}

static ssize_t ldim_reg_store(struct class *class, struct class_attribute *attr,
			      const char *buf, size_t len)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_s *fw = ldim_drv->fw;
	char *buf_orig;

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return len;

	len = fw->fw_debug_store(fw, LDC_DBG_REG, buf_orig, len);

	kfree(buf_orig);
	return len;
}

static ssize_t ldim_dbg_reg_show(struct class *class,
				 struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_s *fw = ldim_drv->fw;
	ssize_t len;

	len = fw->fw_debug_show(fw, LDC_DBG_DBGREG, buf);

	return len;
}

static ssize_t ldim_dbg_reg_store(struct class *class, struct class_attribute *attr,
				  const char *buf, size_t len)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_s *fw = ldim_drv->fw;
	char *buf_orig;

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return len;

	len = fw->fw_debug_store(fw, LDC_DBG_DBGREG, buf_orig, len);

	kfree(buf_orig);
	return len;
}

static ssize_t ldim_demo_show(struct class *class, struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	ret = sprintf(buf, "%d\n", ldim_drv->demo_mode);

	return ret;
}

static ssize_t ldim_demo_store(struct class *class, struct class_attribute *attr,
			       const char *buf, size_t count)
{
	return 0;
}

static ssize_t ldim_debug_show(struct class *class, struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_s *fw = ldim_drv->fw;
	ssize_t len;

	len = fw->fw_debug_show(fw, LDC_DBG_DEBUG, buf);

	return len;
}

static ssize_t ldim_debug_store(struct class *class, struct class_attribute *attr,
				const char *buf, size_t len)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_fw_s *fw = ldim_drv->fw;
	unsigned int n = 0;
	char *buf_orig;
	size_t ret = 0;
	unsigned int seg_size;
	unsigned int i, j;
	unsigned int temp, val;
	char *ps, *token;
	char **parm = NULL;
	char str[3] = {' ', '\n', '\0'};

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return len;
	mutex_lock(&ldim_dbg_mutex);

	ret = fw->fw_debug_store(fw, LDC_DBG_DEBUG, buf_orig, len);
	if (len == ret)
		goto ldim_debug_store_end1;

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

	seg_size = ldim_drv->conf->seg_row * ldim_drv->conf->seg_col;

	if (!strcmp(parm[0], "test")) {
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
			for (i = 0; i < seg_size; i++) {
				for (j = 0; j < seg_size; j++)
					ldim_drv->test_matrix[j] = 0;
				ldim_drv->test_matrix[i] = 4095;
				msleep(500);
			}
			ldim_drv->test_bl_en = 0;
			goto ldim_debug_store_end;
		}
		if (parm[3]) {
			if (kstrtouint(parm[2], 10, &temp) < 0)
				goto ldim_debug_store_err;
			if (kstrtouint(parm[3], 10, &val) < 0)
				goto ldim_debug_store_err;
			if (temp >= seg_size)
				goto ldim_debug_store_err;
			ldim_drv->test_matrix[temp] = val;
		} else if (parm[2]) {
			if (kstrtouint(parm[2], 10, &val) < 0)
				goto ldim_debug_store_err;
			for (i = 0; i < seg_size; i++)
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
		if (!strcmp(parm[1], "brightness")) {
			if (kstrtouint(parm[2], 10, &temp) < 0)
				goto ldim_debug_store_err;
			ldim_drv->brightness_bypass = temp;
		}
	} else if (!strcmp(parm[0], "print")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &temp) < 0)
				goto ldim_debug_store_err;
			ldim_debug_print = (unsigned char)temp;
		}
		pr_info("ldim_debug_print = %d\n", ldim_debug_print);
	} else if (!strcmp(parm[0], "time")) {
		pr_info("arithmetic_time:\n");
		ldim_time_print(ldim_drv->arithmetic_time);
		pr_info("xfer_time:\n");
		ldim_time_print(ldim_drv->xfer_time);
	} else {
		pr_info("no support cmd!!!\n");
	}

ldim_debug_store_end:
	kfree(buf_orig);
	kfree(parm);
	mutex_unlock(&ldim_dbg_mutex);
	return len;

ldim_debug_store_end1:
	kfree(buf_orig);
	mutex_unlock(&ldim_dbg_mutex);
	return len;

ldim_debug_store_err:
	dbg_attr.cmd = LDIM_DBG_ATTR_CMD_ERR;
	pr_info("invalid cmd!!!\n");
	kfree(buf_orig);
	kfree(parm);
	mutex_unlock(&ldim_dbg_mutex);
	return -EINVAL;
}

static struct class_attribute aml_ldim_class_attrs[] = {
	__ATTR(attr, 0644, ldim_attr_show, ldim_attr_store),
	__ATTR(func_en, 0644, ldim_func_en_show, ldim_func_en_store),
	__ATTR(remap, 0644, ldim_remap_show, ldim_remap_store),
	__ATTR(para, 0644, ldim_para_show, ldim_para_store),
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
