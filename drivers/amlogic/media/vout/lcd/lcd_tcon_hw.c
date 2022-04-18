// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/reset.h>
#include "lcd_common.h"
#include "lcd_reg.h"
#include "lcd_tcon.h"
#ifdef CONFIG_AMLOGIC_TEE
#include <linux/amlogic/tee.h>
#endif

static void lcd_tcon_core_reg_pre_od(struct lcd_tcon_config_s *tcon_conf,
				     struct tcon_mem_map_table_s *mm_table)
{
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	unsigned char *table8;
	unsigned int *table32;
	unsigned int reg, bit, en = 0;

	if (!mm_table || !mm_table->core_reg_table) {
		LCDERR("%s: table is NULL\n", __func__);
		return;
	}
	if (tcon_conf->reg_core_od == REG_LCD_TCON_MAX)
		return;

	reg = tcon_conf->reg_core_od;
	bit = tcon_conf->bit_od_en;

	if (tcon_conf->core_reg_width == 8) {
		table8 = mm_table->core_reg_table;
		if (((table8[reg] >> bit) & 1) == 0)
			return;
		if (!tcon_rmem) {
			en = 0;
		} else {
			if (tcon_rmem->flag == 0)
				en = 0;
			else
				en = 1;
		}
		if (en == 0) {
			table8[reg] &= ~(1 << bit);
			LCDPR("%s: invalid buf, disable od function\n",
			      __func__);
		}
	} else {
		table32 = (unsigned int *)mm_table->core_reg_table;
		if (((table32[reg] >> bit) & 1) == 0)
			return;
		if (!tcon_rmem) {
			en = 0;
		} else {
			if (tcon_rmem->flag == 0)
				en = 0;
			else
				en = 1;
		}
		if (en == 0) {
			table32[reg] &= ~(1 << bit);
			LCDPR("%s: invalid buf, disable od function\n",
			      __func__);
		}
	}
}

static void lcd_tcon_core_reg_pre_dis_od(struct lcd_tcon_config_s *tcon_conf,
					 struct tcon_mem_map_table_s *mm_table)
{
	unsigned char *table8;
	unsigned int *table32;
	unsigned int reg, bit;

	if (!mm_table || !mm_table->core_reg_table) {
		LCDERR("%s: table is NULL\n", __func__);
		return;
	}
	if (tcon_conf->reg_core_od == REG_LCD_TCON_MAX)
		return;

	reg = tcon_conf->reg_core_od;
	bit = tcon_conf->bit_od_en;

	if (tcon_conf->core_reg_width == 8) {
		table8 = mm_table->core_reg_table;
		table8[reg] &= ~(1 << bit);
	} else {
		table32 = (unsigned int *)mm_table->core_reg_table;
		table32[reg] &= ~(1 << bit);
	}
}

void lcd_tcon_core_reg_set(struct aml_lcd_drv_s *pdrv,
			   struct lcd_tcon_config_s *tcon_conf,
			   struct tcon_mem_map_table_s *mm_table,
			   unsigned char *core_reg_table)
{
	unsigned char *table8;
	unsigned int *table32;
	unsigned int len, offset;
	int i, ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	if (!mm_table || !core_reg_table) {
		LCDERR("%s: table is NULL\n", __func__);
		return;
	}

	len = mm_table->core_reg_table_size;
	offset = tcon_conf->core_reg_start;
	if (tcon_conf->core_reg_width == 8) {
		table8 = core_reg_table;
		for (i = offset; i < len; i++)
			lcd_tcon_write_byte(pdrv, i, table8[i]);
	} else {
		if (tcon_conf->reg_table_width == 32) {
			table32 = (unsigned int *)core_reg_table;
			len /= 4;
			for (i = offset; i < len; i++)
				lcd_tcon_write(pdrv, i, table32[i]);
		} else {
			table8 = core_reg_table;
			for (i = offset; i < len; i++)
				lcd_tcon_write(pdrv, i, table8[i]);
		}
	}
	LCDPR("%s\n", __func__);
}

static void lcd_tcon_data_init_set(struct aml_lcd_drv_s *pdrv, unsigned char *data_buf)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	struct lcd_tcon_local_cfg_s *local_cfg = get_lcd_tcon_local_cfg();
	struct lcd_tcon_init_block_header_s *data_header;
	unsigned char *core_reg_table;

	if (!tcon_conf)
		return;
	if (!mm_table)
		return;
	if (!local_cfg)
		return;

	data_header = (struct lcd_tcon_init_block_header_s *)data_buf;
	core_reg_table = data_buf + LCD_TCON_DATA_BLOCK_HEADER_SIZE;
	switch (data_header->block_ctrl) {
	case LCD_TCON_DATA_CTRL_FLAG_DLG:
		if (pdrv->config.basic.h_active == data_header->h_active &&
		    pdrv->config.basic.v_active == data_header->v_active) {
			lcd_tcon_init_data_version_update(data_header->version);
			lcd_tcon_core_reg_set(pdrv, tcon_conf, mm_table, core_reg_table);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("%s: dlg %dx%d init, bin_ver:%s\n",
					__func__, data_header->h_active,
					data_header->v_active,
					local_cfg->bin_ver);
			}
		}
		break;
	default:
		break;
	}
}

static void lcd_tcon_axi_rmem_set(struct aml_lcd_drv_s *pdrv,
				  struct lcd_tcon_config_s *tcon_conf)
{
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	unsigned int paddr, i;

	if (!tcon_conf)
		return;
	if (!tcon_rmem)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s\n", __func__);

	if (tcon_rmem->flag == 0 || !tcon_rmem->axi_rmem) {
		LCDERR("%s: invalid axi_mem\n", __func__);
		return;
	}

	for (i = 0; i < tcon_conf->axi_bank; i++) {
		paddr = tcon_rmem->axi_rmem[i].mem_paddr;
		lcd_tcon_write(pdrv, tcon_conf->axi_reg[i], paddr);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("set tcon axi_mem[%d] reg: 0x%08x, paddr: 0x%08x\n",
			      i, tcon_conf->axi_reg[i], paddr);
		}
	}
}

