// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#ifdef CONFIG_AMLOGIC_BACKLIGHT
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#endif
#include <linux/fs.h>
#include <linux/uaccess.h>
#ifdef CONFIG_AMLOGIC_TEE_TODO
#include <linux/amlogic/tee.h>
#endif
#include "lcd_common.h"
#include "lcd_reg.h"
#include "lcd_tcon.h"
/*#include "tcon_ceds.h"*/

static struct lcd_tcon_config_s *lcd_tcon_conf;
static struct tcon_rmem_s tcon_rmem;
static struct tcon_mem_map_table_s tcon_mm_table;
static struct lcd_tcon_local_cfg_s tcon_local_cfg;

/* **********************************
 * tcon common function
 * **********************************
 */
int lcd_tcon_valid_check(void)
{
	if (!lcd_tcon_conf) {
		LCDERR("invalid tcon data\n");
		return -1;
	}
	if (lcd_tcon_conf->tcon_valid == 0) {
		LCDERR("invalid tcon\n");
		return -1;
	}

	return 0;
}

struct lcd_tcon_config_s *get_lcd_tcon_config(void)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return NULL;

	return lcd_tcon_conf;
}

struct tcon_rmem_s *get_lcd_tcon_rmem(void)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return NULL;

	return &tcon_rmem;
}

struct tcon_mem_map_table_s *get_lcd_tcon_mm_table(void)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return NULL;

	return &tcon_mm_table;
}

unsigned int lcd_tcon_data_size_align(unsigned int size)
{
	unsigned int new_size;

	/* ready for burst 128bit */
	new_size = ((size + 15) / 16) * 16;

	return new_size;
}

unsigned char lcd_tcon_checksum(unsigned char *buf, unsigned int len)
{
	unsigned int temp = 0;
	unsigned int i;

	if (!buf)
		return 0;
	if (len == 0)
		return 0;
	for (i = 0; i < len; i++)
		temp += buf[i];

	return (unsigned char)(temp & 0xff);
}

unsigned char lcd_tcon_lrc(unsigned char *buf, unsigned int len)
{
	unsigned char temp = 0;
	unsigned int i;

	if (!buf)
		return 0xff;
	if (len == 0)
		return 0xff;
	temp = buf[0];
	for (i = 1; i < len; i++)
		temp = temp ^ buf[i];

	return temp;
}

static unsigned char *lcd_tcon_paddrtovaddr(unsigned long paddr,
					    unsigned int mem_size)
{
	unsigned int highmem_flag = 0;
	int max_mem_size = 0;
	void *vaddr = NULL;

	if (tcon_rmem.flag == 0) {
		LCDPR("%s: invalid paddr\n", __func__);
		return NULL;
	}

	highmem_flag = PageHighMem(phys_to_page(paddr));
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("%s: paddr 0x%lx highmem_flag:%d\n",
		      __func__, paddr, highmem_flag);
	}
	if (highmem_flag) {
		max_mem_size = PAGE_ALIGN(mem_size);
		max_mem_size = roundup(max_mem_size, PAGE_SIZE);
		vaddr = lcd_vmap(paddr, max_mem_size);
		if (!vaddr) {
			LCDPR("tcon paddr mapping error: addr: 0x%lx\n",
				(unsigned long)paddr);
			return NULL;
		}
		/*LCDPR("tcon vaddr: 0x%p\n", vaddr);*/
	} else {
		vaddr = phys_to_virt(paddr);
		if (!vaddr) {
			LCDERR("tcon vaddr mapping failed: 0x%lx\n",
			       (unsigned long)paddr);
			return NULL;
		}
		/*LCDPR("tcon vaddr: 0x%p\n", vaddr);*/
	}
	return (unsigned char *)vaddr;
}

/* **********************************
 * tcon function api
 * **********************************
 */
unsigned int lcd_tcon_reg_read(struct aml_lcd_drv_s *pdrv, unsigned int addr)
{
	unsigned int val;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return 0;

	if (addr < lcd_tcon_conf->top_reg_base) {
		if (lcd_tcon_conf->core_reg_width == 8)
			val = lcd_tcon_read_byte(pdrv, addr);
		else
			val = lcd_tcon_read(pdrv, addr);
	} else {
		val = lcd_tcon_read(pdrv, addr);
	}

	return val;
}

void lcd_tcon_reg_write(struct aml_lcd_drv_s *pdrv,
			unsigned int addr, unsigned int val)
{
	unsigned char temp;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	if (addr < lcd_tcon_conf->top_reg_base) {
		if (lcd_tcon_conf->core_reg_width == 8) {
			temp = (unsigned char)val;
			lcd_tcon_write_byte(pdrv, addr, temp);
		} else {
			lcd_tcon_write(pdrv, addr, val);
		}
	} else {
		lcd_tcon_write(pdrv, addr, val);
	}
}

#define PR_BUF_MAX    200
void lcd_tcon_reg_table_print(void)
{
	int i, j, n, cnt;
	char *buf;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	if (!tcon_mm_table.core_reg_table) {
		LCDERR("%s: reg_table is null\n", __func__);
		return;
	}

	buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	LCDPR("%s:\n", __func__);
	cnt = tcon_mm_table.core_reg_table_size;
	for (i = 0; i < cnt; i += 16) {
		n = snprintf(buf, PR_BUF_MAX, "%04x: ", i);
		for (j = 0; j < 16; j++) {
			if ((i + j) >= cnt)
				break;
			n += snprintf(buf + n, PR_BUF_MAX, " %02x",
				      tcon_mm_table.core_reg_table[i + j]);
		}
		buf[n] = '\0';
		pr_info("%s\n", buf);
	}
	kfree(buf);
}

void lcd_tcon_reg_readback_print(struct aml_lcd_drv_s *pdrv)
{
	int i, j, n, cnt, offset;
	char *buf;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	LCDPR("%s:\n", __func__);
	cnt = tcon_mm_table.core_reg_table_size;
	offset = lcd_tcon_conf->core_reg_start;
	if (lcd_tcon_conf->core_reg_width == 8) {
		for (i = offset; i < cnt; i += 16) {
			n = snprintf(buf, PR_BUF_MAX, "%04x: ", i);
			for (j = 0; j < 16; j++) {
				if ((i + j) >= cnt)
					break;
				n += snprintf(buf + n, PR_BUF_MAX, " %02x",
					lcd_tcon_read_byte(pdrv, i + j));
			}
			buf[n] = '\0';
			pr_info("%s\n", buf);
		}
	} else {
		if (lcd_tcon_conf->reg_table_width == 32) {
			cnt /= 4;
			for (i = offset; i < cnt; i += 4) {
				n = snprintf(buf, PR_BUF_MAX, "%04x: ", i);
				for (j = 0; j < 4; j++) {
					if ((i + j) >= cnt)
						break;
					n += snprintf(buf + n, PR_BUF_MAX, " %08x",
						lcd_tcon_read(pdrv, i + j));
				}
				buf[n] = '\0';
				pr_info("%s\n", buf);
			}
		} else {
			for (i = offset; i < cnt; i += 16) {
				n = snprintf(buf, PR_BUF_MAX, "%04x: ", i);
				for (j = 0; j < 16; j++) {
					if ((i + j) >= cnt)
						break;
					n += snprintf(buf + n, PR_BUF_MAX, " %02x",
						lcd_tcon_read(pdrv, i + j));
				}
				buf[n] = '\0';
				pr_info("%s\n", buf);
			}
		}
	}

	kfree(buf);
}

unsigned int lcd_tcon_table_read(unsigned int addr)
{
	unsigned char *table8;
	unsigned int *table32, size = 0, val = 0;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return 0;

	if (!tcon_mm_table.core_reg_table) {
		LCDERR("tcon reg_table is null\n");
		return 0;
	}

	if (lcd_tcon_conf->core_reg_width == 8)
		size = tcon_mm_table.core_reg_table_size;
	else
		size = tcon_mm_table.core_reg_table_size / 4;
	if (addr >= size) {
		LCDERR("invalid tcon reg_table addr: 0x%04x\n", addr);
		return 0;
	}

	if (lcd_tcon_conf->core_reg_width == 8) {
		table8 = tcon_mm_table.core_reg_table;
		val = table8[addr];
	} else {
		table32 = (unsigned int *)tcon_mm_table.core_reg_table;
		val = table32[addr];
	}

	return val;
}

unsigned int lcd_tcon_table_write(unsigned int addr, unsigned int val)
{
	unsigned char *table8;
	unsigned int *table32, size = 0, read_val = 0;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return 0;

	if (!tcon_mm_table.core_reg_table) {
		LCDERR("tcon reg_table is null\n");
		return 0;
	}

	if (lcd_tcon_conf->core_reg_width == 8)
		size = tcon_mm_table.core_reg_table_size;
	else
		size = tcon_mm_table.core_reg_table_size / 4;
	if (addr >= size) {
		LCDERR("invalid tcon reg_table addr: 0x%04x\n", addr);
		return 0;
	}

	if (lcd_tcon_conf->core_reg_width == 8) {
		table8 = tcon_mm_table.core_reg_table;
		table8[addr] = (unsigned char)(val & 0xff);
		read_val = table8[addr];
	} else {
		table32 = (unsigned int *)tcon_mm_table.core_reg_table;
		table32[addr] = val;
		read_val = table32[addr];
	}

	return read_val;
}

