/*
 * drivers/amlogic/media/vout/lcd/lcd_tcon.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
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
#include "lcd_common.h"
#include "lcd_reg.h"
#include "lcd_tcon.h"
/*#include "tcon_ceds.h"*/

#define TCON_INTR_MASKN_VAL    0x0  /* default mask all */

static struct tcon_rmem_s tcon_rmem = {
	.flag = 0,
	.mem_paddr = 0,
	.mem_size = 0,
	.core_mem_paddr = 0,
	.core_mem_size = 0,
	.vac_mem_paddr = 0,
	.vac_mem_vaddr = NULL,
	.vac_mem_size = 0,
	.demura_set_paddr = 0,
	.demura_set_vaddr = NULL,
	.demura_set_mem_size = 0,
	.demura_lut_paddr = 0,
	.demura_lut_vaddr = NULL,
	.demura_lut_mem_size = 0,
	.vac_valid = 0,
	.demura_valid = 0,
};

static struct lcd_tcon_data_s *lcd_tcon_data;
static struct delayed_work lcd_tcon_delayed_work;

static int lcd_tcon_valid_check(void)
{
	if (lcd_tcon_data == NULL) {
		LCDERR("invalid tcon data\n");
		return -1;
	}
	if (lcd_tcon_data->tcon_valid == 0) {
		LCDERR("invalid tcon\n");
		return -1;
	}

	return 0;
}

unsigned int lcd_tcon_reg_read(unsigned int addr)
{
	unsigned int val;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return 0;

	if (addr < TCON_TOP_BASE) {
		if (lcd_tcon_data->core_reg_width == 8)
			val = lcd_tcon_read_byte(addr);
		else
			val = lcd_tcon_read(addr);
	} else {
		val = lcd_tcon_read(addr);
	}

	return val;
}

void lcd_tcon_reg_write(unsigned int addr, unsigned int val)
{
	unsigned char temp;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	if (addr < TCON_TOP_BASE) {
		if (lcd_tcon_data->core_reg_width == 8) {
			temp = (unsigned char)val;
			lcd_tcon_write_byte(addr, temp);
		} else {
			lcd_tcon_write(addr, val);
		}
	} else {
		lcd_tcon_write(addr, val);
	}
}

static void lcd_tcon_od_init(unsigned char *table)
{
	unsigned int reg, bit, flag;

	if (lcd_tcon_data->reg_core_od == REG_LCD_TCON_MAX)
		return;

	reg = lcd_tcon_data->reg_core_od;
	bit = lcd_tcon_data->bit_od_en;
	flag = (table[reg] >> bit) & 1;
	lcd_tcon_od_set(flag);
}

static void lcd_tcon_od_check(unsigned char *table)
{
	unsigned int reg, bit;

	if (lcd_tcon_data->reg_core_od == REG_LCD_TCON_MAX)
		return;

	reg = lcd_tcon_data->reg_core_od;
	bit = lcd_tcon_data->bit_od_en;
	if (((table[reg] >> bit) & 1) == 0)
		return;

	if (tcon_rmem.flag == 0) {
		table[reg] &= ~(1 << bit);
		LCDPR("%s: invalid fb, disable od function\n", __func__);
	}
}

void lcd_tcon_core_reg_update(void)
{
	unsigned char *table;
	unsigned int len, temp;
	int i, ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	len = lcd_tcon_data->reg_table_len;
	table = lcd_tcon_data->reg_table;
	if (table == NULL) {
		LCDERR("%s: table is NULL\n", __func__);
		return;
	}
	lcd_tcon_od_check(table);
	if (lcd_tcon_data->core_reg_width == 8) {
		for (i = 0; i < len; i++) {
			lcd_tcon_write_byte((i + TCON_CORE_REG_START),
				table[i]);
		}
	} else {
		for (i = 0; i < len; i++)
			lcd_tcon_write((i + TCON_CORE_REG_START), table[i]);
	}
	LCDPR("tcon core regs update\n");

	if (lcd_tcon_data->reg_core_od != REG_LCD_TCON_MAX) {
		i = lcd_tcon_data->reg_core_od;
		if (lcd_tcon_data->core_reg_width == 8)
			temp = lcd_tcon_read_byte(i + TCON_CORE_REG_START);
		else
			temp = lcd_tcon_read(i + TCON_CORE_REG_START);
		LCDPR("%s: tcon od reg readback: 0x%04x = 0x%04x\n",
			__func__, i, temp);
	}
}

