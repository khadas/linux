// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/major.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/dma-mapping.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_unifykey.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_tcon_fw.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "lcd_common.h"
#include "lcd_reg.h"
#include "lcd_tcon.h"

static unsigned int lcd_tcon_fw_reg_read(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
				unsigned int reg)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return 0;

	if (bit_flag == 8)
		return lcd_tcon_read_byte(pdrv, reg);
	return lcd_tcon_read(pdrv, reg);
}

static void lcd_tcon_fw_reg_write(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
				unsigned int reg, unsigned int value)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return;

	if (bit_flag == 8)
		lcd_tcon_write_byte(pdrv, reg, value);
	else
		lcd_tcon_write(pdrv, reg, value);
}

static void lcd_tcon_fw_reg_setb(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
				unsigned int reg, unsigned int value,
				unsigned int start, unsigned int len)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return;

	if (bit_flag == 8)
		lcd_tcon_setb_byte(pdrv, reg, value, start, len);
	else
		lcd_tcon_setb(pdrv, reg, value, start, len);
}

static unsigned int lcd_tcon_fw_reg_getb(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
				unsigned int reg, unsigned int start, unsigned int len)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return 0;

	if (bit_flag == 8)
		return lcd_tcon_getb_byte(pdrv, reg, start, len);
	return lcd_tcon_getb(pdrv, reg, start, len);
}

static void lcd_tcon_fw_reg_update_bits(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
				unsigned int reg, unsigned int mask, unsigned int value)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return;

	if (bit_flag == 8)
		lcd_tcon_update_bits_byte(pdrv, reg, mask, value);
	else
		lcd_tcon_update_bits(pdrv, reg, mask, value);
}

static int lcd_tcon_fw_reg_check_bits(struct lcd_tcon_fw_s *fw, unsigned int bit_flag,
				unsigned int reg, unsigned int mask, unsigned int value)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return 0;

	if (bit_flag == 8)
		return lcd_tcon_check_bits_byte(pdrv, reg, mask, value);
	return lcd_tcon_check_bits(pdrv, reg, mask, value);
}

static unsigned int lcd_tcon_fw_get_encl_line_cnt(struct lcd_tcon_fw_s *fw)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return 0;

	return lcd_get_encl_line_cnt(pdrv);
}

static unsigned int lcd_tcon_fw_get_encl_frm_cnt(struct lcd_tcon_fw_s *fw)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)fw->drvdat;

	if (!pdrv)
		return 0;

	return lcd_get_encl_frm_cnt(pdrv);
}

static struct tcon_fw_config_s lcd_tcon_fw_config = {
	.config_size = sizeof(struct tcon_fw_config_s),
	.chip_type = TCON_CHIP_MAX,
	.if_type = TCON_IF_TYPE_MAX,
	.axi_cnt = 0,
	.axi_rmem = NULL,
};

static struct tcon_fw_base_timing_s lcd_tcon_fw_base_timing;

static struct lcd_tcon_fw_s lcd_tcon_fw = {
	/* init by driver */
	.para_ver = TCON_FW_PARA_VER,
	.para_size = sizeof(struct lcd_tcon_fw_s),
	.valid = 0,
	.flag = 0, //for some state update
	.tcon_state = 0,

	/* init by fw */
	.fw_ready = 0,
	.fw_ver = "not installed",
	.dbg_level = 0,

	/* driver resource */
	.drvdat = NULL,
	.dev = NULL,
	.config = &lcd_tcon_fw_config,
	.base_timing = &lcd_tcon_fw_base_timing,

	/* fw resource */
	.buf_table_in = NULL,
	.buf_table_out = NULL,

	/* API by driver */
	.reg_read = lcd_tcon_fw_reg_read,
	.reg_write = lcd_tcon_fw_reg_write,
	.reg_setb = lcd_tcon_fw_reg_setb,
	.reg_getb = lcd_tcon_fw_reg_getb,
	.reg_update_bits = lcd_tcon_fw_reg_update_bits,
	.reg_check_bits = lcd_tcon_fw_reg_check_bits,

	.get_encl_line_cnt = lcd_tcon_fw_get_encl_line_cnt,
	.get_encl_frm_cnt = lcd_tcon_fw_get_encl_frm_cnt,

	/* API by fw */
	.vsync_isr = NULL,
	.tcon_alg = NULL,
	.debug_show = NULL,
	.debug_store = NULL,
};

struct lcd_tcon_fw_s *aml_lcd_tcon_get_fw(void)
{
	return &lcd_tcon_fw;
}
EXPORT_SYMBOL(aml_lcd_tcon_get_fw);

/* buf_table:
 * ....top_header
 * ........crc32: only include top_header+index
 * ........data_size: only include top_header+index
 * ....index(buf0~n addr pointer)
 * ....buf[0](header+data)
 * ........crc32: only buf[0] header+data
 * ........data_size: only buf[0] header+data
 * ............
 * ....buf[n]
 */
int lcd_tcon_fw_buf_table_generate(struct lcd_tcon_fw_s *tcon_fw)
{
	return 0;
}

