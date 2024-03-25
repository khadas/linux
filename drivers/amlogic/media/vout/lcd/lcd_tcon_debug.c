// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/compat.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_tcon_data.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/lcd_tcon_fw.h>
#include "lcd_reg.h"
#include "lcd_common.h"
#include "lcd_tcon.h"

//1: unlocked, 0: locked, negative: locked, possible waiters
struct mutex lcd_tcon_dbg_mutex;

/*tcon adb port use */
#define LCD_ADB_TCON_REG_RW_MODE_NULL              0
#define LCD_ADB_TCON_REG_RW_MODE_RN                1
#define LCD_ADB_TCON_REG_RW_MODE_WM                2
#define LCD_ADB_TCON_REG_RW_MODE_WN                3
#define LCD_ADB_TCON_REG_RW_MODE_WS                4
#define LCD_ADB_TCON_REG_RW_MODE_ERR               5

#define ADB_TCON_REG_8_bit                         0
#define ADB_TCON_REG_32_bit                        1

struct lcd_tcon_adb_reg_s {
	unsigned int rw_mode;
	unsigned int bit_width;
	unsigned int addr;
	unsigned int len;
};

/*for tconless reg adb use*/
static struct lcd_tcon_adb_reg_s adb_reg = {
	.rw_mode = LCD_ADB_TCON_REG_RW_MODE_NULL,
	.bit_width = ADB_TCON_REG_8_bit,
	.addr = 0,
	.len = 0,
};

static const char *lcd_debug_tcon_usage_str = {
	"Usage:\n"
	"    echo reg > tcon ; print tcon system regs\n"
	"    echo reg rb <reg> > tcon ; read tcon byte reg\n"
	"    echo reg wb <reg> <val> > tcon ; write tcon byte reg\n"
	"    echo reg db <reg> <cnt> > tcon ; dump tcon byte regs\n"
	"    echo reg r <reg> > tcon ; write tcon reg\n"
	"    echo reg w <reg> <val> > tcon ; write tcon reg\n"
	"    echo reg d <reg> <cnt> > tcon ; dump tcon regs\n"
	"\n"
	"    echo table > tcon ; print tcon reg table\n"
	"    echo table r <index> > tcon ; read tcon reg table by specified index\n"
	"    echo table w <index> <value> > tcon ; write tcon reg table by specified index\n"
	"    echo table d <index> <len> > tcon ; dump tcon reg table\n"
	"data format:\n"
	"    <index>    : hex number\n"
	"    <value>    : hex number\n"
	"    <len>      : dec number\n"
	"\n"
	"    echo table update > tcon ; update tcon reg table into tcon system regs\n"
	"\n"
	"    echo od <en> > tcon ; tcon over driver control\n"
	"data format:\n"
	"    <en>       : 0=disable, 1=enable\n"
	"\n"
	"    echo save <str> <path> > tcon ; tcon mem save to file\n"
	"data format:\n"
	"    <str>       : table, reg, vac, demura, acc\n"
	"    <path>      : save file path\n"
	"\n"
	"    echo gamma <bit_width> <gamma_r> <gamma_g> <gamma_b> > tcon ; tcon gamma pattern\n"
	"data format:\n"
	"    <bit_width>    : 12, 10, 8\n"
	"    <gamma_r/g/b>  : gamma value in hex\n"
};

ssize_t lcd_tcon_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", lcd_debug_tcon_usage_str);
}

static int lcd_tcon_buf_save(char *path, unsigned char *save_buf, unsigned int size)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	mm_segment_t old_fs = get_fs();

	if (!save_buf) {
		LCDERR("%s: save_buf is null\n", __func__);
		return -1;
	}
	if (size == 0) {
		LCDERR("%s: size is zero\n", __func__);
		return -1;
	}

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	if (IS_ERR(filp)) {
		LCDERR("%s: create %s error\n", __func__, path);
		set_fs(old_fs);
		return -1;
	}

	pos = 0;
	buf = (void *)save_buf;
	vfs_write(filp, buf, size, &pos);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);

	return 0;
}

static void lcd_tcon_reg_table_save(char *path, unsigned char *reg_table, unsigned int size)
{
	int ret;

	ret = lcd_tcon_buf_save(path, reg_table, size);

	LCDPR("save tcon reg table to %s finished\n", path);
}

static void lcd_tcon_reg_save(struct aml_lcd_drv_s *pdrv, char *path, unsigned int size)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	unsigned char *temp;
	void *buf = NULL;
	mm_segment_t old_fs = get_fs();
	int ret;

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	if (IS_ERR(filp)) {
		LCDERR("%s: create %s error\n", __func__, path);
		set_fs(old_fs);
		return;
	}

	temp = kcalloc(size, sizeof(unsigned char), GFP_KERNEL);
	if (!temp) {
		LCDERR("%s: Not enough memory\n", __func__);
		filp_close(filp, NULL);
		set_fs(old_fs);
		return;
	}
	ret = lcd_tcon_core_reg_get(pdrv, temp, size);
	if (ret) {
		LCDPR("save tcon reg failed\n");
		filp_close(filp, NULL);
		set_fs(old_fs);
		kfree(temp);
		return;
	}

	pos = 0;
	buf = (void *)temp;
	vfs_write(filp, buf, size, &pos);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
	kfree(temp);

	LCDPR("save tcon reg to %s success\n", path);
}

static void lcd_tcon_axi_rmem_save(unsigned int index, char *path)
{
	unsigned int mem_size;
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	unsigned long paddr;
	unsigned char *vaddr = NULL;

	if (!tcon_rmem || !tcon_rmem->axi_rmem) {
		pr_info("axi_rmem is NULL\n");
		return;
	}
	if (!tcon_conf)
		return;
	if (index > tcon_conf->axi_bank) {
		pr_info("axi_rmem index %d invalid\n", index);
		return;
	}

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);
	if (IS_ERR(filp)) {
		pr_info("%s: create %s error\n", __func__, path);
		set_fs(old_fs);
		return;
	}
	pos = 0;

	paddr = tcon_rmem->axi_rmem[index].mem_paddr;
	mem_size = tcon_rmem->axi_rmem[index].mem_size;
	vaddr = lcd_tcon_paddrtovaddr(paddr, mem_size);
	if (!vaddr)
		goto lcd_tcon_axi_rmem_save_end;

	vfs_write(filp, vaddr, mem_size, &pos);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
	pr_info("save tcon vac to %s finished\n", path);
	return;

