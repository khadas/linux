/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _OSD_BACKUP_H_
#define _OSD_BACKUP_H_

#include "osd_reg.h"

#define OSD_REG_BACKUP_COUNT 12
#define OSD_AFBC_REG_BACKUP_COUNT 9
#define MALI_AFBC_REG_BACKUP_COUNT 41
#define MALI_AFBC_REG_T7_BACKUP_COUNT 28
#define MALI_AFBC1_REG_T7_BACKUP_COUNT 15
#define MALI_AFBC2_REG_T7_BACKUP_COUNT 15
#define OSD_VALUE_COUNT (VIU_OSD1_CTRL_STAT2 - VIU_OSD1_CTRL_STAT + 1)
#define OSD_AFBC_VALUE_COUNT (OSD1_AFBCD_PIXEL_VSCOPE - OSD1_AFBCD_ENABLE + 1)
#define MALI_AFBC_VALUE_COUNT \
	(VPU_MAFBC_PREFETCH_CFG_S2 - VPU_MAFBC_IRQ_MASK + 1)
#define MALI_AFBC_VALUE_T7_COUNT \
	(VPU_MAFBC_PREFETCH_CFG_S1 - VPU_MAFBC_IRQ_MASK + 1)
#define MALI_AFBC1_VALUE_T7_COUNT \
	(VPU_MAFBC1_PREFETCH_CFG_S2 - VPU_MAFBC1_IRQ_MASK + 1)
#define MALI_AFBC2_VALUE_T7_COUNT \
	(VPU_MAFBC2_PREFETCH_CFG_S3 - VPU_MAFBC2_IRQ_MASK + 1)

extern const u16 osd_reg_backup[OSD_REG_BACKUP_COUNT];
extern const u16 osd_afbc_reg_backup[OSD_AFBC_REG_BACKUP_COUNT];
extern const u16 mali_afbc_reg_backup[MALI_AFBC_REG_BACKUP_COUNT];
extern const u16 mali_afbc_reg_t7_backup[MALI_AFBC_REG_T7_BACKUP_COUNT];
extern const u16 mali_afbc1_reg_t7_backup[MALI_AFBC1_REG_T7_BACKUP_COUNT];
extern const u16 mali_afbc2_reg_t7_backup[MALI_AFBC2_REG_T7_BACKUP_COUNT];
extern u32 osd_backup[OSD_VALUE_COUNT];
extern u32 osd_afbc_backup[OSD_AFBC_VALUE_COUNT];
extern u32 mali_afbc_backup[MALI_AFBC_VALUE_COUNT];
extern u32 mali_afbc_t7_backup[MALI_AFBC_VALUE_T7_COUNT];
extern u32 mali_afbc1_t7_backup[MALI_AFBC1_VALUE_T7_COUNT];
extern u32 mali_afbc2_t7_backup[MALI_AFBC2_VALUE_T7_COUNT];


enum hw_reset_flag_e {
	HW_RESET_NONE = 0,
	HW_RESET_AFBCD_REGS = 0x80000000,
	HW_RESET_OSD1_REGS = 0x00000001,
	HW_RESET_OSD2_REGS = 0x00000002,
	HW_RESET_OSD3_REGS = 0x00000004,
	HW_RESET_OSD4_REGS = 0x00000008,
	HW_RESET_AFBCD_HARDWARE = 0x80000000,
	HW_RESET_MALI_AFBCD_REGS = 0x280000,
	HW_RESET_MALI_AFBCD1_REGS = 0x4000000,
	HW_RESET_MALI_AFBCD2_REGS = 0x10000000,
	HW_RESET_MALI_AFBCD_ARB = 0x80000,
};

struct reg_item {
	u32 addr;
	u32 val;
	u32 mask;
	int recovery;
};

struct reg_recovery_table {
	u32 base_addr;
	u32 size;
	struct reg_item *table;
};

void update_backup_reg(u32 addr, u32 value);
s32 get_backup_reg(u32 addr, u32 *value);
void backup_regs_init(u32 backup_mask);
u32 is_backup(void);

void recovery_regs_init(void);
int update_recovery_item(u32 addr, u32 value);
s32 get_recovery_item(u32 addr, u32 *value, u32 *mask);
#endif

