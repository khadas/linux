/* SPDX-License-Identifier: GPL-2.0+ */
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 *
 */
#include "../../lcd_reg.h"

#ifndef CONFIG_AMLOGIC_MEDIA_RDMA
#define LDIM_VSYNC_RDMA      0
#else
#define LDIM_VSYNC_RDMA      0
#endif

int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG(u32 adr);
int VSYNC_WR_MPEG_REG(u32 adr, u32 val);

void ldim_wr_vcbus(unsigned int addr, unsigned int val);
unsigned int ldim_rd_vcbus(unsigned int addr);
void ldim_wr_vcbus_bits(unsigned int addr, unsigned int val,
			unsigned int start, unsigned int len);
unsigned int ldim_rd_vcbus_bits(unsigned int addr,
				unsigned int start, unsigned int len);
void ldim_wr_reg_bits(unsigned int addr, unsigned int val,
				    unsigned int start, unsigned int len);
void ldim_wr_reg(unsigned int addr, unsigned int val);
unsigned int ldim_rd_reg(unsigned int addr);
