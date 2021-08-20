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
#ifndef AV1_GLOBAL_H_
#define AV1_GLOBAL_H_
#define AOM_AV1_MMU_DW
#ifndef HAVE_NEON
#define HAVE_NEON 0
#endif
#ifndef CONFIG_ACCOUNTING
#define CONFIG_ACCOUNTING 0
#endif
#ifndef CONFIG_INSPECTION
#define CONFIG_INSPECTION 0
#endif
#ifndef CONFIG_LPF_MASK
#define CONFIG_LPF_MASK 0
#endif
#ifndef CONFIG_SIZE_LIMIT
#define CONFIG_SIZE_LIMIT 0
#endif

#define SUPPORT_SCALE_FACTOR
#define USE_SCALED_WIDTH_FROM_UCODE
#define AML
#ifdef CONFIG_AMLOGIC_MEDIA_MULTI_DEC
#define AML_DEVICE
#endif
#ifdef BUFMGR_FOR_SIM
#define printf io_printf
#endif

#ifndef INT_MAX
#define INT_MAX 0x7FFFFFFF
#endif
#define AOMMIN(x, y) (((x) < (y)) ? (x) : (y))
#define AOMMAX(x, y) (((x) > (y)) ? (x) : (y))

/*
 *typedef char int8_t;
 *#ifndef BUFMGR_FOR_SIM
 *typedef unsigned char uint8_t;
 *#endif
 *typedef unsigned int uint32_t;
 *typedef int int32_t;
 *typedef long long int64_t;
 */

#ifdef AML
#define AOM_AV1_MMU
#define FILM_GRAIN_REG_SIZE  39
typedef struct buff_s
{
    uint32_t buf_start;
    uint32_t buf_size;
    uint32_t buf_end;
} buff_t;

typedef struct BuffInfo_s
{
    uint32_t max_width;
    uint32_t max_height;
    uint32_t start_adr;
    uint32_t end_adr;
    buff_t ipp;
    buff_t sao_abv;
    buff_t sao_vb;
    buff_t short_term_rps;
    buff_t vps;
    buff_t seg_map;
    buff_t daala_top;
    buff_t sao_up;
    buff_t swap_buf;
    buff_t cdf_buf;
    buff_t gmc_buf;
    buff_t scalelut;
    buff_t dblk_para;
    buff_t dblk_data;
    buff_t cdef_data;
    buff_t ups_data;
#ifdef AOM_AV1_MMU
    buff_t mmu_vbh;
    buff_t cm_header;
#endif
#ifdef AOM_AV1_MMU_DW
    buff_t mmu_vbh_dw;
    buff_t cm_header_dw;
#endif
    buff_t fgs_table;
    buff_t mpred_above;
    buff_t mpred_mv;
    buff_t rpm;
    buff_t lmem;
} BuffInfo_t;
#endif

#if 0
#define va_start(v,l)   __builtin_va_start(v,l)
#define va_end(v)       __builtin_va_end(v)
#define va_arg(v,l)     __builtin_va_arg(v,l)
#endif

/*
mem.h
*/
#if (defined(__GNUC__) && __GNUC__) || defined(__SUNPRO_C)
#define DECLARE_ALIGNED(n, typ, val) typ val __attribute__((aligned(n)))
#elif defined(_MSC_VER)
#define DECLARE_ALIGNED(n, typ, val) __declspec(align(n)) typ val
#else
#warning No alignment directives known for this compiler.
#define DECLARE_ALIGNED(n, typ, val) typ val
#endif

/* Indicates that the usage of the specified variable has been audited to assure
 * that it's safe to use uninitialized. Silences 'may be used uninitialized'
 * warnings on gcc.
 */
#if defined(__GNUC__) && __GNUC__
#define UNINITIALIZED_IS_SAFE(x) x = x
#else
#define UNINITIALIZED_IS_SAFE(x) x
#endif

#if HAVE_NEON && defined(_MSC_VER)
#define __builtin_prefetch(x)
#endif

/* Shift down with rounding for use when n >= 0, value >= 0 */
#define ROUND_POWER_OF_TWO(value, n) (((value) + (((1 << (n)) >> 1))) >> (n))

/* Shift down with rounding for signed integers, for use when n >= 0 */
#define ROUND_POWER_OF_TWO_SIGNED(value, n)           \
  (((value) < 0) ? -ROUND_POWER_OF_TWO(-(value), (n)) \
                 : ROUND_POWER_OF_TWO((value), (n)))

/* Shift down with rounding for use when n >= 0, value >= 0 for (64 bit) */
#define ROUND_POWER_OF_TWO_64(value, n) \
  (((value) + ((((int64_t)1 << (n)) >> 1))) >> (n))
/* Shift down with rounding for signed integers, for use when n >= 0 (64 bit) */
#define ROUND_POWER_OF_TWO_SIGNED_64(value, n)           \
  (((value) < 0) ? -ROUND_POWER_OF_TWO_64(-(value), (n)) \
                 : ROUND_POWER_OF_TWO_64((value), (n)))

/* shift right or left depending on sign of n */
#define RIGHT_SIGNED_SHIFT(value, n) \
  ((n) < 0 ? ((value) << (-(n))) : ((value) >> (n)))