int lcd_tcon_info_print(char *buf, int offset)
{
	unsigned int size, cnt, m;
	unsigned char *mem_vaddr;
	char *str;
	int len = 0, n, ret;
	int i;

	ret = lcd_tcon_valid_check();
	if (ret)
		return len;

	n = lcd_debug_info_len(len + offset);
	if (tcon_mm_table.version == 0) {
		len += snprintf((buf + len), n,
			"\ntcon info:\n"
			"core_reg_width:         %d\n"
			"reg_table_len:          %d\n"
			"tcon_bin_ver:           %s\n"
			"tcon_rmem_flag:         %d\n"
			"tcon_data_flag:         %d\n"
			"reserved_mem paddr:     0x%lx\n"
			"reserved_mem size:      0x%x\n"
			"bin_path_mem_paddr:     0x%lx\n"
			"bin_path_mem_vaddr:     0x%p\n"
			"bin_path_mem_size:      0x%x\n"
			"vac_mem paddr:          0x%lx\n"
			"vac_mem vaddr:          0x%p\n"
			"vac_mem size:           0x%x\n"
			"demura_set_mem paddr:   0x%lx\n"
			"demura_set_mem vaddr:   0x%p\n"
			"demura_set_mem size:    0x%x\n"
			"demura_lut_mem paddr:   0x%lx\n"
			"demura_lut_mem vaddr:   0x%p\n"
			"demura_lut_mem size:    0x%x\n"
			"acc_lut_mem paddr:      0x%lx\n"
			"acc_lut_mem vaddr:      0x%p\n"
			"acc_lut_mem size:       0x%x\n\n",
			lcd_tcon_conf->core_reg_width,
			lcd_tcon_conf->reg_table_len,
			tcon_local_cfg.bin_ver,
			tcon_rmem.flag,
			tcon_mm_table.tcon_data_flag,
			(unsigned long)tcon_rmem.rsv_mem_paddr,
			tcon_rmem.rsv_mem_size,
			(unsigned long)tcon_rmem.bin_path_rmem.mem_paddr,
			tcon_rmem.bin_path_rmem.mem_vaddr,
			tcon_rmem.bin_path_rmem.mem_size,
			(unsigned long)tcon_rmem.vac_rmem.mem_paddr,
			tcon_rmem.vac_rmem.mem_vaddr,
			tcon_rmem.vac_rmem.mem_size,
			(unsigned long)tcon_rmem.demura_set_rmem.mem_paddr,
			tcon_rmem.demura_set_rmem.mem_vaddr,
			tcon_rmem.demura_set_rmem.mem_size,
			(unsigned long)tcon_rmem.demura_lut_rmem.mem_paddr,
			tcon_rmem.demura_lut_rmem.mem_vaddr,
			tcon_rmem.demura_lut_rmem.mem_size,
			(unsigned long)tcon_rmem.acc_lut_rmem.mem_paddr,
			tcon_rmem.acc_lut_rmem.mem_vaddr,
			tcon_rmem.acc_lut_rmem.mem_size);
		if (tcon_rmem.axi_rmem) {
			for (i = 0; i < lcd_tcon_conf->axi_bank; i++) {
				n = lcd_debug_info_len(len + offset);
				len += snprintf((buf + len), n,
					"axi_mem[%d]_paddr:  0x%lx\n"
					"axi_mem[%d]_vaddr:  0x%p\n"
					"axi_mem[%d]_size:   0x%x\n",
					i, (unsigned long)tcon_rmem.axi_rmem[i].mem_paddr,
					i, tcon_rmem.axi_rmem[i].mem_vaddr,
					i, tcon_rmem.axi_rmem[i].mem_size);
			}
		}
	} else {
		len += snprintf((buf + len), n,
			"\ntcon info:\n"
			"core_reg_width:       %d\n"
			"reg_table_len:        %d\n"
			"tcon_bin_ver:         %s\n"
			"tcon_rmem_flag:       %d\n"
			"tcon_data_flag:       %d\n"
			"reserved_mem paddr:   0x%lx\n"
			"reserved_mem size:    0x%x\n"
			"bin_path_mem_paddr:   0x%lx\n"
			"bin_path_mem_vaddr:   0x%px\n"
			"bin_path_mem_size:    0x%x\n\n",
			lcd_tcon_conf->core_reg_width,
			lcd_tcon_conf->reg_table_len,
			tcon_local_cfg.bin_ver,
			tcon_rmem.flag,
			tcon_mm_table.tcon_data_flag,
			(unsigned long)tcon_rmem.rsv_mem_paddr,
			tcon_rmem.rsv_mem_size,
			(unsigned long)tcon_rmem.bin_path_rmem.mem_paddr,
			tcon_rmem.bin_path_rmem.mem_vaddr,
			tcon_rmem.bin_path_rmem.mem_size);
		if (tcon_rmem.axi_rmem) {
			for (i = 0; i < lcd_tcon_conf->axi_bank; i++) {
				n = lcd_debug_info_len(len + offset);
				len += snprintf((buf + len), n,
					"axi_mem[%d]_paddr:  0x%lx\n"
					"axi_mem[%d]_size:   0x%x\n",
					i, (unsigned long)tcon_rmem.axi_rmem[i].mem_paddr,
					i, tcon_rmem.axi_rmem[i].mem_size);
			}
		}
		if (tcon_mm_table.data_mem_vaddr) {
			for (i = 0; i < tcon_mm_table.block_cnt; i++) {
				n = lcd_debug_info_len(len + offset);
				len += snprintf((buf + len), n,
					"data_mem_vaddr[%d]: 0x%px\n",
					i, tcon_mm_table.data_mem_vaddr[i]);
			}
		}
		if (tcon_mm_table.data_multi_cnt > 0 && tcon_mm_table.data_multi) {
			for (i = 0; i < tcon_mm_table.data_multi_cnt; i++) {
				n = lcd_debug_info_len(len + offset);
				len += snprintf((buf + len), n,
					"data_multi[%d] type:    0x%x\n"
					"data_multi[%d] list_cnt: %d\n"
					"data_multi[%d] sel_id:   %d\n",
					i, tcon_mm_table.data_multi[i].block_type,
					i, tcon_mm_table.data_multi[i].list_cnt,
					i, tcon_mm_table.data_multi[i].sel_id);
			}
		}
	}

	if (tcon_rmem.bin_path_rmem.mem_vaddr) {
		mem_vaddr = tcon_rmem.bin_path_rmem.mem_vaddr;
		size = mem_vaddr[4] |
			(mem_vaddr[5] << 8) |
			(mem_vaddr[6] << 16) |
			(mem_vaddr[7] << 24);
		cnt = mem_vaddr[16] |
			(mem_vaddr[17] << 8) |
			(mem_vaddr[18] << 16) |
			(mem_vaddr[19] << 24);
		if (size < (32 + 256 * cnt))
			return len;
		if (cnt > 32)
			return len;
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "\n");
		for (i = 0; i < cnt; i++) {
			m = 32 + 256 * i;
			size = mem_vaddr[m] |
				(mem_vaddr[m + 1] << 8) |
				(mem_vaddr[m + 2] << 16) |
				(mem_vaddr[m + 3] << 24);
			str = (char *)&mem_vaddr[m + 4];
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
					"tcon_path[%d]: size: 0x%x, %s\n",
					i, size, str);
		}
	}

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"\ntcon_status:\n"
		"vac_valid:      %d\n"
		"demura_valid:   %d\n"
		"acc_valid:      %d\n",
		((tcon_mm_table.valid_flag >> LCD_TCON_DATA_VALID_VAC) & 0x1),
		((tcon_mm_table.valid_flag >> LCD_TCON_DATA_VALID_DEMURA) & 0x1),
		((tcon_mm_table.valid_flag >> LCD_TCON_DATA_VALID_ACC) & 0x1));
	return len;
}

int lcd_tcon_core_reg_get(struct aml_lcd_drv_s *pdrv,
			  unsigned char *buf, unsigned int size)
{
	unsigned int reg_max = 0, offset = 0, val;
	int i, ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	if (size > lcd_tcon_conf->reg_table_len) {
		LCDERR("%s: size 0x%x is not enough(0x%x)\n",
		       __func__, size, lcd_tcon_conf->reg_table_len);
		return -1;
	}

	offset = lcd_tcon_conf->core_reg_start;
	if (lcd_tcon_conf->core_reg_width == 8) {
		for (i = offset; i < size; i++)
			buf[i] = lcd_tcon_read_byte(pdrv, i);
	} else {
		reg_max = size / 4;
		for (i = offset; i < reg_max; i++) {
			val = lcd_tcon_read(pdrv, i);
			buf[i * 4] = val & 0xff;
			buf[i * 4 + 1] = (val >> 8) & 0xff;
			buf[i * 4 + 2] = (val >> 16) & 0xff;
			buf[i * 4 + 3] = (val >> 24) & 0xff;
		}
	}

	return 0;
}

int lcd_tcon_od_set(struct aml_lcd_drv_s *pdrv, int flag)
{
	unsigned int reg, bit, temp;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	if (lcd_tcon_conf->reg_core_od == REG_LCD_TCON_MAX) {
		LCDERR("%s: invalid od reg\n", __func__);
		return -1;
	}

	if (flag) {
		if (tcon_rmem.flag == 0) {
			LCDERR("%s: invalid memory, disable od function\n",
				__func__);
			return -1;
		}
	}

	if (!(pdrv->status & LCD_STATUS_IF_ON))
		return -1;

	reg = lcd_tcon_conf->reg_core_od;
	bit = lcd_tcon_conf->bit_od_en;
	temp = flag ? 1 : 0;
	if (lcd_tcon_conf->core_reg_width == 8)
		lcd_tcon_setb_byte(pdrv, reg, temp, bit, 1);
	else
		lcd_tcon_setb(pdrv, reg, temp, bit, 1);

	msleep(100);
	LCDPR("%s: %d\n", __func__, flag);

	return 0;
}

int lcd_tcon_od_get(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg, bit, temp;
	int ret = 0;

	ret = lcd_tcon_valid_check();
	if (ret)
		return 0;

	if (lcd_tcon_conf->reg_core_od == REG_LCD_TCON_MAX) {
		LCDERR("%s: invalid od reg\n", __func__);
		return 0;
	}

	reg = lcd_tcon_conf->reg_core_od;
	bit = lcd_tcon_conf->bit_od_en;
	if (lcd_tcon_conf->core_reg_width == 8)
		temp = lcd_tcon_read_byte(pdrv, reg);
	else
		temp = lcd_tcon_read(pdrv, reg);
	ret = ((temp >> bit) & 1);

	return ret;
}

void lcd_tcon_global_reset(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	if (lcd_tcon_conf->tcon_global_reset) {
		lcd_tcon_conf->tcon_global_reset(pdrv);
		LCDPR("reset tcon\n");
	}
}

int lcd_tcon_gamma_set_pattern(struct aml_lcd_drv_s *pdrv,
			       unsigned int bit_width, unsigned int gamma_r,
			       unsigned int gamma_g, unsigned int gamma_b)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	if (lcd_tcon_conf->tcon_gamma_pattern) {
		ret = lcd_tcon_conf->tcon_gamma_pattern(pdrv,
							bit_width, gamma_r,
							gamma_g, gamma_b);
		LCDPR("%s: %dbits gamma r:0x%x g:0x%x b:0x%x\n",
		      __func__, bit_width, gamma_r, gamma_g, gamma_b);
	}

	return ret;
}

int lcd_tcon_core_update(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	lcd_tcon_core_reg_set(pdrv, lcd_tcon_conf, &tcon_mm_table);

	return 0;
}

int lcd_tcon_enable(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	if (lcd_tcon_conf->tcon_enable)
		lcd_tcon_conf->tcon_enable(pdrv);

	return 0;
}

void lcd_tcon_disable(struct aml_lcd_drv_s *pdrv)
{
	unsigned int i;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	LCDPR("%s\n", __func__);

	/* release data*/
	if (tcon_mm_table.version) {
		for (i = 0; i < tcon_mm_table.block_cnt; i++) {
			if (!tcon_mm_table.data_mem_vaddr[i])
				continue;
			if (!tcon_mm_table.data_size)
				continue;
			memset(tcon_mm_table.data_mem_vaddr[i], 0,
			       tcon_mm_table.data_size[i]);
		}
		tcon_mm_table.valid_flag = 0;
		tcon_mm_table.tcon_data_flag = 0;
		lcd_tcon_data_multi_remvoe(&tcon_mm_table);
	}

	if (lcd_tcon_conf->tcon_disable)
		lcd_tcon_conf->tcon_disable(pdrv);
}

