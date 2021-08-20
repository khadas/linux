/*
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
enum NalUnitType
{
  NAL_UNIT_CODED_SLICE_TRAIL_N = 0,   // 0
  NAL_UNIT_CODED_SLICE_TRAIL_R,   // 1

  NAL_UNIT_CODED_SLICE_TSA_N,     // 2
  NAL_UNIT_CODED_SLICE_TLA,       // 3   // Current name in the spec: TSA_R

  NAL_UNIT_CODED_SLICE_STSA_N,    // 4
  NAL_UNIT_CODED_SLICE_STSA_R,    // 5

  NAL_UNIT_CODED_SLICE_RADL_N,    // 6
  NAL_UNIT_CODED_SLICE_DLP,       // 7 // Current name in the spec: RADL_R

  NAL_UNIT_CODED_SLICE_RASL_N,    // 8
  NAL_UNIT_CODED_SLICE_TFD,       // 9 // Current name in the spec: RASL_R

  NAL_UNIT_RESERVED_10,
  NAL_UNIT_RESERVED_11,
  NAL_UNIT_RESERVED_12,
  NAL_UNIT_RESERVED_13,
  NAL_UNIT_RESERVED_14,
  NAL_UNIT_RESERVED_15,

  NAL_UNIT_CODED_SLICE_BLA,       // 16   // Current name in the spec: BLA_W_LP
  NAL_UNIT_CODED_SLICE_BLANT,     // 17   // Current name in the spec: BLA_W_DLP
  NAL_UNIT_CODED_SLICE_BLA_N_LP,  // 18
  NAL_UNIT_CODED_SLICE_IDR,       // 19  // Current name in the spec: IDR_W_DLP
  NAL_UNIT_CODED_SLICE_IDR_N_LP,  // 20
  NAL_UNIT_CODED_SLICE_CRA,       // 21
  NAL_UNIT_RESERVED_22,
  NAL_UNIT_RESERVED_23,

  NAL_UNIT_RESERVED_24,
  NAL_UNIT_RESERVED_25,
  NAL_UNIT_RESERVED_26,
  NAL_UNIT_RESERVED_27,
  NAL_UNIT_RESERVED_28,
  NAL_UNIT_RESERVED_29,
  NAL_UNIT_RESERVED_30,
  NAL_UNIT_RESERVED_31,

  NAL_UNIT_VPS,                   // 32
  NAL_UNIT_SPS,                   // 33
  NAL_UNIT_PPS,                   // 34
  NAL_UNIT_ACCESS_UNIT_DELIMITER, // 35
  NAL_UNIT_EOS,                   // 36
  NAL_UNIT_EOB,                   // 37
  NAL_UNIT_FILLER_DATA,           // 38
  NAL_UNIT_SEI,                   // 39 Prefix SEI
  NAL_UNIT_SEI_SUFFIX,            // 40 Suffix SEI
  NAL_UNIT_RESERVED_41,
  NAL_UNIT_RESERVED_42,
  NAL_UNIT_RESERVED_43,
  NAL_UNIT_RESERVED_44,
  NAL_UNIT_RESERVED_45,
  NAL_UNIT_RESERVED_46,
  NAL_UNIT_RESERVED_47,
  NAL_UNIT_UNSPECIFIED_48,
  NAL_UNIT_UNSPECIFIED_49,
  NAL_UNIT_UNSPECIFIED_50,
  NAL_UNIT_UNSPECIFIED_51,
  NAL_UNIT_UNSPECIFIED_52,
  NAL_UNIT_UNSPECIFIED_53,
  NAL_UNIT_UNSPECIFIED_54,
  NAL_UNIT_UNSPECIFIED_55,
  NAL_UNIT_UNSPECIFIED_56,
  NAL_UNIT_UNSPECIFIED_57,
  NAL_UNIT_UNSPECIFIED_58,
  NAL_UNIT_UNSPECIFIED_59,
  NAL_UNIT_UNSPECIFIED_60,
  NAL_UNIT_UNSPECIFIED_61,
  NAL_UNIT_UNSPECIFIED_62,
  NAL_UNIT_UNSPECIFIED_63,
  NAL_UNIT_INVALID,
};

int forbidden_zero_bit;
int m_nalUnitType;
int m_reservedZero6Bits;
int m_temporalId;

//---------------------------------------------------
// Amrisc Software Interrupt
//---------------------------------------------------
#define AMRISC_STREAM_EMPTY_REQ 0x01
#define AMRISC_PARSER_REQ       0x02
#define AMRISC_MAIN_REQ         0x04

//---------------------------------------------------
// AOM_AV1_DEC_STATUS (HEVC_DEC_STATUS) define
//---------------------------------------------------
  /*command*/
#define AOM_AV1_DEC_IDLE                     0
#define AOM_AV1_DEC_FRAME_HEADER             1
#define AOM_AV1_DEC_TILE_END                 2
#define AOM_AV1_DEC_TG_END                   3
#define AOM_AV1_DEC_LCU_END                  4
#define AOM_AV1_DECODE_SLICE                 5
#define AOM_AV1_SEARCH_HEAD                  6
#define AOM_AV1_DUMP_LMEM                    7
#define AOM_AV1_FGS_PARAM_CONT               8
#define AOM_AV1_FGS_PARAM_CONT               8
#define AOM_AV1_PIC_END_CONT                 9
  /*status*/
#define AOM_AV1_DEC_PIC_END                  0xe0
  /*AOM_AV1_FGS_PARA:
    Bit[11] - 0 Read, 1 - Write
  Bit[10:8] - film_grain_params_ref_idx, For Write request
  */
#define AOM_AV1_FGS_PARAM                    0xe1
#define AOM_AV1_DEC_PIC_END_PRE              0xe2
#define AOM_AV1_HEAD_PARSER_DONE          0xf0
#define AOM_AV1_HEAD_SEARCH_DONE          0xf1
#define AOM_AV1_SEQ_HEAD_PARSER_DONE      0xf2
#define AOM_AV1_FRAME_HEAD_PARSER_DONE    0xf3
#define AOM_AV1_FRAME_PARSER_DONE   0xf4
#define AOM_AV1_REDUNDANT_FRAME_HEAD_PARSER_DONE   0xf5
#define HEVC_ACTION_DONE            0xff


//---------------------------------------------------
// Include "parser_cmd.h"
//---------------------------------------------------
#define PARSER_CMD_SKIP_CFG_0 0x0000090b

#define PARSER_CMD_SKIP_CFG_1 0x1b14140f

#define PARSER_CMD_SKIP_CFG_2 0x001b1910

#define PARSER_CMD_NUMBER 37

unsigned short parser_cmd[PARSER_CMD_NUMBER] = {
0x0401,
0x8401,
0x0800,
0x0402,
0x9002,
0x1423,
0x8CC3,
0x1423,
0x8804,
0x9825,
0x0800,
0x04FE,
0x8406,
0x8411,
0x1800,
0x8408,
0x8409,
0x8C2A,
0x9C2B,
0x1C00,
0x840F,
0x8407,
0x8000,
0x8408,
0x2000,
0xA800,
0x8410,
0x04DE,
0x840C,
0x840D,
0xAC00,
0xA000,
0x08C0,
0x08E0,
0xA40E,
0xFC00,
0x7C00
};