#define ALIGN_POWER_OF_TWO(value, n) \
  (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

#define DIVIDE_AND_ROUND(x, y) (((x) + ((y) >> 1)) / (y))

#define CONVERT_TO_SHORTPTR(x) ((uint16_t *)(((uintptr_t)(x)) << 1))
#define CONVERT_TO_BYTEPTR(x) ((uint8_t *)(((uintptr_t)(x)) >> 1))

#ifdef AML
#define TYPEDEF typedef
#define UENUM1BYTE(enumvar) enumvar
#define SENUM1BYTE(enumvar) enumvar
#define UENUM2BYTE(enumvar) enumvar
#define SENUM2BYTE(enumvar) enumvar
#define UENUM4BYTE(enumvar) enumvar
#define SENUM4BYTE(enumvar) enumvar

#else
#define TYPEDEF
/*!\brief force enum to be unsigned 1 byte*/
#define UENUM1BYTE(enumvar) \
  ;                         \
  typedef uint8_t enumvar

/*!\brief force enum to be signed 1 byte*/
#define SENUM1BYTE(enumvar) \
  ;                         \
  typedef int8_t enumvar

/*!\brief force enum to be unsigned 2 byte*/
#define UENUM2BYTE(enumvar) \
  ;                         \
  typedef uint16_t enumvar

/*!\brief force enum to be signed 2 byte*/
#define SENUM2BYTE(enumvar) \
  ;                         \
  typedef int16_t enumvar

/*!\brief force enum to be unsigned 4 byte*/
#define UENUM4BYTE(enumvar) \
  ;                         \
  typedef uint32_t enumvar

/*!\brief force enum to be unsigned 4 byte*/
#define SENUM4BYTE(enumvar) \
  ;                         \
  typedef int32_t enumvar
#endif


/*
#include "enums.h"
*/
#undef MAX_SB_SIZE

// Max superblock size
#define MAX_SB_SIZE_LOG2 7
#define MAX_SB_SIZE (1 << MAX_SB_SIZE_LOG2)
#define MAX_SB_SQUARE (MAX_SB_SIZE * MAX_SB_SIZE)

// Min superblock size
#define MIN_SB_SIZE_LOG2 6

// Pixels per Mode Info (MI) unit
#define MI_SIZE_LOG2 2
#define MI_SIZE (1 << MI_SIZE_LOG2)

// MI-units per max superblock (MI Block - MIB)
#define MAX_MIB_SIZE_LOG2 (MAX_SB_SIZE_LOG2 - MI_SIZE_LOG2)
#define MAX_MIB_SIZE (1 << MAX_MIB_SIZE_LOG2)

// MI-units per min superblock
#define MIN_MIB_SIZE_LOG2 (MIN_SB_SIZE_LOG2 - MI_SIZE_LOG2)

// Mask to extract MI offset within max MIB
#define MAX_MIB_MASK (MAX_MIB_SIZE - 1)

// Maximum number of tile rows and tile columns
#define MAX_TILE_ROWS 64
#define MAX_TILE_COLS 64

#define MAX_VARTX_DEPTH 2

#define MI_SIZE_64X64 (64 >> MI_SIZE_LOG2)
#define MI_SIZE_128X128 (128 >> MI_SIZE_LOG2)

#define MAX_PALETTE_SQUARE (64 * 64)
// Maximum number of colors in a palette.
#define PALETTE_MAX_SIZE 8
// Minimum number of colors in a palette.
#define PALETTE_MIN_SIZE 2

#define FRAME_OFFSET_BITS 5
#define MAX_FRAME_DISTANCE ((1 << FRAME_OFFSET_BITS) - 1)

// 4 frame filter levels: y plane vertical, y plane horizontal,
// u plane, and v plane
#define FRAME_LF_COUNT 4
#define DEFAULT_DELTA_LF_MULTI 0
#define MAX_MODE_LF_DELTAS 2

#define DIST_PRECISION_BITS 4
#define DIST_PRECISION (1 << DIST_PRECISION_BITS)  // 16

#define PROFILE_BITS 3
// The following three profiles are currently defined.
// Profile 0.  8-bit and 10-bit 4:2:0 and 4:0:0 only.
// Profile 1.  8-bit and 10-bit 4:4:4
// Profile 2.  8-bit and 10-bit 4:2:2
//            12-bit  4:0:0, 4:2:2 and 4:4:4
// Since we have three bits for the profiles, it can be extended later.
TYPEDEF enum {
  PROFILE_0,
  PROFILE_1,
  PROFILE_2,
  MAX_PROFILES,
} SENUM1BYTE(BITSTREAM_PROFILE);

#define OP_POINTS_CNT_MINUS_1_BITS 5
#define OP_POINTS_IDC_BITS 12

// Note: Some enums use the attribute 'packed' to use smallest possible integer
// type, so that we can save memory when they are used in structs/arrays.

typedef enum ATTRIBUTE_PACKED {
  BLOCK_4X4,
  BLOCK_4X8,
  BLOCK_8X4,
  BLOCK_8X8,
  BLOCK_8X16,
  BLOCK_16X8,
  BLOCK_16X16,
  BLOCK_16X32,
  BLOCK_32X16,
  BLOCK_32X32,
  BLOCK_32X64,
  BLOCK_64X32,
  BLOCK_64X64,
  BLOCK_64X128,
  BLOCK_128X64,
  BLOCK_128X128,
  BLOCK_4X16,
  BLOCK_16X4,
  BLOCK_8X32,
  BLOCK_32X8,
  BLOCK_16X64,
  BLOCK_64X16,
  BLOCK_SIZES_ALL,
  BLOCK_SIZES = BLOCK_4X16,
  BLOCK_INVALID = 255,
  BLOCK_LARGEST = (BLOCK_SIZES - 1)
} BLOCK_SIZE2;

// 4X4, 8X8, 16X16, 32X32, 64X64, 128X128
#define SQR_BLOCK_SIZES 6

TYPEDEF enum {
  PARTITION_NONE,
  PARTITION_HORZ,
  PARTITION_VERT,
  PARTITION_SPLIT,
  PARTITION_HORZ_A,  // HORZ split and the top partition is split again
  PARTITION_HORZ_B,  // HORZ split and the bottom partition is split again
  PARTITION_VERT_A,  // VERT split and the left partition is split again
  PARTITION_VERT_B,  // VERT split and the right partition is split again
  PARTITION_HORZ_4,  // 4:1 horizontal partition
  PARTITION_VERT_4,  // 4:1 vertical partition
  EXT_PARTITION_TYPES,
  PARTITION_TYPES = PARTITION_SPLIT + 1,
  PARTITION_INVALID = 255
} UENUM1BYTE(PARTITION_TYPE);

typedef char PARTITION_CONTEXT;
#define PARTITION_PLOFFSET 4  // number of probability models per block size
#define PARTITION_BLOCK_SIZES 5
#define PARTITION_CONTEXTS (PARTITION_BLOCK_SIZES * PARTITION_PLOFFSET)

// block transform size
TYPEDEF enum {
  TX_4X4,             // 4x4 transform
  TX_8X8,             // 8x8 transform
  TX_16X16,           // 16x16 transform
  TX_32X32,           // 32x32 transform
  TX_64X64,           // 64x64 transform
  TX_4X8,             // 4x8 transform
  TX_8X4,             // 8x4 transform
  TX_8X16,            // 8x16 transform
  TX_16X8,            // 16x8 transform
  TX_16X32,           // 16x32 transform
  TX_32X16,           // 32x16 transform
  TX_32X64,           // 32x64 transform
  TX_64X32,           // 64x32 transform
  TX_4X16,            // 4x16 transform
  TX_16X4,            // 16x4 transform
  TX_8X32,            // 8x32 transform
  TX_32X8,            // 32x8 transform
  TX_16X64,           // 16x64 transform
  TX_64X16,           // 64x16 transform
  TX_SIZES_ALL,       // Includes rectangular transforms
  TX_SIZES = TX_4X8,  // Does NOT include rectangular transforms
  TX_SIZES_LARGEST = TX_64X64,
  TX_INVALID = 255  // Invalid transform size
} UENUM1BYTE(TX_SIZE);

#define TX_SIZE_LUMA_MIN (TX_4X4)
/* We don't need to code a transform size unless the allowed size is at least
   one more than the minimum. */
#define TX_SIZE_CTX_MIN (TX_SIZE_LUMA_MIN + 1)

// Maximum tx_size categories
#define MAX_TX_CATS (TX_SIZES - TX_SIZE_CTX_MIN)
#define MAX_TX_DEPTH 2

#define MAX_TX_SIZE_LOG2 (6)
#define MAX_TX_SIZE (1 << MAX_TX_SIZE_LOG2)
#define MIN_TX_SIZE_LOG2 2
#define MIN_TX_SIZE (1 << MIN_TX_SIZE_LOG2)
#define MAX_TX_SQUARE (MAX_TX_SIZE * MAX_TX_SIZE)

// Pad 4 extra columns to remove horizontal availability check.
#define TX_PAD_HOR_LOG2 2
#define TX_PAD_HOR 4
// Pad 6 extra rows (2 on top and 4 on bottom) to remove vertical availability
// check.
#define TX_PAD_TOP 0
#define TX_PAD_BOTTOM 4
#define TX_PAD_VER (TX_PAD_TOP + TX_PAD_BOTTOM)
// Pad 16 extra bytes to avoid reading overflow in SIMD optimization.
#define TX_PAD_END 16
#define TX_PAD_2D ((32 + TX_PAD_HOR) * (32 + TX_PAD_VER) + TX_PAD_END)

// Number of maxium size transform blocks in the maximum size superblock
#define MAX_TX_BLOCKS_IN_MAX_SB_LOG2 ((MAX_SB_SIZE_LOG2 - MAX_TX_SIZE_LOG2) * 2)
#define MAX_TX_BLOCKS_IN_MAX_SB (1 << MAX_TX_BLOCKS_IN_MAX_SB_LOG2)

// frame transform mode
TYPEDEF enum {
  ONLY_4X4,         // use only 4x4 transform
  TX_MODE_LARGEST,  // transform size is the largest possible for pu size
  TX_MODE_SELECT,   // transform specified for each block
  TX_MODES,
} UENUM1BYTE(TX_MODE);

// 1D tx types
TYPEDEF enum {
  DCT_1D,
  ADST_1D,
  FLIPADST_1D,
  IDTX_1D,
  TX_TYPES_1D,
} UENUM1BYTE(TX_TYPE_1D);

TYPEDEF enum {
  DCT_DCT,            // DCT in both horizontal and vertical
  ADST_DCT,           // ADST in vertical, DCT in horizontal
  DCT_ADST,           // DCT in vertical, ADST in horizontal
  ADST_ADST,          // ADST in both directions
  FLIPADST_DCT,       // FLIPADST in vertical, DCT in horizontal
  DCT_FLIPADST,       // DCT in vertical, FLIPADST in horizontal
  FLIPADST_FLIPADST,  // FLIPADST in both directions
  ADST_FLIPADST,      // ADST in vertical, FLIPADST in horizontal
  FLIPADST_ADST,      // FLIPADST in vertical, ADST in horizontal
  IDTX,               // Identity in both directions
  V_DCT,              // DCT in vertical, identity in horizontal
  H_DCT,              // Identity in vertical, DCT in horizontal
  V_ADST,             // ADST in vertical, identity in horizontal
  H_ADST,             // Identity in vertical, ADST in horizontal
  V_FLIPADST,         // FLIPADST in vertical, identity in horizontal
  H_FLIPADST,         // Identity in vertical, FLIPADST in horizontal
  TX_TYPES,
} UENUM1BYTE(TX_TYPE);

TYPEDEF enum {
  REG_REG,
  REG_SMOOTH,
  REG_SHARP,
  SMOOTH_REG,
  SMOOTH_SMOOTH,
  SMOOTH_SHARP,
  SHARP_REG,
  SHARP_SMOOTH,
  SHARP_SHARP,
} UENUM1BYTE(DUAL_FILTER_TYPE);

TYPEDEF enum {
  // DCT only
  EXT_TX_SET_DCTONLY,
  // DCT + Identity only
  EXT_TX_SET_DCT_IDTX,
  // Discrete Trig transforms w/o flip (4) + Identity (1)
  EXT_TX_SET_DTT4_IDTX,
  // Discrete Trig transforms w/o flip (4) + Identity (1) + 1D Hor/vert DCT (2)
  EXT_TX_SET_DTT4_IDTX_1DDCT,
  // Discrete Trig transforms w/ flip (9) + Identity (1) + 1D Hor/Ver DCT (2)
  EXT_TX_SET_DTT9_IDTX_1DDCT,
  // Discrete Trig transforms w/ flip (9) + Identity (1) + 1D Hor/Ver (6)
  EXT_TX_SET_ALL16,
  EXT_TX_SET_TYPES
} UENUM1BYTE(TxSetType);

#define IS_2D_TRANSFORM(tx_type) (tx_type < IDTX)

#define EXT_TX_SIZES 4       // number of sizes that use extended transforms
#define EXT_TX_SETS_INTER 4  // Sets of transform selections for INTER
#define EXT_TX_SETS_INTRA 3  // Sets of transform selections for INTRA

TYPEDEF enum {
  AOM_LAST_FLAG = 1 << 0,
  AOM_LAST2_FLAG = 1 << 1,
  AOM_LAST3_FLAG = 1 << 2,
  AOM_GOLD_FLAG = 1 << 3,
  AOM_BWD_FLAG = 1 << 4,
  AOM_ALT2_FLAG = 1 << 5,
  AOM_ALT_FLAG = 1 << 6,
  AOM_REFFRAME_ALL = (1 << 7) - 1
} UENUM1BYTE(AOM_REFFRAME);

TYPEDEF enum {
  UNIDIR_COMP_REFERENCE,
  BIDIR_COMP_REFERENCE,
  COMP_REFERENCE_TYPES,
} UENUM1BYTE(COMP_REFERENCE_TYPE);

/*enum { PLANE_TYPE_Y, PLANE_TYPE_UV, PLANE_TYPES } UENUM1BYTE(PLANE_TYPE);*/

#define CFL_ALPHABET_SIZE_LOG2 4
#define CFL_ALPHABET_SIZE (1 << CFL_ALPHABET_SIZE_LOG2)
#define CFL_MAGS_SIZE ((2 << CFL_ALPHABET_SIZE_LOG2) + 1)
#define CFL_IDX_U(idx) (idx >> CFL_ALPHABET_SIZE_LOG2)
#define CFL_IDX_V(idx) (idx & (CFL_ALPHABET_SIZE - 1))

/*enum { CFL_PRED_U, CFL_PRED_V, CFL_PRED_PLANES } UENUM1BYTE(CFL_PRED_TYPE);*/

TYPEDEF enum {
  CFL_SIGN_ZERO,
  CFL_SIGN_NEG,
  CFL_SIGN_POS,
  CFL_SIGNS
} UENUM1BYTE(CFL_SIGN_TYPE);

TYPEDEF enum {
  CFL_DISALLOWED,
  CFL_ALLOWED,
  CFL_ALLOWED_TYPES
} UENUM1BYTE(CFL_ALLOWED_TYPE);

// CFL_SIGN_ZERO,CFL_SIGN_ZERO is invalid
#define CFL_JOINT_SIGNS (CFL_SIGNS * CFL_SIGNS - 1)
// CFL_SIGN_U is equivalent to (js + 1) / 3 for js in 0 to 8
#define CFL_SIGN_U(js) (((js + 1) * 11) >> 5)
// CFL_SIGN_V is equivalent to (js + 1) % 3 for js in 0 to 8
#define CFL_SIGN_V(js) ((js + 1) - CFL_SIGNS * CFL_SIGN_U(js))

// There is no context when the alpha for a given plane is zero.
// So there are 2 fewer contexts than joint signs.
#define CFL_ALPHA_CONTEXTS (CFL_JOINT_SIGNS + 1 - CFL_SIGNS)
#define CFL_CONTEXT_U(js) (js + 1 - CFL_SIGNS)
// Also, the contexts are symmetric under swapping the planes.
#define CFL_CONTEXT_V(js) \
  (CFL_SIGN_V(js) * CFL_SIGNS + CFL_SIGN_U(js) - CFL_SIGNS)

TYPEDEF enum {
  PALETTE_MAP,
  COLOR_MAP_TYPES,
} UENUM1BYTE(COLOR_MAP_TYPE);

TYPEDEF enum {
  TWO_COLORS,
  THREE_COLORS,
  FOUR_COLORS,
  FIVE_COLORS,
  SIX_COLORS,
  SEVEN_COLORS,
  EIGHT_COLORS,
  PALETTE_SIZES
} UENUM1BYTE(PALETTE_SIZE);

TYPEDEF enum {
  PALETTE_COLOR_ONE,
  PALETTE_COLOR_TWO,
  PALETTE_COLOR_THREE,
  PALETTE_COLOR_FOUR,
  PALETTE_COLOR_FIVE,
  PALETTE_COLOR_SIX,
  PALETTE_COLOR_SEVEN,
  PALETTE_COLOR_EIGHT,
  PALETTE_COLORS
} UENUM1BYTE(PALETTE_COLOR);

// Note: All directional predictors must be between V_PRED and D67_PRED (both
// inclusive).
TYPEDEF enum {
  DC_PRED,        // Average of above and left pixels
  V_PRED,         // Vertical
  H_PRED,         // Horizontal
  D45_PRED,       // Directional 45  degree
  D135_PRED,      // Directional 135 degree
  D113_PRED,      // Directional 113 degree
  D157_PRED,      // Directional 157 degree
  D203_PRED,      // Directional 203 degree
  D67_PRED,       // Directional 67  degree
  SMOOTH_PRED,    // Combination of horizontal and vertical interpolation
  SMOOTH_V_PRED,  // Vertical interpolation
  SMOOTH_H_PRED,  // Horizontal interpolation
  PAETH_PRED,     // Predict from the direction of smallest gradient
  NEARESTMV,
  NEARMV,
  GLOBALMV,
  NEWMV,
  // Compound ref compound modes
  NEAREST_NEARESTMV,
  NEAR_NEARMV,
  NEAREST_NEWMV,
  NEW_NEARESTMV,
  NEAR_NEWMV,
  NEW_NEARMV,
  GLOBAL_GLOBALMV,
  NEW_NEWMV,
  MB_MODE_COUNT,
  INTRA_MODE_START = DC_PRED,
  INTRA_MODE_END = NEARESTMV,
  INTRA_MODE_NUM = INTRA_MODE_END - INTRA_MODE_START,
  SINGLE_INTER_MODE_START = NEARESTMV,
  SINGLE_INTER_MODE_END = NEAREST_NEARESTMV,
  SINGLE_INTER_MODE_NUM = SINGLE_INTER_MODE_END - SINGLE_INTER_MODE_START,
  COMP_INTER_MODE_START = NEAREST_NEARESTMV,
  COMP_INTER_MODE_END = MB_MODE_COUNT,
  COMP_INTER_MODE_NUM = COMP_INTER_MODE_END - COMP_INTER_MODE_START,
  INTER_MODE_START = NEARESTMV,
  INTER_MODE_END = MB_MODE_COUNT,
  INTRA_MODES = PAETH_PRED + 1,  // PAETH_PRED has to be the last intra mode.
  INTRA_INVALID = MB_MODE_COUNT  // For uv_mode in inter blocks
} UENUM1BYTE(PREDICTION_MODE);

// TODO(ltrudeau) Do we really want to pack this?
// TODO(ltrudeau) Do we match with PREDICTION_MODE?
TYPEDEF enum {
  UV_DC_PRED,        // Average of above and left pixels
  UV_V_PRED,         // Vertical
  UV_H_PRED,         // Horizontal
  UV_D45_PRED,       // Directional 45  degree
  UV_D135_PRED,      // Directional 135 degree
  UV_D113_PRED,      // Directional 113 degree
  UV_D157_PRED,      // Directional 157 degree
  UV_D203_PRED,      // Directional 203 degree
  UV_D67_PRED,       // Directional 67  degree
  UV_SMOOTH_PRED,    // Combination of horizontal and vertical interpolation
  UV_SMOOTH_V_PRED,  // Vertical interpolation
  UV_SMOOTH_H_PRED,  // Horizontal interpolation
  UV_PAETH_PRED,     // Predict from the direction of smallest gradient
  UV_CFL_PRED,       // Chroma-from-Luma
  UV_INTRA_MODES,
  UV_MODE_INVALID,  // For uv_mode in inter blocks
} UENUM1BYTE(UV_PREDICTION_MODE);

TYPEDEF enum {
  SIMPLE_TRANSLATION,
  OBMC_CAUSAL,    // 2-sided OBMC
  WARPED_CAUSAL,  // 2-sided WARPED
  MOTION_MODES
} UENUM1BYTE(MOTION_MODE);

TYPEDEF enum {
  II_DC_PRED,
  II_V_PRED,
  II_H_PRED,
  II_SMOOTH_PRED,
  INTERINTRA_MODES
} UENUM1BYTE(INTERINTRA_MODE);

TYPEDEF enum {
  COMPOUND_AVERAGE,
  COMPOUND_DISTWTD,
  COMPOUND_WEDGE,
  COMPOUND_DIFFWTD,
  COMPOUND_TYPES,
  MASKED_COMPOUND_TYPES = 2,
} UENUM1BYTE(COMPOUND_TYPE);

TYPEDEF enum {
  FILTER_DC_PRED,
  FILTER_V_PRED,
  FILTER_H_PRED,
  FILTER_D157_PRED,
  FILTER_PAETH_PRED,
  FILTER_INTRA_MODES,
} UENUM1BYTE(FILTER_INTRA_MODE);

TYPEDEF enum {
  SEQ_LEVEL_2_0,
  SEQ_LEVEL_2_1,
  SEQ_LEVEL_2_2,
  SEQ_LEVEL_2_3,
  SEQ_LEVEL_3_0,
  SEQ_LEVEL_3_1,
  SEQ_LEVEL_3_2,
  SEQ_LEVEL_3_3,
  SEQ_LEVEL_4_0,
  SEQ_LEVEL_4_1,
  SEQ_LEVEL_4_2,
  SEQ_LEVEL_4_3,
  SEQ_LEVEL_5_0,
  SEQ_LEVEL_5_1,
  SEQ_LEVEL_5_2,
  SEQ_LEVEL_5_3,
  SEQ_LEVEL_6_0,
  SEQ_LEVEL_6_1,
  SEQ_LEVEL_6_2,
  SEQ_LEVEL_6_3,
  SEQ_LEVEL_7_0,
  SEQ_LEVEL_7_1,
  SEQ_LEVEL_7_2,
  SEQ_LEVEL_7_3,
  SEQ_LEVELS,
  SEQ_LEVEL_MAX = 31
} UENUM1BYTE(AV1_LEVEL);

#define LEVEL_BITS 5

#define DIRECTIONAL_MODES 8
#define MAX_ANGLE_DELTA 3
#define ANGLE_STEP 3

#define INTER_MODES (1 + NEWMV - NEARESTMV)

#define INTER_COMPOUND_MODES (1 + NEW_NEWMV - NEAREST_NEARESTMV)

#define SKIP_CONTEXTS 3
#define SKIP_MODE_CONTEXTS 3

#define COMP_INDEX_CONTEXTS 6
#define COMP_GROUP_IDX_CONTEXTS 6

#define NMV_CONTEXTS 3

#define NEWMV_MODE_CONTEXTS 6
#define GLOBALMV_MODE_CONTEXTS 2
#define REFMV_MODE_CONTEXTS 6
#define DRL_MODE_CONTEXTS 3

#define GLOBALMV_OFFSET 3
#define REFMV_OFFSET 4

#define NEWMV_CTX_MASK ((1 << GLOBALMV_OFFSET) - 1)
#define GLOBALMV_CTX_MASK ((1 << (REFMV_OFFSET - GLOBALMV_OFFSET)) - 1)
#define REFMV_CTX_MASK ((1 << (8 - REFMV_OFFSET)) - 1)

#define COMP_NEWMV_CTXS 5
#define INTER_MODE_CONTEXTS 8

#define DELTA_Q_SMALL 3
#define DELTA_Q_PROBS (DELTA_Q_SMALL)
#define DEFAULT_DELTA_Q_RES_PERCEPTUAL 4
#define DEFAULT_DELTA_Q_RES_OBJECTIVE 4

#define DELTA_LF_SMALL 3
#define DELTA_LF_PROBS (DELTA_LF_SMALL)
#define DEFAULT_DELTA_LF_RES 2

/* Segment Feature Masks */
#define MAX_MV_REF_CANDIDATES 2

#define MAX_REF_MV_STACK_SIZE 8
#define REF_CAT_LEVEL 640

#define INTRA_INTER_CONTEXTS 4
#define COMP_INTER_CONTEXTS 5
#define REF_CONTEXTS 3

#define COMP_REF_TYPE_CONTEXTS 5
#define UNI_COMP_REF_CONTEXTS 3

#define TXFM_PARTITION_CONTEXTS ((TX_SIZES - TX_8X8) * 6 - 3)
#ifdef ORI_CODE
typedef uint8_t TXFM_CONTEXT;
#endif
// An enum for single reference types (and some derived values).
enum {
  NONE_FRAME = -1,
  INTRA_FRAME,
  LAST_FRAME,
  LAST2_FRAME,
  LAST3_FRAME,
  GOLDEN_FRAME,
  BWDREF_FRAME,
  ALTREF2_FRAME,
  ALTREF_FRAME,
  REF_FRAMES,

  // Extra/scratch reference frame. It may be:
  // - used to update the ALTREF2_FRAME ref (see lshift_bwd_ref_frames()), or
  // - updated from ALTREF2_FRAME ref (see rshift_bwd_ref_frames()).
  EXTREF_FRAME = REF_FRAMES,

  // Number of inter (non-intra) reference types.
  INTER_REFS_PER_FRAME = ALTREF_FRAME - LAST_FRAME + 1,

  // Number of forward (aka past) reference types.
  FWD_REFS = GOLDEN_FRAME - LAST_FRAME + 1,

  // Number of backward (aka future) reference types.
  BWD_REFS = ALTREF_FRAME - BWDREF_FRAME + 1,

  SINGLE_REFS = FWD_REFS + BWD_REFS,
};

#define REF_FRAMES_LOG2 3

// REF_FRAMES for the cm->ref_frame_map array, 1 scratch frame for the new
// frame in cm->cur_frame, INTER_REFS_PER_FRAME for scaled references on the
// encoder in the cpi->scaled_ref_buf array.
#define FRAME_BUFFERS (REF_FRAMES + 1 + INTER_REFS_PER_FRAME)

#define FWD_RF_OFFSET(ref) (ref - LAST_FRAME)
#define BWD_RF_OFFSET(ref) (ref - BWDREF_FRAME)

TYPEDEF enum {
  LAST_LAST2_FRAMES,      // { LAST_FRAME, LAST2_FRAME }
  LAST_LAST3_FRAMES,      // { LAST_FRAME, LAST3_FRAME }
  LAST_GOLDEN_FRAMES,     // { LAST_FRAME, GOLDEN_FRAME }
  BWDREF_ALTREF_FRAMES,   // { BWDREF_FRAME, ALTREF_FRAME }
  LAST2_LAST3_FRAMES,     // { LAST2_FRAME, LAST3_FRAME }
  LAST2_GOLDEN_FRAMES,    // { LAST2_FRAME, GOLDEN_FRAME }
  LAST3_GOLDEN_FRAMES,    // { LAST3_FRAME, GOLDEN_FRAME }
  BWDREF_ALTREF2_FRAMES,  // { BWDREF_FRAME, ALTREF2_FRAME }
  ALTREF2_ALTREF_FRAMES,  // { ALTREF2_FRAME, ALTREF_FRAME }
  TOTAL_UNIDIR_COMP_REFS,
  // NOTE: UNIDIR_COMP_REFS is the number of uni-directional reference pairs
  //       that are explicitly signaled.
  UNIDIR_COMP_REFS = BWDREF_ALTREF_FRAMES + 1,
} UENUM1BYTE(UNIDIR_COMP_REF);

#define TOTAL_COMP_REFS (FWD_REFS * BWD_REFS + TOTAL_UNIDIR_COMP_REFS)

#define COMP_REFS (FWD_REFS * BWD_REFS + UNIDIR_COMP_REFS)

// NOTE: A limited number of unidirectional reference pairs can be signalled for
//       compound prediction. The use of skip mode, on the other hand, makes it
//       possible to have a reference pair not listed for explicit signaling.
#define MODE_CTX_REF_FRAMES (REF_FRAMES + TOTAL_COMP_REFS)

// Note: It includes single and compound references. So, it can take values from
// NONE_FRAME to (MODE_CTX_REF_FRAMES - 1). Hence, it is not defined as an enum.
typedef int8_t MV_REFERENCE_FRAME;

TYPEDEF enum {
  RESTORE_NONE,
  RESTORE_WIENER,
  RESTORE_SGRPROJ,
  RESTORE_SWITCHABLE,
  RESTORE_SWITCHABLE_TYPES = RESTORE_SWITCHABLE,
  RESTORE_TYPES = 4,
} UENUM1BYTE(RestorationType);

// Picture prediction structures (0-12 are predefined) in scalability metadata.
TYPEDEF enum {
  SCALABILITY_L1T2 = 0,
  SCALABILITY_L1T3 = 1,
  SCALABILITY_L2T1 = 2,
  SCALABILITY_L2T2 = 3,
  SCALABILITY_L2T3 = 4,
  SCALABILITY_S2T1 = 5,
  SCALABILITY_S2T2 = 6,
  SCALABILITY_S2T3 = 7,
  SCALABILITY_L2T1h = 8,
  SCALABILITY_L2T2h = 9,
  SCALABILITY_L2T3h = 10,
  SCALABILITY_S2T1h = 11,
  SCALABILITY_S2T2h = 12,
  SCALABILITY_S2T3h = 13,
  SCALABILITY_SS = 14
} UENUM1BYTE(SCALABILITY_STRUCTURES);

#define SUPERRES_SCALE_BITS 3
#define SUPERRES_SCALE_DENOMINATOR_MIN (SCALE_NUMERATOR + 1)

// In large_scale_tile coding, external references are used.
#define MAX_EXTERNAL_REFERENCES 128
#define MAX_TILES 512


#define CONFIG_MULTITHREAD 0
#define CONFIG_ENTROPY_STATS 0

#define CONFIG_MAX_DECODE_PROFILE 2

/*
from:
seg_common.h
*/
#ifdef ORI_CODE

#define MAX_SEGMENTS 8
#define SEG_TREE_PROBS (MAX_SEGMENTS - 1)

#define SEG_TEMPORAL_PRED_CTXS 3
#define SPATIAL_PREDICTION_PROBS 3

enum {
  SEG_LVL_ALT_Q,       // Use alternate Quantizer ....
  SEG_LVL_ALT_LF_Y_V,  // Use alternate loop filter value on y plane vertical
  SEG_LVL_ALT_LF_Y_H,  // Use alternate loop filter value on y plane horizontal
  SEG_LVL_ALT_LF_U,    // Use alternate loop filter value on u plane
  SEG_LVL_ALT_LF_V,    // Use alternate loop filter value on v plane
  SEG_LVL_REF_FRAME,   // Optional Segment reference frame
  SEG_LVL_SKIP,        // Optional Segment (0,0) + skip mode
  SEG_LVL_GLOBALMV,
  SEG_LVL_MAX
} UENUM1BYTE(SEG_LVL_FEATURES);

struct segmentation {
  uint8_t enabled;
  uint8_t update_map;
  uint8_t update_data;
  uint8_t temporal_update;

  int16_t feature_data[MAX_SEGMENTS][SEG_LVL_MAX];
  unsigned int feature_mask[MAX_SEGMENTS];
  int last_active_segid;  // The highest numbered segment id that has some
                          // enabled feature.
  uint8_t segid_preskip;  // Whether the segment id will be read before the
                          // skip syntax element.
                          // 1: the segment id will be read first.
                          // 0: the skip syntax element will be read first.
};

/*
from av1_loopfilter.h
*/
#define MAX_LOOP_FILTER 63


/* from
quant_common.h:
*/
#define MAXQ 255

#endif

/*
from:
aom/av1/common/common.h
*/
#define av1_zero(dest) memset(&(dest), 0, sizeof(dest))
#define av1_zero_array(dest, n) memset(dest, 0, n * sizeof(*(dest)))
/*
from:
aom/av1/common/alloccommon.h
*/
#define INVALID_IDX -1  // Invalid buffer index.

/*
from:
aom/av1/common/timing.h
*/
typedef struct aom_timing {
  uint32_t num_units_in_display_tick;
  uint32_t time_scale;
  int equal_picture_interval;
  uint32_t num_ticks_per_picture;
} aom_timing_info_t;

typedef struct aom_dec_model_info {
  uint32_t num_units_in_decoding_tick;
  int encoder_decoder_buffer_delay_length;
  int buffer_removal_time_length;
  int frame_presentation_time_length;
} aom_dec_model_info_t;

typedef struct aom_dec_model_op_parameters {
  int decoder_model_param_present_flag;
  int64_t bitrate;
  int64_t buffer_size;
  uint32_t decoder_buffer_delay;
  uint32_t encoder_buffer_delay;
  int low_delay_mode_flag;
  int display_model_param_present_flag;
  int initial_display_delay;
} aom_dec_model_op_parameters_t;

typedef struct aom_op_timing_info_t {
  uint32_t buffer_removal_time;
} aom_op_timing_info_t;
/*
from:
aom/aom_codec.h
*/
/*!\brief OBU types. */
typedef enum {
  OBU_SEQUENCE_HEADER = 1,
  OBU_TEMPORAL_DELIMITER = 2,
  OBU_FRAME_HEADER = 3,
  OBU_TILE_GROUP = 4,
  OBU_METADATA = 5,
  OBU_FRAME = 6,
  OBU_REDUNDANT_FRAME_HEADER = 7,
  OBU_TILE_LIST = 8,
  OBU_PADDING = 15,
} OBU_TYPE;

typedef enum aom_bit_depth {
  AOM_BITS_8 = 8,   /**<  8 bits */
  AOM_BITS_10 = 10, /**< 10 bits */
  AOM_BITS_12 = 12, /**< 12 bits */
} aom_bit_depth_t;

/*!\brief Algorithm return codes */
typedef enum {
  /*!\brief Operation completed without error */
  AOM_CODEC_OK,

  /*!\brief Unspecified error */
  AOM_CODEC_ERROR,

  /*!\brief Memory operation failed */
  AOM_CODEC_MEM_ERROR,

  /*!\brief ABI version mismatch */
  AOM_CODEC_ABI_MISMATCH,

  /*!\brief Algorithm does not have required capability */
  AOM_CODEC_INCAPABLE,

  /*!\brief The given bitstream is not supported.
   *
   * The bitstream was unable to be parsed at the highest level. The decoder
   * is unable to proceed. This error \ref SHOULD be treated as fatal to the
   * stream. */
  AOM_CODEC_UNSUP_BITSTREAM,

  /*!\brief Encoded bitstream uses an unsupported feature
   *
   * The decoder does not implement a feature required by the encoder. This
   * return code should only be used for features that prevent future
   * pictures from being properly decoded. This error \ref MAY be treated as
   * fatal to the stream or \ref MAY be treated as fatal to the current GOP.
   */
  AOM_CODEC_UNSUP_FEATURE,

  /*!\brief The coded data for this stream is corrupt or incomplete
   *
   * There was a problem decoding the current frame.  This return code
   * should only be used for failures that prevent future pictures from
   * being properly decoded. This error \ref MAY be treated as fatal to the
   * stream or \ref MAY be treated as fatal to the current GOP. If decoding
   * is continued for the current GOP, artifacts may be present.
   */
  AOM_CODEC_CORRUPT_FRAME,

  /*!\brief An application-supplied parameter is not valid.
   *
   */
  AOM_CODEC_INVALID_PARAM,

  /*!\brief An iterator reached the end of list.
   *
   */
  AOM_CODEC_LIST_END

} aom_codec_err_t;

typedef struct cfg_options {
  /*!\brief Reflects if ext_partition should be enabled
   *
   * If this value is non-zero it enabled the feature
   */
  unsigned int ext_partition;
} cfg_options_t;

/*
from:
aom/av1/common/obu_util.h
*/
typedef struct {
  size_t size;  // Size (1 or 2 bytes) of the OBU header (including the
                // optional OBU extension header) in the bitstream.
  OBU_TYPE type;
  int has_size_field;
  int has_extension;
  // The following fields come from the OBU extension header and therefore are
  // only used if has_extension is true.
  int temporal_layer_id;
  int spatial_layer_id;
} ObuHeader;


/*
from:
aom/internal/aom_codec_internal.h
*/

struct aom_internal_error_info {
  aom_codec_err_t error_code;
  int has_detail;
  char detail[80];
  int setjmp;  // Boolean: whether 'jmp' is valid.
#ifdef ORI_CODE
  jmp_buf jmp;
#endif
};

/*
from:
aom/aom_frame_buffer.h
*/
typedef struct aom_codec_frame_buffer {
  uint8_t *data; /**< Pointer to the data buffer */
  size_t size;   /**< Size of data in bytes */
  void *priv;    /**< Frame's private data */
} aom_codec_frame_buffer_t;

/*
from:
aom/aom_image.h
*/
#define AOM_IMAGE_ABI_VERSION (5) /**<\hideinitializer*/

#define AOM_IMG_FMT_PLANAR 0x100  /**< Image is a planar format. */
#define AOM_IMG_FMT_UV_FLIP 0x200 /**< V plane precedes U in memory. */
/** 0x400 used to signal alpha channel, skipping for backwards compatibility. */
#define AOM_IMG_FMT_HIGHBITDEPTH 0x800 /**< Image uses 16bit framebuffer. */

/*!\brief List of supported image formats */
typedef enum aom_img_fmt {
  AOM_IMG_FMT_NONE,
  AOM_IMG_FMT_YV12 =
      AOM_IMG_FMT_PLANAR | AOM_IMG_FMT_UV_FLIP | 1, /**< planar YVU */
  AOM_IMG_FMT_I420 = AOM_IMG_FMT_PLANAR | 2,
  AOM_IMG_FMT_AOMYV12 = AOM_IMG_FMT_PLANAR | AOM_IMG_FMT_UV_FLIP |
                        3, /** < planar 4:2:0 format with aom color space */
  AOM_IMG_FMT_AOMI420 = AOM_IMG_FMT_PLANAR | 4,
  AOM_IMG_FMT_I422 = AOM_IMG_FMT_PLANAR | 5,
  AOM_IMG_FMT_I444 = AOM_IMG_FMT_PLANAR | 6,
  AOM_IMG_FMT_I42016 = AOM_IMG_FMT_I420 | AOM_IMG_FMT_HIGHBITDEPTH,
  AOM_IMG_FMT_YV1216 = AOM_IMG_FMT_YV12 | AOM_IMG_FMT_HIGHBITDEPTH,
  AOM_IMG_FMT_I42216 = AOM_IMG_FMT_I422 | AOM_IMG_FMT_HIGHBITDEPTH,
  AOM_IMG_FMT_I44416 = AOM_IMG_FMT_I444 | AOM_IMG_FMT_HIGHBITDEPTH,
} aom_img_fmt_t; /**< alias for enum aom_img_fmt */

/*!\brief List of supported color primaries */
typedef enum aom_color_primaries {
  AOM_CICP_CP_RESERVED_0 = 0,  /**< For future use */
  AOM_CICP_CP_BT_709 = 1,      /**< BT.709 */
  AOM_CICP_CP_UNSPECIFIED = 2, /**< Unspecified */
  AOM_CICP_CP_RESERVED_3 = 3,  /**< For future use */
  AOM_CICP_CP_BT_470_M = 4,    /**< BT.470 System M (historical) */
  AOM_CICP_CP_BT_470_B_G = 5,  /**< BT.470 System B, G (historical) */
  AOM_CICP_CP_BT_601 = 6,      /**< BT.601 */
  AOM_CICP_CP_SMPTE_240 = 7,   /**< SMPTE 240 */
  AOM_CICP_CP_GENERIC_FILM =
      8, /**< Generic film (color filters using illuminant C) */
  AOM_CICP_CP_BT_2020 = 9,      /**< BT.2020, BT.2100 */
  AOM_CICP_CP_XYZ = 10,         /**< SMPTE 428 (CIE 1921 XYZ) */
  AOM_CICP_CP_SMPTE_431 = 11,   /**< SMPTE RP 431-2 */
  AOM_CICP_CP_SMPTE_432 = 12,   /**< SMPTE EG 432-1  */
  AOM_CICP_CP_RESERVED_13 = 13, /**< For future use (values 13 - 21)  */
  AOM_CICP_CP_EBU_3213 = 22,    /**< EBU Tech. 3213-E  */
  AOM_CICP_CP_RESERVED_23 = 23  /**< For future use (values 23 - 255)  */
} aom_color_primaries_t;        /**< alias for enum aom_color_primaries */

/*!\brief List of supported transfer functions */
typedef enum aom_transfer_characteristics {
  AOM_CICP_TC_RESERVED_0 = 0,  /**< For future use */
  AOM_CICP_TC_BT_709 = 1,      /**< BT.709 */
  AOM_CICP_TC_UNSPECIFIED = 2, /**< Unspecified */
  AOM_CICP_TC_RESERVED_3 = 3,  /**< For future use */
  AOM_CICP_TC_BT_470_M = 4,    /**< BT.470 System M (historical)  */
  AOM_CICP_TC_BT_470_B_G = 5,  /**< BT.470 System B, G (historical) */
  AOM_CICP_TC_BT_601 = 6,      /**< BT.601 */
  AOM_CICP_TC_SMPTE_240 = 7,   /**< SMPTE 240 M */
  AOM_CICP_TC_LINEAR = 8,      /**< Linear */
  AOM_CICP_TC_LOG_100 = 9,     /**< Logarithmic (100 : 1 range) */
  AOM_CICP_TC_LOG_100_SQRT10 =
      10,                     /**< Logarithmic (100 * Sqrt(10) : 1 range) */
  AOM_CICP_TC_IEC_61966 = 11, /**< IEC 61966-2-4 */
  AOM_CICP_TC_BT_1361 = 12,   /**< BT.1361 */
  AOM_CICP_TC_SRGB = 13,      /**< sRGB or sYCC*/
  AOM_CICP_TC_BT_2020_10_BIT = 14, /**< BT.2020 10-bit systems */
  AOM_CICP_TC_BT_2020_12_BIT = 15, /**< BT.2020 12-bit systems */
  AOM_CICP_TC_SMPTE_2084 = 16,     /**< SMPTE ST 2084, ITU BT.2100 PQ */
  AOM_CICP_TC_SMPTE_428 = 17,      /**< SMPTE ST 428 */
  AOM_CICP_TC_HLG = 18,            /**< BT.2100 HLG, ARIB STD-B67 */
  AOM_CICP_TC_RESERVED_19 = 19     /**< For future use (values 19-255) */
} aom_transfer_characteristics_t;  /**< alias for enum aom_transfer_function */

/*!\brief List of supported matrix coefficients */
typedef enum aom_matrix_coefficients {
  AOM_CICP_MC_IDENTITY = 0,    /**< Identity matrix */
  AOM_CICP_MC_BT_709 = 1,      /**< BT.709 */
  AOM_CICP_MC_UNSPECIFIED = 2, /**< Unspecified */
  AOM_CICP_MC_RESERVED_3 = 3,  /**< For future use */
  AOM_CICP_MC_FCC = 4,         /**< US FCC 73.628 */
  AOM_CICP_MC_BT_470_B_G = 5,  /**< BT.470 System B, G (historical) */
  AOM_CICP_MC_BT_601 = 6,      /**< BT.601 */
  AOM_CICP_MC_SMPTE_240 = 7,   /**< SMPTE 240 M */
  AOM_CICP_MC_SMPTE_YCGCO = 8, /**< YCgCo */
  AOM_CICP_MC_BT_2020_NCL =
      9, /**< BT.2020 non-constant luminance, BT.2100 YCbCr  */
  AOM_CICP_MC_BT_2020_CL = 10, /**< BT.2020 constant luminance */
  AOM_CICP_MC_SMPTE_2085 = 11, /**< SMPTE ST 2085 YDzDx */
  AOM_CICP_MC_CHROMAT_NCL =
      12, /**< Chromaticity-derived non-constant luminance */
  AOM_CICP_MC_CHROMAT_CL = 13, /**< Chromaticity-derived constant luminance */
  AOM_CICP_MC_ICTCP = 14,      /**< BT.2100 ICtCp */
  AOM_CICP_MC_RESERVED_15 = 15 /**< For future use (values 15-255)  */
} aom_matrix_coefficients_t;

/*!\brief List of supported color range */
typedef enum aom_color_range {
  AOM_CR_STUDIO_RANGE = 0, /**< Y [16..235], UV [16..240] */
  AOM_CR_FULL_RANGE = 1    /**< YUV/RGB [0..255] */
} aom_color_range_t;       /**< alias for enum aom_color_range */

/*!\brief List of chroma sample positions */
typedef enum aom_chroma_sample_position {
  AOM_CSP_UNKNOWN = 0,          /**< Unknown */
  AOM_CSP_VERTICAL = 1,         /**< Horizontally co-located with luma(0, 0)*/
                                /**< sample, between two vertical samples */
  AOM_CSP_COLOCATED = 2,        /**< Co-located with luma(0, 0) sample */
  AOM_CSP_RESERVED = 3          /**< Reserved value */
} aom_chroma_sample_position_t; /**< alias for enum aom_transfer_function */

/*
from:
aom/aom_scale/yv12config.h
*/
typedef struct PIC_BUFFER_CONFIG_s {
  union {
    struct {
      int y_width;
      int uv_width;
    };
    int widths[2];
  };
  union {
    struct {
      int y_height;
      int uv_height;
    };
    int heights[2];
  };
  union {
    struct {
      int y_crop_width;
      int uv_crop_width;
    };
    int crop_widths[2];
  };
  union {
    struct {
      int y_crop_height;
      int uv_crop_height;
    };
    int crop_heights[2];
  };
  union {
    struct {
      int y_stride;
      int uv_stride;
    };
    int strides[2];
  };
  union {
    struct {
      uint8_t *y_buffer;
      uint8_t *u_buffer;
      uint8_t *v_buffer;
    };
    uint8_t *buffers[3];
  };

  // Indicate whether y_buffer, u_buffer, and v_buffer points to the internally
  // allocated memory or external buffers.
  int use_external_reference_buffers;
  // This is needed to store y_buffer, u_buffer, and v_buffer when set reference
  // uses an external refernece, and restore those buffer pointers after the
  // external reference frame is no longer used.
  uint8_t *store_buf_adr[3];

  // If the frame is stored in a 16-bit buffer, this stores an 8-bit version
  // for use in global motion detection. It is allocated on-demand.
  uint8_t *y_buffer_8bit;
  int buf_8bit_valid;

  uint8_t *buffer_alloc;
  size_t buffer_alloc_sz;
  int border;
  size_t frame_size;
  int subsampling_x;
  int subsampling_y;
  unsigned int bit_depth;
  aom_color_primaries_t color_primaries;
  aom_transfer_characteristics_t transfer_characteristics;
  aom_matrix_coefficients_t matrix_coefficients;
  uint8_t monochrome;
  aom_chroma_sample_position_t chroma_sample_position;
  aom_color_range_t color_range;
  int render_width;
  int render_height;

  int corrupted;
  int flags;

#ifdef AML
    int32_t index;
    int32_t decode_idx;
    int32_t slice_type;
    int32_t RefNum_L0;
    int32_t RefNum_L1;
    int32_t num_reorder_pic;
        int32_t stream_offset;
    uint8_t referenced;
    uint8_t output_mark;
    uint8_t recon_mark;
    uint8_t output_ready;
    uint8_t error_mark;
    /**/
    int32_t slice_idx;
    /*buffer*/
    uint32_t fgs_table_adr;
    uint32_t sfgs_table_phy;
    char *sfgs_table_ptr;
#ifdef AOM_AV1_MMU
    uint32_t header_adr;
#endif
#ifdef AOM_AV1_MMU_DW
    uint32_t header_dw_adr;
#endif
    uint32_t mpred_mv_wr_start_addr;
    uint32_t mc_y_adr;
    uint32_t mc_u_v_adr;
    int32_t mc_canvas_y;
    int32_t mc_canvas_u_v;

    int32_t lcu_total;
    /**/
    unsigned int order_hint;
#endif
#ifdef AML_DEVICE
     int mv_buf_index;
      unsigned long cma_alloc_addr;
      int BUF_index;
      int buf_size;
      int comp_body_size;
      unsigned int dw_y_adr;
      unsigned int dw_u_v_adr;
      int double_write_mode;
      int y_canvas_index;
      int uv_canvas_index;
      int vf_ref;
  struct canvas_config_s canvas_config[2];
    char *aux_data_buf;
    int aux_data_size;
    u32 pts;
  u64 pts64;
  /* picture qos infomation*/
  int max_qp;
  int avg_qp;
  int min_qp;
  int max_skip;
  int avg_skip;
  int min_skip;
  int max_mv;
  int min_mv;
  int avg_mv;
#endif
  u64 timestamp;
  u32 hw_decode_time;
  u32 frame_size2; // For frame base mode
  int ctx_buf_idx;
} PIC_BUFFER_CONFIG;

/*
from:
common/blockd.h
*/
TYPEDEF enum {
  KEY_FRAME = 0,
  INTER_FRAME = 1,
  INTRA_ONLY_FRAME = 2,  // replaces intra-only
  S_FRAME = 3,
  FRAME_TYPES,
} UENUM1BYTE(FRAME_TYPE);

/*from:
mv.h
*/
#ifdef ORI_CODE
typedef struct mv32 {
  int32_t row;
  int32_t col;
} MV32;
#endif
/*from:
 aom_filter.h
*/
#define SUBPEL_BITS 4
#define SUBPEL_MASK ((1 << SUBPEL_BITS) - 1)
#define SUBPEL_SHIFTS (1 << SUBPEL_BITS)
#define SUBPEL_TAPS 8

#define SCALE_SUBPEL_BITS 10
#define SCALE_SUBPEL_SHIFTS (1 << SCALE_SUBPEL_BITS)
#define SCALE_SUBPEL_MASK (SCALE_SUBPEL_SHIFTS - 1)
#define SCALE_EXTRA_BITS (SCALE_SUBPEL_BITS - SUBPEL_BITS)
#define SCALE_EXTRA_OFF ((1 << SCALE_EXTRA_BITS) / 2)

#define RS_SUBPEL_BITS 6
#define RS_SUBPEL_MASK ((1 << RS_SUBPEL_BITS) - 1)
#define RS_SCALE_SUBPEL_BITS 14
#define RS_SCALE_SUBPEL_MASK ((1 << RS_SCALE_SUBPEL_BITS) - 1)
#define RS_SCALE_EXTRA_BITS (RS_SCALE_SUBPEL_BITS - RS_SUBPEL_BITS)
#define RS_SCALE_EXTRA_OFF (1 << (RS_SCALE_EXTRA_BITS - 1))

/*from:
scale.h
*/
#define SCALE_NUMERATOR 8

#define REF_SCALE_SHIFT 14
#define REF_NO_SCALE (1 << REF_SCALE_SHIFT)
#define REF_INVALID_SCALE -1

struct scale_factors {
  int x_scale_fp;  // horizontal fixed point scale factor
  int y_scale_fp;  // vertical fixed point scale factor
  int x_step_q4;
  int y_step_q4;

  int (*scale_value_x)(int val, const struct scale_factors *sf);
  int (*scale_value_y)(int val, const struct scale_factors *sf);
#ifdef ORI_CODE
  // convolve_fn_ptr[subpel_x != 0][subpel_y != 0][is_compound]
  aom_convolve_fn_t convolve[2][2][2];
  aom_highbd_convolve_fn_t highbd_convolve[2][2][2];
#endif
};

#ifdef ORI_CODE
MV32 av1_scale_mv(const MV *mv, int x, int y, const struct scale_factors *sf);
#endif
void av1_setup_scale_factors_for_frame(struct scale_factors *sf, int other_w,
                                       int other_h, int this_w, int this_h);

static inline int av1_is_valid_scale(const struct scale_factors *sf) {
#ifdef ORI_CODE
  assert(sf != NULL);
#endif
  return sf->x_scale_fp != REF_INVALID_SCALE &&
         sf->y_scale_fp != REF_INVALID_SCALE;
}

static inline int av1_is_scaled(const struct scale_factors *sf) {
#ifdef ORI_CODE
  assert(sf != NULL);
#endif
  return av1_is_valid_scale(sf) &&
         (sf->x_scale_fp != REF_NO_SCALE || sf->y_scale_fp != REF_NO_SCALE);
}


/*
from:
common/onyxc_int.h
*/

#define CDEF_MAX_STRENGTHS 16

/* Constant values while waiting for the sequence header */
#define FRAME_ID_LENGTH 15
#define DELTA_FRAME_ID_LENGTH 14

#define FRAME_CONTEXTS (FRAME_BUFFERS + 1)
// Extra frame context which is always kept at default values
#define FRAME_CONTEXT_DEFAULTS (FRAME_CONTEXTS - 1)
#define PRIMARY_REF_BITS 3
#define PRIMARY_REF_NONE 7

#define NUM_PING_PONG_BUFFERS 2

#define MAX_NUM_TEMPORAL_LAYERS 8
#define MAX_NUM_SPATIAL_LAYERS 4
/* clang-format off */
// clang-format seems to think this is a pointer dereference and not a
// multiplication.
#define MAX_NUM_OPERATING_POINTS \
  MAX_NUM_TEMPORAL_LAYERS * MAX_NUM_SPATIAL_LAYERS
/* clang-format on*/

// TODO(jingning): Turning this on to set up transform coefficient
// processing timer.
#define TXCOEFF_TIMER 0
#define TXCOEFF_COST_TIMER 0

TYPEDEF enum {
  SINGLE_REFERENCE = 0,
  COMPOUND_REFERENCE = 1,
  REFERENCE_MODE_SELECT = 2,
  REFERENCE_MODES = 3,
} UENUM1BYTE(REFERENCE_MODE);

TYPEDEF enum {
  /**
   * Frame context updates are disabled
   */
  REFRESH_FRAME_CONTEXT_DISABLED,
  /**
   * Update frame context to values resulting from backward probability
   * updates based on entropy/counts in the decoded frame
   */
  REFRESH_FRAME_CONTEXT_BACKWARD,
} UENUM1BYTE(REFRESH_FRAME_CONTEXT_MODE);

#define MFMV_STACK_SIZE 3

#ifdef AML
#define MV_REF_SIZE 8
#endif

#ifdef ORI_CODE
typedef struct {
  int_mv mfmv0;
  uint8_t ref_frame_offset;
} TPL_MV_REF;
typedef struct {
  int_mv mv;
  MV_REFERENCE_FRAME ref_frame;
} MV_REF;
#endif

typedef struct RefCntBuffer_s {
  // For a RefCntBuffer, the following are reference-holding variables:
  // - cm->ref_frame_map[]
  // - cm->cur_frame
  // - cm->scaled_ref_buf[] (encoder only)
  // - cm->next_ref_frame_map[] (decoder only)
  // - pbi->output_frame_index[] (decoder only)
  // With that definition, 'ref_count' is the number of reference-holding
  // variables that are currently referencing this buffer.
  // For example:
  // - suppose this buffer is at index 'k' in the buffer pool, and
  // - Total 'n' of the variables / array elements above have value 'k' (that
  // is, they are pointing to buffer at index 'k').
  // Then, pool->frame_bufs[k].ref_count = n.
  int ref_count;

  unsigned int order_hint;
  unsigned int ref_order_hints[INTER_REFS_PER_FRAME];

  int intra_only;
  int segmentation_enabled;
  unsigned int segment_feature[8];
#ifdef AML
  int segmentation_update_map;
  int prev_segmentation_enabled;
  int seg_mi_rows;
  int seg_mi_cols;

  unsigned int seg_lf_info_y[8];
  unsigned int seg_lf_info_c[8];
  int8_t ref_deltas[REF_FRAMES];
  int8_t mode_deltas[MAX_MODE_LF_DELTAS];
#endif
  //MV_REF *mvs;
  uint8_t *seg_map;
#ifdef ORI_CODE
  struct segmentation seg;
#endif

  int mi_rows;
  int mi_cols;
  // Width and height give the size of the buffer (before any upscaling, unlike
  // the sizes that can be derived from the buf structure)
  int width;
  int height;
#ifdef ORI_CODE
  WarpedMotionParams global_motion[REF_FRAMES];
#endif
  int showable_frame;  // frame can be used as show existing frame in future
  uint8_t film_grain_params_present;
#ifdef ORI_CODE
  aom_film_grain_t film_grain_params;
#endif
#ifdef AML
  int dec_width;
  uint8_t film_grain_reg_valid;
  wait_queue_head_t wait_sfgs;
  atomic_t fgs_done;
  uint32_t film_grain_reg[FILM_GRAIN_REG_SIZE];
  uint32_t film_grain_ctrl;
#endif
  aom_codec_frame_buffer_t raw_frame_buffer;
  PIC_BUFFER_CONFIG buf;
#ifdef ORI_CODE
  hash_table hash_table;
#endif
  FRAME_TYPE frame_type;

  // This is only used in the encoder but needs to be indexed per ref frame
  // so it's extremely convenient to keep it here.
#ifdef ORI_CODE
  int interp_filter_selected[SWITCHABLE];
#endif
  // Inter frame reference frame delta for loop filter
#ifndef AML
  int8_t ref_deltas[REF_FRAMES];
#endif
#ifdef ORI_CODE
  // 0 = ZERO_MV, MV
  int8_t mode_deltas[MAX_MODE_LF_DELTAS];

  FRAME_CONTEXT frame_context;
#endif
  int show_frame;
} RefCntBuffer;

typedef struct BufferPool_s {
// Protect BufferPool from being accessed by several FrameWorkers at
// the same time during frame parallel decode.
// TODO(hkuang): Try to use atomic variable instead of locking the whole pool.
// TODO(wtc): Remove this. See
// https://chromium-review.googlesource.com/c/webm/libvpx/+/560630.
#if CONFIG_MULTITHREAD
  pthread_mutex_t pool_mutex;
#endif

  // Private data associated with the frame buffer callbacks.
  void *cb_priv;
#ifdef ORI_CODE
  aom_get_frame_buffer_cb_fn_t get_fb_cb;
  aom_release_frame_buffer_cb_fn_t release_fb_cb;
#endif
  RefCntBuffer frame_bufs[FRAME_BUFFERS];

#ifdef ORI_CODE
  // Frame buffers allocated internally by the codec.
  InternalFrameBufferList int_frame_buffers;
#endif
#ifdef AML_DEVICE
    spinlock_t lock;
#endif
} BufferPool;

typedef struct {
  int cdef_pri_damping;
  int cdef_sec_damping;
  int nb_cdef_strengths;
  int cdef_strengths[CDEF_MAX_STRENGTHS];
  int cdef_uv_strengths[CDEF_MAX_STRENGTHS];
  int cdef_bits;
} CdefInfo;

typedef struct {
  int delta_q_present_flag;
  // Resolution of delta quant
  int delta_q_res;
  int delta_lf_present_flag;
  // Resolution of delta lf level
  int delta_lf_res;
  // This is a flag for number of deltas of loop filter level
  // 0: use 1 delta, for y_vertical, y_horizontal, u, and v
  // 1: use separate deltas for each filter level
  int delta_lf_multi;
} DeltaQInfo;

typedef struct {
  int enable_order_hint;           // 0 - disable order hint, and related tools
  int order_hint_bits_minus_1;     // dist_wtd_comp, ref_frame_mvs,
                                   // frame_sign_bias
                                   // if 0, enable_dist_wtd_comp and
                                   // enable_ref_frame_mvs must be set as 0.
  int enable_dist_wtd_comp;        // 0 - disable dist-wtd compound modes
                                   // 1 - enable it
  int enable_ref_frame_mvs;        // 0 - disable ref frame mvs
                                   // 1 - enable it
} OrderHintInfo;

// Sequence header structure.
// Note: All syntax elements of sequence_header_obu that need to be
// bit-identical across multiple sequence headers must be part of this struct,
// so that consistency is checked by are_seq_headers_consistent() function.
typedef struct SequenceHeader {
  int num_bits_width;
  int num_bits_height;
  int max_frame_width;
  int max_frame_height;
  uint8_t frame_id_numbers_present_flag;
  int frame_id_length;
  int delta_frame_id_length;
  BLOCK_SIZE2 sb_size;  // Size of the superblock used for this frame
  int mib_size;        // Size of the superblock in units of MI blocks
  int mib_size_log2;   // Log 2 of above.

  OrderHintInfo order_hint_info;

  uint8_t force_screen_content_tools;  // 0 - force off
                                       // 1 - force on
                                       // 2 - adaptive
  uint8_t still_picture;               // Video is a single frame still picture
  uint8_t reduced_still_picture_hdr;   // Use reduced header for still picture
  uint8_t force_integer_mv;            // 0 - Don't force. MV can use subpel
                                       // 1 - force to integer
                                       // 2 - adaptive
  uint8_t enable_filter_intra;         // enables/disables filterintra
  uint8_t enable_intra_edge_filter;    // enables/disables edge upsampling
  uint8_t enable_interintra_compound;  // enables/disables interintra_compound
  uint8_t enable_masked_compound;      // enables/disables masked compound
  uint8_t enable_dual_filter;          // 0 - disable dual interpolation filter
                                       // 1 - enable vert/horz filter selection
  uint8_t enable_warped_motion;        // 0 - disable warp for the sequence
                                       // 1 - enable warp for the sequence
  uint8_t enable_superres;             // 0 - Disable superres for the sequence
                                       //     and no frame level superres flag
                                       // 1 - Enable superres for the sequence
                                       //     enable per-frame superres flag
  uint8_t enable_cdef;                 // To turn on/off CDEF
  uint8_t enable_restoration;          // To turn on/off loop restoration
  BITSTREAM_PROFILE profile;

  // Operating point info.
  int operating_points_cnt_minus_1;
  int operating_point_idc[MAX_NUM_OPERATING_POINTS];
  uint8_t display_model_info_present_flag;
  uint8_t decoder_model_info_present_flag;
  AV1_LEVEL seq_level_idx[MAX_NUM_OPERATING_POINTS];
  uint8_t tier[MAX_NUM_OPERATING_POINTS];  // seq_tier in the spec. One bit: 0
                                           // or 1.

  // Color config.
  aom_bit_depth_t bit_depth;  // AOM_BITS_8 in profile 0 or 1,
                              // AOM_BITS_10 or AOM_BITS_12 in profile 2 or 3.
  uint8_t use_highbitdepth;   // If true, we need to use 16bit frame buffers.
  uint8_t monochrome;         // Monochorme video
  aom_color_primaries_t color_primaries;
  aom_transfer_characteristics_t transfer_characteristics;
  aom_matrix_coefficients_t matrix_coefficients;
  int color_range;
  int subsampling_x;          // Chroma subsampling for x
  int subsampling_y;          // Chroma subsampling for y
  aom_chroma_sample_position_t chroma_sample_position;
  uint8_t separate_uv_delta_q;
  uint8_t film_gry_dequant_QTXain_params_present;
} SequenceHeader;

typedef struct {
    int skip_mode_allowed;
    int skip_mode_flag;
    int ref_frame_idx_0;
    int ref_frame_idx_1;
} SkipModeInfo;

typedef struct {
  FRAME_TYPE frame_type;
  REFERENCE_MODE reference_mode;

  unsigned int order_hint;
  unsigned int frame_number;
  SkipModeInfo skip_mode_info;
  int refresh_frame_flags;  // Which ref frames are overwritten by this frame
  int frame_refs_short_signaling;
} CurrentFrame;

typedef struct AV1_Common_s {
  CurrentFrame current_frame;
  struct aom_internal_error_info error;
  int width;
  int height;
  int render_width;
  int render_height;
  int timing_info_present;
  aom_timing_info_t timing_info;
  int buffer_removal_time_present;
  aom_dec_model_info_t buffer_model;
  aom_dec_model_op_parameters_t op_params[MAX_NUM_OPERATING_POINTS + 1];
  aom_op_timing_info_t op_frame_timing[MAX_NUM_OPERATING_POINTS + 1];
  uint32_t frame_presentation_time;

  int context_update_tile_id;
#ifdef SUPPORT_SCALE_FACTOR
  // Scale of the current frame with respect to itself.
  struct scale_factors sf_identity;
#endif
  RefCntBuffer *prev_frame;

  // TODO(hkuang): Combine this with cur_buf in macroblockd.
  RefCntBuffer *cur_frame;

  // For encoder, we have a two-level mapping from reference frame type to the
  // corresponding buffer in the buffer pool:
  // * 'remapped_ref_idx[i - 1]' maps reference type 'i' (range: LAST_FRAME ...
  // EXTREF_FRAME) to a remapped index 'j' (in range: 0 ... REF_FRAMES - 1)
  // * Later, 'cm->ref_frame_map[j]' maps the remapped index 'j' to a pointer to
  // the reference counted buffer structure RefCntBuffer, taken from the buffer
  // pool cm->buffer_pool->frame_bufs.
  //
  // LAST_FRAME,                        ...,      EXTREF_FRAME
  //      |                                           |
  //      v                                           v
  // remapped_ref_idx[LAST_FRAME - 1],  ...,  remapped_ref_idx[EXTREF_FRAME - 1]
  //      |                                           |
  //      v                                           v
  // ref_frame_map[],                   ...,     ref_frame_map[]
  //
  // Note: INTRA_FRAME always refers to the current frame, so there's no need to
  // have a remapped index for the same.
  int remapped_ref_idx[REF_FRAMES];

#ifdef SUPPORT_SCALE_FACTOR
  struct scale_factors ref_scale_factors[REF_FRAMES];
#endif
  // For decoder, ref_frame_map[i] maps reference type 'i' to a pointer to
  // the buffer in the buffer pool 'cm->buffer_pool.frame_bufs'.
  // For encoder, ref_frame_map[j] (where j = remapped_ref_idx[i]) maps
  // remapped reference index 'j' (that is, original reference type 'i') to
  // a pointer to the buffer in the buffer pool 'cm->buffer_pool.frame_bufs'.
  RefCntBuffer *ref_frame_map[REF_FRAMES];

  // Prepare ref_frame_map for the next frame.
  // Only used in frame parallel decode.
  RefCntBuffer *next_ref_frame_map[REF_FRAMES];
#ifdef AML
  RefCntBuffer *next_used_ref_frame_map[REF_FRAMES];
#endif
  FRAME_TYPE last_frame_type; /* last frame's frame type for motion search.*/

  int show_frame;
  int showable_frame;  // frame can be used as show existing frame in future
  int show_existing_frame;

  uint8_t disable_cdf_update;
  int allow_high_precision_mv;
  uint8_t cur_frame_force_integer_mv;  // 0 the default in AOM, 1 only integer

  uint8_t allow_screen_content_tools;
  int allow_intrabc;
  int allow_warped_motion;

  // MBs, mb_rows/cols is in 16-pixel units; mi_rows/cols is in
  // MB_MODE_INFO (8-pixel) units.
  int MBs;
  int mb_rows, mi_rows;
  int mb_cols, mi_cols;
  int mi_stride;

  /* profile settings */
  TX_MODE tx_mode;

#if CONFIG_ENTROPY_STATS
  int coef_cdf_category;
#endif

  int base_qindex;
  int y_dc_delta_q;
  int u_dc_delta_q;
  int v_dc_delta_q;
  int u_ac_delta_q;
  int v_ac_delta_q;

#ifdef ORI_CODE
  // The dequantizers below are true dequantizers used only in the
  // dequantization process.  They have the same coefficient
  // shift/scale as TX.
  int16_t y_dequant_QTX[MAX_SEGMENTS][2];
  int16_t u_dequant_QTX[MAX_SEGMENTS][2];
  int16_t v_dequant_QTX[MAX_SEGMENTS][2];

  // Global quant matrix tables
  const qm_val_t *giqmatrix[NUM_QM_LEVELS][3][TX_SIZES_ALL];
  const qm_val_t *gqmatrix[NUM_QM_LEVELS][3][TX_SIZES_ALL];

  // Local quant matrix tables for each frame
  const qm_val_t *y_iqmatrix[MAX_SEGMENTS][TX_SIZES_ALL];
  const qm_val_t *u_iqmatrix[MAX_SEGMENTS][TX_SIZES_ALL];
  const qm_val_t *v_iqmatrix[MAX_SEGMENTS][TX_SIZES_ALL];
#endif
  // Encoder
  int using_qmatrix;
  int qm_y;
  int qm_u;
  int qm_v;
  int min_qmlevel;
  int max_qmlevel;
  int use_quant_b_adapt;

  /* We allocate a MB_MODE_INFO struct for each macroblock, together with
     an extra row on top and column on the left to simplify prediction. */
  int mi_alloc_size;

#ifdef ORI_CODE
  MB_MODE_INFO *mip; /* Base of allocated array */
  MB_MODE_INFO *mi;  /* Corresponds to upper left visible macroblock */

  // TODO(agrange): Move prev_mi into encoder structure.
  // prev_mip and prev_mi will only be allocated in encoder.
  MB_MODE_INFO *prev_mip; /* MB_MODE_INFO array 'mip' from last decoded frame */
  MB_MODE_INFO *prev_mi;  /* 'mi' from last frame (points into prev_mip) */

  // Separate mi functions between encoder and decoder.
  int (*alloc_mi)(struct AV1Common *cm, int mi_size);
  void (*free_mi)(struct AV1Common *cm);
  void (*setup_mi)(struct AV1Common *cm);

  // Grid of pointers to 8x8 MB_MODE_INFO structs.  Any 8x8 not in the visible
  // area will be NULL.
  MB_MODE_INFO **mi_grid_base;
  MB_MODE_INFO **mi_grid_visible;
  MB_MODE_INFO **prev_mi_grid_base;
  MB_MODE_INFO **prev_mi_grid_visible;
#endif
  // Whether to use previous frames' motion vectors for prediction.
  int allow_ref_frame_mvs;

  uint8_t *last_frame_seg_map;

#ifdef ORI_CODE
  InterpFilter interp_filter;
#endif
  int switchable_motion_mode;
#ifdef ORI_CODE
  loop_filter_info_n lf_info;
#endif
  // The denominator of the superres scale; the numerator is fixed.
  uint8_t superres_scale_denominator;
  int superres_upscaled_width;
  int superres_upscaled_height;

#ifdef ORI_CODE
  RestorationInfo rst_info[MAX_MB_PLANE];
#endif
  // Pointer to a scratch buffer used by self-guided restoration
  int32_t *rst_tmpbuf;
#ifdef ORI_CODE
  RestorationLineBuffers *rlbs;
#endif
  // Output of loop restoration
  PIC_BUFFER_CONFIG rst_frame;

  // Flag signaling how frame contexts should be updated at the end of
  // a frame decode
  REFRESH_FRAME_CONTEXT_MODE refresh_frame_context;

  int ref_frame_sign_bias[REF_FRAMES]; /* Two state 0, 1 */

#ifdef ORI_CODE
  struct loopfilter lf;
  struct segmentation seg;
#endif

  int coded_lossless;  // frame is fully lossless at the coded resolution.
  int all_lossless;    // frame is fully lossless at the upscaled resolution.

  int reduced_tx_set_used;

#ifdef ORI_CODE
  // Context probabilities for reference frame prediction
  MV_REFERENCE_FRAME comp_fwd_ref[FWD_REFS];
  MV_REFERENCE_FRAME comp_bwd_ref[BWD_REFS];

  FRAME_CONTEXT *fc;              /* this frame entropy */
  FRAME_CONTEXT *default_frame_context;
#endif
  int primary_ref_frame;

  int error_resilient_mode;

  int tile_cols, tile_rows;

  int max_tile_width_sb;
  int min_log2_tile_cols;
  int max_log2_tile_cols;
  int max_log2_tile_rows;
  int min_log2_tile_rows;
  int min_log2_tiles;
  int max_tile_height_sb;
  int uniform_tile_spacing_flag;
  int log2_tile_cols;                        // only valid for uniform tiles
  int log2_tile_rows;                        // only valid for uniform tiles
  int tile_col_start_sb[MAX_TILE_COLS + 1];  // valid for 0 <= i <= tile_cols
  int tile_row_start_sb[MAX_TILE_ROWS + 1];  // valid for 0 <= i <= tile_rows
  int tile_width, tile_height;               // In MI units
  int min_inner_tile_width;                  // min width of non-rightmost tile

  unsigned int large_scale_tile;
  unsigned int single_tile_decoding;

  int byte_alignment;
  int skip_loop_filter;
  int skip_film_grain;

  // External BufferPool passed from outside.
  BufferPool *buffer_pool;

#ifdef ORI_CODE
  PARTITION_CONTEXT **above_seg_context;
  ENTROPY_CONTEXT **above_context[MAX_MB_PLANE];
  TXFM_CONTEXT **above_txfm_context;
  WarpedMotionParams global_motion[REF_FRAMES];
  aom_film_grain_t film_grain_params;

  CdefInfo cdef_info;
  DeltaQInfo delta_q_info;  // Delta Q and Delta LF parameters
#endif
  int num_tg;
  SequenceHeader seq_params;
  int current_frame_id;
  int ref_frame_id[REF_FRAMES];
  int valid_for_referencing[REF_FRAMES];
#ifdef ORI_CODE
  TPL_MV_REF *tpl_mvs;
#endif
  int tpl_mvs_mem_size;
  // TODO(jingning): This can be combined with sign_bias later.
  int8_t ref_frame_side[REF_FRAMES];

  int is_annexb;

  int temporal_layer_id;
  int spatial_layer_id;
  unsigned int number_temporal_layers;
  unsigned int number_spatial_layers;
  int num_allocated_above_context_mi_col;
  int num_allocated_above_contexts;
  int num_allocated_above_context_planes;

#if TXCOEFF_TIMER
  int64_t cum_txcoeff_timer;
  int64_t txcoeff_timer;
  int txb_count;
#endif

#if TXCOEFF_COST_TIMER
  int64_t cum_txcoeff_cost_timer;
  int64_t txcoeff_cost_timer;
  int64_t txcoeff_cost_count;
#endif
  const cfg_options_t *options;
  int is_decoding;
#ifdef AML
  int mv_ref_offset[MV_REF_SIZE][REF_FRAMES];
  int mv_ref_id[MV_REF_SIZE];
  unsigned char mv_cal_tpl_mvs[MV_REF_SIZE];
  int mv_ref_id_index;
      int prev_fb_idx;
    int new_fb_idx;
    int32_t dec_width;
#endif
#ifdef AML_DEVICE
      int cur_fb_idx_mmu;
#ifdef AOM_AV1_MMU_DW
      int cur_fb_idx_mmu_dw;
#endif
      int current_video_frame;
      int use_prev_frame_mvs;
      int frame_type;
      int intra_only;
  struct RefCntBuffer_s frame_refs[INTER_REFS_PER_FRAME];

#endif
} AV1_COMMON;


/*
from:
	decoder/decoder.h
*/

typedef struct EXTERNAL_REFERENCES {
  PIC_BUFFER_CONFIG refs[MAX_EXTERNAL_REFERENCES];
  int num;
} EXTERNAL_REFERENCES;

typedef struct AV1Decoder {
  //DECLARE_ALIGNED(32, MACROBLOCKD, mb);

  //DECLARE_ALIGNED(32, AV1_COMMON, common);
  AV1_COMMON *common;

#ifdef ORI_CODE
  AVxWorker lf_worker;
  AV1LfSync lf_row_sync;
  AV1LrSync lr_row_sync;
  AV1LrStruct lr_ctxt;
  AVxWorker *tile_workers;
  int num_workers;
  DecWorkerData *thread_data;
  ThreadData td;
  TileDataDec *tile_data;
  int allocated_tiles;
  TileBufferDec tile_buffers[MAX_TILE_ROWS][MAX_TILE_COLS];
  AV1DecTileMT tile_mt_info;
#endif

  // Each time the decoder is called, we expect to receive a full temporal unit.
  // This can contain up to one shown frame per spatial layer in the current
  // operating point (note that some layers may be entirely omitted).
  // If the 'output_all_layers' option is true, we save all of these shown
  // frames so that they can be returned to the application. If the
  // 'output_all_layers' option is false, then we only output one image per
  // temporal unit.
  //
  // Note: The saved buffers are released at the start of the next time the
  // application calls aom_codec_decode().
  int output_all_layers;
  RefCntBuffer *output_frames[MAX_NUM_SPATIAL_LAYERS];
  size_t num_output_frames;  // How many frames are queued up so far?

  // In order to properly support random-access decoding, we need
  // to behave slightly differently for the very first frame we decode.
  // So we track whether this is the first frame or not.
  int decoding_first_frame;

  int allow_lowbitdepth;
  int max_threads;
  int inv_tile_order;
  int need_resync;   // wait for key/intra-only frame.
  int hold_ref_buf;  // Boolean: whether we are holding reference buffers in
                     // common.next_ref_frame_map.
  int reset_decoder_state;

  int tile_size_bytes;
  int tile_col_size_bytes;
  int dec_tile_row, dec_tile_col;  // always -1 for non-VR tile encoding
#if CONFIG_ACCOUNTING
  int acct_enabled;
  Accounting accounting;
#endif
  int tg_size;   // Number of tiles in the current tilegroup
  int tg_start;  // First tile in the current tilegroup
  int tg_size_bit_offset;
  int sequence_header_ready;
  int sequence_header_changed;
#if CONFIG_INSPECTION
  aom_inspect_cb inspect_cb;
  void *inspect_ctx;
#endif
  int operating_point;
  int current_operating_point;
  int seen_frame_header;

  // State if the camera frame header is already decoded while
  // large_scale_tile = 1.
  int camera_frame_header_ready;
  size_t frame_header_size;
#ifdef ORI_CODE
  DataBuffer obu_size_hdr;
#endif
  int output_frame_width_in_tiles_minus_1;
  int output_frame_height_in_tiles_minus_1;
  int tile_count_minus_1;
  uint32_t coded_tile_data_size;
  unsigned int ext_tile_debug;  // for ext-tile software debug & testing
  unsigned int row_mt;
  EXTERNAL_REFERENCES ext_refs;
  PIC_BUFFER_CONFIG tile_list_outbuf;

#ifdef ORI_CODE
  CB_BUFFER *cb_buffer_base;
#endif
  int cb_buffer_alloc_size;

  int allocated_row_mt_sync_rows;

#if CONFIG_MULTITHREAD
  pthread_mutex_t *row_mt_mutex_;
  pthread_cond_t *row_mt_cond_;
#endif

#ifdef ORI_CODE
  AV1DecRowMTInfo frame_row_mt_info;
#endif

#ifdef AML
  unsigned char pred_inter_read_enable;
  int cur_obu_type;
  int decode_idx;
  int bufmgr_proc_count;
  int obu_frame_frame_head_come_after_tile;
    uint32_t frame_width;
    uint32_t frame_height;
    BuffInfo_t* work_space_buf;
    buff_t* mc_buf;
    //unsigned short *rpm_ptr;
    void *private_data;
    u32 pre_stream_offset;
#endif
} AV1Decoder;

#define RPM_BEGIN                                              0x200
#define RPM_END                                                0x280

typedef union param_u {
    struct {
        unsigned short data[RPM_END - RPM_BEGIN];
    } l;
    struct {
        /*sequence head*/
        unsigned short profile;
        unsigned short still_picture;
        unsigned short reduced_still_picture_hdr;
        unsigned short decoder_model_info_present_flag;
        unsigned short max_frame_width;
        unsigned short max_frame_height;
        unsigned short frame_id_numbers_present_flag;
        unsigned short delta_frame_id_length;
        unsigned short frame_id_length;
        unsigned short order_hint_bits_minus_1;
        unsigned short enable_order_hint;
        unsigned short enable_dist_wtd_comp;
        unsigned short enable_ref_frame_mvs;

        /*frame head*/
        unsigned short show_existing_frame;
        unsigned short frame_type;
        unsigned short show_frame;
        unsigned short error_resilient_mode;
        unsigned short refresh_frame_flags;
        unsigned short showable_frame;
        unsigned short current_frame_id;
        unsigned short frame_size_override_flag;
        unsigned short order_hint;
        unsigned short primary_ref_frame;
        unsigned short frame_refs_short_signaling;
        unsigned short frame_width;
        unsigned short dec_frame_width;
        unsigned short frame_width_scaled;
        unsigned short frame_height;
        unsigned short reference_mode;
        unsigned short allow_ref_frame_mvs;
        unsigned short superres_scale_denominator;
        unsigned short lst_ref;
        unsigned short gld_ref;
        unsigned short existing_frame_idx;

        unsigned short remapped_ref_idx[INTER_REFS_PER_FRAME];
        unsigned short delta_frame_id_minus_1[INTER_REFS_PER_FRAME];
        unsigned short ref_order_hint[REF_FRAMES];
        /*other not in reference*/
        unsigned short bit_depth;
        unsigned short seq_flags;
        unsigned short update_parameters;
        unsigned short film_grain_params_ref_idx;

        /*loop_filter & segmentation*/
        unsigned short loop_filter_sharpness_level;
        unsigned short loop_filter_mode_ref_delta_enabled;
        unsigned short loop_filter_ref_deltas_0;
        unsigned short loop_filter_ref_deltas_1;
        unsigned short loop_filter_ref_deltas_2;
        unsigned short loop_filter_ref_deltas_3;
        unsigned short loop_filter_ref_deltas_4;
        unsigned short loop_filter_ref_deltas_5;
        unsigned short loop_filter_ref_deltas_6;
        unsigned short loop_filter_ref_deltas_7;
        unsigned short loop_filter_mode_deltas_0;
        unsigned short loop_filter_mode_deltas_1;
        unsigned short loop_filter_level_0;
        unsigned short loop_filter_level_1;
        unsigned short loop_filter_level_u;
        unsigned short loop_filter_level_v;

        unsigned short segmentation_enabled;
          /*
           SEG_LVL_ALT_LF_Y_V feature_enable: seg_lf_info_y[bit7]
           SEG_LVL_ALT_LF_Y_V data: seg_lf_info_y[bit0~6]
           SEG_LVL_ALT_LF_Y_H feature enable: seg_lf_info_y[bit15]
           SEG_LVL_ALT_LF_Y_H data: seg_lf_info_y[bit8~14]
           */
        unsigned short seg_lf_info_y[8];
          /*
           SEG_LVL_ALT_LF_U feature_enable: seg_lf_info_y[bit7]
           SEG_LVL_ALT_LF_U data: seg_lf_info_y[bit0~6]
           SEG_LVL_ALT_LF_V feature enable: seg_lf_info_y[bit15]
           SEG_LVL_ALT_LF_V data: seg_lf_info_y[bit8~14]
           */
        unsigned short seg_lf_info_c[8];
		unsigned short video_signal_type;
		unsigned short color_description;

        unsigned short mmu_used_num;
        unsigned short dw_mmu_used_num;
        unsigned short seq_flags_2;
		unsigned short film_grain_present_flag;

        /*ucode end*/
        /*other*/
        unsigned short enable_superres;

        /*seqence not use*/
        unsigned short operating_points_cnt_minus_1;
        unsigned short operating_point_idc[MAX_NUM_OPERATING_POINTS];
        unsigned short seq_level_idx[MAX_NUM_OPERATING_POINTS];
        unsigned short decoder_model_param_present_flag[MAX_NUM_OPERATING_POINTS];
        unsigned short timing_info_present;
        /*frame head not use*/
        unsigned short display_frame_id;
        unsigned short frame_presentation_time;
        unsigned short buffer_removal_time_present;
        unsigned short op_frame_timing[MAX_NUM_OPERATING_POINTS + 1];
        unsigned short valid_ref_frame_bits;

    } p;
}param_t;

PIC_BUFFER_CONFIG *av1_get_ref_frame_spec_buf(
    const AV1_COMMON *const cm, const MV_REFERENCE_FRAME ref_frame);

int av1_bufmgr_process(AV1Decoder *pbi, union param_u *params,
    unsigned char new_compressed_data, int obu_type);

struct scale_factors *av1_get_ref_scale_factors(
  AV1_COMMON *const cm, const MV_REFERENCE_FRAME ref_frame);

void av1_set_next_ref_frame_map(AV1Decoder *pbi);

unsigned int av1_get_next_used_ref_info(
    const AV1_COMMON *const cm, int i);

void av1_release_buf(AV1Decoder *pbi, RefCntBuffer *const buf);

int av1_bufmgr_postproc(AV1Decoder *pbi, unsigned char frame_decoded);

AV1Decoder *av1_decoder_create(BufferPool *const pool, AV1_COMMON *cm);

unsigned char av1_frame_is_inter(const AV1_COMMON *const cm);

RefCntBuffer *av1_get_primary_ref_frame_buf(
  const AV1_COMMON *const cm);

void av1_raw_write_image(AV1Decoder *pbi, PIC_BUFFER_CONFIG *sd);

int get_free_frame_buffer(struct AV1_Common_s *cm);

void av1_bufmgr_ctx_reset(AV1Decoder *pbi, BufferPool *const pool, AV1_COMMON *cm);

#if 1
#define lock_buffer_pool(pool, flags) \
    spin_lock_irqsave(&pool->lock, flags)

#define unlock_buffer_pool(pool, flags) \
    spin_unlock_irqrestore(&pool->lock, flags)
#else
#define lock_buffer_pool(pool, flags) flags=1;

#define unlock_buffer_pool(pool, flags) flags=0;

#endif

#define AV1_DEBUG_BUFMGR                   0x01
#define AV1_DEBUG_BUFMGR_MORE              0x02
#define AV1_DEBUG_BUFMGR_DETAIL            0x04
#define AV1_DEBUG_TIMEOUT_INFO             0x08
#define AV1_DEBUG_OUT_PTS                  0x10
#define AOM_DEBUG_HW_MORE                  0x20
#define AOM_DEBUG_VFRAME                   0x40
#define AOM_DEBUG_PRINT_LIST_INFO          0x80
#define AOM_AV1_DEBUG_SEND_PARAM_WITH_REG      0x100
#define AV1_DEBUG_IGNORE_VF_REF            0x200
#define AV1_DEBUG_DBG_LF_PRINT             0x400
#define AV1_DEBUG_REG                      0x800
#define AOM_DEBUG_BUFMGR_ONLY              0x1000
#define AOM_DEBUG_AUX_DATA                 0x2000
#define AV1_DEBUG_QOS_INFO                 0x4000
#define AOM_DEBUG_DW_DISP_MAIN             0x8000
#define AV1_DEBUG_DIS_LOC_ERROR_PROC       0x10000
#define AOM_DEBUG_DIS_RECYCLE_MMU_TAIL     0x20000
#define AV1_DEBUG_DUMP_PIC_LIST       0x40000
#define AV1_DEBUG_TRIG_SLICE_SEGMENT_PROC 0x80000
#define AOM_DEBUG_USE_FIXED_MV_BUF_SIZE  0x100000
#define AV1_DEBUG_LOAD_UCODE_FROM_FILE   0x200000
#define AV1_DEBUG_FORCE_SEND_AGAIN       0x400000
#define AV1_DEBUG_DUMP_DATA              0x800000
#define AV1_DEBUG_CACHE                  0x1000000
#define AV1_DEBUG_CACHE_HIT_RATE         0x2000000
#define AV1_DEBUG_SEI_DETAIL             0x4000000
#define IGNORE_PARAM_FROM_CONFIG         0x8000000
#if 1
/*def MULTI_INSTANCE_SUPPORT*/
#define PRINT_FLAG_ERROR    0x0
#define PRINT_FLAG_V4L_DETAIL   0x10000000
#define PRINT_FLAG_VDEC_STATUS    0x20000000
#define PRINT_FLAG_VDEC_DETAIL    0x40000000
#define PRINT_FLAG_VDEC_DATA    0x80000000
#endif

int av1_print2(int flag, const char *fmt, ...);

unsigned char av1_is_debug(int flag);

#endif