/* return:
 *    0: matched
 *   -1: not match
 *    1: default
 */
static int lcd_tcon_data_multi_match_check(struct aml_lcd_drv_s *pdrv, unsigned char *data_buf)
{
	struct lcd_config_s *pconf;
#ifdef CONFIG_AMLOGIC_BACKLIGHT
	struct aml_bl_drv_s *bldrv;
	struct bl_pwm_config_s *bl_pwm = NULL;
#endif
	struct lcd_tcon_data_block_header_s *block_header;
	struct lcd_tcon_data_block_ext_header_s *ext_header;
	union lcd_tcon_data_part_u data_part;
	unsigned char *p, part_type;
	unsigned int size, data_offset, offset, i, j, k;
	unsigned short part_cnt, block_ctrl_flag, ctrl_data_flag, ctrl_sub_type;
	unsigned int data_byte, data_cnt, data, min, max;
	unsigned int sync_num, sync_den, temp;

	pconf = &pdrv->config;
	block_header = (struct lcd_tcon_data_block_header_s *)data_buf;
	p = data_buf + LCD_TCON_DATA_BLOCK_HEADER_SIZE;
	ext_header = (struct lcd_tcon_data_block_ext_header_s *)p;
	part_cnt = ext_header->part_cnt;

	block_ctrl_flag = block_header->block_ctrl;
	data_offset = LCD_TCON_DATA_BLOCK_HEADER_SIZE + block_header->ext_header_size;
	size = 0;
	for (i = 0; i < part_cnt; i++) {
		p = data_buf + data_offset;
		part_type = p[LCD_TCON_DATA_PART_NAME_SIZE + 3];
		if (lcd_debug_print_flag & LCD_DBG_PR_ISR)
			LCDPR("%s: %s, type = 0x%02x,\n", __func__, p, part_type);

		switch (part_type) {
		case LCD_TCON_DATA_PART_TYPE_CONTROL:
			block_ctrl_flag = 0;
			data_part.ctrl = (struct lcd_tcon_data_part_ctrl_s *)p;
			offset = LCD_TCON_DATA_PART_CTRL_SIZE_PRE;
			size = offset + (data_part.ctrl->data_cnt *
					 data_part.ctrl->data_byte_width);
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_multi_match_check_err_size;
			ctrl_data_flag = data_part.ctrl->ctrl_data_flag;
			ctrl_sub_type = data_part.ctrl->ctrl_sub_type;
			if (ctrl_data_flag != LCD_TCON_DATA_CTRL_FLAG_MULTI)
				break;

			data_byte = data_part.ctrl->data_byte_width;
			data_cnt = data_part.ctrl->data_cnt;

			k = offset;
			data = 0;
			min = 0;
			max = 0;

			if (ctrl_sub_type == LCD_TCON_DATA_CTRL_MULTI_VFREQ) {
				sync_num = pconf->timing.sync_duration_num;
				sync_den = pconf->timing.sync_duration_den;
				temp = sync_num * 100 / sync_den;

				if (data_cnt != 2)
					goto lcd_tcon_data_multi_match_check_err_data_cnt;
				for (j = 0; j < data_byte; j++)
					min |= (p[k + j] << (j * 8));
				k += data_byte;
				for (j = 0; j < data_byte; j++)
					max |= (p[k + j] << (j * 8));

				if (temp < min || temp > max)
					goto lcd_tcon_data_multi_match_check_exit;
			} else if (ctrl_sub_type == LCD_TCON_DATA_CTRL_MULTI_BL_LEVEL) {
#ifdef CONFIG_AMLOGIC_BACKLIGHT
				bldrv = aml_bl_get_driver(pdrv->index);
				if (!bldrv)
					goto lcd_tcon_data_multi_match_check_err_type;
				temp = bldrv->level;

				if (data_cnt != 2)
					goto lcd_tcon_data_multi_match_check_err_data_cnt;
				for (j = 0; j < data_byte; j++)
					min |= (p[k + j] << (j * 8));
				k += data_byte;
				for (j = 0; j < data_byte; j++)
					max |= (p[k + j] << (j * 8));
				if (temp < min || temp > max)
					goto lcd_tcon_data_multi_match_check_exit;
#endif
			} else if (ctrl_sub_type == LCD_TCON_DATA_CTRL_MULTI_BL_PWM_DUTY) {
#ifdef CONFIG_AMLOGIC_BACKLIGHT
				bldrv = aml_bl_get_driver(pdrv->index);
				if (!bldrv)
					goto lcd_tcon_data_multi_match_check_err_type;

				if (data_cnt != 3)
					goto lcd_tcon_data_multi_match_check_err_data_cnt;
				for (j = 0; j < data_byte; j++)
					data |= (p[k + j] << (j * 8));
				k += data_byte;
				for (j = 0; j < data_byte; j++)
					min |= (p[k + j] << (j * 8));
				k += data_byte;
				for (j = 0; j < data_byte; j++)
					max |= (p[k + j] << (j * 8));

				switch (bldrv->bconf.method) {
				case BL_CTRL_PWM:
					bl_pwm = bldrv->bconf.bl_pwm;
					break;
				case BL_CTRL_PWM_COMBO:
					if (data == 0)
						bl_pwm = bldrv->bconf.bl_pwm_combo0;
					else
						bl_pwm = bldrv->bconf.bl_pwm_combo1;
					break;
				default:
					break;
				}
				if (!bl_pwm)
					goto lcd_tcon_data_multi_match_check_err_type;

				temp = bl_pwm->pwm_duty;
				if (temp < min || temp > max)
					goto lcd_tcon_data_multi_match_check_exit;
#endif
			} else if (ctrl_sub_type == LCD_TCON_DATA_CTRL_DEFAULT) {
				return 1;
			}
			break;
		default:
			if (block_ctrl_flag)
				goto lcd_tcon_data_multi_match_check_err_ctrl;
			return 0;
		}
		data_offset += size;
	}

	return 0;

lcd_tcon_data_multi_match_check_exit:
	if (lcd_debug_print_flag & LCD_DBG_PR_ISR)
		LCDPR("%s: block %s control exit\n", __func__, block_header->name);
	return -1;

lcd_tcon_data_multi_match_check_err_data_cnt:
	LCDERR("%s: block %s data_cnt error\n", __func__, block_header->name);
	return -1;

lcd_tcon_data_multi_match_check_err_ctrl:
	LCDERR("%s: block %s need control part\n", __func__, block_header->name);
	return -1;

#ifdef CONFIG_AMLOGIC_BACKLIGHT
lcd_tcon_data_multi_match_check_err_type:
	LCDERR("%s: block %s control type invalid\n", __func__, block_header->name);
	return -1;
#endif

lcd_tcon_data_multi_match_check_err_size:
	LCDERR("%s: block %s size error\n", __func__, block_header->name);
	return -1;
}

static int lcd_tcon_data_multi_update(struct aml_lcd_drv_s *pdrv,
				      struct tcon_mem_map_table_s *mm_table)
{
	struct tcon_data_multi_s *data_multi = NULL;
	struct tcon_data_list_s *temp_list, *dft_list, *match_list;
	int i, ret = 0;

	if (!mm_table || !mm_table->data_multi)
		return 0;
	if (mm_table->data_multi_cnt == 0)
		return 0;

	for (i = 0; i < mm_table->data_multi_cnt; i++) {
		data_multi = &mm_table->data_multi[i];
		temp_list = data_multi->list_header;
		dft_list = NULL;
		match_list = NULL;
		while (temp_list) {
			/* bypass current data */
			if (data_multi->sel_id == temp_list->id) {
				temp_list = temp_list->next;
				continue;
			}
			ret = lcd_tcon_data_multi_match_check(pdrv, temp_list->block_vaddr);
			if (ret == 0) {
				match_list = temp_list;
				break;
			}
			if (ret == 1)
				dft_list = temp_list;

			temp_list = temp_list->next;
		}

		if (!match_list) {
			if (dft_list) {
				ret = lcd_tcon_data_common_parse_set(pdrv,
					dft_list->block_vaddr, 0);
				if (ret == 0) {
					data_multi->sel_id = dft_list->id;
					data_multi->list_cur = dft_list;
				}
				if (lcd_debug_print_flag & LCD_DBG_PR_ISR) {
					LCDPR("%s: sel default, type=0x%x, sel_id=%d\n",
					      __func__, data_multi->block_type,
					      data_multi->sel_id);
				}
			}
		} else {
			ret = lcd_tcon_data_common_parse_set(pdrv, match_list->block_vaddr, 0);
			if (ret == 0) {
				data_multi->sel_id = match_list->id;
				data_multi->list_cur = match_list;
			}
			if (lcd_debug_print_flag & LCD_DBG_PR_ISR) {
				LCDPR("%s: type=0x%x, sel_id=%d\n",
				      __func__, data_multi->block_type,
				      data_multi->sel_id);
			}
		}
	}

	return ret;
}

void lcd_tcon_vsync_isr(struct aml_lcd_drv_s *pdrv)
{
	if (tcon_mm_table.version)
		lcd_tcon_data_multi_update(pdrv, &tcon_mm_table);
}

/* **********************************
 * tcon config
 * **********************************
 */
static int lcd_tcon_vac_load(void)
{
	unsigned int n;
	unsigned char *buff = tcon_rmem.vac_rmem.mem_vaddr;
	unsigned int data_cnt;
	unsigned char data_checksum, data_lrc, temp_checksum, temp_lrc;

	if (tcon_rmem.vac_rmem.mem_size == 0 || !buff)
		return -1;

	data_cnt = (buff[0] |
		    (buff[1] << 8) |
		    (buff[2] << 16) |
		    (buff[3] << 24));
	if (data_cnt == 0) {
		LCDPR("%s: vac data_cnt is zero\n", __func__);
		return -1;
	}
	data_checksum = buff[4];
	data_lrc = buff[5];
	temp_checksum = lcd_tcon_checksum(&buff[8], data_cnt);
	temp_lrc = lcd_tcon_lrc(&buff[8], data_cnt);
	if (data_checksum != temp_checksum) {
		LCDERR("%s: vac_data checksum error\n", __func__);
		return -1;
	}
	if (data_lrc != temp_lrc) {
		LCDERR("%s: vac_data lrc error\n", __func__);
		return -1;
	}
	if (buff[6] != 0x55 || buff[7] != 0xaa) {
		LCDERR("%s: vac_data pattern error\n", __func__);
		return -1;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV) {
		for (n = 0; n < 30; n++)
			LCDPR("%s: vac_data[%d]: 0x%x", __func__,
			      n, buff[n * 1]);
	}

	return 0;
}

