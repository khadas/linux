/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_REGS_H_
#define __AML_REGS_H_

enum clk_sel {
	MASTER_A,
	MASTER_B,
	MASTER_C,
	MASTER_D,
	MASTER_E,
	MASTER_F,
	SLAVE_A,
	SLAVE_B,
	SLAVE_C,
	SLAVE_D,
	SLAVE_E,
	SLAVE_F,
	SLAVE_G,
	SLAVE_H,
	SLAVE_I,
	SLAVE_J
};

struct register_table {
	char *name;
	unsigned int addr;
};

#define AUD_ADDR_OFFSET(addr)              ((addr) << 2)

/*
 *  PDM - Registers
 */
#define PDM_CTRL                           0x00
#define PDM_HCIC_CTRL1                     0x01
#define PDM_HCIC_CTRL2                     0x02
#define PDM_F1_CTRL                        0x03
#define PDM_F2_CTRL                        0x04
#define PDM_F3_CTRL                        0x05
#define PDM_HPF_CTRL                       0x06
#define PDM_CHAN_CTRL                      0x07
#define PDM_CHAN_CTRL1                     0x08
#define PDM_COEFF_ADDR                     0x09
#define PDM_COEFF_DATA                     0x0A
#define PDM_CLKG_CTRL                      0x0B
#define PDM_STS                            0x0C
#define PDM_MUTE_VALUE                     0x0D
#define PDM_MASK_NUM                       0x0E
#define PDM_CHAN_CTRL2                     0x0F
/*
 *	AUDIO CLOCK, MST PAD,
 */
#define EE_AUDIO_CLK_GATE_EN0              0x000
#define EE_AUDIO_CLK_GATE_EN1              0x001
#define EE_AUDIO_MCLK_A_CTRL(offset)       (0x001 + (offset))
#define EE_AUDIO_MCLK_B_CTRL(offset)       (0x002 + (offset))
#define EE_AUDIO_MCLK_C_CTRL(offset)       (0x003 + (offset))
#define EE_AUDIO_MCLK_D_CTRL(offset)       (0x004 + (offset))
#define EE_AUDIO_MCLK_E_CTRL(offset)       (0x005 + (offset))
#define EE_AUDIO_MCLK_F_CTRL(offset)       (0x006 + (offset))
#define EE_AUDIO_MST_PAD_CTRL0(offset)     (0x007 + (offset))
#define EE_AUDIO_MST_PAD_CTRL1(offset)     (0x008 + (offset))

#define REG_BIT_RESET_PDM				BIT(0)
#define REG_BIT_RESET_TDMINA				BIT(1)
#define REG_BIT_RESET_TDMINB				BIT(2)
#define REG_BIT_RESET_TDMINC				BIT(3)
#define REG_BIT_RESET_TDMIN_LB				BIT(4)
#define REG_BIT_RESET_LOOPBACK				BIT(5)
#define REG_BIT_RESET_TODDRA				BIT(6)
#define REG_BIT_RESET_TODDRB				BIT(7)
#define REG_BIT_RESET_TODDRC				BIT(8)
#define REG_BIT_RESET_FRDDRA				BIT(9)
#define REG_BIT_RESET_FRDDRB				BIT(10)
#define REG_BIT_RESET_FRDDRC				BIT(11)
#define REG_BIT_RESET_TDMOUTA				BIT(12)
#define REG_BIT_RESET_TDMOUTB				BIT(13)
#define REG_BIT_RESET_TDMOUTC				BIT(14)
#define REG_BIT_RESET_SPDIFOUTA				BIT(15)
#define REG_BIT_RESET_SPDIFOUTB				BIT(16)
#define REG_BIT_RESET_SPDIFIN				BIT(17)
#define REG_BIT_RESET_EQDRC				BIT(18)
#define REG_BIT_RESET_RESAMPLE				BIT(19)
#define REG_BIT_RESET_DDRARB				BIT(20)
#define REG_BIT_RESET_POWDET				BIT(21)
#define REG_BIT_RESET_TORAM				BIT(22)
#define REG_BIT_RESET_TOACODEC				BIT(23)
#define REG_BIT_RESET_TOHDMITX				BIT(24)
#define REG_BIT_RESET_CLKTREE				BIT(25)
#define REG_BIT_RESET_RESAMPLEB				BIT(26)
#define REG_BIT_RESET_TOVAD				BIT(27)
#define REG_BIT_RESET_LOCKER				BIT(28)
#define REG_BIT_RESET_SPDIFIN_LB			BIT(29)
#define REG_BIT_RESET_FRATV				BIT(30)
#define REG_BIT_RESET_FRHDMIRX				BIT(31)
#define REG_BIT_RESET_TDMOUTD           BIT(12)

#define EE_AUDIO_SW_RESET0(offset)         (0x009 + (offset))
#define EE_AUDIO_SW_RESET1(offset)         (0x00a + (offset))
#define REG_BIT_RESET_FRDDRD				BIT(0)
#define REG_BIT_RESET_TODDRD				BIT(1)
#define REG_BIT_RESET_LOOPBACKB				BIT(2)

#define EE_AUDIO_CLK81_CTRL                0x00c
#define EE_AUDIO_CLK81_EN                  0x00d

#define EE_AUDIO_MST_A_SCLK_CTRL0          0x010
#define EE_AUDIO_MST_A_SCLK_CTRL1          0x011
#define EE_AUDIO_MST_B_SCLK_CTRL0          0x012
#define EE_AUDIO_MST_B_SCLK_CTRL1          0x013
#define EE_AUDIO_MST_C_SCLK_CTRL0          0x014
#define EE_AUDIO_MST_C_SCLK_CTRL1          0x015
#define EE_AUDIO_MST_D_SCLK_CTRL0          0x016
#define EE_AUDIO_MST_D_SCLK_CTRL1          0x017
#define EE_AUDIO_MST_E_SCLK_CTRL0          0x018
#define EE_AUDIO_MST_E_SCLK_CTRL1          0x019
#define EE_AUDIO_MST_F_SCLK_CTRL0          0x01a
#define EE_AUDIO_MST_F_SCLK_CTRL1          0x01b

#define EE_AUDIO_CLK_TDMIN_A_CTRL          0x020
#define EE_AUDIO_CLK_TDMIN_B_CTRL          0x021
#define EE_AUDIO_CLK_TDMIN_C_CTRL          0x022
#define EE_AUDIO_CLK_TDMIN_D_CTRL          0x03a
#define EE_AUDIO_CLK_TDMIN_LB_CTRL         0x023
#define EE_AUDIO_CLK_TDMINB_LB_CTRL        0x037

#define EE_AUDIO_CLK_TDMOUT_A_CTRL         0x024
#define EE_AUDIO_CLK_TDMOUT_B_CTRL         0x025
#define EE_AUDIO_CLK_TDMOUT_C_CTRL         0x026
#define EE_AUDIO_CLK_TDMOUT_D_CTRL         0x03b

#define EE_AUDIO_CLK_SPDIFIN_CTRL          0x027
#define EE_AUDIO_CLK_SPDIFOUT_CTRL         0x028
#define EE_AUDIO_CLK_RESAMPLEA_CTRL        0x029
#define EE_AUDIO_CLK_LOCKER_CTRL           0x02a
#define EE_AUDIO_CLK_PDMIN_CTRL0           0x02b
#define EE_AUDIO_CLK_PDMIN_CTRL1           0x02c
#define EE_AUDIO_CLK_PDMIN_CTRL2           0x038
#define EE_AUDIO_CLK_PDMIN_CTRL3           0x039
#define EE_AUDIO_CLK_SPDIFOUT_B_CTRL       0x02d
#define EE_AUDIO_CLK_RESAMPLEB_CTRL        0x02e
#define EE_AUDIO_CLK_SPDIFIN_LB_CTRL       0x02f
#define EE_AUDIO_CLK_EQDRC_CTRL0           0x030
#define EE_AUDIO_CLK_VAD_CTRL              0x031
#define EE_AUDIO_EARCTX_CMDC_CLK_CTRL      0x032
#define EE_AUDIO_EARCTX_DMAC_CLK_CTRL      0x033
#define EE_AUDIO_EARCRX_CMDC_CLK_CTRL      0x034
#define EE_AUDIO_EARCRX_DMAC_CLK_CTRL      0x035

/*
 *	AUDIO TODDR
 */
#define EE_AUDIO_TODDR_A_CTRL0             0x040
#define EE_AUDIO_TODDR_A_CTRL1             0x041
#define EE_AUDIO_TODDR_A_START_ADDR        0x042
#define EE_AUDIO_TODDR_A_FINISH_ADDR       0x043
#define EE_AUDIO_TODDR_A_INT_ADDR          0x044
#define EE_AUDIO_TODDR_A_STATUS1           0x045
#define EE_AUDIO_TODDR_A_STATUS2           0x046
#define EE_AUDIO_TODDR_A_START_ADDRB       0x047
#define EE_AUDIO_TODDR_A_FINISH_ADDRB      0x048
#define EE_AUDIO_TODDR_A_INIT_ADDR         0x049
#define EE_AUDIO_TODDR_A_CTRL2             0x04a

#define EE_AUDIO_TODDR_A_CHNUM_ID0         0x300
#define EE_AUDIO_TODDR_A_CHNUM_ID1         0x301
#define EE_AUDIO_TODDR_A_CHNUM_ID2         0x302
#define EE_AUDIO_TODDR_A_CHNUM_ID3         0x303
#define EE_AUDIO_TODDR_A_CHNUM_ID4         0x304
#define EE_AUDIO_TODDR_A_CHNUM_ID5         0x305
#define EE_AUDIO_TODDR_A_CHNUM_ID6         0x306
#define EE_AUDIO_TODDR_A_CHNUM_ID7         0x307
#define EE_AUDIO_TODDR_A_CHSYNC_CTRL       0x30F

#define EE_AUDIO_TODDR_B_CTRL0             0x050
#define EE_AUDIO_TODDR_B_CTRL1             0x051
#define EE_AUDIO_TODDR_B_START_ADDR        0x052
#define EE_AUDIO_TODDR_B_FINISH_ADDR       0x053
#define EE_AUDIO_TODDR_B_INT_ADDR          0x054
#define EE_AUDIO_TODDR_B_STATUS1           0x055
#define EE_AUDIO_TODDR_B_STATUS2           0x056
#define EE_AUDIO_TODDR_B_START_ADDRB       0x057
#define EE_AUDIO_TODDR_B_FINISH_ADDRB      0x058
#define EE_AUDIO_TODDR_B_INIT_ADDR         0x059
#define EE_AUDIO_TODDR_B_CTRL2             0x05a

#define EE_AUDIO_TODDR_B_CHNUM_ID0         0x310
#define EE_AUDIO_TODDR_B_CHNUM_ID1         0x311
#define EE_AUDIO_TODDR_B_CHNUM_ID2         0x312
#define EE_AUDIO_TODDR_B_CHNUM_ID3         0x313
#define EE_AUDIO_TODDR_B_CHNUM_ID4         0x314
#define EE_AUDIO_TODDR_B_CHNUM_ID5         0x315
#define EE_AUDIO_TODDR_B_CHNUM_ID6         0x316
#define EE_AUDIO_TODDR_B_CHNUM_ID7         0x317
#define EE_AUDIO_TODDR_B_CHSYNC_CTRL       0x31F

#define EE_AUDIO_TODDR_C_CTRL0             0x060
#define EE_AUDIO_TODDR_C_CTRL1             0x061
#define EE_AUDIO_TODDR_C_START_ADDR        0x062
#define EE_AUDIO_TODDR_C_FINISH_ADDR       0x063
#define EE_AUDIO_TODDR_C_INT_ADDR          0x064
#define EE_AUDIO_TODDR_C_STATUS1           0x065
#define EE_AUDIO_TODDR_C_STATUS2           0x066
#define EE_AUDIO_TODDR_C_START_ADDRB       0x067
#define EE_AUDIO_TODDR_C_FINISH_ADDRB      0x068
#define EE_AUDIO_TODDR_C_INIT_ADDR         0x069
#define EE_AUDIO_TODDR_C_CTRL2             0x06a

#define EE_AUDIO_TODDR_C_CHNUM_ID0         0x320
#define EE_AUDIO_TODDR_C_CHNUM_ID1         0x321
#define EE_AUDIO_TODDR_C_CHNUM_ID2         0x322
#define EE_AUDIO_TODDR_C_CHNUM_ID3         0x323
#define EE_AUDIO_TODDR_C_CHNUM_ID4         0x324
#define EE_AUDIO_TODDR_C_CHNUM_ID5         0x325
#define EE_AUDIO_TODDR_C_CHNUM_ID6         0x326
#define EE_AUDIO_TODDR_C_CHNUM_ID7         0x327
#define EE_AUDIO_TODDR_C_CHSYNC_CTRL       0x32F

#define EE_AUDIO_TODDR_D_CTRL0             0x210
#define EE_AUDIO_TODDR_D_CTRL1             0x211
#define EE_AUDIO_TODDR_D_START_ADDR        0x212
#define EE_AUDIO_TODDR_D_FINISH_ADDR       0x213
#define EE_AUDIO_TODDR_D_INT_ADDR          0x214
#define EE_AUDIO_TODDR_D_STATUS1           0x215
#define EE_AUDIO_TODDR_D_STATUS2           0x216
#define EE_AUDIO_TODDR_D_START_ADDRB       0x217
#define EE_AUDIO_TODDR_D_FINISH_ADDRB      0x218
#define EE_AUDIO_TODDR_D_INIT_ADDR         0x219
#define EE_AUDIO_TODDR_D_CTRL2             0x21a

