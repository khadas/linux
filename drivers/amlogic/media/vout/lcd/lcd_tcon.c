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
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "lcd_common.h"
#include "lcd_reg.h"
#include "lcd_tcon.h"
/*#include "tcon_ceds.h"*/

#define TCON_INTR_MASKN_VAL    0x0  /* default mask all */

static struct lcd_tcon_config_s *lcd_tcon_conf;
static struct tcon_rmem_s tcon_rmem;
static struct tcon_mem_map_table_s tcon_mm_table;
static struct delayed_work lcd_tcon_delayed_work;

static int lcd_tcon_valid_check(void)
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

unsigned int lcd_tcon_reg_read(unsigned int addr)
{
	unsigned int val;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return 0;

	if (addr < TCON_TOP_BASE) {
		if (lcd_tcon_conf->core_reg_width == 8)
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
		if (lcd_tcon_conf->core_reg_width == 8) {
			temp = (unsigned char)val;
			lcd_tcon_write_byte(addr, temp);
		} else {
			lcd_tcon_write(addr, val);
		}
	} else {
		lcd_tcon_write(addr, val);
	}
}

static void lcd_tcon_od_check(unsigned char *table)
{
	unsigned int reg, bit;

	if (lcd_tcon_conf->reg_core_od == REG_LCD_TCON_MAX)
		return;

	reg = lcd_tcon_conf->reg_core_od;
	bit = lcd_tcon_conf->bit_od_en;
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

	len = lcd_tcon_conf->reg_table_len;
	table = lcd_tcon_conf->reg_table;
	if (!table) {
		LCDERR("%s: table is NULL\n", __func__);
		return;
	}
	lcd_tcon_od_check(table);
	if (lcd_tcon_conf->core_reg_width == 8) {
		for (i = 0; i < len; i++) {
			lcd_tcon_write_byte((i + TCON_CORE_REG_START),
					    table[i]);
		}
	} else {
		for (i = 0; i < len; i++)
			lcd_tcon_write((i + TCON_CORE_REG_START), table[i]);
	}
	LCDPR("tcon core regs update\n");

	if (lcd_tcon_conf->reg_core_od != REG_LCD_TCON_MAX) {
		i = lcd_tcon_conf->reg_core_od;
		if (lcd_tcon_conf->core_reg_width == 8)
			temp = lcd_tcon_read_byte(i + TCON_CORE_REG_START);
		else
			temp = lcd_tcon_read(i + TCON_CORE_REG_START);
		LCDPR("%s: tcon od reg readback: 0x%04x = 0x%04x\n",
		      __func__, i, temp);
	}
}

static unsigned char *lcd_tcon_paddrtovaddr(unsigned long paddr,
					    unsigned int mem_size)
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
	if (highmem_flag) {
		i = mem_size % SZ_1M;
		if (i)
			max_mem_size = (mem_size / SZ_1M) + 1;
		vaddr = lcd_vmap(paddr, SZ_1M * max_mem_size);
		if (!vaddr) {
			LCDPR("tcon paddr mapping error: addr: 0x%lx\n",
			      (unsigned long)paddr);
			return NULL;
		}
	} else {
		vaddr = phys_to_virt(paddr);
		if (!vaddr) {
			LCDERR("tcon vaddr mapping failed: 0x%lx\n",
			       (unsigned long)paddr);
			return NULL;
		}
	}
	return (unsigned char *)vaddr;
}

static void lcd_tcon_vac_set_tl1(unsigned int demura_valid)
{
	int len, i, j, n;
	unsigned int d0, d1, temp, set0, set1, set2;
	unsigned char *buf = tcon_mm_table.vac_mem_vaddr;

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
	if (lcd_debug_print_flag)
		LCDPR("vac_set: 0x%x, 0x%x, 0x%x\n", set0, set1, set2);

	lcd_tcon_write_byte(0x0267, lcd_tcon_read_byte(0x0267) | 0xa0);
	/*vac_cntl, 12pipe delay temp for pre_dt*/
	lcd_tcon_write(0x2800, 0x807);
	if (demura_valid)
		lcd_tcon_write(0x2817, (0x1e | ((set1 & 0xff) << 8)));
	else
		lcd_tcon_write(0x2817, (0x1e | ((set0 & 0xff) << 8)));

	len = TCON_VAC_LUT_PARAM_NUM;
	if (lcd_debug_print_flag)
		LCDPR("%s: start write vac_ramt1~2\n", __func__);
	/*write vac_ramt1: 8bit, 256 regs*/
	for (i = 0; i < len; i++)
		lcd_tcon_write_byte(0xa100 + i, buf[n + i * 2]);

	for (i = 0; i < len; i++)
		lcd_tcon_write_byte(0xa200 + i, buf[n + i * 2]);

	/*write vac_ramt2: 8bit, 256 regs*/
	n += (len * 2);
	for (i = 0; i < len; i++)
		lcd_tcon_write_byte(0xa300 + i, buf[n + i * 2]);

	for (i = 0; i < len; i++)
		lcd_tcon_write_byte(0xbc00 + i, buf[n + i * 2]);

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
			d0 = (buf[n + (i * 4)] |
				(buf[n + (i * 4 + 1)] << 8)) & 0xfff;
			d1 = (buf[n + (i * 4 + 2)] |
				(buf[n + (i * 4 + 3)] << 8)) & 0xfff;
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

	LCDPR("tcon vac finish\n");
}

static int lcd_tcon_demura_set_tl1(void)
{
	unsigned char *data_buf;
	unsigned int data_cnt, i;

	if (!tcon_mm_table.demura_set_mem_vaddr) {
		LCDERR("%s: demura_set_mem_vaddr is null\n", __func__);
		return -1;
	}

	if (lcd_tcon_getb_byte(0x23d, 0, 1) == 0) {
		if (lcd_debug_print_flag)
			LCDPR("%s: demura function disabled\n", __func__);
		return 0;
	}

	if (lcd_debug_print_flag == 3) {
		for (i = 0; i < 30; i++)
			LCDPR("demura_set data[%d]: 0x%x\n",
			      i, tcon_mm_table.demura_set_mem_vaddr[i]);
	}

	/*demura_setting:
	 *  8byte chk_data + demura_set_data: 161byte
	 */
	data_cnt = (tcon_mm_table.demura_set_mem_vaddr[0] |
		    (tcon_mm_table.demura_set_mem_vaddr[1] << 8) |
		    (tcon_mm_table.demura_set_mem_vaddr[2] << 16) |
		    (tcon_mm_table.demura_set_mem_vaddr[3] << 24));
	data_buf = &tcon_mm_table.demura_set_mem_vaddr[8];
	for (i = 0; i < data_cnt; i++)
		lcd_tcon_write_byte(0x186, data_buf[i]);

	LCDPR("tcon demura_set cnt %d\n", data_cnt);

	return 0;
}