static int lcd_tcon_demura_set_load(void)
{
	unsigned int n;
	char *buff = tcon_rmem.demura_set_rmem.mem_vaddr;
	unsigned int data_cnt;
	unsigned char data_checksum, data_lrc, temp_checksum, temp_lrc;

	if (tcon_rmem.demura_set_rmem.mem_size == 0 || !buff)
		return -1;

	data_cnt = (buff[0] |
		    (buff[1] << 8) |
		    (buff[2] << 16) |
		    (buff[3] << 24));
	if (data_cnt == 0) {
		LCDPR("%s: demura_set data_cnt is zero\n", __func__);
		return -1;
	}
	data_checksum = buff[4];
	data_lrc = buff[5];
	temp_checksum = lcd_tcon_checksum(&buff[8], data_cnt);
	temp_lrc = lcd_tcon_lrc(&buff[8], data_cnt);
	if (data_checksum != temp_checksum) {
		LCDERR("%s: demura_set checksum error\n", __func__);
		return -1;
	}
	if (data_lrc != temp_lrc) {
		LCDERR("%s: demura_set lrc error\n", __func__);
		return -1;
	}
	if (buff[6] != 0x55 || buff[7] != 0xaa) {
		LCDERR("%s: demura_set pattern error\n", __func__);
		return -1;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV) {
		for (n = 0; n < 30; n++)
			LCDPR("%s: demura_set[%d]: 0x%x",
			      __func__, n, buff[n]);
	}

	return 0;
}

static int lcd_tcon_demura_lut_load(void)
{
	unsigned int n;
	char *buff = tcon_rmem.demura_lut_rmem.mem_vaddr;
	unsigned int data_cnt;
	unsigned char data_checksum, data_lrc, temp_checksum, temp_lrc;

	if (tcon_rmem.demura_lut_rmem.mem_size == 0 || !buff)
		return -1;

	data_cnt = (buff[0] |
		    (buff[1] << 8) |
		    (buff[2] << 16) |
		    (buff[3] << 24));
	if (data_cnt == 0) {
		LCDPR("%s: demura_lut data_cnt is zero\n", __func__);
		return -1;
	}
	data_checksum = buff[4];
	data_lrc = buff[5];
	temp_checksum = lcd_tcon_checksum(&buff[8], data_cnt);
	temp_lrc = lcd_tcon_lrc(&buff[8], data_cnt);
	if (data_checksum != temp_checksum) {
		LCDERR("%s: demura_lut checksum error\n", __func__);
		return -1;
	}
	if (data_lrc != temp_lrc) {
		LCDERR("%s: demura_lut lrc error\n", __func__);
		return -1;
	}
	if (buff[6] != 0x55 || buff[7] != 0xaa) {
		LCDERR("%s: demura_lut pattern error\n", __func__);
		return -1;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV) {
		for (n = 0; n < 30; n++)
			LCDPR("%s: demura_lut[%d]: 0x%x\n",
			      __func__, n, buff[n]);
	}

	return 0;
}

static int lcd_tcon_acc_lut_load(void)
{
	unsigned int n;
	char *buff = tcon_rmem.acc_lut_rmem.mem_vaddr;
	unsigned int data_cnt;
	unsigned char data_checksum, data_lrc, temp_checksum, temp_lrc;

	if (tcon_rmem.acc_lut_rmem.mem_size == 0 || !buff)
		return -1;

	data_cnt = (buff[0] |
		    (buff[1] << 8) |
		    (buff[2] << 16) |
		    (buff[3] << 24));
	if (data_cnt == 0) {
		LCDPR("%s: acc_lut data_cnt is zero\n", __func__);
		return -1;
	}
	data_checksum = buff[4];
	data_lrc = buff[5];
	temp_checksum = lcd_tcon_checksum(&buff[8], data_cnt);
	temp_lrc = lcd_tcon_lrc(&buff[8], data_cnt);
	if (data_checksum != temp_checksum) {
		LCDERR("%s: acc_lut checksum error\n", __func__);
		return -1;
	}
	if (data_lrc != temp_lrc) {
		LCDERR("%s: acc_lut lrc error\n", __func__);
		return -1;
	}
	if (buff[6] != 0x55 || buff[7] != 0xaa) {
		LCDERR("%s: acc_lut pattern error\n", __func__);
		return -1;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV) {
		for (n = 0; n < 30; n++)
			LCDPR("%s: acc_lut[%d]: 0x%x\n",
			      __func__, n, buff[n]);
	}

	return 0;
}

static inline void lcd_tcon_data_list_add(struct tcon_data_list_s *list_header,
					  struct tcon_data_list_s *data_list)
{
	struct tcon_data_list_s *temp_list;

	if (!data_list)
		return;

	if (!list_header) { /* new list */
		list_header = data_list;
		return;
	}

	temp_list = list_header;
	while (temp_list->next)
		temp_list = temp_list->next;
	temp_list->next = data_list;
}

static inline void lcd_tcon_data_list_remove(struct tcon_data_list_s *list_header)
{
	struct tcon_data_list_s *cur_list, *next_list;

	if (!list_header)
		return;

	/* add to exist list */
	cur_list = list_header;
	while (cur_list) {
		next_list = cur_list->next;
		kfree(cur_list);
		cur_list = next_list;
	}
}

/* return:
 *    0: find
 *   -1: failed
 */
static int lcd_tcon_data_multi_default_find(unsigned char *data_buf)
{
	struct lcd_tcon_data_block_header_s *block_header;
	struct lcd_tcon_data_block_ext_header_s *ext_header;
	union lcd_tcon_data_part_u data_part;
	unsigned char *p, part_type;
	unsigned int size, data_offset, offset, i;
	unsigned short part_cnt, block_ctrl_flag, ctrl_data_flag, ctrl_sub_type;

	block_header = (struct lcd_tcon_data_block_header_s *)data_buf;
	p = data_buf + LCD_TCON_DATA_BLOCK_HEADER_SIZE;
	ext_header = (struct lcd_tcon_data_block_ext_header_s *)p;
	part_cnt = ext_header->part_cnt;

	block_ctrl_flag = block_header->block_ctrl;
	data_offset = LCD_TCON_DATA_BLOCK_HEADER_SIZE + block_header->ext_header_size;
	size = 0;
	for (i = 0; i < part_cnt; i++) {
		p = data_buf + data_offset;
		part_type = p[LCD_TCON_DATA_PART_NAME_SIZE + 3];

		switch (part_type) {
		case LCD_TCON_DATA_PART_TYPE_CONTROL:
			block_ctrl_flag = 0;
			data_part.ctrl = (struct lcd_tcon_data_part_ctrl_s *)p;
			offset = LCD_TCON_DATA_PART_CTRL_SIZE_PRE;
			size = offset + (data_part.ctrl->data_cnt *
					 data_part.ctrl->data_byte_width);
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_multi_match_check_err_size;
			ctrl_data_flag = data_part.ctrl->ctrl_data_flag;
			ctrl_sub_type = data_part.ctrl->ctrl_sub_type;
			if (ctrl_data_flag != LCD_TCON_DATA_CTRL_FLAG_MULTI)
				break;

			if (ctrl_sub_type == LCD_TCON_DATA_CTRL_DEFAULT)
				return 0;
			break;
		default:
			if (block_ctrl_flag)
				goto lcd_tcon_data_multi_match_check_err_ctrl;
			return -1;
		}
		data_offset += size;
	}

	return -1;

lcd_tcon_data_multi_match_check_err_ctrl:
	LCDERR("%s: block %s need control part\n", __func__, block_header->name);
	return -1;

lcd_tcon_data_multi_match_check_err_size:
	LCDERR("%s: block %s size error\n", __func__, block_header->name);
	return -1;
}

int lcd_tcon_data_multi_add(struct tcon_mem_map_table_s *mm_table,
			    struct lcd_tcon_data_block_header_s *block_header,
			    unsigned int index)
{
	struct tcon_data_multi_s *data_multi = NULL;
	struct tcon_data_list_s *data_list;
	int i, ret;

	if (!mm_table->data_multi)
		return -1;
	if (mm_table->data_multi_cnt > 0) {
		if ((mm_table->data_multi_cnt + 1) >= mm_table->block_cnt) {
			LCDERR("%s: multi block %s invalid\n",
			       __func__, block_header->name);
			return -1;
		}
	}

	/* create list */
	data_list = kmalloc(sizeof(*data_list), GFP_KERNEL);
	if (!data_list)
		return -1;
	data_list->block_vaddr = mm_table->data_mem_vaddr[index];
	data_list->next = NULL;
	data_list->id = index;

	for (i = 0; i < mm_table->data_multi_cnt; i++) {
		if (mm_table->data_multi[i].block_type == block_header->block_type) {
			data_multi = &mm_table->data_multi[i];
			break;
		}
	}
	if (!data_multi) { /* create new list */
		data_multi = &mm_table->data_multi[mm_table->data_multi_cnt];
		mm_table->data_multi_cnt++;
		data_multi->block_type = block_header->block_type;
		data_multi->list_header = NULL;
		data_multi->list_cnt = 0;
		data_multi->sel_id = 0xff; /* default invalid */
	}

	lcd_tcon_data_list_add(data_multi->list_header, data_list);
	ret = lcd_tcon_data_multi_default_find(data_list->block_vaddr);
	if (ret == 0) {
		data_multi->sel_id = data_list->id;
		data_multi->list_cur = data_list;
	}

	data_multi->list_cnt++;

	return 0;
}

int lcd_tcon_data_multi_remvoe(struct tcon_mem_map_table_s *mm_table)
{
	struct tcon_data_multi_s *data_multi = NULL;
	int i;

	if (!mm_table->data_multi)
		return 0;
	if (mm_table->data_multi_cnt == 0)
		return 0;

	for (i = 0; i < mm_table->data_multi_cnt; i++) {
		data_multi = &mm_table->data_multi[i];
		data_multi->block_type = LCD_TCON_DATA_BLOCK_TYPE_NONE;
		data_multi->list_cnt = 0;
		data_multi->flag = 0xff;
		lcd_tcon_data_list_remove(data_multi->list_header);
		data_multi->list_header = NULL;
	}

	return 0;
}