unsigned char *lcd_tcon_paddrtovaddr(unsigned long paddr, unsigned int mem_size)
{
	unsigned int highmem_flag = 0;
	int i, max_mem_size = 0;
	void *vaddr = NULL;

	if (tcon_rmem.flag == 0) {
		LCDPR("%s: invalid paddr\n", __func__);
		return NULL;
	}

	highmem_flag =
		PageHighMem(phys_to_page(paddr));
	LCDPR("%s: highmem_flag:%d\n", __func__, highmem_flag);
	if (highmem_flag) {
		i = mem_size % SZ_1M;
		if (i)
			max_mem_size = (mem_size / SZ_1M) + 1;
		vaddr = lcd_vmap(paddr, SZ_1M * max_mem_size);
		if (vaddr == NULL) {
			LCDPR("tcon paddr mapping error: addr: 0x%lx\n",
				(unsigned long)paddr);
			return NULL;
		}
		LCDPR("tcon vaddr: 0x%p\n", vaddr);
	} else {
		vaddr = phys_to_virt(paddr);
		if (vaddr == NULL) {
			LCDERR("tcon vaddr mapping failed: 0x%lx\n",
			       (unsigned long)paddr);
			return NULL;
		}
		LCDPR("tcon vaddr: 0x%p\n", vaddr);
	}
	return vaddr;
}

static void lcd_tcon_vac_set_tl1(unsigned int demura_valid)
{
	int len, i, j, n;
	unsigned int d0, d1, temp, demura_support, set1, set2;
	unsigned char *vac_data;

	if (tcon_rmem.vac_mem_vaddr == NULL) {
		LCDPR("%s: vac_mem_paddr null\n", __func__);
		return;
	}

	vac_data = tcon_rmem.vac_mem_vaddr;

	LCDPR("lcd_tcon vac_set\n");

	n = 8;
	len = TCON_VAC_SET_PARAM_NUM;
	demura_support = vac_data[n];
	set1 = vac_data[n + 2];
	set2 = vac_data[n + 4];
	n += (len * 2);
	if (lcd_debug_print_flag)
		LCDPR("vac_set: %d, 0x%x, 0x%x\n", demura_support, set1, set2);

	/* vac support demura policy */
	if (demura_support) {
		if (demura_valid == 0) {
			LCDPR("lcd_tcon vac_set exit for demura invalid\n");
			return;
		}
	}

	lcd_tcon_write_byte(0x0267, lcd_tcon_read_byte(0x0267) | 0xa0);
	/*vac_cntl, 12pipe delay temp for pre_dt*/
	lcd_tcon_write(0x2800, 0x807);
	lcd_tcon_write(0x2817, (0x1e | ((set1 & 0xff) << 8)));

	len = TCON_VAC_LUT_PARAM_NUM;
	if (lcd_debug_print_flag)
		LCDPR("%s: start write vac_ramt1~2\n", __func__);
	/*write vac_ramt1: 8bit, 256 regs*/
	for (i = 0; i < len; i++)
		lcd_tcon_write_byte(0xa100 + i, vac_data[n+i*2]);

	for (i = 0; i < len; i++)
		lcd_tcon_write_byte(0xa200 + i, vac_data[n+i*2]);

	/*write vac_ramt2: 8bit, 256 regs*/
	n += (len * 2);
	for (i = 0; i < len; i++)
		lcd_tcon_write_byte(0xa300 + i, vac_data[n+i*2]);

	for (i = 0; i < len; i++)
		lcd_tcon_write_byte(0xbc00 + i, vac_data[n+i*2]);

	if (lcd_debug_print_flag)
		LCDPR("%s: write vac_ramt1~2 ok\n", __func__);
	for (i = 0; i < len; i++)
		lcd_tcon_read_byte(0xbc00 + i);

	if (lcd_debug_print_flag)
		LCDPR("%s: start write vac_ramt3\n", __func__);
	/*write vac_ramt3_1~6: 16bit({data0[11:0],data1[11:0]},128 regs)*/
	for (j = 0; j < 6; j++) {
		n += (len * 2);
		for (i = 0; i < (len >> 1); i++) {
			d0 = (vac_data[n + (i * 4)] |
				(vac_data[n + (i * 4 + 1)] << 8)) & 0xfff;
			d1 = (vac_data[n + (i * 4 + 2)] |
				(vac_data[n + (i * 4 + 3)] << 8)) & 0xfff;
			temp = ((d0 << 12) | d1);
			lcd_tcon_write((0x2900 + i + (j * 128)), temp);
		}
	}
	if (lcd_debug_print_flag)
		LCDPR("%s: write vac_ramt3 ok\n", __func__);
	for (i = 0; i < ((len >> 1) * 6); i++)
		lcd_tcon_read(0x2900 + i);

	lcd_tcon_write(0x2801, 0x0f000870); /* vac_size */
	lcd_tcon_write(0x2802, (0x58e00d00 | (set2 & 0xff)));
	lcd_tcon_write(0x2803, 0x80400058);
	lcd_tcon_write(0x2804, 0x58804000);
	lcd_tcon_write(0x2805, 0x80400000);
	lcd_tcon_write(0x2806, 0xf080a032);
	lcd_tcon_write(0x2807, 0x4c08a864);
	lcd_tcon_write(0x2808, 0x10200000);
	lcd_tcon_write(0x2809, 0x18200000);
	lcd_tcon_write(0x280a, 0x18000004);
	lcd_tcon_write(0x280b, 0x735244c2);
	lcd_tcon_write(0x280c, 0x9682383d);
	lcd_tcon_write(0x280d, 0x96469449);
	lcd_tcon_write(0x280e, 0xaf363ce7);
	lcd_tcon_write(0x280f, 0xc71fbb56);
	lcd_tcon_write(0x2810, 0x953885a1);
	lcd_tcon_write(0x2811, 0x7a7a7900);
	lcd_tcon_write(0x2812, 0xc4640708);
	lcd_tcon_write(0x2813, 0x4b14b08a);
	lcd_tcon_write(0x2814, 0x4004b12c);
	lcd_tcon_write(0x2815, 0x0);
	/*vac_cntl,always read*/
	lcd_tcon_write(0x2800, 0x381f);
}