lcd_tcon_axi_rmem_save_end:
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
	pr_info("tcon axi_rmem[%d] mapping failed: 0x%lx\n", index, paddr);
}

static void lcd_tcon_rmem_save(char *path, unsigned int flag)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct tcon_rmem_s *rmem = get_lcd_tcon_rmem();
	struct tcon_mem_map_table_s *table = get_lcd_tcon_mm_table();
	char *str = NULL;
	int ret;

	if (!tcon_conf) {
		LCDPR("%s: tcon_conf is null\n", __func__);
		return;
	}
	if (!rmem) {
		LCDPR("%s: tcon_rmem is null\n", __func__);
		return;
	}
	if (!table) {
		LCDPR("%s: tcon_mm_table is null\n", __func__);
		return;
	}

	str = kcalloc(512, sizeof(char), GFP_KERNEL);
	if (!str)
		return;

	switch (flag) {
	case 0: /* bin path */
		if (rmem->bin_path_rmem.mem_vaddr) {
			sprintf(str, "%s.bin", path);
			ret = lcd_tcon_buf_save(str, rmem->bin_path_rmem.mem_vaddr,
						rmem->bin_path_rmem.mem_size);
			if (ret == 0)
				LCDPR("save tcon bin_path to %s finished\n", str);
		} else {
			pr_info("bin_path invalid\n");
		}
		break;
	case 1: /* vac */
		if (table->valid_flag & LCD_TCON_DATA_VALID_VAC) {
			sprintf(str, "%s.bin", path);
			ret = lcd_tcon_buf_save(str, rmem->vac_rmem.mem_vaddr,
						rmem->vac_rmem.mem_size);
			if (ret == 0)
				LCDPR("save tcon vac to %s finished\n", str);
		} else {
			pr_info("vac invalid\n");
		}
		break;
	case 2: /* demura */
		if (table->valid_flag & LCD_TCON_DATA_VALID_DEMURA) {
			sprintf(str, "%s_set.bin", path);
			ret = lcd_tcon_buf_save(str, rmem->demura_set_rmem.mem_vaddr,
						rmem->demura_set_rmem.mem_size);
			if (ret == 0)
				LCDPR("save tcon demura_set to %s finished\n", str);
			sprintf(str, "%s_lut.bin", path);
			ret = lcd_tcon_buf_save(str, rmem->demura_lut_rmem.mem_vaddr,
						rmem->demura_lut_rmem.mem_size);
			if (ret == 0)
				LCDPR("save tcon demura_lut to %s finished\n", str);
		} else {
			pr_info("demura invalid\n");
		}
		break;
	case 3: /* acc */
		if (table->valid_flag & LCD_TCON_DATA_VALID_ACC) {
			sprintf(str, "%s.bin", path);
			ret = lcd_tcon_buf_save(str, rmem->acc_lut_rmem.mem_vaddr,
						rmem->acc_lut_rmem.mem_size);
			if (ret == 0)
				LCDPR("save tcon acc_lut to %s finished\n", str);
		} else {
			pr_info("acc invalid\n");
		}
		break;
	default:
		break;
	}

	kfree(str);
}

static void lcd_tcon_data_save(unsigned int index, char *path)
{
	struct tcon_mem_map_table_s *table = get_lcd_tcon_mm_table();
	struct lcd_tcon_data_block_header_s *block_header;
	char *str = NULL;
	int ret, i;

	if (!table) {
		LCDPR("%s: tcon_mm_table is null\n", __func__);
		return;
	}
	if (table->version == 0) {
		LCDPR("%s: tcon_data invalid\n", __func__);
		return;
	}

	str = kcalloc(512, sizeof(char), GFP_KERNEL);
	if (!str)
		return;

	if (index == 0xff) {
		LCDPR("data_mem_block_cnt:   %d\n", table->block_cnt);

		for (i = 0; i < table->block_cnt; i++) {
			if ((table->block_bit_flag & (1 << i)) == 0) {
				LCDERR("%s: index %d is not ready\n", __func__, i);
				continue;
			}
			if (!table->data_mem_vaddr[i]) {
				LCDERR("%s: data_mem_vaddr[%d] is null\n",
					__func__, i);
				continue;
			}

			sprintf(str, "%s_%d.bin", path, i);
			block_header = (struct lcd_tcon_data_block_header_s *)
				table->data_mem_vaddr[i];
			ret = lcd_tcon_buf_save(str, table->data_mem_vaddr[i],
						block_header->block_size);
			if (ret == 0)
				LCDPR("save tcon_data to %s finish\n", str);
		}
	} else {
		if (index >= table->block_cnt) {
			LCDERR("%s: index %d invalid\n", __func__, index);
			goto lcd_tcon_data_save_exit;
		}
		if ((table->block_bit_flag & (1 << index)) == 0) {
			LCDERR("%s: index %d is not ready\n", __func__, index);
			goto lcd_tcon_data_save_exit;
		}
		if (!table->data_mem_vaddr[index]) {
			LCDERR("%s: data_mem_vaddr[%d] is null\n",
				__func__, index);
			goto lcd_tcon_data_save_exit;
		}

		sprintf(str, "%s_%d.bin", path, index);
		block_header =
			(struct lcd_tcon_data_block_header_s *)table->data_mem_vaddr[index];
		ret = lcd_tcon_buf_save(str, table->data_mem_vaddr[index],
					block_header->block_size);
		if (ret == 0)
			LCDPR("save tcon_data to %s finish\n", str);
	}

lcd_tcon_data_save_exit:
	kfree(str);
}

static void lcd_tcon_reg_table_load(char *path, unsigned char *reg_table, unsigned int table_size)
{
	unsigned int size = 0;
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("read %s error or filp is NULL.\n", path);
		return;
	}

	size = vfs_read(filp, reg_table, table_size, &pos);
	if (size < table_size) {
		pr_info("%s read size %u < %u error.\n",
			__func__, size, table_size);
		return;
	}
	vfs_fsync(filp, 0);

	filp_close(filp, NULL);
	set_fs(old_fs);

	pr_info("load bin file path: %s finish\n", path);
}