void lcd_tcon_fw_base_timing_update(struct aml_lcd_drv_s *pdrv)
{
	if (!pdrv || !lcd_tcon_fw.base_timing)
		return;

	lcd_tcon_fw.base_timing->hsize = pdrv->config.timing.act_timing.h_active;
	lcd_tcon_fw.base_timing->vsize = pdrv->config.timing.act_timing.v_active;
	lcd_tcon_fw.base_timing->htotal = pdrv->config.timing.act_timing.h_period;
	lcd_tcon_fw.base_timing->vtotal = pdrv->config.timing.act_timing.v_period;
	lcd_tcon_fw.base_timing->frame_rate = pdrv->config.timing.act_timing.frame_rate;
	lcd_tcon_fw.base_timing->pclk = pdrv->config.timing.act_timing.pixel_clk;
	lcd_tcon_fw.base_timing->bit_rate = pdrv->config.timing.bit_rate;

	lcd_tcon_fw.base_timing->de_hstart = pdrv->config.timing.hstart;
	lcd_tcon_fw.base_timing->de_hend = pdrv->config.timing.hend;
	lcd_tcon_fw.base_timing->de_vstart = pdrv->config.timing.vstart;
	lcd_tcon_fw.base_timing->de_vend = pdrv->config.timing.vend;
	lcd_tcon_fw.base_timing->pre_de_h = 8;
	lcd_tcon_fw.base_timing->pre_de_v = 8;

	lcd_tcon_fw.base_timing->hsw = pdrv->config.timing.act_timing.hsync_width;
	lcd_tcon_fw.base_timing->hbp = pdrv->config.timing.act_timing.hsync_bp;
	lcd_tcon_fw.base_timing->hfp = pdrv->config.timing.act_timing.hsync_fp;

	lcd_tcon_fw.base_timing->vsw = pdrv->config.timing.act_timing.vsync_width;
	lcd_tcon_fw.base_timing->vbp = pdrv->config.timing.act_timing.vsync_bp;
	lcd_tcon_fw.base_timing->vfp = pdrv->config.timing.act_timing.vsync_fp;

	lcd_tcon_fw.base_timing->update_flag = 1;
}

void lcd_tcon_fw_prepare(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_config_s *tcon_conf)
{
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	int i;

	if (!lcd_tcon_fw.config)
		return;
	if (!pdrv || !pdrv->data || !tcon_conf || !tcon_rmem || !mm_table)
		return;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_TL1:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_TL1;
		break;
	case LCD_CHIP_TM2:
	case LCD_CHIP_TM2B:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_TM2;
		break;
	case LCD_CHIP_T5:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_T5;
		break;
	case LCD_CHIP_T5D:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_T5D;
		break;
	case LCD_CHIP_T3:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_T3;
		break;
	case LCD_CHIP_T5W:
		lcd_tcon_fw.config->chip_type = TCON_CHIP_T5W;
		break;
	default:
		break;
	}
	switch (pdrv->config.basic.lcd_type) {
	case LCD_MLVDS:
		lcd_tcon_fw.config->if_type = TCON_IF_MLVDS;
		break;
	case LCD_P2P:
		lcd_tcon_fw.config->if_type = TCON_IF_P2P;
		break;
	default:
		break;
	}

	lcd_tcon_fw.config->axi_cnt = tcon_conf->axi_bank;
	if (!tcon_rmem->axi_rmem)
		return;
	lcd_tcon_fw.config->axi_rmem = kcalloc(lcd_tcon_fw.config->axi_cnt,
		sizeof(struct tcon_fw_axi_rmem_s), GFP_KERNEL);
	for (i = 0; i < lcd_tcon_fw.config->axi_cnt; i++) {
		lcd_tcon_fw.config->axi_rmem[i].mem_paddr = tcon_rmem->axi_rmem[i].mem_paddr;
		lcd_tcon_fw.config->axi_rmem[i].mem_size = tcon_rmem->axi_rmem[i].mem_size;
	}

	if (pdrv->status & LCD_STATUS_IF_ON)
		lcd_tcon_fw.tcon_state |= TCON_FW_STATE_TCON_EN;
	if (mm_table->valid_flag & LCD_TCON_DATA_VALID_VAC)
		lcd_tcon_fw.tcon_state |= TCON_FW_STATE_VAC_VALID;
	if (mm_table->valid_flag & LCD_TCON_DATA_VALID_DEMURA)
		lcd_tcon_fw.tcon_state |= TCON_FW_STATE_DEMURA_VALID;
	if (mm_table->valid_flag & LCD_TCON_DATA_VALID_ACC)
		lcd_tcon_fw.tcon_state |= TCON_FW_STATE_ACC_VALID;
	if (mm_table->valid_flag & LCD_TCON_DATA_VALID_DITHER)
		lcd_tcon_fw.tcon_state |= TCON_FW_STATE_DITHER_VALID;
	if (mm_table->valid_flag & LCD_TCON_DATA_VALID_OD)
		lcd_tcon_fw.tcon_state |= TCON_FW_STATE_OD_VALID;
	if (mm_table->valid_flag & LCD_TCON_DATA_VALID_LOD)
		lcd_tcon_fw.tcon_state |= TCON_FW_STATE_LOD_VALID;

	lcd_tcon_fw_base_timing_update(pdrv);

	lcd_tcon_fw.drvdat = (void *)pdrv;
	lcd_tcon_fw.dev = pdrv->dev;

	lcd_tcon_fw.valid = 1;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("tcon fw prepare done\n");
}