static int lcd_tcon_deruma_setting_tl1(void)
{
	int i, data;
	char *demura_setting;

	if (tcon_rmem.demura_set_vaddr == NULL)
		return -1;

	demura_setting = tcon_rmem.demura_set_vaddr;

	if (lcd_tcon_getb_byte(0x23d, 0, 1) == 0) {
		if (lcd_debug_print_flag)
			LCDPR("%s: demura function disabled\n", __func__);
		return 0;
	}

	if (lcd_debug_print_flag == 3) {
		for (i = 0; i < 30; i++)
			LCDPR("demura_set data[%d]: 0x%x\n",
			      i, demura_setting[i]);
	}

	/*demura_setting is
	 *chk_data: 8byte + demura_set_data: 161byte
	 */
	for (i = 8; i < 169; i++) {
		data = demura_setting[i];
		lcd_tcon_write_byte(0x186, data);
	}

	return 0;
}

static int lcd_tcon_deruma_lut_tl1(void)
{
	int i, data;
	char *demura_lut;

	if (tcon_rmem.demura_lut_vaddr == NULL)
		return -1;

	demura_lut = tcon_rmem.demura_lut_vaddr;

	if (lcd_debug_print_flag == 3) {
		for (i = 0; i < 30; i++)
			LCDPR("demura_lut data[%d]: 0x%x\n",
			       i, demura_lut[i]);
	}

	if (lcd_tcon_getb_byte(0x23d, 0, 1) == 0) {
		if (lcd_debug_print_flag)
			LCDPR("%s: demura function disabled\n", __func__);
		return 0;
	}

	lcd_tcon_setb_byte(0x181, 1, 0, 1);
	lcd_tcon_write_byte(0x182, 0x01);
	lcd_tcon_write_byte(0x183, 0x86);
	lcd_tcon_write_byte(0x184, 0x01);
	lcd_tcon_write_byte(0x185, 0x87);

	/*demura_lut is
	 *chk_data: 8byte + demura_lut_data: 391053byte
	 */
	for (i = 8; i < 391062; i++) {
		data = demura_lut[i];
		lcd_tcon_write_byte(0x187, data);
	}

	tcon_rmem.demura_valid = 1;
	LCDPR("lcd_tcon 0x23d = 0x%02x\n", lcd_tcon_read_byte(0x23d));

	return 0;
}

static int lcd_tcon_top_set_tl1(struct lcd_config_s *pconf)
{
	unsigned int axi_reg[3] = {
		TCON_AXI_OFST0, TCON_AXI_OFST1, TCON_AXI_OFST2
	};
	unsigned int addr[3] = {0, 0, 0};
	unsigned int size[3] = {4162560, 4162560, 1960440};
	int i;

	LCDPR("lcd tcon top set\n");

	if (tcon_rmem.flag) {
		addr[0] = tcon_rmem.mem_paddr;
		addr[1] = addr[0] + size[0];
		addr[2] = addr[1] + size[1];
		for (i = 0; i < 3; i++) {
			lcd_tcon_write(axi_reg[i], addr[i]);
			LCDPR("set tcon axi_mem_paddr[%d]: 0x%08x\n",
				i, addr[i]);
		}
	} else {
		LCDERR("%s: invalid axi_mem\n", __func__);
	}

	lcd_tcon_write(TCON_CLK_CTRL, 0x001f);
	if (pconf->lcd_basic.lcd_type == LCD_P2P) {
		switch (pconf->lcd_control.p2p_config->p2p_type) {
		case P2P_CHPI:
			lcd_tcon_write(TCON_TOP_CTRL, 0x8199);
			break;
		default:
			lcd_tcon_write(TCON_TOP_CTRL, 0x8999);
			break;
		}
	} else {
		lcd_tcon_write(TCON_TOP_CTRL, 0x8999);
	}
	lcd_tcon_write(TCON_PLLLOCK_CNTL, 0x0037);
	lcd_tcon_write(TCON_RST_CTRL, 0x003f);
	lcd_tcon_write(TCON_RST_CTRL, 0x0000);
	lcd_tcon_write(TCON_DDRIF_CTRL0, 0x33fff000);
	lcd_tcon_write(TCON_DDRIF_CTRL1, 0x300300);

	return 0;
}