#define EE_AUDIO_TODDR_D_CHNUM_ID0         0x330
#define EE_AUDIO_TODDR_D_CHNUM_ID1         0x331
#define EE_AUDIO_TODDR_D_CHNUM_ID2         0x332
#define EE_AUDIO_TODDR_D_CHNUM_ID3         0x333
#define EE_AUDIO_TODDR_D_CHNUM_ID4         0x334
#define EE_AUDIO_TODDR_D_CHNUM_ID5         0x335
#define EE_AUDIO_TODDR_D_CHNUM_ID6         0x336
#define EE_AUDIO_TODDR_D_CHNUM_ID7         0x337
#define EE_AUDIO_TODDR_D_CHSYNC_CTRL       0x33F

#define EE_AUDIO_TODDR_E_CHNUM_ID0                 0x0340
#define EE_AUDIO_TODDR_E_CHNUM_ID1                 0x0341
#define EE_AUDIO_TODDR_E_CHNUM_ID2                 0x0342
#define EE_AUDIO_TODDR_E_CHNUM_ID3                 0x0343
#define EE_AUDIO_TODDR_E_CHNUM_ID4                 0x0344
#define EE_AUDIO_TODDR_E_CHNUM_ID5                 0x0345
#define EE_AUDIO_TODDR_E_CHNUM_ID6                 0x0346
#define EE_AUDIO_TODDR_E_CHNUM_ID7                 0x0347
#define EE_AUDIO_TODDR_E_CHSYNC_CTRL               0x034f

#define EE_AUDIO_TODDR_E_CTRL0                     0x0240
#define EE_AUDIO_TODDR_E_CTRL1                     0x0241
#define EE_AUDIO_TODDR_E_START_ADDR                0x0242
#define EE_AUDIO_TODDR_E_FINISH_ADDR               0x0243
#define EE_AUDIO_TODDR_E_INT_ADDR                  0x0244
#define EE_AUDIO_TODDR_E_STATUS1                   0x0245
#define EE_AUDIO_TODDR_E_STATUS2                   0x0246
#define EE_AUDIO_TODDR_E_START_ADDRB               0x0247
#define EE_AUDIO_TODDR_E_FINISH_ADDRB              0x0248
#define EE_AUDIO_TODDR_E_INIT_ADDR                 0x0249
#define EE_AUDIO_TODDR_E_CTRL2                     0x024a

#define EE_AUDIO_FRDDR_E_CTRL0                     0x0250
#define EE_AUDIO_FRDDR_E_CTRL1                     0x0251
#define EE_AUDIO_FRDDR_E_START_ADDR                0x0252
#define EE_AUDIO_FRDDR_E_FINISH_ADDR               0x0253
#define EE_AUDIO_FRDDR_E_INT_ADDR                  0x0254
#define EE_AUDIO_FRDDR_E_STATUS1                   0x0255
#define EE_AUDIO_FRDDR_E_STATUS2                   0x0256
#define EE_AUDIO_FRDDR_E_START_ADDRB               0x0257
#define EE_AUDIO_FRDDR_E_FINISH_ADDRB              0x0258
#define EE_AUDIO_FRDDR_E_INIT_ADDR                 0x0259
#define EE_AUDIO_FRDDR_E_CTRL2                     0x025a

/*
 *	AUDIO FRDDR
 */
#define EE_AUDIO_FRDDR_A_CTRL0             0x070
#define EE_AUDIO_FRDDR_A_CTRL1             0x071
#define EE_AUDIO_FRDDR_A_START_ADDR        0x072
#define EE_AUDIO_FRDDR_A_FINISH_ADDR       0x073
#define EE_AUDIO_FRDDR_A_INT_ADDR          0x074
#define EE_AUDIO_FRDDR_A_STATUS1           0x075
#define EE_AUDIO_FRDDR_A_STATUS2           0x076
#define EE_AUDIO_FRDDR_A_START_ADDRB       0x077
#define EE_AUDIO_FRDDR_A_FINISH_ADDRB      0x078
#define EE_AUDIO_FRDDR_A_INIT_ADDR         0x079
#define EE_AUDIO_FRDDR_A_CTRL2             0x07a

#define EE_AUDIO_FRDDR_B_CTRL0             0x080
#define EE_AUDIO_FRDDR_B_CTRL1             0x081
#define EE_AUDIO_FRDDR_B_START_ADDR        0x082
#define EE_AUDIO_FRDDR_B_FINISH_ADDR       0x083
#define EE_AUDIO_FRDDR_B_INT_ADDR          0x084
#define EE_AUDIO_FRDDR_B_STATUS1           0x085
#define EE_AUDIO_FRDDR_B_STATUS2           0x086
#define EE_AUDIO_FRDDR_B_START_ADDRB       0x087
#define EE_AUDIO_FRDDR_B_FINISH_ADDRB      0x088
#define EE_AUDIO_FRDDR_B_INIT_ADDR         0x089
#define EE_AUDIO_FRDDR_B_CTRL2             0x08a

#define EE_AUDIO_FRDDR_C_CTRL0             0x090
#define EE_AUDIO_FRDDR_C_CTRL1             0x091
#define EE_AUDIO_FRDDR_C_START_ADDR        0x092
#define EE_AUDIO_FRDDR_C_FINISH_ADDR       0x093
#define EE_AUDIO_FRDDR_C_INT_ADDR          0x094
#define EE_AUDIO_FRDDR_C_STATUS1           0x095
#define EE_AUDIO_FRDDR_C_STATUS2           0x096
#define EE_AUDIO_FRDDR_C_START_ADDRB       0x097
#define EE_AUDIO_FRDDR_C_FINISH_ADDRB      0x098
#define EE_AUDIO_FRDDR_C_INIT_ADDR         0x099
#define EE_AUDIO_FRDDR_C_CTRL2             0x09a

#define EE_AUDIO_FRDDR_D_CTRL0             0x220
#define EE_AUDIO_FRDDR_D_CTRL1             0x221
#define EE_AUDIO_FRDDR_D_START_ADDR        0x222
#define EE_AUDIO_FRDDR_D_FINISH_ADDR       0x223
#define EE_AUDIO_FRDDR_D_INT_ADDR          0x224
#define EE_AUDIO_FRDDR_D_STATUS1           0x225
#define EE_AUDIO_FRDDR_D_STATUS2           0x226
#define EE_AUDIO_FRDDR_D_START_ADDRB       0x227
#define EE_AUDIO_FRDDR_D_FINISH_ADDRB      0x228
#define EE_AUDIO_FRDDR_D_INIT_ADDR         0x229
#define EE_AUDIO_FRDDR_D_CTRL2             0x22a

/*
 *	AUDIO ARB,
 */
#define EE_AUDIO_ARB_CTRL                  0x0a0
#define EE_AUDIO_ARB_CTRL1                 0x0a1
#define EE_AUDIO_ARB_STS                   0x0a8

/* for A5 AM2AXI  */
#define EE_AUDIO_AM2AXI_CTRL0              0x0a2
#define EE_AUDIO_AM2AXI_CTRL1              0x0a3
#define EE_AUDIO_AXI_ASYNC_CTRL0           0x0a4
#define EE_AUDIO_AM2AXI_STS                0x0a9
#define EE_AUDIO_AXI_ASYNC_STS             0x0aa

/*
 *	AUDIO LOOPBACK
 */
#define EE_AUDIO_LB_CTRL0                  0x0b0

#define EE_AUDIO_LB_A_CTRL0                0xb0
#define EE_AUDIO_LB_A_CTRL1                0xb1
#define EE_AUDIO_LB_A_CTRL2                0xb2
#define EE_AUDIO_LB_A_CTRL3                0xb3
#define EE_AUDIO_LB_A_DAT_CH_ID0           0xb4
#define EE_AUDIO_LB_A_DAT_CH_ID1           0xb5
#define EE_AUDIO_LB_A_DAT_CH_ID2           0xb6
#define EE_AUDIO_LB_A_DAT_CH_ID3           0xb7
#define EE_AUDIO_LB_A_LB_CH_ID0            0xb8
#define EE_AUDIO_LB_A_LB_CH_ID1            0xb9
#define EE_AUDIO_LB_A_LB_CH_ID2            0xba
#define EE_AUDIO_LB_A_LB_CH_ID3            0xbb
#define EE_AUDIO_LB_A_STS                  0xbc

#define EE_AUDIO_LB_B_CTRL0                0x230
#define EE_AUDIO_LB_B_CTRL1                0x231
#define EE_AUDIO_LB_B_CTRL2                0x232
#define EE_AUDIO_LB_B_CTRL3                0x233
#define EE_AUDIO_LB_B_DAT_CH_ID0           0x234
#define EE_AUDIO_LB_B_DAT_CH_ID1           0x235
#define EE_AUDIO_LB_B_DAT_CH_ID2           0x236
#define EE_AUDIO_LB_B_DAT_CH_ID3           0x237
#define EE_AUDIO_LB_B_LB_CH_ID0            0x238
#define EE_AUDIO_LB_B_LB_CH_ID1            0x239
#define EE_AUDIO_LB_B_LB_CH_ID2            0x23a
#define EE_AUDIO_LB_B_LB_CH_ID3            0x23b
#define EE_AUDIO_LB_B_STS                  0x23c

/*
 *	AUDIO TDM
 */
#define EE_AUDIO_TDMIN_A_CTRL              0x0c0
#define EE_AUDIO_TDMIN_A_SWAP0             0x0c1
#define EE_AUDIO_TDMIN_A_SWAP1             0x260
#define EE_AUDIO_TDMIN_A_MASK0             0x0c2
#define EE_AUDIO_TDMIN_A_MASK1             0x0c3
#define EE_AUDIO_TDMIN_A_MASK2             0x0c4
#define EE_AUDIO_TDMIN_A_MASK3             0x0c5
#define EE_AUDIO_TDMIN_A_MASK4             0x261
#define EE_AUDIO_TDMIN_A_MASK5             0x262
#define EE_AUDIO_TDMIN_A_MASK6             0x263
#define EE_AUDIO_TDMIN_A_MASK7             0x264
#define EE_AUDIO_TDMIN_A_STAT              0x0c6
#define EE_AUDIO_TDMIN_A_MUTE_VAL          0x0c7
#define EE_AUDIO_TDMIN_A_MUTE0             0x0c8
#define EE_AUDIO_TDMIN_A_MUTE1             0x0c9
#define EE_AUDIO_TDMIN_A_MUTE2             0x0ca
#define EE_AUDIO_TDMIN_A_MUTE3             0x0cb
#define EE_AUDIO_TDMIN_A_MUTE4             0x265
#define EE_AUDIO_TDMIN_A_MUTE5             0x266
#define EE_AUDIO_TDMIN_A_MUTE6             0x267
#define EE_AUDIO_TDMIN_A_MUTE7             0x268

#define EE_AUDIO_TDMIN_B_CTRL              0x0d0
#define EE_AUDIO_TDMIN_B_SWAP0             0x0d1
#define EE_AUDIO_TDMIN_B_SWAP1             0x270
#define EE_AUDIO_TDMIN_B_MASK0             0x0d2
#define EE_AUDIO_TDMIN_B_MASK1             0x0d3
#define EE_AUDIO_TDMIN_B_MASK2             0x0d4
#define EE_AUDIO_TDMIN_B_MASK3             0x0d5
#define EE_AUDIO_TDMIN_B_MASK4             0x271
#define EE_AUDIO_TDMIN_B_MASK5             0x272
#define EE_AUDIO_TDMIN_B_MASK6             0x273
#define EE_AUDIO_TDMIN_B_MASK7             0x274
#define EE_AUDIO_TDMIN_B_STAT              0x0d6
#define EE_AUDIO_TDMIN_B_MUTE_VAL          0x0d7
#define EE_AUDIO_TDMIN_B_MUTE0             0x0d8
#define EE_AUDIO_TDMIN_B_MUTE1             0x0d9
#define EE_AUDIO_TDMIN_B_MUTE2             0x0da
#define EE_AUDIO_TDMIN_B_MUTE3             0x0db
#define EE_AUDIO_TDMIN_B_MUTE4             0x275
#define EE_AUDIO_TDMIN_B_MUTE5             0x276
#define EE_AUDIO_TDMIN_B_MUTE6             0x277
#define EE_AUDIO_TDMIN_B_MUTE7             0x278

#define EE_AUDIO_TDMIN_C_CTRL              0x0e0
#define EE_AUDIO_TDMIN_C_SWAP0             0x0e1
#define EE_AUDIO_TDMIN_C_SWAP1             0x280
#define EE_AUDIO_TDMIN_C_SWAP              0x0e1
#define EE_AUDIO_TDMIN_C_MASK0             0x0e2
#define EE_AUDIO_TDMIN_C_MASK1             0x0e3
#define EE_AUDIO_TDMIN_C_MASK2             0x0e4
#define EE_AUDIO_TDMIN_C_MASK3             0x0e5
#define EE_AUDIO_TDMIN_C_MASK4             0x281
#define EE_AUDIO_TDMIN_C_MASK5             0x282
#define EE_AUDIO_TDMIN_C_MASK6             0x283
#define EE_AUDIO_TDMIN_C_MASK7             0x284
#define EE_AUDIO_TDMIN_C_STAT              0x0e6
#define EE_AUDIO_TDMIN_C_MUTE_VAL          0x0e7
#define EE_AUDIO_TDMIN_C_MUTE0             0x0e8
#define EE_AUDIO_TDMIN_C_MUTE1             0x0e9
#define EE_AUDIO_TDMIN_C_MUTE2             0x0ea
#define EE_AUDIO_TDMIN_C_MUTE3             0x0eb
#define EE_AUDIO_TDMIN_C_MUTE4             0x285
#define EE_AUDIO_TDMIN_C_MUTE5             0x286
#define EE_AUDIO_TDMIN_C_MUTE6             0x287
#define EE_AUDIO_TDMIN_C_MUTE7             0x288

