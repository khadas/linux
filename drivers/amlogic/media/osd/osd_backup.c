// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Linux Headers */
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>

/* Local Headers */
#include "osd_io.h"
#include "osd_backup.h"
#include "osd_hw.h"

const u16 osd_reg_backup[OSD_REG_BACKUP_COUNT] = {
	0x1a10, 0x1a13,
	0x1a17, 0x1a18, 0x1a19, 0x1a1a, 0x1a1b, 0x1a1c, 0x1a1d, 0x1a1e,
	0x1a2b, 0x1a2d
};

const u16 osd_afbc_reg_backup[OSD_AFBC_REG_BACKUP_COUNT] = {
	0x31aa, 0x31a9,
	0x31a6, 0x31a5, 0x31a4, 0x31a3, 0x31a2, 0x31a1, 0x31a0
};

const u16 mali_afbc_reg_backup[MALI_AFBC_REG_BACKUP_COUNT] = {
	0x3a03, 0x3a07,
	0x3a10, 0x3a11, 0x3a12, 0x3a13, 0x3a14, 0x3a15, 0x3a16,
	0x3a17, 0x3a18, 0x3a19, 0x3a1a, 0x3a1b, 0x3a1c,
	0x3a30, 0x3a31, 0x3a32, 0x3a33, 0x3a34, 0x3a35, 0x3a36,
	0x3a37, 0x3a38, 0x3a39, 0x3a3a, 0x3a3b, 0x3a3c,
	0x3a50, 0x3a51, 0x3a52, 0x3a53, 0x3a54, 0x3a55, 0x3a56,
	0x3a57, 0x3a58, 0x3a59, 0x3a5a, 0x3a5b, 0x3a5c
};

const u16 mali_afbc_reg_t7_backup[MALI_AFBC_REG_T7_BACKUP_COUNT] = {
	0x3a03, 0x3a07,
	0x3a10, 0x3a11, 0x3a12, 0x3a13, 0x3a14, 0x3a15, 0x3a16,
	0x3a17, 0x3a18, 0x3a19, 0x3a1a, 0x3a1b, 0x3a1c,
	0x3a30, 0x3a31, 0x3a32, 0x3a33, 0x3a34, 0x3a35, 0x3a36,
	0x3a37, 0x3a38, 0x3a39, 0x3a3a, 0x3a3b, 0x3a3c
};

const u16 mali_afbc1_reg_t7_backup[MALI_AFBC1_REG_T7_BACKUP_COUNT] = {
	0x3b03, 0x3b07,
	0x3b50, 0x3b51, 0x3b52, 0x3b53, 0x3b54, 0x3b55, 0x3b56,
	0x3b57, 0x3b58, 0x3b59, 0x3b5a, 0x3b5b, 0x3b5c
};

const u16 mali_afbc2_reg_t7_backup[MALI_AFBC2_REG_T7_BACKUP_COUNT] = {
	0x3c03, 0x3c07,
	0x3c70, 0x3c71, 0x3c72, 0x3c73, 0x3c74, 0x3c75, 0x3c76,
	0x3c77, 0x3c78, 0x3c79, 0x3c7a, 0x3c7b, 0x3c7c
};

static u32 osd_backup_count = OSD_VALUE_COUNT;
static u32 afbc_backup_count = OSD_AFBC_VALUE_COUNT;
static u32 mali_afbc_backup_count = MALI_AFBC_REG_BACKUP_COUNT;
static u32 mali_afbc_t7_backup_count = MALI_AFBC_REG_T7_BACKUP_COUNT;
static u32 mali_afbc1_t7_backup_count = MALI_AFBC1_REG_T7_BACKUP_COUNT;
static u32 mali_afbc2_t7_backup_count = MALI_AFBC2_REG_T7_BACKUP_COUNT;
u32 osd_backup[OSD_VALUE_COUNT];
u32 osd_afbc_backup[OSD_AFBC_VALUE_COUNT];
u32 mali_afbc_backup[MALI_AFBC_VALUE_COUNT];
u32 mali_afbc_t7_backup[MALI_AFBC_VALUE_T7_COUNT];
u32 mali_afbc1_t7_backup[MALI_AFBC1_VALUE_T7_COUNT];
u32 mali_afbc2_t7_backup[MALI_AFBC2_VALUE_T7_COUNT];
module_param_array(osd_backup, uint, &osd_backup_count, 0444);
MODULE_PARM_DESC(osd_backup, "\n osd register backup\n");
module_param_array(osd_afbc_backup, uint, &afbc_backup_count, 0444);
MODULE_PARM_DESC(osd_afbc_backup, "\n osd afbc register backup\n");
module_param_array(mali_afbc_backup, uint, &mali_afbc_backup_count, 0444);
MODULE_PARM_DESC(mali_afbc_backup, "\n mali afbc register backup\n");
module_param_array(mali_afbc_t7_backup, uint, &mali_afbc_t7_backup_count, 0444);
MODULE_PARM_DESC(mali_afbc_t7_backup, "\n mali afbc register t7 backup\n");
module_param_array(mali_afbc1_t7_backup, uint, &mali_afbc1_t7_backup_count, 0444);
MODULE_PARM_DESC(mali_afbc1_t7_backup, "\n mali afbc1 register t7 backup\n");
module_param_array(mali_afbc2_t7_backup, uint, &mali_afbc2_t7_backup_count, 0444);
MODULE_PARM_DESC(mali_afbc2_t7_backup, "\n mali afbc2 register t7 backup\n");

/* 0: not backup */
static u32 backup_enable;
module_param(backup_enable, uint, 0644);
void update_backup_reg(u32 addr, u32 value)
{
	u32 base = OSD1_AFBCD_ENABLE;

	if (!backup_enable)
		return;
	if (addr >= OSD1_AFBCD_ENABLE &&
	    addr <= OSD1_AFBCD_PIXEL_VSCOPE &&
	    (backup_enable & HW_RESET_AFBCD_REGS)) {
		osd_afbc_backup[addr - base] = value;
		return;
	}
	base = VIU_OSD1_CTRL_STAT;
	if (addr >= VIU_OSD1_CTRL_STAT &&
	    addr <= VIU_OSD1_CTRL_STAT2 &&
	    (backup_enable & HW_RESET_OSD1_REGS)) {
		osd_backup[addr - base] = value;
		return;
	}
	if (osd_dev_hw.multi_afbc_core) {
		base = VPU_MAFBC_IRQ_MASK;
		if (addr >= VPU_MAFBC_IRQ_MASK &&
		    addr <= VPU_MAFBC_PREFETCH_CFG_S1 &&
		    (backup_enable & HW_RESET_MALI_AFBCD_REGS)) {
			mali_afbc_t7_backup[addr - base] = value;
			return;
		}
		base = VPU_MAFBC1_IRQ_MASK;
		if (addr >= VPU_MAFBC1_IRQ_MASK &&
		    addr <= VPU_MAFBC1_PREFETCH_CFG_S2 &&
		    (backup_enable & HW_RESET_MALI_AFBCD1_REGS)) {
			mali_afbc1_t7_backup[addr - base] = value;
			return;
		}
		base = VPU_MAFBC2_IRQ_MASK;
		if (addr >= VPU_MAFBC2_IRQ_MASK &&
		    addr <= VPU_MAFBC2_PREFETCH_CFG_S3 &&
		    (backup_enable & HW_RESET_MALI_AFBCD2_REGS)) {
			mali_afbc2_t7_backup[addr - base] = value;
			return;
		}
	} else {
		base = VPU_MAFBC_IRQ_MASK;
		if (addr >= VPU_MAFBC_IRQ_MASK &&
		    addr <= VPU_MAFBC_PREFETCH_CFG_S2 &&
		    (backup_enable & HW_RESET_MALI_AFBCD_REGS)) {
			mali_afbc_backup[addr - base] = value;
			return;
		}
	}
}

s32 get_backup_reg(u32 addr, u32 *value)
{
	int i;
	u32 base = OSD1_AFBCD_ENABLE;

	if (!backup_enable || !value)
		return -1;

	if (addr >= OSD1_AFBCD_ENABLE &&
	    addr <= OSD1_AFBCD_PIXEL_VSCOPE &&
	    (backup_enable & HW_RESET_AFBCD_REGS)) {
		for (i = 0; i < OSD_AFBC_REG_BACKUP_COUNT; i++)
			if (addr == osd_afbc_reg_backup[i]) {
				*value = osd_afbc_backup[addr - base];
				return 0;
			}
	}
	base = VIU_OSD1_CTRL_STAT;
	if (addr >= VIU_OSD1_CTRL_STAT &&
	    addr <= VIU_OSD1_CTRL_STAT2 &&
	    (backup_enable & HW_RESET_OSD1_REGS)) {
		for (i = 0; i < OSD_REG_BACKUP_COUNT; i++)
			if (addr == osd_reg_backup[i]) {
				*value = osd_backup[addr - base];
				return 0;
			}
	}
	if (osd_dev_hw.multi_afbc_core) {
		base = VPU_MAFBC_IRQ_MASK;
		if (addr >= VPU_MAFBC_IRQ_MASK &&
		    addr <= VPU_MAFBC_PREFETCH_CFG_S1 &&
		    (backup_enable & HW_RESET_MALI_AFBCD_REGS)) {
			for (i = 0; i < MALI_AFBC_REG_T7_BACKUP_COUNT; i++)
				if (addr == mali_afbc_reg_t7_backup[i]) {
					*value = mali_afbc_t7_backup[addr - base];
					return 0;
				}
		}
		base = VPU_MAFBC1_IRQ_MASK;
		if (addr >= VPU_MAFBC1_IRQ_MASK &&
		    addr <= VPU_MAFBC1_PREFETCH_CFG_S2 &&
		    (backup_enable & HW_RESET_MALI_AFBCD1_REGS)) {
			for (i = 0; i < MALI_AFBC1_REG_T7_BACKUP_COUNT; i++)
				if (addr == mali_afbc1_reg_t7_backup[i]) {
					*value = mali_afbc1_t7_backup[addr - base];
					return 0;
				}
		}
		base = VPU_MAFBC2_IRQ_MASK;
		if (addr >= VPU_MAFBC2_IRQ_MASK &&
		    addr <= VPU_MAFBC2_PREFETCH_CFG_S3 &&
		    (backup_enable & HW_RESET_MALI_AFBCD2_REGS)) {
			for (i = 0; i < MALI_AFBC2_REG_T7_BACKUP_COUNT; i++)
				if (addr == mali_afbc2_reg_t7_backup[i]) {
					*value = mali_afbc2_t7_backup[addr - base];
					return 0;
				}
		}
	} else {
		base = VPU_MAFBC_IRQ_MASK;
		if (addr >= VPU_MAFBC_IRQ_MASK &&
		    addr <= VPU_MAFBC_PREFETCH_CFG_S2 &&
		    (backup_enable & HW_RESET_MALI_AFBCD_REGS)) {
			for (i = 0; i < MALI_AFBC_REG_BACKUP_COUNT; i++)
				if (addr == mali_afbc_reg_backup[i]) {
					*value = mali_afbc_backup[addr - base];
					return 0;
				}
		}
	}
	return -1;
}

void backup_regs_init(u32 backup_mask)
{
	int i = 0;
	u32 addr;
	u32 base = VIU_OSD1_CTRL_STAT;

	if (backup_enable)
		return;

	while ((backup_mask & HW_RESET_OSD1_REGS) &&
	       (i < OSD_REG_BACKUP_COUNT)) {
		addr = osd_reg_backup[i];
		osd_backup[addr - base] =
			osd_reg_read(addr);
		i++;
	}
	i = 0;
	base = OSD1_AFBCD_ENABLE;
	while ((backup_mask & HW_RESET_AFBCD_REGS) &&
	       (i < OSD_AFBC_REG_BACKUP_COUNT)) {
		addr = osd_afbc_reg_backup[i];
		osd_afbc_backup[addr - base] =
			osd_reg_read(addr);
		i++;
	}
	if (osd_dev_hw.multi_afbc_core) {
		i = 0;
		base = VPU_MAFBC_IRQ_MASK;
		while ((backup_mask & HW_RESET_MALI_AFBCD_REGS) &&
		       (i < MALI_AFBC_REG_T7_BACKUP_COUNT)) {
			addr = mali_afbc_reg_t7_backup[i];
			mali_afbc_t7_backup[addr - base] =
				osd_reg_read(addr);
			i++;
		}
		i = 0;
		base = VPU_MAFBC1_IRQ_MASK;
		while ((backup_mask & HW_RESET_MALI_AFBCD1_REGS) &&
		       (i < MALI_AFBC1_REG_T7_BACKUP_COUNT)) {
			addr = mali_afbc1_reg_t7_backup[i];
			mali_afbc1_t7_backup[addr - base] =
				osd_reg_read(addr);
			i++;
		}
		i = 0;
		base = VPU_MAFBC2_IRQ_MASK;
		while ((backup_mask & HW_RESET_MALI_AFBCD2_REGS) &&
		       (i < MALI_AFBC2_REG_T7_BACKUP_COUNT)) {
			addr = mali_afbc2_reg_t7_backup[i];
			mali_afbc2_t7_backup[addr - base] =
				osd_reg_read(addr);
			i++;
		}
	} else {
		i = 0;
		base = VPU_MAFBC_IRQ_MASK;
		while ((backup_mask & HW_RESET_MALI_AFBCD_REGS) &&
		       (i < MALI_AFBC_REG_BACKUP_COUNT)) {
			addr = mali_afbc_reg_backup[i];
			mali_afbc_backup[addr - base] =
				osd_reg_read(addr);
			i++;
		}
	}
	backup_enable = backup_mask;
}