static int lcd_tcon_vac_load(void)
{
	unsigned int n;
	unsigned int chk_data = 0;
	unsigned char *vac_data = tcon_rmem.vac_mem_vaddr;

	if ((!tcon_rmem.vac_mem_size) || (vac_data == NULL))
		return -1;

	chk_data |= vac_data[0];
	chk_data |= vac_data[1] << 8;
	chk_data |= vac_data[2] << 16;
	chk_data |= vac_data[3] << 24;
	if (!chk_data) {
		LCDPR("%s: check vac_data error\n", __func__);
		return -1;
	}

	if (lcd_debug_print_flag == 3) {
		for (n = 0; n < 30; n++)
			LCDPR("%s: vac_data[%d]: 0x%x", __func__,
			      n, vac_data[n * 1]);
	}

	return 0;
}

int lcd_tcon_demura_set_load(void)
{
	unsigned int n;
	char *demura_setting;
	unsigned int chk_data = 0;

	if (tcon_rmem.demura_set_vaddr == NULL)
		return -1;

	demura_setting = tcon_rmem.demura_set_vaddr;
	chk_data |= demura_setting[0];
	chk_data |= demura_setting[1] << 8;
	chk_data |= demura_setting[2] << 16;
	chk_data |= demura_setting[3] << 24;
	if (!chk_data) {
		LCDPR("%s: check demura_setting_data error\n", __func__);
		return -1;
	}

	if (lcd_debug_print_flag == 3) {
		for (n = 0; n < 30; n++)
			LCDPR("%s: demura_set_data[%d]: 0x%x",
			      __func__, n, demura_setting[n]);
	}

	return 0;
}

static int lcd_tcon_demura_lut_load(void)
{
	unsigned int n;
	char *demura_lut;
	unsigned int chk_data = 0;

	if (tcon_rmem.demura_lut_vaddr == NULL)
		return -1;

	demura_lut = tcon_rmem.demura_lut_vaddr;
	chk_data |= demura_lut[0];
	chk_data |= demura_lut[1] << 8;
	chk_data |= demura_lut[2] << 16;
	chk_data |= demura_lut[3] << 24;
	if (!chk_data) {
		LCDPR("%s: check demura_lut_data error\n", __func__);
		return -1;
	}

	if (lcd_debug_print_flag == 3) {
		for (n = 0; n < 30; n++)
			LCDPR("%s: demura_lut_data[%d]: 0x%x\n",
			      __func__, n, demura_lut[n]);
	}

	return 0;
}

static int lcd_tcon_data_load(int *vac_valid, int *demura_valid)
{
	unsigned char *table = lcd_tcon_data->reg_table;
	int ret;

	ret = lcd_tcon_vac_load();
	if (ret == 0)
		*vac_valid = 1;
	if (table == NULL) {
		LCDPR("%s: no tcon bin table\n", __func__);
		return 0;
	}
	if (table[0x23d]) {
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
				*demura_valid = 1;
			}
		}
	}
	return 0;
}

static int lcd_tcon_mem_config(void)
{
	int max_size;

	max_size = lcd_tcon_data->core_size +
		lcd_tcon_data->vac_size +
		lcd_tcon_data->demura_set_size +
		lcd_tcon_data->demura_lut_size;

	if (tcon_rmem.mem_size < max_size) {
		LCDERR("%s: the tcon size is too small\n", __func__);
		return -1;
	}

	tcon_rmem.core_mem_size = lcd_tcon_data->core_size;
	tcon_rmem.core_mem_paddr = tcon_rmem.mem_paddr;
	if (lcd_debug_print_flag)
		LCDPR("%s: core_paddr: 0x%08x, size: 0x%x\n",
		      __func__, (unsigned int)tcon_rmem.core_mem_paddr,
		      tcon_rmem.core_mem_size);

	tcon_rmem.vac_mem_size = lcd_tcon_data->vac_size;
	tcon_rmem.vac_mem_paddr =
		tcon_rmem.core_mem_paddr + tcon_rmem.core_mem_size;
	tcon_rmem.vac_mem_vaddr =
		(unsigned char *)lcd_tcon_paddrtovaddr(tcon_rmem.vac_mem_paddr,
		tcon_rmem.vac_mem_size);
	if (tcon_rmem.vac_mem_vaddr == NULL)
		return -1;

	if (lcd_debug_print_flag && (tcon_rmem.vac_mem_size > 0))
		LCDPR("vac_paddr: 0x%08x, vac vaddr: 0x%p, size: 0x%x\n",
		      (unsigned int)tcon_rmem.vac_mem_paddr,
		      tcon_rmem.vac_mem_vaddr,
		      tcon_rmem.vac_mem_size);

	tcon_rmem.demura_set_mem_size = lcd_tcon_data->demura_set_size;
	tcon_rmem.demura_set_paddr =
		tcon_rmem.vac_mem_paddr + tcon_rmem.vac_mem_size;
	tcon_rmem.demura_set_vaddr = tcon_rmem.vac_mem_vaddr +
			tcon_rmem.vac_mem_size;
	if (lcd_debug_print_flag && (tcon_rmem.demura_set_mem_size > 0))
		LCDPR("%s: dem_set_paddr: 0x%08x, vaddr: 0x%p, size: 0x%x\n",
		      __func__, (unsigned int)tcon_rmem.demura_set_paddr,
		      tcon_rmem.demura_set_vaddr,
		      tcon_rmem.demura_set_mem_size);

	tcon_rmem.demura_lut_mem_size = lcd_tcon_data->demura_lut_size;
	tcon_rmem.demura_lut_paddr =
		tcon_rmem.demura_set_paddr + tcon_rmem.demura_set_mem_size;
	tcon_rmem.demura_lut_vaddr = tcon_rmem.demura_set_vaddr +
		tcon_rmem.demura_set_mem_size;
	if (lcd_debug_print_flag && (tcon_rmem.demura_lut_mem_size > 0))
		LCDPR("%s: dem_lut_paddr: 0x%08x, vaddr: 0x%p, size: 0x%x\n",
		      __func__, (unsigned int)tcon_rmem.demura_lut_paddr,
		      tcon_rmem.demura_lut_vaddr,
		      tcon_rmem.demura_lut_mem_size);

	return 0;
}