#define EE_AUDIO_TDMIN_D_CTRL              0x3b0
#define EE_AUDIO_TDMIN_D_SWAP0             0x3b1
#define EE_AUDIO_TDMIN_D_SWAP1             0x3c0
#define EE_AUDIO_TDMIN_D_MASK0             0x3b2
#define EE_AUDIO_TDMIN_D_MASK1             0x3b3
#define EE_AUDIO_TDMIN_D_MASK2             0x3b4
#define EE_AUDIO_TDMIN_D_MASK3             0x3b5
#define EE_AUDIO_TDMIN_D_MASK4             0x3c1
#define EE_AUDIO_TDMIN_D_MASK5             0x3c2
#define EE_AUDIO_TDMIN_D_MASK6             0x3c3
#define EE_AUDIO_TDMIN_D_MASK7             0x3c4
#define EE_AUDIO_TDMIN_D_STAT              0x3b6
#define EE_AUDIO_TDMIN_D_MUTE_VAL          0x3b7
#define EE_AUDIO_TDMIN_D_MUTE0             0x3b8
#define EE_AUDIO_TDMIN_D_MUTE1             0x3b9
#define EE_AUDIO_TDMIN_D_MUTE2             0x3ba
#define EE_AUDIO_TDMIN_D_MUTE3             0x3bb
#define EE_AUDIO_TDMIN_D_MUTE4             0x3c5
#define EE_AUDIO_TDMIN_D_MUTE5             0x3c6
#define EE_AUDIO_TDMIN_D_MUTE6             0x3c7
#define EE_AUDIO_TDMIN_D_MUTE7             0x3c8

#define EE_AUDIO_TDMIN_LB_CTRL             0x0f0
#define EE_AUDIO_TDMIN_LB_SWAP0            0x0f1
#define EE_AUDIO_TDMIN_LB_SWAP1            0x290
#define EE_AUDIO_TDMIN_LB_MASK0            0x0f2
#define EE_AUDIO_TDMIN_LB_MASK1            0x0f3
#define EE_AUDIO_TDMIN_LB_MASK2            0x0f4
#define EE_AUDIO_TDMIN_LB_MASK3            0x0f5
#define EE_AUDIO_TDMIN_LB_MASK4            0x291
#define EE_AUDIO_TDMIN_LB_MASK5            0x292
#define EE_AUDIO_TDMIN_LB_MASK6            0x293
#define EE_AUDIO_TDMIN_LB_MASK7            0x294
#define EE_AUDIO_TDMIN_LB_STAT             0x0f6
#define EE_AUDIO_TDMIN_LB_MUTE_VAL         0x0f7
#define EE_AUDIO_TDMIN_LB_MUTE0            0x0f8
#define EE_AUDIO_TDMIN_LB_MUTE1            0x0f9
#define EE_AUDIO_TDMIN_LB_MUTE2            0x0fa
#define EE_AUDIO_TDMIN_LB_MUTE3            0x0fb
#define EE_AUDIO_TDMIN_LB_MUTE4            0x295
#define EE_AUDIO_TDMIN_LB_MUTE5            0x296
#define EE_AUDIO_TDMIN_LB_MUTE6            0x297
#define EE_AUDIO_TDMIN_LB_MUTE7            0x298

#define EE_AUDIO_TDMINB_LB_CTRL             0x170
#define EE_AUDIO_TDMINB_LB_SWAP0            0x171
#define EE_AUDIO_TDMINB_LB_SWAP1            0x2d0
#define EE_AUDIO_TDMINB_LB_MASK0            0x172
#define EE_AUDIO_TDMINB_LB_MASK1            0x173
#define EE_AUDIO_TDMINB_LB_MASK2            0x174
#define EE_AUDIO_TDMINB_LB_MASK3            0x175
#define EE_AUDIO_TDMINB_LB_MASK4            0x2d1
#define EE_AUDIO_TDMINB_LB_MASK5            0x2d2
#define EE_AUDIO_TDMINB_LB_MASK6            0x2d3
#define EE_AUDIO_TDMINB_LB_MASK7            0x2d4
#define EE_AUDIO_TDMINB_LB_STAT             0x176
#define EE_AUDIO_TDMINB_LB_MUTE_VAL         0x177
#define EE_AUDIO_TDMINB_LB_MUTE0            0x178
#define EE_AUDIO_TDMINB_LB_MUTE1            0x179
#define EE_AUDIO_TDMINB_LB_MUTE2            0x17a
#define EE_AUDIO_TDMINB_LB_MUTE3            0x17b
#define EE_AUDIO_TDMINB_LB_MUTE4            0x2d5
#define EE_AUDIO_TDMINB_LB_MUTE5            0x2d6
#define EE_AUDIO_TDMINB_LB_MUTE6            0x2d7
#define EE_AUDIO_TDMINB_LB_MUTE7            0x2d8
/*
 *	AUDIO OUTPUT
 */
#define EE_AUDIO_SPDIFIN_CTRL0             0x100
#define EE_AUDIO_SPDIFIN_CTRL1             0x101
#define EE_AUDIO_SPDIFIN_CTRL2             0x102
#define EE_AUDIO_SPDIFIN_CTRL3             0x103
#define EE_AUDIO_SPDIFIN_CTRL4             0x104
#define EE_AUDIO_SPDIFIN_CTRL5             0x105
#define EE_AUDIO_SPDIFIN_CTRL6             0x106
#define EE_AUDIO_SPDIFIN_STAT0             0x107
#define EE_AUDIO_SPDIFIN_STAT1             0x108
#define EE_AUDIO_SPDIFIN_STAT2             0x109
#define EE_AUDIO_SPDIFIN_MUTE_VAL          0x10a
#define EE_AUDIO_SPDIFIN_CTRL7             0x10b

#define EE_AUDIO_RESAMPLEA_CTRL0           0x110
#define EE_AUDIO_RESAMPLEA_CTRL1           0x111
#define EE_AUDIO_RESAMPLEA_CTRL2           0x112
#define EE_AUDIO_RESAMPLEA_CTRL3           0x113
#define EE_AUDIO_RESAMPLEA_COEF0           0x114
#define EE_AUDIO_RESAMPLEA_COEF1           0x115
#define EE_AUDIO_RESAMPLEA_COEF2           0x116
#define EE_AUDIO_RESAMPLEA_COEF3           0x117
#define EE_AUDIO_RESAMPLEA_COEF4           0x118
#define EE_AUDIO_RESAMPLEA_STATUS1         0x119

#define EE_AUDIO_RESAMPLEB_CTRL0           0x1e0
#define EE_AUDIO_RESAMPLEB_CTRL1           0x1e1
#define EE_AUDIO_RESAMPLEB_CTRL2           0x1e2
#define EE_AUDIO_RESAMPLEB_CTRL3           0x1e3
#define EE_AUDIO_RESAMPLEB_COEF0           0x1e4
#define EE_AUDIO_RESAMPLEB_COEF1           0x1e5
#define EE_AUDIO_RESAMPLEB_COEF2           0x1e6
#define EE_AUDIO_RESAMPLEB_COEF3           0x1e7
#define EE_AUDIO_RESAMPLEB_COEF4           0x1e8
#define EE_AUDIO_RESAMPLEB_STATUS1         0x1e9

#define EE_AUDIO_SPDIFOUT_STAT             0x120
#define EE_AUDIO_SPDIFOUT_GAIN0            0x121
#define EE_AUDIO_SPDIFOUT_GAIN1            0x122
#define EE_AUDIO_SPDIFOUT_CTRL0            0x123
#define EE_AUDIO_SPDIFOUT_CTRL1            0x124
#define EE_AUDIO_SPDIFOUT_PREAMB           0x125
#define EE_AUDIO_SPDIFOUT_SWAP             0x126
#define EE_AUDIO_SPDIFOUT_CHSTS0           0x127
#define EE_AUDIO_SPDIFOUT_CHSTS1           0x128
#define EE_AUDIO_SPDIFOUT_CHSTS2           0x129
#define EE_AUDIO_SPDIFOUT_CHSTS3           0x12a
#define EE_AUDIO_SPDIFOUT_CHSTS4           0x12b
#define EE_AUDIO_SPDIFOUT_CHSTS5           0x12c
#define EE_AUDIO_SPDIFOUT_CHSTS6           0x12d
#define EE_AUDIO_SPDIFOUT_CHSTS7           0x12e
#define EE_AUDIO_SPDIFOUT_CHSTS8           0x12f
#define EE_AUDIO_SPDIFOUT_CHSTS9           0x130
#define EE_AUDIO_SPDIFOUT_CHSTSA           0x131
#define EE_AUDIO_SPDIFOUT_CHSTSB           0x132
#define EE_AUDIO_SPDIFOUT_MUTE_VAL         0x133
#define EE_AUDIO_SPDIFOUT_GAIN2            0x134
#define EE_AUDIO_SPDIFOUT_GAIN3            0x135
#define EE_AUDIO_SPDIFOUT_GAIN_EN          0x136
#define EE_AUDIO_SPDIFOUT_GAIN_CTRL        0x137

#define EE_AUDIO_TDMOUT_A_CTRL0            0x140
#define EE_AUDIO_TDMOUT_A_CTRL1            0x141
#define EE_AUDIO_TDMOUT_A_CTRL2            0x2a0
#define EE_AUDIO_TDMOUT_A_SWAP0            0x142
#define EE_AUDIO_TDMOUT_A_SWAP1            0x2a1
#define EE_AUDIO_TDMOUT_A_MASK0            0x143
#define EE_AUDIO_TDMOUT_A_MASK1            0x144
#define EE_AUDIO_TDMOUT_A_MASK2            0x145
#define EE_AUDIO_TDMOUT_A_MASK3            0x146
#define EE_AUDIO_TDMOUT_A_MASK4            0x2a4
#define EE_AUDIO_TDMOUT_A_MASK5            0x2a5
#define EE_AUDIO_TDMOUT_A_MASK6            0x2a6
#define EE_AUDIO_TDMOUT_A_MASK7            0x2a7
#define EE_AUDIO_TDMOUT_A_STAT             0x147
#define EE_AUDIO_TDMOUT_A_GAIN0            0x148
#define EE_AUDIO_TDMOUT_A_GAIN1            0x149
#define EE_AUDIO_TDMOUT_A_GAIN2            0x2a2
#define EE_AUDIO_TDMOUT_A_GAIN3            0x2a3
#define EE_AUDIO_TDMOUT_A_MUTE_VAL         0x14a
#define EE_AUDIO_TDMOUT_A_MUTE0            0x14b
#define EE_AUDIO_TDMOUT_A_MUTE1            0x14c
#define EE_AUDIO_TDMOUT_A_MUTE2            0x14d
#define EE_AUDIO_TDMOUT_A_MUTE3            0x14e
#define EE_AUDIO_TDMOUT_A_MUTE4            0x2a8
#define EE_AUDIO_TDMOUT_A_MUTE5            0x2a9
#define EE_AUDIO_TDMOUT_A_MUTE6            0x2aa
#define EE_AUDIO_TDMOUT_A_MUTE7            0x2ab
#define EE_AUDIO_TDMOUT_A_GAIN_EN          0x2ac
#define EE_AUDIO_TDMOUT_A_GAIN_CTRL        0x2ad
#define EE_AUDIO_TDMOUT_A_MASK_VAL         0x14f

#define EE_AUDIO_TDMOUT_B_CTRL0            0x150
#define EE_AUDIO_TDMOUT_B_CTRL1            0x151
#define EE_AUDIO_TDMOUT_B_CTRL2            0x2b0
#define EE_AUDIO_TDMOUT_B_SWAP0            0x152
#define EE_AUDIO_TDMOUT_B_SWAP1            0x2b1
#define EE_AUDIO_TDMOUT_B_MASK0            0x153
#define EE_AUDIO_TDMOUT_B_MASK1            0x154
#define EE_AUDIO_TDMOUT_B_MASK2            0x155
#define EE_AUDIO_TDMOUT_B_MASK3            0x156
#define EE_AUDIO_TDMOUT_B_MASK4            0x2b4
#define EE_AUDIO_TDMOUT_B_MASK5            0x2b5
#define EE_AUDIO_TDMOUT_B_MASK6            0x2b6
#define EE_AUDIO_TDMOUT_B_MASK7            0x2b7
#define EE_AUDIO_TDMOUT_B_STAT             0x157
#define EE_AUDIO_TDMOUT_B_GAIN0            0x158
#define EE_AUDIO_TDMOUT_B_GAIN1            0x159
#define EE_AUDIO_TDMOUT_B_GAIN2            0x2b2
#define EE_AUDIO_TDMOUT_B_GAIN3            0x2b3
#define EE_AUDIO_TDMOUT_B_MUTE_VAL         0x15a
#define EE_AUDIO_TDMOUT_B_MUTE0            0x15b
#define EE_AUDIO_TDMOUT_B_MUTE1            0x15c
#define EE_AUDIO_TDMOUT_B_MUTE2            0x15d
#define EE_AUDIO_TDMOUT_B_MUTE3            0x15e
#define EE_AUDIO_TDMOUT_B_MUTE4            0x2b8
#define EE_AUDIO_TDMOUT_B_MUTE5            0x2b9
#define EE_AUDIO_TDMOUT_B_MUTE6            0x2ba
#define EE_AUDIO_TDMOUT_B_MUTE7            0x2bb
#define EE_AUDIO_TDMOUT_B_GAIN_EN          0x2bc
#define EE_AUDIO_TDMOUT_B_GAIN_CTRL        0x2bd
#define EE_AUDIO_TDMOUT_B_MASK_VAL         0x15f

