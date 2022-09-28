/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DEMOD_REG_H_
#define _DEMOD_REG_H_

#define DEMOD_REG_BASE         (SECURE_BASE + 0x300)

/*bit 25:31 --Unused.*/
/*bit 24 --disable serial2parallel channel0 or not.
 * Set to 1 to disable.
 * default:0x0
 */
/*bit 16:23 --The user-defined header tag sync byte.
 * default:0x47
 */
/*bit 15 --when sop valid, if sync data error,
 * will ignore the packet. Set 1 to ignore.
 * default:0x0
 */
/*bit 14 --serial2parallel channel use the original input
 * FAIL signal or not. Set 1 to use pin.
 * default:0x0
 */
/*bit 13 --Sync channel0 ignore the original input
 * fail signal or not, set 1 to ignore.
 * default:0x0
 */
/*bit 12 -- Not used.*/
/*bit 11:8 --The clk divider when convert serial signals to
 * parallel signals. Divider value is (s2p0_clk_div+1),
 * default:0x7
 */
/*bit 3:7 --Input signals polarity invert. default:0x0
 *bit4: set 1 to invert input error signal
 *bit3: set 1 to invert input data signal
 *bit2: set 1 to invert input sync signal
 *bit1: set 1 to invert input valid signal
 *bit0: set 1 to invert input clk signal
 */
/*bit 2 --for path0_ctrl/path2_ctrl/path3_ctrl is rsvd1
 *for path1_ctrl is ts_s_or_p_sel1,
 *Sync channel0 choose to transfer parallel channel0
 *or original input channel0.
 * 1'b1: parallel channel 0
 * 1'b0: original input channel 0
 */
/*bit 1 -- tie the valid to 1 under 3wire mode
 * when set 1 to avoid the input pad toggling, default: 0
 */
/*bit 0 --serial2parallel channel 0 choose
 * to transform input channel0 or nothing. default:0x1
 *1'b1: select channel 0
 *1'b0: nothing
 */
#define PATH_CTRL_RSVD2						25
#define S2P_DISABLE							24
#define FEC_SYNC_BYTE						16
#define IGNORE_SYNC_ERR_PKT					15
#define USE_FAIL_PIN						14
#define IGNORE_D_FAIL_PKT					13
#define DONOT_USE_SOP						12
#define S2P_CLK_DIV							8
#define SERIAL_CONTROL						3
#define PATH_CTRL_RSVD1						2
#define TS_S_OR_P_SEL1						2
#define FEC_S2P_3WIRE						1
#define FEC_S2P_SEL							0
/*there are 4 path ctrol for 4 TS input s2p*/
#define DEMOD_PATH_CTRL(i)     (DEMOD_REG_BASE + 0xa0 + 4 * (i))

/*bit 28:31 --null*/
/*bit 24:27 --The sid offset in custom header, default:0x1*/
/*bit 16:23 --The user-defined header byte of channel, default:0x4b*/
/*bit 13:15 --null*/
/*bit 12 --1: cable card; 0: custom header, default:0x1*/
/*bit 10:11 --null*/
/*bit 8:9 --The length of header tag in channel0, default:0x2
 *2'b11: 12 bytes
 *2'b10: 8 bytes
 *2'b01: 4 bytes
 *2'b00: no header
 */
/*bit 6:7 --null*/
/*bit 0:5 --The added sid number of channel, default:0x21*/
#define PKT_CFG_RSVD3                       28
#define SID_OFFSET                          24
#define CUSTOM_HEADER                       16
#define PKT_CFG_RSVD2                       13
#define CC_HEADER                           12
#define PKT_CFG_RSVD1                       10
#define HEADER_LEN                          8
#define PKT_CFG_RSVD0                       6
#define SIGNAL_SID                          0
/*there are 4 pkt cfg for 4 ts input sid or header*/
#define DEMOD_PKT_CFG(i)	   (DEMOD_REG_BASE + 0xb0 + 4 * (i))

/*bit 28:31 --no used*/
/*bit 16:27 -- fifo0 threshold, 188x6-1, default:0x457*/
/*bit 12:15 --no used*/
/*bit 0:11 -- fifo0 threshold, 188x6-1, default:0x457*/
#define FIFO_RSVD1							28
#define FIFO1_THRESHOLD_SIZE				16
#define FIFO_RSVD0							12
#define FIFO0_THRESHOLD_SIZE				0
/*there are 4 fifo for 4 ts input */
#define DEMOD_FIFO_CFG(i)		(DEMOD_REG_BASE + 0xc0 + 4 * ((i) / 2))

/*bit 8:12 -- FEC output signal polarity invert. default: 0x0
 *bit4: set 1 to invert output fail signal.
 *bit3: set 1 to invert output data signal.
 *bit2: set 1 to invert output sop signal.
 *bit1: set 1 to invert output valid signal.
 *bit0: set 1 to invert output clk signal.
 */