static void lcd_tcon_reg_setting_load(struct aml_lcd_drv_s *pdrv, char *path)
{
	unsigned int size = 0, table_size = 0, len = 0;
	char *reg_table;
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	/*struct kstat stat;*/
	unsigned int i, n;
	char *ps, *token;
	char str[4] = {',', ' ', '\n', '\0'};
	unsigned int temp[2];

	set_fs(KERNEL_DS);

	/*vfs_stat(path, &stat);
	 *table_size = stat.size;
	 */

	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("read %s error or filp is NULL.\n", path);
		return;
	}
	table_size = filp->f_inode->i_size;
	reg_table = kzalloc((table_size + 2), GFP_KERNEL);
	if (!reg_table) {
		filp_close(filp, NULL);
		set_fs(old_fs);
		return;
	}

	size = vfs_read(filp, reg_table, table_size, &pos);
	if (size < table_size) {
		pr_info("%s read size %u < %u error.\n",
			__func__, size, table_size);
		filp_close(filp, NULL);
		set_fs(old_fs);
		kfree(reg_table);
		return;
	}
	vfs_fsync(filp, 0);

	filp_close(filp, NULL);
	set_fs(old_fs);

	ps = reg_table;
	len = 0;
	i = 0;
	n = 0;
	while (1) {
		if (len >= table_size)
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
		if (kstrtouint(token, 16, &temp[i % 2]) < 0) {
			kfree(reg_table);
			return;
		}
		if ((i % 2) == 1) {
			if (lcd_debug_print_flag & LCD_DBG_PR_TEST)
				pr_info("write tcon reg 0x%04x = 0x%08x\n", temp[0], temp[1]);
			lcd_tcon_reg_write(pdrv, temp[0], temp[1]);
			n++;
		}
		len += (strlen(token) + 1);
		i++;
	}

	pr_info("load setting file path: %s finish, total line %d\n", path, n);
	kfree(reg_table);
}

static void lcd_tcon_axi_rmem_load(struct aml_lcd_drv_s *pdrv, unsigned int index, char *path)
{
	unsigned int size = 0, mem_size;
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();
	unsigned char *buf;
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();

	if (!tcon_rmem || !tcon_rmem->axi_rmem) {
		pr_info("axi_rmem is NULL\n");
		return;
	}
	if (!tcon_conf)
		return;
	if (index >= tcon_conf->axi_bank) {
		pr_info("axi_rmem index %d invalid\n", index);
		return;
	}

	mem_size = tcon_rmem->axi_rmem[index].mem_size;
	buf = kcalloc(mem_size, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("read %s error or filp is NULL.\n", path);
		kfree(buf);
		return;
	}

	size = vfs_read(filp, buf, mem_size, &pos);
	pr_info("%s read size %u\n", __func__, size);
	vfs_fsync(filp, 0);

	filp_close(filp, NULL);
	set_fs(old_fs);

	lcd_tcon_axi_rmem_lut_load(pdrv, 1, buf, size);
	kfree(buf);

	pr_info("load bin file path: %s finish\n", path);
}

static int lcd_tcon_reg_table_check(unsigned char *table, unsigned int size)
{
	if (size == 0)
		return -1;
	if (!table)
		return -1;
	return 0;
}