#define EE_AUDIO_TDMOUT_C_CTRL0            0x160
#define EE_AUDIO_TDMOUT_C_CTRL1            0x161
#define EE_AUDIO_TDMOUT_C_CTRL2            0x2c0
#define EE_AUDIO_TDMOUT_C_SWAP0            0x162
#define EE_AUDIO_TDMOUT_C_SWAP1            0x2c1
#define EE_AUDIO_TDMOUT_C_MASK0            0x163
#define EE_AUDIO_TDMOUT_C_MASK1            0x164
#define EE_AUDIO_TDMOUT_C_MASK2            0x165
#define EE_AUDIO_TDMOUT_C_MASK3            0x166
#define EE_AUDIO_TDMOUT_C_MASK4            0x2c4
#define EE_AUDIO_TDMOUT_C_MASK5            0x2c5
#define EE_AUDIO_TDMOUT_C_MASK6            0x2c6
#define EE_AUDIO_TDMOUT_C_MASK7            0x2c7
#define EE_AUDIO_TDMOUT_C_STAT             0x167
#define EE_AUDIO_TDMOUT_C_GAIN0            0x168
#define EE_AUDIO_TDMOUT_C_GAIN1            0x169
#define EE_AUDIO_TDMOUT_C_GAIN2            0x2c2
#define EE_AUDIO_TDMOUT_C_GAIN3            0x2c3
#define EE_AUDIO_TDMOUT_C_MUTE_VAL         0x16a
#define EE_AUDIO_TDMOUT_C_MUTE0            0x16b
#define EE_AUDIO_TDMOUT_C_MUTE1            0x16c
#define EE_AUDIO_TDMOUT_C_MUTE2            0x16d
#define EE_AUDIO_TDMOUT_C_MUTE3            0x16e
#define EE_AUDIO_TDMOUT_C_MUTE4            0x2c8
#define EE_AUDIO_TDMOUT_C_MUTE5            0x2c9
#define EE_AUDIO_TDMOUT_C_MUTE6            0x2ca
#define EE_AUDIO_TDMOUT_C_MUTE7            0x2cb
#define EE_AUDIO_TDMOUT_C_GAIN_EN          0x2cc
#define EE_AUDIO_TDMOUT_C_GAIN_CTRL        0x2cd
#define EE_AUDIO_TDMOUT_C_MASK_VAL         0x16f

#define EE_AUDIO_TDMOUT_D_CTRL0            0x3d0
#define EE_AUDIO_TDMOUT_D_CTRL1            0x3d1
#define EE_AUDIO_TDMOUT_D_CTRL2            0x3e0
#define EE_AUDIO_TDMOUT_D_SWAP0            0x3d2
#define EE_AUDIO_TDMOUT_D_SWAP1            0x3e1
#define EE_AUDIO_TDMOUT_D_MASK0            0x3d3
#define EE_AUDIO_TDMOUT_D_MASK1            0x3d4
#define EE_AUDIO_TDMOUT_D_MASK2            0x3d5
#define EE_AUDIO_TDMOUT_D_MASK3            0x3d6
#define EE_AUDIO_TDMOUT_D_MASK4            0x3e4
#define EE_AUDIO_TDMOUT_D_MASK5            0x3e5
#define EE_AUDIO_TDMOUT_D_MASK6            0x3e6
#define EE_AUDIO_TDMOUT_D_MASK7            0x3e7
#define EE_AUDIO_TDMOUT_D_STAT             0x3d7
#define EE_AUDIO_TDMOUT_D_GAIN0            0x3d8
#define EE_AUDIO_TDMOUT_D_GAIN1            0x3d9
#define EE_AUDIO_TDMOUT_D_GAIN2            0x3e2
#define EE_AUDIO_TDMOUT_D_GAIN3            0x3e3
#define EE_AUDIO_TDMOUT_D_MUTE_VAL         0x3da
#define EE_AUDIO_TDMOUT_D_MUTE0            0x3db
#define EE_AUDIO_TDMOUT_D_MUTE1            0x3dc
#define EE_AUDIO_TDMOUT_D_MUTE2            0x3dd
#define EE_AUDIO_TDMOUT_D_MUTE3            0x3de
#define EE_AUDIO_TDMOUT_D_MUTE4            0x3e8
#define EE_AUDIO_TDMOUT_D_MUTE5            0x3e9
#define EE_AUDIO_TDMOUT_D_MUTE6            0x3ea
#define EE_AUDIO_TDMOUT_D_MUTE7            0x3eb
#define EE_AUDIO_TDMOUT_D_GAIN_EN          0x3ec
#define EE_AUDIO_TDMOUT_D_GAIN_CTRL        0x3ed
#define EE_AUDIO_TDMOUT_D_MASK_VAL         0x3df
/*
 *	AUDIO POWER DETECT
 */
#define EE_AUDIO_POW_DET_CTRL0             0x180
#define EE_AUDIO_POW_DET_TH_HI             0x181
#define EE_AUDIO_POW_DET_TH_LO             0x182
#define EE_AUDIO_POW_DET_VALUE             0x183
#define EE_AUDIO_SECURITY_CTRL             0x193

/*
 *	AUDIO SPDIF_B
 */
#define EE_AUDIO_SPDIFOUT_B_STAT           0x1a0
#define EE_AUDIO_SPDIFOUT_B_GAIN0          0x1a1
#define EE_AUDIO_SPDIFOUT_B_GAIN1          0x1a2
#define EE_AUDIO_SPDIFOUT_B_CTRL0          0x1a3
#define EE_AUDIO_SPDIFOUT_B_CTRL1          0x1a4
#define EE_AUDIO_SPDIFOUT_B_PREAMB         0x1a5
#define EE_AUDIO_SPDIFOUT_B_SWAP           0x1a6
#define EE_AUDIO_SPDIFOUT_B_CHSTS0         0x1a7
#define EE_AUDIO_SPDIFOUT_B_CHSTS1         0x1a8
#define EE_AUDIO_SPDIFOUT_B_CHSTS2         0x1a9
#define EE_AUDIO_SPDIFOUT_B_CHSTS3         0x1aa
#define EE_AUDIO_SPDIFOUT_B_CHSTS4         0x1ab
#define EE_AUDIO_SPDIFOUT_B_CHSTS5         0x1ac
#define EE_AUDIO_SPDIFOUT_B_CHSTS6         0x1ad
#define EE_AUDIO_SPDIFOUT_B_CHSTS7         0x1ae
#define EE_AUDIO_SPDIFOUT_B_CHSTS8         0x1af
#define EE_AUDIO_SPDIFOUT_B_CHSTS9         0x1b0
#define EE_AUDIO_SPDIFOUT_B_CHSTSA         0x1b1
#define EE_AUDIO_SPDIFOUT_B_CHSTSB         0x1b2
#define EE_AUDIO_SPDIFOUT_B_MUTE_VAL       0x1b3

/*
 *	AUDIO LOCKER
 */
#define EE_AUDIO_TORAM_CTRL0               0x1c0
#define EE_AUDIO_TORAM_CTRL1               0x1c1
#define EE_AUDIO_TORAM_START_ADDR          0x1c2
#define EE_AUDIO_TORAM_FINISH_ADDR         0x1c3
#define EE_AUDIO_TORAM_INT_ADDR            0x1c4
#define EE_AUDIO_TORAM_STATUS1             0x1c5
#define EE_AUDIO_TORAM_STATUS2             0x1c6
#define EE_AUDIO_TORAM_INIT_ADDR           0x1c7

/*
 *	HIU, AUDIO CODEC RESET
 */
#define EE_RESET1                          0x002

/*
 *	HIU, ARC
 */
#define HHI_HDMIRX_PHY_MISC2               0x0e0
#define HHI_HDMIRX_ARC_CNTL                0x0e8
#define HHI_HDMIRX_EARCTX_CNTL0            0x069
#define HHI_HDMIRX_EARCTX_CNTL1            0x06a

/*
 *	AUDIO MUX CONTROLS
 */
#define EE_AUDIO_TOACODEC_CTRL0            0x1d0
#define EE_AUDIO_TOHDMITX_CTRL0            0x1d1
#define EE_AUDIO_TOVAD_CTRL0               0x1d2
#define EE_AUDIO_FRATV_CTRL0               0x1d3

#define EE_AUDIO_SPDIFIN_LB_CTRL0          0x1f0
#define EE_AUDIO_SPDIFIN_LB_CTRL1          0x1f1
#define EE_AUDIO_SPDIFIN_LB_CTRL6          0x1f6
#define EE_AUDIO_SPDIFIN_LB_STAT0          0x1f7
#define EE_AUDIO_SPDIFIN_LB_STAT1          0x1f8
#define EE_AUDIO_SPDIFIN_LB_MUTE_VAL       0x1fa

#define EE_AUDIO_FRHDMIRX_CTRL0            0x200
#define EE_AUDIO_FRHDMIRX_CTRL1            0x201
#define EE_AUDIO_FRHDMIRX_CTRL2            0x202
#define EE_AUDIO_FRHDMIRX_CTRL3            0x203
#define EE_AUDIO_FRHDMIRX_CTRL4            0x204
#define EE_AUDIO_FRHDMIRX_CTRL5            0x205
#define EE_AUDIO_FRHDMIRX_CTRL6            0x206
#define EE_AUDIO_FRHDMIRX_STAT0            0x20a
#define EE_AUDIO_FRHDMIRX_STAT1            0x20b

/*
 *	AUDIO LOCKER
 */
#define AUD_LOCK_EN                        0x000
#define AUD_LOCK_SW_RESET                  0x001
#define AUD_LOCK_SW_LATCH                  0x002
#define AUD_LOCK_HW_LATCH                  0x003
#define AUD_LOCK_REFCLK_SRC                0x004
#define AUD_LOCK_REFCLK_LAT_INT            0x005
#define AUD_LOCK_IMCLK_LAT_INT             0x006
#define AUD_LOCK_OMCLK_LAT_INT             0x007
#define AUD_LOCK_REFCLK_DS_INT             0x008
#define AUD_LOCK_IMCLK_DS_INT              0x009
#define AUD_LOCK_OMCLK_DS_INT              0x00a
#define AUD_LOCK_INT_CLR                   0x00b
#define AUD_LOCK_GCLK_CTRL                 0x00c
#define AUD_LOCK_INT_CTRL                  0x00d
#define RO_REF2IMCLK_CNT_L                 0x010
#define RO_REF2IMCLK_CNT_H                 0x011
#define RO_REF2OMCLK_CNT_L                 0x012
#define RO_REF2OMCLK_CNT_H                 0x013
#define RO_IMCLK2REF_CNT_L                 0x014
#define RO_IMCLK2REF_CNT_H                 0x015
#define RO_OMCLK2REF_CNT_L                 0x016
#define RO_OMCLK2REF_CNT_H                 0x017
#define RO_REFCLK_PKG_CNT                  0x018
#define RO_IMCLK_PKG_CNT                   0x019
#define RO_OMCLK_PKG_CNT                   0x01a
#define RO_AUD_LOCK_INT_STATUS             0x01b

/*
 * EQ DRC, G12X means g12a, g12b
 */