static void lcd_tcon_vac_set_tl1(struct aml_lcd_drv_s *pdrv, unsigned int demura_valid)
{
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	int len, i, j, n;
	unsigned int d0, d1, temp, set0, set1, set2;
	unsigned char *buf = NULL;

	if (!tcon_rmem)
		return;

	buf = tcon_rmem->vac_rmem.mem_vaddr;
	if (!buf) {
		LCDERR("%s: vac_mem_vaddr is null\n", __func__);
		return;
	}

	n = 8;
	len = TCON_VAC_SET_PARAM_NUM;
	set0 = buf[n];
	set1 = buf[n + 2];
	set2 = buf[n + 4];
	n += (len * 2);
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("vac_set: 0x%x, 0x%x, 0x%x\n", set0, set1, set2);

	lcd_tcon_write_byte(pdrv, 0x0267, lcd_tcon_read_byte(pdrv, 0x0267) | 0xa0);
	/*vac_cntl, 12pipe delay temp for pre_dt*/
	lcd_tcon_write(pdrv, 0x2800, 0x807);
	if (demura_valid)
		lcd_tcon_write(pdrv, 0x2817, (0x1e | ((set1 & 0xff) << 8)));
	else
		lcd_tcon_write(pdrv, 0x2817, (0x1e | ((set0 & 0xff) << 8)));

	len = TCON_VAC_LUT_PARAM_NUM;
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: start write vac_ramt1~2\n", __func__);
	/*write vac_ramt1: 8bit, 256 regs*/
	for (i = 0; i < len; i++)
		lcd_tcon_write_byte(pdrv, 0xa100 + i, buf[n + i * 2]);

	for (i = 0; i < len; i++)
		lcd_tcon_write_byte(pdrv, 0xa200 + i, buf[n + i * 2]);

	/*write vac_ramt2: 8bit, 256 regs*/
	n += (len * 2);
	for (i = 0; i < len; i++)
		lcd_tcon_write_byte(pdrv, 0xa300 + i, buf[n + i * 2]);

	for (i = 0; i < len; i++)
		lcd_tcon_write_byte(pdrv, 0xbc00 + i, buf[n + i * 2]);

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: write vac_ramt1~2 ok\n", __func__);
	for (i = 0; i < len; i++)
		lcd_tcon_read_byte(pdrv, 0xbc00 + i);

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: start write vac_ramt3\n", __func__);
	/*write vac_ramt3_1~6: 16bit({data0[11:0],data1[11:0]},128 regs)*/
	for (j = 0; j < 6; j++) {
		n += (len * 2);
		for (i = 0; i < (len >> 1); i++) {
			d0 = (buf[n + (i * 4)] |
				(buf[n + (i * 4 + 1)] << 8)) & 0xfff;
			d1 = (buf[n + (i * 4 + 2)] |
				(buf[n + (i * 4 + 3)] << 8)) & 0xfff;
			temp = ((d0 << 12) | d1);
			lcd_tcon_write(pdrv, (0x2900 + i + (j * 128)), temp);
		}
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: write vac_ramt3 ok\n", __func__);
	for (i = 0; i < ((len >> 1) * 6); i++)
		lcd_tcon_read(pdrv, 0x2900 + i);

	lcd_tcon_write(pdrv, 0x2801, 0x0f000870); /* vac_size */
	lcd_tcon_write(pdrv, 0x2802, (0x58e00d00 | (set2 & 0xff)));
	lcd_tcon_write(pdrv, 0x2803, 0x80400058);
	lcd_tcon_write(pdrv, 0x2804, 0x58804000);
	lcd_tcon_write(pdrv, 0x2805, 0x80400000);
	lcd_tcon_write(pdrv, 0x2806, 0xf080a032);
	lcd_tcon_write(pdrv, 0x2807, 0x4c08a864);
	lcd_tcon_write(pdrv, 0x2808, 0x10200000);
	lcd_tcon_write(pdrv, 0x2809, 0x18200000);
	lcd_tcon_write(pdrv, 0x280a, 0x18000004);
	lcd_tcon_write(pdrv, 0x280b, 0x735244c2);
	lcd_tcon_write(pdrv, 0x280c, 0x9682383d);
	lcd_tcon_write(pdrv, 0x280d, 0x96469449);
	lcd_tcon_write(pdrv, 0x280e, 0xaf363ce7);
	lcd_tcon_write(pdrv, 0x280f, 0xc71fbb56);
	lcd_tcon_write(pdrv, 0x2810, 0x953885a1);
	lcd_tcon_write(pdrv, 0x2811, 0x7a7a7900);
	lcd_tcon_write(pdrv, 0x2812, 0xc4640708);
	lcd_tcon_write(pdrv, 0x2813, 0x4b14b08a);
	lcd_tcon_write(pdrv, 0x2814, 0x4004b12c);
	lcd_tcon_write(pdrv, 0x2815, 0x0);
	/*vac_cntl,always read*/
	lcd_tcon_write(pdrv, 0x2800, 0x381f);

	LCDPR("tcon vac finish\n");
}

static int lcd_tcon_demura_set_tl1(struct aml_lcd_drv_s *pdrv)
{
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	unsigned char *data_buf;
	unsigned int data_cnt, i;

	if (!tcon_rmem || !tcon_rmem->demura_set_rmem.mem_vaddr) {
		LCDERR("%s: demura_set_mem_vaddr is null\n", __func__);
		return -1;
	}

	if (lcd_tcon_getb_byte(pdrv, 0x23d, 0, 1) == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
			LCDPR("%s: demura function disabled\n", __func__);
		return 0;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		for (i = 0; i < 30; i++)
			LCDPR("demura_set data[%d]: 0x%x\n",
			      i, tcon_rmem->demura_set_rmem.mem_vaddr[i]);
	}

	/*demura_setting:
	 *  8byte chk_data + demura_set_data: 161byte
	 */
	data_cnt = (tcon_rmem->demura_set_rmem.mem_vaddr[0] |
		    (tcon_rmem->demura_set_rmem.mem_vaddr[1] << 8) |
		    (tcon_rmem->demura_set_rmem.mem_vaddr[2] << 16) |
		    (tcon_rmem->demura_set_rmem.mem_vaddr[3] << 24));
	data_buf = &tcon_rmem->demura_set_rmem.mem_vaddr[8];
	for (i = 0; i < data_cnt; i++)
		lcd_tcon_write_byte(pdrv, 0x186, data_buf[i]);

	LCDPR("tcon demura_set cnt %d\n", data_cnt);

	return 0;
}

static int lcd_tcon_demura_lut_tl1(struct aml_lcd_drv_s *pdrv)
{
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	unsigned char *data_buf;
	unsigned int data_cnt, i;

	if (!tcon_rmem || !tcon_rmem->demura_lut_rmem.mem_vaddr) {
		LCDERR("%s: demura_lut_mem_vaddr is null\n", __func__);
		return -1;
	}

	if (lcd_tcon_getb_byte(pdrv, 0x23d, 0, 1) == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
			LCDPR("%s: demura function disabled\n", __func__);
		return 0;
	}

	/*disable demura when load lut data*/
	lcd_tcon_setb_byte(pdrv, 0x23d, 0, 0, 1);

	lcd_tcon_setb_byte(pdrv, 0x181, 1, 0, 1);
	lcd_tcon_write_byte(pdrv, 0x182, 0x01);
	lcd_tcon_write_byte(pdrv, 0x183, 0x86);
	lcd_tcon_write_byte(pdrv, 0x184, 0x01);
	lcd_tcon_write_byte(pdrv, 0x185, 0x87);

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		for (i = 0; i < 30; i++)
			LCDPR("demura_lut data[%d]: 0x%x\n",
			      i, tcon_rmem->demura_lut_rmem.mem_vaddr[i]);
	}

	/*demura_lut is
	 *chk_data: 8byte + demura_lut_data: 391053byte
	 */
	data_cnt = (tcon_rmem->demura_lut_rmem.mem_vaddr[0] |
		    (tcon_rmem->demura_lut_rmem.mem_vaddr[1] << 8) |
		    (tcon_rmem->demura_lut_rmem.mem_vaddr[2] << 16) |
		    (tcon_rmem->demura_lut_rmem.mem_vaddr[3] << 24));
	data_buf = &tcon_rmem->demura_lut_rmem.mem_vaddr[8];
	/* fixed 2 byte 0 for border */
	lcd_tcon_write_byte(pdrv, 0x187, 0);
	lcd_tcon_write_byte(pdrv, 0x187, 0);
	for (i = 0; i < data_cnt; i++)
		lcd_tcon_write_byte(pdrv, 0x187, data_buf[i]);

	/*enable demura when load lut data finished*/
	lcd_tcon_setb_byte(pdrv, 0x23d, 1, 0, 1);

	LCDPR("tcon demura_lut cnt %d\n", data_cnt);
	LCDPR("lcd_tcon 0x23d = 0x%02x\n", lcd_tcon_read_byte(pdrv, 0x23d));

	return 0;
}