ssize_t lcd_tcon_debug_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
#define __MAX_PARAM 520
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	char *buf_orig;
	char **parm = NULL;
	unsigned int temp = 0, val, back_val, i, n, size = 0;
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	unsigned char data;
	unsigned char *table = NULL;
	int ret = -1;

	if (mm_table) {
		size = mm_table->core_reg_table_size;
		table = mm_table->core_reg_table;
	}

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;

	parm = kcalloc(__MAX_PARAM, sizeof(char *), GFP_KERNEL);
	if (!parm) {
		kfree(buf_orig);
		return count;
	}

	lcd_debug_parse_param(buf_orig, parm, __MAX_PARAM);

	if (strcmp(parm[0], "reg") == 0) {
		if (!parm[1]) {
			lcd_tcon_reg_readback_print(pdrv);
			goto lcd_tcon_debug_store_end;
		}
		if (strcmp(parm[1], "dump") == 0) {
			lcd_tcon_reg_readback_print(pdrv);
			goto lcd_tcon_debug_store_end;
		} else if (strcmp(parm[1], "rb") == 0) {
			if (!parm[2])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			pr_info("read tcon byte [0x%04x] = 0x%02x\n",
				temp, lcd_tcon_read_byte(pdrv, temp));
		} else if (strcmp(parm[1], "wb") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 16, &val);
			if (ret)
				goto lcd_tcon_debug_store_err;
			data = (unsigned char)val;
			lcd_tcon_write_byte(pdrv, temp, data);
			pr_info("write tcon byte [0x%04x] = 0x%02x\n",
				temp, data);
		} else if (strcmp(parm[1], "wlb") == 0) { /*long write byte*/
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 16, &size);
			if (ret)
				goto lcd_tcon_debug_store_err;

			if (!parm[3 + size]) {
				pr_info("size and data is not match\n");
				goto lcd_tcon_debug_store_err;
			}

			for (i = 0; i < size; i++) {
				ret = kstrtouint(parm[4 + i], 16, &val);
				if (ret)
					goto lcd_tcon_debug_store_err;
				data = (unsigned char)val;
				lcd_tcon_write_byte(pdrv, (temp + i), data);
				pr_info("write tcon byte [0x%04x] = 0x%02x\n",
					(temp + i), data);
			}
		} else if (strcmp(parm[1], "db") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 10, &size);
			if (ret)
				goto lcd_tcon_debug_store_err;
			pr_info("dump tcon byte:\n");
			for (i = 0; i < size; i++) {
				pr_info("  [0x%04x] = 0x%02x\n",
					(temp + i),
					lcd_tcon_read_byte(pdrv, temp + i));
			}
		} else if (strcmp(parm[1], "r") == 0) {
			if (!parm[2])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			pr_info("read tcon [0x%04x] = 0x%08x\n",
				temp, lcd_tcon_read(pdrv, temp));
		} else if (strcmp(parm[1], "w") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 16, &val);
			if (ret)
				goto lcd_tcon_debug_store_err;
			lcd_tcon_write(pdrv, temp, val);
			pr_info("write tcon [0x%04x] = 0x%08x\n",
				temp, val);
		} else if (strcmp(parm[1], "wl") == 0) { /*long write*/
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 16, &size);
			if (ret)
				goto lcd_tcon_debug_store_err;

			if (!parm[3 + size]) {
				pr_info("size and data is not match\n");
				goto lcd_tcon_debug_store_err;
			}

			for (i = 0; i < size; i++) {
				ret = kstrtouint(parm[4 + i], 16, &val);
				if (ret)
					goto lcd_tcon_debug_store_err;
				lcd_tcon_write(pdrv, temp + i, val);
				pr_info("write tcon [0x%04x] = 0x%08x\n",
					(temp + i), val);
			}
		} else if (strcmp(parm[1], "d") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 10, &size);
			if (ret)
				goto lcd_tcon_debug_store_err;
			pr_info("dump tcon:\n");
			for (i = 0; i < size; i++) {
				pr_info("  [0x%04x] = 0x%08x\n",
					(temp + i),
					lcd_tcon_read(pdrv, temp + i));
			}
		}
	} else if (strcmp(parm[0], "table") == 0) {
		if (lcd_tcon_reg_table_check(table, size))
			goto lcd_tcon_debug_store_end;
		if (!parm[1]) {
			lcd_tcon_reg_table_print();
			goto lcd_tcon_debug_store_end;
		}
		if (strcmp(parm[1], "dump") == 0) {
			lcd_tcon_reg_table_print();
			goto lcd_tcon_debug_store_end;
		} else if (strcmp(parm[1], "r") == 0) {
			if (!parm[2])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			val = lcd_tcon_table_read(temp);
			pr_info("read table 0x%x = 0x%x\n", temp, val);
		} else if (strcmp(parm[1], "w") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 16, &val);
			if (ret)
				goto lcd_tcon_debug_store_err;
			back_val = lcd_tcon_table_write(temp, val);
			pr_info("write table 0x%x = 0x%x, readback 0x%x\n",
				temp, val, back_val);
		} else if (strcmp(parm[1], "d") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 16, &n);
			if (ret)
				goto lcd_tcon_debug_store_err;
			pr_info("dump tcon table:\n");
			for (i = temp; i < (temp + n); i++) {
				if (i > size)
					break;
				data = table[i];
				pr_info("  [0x%04x]=0x%02x\n", i, data);
			}
		} else if (strcmp(parm[1], "update") == 0) {
			lcd_tcon_core_update(pdrv);
		} else if (strcmp(parm[1], "load") == 0) {
			if (!parm[2]) {
				pr_info("invalid load path\n");
				goto lcd_tcon_debug_store_err;
			}
			lcd_tcon_reg_table_load(parm[2], table, size);
		} else {
			goto lcd_tcon_debug_store_err;
		}
	} else if (strcmp(parm[0], "od") == 0) { /* over drive */
		if (!parm[1]) {
			temp = lcd_tcon_od_get(pdrv);
			if (temp)
				LCDPR("tcon od is enabled: %d\n", temp);
			else
				LCDPR("tcon od is disabled: %d\n", temp);
		} else {
			ret = kstrtouint(parm[1], 10, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			if (temp)
				lcd_tcon_od_set(pdrv, 1);
			else
				lcd_tcon_od_set(pdrv, 0);
		}
	} else if (strcmp(parm[0], "multi_lut") == 0) {
		lcd_tcon_multi_lut_print();
	} else if (strcmp(parm[0], "multi_update") == 0) {
		if (!parm[1])
			goto lcd_tcon_debug_store_err;
		ret = kstrtouint(parm[1], 10, &temp);
		if (ret)
			goto lcd_tcon_debug_store_err;
		mm_table->multi_lut_update = temp;
		LCDPR("tcon multi_update: %d\n", temp);
	} else if (strcmp(parm[0], "multi_bypass") == 0) {
		if (!parm[2])
			goto lcd_tcon_debug_store_err;
		ret = kstrtouint(parm[2], 16, &temp);
		if (ret)
			goto lcd_tcon_debug_store_err;
		if (strcmp(parm[1], "set") == 0)
			lcd_tcon_data_multi_bypass_set(mm_table, temp, 1);
		else //clr
			lcd_tcon_data_multi_bypass_set(mm_table, temp, 0);
	} else if (strcmp(parm[0], "save") == 0) { /* save buf to bin */
		if (!parm[2])
			goto lcd_tcon_debug_store_err;

		if (strcmp(parm[1], "table") == 0) {
			if (lcd_tcon_reg_table_check(table, size))
				goto lcd_tcon_debug_store_end;
			lcd_tcon_reg_table_save(parm[2], table, size);
		} else if (strcmp(parm[1], "reg") == 0) {
			lcd_tcon_reg_save(pdrv, parm[2], size);
		} else if (strcmp(parm[1], "axi") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 10, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			/* parm[2]: axi index */
			/* parm[3]: save path */
			lcd_tcon_axi_rmem_save(temp, parm[3]);
		} else if (strcmp(parm[1], "path") == 0) {
			lcd_tcon_rmem_save(parm[2], 0);
		} else if (strcmp(parm[1], "vac") == 0) {
			lcd_tcon_rmem_save(parm[2], 1);
		} else if (strcmp(parm[1], "demura") == 0) {
			lcd_tcon_rmem_save(parm[2], 2);
		} else if (strcmp(parm[1], "acc") == 0) {
			lcd_tcon_rmem_save(parm[2], 3);
		} else if (strcmp(parm[1], "data") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 10, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			/* parm[2]: tcon data index */
			/* parm[3]: save path */
			lcd_tcon_data_save(temp, parm[3]);
		} else {
			goto lcd_tcon_debug_store_err;
		}
	} else if (strcmp(parm[0], "tee") == 0) {
		if (!parm[1])
			goto lcd_tcon_debug_store_err;
#ifdef CONFIG_AMLOGIC_TEE
		if (strcmp(parm[1], "status") == 0) {
			lcd_tcon_mem_tee_get_status();
		} else if (strcmp(parm[1], "off") == 0) {
			if (!parm[2])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 10, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = lcd_tcon_mem_tee_protect(temp, 0);
			pr_info("%s: tcon tee unprotect ret %d\n", __func__, ret);
		} else if (strcmp(parm[1], "on") == 0) {
			if (!parm[2])
				goto lcd_tcon_debug_store_err;
			ret = lcd_tcon_mem_tee_protect(temp, 1);
			pr_info("%s: tcon tee protect ret %d\n", __func__, ret);
		} else {
			goto lcd_tcon_debug_store_err;
		}
#endif
	} else if (strcmp(parm[0], "load") == 0) {
		if (!parm[2])
			goto lcd_tcon_debug_store_err;

		if (strcmp(parm[1], "axi") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 10, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			lcd_tcon_axi_rmem_load(pdrv, temp, parm[3]);
		} else if (strcmp(parm[1], "table") == 0) {
			lcd_tcon_reg_table_load(parm[2], table, size);
		} else if (strcmp(parm[1], "setting") == 0) {
			lcd_tcon_reg_setting_load(pdrv, parm[2]);
		} else {
			goto lcd_tcon_debug_store_err;
		}
	} else if (strcmp(parm[0], "isr") == 0) {
		if (parm[1]) {
			if (strcmp(parm[1], "dbg") == 0) {
				if (parm[2]) {
					ret = kstrtouint(parm[2], 16, &temp);
					if (ret)
						goto lcd_tcon_debug_store_err;
					if (temp)
						lcd_debug_print_flag |= LCD_DBG_PR_TEST;
					else
						lcd_debug_print_flag &= ~LCD_DBG_PR_TEST;
					pr_err("tcon isr dbg_log_en: %d\n", temp);
					goto lcd_tcon_debug_store_end;
				}
			} else if (strcmp(parm[1], "clr") == 0) {
				pdrv->tcon_isr_bypass = 1;
				msleep(100);
				lcd_tcon_dbg_trace_clear();
				pdrv->tcon_isr_bypass = 0;
				goto lcd_tcon_debug_store_end;
			} else if (strcmp(parm[1], "log") == 0) {
				if (parm[2]) {
					ret = kstrtouint(parm[2], 16, &temp);
					if (ret)
						goto lcd_tcon_debug_store_err;
				} else {
					temp = 0x1;
				}
				pdrv->tcon_isr_bypass = 1;
				msleep(100);
				lcd_tcon_dbg_trace_print(temp);
				pdrv->tcon_isr_bypass = 0;
				goto lcd_tcon_debug_store_end;
			}
		}
		pr_err("tcon dbg_log_en: %d\n",
			(lcd_debug_print_flag & LCD_DBG_PR_TEST) ? 1 : 0);
		if (lcd_debug_print_flag & LCD_DBG_PR_TEST)
			lcd_tcon_dbg_trace_print(0);
	} else {
		goto lcd_tcon_debug_store_err;
	}

lcd_tcon_debug_store_end:
	kfree(parm);
	kfree(buf_orig);
	return count;

lcd_tcon_debug_store_err:
	pr_info("invalid parameters\n");
	kfree(parm);
	kfree(buf_orig);
	return count;
#undef __MAX_PARAM
}

