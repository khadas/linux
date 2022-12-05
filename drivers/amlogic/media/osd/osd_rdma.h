/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _OSD_RDMA_H_
#define _OSD_RDMA_H_

#include <linux/types.h>
#include "osd_io.h"

extern int rdma_reset_tigger_flag;
extern int rdma_mgr_irq_request;
extern u32 osd_rdma_flag_reg[];
extern u32 rdma_detect_reg;

struct rdma_table_item {
	u32 addr;
	u32 val;
};

struct rdma_warn_array {
	u32 addr;
	u32 count;
	u32 cur_line;
	u32 cur_begin_line;
};

enum {
	VPP0,
	VPP1,
	VPP2,
};

#define WARN_TABLE       64
#define TABLE_SIZE	     PAGE_SIZE
#define MAX_TABLE_ITEM	 (TABLE_SIZE / sizeof(struct rdma_table_item_t))
#define OSD_RDMA_CHANNEL_INDEX	osd_rdma_handle[0]
#define START_ADDR	\
	(RDMA_AHB_START_ADDR_MAN + (OSD_RDMA_CHANNEL_INDEX << 1))
#define START_ADDR_MSB	\
		(RDMA_AHB_START_ADDR_MAN_MSB + (OSD_RDMA_CHANNEL_INDEX << 1))
#define END_ADDR	\
	(RDMA_AHB_END_ADDR_MAN + (OSD_RDMA_CHANNEL_INDEX << 1))
#define END_ADDR_MSB	\
	(RDMA_AHB_END_ADDR_MAN_MSB + (OSD_RDMA_CHANNEL_INDEX << 1))

#define OSD_RDMA_CHANNEL_INDEX_VPP1	osd_rdma_handle[1]
#define START_ADDR_VPP1	\
	(RDMA_AHB_START_ADDR_MAN + (OSD_RDMA_CHANNEL_INDEX_VPP1 << 1))
#define START_ADDR_MSB_VPP1	\
		(RDMA_AHB_START_ADDR_MAN_MSB + (OSD_RDMA_CHANNEL_INDEX_VPP1 << 1))
#define END_ADDR_VPP1	\
	(RDMA_AHB_END_ADDR_MAN + (OSD_RDMA_CHANNEL_INDEX_VPP1 << 1))
#define END_ADDR_MSB_VPP1	\
	(RDMA_AHB_END_ADDR_MAN_MSB + (OSD_RDMA_CHANNEL_INDEX_VPP1 << 1))

#define OSD_RDMA_CHANNEL_INDEX_VPP2	osd_rdma_handle[2]
#define START_ADDR_VPP2	\
	(RDMA_AHB_START_ADDR_MAN + (OSD_RDMA_CHANNEL_INDEX_VPP2 << 1))
#define START_ADDR_MSB_VPP2	\
		(RDMA_AHB_START_ADDR_MAN_MSB + (OSD_RDMA_CHANNEL_INDEX_VPP2 << 1))
#define END_ADDR_VPP2	\
	(RDMA_AHB_END_ADDR_MAN + (OSD_RDMA_CHANNEL_INDEX_VPP2 << 1))
#define END_ADDR_MSB_VPP2	\
	(RDMA_AHB_END_ADDR_MAN_MSB + (OSD_RDMA_CHANNEL_INDEX_VPP2 << 1))

#define OSD_RDMA_FLAG_REG           (osd_rdma_flag_reg[VPP0])
#define OSD_RDMA_FLAG_REG_VPP1      (osd_rdma_flag_reg[VPP1])
#define OSD_RDMA_FLAG_REG_VPP2      (osd_rdma_flag_reg[VPP2])
#define RDMA_DETECT_REG             rdma_detect_reg

#define OSD_RDMA_FLAG_REJECT	(0x99 << 0)
/* hw rdma own this flag, change it to zero when start rdma,
 *  change it to 0 when complete
 */