#define AED_EQ_CH1_COEF00                  0x00
#define AED_EQ_CH1_COEF01                  0x01
#define AED_EQ_CH1_COEF02                  0x02
#define AED_EQ_CH1_COEF03                  0x03
#define AED_EQ_CH1_COEF04                  0x04
#define AED_EQ_CH1_COEF10                  0x05
#define AED_EQ_CH1_COEF11                  0x06
#define AED_EQ_CH1_COEF12                  0x07
#define AED_EQ_CH1_COEF13                  0x08
#define AED_EQ_CH1_COEF14                  0x09
#define AED_EQ_CH1_COEF20                  0x0a
#define AED_EQ_CH1_COEF21                  0x0b
#define AED_EQ_CH1_COEF22                  0x0c
#define AED_EQ_CH1_COEF23                  0x0d
#define AED_EQ_CH1_COEF24                  0x0e
#define AED_EQ_CH1_COEF30                  0x0f
#define AED_EQ_CH1_COEF31                  0x10
#define AED_EQ_CH1_COEF32                  0x11
#define AED_EQ_CH1_COEF33                  0x12
#define AED_EQ_CH1_COEF34                  0x13
#define AED_EQ_CH1_COEF40                  0x14
#define AED_EQ_CH1_COEF41                  0x15
#define AED_EQ_CH1_COEF42                  0x16
#define AED_EQ_CH1_COEF43                  0x17
#define AED_EQ_CH1_COEF44                  0x18
#define AED_EQ_CH1_COEF50                  0x19
#define AED_EQ_CH1_COEF51                  0x1a
#define AED_EQ_CH1_COEF52                  0x1b
#define AED_EQ_CH1_COEF53                  0x1c
#define AED_EQ_CH1_COEF54                  0x1d
#define AED_EQ_CH1_COEF60                  0x1e
#define AED_EQ_CH1_COEF61                  0x1f
#define AED_EQ_CH1_COEF62                  0x20
#define AED_EQ_CH1_COEF63                  0x21
#define AED_EQ_CH1_COEF64                  0x22
#define AED_EQ_CH1_COEF70                  0x23
#define AED_EQ_CH1_COEF71                  0x24
#define AED_EQ_CH1_COEF72                  0x25
#define AED_EQ_CH1_COEF73                  0x26
#define AED_EQ_CH1_COEF74                  0x27
#define AED_EQ_CH1_COEF80                  0x28
#define AED_EQ_CH1_COEF81                  0x29
#define AED_EQ_CH1_COEF82                  0x2a
#define AED_EQ_CH1_COEF83                  0x2b
#define AED_EQ_CH1_COEF84                  0x2c
#define AED_EQ_CH1_COEF90                  0x2d
#define AED_EQ_CH1_COEF91                  0x2e
#define AED_EQ_CH1_COEF92                  0x2f
#define AED_EQ_CH1_COEF93                  0x30
#define AED_EQ_CH1_COEF94                  0x31
#define AED_EQ_CH2_COEF00                  0x32
#define AED_EQ_CH2_COEF01                  0x33
#define AED_EQ_CH2_COEF02                  0x34
#define AED_EQ_CH2_COEF03                  0x35
#define AED_EQ_CH2_COEF04                  0x36
#define AED_EQ_CH2_COEF10                  0x37
#define AED_EQ_CH2_COEF11                  0x38
#define AED_EQ_CH2_COEF12                  0x39
#define AED_EQ_CH2_COEF13                  0x3a
#define AED_EQ_CH2_COEF14                  0x3b
#define AED_EQ_CH2_COEF20                  0x3c
#define AED_EQ_CH2_COEF21                  0x3d
#define AED_EQ_CH2_COEF22                  0x3e
#define AED_EQ_CH2_COEF23                  0x3f
#define AED_EQ_CH2_COEF24                  0x40
#define AED_EQ_CH2_COEF30                  0x41
#define AED_EQ_CH2_COEF31                  0x42
#define AED_EQ_CH2_COEF32                  0x43
#define AED_EQ_CH2_COEF33                  0x44
#define AED_EQ_CH2_COEF34                  0x45
#define AED_EQ_CH2_COEF40                  0x46
#define AED_EQ_CH2_COEF41                  0x47
#define AED_EQ_CH2_COEF42                  0x48
#define AED_EQ_CH2_COEF43                  0x49
#define AED_EQ_CH2_COEF44                  0x4a
#define AED_EQ_CH2_COEF50                  0x4b
#define AED_EQ_CH2_COEF51                  0x4c
#define AED_EQ_CH2_COEF52                  0x4d
#define AED_EQ_CH2_COEF53                  0x4e
#define AED_EQ_CH2_COEF54                  0x4f
#define AED_EQ_CH2_COEF60                  0x50
#define AED_EQ_CH2_COEF61                  0x51
#define AED_EQ_CH2_COEF62                  0x52
#define AED_EQ_CH2_COEF63                  0x53
#define AED_EQ_CH2_COEF64                  0x54
#define AED_EQ_CH2_COEF70                  0x55
#define AED_EQ_CH2_COEF71                  0x56
#define AED_EQ_CH2_COEF72                  0x57
#define AED_EQ_CH2_COEF73                  0x58
#define AED_EQ_CH2_COEF74                  0x59
#define AED_EQ_CH2_COEF80                  0x5a
#define AED_EQ_CH2_COEF81                  0x5b
#define AED_EQ_CH2_COEF82                  0x5c
#define AED_EQ_CH2_COEF83                  0x5d
#define AED_EQ_CH2_COEF84                  0x5e
#define AED_EQ_CH2_COEF90                  0x5f
#define AED_EQ_CH2_COEF91                  0x60
#define AED_EQ_CH2_COEF92                  0x61
#define AED_EQ_CH2_COEF93                  0x62
#define AED_EQ_CH2_COEF94                  0x63
#define AED_EQ_EN_G12X                     0x64
#define AED_EQ_VOLUME_G12X                 0x65
#define AED_EQ_VOLUME_SLEW_CNT_G12X        0x66
#define AED_MUTE_G12X                      0x67
#define AED_DRC_EN                         0x68
#define AED_DRC_AE                         0x69
#define AED_DRC_AA                         0x6a
#define AED_DRC_AD                         0x6b
#define AED_DRC_AE_1M                      0x6c
#define AED_DRC_AA_1M                      0x6d
#define AED_DRC_AD_1M                      0x6e
#define AED_DRC_OFFSET0                    0x6f
#define AED_DRC_OFFSET1                    0x70
#define AED_DRC_THD0_G12X                  0x71
#define AED_DRC_THD1_G12X                  0x72
#define AED_DRC_K0_G12X                    0x73
#define AED_DRC_K1_G12X                    0x74
#define AED_CLIP_THD_G12X                  0x75
#define AED_NG_THD0                        0x76
#define AED_NG_THD1                        0x77
#define AED_NG_CNT_THD                     0x78
#define AED_NG_CTL                         0x79
#define AED_ED_CTL                         0x7a
#define AED_DEBUG0                         0x7b
#define AED_DEBUG1                         0x7c
#define AED_DEBUG2                         0x7d
#define AED_DEBUG3                         0x7e
#define AED_DEBUG4                         0x7f
#define AED_DEBUG5                         0x80
#define AED_DEBUG6                         0x81
#define AED_DRC_AA_H                       0x82
#define AED_DRC_AD_H                       0x83
#define AED_DRC_AA_1M_H                    0x84
#define AED_DRC_AD_1M_H                    0x85
#define AED_NG_CNT                         0x86
#define AED_NG_STEP                        0x87

#define AED_TOP_CTL_G12X                   0x88
#define AED_TOP_REQ_CTL_G12X               0x89

/*
 * EQ DRC, New ARCH, from tl1
 */
#define AED_COEF_RAM_CNTL                  0x00
#define AED_COEF_RAM_DATA                  0x01
#define AED_EQ_EN                          0x02
#define AED_EQ_TAP_CNTL                    0x03
#define AED_EQ_VOLUME                      0x04
#define AED_EQ_VOLUME_SLEW_CNT             0x05
#define AED_MUTE                           0x06
#define AED_DRC_CNTL                       0x07
#define AED_DRC_RMS_COEF0                  0x08
#define AED_DRC_RMS_COEF1                  0x09
#define AED_DRC_THD0                       0x0a
#define AED_DRC_THD1                       0x0b
#define AED_DRC_THD2                       0x0c
#define AED_DRC_THD3                       0x0d
#define AED_DRC_THD4                       0x0e
#define AED_DRC_K0                         0x0f
#define AED_DRC_K1                         0x10
#define AED_DRC_K2                         0x11
#define AED_DRC_K3                         0x12
#define AED_DRC_K4                         0x13
#define AED_DRC_K5                         0x14
#define AED_DRC_THD_OUT0                   0x15
#define AED_DRC_THD_OUT1                   0x16
#define AED_DRC_THD_OUT2                   0x17
#define AED_DRC_THD_OUT3                   0x18
#define AED_DRC_OFFSET                     0x19
#define AED_DRC_RELEASE_COEF00             0x1a
#define AED_DRC_RELEASE_COEF01             0x1b
#define AED_DRC_RELEASE_COEF10             0x1c
#define AED_DRC_RELEASE_COEF11             0x1d
#define AED_DRC_RELEASE_COEF20             0x1e
#define AED_DRC_RELEASE_COEF21             0x1f
#define AED_DRC_RELEASE_COEF30             0x20
#define AED_DRC_RELEASE_COEF31             0x21
#define AED_DRC_RELEASE_COEF40             0x22
#define AED_DRC_RELEASE_COEF41             0x23
#define AED_DRC_RELEASE_COEF50             0x24
#define AED_DRC_RELEASE_COEF51             0x25
#define AED_DRC_ATTACK_COEF00              0x26
#define AED_DRC_ATTACK_COEF01              0x27
#define AED_DRC_ATTACK_COEF10              0x28
#define AED_DRC_ATTACK_COEF11              0x29
#define AED_DRC_ATTACK_COEF20              0x2a
#define AED_DRC_ATTACK_COEF21              0x2b
#define AED_DRC_ATTACK_COEF30              0x2c
#define AED_DRC_ATTACK_COEF31              0x2d
#define AED_DRC_ATTACK_COEF40              0x2e
#define AED_DRC_ATTACK_COEF41              0x2f
#define AED_DRC_ATTACK_COEF50              0x30
#define AED_DRC_ATTACK_COEF51              0x31
#define AED_DRC_LOOPBACK_CNTL              0x32
#define AED_MDRC_CNTL                      0x33
#define AED_MDRC_RMS_COEF00                0x34
#define AED_MDRC_RMS_COEF01                0x35
#define AED_MDRC_RELEASE_COEF00            0x36
#define AED_MDRC_RELEASE_COEF01            0x37
#define AED_MDRC_ATTACK_COEF00             0x38
#define AED_MDRC_ATTACK_COEF01             0x39
#define AED_MDRC_THD0                      0x3a
#define AED_MDRC_K0                        0x3b
#define AED_MDRC_LOW_GAIN                  0x3c
#define AED_MDRC_OFFSET0                   0x3d
#define AED_MDRC_RMS_COEF10                0x3e
#define AED_MDRC_RMS_COEF11                0x3f
#define AED_MDRC_RELEASE_COEF10            0x40
#define AED_MDRC_RELEASE_COEF11            0x41
#define AED_MDRC_ATTACK_COEF10             0x42
#define AED_MDRC_ATTACK_COEF11             0x43
#define AED_MDRC_THD1                      0x44
#define AED_MDRC_K1                        0x45
#define AED_MDRC_OFFSET1                   0x46
#define AED_MDRC_MID_GAIN                  0x47
#define AED_MDRC_RMS_COEF20                0x48
#define AED_MDRC_RMS_COEF21                0x49
#define AED_MDRC_RELEASE_COEF20            0x4a
#define AED_MDRC_RELEASE_COEF21            0x4b
#define AED_MDRC_ATTACK_COEF20             0x4c
#define AED_MDRC_ATTACK_COEF21             0x4d
#define AED_MDRC_THD2                      0x4e
#define AED_MDRC_K2                        0x4f
#define AED_MDRC_OFFSET2                   0x50
#define AED_MDRC_HIGH_GAIN                 0x51
#define AED_ED_CNTL                        0x52
#define AED_DC_EN                          0x53
#define AED_ND_LOW_THD                     0x54
#define AED_ND_HIGH_THD                    0x55
#define AED_ND_CNT_THD                     0x56
#define AED_ND_SUM_NUM                     0x57
#define AED_ND_CZ_NUM                      0x58
#define AED_ND_SUM_THD0                    0x59
#define AED_ND_SUM_THD1                    0x5a
#define AED_ND_CZ_THD0                     0x5b
#define AED_ND_CZ_THD1                     0x5c
#define AED_ND_COND_CNTL                   0x5d
#define AED_ND_RELEASE_COEF0               0x5e
#define AED_ND_RELEASE_COEF1               0x5f
#define AED_ND_ATTACK_COEF0                0x60
#define AED_ND_ATTACK_COEF1                0x61
#define AED_ND_CNTL                        0x62
#define AED_MIX0_LL                        0x63
#define AED_MIX0_RL                        0x64
#define AED_MIX0_LR                        0x65
#define AED_MIX0_RR                        0x66
#define AED_CLIP_THD                       0x67
#define AED_CH1_ND_SUM_OUT                 0x68
#define AED_CH2_ND_SUM_OUT                 0x69
#define AED_CH1_ND_CZ_OUT                  0x6a
#define AED_CH2_ND_CZ_OUT                  0x6b
#define AED_NOISE_STATUS                   0x6c
#define AED_POW_CURRENT_S0                 0x6d
#define AED_POW_CURRENT_S1                 0x6e
#define AED_POW_CURRENT_S2                 0x6f
#define AED_POW_OUT0                       0x70
#define AED_POW_OUT1                       0x71
#define AED_POW_OUT2                       0x72
#define AED_POW_ADJ_INDEX0                 0x73
#define AED_POW_ADJ_INDEX1                 0x74
#define AED_POW_ADJ_INDEX2                 0x75
#define AED_DRC_GAIN_INDEX0                0x76
#define AED_DRC_GAIN_INDEX1                0x77
#define AED_DRC_GAIN_INDEX2                0x78
#define AED_CH1_VOLUME_STATE               0x79
#define AED_CH2_VOLUME_STATE               0x7a
#define AED_CH1_VOLUME_GAIN                0x7b
#define AED_CH2_VOLUME_GAIN                0x7c
#define AED_FULL_POW_CURRENT               0x7d
#define AED_FULL_POW_OUT                   0x7e
#define AED_FULL_POW_ADJ                   0x7f
#define AED_FULL_DRC_GAIN                  0x80
#define AED_MASTER_VOLUME_STATE            0x81
#define AED_MASTER_VOLUME_GAIN             0x82

#define AED_TOP_CTL                        0x83
#define AED_TOP_REQ_CTL                    0x84

#define AED_TOP_CTL0                       0x83
#define AED_TOP_CTL1                       0x84
#define AED_TOP_CTL2                       0x85
#define AED_AED_TOP_ST                     0x86