ssize_t lcd_tcon_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return sprintf(buf, "0x%x\n", pdrv->tcon_status);
}

ssize_t lcd_tcon_reg_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	int len = 0;
	unsigned int i, addr;

	mutex_lock(&lcd_tcon_dbg_mutex);

	len += sprintf(buf + len, "for_tool:");
	if ((pdrv->status & LCD_STATUS_IF_ON) == 0) {
		len += sprintf(buf + len, "ERROR\n");
		mutex_unlock(&lcd_tcon_dbg_mutex);
		return len;
	}
	switch (adb_reg.rw_mode) {
	case LCD_ADB_TCON_REG_RW_MODE_NULL:
		len += sprintf(buf + len, "NULL");
		break;
	case LCD_ADB_TCON_REG_RW_MODE_RN:
		if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
			for (i = 0; i < adb_reg.len; i++) {
				addr = adb_reg.addr + i;
				len += sprintf(buf + len, "%04x=%08x ",
					       addr, lcd_tcon_read(pdrv, addr));
			}
		} else {
			for (i = 0; i < adb_reg.len; i++) {
				addr = adb_reg.addr + i;
				len += sprintf(buf + len, "%04x=%02x ",
					       addr, lcd_tcon_read_byte(pdrv, addr));
			}
		}
		break;
	case LCD_ADB_TCON_REG_RW_MODE_WM:
		if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
			addr = adb_reg.addr;
			len += sprintf(buf + len, "%04x=%08x ",
				       addr, lcd_tcon_read(pdrv, addr));
		} else {
			addr = adb_reg.addr;
			len += sprintf(buf + len, "%04x=%02x ",
				       addr, lcd_tcon_read_byte(pdrv, addr));
		}
		break;
	case LCD_ADB_TCON_REG_RW_MODE_WN:
		if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
			for (i = 0; i < adb_reg.len; i++) {
				addr = adb_reg.addr + i;
				len += sprintf(buf + len, "%04x=%08x ",
					       addr, lcd_tcon_read(pdrv, addr));
			}
		} else {
			for (i = 0; i < adb_reg.len; i++) {
				addr = adb_reg.addr + i;
				len += sprintf(buf + len, "%04x=%02x ",
					       addr, lcd_tcon_read_byte(pdrv, addr));
			}
		}
		break;
	case LCD_ADB_TCON_REG_RW_MODE_WS:
		if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
			addr = adb_reg.addr;
			for (i = 0; i < adb_reg.len; i++) {
				len += sprintf(buf + len, "%04x=%08x ",
					       addr, lcd_tcon_read(pdrv, addr));
			}
		} else {
			addr = adb_reg.addr;
			for (i = 0; i < adb_reg.len; i++) {
				len += sprintf(buf + len, "%04x=%02x ",
					       addr, lcd_tcon_read_byte(pdrv, addr));
			}
		}
		break;
	case LCD_ADB_TCON_REG_RW_MODE_ERR:
		len += sprintf(buf + len, "ERROR");
		break;
	default:
		len += sprintf(buf + len, "ERROR");
		adb_reg.rw_mode = LCD_ADB_TCON_REG_RW_MODE_NULL;
		adb_reg.addr = 0;
		adb_reg.len = 0;
		break;
	}
	len += sprintf(buf + len, "\n");
	mutex_unlock(&lcd_tcon_dbg_mutex);
	return len;
}