int lcd_tcon_data_load(void)
{
	struct aml_lcd_drv_s *pdrv = aml_lcd_get_driver(0);
	unsigned char *table = tcon_mm_table.core_reg_table;
	struct file *filp = NULL;
	mm_segment_t old_fs = get_fs();
	loff_t pos = 0;
	char *str;
	struct lcd_tcon_data_block_header_s block_header;
	struct tcon_data_priority_s *data_prio;
	int i, j, data_cnt = 0, demura_cnt = 0;
	unsigned int n = 0, size, priority;
	unsigned int header_size = LCD_TCON_DATA_BLOCK_HEADER_SIZE;
	int ret;

	if (!table) {
		LCDERR("%s: no tcon bin table\n", __func__);
		return -1;
	}
	LCDPR("%s\n", __func__);

	if (tcon_mm_table.version == 0) {
		if (pdrv->data->chip_type == LCD_CHIP_TL1 ||
		    pdrv->data->chip_type == LCD_CHIP_TM2) {
			ret = lcd_tcon_vac_load();
			if (ret == 0)
				tcon_mm_table.valid_flag |= LCD_TCON_DATA_VALID_VAC;
			ret = lcd_tcon_demura_set_load();
			if (ret) {
				table[0x178] = 0x38;
				table[0x17c] = 0x20;
				table[0x181] = 0x00;
				table[0x23d] &= ~(1 << 0);
			} else {
				ret = lcd_tcon_demura_lut_load();
				if (ret)  {
					table[0x178] = 0x38;
					table[0x17c] = 0x20;
					table[0x181] = 0x00;
					table[0x23d] &= ~(1 << 0);
				} else {
					tcon_mm_table.valid_flag |= LCD_TCON_DATA_VALID_DEMURA;
				}
			}
		}

		ret = lcd_tcon_acc_lut_load();
		if (ret == 0)
			tcon_mm_table.valid_flag |= LCD_TCON_DATA_VALID_ACC;
	} else {
		if (!tcon_mm_table.data_mem_vaddr) {
			LCDERR("%s: data_mem error\n", __func__);
			return -1;
		}
		if (!tcon_mm_table.data_priority) {
			LCDERR("%s: data_priority error\n", __func__);
			return -1;
		}
		if (!tcon_mm_table.data_size) {
			LCDERR("%s: data_size error\n", __func__);
			return -1;
		}
		data_prio = tcon_mm_table.data_priority;

		for (i = 0; i < tcon_mm_table.block_cnt; i++) {
			n = 32 + i * 256;
			size = tcon_mm_table.data_size[i];
			str = (char *)&tcon_rmem.bin_path_rmem.mem_vaddr[n + 4];
			set_fs(KERNEL_DS);
			filp = filp_open(str, O_RDONLY, 0);
			if (IS_ERR(filp)) {
				LCDERR("%s: read %s error\n", __func__, str);
				set_fs(old_fs);
				return -1;
			}

			if (!tcon_mm_table.data_mem_vaddr[i]) {
				data_cnt = vfs_read(filp, (void *)&block_header,
						    header_size, &pos);
				if (data_cnt != header_size) {
					LCDERR("read block head failed, data_cnt: %d\n",
					       data_cnt);
					goto lcd_tcon_data_load_next;
				}

				size = block_header.block_size;
				tcon_mm_table.data_size[i] = size;
				tcon_mm_table.data_mem_vaddr[i] =
					kcalloc(size, sizeof(unsigned char), GFP_KERNEL);
				if (!tcon_mm_table.data_mem_vaddr[i]) {
					LCDERR("%s: Not enough memory\n", __func__);
					goto lcd_tcon_data_load_next;
				}
			}
			pos = 0; /* must reset pos for file read */
			data_cnt =
			vfs_read(filp, (char *)tcon_mm_table.data_mem_vaddr[i], size, &pos);
			if (data_cnt != size) {
				LCDERR("%s: data_cnt: %d, data_size: %d\n",
				       __func__, data_cnt, size);
				goto lcd_tcon_data_load_next;
			}
			memcpy((void *)&block_header, (void *)tcon_mm_table.data_mem_vaddr[i],
			       header_size);

			tcon_mm_table.valid_flag |= block_header.block_flag;
			if (block_header.block_flag == LCD_TCON_DATA_VALID_DEMURA)
				demura_cnt++;

			/* insertion sort for block data init_priority */
			data_prio[i].index = i;
			/*data_prio[i].priority = block_header.init_priority;*/
			/* update init_priority by index */
			priority = i;
			data_prio[i].priority = priority;
			if (i > 0) {
				j = i - 1;
				while (j >= 0) {
					if (priority > data_prio[j].priority)
						break;
					if (priority == data_prio[j].priority) {
						LCDERR("%s: block %d init_prio same as block %d\n",
						       __func__, data_prio[i].index,
						       data_prio[j].index);
						return -1;
					}
					data_prio[j + 1].index = data_prio[j].index;
					data_prio[j + 1].priority = data_prio[j].priority;
					j--;
				}
				data_prio[j + 1].index = i;
				data_prio[j + 1].priority = priority;
			}

			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("%s %d: size=0x%x, type=0x%02x, name=%s, init_priority=%d\n",
				      __func__, i,
				      block_header.block_size,
				      block_header.block_type,
				      block_header.name,
				      priority);
			}

lcd_tcon_data_load_next:
			vfs_fsync(filp, 0);
			filp_close(filp, NULL);
			set_fs(old_fs);
		}

		/* specially check demura setting */
		if (pdrv->data->chip_type == LCD_CHIP_TL1 ||
		    pdrv->data->chip_type == LCD_CHIP_TM2) {
			if (demura_cnt < 2) {
				tcon_mm_table.valid_flag &=
					~LCD_TCON_DATA_VALID_DEMURA;
				/* disable demura */
				table[0x178] = 0x38;
				table[0x17c] = 0x20;
				table[0x181] = 0x00;
				table[0x23d] &= ~(1 << 0);
			}
		}
	}

	tcon_mm_table.tcon_data_flag = 1;

	return 0;
}

static int lcd_tcon_bin_path_update(unsigned int size)
{
	unsigned char *mem_vaddr;
	unsigned int data_size, block_cnt;
	unsigned int data_crc32, temp_crc32;

	if (!tcon_rmem.bin_path_rmem.mem_vaddr) {
		LCDERR("%s: get mem error\n", __func__);
		return -1;
	}
	mem_vaddr = tcon_rmem.bin_path_rmem.mem_vaddr;
	data_size = mem_vaddr[4] |
		(mem_vaddr[5] << 8) |
		(mem_vaddr[6] << 16) |
		(mem_vaddr[7] << 24);
	if (data_size < 32) { /* header size */
		LCDERR("%s: tcon_bin_path data_size error\n", __func__);
		return -1;
	}
	block_cnt = mem_vaddr[16] |
		(mem_vaddr[17] << 8) |
		(mem_vaddr[18] << 16) |
		(mem_vaddr[19] << 24);
	if (block_cnt > 32) {
		LCDERR("%s: tcon_bin_path block_cnt error\n", __func__);
		return -1;
	}
	data_crc32 = mem_vaddr[0] |
		(mem_vaddr[1] << 8) |
		(mem_vaddr[2] << 16) |
		(mem_vaddr[3] << 24);
	temp_crc32 = cal_crc32(0, &mem_vaddr[4], (data_size - 4));
	if (data_crc32 != temp_crc32) {
		LCDERR("%s: tcon_bin_path data crc error\n", __func__);
		return -1;
	}

	tcon_mm_table.version = mem_vaddr[8] |
		(mem_vaddr[9] << 8) |
		(mem_vaddr[10] << 16) |
		(mem_vaddr[11] << 24);
	tcon_mm_table.data_load_level = mem_vaddr[12] |
		(mem_vaddr[13] << 8) |
		(mem_vaddr[14] << 16) |
		(mem_vaddr[15] << 24);
	tcon_mm_table.block_cnt = block_cnt;

	return 0;
}

static int lcd_tcon_mm_table_config_v0(void)
{
	unsigned int max_size;

	max_size = lcd_tcon_conf->axi_mem_size +
		lcd_tcon_conf->bin_path_size +
		lcd_tcon_conf->vac_size +
		lcd_tcon_conf->demura_set_size +
		lcd_tcon_conf->demura_lut_size +
		lcd_tcon_conf->acc_lut_size;
	if (tcon_rmem.rsv_mem_size < max_size) {
		LCDERR("%s: tcon mem size 0x%x is not enough, need 0x%x\n",
			__func__, tcon_rmem.rsv_mem_size, max_size);
		return -1;
	}

	if (tcon_mm_table.block_cnt != 4) {
		LCDERR("%s: tcon data block_cnt %d invalid\n",
		       __func__, tcon_mm_table.block_cnt);
		return -1;
	}

	tcon_rmem.vac_rmem.mem_size = lcd_tcon_conf->vac_size;
	tcon_rmem.vac_rmem.mem_paddr = tcon_rmem.bin_path_rmem.mem_paddr +
			tcon_rmem.bin_path_rmem.mem_size;
	tcon_rmem.vac_rmem.mem_vaddr =
		lcd_tcon_paddrtovaddr(tcon_rmem.vac_rmem.mem_paddr,
				      tcon_rmem.vac_rmem.mem_size);
	if ((lcd_debug_print_flag & LCD_DBG_PR_NORMAL) &&
	    tcon_rmem.vac_rmem.mem_size > 0)
		LCDPR("vac_mem paddr: 0x%08x, vaddr: 0x%p, size: 0x%x\n",
		      (unsigned int)tcon_rmem.vac_rmem.mem_paddr,
		      tcon_rmem.vac_rmem.mem_vaddr,
		      tcon_rmem.vac_rmem.mem_size);

	tcon_rmem.demura_set_rmem.mem_size = lcd_tcon_conf->demura_set_size;
	tcon_rmem.demura_set_rmem.mem_paddr = tcon_rmem.vac_rmem.mem_paddr +
			tcon_rmem.vac_rmem.mem_size;
	tcon_rmem.demura_set_rmem.mem_vaddr =
		lcd_tcon_paddrtovaddr(tcon_rmem.demura_set_rmem.mem_paddr,
				      tcon_rmem.demura_set_rmem.mem_size);
	if ((lcd_debug_print_flag & LCD_DBG_PR_NORMAL) &&
	    tcon_rmem.demura_set_rmem.mem_size > 0)
		LCDPR("demura_set_mem paddr: 0x%08x, vaddr: 0x%p, size: 0x%x\n",
		      (unsigned int)tcon_rmem.demura_set_rmem.mem_paddr,
		      tcon_rmem.demura_set_rmem.mem_vaddr,
		      tcon_rmem.demura_set_rmem.mem_size);

	tcon_rmem.demura_lut_rmem.mem_size = lcd_tcon_conf->demura_lut_size;
	tcon_rmem.demura_lut_rmem.mem_paddr =
			tcon_rmem.demura_set_rmem.mem_paddr +
			tcon_rmem.demura_set_rmem.mem_size;
	tcon_rmem.demura_lut_rmem.mem_vaddr =
		lcd_tcon_paddrtovaddr(tcon_rmem.demura_lut_rmem.mem_paddr,
				      tcon_rmem.demura_lut_rmem.mem_size);
	if ((lcd_debug_print_flag & LCD_DBG_PR_NORMAL) &&
	    tcon_rmem.demura_lut_rmem.mem_size > 0)
		LCDPR("demura_lut_mem paddr: 0x%08x, vaddr: 0x%p, size: 0x%x\n",
		      (unsigned int)tcon_rmem.demura_lut_rmem.mem_paddr,
		      tcon_rmem.demura_lut_rmem.mem_vaddr,
		      tcon_rmem.demura_lut_rmem.mem_size);

	tcon_rmem.acc_lut_rmem.mem_size = lcd_tcon_conf->acc_lut_size;
	tcon_rmem.acc_lut_rmem.mem_paddr = tcon_rmem.demura_lut_rmem.mem_paddr +
			tcon_rmem.demura_lut_rmem.mem_size;
	tcon_rmem.acc_lut_rmem.mem_vaddr =
		lcd_tcon_paddrtovaddr(tcon_rmem.acc_lut_rmem.mem_paddr,
				      tcon_rmem.acc_lut_rmem.mem_size);
	if ((lcd_debug_print_flag & LCD_DBG_PR_NORMAL) &&
	    tcon_rmem.acc_lut_rmem.mem_size > 0)
		LCDPR("acc_lut_mem paddr: 0x%08x, vaddr: 0x%p, size: 0x%x\n",
		      (unsigned int)tcon_rmem.acc_lut_rmem.mem_paddr,
		      tcon_rmem.acc_lut_rmem.mem_vaddr,
		      tcon_rmem.acc_lut_rmem.mem_size);

	return 0;
}