#define  OSD_RDMA_STATUS_IS_REJECT \
	(osd_reg_read(OSD_RDMA_FLAG_REG) & OSD_RDMA_FLAG_REJECT)
#define  OSD_RDMA_VPP1_STATUS_IS_REJECT \
	(osd_reg_read(OSD_RDMA_FLAG_REG_VPP1) & OSD_RDMA_FLAG_REJECT)
#define  OSD_RDMA_VPP2_STATUS_IS_REJECT \
	(osd_reg_read(OSD_RDMA_FLAG_REG_VPP2) & OSD_RDMA_FLAG_REJECT)

/* hw rdma op, set REJECT */
#define  OSD_RDMA_STATUS_MARK_TBL_RST \
	((osd_reg_read(OSD_RDMA_FLAG_REG) \
	& ~OSD_RDMA_FLAG_REJECT) | \
	(OSD_RDMA_FLAG_REJECT))
#define  OSD_RDMA_VPP1_STATUS_MARK_TBL_RST \
	((osd_reg_read(OSD_RDMA_FLAG_REG_VPP1) \
	& ~OSD_RDMA_FLAG_REJECT) | \
	(OSD_RDMA_FLAG_REJECT))
#define  OSD_RDMA_VPP2_STATUS_MARK_TBL_RST \
	((osd_reg_read(OSD_RDMA_FLAG_REG_VPP2) \
	& ~OSD_RDMA_FLAG_REJECT) | \
	(OSD_RDMA_FLAG_REJECT))


#define  OSD_RDMA_STATUS_MARK_TBL_DONE \
	((osd_reg_read(OSD_RDMA_FLAG_REG) \
	& ~OSD_RDMA_FLAG_REJECT))
#define  OSD_RDMA_VPP1_STATUS_MARK_TBL_DONE \
	((osd_reg_read(OSD_RDMA_FLAG_REG_VPP1) \
	& ~OSD_RDMA_FLAG_REJECT))
#define  OSD_RDMA_VPP2_STATUS_MARK_TBL_DONE \
	((osd_reg_read(OSD_RDMA_FLAG_REG_VPP2) \
	& ~OSD_RDMA_FLAG_REJECT))

/* cpu op, clear REJECT */
#define  OSD_RDMA_STATUS_CLEAR_REJECT \
	(osd_reg_write(OSD_RDMA_FLAG_REG, \
	(osd_reg_read(OSD_RDMA_FLAG_REG) & \
	~OSD_RDMA_FLAG_REJECT)))
#define  OSD_RDMA_VPP1_STATUS_CLEAR_REJECT \
	(osd_reg_write(OSD_RDMA_FLAG_REG_VPP1, \
	(osd_reg_read(OSD_RDMA_FLAG_REG_VPP1) & \
	~OSD_RDMA_FLAG_REJECT)))
#define  OSD_RDMA_VPP2_STATUS_CLEAR_REJECT \
	(osd_reg_write(OSD_RDMA_FLAG_REG_VPP2, \
	(osd_reg_read(OSD_RDMA_FLAG_REG_VPP2) & \
	~OSD_RDMA_FLAG_REJECT)))


int rdma_watchdog_setting(int flag);
int read_rdma_table(u32 vpp_index);
int osd_rdma_enable(u32 vpp_index, u32 enable);
int osd_rdma_reset_and_flush(u32 output_index, u32 reset_bit);
void osd_rdma_interrupt_done_clear(u32 vpp_index);
int osd_rdma_uninit(void);
void set_reset_rdma_trigger_line(void);
void enable_line_n_rdma(void);
void enable_vsync_rdma(u32 vpp_index);
int get_rdma_recovery_stat(u32 vpp_index);
int get_rdma_not_hit_recovery_stat(u32 vpp_index);
int get_rdma_irq_done_line(u32 vpp_index);
void osd_rdma_flag_init(void);
#endif