static int lcd_tcon_acc_lut_tl1(struct aml_lcd_drv_s *pdrv)
{
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	unsigned char *data_buf;
	unsigned int data_cnt, i;

	if (!tcon_rmem || !tcon_rmem->acc_lut_rmem.mem_vaddr) {
		LCDERR("%s: acc_lut_mem_vaddr is null\n", __func__);
		return -1;
	}

	/* enable lut access, disable gamma en*/
	lcd_tcon_setb_byte(pdrv, 0x262, 0x2, 0, 2);

	/* write gamma lut */
	/*acc_lut is
	 *chk_data: 8byte + acc_lut_data: 1158byte
	 */
	data_cnt = (tcon_rmem->acc_lut_rmem.mem_vaddr[0] |
		    (tcon_rmem->acc_lut_rmem.mem_vaddr[1] << 8) |
		    (tcon_rmem->acc_lut_rmem.mem_vaddr[2] << 16) |
		    (tcon_rmem->acc_lut_rmem.mem_vaddr[3] << 24));
	if (data_cnt > 1161) { /* 0xb50~0xfd8, 1161 */
		LCDPR("%s: data_cnt %d is invalid, force to 1161\n",
		      __func__, data_cnt);
		data_cnt = 1161;
	}

	data_buf = &tcon_rmem->acc_lut_rmem.mem_vaddr[8];
	for (i = 0; i < data_cnt; i++)
		lcd_tcon_write_byte(pdrv, (0xb50 + i), data_buf[i]);

	/* enable gamma */
	lcd_tcon_setb_byte(pdrv, 0x262, 0x3, 0, 2);

	LCDPR("tcon acc_lut cnt %d\n", data_cnt);

	return 0;
}

void lcd_tcon_axi_rmem_lut_load(unsigned int index, unsigned char *buf,
				unsigned int size)
{
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	unsigned char *p;
	unsigned int span = 0, remain = 0, count = 0;
	unsigned long paddr, phys;
	void *vaddr = NULL;
	unsigned int highmem_flag = 0;
	int i;

	if (!tcon_rmem || !tcon_rmem->axi_rmem) {
		LCDERR("axi_rmem is NULL\n");
		return;
	}
	if (!tcon_conf)
		return;
	if (index > tcon_conf->axi_bank) {
		LCDERR("axi_rmem index %d invalid\n", index);
		return;
	}
	if (tcon_rmem->axi_rmem[index].mem_size < size) {
		LCDERR("axi_mem[%d] size 0x%x is not enough, need 0x%x\n",
		       index, tcon_rmem->axi_rmem[index].mem_size, size);
		return;
	}

	paddr = tcon_rmem->axi_rmem[index].mem_paddr;
	highmem_flag = PageHighMem(phys_to_page(paddr));
	if (highmem_flag == 0) {
		vaddr = phys_to_virt(paddr);
		if (!vaddr)
			goto lcd_tcon_axi_rmem_lut_load_err;
		memcpy(vaddr, buf, size);
	} else {
		span = SZ_1M;
		count = size / PAGE_ALIGN(span);
		remain = size % PAGE_ALIGN(span);

		for (i = 0; i < count; i++) {
			phys = paddr + i * span;
			vaddr = lcd_vmap(phys, span);
			if (!vaddr)
				goto lcd_tcon_axi_rmem_lut_load_err;
			p = buf + i * span;
			memcpy(vaddr, p, span);
			lcd_unmap_phyaddr(vaddr);
		}
		if (remain) {
			phys = paddr + count * span;
			vaddr = lcd_vmap(phys, remain);
			if (!vaddr)
				goto lcd_tcon_axi_rmem_lut_load_err;
			p = buf + count * span;
			memcpy(vaddr, p, remain);
			lcd_unmap_phyaddr(vaddr);
		}
	}

	return;

lcd_tcon_axi_rmem_lut_load_err:
	LCDERR("tcon axi_rmem[%d] mapping failed: 0x%lx\n", index, paddr);
}