static int lcd_tcon_demura_lut_tl1(void)
{
	unsigned char *data_buf;
	unsigned int data_cnt, i;

	if (!tcon_mm_table.demura_lut_mem_vaddr) {
		LCDERR("%s: demura_lut_mem_vaddr is null\n", __func__);
		return -1;
	}

	if (lcd_tcon_getb_byte(0x23d, 0, 1) == 0) {
		if (lcd_debug_print_flag)
			LCDPR("%s: demura function disabled\n", __func__);
		return 0;
	}

	/*disable demura when load lut data*/
	lcd_tcon_setb_byte(0x23d, 0, 0, 1);

	lcd_tcon_setb_byte(0x181, 1, 0, 1);
	lcd_tcon_write_byte(0x182, 0x01);
	lcd_tcon_write_byte(0x183, 0x86);
	lcd_tcon_write_byte(0x184, 0x01);
	lcd_tcon_write_byte(0x185, 0x87);

	if (lcd_debug_print_flag == 3) {
		for (i = 0; i < 30; i++)
			LCDPR("demura_lut data[%d]: 0x%x\n",
			      i, tcon_mm_table.demura_lut_mem_vaddr[i]);
	}

	/*demura_lut is
	 *chk_data: 8byte + demura_lut_data: 391053byte
	 */
	data_cnt = (tcon_mm_table.demura_lut_mem_vaddr[0] |
		    (tcon_mm_table.demura_lut_mem_vaddr[1] << 8) |
		    (tcon_mm_table.demura_lut_mem_vaddr[2] << 16) |
		    (tcon_mm_table.demura_lut_mem_vaddr[3] << 24));
	data_buf = &tcon_mm_table.demura_lut_mem_vaddr[8];
	/* fixed 2 byte 0 for border */
	lcd_tcon_write_byte(0x187, 0);
	lcd_tcon_write_byte(0x187, 0);
	for (i = 0; i < data_cnt; i++)
		lcd_tcon_write_byte(0x187, data_buf[i]);

	/*enable demura when load lut data finished*/
	lcd_tcon_setb_byte(0x23d, 1, 0, 1);

	LCDPR("tcon demura_lut cnt %d\n", data_cnt);
	LCDPR("lcd_tcon 0x23d = 0x%02x\n", lcd_tcon_read_byte(0x23d));

	return 0;
}

static int lcd_tcon_acc_lut_tl1(void)
{
	unsigned char *data_buf;
	unsigned int data_cnt, i;

	if (!tcon_mm_table.acc_lut_mem_vaddr) {
		LCDERR("%s: acc_lut_mem_vaddr is null\n", __func__);
		return -1;
	}

	/* enable lut access, disable gamma en*/
	lcd_tcon_setb_byte(0x262, 0x2, 0, 2);

	/* write gamma lut */
	/*acc_lut is
	 *chk_data: 8byte + acc_lut_data: 1158byte
	 */
	data_cnt = (tcon_mm_table.acc_lut_mem_vaddr[0] |
		    (tcon_mm_table.acc_lut_mem_vaddr[1] << 8) |
		    (tcon_mm_table.acc_lut_mem_vaddr[2] << 16) |
		    (tcon_mm_table.acc_lut_mem_vaddr[3] << 24));
	if (data_cnt > 1161) { /* 0xb50~0xfd8, 1161 */
		LCDPR("%s: data_cnt %d is invalid, force to 1161\n",
		      __func__, data_cnt);
		data_cnt = 1161;
	}

	data_buf = &tcon_mm_table.acc_lut_mem_vaddr[8];
	for (i = 0; i < data_cnt; i++)
		lcd_tcon_write_byte((0xb50 + i), data_buf[i]);

	/* enable gamma */
	lcd_tcon_setb_byte(0x262, 0x3, 0, 2);

	LCDPR("tcon acc_lut cnt %d\n", data_cnt);

	return 0;
}