static int lcd_tcon_mm_table_config_v1(void)
{
	unsigned char *mem_vaddr;
	unsigned int cnt, data_size, n, i;

	if (tcon_mm_table.block_cnt > 32) {
		LCDERR("%s: tcon data block_cnt %d invalid\n",
		       __func__, tcon_mm_table.block_cnt);
		return -1;
	}

	if (tcon_mm_table.data_mem_vaddr)
		return 0;
	if (tcon_mm_table.block_cnt == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: block_cnt is zero\n", __func__);
		return 0;
	}

	cnt = tcon_mm_table.block_cnt;
	tcon_mm_table.data_mem_vaddr = kcalloc(cnt, sizeof(unsigned char *), GFP_KERNEL);
	if (!tcon_mm_table.data_mem_vaddr)
		return -1;

	tcon_mm_table.data_priority =
		kcalloc(cnt, sizeof(struct tcon_data_priority_s), GFP_KERNEL);
	if (!tcon_mm_table.data_priority)
		return -1;

	memset(tcon_mm_table.data_priority, 0xff, cnt * sizeof(struct tcon_data_priority_s));

	tcon_mm_table.data_size = kcalloc(cnt, sizeof(unsigned int), GFP_KERNEL);
	if (!tcon_mm_table.data_size)
		return -1;

	tcon_mm_table.data_multi = kcalloc(cnt, sizeof(struct tcon_data_multi_s), GFP_KERNEL);
	if (!tcon_mm_table.data_multi)
		return -1;

	mem_vaddr = tcon_rmem.bin_path_rmem.mem_vaddr;
	for (i = 0; i < tcon_mm_table.block_cnt; i++) {
		n = 32 + (i * 256);
		data_size = mem_vaddr[n] |
			(mem_vaddr[n + 1] << 8) |
			(mem_vaddr[n + 2] << 16) |
			(mem_vaddr[n + 3] << 24);
		if (data_size == 0) {
			LCDERR("%s: block[%d] size is zero\n", __func__, i);
			continue;
		}
		tcon_mm_table.data_mem_vaddr[i] =
			kcalloc(data_size, sizeof(unsigned char), GFP_KERNEL);
		if (!tcon_mm_table.data_mem_vaddr[i])
			continue;
		tcon_mm_table.data_size[i] = data_size;
	}

	return 0;
}

static void lcd_tcon_axi_mem_config_tl1(void)
{
	unsigned int size[3] = {4162560, 4162560, 1960440};
	unsigned int total_size = 0, temp_size = 0;
	int i;

	for (i = 0; i < lcd_tcon_conf->axi_bank; i++)
		total_size += size[i];
	if (total_size > tcon_rmem.axi_mem_size) {
		LCDERR("%s: tcon axi_mem size 0x%x is not enough, need 0x%x\n",
			__func__, tcon_rmem.axi_mem_size, total_size);
		return;
	}

	tcon_rmem.axi_rmem =
		kcalloc(lcd_tcon_conf->axi_bank,
			sizeof(struct tcon_rmem_config_s), GFP_KERNEL);
	if (!tcon_rmem.axi_rmem)
		return;

	for (i = 0; i < lcd_tcon_conf->axi_bank; i++) {
		tcon_rmem.axi_rmem[i].mem_paddr =
			tcon_rmem.axi_mem_paddr + temp_size;
		tcon_rmem.axi_rmem[i].mem_vaddr = NULL;
		tcon_rmem.axi_rmem[i].mem_size = size[i];
		temp_size += size[i];
	}
}

static void lcd_tcon_axi_mem_config_t5(void)
{
	unsigned int size[2] = {0x00800000, 0x200000};
	unsigned int reg[2] = {0x261, 0x1a9};
	unsigned int total_size = 0, temp_size = 0;
	int i;

	for (i = 0; i < lcd_tcon_conf->axi_bank; i++)
		total_size += size[i];
	if (total_size > tcon_rmem.axi_mem_size) {
		LCDERR("%s: tcon axi_mem size 0x%x is not enough, need 0x%x\n",
			__func__, tcon_rmem.axi_mem_size, total_size);
		return;
	}

	tcon_rmem.axi_rmem =
		kcalloc(lcd_tcon_conf->axi_bank,
			sizeof(struct tcon_rmem_config_s), GFP_KERNEL);
	if (!tcon_rmem.axi_rmem)
		return;

	lcd_tcon_conf->axi_reg = kcalloc(lcd_tcon_conf->axi_bank,
					 sizeof(unsigned int), GFP_KERNEL);
	if (!lcd_tcon_conf->axi_reg) {
		kfree(tcon_rmem.axi_rmem);
		return;
	}

	temp_size = 0;
	for (i = 0; i < lcd_tcon_conf->axi_bank; i++) {
		tcon_rmem.axi_rmem[i].mem_paddr =
			tcon_rmem.axi_mem_paddr + temp_size;
		tcon_rmem.axi_rmem[i].mem_vaddr = NULL;
		tcon_rmem.axi_rmem[i].mem_size = size[i];
		temp_size += size[i];

		lcd_tcon_conf->axi_reg[i] = reg[i];
	}
}

static void lcd_tcon_axi_mem_config_t5d(void)
{
	unsigned int size = 0x00500000;
	unsigned int reg = 0x261;

	if (size > tcon_rmem.axi_mem_size) {
		LCDERR("%s: tcon axi_mem size 0x%x is not enough, need 0x%x\n",
			__func__, tcon_rmem.axi_mem_size, size);
		return;
	}

	tcon_rmem.axi_rmem =
		kzalloc(sizeof(struct tcon_rmem_config_s), GFP_KERNEL);
	if (!tcon_rmem.axi_rmem)
		return;

	lcd_tcon_conf->axi_reg = kzalloc(sizeof(unsigned int), GFP_KERNEL);
	if (!lcd_tcon_conf->axi_reg) {
		kfree(tcon_rmem.axi_rmem);
		return;
	}

	tcon_rmem.axi_rmem->mem_paddr = tcon_rmem.axi_mem_paddr;
	tcon_rmem.axi_rmem->mem_vaddr = NULL;
	tcon_rmem.axi_rmem->mem_size = size;

	*lcd_tcon_conf->axi_reg = reg;
}

static void lcd_tcon_axi_mem_secure_tl1(void)
{
#ifdef CONFIG_AMLOGIC_TEE_TODO
	int ret;

	tcon_local_cfg.secure_cfg.handle = 0;
	ret = tee_protect_mem_by_type(TEE_MEM_TYPE_TCON,
				      tcon_rmem.axi_mem_paddr,
				      tcon_rmem.axi_mem_size,
				      &tcon_local_cfg.secure_cfg.handle);
	if (ret) {
		tcon_local_cfg.secure_cfg.protect = 0;
		LCDERR("%s: the tcon secure mem failed! protect status is:%d\n",
		       __func__, tcon_local_cfg.secure_cfg.protect);
	} else {
		tcon_local_cfg.secure_cfg.protect = 1;
	}
#endif
}

static void lcd_tcon_axi_mem_secure_t5(void)
{
	/* only protect od mem */
#ifdef CONFIG_AMLOGIC_TEE_TODO
	int ret;

	if (!tcon_rmem.axi_rmem)
		return;

	tcon_local_cfg.secure_cfg.handle = 0;
	ret = tee_protect_mem_by_type(TEE_MEM_TYPE_TCON,
				      tcon_rmem.axi_rmem[0].mem_paddr,
				      tcon_rmem.axi_rmem[0].mem_size,
				      &tcon_local_cfg.secure_cfg.handle);
	if (ret) {
		tcon_local_cfg.secure_cfg.protect = 0;
		LCDERR("%s: the tcon secure mem failed! protect status is:%d\n",
		       __func__, tcon_local_cfg.secure_cfg.protect);
	} else {
		tcon_local_cfg.secure_cfg.protect = 1;
	}
#endif
}

static int lcd_tcon_mem_config(void)
{
	unsigned int max_size;
	int ret;

	max_size = lcd_tcon_conf->axi_mem_size + lcd_tcon_conf->bin_path_size;

	if (tcon_rmem.rsv_mem_size < max_size) {
		LCDERR("%s: tcon mem size 0x%x is not enough, need 0x%x\n",
		       __func__, tcon_rmem.rsv_mem_size, max_size);
		return -1;
	}

	tcon_rmem.axi_mem_size = lcd_tcon_conf->axi_mem_size;
	tcon_rmem.axi_mem_paddr = tcon_rmem.rsv_mem_paddr;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s: axi_mem paddr: 0x%08x, size: 0x%x\n",
		      __func__, (unsigned int)tcon_rmem.axi_mem_paddr,
		      tcon_rmem.axi_mem_size);
	if (lcd_tcon_conf->tcon_axi_mem_config)
		lcd_tcon_conf->tcon_axi_mem_config();
	if (lcd_tcon_conf->tcon_axi_mem_secure)
		lcd_tcon_conf->tcon_axi_mem_secure();

	tcon_rmem.bin_path_rmem.mem_size = lcd_tcon_conf->bin_path_size;
	tcon_rmem.bin_path_rmem.mem_paddr =
		tcon_rmem.axi_mem_paddr + tcon_rmem.axi_mem_size;
	tcon_rmem.bin_path_rmem.mem_vaddr =
		lcd_tcon_paddrtovaddr(tcon_rmem.bin_path_rmem.mem_paddr,
				      tcon_rmem.bin_path_rmem.mem_size);
	if ((lcd_debug_print_flag & LCD_DBG_PR_NORMAL) &&
	    tcon_rmem.bin_path_rmem.mem_size > 0)
		LCDPR("tcon bin_path paddr: 0x%08x, vaddr: 0x%p, size: 0x%x\n",
		      (unsigned int)tcon_rmem.bin_path_rmem.mem_paddr,
		      tcon_rmem.bin_path_rmem.mem_vaddr,
		      tcon_rmem.bin_path_rmem.mem_size);

	ret = lcd_tcon_bin_path_update(tcon_rmem.bin_path_rmem.mem_size);
	if (ret)
		return -1;

	if (tcon_mm_table.version == 0)
		ret = lcd_tcon_mm_table_config_v0();
	else
		ret = lcd_tcon_mm_table_config_v1();
	return ret;
}