static int lcd_tcon_wr_n_data_write(struct aml_lcd_drv_s *pdrv,
				    struct lcd_tcon_data_part_wr_n_s *wr_n,
				    unsigned char *p,
				    unsigned int n,
				    unsigned int reg,
				    unsigned int init_flag)
{
	unsigned int k, data, d;

	if (wr_n->reg_inc) {
		for (k = 0; k < wr_n->data_cnt; k++) {
			data = 0;
			for (d = 0; d < wr_n->reg_data_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (wr_n->reg_data_byte == 1)
				lcd_tcon_write_byte(pdrv, (reg + k), data);
			else
				lcd_tcon_write(pdrv, (reg + k), data);
			if ((lcd_debug_print_flag & LCD_DBG_PR_ADV2) && init_flag) {
				LCDPR("%s: write reg 0x%x=0x%x\n",
				      __func__, (reg + k), data);
			}
			n += wr_n->reg_data_byte;
		}
	} else {
		for (k = 0; k < wr_n->data_cnt; k++) {
			data = 0;
			for (d = 0; d < wr_n->reg_data_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (wr_n->reg_data_byte == 1)
				lcd_tcon_write_byte(pdrv, reg, data);
			else
				lcd_tcon_write(pdrv, reg, data);
			if ((lcd_debug_print_flag & LCD_DBG_PR_ADV2) && init_flag) {
				LCDPR("%s: write reg 0x%x=0x%x\n",
				      __func__, reg, data);
			}
			n += wr_n->reg_data_byte;
		}
	}

	return 0;
}

int lcd_tcon_data_common_parse_set(struct aml_lcd_drv_s *pdrv,
				   unsigned char *data_buf, int init_flag)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct lcd_tcon_data_block_header_s *block_header;
	struct lcd_tcon_data_block_ext_header_s *ext_header;
	unsigned char *p;
	unsigned short part_cnt;
	unsigned char part_type;
	unsigned int size, reg, data, mask, temp, reg_base = 0;
	union lcd_tcon_data_part_u data_part;
	unsigned int data_offset, offset, i, j, k, d, m, n, step = 0;
	unsigned int reg_cnt, reg_byte, data_cnt, data_byte;
	unsigned short block_ctrl_flag;
	int ret;

	if (tcon_conf)
		reg_base = tcon_conf->core_reg_start;

	block_header = (struct lcd_tcon_data_block_header_s *)data_buf;
	p = data_buf + LCD_TCON_DATA_BLOCK_HEADER_SIZE;
	ext_header = (struct lcd_tcon_data_block_ext_header_s *)p;
	part_cnt = ext_header->part_cnt;
	if (((lcd_debug_print_flag & LCD_DBG_PR_NORMAL) && init_flag) ||
	    (lcd_debug_print_flag & LCD_DBG_PR_ISR)) {
		LCDPR("%s: %s, part_cnt: %d\n", __func__, block_header->name, part_cnt);
	}

	block_ctrl_flag = block_header->block_ctrl;
	data_offset = LCD_TCON_DATA_BLOCK_HEADER_SIZE + block_header->ext_header_size;
	size = 0;
	for (i = 0; i < part_cnt; i++) {
		p = data_buf + data_offset;
		part_type = p[LCD_TCON_DATA_PART_NAME_SIZE + 3];
		if ((lcd_debug_print_flag & LCD_DBG_PR_ADV) && init_flag) {
			LCDPR("%s: start step %d, %s, type = 0x%02x,\n",
			      __func__, step, p, part_type);
		}
		switch (part_type) {
		case LCD_TCON_DATA_PART_TYPE_CONTROL:
			block_ctrl_flag = 0;
			data_part.ctrl = (struct lcd_tcon_data_part_ctrl_s *)p;
			offset = LCD_TCON_DATA_PART_CTRL_SIZE_PRE;
			size = offset + (data_part.ctrl->data_cnt *
					 data_part.ctrl->data_byte_width);
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			if (block_header->block_ctrl == 0)
				break;
			if (((lcd_debug_print_flag & LCD_DBG_PR_NORMAL) && init_flag) ||
			    (lcd_debug_print_flag & LCD_DBG_PR_ISR)) {
				LCDPR("%s: block %s: ctrl data_flag=0x%x, sub_type=0x%x\n",
				      __func__, block_header->name,
				      data_part.ctrl->ctrl_data_flag,
				      data_part.ctrl->ctrl_method);
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_WR_N:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.wr_n = (struct lcd_tcon_data_part_wr_n_s *)p;
			offset = LCD_TCON_DATA_PART_WR_N_SIZE_PRE;
			size = offset + (data_part.wr_n->reg_cnt *
					 data_part.wr_n->reg_addr_byte) +
					(data_part.wr_n->data_cnt *
					 data_part.wr_n->reg_data_byte);
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			reg_cnt = data_part.wr_n->reg_cnt;
			reg_byte = data_part.wr_n->reg_addr_byte;
			m = offset; /*for reg*/
			/*for data*/
			n = m + (reg_cnt * reg_byte);
			for (j = 0; j < reg_cnt; j++) {
				reg = 0;
				for (d = 0; d < reg_byte; d++)
					reg |= (p[m + d] << (d * 8));
				if (reg < reg_base)
					goto lcd_tcon_data_common_parse_set_err_reg;
				lcd_tcon_wr_n_data_write(pdrv, data_part.wr_n,
							 p, n, reg, init_flag);
				m += reg_byte;
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_WR_DDR:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.wr_ddr = (struct lcd_tcon_data_part_wr_ddr_s *)p;
			offset = LCD_TCON_DATA_PART_WR_DDR_SIZE_PRE;
			m = data_part.wr_ddr->data_cnt *
				data_part.wr_ddr->data_byte;
			size = offset + m;
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			n = data_part.wr_ddr->axi_buf_id;
			lcd_tcon_axi_rmem_lut_load(n, &p[offset], m);
			break;
		case LCD_TCON_DATA_PART_TYPE_WR_MASK:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.wr_mask = (struct lcd_tcon_data_part_wr_mask_s *)p;
			offset = LCD_TCON_DATA_PART_WR_MASK_SIZE_PRE;
			size = offset + data_part.wr_mask->reg_addr_byte +
				(2 * data_part.wr_mask->reg_data_byte);
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			reg_byte = data_part.wr_mask->reg_addr_byte;
			data_byte = data_part.wr_mask->reg_data_byte;
			m = offset; /* for reg */
			/* for data */
			n = m + reg_byte;
			reg = 0;
			for (d = 0; d < reg_byte; d++)
				reg |= (p[m + d] << (d * 8));
			if (reg < reg_base)
				goto lcd_tcon_data_common_parse_set_err_reg;
			mask = 0;
			for (d = 0; d < data_byte; d++)
				mask |= (p[n + d] << (d * 8));
			n += data_byte;
			data = 0;
			for (d = 0; d < data_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (data_byte == 1)
				lcd_tcon_update_bits_byte(pdrv, reg, mask, data);
			else
				lcd_tcon_update_bits(pdrv, reg, mask, data);
			if ((lcd_debug_print_flag & LCD_DBG_PR_ADV2) && init_flag) {
				LCDPR("%s: write reg 0x%x, data=0x%x, mask=0x%x\n",
				      __func__, reg, mask, data);
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_RD_MASK:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.rd_mask = (struct lcd_tcon_data_part_rd_mask_s *)p;
			offset = LCD_TCON_DATA_PART_RD_MASK_SIZE_PRE;
			size = offset + data_part.rd_mask->reg_addr_byte +
				data_part.rd_mask->reg_data_byte;
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			reg_byte = data_part.rd_mask->reg_addr_byte;
			data_byte = data_part.rd_mask->reg_data_byte;
			m = offset; /* for reg */
			/* for data */
			n = m + reg_byte;
			reg = 0;
			for (d = 0; d < reg_byte; d++)
				reg |= (p[m + d] << (d * 8));
			if (reg < reg_base)
				goto lcd_tcon_data_common_parse_set_err_reg;
			mask = 0;
			for (d = 0; d < data_byte; d++)
				mask |= (p[n + d] << (d * 8));
			if (data_byte == 1) {
				data = lcd_tcon_read_byte(pdrv, reg) & mask;
				if ((lcd_debug_print_flag & LCD_DBG_PR_ADV2) && init_flag)
					LCDPR("%s read reg 0x%04x = 0x%02x, mask = 0x%02x\n",
					      __func__, reg, data, mask);
			} else {
				data = lcd_tcon_read(pdrv, reg) & mask;
				if ((lcd_debug_print_flag & LCD_DBG_PR_ADV2) && init_flag)
					LCDPR("%s read reg 0x%04x = 0x%02x, mask = 0x%02x\n",
					      __func__, reg, data, mask);
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_CHK_WR_MASK:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.chk_wr_mask = (struct lcd_tcon_data_part_chk_wr_mask_s *)p;
			offset = LCD_TCON_DATA_PART_CHK_WR_MASK_SIZE_PRE;
			size = offset + data_part.chk_wr_mask->reg_chk_addr_byte +
				//include mask
				data_part.chk_wr_mask->reg_chk_data_byte *
				(data_part.chk_wr_mask->data_chk_cnt + 1) +
				data_part.chk_wr_mask->reg_wr_addr_byte +
				//include mask, default
				data_part.chk_wr_mask->reg_wr_data_byte *
				(data_part.chk_wr_mask->data_chk_cnt + 2);
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			reg_byte = data_part.chk_wr_mask->reg_chk_addr_byte;
			data_cnt = data_part.chk_wr_mask->data_chk_cnt;
			data_byte = data_part.chk_wr_mask->reg_chk_data_byte;
			m = offset; /* for reg */
			n = m + reg_byte; /* for data */
			reg = 0;
			for (d = 0; d < reg_byte; d++)
				reg |= (p[m + d] << (d * 8));
			if (reg < reg_base)
				goto lcd_tcon_data_common_parse_set_err_reg;
			mask = 0;
			for (d = 0; d < data_byte; d++)
				mask |= (p[n + d] << (d * 8));
			if (data_byte == 1)
				temp = lcd_tcon_read_byte(pdrv, reg) & mask;
			else
				temp = lcd_tcon_read(pdrv, reg) & mask;
			if ((lcd_debug_print_flag & LCD_DBG_PR_ADV2) && init_flag) {
				LCDPR("%s read reg 0x%04x = 0x%02x, mask = 0x%02x\n",
				      __func__, reg, temp, mask);
			}
			n += data_byte;
			for (j = 0; j < data_cnt; j++) {
				data = 0;
				for (d = 0; d < data_byte; d++)
					data |= (p[n + d] << (d * 8));
				if ((data & mask) == temp)
					break;
				n += data_byte;
			}
			k = j;

			/* for reg */
			m = offset + reg_byte + data_byte * (data_cnt + 1);
			/* for data */
			n = m + data_part.chk_wr_mask->reg_wr_addr_byte;
			reg_byte = data_part.chk_wr_mask->reg_wr_addr_byte;
			data_byte = data_part.chk_wr_mask->reg_wr_data_byte;
			reg = 0;
			for (d = 0; d < reg_byte; d++)
				reg |= (p[m + d] << (d * 8));
			if (reg < reg_base)
				goto lcd_tcon_data_common_parse_set_err_reg;
			mask = 0;
			for (d = 0; d < data_byte; d++)
				mask |= (p[n + d] << (d * 8));
			n += data_byte;
			n += data_byte * k;
			data = 0;
			for (d = 0; d < data_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (data_byte == 1)
				lcd_tcon_update_bits_byte(pdrv, reg, mask, data);
			else
				lcd_tcon_update_bits(pdrv, reg, mask, data);
			if ((lcd_debug_print_flag & LCD_DBG_PR_ADV2) && init_flag) {
				LCDPR("%s: write reg 0x%x, data=0x%x, mask=0x%x\n",
				      __func__, reg, mask, data);
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_CHK_EXIT:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.chk_exit = (struct lcd_tcon_data_part_chk_exit_s *)p;
			offset = LCD_TCON_DATA_PART_CHK_EXIT_SIZE_PRE;
			size = offset + data_part.chk_exit->reg_addr_byte +
				(2 * data_part.chk_exit->reg_data_byte);
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			reg_byte = data_part.chk_exit->reg_addr_byte;
			data_byte = data_part.chk_exit->reg_data_byte;
			m = offset; /* for reg */
			/* for data */
			n = m + reg_byte;
			reg = 0;
			for (d = 0; d < reg_byte; d++)
				reg |= (p[m + d] << (d * 8));
			if (reg < reg_base)
				goto lcd_tcon_data_common_parse_set_err_reg;
			mask = 0;
			for (d = 0; d < data_byte; d++)
				mask |= (p[n + d] << (d * 8));
			n += data_byte;
			data = 0;
			for (d = 0; d < data_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (data_byte == 1)
				ret = lcd_tcon_check_bits_byte(pdrv, reg, mask, data);
			else
				ret = lcd_tcon_check_bits(pdrv, reg, mask, data);
			if (ret) {
				LCDPR("%s: block %s data_part %d check exit\n",
				      __func__, block_header->name, i);
				return 0;
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_DELAY:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.delay = (struct lcd_tcon_data_part_delay_s *)p;
			size = LCD_TCON_DATA_PART_DELAY_SIZE;
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			if (init_flag == 0) /* calling by vsync isr */
				break;
			if (data_part.delay->delay_us > 20000) {
				m = data_part.delay->delay_us / 1000;
				n = data_part.delay->delay_us % 1000;
				msleep(m);
				if (n > 20)
					usleep_range(n, n + 1);
				else if (n > 0)
					udelay(n);
			} else {
				n = data_part.delay->delay_us;
				if (n > 20)
					usleep_range(n, n + 1);
				else if (n > 0)
					udelay(n);
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_PARAM:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.param = (struct lcd_tcon_data_part_param_s *)p;
			offset = LCD_TCON_DATA_PART_PARAM_SIZE_PRE;
			size = offset + data_part.param->param_size;
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			break;
		default:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			LCDERR("%s: unsupport data part type 0x%02x\n",
			       __func__, part_type);
			break;
		}
		if ((lcd_debug_print_flag & LCD_DBG_PR_ADV) && init_flag) {
			LCDPR("%s: end step %d, %s, type=0x%02x, size=%d\n",
			      __func__, step, p, part_type, size);
		}
		data_offset += size;
		step++;
	}

	return 0;

lcd_tcon_data_common_parse_set_ctrl_err:
	LCDERR("%s: block %s need control part\n", __func__, block_header->name);
	return -1;

lcd_tcon_data_common_parse_set_err_reg:
	LCDERR("%s: block %s step %d reg 0x%04x error\n",
	       __func__, block_header->name, step, reg);
	return -1;

lcd_tcon_data_common_parse_set_err_size:
	LCDERR("%s: block %s step %d size error\n",
	       __func__, block_header->name, step);
	return -1;
}

static int lcd_tcon_data_set(struct aml_lcd_drv_s *pdrv,
			     struct tcon_mem_map_table_s *mm_table)
{
	struct lcd_tcon_data_block_header_s *block_header;
	unsigned char *data_buf;
	unsigned int temp_crc32;
	unsigned int i, index, chk_size;
	int ret;

	if (!mm_table->data_mem_vaddr) {
		LCDERR("%s: data_mem error\n", __func__);
		return -1;
	}

	if (!mm_table->data_priority) {
		LCDERR("%s: data_priority is null\n", __func__);
		return -1;
	}

	for (i = 0; i < mm_table->block_cnt; i++) {
		index = mm_table->data_priority[i].index;
		if (index >= mm_table->block_cnt) {
			LCDERR("%s: data index %d is invalid\n",
			       __func__, index);
			return -1;
		}
		if (!mm_table->data_mem_vaddr[index]) {
			LCDERR("%s: data_mem_vaddr[%d] is null\n",
			       __func__, index);
			continue;
		}
		data_buf = mm_table->data_mem_vaddr[index];
		block_header = (struct lcd_tcon_data_block_header_s *)data_buf;
		if (block_header->block_size < sizeof(struct lcd_tcon_data_block_header_s)) {
			LCDERR("%s: block_size[%d] 0x%x is invalid\n",
			       __func__, index, block_header->block_size);
			continue;
		}
		chk_size = block_header->block_size - 4;
		temp_crc32 = cal_crc32(0, &data_buf[4], chk_size);
		if (temp_crc32 != block_header->crc32) {
			LCDERR("%s: block %d, %s data crc error\n",
				__func__, mm_table->data_priority[i].index,
				block_header->name);
			continue;
		}

		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR
	("%s: block %d, %s, priority %d: size=0x%x, type=0x%02x, ctrl=0x%x\n",
			      __func__, mm_table->data_priority[i].index,
			      block_header->name,
			      mm_table->data_priority[i].priority,
			      block_header->block_size,
			      block_header->block_type,
			      block_header->block_ctrl);
		}

		/* apply data */
		if (block_header->block_type == LCD_TCON_DATA_BLOCK_TYPE_BASIC_INIT) {
			lcd_tcon_data_init_set(pdrv, data_buf);
			continue;
		}

		if (block_header->block_ctrl == LCD_TCON_DATA_CTRL_FLAG_MULTI) {
			ret = lcd_tcon_data_multi_match_find(pdrv, data_buf);
			if (ret == 0) {
				ret = lcd_tcon_data_common_parse_set(pdrv, data_buf, 1);
				if (ret == 0) {
					lcd_tcon_data_multi_current_update(mm_table,
						block_header, index);
				}
			}
		} else {
			lcd_tcon_data_common_parse_set(pdrv, data_buf, 1);
		}
	}

	LCDPR("%s finish\n", __func__);
	return 0;
}

static int lcd_tcon_top_set_tl1(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	unsigned int axi_reg[3] = {
		TCON_AXI_OFST0, TCON_AXI_OFST1, TCON_AXI_OFST2
	};
	unsigned int paddr, p2p_type;
	int i;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("lcd tcon top set\n");

	if (tcon_rmem && tcon_rmem->flag) {
		if (!tcon_rmem->axi_rmem) {
			LCDERR("%s: invalid axi_mem\n", __func__);
		} else {
			for (i = 0; i < 3; i++) {
				paddr = tcon_rmem->axi_rmem[i].mem_paddr;
				lcd_tcon_write(pdrv, axi_reg[i], paddr);
				LCDPR("set tcon axi_mem paddr[%d]: 0x%08x\n",
				      i, paddr);
			}
		}
	} else {
		LCDERR("%s: invalid axi_mem\n", __func__);
	}

	lcd_tcon_write(pdrv, TCON_CLK_CTRL, 0x001f);
	if (pconf->basic.lcd_type == LCD_P2P) {
		p2p_type = pconf->control.p2p_cfg.p2p_type & 0x1f;
		switch (p2p_type) {
		case P2P_CHPI:
		case P2P_USIT:
			lcd_tcon_write(pdrv, TCON_TOP_CTRL, 0x8199);
			break;
		default:
			lcd_tcon_write(pdrv, TCON_TOP_CTRL, 0x8999);
			break;
		}
	} else {
		lcd_tcon_write(pdrv, TCON_TOP_CTRL, 0x8999);
	}
	lcd_tcon_write(pdrv, TCON_PLLLOCK_CNTL, 0x0037);
	lcd_tcon_write(pdrv, TCON_RST_CTRL, 0x003f);
	lcd_tcon_write(pdrv, TCON_RST_CTRL, 0x0000);
	lcd_tcon_write(pdrv, TCON_DDRIF_CTRL0, 0x33fff000);
	lcd_tcon_write(pdrv, TCON_DDRIF_CTRL1, 0x300300);

	return 0;
}

static int lcd_tcon_top_set_t5(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int p2p_type;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("lcd tcon top set\n");

	lcd_tcon_write(pdrv, TCON_CLK_CTRL, 0x001f);
	if (pconf->basic.lcd_type == LCD_P2P) {
		p2p_type = pconf->control.p2p_cfg.p2p_type & 0x1f;
		switch (p2p_type) {
		case P2P_CHPI:
		case P2P_USIT:
			lcd_tcon_write(pdrv, TCON_TOP_CTRL, 0x8399);
			break;
		default:
			lcd_tcon_write(pdrv, TCON_TOP_CTRL, 0x8b99);
			break;
		}
	} else {
		lcd_tcon_write(pdrv, TCON_TOP_CTRL, 0x8b99);
	}
	lcd_tcon_write(pdrv, TCON_PLLLOCK_CNTL, 0x0037);
	lcd_tcon_write(pdrv, TCON_RST_CTRL, 0x003f);
	lcd_tcon_write(pdrv, TCON_RST_CTRL, 0x0000);
	lcd_tcon_write(pdrv, TCON_DDRIF_CTRL0, 0x33fff000);
	lcd_tcon_write(pdrv, TCON_DDRIF_CTRL1, 0x300300);

	return 0;
}

void lcd_tcon_global_reset_t5(struct aml_lcd_drv_s *pdrv)
{
	/* global reset tcon */
	lcd_reset_setb(pdrv, RESET1_MASK_T5, 0, 4, 1);
	lcd_reset_setb(pdrv, RESET1_LEVEL_T5, 0, 4, 1);
	udelay(1);
	lcd_reset_setb(pdrv, RESET1_LEVEL_T5, 1, 4, 1);
	udelay(2);
}

void lcd_tcon_global_reset_t3(struct aml_lcd_drv_s *pdrv)
{
	/* global reset tcon */
	lcd_reset_setb(pdrv, RESETCTRL_RESET2_MASK, 0, 5, 1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET2_LEVEL, 0, 5, 1);
	udelay(1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET2_LEVEL, 1, 5, 1);
	udelay(2);
}

int lcd_tcon_gamma_pattern_tl1(struct aml_lcd_drv_s *pdrv,
			       unsigned int bit_width, unsigned int gamma_r,
			       unsigned int gamma_g, unsigned int gamma_b)
{
	unsigned int val_r = 0, val_g = 0, val_b = 0, reg;
	unsigned char temp[3];
	int i;

	switch (bit_width) {
	case 12:
		val_r = gamma_r;
		val_g = gamma_g;
		val_b = gamma_b;
		break;
	case 10:
		val_r = (gamma_r << 2);
		val_g = (gamma_g << 2);
		val_b = (gamma_b << 2);
		break;
	case 8:
		val_r = (gamma_r << 4);
		val_g = (gamma_g << 4);
		val_b = (gamma_b << 4);
		break;
	default:
		LCDERR("gamam_set: invalid bit_width %d\n", bit_width);
		return -1;
	}

	/* enable lut access, disable gamma en*/
	lcd_tcon_setb_byte(pdrv, 0x262, 0x2, 0, 2);

	/* gamma_r */
	/*2 data splice 3 bytes*/
	temp[0] = val_r & 0xff;
	temp[1] = ((val_r >> 8) & 0xf) | ((val_r & 0xf) << 4);
	temp[2] = (val_r >> 4) & 0xff;
	reg = 0xb50;
	for (i = 0; i < 384; i += 3) { /* 256 * 1.5 */
		lcd_tcon_write_byte(pdrv, (reg + i), temp[0]);
		lcd_tcon_write_byte(pdrv, (reg + i + 1), temp[1]);
		lcd_tcon_write_byte(pdrv, (reg + i + 2), temp[2]);
	}
	temp[0] = val_r & 0xff;
	temp[1] = ((val_r >> 8) & 0xf);
	temp[2] = 0;
	reg += 384;
	lcd_tcon_write_byte(pdrv, (reg), temp[0]);
	lcd_tcon_write_byte(pdrv, (reg + 1), temp[1]);
	lcd_tcon_write_byte(pdrv, (reg + 2), temp[2]);

	/* gamma_g */
	/*2 data splice 3 bytes*/
	temp[0] = val_g & 0xff;
	temp[1] = ((val_g >> 8) & 0xf) | ((val_g & 0xf) << 4);
	temp[2] = (val_g >> 4) & 0xff;
	reg += 3;
	for (i = 0; i < 384; i += 3) { /* 256 * 1.5 */
		lcd_tcon_write_byte(pdrv, (reg + i), temp[0]);
		lcd_tcon_write_byte(pdrv, (reg + i + 1), temp[1]);
		lcd_tcon_write_byte(pdrv, (reg + i + 2), temp[2]);
	}
	temp[0] = val_g & 0xff;
	temp[1] = ((val_g >> 8) & 0xf);
	temp[2] = 0;
	reg += 384;
	lcd_tcon_write_byte(pdrv, (reg), temp[0]);
	lcd_tcon_write_byte(pdrv, (reg + 1), temp[1]);
	lcd_tcon_write_byte(pdrv, (reg + 2), temp[2]);

	/* gamma_b */
	/*2 data splice 3 bytes*/
	temp[0] = val_b & 0xff;
	temp[1] = ((val_b >> 8) & 0xf) | ((val_b & 0xf) << 4);
	temp[2] = (val_b >> 4) & 0xff;
	reg += 3;
	for (i = 0; i < 384; i += 3) { /* 256 * 1.5 */
		lcd_tcon_write_byte(pdrv, (reg + i), temp[0]);
		lcd_tcon_write_byte(pdrv, (reg + i + 1), temp[1]);
		lcd_tcon_write_byte(pdrv, (reg + i + 2), temp[2]);
	}
	temp[0] = val_b & 0xff;
	temp[1] = ((val_b >> 8) & 0xf);
	temp[2] = 0;
	reg += 384;
	lcd_tcon_write_byte(pdrv, (reg), temp[0]);
	lcd_tcon_write_byte(pdrv, (reg + 1), temp[1]);
	lcd_tcon_write_byte(pdrv, (reg + 2), temp[2]);

	/* enable gamma */
	lcd_tcon_setb_byte(pdrv, 0x262, 0x3, 0, 2);

	return 0;
}

int lcd_tcon_gamma_pattern_t5(struct aml_lcd_drv_s *pdrv,
			      unsigned int bit_width, unsigned int gamma_r,
			      unsigned int gamma_g, unsigned int gamma_b)
{
	unsigned int val_r = 0, val_g = 0, val_b = 0;
	unsigned int temp;

	switch (bit_width) {
	case 10:
		val_r = gamma_r & 0x3ff;
		val_g = gamma_g & 0x3ff;
		val_b = gamma_b & 0x3ff;
		break;
	case 8:
		val_r = (gamma_r << 2) & 0x3ff;
		val_g = (gamma_g << 2) & 0x3ff;
		val_b = (gamma_b << 2) & 0x3ff;
		break;
	default:
		LCDERR("gamam_set: invalid bit_width %d\n", bit_width);
		return -1;
	}

	/* gamma calibrate mode */
	lcd_tcon_setb(pdrv, 0x222, 1, 13, 1);
	lcd_tcon_setb(pdrv, 0x222, 0, 12, 1);
	lcd_tcon_setb(pdrv, 0x222, 0, 8, 2);
	lcd_tcon_setb(pdrv, 0x201, 1, 31, 1); //test pattern enable

	/* gamma pattern */
	lcd_tcon_setb(pdrv, 0x201, 0, 26, 4);
	temp = (val_r << 0) | (val_g << 10) | (val_b << 20);
	lcd_tcon_setb(pdrv, 0x204, temp, 0, 30);

	return 0;
}

int lcd_tcon_enable_tl1(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	unsigned int p2p_type;
	int ret, flag;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;
	if (!tcon_conf)
		return -1;
	if (!mm_table)
		return -1;
	if (mm_table->init_load == 0) {
		if (mm_table->tcon_data_flag == 0)
			lcd_tcon_bin_load(pdrv);
	}

	/* step 1: tcon top */
	lcd_tcon_top_set_tl1(pdrv);

	/* step 2: tcon_core_reg_update */
	lcd_tcon_core_reg_pre_od(tcon_conf, mm_table);
	if (mm_table->core_reg_header) {
		if (mm_table->core_reg_header->block_ctrl == 0) {
			lcd_tcon_core_reg_set(pdrv, tcon_conf,
				mm_table, mm_table->core_reg_table);
		}
	}
	if (pconf->basic.lcd_type == LCD_P2P) {
		p2p_type = pconf->control.p2p_cfg.p2p_type & 0x1f;
		switch (p2p_type) {
		case P2P_CHPI:
			lcd_phy_tcon_chpi_bbc_init_tl1(pconf);
			break;
		default:
			break;
		}
	}
	if (mm_table->version == 0) {
		if (mm_table->valid_flag & LCD_TCON_DATA_VALID_VAC) {
			if (mm_table->valid_flag & LCD_TCON_DATA_VALID_DEMURA)
				flag = 1;
			else
				flag = 0;
			lcd_tcon_vac_set_tl1(pdrv, flag);
		}
		if (mm_table->valid_flag & LCD_TCON_DATA_VALID_DEMURA) {
			lcd_tcon_demura_set_tl1(pdrv);
			lcd_tcon_demura_lut_tl1(pdrv);
		}
		if (mm_table->valid_flag & LCD_TCON_DATA_VALID_ACC)
			lcd_tcon_acc_lut_tl1(pdrv);
	} else {
		lcd_tcon_data_set(pdrv, mm_table);
	}

	/* step 3: tcon_top_output_set */
	lcd_tcon_write(pdrv, TCON_OUT_CH_SEL0, 0x76543210);
	lcd_tcon_write(pdrv, TCON_OUT_CH_SEL1, 0xba98);

	/* step 4: tcon_intr_mask */
	lcd_tcon_write(pdrv, TCON_INTR_MASKN, TCON_INTR_MASKN_VAL);

	return 0;
}

int lcd_tcon_disable_tl1(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	unsigned int reg, i, cnt, offset, bit;

	if (!tcon_conf)
		return 0;

	/* disable tcon intr */
	lcd_tcon_write(pdrv, TCON_INTR_MASKN, 0);

	/* disable over_drive */
	if (tcon_conf->reg_core_od != REG_LCD_TCON_MAX) {
		reg = tcon_conf->reg_core_od;
		bit = tcon_conf->bit_od_en;
		if (tcon_conf->core_reg_width == 8)
			lcd_tcon_setb_byte(pdrv, reg, 0, bit, 1);
		else
			lcd_tcon_setb(pdrv, reg, 0, bit, 1);
		msleep(100);
	}

	/* disable all ctrl signal */
	if (tcon_conf->reg_ctrl_timing_base == REG_LCD_TCON_MAX)
		goto lcd_tcon_disable_tl1_next;
	reg = tcon_conf->reg_ctrl_timing_base;
	offset = tcon_conf->ctrl_timing_offset;
	cnt = tcon_conf->ctrl_timing_cnt;
	for (i = 0; i < cnt; i++) {
		if (tcon_conf->core_reg_width == 8)
			lcd_tcon_setb_byte(pdrv, (reg + (i * offset)), 1, 3, 1);
		else
			lcd_tcon_setb(pdrv, (reg + (i * offset)), 1, 3, 1);
	}

	/* disable top */
lcd_tcon_disable_tl1_next:
	if (tcon_conf->reg_top_ctrl != REG_LCD_TCON_MAX) {
		reg = tcon_conf->reg_top_ctrl;
		bit = tcon_conf->bit_en;
		lcd_tcon_setb(pdrv, reg, 0, bit, 1);
	}

	return 0;
}

int lcd_tcon_enable_t5(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;
	if (!tcon_conf)
		return -1;
	if (!mm_table)
		return -1;
	if (mm_table->init_load == 0) {
		if (mm_table->tcon_data_flag == 0)
			lcd_tcon_bin_load(pdrv);
	}

	lcd_vcbus_write(ENCL_VIDEO_EN, 0);

	/* step 1: tcon top */
	lcd_tcon_top_set_t5(pdrv);

	/* step 2: tcon_core_reg_update */
	lcd_tcon_core_reg_pre_dis_od(tcon_conf, mm_table);
	if (mm_table->core_reg_header) {
		if (mm_table->core_reg_header->block_ctrl == 0) {
			lcd_tcon_core_reg_set(pdrv, tcon_conf,
				mm_table, mm_table->core_reg_table);
		}
	}

	/* step 3: set axi rmem, must before tcon data */
	lcd_tcon_axi_rmem_set(pdrv, tcon_conf);

	/* step 4: tcon data set */
	if (mm_table->version)
		lcd_tcon_data_set(pdrv, mm_table);

	/* step 5: tcon_top_output_set */
	lcd_tcon_write(pdrv, TCON_OUT_CH_SEL0, 0x76543210);
	lcd_tcon_write(pdrv, TCON_OUT_CH_SEL1, 0xba98);

	/* step 6: tcon_intr_mask */
	lcd_tcon_write(pdrv, TCON_INTR_MASKN, TCON_INTR_MASKN_VAL);

	lcd_vcbus_write(ENCL_VIDEO_EN, 1);

	return 0;
}

int lcd_tcon_reload_t3(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();

	if (!mm_table || !tcon_conf)
		return -1;

	if (mm_table->core_reg_header) {
		if (mm_table->core_reg_header->block_ctrl == 0) {
			lcd_tcon_core_reg_set(pdrv, tcon_conf,
				mm_table, mm_table->core_reg_table);
		}
	}

	if (mm_table->version)
		lcd_tcon_data_set(pdrv, mm_table);

	return 0;
}

int lcd_tcon_enable_t3(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;
	if (!tcon_conf)
		return -1;
	if (!mm_table)
		return -1;
	if (mm_table->init_load == 0) {
		if (mm_table->tcon_data_flag == 0)
			lcd_tcon_bin_load(pdrv);
	}

	lcd_vcbus_write(ENCL_VIDEO_EN, 0);

	/* step 1: tcon top */
	lcd_tcon_top_set_t5(pdrv);

	/* step 2: tcon_core_reg_update */
	lcd_tcon_core_reg_pre_dis_od(tcon_conf, mm_table);
	if (mm_table->core_reg_header) {
		if (mm_table->core_reg_header->block_ctrl == 0) {
			lcd_tcon_core_reg_set(pdrv, tcon_conf,
				mm_table, mm_table->core_reg_table);
		}
	}

	/* step 3: set axi rmem, must before tcon data */
	lcd_tcon_axi_rmem_set(pdrv, tcon_conf);

	/* step 4: tcon data set */
	if (mm_table->version)
		lcd_tcon_data_set(pdrv, mm_table);

	/* step 5: tcon_top_output_set */
	lcd_tcon_write(pdrv, TCON_OUT_CH_SEL0, 0x76543210);
	lcd_tcon_write(pdrv, TCON_OUT_CH_SEL1, 0xba98);

	/* step 6: tcon_intr_mask */
	lcd_tcon_write(pdrv, TCON_INTR_MASKN, TCON_INTR_MASKN_VAL);

	lcd_vcbus_write(ENCL_VIDEO_EN, 1);

	return 0;
}

int lcd_tcon_disable_t5(struct aml_lcd_drv_s *pdrv)
{
	/* disable tcon intr */
	lcd_tcon_write(pdrv, TCON_INTR_MASKN, 0);

	/* disable od ddr_if */
	lcd_tcon_setb(pdrv, 0x263, 0, 31, 1);
	msleep(100);

	/* top reset */
	lcd_tcon_write(pdrv, TCON_RST_CTRL, 0x003f);

	lcd_tcon_global_reset_t5(pdrv);

	return 0;
}

int lcd_tcon_disable_t3(struct aml_lcd_drv_s *pdrv)
{
	/* disable tcon intr */
	lcd_tcon_write(pdrv, TCON_INTR_MASKN, 0);

	/* disable od ddr_if */
	lcd_tcon_setb(pdrv, 0x263, 0, 31, 1);
	msleep(100);

	/* top reset */
	lcd_tcon_write(pdrv, TCON_RST_CTRL, 0x003f);

	lcd_tcon_global_reset_t3(pdrv);

	return 0;
}