/* dynamic control the ram coef from tm2_revb */
#define AED_EQDRC_DYNAMIC_CNTL             0x90
#define AED_COEF_RAM_CNTL_B                0x91
#define AED_COEF_RAM_DATA_B                0x92
#define AED_DRC_RMS_COEF0_B                0x93
#define AED_DRC_RMS_COEF1_B                0x94
#define AED_DRC_THD0_B                     0x95
#define AED_DRC_THD1_B                     0x96
#define AED_DRC_THD2_B                     0x97
#define AED_DRC_THD3_B                     0x98
#define AED_DRC_THD4_B                     0x99
#define AED_DRC_K0_B                       0x9a
#define AED_DRC_K1_B                       0x9b
#define AED_DRC_K2_B                       0x9c
#define AED_DRC_K3_B                       0x9d
#define AED_DRC_K4_B                       0x9e
#define AED_DRC_K5_B                       0x9f
#define AED_DRC_THD_OUT0_B                 0xa0
#define AED_DRC_THD_OUT1_B                 0xa1
#define AED_DRC_THD_OUT2_B                 0xa2
#define AED_DRC_THD_OUT3_B                 0xa3
#define AED_DRC_OFFSET_B                   0xa4
#define AED_DRC_RELEASE_COEF00_B           0xa5
#define AED_DRC_RELEASE_COEF01_B           0xa6
#define AED_DRC_RELEASE_COEF10_B           0xa7
#define AED_DRC_RELEASE_COEF11_B           0xa8
#define AED_DRC_RELEASE_COEF20_B           0xa9
#define AED_DRC_RELEASE_COEF21_B           0xaa
#define AED_DRC_RELEASE_COEF30_B           0xab
#define AED_DRC_RELEASE_COEF31_B           0xac
#define AED_DRC_RELEASE_COEF40_B           0xad
#define AED_DRC_RELEASE_COEF41_B           0xae
#define AED_DRC_RELEASE_COEF50_B           0xaf
#define AED_DRC_RELEASE_COEF51_B           0xb0
#define AED_DRC_ATTACK_COEF00_B            0xb1
#define AED_DRC_ATTACK_COEF01_B            0xb2
#define AED_DRC_ATTACK_COEF10_B            0xb3
#define AED_DRC_ATTACK_COEF11_B            0xb4
#define AED_DRC_ATTACK_COEF20_B            0xb5
#define AED_DRC_ATTACK_COEF21_B            0xb6
#define AED_DRC_ATTACK_COEF30_B            0xb7
#define AED_DRC_ATTACK_COEF31_B            0xb8
#define AED_DRC_ATTACK_COEF40_B            0xb9
#define AED_DRC_ATTACK_COEF41_B            0xba
#define AED_DRC_ATTACK_COEF50_B            0xbb
#define AED_DRC_ATTACK_COEF51_B            0xbc
#define AED_MDRC_RMS_COEF00_B              0xbd
#define AED_MDRC_RMS_COEF01_B              0xbe
#define AED_MDRC_RMS_COEF10_B              0xbf
#define AED_MDRC_RMS_COEF11_B              0xc0
#define AED_MDRC_RMS_COEF20_B              0xc1
#define AED_MDRC_RMS_COEF21_B              0xc2
#define AED_MDRC_RELEASE_COEF00_B          0xc3
#define AED_MDRC_RELEASE_COEF01_B          0xc4
#define AED_MDRC_RELEASE_COEF10_B          0xc5
#define AED_MDRC_RELEASE_COEF11_B          0xc6
#define AED_MDRC_RELEASE_COEF20_B          0xc7
#define AED_MDRC_RELEASE_COEF21_B          0xc8
#define AED_MDRC_ATTACK_COEF00_B           0xc9
#define AED_MDRC_ATTACK_COEF01_B           0xca
#define AED_MDRC_ATTACK_COEF10_B           0xcb
#define AED_MDRC_ATTACK_COEF11_B           0xcc
#define AED_MDRC_ATTACK_COEF20_B           0xcd
#define AED_MDRC_ATTACK_COEF21_B           0xce
#define AED_MDRC_THD0_B                    0xcf
#define AED_MDRC_THD1_B                    0xd0
#define AED_MDRC_THD2_B                    0xd1
#define AED_MDRC_K0_B                      0xd2
#define AED_MDRC_K1_B                      0xd3
#define AED_MDRC_K2_B                      0xd4
#define AED_MDRC_OFFSET0_B                 0xd5
#define AED_MDRC_OFFSET1_B                 0xd6
#define AED_MDRC_OFFSET2_B                 0xd7
#define AED_MDRC_LOW_GAIN_B                0xd8
#define AED_MDRC_MID_GAIN_B                0xd9
#define AED_MDRC_HIGH_GAIN_B               0xda
#define AED_DRC_CNTL_B                     0xdb
#define AED_DRC_LOOPBACK_CNTL_B            0xdc
#define AED_MDRC_CNTL_B                    0xdd
#define AED_STATUS_REG                     0xde

/*
 * VAD, Voice activity detection
 */
#define VAD_TOP_CTRL0                      0x000
#define VAD_TOP_CTRL1                      0x001
#define VAD_TOP_CTRL2                      0x002
#define VAD_FIR_CTRL	                   0x003
#define VAD_FIR_EMP                        0x004
#define VAD_FIR_COEF0                      0x005
#define VAD_FIR_COEF1                      0x006
#define VAD_FIR_COEF2                      0x007
#define VAD_FIR_COEF3                      0x008
#define VAD_FIR_COEF4                      0x009
#define VAD_FIR_COEF5                      0x00a
#define VAD_FIR_COEF6                      0x00b
#define VAD_FIR_COEF7                      0x00c
#define VAD_FIR_COEF8                      0x00d
#define VAD_FIR_COEF9                      0x00e
#define VAD_FIR_COEF10                     0x00f
#define VAD_FIR_COEF11                     0x010
#define VAD_FIR_COEF12                     0x011
#define VAD_FRAME_CTRL0                    0x012
#define VAD_FRAME_CTRL1                    0x013
#define VAD_FRAME_CTRL2                    0x014
#define VAD_CEP_CTRL0                      0x015
#define VAD_CEP_CTRL1                      0x016
#define VAD_CEP_CTRL2                      0x017
#define VAD_CEP_CTRL3                      0x018
#define VAD_CEP_CTRL4                      0x019
#define VAD_CEP_CTRL5                      0x01a
#define VAD_DEC_CTRL                       0x01b
#define VAD_TOP_STS0                       0x01c
#define VAD_TOP_STS1                       0x01d
#define VAD_TOP_STS2                       0x01e
#define VAD_FIR_STS0                       0x01f
#define VAD_FIR_STS1                       0x020
#define VAD_POW_STS0                       0x021
#define VAD_POW_STS1                       0x022
#define VAD_POW_STS2                       0x023
#define VAD_FFT_STS0                       0x024
#define VAD_FFT_STS1                       0x025
#define VAD_SPE_STS0                       0x026
#define VAD_SPE_STS1                       0x027
#define VAD_SPE_STS2                       0x028
#define VAD_SPE_STS3                       0x029
#define VAD_DEC_STS0                       0x02a
#define VAD_DEC_STS1                       0x02b
#define VAD_LUT_CTRL                       0x02c
#define VAD_LUT_WR                         0x02d
#define VAD_LUT_RD                         0x02e
#define VAD_IN_SEL0                        0x02f
#define VAD_IN_SEL1                        0x030
#define VAD_TO_DDR                         0x031

/*
 * eARC
 */
/* eARC RX CMDC */
#define EARC_RX_CMDC_TOP_CTRL0             0x000
#define EARC_RX_CMDC_TOP_CTRL1             0x001
#define EARC_RX_CMDC_TOP_CTRL2             0x002
#define EARC_RX_CMDC_TIMER_CTRL0           0x003
#define EARC_RX_CMDC_TIMER_CTRL1           0x004
#define EARC_RX_CMDC_TIMER_CTRL2           0x005
#define EARC_RX_CMDC_TIMER_CTRL3           0x006
#define EARC_RX_CMDC_VSM_CTRL0             0x007
#define EARC_RX_CMDC_VSM_CTRL1             0x008
#define EARC_RX_CMDC_VSM_CTRL2             0x009
#define EARC_RX_CMDC_VSM_CTRL3             0x00a
#define EARC_RX_CMDC_VSM_CTRL4             0x00b
#define EARC_RX_CMDC_VSM_CTRL5             0x00c
#define EARC_RX_CMDC_VSM_CTRL6             0x00d
#define EARC_RX_CMDC_VSM_CTRL7             0x00e
#define EARC_RX_CMDC_VSM_CTRL8             0x00f
#define EARC_RX_CMDC_VSM_CTRL9             0x010
#define EARC_RX_CMDC_SENDER_CTRL0          0x011
#define EARC_RX_CMDC_PACKET_CTRL0          0x012
#define EARC_RX_CMDC_PACKET_CTRL1          0x013
#define EARC_RX_CMDC_PACKET_CTRL2          0x014
#define EARC_RX_CMDC_PACKET_CTRL3          0x015
#define EARC_RX_CMDC_PACKET_CTRL4          0x016
#define EARC_RX_CMDC_PACKET_CTRL5          0x017
#define EARC_RX_CMDC_PACKET_CTRL6          0x018
#define EARC_RX_CMDC_BIPHASE_CTRL0         0x019
#define EARC_RX_CMDC_BIPHASE_CTRL1         0x01a
#define EARC_RX_CMDC_BIPHASE_CTRL2         0x01b
#define EARC_RX_CMDC_BIPHASE_CTRL3         0x01c
#define EARC_RX_CMDC_DEVICE_ID_CTRL        0x01d
#define EARC_RX_CMDC_DEVICE_WDATA          0x01e
#define EARC_RX_CMDC_DEVICE_RDATA          0x01f
#define EARC_RX_ANA_CTRL0                  0x020
#define EARC_RX_ANA_CTRL1                  0x021
#define EARC_RX_ANA_CTRL2                  0x022
#define EARC_RX_ANA_CTRL3                  0x023
#define EARC_RX_ANA_CTRL4                  0x024
#define EARC_RX_ANA_CTRL5                  0x025
#define EARC_RX_ANA_STAT0                  0x026
#define EARC_RX_CMDC_STATUS0               0x027
#define EARC_RX_CMDC_STATUS1               0x028
#define EARC_RX_CMDC_STATUS2               0x029
#define EARC_RX_CMDC_STATUS3               0x02a
#define EARC_RX_CMDC_STATUS4               0x02b
#define EARC_RX_CMDC_STATUS5               0x02c
#define EARC_RX_CMDC_STATUS6               0x02d
/* eARC TX CMDC */
#define EARC_TX_CMDC_TOP_CTRL0             0x030
#define EARC_TX_CMDC_TOP_CTRL1             0x031
#define EARC_TX_CMDC_TOP_CTRL2             0x032
#define EARC_TX_CMDC_TIMER_CTRL0           0x033
#define EARC_TX_CMDC_TIMER_CTRL1           0x034
#define EARC_TX_CMDC_TIMER_CTRL2           0x035
#define EARC_TX_CMDC_TIMER_CTRL3           0x036
#define EARC_TX_CMDC_VSM_CTRL0             0x037
#define EARC_TX_CMDC_VSM_CTRL1             0x038
#define EARC_TX_CMDC_VSM_CTRL2             0x039
#define EARC_TX_CMDC_VSM_CTRL3             0x03a
#define EARC_TX_CMDC_VSM_CTRL4             0x03b
#define EARC_TX_CMDC_VSM_CTRL5             0x03c
#define EARC_TX_CMDC_VSM_CTRL6             0x03d
#define EARC_TX_CMDC_VSM_CTRL7             0x03e
#define EARC_TX_CMDC_VSM_CTRL8             0x03f
#define EARC_TX_CMDC_VSM_CTRL9             0x041
#define EARC_TX_CMDC_SENDER_CTRL0          0x042
#define EARC_TX_CMDC_PACKET_CTRL0          0x043
#define EARC_TX_CMDC_PACKET_CTRL1          0x044
#define EARC_TX_CMDC_PACKET_CTRL2          0x045
#define EARC_TX_CMDC_PACKET_CTRL3          0x046
#define EARC_TX_CMDC_PACKET_CTRL4          0x047
#define EARC_TX_CMDC_PACKET_CTRL5          0x048
#define EARC_TX_CMDC_PACKET_CTRL6          0x049
#define EARC_TX_CMDC_BIPHASE_CTRL0         0x04a
#define EARC_TX_CMDC_BIPHASE_CTRL1         0x04b
#define EARC_TX_CMDC_BIPHASE_CTRL2         0x04c
#define EARC_TX_CMDC_BIPHASE_CTRL3         0x04d
#define EARC_TX_CMDC_DEVICE_ID_CTRL        0x04e
#define EARC_TX_CMDC_DEVICE_WDATA          0x04f
#define EARC_TX_CMDC_DEVICE_RDATA          0x050
#define EARC_TX_CMDC_MASTER_CTRL           0x051
#define EARC_TX_ANA_CTRL0                  0x052
#define EARC_TX_ANA_CTRL1                  0x053
#define EARC_TX_ANA_CTRL2                  0x054
#define EARC_TX_ANA_CTRL3                  0x055
#define EARC_TX_ANA_CTRL4                  0x056
#define EARC_TX_ANA_CTRL5                  0x057
#define EARC_TX_ANA_STAT0                  0x058
#define EARC_TX_CMDC_STATUS0               0x059
#define EARC_TX_CMDC_STATUS1               0x05a
#define EARC_TX_CMDC_STATUS2               0x05b
#define EARC_TX_CMDC_STATUS3               0x05c
#define EARC_TX_CMDC_STATUS4               0x05d
#define EARC_TX_CMDC_STATUS5               0x05e
#define EARC_TX_CMDC_STATUS6               0x05f
/* eARC RX DMAC */
#define EARCRX_DMAC_TOP_CTRL0              0x000
#define EARCRX_DMAC_SYNC_CTRL0             0x001
#define EARCRX_DMAC_SYNC_STAT0             0x002
#define EARCRX_SPDIFIN_SAMPLE_CTRL0        0x003
#define EARCRX_SPDIFIN_SAMPLE_CTRL1        0x004
#define EARCRX_SPDIFIN_SAMPLE_CTRL2        0x005
#define EARCRX_SPDIFIN_SAMPLE_CTRL3        0x006
#define EARCRX_SPDIFIN_SAMPLE_CTRL4        0x007
#define EARCRX_SPDIFIN_SAMPLE_CTRL5        0x008
#define EARCRX_SPDIFIN_SAMPLE_STAT0        0x009
#define EARCRX_SPDIFIN_SAMPLE_STAT1        0x00a
#define EARCRX_SPDIFIN_MUTE_VAL            0x00b
#define EARCRX_SPDIFIN_CTRL0               0x00c
#define EARCRX_SPDIFIN_CTRL1               0x00d
#define EARCRX_SPDIFIN_CTRL2               0x00e
#define EARCRX_SPDIFIN_CTRL3               0x00f
#define EARCRX_SPDIFIN_STAT0               0x010
#define EARCRX_SPDIFIN_STAT1               0x011
#define EARCRX_SPDIFIN_STAT2               0x012
#define EARCRX_DMAC_UBIT_CTRL0             0x013
#define EARCRX_IU_RDATA                    0x014
#define EARCRX_DMAC_UBIT_STAT0             0x015
#define EARCRX_ERR_CORRECT_CTRL0           0x016
#define EARCRX_ERR_CORRECT_STAT0           0x017
#define EARCRX_ANA_RST_CTRL0               0x018
#define EARCRX_ANA_RST_CTRL1               0x019
#define EARCRX_SPDIFIN_CTRL4               0x020
#define EARCRX_SPDIFIN_CTRL5               0x021
#define EARCRX_SPDIFIN_CTRL6               0x022
#define EARCRX_DMAC_SYNC_CTRL1             0x023
#define EARCRX_SPDIFIN_SAMPLE_CTRL6        0x024
#define EARCRX_DMAC_SYNC_CTRL3             0x025
#define EARCRX_DMAC_SYNC_CTRL4             0x026
#define EARCRX_DMAC_SYNC_CTRL5             0x027
#define EARCRX_DMAC_SYNC_STAT1             0x028
#define EARCRX_DMAC_SYNC_STAT2             0x029
#define EARCRX_DMAC_SYNC_STAT3             0x02a
/* eARC TX DMAC */
#define EARCTX_DMAC_TOP_CTRL0              0x000
#define EARCTX_MUTE_VAL                    0x001
#define EARCTX_SPDIFOUT_GAIN0              0x002
#define EARCTX_SPDIFOUT_GAIN1              0x003
#define EARCTX_SPDIFOUT_CTRL0              0x004
#define EARCTX_SPDIFOUT_CTRL1              0x005
#define EARCTX_SPDIFOUT_PREAMB             0x006
#define EARCTX_SPDIFOUT_SWAP               0x007
#define EARCTX_ERR_CORRT_CTRL0             0x008
#define EARCTX_ERR_CORRT_CTRL1             0x009
#define EARCTX_ERR_CORRT_CTRL2             0x00a
#define EARCTX_ERR_CORRT_CTRL3             0x00b
#define EARCTX_ERR_CORRT_CTRL4             0x00c
#define EARCTX_ERR_CORRT_STAT0             0x00d
#define EARCTX_SPDIFOUT_CHSTS0             0x00e
#define EARCTX_SPDIFOUT_CHSTS1             0x00f
#define EARCTX_SPDIFOUT_CHSTS2             0x010
#define EARCTX_SPDIFOUT_CHSTS3             0x011
#define EARCTX_SPDIFOUT_CHSTS4             0x012
#define EARCTX_SPDIFOUT_CHSTS5             0x013
#define EARCTX_SPDIFOUT_CHSTS6             0x014
#define EARCTX_SPDIFOUT_CHSTS7             0x015
#define EARCTX_SPDIFOUT_CHSTS8             0x016
#define EARCTX_SPDIFOUT_CHSTS9             0x017
#define EARCTX_SPDIFOUT_CHSTSA             0x018
#define EARCTX_SPDIFOUT_CHSTSB             0x019
#define EARCTX_FE_CTRL0                    0x01a
#define EARCTX_FE_STAT0                    0x01b
#define EARCTX_SPDIFOUT_STAT               0x01c
#define EARCTX_SPDIFOUT_CTRL2              0x01d
#define EARCTX_SPDIFOUT_GAIN2              0x01e
#define EARCTX_SPDIFOUT_GAIN3              0x01f
#define EARCTX_SPDIFOUT_GAIN4              0x020
#define EARCTX_SPDIFOUT_GAIN5              0x021
/* eARC RX */
#define EARCRX_TOP_CTRL0                   0x000
#define EARCRX_DMAC_INT_MASK               0x001
#define EARCRX_DMAC_INT_PENDING            0x002
#define EARCRX_CMDC_INT_MASK               0x003
#define EARCRX_CMDC_INT_PENDING            0x004
#define EARCRX_ANA_CTRL0                   0x005
#define EARCRX_ANA_CTRL1                   0x006
#define EARCRX_ANA_STAT0                   0x007
#define EARCRX_PLL_CTRL0                   0x008
#define EARCRX_PLL_CTRL1                   0x009
#define EARCRX_PLL_CTRL2                   0x00a
#define EARCRX_PLL_CTRL3                   0x00b
#define EARCRX_PLL_STAT0                   0x00c
/* eARC TX */
#define EARCTX_TOP_CTRL0                   0x000
#define EARCTX_DMAC_INT_MASK               0x001
#define EARCTX_DMAC_INT_PENDING            0x002
#define EARCTX_CMDC_INT_MASK               0x003
#define EARCTX_CMDC_INT_PENDING            0x004
#define EARCTX_ANA_CTRL0                   0x005
#define EARCTX_ANA_CTRL1                   0x006
#define EARCTX_ANA_CTRL2                   0x007
#define EARCTX_ANA_STAT0                   0x008
/* new resample */
#define AUDIO_RSAMP_CTRL0                  0x000
#define AUDIO_RSAMP_CTRL1                  0x001
#define AUDIO_RSAMP_CTRL2                  0x002
#define AUDIO_RSAMP_PHSINIT                0x003
#define AUDIO_RSAMP_PHSSTEP                0x004
#define AUDIO_RSAMP_SHIFT                  0x005
#define AUDIO_RSAMP_ADJ_CTRL0              0x006
#define AUDIO_RSAMP_ADJ_CTRL1              0x007
#define AUDIO_RSAMP_ADJ_SFT                0x008
#define AUDIO_RSAMP_ADJ_IDET_LEN           0x009
#define AUDIO_RSAMP_ADJ_FORCE              0x00a
#define AUDIO_RSAMP_ADJ_KI_FORCE           0x00b
#define AUDIO_RSAMP_WATCHDOG_THRD          0x00c
#define AUDIO_RSAMP_RO_STATUS              0x010
#define AUDIO_RSAMP_RO_ADJ_FREQ            0x011
#define AUDIO_RSAMP_RO_ADJ_DIFF_BAK        0x012
#define AUDIO_RSAMP_RO_ADJ_DIFF_DLT        0x013
#define AUDIO_RSAMP_RO_ADJ_PHS_ERR         0x014
#define AUDIO_RSAMP_RO_ADJ_KI_OUT          0x015
#define AUDIO_RSAMP_RO_IN_CNT              0x016
#define AUDIO_RSAMP_RO_OUT_CNT             0x017
#define AUDIO_RSAMP_POST_COEF0             0x020
#define AUDIO_RSAMP_POST_COEF1             0x021
#define AUDIO_RSAMP_POST_COEF2             0x022
#define AUDIO_RSAMP_POST_COEF3             0x023
#define AUDIO_RSAMP_POST_COEF4             0x024
#define AUDIO_RSAMP_AA_COEF_ADDR           0x030
#define AUDIO_RSAMP_AA_COEF_DATA           0x031
#define AUDIO_RSAMP_SINC_COEF_ADDR         0x040
#define AUDIO_RSAMP_SINC_COEF_DATA         0x041