#ifdef CONFIG_CMA
static int lcd_tcon_cma_mem_config(struct cma *cma)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	unsigned int mem_size;

	tcon_rmem.mem_paddr = cma_get_base(cma);
	LCDPR("tcon axi_mem base:0x%lx, size:0x%lx\n",
	      (unsigned long)tcon_rmem.mem_paddr,
	      cma_get_size(cma));
	mem_size = lcd_tcon_data->axi_mem_size;
	if (cma_get_size(cma) < mem_size)
		tcon_rmem.flag = 0;
	else {
		tcon_rmem.mem_vaddr =
			dma_alloc_from_contiguous(lcd_drv->dev,
				(mem_size >> PAGE_SHIFT), 0);
		if (tcon_rmem.mem_vaddr == NULL) {
			LCDERR("tcon axi_mem alloc failed\n");
		} else {
			LCDPR("tcon axi_mem dma_alloc=0x%x\n", mem_size);
			tcon_rmem.mem_size = mem_size;
			tcon_rmem.flag = 2; /* cma memory */
		}
	}

	return 0;
}
#endif

static int lcd_tcon_enable_tl1(struct lcd_config_s *pconf)
{
	unsigned int n = 10;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	/* step 1: tcon top */
	lcd_tcon_top_set_tl1(pconf);

	/* step 2: tcon_core_reg_update */
	lcd_tcon_core_reg_update();
	if (pconf->lcd_basic.lcd_type == LCD_P2P) {
		switch (pconf->lcd_control.p2p_config->p2p_type) {
		case P2P_CHPI:
			lcd_phy_tcon_chpi_bbc_init_tl1(n);
			break;
		default:
			break;
		}
	}

	if (tcon_rmem.vac_valid)
		lcd_tcon_vac_set_tl1(tcon_rmem.demura_valid);
	if (tcon_rmem.demura_valid) {
		lcd_tcon_deruma_setting_tl1();
		lcd_tcon_deruma_lut_tl1();
	}

	/* step 3: tcon_top_output_set */
	lcd_tcon_write(TCON_OUT_CH_SEL0, 0x76543210);
	lcd_tcon_write(TCON_OUT_CH_SEL1, 0xba98);

	/* step 4: tcon_intr_mask */
	lcd_tcon_write(TCON_INTR_MASKN, TCON_INTR_MASKN_VAL);

	return 0;
}

static irqreturn_t lcd_tcon_isr(int irq, void *dev_id)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	unsigned int temp;

	if ((lcd_drv->lcd_status & LCD_STATUS_IF_ON) == 0)
		return IRQ_HANDLED;

	temp = lcd_tcon_read(TCON_INTR_RO);
	if (temp & 0x2) {
		LCDPR("%s: tcon sw_reset triggered\n", __func__);
		lcd_tcon_core_reg_update();
	}
	if (temp & 0x40)
		LCDPR("%s: tcon ddr interface error triggered\n", __func__);

	return IRQ_HANDLED;
}

static void lcd_tcon_intr_init(struct aml_lcd_drv_s *lcd_drv)
{
	unsigned int tcon_irq = 0;

	if (!lcd_drv->res_tcon_irq) {
		LCDERR("res_tcon_irq is null\n");
		return;
	}
	tcon_irq = lcd_drv->res_tcon_irq->start;
	if (lcd_debug_print_flag)
		LCDPR("tcon_irq: %d\n", tcon_irq);

	if (request_irq(tcon_irq, lcd_tcon_isr, IRQF_SHARED,
		"lcd_tcon", (void *)"lcd_tcon"))
		LCDERR("can't request lcd_tcon irq\n");
	else {
		if (lcd_debug_print_flag)
			LCDPR("request lcd_tcon irq successful\n");
	}

	lcd_tcon_write(TCON_INTR_MASKN, TCON_INTR_MASKN_VAL);
}