#ifdef CONFIG_AMLOGIC_TEE_TODO
int lcd_tcon_mem_tee_get_status(void)
{
	return tcon_local_cfg.secure_cfg.protect;
}

int lcd_tcon_mem_tee_unprotect(void)
{
	if (tcon_local_cfg.secure_cfg.protect) {
		tee_unprotect_mem(tcon_local_cfg.secure_cfg.handle);
		tcon_local_cfg.secure_cfg.protect = 0;
	}
	return tcon_local_cfg.secure_cfg.protect;
}
#endif

#ifdef CONFIG_CMA
static int lcd_tcon_cma_mem_config(struct cma *cma)
{
	struct aml_lcd_drv_s *pdrv = aml_lcd_get_driver(0);
	unsigned int mem_size;

	tcon_rmem.rsv_mem_paddr = cma_get_base(cma);
	LCDPR("tcon resv_mem base:0x%lx, size:0x%lx\n",
	      (unsigned long)tcon_rmem.rsv_mem_paddr,
	      cma_get_size(cma));
	mem_size = lcd_tcon_conf->rsv_mem_size;
	if (cma_get_size(cma) < mem_size) {
		tcon_rmem.flag = 0;
	} else {
		tcon_rmem.rsv_mem_vaddr =
			dma_alloc_from_contiguous(pdrv->dev, (mem_size >> PAGE_SHIFT), 0, 0);
		if (!tcon_rmem.rsv_mem_vaddr) {
			LCDERR("tcon resv_mem alloc failed\n");
		} else {
			LCDPR("tcon resv_mem dma_alloc=0x%x\n", mem_size);
			tcon_rmem.rsv_mem_size = mem_size;
			tcon_rmem.flag = 2; /* cma memory */
		}
	}

	return 0;
}
#endif

static irqreturn_t lcd_tcon_isr(int irq, void *dev_id)
{
	struct aml_lcd_drv_s *pdrv = aml_lcd_get_driver(0);
	unsigned int temp;

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return IRQ_HANDLED;

	temp = lcd_tcon_read(pdrv, TCON_INTR_RO);
	if (temp & 0x2) {
		LCDPR("%s: tcon sw_reset triggered\n", __func__);
		lcd_tcon_core_update(pdrv);
	}
	if (temp & 0x40)
		LCDPR("%s: tcon ddr interface error triggered\n", __func__);

	return IRQ_HANDLED;
}

static void lcd_tcon_intr_init(struct aml_lcd_drv_s *pdrv)
{
	unsigned int tcon_irq = 0;

	if (!pdrv->res_tcon_irq) {
		LCDERR("res_tcon_irq is null\n");
		return;
	}
	tcon_irq = pdrv->res_tcon_irq->start;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("tcon_irq: %d\n", tcon_irq);

	if (request_irq(tcon_irq, lcd_tcon_isr, IRQF_SHARED,
		"lcd_tcon", (void *)"lcd_tcon")) {
		LCDERR("can't request lcd_tcon irq\n");
	} else {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("request lcd_tcon irq successful\n");
	}

	lcd_tcon_write(pdrv, TCON_INTR_MASKN, TCON_INTR_MASKN_VAL);
}

static int lcd_tcon_load_init_data_from_unifykey(void)
{
	int key_len, data_len, ret;

	data_len = tcon_mm_table.core_reg_table_size;
	tcon_mm_table.core_reg_table =
		kcalloc(data_len, sizeof(unsigned char), GFP_KERNEL);
	if (!tcon_mm_table.core_reg_table)
		return -1;
	key_len = data_len;
	ret = lcd_unifykey_get_no_header("lcd_tcon", tcon_mm_table.core_reg_table, &key_len);
	if (ret)
		goto lcd_tcon_load_init_data_err;
	if (key_len != data_len)
		goto lcd_tcon_load_init_data_err;

	memset(tcon_local_cfg.bin_ver, 0, TCON_BIN_VER_LEN);
	LCDPR("tcon: load init data len: %d\n", data_len);
	return 0;

lcd_tcon_load_init_data_err:
	kfree(tcon_mm_table.core_reg_table);
	tcon_mm_table.core_reg_table = NULL;
	LCDERR("%s: !!!!!!tcon unifykey load error!!!!!!\n", __func__);
	return -1;
}

static int lcd_tcon_load_init_data_from_unifykey_new(void)
{
	int key_len, data_len;
	unsigned char *buf, *p;
	struct lcd_tcon_init_block_header_s *data_header;
	int ret;

	data_len = tcon_mm_table.core_reg_table_size +
		LCD_TCON_DATA_BLOCK_HEADER_SIZE;
	buf = kcalloc(data_len, sizeof(unsigned char), GFP_KERNEL);
	if (!buf)
		return -1;

	key_len = data_len;
	ret = lcd_unifykey_get_tcon("lcd_tcon", buf, &key_len);
	if (ret)
		goto lcd_tcon_load_init_data_new_err;
	if (key_len != data_len)
		goto lcd_tcon_load_init_data_new_err;

	data_header = (struct lcd_tcon_init_block_header_s *)buf;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("unifykey header:\n");
		LCDPR("crc32             = 0x%08x\n", data_header->crc32);
		LCDPR("block_size        = %d\n", data_header->block_size);
		LCDPR("chipid            = %d\n", data_header->chipid);
		LCDPR("name              = %s\n", data_header->name);
	}
	memcpy(tcon_local_cfg.bin_ver, data_header->version,
	       LCD_TCON_INIT_BIN_VERSION_SIZE);
	tcon_local_cfg.bin_ver[TCON_BIN_VER_LEN - 1] = '\0';

	data_len = tcon_mm_table.core_reg_table_size;
	tcon_mm_table.core_reg_table = kcalloc(data_len, sizeof(unsigned char), GFP_KERNEL);
	if (!tcon_mm_table.core_reg_table)
		goto lcd_tcon_load_init_data_new_err;
	p = buf + LCD_TCON_DATA_BLOCK_HEADER_SIZE;
	memcpy(tcon_mm_table.core_reg_table, p, data_len);
	kfree(buf);

	LCDPR("tcon: load init data len: %d, ver: %s\n",
	      data_len, tcon_local_cfg.bin_ver);
	return 0;

lcd_tcon_load_init_data_new_err:
	kfree(buf);
	LCDERR("%s: !!!!!!tcon unifykey load error!!!!!!\n", __func__);
	return -1;
}

static int lcd_tcon_get_config(struct aml_lcd_drv_s *pdrv)
{
	tcon_mm_table.core_reg_table_size = lcd_tcon_conf->reg_table_len;
	if (lcd_tcon_conf->core_reg_ver)
		lcd_tcon_load_init_data_from_unifykey_new();
	else
		lcd_tcon_load_init_data_from_unifykey();

	if (tcon_mm_table.tcon_data_flag == 0)
		lcd_tcon_data_load();
	pdrv->tcon_status = tcon_mm_table.valid_flag;

	lcd_tcon_intr_init(pdrv);

	return 0;
}

static void lcd_tcon_get_config_work(struct work_struct *work)
{
	struct aml_lcd_drv_s *pdrv;
	bool is_init;
	int i = 0;

	pdrv = container_of(work, struct aml_lcd_drv_s, tcon_config_work);

	is_init = lcd_unifykey_init_get();
	while (!is_init) {
		if (i++ >= LCD_UNIFYKEY_WAIT_TIMEOUT)
			break;
		msleep(LCD_UNIFYKEY_RETRY_INTERVAL);
		is_init = lcd_unifykey_init_get();
	}
	if (is_init)
		lcd_tcon_get_config(pdrv);
	else
		LCDPR("tcon: key_init_flag=%d, i=%d\n", is_init, i);
}

/* **********************************
 * tcon match data
 * **********************************
 */
static struct lcd_tcon_config_s tcon_data_tl1 = {
	.tcon_valid = 0,

	.core_reg_ver = 0,
	.core_reg_width = LCD_TCON_CORE_REG_WIDTH_TL1,
	.reg_table_width = LCD_TCON_TABLE_WIDTH_TL1,
	.reg_table_len = LCD_TCON_TABLE_LEN_TL1,
	.core_reg_start = TCON_CORE_REG_START_TL1,
	.top_reg_base = TCON_TOP_BASE,

	.reg_top_ctrl = TCON_TOP_CTRL,
	.bit_en = BIT_TOP_EN_TL1,

	.reg_core_od = REG_CORE_OD_TL1,
	.bit_od_en = BIT_OD_EN_TL1,

	.reg_ctrl_timing_base = REG_LCD_TCON_MAX,
	.ctrl_timing_offset = CTRL_TIMING_OFFSET_TL1,
	.ctrl_timing_cnt = CTRL_TIMING_CNT_TL1,

	.axi_bank = LCD_TCON_AXI_BANK_TL1,

	/*rsv_mem(12M)    axi_mem(10M)   bin_path(10K)
	 *             |----------------|-------------|
	 */
	.rsv_mem_size    = 0x00c00000,
	.axi_mem_size    = 0x00a00000,
	.bin_path_size   = 0x00002800,
	.vac_size        = 0x00002000, /* 8k */
	.demura_set_size = 0x00001000, /* 4k */
	.demura_lut_size = 0x00120000, /* 1152k */
	.acc_lut_size    = 0x00001000, /* 4k */

	.axi_reg = NULL,
	.tcon_axi_mem_config = lcd_tcon_axi_mem_config_tl1,
	.tcon_axi_mem_secure = lcd_tcon_axi_mem_secure_tl1,
	.tcon_global_reset = NULL,
	.tcon_gamma_pattern = lcd_tcon_gamma_pattern_tl1,
	.tcon_enable = lcd_tcon_enable_tl1,
	.tcon_disable = lcd_tcon_disable_tl1,
};

static struct lcd_tcon_config_s tcon_data_t5 = {
	.tcon_valid = 0,

	.core_reg_ver = 1, /* new version with header */
	.core_reg_width = LCD_TCON_CORE_REG_WIDTH_T5,
	.reg_table_width = LCD_TCON_TABLE_WIDTH_T5,
	.reg_table_len = LCD_TCON_TABLE_LEN_T5,
	.core_reg_start = TCON_CORE_REG_START_T5,
	.top_reg_base = TCON_TOP_BASE,

