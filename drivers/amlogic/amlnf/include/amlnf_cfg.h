/*
 * drivers/amlogic/amlnf/include/amlnf_cfg.h
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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

#ifndef __AML_NF_CFG_H__
#define __AML_NF_CFG_H__
#define	AML_SLC_NAND_SUPPORT
#define	AML_MLC_NAND_SUPPORT
/*
#define	AML_NAND_DBG
*/
#define AML_CFG_2PLANE_READ_EN		(0)
#define	AML_CFG_NEW_NAND_SUPPORT	(1)
#define AML_CFG_NEWOOB_EN			(1)
#define AML_CFG_PINMUX_ONCE_FOR_ALL	(1)
#define AML_CFG_CONHERENT_BUFFER	(0)
/* store dtd in rsv area! */
#define AML_CFG_DTB_RSV_EN			(1)

#define NAND_ADJUST_PART_TABLE

#ifdef NAND_ADJUST_PART_TABLE
#define	ADJUST_BLOCK_NUM	4
#else
#define	ADJUST_BLOCK_NUM	0
#endif

#define AML_NAND_RB_IRQ
/*
#define AML_NAND_DMA_POLLING
*/

extern  int is_phydev_off_adjust(void);
extern  int get_adjust_block_num(void);
#endif