#define EE_AUDIO_RSAMP_A_CHNUM_ID0         0x350
#define EE_AUDIO_RSAMP_A_CHNUM_ID1         0x351
#define EE_AUDIO_RSAMP_A_CHNUM_ID2         0x352
#define EE_AUDIO_RSAMP_A_CHNUM_ID3         0x353
#define EE_AUDIO_RSAMP_A_CHNUM_ID4         0x354
#define EE_AUDIO_RSAMP_A_CHNUM_ID5         0x355
#define EE_AUDIO_RSAMP_A_CHNUM_ID6         0x356
#define EE_AUDIO_RSAMP_A_CHNUM_ID7         0x357
#define EE_AUDIO_RSAMP_A_CHNUM_MASK        0x35E
#define EE_AUDIO_RSAMP_A_CHSYNC_CTRL       0x35F

#define EE_AUDIO_RSAMP_B_CHNUM_ID0         0x360
#define EE_AUDIO_RSAMP_B_CHNUM_ID1         0x361
#define EE_AUDIO_RSAMP_B_CHNUM_ID2         0x362
#define EE_AUDIO_RSAMP_B_CHNUM_ID3         0x363
#define EE_AUDIO_RSAMP_B_CHNUM_ID4         0x364
#define EE_AUDIO_RSAMP_B_CHNUM_ID5         0x365
#define EE_AUDIO_RSAMP_B_CHNUM_ID6         0x366
#define EE_AUDIO_RSAMP_B_CHNUM_ID7         0x367
#define EE_AUDIO_RSAMP_B_CHSYNC_CTRL       0x36F

/* AUDIO_TOP_VAD */
#define EE_AUDIO2_CLK81_CTRL               0x000
#define EE_AUDIO2_CLK81_EN                 0x001
#define EE_AUDIO2_SW_RESET0                0x002
#define EE_AUDIO2_CLK_GATE_EN0             0x003
#define EE_AUDIO2_SECURITY_CTRL0           0x004
#define EE_AUDIO2_SECURITY_CTRL1           0x005
#define EE_AUDIO2_MCLK_VAD_CTRL            0x010
#define EE_AUDIO2_CLK_VAD_CTRL             0x011
#define EE_AUDIO2_MST_DLY_CTRL0            0x012
#define EE_AUDIO2_MST_VAD_SCLK_CTRL0       0x013
#define EE_AUDIO2_MST_VAD_SCLK_CTRL1       0x014
#define EE_AUDIO2_CLK_TDMIN_VAD_CTRL       0x015
#define EE_AUDIO2_CLK_PDMIN_CTRL0          0x016
#define EE_AUDIO2_CLK_PDMIN_CTRL1          0x017
#define EE_AUDIO2_AUD_VAD_PAD_CTRL0        0x018
#define EE_AUDIO2_TOVAD_CTRL0              0x020
#define EE_AUDIO2_TODDR_VAD_CTRL0          0x030
#define EE_AUDIO2_TODDR_VAD_CTRL1          0x031
#define EE_AUDIO2_TODDR_VAD_CTRL2          0x032
#define EE_AUDIO2_TODDR_VAD_START_ADDR     0x033
#define EE_AUDIO2_TODDR_VAD_INIT_ADDR      0x034
#define EE_AUDIO2_TODDR_VAD_FINISH_ADDR    0x035
#define EE_AUDIO2_TODDR_VAD_START_ADDRB    0x036
#define EE_AUDIO2_TODDR_VAD_FINISH_ADDRB   0x037
#define EE_AUDIO2_TODDR_VAD_INT_ADDR       0x038
#define EE_AUDIO2_TODDR_VAD_STATUS1        0x039
#define EE_AUDIO2_TODDR_VAD_STATUS2        0x03a
#define EE_AUDIO2_TDMIN_VAD_CTRL           0x040
#define EE_AUDIO2_TDMIN_VAD_SWAP0          0x041
#define EE_AUDIO2_TDMIN_VAD_SWAP1          0x042
#define EE_AUDIO2_TDMIN_VAD_MUTE_VAL       0x043
#define EE_AUDIO2_TDMIN_VAD_STAT           0x044
#define EE_AUDIO2_TDMIN_VAD_MUTE0          0x050
#define EE_AUDIO2_TDMIN_VAD_MUTE1          0x051
#define EE_AUDIO2_TDMIN_VAD_MUTE2          0x052
#define EE_AUDIO2_TDMIN_VAD_MUTE3          0x053
#define EE_AUDIO2_TDMIN_VAD_MUTE4          0x054
#define EE_AUDIO2_TDMIN_VAD_MUTE5          0x055
#define EE_AUDIO2_TDMIN_VAD_MUTE6          0x056
#define EE_AUDIO2_TDMIN_VAD_MUTE7          0x057
#define EE_AUDIO2_TDMIN_VAD_MASK0          0x058
#define EE_AUDIO2_TDMIN_VAD_MASK1          0x059
#define EE_AUDIO2_TDMIN_VAD_MASK2          0x05a
#define EE_AUDIO2_TDMIN_VAD_MASK3          0x05b
#define EE_AUDIO2_TDMIN_VAD_MASK4          0x05c
#define EE_AUDIO2_TDMIN_VAD_MASK5          0x05d
#define EE_AUDIO2_TDMIN_VAD_MASK6          0x05e
#define EE_AUDIO2_TDMIN_VAD_MASK7          0x05f
#define EE_AUDIO2_VAD_DAT_PAD_CTRL0        0x060
#define EE_AUDIO2_VAD_DAT_PAD_CTRL1        0x061
#define EE_AUDIO2_TODDR_VAD_CHNUM_ID0      0x070
#define EE_AUDIO2_TODDR_VAD_CHNUM_ID1      0x071
#define EE_AUDIO2_TODDR_VAD_CHNUM_ID2      0x072
#define EE_AUDIO2_TODDR_VAD_CHNUM_ID3      0x073
#define EE_AUDIO2_TODDR_VAD_CHNUM_ID4      0x074
#define EE_AUDIO2_TODDR_VAD_CHNUM_ID5      0x075
#define EE_AUDIO2_TODDR_VAD_CHNUM_ID6      0x076
#define EE_AUDIO2_TODDR_VAD_CHNUM_ID7      0x077
#define EE_AUDIO2_TODDR_VAD_CHSYNC_CTRL    0x07f
#define EE_AUDIO2_AM2AXI_CTRL0             0x080
#define EE_AUDIO2_AXIWR_ASYNC_CTRL0        0x081
#define EE_AUDIO2_AM2AXI_STAT              0x088
#define EE_AUDIO2_AXIWR_ASYNC_STAT         0x089
#define EE_AUDIO2_EXCEPTION_IRQ_STS0       0x090
#define EE_AUDIO2_EXCEPTION_IRQ_MASK0      0x091
#define EE_AUDIO2_EXCEPTION_IRQ_MODE0      0x092
#define EE_AUDIO2_EXCEPTION_IRQ_CLR0       0x093
#define EE_AUDIO2_EXCEPTION_IRQ_INV0       0x094

/* audio out clkmux registers */
#define EE_REG_MUX_CTRL0                   0x0E3
#define EE_REG_MUX_CTRL1                   0x0E4

/* 64 exception irq */
#define EE_AUDIO_EXCEPTION_IRQ_STS0        0x380
#define EE_AUDIO_EXCEPTION_IRQ_STS1        0x381
#define EE_AUDIO_EXCEPTION_IRQ_MASK0       0x382
#define EE_AUDIO_EXCEPTION_IRQ_MASK1       0x383
#define EE_AUDIO_EXCEPTION_IRQ_CLR0        0x386
#define EE_AUDIO_EXCEPTION_IRQ_CLR1        0x387

