/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MEM_DESC_REG_H_
#define _MEM_DESC_REG_H_

#define TS_DMA_RCH_BASE (SECURE_BASE + 0x4000)
#define TS_DMA_WCH_BASE (SECURE_BASE + 0x4000 + 0x1000)

/******************************EACH RCH CONFIG***********************/
/*READY bit define*/
#define RCH_READY_SEMAPHORE					0
/*Read 1 to own read channel;
 * Write 1 to release read channel.default 0
 */
#define TS_DMA_RCH_READY(i) (TS_DMA_RCH_BASE \
		+ (i) * 0x20 + 0)

/*STATUS bit define*/
#define RCH_STATUS_READ_CMD_CUR_CHAN_NUM	0
/*current channel read cmd number*/
#define RCH_STATUS_READ_CMD_TOTAL_NUM		4
/*the sum of all read cmd*/
#define RCH_STATUS_AXI_BUS_DATA_LINE_NUM	8
/*the number axi bus data line*/
#define RCH_STATUS_READ_FIFO_LEVEL          16
/*read fifo level*/
#define TS_DMA_RCH_STATUS(i) (TS_DMA_RCH_BASE \
		+ (i) * 0x20 + 4)

/*CFG bit define*/
#define RCH_CFG_READ_LEN					0
/*single read length*/
#define RCH_CFG_PACKET_LEN					16
/*a packet length*/
#define RCH_CFG_RESERVED					25
#define RCH_CFG_PAUSE						26
#define RCH_CFG_ENABLE						27
#define RCH_CFG_DONE						28
/*no used*/
#define TS_DMA_RCH_EACH_CFG(i)	(TS_DMA_RCH_BASE \
		+ (i) * 0x20 + 8)

/*ADDR bit define */
#define RCH_DESC_ADDR						0
/*First descriptor address*/
#define TS_DMA_RCH_ADDR(i)	(TS_DMA_RCH_BASE \
		+ (i) * 0x20 + 0xc)

/*LEN bit define*/
#define RCH_TOTAL_BYTES						0
/*Totally bytes for the TS*/
#define TS_DMA_RCH_LEN(i)	(TS_DMA_RCH_BASE \
		+ (i) * 0x20 + 0x10)

/*RD LEN bit define*/
#define RCH_READ_DATA_NUM					0
/*Amount of read data, read only for debug*/
#define TS_DMA_RCH_RD_LEN(i)	(TS_DMA_RCH_BASE \
		+ (i) * 0x20 + 0x14)

/*PTR bit define*/
#define RCH_READ_ADDR						0
/*Read addr pointer, read only for debug*/
#define TS_DMA_RCH_PTR(i)	(TS_DMA_RCH_BASE \
		+ (i) * 0x20 + 0x18)

/*PKT SYNC STATUS bit define*/
#define RCH_LAST_PKT_SYNC_CNT               0
/*the last pkt_sync counter of current channel*/
#define RCH_PKT_SYNC_CNT                    4
/*the counter of pkt_sync*/
#define RCH_TS_RESYNC_NUM                   8
/*the number of ts re_sync*/
#define RCH_SYNC_FSM_PRE_STATE              16
/*the last state machine of current channel*/
#define RCH_SYNC_FSM_CUR_STATE              18
/*the state of sync machine*/
#define RCH_CUR_TS_SID                      20
/*current ts sid*/
#define TS_DMA_RCH_PKT_SYNC_STATUS(i) (TS_DMA_RCH_BASE \
		+ (i) * 0x20 + 0x1C)

#define TS_DMA_RCH_DONE			(TS_DMA_RCH_BASE + 0x204c)
#define TS_DMA_RCH_CLEAN		(TS_DMA_RCH_BASE + 0x2024)

#define TS_DMA_RCH_ACTIVE		(TS_DMA_RCH_BASE + 0x2038)

#define TS_DMA_RCH_RDES_ERR		(TS_DMA_RCH_BASE + 0x2060)
#define TS_DMA_RCH_RDES_LEN_ERR	(TS_DMA_RCH_BASE + 0x2064)

#define TS_DMA_RCH_CFG			(TS_DMA_RCH_BASE + 0x20ac)

/******************************EACH WCH CONFIG***********************/
/*READY bit define*/
#define WCH_READY_SEMAPHORE					0
/*Read 1 to own read channel
 *Write 1 to release read channel
 */
#define TS_DMA_WCH_READY(i)	(TS_DMA_WCH_BASE \
		+ (i) * 0x20 + 0)

/*DEBUG bit define*/
#define WCH_DEBUG_CHECK_SEMAPHORE			0
/*Debug*/
#define TS_DMA_WCH_DEBUG(i)	(TS_DMA_WCH_BASE \
		+ (i) * 0x20 + 4)

/*addr bit define*/
#define WCH_DESC_ADDR						0
/*First descriptor address*/
#define TS_DMA_WCH_ADDR(i)	(TS_DMA_WCH_BASE + (i) * 0x20 \
		+ 0xc)

/*wch len bit define*/
#define WCH_TOTAL_BYTES						0
/*Totally bytes for the TS*/
#define TS_DMA_WCH_LEN(i)	(TS_DMA_WCH_BASE + (i) * 0x20 \
		+ 0x10)

/*wch w len bit define*/
#define WCH_WRITE_DATA_NUM					0
 /*DEBUG*/
#define TS_DMA_WCH_WR_LEN(i) (TS_DMA_WCH_BASE + (i) * 0x20 + 0x14)
/*wch ptr bit define*/
#define WCH_WRITE_ADDR						0
/*Write addr pointer, read only for debug*/
#define TS_DMA_WCH_PTR(i)	(TS_DMA_WCH_BASE + (i) * 0x20 + 0x18)
/*cmd cnt bit define*/
#define WCH_CMD_CNT                         0
/*The counter of wr cmd is on-going*/
#define TS_DMA_WCH_CMD_CNT(i)	(TS_DMA_WCH_BASE + (i) * 0x20 + 0x1C)
/*CFG bit define*/
#define WCH_CFG_CLEAR						25
#define TS_DMA_WCH_CFG(i)	(TS_DMA_WCH_BASE + (i) * 0x20 + 0x8)
#define TS_DMA_WCH_INT_MASK			(TS_DMA_RCH_BASE + 0x2004)
#define TS_DMA_WCH_CLEAN_BATCH		(TS_DMA_RCH_BASE + 0x2014)
#define TS_DAM_WCH_CLEAN			(TS_DMA_RCH_BASE + 0x2028)
#define TS_DMA_WCH_ACTIVE			(TS_DMA_RCH_BASE + 0x203c)
#define TS_DMA_WCH_DONE				(TS_DMA_RCH_BASE + 0x2050)
#define TS_DMA_WCH_ERR				(TS_DMA_RCH_BASE + 0x2068)
#define TS_DMA_WCH_BATCH_END		(TS_DMA_RCH_BASE + 0x2078)
#define TS_DMA_WCH_EOC_DONE			(TS_DMA_RCH_BASE + 0x2088)
#define TS_DMA_WCH_RESP_ERR			(TS_DMA_RCH_BASE + 0x2098)
#define TS_DMA_WCH_CFG_FAST_MODE	(TS_DMA_RCH_BASE + 0x20b0)
#endif
