/*
 * drivers/amlogic/display/osd/osd_rdma.h
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

#ifndef _OSD_RDMA_H_
#define _OSD_RDMA_H_

#include <linux/types.h>
#include "osd_io.h"
#include "osd_reg.h"

struct rdma_table_item_t {
	u32  addr;
	u32  val;
};

#define TABLE_SIZE	 PAGE_SIZE
#define MAX_TABLE_ITEM	 (TABLE_SIZE/sizeof(struct rdma_table_item_t))
#define OSD_RDMA_CHANNEL_INDEX	3  /*auto  1,2,3   manual is */
#define START_ADDR	(RDMA_AHB_START_ADDR_MAN+(OSD_RDMA_CHANNEL_INDEX<<1))
#define END_ADDR	(RDMA_AHB_END_ADDR_MAN+(OSD_RDMA_CHANNEL_INDEX<<1))

#define OSD_RDMA_FLAG_REG	VIU_OSD2_TCOLOR_AG3

#define OSD_RDMA_FLAG_DONE	(1<<0)
/*hw rdma change it to 1 when rdma complete,
	rdma isr chagne it to 0 to reset it.*/
#define OSD_RDMA_FLAG_REJECT	(1<<1)
/*hw rdma own this flag, change it to zero when s
	tart rdma,change it to 0 when complete*/
#define	OSD_RDMA_FLAG_DIRTY	(1<<2)
/*hw rdma change it to 0 , cpu write change it to 1*/

#define OSD_RDMA_FLAGS_ALL_ENABLE \
	(OSD_RDMA_FLAG_DONE|OSD_RDMA_FLAG_REJECT|OSD_RDMA_FLAG_DIRTY)

#define  OSD_RDMA_STATUS \
	(osd_reg_read(OSD_RDMA_FLAG_REG)& \
	(OSD_RDMA_FLAG_REJECT|OSD_RDMA_FLAG_DONE|OSD_RDMA_FLAG_DIRTY))
#define  OSD_RDMA_STATUS_IS_REJECT \
	(osd_reg_read(OSD_RDMA_FLAG_REG)& \
		OSD_RDMA_FLAG_REJECT)
#define  OSD_RDMA_STAUS_IS_DIRTY \
		(osd_reg_read(OSD_RDMA_FLAG_REG)&OSD_RDMA_FLAG_DIRTY)
#define  OSD_RDMA_STAUS_IS_DONE	\
	(osd_reg_read(OSD_RDMA_FLAG_REG)&OSD_RDMA_FLAG_DONE)

/*hw rdma op, set DONE && clear DIRTY && clear REJECT*/
#define  OSD_RDMA_STATUS_MARK_COMPLETE \
	((osd_reg_read(OSD_RDMA_FLAG_REG)&~OSD_RDMA_FLAGS_ALL_ENABLE)|\
		(OSD_RDMA_FLAG_DONE))
/*hw rdma op,set REJECT && set DIRTY.*/
#define  OSD_RDMA_STATUS_MARK_TBL_RST \
	((osd_reg_read(OSD_RDMA_FLAG_REG)&~OSD_RDMA_FLAGS_ALL_ENABLE)| \
	(OSD_RDMA_FLAG_REJECT|OSD_RDMA_FLAG_DIRTY))

/*cpu op*/
#define  OSD_RDMA_STAUS_MARK_DIRTY \
	(osd_reg_write(OSD_RDMA_FLAG_REG, osd_reg_read(OSD_RDMA_FLAG_REG)|\
		OSD_RDMA_FLAG_DIRTY))
/*isr op*/
#define  OSD_RDMA_STAUS_CLEAR_DONE \
	(osd_reg_write(OSD_RDMA_FLAG_REG, osd_reg_read(OSD_RDMA_FLAG_REG)& \
	~(OSD_RDMA_FLAG_DONE)))
/*cpu reset op.*/
#define  OSD_RDMA_STATUS_CLEAR_ALL \
	(osd_reg_write(OSD_RDMA_FLAG_REG, (osd_reg_read(OSD_RDMA_FLAG_REG)& \
		~OSD_RDMA_FLAGS_ALL_ENABLE)))
extern void osd_rdma_start(void);
extern int read_rdma_table(void);

#endif
