/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_AMLOGIC_MESON_DOS_RESET_H
#define _DT_BINDINGS_AMLOGIC_MESON_DOS_RESET_H

/*
 * DOS_SW_RESET0 base is 0x3f00
 * [0]  reserved
 * [1]  Reset Dos top level registers, except sw_reset0/1/2
 *      which is only reset by external reset_n.
 * [2]  VDEC1 Reset assist, mdec's CBUS
 * [3]  VDEC1 Reset mdec's VLD
 * [4]  VDEC1 Reset mdec's VLD
 * [5]  VDEC1 Reset mdec's VLD
 * [6]  VDEC1 Reset mdec's IQIDCT
 * [7]  VDEC1 Reset mdec's MC
 * [8]  VDEC1 Reset mdec's DBLK
 * [9]  VDEC1 Reset mdec's PIC_DC
 * [10] VDEC1 Reset mdec's Pscale
 * [11] VDEC1 Reset vcpu's MCPU
 * [12] VDEC1 Reset vcpu's CCPU
 * [13] VDEC1 Reset mmc_pre_arb
 * [31-14] RESERVED
 */
#define RESET0_VDEC1_RESERVED0		(0)
#define RESET0_VDEC1_TOP_LEVEL		(1)
#define RESET0_VDEC1_MDEC_CBUS		(2)
#define RESET0_VDEC1_MDEC_VLD0		(3)
#define RESET0_VDEC1_MDEC_VLD1		(4)
#define RESET0_VDEC1_MDEC_VLD2		(5)
#define RESET0_VDEC1_MDEC_IQIDCT	(6)
#define RESET0_VDEC1_MDEC_MC		(7)
#define RESET0_VDEC1_MDEC_DBLK		(8)
#define RESET0_VDEC1_MDEC_PIC_DC	(9)
#define RESET0_VDEC1_MDEC_PSCALE	(10)
#define RESET0_VDEC1_VCPU_MCPU		(11)
#define RESET0_VDEC1_VCPU_CCPU		(12)
#define RESET0_VDEC1_MMC_PRE_ARB	(13)

/*
 * DOS_SW_RESET1 base is 0x3f07
 * [0]  Reserved
 * [1]  Reserved
 * [2]  HCODEC Reset assist, hdec's CBUS
 * [3]  HCODEC Reset hdec's VLD
 * [4]  HCODEC Reset hdec's VLD
 * [5]  HCODEC Reset hdec's VLD
 * [6]  HCODEC Reset hdec's IQIDCT
 * [7]  HCODEC Reset hdec's MC
 * [8]  HCODEC Reset hdec's DBLK
 * [9]  HCODEC Reset hdec's PIC_DC
 * [10] HCODEC Reset hdec's Pscale
 * [11] HCODEC Reset vcpu's MCPU
 * [12] HCODEC Reset vcpu's CCPU
 * [13] HCODEC Reset mmc_pre_arb
 * [14] HCODEC Reserved
 * [15] HCODEC Reserved
 * [16] HCODEC Reset henc's VLC
 * [31-17] RESERVED
 */
#define RESET1_HCODEC_RESERVED0		(32)
#define RESET1_HCODEC_RESERVED1		(33)
#define RESET1_HCODEC_HDEC_CBUS		(34)
#define RESET1_HCODEC_HDEC_VLD0		(35)
#define RESET1_HCODEC_HDEC_VLD1		(36)
#define RESET1_HCODEC_HDEC_VLD2		(37)
#define RESET1_HCODEC_HDEC_IQIDCT	(38)
#define RESET1_HCODEC_HDEC_MC		(39)
#define RESET1_HCODEC_HDEC_DBLK		(40)
#define RESET1_HCODEC_HDEC_PIC_DC	(41)
#define RESET1_HCODEC_HDEC_PSCALE	(42)
#define RESET1_HCODEC_VCPU_MCPU		(43)
#define RESET1_HCODEC_VCPU_CCPU		(44)
#define RESET1_HCODEC_MMC_PRE_ARB	(45)
#define RESET1_HCODEC_RESERVED2		(46)
#define RESET1_HCODEC_RESERVED3		(47)
#define RESET1_HCODEC_HENC_VLC		(48)

/*
 * DOS_SW_RESET2 base is 0x3f07
 * [0]  Reserved
 * [1]  Reserved
 * [2]  VDEC2 Reset assist, mdec's CBUS
 * [3]  VDEC2 Reset mdec's VLD
 * [4]  VDEC2 Reset mdec's VLD
 * [5]  VDEC2 Reset mdec's VLD
 * [6]  VDEC2 Reset mdec's IQIDCT
 * [7]  VDEC2 Reset mdec's MC
 * [8]  VDEC2 Reset mdec's DBLK
 * [9]  VDEC2 Reset mdec's PIC_DC
 * [10] VDEC2 Reset mdec's Pscale
 * [11] VDEC2 Reset vcpu's MCPU
 * [12] VDEC2 Reset vcpu's CCPU
 * [13] VDEC2 Reset mmc_pre_arb
 * [31-14]
 */

#define RESET2_VDEC2_RESERVED0		(64)
#define RESET2_VDEC2_RESERVED1		(65)
#define RESET2_VDEC2_MDEC_CBUS		(66)
#define RESET2_VDEC2_MDEC_VLD0		(67)
#define RESET2_VDEC2_MDEC_VLD1		(68)
#define RESET2_VDEC2_MDEC_VLD2		(69)
#define RESET2_VDEC2_MDEC_IQIDCT	(70)
#define RESET2_VDEC2_MDEC_MC		(71)
#define RESET2_VDEC2_MDEC_DBLK		(72)
#define RESET2_VDEC2_MDEC_PIC_DC	(73)
#define RESET2_VDEC2_MDEC_PSCALE	(74)
#define RESET2_VDEC2_VCPU_MCPU		(75)
#define RESET2_VDEC2_VCPU_CCPU		(76)
#define RESET2_VDEC2_MMC_PRE_ARB	(77)

/* DOS_SW_RESET3 base is 0x3f34 */
#define RESET3_RESERVED0		(96)
/**97**/
#define RESET3_BIT2			(98)
#define RESET3_BIT3			(99)
#define RESET3_BIT4			(100)
#define RESET3_BIT5			(101)
#define RESET3_BIT6			(102)
#define RESET3_BIT7			(103)
#define RESET3_BIT8			(104)
#define RESET3_BIT9			(105)
#define RESET3_BIT10			(106)
#define RESET3_BIT11			(107)
#define RESET3_BIT12			(108)
#define RESET3_BIT13			(109)
#define RESET3_BIT14			(110)
#define RESET3_BIT15			(111)
#define RESET3_BIT16			(112)
#define RESET3_BIT17			(113)
#define RESET3_BIT18			(114)
#define RESET3_BIT19			(115)
/**116~119**/
#define RESET3_BIT24			(120)

/* DOS_SW_RESET4 base is 0x3f37 */
#define RESET4_RESERVED0		(128)
/**127~135**/
#define RESET4_BIT8			(136)
#define RESET4_BIT9			(137)
#define RESET4_BIT10			(138)
#define RESET4_BIT11			(139)
#define RESET4_RESERVED31		(159)

#endif //_DT_BINDINGS_AMLOGIC_MESON_DOS_RESET_H
