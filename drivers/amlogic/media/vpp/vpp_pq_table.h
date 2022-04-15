/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPP_PQ_TABLE_H__
#define __VPP_PQ_TABLE_H__

#define PAGE_REG_COUNT_MAX (256)

struct vpp_pq_reg_s {
	unsigned char addr;      /*Register address*/
	unsigned char mask_type; /*Mask type*/
	unsigned char update;    /*Update flag*/
	unsigned int  val;       /*Register value*/
};

struct vpp_pq_page_s {
	unsigned char page_addr;  /*Page address*/
	unsigned int  count;      /*Count of total reg, max is 256 for each page*/
	struct vpp_pq_reg_s reg[PAGE_REG_COUNT_MAX];
};

struct vpp_pq_page_s pq_reg_table[] = {
	/*sr0 sharpness reg*/
	{
		0X50, 94,
		{0x00, 0, 1, 0x02d00240},
		{0x01, 0, 1, 0x00082060},
	},
	{
		0X51, 94,
		{0x00, 0, 1, 0x02d00240},
		{0x01, 0, 1, 0x00082060},
	},
};

#endif