ssize_t lcd_tcon_reg_debug_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
#define __MAX_PARAM 1500
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	char *buf_orig;
	char **parm = NULL;
	unsigned int  temp32 = 0, temp_reg = 0;
	unsigned int  temp_len = 0, temp_mask = 0, temp_val = 0;
	unsigned char temp8 = 0;
	int ret = -1, i;

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return count;

	if (!buf)
		return count;

	mutex_lock(&lcd_tcon_dbg_mutex);

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		mutex_unlock(&lcd_tcon_dbg_mutex);
		return count;
	}

	parm = kcalloc(__MAX_PARAM, sizeof(char *), GFP_KERNEL);
	if (!parm) {
		kfree(buf_orig);
		mutex_unlock(&lcd_tcon_dbg_mutex);
		return count;
	}

	lcd_debug_parse_param(buf_orig, parm, __MAX_PARAM);

	if (strcmp(parm[0], "wn") == 0) {
		if (!parm[3])
			goto lcd_tcon_adb_debug_store_err;
		if (strcmp(parm[1], "8") == 0)
			adb_reg.bit_width = ADB_TCON_REG_8_bit;
		else if (strcmp(parm[1], "32") == 0)
			adb_reg.bit_width = ADB_TCON_REG_32_bit;
		else
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[2], 16, &temp_reg);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[3], 10, &temp_len);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		if (temp_len <= 0)
			goto lcd_tcon_adb_debug_store_err;
		if (!parm[4 + temp_len - 1])
			goto lcd_tcon_adb_debug_store_err;
		if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
			/*(4k - 9)/(8+1) ~=454*/
			if (temp_len > 454)
				goto lcd_tcon_adb_debug_store_err;
		} else {
			/*(4k - 9)/(2+1) ~=1362*/
			if (temp_len > 1362)
				goto lcd_tcon_adb_debug_store_err;
		}
		adb_reg.len = temp_len; /* for cat use */
		adb_reg.addr = temp_reg;
		adb_reg.rw_mode = LCD_ADB_TCON_REG_RW_MODE_WN;
		for (i = 0; i < temp_len; i++) {
			ret = kstrtouint(parm[i + 4], 16, &temp_val);
			if (ret)
				goto lcd_tcon_adb_debug_store_err;
			if (adb_reg.bit_width == ADB_TCON_REG_32_bit)
				lcd_tcon_write(pdrv, temp_reg, temp_val);
			else
				lcd_tcon_write_byte(pdrv, temp_reg, temp_val);
			temp_reg++;
		}
	} else if (strcmp(parm[0], "wm") == 0) {
		if (!parm[4])
			goto lcd_tcon_adb_debug_store_err;
		if (strcmp(parm[1], "8") == 0)
			adb_reg.bit_width = ADB_TCON_REG_8_bit;
		else if (strcmp(parm[1], "32") == 0)
			adb_reg.bit_width = ADB_TCON_REG_32_bit;
		else
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[2], 16, &temp_reg);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[3], 16, &temp_mask);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[4], 16, &temp_val);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		adb_reg.len = 1; /* for cat use */
		adb_reg.addr = temp_reg;
		adb_reg.rw_mode = LCD_ADB_TCON_REG_RW_MODE_WM;
		if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
			temp32 = lcd_tcon_read(pdrv, temp_reg);
			temp32 &= ~temp_mask;
			temp32 |= temp_val & temp_mask;
			lcd_tcon_write(pdrv, temp_reg, temp32);
		} else {
			temp8 = lcd_tcon_read_byte(pdrv, temp_reg);
			temp8 &= ~temp_mask;
			temp8 |= temp_val & temp_mask;
			lcd_tcon_write_byte(pdrv, temp_reg, temp8);
		}
	} else if (strcmp(parm[0], "ws") == 0) {
		if (!parm[3])
			goto lcd_tcon_adb_debug_store_err;
		if (strcmp(parm[1], "8") == 0)
			adb_reg.bit_width = ADB_TCON_REG_8_bit;
		else if (strcmp(parm[1], "32") == 0)
			adb_reg.bit_width = ADB_TCON_REG_32_bit;
		else
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[2], 16, &temp_reg);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[3], 10, &temp_len);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		if (temp_len <= 0)
			goto lcd_tcon_adb_debug_store_err;
		if (!parm[4 + temp_len - 1])
			goto lcd_tcon_adb_debug_store_err;
		if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
			/*(4k - 9)/(8+1) ~=454*/
			if (temp_len > 454)
				goto lcd_tcon_adb_debug_store_err;
		} else {
			/*(4k - 9)/(2+1) ~=1362*/
			if (temp_len > 1362)
				goto lcd_tcon_adb_debug_store_err;
		}
		adb_reg.len = temp_len; /* for cat use */
		adb_reg.addr = temp_reg;
		adb_reg.rw_mode = LCD_ADB_TCON_REG_RW_MODE_WS;
		for (i = 0; i < temp_len; i++) {
			ret = kstrtouint(parm[i + 4], 16, &temp_val);
			if (ret)
				goto lcd_tcon_adb_debug_store_err;
			if (adb_reg.bit_width == ADB_TCON_REG_32_bit)
				lcd_tcon_write(pdrv, temp_reg, temp_val);
			else
				lcd_tcon_write_byte(pdrv, temp_reg, temp_val);
		}
	} else if (strcmp(parm[0], "rn") == 0) {
		if (!parm[2])
			goto lcd_tcon_adb_debug_store_err;
		if (strcmp(parm[1], "8") == 0)
			adb_reg.bit_width = ADB_TCON_REG_8_bit;
		else if (strcmp(parm[1], "32") == 0)
			adb_reg.bit_width = ADB_TCON_REG_32_bit;
		else
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[2], 16, &temp_reg);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		if (parm[3]) {
			ret = kstrtouint(parm[3], 10, &temp_len);
			if (ret)
				goto lcd_tcon_adb_debug_store_err;
			if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
				/*(4k - 9)/(8+1) ~=454*/
				if (temp_len > 454)
					goto lcd_tcon_adb_debug_store_err;
			} else {
				/*(4k - 9)/(2+1) ~=1362*/
				if (temp_len > 1362)
					goto lcd_tcon_adb_debug_store_err;
			}
			adb_reg.len = temp_len; /* for cat use */
			adb_reg.addr = temp_reg;
			adb_reg.rw_mode = LCD_ADB_TCON_REG_RW_MODE_RN;
		} else {
			adb_reg.len = 1; /* for cat use */
			adb_reg.addr = temp_reg;
			adb_reg.rw_mode = LCD_ADB_TCON_REG_RW_MODE_RN;
		}
	} else {
		goto lcd_tcon_adb_debug_store_err;
	}

	kfree(parm);
	kfree(buf_orig);
	mutex_unlock(&lcd_tcon_dbg_mutex);
	return count;