/*bit 0:7 -- FEC output signal select. default:0x0
 *8'b00000000: ts0
 *8'b00000001: ts1
 *8b'00000011: ts2
 *8'b00000111: ts3
 *8'b00001111: s2p0
 *8'b00011111: s2p1
 *8'b00111111: s2p2
 *8'b01111111: s2p3
 *8'b11111111: file
 *Else: output all 0
 */
#define TS_OUT_INVERT			8
#define FEC_OUT_SEL				0
//no used
#define DEMOD_TS_O_PATH_CTRL	(DEMOD_REG_BASE + 0xc8)

/*bit 15:17 -- no used*/
/*bit 12:14 --default:0x0
 *0: clean pkt_d_error_int
 *1: clean pkt_sync_error_int
 *2: clean fifo overflow int
 */
/*bit 11 -- no used*/
/*bit 8:10 --default:0x0
 *0: clean pkt_d_error_int
 *1: clean pkt_sync_error_int
 *2: clean fifo overflow int
 */
/*bit 7 -- no used*/
/*bit 4:6 --default:0x0
 *0: clean pkt_d_error_int
 *1: clean pkt_sync_error_int
 *2: clean fifo overflow int
 */
/*bit 3 -- no used*/
/*bit 0:2 --default:0x0
 *0: clean pkt_d_error_int
 *1: clean pkt_sync_error_int
 *2: clean fifo overflow int
 */
#define CLEAN_CHAN_RSVD3			15
#define CLEAN_CHAN3_INT				12
#define CLEAN_CHAN_RSVD2			11
#define CLEAN_CHAN2_INT				8
#define CLEAN_CHAN_RSVD1			7
#define CLEAN_CHAN1_INT				4
#define CLEAN_CHAN_RSVD0			3
#define CLEAN_CHAN0_INT				0
#define DEMOD_CLEAN_DEMOD_INT	(DEMOD_REG_BASE + 0xcc)

/*bit 15:17 -- no used*/
/*bit 12:14 --default:0x0
 *0: mask pkt_d_error_int
 *1: mask pkt_sync_error_int
 *2: mask fifo overflow int
 */
/*bit 11 -- no used*/
/*bit 8:10 --default:0x0
 *0: mask pkt_d_error_int
 *1: mask pkt_sync_error_int
 *2: mask fifo overflow int
 */
/*bit 7 -- no used*/
/*bit 4:6 --default:0x0
 *0: mask pkt_d_error_int
 *1: mask pkt_sync_error_int
 *2: mask fifo overflow int
 */
/*bit 3 -- no used*/
/*bit 0:2 --default:0x0
 *0: mask pkt_d_error_int
 *1: mask pkt_sync_error_int
 *2: mask fifo overflow int
 */
#define MASK_CHAN_RSVD3				15
#define MASK_CHAN3_INT				12
#define MASK_CHAN_RSVD2				11
#define MASK_CHAN2_INT				8
#define MASK_CHAN_RSVD1				7
#define MASK_CHAN1_INT				4
#define MASK_CHAN_RSVD0				3
#define MASK_CHAN0_INT				0
#define DEMOD_INT_MASK			(DEMOD_REG_BASE + 0xd0)

/*bit 15--null*/
/*bit 14--fifo overflow int*/
/*bit 13--pkt_sync_error_int*/
/*bit 12--pkt_d_error_int*/
/*bit 11--null*/
/*bit 10--fifo overflow int*/
/*bit 9--pkt_sync_error_int*/
/*bit 8--pkt_d_error_int*/
/*bit 7--null*/
/*bit 6--fifo overflow int*/
/*bit 5--pkt_sync_error_int*/
/*bit 4--pkt_d_error_int*/
/*bit 3--null*/
/*bit 2--fifo overflow int*/
/*bit 1--pkt_sync_error_int*/
/*bit 0--pkt_d_error_int*/
#define PKT_CH_RSVD3                15
#define CH_OV3                      14
#define PKT_SYNC_ERR3               13
#define PKT_FAIL3                   12
#define PKT_CH_RSVD2                11
#define CH_OV2                      10
#define PKT_SYNC_ERR2               9
#define PKT_FAIL2                   8
#define PKT_CH_RSVD1                7
#define CH_OV1                      6
#define PKT_SYNC_ERR1               5
#define PKT_FAIL1                   4
#define PKT_CH_RSVD0                3
#define CH_OV0                      2
#define PKT_SYNC_ERR0               1
#define PKT_FAIL0                   0
#define DEMOD_INT_STATUS            (DEMOD_REG_BASE + 0xd4)

/*bit 24:31 -- reserved*/
/*bit 16:23 -- FIFO overflow counter for channel*/
/*bit 8:15 --Packet sync error counter for channel */
/*bit 0:7 -- Error packet counter for channe*/
#define CH_RSVD                     24
#define CH_OV_CNT                   16
#define CH_PTK_SYNC_ERR_CNT         8
#define CH_PTK_D_ERR_CNT            0
#define DEMOD_TS_CH_ERR_STATUS(i)   (DEMOD_REG_BASE + 0xd8 + 4 * (i))

#endif