static int lcd_tcon_wr_n_data_write(struct lcd_tcon_data_part_wr_n_s *wr_n,
				    unsigned char *p,
				    unsigned int n,
				    unsigned int reg)
{
	unsigned int k, data, d;

	if (wr_n->reg_inc) {
		for (k = 0; k < wr_n->data_cnt; k++) {
			data = 0;
			for (d = 0; d < wr_n->reg_data_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (wr_n->reg_data_byte == 1)
				lcd_tcon_write_byte((reg + k), data);
			else
				lcd_tcon_write((reg + k), data);
			n += wr_n->reg_data_byte;
		}
	} else {
		for (k = 0; k < wr_n->data_cnt; k++) {
			data = 0;
			for (d = 0; d < wr_n->reg_data_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (wr_n->reg_data_byte == 1)
				lcd_tcon_write_byte(reg, data);
			else
				lcd_tcon_write(reg, data);
			n += wr_n->reg_data_byte;
		}
	}

	return 0;
}

static int lcd_tcon_data_common_parse_set(unsigned char *data_buf,
					  struct lcd_tcon_data_block_header_s
					  *block_header)
{
	unsigned char *p;
	unsigned short part_cnt;
	unsigned char part_type;
	unsigned int size, reg, data, mask, temp;
	struct lcd_tcon_data_block_ext_header_s *ext_header;
	union lcd_tcon_data_part_u data_part;
	unsigned int data_offset, offset, i, j, k, d, m, n, step = 0;
	int ret;

	p = data_buf + LCD_TCON_DATA_BLOCK_HEADER_SIZE;
	ext_header = (struct lcd_tcon_data_block_ext_header_s *)p;
	part_cnt = ext_header->part_cnt;
	if (lcd_debug_print_flag)
		LCDPR("%s: part_cnt: %d\n", __func__, part_cnt);

	data_offset = LCD_TCON_DATA_BLOCK_HEADER_SIZE + block_header->ext_header_size;
	size = 0;
	for (i = 0; i < part_cnt; i++) {
		p = data_buf + data_offset;
		part_type = p[LCD_TCON_DATA_PART_NAME_SIZE + 3];
		if (lcd_debug_print_flag) {
			LCDPR("%s: start step %d, %s, type = 0x%02x,\n",
			      __func__, step, p, part_type);
		}
		switch (part_type) {
		case LCD_TCON_DATA_PART_TYPE_WR_N:
			data_part.wr_n = (struct lcd_tcon_data_part_wr_n_s *)p;
			offset = LCD_TCON_DATA_PART_WR_N_SIZE_PRE;
			size = offset + (data_part.wr_n->reg_cnt *
					 data_part.wr_n->reg_addr_byte) +
					(data_part.wr_n->data_cnt *
					 data_part.wr_n->reg_data_byte);
			if ((size + data_offset) > block_header->block_size) {
				LCDERR("%s: block %s data_part %d error\n",
				       __func__, block_header->name, i);
				return -1;
			}
			m = offset; /*for reg*/
			/*for data*/
			n = m + (data_part.wr_n->reg_cnt *
				 data_part.wr_n->reg_addr_byte);
			for (j = 0; j < data_part.wr_n->reg_cnt; j++) {
				reg = 0;
				for (d = 0; d < data_part.wr_n->reg_addr_byte;
				     d++)
					reg |= (p[m + d] << (d * 8));
				lcd_tcon_wr_n_data_write(data_part.wr_n, p, n, reg);
				m += data_part.wr_n->reg_addr_byte;
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_WR_MASK:
			data_part.wr_mask = (struct lcd_tcon_data_part_wr_mask_s *)p;
			offset = LCD_TCON_DATA_PART_WR_MASK_SIZE_PRE;
			size = offset + data_part.wr_mask->reg_addr_byte +
				(2 * data_part.wr_mask->reg_data_byte);
			if ((size + data_offset) > block_header->block_size) {
				LCDERR("%s: block %s data_part %d error\n",
				       __func__, block_header->name, i);
				return -1;
			}

			m = offset; /* for reg */
			/* for data */
			n = m + data_part.wr_mask->reg_addr_byte;
			reg = 0;
			for (d = 0; d < data_part.wr_mask->reg_addr_byte; d++)
				reg |= (p[m + d] << (d * 8));
			mask = 0;
			for (d = 0; d < data_part.wr_mask->reg_addr_byte; d++)
				mask |= (p[n + d] << (d * 8));
			n += data_part.wr_mask->reg_data_byte;
			data = 0;
			for (d = 0; d < data_part.wr_mask->reg_addr_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (data_part.wr_mask->reg_data_byte == 1)
				lcd_tcon_update_bits_byte(reg, mask, data);
			else
				lcd_tcon_update_bits(reg, mask, data);
			break;
		case LCD_TCON_DATA_PART_TYPE_RD_MASK:
			data_part.rd_mask = (struct lcd_tcon_data_part_rd_mask_s *)p;
			offset = LCD_TCON_DATA_PART_RD_MASK_SIZE_PRE;
			size = offset + data_part.rd_mask->reg_addr_byte +
				data_part.rd_mask->reg_data_byte;
			if ((size + data_offset) > block_header->block_size) {
				LCDERR("%s: block %s data_part %d error\n",
				       __func__, block_header->name, i);
				return -1;
			}
			m = offset; /* for reg */
			/* for data */
			n = m + data_part.rd_mask->reg_addr_byte;
			reg = 0;
			for (d = 0; d < data_part.rd_mask->reg_addr_byte;
			     d++)
				reg |= (p[m + d] << (d * 8));
			mask = 0;
			for (d = 0; d < data_part.rd_mask->reg_data_byte; d++)
				mask |= (p[n + d] << (d * 8));
			if (data_part.rd_mask->reg_data_byte == 1) {
				data = lcd_tcon_read_byte(reg) & mask;
				if (lcd_debug_print_flag)
					LCDPR("%s read reg 0x%04x = 0x%02x, mask = 0x%02x\n",
					      __func__, reg, data, mask);
			} else {
				data = lcd_tcon_read(reg) & mask;
				if (lcd_debug_print_flag)
					LCDPR("%s read reg 0x%04x = 0x%02x, mask = 0x%02x\n",
					      __func__, reg, data, mask);
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_CHK_WR_MASK:
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
			if ((size + data_offset) > block_header->block_size) {
				LCDERR("%s: block %s data_part %d error\n",
				       __func__, block_header->name, i);
				return -1;
			}
			m = offset; /* for reg */
			n = m + data_part.chk_wr_mask->reg_chk_addr_byte; /* for data */
			reg = 0;
			for (d = 0; d < data_part.chk_wr_mask->reg_chk_addr_byte; d++)
				reg |= (p[m + d] << (d * 8));
			mask = 0;
			for (d = 0; d < data_part.chk_wr_mask->reg_chk_data_byte; d++)
				mask |= (p[n + d] << (d * 8));
			if (data_part.chk_wr_mask->reg_chk_data_byte == 1)
				temp = lcd_tcon_read_byte(reg) & mask;
			else
				temp = lcd_tcon_read(reg) & mask;
			n += data_part.chk_wr_mask->reg_chk_data_byte;
			for (j = 0; j < data_part.chk_wr_mask->data_chk_cnt; j++) {
				data = 0;
				for (d = 0; d < data_part.chk_wr_mask->reg_chk_data_byte; d++)
					data |= (p[n + d] << (d * 8));
				if ((data & mask) == temp)
					break;
				n += data_part.chk_wr_mask->reg_chk_data_byte;
			}
			k = j;

			/* for reg */
			m = offset + data_part.chk_wr_mask->reg_chk_addr_byte +
			    data_part.chk_wr_mask->reg_chk_data_byte *
			    (data_part.chk_wr_mask->data_chk_cnt + 1);
			/* for data */
			n = m + data_part.chk_wr_mask->reg_wr_addr_byte;
			reg = 0;
			for (d = 0; d < data_part.chk_wr_mask->reg_wr_addr_byte; d++)
				reg |= (p[m + d] << (d * 8));
			mask = 0;
			for (d = 0; d < data_part.chk_wr_mask->reg_wr_data_byte; d++)
				mask |= (p[n + d] << (d * 8));
			n += data_part.chk_wr_mask->reg_wr_data_byte;
			n += data_part.chk_wr_mask->reg_wr_data_byte * k;
			data = 0;
			for (d = 0; d < data_part.chk_wr_mask->reg_wr_data_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (data_part.chk_wr_mask->reg_wr_data_byte == 1)
				lcd_tcon_update_bits_byte(reg, mask, data);
			else
				lcd_tcon_update_bits(reg, mask, data);
			break;
		case LCD_TCON_DATA_PART_TYPE_CHK_EXIT:
			data_part.chk_exit =
				(struct lcd_tcon_data_part_chk_exit_s *)p;
			offset = LCD_TCON_DATA_PART_CHK_EXIT_SIZE_PRE;
			size = offset + data_part.chk_exit->reg_addr_byte +
				(2 * data_part.chk_exit->reg_data_byte);
			if ((size + data_offset) > block_header->block_size) {
				LCDERR("%s: block %s data_part %d error\n",
				       __func__, block_header->name, i);
				return -1;
			}
			m = offset; /* for reg */
			/* for data */
			n = m + data_part.chk_exit->reg_addr_byte;
			reg = 0;
			for (d = 0; d < data_part.chk_exit->reg_addr_byte; d++)
				reg |= (p[m + d] << (d * 8));
			mask = 0;
			for (d = 0; d < data_part.chk_exit->reg_addr_byte; d++)
				mask |= (p[n + d] << (d * 8));
			n += data_part.chk_exit->reg_data_byte;
			data = 0;
			for (d = 0; d < data_part.chk_exit->reg_data_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (data_part.chk_exit->reg_data_byte == 1)
				ret = lcd_tcon_check_bits_byte(reg, mask, data);
			else
				ret = lcd_tcon_check_bits(reg, mask, data);
			if (ret) {
				LCDPR("%s: block %s data_part %d check exit\n",
				      __func__, block_header->name, i);
				return 0;
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_DELAY:
			data_part.delay = (struct lcd_tcon_data_part_delay_s *)p;
			size = LCD_TCON_DATA_PART_DELAY_SIZE;
			if ((size + data_offset) > block_header->block_size) {
				LCDERR("%s: block %s data_part %d error\n",
				       __func__, block_header->name, i);
				return -1;
			}
			if (data_part.delay->delay_us)
				mdelay(data_part.delay->delay_us);
			break;
		case LCD_TCON_DATA_PART_TYPE_PARAM:
			data_part.param = (struct lcd_tcon_data_part_param_s *)p;
			offset = LCD_TCON_DATA_PART_PARAM_SIZE_PRE;
			size = offset + data_part.param->param_size;
			if ((size + data_offset) > block_header->block_size) {
				LCDERR("%s: block %s data_part %d error\n",
				       __func__, block_header->name, i);
				return -1;
			}
			break;
		default:
			LCDERR("%s: unsupport dat part type 0x%02x\n",
			       __func__, part_type);
			break;
		}
		if (lcd_debug_print_flag) {
			LCDPR("%s: end step %d, %s, type=0x%02x, size=%d\n",
			      __func__, step, p, part_type, size);
		}
		data_offset += size;
		step++;
	}

	return 0;
}

static int lcd_tcon_data_set(void)
{
	struct lcd_tcon_data_block_header_s block_header;
	unsigned char *data_buf;
	unsigned char temp_checksum, temp_lrc;
	int i;

	if (!tcon_mm_table.data_mem_vaddr) {
		LCDERR("%s: data_mem error\n", __func__);
		return -1;
	}

	if (!tcon_mm_table.data_priority) {
		LCDERR("%s: data_priority is null\n", __func__);
		return -1;
	}

	for (i = 0; i < tcon_mm_table.block_cnt; i++) {
		if (tcon_mm_table.data_priority[i].index >=
		     tcon_mm_table.block_cnt ||
		     tcon_mm_table.data_priority[i].priority == 0xff) {
			LCDERR("%s: data index or priority is invalid\n",
			       __func__);
			return -1;
		}
		data_buf =
	tcon_mm_table.data_mem_vaddr[tcon_mm_table.data_priority[i].index];
		memcpy(&block_header, data_buf, LCD_TCON_DATA_BLOCK_HEADER_SIZE);
		temp_checksum = lcd_tcon_checksum
			(&data_buf[4], (block_header.block_size - 4));
		temp_lrc = lcd_tcon_lrc(&data_buf[4],
					(block_header.block_size - 4));
		if (temp_checksum != block_header.checksum ||
		    temp_lrc != block_header.lrc ||
		    data_buf[2] != 0x55 ||
		    data_buf[3] != 0xaa) {
			LCDERR("%s: block %d, %s data check error\n",
			       __func__, tcon_mm_table.data_priority[i].index,
			       block_header.name);
			continue;
		}

		if (lcd_debug_print_flag) {
			LCDPR
	("%s: block %d, %s, size=0x%x, type=0x%02x(%s), init_priority: %d\n",
			      __func__, tcon_mm_table.data_priority[i].index,
			      block_header.name,
			      block_header.block_size,
			      block_header.block_type,
			      block_header.name,
			      block_header.init_priority);
		}
		lcd_tcon_data_common_parse_set(data_buf, &block_header);
	}

	LCDPR("%s finish\n", __func__);
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
		addr[0] = tcon_rmem.rsv_mem_paddr;
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
		case P2P_USIT:
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
	unsigned char *buff = tcon_mm_table.vac_mem_vaddr;
	unsigned int data_cnt;
	unsigned char data_checksum, data_lrc, temp_checksum, temp_lrc;

	if (tcon_mm_table.vac_mem_size == 0 || !buff)
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

	if (lcd_debug_print_flag == 3) {
		for (n = 0; n < 30; n++)
			LCDPR("%s: vac_data[%d]: 0x%x", __func__,
			      n, buff[n * 1]);
	}

	return 0;
}

static int lcd_tcon_demura_set_load(void)
{
	unsigned int n;
	char *buff = tcon_mm_table.demura_set_mem_vaddr;
	unsigned int data_cnt;
	unsigned char data_checksum, data_lrc, temp_checksum, temp_lrc;

	if (tcon_mm_table.demura_set_mem_size == 0 || !buff)
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

	if (lcd_debug_print_flag == 3) {
		for (n = 0; n < 30; n++)
			LCDPR("%s: demura_set[%d]: 0x%x",
			      __func__, n, buff[n]);
	}

	return 0;
}

static int lcd_tcon_demura_lut_load(void)
{
	unsigned int n;
	char *buff = tcon_mm_table.demura_lut_mem_vaddr;
	unsigned int data_cnt;
	unsigned char data_checksum, data_lrc, temp_checksum, temp_lrc;

	if (tcon_mm_table.demura_lut_mem_size == 0 || !buff)
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

	if (lcd_debug_print_flag == 3) {
		for (n = 0; n < 30; n++)
			LCDPR("%s: demura_lut[%d]: 0x%x\n",
			      __func__, n, buff[n]);
	}

	return 0;
}

static int lcd_tcon_acc_lut_load(void)
{
	unsigned int n;
	char *buff = tcon_mm_table.acc_lut_mem_vaddr;
	unsigned int data_cnt;
	unsigned char data_checksum, data_lrc, temp_checksum, temp_lrc;

	if (tcon_mm_table.acc_lut_mem_size == 0 || !buff)
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

	if (lcd_debug_print_flag == 3) {
		for (n = 0; n < 30; n++)
			LCDPR("%s: acc_lut[%d]: 0x%x\n",
			      __func__, n, buff[n]);
	}

	return 0;
}

static int lcd_tcon_data_load(void)
{
	unsigned char *table = lcd_tcon_conf->reg_table;
	struct file *filp = NULL;
	mm_segment_t old_fs = get_fs();
	loff_t pos = 0;
	int ret;
	int data_cnt = 0;
	struct lcd_tcon_data_block_header_s block_header;
	struct tcon_data_priority_s *data_prio;
	int i, j, demura_cnt = 0;
	int n = 0;

	if (!table) {
		LCDERR("%s: no tcon bin table\n", __func__);
		return -1;
	}
	LCDPR("%s\n", __func__);

	if (tcon_mm_table.version == 0) {
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
				tcon_mm_table.valid_flag |=
					LCD_TCON_DATA_VALID_DEMURA;
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
		data_prio = tcon_mm_table.data_priority;

		for (i = 0; i < tcon_mm_table.block_cnt; i++) {
			n = 32 + i * 256;
			set_fs(KERNEL_DS);
			filp = filp_open
			((char *)&tcon_rmem.bin_path_mem_vaddr[n], O_RDONLY, 0);
			if (IS_ERR(filp)) {
				LCDERR("%s: read %s error\n", __func__,
				       &tcon_rmem.bin_path_mem_vaddr[n]);
				set_fs(old_fs);
				return -1;
			}
			data_cnt = vfs_read(filp, (void *)&block_header,
					    LCD_TCON_DATA_BLOCK_HEADER_SIZE, &pos);
			if (data_cnt != LCD_TCON_DATA_BLOCK_HEADER_SIZE) {
				LCDERR("read block head failed, data_cnt: %d\n",
				       data_cnt);
				vfs_fsync(filp, 0);
				filp_close(filp, NULL);
				set_fs(old_fs);
				continue;
			}

			tcon_mm_table.data_mem_vaddr[i] =
				kmalloc(block_header.block_size *
					sizeof(unsigned char),
					GFP_KERNEL);
			if (!tcon_mm_table.data_mem_vaddr[i]) {
				LCDERR("%s: Not enough memory\n", __func__);
				continue;
			}
			data_cnt =
			vfs_read(filp, (char *)tcon_mm_table.data_mem_vaddr[i],
				 block_header.block_size, &pos);
			if (data_cnt != block_header.block_size) {
				LCDERR("%s: data_cnt: %d, data_size: %d\n",
				       __func__, data_cnt,
				       block_header.block_size);
				vfs_fsync(filp, 0);
				filp_close(filp, NULL);
				set_fs(old_fs);
				continue;
			}

			tcon_mm_table.valid_flag |= block_header.block_flag;
			if (block_header.block_flag ==
			    LCD_TCON_DATA_VALID_DEMURA)
				demura_cnt++;

			/* insertion sort for block data init_priority */
			data_prio[i].index = i;
			data_prio[i].priority = block_header.init_priority;
			if (i > 0) {
				j = i - 1;
				while (j >= 0) {
					if (block_header.init_priority >
					    data_prio[j].priority)
						break;
					if (block_header.init_priority ==
					    data_prio[j].priority) {
						LCDERR
				("%s: block %d init_prio same as block %d\n",
				 __func__, data_prio[i].index,
				 data_prio[j].index);
						return -1;
					}
					data_prio[j + 1].index =
						data_prio[j].index;
					data_prio[j + 1].priority =
						data_prio[j].priority;
					j--;
				}
				data_prio[j + 1].index = i;
				data_prio[j + 1].priority =
					block_header.init_priority;
			}

			if (lcd_debug_print_flag) {
				LCDPR
		("%s %d: size=0x%x, type=0x%02x, name=%s, init_priority=%d\n",
				      __func__, i,
				      block_header.block_size,
				      block_header.block_type,
				      block_header.name,
				      block_header.init_priority);
			}
		}

		/* specially check demura setting */
		if (demura_cnt < 2) {
			tcon_mm_table.valid_flag &= ~LCD_TCON_DATA_VALID_DEMURA;
			/* disable demura */
			table[0x178] = 0x38;
			table[0x17c] = 0x20;
			table[0x181] = 0x00;
			table[0x23d] &= ~(1 << 0);
		}
	}

	tcon_mm_table.tcon_data_flag = 1;

	return 0;
}

static int lcd_tcon_bin_path_update(unsigned int size)
{
	unsigned char *mem_vaddr;
	unsigned int data_size;
	unsigned char data_checksum, data_lrc, temp_checksum, temp_lrc;

	if (!tcon_rmem.bin_path_mem_vaddr) {
		LCDERR("%s: get mem error\n", __func__);
		return -1;
	}
	mem_vaddr = tcon_rmem.bin_path_mem_vaddr;
	data_size = mem_vaddr[4] |
		(mem_vaddr[5] << 8) |
		(mem_vaddr[6] << 16) |
		(mem_vaddr[7] << 24);
	if (data_size < 4) {
		LCDERR("%s: tcon_bin_path data_size error\n", __func__);
		return -1;
	}
	data_checksum = mem_vaddr[0];
	data_lrc = mem_vaddr[1];
	temp_checksum = lcd_tcon_checksum(&mem_vaddr[4], (data_size - 4));
	temp_lrc = lcd_tcon_lrc(&mem_vaddr[4], (data_size - 4));
	if (data_checksum != temp_checksum) {
		LCDERR("%s: tcon_bin_path checksum error\n", __func__);
		return -1;
	}
	if (data_lrc != temp_lrc) {
		LCDERR("%s: tcon_bin_path lrc error\n", __func__);
		return -1;
	}
	if (mem_vaddr[2] != 0x55 || mem_vaddr[3] != 0xaa) {
		LCDERR("%s: tcon_bin_path pattern error\n", __func__);
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
	tcon_mm_table.block_cnt = mem_vaddr[16] |
		(mem_vaddr[17] << 8) |
		(mem_vaddr[18] << 16) |
		(mem_vaddr[19] << 24);

	return 0;
}

static int lcd_tcon_mm_table_config_v0(void)
{
	if (tcon_mm_table.block_cnt != 4) {
		LCDERR("%s: tcon data block_cnt %d invalid\n",
		       __func__, tcon_mm_table.block_cnt);
		return -1;
	}

	tcon_mm_table.vac_mem_size = lcd_tcon_conf->vac_size;
	tcon_rmem.vac_mem_paddr = tcon_rmem.bin_path_mem_paddr +
			tcon_rmem.bin_path_mem_size;
	tcon_mm_table.vac_mem_vaddr =
		lcd_tcon_paddrtovaddr(tcon_rmem.vac_mem_paddr,
				      tcon_mm_table.vac_mem_size);
	if (lcd_debug_print_flag && tcon_mm_table.vac_mem_size > 0)
		LCDPR("vac_mem paddr: 0x%08x, vaddr: 0x%p, size: 0x%x\n",
		      (unsigned int)tcon_rmem.vac_mem_paddr,
		      tcon_mm_table.vac_mem_vaddr,
		      tcon_mm_table.vac_mem_size);

	tcon_mm_table.demura_set_mem_size = lcd_tcon_conf->demura_set_size;
	tcon_rmem.demura_set_mem_paddr = tcon_rmem.vac_mem_paddr +
			tcon_mm_table.vac_mem_size;
	tcon_mm_table.demura_set_mem_vaddr =
		lcd_tcon_paddrtovaddr(tcon_rmem.demura_set_mem_paddr,
				      tcon_mm_table.demura_set_mem_size);
	if (lcd_debug_print_flag && tcon_mm_table.demura_set_mem_size > 0)
		LCDPR("demura_set_mem paddr: 0x%08x, vaddr: 0x%p, size: 0x%x\n",
		      (unsigned int)tcon_rmem.demura_set_mem_paddr,
		      tcon_mm_table.demura_set_mem_vaddr,
		      tcon_mm_table.demura_set_mem_size);

	tcon_mm_table.demura_lut_mem_size = lcd_tcon_conf->demura_lut_size;
	tcon_rmem.demura_lut_mem_paddr =
			tcon_rmem.demura_set_mem_paddr +
			tcon_mm_table.demura_set_mem_size;
	tcon_mm_table.demura_lut_mem_vaddr =
		lcd_tcon_paddrtovaddr(tcon_rmem.demura_lut_mem_paddr,
				      tcon_mm_table.demura_lut_mem_size);
	if (lcd_debug_print_flag && tcon_mm_table.demura_lut_mem_size > 0)
		LCDPR("demura_lut_mem paddr: 0x%08x, vaddr: 0x%p, size: 0x%x\n",
		      (unsigned int)tcon_rmem.demura_lut_mem_paddr,
		      tcon_mm_table.demura_lut_mem_vaddr,
		      tcon_mm_table.demura_lut_mem_size);

	tcon_mm_table.acc_lut_mem_size = lcd_tcon_conf->acc_lut_size;
	tcon_rmem.acc_lut_mem_paddr = tcon_rmem.demura_lut_mem_paddr +
			tcon_mm_table.demura_lut_mem_size;
	tcon_mm_table.acc_lut_mem_vaddr =
		lcd_tcon_paddrtovaddr(tcon_rmem.acc_lut_mem_paddr,
				      tcon_mm_table.acc_lut_mem_size);
	if (lcd_debug_print_flag && tcon_mm_table.acc_lut_mem_size > 0)
		LCDPR("acc_lut_mem paddr: 0x%08x, vaddr: 0x%p, size: 0x%x\n",
		      (unsigned int)tcon_rmem.acc_lut_mem_paddr,
		      tcon_mm_table.acc_lut_mem_vaddr,
		      tcon_mm_table.acc_lut_mem_size);

	return 0;
}

static int lcd_tcon_mm_table_config_v1(void)
{
	if (tcon_mm_table.block_cnt > 32) {
		LCDERR("%s: tcon data block_cnt %d invalid\n",
		       __func__, tcon_mm_table.block_cnt);
		return -1;
	}

	if (tcon_mm_table.data_mem_vaddr)
		return 0;
	if (tcon_mm_table.block_cnt == 0) {
		if (lcd_debug_print_flag)
			LCDPR("%s: block_cnt is zero\n", __func__);
		return 0;
	}

	tcon_mm_table.data_mem_vaddr = kmalloc(tcon_mm_table.block_cnt *
					       sizeof(unsigned char *),
					       GFP_KERNEL);
	if (!tcon_mm_table.data_mem_vaddr) {
		LCDERR("%s: Not enough memory\n", __func__);
		return -1;
	}
	memset(tcon_mm_table.data_mem_vaddr, 0,
	       tcon_mm_table.block_cnt * sizeof(unsigned char *));

	tcon_mm_table.data_priority = kmalloc_array
					(tcon_mm_table.block_cnt,
					 sizeof(struct tcon_data_priority_s),
					 GFP_KERNEL);
	if (!tcon_mm_table.data_priority) {
		LCDERR("%s: Not enough memory\n", __func__);
		return -1;
	}
	memset(tcon_mm_table.data_priority, 0xff,
	       tcon_mm_table.block_cnt * sizeof(struct tcon_data_priority_s));

	return 0;
}

static int lcd_tcon_mem_config(void)
{
	unsigned int max_size;
	int ret;

	max_size = lcd_tcon_conf->axi_size +
		lcd_tcon_conf->bin_path_size;

	if (tcon_rmem.rsv_mem_size < max_size) {
		LCDERR("%s: the tcon size 0x%x is not enough, need 0x%x\n",
		       __func__, tcon_rmem.rsv_mem_size, max_size);
		return -1;
	}

	tcon_rmem.axi_mem_size = lcd_tcon_conf->axi_size;
	tcon_rmem.axi_mem_paddr = tcon_rmem.rsv_mem_paddr;
	tcon_rmem.axi_mem_vaddr =
		lcd_tcon_paddrtovaddr(tcon_rmem.axi_mem_paddr,
				      tcon_rmem.axi_mem_size);
	if (lcd_debug_print_flag)
		LCDPR("%s: axi_mem paddr: 0x%08x, vaddr: 0x%p, size: 0x%x\n",
		      __func__, (unsigned int)tcon_rmem.axi_mem_paddr,
		      tcon_rmem.axi_mem_vaddr,
		      tcon_rmem.axi_mem_size);

	tcon_rmem.bin_path_mem_size = lcd_tcon_conf->bin_path_size;
	tcon_rmem.bin_path_mem_paddr =
		tcon_rmem.axi_mem_paddr + tcon_rmem.axi_mem_size;
	tcon_rmem.bin_path_mem_vaddr =
		lcd_tcon_paddrtovaddr(tcon_rmem.bin_path_mem_paddr,
				      tcon_rmem.bin_path_mem_size);
	if (lcd_debug_print_flag && tcon_rmem.bin_path_mem_size > 0)
		LCDPR("tcon bin_path paddr: 0x%08x, vaddr: 0x%p, size: 0x%x\n",
		      (unsigned int)tcon_rmem.bin_path_mem_paddr,
		      tcon_rmem.bin_path_mem_vaddr,
		      tcon_rmem.bin_path_mem_size);

	ret = lcd_tcon_bin_path_update(tcon_rmem.bin_path_mem_size);
	if (ret)
		return -1;

	if (tcon_mm_table.version == 0) {
		max_size = lcd_tcon_conf->axi_size +
			tcon_rmem.bin_path_mem_size +
			lcd_tcon_conf->vac_size +
			lcd_tcon_conf->demura_set_size +
			lcd_tcon_conf->demura_lut_size +
			lcd_tcon_conf->acc_lut_size;
		if (tcon_rmem.rsv_mem_size < max_size) {
			LCDERR("%s: the tcon size 0x%x is not enough, need 0x%x\n",
			       __func__, tcon_rmem.rsv_mem_size, max_size);
			return -1;
		}
		ret = lcd_tcon_mm_table_config_v0();
	} else {
		ret = lcd_tcon_mm_table_config_v1();
	}
	return ret;
}

#ifdef CONFIG_CMA
static int lcd_tcon_cma_mem_config(struct cma *cma)
{
	struct aml_lcd_drv_s *lcd_drv = aml_lcd_get_driver();
	unsigned int mem_size;

	tcon_rmem.rsv_mem_paddr = cma_get_base(cma);
	LCDPR("tcon resv_mem base:0x%lx, size:0x%lx\n",
	      (unsigned long)tcon_rmem.rsv_mem_paddr,
	      cma_get_size(cma));
	mem_size = lcd_tcon_conf->rsv_size;
	if (cma_get_size(cma) < mem_size) {
		tcon_rmem.flag = 0;
	} else {
		tcon_rmem.rsv_mem_vaddr =
			dma_alloc_from_contiguous
			(lcd_drv->dev,
			 (mem_size >> PAGE_SHIFT), 0, 0);
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

static int lcd_tcon_gamma_pattern_tl1(unsigned int bit_width,
				      unsigned int gamma_r,
				      unsigned int gamma_g,
				      unsigned int gamma_b)
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
	lcd_tcon_setb_byte(0x262, 0x2, 0, 2);

	/* gamma_r */
	/*2 data splice 3 bytes*/
	temp[0] = val_r & 0xff;
	temp[1] = ((val_r >> 8) & 0xf) | ((val_r & 0xf) << 4);
	temp[2] = (val_r >> 4) & 0xff;
	reg = 0xb50;
	for (i = 0; i < 384; i += 3) { /* 256 * 1.5 */
		lcd_tcon_write_byte((reg + i), temp[0]);
		lcd_tcon_write_byte((reg + i + 1), temp[1]);
		lcd_tcon_write_byte((reg + i + 2), temp[2]);
	}
	temp[0] = val_r & 0xff;
	temp[1] = ((val_r >> 8) & 0xf);
	temp[2] = 0;
	reg += 384;
	lcd_tcon_write_byte((reg), temp[0]);
	lcd_tcon_write_byte((reg + 1), temp[1]);
	lcd_tcon_write_byte((reg + 2), temp[2]);

	/* gamma_g */
	/*2 data splice 3 bytes*/
	temp[0] = val_g & 0xff;
	temp[1] = ((val_g >> 8) & 0xf) | ((val_g & 0xf) << 4);
	temp[2] = (val_g >> 4) & 0xff;
	reg += 3;
	for (i = 0; i < 384; i += 3) { /* 256 * 1.5 */
		lcd_tcon_write_byte((reg + i), temp[0]);
		lcd_tcon_write_byte((reg + i + 1), temp[1]);
		lcd_tcon_write_byte((reg + i + 2), temp[2]);
	}
	temp[0] = val_g & 0xff;
	temp[1] = ((val_g >> 8) & 0xf);
	temp[2] = 0;
	reg += 384;
	lcd_tcon_write_byte((reg), temp[0]);
	lcd_tcon_write_byte((reg + 1), temp[1]);
	lcd_tcon_write_byte((reg + 2), temp[2]);

	/* gamma_b */
	/*2 data splice 3 bytes*/
	temp[0] = val_b & 0xff;
	temp[1] = ((val_b >> 8) & 0xf) | ((val_b & 0xf) << 4);
	temp[2] = (val_b >> 4) & 0xff;
	reg += 3;
	for (i = 0; i < 384; i += 3) { /* 256 * 1.5 */
		lcd_tcon_write_byte((reg + i), temp[0]);
		lcd_tcon_write_byte((reg + i + 1), temp[1]);
		lcd_tcon_write_byte((reg + i + 2), temp[2]);
	}
	temp[0] = val_b & 0xff;
	temp[1] = ((val_b >> 8) & 0xf);
	temp[2] = 0;
	reg += 384;
	lcd_tcon_write_byte((reg), temp[0]);
	lcd_tcon_write_byte((reg + 1), temp[1]);
	lcd_tcon_write_byte((reg + 2), temp[2]);

	/* enable gamma */
	lcd_tcon_setb_byte(0x262, 0x3, 0, 2);

	return 0;
}

static int lcd_tcon_enable_tl1(struct lcd_config_s *pconf)
{
	int ret, flag;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	if (tcon_mm_table.tcon_data_flag)
		lcd_tcon_data_load();

	/* step 1: tcon top */
	lcd_tcon_top_set_tl1(pconf);

	/* step 2: tcon_core_reg_update */
	lcd_tcon_core_reg_update();
	if (pconf->lcd_basic.lcd_type == LCD_P2P) {
		switch (pconf->lcd_control.p2p_config->p2p_type) {
		case P2P_CHPI:
			lcd_phy_tcon_chpi_bbc_init_tl1(pconf);
			break;
		default:
			break;
		}
	}
	if (tcon_mm_table.version == 0) {
		if (tcon_mm_table.valid_flag & LCD_TCON_DATA_VALID_VAC) {
			if (tcon_mm_table.valid_flag &
			    LCD_TCON_DATA_VALID_DEMURA)
				flag = 1;
			else
				flag = 0;
			lcd_tcon_vac_set_tl1(flag);
		}
		if (tcon_mm_table.valid_flag & LCD_TCON_DATA_VALID_DEMURA) {
			lcd_tcon_demura_set_tl1();
			lcd_tcon_demura_lut_tl1();
		}
		if (tcon_mm_table.valid_flag & LCD_TCON_DATA_VALID_ACC)
			lcd_tcon_acc_lut_tl1();
	} else {
		lcd_tcon_data_set();
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
			"lcd_tcon", (void *)"lcd_tcon")) {
		LCDERR("can't request lcd_tcon irq\n");
	} else {
		if (lcd_debug_print_flag)
			LCDPR("request lcd_tcon irq successful\n");
	}

	lcd_tcon_write(TCON_INTR_MASKN, TCON_INTR_MASKN_VAL);
}

static int lcd_tcon_config(struct aml_lcd_drv_s *lcd_drv)
{
	int key_len, reg_len, ret;

	/* get reg table from unifykey */
	reg_len = lcd_tcon_conf->reg_table_len;
	if (!lcd_tcon_conf->reg_table) {
		lcd_tcon_conf->reg_table =
			kcalloc(reg_len, sizeof(unsigned char), GFP_KERNEL);
		if (!lcd_tcon_conf->reg_table) {
			LCDERR("%s: Not enough memory\n", __func__);
			return -1;
		}
	}
	key_len = reg_len;
	ret = lcd_unifykey_get_no_header
		("lcd_tcon",
		 lcd_tcon_conf->reg_table, &key_len);
	if (ret) {
		kfree(lcd_tcon_conf->reg_table);
		lcd_tcon_conf->reg_table = NULL;
		LCDERR("%s: !!!!!!!!tcon unifykey load error!!!!!!!!\n",
		       __func__);
		return -1;
	}
	if (key_len != reg_len) {
		kfree(lcd_tcon_conf->reg_table);
		lcd_tcon_conf->reg_table = NULL;
		LCDERR("%s: !!!!!!!!tcon unifykey load length error!!!!!!!!\n",
		       __func__);
		return -1;
	}
	LCDPR("tcon: load unifykey len: %d\n", key_len);

	if (!tcon_mm_table.tcon_data_flag)
		lcd_tcon_data_load();
	lcd_drv->tcon_status = tcon_mm_table.valid_flag;

	lcd_tcon_intr_init(lcd_drv);

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

	if (!lcd_tcon_conf->reg_table) {
		LCDERR("%s: reg_table is null\n", __func__);
		return;
	}

	buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
	if (!buf) {
		LCDERR("%s: buf malloc error\n", __func__);
		return;
	}

	LCDPR("%s:\n", __func__);
	cnt = lcd_tcon_conf->reg_table_len;
	for (i = 0; i < cnt; i += 16) {
		n = snprintf(buf, PR_BUF_MAX, "0x%04x: ", i);
		for (j = 0; j < 16; j++) {
			if ((i + j) >= cnt)
				break;
			n += snprintf(buf + n, PR_BUF_MAX, " 0x%02x",
				      lcd_tcon_conf->reg_table[i + j]);
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
	if (!buf) {
		LCDERR("%s: buf malloc error\n", __func__);
		return;
	}

	LCDPR("%s:\n", __func__);
	cnt = lcd_tcon_conf->reg_table_len;
	for (i = 0; i < cnt; i += 16) {
		n = snprintf(buf, PR_BUF_MAX, "0x%04x: ", i);
		for (j = 0; j < 16; j++) {
			if ((i + j) >= cnt)
				break;
			if (lcd_tcon_conf->core_reg_width == 8) {
				n += snprintf(buf + n, PR_BUF_MAX, " 0x%02x",
					lcd_tcon_read_byte(i + j));
			} else {
				n += snprintf(buf + n, PR_BUF_MAX, " 0x%02x",
					lcd_tcon_read(i + j));
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
	len += snprintf((buf + len), n,
		"tcon info:\n"
		"core_reg_width:         %d\n"
		"reg_table_len:          %d\n"
		"reserved_mem paddr:     0x%lx\n"
		"reserved_mem size:      0x%x\n"
		"axi_mem paddr:          0x%lx\n"
		"axi_mem vaddr:          0x%p\n"
		"axi_mem size:           0x%x\n"
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
		(unsigned long)tcon_rmem.rsv_mem_paddr,
		tcon_rmem.rsv_mem_size,
		(unsigned long)tcon_rmem.axi_mem_paddr,
		tcon_rmem.axi_mem_vaddr, tcon_rmem.axi_mem_size,
		(unsigned long)tcon_rmem.vac_mem_paddr,
		tcon_mm_table.vac_mem_vaddr, tcon_mm_table.vac_mem_size,
		(unsigned long)tcon_rmem.demura_set_mem_paddr,
		tcon_mm_table.demura_set_mem_vaddr,
		tcon_mm_table.demura_set_mem_size,
		(unsigned long)tcon_rmem.demura_lut_mem_paddr,
		tcon_mm_table.demura_lut_mem_vaddr,
		tcon_mm_table.demura_lut_mem_size,
		(unsigned long)tcon_rmem.acc_lut_mem_paddr,
		tcon_mm_table.acc_lut_mem_vaddr,
		tcon_mm_table.acc_lut_mem_size);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"tcon_status:\n"
		"vac_valid:              %d\n"
		"demura_valid:           %d\n"
		"acc_valid:              %d\n\n",
		((tcon_mm_table.valid_flag >> LCD_TCON_DATA_VALID_VAC) & 0x1),
		((tcon_mm_table.valid_flag >> LCD_TCON_DATA_VALID_DEMURA) & 0x1),
		((tcon_mm_table.valid_flag >> LCD_TCON_DATA_VALID_ACC) & 0x1));
	return len;
}

int lcd_tcon_reg_table_size_get(void)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	return lcd_tcon_conf->reg_table_len;
}

unsigned char *lcd_tcon_reg_table_get(void)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return NULL;

	return lcd_tcon_conf->reg_table;
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

int lcd_tcon_core_reg_get(unsigned char *buf, unsigned int size)
{
	int i, ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	if (size > lcd_tcon_conf->reg_table_len) {
		LCDERR("%s: size is not enough\n", __func__);
		return -1;
	}

	if (lcd_tcon_conf->core_reg_width == 8) {
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

	if (!(lcd_drv->lcd_status & LCD_STATUS_IF_ON))
		return -1;

	reg = lcd_tcon_conf->reg_core_od;
	bit = lcd_tcon_conf->bit_od_en;
	if (lcd_tcon_conf->core_reg_width == 8)
		temp = lcd_tcon_read_byte(reg + TCON_CORE_REG_START);
	else
		temp = lcd_tcon_read(reg + TCON_CORE_REG_START);
	if (flag)
		temp |= (1 << bit);
	else
		temp &= ~(1 << bit);
	temp &= 0xff;
	if (lcd_tcon_conf->core_reg_width == 8)
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

	if (lcd_tcon_conf->reg_core_od == REG_LCD_TCON_MAX) {
		LCDERR("%s: invalid od reg\n", __func__);
		return 0;
	}

	reg = lcd_tcon_conf->reg_core_od;
	bit = lcd_tcon_conf->bit_od_en;
	if (lcd_tcon_conf->core_reg_width == 8)
		temp = lcd_tcon_read_byte(reg + TCON_CORE_REG_START);
	else
		temp = lcd_tcon_read(reg + TCON_CORE_REG_START);
	ret = ((temp >> bit) & 1);

	return ret;
}

int lcd_tcon_gamma_set_pattern(unsigned int bit_width, unsigned int gamma_r,
			       unsigned int gamma_g, unsigned int gamma_b)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	if (lcd_tcon_conf->tcon_gamma_pattern) {
		ret = lcd_tcon_conf->tcon_gamma_pattern(bit_width, gamma_r,
							gamma_g, gamma_b);
		LCDPR("%s: %dbits gamma r:0x%x g:0x%x b:0x%x\n",
		      __func__, bit_width, gamma_r, gamma_g, gamma_b);
	}

	return ret;
}

int lcd_tcon_enable(struct lcd_config_s *pconf)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	if (lcd_tcon_conf->tcon_enable)
		lcd_tcon_conf->tcon_enable(pconf);

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

	/* release data*/
	if (tcon_mm_table.version) {
		for (i = 0; i < tcon_mm_table.block_cnt; i++) {
			if (!tcon_mm_table.data_mem_vaddr[i])
				continue;
			kfree(tcon_mm_table.data_mem_vaddr[i]);
			tcon_mm_table.data_mem_vaddr[i] = NULL;
		}
		tcon_mm_table.valid_flag = 0;
		tcon_mm_table.tcon_data_flag = 0;
	}

	/* disable tcon intr */
	lcd_tcon_write(TCON_INTR_MASKN, 0);

	/* disable over_drive */
	if (lcd_tcon_conf->reg_core_od != REG_LCD_TCON_MAX) {
		reg = lcd_tcon_conf->reg_core_od + TCON_CORE_REG_START;
		bit = lcd_tcon_conf->bit_od_en;
		if (lcd_tcon_conf->core_reg_width == 8)
			lcd_tcon_setb_byte(reg, 0, bit, 1);
		else
			lcd_tcon_setb(reg, 0, bit, 1);
		msleep(100);
	}

	/* disable all ctrl signal */
	if (lcd_tcon_conf->reg_core_ctrl_timing_base == REG_LCD_TCON_MAX)
		goto lcd_tcon_disable_next;
	reg = lcd_tcon_conf->reg_core_ctrl_timing_base + TCON_CORE_REG_START;
	offset = lcd_tcon_conf->ctrl_timing_offset;
	cnt = lcd_tcon_conf->ctrl_timing_cnt;
	for (i = 0; i < cnt; i++) {
		if (lcd_tcon_conf->core_reg_width == 8)
			lcd_tcon_setb_byte((reg + (i * offset)), 1, 3, 1);
		else
			lcd_tcon_setb((reg + (i * offset)), 1, 3, 1);
	}

	/* disable top */
lcd_tcon_disable_next:
	if (lcd_tcon_conf->reg_top_ctrl != REG_LCD_TCON_MAX) {
		reg = lcd_tcon_conf->reg_top_ctrl;
		bit = lcd_tcon_conf->bit_en;
		lcd_tcon_setb(reg, 0, bit, 1);
	}
}

/* **********************************
 * tcon match data
 * **********************************
 */
static struct lcd_tcon_config_s tcon_data_tl1 = {
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

	/*rsv_mem(12M)    axi_mem(10M)   bin_path(10K)
	 *             |----------------|-------------|
	 */
	.rsv_size    = 0x00c00000,
	.axi_size        = 0x00a00000,
	.bin_path_size   = 0x00002800,
	.vac_size        = 0x00002000, /* 8k */
	.demura_set_size = 0x00001000, /* 4k */
	.demura_lut_size = 0x00120000, /* 1152k */
	.acc_lut_size    = 0x00001000, /* 4k */
	.reg_table = NULL,
	.tcon_gamma_pattern = lcd_tcon_gamma_pattern_tl1,
	.tcon_enable = lcd_tcon_enable_tl1,
};

int lcd_tcon_probe(struct aml_lcd_drv_s *lcd_drv)
{
	struct cma *cma;
	unsigned int mem_size;
	int key_init_flag = 0;
	int ret = 0;

	lcd_tcon_conf = NULL;
	switch (lcd_drv->data->chip_type) {
	case LCD_CHIP_TL1:
	case LCD_CHIP_TM2:
	case LCD_CHIP_T5:
		switch (lcd_drv->lcd_config->lcd_basic.lcd_type) {
		case LCD_MLVDS:
		case LCD_P2P:
			lcd_tcon_conf = &tcon_data_tl1;
			lcd_tcon_conf->tcon_valid = 1;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	if (!lcd_tcon_conf)
		return 0;
	if (lcd_tcon_conf->tcon_valid == 0)
		return 0;

	/* init reserved memory */
	ret = of_reserved_mem_device_init(lcd_drv->dev);
	if (ret) {
		LCDERR("tcon: init reserved memory failed\n");
	} else {
		if (!(void *)tcon_rmem.rsv_mem_paddr) {
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
			mem_size = tcon_rmem.rsv_mem_size;
			if (mem_size < lcd_tcon_conf->rsv_size) {
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
	if (tcon_rmem.axi_mem_vaddr) {
		lcd_unmap_phyaddr(tcon_rmem.axi_mem_vaddr);
		tcon_rmem.axi_mem_vaddr = NULL;
	}
	if (tcon_mm_table.data_mem_vaddr) {
		lcd_unmap_phyaddr(*tcon_mm_table.data_mem_vaddr);
		tcon_mm_table.vac_mem_vaddr = NULL;
		tcon_mm_table.demura_set_mem_vaddr = NULL;
		tcon_mm_table.demura_lut_mem_vaddr = NULL;
		tcon_mm_table.acc_lut_mem_vaddr = NULL;
	}

	if (tcon_rmem.flag == 2) {
		LCDPR("tcon free memory: base:0x%lx, size:0x%x\n",
			(unsigned long)tcon_rmem.rsv_mem_paddr,
			tcon_rmem.rsv_mem_size);
#ifdef CONFIG_CMA
		dma_release_from_contiguous(lcd_drv->dev,
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
	tcon_rmem.rsv_mem_paddr = rmem->base;
	tcon_rmem.rsv_mem_size = rmem->size;
	rmem->ops = &tcon_fb_ops;
	LCDPR("tcon: Reserved memory: created fb at 0x%lx, size %ld MiB\n",
	      (unsigned long)rmem->base, (unsigned long)rmem->size / SZ_1M);
	return 0;
}

RESERVEDMEM_OF_DECLARE(fb, "amlogic, lcd_tcon-memory", tcon_fb_setup);