lcd_tcon_adb_debug_store_err:
	adb_reg.rw_mode = LCD_ADB_TCON_REG_RW_MODE_ERR;

	kfree(parm);
	kfree(buf_orig);
	mutex_unlock(&lcd_tcon_dbg_mutex);
	return count;
#undef __MAX_PARAM
}

ssize_t lcd_tcon_fw_dbg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct lcd_tcon_fw_s *tcon_fw = aml_lcd_tcon_get_fw();
	ssize_t ret = 0;

	if (!tcon_fw)
		return ret;

	if (tcon_fw->debug_show)
		ret = tcon_fw->debug_show(tcon_fw, buf);
	else
		LCDERR("%s: tcon_fw is not installed\n", __func__);

	return ret;
}

ssize_t lcd_tcon_fw_dbg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct lcd_tcon_fw_s *tcon_fw = aml_lcd_tcon_get_fw();

	if (!buf)
		return count;
	if (!tcon_fw)
		return count;

	mutex_lock(&lcd_tcon_dbg_mutex);

	if (tcon_fw->debug_store)
		tcon_fw->debug_store(tcon_fw, buf, count);
	else
		LCDERR("%s: tcon_fw is not installed\n", __func__);

	mutex_unlock(&lcd_tcon_dbg_mutex);
	return count;
}

static struct device_attribute lcd_tcon_debug_attrs[] = {
	__ATTR(debug,     0644, lcd_tcon_debug_show, lcd_tcon_debug_store),
	__ATTR(status,    0444, lcd_tcon_status_show, NULL),
	__ATTR(reg,       0644, lcd_tcon_reg_debug_show, lcd_tcon_reg_debug_store),
	__ATTR(tcon_fw,   0644, lcd_tcon_fw_dbg_show, lcd_tcon_fw_dbg_store)
};

/* **********************************
 * tcon IOCTL
 * **********************************
 */
static unsigned int lcd_tcon_bin_path_index;