static int lcd_tcon_config(struct aml_lcd_drv_s *lcd_drv)
{
	int key_len, reg_len, ret;

#if 1
	/* get reg table from unifykey */
	reg_len = lcd_tcon_data->reg_table_len;
	if (lcd_tcon_data->reg_table == NULL) {
		lcd_tcon_data->reg_table =
			kcalloc(reg_len, sizeof(unsigned char), GFP_KERNEL);
		if (!lcd_tcon_data->reg_table) {
			LCDERR("%s: Not enough memory\n", __func__);
			return -1;
		}
	}
	key_len = reg_len;
	ret = lcd_unifykey_get_no_header("lcd_tcon",
		lcd_tcon_data->reg_table, &key_len);
	if (ret) {
		kfree(lcd_tcon_data->reg_table);
		lcd_tcon_data->reg_table = NULL;
		LCDERR("%s: !!!!!!!!tcon unifykey load error!!!!!!!!\n",
			__func__);
		return -1;
	}
	if (key_len != reg_len) {
		kfree(lcd_tcon_data->reg_table);
		lcd_tcon_data->reg_table = NULL;
		LCDERR("%s: !!!!!!!!tcon unifykey load length error!!!!!!!!\n",
			__func__);
		return -1;
	}
	LCDPR("tcon: load unifykey len: %d\n", key_len);
#else
	reg_len = lcd_tcon_data->reg_table_len;
	lcd_tcon_data->reg_table = uhd_tcon_setting_ceds_h10;
	key_len = sizeof(uhd_tcon_setting_ceds_h10)/sizeof(unsigned char);
	if (key_len != reg_len) {
		lcd_tcon_data->reg_table = NULL;
		LCDERR("%s: !!!!!!!!tcon unifykey load length error!!!!!!!!\n",
			__func__);
		return -1;
	}
	LCDPR("tcon: load default table len: %d\n", key_len);
#endif

	LCDPR("%s: lcd_tcon_data_load\n", __func__);
	lcd_tcon_data_load(&tcon_rmem.vac_valid, &tcon_rmem.demura_valid);
	lcd_drv->tcon_status = tcon_rmem.vac_valid |
			       (tcon_rmem.demura_valid << 1);

	lcd_tcon_intr_init(lcd_drv);
	lcd_tcon_od_init(lcd_tcon_data->reg_table);

	return 0;
}

static void lcd_tcon_config_delayed(struct work_struct *work)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	int key_init_flag = 0;
	int i = 0;

	key_init_flag = key_unify_get_init_flag();
	while (key_init_flag == 0) {
		if (i++ >= LCD_UNIFYKEY_WAIT_TIMEOUT)
			break;
		msleep(LCD_UNIFYKEY_RETRY_INTERVAL);
		key_init_flag = key_unify_get_init_flag();
	}
	LCDPR("tcon: key_init_flag=%d, i=%d\n", key_init_flag, i);
	if (key_init_flag)
		lcd_tcon_config(lcd_drv);
}

/* **********************************
 * tcon function api
 * **********************************
 */
#define PR_BUF_MAX    200
void lcd_tcon_reg_table_print(void)
{
	int i, j, n, cnt;
	char *buf;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	if (lcd_tcon_data->reg_table == NULL) {
		LCDERR("%s: reg_table is null\n", __func__);
		return;
	}

	buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
	if (buf == NULL) {
		LCDERR("%s: buf malloc error\n", __func__);
		return;
	}

	LCDPR("%s:\n", __func__);
	cnt = lcd_tcon_data->reg_table_len;
	for (i = 0; i < cnt; i += 16) {
		n = snprintf(buf, PR_BUF_MAX, "0x%04x: ", i);
		for (j = 0; j < 16; j++) {
			if ((i + j) >= cnt)
				break;
			n += snprintf(buf+n, PR_BUF_MAX, " 0x%02x",
				lcd_tcon_data->reg_table[i+j]);
		}
		buf[n] = '\0';
		pr_info("%s\n", buf);
	}
	kfree(buf);
}

void lcd_tcon_reg_readback_print(void)
{
	int i, j, n, cnt;
	char *buf;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
	if (buf == NULL) {
		LCDERR("%s: buf malloc error\n", __func__);
		return;
	}

	LCDPR("%s:\n", __func__);
	cnt = lcd_tcon_data->reg_table_len;
	for (i = 0; i < cnt; i += 16) {
		n = snprintf(buf, PR_BUF_MAX, "0x%04x: ", i);
		for (j = 0; j < 16; j++) {
			if ((i + j) >= cnt)
				break;
			if (lcd_tcon_data->core_reg_width == 8) {
				n += snprintf(buf+n, PR_BUF_MAX, " 0x%02x",
					lcd_tcon_read_byte(i+j));
			} else {
				n += snprintf(buf+n, PR_BUF_MAX, " 0x%02x",
					lcd_tcon_read(i+j));
			}
		}
		buf[n] = '\0';
		pr_info("%s\n", buf);
	}
	kfree(buf);
}

int lcd_tcon_info_print(char *buf, int offset)
{
	int len = 0, n, ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return len;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf+len), n,
		"tcon info:\n"
		"core_reg_width:    %d\n"
		"reg_table_len:     %d\n"
		"axi_mem paddr:     0x%lx\n"
		"axi_mem size:      0x%x\n\n",
		lcd_tcon_data->core_reg_width,
		lcd_tcon_data->reg_table_len,
		(unsigned long)tcon_rmem.mem_paddr,
		tcon_rmem.mem_size);

	return len;
}

