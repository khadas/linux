/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _TS_OUTPUT_REG_H_
#define _TS_OUTPUT_REG_H_

//#define SECURE_BASE  (0xFE440000 - 0xFE000000)
#define SECURE_BASE  (0x0)
#define TS_OUT_REG_BASE  (SECURE_BASE + 0x2c00)

union PID_RDY_FIELD {
	unsigned int data;
	struct {
		unsigned int pid_rdy:1;
		unsigned int rsvd_0:31;
	} b;
};

#define PID_RDY  (TS_OUT_REG_BASE + 0x0)

union PID_RDY_DBG_FIELD {
	unsigned int data;
	struct {
		unsigned int pid_rdy_dbg:1;
		unsigned int rsvd_0:31;
	} b;
};

#define PID_RDY_DBG  (TS_OUT_REG_BASE + 0x4)

// Bit 9   :0      pid_entry                      U     RW        default = 'h0
// Bit 10          remap_flag                     U     RW        default = 'h0
// Bit 22  :16     buffer_id                      U     RW        default = 'h0
// Bit 24          on_off                         U     RW        default = 'h0
// Bit 29  :28     mode                           U     RW        default = 'h0
// Bit 31          ap_pending                     U     RW        default = 'h0
union PID_CFG_FIELD {
	unsigned int data;
	struct {
		unsigned int pid_entry:10;
		unsigned int remap_flag:1;
		unsigned int rsvd_0:5;
		unsigned int buffer_id:7;
		unsigned int rsvd_1:1;
		unsigned int on_off:1;
		unsigned int rsvd_2:3;
		unsigned int mode:2;
		unsigned int rsvd_3:1;
		unsigned int ap_pending:1;
	} b;
};

#define TS_OUTPUT_PID_CFG  (TS_OUT_REG_BASE + 0x8)

// Bit 12  :0      pid_mask                       U     RW        default = 'h0
// Bit 28  :16     pid                            U     RW        default = 'h0
union PID_DATA_FIELD {
	unsigned int data;
	struct {
		unsigned int pid_mask:13;
		unsigned int rsvd_0:3;
		unsigned int pid:13;
		unsigned int rsvd_1:3;
	} b;
};

#define TS_OUTPUT_PID_DAT  (TS_OUT_REG_BASE + 0xc)

// Bit 5   :0      sys_freq                       U     RW        default = 'h18
union OUT_CFG_FIELD {
	unsigned int data;
	struct {
		unsigned int sys_freq:6;
		unsigned int rsvd_0:26;
	} b;
};

#define OUT_CFG  (TS_OUT_REG_BASE + 0x10)

/*from 0~15*/
// Bit 12  :0      pcr_pid_0                      U     RW        default = 'h0
// Bit 18  :13     sid_0                          U     RW        default = 'h0
// Bit 19          valid_0                        U     RW        default = 'h0
// Bit 26  :20     buffer id                     U     RW
// Bit 27          ext                           U     RW
union PCR_TEMI_TAB_FIELD {
	unsigned int data;
	struct {
		unsigned int pcr_pid:13;
		unsigned int sid:6;
		unsigned int valid:1;
		unsigned int buffer_id:7;
		unsigned int ext:1;
		unsigned int rsvd:4;
	} b;
};

#define TS_OUTPUT_PCR_TAB(i) (TS_OUT_REG_BASE + 0x40 + (i) * 4)

/*fromt 0~31*/
// Bit 7   :0      sid0_begin_0                   U     RW        default = 'h0
// Bit 15  :8      sid0_length_0                  U     RW        default = 'h0
// Bit 23  :16     sid1_begin_0                   U     RW        default = 'h0
// Bit 31  :24     sid1_length_0                  U     RW        default = 'h0
union SID_TAB_FIELD {
	unsigned int data;
	struct {
		unsigned int sid0_begin:8;
		unsigned int sid0_length:8;
		unsigned int sid1_begin:8;
		unsigned int sid1_length:8;
	} b;
};

#define SID_TAB(i) (TS_OUT_REG_BASE + 0x80 + (i) * 4)

/*from 0~63*/
// Bit 12  :0      pid_0                          U     RW        default = 'h0
// Bit 18  :13     sid_0                          U     RW        default = 'h0
// Bit 19          reset_0                        U     RW        default = 'h0
// Bit 20          dup_ok_0                       U     RW        default = 'h0
// Bit 23  :22     on_off_0                       U     RW        default = 'h0
union ES_TAB_FIELD {
	unsigned int data;
	struct {
		unsigned int pid:13;
		unsigned int sid:6;
		unsigned int reset:1;
		unsigned int dup_ok:1;
		unsigned int rsvd_0:1;
		unsigned int on_off:2;
		unsigned int rsvd_1:8;
	} b;
};

#define TS_OUTPUT_ES_TAB(i)   (TS_OUT_REG_BASE + 0x100 + (i) * 4)

/*from 0~15*/
// Bit 31  :0      lsb_0                          U     RW        default = 'h0
union PCR_REG_LSB_FIELD {
	unsigned int data;
	struct {
		unsigned int lsb:32;
	} b;
};

#define TS_OUTPUT_PCR_REG_LSB(i) (TS_OUT_REG_BASE + 0x200 + (i) * 8)

/*from 0~15*/
// Bit 0           msb_0                          U     RW        default = 'h0
union PCR_REG_MSB_FIELD {
	unsigned int data;
	struct {
		unsigned int msb:1;
		unsigned int rsvd:31;
	} b;
};

#define TS_OUTPUT_PCR_REG_MSB(i) (TS_OUT_REG_BASE + 0x204 + (i) * 8)

#define TS_OUT_SID_TAB_BASE			   (TS_OUT_REG_BASE + 0x80)

/*PID table entry*/
#define PID_ENTRY_VALID         63
#define PID_ENTRY_RESERVED      56
#define PID_ENTRY_TEE           55
#define PID_ENTRY_SCB00         54
#define PID_ENTRY_SCBOUT        52
#define PID_ENTRY_SCBASIS       51
#define PID_ENTRY_ODD_IV        45
#define PID_ENTRY_EVEN_00_IV    39
#define PID_ENTRY_SID           33
#define PID_ENTRY_PID           20
#define PID_ENTRY_ALGO          16
#define PID_ENTRY_KTE_ODD       8
#define PID_ENTRY_KET_EVEN_00   0

/*************************************************************/
/*************************************************************/
/*************************************************************/
/*************************************************************/
/*TS table entry*/
#define TS_PID_ENTRY_ON_OFF     33
#define TS_PID_ENTRY_PID        20
#define TS_PID_ENTRY_PIDMASK    7
#define TS_PID_ENTRY_BUFFER_ID  0

#endif