long lcd_tcon_ioctl_handler(struct aml_lcd_drv_s *pdrv, int mcd_nr, unsigned long arg)
{
	void __user *argp;
	struct aml_lcd_tcon_bin_s lcd_tcon_buff;
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	struct lcd_tcon_data_block_header_s block_header;
	unsigned int size = 0, temp, m = 0;
	unsigned char *mem_vaddr = NULL;
	char *str = NULL;
	int index = 0;
	int ret = 0;

	if (!pdrv)
		return -EFAULT;

	argp = (void __user *)arg;
	switch (mcd_nr) {
	case LCD_IOC_GET_TCON_BIN_MAX_CNT_INFO:
		if (!mm_table) {
			ret = -EFAULT;
			break;
		}
		if (copy_to_user(argp, &mm_table->block_cnt, sizeof(unsigned int)))
			ret = -EFAULT;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("tcon: get bin max_cnt: %d\n", mm_table->block_cnt);
		break;
	case LCD_IOC_SET_TCON_DATA_INDEX_INFO:
		if (copy_from_user(&lcd_tcon_bin_path_index, argp, sizeof(unsigned int)))
			ret = -EFAULT;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("tcon: set bin index: %d\n", lcd_tcon_bin_path_index);
		break;
	case LCD_IOC_GET_TCON_BIN_PATH_INFO:
		if (!mm_table || !tcon_rmem) {
			ret = -EFAULT;
			break;
		}

		mem_vaddr = tcon_rmem->bin_path_rmem.mem_vaddr;
		if (!mem_vaddr) {
			LCDERR("%s: no tcon bin path rmem\n", __func__);
			ret = -EFAULT;
			break;
		}
		if (lcd_tcon_bin_path_index >= mm_table->block_cnt) {
			ret = -EFAULT;
			break;
		}
		m = 32 + 256 * lcd_tcon_bin_path_index;
		str = (char *)&mem_vaddr[m + 4];
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("tcon: get bin_path[%d]: %s\n", lcd_tcon_bin_path_index, str);

		if (copy_to_user(argp, str, 256))
			ret = -EFAULT;
		break;
	case LCD_IOC_SET_TCON_BIN_DATA_INFO:
		if (!mm_table || !tcon_rmem) {
			ret = -EFAULT;
			break;
		}

		mem_vaddr = tcon_rmem->bin_path_rmem.mem_vaddr;
		if (!mem_vaddr) {
			LCDERR("%s: no tcon bin path rmem\n", __func__);
			ret = -EFAULT;
			break;
		}

		memset(&lcd_tcon_buff, 0, sizeof(struct aml_lcd_tcon_bin_s));
		if (copy_from_user(&lcd_tcon_buff, argp, sizeof(struct aml_lcd_tcon_bin_s))) {
			ret = -EFAULT;
			break;
		}
		if (lcd_tcon_buff.size == 0) {
			LCDERR("%s: invalid data size %d\n", __func__, size);
			ret = -EFAULT;
			break;
		}
		index = lcd_tcon_buff.index;
		if (index >= mm_table->block_cnt) {
			LCDERR("%s: invalid index %d\n", __func__, index);
			ret = -EFAULT;
			break;
		}
		m = 32 + 256 * index;
		str = (char *)&mem_vaddr[m + 4];
		temp = *(unsigned int *)&mem_vaddr[m];

		memset(&block_header, 0, sizeof(struct lcd_tcon_data_block_header_s));
		argp = (void __user *)lcd_tcon_buff.ptr;
		if (copy_from_user(&block_header, argp,
			sizeof(struct lcd_tcon_data_block_header_s))) {
			ret = -EFAULT;
			break;
		}
		size = block_header.block_size;
		if (size > lcd_tcon_buff.size ||
		    size < sizeof(struct lcd_tcon_data_block_header_s)) {
			LCDERR("%s: block[%d] size 0x%x error\n", __func__, index, size);
			ret = -EFAULT;
			break;
		}

		kfree(mm_table->data_mem_vaddr[index]);
		mm_table->data_mem_vaddr[index] =
			kcalloc(size, sizeof(unsigned char), GFP_KERNEL);
		if (!mm_table->data_mem_vaddr[index]) {
			ret = -EFAULT;
			break;
		}
		if (copy_from_user(mm_table->data_mem_vaddr[index], argp, size)) {
			kfree(mm_table->data_mem_vaddr[index]);
			mm_table->data_mem_vaddr[index] = NULL;
			ret = -EFAULT;
			break;
		}

		LCDPR("tcon: load bin_path[%d]: %s, size: 0x%x -> 0x%x\n",
		      index, str, temp, size);

		ret = lcd_tcon_data_load(pdrv, mm_table->data_mem_vaddr[index], index);
		if (ret) {
			kfree(mm_table->data_mem_vaddr[index]);
			mm_table->data_mem_vaddr[index] = NULL;
			ret = -EFAULT;
		}
		break;
	default:
		LCDERR("tcon: don't support ioctl cmd_nr: 0x%x\n", mcd_nr);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int lcd_tcon_open(struct inode *inode, struct file *file)
{
	struct lcd_tcon_local_cfg_s *tcon_cfg;

	/* Get the per-device structure that contains this cdev */
	tcon_cfg = container_of(inode->i_cdev, struct lcd_tcon_local_cfg_s, cdev);
	file->private_data = tcon_cfg;
	return 0;
}

static int lcd_tcon_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long lcd_tcon_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct lcd_tcon_local_cfg_s *local_cfg = (struct lcd_tcon_local_cfg_s *)file->private_data;
	struct aml_lcd_drv_s *pdrv;
	void __user *argp;
	int mcd_nr = -1;
	int ret = 0;

	if (!local_cfg)
		return -EFAULT;
	pdrv = dev_get_drvdata(local_cfg->dev);
	if (!pdrv)
		return -EFAULT;

	mcd_nr = _IOC_NR(cmd);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("tcon: %s: cmd_dir = 0x%x, cmd_nr = 0x%x\n",
			__func__, _IOC_DIR(cmd), mcd_nr);
	}

	argp = (void __user *)arg;
	switch (mcd_nr) {
	case LCD_IOC_GET_TCON_BIN_MAX_CNT_INFO:
	case LCD_IOC_SET_TCON_DATA_INDEX_INFO:
	case LCD_IOC_GET_TCON_BIN_PATH_INFO:
	case LCD_IOC_SET_TCON_BIN_DATA_INFO:
		lcd_tcon_ioctl_handler(pdrv, mcd_nr, arg);
		break;
	default:
		LCDERR("tcon: don't support ioctl cmd_nr: 0x%x\n", mcd_nr);
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long lcd_tcon_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = lcd_tcon_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations lcd_tcon_fops = {
	.owner          = THIS_MODULE,
	.open           = lcd_tcon_open,
	.release        = lcd_tcon_release,
	.unlocked_ioctl = lcd_tcon_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = lcd_tcon_compat_ioctl,
#endif
};

/* **********************************
 * tcon debug api
 * **********************************
 */
void lcd_tcon_debug_file_add(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_local_cfg_s *local_cfg)
{
	int i, ret;

	if (!local_cfg)
		return;

	local_cfg->clsp = class_create(THIS_MODULE, AML_TCON_CLASS_NAME);
	if (IS_ERR(local_cfg->clsp)) {
		LCDERR("tcon: failed to create class\n");
		return;
	}

	ret = alloc_chrdev_region(&local_cfg->devno, 0, 1, AML_TCON_DEVICE_NAME);
	if (ret < 0) {
		LCDERR("tcon: failed to alloc major number\n");
		goto err1;
	}

	local_cfg->dev = device_create(local_cfg->clsp, NULL,
		local_cfg->devno, (void *)pdrv, AML_TCON_DEVICE_NAME);
	if (IS_ERR(local_cfg->dev)) {
		LCDERR("tcon: failed to create device\n");
		goto err2;
	}

	for (i = 0; i < ARRAY_SIZE(lcd_tcon_debug_attrs); i++) {
		if (device_create_file(local_cfg->dev, &lcd_tcon_debug_attrs[i])) {
			LCDERR("tcon: create tcon debug attribute %s fail\n",
			       lcd_tcon_debug_attrs[i].attr.name);
			goto err3;
		}
	}

	/* connect the file operations with cdev */
	cdev_init(&local_cfg->cdev, &lcd_tcon_fops);
	local_cfg->cdev.owner = THIS_MODULE;

	/* connect the major/minor number to the cdev */
	ret = cdev_add(&local_cfg->cdev, local_cfg->devno, 1);
	if (ret) {
		LCDERR("tcon: failed to add device\n");
		goto err4;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("tcon: %s: OK\n", __func__);
	return;

err4:
	for (i = 0; i < ARRAY_SIZE(lcd_tcon_debug_attrs); i++)
		device_remove_file(local_cfg->dev, &lcd_tcon_debug_attrs[i]);
err3:
	device_destroy(local_cfg->clsp, local_cfg->devno);
	local_cfg->dev = NULL;
err2:
	unregister_chrdev_region(local_cfg->devno, 1);
err1:
	class_destroy(local_cfg->clsp);
	local_cfg->clsp = NULL;
	LCDERR("tcon: %s error\n", __func__);
}

void lcd_tcon_debug_file_remove(struct lcd_tcon_local_cfg_s *local_cfg)
{
	int i;

	if (!local_cfg)
		return;

	for (i = 0; i < ARRAY_SIZE(lcd_tcon_debug_attrs); i++)
		device_remove_file(local_cfg->dev, &lcd_tcon_debug_attrs[i]);

	cdev_del(&local_cfg->cdev);
	device_destroy(local_cfg->clsp, local_cfg->devno);
	local_cfg->dev = NULL;
	class_destroy(local_cfg->clsp);
	local_cfg->clsp = NULL;
	unregister_chrdev_region(local_cfg->devno, 1);
}