#define EE_AUDIO_DAT_PAD_CTRL0             0x390
#define EE_AUDIO_DAT_PAD_CTRL1             0x391
#define EE_AUDIO_DAT_PAD_CTRL2             0x392
#define EE_AUDIO_DAT_PAD_CTRL3             0x393
#define EE_AUDIO_DAT_PAD_CTRL4             0x394
#define EE_AUDIO_DAT_PAD_CTRL5             0x395
#define EE_AUDIO_DAT_PAD_CTRL6             0x396
#define EE_AUDIO_DAT_PAD_CTRL7             0x397
#define EE_AUDIO_DAT_PAD_CTRL8             0x398
#define EE_AUDIO_DAT_PAD_CTRL9             0x399
#define EE_AUDIO_DAT_PAD_CTRLA             0x39A
#define EE_AUDIO_DAT_PAD_CTRLB             0x39B
#define EE_AUDIO_DAT_PAD_CTRLC             0x39C
#define EE_AUDIO_DAT_PAD_CTRLD             0x39D
#define EE_AUDIO_DAT_PAD_CTRLE             0x39E
#define EE_AUDIO_DAT_PAD_CTRLF             0x39F
#define EE_AUDIO_DAT_PAD_CTRLG             0x3a5
#define EE_AUDIO_DAT_PAD_CTRLH             0x3a6

#define EE_AUDIO_MCLK_PAD_CTRL0            0x3A0
#define EE_AUDIO_MCLK_PAD_CTRL1            0x3A1
#define EE_AUDIO_SCLK_PAD_CTRL0            0x3A2
#define EE_AUDIO_SCLK_PAD_CTRL1            0x3A3

/*HHI bus*/
#define HHI_AUDIO_MEM_PD_REG0              0x045

/* hdmirx for arc */
#define HDMIRX_PHY_MISC2                   0x007
#define HDMIRX_EARCTX_CNTL0                0x040
#define HDMIRX_EARCTX_CNTL1                0x041
#define HDMIRX_ARC_CNTL                    0x042

/*pcpd monitor */
#define EE_AUDIO_PCPD_MON_A_CTRL0          0x3b0
#define EE_AUDIO_PCPD_MON_A_CTRL1          0x3b1
#define EE_AUDIO_PCPD_MON_A_CTRL2          0x3b2
#define EE_AUDIO_PCPD_MON_A_CTRL3          0x3b3
#define EE_AUDIO_PCPD_MON_A_CTRL4          0x3b4
#define EE_AUDIO_PCPD_MON_A_CTRL5          0x3b5
#define EE_AUDIO_PCPD_MON_A_STAT0          0x3b8
#define EE_AUDIO_PCPD_MON_A_STAT1          0x3b9

#define EE_AUDIO_PCPD_MON_B_CTRL0          0x3c0
#define EE_AUDIO_PCPD_MON_B_CTRL1          0x3c1
#define EE_AUDIO_PCPD_MON_B_CTRL2          0x3c2
#define EE_AUDIO_PCPD_MON_B_CTRL3          0x3c3
#define EE_AUDIO_PCPD_MON_B_CTRL4          0x3c4
#define EE_AUDIO_PCPD_MON_B_CTRL5          0x3c5
#define EE_AUDIO_PCPD_MON_B_STAT0          0x3c8
#define EE_AUDIO_PCPD_MON_B_STAT1          0x3c9

/* AUDIO EQ CORE */
#define AEQ_COEF_CNTL_A                    0x000
#define AEQ_COEF_DATA_A                    0x001
#define AEQ_COEF_CNTL_B                    0x002
#define AEQ_COEF_DATA_B                    0x003
#define AEQ_SOFT_RSET                      0x009
#define AEQ_TOP_CTRL                       0x010
#define AEQ_STATUS0_CTRL                   0x011
#define AEQ_STATUS1_CTRL                   0x012
#define AEQ_STATUS2_CTRL                   0x013
#define AEQ_STATUS3_CTRL                   0x014
#define AEQ_STATUS4_CTRL                   0x015
#define AEQ_STATUS5_CTRL                   0x016
#define AEQ_STATUS6_CTRL                   0x017
#define AEQ_STATUS7_CTRL                   0x018

/* AUDIO RESAMPLE_ACC */
#define AUDIO_RSAMP_ACC_CTRL0              0x400
#define AUDIO_RSAMP_ACC_CTRL1              0x401
#define AUDIO_RSAMP_ACC_CTRL2              0x402
#define AUDIO_RSAMP_ACC_PHSINIT            0x403
#define AUDIO_RSAMP_ACC_PHSSTEP            0x404
#define AUDIO_RSAMP_ACC_SHIFT              0x405
#define AUDIO_RSAMP_ACC_ADJ_CTRL0          0x406
#define AUDIO_RSAMP_ACC_ADJ_CTRL1          0x407
#define AUDIO_RSAMP_ACC_ADJ_SFT            0x408
#define AUDIO_RSAMP_ACC_ADJ_IDET_LEN       0x409
#define AUDIO_RSAMP_ACC_ADJ_FORCE          0x40a
#define AUDIO_RSAMP_ACC_ADJ_KI_FORCE       0x40b
#define AUDIO_RSAMP_ACC_WATCHDOG_THRD      0x40c
#define AUDIO_RSAMP_ACC_DBG_INFO           0x40d
#define AUDIO_RSAMP_ACC_AOUT_FORCE         0x40e
#define AUDIO_RSAMP_ACC_IRQ_CTRL           0x40f
#define AUDIO_RSAMP_ACC_PKG_CTRL           0x419
#define AUDIO_RSAMP_ACC_POST_COEF0         0x420
#define AUDIO_RSAMP_ACC_POST_COEF1         0x421
#define AUDIO_RSAMP_ACC_POST_COEF2         0x422
#define AUDIO_RSAMP_ACC_POST_COEF3         0x423
#define AUDIO_RSAMP_ACC_POST_COEF4         0x424
#define AUDIO_RSAMP_ACC_AA_COEF_ADDR       0x430
#define AUDIO_RSAMP_ACC_AA_COEF_DATA       0x431
#define AUDIO_RSAMP_ACC_SINC_COEF_ADDR     0x440
#define AUDIO_RSAMP_ACC_SINC_COEF_DATA     0x441
#define AUDIO_RSAMP_ACC_RO_STATUS          0x450
#define AUDIO_RSAMP_ACC_RO_ADJ_FREQ        0x451
#define AUDIO_RSAMP_ACC_RO_ADJ_DIFF_BAK    0x452
#define AUDIO_RSAMP_ACC_RO_ADJ_DIFF_DLT    0x453
#define AUDIO_RSAMP_ACC_RO_ADJ_PHS_ERR     0x454
#define AUDIO_RSAMP_ACC_RO_ADJ_KI_OUT      0x455
#define AUDIO_RSAMP_ACC_RO_IN_CNT          0x456
#define AUDIO_RSAMP_ACC_RO_OUT_CNT         0x457
#define AUDIO_RSAMP_ACC_RO_ADJ_PHS_ERR_VAR 0x458

/* AUDIO ASRC WRAPPER */
#define AUDIO_ACC_ASRC_WRAPPER_TOP_CTL0    0x500
#define AUDIO_ACC_ASRC_WRAPPER_TOP_CTL1    0x501
#define AUDIO_ACC_ASRC_WRAPPER_TOP_CTL2    0x502
#define AUDIO_ACC_ASRC_WRAPPER_TOP_CTL3    0x503
#define AUDIO_ACC_ASRC_WRAPPER_TOP_CTL4    0x504
#define AUDIO_ACC_ASRC_WRAPPER_TOP_ST0     0x510
#define AUDIO_ACC_ASRC_WRAPPER_TOP_ST1     0x511

/* AUDIO EQDRC WRAPPER */
#define AUDIO_ACC_EQDRC_WRAPPER_TOP_CTL0   0x600
#define AUDIO_ACC_EQDRC_WRAPPER_TOP_CTL1   0x601
#define AUDIO_ACC_EQDRC_WRAPPER_TOP_CTL2   0x602
#define AUDIO_ACC_EQDRC_WRAPPER_TOP_CTL3   0x603
#define AUDIO_ACC_EQDRC_WRAPPER_TOP_CTL4   0x604
#define AUDIO_ACC_EQDRC_WRAPPER_TOP_ST0    0x610
#define AUDIO_ACC_EQDRC_WRAPPER_TOP_ST1    0x611

/* AUDIO ACC TOP */
#define AUDIO_ACC_CLK_GATE_EN              0x700
#define AUDIO_ACC_SW_RESERT                0x701
#define AUDIO_ACC_SECURITY_CTRL_0          0x70a
#define AUDIO_ACC_SECURITY_CTRL_1          0x70b
#define AUDIO_ACC_SECURITY_CTRL_2          0x70c
#define AUDIO_ACC_DBG_0                    0x710
#define AUDIO_ACC_DBG_1                    0x711
#define AUDIO_ACC_FRDDR_0_CTRL0            0x720
#define AUDIO_ACC_FRDDR_0_CTRL1            0x721
#define AUDIO_ACC_FRDDR_0_CTRL2            0x722
#define AUDIO_ACC_FRDDR_0_START_ADDR       0x723
#define AUDIO_ACC_FRDDR_0_INIT_ADDR        0x724
#define AUDIO_ACC_FRDDR_0_FINISH_ADDR      0x725
#define AUDIO_ACC_FRDDR_0_START_ADDRB      0x726
#define AUDIO_ACC_FRDDR_0_FINISH_ADDRB     0x727
#define AUDIO_ACC_FRDDR_0_INT_ADDR         0x728
#define AUDIO_ACC_FRDDR_0_RO_STATUS1       0x729
#define AUDIO_ACC_FRDDR_0_RO_STATUS2       0x72a
#define AUDIO_ACC_FRDDR_0_CTRL3            0x72b
#define AUDIO_ACC_FRDDR_1_CTRL0            0x730
#define AUDIO_ACC_FRDDR_1_CTRL1            0x731
#define AUDIO_ACC_FRDDR_1_CTRL2            0x732
#define AUDIO_ACC_FRDDR_1_START_ADDR       0x733
#define AUDIO_ACC_FRDDR_1_INIT_ADDR        0x734
#define AUDIO_ACC_FRDDR_1_FINISH_ADDR      0x735
#define AUDIO_ACC_FRDDR_1_START_ADDRB      0x736
#define AUDIO_ACC_FRDDR_1_FINISH_ADDRB     0x737
#define AUDIO_ACC_FRDDR_1_INT_ADDR         0x738
#define AUDIO_ACC_FRDDR_1_RO_STATUS1       0x739
#define AUDIO_ACC_FRDDR_1_RO_STATUS2       0x73a
#define AUDIO_ACC_FRDDR_1_CTRL3            0x73b
#define AUDIO_ACC_TODDR_0_CTRL0            0x740
#define AUDIO_ACC_TODDR_0_CTRL1            0x741
#define AUDIO_ACC_TODDR_0_CTRL2            0x742
#define AUDIO_ACC_TODDR_0_START_ADDR       0x743
#define AUDIO_ACC_TODDR_0_INIT_ADDR        0x744
#define AUDIO_ACC_TODDR_0_FINISH_ADDR      0x745
#define AUDIO_ACC_TODDR_0_START_ADDRB      0x746
#define AUDIO_ACC_TODDR_0_FINISH_ADDRB     0x747
#define AUDIO_ACC_TODDR_0_INT_ADDR         0x748
#define AUDIO_ACC_TODDR_0_RO_STATUS1       0x749
#define AUDIO_ACC_TODDR_0_RO_STATUS2       0x74a
#define AUDIO_ACC_TODDR_0_CHSYNC_CTRL      0x74b
#define AUDIO_ACC_TODDR_0_CHNUM_ID_0       0x74c
#define AUDIO_ACC_TODDR_0_CHNUM_ID_1       0x74d
#define AUDIO_ACC_TODDR_0_CHNUM_ID_2       0x74e
#define AUDIO_ACC_TODDR_0_CHNUM_ID_3       0x74f
#define AUDIO_ACC_TODDR_0_CHNUM_ID_4       0x750
#define AUDIO_ACC_TODDR_0_CHNUM_ID_5       0x751
#define AUDIO_ACC_TODDR_0_CHNUM_ID_6       0x752
#define AUDIO_ACC_TODDR_0_CHNUM_ID_7       0x753
#define AUDIO_ACC_TODDR_0_CTRL3            0x754
#define AUDIO_ACC_TODDR_1_CTRL0            0x760
#define AUDIO_ACC_TODDR_1_CTRL1            0x761
#define AUDIO_ACC_TODDR_1_CTRL2            0x762
#define AUDIO_ACC_TODDR_1_START_ADDR       0x763
#define AUDIO_ACC_TODDR_1_INIT_ADDR        0x764
#define AUDIO_ACC_TODDR_1_FINISH_ADDR      0x765
#define AUDIO_ACC_TODDR_1_START_ADDRB      0x766
#define AUDIO_ACC_TODDR_1_FINISH_ADDRB     0x767
#define AUDIO_ACC_TODDR_1_INT_ADDR         0x768
#define AUDIO_ACC_TODDR_1_RO_STATUS1       0x769
#define AUDIO_ACC_TODDR_1_RO_STATUS2       0x76a
#define AUDIO_ACC_TODDR_1_CHSYNC_CTRL      0x76b
#define AUDIO_ACC_TODDR_1_CHNUM_ID_0       0x76c
#define AUDIO_ACC_TODDR_1_CHNUM_ID_1       0x76d
#define AUDIO_ACC_TODDR_1_CHNUM_ID_2       0x76e
#define AUDIO_ACC_TODDR_1_CHNUM_ID_3       0x76f
#define AUDIO_ACC_TODDR_1_CHNUM_ID_4       0x770
#define AUDIO_ACC_TODDR_1_CHNUM_ID_5       0x771
#define AUDIO_ACC_TODDR_1_CHNUM_ID_6       0x772
#define AUDIO_ACC_TODDR_1_CHNUM_ID_7       0x773
#define AUDIO_ACC_TODDR_1_CTRL3            0x774
#define AUDIO_ACC_A2P_CONV_0               0x780
#define AUDIO_ACC_A2P_CONV_1               0x781
#define AUDIO_ACC_PATH_CTRL                0x782

#endif