int lcd_tcon_reg_table_size_get(void)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	return lcd_tcon_data->reg_table_len;
}

unsigned char *lcd_tcon_reg_table_get(void)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return NULL;

	return lcd_tcon_data->reg_table;
}

int lcd_tcon_core_reg_get(unsigned char *buf, unsigned int size)
{
	int i, ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	if (size > lcd_tcon_data->reg_table_len) {
		LCDERR("%s: size is not enough\n", __func__);
		return -1;
	}

	if (lcd_tcon_data->core_reg_width == 8) {
		for (i = 0; i < size; i++)
			buf[i] = lcd_tcon_read_byte(i + TCON_CORE_REG_START);
	} else {
		for (i = 0; i < size; i++)
			buf[i] = lcd_tcon_read(i + TCON_CORE_REG_START);
	}

	return 0;
}

int lcd_tcon_od_set(int flag)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	unsigned int reg, bit, temp;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	if (lcd_tcon_data->reg_core_od == REG_LCD_TCON_MAX) {
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

	if (!(lcd_drv->lcd_status & LCD_STATUS_IF_ON))
		return -1;

	reg = lcd_tcon_data->reg_core_od;
	bit = lcd_tcon_data->bit_od_en;
	if (lcd_tcon_data->core_reg_width == 8)
		temp = lcd_tcon_read_byte(reg + TCON_CORE_REG_START);
	else
		temp = lcd_tcon_read(reg + TCON_CORE_REG_START);
	if (flag)
		temp |= (1 << bit);
	else
		temp &= ~(1 << bit);
	temp &= 0xff;
	if (lcd_tcon_data->core_reg_width == 8)
		lcd_tcon_write_byte((reg + TCON_CORE_REG_START), temp);
	else
		lcd_tcon_write((reg + TCON_CORE_REG_START), temp);

	msleep(100);
	LCDPR("%s: %d\n", __func__, flag);

	return 0;
}

int lcd_tcon_od_get(void)
{
	unsigned int reg, bit, temp;
	int ret = 0;

	ret = lcd_tcon_valid_check();
	if (ret)
		return 0;

	if (lcd_tcon_data->reg_core_od == REG_LCD_TCON_MAX) {
		LCDERR("%s: invalid od reg\n", __func__);
		return 0;
	}

	reg = lcd_tcon_data->reg_core_od;
	bit = lcd_tcon_data->bit_od_en;
	if (lcd_tcon_data->core_reg_width == 8)
		temp = lcd_tcon_read_byte(reg + TCON_CORE_REG_START);
	else
		temp = lcd_tcon_read(reg + TCON_CORE_REG_START);
	ret = ((temp >> bit) & 1);

	return ret;
}

int lcd_tcon_enable(struct lcd_config_s *pconf)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	if (lcd_tcon_data->tcon_enable)
		lcd_tcon_data->tcon_enable(pconf);

	return 0;
}

#define TCON_CTRL_TIMING_OFFSET    12
void lcd_tcon_disable(void)
{
	unsigned int reg, i, cnt, offset, bit;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	LCDPR("%s\n", __func__);

	/* disable tcon intr */
	lcd_tcon_write(TCON_INTR_MASKN, 0);

	/* disable over_drive */
	if (lcd_tcon_data->reg_core_od != REG_LCD_TCON_MAX) {
		reg = lcd_tcon_data->reg_core_od + TCON_CORE_REG_START;
		bit = lcd_tcon_data->bit_od_en;
		if (lcd_tcon_data->core_reg_width == 8)
			lcd_tcon_setb_byte(reg, 0, bit, 1);
		else
			lcd_tcon_setb(reg, 0, bit, 1);
		msleep(100);
	}

	/* disable all ctrl signal */
	if (lcd_tcon_data->reg_core_ctrl_timing_base == REG_LCD_TCON_MAX)
		goto lcd_tcon_disable_next;
	reg = lcd_tcon_data->reg_core_ctrl_timing_base + TCON_CORE_REG_START;
	offset = lcd_tcon_data->ctrl_timing_offset;
	cnt = lcd_tcon_data->ctrl_timing_cnt;
	for (i = 0; i < cnt; i++) {
		if (lcd_tcon_data->core_reg_width == 8)
			lcd_tcon_setb_byte((reg + (i * offset)), 1, 3, 1);
		else
			lcd_tcon_setb((reg + (i * offset)), 1, 3, 1);
	}

	/* disable top */
lcd_tcon_disable_next:
	if (lcd_tcon_data->reg_top_ctrl != REG_LCD_TCON_MAX) {
		reg = lcd_tcon_data->reg_top_ctrl;
		bit = lcd_tcon_data->bit_en;
		lcd_tcon_setb(reg, 0, bit, 1);
	}
}

/* **********************************
 * tcon match data
 * **********************************
 */
static struct lcd_tcon_data_s tcon_data_tl1 = {
	.tcon_valid = 0,

	.core_reg_width = LCD_TCON_CORE_REG_WIDTH_TL1,
	.reg_table_len = LCD_TCON_TABLE_LEN_TL1,