u32 is_backup(void)
{
	return backup_enable;
}

/* recovery section */
#define INVALID_REG_ITEM {0xffff, 0x0, 0x0, 0x0}
#define REG_RECOVERY_TABLE 15

static struct reg_recovery_table gRecovery[REG_RECOVERY_TABLE];
static u32 recovery_enable;

/* Before G12A Chip */
static struct reg_item osd1_recovery_table[] = {
	{VIU_OSD1_CTRL_STAT, 0x0, 0x401ff9f1, 1},
	INVALID_REG_ITEM, /* VIU_OSD1_COLOR_ADDR 0x1a11 */
	INVALID_REG_ITEM, /* VIU_OSD1_COLOR 0x1a12 */
	{VIU_OSD1_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	INVALID_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W4 0x1a14 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W4 0x1a15 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W4 0x1a16 */
	{VIU_OSD1_TCOLOR_AG0, 0x0, 0xffffffff, 1},
	{VIU_OSD1_TCOLOR_AG1, 0x0, 0xffffffff, 0},
	{VIU_OSD1_TCOLOR_AG2, 0x0, 0xffffffff, 0},
	{VIU_OSD1_TCOLOR_AG3, 0x0, 0xffffffff, 0},
	{VIU_OSD1_BLK0_CFG_W0, 0x0, 0x30ffffff, 1},
	{VIU_OSD1_BLK0_CFG_W1, 0x0, 0x1fff1fff, 1},
	{VIU_OSD1_BLK0_CFG_W2, 0x0, 0x1fff1fff, 1},
	{VIU_OSD1_BLK0_CFG_W3, 0x0, 0x0fff0fff, 1},
	INVALID_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W0 0x1a1f */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W1 0x1a20 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W2 0x1a21 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W3 0x1a22 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W0 0x1a23 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W1 0x1a24 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W2 0x1a25 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W3 0x1a26 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W0 0x1a27 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W1 0x1a28 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W2 0x1a29 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W3 0x1a2a */
	{VIU_OSD1_FIFO_CTRL_STAT, 0x0, 0xffc3ffff, 1},
	INVALID_REG_ITEM, /* VIU_OSD1_TEST_RDDATA 0x1a2c */
	{VIU_OSD1_CTRL_STAT2, 0x0, 0x0000ffff, 1},
};

static struct reg_item osd_afbcd_recovery_table[] = {
	{OSD1_AFBCD_ENABLE, 0x0, 0x0000ff01, 1},
	{OSD1_AFBCD_MODE, 0x0, 0x937f007f, 1},
	{OSD1_AFBCD_SIZE_IN, 0x0, 0xffffffff, 1},
	{OSD1_AFBCD_HDR_PTR, 0x0, 0xffffffff, 1},
	{OSD1_AFBCD_FRAME_PTR, 0x0, 0xffffffff, 1},
	{OSD1_AFBCD_CHROMA_PTR, 0x0, 0xffffffff, 1},
	{OSD1_AFBCD_CONV_CTRL, 0x0, 0x0000ffff, 1},
	INVALID_REG_ITEM, /* unused 0x31a7 */
	INVALID_REG_ITEM, /* OSD1_AFBCD_STATUS 0x31a8 */
	{OSD1_AFBCD_PIXEL_HSCOPE, 0x0, 0xffffffff, 1},
	{OSD1_AFBCD_PIXEL_VSCOPE, 0x0, 0xffffffff, 1}
};

static struct reg_item freescale_recovery_table[] = {
	{VPP_OSD_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{VPP_OSD_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{VPP_OSD_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{VPP_OSD_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{VPP_OSD_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{VPP_OSD_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{VPP_OSD_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{VPP_OSD_SC_DUMMY_DATA, 0x0, 0xffffffff, 0},
	{VPP_OSD_SC_CTRL0, 0x0, 0x00007fff, 1},
	{VPP_OSD_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{VPP_OSD_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{VPP_OSD_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
	{VPP_OSD_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{VPP_OSD_SCALE_COEF, 0x0, 0xffffffff, 0}
};

static struct reg_item osd2_recovery_table[] = {
	{VIU_OSD2_CTRL_STAT, 0x0, 0x401ff9f1, 1},
	INVALID_REG_ITEM, /* VIU_OSD2_COLOR_ADDR 0x1a31 */
	INVALID_REG_ITEM, /* VIU_OSD2_COLOR 0x1a32 */
	INVALID_REG_ITEM, /* VIU_OSD2_HL1_H_START_END 0x1a33 */
	INVALID_REG_ITEM, /* VIU_OSD2_HL1_V_START_END 0x1a34 */
	INVALID_REG_ITEM, /* VIU_OSD2_HL2_H_START_END 0x1a35 */
	INVALID_REG_ITEM, /* VIU_OSD2_HL2_V_START_END 0x1a36 */
	{VIU_OSD2_TCOLOR_AG0, 0x0, 0xffffffff, 1},
	{VIU_OSD2_TCOLOR_AG1, 0x0, 0xffffffff, 0},
	{VIU_OSD2_TCOLOR_AG2, 0x0, 0xffffffff, 0},
	{VIU_OSD2_TCOLOR_AG3, 0x0, 0xffffffff, 0},
	{VIU_OSD2_BLK0_CFG_W0, 0x0, 0x00ffffff, 1},
	{VIU_OSD2_BLK0_CFG_W1, 0x0, 0x1fff1fff, 1},
	{VIU_OSD2_BLK0_CFG_W2, 0x0, 0x1fff1fff, 1},
	{VIU_OSD2_BLK0_CFG_W3, 0x0, 0x0fff0fff, 1},
	{VIU_OSD2_BLK1_CFG_W0, 0x0, 0x00ffffff, 0},
	{VIU_OSD2_BLK1_CFG_W1, 0x0, 0x1fff1fff, 0},
	{VIU_OSD2_BLK1_CFG_W2, 0x0, 0x1fff1fff, 0},
	{VIU_OSD2_BLK1_CFG_W3, 0x0, 0x0fff0fff, 0},
	{VIU_OSD2_BLK2_CFG_W0, 0x0, 0x00ffffff, 0},
	{VIU_OSD2_BLK2_CFG_W1, 0x0, 0x1fff1fff, 0},
	{VIU_OSD2_BLK2_CFG_W2, 0x0, 0x1fff1fff, 0},
	{VIU_OSD2_BLK2_CFG_W3, 0x0, 0x0fff0fff, 0},
	{VIU_OSD2_BLK3_CFG_W0, 0x0, 0x00ffffff, 0},
	{VIU_OSD2_BLK3_CFG_W1, 0x0, 0x1fff1fff, 0},
	{VIU_OSD2_BLK3_CFG_W2, 0x0, 0x1fff1fff, 0},
	{VIU_OSD2_BLK3_CFG_W3, 0x0, 0x0fff0fff, 0},
	{VIU_OSD2_FIFO_CTRL_STAT, 0x0, 0xffc3ffff, 1},
	INVALID_REG_ITEM, /* VIU_OSD2_TEST_RDDATA 0x1a4c */
	{VIU_OSD2_CTRL_STAT2, 0x0, 0x00007ffd, 1}
};

static struct reg_item misc_recovery_table[] = {
	{VIU_OSD2_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	{VIU_OSD2_BLK1_CFG_W4, 0x0, 0xffffffff, 0},
	{VIU_OSD2_BLK2_CFG_W4, 0x0, 0xffffffff, 0},
	{VIU_OSD2_BLK3_CFG_W4, 0x0, 0xffffffff, 0},
	{VPU_RDARB_MODE_L1C2, 0x0, 0x00010000, 1},
	{VIU_MISC_CTRL1, 0x0, 0x0000ff00, 1},
	{AMDV_CORE2A_SWAP_CTRL1, 0x0, 0x0fffffff, 1},
	{AMDV_CORE2A_SWAP_CTRL2, 0x0, 0xffffffff, 1}
};

/* After G12A Chip */
static struct reg_item osd12_recovery_table_g12a[] = {
	/* osd1 */
	{VIU_OSD1_CTRL_STAT, 0x0, 0xc01ff9f7, 1},
	INVALID_REG_ITEM, /* VIU_OSD1_COLOR_ADDR 0x1a11 */
	INVALID_REG_ITEM, /* VIU_OSD1_COLOR 0x1a12 */
	{VIU_OSD1_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	{VIU_OSD1_BLK1_CFG_W4, 0x0, 0xffffffff, 1},
	{VIU_OSD1_BLK2_CFG_W4, 0x0, 0xffffffff, 1},
	INVALID_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W4 0x1a16 */
	{VIU_OSD1_TCOLOR_AG0, 0x0, 0xffffffff, 1},
	{VIU_OSD1_TCOLOR_AG1, 0x0, 0xffffffff, 0},
	{VIU_OSD1_TCOLOR_AG2, 0x0, 0xffffffff, 0},
	{VIU_OSD1_TCOLOR_AG3, 0x0, 0xffffffff, 0},
	{VIU_OSD1_BLK0_CFG_W0, 0x0, 0x70ffff7f, 1},
	{VIU_OSD1_BLK0_CFG_W1, 0x0, 0x1fff1fff, 1},
	{VIU_OSD1_BLK0_CFG_W2, 0x0, 0x1fff1fff, 1},
	{VIU_OSD1_BLK0_CFG_W3, 0x0, 0x0fff0fff, 1},
	INVALID_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W0 0x1a1f */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W1 0x1a20 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W2 0x1a21 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W3 0x1a22 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W0 0x1a23 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W1 0x1a24 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W2 0x1a25 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W3 0x1a26 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W0 0x1a27 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W1 0x1a28 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W2 0x1a29 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W3 0x1a2a */
	{VIU_OSD1_FIFO_CTRL_STAT, 0x0, 0xffc7ffff, 1},
	INVALID_REG_ITEM, /* VIU_OSD1_TEST_RDDATA 0x1a2c */
	{VIU_OSD1_CTRL_STAT2, 0x0, 0x0000ffff, 1},
	{VIU_OSD1_PROT_CTRL, 0x0, 0xffff0000, 1},
	{VIU_OSD1_MALI_UNPACK_CTRL, 0x0, 0x9f01ffff, 1},
	/* osd2 */
	{VIU_OSD2_CTRL_STAT, 0x0, 0xc01ff9f7, 1},
	INVALID_REG_ITEM, /* VIU_OSD2_COLOR_ADDR 0x1a31 */
	INVALID_REG_ITEM, /* VIU_OSD2_COLOR 0x1a32 */
	INVALID_REG_ITEM, /* 0x1a33 */
	INVALID_REG_ITEM, /* 0x1a34 */
	INVALID_REG_ITEM, /* 0x1a35 */
	INVALID_REG_ITEM, /* 0x1a36 */
	{VIU_OSD2_TCOLOR_AG0, 0x0, 0xffffffff, 1},
	{VIU_OSD2_TCOLOR_AG1, 0x0, 0xffffffff, 0},
	{VIU_OSD2_TCOLOR_AG2, 0x0, 0xffffffff, 0},
	{VIU_OSD2_TCOLOR_AG3, 0x0, 0xffffffff, 0},
	{VIU_OSD2_BLK0_CFG_W0, 0x0, 0x70ffff7f, 1},
	{VIU_OSD2_BLK0_CFG_W1, 0x0, 0x1fff1fff, 1},
	{VIU_OSD2_BLK0_CFG_W2, 0x0, 0x1fff1fff, 1},
	{VIU_OSD2_BLK0_CFG_W3, 0x0, 0x0fff0fff, 1},
	INVALID_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W0 0x1a3f */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W1 0x1a40 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W2 0x1a41 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK1_CFG_W3 0x1a42 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W0 0x1a43 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W1 0x1a44 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W2 0x1a45 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK2_CFG_W3 0x1a46 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W0 0x1a47 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W1 0x1a48 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W2 0x1a49 */
	INVALID_REG_ITEM, /* VIU_OSD1_BLK3_CFG_W3 0x1a4a */
	{VIU_OSD2_FIFO_CTRL_STAT, 0x0, 0xffc7ffff, 1},
	INVALID_REG_ITEM, /* VIU_OSD2_TEST_RDDATA 0x1a4c */
	{VIU_OSD2_CTRL_STAT2, 0x0, 0x0000ffff, 1},
	{VIU_OSD2_PROT_CTRL, 0x0, 0xffff0000, 1},
};

static struct reg_item osd3_recovery_table_g12a[] = {
	{VIU_OSD3_CTRL_STAT, 0x0, 0xc01ff9f7, 1},
	{VIU_OSD3_CTRL_STAT2, 0x0, 0x0000ffff, 1},
	INVALID_REG_ITEM, /* VIU_OSD3_COLOR_ADDR 0x3d82 */
	INVALID_REG_ITEM, /* VIU_OSD3_COLOR 0x3d83 */
	{VIU_OSD3_TCOLOR_AG0, 0x0, 0xffffffff, 1},
	{VIU_OSD3_TCOLOR_AG1, 0x0, 0xffffffff, 0},
	{VIU_OSD3_TCOLOR_AG2, 0x0, 0xffffffff, 0},
	{VIU_OSD3_TCOLOR_AG3, 0x0, 0xffffffff, 0},
	{VIU_OSD3_BLK0_CFG_W0, 0x0, 0x70ffff7f, 1},
	INVALID_REG_ITEM, /* 0x3d89 */
	INVALID_REG_ITEM, /* 0x3d8a */
	INVALID_REG_ITEM, /* 0x3d8b */
	{VIU_OSD3_BLK0_CFG_W1, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x3d8d */
	INVALID_REG_ITEM, /* 0x3d8e */
	INVALID_REG_ITEM, /* 0x3d8f */
	{VIU_OSD3_BLK0_CFG_W2, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x3d91 */
	INVALID_REG_ITEM, /* 0x3d92 */
	INVALID_REG_ITEM, /* 0x3d93 */
	{VIU_OSD3_BLK0_CFG_W3, 0x0, 0x0fff0fff, 1},
	INVALID_REG_ITEM, /* 0x3d95 */
	INVALID_REG_ITEM, /* 0x3d96 */
	INVALID_REG_ITEM, /* 0x3d97 */
	{VIU_OSD3_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	{VIU_OSD3_BLK1_CFG_W4, 0x0, 0xffffffff, 1},
	{VIU_OSD3_BLK2_CFG_W4, 0x0, 0xffffffff, 1},
	INVALID_REG_ITEM, /* 0x3d9b */
	{VIU_OSD3_FIFO_CTRL_STAT, 0x0, 0xffc7ffff, 1},
	INVALID_REG_ITEM, /* VIU_OSD3_TEST_RDDATA 0x3d9d */
	{VIU_OSD3_PROT_CTRL, 0x0, 0xffff0000, 1},
	{VIU_OSD3_MALI_UNPACK_CTRL, 0x0, 0x9f01ffff, 1},
	{VIU_OSD3_DIMM_CTRL, 0x0, 0x7fffffff, 1},
};

static struct reg_item osd4_recovery_table_t7[] = {
	{VIU_OSD4_CTRL_STAT, 0x0, 0xc01ff9f7, 1},
	{VIU_OSD4_CTRL_STAT2, 0x0, 0x0000ffff, 1},
	INVALID_REG_ITEM, /* VIU_OSD4_COLOR_ADDR 0x3d82 */
	INVALID_REG_ITEM, /* VIU_OSD4_COLOR 0x3d83 */
	{VIU_OSD4_TCOLOR_AG0, 0x0, 0xffffffff, 1},
	{VIU_OSD4_TCOLOR_AG1, 0x0, 0xffffffff, 0},
	{VIU_OSD4_TCOLOR_AG2, 0x0, 0xffffffff, 0},
	{VIU_OSD4_TCOLOR_AG3, 0x0, 0xffffffff, 0},
	{VIU_OSD4_BLK0_CFG_W0, 0x0, 0x70ffff7f, 1},
	INVALID_REG_ITEM, /* 0x3d89 */
	INVALID_REG_ITEM, /* 0x3d8a */
	INVALID_REG_ITEM, /* 0x3d8b */
	{VIU_OSD4_BLK0_CFG_W1, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x3d8d */
	INVALID_REG_ITEM, /* 0x3d8e */
	INVALID_REG_ITEM, /* 0x3d8f */
	{VIU_OSD4_BLK0_CFG_W2, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x3d91 */
	INVALID_REG_ITEM, /* 0x3d92 */
	INVALID_REG_ITEM, /* 0x3d93 */
	{VIU_OSD4_BLK0_CFG_W3, 0x0, 0x0fff0fff, 1},
	INVALID_REG_ITEM, /* 0x3d95 */
	INVALID_REG_ITEM, /* 0x3d96 */
	INVALID_REG_ITEM, /* 0x3d97 */
	{VIU_OSD4_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	{VIU_OSD4_BLK1_CFG_W4, 0x0, 0xffffffff, 1},
	{VIU_OSD4_BLK2_CFG_W4, 0x0, 0xffffffff, 1},
	INVALID_REG_ITEM, /* 0x3d9b */
	{VIU_OSD4_FIFO_CTRL_STAT, 0x0, 0xffc7ffff, 1},
	INVALID_REG_ITEM, /* VIU_OSD3_TEST_RDDATA 0x3d9d */
	{VIU_OSD4_PROT_CTRL, 0x0, 0xffff0000, 1},
	{VIU_OSD4_MALI_UNPACK_CTRL, 0x0, 0x9f01ffff, 1},
	{VIU_OSD4_DIMM_CTRL, 0x0, 0x7fffffff, 1},
};

/* After S5 Chip */
static struct reg_item osd1_recovery_table_s5[] = {
	/* osd1 */
	{S5_VIU_OSD1_CTRL_STAT, 0x0, 0xc01ff9f7, 1},
	{S5_VIU_OSD1_CTRL_STAT2, 0x0, 0x0000ffff, 1},
	INVALID_REG_ITEM, /* VIU_OSD1_COLOR_ADDR 0x4242 */
	INVALID_REG_ITEM, /* VIU_OSD1_COLOR 0x4243 */
	{S5_VIU_OSD1_TCOLOR_AG0, 0x0, 0xffffffff, 1},
	{S5_VIU_OSD1_TCOLOR_AG1, 0x0, 0xffffffff, 0},
	{S5_VIU_OSD1_TCOLOR_AG2, 0x0, 0xffffffff, 0},
	{S5_VIU_OSD1_TCOLOR_AG3, 0x0, 0xffffffff, 0},
	{S5_VIU_OSD1_BLK0_CFG_W0, 0x0, 0x70ffff7f, 1},
	INVALID_REG_ITEM, /* 0x4249 */
	INVALID_REG_ITEM, /* 0x424a */
	INVALID_REG_ITEM, /* 0x424b */
	{S5_VIU_OSD1_BLK0_CFG_W1, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x424d */
	INVALID_REG_ITEM, /* 0x424e */
	INVALID_REG_ITEM, /* 0x424f */
	{S5_VIU_OSD1_BLK0_CFG_W2, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x4251 */
	INVALID_REG_ITEM, /* 0x4252 */
	INVALID_REG_ITEM, /* 0x4253 */
	{S5_VIU_OSD1_BLK0_CFG_W3, 0x0, 0x0fff0fff, 1},
	INVALID_REG_ITEM, /* 0x4255 */
	INVALID_REG_ITEM, /* 0x4256 */
	INVALID_REG_ITEM, /* 0x4257 */
	{S5_VIU_OSD1_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	{S5_VIU_OSD1_BLK1_CFG_W4, 0x0, 0xffffffff, 1},
	{S5_VIU_OSD1_BLK2_CFG_W4, 0x0, 0xffffffff, 1},
	INVALID_REG_ITEM, /* 0x425b */
	{S5_VIU_OSD1_FIFO_CTRL_STAT, 0x0, 0xffc7ffff, 1},
	INVALID_REG_ITEM, /* S5_VIU_OSD1_TEST_RDDATA 0x425d*/
	{S5_VIU_OSD1_PROT_CTRL, 0x0, 0xffff0000, 1},
	{S5_VIU_OSD1_MALI_UNPACK_CTRL, 0x0, 0x9f01ffff, 1},
	{S5_VIU_OSD1_DIMM_CTRL, 0x0, 0x7fffffff, 1},
};

static struct reg_item osd2_recovery_table_s5[] = {
	/* osd2 */
	{S5_VIU_OSD2_CTRL_STAT, 0x0, 0xc01ff9f7, 1},
	{S5_VIU_OSD2_CTRL_STAT2, 0x0, 0x0000ffff, 1},
	INVALID_REG_ITEM, /* VIU_OSD2_COLOR_ADDR 0x4202 */
	INVALID_REG_ITEM, /* VIU_OSD2_COLOR 0x4203 */
	{S5_VIU_OSD2_TCOLOR_AG0, 0x0, 0xffffffff, 1},
	{S5_VIU_OSD2_TCOLOR_AG1, 0x0, 0xffffffff, 0},
	{S5_VIU_OSD2_TCOLOR_AG2, 0x0, 0xffffffff, 0},
	{S5_VIU_OSD2_TCOLOR_AG3, 0x0, 0xffffffff, 0},
	{S5_VIU_OSD2_BLK0_CFG_W0, 0x0, 0x70ffff7f, 1},
	INVALID_REG_ITEM, /* 0x4209 */
	INVALID_REG_ITEM, /* 0x420a */
	INVALID_REG_ITEM, /* 0x420b */
	{S5_VIU_OSD2_BLK0_CFG_W1, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x420d */
	INVALID_REG_ITEM, /* 0x420e */
	INVALID_REG_ITEM, /* 0x420f */
	{S5_VIU_OSD2_BLK0_CFG_W2, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x4211 */
	INVALID_REG_ITEM, /* 0x4212 */
	INVALID_REG_ITEM, /* 0x4213 */
	{S5_VIU_OSD2_BLK0_CFG_W3, 0x0, 0x0fff0fff, 1},
	INVALID_REG_ITEM, /* 0x4215 */
	INVALID_REG_ITEM, /* 0x4216 */
	INVALID_REG_ITEM, /* 0x4217 */
	{S5_VIU_OSD2_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	{S5_VIU_OSD2_BLK1_CFG_W4, 0x0, 0xffffffff, 1},
	{S5_VIU_OSD2_BLK2_CFG_W4, 0x0, 0xffffffff, 1},
	INVALID_REG_ITEM, /* 0x421b */
	{S5_VIU_OSD2_FIFO_CTRL_STAT, 0x0, 0xffc7ffff, 1},
	INVALID_REG_ITEM, /* S5_VIU_OSD2_TEST_RDDATA 0x421d*/
	{S5_VIU_OSD2_PROT_CTRL, 0x0, 0xffff0000, 1},
	{S5_VIU_OSD2_MALI_UNPACK_CTRL, 0x0, 0x9f01ffff, 1},
	{S5_VIU_OSD2_DIMM_CTRL, 0x0, 0x7fffffff, 1},
};

static struct reg_item osd3_recovery_table_s5[] = {
	{S5_VIU_OSD3_CTRL_STAT, 0x0, 0xc01ff9f7, 1},
	{S5_VIU_OSD3_CTRL_STAT2, 0x0, 0x0000ffff, 1},
	INVALID_REG_ITEM, /* S5_VIU_OSD3_COLOR_ADDR 0x4282 */
	INVALID_REG_ITEM, /* S5_VIU_OSD3_COLOR 0x4283 */
	{S5_VIU_OSD3_TCOLOR_AG0, 0x0, 0xffffffff, 1},
	{S5_VIU_OSD3_TCOLOR_AG1, 0x0, 0xffffffff, 0},
	{S5_VIU_OSD3_TCOLOR_AG2, 0x0, 0xffffffff, 0},
	{S5_VIU_OSD3_TCOLOR_AG3, 0x0, 0xffffffff, 0},
	{S5_VIU_OSD3_BLK0_CFG_W0, 0x0, 0x70ffff7f, 1},
	INVALID_REG_ITEM, /* 0x4289 */
	INVALID_REG_ITEM, /* 0x428a */
	INVALID_REG_ITEM, /* 0x428b */
	{S5_VIU_OSD3_BLK0_CFG_W1, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x428d */
	INVALID_REG_ITEM, /* 0x428e */
	INVALID_REG_ITEM, /* 0x428f */
	{S5_VIU_OSD3_BLK0_CFG_W2, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x4291 */
	INVALID_REG_ITEM, /* 0x4292 */
	INVALID_REG_ITEM, /* 0x4293 */
	{S5_VIU_OSD3_BLK0_CFG_W3, 0x0, 0x0fff0fff, 1},
	INVALID_REG_ITEM, /* 0x4295 */
	INVALID_REG_ITEM, /* 0x4296 */
	INVALID_REG_ITEM, /* 0x4297 */
	{S5_VIU_OSD3_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	{S5_VIU_OSD3_BLK1_CFG_W4, 0x0, 0xffffffff, 1},
	{S5_VIU_OSD3_BLK2_CFG_W4, 0x0, 0xffffffff, 1},
	INVALID_REG_ITEM, /* 0x429b */
	{S5_VIU_OSD3_FIFO_CTRL_STAT, 0x0, 0xffc7ffff, 1},
	INVALID_REG_ITEM, /* S5_VIU_OSD3_TEST_RDDATA 0x429d */
	{S5_VIU_OSD3_PROT_CTRL, 0x0, 0xffff0000, 1},
	{S5_VIU_OSD3_MALI_UNPACK_CTRL, 0x0, 0x9f01ffff, 1},
	{S5_VIU_OSD3_DIMM_CTRL, 0x0, 0x7fffffff, 1},
};

static struct reg_item osd1_sc_recovery_table_g12a[] = {
	{VPP_OSD_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{VPP_OSD_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{VPP_OSD_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{VPP_OSD_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{VPP_OSD_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{VPP_OSD_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{VPP_OSD_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{VPP_OSD_SC_DUMMY_DATA, 0x0, 0xffffffff, 0},
	{VPP_OSD_SC_CTRL0, 0x0, 0x0fff3ffc, 1},
	{VPP_OSD_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{VPP_OSD_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{VPP_OSD_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
	{VPP_OSD_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{VPP_OSD_SCALE_COEF, 0x0, 0xffffffff, 0}
};

static struct reg_item osd2_sc_recovery_table_g12a[] = {
	{OSD2_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{OSD2_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{OSD2_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{OSD2_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{OSD2_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{OSD2_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{OSD2_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{OSD2_SC_DUMMY_DATA, 0x0, 0xffffffff, 0},
	{OSD2_SC_CTRL0, 0x0, 0x0fff3ffc, 1},
	{OSD2_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{OSD2_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{OSD2_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
	INVALID_REG_ITEM, /* 0x3d0c */
	INVALID_REG_ITEM, /* 0x3d0d */
	INVALID_REG_ITEM, /* 0x3d0e */
	INVALID_REG_ITEM, /* 0x3d0f */
	INVALID_REG_ITEM, /* 0x3d10 */
	INVALID_REG_ITEM, /* 0x3d11 */
	INVALID_REG_ITEM, /* 0x3d12 */
	INVALID_REG_ITEM, /* 0x3d13 */
	INVALID_REG_ITEM, /* 0x3d14 */
	INVALID_REG_ITEM, /* 0x3d15 */
	INVALID_REG_ITEM, /* 0x3d16 */
	INVALID_REG_ITEM, /* 0x3d17 */
	{OSD2_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{OSD2_SCALE_COEF, 0x0, 0xffffffff, 0},
};

static struct reg_item osd3_sc_recovery_table_g12a[] = {
	{OSD34_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{OSD34_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{OSD34_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{OSD34_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{OSD34_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{OSD34_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{OSD34_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{OSD34_SC_DUMMY_DATA, 0x0, 0xffffffff, 0},
	{OSD34_SC_CTRL0, 0x0, 0x0fff3ffc, 1},
	{OSD34_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{OSD34_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{OSD34_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
	{OSD34_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{OSD34_SCALE_COEF, 0x0, 0xffffffff, 0},

};

static struct reg_item osd1_sc_recovery_table_t7[] = {
	{T7_VPP_OSD_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{T7_VPP_OSD_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{T7_VPP_OSD_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{T7_VPP_OSD_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{T7_VPP_OSD_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{T7_VPP_OSD_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{T7_VPP_OSD_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{T7_VPP_OSD_SC_DUMMY_DATA, 0x0, 0xffffffff, 1},
	{T7_VPP_OSD_SC_CTRL0, 0x0, 0x0fff3ffc, 1},
	{T7_VPP_OSD_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{T7_VPP_OSD_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{T7_VPP_OSD_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
	{T7_VPP_OSD_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{T7_VPP_OSD_SCALE_COEF, 0x0, 0xffffffff, 0}
};

static struct reg_item osd2_sc_recovery_table_t7[] = {
	{T7_OSD2_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{T7_OSD2_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{T7_OSD2_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{T7_OSD2_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{T7_OSD2_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{T7_OSD2_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{T7_OSD2_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{T7_OSD2_SC_DUMMY_DATA, 0x0, 0xffffffff, 1},
	{T7_OSD2_SC_CTRL0, 0x0, 0x0fff3ffc, 1},
	{T7_OSD2_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{T7_OSD2_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{T7_OSD2_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
	{T7_OSD2_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{T7_OSD2_SCALE_COEF, 0x0, 0xffffffff, 0},
};

static struct reg_item osd3_sc_recovery_table_t7[] = {
	{T7_OSD34_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{T7_OSD34_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{T7_OSD34_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{T7_OSD34_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{T7_OSD34_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{T7_OSD34_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{T7_OSD34_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{T7_OSD34_SC_DUMMY_DATA, 0x0, 0xffffffff, 1},
	{T7_OSD34_SC_CTRL0, 0x0, 0x0fff3ffc, 1},
	{T7_OSD34_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{T7_OSD34_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{T7_OSD34_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
	{T7_OSD34_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{T7_OSD34_SCALE_COEF, 0x0, 0xffffffff, 0},
};

static struct reg_item osd4_sc_recovery_table_t7[] = {
	{T7_OSD4_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{T7_OSD4_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{T7_OSD4_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{T7_OSD4_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{T7_OSD4_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{T7_OSD4_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{T7_OSD4_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{T7_OSD4_SC_DUMMY_DATA, 0x0, 0xffffffff, 0},
	{T7_OSD4_SC_CTRL0, 0x0, 0x0fff3ffc, 1},
	{T7_OSD4_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{T7_OSD4_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{T7_OSD4_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
	{T7_OSD4_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{T7_OSD4_SCALE_COEF, 0x0, 0xffffffff, 0},
};

static struct reg_item osd1_sc_recovery_table_s5[] = {
	{OSD1_PROC_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{OSD1_PROC_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{OSD1_PROC_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{OSD1_PROC_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{OSD1_PROC_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{OSD1_PROC_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{OSD1_PROC_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{OSD1_PROC_SC_DUMMY_DATA, 0x0, 0xffffffff, 1},
	{OSD1_PROC_SC_CTRL0, 0x0, 0x0fff3ffc, 1},
	{OSD1_PROC_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{OSD1_PROC_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{OSD1_PROC_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
	{OSD1_PROC_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{OSD1_PROC_SCALE_COEF, 0x0, 0xffffffff, 0}
};

static struct reg_item osd2_sc_recovery_table_s5[] = {
	{OSD2_PROC_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{OSD2_PROC_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{OSD2_PROC_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{OSD2_PROC_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{OSD2_PROC_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{OSD2_PROC_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{OSD2_PROC_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{OSD2_PROC_SC_DUMMY_DATA, 0x0, 0xffffffff, 1},
	{OSD2_PROC_SC_CTRL0, 0x0, 0x0fff3ffc, 1},
	{OSD2_PROC_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{OSD2_PROC_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{OSD2_PROC_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
	{OSD2_PROC_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{OSD2_PROC_SCALE_COEF, 0x0, 0xffffffff, 0},
};

static struct reg_item osd3_sc_recovery_table_s5[] = {
	{OSD3_PROC_VSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{OSD3_PROC_VSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{OSD3_PROC_VSC_CTRL0, 0x0, 0x01fb7b7f, 1},
	{OSD3_PROC_HSC_PHASE_STEP, 0x0, 0x0fffffff, 1},
	{OSD3_PROC_HSC_INI_PHASE, 0x0, 0xffffffff, 1},
	{OSD3_PROC_HSC_CTRL0, 0x0, 0x007b7b7f, 1},
	{OSD3_PROC_HSC_INI_PAT_CTRL, 0x0, 0x0000ff77, 1},
	{OSD3_PROC_SC_DUMMY_DATA, 0x0, 0xffffffff, 1},
	{OSD3_PROC_SC_CTRL0, 0x0, 0x0fff3ffc, 1},
	{OSD3_PROC_SCI_WH_M1, 0x0, 0x1fff1fff, 1},
	{OSD3_PROC_SCO_H_START_END, 0x0, 0x0fff0fff, 1},
	{OSD3_PROC_SCO_V_START_END, 0x0, 0x0fff0fff, 1},
	{OSD3_PROC_SCALE_COEF_IDX, 0x0, 0x0000c37f, 0},
	{OSD3_PROC_SCALE_COEF, 0x0, 0xffffffff, 0},
};

static struct reg_item vpu_afbcd_recovery_table_g12a[] = {
	{
		VPU_MAFBC_BLOCK_ID,
		0x0, 0xffffffff, 0
	},
	{
		VPU_MAFBC_IRQ_RAW_STATUS,
		0x0, 0x0000003f, 0
	},
	{
		VPU_MAFBC_IRQ_CLEAR,
		0x0, 0x0000003f, 0
	},
	{
		VPU_MAFBC_IRQ_MASK,
		0x0, 0x0000003f, 1
	},
	{
		VPU_MAFBC_IRQ_STATUS,
		0x0, 0x0000003f, 0
	},
	{
		VPU_MAFBC_COMMAND,
		0x0, 0x00000003, 0
	},
	{
		VPU_MAFBC_STATUS,
		0x0, 0x00000007, 0
	},
	{
		VPU_MAFBC_SURFACE_CFG,
		0x0, 0x0001000f, 1
	}
};

static struct reg_item osd1_afbcd_recovery_table_g12a[] = {
	{
		VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0,
		0x0, 0xffffffff, 1
	},
	{
		VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S0,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_FORMAT_SPECIFIER_S0,
		0x0, 0x000f030f, 1
	},
	{
		VPU_MAFBC_BUFFER_WIDTH_S0,
		0x0, 0x00003fff, 1
	},
	{
		VPU_MAFBC_BUFFER_HEIGHT_S0,
		0x0, 0x00003fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_X_START_S0,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_X_END_S0,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_Y_START_S0,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_Y_END_S0,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S0,
		0x0, 0xffffffff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S0,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_STRIDE_S0,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_PREFETCH_CFG_S0, 0x0, 3, 1
	}
};

static struct reg_item osd2_afbcd_recovery_table_g12a[] = {
	{
		VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1,
		0x0, 0xffffffff, 1
	},
	{
		VPU_MAFBC_HEADER_BUF_ADDR_HIGH_S1,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_FORMAT_SPECIFIER_S1,
		0x0, 0x000f030f, 1
	},
	{
		VPU_MAFBC_BUFFER_WIDTH_S1,
		0x0, 0x00003fff, 1
	},
	{
		VPU_MAFBC_BUFFER_HEIGHT_S1,
		0x0, 0x00003fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_X_START_S1,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_X_END_S1,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_Y_START_S1,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_BOUNDING_BOX_Y_END_S1,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_ADDR_LOW_S1,
		0x0, 0xffffffff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_ADDR_HIGH_S1,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_OUTPUT_BUF_STRIDE_S1,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC_PREFETCH_CFG_S1, 0x0, 3, 1
	}
};

static struct reg_item osd3_afbcd_recovery_table_g12a[] = {
	{
		VPU_MAFBC1_HEADER_BUF_ADDR_LOW_S2,
		0x0, 0xffffffff, 1
	},
	{
		VPU_MAFBC1_HEADER_BUF_ADDR_HIGH_S2,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC1_FORMAT_SPECIFIER_S2,
		0x0, 0x000f030f, 1
	},
	{
		VPU_MAFBC1_BUFFER_WIDTH_S2,
		0x0, 0x00003fff, 1
	},
	{
		VPU_MAFBC1_BUFFER_HEIGHT_S2,
		0x0, 0x00003fff, 1
	},
	{
		VPU_MAFBC1_BOUNDING_BOX_X_START_S2,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC1_BOUNDING_BOX_X_END_S2,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC1_BOUNDING_BOX_Y_START_S2,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC1_BOUNDING_BOX_Y_END_S2,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC1_OUTPUT_BUF_ADDR_LOW_S2,
		0x0, 0xffffffff, 1
	},
	{
		VPU_MAFBC1_OUTPUT_BUF_ADDR_HIGH_S2,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC1_OUTPUT_BUF_STRIDE_S2,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC1_PREFETCH_CFG_S2, 0x0, 3, 1
	}
};

static struct reg_item osd4_afbcd_recovery_table_t7[] = {
	{
		VPU_MAFBC2_HEADER_BUF_ADDR_LOW_S3,
		0x0, 0xffffffff, 1
	},
	{
		VPU_MAFBC2_HEADER_BUF_ADDR_HIGH_S3,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC2_FORMAT_SPECIFIER_S3,
		0x0, 0x000f030f, 1
	},
	{
		VPU_MAFBC2_BUFFER_WIDTH_S3,
		0x0, 0x00003fff, 1
	},
	{
		VPU_MAFBC2_BUFFER_HEIGHT_S3,
		0x0, 0x00003fff, 1
	},
	{
		VPU_MAFBC2_BOUNDING_BOX_X_START_S3,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC2_BOUNDING_BOX_X_END_S3,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC2_BOUNDING_BOX_Y_START_S3,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC2_BOUNDING_BOX_Y_END_S3,
		0x0, 0x00001fff, 1
	},
	{
		VPU_MAFBC2_OUTPUT_BUF_ADDR_LOW_S3,
		0x0, 0xffffffff, 1
	},
	{
		VPU_MAFBC2_OUTPUT_BUF_ADDR_HIGH_S3,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC2_OUTPUT_BUF_STRIDE_S3,
		0x0, 0x0000ffff, 1
	},
	{
		VPU_MAFBC2_PREFETCH_CFG_S3, 0x0, 3, 1
	}
};

static struct reg_item blend_recovery_table_g12a[] = {
	{VIU_OSD_BLEND_CTRL, 0x0, 0xffffffff, 1},
	{VIU_OSD_BLEND_DIN0_SCOPE_H, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DIN0_SCOPE_V, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DIN1_SCOPE_H, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DIN1_SCOPE_V, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DIN2_SCOPE_H, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DIN2_SCOPE_V, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DIN3_SCOPE_H, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DIN3_SCOPE_V, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_DUMMY_DATA0, 0x0, 0x00ffffff, 1},
	{VIU_OSD_BLEND_DUMMY_ALPHA, 0x0, 0x1fffffff, 1},
	{VIU_OSD_BLEND_BLEND0_SIZE, 0x0, 0x1fff1fff, 1},
	{VIU_OSD_BLEND_BLEND1_SIZE, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x39bd */
	INVALID_REG_ITEM, /* 0x39be */
	INVALID_REG_ITEM, /* 0x39bf */
	{VIU_OSD_BLEND_CTRL1, 0x0, 0x00037337, 1},
};

static struct reg_item blend_recovery_table_s5[] = {
	{S5_VIU_OSD_BLEND_CTRL, 0x0, 0xffffffff, 1},
	{S5_VIU_OSD_BLEND_DIN0_SCOPE_H, 0x0, 0x1fff1fff, 1},
	{S5_VIU_OSD_BLEND_DIN0_SCOPE_V, 0x0, 0x1fff1fff, 1},
	{S5_VIU_OSD_BLEND_DIN1_SCOPE_H, 0x0, 0x1fff1fff, 1},
	{S5_VIU_OSD_BLEND_DIN1_SCOPE_V, 0x0, 0x1fff1fff, 1},
	{S5_VIU_OSD_BLEND_DIN2_SCOPE_H, 0x0, 0x1fff1fff, 1},
	{S5_VIU_OSD_BLEND_DIN2_SCOPE_V, 0x0, 0x1fff1fff, 1},
	{S5_VIU_OSD_BLEND_DIN3_SCOPE_H, 0x0, 0x1fff1fff, 1},
	{S5_VIU_OSD_BLEND_DIN3_SCOPE_V, 0x0, 0x1fff1fff, 1},
	{S5_VIU_OSD_BLEND_DUMMY_DATA0, 0x0, 0x00ffffff, 1},
	{S5_VIU_OSD_BLEND_DUMMY_ALPHA, 0x0, 0x1fffffff, 1},
	{S5_VIU_OSD_BLEND_BLEND0_SIZE, 0x0, 0x1fff1fff, 1},
	{S5_VIU_OSD_BLEND_BLEND1_SIZE, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x60bd */
	INVALID_REG_ITEM, /* 0x60be */
	INVALID_REG_ITEM, /* 0x60bf */
	{S5_VIU_OSD_BLEND_CTRL1, 0x0, 0x00037337, 1},
};

static struct reg_item post_blend_recovery_table_g12a[] = {
	{VPP_VD2_HDR_IN_SIZE, 0x0, 0x1fff1fff, 0},
	{VPP_OSD1_IN_SIZE, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x1df2 */
	INVALID_REG_ITEM, /* 0x1df3 */
	INVALID_REG_ITEM, /* 0x1df4 */
	{VPP_OSD1_BLD_H_SCOPE, 0x0, 0x1fff1fff, 1},
	{VPP_OSD1_BLD_V_SCOPE, 0x0, 0x1fff1fff, 1},
	{VPP_OSD2_BLD_H_SCOPE, 0x0, 0x1fff1fff, 1},
	{VPP_OSD2_BLD_V_SCOPE, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x1df9 */
	INVALID_REG_ITEM, /* 0x1dfa */
	INVALID_REG_ITEM, /* 0x1dfb */
	INVALID_REG_ITEM, /* 0x1dfc */
	{OSD1_BLEND_SRC_CTRL, 0x0, 0x00110f1f, 1},
	{OSD2_BLEND_SRC_CTRL, 0x0, 0x00110f1f, 1},
};

static struct reg_item post_blend_recovery_table_s5[] = {
	{S5_VPP_OSD1_BLD_H_SCOPE, 0x0, 0x1fff1fff, 1},
	{S5_VPP_OSD1_BLD_V_SCOPE, 0x0, 0x1fff1fff, 1},
	{S5_VPP_OSD2_BLD_H_SCOPE, 0x0, 0x1fff1fff, 1},
	{S5_VPP_OSD2_BLD_V_SCOPE, 0x0, 0x1fff1fff, 1},
	INVALID_REG_ITEM, /* 0x1d0d */
	INVALID_REG_ITEM, /* 0x1d0e */
	INVALID_REG_ITEM, /* 0x1d0f */
	{S5_OSD1_BLEND_SRC_CTRL, 0x0, 0x1f, 1},
	{S5_OSD2_BLEND_SRC_CTRL, 0x0, 0x1f, 1},
};

static struct reg_item misc_recovery_table_g12a[] = {
	{AMDV_PATH_CTRL, 0x0, 0x000000cc, 1},
	{OSD_PATH_MISC_CTRL, 0x0, 0x000000ff, 1},
	{VIU_OSD1_DIMM_CTRL, 0x0, 0x7fffffff, 1},
	{VIU_OSD2_DIMM_CTRL, 0x0, 0x7fffffff, 1},
	{VIU_OSD2_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	{VIU_OSD2_BLK1_CFG_W4, 0x0, 0xffffffff, 1},
	{VIU_OSD2_BLK2_CFG_W4, 0x0, 0xffffffff, 1},
	{VIU_OSD2_MALI_UNPACK_CTRL, 0x0, 0x9f01ffff, 1},
	{AMDV_CORE2A_SWAP_CTRL1, 0x0, 0x0fffffff, 1},
	{AMDV_CORE2A_SWAP_CTRL2, 0x0, 0xffffffff, 1},
};

static struct reg_item misc_recovery_table_t7[] = {
	{AMDV_PATH_CTRL, 0x0, 0x000000cc, 1},
	{OSD_PATH_MISC_CTRL, 0x0, 0xffff0000, 1},
	{VIU_OSD1_DIMM_CTRL, 0x0, 0x7fffffff, 1},
	{VIU_OSD2_DIMM_CTRL, 0x0, 0x7fffffff, 1},
	{VIU_OSD2_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	{VIU_OSD2_BLK1_CFG_W4, 0x0, 0xffffffff, 1},
	{VIU_OSD2_BLK2_CFG_W4, 0x0, 0xffffffff, 1},
	{VIU_OSD2_MALI_UNPACK_CTRL, 0x0, 0x9f01ffff, 1},
	{AMDV_CORE2A_SWAP_CTRL1, 0x0, 0x0fffffff, 1},
	{AMDV_CORE2A_SWAP_CTRL2, 0x0, 0xffffffff, 1},
	{OSD1_HDR_IN_SIZE, 0x0, 0x0fff0fff, 1},
	{OSD2_HDR_IN_SIZE, 0x0, 0x0fff0fff, 1},
	{OSD3_HDR_IN_SIZE, 0x0, 0x0fff0fff, 1},
	{OSD4_HDR_IN_SIZE, 0x0, 0x0fff0fff, 1},
	{MALI_AFBCD_TOP_CTRL, 0x0, 0x00ffbfff, 1},
	{MALI_AFBCD1_TOP_CTRL, 0x0, 0x00f7ffff, 1},
	{MALI_AFBCD2_TOP_CTRL, 0x0, 0x00ffffff, 1},
	{PATH_START_SEL, 0x0, 0xffff0000, 1},
	{VIU_OSD1_PATH_CTRL, 0x0, 0xb0030017, 1},
	{VIU_OSD2_PATH_CTRL, 0x0, 0xb0030017, 1},
	{VIU_OSD3_PATH_CTRL, 0x0, 0xb0030017, 1},
	{VPU_MAFBC1_IRQ_MASK, 0x0, 0x0000003f, 1},
	{VPU_MAFBC1_SURFACE_CFG, 0x0, 0x0001000f, 1},
	{VPU_MAFBC2_IRQ_MASK, 0x0, 0x0000003f, 1},
	{VPU_MAFBC2_SURFACE_CFG, 0x0, 0x0001000f, 1},
};

static struct reg_item misc_recovery_table_s5[] = {
	{S5_VIU_OSD1_DIMM_CTRL, 0x0, 0x7fffffff, 1},
	{S5_VIU_OSD2_DIMM_CTRL, 0x0, 0x7fffffff, 1},
	{S5_VIU_OSD2_BLK0_CFG_W4, 0x0, 0x0fff0fff, 1},
	{S5_VIU_OSD2_BLK1_CFG_W4, 0x0, 0xffffffff, 1},
	{S5_VIU_OSD2_BLK2_CFG_W4, 0x0, 0xffffffff, 1},
	{S5_VIU_OSD2_MALI_UNPACK_CTRL, 0x0, 0x9f01ffff, 1},
	{VPP_INTF_OSD1_CTRL, 0x0, 0x1, 1},
	{VPP_INTF_OSD2_CTRL, 0x0, 0x1, 1},
	{VPP_INTF_OSD3_CTRL, 0x0, 0x1, 1},
	{VPU_MAFBC1_IRQ_MASK, 0x0, 0x0000003f, 1},
	{VPU_MAFBC1_SURFACE_CFG, 0x0, 0x0001000f, 1},
	{VPU_MAFBC2_IRQ_MASK, 0x0, 0x0000003f, 1},
	{VPU_MAFBC2_SURFACE_CFG, 0x0, 0x0001000f, 1},
	{OSD1_PROC_IN_SIZE, 0x0, 0x1fff1fff, 1},
	{OSD2_PROC_IN_SIZE, 0x0, 0x1fff1fff, 1},
	{OSD3_PROC_IN_SIZE, 0x0, 0x1fff1fff, 1},
	{OSD1_PROC_OUT_SIZE, 0x0, 0x1fff1fff, 1},
	{OSD2_PROC_OUT_SIZE, 0x0, 0x1fff1fff, 1},
	{OSD3_PROC_OUT_SIZE, 0x0, 0x1fff1fff, 1},
	{OSD_BLEND_DOUT0_SIZE, 0x0, 0x1fff1fff, 1},
	{OSD_BLEND_DOUT1_SIZE, 0x0, 0x1fff1fff, 1},
	{OSD_2SLICE2PPC_IN_SIZE, 0x0, 0x1fff1fff, 1},
	{OSD_2SLICE2PPC_MODE, 0x0, 0x1, 1},
	{OSD_SYS_HWIN0_CUT, 0x0, 0x3fff1fff, 1},
	{OSD_SYS_HWIN1_CUT, 0x0, 0x3fff1fff, 1},
	{OSD_SYS_PAD_CTRL, 0x0, 0x1, 1},
	{OSD_SYS_PAD_DUMMY_DATA0, 0x0, 0x0fff0fff, 1},
	{OSD_SYS_PAD_DUMMY_DATA1, 0x0, 0x0fff0fff, 1},
	{OSD_SYS_PAD_H_SIZE, 0x0, 0x1fff1fff, 1},
	{OSD_SYS_PAD_V_SIZE, 0x0, 0x1fff1fff, 1},
	{OSD_SYS_2SLICE_HWIN_CUT, 0x0, 0x3fff1fff, 1},
};

static void recovery_regs_init_old(void)
{
	int i = 0;
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	int j;
	int cpu_id = osd_hw.osd_meson_dev.cpu_id;
#endif
	gRecovery[i].base_addr = VIU_OSD1_CTRL_STAT;
	gRecovery[i].size = sizeof(osd1_recovery_table)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd1_recovery_table[0];

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (cpu_id == __MESON_CPU_MAJOR_ID_TXLX ||
	    cpu_id == __MESON_CPU_MAJOR_ID_TXL ||
	    cpu_id == __MESON_CPU_MAJOR_ID_TXHD) {
		for (j = 0; j < gRecovery[i].size; j++) {
			if (gRecovery[i].table[j].addr ==
				VIU_OSD1_FIFO_CTRL_STAT) {
				gRecovery[i].table[j].mask = 0xffc7ffff;
				break;
			}
		}
	}
#endif
	i++;
	gRecovery[i].base_addr = OSD1_AFBCD_ENABLE;
	gRecovery[i].size = sizeof(osd_afbcd_recovery_table)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd_afbcd_recovery_table[0];

	i++;
	gRecovery[i].base_addr = VPP_OSD_VSC_PHASE_STEP;
	gRecovery[i].size = sizeof(freescale_recovery_table)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&freescale_recovery_table[0];

	i++;
	gRecovery[i].base_addr = VIU_OSD2_CTRL_STAT;
	gRecovery[i].size = sizeof(osd2_recovery_table)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd2_recovery_table[0];

	i++;
	gRecovery[i].base_addr = 0xffffffff; /* not base addr */
	gRecovery[i].size = sizeof(misc_recovery_table)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&misc_recovery_table[0];
}

static void recovery_regs_init_g12a(void)
{
	int i = 0;

	gRecovery[i].base_addr = VIU_OSD1_CTRL_STAT;
	gRecovery[i].size = sizeof(osd12_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd12_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VIU_OSD3_CTRL_STAT;
	gRecovery[i].size = sizeof(osd3_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd3_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPP_OSD_VSC_PHASE_STEP;
	gRecovery[i].size = sizeof(osd1_sc_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd1_sc_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = OSD2_VSC_PHASE_STEP;
	gRecovery[i].size = sizeof(osd2_sc_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd2_sc_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = OSD34_SCALE_COEF_IDX;
	gRecovery[i].size = sizeof(osd3_sc_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd3_sc_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC_BLOCK_ID;
	gRecovery[i].size = sizeof(vpu_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&vpu_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0;
	gRecovery[i].size = sizeof(osd1_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd1_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1;
	gRecovery[i].size = sizeof(osd2_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd2_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC_HEADER_BUF_ADDR_LOW_S2;
	gRecovery[i].size = sizeof(osd3_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd3_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VIU_OSD_BLEND_CTRL;
	gRecovery[i].size = sizeof(blend_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&blend_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPP_VD2_HDR_IN_SIZE;
	gRecovery[i].size = sizeof(post_blend_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&post_blend_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = 0xffffffff; /* not base addr */
	gRecovery[i].size = sizeof(misc_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&misc_recovery_table_g12a[0];
}

static void recovery_regs_init_t7(void)
{
	int i = 0;

	gRecovery[i].base_addr = VIU_OSD1_CTRL_STAT;
	gRecovery[i].size = sizeof(osd12_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd12_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VIU_OSD3_CTRL_STAT;
	gRecovery[i].size = sizeof(osd3_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd3_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VIU_OSD4_CTRL_STAT;
	gRecovery[i].size = sizeof(osd4_recovery_table_t7)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd4_recovery_table_t7[0];

	i++;
	gRecovery[i].base_addr = T7_VPP_OSD_VSC_PHASE_STEP;
	gRecovery[i].size = sizeof(osd1_sc_recovery_table_t7)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd1_sc_recovery_table_t7[0];

	i++;
	gRecovery[i].base_addr = T7_OSD2_VSC_PHASE_STEP;
	gRecovery[i].size = sizeof(osd2_sc_recovery_table_t7)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd2_sc_recovery_table_t7[0];

	i++;
	gRecovery[i].base_addr = T7_OSD34_VSC_PHASE_STEP;
	gRecovery[i].size = sizeof(osd3_sc_recovery_table_t7)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd3_sc_recovery_table_t7[0];

	i++;
	gRecovery[i].base_addr = T7_OSD4_VSC_PHASE_STEP;
	gRecovery[i].size = sizeof(osd4_sc_recovery_table_t7)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd4_sc_recovery_table_t7[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC_BLOCK_ID;
	gRecovery[i].size = sizeof(vpu_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&vpu_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0;
	gRecovery[i].size = sizeof(osd1_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd1_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1;
	gRecovery[i].size = sizeof(osd2_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd2_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC1_HEADER_BUF_ADDR_LOW_S2;
	gRecovery[i].size = sizeof(osd3_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd3_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC2_HEADER_BUF_ADDR_LOW_S3;
	gRecovery[i].size = sizeof(osd4_afbcd_recovery_table_t7)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd4_afbcd_recovery_table_t7[0];

	i++;
	gRecovery[i].base_addr = VIU_OSD_BLEND_CTRL;
	gRecovery[i].size = sizeof(blend_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&blend_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPP_VD2_HDR_IN_SIZE;
	gRecovery[i].size = sizeof(post_blend_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&post_blend_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = 0xffffffff; /* not base addr */
	gRecovery[i].size = sizeof(misc_recovery_table_t7)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&misc_recovery_table_t7[0];
}

static void recovery_regs_init_s5(void)
{
	int i = 0;

	gRecovery[i].base_addr = S5_VIU_OSD1_CTRL_STAT;
	gRecovery[i].size = sizeof(osd1_recovery_table_s5)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd1_recovery_table_s5[0];

	i++;
	gRecovery[i].base_addr = S5_VIU_OSD2_CTRL_STAT;
	gRecovery[i].size = sizeof(osd2_recovery_table_s5)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd2_recovery_table_s5[0];

	i++;
	gRecovery[i].base_addr = S5_VIU_OSD3_CTRL_STAT;
	gRecovery[i].size = sizeof(osd3_recovery_table_s5)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd3_recovery_table_s5[0];

	i++;
	gRecovery[i].base_addr = OSD1_PROC_VSC_PHASE_STEP;
	gRecovery[i].size = sizeof(osd1_sc_recovery_table_s5)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd1_sc_recovery_table_s5[0];

	i++;
	gRecovery[i].base_addr = OSD2_PROC_VSC_PHASE_STEP;
	gRecovery[i].size = sizeof(osd2_sc_recovery_table_s5)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd2_sc_recovery_table_s5[0];

	i++;
	gRecovery[i].base_addr = OSD3_PROC_VSC_PHASE_STEP;
	gRecovery[i].size = sizeof(osd3_sc_recovery_table_s5)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd3_sc_recovery_table_s5[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC_BLOCK_ID;
	gRecovery[i].size = sizeof(vpu_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&vpu_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0;
	gRecovery[i].size = sizeof(osd1_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd1_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1;
	gRecovery[i].size = sizeof(osd2_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd2_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC1_HEADER_BUF_ADDR_LOW_S2;
	gRecovery[i].size = sizeof(osd3_afbcd_recovery_table_g12a)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd3_afbcd_recovery_table_g12a[0];

	i++;
	gRecovery[i].base_addr = VPU_MAFBC2_HEADER_BUF_ADDR_LOW_S3;
	gRecovery[i].size = sizeof(osd4_afbcd_recovery_table_t7)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&osd4_afbcd_recovery_table_t7[0];

	i++;
	gRecovery[i].base_addr = S5_VIU_OSD_BLEND_CTRL;
	gRecovery[i].size = sizeof(blend_recovery_table_s5)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&blend_recovery_table_s5[0];

	i++;
	gRecovery[i].base_addr = S5_VPP_OSD1_BLD_H_SCOPE;
	gRecovery[i].size = sizeof(post_blend_recovery_table_s5)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&post_blend_recovery_table_s5[0];

	i++;
	gRecovery[i].base_addr = 0xffffffff; /* not base addr */
	gRecovery[i].size = sizeof(misc_recovery_table_s5)
		/ sizeof(struct reg_item);
	gRecovery[i].table =
		(struct reg_item *)&misc_recovery_table_s5[0];
}

void recovery_regs_init(void)
{
	int cpu_id = osd_hw.osd_meson_dev.cpu_id;

	if (recovery_enable)
		return;
	memset(gRecovery, 0, sizeof(gRecovery));

	if (cpu_id >= __MESON_CPU_MAJOR_ID_S5)
		recovery_regs_init_s5();
	else if (cpu_id >= __MESON_CPU_MAJOR_ID_T7)
		recovery_regs_init_t7();
	else if (cpu_id >= __MESON_CPU_MAJOR_ID_G12A)
		recovery_regs_init_g12a();
	else
		recovery_regs_init_old();
	recovery_enable = 1;
}

static int update_recovery_item_old(u32 addr, u32 value)
{
	u32 base, size;
	int i;
	struct reg_item *table = NULL;
	int ret = -1;
	int cpu_id = osd_hw.osd_meson_dev.cpu_id;

	if (!recovery_enable)
		return ret;

	if ((addr == AMDV_CORE2A_SWAP_CTRL1 ||
	     addr == AMDV_CORE2A_SWAP_CTRL2) &&
	     cpu_id != __MESON_CPU_MAJOR_ID_TXLX &&
	     cpu_id != __MESON_CPU_MAJOR_ID_GXM)
		return ret;

	base = addr & 0xfff0;
	switch (base) {
	case VIU_OSD1_CTRL_STAT:
	case VIU_OSD1_BLK1_CFG_W1:
		/* osd1 */
		if (backup_enable &
			HW_RESET_OSD1_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[0].base_addr;
		size = gRecovery[0].size;
		table = gRecovery[0].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VIU_OSD2_CTRL_STAT:
	case VIU_OSD2_BLK1_CFG_W1:
		/* osd2 */
		base = gRecovery[3].base_addr;
		size = gRecovery[3].size;
		table = gRecovery[3].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case OSD1_AFBCD_ENABLE:
		/* osd1 afbcd */
		if (backup_enable &
			HW_RESET_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[1].base_addr;
		size = gRecovery[1].size;
		table = gRecovery[1].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPP_OSD_VSC_PHASE_STEP:
		base = gRecovery[2].base_addr;
		size = gRecovery[2].size;
		table = gRecovery[2].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	default:
		/* recovery misc */
		table = gRecovery[4].table;
		for (i = 0; i <  gRecovery[4].size; i++) {
			if (addr == table[i].addr) {
				table[i].val = value;
				if (table[i].recovery)
					table[i].recovery = 1;
				ret = 0;
				break;
			}
		}
		break;
	}

	return ret;
}

static s32 get_recovery_item_old(u32 addr, u32 *value, u32 *mask)
{
	u32 base, size;
	int i;
	struct reg_item *table = NULL;
	int ret = -1;
	int cpu_id = osd_hw.osd_meson_dev.cpu_id;

	if (!recovery_enable)
		return ret;

	if ((addr == AMDV_CORE2A_SWAP_CTRL1 ||
	     addr == AMDV_CORE2A_SWAP_CTRL2) &&
	     cpu_id != __MESON_CPU_MAJOR_ID_TXLX &&
	     cpu_id != __MESON_CPU_MAJOR_ID_GXM)
		return ret;

	base = addr & 0xfff0;
	switch (base) {
	case VIU_OSD1_CTRL_STAT:
	case VIU_OSD1_BLK1_CFG_W1:
		/* osd1 */
		if (backup_enable &
			HW_RESET_OSD1_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[0].base_addr;
		size = gRecovery[0].size;
		table = gRecovery[0].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VIU_OSD2_CTRL_STAT:
	case VIU_OSD2_BLK1_CFG_W1:
		/* osd2 */
		base = gRecovery[3].base_addr;
		size = gRecovery[3].size;
		table = gRecovery[3].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case OSD1_AFBCD_ENABLE:
		/* osd1 afbcd */
		if (backup_enable &
			HW_RESET_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[1].base_addr;
		size = gRecovery[1].size;
		table = gRecovery[1].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPP_OSD_VSC_PHASE_STEP:
		base = gRecovery[2].base_addr;
		size = gRecovery[2].size;
		table = gRecovery[2].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	default:
		/* recovery misc */
		table = gRecovery[4].table;
		for (i = 0; i <  gRecovery[4].size; i++) {
			if (addr == table[i].addr) {
				table += i;
				ret = 0;
				break;
			}
		}
		break;
	}

	if (ret == 0 && table) {
		if (table->recovery == 1) {
			u32 regmask = table->mask;
			u32 real_value = osd_reg_read(addr);

			if ((real_value & regmask)
				== (table->val & regmask)) {
				ret = 1;
				*mask = regmask;
				*value = real_value;
			} else {
				*mask = regmask;
				*value = real_value & ~(regmask);
				*value |= (table->val & regmask);
			}
			table->recovery = 2;
		} else if (table->recovery == 2) {
			ret = 1;
		} else {
			ret = -1;
		}
	}
	/* ret = 1, 2 need not recovery,
	 *	ret = 0 need recovery,
	 *	ret = -1, not find
	 */
	return ret;
}

static int update_recovery_item_g12a(u32 addr, u32 value)
{
	u32 base, size;
	int i;
	struct reg_item *table = NULL;
	int ret = -1;

	if (!recovery_enable)
		return ret;

	base = addr & 0xfff0;
	switch (base) {
	case VIU_OSD1_CTRL_STAT:
	case VIU_OSD1_BLK1_CFG_W1:
		/* osd1 */
		if (backup_enable &
			HW_RESET_OSD1_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[0].base_addr;
		size = gRecovery[0].size;
		table = gRecovery[0].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VIU_OSD2_CTRL_STAT:
	case VIU_OSD2_BLK1_CFG_W1:
		/* osd2 */
		if (backup_enable &
			HW_RESET_OSD2_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[0].base_addr;
		size = gRecovery[0].size;
		table = gRecovery[0].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VIU_OSD3_CTRL_STAT:
	case VIU_OSD3_BLK0_CFG_W2:
		/* osd3 */
		if (backup_enable &
			HW_RESET_OSD3_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[1].base_addr;
		size = gRecovery[1].size;
		table = gRecovery[1].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPP_OSD_VSC_PHASE_STEP:
		/* osd1 sc */
		base = gRecovery[2].base_addr;
		size = gRecovery[2].size;
		table = gRecovery[2].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case OSD2_VSC_PHASE_STEP:
		/* osd2 osd 3 sc */
		base = gRecovery[3].base_addr;
		size = gRecovery[3].size;
		table = gRecovery[3].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case OSD34_VSC_PHASE_STEP:
		/* osd2 osd 3 sc */
		base = gRecovery[4].base_addr;
		size = gRecovery[4].size;
		table = gRecovery[4].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC_BLOCK_ID:
		/* vpu mali common */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[5].base_addr;
		size = gRecovery[5].size;
		table = gRecovery[5].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0:
		/* vpu mali src0 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[6].base_addr;
		size = gRecovery[6].size;
		table = gRecovery[6].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1:
		/* vpu mali src1 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[7].base_addr;
		size = gRecovery[7].size;
		table = gRecovery[7].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S2:
		/* vpu mali src2 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[8].base_addr;
		size = gRecovery[8].size;
		table = gRecovery[8].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VIU_OSD_BLEND_CTRL:
	case VIU_OSD_BLEND_CTRL1:
		/* osd blend ctrl */
		base = gRecovery[9].base_addr;
		size = gRecovery[9].size;
		table = gRecovery[9].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPP_VD2_HDR_IN_SIZE:
		/* vpp blend ctrl */
		base = gRecovery[10].base_addr;
		size = gRecovery[10].size;
		table = gRecovery[10].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	default:
		/* recovery misc */
		table = gRecovery[11].table;
		for (i = 0; i <  gRecovery[11].size; i++) {
			if (addr == table[i].addr) {
				table[i].val = value;
				if (table[i].recovery)
					table[i].recovery = 1;
				ret = 0;
				break;
			}
		}
		break;
	}

	return ret;
}

static s32 get_recovery_item_g12a(u32 addr, u32 *value, u32 *mask)
{
	int cpu_id = osd_hw.osd_meson_dev.cpu_id;
	u32 base, size;
	int i;
	struct reg_item *table = NULL;
	int ret = -1;

	if (!recovery_enable)
		return ret;
	if (addr == AMDV_CORE2A_SWAP_CTRL1 ||
	     addr == AMDV_CORE2A_SWAP_CTRL2)
		return ret;

	base = addr & 0xfff0;
	switch (base) {
	case VIU_OSD1_CTRL_STAT:
	case VIU_OSD1_BLK1_CFG_W1:
		/* osd1 */
		if (backup_enable &
			HW_RESET_OSD1_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[0].base_addr;
		size = gRecovery[0].size;
		table = gRecovery[0].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VIU_OSD2_CTRL_STAT:
	case VIU_OSD2_BLK1_CFG_W1:
		/* osd2 */
		if (backup_enable &
			HW_RESET_OSD2_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[0].base_addr;
		size = gRecovery[0].size;
		table = gRecovery[0].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VIU_OSD3_CTRL_STAT:
	case VIU_OSD3_BLK0_CFG_W2:
		/* osd3 */
		if (backup_enable &
			HW_RESET_OSD3_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[1].base_addr;
		size = gRecovery[1].size;
		table = gRecovery[1].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPP_OSD_VSC_PHASE_STEP:
		/* osd1 sc */
		base = gRecovery[2].base_addr;
		size = gRecovery[2].size;
		table = gRecovery[2].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case OSD2_VSC_PHASE_STEP:
		/* osd2 */
		base = gRecovery[3].base_addr;
		size = gRecovery[3].size;
		table = gRecovery[3].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case OSD34_VSC_PHASE_STEP:
		/* osd3 sc */
		base = gRecovery[4].base_addr;
		size = gRecovery[4].size;
		table = gRecovery[4].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC_BLOCK_ID:
		/* vpu mali common */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[5].base_addr;
		size = gRecovery[5].size;
		table = gRecovery[5].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0:
		/* vpu mali src0 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[6].base_addr;
		size = gRecovery[6].size;
		table = gRecovery[6].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1:
		/* vpu mali src1 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[7].base_addr;
		size = gRecovery[7].size;
		table = gRecovery[7].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S2:
		/* vpu mali src2 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[8].base_addr;
		size = gRecovery[8].size;
		table = gRecovery[8].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VIU_OSD_BLEND_CTRL:
	case VIU_OSD_BLEND_CTRL1:
		/* osd blend ctrl */
		base = gRecovery[9].base_addr;
		size = gRecovery[9].size;
		table = gRecovery[9].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPP_VD2_HDR_IN_SIZE:
		/* vpp blend ctrl */
		base = gRecovery[10].base_addr;
		size = gRecovery[10].size;
		table = gRecovery[10].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	default:
		/* recovery misc */
		table = gRecovery[11].table;
		for (i = 0; i <  gRecovery[11].size; i++) {
			if (addr == table[i].addr) {
				table += i;
				ret = 0;
				break;
			}
		}
		break;
	}

	if (ret == 0 && table) {
		if (table->recovery == 1) {
			u32 regmask = table->mask;
			u32 real_value = osd_reg_read(addr);
			u32 temp1 = 0, temp2 = 0;

			if (cpu_id <= __MESON_CPU_MAJOR_ID_SM1 &&
			    addr == VIU_OSD_BLEND_CTRL1) {
				/* hw bug, >=bit6 need << 2*/
				temp1 = real_value & 0x3f;
				temp2 = (real_value & (~0x3f)) << 2;
				temp2 |= temp1;
				real_value = temp2;
			}
			if (enable_vd_zorder &&
			    addr == OSD2_BLEND_SRC_CTRL) {
				ret = 1;
				table->recovery = 0;
			} else if ((real_value & regmask)
				== (table->val & regmask)) {
				ret = 1;
				*mask = regmask;
				*value = real_value;
			} else {
				*mask = regmask;
				*value = real_value & ~(regmask);
				*value |= (table->val & regmask);
			}
			table->recovery = 2;
		} else if (table->recovery == 2) {
			ret = 1;
		} else {
			ret = -1;
		}
	}
	/* ret = 1, 2 need not recovery,
	 *	ret = 0 need recovery,
	 *	ret = -1, not find
	 */
	return ret;
}

static int update_recovery_item_t7(u32 addr, u32 value)
{
	u32 base, size;
	int i;
	struct reg_item *table = NULL;
	int ret = -1;

	if (!recovery_enable)
		return ret;
	if ((addr == AMDV_CORE2A_SWAP_CTRL1 ||
	     addr == AMDV_CORE2A_SWAP_CTRL2))
		return ret;

	base = addr & 0xfff0;
	switch (base) {
	case VIU_OSD1_CTRL_STAT:
	case VIU_OSD1_BLK1_CFG_W1:
		/* osd1 */
		if (backup_enable &
			HW_RESET_OSD1_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[0].base_addr;
		size = gRecovery[0].size;
		table = gRecovery[0].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VIU_OSD2_CTRL_STAT:
	case VIU_OSD2_BLK1_CFG_W1:
		/* osd2 */
		if (backup_enable &
			HW_RESET_OSD2_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[0].base_addr;
		size = gRecovery[0].size;
		table = gRecovery[0].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VIU_OSD3_CTRL_STAT:
	case VIU_OSD3_BLK0_CFG_W2:
	case VIU_OSD3_DIMM_CTRL:
		/* osd3 */
		if (backup_enable &
			HW_RESET_OSD3_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[1].base_addr;
		size = gRecovery[1].size;
		table = gRecovery[1].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VIU_OSD4_CTRL_STAT:
	case VIU_OSD4_BLK0_CFG_W2:
	case VIU_OSD4_DIMM_CTRL:
		/* osd3 */
		if (backup_enable &
			HW_RESET_OSD4_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[2].base_addr;
		size = gRecovery[2].size;
		table = gRecovery[2].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;

	case T7_VPP_OSD_VSC_PHASE_STEP:
		/* osd1 sc */
		base = gRecovery[3].base_addr;
		size = gRecovery[3].size;
		table = gRecovery[3].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case T7_OSD2_VSC_PHASE_STEP:
		/* osd2 osd 3 sc */
		base = gRecovery[4].base_addr;
		size = gRecovery[4].size;
		table = gRecovery[4].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case T7_OSD34_VSC_PHASE_STEP:
		/* osd2 osd 3 sc */
		base = gRecovery[5].base_addr;
		size = gRecovery[5].size;
		table = gRecovery[5].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case T7_OSD4_VSC_PHASE_STEP:
		/* osd2 osd 3 sc */
		base = gRecovery[6].base_addr;
		size = gRecovery[6].size;
		table = gRecovery[6].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;

	case VPU_MAFBC_BLOCK_ID:
		/* vpu mali common */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[7].base_addr;
		size = gRecovery[7].size;
		table = gRecovery[7].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0:
		/* vpu mali src0 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[8].base_addr;
		size = gRecovery[8].size;
		table = gRecovery[8].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1:
		/* vpu mali src1 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[9].base_addr;
		size = gRecovery[9].size;
		table = gRecovery[9].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC1_HEADER_BUF_ADDR_LOW_S2:
		/* vpu mali src2 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[10].base_addr;
		size = gRecovery[10].size;
		table = gRecovery[10].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC2_HEADER_BUF_ADDR_LOW_S3:
		/* vpu mali src2 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[11].base_addr;
		size = gRecovery[11].size;
		table = gRecovery[11].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VIU_OSD_BLEND_CTRL:
	case VIU_OSD_BLEND_CTRL1:
		/* osd blend ctrl */
		base = gRecovery[12].base_addr;
		size = gRecovery[12].size;
		table = gRecovery[12].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPP_VD2_HDR_IN_SIZE:
		/* vpp blend ctrl */
		base = gRecovery[13].base_addr;
		size = gRecovery[13].size;
		table = gRecovery[13].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	default:
		/* recovery misc */
		table = gRecovery[14].table;
		for (i = 0; i <  gRecovery[14].size; i++) {
			if (addr == table[i].addr) {
				table[i].val = value;
				if (table[i].recovery)
					table[i].recovery = 1;
				ret = 0;
				break;
			}
		}
		break;
	}

	return ret;
}

static int update_recovery_item_s5(u32 addr, u32 value)
{
	u32 base, size;
	int i;
	struct reg_item *table = NULL;
	int ret = -1;

	if (!recovery_enable)
		return ret;

	base = addr & 0xfff0;
	switch (base) {
	case S5_VIU_OSD1_CTRL_STAT:
	case S5_VIU_OSD1_BLK0_CFG_W2:
	case S5_VIU_OSD1_DIMM_CTRL:
		/* osd1 */
		if (backup_enable &
			HW_RESET_OSD1_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[0].base_addr;
		size = gRecovery[0].size;
		table = gRecovery[0].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case S5_VIU_OSD2_CTRL_STAT:
	case S5_VIU_OSD2_BLK0_CFG_W2:
	case S5_VIU_OSD2_DIMM_CTRL:
		/* osd2 */
		if (backup_enable &
			HW_RESET_OSD2_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[1].base_addr;
		size = gRecovery[1].size;
		table = gRecovery[1].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case S5_VIU_OSD3_CTRL_STAT:
	case S5_VIU_OSD3_BLK0_CFG_W2:
	case S5_VIU_OSD3_DIMM_CTRL:
		/* osd3 */
		if (backup_enable &
			HW_RESET_OSD3_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[2].base_addr;
		size = gRecovery[2].size;
		table = gRecovery[2].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case OSD1_PROC_VSC_PHASE_STEP:
		/* osd1 sc */
		base = gRecovery[3].base_addr;
		size = gRecovery[3].size;
		table = gRecovery[3].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case OSD2_PROC_VSC_PHASE_STEP:
		/* osd2 sc */
		base = gRecovery[4].base_addr;
		size = gRecovery[4].size;
		table = gRecovery[4].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case OSD3_PROC_VSC_PHASE_STEP:
		/* osd3 sc */
		base = gRecovery[5].base_addr;
		size = gRecovery[5].size;
		table = gRecovery[5].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;

	case VPU_MAFBC_BLOCK_ID:
		/* vpu mali common */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[6].base_addr;
		size = gRecovery[6].size;
		table = gRecovery[6].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0:
		/* vpu mali src0 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[7].base_addr;
		size = gRecovery[7].size;
		table = gRecovery[7].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1:
		/* vpu mali src1 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD1_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[8].base_addr;
		size = gRecovery[8].size;
		table = gRecovery[8].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC1_HEADER_BUF_ADDR_LOW_S2:
		/* vpu mali src2 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD2_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[9].base_addr;
		size = gRecovery[9].size;
		table = gRecovery[9].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VPU_MAFBC2_HEADER_BUF_ADDR_LOW_S3:
		/* vpu mali src3 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 1;
			break;
		}
		base = gRecovery[10].base_addr;
		size = gRecovery[10].size;
		table = gRecovery[10].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case VIU_OSD_BLEND_CTRL:
	case VIU_OSD_BLEND_CTRL1:
		/* osd blend ctrl */
		base = gRecovery[11].base_addr;
		size = gRecovery[11].size;
		table = gRecovery[11].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	case S5_VPP_OSD1_BLD_H_SCOPE:
	case S5_OSD1_BLEND_SRC_CTRL:
		/* vpp blend ctrl */
		base = gRecovery[12].base_addr;
		size = gRecovery[12].size;
		table = gRecovery[12].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table[addr - base].val = value;
			if (table[addr - base].recovery)
				table[addr - base].recovery = 1;
			ret = 0;
		}
		break;
	default:
		/* recovery misc */
		table = gRecovery[13].table;
		for (i = 0; i <  gRecovery[13].size; i++) {
			if (addr == table[i].addr) {
				table[i].val = value;
				if (table[i].recovery)
					table[i].recovery = 1;
				ret = 0;
				break;
			}
		}
		break;
	}

	return ret;
}

static s32 get_recovery_item_t7(u32 addr, u32 *value, u32 *mask)
{
	u32 base, size;
	int i;
	struct reg_item *table = NULL;
	int ret = -1;

	if (!recovery_enable)
		return ret;
	if (addr == AMDV_CORE2A_SWAP_CTRL1 ||
	     addr == AMDV_CORE2A_SWAP_CTRL2)
		return ret;

	base = addr & 0xfff0;
	switch (base) {
	case VIU_OSD1_CTRL_STAT:
	case VIU_OSD1_BLK1_CFG_W1:
		/* osd1 */
		if (backup_enable &
			HW_RESET_OSD1_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[0].base_addr;
		size = gRecovery[0].size;
		table = gRecovery[0].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VIU_OSD2_CTRL_STAT:
	case VIU_OSD2_BLK1_CFG_W1:
		/* osd2 */
		if (backup_enable &
			HW_RESET_OSD2_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[0].base_addr;
		size = gRecovery[0].size;
		table = gRecovery[0].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VIU_OSD3_CTRL_STAT:
	case VIU_OSD3_BLK0_CFG_W2:
	case VIU_OSD3_DIMM_CTRL:
		/* osd3 */
		if (backup_enable &
			HW_RESET_OSD3_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[1].base_addr;
		size = gRecovery[1].size;
		table = gRecovery[1].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VIU_OSD4_CTRL_STAT:
	case VIU_OSD4_BLK0_CFG_W2:
	case VIU_OSD4_DIMM_CTRL:
		/* osd4 */
		if (backup_enable &
			HW_RESET_OSD4_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[2].base_addr;
		size = gRecovery[2].size;
		table = gRecovery[2].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;

	case T7_VPP_OSD_VSC_PHASE_STEP:
		/* osd1 sc */
		base = gRecovery[3].base_addr;
		size = gRecovery[3].size;
		table = gRecovery[3].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case T7_OSD2_VSC_PHASE_STEP:
		/* osd2 */
		base = gRecovery[4].base_addr;
		size = gRecovery[4].size;
		table = gRecovery[4].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case T7_OSD34_VSC_PHASE_STEP:
		/* osd3 sc */
		base = gRecovery[5].base_addr;
		size = gRecovery[5].size;
		table = gRecovery[5].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case T7_OSD4_VSC_PHASE_STEP:
		/* osd4 sc */
		base = gRecovery[6].base_addr;
		size = gRecovery[6].size;
		table = gRecovery[6].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC_BLOCK_ID:
		/* vpu mali common */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[7].base_addr;
		size = gRecovery[7].size;
		table = gRecovery[7].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0:
		/* vpu mali src0 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[8].base_addr;
		size = gRecovery[8].size;
		table = gRecovery[8].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1:
		/* vpu mali src1 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[9].base_addr;
		size = gRecovery[9].size;
		table = gRecovery[9].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC1_HEADER_BUF_ADDR_LOW_S2:
		/* vpu mali src2 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[10].base_addr;
		size = gRecovery[10].size;
		table = gRecovery[10].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC2_HEADER_BUF_ADDR_LOW_S3:
		/* vpu mali src3 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[11].base_addr;
		size = gRecovery[11].size;
		table = gRecovery[11].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VIU_OSD_BLEND_CTRL:
	case VIU_OSD_BLEND_CTRL1:
		/* osd blend ctrl */
		base = gRecovery[12].base_addr;
		size = gRecovery[12].size;
		table = gRecovery[12].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPP_VD2_HDR_IN_SIZE:
		/* vpp blend ctrl */
		base = gRecovery[13].base_addr;
		size = gRecovery[13].size;
		table = gRecovery[13].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	default:
		/* recovery misc */
		table = gRecovery[14].table;
		for (i = 0; i <  gRecovery[14].size; i++) {
			if (addr == table[i].addr) {
				table += i;
				ret = 0;
				break;
			}
		}
		break;
	}

	if (ret == 0 && table) {
		if (table->recovery == 1) {
			u32 regmask = table->mask;
			u32 real_value = osd_reg_read(addr);

			if (enable_vd_zorder &&
			    addr == OSD2_BLEND_SRC_CTRL) {
				ret = 1;
				table->recovery = 0;
			} else if ((real_value & regmask)
				== (table->val & regmask)) {
				ret = 1;
				*mask = regmask;
				*value = real_value;
			} else {
				*mask = regmask;
				*value = real_value & ~(regmask);
				*value |= (table->val & regmask);
			}
			table->recovery = 2;
		} else if (table->recovery == 2) {
			ret = 1;
		} else {
			ret = -1;
		}
	}
	/* ret = 1, 2 need not recovery,
	 *	ret = 0 need recovery,
	 *	ret = -1, not find
	 */
	return ret;
}

static s32 get_recovery_item_s5(u32 addr, u32 *value, u32 *mask)
{
	u32 base, size;
	int i;
	struct reg_item *table = NULL;
	int ret = -1;

	if (!recovery_enable)
		return ret;

	base = addr & 0xfff0;
	switch (base) {
	case S5_VIU_OSD1_CTRL_STAT:
	case S5_VIU_OSD1_BLK0_CFG_W2:
	case S5_VIU_OSD1_DIMM_CTRL:
		/* osd1 */
		if (backup_enable &
			HW_RESET_OSD1_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[0].base_addr;
		size = gRecovery[0].size;
		table = gRecovery[0].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case S5_VIU_OSD2_CTRL_STAT:
	case S5_VIU_OSD2_BLK0_CFG_W2:
	case S5_VIU_OSD2_DIMM_CTRL:
		/* osd2 */
		if (backup_enable &
			HW_RESET_OSD2_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[1].base_addr;
		size = gRecovery[1].size;
		table = gRecovery[1].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case S5_VIU_OSD3_CTRL_STAT:
	case S5_VIU_OSD3_BLK0_CFG_W2:
	case S5_VIU_OSD3_DIMM_CTRL:
		/* osd3 */
		if (backup_enable &
			HW_RESET_OSD3_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[2].base_addr;
		size = gRecovery[2].size;
		table = gRecovery[2].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case OSD1_PROC_VSC_PHASE_STEP:
		/* osd1 sc */
		base = gRecovery[3].base_addr;
		size = gRecovery[3].size;
		table = gRecovery[3].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case OSD2_PROC_VSC_PHASE_STEP:
		/* osd2 sc */
		base = gRecovery[4].base_addr;
		size = gRecovery[4].size;
		table = gRecovery[4].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case OSD3_PROC_VSC_PHASE_STEP:
		/* osd3 sc */
		base = gRecovery[5].base_addr;
		size = gRecovery[5].size;
		table = gRecovery[5].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC_BLOCK_ID:
		/* vpu mali common */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[6].base_addr;
		size = gRecovery[6].size;
		table = gRecovery[6].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S0:
		/* vpu mali src0 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[7].base_addr;
		size = gRecovery[7].size;
		table = gRecovery[7].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC_HEADER_BUF_ADDR_LOW_S1:
		/* vpu mali src1 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD1_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[8].base_addr;
		size = gRecovery[8].size;
		table = gRecovery[8].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC1_HEADER_BUF_ADDR_LOW_S2:
		/* vpu mali src2 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD2_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[9].base_addr;
		size = gRecovery[9].size;
		table = gRecovery[9].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VPU_MAFBC2_HEADER_BUF_ADDR_LOW_S3:
		/* vpu mali src3 */
		if (backup_enable &
			HW_RESET_MALI_AFBCD_REGS) {
			ret = 2;
			break;
		}
		base = gRecovery[10].base_addr;
		size = gRecovery[10].size;
		table = gRecovery[10].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case VIU_OSD_BLEND_CTRL:
	case VIU_OSD_BLEND_CTRL1:
		/* osd blend ctrl */
		base = gRecovery[11].base_addr;
		size = gRecovery[11].size;
		table = gRecovery[11].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	case S5_VPP_OSD1_BLD_H_SCOPE:
	case S5_OSD1_BLEND_SRC_CTRL:
		/* vpp blend ctrl */
		base = gRecovery[12].base_addr;
		size = gRecovery[12].size;
		table = gRecovery[12].table;
		if (addr >= base &&
		    (addr < base + size)) {
			table += (addr - base);
			ret = 0;
		}
		break;
	default:
		/* recovery misc */
		table = gRecovery[13].table;
		for (i = 0; i <  gRecovery[13].size; i++) {
			if (addr == table[i].addr) {
				table += i;
				ret = 0;
				break;
			}
		}
		break;
	}

	if (ret == 0 && table) {
		if (table->recovery == 1) {
			u32 regmask = table->mask;
			u32 real_value = osd_reg_read(addr);

			if (enable_vd_zorder &&
			    addr == S5_OSD2_BLEND_SRC_CTRL) {
				ret = 1;
				table->recovery = 0;
			} else if ((real_value & regmask)
				== (table->val & regmask)) {
				ret = 1;
				*mask = regmask;
				*value = real_value;
			} else {
				*mask = regmask;
				*value = real_value & ~(regmask);
				*value |= (table->val & regmask);
			}
			table->recovery = 2;
		} else if (table->recovery == 2) {
			ret = 1;
		} else {
			ret = -1;
		}
	}
	/* ret = 1, 2 need not recovery,
	 *	ret = 0 need recovery,
	 *	ret = -1, not find
	 */
	return ret;
}

int update_recovery_item(u32 addr, u32 value)
{
	int ret = -1;
	int cpu_id = osd_hw.osd_meson_dev.cpu_id;

	if (!recovery_enable)
		return ret;

	if (cpu_id >= __MESON_CPU_MAJOR_ID_S5)
		ret = update_recovery_item_s5(addr, value);
	else if (cpu_id >= __MESON_CPU_MAJOR_ID_T7)
		ret = update_recovery_item_t7(addr, value);
	else if (cpu_id >= __MESON_CPU_MAJOR_ID_G12A)
		ret = update_recovery_item_g12a(addr, value);
	else
		ret = update_recovery_item_old(addr, value);

	return ret;
}

s32 get_recovery_item(u32 addr, u32 *value, u32 *mask)
{
	int ret = -1;
	int cpu_id = osd_hw.osd_meson_dev.cpu_id;

	if (!recovery_enable)
		return ret;

	if (cpu_id >= __MESON_CPU_MAJOR_ID_S5)
		ret = get_recovery_item_s5(addr, value, mask);
	else if (cpu_id >= __MESON_CPU_MAJOR_ID_T7)
		ret = get_recovery_item_t7(addr, value, mask);
	else if (cpu_id >= __MESON_CPU_MAJOR_ID_G12A)
		ret = get_recovery_item_g12a(addr, value, mask);
	else
		ret = get_recovery_item_old(addr, value, mask);

	return ret;
}
