/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __SYSTEM_GDC_IO_H__
#define __SYSTEM_GDC_IO_H__

/**
 *   Read 32 bit word from gdc memory
 *
 *   This function returns a 32 bits word from GDC memory with a given offset.
 *
 *   @param addr - the offset in GDC memory to read 32 bits word.
 *
 *   @return 32 bits memory value
 */
u32 system_gdc_read_32(u32 addr, u32 core_id);

/**
 *   Write 32 bits word to gdc memory
 *
 *   This function writes a 32 bits word to GDC memory with a given offset.
 *
 *   @param addr - the offset in GDC memory to write data.
 *   @param data - data to be written
 */
void system_gdc_write_32(u32 addr, u32 data, u32 core_id);
u32 system_ext_8g_msb_read_32(void);
void system_ext_8g_msb_write_32(u32 data);
#endif /* __SYSTEM_GDC_IO_H__ */
