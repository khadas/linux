/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifndef __FRC_DBG_H__
#define __FRC_DBG_H__

extern const char * const frc_state_ary[];
extern u32 g_input_hsize;
extern u32 g_input_vsize;
extern int frc_dbg_en;

void frc_debug_if(struct frc_dev_s *frc_devp, const char *buf, size_t count);
void frc_reg_io(const char *buf);
void frc_tool_dbg_store(struct frc_dev_s *devp, const char *buf);
#endif