	.reg_top_ctrl = TCON_TOP_CTRL,
	.bit_en = BIT_TOP_EN_TL1,

	.reg_core_od = REG_CORE_OD_TL1,
	.bit_od_en = BIT_OD_EN_TL1,

	.reg_core_ctrl_timing_base = REG_LCD_TCON_MAX,
	.ctrl_timing_offset = CTRL_TIMING_OFFSET_TL1,
	.ctrl_timing_cnt = CTRL_TIMING_CNT_TL1,

	/*tcon_mem_total: core_mem(10M)  vac(8k) dem_set(4k) dem_lut(400k)*/
	/* (12M) :      |--------------|-------|-----------|--------|*/
	.axi_mem_size = 0xc00000,
	.core_size = 0x00a00000,
	.vac_size = 0x00002000,
	.demura_set_size = 0x00001000,
	.demura_lut_size = 0x00064000,
	.reg_table = NULL,

	.tcon_enable = lcd_tcon_enable_tl1,
};

int lcd_tcon_probe(struct aml_lcd_drv_s *lcd_drv)
{
	struct cma *cma;
	unsigned int mem_size;
	int key_init_flag = 0;
	int ret = 0;

	lcd_tcon_data = NULL;
	switch (lcd_drv->data->chip_type) {
	case LCD_CHIP_TL1:
	case LCD_CHIP_TM2:
		switch (lcd_drv->lcd_config->lcd_basic.lcd_type) {
		case LCD_MLVDS:
		case LCD_P2P:
			lcd_tcon_data = &tcon_data_tl1;
			lcd_tcon_data->tcon_valid = 1;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	if (lcd_tcon_data == NULL)
		return 0;
	if (lcd_tcon_data->tcon_valid == 0)
		return 0;

	/* init reserved memory */
	ret = of_reserved_mem_device_init(lcd_drv->dev);
	if (ret) {
		LCDERR("tcon: init reserved memory failed\n");
	} else {
		if ((void *)tcon_rmem.mem_paddr == NULL) {
#ifdef CONFIG_CMA
			cma = dev_get_cma_area(lcd_drv->dev);
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
			mem_size = tcon_rmem.mem_size;
			if (mem_size < lcd_tcon_data->axi_mem_size)
				tcon_rmem.flag = 0;
			else {
				tcon_rmem.flag = 1; /* reserved memory */
				lcd_tcon_mem_config();
				LCDPR("tcon axi_mem base:0x%lx, size:0x%x\n",
				(unsigned long)tcon_rmem.mem_paddr, mem_size);
			}
		}
	}

	INIT_DELAYED_WORK(&lcd_tcon_delayed_work, lcd_tcon_config_delayed);

	key_init_flag = key_unify_get_init_flag();
	if (key_init_flag) {
		ret = lcd_tcon_config(lcd_drv);
	} else {
		if (lcd_drv->workqueue) {
			queue_delayed_work(lcd_drv->workqueue,
				&lcd_tcon_delayed_work,
				msecs_to_jiffies(2000));
		} else {
			schedule_delayed_work(&lcd_tcon_delayed_work,
				msecs_to_jiffies(2000));
		}
	}

	return ret;
}

int lcd_tcon_remove(struct aml_lcd_drv_s *lcd_drv)
{
	if (tcon_rmem.flag == 2) {
		LCDPR("tcon free memory: base:0x%lx, size:0x%x\n",
			(unsigned long)tcon_rmem.mem_paddr,
			tcon_rmem.mem_size);
#ifdef CONFIG_CMA
		dma_release_from_contiguous(lcd_drv->dev,
			tcon_rmem.mem_vaddr,
			tcon_rmem.mem_size >> PAGE_SHIFT);
#endif
	}

	if (tcon_rmem.vac_mem_vaddr != NULL) {
		lcd_unmap_phyaddr(tcon_rmem.vac_mem_vaddr);
		tcon_rmem.vac_mem_vaddr = NULL;
		tcon_rmem.demura_set_vaddr = NULL;
		tcon_rmem.demura_lut_vaddr = NULL;
	}

	if (lcd_tcon_data) {
		/* lcd_tcon_data == NULL; */
		lcd_tcon_data->tcon_valid = 0;
	}

	return 0;
}

static int __init tcon_fb_device_init(struct reserved_mem *rmem,
		struct device *dev)
{
	return 0;
}

static const struct reserved_mem_ops tcon_fb_ops = {
	.device_init = tcon_fb_device_init,
};

static int __init tcon_fb_setup(struct reserved_mem *rmem)
{
	tcon_rmem.mem_paddr = rmem->base;
	tcon_rmem.mem_size = rmem->size;
	rmem->ops = &tcon_fb_ops;
	LCDPR("tcon: Reserved memory: created fb at 0x%lx, size %ld MiB\n",
		(unsigned long)rmem->base, (unsigned long)rmem->size / SZ_1M);
	return 0;
}
RESERVEDMEM_OF_DECLARE(fb, "amlogic, lcd_tcon-memory", tcon_fb_setup);