	.reg_top_ctrl = REG_LCD_TCON_MAX,
	.bit_en = BIT_TOP_EN_T5,

	.reg_core_od = REG_CORE_OD_T5,
	.bit_od_en = BIT_OD_EN_T5,

	.reg_ctrl_timing_base = REG_LCD_TCON_MAX,
	.ctrl_timing_offset = CTRL_TIMING_OFFSET_T5,
	.ctrl_timing_cnt = CTRL_TIMING_CNT_T5,

	.axi_bank = LCD_TCON_AXI_BANK_T5,

	/*rsv_mem(12M)    axi_mem(10M)   bin_path(10K)
	 *             |----------------|-------------|
	 */
	.rsv_mem_size    = 0x00c00000,
	.axi_mem_size    = 0x00a00000,
	.bin_path_size   = 0x00002800,
	.vac_size        = 0,
	.demura_set_size = 0,
	.demura_lut_size = 0,
	.acc_lut_size    = 0,

	.axi_reg = NULL,
	.tcon_axi_mem_config = lcd_tcon_axi_mem_config_t5,
	.tcon_axi_mem_secure = lcd_tcon_axi_mem_secure_t5,
	.tcon_global_reset = lcd_tcon_global_reset_t5,
	.tcon_gamma_pattern = lcd_tcon_gamma_pattern_t5,
	.tcon_enable = lcd_tcon_enable_t5,
	.tcon_disable = lcd_tcon_disable_t5,
};

static struct lcd_tcon_config_s tcon_data_t5d = {
	.tcon_valid = 0,

	.core_reg_ver = 1, /* new version with header */
	.core_reg_width = LCD_TCON_CORE_REG_WIDTH_T5D,
	.reg_table_width = LCD_TCON_TABLE_WIDTH_T5D,
	.reg_table_len = LCD_TCON_TABLE_LEN_T5D,
	.core_reg_start = TCON_CORE_REG_START_T5D,

	.reg_top_ctrl = REG_LCD_TCON_MAX,
	.bit_en = BIT_TOP_EN_T5D,

	.reg_core_od = REG_CORE_OD_T5D,
	.bit_od_en = BIT_OD_EN_T5D,

	.reg_ctrl_timing_base = REG_LCD_TCON_MAX,
	.ctrl_timing_offset = CTRL_TIMING_OFFSET_T5D,
	.ctrl_timing_cnt = CTRL_TIMING_CNT_T5D,

	.axi_bank = LCD_TCON_AXI_BANK_T5D,

	.rsv_mem_size    = 0x00800000, /* 8M */
	.axi_mem_size    = 0x00500000, /* 5M */
	.bin_path_size   = 0x00002800, /* 10K */
	.vac_size        = 0,
	.demura_set_size = 0,
	.demura_lut_size = 0,
	.acc_lut_size    = 0,

	.axi_reg = NULL,
	.tcon_axi_mem_config = lcd_tcon_axi_mem_config_t5d,
	.tcon_axi_mem_secure = lcd_tcon_axi_mem_secure_t5,
	.tcon_global_reset = lcd_tcon_global_reset_t5,
	.tcon_gamma_pattern = lcd_tcon_gamma_pattern_t5,
	.tcon_enable = lcd_tcon_enable_t5,
	.tcon_disable = lcd_tcon_disable_t5,
};

static struct lcd_tcon_config_s tcon_data_t3 = {
	.tcon_valid = 0,

	.core_reg_ver = 1, /* new version with header */
	.core_reg_width = LCD_TCON_CORE_REG_WIDTH_T5,
	.reg_table_width = LCD_TCON_TABLE_WIDTH_T5,
	.reg_table_len = LCD_TCON_TABLE_LEN_T5,
	.core_reg_start = TCON_CORE_REG_START_T5,
	.top_reg_base = TCON_TOP_BASE,

	.reg_top_ctrl = REG_LCD_TCON_MAX,
	.bit_en = BIT_TOP_EN_T5,

	.reg_core_od = REG_CORE_OD_T5,
	.bit_od_en = BIT_OD_EN_T5,

	.reg_ctrl_timing_base = REG_LCD_TCON_MAX,
	.ctrl_timing_offset = CTRL_TIMING_OFFSET_T5,
	.ctrl_timing_cnt = CTRL_TIMING_CNT_T5,

	.axi_bank = LCD_TCON_AXI_BANK_T5,

	/*rsv_mem(12M)    axi_mem(10M)   bin_path(10K)
	 *             |----------------|-------------|
	 */
	.rsv_mem_size    = 0x00c00000,
	.axi_mem_size    = 0x00a00000,
	.bin_path_size   = 0x00002800,
	.vac_size        = 0,
	.demura_set_size = 0,
	.demura_lut_size = 0,
	.acc_lut_size    = 0,

	.axi_reg = NULL,
	.tcon_axi_mem_config = lcd_tcon_axi_mem_config_t5,
	.tcon_axi_mem_secure = lcd_tcon_axi_mem_secure_t5,
	.tcon_global_reset = lcd_tcon_global_reset_t3,
	.tcon_gamma_pattern = lcd_tcon_gamma_pattern_t5,
	.tcon_enable = lcd_tcon_enable_t5,
	.tcon_disable = lcd_tcon_disable_t3,
};

int lcd_tcon_probe(struct aml_lcd_drv_s *pdrv)
{
	struct cma *cma;
	unsigned int mem_size;
	int ret = 0;

	lcd_tcon_conf = NULL;
	switch (pdrv->data->chip_type) {
	case LCD_CHIP_TL1:
	case LCD_CHIP_TM2:
		lcd_tcon_conf = &tcon_data_tl1;
		break;
	case LCD_CHIP_T5:
		lcd_tcon_conf = &tcon_data_t5;
		break;
	case LCD_CHIP_T5D:
		lcd_tcon_conf = &tcon_data_t5d;
		break;
	case LCD_CHIP_T3:
		lcd_tcon_conf = &tcon_data_t3;
		break;
	default:
		break;
	}
	if (!lcd_tcon_conf)
		return 0;

	switch (pdrv->config.basic.lcd_type) {
	case LCD_MLVDS:
		lcd_tcon_conf->tcon_valid = 1;
		break;
	case LCD_P2P:
		if (pdrv->data->chip_type == LCD_CHIP_T5D)
			lcd_tcon_conf->tcon_valid = 0;
		else
			lcd_tcon_conf->tcon_valid = 1;
		break;
	default:
		break;
	}
	if (lcd_tcon_conf->tcon_valid == 0)
		return 0;

	/* init reserved memory */
	ret = of_reserved_mem_device_init(pdrv->dev);
	if (ret) {
		LCDERR("tcon: init reserved memory failed\n");
	} else {
		if (!(void *)tcon_rmem.rsv_mem_paddr) {
#ifdef CONFIG_CMA
			cma = dev_get_cma_area(pdrv->dev);
			if (cma) {
				lcd_tcon_cma_mem_config(cma);
				lcd_tcon_mem_config();
			} else {
				LCDERR("tcon: NO CMA\n");
			}
#else
			LCDERR("tcon axi_mem alloc failed\n");
#endif
		} else {
			mem_size = tcon_rmem.rsv_mem_size;
			if (mem_size < lcd_tcon_conf->rsv_mem_size) {
				tcon_rmem.flag = 0;
			} else {
				tcon_rmem.flag = 1; /* reserved memory */
				LCDPR("tcon resv_mem base:0x%lx, size:0x%x\n",
				      (unsigned long)tcon_rmem.rsv_mem_paddr,
				      mem_size);
				lcd_tcon_mem_config();
			}
		}
	}

	INIT_WORK(&pdrv->tcon_config_work, lcd_tcon_get_config_work);

	if (lcd_unifykey_init_get())
		ret = lcd_tcon_get_config(pdrv);
	else
		lcd_queue_work(&pdrv->tcon_config_work);

	return ret;
}

int lcd_tcon_remove(struct aml_lcd_drv_s *pdrv)
{
	int i;

	kfree(tcon_mm_table.core_reg_table);
	tcon_mm_table.core_reg_table = NULL;
	if (tcon_mm_table.version) {
		if (tcon_mm_table.data_mem_vaddr) {
			for (i = 0; i < tcon_mm_table.block_cnt; i++) {
				if (!tcon_mm_table.data_mem_vaddr[i])
					continue;
				kfree(tcon_mm_table.data_mem_vaddr[i]);
				tcon_mm_table.data_mem_vaddr[i] = NULL;
			}
			kfree(tcon_mm_table.data_mem_vaddr);
			tcon_mm_table.data_mem_vaddr = NULL;
		}
	} else {
		lcd_unmap_phyaddr(tcon_rmem.vac_rmem.mem_vaddr);
		lcd_unmap_phyaddr(tcon_rmem.demura_set_rmem.mem_vaddr);
		lcd_unmap_phyaddr(tcon_rmem.demura_lut_rmem.mem_vaddr);
		lcd_unmap_phyaddr(tcon_rmem.acc_lut_rmem.mem_vaddr);
		tcon_rmem.vac_rmem.mem_vaddr = NULL;
		tcon_rmem.demura_set_rmem.mem_vaddr = NULL;
		tcon_rmem.demura_lut_rmem.mem_vaddr = NULL;
		tcon_rmem.acc_lut_rmem.mem_vaddr = NULL;
	}

	if (tcon_rmem.flag == 2) {
		LCDPR("tcon free memory: base:0x%lx, size:0x%x\n",
			(unsigned long)tcon_rmem.rsv_mem_paddr,
			tcon_rmem.rsv_mem_size);
#ifdef CONFIG_CMA
		dma_release_from_contiguous(pdrv->dev,
			tcon_rmem.rsv_mem_vaddr,
			tcon_rmem.rsv_mem_size >> PAGE_SHIFT);
#endif
	}

	if (lcd_tcon_conf) {
		/* lcd_tcon_conf == NULL; */
		lcd_tcon_conf->tcon_valid = 0;
	}

	return 0;
}

static int __init tcon_buf_device_init(struct reserved_mem *rmem, struct device *dev)
{
	return 0;
}

static const struct reserved_mem_ops tcon_buf_ops = {
	.device_init = tcon_buf_device_init,
};

static int __init tcon_buf_setup(struct reserved_mem *rmem)
{
	tcon_rmem.rsv_mem_paddr = rmem->base;
	tcon_rmem.rsv_mem_size = rmem->size;
	rmem->ops = &tcon_buf_ops;
	LCDPR("tcon: Reserved memory: created buf at 0x%lx, size %ld MiB\n",
	      (unsigned long)rmem->base, (unsigned long)rmem->size / SZ_1M);
	return 0;
}

RESERVEDMEM_OF_DECLARE(buf, "amlogic, lcd_tcon-memory", tcon_buf_setup);

